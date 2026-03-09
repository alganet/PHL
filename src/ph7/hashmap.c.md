# src/ph7/hashmap.c

<style>code, pre { background: none !important; white-space: pre !important; width: 100% !important; display: inline-block !important; } td { border: none !important; margin-top: 0 !important; margin-bottom: 0 !important; padding-top: 0 !important; padding-bottom: 0 !important; }</style>

Coverage: 2671/3163 lines (84.45%)

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
| 2793554 |   18 | `static sxu32 IntHash(sxi64 iKey)` |
|       2 |   19 |  |
| 2793556 |   20 | `	return (sxu32)(iKey ^ (iKey << 8) ^ (iKey >> 8));` |
|       2 |   21 |  |
|       - |   22 | `/*` |
|       - |   23 | ` * Default hash function for string/BLOB keys.` |
|       - |   24 | ` */` |
|  225722 |   25 | `static sxu32 BinHash(const void *pSrc,sxu32 nLen)` |
|       2 |   26 |  |
|  225724 |   27 | `	register unsigned char *zIn = (unsigned char *)pSrc;` |
|       - |   28 | `	unsigned char *zEnd;` |
|  225724 |   29 | `	sxu32 nH = 5381;` |
|  225724 |   30 | `	zEnd = &zIn[nLen];` |
|  258825 |   31 | `	for(;;){` |
|  517652 |   32 | `		if( zIn >= zEnd ){ break; } nH = nH * 33 + zIn[0] ; zIn++;` |
|  464384 |   33 | `		if( zIn >= zEnd ){ break; } nH = nH * 33 + zIn[0] ; zIn++;` |
|  419580 |   34 | `		if( zIn >= zEnd ){ break; } nH = nH * 33 + zIn[0] ; zIn++;` |
|  339948 |   35 | `		if( zIn >= zEnd ){ break; } nH = nH * 33 + zIn[0] ; zIn++;` |
|       2 |   36 | `	}` |
|  225724 |   37 | `	return nH;` |
|       2 |   38 |  |
|       - |   39 | `/*` |
|       - |   40 | ` * Return the total number of entries in a given hashmap.` |
|       - |   41 | ` * If bRecurisve is set to TRUE then recurse on hashmap entries.` |
|       - |   42 | ` * If the nesting limit is reached,this function abort immediately.` |
|       - |   43 | ` */` |
|     816 |   44 | `static sxi64 HashmapCount(ph7_hashmap *pMap,int bRecursive,int iRecCount)` |
|       2 |   45 |  |
|     818 |   46 | `	sxi64 iCount = 0;` |
|     818 |   47 | `	if( !bRecursive ){` |
|     542 |   48 | `		iCount = pMap->nEntry;` |
|     272 |   49 | `	}else{` |
|       - |   50 | `		/* Recursive hashmap walk */` |
|     277 |   51 | `		ph7_hashmap_node *pEntry = pMap->pLast;` |
|       - |   52 | `		ph7_value *pElem;` |
|     277 |   53 | `		sxu32 n = 0;` |
|     331 |   54 | `		for(;;){` |
|     663 |   55 | `			if( n >= pMap->nEntry ){` |
|     273 |   56 | `				break;` |
|       - |   57 | `			}` |
|       - |   58 | `			/* Point to the element value */` |
|     391 |   59 | `			pElem = (ph7_value *)SySetAt(&pMap->pVm->aMemObj,pEntry->nValIdx);` |
|     391 |   60 | `			if( pElem ){` |
|     391 |   61 | `				if( pElem->iFlags & MEMOBJ_HASHMAP ){` |
|     251 |   62 | `					if( iRecCount > 31 ){` |
|       - |   63 | `						/* Nesting limit reached */` |
|       5 |   64 | `						return iCount;` |
|       - |   65 | `					}` |
|       - |   66 | `					/* Recurse */` |
|     247 |   67 | `					iRecCount++;` |
|     247 |   68 | `					iCount += HashmapCount((ph7_hashmap *)pElem->x.pOther,TRUE,iRecCount);` |
|     247 |   69 | `					iRecCount--;` |
|     123 |   70 | `				}` |
|     193 |   71 | `			}` |
|       - |   72 | `			/* Point to the next entry */` |
|     387 |   73 | `			pEntry = pEntry->pNext;` |
|     387 |   74 | `			++n;` |
|       1 |   75 | `		}` |
|       - |   76 | `		/* Update count */` |
|     273 |   77 | `		iCount += pMap->nEntry;` |
|       - |   78 | `	}` |
|     814 |   79 | `	return iCount;` |
|     410 |   80 |  |
|       - |   81 | `/*` |
|       - |   82 | ` * Allocate a new hashmap node with a 64-bit integer key.` |
|       - |   83 | ` * If something goes wrong [i.e: out of memory],this function return NULL.` |
|       - |   84 | ` * Otherwise a fresh [ph7_hashmap_node] instance is returned.` |
|       - |   85 | ` */` |
| 2739132 |   86 | `static ph7_hashmap_node * HashmapNewIntNode(ph7_hashmap *pMap,sxi64 iKey,sxu32 nHash,sxu32 nValIdx)` |
|       2 |   87 |  |
|       - |   88 | `	ph7_hashmap_node *pNode;` |
|       - |   89 | `	/* Allocate a new node */` |
| 2739134 |   90 | `	pNode = (ph7_hashmap_node *)SyMemBackendPoolAlloc(&pMap->pVm->sAllocator,sizeof(ph7_hashmap_node));` |
| 2739134 |   91 | `	if( pNode == 0 ){` |
|     ! 0 |   92 | `		return 0;` |
|       - |   93 | `	}` |
|       - |   94 | `	/* Zero the stucture */` |
| 2739134 |   95 | `	SyZero(pNode,sizeof(ph7_hashmap_node));` |
|       - |   96 | `	/* Fill in the structure */` |
| 2739134 |   97 | `	pNode->pMap  = &(*pMap);` |
| 2739134 |   98 | `	pNode->iType = HASHMAP_INT_NODE;` |
| 2739134 |   99 | `	pNode->nHash = nHash;` |
| 2739134 |  100 | `	pNode->xKey.iKey = iKey;` |
| 2739134 |  101 | `	pNode->nValIdx  = nValIdx;` |
| 2739134 |  102 | `	return pNode;` |
| 1369568 |  103 |  |
|       - |  104 | `/*` |
|       - |  105 | ` * Allocate a new hashmap node with a BLOB key.` |
|       - |  106 | ` * If something goes wrong [i.e: out of memory],this function return NULL.` |
|       - |  107 | ` * Otherwise a fresh [ph7_hashmap_node] instance is returned.` |
|       - |  108 | ` */` |
|   78470 |  109 | `static ph7_hashmap_node * HashmapNewBlobNode(ph7_hashmap *pMap,const void *pKey,sxu32 nKeyLen,sxu32 nHash,sxu32 nValIdx)` |
|       2 |  110 |  |
|       - |  111 | `	ph7_hashmap_node *pNode;` |
|       - |  112 | `	/* Allocate a new node */` |
|   78472 |  113 | `	pNode = (ph7_hashmap_node *)SyMemBackendPoolAlloc(&pMap->pVm->sAllocator,sizeof(ph7_hashmap_node));` |
|   78472 |  114 | `	if( pNode == 0 ){` |
|     ! 0 |  115 | `		return 0;` |
|       - |  116 | `	}` |
|       - |  117 | `	/* Zero the stucture */` |
|   78472 |  118 | `	SyZero(pNode,sizeof(ph7_hashmap_node));` |
|       - |  119 | `	/* Fill in the structure */` |
|   78472 |  120 | `	pNode->pMap  = &(*pMap);` |
|   78472 |  121 | `	pNode->iType = HASHMAP_BLOB_NODE;` |
|   78472 |  122 | `	pNode->nHash = nHash;` |
|   78472 |  123 | `	SyBlobInit(&pNode->xKey.sKey,&pMap->pVm->sAllocator);` |
|   78472 |  124 | `	SyBlobAppend(&pNode->xKey.sKey,pKey,nKeyLen);` |
|   78472 |  125 | `	pNode->nValIdx = nValIdx;` |
|   78472 |  126 | `	return pNode;` |
|   39237 |  127 |  |
|       - |  128 | `/*` |
|       - |  129 | ` * link a hashmap node to the given bucket index (last argument to this function).` |
|       - |  130 | ` */` |
| 2817602 |  131 | `static void HashmapNodeLink(ph7_hashmap *pMap,ph7_hashmap_node *pNode,sxu32 nBucketIdx)` |
|       2 |  132 |  |
|       - |  133 | `	/* Link */` |
| 2817604 |  134 | `	if( pMap->apBucket[nBucketIdx] != 0 ){` |
| 2602096 |  135 | `		pNode->pNextCollide = pMap->apBucket[nBucketIdx];` |
| 2602096 |  136 | `		pMap->apBucket[nBucketIdx]->pPrevCollide = pNode;` |
| 1301047 |  137 | `	}` |
| 2817604 |  138 | `	pMap->apBucket[nBucketIdx] = pNode;` |
|       - |  139 | `	/* Link to the map list */` |
| 2817604 |  140 | `	if( pMap->pFirst == 0 ){` |
|   36342 |  141 | `		pMap->pFirst = pMap->pLast = pNode;` |
|       - |  142 | `		/* Point to the first inserted node */` |
|   36342 |  143 | `		pMap->pCur = pNode;` |
|   18172 |  144 | `	}else{` |
| 2781264 |  145 | `		MACRO_LD_PUSH(pMap->pLast,pNode);` |
|       - |  146 | `	}` |
| 2817604 |  147 | `	++pMap->nEntry;` |
| 2817604 |  148 |  |
|       - |  149 | `/*` |
|       - |  150 | ` * Unlink a node from the hashmap.` |
|       - |  151 | ` * If the node count reaches zero then release the whole hash-bucket.` |
|       - |  152 | ` */` |
|    5504 |  153 | `PH7_PRIVATE void PH7_HashmapUnlinkNode(ph7_hashmap_node *pNode,int bRestore)` |
|       2 |  154 |  |
|    5506 |  155 | `	ph7_hashmap *pMap = pNode->pMap;` |
|    5506 |  156 | `	ph7_vm *pVm = pMap->pVm;` |
|       - |  157 | `	/* Unlink from the corresponding bucket */` |
|    5506 |  158 | `	if( pNode->pPrevCollide == 0 ){` |
|    5082 |  159 | `		pMap->apBucket[pNode->nHash & (pMap->nSize - 1)] = pNode->pNextCollide;` |
|    2542 |  160 | `	}else{` |
|     425 |  161 | `		pNode->pPrevCollide->pNextCollide = pNode->pNextCollide;` |
|       - |  162 | `	}` |
|    5506 |  163 | `	if( pNode->pNextCollide ){` |
|    4307 |  164 | `		pNode->pNextCollide->pPrevCollide = pNode->pPrevCollide;` |
|    2153 |  165 | `	}` |
|    5506 |  166 | `	if( pMap->pFirst == pNode ){` |
|      58 |  167 | `		pMap->pFirst = pNode->pPrev;` |
|      28 |  168 | `	}` |
|    5506 |  169 | `	if( pMap->pCur == pNode ){` |
|       - |  170 | `		/* Advance the node cursor */` |
|      60 |  171 | `		pMap->pCur = pMap->pCur->pPrev; /* Reverse link */` |
|      29 |  172 | `	}` |
|       - |  173 | `	/* Unlink from the map list */` |
|    5506 |  174 | `	MACRO_LD_REMOVE(pMap->pLast,pNode);` |
|    5506 |  175 | `	if( bRestore ){` |
|       - |  176 | `		/* Remove the ph7_value associated with this node from the reference table */` |
|      30 |  177 | `		PH7_VmRefObjRemove(pVm,pNode->nValIdx,0,pNode);` |
|       - |  178 | `		/* Restore to the freelist */` |
|      30 |  179 | `		if( (pNode->iFlags & HASHMAP_NODE_FOREIGN_OBJ) == 0 ){` |
|      30 |  180 | `			PH7_VmUnsetMemObj(pVm,pNode->nValIdx,FALSE);` |
|      14 |  181 | `		}` |
|      14 |  182 | `	}` |
|    5506 |  183 | `	if( pNode->iType == HASHMAP_BLOB_NODE ){` |
|    5457 |  184 | `		SyBlobRelease(&pNode->xKey.sKey);` |
|    2728 |  185 | `	}` |
|    5506 |  186 | `	SyMemBackendPoolFree(&pVm->sAllocator,pNode);` |
|    5506 |  187 | `	pMap->nEntry--;` |
|    5506 |  188 | `	if( pMap->nEntry < 1 && pMap != pVm->pGlobal ){` |
|       - |  189 | `		/* Free the hash-bucket */` |
|      26 |  190 | `		SyMemBackendFree(&pVm->sAllocator,pMap->apBucket);` |
|      26 |  191 | `		pMap->apBucket = 0;` |
|      26 |  192 | `		pMap->nSize = 0;` |
|      26 |  193 | `		pMap->pFirst = pMap->pLast = pMap->pCur = 0;` |
|      12 |  194 | `	}` |
|    5506 |  195 |  |
|       - |  196 | `#define HASHMAP_FILL_FACTOR 3` |
|       - |  197 | `/*` |
|       - |  198 | ` * Grow the hash-table and rehash all entries.` |
|       - |  199 | ` */` |
| 2817602 |  200 | `static sxi32 HashmapGrowBucket(ph7_hashmap *pMap)` |
|       2 |  201 |  |
| 2817604 |  202 | `	if( pMap->nEntry >= pMap->nSize * HASHMAP_FILL_FACTOR ){` |
|   39958 |  203 | `		ph7_hashmap_node **apOld = pMap->apBucket;` |
|       - |  204 | `		ph7_hashmap_node *pEntry,**apNew;` |
|   39958 |  205 | `		sxu32 nNew = pMap->nSize << 1;` |
|       - |  206 | `		sxu32 nBucket;` |
|       - |  207 | `		sxu32 n;` |
|   39958 |  208 | `		if( nNew < 1 ){` |
|   36342 |  209 | `			nNew = 16;` |
|   18170 |  210 | `		}` |
|       - |  211 | `		/* Allocate a new bucket */` |
|   39958 |  212 | `		apNew = (ph7_hashmap_node **)SyMemBackendAlloc(&pMap->pVm->sAllocator,nNew * sizeof(ph7_hashmap_node *));` |
|   39958 |  213 | `		if( apNew == 0 ){` |
|     ! 0 |  214 | `			if( pMap->nSize < 1 ){` |
|     ! 0 |  215 | `				return SXERR_MEM; /* Fatal */` |
|       - |  216 | `			}` |
|       - |  217 | `			/* Not so fatal here,simply a performance hit */` |
|     ! 0 |  218 | `			return SXRET_OK;` |
|       - |  219 | `		}` |
|       - |  220 | `		/* Zero the table */` |
|   39958 |  221 | `		SyZero((void *)apNew,nNew * sizeof(ph7_hashmap_node *));` |
|       - |  222 | `		/* Reflect the change */` |
|   39958 |  223 | `		pMap->apBucket = apNew;` |
|   39958 |  224 | `		pMap->nSize = nNew;` |
|   39958 |  225 | `		if( apOld == 0 ){` |
|       - |  226 | `			/* First allocated table [i.e: no entry],return immediately */` |
|   36342 |  227 | `			return SXRET_OK;` |
|       - |  228 | `		}` |
|       - |  229 | `		/* Rehash old entries */` |
|    3618 |  230 | `		pEntry = pMap->pFirst;` |
|    3618 |  231 | `		n = 0;` |
| 1923296 |  232 | `		for( ;; ){` |
| 3846594 |  233 | `			if( n >= pMap->nEntry ){` |
|    3618 |  234 | `				break;` |
|       - |  235 | `			}` |
|       - |  236 | `			/* Clear the old collision link */` |
| 3842978 |  237 | `			pEntry->pNextCollide = pEntry->pPrevCollide = 0;` |
|       - |  238 | `			/* Link to the new bucket */` |
| 3842978 |  239 | `			nBucket = pEntry->nHash & (nNew - 1);` |
| 3842978 |  240 | `			if( pMap->apBucket[nBucket] != 0 ){` |
| 3396996 |  241 | `				pEntry->pNextCollide = pMap->apBucket[nBucket];` |
| 3396996 |  242 | `				pMap->apBucket[nBucket]->pPrevCollide = pEntry;` |
| 1698497 |  243 | `			}` |
| 3842978 |  244 | `			pMap->apBucket[nBucket] = pEntry;` |
|       - |  245 | `			/* Point to the next entry */` |
| 3842978 |  246 | `			pEntry = pEntry->pPrev; /* Reverse link */` |
| 3842978 |  247 | `			n++;` |
|       2 |  248 | `		}` |
|       - |  249 | `		/* Free the old table */` |
|    3618 |  250 | `		SyMemBackendFree(&pMap->pVm->sAllocator,(void *)apOld);` |
|    1808 |  251 | `	}` |
| 2781264 |  252 | `	return SXRET_OK;` |
| 1408803 |  253 |  |
|       - |  254 | `/*` |
|       - |  255 | ` * Insert a 64-bit integer key and it's associated value (if any) in the given` |
|       - |  256 | ` * hashmap.` |
|       - |  257 | ` */` |
| 2739132 |  258 | `static sxi32 HashmapInsertIntKey(ph7_hashmap *pMap,sxi64 iKey,ph7_value *pValue,sxu32 nRefIdx,int isForeign)` |
|       2 |  259 |  |
|       - |  260 | `	ph7_hashmap_node *pNode;` |
|       - |  261 | `	sxu32 nIdx;` |
|       - |  262 | `	sxu32 nHash;` |
|       - |  263 | `	sxi32 rc;` |
| 2739134 |  264 | `	if( !isForeign ){` |
|       - |  265 | `		ph7_value *pObj;` |
|       - |  266 | `		/* Reserve a ph7_value for the value */` |
| 2739110 |  267 | `		pObj = PH7_ReserveMemObj(pMap->pVm);` |
| 2739110 |  268 | `		if( pObj == 0 ){` |
|     ! 0 |  269 | `			return SXERR_MEM;` |
|       - |  270 | `		}` |
| 2739110 |  271 | `		if( pValue ){` |
|       - |  272 | `			/* Duplicate the value */` |
| 2739110 |  273 | `			PH7_MemObjStore(pValue,pObj);` |
| 1369554 |  274 | `		}` |
| 2739110 |  275 | `		nIdx = pObj->nIdx;` |
| 1369556 |  276 | `	}else{` |
|      25 |  277 | `		nIdx = nRefIdx;` |
|       - |  278 | `	}` |
|       - |  279 | `	/* Hash the key */` |
| 2739134 |  280 | `	nHash = pMap->xIntHash(iKey);` |
|       - |  281 | `	/* Allocate a new int node */` |
| 2739134 |  282 | `	pNode = HashmapNewIntNode(&(*pMap),iKey,nHash,nIdx);` |
| 2739134 |  283 | `	if( pNode == 0 ){` |
|     ! 0 |  284 | `		return SXERR_MEM;` |
|       - |  285 | `	}` |
| 2739134 |  286 | `	if( isForeign ){` |
|       - |  287 | `		/* Mark as a foregin entry */` |
|      25 |  288 | `		pNode->iFlags \|= HASHMAP_NODE_FOREIGN_OBJ;` |
|      12 |  289 | `	}` |
|       - |  290 | `	/* Make sure the bucket is big enough to hold the new entry */` |
| 2739134 |  291 | `	rc = HashmapGrowBucket(&(*pMap));` |
| 2739134 |  292 | `	if( rc != SXRET_OK ){` |
|     ! 0 |  293 | `		SyMemBackendPoolFree(&pMap->pVm->sAllocator,pNode);` |
|     ! 0 |  294 | `		return rc;` |
|       - |  295 | `	}` |
|       - |  296 | `	/* Perform the insertion */` |
| 2739134 |  297 | `	HashmapNodeLink(&(*pMap),pNode,nHash & (pMap->nSize - 1));` |
|       - |  298 | `	/* Install in the reference table */` |
| 2739134 |  299 | `	PH7_VmRefObjInstall(pMap->pVm,nIdx,0,pNode,0);` |
|       - |  300 | `	/* All done */` |
| 2739134 |  301 | `	return SXRET_OK;` |
| 1369568 |  302 |  |
|       - |  303 | `/*` |
|       - |  304 | ` * Insert a BLOB key and it's associated value (if any) in the given` |
|       - |  305 | ` * hashmap.` |
|       - |  306 | ` */` |
|   78470 |  307 | `static sxi32 HashmapInsertBlobKey(ph7_hashmap *pMap,const void *pKey,sxu32 nKeyLen,ph7_value *pValue,sxu32 nRefIdx,int isForeign)` |
|       2 |  308 |  |
|       - |  309 | `	ph7_hashmap_node *pNode;` |
|       - |  310 | `	sxu32 nHash;` |
|       - |  311 | `	sxu32 nIdx;` |
|       - |  312 | `	sxi32 rc;` |
|   78472 |  313 | `	if( !isForeign ){` |
|       - |  314 | `		ph7_value *pObj;` |
|       - |  315 | `		/* Reserve a ph7_value for the value */` |
|   58434 |  316 | `		pObj = PH7_ReserveMemObj(pMap->pVm);` |
|   58434 |  317 | `		if( pObj == 0 ){` |
|     ! 0 |  318 | `			return SXERR_MEM;` |
|       - |  319 | `		}` |
|   58434 |  320 | `		if( pValue ){` |
|       - |  321 | `			/* Duplicate the value */` |
|   58434 |  322 | `			PH7_MemObjStore(pValue,pObj);` |
|   29216 |  323 | `		}` |
|   58434 |  324 | `		nIdx = pObj->nIdx;` |
|   29218 |  325 | `	}else{` |
|   20040 |  326 | `		nIdx = nRefIdx;` |
|       - |  327 | `	}` |
|       - |  328 | `	/* Hash the key */` |
|   78472 |  329 | `	nHash = pMap->xBlobHash(pKey,nKeyLen);` |
|       - |  330 | `	/* Allocate a new blob node */` |
|   78472 |  331 | `	pNode = HashmapNewBlobNode(&(*pMap),pKey,nKeyLen,nHash,nIdx);` |
|   78472 |  332 | `	if( pNode == 0 ){` |
|     ! 0 |  333 | `		return SXERR_MEM;` |
|       - |  334 | `	}` |
|   78472 |  335 | `	if( isForeign ){` |
|       - |  336 | `		/* Mark as a foregin entry */` |
|   20040 |  337 | `		pNode->iFlags \|= HASHMAP_NODE_FOREIGN_OBJ;` |
|   10019 |  338 | `	}` |
|       - |  339 | `	/* Make sure the bucket is big enough to hold the new entry */` |
|   78472 |  340 | `	rc = HashmapGrowBucket(&(*pMap));` |
|   78472 |  341 | `	if( rc != SXRET_OK ){` |
|     ! 0 |  342 | `		SyMemBackendPoolFree(&pMap->pVm->sAllocator,pNode);` |
|     ! 0 |  343 | `		return rc;` |
|       - |  344 | `	}` |
|       - |  345 | `	/* Perform the insertion */` |
|   78472 |  346 | `	HashmapNodeLink(&(*pMap),pNode,nHash & (pMap->nSize - 1));` |
|       - |  347 | `	/* Install in the reference table */` |
|   78472 |  348 | `	PH7_VmRefObjInstall(pMap->pVm,nIdx,0,pNode,0);` |
|       - |  349 | `	/* All done */` |
|   78472 |  350 | `	return SXRET_OK;` |
|   39237 |  351 |  |
|       - |  352 | `/*` |
|       - |  353 | ` * Check if a given 64-bit integer key exists in the given hashmap.` |
|       - |  354 | ` * Write a pointer to the target node on success. Otherwise` |
|       - |  355 | ` * SXERR_NOTFOUND is returned on failure.` |
|       - |  356 | ` */` |
|   46810 |  357 | `static sxi32 HashmapLookupIntKey(` |
|       - |  358 | `	ph7_hashmap *pMap,         /* Target hashmap */` |
|       - |  359 | `	sxi64 iKey,                /* lookup key */` |
|       - |  360 | `	ph7_hashmap_node **ppNode  /* OUT: target node on success */` |
|       - |  361 | `	)` |
|       2 |  362 |  |
|       - |  363 | `	ph7_hashmap_node *pNode;` |
|       - |  364 | `	sxu32 nHash;` |
|   46812 |  365 | `	if( pMap->nEntry < 1 ){` |
|       - |  366 | `		/* Don't bother hashing,there is no entry anyway */` |
|     373 |  367 | `		return SXERR_NOTFOUND;` |
|       - |  368 | `	}` |
|       - |  369 | `	/* Hash the key first */` |
|   46440 |  370 | `	nHash = pMap->xIntHash(iKey);` |
|       - |  371 | `	/* Point to the appropriate bucket */` |
|   46440 |  372 | `	pNode = pMap->apBucket[nHash & (pMap->nSize - 1)];` |
|       - |  373 | `	/* Perform the lookup */` |
|  411576 |  374 | `	for(;;){` |
|  823154 |  375 | `		if( pNode == 0 ){` |
|   45771 |  376 | `			break;` |
|       - |  377 | `		}` |
|  777716 |  378 | `		if( pNode->iType == HASHMAP_INT_NODE` |
|  774364 |  379 | `			&& pNode->nHash == nHash` |
|  386009 |  380 | `			&& pNode->xKey.iKey == iKey ){` |
|       - |  381 | `				/* Node found */` |
|     670 |  382 | `				if( ppNode ){` |
|     658 |  383 | `					*ppNode = pNode;` |
|     328 |  384 | `				}` |
|     670 |  385 | `				return SXRET_OK;` |
|       - |  386 | `		}` |
|       - |  387 | `		/* Follow the collision link */` |
|  776715 |  388 | `		pNode = pNode->pNextCollide;` |
|       1 |  389 | `	}` |
|       - |  390 | `	/* No such entry */` |
|   45771 |  391 | `	return SXERR_NOTFOUND;` |
|   23407 |  392 |  |
|       - |  393 | `/*` |
|       - |  394 | ` * Check if a given BLOB key exists in the given hashmap.` |
|       - |  395 | ` * Write a pointer to the target node on success. Otherwise` |
|       - |  396 | ` * SXERR_NOTFOUND is returned on failure.` |
|       - |  397 | ` */` |
|  155362 |  398 | `static sxi32 HashmapLookupBlobKey(` |
|       - |  399 | `	ph7_hashmap *pMap,          /* Target hashmap */` |
|       - |  400 | `	const void *pKey,           /* Lookup key */` |
|       - |  401 | `	sxu32 nKeyLen,              /* Key length in bytes */` |
|       - |  402 | `	ph7_hashmap_node **ppNode   /* OUT: target node on success */` |
|       - |  403 | `	)` |
|       2 |  404 |  |
|       - |  405 | `	ph7_hashmap_node *pNode;` |
|       - |  406 | `	sxu32 nHash;` |
|  155364 |  407 | `	if( pMap->nEntry < 1 ){` |
|       - |  408 | `		/* Don't bother hashing,there is no entry anyway */` |
|    8112 |  409 | `		return SXERR_NOTFOUND;` |
|       - |  410 | `	}` |
|       - |  411 | `	/* Hash the key first */` |
|  147254 |  412 | `	nHash = pMap->xBlobHash(pKey,nKeyLen);` |
|       - |  413 | `	/* Point to the appropriate bucket */` |
|  147254 |  414 | `	pNode = pMap->apBucket[nHash & (pMap->nSize - 1)];` |
|       - |  415 | `	/* Perform the lookup */` |
|  146905 |  416 | `	for(;;){` |
|  293812 |  417 | `		if( pNode == 0 ){` |
|  111560 |  418 | `			break;` |
|       - |  419 | `		}` |
|  200099 |  420 | `		if( pNode->iType == HASHMAP_BLOB_NODE` |
|  180752 |  421 | `			&& pNode->nHash == nHash` |
|  107473 |  422 | `			&& SyBlobLength(&pNode->xKey.sKey) == nKeyLen` |
|   35696 |  423 | `			&& SyMemcmp(SyBlobData(&pNode->xKey.sKey),pKey,nKeyLen) == 0 ){` |
|       - |  424 | `				/* Node found */` |
|   35696 |  425 | `				if( ppNode ){` |
|   35668 |  426 | `					*ppNode = pNode;` |
|   17833 |  427 | `				}` |
|   35696 |  428 | `				return SXRET_OK;` |
|       - |  429 | `		}` |
|       - |  430 | `		/* Follow the collision link */` |
|  146560 |  431 | `		pNode = pNode->pNextCollide;` |
|       2 |  432 | `	}` |
|       - |  433 | `	/* No such entry */` |
|  111560 |  434 | `	return SXERR_NOTFOUND;` |
|   77683 |  435 |  |
|       - |  436 | `/*` |
|       - |  437 | ` * Check if the given BLOB key looks like a decimal number.` |
|       - |  438 | ` * Retrurn TRUE on success.FALSE otherwise.` |
|       - |  439 | ` */` |
|  155504 |  440 | `static int HashmapIsIntKey(SyBlob *pKey)` |
|       2 |  441 |  |
|  155506 |  442 | `	const char *zIn  = (const char *)SyBlobData(pKey);` |
|  155506 |  443 | `	const char *zEnd = &zIn[SyBlobLength(pKey)];` |
|  155506 |  444 | `	if( (int)(zEnd-zIn) > 1 && zIn[0] == '0' ){` |
|       - |  445 | `		/* Octal not decimal number */` |
|       5 |  446 | `		return FALSE;` |
|       - |  447 | `	}` |
|  155502 |  448 | `	if( (zIn[0] == '-' \|\| zIn[0] == '+') && &zIn[1] < zEnd ){` |
|     ! 0 |  449 | `		zIn++;` |
|     ! 0 |  450 | `	}` |
|   78083 |  451 | `	for(;;){` |
|  156168 |  452 | `		if( zIn >= zEnd ){` |
|     233 |  453 | `			return TRUE;` |
|       - |  454 | `		}` |
|  155936 |  455 | `		if( (unsigned char)zIn[0] >= 0xc0 /* UTF-8 stream */  \|\| !SyisDigit(zIn[0]) ){` |
|   77636 |  456 | `			break;` |
|       - |  457 | `		}` |
|     667 |  458 | `		zIn++;` |
|       1 |  459 | `	}` |
|       - |  460 | `	/* Key does not look like a decimal number */` |
|  155270 |  461 | `	return FALSE;` |
|   77754 |  462 |  |
|       - |  463 | `/*` |
|       - |  464 | ` * Check if a given key exists in the given hashmap.` |
|       - |  465 | ` * Write a pointer to the target node on success.` |
|       - |  466 | ` * Otherwise SXERR_NOTFOUND is returned on failure.` |
|       - |  467 | ` */` |
|   77470 |  468 | `static sxi32 HashmapLookup(` |
|       - |  469 | `	ph7_hashmap *pMap,          /* Target hashmap */` |
|       - |  470 | `	ph7_value *pKey,            /* Lookup key */` |
|       - |  471 | `	ph7_hashmap_node **ppNode   /* OUT: target node on success */` |
|       - |  472 | `	)` |
|       2 |  473 |  |
|   77472 |  474 | `	ph7_hashmap_node *pNode = 0; /* cc -O6 warning */` |
|       - |  475 | `	sxi32 rc;` |
|   77472 |  476 | `	if( pKey->iFlags & (MEMOBJ_STRING\|MEMOBJ_HASHMAP\|MEMOBJ_OBJ\|MEMOBJ_RES) ){` |
|   76886 |  477 | `		if( (pKey->iFlags & MEMOBJ_STRING) == 0 ){` |
|       - |  478 | `			/* Force a string cast */` |
|     ! 0 |  479 | `			PH7_MemObjToString(&(*pKey));` |
|     ! 0 |  480 | `		}` |
|   76886 |  481 | `		if( SyBlobLength(&pKey->sBlob) > 0 && !HashmapIsIntKey(&pKey->sBlob) ){` |
|       - |  482 | `			/* Perform a blob lookup */` |
|   76870 |  483 | `			rc = HashmapLookupBlobKey(&(*pMap),SyBlobData(&pKey->sBlob),SyBlobLength(&pKey->sBlob),&pNode);` |
|   76870 |  484 | `			goto result;` |
|       - |  485 | `		}` |
|       8 |  486 | `	}` |
|       - |  487 | `	/* Perform an int lookup */` |
|     604 |  488 | `	if((pKey->iFlags & MEMOBJ_INT) == 0 ){` |
|       - |  489 | `		/* Force an integer cast */` |
|      27 |  490 | `		PH7_MemObjToInteger(pKey);` |
|      13 |  491 | `	}` |
|       - |  492 | `	/* Perform an int lookup */` |
|     604 |  493 | `	rc = HashmapLookupIntKey(&(*pMap),pKey->x.iVal,&pNode);` |
|   38735 |  494 | `result:` |
|   77472 |  495 | `	if( rc == SXRET_OK ){` |
|       - |  496 | `		/* Node found */` |
|   36204 |  497 | `		if( ppNode ){` |
|   36180 |  498 | `			*ppNode = pNode;` |
|   18089 |  499 | `		}` |
|   36204 |  500 | `		return SXRET_OK;` |
|       - |  501 | `	}` |
|       - |  502 | `	/* No such entry */` |
|   41270 |  503 | `	return SXERR_NOTFOUND;` |
|   38737 |  504 |  |
|       - |  505 | `/*` |
|       - |  506 | ` * Insert a given key and it's associated value (if any) in the given` |
|       - |  507 | ` * hashmap.` |
|       - |  508 | ` * If a node with the given key already exists in the database` |
|       - |  509 | ` * then this function overwrite the old value.` |
|       - |  510 | ` */` |
| 2797326 |  511 | `static sxi32 HashmapInsert(` |
|       - |  512 | `	ph7_hashmap *pMap, /* Target hashmap */` |
|       - |  513 | `	ph7_value *pKey,   /* Lookup key  */` |
|       - |  514 | `	ph7_value *pVal    /* Node value */` |
|       - |  515 | `	)` |
|       2 |  516 |  |
| 2797328 |  517 | `	ph7_hashmap_node *pNode = 0;` |
| 2797328 |  518 | `	sxi32 rc = SXRET_OK;` |
| 2797328 |  519 | `	if( pKey && pKey->iFlags & (MEMOBJ_STRING\|MEMOBJ_HASHMAP\|MEMOBJ_OBJ\|MEMOBJ_RES) ){` |
|   58616 |  520 | `		if( (pKey->iFlags & MEMOBJ_STRING) == 0 ){` |
|       - |  521 | `			/* Force a string cast */` |
|       3 |  522 | `			PH7_MemObjToString(&(*pKey));` |
|       1 |  523 | `		}` |
|   58616 |  524 | `		if( SyBlobLength(&pKey->sBlob) < 1 \|\| HashmapIsIntKey(&pKey->sBlob) ){` |
|     256 |  525 | `			if(SyBlobLength(&pKey->sBlob) < 1){` |
|       - |  526 | `				/* Automatic index assign */` |
|      34 |  527 | `				pKey = 0;` |
|      16 |  528 | `			}` |
|     256 |  529 | `			goto IntKey;` |
|       - |  530 | `		}` |
|   87542 |  531 | `		if( SXRET_OK == HashmapLookupBlobKey(&(*pMap),SyBlobData(&pKey->sBlob),` |
|   29180 |  532 | `			SyBlobLength(&pKey->sBlob),&pNode) ){` |
|       - |  533 | `				/* Overwrite the old value */` |
|       - |  534 | `				ph7_value *pElem;` |
|      25 |  535 | `				pElem = (ph7_value *)SySetAt(&pMap->pVm->aMemObj,pNode->nValIdx);` |
|      25 |  536 | `				if( pElem ){` |
|      25 |  537 | `					if( pVal ){` |
|      25 |  538 | `						PH7_MemObjStore(pVal,pElem);` |
|      13 |  539 | `					}else{` |
|       - |  540 | `						/* Nullify the entry */` |
|     ! 0 |  541 | `						PH7_MemObjToNull(pElem);` |
|       - |  542 | `					}` |
|      12 |  543 | `				}` |
|      25 |  544 | `				return SXRET_OK;` |
|       - |  545 | `		}` |
|   58338 |  546 | `		if( pMap == pMap->pVm->pGlobal ){` |
|       - |  547 | `			/* Forbidden */` |
|       3 |  548 | `			PH7_VmThrowError(pMap->pVm,0,PH7_CTX_NOTICE,"$GLOBALS is a read-only array,insertion is forbidden");` |
|       3 |  549 | `			return SXRET_OK;` |
|       - |  550 | `		}` |
|       - |  551 | `		/* Perform a blob-key insertion */` |
|   58336 |  552 | `		rc = HashmapInsertBlobKey(&(*pMap),SyBlobData(&pKey->sBlob),SyBlobLength(&pKey->sBlob),&(*pVal),0,FALSE);` |
|   58336 |  553 | `		return rc;` |
|       - |  554 | `	}` |
| 1369356 |  555 | `IntKey:` |
| 2738968 |  556 | `	if( pKey ){` |
|   23209 |  557 | `		if((pKey->iFlags & MEMOBJ_INT) == 0 ){` |
|       - |  558 | `			/* Force an integer cast */` |
|     251 |  559 | `			PH7_MemObjToInteger(pKey);` |
|     125 |  560 | `		}` |
|   23209 |  561 | `		if( SXRET_OK == HashmapLookupIntKey(&(*pMap),pKey->x.iVal,&pNode) ){` |
|       - |  562 | `			/* Overwrite the old value */` |
|       - |  563 | `			ph7_value *pElem;` |
|      37 |  564 | `			pElem = (ph7_value *)SySetAt(&pMap->pVm->aMemObj,pNode->nValIdx);` |
|      37 |  565 | `			if( pElem ){` |
|      37 |  566 | `				if( pVal ){` |
|      37 |  567 | `					PH7_MemObjStore(pVal,pElem);` |
|      19 |  568 | `				}else{` |
|       - |  569 | `					/* Nullify the entry */` |
|     ! 0 |  570 | `					PH7_MemObjToNull(pElem);` |
|       - |  571 | `				}` |
|      18 |  572 | `			}` |
|      37 |  573 | `			return SXRET_OK;` |
|       - |  574 | `		}` |
|   23173 |  575 | `		if( pMap == pMap->pVm->pGlobal ){` |
|       - |  576 | `			/* Forbidden */` |
|       3 |  577 | `			PH7_VmThrowError(pMap->pVm,0,PH7_CTX_NOTICE,"$GLOBALS is a read-only array,insertion is forbidden");` |
|       3 |  578 | `			return SXRET_OK;` |
|       - |  579 | `		}` |
|       - |  580 | `		/* Perform a 64-bit-int-key insertion */` |
|   23171 |  581 | `		rc = HashmapInsertIntKey(&(*pMap),pKey->x.iVal,&(*pVal),0,FALSE);` |
|   23171 |  582 | `		if( rc == SXRET_OK ){` |
|   23171 |  583 | `			if( pKey->x.iVal >= pMap->iNextIdx ){` |
|       - |  584 | `				/* Increment the automatic index */` |
|   22943 |  585 | `				pMap->iNextIdx = pKey->x.iVal + 1;` |
|       - |  586 | `				/* Make sure the automatic index is not reserved */` |
|   22943 |  587 | `				while( SXRET_OK == HashmapLookupIntKey(&(*pMap),pMap->iNextIdx,0) ){` |
|     ! 0 |  588 | `					pMap->iNextIdx++;` |
|     ! 0 |  589 | `				}` |
|   11471 |  590 | `			}` |
|   11585 |  591 | `		}` |
|   11586 |  592 | `	}else{` |
| 2715760 |  593 | `		if( pMap == pMap->pVm->pGlobal ){` |
|       - |  594 | `			/* Forbidden */` |
|       3 |  595 | `			PH7_VmThrowError(pMap->pVm,0,PH7_CTX_NOTICE,"$GLOBALS is a read-only array,insertion is forbidden");` |
|       3 |  596 | `			return SXRET_OK;` |
|       - |  597 | `		}` |
|       - |  598 | `		/* Assign an automatic index */` |
| 2715758 |  599 | `		rc = HashmapInsertIntKey(&(*pMap),pMap->iNextIdx,&(*pVal),0,FALSE);` |
| 2715758 |  600 | `		if( rc == SXRET_OK ){` |
| 2715758 |  601 | `			++pMap->iNextIdx;` |
| 1357878 |  602 | `		}` |
|       - |  603 | `	}` |
|       - |  604 | `	/* Insertion result */` |
| 2738928 |  605 | `	return rc;` |
| 1398665 |  606 |  |
|       - |  607 | `/*` |
|       - |  608 | ` * Insert a given key and it's associated value (foreign index) in the given` |
|       - |  609 | ` * hashmap.` |
|       - |  610 | ` * This is insertion by reference so be careful to mark the node` |
|       - |  611 | ` * with the HASHMAP_NODE_FOREIGN_OBJ flag being set.` |
|       - |  612 | ` * The insertion by reference is triggered when the following` |
|       - |  613 | ` * expression is encountered.` |
|       - |  614 | ` * $var = 10;` |
|       - |  615 | ` *  $a = array(&var);` |
|       - |  616 | ` * OR` |
|       - |  617 | ` *  $a[] =& $var;` |
|       - |  618 | ` * That is,$var is a foreign ph7_value and the $a array have no control` |
|       - |  619 | ` * over it's contents.` |
|       - |  620 | ` * Note that the node that hold the foreign ph7_value is automatically` |
|       - |  621 | ` * removed when the foreign ph7_value is unset.` |
|       - |  622 | ` * Example:` |
|       - |  623 | ` *  $var = 10;` |
|       - |  624 | ` *  $a[] =& $var;` |
|       - |  625 | ` *  echo count($a).PHP_EOL; //1` |
|       - |  626 | ` *  //Unset the foreign ph7_value now` |
|       - |  627 | ` *  unset($var);` |
|       - |  628 | ` *  echo count($a); //0` |
|       - |  629 | ` * Note that this is a PH7 eXtension.` |
|       - |  630 | ` * Refer to the official documentation for more information.` |
|       - |  631 | ` * If a node with the given key already exists in the database` |
|       - |  632 | ` * then this function overwrite the old value.` |
|       - |  633 | ` */` |
|   20068 |  634 | `static sxi32 HashmapInsertByRef(` |
|       - |  635 | `	ph7_hashmap *pMap,   /* Target hashmap */` |
|       - |  636 | `	ph7_value *pKey,     /* Lookup key */` |
|       - |  637 | `	sxu32 nRefIdx        /* Foreign ph7_value index */` |
|       - |  638 | `	)` |
|       2 |  639 |  |
|   20070 |  640 | `	ph7_hashmap_node *pNode = 0;` |
|   20070 |  641 | `	sxi32 rc = SXRET_OK;` |
|   20070 |  642 | `	if( pKey && pKey->iFlags & (MEMOBJ_STRING\|MEMOBJ_HASHMAP\|MEMOBJ_OBJ\|MEMOBJ_RES) ){` |
|   20046 |  643 | `		if( (pKey->iFlags & MEMOBJ_STRING) == 0 ){` |
|       - |  644 | `			/* Force a string cast */` |
|     ! 0 |  645 | `			PH7_MemObjToString(&(*pKey));` |
|     ! 0 |  646 | `		}` |
|   20046 |  647 | `		if( SyBlobLength(&pKey->sBlob) < 1 \|\| HashmapIsIntKey(&pKey->sBlob) ){` |
|     ! 0 |  648 | `			if(SyBlobLength(&pKey->sBlob) < 1){` |
|       - |  649 | `				/* Automatic index assign */` |
|     ! 0 |  650 | `				pKey = 0;` |
|     ! 0 |  651 | `			}` |
|     ! 0 |  652 | `			goto IntKey;` |
|       - |  653 | `		}` |
|   30068 |  654 | `		if( SXRET_OK == HashmapLookupBlobKey(&(*pMap),SyBlobData(&pKey->sBlob),` |
|   10022 |  655 | `			SyBlobLength(&pKey->sBlob),&pNode) ){` |
|       - |  656 | `				/* Overwrite */` |
|       7 |  657 | `				PH7_VmRefObjRemove(pMap->pVm,pNode->nValIdx,0,pNode);` |
|       7 |  658 | `				pNode->nValIdx = nRefIdx;` |
|       - |  659 | `				/* Install in the reference table */` |
|       7 |  660 | `				PH7_VmRefObjInstall(pMap->pVm,nRefIdx,0,pNode,0);` |
|       7 |  661 | `				return SXRET_OK;` |
|       - |  662 | `		}` |
|       - |  663 | `		/* Perform a blob-key insertion */` |
|   20040 |  664 | `		rc = HashmapInsertBlobKey(&(*pMap),SyBlobData(&pKey->sBlob),SyBlobLength(&pKey->sBlob),0,nRefIdx,TRUE);` |
|   20040 |  665 | `		return rc;` |
|       - |  666 | `	}` |
|      12 |  667 | `IntKey:` |
|      25 |  668 | `	if( pKey ){` |
|       3 |  669 | `		if((pKey->iFlags & MEMOBJ_INT) == 0 ){` |
|       - |  670 | `			/* Force an integer cast */` |
|     ! 0 |  671 | `			PH7_MemObjToInteger(pKey);` |
|     ! 0 |  672 | `		}` |
|       3 |  673 | `		if( SXRET_OK == HashmapLookupIntKey(&(*pMap),pKey->x.iVal,&pNode) ){` |
|       - |  674 | `			/* Overwrite */` |
|     ! 0 |  675 | `			PH7_VmRefObjRemove(pMap->pVm,pNode->nValIdx,0,pNode);` |
|     ! 0 |  676 | `			pNode->nValIdx = nRefIdx;` |
|       - |  677 | `			/* Install in the reference table */` |
|     ! 0 |  678 | `			PH7_VmRefObjInstall(pMap->pVm,nRefIdx,0,pNode,0);` |
|     ! 0 |  679 | `			return SXRET_OK;` |
|       - |  680 | `		}` |
|       - |  681 | `		/* Perform a 64-bit-int-key insertion */` |
|       3 |  682 | `		rc = HashmapInsertIntKey(&(*pMap),pKey->x.iVal,0,nRefIdx,TRUE);` |
|       3 |  683 | `		if( rc == SXRET_OK ){` |
|       3 |  684 | `			if( pKey->x.iVal >= pMap->iNextIdx ){` |
|       - |  685 | `				/* Increment the automatic index */` |
|       3 |  686 | `				pMap->iNextIdx = pKey->x.iVal + 1;` |
|       - |  687 | `				/* Make sure the automatic index is not reserved */` |
|       3 |  688 | `				while( SXRET_OK == HashmapLookupIntKey(&(*pMap),pMap->iNextIdx,0) ){` |
|     ! 0 |  689 | `					pMap->iNextIdx++;` |
|     ! 0 |  690 | `				}` |
|       1 |  691 | `			}` |
|       1 |  692 | `		}` |
|       2 |  693 | `	}else{` |
|       - |  694 | `		/* Assign an automatic index */` |
|      23 |  695 | `		rc = HashmapInsertIntKey(&(*pMap),pMap->iNextIdx,0,nRefIdx,TRUE);` |
|      23 |  696 | `		if( rc == SXRET_OK ){` |
|      23 |  697 | `			++pMap->iNextIdx;` |
|      11 |  698 | `		}` |
|       - |  699 | `	}` |
|       - |  700 | `	/* Insertion result */` |
|      25 |  701 | `	return rc;` |
|   10036 |  702 |  |
|       - |  703 | `/*` |
|       - |  704 | ` * Extract node value.` |
|       - |  705 | ` */` |
|  852372 |  706 | `static ph7_value * HashmapExtractNodeValue(ph7_hashmap_node *pNode)` |
|       2 |  707 |  |
|       - |  708 | `	/* Point to the desired object */` |
|       - |  709 | `	ph7_value *pObj;` |
|  852374 |  710 | `	pObj = (ph7_value *)SySetAt(&pNode->pMap->pVm->aMemObj,pNode->nValIdx);` |
|  852374 |  711 | `	return pObj;` |
|       2 |  712 |  |
|       - |  713 | `/*` |
|       - |  714 | ` * Insert a node in the given hashmap.` |
|       - |  715 | ` * If a node with the given key already exists in the database` |
|       - |  716 | ` * then this function overwrite the old value.` |
|       - |  717 | ` */` |
|     352 |  718 | `static sxi32 HashmapInsertNode(ph7_hashmap *pMap,ph7_hashmap_node *pNode,int bPreserve)` |
|       1 |  719 |  |
|       - |  720 | `	ph7_value *pObj;` |
|       - |  721 | `	sxi32 rc;` |
|       - |  722 | `	/* Extract the node value */` |
|     353 |  723 | `	pObj = HashmapExtractNodeValue(&(*pNode));` |
|     353 |  724 | `	if( pObj == 0 ){` |
|     ! 0 |  725 | `		return SXERR_EMPTY;` |
|       - |  726 | `	}` |
|       - |  727 | `	/* Preserve key */` |
|     353 |  728 | `	if( pNode->iType == HASHMAP_INT_NODE){` |
|       - |  729 | `		/* Int64 key */` |
|     225 |  730 | `		if( !bPreserve ){` |
|       - |  731 | `			/* Assign an automatic index */` |
|      85 |  732 | `			rc = HashmapInsert(&(*pMap),0,pObj);` |
|      43 |  733 | `		}else{` |
|     141 |  734 | `			rc = HashmapInsertIntKey(&(*pMap),pNode->xKey.iKey,pObj,0,FALSE);` |
|       - |  735 | `		}` |
|     113 |  736 | `	}else{` |
|       - |  737 | `		/* Blob key */` |
|     129 |  738 | `		if( !bPreserve ){` |
|       - |  739 | `			/* treat it like an automatically-indexed element, drop the` |
|       - |  740 | `			 * original string key entirely */` |
|      33 |  741 | `			rc = HashmapInsert(&(*pMap),0,pObj);` |
|      17 |  742 | `		}else{` |
|     145 |  743 | `			rc = HashmapInsertBlobKey(&(*pMap),SyBlobData(&pNode->xKey.sKey),` |
|      48 |  744 | `				SyBlobLength(&pNode->xKey.sKey),pObj,0,FALSE);` |
|       - |  745 | `		}` |
|       - |  746 | `	}` |
|     353 |  747 | `	return rc;` |
|     177 |  748 |  |
|       - |  749 | `/*` |
|       - |  750 | ` * Compare two node values.` |
|       - |  751 | ` * Return 0 if the node values are equals, > 0 if pLeft is greater than pRight` |
|       - |  752 | ` * or < 0 if pRight is greater than pLeft.` |
|       - |  753 | ` * For a full description on ph7_values comparison,refer to the implementation` |
|       - |  754 | ` * of the [PH7_MemObjCmp()] function defined in memobj.c or the official` |
|       - |  755 | ` * documenation.` |
|       - |  756 | ` */` |
|   36126 |  757 | `static sxi32 HashmapNodeCmp(ph7_hashmap_node *pLeft,ph7_hashmap_node *pRight,int bStrict)` |
|       2 |  758 |  |
|       - |  759 | `	ph7_value sObj1,sObj2;` |
|       - |  760 | `	sxi32 rc;` |
|   36128 |  761 | `	if( pLeft == pRight ){` |
|       - |  762 | `		/*` |
|       - |  763 | `		 * Same node.Refer to the sort() implementation defined` |
|       - |  764 | `		 * below for more information on this sceanario.` |
|       - |  765 | `		 */` |
|     ! 0 |  766 | `		return 0;` |
|       - |  767 | `	}` |
|       - |  768 | `	/* Do the comparison */` |
|   36128 |  769 | `	PH7_MemObjInit(pLeft->pMap->pVm,&sObj1);` |
|   36128 |  770 | `	PH7_MemObjInit(pLeft->pMap->pVm,&sObj2);` |
|   36128 |  771 | `	PH7_HashmapExtractNodeValue(pLeft,&sObj1,FALSE);` |
|   36128 |  772 | `	PH7_HashmapExtractNodeValue(pRight,&sObj2,FALSE);` |
|   36128 |  773 | `	rc = PH7_MemObjCmp(&sObj1,&sObj2,bStrict,0);` |
|   36128 |  774 | `	PH7_MemObjRelease(&sObj1);` |
|   36128 |  775 | `	PH7_MemObjRelease(&sObj2);` |
|   36128 |  776 | `	return rc;` |
|   18096 |  777 |  |
|       - |  778 | `/*` |
|       - |  779 | ` * Rehash a node with a 64-bit integer key.` |
|       - |  780 | ` * Refer to [merge_sort(),array_shift()] implementations for more information.` |
|       - |  781 | ` */` |
|    7984 |  782 | `static void HashmapRehashIntNode(ph7_hashmap_node *pEntry)` |
|       2 |  783 |  |
|    7986 |  784 | `	ph7_hashmap *pMap = pEntry->pMap;` |
|       - |  785 | `	sxu32 nBucket;` |
|       - |  786 | `	/* Remove old collision links */` |
|    7986 |  787 | `	if( pEntry->pPrevCollide ){` |
|    6368 |  788 | `		pEntry->pPrevCollide->pNextCollide = pEntry->pNextCollide;` |
|    3178 |  789 | `	}else{` |
|    1620 |  790 | `		pMap->apBucket[pEntry->nHash & (pMap->nSize - 1)] = pEntry->pNextCollide;` |
|       - |  791 | `	}` |
|    7986 |  792 | `	if( pEntry->pNextCollide ){` |
|     639 |  793 | `		pEntry->pNextCollide->pPrevCollide = pEntry->pPrevCollide;` |
|     317 |  794 | `	}` |
|    7986 |  795 | `	pEntry->pNextCollide = pEntry->pPrevCollide = 0;` |
|       - |  796 | `	/* Compute the new hash */` |
|    7986 |  797 | `	pEntry->nHash = pMap->xIntHash(pMap->iNextIdx);` |
|    7986 |  798 | `	pEntry->xKey.iKey = pMap->iNextIdx;` |
|    7986 |  799 | `	nBucket = pEntry->nHash & (pMap->nSize - 1);` |
|       - |  800 | `	/* Link to the new bucket */` |
|    7986 |  801 | `	pEntry->pNextCollide = pMap->apBucket[nBucket];` |
|    7986 |  802 | `	if( pMap->apBucket[nBucket] ){` |
|    6537 |  803 | `		pMap->apBucket[nBucket]->pPrevCollide = pEntry;` |
|    3267 |  804 | `	}` |
|    7986 |  805 | `	pEntry->pNextCollide = pMap->apBucket[nBucket];` |
|    7986 |  806 | `	pMap->apBucket[nBucket] = pEntry;` |
|       - |  807 | `	/* Increment the automatic index */` |
|    7986 |  808 | `	pMap->iNextIdx++;` |
|    7986 |  809 |  |
|       - |  810 | `/*` |
|       - |  811 | ` * Perform a linear search on a given hashmap.` |
|       - |  812 | ` * Write a pointer to the target node on success.` |
|       - |  813 | ` * Otherwise SXERR_NOTFOUND is returned on failure.` |
|       - |  814 | ` * Refer to [array_intersect(),array_diff(),in_array(),...] implementations` |
|       - |  815 | ` * for more information.` |
|       - |  816 | ` */` |
|   20294 |  817 | `static int HashmapFindValue(` |
|       - |  818 | `	ph7_hashmap *pMap,   /* Target hashmap */` |
|       - |  819 | `	ph7_value *pNeedle,  /* Lookup key */` |
|       - |  820 | `	ph7_hashmap_node **ppNode, /* OUT: target node on success  */` |
|       - |  821 | `	int bStrict      /* TRUE for strict comparison */` |
|       - |  822 | `	)` |
|       2 |  823 |  |
|       - |  824 | `	ph7_hashmap_node *pEntry;` |
|       - |  825 | `	ph7_value sVal,*pVal;` |
|       - |  826 | `	ph7_value sNeedle;` |
|       - |  827 | `	sxi32 rc;` |
|       - |  828 | `	sxu32 n;` |
|       - |  829 | `	/* Perform a linear search since we cannot sort the hashmap based on values */` |
|   20296 |  830 | `	pEntry = pMap->pFirst;` |
|   20296 |  831 | `	n = pMap->nEntry;` |
|   20296 |  832 | `	PH7_MemObjInit(pMap->pVm,&sVal);` |
|   20296 |  833 | `	PH7_MemObjInit(pMap->pVm,&sNeedle);` |
|   48535 |  834 | `	for(;;){` |
|   97072 |  835 | `		if( n < 1 ){` |
|      99 |  836 | `			break;` |
|       - |  837 | `		}` |
|       - |  838 | `		/* Extract node value */` |
|   96974 |  839 | `		pVal = HashmapExtractNodeValue(pEntry);` |
|   96974 |  840 | `		if( pVal ){` |
|   96974 |  841 | `			if( (pVal->iFlags\|pNeedle->iFlags) & MEMOBJ_NULL ){` |
|     ! 0 |  842 | `				sxi32 iF1 = pVal->iFlags&~MEMOBJ_AUX;` |
|     ! 0 |  843 | `				sxi32 iF2 = pNeedle->iFlags&~MEMOBJ_AUX;` |
|     ! 0 |  844 | `				if( iF1 == iF2 ){` |
|       - |  845 | `					/* NULL values are equals */` |
|     ! 0 |  846 | `					if( ppNode ){` |
|     ! 0 |  847 | `						*ppNode = pEntry;` |
|     ! 0 |  848 | `					}` |
|     ! 0 |  849 | `					return SXRET_OK;` |
|       - |  850 | `				}` |
|     ! 0 |  851 | `			}else{` |
|       - |  852 | `				/* Duplicate value */` |
|   96974 |  853 | `				PH7_MemObjLoad(pVal,&sVal);` |
|   96974 |  854 | `				PH7_MemObjLoad(pNeedle,&sNeedle);` |
|   96974 |  855 | `				rc = PH7_MemObjCmp(&sNeedle,&sVal,bStrict,0);` |
|   96974 |  856 | `				PH7_MemObjRelease(&sVal);` |
|   96974 |  857 | `				PH7_MemObjRelease(&sNeedle);` |
|   96974 |  858 | `				if( rc == 0 ){` |
|   20198 |  859 | `					if( ppNode ){` |
|      23 |  860 | `						*ppNode = pEntry;` |
|      11 |  861 | `					}` |
|       - |  862 | `					/* Match found*/` |
|   20198 |  863 | `					return SXRET_OK;` |
|       - |  864 | `				}` |
|       - |  865 | `			}` |
|   38388 |  866 | `		}` |
|       - |  867 | `		/* Point to the next entry */` |
|   76778 |  868 | `		pEntry = pEntry->pPrev; /* Reverse link */` |
|   76778 |  869 | `		n--;` |
|       2 |  870 | `	}` |
|       - |  871 | `	/* No such entry */` |
|      99 |  872 | `	return SXERR_NOTFOUND;` |
|   10149 |  873 |  |
|       - |  874 | `/*` |
|       - |  875 | ` * Perform a linear search on a given hashmap but use an user-defined callback` |
|       - |  876 | ` * for values comparison.` |
|       - |  877 | ` * Write a pointer to the target node on success.` |
|       - |  878 | ` * Otherwise SXERR_NOTFOUND is returned on failure.` |
|       - |  879 | ` * Refer to [array_uintersect(),array_udiff()...] implementations` |
|       - |  880 | ` * for more information.` |
|       - |  881 | ` */` |
|      12 |  882 | `static int HashmapFindValueByCallback(` |
|       - |  883 | `	ph7_hashmap *pMap,     /* Target hashmap */` |
|       - |  884 | `	ph7_value *pNeedle,    /* Lookup key */` |
|       - |  885 | `	ph7_value *pCallback,  /* User defined callback */` |
|       - |  886 | `	ph7_hashmap_node **ppNode /* OUT: target node on success */` |
|       - |  887 | `	)` |
|       1 |  888 |  |
|       - |  889 | `	ph7_hashmap_node *pEntry;` |
|       - |  890 | `	ph7_value sResult,*pVal;` |
|       - |  891 | `	ph7_value *apArg[2];    /* Callback arguments */` |
|       - |  892 | `	sxi32 rc;` |
|       - |  893 | `	sxu32 n;` |
|       - |  894 | `	/* Perform a linear search since we cannot sort the array based on values */` |
|      13 |  895 | `	pEntry = pMap->pFirst;` |
|      13 |  896 | `	n = pMap->nEntry;` |
|       - |  897 | `	/* Store callback result here */` |
|      13 |  898 | `	PH7_MemObjInit(pMap->pVm,&sResult);` |
|       - |  899 | `	/* First argument to the callback */` |
|      13 |  900 | `	apArg[0] = pNeedle;` |
|      15 |  901 | `	for(;;){` |
|      31 |  902 | `		if( n < 1 ){` |
|       7 |  903 | `			break;` |
|       - |  904 | `		}` |
|       - |  905 | `		/* Extract node value */` |
|      25 |  906 | `		pVal = HashmapExtractNodeValue(pEntry);` |
|      25 |  907 | `		if( pVal ){` |
|       - |  908 | `			/* Invoke the user callback */` |
|      25 |  909 | `			apArg[1] = pVal; /* Second argument to the callback */` |
|      25 |  910 | `			rc = PH7_VmCallUserFunction(pMap->pVm,pCallback,2,apArg,&sResult);` |
|      25 |  911 | `			if( rc == SXRET_OK ){` |
|       - |  912 | `				/* Extract callback result */` |
|      25 |  913 | `				if( (sResult.iFlags & MEMOBJ_INT) == 0 ){` |
|       - |  914 | `					/* Perform an int cast */` |
|     ! 0 |  915 | `					PH7_MemObjToInteger(&sResult);` |
|     ! 0 |  916 | `				}` |
|      25 |  917 | `				rc = (sxi32)sResult.x.iVal;` |
|      25 |  918 | `				PH7_MemObjRelease(&sResult);` |
|      25 |  919 | `				if( rc == 0 ){` |
|       - |  920 | `					/* Match found*/` |
|       7 |  921 | `					if( ppNode ){` |
|     ! 0 |  922 | `						*ppNode = pEntry;` |
|     ! 0 |  923 | `					}` |
|       7 |  924 | `					return SXRET_OK;` |
|       - |  925 | `				}` |
|       9 |  926 | `			}` |
|       9 |  927 | `		}` |
|       - |  928 | `		/* Point to the next entry */` |
|      19 |  929 | `		pEntry = pEntry->pPrev; /* Reverse link */` |
|      19 |  930 | `		n--;` |
|       1 |  931 | `	}` |
|       - |  932 | `	/* No such entry */` |
|       7 |  933 | `	return SXERR_NOTFOUND;` |
|       7 |  934 |  |
|       - |  935 | `/*` |
|       - |  936 | ` * Compare two hashmaps.` |
|       - |  937 | ` * Return 0 if the hashmaps are equals.Any other value indicates inequality.` |
|       - |  938 | ` * Note on array comparison operators.` |
|       - |  939 | ` *  According to the PHP language reference manual.` |
|       - |  940 | ` *  Array Operators Example 	Name 	Result` |
|       - |  941 | ` *  $a + $b 	Union 	Union of $a and $b.` |
|       - |  942 | ` *  $a == $b 	Equality 	TRUE if $a and $b have the same key/value pairs.` |
|       - |  943 | ` *  $a === $b 	Identity 	TRUE if $a and $b have the same key/value pairs in the same` |
|       - |  944 | ` *                          order and of the same types.` |
|       - |  945 | ` *  $a != $b 	Inequality 	TRUE if $a is not equal to $b.` |
|       - |  946 | ` *  $a <> $b 	Inequality 	TRUE if $a is not equal to $b.` |
|       - |  947 | ` *  $a !== $b 	Non-identity 	TRUE if $a is not identical to $b.` |
|       - |  948 | ` * The + operator returns the right-hand array appended to the left-hand array;` |
|       - |  949 | ` * For keys that exist in both arrays, the elements from the left-hand array will be used` |
|       - |  950 | ` * and the matching elements from the right-hand array will be ignored.` |
|       - |  951 | ` * <?php` |
|       - |  952 | ` * $a = array("a" => "apple", "b" => "banana");` |
|       - |  953 | ` * $b = array("a" => "pear", "b" => "strawberry", "c" => "cherry");` |
|       - |  954 | ` * $c = $a + $b; // Union of $a and $b` |
|       - |  955 | ` * echo "Union of \$a and \$b: \n";` |
|       - |  956 | ` * var_dump($c);` |
|       - |  957 | ` * $c = $b + $a; // Union of $b and $a` |
|       - |  958 | ` * echo "Union of \$b and \$a: \n";` |
|       - |  959 | ` * var_dump($c);` |
|       - |  960 | ` * ?>` |
|       - |  961 | ` * When executed, this script will print the following:` |
|       - |  962 | ` * Union of $a and $b:` |
|       - |  963 | ` * array(3) {` |
|       - |  964 | ` *  ["a"]=>` |
|       - |  965 | ` *  string(5) "apple"` |
|       - |  966 | ` *  ["b"]=>` |
|       - |  967 | ` * string(6) "banana"` |
|       - |  968 | ` *  ["c"]=>` |
|       - |  969 | ` * string(6) "cherry"` |
|       - |  970 | ` * }` |
|       - |  971 | ` * Union of $b and $a:` |
|       - |  972 | ` * array(3) {` |
|       - |  973 | ` * ["a"]=>` |
|       - |  974 | ` * string(4) "pear"` |
|       - |  975 | ` * ["b"]=>` |
|       - |  976 | ` * string(10) "strawberry"` |
|       - |  977 | ` * ["c"]=>` |
|       - |  978 | ` * string(6) "cherry"` |
|       - |  979 | ` * }` |
|       - |  980 | ` * Elements of arrays are equal for the comparison if they have the same key and value.` |
|       - |  981 | ` */` |
|       8 |  982 | `PH7_PRIVATE sxi32 PH7_HashmapCmp(` |
|       - |  983 | `	ph7_hashmap *pLeft,  /* Left hashmap */` |
|       - |  984 | `	ph7_hashmap *pRight, /* Right hashmap */` |
|       - |  985 | `	int bStrict          /* TRUE for strict comparison */` |
|       - |  986 | `	)` |
|       1 |  987 |  |
|       - |  988 | `	ph7_hashmap_node *pLe,*pRe;` |
|       - |  989 | `	sxi32 rc;` |
|       - |  990 | `	sxu32 n;` |
|       9 |  991 | `	if( pLeft == pRight ){` |
|       - |  992 | `		/* Same hashmap instance. This can easily happen since hashmaps are passed by reference.` |
|       - |  993 | `		 * Unlike the zend engine.` |
|       - |  994 | `		 */` |
|     ! 0 |  995 | `		return 0;` |
|       - |  996 | `	}` |
|       9 |  997 | `	if( pLeft->nEntry != pRight->nEntry ){` |
|       - |  998 | `		/* Must have the same number of entries */` |
|     ! 0 |  999 | `		return pLeft->nEntry > pRight->nEntry ? 1 : -1;` |
|       - | 1000 | `	}` |
|       - | 1001 | `	/* Point to the first inserted entry of the left hashmap */` |
|       9 | 1002 | `	pLe = pLeft->pFirst;` |
|       9 | 1003 | `	pRe = 0; /* cc warning */` |
|       - | 1004 | `	/* Perform the comparison */` |
|       9 | 1005 | `	n = pLeft->nEntry;` |
|       8 | 1006 | `	for(;;){` |
|      17 | 1007 | `		if( n < 1 ){` |
|       7 | 1008 | `			break;` |
|       - | 1009 | `		}` |
|      11 | 1010 | `		if( pLe->iType == HASHMAP_INT_NODE){` |
|       - | 1011 | `			/* Int key */` |
|       7 | 1012 | `			rc = HashmapLookupIntKey(&(*pRight),pLe->xKey.iKey,&pRe);` |
|       4 | 1013 | `		}else{` |
|       5 | 1014 | `			SyBlob *pKey = &pLe->xKey.sKey;` |
|       - | 1015 | `			/* Blob key */` |
|       5 | 1016 | `			rc = HashmapLookupBlobKey(&(*pRight),SyBlobData(pKey),SyBlobLength(pKey),&pRe);` |
|       - | 1017 | `		}` |
|      11 | 1018 | `		if( rc != SXRET_OK ){` |
|       - | 1019 | `			/* No such entry in the right side */` |
|     ! 0 | 1020 | `			return 1;` |
|       - | 1021 | `		}` |
|      11 | 1022 | `		rc = 0;` |
|      11 | 1023 | `		if( bStrict ){` |
|       - | 1024 | `			/* Make sure,the keys are of the same type */` |
|       3 | 1025 | `			if( pLe->iType != pRe->iType ){` |
|     ! 0 | 1026 | `				rc = 1;` |
|     ! 0 | 1027 | `			}` |
|       1 | 1028 | `		}` |
|      11 | 1029 | `		if( !rc ){` |
|       - | 1030 | `			/* Compare nodes */` |
|      11 | 1031 | `			rc = HashmapNodeCmp(pLe,pRe,bStrict);` |
|       5 | 1032 | `		}` |
|      11 | 1033 | `		if( rc != 0 ){` |
|       - | 1034 | `			/* Nodes key/value differ */` |
|       3 | 1035 | `			return rc;` |
|       - | 1036 | `		}` |
|       - | 1037 | `		/* Point to the next entry */` |
|       9 | 1038 | `		pLe = pLe->pPrev; /* Reverse link */` |
|       9 | 1039 | `		n--;` |
|       1 | 1040 | `	}` |
|       7 | 1041 | `	return 0; /* Hashmaps are equals */` |
|       5 | 1042 |  |
|       - | 1043 | `/*` |
|       - | 1044 | ` * Duplicate a hashmap node.` |
|       - | 1045 | ` * This function is used by HashmapMerge, HashmapOverwrite and PH7_HashmapDup.` |
|       - | 1046 | ` */` |
|  409774 | 1047 | `static sxi32 HashmapDuplicateNode(` |
|       - | 1048 | `	ph7_hashmap *pDest,` |
|       - | 1049 | `	ph7_hashmap_node *pEntry,` |
|       - | 1050 | `	ph7_value *pVal,` |
|       - | 1051 | `	int iAction /* 0: Merge, 1: Overwrite, 2: Dup */` |
|       - | 1052 | `	)` |
|       2 | 1053 |  |
|  409776 | 1054 | `	ph7_value sSafeVal = *pVal;` |
|       - | 1055 | `	ph7_value sKey;` |
|       - | 1056 | `	sxi32 rc;` |
|       - | 1057 |  |
|  409776 | 1058 | `	if( pEntry->iType == HASHMAP_BLOB_NODE ){` |
|       - | 1059 | `		/* Blob key insertion */` |
|      31 | 1060 | `		PH7_MemObjInitFromString(pDest->pVm,&sKey,0);` |
|      31 | 1061 | `		PH7_MemObjStringAppend(&sKey,(const char *)SyBlobData(&pEntry->xKey.sKey),SyBlobLength(&pEntry->xKey.sKey));` |
|      31 | 1062 | `		rc = PH7_HashmapInsert(pDest,&sKey,&sSafeVal);` |
|      31 | 1063 | `		PH7_MemObjRelease(&sKey);` |
|      16 | 1064 | `	}else{` |
|       - | 1065 | `		/* Int key */` |
|  409746 | 1066 | `		if( iAction == 0 ){ /* Merge */` |
|  409700 | 1067 | `			rc = HashmapInsert(pDest,0/* Automatic index assign */,&sSafeVal);` |
|  204897 | 1068 | `		}else if( iAction == 1 ){ /* Overwrite */` |
|       5 | 1069 | `			PH7_MemObjInitFromInt(pDest->pVm,&sKey,pEntry->xKey.iKey);` |
|       5 | 1070 | `			rc = PH7_HashmapInsert(pDest,&sKey,&sSafeVal);` |
|       5 | 1071 | `			PH7_MemObjRelease(&sKey);` |
|       3 | 1072 | `		}else{ /* Dup */` |
|      44 | 1073 | `			rc = HashmapInsertIntKey(pDest,pEntry->xKey.iKey,&sSafeVal,0,FALSE);` |
|       - | 1074 | `		}` |
|       - | 1075 | `	}` |
|  409776 | 1076 | `	return rc;` |
|       2 | 1077 |  |
|       - | 1078 | `/*` |
|       - | 1079 | ` * Merge two hashmaps.` |
|       - | 1080 | ` * Note on the merge process` |
|       - | 1081 | ` * According to the PHP language reference manual.` |
|       - | 1082 | ` *  Merges the elements of two arrays together so that the values of one are appended` |
|       - | 1083 | ` *  to the end of the previous one. It returns the resulting array (pDest).` |
|       - | 1084 | ` *  If the input arrays have the same string keys, then the later value for that key` |
|       - | 1085 | ` *  will overwrite the previous one. If, however, the arrays contain numeric keys` |
|       - | 1086 | ` *  the later value will not overwrite the original value, but will be appended.` |
|       - | 1087 | ` *  Values in the input array with numeric keys will be renumbered with incrementing` |
|       - | 1088 | ` *  keys starting from zero in the result array.` |
|       - | 1089 | ` */` |
|    1712 | 1090 | `static sxi32 HashmapMerge(ph7_hashmap *pSrc,ph7_hashmap *pDest)` |
|       2 | 1091 |  |
|       - | 1092 | `	ph7_hashmap_node *pEntry;` |
|       - | 1093 | `	ph7_value *pVal;` |
|       - | 1094 | `	sxi32 rc;` |
|       - | 1095 | `	sxu32 n;` |
|    1714 | 1096 | `	if( pSrc == pDest ){` |
|       - | 1097 | `		/* Same map. This can easily happen since hashmaps are passed by reference.` |
|       - | 1098 | `		 * Unlike the zend engine.` |
|       - | 1099 | `		 */` |
|     ! 0 | 1100 | `		return SXRET_OK;` |
|       - | 1101 | `	}` |
|       - | 1102 | `	/* Point to the first inserted entry in the source */` |
|    1714 | 1103 | `	pEntry = pSrc->pFirst;` |
|       - | 1104 | `	/* Perform the merge */` |
|  411428 | 1105 | `	for( n = 0 ; n < pSrc->nEntry ; ++n ){` |
|       - | 1106 | `		/* Extract the node value */` |
|  409716 | 1107 | `		pVal = HashmapExtractNodeValue(pEntry);` |
|  409716 | 1108 | `		if( pVal ){` |
|       - | 1109 | `			/* Make a local copy of the value.` |
|       - | 1110 | `			 * The insertion call below may trigger a memory pool reallocation` |
|       - | 1111 | `			 * which will invalidate the 'pVal' pointer since it points` |
|       - | 1112 | `			 * to the old pool.` |
|       - | 1113 | `			 */` |
|  409716 | 1114 | `			rc = HashmapDuplicateNode(pDest,pEntry,pVal,0);` |
|  204859 | 1115 | `		}else{` |
|     ! 0 | 1116 | `			rc = SXRET_OK;` |
|       - | 1117 | `		}` |
|  409716 | 1118 | `		if( rc != SXRET_OK ){` |
|     ! 0 | 1119 | `			return rc;` |
|       - | 1120 | `		}` |
|       - | 1121 | `		/* Point to the next entry */` |
|  409716 | 1122 | `		pEntry = pEntry->pPrev; /* Reverse link */` |
|  204859 | 1123 | `	}` |
|    1714 | 1124 | `	return SXRET_OK;` |
|     858 | 1125 |  |
|       - | 1126 | `/*` |
|       - | 1127 | ` * Overwrite entries with the same key.` |
|       - | 1128 | ` * Refer to the [array_replace()] implementation for more information.` |
|       - | 1129 | ` *  According to the PHP language reference manual.` |
|       - | 1130 | ` *  array_replace() replaces the values of the first array with the same values` |
|       - | 1131 | ` *  from all the following arrays. If a key from the first array exists in the second` |
|       - | 1132 | ` *  array, its value will be replaced by the value from the second array. If the key` |
|       - | 1133 | ` *  exists in the second array, and not the first, it will be created in the first array.` |
|       - | 1134 | ` *  If a key only exists in the first array, it will be left as is. If several arrays` |
|       - | 1135 | ` *  are passed for replacement, they will be processed in order, the later arrays` |
|       - | 1136 | ` *  overwriting the previous values.` |
|       - | 1137 | ` *  array_replace() is not recursive : it will replace values in the first array` |
|       - | 1138 | ` *  by whatever type is in the second array.` |
|       - | 1139 | ` */` |
|       4 | 1140 | `static sxi32 HashmapOverwrite(ph7_hashmap *pSrc,ph7_hashmap *pDest)` |
|       1 | 1141 |  |
|       - | 1142 | `	ph7_hashmap_node *pEntry;` |
|       - | 1143 | `	ph7_value *pVal;` |
|       - | 1144 | `	sxi32 rc;` |
|       - | 1145 | `	sxu32 n;` |
|       5 | 1146 | `	if( pSrc == pDest ){` |
|       - | 1147 | `		/* Same map. This can easily happen since hashmaps are passed by reference.` |
|       - | 1148 | `		 * Unlike the zend engine.` |
|       - | 1149 | `		 */` |
|     ! 0 | 1150 | `		return SXRET_OK;` |
|       - | 1151 | `	}` |
|       - | 1152 | `	/* Point to the first inserted entry in the source */` |
|       5 | 1153 | `	pEntry = pSrc->pFirst;` |
|       - | 1154 | `	/* Perform the merge */` |
|      13 | 1155 | `	for( n = 0 ; n < pSrc->nEntry ; ++n ){` |
|       - | 1156 | `		/* Extract the node value */` |
|       9 | 1157 | `		pVal = HashmapExtractNodeValue(pEntry);` |
|       9 | 1158 | `		if( pVal ){` |
|       9 | 1159 | `			rc = HashmapDuplicateNode(pDest,pEntry,pVal,1);` |
|       5 | 1160 | `		}else{` |
|     ! 0 | 1161 | `			rc = SXRET_OK;` |
|       - | 1162 | `		}` |
|       9 | 1163 | `		if( rc != SXRET_OK ){` |
|     ! 0 | 1164 | `			return rc;` |
|       - | 1165 | `		}` |
|       - | 1166 | `		/* Point to the next entry */` |
|       9 | 1167 | `		pEntry = pEntry->pPrev; /* Reverse link */` |
|       5 | 1168 | `	}` |
|       5 | 1169 | `	return SXRET_OK;` |
|       3 | 1170 |  |
|       - | 1171 | `/*` |
|       - | 1172 | ` * Duplicate the contents of a hashmap. Store the copy in pDest.` |
|       - | 1173 | ` * Refer to the [array_pad(),array_copy(),...] implementation for more information.` |
|       - | 1174 | ` */` |
|      30 | 1175 | `PH7_PRIVATE sxi32 PH7_HashmapDup(ph7_hashmap *pSrc,ph7_hashmap *pDest)` |
|       2 | 1176 |  |
|       - | 1177 | `	ph7_hashmap_node *pEntry;` |
|       - | 1178 | `	ph7_value *pVal;` |
|       - | 1179 | `	sxi32 rc;` |
|       - | 1180 | `	sxu32 n;` |
|      32 | 1181 | `	if( pSrc == pDest ){` |
|       - | 1182 | `		/* Same map. This can easily happen since hashmaps are passed by reference.` |
|       - | 1183 | `		 * Unlike the zend engine.` |
|       - | 1184 | `		 */` |
|     ! 0 | 1185 | `		return SXRET_OK;` |
|       - | 1186 | `	}` |
|       - | 1187 | `	/* Point to the first inserted entry in the source */` |
|      32 | 1188 | `	pEntry = pSrc->pFirst;` |
|       - | 1189 | `	/* Perform the duplication */` |
|      84 | 1190 | `	for( n = 0 ; n < pSrc->nEntry ; ++n ){` |
|       - | 1191 | `		/* Extract the node value */` |
|      54 | 1192 | `		pVal = HashmapExtractNodeValue(pEntry);` |
|      54 | 1193 | `		if( pVal ){` |
|      54 | 1194 | `			rc = HashmapDuplicateNode(pDest,pEntry,pVal,2);` |
|      28 | 1195 | `		}else{` |
|     ! 0 | 1196 | `			rc = SXRET_OK;` |
|       - | 1197 | `		}` |
|      54 | 1198 | `		if( rc != SXRET_OK ){` |
|     ! 0 | 1199 | `			return rc;` |
|       - | 1200 | `		}` |
|       - | 1201 | `		/* Point to the next entry */` |
|      54 | 1202 | `		pEntry = pEntry->pPrev; /* Reverse link */` |
|      28 | 1203 | `	}` |
|      32 | 1204 | `	return SXRET_OK;` |
|      17 | 1205 |  |
|       - | 1206 | `/*` |
|       - | 1207 | ` * Perform the union of two hashmaps.` |
|       - | 1208 | ` * This operation is performed only if the user uses the '+' operator` |
|       - | 1209 | ` * with a variable holding an array as follows:` |
|       - | 1210 | ` * <?php` |
|       - | 1211 | ` * $a = array("a" => "apple", "b" => "banana");` |
|       - | 1212 | ` * $b = array("a" => "pear", "b" => "strawberry", "c" => "cherry");` |
|       - | 1213 | ` * $c = $a + $b; // Union of $a and $b` |
|       - | 1214 | ` * echo "Union of \$a and \$b: \n";` |
|       - | 1215 | ` * var_dump($c);` |
|       - | 1216 | ` * $c = $b + $a; // Union of $b and $a` |
|       - | 1217 | ` * echo "Union of \$b and \$a: \n";` |
|       - | 1218 | ` * var_dump($c);` |
|       - | 1219 | ` * ?>` |
|       - | 1220 | ` * When executed, this script will print the following:` |
|       - | 1221 | ` * Union of $a and $b:` |
|       - | 1222 | ` * array(3) {` |
|       - | 1223 | ` *  ["a"]=>` |
|       - | 1224 | ` *  string(5) "apple"` |
|       - | 1225 | ` *  ["b"]=>` |
|       - | 1226 | ` * string(6) "banana"` |
|       - | 1227 | ` *  ["c"]=>` |
|       - | 1228 | ` * string(6) "cherry"` |
|       - | 1229 | ` * }` |
|       - | 1230 | ` * Union of $b and $a:` |
|       - | 1231 | ` * array(3) {` |
|       - | 1232 | ` * ["a"]=>` |
|       - | 1233 | ` * string(4) "pear"` |
|       - | 1234 | ` * ["b"]=>` |
|       - | 1235 | ` * string(10) "strawberry"` |
|       - | 1236 | ` * ["c"]=>` |
|       - | 1237 | ` * string(6) "cherry"` |
|       - | 1238 | ` * }` |
|       - | 1239 | ` * The + operator returns the right-hand array appended to the left-hand array;` |
|       - | 1240 | ` * For keys that exist in both arrays, the elements from the left-hand array will be used` |
|       - | 1241 | ` * and the matching elements from the right-hand array will be ignored.` |
|       - | 1242 | ` */` |
|       4 | 1243 | `PH7_PRIVATE sxi32 PH7_HashmapUnion(ph7_hashmap *pLeft,ph7_hashmap *pRight)` |
|       2 | 1244 |  |
|       - | 1245 | `	ph7_hashmap_node *pEntry;` |
|       6 | 1246 | `	sxi32 rc = SXRET_OK;` |
|       - | 1247 | `	ph7_value *pObj;` |
|       - | 1248 | `	sxu32 n;` |
|       6 | 1249 | `	if( pLeft == pRight ){` |
|       - | 1250 | `		/* Same map. This can easily happen since hashmaps are passed by reference.` |
|       - | 1251 | `		 * Unlike the zend engine.` |
|       - | 1252 | `		 */` |
|     ! 0 | 1253 | `		return SXRET_OK;` |
|       - | 1254 | `	}` |
|       - | 1255 | `	/* Perform the union */` |
|       6 | 1256 | `	pEntry = pRight->pFirst;` |
|      16 | 1257 | `	for(n = 0 ; n < pRight->nEntry ; ++n ){` |
|       - | 1258 | `		/* Make sure the given key does not exists in the left array */` |
|      12 | 1259 | `		if( pEntry->iType == HASHMAP_BLOB_NODE ){` |
|       - | 1260 | `			/* BLOB key */` |
|       7 | 1261 | `			if( SXRET_OK !=` |
|       6 | 1262 | `				HashmapLookupBlobKey(&(*pLeft),SyBlobData(&pEntry->xKey.sKey),SyBlobLength(&pEntry->xKey.sKey),0) ){` |
|       3 | 1263 | `					pObj = HashmapExtractNodeValue(pEntry);` |
|       3 | 1264 | `					if( pObj ){` |
|       3 | 1265 | `						ph7_value sSafeVal = *pObj;` |
|       - | 1266 | `						/* Perform the insertion */` |
|       3 | 1267 | `						rc = HashmapInsertBlobKey(&(*pLeft),SyBlobData(&pEntry->xKey.sKey),SyBlobLength(&pEntry->xKey.sKey),` |
|       - | 1268 | `							&sSafeVal,0,FALSE);` |
|       3 | 1269 | `						if( rc != SXRET_OK ){` |
|     ! 0 | 1270 | `							return rc;` |
|       - | 1271 | `						}` |
|       1 | 1272 | `					}` |
|       1 | 1273 | `			}` |
|       4 | 1274 | `		}else{` |
|       - | 1275 | `			/* INT key */` |
|       5 | 1276 | `			if( SXRET_OK != HashmapLookupIntKey(&(*pLeft),pEntry->xKey.iKey,0) ){` |
|     ! 0 | 1277 | `				pObj = HashmapExtractNodeValue(pEntry);` |
|     ! 0 | 1278 | `				if( pObj ){` |
|     ! 0 | 1279 | `					ph7_value sSafeVal = *pObj;` |
|       - | 1280 | `					/* Perform the insertion */` |
|     ! 0 | 1281 | `					rc = HashmapInsertIntKey(&(*pLeft),pEntry->xKey.iKey,&sSafeVal,0,FALSE);` |
|     ! 0 | 1282 | `					if( rc != SXRET_OK ){` |
|     ! 0 | 1283 | `						return rc;` |
|       - | 1284 | `					}` |
|     ! 0 | 1285 | `				}` |
|     ! 0 | 1286 | `			}` |
|       - | 1287 | `		}` |
|       - | 1288 | `		/* Point to the next entry */` |
|      12 | 1289 | `		pEntry = pEntry->pPrev; /* Reverse link */` |
|       7 | 1290 | `	}` |
|       6 | 1291 | `	return SXRET_OK;` |
|       4 | 1292 |  |
|       - | 1293 | `/*` |
|       - | 1294 | ` * Allocate a new hashmap.` |
|       - | 1295 | ` * Return a pointer to the freshly allocated hashmap on success.NULL otherwise.` |
|       - | 1296 | ` */` |
|   53648 | 1297 | `PH7_PRIVATE ph7_hashmap * PH7_NewHashmap(` |
|       - | 1298 | `	ph7_vm *pVm,              /* VM that trigger the hashmap creation */` |
|       - | 1299 | `	sxu32 (*xIntHash)(sxi64), /* Hash function for int keys.NULL otherwise*/` |
|       - | 1300 | `	sxu32 (*xBlobHash)(const void *,sxu32) /* Hash function for BLOB keys.NULL otherwise */` |
|       - | 1301 | `	)` |
|       2 | 1302 |  |
|       - | 1303 | `	ph7_hashmap *pMap;` |
|       - | 1304 | `	/* Allocate a new instance */` |
|   53650 | 1305 | `	pMap = (ph7_hashmap *)SyMemBackendPoolAlloc(&pVm->sAllocator,sizeof(ph7_hashmap));` |
|   53650 | 1306 | `	if( pMap == 0 ){` |
|     ! 0 | 1307 | `		return 0;` |
|       - | 1308 | `	}` |
|       - | 1309 | `	/* Zero the structure */` |
|   53650 | 1310 | `	SyZero(pMap,sizeof(ph7_hashmap));` |
|       - | 1311 | `	/* Fill in the structure */` |
|   53650 | 1312 | `	pMap->pVm = &(*pVm);` |
|   53650 | 1313 | `	pMap->iRef = 1;` |
|       - | 1314 | `	/* Default hash functions */` |
|   53650 | 1315 | `	pMap->xIntHash  = xIntHash ? xIntHash : IntHash;` |
|   53650 | 1316 | `	pMap->xBlobHash = xBlobHash ? xBlobHash : BinHash;` |
|   53650 | 1317 | `	return pMap;` |
|   26826 | 1318 |  |
|       - | 1319 | `/*` |
|       - | 1320 | ` * Install superglobals in the given virtual machine.` |
|       - | 1321 | ` * Note on superglobals.` |
|       - | 1322 | ` *  According to the PHP language reference manual.` |
|       - | 1323 | ` *  Superglobals are built-in variables that are always available in all scopes.` |
|       - | 1324 | `*   Description` |
|       - | 1325 | `*   Several predefined variables in PHP are "superglobals", which means they` |
|       - | 1326 | `*   are available in all scopes throughout a script. There is no need to do` |
|       - | 1327 | `*   global $variable; to access them within functions or methods.` |
|       - | 1328 | `*   These superglobal variables are:` |
|       - | 1329 | `*    $GLOBALS` |
|       - | 1330 | `*    $_SERVER` |
|       - | 1331 | `*    $_GET` |
|       - | 1332 | `*    $_POST` |
|       - | 1333 | `*    $_FILES` |
|       - | 1334 | `*    $_COOKIE` |
|       - | 1335 | `*    $_SESSION` |
|       - | 1336 | `*    $_REQUEST` |
|       - | 1337 | `*    $_ENV` |
|       - | 1338 | `*/` |
|    1472 | 1339 | `PH7_PRIVATE sxi32 PH7_HashmapCreateSuper(ph7_vm *pVm)` |
|       2 | 1340 |  |
|       - | 1341 | `	static const char * azSuper[] = {` |
|       - | 1342 | `		"_SERVER",   /* $_SERVER */` |
|       - | 1343 | `		"_GET",      /* $_GET */` |
|       - | 1344 | `		"_POST",     /* $_POST */` |
|       - | 1345 | `		"_FILES",    /* $_FILES */` |
|       - | 1346 | `		"_COOKIE",   /* $_COOKIE */` |
|       - | 1347 | `		"_SESSION",  /* $_SESSION */` |
|       - | 1348 | `		"_REQUEST",  /* $_REQUEST */` |
|       - | 1349 | `		"_ENV",      /* $_ENV */` |
|       - | 1350 | `		"_HEADER",   /* $_HEADER */` |
|       - | 1351 | `		"argv"       /* $argv */` |
|       - | 1352 | `	};` |
|       - | 1353 | `	ph7_hashmap *pMap;` |
|       - | 1354 | `	ph7_value *pObj;` |
|       - | 1355 | `	SyString *pFile;` |
|       - | 1356 | `	sxi32 rc;` |
|       - | 1357 | `	sxu32 n;` |
|       - | 1358 | `	/* Allocate a new hashmap for the $GLOBALS array */` |
|    1474 | 1359 | `	pMap = PH7_NewHashmap(&(*pVm),0,0);` |
|    1474 | 1360 | `	if( pMap == 0 ){` |
|     ! 0 | 1361 | `		return SXERR_MEM;` |
|       - | 1362 | `	}` |
|    1474 | 1363 | `	pVm->pGlobal = pMap;` |
|       - | 1364 | `	/* Reserve a ph7_value for the $GLOBALS array*/` |
|    1474 | 1365 | `	pObj = PH7_ReserveMemObj(&(*pVm));` |
|    1474 | 1366 | `	if( pObj == 0 ){` |
|     ! 0 | 1367 | `		return SXERR_MEM;` |
|       - | 1368 | `	}` |
|    1474 | 1369 | `	PH7_MemObjInitFromArray(&(*pVm),pObj,pMap);` |
|       - | 1370 | `	/* Record object index */` |
|    1474 | 1371 | `	pVm->nGlobalIdx = pObj->nIdx;` |
|       - | 1372 | `	/* Install the special $GLOBALS array */` |
|    1474 | 1373 | `	rc = SyHashInsert(&pVm->hSuper,(const void *)"GLOBALS",sizeof("GLOBALS")-1,SX_INT_TO_PTR(pVm->nGlobalIdx));` |
|    1474 | 1374 | `	if( rc != SXRET_OK ){` |
|     ! 0 | 1375 | `		return rc;` |
|       - | 1376 | `	}` |
|       - | 1377 | `	/* Install superglobals now */` |
|   16194 | 1378 | `	for( n =  0 ; n < SX_ARRAYSIZE(azSuper)  ; n++ ){` |
|       - | 1379 | `		ph7_value *pSuper;` |
|       - | 1380 | `		/* Request an empty array */` |
|   14722 | 1381 | `		pSuper = ph7_new_array(&(*pVm));` |
|   14722 | 1382 | `		if( pSuper == 0 ){` |
|     ! 0 | 1383 | `			return SXERR_MEM;` |
|       - | 1384 | `		}` |
|       - | 1385 | `		/* Install */` |
|   14722 | 1386 | `		rc = ph7_vm_config(&(*pVm),PH7_VM_CONFIG_CREATE_SUPER,azSuper[n]/* Super-global name*/,pSuper/* Super-global value */);` |
|   14722 | 1387 | `		if( rc != SXRET_OK ){` |
|     ! 0 | 1388 | `			return rc;` |
|       - | 1389 | `		}` |
|       - | 1390 | `		/* Release the value now it have been installed */` |
|   14722 | 1391 | `		ph7_release_value(&(*pVm),pSuper);` |
|    7362 | 1392 | `	}` |
|       - | 1393 | `	/* Set some $_SERVER entries */` |
|    1474 | 1394 | `	pFile = (SyString *)SySetPeek(&pVm->aFiles);` |
|       - | 1395 | `	/*` |
|       - | 1396 | `	 * 'SCRIPT_FILENAME'` |
|       - | 1397 | `	 * The absolute pathname of the currently executing script.` |
|       - | 1398 | `	 */` |
|    2942 | 1399 | `	ph7_vm_config(pVm,PH7_VM_CONFIG_SERVER_ATTR,` |
|       - | 1400 | `		"SCRIPT_FILENAME",` |
|     736 | 1401 | `		pFile ? pFile->zString : ":Memory:",` |
|    1468 | 1402 | `		pFile ? pFile->nByte : sizeof(":Memory:") - 1` |
|       - | 1403 | `		);` |
|       - | 1404 | `	/* All done,all super-global are installed now */` |
|    1474 | 1405 | `	return SXRET_OK;` |
|     738 | 1406 |  |
|       - | 1407 | `/*` |
|       - | 1408 | ` * Release a hashmap.` |
|       - | 1409 | ` */` |
|   37412 | 1410 | `PH7_PRIVATE sxi32 PH7_HashmapRelease(ph7_hashmap *pMap,int FreeDS)` |
|       2 | 1411 |  |
|       - | 1412 | `	ph7_hashmap_node *pEntry,*pNext;` |
|   37414 | 1413 | `	ph7_vm *pVm = pMap->pVm;` |
|       - | 1414 | `	sxu32 n;` |
|   37414 | 1415 | `	if( pMap == pVm->pGlobal ){` |
|       - | 1416 | `		/* Cannot delete the $GLOBALS array */` |
|     ! 0 | 1417 | `		PH7_VmThrowError(pMap->pVm,0,PH7_CTX_NOTICE,"$GLOBALS is a read-only array,deletion is forbidden");` |
|     ! 0 | 1418 | `		return SXRET_OK;` |
|       - | 1419 | `	}` |
|       - | 1420 | `	/* Start the release process */` |
|   37414 | 1421 | `	n = 0;` |
|   37414 | 1422 | `	pEntry = pMap->pFirst;` |
| 1414469 | 1423 | `	for(;;){` |
| 2828940 | 1424 | `		if( n >= pMap->nEntry ){` |
|   37414 | 1425 | `			break;` |
|       - | 1426 | `		}` |
| 2791528 | 1427 | `		pNext = pEntry->pPrev; /* Reverse link */` |
|       - | 1428 | `		/* Remove the reference from the foreign table */` |
| 2791528 | 1429 | `		PH7_VmRefObjRemove(pVm,pEntry->nValIdx,0,pEntry);` |
| 2791528 | 1430 | `		if( (pEntry->iFlags & HASHMAP_NODE_FOREIGN_OBJ) == 0 ){` |
|       - | 1431 | `			/* Restore the ph7_value to the free list */` |
| 2791520 | 1432 | `			PH7_VmUnsetMemObj(pVm,pEntry->nValIdx,FALSE);` |
| 1395759 | 1433 | `		}` |
|       - | 1434 | `		/* Release the node */` |
| 2791528 | 1435 | `		if( pEntry->iType == HASHMAP_BLOB_NODE ){` |
|   56326 | 1436 | `			SyBlobRelease(&pEntry->xKey.sKey);` |
|   28162 | 1437 | `		}` |
| 2791528 | 1438 | `		SyMemBackendPoolFree(&pVm->sAllocator,pEntry);` |
|       - | 1439 | `		/* Point to the next entry */` |
| 2791528 | 1440 | `		pEntry = pNext;` |
| 2791528 | 1441 | `		n++;` |
|       2 | 1442 | `	}` |
|   37414 | 1443 | `	if( pMap->nEntry > 0 ){` |
|       - | 1444 | `		/* Release the hash bucket */` |
|   33312 | 1445 | `		SyMemBackendFree(&pVm->sAllocator,pMap->apBucket);` |
|   16655 | 1446 | `	}` |
|   37414 | 1447 | `	if( FreeDS ){` |
|       - | 1448 | `		/* Free the whole instance */` |
|   37398 | 1449 | `		SyMemBackendPoolFree(&pVm->sAllocator,pMap);` |
|   18700 | 1450 | `	}else{` |
|       - | 1451 | `		/* Keep the instance but reset it's fields */` |
|      17 | 1452 | `		pMap->apBucket = 0;` |
|      17 | 1453 | `		pMap->iNextIdx = 0;` |
|      17 | 1454 | `		pMap->nEntry = pMap->nSize = 0;` |
|      17 | 1455 | `		pMap->pFirst = pMap->pLast = pMap->pCur = 0;` |
|       - | 1456 | `	}` |
|   37414 | 1457 | `	return SXRET_OK;` |
|   18708 | 1458 |  |
|       - | 1459 | `/*` |
|       - | 1460 | ` * Decrement the reference count of a given hashmap.` |
|       - | 1461 | ` * If the count reaches zero which mean no more variables` |
|       - | 1462 | ` * are pointing to this hashmap,then release the whole instance.` |
|       - | 1463 | ` */` |
|  431092 | 1464 | `PH7_PRIVATE void  PH7_HashmapUnref(ph7_hashmap *pMap)` |
|       2 | 1465 |  |
|  431094 | 1466 | `	ph7_vm *pVm = pMap->pVm;` |
|       - | 1467 | `	/* TICKET 1432-49: $GLOBALS is not subject to garbage collection */` |
|  431094 | 1468 | `	pMap->iRef--;` |
|  431094 | 1469 | `	if( pMap->iRef < 1 && pMap != pVm->pGlobal){` |
|   37398 | 1470 | `		PH7_HashmapRelease(pMap,TRUE);` |
|   18698 | 1471 | `	}` |
|  431094 | 1472 |  |
|       - | 1473 | `/*` |
|       - | 1474 | ` * Check if a given key exists in the given hashmap.` |
|       - | 1475 | ` * Write a pointer to the target node on success.` |
|       - | 1476 | ` * Otherwise SXERR_NOTFOUND is returned on failure.` |
|       - | 1477 | ` */` |
|   77478 | 1478 | `PH7_PRIVATE sxi32 PH7_HashmapLookup(` |
|       - | 1479 | `	ph7_hashmap *pMap,        /* Target hashmap */` |
|       - | 1480 | `	ph7_value *pKey,          /* Lookup key */` |
|       - | 1481 | `	ph7_hashmap_node **ppNode /* OUT: Target node on success */` |
|       - | 1482 | `	)` |
|       2 | 1483 |  |
|       - | 1484 | `	sxi32 rc;` |
|   77480 | 1485 | `	if( pMap->nEntry < 1 ){` |
|       - | 1486 | `		/* TICKET 1433-25: Don't bother hashing,the hashmap is empty anyway.` |
|       - | 1487 | `		 */` |
|       9 | 1488 | `		return SXERR_NOTFOUND;` |
|       - | 1489 | `	}` |
|   77472 | 1490 | `	rc = HashmapLookup(&(*pMap),&(*pKey),ppNode);` |
|   77472 | 1491 | `	return rc;` |
|   38741 | 1492 |  |
|       - | 1493 | `/*` |
|       - | 1494 | ` * Insert a given key and it's associated value (if any) in the given` |
|       - | 1495 | ` * hashmap.` |
|       - | 1496 | ` * If a node with the given key already exists in the database` |
|       - | 1497 | ` * then this function overwrite the old value.` |
|       - | 1498 | ` */` |
| 2387512 | 1499 | `PH7_PRIVATE sxi32 PH7_HashmapInsert(` |
|       - | 1500 | `	ph7_hashmap *pMap, /* Target hashmap */` |
|       - | 1501 | `	ph7_value *pKey,   /* Lookup key */` |
|       - | 1502 | `	ph7_value *pVal    /* Node value.NULL otherwise */` |
|       - | 1503 | `	)` |
|       2 | 1504 |  |
|       - | 1505 | `	sxi32 rc;` |
| 2387514 | 1506 | `	if( pVal && (pVal->iFlags & MEMOBJ_HASHMAP) && (ph7_hashmap *)pVal->x.pOther == pMap->pVm->pGlobal ){` |
|       - | 1507 | `		/*` |
|       - | 1508 | `		 * TICKET 1433-35: Insertion in the $GLOBALS array is forbidden.` |
|       - | 1509 | `		 */` |
|     ! 0 | 1510 | `		PH7_VmThrowError(pMap->pVm,0,PH7_CTX_ERR,"$GLOBALS is a read-only array,insertion is forbidden");` |
|     ! 0 | 1511 | `		return SXRET_OK;` |
|       - | 1512 | `	}` |
| 2387514 | 1513 | `	rc = HashmapInsert(&(*pMap),&(*pKey),&(*pVal));` |
| 2387514 | 1514 | `	return rc;` |
| 1193758 | 1515 |  |
|       - | 1516 | `/*` |
|       - | 1517 | ` * Insert a given key and it's associated value (foreign index) in the given` |
|       - | 1518 | ` * hashmap.` |
|       - | 1519 | ` * This is insertion by reference so be careful to mark the node` |
|       - | 1520 | ` * with the HASHMAP_NODE_FOREIGN_OBJ flag being set.` |
|       - | 1521 | ` * The insertion by reference is triggered when the following` |
|       - | 1522 | ` * expression is encountered.` |
|       - | 1523 | ` * $var = 10;` |
|       - | 1524 | ` *  $a = array(&var);` |
|       - | 1525 | ` * OR` |
|       - | 1526 | ` *  $a[] =& $var;` |
|       - | 1527 | ` * That is,$var is a foreign ph7_value and the $a array have no control` |
|       - | 1528 | ` * over it's contents.` |
|       - | 1529 | ` * Note that the node that hold the foreign ph7_value is automatically` |
|       - | 1530 | ` * removed when the foreign ph7_value is unset.` |
|       - | 1531 | ` * Example:` |
|       - | 1532 | ` *  $var = 10;` |
|       - | 1533 | ` *  $a[] =& $var;` |
|       - | 1534 | ` *  echo count($a).PHP_EOL; //1` |
|       - | 1535 | ` *  //Unset the foreign ph7_value now` |
|       - | 1536 | ` *  unset($var);` |
|       - | 1537 | ` *  echo count($a); //0` |
|       - | 1538 | ` * Note that this is a PH7 eXtension.` |
|       - | 1539 | ` * Refer to the official documentation for more information.` |
|       - | 1540 | ` * If a node with the given key already exists in the database` |
|       - | 1541 | ` * then this function overwrite the old value.` |
|       - | 1542 | ` */` |
|   20068 | 1543 | `PH7_PRIVATE sxi32 PH7_HashmapInsertByRef(` |
|       - | 1544 | `	ph7_hashmap *pMap, /* Target hashmap */` |
|       - | 1545 | `	ph7_value *pKey,   /* Lookup key */` |
|       - | 1546 | `	sxu32 nRefIdx      /* Foreign ph7_value index */` |
|       - | 1547 | `	)` |
|       2 | 1548 |  |
|       - | 1549 | `	sxi32 rc;` |
|   20070 | 1550 | `	if( nRefIdx == pMap->pVm->nGlobalIdx ){` |
|       - | 1551 | `		/*` |
|       - | 1552 | `		 * TICKET 1433-35: Insertion in the $GLOBALS array is forbidden.` |
|       - | 1553 | `		 */` |
|     ! 0 | 1554 | `		PH7_VmThrowError(pMap->pVm,0,PH7_CTX_ERR,"$GLOBALS is a read-only array,insertion is forbidden");` |
|     ! 0 | 1555 | `		return SXRET_OK;` |
|       - | 1556 | `	}` |
|   20070 | 1557 | `	rc = HashmapInsertByRef(&(*pMap),&(*pKey),nRefIdx);` |
|   20070 | 1558 | `	return rc;` |
|   10036 | 1559 |  |
|       - | 1560 | `/*` |
|       - | 1561 | ` * Reset the node cursor of a given hashmap.` |
|       - | 1562 | ` */` |
|   16628 | 1563 | `PH7_PRIVATE void PH7_HashmapResetLoopCursor(ph7_hashmap *pMap)` |
|       2 | 1564 |  |
|       - | 1565 | `	/* Reset the loop cursor */` |
|   16630 | 1566 | `	pMap->pCur = pMap->pFirst;` |
|   16630 | 1567 |  |
|       - | 1568 | `/*` |
|       - | 1569 | ` * Return a pointer to the node currently pointed by the node cursor.` |
|       - | 1570 | ` * If the cursor reaches the end of the list,then this function` |
|       - | 1571 | ` * return NULL.` |
|       - | 1572 | ` * Note that the node cursor is automatically advanced by this function.` |
|       - | 1573 | ` */` |
|  135718 | 1574 | `PH7_PRIVATE ph7_hashmap_node * PH7_HashmapGetNextEntry(ph7_hashmap *pMap)` |
|       2 | 1575 |  |
|  135720 | 1576 | `	ph7_hashmap_node *pCur = pMap->pCur;` |
|  135720 | 1577 | `	if( pCur == 0 ){` |
|       - | 1578 | `		/* End of the list,return null */` |
|    8318 | 1579 | `		return 0;` |
|       - | 1580 | `	}` |
|       - | 1581 | `	/* Advance the node cursor */` |
|  127404 | 1582 | `	pMap->pCur = pCur->pPrev; /* Reverse link */` |
|  127404 | 1583 | `	return pCur;` |
|   67861 | 1584 |  |
|       - | 1585 | `/*` |
|       - | 1586 | ` * Extract a node value.` |
|       - | 1587 | ` */` |
|  321998 | 1588 | `PH7_PRIVATE void PH7_HashmapExtractNodeValue(ph7_hashmap_node *pNode,ph7_value *pValue,int bStore)` |
|       2 | 1589 |  |
|  322000 | 1590 | `	ph7_value *pEntry = HashmapExtractNodeValue(pNode);` |
|  322000 | 1591 | `	if( pEntry ){` |
|  322000 | 1592 | `		if( bStore ){` |
|  127458 | 1593 | `			PH7_MemObjStore(pEntry,pValue);` |
|   63730 | 1594 | `		}else{` |
|  194544 | 1595 | `			PH7_MemObjLoad(pEntry,pValue);` |
|       - | 1596 | `		}` |
|  161063 | 1597 | `	}else{` |
|     ! 0 | 1598 | `		PH7_MemObjRelease(pValue);` |
|       - | 1599 | `	}` |
|  322000 | 1600 |  |
|       - | 1601 | `/*` |
|       - | 1602 | ` * Extract a node key.` |
|       - | 1603 | ` */` |
|   86516 | 1604 | `PH7_PRIVATE void PH7_HashmapExtractNodeKey(ph7_hashmap_node *pNode,ph7_value *pKey)` |
|       2 | 1605 |  |
|       - | 1606 | `	/* Fill with the current key */` |
|   86518 | 1607 | `	if( pNode->iType == HASHMAP_INT_NODE ){` |
|   86364 | 1608 | `		if( SyBlobLength(&pKey->sBlob) > 0 ){` |
|      13 | 1609 | `			SyBlobRelease(&pKey->sBlob);` |
|       6 | 1610 | `		}` |
|   86364 | 1611 | `		pKey->x.iVal = pNode->xKey.iKey;` |
|   86364 | 1612 | `		MemObjSetType(pKey,MEMOBJ_INT);` |
|   43183 | 1613 | `	}else{` |
|     155 | 1614 | `		SyBlobReset(&pKey->sBlob);` |
|     155 | 1615 | `		SyBlobAppend(&pKey->sBlob,SyBlobData(&pNode->xKey.sKey),SyBlobLength(&pNode->xKey.sKey));` |
|     155 | 1616 | `		MemObjSetType(pKey,MEMOBJ_STRING);` |
|       - | 1617 | `	}` |
|   86518 | 1618 |  |
|       - | 1619 | `#ifndef PH7_DISABLE_DISK_IO` |
|       - | 1620 | `/*` |
|       - | 1621 | ` * Store the address of nodes value in the given container.` |
|       - | 1622 | ` * Refer to the [vfprintf(),vprintf(),vsprintf()] implementations` |
|       - | 1623 | ` * defined in 'builtin.c' for more information.` |
|       - | 1624 | ` */` |
|      10 | 1625 | `PH7_PRIVATE int PH7_HashmapValuesToSet(ph7_hashmap *pMap,SySet *pOut)` |
|       1 | 1626 |  |
|      11 | 1627 | `	ph7_hashmap_node *pEntry = pMap->pFirst;` |
|       - | 1628 | `	ph7_value *pValue;` |
|       - | 1629 | `	sxu32 n;` |
|       - | 1630 | `	/* Initialize the container */` |
|      11 | 1631 | `	SySetInit(pOut,&pMap->pVm->sAllocator,sizeof(ph7_value *));` |
|      27 | 1632 | `	for(n = 0 ; n < pMap->nEntry ; n++ ){` |
|       - | 1633 | `		/* Extract node value */` |
|      17 | 1634 | `		pValue = HashmapExtractNodeValue(pEntry);` |
|      17 | 1635 | `		if( pValue ){` |
|      17 | 1636 | `			SySetPut(pOut,(const void *)&pValue);` |
|       8 | 1637 | `		}` |
|       - | 1638 | `		/* Point to the next entry */` |
|      17 | 1639 | `		pEntry = pEntry->pPrev; /* Reverse link */` |
|       9 | 1640 | `	}` |
|       - | 1641 | `	/* Total inserted entries */` |
|      11 | 1642 | `	return (int)SySetUsed(pOut);` |
|       1 | 1643 |  |
|       - | 1644 | `#endif /* PH7_DISABLE_BUILTIN_FUNC \|\| PH7_DISABLE_DISK_IO */` |
|       - | 1645 | `/*` |
|       - | 1646 | ` * Merge sort.` |
|       - | 1647 | ` * The merge sort implementation is based on the one found in the SQLite3 source tree.` |
|       - | 1648 | ` * Status: Public domain` |
|       - | 1649 | ` */` |
|       - | 1650 | `/* Node comparison callback signature */` |
|       - | 1651 | `typedef sxi32 (*ProcNodeCmp)(ph7_hashmap_node *,ph7_hashmap_node *,void *);` |
|       - | 1652 | `/*` |
|       - | 1653 | `** Inputs:` |
|       - | 1654 | `**   a:       A sorted, null-terminated linked list.  (May be null).` |
|       - | 1655 | `**   b:       A sorted, null-terminated linked list.  (May be null).` |
|       - | 1656 | `**   cmp:     A pointer to the comparison function.` |
|       - | 1657 | `**` |
|       - | 1658 | `** Return Value:` |
|       - | 1659 | `**   A pointer to the head of a sorted list containing the elements` |
|       - | 1660 | `**   of both a and b.` |
|       - | 1661 | `**` |
|       - | 1662 | `** Side effects:` |
|       - | 1663 | `**   The "next","prev" pointers for elements in the lists a and b are` |
|       - | 1664 | `**   changed.` |
|       - | 1665 | `*/` |
|   23456 | 1666 | `static ph7_hashmap_node * HashmapNodeMerge(ph7_hashmap_node *pA,ph7_hashmap_node *pB,ProcNodeCmp xCmp,void *pCmpData)` |
|       2 | 1667 |  |
|       - | 1668 | `	ph7_hashmap_node result,*pTail;` |
|       - | 1669 | `    /* Prevent compiler warning */` |
|   23458 | 1670 | `	result.pNext = result.pPrev = 0;` |
|   23458 | 1671 | `	pTail = &result;` |
|   59630 | 1672 | `	while( pA && pB ){` |
|   36174 | 1673 | `		if( xCmp(pA,pB,pCmpData) < 0 ){` |
|   23595 | 1674 | `			pTail->pPrev = pA;` |
|   23595 | 1675 | `			pA->pNext = pTail;` |
|   23595 | 1676 | `			pTail = pA;` |
|   23595 | 1677 | `			pA = pA->pPrev;` |
|   11805 | 1678 | `		}else{` |
|   12581 | 1679 | `			pTail->pPrev = pB;` |
|   12581 | 1680 | `			pB->pNext = pTail;` |
|   12581 | 1681 | `			pTail = pB;` |
|   12581 | 1682 | `			pB = pB->pPrev;` |
|       - | 1683 | `		}` |
|       2 | 1684 | `	}` |
|   23458 | 1685 | `	if( pA ){` |
|   17422 | 1686 | `		pTail->pPrev = pA;` |
|   17422 | 1687 | `		pA->pNext = pTail;` |
|   14758 | 1688 | `	}else if( pB ){` |
|    5890 | 1689 | `		pTail->pPrev = pB;` |
|    5890 | 1690 | `		pB->pNext = pTail;` |
|    2936 | 1691 | `	}else{` |
|     150 | 1692 | `		pTail->pPrev = pTail->pNext = 0;` |
|       - | 1693 | `	}` |
|   23458 | 1694 | `	return result.pPrev;` |
|       2 | 1695 |  |
|       - | 1696 | `/*` |
|       - | 1697 | `** Inputs:` |
|       - | 1698 | `**   Map:       Input hashmap` |
|       - | 1699 | `**   cmp:       A comparison function.` |
|       - | 1700 | `**` |
|       - | 1701 | `** Return Value:` |
|       - | 1702 | `**   Sorted hashmap.` |
|       - | 1703 | `**` |
|       - | 1704 | `** Side effects:` |
|       - | 1705 | `**   The "next" pointers for elements in list are changed.` |
|       - | 1706 | `*/` |
|       - | 1707 | `#define N_SORT_BUCKET  32` |
|     528 | 1708 | `static sxi32 HashmapMergeSort(ph7_hashmap *pMap,ProcNodeCmp xCmp,void *pCmpData)` |
|       2 | 1709 |  |
|       - | 1710 | `	ph7_hashmap_node *a[N_SORT_BUCKET], *p,*pIn;` |
|       - | 1711 | `	sxu32 i;` |
|     530 | 1712 | `	SyZero(a,sizeof(a));` |
|       - | 1713 | `	/* Point to the first inserted entry */` |
|     530 | 1714 | `	pIn = pMap->pFirst;` |
|    8518 | 1715 | `	while( pIn ){` |
|    7990 | 1716 | `		p = pIn;` |
|    7990 | 1717 | `		pIn = p->pPrev;` |
|    7990 | 1718 | `		p->pPrev = 0;` |
|   15078 | 1719 | `		for(i=0; i<N_SORT_BUCKET-1; i++){` |
|   15078 | 1720 | `			if( a[i]==0 ){` |
|    7990 | 1721 | `				a[i] = p;` |
|    7990 | 1722 | `				break;` |
|     ! 0 | 1723 | `			}else{` |
|    7090 | 1724 | `				p = HashmapNodeMerge(a[i],p,xCmp,pCmpData);` |
|    7090 | 1725 | `				a[i] = 0;` |
|       - | 1726 | `			}` |
|    3546 | 1727 | `		}` |
|    7990 | 1728 | `		if( i==N_SORT_BUCKET-1 ){` |
|       - | 1729 | `			/* To get here, there need to be 2^(N_SORT_BUCKET) elements in he input list.` |
|       - | 1730 | `			 * But that is impossible.` |
|       - | 1731 | `			 */` |
|     ! 0 | 1732 | `			a[i] = HashmapNodeMerge(a[i], p,xCmp,pCmpData);` |
|     ! 0 | 1733 | `		}` |
|       2 | 1734 | `	}` |
|     530 | 1735 | `	p = a[0];` |
|   16898 | 1736 | `	for(i=1; i<N_SORT_BUCKET; i++){` |
|   16370 | 1737 | `		p = HashmapNodeMerge(p,a[i],xCmp,pCmpData);` |
|    8186 | 1738 | `	}` |
|     530 | 1739 | `	p->pNext = 0;` |
|       - | 1740 | `	/* Reflect the change */` |
|     530 | 1741 | `	pMap->pFirst = p;` |
|       - | 1742 | `	/* Reset the loop cursor */` |
|     530 | 1743 | `	pMap->pCur = pMap->pFirst;` |
|     530 | 1744 | `	return SXRET_OK;` |
|       2 | 1745 |  |
|       - | 1746 | `/*` |
|       - | 1747 | ` * Node comparison callback.` |
|       - | 1748 | ` * used-by: [sort(),asort(),...]` |
|       - | 1749 | ` */` |
|   36108 | 1750 | `static sxi32 HashmapCmpCallback1(ph7_hashmap_node *pA,ph7_hashmap_node *pB,void *pCmpData)` |
|       2 | 1751 |  |
|       - | 1752 | `	ph7_value sA,sB;` |
|       - | 1753 | `	sxi32 iFlags;` |
|       - | 1754 | `	int rc;` |
|   36110 | 1755 | `	if( pCmpData == 0 ){` |
|       - | 1756 | `		/* Perform a standard comparison */` |
|   36106 | 1757 | `		rc = HashmapNodeCmp(pA,pB,FALSE);` |
|   36106 | 1758 | `		return rc;` |
|       - | 1759 | `	}` |
|       5 | 1760 | `	iFlags = SX_PTR_TO_INT(pCmpData);` |
|       - | 1761 | `	/* Duplicate node values */` |
|       5 | 1762 | `	PH7_MemObjInit(pA->pMap->pVm,&sA);` |
|       5 | 1763 | `	PH7_MemObjInit(pA->pMap->pVm,&sB);` |
|       5 | 1764 | `	PH7_HashmapExtractNodeValue(pA,&sA,FALSE);` |
|       5 | 1765 | `	PH7_HashmapExtractNodeValue(pB,&sB,FALSE);` |
|       5 | 1766 | `	if( iFlags == 5 ){` |
|       - | 1767 | `		/* String cast */` |
|       5 | 1768 | `		if( (sA.iFlags & MEMOBJ_STRING) == 0 ){` |
|     ! 0 | 1769 | `			PH7_MemObjToString(&sA);` |
|     ! 0 | 1770 | `		}` |
|       5 | 1771 | `		if( (sB.iFlags & MEMOBJ_STRING) == 0 ){` |
|     ! 0 | 1772 | `			PH7_MemObjToString(&sB);` |
|     ! 0 | 1773 | `		}` |
|       3 | 1774 | `	}else{` |
|       - | 1775 | `		/* Numeric cast */` |
|     ! 0 | 1776 | `		PH7_MemObjToNumeric(&sA);` |
|     ! 0 | 1777 | `		PH7_MemObjToNumeric(&sB);` |
|       - | 1778 | `	}` |
|       - | 1779 | `	/* Perform the comparison */` |
|       5 | 1780 | `	rc = PH7_MemObjCmp(&sA,&sB,FALSE,0);` |
|       5 | 1781 | `	PH7_MemObjRelease(&sA);` |
|       5 | 1782 | `	PH7_MemObjRelease(&sB);` |
|       5 | 1783 | `	return rc;` |
|   18087 | 1784 |  |
|       - | 1785 | `/*` |
|       - | 1786 | ` * Node comparison callback: Compare nodes by keys only.` |
|       - | 1787 | ` * used-by: [ksort()]` |
|       - | 1788 | ` */` |
|      14 | 1789 | `static sxi32 HashmapCmpCallback2(ph7_hashmap_node *pA,ph7_hashmap_node *pB,void *pCmpData)` |
|       1 | 1790 |  |
|       - | 1791 | `	sxi32 rc;` |
|       7 | 1792 | `	SXUNUSED(pCmpData); /* cc warning */` |
|      15 | 1793 | `	if( pA->iType == HASHMAP_BLOB_NODE && pB->iType == HASHMAP_BLOB_NODE ){` |
|       - | 1794 | `		/* Perform a string comparison */` |
|       5 | 1795 | `		rc = SyBlobCmp(&pA->xKey.sKey,&pB->xKey.sKey);` |
|       3 | 1796 | `	}else{` |
|       - | 1797 | `		SyString sStr;` |
|       - | 1798 | `		sxi64 iA,iB;` |
|       - | 1799 | `		/* Perform a numeric comparison */` |
|      11 | 1800 | `		if( pA->iType == HASHMAP_BLOB_NODE ){` |
|       - | 1801 | `			/* Cast to 64-bit integer */` |
|     ! 0 | 1802 | `			SyStringInitFromBuf(&sStr,SyBlobData(&pA->xKey.sKey),SyBlobLength(&pA->xKey.sKey));` |
|     ! 0 | 1803 | `			if( sStr.nByte < 1 ){` |
|     ! 0 | 1804 | `				iA = 0;` |
|     ! 0 | 1805 | `			}else{` |
|     ! 0 | 1806 | `				SyStrToInt64(sStr.zString,sStr.nByte,(void *)&iA,0);` |
|       - | 1807 | `			}` |
|     ! 0 | 1808 | `		}else{` |
|      11 | 1809 | `			iA = pA->xKey.iKey;` |
|       - | 1810 | `		}` |
|      11 | 1811 | `		if( pB->iType == HASHMAP_BLOB_NODE ){` |
|       - | 1812 | `			/* Cast to 64-bit integer */` |
|     ! 0 | 1813 | `			SyStringInitFromBuf(&sStr,SyBlobData(&pB->xKey.sKey),SyBlobLength(&pB->xKey.sKey));` |
|     ! 0 | 1814 | `			if( sStr.nByte < 1 ){` |
|     ! 0 | 1815 | `				iB = 0;` |
|     ! 0 | 1816 | `			}else{` |
|     ! 0 | 1817 | `				SyStrToInt64(sStr.zString,sStr.nByte,(void *)&iB,0);` |
|       - | 1818 | `			}` |
|     ! 0 | 1819 | `		}else{` |
|      11 | 1820 | `			iB = pB->xKey.iKey;` |
|       - | 1821 | `		}` |
|      11 | 1822 | `		rc = (sxi32)(iA-iB);` |
|       - | 1823 | `	}` |
|       - | 1824 | `	/* Comparison result */` |
|      15 | 1825 | `	return rc;` |
|       1 | 1826 |  |
|       - | 1827 | `/*` |
|       - | 1828 | ` * Node comparison callback.` |
|       - | 1829 | ` * Used by: [rsort(),arsort()];` |
|       - | 1830 | ` */` |
|      12 | 1831 | `static sxi32 HashmapCmpCallback3(ph7_hashmap_node *pA,ph7_hashmap_node *pB,void *pCmpData)` |
|       1 | 1832 |  |
|       - | 1833 | `	ph7_value sA,sB;` |
|       - | 1834 | `	sxi32 iFlags;` |
|       - | 1835 | `	int rc;` |
|      13 | 1836 | `	if( pCmpData == 0 ){` |
|       - | 1837 | `		/* Perform a standard comparison */` |
|      13 | 1838 | `		rc = HashmapNodeCmp(pA,pB,FALSE);` |
|      13 | 1839 | `		return -rc;` |
|       - | 1840 | `	}` |
|     ! 0 | 1841 | `	iFlags = SX_PTR_TO_INT(pCmpData);` |
|       - | 1842 | `	/* Duplicate node values */` |
|     ! 0 | 1843 | `	PH7_MemObjInit(pA->pMap->pVm,&sA);` |
|     ! 0 | 1844 | `	PH7_MemObjInit(pA->pMap->pVm,&sB);` |
|     ! 0 | 1845 | `	PH7_HashmapExtractNodeValue(pA,&sA,FALSE);` |
|     ! 0 | 1846 | `	PH7_HashmapExtractNodeValue(pB,&sB,FALSE);` |
|     ! 0 | 1847 | `	if( iFlags == 5 ){` |
|       - | 1848 | `		/* String cast */` |
|     ! 0 | 1849 | `		if( (sA.iFlags & MEMOBJ_STRING) == 0 ){` |
|     ! 0 | 1850 | `			PH7_MemObjToString(&sA);` |
|     ! 0 | 1851 | `		}` |
|     ! 0 | 1852 | `		if( (sB.iFlags & MEMOBJ_STRING) == 0 ){` |
|     ! 0 | 1853 | `			PH7_MemObjToString(&sB);` |
|     ! 0 | 1854 | `		}` |
|     ! 0 | 1855 | `	}else{` |
|       - | 1856 | `		/* Numeric cast */` |
|     ! 0 | 1857 | `		PH7_MemObjToNumeric(&sA);` |
|     ! 0 | 1858 | `		PH7_MemObjToNumeric(&sB);` |
|       - | 1859 | `	}` |
|       - | 1860 | `	/* Perform the comparison */` |
|     ! 0 | 1861 | `	rc = PH7_MemObjCmp(&sA,&sB,FALSE,0);` |
|     ! 0 | 1862 | `	PH7_MemObjRelease(&sA);` |
|     ! 0 | 1863 | `	PH7_MemObjRelease(&sB);` |
|     ! 0 | 1864 | `	return -rc;` |
|       7 | 1865 |  |
|       - | 1866 | `/*` |
|       - | 1867 | ` * Node comparison callback: Invoke an user-defined callback for the purpose of node comparison.` |
|       - | 1868 | ` * used-by: [usort(),uasort()]` |
|       - | 1869 | ` */` |
|      12 | 1870 | `static sxi32 HashmapCmpCallback4(ph7_hashmap_node *pA,ph7_hashmap_node *pB,void *pCmpData)` |
|       1 | 1871 |  |
|       - | 1872 | `	ph7_value sResult,*pCallback;` |
|       - | 1873 | `	ph7_value *pV1,*pV2;` |
|       - | 1874 | `	ph7_value *apArg[2];  /* Callback arguments */` |
|       - | 1875 | `	sxi32 rc;` |
|       - | 1876 | `	/* Point to the desired callback */` |
|      13 | 1877 | `	pCallback = (ph7_value *)pCmpData;` |
|       - | 1878 | `	/* initialize the result value */` |
|      13 | 1879 | `	PH7_MemObjInit(pA->pMap->pVm,&sResult);` |
|       - | 1880 | `	/* Extract nodes values */` |
|      13 | 1881 | `	pV1 = HashmapExtractNodeValue(pA);` |
|      13 | 1882 | `	pV2 = HashmapExtractNodeValue(pB);` |
|      13 | 1883 | `	apArg[0] = pV1;` |
|      13 | 1884 | `	apArg[1] = pV2;` |
|       - | 1885 | `	/* Invoke the callback */` |
|      13 | 1886 | `	rc = PH7_VmCallUserFunction(pA->pMap->pVm,pCallback,2,apArg,&sResult);` |
|      13 | 1887 | `	if( rc != SXRET_OK ){` |
|       - | 1888 | `		/* An error occured while calling user defined function [i.e: not defined] */` |
|     ! 0 | 1889 | `		rc = -1; /* Set a dummy result */` |
|     ! 0 | 1890 | `	}else{` |
|       - | 1891 | `		/* Extract callback result */` |
|      13 | 1892 | `		if((sResult.iFlags & MEMOBJ_INT) == 0 ){` |
|       - | 1893 | `			/* Perform an int cast */` |
|     ! 0 | 1894 | `			PH7_MemObjToInteger(&sResult);` |
|     ! 0 | 1895 | `		}` |
|      13 | 1896 | `		rc = (sxi32)sResult.x.iVal;` |
|       - | 1897 | `	}` |
|      13 | 1898 | `	PH7_MemObjRelease(&sResult);` |
|       - | 1899 | `	/* Callback result */` |
|      13 | 1900 | `	return rc;` |
|       1 | 1901 |  |
|       - | 1902 | `/*` |
|       - | 1903 | ` * Node comparison callback: Compare nodes by keys only.` |
|       - | 1904 | ` * used-by: [krsort()]` |
|       - | 1905 | ` */` |
|       4 | 1906 | `static sxi32 HashmapCmpCallback5(ph7_hashmap_node *pA,ph7_hashmap_node *pB,void *pCmpData)` |
|       1 | 1907 |  |
|       - | 1908 | `	sxi32 rc;` |
|       2 | 1909 | `	SXUNUSED(pCmpData); /* cc warning */` |
|       5 | 1910 | `	if( pA->iType == HASHMAP_BLOB_NODE && pB->iType == HASHMAP_BLOB_NODE ){` |
|       - | 1911 | `		/* Perform a string comparison */` |
|       5 | 1912 | `		rc = SyBlobCmp(&pA->xKey.sKey,&pB->xKey.sKey);` |
|       3 | 1913 | `	}else{` |
|       - | 1914 | `		SyString sStr;` |
|       - | 1915 | `		sxi64 iA,iB;` |
|       - | 1916 | `		/* Perform a numeric comparison */` |
|     ! 0 | 1917 | `		if( pA->iType == HASHMAP_BLOB_NODE ){` |
|       - | 1918 | `			/* Cast to 64-bit integer */` |
|     ! 0 | 1919 | `			SyStringInitFromBuf(&sStr,SyBlobData(&pA->xKey.sKey),SyBlobLength(&pA->xKey.sKey));` |
|     ! 0 | 1920 | `			if( sStr.nByte < 1 ){` |
|     ! 0 | 1921 | `				iA = 0;` |
|     ! 0 | 1922 | `			}else{` |
|     ! 0 | 1923 | `				SyStrToInt64(sStr.zString,sStr.nByte,(void *)&iA,0);` |
|       - | 1924 | `			}` |
|     ! 0 | 1925 | `		}else{` |
|     ! 0 | 1926 | `			iA = pA->xKey.iKey;` |
|       - | 1927 | `		}` |
|     ! 0 | 1928 | `		if( pB->iType == HASHMAP_BLOB_NODE ){` |
|       - | 1929 | `			/* Cast to 64-bit integer */` |
|     ! 0 | 1930 | `			SyStringInitFromBuf(&sStr,SyBlobData(&pB->xKey.sKey),SyBlobLength(&pB->xKey.sKey));` |
|     ! 0 | 1931 | `			if( sStr.nByte < 1 ){` |
|     ! 0 | 1932 | `				iB = 0;` |
|     ! 0 | 1933 | `			}else{` |
|     ! 0 | 1934 | `				SyStrToInt64(sStr.zString,sStr.nByte,(void *)&iB,0);` |
|       - | 1935 | `			}` |
|     ! 0 | 1936 | `		}else{` |
|     ! 0 | 1937 | `			iB = pB->xKey.iKey;` |
|       - | 1938 | `		}` |
|     ! 0 | 1939 | `		rc = (sxi32)(iA-iB);` |
|       - | 1940 | `	}` |
|       5 | 1941 | `	return -rc; /* Reverse result */` |
|       1 | 1942 |  |
|       - | 1943 | `/*` |
|       - | 1944 | ` * Node comparison callback: Invoke an user-defined callback for the purpose of node comparison.` |
|       - | 1945 | ` * used-by: [uksort()]` |
|       - | 1946 | ` */` |
|       6 | 1947 | `static sxi32 HashmapCmpCallback6(ph7_hashmap_node *pA,ph7_hashmap_node *pB,void *pCmpData)` |
|       1 | 1948 |  |
|       - | 1949 | `	ph7_value sResult,*pCallback;` |
|       - | 1950 | `	ph7_value *apArg[2];  /* Callback arguments */` |
|       - | 1951 | `	ph7_value sK1,sK2;` |
|       - | 1952 | `	sxi32 rc;` |
|       - | 1953 | `	/* Point to the desired callback */` |
|       7 | 1954 | `	pCallback = (ph7_value *)pCmpData;` |
|       - | 1955 | `	/* initialize the result value */` |
|       7 | 1956 | `	PH7_MemObjInit(pA->pMap->pVm,&sResult);` |
|       7 | 1957 | `	PH7_MemObjInit(pA->pMap->pVm,&sK1);` |
|       7 | 1958 | `	PH7_MemObjInit(pA->pMap->pVm,&sK2);` |
|       - | 1959 | `	/* Extract nodes keys */` |
|       7 | 1960 | `	PH7_HashmapExtractNodeKey(pA,&sK1);` |
|       7 | 1961 | `	PH7_HashmapExtractNodeKey(pB,&sK2);` |
|       7 | 1962 | `	apArg[0] = &sK1;` |
|       7 | 1963 | `	apArg[1] = &sK2;` |
|       - | 1964 | `	/* Mark keys as constants */` |
|       7 | 1965 | `	sK1.nIdx = SXU32_HIGH;` |
|       7 | 1966 | `	sK2.nIdx = SXU32_HIGH;` |
|       - | 1967 | `	/* Invoke the callback */` |
|       7 | 1968 | `	rc = PH7_VmCallUserFunction(pA->pMap->pVm,pCallback,2,apArg,&sResult);` |
|       7 | 1969 | `	if( rc != SXRET_OK ){` |
|       - | 1970 | `		/* An error occured while calling user defined function [i.e: not defined] */` |
|     ! 0 | 1971 | `		rc = -1; /* Set a dummy result */` |
|     ! 0 | 1972 | `	}else{` |
|       - | 1973 | `		/* Extract callback result */` |
|       7 | 1974 | `		if((sResult.iFlags & MEMOBJ_INT) == 0 ){` |
|       - | 1975 | `			/* Perform an int cast */` |
|     ! 0 | 1976 | `			PH7_MemObjToInteger(&sResult);` |
|     ! 0 | 1977 | `		}` |
|       7 | 1978 | `		rc = (sxi32)sResult.x.iVal;` |
|       - | 1979 | `	}` |
|       7 | 1980 | `	PH7_MemObjRelease(&sResult);` |
|       7 | 1981 | `	PH7_MemObjRelease(&sK1);` |
|       7 | 1982 | `	PH7_MemObjRelease(&sK2);` |
|       - | 1983 | `	/* Callback result */` |
|       7 | 1984 | `	return rc;` |
|       1 | 1985 |  |
|       - | 1986 | `/*` |
|       - | 1987 | ` * Node comparison callback: Random node comparison.` |
|       - | 1988 | ` * used-by: [shuffle()]` |
|       - | 1989 | ` */` |
|      16 | 1990 | `static sxi32 HashmapCmpCallback7(ph7_hashmap_node *pA,ph7_hashmap_node *pB,void *pCmpData)` |
|       1 | 1991 |  |
|       - | 1992 | `	sxu32 n;` |
|       6 | 1993 | `	SXUNUSED(pB); /* cc warning */` |
|       6 | 1994 | `	SXUNUSED(pCmpData);` |
|       - | 1995 | `	/* Grab a random number */` |
|      17 | 1996 | `	n = PH7_VmRandomNum(pA->pMap->pVm);` |
|       - | 1997 | `	/* if the random number is odd then the first node 'pA' is greater then` |
|       - | 1998 | `	 * the second node 'pB'. Otherwise the reverse is assumed.` |
|       - | 1999 | `	 */` |
|      17 | 2000 | `	return n&1 ? 1 : -1;` |
|       1 | 2001 |  |
|       - | 2002 | `/*` |
|       - | 2003 | ` * Rehash all nodes keys after a merge-sort have been applied.` |
|       - | 2004 | ` * Used by [sort(),usort() and rsort()].` |
|       - | 2005 | ` */` |
|     512 | 2006 | `static void HashmapSortRehash(ph7_hashmap *pMap)` |
|       2 | 2007 |  |
|       - | 2008 | `	ph7_hashmap_node *p,*pLast;` |
|       - | 2009 | `	sxu32 i;` |
|       - | 2010 | `	/* Rehash all entries */` |
|     514 | 2011 | `	pLast = p = pMap->pFirst;` |
|     514 | 2012 | `	pMap->iNextIdx = 0; /* Reset the automatic index */` |
|     514 | 2013 | `	i = 0;` |
|    4223 | 2014 | `	for( ;; ){` |
|    8448 | 2015 | `		if( i >= pMap->nEntry ){` |
|     514 | 2016 | `			pMap->pLast = pLast; /* Fix the last link broken by the merge-sort */` |
|     514 | 2017 | `			break;` |
|       - | 2018 | `		}` |
|    7936 | 2019 | `		if( p->iType == HASHMAP_BLOB_NODE ){` |
|       - | 2020 | `			/* Do not maintain index association as requested by the PHP specification */` |
|       5 | 2021 | `			SyBlobRelease(&p->xKey.sKey);` |
|       - | 2022 | `			/* Change key type */` |
|       5 | 2023 | `			p->iType = HASHMAP_INT_NODE;` |
|       2 | 2024 | `		}` |
|    7936 | 2025 | `		HashmapRehashIntNode(p);` |
|       - | 2026 | `		/* Point to the next entry */` |
|    7936 | 2027 | `		i++;` |
|    7936 | 2028 | `		pLast = p;` |
|    7936 | 2029 | `		p = p->pPrev; /* Reverse link */` |
|       2 | 2030 | `	}` |
|     514 | 2031 |  |
|       - | 2032 | `/*` |
|       - | 2033 | ` * Array functions implementation.` |
|       - | 2034 | ` * Status:` |
|       - | 2035 | ` *  Stable.` |
|       - | 2036 | ` */` |
|       - | 2037 | `/*` |
|       - | 2038 | ` * bool sort(array &$array[,int $sort_flags = SORT_REGULAR ] )` |
|       - | 2039 | ` * Sort an array.` |
|       - | 2040 | ` * Parameters` |
|       - | 2041 | ` *  $array` |
|       - | 2042 | ` *   The input array.` |
|       - | 2043 | ` * $sort_flags` |
|       - | 2044 | ` *  The optional second parameter sort_flags may be used to modify the sorting behavior using these values:` |
|       - | 2045 | ` *  Sorting type flags:` |
|       - | 2046 | ` *   SORT_REGULAR - compare items normally (don't change types)` |
|       - | 2047 | ` *   SORT_NUMERIC - compare items numerically` |
|       - | 2048 | ` *   SORT_STRING - compare items as strings` |
|       - | 2049 | ` * Return` |
|       - | 2050 | ` *  TRUE on success or FALSE on failure.` |
|       - | 2051 | ` *` |
|       - | 2052 | ` */` |
|     828 | 2053 | `static int ph7_hashmap_sort(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       2 | 2054 |  |
|       - | 2055 | `	ph7_hashmap *pMap;` |
|       - | 2056 | `	/* Make sure we are dealing with a valid hashmap */` |
|     830 | 2057 | `	if( nArg < 1 \|\| !ph7_value_is_array(apArg[0]) ){` |
|       - | 2058 | `		/* Missing/Invalid arguments,return FALSE */` |
|     ! 0 | 2059 | `		ph7_result_bool(pCtx,0);` |
|     ! 0 | 2060 | `		return PH7_OK;` |
|       - | 2061 | `	}` |
|       - | 2062 | `	/* Point to the internal representation of the input hashmap */` |
|     830 | 2063 | `	pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|     830 | 2064 | `	if( pMap->nEntry > 1 ){` |
|     508 | 2065 | `		sxi32 iCmpFlags = 0;` |
|     508 | 2066 | `		if( nArg > 1 ){` |
|       - | 2067 | `			/* Extract comparison flags */` |
|       3 | 2068 | `			iCmpFlags = ph7_value_to_int(apArg[1]);` |
|       3 | 2069 | `			if( iCmpFlags == 3 /* SORT_REGULAR */ ){` |
|     ! 0 | 2070 | `				iCmpFlags = 0; /* Standard comparison */` |
|     ! 0 | 2071 | `			}` |
|       1 | 2072 | `		}` |
|       - | 2073 | `		/* Do the merge sort */` |
|     508 | 2074 | `		HashmapMergeSort(pMap,HashmapCmpCallback1,SX_INT_TO_PTR(iCmpFlags));` |
|       - | 2075 | `		/* Rehash [Do not maintain index association as requested by the PHP specification] */` |
|     508 | 2076 | `		HashmapSortRehash(pMap);` |
|     253 | 2077 | `	}` |
|       - | 2078 | `	/* All done,return TRUE */` |
|     830 | 2079 | `	ph7_result_bool(pCtx,1);` |
|     830 | 2080 | `	return PH7_OK;` |
|     416 | 2081 |  |
|       - | 2082 | `/*` |
|       - | 2083 | ` * bool asort(array &$array[,int $sort_flags = SORT_REGULAR ] )` |
|       - | 2084 | ` *  Sort an array and maintain index association.` |
|       - | 2085 | ` * Parameters` |
|       - | 2086 | ` *  $array` |
|       - | 2087 | ` *   The input array.` |
|       - | 2088 | ` * $sort_flags` |
|       - | 2089 | ` *  The optional second parameter sort_flags may be used to modify the sorting behavior using these values:` |
|       - | 2090 | ` *  Sorting type flags:` |
|       - | 2091 | ` *   SORT_REGULAR - compare items normally (don't change types)` |
|       - | 2092 | ` *   SORT_NUMERIC - compare items numerically` |
|       - | 2093 | ` *   SORT_STRING - compare items as strings` |
|       - | 2094 | ` * Return` |
|       - | 2095 | ` *  TRUE on success or FALSE on failure.` |
|       - | 2096 | ` */` |
|       2 | 2097 | `static int ph7_hashmap_asort(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       1 | 2098 |  |
|       - | 2099 | `	ph7_hashmap *pMap;` |
|       - | 2100 | `	/* Make sure we are dealing with a valid hashmap */` |
|       3 | 2101 | `	if( nArg < 1 \|\| !ph7_value_is_array(apArg[0]) ){` |
|       - | 2102 | `		/* Missing/Invalid arguments,return FALSE */` |
|     ! 0 | 2103 | `		ph7_result_bool(pCtx,0);` |
|     ! 0 | 2104 | `		return PH7_OK;` |
|       - | 2105 | `	}` |
|       - | 2106 | `	/* Point to the internal representation of the input hashmap */` |
|       3 | 2107 | `	pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|       3 | 2108 | `	if( pMap->nEntry > 1 ){` |
|       3 | 2109 | `		sxi32 iCmpFlags = 0;` |
|       3 | 2110 | `		if( nArg > 1 ){` |
|       - | 2111 | `			/* Extract comparison flags */` |
|     ! 0 | 2112 | `			iCmpFlags = ph7_value_to_int(apArg[1]);` |
|     ! 0 | 2113 | `			if( iCmpFlags == 3 /* SORT_REGULAR */ ){` |
|     ! 0 | 2114 | `				iCmpFlags = 0; /* Standard comparison */` |
|     ! 0 | 2115 | `			}` |
|     ! 0 | 2116 | `		}` |
|       - | 2117 | `		/* Do the merge sort */` |
|       3 | 2118 | `		HashmapMergeSort(pMap,HashmapCmpCallback1,SX_INT_TO_PTR(iCmpFlags));` |
|       - | 2119 | `		/* Fix the last link broken by the merge */` |
|       5 | 2120 | `		while(pMap->pLast->pPrev){` |
|       3 | 2121 | `			pMap->pLast = pMap->pLast->pPrev;` |
|       1 | 2122 | `		}` |
|       1 | 2123 | `	}` |
|       - | 2124 | `	/* All done,return TRUE */` |
|       3 | 2125 | `	ph7_result_bool(pCtx,1);` |
|       3 | 2126 | `	return PH7_OK;` |
|       2 | 2127 |  |
|       - | 2128 | `/*` |
|       - | 2129 | ` * bool arsort(array &$array[,int $sort_flags = SORT_REGULAR ] )` |
|       - | 2130 | ` *  Sort an array in reverse order and maintain index association.` |
|       - | 2131 | ` * Parameters` |
|       - | 2132 | ` *  $array` |
|       - | 2133 | ` *   The input array.` |
|       - | 2134 | ` * $sort_flags` |
|       - | 2135 | ` *  The optional second parameter sort_flags may be used to modify the sorting behavior using these values:` |
|       - | 2136 | ` *  Sorting type flags:` |
|       - | 2137 | ` *   SORT_REGULAR - compare items normally (don't change types)` |
|       - | 2138 | ` *   SORT_NUMERIC - compare items numerically` |
|       - | 2139 | ` *   SORT_STRING - compare items as strings` |
|       - | 2140 | ` * Return` |
|       - | 2141 | ` *  TRUE on success or FALSE on failure.` |
|       - | 2142 | ` */` |
|       2 | 2143 | `static int ph7_hashmap_arsort(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       1 | 2144 |  |
|       - | 2145 | `	ph7_hashmap *pMap;` |
|       - | 2146 | `	/* Make sure we are dealing with a valid hashmap */` |
|       3 | 2147 | `	if( nArg < 1 \|\| !ph7_value_is_array(apArg[0]) ){` |
|       - | 2148 | `		/* Missing/Invalid arguments,return FALSE */` |
|     ! 0 | 2149 | `		ph7_result_bool(pCtx,0);` |
|     ! 0 | 2150 | `		return PH7_OK;` |
|       - | 2151 | `	}` |
|       - | 2152 | `	/* Point to the internal representation of the input hashmap */` |
|       3 | 2153 | `	pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|       3 | 2154 | `	if( pMap->nEntry > 1 ){` |
|       3 | 2155 | `		sxi32 iCmpFlags = 0;` |
|       3 | 2156 | `		if( nArg > 1 ){` |
|       - | 2157 | `			/* Extract comparison flags */` |
|     ! 0 | 2158 | `			iCmpFlags = ph7_value_to_int(apArg[1]);` |
|     ! 0 | 2159 | `			if( iCmpFlags == 3 /* SORT_REGULAR */ ){` |
|     ! 0 | 2160 | `				iCmpFlags = 0; /* Standard comparison */` |
|     ! 0 | 2161 | `			}` |
|     ! 0 | 2162 | `		}` |
|       - | 2163 | `		/* Do the merge sort */` |
|       3 | 2164 | `		HashmapMergeSort(pMap,HashmapCmpCallback3,SX_INT_TO_PTR(iCmpFlags));` |
|       - | 2165 | `		/* Fix the last link broken by the merge */` |
|       5 | 2166 | `		while(pMap->pLast->pPrev){` |
|       3 | 2167 | `			pMap->pLast = pMap->pLast->pPrev;` |
|       1 | 2168 | `		}` |
|       1 | 2169 | `	}` |
|       - | 2170 | `	/* All done,return TRUE */` |
|       3 | 2171 | `	ph7_result_bool(pCtx,1);` |
|       3 | 2172 | `	return PH7_OK;` |
|       2 | 2173 |  |
|       - | 2174 | `/*` |
|       - | 2175 | ` * bool ksort(array &$array[,int $sort_flags = SORT_REGULAR ] )` |
|       - | 2176 | ` *  Sort an array by key.` |
|       - | 2177 | ` * Parameters` |
|       - | 2178 | ` *  $array` |
|       - | 2179 | ` *   The input array.` |
|       - | 2180 | ` * $sort_flags` |
|       - | 2181 | ` *  The optional second parameter sort_flags may be used to modify the sorting behavior using these values:` |
|       - | 2182 | ` *  Sorting type flags:` |
|       - | 2183 | ` *   SORT_REGULAR - compare items normally (don't change types)` |
|       - | 2184 | ` *   SORT_NUMERIC - compare items numerically` |
|       - | 2185 | ` *   SORT_STRING - compare items as strings` |
|       - | 2186 | ` * Return` |
|       - | 2187 | ` *  TRUE on success or FALSE on failure.` |
|       - | 2188 | ` */` |
|       4 | 2189 | `static int ph7_hashmap_ksort(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       1 | 2190 |  |
|       - | 2191 | `	ph7_hashmap *pMap;` |
|       - | 2192 | `	/* Make sure we are dealing with a valid hashmap */` |
|       5 | 2193 | `	if( nArg < 1 \|\| !ph7_value_is_array(apArg[0]) ){` |
|       - | 2194 | `		/* Missing/Invalid arguments,return FALSE */` |
|     ! 0 | 2195 | `		ph7_result_bool(pCtx,0);` |
|     ! 0 | 2196 | `		return PH7_OK;` |
|       - | 2197 | `	}` |
|       - | 2198 | `	/* Point to the internal representation of the input hashmap */` |
|       5 | 2199 | `	pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|       5 | 2200 | `	if( pMap->nEntry > 1 ){` |
|       5 | 2201 | `		sxi32 iCmpFlags = 0;` |
|       5 | 2202 | `		if( nArg > 1 ){` |
|       - | 2203 | `			/* Extract comparison flags */` |
|     ! 0 | 2204 | `			iCmpFlags = ph7_value_to_int(apArg[1]);` |
|     ! 0 | 2205 | `			if( iCmpFlags == 3 /* SORT_REGULAR */ ){` |
|     ! 0 | 2206 | `				iCmpFlags = 0; /* Standard comparison */` |
|     ! 0 | 2207 | `			}` |
|     ! 0 | 2208 | `		}` |
|       - | 2209 | `		/* Do the merge sort */` |
|       5 | 2210 | `		HashmapMergeSort(pMap,HashmapCmpCallback2,SX_INT_TO_PTR(iCmpFlags));` |
|       - | 2211 | `		/* Fix the last link broken by the merge */` |
|      15 | 2212 | `		while(pMap->pLast->pPrev){` |
|      11 | 2213 | `			pMap->pLast = pMap->pLast->pPrev;` |
|       1 | 2214 | `		}` |
|       2 | 2215 | `	}` |
|       - | 2216 | `	/* All done,return TRUE */` |
|       5 | 2217 | `	ph7_result_bool(pCtx,1);` |
|       5 | 2218 | `	return PH7_OK;` |
|       3 | 2219 |  |
|       - | 2220 | `/*` |
|       - | 2221 | ` * bool krsort(array &$array[,int $sort_flags = SORT_REGULAR ] )` |
|       - | 2222 | ` *  Sort an array by key in reverse order.` |
|       - | 2223 | ` * Parameters` |
|       - | 2224 | ` *  $array` |
|       - | 2225 | ` *   The input array.` |
|       - | 2226 | ` * $sort_flags` |
|       - | 2227 | ` *  The optional second parameter sort_flags may be used to modify the sorting behavior using these values:` |
|       - | 2228 | ` *  Sorting type flags:` |
|       - | 2229 | ` *   SORT_REGULAR - compare items normally (don't change types)` |
|       - | 2230 | ` *   SORT_NUMERIC - compare items numerically` |
|       - | 2231 | ` *   SORT_STRING - compare items as strings` |
|       - | 2232 | ` * Return` |
|       - | 2233 | ` *  TRUE on success or FALSE on failure.` |
|       - | 2234 | ` */` |
|       2 | 2235 | `static int ph7_hashmap_krsort(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       1 | 2236 |  |
|       - | 2237 | `	ph7_hashmap *pMap;` |
|       - | 2238 | `	/* Make sure we are dealing with a valid hashmap */` |
|       3 | 2239 | `	if( nArg < 1 \|\| !ph7_value_is_array(apArg[0]) ){` |
|       - | 2240 | `		/* Missing/Invalid arguments,return FALSE */` |
|     ! 0 | 2241 | `		ph7_result_bool(pCtx,0);` |
|     ! 0 | 2242 | `		return PH7_OK;` |
|       - | 2243 | `	}` |
|       - | 2244 | `	/* Point to the internal representation of the input hashmap */` |
|       3 | 2245 | `	pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|       3 | 2246 | `	if( pMap->nEntry > 1 ){` |
|       3 | 2247 | `		sxi32 iCmpFlags = 0;` |
|       3 | 2248 | `		if( nArg > 1 ){` |
|       - | 2249 | `			/* Extract comparison flags */` |
|     ! 0 | 2250 | `			iCmpFlags = ph7_value_to_int(apArg[1]);` |
|     ! 0 | 2251 | `			if( iCmpFlags == 3 /* SORT_REGULAR */ ){` |
|     ! 0 | 2252 | `				iCmpFlags = 0; /* Standard comparison */` |
|     ! 0 | 2253 | `			}` |
|     ! 0 | 2254 | `		}` |
|       - | 2255 | `		/* Do the merge sort */` |
|       3 | 2256 | `		HashmapMergeSort(pMap,HashmapCmpCallback5,SX_INT_TO_PTR(iCmpFlags));` |
|       - | 2257 | `		/* Fix the last link broken by the merge */` |
|       7 | 2258 | `		while(pMap->pLast->pPrev){` |
|       5 | 2259 | `			pMap->pLast = pMap->pLast->pPrev;` |
|       1 | 2260 | `		}` |
|       1 | 2261 | `	}` |
|       - | 2262 | `	/* All done,return TRUE */` |
|       3 | 2263 | `	ph7_result_bool(pCtx,1);` |
|       3 | 2264 | `	return PH7_OK;` |
|       2 | 2265 |  |
|       - | 2266 | `/*` |
|       - | 2267 | ` * bool rsort(array &$array[,int $sort_flags = SORT_REGULAR ] )` |
|       - | 2268 | ` * Sort an array in reverse order.` |
|       - | 2269 | ` * Parameters` |
|       - | 2270 | ` *  $array` |
|       - | 2271 | ` *   The input array.` |
|       - | 2272 | ` * $sort_flags` |
|       - | 2273 | ` *  The optional second parameter sort_flags may be used to modify the sorting behavior using these values:` |
|       - | 2274 | ` *  Sorting type flags:` |
|       - | 2275 | ` *   SORT_REGULAR - compare items normally (don't change types)` |
|       - | 2276 | ` *   SORT_NUMERIC - compare items numerically` |
|       - | 2277 | ` *   SORT_STRING - compare items as strings` |
|       - | 2278 | ` * Return` |
|       - | 2279 | ` *  TRUE on success or FALSE on failure.` |
|       - | 2280 | ` */` |
|       2 | 2281 | `static int ph7_hashmap_rsort(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       1 | 2282 |  |
|       - | 2283 | `	ph7_hashmap *pMap;` |
|       - | 2284 | `	/* Make sure we are dealing with a valid hashmap */` |
|       3 | 2285 | `	if( nArg < 1 \|\| !ph7_value_is_array(apArg[0]) ){` |
|       - | 2286 | `		/* Missing/Invalid arguments,return FALSE */` |
|     ! 0 | 2287 | `		ph7_result_bool(pCtx,0);` |
|     ! 0 | 2288 | `		return PH7_OK;` |
|       - | 2289 | `	}` |
|       - | 2290 | `	/* Point to the internal representation of the input hashmap */` |
|       3 | 2291 | `	pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|       3 | 2292 | `	if( pMap->nEntry > 1 ){` |
|       3 | 2293 | `		sxi32 iCmpFlags = 0;` |
|       3 | 2294 | `		if( nArg > 1 ){` |
|       - | 2295 | `			/* Extract comparison flags */` |
|     ! 0 | 2296 | `			iCmpFlags = ph7_value_to_int(apArg[1]);` |
|     ! 0 | 2297 | `			if( iCmpFlags == 3 /* SORT_REGULAR */ ){` |
|     ! 0 | 2298 | `				iCmpFlags = 0; /* Standard comparison */` |
|     ! 0 | 2299 | `			}` |
|     ! 0 | 2300 | `		}` |
|       - | 2301 | `		/* Do the merge sort */` |
|       3 | 2302 | `		HashmapMergeSort(pMap,HashmapCmpCallback3,SX_INT_TO_PTR(iCmpFlags));` |
|       - | 2303 | `		/* Rehash [Do not maintain index association as requested by the PHP specification] */` |
|       3 | 2304 | `		HashmapSortRehash(pMap);` |
|       1 | 2305 | `	}` |
|       - | 2306 | `	/* All done,return TRUE */` |
|       3 | 2307 | `	ph7_result_bool(pCtx,1);` |
|       3 | 2308 | `	return PH7_OK;` |
|       2 | 2309 |  |
|       - | 2310 | `/*` |
|       - | 2311 | ` * bool usort(array &$array,callable $cmp_function)` |
|       - | 2312 | ` *  Sort an array by values using a user-defined comparison function.` |
|       - | 2313 | ` * Parameters` |
|       - | 2314 | ` *  $array` |
|       - | 2315 | ` *   The input array.` |
|       - | 2316 | ` * $cmp_function` |
|       - | 2317 | ` *  The comparison function must return an integer less than, equal to, or greater` |
|       - | 2318 | ` *  than zero if the first argument is considered to be respectively less than, equal` |
|       - | 2319 | ` *  to, or greater than the second.` |
|       - | 2320 | ` *    int callback ( mixed $a, mixed $b )` |
|       - | 2321 | ` * Return` |
|       - | 2322 | ` *  TRUE on success or FALSE on failure.` |
|       - | 2323 | ` */` |
|       2 | 2324 | `static int ph7_hashmap_usort(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       1 | 2325 |  |
|       - | 2326 | `	ph7_hashmap *pMap;` |
|       - | 2327 | `	/* Make sure we are dealing with a valid hashmap */` |
|       3 | 2328 | `	if( nArg < 1 \|\| !ph7_value_is_array(apArg[0]) ){` |
|       - | 2329 | `		/* Missing/Invalid arguments,return FALSE */` |
|     ! 0 | 2330 | `		ph7_result_bool(pCtx,0);` |
|     ! 0 | 2331 | `		return PH7_OK;` |
|       - | 2332 | `	}` |
|       - | 2333 | `	/* Point to the internal representation of the input hashmap */` |
|       3 | 2334 | `	pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|       3 | 2335 | `	if( pMap->nEntry > 1 ){` |
|       3 | 2336 | `		ph7_value *pCallback = 0;` |
|       - | 2337 | `		ProcNodeCmp xCmp;` |
|       3 | 2338 | `		xCmp = HashmapCmpCallback4; /* User-defined function as the comparison callback */` |
|       3 | 2339 | `		if( nArg > 1 && ph7_value_is_callable(apArg[1]) ){` |
|       - | 2340 | `			/* Point to the desired callback */` |
|       3 | 2341 | `			pCallback = apArg[1];` |
|       2 | 2342 | `		}else{` |
|       - | 2343 | `			/* Use the default comparison function */` |
|     ! 0 | 2344 | `			xCmp = HashmapCmpCallback1;` |
|       - | 2345 | `		}` |
|       - | 2346 | `		/* Do the merge sort */` |
|       3 | 2347 | `		HashmapMergeSort(pMap,xCmp,pCallback);` |
|       - | 2348 | `		/* Rehash [Do not maintain index association as requested by the PHP specification] */` |
|       3 | 2349 | `		HashmapSortRehash(pMap);` |
|       1 | 2350 | `	}` |
|       - | 2351 | `	/* All done,return TRUE */` |
|       3 | 2352 | `	ph7_result_bool(pCtx,1);` |
|       3 | 2353 | `	return PH7_OK;` |
|       2 | 2354 |  |
|       - | 2355 | `/*` |
|       - | 2356 | ` * bool uasort(array &$array,callable $cmp_function)` |
|       - | 2357 | ` *  Sort an array by values using a user-defined comparison function` |
|       - | 2358 | ` *  and maintain index association.` |
|       - | 2359 | ` * Parameters` |
|       - | 2360 | ` *  $array` |
|       - | 2361 | ` *   The input array.` |
|       - | 2362 | ` * $cmp_function` |
|       - | 2363 | ` *  The comparison function must return an integer less than, equal to, or greater` |
|       - | 2364 | ` *  than zero if the first argument is considered to be respectively less than, equal` |
|       - | 2365 | ` *  to, or greater than the second.` |
|       - | 2366 | ` *    int callback ( mixed $a, mixed $b )` |
|       - | 2367 | ` * Return` |
|       - | 2368 | ` *  TRUE on success or FALSE on failure.` |
|       - | 2369 | ` */` |
|       2 | 2370 | `static int ph7_hashmap_uasort(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       1 | 2371 |  |
|       - | 2372 | `	ph7_hashmap *pMap;` |
|       - | 2373 | `	/* Make sure we are dealing with a valid hashmap */` |
|       3 | 2374 | `	if( nArg < 1 \|\| !ph7_value_is_array(apArg[0]) ){` |
|       - | 2375 | `		/* Missing/Invalid arguments,return FALSE */` |
|     ! 0 | 2376 | `		ph7_result_bool(pCtx,0);` |
|     ! 0 | 2377 | `		return PH7_OK;` |
|       - | 2378 | `	}` |
|       - | 2379 | `	/* Point to the internal representation of the input hashmap */` |
|       3 | 2380 | `	pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|       3 | 2381 | `	if( pMap->nEntry > 1 ){` |
|       3 | 2382 | `		ph7_value *pCallback = 0;` |
|       - | 2383 | `		ProcNodeCmp xCmp;` |
|       3 | 2384 | `		xCmp = HashmapCmpCallback4; /* User-defined function as the comparison callback */` |
|       3 | 2385 | `		if( nArg > 1 && ph7_value_is_callable(apArg[1]) ){` |
|       - | 2386 | `			/* Point to the desired callback */` |
|       3 | 2387 | `			pCallback = apArg[1];` |
|       2 | 2388 | `		}else{` |
|       - | 2389 | `			/* Use the default comparison function */` |
|     ! 0 | 2390 | `			xCmp = HashmapCmpCallback1;` |
|       - | 2391 | `		}` |
|       - | 2392 | `		/* Do the merge sort */` |
|       3 | 2393 | `		HashmapMergeSort(pMap,xCmp,pCallback);` |
|       - | 2394 | `		/* Fix the last link broken by the merge */` |
|       5 | 2395 | `		while(pMap->pLast->pPrev){` |
|       3 | 2396 | `			pMap->pLast = pMap->pLast->pPrev;` |
|       1 | 2397 | `		}` |
|       1 | 2398 | `	}` |
|       - | 2399 | `	/* All done,return TRUE */` |
|       3 | 2400 | `	ph7_result_bool(pCtx,1);` |
|       3 | 2401 | `	return PH7_OK;` |
|       2 | 2402 |  |
|       - | 2403 | `/*` |
|       - | 2404 | ` * bool uksort(array &$array,callable $cmp_function)` |
|       - | 2405 | ` *  Sort an array by keys using a user-defined comparison` |
|       - | 2406 | ` *  function and maintain index association.` |
|       - | 2407 | ` * Parameters` |
|       - | 2408 | ` *  $array` |
|       - | 2409 | ` *   The input array.` |
|       - | 2410 | ` * $cmp_function` |
|       - | 2411 | ` *  The comparison function must return an integer less than, equal to, or greater` |
|       - | 2412 | ` *  than zero if the first argument is considered to be respectively less than, equal` |
|       - | 2413 | ` *  to, or greater than the second.` |
|       - | 2414 | ` *    int callback ( mixed $a, mixed $b )` |
|       - | 2415 | ` * Return` |
|       - | 2416 | ` *  TRUE on success or FALSE on failure.` |
|       - | 2417 | ` */` |
|       2 | 2418 | `static int ph7_hashmap_uksort(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       1 | 2419 |  |
|       - | 2420 | `	ph7_hashmap *pMap;` |
|       - | 2421 | `	/* Make sure we are dealing with a valid hashmap */` |
|       3 | 2422 | `	if( nArg < 1 \|\| !ph7_value_is_array(apArg[0]) ){` |
|       - | 2423 | `		/* Missing/Invalid arguments,return FALSE */` |
|     ! 0 | 2424 | `		ph7_result_bool(pCtx,0);` |
|     ! 0 | 2425 | `		return PH7_OK;` |
|       - | 2426 | `	}` |
|       - | 2427 | `	/* Point to the internal representation of the input hashmap */` |
|       3 | 2428 | `	pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|       3 | 2429 | `	if( pMap->nEntry > 1 ){` |
|       3 | 2430 | `		ph7_value *pCallback = 0;` |
|       - | 2431 | `		ProcNodeCmp xCmp;` |
|       3 | 2432 | `		xCmp = HashmapCmpCallback6; /* User-defined function as the comparison callback */` |
|       3 | 2433 | `		if( nArg > 1 && ph7_value_is_callable(apArg[1]) ){` |
|       - | 2434 | `			/* Point to the desired callback */` |
|       3 | 2435 | `			pCallback = apArg[1];` |
|       2 | 2436 | `		}else{` |
|       - | 2437 | `			/* Use the default comparison function */` |
|     ! 0 | 2438 | `			xCmp = HashmapCmpCallback2;` |
|       - | 2439 | `		}` |
|       - | 2440 | `		/* Do the merge sort */` |
|       3 | 2441 | `		HashmapMergeSort(pMap,xCmp,pCallback);` |
|       - | 2442 | `		/* Fix the last link broken by the merge */` |
|       3 | 2443 | `		while(pMap->pLast->pPrev){` |
|     ! 0 | 2444 | `			pMap->pLast = pMap->pLast->pPrev;` |
|     ! 0 | 2445 | `		}` |
|       1 | 2446 | `	}` |
|       - | 2447 | `	/* All done,return TRUE */` |
|       3 | 2448 | `	ph7_result_bool(pCtx,1);` |
|       3 | 2449 | `	return PH7_OK;` |
|       2 | 2450 |  |
|       - | 2451 | `/*` |
|       - | 2452 | ` * bool shuffle(array &$array)` |
|       - | 2453 | ` *  shuffles (randomizes the order of the elements in) an array.` |
|       - | 2454 | ` * Parameters` |
|       - | 2455 | ` *  $array` |
|       - | 2456 | ` *   The input array.` |
|       - | 2457 | ` * Return` |
|       - | 2458 | ` *  TRUE on success or FALSE on failure.` |
|       - | 2459 | ` *` |
|       - | 2460 | ` */` |
|       2 | 2461 | `static int ph7_hashmap_shuffle(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       1 | 2462 |  |
|       - | 2463 | `	ph7_hashmap *pMap;` |
|       - | 2464 | `	/* Make sure we are dealing with a valid hashmap */` |
|       3 | 2465 | `	if( nArg < 1 \|\| !ph7_value_is_array(apArg[0]) ){` |
|       - | 2466 | `		/* Missing/Invalid arguments,return FALSE */` |
|     ! 0 | 2467 | `		ph7_result_bool(pCtx,0);` |
|     ! 0 | 2468 | `		return PH7_OK;` |
|       - | 2469 | `	}` |
|       - | 2470 | `	/* Point to the internal representation of the input hashmap */` |
|       3 | 2471 | `	pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|       3 | 2472 | `	if( pMap->nEntry > 1 ){` |
|       - | 2473 | `		/* Do the merge sort */` |
|       3 | 2474 | `		HashmapMergeSort(pMap,HashmapCmpCallback7,0);` |
|       - | 2475 | `		/* Fix the last link broken by the merge */` |
|       7 | 2476 | `		while(pMap->pLast->pPrev){` |
|       5 | 2477 | `			pMap->pLast = pMap->pLast->pPrev;` |
|       1 | 2478 | `		}` |
|       1 | 2479 | `	}` |
|       - | 2480 | `	/* All done,return TRUE */` |
|       3 | 2481 | `	ph7_result_bool(pCtx,1);` |
|       3 | 2482 | `	return PH7_OK;` |
|       2 | 2483 |  |
|       - | 2484 | `/*` |
|       - | 2485 | ` * int count(array $var [, int $mode = COUNT_NORMAL ])` |
|       - | 2486 | ` *   Count all elements in an array, or something in an object.` |
|       - | 2487 | ` * Parameters` |
|       - | 2488 | ` *  $var` |
|       - | 2489 | ` *   The array or the object.` |
|       - | 2490 | ` * $mode` |
|       - | 2491 | ` *  If the optional mode parameter is set to COUNT_RECURSIVE (or 1), count()` |
|       - | 2492 | ` *  will recursively count the array. This is particularly useful for counting` |
|       - | 2493 | ` *  all the elements of a multidimensional array. count() does not detect infinite` |
|       - | 2494 | ` *  recursion.` |
|       - | 2495 | ` * Return` |
|       - | 2496 | ` *  Returns the number of elements in the array.` |
|       - | 2497 | ` */` |
|     572 | 2498 | `static int ph7_hashmap_count(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       2 | 2499 |  |
|     574 | 2500 | `	int bRecursive = FALSE;` |
|       - | 2501 | `	sxi64 iCount;` |
|     574 | 2502 | `	if( nArg < 1 ){` |
|       - | 2503 | `		/* Missing arguments,return 0 */` |
|     ! 0 | 2504 | `		ph7_result_int(pCtx,0);` |
|     ! 0 | 2505 | `		return PH7_OK;` |
|       - | 2506 | `	}` |
|     574 | 2507 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|       - | 2508 | `		/* TICKET 1433-19: Handle objects */` |
|       3 | 2509 | `		int res = !ph7_value_is_null(apArg[0]);` |
|       3 | 2510 | `		ph7_result_int(pCtx,res);` |
|       3 | 2511 | `		return PH7_OK;` |
|       - | 2512 | `	}` |
|     572 | 2513 | `	if( nArg > 1 ){` |
|       - | 2514 | `		/* Recursive count? */` |
|      31 | 2515 | `		bRecursive = ph7_value_to_int(apArg[1]) == 1 /* COUNT_RECURSIVE */;` |
|      15 | 2516 | `	}` |
|       - | 2517 | `	/* Count */` |
|     572 | 2518 | `	iCount = HashmapCount((ph7_hashmap *)apArg[0]->x.pOther,bRecursive,0);` |
|     572 | 2519 | `	ph7_result_int64(pCtx,iCount);` |
|     572 | 2520 | `	return PH7_OK;` |
|     288 | 2521 |  |
|       - | 2522 | `/*` |
|       - | 2523 | ` * bool array_key_exists(value $key,array $search)` |
|       - | 2524 | ` *  Checks if the given key or index exists in the array.` |
|       - | 2525 | ` * Parameters` |
|       - | 2526 | ` * $key` |
|       - | 2527 | ` *   Value to check.` |
|       - | 2528 | ` * $search` |
|       - | 2529 | ` *  An array with keys to check.` |
|       - | 2530 | ` * Return` |
|       - | 2531 | ` *  TRUE on success or FALSE on failure.` |
|       - | 2532 | ` */` |
|      46 | 2533 | `static int ph7_hashmap_key_exists(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       2 | 2534 |  |
|       - | 2535 | `	sxi32 rc;` |
|      48 | 2536 | `	if( nArg != 2 ){` |
|       - | 2537 | `		/* PHP requires exactly two arguments */` |
|      10 | 2538 | `		return PH7_VmThrowException(pCtx,` |
|       - | 2539 | `			"ArgumentCountError",` |
|       - | 2540 | `			"array_key_exists() expects exactly 2 arguments, %d given",` |
|       3 | 2541 | `			nArg` |
|       - | 2542 | `			);` |
|       - | 2543 | `	}` |
|       - | 2544 | `	/* Make sure we are dealing with a valid hashmap */` |
|      42 | 2545 | `	if( !ph7_value_is_array(apArg[1]) ){` |
|       - | 2546 | `		/* Type mismatch -> TypeError */` |
|       7 | 2547 | `		return PH7_VmThrowException(pCtx,` |
|       - | 2548 | `			"TypeError",` |
|       - | 2549 | `			"array_key_exists(): Argument #2 ($array) must be of type array, %s given",` |
|       4 | 2550 | `			ph7_type_name(apArg[1])` |
|       - | 2551 | `			);` |
|       - | 2552 | `	}` |
|       - | 2553 | `	/* Emit deprecation warnings matching PHP behaviour */` |
|      37 | 2554 | `	if( apArg[0]->iFlags & MEMOBJ_NULL ){` |
|       3 | 2555 | `		ph7_context_throw_error_format(pCtx,8192,` |
|       - | 2556 | `			"Using null as the key parameter for array_key_exists() is deprecated, "` |
|       - | 2557 | `			"use an empty string instead"` |
|       - | 2558 | `			);` |
|      36 | 2559 | `	}else if( apArg[0]->iFlags & MEMOBJ_REAL ){` |
|       3 | 2560 | `		ph7_real rVal = apArg[0]->rVal;` |
|       3 | 2561 | `		if( rVal != (ph7_real)(sxi64)rVal ){` |
|       4 | 2562 | `			ph7_context_throw_error_format(pCtx,8192,` |
|       - | 2563 | `				"Implicit conversion from float %g to int loses precision"` |
|       1 | 2564 | `				,rVal` |
|       - | 2565 | `				);` |
|       1 | 2566 | `		}` |
|       1 | 2567 | `	}` |
|       - | 2568 | `	/* Perform the lookup */` |
|      37 | 2569 | `	rc = PH7_HashmapLookup((ph7_hashmap *)apArg[1]->x.pOther,apArg[0],0);` |
|       - | 2570 | `	/* lookup result */` |
|      37 | 2571 | `	ph7_result_bool(pCtx,rc == SXRET_OK ? 1 : 0);` |
|      37 | 2572 | `	return PH7_OK;` |
|      25 | 2573 |  |
|       - | 2574 | `/*` |
|       - | 2575 | ` * value array_pop(array $array)` |
|       - | 2576 | ` *   POP the last inserted element from the array.` |
|       - | 2577 | ` * Parameter` |
|       - | 2578 | ` *  The array to get the value from.` |
|       - | 2579 | ` * Return` |
|       - | 2580 | ` *  Poped value or NULL on failure.` |
|       - | 2581 | ` */` |
|      16 | 2582 | `static int ph7_hashmap_pop(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       2 | 2583 |  |
|       - | 2584 | `	ph7_hashmap *pMap;` |
|       - | 2585 | `	/* PHP requires exactly one argument and it must be passed by reference */` |
|      18 | 2586 | `	if( nArg != 1 ){` |
|       7 | 2587 | `		return PH7_VmThrowException(pCtx,` |
|       - | 2588 | `			"ArgumentCountError",` |
|       - | 2589 | `			"array_pop() expects exactly 1 argument, %d given",` |
|       2 | 2590 | `			nArg` |
|       - | 2591 | `			);` |
|       - | 2592 | `	}` |
|       - | 2593 | `	/* Passing a constant (including literals) or non-variable triggers the same` |
|       - | 2594 | `	 * error message as official PHP. Check the index to detect constants. */` |
|      14 | 2595 | `	if( apArg[0]->nIdx == SXU32_HIGH ){` |
|       5 | 2596 | `		return PH7_VmThrowException(pCtx,` |
|       - | 2597 | `			"Error",` |
|       - | 2598 | `			"array_pop(): Argument #1 ($array) could not be passed by reference"` |
|       - | 2599 | `			);` |
|       - | 2600 | `	}` |
|       - | 2601 | `	/* Make sure we are dealing with a valid hashmap */` |
|      10 | 2602 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|       4 | 2603 | `		return PH7_VmThrowException(pCtx,` |
|       - | 2604 | `			"TypeError",` |
|       - | 2605 | `			"array_pop(): Argument #1 ($array) must be of type array, %s given",` |
|       1 | 2606 | `			ph7_type_name(apArg[0])` |
|       - | 2607 | `			);` |
|       - | 2608 | `	}` |
|       7 | 2609 | `	pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|       7 | 2610 | `	if( pMap->nEntry < 1 ){` |
|       - | 2611 | `		/* Nothing to pop,return NULL */` |
|       3 | 2612 | `		ph7_result_null(pCtx);` |
|       2 | 2613 | `	}else{` |
|       5 | 2614 | `		ph7_hashmap_node *pLast = pMap->pLast;` |
|       - | 2615 | `		ph7_value *pObj;` |
|       5 | 2616 | `		pObj = HashmapExtractNodeValue(pLast);` |
|       5 | 2617 | `		if( pObj ){` |
|       - | 2618 | `			/* Node value */` |
|       5 | 2619 | `			ph7_result_value(pCtx,pObj);` |
|       - | 2620 | `			/* Unlink the node */` |
|       5 | 2621 | `			PH7_HashmapUnlinkNode(pLast,TRUE);` |
|       3 | 2622 | `		}else{` |
|     ! 0 | 2623 | `			ph7_result_null(pCtx);` |
|       - | 2624 | `		}` |
|       - | 2625 | `		/* Reset the cursor */` |
|       5 | 2626 | `		pMap->pCur = pMap->pFirst;` |
|       - | 2627 | `	}` |
|       7 | 2628 | `	return PH7_OK;` |
|      10 | 2629 |  |
|       - | 2630 | `/*` |
|       - | 2631 | ` * int array_push($array,$var,...)` |
|       - | 2632 | ` *   Push one or more elements onto the end of array. (Stack insertion)` |
|       - | 2633 | ` * Parameters` |
|       - | 2634 | ` *  array` |
|       - | 2635 | ` *    The input array.` |
|       - | 2636 | ` *  var` |
|       - | 2637 | ` *   On or more value to push.` |
|       - | 2638 | ` * Return` |
|       - | 2639 | ` *  New array count (including old items).` |
|       - | 2640 | ` */` |
|      20 | 2641 | `static int ph7_hashmap_push(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       2 | 2642 |  |
|       - | 2643 | `	ph7_hashmap *pMap;` |
|       - | 2644 | `	sxi32 rc;` |
|       - | 2645 | `	int i;` |
|      22 | 2646 | `	if( nArg < 1 ){` |
|       4 | 2647 | `		return PH7_VmThrowException(pCtx,` |
|       - | 2648 | `			"ArgumentCountError",` |
|       - | 2649 | `			"array_push() expects at least 1 argument, %d given",` |
|       1 | 2650 | `			nArg` |
|       - | 2651 | `			);` |
|       - | 2652 | `	}` |
|       - | 2653 | `	/* Passing a constant (including literals) or non-variable triggers the same` |
|       - | 2654 | `	 * error message as official PHP. Check the index to detect constants. */` |
|      20 | 2655 | `	if( apArg[0]->nIdx == SXU32_HIGH ){` |
|       5 | 2656 | `		return PH7_VmThrowException(pCtx,` |
|       - | 2657 | `			"Error",` |
|       - | 2658 | `			"array_push(): Argument #1 ($array) could not be passed by reference"` |
|       - | 2659 | `			);` |
|       - | 2660 | `	}` |
|       - | 2661 | `	/* Make sure we are dealing with a valid hashmap */` |
|      16 | 2662 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|       4 | 2663 | `		return PH7_VmThrowException(pCtx,` |
|       - | 2664 | `			"TypeError",` |
|       - | 2665 | `			"array_push(): Argument #1 ($array) must be of type array, %s given",` |
|       1 | 2666 | `			ph7_type_name(apArg[0])` |
|       - | 2667 | `			);` |
|       - | 2668 | `	}` |
|       - | 2669 | `	/* Point to the internal representation of the input hashmap */` |
|      13 | 2670 | `	pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|       - | 2671 | `	/* Start pushing given values */` |
|      27 | 2672 | `	for( i = 1 ; i < nArg ; ++i ){` |
|      15 | 2673 | `		rc = PH7_HashmapInsert(pMap,0,apArg[i]);` |
|      15 | 2674 | `		if( rc != SXRET_OK ){` |
|     ! 0 | 2675 | `			break;` |
|       - | 2676 | `		}` |
|       8 | 2677 | `	}` |
|       - | 2678 | `	/* Return the new count */` |
|      13 | 2679 | `	ph7_result_int64(pCtx,(sxi64)pMap->nEntry);` |
|      13 | 2680 | `	return PH7_OK;` |
|      12 | 2681 |  |
|       - | 2682 | `/*` |
|       - | 2683 | ` * value array_shift(array $array)` |
|       - | 2684 | ` *   Shift an element off the beginning of array.` |
|       - | 2685 | ` * Parameter` |
|       - | 2686 | ` *  The array to get the value from.` |
|       - | 2687 | ` * Return` |
|       - | 2688 | ` *  Shifted value or NULL on failure.` |
|       - | 2689 | ` */` |
|      36 | 2690 | `static int ph7_hashmap_shift(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       2 | 2691 |  |
|       - | 2692 | `	ph7_hashmap *pMap;` |
|       - | 2693 | `	/* PHP requires exactly one argument and it must be passed by reference */` |
|      38 | 2694 | `	if( nArg != 1 ){` |
|       7 | 2695 | `		return PH7_VmThrowException(pCtx,` |
|       - | 2696 | `			"ArgumentCountError",` |
|       - | 2697 | `			"array_shift() expects exactly 1 argument, %d given",` |
|       2 | 2698 | `			nArg` |
|       - | 2699 | `			);` |
|       - | 2700 | `	}` |
|       - | 2701 | `	/* Detect constants or literals, which cannot be passed by reference. */` |
|      34 | 2702 | `	if( apArg[0]->nIdx == SXU32_HIGH ){` |
|       5 | 2703 | `		return PH7_VmThrowException(pCtx,` |
|       - | 2704 | `			"Error",` |
|       - | 2705 | `			"array_shift(): Argument #1 ($array) could not be passed by reference"` |
|       - | 2706 | `			);` |
|       - | 2707 | `	}` |
|       - | 2708 | `	/* Make sure we are dealing with a valid hashmap */` |
|      30 | 2709 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|       4 | 2710 | `		return PH7_VmThrowException(pCtx,` |
|       - | 2711 | `			"TypeError",` |
|       - | 2712 | `			"array_shift(): Argument #1 ($array) must be of type array, %s given",` |
|       1 | 2713 | `			ph7_type_name(apArg[0])` |
|       - | 2714 | `			);` |
|       - | 2715 | `	}` |
|       - | 2716 | `	/* Point to the internal representation of the hashmap */` |
|      28 | 2717 | `	pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|      28 | 2718 | `	if( pMap->nEntry < 1 ){` |
|       - | 2719 | `		/* Empty hashmap,return NULL */` |
|       3 | 2720 | `		ph7_result_null(pCtx);` |
|       2 | 2721 | `	}else{` |
|      26 | 2722 | `		ph7_hashmap_node *pEntry = pMap->pFirst;` |
|       - | 2723 | `		ph7_value *pObj;` |
|       - | 2724 | `		sxu32 n;` |
|      26 | 2725 | `		pObj = HashmapExtractNodeValue(pEntry);` |
|      26 | 2726 | `		if( pObj ){` |
|       - | 2727 | `			/* Node value */` |
|      26 | 2728 | `			ph7_result_value(pCtx,pObj);` |
|       - | 2729 | `			/* Unlink the first node */` |
|      26 | 2730 | `			PH7_HashmapUnlinkNode(pEntry,TRUE);` |
|      14 | 2731 | `		}else{` |
|     ! 0 | 2732 | `			ph7_result_null(pCtx);` |
|       - | 2733 | `		}` |
|       - | 2734 | `		/* Rehash all int keys */` |
|      26 | 2735 | `		n = pMap->nEntry;` |
|      26 | 2736 | `		pEntry = pMap->pFirst;` |
|      26 | 2737 | `		pMap->iNextIdx = 0; /* Reset the automatic index */` |
|      37 | 2738 | `		for(;;){` |
|      76 | 2739 | `			if( n < 1 ){` |
|      26 | 2740 | `				break;` |
|       - | 2741 | `			}` |
|      52 | 2742 | `			if( pEntry->iType == HASHMAP_INT_NODE ){` |
|      52 | 2743 | `				HashmapRehashIntNode(pEntry);` |
|      25 | 2744 | `			}` |
|       - | 2745 | `			/* Point to the next entry */` |
|      52 | 2746 | `			pEntry = pEntry->pPrev; /* Reverse link */` |
|      52 | 2747 | `			n--;` |
|       2 | 2748 | `		}` |
|       - | 2749 | `		/* Reset the cursor */` |
|      26 | 2750 | `		pMap->pCur = pMap->pFirst;` |
|       - | 2751 | `	}` |
|      28 | 2752 | `	return PH7_OK;` |
|      20 | 2753 |  |
|       - | 2754 | `/*` |
|       - | 2755 | ` * Extract the node cursor value.` |
|       - | 2756 | ` */` |
|      24 | 2757 | `static sxi32 HashmapCurrentValue(ph7_context *pCtx,ph7_hashmap *pMap,int iDirection)` |
|       1 | 2758 |  |
|      25 | 2759 | `	ph7_hashmap_node *pCur = pMap->pCur;` |
|       - | 2760 | `	ph7_value *pVal;` |
|      25 | 2761 | `	if( pCur == 0 ){` |
|       - | 2762 | `		/* Cursor does not point to anything,return FALSE */` |
|     ! 0 | 2763 | `		ph7_result_bool(pCtx,0);` |
|     ! 0 | 2764 | `		return PH7_OK;` |
|       - | 2765 | `	}` |
|      25 | 2766 | `	if( iDirection != 0 ){` |
|       9 | 2767 | `		if( iDirection > 0 ){` |
|       - | 2768 | `			/* Point to the next entry */` |
|       7 | 2769 | `			pMap->pCur = pCur->pPrev; /* Reverse link */` |
|       7 | 2770 | `			pCur = pMap->pCur;` |
|       4 | 2771 | `		}else{` |
|       - | 2772 | `			/* Point to the previous entry */` |
|       3 | 2773 | `			pMap->pCur = pCur->pNext; /* Reverse link */` |
|       3 | 2774 | `			pCur = pMap->pCur;` |
|       - | 2775 | `		}` |
|       9 | 2776 | `		if( pCur == 0 ){` |
|       - | 2777 | `			/* End of input reached,return FALSE */` |
|     ! 0 | 2778 | `			ph7_result_bool(pCtx,0);` |
|     ! 0 | 2779 | `			return PH7_OK;` |
|       - | 2780 | `		}` |
|       4 | 2781 | `	}` |
|       - | 2782 | `	/* Point to the desired element */` |
|      25 | 2783 | `	pVal = HashmapExtractNodeValue(pCur);` |
|      25 | 2784 | `	if( pVal ){` |
|      25 | 2785 | `		ph7_result_value(pCtx,pVal);` |
|      13 | 2786 | `	}else{` |
|     ! 0 | 2787 | `		ph7_result_bool(pCtx,0);` |
|       - | 2788 | `	}` |
|      25 | 2789 | `	return PH7_OK;` |
|      13 | 2790 |  |
|       - | 2791 | `/*` |
|       - | 2792 | ` * value current(array $array)` |
|       - | 2793 | ` *  Return the current element in an array.` |
|       - | 2794 | ` * Parameter` |
|       - | 2795 | ` *  $input: The input array.` |
|       - | 2796 | ` * Return` |
|       - | 2797 | ` *  The current() function simply returns the value of the array element that's currently` |
|       - | 2798 | ` *  being pointed to by the internal pointer. It does not move the pointer in any way.` |
|       - | 2799 | ` *  If the internal pointer points beyond the end of the elements list or the array` |
|       - | 2800 | ` *  is empty, current() returns FALSE.` |
|       - | 2801 | ` */` |
|      10 | 2802 | `static int ph7_hashmap_current(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       1 | 2803 |  |
|      11 | 2804 | `	if( nArg < 1 ){` |
|       - | 2805 | `		/* Missing arguments,return FALSE */` |
|     ! 0 | 2806 | `		ph7_result_bool(pCtx,0);` |
|     ! 0 | 2807 | `		return PH7_OK;` |
|       - | 2808 | `	}` |
|       - | 2809 | `	/* Make sure we are dealing with a valid hashmap */` |
|      11 | 2810 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|       - | 2811 | `		/* Invalid argument,return FALSE */` |
|     ! 0 | 2812 | `		ph7_result_bool(pCtx,0);` |
|     ! 0 | 2813 | `		return PH7_OK;` |
|       - | 2814 | `	}` |
|      11 | 2815 | `	HashmapCurrentValue(&(*pCtx),(ph7_hashmap *)apArg[0]->x.pOther,0);` |
|      11 | 2816 | `	return PH7_OK;` |
|       6 | 2817 |  |
|       - | 2818 | `/*` |
|       - | 2819 | ` * value next(array $input)` |
|       - | 2820 | ` *  Advance the internal array pointer of an array.` |
|       - | 2821 | ` * Parameter` |
|       - | 2822 | ` *  $input: The input array.` |
|       - | 2823 | ` * Return` |
|       - | 2824 | ` *  next() behaves like current(), with one difference. It advances the internal array` |
|       - | 2825 | ` *  pointer one place forward before returning the element value. That means it returns` |
|       - | 2826 | ` *  the next array value and advances the internal array pointer by one.` |
|       - | 2827 | ` */` |
|       6 | 2828 | `static int ph7_hashmap_next(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       1 | 2829 |  |
|       7 | 2830 | `	if( nArg < 1 ){` |
|       - | 2831 | `		/* Missing arguments,return FALSE */` |
|     ! 0 | 2832 | `		ph7_result_bool(pCtx,0);` |
|     ! 0 | 2833 | `		return PH7_OK;` |
|       - | 2834 | `	}` |
|       - | 2835 | `	/* Make sure we are dealing with a valid hashmap */` |
|       7 | 2836 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|       - | 2837 | `		/* Invalid argument,return FALSE */` |
|     ! 0 | 2838 | `		ph7_result_bool(pCtx,0);` |
|     ! 0 | 2839 | `		return PH7_OK;` |
|       - | 2840 | `	}` |
|       7 | 2841 | `	HashmapCurrentValue(&(*pCtx),(ph7_hashmap *)apArg[0]->x.pOther,1);` |
|       7 | 2842 | `	return PH7_OK;` |
|       4 | 2843 |  |
|       - | 2844 | `/*` |
|       - | 2845 | ` * value prev(array $input)` |
|       - | 2846 | ` *  Rewind the internal array pointer.` |
|       - | 2847 | ` * Parameter` |
|       - | 2848 | ` *  $input: The input array.` |
|       - | 2849 | ` * Return` |
|       - | 2850 | ` *  Returns the array value in the previous place that's pointed` |
|       - | 2851 | ` *  to by the internal array pointer, or FALSE if there are no more` |
|       - | 2852 | ` *  elements.` |
|       - | 2853 | ` */` |
|       2 | 2854 | `static int ph7_hashmap_prev(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       1 | 2855 |  |
|       3 | 2856 | `	if( nArg < 1 ){` |
|       - | 2857 | `		/* Missing arguments,return FALSE */` |
|     ! 0 | 2858 | `		ph7_result_bool(pCtx,0);` |
|     ! 0 | 2859 | `		return PH7_OK;` |
|       - | 2860 | `	}` |
|       - | 2861 | `	/* Make sure we are dealing with a valid hashmap */` |
|       3 | 2862 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|       - | 2863 | `		/* Invalid argument,return FALSE */` |
|     ! 0 | 2864 | `		ph7_result_bool(pCtx,0);` |
|     ! 0 | 2865 | `		return PH7_OK;` |
|       - | 2866 | `	}` |
|       3 | 2867 | `	HashmapCurrentValue(&(*pCtx),(ph7_hashmap *)apArg[0]->x.pOther,-1);` |
|       3 | 2868 | `	return PH7_OK;` |
|       2 | 2869 |  |
|       - | 2870 | `/*` |
|       - | 2871 | ` * value end(array $input)` |
|       - | 2872 | ` *  Set the internal pointer of an array to its last element.` |
|       - | 2873 | ` * Parameter` |
|       - | 2874 | ` *  $input: The input array.` |
|       - | 2875 | ` * Return` |
|       - | 2876 | ` *  Returns the value of the last element or FALSE for empty array.` |
|       - | 2877 | ` */` |
|       2 | 2878 | `static int ph7_hashmap_end(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       1 | 2879 |  |
|       - | 2880 | `	ph7_hashmap *pMap;` |
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
|       - | 2892 | `	/* Point to the internal representation of the input hashmap */` |
|       3 | 2893 | `	pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|       - | 2894 | `	/* Point to the last node */` |
|       3 | 2895 | `	pMap->pCur = pMap->pLast;` |
|       - | 2896 | `	/* Return the last node value */` |
|       3 | 2897 | `	HashmapCurrentValue(&(*pCtx),pMap,0);` |
|       3 | 2898 | `	return PH7_OK;` |
|       2 | 2899 |  |
|       - | 2900 | `/*` |
|       - | 2901 | ` * value reset(array $array )` |
|       - | 2902 | ` *  Set the internal pointer of an array to its first element.` |
|       - | 2903 | ` * Parameter` |
|       - | 2904 | ` *  $input: The input array.` |
|       - | 2905 | ` * Return` |
|       - | 2906 | ` *  Returns the value of the first array element,or FALSE if the array is empty.` |
|       - | 2907 | ` */` |
|       4 | 2908 | `static int ph7_hashmap_reset(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       1 | 2909 |  |
|       - | 2910 | `	ph7_hashmap *pMap;` |
|       5 | 2911 | `	if( nArg < 1 ){` |
|       - | 2912 | `		/* Missing arguments,return FALSE */` |
|     ! 0 | 2913 | `		ph7_result_bool(pCtx,0);` |
|     ! 0 | 2914 | `		return PH7_OK;` |
|       - | 2915 | `	}` |
|       - | 2916 | `	/* Make sure we are dealing with a valid hashmap */` |
|       5 | 2917 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|       - | 2918 | `		/* Invalid argument,return FALSE */` |
|     ! 0 | 2919 | `		ph7_result_bool(pCtx,0);` |
|     ! 0 | 2920 | `		return PH7_OK;` |
|       - | 2921 | `	}` |
|       - | 2922 | `	/* Point to the internal representation of the input hashmap */` |
|       5 | 2923 | `	pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|       - | 2924 | `	/* Point to the first node */` |
|       5 | 2925 | `	pMap->pCur = pMap->pFirst;` |
|       - | 2926 | `	/* Return the last node value if available */` |
|       5 | 2927 | `	HashmapCurrentValue(&(*pCtx),pMap,0);` |
|       5 | 2928 | `	return PH7_OK;` |
|       3 | 2929 |  |
|       - | 2930 | `/*` |
|       - | 2931 | ` * value key(array $array)` |
|       - | 2932 | ` *   Fetch a key from an array` |
|       - | 2933 | ` * Parameter` |
|       - | 2934 | ` *  $input` |
|       - | 2935 | ` *   The input array.` |
|       - | 2936 | ` * Return` |
|       - | 2937 | ` *  The key() function simply returns the key of the array element that's currently` |
|       - | 2938 | ` *  being pointed to by the internal pointer. It does not move the pointer in any way.` |
|       - | 2939 | ` *  If the internal pointer points beyond the end of the elements list or the array` |
|       - | 2940 | ` *  is empty, key() returns NULL.` |
|       - | 2941 | ` */` |
|       4 | 2942 | `static int ph7_hashmap_simple_key(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       1 | 2943 |  |
|       - | 2944 | `	ph7_hashmap_node *pCur;` |
|       - | 2945 | `	ph7_hashmap *pMap;` |
|       5 | 2946 | `	if( nArg < 1 ){` |
|       - | 2947 | `		/* Missing arguments,return NULL */` |
|     ! 0 | 2948 | `		ph7_result_null(pCtx);` |
|     ! 0 | 2949 | `		return PH7_OK;` |
|       - | 2950 | `	}` |
|       - | 2951 | `	/* Make sure we are dealing with a valid hashmap */` |
|       5 | 2952 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|       - | 2953 | `		/* Invalid argument,return NULL */` |
|     ! 0 | 2954 | `		ph7_result_null(pCtx);` |
|     ! 0 | 2955 | `		return PH7_OK;` |
|       - | 2956 | `	}` |
|       5 | 2957 | `	pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|       5 | 2958 | `	pCur = pMap->pCur;` |
|       5 | 2959 | `	if( pCur == 0 ){` |
|       - | 2960 | `		/* Cursor does not point to anything,return NULL */` |
|     ! 0 | 2961 | `		ph7_result_null(pCtx);` |
|     ! 0 | 2962 | `		return PH7_OK;` |
|       - | 2963 | `	}` |
|       5 | 2964 | `	if( pCur->iType == HASHMAP_INT_NODE){` |
|       - | 2965 | `		/* Key is integer */` |
|     ! 0 | 2966 | `		ph7_result_int64(pCtx,pCur->xKey.iKey);` |
|     ! 0 | 2967 | `	}else{` |
|       - | 2968 | `		/* Key is blob */` |
|       7 | 2969 | `		ph7_result_string(pCtx,` |
|       4 | 2970 | `			(const char *)SyBlobData(&pCur->xKey.sKey),(int)SyBlobLength(&pCur->xKey.sKey));` |
|       - | 2971 | `	}` |
|       5 | 2972 | `	return PH7_OK;` |
|       3 | 2973 |  |
|       - | 2974 | `/*` |
|       - | 2975 | ` * array each(array $input)` |
|       - | 2976 | ` *  Return the current key and value pair from an array and advance the array cursor.` |
|       - | 2977 | ` * Parameter` |
|       - | 2978 | ` *  $input` |
|       - | 2979 | ` *    The input array.` |
|       - | 2980 | ` * Return` |
|       - | 2981 | ` *  Returns the current key and value pair from the array array. This pair is returned` |
|       - | 2982 | ` *  in a four-element array, with the keys 0, 1, key, and value. Elements 0 and key` |
|       - | 2983 | ` *  contain the key name of the array element, and 1 and value contain the data.` |
|       - | 2984 | ` *  If the internal pointer for the array points past the end of the array contents` |
|       - | 2985 | ` *  each() returns FALSE.` |
|       - | 2986 | ` */` |
|      22 | 2987 | `static int ph7_hashmap_each(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       1 | 2988 |  |
|       - | 2989 | `	ph7_hashmap_node *pCur;` |
|       - | 2990 | `	ph7_hashmap *pMap;` |
|       - | 2991 | `	ph7_value *pArray;` |
|       - | 2992 | `	ph7_value *pVal;` |
|       - | 2993 | `	ph7_value sKey;` |
|      23 | 2994 | `	if( nArg < 1 ){` |
|       - | 2995 | `		/* Missing arguments,return FALSE */` |
|     ! 0 | 2996 | `		ph7_result_bool(pCtx,0);` |
|     ! 0 | 2997 | `		return PH7_OK;` |
|       - | 2998 | `	}` |
|       - | 2999 | `	/* Make sure we are dealing with a valid hashmap */` |
|      23 | 3000 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|       - | 3001 | `		/* Invalid argument,return FALSE */` |
|     ! 0 | 3002 | `		ph7_result_bool(pCtx,0);` |
|     ! 0 | 3003 | `		return PH7_OK;` |
|       - | 3004 | `	}` |
|       - | 3005 | `	/* Point to the internal representation that describe the input hashmap */` |
|      23 | 3006 | `	pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|      23 | 3007 | `	if( pMap->pCur == 0 ){` |
|       - | 3008 | `		/* Cursor does not point to anything,return FALSE */` |
|       9 | 3009 | `		ph7_result_bool(pCtx,0);` |
|       9 | 3010 | `		return PH7_OK;` |
|       - | 3011 | `	}` |
|      15 | 3012 | `	pCur = pMap->pCur;` |
|       - | 3013 | `	/* Create a new array */` |
|      15 | 3014 | `	pArray = ph7_context_new_array(pCtx);` |
|      15 | 3015 | `	if( pArray == 0 ){` |
|     ! 0 | 3016 | `		ph7_result_bool(pCtx,0);` |
|     ! 0 | 3017 | `		return PH7_OK;` |
|       - | 3018 | `	}` |
|      15 | 3019 | `	pVal = HashmapExtractNodeValue(pCur);` |
|       - | 3020 | `	/* Insert the current value */` |
|      15 | 3021 | `	ph7_array_add_intkey_elem(pArray,1,pVal);` |
|      15 | 3022 | `	ph7_array_add_strkey_elem(pArray,"value",pVal);` |
|       - | 3023 | `	/* Make the key */` |
|      15 | 3024 | `	if( pCur->iType == HASHMAP_INT_NODE ){` |
|       7 | 3025 | `		PH7_MemObjInitFromInt(pMap->pVm,&sKey,pCur->xKey.iKey);` |
|       4 | 3026 | `	}else{` |
|       9 | 3027 | `		PH7_MemObjInitFromString(pMap->pVm,&sKey,0);` |
|       9 | 3028 | `		PH7_MemObjStringAppend(&sKey,(const char *)SyBlobData(&pCur->xKey.sKey),SyBlobLength(&pCur->xKey.sKey));` |
|       - | 3029 | `	}` |
|       - | 3030 | `	/* Insert the current key */` |
|      15 | 3031 | `	ph7_array_add_intkey_elem(pArray,0,&sKey);` |
|      15 | 3032 | `	ph7_array_add_strkey_elem(pArray,"key",&sKey);` |
|      15 | 3033 | `	PH7_MemObjRelease(&sKey);` |
|       - | 3034 | `	/* Advance the cursor */` |
|      15 | 3035 | `	pMap->pCur = pCur->pPrev; /* Reverse link */` |
|       - | 3036 | `	/* Return the current entry */` |
|      15 | 3037 | `	ph7_result_value(pCtx,pArray);` |
|      15 | 3038 | `	return PH7_OK;` |
|      12 | 3039 |  |
|       - | 3040 | `/*` |
|       - | 3041 | ` * array range(int $start,int $limit,int $step)` |
|       - | 3042 | ` *  Create an array containing a range of elements` |
|       - | 3043 | ` * Parameter` |
|       - | 3044 | ` *  start` |
|       - | 3045 | ` *   First value of the sequence.` |
|       - | 3046 | ` *  limit` |
|       - | 3047 | ` *   The sequence is ended upon reaching the limit value.` |
|       - | 3048 | ` *  step` |
|       - | 3049 | ` *  If a step value is given, it will be used as the increment between elements in the sequence.` |
|       - | 3050 | ` *  step should be given as a positive number. If not specified, step will default to 1.` |
|       - | 3051 | ` * Return` |
|       - | 3052 | ` *  An array of elements from start to limit, inclusive.` |
|       - | 3053 | ` * NOTE:` |
|       - | 3054 | ` *  Only 32/64 bit integer key is supported.` |
|       - | 3055 | ` */` |
|       2 | 3056 | `static int ph7_hashmap_range(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       1 | 3057 |  |
|       - | 3058 | `	ph7_value *pValue,*pArray;` |
|       - | 3059 | `	sxi64 iOfft,iLimit;` |
|       3 | 3060 | `	int iStep = 1;` |
|       - | 3061 |  |
|       3 | 3062 | `	iOfft = iLimit = 0; /* cc -O6 */` |
|       3 | 3063 | `	if( nArg > 0 ){` |
|       - | 3064 | `		/* Extract the offset */` |
|       3 | 3065 | `		iOfft = ph7_value_to_int64(apArg[0]);` |
|       3 | 3066 | `		if( nArg > 1 ){` |
|       - | 3067 | `			/* Extract the limit */` |
|       3 | 3068 | `			iLimit = ph7_value_to_int64(apArg[1]);` |
|       3 | 3069 | `			if( nArg > 2 ){` |
|       - | 3070 | `				/* Extract the increment */` |
|       3 | 3071 | `				iStep = ph7_value_to_int(apArg[2]);` |
|       3 | 3072 | `				if( iStep < 1 ){` |
|       - | 3073 | `					/* Only positive number are allowed */` |
|       3 | 3074 | `					iStep = 1;` |
|       1 | 3075 | `				}` |
|       1 | 3076 | `			}` |
|       1 | 3077 | `		}` |
|       1 | 3078 | `	}` |
|       - | 3079 | `	/* Element container */` |
|       3 | 3080 | `	pValue = ph7_context_new_scalar(pCtx);` |
|       - | 3081 | `	/* Create the new array */` |
|       3 | 3082 | `	pArray = ph7_context_new_array(pCtx);` |
|       3 | 3083 | `	if( pArray == 0 ){` |
|     ! 0 | 3084 | `		ph7_result_null(pCtx);` |
|     ! 0 | 3085 | `		return PH7_OK;` |
|       - | 3086 | `	}` |
|       - | 3087 | `	/* Start filling */` |
|       3 | 3088 | `	while( iOfft <= iLimit ){` |
|     ! 0 | 3089 | `		ph7_value_int64(pValue,iOfft);` |
|       - | 3090 | `		/* Perform the insertion */` |
|     ! 0 | 3091 | `		ph7_array_add_elem(pArray,0/* Automatic index assign*/,pValue);` |
|       - | 3092 | `		/* Increment */` |
|     ! 0 | 3093 | `		iOfft += iStep;` |
|     ! 0 | 3094 | `	}` |
|       - | 3095 | `	/* Return the new array */` |
|       3 | 3096 | `	ph7_result_value(pCtx,pArray);` |
|       - | 3097 | `	/* Dont'worry about freeing 'pValue',it will be released automatically` |
|       - | 3098 | `	 * by the virtual machine as soon we return from this foreign function.` |
|       - | 3099 | `	 */` |
|       3 | 3100 | `	return PH7_OK;` |
|       2 | 3101 |  |
|       - | 3102 | `/*` |
|       - | 3103 | ` * array array_values(array $array)` |
|       - | 3104 | ` *  Return all the values of an array, indexed numerically.` |
|       - | 3105 | ` * Parameters` |
|       - | 3106 | ` *  $array` |
|       - | 3107 | ` *   The input array.` |
|       - | 3108 | ` * Return` |
|       - | 3109 | ` *  An indexed array of values or NULL on allocation failure.` |
|       - | 3110 | ` */` |
|      30 | 3111 | `static int ph7_hashmap_values(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       2 | 3112 |  |
|       - | 3113 | `	ph7_hashmap_node *pNode;` |
|       - | 3114 | `	ph7_hashmap *pMap;` |
|       - | 3115 | `	ph7_value *pArray;` |
|       - | 3116 | `	ph7_value *pObj;` |
|       - | 3117 | `	sxu32 n;` |
|      32 | 3118 | `	if( nArg != 1 ){` |
|       - | 3119 | `		/* Wrong argument count, throw ArgumentCountError */` |
|       7 | 3120 | `		return PH7_VmThrowException(pCtx,` |
|       - | 3121 | `			"ArgumentCountError",` |
|       - | 3122 | `			"array_values() expects exactly 1 argument, %d given",` |
|       2 | 3123 | `			nArg` |
|       - | 3124 | `			);` |
|       - | 3125 | `	}` |
|       - | 3126 | `	/* Make sure we are dealing with a valid hashmap */` |
|      28 | 3127 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|       - | 3128 | `		/* Type mismatch, throw TypeError */` |
|       4 | 3129 | `		return PH7_VmThrowException(pCtx,` |
|       - | 3130 | `			"TypeError",` |
|       - | 3131 | `			"array_values(): Argument #1 ($array) must be of type array, %s given",` |
|       1 | 3132 | `			ph7_type_name(apArg[0])` |
|       - | 3133 | `			);` |
|       - | 3134 | `	}` |
|       - | 3135 | `	/* Point to the internal representation that describe the input hashmap */` |
|      25 | 3136 | `	pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|       - | 3137 | `	/* Create a new array */` |
|      25 | 3138 | `	pArray = ph7_context_new_array(pCtx);` |
|      25 | 3139 | `	if( pArray == 0 ){` |
|     ! 0 | 3140 | `		ph7_result_null(pCtx);` |
|     ! 0 | 3141 | `		return PH7_OK;` |
|       - | 3142 | `	}` |
|       - | 3143 | `	/* Perform the requested operation */` |
|      25 | 3144 | `	pNode = pMap->pFirst;` |
|      83 | 3145 | `	for( n = 0 ; n < pMap->nEntry ; ++n ){` |
|      59 | 3146 | `		pObj = HashmapExtractNodeValue(pNode);` |
|      59 | 3147 | `		if( pObj ){` |
|       - | 3148 | `			/* perform the insertion */` |
|      59 | 3149 | `			ph7_array_add_elem(pArray,0/* Automatic index assign */,pObj);` |
|      29 | 3150 | `		}` |
|       - | 3151 | `		/* Point to the next entry */` |
|      59 | 3152 | `		pNode = pNode->pPrev; /* Reverse link */` |
|      30 | 3153 | `	}` |
|       - | 3154 | `	/* return the new array */` |
|      25 | 3155 | `	ph7_result_value(pCtx,pArray);` |
|      25 | 3156 | `	return PH7_OK;` |
|      17 | 3157 |  |
|       - | 3158 | `/*` |
|       - | 3159 | ` * array array_keys(array $input [, val $search_value [, bool $strict = false ]] )` |
|       - | 3160 | ` *  Return all the keys or a subset of the keys of an array.` |
|       - | 3161 | ` * Parameters` |
|       - | 3162 | ` *  $input` |
|       - | 3163 | ` *   An array containing keys to return.` |
|       - | 3164 | ` * $search_value` |
|       - | 3165 | ` *   If specified, then only keys containing these values are returned.` |
|       - | 3166 | ` * $strict` |
|       - | 3167 | ` *   Determines if strict comparison (===) should be used during the search.` |
|       - | 3168 | ` * Return` |
|       - | 3169 | ` *  An array of all the keys in input or NULL on failure.` |
|       - | 3170 | ` */` |
|     120 | 3171 | `static int ph7_hashmap_keys(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       2 | 3172 |  |
|       - | 3173 | `	ph7_hashmap_node *pNode;` |
|       - | 3174 | `	ph7_hashmap *pMap;` |
|       - | 3175 | `	ph7_value *pArray;` |
|       - | 3176 | `	ph7_value sObj;` |
|       - | 3177 | `	ph7_value sVal;` |
|       - | 3178 | `	SyString sKey;` |
|       - | 3179 | `	int bStrict;` |
|       - | 3180 | `	sxi32 rc;` |
|       - | 3181 | `	sxu32 n;` |
|     122 | 3182 | `	if( nArg < 1 ){` |
|       - | 3183 | `		/* Missing argument,throw ArgumentCountError */` |
|       3 | 3184 | `		return PH7_VmThrowException(pCtx,` |
|       - | 3185 | `			"ArgumentCountError",` |
|       - | 3186 | `			"array_keys() expects at least 1 argument, 0 given"` |
|       - | 3187 | `			);` |
|       - | 3188 | `	}` |
|       - | 3189 | `	/* Make sure we are dealing with a valid hashmap */` |
|     120 | 3190 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|       - | 3191 | `		/* haystack must be an array,throw TypeError */` |
|       4 | 3192 | `		return PH7_VmThrowException(pCtx,` |
|       - | 3193 | `			"TypeError",` |
|       - | 3194 | `			"array_keys(): Argument #1 ($array) must be of type array, %s given",` |
|       1 | 3195 | `			ph7_type_name(apArg[0])` |
|       - | 3196 | `			);` |
|       - | 3197 | `	}` |
|       - | 3198 | `	/* Point to the internal representation of the input hashmap */` |
|     118 | 3199 | `	pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|       - | 3200 | `	/* Create a new array */` |
|     118 | 3201 | `	pArray = ph7_context_new_array(pCtx);` |
|     118 | 3202 | `	if( pArray == 0 ){` |
|     ! 0 | 3203 | `		ph7_result_null(pCtx);` |
|     ! 0 | 3204 | `		return PH7_OK;` |
|       - | 3205 | `	}` |
|     118 | 3206 | `	bStrict = FALSE;` |
|     118 | 3207 | `	if( nArg > 2 ){` |
|       - | 3208 | `		/* In PHP, non-scalar values for a bool-hinted parameter raise TypeError */` |
|       8 | 3209 | `		if( ph7_value_is_array(apArg[2]) \|\| ph7_value_is_object(apArg[2]) \|\| ph7_value_is_resource(apArg[2]) ){` |
|       4 | 3210 | `			return PH7_VmThrowException(pCtx,` |
|       - | 3211 | `				"TypeError",` |
|       - | 3212 | `				"array_keys(): Argument #3 ($strict) must be of type bool, %s given",` |
|       2 | 3213 | `				ph7_type_name(apArg[2])` |
|       - | 3214 | `				);` |
|       - | 3215 | `		}` |
|       5 | 3216 | `		bStrict = ph7_value_to_bool(apArg[2]);` |
|       2 | 3217 | `	}` |
|       - | 3218 | `	/* Perform the requested operation */` |
|     115 | 3219 | `	pNode = pMap->pFirst;` |
|     115 | 3220 | `	PH7_MemObjInit(pMap->pVm,&sVal);` |
|     553 | 3221 | `	for( n = 0 ; n < pMap->nEntry ; ++n ){` |
|     439 | 3222 | `		if( pNode->iType == HASHMAP_INT_NODE ){` |
|     117 | 3223 | `			PH7_MemObjInitFromInt(pMap->pVm,&sObj,pNode->xKey.iKey);` |
|      59 | 3224 | `		}else{` |
|     323 | 3225 | `			SyStringInitFromBuf(&sKey,SyBlobData(&pNode->xKey.sKey),SyBlobLength(&pNode->xKey.sKey));` |
|     323 | 3226 | `			PH7_MemObjInitFromString(pMap->pVm,&sObj,&sKey);` |
|       - | 3227 | `		}` |
|     439 | 3228 | `		rc = 0;` |
|     439 | 3229 | `		if( nArg > 1 ){` |
|      31 | 3230 | `			ph7_value *pValue = HashmapExtractNodeValue(pNode);` |
|      31 | 3231 | `			if( pValue ){` |
|      31 | 3232 | `				PH7_MemObjLoad(pValue,&sVal);` |
|       - | 3233 | `				/* Filter key */` |
|      31 | 3234 | `				rc = ph7_value_compare(&sVal,apArg[1],bStrict);` |
|      31 | 3235 | `				PH7_MemObjRelease(&sVal);` |
|      15 | 3236 | `			}` |
|      15 | 3237 | `		}` |
|     439 | 3238 | `		if( rc == 0 ){` |
|       - | 3239 | `			/* Perform the insertion */` |
|     421 | 3240 | `			ph7_array_add_elem(pArray,0,&sObj);` |
|     210 | 3241 | `		}` |
|     439 | 3242 | `		PH7_MemObjRelease(&sObj);` |
|       - | 3243 | `		/* Point to the next entry */` |
|     439 | 3244 | `		pNode = pNode->pPrev; /* Reverse link */` |
|     220 | 3245 | `	}` |
|       - | 3246 | `	/* return the new array */` |
|     115 | 3247 | `	ph7_result_value(pCtx,pArray);` |
|     115 | 3248 | `	return PH7_OK;` |
|      62 | 3249 |  |
|       - | 3250 | `/*` |
|       - | 3251 | ` * bool array_same(array $arr1,array $arr2)` |
|       - | 3252 | ` *  Return TRUE if the given arrays are the same instance.` |
|       - | 3253 | ` *  This function is useful under PH7 since arrays are passed` |
|       - | 3254 | ` *  by reference unlike the zend engine which use pass by values.` |
|       - | 3255 | ` * Parameters` |
|       - | 3256 | ` *  $arr1` |
|       - | 3257 | ` *   First array` |
|       - | 3258 | ` *  $arr2` |
|       - | 3259 | ` *   Second array` |
|       - | 3260 | ` * Return` |
|       - | 3261 | ` *  TRUE if the arrays are the same instance.FALSE otherwise.` |
|       - | 3262 | ` * Note` |
|       - | 3263 | ` *  This function is a symisc eXtension.` |
|       - | 3264 | ` */` |
|       4 | 3265 | `static int ph7_hashmap_same(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       1 | 3266 |  |
|       - | 3267 | `	ph7_hashmap *p1,*p2;` |
|       - | 3268 | `	int rc;` |
|       5 | 3269 | `	if( nArg < 2 \|\| !ph7_value_is_array(apArg[0]) \|\| !ph7_value_is_array(apArg[1]) ){` |
|       - | 3270 | `		/* Missing or invalid arguments,return FALSE*/` |
|     ! 0 | 3271 | `		ph7_result_bool(pCtx,0);` |
|     ! 0 | 3272 | `		return PH7_OK;` |
|       - | 3273 | `	}` |
|       - | 3274 | `	/* Point to the hashmaps */` |
|       5 | 3275 | `	p1 = (ph7_hashmap *)apArg[0]->x.pOther;` |
|       5 | 3276 | `	p2 = (ph7_hashmap *)apArg[1]->x.pOther;` |
|       5 | 3277 | `	rc = (p1 == p2);` |
|       - | 3278 | `	/* Same instance? */` |
|       5 | 3279 | `	ph7_result_bool(pCtx,rc);` |
|       5 | 3280 | `	return PH7_OK;` |
|       3 | 3281 |  |
|       - | 3282 | `/*` |
|       - | 3283 | ` * array array_merge(array ...$arrays)` |
|       - | 3284 | ` *  Merge one or more arrays.` |
|       - | 3285 | ` * Parameters` |
|       - | 3286 | ` *  ...$arrays` |
|       - | 3287 | ` *   Variable list of arrays to merge. Each argument must be an array;` |
|       - | 3288 | ` *   passing a non-array argument throws a TypeError.` |
|       - | 3289 | ` * Return` |
|       - | 3290 | ` *  The resulting merged array. Returns an empty array when called` |
|       - | 3291 | ` *  with no arguments.` |
|       - | 3292 | ` */` |
|     856 | 3293 | `static int ph7_hashmap_merge(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       2 | 3294 |  |
|       - | 3295 | `	ph7_hashmap *pMap,*pSrc;` |
|       - | 3296 | `	ph7_value *pArray;` |
|       - | 3297 | `	int i;` |
|       - | 3298 | `	/* Create a new array */` |
|     858 | 3299 | `	pArray = ph7_context_new_array(pCtx);` |
|     858 | 3300 | `	if( pArray == 0 ){` |
|     ! 0 | 3301 | `		ph7_result_null(pCtx);` |
|     ! 0 | 3302 | `		return PH7_OK;` |
|       - | 3303 | `	}` |
|       - | 3304 | `	/* Point to the internal representation of the hashmap */` |
|     858 | 3305 | `	pMap = (ph7_hashmap *)pArray->x.pOther;` |
|       - | 3306 | `	/* Start merging */` |
|    2560 | 3307 | `	for( i = 0 ; i < nArg ; i++ ){` |
|       - | 3308 | `		/* Make sure we are dealing with a valid hashmap */` |
|    1708 | 3309 | `		if( !ph7_value_is_array(apArg[i]) ){` |
|       - | 3310 | `			/* Type mismatch -> TypeError */` |
|       7 | 3311 | `			return PH7_VmThrowException(pCtx,` |
|       - | 3312 | `				"TypeError",` |
|       - | 3313 | `				"array_merge(): Argument #%d must be of type array, %s given",` |
|       2 | 3314 | `				i + 1,` |
|       4 | 3315 | `				ph7_type_name(apArg[i])` |
|       - | 3316 | `				);` |
|     ! 0 | 3317 | `		}else{` |
|    1704 | 3318 | `			pSrc = (ph7_hashmap *)apArg[i]->x.pOther;` |
|       - | 3319 | `			/* Merge the two hashmaps */` |
|    1704 | 3320 | `			HashmapMerge(pSrc,pMap);` |
|       - | 3321 | `		}` |
|     853 | 3322 | `	}` |
|       - | 3323 | `	/* Return the freshly created array */` |
|     854 | 3324 | `	ph7_result_value(pCtx,pArray);` |
|     854 | 3325 | `	return PH7_OK;` |
|     430 | 3326 |  |
|       - | 3327 | `/*` |
|       - | 3328 | ` * array array_copy(array $source)` |
|       - | 3329 | ` *  Make a blind copy of the target array.` |
|       - | 3330 | ` * Parameters` |
|       - | 3331 | ` *  $source` |
|       - | 3332 | ` *   Target array` |
|       - | 3333 | ` * Return` |
|       - | 3334 | ` *  Copy of the target array on success.NULL otherwise.` |
|       - | 3335 | ` * Note` |
|       - | 3336 | ` *  This function is a symisc eXtension.` |
|       - | 3337 | ` */` |
|      16 | 3338 | `static int ph7_hashmap_copy(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       1 | 3339 |  |
|       - | 3340 | `	ph7_hashmap *pMap;` |
|       - | 3341 | `	ph7_value *pArray;` |
|      17 | 3342 | `	if( nArg < 1 ){` |
|       - | 3343 | `		/* Missing arguments,return NULL */` |
|     ! 0 | 3344 | `		ph7_result_null(pCtx);` |
|     ! 0 | 3345 | `		return PH7_OK;` |
|       - | 3346 | `	}` |
|       - | 3347 | `	/* Create a new array */` |
|      17 | 3348 | `	pArray = ph7_context_new_array(pCtx);` |
|      17 | 3349 | `	if( pArray == 0 ){` |
|     ! 0 | 3350 | `		ph7_result_null(pCtx);` |
|     ! 0 | 3351 | `		return PH7_OK;` |
|       - | 3352 | `	}` |
|       - | 3353 | `	/* Point to the internal representation of the hashmap */` |
|      17 | 3354 | `	pMap = (ph7_hashmap *)pArray->x.pOther;` |
|      17 | 3355 | `	if( ph7_value_is_array(apArg[0])){` |
|       - | 3356 | `		/* Point to the internal representation of the source */` |
|      17 | 3357 | `		ph7_hashmap *pSrc = (ph7_hashmap *)apArg[0]->x.pOther;` |
|       - | 3358 | `		/* Perform the copy */` |
|      17 | 3359 | `		PH7_HashmapDup(pSrc,pMap);` |
|       9 | 3360 | `	}else{` |
|       - | 3361 | `		/* Simple insertion */` |
|     ! 0 | 3362 | `		PH7_HashmapInsert(pMap,0/* Automatic index assign*/,apArg[0]);` |
|       - | 3363 | `	}` |
|       - | 3364 | `	/* Return the duplicated array */` |
|      17 | 3365 | `	ph7_result_value(pCtx,pArray);` |
|      17 | 3366 | `	return PH7_OK;` |
|       9 | 3367 |  |
|       - | 3368 | `/*` |
|       - | 3369 | ` * bool array_erase(array $source)` |
|       - | 3370 | ` *  Remove all elements from a given array.` |
|       - | 3371 | ` * Parameters` |
|       - | 3372 | ` *  $source` |
|       - | 3373 | ` *   Target array` |
|       - | 3374 | ` * Return` |
|       - | 3375 | ` *  TRUE on success.FALSE otherwise.` |
|       - | 3376 | ` * Note` |
|       - | 3377 | ` *  This function is a symisc eXtension.` |
|       - | 3378 | ` */` |
|      16 | 3379 | `static int ph7_hashmap_erase(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       1 | 3380 |  |
|       - | 3381 | `	ph7_hashmap *pMap;` |
|      17 | 3382 | `	if( nArg < 1 ){` |
|       - | 3383 | `		/* Missing arguments */` |
|     ! 0 | 3384 | `		ph7_result_bool(pCtx,0);` |
|     ! 0 | 3385 | `		return PH7_OK;` |
|       - | 3386 | `	}` |
|       - | 3387 | `	/* Point to the target hashmap */` |
|      17 | 3388 | `	pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|       - | 3389 | `	/* Erase */` |
|      17 | 3390 | `	PH7_HashmapRelease(pMap,FALSE);` |
|      17 | 3391 | `	return PH7_OK;` |
|       9 | 3392 |  |
|       - | 3393 | `/*` |
|       - | 3394 | ` * array array_slice(array $array, int $offset [, ?int $length = null [, bool $preserve_keys = false ]])` |
|       - | 3395 | ` *  Extract a slice of the array.` |
|       - | 3396 | ` * Parameters` |
|       - | 3397 | ` *  $array` |
|       - | 3398 | ` *    The input array.` |
|       - | 3399 | ` * $offset` |
|       - | 3400 | ` *    If offset is non-negative, the sequence will start at that offset in the array.` |
|       - | 3401 | ` *    If offset is negative, the sequence will start that far from the end of the array.` |
|       - | 3402 | ` * $length (optional, nullable)` |
|       - | 3403 | ` *    If length is given and is positive, then the sequence will have that many elements` |
|       - | 3404 | ` *    in it. If length is given and is negative then the sequence will stop that many` |
|       - | 3405 | ` *    elements from the end of the array. If it is omitted or NULL, then the sequence` |
|       - | 3406 | ` *    will have everything from offset up until the end of the array.` |
|       - | 3407 | ` * $preserve_keys (optional)` |
|       - | 3408 | ` *    Note that array_slice() will reorder and reset the array indices by default.` |
|       - | 3409 | ` *    You can change this behaviour by setting preserve_keys to TRUE.` |
|       - | 3410 | ` * Return` |
|       - | 3411 | ` *   The new slice.` |
|       - | 3412 | ` */` |
|      46 | 3413 | `static int ph7_hashmap_slice(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       2 | 3414 |  |
|       - | 3415 | `	ph7_hashmap *pMap,*pSrc;` |
|       - | 3416 | `	ph7_hashmap_node *pCur;` |
|       - | 3417 | `	ph7_value *pArray;` |
|       - | 3418 | `	int iLength,iOfft;` |
|       - | 3419 | `	int bPreserve;` |
|       - | 3420 | `	sxi32 rc;` |
|      48 | 3421 | `	if( nArg < 2 ){` |
|       7 | 3422 | `		return PH7_VmThrowException(pCtx,` |
|       - | 3423 | `			"ArgumentCountError",` |
|       - | 3424 | `			"array_slice() expects at least 2 arguments, %d given",` |
|       2 | 3425 | `			nArg` |
|       - | 3426 | `			);` |
|       - | 3427 | `	}` |
|      44 | 3428 | `	if( nArg > 4 ){` |
|       4 | 3429 | `		return PH7_VmThrowException(pCtx,` |
|       - | 3430 | `			"ArgumentCountError",` |
|       - | 3431 | `			"array_slice() expects at most 4 arguments, %d given",` |
|       1 | 3432 | `			nArg` |
|       - | 3433 | `			);` |
|       - | 3434 | `	}` |
|      42 | 3435 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|       4 | 3436 | `		return PH7_VmThrowException(pCtx,` |
|       - | 3437 | `			"TypeError",` |
|       - | 3438 | `			"array_slice(): Argument #1 ($array) must be of type array, %s given",` |
|       1 | 3439 | `			ph7_type_name(apArg[0])` |
|       - | 3440 | `			);` |
|       - | 3441 | `	}` |
|       - | 3442 | `	/* Validate $offset type: reject string, array, object, resource */` |
|      74 | 3443 | `	if( ph7_value_is_string(apArg[1]) \|\| ph7_value_is_array(apArg[1]) \|\|` |
|      56 | 3444 | `		ph7_value_is_object(apArg[1]) \|\| ph7_value_is_resource(apArg[1]) ){` |
|       4 | 3445 | `		return PH7_VmThrowException(pCtx,` |
|       - | 3446 | `			"TypeError",` |
|       - | 3447 | `			"array_slice(): Argument #2 ($offset) must be of type int, %s given",` |
|       2 | 3448 | `			ph7_type_name(apArg[1])` |
|       - | 3449 | `			);` |
|       - | 3450 | `	}` |
|       - | 3451 | `	/* Validate $length type if provided: nullable int */` |
|      38 | 3452 | `	if( nArg > 2 && !ph7_value_is_null(apArg[2]) ){` |
|      34 | 3453 | `		if( ph7_value_is_string(apArg[2]) \|\| ph7_value_is_array(apArg[2]) \|\|` |
|      26 | 3454 | `			ph7_value_is_object(apArg[2]) \|\| ph7_value_is_resource(apArg[2]) ){` |
|       4 | 3455 | `			return PH7_VmThrowException(pCtx,` |
|       - | 3456 | `				"TypeError",` |
|       - | 3457 | `				"array_slice(): Argument #3 ($length) must be of type ?int, %s given",` |
|       2 | 3458 | `				ph7_type_name(apArg[2])` |
|       - | 3459 | `				);` |
|       - | 3460 | `		}` |
|       8 | 3461 | `	}` |
|       - | 3462 | `	/* Validate $preserve_keys type if provided: reject array, object, resource */` |
|      36 | 3463 | `	if( nArg > 3 ){` |
|      10 | 3464 | `		if( ph7_value_is_array(apArg[3]) \|\| ph7_value_is_object(apArg[3]) \|\|` |
|       4 | 3465 | `			ph7_value_is_resource(apArg[3]) ){` |
|       4 | 3466 | `			return PH7_VmThrowException(pCtx,` |
|       - | 3467 | `				"TypeError",` |
|       - | 3468 | `				"array_slice(): Argument #4 ($preserve_keys) must be of type bool, %s given",` |
|       2 | 3469 | `				ph7_type_name(apArg[3])` |
|       - | 3470 | `				);` |
|       - | 3471 | `		}` |
|       2 | 3472 | `	}` |
|       - | 3473 | `	/* Point the internal representation of the target array */` |
|      33 | 3474 | `	pSrc = (ph7_hashmap *)apArg[0]->x.pOther;` |
|      33 | 3475 | `	bPreserve = FALSE;` |
|       - | 3476 | `	/* Get the offset */` |
|      33 | 3477 | `	iOfft = ph7_value_to_int(apArg[1]);` |
|      33 | 3478 | `	if( iOfft < 0 ){` |
|       5 | 3479 | `		iOfft = (int)pSrc->nEntry + iOfft;` |
|       5 | 3480 | `		if( iOfft < 0 ){` |
|       3 | 3481 | `			iOfft = 0;` |
|       1 | 3482 | `		}` |
|       2 | 3483 | `	}` |
|      33 | 3484 | `	if( iOfft >= (int)pSrc->nEntry ){` |
|       - | 3485 | `		/* Offset past end of array, return empty array */` |
|       5 | 3486 | `		pArray = ph7_context_new_array(pCtx);` |
|       5 | 3487 | `		if( pArray == 0 ){` |
|     ! 0 | 3488 | `			ph7_result_null(pCtx);` |
|     ! 0 | 3489 | `			return PH7_OK;` |
|       - | 3490 | `		}` |
|       5 | 3491 | `		ph7_result_value(pCtx,pArray);` |
|       5 | 3492 | `		return PH7_OK;` |
|       - | 3493 | `	}` |
|       - | 3494 | `	/* Get the length: NULL means "all remaining" (same as omitting) */` |
|      29 | 3495 | `	iLength = (int)pSrc->nEntry - iOfft;` |
|      29 | 3496 | `	if( nArg > 2 && !ph7_value_is_null(apArg[2]) ){` |
|      15 | 3497 | `		iLength = ph7_value_to_int(apArg[2]);` |
|      15 | 3498 | `		if( iLength < 0 ){` |
|       5 | 3499 | `			iLength = ((int)pSrc->nEntry + iLength) - iOfft;` |
|       2 | 3500 | `		}` |
|      15 | 3501 | `		if( iLength < 0 ){` |
|       3 | 3502 | `			iLength = 0;` |
|       1 | 3503 | `		}` |
|      15 | 3504 | `		if( iOfft + iLength > (int)pSrc->nEntry ){` |
|       3 | 3505 | `			iLength = (int)pSrc->nEntry - iOfft;` |
|       1 | 3506 | `		}` |
|       7 | 3507 | `	}` |
|      29 | 3508 | `	if( nArg > 3 ){` |
|       5 | 3509 | `		bPreserve = ph7_value_to_bool(apArg[3]);` |
|       2 | 3510 | `	}` |
|       - | 3511 | `	/* Create a new array */` |
|      29 | 3512 | `	pArray = ph7_context_new_array(pCtx);` |
|      29 | 3513 | `	if( pArray == 0 ){` |
|     ! 0 | 3514 | `		ph7_result_null(pCtx);` |
|     ! 0 | 3515 | `		return PH7_OK;` |
|       - | 3516 | `	}` |
|      29 | 3517 | `	if( iLength < 1 ){` |
|       - | 3518 | `		/* Don't bother processing,return the empty array */` |
|       5 | 3519 | `		ph7_result_value(pCtx,pArray);` |
|       5 | 3520 | `		return PH7_OK;` |
|       - | 3521 | `	}` |
|       - | 3522 | `	/* Point to the desired entry */` |
|      25 | 3523 | `	pCur = pSrc->pFirst;` |
|      24 | 3524 | `	for(;;){` |
|      49 | 3525 | `		if( iOfft < 1 ){` |
|      25 | 3526 | `			break;` |
|       - | 3527 | `		}` |
|       - | 3528 | `		/* Point to the next entry */` |
|      25 | 3529 | `		pCur = pCur->pPrev; /* Reverse link */` |
|      25 | 3530 | `		iOfft--;` |
|       1 | 3531 | `	}` |
|       - | 3532 | `	/* Point to the internal representation of the hashmap */` |
|      25 | 3533 | `	pMap = (ph7_hashmap *)pArray->x.pOther;` |
|      39 | 3534 | `	for(;;){` |
|      79 | 3535 | `		if( iLength < 1 ){` |
|      25 | 3536 | `			break;` |
|       - | 3537 | `		}` |
|       - | 3538 | `		/* String keys are always preserved; preserve_keys only affects int keys */` |
|       - | 3539 | `		{` |
|      55 | 3540 | `			int bKeep = (pCur->iType == HASHMAP_INT_NODE) ? bPreserve : TRUE;` |
|      55 | 3541 | `			rc = HashmapInsertNode(pMap,pCur,bKeep);` |
|       - | 3542 | `		}` |
|      55 | 3543 | `		if( rc != SXRET_OK ){` |
|     ! 0 | 3544 | `			break;` |
|       - | 3545 | `		}` |
|       - | 3546 | `		/* Point to the next entry */` |
|      55 | 3547 | `		pCur = pCur->pPrev; /* Reverse link */` |
|      55 | 3548 | `		iLength--;` |
|       1 | 3549 | `	}` |
|       - | 3550 | `	/* Return the freshly created array */` |
|      25 | 3551 | `	ph7_result_value(pCtx,pArray);` |
|      25 | 3552 | `	return PH7_OK;` |
|      25 | 3553 |  |
|       - | 3554 | `/*` |
|       - | 3555 | ` * array array_splice(array $array,int $offset [,int $length [,value $replacement ]])` |
|       - | 3556 | ` *  Remove a portion of the array and replace it with something else.` |
|       - | 3557 | ` * Parameters` |
|       - | 3558 | ` *  $array` |
|       - | 3559 | ` *    The input array.` |
|       - | 3560 | ` * $offset` |
|       - | 3561 | ` *    If offset is positive then the start of removed portion is at that offset from` |
|       - | 3562 | ` *    the beginning of the input array. If offset is negative then it starts that far` |
|       - | 3563 | ` *    from the end of the input array.` |
|       - | 3564 | ` * $length (optional)` |
|       - | 3565 | ` *    If length is omitted, removes everything from offset to the end of the array.` |
|       - | 3566 | ` *    If length is specified and is positive, then that many elements will be removed.` |
|       - | 3567 | ` *    If length is specified and is negative then the end of the removed portion will` |
|       - | 3568 | ` *    be that many elements from the end of the array.` |
|       - | 3569 | ` * $replacement (optional)` |
|       - | 3570 | ` *  If replacement array is specified, then the removed elements are replaced` |
|       - | 3571 | ` *  with elements from this array.` |
|       - | 3572 | ` *  If offset and length are such that nothing is removed, then the elements` |
|       - | 3573 | ` *  from the replacement array are inserted in the place specified by the offset.` |
|       - | 3574 | ` *  Note that keys in replacement array are not preserved.` |
|       - | 3575 | ` *  If replacement is just one element it is not necessary to put array() around` |
|       - | 3576 | ` *  it, unless the element is an array itself, an object or NULL.` |
|       - | 3577 | ` * Return` |
|       - | 3578 | ` *   A new array consisting of the extracted elements.` |
|       - | 3579 | ` */` |
|       2 | 3580 | `static int ph7_hashmap_splice(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       1 | 3581 |  |
|       - | 3582 | `	ph7_hashmap_node *pCur,*pPrev,*pRnode;` |
|       - | 3583 | `	ph7_value *pArray,*pRvalue,*pOld;` |
|       - | 3584 | `	ph7_hashmap *pMap,*pSrc,*pRep;` |
|       - | 3585 | `	int iLength,iOfft;` |
|       - | 3586 | `	sxi32 rc;` |
|       3 | 3587 | `	if( nArg < 2 \|\| !ph7_value_is_array(apArg[0]) ){` |
|       - | 3588 | `		/* Missing/Invalid arguments,return NULL */` |
|     ! 0 | 3589 | `		ph7_result_null(pCtx);` |
|     ! 0 | 3590 | `		return PH7_OK;` |
|       - | 3591 | `	}` |
|       - | 3592 | `	/* Point the internal representation of the target array */` |
|       3 | 3593 | `	pSrc = (ph7_hashmap *)apArg[0]->x.pOther;` |
|       - | 3594 | `	/* Get the offset */` |
|       3 | 3595 | `	iOfft = ph7_value_to_int(apArg[1]);` |
|       3 | 3596 | `	if( iOfft < 0 ){` |
|     ! 0 | 3597 | `		iOfft = (int)pSrc->nEntry + iOfft;` |
|     ! 0 | 3598 | `	}` |
|       3 | 3599 | `	if( iOfft < 0 \|\| iOfft > (int)pSrc->nEntry ){` |
|       - | 3600 | `		/* Invalid offset,remove the last entry */` |
|     ! 0 | 3601 | `		iOfft = (int)pSrc->nEntry - 1;` |
|     ! 0 | 3602 | `	}` |
|       - | 3603 | `	/* Get the length */` |
|       3 | 3604 | `	iLength = (int)pSrc->nEntry - iOfft;` |
|       3 | 3605 | `	if( nArg > 2 ){` |
|       3 | 3606 | `		iLength = ph7_value_to_int(apArg[2]);` |
|       3 | 3607 | `		if( iLength < 0 ){` |
|     ! 0 | 3608 | `			iLength = ((int)pSrc->nEntry + iLength) - iOfft;` |
|     ! 0 | 3609 | `		}` |
|       3 | 3610 | `		if( iLength < 0 \|\| iOfft + iLength >= (int)pSrc->nEntry ){` |
|     ! 0 | 3611 | `			iLength = (int)pSrc->nEntry - iOfft;` |
|     ! 0 | 3612 | `		}` |
|       1 | 3613 | `	}` |
|       - | 3614 | `	/* Create a new array */` |
|       3 | 3615 | `	pArray = ph7_context_new_array(pCtx);` |
|       3 | 3616 | `	if( pArray == 0 ){` |
|     ! 0 | 3617 | `		ph7_result_null(pCtx);` |
|     ! 0 | 3618 | `		return PH7_OK;` |
|       - | 3619 | `	}` |
|       3 | 3620 | `	if( iLength < 1 ){` |
|       - | 3621 | `		/* Don't bother processing,return the empty array */` |
|     ! 0 | 3622 | `		ph7_result_value(pCtx,pArray);` |
|     ! 0 | 3623 | `		return PH7_OK;` |
|       - | 3624 | `	}` |
|       - | 3625 | `	/* Point to the desired entry */` |
|       3 | 3626 | `	pCur = pSrc->pFirst;` |
|       2 | 3627 | `	for(;;){` |
|       5 | 3628 | `		if( iOfft < 1 ){` |
|       3 | 3629 | `			break;` |
|       - | 3630 | `		}` |
|       - | 3631 | `		/* Point to the next entry */` |
|       3 | 3632 | `		pCur = pCur->pPrev; /* Reverse link */` |
|       3 | 3633 | `		iOfft--;` |
|       1 | 3634 | `	}` |
|       3 | 3635 | `	pRep = 0;` |
|       3 | 3636 | `	if( nArg > 3 ){` |
|       3 | 3637 | `		if( !ph7_value_is_array(apArg[3]) ){` |
|       - | 3638 | `			/* Perform an array cast */` |
|     ! 0 | 3639 | `			PH7_MemObjToHashmap(apArg[3]);` |
|     ! 0 | 3640 | `			if(ph7_value_is_array(apArg[3])){` |
|     ! 0 | 3641 | `				pRep = (ph7_hashmap *)apArg[3]->x.pOther;` |
|     ! 0 | 3642 | `			}` |
|     ! 0 | 3643 | `		}else{` |
|       3 | 3644 | `			pRep = (ph7_hashmap *)apArg[3]->x.pOther;` |
|       - | 3645 | `		}` |
|       3 | 3646 | `		if( pRep ){` |
|       - | 3647 | `			/* Reset the loop cursor */` |
|       3 | 3648 | `			pRep->pCur = pRep->pFirst;` |
|       1 | 3649 | `		}` |
|       1 | 3650 | `	}` |
|       - | 3651 | `	/* Point to the internal representation of the hashmap */` |
|       3 | 3652 | `	pMap = (ph7_hashmap *)pArray->x.pOther;` |
|       3 | 3653 | `	for(;;){` |
|       7 | 3654 | `		if( iLength < 1 ){` |
|       3 | 3655 | `			break;` |
|       - | 3656 | `		}` |
|       5 | 3657 | `		pPrev = pCur->pPrev;` |
|       5 | 3658 | `		rc = HashmapInsertNode(pMap,pCur,FALSE);` |
|       5 | 3659 | `		if( pRep && (pRnode = PH7_HashmapGetNextEntry(pRep)) != 0 ){` |
|       - | 3660 | `			/* Extract node value */` |
|       5 | 3661 | `			pRvalue = HashmapExtractNodeValue(pRnode);` |
|       - | 3662 | `			/* Replace the old node */` |
|       5 | 3663 | `			pOld = HashmapExtractNodeValue(pCur);` |
|       5 | 3664 | `			if( pRvalue && pOld ){` |
|       5 | 3665 | `				PH7_MemObjStore(pRvalue,pOld);` |
|       2 | 3666 | `			}` |
|       3 | 3667 | `		}else{` |
|       - | 3668 | `			/* Unlink the node from the source hashmap */` |
|     ! 0 | 3669 | `			PH7_HashmapUnlinkNode(pCur,TRUE);` |
|       - | 3670 | `		}` |
|       5 | 3671 | `		if( rc != SXRET_OK ){` |
|     ! 0 | 3672 | `			break;` |
|       - | 3673 | `		}` |
|       - | 3674 | `		/* Point to the next entry */` |
|       5 | 3675 | `		pCur = pPrev; /* Reverse link */` |
|       5 | 3676 | `		iLength--;` |
|       1 | 3677 | `	}` |
|       3 | 3678 | `	if( pRep ){` |
|       3 | 3679 | `		while((pRnode = PH7_HashmapGetNextEntry(pRep)) != 0 ){` |
|     ! 0 | 3680 | `			HashmapInsertNode(pSrc,pRnode,FALSE);` |
|     ! 0 | 3681 | `		}` |
|       1 | 3682 | `	}` |
|       - | 3683 | `	/* Return the freshly created array */` |
|       3 | 3684 | `	ph7_result_value(pCtx,pArray);` |
|       3 | 3685 | `	return PH7_OK;` |
|       2 | 3686 |  |
|       - | 3687 | `/*` |
|       - | 3688 | ` * bool in_array(value $needle,array $haystack[,bool $strict = FALSE ])` |
|       - | 3689 | ` *  Checks if a value exists in an array.` |
|       - | 3690 | ` * Parameters` |
|       - | 3691 | ` *  $needle` |
|       - | 3692 | ` *   The searched value.` |
|       - | 3693 | ` *   Note:` |
|       - | 3694 | ` *    If needle is a string, the comparison is done in a case-sensitive manner.` |
|       - | 3695 | ` * $haystack` |
|       - | 3696 | ` *  The target array.` |
|       - | 3697 | ` * $strict` |
|       - | 3698 | ` *  If the third parameter strict is set to TRUE then the in_array() function` |
|       - | 3699 | ` *  will also check the types of the needle in the haystack.` |
|       - | 3700 | ` */` |
|   20102 | 3701 | `static int ph7_hashmap_in_array(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       2 | 3702 |  |
|       - | 3703 | `	ph7_value *pNeedle;` |
|       - | 3704 | `	int bStrict;` |
|       - | 3705 | `	int rc;` |
|   20104 | 3706 | `	if( nArg < 2 ){` |
|       - | 3707 | `		/* Missing argument,return FALSE */` |
|     ! 0 | 3708 | `		ph7_result_bool(pCtx,0);` |
|     ! 0 | 3709 | `		return PH7_OK;` |
|       - | 3710 | `	}` |
|   20104 | 3711 | `	pNeedle = apArg[0];` |
|   20104 | 3712 | `	bStrict = 0;` |
|   20104 | 3713 | `	if( nArg > 2 ){` |
|       5 | 3714 | `		bStrict = ph7_value_to_bool(apArg[2]);` |
|       2 | 3715 | `	}` |
|   20104 | 3716 | `	if( !ph7_value_is_array(apArg[1]) ){` |
|       - | 3717 | `		/* haystack must be an array,perform a standard comparison */` |
|     ! 0 | 3718 | `		rc = ph7_value_compare(pNeedle,apArg[1],bStrict);` |
|       - | 3719 | `		/* Set the comparison result */` |
|     ! 0 | 3720 | `		ph7_result_bool(pCtx,rc == 0);` |
|     ! 0 | 3721 | `		return PH7_OK;` |
|       - | 3722 | `	}` |
|       - | 3723 | `	/* Perform the lookup */` |
|   20104 | 3724 | `	rc = HashmapFindValue((ph7_hashmap *)apArg[1]->x.pOther,pNeedle,0,bStrict);` |
|       - | 3725 | `	/* Lookup result */` |
|   20104 | 3726 | `	ph7_result_bool(pCtx,rc == SXRET_OK);` |
|   20104 | 3727 | `	return PH7_OK;` |
|   10053 | 3728 |  |
|       - | 3729 | `/*` |
|       - | 3730 | ` * value array_search(value $needle,array $haystack[,bool $strict = false ])` |
|       - | 3731 | ` *  Searches the array for a given value and returns the corresponding key if successful.` |
|       - | 3732 | ` * Parameters` |
|       - | 3733 | ` * $needle` |
|       - | 3734 | ` *   The searched value.` |
|       - | 3735 | ` * $haystack` |
|       - | 3736 | ` *   The array.` |
|       - | 3737 | ` * $strict` |
|       - | 3738 | ` *  If the third parameter strict is set to TRUE then the array_search() function` |
|       - | 3739 | ` *  will search for identical elements in the haystack. This means it will also check` |
|       - | 3740 | ` *  the types of the needle in the haystack, and objects must be the same instance.` |
|       - | 3741 | ` * Return` |
|       - | 3742 | ` *  Returns the key for needle if it is found in the array, FALSE otherwise.` |
|       - | 3743 | ` */` |
|      28 | 3744 | `static int ph7_hashmap_search(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       2 | 3745 |  |
|       - | 3746 | `	ph7_hashmap_node *pEntry;` |
|       - | 3747 | `	ph7_value *pVal,sNeedle;` |
|       - | 3748 | `	ph7_hashmap *pMap;` |
|       - | 3749 | `	ph7_value sVal;` |
|       - | 3750 | `	int bStrict;` |
|       - | 3751 | `	sxu32 n;` |
|       - | 3752 | `	int rc;` |
|      30 | 3753 | `	if( nArg < 2 ){` |
|       - | 3754 | `		/* Missing argument,throw ArgumentCountError */` |
|       7 | 3755 | `		return PH7_VmThrowException(pCtx,` |
|       - | 3756 | `			"ArgumentCountError",` |
|       - | 3757 | `			"array_search() expects at least 2 arguments, %d given",` |
|       2 | 3758 | `			nArg` |
|       - | 3759 | `			);` |
|       - | 3760 | `	}` |
|      26 | 3761 | `	bStrict = FALSE;` |
|      26 | 3762 | `	if( !ph7_value_is_array(apArg[1]) ){` |
|       - | 3763 | `		/* haystack must be an array,throw TypeError */` |
|       4 | 3764 | `		return PH7_VmThrowException(pCtx,` |
|       - | 3765 | `			"TypeError",` |
|       - | 3766 | `			"array_search(): Argument #2 ($haystack) must be of type array, %s given",` |
|       2 | 3767 | `			ph7_type_name(apArg[1])` |
|       - | 3768 | `			);` |
|       - | 3769 | `	}` |
|      24 | 3770 | `	if( nArg > 2 ){` |
|       - | 3771 | `		/* In PHP, non-scalar values for a bool-hinted parameter raise TypeError */` |
|      12 | 3772 | `		if( ph7_value_is_array(apArg[2]) \|\| ph7_value_is_object(apArg[2]) \|\| ph7_value_is_resource(apArg[2]) ){` |
|       4 | 3773 | `			return PH7_VmThrowException(pCtx,` |
|       - | 3774 | `				"TypeError",` |
|       - | 3775 | `				"array_search(): Argument #3 ($strict) must be of type bool, %s given",` |
|       2 | 3776 | `				ph7_type_name(apArg[2])` |
|       - | 3777 | `				);` |
|       - | 3778 | `		}` |
|       9 | 3779 | `		bStrict = ph7_value_to_bool(apArg[2]);` |
|       4 | 3780 | `	}` |
|       - | 3781 | `	/* Point to the internal representation of the internal hashmap */` |
|      21 | 3782 | `	pMap = (ph7_hashmap *)apArg[1]->x.pOther;` |
|       - | 3783 | `	/* Perform a linear search since we cannot sort the hashmap based on values */` |
|      21 | 3784 | `	PH7_MemObjInit(pMap->pVm,&sVal);` |
|      21 | 3785 | `	PH7_MemObjInit(pMap->pVm,&sNeedle);` |
|      21 | 3786 | `	pEntry = pMap->pFirst;` |
|      21 | 3787 | `	n = pMap->nEntry;` |
|      23 | 3788 | `	for(;;){` |
|      47 | 3789 | `		if( !n ){` |
|       9 | 3790 | `			break;` |
|       - | 3791 | `		}` |
|       - | 3792 | `		/* Extract node value */` |
|      39 | 3793 | `		pVal = HashmapExtractNodeValue(pEntry);` |
|      39 | 3794 | `		if( pVal ){` |
|       - | 3795 | `			/* Make a copy of the vuurent values since the comparison routine` |
|       - | 3796 | `			 * can change their type.` |
|       - | 3797 | `			 */` |
|      39 | 3798 | `			PH7_MemObjLoad(pVal,&sVal);` |
|      39 | 3799 | `			PH7_MemObjLoad(apArg[0],&sNeedle);` |
|      39 | 3800 | `			rc = PH7_MemObjCmp(&sNeedle,&sVal,bStrict,0);` |
|      39 | 3801 | `			PH7_MemObjRelease(&sVal);` |
|      39 | 3802 | `			PH7_MemObjRelease(&sNeedle);` |
|      39 | 3803 | `			if( rc == 0 ){` |
|       - | 3804 | `				/* Match found,return key */` |
|      13 | 3805 | `				if( pEntry->iType == HASHMAP_INT_NODE){` |
|       - | 3806 | `					/* INT key */` |
|       7 | 3807 | `					ph7_result_int64(pCtx,pEntry->xKey.iKey);` |
|       4 | 3808 | `				}else{` |
|       7 | 3809 | `					SyBlob *pKey = &pEntry->xKey.sKey;` |
|       - | 3810 | `					/* Blob key */` |
|       7 | 3811 | `					ph7_result_string(pCtx,(const char *)SyBlobData(pKey),(int)SyBlobLength(pKey));` |
|       - | 3812 | `				}` |
|      13 | 3813 | `				return PH7_OK;` |
|       - | 3814 | `			}` |
|      13 | 3815 | `		}` |
|       - | 3816 | `		/* Point to the next entry */` |
|      27 | 3817 | `		pEntry = pEntry->pPrev; /* Reverse link */` |
|      27 | 3818 | `		n--;` |
|       1 | 3819 | `	}` |
|       - | 3820 | `	/* No such value,return FALSE */` |
|       9 | 3821 | `	ph7_result_bool(pCtx,0);` |
|       9 | 3822 | `	return PH7_OK;` |
|      16 | 3823 |  |
|       - | 3824 | `/*` |
|       - | 3825 | ` * array array_diff(array $array1,array $array2,...)` |
|       - | 3826 | ` *  Computes the difference of arrays.` |
|       - | 3827 | ` * Parameters` |
|       - | 3828 | ` *  $array1` |
|       - | 3829 | ` *    The array to compare from` |
|       - | 3830 | ` *  $array2` |
|       - | 3831 | ` *    An array to compare against` |
|       - | 3832 | ` *  $...` |
|       - | 3833 | ` *   More arrays to compare against` |
|       - | 3834 | ` * Return` |
|       - | 3835 | ` *  Returns an array containing all the entries from array1 that` |
|       - | 3836 | ` *  are not present in any of the other arrays.` |
|       - | 3837 | ` */` |
|      22 | 3838 | `static int ph7_hashmap_diff(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       2 | 3839 |  |
|       - | 3840 | `	ph7_hashmap_node *pEntry;` |
|       - | 3841 | `	ph7_hashmap *pSrc,*pMap;` |
|       - | 3842 | `	ph7_value *pArray;` |
|       - | 3843 | `	ph7_value *pVal;` |
|       - | 3844 | `	sxi32 rc;` |
|       - | 3845 | `	sxu32 n;` |
|       - | 3846 | `	int i;` |
|       - | 3847 | `	/* Validate arguments to mimic PHP behaviour. Earlier versions simply` |
|       - | 3848 | `	 * returned NULL when the caller passed invalid parameters which made` |
|       - | 3849 | `	 * debugging difficult. */` |
|      24 | 3850 | `	if( nArg < 1 ){` |
|       4 | 3851 | `		return PH7_VmThrowException(pCtx,` |
|       - | 3852 | `			"ArgumentCountError",` |
|       - | 3853 | `			"array_diff() expects at least 1 argument, %d given",` |
|       1 | 3854 | `			nArg` |
|       - | 3855 | `			);` |
|       - | 3856 | `	}` |
|      22 | 3857 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|       4 | 3858 | `		return PH7_VmThrowException(pCtx,` |
|       - | 3859 | `			"TypeError",` |
|       - | 3860 | `			"array_diff(): Argument #1 ($array) must be of type array, %s given",` |
|       1 | 3861 | `			ph7_type_name(apArg[0])` |
|       - | 3862 | `			);` |
|       - | 3863 | `	}` |
|      36 | 3864 | `	for(i = 1 ; i < nArg ; i++){` |
|      20 | 3865 | `		if( !ph7_value_is_array(apArg[i]) ){` |
|       4 | 3866 | `			return PH7_VmThrowException(pCtx,` |
|       - | 3867 | `				"TypeError",` |
|       - | 3868 | `				"array_diff(): Argument #%d must be of type array, %s given",` |
|       1 | 3869 | `				i + 1,` |
|       2 | 3870 | `				ph7_type_name(apArg[i])` |
|       - | 3871 | `				);` |
|       - | 3872 | `		}` |
|       9 | 3873 | `	}` |
|      17 | 3874 | `	if( nArg == 1 ){` |
|       - | 3875 | `		/* Return the first array since we cannot perform a diff */` |
|       3 | 3876 | `		ph7_result_value(pCtx,apArg[0]);` |
|       3 | 3877 | `		return PH7_OK;` |
|       - | 3878 | `	}` |
|       - | 3879 | `	/* Create a new array */` |
|      15 | 3880 | `	pArray = ph7_context_new_array(pCtx);` |
|      15 | 3881 | `	if( pArray == 0 ){` |
|     ! 0 | 3882 | `		ph7_result_null(pCtx);` |
|     ! 0 | 3883 | `		return PH7_OK;` |
|       - | 3884 | `	}` |
|       - | 3885 | `	/* Point to the internal representation of the source hashmap */` |
|      15 | 3886 | `	pSrc = (ph7_hashmap *)apArg[0]->x.pOther;` |
|       - | 3887 | `	/* Perform the diff */` |
|      15 | 3888 | `	pEntry = pSrc->pFirst;` |
|      15 | 3889 | `	n = pSrc->nEntry;` |
|      27 | 3890 | `	for(;;){` |
|      55 | 3891 | `		if( n < 1 ){` |
|      15 | 3892 | `			break;` |
|       - | 3893 | `		}` |
|       - | 3894 | `		/* Extract the node value */` |
|      41 | 3895 | `		pVal = HashmapExtractNodeValue(pEntry);` |
|      41 | 3896 | `		if( pVal ){` |
|      69 | 3897 | `			for( i = 1 ; i < nArg ; i++ ){` |
|       - | 3898 | `				/* Point to the internal representation of the hashmap */` |
|      45 | 3899 | `				pMap = (ph7_hashmap *)apArg[i]->x.pOther;` |
|       - | 3900 | `				/* Perform the lookup */` |
|      45 | 3901 | `				rc = HashmapFindValue(pMap,pVal,0,TRUE);` |
|      45 | 3902 | `				if( rc == SXRET_OK ){` |
|       - | 3903 | `					/* Value exist */` |
|      17 | 3904 | `					break;` |
|       - | 3905 | `				}` |
|      15 | 3906 | `			}` |
|      41 | 3907 | `			if( i >= nArg ){` |
|       - | 3908 | `				/* Perform the insertion */` |
|      25 | 3909 | `				HashmapInsertNode((ph7_hashmap *)pArray->x.pOther,pEntry,TRUE);` |
|      12 | 3910 | `			}` |
|      20 | 3911 | `		}` |
|       - | 3912 | `		/* Point to the next entry */` |
|      41 | 3913 | `		pEntry = pEntry->pPrev; /* Reverse link */` |
|      41 | 3914 | `		n--;` |
|       1 | 3915 | `	}` |
|       - | 3916 | `	/* Return the freshly created array */` |
|      15 | 3917 | `	ph7_result_value(pCtx,pArray);` |
|      15 | 3918 | `	return PH7_OK;` |
|      13 | 3919 |  |
|       - | 3920 | `/*` |
|       - | 3921 | ` * array array_udiff(array $array1,array $array2,...,$callback)` |
|       - | 3922 | ` *  Computes the difference of arrays by using a callback function for data comparison.` |
|       - | 3923 | ` * Parameters` |
|       - | 3924 | ` *  $array1` |
|       - | 3925 | ` *    The array to compare from` |
|       - | 3926 | ` *  $array2` |
|       - | 3927 | ` *    An array to compare against` |
|       - | 3928 | ` *  $...` |
|       - | 3929 | ` *   More arrays to compare against.` |
|       - | 3930 | ` * $callback` |
|       - | 3931 | ` *  The callback comparison function.` |
|       - | 3932 | ` *  The comparison function must return an integer less than, equal to, or greater than zero` |
|       - | 3933 | ` *  if the first argument is considered to be respectively less than, equal to, or greater` |
|       - | 3934 | ` *  than the second.` |
|       - | 3935 | ` *     int callback ( mixed $a, mixed $b )` |
|       - | 3936 | ` * Return` |
|       - | 3937 | ` *  Returns an array containing all the entries from array1 that` |
|       - | 3938 | ` *  are not present in any of the other arrays.` |
|       - | 3939 | ` */` |
|       2 | 3940 | `static int ph7_hashmap_udiff(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       1 | 3941 |  |
|       - | 3942 | `	ph7_hashmap_node *pEntry;` |
|       - | 3943 | `	ph7_hashmap *pSrc,*pMap;` |
|       - | 3944 | `	ph7_value *pCallback;` |
|       - | 3945 | `	ph7_value *pArray;` |
|       - | 3946 | `	ph7_value *pVal;` |
|       - | 3947 | `	sxi32 rc;` |
|       - | 3948 | `	sxu32 n;` |
|       - | 3949 | `	int i;` |
|       3 | 3950 | `	if( nArg < 2 \|\| !ph7_value_is_array(apArg[0]) ){` |
|       - | 3951 | `		/* Missing/Invalid arguments,return NULL */` |
|     ! 0 | 3952 | `		ph7_result_null(pCtx);` |
|     ! 0 | 3953 | `		return PH7_OK;` |
|       - | 3954 | `	}` |
|       - | 3955 | `	/* Point to the callback */` |
|       3 | 3956 | `	pCallback = apArg[nArg - 1];` |
|       3 | 3957 | `	if( nArg == 2 ){` |
|       - | 3958 | `		/* Return the first array since we cannot perform a diff */` |
|     ! 0 | 3959 | `		ph7_result_value(pCtx,apArg[0]);` |
|     ! 0 | 3960 | `		return PH7_OK;` |
|       - | 3961 | `	}` |
|       - | 3962 | `	/* Create a new array */` |
|       3 | 3963 | `	pArray = ph7_context_new_array(pCtx);` |
|       3 | 3964 | `	if( pArray == 0 ){` |
|     ! 0 | 3965 | `		ph7_result_null(pCtx);` |
|     ! 0 | 3966 | `		return PH7_OK;` |
|       - | 3967 | `	}` |
|       - | 3968 | `	/* Point to the internal representation of the source hashmap */` |
|       3 | 3969 | `	pSrc = (ph7_hashmap *)apArg[0]->x.pOther;` |
|       - | 3970 | `	/* Perform the diff */` |
|       3 | 3971 | `	pEntry = pSrc->pFirst;` |
|       3 | 3972 | `	n = pSrc->nEntry;` |
|       4 | 3973 | `	for(;;){` |
|       9 | 3974 | `		if( n < 1 ){` |
|       3 | 3975 | `			break;` |
|       - | 3976 | `		}` |
|       - | 3977 | `		/* Extract the node value */` |
|       7 | 3978 | `		pVal = HashmapExtractNodeValue(pEntry);` |
|       7 | 3979 | `		if( pVal ){` |
|      11 | 3980 | `			for( i = 1 ; i < nArg - 1; i++ ){` |
|       7 | 3981 | `				if( !ph7_value_is_array(apArg[i])) {` |
|       - | 3982 | `					/* ignore */` |
|     ! 0 | 3983 | `					continue;` |
|       - | 3984 | `				}` |
|       - | 3985 | `				/* Point to the internal representation of the hashmap */` |
|       7 | 3986 | `				pMap = (ph7_hashmap *)apArg[i]->x.pOther;` |
|       - | 3987 | `				/* Perform the lookup */` |
|       7 | 3988 | `				rc = HashmapFindValueByCallback(pMap,pVal,pCallback,0);` |
|       7 | 3989 | `				if( rc == SXRET_OK ){` |
|       - | 3990 | `					/* Value exist */` |
|       3 | 3991 | `					break;` |
|       - | 3992 | `				}` |
|       3 | 3993 | `			}` |
|       7 | 3994 | `			if( i >= (nArg - 1)){` |
|       - | 3995 | `				/* Perform the insertion */` |
|       5 | 3996 | `				HashmapInsertNode((ph7_hashmap *)pArray->x.pOther,pEntry,TRUE);` |
|       2 | 3997 | `			}` |
|       3 | 3998 | `		}` |
|       - | 3999 | `		/* Point to the next entry */` |
|       7 | 4000 | `		pEntry = pEntry->pPrev; /* Reverse link */` |
|       7 | 4001 | `		n--;` |
|       1 | 4002 | `	}` |
|       - | 4003 | `	/* Return the freshly created array */` |
|       3 | 4004 | `	ph7_result_value(pCtx,pArray);` |
|       3 | 4005 | `	return PH7_OK;` |
|       2 | 4006 |  |
|       - | 4007 | `/*` |
|       - | 4008 | ` * array array_diff_assoc(array $array1,array $array2,...)` |
|       - | 4009 | ` *  Computes the difference of arrays with additional index check.` |
|       - | 4010 | ` * Parameters` |
|       - | 4011 | ` *  $array1` |
|       - | 4012 | ` *    The array to compare from` |
|       - | 4013 | ` *  $array2` |
|       - | 4014 | ` *    An array to compare against` |
|       - | 4015 | ` *  $...` |
|       - | 4016 | ` *   More arrays to compare against` |
|       - | 4017 | ` * Return` |
|       - | 4018 | ` *  Returns an array containing all the entries from array1 that` |
|       - | 4019 | ` *  are not present in any of the other arrays.` |
|       - | 4020 | ` */` |
|      20 | 4021 | `static int ph7_hashmap_diff_assoc(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       2 | 4022 |  |
|       - | 4023 | `	ph7_hashmap_node *pN1,*pN2,*pEntry;` |
|       - | 4024 | `	ph7_hashmap *pSrc,*pMap;` |
|       - | 4025 | `	ph7_value *pArray;` |
|       - | 4026 | `	ph7_value *pVal;` |
|       - | 4027 | `	sxi32 rc;` |
|       - | 4028 | `	sxu32 n;` |
|       - | 4029 | `	int i;` |
|       - | 4030 | `	/* Ensure the argument list is valid, emitting the same errors PHP` |
|       - | 4031 | `	 * would produce. This makes behaviour predictable and allows the` |
|       - | 4032 | `	 * accompanying integration tests to pass. */` |
|      22 | 4033 | `	if( nArg < 1 ){` |
|       4 | 4034 | `		return PH7_VmThrowException(pCtx,` |
|       - | 4035 | `			"ArgumentCountError",` |
|       - | 4036 | `			"array_diff_assoc() expects at least 1 argument, %d given",` |
|       1 | 4037 | `			nArg` |
|       - | 4038 | `			);` |
|       - | 4039 | `	}` |
|      20 | 4040 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|       4 | 4041 | `		return PH7_VmThrowException(pCtx,` |
|       - | 4042 | `			"TypeError",` |
|       - | 4043 | `			"array_diff_assoc(): Argument #1 ($array) must be of type array, %s given",` |
|       1 | 4044 | `			ph7_type_name(apArg[0])` |
|       - | 4045 | `			);` |
|       - | 4046 | `	}` |
|      32 | 4047 | `	for(i = 1 ; i < nArg ; i++){` |
|      20 | 4048 | `		if( !ph7_value_is_array(apArg[i]) ){` |
|       7 | 4049 | `			return PH7_VmThrowException(pCtx,` |
|       - | 4050 | `				"TypeError",` |
|       - | 4051 | `				"array_diff_assoc(): Argument #%d must be of type array, %s given",` |
|       2 | 4052 | `				i + 1,` |
|       4 | 4053 | `				ph7_type_name(apArg[i])` |
|       - | 4054 | `				);` |
|       - | 4055 | `		}` |
|       9 | 4056 | `	}` |
|      13 | 4057 | `	if( nArg == 1 ){` |
|       - | 4058 | `		/* Return the first array since we cannot perform a diff */` |
|       3 | 4059 | `		ph7_result_value(pCtx,apArg[0]);` |
|       3 | 4060 | `		return PH7_OK;` |
|       - | 4061 | `	}` |
|       - | 4062 | `	/* Create a new array */` |
|      11 | 4063 | `	pArray = ph7_context_new_array(pCtx);` |
|      11 | 4064 | `	if( pArray == 0 ){` |
|     ! 0 | 4065 | `		ph7_result_null(pCtx);` |
|     ! 0 | 4066 | `		return PH7_OK;` |
|       - | 4067 | `	}` |
|       - | 4068 | `	/* Point to the internal representation of the source hashmap */` |
|      11 | 4069 | `	pSrc = (ph7_hashmap *)apArg[0]->x.pOther;` |
|       - | 4070 | `	/* Perform the diff */` |
|      11 | 4071 | `	pEntry = pSrc->pFirst;` |
|      11 | 4072 | `	n = pSrc->nEntry;` |
|      11 | 4073 | `	pN1 = pN2 = 0;` |
|      29 | 4074 | `	for(;;){` |
|       - | 4075 | `		int keep;` |
|      35 | 4076 | `		if( n < 1 ){` |
|      11 | 4077 | `			break;` |
|       - | 4078 | `		}` |
|       - | 4079 | `		/* assume the element should be kept until we find a match */` |
|      25 | 4080 | `		keep = 1;` |
|      41 | 4081 | `		for( i = 1 ; i < nArg ; i++ ){` |
|       - | 4082 | `			/* all arguments have been validated already, so cast directly */` |
|      29 | 4083 | `			pMap = (ph7_hashmap *)apArg[i]->x.pOther;` |
|       - | 4084 | `			/* Perform a key lookup first */` |
|      29 | 4085 | `			if( pEntry->iType == HASHMAP_INT_NODE ){` |
|      13 | 4086 | `				rc = HashmapLookupIntKey(pMap,pEntry->xKey.iKey,&pN1);` |
|       7 | 4087 | `			}else{` |
|      17 | 4088 | `				rc = HashmapLookupBlobKey(pMap,SyBlobData(&pEntry->xKey.sKey),SyBlobLength(&pEntry->xKey.sKey),&pN1);` |
|       - | 4089 | `			}` |
|      29 | 4090 | `			if( rc != SXRET_OK ){` |
|       - | 4091 | `				/* this array does not contain the key, continue checking others */` |
|      15 | 4092 | `				continue;` |
|       - | 4093 | `			}` |
|       - | 4094 | `			/* key exists; check that value stored in the matching node is equal */` |
|      15 | 4095 | `			pVal = HashmapExtractNodeValue(pEntry);` |
|      15 | 4096 | `			if( pVal ){` |
|       - | 4097 | `				/* directly compare with value at pN1 rather than searching again */` |
|      15 | 4098 | `				ph7_value *pVal2 = HashmapExtractNodeValue(pN1);` |
|      15 | 4099 | `				if( pVal2 ){` |
|      15 | 4100 | `					sxi32 cmp = PH7_MemObjCmp(pVal, pVal2, TRUE, 0);` |
|      15 | 4101 | `					if( cmp == 0 ){` |
|       - | 4102 | `						/* identical key+value found in one of the arrays => drop it */` |
|      13 | 4103 | `						keep = 0;` |
|      13 | 4104 | `						break;` |
|       - | 4105 | `					}` |
|       1 | 4106 | `				}` |
|       1 | 4107 | `			}` |
|       2 | 4108 | `		}` |
|      25 | 4109 | `		if( keep ){` |
|       - | 4110 | `			/* Perform the insertion */` |
|      13 | 4111 | `			HashmapInsertNode((ph7_hashmap *)pArray->x.pOther,pEntry,TRUE);` |
|       6 | 4112 | `		}` |
|       - | 4113 | `		/* Point to the next entry */` |
|      25 | 4114 | `		pEntry = pEntry->pPrev; /* Reverse link */` |
|      25 | 4115 | `		n--;` |
|       1 | 4116 | `	}` |
|       - | 4117 | `	/* Return the freshly created array */` |
|      11 | 4118 | `	ph7_result_value(pCtx,pArray);` |
|      11 | 4119 | `	return PH7_OK;` |
|      12 | 4120 |  |
|       - | 4121 | `/*` |
|       - | 4122 | ` * array array_diff_uassoc(array $array1,array $array2,...,callback $key_compare_func)` |
|       - | 4123 | ` *  Computes the difference of arrays with additional index check which is performed` |
|       - | 4124 | ` *  by a user supplied callback function.` |
|       - | 4125 | ` * Parameters` |
|       - | 4126 | ` *  $array1` |
|       - | 4127 | ` *    The array to compare from` |
|       - | 4128 | ` *  $array2` |
|       - | 4129 | ` *    An array to compare against` |
|       - | 4130 | ` *  $...` |
|       - | 4131 | ` *   More arrays to compare against.` |
|       - | 4132 | ` *  $key_compare_func` |
|       - | 4133 | ` *   Callback function to use. The callback function must return an integer` |
|       - | 4134 | ` *   less than, equal to, or greater than zero if the first argument is considered` |
|       - | 4135 | ` *   to be respectively less than, equal to, or greater than the second.` |
|       - | 4136 | ` * Return` |
|       - | 4137 | ` *  Returns an array containing all the entries from array1 that` |
|       - | 4138 | ` *  are not present in any of the other arrays.` |
|       - | 4139 | ` */` |
|      22 | 4140 | `static int ph7_hashmap_diff_uassoc(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       2 | 4141 |  |
|       - | 4142 | `	ph7_hashmap_node *pEntry;` |
|       - | 4143 | `	ph7_hashmap *pSrc,*pMap;` |
|       - | 4144 | `	ph7_value *pCallback;` |
|       - | 4145 | `	ph7_value *pArray;` |
|       - | 4146 | `	sxi32 rc;` |
|       - | 4147 | `	sxu32 n;` |
|       - | 4148 | `	int i;` |
|       - | 4149 |  |
|       - | 4150 | `	/* Argument validation mimicking PHP errors. */` |
|      24 | 4151 | `	if( nArg < 2 ){` |
|       4 | 4152 | `		return PH7_VmThrowException(pCtx,` |
|       - | 4153 | `			"ArgumentCountError",` |
|       - | 4154 | `			"array_diff_uassoc() expects at least 2 arguments, %d given",` |
|       1 | 4155 | `			nArg` |
|       - | 4156 | `			);` |
|       - | 4157 | `	}` |
|      22 | 4158 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|       4 | 4159 | `		return PH7_VmThrowException(pCtx,` |
|       - | 4160 | `			"TypeError",` |
|       - | 4161 | `			"array_diff_uassoc(): Argument #1 ($array) must be of type array, %s given",` |
|       1 | 4162 | `			ph7_type_name(apArg[0])` |
|       - | 4163 | `			);` |
|       - | 4164 | `	}` |
|       - | 4165 | `	/* Intermediate arguments (except last) must be arrays. Last argument is` |
|       - | 4166 | `	 * expected to be a callback. */` |
|      32 | 4167 | `	for(i = 1 ; i < nArg - 1; i++){` |
|      16 | 4168 | `		if( !ph7_value_is_array(apArg[i]) ){` |
|       4 | 4169 | `			return PH7_VmThrowException(pCtx,` |
|       - | 4170 | `				"TypeError",` |
|       - | 4171 | `				"array_diff_uassoc(): Argument #%d must be of type array, %s given",` |
|       1 | 4172 | `				i + 1,` |
|       2 | 4173 | `				ph7_type_name(apArg[i])` |
|       - | 4174 | `				);` |
|       - | 4175 | `		}` |
|       8 | 4176 | `	}` |
|       - | 4177 | `	/* Point to the callback value */` |
|      18 | 4178 | `	pCallback = apArg[nArg - 1];` |
|      18 | 4179 | `	if( !ph7_value_is_callable(pCallback) ){` |
|       - | 4180 | `		/* Compose an error message that closely matches PHP output. When the` |
|       - | 4181 | `		 * argument is an array of the wrong shape we include an extra clause.` |
|       - | 4182 | `		 * If the value is neither array nor string, PHP says "no array or` |
|       - | 4183 | `		 * string given" which we also reproduce. */` |
|       7 | 4184 | `		if( ph7_value_is_array(pCallback) ){` |
|       - | 4185 | `			/* ARRAY CALLBACK must have exactly two members */` |
|       4 | 4186 | `			return PH7_VmThrowException(pCtx,` |
|       - | 4187 | `				"TypeError",` |
|       - | 4188 | `				"array_diff_uassoc(): Argument #%d must be a valid callback, array callback must have exactly two members",` |
|       1 | 4189 | `				nArg` |
|       - | 4190 | `				);` |
|       - | 4191 | `		}` |
|       5 | 4192 | `		if( !ph7_value_is_string(pCallback) ){` |
|       - | 4193 | `			/* neither array nor string */` |
|       7 | 4194 | `			return PH7_VmThrowException(pCtx,` |
|       - | 4195 | `				"TypeError",` |
|       - | 4196 | `				"array_diff_uassoc(): Argument #%d must be a valid callback, no array or string given",` |
|       2 | 4197 | `				nArg` |
|       - | 4198 | `				);` |
|       - | 4199 | `		}` |
|       - | 4200 | `		/* Fallback for string (non-callable) or other leftover cases */` |
|     ! 0 | 4201 | `		return PH7_VmThrowException(pCtx,` |
|       - | 4202 | `			"TypeError",` |
|       - | 4203 | `			"array_diff_uassoc(): Argument #%d must be a valid callback, %s given",` |
|     ! 0 | 4204 | `			nArg,` |
|     ! 0 | 4205 | `			ph7_type_name(pCallback)` |
|       - | 4206 | `			);` |
|       - | 4207 | `	}` |
|      11 | 4208 | `	if( nArg == 2 ){` |
|       - | 4209 | `		/* If we only have the first array and the callback, just return the` |
|       - | 4210 | `		 * input array. */` |
|       3 | 4211 | `		ph7_result_value(pCtx,apArg[0]);` |
|       3 | 4212 | `		return PH7_OK;` |
|       - | 4213 | `	}` |
|       - | 4214 | `	/* Create a new array */` |
|       9 | 4215 | `	pArray = ph7_context_new_array(pCtx);` |
|       9 | 4216 | `	if( pArray == 0 ){` |
|     ! 0 | 4217 | `		ph7_result_null(pCtx);` |
|     ! 0 | 4218 | `		return PH7_OK;` |
|       - | 4219 | `	}` |
|       - | 4220 | `	/* Point to the internal representation of the source hashmap */` |
|       9 | 4221 | `	pSrc = (ph7_hashmap *)apArg[0]->x.pOther;` |
|       - | 4222 | `	/* Perform the diff */` |
|       9 | 4223 | `	pEntry = pSrc->pFirst;` |
|       9 | 4224 | `	n = pSrc->nEntry;` |
|      20 | 4225 | `	for(;;){` |
|       - | 4226 | `		int keep;` |
|      25 | 4227 | `		if( n < 1 ){` |
|       9 | 4228 | `			break;` |
|       - | 4229 | `		}` |
|      17 | 4230 | `		keep = 1;` |
|      29 | 4231 | `		for( i = 1 ; i < nArg - 1; i++ ){` |
|       - | 4232 | `			/* each of these must already be arrays thanks to earlier validation */` |
|      21 | 4233 | `			pMap = (ph7_hashmap *)apArg[i]->x.pOther;` |
|       - | 4234 | `			/* we must compare keys via callback, not by direct lookup */` |
|      21 | 4235 | `			ph7_hashmap_node *pIt = pMap->pFirst;` |
|      43 | 4236 | `			while( pIt ){` |
|       - | 4237 | `				/* build temporary key values for callback */` |
|       - | 4238 | `				ph7_value key1, key2, result;` |
|       - | 4239 | `				/* initialise only once using the appropriate helper */` |
|      31 | 4240 | `				if( pEntry->iType == HASHMAP_INT_NODE ){` |
|     ! 0 | 4241 | `					PH7_MemObjInitFromInt(pMap->pVm,&key1,pEntry->xKey.iKey);` |
|     ! 0 | 4242 | `				}else{` |
|       - | 4243 | `					SyString sStr;` |
|      31 | 4244 | `					SyStringInitFromBuf(&sStr,` |
|       - | 4245 | `						SyBlobData(&pEntry->xKey.sKey),` |
|       - | 4246 | `						SyBlobLength(&pEntry->xKey.sKey));` |
|      31 | 4247 | `					PH7_MemObjInitFromString(pMap->pVm,&key1,&sStr);` |
|       - | 4248 | `				}` |
|      31 | 4249 | `				if( pIt->iType == HASHMAP_INT_NODE ){` |
|     ! 0 | 4250 | `					PH7_MemObjInitFromInt(pMap->pVm,&key2,pIt->xKey.iKey);` |
|     ! 0 | 4251 | `				}else{` |
|       - | 4252 | `					SyString sStr;` |
|      31 | 4253 | `					SyStringInitFromBuf(&sStr,` |
|       - | 4254 | `						SyBlobData(&pIt->xKey.sKey),` |
|       - | 4255 | `						SyBlobLength(&pIt->xKey.sKey));` |
|      31 | 4256 | `					PH7_MemObjInitFromString(pMap->pVm,&key2,&sStr);` |
|       - | 4257 | `				}` |
|      31 | 4258 | `				PH7_MemObjInit(pMap->pVm,&result);` |
|       - | 4259 | `				/* call user callback with (key1, key2) */` |
|       - | 4260 | `				{` |
|       - | 4261 | `					ph7_value *apK[2];` |
|      31 | 4262 | `					apK[0] = &key1;` |
|      31 | 4263 | `					apK[1] = &key2;` |
|      31 | 4264 | `					rc = PH7_VmCallUserFunction(pMap->pVm,pCallback,2,apK,&result);` |
|       - | 4265 | `				}` |
|      31 | 4266 | `				if( rc == SXRET_OK ){` |
|      31 | 4267 | `					if( (result.iFlags & MEMOBJ_INT) == 0 ){` |
|     ! 0 | 4268 | `						PH7_MemObjToInteger(&result);` |
|     ! 0 | 4269 | `					}` |
|      31 | 4270 | `					if( result.x.iVal == 0 ){` |
|       - | 4271 | `						/* keys considered equal by callback; now compare values */` |
|      13 | 4272 | `						ph7_value *pVal1 = HashmapExtractNodeValue(pEntry);` |
|      13 | 4273 | `						ph7_value *pVal2 = HashmapExtractNodeValue(pIt);` |
|      13 | 4274 | `						if( pVal1 && pVal2 ){` |
|      13 | 4275 | `							if( PH7_MemObjCmp(pVal1,pVal2,TRUE,0) == 0 ){` |
|       9 | 4276 | `								keep = 0;` |
|       9 | 4277 | `								PH7_MemObjRelease(&result);` |
|       - | 4278 | `								/* release keys too before breaking */` |
|       9 | 4279 | `								PH7_MemObjRelease(&key1);` |
|       9 | 4280 | `								PH7_MemObjRelease(&key2);` |
|       9 | 4281 | `								break;` |
|       - | 4282 | `							}` |
|       2 | 4283 | `						}` |
|       2 | 4284 | `					}` |
|      11 | 4285 | `				}` |
|      23 | 4286 | `				PH7_MemObjRelease(&result);` |
|      23 | 4287 | `				PH7_MemObjRelease(&key1);` |
|      23 | 4288 | `				PH7_MemObjRelease(&key2);` |
|       - | 4289 | `				/* move to next node */` |
|      23 | 4290 | `				pIt = pIt->pPrev;` |
|      23 | 4291 | `				if( keep == 0 ) break;` |
|       1 | 4292 | `			}` |
|      21 | 4293 | `			if( keep == 0 ) break;` |
|       7 | 4294 | `		}` |
|      17 | 4295 | `		if( keep ){` |
|       - | 4296 | `			/* Perform the insertion */` |
|       9 | 4297 | `			HashmapInsertNode((ph7_hashmap *)pArray->x.pOther,pEntry,TRUE);` |
|       4 | 4298 | `		}` |
|       - | 4299 | `		/* Point to the next entry */` |
|      17 | 4300 | `		pEntry = pEntry->pPrev; /* Reverse link */` |
|      17 | 4301 | `		n--;` |
|       1 | 4302 | `	}` |
|       - | 4303 | `	/* Return the freshly created array */` |
|       9 | 4304 | `	ph7_result_value(pCtx,pArray);` |
|       9 | 4305 | `	return PH7_OK;` |
|      13 | 4306 |  |
|       - | 4307 | `/*` |
|       - | 4308 | ` * array array_diff_key(array $array1 ,array $array2,...)` |
|       - | 4309 | ` *  Computes the difference of arrays using keys for comparison.` |
|       - | 4310 | ` * Parameters` |
|       - | 4311 | ` *  $array1` |
|       - | 4312 | ` *    The array to compare from` |
|       - | 4313 | ` *  $array2` |
|       - | 4314 | ` *    An array to compare against` |
|       - | 4315 | ` *  $...` |
|       - | 4316 | ` *   More arrays to compare against` |
|       - | 4317 | ` * Return` |
|       - | 4318 | ` *  Returns an array containing all the entries from array1 whose keys are not present` |
|       - | 4319 | ` *  in any of the other arrays.` |
|       - | 4320 | ` * Note that NULL is returned on failure.` |
|       - | 4321 | ` */` |
|      14 | 4322 | `static int ph7_hashmap_diff_key(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       2 | 4323 |  |
|       - | 4324 | `	ph7_hashmap_node *pEntry;` |
|       - | 4325 | `	ph7_hashmap *pSrc,*pMap;` |
|       - | 4326 | `	ph7_value *pArray;` |
|       - | 4327 | `	sxi32 rc;` |
|       - | 4328 | `	sxu32 n;` |
|       - | 4329 | `	int i;` |
|       - | 4330 | `	/* Validate arguments to mirror PHP behaviour. Previously invalid inputs` |
|       - | 4331 | `	 * would quietly return NULL which is inconsistent with other hashmap` |
|       - | 4332 | `	 * helpers. */` |
|      16 | 4333 | `	if( nArg < 1 ){` |
|       4 | 4334 | `		return PH7_VmThrowException(pCtx,` |
|       - | 4335 | `			"ArgumentCountError",` |
|       - | 4336 | `			"array_diff_key() expects at least 1 argument, %d given",` |
|       1 | 4337 | `			nArg` |
|       - | 4338 | `			);` |
|       - | 4339 | `	}` |
|      14 | 4340 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|       4 | 4341 | `		return PH7_VmThrowException(pCtx,` |
|       - | 4342 | `			"TypeError",` |
|       - | 4343 | `			"array_diff_key(): Argument #1 ($array) must be of type array, %s given",` |
|       1 | 4344 | `			ph7_type_name(apArg[0])` |
|       - | 4345 | `			);` |
|       - | 4346 | `	}` |
|      20 | 4347 | `	for(i = 1 ; i < nArg ; i++){` |
|      12 | 4348 | `		if( !ph7_value_is_array(apArg[i]) ){` |
|       4 | 4349 | `			return PH7_VmThrowException(pCtx,` |
|       - | 4350 | `				"TypeError",` |
|       - | 4351 | `				"array_diff_key(): Argument #%d must be of type array, %s given",` |
|       1 | 4352 | `				i + 1,` |
|       2 | 4353 | `				ph7_type_name(apArg[i])` |
|       - | 4354 | `				);` |
|       - | 4355 | `		}` |
|       5 | 4356 | `	}` |
|       9 | 4357 | `	if( nArg == 1 ){` |
|       - | 4358 | `		/* Return the first array since we cannot perform a diff */` |
|       3 | 4359 | `		ph7_result_value(pCtx,apArg[0]);` |
|       3 | 4360 | `		return PH7_OK;` |
|       - | 4361 | `	}` |
|       - | 4362 | `	/* Create a new array */` |
|       7 | 4363 | `	pArray = ph7_context_new_array(pCtx);` |
|       7 | 4364 | `	if( pArray == 0 ){` |
|     ! 0 | 4365 | `		ph7_result_null(pCtx);` |
|     ! 0 | 4366 | `		return PH7_OK;` |
|       - | 4367 | `	}` |
|       - | 4368 | `	/* Point to the internal representation of the main hashmap */` |
|       7 | 4369 | `	pSrc = (ph7_hashmap *)apArg[0]->x.pOther;` |
|       - | 4370 | `	/* Perfrom the diff */` |
|       7 | 4371 | `	pEntry = pSrc->pFirst;` |
|       7 | 4372 | `	n = pSrc->nEntry;` |
|      12 | 4373 | `	for(;;){` |
|      25 | 4374 | `		if( n < 1 ){` |
|       7 | 4375 | `			break;` |
|       - | 4376 | `		}` |
|      31 | 4377 | `		for( i = 1 ; i < nArg ; i++ ){` |
|      23 | 4378 | `			if( !ph7_value_is_array(apArg[i])) {` |
|       - | 4379 | `				/* ignore */` |
|     ! 0 | 4380 | `				continue;` |
|       - | 4381 | `			}` |
|      23 | 4382 | `			pMap = (ph7_hashmap *)apArg[i]->x.pOther;` |
|      23 | 4383 | `			if( pEntry->iType == HASHMAP_BLOB_NODE ){` |
|      17 | 4384 | `				SyBlob *pKey = &pEntry->xKey.sKey;` |
|       - | 4385 | `				/* Blob lookup */` |
|      17 | 4386 | `				rc = HashmapLookupBlobKey(pMap,SyBlobData(pKey),SyBlobLength(pKey),0);` |
|       9 | 4387 | `			}else{` |
|       - | 4388 | `				/* Int lookup */` |
|       7 | 4389 | `				rc = HashmapLookupIntKey(pMap,pEntry->xKey.iKey,0);` |
|       - | 4390 | `			}` |
|      23 | 4391 | `			if( rc == SXRET_OK ){` |
|       - | 4392 | `				/* Key exists,break immediately */` |
|      11 | 4393 | `				break;` |
|       - | 4394 | `			}` |
|       7 | 4395 | `		}` |
|      19 | 4396 | `		if( i >= nArg ){` |
|       - | 4397 | `			/* Perform the insertion */` |
|       9 | 4398 | `			HashmapInsertNode((ph7_hashmap *)pArray->x.pOther,pEntry,TRUE);` |
|       4 | 4399 | `		}` |
|       - | 4400 | `		/* Point to the next entry */` |
|      19 | 4401 | `		pEntry = pEntry->pPrev; /* Reverse link */` |
|      19 | 4402 | `		n--;` |
|       1 | 4403 | `	}` |
|       - | 4404 | `	/* Return the freshly created array */` |
|       7 | 4405 | `	ph7_result_value(pCtx,pArray);` |
|       7 | 4406 | `	return PH7_OK;` |
|       9 | 4407 |  |
|       - | 4408 | `/*` |
|       - | 4409 | ` * array array_intersect(array $array1 ,array $array2,...)` |
|       - | 4410 | ` *  Computes the intersection of arrays.` |
|       - | 4411 | ` * Parameters` |
|       - | 4412 | ` *  $array1` |
|       - | 4413 | ` *    The array to compare from` |
|       - | 4414 | ` *  $array2` |
|       - | 4415 | ` *    An array to compare against` |
|       - | 4416 | ` *  $...` |
|       - | 4417 | ` *   More arrays to compare against` |
|       - | 4418 | ` * Return` |
|       - | 4419 | ` *  Returns an array containing all of the values in array1 whose values exist` |
|       - | 4420 | ` *  in all of the parameters.` |
|       - | 4421 | ` * Throws ArgumentCountError if no arguments are given.` |
|       - | 4422 | ` * Throws TypeError if any argument is not an array.` |
|       - | 4423 | ` */` |
|      22 | 4424 | `static int ph7_hashmap_intersect(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       2 | 4425 |  |
|       - | 4426 | `	ph7_hashmap_node *pEntry;` |
|       - | 4427 | `	ph7_hashmap *pSrc,*pMap;` |
|       - | 4428 | `	ph7_value *pArray;` |
|       - | 4429 | `	ph7_value *pVal;` |
|       - | 4430 | `	sxi32 rc;` |
|       - | 4431 | `	sxu32 n;` |
|       - | 4432 | `	int i;` |
|      24 | 4433 | `	if( nArg < 1 ){` |
|       4 | 4434 | `		return PH7_VmThrowException(pCtx,` |
|       - | 4435 | `			"ArgumentCountError",` |
|       - | 4436 | `			"array_intersect() expects at least 1 argument, %d given",` |
|       1 | 4437 | `			nArg` |
|       - | 4438 | `			);` |
|       - | 4439 | `	}` |
|      22 | 4440 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|       4 | 4441 | `		return PH7_VmThrowException(pCtx,` |
|       - | 4442 | `			"TypeError",` |
|       - | 4443 | `			"array_intersect(): Argument #1 ($array) must be of type array, %s given",` |
|       1 | 4444 | `			ph7_type_name(apArg[0])` |
|       - | 4445 | `			);` |
|       - | 4446 | `	}` |
|      36 | 4447 | `	for( i = 1 ; i < nArg ; i++ ){` |
|      20 | 4448 | `		if( !ph7_value_is_array(apArg[i]) ){` |
|       4 | 4449 | `			return PH7_VmThrowException(pCtx,` |
|       - | 4450 | `				"TypeError",` |
|       - | 4451 | `				"array_intersect(): Argument #%d must be of type array, %s given",` |
|       1 | 4452 | `				i + 1,` |
|       2 | 4453 | `				ph7_type_name(apArg[i])` |
|       - | 4454 | `				);` |
|       - | 4455 | `		}` |
|       9 | 4456 | `	}` |
|      17 | 4457 | `	if( nArg == 1 ){` |
|       - | 4458 | `		/* Return the first array since we cannot perform a diff */` |
|       3 | 4459 | `		ph7_result_value(pCtx,apArg[0]);` |
|       3 | 4460 | `		return PH7_OK;` |
|       - | 4461 | `	}` |
|       - | 4462 | `	/* Create a new array */` |
|      15 | 4463 | `	pArray = ph7_context_new_array(pCtx);` |
|      15 | 4464 | `	if( pArray == 0 ){` |
|     ! 0 | 4465 | `		ph7_result_null(pCtx);` |
|     ! 0 | 4466 | `		return PH7_OK;` |
|       - | 4467 | `	}` |
|       - | 4468 | `	/* Point to the internal representation of the source hashmap */` |
|      15 | 4469 | `	pSrc = (ph7_hashmap *)apArg[0]->x.pOther;` |
|       - | 4470 | `	/* Perform the intersection */` |
|      15 | 4471 | `	pEntry = pSrc->pFirst;` |
|      15 | 4472 | `	n = pSrc->nEntry;` |
|      31 | 4473 | `	for(;;){` |
|      63 | 4474 | `		if( n < 1 ){` |
|      15 | 4475 | `			break;` |
|       - | 4476 | `		}` |
|       - | 4477 | `		/* Extract the node value */` |
|      49 | 4478 | `		pVal = HashmapExtractNodeValue(pEntry);` |
|      49 | 4479 | `		if( pVal ){` |
|      79 | 4480 | `			for( i = 1 ; i < nArg ; i++ ){` |
|       - | 4481 | `				/* Point to the internal representation of the hashmap */` |
|      55 | 4482 | `				pMap = (ph7_hashmap *)apArg[i]->x.pOther;` |
|       - | 4483 | `				/* Perform the lookup */` |
|      55 | 4484 | `				rc = HashmapFindValue(pMap,pVal,0,TRUE);` |
|      55 | 4485 | `				if( rc != SXRET_OK ){` |
|       - | 4486 | `					/* Value does not exist */` |
|      25 | 4487 | `					break;` |
|       - | 4488 | `				}` |
|      16 | 4489 | `			}` |
|      49 | 4490 | `			if( i >= nArg ){` |
|       - | 4491 | `				/* Perform the insertion */` |
|      25 | 4492 | `				HashmapInsertNode((ph7_hashmap *)pArray->x.pOther,pEntry,TRUE);` |
|      12 | 4493 | `			}` |
|      24 | 4494 | `		}` |
|       - | 4495 | `		/* Point to the next entry */` |
|      49 | 4496 | `		pEntry = pEntry->pPrev; /* Reverse link */` |
|      49 | 4497 | `		n--;` |
|       1 | 4498 | `	}` |
|       - | 4499 | `	/* Return the freshly created array */` |
|      15 | 4500 | `	ph7_result_value(pCtx,pArray);` |
|      15 | 4501 | `	return PH7_OK;` |
|      13 | 4502 |  |
|       - | 4503 | `/*` |
|       - | 4504 | ` * array array_intersect_assoc(array $array1 ,array $array2,...)` |
|       - | 4505 | ` *  Computes the intersection of arrays with additional index check.` |
|       - | 4506 | ` * Parameters` |
|       - | 4507 | ` *  $array1` |
|       - | 4508 | ` *    The array to compare from` |
|       - | 4509 | ` *  $array2` |
|       - | 4510 | ` *    An array to compare against` |
|       - | 4511 | ` *  $...` |
|       - | 4512 | ` *   More arrays to compare against` |
|       - | 4513 | ` * Return` |
|       - | 4514 | ` *  Returns an array containing all the values of array1 that are present` |
|       - | 4515 | ` *  in all the arguments, with matching keys.` |
|       - | 4516 | ` */` |
|      22 | 4517 | `static int ph7_hashmap_intersect_assoc(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       2 | 4518 |  |
|       - | 4519 | `	ph7_hashmap_node *pEntry,*pN1,*pN2;` |
|       - | 4520 | `	ph7_hashmap *pSrc,*pMap;` |
|       - | 4521 | `	ph7_value *pArray;` |
|       - | 4522 | `	ph7_value *pVal;` |
|       - | 4523 | `	sxi32 rc;` |
|       - | 4524 | `	sxu32 n;` |
|       - | 4525 | `	int i;` |
|      24 | 4526 | `	if( nArg < 1 ){` |
|       4 | 4527 | `		return PH7_VmThrowException(pCtx,` |
|       - | 4528 | `			"ArgumentCountError",` |
|       - | 4529 | `			"array_intersect_assoc() expects at least 1 argument, %d given",` |
|       1 | 4530 | `			nArg` |
|       - | 4531 | `			);` |
|       - | 4532 | `	}` |
|      22 | 4533 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|       4 | 4534 | `		return PH7_VmThrowException(pCtx,` |
|       - | 4535 | `			"TypeError",` |
|       - | 4536 | `			"array_intersect_assoc(): Argument #1 ($array) must be of type array, %s given",` |
|       1 | 4537 | `			ph7_type_name(apArg[0])` |
|       - | 4538 | `			);` |
|       - | 4539 | `	}` |
|      36 | 4540 | `	for( i = 1 ; i < nArg ; i++ ){` |
|      20 | 4541 | `		if( !ph7_value_is_array(apArg[i]) ){` |
|       4 | 4542 | `			return PH7_VmThrowException(pCtx,` |
|       - | 4543 | `				"TypeError",` |
|       - | 4544 | `				"array_intersect_assoc(): Argument #%d must be of type array, %s given",` |
|       1 | 4545 | `				i + 1,` |
|       2 | 4546 | `				ph7_type_name(apArg[i])` |
|       - | 4547 | `				);` |
|       - | 4548 | `		}` |
|       9 | 4549 | `	}` |
|      17 | 4550 | `	if( nArg == 1 ){` |
|       - | 4551 | `		/* Return the first array since we cannot perform an intersection */` |
|       3 | 4552 | `		ph7_result_value(pCtx,apArg[0]);` |
|       3 | 4553 | `		return PH7_OK;` |
|       - | 4554 | `	}` |
|       - | 4555 | `	/* Create a new array */` |
|      15 | 4556 | `	pArray = ph7_context_new_array(pCtx);` |
|      15 | 4557 | `	if( pArray == 0 ){` |
|     ! 0 | 4558 | `		ph7_result_null(pCtx);` |
|     ! 0 | 4559 | `		return PH7_OK;` |
|       - | 4560 | `	}` |
|       - | 4561 | `	/* Point to the internal representation of the source hashmap */` |
|      15 | 4562 | `	pSrc = (ph7_hashmap *)apArg[0]->x.pOther;` |
|       - | 4563 | `	/* Perform the intersection */` |
|      15 | 4564 | `	pEntry = pSrc->pFirst;` |
|      15 | 4565 | `	n = pSrc->nEntry;` |
|      15 | 4566 | `	pN1 = pN2 = 0; /* cc warning */` |
|      23 | 4567 | `	for(;;){` |
|      47 | 4568 | `		if( n < 1 ){` |
|      15 | 4569 | `			break;` |
|       - | 4570 | `		}` |
|       - | 4571 | `		/* Extract the node value */` |
|      33 | 4572 | `		pVal = HashmapExtractNodeValue(pEntry);` |
|      33 | 4573 | `		if( pVal ){` |
|      53 | 4574 | `			for( i = 1 ; i < nArg ; i++ ){` |
|       - | 4575 | `				/* Point to the internal representation of the hashmap */` |
|      37 | 4576 | `				pMap = (ph7_hashmap *)apArg[i]->x.pOther;` |
|       - | 4577 | `				/* Perform a key lookup first */` |
|      37 | 4578 | `				if( pEntry->iType == HASHMAP_INT_NODE ){` |
|      15 | 4579 | `					rc = HashmapLookupIntKey(pMap,pEntry->xKey.iKey,&pN1);` |
|       8 | 4580 | `				}else{` |
|      23 | 4581 | `					rc = HashmapLookupBlobKey(pMap,SyBlobData(&pEntry->xKey.sKey),SyBlobLength(&pEntry->xKey.sKey),&pN1);` |
|       - | 4582 | `				}` |
|      37 | 4583 | `				if( rc != SXRET_OK ){` |
|       - | 4584 | `					/* No such key,break immediately */` |
|       7 | 4585 | `					break;` |
|       - | 4586 | `				}` |
|       - | 4587 | `				/* Perform the lookup */` |
|      31 | 4588 | `				rc = HashmapFindValue(pMap,pVal,&pN2,TRUE);` |
|      31 | 4589 | `				if( rc != SXRET_OK \|\| pN1 != pN2 ){` |
|       - | 4590 | `					/* Value does not exist */` |
|       6 | 4591 | `					break;` |
|       - | 4592 | `				}` |
|      11 | 4593 | `			}` |
|      33 | 4594 | `			if( i >= nArg ){` |
|       - | 4595 | `				/* Perform the insertion */` |
|      17 | 4596 | `				HashmapInsertNode((ph7_hashmap *)pArray->x.pOther,pEntry,TRUE);` |
|       8 | 4597 | `			}` |
|      16 | 4598 | `		}` |
|       - | 4599 | `		/* Point to the next entry */` |
|      33 | 4600 | `		pEntry = pEntry->pPrev; /* Reverse link */` |
|      33 | 4601 | `		n--;` |
|       1 | 4602 | `	}` |
|       - | 4603 | `	/* Return the freshly created array */` |
|      15 | 4604 | `	ph7_result_value(pCtx,pArray);` |
|      15 | 4605 | `	return PH7_OK;` |
|      13 | 4606 |  |
|       - | 4607 | `/*` |
|       - | 4608 | ` * array array_intersect_key(array $array1 ,...)` |
|       - | 4609 | ` *  Computes the intersection of arrays using keys for comparison.` |
|       - | 4610 | ` * Parameters` |
|       - | 4611 | ` *  $array1` |
|       - | 4612 | ` *    The array to compare from` |
|       - | 4613 | ` *  $...` |
|       - | 4614 | ` *   More arrays to compare against` |
|       - | 4615 | ` * Return` |
|       - | 4616 | ` *  Returns an associative array containing all the entries of array1 which` |
|       - | 4617 | ` *  have keys that are present in all arguments.` |
|       - | 4618 | ` * Note that NULL is returned on failure.` |
|       - | 4619 | ` */` |
|      22 | 4620 | `static int ph7_hashmap_intersect_key(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       2 | 4621 |  |
|       - | 4622 | `	ph7_hashmap_node *pEntry;` |
|       - | 4623 | `	ph7_hashmap *pSrc,*pMap;` |
|       - | 4624 | `	ph7_value *pArray;` |
|       - | 4625 | `	sxi32 rc;` |
|       - | 4626 | `	sxu32 n;` |
|       - | 4627 | `	int i;` |
|      24 | 4628 | `	if( nArg < 1 ){` |
|       4 | 4629 | `		return PH7_VmThrowException(pCtx,` |
|       - | 4630 | `			"ArgumentCountError",` |
|       - | 4631 | `			"array_intersect_key() expects at least 1 argument, %d given",` |
|       1 | 4632 | `			nArg` |
|       - | 4633 | `			);` |
|       - | 4634 | `	}` |
|      22 | 4635 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|       4 | 4636 | `		return PH7_VmThrowException(pCtx,` |
|       - | 4637 | `			"TypeError",` |
|       - | 4638 | `			"array_intersect_key(): Argument #1 ($array) must be of type array, %s given",` |
|       1 | 4639 | `			ph7_type_name(apArg[0])` |
|       - | 4640 | `			);` |
|       - | 4641 | `	}` |
|      36 | 4642 | `	for( i = 1 ; i < nArg ; i++ ){` |
|      20 | 4643 | `		if( !ph7_value_is_array(apArg[i]) ){` |
|       4 | 4644 | `			return PH7_VmThrowException(pCtx,` |
|       - | 4645 | `				"TypeError",` |
|       - | 4646 | `				"array_intersect_key(): Argument #%d must be of type array, %s given",` |
|       1 | 4647 | `				i + 1,` |
|       2 | 4648 | `				ph7_type_name(apArg[i])` |
|       - | 4649 | `				);` |
|       - | 4650 | `		}` |
|       9 | 4651 | `	}` |
|      17 | 4652 | `	if( nArg == 1 ){` |
|       - | 4653 | `		/* Return the first array since we cannot perform an intersection */` |
|       3 | 4654 | `		ph7_result_value(pCtx,apArg[0]);` |
|       3 | 4655 | `		return PH7_OK;` |
|       - | 4656 | `	}` |
|       - | 4657 | `	/* Create a new array */` |
|      15 | 4658 | `	pArray = ph7_context_new_array(pCtx);` |
|      15 | 4659 | `	if( pArray == 0 ){` |
|     ! 0 | 4660 | `		ph7_result_null(pCtx);` |
|     ! 0 | 4661 | `		return PH7_OK;` |
|       - | 4662 | `	}` |
|       - | 4663 | `	/* Point to the internal representation of the main hashmap */` |
|      15 | 4664 | `	pSrc = (ph7_hashmap *)apArg[0]->x.pOther;` |
|       - | 4665 | `	/* Perform the intersection */` |
|      15 | 4666 | `	pEntry = pSrc->pFirst;` |
|      15 | 4667 | `	n = pSrc->nEntry;` |
|      24 | 4668 | `	for(;;){` |
|      49 | 4669 | `		if( n < 1 ){` |
|      15 | 4670 | `			break;` |
|       - | 4671 | `		}` |
|      57 | 4672 | `		for( i = 1 ; i < nArg ; i++ ){` |
|      39 | 4673 | `			pMap = (ph7_hashmap *)apArg[i]->x.pOther;` |
|      39 | 4674 | `			if( pEntry->iType == HASHMAP_BLOB_NODE ){` |
|      27 | 4675 | `				SyBlob *pKey = &pEntry->xKey.sKey;` |
|       - | 4676 | `				/* Blob lookup */` |
|      27 | 4677 | `				rc = HashmapLookupBlobKey(pMap,SyBlobData(pKey),SyBlobLength(pKey),0);` |
|      14 | 4678 | `			}else{` |
|       - | 4679 | `				/* Int key */` |
|      13 | 4680 | `				rc = HashmapLookupIntKey(pMap,pEntry->xKey.iKey,0);` |
|       - | 4681 | `			}` |
|      39 | 4682 | `			if( rc != SXRET_OK ){` |
|       - | 4683 | `				/* Key does not exist, break immediately */` |
|      17 | 4684 | `				break;` |
|       - | 4685 | `			}` |
|      12 | 4686 | `		}` |
|      35 | 4687 | `		if( i >= nArg ){` |
|       - | 4688 | `			/* Perform the insertion */` |
|      19 | 4689 | `			HashmapInsertNode((ph7_hashmap *)pArray->x.pOther,pEntry,TRUE);` |
|       9 | 4690 | `		}` |
|       - | 4691 | `		/* Point to the next entry */` |
|      35 | 4692 | `		pEntry = pEntry->pPrev; /* Reverse link */` |
|      35 | 4693 | `		n--;` |
|       1 | 4694 | `	}` |
|       - | 4695 | `	/* Return the freshly created array */` |
|      15 | 4696 | `	ph7_result_value(pCtx,pArray);` |
|      15 | 4697 | `	return PH7_OK;` |
|      13 | 4698 |  |
|       - | 4699 | `/*` |
|       - | 4700 | ` * array array_uintersect(array $array1 ,array $array2,...,$callback)` |
|       - | 4701 | ` *  Computes the intersection of arrays.` |
|       - | 4702 | ` * Parameters` |
|       - | 4703 | ` *  $array1` |
|       - | 4704 | ` *    The array to compare from` |
|       - | 4705 | ` *  $array2` |
|       - | 4706 | ` *    An array to compare against` |
|       - | 4707 | ` *  $...` |
|       - | 4708 | ` *   More arrays to compare against` |
|       - | 4709 | ` * $callback` |
|       - | 4710 | ` *  The callback comparison function.` |
|       - | 4711 | ` *  The comparison function must return an integer less than, equal to, or greater than zero` |
|       - | 4712 | ` *  if the first argument is considered to be respectively less than, equal to, or greater` |
|       - | 4713 | ` *  than the second.` |
|       - | 4714 | ` *     int callback ( mixed $a, mixed $b )` |
|       - | 4715 | ` * Return` |
|       - | 4716 | ` *  Returns an array containing all of the values in array1 whose values exist` |
|       - | 4717 | ` *  in all of the parameters. .` |
|       - | 4718 | ` * Note that NULL is returned on failure.` |
|       - | 4719 | ` */` |
|       2 | 4720 | `static int ph7_hashmap_uintersect(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       1 | 4721 |  |
|       - | 4722 | `	ph7_hashmap_node *pEntry;` |
|       - | 4723 | `	ph7_hashmap *pSrc,*pMap;` |
|       - | 4724 | `	ph7_value *pCallback;` |
|       - | 4725 | `	ph7_value *pArray;` |
|       - | 4726 | `	ph7_value *pVal;` |
|       - | 4727 | `	sxi32 rc;` |
|       - | 4728 | `	sxu32 n;` |
|       - | 4729 | `	int i;` |
|       - | 4730 |  |
|       3 | 4731 | `	if( nArg < 2 \|\| !ph7_value_is_array(apArg[0]) ){` |
|       - | 4732 | `		/* Missing/Invalid arguments,return NULL */` |
|     ! 0 | 4733 | `		ph7_result_null(pCtx);` |
|     ! 0 | 4734 | `		return PH7_OK;` |
|       - | 4735 | `	}` |
|       - | 4736 | `	/* Point to the callback */` |
|       3 | 4737 | `	pCallback = apArg[nArg - 1];` |
|       3 | 4738 | `	if( nArg == 2 ){` |
|       - | 4739 | `		/* Return the first array since we cannot perform a diff */` |
|     ! 0 | 4740 | `		ph7_result_value(pCtx,apArg[0]);` |
|     ! 0 | 4741 | `		return PH7_OK;` |
|       - | 4742 | `	}` |
|       - | 4743 | `	/* Create a new array */` |
|       3 | 4744 | `	pArray = ph7_context_new_array(pCtx);` |
|       3 | 4745 | `	if( pArray == 0 ){` |
|     ! 0 | 4746 | `		ph7_result_null(pCtx);` |
|     ! 0 | 4747 | `		return PH7_OK;` |
|       - | 4748 | `	}` |
|       - | 4749 | `	/* Point to the internal representation of the source hashmap */` |
|       3 | 4750 | `	pSrc = (ph7_hashmap *)apArg[0]->x.pOther;` |
|       - | 4751 | `	/* Perform the intersection */` |
|       3 | 4752 | `	pEntry = pSrc->pFirst;` |
|       3 | 4753 | `	n = pSrc->nEntry;` |
|       4 | 4754 | `	for(;;){` |
|       9 | 4755 | `		if( n < 1 ){` |
|       3 | 4756 | `			break;` |
|       - | 4757 | `		}` |
|       - | 4758 | `		/* Extract the node value */` |
|       7 | 4759 | `		pVal = HashmapExtractNodeValue(pEntry);` |
|       7 | 4760 | `		if( pVal ){` |
|      11 | 4761 | `			for( i = 1 ; i < nArg - 1; i++ ){` |
|       7 | 4762 | `				if( !ph7_value_is_array(apArg[i])) {` |
|       - | 4763 | `					/* ignore */` |
|     ! 0 | 4764 | `					continue;` |
|       - | 4765 | `				}` |
|       - | 4766 | `				/* Point to the internal representation of the hashmap */` |
|       7 | 4767 | `				pMap = (ph7_hashmap *)apArg[i]->x.pOther;` |
|       - | 4768 | `				/* Perform the lookup */` |
|       7 | 4769 | `				rc = HashmapFindValueByCallback(pMap,pVal,pCallback,0);` |
|       7 | 4770 | `				if( rc != SXRET_OK ){` |
|       - | 4771 | `					/* Value does not exist */` |
|       3 | 4772 | `					break;` |
|       - | 4773 | `				}` |
|       3 | 4774 | `			}` |
|       7 | 4775 | `			if( i >= (nArg-1) ){` |
|       - | 4776 | `				/* Perform the insertion */` |
|       5 | 4777 | `				HashmapInsertNode((ph7_hashmap *)pArray->x.pOther,pEntry,TRUE);` |
|       2 | 4778 | `			}` |
|       3 | 4779 | `		}` |
|       - | 4780 | `		/* Point to the next entry */` |
|       7 | 4781 | `		pEntry = pEntry->pPrev; /* Reverse link */` |
|       7 | 4782 | `		n--;` |
|       1 | 4783 | `	}` |
|       - | 4784 | `	/* Return the freshly created array */` |
|       3 | 4785 | `	ph7_result_value(pCtx,pArray);` |
|       3 | 4786 | `	return PH7_OK;` |
|       2 | 4787 |  |
|       - | 4788 | `/*` |
|       - | 4789 | ` * array array_fill(int $start_index,int $num,var $value)` |
|       - | 4790 | ` *  Fill an array with values.` |
|       - | 4791 | ` * Parameters` |
|       - | 4792 | ` *  $start_index` |
|       - | 4793 | ` *    The first index of the returned array.` |
|       - | 4794 | ` *  $num` |
|       - | 4795 | ` *   Number of elements to insert.` |
|       - | 4796 | ` *  $value` |
|       - | 4797 | ` *    Value to use for filling.` |
|       - | 4798 | ` * Return` |
|       - | 4799 | ` *  The filled array or null on failure.` |
|       - | 4800 | ` */` |
|     238 | 4801 | `static int ph7_hashmap_fill(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       2 | 4802 |  |
|       - | 4803 | `	ph7_value *pArray;` |
|       - | 4804 | `	int i,nEntry;` |
|       - | 4805 |  |
|       - | 4806 | `	/* PHP enforces argument count and type checks. */` |
|     240 | 4807 | `	if( nArg != 3 ){` |
|       - | 4808 | `		/* wrong number of arguments -> ArgumentCountError */` |
|       7 | 4809 | `		return PH7_VmThrowException(pCtx,` |
|       - | 4810 | `			"ArgumentCountError",` |
|       - | 4811 | `			"array_fill() expects exactly 3 arguments, %d given",` |
|       2 | 4812 | `			nArg` |
|       - | 4813 | `			);` |
|       - | 4814 | `	}` |
|       - | 4815 |  |
|       - | 4816 | `	/* Argument #1: start index must be convertible to int.  Accept booleans,` |
|       - | 4817 | `	 * floats, and numeric strings (including those with decimal point) by` |
|       - | 4818 | `	 * allowing them through the conversion.  Only arrays, objects, resources` |
|       - | 4819 | `	 * and NULLs are rejected outright. */` |
|     466 | 4820 | `	if( ph7_value_is_array(apArg[0]) \|\| ph7_value_is_object(apArg[0]) \|\|` |
|     350 | 4821 | `		ph7_value_is_resource(apArg[0]) \|\| ph7_value_is_null(apArg[0]) ){` |
|       4 | 4822 | `		return PH7_VmThrowException(pCtx,` |
|       - | 4823 | `			"TypeError",` |
|       - | 4824 | `			"array_fill(): Argument #1 ($start_index) must be of type int, %s given",` |
|       1 | 4825 | `			ph7_type_name(apArg[0])` |
|       - | 4826 | `			);` |
|       - | 4827 | `	}` |
|     234 | 4828 | `	if( ph7_value_is_string(apArg[0]) ){` |
|       - | 4829 | `		int len;` |
|       8 | 4830 | `		sxu8 bReal = FALSE;` |
|       8 | 4831 | `		const char *zStr = ph7_value_to_string(apArg[0], &len);` |
|       8 | 4832 | `		if( SyStrIsNumeric(zStr, len, &bReal, 0) != SXRET_OK ){` |
|       - | 4833 | `			/* Non‑numeric string is an error. */` |
|       3 | 4834 | `			return PH7_VmThrowException(pCtx,` |
|       - | 4835 | `				"TypeError",` |
|       - | 4836 | `				"array_fill(): Argument #1 ($start_index) must be of type int, string given"` |
|       - | 4837 | `				);` |
|       - | 4838 | `		}` |
|       5 | 4839 | `		if( bReal ){` |
|       - | 4840 | `			/* float-string -> deprecation warning */` |
|       4 | 4841 | `			ph7_context_throw_error_format(pCtx, E_DEPRECATED,` |
|       - | 4842 | `				"Implicit conversion from float-string \"%s\" to int loses precision",` |
|       1 | 4843 | `				zStr` |
|       - | 4844 | `				);` |
|       1 | 4845 | `		}` |
|       2 | 4846 | `	}` |
|       - | 4847 |  |
|       - | 4848 | `	/* Argument #2: count must be convertible to non-negative int.  Allow booleans,` |
|       - | 4849 | `	 * floats and numeric strings; reject arrays, objects, resources and NULL. */` |
|     460 | 4850 | `	if( ph7_value_is_array(apArg[1]) \|\| ph7_value_is_object(apArg[1]) \|\|` |
|     347 | 4851 | `		ph7_value_is_resource(apArg[1]) \|\| ph7_value_is_null(apArg[1]) ){` |
|     ! 0 | 4852 | `		return PH7_VmThrowException(pCtx,` |
|       - | 4853 | `			"TypeError",` |
|       - | 4854 | `			"array_fill(): Argument #2 ($count) must be of type int, %s given",` |
|     ! 0 | 4855 | `			ph7_type_name(apArg[1])` |
|       - | 4856 | `			);` |
|       - | 4857 | `	}` |
|     232 | 4858 | `	if( ph7_value_is_string(apArg[1]) ){` |
|       - | 4859 | `		int len;` |
|       3 | 4860 | `		sxu8 bReal = FALSE;` |
|       3 | 4861 | `		const char *zStr = ph7_value_to_string(apArg[1], &len);` |
|       3 | 4862 | `		if( SyStrIsNumeric(zStr, len, &bReal, 0) != SXRET_OK ){` |
|       3 | 4863 | `			return PH7_VmThrowException(pCtx,` |
|       - | 4864 | `				"TypeError",` |
|       - | 4865 | `				"array_fill(): Argument #2 ($count) must be of type int, string given"` |
|       - | 4866 | `				);` |
|       - | 4867 | `		}` |
|     ! 0 | 4868 | `	}` |
|       - | 4869 | `	/* Note: booleans and floats (including fractional) are now accepted; they` |
|       - | 4870 | `	 * will be converted by ph7_value_to_int below. */` |
|     230 | 4871 | `	if( ph7_value_is_float(apArg[1]) ){` |
|       5 | 4872 | `		double d = ph7_value_to_double(apArg[1]);` |
|       - | 4873 | `		/* avoid hiding outer 'i' (loop index) */` |
|       5 | 4874 | `		sxi64 i64 = (sxi64)d;` |
|       5 | 4875 | `		if( d != (double)i64 ){` |
|       7 | 4876 | `			ph7_context_throw_error_format(pCtx, E_DEPRECATED,` |
|       - | 4877 | `				"Implicit conversion from float %g to int loses precision",` |
|       2 | 4878 | `				d` |
|       - | 4879 | `				);` |
|       2 | 4880 | `		}` |
|       2 | 4881 | `	}` |
|       - | 4882 |  |
|       - | 4883 | `	/* Total number of entries to insert */` |
|     230 | 4884 | `	nEntry = ph7_value_to_int(apArg[1]);` |
|       - | 4885 | `	/* Reject negative counts with a ValueError like PHP. */` |
|     230 | 4886 | `	if( nEntry < 0 ){` |
|       3 | 4887 | `		return PH7_VmThrowException(pCtx,` |
|       - | 4888 | `			"ValueError",` |
|       - | 4889 | `			"array_fill(): Argument #2 ($count) must be greater than or equal to 0"` |
|       - | 4890 | `			);` |
|       - | 4891 | `	}` |
|       - | 4892 |  |
|       - | 4893 | `	/* If zero elements were requested, return an empty array without allocating */` |
|     227 | 4894 | `	if( nEntry == 0 ){` |
|       7 | 4895 | `		ph7_result_value(pCtx, ph7_context_new_array(pCtx));` |
|       7 | 4896 | `		return PH7_OK;` |
|       - | 4897 | `	}` |
|       - | 4898 |  |
|       - | 4899 | `	/* Create a new array */` |
|     221 | 4900 | `	pArray = ph7_context_new_array(pCtx);` |
|     221 | 4901 | `	if( pArray == 0 ){` |
|     ! 0 | 4902 | `		ph7_result_null(pCtx);` |
|     ! 0 | 4903 | `		return PH7_OK;` |
|       - | 4904 | `	}` |
|       - | 4905 |  |
|       - | 4906 | `	/* Insert the first entry alone because it has its own key */` |
|     221 | 4907 | `	ph7_array_add_intkey_elem(pArray, ph7_value_to_int(apArg[0]), apArg[2]);` |
|       - | 4908 | `	/* Repeat insertion of the desired value */` |
| 2117583 | 4909 | `	for( i = 1 ; i < nEntry ; i++ ){` |
| 2117363 | 4910 | `		ph7_array_add_elem(pArray, 0/*Automatic index assign */, apArg[2]);` |
| 1058682 | 4911 | `	}` |
|       - | 4912 | `	/* Return the filled array */` |
|     221 | 4913 | `	ph7_result_value(pCtx, pArray);` |
|     221 | 4914 | `	return PH7_OK;` |
|     121 | 4915 |  |
|       - | 4916 | `/*` |
|       - | 4917 | ` * array array_fill_keys(array $input,mixed $value)` |
|       - | 4918 | ` *  Fill an array with values, specifying keys.` |
|       - | 4919 | ` * Parameters` |
|       - | 4920 | ` *  $input` |
|       - | 4921 | ` *   Array of values that will be used as key.` |
|       - | 4922 | ` *  $value` |
|       - | 4923 | ` *    Value to use for filling.` |
|       - | 4924 | ` * Return` |
|       - | 4925 | ` *  The filled array.` |
|       - | 4926 | ` * Throws` |
|       - | 4927 | ` *  ValueError if $input is not an array.` |
|       - | 4928 | ` */` |
|      26 | 4929 | `static int ph7_hashmap_fill_keys(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       2 | 4930 |  |
|       - | 4931 | `	ph7_hashmap_node *pEntry;` |
|       - | 4932 | `	ph7_hashmap *pSrc;` |
|       - | 4933 | `	ph7_value *pArray;` |
|       - | 4934 | `	sxu32 n;` |
|       - | 4935 | `	/* PHP enforces exactly 2 arguments. */` |
|      28 | 4936 | `	if( nArg != 2 ){` |
|      10 | 4937 | `		return PH7_VmThrowException(pCtx,` |
|       - | 4938 | `			"ArgumentCountError",` |
|       - | 4939 | `			"array_fill_keys() expects exactly 2 arguments, %d given",` |
|       3 | 4940 | `			nArg` |
|       - | 4941 | `			);` |
|       - | 4942 | `	}` |
|       - | 4943 | `	/* Make sure we are dealing with a valid hashmap */` |
|      22 | 4944 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|       7 | 4945 | `		return PH7_VmThrowException(pCtx,` |
|       - | 4946 | `			"TypeError",` |
|       - | 4947 | `			"array_fill_keys(): Argument #1 ($keys) must be of type array, %s given",` |
|       2 | 4948 | `			ph7_type_name(apArg[0])` |
|       - | 4949 | `			);` |
|       - | 4950 | `	}` |
|       - | 4951 | `	/* Point to the internal representation of the input hashmap */` |
|      17 | 4952 | `	pSrc = (ph7_hashmap *)apArg[0]->x.pOther;` |
|       - | 4953 | `	/* Create a new array */` |
|      17 | 4954 | `	pArray = ph7_context_new_array(pCtx);` |
|      17 | 4955 | `	if( pArray == 0 ){` |
|     ! 0 | 4956 | `		ph7_result_null(pCtx);` |
|     ! 0 | 4957 | `		return PH7_OK;` |
|       - | 4958 | `	}` |
|       - | 4959 | `	/* Perform the requested operation */` |
|      17 | 4960 | `	pEntry = pSrc->pFirst;` |
|      45 | 4961 | `	for( n = 0 ; n < pSrc->nEntry ; n++ ){` |
|      29 | 4962 | `		ph7_array_add_elem(pArray,HashmapExtractNodeValue(pEntry),apArg[1]);` |
|       - | 4963 | `		/* Point to the next entry */` |
|      29 | 4964 | `		pEntry = pEntry->pPrev; /* Reverse link */` |
|      15 | 4965 | `	}` |
|       - | 4966 | `	/* Return the filled array */` |
|      17 | 4967 | `	ph7_result_value(pCtx,pArray);` |
|      17 | 4968 | `	return PH7_OK;` |
|      15 | 4969 |  |
|       - | 4970 | `/*` |
|       - | 4971 | ` * array array_combine(array $keys,array $values)` |
|       - | 4972 | ` *  Creates an array by using one array for keys and another for its values.` |
|       - | 4973 | ` * Parameters` |
|       - | 4974 | ` *  $keys` |
|       - | 4975 | ` *    Array of keys to be used.` |
|       - | 4976 | ` * $values` |
|       - | 4977 | ` *   Array of values to be used.` |
|       - | 4978 | ` * Return` |
|       - | 4979 | ` *  Returns the combined array. Otherwise FALSE if the number of elements` |
|       - | 4980 | ` *  for each array isn't equal or if one of the given arguments is` |
|       - | 4981 | ` *  not an array.` |
|       - | 4982 | ` */` |
|      18 | 4983 | `static int ph7_hashmap_combine(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       2 | 4984 |  |
|       - | 4985 | `	ph7_hashmap_node *pKe,*pVe;` |
|       - | 4986 | `	ph7_hashmap *pKey,*pValue;` |
|       - | 4987 | `	ph7_value *pArray;` |
|       - | 4988 | `	sxu32 n;` |
|       - | 4989 | `	/* PHP enforces argument count and type checks. */` |
|      20 | 4990 | `	if( nArg != 2 ){` |
|       - | 4991 | `		/* wrong number of arguments -> ArgumentCountError */` |
|       4 | 4992 | `		return PH7_VmThrowException(pCtx,` |
|       - | 4993 | `			"ArgumentCountError",` |
|       - | 4994 | `			"array_combine() expects exactly 2 arguments, %d given",` |
|       1 | 4995 | `			nArg` |
|       - | 4996 | `			);` |
|       - | 4997 | `	}` |
|       - | 4998 | `	/* Validate argument types individually so we can report the correct` |
|       - | 4999 | `	 * argument index in the error message. */` |
|      18 | 5000 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|       4 | 5001 | `		return PH7_VmThrowException(pCtx,` |
|       - | 5002 | `			"TypeError",` |
|       - | 5003 | `			"array_combine(): Argument #1 ($keys) must be of type array, %s given",` |
|       1 | 5004 | `			ph7_type_name(apArg[0])` |
|       - | 5005 | `			);` |
|       - | 5006 | `	}` |
|      16 | 5007 | `	if( !ph7_value_is_array(apArg[1]) ){` |
|       4 | 5008 | `		return PH7_VmThrowException(pCtx,` |
|       - | 5009 | `			"TypeError",` |
|       - | 5010 | `			"array_combine(): Argument #2 ($values) must be of type array, %s given",` |
|       2 | 5011 | `			ph7_type_name(apArg[1])` |
|       - | 5012 | `			);` |
|       - | 5013 | `	}` |
|       - | 5014 | `	/* Point to the internal representation of the input hashmaps */` |
|      14 | 5015 | `	pKey   = (ph7_hashmap *)apArg[0]->x.pOther;` |
|      14 | 5016 | `	pValue = (ph7_hashmap *)apArg[1]->x.pOther;` |
|      14 | 5017 | `	if( pKey->nEntry != pValue->nEntry ){` |
|       - | 5018 | `		/* Length mismatch -> ValueError */` |
|       3 | 5019 | `		return PH7_VmThrowException(pCtx,` |
|       - | 5020 | `			"ValueError",` |
|       - | 5021 | `			"array_combine(): Argument #1 ($keys) and argument #2 ($values) must have the same number of elements"` |
|       - | 5022 | `			);` |
|       - | 5023 | `	}` |
|       - | 5024 | `	/* Create a new array */` |
|      11 | 5025 | `	pArray = ph7_context_new_array(pCtx);` |
|      11 | 5026 | `	if( pArray == 0 ){` |
|     ! 0 | 5027 | `		ph7_result_bool(pCtx,0);` |
|     ! 0 | 5028 | `		return PH7_OK;` |
|       - | 5029 | `	}` |
|       - | 5030 | `	/* Perform the requested operation */` |
|      11 | 5031 | `	pKe = pKey->pFirst;` |
|      11 | 5032 | `	pVe = pValue->pFirst;` |
|      33 | 5033 | `	for( n = 0 ; n < pKey->nEntry ; n++ ){` |
|      23 | 5034 | `		ph7_value *pKeyVal = HashmapExtractNodeValue(pKe);` |
|      23 | 5035 | `		ph7_value *pValVal = HashmapExtractNodeValue(pVe);` |
|       - | 5036 | `		/* PHP treats floats used as keys in array_combine differently than` |
|       - | 5037 | `		 * ordinary offset access: the float is stringified rather than` |
|       - | 5038 | `		 * truncated.  To emulate this behavior we create a temporary copy of` |
|       - | 5039 | `		 * the value when it is a float and convert the copy to string.  The` |
|       - | 5040 | `		 * original array must not be mutated. */` |
|      23 | 5041 | `		ph7_value *pKeyCopy = pKeyVal;` |
|      23 | 5042 | `		if( ph7_value_is_float(pKeyVal) ){` |
|       5 | 5043 | `			ph7_value *pTmpKey = ph7_context_new_scalar(pCtx);` |
|       5 | 5044 | `			if( pTmpKey ){` |
|       5 | 5045 | `				PH7_MemObjStore(pKeyVal,pTmpKey);` |
|       - | 5046 | `				/* Convert copy to string so it becomes "1.5" or "2" etc. */` |
|       5 | 5047 | `				PH7_MemObjToString(pTmpKey);` |
|       5 | 5048 | `				pKeyCopy = pTmpKey;` |
|       2 | 5049 | `			}` |
|       2 | 5050 | `		}` |
|      23 | 5051 | `		ph7_array_add_elem(pArray,pKeyCopy,pValVal);` |
|       - | 5052 | `		/* Point to the next entry */` |
|      23 | 5053 | `		pKe = pKe->pPrev; /* Reverse link */` |
|      23 | 5054 | `		pVe = pVe->pPrev;` |
|      12 | 5055 | `	}` |
|       - | 5056 | `	/* Return the filled array */` |
|      11 | 5057 | `	ph7_result_value(pCtx,pArray);` |
|      11 | 5058 | `	return PH7_OK;` |
|      11 | 5059 |  |
|       - | 5060 | `/*` |
|       - | 5061 | ` * array array_reverse(array $array [,bool $preserve_keys = false ])` |
|       - | 5062 | ` *  Return an array with elements in reverse order.` |
|       - | 5063 | ` * Parameters` |
|       - | 5064 | ` *  $array` |
|       - | 5065 | ` *   The input array.` |
|       - | 5066 | ` *  $preserve_keys (optional)` |
|       - | 5067 | ` *   If set to TRUE keys are preserved.` |
|       - | 5068 | ` * Return` |
|       - | 5069 | ` *  The reversed array.` |
|       - | 5070 | ` */` |
|      20 | 5071 | `static int ph7_hashmap_reverse(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       2 | 5072 |  |
|       - | 5073 | `	ph7_hashmap_node *pEntry;` |
|       - | 5074 | `	ph7_hashmap *pSrc;` |
|       - | 5075 | `	ph7_value *pArray;` |
|       - | 5076 | `	int bPreserve;` |
|       - | 5077 | `	sxu32 n;` |
|      22 | 5078 | `	if( nArg < 1 ){` |
|       4 | 5079 | `		return PH7_VmThrowException(pCtx,` |
|       - | 5080 | `			"ArgumentCountError",` |
|       - | 5081 | `			"array_reverse() expects at least 1 argument, %d given",` |
|       1 | 5082 | `			nArg` |
|       - | 5083 | `			);` |
|       - | 5084 | `	}` |
|       - | 5085 | `	/* Make sure we are dealing with a valid hashmap */` |
|      20 | 5086 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|       4 | 5087 | `		return PH7_VmThrowException(pCtx,` |
|       - | 5088 | `			"TypeError",` |
|       - | 5089 | `			"array_reverse(): Argument #1 ($array) must be of type array, %s given",` |
|       1 | 5090 | `			ph7_type_name(apArg[0])` |
|       - | 5091 | `			);` |
|       - | 5092 | `	}` |
|      17 | 5093 | `	bPreserve = FALSE;` |
|      17 | 5094 | `	if( nArg > 1 ){` |
|       7 | 5095 | `		bPreserve = ph7_value_to_bool(apArg[1]);` |
|       3 | 5096 | `	}` |
|       - | 5097 | `	/* Point to the internal representation of the input hashmap */` |
|      17 | 5098 | `	pSrc = (ph7_hashmap *)apArg[0]->x.pOther;` |
|       - | 5099 | `	/* Create a new array */` |
|      17 | 5100 | `	pArray = ph7_context_new_array(pCtx);` |
|      17 | 5101 | `	if( pArray == 0 ){` |
|     ! 0 | 5102 | `		ph7_result_null(pCtx);` |
|     ! 0 | 5103 | `		return PH7_OK;` |
|       - | 5104 | `	}` |
|       - | 5105 | `	/* Perform the requested operation */` |
|      17 | 5106 | `	pEntry = pSrc->pLast;` |
|      55 | 5107 | `	for( n = 0 ; n < pSrc->nEntry ; n++ ){` |
|       - | 5108 | `		/* String keys are always preserved; preserve_keys only affects int keys */` |
|      39 | 5109 | `		int bKeep = (pEntry->iType == HASHMAP_INT_NODE) ? bPreserve : TRUE;` |
|      39 | 5110 | `		HashmapInsertNode((ph7_hashmap *)pArray->x.pOther,pEntry,bKeep);` |
|       - | 5111 | `		/* Point to the previous entry */` |
|      39 | 5112 | `		pEntry = pEntry->pNext; /* Reverse link */` |
|      20 | 5113 | `	}` |
|      17 | 5114 | `	ph7_result_value(pCtx,pArray);` |
|      17 | 5115 | `	return PH7_OK;` |
|      12 | 5116 |  |
|       - | 5117 | `/*` |
|       - | 5118 | ` * array array_unique(array $array, int $flags = SORT_STRING)` |
|       - | 5119 | ` *  Removes duplicate values from an array.` |
|       - | 5120 | ` * Parameters` |
|       - | 5121 | ` *  $array` |
|       - | 5122 | ` *   The input array.` |
|       - | 5123 | ` *  $flags` |
|       - | 5124 | ` *   The optional second parameter may be used to modify the comparison` |
|       - | 5125 | ` *   behavior using these values:` |
|       - | 5126 | ` *     SORT_REGULAR - compare items normally (don't change types)` |
|       - | 5127 | ` *     SORT_NUMERIC - compare items numerically` |
|       - | 5128 | ` *     SORT_STRING  - compare items as strings` |
|       - | 5129 | ` * Return` |
|       - | 5130 | ` *  The filtered array.` |
|       - | 5131 | ` */` |
|      24 | 5132 | `static int ph7_hashmap_unique(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       2 | 5133 |  |
|       - | 5134 | `	ph7_hashmap_node *pEntry;` |
|       - | 5135 | `	ph7_value *pNeedle;` |
|       - | 5136 | `	ph7_hashmap *pSrc;` |
|       - | 5137 | `	ph7_value *pArray;` |
|       - | 5138 | `	int bStrict;` |
|       - | 5139 | `	sxi32 rc;` |
|       - | 5140 | `	sxu32 n;` |
|      26 | 5141 | `	if( nArg < 1 ){` |
|       - | 5142 | `		/* Missing arguments, throw ArgumentCountError */` |
|       3 | 5143 | `		return PH7_VmThrowException(pCtx,` |
|       - | 5144 | `			"ArgumentCountError",` |
|       - | 5145 | `			"array_unique() expects at least 1 argument, 0 given"` |
|       - | 5146 | `			);` |
|       - | 5147 | `	}` |
|      24 | 5148 | `	if( nArg > 2 ){` |
|       - | 5149 | `		/* Too many arguments, throw ArgumentCountError */` |
|       4 | 5150 | `		return PH7_VmThrowException(pCtx,` |
|       - | 5151 | `			"ArgumentCountError",` |
|       - | 5152 | `			"array_unique() expects at most 2 arguments, %d given",` |
|       1 | 5153 | `			nArg` |
|       - | 5154 | `			);` |
|       - | 5155 | `	}` |
|       - | 5156 | `	/* Make sure we are dealing with a valid hashmap */` |
|      22 | 5157 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|       - | 5158 | `		/* Type mismatch, throw TypeError */` |
|       4 | 5159 | `		return PH7_VmThrowException(pCtx,` |
|       - | 5160 | `			"TypeError",` |
|       - | 5161 | `			"array_unique(): Argument #1 ($array) must be of type array, %s given",` |
|       1 | 5162 | `			ph7_type_name(apArg[0])` |
|       - | 5163 | `			);` |
|       - | 5164 | `	}` |
|      19 | 5165 | `	bStrict = FALSE;` |
|       - | 5166 | `	/* Point to the internal representation of the input hashmap */` |
|      19 | 5167 | `	pSrc = (ph7_hashmap *)apArg[0]->x.pOther;` |
|       - | 5168 | `	/* Create a new array */` |
|      19 | 5169 | `	pArray = ph7_context_new_array(pCtx);` |
|      19 | 5170 | `	if( pArray == 0 ){` |
|     ! 0 | 5171 | `		ph7_result_null(pCtx);` |
|     ! 0 | 5172 | `		return PH7_OK;` |
|       - | 5173 | `	}` |
|       - | 5174 | `	/* Perform the requested operation */` |
|      19 | 5175 | `	pEntry = pSrc->pFirst;` |
|      83 | 5176 | `	for( n = 0 ; n < pSrc->nEntry ; n++ ){` |
|      65 | 5177 | `		pNeedle = HashmapExtractNodeValue(pEntry);` |
|      65 | 5178 | `		rc = SXERR_NOTFOUND;` |
|      65 | 5179 | `		if( pNeedle ){` |
|      65 | 5180 | `			rc = HashmapFindValue((ph7_hashmap *)pArray->x.pOther,pNeedle,0,bStrict);` |
|      32 | 5181 | `		}` |
|      65 | 5182 | `		if( rc != SXRET_OK ){` |
|       - | 5183 | `			/* Perform the insertion */` |
|      37 | 5184 | `			HashmapInsertNode((ph7_hashmap *)pArray->x.pOther,pEntry,TRUE);` |
|      18 | 5185 | `		}` |
|       - | 5186 | `		/* Point to the next entry */` |
|      65 | 5187 | `		pEntry = pEntry->pPrev; /* Reverse link */` |
|      33 | 5188 | `	}` |
|       - | 5189 | `	/* Return the freshly created array */` |
|      19 | 5190 | `	ph7_result_value(pCtx,pArray);` |
|      19 | 5191 | `	return PH7_OK;` |
|      14 | 5192 |  |
|       - | 5193 | `/*` |
|       - | 5194 | ` * array array_flip(array $input)` |
|       - | 5195 | ` *  Exchanges all keys with their associated values in an array.` |
|       - | 5196 | ` * Parameter` |
|       - | 5197 | ` *  $input` |
|       - | 5198 | ` *   Input array.` |
|       - | 5199 | ` * Return` |
|       - | 5200 | ` *   The flipped array on success or NULL on failure.` |
|       - | 5201 | ` */` |
|      34 | 5202 | `static int ph7_hashmap_flip(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       2 | 5203 |  |
|       - | 5204 | `	ph7_hashmap_node *pEntry;` |
|       - | 5205 | `	ph7_hashmap *pSrc;` |
|       - | 5206 | `	ph7_value *pArray;` |
|       - | 5207 | `	ph7_value *pKey;` |
|       - | 5208 | `	ph7_value sVal;` |
|       - | 5209 | `	sxu32 n;` |
|       - | 5210 |  |
|       - | 5211 | `	/* PHP requires exactly one argument */` |
|      36 | 5212 | `	if( nArg != 1 ){` |
|       - | 5213 | `		/* Use ArgumentCountError like other array helpers */` |
|       7 | 5214 | `		return PH7_VmThrowException(pCtx,` |
|       - | 5215 | `			"ArgumentCountError",` |
|       - | 5216 | `			"array_flip() expects exactly 1 argument, %d given",` |
|       2 | 5217 | `			nArg` |
|       - | 5218 | `			);` |
|       - | 5219 | `	}` |
|       - | 5220 | `	/* Make sure we are dealing with a valid hashmap */` |
|      32 | 5221 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|       - | 5222 | `		/* Type mismatch -> TypeError */` |
|       7 | 5223 | `		return PH7_VmThrowException(pCtx,` |
|       - | 5224 | `			"TypeError",` |
|       - | 5225 | `			"array_flip(): Argument #1 ($array) must be of type array, %s given",` |
|       2 | 5226 | `			ph7_type_name(apArg[0])` |
|       - | 5227 | `			);` |
|       - | 5228 | `	}` |
|       - | 5229 | `	/* Point to the internal representation of the input hashmap */` |
|      27 | 5230 | `	pSrc = (ph7_hashmap *)apArg[0]->x.pOther;` |
|       - | 5231 | `	/* Create a new array */` |
|      27 | 5232 | `	pArray = ph7_context_new_array(pCtx);` |
|      27 | 5233 | `	if( pArray == 0 ){` |
|     ! 0 | 5234 | `		ph7_result_null(pCtx);` |
|     ! 0 | 5235 | `		return PH7_OK;` |
|       - | 5236 | `	}` |
|       - | 5237 | `	/* Start processing */` |
|      27 | 5238 | `	pEntry = pSrc->pFirst;` |
|   22263 | 5239 | `	for( n = 0 ; n < pSrc->nEntry ; n++ ){` |
|       - | 5240 | `		/* Extract the node value (will become a key in the result) */` |
|   22237 | 5241 | `		pKey = HashmapExtractNodeValue(pEntry);` |
|   22237 | 5242 | `		if( pKey ){` |
|       - | 5243 | `			/* NULL values are not valid keys either, PHP emits a warning */` |
|   22237 | 5244 | `			if( pKey->iFlags & MEMOBJ_NULL ){` |
|       3 | 5245 | `				PH7_VmThrowError(pCtx->pVm,0,PH7_CTX_WARNING,` |
|       - | 5246 | `					"array_flip(): Can only flip string and integer values, entry skipped"` |
|       - | 5247 | `					);` |
|   22236 | 5248 | `			}else if( (pKey->iFlags & MEMOBJ_INT) \|\| (pKey->iFlags & MEMOBJ_STRING) ){` |
|       - | 5249 | `				/* Prepare the value for insertion (original key) */` |
|   22227 | 5250 | `				if( pEntry->iType == HASHMAP_INT_NODE ){` |
|   20001 | 5251 | `					PH7_MemObjInitFromInt(pSrc->pVm,&sVal,pEntry->xKey.iKey);` |
|   10001 | 5252 | `				}else{` |
|       - | 5253 | `					SyString sStr;` |
|    2227 | 5254 | `					SyStringInitFromBuf(&sStr,SyBlobData(&pEntry->xKey.sKey),SyBlobLength(&pEntry->xKey.sKey));` |
|    2227 | 5255 | `					PH7_MemObjInitFromString(pSrc->pVm,&sVal,&sStr);` |
|       - | 5256 | `				}` |
|       - | 5257 | `				/* Perform the insertion */` |
|   22227 | 5258 | `				ph7_array_add_elem(pArray,pKey,&sVal);` |
|       - | 5259 | `				/* Safely release the value because each inserted entry` |
|       - | 5260 | `				 * has its own private copy of the value.` |
|       - | 5261 | `				 */` |
|   22227 | 5262 | `				PH7_MemObjRelease(&sVal);` |
|   11114 | 5263 | `			}else{` |
|       - | 5264 | `				/* Unsupported value type -> emit warning and skip the entry */` |
|       9 | 5265 | `				PH7_VmThrowError(pCtx->pVm,0,PH7_CTX_WARNING,` |
|       - | 5266 | `					"array_flip(): Can only flip string and integer values, entry skipped"` |
|       - | 5267 | `					);` |
|       - | 5268 | `			}` |
|   11118 | 5269 | `		}` |
|       - | 5270 | `		/* Point to the next entry */` |
|   22237 | 5271 | `		pEntry = pEntry->pPrev; /* Reverse link */` |
|   11119 | 5272 | `	}` |
|       - | 5273 | `	/* Return the freshly created array */` |
|      27 | 5274 | `	ph7_result_value(pCtx,pArray);` |
|      27 | 5275 | `	return PH7_OK;` |
|      19 | 5276 |  |
|       - | 5277 | `/*` |
|       - | 5278 | ` * number array_sum(array $array )` |
|       - | 5279 | ` *  Calculate the sum of values in an array.` |
|       - | 5280 | ` * Parameters` |
|       - | 5281 | ` *  $array: The input array.` |
|       - | 5282 | ` * Return` |
|       - | 5283 | ` *  Returns the sum of values as an integer or float.` |
|       - | 5284 | ` */` |
|      24 | 5285 | `static void DoubleSum(ph7_context *pCtx,ph7_hashmap *pMap)` |
|       1 | 5286 |  |
|       - | 5287 | `	ph7_hashmap_node *pEntry;` |
|       - | 5288 | `	ph7_value *pObj;` |
|      25 | 5289 | `	double dSum = 0;` |
|       - | 5290 | `	sxu32 n;` |
|      25 | 5291 | `	pEntry = pMap->pFirst;` |
|      91 | 5292 | `	for( n = 0 ; n < pMap->nEntry ; n++ ){` |
|      67 | 5293 | `		pObj = HashmapExtractNodeValue(pEntry);` |
|      67 | 5294 | `		if( pObj ){` |
|      67 | 5295 | `			if( pObj->iFlags & MEMOBJ_REAL ){` |
|      29 | 5296 | `				dSum += pObj->rVal;` |
|      53 | 5297 | `			}else if( pObj->iFlags & (MEMOBJ_INT\|MEMOBJ_BOOL) ){` |
|      21 | 5298 | `				dSum += (double)pObj->x.iVal;` |
|      29 | 5299 | `			}else if( pObj->iFlags & MEMOBJ_STRING ){` |
|      15 | 5300 | `				if( SyBlobLength(&pObj->sBlob) > 0 ){` |
|      13 | 5301 | `					double dv = 0;` |
|      13 | 5302 | `					SyStrToReal((const char *)SyBlobData(&pObj->sBlob),SyBlobLength(&pObj->sBlob),(void *)&dv,0);` |
|      13 | 5303 | `					dSum += dv;` |
|       7 | 5304 | `				}` |
|      12 | 5305 | `			}else if( pObj->iFlags & MEMOBJ_HASHMAP ){` |
|       3 | 5306 | `				PH7_VmThrowError(pCtx->pVm,0,PH7_CTX_WARNING,` |
|       - | 5307 | `					"array_sum(): Addition is not supported on type array");` |
|       4 | 5308 | `			}else if( pObj->iFlags & MEMOBJ_OBJ ){` |
|     ! 0 | 5309 | `				PH7_VmThrowError(pCtx->pVm,0,PH7_CTX_WARNING,` |
|       - | 5310 | `					"array_sum(): Addition is not supported on type object");` |
|       3 | 5311 | `			}else if( pObj->iFlags & MEMOBJ_RES ){` |
|     ! 0 | 5312 | `				PH7_VmThrowError(pCtx->pVm,0,PH7_CTX_WARNING,` |
|       - | 5313 | `					"array_sum(): Addition is not supported on type resource");` |
|     ! 0 | 5314 | `			}` |
|       - | 5315 | `			/* NULL is silently treated as 0 (matches PHP) */` |
|      33 | 5316 | `		}` |
|       - | 5317 | `		/* Point to the next entry */` |
|      67 | 5318 | `		pEntry = pEntry->pPrev; /* Reverse link */` |
|      34 | 5319 | `	}` |
|       - | 5320 | `	/* Return sum */` |
|      25 | 5321 | `	ph7_result_double(pCtx,dSum);` |
|      25 | 5322 |  |
|      18 | 5323 | `static void Int64Sum(ph7_context *pCtx,ph7_hashmap *pMap)` |
|       2 | 5324 |  |
|       - | 5325 | `	ph7_hashmap_node *pEntry;` |
|       - | 5326 | `	ph7_value *pObj;` |
|      20 | 5327 | `	sxi64 nSum = 0;` |
|       - | 5328 | `	sxu32 n;` |
|      20 | 5329 | `	pEntry = pMap->pFirst;` |
|      80 | 5330 | `	for( n = 0 ; n < pMap->nEntry ; n++ ){` |
|      62 | 5331 | `		pObj = HashmapExtractNodeValue(pEntry);` |
|      62 | 5332 | `		if( pObj ){` |
|      62 | 5333 | `			if( pObj->iFlags & (MEMOBJ_INT\|MEMOBJ_BOOL) ){` |
|      52 | 5334 | `				nSum += pObj->x.iVal;` |
|      36 | 5335 | `			}else if( pObj->iFlags & MEMOBJ_STRING ){` |
|       7 | 5336 | `				if( SyBlobLength(&pObj->sBlob) > 0 ){` |
|       5 | 5337 | `					sxi64 nv = 0;` |
|       5 | 5338 | `					SyStrToInt64((const char *)SyBlobData(&pObj->sBlob),SyBlobLength(&pObj->sBlob),(void *)&nv,0);` |
|       5 | 5339 | `					nSum += nv;` |
|       3 | 5340 | `				}` |
|       8 | 5341 | `			}else if( pObj->iFlags & MEMOBJ_HASHMAP ){` |
|       3 | 5342 | `				PH7_VmThrowError(pCtx->pVm,0,PH7_CTX_WARNING,` |
|       - | 5343 | `					"array_sum(): Addition is not supported on type array");` |
|       4 | 5344 | `			}else if( pObj->iFlags & MEMOBJ_OBJ ){` |
|     ! 0 | 5345 | `				PH7_VmThrowError(pCtx->pVm,0,PH7_CTX_WARNING,` |
|       - | 5346 | `					"array_sum(): Addition is not supported on type object");` |
|       3 | 5347 | `			}else if( pObj->iFlags & MEMOBJ_RES ){` |
|     ! 0 | 5348 | `				PH7_VmThrowError(pCtx->pVm,0,PH7_CTX_WARNING,` |
|       - | 5349 | `					"array_sum(): Addition is not supported on type resource");` |
|     ! 0 | 5350 | `			}` |
|       - | 5351 | `			/* NULL is silently treated as 0 (matches PHP) */` |
|      30 | 5352 | `		}` |
|       - | 5353 | `		/* Point to the next entry */` |
|      62 | 5354 | `		pEntry = pEntry->pPrev; /* Reverse link */` |
|      32 | 5355 | `	}` |
|       - | 5356 | `	/* Return sum */` |
|      20 | 5357 | `	ph7_result_int64(pCtx,nSum);` |
|      20 | 5358 |  |
|       - | 5359 | `/* number array_sum(array $array )` |
|       - | 5360 | ` * (See block-coment above)` |
|       - | 5361 | ` */` |
|      52 | 5362 | `static int ph7_hashmap_sum(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       2 | 5363 |  |
|       - | 5364 | `	ph7_hashmap_node *pEntry;` |
|       - | 5365 | `	ph7_hashmap *pMap;` |
|       - | 5366 | `	ph7_value *pObj;` |
|      54 | 5367 | `	int useDouble = 0;` |
|       - | 5368 | `	sxu32 n;` |
|       - | 5369 | `	/* PHP requires exactly one argument */` |
|      54 | 5370 | `	if( nArg != 1 ){` |
|       7 | 5371 | `		return PH7_VmThrowException(pCtx,` |
|       - | 5372 | `			"ArgumentCountError",` |
|       - | 5373 | `			"array_sum() expects exactly 1 argument, %d given",` |
|       2 | 5374 | `			nArg` |
|       - | 5375 | `			);` |
|       - | 5376 | `	}` |
|       - | 5377 | `	/* Make sure we are dealing with a valid hashmap */` |
|      50 | 5378 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|       - | 5379 | `		/* Type mismatch -> TypeError */` |
|       7 | 5380 | `		return PH7_VmThrowException(pCtx,` |
|       - | 5381 | `			"TypeError",` |
|       - | 5382 | `			"array_sum(): Argument #1 ($array) must be of type array, %s given",` |
|       2 | 5383 | `			ph7_type_name(apArg[0])` |
|       - | 5384 | `			);` |
|       - | 5385 | `	}` |
|      46 | 5386 | `	pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|      46 | 5387 | `	if( pMap->nEntry < 1 ){` |
|       - | 5388 | `		/* Nothing to compute,return 0 */` |
|       3 | 5389 | `		ph7_result_int(pCtx,0);` |
|       3 | 5390 | `		return PH7_OK;` |
|       - | 5391 | `	}` |
|       - | 5392 | `	/* Scan all elements: if any value is a float, use floating-point` |
|       - | 5393 | `	 * arithmetic for the entire sum (matches PHP behaviour).` |
|       - | 5394 | `	 */` |
|      44 | 5395 | `	pEntry = pMap->pFirst;` |
|     112 | 5396 | `	for( n = 0 ; n < pMap->nEntry ; n++ ){` |
|      94 | 5397 | `		pObj = HashmapExtractNodeValue(pEntry);` |
|      94 | 5398 | `		if( pObj ){` |
|      94 | 5399 | `			if( pObj->iFlags & MEMOBJ_REAL ){` |
|      19 | 5400 | `				useDouble = 1;` |
|      19 | 5401 | `				break;` |
|       - | 5402 | `			}` |
|      76 | 5403 | `			if( pObj->iFlags & MEMOBJ_STRING ){` |
|      13 | 5404 | `				const char *zStr = (const char *)SyBlobData(&pObj->sBlob);` |
|      13 | 5405 | `				sxu32 nLen = SyBlobLength(&pObj->sBlob);` |
|       - | 5406 | `				sxu32 i;` |
|      23 | 5407 | `				for( i = 0 ; i < nLen ; i++ ){` |
|      17 | 5408 | `					if( zStr[i] == '.' \|\| zStr[i] == 'e' \|\| zStr[i] == 'E' ){` |
|       7 | 5409 | `						useDouble = 1;` |
|       7 | 5410 | `						break;` |
|       - | 5411 | `					}` |
|       6 | 5412 | `				}` |
|      13 | 5413 | `				if( useDouble ){` |
|       7 | 5414 | `					break;` |
|       - | 5415 | `				}` |
|       3 | 5416 | `			}` |
|      34 | 5417 | `		}` |
|      70 | 5418 | `		pEntry = pEntry->pPrev;` |
|      36 | 5419 | `	}` |
|      44 | 5420 | `	if( useDouble ){` |
|      25 | 5421 | `		DoubleSum(pCtx,pMap);` |
|      13 | 5422 | `	}else{` |
|      20 | 5423 | `		Int64Sum(pCtx,pMap);` |
|       - | 5424 | `	}` |
|      44 | 5425 | `	return PH7_OK;` |
|      28 | 5426 |  |
|       - | 5427 | `/*` |
|       - | 5428 | ` * number array_product(array $array )` |
|       - | 5429 | ` *  Calculate the product of values in an array.` |
|       - | 5430 | ` * Parameters` |
|       - | 5431 | ` *  $array: The input array.` |
|       - | 5432 | ` * Return` |
|       - | 5433 | ` *  Returns the product of values as an integer or float.` |
|       - | 5434 | ` */` |
|     ! 0 | 5435 | `static void DoubleProd(ph7_context *pCtx,ph7_hashmap *pMap)` |
|     ! 0 | 5436 |  |
|       - | 5437 | `	ph7_hashmap_node *pEntry;` |
|       - | 5438 | `	ph7_value *pObj;` |
|       - | 5439 | `	double dProd;` |
|       - | 5440 | `	sxu32 n;` |
|     ! 0 | 5441 | `	pEntry = pMap->pFirst;` |
|     ! 0 | 5442 | `	dProd = 1;` |
|     ! 0 | 5443 | `	for( n = 0 ; n < pMap->nEntry ; n++ ){` |
|     ! 0 | 5444 | `		pObj = HashmapExtractNodeValue(pEntry);` |
|     ! 0 | 5445 | `		if( pObj && (pObj->iFlags & (MEMOBJ_NULL\|MEMOBJ_HASHMAP\|MEMOBJ_OBJ\|MEMOBJ_RES)) == 0){` |
|     ! 0 | 5446 | `			if( pObj->iFlags & MEMOBJ_REAL ){` |
|     ! 0 | 5447 | `				dProd *= pObj->rVal;` |
|     ! 0 | 5448 | `			}else if( pObj->iFlags & (MEMOBJ_INT\|MEMOBJ_BOOL) ){` |
|     ! 0 | 5449 | `				dProd *= (double)pObj->x.iVal;` |
|     ! 0 | 5450 | `			}else if( pObj->iFlags & MEMOBJ_STRING ){` |
|     ! 0 | 5451 | `				if( SyBlobLength(&pObj->sBlob) > 0 ){` |
|     ! 0 | 5452 | `					double dv = 0;` |
|     ! 0 | 5453 | `					SyStrToReal((const char *)SyBlobData(&pObj->sBlob),SyBlobLength(&pObj->sBlob),(void *)&dv,0);` |
|     ! 0 | 5454 | `					dProd *= dv;` |
|     ! 0 | 5455 | `				}` |
|     ! 0 | 5456 | `			}` |
|     ! 0 | 5457 | `		}` |
|       - | 5458 | `		/* Point to the next entry */` |
|     ! 0 | 5459 | `		pEntry = pEntry->pPrev; /* Reverse link */` |
|     ! 0 | 5460 | `	}` |
|       - | 5461 | `	/* Return product */` |
|     ! 0 | 5462 | `	ph7_result_double(pCtx,dProd);` |
|     ! 0 | 5463 |  |
|     ! 0 | 5464 | `static void Int64Prod(ph7_context *pCtx,ph7_hashmap *pMap)` |
|     ! 0 | 5465 |  |
|       - | 5466 | `	ph7_hashmap_node *pEntry;` |
|       - | 5467 | `	ph7_value *pObj;` |
|       - | 5468 | `	sxi64 nProd;` |
|       - | 5469 | `	sxu32 n;` |
|     ! 0 | 5470 | `	pEntry = pMap->pFirst;` |
|     ! 0 | 5471 | `	nProd = 1;` |
|     ! 0 | 5472 | `	for( n = 0 ; n < pMap->nEntry ; n++ ){` |
|     ! 0 | 5473 | `		pObj = HashmapExtractNodeValue(pEntry);` |
|     ! 0 | 5474 | `		if( pObj && (pObj->iFlags & (MEMOBJ_NULL\|MEMOBJ_HASHMAP\|MEMOBJ_OBJ\|MEMOBJ_RES)) == 0){` |
|     ! 0 | 5475 | `			if( pObj->iFlags & MEMOBJ_REAL ){` |
|     ! 0 | 5476 | `				nProd *= (sxi64)pObj->rVal;` |
|     ! 0 | 5477 | `			}else if( pObj->iFlags & (MEMOBJ_INT\|MEMOBJ_BOOL) ){` |
|     ! 0 | 5478 | `				nProd *= pObj->x.iVal;` |
|     ! 0 | 5479 | `			}else if( pObj->iFlags & MEMOBJ_STRING ){` |
|     ! 0 | 5480 | `				if( SyBlobLength(&pObj->sBlob) > 0 ){` |
|     ! 0 | 5481 | `					sxi64 nv = 0;` |
|     ! 0 | 5482 | `					SyStrToInt64((const char *)SyBlobData(&pObj->sBlob),SyBlobLength(&pObj->sBlob),(void *)&nv,0);` |
|     ! 0 | 5483 | `					nProd *= nv;` |
|     ! 0 | 5484 | `				}` |
|     ! 0 | 5485 | `			}` |
|     ! 0 | 5486 | `		}` |
|       - | 5487 | `		/* Point to the next entry */` |
|     ! 0 | 5488 | `		pEntry = pEntry->pPrev; /* Reverse link */` |
|     ! 0 | 5489 | `	}` |
|       - | 5490 | `	/* Return product */` |
|     ! 0 | 5491 | `	ph7_result_int64(pCtx,nProd);` |
|     ! 0 | 5492 |  |
|       - | 5493 | `/* number array_product(array $array )` |
|       - | 5494 | ` * (See block-block comment above)` |
|       - | 5495 | ` */` |
|     ! 0 | 5496 | `static int ph7_hashmap_product(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     ! 0 | 5497 |  |
|       - | 5498 | `	ph7_hashmap *pMap;` |
|       - | 5499 | `	ph7_value *pObj;` |
|     ! 0 | 5500 | `	if( nArg < 1 ){` |
|       - | 5501 | `		/* Missing arguments,return 0 */` |
|     ! 0 | 5502 | `		ph7_result_int(pCtx,0);` |
|     ! 0 | 5503 | `		return PH7_OK;` |
|       - | 5504 | `	}` |
|       - | 5505 | `	/* Make sure we are dealing with a valid hashmap */` |
|     ! 0 | 5506 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|       - | 5507 | `		/* Invalid argument,return 0 */` |
|     ! 0 | 5508 | `		ph7_result_int(pCtx,0);` |
|     ! 0 | 5509 | `		return PH7_OK;` |
|       - | 5510 | `	}` |
|     ! 0 | 5511 | `	pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|     ! 0 | 5512 | `	if( pMap->nEntry < 1 ){` |
|       - | 5513 | `		/* Nothing to compute,return 0 */` |
|     ! 0 | 5514 | `		ph7_result_int(pCtx,0);` |
|     ! 0 | 5515 | `		return PH7_OK;` |
|       - | 5516 | `	}` |
|       - | 5517 | `	/* If the first element is of type float,then perform floating` |
|       - | 5518 | `	 * point computaion.Otherwise switch to int64 computaion.` |
|       - | 5519 | `	 */` |
|     ! 0 | 5520 | `	pObj = HashmapExtractNodeValue(pMap->pFirst);` |
|     ! 0 | 5521 | `	if( pObj == 0 ){` |
|     ! 0 | 5522 | `		ph7_result_int(pCtx,0);` |
|     ! 0 | 5523 | `		return PH7_OK;` |
|       - | 5524 | `	}` |
|     ! 0 | 5525 | `	if( pObj->iFlags & MEMOBJ_REAL ){` |
|     ! 0 | 5526 | `		DoubleProd(pCtx,pMap);` |
|     ! 0 | 5527 | `	}else{` |
|     ! 0 | 5528 | `		Int64Prod(pCtx,pMap);` |
|       - | 5529 | `	}` |
|     ! 0 | 5530 | `	return PH7_OK;` |
|     ! 0 | 5531 |  |
|       - | 5532 | `/*` |
|       - | 5533 | ` * value array_rand(array $input[,int $num_req = 1 ])` |
|       - | 5534 | ` *  Pick one or more random entries out of an array.` |
|       - | 5535 | ` * Parameters` |
|       - | 5536 | ` * $input` |
|       - | 5537 | ` *  The input array.` |
|       - | 5538 | ` * $num_req` |
|       - | 5539 | ` *  Specifies how many entries you want to pick.` |
|       - | 5540 | ` * Return` |
|       - | 5541 | ` *  If you are picking only one entry, array_rand() returns the key for a random entry.` |
|       - | 5542 | ` *  Otherwise, it returns an array of keys for the random entries.` |
|       - | 5543 | ` *  NULL is returned on failure.` |
|       - | 5544 | ` */` |
|       6 | 5545 | `static int ph7_hashmap_rand(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       1 | 5546 |  |
|       - | 5547 | `	ph7_hashmap_node *pNode;` |
|       - | 5548 | `	ph7_hashmap *pMap;` |
|       7 | 5549 | `	int nItem = 1;` |
|       7 | 5550 | `	if( nArg < 1 ){` |
|       - | 5551 | `		/* Missing argument,return NULL */` |
|     ! 0 | 5552 | `		ph7_result_null(pCtx);` |
|     ! 0 | 5553 | `		return PH7_OK;` |
|       - | 5554 | `	}` |
|       - | 5555 | `	/* Make sure we are dealing with an array */` |
|       7 | 5556 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|     ! 0 | 5557 | `		ph7_result_null(pCtx);` |
|     ! 0 | 5558 | `		return PH7_OK;` |
|       - | 5559 | `	}` |
|       - | 5560 | `	/* Point to the internal representation of the input hashmap */` |
|       7 | 5561 | `	pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|       7 | 5562 | `	if(pMap->nEntry < 1 ){` |
|       - | 5563 | `		/* Empty hashmap,return NULL */` |
|     ! 0 | 5564 | `		ph7_result_null(pCtx);` |
|     ! 0 | 5565 | `		return PH7_OK;` |
|       - | 5566 | `	}` |
|       7 | 5567 | `	if( nArg > 1 ){` |
|       3 | 5568 | `		nItem = ph7_value_to_int(apArg[1]);` |
|       1 | 5569 | `	}` |
|       7 | 5570 | `	if( nItem < 2 ){` |
|       - | 5571 | `		sxu32 nEntry;` |
|       - | 5572 | `		/* Select a random number */` |
|       5 | 5573 | `		nEntry = PH7_VmRandomNum(pMap->pVm) % pMap->nEntry;` |
|       - | 5574 | `		/* Extract the desired entry.` |
|       - | 5575 | `		 * Note that we perform a linear lookup here (later version must change this)` |
|       - | 5576 | `		 */` |
|       5 | 5577 | `		if( nEntry > pMap->nEntry / 2 ){` |
|       2 | 5578 | `			pNode = pMap->pLast;` |
|       2 | 5579 | `			nEntry = pMap->nEntry - nEntry;` |
|       2 | 5580 | `			if( nEntry > 1 ){` |
|     ! 0 | 5581 | `				for(;;){` |
|     ! 0 | 5582 | `					if( nEntry == 0 ){` |
|     ! 0 | 5583 | `						break;` |
|       - | 5584 | `					}` |
|       - | 5585 | `					/* Point to the previous entry */` |
|     ! 0 | 5586 | `					pNode = pNode->pNext; /* Reverse link */` |
|     ! 0 | 5587 | `					nEntry--;` |
|     ! 0 | 5588 | `				}` |
|     ! 0 | 5589 | `			}` |
|       1 | 5590 | `		}else{` |
|       3 | 5591 | `			pNode = pMap->pFirst;` |
|       1 | 5592 | `			for(;;){` |
|       3 | 5593 | `				if( nEntry == 0 ){` |
|       3 | 5594 | `					break;` |
|       - | 5595 | `				}` |
|       - | 5596 | `				/* Point to the next entry */` |
|     ! 0 | 5597 | `				pNode = pNode->pPrev; /* Reverse link */` |
|     ! 0 | 5598 | `				nEntry--;` |
|     ! 0 | 5599 | `			}` |
|       - | 5600 | `		}` |
|       5 | 5601 | `		if( pNode->iType == HASHMAP_INT_NODE ){` |
|       - | 5602 | `			/* Int key */` |
|       3 | 5603 | `			ph7_result_int64(pCtx,pNode->xKey.iKey);` |
|       2 | 5604 | `		}else{` |
|       - | 5605 | `			/* Blob key */` |
|       3 | 5606 | `			ph7_result_string(pCtx,(const char *)SyBlobData(&pNode->xKey.sKey),(int)SyBlobLength(&pNode->xKey.sKey));` |
|       - | 5607 | `		}` |
|       3 | 5608 | `	}else{` |
|       - | 5609 | `		ph7_value sKey,*pArray;` |
|       - | 5610 | `		ph7_hashmap *pDest;` |
|       - | 5611 | `		/* Create a new array */` |
|       3 | 5612 | `		pArray = ph7_context_new_array(pCtx);` |
|       3 | 5613 | `		if( pArray == 0 ){` |
|     ! 0 | 5614 | `			ph7_result_null(pCtx);` |
|     ! 0 | 5615 | `			return PH7_OK;` |
|       - | 5616 | `		}` |
|       - | 5617 | `		/* Point to the internal representation of the hashmap */` |
|       3 | 5618 | `		pDest = (ph7_hashmap *)pArray->x.pOther;` |
|       3 | 5619 | `		PH7_MemObjInit(pDest->pVm,&sKey);` |
|       - | 5620 | `		/* Copy the first n items */` |
|       3 | 5621 | `		pNode = pMap->pFirst;` |
|       3 | 5622 | `		if( nItem > (int)pMap->nEntry ){` |
|     ! 0 | 5623 | `			nItem = (int)pMap->nEntry;` |
|     ! 0 | 5624 | `		}` |
|       7 | 5625 | `		while( nItem > 0){` |
|       5 | 5626 | `			PH7_HashmapExtractNodeKey(pNode,&sKey);` |
|       5 | 5627 | `			PH7_HashmapInsert(pDest,0/* Automatic index assign*/,&sKey);` |
|       5 | 5628 | `			PH7_MemObjRelease(&sKey);` |
|       - | 5629 | `			/* Point to the next entry */` |
|       5 | 5630 | `			pNode = pNode->pPrev; /* Reverse link */` |
|       5 | 5631 | `			nItem--;` |
|       1 | 5632 | `		}` |
|       - | 5633 | `		/* Shuffle the array */` |
|       3 | 5634 | `		HashmapMergeSort(pDest,HashmapCmpCallback7,0);` |
|       - | 5635 | `		/* Rehash node */` |
|       3 | 5636 | `		HashmapSortRehash(pDest);` |
|       - | 5637 | `		/* Return the random array */` |
|       3 | 5638 | `		ph7_result_value(pCtx,pArray);` |
|       - | 5639 | `	}` |
|       7 | 5640 | `	return PH7_OK;` |
|       4 | 5641 |  |
|       - | 5642 | `/*` |
|       - | 5643 | ` * array array_chunk (array $input,int $size [,bool $preserve_keys = false ])` |
|       - | 5644 | ` *  Split an array into chunks.` |
|       - | 5645 | ` * Parameters` |
|       - | 5646 | ` * $input` |
|       - | 5647 | ` *   The array to work on` |
|       - | 5648 | ` * $size` |
|       - | 5649 | ` *   The size of each chunk` |
|       - | 5650 | ` * $preserve_keys` |
|       - | 5651 | ` *   When set to TRUE keys will be preserved. Default is FALSE which will reindex` |
|       - | 5652 | ` *   the chunk numerically.` |
|       - | 5653 | ` * Return` |
|       - | 5654 | ` *  Returns a multidimensional numerically indexed array, starting with` |
|       - | 5655 | ` *  zero, with each dimension containing size elements.` |
|       - | 5656 | ` */` |
|      42 | 5657 | `static int ph7_hashmap_chunk(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       2 | 5658 |  |
|       - | 5659 | `	ph7_value *pArray,*pChunk;` |
|       - | 5660 | `	ph7_hashmap_node *pEntry;` |
|       - | 5661 | `	ph7_hashmap *pMap;` |
|       - | 5662 | `	int bPreserve;` |
|       - | 5663 | `	sxu32 nChunk;` |
|       - | 5664 | `	sxu32 nSize;` |
|       - | 5665 | `	sxu32 n;` |
|       - | 5666 | `	/* Argument count and types follow PHP semantics. */` |
|      44 | 5667 | `	if( nArg < 2 ){` |
|       - | 5668 | `		/* fewer than required arguments -> ArgumentCountError */` |
|       4 | 5669 | `		return PH7_VmThrowException(pCtx,` |
|       - | 5670 | `			"ArgumentCountError",` |
|       - | 5671 | `			"array_chunk() expects at least 2 arguments, %d given",` |
|       1 | 5672 | `			nArg` |
|       - | 5673 | `			);` |
|       - | 5674 | `	}` |
|      42 | 5675 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|       4 | 5676 | `		return PH7_VmThrowException(pCtx,` |
|       - | 5677 | `			"TypeError",` |
|       - | 5678 | `			"array_chunk(): Argument #1 ($array) must be of type array, %s given",` |
|       1 | 5679 | `			ph7_type_name(apArg[0])` |
|       - | 5680 | `			);` |
|       - | 5681 | `	}` |
|       - | 5682 | `	/* Create a new array */` |
|      40 | 5683 | `	pArray = ph7_context_new_array(pCtx);` |
|      40 | 5684 | `	if( pArray == 0 ){` |
|     ! 0 | 5685 | `		ph7_result_null(pCtx);` |
|     ! 0 | 5686 | `		return PH7_OK;` |
|       - | 5687 | `	}` |
|       - | 5688 | `	/* Point to the internal representation of the input hashmap */` |
|      40 | 5689 | `	pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|       - | 5690 | `	/* Extract and validate the chunk size argument. */` |
|       - | 5691 | `	/* Reject types that cannot be sensibly converted to an integer. */` |
|      76 | 5692 | `	if( ph7_value_is_array(apArg[1]) \|\| ph7_value_is_object(apArg[1]) \|\|` |
|      78 | 5693 | `		ph7_value_is_resource(apArg[1]) \|\| ph7_value_is_null(apArg[1]) \|\|` |
|      38 | 5694 | `		ph7_value_is_bool(apArg[1]) ){` |
|     ! 0 | 5695 | `		return PH7_VmThrowException(pCtx,` |
|       - | 5696 | `			"TypeError",` |
|       - | 5697 | `			"array_chunk(): Argument #2 ($length) must be of type int, %s given",` |
|     ! 0 | 5698 | `			ph7_type_name(apArg[1])` |
|       - | 5699 | `			);` |
|       - | 5700 | `	}` |
|       - | 5701 | `	/* Strings that are non-numeric produce a TypeError.  Numeric` |
|       - | 5702 | `	 * strings are permitted; however those representing floats lose` |
|       - | 5703 | `	 * precision and PHP emits a deprecation warning. */` |
|      40 | 5704 | `	if( ph7_value_is_string(apArg[1]) ){` |
|       - | 5705 | `		int len;` |
|       3 | 5706 | `		sxu8 bReal = FALSE;` |
|       3 | 5707 | `		const char *zStr = ph7_value_to_string(apArg[1], &len);` |
|       3 | 5708 | `		if( SyStrIsNumeric(zStr, len, &bReal, 0) != SXRET_OK ){` |
|       3 | 5709 | `			return PH7_VmThrowException(pCtx,` |
|       - | 5710 | `				"TypeError",` |
|       - | 5711 | `				"array_chunk(): Argument #2 ($length) must be of type int, string given"` |
|       - | 5712 | `				);` |
|       - | 5713 | `		}` |
|     ! 0 | 5714 | `		if( bReal ){` |
|       - | 5715 | `			/* float-string -> warn but allow */` |
|     ! 0 | 5716 | `			ph7_context_throw_error_format(pCtx, E_DEPRECATED,` |
|       - | 5717 | `				"Implicit conversion from float-string \"%s\" to int loses precision",` |
|     ! 0 | 5718 | `				zStr` |
|       - | 5719 | `				);` |
|     ! 0 | 5720 | `		}` |
|     ! 0 | 5721 | `	}` |
|       - | 5722 | `	/* If the value is a float with a fractional component, emit a` |
|       - | 5723 | `	 * deprecation warning but continue.  The following conversion occurs` |
|       - | 5724 | `	 * later via ph7_value_to_int. */` |
|      38 | 5725 | `	if( ph7_value_is_float(apArg[1]) ){` |
|       3 | 5726 | `		double d = ph7_value_to_double(apArg[1]);` |
|       3 | 5727 | `		sxi64 i = (sxi64)d;` |
|       3 | 5728 | `		if( d != (double)i ){` |
|       4 | 5729 | `			ph7_context_throw_error_format(pCtx, E_DEPRECATED,` |
|       - | 5730 | `				"Implicit conversion from float %g to int loses precision",` |
|       1 | 5731 | `				d` |
|       - | 5732 | `				);` |
|       1 | 5733 | `		}` |
|       1 | 5734 | `	}` |
|       - | 5735 | `	/* Convert using ph7_value_to_int; now that float fractions are` |
|       - | 5736 | `	 * eliminated, this will not produce a warning. */` |
|       - | 5737 | `	{` |
|      38 | 5738 | `		sxi64 nSizeSigned = ph7_value_to_int(apArg[1]);` |
|      38 | 5739 | `		if( nSizeSigned < 1 ){` |
|       - | 5740 | `			/* size <= 0 -> ValueError */` |
|       5 | 5741 | `			return PH7_VmThrowException(pCtx,` |
|       - | 5742 | `				"ValueError",` |
|       - | 5743 | `				"array_chunk(): Argument #2 ($length) must be greater than 0"` |
|       - | 5744 | `				);` |
|       - | 5745 | `		}` |
|      34 | 5746 | `		nSize = (sxu32)nSizeSigned;` |
|       - | 5747 | `	}` |
|      34 | 5748 | `	if( nSize >= pMap->nEntry ){` |
|       - | 5749 | `		/* Return the whole array */` |
|       3 | 5750 | `		ph7_array_add_elem(pArray,0,apArg[0]);` |
|       3 | 5751 | `		ph7_result_value(pCtx,pArray);` |
|       3 | 5752 | `		return PH7_OK;` |
|       - | 5753 | `	}` |
|      32 | 5754 | `	bPreserve = 0;` |
|      32 | 5755 | `	if( nArg > 2 ){` |
|       - | 5756 | `		/* The third argument has a bool type hint in PHP.  Values that` |
|       - | 5757 | `		 * cannot be sensibly converted (arrays, objects, resources) are` |
|       - | 5758 | `		 * rejected with a TypeError.  Scalars and null coerce to bool` |
|       - | 5759 | `		 * normally, matching PHP behaviour. */` |
|      45 | 5760 | `		if( ph7_value_is_array(apArg[2]) \|\|` |
|      34 | 5761 | `			ph7_value_is_object(apArg[2]) \|\|` |
|      20 | 5762 | `			ph7_value_is_resource(apArg[2]) ){` |
|       7 | 5763 | `			return PH7_VmThrowException(pCtx,` |
|       - | 5764 | `				"TypeError",` |
|       - | 5765 | `				"array_chunk(): Argument #3 ($preserve_keys) must be of type bool, %s given",` |
|       4 | 5766 | `				ph7_type_name(apArg[2])` |
|       - | 5767 | `				);` |
|       - | 5768 | `		}` |
|      21 | 5769 | `		bPreserve = ph7_value_to_bool(apArg[2]);` |
|      10 | 5770 | `	}` |
|       - | 5771 | `	/* Start processing */` |
|      27 | 5772 | `	pEntry = pMap->pFirst;` |
|      27 | 5773 | `	nChunk = 0;` |
|      27 | 5774 | `	pChunk = 0;` |
|      27 | 5775 | `	n = pMap->nEntry;` |
|      56 | 5776 | `	for( ;; ){` |
|     113 | 5777 | `		if( n < 1 ){` |
|       - | 5778 | `			/* When the loop terminates we may still have a current chunk` |
|       - | 5779 | `			 * that hasn't been added to the result array.  The previous` |
|       - | 5780 | `			 * implementation only pushed it if nChunk>0 which dropped the` |
|       - | 5781 | `			 * final chunk when the input size was an exact multiple of` |
|       - | 5782 | `			 * the chunk length.  Always append the pending chunk if it` |
|       - | 5783 | `			 * exists. */` |
|      27 | 5784 | `			if( pChunk ){` |
|      27 | 5785 | `				ph7_array_add_elem(pArray,0,pChunk); /* Will have its own copy */` |
|      13 | 5786 | `			}` |
|      27 | 5787 | `			break;` |
|       - | 5788 | `		}` |
|      87 | 5789 | `		if( nChunk < 1 ){` |
|      71 | 5790 | `			if( pChunk ){` |
|       - | 5791 | `				/* Put the first chunk */` |
|      45 | 5792 | `				ph7_array_add_elem(pArray,0,pChunk); /* Will have it's own copy */` |
|      22 | 5793 | `			}` |
|       - | 5794 | `			/* Create a new dimension */` |
|      71 | 5795 | `			pChunk = ph7_context_new_array(pCtx); /* Don't worry about freeing memory here,everything` |
|       - | 5796 | `												   * will be automatically released as soon we return` |
|       - | 5797 | `												   * from this function */` |
|      71 | 5798 | `			if( pChunk == 0 ){` |
|     ! 0 | 5799 | `				break;` |
|       - | 5800 | `			}` |
|      71 | 5801 | `			nChunk = nSize;` |
|      35 | 5802 | `		}` |
|       - | 5803 | `		/* Insert the entry */` |
|      87 | 5804 | `		HashmapInsertNode((ph7_hashmap *)pChunk->x.pOther,pEntry,bPreserve);` |
|       - | 5805 | `		/* Point to the next entry */` |
|      87 | 5806 | `		pEntry = pEntry->pPrev; /* Reverse link */` |
|      87 | 5807 | `		nChunk--;` |
|      87 | 5808 | `		n--;` |
|       1 | 5809 | `	}` |
|       - | 5810 | `	/* Return the multidimensional array */` |
|      27 | 5811 | `	ph7_result_value(pCtx,pArray);` |
|      27 | 5812 | `	return PH7_OK;` |
|      23 | 5813 |  |
|       - | 5814 | `/*` |
|       - | 5815 | ` * array array_pad(array $input,int $pad_size,value $pad_value)` |
|       - | 5816 | ` *  Pad array to the specified length with a value.` |
|       - | 5817 | ` * $input` |
|       - | 5818 | ` *   Initial array of values to pad.` |
|       - | 5819 | ` * $pad_size` |
|       - | 5820 | ` *   New size of the array.` |
|       - | 5821 | ` * $pad_value` |
|       - | 5822 | ` *   Value to pad if input is less than pad_size.` |
|       - | 5823 | ` */` |
|      28 | 5824 | `static int ph7_hashmap_pad(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       2 | 5825 |  |
|       - | 5826 | `	ph7_hashmap *pMap;` |
|       - | 5827 | `	ph7_value *pArray;` |
|       - | 5828 | `	int nEntry;` |
|      30 | 5829 | `	if( nArg != 3 ){` |
|      10 | 5830 | `		return PH7_VmThrowException(pCtx,` |
|       - | 5831 | `			"ArgumentCountError",` |
|       - | 5832 | `			"array_pad() expects exactly 3 arguments, %d given",` |
|       3 | 5833 | `			nArg` |
|       - | 5834 | `			);` |
|       - | 5835 | `	}` |
|      24 | 5836 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|       4 | 5837 | `		return PH7_VmThrowException(pCtx,` |
|       - | 5838 | `			"TypeError",` |
|       - | 5839 | `			"array_pad(): Argument #1 ($array) must be of type array, %s given",` |
|       1 | 5840 | `			ph7_type_name(apArg[0])` |
|       - | 5841 | `			);` |
|       - | 5842 | `	}` |
|       - | 5843 | `	/* Create a new array */` |
|      21 | 5844 | `	pArray = ph7_context_new_array(pCtx);` |
|      21 | 5845 | `	if( pArray == 0 ){` |
|     ! 0 | 5846 | `		ph7_result_null(pCtx);` |
|     ! 0 | 5847 | `		return PH7_OK;` |
|       - | 5848 | `	}` |
|       - | 5849 | `	/* Point to the internal representation of the input hashmap */` |
|      21 | 5850 | `	pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|       - | 5851 | `	/* Extract the total number of desired entry to insert */` |
|      21 | 5852 | `	nEntry = ph7_value_to_int(apArg[1]);` |
|      21 | 5853 | `	if( nEntry < 0 ){` |
|       9 | 5854 | `		nEntry = -nEntry;` |
|       9 | 5855 | `		if( nEntry > (int)pMap->nEntry ){` |
|       5 | 5856 | `			nEntry -= (int)pMap->nEntry;` |
|       - | 5857 | `			/* Insert given items first */` |
|      17 | 5858 | `			while( nEntry > 0 ){` |
|      13 | 5859 | `				ph7_array_add_elem(pArray,0,apArg[2]);` |
|      13 | 5860 | `				nEntry--;` |
|       1 | 5861 | `			}` |
|       - | 5862 | `			/* Merge the two arrays */` |
|       5 | 5863 | `			HashmapMerge(pMap,(ph7_hashmap *)pArray->x.pOther);` |
|       3 | 5864 | `		}else{` |
|       5 | 5865 | `			PH7_HashmapDup(pMap,(ph7_hashmap *)pArray->x.pOther);` |
|       1 | 5866 | `		}` |
|      17 | 5867 | `	}else if( nEntry > 0 ){` |
|      11 | 5868 | `		if( nEntry > (int)pMap->nEntry ){` |
|       7 | 5869 | `			nEntry -= (int)pMap->nEntry;` |
|       - | 5870 | `			/* Merge the two arrays first */` |
|       7 | 5871 | `			HashmapMerge(pMap,(ph7_hashmap *)pArray->x.pOther);` |
|       - | 5872 | `			/* Insert given items */` |
|      25 | 5873 | `			while( nEntry > 0 ){` |
|      19 | 5874 | `				ph7_array_add_elem(pArray,0,apArg[2]);` |
|      19 | 5875 | `				nEntry--;` |
|       1 | 5876 | `			}` |
|       4 | 5877 | `		}else{` |
|       5 | 5878 | `			PH7_HashmapDup(pMap,(ph7_hashmap *)pArray->x.pOther);` |
|       - | 5879 | `		}` |
|       6 | 5880 | `	}else{` |
|       - | 5881 | `		/* nEntry == 0: return a copy of the input array */` |
|       3 | 5882 | `		PH7_HashmapDup(pMap,(ph7_hashmap *)pArray->x.pOther);` |
|       - | 5883 | `	}` |
|       - | 5884 | `	/* Return the new array */` |
|      21 | 5885 | `	ph7_result_value(pCtx,pArray);` |
|      21 | 5886 | `	return PH7_OK;` |
|      16 | 5887 |  |
|       - | 5888 | `/*` |
|       - | 5889 | ` * array array_replace(array &$array,array &$array1,...)` |
|       - | 5890 | ` *  Replaces elements from passed arrays into the first array.` |
|       - | 5891 | ` * Parameters` |
|       - | 5892 | ` * $array` |
|       - | 5893 | ` *   The array in which elements are replaced.` |
|       - | 5894 | ` * $array1` |
|       - | 5895 | ` *   The array from which elements will be extracted.` |
|       - | 5896 | ` * ....` |
|       - | 5897 | ` *  More arrays from which elements will be extracted.` |
|       - | 5898 | ` *  Values from later arrays overwrite the previous values.` |
|       - | 5899 | ` * Return` |
|       - | 5900 | ` *  Returns an array, or NULL if an error occurs.` |
|       - | 5901 | ` */` |
|       2 | 5902 | `static int ph7_hashmap_replace(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       1 | 5903 |  |
|       - | 5904 | `	ph7_hashmap *pMap;` |
|       - | 5905 | `	ph7_value *pArray;` |
|       - | 5906 | `	int i;` |
|       3 | 5907 | `	if( nArg < 1 ){` |
|       - | 5908 | `		/* Invalid arguments,return NULL */` |
|     ! 0 | 5909 | `		ph7_result_null(pCtx);` |
|     ! 0 | 5910 | `		return PH7_OK;` |
|       - | 5911 | `	}` |
|       - | 5912 | `	/* Create a new array */` |
|       3 | 5913 | `	pArray = ph7_context_new_array(pCtx);` |
|       3 | 5914 | `	if( pArray == 0 ){` |
|     ! 0 | 5915 | `		ph7_result_null(pCtx);` |
|     ! 0 | 5916 | `		return PH7_OK;` |
|       - | 5917 | `	}` |
|       - | 5918 | `	/* Perform the requested operation */` |
|       7 | 5919 | `	for( i = 0 ; i < nArg ; i++ ){` |
|       5 | 5920 | `		if( !ph7_value_is_array(apArg[i]) ){` |
|     ! 0 | 5921 | `			continue;` |
|       - | 5922 | `		}` |
|       - | 5923 | `		/* Point to the internal representation of the input hashmap */` |
|       5 | 5924 | `		pMap = (ph7_hashmap *)apArg[i]->x.pOther;` |
|       5 | 5925 | `		HashmapOverwrite(pMap,(ph7_hashmap *)pArray->x.pOther);` |
|       3 | 5926 | `	}` |
|       - | 5927 | `	/* Return the new array */` |
|       3 | 5928 | `	ph7_result_value(pCtx,pArray);` |
|       3 | 5929 | `	return PH7_OK;` |
|       2 | 5930 |  |
|       - | 5931 | `/*` |
|       - | 5932 | ` * array array_filter(array $input [,callback $callback ])` |
|       - | 5933 | ` *  Filters elements of an array using a callback function.` |
|       - | 5934 | ` * Parameters` |
|       - | 5935 | ` *  $input` |
|       - | 5936 | ` *    The array to iterate over` |
|       - | 5937 | ` * $callback` |
|       - | 5938 | ` *    The callback function to use` |
|       - | 5939 | ` *    If no callback is supplied, all entries of input equal to FALSE (see converting to boolean)` |
|       - | 5940 | ` *    will be removed.` |
|       - | 5941 | ` * Return` |
|       - | 5942 | ` *  The filtered array.` |
|       - | 5943 | ` */` |
|      18 | 5944 | `static int ph7_hashmap_filter(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       2 | 5945 |  |
|       - | 5946 | `	ph7_hashmap_node *pEntry;` |
|       - | 5947 | `	ph7_hashmap *pMap;` |
|       - | 5948 | `	ph7_value *pArray;` |
|       - | 5949 | `	ph7_value sResult;   /* Callback result */` |
|       - | 5950 | `	ph7_value *pValue;` |
|       - | 5951 | `	sxi32 rc;` |
|       - | 5952 | `	int keep;` |
|       - | 5953 | `	sxu32 n;` |
|      20 | 5954 | `	if( nArg < 1 \|\| !ph7_value_is_array(apArg[0]) ){` |
|       - | 5955 | `		/* Invalid arguments,return NULL */` |
|       5 | 5956 | `		ph7_result_null(pCtx);` |
|       5 | 5957 | `		return PH7_OK;` |
|       - | 5958 | `	}` |
|       - | 5959 | `	/* Create a new array */` |
|      16 | 5960 | `	pArray = ph7_context_new_array(pCtx);` |
|      16 | 5961 | `	if( pArray == 0 ){` |
|     ! 0 | 5962 | `		ph7_result_null(pCtx);` |
|     ! 0 | 5963 | `		return PH7_OK;` |
|       - | 5964 | `	}` |
|       - | 5965 | `	/* Point to the internal representation of the input hashmap */` |
|      16 | 5966 | `	pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|      16 | 5967 | `	pEntry = pMap->pFirst;` |
|      16 | 5968 | `	PH7_MemObjInit(pMap->pVm,&sResult);` |
|      16 | 5969 | `	sResult.nIdx = SXU32_HIGH; /* Mark as constant */` |
|       - | 5970 | `	/* Perform the requested operation */` |
|      66 | 5971 | `	for( n = 0 ; n < pMap->nEntry ; n++ ){` |
|       - | 5972 | `		/* Extract node value (may be NULL if allocation failed) */` |
|      54 | 5973 | `		pValue = HashmapExtractNodeValue(pEntry);` |
|      54 | 5974 | `		if( pValue == 0 ){` |
|       - | 5975 | `			/* Can happen if SySetAt() failed earlier; drop the entry. */` |
|     ! 0 | 5976 | `			keep = FALSE;` |
|      54 | 5977 | `		}else if( nArg > 1 && !ph7_value_is_null(apArg[1]) ){` |
|       - | 5978 | `			/* Callback was supplied (not NULL).  PHP 8 throws a` |
|       - | 5979 | `				* TypeError when the value is not callable or null; prior PH7` |
|       - | 5980 | `				* silently dropped the element.  Emit similar message. */` |
|      26 | 5981 | `			if( !ph7_value_is_callable(apArg[1]) ){` |
|       3 | 5982 | `				if( ph7_value_is_string(apArg[1]) ){` |
|       - | 5983 | `					int len;` |
|       3 | 5984 | `					const char *zName = ph7_value_to_string(apArg[1], &len);` |
|       4 | 5985 | `					return PH7_VmThrowException(pCtx,` |
|       - | 5986 | `						"TypeError",` |
|       - | 5987 | `						"array_filter(): Argument #2 ($callback) must be a valid callback or null, function \"%s\" not found or invalid function name",` |
|       1 | 5988 | `						zName` |
|       - | 5989 | `						);` |
|     ! 0 | 5990 | `				}else{` |
|     ! 0 | 5991 | `					return PH7_VmThrowException(pCtx,` |
|       - | 5992 | `						"TypeError",` |
|       - | 5993 | `						"array_filter(): Argument #2 ($callback) must be a valid callback or null, %s given",` |
|     ! 0 | 5994 | `						ph7_type_name(apArg[1])` |
|       - | 5995 | `						);` |
|       - | 5996 | `				}` |
|       - | 5997 | `			}` |
|      23 | 5998 | `			keep = FALSE;` |
|      23 | 5999 | `			rc = PH7_VmCallUserFunction(pMap->pVm,apArg[1],1,&pValue,&sResult);` |
|      23 | 6000 | `			if( rc == SXRET_OK ){` |
|       - | 6001 | `				/* Perform a boolean cast */` |
|      23 | 6002 | `				keep = ph7_value_to_bool(&sResult);` |
|      11 | 6003 | `			}` |
|      23 | 6004 | `			PH7_MemObjRelease(&sResult);` |
|      12 | 6005 | `		}else{` |
|       - | 6006 | `			/* No callback provided or callback explicitly NULL: use default` |
|       - | 6007 | `			 * behaviour where "empty" values are removed. This also covers` |
|       - | 6008 | `			 * the case where the callback argument is missing entirely.` |
|       - | 6009 | `			 */` |
|      29 | 6010 | `			keep = !PH7_MemObjIsEmpty(pValue);` |
|       - | 6011 | `		}` |
|      51 | 6012 | `		if( keep ){` |
|       - | 6013 | `			/* Perform the insertion,now the callback returned true */` |
|      17 | 6014 | `			HashmapInsertNode((ph7_hashmap *)pArray->x.pOther,pEntry,TRUE);` |
|       8 | 6015 | `		}` |
|       - | 6016 | `		/* Point to the next entry */` |
|      51 | 6017 | `		pEntry = pEntry->pPrev; /* Reverse link */` |
|      26 | 6018 | `	}` |
|      13 | 6019 | `	ph7_result_value(pCtx,pArray);` |
|      13 | 6020 | `	return PH7_OK;` |
|      11 | 6021 |  |
|       - | 6022 | `/*` |
|       - | 6023 | ` * array array_map(?callable $callback, array $array)` |
|       - | 6024 | ` *  Applies the callback to the elements of the given array.` |
|       - | 6025 | ` * Parameters` |
|       - | 6026 | ` *  $callback` |
|       - | 6027 | ` *   A callable to run for each element in the array, or NULL for the` |
|       - | 6028 | ` *   identity function (returns the array unchanged).` |
|       - | 6029 | ` *  $array` |
|       - | 6030 | ` *   An array to run through the callback function.` |
|       - | 6031 | ` * Return` |
|       - | 6032 | ` *  Returns an array containing the results of applying the callback` |
|       - | 6033 | ` *  function to each element of $array.` |
|       - | 6034 | ` */` |
|      28 | 6035 | `static int ph7_hashmap_map(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       2 | 6036 |  |
|       - | 6037 | `	ph7_value *pArray,*pValue,sKey,sResult;` |
|       - | 6038 | `	ph7_hashmap_node *pEntry;` |
|       - | 6039 | `	ph7_hashmap *pMap;` |
|       - | 6040 | `	int bNullCallback;` |
|       - | 6041 | `	sxu32 n;` |
|      30 | 6042 | `	if( nArg < 2 ){` |
|       7 | 6043 | `		return PH7_VmThrowException(pCtx,` |
|       - | 6044 | `			"ArgumentCountError",` |
|       - | 6045 | `			"array_map() expects at least 2 arguments, %d given",` |
|       2 | 6046 | `			nArg` |
|       - | 6047 | `			);` |
|       - | 6048 | `	}` |
|      26 | 6049 | `	if( !ph7_value_is_array(apArg[1]) ){` |
|       4 | 6050 | `		return PH7_VmThrowException(pCtx,` |
|       - | 6051 | `			"TypeError",` |
|       - | 6052 | `			"array_map(): Argument #2 ($array) must be of type array, %s given",` |
|       2 | 6053 | `			ph7_type_name(apArg[1])` |
|       - | 6054 | `			);` |
|       - | 6055 | `	}` |
|      24 | 6056 | `	bNullCallback = ph7_value_is_null(apArg[0]);` |
|      24 | 6057 | `	if( !bNullCallback && !ph7_value_is_callable(apArg[0]) ){` |
|       5 | 6058 | `		if( ph7_value_is_string(apArg[0]) ){` |
|       3 | 6059 | `			const char *zFunc = ph7_value_to_string(apArg[0],0);` |
|       4 | 6060 | `			return PH7_VmThrowException(pCtx,` |
|       - | 6061 | `				"TypeError",` |
|       - | 6062 | `				"array_map(): Argument #1 ($callback) must be a valid callback or null, "` |
|       - | 6063 | `				"function \"%s\" not found or invalid function name",` |
|       1 | 6064 | `				zFunc` |
|       - | 6065 | `				);` |
|       - | 6066 | `		}` |
|       3 | 6067 | `		return PH7_VmThrowException(pCtx,` |
|       - | 6068 | `			"TypeError",` |
|       - | 6069 | `			"array_map(): Argument #1 ($callback) must be a valid callback or null, "` |
|       - | 6070 | `			"no array or string given"` |
|       - | 6071 | `			);` |
|       - | 6072 | `	}` |
|       - | 6073 | `	/* Create a new array */` |
|      19 | 6074 | `	pArray = ph7_context_new_array(pCtx);` |
|      19 | 6075 | `	if( pArray == 0 ){` |
|     ! 0 | 6076 | `		ph7_result_null(pCtx);` |
|     ! 0 | 6077 | `		return PH7_OK;` |
|       - | 6078 | `	}` |
|       - | 6079 | `	/* Point to the internal representation of the input hashmap */` |
|      19 | 6080 | `	pMap = (ph7_hashmap *)apArg[1]->x.pOther;` |
|      19 | 6081 | `	PH7_MemObjInit(pMap->pVm,&sResult);` |
|      19 | 6082 | `	PH7_MemObjInit(pMap->pVm,&sKey);` |
|      19 | 6083 | `	sResult.nIdx = SXU32_HIGH; /* Mark as constant */` |
|      19 | 6084 | `	sKey.nIdx    = SXU32_HIGH; /* Mark as constant */` |
|       - | 6085 | `	/* Perform the requested operation */` |
|      19 | 6086 | `	pEntry = pMap->pFirst;` |
|      53 | 6087 | `	for( n = 0 ; n < pMap->nEntry ; n++ ){` |
|       - | 6088 | `		/* Extract the node value */` |
|      35 | 6089 | `		pValue = HashmapExtractNodeValue(pEntry);` |
|      35 | 6090 | `		if( pValue ){` |
|       - | 6091 | `			/* Extract the node key */` |
|      35 | 6092 | `			PH7_HashmapExtractNodeKey(pEntry,&sKey);` |
|      35 | 6093 | `			if( bNullCallback ){` |
|       - | 6094 | `				/* NULL callback: identity function, keep original value */` |
|      11 | 6095 | `				ph7_array_add_elem(pArray,&sKey,pValue);` |
|       6 | 6096 | `			}else{` |
|       - | 6097 | `				/* Invoke the supplied callback */` |
|      25 | 6098 | `				PH7_VmCallUserFunction(pMap->pVm,apArg[0],1,&pValue,&sResult);` |
|       - | 6099 | `				/* Insert the callback return value */` |
|      25 | 6100 | `				ph7_array_add_elem(pArray,&sKey,&sResult);` |
|       - | 6101 | `			}` |
|      35 | 6102 | `			PH7_MemObjRelease(&sKey);` |
|      35 | 6103 | `			PH7_MemObjRelease(&sResult);` |
|      17 | 6104 | `		}` |
|       - | 6105 | `		/* Point to the next entry */` |
|      35 | 6106 | `		pEntry = pEntry->pPrev; /* Reverse link */` |
|      18 | 6107 | `	}` |
|      19 | 6108 | `	ph7_result_value(pCtx,pArray);` |
|      19 | 6109 | `	return PH7_OK;` |
|      16 | 6110 |  |
|       - | 6111 | `/*` |
|       - | 6112 | ` * value array_reduce(array $input,callback $function[, value $initial = NULL ])` |
|       - | 6113 | ` *  Iteratively reduce the array to a single value using a callback function.` |
|       - | 6114 | ` * Parameters` |
|       - | 6115 | ` *  $input` |
|       - | 6116 | ` *   The input array.` |
|       - | 6117 | ` *  $function` |
|       - | 6118 | ` *  The callback function.` |
|       - | 6119 | ` * $initial` |
|       - | 6120 | ` *  If the optional initial is available, it will be used at the beginning` |
|       - | 6121 | ` *  of the process, or as a final result in case the array is empty.` |
|       - | 6122 | ` * Return` |
|       - | 6123 | ` *  Returns the resulting value.` |
|       - | 6124 | ` *  If the array is empty and initial is not passed, array_reduce() returns NULL.` |
|       - | 6125 | ` */` |
|       4 | 6126 | `static int ph7_hashmap_reduce(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       1 | 6127 |  |
|       - | 6128 | `	ph7_hashmap_node *pEntry;` |
|       - | 6129 | `	ph7_hashmap *pMap;` |
|       - | 6130 | `	ph7_value *pValue;` |
|       - | 6131 | `	ph7_value sResult;` |
|       - | 6132 | `	sxu32 n;` |
|       5 | 6133 | `	if( nArg < 2 \|\| !ph7_value_is_array(apArg[0]) ){` |
|       - | 6134 | `		/* Invalid/Missing arguments,return NULL */` |
|     ! 0 | 6135 | `		ph7_result_null(pCtx);` |
|     ! 0 | 6136 | `		return PH7_OK;` |
|       - | 6137 | `	}` |
|       - | 6138 | `	/* Point to the internal representation of the input hashmap */` |
|       5 | 6139 | `	pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|       - | 6140 | `	/* Assume a NULL initial value */` |
|       5 | 6141 | `	PH7_MemObjInit(pMap->pVm,&sResult);` |
|       5 | 6142 | `	sResult.nIdx = SXU32_HIGH; /* Mark as constant */` |
|       5 | 6143 | `	if( nArg > 2 ){` |
|       - | 6144 | `		/* Set the initial value */` |
|       5 | 6145 | `		PH7_MemObjLoad(apArg[2],&sResult);` |
|       2 | 6146 | `	}` |
|       - | 6147 | `	/* Perform the requested operation */` |
|       5 | 6148 | `	pEntry = pMap->pFirst;` |
|      19 | 6149 | `	for( n = 0 ; n < pMap->nEntry ; n++ ){` |
|       - | 6150 | `		/* Extract the node value */` |
|      15 | 6151 | `		pValue = HashmapExtractNodeValue(pEntry);` |
|       - | 6152 | `		/* Invoke the supplied callback */` |
|      15 | 6153 | `		PH7_VmCallUserFunctionAp(pMap->pVm,apArg[1],&sResult,&sResult,pValue,0);` |
|       - | 6154 | `		/* Point to the next entry */` |
|      15 | 6155 | `		pEntry = pEntry->pPrev; /* Reverse link */` |
|       8 | 6156 | `	}` |
|       5 | 6157 | `	ph7_result_value(pCtx,&sResult); /* Will make it's own copy */` |
|       5 | 6158 | `	PH7_MemObjRelease(&sResult);` |
|       5 | 6159 | `	return PH7_OK;` |
|       3 | 6160 |  |
|       - | 6161 | `/*` |
|       - | 6162 | ` * bool array_walk(array &$array,callback $funcname [, value $userdata ] )` |
|       - | 6163 | ` *  Apply a user function to every member of an array.` |
|       - | 6164 | ` * Parameters` |
|       - | 6165 | ` *  $array` |
|       - | 6166 | ` *   The input array.` |
|       - | 6167 | ` * $funcname` |
|       - | 6168 | ` *  Typically, funcname takes on two parameters.The array parameter's value being` |
|       - | 6169 | ` *  the first, and the key/index second.` |
|       - | 6170 | ` * Note:` |
|       - | 6171 | ` *  If funcname needs to be working with the actual values of the array,specify the first` |
|       - | 6172 | ` *  parameter of funcname as a reference. Then, any changes made to those elements will` |
|       - | 6173 | ` *  be made in the original array itself.` |
|       - | 6174 | ` * $userdata` |
|       - | 6175 | ` *  If the optional userdata parameter is supplied, it will be passed as the third parameter` |
|       - | 6176 | ` *  to the callback funcname.` |
|       - | 6177 | ` * Return` |
|       - | 6178 | ` *  Returns TRUE on success or FALSE on failure.` |
|       - | 6179 | ` */` |
|      12 | 6180 | `static int ph7_hashmap_walk(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       1 | 6181 |  |
|       - | 6182 | `	ph7_value *pValue,*pUserData,sKey;` |
|       - | 6183 | `	ph7_hashmap_node *pEntry;` |
|       - | 6184 | `	ph7_hashmap *pMap;` |
|       - | 6185 | `	sxi32 rc;` |
|       - | 6186 | `	sxu32 n;` |
|      13 | 6187 | `	if( nArg < 2 \|\| !ph7_value_is_array(apArg[0]) ){` |
|       - | 6188 | `		/* Invalid/Missing arguments,return FALSE */` |
|     ! 0 | 6189 | `		ph7_result_bool(pCtx,0);` |
|     ! 0 | 6190 | `		return PH7_OK;` |
|       - | 6191 | `	}` |
|      13 | 6192 | `	pUserData = nArg > 2 ? apArg[2] : 0;` |
|       - | 6193 | `	/* Point to the internal representation of the input hashmap */` |
|      13 | 6194 | `	pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|      13 | 6195 | `	PH7_MemObjInit(pMap->pVm,&sKey);` |
|      13 | 6196 | `	sKey.nIdx = SXU32_HIGH; /* Mark as constant */` |
|       - | 6197 | `	/* Perform the desired operation */` |
|      13 | 6198 | `	pEntry = pMap->pFirst;` |
|      41 | 6199 | `	for( n = 0 ; n < pMap->nEntry ; n++ ){` |
|       - | 6200 | `		/* Extract the node value */` |
|      29 | 6201 | `		pValue = HashmapExtractNodeValue(pEntry);` |
|      29 | 6202 | `		if( pValue ){` |
|       - | 6203 | `			/* Extract the entry key */` |
|      29 | 6204 | `			PH7_HashmapExtractNodeKey(pEntry,&sKey);` |
|       - | 6205 | `			/* Invoke the supplied callback */` |
|      29 | 6206 | `			rc = PH7_VmCallUserFunctionAp(pMap->pVm,apArg[1],0,pValue,&sKey,pUserData,0);` |
|      29 | 6207 | `			PH7_MemObjRelease(&sKey);` |
|      29 | 6208 | `			if( rc != SXRET_OK ){` |
|       - | 6209 | `				/* An error occured while invoking the supplied callback [i.e: not defined] */` |
|     ! 0 | 6210 | `				ph7_result_bool(pCtx,0); /* return FALSE */` |
|     ! 0 | 6211 | `				return PH7_OK;` |
|       - | 6212 | `			}` |
|      14 | 6213 | `		}` |
|       - | 6214 | `		/* Point to the next entry */` |
|      29 | 6215 | `		pEntry = pEntry->pPrev; /* Reverse link */` |
|      15 | 6216 | `	}` |
|       - | 6217 | `	/* All done,return TRUE */` |
|      13 | 6218 | `	ph7_result_bool(pCtx,1);` |
|      13 | 6219 | `	return PH7_OK;` |
|       7 | 6220 |  |
|       - | 6221 | `/*` |
|       - | 6222 | ` * Apply a user function to every member of an array.(Recurse on array's).` |
|       - | 6223 | ` * Refer to the [array_walk_recursive()] implementation for more information.` |
|       - | 6224 | ` */` |
|       6 | 6225 | `static int HashmapWalkRecursive(` |
|       - | 6226 | `	ph7_hashmap *pMap,    /* Target hashmap */` |
|       - | 6227 | `	ph7_value *pCallback, /* User callback */` |
|       - | 6228 | `	ph7_value *pUserData, /* Callback private data */` |
|       - | 6229 | `	int iNest             /* Nesting level */` |
|       - | 6230 | `	)` |
|       1 | 6231 |  |
|       - | 6232 | `	ph7_hashmap_node *pEntry;` |
|       - | 6233 | `	ph7_value *pValue,sKey;` |
|       - | 6234 | `	sxi32 rc;` |
|       - | 6235 | `	sxu32 n;` |
|       - | 6236 | `	/* Iterate throw hashmap entries */` |
|       7 | 6237 | `	PH7_MemObjInit(pMap->pVm,&sKey);` |
|       7 | 6238 | `	sKey.nIdx = SXU32_HIGH; /* Mark as constant */` |
|       7 | 6239 | `	pEntry = pMap->pFirst;` |
|      17 | 6240 | `	for( n = 0 ; n < pMap->nEntry ; n++ ){` |
|       - | 6241 | `		/* Extract the node value */` |
|      11 | 6242 | `		pValue = HashmapExtractNodeValue(pEntry);` |
|      11 | 6243 | `		if( pValue ){` |
|      11 | 6244 | `			if( pValue->iFlags & MEMOBJ_HASHMAP ){` |
|       5 | 6245 | `				if( iNest < 32 ){` |
|       - | 6246 | `					/* Recurse */` |
|       5 | 6247 | `					iNest++;` |
|       5 | 6248 | `					HashmapWalkRecursive((ph7_hashmap *)pValue->x.pOther,pCallback,pUserData,iNest);` |
|       5 | 6249 | `					iNest--;` |
|       2 | 6250 | `				}` |
|       3 | 6251 | `			}else{` |
|       - | 6252 | `				/* Extract the node key */` |
|       7 | 6253 | `				PH7_HashmapExtractNodeKey(pEntry,&sKey);` |
|       - | 6254 | `				/* Invoke the supplied callback */` |
|       7 | 6255 | `				rc = PH7_VmCallUserFunctionAp(pMap->pVm,pCallback,0,pValue,&sKey,pUserData,0);` |
|       7 | 6256 | `				PH7_MemObjRelease(&sKey);` |
|       7 | 6257 | `				if( rc != SXRET_OK ){` |
|     ! 0 | 6258 | `					return rc;` |
|       - | 6259 | `				}` |
|       - | 6260 | `			}` |
|       5 | 6261 | `		}` |
|       - | 6262 | `		/* Point to the next entry */` |
|      11 | 6263 | `		pEntry = pEntry->pPrev; /* Reverse link */` |
|       6 | 6264 | `	}` |
|       7 | 6265 | `	return SXRET_OK;` |
|       4 | 6266 |  |
|       - | 6267 | `/*` |
|       - | 6268 | ` * bool array_walk_recursive(array &$array,callback $funcname [, value $userdata ] )` |
|       - | 6269 | ` *  Apply a user function recursively to every member of an array.` |
|       - | 6270 | ` * Parameters` |
|       - | 6271 | ` *  $array` |
|       - | 6272 | ` *   The input array.` |
|       - | 6273 | ` * $funcname` |
|       - | 6274 | ` *  Typically, funcname takes on two parameters.The array parameter's value being` |
|       - | 6275 | ` *  the first, and the key/index second.` |
|       - | 6276 | ` * Note:` |
|       - | 6277 | ` *  If funcname needs to be working with the actual values of the array,specify the first` |
|       - | 6278 | ` *  parameter of funcname as a reference. Then, any changes made to those elements will` |
|       - | 6279 | ` *  be made in the original array itself.` |
|       - | 6280 | ` * $userdata` |
|       - | 6281 | ` *  If the optional userdata parameter is supplied, it will be passed as the third parameter` |
|       - | 6282 | ` *  to the callback funcname.` |
|       - | 6283 | ` * Return` |
|       - | 6284 | ` *  Returns TRUE on success or FALSE on failure.` |
|       - | 6285 | ` */` |
|       2 | 6286 | `static int ph7_hashmap_walk_recursive(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       1 | 6287 |  |
|       - | 6288 | `	ph7_hashmap *pMap;` |
|       - | 6289 | `	sxi32 rc;` |
|       3 | 6290 | `	if( nArg < 2 \|\| !ph7_value_is_array(apArg[0]) ){` |
|       - | 6291 | `		/* Invalid/Missing arguments,return FALSE */` |
|     ! 0 | 6292 | `		ph7_result_bool(pCtx,0);` |
|     ! 0 | 6293 | `		return PH7_OK;` |
|       - | 6294 | `	}` |
|       - | 6295 | `	/* Point to the internal representation of the input hashmap */` |
|       3 | 6296 | `	pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|       - | 6297 | `	/* Perform the desired operation */` |
|       3 | 6298 | `	rc = HashmapWalkRecursive(pMap,apArg[1],nArg > 2 ? apArg[2] : 0,0);` |
|       - | 6299 | `	/* All done */` |
|       3 | 6300 | `	ph7_result_bool(pCtx,rc == SXRET_OK);` |
|       3 | 6301 | `	return PH7_OK;` |
|       2 | 6302 |  |
|       - | 6303 | `/*` |
|       - | 6304 | ` * Table of hashmap functions.` |
|       - | 6305 | ` */` |
|       - | 6306 | `static const ph7_builtin_func aHashmapFunc[] = {` |
|       - | 6307 | `	{"count",             ph7_hashmap_count },` |
|       - | 6308 | `	{"sizeof",            ph7_hashmap_count },` |
|       - | 6309 | `	{"array_key_exists",  ph7_hashmap_key_exists },` |
|       - | 6310 | `	{"array_pop",         ph7_hashmap_pop     },` |
|       - | 6311 | `	{"array_push",        ph7_hashmap_push    },` |
|       - | 6312 | `	{"array_shift",       ph7_hashmap_shift   },` |
|       - | 6313 | `	{"array_product",     ph7_hashmap_product },` |
|       - | 6314 | `	{"array_sum",         ph7_hashmap_sum     },` |
|       - | 6315 | `	{"array_keys",        ph7_hashmap_keys    },` |
|       - | 6316 | `	{"array_values",      ph7_hashmap_values  },` |
|       - | 6317 | `	{"array_same",        ph7_hashmap_same    },  /* Symisc eXtension */` |
|       - | 6318 | `	{"array_merge",       ph7_hashmap_merge   },` |
|       - | 6319 | `	{"array_slice",       ph7_hashmap_slice   },` |
|       - | 6320 | `	{"array_splice",      ph7_hashmap_splice  },` |
|       - | 6321 | `	{"array_search",      ph7_hashmap_search  },` |
|       - | 6322 | `	{"array_diff",        ph7_hashmap_diff    },` |
|       - | 6323 | `	{"array_udiff",       ph7_hashmap_udiff   },` |
|       - | 6324 | `	{"array_diff_assoc",  ph7_hashmap_diff_assoc },` |
|       - | 6325 | `	{"array_diff_uassoc", ph7_hashmap_diff_uassoc },` |
|       - | 6326 | `	{"array_diff_key",    ph7_hashmap_diff_key },` |
|       - | 6327 | `	{"array_intersect",   ph7_hashmap_intersect},` |
|       - | 6328 | `	{"array_intersect_assoc", ph7_hashmap_intersect_assoc},` |
|       - | 6329 | `	{"array_uintersect",  ph7_hashmap_uintersect},` |
|       - | 6330 | `	{"array_intersect_key",   ph7_hashmap_intersect_key},` |
|       - | 6331 | `	{"array_copy",        ph7_hashmap_copy    },` |
|       - | 6332 | `	{"array_erase",       ph7_hashmap_erase   },` |
|       - | 6333 | `	{"array_fill",        ph7_hashmap_fill    },` |
|       - | 6334 | `	{"array_fill_keys",   ph7_hashmap_fill_keys},` |
|       - | 6335 | `	{"array_combine",     ph7_hashmap_combine },` |
|       - | 6336 | `	{"array_reverse",     ph7_hashmap_reverse },` |
|       - | 6337 | `	{"array_unique",      ph7_hashmap_unique  },` |
|       - | 6338 | `	{"array_flip",        ph7_hashmap_flip    },` |
|       - | 6339 | `	{"array_rand",        ph7_hashmap_rand    },` |
|       - | 6340 | `	{"array_chunk",       ph7_hashmap_chunk   },` |
|       - | 6341 | `	{"array_pad",         ph7_hashmap_pad     },` |
|       - | 6342 | `	{"array_replace",     ph7_hashmap_replace },` |
|       - | 6343 | `	{"array_filter",      ph7_hashmap_filter  },` |
|       - | 6344 | `	{"array_map",         ph7_hashmap_map     },` |
|       - | 6345 | `	{"array_reduce",      ph7_hashmap_reduce  },` |
|       - | 6346 | `	{"array_walk",        ph7_hashmap_walk    },` |
|       - | 6347 | `	{"array_walk_recursive", ph7_hashmap_walk_recursive },` |
|       - | 6348 | `	{"in_array",          ph7_hashmap_in_array},` |
|       - | 6349 | `	{"sort",              ph7_hashmap_sort    },` |
|       - | 6350 | `	{"asort",             ph7_hashmap_asort   },` |
|       - | 6351 | `	{"arsort",            ph7_hashmap_arsort  },` |
|       - | 6352 | `	{"ksort",             ph7_hashmap_ksort   },` |
|       - | 6353 | `	{"krsort",            ph7_hashmap_krsort  },` |
|       - | 6354 | `	{"rsort",             ph7_hashmap_rsort   },` |
|       - | 6355 | `	{"usort",             ph7_hashmap_usort   },` |
|       - | 6356 | `	{"uasort",            ph7_hashmap_uasort  },` |
|       - | 6357 | `	{"uksort",            ph7_hashmap_uksort  },` |
|       - | 6358 | `	{"shuffle",           ph7_hashmap_shuffle },` |
|       - | 6359 | `	{"range",             ph7_hashmap_range   },` |
|       - | 6360 | `	{"current",           ph7_hashmap_current },` |
|       - | 6361 | `	{"each",              ph7_hashmap_each    },` |
|       - | 6362 | `	{"pos",               ph7_hashmap_current },` |
|       - | 6363 | `	{"next",              ph7_hashmap_next    },` |
|       - | 6364 | `	{"prev",              ph7_hashmap_prev    },` |
|       - | 6365 | `	{"end",               ph7_hashmap_end     },` |
|       - | 6366 | `	{"reset",             ph7_hashmap_reset   },` |
|       - | 6367 | `	{"key",               ph7_hashmap_simple_key }` |
|       - | 6368 | `};` |
|       - | 6369 | `/*` |
|       - | 6370 | ` * Register the built-in hashmap functions defined above.` |
|       - | 6371 | ` */` |
|    1472 | 6372 | `PH7_PRIVATE void PH7_RegisterHashmapFunctions(ph7_vm *pVm)` |
|       2 | 6373 |  |
|       - | 6374 | `	sxu32 n;` |
|   91266 | 6375 | `	for( n = 0 ; n < SX_ARRAYSIZE(aHashmapFunc) ; n++ ){` |
|   89794 | 6376 | `		ph7_create_function(&(*pVm),aHashmapFunc[n].zName,aHashmapFunc[n].xFunc,0);` |
|   44898 | 6377 | `	}` |
|    1474 | 6378 |  |
|       - | 6379 | `/*` |
|       - | 6380 | ` * Dump a hashmap instance and it's entries and the store the dump in` |
|       - | 6381 | ` * the BLOB given as the first argument.` |
|       - | 6382 | ` * This function is typically invoked when the user issue a call to` |
|       - | 6383 | ` * [var_dump(),var_export(),print_r(),...]` |
|       - | 6384 | ` * This function SXRET_OK on success. Any other return value including` |
|       - | 6385 | ` * SXERR_LIMIT(infinite recursion) indicates failure.` |
|       - | 6386 | ` */` |
|      26 | 6387 | `PH7_PRIVATE sxi32 PH7_HashmapDump(SyBlob *pOut,ph7_hashmap *pMap,int ShowType,int nTab,int nDepth)` |
|       2 | 6388 |  |
|       - | 6389 | `	ph7_hashmap_node *pEntry;` |
|       - | 6390 | `	ph7_value *pObj;` |
|      28 | 6391 | `	sxu32 n = 0;` |
|       - | 6392 | `	int isRef;` |
|       - | 6393 | `	sxi32 rc;` |
|       - | 6394 | `	int i;` |
|      28 | 6395 | `	if( nDepth > 31 ){` |
|       - | 6396 | `		static const char zInfinite[] = "Nesting limit reached: Infinite recursion?";` |
|       - | 6397 | `		/* Nesting limit reached */` |
|     ! 0 | 6398 | `		SyBlobAppend(&(*pOut),zInfinite,sizeof(zInfinite)-1);` |
|     ! 0 | 6399 | `		if( ShowType ){` |
|     ! 0 | 6400 | `			SyBlobAppend(&(*pOut),")",sizeof(char));` |
|     ! 0 | 6401 | `		}` |
|     ! 0 | 6402 | `		return SXERR_LIMIT;` |
|       - | 6403 | `	}` |
|       - | 6404 | `	/* Point to the first inserted entry */` |
|      28 | 6405 | `	pEntry = pMap->pFirst;` |
|      28 | 6406 | `	rc = SXRET_OK;` |
|      28 | 6407 | `	if( !ShowType ){` |
|      15 | 6408 | `		SyBlobAppend(&(*pOut),"Array(",sizeof("Array(")-1);` |
|       7 | 6409 | `	}` |
|       - | 6410 | `	/* Total entries */` |
|      28 | 6411 | `	SyBlobFormat(&(*pOut),"%u) {",pMap->nEntry);` |
|       - | 6412 | `#ifdef __WINNT__` |
|       2 | 6413 | `	SyBlobAppend(&(*pOut),"\r\n",sizeof("\r\n")-1);` |
|       - | 6414 | `#else` |
|      26 | 6415 | `	SyBlobAppend(&(*pOut),"\n",sizeof(char));` |
|       - | 6416 | `#endif` |
|      62 | 6417 | `	for(;;){` |
|     126 | 6418 | `		if( n >= pMap->nEntry ){` |
|      28 | 6419 | `			break;` |
|       - | 6420 | `		}` |
|     198 | 6421 | `		for( i = 0 ; i < nTab ; i++ ){` |
|     100 | 6422 | `			SyBlobAppend(&(*pOut)," ",sizeof(char));` |
|      51 | 6423 | `		}` |
|       - | 6424 | `		/* Dump key */` |
|     100 | 6425 | `		if( pEntry->iType == HASHMAP_INT_NODE){` |
|      33 | 6426 | `			SyBlobFormat(&(*pOut),"[%qd] =>",pEntry->xKey.iKey);` |
|      17 | 6427 | `		}else{` |
|     101 | 6428 | `			SyBlobFormat(&(*pOut),"[%.*s] =>",` |
|      33 | 6429 | `				SyBlobLength(&pEntry->xKey.sKey),SyBlobData(&pEntry->xKey.sKey));` |
|       - | 6430 | `		}` |
|       - | 6431 | `#ifdef __WINNT__` |
|       2 | 6432 | `		SyBlobAppend(&(*pOut),"\r\n",sizeof("\r\n")-1);` |
|       - | 6433 | `#else` |
|      98 | 6434 | `		SyBlobAppend(&(*pOut),"\n",sizeof(char));` |
|       - | 6435 | `#endif` |
|       - | 6436 | `		/* Dump node value */` |
|     100 | 6437 | `		pObj = HashmapExtractNodeValue(pEntry);` |
|     100 | 6438 | `		isRef = 0;` |
|     100 | 6439 | `		if( pObj ){` |
|     100 | 6440 | `			if( pEntry->iFlags & HASHMAP_NODE_FOREIGN_OBJ ){` |
|       - | 6441 | `				/* Referenced object */` |
|     ! 0 | 6442 | `				isRef = 1;` |
|     ! 0 | 6443 | `			}` |
|     100 | 6444 | `			rc = PH7_MemObjDump(&(*pOut),pObj,ShowType,nTab+1,nDepth,isRef);` |
|     100 | 6445 | `			if( rc == SXERR_LIMIT ){` |
|     ! 0 | 6446 | `				break;` |
|       - | 6447 | `			}` |
|      49 | 6448 | `		}` |
|       - | 6449 | `		/* Point to the next entry */` |
|     100 | 6450 | `		n++;` |
|     100 | 6451 | `		pEntry = pEntry->pPrev; /* Reverse link */` |
|       2 | 6452 | `	}` |
|      54 | 6453 | `	for( i = 0 ; i < nTab ; i++ ){` |
|      28 | 6454 | `		SyBlobAppend(&(*pOut)," ",sizeof(char));` |
|      15 | 6455 | `	}` |
|      28 | 6456 | `	SyBlobAppend(&(*pOut),"}",sizeof(char));` |
|      28 | 6457 | `	return rc;` |
|      15 | 6458 |  |
|       - | 6459 | `/*` |
|       - | 6460 | ` * Iterate throw hashmap entries and invoke the given callback [i.e: xWalk()] for each` |
|       - | 6461 | ` * retrieved entry.` |
|       - | 6462 | ` * Note that argument are passed to the callback by copy. That is,any modification to` |
|       - | 6463 | ` * the entry value in the callback body will not alter the real value.` |
|       - | 6464 | ` * If the callback wishes to abort processing [i.e: it's invocation] it must return` |
|       - | 6465 | ` * a value different from PH7_OK.` |
|       - | 6466 | ` * Refer to [ph7_array_walk()] for more information.` |
|       - | 6467 | ` */` |
|   20302 | 6468 | `PH7_PRIVATE sxi32 PH7_HashmapWalk(` |
|       - | 6469 | `	ph7_hashmap *pMap, /* Target hashmap */` |
|       - | 6470 | `	int (*xWalk)(ph7_value *,ph7_value *,void *), /* Walker callback */` |
|       - | 6471 | `	void *pUserData /* Last argument to xWalk() */` |
|       - | 6472 | `	)` |
|       2 | 6473 |  |
|       - | 6474 | `	ph7_hashmap_node *pEntry;` |
|       - | 6475 | `	ph7_value sKey,sValue;` |
|       - | 6476 | `	sxi32 rc;` |
|       - | 6477 | `	sxu32 n;` |
|       - | 6478 | `	/* Initialize walker parameter */` |
|   20304 | 6479 | `	rc = SXRET_OK;` |
|   20304 | 6480 | `	PH7_MemObjInit(pMap->pVm,&sKey);` |
|   20304 | 6481 | `	PH7_MemObjInit(pMap->pVm,&sValue);` |
|   20304 | 6482 | `	n = pMap->nEntry;` |
|   20304 | 6483 | `	pEntry = pMap->pFirst;` |
|       - | 6484 | `	/* Start the iteration process */` |
|   53238 | 6485 | `	for(;;){` |
|  106478 | 6486 | `		if( n < 1 ){` |
|   20304 | 6487 | `			break;` |
|       - | 6488 | `		}` |
|       - | 6489 | `		/* Extract a copy of the key and a copy the current value */` |
|   86176 | 6490 | `		PH7_HashmapExtractNodeKey(pEntry,&sKey);` |
|   86176 | 6491 | `		PH7_HashmapExtractNodeValue(pEntry,&sValue,FALSE);` |
|       - | 6492 | `		/* Invoke the user callback */` |
|   86176 | 6493 | `		rc = xWalk(&sKey,&sValue,pUserData);` |
|       - | 6494 | `		/* Release the copy of the key and the value */` |
|   86176 | 6495 | `		PH7_MemObjRelease(&sKey);` |
|   86176 | 6496 | `		PH7_MemObjRelease(&sValue);` |
|   86176 | 6497 | `		if( rc != PH7_OK ){` |
|       - | 6498 | `			/* Callback request an operation abort */` |
|     ! 0 | 6499 | `			return SXERR_ABORT;` |
|       - | 6500 | `		}` |
|       - | 6501 | `		/* Point to the next entry */` |
|   86176 | 6502 | `		pEntry = pEntry->pPrev; /* Reverse link */` |
|   86176 | 6503 | `		n--;` |
|       2 | 6504 | `	}` |
|       - | 6505 | `	/* All done */` |
|   20304 | 6506 | `	return SXRET_OK;` |
|   10153 | 6507 |  |
|       - | 6508 |  |
