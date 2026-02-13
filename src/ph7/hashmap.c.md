# src/ph7/hashmap.c

<style>code, pre { background: none !important; white-space: pre !important; width: 100% !important; display: inline-block !important; } td { border: none !important; margin-top: 0 !important; margin-bottom: 0 !important; padding-top: 0 !important; padding-bottom: 0 !important; }</style>

Coverage: 2320/2882 lines (80.50%)

[Root index](../../index.md) | [Directory index](index.md)

|   Hits | Line | Source |
| -----: | ---: | :--- |
|      - |    1 | `/**` |
|      - |    2 | ` * SPDX-FileCopyrightText: 2011, 2012, 2013, 2014 Symisc Systems <licensing@symisc.net>` |
|      - |    3 | ` * SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>` |
|      - |    4 | ` * SPDX-License-Identifier: BSD-3-Clause` |
|      - |    5 | ` */` |
|      - |    6 | `#include "ph7int.h"` |
|      - |    7 | `/* This file implement generic hashmaps known as 'array' in the PHP world */` |
|      - |    8 | `/* Allowed node types */` |
|      - |    9 | `#define HASHMAP_INT_NODE   1  /* Node with an int [i.e: 64-bit integer] key */` |
|      - |   10 | `#define HASHMAP_BLOB_NODE  2  /* Node with a string/BLOB key */` |
|      - |   11 | `/* Node control flags */` |
|      - |   12 | `#define HASHMAP_NODE_FOREIGN_OBJ 0x001 /* Node hold a reference to a foreign ph7_value` |
|      - |   13 | `                                        * [i.e: array(&var)/$a[] =& $var ]` |
|      - |   14 | `										*/` |
|      - |   15 | `/*` |
|      - |   16 | ` * Default hash function for int [i.e; 64-bit integer] keys.` |
|      - |   17 | ` */` |
| 535712 |   18 | `static sxu32 IntHash(sxi64 iKey)` |
|      2 |   19 |  |
| 535714 |   20 | `	return (sxu32)(iKey ^ (iKey << 8) ^ (iKey >> 8));` |
|      2 |   21 |  |
|      - |   22 | `/*` |
|      - |   23 | ` * Default hash function for string/BLOB keys.` |
|      - |   24 | ` */` |
| 191580 |   25 | `static sxu32 BinHash(const void *pSrc,sxu32 nLen)` |
|      2 |   26 |  |
| 191582 |   27 | `	register unsigned char *zIn = (unsigned char *)pSrc;` |
|      - |   28 | `	unsigned char *zEnd;` |
| 191582 |   29 | `	sxu32 nH = 5381;` |
| 191582 |   30 | `	zEnd = &zIn[nLen];` |
| 224199 |   31 | `	for(;;){` |
| 448400 |   32 | `		if( zIn >= zEnd ){ break; } nH = nH * 33 + zIn[0] ; zIn++;` |
| 408670 |   33 | `		if( zIn >= zEnd ){ break; } nH = nH * 33 + zIn[0] ; zIn++;` |
| 371366 |   34 | `		if( zIn >= zEnd ){ break; } nH = nH * 33 + zIn[0] ; zIn++;` |
| 296566 |   35 | `		if( zIn >= zEnd ){ break; } nH = nH * 33 + zIn[0] ; zIn++;` |
|      2 |   36 | `	}` |
| 191582 |   37 | `	return nH;` |
|      2 |   38 |  |
|      - |   39 | `/*` |
|      - |   40 | ` * Return the total number of entries in a given hashmap.` |
|      - |   41 | ` * If bRecurisve is set to TRUE then recurse on hashmap entries.` |
|      - |   42 | ` * If the nesting limit is reached,this function abort immediately.` |
|      - |   43 | ` */` |
|    680 |   44 | `static sxi64 HashmapCount(ph7_hashmap *pMap,int bRecursive,int iRecCount)` |
|      2 |   45 |  |
|    682 |   46 | `	sxi64 iCount = 0;` |
|    682 |   47 | `	if( !bRecursive ){` |
|    406 |   48 | `		iCount = pMap->nEntry;` |
|    204 |   49 | `	}else{` |
|      - |   50 | `		/* Recursive hashmap walk */` |
|    277 |   51 | `		ph7_hashmap_node *pEntry = pMap->pLast;` |
|      - |   52 | `		ph7_value *pElem;` |
|    277 |   53 | `		sxu32 n = 0;` |
|    331 |   54 | `		for(;;){` |
|    663 |   55 | `			if( n >= pMap->nEntry ){` |
|    273 |   56 | `				break;` |
|      - |   57 | `			}` |
|      - |   58 | `			/* Point to the element value */` |
|    391 |   59 | `			pElem = (ph7_value *)SySetAt(&pMap->pVm->aMemObj,pEntry->nValIdx);` |
|    391 |   60 | `			if( pElem ){` |
|    391 |   61 | `				if( pElem->iFlags & MEMOBJ_HASHMAP ){` |
|    251 |   62 | `					if( iRecCount > 31 ){` |
|      - |   63 | `						/* Nesting limit reached */` |
|      5 |   64 | `						return iCount;` |
|      - |   65 | `					}` |
|      - |   66 | `					/* Recurse */` |
|    247 |   67 | `					iRecCount++;` |
|    247 |   68 | `					iCount += HashmapCount((ph7_hashmap *)pElem->x.pOther,TRUE,iRecCount);` |
|    247 |   69 | `					iRecCount--;` |
|    123 |   70 | `				}` |
|    193 |   71 | `			}` |
|      - |   72 | `			/* Point to the next entry */` |
|    387 |   73 | `			pEntry = pEntry->pNext;` |
|    387 |   74 | `			++n;` |
|      1 |   75 | `		}` |
|      - |   76 | `		/* Update count */` |
|    273 |   77 | `		iCount += pMap->nEntry;` |
|      - |   78 | `	}` |
|    678 |   79 | `	return iCount;` |
|    342 |   80 |  |
|      - |   81 | `/*` |
|      - |   82 | ` * Allocate a new hashmap node with a 64-bit integer key.` |
|      - |   83 | ` * If something goes wrong [i.e: out of memory],this function return NULL.` |
|      - |   84 | ` * Otherwise a fresh [ph7_hashmap_node] instance is returned.` |
|      - |   85 | ` */` |
| 483310 |   86 | `static ph7_hashmap_node * HashmapNewIntNode(ph7_hashmap *pMap,sxi64 iKey,sxu32 nHash,sxu32 nValIdx)` |
|      2 |   87 |  |
|      - |   88 | `	ph7_hashmap_node *pNode;` |
|      - |   89 | `	/* Allocate a new node */` |
| 483312 |   90 | `	pNode = (ph7_hashmap_node *)SyMemBackendPoolAlloc(&pMap->pVm->sAllocator,sizeof(ph7_hashmap_node));` |
| 483312 |   91 | `	if( pNode == 0 ){` |
|    ! 0 |   92 | `		return 0;` |
|      - |   93 | `	}` |
|      - |   94 | `	/* Zero the stucture */` |
| 483312 |   95 | `	SyZero(pNode,sizeof(ph7_hashmap_node));` |
|      - |   96 | `	/* Fill in the structure */` |
| 483312 |   97 | `	pNode->pMap  = &(*pMap);` |
| 483312 |   98 | `	pNode->iType = HASHMAP_INT_NODE;` |
| 483312 |   99 | `	pNode->nHash = nHash;` |
| 483312 |  100 | `	pNode->xKey.iKey = iKey;` |
| 483312 |  101 | `	pNode->nValIdx  = nValIdx;` |
| 483312 |  102 | `	return pNode;` |
| 241657 |  103 |  |
|      - |  104 | `/*` |
|      - |  105 | ` * Allocate a new hashmap node with a BLOB key.` |
|      - |  106 | ` * If something goes wrong [i.e: out of memory],this function return NULL.` |
|      - |  107 | ` * Otherwise a fresh [ph7_hashmap_node] instance is returned.` |
|      - |  108 | ` */` |
|  66796 |  109 | `static ph7_hashmap_node * HashmapNewBlobNode(ph7_hashmap *pMap,const void *pKey,sxu32 nKeyLen,sxu32 nHash,sxu32 nValIdx)` |
|      2 |  110 |  |
|      - |  111 | `	ph7_hashmap_node *pNode;` |
|      - |  112 | `	/* Allocate a new node */` |
|  66798 |  113 | `	pNode = (ph7_hashmap_node *)SyMemBackendPoolAlloc(&pMap->pVm->sAllocator,sizeof(ph7_hashmap_node));` |
|  66798 |  114 | `	if( pNode == 0 ){` |
|    ! 0 |  115 | `		return 0;` |
|      - |  116 | `	}` |
|      - |  117 | `	/* Zero the stucture */` |
|  66798 |  118 | `	SyZero(pNode,sizeof(ph7_hashmap_node));` |
|      - |  119 | `	/* Fill in the structure */` |
|  66798 |  120 | `	pNode->pMap  = &(*pMap);` |
|  66798 |  121 | `	pNode->iType = HASHMAP_BLOB_NODE;` |
|  66798 |  122 | `	pNode->nHash = nHash;` |
|  66798 |  123 | `	SyBlobInit(&pNode->xKey.sKey,&pMap->pVm->sAllocator);` |
|  66798 |  124 | `	SyBlobAppend(&pNode->xKey.sKey,pKey,nKeyLen);` |
|  66798 |  125 | `	pNode->nValIdx = nValIdx;` |
|  66798 |  126 | `	return pNode;` |
|  33400 |  127 |  |
|      - |  128 | `/*` |
|      - |  129 | ` * link a hashmap node to the given bucket index (last argument to this function).` |
|      - |  130 | ` */` |
| 550106 |  131 | `static void HashmapNodeLink(ph7_hashmap *pMap,ph7_hashmap_node *pNode,sxu32 nBucketIdx)` |
|      2 |  132 |  |
|      - |  133 | `	/* Link */` |
| 550108 |  134 | `	if( pMap->apBucket[nBucketIdx] != 0 ){` |
| 399612 |  135 | `		pNode->pNextCollide = pMap->apBucket[nBucketIdx];` |
| 399612 |  136 | `		pMap->apBucket[nBucketIdx]->pPrevCollide = pNode;` |
| 199805 |  137 | `	}` |
| 550108 |  138 | `	pMap->apBucket[nBucketIdx] = pNode;` |
|      - |  139 | `	/* Link to the map list */` |
| 550108 |  140 | `	if( pMap->pFirst == 0 ){` |
|  28952 |  141 | `		pMap->pFirst = pMap->pLast = pNode;` |
|      - |  142 | `		/* Point to the first inserted node */` |
|  28952 |  143 | `		pMap->pCur = pNode;` |
|  14477 |  144 | `	}else{` |
| 521158 |  145 | `		MACRO_LD_PUSH(pMap->pLast,pNode);` |
|      - |  146 | `	}` |
| 550108 |  147 | `	++pMap->nEntry;` |
| 550108 |  148 |  |
|      - |  149 | `/*` |
|      - |  150 | ` * Unlink a node from the hashmap.` |
|      - |  151 | ` * If the node count reaches zero then release the whole hash-bucket.` |
|      - |  152 | ` */` |
|   4894 |  153 | `PH7_PRIVATE void PH7_HashmapUnlinkNode(ph7_hashmap_node *pNode,int bRestore)` |
|      2 |  154 |  |
|   4896 |  155 | `	ph7_hashmap *pMap = pNode->pMap;` |
|   4896 |  156 | `	ph7_vm *pVm = pMap->pVm;` |
|      - |  157 | `	/* Unlink from the corresponding bucket */` |
|   4896 |  158 | `	if( pNode->pPrevCollide == 0 ){` |
|   4464 |  159 | `		pMap->apBucket[pNode->nHash & (pMap->nSize - 1)] = pNode->pNextCollide;` |
|   2233 |  160 | `	}else{` |
|    433 |  161 | `		pNode->pPrevCollide->pNextCollide = pNode->pNextCollide;` |
|      - |  162 | `	}` |
|   4896 |  163 | `	if( pNode->pNextCollide ){` |
|   3703 |  164 | `		pNode->pNextCollide->pPrevCollide = pNode->pPrevCollide;` |
|   1851 |  165 | `	}` |
|   4896 |  166 | `	if( pMap->pFirst == pNode ){` |
|     48 |  167 | `		pMap->pFirst = pNode->pPrev;` |
|     23 |  168 | `	}` |
|   4896 |  169 | `	if( pMap->pCur == pNode ){` |
|      - |  170 | `		/* Advance the node cursor */` |
|     50 |  171 | `		pMap->pCur = pMap->pCur->pPrev; /* Reverse link */` |
|     24 |  172 | `	}` |
|      - |  173 | `	/* Unlink from the map list */` |
|   4896 |  174 | `	MACRO_LD_REMOVE(pMap->pLast,pNode);` |
|   4896 |  175 | `	if( bRestore ){` |
|      - |  176 | `		/* Remove the ph7_value associated with this node from the reference table */` |
|     18 |  177 | `		PH7_VmRefObjRemove(pVm,pNode->nValIdx,0,pNode);` |
|      - |  178 | `		/* Restore to the freelist */` |
|     18 |  179 | `		if( (pNode->iFlags & HASHMAP_NODE_FOREIGN_OBJ) == 0 ){` |
|     18 |  180 | `			PH7_VmUnsetMemObj(pVm,pNode->nValIdx,FALSE);` |
|      8 |  181 | `		}` |
|      8 |  182 | `	}` |
|   4896 |  183 | `	if( pNode->iType == HASHMAP_BLOB_NODE ){` |
|   4859 |  184 | `		SyBlobRelease(&pNode->xKey.sKey);` |
|   2429 |  185 | `	}` |
|   4896 |  186 | `	SyMemBackendPoolFree(&pVm->sAllocator,pNode);` |
|   4896 |  187 | `	pMap->nEntry--;` |
|   4896 |  188 | `	if( pMap->nEntry < 1 && pMap != pVm->pGlobal ){` |
|      - |  189 | `		/* Free the hash-bucket */` |
|     26 |  190 | `		SyMemBackendFree(&pVm->sAllocator,pMap->apBucket);` |
|     26 |  191 | `		pMap->apBucket = 0;` |
|     26 |  192 | `		pMap->nSize = 0;` |
|     26 |  193 | `		pMap->pFirst = pMap->pLast = pMap->pCur = 0;` |
|     12 |  194 | `	}` |
|   4896 |  195 |  |
|      - |  196 | `#define HASHMAP_FILL_FACTOR 3` |
|      - |  197 | `/*` |
|      - |  198 | ` * Grow the hash-table and rehash all entries.` |
|      - |  199 | ` */` |
| 550106 |  200 | `static sxi32 HashmapGrowBucket(ph7_hashmap *pMap)` |
|      2 |  201 |  |
| 550108 |  202 | `	if( pMap->nEntry >= pMap->nSize * HASHMAP_FILL_FACTOR ){` |
|  31826 |  203 | `		ph7_hashmap_node **apOld = pMap->apBucket;` |
|      - |  204 | `		ph7_hashmap_node *pEntry,**apNew;` |
|  31826 |  205 | `		sxu32 nNew = pMap->nSize << 1;` |
|      - |  206 | `		sxu32 nBucket;` |
|      - |  207 | `		sxu32 n;` |
|  31826 |  208 | `		if( nNew < 1 ){` |
|  28952 |  209 | `			nNew = 16;` |
|  14475 |  210 | `		}` |
|      - |  211 | `		/* Allocate a new bucket */` |
|  31826 |  212 | `		apNew = (ph7_hashmap_node **)SyMemBackendAlloc(&pMap->pVm->sAllocator,nNew * sizeof(ph7_hashmap_node *));` |
|  31826 |  213 | `		if( apNew == 0 ){` |
|    ! 0 |  214 | `			if( pMap->nSize < 1 ){` |
|    ! 0 |  215 | `				return SXERR_MEM; /* Fatal */` |
|      - |  216 | `			}` |
|      - |  217 | `			/* Not so fatal here,simply a performance hit */` |
|    ! 0 |  218 | `			return SXRET_OK;` |
|      - |  219 | `		}` |
|      - |  220 | `		/* Zero the table */` |
|  31826 |  221 | `		SyZero((void *)apNew,nNew * sizeof(ph7_hashmap_node *));` |
|      - |  222 | `		/* Reflect the change */` |
|  31826 |  223 | `		pMap->apBucket = apNew;` |
|  31826 |  224 | `		pMap->nSize = nNew;` |
|  31826 |  225 | `		if( apOld == 0 ){` |
|      - |  226 | `			/* First allocated table [i.e: no entry],return immediately */` |
|  28952 |  227 | `			return SXRET_OK;` |
|      - |  228 | `		}` |
|      - |  229 | `		/* Rehash old entries */` |
|   2876 |  230 | `		pEntry = pMap->pFirst;` |
|   2876 |  231 | `		n = 0;` |
| 242301 |  232 | `		for( ;; ){` |
| 484604 |  233 | `			if( n >= pMap->nEntry ){` |
|   2876 |  234 | `				break;` |
|      - |  235 | `			}` |
|      - |  236 | `			/* Clear the old collision link */` |
| 481730 |  237 | `			pEntry->pNextCollide = pEntry->pPrevCollide = 0;` |
|      - |  238 | `			/* Link to the new bucket */` |
| 481730 |  239 | `			nBucket = pEntry->nHash & (nNew - 1);` |
| 481730 |  240 | `			if( pMap->apBucket[nBucket] != 0 ){` |
| 214244 |  241 | `				pEntry->pNextCollide = pMap->apBucket[nBucket];` |
| 214244 |  242 | `				pMap->apBucket[nBucket]->pPrevCollide = pEntry;` |
| 107121 |  243 | `			}` |
| 481730 |  244 | `			pMap->apBucket[nBucket] = pEntry;` |
|      - |  245 | `			/* Point to the next entry */` |
| 481730 |  246 | `			pEntry = pEntry->pPrev; /* Reverse link */` |
| 481730 |  247 | `			n++;` |
|      2 |  248 | `		}` |
|      - |  249 | `		/* Free the old table */` |
|   2876 |  250 | `		SyMemBackendFree(&pMap->pVm->sAllocator,(void *)apOld);` |
|   1437 |  251 | `	}` |
| 521158 |  252 | `	return SXRET_OK;` |
| 275055 |  253 |  |
|      - |  254 | `/*` |
|      - |  255 | ` * Insert a 64-bit integer key and it's associated value (if any) in the given` |
|      - |  256 | ` * hashmap.` |
|      - |  257 | ` */` |
| 483310 |  258 | `static sxi32 HashmapInsertIntKey(ph7_hashmap *pMap,sxi64 iKey,ph7_value *pValue,sxu32 nRefIdx,int isForeign)` |
|      2 |  259 |  |
|      - |  260 | `	ph7_hashmap_node *pNode;` |
|      - |  261 | `	sxu32 nIdx;` |
|      - |  262 | `	sxu32 nHash;` |
|      - |  263 | `	sxi32 rc;` |
| 483312 |  264 | `	if( !isForeign ){` |
|      - |  265 | `		ph7_value *pObj;` |
|      - |  266 | `		/* Reserve a ph7_value for the value */` |
| 483288 |  267 | `		pObj = PH7_ReserveMemObj(pMap->pVm);` |
| 483288 |  268 | `		if( pObj == 0 ){` |
|    ! 0 |  269 | `			return SXERR_MEM;` |
|      - |  270 | `		}` |
| 483288 |  271 | `		if( pValue ){` |
|      - |  272 | `			/* Duplicate the value */` |
| 483288 |  273 | `			PH7_MemObjStore(pValue,pObj);` |
| 241643 |  274 | `		}` |
| 483288 |  275 | `		nIdx = pObj->nIdx;` |
| 241645 |  276 | `	}else{` |
|     25 |  277 | `		nIdx = nRefIdx;` |
|      - |  278 | `	}` |
|      - |  279 | `	/* Hash the key */` |
| 483312 |  280 | `	nHash = pMap->xIntHash(iKey);` |
|      - |  281 | `	/* Allocate a new int node */` |
| 483312 |  282 | `	pNode = HashmapNewIntNode(&(*pMap),iKey,nHash,nIdx);` |
| 483312 |  283 | `	if( pNode == 0 ){` |
|    ! 0 |  284 | `		return SXERR_MEM;` |
|      - |  285 | `	}` |
| 483312 |  286 | `	if( isForeign ){` |
|      - |  287 | `		/* Mark as a foregin entry */` |
|     25 |  288 | `		pNode->iFlags \|= HASHMAP_NODE_FOREIGN_OBJ;` |
|     12 |  289 | `	}` |
|      - |  290 | `	/* Make sure the bucket is big enough to hold the new entry */` |
| 483312 |  291 | `	rc = HashmapGrowBucket(&(*pMap));` |
| 483312 |  292 | `	if( rc != SXRET_OK ){` |
|    ! 0 |  293 | `		SyMemBackendPoolFree(&pMap->pVm->sAllocator,pNode);` |
|    ! 0 |  294 | `		return rc;` |
|      - |  295 | `	}` |
|      - |  296 | `	/* Perform the insertion */` |
| 483312 |  297 | `	HashmapNodeLink(&(*pMap),pNode,nHash & (pMap->nSize - 1));` |
|      - |  298 | `	/* Install in the reference table */` |
| 483312 |  299 | `	PH7_VmRefObjInstall(pMap->pVm,nIdx,0,pNode,0);` |
|      - |  300 | `	/* All done */` |
| 483312 |  301 | `	return SXRET_OK;` |
| 241657 |  302 |  |
|      - |  303 | `/*` |
|      - |  304 | ` * Insert a BLOB key and it's associated value (if any) in the given` |
|      - |  305 | ` * hashmap.` |
|      - |  306 | ` */` |
|  66796 |  307 | `static sxi32 HashmapInsertBlobKey(ph7_hashmap *pMap,const void *pKey,sxu32 nKeyLen,ph7_value *pValue,sxu32 nRefIdx,int isForeign)` |
|      2 |  308 |  |
|      - |  309 | `	ph7_hashmap_node *pNode;` |
|      - |  310 | `	sxu32 nHash;` |
|      - |  311 | `	sxu32 nIdx;` |
|      - |  312 | `	sxi32 rc;` |
|  66798 |  313 | `	if( !isForeign ){` |
|      - |  314 | `		ph7_value *pObj;` |
|      - |  315 | `		/* Reserve a ph7_value for the value */` |
|  52848 |  316 | `		pObj = PH7_ReserveMemObj(pMap->pVm);` |
|  52848 |  317 | `		if( pObj == 0 ){` |
|    ! 0 |  318 | `			return SXERR_MEM;` |
|      - |  319 | `		}` |
|  52848 |  320 | `		if( pValue ){` |
|      - |  321 | `			/* Duplicate the value */` |
|  52848 |  322 | `			PH7_MemObjStore(pValue,pObj);` |
|  26423 |  323 | `		}` |
|  52848 |  324 | `		nIdx = pObj->nIdx;` |
|  26425 |  325 | `	}else{` |
|  13952 |  326 | `		nIdx = nRefIdx;` |
|      - |  327 | `	}` |
|      - |  328 | `	/* Hash the key */` |
|  66798 |  329 | `	nHash = pMap->xBlobHash(pKey,nKeyLen);` |
|      - |  330 | `	/* Allocate a new blob node */` |
|  66798 |  331 | `	pNode = HashmapNewBlobNode(&(*pMap),pKey,nKeyLen,nHash,nIdx);` |
|  66798 |  332 | `	if( pNode == 0 ){` |
|    ! 0 |  333 | `		return SXERR_MEM;` |
|      - |  334 | `	}` |
|  66798 |  335 | `	if( isForeign ){` |
|      - |  336 | `		/* Mark as a foregin entry */` |
|  13952 |  337 | `		pNode->iFlags \|= HASHMAP_NODE_FOREIGN_OBJ;` |
|   6975 |  338 | `	}` |
|      - |  339 | `	/* Make sure the bucket is big enough to hold the new entry */` |
|  66798 |  340 | `	rc = HashmapGrowBucket(&(*pMap));` |
|  66798 |  341 | `	if( rc != SXRET_OK ){` |
|    ! 0 |  342 | `		SyMemBackendPoolFree(&pMap->pVm->sAllocator,pNode);` |
|    ! 0 |  343 | `		return rc;` |
|      - |  344 | `	}` |
|      - |  345 | `	/* Perform the insertion */` |
|  66798 |  346 | `	HashmapNodeLink(&(*pMap),pNode,nHash & (pMap->nSize - 1));` |
|      - |  347 | `	/* Install in the reference table */` |
|  66798 |  348 | `	PH7_VmRefObjInstall(pMap->pVm,nIdx,0,pNode,0);` |
|      - |  349 | `	/* All done */` |
|  66798 |  350 | `	return SXRET_OK;` |
|  33400 |  351 |  |
|      - |  352 | `/*` |
|      - |  353 | ` * Check if a given 64-bit integer key exists in the given hashmap.` |
|      - |  354 | ` * Write a pointer to the target node on success. Otherwise` |
|      - |  355 | ` * SXERR_NOTFOUND is returned on failure.` |
|      - |  356 | ` */` |
|  46214 |  357 | `static sxi32 HashmapLookupIntKey(` |
|      - |  358 | `	ph7_hashmap *pMap,         /* Target hashmap */` |
|      - |  359 | `	sxi64 iKey,                /* lookup key */` |
|      - |  360 | `	ph7_hashmap_node **ppNode  /* OUT: target node on success */` |
|      - |  361 | `	)` |
|      2 |  362 |  |
|      - |  363 | `	ph7_hashmap_node *pNode;` |
|      - |  364 | `	sxu32 nHash;` |
|  46216 |  365 | `	if( pMap->nEntry < 1 ){` |
|      - |  366 | `		/* Don't bother hashing,there is no entry anyway */` |
|    299 |  367 | `		return SXERR_NOTFOUND;` |
|      - |  368 | `	}` |
|      - |  369 | `	/* Hash the key first */` |
|  45918 |  370 | `	nHash = pMap->xIntHash(iKey);` |
|      - |  371 | `	/* Point to the appropriate bucket */` |
|  45918 |  372 | `	pNode = pMap->apBucket[nHash & (pMap->nSize - 1)];` |
|      - |  373 | `	/* Perform the lookup */` |
| 411315 |  374 | `	for(;;){` |
| 822632 |  375 | `		if( pNode == 0 ){` |
|  45539 |  376 | `			break;` |
|      - |  377 | `		}` |
| 777281 |  378 | `		if( pNode->iType == HASHMAP_INT_NODE` |
| 774074 |  379 | `			&& pNode->nHash == nHash` |
| 385719 |  380 | `			&& pNode->xKey.iKey == iKey ){` |
|      - |  381 | `				/* Node found */` |
|    380 |  382 | `				if( ppNode ){` |
|    374 |  383 | `					*ppNode = pNode;` |
|    186 |  384 | `				}` |
|    380 |  385 | `				return SXRET_OK;` |
|      - |  386 | `		}` |
|      - |  387 | `		/* Follow the collision link */` |
| 776715 |  388 | `		pNode = pNode->pNextCollide;` |
|      1 |  389 | `	}` |
|      - |  390 | `	/* No such entry */` |
|  45539 |  391 | `	return SXERR_NOTFOUND;` |
|  23109 |  392 |  |
|      - |  393 | `/*` |
|      - |  394 | ` * Check if a given BLOB key exists in the given hashmap.` |
|      - |  395 | ` * Write a pointer to the target node on success. Otherwise` |
|      - |  396 | ` * SXERR_NOTFOUND is returned on failure.` |
|      - |  397 | ` */` |
| 130708 |  398 | `static sxi32 HashmapLookupBlobKey(` |
|      - |  399 | `	ph7_hashmap *pMap,          /* Target hashmap */` |
|      - |  400 | `	const void *pKey,           /* Lookup key */` |
|      - |  401 | `	sxu32 nKeyLen,              /* Key length in bytes */` |
|      - |  402 | `	ph7_hashmap_node **ppNode   /* OUT: target node on success */` |
|      - |  403 | `	)` |
|      2 |  404 |  |
|      - |  405 | `	ph7_hashmap_node *pNode;` |
|      - |  406 | `	sxu32 nHash;` |
| 130710 |  407 | `	if( pMap->nEntry < 1 ){` |
|      - |  408 | `		/* Don't bother hashing,there is no entry anyway */` |
|   5926 |  409 | `		return SXERR_NOTFOUND;` |
|      - |  410 | `	}` |
|      - |  411 | `	/* Hash the key first */` |
| 124786 |  412 | `	nHash = pMap->xBlobHash(pKey,nKeyLen);` |
|      - |  413 | `	/* Point to the appropriate bucket */` |
| 124786 |  414 | `	pNode = pMap->apBucket[nHash & (pMap->nSize - 1)];` |
|      - |  415 | `	/* Perform the lookup */` |
| 132111 |  416 | `	for(;;){` |
| 264224 |  417 | `		if( pNode == 0 ){` |
|  93756 |  418 | `			break;` |
|      - |  419 | `		}` |
| 185983 |  420 | `		if( pNode->iType == HASHMAP_BLOB_NODE` |
| 168968 |  421 | `			&& pNode->nHash == nHash` |
|  99249 |  422 | `			&& SyBlobLength(&pNode->xKey.sKey) == nKeyLen` |
|  31032 |  423 | `			&& SyMemcmp(SyBlobData(&pNode->xKey.sKey),pKey,nKeyLen) == 0 ){` |
|      - |  424 | `				/* Node found */` |
|  31032 |  425 | `				if( ppNode ){` |
|  31020 |  426 | `					*ppNode = pNode;` |
|  15509 |  427 | `				}` |
|  31032 |  428 | `				return SXRET_OK;` |
|      - |  429 | `		}` |
|      - |  430 | `		/* Follow the collision link */` |
| 139440 |  431 | `		pNode = pNode->pNextCollide;` |
|      2 |  432 | `	}` |
|      - |  433 | `	/* No such entry */` |
|  93756 |  434 | `	return SXERR_NOTFOUND;` |
|  65356 |  435 |  |
|      - |  436 | `/*` |
|      - |  437 | ` * Check if the given BLOB key looks like a decimal number.` |
|      - |  438 | ` * Retrurn TRUE on success.FALSE otherwise.` |
|      - |  439 | ` */` |
| 130900 |  440 | `static int HashmapIsIntKey(SyBlob *pKey)` |
|      2 |  441 |  |
| 130902 |  442 | `	const char *zIn  = (const char *)SyBlobData(pKey);` |
| 130902 |  443 | `	const char *zEnd = &zIn[SyBlobLength(pKey)];` |
| 130902 |  444 | `	if( (int)(zEnd-zIn) > 1 && zIn[0] == '0' ){` |
|      - |  445 | `		/* Octal not decimal number */` |
|      5 |  446 | `		return FALSE;` |
|      - |  447 | `	}` |
| 130898 |  448 | `	if( (zIn[0] == '-' \|\| zIn[0] == '+') && &zIn[1] < zEnd ){` |
|    ! 0 |  449 | `		zIn++;` |
|    ! 0 |  450 | `	}` |
|  65779 |  451 | `	for(;;){` |
| 131560 |  452 | `		if( zIn >= zEnd ){` |
|    231 |  453 | `			return TRUE;` |
|      - |  454 | `		}` |
| 131330 |  455 | `		if( (unsigned char)zIn[0] >= 0xc0 /* UTF-8 stream */  \|\| !SyisDigit(zIn[0]) ){` |
|  65335 |  456 | `			break;` |
|      - |  457 | `		}` |
|    663 |  458 | `		zIn++;` |
|      1 |  459 | `	}` |
|      - |  460 | `	/* Key does not look like a decimal number */` |
| 130668 |  461 | `	return FALSE;` |
|  65452 |  462 |  |
|      - |  463 | `/*` |
|      - |  464 | ` * Check if a given key exists in the given hashmap.` |
|      - |  465 | ` * Write a pointer to the target node on success.` |
|      - |  466 | ` * Otherwise SXERR_NOTFOUND is returned on failure.` |
|      - |  467 | ` */` |
|  64210 |  468 | `static sxi32 HashmapLookup(` |
|      - |  469 | `	ph7_hashmap *pMap,          /* Target hashmap */` |
|      - |  470 | `	ph7_value *pKey,            /* Lookup key */` |
|      - |  471 | `	ph7_hashmap_node **ppNode   /* OUT: target node on success */` |
|      - |  472 | `	)` |
|      2 |  473 |  |
|  64212 |  474 | `	ph7_hashmap_node *pNode = 0; /* cc -O6 warning */` |
|      - |  475 | `	sxi32 rc;` |
|  64212 |  476 | `	if( pKey->iFlags & (MEMOBJ_STRING\|MEMOBJ_HASHMAP\|MEMOBJ_OBJ\|MEMOBJ_RES) ){` |
|  63888 |  477 | `		if( (pKey->iFlags & MEMOBJ_STRING) == 0 ){` |
|      - |  478 | `			/* Force a string cast */` |
|    ! 0 |  479 | `			PH7_MemObjToString(&(*pKey));` |
|    ! 0 |  480 | `		}` |
|  63888 |  481 | `		if( SyBlobLength(&pKey->sBlob) > 0 && !HashmapIsIntKey(&pKey->sBlob) ){` |
|      - |  482 | `			/* Perform a blob lookup */` |
|  63878 |  483 | `			rc = HashmapLookupBlobKey(&(*pMap),SyBlobData(&pKey->sBlob),SyBlobLength(&pKey->sBlob),&pNode);` |
|  63878 |  484 | `			goto result;` |
|      - |  485 | `		}` |
|      5 |  486 | `	}` |
|      - |  487 | `	/* Perform an int lookup */` |
|    336 |  488 | `	if((pKey->iFlags & MEMOBJ_INT) == 0 ){` |
|      - |  489 | `		/* Force an integer cast */` |
|     19 |  490 | `		PH7_MemObjToInteger(pKey);` |
|      9 |  491 | `	}` |
|      - |  492 | `	/* Perform an int lookup */` |
|    336 |  493 | `	rc = HashmapLookupIntKey(&(*pMap),pKey->x.iVal,&pNode);` |
|  32105 |  494 | `result:` |
|  64212 |  495 | `	if( rc == SXRET_OK ){` |
|      - |  496 | `		/* Node found */` |
|  31308 |  497 | `		if( ppNode ){` |
|  31292 |  498 | `			*ppNode = pNode;` |
|  15645 |  499 | `		}` |
|  31308 |  500 | `		return SXRET_OK;` |
|      - |  501 | `	}` |
|      - |  502 | `	/* No such entry */` |
|  32906 |  503 | `	return SXERR_NOTFOUND;` |
|  32107 |  504 |  |
|      - |  505 | `/*` |
|      - |  506 | ` * Insert a given key and it's associated value (if any) in the given` |
|      - |  507 | ` * hashmap.` |
|      - |  508 | ` * If a node with the given key already exists in the database` |
|      - |  509 | ` * then this function overwrite the old value.` |
|      - |  510 | ` */` |
| 536122 |  511 | `static sxi32 HashmapInsert(` |
|      - |  512 | `	ph7_hashmap *pMap, /* Target hashmap */` |
|      - |  513 | `	ph7_value *pKey,   /* Lookup key  */` |
|      - |  514 | `	ph7_value *pVal    /* Node value */` |
|      - |  515 | `	)` |
|      2 |  516 |  |
| 536124 |  517 | `	ph7_hashmap_node *pNode = 0;` |
| 536124 |  518 | `	sxi32 rc = SXRET_OK;` |
| 536124 |  519 | `	if( pKey && pKey->iFlags & (MEMOBJ_STRING\|MEMOBJ_HASHMAP\|MEMOBJ_OBJ\|MEMOBJ_RES) ){` |
|  53074 |  520 | `		if( (pKey->iFlags & MEMOBJ_STRING) == 0 ){` |
|      - |  521 | `			/* Force a string cast */` |
|      8 |  522 | `			PH7_MemObjToString(&(*pKey));` |
|      3 |  523 | `		}` |
|  53074 |  524 | `		if( SyBlobLength(&pKey->sBlob) < 1 \|\| HashmapIsIntKey(&pKey->sBlob) ){` |
|    236 |  525 | `			if(SyBlobLength(&pKey->sBlob) < 1){` |
|      - |  526 | `				/* Automatic index assign */` |
|     16 |  527 | `				pKey = 0;` |
|      7 |  528 | `			}` |
|    236 |  529 | `			goto IntKey;` |
|      - |  530 | `		}` |
|  79259 |  531 | `		if( SXRET_OK == HashmapLookupBlobKey(&(*pMap),SyBlobData(&pKey->sBlob),` |
|  26419 |  532 | `			SyBlobLength(&pKey->sBlob),&pNode) ){` |
|      - |  533 | `				/* Overwrite the old value */` |
|      - |  534 | `				ph7_value *pElem;` |
|     21 |  535 | `				pElem = (ph7_value *)SySetAt(&pMap->pVm->aMemObj,pNode->nValIdx);` |
|     21 |  536 | `				if( pElem ){` |
|     21 |  537 | `					if( pVal ){` |
|     21 |  538 | `						PH7_MemObjStore(pVal,pElem);` |
|     11 |  539 | `					}else{` |
|      - |  540 | `						/* Nullify the entry */` |
|    ! 0 |  541 | `						PH7_MemObjToNull(pElem);` |
|      - |  542 | `					}` |
|     10 |  543 | `				}` |
|     21 |  544 | `				return SXRET_OK;` |
|      - |  545 | `		}` |
|  52820 |  546 | `		if( pMap == pMap->pVm->pGlobal ){` |
|      - |  547 | `			/* Forbidden */` |
|      3 |  548 | `			PH7_VmThrowError(pMap->pVm,0,PH7_CTX_NOTICE,"$GLOBALS is a read-only array,insertion is forbidden");` |
|      3 |  549 | `			return SXRET_OK;` |
|      - |  550 | `		}` |
|      - |  551 | `		/* Perform a blob-key insertion */` |
|  52818 |  552 | `		rc = HashmapInsertBlobKey(&(*pMap),SyBlobData(&pKey->sBlob),SyBlobLength(&pKey->sBlob),&(*pVal),0,FALSE);` |
|  52818 |  553 | `		return rc;` |
|      - |  554 | `	}` |
| 241525 |  555 | `IntKey:` |
| 483286 |  556 | `	if( pKey ){` |
|  23063 |  557 | `		if((pKey->iFlags & MEMOBJ_INT) == 0 ){` |
|      - |  558 | `			/* Force an integer cast */` |
|    247 |  559 | `			PH7_MemObjToInteger(pKey);` |
|    123 |  560 | `		}` |
|  23063 |  561 | `		if( SXRET_OK == HashmapLookupIntKey(&(*pMap),pKey->x.iVal,&pNode) ){` |
|      - |  562 | `			/* Overwrite the old value */` |
|      - |  563 | `			ph7_value *pElem;` |
|     39 |  564 | `			pElem = (ph7_value *)SySetAt(&pMap->pVm->aMemObj,pNode->nValIdx);` |
|     39 |  565 | `			if( pElem ){` |
|     39 |  566 | `				if( pVal ){` |
|     39 |  567 | `					PH7_MemObjStore(pVal,pElem);` |
|     20 |  568 | `				}else{` |
|      - |  569 | `					/* Nullify the entry */` |
|    ! 0 |  570 | `					PH7_MemObjToNull(pElem);` |
|      - |  571 | `				}` |
|     19 |  572 | `			}` |
|     39 |  573 | `			return SXRET_OK;` |
|      - |  574 | `		}` |
|  23025 |  575 | `		if( pMap == pMap->pVm->pGlobal ){` |
|      - |  576 | `			/* Forbidden */` |
|      3 |  577 | `			PH7_VmThrowError(pMap->pVm,0,PH7_CTX_NOTICE,"$GLOBALS is a read-only array,insertion is forbidden");` |
|      3 |  578 | `			return SXRET_OK;` |
|      - |  579 | `		}` |
|      - |  580 | `		/* Perform a 64-bit-int-key insertion */` |
|  23023 |  581 | `		rc = HashmapInsertIntKey(&(*pMap),pKey->x.iVal,&(*pVal),0,FALSE);` |
|  23023 |  582 | `		if( rc == SXRET_OK ){` |
|  23023 |  583 | `			if( pKey->x.iVal >= pMap->iNextIdx ){` |
|      - |  584 | `				/* Increment the automatic index */` |
|  22799 |  585 | `				pMap->iNextIdx = pKey->x.iVal + 1;` |
|      - |  586 | `				/* Make sure the automatic index is not reserved */` |
|  22799 |  587 | `				while( SXRET_OK == HashmapLookupIntKey(&(*pMap),pMap->iNextIdx,0) ){` |
|    ! 0 |  588 | `					pMap->iNextIdx++;` |
|    ! 0 |  589 | `				}` |
|  11399 |  590 | `			}` |
|  11511 |  591 | `		}` |
|  11512 |  592 | `	}else{` |
| 460224 |  593 | `		if( pMap == pMap->pVm->pGlobal ){` |
|      - |  594 | `			/* Forbidden */` |
|      3 |  595 | `			PH7_VmThrowError(pMap->pVm,0,PH7_CTX_NOTICE,"$GLOBALS is a read-only array,insertion is forbidden");` |
|      3 |  596 | `			return SXRET_OK;` |
|      - |  597 | `		}` |
|      - |  598 | `		/* Assign an automatic index */` |
| 460222 |  599 | `		rc = HashmapInsertIntKey(&(*pMap),pMap->iNextIdx,&(*pVal),0,FALSE);` |
| 460222 |  600 | `		if( rc == SXRET_OK ){` |
| 460222 |  601 | `			++pMap->iNextIdx;` |
| 230110 |  602 | `		}` |
|      - |  603 | `	}` |
|      - |  604 | `	/* Insertion result */` |
| 483244 |  605 | `	return rc;` |
| 268063 |  606 |  |
|      - |  607 | `/*` |
|      - |  608 | ` * Insert a given key and it's associated value (foreign index) in the given` |
|      - |  609 | ` * hashmap.` |
|      - |  610 | ` * This is insertion by reference so be careful to mark the node` |
|      - |  611 | ` * with the HASHMAP_NODE_FOREIGN_OBJ flag being set.` |
|      - |  612 | ` * The insertion by reference is triggered when the following` |
|      - |  613 | ` * expression is encountered.` |
|      - |  614 | ` * $var = 10;` |
|      - |  615 | ` *  $a = array(&var);` |
|      - |  616 | ` * OR` |
|      - |  617 | ` *  $a[] =& $var;` |
|      - |  618 | ` * That is,$var is a foreign ph7_value and the $a array have no control` |
|      - |  619 | ` * over it's contents.` |
|      - |  620 | ` * Note that the node that hold the foreign ph7_value is automatically` |
|      - |  621 | ` * removed when the foreign ph7_value is unset.` |
|      - |  622 | ` * Example:` |
|      - |  623 | ` *  $var = 10;` |
|      - |  624 | ` *  $a[] =& $var;` |
|      - |  625 | ` *  echo count($a).PHP_EOL; //1` |
|      - |  626 | ` *  //Unset the foreign ph7_value now` |
|      - |  627 | ` *  unset($var);` |
|      - |  628 | ` *  echo count($a); //0` |
|      - |  629 | ` * Note that this is a PH7 eXtension.` |
|      - |  630 | ` * Refer to the official documentation for more information.` |
|      - |  631 | ` * If a node with the given key already exists in the database` |
|      - |  632 | ` * then this function overwrite the old value.` |
|      - |  633 | ` */` |
|  13980 |  634 | `static sxi32 HashmapInsertByRef(` |
|      - |  635 | `	ph7_hashmap *pMap,   /* Target hashmap */` |
|      - |  636 | `	ph7_value *pKey,     /* Lookup key */` |
|      - |  637 | `	sxu32 nRefIdx        /* Foreign ph7_value index */` |
|      - |  638 | `	)` |
|      2 |  639 |  |
|  13982 |  640 | `	ph7_hashmap_node *pNode = 0;` |
|  13982 |  641 | `	sxi32 rc = SXRET_OK;` |
|  13982 |  642 | `	if( pKey && pKey->iFlags & (MEMOBJ_STRING\|MEMOBJ_HASHMAP\|MEMOBJ_OBJ\|MEMOBJ_RES) ){` |
|  13958 |  643 | `		if( (pKey->iFlags & MEMOBJ_STRING) == 0 ){` |
|      - |  644 | `			/* Force a string cast */` |
|    ! 0 |  645 | `			PH7_MemObjToString(&(*pKey));` |
|    ! 0 |  646 | `		}` |
|  13958 |  647 | `		if( SyBlobLength(&pKey->sBlob) < 1 \|\| HashmapIsIntKey(&pKey->sBlob) ){` |
|    ! 0 |  648 | `			if(SyBlobLength(&pKey->sBlob) < 1){` |
|      - |  649 | `				/* Automatic index assign */` |
|    ! 0 |  650 | `				pKey = 0;` |
|    ! 0 |  651 | `			}` |
|    ! 0 |  652 | `			goto IntKey;` |
|      - |  653 | `		}` |
|  20936 |  654 | `		if( SXRET_OK == HashmapLookupBlobKey(&(*pMap),SyBlobData(&pKey->sBlob),` |
|   6978 |  655 | `			SyBlobLength(&pKey->sBlob),&pNode) ){` |
|      - |  656 | `				/* Overwrite */` |
|      7 |  657 | `				PH7_VmRefObjRemove(pMap->pVm,pNode->nValIdx,0,pNode);` |
|      7 |  658 | `				pNode->nValIdx = nRefIdx;` |
|      - |  659 | `				/* Install in the reference table */` |
|      7 |  660 | `				PH7_VmRefObjInstall(pMap->pVm,nRefIdx,0,pNode,0);` |
|      7 |  661 | `				return SXRET_OK;` |
|      - |  662 | `		}` |
|      - |  663 | `		/* Perform a blob-key insertion */` |
|  13952 |  664 | `		rc = HashmapInsertBlobKey(&(*pMap),SyBlobData(&pKey->sBlob),SyBlobLength(&pKey->sBlob),0,nRefIdx,TRUE);` |
|  13952 |  665 | `		return rc;` |
|      - |  666 | `	}` |
|     12 |  667 | `IntKey:` |
|     25 |  668 | `	if( pKey ){` |
|      3 |  669 | `		if((pKey->iFlags & MEMOBJ_INT) == 0 ){` |
|      - |  670 | `			/* Force an integer cast */` |
|    ! 0 |  671 | `			PH7_MemObjToInteger(pKey);` |
|    ! 0 |  672 | `		}` |
|      3 |  673 | `		if( SXRET_OK == HashmapLookupIntKey(&(*pMap),pKey->x.iVal,&pNode) ){` |
|      - |  674 | `			/* Overwrite */` |
|    ! 0 |  675 | `			PH7_VmRefObjRemove(pMap->pVm,pNode->nValIdx,0,pNode);` |
|    ! 0 |  676 | `			pNode->nValIdx = nRefIdx;` |
|      - |  677 | `			/* Install in the reference table */` |
|    ! 0 |  678 | `			PH7_VmRefObjInstall(pMap->pVm,nRefIdx,0,pNode,0);` |
|    ! 0 |  679 | `			return SXRET_OK;` |
|      - |  680 | `		}` |
|      - |  681 | `		/* Perform a 64-bit-int-key insertion */` |
|      3 |  682 | `		rc = HashmapInsertIntKey(&(*pMap),pKey->x.iVal,0,nRefIdx,TRUE);` |
|      3 |  683 | `		if( rc == SXRET_OK ){` |
|      3 |  684 | `			if( pKey->x.iVal >= pMap->iNextIdx ){` |
|      - |  685 | `				/* Increment the automatic index */` |
|      3 |  686 | `				pMap->iNextIdx = pKey->x.iVal + 1;` |
|      - |  687 | `				/* Make sure the automatic index is not reserved */` |
|      3 |  688 | `				while( SXRET_OK == HashmapLookupIntKey(&(*pMap),pMap->iNextIdx,0) ){` |
|    ! 0 |  689 | `					pMap->iNextIdx++;` |
|    ! 0 |  690 | `				}` |
|      1 |  691 | `			}` |
|      1 |  692 | `		}` |
|      2 |  693 | `	}else{` |
|      - |  694 | `		/* Assign an automatic index */` |
|     23 |  695 | `		rc = HashmapInsertIntKey(&(*pMap),pMap->iNextIdx,0,nRefIdx,TRUE);` |
|     23 |  696 | `		if( rc == SXRET_OK ){` |
|     23 |  697 | `			++pMap->iNextIdx;` |
|     11 |  698 | `		}` |
|      - |  699 | `	}` |
|      - |  700 | `	/* Insertion result */` |
|     25 |  701 | `	return rc;` |
|   6992 |  702 |  |
|      - |  703 | `/*` |
|      - |  704 | ` * Extract node value.` |
|      - |  705 | ` */` |
| 657282 |  706 | `static ph7_value * HashmapExtractNodeValue(ph7_hashmap_node *pNode)` |
|      2 |  707 |  |
|      - |  708 | `	/* Point to the desired object */` |
|      - |  709 | `	ph7_value *pObj;` |
| 657284 |  710 | `	pObj = (ph7_value *)SySetAt(&pNode->pMap->pVm->aMemObj,pNode->nValIdx);` |
| 657284 |  711 | `	return pObj;` |
|      2 |  712 |  |
|      - |  713 | `/*` |
|      - |  714 | ` * Insert a node in the given hashmap.` |
|      - |  715 | ` * If a node with the given key already exists in the database` |
|      - |  716 | ` * then this function overwrite the old value.` |
|      - |  717 | ` */` |
|     98 |  718 | `static sxi32 HashmapInsertNode(ph7_hashmap *pMap,ph7_hashmap_node *pNode,int bPreserve)` |
|      1 |  719 |  |
|      - |  720 | `	ph7_value *pObj;` |
|      - |  721 | `	sxi32 rc;` |
|      - |  722 | `	/* Extract the node value */` |
|     99 |  723 | `	pObj = HashmapExtractNodeValue(&(*pNode));` |
|     99 |  724 | `	if( pObj == 0 ){` |
|    ! 0 |  725 | `		return SXERR_EMPTY;` |
|      - |  726 | `	}` |
|      - |  727 | `	/* Preserve key */` |
|     99 |  728 | `	if( pNode->iType == HASHMAP_INT_NODE){` |
|      - |  729 | `		/* Int64 key */` |
|     71 |  730 | `		if( !bPreserve ){` |
|      - |  731 | `			/* Assign an automatic index */` |
|     39 |  732 | `			rc = HashmapInsert(&(*pMap),0,pObj);` |
|     20 |  733 | `		}else{` |
|     33 |  734 | `			rc = HashmapInsertIntKey(&(*pMap),pNode->xKey.iKey,pObj,0,FALSE);` |
|      - |  735 | `		}` |
|     36 |  736 | `	}else{` |
|      - |  737 | `		/* Blob key */` |
|     43 |  738 | `		rc = HashmapInsertBlobKey(&(*pMap),SyBlobData(&pNode->xKey.sKey),` |
|     14 |  739 | `			SyBlobLength(&pNode->xKey.sKey),pObj,0,FALSE);` |
|      - |  740 | `	}` |
|     99 |  741 | `	return rc;` |
|     50 |  742 |  |
|      - |  743 | `/*` |
|      - |  744 | ` * Compare two node values.` |
|      - |  745 | ` * Return 0 if the node values are equals, > 0 if pLeft is greater than pRight` |
|      - |  746 | ` * or < 0 if pRight is greater than pLeft.` |
|      - |  747 | ` * For a full description on ph7_values comparison,refer to the implementation` |
|      - |  748 | ` * of the [PH7_MemObjCmp()] function defined in memobj.c or the official` |
|      - |  749 | ` * documenation.` |
|      - |  750 | ` */` |
|  30785 |  751 | `static sxi32 HashmapNodeCmp(ph7_hashmap_node *pLeft,ph7_hashmap_node *pRight,int bStrict)` |
|      2 |  752 |  |
|      - |  753 | `	ph7_value sObj1,sObj2;` |
|      - |  754 | `	sxi32 rc;` |
|  30787 |  755 | `	if( pLeft == pRight ){` |
|      - |  756 | `		/*` |
|      - |  757 | `		 * Same node.Refer to the sort() implementation defined` |
|      - |  758 | `		 * below for more information on this sceanario.` |
|      - |  759 | `		 */` |
|    ! 0 |  760 | `		return 0;` |
|      - |  761 | `	}` |
|      - |  762 | `	/* Do the comparison */` |
|  30787 |  763 | `	PH7_MemObjInit(pLeft->pMap->pVm,&sObj1);` |
|  30787 |  764 | `	PH7_MemObjInit(pLeft->pMap->pVm,&sObj2);` |
|  30787 |  765 | `	PH7_HashmapExtractNodeValue(pLeft,&sObj1,FALSE);` |
|  30787 |  766 | `	PH7_HashmapExtractNodeValue(pRight,&sObj2,FALSE);` |
|  30787 |  767 | `	rc = PH7_MemObjCmp(&sObj1,&sObj2,bStrict,0);` |
|  30787 |  768 | `	PH7_MemObjRelease(&sObj1);` |
|  30787 |  769 | `	PH7_MemObjRelease(&sObj2);` |
|  30787 |  770 | `	return rc;` |
|  15431 |  771 |  |
|      - |  772 | `/*` |
|      - |  773 | ` * Rehash a node with a 64-bit integer key.` |
|      - |  774 | ` * Refer to [merge_sort(),array_shift()] implementations for more information.` |
|      - |  775 | ` */` |
|   6486 |  776 | `static void HashmapRehashIntNode(ph7_hashmap_node *pEntry)` |
|      2 |  777 |  |
|   6488 |  778 | `	ph7_hashmap *pMap = pEntry->pMap;` |
|      - |  779 | `	sxu32 nBucket;` |
|      - |  780 | `	/* Remove old collision links */` |
|   6488 |  781 | `	if( pEntry->pPrevCollide ){` |
|   5179 |  782 | `		pEntry->pPrevCollide->pNextCollide = pEntry->pNextCollide;` |
|   2593 |  783 | `	}else{` |
|   1311 |  784 | `		pMap->apBucket[pEntry->nHash & (pMap->nSize - 1)] = pEntry->pNextCollide;` |
|      - |  785 | `	}` |
|   6488 |  786 | `	if( pEntry->pNextCollide ){` |
|    610 |  787 | `		pEntry->pNextCollide->pPrevCollide = pEntry->pPrevCollide;` |
|    300 |  788 | `	}` |
|   6488 |  789 | `	pEntry->pNextCollide = pEntry->pPrevCollide = 0;` |
|      - |  790 | `	/* Compute the new hash */` |
|   6488 |  791 | `	pEntry->nHash = pMap->xIntHash(pMap->iNextIdx);` |
|   6488 |  792 | `	pEntry->xKey.iKey = pMap->iNextIdx;` |
|   6488 |  793 | `	nBucket = pEntry->nHash & (pMap->nSize - 1);` |
|      - |  794 | `	/* Link to the new bucket */` |
|   6488 |  795 | `	pEntry->pNextCollide = pMap->apBucket[nBucket];` |
|   6488 |  796 | `	if( pMap->apBucket[nBucket] ){` |
|   5329 |  797 | `		pMap->apBucket[nBucket]->pPrevCollide = pEntry;` |
|   2669 |  798 | `	}` |
|   6488 |  799 | `	pEntry->pNextCollide = pMap->apBucket[nBucket];` |
|   6488 |  800 | `	pMap->apBucket[nBucket] = pEntry;` |
|      - |  801 | `	/* Increment the automatic index */` |
|   6488 |  802 | `	pMap->iNextIdx++;` |
|   6488 |  803 |  |
|      - |  804 | `/*` |
|      - |  805 | ` * Perform a linear search on a given hashmap.` |
|      - |  806 | ` * Write a pointer to the target node on success.` |
|      - |  807 | ` * Otherwise SXERR_NOTFOUND is returned on failure.` |
|      - |  808 | ` * Refer to [array_intersect(),array_diff(),in_array(),...] implementations` |
|      - |  809 | ` * for more information.` |
|      - |  810 | ` */` |
|  16786 |  811 | `static int HashmapFindValue(` |
|      - |  812 | `	ph7_hashmap *pMap,   /* Target hashmap */` |
|      - |  813 | `	ph7_value *pNeedle,  /* Lookup key */` |
|      - |  814 | `	ph7_hashmap_node **ppNode, /* OUT: target node on success  */` |
|      - |  815 | `	int bStrict      /* TRUE for strict comparison */` |
|      - |  816 | `	)` |
|      2 |  817 |  |
|      - |  818 | `	ph7_hashmap_node *pEntry;` |
|      - |  819 | `	ph7_value sVal,*pVal;` |
|      - |  820 | `	ph7_value sNeedle;` |
|      - |  821 | `	sxi32 rc;` |
|      - |  822 | `	sxu32 n;` |
|      - |  823 | `	/* Perform a linear search since we cannot sort the hashmap based on values */` |
|  16788 |  824 | `	pEntry = pMap->pFirst;` |
|  16788 |  825 | `	n = pMap->nEntry;` |
|  16788 |  826 | `	PH7_MemObjInit(pMap->pVm,&sVal);` |
|  16788 |  827 | `	PH7_MemObjInit(pMap->pVm,&sNeedle);` |
|  40141 |  828 | `	for(;;){` |
|  80286 |  829 | `		if( n < 1 ){` |
|     19 |  830 | `			break;` |
|      - |  831 | `		}` |
|      - |  832 | `		/* Extract node value */` |
|  80268 |  833 | `		pVal = HashmapExtractNodeValue(pEntry);` |
|  80268 |  834 | `		if( pVal ){` |
|  80268 |  835 | `			if( (pVal->iFlags\|pNeedle->iFlags) & MEMOBJ_NULL ){` |
|    ! 0 |  836 | `				sxi32 iF1 = pVal->iFlags&~MEMOBJ_AUX;` |
|    ! 0 |  837 | `				sxi32 iF2 = pNeedle->iFlags&~MEMOBJ_AUX;` |
|    ! 0 |  838 | `				if( iF1 == iF2 ){` |
|      - |  839 | `					/* NULL values are equals */` |
|    ! 0 |  840 | `					if( ppNode ){` |
|    ! 0 |  841 | `						*ppNode = pEntry;` |
|    ! 0 |  842 | `					}` |
|    ! 0 |  843 | `					return SXRET_OK;` |
|      - |  844 | `				}` |
|    ! 0 |  845 | `			}else{` |
|      - |  846 | `				/* Duplicate value */` |
|  80268 |  847 | `				PH7_MemObjLoad(pVal,&sVal);` |
|  80268 |  848 | `				PH7_MemObjLoad(pNeedle,&sNeedle);` |
|  80268 |  849 | `				rc = PH7_MemObjCmp(&sNeedle,&sVal,bStrict,0);` |
|  80268 |  850 | `				PH7_MemObjRelease(&sVal);` |
|  80268 |  851 | `				PH7_MemObjRelease(&sNeedle);` |
|  80268 |  852 | `				if( rc == 0 ){` |
|  16770 |  853 | `					if( ppNode ){` |
|      5 |  854 | `						*ppNode = pEntry;` |
|      2 |  855 | `					}` |
|      - |  856 | `					/* Match found*/` |
|  16770 |  857 | `					return SXRET_OK;` |
|      - |  858 | `				}` |
|      - |  859 | `			}` |
|  31748 |  860 | `		}` |
|      - |  861 | `		/* Point to the next entry */` |
|  63500 |  862 | `		pEntry = pEntry->pPrev; /* Reverse link */` |
|  63500 |  863 | `		n--;` |
|      2 |  864 | `	}` |
|      - |  865 | `	/* No such entry */` |
|     19 |  866 | `	return SXERR_NOTFOUND;` |
|   8395 |  867 |  |
|      - |  868 | `/*` |
|      - |  869 | ` * Perform a linear search on a given hashmap but use an user-defined callback` |
|      - |  870 | ` * for values comparison.` |
|      - |  871 | ` * Write a pointer to the target node on success.` |
|      - |  872 | ` * Otherwise SXERR_NOTFOUND is returned on failure.` |
|      - |  873 | ` * Refer to [array_uintersect(),array_udiff()...] implementations` |
|      - |  874 | ` * for more information.` |
|      - |  875 | ` */` |
|     16 |  876 | `static int HashmapFindValueByCallback(` |
|      - |  877 | `	ph7_hashmap *pMap,     /* Target hashmap */` |
|      - |  878 | `	ph7_value *pNeedle,    /* Lookup key */` |
|      - |  879 | `	ph7_value *pCallback,  /* User defined callback */` |
|      - |  880 | `	ph7_hashmap_node **ppNode /* OUT: target node on success */` |
|      - |  881 | `	)` |
|      1 |  882 |  |
|      - |  883 | `	ph7_hashmap_node *pEntry;` |
|      - |  884 | `	ph7_value sResult,*pVal;` |
|      - |  885 | `	ph7_value *apArg[2];    /* Callback arguments */` |
|      - |  886 | `	sxi32 rc;` |
|      - |  887 | `	sxu32 n;` |
|      - |  888 | `	/* Perform a linear search since we cannot sort the array based on values */` |
|     17 |  889 | `	pEntry = pMap->pFirst;` |
|     17 |  890 | `	n = pMap->nEntry;` |
|      - |  891 | `	/* Store callback result here */` |
|     17 |  892 | `	PH7_MemObjInit(pMap->pVm,&sResult);` |
|      - |  893 | `	/* First argument to the callback */` |
|     17 |  894 | `	apArg[0] = pNeedle;` |
|     21 |  895 | `	for(;;){` |
|     43 |  896 | `		if( n < 1 ){` |
|      9 |  897 | `			break;` |
|      - |  898 | `		}` |
|      - |  899 | `		/* Extract node value */` |
|     35 |  900 | `		pVal = HashmapExtractNodeValue(pEntry);` |
|     35 |  901 | `		if( pVal ){` |
|      - |  902 | `			/* Invoke the user callback */` |
|     35 |  903 | `			apArg[1] = pVal; /* Second argument to the callback */` |
|     35 |  904 | `			rc = PH7_VmCallUserFunction(pMap->pVm,pCallback,2,apArg,&sResult);` |
|     35 |  905 | `			if( rc == SXRET_OK ){` |
|      - |  906 | `				/* Extract callback result */` |
|     35 |  907 | `				if( (sResult.iFlags & MEMOBJ_INT) == 0 ){` |
|      - |  908 | `					/* Perform an int cast */` |
|    ! 0 |  909 | `					PH7_MemObjToInteger(&sResult);` |
|    ! 0 |  910 | `				}` |
|     35 |  911 | `				rc = (sxi32)sResult.x.iVal;` |
|     35 |  912 | `				PH7_MemObjRelease(&sResult);` |
|     35 |  913 | `				if( rc == 0 ){` |
|      - |  914 | `					/* Match found*/` |
|      9 |  915 | `					if( ppNode ){` |
|      3 |  916 | `						*ppNode = pEntry;` |
|      1 |  917 | `					}` |
|      9 |  918 | `					return SXRET_OK;` |
|      - |  919 | `				}` |
|     13 |  920 | `			}` |
|     13 |  921 | `		}` |
|      - |  922 | `		/* Point to the next entry */` |
|     27 |  923 | `		pEntry = pEntry->pPrev; /* Reverse link */` |
|     27 |  924 | `		n--;` |
|      1 |  925 | `	}` |
|      - |  926 | `	/* No such entry */` |
|      9 |  927 | `	return SXERR_NOTFOUND;` |
|      9 |  928 |  |
|      - |  929 | `/*` |
|      - |  930 | ` * Compare two hashmaps.` |
|      - |  931 | ` * Return 0 if the hashmaps are equals.Any other value indicates inequality.` |
|      - |  932 | ` * Note on array comparison operators.` |
|      - |  933 | ` *  According to the PHP language reference manual.` |
|      - |  934 | ` *  Array Operators Example 	Name 	Result` |
|      - |  935 | ` *  $a + $b 	Union 	Union of $a and $b.` |
|      - |  936 | ` *  $a == $b 	Equality 	TRUE if $a and $b have the same key/value pairs.` |
|      - |  937 | ` *  $a === $b 	Identity 	TRUE if $a and $b have the same key/value pairs in the same` |
|      - |  938 | ` *                          order and of the same types.` |
|      - |  939 | ` *  $a != $b 	Inequality 	TRUE if $a is not equal to $b.` |
|      - |  940 | ` *  $a <> $b 	Inequality 	TRUE if $a is not equal to $b.` |
|      - |  941 | ` *  $a !== $b 	Non-identity 	TRUE if $a is not identical to $b.` |
|      - |  942 | ` * The + operator returns the right-hand array appended to the left-hand array;` |
|      - |  943 | ` * For keys that exist in both arrays, the elements from the left-hand array will be used` |
|      - |  944 | ` * and the matching elements from the right-hand array will be ignored.` |
|      - |  945 | ` * <?php` |
|      - |  946 | ` * $a = array("a" => "apple", "b" => "banana");` |
|      - |  947 | ` * $b = array("a" => "pear", "b" => "strawberry", "c" => "cherry");` |
|      - |  948 | ` * $c = $a + $b; // Union of $a and $b` |
|      - |  949 | ` * echo "Union of \$a and \$b: \n";` |
|      - |  950 | ` * var_dump($c);` |
|      - |  951 | ` * $c = $b + $a; // Union of $b and $a` |
|      - |  952 | ` * echo "Union of \$b and \$a: \n";` |
|      - |  953 | ` * var_dump($c);` |
|      - |  954 | ` * ?>` |
|      - |  955 | ` * When executed, this script will print the following:` |
|      - |  956 | ` * Union of $a and $b:` |
|      - |  957 | ` * array(3) {` |
|      - |  958 | ` *  ["a"]=>` |
|      - |  959 | ` *  string(5) "apple"` |
|      - |  960 | ` *  ["b"]=>` |
|      - |  961 | ` * string(6) "banana"` |
|      - |  962 | ` *  ["c"]=>` |
|      - |  963 | ` * string(6) "cherry"` |
|      - |  964 | ` * }` |
|      - |  965 | ` * Union of $b and $a:` |
|      - |  966 | ` * array(3) {` |
|      - |  967 | ` * ["a"]=>` |
|      - |  968 | ` * string(4) "pear"` |
|      - |  969 | ` * ["b"]=>` |
|      - |  970 | ` * string(10) "strawberry"` |
|      - |  971 | ` * ["c"]=>` |
|      - |  972 | ` * string(6) "cherry"` |
|      - |  973 | ` * }` |
|      - |  974 | ` * Elements of arrays are equal for the comparison if they have the same key and value.` |
|      - |  975 | ` */` |
|      6 |  976 | `PH7_PRIVATE sxi32 PH7_HashmapCmp(` |
|      - |  977 | `	ph7_hashmap *pLeft,  /* Left hashmap */` |
|      - |  978 | `	ph7_hashmap *pRight, /* Right hashmap */` |
|      - |  979 | `	int bStrict          /* TRUE for strict comparison */` |
|      - |  980 | `	)` |
|      1 |  981 |  |
|      - |  982 | `	ph7_hashmap_node *pLe,*pRe;` |
|      - |  983 | `	sxi32 rc;` |
|      - |  984 | `	sxu32 n;` |
|      7 |  985 | `	if( pLeft == pRight ){` |
|      - |  986 | `		/* Same hashmap instance. This can easily happen since hashmaps are passed by reference.` |
|      - |  987 | `		 * Unlike the zend engine.` |
|      - |  988 | `		 */` |
|    ! 0 |  989 | `		return 0;` |
|      - |  990 | `	}` |
|      7 |  991 | `	if( pLeft->nEntry != pRight->nEntry ){` |
|      - |  992 | `		/* Must have the same number of entries */` |
|    ! 0 |  993 | `		return pLeft->nEntry > pRight->nEntry ? 1 : -1;` |
|      - |  994 | `	}` |
|      - |  995 | `	/* Point to the first inserted entry of the left hashmap */` |
|      7 |  996 | `	pLe = pLeft->pFirst;` |
|      7 |  997 | `	pRe = 0; /* cc warning */` |
|      - |  998 | `	/* Perform the comparison */` |
|      7 |  999 | `	n = pLeft->nEntry;` |
|      7 | 1000 | `	for(;;){` |
|     15 | 1001 | `		if( n < 1 ){` |
|      5 | 1002 | `			break;` |
|      - | 1003 | `		}` |
|     11 | 1004 | `		if( pLe->iType == HASHMAP_INT_NODE){` |
|      - | 1005 | `			/* Int key */` |
|      7 | 1006 | `			rc = HashmapLookupIntKey(&(*pRight),pLe->xKey.iKey,&pRe);` |
|      4 | 1007 | `		}else{` |
|      5 | 1008 | `			SyBlob *pKey = &pLe->xKey.sKey;` |
|      - | 1009 | `			/* Blob key */` |
|      5 | 1010 | `			rc = HashmapLookupBlobKey(&(*pRight),SyBlobData(pKey),SyBlobLength(pKey),&pRe);` |
|      - | 1011 | `		}` |
|     11 | 1012 | `		if( rc != SXRET_OK ){` |
|      - | 1013 | `			/* No such entry in the right side */` |
|    ! 0 | 1014 | `			return 1;` |
|      - | 1015 | `		}` |
|     11 | 1016 | `		rc = 0;` |
|     11 | 1017 | `		if( bStrict ){` |
|      - | 1018 | `			/* Make sure,the keys are of the same type */` |
|      3 | 1019 | `			if( pLe->iType != pRe->iType ){` |
|    ! 0 | 1020 | `				rc = 1;` |
|    ! 0 | 1021 | `			}` |
|      1 | 1022 | `		}` |
|     11 | 1023 | `		if( !rc ){` |
|      - | 1024 | `			/* Compare nodes */` |
|     11 | 1025 | `			rc = HashmapNodeCmp(pLe,pRe,bStrict);` |
|      5 | 1026 | `		}` |
|     11 | 1027 | `		if( rc != 0 ){` |
|      - | 1028 | `			/* Nodes key/value differ */` |
|      3 | 1029 | `			return rc;` |
|      - | 1030 | `		}` |
|      - | 1031 | `		/* Point to the next entry */` |
|      9 | 1032 | `		pLe = pLe->pPrev; /* Reverse link */` |
|      9 | 1033 | `		n--;` |
|      1 | 1034 | `	}` |
|      5 | 1035 | `	return 0; /* Hashmaps are equals */` |
|      4 | 1036 |  |
|      - | 1037 | `/*` |
|      - | 1038 | ` * Duplicate a hashmap node.` |
|      - | 1039 | ` * This function is used by HashmapMerge, HashmapOverwrite and PH7_HashmapDup.` |
|      - | 1040 | ` */` |
| 273898 | 1041 | `static sxi32 HashmapDuplicateNode(` |
|      - | 1042 | `	ph7_hashmap *pDest,` |
|      - | 1043 | `	ph7_hashmap_node *pEntry,` |
|      - | 1044 | `	ph7_value *pVal,` |
|      - | 1045 | `	int iAction /* 0: Merge, 1: Overwrite, 2: Dup */` |
|      - | 1046 | `	)` |
|      2 | 1047 |  |
| 273900 | 1048 | `	ph7_value sSafeVal = *pVal;` |
|      - | 1049 | `	ph7_value sKey;` |
|      - | 1050 | `	sxi32 rc;` |
|      - | 1051 |  |
| 273900 | 1052 | `	if( pEntry->iType == HASHMAP_BLOB_NODE ){` |
|      - | 1053 | `		/* Blob key insertion */` |
|     19 | 1054 | `		PH7_MemObjInitFromString(pDest->pVm,&sKey,0);` |
|     19 | 1055 | `		PH7_MemObjStringAppend(&sKey,(const char *)SyBlobData(&pEntry->xKey.sKey),SyBlobLength(&pEntry->xKey.sKey));` |
|     19 | 1056 | `		rc = PH7_HashmapInsert(pDest,&sKey,&sSafeVal);` |
|     19 | 1057 | `		PH7_MemObjRelease(&sKey);` |
|     10 | 1058 | `	}else{` |
|      - | 1059 | `		/* Int key */` |
| 273882 | 1060 | `		if( iAction == 0 ){ /* Merge */` |
| 273866 | 1061 | `			rc = HashmapInsert(pDest,0/* Automatic index assign */,&sSafeVal);` |
| 136950 | 1062 | `		}else if( iAction == 1 ){ /* Overwrite */` |
|      5 | 1063 | `			PH7_MemObjInitFromInt(pDest->pVm,&sKey,pEntry->xKey.iKey);` |
|      5 | 1064 | `			rc = PH7_HashmapInsert(pDest,&sKey,&sSafeVal);` |
|      5 | 1065 | `			PH7_MemObjRelease(&sKey);` |
|      3 | 1066 | `		}else{ /* Dup */` |
|     14 | 1067 | `			rc = HashmapInsertIntKey(pDest,pEntry->xKey.iKey,&sSafeVal,0,FALSE);` |
|      - | 1068 | `		}` |
|      - | 1069 | `	}` |
| 273900 | 1070 | `	return rc;` |
|      2 | 1071 |  |
|      - | 1072 | `/*` |
|      - | 1073 | ` * Merge two hashmaps.` |
|      - | 1074 | ` * Note on the merge process` |
|      - | 1075 | ` * According to the PHP language reference manual.` |
|      - | 1076 | ` *  Merges the elements of two arrays together so that the values of one are appended` |
|      - | 1077 | ` *  to the end of the previous one. It returns the resulting array (pDest).` |
|      - | 1078 | ` *  If the input arrays have the same string keys, then the later value for that key` |
|      - | 1079 | ` *  will overwrite the previous one. If, however, the arrays contain numeric keys` |
|      - | 1080 | ` *  the later value will not overwrite the original value, but will be appended.` |
|      - | 1081 | ` *  Values in the input array with numeric keys will be renumbered with incrementing` |
|      - | 1082 | ` *  keys starting from zero in the result array.` |
|      - | 1083 | ` */` |
|   1500 | 1084 | `static sxi32 HashmapMerge(ph7_hashmap *pSrc,ph7_hashmap *pDest)` |
|      2 | 1085 |  |
|      - | 1086 | `	ph7_hashmap_node *pEntry;` |
|      - | 1087 | `	ph7_value *pVal;` |
|      - | 1088 | `	sxi32 rc;` |
|      - | 1089 | `	sxu32 n;` |
|   1502 | 1090 | `	if( pSrc == pDest ){` |
|      - | 1091 | `		/* Same map. This can easily happen since hashmaps are passed by reference.` |
|      - | 1092 | `		 * Unlike the zend engine.` |
|      - | 1093 | `		 */` |
|    ! 0 | 1094 | `		return SXRET_OK;` |
|      - | 1095 | `	}` |
|      - | 1096 | `	/* Point to the first inserted entry in the source */` |
|   1502 | 1097 | `	pEntry = pSrc->pFirst;` |
|      - | 1098 | `	/* Perform the merge */` |
| 275372 | 1099 | `	for( n = 0 ; n < pSrc->nEntry ; ++n ){` |
|      - | 1100 | `		/* Extract the node value */` |
| 273872 | 1101 | `		pVal = HashmapExtractNodeValue(pEntry);` |
| 273872 | 1102 | `		if( pVal ){` |
|      - | 1103 | `			/* Make a local copy of the value.` |
|      - | 1104 | `			 * The insertion call below may trigger a memory pool reallocation` |
|      - | 1105 | `			 * which will invalidate the 'pVal' pointer since it points` |
|      - | 1106 | `			 * to the old pool.` |
|      - | 1107 | `			 */` |
| 273872 | 1108 | `			rc = HashmapDuplicateNode(pDest,pEntry,pVal,0);` |
| 136937 | 1109 | `		}else{` |
|    ! 0 | 1110 | `			rc = SXRET_OK;` |
|      - | 1111 | `		}` |
| 273872 | 1112 | `		if( rc != SXRET_OK ){` |
|    ! 0 | 1113 | `			return rc;` |
|      - | 1114 | `		}` |
|      - | 1115 | `		/* Point to the next entry */` |
| 273872 | 1116 | `		pEntry = pEntry->pPrev; /* Reverse link */` |
| 136937 | 1117 | `	}` |
|   1502 | 1118 | `	return SXRET_OK;` |
|    752 | 1119 |  |
|      - | 1120 | `/*` |
|      - | 1121 | ` * Overwrite entries with the same key.` |
|      - | 1122 | ` * Refer to the [array_replace()] implementation for more information.` |
|      - | 1123 | ` *  According to the PHP language reference manual.` |
|      - | 1124 | ` *  array_replace() replaces the values of the first array with the same values` |
|      - | 1125 | ` *  from all the following arrays. If a key from the first array exists in the second` |
|      - | 1126 | ` *  array, its value will be replaced by the value from the second array. If the key` |
|      - | 1127 | ` *  exists in the second array, and not the first, it will be created in the first array.` |
|      - | 1128 | ` *  If a key only exists in the first array, it will be left as is. If several arrays` |
|      - | 1129 | ` *  are passed for replacement, they will be processed in order, the later arrays` |
|      - | 1130 | ` *  overwriting the previous values.` |
|      - | 1131 | ` *  array_replace() is not recursive : it will replace values in the first array` |
|      - | 1132 | ` *  by whatever type is in the second array.` |
|      - | 1133 | ` */` |
|      4 | 1134 | `static sxi32 HashmapOverwrite(ph7_hashmap *pSrc,ph7_hashmap *pDest)` |
|      1 | 1135 |  |
|      - | 1136 | `	ph7_hashmap_node *pEntry;` |
|      - | 1137 | `	ph7_value *pVal;` |
|      - | 1138 | `	sxi32 rc;` |
|      - | 1139 | `	sxu32 n;` |
|      5 | 1140 | `	if( pSrc == pDest ){` |
|      - | 1141 | `		/* Same map. This can easily happen since hashmaps are passed by reference.` |
|      - | 1142 | `		 * Unlike the zend engine.` |
|      - | 1143 | `		 */` |
|    ! 0 | 1144 | `		return SXRET_OK;` |
|      - | 1145 | `	}` |
|      - | 1146 | `	/* Point to the first inserted entry in the source */` |
|      5 | 1147 | `	pEntry = pSrc->pFirst;` |
|      - | 1148 | `	/* Perform the merge */` |
|     13 | 1149 | `	for( n = 0 ; n < pSrc->nEntry ; ++n ){` |
|      - | 1150 | `		/* Extract the node value */` |
|      9 | 1151 | `		pVal = HashmapExtractNodeValue(pEntry);` |
|      9 | 1152 | `		if( pVal ){` |
|      9 | 1153 | `			rc = HashmapDuplicateNode(pDest,pEntry,pVal,1);` |
|      5 | 1154 | `		}else{` |
|    ! 0 | 1155 | `			rc = SXRET_OK;` |
|      - | 1156 | `		}` |
|      9 | 1157 | `		if( rc != SXRET_OK ){` |
|    ! 0 | 1158 | `			return rc;` |
|      - | 1159 | `		}` |
|      - | 1160 | `		/* Point to the next entry */` |
|      9 | 1161 | `		pEntry = pEntry->pPrev; /* Reverse link */` |
|      5 | 1162 | `	}` |
|      5 | 1163 | `	return SXRET_OK;` |
|      3 | 1164 |  |
|      - | 1165 | `/*` |
|      - | 1166 | ` * Duplicate the contents of a hashmap. Store the copy in pDest.` |
|      - | 1167 | ` * Refer to the [array_pad(),array_copy(),...] implementation for more information.` |
|      - | 1168 | ` */` |
|     10 | 1169 | `PH7_PRIVATE sxi32 PH7_HashmapDup(ph7_hashmap *pSrc,ph7_hashmap *pDest)` |
|      2 | 1170 |  |
|      - | 1171 | `	ph7_hashmap_node *pEntry;` |
|      - | 1172 | `	ph7_value *pVal;` |
|      - | 1173 | `	sxi32 rc;` |
|      - | 1174 | `	sxu32 n;` |
|     12 | 1175 | `	if( pSrc == pDest ){` |
|      - | 1176 | `		/* Same map. This can easily happen since hashmaps are passed by reference.` |
|      - | 1177 | `		 * Unlike the zend engine.` |
|      - | 1178 | `		 */` |
|    ! 0 | 1179 | `		return SXRET_OK;` |
|      - | 1180 | `	}` |
|      - | 1181 | `	/* Point to the first inserted entry in the source */` |
|     12 | 1182 | `	pEntry = pSrc->pFirst;` |
|      - | 1183 | `	/* Perform the duplication */` |
|     32 | 1184 | `	for( n = 0 ; n < pSrc->nEntry ; ++n ){` |
|      - | 1185 | `		/* Extract the node value */` |
|     22 | 1186 | `		pVal = HashmapExtractNodeValue(pEntry);` |
|     22 | 1187 | `		if( pVal ){` |
|     22 | 1188 | `			rc = HashmapDuplicateNode(pDest,pEntry,pVal,2);` |
|     12 | 1189 | `		}else{` |
|    ! 0 | 1190 | `			rc = SXRET_OK;` |
|      - | 1191 | `		}` |
|     22 | 1192 | `		if( rc != SXRET_OK ){` |
|    ! 0 | 1193 | `			return rc;` |
|      - | 1194 | `		}` |
|      - | 1195 | `		/* Point to the next entry */` |
|     22 | 1196 | `		pEntry = pEntry->pPrev; /* Reverse link */` |
|     12 | 1197 | `	}` |
|     12 | 1198 | `	return SXRET_OK;` |
|      7 | 1199 |  |
|      - | 1200 | `/*` |
|      - | 1201 | ` * Perform the union of two hashmaps.` |
|      - | 1202 | ` * This operation is performed only if the user uses the '+' operator` |
|      - | 1203 | ` * with a variable holding an array as follows:` |
|      - | 1204 | ` * <?php` |
|      - | 1205 | ` * $a = array("a" => "apple", "b" => "banana");` |
|      - | 1206 | ` * $b = array("a" => "pear", "b" => "strawberry", "c" => "cherry");` |
|      - | 1207 | ` * $c = $a + $b; // Union of $a and $b` |
|      - | 1208 | ` * echo "Union of \$a and \$b: \n";` |
|      - | 1209 | ` * var_dump($c);` |
|      - | 1210 | ` * $c = $b + $a; // Union of $b and $a` |
|      - | 1211 | ` * echo "Union of \$b and \$a: \n";` |
|      - | 1212 | ` * var_dump($c);` |
|      - | 1213 | ` * ?>` |
|      - | 1214 | ` * When executed, this script will print the following:` |
|      - | 1215 | ` * Union of $a and $b:` |
|      - | 1216 | ` * array(3) {` |
|      - | 1217 | ` *  ["a"]=>` |
|      - | 1218 | ` *  string(5) "apple"` |
|      - | 1219 | ` *  ["b"]=>` |
|      - | 1220 | ` * string(6) "banana"` |
|      - | 1221 | ` *  ["c"]=>` |
|      - | 1222 | ` * string(6) "cherry"` |
|      - | 1223 | ` * }` |
|      - | 1224 | ` * Union of $b and $a:` |
|      - | 1225 | ` * array(3) {` |
|      - | 1226 | ` * ["a"]=>` |
|      - | 1227 | ` * string(4) "pear"` |
|      - | 1228 | ` * ["b"]=>` |
|      - | 1229 | ` * string(10) "strawberry"` |
|      - | 1230 | ` * ["c"]=>` |
|      - | 1231 | ` * string(6) "cherry"` |
|      - | 1232 | ` * }` |
|      - | 1233 | ` * The + operator returns the right-hand array appended to the left-hand array;` |
|      - | 1234 | ` * For keys that exist in both arrays, the elements from the left-hand array will be used` |
|      - | 1235 | ` * and the matching elements from the right-hand array will be ignored.` |
|      - | 1236 | ` */` |
|      4 | 1237 | `PH7_PRIVATE sxi32 PH7_HashmapUnion(ph7_hashmap *pLeft,ph7_hashmap *pRight)` |
|      2 | 1238 |  |
|      - | 1239 | `	ph7_hashmap_node *pEntry;` |
|      6 | 1240 | `	sxi32 rc = SXRET_OK;` |
|      - | 1241 | `	ph7_value *pObj;` |
|      - | 1242 | `	sxu32 n;` |
|      6 | 1243 | `	if( pLeft == pRight ){` |
|      - | 1244 | `		/* Same map. This can easily happen since hashmaps are passed by reference.` |
|      - | 1245 | `		 * Unlike the zend engine.` |
|      - | 1246 | `		 */` |
|    ! 0 | 1247 | `		return SXRET_OK;` |
|      - | 1248 | `	}` |
|      - | 1249 | `	/* Perform the union */` |
|      6 | 1250 | `	pEntry = pRight->pFirst;` |
|     16 | 1251 | `	for(n = 0 ; n < pRight->nEntry ; ++n ){` |
|      - | 1252 | `		/* Make sure the given key does not exists in the left array */` |
|     12 | 1253 | `		if( pEntry->iType == HASHMAP_BLOB_NODE ){` |
|      - | 1254 | `			/* BLOB key */` |
|      7 | 1255 | `			if( SXRET_OK !=` |
|      6 | 1256 | `				HashmapLookupBlobKey(&(*pLeft),SyBlobData(&pEntry->xKey.sKey),SyBlobLength(&pEntry->xKey.sKey),0) ){` |
|      3 | 1257 | `					pObj = HashmapExtractNodeValue(pEntry);` |
|      3 | 1258 | `					if( pObj ){` |
|      3 | 1259 | `						ph7_value sSafeVal = *pObj;` |
|      - | 1260 | `						/* Perform the insertion */` |
|      3 | 1261 | `						rc = HashmapInsertBlobKey(&(*pLeft),SyBlobData(&pEntry->xKey.sKey),SyBlobLength(&pEntry->xKey.sKey),` |
|      - | 1262 | `							&sSafeVal,0,FALSE);` |
|      3 | 1263 | `						if( rc != SXRET_OK ){` |
|    ! 0 | 1264 | `							return rc;` |
|      - | 1265 | `						}` |
|      1 | 1266 | `					}` |
|      1 | 1267 | `			}` |
|      4 | 1268 | `		}else{` |
|      - | 1269 | `			/* INT key */` |
|      5 | 1270 | `			if( SXRET_OK != HashmapLookupIntKey(&(*pLeft),pEntry->xKey.iKey,0) ){` |
|    ! 0 | 1271 | `				pObj = HashmapExtractNodeValue(pEntry);` |
|    ! 0 | 1272 | `				if( pObj ){` |
|    ! 0 | 1273 | `					ph7_value sSafeVal = *pObj;` |
|      - | 1274 | `					/* Perform the insertion */` |
|    ! 0 | 1275 | `					rc = HashmapInsertIntKey(&(*pLeft),pEntry->xKey.iKey,&sSafeVal,0,FALSE);` |
|    ! 0 | 1276 | `					if( rc != SXRET_OK ){` |
|    ! 0 | 1277 | `						return rc;` |
|      - | 1278 | `					}` |
|    ! 0 | 1279 | `				}` |
|    ! 0 | 1280 | `			}` |
|      - | 1281 | `		}` |
|      - | 1282 | `		/* Point to the next entry */` |
|     12 | 1283 | `		pEntry = pEntry->pPrev; /* Reverse link */` |
|      7 | 1284 | `	}` |
|      6 | 1285 | `	return SXRET_OK;` |
|      4 | 1286 |  |
|      - | 1287 | `/*` |
|      - | 1288 | ` * Allocate a new hashmap.` |
|      - | 1289 | ` * Return a pointer to the freshly allocated hashmap on success.NULL otherwise.` |
|      - | 1290 | ` */` |
|  40476 | 1291 | `PH7_PRIVATE ph7_hashmap * PH7_NewHashmap(` |
|      - | 1292 | `	ph7_vm *pVm,              /* VM that trigger the hashmap creation */` |
|      - | 1293 | `	sxu32 (*xIntHash)(sxi64), /* Hash function for int keys.NULL otherwise*/` |
|      - | 1294 | `	sxu32 (*xBlobHash)(const void *,sxu32) /* Hash function for BLOB keys.NULL otherwise */` |
|      - | 1295 | `	)` |
|      2 | 1296 |  |
|      - | 1297 | `	ph7_hashmap *pMap;` |
|      - | 1298 | `	/* Allocate a new instance */` |
|  40478 | 1299 | `	pMap = (ph7_hashmap *)SyMemBackendPoolAlloc(&pVm->sAllocator,sizeof(ph7_hashmap));` |
|  40478 | 1300 | `	if( pMap == 0 ){` |
|    ! 0 | 1301 | `		return 0;` |
|      - | 1302 | `	}` |
|      - | 1303 | `	/* Zero the structure */` |
|  40478 | 1304 | `	SyZero(pMap,sizeof(ph7_hashmap));` |
|      - | 1305 | `	/* Fill in the structure */` |
|  40478 | 1306 | `	pMap->pVm = &(*pVm);` |
|  40478 | 1307 | `	pMap->iRef = 1;` |
|      - | 1308 | `	/* Default hash functions */` |
|  40478 | 1309 | `	pMap->xIntHash  = xIntHash ? xIntHash : IntHash;` |
|  40478 | 1310 | `	pMap->xBlobHash = xBlobHash ? xBlobHash : BinHash;` |
|  40478 | 1311 | `	return pMap;` |
|  20240 | 1312 |  |
|      - | 1313 | `/*` |
|      - | 1314 | ` * Install superglobals in the given virtual machine.` |
|      - | 1315 | ` * Note on superglobals.` |
|      - | 1316 | ` *  According to the PHP language reference manual.` |
|      - | 1317 | ` *  Superglobals are built-in variables that are always available in all scopes.` |
|      - | 1318 | `*   Description` |
|      - | 1319 | `*   Several predefined variables in PHP are "superglobals", which means they` |
|      - | 1320 | `*   are available in all scopes throughout a script. There is no need to do` |
|      - | 1321 | `*   global $variable; to access them within functions or methods.` |
|      - | 1322 | `*   These superglobal variables are:` |
|      - | 1323 | `*    $GLOBALS` |
|      - | 1324 | `*    $_SERVER` |
|      - | 1325 | `*    $_GET` |
|      - | 1326 | `*    $_POST` |
|      - | 1327 | `*    $_FILES` |
|      - | 1328 | `*    $_COOKIE` |
|      - | 1329 | `*    $_SESSION` |
|      - | 1330 | `*    $_REQUEST` |
|      - | 1331 | `*    $_ENV` |
|      - | 1332 | `*/` |
|    926 | 1333 | `PH7_PRIVATE sxi32 PH7_HashmapCreateSuper(ph7_vm *pVm)` |
|      2 | 1334 |  |
|      - | 1335 | `	static const char * azSuper[] = {` |
|      - | 1336 | `		"_SERVER",   /* $_SERVER */` |
|      - | 1337 | `		"_GET",      /* $_GET */` |
|      - | 1338 | `		"_POST",     /* $_POST */` |
|      - | 1339 | `		"_FILES",    /* $_FILES */` |
|      - | 1340 | `		"_COOKIE",   /* $_COOKIE */` |
|      - | 1341 | `		"_SESSION",  /* $_SESSION */` |
|      - | 1342 | `		"_REQUEST",  /* $_REQUEST */` |
|      - | 1343 | `		"_ENV",      /* $_ENV */` |
|      - | 1344 | `		"_HEADER",   /* $_HEADER */` |
|      - | 1345 | `		"argv"       /* $argv */` |
|      - | 1346 | `	};` |
|      - | 1347 | `	ph7_hashmap *pMap;` |
|      - | 1348 | `	ph7_value *pObj;` |
|      - | 1349 | `	SyString *pFile;` |
|      - | 1350 | `	sxi32 rc;` |
|      - | 1351 | `	sxu32 n;` |
|      - | 1352 | `	/* Allocate a new hashmap for the $GLOBALS array */` |
|    928 | 1353 | `	pMap = PH7_NewHashmap(&(*pVm),0,0);` |
|    928 | 1354 | `	if( pMap == 0 ){` |
|    ! 0 | 1355 | `		return SXERR_MEM;` |
|      - | 1356 | `	}` |
|    928 | 1357 | `	pVm->pGlobal = pMap;` |
|      - | 1358 | `	/* Reserve a ph7_value for the $GLOBALS array*/` |
|    928 | 1359 | `	pObj = PH7_ReserveMemObj(&(*pVm));` |
|    928 | 1360 | `	if( pObj == 0 ){` |
|    ! 0 | 1361 | `		return SXERR_MEM;` |
|      - | 1362 | `	}` |
|    928 | 1363 | `	PH7_MemObjInitFromArray(&(*pVm),pObj,pMap);` |
|      - | 1364 | `	/* Record object index */` |
|    928 | 1365 | `	pVm->nGlobalIdx = pObj->nIdx;` |
|      - | 1366 | `	/* Install the special $GLOBALS array */` |
|    928 | 1367 | `	rc = SyHashInsert(&pVm->hSuper,(const void *)"GLOBALS",sizeof("GLOBALS")-1,SX_INT_TO_PTR(pVm->nGlobalIdx));` |
|    928 | 1368 | `	if( rc != SXRET_OK ){` |
|    ! 0 | 1369 | `		return rc;` |
|      - | 1370 | `	}` |
|      - | 1371 | `	/* Install superglobals now */` |
|  10188 | 1372 | `	for( n =  0 ; n < SX_ARRAYSIZE(azSuper)  ; n++ ){` |
|      - | 1373 | `		ph7_value *pSuper;` |
|      - | 1374 | `		/* Request an empty array */` |
|   9262 | 1375 | `		pSuper = ph7_new_array(&(*pVm));` |
|   9262 | 1376 | `		if( pSuper == 0 ){` |
|    ! 0 | 1377 | `			return SXERR_MEM;` |
|      - | 1378 | `		}` |
|      - | 1379 | `		/* Install */` |
|   9262 | 1380 | `		rc = ph7_vm_config(&(*pVm),PH7_VM_CONFIG_CREATE_SUPER,azSuper[n]/* Super-global name*/,pSuper/* Super-global value */);` |
|   9262 | 1381 | `		if( rc != SXRET_OK ){` |
|    ! 0 | 1382 | `			return rc;` |
|      - | 1383 | `		}` |
|      - | 1384 | `		/* Release the value now it have been installed */` |
|   9262 | 1385 | `		ph7_release_value(&(*pVm),pSuper);` |
|   4632 | 1386 | `	}` |
|      - | 1387 | `	/* Set some $_SERVER entries */` |
|    928 | 1388 | `	pFile = (SyString *)SySetPeek(&pVm->aFiles);` |
|      - | 1389 | `	/*` |
|      - | 1390 | `	 * 'SCRIPT_FILENAME'` |
|      - | 1391 | `	 * The absolute pathname of the currently executing script.` |
|      - | 1392 | `	 */` |
|   1850 | 1393 | `	ph7_vm_config(pVm,PH7_VM_CONFIG_SERVER_ATTR,` |
|      - | 1394 | `		"SCRIPT_FILENAME",` |
|    463 | 1395 | `		pFile ? pFile->zString : ":Memory:",` |
|    922 | 1396 | `		pFile ? pFile->nByte : sizeof(":Memory:") - 1` |
|      - | 1397 | `		);` |
|      - | 1398 | `	/* All done,all super-global are installed now */` |
|    928 | 1399 | `	return SXRET_OK;` |
|    465 | 1400 |  |
|      - | 1401 | `/*` |
|      - | 1402 | ` * Release a hashmap.` |
|      - | 1403 | ` */` |
|  30248 | 1404 | `PH7_PRIVATE sxi32 PH7_HashmapRelease(ph7_hashmap *pMap,int FreeDS)` |
|      2 | 1405 |  |
|      - | 1406 | `	ph7_hashmap_node *pEntry,*pNext;` |
|  30250 | 1407 | `	ph7_vm *pVm = pMap->pVm;` |
|      - | 1408 | `	sxu32 n;` |
|  30250 | 1409 | `	if( pMap == pVm->pGlobal ){` |
|      - | 1410 | `		/* Cannot delete the $GLOBALS array */` |
|    ! 0 | 1411 | `		PH7_VmThrowError(pMap->pVm,0,PH7_CTX_NOTICE,"$GLOBALS is a read-only array,deletion is forbidden");` |
|    ! 0 | 1412 | `		return SXRET_OK;` |
|      - | 1413 | `	}` |
|      - | 1414 | `	/* Start the release process */` |
|  30250 | 1415 | `	n = 0;` |
|  30250 | 1416 | `	pEntry = pMap->pFirst;` |
| 280820 | 1417 | `	for(;;){` |
| 561642 | 1418 | `		if( n >= pMap->nEntry ){` |
|  30250 | 1419 | `			break;` |
|      - | 1420 | `		}` |
| 531394 | 1421 | `		pNext = pEntry->pPrev; /* Reverse link */` |
|      - | 1422 | `		/* Remove the reference from the foreign table */` |
| 531394 | 1423 | `		PH7_VmRefObjRemove(pVm,pEntry->nValIdx,0,pEntry);` |
| 531394 | 1424 | `		if( (pEntry->iFlags & HASHMAP_NODE_FOREIGN_OBJ) == 0 ){` |
|      - | 1425 | `			/* Restore the ph7_value to the free list */` |
| 531386 | 1426 | `			PH7_VmUnsetMemObj(pVm,pEntry->nValIdx,FALSE);` |
| 265692 | 1427 | `		}` |
|      - | 1428 | `		/* Release the node */` |
| 531394 | 1429 | `		if( pEntry->iType == HASHMAP_BLOB_NODE ){` |
|  51288 | 1430 | `			SyBlobRelease(&pEntry->xKey.sKey);` |
|  25643 | 1431 | `		}` |
| 531394 | 1432 | `		SyMemBackendPoolFree(&pVm->sAllocator,pEntry);` |
|      - | 1433 | `		/* Point to the next entry */` |
| 531394 | 1434 | `		pEntry = pNext;` |
| 531394 | 1435 | `		n++;` |
|      2 | 1436 | `	}` |
|  30250 | 1437 | `	if( pMap->nEntry > 0 ){` |
|      - | 1438 | `		/* Release the hash bucket */` |
|  27030 | 1439 | `		SyMemBackendFree(&pVm->sAllocator,pMap->apBucket);` |
|  13514 | 1440 | `	}` |
|  30250 | 1441 | `	if( FreeDS ){` |
|      - | 1442 | `		/* Free the whole instance */` |
|  30248 | 1443 | `		SyMemBackendPoolFree(&pVm->sAllocator,pMap);` |
|  15125 | 1444 | `	}else{` |
|      - | 1445 | `		/* Keep the instance but reset it's fields */` |
|      3 | 1446 | `		pMap->apBucket = 0;` |
|      3 | 1447 | `		pMap->iNextIdx = 0;` |
|      3 | 1448 | `		pMap->nEntry = pMap->nSize = 0;` |
|      3 | 1449 | `		pMap->pFirst = pMap->pLast = pMap->pCur = 0;` |
|      - | 1450 | `	}` |
|  30250 | 1451 | `	return SXRET_OK;` |
|  15126 | 1452 |  |
|      - | 1453 | `/*` |
|      - | 1454 | ` * Decrement the reference count of a given hashmap.` |
|      - | 1455 | ` * If the count reaches zero which mean no more variables` |
|      - | 1456 | ` * are pointing to this hashmap,then release the whole instance.` |
|      - | 1457 | ` */` |
| 368366 | 1458 | `PH7_PRIVATE void  PH7_HashmapUnref(ph7_hashmap *pMap)` |
|      2 | 1459 |  |
| 368368 | 1460 | `	ph7_vm *pVm = pMap->pVm;` |
|      - | 1461 | `	/* TICKET 1432-49: $GLOBALS is not subject to garbage collection */` |
| 368368 | 1462 | `	pMap->iRef--;` |
| 368368 | 1463 | `	if( pMap->iRef < 1 && pMap != pVm->pGlobal){` |
|  30248 | 1464 | `		PH7_HashmapRelease(pMap,TRUE);` |
|  15123 | 1465 | `	}` |
| 368368 | 1466 |  |
|      - | 1467 | `/*` |
|      - | 1468 | ` * Check if a given key exists in the given hashmap.` |
|      - | 1469 | ` * Write a pointer to the target node on success.` |
|      - | 1470 | ` * Otherwise SXERR_NOTFOUND is returned on failure.` |
|      - | 1471 | ` */` |
|  64216 | 1472 | `PH7_PRIVATE sxi32 PH7_HashmapLookup(` |
|      - | 1473 | `	ph7_hashmap *pMap,        /* Target hashmap */` |
|      - | 1474 | `	ph7_value *pKey,          /* Lookup key */` |
|      - | 1475 | `	ph7_hashmap_node **ppNode /* OUT: Target node on success */` |
|      - | 1476 | `	)` |
|      2 | 1477 |  |
|      - | 1478 | `	sxi32 rc;` |
|  64218 | 1479 | `	if( pMap->nEntry < 1 ){` |
|      - | 1480 | `		/* TICKET 1433-25: Don't bother hashing,the hashmap is empty anyway.` |
|      - | 1481 | `		 */` |
|      7 | 1482 | `		return SXERR_NOTFOUND;` |
|      - | 1483 | `	}` |
|  64212 | 1484 | `	rc = HashmapLookup(&(*pMap),&(*pKey),ppNode);` |
|  64212 | 1485 | `	return rc;` |
|  32110 | 1486 |  |
|      - | 1487 | `/*` |
|      - | 1488 | ` * Insert a given key and it's associated value (if any) in the given` |
|      - | 1489 | ` * hashmap.` |
|      - | 1490 | ` * If a node with the given key already exists in the database` |
|      - | 1491 | ` * then this function overwrite the old value.` |
|      - | 1492 | ` */` |
| 262220 | 1493 | `PH7_PRIVATE sxi32 PH7_HashmapInsert(` |
|      - | 1494 | `	ph7_hashmap *pMap, /* Target hashmap */` |
|      - | 1495 | `	ph7_value *pKey,   /* Lookup key */` |
|      - | 1496 | `	ph7_value *pVal    /* Node value.NULL otherwise */` |
|      - | 1497 | `	)` |
|      2 | 1498 |  |
|      - | 1499 | `	sxi32 rc;` |
| 262222 | 1500 | `	if( pVal && (pVal->iFlags & MEMOBJ_HASHMAP) && (ph7_hashmap *)pVal->x.pOther == pMap->pVm->pGlobal ){` |
|      - | 1501 | `		/*` |
|      - | 1502 | `		 * TICKET 1433-35: Insertion in the $GLOBALS array is forbidden.` |
|      - | 1503 | `		 */` |
|    ! 0 | 1504 | `		PH7_VmThrowError(pMap->pVm,0,PH7_CTX_ERR,"$GLOBALS is a read-only array,insertion is forbidden");` |
|    ! 0 | 1505 | `		return SXRET_OK;` |
|      - | 1506 | `	}` |
| 262222 | 1507 | `	rc = HashmapInsert(&(*pMap),&(*pKey),&(*pVal));` |
| 262222 | 1508 | `	return rc;` |
| 131112 | 1509 |  |
|      - | 1510 | `/*` |
|      - | 1511 | ` * Insert a given key and it's associated value (foreign index) in the given` |
|      - | 1512 | ` * hashmap.` |
|      - | 1513 | ` * This is insertion by reference so be careful to mark the node` |
|      - | 1514 | ` * with the HASHMAP_NODE_FOREIGN_OBJ flag being set.` |
|      - | 1515 | ` * The insertion by reference is triggered when the following` |
|      - | 1516 | ` * expression is encountered.` |
|      - | 1517 | ` * $var = 10;` |
|      - | 1518 | ` *  $a = array(&var);` |
|      - | 1519 | ` * OR` |
|      - | 1520 | ` *  $a[] =& $var;` |
|      - | 1521 | ` * That is,$var is a foreign ph7_value and the $a array have no control` |
|      - | 1522 | ` * over it's contents.` |
|      - | 1523 | ` * Note that the node that hold the foreign ph7_value is automatically` |
|      - | 1524 | ` * removed when the foreign ph7_value is unset.` |
|      - | 1525 | ` * Example:` |
|      - | 1526 | ` *  $var = 10;` |
|      - | 1527 | ` *  $a[] =& $var;` |
|      - | 1528 | ` *  echo count($a).PHP_EOL; //1` |
|      - | 1529 | ` *  //Unset the foreign ph7_value now` |
|      - | 1530 | ` *  unset($var);` |
|      - | 1531 | ` *  echo count($a); //0` |
|      - | 1532 | ` * Note that this is a PH7 eXtension.` |
|      - | 1533 | ` * Refer to the official documentation for more information.` |
|      - | 1534 | ` * If a node with the given key already exists in the database` |
|      - | 1535 | ` * then this function overwrite the old value.` |
|      - | 1536 | ` */` |
|  13980 | 1537 | `PH7_PRIVATE sxi32 PH7_HashmapInsertByRef(` |
|      - | 1538 | `	ph7_hashmap *pMap, /* Target hashmap */` |
|      - | 1539 | `	ph7_value *pKey,   /* Lookup key */` |
|      - | 1540 | `	sxu32 nRefIdx      /* Foreign ph7_value index */` |
|      - | 1541 | `	)` |
|      2 | 1542 |  |
|      - | 1543 | `	sxi32 rc;` |
|  13982 | 1544 | `	if( nRefIdx == pMap->pVm->nGlobalIdx ){` |
|      - | 1545 | `		/*` |
|      - | 1546 | `		 * TICKET 1433-35: Insertion in the $GLOBALS array is forbidden.` |
|      - | 1547 | `		 */` |
|    ! 0 | 1548 | `		PH7_VmThrowError(pMap->pVm,0,PH7_CTX_ERR,"$GLOBALS is a read-only array,insertion is forbidden");` |
|    ! 0 | 1549 | `		return SXRET_OK;` |
|      - | 1550 | `	}` |
|  13982 | 1551 | `	rc = HashmapInsertByRef(&(*pMap),&(*pKey),nRefIdx);` |
|  13982 | 1552 | `	return rc;` |
|   6992 | 1553 |  |
|      - | 1554 | `/*` |
|      - | 1555 | ` * Reset the node cursor of a given hashmap.` |
|      - | 1556 | ` */` |
|  13632 | 1557 | `PH7_PRIVATE void PH7_HashmapResetLoopCursor(ph7_hashmap *pMap)` |
|      2 | 1558 |  |
|      - | 1559 | `	/* Reset the loop cursor */` |
|  13634 | 1560 | `	pMap->pCur = pMap->pFirst;` |
|  13634 | 1561 |  |
|      - | 1562 | `/*` |
|      - | 1563 | ` * Return a pointer to the node currently pointed by the node cursor.` |
|      - | 1564 | ` * If the cursor reaches the end of the list,then this function` |
|      - | 1565 | ` * return NULL.` |
|      - | 1566 | ` * Note that the node cursor is automatically advanced by this function.` |
|      - | 1567 | ` */` |
| 115608 | 1568 | `PH7_PRIVATE ph7_hashmap_node * PH7_HashmapGetNextEntry(ph7_hashmap *pMap)` |
|      2 | 1569 |  |
| 115610 | 1570 | `	ph7_hashmap_node *pCur = pMap->pCur;` |
| 115610 | 1571 | `	if( pCur == 0 ){` |
|      - | 1572 | `		/* End of the list,return null */` |
|   6820 | 1573 | `		return 0;` |
|      - | 1574 | `	}` |
|      - | 1575 | `	/* Advance the node cursor */` |
| 108792 | 1576 | `	pMap->pCur = pCur->pPrev; /* Reverse link */` |
| 108792 | 1577 | `	return pCur;` |
|  57806 | 1578 |  |
|      - | 1579 | `/*` |
|      - | 1580 | ` * Extract a node value.` |
|      - | 1581 | ` */` |
| 280228 | 1582 | `PH7_PRIVATE void PH7_HashmapExtractNodeValue(ph7_hashmap_node *pNode,ph7_value *pValue,int bStore)` |
|      2 | 1583 |  |
| 280230 | 1584 | `	ph7_value *pEntry = HashmapExtractNodeValue(pNode);` |
| 280230 | 1585 | `	if( pEntry ){` |
| 280230 | 1586 | `		if( bStore ){` |
| 108840 | 1587 | `			PH7_MemObjStore(pEntry,pValue);` |
|  54421 | 1588 | `		}else{` |
| 171392 | 1589 | `			PH7_MemObjLoad(pEntry,pValue);` |
|      - | 1590 | `		}` |
| 140189 | 1591 | `	}else{` |
|    ! 0 | 1592 | `		PH7_MemObjRelease(pValue);` |
|      - | 1593 | `	}` |
| 280230 | 1594 |  |
|      - | 1595 | `/*` |
|      - | 1596 | ` * Extract a node key.` |
|      - | 1597 | ` */` |
|  78784 | 1598 | `PH7_PRIVATE void PH7_HashmapExtractNodeKey(ph7_hashmap_node *pNode,ph7_value *pKey)` |
|      2 | 1599 |  |
|      - | 1600 | `	/* Fill with the current key */` |
|  78786 | 1601 | `	if( pNode->iType == HASHMAP_INT_NODE ){` |
|  78654 | 1602 | `		if( SyBlobLength(&pKey->sBlob) > 0 ){` |
|    ! 0 | 1603 | `			SyBlobRelease(&pKey->sBlob);` |
|    ! 0 | 1604 | `		}` |
|  78654 | 1605 | `		pKey->x.iVal = pNode->xKey.iKey;` |
|  78654 | 1606 | `		MemObjSetType(pKey,MEMOBJ_INT);` |
|  39328 | 1607 | `	}else{` |
|    133 | 1608 | `		SyBlobReset(&pKey->sBlob);` |
|    133 | 1609 | `		SyBlobAppend(&pKey->sBlob,SyBlobData(&pNode->xKey.sKey),SyBlobLength(&pNode->xKey.sKey));` |
|    133 | 1610 | `		MemObjSetType(pKey,MEMOBJ_STRING);` |
|      - | 1611 | `	}` |
|  78786 | 1612 |  |
|      - | 1613 | `#ifndef PH7_DISABLE_DISK_IO` |
|      - | 1614 | `/*` |
|      - | 1615 | ` * Store the address of nodes value in the given container.` |
|      - | 1616 | ` * Refer to the [vfprintf(),vprintf(),vsprintf()] implementations` |
|      - | 1617 | ` * defined in 'builtin.c' for more information.` |
|      - | 1618 | ` */` |
|     10 | 1619 | `PH7_PRIVATE int PH7_HashmapValuesToSet(ph7_hashmap *pMap,SySet *pOut)` |
|      1 | 1620 |  |
|     11 | 1621 | `	ph7_hashmap_node *pEntry = pMap->pFirst;` |
|      - | 1622 | `	ph7_value *pValue;` |
|      - | 1623 | `	sxu32 n;` |
|      - | 1624 | `	/* Initialize the container */` |
|     11 | 1625 | `	SySetInit(pOut,&pMap->pVm->sAllocator,sizeof(ph7_value *));` |
|     27 | 1626 | `	for(n = 0 ; n < pMap->nEntry ; n++ ){` |
|      - | 1627 | `		/* Extract node value */` |
|     17 | 1628 | `		pValue = HashmapExtractNodeValue(pEntry);` |
|     17 | 1629 | `		if( pValue ){` |
|     17 | 1630 | `			SySetPut(pOut,(const void *)&pValue);` |
|      8 | 1631 | `		}` |
|      - | 1632 | `		/* Point to the next entry */` |
|     17 | 1633 | `		pEntry = pEntry->pPrev; /* Reverse link */` |
|      9 | 1634 | `	}` |
|      - | 1635 | `	/* Total inserted entries */` |
|     11 | 1636 | `	return (int)SySetUsed(pOut);` |
|      1 | 1637 |  |
|      - | 1638 | `#endif /* PH7_DISABLE_BUILTIN_FUNC \|\| PH7_DISABLE_DISK_IO */` |
|      - | 1639 | `/*` |
|      - | 1640 | ` * Merge sort.` |
|      - | 1641 | ` * The merge sort implementation is based on the one found in the SQLite3 source tree.` |
|      - | 1642 | ` * Status: Public domain` |
|      - | 1643 | ` */` |
|      - | 1644 | `/* Node comparison callback signature */` |
|      - | 1645 | `typedef sxi32 (*ProcNodeCmp)(ph7_hashmap_node *,ph7_hashmap_node *,void *);` |
|      - | 1646 | `/*` |
|      - | 1647 | `** Inputs:` |
|      - | 1648 | `**   a:       A sorted, null-terminated linked list.  (May be null).` |
|      - | 1649 | `**   b:       A sorted, null-terminated linked list.  (May be null).` |
|      - | 1650 | `**   cmp:     A pointer to the comparison function.` |
|      - | 1651 | `**` |
|      - | 1652 | `** Return Value:` |
|      - | 1653 | `**   A pointer to the head of a sorted list containing the elements` |
|      - | 1654 | `**   of both a and b.` |
|      - | 1655 | `**` |
|      - | 1656 | `** Side effects:` |
|      - | 1657 | `**   The "next","prev" pointers for elements in the lists a and b are` |
|      - | 1658 | `**   changed.` |
|      - | 1659 | `*/` |
|  19136 | 1660 | `static ph7_hashmap_node * HashmapNodeMerge(ph7_hashmap_node *pA,ph7_hashmap_node *pB,ProcNodeCmp xCmp,void *pCmpData)` |
|      2 | 1661 |  |
|      - | 1662 | `	ph7_hashmap_node result,*pTail;` |
|      - | 1663 | `    /* Prevent compiler warning */` |
|  19138 | 1664 | `	result.pNext = result.pPrev = 0;` |
|  19138 | 1665 | `	pTail = &result;` |
|  49967 | 1666 | `	while( pA && pB ){` |
|  30831 | 1667 | `		if( xCmp(pA,pB,pCmpData) < 0 ){` |
|  20215 | 1668 | `			pTail->pPrev = pA;` |
|  20215 | 1669 | `			pA->pNext = pTail;` |
|  20215 | 1670 | `			pTail = pA;` |
|  20215 | 1671 | `			pA = pA->pPrev;` |
|  10123 | 1672 | `		}else{` |
|  10618 | 1673 | `			pTail->pPrev = pB;` |
|  10618 | 1674 | `			pB->pNext = pTail;` |
|  10618 | 1675 | `			pTail = pB;` |
|  10618 | 1676 | `			pB = pB->pPrev;` |
|      - | 1677 | `		}` |
|      2 | 1678 | `	}` |
|  19138 | 1679 | `	if( pA ){` |
|  14190 | 1680 | `		pTail->pPrev = pA;` |
|  14190 | 1681 | `		pA->pNext = pTail;` |
|  12046 | 1682 | `	}else if( pB ){` |
|   4850 | 1683 | `		pTail->pPrev = pB;` |
|   4850 | 1684 | `		pB->pNext = pTail;` |
|   2424 | 1685 | `	}else{` |
|    102 | 1686 | `		pTail->pPrev = pTail->pNext = 0;` |
|      - | 1687 | `	}` |
|  19138 | 1688 | `	return result.pPrev;` |
|      2 | 1689 |  |
|      - | 1690 | `/*` |
|      - | 1691 | `** Inputs:` |
|      - | 1692 | `**   Map:       Input hashmap` |
|      - | 1693 | `**   cmp:       A comparison function.` |
|      - | 1694 | `**` |
|      - | 1695 | `** Return Value:` |
|      - | 1696 | `**   Sorted hashmap.` |
|      - | 1697 | `**` |
|      - | 1698 | `** Side effects:` |
|      - | 1699 | `**   The "next" pointers for elements in list are changed.` |
|      - | 1700 | `*/` |
|      - | 1701 | `#define N_SORT_BUCKET  32` |
|    430 | 1702 | `static sxi32 HashmapMergeSort(ph7_hashmap *pMap,ProcNodeCmp xCmp,void *pCmpData)` |
|      2 | 1703 |  |
|      - | 1704 | `	ph7_hashmap_node *a[N_SORT_BUCKET], *p,*pIn;` |
|      - | 1705 | `	sxu32 i;` |
|    432 | 1706 | `	SyZero(a,sizeof(a));` |
|      - | 1707 | `	/* Point to the first inserted entry */` |
|    432 | 1708 | `	pIn = pMap->pFirst;` |
|   6954 | 1709 | `	while( pIn ){` |
|   6524 | 1710 | `		p = pIn;` |
|   6524 | 1711 | `		pIn = p->pPrev;` |
|   6524 | 1712 | `		p->pPrev = 0;` |
|  12330 | 1713 | `		for(i=0; i<N_SORT_BUCKET-1; i++){` |
|  12330 | 1714 | `			if( a[i]==0 ){` |
|   6524 | 1715 | `				a[i] = p;` |
|   6524 | 1716 | `				break;` |
|    ! 0 | 1717 | `			}else{` |
|   5808 | 1718 | `				p = HashmapNodeMerge(a[i],p,xCmp,pCmpData);` |
|   5808 | 1719 | `				a[i] = 0;` |
|      - | 1720 | `			}` |
|   2905 | 1721 | `		}` |
|   6524 | 1722 | `		if( i==N_SORT_BUCKET-1 ){` |
|      - | 1723 | `			/* To get here, there need to be 2^(N_SORT_BUCKET) elements in he input list.` |
|      - | 1724 | `			 * But that is impossible.` |
|      - | 1725 | `			 */` |
|    ! 0 | 1726 | `			a[i] = HashmapNodeMerge(a[i], p,xCmp,pCmpData);` |
|    ! 0 | 1727 | `		}` |
|      2 | 1728 | `	}` |
|    432 | 1729 | `	p = a[0];` |
|  13762 | 1730 | `	for(i=1; i<N_SORT_BUCKET; i++){` |
|  13332 | 1731 | `		p = HashmapNodeMerge(p,a[i],xCmp,pCmpData);` |
|   6667 | 1732 | `	}` |
|    432 | 1733 | `	p->pNext = 0;` |
|      - | 1734 | `	/* Reflect the change */` |
|    432 | 1735 | `	pMap->pFirst = p;` |
|      - | 1736 | `	/* Reset the loop cursor */` |
|    432 | 1737 | `	pMap->pCur = pMap->pFirst;` |
|    432 | 1738 | `	return SXRET_OK;` |
|      2 | 1739 |  |
|      - | 1740 | `/*` |
|      - | 1741 | ` * Node comparison callback.` |
|      - | 1742 | ` * used-by: [sort(),asort(),...]` |
|      - | 1743 | ` */` |
|  30767 | 1744 | `static sxi32 HashmapCmpCallback1(ph7_hashmap_node *pA,ph7_hashmap_node *pB,void *pCmpData)` |
|      2 | 1745 |  |
|      - | 1746 | `	ph7_value sA,sB;` |
|      - | 1747 | `	sxi32 iFlags;` |
|      - | 1748 | `	int rc;` |
|  30769 | 1749 | `	if( pCmpData == 0 ){` |
|      - | 1750 | `		/* Perform a standard comparison */` |
|  30765 | 1751 | `		rc = HashmapNodeCmp(pA,pB,FALSE);` |
|  30765 | 1752 | `		return rc;` |
|      - | 1753 | `	}` |
|      5 | 1754 | `	iFlags = SX_PTR_TO_INT(pCmpData);` |
|      - | 1755 | `	/* Duplicate node values */` |
|      5 | 1756 | `	PH7_MemObjInit(pA->pMap->pVm,&sA);` |
|      5 | 1757 | `	PH7_MemObjInit(pA->pMap->pVm,&sB);` |
|      5 | 1758 | `	PH7_HashmapExtractNodeValue(pA,&sA,FALSE);` |
|      5 | 1759 | `	PH7_HashmapExtractNodeValue(pB,&sB,FALSE);` |
|      5 | 1760 | `	if( iFlags == 5 ){` |
|      - | 1761 | `		/* String cast */` |
|      5 | 1762 | `		if( (sA.iFlags & MEMOBJ_STRING) == 0 ){` |
|    ! 0 | 1763 | `			PH7_MemObjToString(&sA);` |
|    ! 0 | 1764 | `		}` |
|      5 | 1765 | `		if( (sB.iFlags & MEMOBJ_STRING) == 0 ){` |
|    ! 0 | 1766 | `			PH7_MemObjToString(&sB);` |
|    ! 0 | 1767 | `		}` |
|      3 | 1768 | `	}else{` |
|      - | 1769 | `		/* Numeric cast */` |
|    ! 0 | 1770 | `		PH7_MemObjToNumeric(&sA);` |
|    ! 0 | 1771 | `		PH7_MemObjToNumeric(&sB);` |
|      - | 1772 | `	}` |
|      - | 1773 | `	/* Perform the comparison */` |
|      5 | 1774 | `	rc = PH7_MemObjCmp(&sA,&sB,FALSE,0);` |
|      5 | 1775 | `	PH7_MemObjRelease(&sA);` |
|      5 | 1776 | `	PH7_MemObjRelease(&sB);` |
|      5 | 1777 | `	return rc;` |
|  15422 | 1778 |  |
|      - | 1779 | `/*` |
|      - | 1780 | ` * Node comparison callback: Compare nodes by keys only.` |
|      - | 1781 | ` * used-by: [ksort()]` |
|      - | 1782 | ` */` |
|     14 | 1783 | `static sxi32 HashmapCmpCallback2(ph7_hashmap_node *pA,ph7_hashmap_node *pB,void *pCmpData)` |
|      1 | 1784 |  |
|      - | 1785 | `	sxi32 rc;` |
|      7 | 1786 | `	SXUNUSED(pCmpData); /* cc warning */` |
|     15 | 1787 | `	if( pA->iType == HASHMAP_BLOB_NODE && pB->iType == HASHMAP_BLOB_NODE ){` |
|      - | 1788 | `		/* Perform a string comparison */` |
|      5 | 1789 | `		rc = SyBlobCmp(&pA->xKey.sKey,&pB->xKey.sKey);` |
|      3 | 1790 | `	}else{` |
|      - | 1791 | `		SyString sStr;` |
|      - | 1792 | `		sxi64 iA,iB;` |
|      - | 1793 | `		/* Perform a numeric comparison */` |
|     11 | 1794 | `		if( pA->iType == HASHMAP_BLOB_NODE ){` |
|      - | 1795 | `			/* Cast to 64-bit integer */` |
|    ! 0 | 1796 | `			SyStringInitFromBuf(&sStr,SyBlobData(&pA->xKey.sKey),SyBlobLength(&pA->xKey.sKey));` |
|    ! 0 | 1797 | `			if( sStr.nByte < 1 ){` |
|    ! 0 | 1798 | `				iA = 0;` |
|    ! 0 | 1799 | `			}else{` |
|    ! 0 | 1800 | `				SyStrToInt64(sStr.zString,sStr.nByte,(void *)&iA,0);` |
|      - | 1801 | `			}` |
|    ! 0 | 1802 | `		}else{` |
|     11 | 1803 | `			iA = pA->xKey.iKey;` |
|      - | 1804 | `		}` |
|     11 | 1805 | `		if( pB->iType == HASHMAP_BLOB_NODE ){` |
|      - | 1806 | `			/* Cast to 64-bit integer */` |
|    ! 0 | 1807 | `			SyStringInitFromBuf(&sStr,SyBlobData(&pB->xKey.sKey),SyBlobLength(&pB->xKey.sKey));` |
|    ! 0 | 1808 | `			if( sStr.nByte < 1 ){` |
|    ! 0 | 1809 | `				iB = 0;` |
|    ! 0 | 1810 | `			}else{` |
|    ! 0 | 1811 | `				SyStrToInt64(sStr.zString,sStr.nByte,(void *)&iB,0);` |
|      - | 1812 | `			}` |
|    ! 0 | 1813 | `		}else{` |
|     11 | 1814 | `			iB = pB->xKey.iKey;` |
|      - | 1815 | `		}` |
|     11 | 1816 | `		rc = (sxi32)(iA-iB);` |
|      - | 1817 | `	}` |
|      - | 1818 | `	/* Comparison result */` |
|     15 | 1819 | `	return rc;` |
|      1 | 1820 |  |
|      - | 1821 | `/*` |
|      - | 1822 | ` * Node comparison callback.` |
|      - | 1823 | ` * Used by: [rsort(),arsort()];` |
|      - | 1824 | ` */` |
|     12 | 1825 | `static sxi32 HashmapCmpCallback3(ph7_hashmap_node *pA,ph7_hashmap_node *pB,void *pCmpData)` |
|      1 | 1826 |  |
|      - | 1827 | `	ph7_value sA,sB;` |
|      - | 1828 | `	sxi32 iFlags;` |
|      - | 1829 | `	int rc;` |
|     13 | 1830 | `	if( pCmpData == 0 ){` |
|      - | 1831 | `		/* Perform a standard comparison */` |
|     13 | 1832 | `		rc = HashmapNodeCmp(pA,pB,FALSE);` |
|     13 | 1833 | `		return -rc;` |
|      - | 1834 | `	}` |
|    ! 0 | 1835 | `	iFlags = SX_PTR_TO_INT(pCmpData);` |
|      - | 1836 | `	/* Duplicate node values */` |
|    ! 0 | 1837 | `	PH7_MemObjInit(pA->pMap->pVm,&sA);` |
|    ! 0 | 1838 | `	PH7_MemObjInit(pA->pMap->pVm,&sB);` |
|    ! 0 | 1839 | `	PH7_HashmapExtractNodeValue(pA,&sA,FALSE);` |
|    ! 0 | 1840 | `	PH7_HashmapExtractNodeValue(pB,&sB,FALSE);` |
|    ! 0 | 1841 | `	if( iFlags == 5 ){` |
|      - | 1842 | `		/* String cast */` |
|    ! 0 | 1843 | `		if( (sA.iFlags & MEMOBJ_STRING) == 0 ){` |
|    ! 0 | 1844 | `			PH7_MemObjToString(&sA);` |
|    ! 0 | 1845 | `		}` |
|    ! 0 | 1846 | `		if( (sB.iFlags & MEMOBJ_STRING) == 0 ){` |
|    ! 0 | 1847 | `			PH7_MemObjToString(&sB);` |
|    ! 0 | 1848 | `		}` |
|    ! 0 | 1849 | `	}else{` |
|      - | 1850 | `		/* Numeric cast */` |
|    ! 0 | 1851 | `		PH7_MemObjToNumeric(&sA);` |
|    ! 0 | 1852 | `		PH7_MemObjToNumeric(&sB);` |
|      - | 1853 | `	}` |
|      - | 1854 | `	/* Perform the comparison */` |
|    ! 0 | 1855 | `	rc = PH7_MemObjCmp(&sA,&sB,FALSE,0);` |
|    ! 0 | 1856 | `	PH7_MemObjRelease(&sA);` |
|    ! 0 | 1857 | `	PH7_MemObjRelease(&sB);` |
|    ! 0 | 1858 | `	return -rc;` |
|      7 | 1859 |  |
|      - | 1860 | `/*` |
|      - | 1861 | ` * Node comparison callback: Invoke an user-defined callback for the purpose of node comparison.` |
|      - | 1862 | ` * used-by: [usort(),uasort()]` |
|      - | 1863 | ` */` |
|     12 | 1864 | `static sxi32 HashmapCmpCallback4(ph7_hashmap_node *pA,ph7_hashmap_node *pB,void *pCmpData)` |
|      1 | 1865 |  |
|      - | 1866 | `	ph7_value sResult,*pCallback;` |
|      - | 1867 | `	ph7_value *pV1,*pV2;` |
|      - | 1868 | `	ph7_value *apArg[2];  /* Callback arguments */` |
|      - | 1869 | `	sxi32 rc;` |
|      - | 1870 | `	/* Point to the desired callback */` |
|     13 | 1871 | `	pCallback = (ph7_value *)pCmpData;` |
|      - | 1872 | `	/* initialize the result value */` |
|     13 | 1873 | `	PH7_MemObjInit(pA->pMap->pVm,&sResult);` |
|      - | 1874 | `	/* Extract nodes values */` |
|     13 | 1875 | `	pV1 = HashmapExtractNodeValue(pA);` |
|     13 | 1876 | `	pV2 = HashmapExtractNodeValue(pB);` |
|     13 | 1877 | `	apArg[0] = pV1;` |
|     13 | 1878 | `	apArg[1] = pV2;` |
|      - | 1879 | `	/* Invoke the callback */` |
|     13 | 1880 | `	rc = PH7_VmCallUserFunction(pA->pMap->pVm,pCallback,2,apArg,&sResult);` |
|     13 | 1881 | `	if( rc != SXRET_OK ){` |
|      - | 1882 | `		/* An error occured while calling user defined function [i.e: not defined] */` |
|    ! 0 | 1883 | `		rc = -1; /* Set a dummy result */` |
|    ! 0 | 1884 | `	}else{` |
|      - | 1885 | `		/* Extract callback result */` |
|     13 | 1886 | `		if((sResult.iFlags & MEMOBJ_INT) == 0 ){` |
|      - | 1887 | `			/* Perform an int cast */` |
|    ! 0 | 1888 | `			PH7_MemObjToInteger(&sResult);` |
|    ! 0 | 1889 | `		}` |
|     13 | 1890 | `		rc = (sxi32)sResult.x.iVal;` |
|      - | 1891 | `	}` |
|     13 | 1892 | `	PH7_MemObjRelease(&sResult);` |
|      - | 1893 | `	/* Callback result */` |
|     13 | 1894 | `	return rc;` |
|      1 | 1895 |  |
|      - | 1896 | `/*` |
|      - | 1897 | ` * Node comparison callback: Compare nodes by keys only.` |
|      - | 1898 | ` * used-by: [krsort()]` |
|      - | 1899 | ` */` |
|      4 | 1900 | `static sxi32 HashmapCmpCallback5(ph7_hashmap_node *pA,ph7_hashmap_node *pB,void *pCmpData)` |
|      1 | 1901 |  |
|      - | 1902 | `	sxi32 rc;` |
|      2 | 1903 | `	SXUNUSED(pCmpData); /* cc warning */` |
|      5 | 1904 | `	if( pA->iType == HASHMAP_BLOB_NODE && pB->iType == HASHMAP_BLOB_NODE ){` |
|      - | 1905 | `		/* Perform a string comparison */` |
|      5 | 1906 | `		rc = SyBlobCmp(&pA->xKey.sKey,&pB->xKey.sKey);` |
|      3 | 1907 | `	}else{` |
|      - | 1908 | `		SyString sStr;` |
|      - | 1909 | `		sxi64 iA,iB;` |
|      - | 1910 | `		/* Perform a numeric comparison */` |
|    ! 0 | 1911 | `		if( pA->iType == HASHMAP_BLOB_NODE ){` |
|      - | 1912 | `			/* Cast to 64-bit integer */` |
|    ! 0 | 1913 | `			SyStringInitFromBuf(&sStr,SyBlobData(&pA->xKey.sKey),SyBlobLength(&pA->xKey.sKey));` |
|    ! 0 | 1914 | `			if( sStr.nByte < 1 ){` |
|    ! 0 | 1915 | `				iA = 0;` |
|    ! 0 | 1916 | `			}else{` |
|    ! 0 | 1917 | `				SyStrToInt64(sStr.zString,sStr.nByte,(void *)&iA,0);` |
|      - | 1918 | `			}` |
|    ! 0 | 1919 | `		}else{` |
|    ! 0 | 1920 | `			iA = pA->xKey.iKey;` |
|      - | 1921 | `		}` |
|    ! 0 | 1922 | `		if( pB->iType == HASHMAP_BLOB_NODE ){` |
|      - | 1923 | `			/* Cast to 64-bit integer */` |
|    ! 0 | 1924 | `			SyStringInitFromBuf(&sStr,SyBlobData(&pB->xKey.sKey),SyBlobLength(&pB->xKey.sKey));` |
|    ! 0 | 1925 | `			if( sStr.nByte < 1 ){` |
|    ! 0 | 1926 | `				iB = 0;` |
|    ! 0 | 1927 | `			}else{` |
|    ! 0 | 1928 | `				SyStrToInt64(sStr.zString,sStr.nByte,(void *)&iB,0);` |
|      - | 1929 | `			}` |
|    ! 0 | 1930 | `		}else{` |
|    ! 0 | 1931 | `			iB = pB->xKey.iKey;` |
|      - | 1932 | `		}` |
|    ! 0 | 1933 | `		rc = (sxi32)(iA-iB);` |
|      - | 1934 | `	}` |
|      5 | 1935 | `	return -rc; /* Reverse result */` |
|      1 | 1936 |  |
|      - | 1937 | `/*` |
|      - | 1938 | ` * Node comparison callback: Invoke an user-defined callback for the purpose of node comparison.` |
|      - | 1939 | ` * used-by: [uksort()]` |
|      - | 1940 | ` */` |
|      6 | 1941 | `static sxi32 HashmapCmpCallback6(ph7_hashmap_node *pA,ph7_hashmap_node *pB,void *pCmpData)` |
|      1 | 1942 |  |
|      - | 1943 | `	ph7_value sResult,*pCallback;` |
|      - | 1944 | `	ph7_value *apArg[2];  /* Callback arguments */` |
|      - | 1945 | `	ph7_value sK1,sK2;` |
|      - | 1946 | `	sxi32 rc;` |
|      - | 1947 | `	/* Point to the desired callback */` |
|      7 | 1948 | `	pCallback = (ph7_value *)pCmpData;` |
|      - | 1949 | `	/* initialize the result value */` |
|      7 | 1950 | `	PH7_MemObjInit(pA->pMap->pVm,&sResult);` |
|      7 | 1951 | `	PH7_MemObjInit(pA->pMap->pVm,&sK1);` |
|      7 | 1952 | `	PH7_MemObjInit(pA->pMap->pVm,&sK2);` |
|      - | 1953 | `	/* Extract nodes keys */` |
|      7 | 1954 | `	PH7_HashmapExtractNodeKey(pA,&sK1);` |
|      7 | 1955 | `	PH7_HashmapExtractNodeKey(pB,&sK2);` |
|      7 | 1956 | `	apArg[0] = &sK1;` |
|      7 | 1957 | `	apArg[1] = &sK2;` |
|      - | 1958 | `	/* Mark keys as constants */` |
|      7 | 1959 | `	sK1.nIdx = SXU32_HIGH;` |
|      7 | 1960 | `	sK2.nIdx = SXU32_HIGH;` |
|      - | 1961 | `	/* Invoke the callback */` |
|      7 | 1962 | `	rc = PH7_VmCallUserFunction(pA->pMap->pVm,pCallback,2,apArg,&sResult);` |
|      7 | 1963 | `	if( rc != SXRET_OK ){` |
|      - | 1964 | `		/* An error occured while calling user defined function [i.e: not defined] */` |
|    ! 0 | 1965 | `		rc = -1; /* Set a dummy result */` |
|    ! 0 | 1966 | `	}else{` |
|      - | 1967 | `		/* Extract callback result */` |
|      7 | 1968 | `		if((sResult.iFlags & MEMOBJ_INT) == 0 ){` |
|      - | 1969 | `			/* Perform an int cast */` |
|    ! 0 | 1970 | `			PH7_MemObjToInteger(&sResult);` |
|    ! 0 | 1971 | `		}` |
|      7 | 1972 | `		rc = (sxi32)sResult.x.iVal;` |
|      - | 1973 | `	}` |
|      7 | 1974 | `	PH7_MemObjRelease(&sResult);` |
|      7 | 1975 | `	PH7_MemObjRelease(&sK1);` |
|      7 | 1976 | `	PH7_MemObjRelease(&sK2);` |
|      - | 1977 | `	/* Callback result */` |
|      7 | 1978 | `	return rc;` |
|      1 | 1979 |  |
|      - | 1980 | `/*` |
|      - | 1981 | ` * Node comparison callback: Random node comparison.` |
|      - | 1982 | ` * used-by: [shuffle()]` |
|      - | 1983 | ` */` |
|     14 | 1984 | `static sxi32 HashmapCmpCallback7(ph7_hashmap_node *pA,ph7_hashmap_node *pB,void *pCmpData)` |
|      1 | 1985 |  |
|      - | 1986 | `	sxu32 n;` |
|      8 | 1987 | `	SXUNUSED(pB); /* cc warning */` |
|      8 | 1988 | `	SXUNUSED(pCmpData);` |
|      - | 1989 | `	/* Grab a random number */` |
|     15 | 1990 | `	n = PH7_VmRandomNum(pA->pMap->pVm);` |
|      - | 1991 | `	/* if the random number is odd then the first node 'pA' is greater then` |
|      - | 1992 | `	 * the second node 'pB'. Otherwise the reverse is assumed.` |
|      - | 1993 | `	 */` |
|     15 | 1994 | `	return n&1 ? 1 : -1;` |
|      1 | 1995 |  |
|      - | 1996 | `/*` |
|      - | 1997 | ` * Rehash all nodes keys after a merge-sort have been applied.` |
|      - | 1998 | ` * Used by [sort(),usort() and rsort()].` |
|      - | 1999 | ` */` |
|    414 | 2000 | `static void HashmapSortRehash(ph7_hashmap *pMap)` |
|      2 | 2001 |  |
|      - | 2002 | `	ph7_hashmap_node *p,*pLast;` |
|      - | 2003 | `	sxu32 i;` |
|      - | 2004 | `	/* Rehash all entries */` |
|    416 | 2005 | `	pLast = p = pMap->pFirst;` |
|    416 | 2006 | `	pMap->iNextIdx = 0; /* Reset the automatic index */` |
|    416 | 2007 | `	i = 0;` |
|   3441 | 2008 | `	for( ;; ){` |
|   6884 | 2009 | `		if( i >= pMap->nEntry ){` |
|    416 | 2010 | `			pMap->pLast = pLast; /* Fix the last link broken by the merge-sort */` |
|    416 | 2011 | `			break;` |
|      - | 2012 | `		}` |
|   6470 | 2013 | `		if( p->iType == HASHMAP_BLOB_NODE ){` |
|      - | 2014 | `			/* Do not maintain index association as requested by the PHP specification */` |
|      5 | 2015 | `			SyBlobRelease(&p->xKey.sKey);` |
|      - | 2016 | `			/* Change key type */` |
|      5 | 2017 | `			p->iType = HASHMAP_INT_NODE;` |
|      2 | 2018 | `		}` |
|   6470 | 2019 | `		HashmapRehashIntNode(p);` |
|      - | 2020 | `		/* Point to the next entry */` |
|   6470 | 2021 | `		i++;` |
|   6470 | 2022 | `		pLast = p;` |
|   6470 | 2023 | `		p = p->pPrev; /* Reverse link */` |
|      2 | 2024 | `	}` |
|    416 | 2025 |  |
|      - | 2026 | `/*` |
|      - | 2027 | ` * Array functions implementation.` |
|      - | 2028 | ` * Status:` |
|      - | 2029 | ` *  Stable.` |
|      - | 2030 | ` */` |
|      - | 2031 | `/*` |
|      - | 2032 | ` * bool sort(array &$array[,int $sort_flags = SORT_REGULAR ] )` |
|      - | 2033 | ` * Sort an array.` |
|      - | 2034 | ` * Parameters` |
|      - | 2035 | ` *  $array` |
|      - | 2036 | ` *   The input array.` |
|      - | 2037 | ` * $sort_flags` |
|      - | 2038 | ` *  The optional second parameter sort_flags may be used to modify the sorting behavior using these values:` |
|      - | 2039 | ` *  Sorting type flags:` |
|      - | 2040 | ` *   SORT_REGULAR - compare items normally (don't change types)` |
|      - | 2041 | ` *   SORT_NUMERIC - compare items numerically` |
|      - | 2042 | ` *   SORT_STRING - compare items as strings` |
|      - | 2043 | ` * Return` |
|      - | 2044 | ` *  TRUE on success or FALSE on failure.` |
|      - | 2045 | ` *` |
|      - | 2046 | ` */` |
|    754 | 2047 | `static int ph7_hashmap_sort(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      2 | 2048 |  |
|      - | 2049 | `	ph7_hashmap *pMap;` |
|      - | 2050 | `	/* Make sure we are dealing with a valid hashmap */` |
|    756 | 2051 | `	if( nArg < 1 \|\| !ph7_value_is_array(apArg[0]) ){` |
|      - | 2052 | `		/* Missing/Invalid arguments,return FALSE */` |
|    ! 0 | 2053 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 2054 | `		return PH7_OK;` |
|      - | 2055 | `	}` |
|      - | 2056 | `	/* Point to the internal representation of the input hashmap */` |
|    756 | 2057 | `	pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|    756 | 2058 | `	if( pMap->nEntry > 1 ){` |
|    410 | 2059 | `		sxi32 iCmpFlags = 0;` |
|    410 | 2060 | `		if( nArg > 1 ){` |
|      - | 2061 | `			/* Extract comparison flags */` |
|      3 | 2062 | `			iCmpFlags = ph7_value_to_int(apArg[1]);` |
|      3 | 2063 | `			if( iCmpFlags == 3 /* SORT_REGULAR */ ){` |
|    ! 0 | 2064 | `				iCmpFlags = 0; /* Standard comparison */` |
|    ! 0 | 2065 | `			}` |
|      1 | 2066 | `		}` |
|      - | 2067 | `		/* Do the merge sort */` |
|    410 | 2068 | `		HashmapMergeSort(pMap,HashmapCmpCallback1,SX_INT_TO_PTR(iCmpFlags));` |
|      - | 2069 | `		/* Rehash [Do not maintain index association as requested by the PHP specification] */` |
|    410 | 2070 | `		HashmapSortRehash(pMap);` |
|    204 | 2071 | `	}` |
|      - | 2072 | `	/* All done,return TRUE */` |
|    756 | 2073 | `	ph7_result_bool(pCtx,1);` |
|    756 | 2074 | `	return PH7_OK;` |
|    379 | 2075 |  |
|      - | 2076 | `/*` |
|      - | 2077 | ` * bool asort(array &$array[,int $sort_flags = SORT_REGULAR ] )` |
|      - | 2078 | ` *  Sort an array and maintain index association.` |
|      - | 2079 | ` * Parameters` |
|      - | 2080 | ` *  $array` |
|      - | 2081 | ` *   The input array.` |
|      - | 2082 | ` * $sort_flags` |
|      - | 2083 | ` *  The optional second parameter sort_flags may be used to modify the sorting behavior using these values:` |
|      - | 2084 | ` *  Sorting type flags:` |
|      - | 2085 | ` *   SORT_REGULAR - compare items normally (don't change types)` |
|      - | 2086 | ` *   SORT_NUMERIC - compare items numerically` |
|      - | 2087 | ` *   SORT_STRING - compare items as strings` |
|      - | 2088 | ` * Return` |
|      - | 2089 | ` *  TRUE on success or FALSE on failure.` |
|      - | 2090 | ` */` |
|      2 | 2091 | `static int ph7_hashmap_asort(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 2092 |  |
|      - | 2093 | `	ph7_hashmap *pMap;` |
|      - | 2094 | `	/* Make sure we are dealing with a valid hashmap */` |
|      3 | 2095 | `	if( nArg < 1 \|\| !ph7_value_is_array(apArg[0]) ){` |
|      - | 2096 | `		/* Missing/Invalid arguments,return FALSE */` |
|    ! 0 | 2097 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 2098 | `		return PH7_OK;` |
|      - | 2099 | `	}` |
|      - | 2100 | `	/* Point to the internal representation of the input hashmap */` |
|      3 | 2101 | `	pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|      3 | 2102 | `	if( pMap->nEntry > 1 ){` |
|      3 | 2103 | `		sxi32 iCmpFlags = 0;` |
|      3 | 2104 | `		if( nArg > 1 ){` |
|      - | 2105 | `			/* Extract comparison flags */` |
|    ! 0 | 2106 | `			iCmpFlags = ph7_value_to_int(apArg[1]);` |
|    ! 0 | 2107 | `			if( iCmpFlags == 3 /* SORT_REGULAR */ ){` |
|    ! 0 | 2108 | `				iCmpFlags = 0; /* Standard comparison */` |
|    ! 0 | 2109 | `			}` |
|    ! 0 | 2110 | `		}` |
|      - | 2111 | `		/* Do the merge sort */` |
|      3 | 2112 | `		HashmapMergeSort(pMap,HashmapCmpCallback1,SX_INT_TO_PTR(iCmpFlags));` |
|      - | 2113 | `		/* Fix the last link broken by the merge */` |
|      5 | 2114 | `		while(pMap->pLast->pPrev){` |
|      3 | 2115 | `			pMap->pLast = pMap->pLast->pPrev;` |
|      1 | 2116 | `		}` |
|      1 | 2117 | `	}` |
|      - | 2118 | `	/* All done,return TRUE */` |
|      3 | 2119 | `	ph7_result_bool(pCtx,1);` |
|      3 | 2120 | `	return PH7_OK;` |
|      2 | 2121 |  |
|      - | 2122 | `/*` |
|      - | 2123 | ` * bool arsort(array &$array[,int $sort_flags = SORT_REGULAR ] )` |
|      - | 2124 | ` *  Sort an array in reverse order and maintain index association.` |
|      - | 2125 | ` * Parameters` |
|      - | 2126 | ` *  $array` |
|      - | 2127 | ` *   The input array.` |
|      - | 2128 | ` * $sort_flags` |
|      - | 2129 | ` *  The optional second parameter sort_flags may be used to modify the sorting behavior using these values:` |
|      - | 2130 | ` *  Sorting type flags:` |
|      - | 2131 | ` *   SORT_REGULAR - compare items normally (don't change types)` |
|      - | 2132 | ` *   SORT_NUMERIC - compare items numerically` |
|      - | 2133 | ` *   SORT_STRING - compare items as strings` |
|      - | 2134 | ` * Return` |
|      - | 2135 | ` *  TRUE on success or FALSE on failure.` |
|      - | 2136 | ` */` |
|      2 | 2137 | `static int ph7_hashmap_arsort(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 2138 |  |
|      - | 2139 | `	ph7_hashmap *pMap;` |
|      - | 2140 | `	/* Make sure we are dealing with a valid hashmap */` |
|      3 | 2141 | `	if( nArg < 1 \|\| !ph7_value_is_array(apArg[0]) ){` |
|      - | 2142 | `		/* Missing/Invalid arguments,return FALSE */` |
|    ! 0 | 2143 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 2144 | `		return PH7_OK;` |
|      - | 2145 | `	}` |
|      - | 2146 | `	/* Point to the internal representation of the input hashmap */` |
|      3 | 2147 | `	pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|      3 | 2148 | `	if( pMap->nEntry > 1 ){` |
|      3 | 2149 | `		sxi32 iCmpFlags = 0;` |
|      3 | 2150 | `		if( nArg > 1 ){` |
|      - | 2151 | `			/* Extract comparison flags */` |
|    ! 0 | 2152 | `			iCmpFlags = ph7_value_to_int(apArg[1]);` |
|    ! 0 | 2153 | `			if( iCmpFlags == 3 /* SORT_REGULAR */ ){` |
|    ! 0 | 2154 | `				iCmpFlags = 0; /* Standard comparison */` |
|    ! 0 | 2155 | `			}` |
|    ! 0 | 2156 | `		}` |
|      - | 2157 | `		/* Do the merge sort */` |
|      3 | 2158 | `		HashmapMergeSort(pMap,HashmapCmpCallback3,SX_INT_TO_PTR(iCmpFlags));` |
|      - | 2159 | `		/* Fix the last link broken by the merge */` |
|      5 | 2160 | `		while(pMap->pLast->pPrev){` |
|      3 | 2161 | `			pMap->pLast = pMap->pLast->pPrev;` |
|      1 | 2162 | `		}` |
|      1 | 2163 | `	}` |
|      - | 2164 | `	/* All done,return TRUE */` |
|      3 | 2165 | `	ph7_result_bool(pCtx,1);` |
|      3 | 2166 | `	return PH7_OK;` |
|      2 | 2167 |  |
|      - | 2168 | `/*` |
|      - | 2169 | ` * bool ksort(array &$array[,int $sort_flags = SORT_REGULAR ] )` |
|      - | 2170 | ` *  Sort an array by key.` |
|      - | 2171 | ` * Parameters` |
|      - | 2172 | ` *  $array` |
|      - | 2173 | ` *   The input array.` |
|      - | 2174 | ` * $sort_flags` |
|      - | 2175 | ` *  The optional second parameter sort_flags may be used to modify the sorting behavior using these values:` |
|      - | 2176 | ` *  Sorting type flags:` |
|      - | 2177 | ` *   SORT_REGULAR - compare items normally (don't change types)` |
|      - | 2178 | ` *   SORT_NUMERIC - compare items numerically` |
|      - | 2179 | ` *   SORT_STRING - compare items as strings` |
|      - | 2180 | ` * Return` |
|      - | 2181 | ` *  TRUE on success or FALSE on failure.` |
|      - | 2182 | ` */` |
|      4 | 2183 | `static int ph7_hashmap_ksort(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 2184 |  |
|      - | 2185 | `	ph7_hashmap *pMap;` |
|      - | 2186 | `	/* Make sure we are dealing with a valid hashmap */` |
|      5 | 2187 | `	if( nArg < 1 \|\| !ph7_value_is_array(apArg[0]) ){` |
|      - | 2188 | `		/* Missing/Invalid arguments,return FALSE */` |
|    ! 0 | 2189 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 2190 | `		return PH7_OK;` |
|      - | 2191 | `	}` |
|      - | 2192 | `	/* Point to the internal representation of the input hashmap */` |
|      5 | 2193 | `	pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|      5 | 2194 | `	if( pMap->nEntry > 1 ){` |
|      5 | 2195 | `		sxi32 iCmpFlags = 0;` |
|      5 | 2196 | `		if( nArg > 1 ){` |
|      - | 2197 | `			/* Extract comparison flags */` |
|    ! 0 | 2198 | `			iCmpFlags = ph7_value_to_int(apArg[1]);` |
|    ! 0 | 2199 | `			if( iCmpFlags == 3 /* SORT_REGULAR */ ){` |
|    ! 0 | 2200 | `				iCmpFlags = 0; /* Standard comparison */` |
|    ! 0 | 2201 | `			}` |
|    ! 0 | 2202 | `		}` |
|      - | 2203 | `		/* Do the merge sort */` |
|      5 | 2204 | `		HashmapMergeSort(pMap,HashmapCmpCallback2,SX_INT_TO_PTR(iCmpFlags));` |
|      - | 2205 | `		/* Fix the last link broken by the merge */` |
|     15 | 2206 | `		while(pMap->pLast->pPrev){` |
|     11 | 2207 | `			pMap->pLast = pMap->pLast->pPrev;` |
|      1 | 2208 | `		}` |
|      2 | 2209 | `	}` |
|      - | 2210 | `	/* All done,return TRUE */` |
|      5 | 2211 | `	ph7_result_bool(pCtx,1);` |
|      5 | 2212 | `	return PH7_OK;` |
|      3 | 2213 |  |
|      - | 2214 | `/*` |
|      - | 2215 | ` * bool krsort(array &$array[,int $sort_flags = SORT_REGULAR ] )` |
|      - | 2216 | ` *  Sort an array by key in reverse order.` |
|      - | 2217 | ` * Parameters` |
|      - | 2218 | ` *  $array` |
|      - | 2219 | ` *   The input array.` |
|      - | 2220 | ` * $sort_flags` |
|      - | 2221 | ` *  The optional second parameter sort_flags may be used to modify the sorting behavior using these values:` |
|      - | 2222 | ` *  Sorting type flags:` |
|      - | 2223 | ` *   SORT_REGULAR - compare items normally (don't change types)` |
|      - | 2224 | ` *   SORT_NUMERIC - compare items numerically` |
|      - | 2225 | ` *   SORT_STRING - compare items as strings` |
|      - | 2226 | ` * Return` |
|      - | 2227 | ` *  TRUE on success or FALSE on failure.` |
|      - | 2228 | ` */` |
|      2 | 2229 | `static int ph7_hashmap_krsort(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 2230 |  |
|      - | 2231 | `	ph7_hashmap *pMap;` |
|      - | 2232 | `	/* Make sure we are dealing with a valid hashmap */` |
|      3 | 2233 | `	if( nArg < 1 \|\| !ph7_value_is_array(apArg[0]) ){` |
|      - | 2234 | `		/* Missing/Invalid arguments,return FALSE */` |
|    ! 0 | 2235 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 2236 | `		return PH7_OK;` |
|      - | 2237 | `	}` |
|      - | 2238 | `	/* Point to the internal representation of the input hashmap */` |
|      3 | 2239 | `	pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|      3 | 2240 | `	if( pMap->nEntry > 1 ){` |
|      3 | 2241 | `		sxi32 iCmpFlags = 0;` |
|      3 | 2242 | `		if( nArg > 1 ){` |
|      - | 2243 | `			/* Extract comparison flags */` |
|    ! 0 | 2244 | `			iCmpFlags = ph7_value_to_int(apArg[1]);` |
|    ! 0 | 2245 | `			if( iCmpFlags == 3 /* SORT_REGULAR */ ){` |
|    ! 0 | 2246 | `				iCmpFlags = 0; /* Standard comparison */` |
|    ! 0 | 2247 | `			}` |
|    ! 0 | 2248 | `		}` |
|      - | 2249 | `		/* Do the merge sort */` |
|      3 | 2250 | `		HashmapMergeSort(pMap,HashmapCmpCallback5,SX_INT_TO_PTR(iCmpFlags));` |
|      - | 2251 | `		/* Fix the last link broken by the merge */` |
|      7 | 2252 | `		while(pMap->pLast->pPrev){` |
|      5 | 2253 | `			pMap->pLast = pMap->pLast->pPrev;` |
|      1 | 2254 | `		}` |
|      1 | 2255 | `	}` |
|      - | 2256 | `	/* All done,return TRUE */` |
|      3 | 2257 | `	ph7_result_bool(pCtx,1);` |
|      3 | 2258 | `	return PH7_OK;` |
|      2 | 2259 |  |
|      - | 2260 | `/*` |
|      - | 2261 | ` * bool rsort(array &$array[,int $sort_flags = SORT_REGULAR ] )` |
|      - | 2262 | ` * Sort an array in reverse order.` |
|      - | 2263 | ` * Parameters` |
|      - | 2264 | ` *  $array` |
|      - | 2265 | ` *   The input array.` |
|      - | 2266 | ` * $sort_flags` |
|      - | 2267 | ` *  The optional second parameter sort_flags may be used to modify the sorting behavior using these values:` |
|      - | 2268 | ` *  Sorting type flags:` |
|      - | 2269 | ` *   SORT_REGULAR - compare items normally (don't change types)` |
|      - | 2270 | ` *   SORT_NUMERIC - compare items numerically` |
|      - | 2271 | ` *   SORT_STRING - compare items as strings` |
|      - | 2272 | ` * Return` |
|      - | 2273 | ` *  TRUE on success or FALSE on failure.` |
|      - | 2274 | ` */` |
|      2 | 2275 | `static int ph7_hashmap_rsort(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 2276 |  |
|      - | 2277 | `	ph7_hashmap *pMap;` |
|      - | 2278 | `	/* Make sure we are dealing with a valid hashmap */` |
|      3 | 2279 | `	if( nArg < 1 \|\| !ph7_value_is_array(apArg[0]) ){` |
|      - | 2280 | `		/* Missing/Invalid arguments,return FALSE */` |
|    ! 0 | 2281 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 2282 | `		return PH7_OK;` |
|      - | 2283 | `	}` |
|      - | 2284 | `	/* Point to the internal representation of the input hashmap */` |
|      3 | 2285 | `	pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|      3 | 2286 | `	if( pMap->nEntry > 1 ){` |
|      3 | 2287 | `		sxi32 iCmpFlags = 0;` |
|      3 | 2288 | `		if( nArg > 1 ){` |
|      - | 2289 | `			/* Extract comparison flags */` |
|    ! 0 | 2290 | `			iCmpFlags = ph7_value_to_int(apArg[1]);` |
|    ! 0 | 2291 | `			if( iCmpFlags == 3 /* SORT_REGULAR */ ){` |
|    ! 0 | 2292 | `				iCmpFlags = 0; /* Standard comparison */` |
|    ! 0 | 2293 | `			}` |
|    ! 0 | 2294 | `		}` |
|      - | 2295 | `		/* Do the merge sort */` |
|      3 | 2296 | `		HashmapMergeSort(pMap,HashmapCmpCallback3,SX_INT_TO_PTR(iCmpFlags));` |
|      - | 2297 | `		/* Rehash [Do not maintain index association as requested by the PHP specification] */` |
|      3 | 2298 | `		HashmapSortRehash(pMap);` |
|      1 | 2299 | `	}` |
|      - | 2300 | `	/* All done,return TRUE */` |
|      3 | 2301 | `	ph7_result_bool(pCtx,1);` |
|      3 | 2302 | `	return PH7_OK;` |
|      2 | 2303 |  |
|      - | 2304 | `/*` |
|      - | 2305 | ` * bool usort(array &$array,callable $cmp_function)` |
|      - | 2306 | ` *  Sort an array by values using a user-defined comparison function.` |
|      - | 2307 | ` * Parameters` |
|      - | 2308 | ` *  $array` |
|      - | 2309 | ` *   The input array.` |
|      - | 2310 | ` * $cmp_function` |
|      - | 2311 | ` *  The comparison function must return an integer less than, equal to, or greater` |
|      - | 2312 | ` *  than zero if the first argument is considered to be respectively less than, equal` |
|      - | 2313 | ` *  to, or greater than the second.` |
|      - | 2314 | ` *    int callback ( mixed $a, mixed $b )` |
|      - | 2315 | ` * Return` |
|      - | 2316 | ` *  TRUE on success or FALSE on failure.` |
|      - | 2317 | ` */` |
|      2 | 2318 | `static int ph7_hashmap_usort(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 2319 |  |
|      - | 2320 | `	ph7_hashmap *pMap;` |
|      - | 2321 | `	/* Make sure we are dealing with a valid hashmap */` |
|      3 | 2322 | `	if( nArg < 1 \|\| !ph7_value_is_array(apArg[0]) ){` |
|      - | 2323 | `		/* Missing/Invalid arguments,return FALSE */` |
|    ! 0 | 2324 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 2325 | `		return PH7_OK;` |
|      - | 2326 | `	}` |
|      - | 2327 | `	/* Point to the internal representation of the input hashmap */` |
|      3 | 2328 | `	pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|      3 | 2329 | `	if( pMap->nEntry > 1 ){` |
|      3 | 2330 | `		ph7_value *pCallback = 0;` |
|      - | 2331 | `		ProcNodeCmp xCmp;` |
|      3 | 2332 | `		xCmp = HashmapCmpCallback4; /* User-defined function as the comparison callback */` |
|      3 | 2333 | `		if( nArg > 1 && ph7_value_is_callable(apArg[1]) ){` |
|      - | 2334 | `			/* Point to the desired callback */` |
|      3 | 2335 | `			pCallback = apArg[1];` |
|      2 | 2336 | `		}else{` |
|      - | 2337 | `			/* Use the default comparison function */` |
|    ! 0 | 2338 | `			xCmp = HashmapCmpCallback1;` |
|      - | 2339 | `		}` |
|      - | 2340 | `		/* Do the merge sort */` |
|      3 | 2341 | `		HashmapMergeSort(pMap,xCmp,pCallback);` |
|      - | 2342 | `		/* Rehash [Do not maintain index association as requested by the PHP specification] */` |
|      3 | 2343 | `		HashmapSortRehash(pMap);` |
|      1 | 2344 | `	}` |
|      - | 2345 | `	/* All done,return TRUE */` |
|      3 | 2346 | `	ph7_result_bool(pCtx,1);` |
|      3 | 2347 | `	return PH7_OK;` |
|      2 | 2348 |  |
|      - | 2349 | `/*` |
|      - | 2350 | ` * bool uasort(array &$array,callable $cmp_function)` |
|      - | 2351 | ` *  Sort an array by values using a user-defined comparison function` |
|      - | 2352 | ` *  and maintain index association.` |
|      - | 2353 | ` * Parameters` |
|      - | 2354 | ` *  $array` |
|      - | 2355 | ` *   The input array.` |
|      - | 2356 | ` * $cmp_function` |
|      - | 2357 | ` *  The comparison function must return an integer less than, equal to, or greater` |
|      - | 2358 | ` *  than zero if the first argument is considered to be respectively less than, equal` |
|      - | 2359 | ` *  to, or greater than the second.` |
|      - | 2360 | ` *    int callback ( mixed $a, mixed $b )` |
|      - | 2361 | ` * Return` |
|      - | 2362 | ` *  TRUE on success or FALSE on failure.` |
|      - | 2363 | ` */` |
|      2 | 2364 | `static int ph7_hashmap_uasort(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 2365 |  |
|      - | 2366 | `	ph7_hashmap *pMap;` |
|      - | 2367 | `	/* Make sure we are dealing with a valid hashmap */` |
|      3 | 2368 | `	if( nArg < 1 \|\| !ph7_value_is_array(apArg[0]) ){` |
|      - | 2369 | `		/* Missing/Invalid arguments,return FALSE */` |
|    ! 0 | 2370 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 2371 | `		return PH7_OK;` |
|      - | 2372 | `	}` |
|      - | 2373 | `	/* Point to the internal representation of the input hashmap */` |
|      3 | 2374 | `	pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|      3 | 2375 | `	if( pMap->nEntry > 1 ){` |
|      3 | 2376 | `		ph7_value *pCallback = 0;` |
|      - | 2377 | `		ProcNodeCmp xCmp;` |
|      3 | 2378 | `		xCmp = HashmapCmpCallback4; /* User-defined function as the comparison callback */` |
|      3 | 2379 | `		if( nArg > 1 && ph7_value_is_callable(apArg[1]) ){` |
|      - | 2380 | `			/* Point to the desired callback */` |
|      3 | 2381 | `			pCallback = apArg[1];` |
|      2 | 2382 | `		}else{` |
|      - | 2383 | `			/* Use the default comparison function */` |
|    ! 0 | 2384 | `			xCmp = HashmapCmpCallback1;` |
|      - | 2385 | `		}` |
|      - | 2386 | `		/* Do the merge sort */` |
|      3 | 2387 | `		HashmapMergeSort(pMap,xCmp,pCallback);` |
|      - | 2388 | `		/* Fix the last link broken by the merge */` |
|      5 | 2389 | `		while(pMap->pLast->pPrev){` |
|      3 | 2390 | `			pMap->pLast = pMap->pLast->pPrev;` |
|      1 | 2391 | `		}` |
|      1 | 2392 | `	}` |
|      - | 2393 | `	/* All done,return TRUE */` |
|      3 | 2394 | `	ph7_result_bool(pCtx,1);` |
|      3 | 2395 | `	return PH7_OK;` |
|      2 | 2396 |  |
|      - | 2397 | `/*` |
|      - | 2398 | ` * bool uksort(array &$array,callable $cmp_function)` |
|      - | 2399 | ` *  Sort an array by keys using a user-defined comparison` |
|      - | 2400 | ` *  function and maintain index association.` |
|      - | 2401 | ` * Parameters` |
|      - | 2402 | ` *  $array` |
|      - | 2403 | ` *   The input array.` |
|      - | 2404 | ` * $cmp_function` |
|      - | 2405 | ` *  The comparison function must return an integer less than, equal to, or greater` |
|      - | 2406 | ` *  than zero if the first argument is considered to be respectively less than, equal` |
|      - | 2407 | ` *  to, or greater than the second.` |
|      - | 2408 | ` *    int callback ( mixed $a, mixed $b )` |
|      - | 2409 | ` * Return` |
|      - | 2410 | ` *  TRUE on success or FALSE on failure.` |
|      - | 2411 | ` */` |
|      2 | 2412 | `static int ph7_hashmap_uksort(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 2413 |  |
|      - | 2414 | `	ph7_hashmap *pMap;` |
|      - | 2415 | `	/* Make sure we are dealing with a valid hashmap */` |
|      3 | 2416 | `	if( nArg < 1 \|\| !ph7_value_is_array(apArg[0]) ){` |
|      - | 2417 | `		/* Missing/Invalid arguments,return FALSE */` |
|    ! 0 | 2418 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 2419 | `		return PH7_OK;` |
|      - | 2420 | `	}` |
|      - | 2421 | `	/* Point to the internal representation of the input hashmap */` |
|      3 | 2422 | `	pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|      3 | 2423 | `	if( pMap->nEntry > 1 ){` |
|      3 | 2424 | `		ph7_value *pCallback = 0;` |
|      - | 2425 | `		ProcNodeCmp xCmp;` |
|      3 | 2426 | `		xCmp = HashmapCmpCallback6; /* User-defined function as the comparison callback */` |
|      3 | 2427 | `		if( nArg > 1 && ph7_value_is_callable(apArg[1]) ){` |
|      - | 2428 | `			/* Point to the desired callback */` |
|      3 | 2429 | `			pCallback = apArg[1];` |
|      2 | 2430 | `		}else{` |
|      - | 2431 | `			/* Use the default comparison function */` |
|    ! 0 | 2432 | `			xCmp = HashmapCmpCallback2;` |
|      - | 2433 | `		}` |
|      - | 2434 | `		/* Do the merge sort */` |
|      3 | 2435 | `		HashmapMergeSort(pMap,xCmp,pCallback);` |
|      - | 2436 | `		/* Fix the last link broken by the merge */` |
|      3 | 2437 | `		while(pMap->pLast->pPrev){` |
|    ! 0 | 2438 | `			pMap->pLast = pMap->pLast->pPrev;` |
|    ! 0 | 2439 | `		}` |
|      1 | 2440 | `	}` |
|      - | 2441 | `	/* All done,return TRUE */` |
|      3 | 2442 | `	ph7_result_bool(pCtx,1);` |
|      3 | 2443 | `	return PH7_OK;` |
|      2 | 2444 |  |
|      - | 2445 | `/*` |
|      - | 2446 | ` * bool shuffle(array &$array)` |
|      - | 2447 | ` *  shuffles (randomizes the order of the elements in) an array.` |
|      - | 2448 | ` * Parameters` |
|      - | 2449 | ` *  $array` |
|      - | 2450 | ` *   The input array.` |
|      - | 2451 | ` * Return` |
|      - | 2452 | ` *  TRUE on success or FALSE on failure.` |
|      - | 2453 | ` *` |
|      - | 2454 | ` */` |
|      2 | 2455 | `static int ph7_hashmap_shuffle(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 2456 |  |
|      - | 2457 | `	ph7_hashmap *pMap;` |
|      - | 2458 | `	/* Make sure we are dealing with a valid hashmap */` |
|      3 | 2459 | `	if( nArg < 1 \|\| !ph7_value_is_array(apArg[0]) ){` |
|      - | 2460 | `		/* Missing/Invalid arguments,return FALSE */` |
|    ! 0 | 2461 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 2462 | `		return PH7_OK;` |
|      - | 2463 | `	}` |
|      - | 2464 | `	/* Point to the internal representation of the input hashmap */` |
|      3 | 2465 | `	pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|      3 | 2466 | `	if( pMap->nEntry > 1 ){` |
|      - | 2467 | `		/* Do the merge sort */` |
|      3 | 2468 | `		HashmapMergeSort(pMap,HashmapCmpCallback7,0);` |
|      - | 2469 | `		/* Fix the last link broken by the merge */` |
|     10 | 2470 | `		while(pMap->pLast->pPrev){` |
|      8 | 2471 | `			pMap->pLast = pMap->pLast->pPrev;` |
|      1 | 2472 | `		}` |
|      1 | 2473 | `	}` |
|      - | 2474 | `	/* All done,return TRUE */` |
|      3 | 2475 | `	ph7_result_bool(pCtx,1);` |
|      3 | 2476 | `	return PH7_OK;` |
|      2 | 2477 |  |
|      - | 2478 | `/*` |
|      - | 2479 | ` * int count(array $var [, int $mode = COUNT_NORMAL ])` |
|      - | 2480 | ` *   Count all elements in an array, or something in an object.` |
|      - | 2481 | ` * Parameters` |
|      - | 2482 | ` *  $var` |
|      - | 2483 | ` *   The array or the object.` |
|      - | 2484 | ` * $mode` |
|      - | 2485 | ` *  If the optional mode parameter is set to COUNT_RECURSIVE (or 1), count()` |
|      - | 2486 | ` *  will recursively count the array. This is particularly useful for counting` |
|      - | 2487 | ` *  all the elements of a multidimensional array. count() does not detect infinite` |
|      - | 2488 | ` *  recursion.` |
|      - | 2489 | ` * Return` |
|      - | 2490 | ` *  Returns the number of elements in the array.` |
|      - | 2491 | ` */` |
|    436 | 2492 | `static int ph7_hashmap_count(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      2 | 2493 |  |
|    438 | 2494 | `	int bRecursive = FALSE;` |
|      - | 2495 | `	sxi64 iCount;` |
|    438 | 2496 | `	if( nArg < 1 ){` |
|      - | 2497 | `		/* Missing arguments,return 0 */` |
|    ! 0 | 2498 | `		ph7_result_int(pCtx,0);` |
|    ! 0 | 2499 | `		return PH7_OK;` |
|      - | 2500 | `	}` |
|    438 | 2501 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|      - | 2502 | `		/* TICKET 1433-19: Handle objects */` |
|      3 | 2503 | `		int res = !ph7_value_is_null(apArg[0]);` |
|      3 | 2504 | `		ph7_result_int(pCtx,res);` |
|      3 | 2505 | `		return PH7_OK;` |
|      - | 2506 | `	}` |
|    436 | 2507 | `	if( nArg > 1 ){` |
|      - | 2508 | `		/* Recursive count? */` |
|     31 | 2509 | `		bRecursive = ph7_value_to_int(apArg[1]) == 1 /* COUNT_RECURSIVE */;` |
|     15 | 2510 | `	}` |
|      - | 2511 | `	/* Count */` |
|    436 | 2512 | `	iCount = HashmapCount((ph7_hashmap *)apArg[0]->x.pOther,bRecursive,0);` |
|    436 | 2513 | `	ph7_result_int64(pCtx,iCount);` |
|    436 | 2514 | `	return PH7_OK;` |
|    220 | 2515 |  |
|      - | 2516 | `/*` |
|      - | 2517 | ` * bool array_key_exists(value $key,array $search)` |
|      - | 2518 | ` *  Checks if the given key or index exists in the array.` |
|      - | 2519 | ` * Parameters` |
|      - | 2520 | ` * $key` |
|      - | 2521 | ` *   Value to check.` |
|      - | 2522 | ` * $search` |
|      - | 2523 | ` *  An array with keys to check.` |
|      - | 2524 | ` * Return` |
|      - | 2525 | ` *  TRUE on success or FALSE on failure.` |
|      - | 2526 | ` */` |
|     32 | 2527 | `static int ph7_hashmap_key_exists(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 2528 |  |
|      - | 2529 | `	sxi32 rc;` |
|     33 | 2530 | `	if( nArg < 2 ){` |
|      - | 2531 | `		/* Missing arguments,return FALSE */` |
|      7 | 2532 | `		ph7_result_bool(pCtx,0);` |
|      7 | 2533 | `		return PH7_OK;` |
|      - | 2534 | `	}` |
|      - | 2535 | `	/* Make sure we are dealing with a valid hashmap */` |
|     27 | 2536 | `	if( !ph7_value_is_array(apArg[1]) ){` |
|      - | 2537 | `		/* Invalid argument,return FALSE */` |
|      3 | 2538 | `		ph7_result_bool(pCtx,0);` |
|      3 | 2539 | `		return PH7_OK;` |
|      - | 2540 | `	}` |
|      - | 2541 | `	/* Perform the lookup */` |
|     25 | 2542 | `	rc = PH7_HashmapLookup((ph7_hashmap *)apArg[1]->x.pOther,apArg[0],0);` |
|      - | 2543 | `	/* lookup result */` |
|     25 | 2544 | `	ph7_result_bool(pCtx,rc == SXRET_OK ? 1 : 0);` |
|     25 | 2545 | `	return PH7_OK;` |
|     17 | 2546 |  |
|      - | 2547 | `/*` |
|      - | 2548 | ` * value array_pop(array $array)` |
|      - | 2549 | ` *   POP the last inserted element from the array.` |
|      - | 2550 | ` * Parameter` |
|      - | 2551 | ` *  The array to get the value from.` |
|      - | 2552 | ` * Return` |
|      - | 2553 | ` *  Poped value or NULL on failure.` |
|      - | 2554 | ` */` |
|      4 | 2555 | `static int ph7_hashmap_pop(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 2556 |  |
|      - | 2557 | `	ph7_hashmap *pMap;` |
|      5 | 2558 | `	if( nArg < 1 ){` |
|      - | 2559 | `		/* Missing arguments,return null */` |
|    ! 0 | 2560 | `		ph7_result_null(pCtx);` |
|    ! 0 | 2561 | `		return PH7_OK;` |
|      - | 2562 | `	}` |
|      - | 2563 | `	/* Make sure we are dealing with a valid hashmap */` |
|      5 | 2564 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|      - | 2565 | `		/* Invalid argument,return null */` |
|    ! 0 | 2566 | `		ph7_result_null(pCtx);` |
|    ! 0 | 2567 | `		return PH7_OK;` |
|      - | 2568 | `	}` |
|      5 | 2569 | `	pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|      5 | 2570 | `	if( pMap->nEntry < 1 ){` |
|      - | 2571 | `		/* Noting to pop,return NULL */` |
|      3 | 2572 | `		ph7_result_null(pCtx);` |
|      2 | 2573 | `	}else{` |
|      3 | 2574 | `		ph7_hashmap_node *pLast = pMap->pLast;` |
|      - | 2575 | `		ph7_value *pObj;` |
|      3 | 2576 | `		pObj = HashmapExtractNodeValue(pLast);` |
|      3 | 2577 | `		if( pObj ){` |
|      - | 2578 | `			/* Node value */` |
|      3 | 2579 | `			ph7_result_value(pCtx,pObj);` |
|      - | 2580 | `			/* Unlink the node */` |
|      3 | 2581 | `			PH7_HashmapUnlinkNode(pLast,TRUE);` |
|      2 | 2582 | `		}else{` |
|    ! 0 | 2583 | `			ph7_result_null(pCtx);` |
|      - | 2584 | `		}` |
|      - | 2585 | `		/* Reset the cursor */` |
|      3 | 2586 | `		pMap->pCur = pMap->pFirst;` |
|      - | 2587 | `	}` |
|      5 | 2588 | `	return PH7_OK;` |
|      3 | 2589 |  |
|      - | 2590 | `/*` |
|      - | 2591 | ` * int array_push($array,$var,...)` |
|      - | 2592 | ` *   Push one or more elements onto the end of array. (Stack insertion)` |
|      - | 2593 | ` * Parameters` |
|      - | 2594 | ` *  array` |
|      - | 2595 | ` *    The input array.` |
|      - | 2596 | ` *  var` |
|      - | 2597 | ` *   On or more value to push.` |
|      - | 2598 | ` * Return` |
|      - | 2599 | ` *  New array count (including old items).` |
|      - | 2600 | ` */` |
|      2 | 2601 | `static int ph7_hashmap_push(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 2602 |  |
|      - | 2603 | `	ph7_hashmap *pMap;` |
|      - | 2604 | `	sxi32 rc;` |
|      - | 2605 | `	int i;` |
|      3 | 2606 | `	if( nArg < 1 ){` |
|      - | 2607 | `		/* Missing arguments,return 0 */` |
|    ! 0 | 2608 | `		ph7_result_int(pCtx,0);` |
|    ! 0 | 2609 | `		return PH7_OK;` |
|      - | 2610 | `	}` |
|      - | 2611 | `	/* Make sure we are dealing with a valid hashmap */` |
|      3 | 2612 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|      - | 2613 | `		/* Invalid argument,return 0 */` |
|    ! 0 | 2614 | `		ph7_result_int(pCtx,0);` |
|    ! 0 | 2615 | `		return PH7_OK;` |
|      - | 2616 | `	}` |
|      - | 2617 | `	/* Point to the internal representation of the input hashmap */` |
|      3 | 2618 | `	pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|      - | 2619 | `	/* Start pushing given values */` |
|      7 | 2620 | `	for( i = 1 ; i < nArg ; ++i ){` |
|      5 | 2621 | `		rc = PH7_HashmapInsert(pMap,0,apArg[i]);` |
|      5 | 2622 | `		if( rc != SXRET_OK ){` |
|    ! 0 | 2623 | `			break;` |
|      - | 2624 | `		}` |
|      3 | 2625 | `	}` |
|      - | 2626 | `	/* Return the new count */` |
|      3 | 2627 | `	ph7_result_int64(pCtx,(sxi64)pMap->nEntry);` |
|      3 | 2628 | `	return PH7_OK;` |
|      2 | 2629 |  |
|      - | 2630 | `/*` |
|      - | 2631 | ` * value array_shift(array $array)` |
|      - | 2632 | ` *   Shift an element off the beginning of array.` |
|      - | 2633 | ` * Parameter` |
|      - | 2634 | ` *  The array to get the value from.` |
|      - | 2635 | ` * Return` |
|      - | 2636 | ` *  Shifted value or NULL on failure.` |
|      - | 2637 | ` */` |
|     16 | 2638 | `static int ph7_hashmap_shift(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      2 | 2639 |  |
|      - | 2640 | `	ph7_hashmap *pMap;` |
|     18 | 2641 | `	if( nArg < 1 ){` |
|      - | 2642 | `		/* Missing arguments,return null */` |
|    ! 0 | 2643 | `		ph7_result_null(pCtx);` |
|    ! 0 | 2644 | `		return PH7_OK;` |
|      - | 2645 | `	}` |
|      - | 2646 | `	/* Make sure we are dealing with a valid hashmap */` |
|     18 | 2647 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|      - | 2648 | `		/* Invalid argument,return null */` |
|    ! 0 | 2649 | `		ph7_result_null(pCtx);` |
|    ! 0 | 2650 | `		return PH7_OK;` |
|      - | 2651 | `	}` |
|      - | 2652 | `	/* Point to the internal representation of the hashmap */` |
|     18 | 2653 | `	pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|     18 | 2654 | `	if( pMap->nEntry < 1 ){` |
|      - | 2655 | `		/* Empty hashmap,return NULL */` |
|      3 | 2656 | `		ph7_result_null(pCtx);` |
|      2 | 2657 | `	}else{` |
|     16 | 2658 | `		ph7_hashmap_node *pEntry = pMap->pFirst;` |
|      - | 2659 | `		ph7_value *pObj;` |
|      - | 2660 | `		sxu32 n;` |
|     16 | 2661 | `		pObj = HashmapExtractNodeValue(pEntry);` |
|     16 | 2662 | `		if( pObj ){` |
|      - | 2663 | `			/* Node value */` |
|     16 | 2664 | `			ph7_result_value(pCtx,pObj);` |
|      - | 2665 | `			/* Unlink the first node */` |
|     16 | 2666 | `			PH7_HashmapUnlinkNode(pEntry,TRUE);` |
|      9 | 2667 | `		}else{` |
|    ! 0 | 2668 | `			ph7_result_null(pCtx);` |
|      - | 2669 | `		}` |
|      - | 2670 | `		/* Rehash all int keys */` |
|     16 | 2671 | `		n = pMap->nEntry;` |
|     16 | 2672 | `		pEntry = pMap->pFirst;` |
|     16 | 2673 | `		pMap->iNextIdx = 0; /* Reset the automatic index */` |
|     16 | 2674 | `		for(;;){` |
|     34 | 2675 | `			if( n < 1 ){` |
|     16 | 2676 | `				break;` |
|      - | 2677 | `			}` |
|     20 | 2678 | `			if( pEntry->iType == HASHMAP_INT_NODE ){` |
|     20 | 2679 | `				HashmapRehashIntNode(pEntry);` |
|      9 | 2680 | `			}` |
|      - | 2681 | `			/* Point to the next entry */` |
|     20 | 2682 | `			pEntry = pEntry->pPrev; /* Reverse link */` |
|     20 | 2683 | `			n--;` |
|      2 | 2684 | `		}` |
|      - | 2685 | `		/* Reset the cursor */` |
|     16 | 2686 | `		pMap->pCur = pMap->pFirst;` |
|      - | 2687 | `	}` |
|     18 | 2688 | `	return PH7_OK;` |
|     10 | 2689 |  |
|      - | 2690 | `/*` |
|      - | 2691 | ` * Extract the node cursor value.` |
|      - | 2692 | ` */` |
|     24 | 2693 | `static sxi32 HashmapCurrentValue(ph7_context *pCtx,ph7_hashmap *pMap,int iDirection)` |
|      1 | 2694 |  |
|     25 | 2695 | `	ph7_hashmap_node *pCur = pMap->pCur;` |
|      - | 2696 | `	ph7_value *pVal;` |
|     25 | 2697 | `	if( pCur == 0 ){` |
|      - | 2698 | `		/* Cursor does not point to anything,return FALSE */` |
|    ! 0 | 2699 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 2700 | `		return PH7_OK;` |
|      - | 2701 | `	}` |
|     25 | 2702 | `	if( iDirection != 0 ){` |
|      9 | 2703 | `		if( iDirection > 0 ){` |
|      - | 2704 | `			/* Point to the next entry */` |
|      7 | 2705 | `			pMap->pCur = pCur->pPrev; /* Reverse link */` |
|      7 | 2706 | `			pCur = pMap->pCur;` |
|      4 | 2707 | `		}else{` |
|      - | 2708 | `			/* Point to the previous entry */` |
|      3 | 2709 | `			pMap->pCur = pCur->pNext; /* Reverse link */` |
|      3 | 2710 | `			pCur = pMap->pCur;` |
|      - | 2711 | `		}` |
|      9 | 2712 | `		if( pCur == 0 ){` |
|      - | 2713 | `			/* End of input reached,return FALSE */` |
|    ! 0 | 2714 | `			ph7_result_bool(pCtx,0);` |
|    ! 0 | 2715 | `			return PH7_OK;` |
|      - | 2716 | `		}` |
|      4 | 2717 | `	}` |
|      - | 2718 | `	/* Point to the desired element */` |
|     25 | 2719 | `	pVal = HashmapExtractNodeValue(pCur);` |
|     25 | 2720 | `	if( pVal ){` |
|     25 | 2721 | `		ph7_result_value(pCtx,pVal);` |
|     13 | 2722 | `	}else{` |
|    ! 0 | 2723 | `		ph7_result_bool(pCtx,0);` |
|      - | 2724 | `	}` |
|     25 | 2725 | `	return PH7_OK;` |
|     13 | 2726 |  |
|      - | 2727 | `/*` |
|      - | 2728 | ` * value current(array $array)` |
|      - | 2729 | ` *  Return the current element in an array.` |
|      - | 2730 | ` * Parameter` |
|      - | 2731 | ` *  $input: The input array.` |
|      - | 2732 | ` * Return` |
|      - | 2733 | ` *  The current() function simply returns the value of the array element that's currently` |
|      - | 2734 | ` *  being pointed to by the internal pointer. It does not move the pointer in any way.` |
|      - | 2735 | ` *  If the internal pointer points beyond the end of the elements list or the array` |
|      - | 2736 | ` *  is empty, current() returns FALSE.` |
|      - | 2737 | ` */` |
|     10 | 2738 | `static int ph7_hashmap_current(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 2739 |  |
|     11 | 2740 | `	if( nArg < 1 ){` |
|      - | 2741 | `		/* Missing arguments,return FALSE */` |
|    ! 0 | 2742 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 2743 | `		return PH7_OK;` |
|      - | 2744 | `	}` |
|      - | 2745 | `	/* Make sure we are dealing with a valid hashmap */` |
|     11 | 2746 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|      - | 2747 | `		/* Invalid argument,return FALSE */` |
|    ! 0 | 2748 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 2749 | `		return PH7_OK;` |
|      - | 2750 | `	}` |
|     11 | 2751 | `	HashmapCurrentValue(&(*pCtx),(ph7_hashmap *)apArg[0]->x.pOther,0);` |
|     11 | 2752 | `	return PH7_OK;` |
|      6 | 2753 |  |
|      - | 2754 | `/*` |
|      - | 2755 | ` * value next(array $input)` |
|      - | 2756 | ` *  Advance the internal array pointer of an array.` |
|      - | 2757 | ` * Parameter` |
|      - | 2758 | ` *  $input: The input array.` |
|      - | 2759 | ` * Return` |
|      - | 2760 | ` *  next() behaves like current(), with one difference. It advances the internal array` |
|      - | 2761 | ` *  pointer one place forward before returning the element value. That means it returns` |
|      - | 2762 | ` *  the next array value and advances the internal array pointer by one.` |
|      - | 2763 | ` */` |
|      6 | 2764 | `static int ph7_hashmap_next(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 2765 |  |
|      7 | 2766 | `	if( nArg < 1 ){` |
|      - | 2767 | `		/* Missing arguments,return FALSE */` |
|    ! 0 | 2768 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 2769 | `		return PH7_OK;` |
|      - | 2770 | `	}` |
|      - | 2771 | `	/* Make sure we are dealing with a valid hashmap */` |
|      7 | 2772 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|      - | 2773 | `		/* Invalid argument,return FALSE */` |
|    ! 0 | 2774 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 2775 | `		return PH7_OK;` |
|      - | 2776 | `	}` |
|      7 | 2777 | `	HashmapCurrentValue(&(*pCtx),(ph7_hashmap *)apArg[0]->x.pOther,1);` |
|      7 | 2778 | `	return PH7_OK;` |
|      4 | 2779 |  |
|      - | 2780 | `/*` |
|      - | 2781 | ` * value prev(array $input)` |
|      - | 2782 | ` *  Rewind the internal array pointer.` |
|      - | 2783 | ` * Parameter` |
|      - | 2784 | ` *  $input: The input array.` |
|      - | 2785 | ` * Return` |
|      - | 2786 | ` *  Returns the array value in the previous place that's pointed` |
|      - | 2787 | ` *  to by the internal array pointer, or FALSE if there are no more` |
|      - | 2788 | ` *  elements.` |
|      - | 2789 | ` */` |
|      2 | 2790 | `static int ph7_hashmap_prev(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 2791 |  |
|      3 | 2792 | `	if( nArg < 1 ){` |
|      - | 2793 | `		/* Missing arguments,return FALSE */` |
|    ! 0 | 2794 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 2795 | `		return PH7_OK;` |
|      - | 2796 | `	}` |
|      - | 2797 | `	/* Make sure we are dealing with a valid hashmap */` |
|      3 | 2798 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|      - | 2799 | `		/* Invalid argument,return FALSE */` |
|    ! 0 | 2800 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 2801 | `		return PH7_OK;` |
|      - | 2802 | `	}` |
|      3 | 2803 | `	HashmapCurrentValue(&(*pCtx),(ph7_hashmap *)apArg[0]->x.pOther,-1);` |
|      3 | 2804 | `	return PH7_OK;` |
|      2 | 2805 |  |
|      - | 2806 | `/*` |
|      - | 2807 | ` * value end(array $input)` |
|      - | 2808 | ` *  Set the internal pointer of an array to its last element.` |
|      - | 2809 | ` * Parameter` |
|      - | 2810 | ` *  $input: The input array.` |
|      - | 2811 | ` * Return` |
|      - | 2812 | ` *  Returns the value of the last element or FALSE for empty array.` |
|      - | 2813 | ` */` |
|      2 | 2814 | `static int ph7_hashmap_end(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 2815 |  |
|      - | 2816 | `	ph7_hashmap *pMap;` |
|      3 | 2817 | `	if( nArg < 1 ){` |
|      - | 2818 | `		/* Missing arguments,return FALSE */` |
|    ! 0 | 2819 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 2820 | `		return PH7_OK;` |
|      - | 2821 | `	}` |
|      - | 2822 | `	/* Make sure we are dealing with a valid hashmap */` |
|      3 | 2823 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|      - | 2824 | `		/* Invalid argument,return FALSE */` |
|    ! 0 | 2825 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 2826 | `		return PH7_OK;` |
|      - | 2827 | `	}` |
|      - | 2828 | `	/* Point to the internal representation of the input hashmap */` |
|      3 | 2829 | `	pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|      - | 2830 | `	/* Point to the last node */` |
|      3 | 2831 | `	pMap->pCur = pMap->pLast;` |
|      - | 2832 | `	/* Return the last node value */` |
|      3 | 2833 | `	HashmapCurrentValue(&(*pCtx),pMap,0);` |
|      3 | 2834 | `	return PH7_OK;` |
|      2 | 2835 |  |
|      - | 2836 | `/*` |
|      - | 2837 | ` * value reset(array $array )` |
|      - | 2838 | ` *  Set the internal pointer of an array to its first element.` |
|      - | 2839 | ` * Parameter` |
|      - | 2840 | ` *  $input: The input array.` |
|      - | 2841 | ` * Return` |
|      - | 2842 | ` *  Returns the value of the first array element,or FALSE if the array is empty.` |
|      - | 2843 | ` */` |
|      4 | 2844 | `static int ph7_hashmap_reset(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 2845 |  |
|      - | 2846 | `	ph7_hashmap *pMap;` |
|      5 | 2847 | `	if( nArg < 1 ){` |
|      - | 2848 | `		/* Missing arguments,return FALSE */` |
|    ! 0 | 2849 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 2850 | `		return PH7_OK;` |
|      - | 2851 | `	}` |
|      - | 2852 | `	/* Make sure we are dealing with a valid hashmap */` |
|      5 | 2853 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|      - | 2854 | `		/* Invalid argument,return FALSE */` |
|    ! 0 | 2855 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 2856 | `		return PH7_OK;` |
|      - | 2857 | `	}` |
|      - | 2858 | `	/* Point to the internal representation of the input hashmap */` |
|      5 | 2859 | `	pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|      - | 2860 | `	/* Point to the first node */` |
|      5 | 2861 | `	pMap->pCur = pMap->pFirst;` |
|      - | 2862 | `	/* Return the last node value if available */` |
|      5 | 2863 | `	HashmapCurrentValue(&(*pCtx),pMap,0);` |
|      5 | 2864 | `	return PH7_OK;` |
|      3 | 2865 |  |
|      - | 2866 | `/*` |
|      - | 2867 | ` * value key(array $array)` |
|      - | 2868 | ` *   Fetch a key from an array` |
|      - | 2869 | ` * Parameter` |
|      - | 2870 | ` *  $input` |
|      - | 2871 | ` *   The input array.` |
|      - | 2872 | ` * Return` |
|      - | 2873 | ` *  The key() function simply returns the key of the array element that's currently` |
|      - | 2874 | ` *  being pointed to by the internal pointer. It does not move the pointer in any way.` |
|      - | 2875 | ` *  If the internal pointer points beyond the end of the elements list or the array` |
|      - | 2876 | ` *  is empty, key() returns NULL.` |
|      - | 2877 | ` */` |
|      4 | 2878 | `static int ph7_hashmap_simple_key(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 2879 |  |
|      - | 2880 | `	ph7_hashmap_node *pCur;` |
|      - | 2881 | `	ph7_hashmap *pMap;` |
|      5 | 2882 | `	if( nArg < 1 ){` |
|      - | 2883 | `		/* Missing arguments,return NULL */` |
|    ! 0 | 2884 | `		ph7_result_null(pCtx);` |
|    ! 0 | 2885 | `		return PH7_OK;` |
|      - | 2886 | `	}` |
|      - | 2887 | `	/* Make sure we are dealing with a valid hashmap */` |
|      5 | 2888 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|      - | 2889 | `		/* Invalid argument,return NULL */` |
|    ! 0 | 2890 | `		ph7_result_null(pCtx);` |
|    ! 0 | 2891 | `		return PH7_OK;` |
|      - | 2892 | `	}` |
|      5 | 2893 | `	pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|      5 | 2894 | `	pCur = pMap->pCur;` |
|      5 | 2895 | `	if( pCur == 0 ){` |
|      - | 2896 | `		/* Cursor does not point to anything,return NULL */` |
|    ! 0 | 2897 | `		ph7_result_null(pCtx);` |
|    ! 0 | 2898 | `		return PH7_OK;` |
|      - | 2899 | `	}` |
|      5 | 2900 | `	if( pCur->iType == HASHMAP_INT_NODE){` |
|      - | 2901 | `		/* Key is integer */` |
|    ! 0 | 2902 | `		ph7_result_int64(pCtx,pCur->xKey.iKey);` |
|    ! 0 | 2903 | `	}else{` |
|      - | 2904 | `		/* Key is blob */` |
|      7 | 2905 | `		ph7_result_string(pCtx,` |
|      4 | 2906 | `			(const char *)SyBlobData(&pCur->xKey.sKey),(int)SyBlobLength(&pCur->xKey.sKey));` |
|      - | 2907 | `	}` |
|      5 | 2908 | `	return PH7_OK;` |
|      3 | 2909 |  |
|      - | 2910 | `/*` |
|      - | 2911 | ` * array each(array $input)` |
|      - | 2912 | ` *  Return the current key and value pair from an array and advance the array cursor.` |
|      - | 2913 | ` * Parameter` |
|      - | 2914 | ` *  $input` |
|      - | 2915 | ` *    The input array.` |
|      - | 2916 | ` * Return` |
|      - | 2917 | ` *  Returns the current key and value pair from the array array. This pair is returned` |
|      - | 2918 | ` *  in a four-element array, with the keys 0, 1, key, and value. Elements 0 and key` |
|      - | 2919 | ` *  contain the key name of the array element, and 1 and value contain the data.` |
|      - | 2920 | ` *  If the internal pointer for the array points past the end of the array contents` |
|      - | 2921 | ` *  each() returns FALSE.` |
|      - | 2922 | ` */` |
|     22 | 2923 | `static int ph7_hashmap_each(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 2924 |  |
|      - | 2925 | `	ph7_hashmap_node *pCur;` |
|      - | 2926 | `	ph7_hashmap *pMap;` |
|      - | 2927 | `	ph7_value *pArray;` |
|      - | 2928 | `	ph7_value *pVal;` |
|      - | 2929 | `	ph7_value sKey;` |
|     23 | 2930 | `	if( nArg < 1 ){` |
|      - | 2931 | `		/* Missing arguments,return FALSE */` |
|    ! 0 | 2932 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 2933 | `		return PH7_OK;` |
|      - | 2934 | `	}` |
|      - | 2935 | `	/* Make sure we are dealing with a valid hashmap */` |
|     23 | 2936 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|      - | 2937 | `		/* Invalid argument,return FALSE */` |
|    ! 0 | 2938 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 2939 | `		return PH7_OK;` |
|      - | 2940 | `	}` |
|      - | 2941 | `	/* Point to the internal representation that describe the input hashmap */` |
|     23 | 2942 | `	pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|     23 | 2943 | `	if( pMap->pCur == 0 ){` |
|      - | 2944 | `		/* Cursor does not point to anything,return FALSE */` |
|      9 | 2945 | `		ph7_result_bool(pCtx,0);` |
|      9 | 2946 | `		return PH7_OK;` |
|      - | 2947 | `	}` |
|     15 | 2948 | `	pCur = pMap->pCur;` |
|      - | 2949 | `	/* Create a new array */` |
|     15 | 2950 | `	pArray = ph7_context_new_array(pCtx);` |
|     15 | 2951 | `	if( pArray == 0 ){` |
|    ! 0 | 2952 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 2953 | `		return PH7_OK;` |
|      - | 2954 | `	}` |
|     15 | 2955 | `	pVal = HashmapExtractNodeValue(pCur);` |
|      - | 2956 | `	/* Insert the current value */` |
|     15 | 2957 | `	ph7_array_add_intkey_elem(pArray,1,pVal);` |
|     15 | 2958 | `	ph7_array_add_strkey_elem(pArray,"value",pVal);` |
|      - | 2959 | `	/* Make the key */` |
|     15 | 2960 | `	if( pCur->iType == HASHMAP_INT_NODE ){` |
|      7 | 2961 | `		PH7_MemObjInitFromInt(pMap->pVm,&sKey,pCur->xKey.iKey);` |
|      4 | 2962 | `	}else{` |
|      9 | 2963 | `		PH7_MemObjInitFromString(pMap->pVm,&sKey,0);` |
|      9 | 2964 | `		PH7_MemObjStringAppend(&sKey,(const char *)SyBlobData(&pCur->xKey.sKey),SyBlobLength(&pCur->xKey.sKey));` |
|      - | 2965 | `	}` |
|      - | 2966 | `	/* Insert the current key */` |
|     15 | 2967 | `	ph7_array_add_intkey_elem(pArray,0,&sKey);` |
|     15 | 2968 | `	ph7_array_add_strkey_elem(pArray,"key",&sKey);` |
|     15 | 2969 | `	PH7_MemObjRelease(&sKey);` |
|      - | 2970 | `	/* Advance the cursor */` |
|     15 | 2971 | `	pMap->pCur = pCur->pPrev; /* Reverse link */` |
|      - | 2972 | `	/* Return the current entry */` |
|     15 | 2973 | `	ph7_result_value(pCtx,pArray);` |
|     15 | 2974 | `	return PH7_OK;` |
|     12 | 2975 |  |
|      - | 2976 | `/*` |
|      - | 2977 | ` * array range(int $start,int $limit,int $step)` |
|      - | 2978 | ` *  Create an array containing a range of elements` |
|      - | 2979 | ` * Parameter` |
|      - | 2980 | ` *  start` |
|      - | 2981 | ` *   First value of the sequence.` |
|      - | 2982 | ` *  limit` |
|      - | 2983 | ` *   The sequence is ended upon reaching the limit value.` |
|      - | 2984 | ` *  step` |
|      - | 2985 | ` *  If a step value is given, it will be used as the increment between elements in the sequence.` |
|      - | 2986 | ` *  step should be given as a positive number. If not specified, step will default to 1.` |
|      - | 2987 | ` * Return` |
|      - | 2988 | ` *  An array of elements from start to limit, inclusive.` |
|      - | 2989 | ` * NOTE:` |
|      - | 2990 | ` *  Only 32/64 bit integer key is supported.` |
|      - | 2991 | ` */` |
|      2 | 2992 | `static int ph7_hashmap_range(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 2993 |  |
|      - | 2994 | `	ph7_value *pValue,*pArray;` |
|      - | 2995 | `	sxi64 iOfft,iLimit;` |
|      3 | 2996 | `	int iStep = 1;` |
|      - | 2997 |  |
|      3 | 2998 | `	iOfft = iLimit = 0; /* cc -O6 */` |
|      3 | 2999 | `	if( nArg > 0 ){` |
|      - | 3000 | `		/* Extract the offset */` |
|      3 | 3001 | `		iOfft = ph7_value_to_int64(apArg[0]);` |
|      3 | 3002 | `		if( nArg > 1 ){` |
|      - | 3003 | `			/* Extract the limit */` |
|      3 | 3004 | `			iLimit = ph7_value_to_int64(apArg[1]);` |
|      3 | 3005 | `			if( nArg > 2 ){` |
|      - | 3006 | `				/* Extract the increment */` |
|      3 | 3007 | `				iStep = ph7_value_to_int(apArg[2]);` |
|      3 | 3008 | `				if( iStep < 1 ){` |
|      - | 3009 | `					/* Only positive number are allowed */` |
|      3 | 3010 | `					iStep = 1;` |
|      1 | 3011 | `				}` |
|      1 | 3012 | `			}` |
|      1 | 3013 | `		}` |
|      1 | 3014 | `	}` |
|      - | 3015 | `	/* Element container */` |
|      3 | 3016 | `	pValue = ph7_context_new_scalar(pCtx);` |
|      - | 3017 | `	/* Create the new array */` |
|      3 | 3018 | `	pArray = ph7_context_new_array(pCtx);` |
|      3 | 3019 | `	if( pArray == 0 ){` |
|    ! 0 | 3020 | `		ph7_result_null(pCtx);` |
|    ! 0 | 3021 | `		return PH7_OK;` |
|      - | 3022 | `	}` |
|      - | 3023 | `	/* Start filling */` |
|      3 | 3024 | `	while( iOfft <= iLimit ){` |
|    ! 0 | 3025 | `		ph7_value_int64(pValue,iOfft);` |
|      - | 3026 | `		/* Perform the insertion */` |
|    ! 0 | 3027 | `		ph7_array_add_elem(pArray,0/* Automatic index assign*/,pValue);` |
|      - | 3028 | `		/* Increment */` |
|    ! 0 | 3029 | `		iOfft += iStep;` |
|    ! 0 | 3030 | `	}` |
|      - | 3031 | `	/* Return the new array */` |
|      3 | 3032 | `	ph7_result_value(pCtx,pArray);` |
|      - | 3033 | `	/* Dont'worry about freeing 'pValue',it will be released automatically` |
|      - | 3034 | `	 * by the virtual machine as soon we return from this foreign function.` |
|      - | 3035 | `	 */` |
|      3 | 3036 | `	return PH7_OK;` |
|      2 | 3037 |  |
|      - | 3038 | `/*` |
|      - | 3039 | ` * array array_values(array $input)` |
|      - | 3040 | ` *   Returns all the values from the input array and indexes numerically the array.` |
|      - | 3041 | ` * Parameters` |
|      - | 3042 | ` *   input: The input array.` |
|      - | 3043 | ` * Return` |
|      - | 3044 | ` *  An indexed array of values or NULL on failure.` |
|      - | 3045 | ` */` |
|     18 | 3046 | `static int ph7_hashmap_values(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 3047 |  |
|      - | 3048 | `	ph7_hashmap_node *pNode;` |
|      - | 3049 | `	ph7_hashmap *pMap;` |
|      - | 3050 | `	ph7_value *pArray;` |
|      - | 3051 | `	ph7_value *pObj;` |
|      - | 3052 | `	sxu32 n;` |
|     19 | 3053 | `	if( nArg < 1 ){` |
|      - | 3054 | `		/* Missing arguments,return NULL */` |
|      3 | 3055 | `		ph7_result_null(pCtx);` |
|      3 | 3056 | `		return PH7_OK;` |
|      - | 3057 | `	}` |
|      - | 3058 | `	/* Make sure we are dealing with a valid hashmap */` |
|     17 | 3059 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|      - | 3060 | `		/* Invalid argument,return NULL */` |
|    ! 0 | 3061 | `		ph7_result_null(pCtx);` |
|    ! 0 | 3062 | `		return PH7_OK;` |
|      - | 3063 | `	}` |
|      - | 3064 | `	/* Point to the internal representation that describe the input hashmap */` |
|     17 | 3065 | `	pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|      - | 3066 | `	/* Create a new array */` |
|     17 | 3067 | `	pArray = ph7_context_new_array(pCtx);` |
|     17 | 3068 | `	if( pArray == 0 ){` |
|    ! 0 | 3069 | `		ph7_result_null(pCtx);` |
|    ! 0 | 3070 | `		return PH7_OK;` |
|      - | 3071 | `	}` |
|      - | 3072 | `	/* Perform the requested operation */` |
|     17 | 3073 | `	pNode = pMap->pFirst;` |
|     61 | 3074 | `	for( n = 0 ; n < pMap->nEntry ; ++n ){` |
|     45 | 3075 | `		pObj = HashmapExtractNodeValue(pNode);` |
|     45 | 3076 | `		if( pObj ){` |
|      - | 3077 | `			/* perform the insertion */` |
|     45 | 3078 | `			ph7_array_add_elem(pArray,0/* Automatic index assign */,pObj);` |
|     22 | 3079 | `		}` |
|      - | 3080 | `		/* Point to the next entry */` |
|     45 | 3081 | `		pNode = pNode->pPrev; /* Reverse link */` |
|     23 | 3082 | `	}` |
|      - | 3083 | `	/* return the new array */` |
|     17 | 3084 | `	ph7_result_value(pCtx,pArray);` |
|     17 | 3085 | `	return PH7_OK;` |
|     10 | 3086 |  |
|      - | 3087 | `/*` |
|      - | 3088 | ` * array array_keys(array $input [, val $search_value [, bool $strict = false ]] )` |
|      - | 3089 | ` *  Return all the keys or a subset of the keys of an array.` |
|      - | 3090 | ` * Parameters` |
|      - | 3091 | ` *  $input` |
|      - | 3092 | ` *   An array containing keys to return.` |
|      - | 3093 | ` * $search_value` |
|      - | 3094 | ` *   If specified, then only keys containing these values are returned.` |
|      - | 3095 | ` * $strict` |
|      - | 3096 | ` *   Determines if strict comparison (===) should be used during the search.` |
|      - | 3097 | ` * Return` |
|      - | 3098 | ` *  An array of all the keys in input or NULL on failure.` |
|      - | 3099 | ` */` |
|     46 | 3100 | `static int ph7_hashmap_keys(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 3101 |  |
|      - | 3102 | `	ph7_hashmap_node *pNode;` |
|      - | 3103 | `	ph7_hashmap *pMap;` |
|      - | 3104 | `	ph7_value *pArray;` |
|      - | 3105 | `	ph7_value sObj;` |
|      - | 3106 | `	ph7_value sVal;` |
|      - | 3107 | `	SyString sKey;` |
|      - | 3108 | `	int bStrict;` |
|      - | 3109 | `	sxi32 rc;` |
|      - | 3110 | `	sxu32 n;` |
|     47 | 3111 | `	if( nArg < 1 ){` |
|      - | 3112 | `		/* Missing arguments,return NULL */` |
|      3 | 3113 | `		ph7_result_null(pCtx);` |
|      3 | 3114 | `		return PH7_OK;` |
|      - | 3115 | `	}` |
|      - | 3116 | `	/* Make sure we are dealing with a valid hashmap */` |
|     45 | 3117 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|      - | 3118 | `		/* Invalid argument,return NULL */` |
|    ! 0 | 3119 | `		ph7_result_null(pCtx);` |
|    ! 0 | 3120 | `		return PH7_OK;` |
|      - | 3121 | `	}` |
|      - | 3122 | `	/* Point to the internal representation of the input hashmap */` |
|     45 | 3123 | `	pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|      - | 3124 | `	/* Create a new array */` |
|     45 | 3125 | `	pArray = ph7_context_new_array(pCtx);` |
|     45 | 3126 | `	if( pArray == 0 ){` |
|    ! 0 | 3127 | `		ph7_result_null(pCtx);` |
|    ! 0 | 3128 | `		return PH7_OK;` |
|      - | 3129 | `	}` |
|     45 | 3130 | `	bStrict = FALSE;` |
|     45 | 3131 | `	if( nArg > 2 && ph7_value_is_bool(apArg[2]) ){` |
|    ! 0 | 3132 | `		bStrict = ph7_value_to_bool(apArg[2]);` |
|    ! 0 | 3133 | `	}` |
|      - | 3134 | `	/* Perform the requested operation */` |
|     45 | 3135 | `	pNode = pMap->pFirst;` |
|     45 | 3136 | `	PH7_MemObjInit(pMap->pVm,&sVal);` |
|    347 | 3137 | `	for( n = 0 ; n < pMap->nEntry ; ++n ){` |
|    303 | 3138 | `		if( pNode->iType == HASHMAP_INT_NODE ){` |
|     41 | 3139 | `			PH7_MemObjInitFromInt(pMap->pVm,&sObj,pNode->xKey.iKey);` |
|     21 | 3140 | `		}else{` |
|    263 | 3141 | `			SyStringInitFromBuf(&sKey,SyBlobData(&pNode->xKey.sKey),SyBlobLength(&pNode->xKey.sKey));` |
|    263 | 3142 | `			PH7_MemObjInitFromString(pMap->pVm,&sObj,&sKey);` |
|      - | 3143 | `		}` |
|    303 | 3144 | `		rc = 0;` |
|    303 | 3145 | `		if( nArg > 1 ){` |
|    ! 0 | 3146 | `			ph7_value *pValue = HashmapExtractNodeValue(pNode);` |
|    ! 0 | 3147 | `			if( pValue ){` |
|    ! 0 | 3148 | `				PH7_MemObjLoad(pValue,&sVal);` |
|      - | 3149 | `				/* Filter key */` |
|    ! 0 | 3150 | `				rc = ph7_value_compare(&sVal,apArg[1],bStrict);` |
|    ! 0 | 3151 | `				PH7_MemObjRelease(pValue);` |
|    ! 0 | 3152 | `			}` |
|    ! 0 | 3153 | `		}` |
|    303 | 3154 | `		if( rc == 0 ){` |
|      - | 3155 | `			/* Perform the insertion */` |
|    303 | 3156 | `			ph7_array_add_elem(pArray,0,&sObj);` |
|    151 | 3157 | `		}` |
|    303 | 3158 | `		PH7_MemObjRelease(&sObj);` |
|      - | 3159 | `		/* Point to the next entry */` |
|    303 | 3160 | `		pNode = pNode->pPrev; /* Reverse link */` |
|    152 | 3161 | `	}` |
|      - | 3162 | `	/* return the new array */` |
|     45 | 3163 | `	ph7_result_value(pCtx,pArray);` |
|     45 | 3164 | `	return PH7_OK;` |
|     24 | 3165 |  |
|      - | 3166 | `/*` |
|      - | 3167 | ` * bool array_same(array $arr1,array $arr2)` |
|      - | 3168 | ` *  Return TRUE if the given arrays are the same instance.` |
|      - | 3169 | ` *  This function is useful under PH7 since arrays are passed` |
|      - | 3170 | ` *  by reference unlike the zend engine which use pass by values.` |
|      - | 3171 | ` * Parameters` |
|      - | 3172 | ` *  $arr1` |
|      - | 3173 | ` *   First array` |
|      - | 3174 | ` *  $arr2` |
|      - | 3175 | ` *   Second array` |
|      - | 3176 | ` * Return` |
|      - | 3177 | ` *  TRUE if the arrays are the same instance.FALSE otherwise.` |
|      - | 3178 | ` * Note` |
|      - | 3179 | ` *  This function is a symisc eXtension.` |
|      - | 3180 | ` */` |
|      4 | 3181 | `static int ph7_hashmap_same(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 3182 |  |
|      - | 3183 | `	ph7_hashmap *p1,*p2;` |
|      - | 3184 | `	int rc;` |
|      5 | 3185 | `	if( nArg < 2 \|\| !ph7_value_is_array(apArg[0]) \|\| !ph7_value_is_array(apArg[1]) ){` |
|      - | 3186 | `		/* Missing or invalid arguments,return FALSE*/` |
|    ! 0 | 3187 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 3188 | `		return PH7_OK;` |
|      - | 3189 | `	}` |
|      - | 3190 | `	/* Point to the hashmaps */` |
|      5 | 3191 | `	p1 = (ph7_hashmap *)apArg[0]->x.pOther;` |
|      5 | 3192 | `	p2 = (ph7_hashmap *)apArg[1]->x.pOther;` |
|      5 | 3193 | `	rc = (p1 == p2);` |
|      - | 3194 | `	/* Same instance? */` |
|      5 | 3195 | `	ph7_result_bool(pCtx,rc);` |
|      5 | 3196 | `	return PH7_OK;` |
|      3 | 3197 |  |
|      - | 3198 | `/*` |
|      - | 3199 | ` * array array_merge(array $array1,...)` |
|      - | 3200 | ` *  Merge one or more arrays.` |
|      - | 3201 | ` * Parameters` |
|      - | 3202 | ` *  $array1` |
|      - | 3203 | ` *    Initial array to merge.` |
|      - | 3204 | ` *  ...` |
|      - | 3205 | ` *   More array to merge.` |
|      - | 3206 | ` * Return` |
|      - | 3207 | ` *  The resulting array.` |
|      - | 3208 | ` */` |
|    750 | 3209 | `static int ph7_hashmap_merge(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      2 | 3210 |  |
|      - | 3211 | `	ph7_hashmap *pMap,*pSrc;` |
|      - | 3212 | `	ph7_value *pArray;` |
|      - | 3213 | `	int i;` |
|    752 | 3214 | `	if( nArg < 1 ){` |
|      - | 3215 | `		/* Missing arguments,return NULL */` |
|    ! 0 | 3216 | `		ph7_result_null(pCtx);` |
|    ! 0 | 3217 | `		return PH7_OK;` |
|      - | 3218 | `	}` |
|      - | 3219 | `	/* Create a new array */` |
|    752 | 3220 | `	pArray = ph7_context_new_array(pCtx);` |
|    752 | 3221 | `	if( pArray == 0 ){` |
|    ! 0 | 3222 | `		ph7_result_null(pCtx);` |
|    ! 0 | 3223 | `		return PH7_OK;` |
|      - | 3224 | `	}` |
|      - | 3225 | `	/* Point to the internal representation of the hashmap */` |
|    752 | 3226 | `	pMap = (ph7_hashmap *)pArray->x.pOther;` |
|      - | 3227 | `	/* Start merging */` |
|   2252 | 3228 | `	for( i = 0 ; i < nArg ; i++ ){` |
|      - | 3229 | `		/* Make sure we are dealing with a valid hashmap */` |
|   1502 | 3230 | `		if( !ph7_value_is_array(apArg[i]) ){` |
|      - | 3231 | `			/* Insert scalar value */` |
|      5 | 3232 | `			ph7_array_add_elem(pArray,0,apArg[i]);` |
|      3 | 3233 | `		}else{` |
|   1498 | 3234 | `			pSrc = (ph7_hashmap *)apArg[i]->x.pOther;` |
|      - | 3235 | `			/* Merge the two hashmaps */` |
|   1498 | 3236 | `			HashmapMerge(pSrc,pMap);` |
|      - | 3237 | `		}` |
|    752 | 3238 | `	}` |
|      - | 3239 | `	/* Return the freshly created array */` |
|    752 | 3240 | `	ph7_result_value(pCtx,pArray);` |
|    752 | 3241 | `	return PH7_OK;` |
|    377 | 3242 |  |
|      - | 3243 | `/*` |
|      - | 3244 | ` * array array_copy(array $source)` |
|      - | 3245 | ` *  Make a blind copy of the target array.` |
|      - | 3246 | ` * Parameters` |
|      - | 3247 | ` *  $source` |
|      - | 3248 | ` *   Target array` |
|      - | 3249 | ` * Return` |
|      - | 3250 | ` *  Copy of the target array on success.NULL otherwise.` |
|      - | 3251 | ` * Note` |
|      - | 3252 | ` *  This function is a symisc eXtension.` |
|      - | 3253 | ` */` |
|      2 | 3254 | `static int ph7_hashmap_copy(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 3255 |  |
|      - | 3256 | `	ph7_hashmap *pMap;` |
|      - | 3257 | `	ph7_value *pArray;` |
|      3 | 3258 | `	if( nArg < 1 ){` |
|      - | 3259 | `		/* Missing arguments,return NULL */` |
|    ! 0 | 3260 | `		ph7_result_null(pCtx);` |
|    ! 0 | 3261 | `		return PH7_OK;` |
|      - | 3262 | `	}` |
|      - | 3263 | `	/* Create a new array */` |
|      3 | 3264 | `	pArray = ph7_context_new_array(pCtx);` |
|      3 | 3265 | `	if( pArray == 0 ){` |
|    ! 0 | 3266 | `		ph7_result_null(pCtx);` |
|    ! 0 | 3267 | `		return PH7_OK;` |
|      - | 3268 | `	}` |
|      - | 3269 | `	/* Point to the internal representation of the hashmap */` |
|      3 | 3270 | `	pMap = (ph7_hashmap *)pArray->x.pOther;` |
|      3 | 3271 | `	if( ph7_value_is_array(apArg[0])){` |
|      - | 3272 | `		/* Point to the internal representation of the source */` |
|      3 | 3273 | `		ph7_hashmap *pSrc = (ph7_hashmap *)apArg[0]->x.pOther;` |
|      - | 3274 | `		/* Perform the copy */` |
|      3 | 3275 | `		PH7_HashmapDup(pSrc,pMap);` |
|      2 | 3276 | `	}else{` |
|      - | 3277 | `		/* Simple insertion */` |
|    ! 0 | 3278 | `		PH7_HashmapInsert(pMap,0/* Automatic index assign*/,apArg[0]);` |
|      - | 3279 | `	}` |
|      - | 3280 | `	/* Return the duplicated array */` |
|      3 | 3281 | `	ph7_result_value(pCtx,pArray);` |
|      3 | 3282 | `	return PH7_OK;` |
|      2 | 3283 |  |
|      - | 3284 | `/*` |
|      - | 3285 | ` * bool array_erase(array $source)` |
|      - | 3286 | ` *  Remove all elements from a given array.` |
|      - | 3287 | ` * Parameters` |
|      - | 3288 | ` *  $source` |
|      - | 3289 | ` *   Target array` |
|      - | 3290 | ` * Return` |
|      - | 3291 | ` *  TRUE on success.FALSE otherwise.` |
|      - | 3292 | ` * Note` |
|      - | 3293 | ` *  This function is a symisc eXtension.` |
|      - | 3294 | ` */` |
|      2 | 3295 | `static int ph7_hashmap_erase(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 3296 |  |
|      - | 3297 | `	ph7_hashmap *pMap;` |
|      3 | 3298 | `	if( nArg < 1 ){` |
|      - | 3299 | `		/* Missing arguments */` |
|    ! 0 | 3300 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 3301 | `		return PH7_OK;` |
|      - | 3302 | `	}` |
|      - | 3303 | `	/* Point to the target hashmap */` |
|      3 | 3304 | `	pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|      - | 3305 | `	/* Erase */` |
|      3 | 3306 | `	PH7_HashmapRelease(pMap,FALSE);` |
|      3 | 3307 | `	return PH7_OK;` |
|      2 | 3308 |  |
|      - | 3309 | `/*` |
|      - | 3310 | ` * array array_slice(array $array,int $offset [,int $length [, bool $preserve_keys = false ]])` |
|      - | 3311 | ` *  Extract a slice of the array.` |
|      - | 3312 | ` * Parameters` |
|      - | 3313 | ` *  $array` |
|      - | 3314 | ` *    The input array.` |
|      - | 3315 | ` * $offset` |
|      - | 3316 | ` *    If offset is non-negative, the sequence will start at that offset in the array.` |
|      - | 3317 | ` *    If offset is negative, the sequence will start that far from the end of the array.` |
|      - | 3318 | ` * $length (optional)` |
|      - | 3319 | ` *    If length is given and is positive, then the sequence will have that many elements` |
|      - | 3320 | ` *    in it. If length is given and is negative then the sequence will stop that many` |
|      - | 3321 | ` *   elements from the end of the array. If it is omitted, then the sequence will have` |
|      - | 3322 | ` *   everything from offset up until the end of the array.` |
|      - | 3323 | ` * $preserve_keys (optional)` |
|      - | 3324 | ` *    Note that array_slice() will reorder and reset the array indices by default.` |
|      - | 3325 | ` *    You can change this behaviour by setting preserve_keys to TRUE.` |
|      - | 3326 | ` * Return` |
|      - | 3327 | ` *   The new slice.` |
|      - | 3328 | ` */` |
|      8 | 3329 | `static int ph7_hashmap_slice(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 3330 |  |
|      - | 3331 | `	ph7_hashmap *pMap,*pSrc;` |
|      - | 3332 | `	ph7_hashmap_node *pCur;` |
|      - | 3333 | `	ph7_value *pArray;` |
|      - | 3334 | `	int iLength,iOfft;` |
|      - | 3335 | `	int bPreserve;` |
|      - | 3336 | `	sxi32 rc;` |
|      9 | 3337 | `	if( nArg < 2 \|\| !ph7_value_is_array(apArg[0]) ){` |
|      - | 3338 | `		/* Missing/Invalid arguments,return NULL */` |
|    ! 0 | 3339 | `		ph7_result_null(pCtx);` |
|    ! 0 | 3340 | `		return PH7_OK;` |
|      - | 3341 | `	}` |
|      - | 3342 | `	/* Point the internal representation of the target array */` |
|      9 | 3343 | `	pSrc = (ph7_hashmap *)apArg[0]->x.pOther;` |
|      9 | 3344 | `	bPreserve = FALSE;` |
|      - | 3345 | `	/* Get the offset */` |
|      9 | 3346 | `	iOfft = ph7_value_to_int(apArg[1]);` |
|      9 | 3347 | `	if( iOfft < 0 ){` |
|      3 | 3348 | `		iOfft = (int)pSrc->nEntry + iOfft;` |
|      1 | 3349 | `	}` |
|      9 | 3350 | `	if( iOfft < 0 \|\| iOfft > (int)pSrc->nEntry ){` |
|      - | 3351 | `		/* Invalid offset,return the last entry */` |
|    ! 0 | 3352 | `		iOfft = (int)pSrc->nEntry - 1;` |
|    ! 0 | 3353 | `	}` |
|      - | 3354 | `	/* Get the length */` |
|      9 | 3355 | `	iLength = (int)pSrc->nEntry - iOfft;` |
|      9 | 3356 | `	if( nArg > 2 ){` |
|      7 | 3357 | `		iLength = ph7_value_to_int(apArg[2]);` |
|      7 | 3358 | `		if( iLength < 0 ){` |
|    ! 0 | 3359 | `			iLength = ((int)pSrc->nEntry + iLength) - iOfft;` |
|    ! 0 | 3360 | `		}` |
|      7 | 3361 | `		if( iLength < 0 \|\| iOfft + iLength >= (int)pSrc->nEntry ){` |
|    ! 0 | 3362 | `			iLength = (int)pSrc->nEntry - iOfft;` |
|    ! 0 | 3363 | `		}` |
|      7 | 3364 | `		if( nArg > 3 && ph7_value_is_bool(apArg[3]) ){` |
|      3 | 3365 | `			bPreserve = ph7_value_to_bool(apArg[3]);` |
|      1 | 3366 | `		}` |
|      3 | 3367 | `	}` |
|      - | 3368 | `	/* Create a new array */` |
|      9 | 3369 | `	pArray = ph7_context_new_array(pCtx);` |
|      9 | 3370 | `	if( pArray == 0 ){` |
|    ! 0 | 3371 | `		ph7_result_null(pCtx);` |
|    ! 0 | 3372 | `		return PH7_OK;` |
|      - | 3373 | `	}` |
|      9 | 3374 | `	if( iLength < 1 ){` |
|      - | 3375 | `		/* Don't bother processing,return the empty array */` |
|    ! 0 | 3376 | `		ph7_result_value(pCtx,pArray);` |
|    ! 0 | 3377 | `		return PH7_OK;` |
|      - | 3378 | `	}` |
|      - | 3379 | `	/* Point to the desired entry */` |
|      9 | 3380 | `	pCur = pSrc->pFirst;` |
|      9 | 3381 | `	for(;;){` |
|     19 | 3382 | `		if( iOfft < 1 ){` |
|      9 | 3383 | `			break;` |
|      - | 3384 | `		}` |
|      - | 3385 | `		/* Point to the next entry */` |
|     11 | 3386 | `		pCur = pCur->pPrev; /* Reverse link */` |
|     11 | 3387 | `		iOfft--;` |
|      1 | 3388 | `	}` |
|      - | 3389 | `	/* Point to the internal representation of the hashmap */` |
|      9 | 3390 | `	pMap = (ph7_hashmap *)pArray->x.pOther;` |
|     12 | 3391 | `	for(;;){` |
|     25 | 3392 | `		if( iLength < 1 ){` |
|      9 | 3393 | `			break;` |
|      - | 3394 | `		}` |
|     17 | 3395 | `		rc = HashmapInsertNode(pMap,pCur,bPreserve);` |
|     17 | 3396 | `		if( rc != SXRET_OK ){` |
|    ! 0 | 3397 | `			break;` |
|      - | 3398 | `		}` |
|      - | 3399 | `		/* Point to the next entry */` |
|     17 | 3400 | `		pCur = pCur->pPrev; /* Reverse link */` |
|     17 | 3401 | `		iLength--;` |
|      1 | 3402 | `	}` |
|      - | 3403 | `	/* Return the freshly created array */` |
|      9 | 3404 | `	ph7_result_value(pCtx,pArray);` |
|      9 | 3405 | `	return PH7_OK;` |
|      5 | 3406 |  |
|      - | 3407 | `/*` |
|      - | 3408 | ` * array array_splice(array $array,int $offset [,int $length [,value $replacement ]])` |
|      - | 3409 | ` *  Remove a portion of the array and replace it with something else.` |
|      - | 3410 | ` * Parameters` |
|      - | 3411 | ` *  $array` |
|      - | 3412 | ` *    The input array.` |
|      - | 3413 | ` * $offset` |
|      - | 3414 | ` *    If offset is positive then the start of removed portion is at that offset from` |
|      - | 3415 | ` *    the beginning of the input array. If offset is negative then it starts that far` |
|      - | 3416 | ` *    from the end of the input array.` |
|      - | 3417 | ` * $length (optional)` |
|      - | 3418 | ` *    If length is omitted, removes everything from offset to the end of the array.` |
|      - | 3419 | ` *    If length is specified and is positive, then that many elements will be removed.` |
|      - | 3420 | ` *    If length is specified and is negative then the end of the removed portion will` |
|      - | 3421 | ` *    be that many elements from the end of the array.` |
|      - | 3422 | ` * $replacement (optional)` |
|      - | 3423 | ` *  If replacement array is specified, then the removed elements are replaced` |
|      - | 3424 | ` *  with elements from this array.` |
|      - | 3425 | ` *  If offset and length are such that nothing is removed, then the elements` |
|      - | 3426 | ` *  from the replacement array are inserted in the place specified by the offset.` |
|      - | 3427 | ` *  Note that keys in replacement array are not preserved.` |
|      - | 3428 | ` *  If replacement is just one element it is not necessary to put array() around` |
|      - | 3429 | ` *  it, unless the element is an array itself, an object or NULL.` |
|      - | 3430 | ` * Return` |
|      - | 3431 | ` *   A new array consisting of the extracted elements.` |
|      - | 3432 | ` */` |
|      2 | 3433 | `static int ph7_hashmap_splice(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 3434 |  |
|      - | 3435 | `	ph7_hashmap_node *pCur,*pPrev,*pRnode;` |
|      - | 3436 | `	ph7_value *pArray,*pRvalue,*pOld;` |
|      - | 3437 | `	ph7_hashmap *pMap,*pSrc,*pRep;` |
|      - | 3438 | `	int iLength,iOfft;` |
|      - | 3439 | `	sxi32 rc;` |
|      3 | 3440 | `	if( nArg < 2 \|\| !ph7_value_is_array(apArg[0]) ){` |
|      - | 3441 | `		/* Missing/Invalid arguments,return NULL */` |
|    ! 0 | 3442 | `		ph7_result_null(pCtx);` |
|    ! 0 | 3443 | `		return PH7_OK;` |
|      - | 3444 | `	}` |
|      - | 3445 | `	/* Point the internal representation of the target array */` |
|      3 | 3446 | `	pSrc = (ph7_hashmap *)apArg[0]->x.pOther;` |
|      - | 3447 | `	/* Get the offset */` |
|      3 | 3448 | `	iOfft = ph7_value_to_int(apArg[1]);` |
|      3 | 3449 | `	if( iOfft < 0 ){` |
|    ! 0 | 3450 | `		iOfft = (int)pSrc->nEntry + iOfft;` |
|    ! 0 | 3451 | `	}` |
|      3 | 3452 | `	if( iOfft < 0 \|\| iOfft > (int)pSrc->nEntry ){` |
|      - | 3453 | `		/* Invalid offset,remove the last entry */` |
|    ! 0 | 3454 | `		iOfft = (int)pSrc->nEntry - 1;` |
|    ! 0 | 3455 | `	}` |
|      - | 3456 | `	/* Get the length */` |
|      3 | 3457 | `	iLength = (int)pSrc->nEntry - iOfft;` |
|      3 | 3458 | `	if( nArg > 2 ){` |
|      3 | 3459 | `		iLength = ph7_value_to_int(apArg[2]);` |
|      3 | 3460 | `		if( iLength < 0 ){` |
|    ! 0 | 3461 | `			iLength = ((int)pSrc->nEntry + iLength) - iOfft;` |
|    ! 0 | 3462 | `		}` |
|      3 | 3463 | `		if( iLength < 0 \|\| iOfft + iLength >= (int)pSrc->nEntry ){` |
|    ! 0 | 3464 | `			iLength = (int)pSrc->nEntry - iOfft;` |
|    ! 0 | 3465 | `		}` |
|      1 | 3466 | `	}` |
|      - | 3467 | `	/* Create a new array */` |
|      3 | 3468 | `	pArray = ph7_context_new_array(pCtx);` |
|      3 | 3469 | `	if( pArray == 0 ){` |
|    ! 0 | 3470 | `		ph7_result_null(pCtx);` |
|    ! 0 | 3471 | `		return PH7_OK;` |
|      - | 3472 | `	}` |
|      3 | 3473 | `	if( iLength < 1 ){` |
|      - | 3474 | `		/* Don't bother processing,return the empty array */` |
|    ! 0 | 3475 | `		ph7_result_value(pCtx,pArray);` |
|    ! 0 | 3476 | `		return PH7_OK;` |
|      - | 3477 | `	}` |
|      - | 3478 | `	/* Point to the desired entry */` |
|      3 | 3479 | `	pCur = pSrc->pFirst;` |
|      2 | 3480 | `	for(;;){` |
|      5 | 3481 | `		if( iOfft < 1 ){` |
|      3 | 3482 | `			break;` |
|      - | 3483 | `		}` |
|      - | 3484 | `		/* Point to the next entry */` |
|      3 | 3485 | `		pCur = pCur->pPrev; /* Reverse link */` |
|      3 | 3486 | `		iOfft--;` |
|      1 | 3487 | `	}` |
|      3 | 3488 | `	pRep = 0;` |
|      3 | 3489 | `	if( nArg > 3 ){` |
|      3 | 3490 | `		if( !ph7_value_is_array(apArg[3]) ){` |
|      - | 3491 | `			/* Perform an array cast */` |
|    ! 0 | 3492 | `			PH7_MemObjToHashmap(apArg[3]);` |
|    ! 0 | 3493 | `			if(ph7_value_is_array(apArg[3])){` |
|    ! 0 | 3494 | `				pRep = (ph7_hashmap *)apArg[3]->x.pOther;` |
|    ! 0 | 3495 | `			}` |
|    ! 0 | 3496 | `		}else{` |
|      3 | 3497 | `			pRep = (ph7_hashmap *)apArg[3]->x.pOther;` |
|      - | 3498 | `		}` |
|      3 | 3499 | `		if( pRep ){` |
|      - | 3500 | `			/* Reset the loop cursor */` |
|      3 | 3501 | `			pRep->pCur = pRep->pFirst;` |
|      1 | 3502 | `		}` |
|      1 | 3503 | `	}` |
|      - | 3504 | `	/* Point to the internal representation of the hashmap */` |
|      3 | 3505 | `	pMap = (ph7_hashmap *)pArray->x.pOther;` |
|      3 | 3506 | `	for(;;){` |
|      7 | 3507 | `		if( iLength < 1 ){` |
|      3 | 3508 | `			break;` |
|      - | 3509 | `		}` |
|      5 | 3510 | `		pPrev = pCur->pPrev;` |
|      5 | 3511 | `		rc = HashmapInsertNode(pMap,pCur,FALSE);` |
|      5 | 3512 | `		if( pRep && (pRnode = PH7_HashmapGetNextEntry(pRep)) != 0 ){` |
|      - | 3513 | `			/* Extract node value */` |
|      5 | 3514 | `			pRvalue = HashmapExtractNodeValue(pRnode);` |
|      - | 3515 | `			/* Replace the old node */` |
|      5 | 3516 | `			pOld = HashmapExtractNodeValue(pCur);` |
|      5 | 3517 | `			if( pRvalue && pOld ){` |
|      5 | 3518 | `				PH7_MemObjStore(pRvalue,pOld);` |
|      2 | 3519 | `			}` |
|      3 | 3520 | `		}else{` |
|      - | 3521 | `			/* Unlink the node from the source hashmap */` |
|    ! 0 | 3522 | `			PH7_HashmapUnlinkNode(pCur,TRUE);` |
|      - | 3523 | `		}` |
|      5 | 3524 | `		if( rc != SXRET_OK ){` |
|    ! 0 | 3525 | `			break;` |
|      - | 3526 | `		}` |
|      - | 3527 | `		/* Point to the next entry */` |
|      5 | 3528 | `		pCur = pPrev; /* Reverse link */` |
|      5 | 3529 | `		iLength--;` |
|      1 | 3530 | `	}` |
|      3 | 3531 | `	if( pRep ){` |
|      3 | 3532 | `		while((pRnode = PH7_HashmapGetNextEntry(pRep)) != 0 ){` |
|    ! 0 | 3533 | `			HashmapInsertNode(pSrc,pRnode,FALSE);` |
|    ! 0 | 3534 | `		}` |
|      1 | 3535 | `	}` |
|      - | 3536 | `	/* Return the freshly created array */` |
|      3 | 3537 | `	ph7_result_value(pCtx,pArray);` |
|      3 | 3538 | `	return PH7_OK;` |
|      2 | 3539 |  |
|      - | 3540 | `/*` |
|      - | 3541 | ` * bool in_array(value $needle,array $haystack[,bool $strict = FALSE ])` |
|      - | 3542 | ` *  Checks if a value exists in an array.` |
|      - | 3543 | ` * Parameters` |
|      - | 3544 | ` *  $needle` |
|      - | 3545 | ` *   The searched value.` |
|      - | 3546 | ` *   Note:` |
|      - | 3547 | ` *    If needle is a string, the comparison is done in a case-sensitive manner.` |
|      - | 3548 | ` * $haystack` |
|      - | 3549 | ` *  The target array.` |
|      - | 3550 | ` * $strict` |
|      - | 3551 | ` *  If the third parameter strict is set to TRUE then the in_array() function` |
|      - | 3552 | ` *  will also check the types of the needle in the haystack.` |
|      - | 3553 | ` */` |
|  16756 | 3554 | `static int ph7_hashmap_in_array(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      2 | 3555 |  |
|      - | 3556 | `	ph7_value *pNeedle;` |
|      - | 3557 | `	int bStrict;` |
|      - | 3558 | `	int rc;` |
|  16758 | 3559 | `	if( nArg < 2 ){` |
|      - | 3560 | `		/* Missing argument,return FALSE */` |
|    ! 0 | 3561 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 3562 | `		return PH7_OK;` |
|      - | 3563 | `	}` |
|  16758 | 3564 | `	pNeedle = apArg[0];` |
|  16758 | 3565 | `	bStrict = 0;` |
|  16758 | 3566 | `	if( nArg > 2 ){` |
|      5 | 3567 | `		bStrict = ph7_value_to_bool(apArg[2]);` |
|      2 | 3568 | `	}` |
|  16758 | 3569 | `	if( !ph7_value_is_array(apArg[1]) ){` |
|      - | 3570 | `		/* haystack must be an array,perform a standard comparison */` |
|    ! 0 | 3571 | `		rc = ph7_value_compare(pNeedle,apArg[1],bStrict);` |
|      - | 3572 | `		/* Set the comparison result */` |
|    ! 0 | 3573 | `		ph7_result_bool(pCtx,rc == 0);` |
|    ! 0 | 3574 | `		return PH7_OK;` |
|      - | 3575 | `	}` |
|      - | 3576 | `	/* Perform the lookup */` |
|  16758 | 3577 | `	rc = HashmapFindValue((ph7_hashmap *)apArg[1]->x.pOther,pNeedle,0,bStrict);` |
|      - | 3578 | `	/* Lookup result */` |
|  16758 | 3579 | `	ph7_result_bool(pCtx,rc == SXRET_OK);` |
|  16758 | 3580 | `	return PH7_OK;` |
|   8380 | 3581 |  |
|      - | 3582 | `/*` |
|      - | 3583 | ` * value array_search(value $needle,array $haystack[,bool $strict = false ])` |
|      - | 3584 | ` *  Searches the array for a given value and returns the corresponding key if successful.` |
|      - | 3585 | ` * Parameters` |
|      - | 3586 | ` * $needle` |
|      - | 3587 | ` *   The searched value.` |
|      - | 3588 | ` * $haystack` |
|      - | 3589 | ` *   The array.` |
|      - | 3590 | ` * $strict` |
|      - | 3591 | ` *  If the third parameter strict is set to TRUE then the array_search() function` |
|      - | 3592 | ` *  will search for identical elements in the haystack. This means it will also check` |
|      - | 3593 | ` *  the types of the needle in the haystack, and objects must be the same instance.` |
|      - | 3594 | ` * Return` |
|      - | 3595 | ` *  Returns the key for needle if it is found in the array, FALSE otherwise.` |
|      - | 3596 | ` */` |
|     26 | 3597 | `static int ph7_hashmap_search(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 3598 |  |
|      - | 3599 | `	ph7_hashmap_node *pEntry;` |
|      - | 3600 | `	ph7_value *pVal,sNeedle;` |
|      - | 3601 | `	ph7_hashmap *pMap;` |
|      - | 3602 | `	ph7_value sVal;` |
|      - | 3603 | `	int bStrict;` |
|      - | 3604 | `	sxu32 n;` |
|      - | 3605 | `	int rc;` |
|     27 | 3606 | `	if( nArg < 2 ){` |
|      - | 3607 | `		/* Missing argument,return FALSE*/` |
|    ! 0 | 3608 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 3609 | `		return PH7_OK;` |
|      - | 3610 | `	}` |
|     27 | 3611 | `	bStrict = FALSE;` |
|     27 | 3612 | `	if( !ph7_value_is_array(apArg[1]) ){` |
|      - | 3613 | `		/* hasystack must be an array,return FALSE */` |
|      3 | 3614 | `		ph7_result_bool(pCtx,0);` |
|      3 | 3615 | `		return PH7_OK;` |
|      - | 3616 | `	}` |
|     25 | 3617 | `	if( nArg > 2 && ph7_value_is_bool(apArg[2]) ){` |
|     19 | 3618 | `		bStrict = ph7_value_to_bool(apArg[2]);` |
|      9 | 3619 | `	}` |
|      - | 3620 | `	/* Point to the internal representation of the internal hashmap */` |
|     25 | 3621 | `	pMap = (ph7_hashmap *)apArg[1]->x.pOther;` |
|      - | 3622 | `	/* Perform a linear search since we cannot sort the hashmap based on values */` |
|     25 | 3623 | `	PH7_MemObjInit(pMap->pVm,&sVal);` |
|     25 | 3624 | `	PH7_MemObjInit(pMap->pVm,&sNeedle);` |
|     25 | 3625 | `	pEntry = pMap->pFirst;` |
|     25 | 3626 | `	n = pMap->nEntry;` |
|     38 | 3627 | `	for(;;){` |
|     77 | 3628 | `		if( !n ){` |
|      7 | 3629 | `			break;` |
|      - | 3630 | `		}` |
|      - | 3631 | `		/* Extract node value */` |
|     71 | 3632 | `		pVal = HashmapExtractNodeValue(pEntry);` |
|     71 | 3633 | `		if( pVal ){` |
|      - | 3634 | `			/* Make a copy of the vuurent values since the comparison routine` |
|      - | 3635 | `			 * can change their type.` |
|      - | 3636 | `			 */` |
|     71 | 3637 | `			PH7_MemObjLoad(pVal,&sVal);` |
|     71 | 3638 | `			PH7_MemObjLoad(apArg[0],&sNeedle);` |
|     71 | 3639 | `			rc = PH7_MemObjCmp(&sNeedle,&sVal,bStrict,0);` |
|     71 | 3640 | `			PH7_MemObjRelease(&sVal);` |
|     71 | 3641 | `			PH7_MemObjRelease(&sNeedle);` |
|     71 | 3642 | `			if( rc == 0 ){` |
|      - | 3643 | `				/* Match found,return key */` |
|     19 | 3644 | `				if( pEntry->iType == HASHMAP_INT_NODE){` |
|      - | 3645 | `					/* INT key */` |
|     13 | 3646 | `					ph7_result_int64(pCtx,pEntry->xKey.iKey);` |
|      7 | 3647 | `				}else{` |
|      7 | 3648 | `					SyBlob *pKey = &pEntry->xKey.sKey;` |
|      - | 3649 | `					/* Blob key */` |
|      7 | 3650 | `					ph7_result_string(pCtx,(const char *)SyBlobData(pKey),(int)SyBlobLength(pKey));` |
|      - | 3651 | `				}` |
|     19 | 3652 | `				return PH7_OK;` |
|      - | 3653 | `			}` |
|     26 | 3654 | `		}` |
|      - | 3655 | `		/* Point to the next entry */` |
|     53 | 3656 | `		pEntry = pEntry->pPrev; /* Reverse link */` |
|     53 | 3657 | `		n--;` |
|      1 | 3658 | `	}` |
|      - | 3659 | `	/* No such value,return FALSE */` |
|      7 | 3660 | `	ph7_result_bool(pCtx,0);` |
|      7 | 3661 | `	return PH7_OK;` |
|     14 | 3662 |  |
|      - | 3663 | `/*` |
|      - | 3664 | ` * array array_diff(array $array1,array $array2,...)` |
|      - | 3665 | ` *  Computes the difference of arrays.` |
|      - | 3666 | ` * Parameters` |
|      - | 3667 | ` *  $array1` |
|      - | 3668 | ` *    The array to compare from` |
|      - | 3669 | ` *  $array2` |
|      - | 3670 | ` *    An array to compare against` |
|      - | 3671 | ` *  $...` |
|      - | 3672 | ` *   More arrays to compare against` |
|      - | 3673 | ` * Return` |
|      - | 3674 | ` *  Returns an array containing all the entries from array1 that` |
|      - | 3675 | ` *  are not present in any of the other arrays.` |
|      - | 3676 | ` */` |
|      2 | 3677 | `static int ph7_hashmap_diff(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 3678 |  |
|      - | 3679 | `	ph7_hashmap_node *pEntry;` |
|      - | 3680 | `	ph7_hashmap *pSrc,*pMap;` |
|      - | 3681 | `	ph7_value *pArray;` |
|      - | 3682 | `	ph7_value *pVal;` |
|      - | 3683 | `	sxi32 rc;` |
|      - | 3684 | `	sxu32 n;` |
|      - | 3685 | `	int i;` |
|      3 | 3686 | `	if( nArg < 1 \|\| !ph7_value_is_array(apArg[0]) ){` |
|      - | 3687 | `		/* Missing arguments,return NULL */` |
|    ! 0 | 3688 | `		ph7_result_null(pCtx);` |
|    ! 0 | 3689 | `		return PH7_OK;` |
|      - | 3690 | `	}` |
|      3 | 3691 | `	if( nArg == 1 ){` |
|      - | 3692 | `		/* Return the first array since we cannot perform a diff */` |
|    ! 0 | 3693 | `		ph7_result_value(pCtx,apArg[0]);` |
|    ! 0 | 3694 | `		return PH7_OK;` |
|      - | 3695 | `	}` |
|      - | 3696 | `	/* Create a new array */` |
|      3 | 3697 | `	pArray = ph7_context_new_array(pCtx);` |
|      3 | 3698 | `	if( pArray == 0 ){` |
|    ! 0 | 3699 | `		ph7_result_null(pCtx);` |
|    ! 0 | 3700 | `		return PH7_OK;` |
|      - | 3701 | `	}` |
|      - | 3702 | `	/* Point to the internal representation of the source hashmap */` |
|      3 | 3703 | `	pSrc = (ph7_hashmap *)apArg[0]->x.pOther;` |
|      - | 3704 | `	/* Perform the diff */` |
|      3 | 3705 | `	pEntry = pSrc->pFirst;` |
|      3 | 3706 | `	n = pSrc->nEntry;` |
|      4 | 3707 | `	for(;;){` |
|      9 | 3708 | `		if( n < 1 ){` |
|      3 | 3709 | `			break;` |
|      - | 3710 | `		}` |
|      - | 3711 | `		/* Extract the node value */` |
|      7 | 3712 | `		pVal = HashmapExtractNodeValue(pEntry);` |
|      7 | 3713 | `		if( pVal ){` |
|     11 | 3714 | `			for( i = 1 ; i < nArg ; i++ ){` |
|      7 | 3715 | `				if( !ph7_value_is_array(apArg[i])) {` |
|      - | 3716 | `					/* ignore */` |
|    ! 0 | 3717 | `					continue;` |
|      - | 3718 | `				}` |
|      - | 3719 | `				/* Point to the internal representation of the hashmap */` |
|      7 | 3720 | `				pMap = (ph7_hashmap *)apArg[i]->x.pOther;` |
|      - | 3721 | `				/* Perform the lookup */` |
|      7 | 3722 | `				rc = HashmapFindValue(pMap,pVal,0,TRUE);` |
|      7 | 3723 | `				if( rc == SXRET_OK ){` |
|      - | 3724 | `					/* Value exist */` |
|      3 | 3725 | `					break;` |
|      - | 3726 | `				}` |
|      3 | 3727 | `			}` |
|      7 | 3728 | `			if( i >= nArg ){` |
|      - | 3729 | `				/* Perform the insertion */` |
|      5 | 3730 | `				HashmapInsertNode((ph7_hashmap *)pArray->x.pOther,pEntry,TRUE);` |
|      2 | 3731 | `			}` |
|      3 | 3732 | `		}` |
|      - | 3733 | `		/* Point to the next entry */` |
|      7 | 3734 | `		pEntry = pEntry->pPrev; /* Reverse link */` |
|      7 | 3735 | `		n--;` |
|      1 | 3736 | `	}` |
|      - | 3737 | `	/* Return the freshly created array */` |
|      3 | 3738 | `	ph7_result_value(pCtx,pArray);` |
|      3 | 3739 | `	return PH7_OK;` |
|      2 | 3740 |  |
|      - | 3741 | `/*` |
|      - | 3742 | ` * array array_udiff(array $array1,array $array2,...,$callback)` |
|      - | 3743 | ` *  Computes the difference of arrays by using a callback function for data comparison.` |
|      - | 3744 | ` * Parameters` |
|      - | 3745 | ` *  $array1` |
|      - | 3746 | ` *    The array to compare from` |
|      - | 3747 | ` *  $array2` |
|      - | 3748 | ` *    An array to compare against` |
|      - | 3749 | ` *  $...` |
|      - | 3750 | ` *   More arrays to compare against.` |
|      - | 3751 | ` * $callback` |
|      - | 3752 | ` *  The callback comparison function.` |
|      - | 3753 | ` *  The comparison function must return an integer less than, equal to, or greater than zero` |
|      - | 3754 | ` *  if the first argument is considered to be respectively less than, equal to, or greater` |
|      - | 3755 | ` *  than the second.` |
|      - | 3756 | ` *     int callback ( mixed $a, mixed $b )` |
|      - | 3757 | ` * Return` |
|      - | 3758 | ` *  Returns an array containing all the entries from array1 that` |
|      - | 3759 | ` *  are not present in any of the other arrays.` |
|      - | 3760 | ` */` |
|      2 | 3761 | `static int ph7_hashmap_udiff(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 3762 |  |
|      - | 3763 | `	ph7_hashmap_node *pEntry;` |
|      - | 3764 | `	ph7_hashmap *pSrc,*pMap;` |
|      - | 3765 | `	ph7_value *pCallback;` |
|      - | 3766 | `	ph7_value *pArray;` |
|      - | 3767 | `	ph7_value *pVal;` |
|      - | 3768 | `	sxi32 rc;` |
|      - | 3769 | `	sxu32 n;` |
|      - | 3770 | `	int i;` |
|      3 | 3771 | `	if( nArg < 2 \|\| !ph7_value_is_array(apArg[0]) ){` |
|      - | 3772 | `		/* Missing/Invalid arguments,return NULL */` |
|    ! 0 | 3773 | `		ph7_result_null(pCtx);` |
|    ! 0 | 3774 | `		return PH7_OK;` |
|      - | 3775 | `	}` |
|      - | 3776 | `	/* Point to the callback */` |
|      3 | 3777 | `	pCallback = apArg[nArg - 1];` |
|      3 | 3778 | `	if( nArg == 2 ){` |
|      - | 3779 | `		/* Return the first array since we cannot perform a diff */` |
|    ! 0 | 3780 | `		ph7_result_value(pCtx,apArg[0]);` |
|    ! 0 | 3781 | `		return PH7_OK;` |
|      - | 3782 | `	}` |
|      - | 3783 | `	/* Create a new array */` |
|      3 | 3784 | `	pArray = ph7_context_new_array(pCtx);` |
|      3 | 3785 | `	if( pArray == 0 ){` |
|    ! 0 | 3786 | `		ph7_result_null(pCtx);` |
|    ! 0 | 3787 | `		return PH7_OK;` |
|      - | 3788 | `	}` |
|      - | 3789 | `	/* Point to the internal representation of the source hashmap */` |
|      3 | 3790 | `	pSrc = (ph7_hashmap *)apArg[0]->x.pOther;` |
|      - | 3791 | `	/* Perform the diff */` |
|      3 | 3792 | `	pEntry = pSrc->pFirst;` |
|      3 | 3793 | `	n = pSrc->nEntry;` |
|      4 | 3794 | `	for(;;){` |
|      9 | 3795 | `		if( n < 1 ){` |
|      3 | 3796 | `			break;` |
|      - | 3797 | `		}` |
|      - | 3798 | `		/* Extract the node value */` |
|      7 | 3799 | `		pVal = HashmapExtractNodeValue(pEntry);` |
|      7 | 3800 | `		if( pVal ){` |
|     11 | 3801 | `			for( i = 1 ; i < nArg - 1; i++ ){` |
|      7 | 3802 | `				if( !ph7_value_is_array(apArg[i])) {` |
|      - | 3803 | `					/* ignore */` |
|    ! 0 | 3804 | `					continue;` |
|      - | 3805 | `				}` |
|      - | 3806 | `				/* Point to the internal representation of the hashmap */` |
|      7 | 3807 | `				pMap = (ph7_hashmap *)apArg[i]->x.pOther;` |
|      - | 3808 | `				/* Perform the lookup */` |
|      7 | 3809 | `				rc = HashmapFindValueByCallback(pMap,pVal,pCallback,0);` |
|      7 | 3810 | `				if( rc == SXRET_OK ){` |
|      - | 3811 | `					/* Value exist */` |
|      3 | 3812 | `					break;` |
|      - | 3813 | `				}` |
|      3 | 3814 | `			}` |
|      7 | 3815 | `			if( i >= (nArg - 1)){` |
|      - | 3816 | `				/* Perform the insertion */` |
|      5 | 3817 | `				HashmapInsertNode((ph7_hashmap *)pArray->x.pOther,pEntry,TRUE);` |
|      2 | 3818 | `			}` |
|      3 | 3819 | `		}` |
|      - | 3820 | `		/* Point to the next entry */` |
|      7 | 3821 | `		pEntry = pEntry->pPrev; /* Reverse link */` |
|      7 | 3822 | `		n--;` |
|      1 | 3823 | `	}` |
|      - | 3824 | `	/* Return the freshly created array */` |
|      3 | 3825 | `	ph7_result_value(pCtx,pArray);` |
|      3 | 3826 | `	return PH7_OK;` |
|      2 | 3827 |  |
|      - | 3828 | `/*` |
|      - | 3829 | ` * array array_diff_assoc(array $array1,array $array2,...)` |
|      - | 3830 | ` *  Computes the difference of arrays with additional index check.` |
|      - | 3831 | ` * Parameters` |
|      - | 3832 | ` *  $array1` |
|      - | 3833 | ` *    The array to compare from` |
|      - | 3834 | ` *  $array2` |
|      - | 3835 | ` *    An array to compare against` |
|      - | 3836 | ` *  $...` |
|      - | 3837 | ` *   More arrays to compare against` |
|      - | 3838 | ` * Return` |
|      - | 3839 | ` *  Returns an array containing all the entries from array1 that` |
|      - | 3840 | ` *  are not present in any of the other arrays.` |
|      - | 3841 | ` */` |
|      2 | 3842 | `static int ph7_hashmap_diff_assoc(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 3843 |  |
|      - | 3844 | `	ph7_hashmap_node *pN1,*pN2,*pEntry;` |
|      - | 3845 | `	ph7_hashmap *pSrc,*pMap;` |
|      - | 3846 | `	ph7_value *pArray;` |
|      - | 3847 | `	ph7_value *pVal;` |
|      - | 3848 | `	sxi32 rc;` |
|      - | 3849 | `	sxu32 n;` |
|      - | 3850 | `	int i;` |
|      3 | 3851 | `	if( nArg < 1 \|\| !ph7_value_is_array(apArg[0]) ){` |
|      - | 3852 | `		/* Missing arguments,return NULL */` |
|    ! 0 | 3853 | `		ph7_result_null(pCtx);` |
|    ! 0 | 3854 | `		return PH7_OK;` |
|      - | 3855 | `	}` |
|      3 | 3856 | `	if( nArg == 1 ){` |
|      - | 3857 | `		/* Return the first array since we cannot perform a diff */` |
|    ! 0 | 3858 | `		ph7_result_value(pCtx,apArg[0]);` |
|    ! 0 | 3859 | `		return PH7_OK;` |
|      - | 3860 | `	}` |
|      - | 3861 | `	/* Create a new array */` |
|      3 | 3862 | `	pArray = ph7_context_new_array(pCtx);` |
|      3 | 3863 | `	if( pArray == 0 ){` |
|    ! 0 | 3864 | `		ph7_result_null(pCtx);` |
|    ! 0 | 3865 | `		return PH7_OK;` |
|      - | 3866 | `	}` |
|      - | 3867 | `	/* Point to the internal representation of the source hashmap */` |
|      3 | 3868 | `	pSrc = (ph7_hashmap *)apArg[0]->x.pOther;` |
|      - | 3869 | `	/* Perform the diff */` |
|      3 | 3870 | `	pEntry = pSrc->pFirst;` |
|      3 | 3871 | `	n = pSrc->nEntry;` |
|      3 | 3872 | `	pN1 = pN2 = 0;` |
|      3 | 3873 | `	for(;;){` |
|      7 | 3874 | `		if( n < 1 ){` |
|      3 | 3875 | `			break;` |
|      - | 3876 | `		}` |
|      7 | 3877 | `		for( i = 1 ; i < nArg ; i++ ){` |
|      5 | 3878 | `			if( !ph7_value_is_array(apArg[i])) {` |
|      - | 3879 | `				/* ignore */` |
|    ! 0 | 3880 | `				continue;` |
|      - | 3881 | `			}` |
|      - | 3882 | `			/* Point to the internal representation of the hashmap */` |
|      5 | 3883 | `			pMap = (ph7_hashmap *)apArg[i]->x.pOther;` |
|      - | 3884 | `			/* Perform a key lookup first */` |
|      5 | 3885 | `			if( pEntry->iType == HASHMAP_INT_NODE ){` |
|    ! 0 | 3886 | `				rc = HashmapLookupIntKey(pMap,pEntry->xKey.iKey,&pN1);` |
|    ! 0 | 3887 | `			}else{` |
|      5 | 3888 | `				rc = HashmapLookupBlobKey(pMap,SyBlobData(&pEntry->xKey.sKey),SyBlobLength(&pEntry->xKey.sKey),&pN1);` |
|      - | 3889 | `			}` |
|      5 | 3890 | `			if( rc != SXRET_OK ){` |
|      - | 3891 | `				/* No such key,break immediately */` |
|      3 | 3892 | `				break;` |
|      - | 3893 | `			}` |
|      - | 3894 | `			/* Extract node value */` |
|      3 | 3895 | `			pVal = HashmapExtractNodeValue(pEntry);` |
|      3 | 3896 | `			if( pVal ){` |
|      - | 3897 | `				/* Perform the lookup */` |
|      3 | 3898 | `				rc = HashmapFindValue(pMap,pVal,&pN2,TRUE);` |
|      3 | 3899 | `				if( rc != SXRET_OK \|\| pN1 != pN2 ){` |
|      - | 3900 | `					/* Value does not exist */` |
|    ! 0 | 3901 | `					break;` |
|      - | 3902 | `				}` |
|      1 | 3903 | `			}` |
|      2 | 3904 | `		}` |
|      5 | 3905 | `		if( i < nArg ){` |
|      - | 3906 | `			/* Perform the insertion */` |
|      3 | 3907 | `			HashmapInsertNode((ph7_hashmap *)pArray->x.pOther,pEntry,TRUE);` |
|      1 | 3908 | `		}` |
|      - | 3909 | `		/* Point to the next entry */` |
|      5 | 3910 | `		pEntry = pEntry->pPrev; /* Reverse link */` |
|      5 | 3911 | `		n--;` |
|      1 | 3912 | `	}` |
|      - | 3913 | `	/* Return the freshly created array */` |
|      3 | 3914 | `	ph7_result_value(pCtx,pArray);` |
|      3 | 3915 | `	return PH7_OK;` |
|      2 | 3916 |  |
|      - | 3917 | `/*` |
|      - | 3918 | ` * array array_diff_uassoc(array $array1,array $array2,...,callback $key_compare_func)` |
|      - | 3919 | ` *  Computes the difference of arrays with additional index check which is performed` |
|      - | 3920 | ` *  by a user supplied callback function.` |
|      - | 3921 | ` * Parameters` |
|      - | 3922 | ` *  $array1` |
|      - | 3923 | ` *    The array to compare from` |
|      - | 3924 | ` *  $array2` |
|      - | 3925 | ` *    An array to compare against` |
|      - | 3926 | ` *  $...` |
|      - | 3927 | ` *   More arrays to compare against.` |
|      - | 3928 | ` *  $key_compare_func` |
|      - | 3929 | ` *   Callback function to use. The callback function must return an integer` |
|      - | 3930 | ` *   less than, equal to, or greater than zero if the first argument is considered` |
|      - | 3931 | ` *   to be respectively less than, equal to, or greater than the second.` |
|      - | 3932 | ` * Return` |
|      - | 3933 | ` *  Returns an array containing all the entries from array1 that` |
|      - | 3934 | ` *  are not present in any of the other arrays.` |
|      - | 3935 | ` */` |
|      2 | 3936 | `static int ph7_hashmap_diff_uassoc(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 3937 |  |
|      - | 3938 | `	ph7_hashmap_node *pN1,*pN2,*pEntry;` |
|      - | 3939 | `	ph7_hashmap *pSrc,*pMap;` |
|      - | 3940 | `	ph7_value *pCallback;` |
|      - | 3941 | `	ph7_value *pArray;` |
|      - | 3942 | `	ph7_value *pVal;` |
|      - | 3943 | `	sxi32 rc;` |
|      - | 3944 | `	sxu32 n;` |
|      - | 3945 | `	int i;` |
|      - | 3946 |  |
|      3 | 3947 | `	if( nArg < 2 \|\| !ph7_value_is_array(apArg[0]) ){` |
|      - | 3948 | `		/* Missing/Invalid arguments,return NULL */` |
|    ! 0 | 3949 | `		ph7_result_null(pCtx);` |
|    ! 0 | 3950 | `		return PH7_OK;` |
|      - | 3951 | `	}` |
|      - | 3952 | `	/* Point to the callback */` |
|      3 | 3953 | `	pCallback = apArg[nArg - 1];` |
|      3 | 3954 | `	if( nArg == 2 ){` |
|      - | 3955 | `		/* Return the first array since we cannot perform a diff */` |
|    ! 0 | 3956 | `		ph7_result_value(pCtx,apArg[0]);` |
|    ! 0 | 3957 | `		return PH7_OK;` |
|      - | 3958 | `	}` |
|      - | 3959 | `	/* Create a new array */` |
|      3 | 3960 | `	pArray = ph7_context_new_array(pCtx);` |
|      3 | 3961 | `	if( pArray == 0 ){` |
|    ! 0 | 3962 | `		ph7_result_null(pCtx);` |
|    ! 0 | 3963 | `		return PH7_OK;` |
|      - | 3964 | `	}` |
|      - | 3965 | `	/* Point to the internal representation of the source hashmap */` |
|      3 | 3966 | `	pSrc = (ph7_hashmap *)apArg[0]->x.pOther;` |
|      - | 3967 | `	/* Perform the diff */` |
|      3 | 3968 | `	pEntry = pSrc->pFirst;` |
|      3 | 3969 | `	n = pSrc->nEntry;` |
|      3 | 3970 | `	pN1 = pN2 = 0; /* cc warning */` |
|      4 | 3971 | `	for(;;){` |
|      9 | 3972 | `		if( n < 1 ){` |
|      3 | 3973 | `			break;` |
|      - | 3974 | `		}` |
|      9 | 3975 | `		for( i = 1 ; i < nArg - 1; i++ ){` |
|      7 | 3976 | `			if( !ph7_value_is_array(apArg[i])) {` |
|      - | 3977 | `				/* ignore */` |
|    ! 0 | 3978 | `				continue;` |
|      - | 3979 | `			}` |
|      - | 3980 | `			/* Point to the internal representation of the hashmap */` |
|      7 | 3981 | `			pMap = (ph7_hashmap *)apArg[i]->x.pOther;` |
|      - | 3982 | `			/* Perform a key lookup first */` |
|      7 | 3983 | `			if( pEntry->iType == HASHMAP_INT_NODE ){` |
|    ! 0 | 3984 | `				rc = HashmapLookupIntKey(pMap,pEntry->xKey.iKey,&pN1);` |
|    ! 0 | 3985 | `			}else{` |
|      7 | 3986 | `				rc = HashmapLookupBlobKey(pMap,SyBlobData(&pEntry->xKey.sKey),SyBlobLength(&pEntry->xKey.sKey),&pN1);` |
|      - | 3987 | `			}` |
|      7 | 3988 | `			if( rc != SXRET_OK ){` |
|      - | 3989 | `				/* No such key,break immediately */` |
|      3 | 3990 | `				break;` |
|      - | 3991 | `			}` |
|      - | 3992 | `			/* Extract node value */` |
|      5 | 3993 | `			pVal = HashmapExtractNodeValue(pEntry);` |
|      5 | 3994 | `			if( pVal ){` |
|      - | 3995 | `				/* Invoke the user callback */` |
|      5 | 3996 | `				rc = HashmapFindValueByCallback(pMap,pVal,pCallback,&pN2);` |
|      5 | 3997 | `				if( rc != SXRET_OK \|\| pN1 != pN2 ){` |
|      - | 3998 | `					/* Value does not exist */` |
|      2 | 3999 | `					break;` |
|      - | 4000 | `				}` |
|      1 | 4001 | `			}` |
|      2 | 4002 | `		}` |
|      7 | 4003 | `		if( i < (nArg-1) ){` |
|      - | 4004 | `			/* Perform the insertion */` |
|      5 | 4005 | `			HashmapInsertNode((ph7_hashmap *)pArray->x.pOther,pEntry,TRUE);` |
|      2 | 4006 | `		}` |
|      - | 4007 | `		/* Point to the next entry */` |
|      7 | 4008 | `		pEntry = pEntry->pPrev; /* Reverse link */` |
|      7 | 4009 | `		n--;` |
|      1 | 4010 | `	}` |
|      - | 4011 | `	/* Return the freshly created array */` |
|      3 | 4012 | `	ph7_result_value(pCtx,pArray);` |
|      3 | 4013 | `	return PH7_OK;` |
|      2 | 4014 |  |
|      - | 4015 | `/*` |
|      - | 4016 | ` * array array_diff_key(array $array1 ,array $array2,...)` |
|      - | 4017 | ` *  Computes the difference of arrays using keys for comparison.` |
|      - | 4018 | ` * Parameters` |
|      - | 4019 | ` *  $array1` |
|      - | 4020 | ` *    The array to compare from` |
|      - | 4021 | ` *  $array2` |
|      - | 4022 | ` *    An array to compare against` |
|      - | 4023 | ` *  $...` |
|      - | 4024 | ` *   More arrays to compare against` |
|      - | 4025 | ` * Return` |
|      - | 4026 | ` *  Returns an array containing all the entries from array1 whose keys are not present` |
|      - | 4027 | ` *  in any of the other arrays.` |
|      - | 4028 | ` * Note that NULL is returned on failure.` |
|      - | 4029 | ` */` |
|      2 | 4030 | `static int ph7_hashmap_diff_key(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 4031 |  |
|      - | 4032 | `	ph7_hashmap_node *pEntry;` |
|      - | 4033 | `	ph7_hashmap *pSrc,*pMap;` |
|      - | 4034 | `	ph7_value *pArray;` |
|      - | 4035 | `	sxi32 rc;` |
|      - | 4036 | `	sxu32 n;` |
|      - | 4037 | `	int i;` |
|      3 | 4038 | `	if( nArg < 1 \|\| !ph7_value_is_array(apArg[0]) ){` |
|      - | 4039 | `		/* Missing arguments,return NULL */` |
|    ! 0 | 4040 | `		ph7_result_null(pCtx);` |
|    ! 0 | 4041 | `		return PH7_OK;` |
|      - | 4042 | `	}` |
|      3 | 4043 | `	if( nArg == 1 ){` |
|      - | 4044 | `		/* Return the first array since we cannot perform a diff */` |
|    ! 0 | 4045 | `		ph7_result_value(pCtx,apArg[0]);` |
|    ! 0 | 4046 | `		return PH7_OK;` |
|      - | 4047 | `	}` |
|      - | 4048 | `	/* Create a new array */` |
|      3 | 4049 | `	pArray = ph7_context_new_array(pCtx);` |
|      3 | 4050 | `	if( pArray == 0 ){` |
|    ! 0 | 4051 | `		ph7_result_null(pCtx);` |
|    ! 0 | 4052 | `		return PH7_OK;` |
|      - | 4053 | `	}` |
|      - | 4054 | `	/* Point to the internal representation of the main hashmap */` |
|      3 | 4055 | `	pSrc = (ph7_hashmap *)apArg[0]->x.pOther;` |
|      - | 4056 | `	/* Perfrom the diff */` |
|      3 | 4057 | `	pEntry = pSrc->pFirst;` |
|      3 | 4058 | `	n = pSrc->nEntry;` |
|      4 | 4059 | `	for(;;){` |
|      9 | 4060 | `		if( n < 1 ){` |
|      3 | 4061 | `			break;` |
|      - | 4062 | `		}` |
|      9 | 4063 | `		for( i = 1 ; i < nArg ; i++ ){` |
|      7 | 4064 | `			if( !ph7_value_is_array(apArg[i])) {` |
|      - | 4065 | `				/* ignore */` |
|    ! 0 | 4066 | `				continue;` |
|      - | 4067 | `			}` |
|      7 | 4068 | `			pMap = (ph7_hashmap *)apArg[i]->x.pOther;` |
|      7 | 4069 | `			if( pEntry->iType == HASHMAP_BLOB_NODE ){` |
|      7 | 4070 | `				SyBlob *pKey = &pEntry->xKey.sKey;` |
|      - | 4071 | `				/* Blob lookup */` |
|      7 | 4072 | `				rc = HashmapLookupBlobKey(pMap,SyBlobData(pKey),SyBlobLength(pKey),0);` |
|      4 | 4073 | `			}else{` |
|      - | 4074 | `				/* Int lookup */` |
|    ! 0 | 4075 | `				rc = HashmapLookupIntKey(pMap,pEntry->xKey.iKey,0);` |
|      - | 4076 | `			}` |
|      7 | 4077 | `			if( rc == SXRET_OK ){` |
|      - | 4078 | `				/* Key exists,break immediately */` |
|      5 | 4079 | `				break;` |
|      - | 4080 | `			}` |
|      2 | 4081 | `		}` |
|      7 | 4082 | `		if( i >= nArg ){` |
|      - | 4083 | `			/* Perform the insertion */` |
|      3 | 4084 | `			HashmapInsertNode((ph7_hashmap *)pArray->x.pOther,pEntry,TRUE);` |
|      1 | 4085 | `		}` |
|      - | 4086 | `		/* Point to the next entry */` |
|      7 | 4087 | `		pEntry = pEntry->pPrev; /* Reverse link */` |
|      7 | 4088 | `		n--;` |
|      1 | 4089 | `	}` |
|      - | 4090 | `	/* Return the freshly created array */` |
|      3 | 4091 | `	ph7_result_value(pCtx,pArray);` |
|      3 | 4092 | `	return PH7_OK;` |
|      2 | 4093 |  |
|      - | 4094 | `/*` |
|      - | 4095 | ` * array array_intersect(array $array1 ,array $array2,...)` |
|      - | 4096 | ` *  Computes the intersection of arrays.` |
|      - | 4097 | ` * Parameters` |
|      - | 4098 | ` *  $array1` |
|      - | 4099 | ` *    The array to compare from` |
|      - | 4100 | ` *  $array2` |
|      - | 4101 | ` *    An array to compare against` |
|      - | 4102 | ` *  $...` |
|      - | 4103 | ` *   More arrays to compare against` |
|      - | 4104 | ` * Return` |
|      - | 4105 | ` *  Returns an array containing all of the values in array1 whose values exist` |
|      - | 4106 | ` *  in all of the parameters. .` |
|      - | 4107 | ` * Note that NULL is returned on failure.` |
|      - | 4108 | ` */` |
|      2 | 4109 | `static int ph7_hashmap_intersect(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 4110 |  |
|      - | 4111 | `	ph7_hashmap_node *pEntry;` |
|      - | 4112 | `	ph7_hashmap *pSrc,*pMap;` |
|      - | 4113 | `	ph7_value *pArray;` |
|      - | 4114 | `	ph7_value *pVal;` |
|      - | 4115 | `	sxi32 rc;` |
|      - | 4116 | `	sxu32 n;` |
|      - | 4117 | `	int i;` |
|      3 | 4118 | `	if( nArg < 1 \|\| !ph7_value_is_array(apArg[0]) ){` |
|      - | 4119 | `		/* Missing arguments,return NULL */` |
|    ! 0 | 4120 | `		ph7_result_null(pCtx);` |
|    ! 0 | 4121 | `		return PH7_OK;` |
|      - | 4122 | `	}` |
|      3 | 4123 | `	if( nArg == 1 ){` |
|      - | 4124 | `		/* Return the first array since we cannot perform a diff */` |
|    ! 0 | 4125 | `		ph7_result_value(pCtx,apArg[0]);` |
|    ! 0 | 4126 | `		return PH7_OK;` |
|      - | 4127 | `	}` |
|      - | 4128 | `	/* Create a new array */` |
|      3 | 4129 | `	pArray = ph7_context_new_array(pCtx);` |
|      3 | 4130 | `	if( pArray == 0 ){` |
|    ! 0 | 4131 | `		ph7_result_null(pCtx);` |
|    ! 0 | 4132 | `		return PH7_OK;` |
|      - | 4133 | `	}` |
|      - | 4134 | `	/* Point to the internal representation of the source hashmap */` |
|      3 | 4135 | `	pSrc = (ph7_hashmap *)apArg[0]->x.pOther;` |
|      - | 4136 | `	/* Perform the intersection */` |
|      3 | 4137 | `	pEntry = pSrc->pFirst;` |
|      3 | 4138 | `	n = pSrc->nEntry;` |
|      5 | 4139 | `	for(;;){` |
|     11 | 4140 | `		if( n < 1 ){` |
|      3 | 4141 | `			break;` |
|      - | 4142 | `		}` |
|      - | 4143 | `		/* Extract the node value */` |
|      9 | 4144 | `		pVal = HashmapExtractNodeValue(pEntry);` |
|      9 | 4145 | `		if( pVal ){` |
|     13 | 4146 | `			for( i = 1 ; i < nArg ; i++ ){` |
|      9 | 4147 | `				if( !ph7_value_is_array(apArg[i])) {` |
|      - | 4148 | `					/* ignore */` |
|    ! 0 | 4149 | `					continue;` |
|      - | 4150 | `				}` |
|      - | 4151 | `				/* Point to the internal representation of the hashmap */` |
|      9 | 4152 | `				pMap = (ph7_hashmap *)apArg[i]->x.pOther;` |
|      - | 4153 | `				/* Perform the lookup */` |
|      9 | 4154 | `				rc = HashmapFindValue(pMap,pVal,0,TRUE);` |
|      9 | 4155 | `				if( rc != SXRET_OK ){` |
|      - | 4156 | `					/* Value does not exist */` |
|      5 | 4157 | `					break;` |
|      - | 4158 | `				}` |
|      3 | 4159 | `			}` |
|      9 | 4160 | `			if( i >= nArg ){` |
|      - | 4161 | `				/* Perform the insertion */` |
|      5 | 4162 | `				HashmapInsertNode((ph7_hashmap *)pArray->x.pOther,pEntry,TRUE);` |
|      2 | 4163 | `			}` |
|      4 | 4164 | `		}` |
|      - | 4165 | `		/* Point to the next entry */` |
|      9 | 4166 | `		pEntry = pEntry->pPrev; /* Reverse link */` |
|      9 | 4167 | `		n--;` |
|      1 | 4168 | `	}` |
|      - | 4169 | `	/* Return the freshly created array */` |
|      3 | 4170 | `	ph7_result_value(pCtx,pArray);` |
|      3 | 4171 | `	return PH7_OK;` |
|      2 | 4172 |  |
|      - | 4173 | `/*` |
|      - | 4174 | ` * array array_intersect_assoc(array $array1 ,array $array2,...)` |
|      - | 4175 | ` *  Computes the intersection of arrays.` |
|      - | 4176 | ` * Parameters` |
|      - | 4177 | ` *  $array1` |
|      - | 4178 | ` *    The array to compare from` |
|      - | 4179 | ` *  $array2` |
|      - | 4180 | ` *    An array to compare against` |
|      - | 4181 | ` *  $...` |
|      - | 4182 | ` *   More arrays to compare against` |
|      - | 4183 | ` * Return` |
|      - | 4184 | ` *  Returns an array containing all of the values in array1 whose values exist` |
|      - | 4185 | ` *  in all of the parameters. .` |
|      - | 4186 | ` * Note that NULL is returned on failure.` |
|      - | 4187 | ` */` |
|      2 | 4188 | `static int ph7_hashmap_intersect_assoc(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 4189 |  |
|      - | 4190 | `	ph7_hashmap_node *pEntry,*pN1,*pN2;` |
|      - | 4191 | `	ph7_hashmap *pSrc,*pMap;` |
|      - | 4192 | `	ph7_value *pArray;` |
|      - | 4193 | `	ph7_value *pVal;` |
|      - | 4194 | `	sxi32 rc;` |
|      - | 4195 | `	sxu32 n;` |
|      - | 4196 | `	int i;` |
|      3 | 4197 | `	if( nArg < 1 \|\| !ph7_value_is_array(apArg[0]) ){` |
|      - | 4198 | `		/* Missing arguments,return NULL */` |
|    ! 0 | 4199 | `		ph7_result_null(pCtx);` |
|    ! 0 | 4200 | `		return PH7_OK;` |
|      - | 4201 | `	}` |
|      3 | 4202 | `	if( nArg == 1 ){` |
|      - | 4203 | `		/* Return the first array since we cannot perform a diff */` |
|    ! 0 | 4204 | `		ph7_result_value(pCtx,apArg[0]);` |
|    ! 0 | 4205 | `		return PH7_OK;` |
|      - | 4206 | `	}` |
|      - | 4207 | `	/* Create a new array */` |
|      3 | 4208 | `	pArray = ph7_context_new_array(pCtx);` |
|      3 | 4209 | `	if( pArray == 0 ){` |
|    ! 0 | 4210 | `		ph7_result_null(pCtx);` |
|    ! 0 | 4211 | `		return PH7_OK;` |
|      - | 4212 | `	}` |
|      - | 4213 | `	/* Point to the internal representation of the source hashmap */` |
|      3 | 4214 | `	pSrc = (ph7_hashmap *)apArg[0]->x.pOther;` |
|      - | 4215 | `	/* Perform the intersection */` |
|      3 | 4216 | `	pEntry = pSrc->pFirst;` |
|      3 | 4217 | `	n = pSrc->nEntry;` |
|      3 | 4218 | `	pN1 = pN2 = 0; /* cc warning */` |
|      4 | 4219 | `	for(;;){` |
|      9 | 4220 | `		if( n < 1 ){` |
|      3 | 4221 | `			break;` |
|      - | 4222 | `		}` |
|      - | 4223 | `		/* Extract the node value */` |
|      7 | 4224 | `		pVal = HashmapExtractNodeValue(pEntry);` |
|      7 | 4225 | `		if( pVal ){` |
|      9 | 4226 | `			for( i = 1 ; i < nArg ; i++ ){` |
|      7 | 4227 | `				if( !ph7_value_is_array(apArg[i])) {` |
|      - | 4228 | `					/* ignore */` |
|    ! 0 | 4229 | `					continue;` |
|      - | 4230 | `				}` |
|      - | 4231 | `				/* Point to the internal representation of the hashmap */` |
|      7 | 4232 | `				pMap = (ph7_hashmap *)apArg[i]->x.pOther;` |
|      - | 4233 | `				/* Perform a key lookup first */` |
|      7 | 4234 | `				if( pEntry->iType == HASHMAP_INT_NODE ){` |
|    ! 0 | 4235 | `					rc = HashmapLookupIntKey(pMap,pEntry->xKey.iKey,&pN1);` |
|    ! 0 | 4236 | `				}else{` |
|      7 | 4237 | `					rc = HashmapLookupBlobKey(pMap,SyBlobData(&pEntry->xKey.sKey),SyBlobLength(&pEntry->xKey.sKey),&pN1);` |
|      - | 4238 | `				}` |
|      7 | 4239 | `				if( rc != SXRET_OK ){` |
|      - | 4240 | `					/* No such key,break immediately */` |
|      3 | 4241 | `					break;` |
|      - | 4242 | `				}` |
|      - | 4243 | `				/* Perform the lookup */` |
|      5 | 4244 | `				rc = HashmapFindValue(pMap,pVal,&pN2,TRUE);` |
|      5 | 4245 | `				if( rc != SXRET_OK \|\| pN1 != pN2 ){` |
|      - | 4246 | `					/* Value does not exist */` |
|      2 | 4247 | `					break;` |
|      - | 4248 | `				}` |
|      2 | 4249 | `			}` |
|      7 | 4250 | `			if( i >= nArg ){` |
|      - | 4251 | `				/* Perform the insertion */` |
|      3 | 4252 | `				HashmapInsertNode((ph7_hashmap *)pArray->x.pOther,pEntry,TRUE);` |
|      1 | 4253 | `			}` |
|      3 | 4254 | `		}` |
|      - | 4255 | `		/* Point to the next entry */` |
|      7 | 4256 | `		pEntry = pEntry->pPrev; /* Reverse link */` |
|      7 | 4257 | `		n--;` |
|      1 | 4258 | `	}` |
|      - | 4259 | `	/* Return the freshly created array */` |
|      3 | 4260 | `	ph7_result_value(pCtx,pArray);` |
|      3 | 4261 | `	return PH7_OK;` |
|      2 | 4262 |  |
|      - | 4263 | `/*` |
|      - | 4264 | ` * array array_intersect_key(array $array1 ,array $array2,...)` |
|      - | 4265 | ` *  Computes the intersection of arrays using keys for comparison.` |
|      - | 4266 | ` * Parameters` |
|      - | 4267 | ` *  $array1` |
|      - | 4268 | ` *    The array to compare from` |
|      - | 4269 | ` *  $array2` |
|      - | 4270 | ` *    An array to compare against` |
|      - | 4271 | ` *  $...` |
|      - | 4272 | ` *   More arrays to compare against` |
|      - | 4273 | ` * Return` |
|      - | 4274 | ` *  Returns an associative array containing all the entries of array1 which` |
|      - | 4275 | ` *  have keys that are present in all arguments.` |
|      - | 4276 | ` * Note that NULL is returned on failure.` |
|      - | 4277 | ` */` |
|      4 | 4278 | `static int ph7_hashmap_intersect_key(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 4279 |  |
|      - | 4280 | `	ph7_hashmap_node *pEntry;` |
|      - | 4281 | `	ph7_hashmap *pSrc,*pMap;` |
|      - | 4282 | `	ph7_value *pArray;` |
|      - | 4283 | `	sxi32 rc;` |
|      - | 4284 | `	sxu32 n;` |
|      - | 4285 | `	int i;` |
|      5 | 4286 | `	if( nArg < 1 \|\| !ph7_value_is_array(apArg[0]) ){` |
|      - | 4287 | `		/* Missing arguments,return NULL */` |
|    ! 0 | 4288 | `		ph7_result_null(pCtx);` |
|    ! 0 | 4289 | `		return PH7_OK;` |
|      - | 4290 | `	}` |
|      5 | 4291 | `	if( nArg == 1 ){` |
|      - | 4292 | `		/* Return the first array since we cannot perform a diff */` |
|    ! 0 | 4293 | `		ph7_result_value(pCtx,apArg[0]);` |
|    ! 0 | 4294 | `		return PH7_OK;` |
|      - | 4295 | `	}` |
|      - | 4296 | `	/* Create a new array */` |
|      5 | 4297 | `	pArray = ph7_context_new_array(pCtx);` |
|      5 | 4298 | `	if( pArray == 0 ){` |
|    ! 0 | 4299 | `		ph7_result_null(pCtx);` |
|    ! 0 | 4300 | `		return PH7_OK;` |
|      - | 4301 | `	}` |
|      - | 4302 | `	/* Point to the internal representation of the main hashmap */` |
|      5 | 4303 | `	pSrc = (ph7_hashmap *)apArg[0]->x.pOther;` |
|      - | 4304 | `	/* Perfrom the intersection */` |
|      5 | 4305 | `	pEntry = pSrc->pFirst;` |
|      5 | 4306 | `	n = pSrc->nEntry;` |
|      8 | 4307 | `	for(;;){` |
|     17 | 4308 | `		if( n < 1 ){` |
|      5 | 4309 | `			break;` |
|      - | 4310 | `		}` |
|     19 | 4311 | `		for( i = 1 ; i < nArg ; i++ ){` |
|     13 | 4312 | `			if( !ph7_value_is_array(apArg[i])) {` |
|      - | 4313 | `				/* ignore */` |
|    ! 0 | 4314 | `				continue;` |
|      - | 4315 | `			}` |
|     13 | 4316 | `			pMap = (ph7_hashmap *)apArg[i]->x.pOther;` |
|     13 | 4317 | `			if( pEntry->iType == HASHMAP_BLOB_NODE ){` |
|      7 | 4318 | `				SyBlob *pKey = &pEntry->xKey.sKey;` |
|      - | 4319 | `				/* Blob lookup */` |
|      7 | 4320 | `				rc = HashmapLookupBlobKey(pMap,SyBlobData(pKey),SyBlobLength(pKey),0);` |
|      4 | 4321 | `			}else{` |
|      - | 4322 | `				/* Int key */` |
|      7 | 4323 | `				rc = HashmapLookupIntKey(pMap,pEntry->xKey.iKey,0);` |
|      - | 4324 | `			}` |
|     13 | 4325 | `			if( rc != SXRET_OK ){` |
|      - | 4326 | `				/* Key does not exists,break immediately */` |
|      7 | 4327 | `				break;` |
|      - | 4328 | `			}` |
|      4 | 4329 | `		}` |
|     13 | 4330 | `		if( i >= nArg ){` |
|      - | 4331 | `			/* Perform the insertion */` |
|      7 | 4332 | `			HashmapInsertNode((ph7_hashmap *)pArray->x.pOther,pEntry,TRUE);` |
|      3 | 4333 | `		}` |
|      - | 4334 | `		/* Point to the next entry */` |
|     13 | 4335 | `		pEntry = pEntry->pPrev; /* Reverse link */` |
|     13 | 4336 | `		n--;` |
|      1 | 4337 | `	}` |
|      - | 4338 | `	/* Return the freshly created array */` |
|      5 | 4339 | `	ph7_result_value(pCtx,pArray);` |
|      5 | 4340 | `	return PH7_OK;` |
|      3 | 4341 |  |
|      - | 4342 | `/*` |
|      - | 4343 | ` * array array_uintersect(array $array1 ,array $array2,...,$callback)` |
|      - | 4344 | ` *  Computes the intersection of arrays.` |
|      - | 4345 | ` * Parameters` |
|      - | 4346 | ` *  $array1` |
|      - | 4347 | ` *    The array to compare from` |
|      - | 4348 | ` *  $array2` |
|      - | 4349 | ` *    An array to compare against` |
|      - | 4350 | ` *  $...` |
|      - | 4351 | ` *   More arrays to compare against` |
|      - | 4352 | ` * $callback` |
|      - | 4353 | ` *  The callback comparison function.` |
|      - | 4354 | ` *  The comparison function must return an integer less than, equal to, or greater than zero` |
|      - | 4355 | ` *  if the first argument is considered to be respectively less than, equal to, or greater` |
|      - | 4356 | ` *  than the second.` |
|      - | 4357 | ` *     int callback ( mixed $a, mixed $b )` |
|      - | 4358 | ` * Return` |
|      - | 4359 | ` *  Returns an array containing all of the values in array1 whose values exist` |
|      - | 4360 | ` *  in all of the parameters. .` |
|      - | 4361 | ` * Note that NULL is returned on failure.` |
|      - | 4362 | ` */` |
|      2 | 4363 | `static int ph7_hashmap_uintersect(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 4364 |  |
|      - | 4365 | `	ph7_hashmap_node *pEntry;` |
|      - | 4366 | `	ph7_hashmap *pSrc,*pMap;` |
|      - | 4367 | `	ph7_value *pCallback;` |
|      - | 4368 | `	ph7_value *pArray;` |
|      - | 4369 | `	ph7_value *pVal;` |
|      - | 4370 | `	sxi32 rc;` |
|      - | 4371 | `	sxu32 n;` |
|      - | 4372 | `	int i;` |
|      - | 4373 |  |
|      3 | 4374 | `	if( nArg < 2 \|\| !ph7_value_is_array(apArg[0]) ){` |
|      - | 4375 | `		/* Missing/Invalid arguments,return NULL */` |
|    ! 0 | 4376 | `		ph7_result_null(pCtx);` |
|    ! 0 | 4377 | `		return PH7_OK;` |
|      - | 4378 | `	}` |
|      - | 4379 | `	/* Point to the callback */` |
|      3 | 4380 | `	pCallback = apArg[nArg - 1];` |
|      3 | 4381 | `	if( nArg == 2 ){` |
|      - | 4382 | `		/* Return the first array since we cannot perform a diff */` |
|    ! 0 | 4383 | `		ph7_result_value(pCtx,apArg[0]);` |
|    ! 0 | 4384 | `		return PH7_OK;` |
|      - | 4385 | `	}` |
|      - | 4386 | `	/* Create a new array */` |
|      3 | 4387 | `	pArray = ph7_context_new_array(pCtx);` |
|      3 | 4388 | `	if( pArray == 0 ){` |
|    ! 0 | 4389 | `		ph7_result_null(pCtx);` |
|    ! 0 | 4390 | `		return PH7_OK;` |
|      - | 4391 | `	}` |
|      - | 4392 | `	/* Point to the internal representation of the source hashmap */` |
|      3 | 4393 | `	pSrc = (ph7_hashmap *)apArg[0]->x.pOther;` |
|      - | 4394 | `	/* Perform the intersection */` |
|      3 | 4395 | `	pEntry = pSrc->pFirst;` |
|      3 | 4396 | `	n = pSrc->nEntry;` |
|      4 | 4397 | `	for(;;){` |
|      9 | 4398 | `		if( n < 1 ){` |
|      3 | 4399 | `			break;` |
|      - | 4400 | `		}` |
|      - | 4401 | `		/* Extract the node value */` |
|      7 | 4402 | `		pVal = HashmapExtractNodeValue(pEntry);` |
|      7 | 4403 | `		if( pVal ){` |
|     11 | 4404 | `			for( i = 1 ; i < nArg - 1; i++ ){` |
|      7 | 4405 | `				if( !ph7_value_is_array(apArg[i])) {` |
|      - | 4406 | `					/* ignore */` |
|    ! 0 | 4407 | `					continue;` |
|      - | 4408 | `				}` |
|      - | 4409 | `				/* Point to the internal representation of the hashmap */` |
|      7 | 4410 | `				pMap = (ph7_hashmap *)apArg[i]->x.pOther;` |
|      - | 4411 | `				/* Perform the lookup */` |
|      7 | 4412 | `				rc = HashmapFindValueByCallback(pMap,pVal,pCallback,0);` |
|      7 | 4413 | `				if( rc != SXRET_OK ){` |
|      - | 4414 | `					/* Value does not exist */` |
|      3 | 4415 | `					break;` |
|      - | 4416 | `				}` |
|      3 | 4417 | `			}` |
|      7 | 4418 | `			if( i >= (nArg-1) ){` |
|      - | 4419 | `				/* Perform the insertion */` |
|      5 | 4420 | `				HashmapInsertNode((ph7_hashmap *)pArray->x.pOther,pEntry,TRUE);` |
|      2 | 4421 | `			}` |
|      3 | 4422 | `		}` |
|      - | 4423 | `		/* Point to the next entry */` |
|      7 | 4424 | `		pEntry = pEntry->pPrev; /* Reverse link */` |
|      7 | 4425 | `		n--;` |
|      1 | 4426 | `	}` |
|      - | 4427 | `	/* Return the freshly created array */` |
|      3 | 4428 | `	ph7_result_value(pCtx,pArray);` |
|      3 | 4429 | `	return PH7_OK;` |
|      2 | 4430 |  |
|      - | 4431 | `/*` |
|      - | 4432 | ` * array array_fill(int $start_index,int $num,var $value)` |
|      - | 4433 | ` *  Fill an array with values.` |
|      - | 4434 | ` * Parameters` |
|      - | 4435 | ` *  $start_index` |
|      - | 4436 | ` *    The first index of the returned array.` |
|      - | 4437 | ` *  $num` |
|      - | 4438 | ` *   Number of elements to insert.` |
|      - | 4439 | ` *  $value` |
|      - | 4440 | ` *    Value to use for filling.` |
|      - | 4441 | ` * Return` |
|      - | 4442 | ` *  The filled array or null on failure.` |
|      - | 4443 | ` */` |
|    208 | 4444 | `static int ph7_hashmap_fill(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 4445 |  |
|      - | 4446 | `	ph7_value *pArray;` |
|      - | 4447 | `	int i,nEntry;` |
|    209 | 4448 | `	if( nArg < 3 ){` |
|      - | 4449 | `		/* Missing arguments,return NULL */` |
|    ! 0 | 4450 | `		ph7_result_null(pCtx);` |
|    ! 0 | 4451 | `		return PH7_OK;` |
|      - | 4452 | `	}` |
|      - | 4453 | `	/* Create a new array */` |
|    209 | 4454 | `	pArray = ph7_context_new_array(pCtx);` |
|    209 | 4455 | `	if( pArray == 0 ){` |
|    ! 0 | 4456 | `		ph7_result_null(pCtx);` |
|    ! 0 | 4457 | `		return PH7_OK;` |
|      - | 4458 | `	}` |
|      - | 4459 | `	/* Total number of entries to insert */` |
|    209 | 4460 | `	nEntry = ph7_value_to_int(apArg[1]);` |
|      - | 4461 | `	/* Insert the first entry alone because it have it's own key */` |
|    209 | 4462 | `	ph7_array_add_intkey_elem(pArray,ph7_value_to_int(apArg[0]),apArg[2]);` |
|      - | 4463 | `	/* Repeat insertion of the desired value */` |
|  20409 | 4464 | `	for( i = 1 ; i < nEntry ; i++ ){` |
|  20201 | 4465 | `		ph7_array_add_elem(pArray,0/*Automatic index assign */,apArg[2]);` |
|  10101 | 4466 | `	}` |
|      - | 4467 | `	/* Return the filled array */` |
|    209 | 4468 | `	ph7_result_value(pCtx,pArray);` |
|    209 | 4469 | `	return PH7_OK;` |
|    105 | 4470 |  |
|      - | 4471 | `/*` |
|      - | 4472 | ` * array array_fill_keys(array $input,var $value)` |
|      - | 4473 | ` *  Fill an array with values, specifying keys.` |
|      - | 4474 | ` * Parameters` |
|      - | 4475 | ` *  $input` |
|      - | 4476 | ` *   Array of values that will be used as key.` |
|      - | 4477 | ` *  $value` |
|      - | 4478 | ` *    Value to use for filling.` |
|      - | 4479 | ` * Return` |
|      - | 4480 | ` *  The filled array or null on failure.` |
|      - | 4481 | ` */` |
|      2 | 4482 | `static int ph7_hashmap_fill_keys(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 4483 |  |
|      - | 4484 | `	ph7_hashmap_node *pEntry;` |
|      - | 4485 | `	ph7_hashmap *pSrc;` |
|      - | 4486 | `	ph7_value *pArray;` |
|      - | 4487 | `	sxu32 n;` |
|      3 | 4488 | `	if( nArg < 2 ){` |
|      - | 4489 | `		/* Missing arguments,return NULL */` |
|    ! 0 | 4490 | `		ph7_result_null(pCtx);` |
|    ! 0 | 4491 | `		return PH7_OK;` |
|      - | 4492 | `	}` |
|      - | 4493 | `	/* Make sure we are dealing with a valid hashmap */` |
|      3 | 4494 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|      - | 4495 | `		/* Invalid argument,return NULL */` |
|    ! 0 | 4496 | `		ph7_result_null(pCtx);` |
|    ! 0 | 4497 | `		return PH7_OK;` |
|      - | 4498 | `	}` |
|      - | 4499 | `	/* Point to the internal representation of the input hashmap */` |
|      3 | 4500 | `	pSrc = (ph7_hashmap *)apArg[0]->x.pOther;` |
|      - | 4501 | `	/* Create a new array */` |
|      3 | 4502 | `	pArray = ph7_context_new_array(pCtx);` |
|      3 | 4503 | `	if( pArray == 0 ){` |
|    ! 0 | 4504 | `		ph7_result_null(pCtx);` |
|    ! 0 | 4505 | `		return PH7_OK;` |
|      - | 4506 | `	}` |
|      - | 4507 | `	/* Perform the requested operation */` |
|      3 | 4508 | `	pEntry = pSrc->pFirst;` |
|      7 | 4509 | `	for( n = 0 ; n < pSrc->nEntry ; n++ ){` |
|      5 | 4510 | `		ph7_array_add_elem(pArray,HashmapExtractNodeValue(pEntry),apArg[1]);` |
|      - | 4511 | `		/* Point to the next entry */` |
|      5 | 4512 | `		pEntry = pEntry->pPrev; /* Reverse link */` |
|      3 | 4513 | `	}` |
|      - | 4514 | `	/* Return the filled array */` |
|      3 | 4515 | `	ph7_result_value(pCtx,pArray);` |
|      3 | 4516 | `	return PH7_OK;` |
|      2 | 4517 |  |
|      - | 4518 | `/*` |
|      - | 4519 | ` * array array_combine(array $keys,array $values)` |
|      - | 4520 | ` *  Creates an array by using one array for keys and another for its values.` |
|      - | 4521 | ` * Parameters` |
|      - | 4522 | ` *  $keys` |
|      - | 4523 | ` *    Array of keys to be used.` |
|      - | 4524 | ` * $values` |
|      - | 4525 | ` *   Array of values to be used.` |
|      - | 4526 | ` * Return` |
|      - | 4527 | ` *  Returns the combined array. Otherwise FALSE if the number of elements` |
|      - | 4528 | ` *  for each array isn't equal or if one of the given arguments is` |
|      - | 4529 | ` *  not an array.` |
|      - | 4530 | ` */` |
|      2 | 4531 | `static int ph7_hashmap_combine(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 4532 |  |
|      - | 4533 | `	ph7_hashmap_node *pKe,*pVe;` |
|      - | 4534 | `	ph7_hashmap *pKey,*pValue;` |
|      - | 4535 | `	ph7_value *pArray;` |
|      - | 4536 | `	sxu32 n;` |
|      3 | 4537 | `	if( nArg < 2 ){` |
|      - | 4538 | `		/* Missing arguments,return FALSE */` |
|    ! 0 | 4539 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 4540 | `		return PH7_OK;` |
|      - | 4541 | `	}` |
|      - | 4542 | `	/* Make sure we are dealing with a valid hashmap */` |
|      3 | 4543 | `	if( !ph7_value_is_array(apArg[0]) \|\| !ph7_value_is_array(apArg[1]) ){` |
|      - | 4544 | `		/* Invalid argument,return FALSE */` |
|    ! 0 | 4545 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 4546 | `		return PH7_OK;` |
|      - | 4547 | `	}` |
|      - | 4548 | `	/* Point to the internal representation of the input hashmaps */` |
|      3 | 4549 | `	pKey   = (ph7_hashmap *)apArg[0]->x.pOther;` |
|      3 | 4550 | `	pValue = (ph7_hashmap *)apArg[1]->x.pOther;` |
|      3 | 4551 | `	if( pKey->nEntry != pValue->nEntry ){` |
|      - | 4552 | `		/* Array length differs,return FALSE */` |
|    ! 0 | 4553 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 4554 | `		return PH7_OK;` |
|      - | 4555 | `	}` |
|      - | 4556 | `	/* Create a new array */` |
|      3 | 4557 | `	pArray = ph7_context_new_array(pCtx);` |
|      3 | 4558 | `	if( pArray == 0 ){` |
|    ! 0 | 4559 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 4560 | `		return PH7_OK;` |
|      - | 4561 | `	}` |
|      - | 4562 | `	/* Perform the requested operation */` |
|      3 | 4563 | `	pKe = pKey->pFirst;` |
|      3 | 4564 | `	pVe = pValue->pFirst;` |
|      9 | 4565 | `	for( n = 0 ; n < pKey->nEntry ; n++ ){` |
|      7 | 4566 | `		ph7_array_add_elem(pArray,HashmapExtractNodeValue(pKe),HashmapExtractNodeValue(pVe));` |
|      - | 4567 | `		/* Point to the next entry */` |
|      7 | 4568 | `		pKe = pKe->pPrev; /* Reverse link */` |
|      7 | 4569 | `		pVe = pVe->pPrev;` |
|      4 | 4570 | `	}` |
|      - | 4571 | `	/* Return the filled array */` |
|      3 | 4572 | `	ph7_result_value(pCtx,pArray);` |
|      3 | 4573 | `	return PH7_OK;` |
|      2 | 4574 |  |
|      - | 4575 | `/*` |
|      - | 4576 | ` * array array_reverse(array $array [,bool $preserve_keys = false ])` |
|      - | 4577 | ` *  Return an array with elements in reverse order.` |
|      - | 4578 | ` * Parameters` |
|      - | 4579 | ` *  $array` |
|      - | 4580 | ` *   The input array.` |
|      - | 4581 | ` *  $preserve_keys (optional)` |
|      - | 4582 | ` *   If set to TRUE keys are preserved.` |
|      - | 4583 | ` * Return` |
|      - | 4584 | ` *  The reversed array.` |
|      - | 4585 | ` */` |
|      6 | 4586 | `static int ph7_hashmap_reverse(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 4587 |  |
|      - | 4588 | `	ph7_hashmap_node *pEntry;` |
|      - | 4589 | `	ph7_hashmap *pSrc;` |
|      - | 4590 | `	ph7_value *pArray;` |
|      - | 4591 | `	int bPreserve;` |
|      - | 4592 | `	sxu32 n;` |
|      7 | 4593 | `	if( nArg < 1 ){` |
|      - | 4594 | `		/* Missing arguments,return NULL */` |
|    ! 0 | 4595 | `		ph7_result_null(pCtx);` |
|    ! 0 | 4596 | `		return PH7_OK;` |
|      - | 4597 | `	}` |
|      - | 4598 | `	/* Make sure we are dealing with a valid hashmap */` |
|      7 | 4599 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|      - | 4600 | `		/* Invalid argument,return NULL */` |
|    ! 0 | 4601 | `		ph7_result_null(pCtx);` |
|    ! 0 | 4602 | `		return PH7_OK;` |
|      - | 4603 | `	}` |
|      7 | 4604 | `	bPreserve = FALSE;` |
|      7 | 4605 | `	if( nArg > 1 && ph7_value_is_bool(apArg[1]) ){` |
|      3 | 4606 | `		bPreserve = ph7_value_to_bool(apArg[1]);` |
|      1 | 4607 | `	}` |
|      - | 4608 | `	/* Point to the internal representation of the input hashmap */` |
|      7 | 4609 | `	pSrc = (ph7_hashmap *)apArg[0]->x.pOther;` |
|      - | 4610 | `	/* Create a new array */` |
|      7 | 4611 | `	pArray = ph7_context_new_array(pCtx);` |
|      7 | 4612 | `	if( pArray == 0 ){` |
|    ! 0 | 4613 | `		ph7_result_null(pCtx);` |
|    ! 0 | 4614 | `		return PH7_OK;` |
|      - | 4615 | `	}` |
|      - | 4616 | `	/* Perform the requested operation */` |
|      7 | 4617 | `	pEntry = pSrc->pLast;` |
|     23 | 4618 | `	for( n = 0 ; n < pSrc->nEntry ; n++ ){` |
|     17 | 4619 | `		HashmapInsertNode((ph7_hashmap *)pArray->x.pOther,pEntry,bPreserve);` |
|      - | 4620 | `		/* Point to the previous entry */` |
|     17 | 4621 | `		pEntry = pEntry->pNext; /* Reverse link */` |
|      9 | 4622 | `	}` |
|      7 | 4623 | `	ph7_result_value(pCtx,pArray);` |
|      7 | 4624 | `	return PH7_OK;` |
|      4 | 4625 |  |
|      - | 4626 | `/*` |
|      - | 4627 | ` * array array_unique(array $array[,int $sort_flags = SORT_STRING ])` |
|      - | 4628 | ` *  Removes duplicate values from an array` |
|      - | 4629 | ` * Parameter` |
|      - | 4630 | ` *  $array` |
|      - | 4631 | ` *   The input array.` |
|      - | 4632 | ` *  $sort_flags` |
|      - | 4633 | ` *    The optional second parameter sort_flags may be used to modify the sorting behavior using these values:` |
|      - | 4634 | ` *    Sorting type flags:` |
|      - | 4635 | ` *       SORT_REGULAR - compare items normally (don't change types)` |
|      - | 4636 | ` *       SORT_NUMERIC - compare items numerically` |
|      - | 4637 | ` *       SORT_STRING - compare items as strings` |
|      - | 4638 | ` *       SORT_LOCALE_STRING - compare items as` |
|      - | 4639 | ` * Return` |
|      - | 4640 | ` *  Filtered array or NULL on failure.` |
|      - | 4641 | ` */` |
|      2 | 4642 | `static int ph7_hashmap_unique(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 4643 |  |
|      - | 4644 | `	ph7_hashmap_node *pEntry;` |
|      - | 4645 | `	ph7_value *pNeedle;` |
|      - | 4646 | `	ph7_hashmap *pSrc;` |
|      - | 4647 | `	ph7_value *pArray;` |
|      - | 4648 | `	int bStrict;` |
|      - | 4649 | `	sxi32 rc;` |
|      - | 4650 | `	sxu32 n;` |
|      3 | 4651 | `	if( nArg < 1 ){` |
|      - | 4652 | `		/* Missing arguments,return NULL */` |
|    ! 0 | 4653 | `		ph7_result_null(pCtx);` |
|    ! 0 | 4654 | `		return PH7_OK;` |
|      - | 4655 | `	}` |
|      - | 4656 | `	/* Make sure we are dealing with a valid hashmap */` |
|      3 | 4657 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|      - | 4658 | `		/* Invalid argument,return NULL */` |
|    ! 0 | 4659 | `		ph7_result_null(pCtx);` |
|    ! 0 | 4660 | `		return PH7_OK;` |
|      - | 4661 | `	}` |
|      3 | 4662 | `	bStrict = FALSE;` |
|      3 | 4663 | `	if( nArg > 1 ){` |
|    ! 0 | 4664 | `		bStrict = ph7_value_to_int(apArg[1]) == 3 /* SORT_REGULAR */ ? 1 : 0;` |
|    ! 0 | 4665 | `	}` |
|      - | 4666 | `	/* Point to the internal representation of the input hashmap */` |
|      3 | 4667 | `	pSrc = (ph7_hashmap *)apArg[0]->x.pOther;` |
|      - | 4668 | `	/* Create a new array */` |
|      3 | 4669 | `	pArray = ph7_context_new_array(pCtx);` |
|      3 | 4670 | `	if( pArray == 0 ){` |
|    ! 0 | 4671 | `		ph7_result_null(pCtx);` |
|    ! 0 | 4672 | `		return PH7_OK;` |
|      - | 4673 | `	}` |
|      - | 4674 | `	/* Perform the requested operation */` |
|      3 | 4675 | `	pEntry = pSrc->pFirst;` |
|     13 | 4676 | `	for( n = 0 ; n < pSrc->nEntry ; n++ ){` |
|     11 | 4677 | `		pNeedle = HashmapExtractNodeValue(pEntry);` |
|     11 | 4678 | `		rc = SXERR_NOTFOUND;` |
|     11 | 4679 | `		if( pNeedle ){` |
|     11 | 4680 | `			rc = HashmapFindValue((ph7_hashmap *)pArray->x.pOther,pNeedle,0,bStrict);` |
|      5 | 4681 | `		}` |
|     11 | 4682 | `		if( rc != SXRET_OK ){` |
|      - | 4683 | `			/* Perform the insertion */` |
|      7 | 4684 | `			HashmapInsertNode((ph7_hashmap *)pArray->x.pOther,pEntry,TRUE);` |
|      3 | 4685 | `		}` |
|      - | 4686 | `		/* Point to the next entry */` |
|     11 | 4687 | `		pEntry = pEntry->pPrev; /* Reverse link */` |
|      6 | 4688 | `	}` |
|      - | 4689 | `	/* Return the freshly created array */` |
|      3 | 4690 | `	ph7_result_value(pCtx,pArray);` |
|      3 | 4691 | `	return PH7_OK;` |
|      2 | 4692 |  |
|      - | 4693 | `/*` |
|      - | 4694 | ` * array array_flip(array $input)` |
|      - | 4695 | ` *  Exchanges all keys with their associated values in an array.` |
|      - | 4696 | ` * Parameter` |
|      - | 4697 | ` *  $input` |
|      - | 4698 | ` *   Input array.` |
|      - | 4699 | ` * Return` |
|      - | 4700 | ` *   The flipped array on success or NULL on failure.` |
|      - | 4701 | ` */` |
|     28 | 4702 | `static int ph7_hashmap_flip(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 4703 |  |
|      - | 4704 | `	ph7_hashmap_node *pEntry;` |
|      - | 4705 | `	ph7_hashmap *pSrc;` |
|      - | 4706 | `	ph7_value *pArray;` |
|      - | 4707 | `	ph7_value *pKey;` |
|      - | 4708 | `	ph7_value sVal;` |
|      - | 4709 | `	sxu32 n;` |
|     29 | 4710 | `	if( nArg < 1 ){` |
|      - | 4711 | `		/* Missing arguments,return NULL */` |
|    ! 0 | 4712 | `		ph7_result_null(pCtx);` |
|    ! 0 | 4713 | `		return PH7_OK;` |
|      - | 4714 | `	}` |
|      - | 4715 | `	/* Make sure we are dealing with a valid hashmap */` |
|     29 | 4716 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|      - | 4717 | `		/* Invalid argument,return NULL */` |
|      5 | 4718 | `		ph7_result_null(pCtx);` |
|      5 | 4719 | `		return PH7_OK;` |
|      - | 4720 | `	}` |
|      - | 4721 | `	/* Point to the internal representation of the input hashmap */` |
|     25 | 4722 | `	pSrc = (ph7_hashmap *)apArg[0]->x.pOther;` |
|      - | 4723 | `	/* Create a new array */` |
|     25 | 4724 | `	pArray = ph7_context_new_array(pCtx);` |
|     25 | 4725 | `	if( pArray == 0 ){` |
|    ! 0 | 4726 | `		ph7_result_null(pCtx);` |
|    ! 0 | 4727 | `		return PH7_OK;` |
|      - | 4728 | `	}` |
|      - | 4729 | `	/* Start processing */` |
|     25 | 4730 | `	pEntry = pSrc->pFirst;` |
|  22259 | 4731 | `	for( n = 0 ; n < pSrc->nEntry ; n++ ){` |
|      - | 4732 | `		/* Extract the node value */` |
|  22235 | 4733 | `		pKey = HashmapExtractNodeValue(pEntry);` |
|  22235 | 4734 | `		if( pKey && (pKey->iFlags & MEMOBJ_NULL) == 0){` |
|      - | 4735 | `			/* Prepare the value for insertion */` |
|  22233 | 4736 | `			if( pEntry->iType == HASHMAP_INT_NODE ){` |
|  20001 | 4737 | `				PH7_MemObjInitFromInt(pSrc->pVm,&sVal,pEntry->xKey.iKey);` |
|  10001 | 4738 | `			}else{` |
|      - | 4739 | `				SyString sStr;` |
|   2233 | 4740 | `				SyStringInitFromBuf(&sStr,SyBlobData(&pEntry->xKey.sKey),SyBlobLength(&pEntry->xKey.sKey));` |
|   2233 | 4741 | `				PH7_MemObjInitFromString(pSrc->pVm,&sVal,&sStr);` |
|      - | 4742 | `			}` |
|      - | 4743 | `			/* Perform the insertion */` |
|  22233 | 4744 | `			ph7_array_add_elem(pArray,pKey,&sVal);` |
|      - | 4745 | `			/* Safely release the value because each inserted entry` |
|      - | 4746 | `			 * have it's own private copy of the value.` |
|      - | 4747 | `			 */` |
|  22233 | 4748 | `			PH7_MemObjRelease(&sVal);` |
|  11116 | 4749 | `		}` |
|      - | 4750 | `		/* Point to the next entry */` |
|  22235 | 4751 | `		pEntry = pEntry->pPrev; /* Reverse link */` |
|  11118 | 4752 | `	}` |
|      - | 4753 | `	/* Return the freshly created array */` |
|     25 | 4754 | `	ph7_result_value(pCtx,pArray);` |
|     25 | 4755 | `	return PH7_OK;` |
|     15 | 4756 |  |
|      - | 4757 | `/*` |
|      - | 4758 | ` * number array_sum(array $array )` |
|      - | 4759 | ` *  Calculate the sum of values in an array.` |
|      - | 4760 | ` * Parameters` |
|      - | 4761 | ` *  $array: The input array.` |
|      - | 4762 | ` * Return` |
|      - | 4763 | ` *  Returns the sum of values as an integer or float.` |
|      - | 4764 | ` */` |
|      4 | 4765 | `static void DoubleSum(ph7_context *pCtx,ph7_hashmap *pMap)` |
|      1 | 4766 |  |
|      - | 4767 | `	ph7_hashmap_node *pEntry;` |
|      - | 4768 | `	ph7_value *pObj;` |
|      5 | 4769 | `	double dSum = 0;` |
|      - | 4770 | `	sxu32 n;` |
|      5 | 4771 | `	pEntry = pMap->pFirst;` |
|     19 | 4772 | `	for( n = 0 ; n < pMap->nEntry ; n++ ){` |
|     15 | 4773 | `		pObj = HashmapExtractNodeValue(pEntry);` |
|     15 | 4774 | `		if( pObj && (pObj->iFlags & (MEMOBJ_NULL\|MEMOBJ_HASHMAP\|MEMOBJ_OBJ\|MEMOBJ_RES)) == 0){` |
|     15 | 4775 | `			if( pObj->iFlags & MEMOBJ_REAL ){` |
|     15 | 4776 | `				dSum += pObj->rVal;` |
|      7 | 4777 | `			}else if( pObj->iFlags & (MEMOBJ_INT\|MEMOBJ_BOOL) ){` |
|    ! 0 | 4778 | `				dSum += (double)pObj->x.iVal;` |
|    ! 0 | 4779 | `			}else if( pObj->iFlags & MEMOBJ_STRING ){` |
|    ! 0 | 4780 | `				if( SyBlobLength(&pObj->sBlob) > 0 ){` |
|    ! 0 | 4781 | `					double dv = 0;` |
|    ! 0 | 4782 | `					SyStrToReal((const char *)SyBlobData(&pObj->sBlob),SyBlobLength(&pObj->sBlob),(void *)&dv,0);` |
|    ! 0 | 4783 | `					dSum += dv;` |
|    ! 0 | 4784 | `				}` |
|    ! 0 | 4785 | `			}` |
|      7 | 4786 | `		}` |
|      - | 4787 | `		/* Point to the next entry */` |
|     15 | 4788 | `		pEntry = pEntry->pPrev; /* Reverse link */` |
|      8 | 4789 | `	}` |
|      - | 4790 | `	/* Return sum */` |
|      5 | 4791 | `	ph7_result_double(pCtx,dSum);` |
|      5 | 4792 |  |
|      6 | 4793 | `static void Int64Sum(ph7_context *pCtx,ph7_hashmap *pMap)` |
|      2 | 4794 |  |
|      - | 4795 | `	ph7_hashmap_node *pEntry;` |
|      - | 4796 | `	ph7_value *pObj;` |
|      8 | 4797 | `	sxi64 nSum = 0;` |
|      - | 4798 | `	sxu32 n;` |
|      8 | 4799 | `	pEntry = pMap->pFirst;` |
|     34 | 4800 | `	for( n = 0 ; n < pMap->nEntry ; n++ ){` |
|     28 | 4801 | `		pObj = HashmapExtractNodeValue(pEntry);` |
|     28 | 4802 | `		if( pObj && (pObj->iFlags & (MEMOBJ_NULL\|MEMOBJ_HASHMAP\|MEMOBJ_OBJ\|MEMOBJ_RES)) == 0){` |
|     28 | 4803 | `			if( pObj->iFlags & MEMOBJ_REAL ){` |
|    ! 0 | 4804 | `				nSum += (sxi64)pObj->rVal;` |
|     28 | 4805 | `			}else if( pObj->iFlags & (MEMOBJ_INT\|MEMOBJ_BOOL) ){` |
|     28 | 4806 | `				nSum += pObj->x.iVal;` |
|     13 | 4807 | `			}else if( pObj->iFlags & MEMOBJ_STRING ){` |
|    ! 0 | 4808 | `				if( SyBlobLength(&pObj->sBlob) > 0 ){` |
|    ! 0 | 4809 | `					sxi64 nv = 0;` |
|    ! 0 | 4810 | `					SyStrToInt64((const char *)SyBlobData(&pObj->sBlob),SyBlobLength(&pObj->sBlob),(void *)&nv,0);` |
|    ! 0 | 4811 | `					nSum += nv;` |
|    ! 0 | 4812 | `				}` |
|    ! 0 | 4813 | `			}` |
|     13 | 4814 | `		}` |
|      - | 4815 | `		/* Point to the next entry */` |
|     28 | 4816 | `		pEntry = pEntry->pPrev; /* Reverse link */` |
|     15 | 4817 | `	}` |
|      - | 4818 | `	/* Return sum */` |
|      8 | 4819 | `	ph7_result_int64(pCtx,nSum);` |
|      8 | 4820 |  |
|      - | 4821 | `/* number array_sum(array $array )` |
|      - | 4822 | ` * (See block-coment above)` |
|      - | 4823 | ` */` |
|     16 | 4824 | `static int ph7_hashmap_sum(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      2 | 4825 |  |
|      - | 4826 | `	ph7_hashmap *pMap;` |
|      - | 4827 | `	ph7_value *pObj;` |
|     18 | 4828 | `	if( nArg < 1 ){` |
|      - | 4829 | `		/* Missing arguments,return 0 */` |
|      3 | 4830 | `		ph7_result_int(pCtx,0);` |
|      3 | 4831 | `		return PH7_OK;` |
|      - | 4832 | `	}` |
|      - | 4833 | `	/* Make sure we are dealing with a valid hashmap */` |
|     16 | 4834 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|      - | 4835 | `		/* Invalid argument,return 0 */` |
|      5 | 4836 | `		ph7_result_int(pCtx,0);` |
|      5 | 4837 | `		return PH7_OK;` |
|      - | 4838 | `	}` |
|     12 | 4839 | `	pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|     12 | 4840 | `	if( pMap->nEntry < 1 ){` |
|      - | 4841 | `		/* Nothing to compute,return 0 */` |
|    ! 0 | 4842 | `		ph7_result_int(pCtx,0);` |
|    ! 0 | 4843 | `		return PH7_OK;` |
|      - | 4844 | `	}` |
|      - | 4845 | `	/* If the first element is of type float,then perform floating` |
|      - | 4846 | `	 * point computaion.Otherwise switch to int64 computaion.` |
|      - | 4847 | `	 */` |
|     12 | 4848 | `	pObj = HashmapExtractNodeValue(pMap->pFirst);` |
|     12 | 4849 | `	if( pObj == 0 ){` |
|    ! 0 | 4850 | `		ph7_result_int(pCtx,0);` |
|    ! 0 | 4851 | `		return PH7_OK;` |
|      - | 4852 | `	}` |
|     12 | 4853 | `	if( pObj->iFlags & MEMOBJ_REAL ){` |
|      5 | 4854 | `		DoubleSum(pCtx,pMap);` |
|      3 | 4855 | `	}else{` |
|      8 | 4856 | `		Int64Sum(pCtx,pMap);` |
|      - | 4857 | `	}` |
|     12 | 4858 | `	return PH7_OK;` |
|     10 | 4859 |  |
|      - | 4860 | `/*` |
|      - | 4861 | ` * number array_product(array $array )` |
|      - | 4862 | ` *  Calculate the product of values in an array.` |
|      - | 4863 | ` * Parameters` |
|      - | 4864 | ` *  $array: The input array.` |
|      - | 4865 | ` * Return` |
|      - | 4866 | ` *  Returns the product of values as an integer or float.` |
|      - | 4867 | ` */` |
|    ! 0 | 4868 | `static void DoubleProd(ph7_context *pCtx,ph7_hashmap *pMap)` |
|    ! 0 | 4869 |  |
|      - | 4870 | `	ph7_hashmap_node *pEntry;` |
|      - | 4871 | `	ph7_value *pObj;` |
|      - | 4872 | `	double dProd;` |
|      - | 4873 | `	sxu32 n;` |
|    ! 0 | 4874 | `	pEntry = pMap->pFirst;` |
|    ! 0 | 4875 | `	dProd = 1;` |
|    ! 0 | 4876 | `	for( n = 0 ; n < pMap->nEntry ; n++ ){` |
|    ! 0 | 4877 | `		pObj = HashmapExtractNodeValue(pEntry);` |
|    ! 0 | 4878 | `		if( pObj && (pObj->iFlags & (MEMOBJ_NULL\|MEMOBJ_HASHMAP\|MEMOBJ_OBJ\|MEMOBJ_RES)) == 0){` |
|    ! 0 | 4879 | `			if( pObj->iFlags & MEMOBJ_REAL ){` |
|    ! 0 | 4880 | `				dProd *= pObj->rVal;` |
|    ! 0 | 4881 | `			}else if( pObj->iFlags & (MEMOBJ_INT\|MEMOBJ_BOOL) ){` |
|    ! 0 | 4882 | `				dProd *= (double)pObj->x.iVal;` |
|    ! 0 | 4883 | `			}else if( pObj->iFlags & MEMOBJ_STRING ){` |
|    ! 0 | 4884 | `				if( SyBlobLength(&pObj->sBlob) > 0 ){` |
|    ! 0 | 4885 | `					double dv = 0;` |
|    ! 0 | 4886 | `					SyStrToReal((const char *)SyBlobData(&pObj->sBlob),SyBlobLength(&pObj->sBlob),(void *)&dv,0);` |
|    ! 0 | 4887 | `					dProd *= dv;` |
|    ! 0 | 4888 | `				}` |
|    ! 0 | 4889 | `			}` |
|    ! 0 | 4890 | `		}` |
|      - | 4891 | `		/* Point to the next entry */` |
|    ! 0 | 4892 | `		pEntry = pEntry->pPrev; /* Reverse link */` |
|    ! 0 | 4893 | `	}` |
|      - | 4894 | `	/* Return product */` |
|    ! 0 | 4895 | `	ph7_result_double(pCtx,dProd);` |
|    ! 0 | 4896 |  |
|      2 | 4897 | `static void Int64Prod(ph7_context *pCtx,ph7_hashmap *pMap)` |
|      1 | 4898 |  |
|      - | 4899 | `	ph7_hashmap_node *pEntry;` |
|      - | 4900 | `	ph7_value *pObj;` |
|      - | 4901 | `	sxi64 nProd;` |
|      - | 4902 | `	sxu32 n;` |
|      3 | 4903 | `	pEntry = pMap->pFirst;` |
|      3 | 4904 | `	nProd = 1;` |
|      9 | 4905 | `	for( n = 0 ; n < pMap->nEntry ; n++ ){` |
|      7 | 4906 | `		pObj = HashmapExtractNodeValue(pEntry);` |
|      7 | 4907 | `		if( pObj && (pObj->iFlags & (MEMOBJ_NULL\|MEMOBJ_HASHMAP\|MEMOBJ_OBJ\|MEMOBJ_RES)) == 0){` |
|      7 | 4908 | `			if( pObj->iFlags & MEMOBJ_REAL ){` |
|    ! 0 | 4909 | `				nProd *= (sxi64)pObj->rVal;` |
|      7 | 4910 | `			}else if( pObj->iFlags & (MEMOBJ_INT\|MEMOBJ_BOOL) ){` |
|      7 | 4911 | `				nProd *= pObj->x.iVal;` |
|      3 | 4912 | `			}else if( pObj->iFlags & MEMOBJ_STRING ){` |
|    ! 0 | 4913 | `				if( SyBlobLength(&pObj->sBlob) > 0 ){` |
|    ! 0 | 4914 | `					sxi64 nv = 0;` |
|    ! 0 | 4915 | `					SyStrToInt64((const char *)SyBlobData(&pObj->sBlob),SyBlobLength(&pObj->sBlob),(void *)&nv,0);` |
|    ! 0 | 4916 | `					nProd *= nv;` |
|    ! 0 | 4917 | `				}` |
|    ! 0 | 4918 | `			}` |
|      3 | 4919 | `		}` |
|      - | 4920 | `		/* Point to the next entry */` |
|      7 | 4921 | `		pEntry = pEntry->pPrev; /* Reverse link */` |
|      4 | 4922 | `	}` |
|      - | 4923 | `	/* Return product */` |
|      3 | 4924 | `	ph7_result_int64(pCtx,nProd);` |
|      3 | 4925 |  |
|      - | 4926 | `/* number array_product(array $array )` |
|      - | 4927 | ` * (See block-block comment above)` |
|      - | 4928 | ` */` |
|      2 | 4929 | `static int ph7_hashmap_product(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 4930 |  |
|      - | 4931 | `	ph7_hashmap *pMap;` |
|      - | 4932 | `	ph7_value *pObj;` |
|      3 | 4933 | `	if( nArg < 1 ){` |
|      - | 4934 | `		/* Missing arguments,return 0 */` |
|    ! 0 | 4935 | `		ph7_result_int(pCtx,0);` |
|    ! 0 | 4936 | `		return PH7_OK;` |
|      - | 4937 | `	}` |
|      - | 4938 | `	/* Make sure we are dealing with a valid hashmap */` |
|      3 | 4939 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|      - | 4940 | `		/* Invalid argument,return 0 */` |
|    ! 0 | 4941 | `		ph7_result_int(pCtx,0);` |
|    ! 0 | 4942 | `		return PH7_OK;` |
|      - | 4943 | `	}` |
|      3 | 4944 | `	pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|      3 | 4945 | `	if( pMap->nEntry < 1 ){` |
|      - | 4946 | `		/* Nothing to compute,return 0 */` |
|    ! 0 | 4947 | `		ph7_result_int(pCtx,0);` |
|    ! 0 | 4948 | `		return PH7_OK;` |
|      - | 4949 | `	}` |
|      - | 4950 | `	/* If the first element is of type float,then perform floating` |
|      - | 4951 | `	 * point computaion.Otherwise switch to int64 computaion.` |
|      - | 4952 | `	 */` |
|      3 | 4953 | `	pObj = HashmapExtractNodeValue(pMap->pFirst);` |
|      3 | 4954 | `	if( pObj == 0 ){` |
|    ! 0 | 4955 | `		ph7_result_int(pCtx,0);` |
|    ! 0 | 4956 | `		return PH7_OK;` |
|      - | 4957 | `	}` |
|      3 | 4958 | `	if( pObj->iFlags & MEMOBJ_REAL ){` |
|    ! 0 | 4959 | `		DoubleProd(pCtx,pMap);` |
|    ! 0 | 4960 | `	}else{` |
|      3 | 4961 | `		Int64Prod(pCtx,pMap);` |
|      - | 4962 | `	}` |
|      3 | 4963 | `	return PH7_OK;` |
|      2 | 4964 |  |
|      - | 4965 | `/*` |
|      - | 4966 | ` * value array_rand(array $input[,int $num_req = 1 ])` |
|      - | 4967 | ` *  Pick one or more random entries out of an array.` |
|      - | 4968 | ` * Parameters` |
|      - | 4969 | ` * $input` |
|      - | 4970 | ` *  The input array.` |
|      - | 4971 | ` * $num_req` |
|      - | 4972 | ` *  Specifies how many entries you want to pick.` |
|      - | 4973 | ` * Return` |
|      - | 4974 | ` *  If you are picking only one entry, array_rand() returns the key for a random entry.` |
|      - | 4975 | ` *  Otherwise, it returns an array of keys for the random entries.` |
|      - | 4976 | ` *  NULL is returned on failure.` |
|      - | 4977 | ` */` |
|      6 | 4978 | `static int ph7_hashmap_rand(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 4979 |  |
|      - | 4980 | `	ph7_hashmap_node *pNode;` |
|      - | 4981 | `	ph7_hashmap *pMap;` |
|      7 | 4982 | `	int nItem = 1;` |
|      7 | 4983 | `	if( nArg < 1 ){` |
|      - | 4984 | `		/* Missing argument,return NULL */` |
|    ! 0 | 4985 | `		ph7_result_null(pCtx);` |
|    ! 0 | 4986 | `		return PH7_OK;` |
|      - | 4987 | `	}` |
|      - | 4988 | `	/* Make sure we are dealing with an array */` |
|      7 | 4989 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|    ! 0 | 4990 | `		ph7_result_null(pCtx);` |
|    ! 0 | 4991 | `		return PH7_OK;` |
|      - | 4992 | `	}` |
|      - | 4993 | `	/* Point to the internal representation of the input hashmap */` |
|      7 | 4994 | `	pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|      7 | 4995 | `	if(pMap->nEntry < 1 ){` |
|      - | 4996 | `		/* Empty hashmap,return NULL */` |
|    ! 0 | 4997 | `		ph7_result_null(pCtx);` |
|    ! 0 | 4998 | `		return PH7_OK;` |
|      - | 4999 | `	}` |
|      7 | 5000 | `	if( nArg > 1 ){` |
|      3 | 5001 | `		nItem = ph7_value_to_int(apArg[1]);` |
|      1 | 5002 | `	}` |
|      7 | 5003 | `	if( nItem < 2 ){` |
|      - | 5004 | `		sxu32 nEntry;` |
|      - | 5005 | `		/* Select a random number */` |
|      5 | 5006 | `		nEntry = PH7_VmRandomNum(pMap->pVm) % pMap->nEntry;` |
|      - | 5007 | `		/* Extract the desired entry.` |
|      - | 5008 | `		 * Note that we perform a linear lookup here (later version must change this)` |
|      - | 5009 | `		 */` |
|      5 | 5010 | `		if( nEntry > pMap->nEntry / 2 ){` |
|      2 | 5011 | `			pNode = pMap->pLast;` |
|      2 | 5012 | `			nEntry = pMap->nEntry - nEntry;` |
|      2 | 5013 | `			if( nEntry > 1 ){` |
|    ! 0 | 5014 | `				for(;;){` |
|    ! 0 | 5015 | `					if( nEntry == 0 ){` |
|    ! 0 | 5016 | `						break;` |
|      - | 5017 | `					}` |
|      - | 5018 | `					/* Point to the previous entry */` |
|    ! 0 | 5019 | `					pNode = pNode->pNext; /* Reverse link */` |
|    ! 0 | 5020 | `					nEntry--;` |
|    ! 0 | 5021 | `				}` |
|    ! 0 | 5022 | `			}` |
|      1 | 5023 | `		}else{` |
|      4 | 5024 | `			pNode = pMap->pFirst;` |
|      3 | 5025 | `			for(;;){` |
|      5 | 5026 | `				if( nEntry == 0 ){` |
|      4 | 5027 | `					break;` |
|      - | 5028 | `				}` |
|      - | 5029 | `				/* Point to the next entry */` |
|      2 | 5030 | `				pNode = pNode->pPrev; /* Reverse link */` |
|      2 | 5031 | `				nEntry--;` |
|      1 | 5032 | `			}` |
|      - | 5033 | `		}` |
|      5 | 5034 | `		if( pNode->iType == HASHMAP_INT_NODE ){` |
|      - | 5035 | `			/* Int key */` |
|      3 | 5036 | `			ph7_result_int64(pCtx,pNode->xKey.iKey);` |
|      2 | 5037 | `		}else{` |
|      - | 5038 | `			/* Blob key */` |
|      3 | 5039 | `			ph7_result_string(pCtx,(const char *)SyBlobData(&pNode->xKey.sKey),(int)SyBlobLength(&pNode->xKey.sKey));` |
|      - | 5040 | `		}` |
|      3 | 5041 | `	}else{` |
|      - | 5042 | `		ph7_value sKey,*pArray;` |
|      - | 5043 | `		ph7_hashmap *pDest;` |
|      - | 5044 | `		/* Create a new array */` |
|      3 | 5045 | `		pArray = ph7_context_new_array(pCtx);` |
|      3 | 5046 | `		if( pArray == 0 ){` |
|    ! 0 | 5047 | `			ph7_result_null(pCtx);` |
|    ! 0 | 5048 | `			return PH7_OK;` |
|      - | 5049 | `		}` |
|      - | 5050 | `		/* Point to the internal representation of the hashmap */` |
|      3 | 5051 | `		pDest = (ph7_hashmap *)pArray->x.pOther;` |
|      3 | 5052 | `		PH7_MemObjInit(pDest->pVm,&sKey);` |
|      - | 5053 | `		/* Copy the first n items */` |
|      3 | 5054 | `		pNode = pMap->pFirst;` |
|      3 | 5055 | `		if( nItem > (int)pMap->nEntry ){` |
|    ! 0 | 5056 | `			nItem = (int)pMap->nEntry;` |
|    ! 0 | 5057 | `		}` |
|      7 | 5058 | `		while( nItem > 0){` |
|      5 | 5059 | `			PH7_HashmapExtractNodeKey(pNode,&sKey);` |
|      5 | 5060 | `			PH7_HashmapInsert(pDest,0/* Automatic index assign*/,&sKey);` |
|      5 | 5061 | `			PH7_MemObjRelease(&sKey);` |
|      - | 5062 | `			/* Point to the next entry */` |
|      5 | 5063 | `			pNode = pNode->pPrev; /* Reverse link */` |
|      5 | 5064 | `			nItem--;` |
|      1 | 5065 | `		}` |
|      - | 5066 | `		/* Shuffle the array */` |
|      3 | 5067 | `		HashmapMergeSort(pDest,HashmapCmpCallback7,0);` |
|      - | 5068 | `		/* Rehash node */` |
|      3 | 5069 | `		HashmapSortRehash(pDest);` |
|      - | 5070 | `		/* Return the random array */` |
|      3 | 5071 | `		ph7_result_value(pCtx,pArray);` |
|      - | 5072 | `	}` |
|      7 | 5073 | `	return PH7_OK;` |
|      4 | 5074 |  |
|      - | 5075 | `/*` |
|      - | 5076 | ` * array array_chunk (array $input,int $size [,bool $preserve_keys = false ])` |
|      - | 5077 | ` *  Split an array into chunks.` |
|      - | 5078 | ` * Parameters` |
|      - | 5079 | ` * $input` |
|      - | 5080 | ` *   The array to work on` |
|      - | 5081 | ` * $size` |
|      - | 5082 | ` *   The size of each chunk` |
|      - | 5083 | ` * $preserve_keys` |
|      - | 5084 | ` *   When set to TRUE keys will be preserved. Default is FALSE which will reindex` |
|      - | 5085 | ` *   the chunk numerically.` |
|      - | 5086 | ` * Return` |
|      - | 5087 | ` *  Returns a multidimensional numerically indexed array, starting with` |
|      - | 5088 | ` *  zero, with each dimension containing size elements.` |
|      - | 5089 | ` */` |
|     12 | 5090 | `static int ph7_hashmap_chunk(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 5091 |  |
|      - | 5092 | `	ph7_value *pArray,*pChunk;` |
|      - | 5093 | `	ph7_hashmap_node *pEntry;` |
|      - | 5094 | `	ph7_hashmap *pMap;` |
|      - | 5095 | `	int bPreserve;` |
|      - | 5096 | `	sxu32 nChunk;` |
|      - | 5097 | `	sxu32 nSize;` |
|      - | 5098 | `	sxu32 n;` |
|     13 | 5099 | `	if( nArg < 2 \|\| !ph7_value_is_array(apArg[0]) ){` |
|      - | 5100 | `		/* Invalid arguments,return NULL */` |
|      5 | 5101 | `		ph7_result_null(pCtx);` |
|      5 | 5102 | `		return PH7_OK;` |
|      - | 5103 | `	}` |
|      - | 5104 | `	/* Create a new array */` |
|      9 | 5105 | `	pArray = ph7_context_new_array(pCtx);` |
|      9 | 5106 | `	if( pArray == 0 ){` |
|    ! 0 | 5107 | `		ph7_result_null(pCtx);` |
|    ! 0 | 5108 | `		return PH7_OK;` |
|      - | 5109 | `	}` |
|      - | 5110 | `	/* Point to the internal representation of the input hashmap */` |
|      9 | 5111 | `	pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|      - | 5112 | `	/* Extract the chunk size */` |
|      9 | 5113 | `	nSize = (sxu32)ph7_value_to_int(apArg[1]);` |
|      9 | 5114 | `	if( nSize < 1 ){` |
|      3 | 5115 | `		ph7_result_null(pCtx);` |
|      3 | 5116 | `		return PH7_OK;` |
|      - | 5117 | `	}` |
|      7 | 5118 | `	if( nSize >= pMap->nEntry ){` |
|      - | 5119 | `		/* Return the whole array */` |
|      3 | 5120 | `		ph7_array_add_elem(pArray,0,apArg[0]);` |
|      3 | 5121 | `		ph7_result_value(pCtx,pArray);` |
|      3 | 5122 | `		return PH7_OK;` |
|      - | 5123 | `	}` |
|      5 | 5124 | `	bPreserve = 0;` |
|      5 | 5125 | `	if( nArg > 2 ){` |
|      3 | 5126 | `		bPreserve = ph7_value_to_bool(apArg[2]);` |
|      1 | 5127 | `	}` |
|      - | 5128 | `	/* Start processing */` |
|      5 | 5129 | `	pEntry = pMap->pFirst;` |
|      5 | 5130 | `	nChunk = 0;` |
|      5 | 5131 | `	pChunk = 0;` |
|      5 | 5132 | `	n = pMap->nEntry;` |
|     12 | 5133 | `	for( ;; ){` |
|     25 | 5134 | `		if( n < 1 ){` |
|      5 | 5135 | `			if( nChunk > 0 ){` |
|      - | 5136 | `				/* Insert the last chunk */` |
|      5 | 5137 | `				ph7_array_add_elem(pArray,0,pChunk); /* Will have it's own copy */` |
|      2 | 5138 | `			}` |
|      5 | 5139 | `			break;` |
|      - | 5140 | `		}` |
|     21 | 5141 | `		if( nChunk < 1 ){` |
|     13 | 5142 | `			if( pChunk ){` |
|      - | 5143 | `				/* Put the first chunk */` |
|      9 | 5144 | `				ph7_array_add_elem(pArray,0,pChunk); /* Will have it's own copy */` |
|      4 | 5145 | `			}` |
|      - | 5146 | `			/* Create a new dimension */` |
|     13 | 5147 | `			pChunk = ph7_context_new_array(pCtx); /* Don't worry about freeing memory here,everything` |
|      - | 5148 | `												   * will be automatically released as soon we return` |
|      - | 5149 | `												   * from this function */` |
|     13 | 5150 | `			if( pChunk == 0 ){` |
|    ! 0 | 5151 | `				break;` |
|      - | 5152 | `			}` |
|     13 | 5153 | `			nChunk = nSize;` |
|      6 | 5154 | `		}` |
|      - | 5155 | `		/* Insert the entry */` |
|     21 | 5156 | `		HashmapInsertNode((ph7_hashmap *)pChunk->x.pOther,pEntry,bPreserve);` |
|      - | 5157 | `		/* Point to the next entry */` |
|     21 | 5158 | `		pEntry = pEntry->pPrev; /* Reverse link */` |
|     21 | 5159 | `		nChunk--;` |
|     21 | 5160 | `		n--;` |
|      1 | 5161 | `	}` |
|      - | 5162 | `	/* Return the multidimensional array */` |
|      5 | 5163 | `	ph7_result_value(pCtx,pArray);` |
|      5 | 5164 | `	return PH7_OK;` |
|      7 | 5165 |  |
|      - | 5166 | `/*` |
|      - | 5167 | ` * array array_pad(array $input,int $pad_size,value $pad_value)` |
|      - | 5168 | ` *  Pad array to the specified length with a value.` |
|      - | 5169 | ` * $input` |
|      - | 5170 | ` *   Initial array of values to pad.` |
|      - | 5171 | ` * $pad_size` |
|      - | 5172 | ` *   New size of the array.` |
|      - | 5173 | ` * $pad_value` |
|      - | 5174 | ` *   Value to pad if input is less than pad_size.` |
|      - | 5175 | ` */` |
|      8 | 5176 | `static int ph7_hashmap_pad(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 5177 |  |
|      - | 5178 | `	ph7_hashmap *pMap;` |
|      - | 5179 | `	ph7_value *pArray;` |
|      - | 5180 | `	int nEntry;` |
|      9 | 5181 | `	if( nArg < 3 \|\| !ph7_value_is_array(apArg[0]) ){` |
|      - | 5182 | `		/* Invalid arguments,return NULL */` |
|    ! 0 | 5183 | `		ph7_result_null(pCtx);` |
|    ! 0 | 5184 | `		return PH7_OK;` |
|      - | 5185 | `	}` |
|      - | 5186 | `	/* Create a new array */` |
|      9 | 5187 | `	pArray = ph7_context_new_array(pCtx);` |
|      9 | 5188 | `	if( pArray == 0 ){` |
|    ! 0 | 5189 | `		ph7_result_null(pCtx);` |
|    ! 0 | 5190 | `		return PH7_OK;` |
|      - | 5191 | `	}` |
|      - | 5192 | `	/* Point to the internal representation of the input hashmap */` |
|      9 | 5193 | `	pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|      - | 5194 | `	/* Extract the total number of desired entry to insert */` |
|      9 | 5195 | `	nEntry = ph7_value_to_int(apArg[1]);` |
|      9 | 5196 | `	if( nEntry < 0 ){` |
|      5 | 5197 | `		nEntry = -nEntry;` |
|      5 | 5198 | `		if( nEntry > 1048576 ){` |
|    ! 0 | 5199 | `			nEntry = 1048576; /* Limit imposed by PHP */` |
|    ! 0 | 5200 | `		}` |
|      5 | 5201 | `		if( nEntry > (int)pMap->nEntry ){` |
|      3 | 5202 | `			nEntry -= (int)pMap->nEntry;` |
|      - | 5203 | `			/* Insert given items first */` |
|      7 | 5204 | `			while( nEntry > 0 ){` |
|      5 | 5205 | `				ph7_array_add_elem(pArray,0,apArg[2]);` |
|      5 | 5206 | `				nEntry--;` |
|      1 | 5207 | `			}` |
|      - | 5208 | `			/* Merge the two arrays */` |
|      3 | 5209 | `			HashmapMerge(pMap,(ph7_hashmap *)pArray->x.pOther);` |
|      2 | 5210 | `		}else{` |
|      3 | 5211 | `			PH7_HashmapDup(pMap,(ph7_hashmap *)pArray->x.pOther);` |
|      1 | 5212 | `		}` |
|      7 | 5213 | `	}else if( nEntry > 0 ){` |
|      5 | 5214 | `		if( nEntry > 1048576 ){` |
|    ! 0 | 5215 | `			nEntry = 1048576; /* Limit imposed by PHP */` |
|    ! 0 | 5216 | `		}` |
|      5 | 5217 | `		if( nEntry > (int)pMap->nEntry ){` |
|      3 | 5218 | `			nEntry -= (int)pMap->nEntry;` |
|      - | 5219 | `			/* Merge the two arrays first */` |
|      3 | 5220 | `			HashmapMerge(pMap,(ph7_hashmap *)pArray->x.pOther);` |
|      - | 5221 | `			/* Insert given items */` |
|      7 | 5222 | `			while( nEntry > 0 ){` |
|      5 | 5223 | `				ph7_array_add_elem(pArray,0,apArg[2]);` |
|      5 | 5224 | `				nEntry--;` |
|      1 | 5225 | `			}` |
|      2 | 5226 | `		}else{` |
|      3 | 5227 | `			PH7_HashmapDup(pMap,(ph7_hashmap *)pArray->x.pOther);` |
|      - | 5228 | `		}` |
|      2 | 5229 | `	}` |
|      - | 5230 | `	/* Return the new array */` |
|      9 | 5231 | `	ph7_result_value(pCtx,pArray);` |
|      9 | 5232 | `	return PH7_OK;` |
|      5 | 5233 |  |
|      - | 5234 | `/*` |
|      - | 5235 | ` * array array_replace(array &$array,array &$array1,...)` |
|      - | 5236 | ` *  Replaces elements from passed arrays into the first array.` |
|      - | 5237 | ` * Parameters` |
|      - | 5238 | ` * $array` |
|      - | 5239 | ` *   The array in which elements are replaced.` |
|      - | 5240 | ` * $array1` |
|      - | 5241 | ` *   The array from which elements will be extracted.` |
|      - | 5242 | ` * ....` |
|      - | 5243 | ` *  More arrays from which elements will be extracted.` |
|      - | 5244 | ` *  Values from later arrays overwrite the previous values.` |
|      - | 5245 | ` * Return` |
|      - | 5246 | ` *  Returns an array, or NULL if an error occurs.` |
|      - | 5247 | ` */` |
|      2 | 5248 | `static int ph7_hashmap_replace(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 5249 |  |
|      - | 5250 | `	ph7_hashmap *pMap;` |
|      - | 5251 | `	ph7_value *pArray;` |
|      - | 5252 | `	int i;` |
|      3 | 5253 | `	if( nArg < 1 ){` |
|      - | 5254 | `		/* Invalid arguments,return NULL */` |
|    ! 0 | 5255 | `		ph7_result_null(pCtx);` |
|    ! 0 | 5256 | `		return PH7_OK;` |
|      - | 5257 | `	}` |
|      - | 5258 | `	/* Create a new array */` |
|      3 | 5259 | `	pArray = ph7_context_new_array(pCtx);` |
|      3 | 5260 | `	if( pArray == 0 ){` |
|    ! 0 | 5261 | `		ph7_result_null(pCtx);` |
|    ! 0 | 5262 | `		return PH7_OK;` |
|      - | 5263 | `	}` |
|      - | 5264 | `	/* Perform the requested operation */` |
|      7 | 5265 | `	for( i = 0 ; i < nArg ; i++ ){` |
|      5 | 5266 | `		if( !ph7_value_is_array(apArg[i]) ){` |
|    ! 0 | 5267 | `			continue;` |
|      - | 5268 | `		}` |
|      - | 5269 | `		/* Point to the internal representation of the input hashmap */` |
|      5 | 5270 | `		pMap = (ph7_hashmap *)apArg[i]->x.pOther;` |
|      5 | 5271 | `		HashmapOverwrite(pMap,(ph7_hashmap *)pArray->x.pOther);` |
|      3 | 5272 | `	}` |
|      - | 5273 | `	/* Return the new array */` |
|      3 | 5274 | `	ph7_result_value(pCtx,pArray);` |
|      3 | 5275 | `	return PH7_OK;` |
|      2 | 5276 |  |
|      - | 5277 | `/*` |
|      - | 5278 | ` * array array_filter(array $input [,callback $callback ])` |
|      - | 5279 | ` *  Filters elements of an array using a callback function.` |
|      - | 5280 | ` * Parameters` |
|      - | 5281 | ` *  $input` |
|      - | 5282 | ` *    The array to iterate over` |
|      - | 5283 | ` * $callback` |
|      - | 5284 | ` *    The callback function to use` |
|      - | 5285 | ` *    If no callback is supplied, all entries of input equal to FALSE (see converting to boolean)` |
|      - | 5286 | ` *    will be removed.` |
|      - | 5287 | ` * Return` |
|      - | 5288 | ` *  The filtered array.` |
|      - | 5289 | ` */` |
|      8 | 5290 | `static int ph7_hashmap_filter(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 5291 |  |
|      - | 5292 | `	ph7_hashmap_node *pEntry;` |
|      - | 5293 | `	ph7_hashmap *pMap;` |
|      - | 5294 | `	ph7_value *pArray;` |
|      - | 5295 | `	ph7_value sResult;   /* Callback result */` |
|      - | 5296 | `	ph7_value *pValue;` |
|      - | 5297 | `	sxi32 rc;` |
|      - | 5298 | `	int keep;` |
|      - | 5299 | `	sxu32 n;` |
|      9 | 5300 | `	if( nArg < 1 \|\| !ph7_value_is_array(apArg[0]) ){` |
|      - | 5301 | `		/* Invalid arguments,return NULL */` |
|      5 | 5302 | `		ph7_result_null(pCtx);` |
|      5 | 5303 | `		return PH7_OK;` |
|      - | 5304 | `	}` |
|      - | 5305 | `	/* Create a new array */` |
|      5 | 5306 | `	pArray = ph7_context_new_array(pCtx);` |
|      5 | 5307 | `	if( pArray == 0 ){` |
|    ! 0 | 5308 | `		ph7_result_null(pCtx);` |
|    ! 0 | 5309 | `		return PH7_OK;` |
|      - | 5310 | `	}` |
|      - | 5311 | `	/* Point to the internal representation of the input hashmap */` |
|      5 | 5312 | `	pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|      5 | 5313 | `	pEntry = pMap->pFirst;` |
|      5 | 5314 | `	PH7_MemObjInit(pMap->pVm,&sResult);` |
|      5 | 5315 | `	sResult.nIdx = SXU32_HIGH; /* Mark as constant */` |
|      - | 5316 | `	/* Perform the requested operation */` |
|     21 | 5317 | `	for( n = 0 ; n < pMap->nEntry ; n++ ){` |
|      - | 5318 | `		/* Extract node value */` |
|     17 | 5319 | `		pValue = HashmapExtractNodeValue(pEntry);` |
|     17 | 5320 | `		if( nArg > 1 && pValue ){` |
|      - | 5321 | `			/* Invoke the given callback */` |
|     17 | 5322 | `			keep = FALSE;` |
|     17 | 5323 | `			rc = PH7_VmCallUserFunction(pMap->pVm,apArg[1],1,&pValue,&sResult);` |
|     17 | 5324 | `			if( rc == SXRET_OK ){` |
|      - | 5325 | `				/* Perform a boolean cast */` |
|     17 | 5326 | `				keep = ph7_value_to_bool(&sResult);` |
|      8 | 5327 | `			}` |
|     17 | 5328 | `			PH7_MemObjRelease(&sResult);` |
|      9 | 5329 | `		}else{` |
|      - | 5330 | `			/* No available callback,check for empty item */` |
|    ! 0 | 5331 | `			keep = !PH7_MemObjIsEmpty(pValue);` |
|      - | 5332 | `		}` |
|     17 | 5333 | `		if( keep ){` |
|      - | 5334 | `			/* Perform the insertion,now the callback returned true */` |
|      5 | 5335 | `			HashmapInsertNode((ph7_hashmap *)pArray->x.pOther,pEntry,TRUE);` |
|      2 | 5336 | `		}` |
|      - | 5337 | `		/* Point to the next entry */` |
|     17 | 5338 | `		pEntry = pEntry->pPrev; /* Reverse link */` |
|      9 | 5339 | `	}` |
|      5 | 5340 | `	ph7_result_value(pCtx,pArray);` |
|      5 | 5341 | `	return PH7_OK;` |
|      5 | 5342 |  |
|      - | 5343 | `/*` |
|      - | 5344 | ` * array array_map(callback $callback,array $arr1)` |
|      - | 5345 | ` *  Applies the callback to the elements of the given arrays.` |
|      - | 5346 | ` * Parameters` |
|      - | 5347 | ` *  $callback` |
|      - | 5348 | ` *   Callback function to run for each element in each array.` |
|      - | 5349 | ` * $arr1` |
|      - | 5350 | ` *   An array to run through the callback function.` |
|      - | 5351 | ` * Return` |
|      - | 5352 | ` *  Returns an array containing all the elements of arr1 after applying` |
|      - | 5353 | ` *  the callback function to each one.` |
|      - | 5354 | ` * NOTE:` |
|      - | 5355 | ` *  array_map() passes only a single value to the callback.` |
|      - | 5356 | ` */` |
|     10 | 5357 | `static int ph7_hashmap_map(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 5358 |  |
|      - | 5359 | `	ph7_value *pArray,*pValue,sKey,sResult;` |
|      - | 5360 | `	ph7_hashmap_node *pEntry;` |
|      - | 5361 | `	ph7_hashmap *pMap;` |
|      - | 5362 | `	sxu32 n;` |
|     11 | 5363 | `	if( nArg < 2 \|\| !ph7_value_is_array(apArg[1]) ){` |
|      - | 5364 | `		/* Invalid arguments,return NULL */` |
|      5 | 5365 | `		ph7_result_null(pCtx);` |
|      5 | 5366 | `		return PH7_OK;` |
|      - | 5367 | `	}` |
|      - | 5368 | `	/* Create a new array */` |
|      7 | 5369 | `	pArray = ph7_context_new_array(pCtx);` |
|      7 | 5370 | `	if( pArray == 0 ){` |
|    ! 0 | 5371 | `		ph7_result_null(pCtx);` |
|    ! 0 | 5372 | `		return PH7_OK;` |
|      - | 5373 | `	}` |
|      - | 5374 | `	/* Point to the internal representation of the input hashmap */` |
|      7 | 5375 | `	pMap = (ph7_hashmap *)apArg[1]->x.pOther;` |
|      7 | 5376 | `	PH7_MemObjInit(pMap->pVm,&sResult);` |
|      7 | 5377 | `	PH7_MemObjInit(pMap->pVm,&sKey);` |
|      7 | 5378 | `	sResult.nIdx = SXU32_HIGH; /* Mark as constant */` |
|      7 | 5379 | `	sKey.nIdx    = SXU32_HIGH; /* Mark as constant */` |
|      - | 5380 | `	/* Perform the requested operation */` |
|      7 | 5381 | `	pEntry = pMap->pFirst;` |
|     21 | 5382 | `	for( n = 0 ; n < pMap->nEntry ; n++ ){` |
|      - | 5383 | `		/* Extrcat the node value */` |
|     15 | 5384 | `		pValue = HashmapExtractNodeValue(pEntry);` |
|     15 | 5385 | `		if( pValue ){` |
|      - | 5386 | `			sxi32 rc;` |
|      - | 5387 | `			/* Invoke the supplied callback */` |
|     15 | 5388 | `			rc = PH7_VmCallUserFunction(pMap->pVm,apArg[0],1,&pValue,&sResult);` |
|      - | 5389 | `			/* Extract the node key */` |
|     15 | 5390 | `			PH7_HashmapExtractNodeKey(pEntry,&sKey);` |
|     15 | 5391 | `			if( rc != SXRET_OK ){` |
|      - | 5392 | `				/* An error occured while invoking the supplied callback [i.e: not defined] */` |
|    ! 0 | 5393 | `				ph7_array_add_elem(pArray,&sKey,pValue); /* Keep the same value */` |
|    ! 0 | 5394 | `			}else{` |
|      - | 5395 | `				/* Insert the callback return value */` |
|     15 | 5396 | `				ph7_array_add_elem(pArray,&sKey,&sResult);` |
|      - | 5397 | `			}` |
|     15 | 5398 | `			PH7_MemObjRelease(&sKey);` |
|     15 | 5399 | `			PH7_MemObjRelease(&sResult);` |
|      7 | 5400 | `		}` |
|      - | 5401 | `		/* Point to the next entry */` |
|     15 | 5402 | `		pEntry = pEntry->pPrev; /* Reverse link */` |
|      8 | 5403 | `	}` |
|      7 | 5404 | `	ph7_result_value(pCtx,pArray);` |
|      7 | 5405 | `	return PH7_OK;` |
|      6 | 5406 |  |
|      - | 5407 | `/*` |
|      - | 5408 | ` * value array_reduce(array $input,callback $function[, value $initial = NULL ])` |
|      - | 5409 | ` *  Iteratively reduce the array to a single value using a callback function.` |
|      - | 5410 | ` * Parameters` |
|      - | 5411 | ` *  $input` |
|      - | 5412 | ` *   The input array.` |
|      - | 5413 | ` *  $function` |
|      - | 5414 | ` *  The callback function.` |
|      - | 5415 | ` * $initial` |
|      - | 5416 | ` *  If the optional initial is available, it will be used at the beginning` |
|      - | 5417 | ` *  of the process, or as a final result in case the array is empty.` |
|      - | 5418 | ` * Return` |
|      - | 5419 | ` *  Returns the resulting value.` |
|      - | 5420 | ` *  If the array is empty and initial is not passed, array_reduce() returns NULL.` |
|      - | 5421 | ` */` |
|      4 | 5422 | `static int ph7_hashmap_reduce(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 5423 |  |
|      - | 5424 | `	ph7_hashmap_node *pEntry;` |
|      - | 5425 | `	ph7_hashmap *pMap;` |
|      - | 5426 | `	ph7_value *pValue;` |
|      - | 5427 | `	ph7_value sResult;` |
|      - | 5428 | `	sxu32 n;` |
|      5 | 5429 | `	if( nArg < 2 \|\| !ph7_value_is_array(apArg[0]) ){` |
|      - | 5430 | `		/* Invalid/Missing arguments,return NULL */` |
|    ! 0 | 5431 | `		ph7_result_null(pCtx);` |
|    ! 0 | 5432 | `		return PH7_OK;` |
|      - | 5433 | `	}` |
|      - | 5434 | `	/* Point to the internal representation of the input hashmap */` |
|      5 | 5435 | `	pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|      - | 5436 | `	/* Assume a NULL initial value */` |
|      5 | 5437 | `	PH7_MemObjInit(pMap->pVm,&sResult);` |
|      5 | 5438 | `	sResult.nIdx = SXU32_HIGH; /* Mark as constant */` |
|      5 | 5439 | `	if( nArg > 2 ){` |
|      - | 5440 | `		/* Set the initial value */` |
|      5 | 5441 | `		PH7_MemObjLoad(apArg[2],&sResult);` |
|      2 | 5442 | `	}` |
|      - | 5443 | `	/* Perform the requested operation */` |
|      5 | 5444 | `	pEntry = pMap->pFirst;` |
|     19 | 5445 | `	for( n = 0 ; n < pMap->nEntry ; n++ ){` |
|      - | 5446 | `		/* Extract the node value */` |
|     15 | 5447 | `		pValue = HashmapExtractNodeValue(pEntry);` |
|      - | 5448 | `		/* Invoke the supplied callback */` |
|     15 | 5449 | `		PH7_VmCallUserFunctionAp(pMap->pVm,apArg[1],&sResult,&sResult,pValue,0);` |
|      - | 5450 | `		/* Point to the next entry */` |
|     15 | 5451 | `		pEntry = pEntry->pPrev; /* Reverse link */` |
|      8 | 5452 | `	}` |
|      5 | 5453 | `	ph7_result_value(pCtx,&sResult); /* Will make it's own copy */` |
|      5 | 5454 | `	PH7_MemObjRelease(&sResult);` |
|      5 | 5455 | `	return PH7_OK;` |
|      3 | 5456 |  |
|      - | 5457 | `/*` |
|      - | 5458 | ` * bool array_walk(array &$array,callback $funcname [, value $userdata ] )` |
|      - | 5459 | ` *  Apply a user function to every member of an array.` |
|      - | 5460 | ` * Parameters` |
|      - | 5461 | ` *  $array` |
|      - | 5462 | ` *   The input array.` |
|      - | 5463 | ` * $funcname` |
|      - | 5464 | ` *  Typically, funcname takes on two parameters.The array parameter's value being` |
|      - | 5465 | ` *  the first, and the key/index second.` |
|      - | 5466 | ` * Note:` |
|      - | 5467 | ` *  If funcname needs to be working with the actual values of the array,specify the first` |
|      - | 5468 | ` *  parameter of funcname as a reference. Then, any changes made to those elements will` |
|      - | 5469 | ` *  be made in the original array itself.` |
|      - | 5470 | ` * $userdata` |
|      - | 5471 | ` *  If the optional userdata parameter is supplied, it will be passed as the third parameter` |
|      - | 5472 | ` *  to the callback funcname.` |
|      - | 5473 | ` * Return` |
|      - | 5474 | ` *  Returns TRUE on success or FALSE on failure.` |
|      - | 5475 | ` */` |
|     12 | 5476 | `static int ph7_hashmap_walk(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 5477 |  |
|      - | 5478 | `	ph7_value *pValue,*pUserData,sKey;` |
|      - | 5479 | `	ph7_hashmap_node *pEntry;` |
|      - | 5480 | `	ph7_hashmap *pMap;` |
|      - | 5481 | `	sxi32 rc;` |
|      - | 5482 | `	sxu32 n;` |
|     13 | 5483 | `	if( nArg < 2 \|\| !ph7_value_is_array(apArg[0]) ){` |
|      - | 5484 | `		/* Invalid/Missing arguments,return FALSE */` |
|    ! 0 | 5485 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 5486 | `		return PH7_OK;` |
|      - | 5487 | `	}` |
|     13 | 5488 | `	pUserData = nArg > 2 ? apArg[2] : 0;` |
|      - | 5489 | `	/* Point to the internal representation of the input hashmap */` |
|     13 | 5490 | `	pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|     13 | 5491 | `	PH7_MemObjInit(pMap->pVm,&sKey);` |
|     13 | 5492 | `	sKey.nIdx = SXU32_HIGH; /* Mark as constant */` |
|      - | 5493 | `	/* Perform the desired operation */` |
|     13 | 5494 | `	pEntry = pMap->pFirst;` |
|     41 | 5495 | `	for( n = 0 ; n < pMap->nEntry ; n++ ){` |
|      - | 5496 | `		/* Extract the node value */` |
|     29 | 5497 | `		pValue = HashmapExtractNodeValue(pEntry);` |
|     29 | 5498 | `		if( pValue ){` |
|      - | 5499 | `			/* Extract the entry key */` |
|     29 | 5500 | `			PH7_HashmapExtractNodeKey(pEntry,&sKey);` |
|      - | 5501 | `			/* Invoke the supplied callback */` |
|     29 | 5502 | `			rc = PH7_VmCallUserFunctionAp(pMap->pVm,apArg[1],0,pValue,&sKey,pUserData,0);` |
|     29 | 5503 | `			PH7_MemObjRelease(&sKey);` |
|     29 | 5504 | `			if( rc != SXRET_OK ){` |
|      - | 5505 | `				/* An error occured while invoking the supplied callback [i.e: not defined] */` |
|    ! 0 | 5506 | `				ph7_result_bool(pCtx,0); /* return FALSE */` |
|    ! 0 | 5507 | `				return PH7_OK;` |
|      - | 5508 | `			}` |
|     14 | 5509 | `		}` |
|      - | 5510 | `		/* Point to the next entry */` |
|     29 | 5511 | `		pEntry = pEntry->pPrev; /* Reverse link */` |
|     15 | 5512 | `	}` |
|      - | 5513 | `	/* All done,return TRUE */` |
|     13 | 5514 | `	ph7_result_bool(pCtx,1);` |
|     13 | 5515 | `	return PH7_OK;` |
|      7 | 5516 |  |
|      - | 5517 | `/*` |
|      - | 5518 | ` * Apply a user function to every member of an array.(Recurse on array's).` |
|      - | 5519 | ` * Refer to the [array_walk_recursive()] implementation for more information.` |
|      - | 5520 | ` */` |
|      6 | 5521 | `static int HashmapWalkRecursive(` |
|      - | 5522 | `	ph7_hashmap *pMap,    /* Target hashmap */` |
|      - | 5523 | `	ph7_value *pCallback, /* User callback */` |
|      - | 5524 | `	ph7_value *pUserData, /* Callback private data */` |
|      - | 5525 | `	int iNest             /* Nesting level */` |
|      - | 5526 | `	)` |
|      1 | 5527 |  |
|      - | 5528 | `	ph7_hashmap_node *pEntry;` |
|      - | 5529 | `	ph7_value *pValue,sKey;` |
|      - | 5530 | `	sxi32 rc;` |
|      - | 5531 | `	sxu32 n;` |
|      - | 5532 | `	/* Iterate throw hashmap entries */` |
|      7 | 5533 | `	PH7_MemObjInit(pMap->pVm,&sKey);` |
|      7 | 5534 | `	sKey.nIdx = SXU32_HIGH; /* Mark as constant */` |
|      7 | 5535 | `	pEntry = pMap->pFirst;` |
|     17 | 5536 | `	for( n = 0 ; n < pMap->nEntry ; n++ ){` |
|      - | 5537 | `		/* Extract the node value */` |
|     11 | 5538 | `		pValue = HashmapExtractNodeValue(pEntry);` |
|     11 | 5539 | `		if( pValue ){` |
|     11 | 5540 | `			if( pValue->iFlags & MEMOBJ_HASHMAP ){` |
|      5 | 5541 | `				if( iNest < 32 ){` |
|      - | 5542 | `					/* Recurse */` |
|      5 | 5543 | `					iNest++;` |
|      5 | 5544 | `					HashmapWalkRecursive((ph7_hashmap *)pValue->x.pOther,pCallback,pUserData,iNest);` |
|      5 | 5545 | `					iNest--;` |
|      2 | 5546 | `				}` |
|      3 | 5547 | `			}else{` |
|      - | 5548 | `				/* Extract the node key */` |
|      7 | 5549 | `				PH7_HashmapExtractNodeKey(pEntry,&sKey);` |
|      - | 5550 | `				/* Invoke the supplied callback */` |
|      7 | 5551 | `				rc = PH7_VmCallUserFunctionAp(pMap->pVm,pCallback,0,pValue,&sKey,pUserData,0);` |
|      7 | 5552 | `				PH7_MemObjRelease(&sKey);` |
|      7 | 5553 | `				if( rc != SXRET_OK ){` |
|    ! 0 | 5554 | `					return rc;` |
|      - | 5555 | `				}` |
|      - | 5556 | `			}` |
|      5 | 5557 | `		}` |
|      - | 5558 | `		/* Point to the next entry */` |
|     11 | 5559 | `		pEntry = pEntry->pPrev; /* Reverse link */` |
|      6 | 5560 | `	}` |
|      7 | 5561 | `	return SXRET_OK;` |
|      4 | 5562 |  |
|      - | 5563 | `/*` |
|      - | 5564 | ` * bool array_walk_recursive(array &$array,callback $funcname [, value $userdata ] )` |
|      - | 5565 | ` *  Apply a user function recursively to every member of an array.` |
|      - | 5566 | ` * Parameters` |
|      - | 5567 | ` *  $array` |
|      - | 5568 | ` *   The input array.` |
|      - | 5569 | ` * $funcname` |
|      - | 5570 | ` *  Typically, funcname takes on two parameters.The array parameter's value being` |
|      - | 5571 | ` *  the first, and the key/index second.` |
|      - | 5572 | ` * Note:` |
|      - | 5573 | ` *  If funcname needs to be working with the actual values of the array,specify the first` |
|      - | 5574 | ` *  parameter of funcname as a reference. Then, any changes made to those elements will` |
|      - | 5575 | ` *  be made in the original array itself.` |
|      - | 5576 | ` * $userdata` |
|      - | 5577 | ` *  If the optional userdata parameter is supplied, it will be passed as the third parameter` |
|      - | 5578 | ` *  to the callback funcname.` |
|      - | 5579 | ` * Return` |
|      - | 5580 | ` *  Returns TRUE on success or FALSE on failure.` |
|      - | 5581 | ` */` |
|      2 | 5582 | `static int ph7_hashmap_walk_recursive(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 5583 |  |
|      - | 5584 | `	ph7_hashmap *pMap;` |
|      - | 5585 | `	sxi32 rc;` |
|      3 | 5586 | `	if( nArg < 2 \|\| !ph7_value_is_array(apArg[0]) ){` |
|      - | 5587 | `		/* Invalid/Missing arguments,return FALSE */` |
|    ! 0 | 5588 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 5589 | `		return PH7_OK;` |
|      - | 5590 | `	}` |
|      - | 5591 | `	/* Point to the internal representation of the input hashmap */` |
|      3 | 5592 | `	pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|      - | 5593 | `	/* Perform the desired operation */` |
|      3 | 5594 | `	rc = HashmapWalkRecursive(pMap,apArg[1],nArg > 2 ? apArg[2] : 0,0);` |
|      - | 5595 | `	/* All done */` |
|      3 | 5596 | `	ph7_result_bool(pCtx,rc == SXRET_OK);` |
|      3 | 5597 | `	return PH7_OK;` |
|      2 | 5598 |  |
|      - | 5599 | `/*` |
|      - | 5600 | ` * Table of hashmap functions.` |
|      - | 5601 | ` */` |
|      - | 5602 | `static const ph7_builtin_func aHashmapFunc[] = {` |
|      - | 5603 | `	{"count",             ph7_hashmap_count },` |
|      - | 5604 | `	{"sizeof",            ph7_hashmap_count },` |
|      - | 5605 | `	{"array_key_exists",  ph7_hashmap_key_exists },` |
|      - | 5606 | `	{"array_pop",         ph7_hashmap_pop     },` |
|      - | 5607 | `	{"array_push",        ph7_hashmap_push    },` |
|      - | 5608 | `	{"array_shift",       ph7_hashmap_shift   },` |
|      - | 5609 | `	{"array_product",     ph7_hashmap_product },` |
|      - | 5610 | `	{"array_sum",         ph7_hashmap_sum     },` |
|      - | 5611 | `	{"array_keys",        ph7_hashmap_keys    },` |
|      - | 5612 | `	{"array_values",      ph7_hashmap_values  },` |
|      - | 5613 | `	{"array_same",        ph7_hashmap_same    },  /* Symisc eXtension */` |
|      - | 5614 | `	{"array_merge",       ph7_hashmap_merge   },` |
|      - | 5615 | `	{"array_slice",       ph7_hashmap_slice   },` |
|      - | 5616 | `	{"array_splice",      ph7_hashmap_splice  },` |
|      - | 5617 | `	{"array_search",      ph7_hashmap_search  },` |
|      - | 5618 | `	{"array_diff",        ph7_hashmap_diff    },` |
|      - | 5619 | `	{"array_udiff",       ph7_hashmap_udiff   },` |
|      - | 5620 | `	{"array_diff_assoc",  ph7_hashmap_diff_assoc },` |
|      - | 5621 | `	{"array_diff_uassoc", ph7_hashmap_diff_uassoc },` |
|      - | 5622 | `	{"array_diff_key",    ph7_hashmap_diff_key },` |
|      - | 5623 | `	{"array_intersect",   ph7_hashmap_intersect},` |
|      - | 5624 | `	{"array_intersect_assoc", ph7_hashmap_intersect_assoc},` |
|      - | 5625 | `	{"array_uintersect",  ph7_hashmap_uintersect},` |
|      - | 5626 | `	{"array_intersect_key",   ph7_hashmap_intersect_key},` |
|      - | 5627 | `	{"array_copy",        ph7_hashmap_copy    },` |
|      - | 5628 | `	{"array_erase",       ph7_hashmap_erase   },` |
|      - | 5629 | `	{"array_fill",        ph7_hashmap_fill    },` |
|      - | 5630 | `	{"array_fill_keys",   ph7_hashmap_fill_keys},` |
|      - | 5631 | `	{"array_combine",     ph7_hashmap_combine },` |
|      - | 5632 | `	{"array_reverse",     ph7_hashmap_reverse },` |
|      - | 5633 | `	{"array_unique",      ph7_hashmap_unique  },` |
|      - | 5634 | `	{"array_flip",        ph7_hashmap_flip    },` |
|      - | 5635 | `	{"array_rand",        ph7_hashmap_rand    },` |
|      - | 5636 | `	{"array_chunk",       ph7_hashmap_chunk   },` |
|      - | 5637 | `	{"array_pad",         ph7_hashmap_pad     },` |
|      - | 5638 | `	{"array_replace",     ph7_hashmap_replace },` |
|      - | 5639 | `	{"array_filter",      ph7_hashmap_filter  },` |
|      - | 5640 | `	{"array_map",         ph7_hashmap_map     },` |
|      - | 5641 | `	{"array_reduce",      ph7_hashmap_reduce  },` |
|      - | 5642 | `	{"array_walk",        ph7_hashmap_walk    },` |
|      - | 5643 | `	{"array_walk_recursive", ph7_hashmap_walk_recursive },` |
|      - | 5644 | `	{"in_array",          ph7_hashmap_in_array},` |
|      - | 5645 | `	{"sort",              ph7_hashmap_sort    },` |
|      - | 5646 | `	{"asort",             ph7_hashmap_asort   },` |
|      - | 5647 | `	{"arsort",            ph7_hashmap_arsort  },` |
|      - | 5648 | `	{"ksort",             ph7_hashmap_ksort   },` |
|      - | 5649 | `	{"krsort",            ph7_hashmap_krsort  },` |
|      - | 5650 | `	{"rsort",             ph7_hashmap_rsort   },` |
|      - | 5651 | `	{"usort",             ph7_hashmap_usort   },` |
|      - | 5652 | `	{"uasort",            ph7_hashmap_uasort  },` |
|      - | 5653 | `	{"uksort",            ph7_hashmap_uksort  },` |
|      - | 5654 | `	{"shuffle",           ph7_hashmap_shuffle },` |
|      - | 5655 | `	{"range",             ph7_hashmap_range   },` |
|      - | 5656 | `	{"current",           ph7_hashmap_current },` |
|      - | 5657 | `	{"each",              ph7_hashmap_each    },` |
|      - | 5658 | `	{"pos",               ph7_hashmap_current },` |
|      - | 5659 | `	{"next",              ph7_hashmap_next    },` |
|      - | 5660 | `	{"prev",              ph7_hashmap_prev    },` |
|      - | 5661 | `	{"end",               ph7_hashmap_end     },` |
|      - | 5662 | `	{"reset",             ph7_hashmap_reset   },` |
|      - | 5663 | `	{"key",               ph7_hashmap_simple_key }` |
|      - | 5664 | `};` |
|      - | 5665 | `/*` |
|      - | 5666 | ` * Register the built-in hashmap functions defined above.` |
|      - | 5667 | ` */` |
|    926 | 5668 | `PH7_PRIVATE void PH7_RegisterHashmapFunctions(ph7_vm *pVm)` |
|      2 | 5669 |  |
|      - | 5670 | `	sxu32 n;` |
|  57414 | 5671 | `	for( n = 0 ; n < SX_ARRAYSIZE(aHashmapFunc) ; n++ ){` |
|  56488 | 5672 | `		ph7_create_function(&(*pVm),aHashmapFunc[n].zName,aHashmapFunc[n].xFunc,0);` |
|  28245 | 5673 | `	}` |
|    928 | 5674 |  |
|      - | 5675 | `/*` |
|      - | 5676 | ` * Dump a hashmap instance and it's entries and the store the dump in` |
|      - | 5677 | ` * the BLOB given as the first argument.` |
|      - | 5678 | ` * This function is typically invoked when the user issue a call to` |
|      - | 5679 | ` * [var_dump(),var_export(),print_r(),...]` |
|      - | 5680 | ` * This function SXRET_OK on success. Any other return value including` |
|      - | 5681 | ` * SXERR_LIMIT(infinite recursion) indicates failure.` |
|      - | 5682 | ` */` |
|     28 | 5683 | `PH7_PRIVATE sxi32 PH7_HashmapDump(SyBlob *pOut,ph7_hashmap *pMap,int ShowType,int nTab,int nDepth)` |
|      2 | 5684 |  |
|      - | 5685 | `	ph7_hashmap_node *pEntry;` |
|      - | 5686 | `	ph7_value *pObj;` |
|     30 | 5687 | `	sxu32 n = 0;` |
|      - | 5688 | `	int isRef;` |
|      - | 5689 | `	sxi32 rc;` |
|      - | 5690 | `	int i;` |
|     30 | 5691 | `	if( nDepth > 31 ){` |
|      - | 5692 | `		static const char zInfinite[] = "Nesting limit reached: Infinite recursion?";` |
|      - | 5693 | `		/* Nesting limit reached */` |
|    ! 0 | 5694 | `		SyBlobAppend(&(*pOut),zInfinite,sizeof(zInfinite)-1);` |
|    ! 0 | 5695 | `		if( ShowType ){` |
|    ! 0 | 5696 | `			SyBlobAppend(&(*pOut),")",sizeof(char));` |
|    ! 0 | 5697 | `		}` |
|    ! 0 | 5698 | `		return SXERR_LIMIT;` |
|      - | 5699 | `	}` |
|      - | 5700 | `	/* Point to the first inserted entry */` |
|     30 | 5701 | `	pEntry = pMap->pFirst;` |
|     30 | 5702 | `	rc = SXRET_OK;` |
|     30 | 5703 | `	if( !ShowType ){` |
|     15 | 5704 | `		SyBlobAppend(&(*pOut),"Array(",sizeof("Array(")-1);` |
|      7 | 5705 | `	}` |
|      - | 5706 | `	/* Total entries */` |
|     30 | 5707 | `	SyBlobFormat(&(*pOut),"%u) {",pMap->nEntry);` |
|      - | 5708 | `#ifdef __WINNT__` |
|      2 | 5709 | `	SyBlobAppend(&(*pOut),"\r\n",sizeof("\r\n")-1);` |
|      - | 5710 | `#else` |
|     28 | 5711 | `	SyBlobAppend(&(*pOut),"\n",sizeof(char));` |
|      - | 5712 | `#endif` |
|     65 | 5713 | `	for(;;){` |
|    132 | 5714 | `		if( n >= pMap->nEntry ){` |
|     30 | 5715 | `			break;` |
|      - | 5716 | `		}` |
|    206 | 5717 | `		for( i = 0 ; i < nTab ; i++ ){` |
|    104 | 5718 | `			SyBlobAppend(&(*pOut)," ",sizeof(char));` |
|     53 | 5719 | `		}` |
|      - | 5720 | `		/* Dump key */` |
|    104 | 5721 | `		if( pEntry->iType == HASHMAP_INT_NODE){` |
|     37 | 5722 | `			SyBlobFormat(&(*pOut),"[%qd] =>",pEntry->xKey.iKey);` |
|     19 | 5723 | `		}else{` |
|    101 | 5724 | `			SyBlobFormat(&(*pOut),"[%.*s] =>",` |
|     33 | 5725 | `				SyBlobLength(&pEntry->xKey.sKey),SyBlobData(&pEntry->xKey.sKey));` |
|      - | 5726 | `		}` |
|      - | 5727 | `#ifdef __WINNT__` |
|      2 | 5728 | `		SyBlobAppend(&(*pOut),"\r\n",sizeof("\r\n")-1);` |
|      - | 5729 | `#else` |
|    102 | 5730 | `		SyBlobAppend(&(*pOut),"\n",sizeof(char));` |
|      - | 5731 | `#endif` |
|      - | 5732 | `		/* Dump node value */` |
|    104 | 5733 | `		pObj = HashmapExtractNodeValue(pEntry);` |
|    104 | 5734 | `		isRef = 0;` |
|    104 | 5735 | `		if( pObj ){` |
|    104 | 5736 | `			if( pEntry->iFlags & HASHMAP_NODE_FOREIGN_OBJ ){` |
|      - | 5737 | `				/* Referenced object */` |
|    ! 0 | 5738 | `				isRef = 1;` |
|    ! 0 | 5739 | `			}` |
|    104 | 5740 | `			rc = PH7_MemObjDump(&(*pOut),pObj,ShowType,nTab+1,nDepth,isRef);` |
|    104 | 5741 | `			if( rc == SXERR_LIMIT ){` |
|    ! 0 | 5742 | `				break;` |
|      - | 5743 | `			}` |
|     51 | 5744 | `		}` |
|      - | 5745 | `		/* Point to the next entry */` |
|    104 | 5746 | `		n++;` |
|    104 | 5747 | `		pEntry = pEntry->pPrev; /* Reverse link */` |
|      2 | 5748 | `	}` |
|     58 | 5749 | `	for( i = 0 ; i < nTab ; i++ ){` |
|     30 | 5750 | `		SyBlobAppend(&(*pOut)," ",sizeof(char));` |
|     16 | 5751 | `	}` |
|     30 | 5752 | `	SyBlobAppend(&(*pOut),"}",sizeof(char));` |
|     30 | 5753 | `	return rc;` |
|     16 | 5754 |  |
|      - | 5755 | `/*` |
|      - | 5756 | ` * Iterate throw hashmap entries and invoke the given callback [i.e: xWalk()] for each` |
|      - | 5757 | ` * retrieved entry.` |
|      - | 5758 | ` * Note that argument are passed to the callback by copy. That is,any modification to` |
|      - | 5759 | ` * the entry value in the callback body will not alter the real value.` |
|      - | 5760 | ` * If the callback wishes to abort processing [i.e: it's invocation] it must return` |
|      - | 5761 | ` * a value different from PH7_OK.` |
|      - | 5762 | ` * Refer to [ph7_array_walk()] for more information.` |
|      - | 5763 | ` */` |
|  16916 | 5764 | `PH7_PRIVATE sxi32 PH7_HashmapWalk(` |
|      - | 5765 | `	ph7_hashmap *pMap, /* Target hashmap */` |
|      - | 5766 | `	int (*xWalk)(ph7_value *,ph7_value *,void *), /* Walker callback */` |
|      - | 5767 | `	void *pUserData /* Last argument to xWalk() */` |
|      - | 5768 | `	)` |
|      2 | 5769 |  |
|      - | 5770 | `	ph7_hashmap_node *pEntry;` |
|      - | 5771 | `	ph7_value sKey,sValue;` |
|      - | 5772 | `	sxi32 rc;` |
|      - | 5773 | `	sxu32 n;` |
|      - | 5774 | `	/* Initialize walker parameter */` |
|  16918 | 5775 | `	rc = SXRET_OK;` |
|  16918 | 5776 | `	PH7_MemObjInit(pMap->pVm,&sKey);` |
|  16918 | 5777 | `	PH7_MemObjInit(pMap->pVm,&sValue);` |
|  16918 | 5778 | `	n = pMap->nEntry;` |
|  16918 | 5779 | `	pEntry = pMap->pFirst;` |
|      - | 5780 | `	/* Start the iteration process */` |
|  47751 | 5781 | `	for(;;){` |
|  95504 | 5782 | `		if( n < 1 ){` |
|  16918 | 5783 | `			break;` |
|      - | 5784 | `		}` |
|      - | 5785 | `		/* Extract a copy of the key and a copy the current value */` |
|  78588 | 5786 | `		PH7_HashmapExtractNodeKey(pEntry,&sKey);` |
|  78588 | 5787 | `		PH7_HashmapExtractNodeValue(pEntry,&sValue,FALSE);` |
|      - | 5788 | `		/* Invoke the user callback */` |
|  78588 | 5789 | `		rc = xWalk(&sKey,&sValue,pUserData);` |
|      - | 5790 | `		/* Release the copy of the key and the value */` |
|  78588 | 5791 | `		PH7_MemObjRelease(&sKey);` |
|  78588 | 5792 | `		PH7_MemObjRelease(&sValue);` |
|  78588 | 5793 | `		if( rc != PH7_OK ){` |
|      - | 5794 | `			/* Callback request an operation abort */` |
|    ! 0 | 5795 | `			return SXERR_ABORT;` |
|      - | 5796 | `		}` |
|      - | 5797 | `		/* Point to the next entry */` |
|  78588 | 5798 | `		pEntry = pEntry->pPrev; /* Reverse link */` |
|  78588 | 5799 | `		n--;` |
|      2 | 5800 | `	}` |
|      - | 5801 | `	/* All done */` |
|  16918 | 5802 | `	return SXRET_OK;` |
|   8460 | 5803 |  |
|      - | 5804 |  |
