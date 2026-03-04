# src/ph7/hashmap.c

<style>code, pre { background: none !important; white-space: pre !important; width: 100% !important; display: inline-block !important; } td { border: none !important; margin-top: 0 !important; margin-bottom: 0 !important; padding-top: 0 !important; padding-bottom: 0 !important; }</style>

Coverage: 2548/3099 lines (82.22%)

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
| 2715380 |   18 | `static sxu32 IntHash(sxi64 iKey)` |
|       2 |   19 |  |
| 2715382 |   20 | `	return (sxu32)(iKey ^ (iKey << 8) ^ (iKey >> 8));` |
|       2 |   21 |  |
|       - |   22 | `/*` |
|       - |   23 | ` * Default hash function for string/BLOB keys.` |
|       - |   24 | ` */` |
|  211450 |   25 | `static sxu32 BinHash(const void *pSrc,sxu32 nLen)` |
|       2 |   26 |  |
|  211452 |   27 | `	register unsigned char *zIn = (unsigned char *)pSrc;` |
|       - |   28 | `	unsigned char *zEnd;` |
|  211452 |   29 | `	sxu32 nH = 5381;` |
|  211452 |   30 | `	zEnd = &zIn[nLen];` |
|  244685 |   31 | `	for(;;){` |
|  489372 |   32 | `		if( zIn >= zEnd ){ break; } nH = nH * 33 + zIn[0] ; zIn++;` |
|  441438 |   33 | `		if( zIn >= zEnd ){ break; } nH = nH * 33 + zIn[0] ; zIn++;` |
|  400214 |   34 | `		if( zIn >= zEnd ){ break; } nH = nH * 33 + zIn[0] ; zIn++;` |
|  322688 |   35 | `		if( zIn >= zEnd ){ break; } nH = nH * 33 + zIn[0] ; zIn++;` |
|       2 |   36 | `	}` |
|  211452 |   37 | `	return nH;` |
|       2 |   38 |  |
|       - |   39 | `/*` |
|       - |   40 | ` * Return the total number of entries in a given hashmap.` |
|       - |   41 | ` * If bRecurisve is set to TRUE then recurse on hashmap entries.` |
|       - |   42 | ` * If the nesting limit is reached,this function abort immediately.` |
|       - |   43 | ` */` |
|     720 |   44 | `static sxi64 HashmapCount(ph7_hashmap *pMap,int bRecursive,int iRecCount)` |
|       2 |   45 |  |
|     722 |   46 | `	sxi64 iCount = 0;` |
|     722 |   47 | `	if( !bRecursive ){` |
|     446 |   48 | `		iCount = pMap->nEntry;` |
|     224 |   49 | `	}else{` |
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
|     718 |   79 | `	return iCount;` |
|     362 |   80 |  |
|       - |   81 | `/*` |
|       - |   82 | ` * Allocate a new hashmap node with a 64-bit integer key.` |
|       - |   83 | ` * If something goes wrong [i.e: out of memory],this function return NULL.` |
|       - |   84 | ` * Otherwise a fresh [ph7_hashmap_node] instance is returned.` |
|       - |   85 | ` */` |
| 2661982 |   86 | `static ph7_hashmap_node * HashmapNewIntNode(ph7_hashmap *pMap,sxi64 iKey,sxu32 nHash,sxu32 nValIdx)` |
|       2 |   87 |  |
|       - |   88 | `	ph7_hashmap_node *pNode;` |
|       - |   89 | `	/* Allocate a new node */` |
| 2661984 |   90 | `	pNode = (ph7_hashmap_node *)SyMemBackendPoolAlloc(&pMap->pVm->sAllocator,sizeof(ph7_hashmap_node));` |
| 2661984 |   91 | `	if( pNode == 0 ){` |
|     ! 0 |   92 | `		return 0;` |
|       - |   93 | `	}` |
|       - |   94 | `	/* Zero the stucture */` |
| 2661984 |   95 | `	SyZero(pNode,sizeof(ph7_hashmap_node));` |
|       - |   96 | `	/* Fill in the structure */` |
| 2661984 |   97 | `	pNode->pMap  = &(*pMap);` |
| 2661984 |   98 | `	pNode->iType = HASHMAP_INT_NODE;` |
| 2661984 |   99 | `	pNode->nHash = nHash;` |
| 2661984 |  100 | `	pNode->xKey.iKey = iKey;` |
| 2661984 |  101 | `	pNode->nValIdx  = nValIdx;` |
| 2661984 |  102 | `	return pNode;` |
| 1330993 |  103 |  |
|       - |  104 | `/*` |
|       - |  105 | ` * Allocate a new hashmap node with a BLOB key.` |
|       - |  106 | ` * If something goes wrong [i.e: out of memory],this function return NULL.` |
|       - |  107 | ` * Otherwise a fresh [ph7_hashmap_node] instance is returned.` |
|       - |  108 | ` */` |
|   73816 |  109 | `static ph7_hashmap_node * HashmapNewBlobNode(ph7_hashmap *pMap,const void *pKey,sxu32 nKeyLen,sxu32 nHash,sxu32 nValIdx)` |
|       2 |  110 |  |
|       - |  111 | `	ph7_hashmap_node *pNode;` |
|       - |  112 | `	/* Allocate a new node */` |
|   73818 |  113 | `	pNode = (ph7_hashmap_node *)SyMemBackendPoolAlloc(&pMap->pVm->sAllocator,sizeof(ph7_hashmap_node));` |
|   73818 |  114 | `	if( pNode == 0 ){` |
|     ! 0 |  115 | `		return 0;` |
|       - |  116 | `	}` |
|       - |  117 | `	/* Zero the stucture */` |
|   73818 |  118 | `	SyZero(pNode,sizeof(ph7_hashmap_node));` |
|       - |  119 | `	/* Fill in the structure */` |
|   73818 |  120 | `	pNode->pMap  = &(*pMap);` |
|   73818 |  121 | `	pNode->iType = HASHMAP_BLOB_NODE;` |
|   73818 |  122 | `	pNode->nHash = nHash;` |
|   73818 |  123 | `	SyBlobInit(&pNode->xKey.sKey,&pMap->pVm->sAllocator);` |
|   73818 |  124 | `	SyBlobAppend(&pNode->xKey.sKey,pKey,nKeyLen);` |
|   73818 |  125 | `	pNode->nValIdx = nValIdx;` |
|   73818 |  126 | `	return pNode;` |
|   36910 |  127 |  |
|       - |  128 | `/*` |
|       - |  129 | ` * link a hashmap node to the given bucket index (last argument to this function).` |
|       - |  130 | ` */` |
| 2735798 |  131 | `static void HashmapNodeLink(ph7_hashmap *pMap,ph7_hashmap_node *pNode,sxu32 nBucketIdx)` |
|       2 |  132 |  |
|       - |  133 | `	/* Link */` |
| 2735800 |  134 | `	if( pMap->apBucket[nBucketIdx] != 0 ){` |
| 2534408 |  135 | `		pNode->pNextCollide = pMap->apBucket[nBucketIdx];` |
| 2534408 |  136 | `		pMap->apBucket[nBucketIdx]->pPrevCollide = pNode;` |
| 1267203 |  137 | `	}` |
| 2735800 |  138 | `	pMap->apBucket[nBucketIdx] = pNode;` |
|       - |  139 | `	/* Link to the map list */` |
| 2735800 |  140 | `	if( pMap->pFirst == 0 ){` |
|   33134 |  141 | `		pMap->pFirst = pMap->pLast = pNode;` |
|       - |  142 | `		/* Point to the first inserted node */` |
|   33134 |  143 | `		pMap->pCur = pNode;` |
|   16568 |  144 | `	}else{` |
| 2702668 |  145 | `		MACRO_LD_PUSH(pMap->pLast,pNode);` |
|       - |  146 | `	}` |
| 2735800 |  147 | `	++pMap->nEntry;` |
| 2735800 |  148 |  |
|       - |  149 | `/*` |
|       - |  150 | ` * Unlink a node from the hashmap.` |
|       - |  151 | ` * If the node count reaches zero then release the whole hash-bucket.` |
|       - |  152 | ` */` |
|    5118 |  153 | `PH7_PRIVATE void PH7_HashmapUnlinkNode(ph7_hashmap_node *pNode,int bRestore)` |
|       2 |  154 |  |
|    5120 |  155 | `	ph7_hashmap *pMap = pNode->pMap;` |
|    5120 |  156 | `	ph7_vm *pVm = pMap->pVm;` |
|       - |  157 | `	/* Unlink from the corresponding bucket */` |
|    5120 |  158 | `	if( pNode->pPrevCollide == 0 ){` |
|    4694 |  159 | `		pMap->apBucket[pNode->nHash & (pMap->nSize - 1)] = pNode->pNextCollide;` |
|    2348 |  160 | `	}else{` |
|     427 |  161 | `		pNode->pPrevCollide->pNextCollide = pNode->pNextCollide;` |
|       - |  162 | `	}` |
|    5120 |  163 | `	if( pNode->pNextCollide ){` |
|    3917 |  164 | `		pNode->pNextCollide->pPrevCollide = pNode->pPrevCollide;` |
|    1958 |  165 | `	}` |
|    5120 |  166 | `	if( pMap->pFirst == pNode ){` |
|      58 |  167 | `		pMap->pFirst = pNode->pPrev;` |
|      28 |  168 | `	}` |
|    5120 |  169 | `	if( pMap->pCur == pNode ){` |
|       - |  170 | `		/* Advance the node cursor */` |
|      60 |  171 | `		pMap->pCur = pMap->pCur->pPrev; /* Reverse link */` |
|      29 |  172 | `	}` |
|       - |  173 | `	/* Unlink from the map list */` |
|    5120 |  174 | `	MACRO_LD_REMOVE(pMap->pLast,pNode);` |
|    5120 |  175 | `	if( bRestore ){` |
|       - |  176 | `		/* Remove the ph7_value associated with this node from the reference table */` |
|      30 |  177 | `		PH7_VmRefObjRemove(pVm,pNode->nValIdx,0,pNode);` |
|       - |  178 | `		/* Restore to the freelist */` |
|      30 |  179 | `		if( (pNode->iFlags & HASHMAP_NODE_FOREIGN_OBJ) == 0 ){` |
|      30 |  180 | `			PH7_VmUnsetMemObj(pVm,pNode->nValIdx,FALSE);` |
|      14 |  181 | `		}` |
|      14 |  182 | `	}` |
|    5120 |  183 | `	if( pNode->iType == HASHMAP_BLOB_NODE ){` |
|    5071 |  184 | `		SyBlobRelease(&pNode->xKey.sKey);` |
|    2535 |  185 | `	}` |
|    5120 |  186 | `	SyMemBackendPoolFree(&pVm->sAllocator,pNode);` |
|    5120 |  187 | `	pMap->nEntry--;` |
|    5120 |  188 | `	if( pMap->nEntry < 1 && pMap != pVm->pGlobal ){` |
|       - |  189 | `		/* Free the hash-bucket */` |
|      26 |  190 | `		SyMemBackendFree(&pVm->sAllocator,pMap->apBucket);` |
|      26 |  191 | `		pMap->apBucket = 0;` |
|      26 |  192 | `		pMap->nSize = 0;` |
|      26 |  193 | `		pMap->pFirst = pMap->pLast = pMap->pCur = 0;` |
|      12 |  194 | `	}` |
|    5120 |  195 |  |
|       - |  196 | `#define HASHMAP_FILL_FACTOR 3` |
|       - |  197 | `/*` |
|       - |  198 | ` * Grow the hash-table and rehash all entries.` |
|       - |  199 | ` */` |
| 2735798 |  200 | `static sxi32 HashmapGrowBucket(ph7_hashmap *pMap)` |
|       2 |  201 |  |
| 2735800 |  202 | `	if( pMap->nEntry >= pMap->nSize * HASHMAP_FILL_FACTOR ){` |
|   36478 |  203 | `		ph7_hashmap_node **apOld = pMap->apBucket;` |
|       - |  204 | `		ph7_hashmap_node *pEntry,**apNew;` |
|   36478 |  205 | `		sxu32 nNew = pMap->nSize << 1;` |
|       - |  206 | `		sxu32 nBucket;` |
|       - |  207 | `		sxu32 n;` |
|   36478 |  208 | `		if( nNew < 1 ){` |
|   33134 |  209 | `			nNew = 16;` |
|   16566 |  210 | `		}` |
|       - |  211 | `		/* Allocate a new bucket */` |
|   36478 |  212 | `		apNew = (ph7_hashmap_node **)SyMemBackendAlloc(&pMap->pVm->sAllocator,nNew * sizeof(ph7_hashmap_node *));` |
|   36478 |  213 | `		if( apNew == 0 ){` |
|     ! 0 |  214 | `			if( pMap->nSize < 1 ){` |
|     ! 0 |  215 | `				return SXERR_MEM; /* Fatal */` |
|       - |  216 | `			}` |
|       - |  217 | `			/* Not so fatal here,simply a performance hit */` |
|     ! 0 |  218 | `			return SXRET_OK;` |
|       - |  219 | `		}` |
|       - |  220 | `		/* Zero the table */` |
|   36478 |  221 | `		SyZero((void *)apNew,nNew * sizeof(ph7_hashmap_node *));` |
|       - |  222 | `		/* Reflect the change */` |
|   36478 |  223 | `		pMap->apBucket = apNew;` |
|   36478 |  224 | `		pMap->nSize = nNew;` |
|   36478 |  225 | `		if( apOld == 0 ){` |
|       - |  226 | `			/* First allocated table [i.e: no entry],return immediately */` |
|   33134 |  227 | `			return SXRET_OK;` |
|       - |  228 | `		}` |
|       - |  229 | `		/* Rehash old entries */` |
|    3346 |  230 | `		pEntry = pMap->pFirst;` |
|    3346 |  231 | `		n = 0;` |
| 1871992 |  232 | `		for( ;; ){` |
| 3743986 |  233 | `			if( n >= pMap->nEntry ){` |
|    3346 |  234 | `				break;` |
|       - |  235 | `			}` |
|       - |  236 | `			/* Clear the old collision link */` |
| 3740642 |  237 | `			pEntry->pNextCollide = pEntry->pPrevCollide = 0;` |
|       - |  238 | `			/* Link to the new bucket */` |
| 3740642 |  239 | `			nBucket = pEntry->nHash & (nNew - 1);` |
| 3740642 |  240 | `			if( pMap->apBucket[nBucket] != 0 ){` |
| 3344964 |  241 | `				pEntry->pNextCollide = pMap->apBucket[nBucket];` |
| 3344964 |  242 | `				pMap->apBucket[nBucket]->pPrevCollide = pEntry;` |
| 1672481 |  243 | `			}` |
| 3740642 |  244 | `			pMap->apBucket[nBucket] = pEntry;` |
|       - |  245 | `			/* Point to the next entry */` |
| 3740642 |  246 | `			pEntry = pEntry->pPrev; /* Reverse link */` |
| 3740642 |  247 | `			n++;` |
|       2 |  248 | `		}` |
|       - |  249 | `		/* Free the old table */` |
|    3346 |  250 | `		SyMemBackendFree(&pMap->pVm->sAllocator,(void *)apOld);` |
|    1672 |  251 | `	}` |
| 2702668 |  252 | `	return SXRET_OK;` |
| 1367901 |  253 |  |
|       - |  254 | `/*` |
|       - |  255 | ` * Insert a 64-bit integer key and it's associated value (if any) in the given` |
|       - |  256 | ` * hashmap.` |
|       - |  257 | ` */` |
| 2661982 |  258 | `static sxi32 HashmapInsertIntKey(ph7_hashmap *pMap,sxi64 iKey,ph7_value *pValue,sxu32 nRefIdx,int isForeign)` |
|       2 |  259 |  |
|       - |  260 | `	ph7_hashmap_node *pNode;` |
|       - |  261 | `	sxu32 nIdx;` |
|       - |  262 | `	sxu32 nHash;` |
|       - |  263 | `	sxi32 rc;` |
| 2661984 |  264 | `	if( !isForeign ){` |
|       - |  265 | `		ph7_value *pObj;` |
|       - |  266 | `		/* Reserve a ph7_value for the value */` |
| 2661960 |  267 | `		pObj = PH7_ReserveMemObj(pMap->pVm);` |
| 2661960 |  268 | `		if( pObj == 0 ){` |
|     ! 0 |  269 | `			return SXERR_MEM;` |
|       - |  270 | `		}` |
| 2661960 |  271 | `		if( pValue ){` |
|       - |  272 | `			/* Duplicate the value */` |
| 2661960 |  273 | `			PH7_MemObjStore(pValue,pObj);` |
| 1330979 |  274 | `		}` |
| 2661960 |  275 | `		nIdx = pObj->nIdx;` |
| 1330981 |  276 | `	}else{` |
|      25 |  277 | `		nIdx = nRefIdx;` |
|       - |  278 | `	}` |
|       - |  279 | `	/* Hash the key */` |
| 2661984 |  280 | `	nHash = pMap->xIntHash(iKey);` |
|       - |  281 | `	/* Allocate a new int node */` |
| 2661984 |  282 | `	pNode = HashmapNewIntNode(&(*pMap),iKey,nHash,nIdx);` |
| 2661984 |  283 | `	if( pNode == 0 ){` |
|     ! 0 |  284 | `		return SXERR_MEM;` |
|       - |  285 | `	}` |
| 2661984 |  286 | `	if( isForeign ){` |
|       - |  287 | `		/* Mark as a foregin entry */` |
|      25 |  288 | `		pNode->iFlags \|= HASHMAP_NODE_FOREIGN_OBJ;` |
|      12 |  289 | `	}` |
|       - |  290 | `	/* Make sure the bucket is big enough to hold the new entry */` |
| 2661984 |  291 | `	rc = HashmapGrowBucket(&(*pMap));` |
| 2661984 |  292 | `	if( rc != SXRET_OK ){` |
|     ! 0 |  293 | `		SyMemBackendPoolFree(&pMap->pVm->sAllocator,pNode);` |
|     ! 0 |  294 | `		return rc;` |
|       - |  295 | `	}` |
|       - |  296 | `	/* Perform the insertion */` |
| 2661984 |  297 | `	HashmapNodeLink(&(*pMap),pNode,nHash & (pMap->nSize - 1));` |
|       - |  298 | `	/* Install in the reference table */` |
| 2661984 |  299 | `	PH7_VmRefObjInstall(pMap->pVm,nIdx,0,pNode,0);` |
|       - |  300 | `	/* All done */` |
| 2661984 |  301 | `	return SXRET_OK;` |
| 1330993 |  302 |  |
|       - |  303 | `/*` |
|       - |  304 | ` * Insert a BLOB key and it's associated value (if any) in the given` |
|       - |  305 | ` * hashmap.` |
|       - |  306 | ` */` |
|   73816 |  307 | `static sxi32 HashmapInsertBlobKey(ph7_hashmap *pMap,const void *pKey,sxu32 nKeyLen,ph7_value *pValue,sxu32 nRefIdx,int isForeign)` |
|       2 |  308 |  |
|       - |  309 | `	ph7_hashmap_node *pNode;` |
|       - |  310 | `	sxu32 nHash;` |
|       - |  311 | `	sxu32 nIdx;` |
|       - |  312 | `	sxi32 rc;` |
|   73818 |  313 | `	if( !isForeign ){` |
|       - |  314 | `		ph7_value *pObj;` |
|       - |  315 | `		/* Reserve a ph7_value for the value */` |
|   56090 |  316 | `		pObj = PH7_ReserveMemObj(pMap->pVm);` |
|   56090 |  317 | `		if( pObj == 0 ){` |
|     ! 0 |  318 | `			return SXERR_MEM;` |
|       - |  319 | `		}` |
|   56090 |  320 | `		if( pValue ){` |
|       - |  321 | `			/* Duplicate the value */` |
|   56090 |  322 | `			PH7_MemObjStore(pValue,pObj);` |
|   28044 |  323 | `		}` |
|   56090 |  324 | `		nIdx = pObj->nIdx;` |
|   28046 |  325 | `	}else{` |
|   17730 |  326 | `		nIdx = nRefIdx;` |
|       - |  327 | `	}` |
|       - |  328 | `	/* Hash the key */` |
|   73818 |  329 | `	nHash = pMap->xBlobHash(pKey,nKeyLen);` |
|       - |  330 | `	/* Allocate a new blob node */` |
|   73818 |  331 | `	pNode = HashmapNewBlobNode(&(*pMap),pKey,nKeyLen,nHash,nIdx);` |
|   73818 |  332 | `	if( pNode == 0 ){` |
|     ! 0 |  333 | `		return SXERR_MEM;` |
|       - |  334 | `	}` |
|   73818 |  335 | `	if( isForeign ){` |
|       - |  336 | `		/* Mark as a foregin entry */` |
|   17730 |  337 | `		pNode->iFlags \|= HASHMAP_NODE_FOREIGN_OBJ;` |
|    8864 |  338 | `	}` |
|       - |  339 | `	/* Make sure the bucket is big enough to hold the new entry */` |
|   73818 |  340 | `	rc = HashmapGrowBucket(&(*pMap));` |
|   73818 |  341 | `	if( rc != SXRET_OK ){` |
|     ! 0 |  342 | `		SyMemBackendPoolFree(&pMap->pVm->sAllocator,pNode);` |
|     ! 0 |  343 | `		return rc;` |
|       - |  344 | `	}` |
|       - |  345 | `	/* Perform the insertion */` |
|   73818 |  346 | `	HashmapNodeLink(&(*pMap),pNode,nHash & (pMap->nSize - 1));` |
|       - |  347 | `	/* Install in the reference table */` |
|   73818 |  348 | `	PH7_VmRefObjInstall(pMap->pVm,nIdx,0,pNode,0);` |
|       - |  349 | `	/* All done */` |
|   73818 |  350 | `	return SXRET_OK;` |
|   36910 |  351 |  |
|       - |  352 | `/*` |
|       - |  353 | ` * Check if a given 64-bit integer key exists in the given hashmap.` |
|       - |  354 | ` * Write a pointer to the target node on success. Otherwise` |
|       - |  355 | ` * SXERR_NOTFOUND is returned on failure.` |
|       - |  356 | ` */` |
|   46396 |  357 | `static sxi32 HashmapLookupIntKey(` |
|       - |  358 | `	ph7_hashmap *pMap,         /* Target hashmap */` |
|       - |  359 | `	sxi64 iKey,                /* lookup key */` |
|       - |  360 | `	ph7_hashmap_node **ppNode  /* OUT: target node on success */` |
|       - |  361 | `	)` |
|       2 |  362 |  |
|       - |  363 | `	ph7_hashmap_node *pNode;` |
|       - |  364 | `	sxu32 nHash;` |
|   46398 |  365 | `	if( pMap->nEntry < 1 ){` |
|       - |  366 | `		/* Don't bother hashing,there is no entry anyway */` |
|     335 |  367 | `		return SXERR_NOTFOUND;` |
|       - |  368 | `	}` |
|       - |  369 | `	/* Hash the key first */` |
|   46064 |  370 | `	nHash = pMap->xIntHash(iKey);` |
|       - |  371 | `	/* Point to the appropriate bucket */` |
|   46064 |  372 | `	pNode = pMap->apBucket[nHash & (pMap->nSize - 1)];` |
|       - |  373 | `	/* Perform the lookup */` |
|  411388 |  374 | `	for(;;){` |
|  822778 |  375 | `		if( pNode == 0 ){` |
|   45631 |  376 | `			break;` |
|       - |  377 | `		}` |
|  777362 |  378 | `		if( pNode->iType == HASHMAP_INT_NODE` |
|  774128 |  379 | `			&& pNode->nHash == nHash` |
|  385773 |  380 | `			&& pNode->xKey.iKey == iKey ){` |
|       - |  381 | `				/* Node found */` |
|     434 |  382 | `				if( ppNode ){` |
|     426 |  383 | `					*ppNode = pNode;` |
|     212 |  384 | `				}` |
|     434 |  385 | `				return SXRET_OK;` |
|       - |  386 | `		}` |
|       - |  387 | `		/* Follow the collision link */` |
|  776715 |  388 | `		pNode = pNode->pNextCollide;` |
|       1 |  389 | `	}` |
|       - |  390 | `	/* No such entry */` |
|   45631 |  391 | `	return SXERR_NOTFOUND;` |
|   23200 |  392 |  |
|       - |  393 | `/*` |
|       - |  394 | ` * Check if a given BLOB key exists in the given hashmap.` |
|       - |  395 | ` * Write a pointer to the target node on success. Otherwise` |
|       - |  396 | ` * SXERR_NOTFOUND is returned on failure.` |
|       - |  397 | ` */` |
|  144892 |  398 | `static sxi32 HashmapLookupBlobKey(` |
|       - |  399 | `	ph7_hashmap *pMap,          /* Target hashmap */` |
|       - |  400 | `	const void *pKey,           /* Lookup key */` |
|       - |  401 | `	sxu32 nKeyLen,              /* Key length in bytes */` |
|       - |  402 | `	ph7_hashmap_node **ppNode   /* OUT: target node on success */` |
|       - |  403 | `	)` |
|       2 |  404 |  |
|       - |  405 | `	ph7_hashmap_node *pNode;` |
|       - |  406 | `	sxu32 nHash;` |
|  144894 |  407 | `	if( pMap->nEntry < 1 ){` |
|       - |  408 | `		/* Don't bother hashing,there is no entry anyway */` |
|    7260 |  409 | `		return SXERR_NOTFOUND;` |
|       - |  410 | `	}` |
|       - |  411 | `	/* Hash the key first */` |
|  137636 |  412 | `	nHash = pMap->xBlobHash(pKey,nKeyLen);` |
|       - |  413 | `	/* Point to the appropriate bucket */` |
|  137636 |  414 | `	pNode = pMap->apBucket[nHash & (pMap->nSize - 1)];` |
|       - |  415 | `	/* Perform the lookup */` |
|  140474 |  416 | `	for(;;){` |
|  280950 |  417 | `		if( pNode == 0 ){` |
|  104066 |  418 | `			break;` |
|       - |  419 | `		}` |
|  193669 |  420 | `		if( pNode->iType == HASHMAP_BLOB_NODE` |
|  175384 |  421 | `			&& pNode->nHash == nHash` |
|  103727 |  422 | `			&& SyBlobLength(&pNode->xKey.sKey) == nKeyLen` |
|   33572 |  423 | `			&& SyMemcmp(SyBlobData(&pNode->xKey.sKey),pKey,nKeyLen) == 0 ){` |
|       - |  424 | `				/* Node found */` |
|   33572 |  425 | `				if( ppNode ){` |
|   33556 |  426 | `					*ppNode = pNode;` |
|   16777 |  427 | `				}` |
|   33572 |  428 | `				return SXRET_OK;` |
|       - |  429 | `		}` |
|       - |  430 | `		/* Follow the collision link */` |
|  143316 |  431 | `		pNode = pNode->pNextCollide;` |
|       2 |  432 | `	}` |
|       - |  433 | `	/* No such entry */` |
|  104066 |  434 | `	return SXERR_NOTFOUND;` |
|   72448 |  435 |  |
|       - |  436 | `/*` |
|       - |  437 | ` * Check if the given BLOB key looks like a decimal number.` |
|       - |  438 | ` * Retrurn TRUE on success.FALSE otherwise.` |
|       - |  439 | ` */` |
|  145070 |  440 | `static int HashmapIsIntKey(SyBlob *pKey)` |
|       2 |  441 |  |
|  145072 |  442 | `	const char *zIn  = (const char *)SyBlobData(pKey);` |
|  145072 |  443 | `	const char *zEnd = &zIn[SyBlobLength(pKey)];` |
|  145072 |  444 | `	if( (int)(zEnd-zIn) > 1 && zIn[0] == '0' ){` |
|       - |  445 | `		/* Octal not decimal number */` |
|       5 |  446 | `		return FALSE;` |
|       - |  447 | `	}` |
|  145068 |  448 | `	if( (zIn[0] == '-' \|\| zIn[0] == '+') && &zIn[1] < zEnd ){` |
|     ! 0 |  449 | `		zIn++;` |
|     ! 0 |  450 | `	}` |
|   72866 |  451 | `	for(;;){` |
|  145734 |  452 | `		if( zIn >= zEnd ){` |
|     233 |  453 | `			return TRUE;` |
|       - |  454 | `		}` |
|  145502 |  455 | `		if( (unsigned char)zIn[0] >= 0xc0 /* UTF-8 stream */  \|\| !SyisDigit(zIn[0]) ){` |
|   72419 |  456 | `			break;` |
|       - |  457 | `		}` |
|     667 |  458 | `		zIn++;` |
|       1 |  459 | `	}` |
|       - |  460 | `	/* Key does not look like a decimal number */` |
|  144836 |  461 | `	return FALSE;` |
|   72537 |  462 |  |
|       - |  463 | `/*` |
|       - |  464 | ` * Check if a given key exists in the given hashmap.` |
|       - |  465 | ` * Write a pointer to the target node on success.` |
|       - |  466 | ` * Otherwise SXERR_NOTFOUND is returned on failure.` |
|       - |  467 | ` */` |
|   71456 |  468 | `static sxi32 HashmapLookup(` |
|       - |  469 | `	ph7_hashmap *pMap,          /* Target hashmap */` |
|       - |  470 | `	ph7_value *pKey,            /* Lookup key */` |
|       - |  471 | `	ph7_hashmap_node **ppNode   /* OUT: target node on success */` |
|       - |  472 | `	)` |
|       2 |  473 |  |
|   71458 |  474 | `	ph7_hashmap_node *pNode = 0; /* cc -O6 warning */` |
|       - |  475 | `	sxi32 rc;` |
|   71458 |  476 | `	if( pKey->iFlags & (MEMOBJ_STRING\|MEMOBJ_HASHMAP\|MEMOBJ_OBJ\|MEMOBJ_RES) ){` |
|   71090 |  477 | `		if( (pKey->iFlags & MEMOBJ_STRING) == 0 ){` |
|       - |  478 | `			/* Force a string cast */` |
|     ! 0 |  479 | `			PH7_MemObjToString(&(*pKey));` |
|     ! 0 |  480 | `		}` |
|   71090 |  481 | `		if( SyBlobLength(&pKey->sBlob) > 0 && !HashmapIsIntKey(&pKey->sBlob) ){` |
|       - |  482 | `			/* Perform a blob lookup */` |
|   71074 |  483 | `			rc = HashmapLookupBlobKey(&(*pMap),SyBlobData(&pKey->sBlob),SyBlobLength(&pKey->sBlob),&pNode);` |
|   71074 |  484 | `			goto result;` |
|       - |  485 | `		}` |
|       8 |  486 | `	}` |
|       - |  487 | `	/* Perform an int lookup */` |
|     386 |  488 | `	if((pKey->iFlags & MEMOBJ_INT) == 0 ){` |
|       - |  489 | `		/* Force an integer cast */` |
|      27 |  490 | `		PH7_MemObjToInteger(pKey);` |
|      13 |  491 | `	}` |
|       - |  492 | `	/* Perform an int lookup */` |
|     386 |  493 | `	rc = HashmapLookupIntKey(&(*pMap),pKey->x.iVal,&pNode);` |
|   35728 |  494 | `result:` |
|   71458 |  495 | `	if( rc == SXRET_OK ){` |
|       - |  496 | `		/* Node found */` |
|   33888 |  497 | `		if( ppNode ){` |
|   33864 |  498 | `			*ppNode = pNode;` |
|   16931 |  499 | `		}` |
|   33888 |  500 | `		return SXRET_OK;` |
|       - |  501 | `	}` |
|       - |  502 | `	/* No such entry */` |
|   37572 |  503 | `	return SXERR_NOTFOUND;` |
|   35730 |  504 |  |
|       - |  505 | `/*` |
|       - |  506 | ` * Insert a given key and it's associated value (if any) in the given` |
|       - |  507 | ` * hashmap.` |
|       - |  508 | ` * If a node with the given key already exists in the database` |
|       - |  509 | ` * then this function overwrite the old value.` |
|       - |  510 | ` */` |
| 2717976 |  511 | `static sxi32 HashmapInsert(` |
|       - |  512 | `	ph7_hashmap *pMap, /* Target hashmap */` |
|       - |  513 | `	ph7_value *pKey,   /* Lookup key  */` |
|       - |  514 | `	ph7_value *pVal    /* Node value */` |
|       - |  515 | `	)` |
|       2 |  516 |  |
| 2717978 |  517 | `	ph7_hashmap_node *pNode = 0;` |
| 2717978 |  518 | `	sxi32 rc = SXRET_OK;` |
| 2717978 |  519 | `	if( pKey && pKey->iFlags & (MEMOBJ_STRING\|MEMOBJ_HASHMAP\|MEMOBJ_OBJ\|MEMOBJ_RES) ){` |
|   56288 |  520 | `		if( (pKey->iFlags & MEMOBJ_STRING) == 0 ){` |
|       - |  521 | `			/* Force a string cast */` |
|       3 |  522 | `			PH7_MemObjToString(&(*pKey));` |
|       1 |  523 | `		}` |
|   56288 |  524 | `		if( SyBlobLength(&pKey->sBlob) < 1 \|\| HashmapIsIntKey(&pKey->sBlob) ){` |
|     256 |  525 | `			if(SyBlobLength(&pKey->sBlob) < 1){` |
|       - |  526 | `				/* Automatic index assign */` |
|      34 |  527 | `				pKey = 0;` |
|      16 |  528 | `			}` |
|     256 |  529 | `			goto IntKey;` |
|       - |  530 | `		}` |
|   84050 |  531 | `		if( SXRET_OK == HashmapLookupBlobKey(&(*pMap),SyBlobData(&pKey->sBlob),` |
|   28016 |  532 | `			SyBlobLength(&pKey->sBlob),&pNode) ){` |
|       - |  533 | `				/* Overwrite the old value */` |
|       - |  534 | `				ph7_value *pElem;` |
|      23 |  535 | `				pElem = (ph7_value *)SySetAt(&pMap->pVm->aMemObj,pNode->nValIdx);` |
|      23 |  536 | `				if( pElem ){` |
|      23 |  537 | `					if( pVal ){` |
|      23 |  538 | `						PH7_MemObjStore(pVal,pElem);` |
|      12 |  539 | `					}else{` |
|       - |  540 | `						/* Nullify the entry */` |
|     ! 0 |  541 | `						PH7_MemObjToNull(pElem);` |
|       - |  542 | `					}` |
|      11 |  543 | `				}` |
|      23 |  544 | `				return SXRET_OK;` |
|       - |  545 | `		}` |
|   56012 |  546 | `		if( pMap == pMap->pVm->pGlobal ){` |
|       - |  547 | `			/* Forbidden */` |
|       3 |  548 | `			PH7_VmThrowError(pMap->pVm,0,PH7_CTX_NOTICE,"$GLOBALS is a read-only array,insertion is forbidden");` |
|       3 |  549 | `			return SXRET_OK;` |
|       - |  550 | `		}` |
|       - |  551 | `		/* Perform a blob-key insertion */` |
|   56010 |  552 | `		rc = HashmapInsertBlobKey(&(*pMap),SyBlobData(&pKey->sBlob),SyBlobLength(&pKey->sBlob),&(*pVal),0,FALSE);` |
|   56010 |  553 | `		return rc;` |
|       - |  554 | `	}` |
| 1330845 |  555 | `IntKey:` |
| 2661946 |  556 | `	if( pKey ){` |
|   23121 |  557 | `		if((pKey->iFlags & MEMOBJ_INT) == 0 ){` |
|       - |  558 | `			/* Force an integer cast */` |
|     251 |  559 | `			PH7_MemObjToInteger(pKey);` |
|     125 |  560 | `		}` |
|   23121 |  561 | `		if( SXRET_OK == HashmapLookupIntKey(&(*pMap),pKey->x.iVal,&pNode) ){` |
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
|   23085 |  575 | `		if( pMap == pMap->pVm->pGlobal ){` |
|       - |  576 | `			/* Forbidden */` |
|       3 |  577 | `			PH7_VmThrowError(pMap->pVm,0,PH7_CTX_NOTICE,"$GLOBALS is a read-only array,insertion is forbidden");` |
|       3 |  578 | `			return SXRET_OK;` |
|       - |  579 | `		}` |
|       - |  580 | `		/* Perform a 64-bit-int-key insertion */` |
|   23083 |  581 | `		rc = HashmapInsertIntKey(&(*pMap),pKey->x.iVal,&(*pVal),0,FALSE);` |
|   23083 |  582 | `		if( rc == SXRET_OK ){` |
|   23083 |  583 | `			if( pKey->x.iVal >= pMap->iNextIdx ){` |
|       - |  584 | `				/* Increment the automatic index */` |
|   22855 |  585 | `				pMap->iNextIdx = pKey->x.iVal + 1;` |
|       - |  586 | `				/* Make sure the automatic index is not reserved */` |
|   22855 |  587 | `				while( SXRET_OK == HashmapLookupIntKey(&(*pMap),pMap->iNextIdx,0) ){` |
|     ! 0 |  588 | `					pMap->iNextIdx++;` |
|     ! 0 |  589 | `				}` |
|   11427 |  590 | `			}` |
|   11541 |  591 | `		}` |
|   11542 |  592 | `	}else{` |
| 2638826 |  593 | `		if( pMap == pMap->pVm->pGlobal ){` |
|       - |  594 | `			/* Forbidden */` |
|       3 |  595 | `			PH7_VmThrowError(pMap->pVm,0,PH7_CTX_NOTICE,"$GLOBALS is a read-only array,insertion is forbidden");` |
|       3 |  596 | `			return SXRET_OK;` |
|       - |  597 | `		}` |
|       - |  598 | `		/* Assign an automatic index */` |
| 2638824 |  599 | `		rc = HashmapInsertIntKey(&(*pMap),pMap->iNextIdx,&(*pVal),0,FALSE);` |
| 2638824 |  600 | `		if( rc == SXRET_OK ){` |
| 2638824 |  601 | `			++pMap->iNextIdx;` |
| 1319411 |  602 | `		}` |
|       - |  603 | `	}` |
|       - |  604 | `	/* Insertion result */` |
| 2661906 |  605 | `	return rc;` |
| 1358990 |  606 |  |
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
|   17758 |  634 | `static sxi32 HashmapInsertByRef(` |
|       - |  635 | `	ph7_hashmap *pMap,   /* Target hashmap */` |
|       - |  636 | `	ph7_value *pKey,     /* Lookup key */` |
|       - |  637 | `	sxu32 nRefIdx        /* Foreign ph7_value index */` |
|       - |  638 | `	)` |
|       2 |  639 |  |
|   17760 |  640 | `	ph7_hashmap_node *pNode = 0;` |
|   17760 |  641 | `	sxi32 rc = SXRET_OK;` |
|   17760 |  642 | `	if( pKey && pKey->iFlags & (MEMOBJ_STRING\|MEMOBJ_HASHMAP\|MEMOBJ_OBJ\|MEMOBJ_RES) ){` |
|   17736 |  643 | `		if( (pKey->iFlags & MEMOBJ_STRING) == 0 ){` |
|       - |  644 | `			/* Force a string cast */` |
|     ! 0 |  645 | `			PH7_MemObjToString(&(*pKey));` |
|     ! 0 |  646 | `		}` |
|   17736 |  647 | `		if( SyBlobLength(&pKey->sBlob) < 1 \|\| HashmapIsIntKey(&pKey->sBlob) ){` |
|     ! 0 |  648 | `			if(SyBlobLength(&pKey->sBlob) < 1){` |
|       - |  649 | `				/* Automatic index assign */` |
|     ! 0 |  650 | `				pKey = 0;` |
|     ! 0 |  651 | `			}` |
|     ! 0 |  652 | `			goto IntKey;` |
|       - |  653 | `		}` |
|   26603 |  654 | `		if( SXRET_OK == HashmapLookupBlobKey(&(*pMap),SyBlobData(&pKey->sBlob),` |
|    8867 |  655 | `			SyBlobLength(&pKey->sBlob),&pNode) ){` |
|       - |  656 | `				/* Overwrite */` |
|       7 |  657 | `				PH7_VmRefObjRemove(pMap->pVm,pNode->nValIdx,0,pNode);` |
|       7 |  658 | `				pNode->nValIdx = nRefIdx;` |
|       - |  659 | `				/* Install in the reference table */` |
|       7 |  660 | `				PH7_VmRefObjInstall(pMap->pVm,nRefIdx,0,pNode,0);` |
|       7 |  661 | `				return SXRET_OK;` |
|       - |  662 | `		}` |
|       - |  663 | `		/* Perform a blob-key insertion */` |
|   17730 |  664 | `		rc = HashmapInsertBlobKey(&(*pMap),SyBlobData(&pKey->sBlob),SyBlobLength(&pKey->sBlob),0,nRefIdx,TRUE);` |
|   17730 |  665 | `		return rc;` |
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
|    8881 |  702 |  |
|       - |  703 | `/*` |
|       - |  704 | ` * Extract node value.` |
|       - |  705 | ` */` |
|  759320 |  706 | `static ph7_value * HashmapExtractNodeValue(ph7_hashmap_node *pNode)` |
|       2 |  707 |  |
|       - |  708 | `	/* Point to the desired object */` |
|       - |  709 | `	ph7_value *pObj;` |
|  759322 |  710 | `	pObj = (ph7_value *)SySetAt(&pNode->pMap->pVm->aMemObj,pNode->nValIdx);` |
|  759322 |  711 | `	return pObj;` |
|       2 |  712 |  |
|       - |  713 | `/*` |
|       - |  714 | ` * Insert a node in the given hashmap.` |
|       - |  715 | ` * If a node with the given key already exists in the database` |
|       - |  716 | ` * then this function overwrite the old value.` |
|       - |  717 | ` */` |
|     198 |  718 | `static sxi32 HashmapInsertNode(ph7_hashmap *pMap,ph7_hashmap_node *pNode,int bPreserve)` |
|       1 |  719 |  |
|       - |  720 | `	ph7_value *pObj;` |
|       - |  721 | `	sxi32 rc;` |
|       - |  722 | `	/* Extract the node value */` |
|     199 |  723 | `	pObj = HashmapExtractNodeValue(&(*pNode));` |
|     199 |  724 | `	if( pObj == 0 ){` |
|     ! 0 |  725 | `		return SXERR_EMPTY;` |
|       - |  726 | `	}` |
|       - |  727 | `	/* Preserve key */` |
|     199 |  728 | `	if( pNode->iType == HASHMAP_INT_NODE){` |
|       - |  729 | `		/* Int64 key */` |
|      89 |  730 | `		if( !bPreserve ){` |
|       - |  731 | `			/* Assign an automatic index */` |
|      47 |  732 | `			rc = HashmapInsert(&(*pMap),0,pObj);` |
|      24 |  733 | `		}else{` |
|      43 |  734 | `			rc = HashmapInsertIntKey(&(*pMap),pNode->xKey.iKey,pObj,0,FALSE);` |
|       - |  735 | `		}` |
|      45 |  736 | `	}else{` |
|       - |  737 | `		/* Blob key */` |
|     111 |  738 | `		if( !bPreserve ){` |
|       - |  739 | `			/* treat it like an automatically-indexed element, drop the` |
|       - |  740 | `			 * original string key entirely */` |
|      33 |  741 | `			rc = HashmapInsert(&(*pMap),0,pObj);` |
|      17 |  742 | `		}else{` |
|     118 |  743 | `			rc = HashmapInsertBlobKey(&(*pMap),SyBlobData(&pNode->xKey.sKey),` |
|      39 |  744 | `				SyBlobLength(&pNode->xKey.sKey),pObj,0,FALSE);` |
|       - |  745 | `		}` |
|       - |  746 | `	}` |
|     199 |  747 | `	return rc;` |
|     100 |  748 |  |
|       - |  749 | `/*` |
|       - |  750 | ` * Compare two node values.` |
|       - |  751 | ` * Return 0 if the node values are equals, > 0 if pLeft is greater than pRight` |
|       - |  752 | ` * or < 0 if pRight is greater than pLeft.` |
|       - |  753 | ` * For a full description on ph7_values comparison,refer to the implementation` |
|       - |  754 | ` * of the [PH7_MemObjCmp()] function defined in memobj.c or the official` |
|       - |  755 | ` * documenation.` |
|       - |  756 | ` */` |
|   33971 |  757 | `static sxi32 HashmapNodeCmp(ph7_hashmap_node *pLeft,ph7_hashmap_node *pRight,int bStrict)` |
|       2 |  758 |  |
|       - |  759 | `	ph7_value sObj1,sObj2;` |
|       - |  760 | `	sxi32 rc;` |
|   33973 |  761 | `	if( pLeft == pRight ){` |
|       - |  762 | `		/*` |
|       - |  763 | `		 * Same node.Refer to the sort() implementation defined` |
|       - |  764 | `		 * below for more information on this sceanario.` |
|       - |  765 | `		 */` |
|     ! 0 |  766 | `		return 0;` |
|       - |  767 | `	}` |
|       - |  768 | `	/* Do the comparison */` |
|   33973 |  769 | `	PH7_MemObjInit(pLeft->pMap->pVm,&sObj1);` |
|   33973 |  770 | `	PH7_MemObjInit(pLeft->pMap->pVm,&sObj2);` |
|   33973 |  771 | `	PH7_HashmapExtractNodeValue(pLeft,&sObj1,FALSE);` |
|   33973 |  772 | `	PH7_HashmapExtractNodeValue(pRight,&sObj2,FALSE);` |
|   33973 |  773 | `	rc = PH7_MemObjCmp(&sObj1,&sObj2,bStrict,0);` |
|   33973 |  774 | `	PH7_MemObjRelease(&sObj1);` |
|   33973 |  775 | `	PH7_MemObjRelease(&sObj2);` |
|   33973 |  776 | `	return rc;` |
|   17016 |  777 |  |
|       - |  778 | `/*` |
|       - |  779 | ` * Rehash a node with a 64-bit integer key.` |
|       - |  780 | ` * Refer to [merge_sort(),array_shift()] implementations for more information.` |
|       - |  781 | ` */` |
|    7336 |  782 | `static void HashmapRehashIntNode(ph7_hashmap_node *pEntry)` |
|       2 |  783 |  |
|    7338 |  784 | `	ph7_hashmap *pMap = pEntry->pMap;` |
|       - |  785 | `	sxu32 nBucket;` |
|       - |  786 | `	/* Remove old collision links */` |
|    7338 |  787 | `	if( pEntry->pPrevCollide ){` |
|    5889 |  788 | `		pEntry->pPrevCollide->pNextCollide = pEntry->pNextCollide;` |
|    2940 |  789 | `	}else{` |
|    1451 |  790 | `		pMap->apBucket[pEntry->nHash & (pMap->nSize - 1)] = pEntry->pNextCollide;` |
|       - |  791 | `	}` |
|    7338 |  792 | `	if( pEntry->pNextCollide ){` |
|     628 |  793 | `		pEntry->pNextCollide->pPrevCollide = pEntry->pPrevCollide;` |
|     309 |  794 | `	}` |
|    7338 |  795 | `	pEntry->pNextCollide = pEntry->pPrevCollide = 0;` |
|       - |  796 | `	/* Compute the new hash */` |
|    7338 |  797 | `	pEntry->nHash = pMap->xIntHash(pMap->iNextIdx);` |
|    7338 |  798 | `	pEntry->xKey.iKey = pMap->iNextIdx;` |
|    7338 |  799 | `	nBucket = pEntry->nHash & (pMap->nSize - 1);` |
|       - |  800 | `	/* Link to the new bucket */` |
|    7338 |  801 | `	pEntry->pNextCollide = pMap->apBucket[nBucket];` |
|    7338 |  802 | `	if( pMap->apBucket[nBucket] ){` |
|    6051 |  803 | `		pMap->apBucket[nBucket]->pPrevCollide = pEntry;` |
|    3024 |  804 | `	}` |
|    7338 |  805 | `	pEntry->pNextCollide = pMap->apBucket[nBucket];` |
|    7338 |  806 | `	pMap->apBucket[nBucket] = pEntry;` |
|       - |  807 | `	/* Increment the automatic index */` |
|    7338 |  808 | `	pMap->iNextIdx++;` |
|    7338 |  809 |  |
|       - |  810 | `/*` |
|       - |  811 | ` * Perform a linear search on a given hashmap.` |
|       - |  812 | ` * Write a pointer to the target node on success.` |
|       - |  813 | ` * Otherwise SXERR_NOTFOUND is returned on failure.` |
|       - |  814 | ` * Refer to [array_intersect(),array_diff(),in_array(),...] implementations` |
|       - |  815 | ` * for more information.` |
|       - |  816 | ` */` |
|   18642 |  817 | `static int HashmapFindValue(` |
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
|   18644 |  830 | `	pEntry = pMap->pFirst;` |
|   18644 |  831 | `	n = pMap->nEntry;` |
|   18644 |  832 | `	PH7_MemObjInit(pMap->pVm,&sVal);` |
|   18644 |  833 | `	PH7_MemObjInit(pMap->pVm,&sNeedle);` |
|   44706 |  834 | `	for(;;){` |
|   89412 |  835 | `		if( n < 1 ){` |
|      25 |  836 | `			break;` |
|       - |  837 | `		}` |
|       - |  838 | `		/* Extract node value */` |
|   89388 |  839 | `		pVal = HashmapExtractNodeValue(pEntry);` |
|   89388 |  840 | `		if( pVal ){` |
|   89388 |  841 | `			if( (pVal->iFlags\|pNeedle->iFlags) & MEMOBJ_NULL ){` |
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
|   89388 |  853 | `				PH7_MemObjLoad(pVal,&sVal);` |
|   89388 |  854 | `				PH7_MemObjLoad(pNeedle,&sNeedle);` |
|   89388 |  855 | `				rc = PH7_MemObjCmp(&sNeedle,&sVal,bStrict,0);` |
|   89388 |  856 | `				PH7_MemObjRelease(&sVal);` |
|   89388 |  857 | `				PH7_MemObjRelease(&sNeedle);` |
|   89388 |  858 | `				if( rc == 0 ){` |
|   18620 |  859 | `					if( ppNode ){` |
|       3 |  860 | `						*ppNode = pEntry;` |
|       1 |  861 | `					}` |
|       - |  862 | `					/* Match found*/` |
|   18620 |  863 | `					return SXRET_OK;` |
|       - |  864 | `				}` |
|       - |  865 | `			}` |
|   35385 |  866 | `		}` |
|       - |  867 | `		/* Point to the next entry */` |
|   70770 |  868 | `		pEntry = pEntry->pPrev; /* Reverse link */` |
|   70770 |  869 | `		n--;` |
|       2 |  870 | `	}` |
|       - |  871 | `	/* No such entry */` |
|      25 |  872 | `	return SXERR_NOTFOUND;` |
|    9323 |  873 |  |
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
|  343336 | 1047 | `static sxi32 HashmapDuplicateNode(` |
|       - | 1048 | `	ph7_hashmap *pDest,` |
|       - | 1049 | `	ph7_hashmap_node *pEntry,` |
|       - | 1050 | `	ph7_value *pVal,` |
|       - | 1051 | `	int iAction /* 0: Merge, 1: Overwrite, 2: Dup */` |
|       - | 1052 | `	)` |
|       2 | 1053 |  |
|  343338 | 1054 | `	ph7_value sSafeVal = *pVal;` |
|       - | 1055 | `	ph7_value sKey;` |
|       - | 1056 | `	sxi32 rc;` |
|       - | 1057 |  |
|  343338 | 1058 | `	if( pEntry->iType == HASHMAP_BLOB_NODE ){` |
|       - | 1059 | `		/* Blob key insertion */` |
|      19 | 1060 | `		PH7_MemObjInitFromString(pDest->pVm,&sKey,0);` |
|      19 | 1061 | `		PH7_MemObjStringAppend(&sKey,(const char *)SyBlobData(&pEntry->xKey.sKey),SyBlobLength(&pEntry->xKey.sKey));` |
|      19 | 1062 | `		rc = PH7_HashmapInsert(pDest,&sKey,&sSafeVal);` |
|      19 | 1063 | `		PH7_MemObjRelease(&sKey);` |
|      10 | 1064 | `	}else{` |
|       - | 1065 | `		/* Int key */` |
|  343320 | 1066 | `		if( iAction == 0 ){ /* Merge */` |
|  343304 | 1067 | `			rc = HashmapInsert(pDest,0/* Automatic index assign */,&sSafeVal);` |
|  171669 | 1068 | `		}else if( iAction == 1 ){ /* Overwrite */` |
|       5 | 1069 | `			PH7_MemObjInitFromInt(pDest->pVm,&sKey,pEntry->xKey.iKey);` |
|       5 | 1070 | `			rc = PH7_HashmapInsert(pDest,&sKey,&sSafeVal);` |
|       5 | 1071 | `			PH7_MemObjRelease(&sKey);` |
|       3 | 1072 | `		}else{ /* Dup */` |
|      14 | 1073 | `			rc = HashmapInsertIntKey(pDest,pEntry->xKey.iKey,&sSafeVal,0,FALSE);` |
|       - | 1074 | `		}` |
|       - | 1075 | `	}` |
|  343338 | 1076 | `	return rc;` |
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
|    1588 | 1090 | `static sxi32 HashmapMerge(ph7_hashmap *pSrc,ph7_hashmap *pDest)` |
|       2 | 1091 |  |
|       - | 1092 | `	ph7_hashmap_node *pEntry;` |
|       - | 1093 | `	ph7_value *pVal;` |
|       - | 1094 | `	sxi32 rc;` |
|       - | 1095 | `	sxu32 n;` |
|    1590 | 1096 | `	if( pSrc == pDest ){` |
|       - | 1097 | `		/* Same map. This can easily happen since hashmaps are passed by reference.` |
|       - | 1098 | `		 * Unlike the zend engine.` |
|       - | 1099 | `		 */` |
|     ! 0 | 1100 | `		return SXRET_OK;` |
|       - | 1101 | `	}` |
|       - | 1102 | `	/* Point to the first inserted entry in the source */` |
|    1590 | 1103 | `	pEntry = pSrc->pFirst;` |
|       - | 1104 | `	/* Perform the merge */` |
|  344898 | 1105 | `	for( n = 0 ; n < pSrc->nEntry ; ++n ){` |
|       - | 1106 | `		/* Extract the node value */` |
|  343310 | 1107 | `		pVal = HashmapExtractNodeValue(pEntry);` |
|  343310 | 1108 | `		if( pVal ){` |
|       - | 1109 | `			/* Make a local copy of the value.` |
|       - | 1110 | `			 * The insertion call below may trigger a memory pool reallocation` |
|       - | 1111 | `			 * which will invalidate the 'pVal' pointer since it points` |
|       - | 1112 | `			 * to the old pool.` |
|       - | 1113 | `			 */` |
|  343310 | 1114 | `			rc = HashmapDuplicateNode(pDest,pEntry,pVal,0);` |
|  171656 | 1115 | `		}else{` |
|     ! 0 | 1116 | `			rc = SXRET_OK;` |
|       - | 1117 | `		}` |
|  343310 | 1118 | `		if( rc != SXRET_OK ){` |
|     ! 0 | 1119 | `			return rc;` |
|       - | 1120 | `		}` |
|       - | 1121 | `		/* Point to the next entry */` |
|  343310 | 1122 | `		pEntry = pEntry->pPrev; /* Reverse link */` |
|  171656 | 1123 | `	}` |
|    1590 | 1124 | `	return SXRET_OK;` |
|     796 | 1125 |  |
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
|      10 | 1175 | `PH7_PRIVATE sxi32 PH7_HashmapDup(ph7_hashmap *pSrc,ph7_hashmap *pDest)` |
|       2 | 1176 |  |
|       - | 1177 | `	ph7_hashmap_node *pEntry;` |
|       - | 1178 | `	ph7_value *pVal;` |
|       - | 1179 | `	sxi32 rc;` |
|       - | 1180 | `	sxu32 n;` |
|      12 | 1181 | `	if( pSrc == pDest ){` |
|       - | 1182 | `		/* Same map. This can easily happen since hashmaps are passed by reference.` |
|       - | 1183 | `		 * Unlike the zend engine.` |
|       - | 1184 | `		 */` |
|     ! 0 | 1185 | `		return SXRET_OK;` |
|       - | 1186 | `	}` |
|       - | 1187 | `	/* Point to the first inserted entry in the source */` |
|      12 | 1188 | `	pEntry = pSrc->pFirst;` |
|       - | 1189 | `	/* Perform the duplication */` |
|      32 | 1190 | `	for( n = 0 ; n < pSrc->nEntry ; ++n ){` |
|       - | 1191 | `		/* Extract the node value */` |
|      22 | 1192 | `		pVal = HashmapExtractNodeValue(pEntry);` |
|      22 | 1193 | `		if( pVal ){` |
|      22 | 1194 | `			rc = HashmapDuplicateNode(pDest,pEntry,pVal,2);` |
|      12 | 1195 | `		}else{` |
|     ! 0 | 1196 | `			rc = SXRET_OK;` |
|       - | 1197 | `		}` |
|      22 | 1198 | `		if( rc != SXRET_OK ){` |
|     ! 0 | 1199 | `			return rc;` |
|       - | 1200 | `		}` |
|       - | 1201 | `		/* Point to the next entry */` |
|      22 | 1202 | `		pEntry = pEntry->pPrev; /* Reverse link */` |
|      12 | 1203 | `	}` |
|      12 | 1204 | `	return SXRET_OK;` |
|       7 | 1205 |  |
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
|   48318 | 1297 | `PH7_PRIVATE ph7_hashmap * PH7_NewHashmap(` |
|       - | 1298 | `	ph7_vm *pVm,              /* VM that trigger the hashmap creation */` |
|       - | 1299 | `	sxu32 (*xIntHash)(sxi64), /* Hash function for int keys.NULL otherwise*/` |
|       - | 1300 | `	sxu32 (*xBlobHash)(const void *,sxu32) /* Hash function for BLOB keys.NULL otherwise */` |
|       - | 1301 | `	)` |
|       2 | 1302 |  |
|       - | 1303 | `	ph7_hashmap *pMap;` |
|       - | 1304 | `	/* Allocate a new instance */` |
|   48320 | 1305 | `	pMap = (ph7_hashmap *)SyMemBackendPoolAlloc(&pVm->sAllocator,sizeof(ph7_hashmap));` |
|   48320 | 1306 | `	if( pMap == 0 ){` |
|     ! 0 | 1307 | `		return 0;` |
|       - | 1308 | `	}` |
|       - | 1309 | `	/* Zero the structure */` |
|   48320 | 1310 | `	SyZero(pMap,sizeof(ph7_hashmap));` |
|       - | 1311 | `	/* Fill in the structure */` |
|   48320 | 1312 | `	pMap->pVm = &(*pVm);` |
|   48320 | 1313 | `	pMap->iRef = 1;` |
|       - | 1314 | `	/* Default hash functions */` |
|   48320 | 1315 | `	pMap->xIntHash  = xIntHash ? xIntHash : IntHash;` |
|   48320 | 1316 | `	pMap->xBlobHash = xBlobHash ? xBlobHash : BinHash;` |
|   48320 | 1317 | `	return pMap;` |
|   24161 | 1318 |  |
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
|    1280 | 1339 | `PH7_PRIVATE sxi32 PH7_HashmapCreateSuper(ph7_vm *pVm)` |
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
|    1282 | 1359 | `	pMap = PH7_NewHashmap(&(*pVm),0,0);` |
|    1282 | 1360 | `	if( pMap == 0 ){` |
|     ! 0 | 1361 | `		return SXERR_MEM;` |
|       - | 1362 | `	}` |
|    1282 | 1363 | `	pVm->pGlobal = pMap;` |
|       - | 1364 | `	/* Reserve a ph7_value for the $GLOBALS array*/` |
|    1282 | 1365 | `	pObj = PH7_ReserveMemObj(&(*pVm));` |
|    1282 | 1366 | `	if( pObj == 0 ){` |
|     ! 0 | 1367 | `		return SXERR_MEM;` |
|       - | 1368 | `	}` |
|    1282 | 1369 | `	PH7_MemObjInitFromArray(&(*pVm),pObj,pMap);` |
|       - | 1370 | `	/* Record object index */` |
|    1282 | 1371 | `	pVm->nGlobalIdx = pObj->nIdx;` |
|       - | 1372 | `	/* Install the special $GLOBALS array */` |
|    1282 | 1373 | `	rc = SyHashInsert(&pVm->hSuper,(const void *)"GLOBALS",sizeof("GLOBALS")-1,SX_INT_TO_PTR(pVm->nGlobalIdx));` |
|    1282 | 1374 | `	if( rc != SXRET_OK ){` |
|     ! 0 | 1375 | `		return rc;` |
|       - | 1376 | `	}` |
|       - | 1377 | `	/* Install superglobals now */` |
|   14082 | 1378 | `	for( n =  0 ; n < SX_ARRAYSIZE(azSuper)  ; n++ ){` |
|       - | 1379 | `		ph7_value *pSuper;` |
|       - | 1380 | `		/* Request an empty array */` |
|   12802 | 1381 | `		pSuper = ph7_new_array(&(*pVm));` |
|   12802 | 1382 | `		if( pSuper == 0 ){` |
|     ! 0 | 1383 | `			return SXERR_MEM;` |
|       - | 1384 | `		}` |
|       - | 1385 | `		/* Install */` |
|   12802 | 1386 | `		rc = ph7_vm_config(&(*pVm),PH7_VM_CONFIG_CREATE_SUPER,azSuper[n]/* Super-global name*/,pSuper/* Super-global value */);` |
|   12802 | 1387 | `		if( rc != SXRET_OK ){` |
|     ! 0 | 1388 | `			return rc;` |
|       - | 1389 | `		}` |
|       - | 1390 | `		/* Release the value now it have been installed */` |
|   12802 | 1391 | `		ph7_release_value(&(*pVm),pSuper);` |
|    6402 | 1392 | `	}` |
|       - | 1393 | `	/* Set some $_SERVER entries */` |
|    1282 | 1394 | `	pFile = (SyString *)SySetPeek(&pVm->aFiles);` |
|       - | 1395 | `	/*` |
|       - | 1396 | `	 * 'SCRIPT_FILENAME'` |
|       - | 1397 | `	 * The absolute pathname of the currently executing script.` |
|       - | 1398 | `	 */` |
|    2558 | 1399 | `	ph7_vm_config(pVm,PH7_VM_CONFIG_SERVER_ATTR,` |
|       - | 1400 | `		"SCRIPT_FILENAME",` |
|     640 | 1401 | `		pFile ? pFile->zString : ":Memory:",` |
|    1276 | 1402 | `		pFile ? pFile->nByte : sizeof(":Memory:") - 1` |
|       - | 1403 | `		);` |
|       - | 1404 | `	/* All done,all super-global are installed now */` |
|    1282 | 1405 | `	return SXRET_OK;` |
|     642 | 1406 |  |
|       - | 1407 | `/*` |
|       - | 1408 | ` * Release a hashmap.` |
|       - | 1409 | ` */` |
|   34180 | 1410 | `PH7_PRIVATE sxi32 PH7_HashmapRelease(ph7_hashmap *pMap,int FreeDS)` |
|       2 | 1411 |  |
|       - | 1412 | `	ph7_hashmap_node *pEntry,*pNext;` |
|   34182 | 1413 | `	ph7_vm *pVm = pMap->pVm;` |
|       - | 1414 | `	sxu32 n;` |
|   34182 | 1415 | `	if( pMap == pVm->pGlobal ){` |
|       - | 1416 | `		/* Cannot delete the $GLOBALS array */` |
|     ! 0 | 1417 | `		PH7_VmThrowError(pMap->pVm,0,PH7_CTX_NOTICE,"$GLOBALS is a read-only array,deletion is forbidden");` |
|     ! 0 | 1418 | `		return SXRET_OK;` |
|       - | 1419 | `	}` |
|       - | 1420 | `	/* Start the release process */` |
|   34182 | 1421 | `	n = 0;` |
|   34182 | 1422 | `	pEntry = pMap->pFirst;` |
| 1373354 | 1423 | `	for(;;){` |
| 2746710 | 1424 | `		if( n >= pMap->nEntry ){` |
|   34182 | 1425 | `			break;` |
|       - | 1426 | `		}` |
| 2712530 | 1427 | `		pNext = pEntry->pPrev; /* Reverse link */` |
|       - | 1428 | `		/* Remove the reference from the foreign table */` |
| 2712530 | 1429 | `		PH7_VmRefObjRemove(pVm,pEntry->nValIdx,0,pEntry);` |
| 2712530 | 1430 | `		if( (pEntry->iFlags & HASHMAP_NODE_FOREIGN_OBJ) == 0 ){` |
|       - | 1431 | `			/* Restore the ph7_value to the free list */` |
| 2712522 | 1432 | `			PH7_VmUnsetMemObj(pVm,pEntry->nValIdx,FALSE);` |
| 1356260 | 1433 | `		}` |
|       - | 1434 | `		/* Release the node */` |
| 2712530 | 1435 | `		if( pEntry->iType == HASHMAP_BLOB_NODE ){` |
|   54174 | 1436 | `			SyBlobRelease(&pEntry->xKey.sKey);` |
|   27086 | 1437 | `		}` |
| 2712530 | 1438 | `		SyMemBackendPoolFree(&pVm->sAllocator,pEntry);` |
|       - | 1439 | `		/* Point to the next entry */` |
| 2712530 | 1440 | `		pEntry = pNext;` |
| 2712530 | 1441 | `		n++;` |
|       2 | 1442 | `	}` |
|   34182 | 1443 | `	if( pMap->nEntry > 0 ){` |
|       - | 1444 | `		/* Release the hash bucket */` |
|   30488 | 1445 | `		SyMemBackendFree(&pVm->sAllocator,pMap->apBucket);` |
|   15243 | 1446 | `	}` |
|   34182 | 1447 | `	if( FreeDS ){` |
|       - | 1448 | `		/* Free the whole instance */` |
|   34180 | 1449 | `		SyMemBackendPoolFree(&pVm->sAllocator,pMap);` |
|   17091 | 1450 | `	}else{` |
|       - | 1451 | `		/* Keep the instance but reset it's fields */` |
|       3 | 1452 | `		pMap->apBucket = 0;` |
|       3 | 1453 | `		pMap->iNextIdx = 0;` |
|       3 | 1454 | `		pMap->nEntry = pMap->nSize = 0;` |
|       3 | 1455 | `		pMap->pFirst = pMap->pLast = pMap->pCur = 0;` |
|       - | 1456 | `	}` |
|   34182 | 1457 | `	return SXRET_OK;` |
|   17092 | 1458 |  |
|       - | 1459 | `/*` |
|       - | 1460 | ` * Decrement the reference count of a given hashmap.` |
|       - | 1461 | ` * If the count reaches zero which mean no more variables` |
|       - | 1462 | ` * are pointing to this hashmap,then release the whole instance.` |
|       - | 1463 | ` */` |
|  402962 | 1464 | `PH7_PRIVATE void  PH7_HashmapUnref(ph7_hashmap *pMap)` |
|       2 | 1465 |  |
|  402964 | 1466 | `	ph7_vm *pVm = pMap->pVm;` |
|       - | 1467 | `	/* TICKET 1432-49: $GLOBALS is not subject to garbage collection */` |
|  402964 | 1468 | `	pMap->iRef--;` |
|  402964 | 1469 | `	if( pMap->iRef < 1 && pMap != pVm->pGlobal){` |
|   34180 | 1470 | `		PH7_HashmapRelease(pMap,TRUE);` |
|   17089 | 1471 | `	}` |
|  402964 | 1472 |  |
|       - | 1473 | `/*` |
|       - | 1474 | ` * Check if a given key exists in the given hashmap.` |
|       - | 1475 | ` * Write a pointer to the target node on success.` |
|       - | 1476 | ` * Otherwise SXERR_NOTFOUND is returned on failure.` |
|       - | 1477 | ` */` |
|   71464 | 1478 | `PH7_PRIVATE sxi32 PH7_HashmapLookup(` |
|       - | 1479 | `	ph7_hashmap *pMap,        /* Target hashmap */` |
|       - | 1480 | `	ph7_value *pKey,          /* Lookup key */` |
|       - | 1481 | `	ph7_hashmap_node **ppNode /* OUT: Target node on success */` |
|       - | 1482 | `	)` |
|       2 | 1483 |  |
|       - | 1484 | `	sxi32 rc;` |
|   71466 | 1485 | `	if( pMap->nEntry < 1 ){` |
|       - | 1486 | `		/* TICKET 1433-25: Don't bother hashing,the hashmap is empty anyway.` |
|       - | 1487 | `		 */` |
|       9 | 1488 | `		return SXERR_NOTFOUND;` |
|       - | 1489 | `	}` |
|   71458 | 1490 | `	rc = HashmapLookup(&(*pMap),&(*pKey),ppNode);` |
|   71458 | 1491 | `	return rc;` |
|   35734 | 1492 |  |
|       - | 1493 | `/*` |
|       - | 1494 | ` * Insert a given key and it's associated value (if any) in the given` |
|       - | 1495 | ` * hashmap.` |
|       - | 1496 | ` * If a node with the given key already exists in the database` |
|       - | 1497 | ` * then this function overwrite the old value.` |
|       - | 1498 | ` */` |
| 2374596 | 1499 | `PH7_PRIVATE sxi32 PH7_HashmapInsert(` |
|       - | 1500 | `	ph7_hashmap *pMap, /* Target hashmap */` |
|       - | 1501 | `	ph7_value *pKey,   /* Lookup key */` |
|       - | 1502 | `	ph7_value *pVal    /* Node value.NULL otherwise */` |
|       - | 1503 | `	)` |
|       2 | 1504 |  |
|       - | 1505 | `	sxi32 rc;` |
| 2374598 | 1506 | `	if( pVal && (pVal->iFlags & MEMOBJ_HASHMAP) && (ph7_hashmap *)pVal->x.pOther == pMap->pVm->pGlobal ){` |
|       - | 1507 | `		/*` |
|       - | 1508 | `		 * TICKET 1433-35: Insertion in the $GLOBALS array is forbidden.` |
|       - | 1509 | `		 */` |
|     ! 0 | 1510 | `		PH7_VmThrowError(pMap->pVm,0,PH7_CTX_ERR,"$GLOBALS is a read-only array,insertion is forbidden");` |
|     ! 0 | 1511 | `		return SXRET_OK;` |
|       - | 1512 | `	}` |
| 2374598 | 1513 | `	rc = HashmapInsert(&(*pMap),&(*pKey),&(*pVal));` |
| 2374598 | 1514 | `	return rc;` |
| 1187300 | 1515 |  |
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
|   17758 | 1543 | `PH7_PRIVATE sxi32 PH7_HashmapInsertByRef(` |
|       - | 1544 | `	ph7_hashmap *pMap, /* Target hashmap */` |
|       - | 1545 | `	ph7_value *pKey,   /* Lookup key */` |
|       - | 1546 | `	sxu32 nRefIdx      /* Foreign ph7_value index */` |
|       - | 1547 | `	)` |
|       2 | 1548 |  |
|       - | 1549 | `	sxi32 rc;` |
|   17760 | 1550 | `	if( nRefIdx == pMap->pVm->nGlobalIdx ){` |
|       - | 1551 | `		/*` |
|       - | 1552 | `		 * TICKET 1433-35: Insertion in the $GLOBALS array is forbidden.` |
|       - | 1553 | `		 */` |
|     ! 0 | 1554 | `		PH7_VmThrowError(pMap->pVm,0,PH7_CTX_ERR,"$GLOBALS is a read-only array,insertion is forbidden");` |
|     ! 0 | 1555 | `		return SXRET_OK;` |
|       - | 1556 | `	}` |
|   17760 | 1557 | `	rc = HashmapInsertByRef(&(*pMap),&(*pKey),nRefIdx);` |
|   17760 | 1558 | `	return rc;` |
|    8881 | 1559 |  |
|       - | 1560 | `/*` |
|       - | 1561 | ` * Reset the node cursor of a given hashmap.` |
|       - | 1562 | ` */` |
|   15252 | 1563 | `PH7_PRIVATE void PH7_HashmapResetLoopCursor(ph7_hashmap *pMap)` |
|       2 | 1564 |  |
|       - | 1565 | `	/* Reset the loop cursor */` |
|   15254 | 1566 | `	pMap->pCur = pMap->pFirst;` |
|   15254 | 1567 |  |
|       - | 1568 | `/*` |
|       - | 1569 | ` * Return a pointer to the node currently pointed by the node cursor.` |
|       - | 1570 | ` * If the cursor reaches the end of the list,then this function` |
|       - | 1571 | ` * return NULL.` |
|       - | 1572 | ` * Note that the node cursor is automatically advanced by this function.` |
|       - | 1573 | ` */` |
|  126482 | 1574 | `PH7_PRIVATE ph7_hashmap_node * PH7_HashmapGetNextEntry(ph7_hashmap *pMap)` |
|       2 | 1575 |  |
|  126484 | 1576 | `	ph7_hashmap_node *pCur = pMap->pCur;` |
|  126484 | 1577 | `	if( pCur == 0 ){` |
|       - | 1578 | `		/* End of the list,return null */` |
|    7630 | 1579 | `		return 0;` |
|       - | 1580 | `	}` |
|       - | 1581 | `	/* Advance the node cursor */` |
|  118856 | 1582 | `	pMap->pCur = pCur->pPrev; /* Reverse link */` |
|  118856 | 1583 | `	return pCur;` |
|   63243 | 1584 |  |
|       - | 1585 | `/*` |
|       - | 1586 | ` * Extract a node value.` |
|       - | 1587 | ` */` |
|  303284 | 1588 | `PH7_PRIVATE void PH7_HashmapExtractNodeValue(ph7_hashmap_node *pNode,ph7_value *pValue,int bStore)` |
|       2 | 1589 |  |
|  303286 | 1590 | `	ph7_value *pEntry = HashmapExtractNodeValue(pNode);` |
|  303286 | 1591 | `	if( pEntry ){` |
|  303286 | 1592 | `		if( bStore ){` |
|  118910 | 1593 | `			PH7_MemObjStore(pEntry,pValue);` |
|   59456 | 1594 | `		}else{` |
|  184378 | 1595 | `			PH7_MemObjLoad(pEntry,pValue);` |
|       - | 1596 | `		}` |
|  151701 | 1597 | `	}else{` |
|     ! 0 | 1598 | `		PH7_MemObjRelease(pValue);` |
|       - | 1599 | `	}` |
|  303286 | 1600 |  |
|       - | 1601 | `/*` |
|       - | 1602 | ` * Extract a node key.` |
|       - | 1603 | ` */` |
|   82836 | 1604 | `PH7_PRIVATE void PH7_HashmapExtractNodeKey(ph7_hashmap_node *pNode,ph7_value *pKey)` |
|       2 | 1605 |  |
|       - | 1606 | `	/* Fill with the current key */` |
|   82838 | 1607 | `	if( pNode->iType == HASHMAP_INT_NODE ){` |
|   82704 | 1608 | `		if( SyBlobLength(&pKey->sBlob) > 0 ){` |
|       3 | 1609 | `			SyBlobRelease(&pKey->sBlob);` |
|       1 | 1610 | `		}` |
|   82704 | 1611 | `		pKey->x.iVal = pNode->xKey.iKey;` |
|   82704 | 1612 | `		MemObjSetType(pKey,MEMOBJ_INT);` |
|   41353 | 1613 | `	}else{` |
|     135 | 1614 | `		SyBlobReset(&pKey->sBlob);` |
|     135 | 1615 | `		SyBlobAppend(&pKey->sBlob,SyBlobData(&pNode->xKey.sKey),SyBlobLength(&pNode->xKey.sKey));` |
|     135 | 1616 | `		MemObjSetType(pKey,MEMOBJ_STRING);` |
|       - | 1617 | `	}` |
|   82838 | 1618 |  |
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
|   21656 | 1666 | `static ph7_hashmap_node * HashmapNodeMerge(ph7_hashmap_node *pA,ph7_hashmap_node *pB,ProcNodeCmp xCmp,void *pCmpData)` |
|       2 | 1667 |  |
|       - | 1668 | `	ph7_hashmap_node result,*pTail;` |
|       - | 1669 | `    /* Prevent compiler warning */` |
|   21658 | 1670 | `	result.pNext = result.pPrev = 0;` |
|   21658 | 1671 | `	pTail = &result;` |
|   55676 | 1672 | `	while( pA && pB ){` |
|   34020 | 1673 | `		if( xCmp(pA,pB,pCmpData) < 0 ){` |
|   22328 | 1674 | `			pTail->pPrev = pA;` |
|   22328 | 1675 | `			pA->pNext = pTail;` |
|   22328 | 1676 | `			pTail = pA;` |
|   22328 | 1677 | `			pA = pA->pPrev;` |
|   11171 | 1678 | `		}else{` |
|   11694 | 1679 | `			pTail->pPrev = pB;` |
|   11694 | 1680 | `			pB->pNext = pTail;` |
|   11694 | 1681 | `			pTail = pB;` |
|   11694 | 1682 | `			pB = pB->pPrev;` |
|       - | 1683 | `		}` |
|       2 | 1684 | `	}` |
|   21658 | 1685 | `	if( pA ){` |
|   16095 | 1686 | `		pTail->pPrev = pA;` |
|   16095 | 1687 | `		pA->pNext = pTail;` |
|   13619 | 1688 | `	}else if( pB ){` |
|    5427 | 1689 | `		pTail->pPrev = pB;` |
|    5427 | 1690 | `		pB->pNext = pTail;` |
|    2707 | 1691 | `	}else{` |
|     140 | 1692 | `		pTail->pPrev = pTail->pNext = 0;` |
|       - | 1693 | `	}` |
|   21658 | 1694 | `	return result.pPrev;` |
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
|     488 | 1708 | `static sxi32 HashmapMergeSort(ph7_hashmap *pMap,ProcNodeCmp xCmp,void *pCmpData)` |
|       2 | 1709 |  |
|       - | 1710 | `	ph7_hashmap_node *a[N_SORT_BUCKET], *p,*pIn;` |
|       - | 1711 | `	sxu32 i;` |
|     490 | 1712 | `	SyZero(a,sizeof(a));` |
|       - | 1713 | `	/* Point to the first inserted entry */` |
|     490 | 1714 | `	pIn = pMap->pFirst;` |
|    7830 | 1715 | `	while( pIn ){` |
|    7342 | 1716 | `		p = pIn;` |
|    7342 | 1717 | `		pIn = p->pPrev;` |
|    7342 | 1718 | `		p->pPrev = 0;` |
|   13870 | 1719 | `		for(i=0; i<N_SORT_BUCKET-1; i++){` |
|   13870 | 1720 | `			if( a[i]==0 ){` |
|    7342 | 1721 | `				a[i] = p;` |
|    7342 | 1722 | `				break;` |
|     ! 0 | 1723 | `			}else{` |
|    6530 | 1724 | `				p = HashmapNodeMerge(a[i],p,xCmp,pCmpData);` |
|    6530 | 1725 | `				a[i] = 0;` |
|       - | 1726 | `			}` |
|    3266 | 1727 | `		}` |
|    7342 | 1728 | `		if( i==N_SORT_BUCKET-1 ){` |
|       - | 1729 | `			/* To get here, there need to be 2^(N_SORT_BUCKET) elements in he input list.` |
|       - | 1730 | `			 * But that is impossible.` |
|       - | 1731 | `			 */` |
|     ! 0 | 1732 | `			a[i] = HashmapNodeMerge(a[i], p,xCmp,pCmpData);` |
|     ! 0 | 1733 | `		}` |
|       2 | 1734 | `	}` |
|     490 | 1735 | `	p = a[0];` |
|   15618 | 1736 | `	for(i=1; i<N_SORT_BUCKET; i++){` |
|   15130 | 1737 | `		p = HashmapNodeMerge(p,a[i],xCmp,pCmpData);` |
|    7566 | 1738 | `	}` |
|     490 | 1739 | `	p->pNext = 0;` |
|       - | 1740 | `	/* Reflect the change */` |
|     490 | 1741 | `	pMap->pFirst = p;` |
|       - | 1742 | `	/* Reset the loop cursor */` |
|     490 | 1743 | `	pMap->pCur = pMap->pFirst;` |
|     490 | 1744 | `	return SXRET_OK;` |
|       2 | 1745 |  |
|       - | 1746 | `/*` |
|       - | 1747 | ` * Node comparison callback.` |
|       - | 1748 | ` * used-by: [sort(),asort(),...]` |
|       - | 1749 | ` */` |
|   33953 | 1750 | `static sxi32 HashmapCmpCallback1(ph7_hashmap_node *pA,ph7_hashmap_node *pB,void *pCmpData)` |
|       2 | 1751 |  |
|       - | 1752 | `	ph7_value sA,sB;` |
|       - | 1753 | `	sxi32 iFlags;` |
|       - | 1754 | `	int rc;` |
|   33955 | 1755 | `	if( pCmpData == 0 ){` |
|       - | 1756 | `		/* Perform a standard comparison */` |
|   33951 | 1757 | `		rc = HashmapNodeCmp(pA,pB,FALSE);` |
|   33951 | 1758 | `		return rc;` |
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
|   17007 | 1784 |  |
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
|      17 | 1990 | `static sxi32 HashmapCmpCallback7(ph7_hashmap_node *pA,ph7_hashmap_node *pB,void *pCmpData)` |
|       1 | 1991 |  |
|       - | 1992 | `	sxu32 n;` |
|      10 | 1993 | `	SXUNUSED(pB); /* cc warning */` |
|      10 | 1994 | `	SXUNUSED(pCmpData);` |
|       - | 1995 | `	/* Grab a random number */` |
|      18 | 1996 | `	n = PH7_VmRandomNum(pA->pMap->pVm);` |
|       - | 1997 | `	/* if the random number is odd then the first node 'pA' is greater then` |
|       - | 1998 | `	 * the second node 'pB'. Otherwise the reverse is assumed.` |
|       - | 1999 | `	 */` |
|      18 | 2000 | `	return n&1 ? 1 : -1;` |
|       1 | 2001 |  |
|       - | 2002 | `/*` |
|       - | 2003 | ` * Rehash all nodes keys after a merge-sort have been applied.` |
|       - | 2004 | ` * Used by [sort(),usort() and rsort()].` |
|       - | 2005 | ` */` |
|     472 | 2006 | `static void HashmapSortRehash(ph7_hashmap *pMap)` |
|       2 | 2007 |  |
|       - | 2008 | `	ph7_hashmap_node *p,*pLast;` |
|       - | 2009 | `	sxu32 i;` |
|       - | 2010 | `	/* Rehash all entries */` |
|     474 | 2011 | `	pLast = p = pMap->pFirst;` |
|     474 | 2012 | `	pMap->iNextIdx = 0; /* Reset the automatic index */` |
|     474 | 2013 | `	i = 0;` |
|    3879 | 2014 | `	for( ;; ){` |
|    7760 | 2015 | `		if( i >= pMap->nEntry ){` |
|     474 | 2016 | `			pMap->pLast = pLast; /* Fix the last link broken by the merge-sort */` |
|     474 | 2017 | `			break;` |
|       - | 2018 | `		}` |
|    7288 | 2019 | `		if( p->iType == HASHMAP_BLOB_NODE ){` |
|       - | 2020 | `			/* Do not maintain index association as requested by the PHP specification */` |
|       5 | 2021 | `			SyBlobRelease(&p->xKey.sKey);` |
|       - | 2022 | `			/* Change key type */` |
|       5 | 2023 | `			p->iType = HASHMAP_INT_NODE;` |
|       2 | 2024 | `		}` |
|    7288 | 2025 | `		HashmapRehashIntNode(p);` |
|       - | 2026 | `		/* Point to the next entry */` |
|    7288 | 2027 | `		i++;` |
|    7288 | 2028 | `		pLast = p;` |
|    7288 | 2029 | `		p = p->pPrev; /* Reverse link */` |
|       2 | 2030 | `	}` |
|     474 | 2031 |  |
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
|     798 | 2053 | `static int ph7_hashmap_sort(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       2 | 2054 |  |
|       - | 2055 | `	ph7_hashmap *pMap;` |
|       - | 2056 | `	/* Make sure we are dealing with a valid hashmap */` |
|     800 | 2057 | `	if( nArg < 1 \|\| !ph7_value_is_array(apArg[0]) ){` |
|       - | 2058 | `		/* Missing/Invalid arguments,return FALSE */` |
|     ! 0 | 2059 | `		ph7_result_bool(pCtx,0);` |
|     ! 0 | 2060 | `		return PH7_OK;` |
|       - | 2061 | `	}` |
|       - | 2062 | `	/* Point to the internal representation of the input hashmap */` |
|     800 | 2063 | `	pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|     800 | 2064 | `	if( pMap->nEntry > 1 ){` |
|     468 | 2065 | `		sxi32 iCmpFlags = 0;` |
|     468 | 2066 | `		if( nArg > 1 ){` |
|       - | 2067 | `			/* Extract comparison flags */` |
|       3 | 2068 | `			iCmpFlags = ph7_value_to_int(apArg[1]);` |
|       3 | 2069 | `			if( iCmpFlags == 3 /* SORT_REGULAR */ ){` |
|     ! 0 | 2070 | `				iCmpFlags = 0; /* Standard comparison */` |
|     ! 0 | 2071 | `			}` |
|       1 | 2072 | `		}` |
|       - | 2073 | `		/* Do the merge sort */` |
|     468 | 2074 | `		HashmapMergeSort(pMap,HashmapCmpCallback1,SX_INT_TO_PTR(iCmpFlags));` |
|       - | 2075 | `		/* Rehash [Do not maintain index association as requested by the PHP specification] */` |
|     468 | 2076 | `		HashmapSortRehash(pMap);` |
|     233 | 2077 | `	}` |
|       - | 2078 | `	/* All done,return TRUE */` |
|     800 | 2079 | `	ph7_result_bool(pCtx,1);` |
|     800 | 2080 | `	return PH7_OK;` |
|     401 | 2081 |  |
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
|     476 | 2498 | `static int ph7_hashmap_count(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       2 | 2499 |  |
|     478 | 2500 | `	int bRecursive = FALSE;` |
|       - | 2501 | `	sxi64 iCount;` |
|     478 | 2502 | `	if( nArg < 1 ){` |
|       - | 2503 | `		/* Missing arguments,return 0 */` |
|     ! 0 | 2504 | `		ph7_result_int(pCtx,0);` |
|     ! 0 | 2505 | `		return PH7_OK;` |
|       - | 2506 | `	}` |
|     478 | 2507 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|       - | 2508 | `		/* TICKET 1433-19: Handle objects */` |
|       3 | 2509 | `		int res = !ph7_value_is_null(apArg[0]);` |
|       3 | 2510 | `		ph7_result_int(pCtx,res);` |
|       3 | 2511 | `		return PH7_OK;` |
|       - | 2512 | `	}` |
|     476 | 2513 | `	if( nArg > 1 ){` |
|       - | 2514 | `		/* Recursive count? */` |
|      31 | 2515 | `		bRecursive = ph7_value_to_int(apArg[1]) == 1 /* COUNT_RECURSIVE */;` |
|      15 | 2516 | `	}` |
|       - | 2517 | `	/* Count */` |
|     476 | 2518 | `	iCount = HashmapCount((ph7_hashmap *)apArg[0]->x.pOther,bRecursive,0);` |
|     476 | 2519 | `	ph7_result_int64(pCtx,iCount);` |
|     476 | 2520 | `	return PH7_OK;` |
|     240 | 2521 |  |
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
|       - | 3103 | ` * array array_values(array $input)` |
|       - | 3104 | ` *   Returns all the values from the input array and indexes numerically the array.` |
|       - | 3105 | ` * Parameters` |
|       - | 3106 | ` *   input: The input array.` |
|       - | 3107 | ` * Return` |
|       - | 3108 | ` *  An indexed array of values or NULL on failure.` |
|       - | 3109 | ` */` |
|      24 | 3110 | `static int ph7_hashmap_values(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       1 | 3111 |  |
|       - | 3112 | `	ph7_hashmap_node *pNode;` |
|       - | 3113 | `	ph7_hashmap *pMap;` |
|       - | 3114 | `	ph7_value *pArray;` |
|       - | 3115 | `	ph7_value *pObj;` |
|       - | 3116 | `	sxu32 n;` |
|      25 | 3117 | `	if( nArg < 1 ){` |
|       - | 3118 | `		/* Missing arguments,return NULL */` |
|       3 | 3119 | `		ph7_result_null(pCtx);` |
|       3 | 3120 | `		return PH7_OK;` |
|       - | 3121 | `	}` |
|       - | 3122 | `	/* Make sure we are dealing with a valid hashmap */` |
|      23 | 3123 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|       - | 3124 | `		/* Invalid argument,return NULL */` |
|     ! 0 | 3125 | `		ph7_result_null(pCtx);` |
|     ! 0 | 3126 | `		return PH7_OK;` |
|       - | 3127 | `	}` |
|       - | 3128 | `	/* Point to the internal representation that describe the input hashmap */` |
|      23 | 3129 | `	pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|       - | 3130 | `	/* Create a new array */` |
|      23 | 3131 | `	pArray = ph7_context_new_array(pCtx);` |
|      23 | 3132 | `	if( pArray == 0 ){` |
|     ! 0 | 3133 | `		ph7_result_null(pCtx);` |
|     ! 0 | 3134 | `		return PH7_OK;` |
|       - | 3135 | `	}` |
|       - | 3136 | `	/* Perform the requested operation */` |
|      23 | 3137 | `	pNode = pMap->pFirst;` |
|      81 | 3138 | `	for( n = 0 ; n < pMap->nEntry ; ++n ){` |
|      59 | 3139 | `		pObj = HashmapExtractNodeValue(pNode);` |
|      59 | 3140 | `		if( pObj ){` |
|       - | 3141 | `			/* perform the insertion */` |
|      59 | 3142 | `			ph7_array_add_elem(pArray,0/* Automatic index assign */,pObj);` |
|      29 | 3143 | `		}` |
|       - | 3144 | `		/* Point to the next entry */` |
|      59 | 3145 | `		pNode = pNode->pPrev; /* Reverse link */` |
|      30 | 3146 | `	}` |
|       - | 3147 | `	/* return the new array */` |
|      23 | 3148 | `	ph7_result_value(pCtx,pArray);` |
|      23 | 3149 | `	return PH7_OK;` |
|      13 | 3150 |  |
|       - | 3151 | `/*` |
|       - | 3152 | ` * array array_keys(array $input [, val $search_value [, bool $strict = false ]] )` |
|       - | 3153 | ` *  Return all the keys or a subset of the keys of an array.` |
|       - | 3154 | ` * Parameters` |
|       - | 3155 | ` *  $input` |
|       - | 3156 | ` *   An array containing keys to return.` |
|       - | 3157 | ` * $search_value` |
|       - | 3158 | ` *   If specified, then only keys containing these values are returned.` |
|       - | 3159 | ` * $strict` |
|       - | 3160 | ` *   Determines if strict comparison (===) should be used during the search.` |
|       - | 3161 | ` * Return` |
|       - | 3162 | ` *  An array of all the keys in input or NULL on failure.` |
|       - | 3163 | ` */` |
|      98 | 3164 | `static int ph7_hashmap_keys(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       1 | 3165 |  |
|       - | 3166 | `	ph7_hashmap_node *pNode;` |
|       - | 3167 | `	ph7_hashmap *pMap;` |
|       - | 3168 | `	ph7_value *pArray;` |
|       - | 3169 | `	ph7_value sObj;` |
|       - | 3170 | `	ph7_value sVal;` |
|       - | 3171 | `	SyString sKey;` |
|       - | 3172 | `	int bStrict;` |
|       - | 3173 | `	sxi32 rc;` |
|       - | 3174 | `	sxu32 n;` |
|      99 | 3175 | `	if( nArg < 1 ){` |
|       - | 3176 | `		/* Missing arguments,return NULL */` |
|       3 | 3177 | `		ph7_result_null(pCtx);` |
|       3 | 3178 | `		return PH7_OK;` |
|       - | 3179 | `	}` |
|       - | 3180 | `	/* Make sure we are dealing with a valid hashmap */` |
|      97 | 3181 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|       - | 3182 | `		/* Invalid argument,return NULL */` |
|     ! 0 | 3183 | `		ph7_result_null(pCtx);` |
|     ! 0 | 3184 | `		return PH7_OK;` |
|       - | 3185 | `	}` |
|       - | 3186 | `	/* Point to the internal representation of the input hashmap */` |
|      97 | 3187 | `	pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|       - | 3188 | `	/* Create a new array */` |
|      97 | 3189 | `	pArray = ph7_context_new_array(pCtx);` |
|      97 | 3190 | `	if( pArray == 0 ){` |
|     ! 0 | 3191 | `		ph7_result_null(pCtx);` |
|     ! 0 | 3192 | `		return PH7_OK;` |
|       - | 3193 | `	}` |
|      97 | 3194 | `	bStrict = FALSE;` |
|      97 | 3195 | `	if( nArg > 2 && ph7_value_is_bool(apArg[2]) ){` |
|     ! 0 | 3196 | `		bStrict = ph7_value_to_bool(apArg[2]);` |
|     ! 0 | 3197 | `	}` |
|       - | 3198 | `	/* Perform the requested operation */` |
|      97 | 3199 | `	pNode = pMap->pFirst;` |
|      97 | 3200 | `	PH7_MemObjInit(pMap->pVm,&sVal);` |
|     481 | 3201 | `	for( n = 0 ; n < pMap->nEntry ; ++n ){` |
|     385 | 3202 | `		if( pNode->iType == HASHMAP_INT_NODE ){` |
|      81 | 3203 | `			PH7_MemObjInitFromInt(pMap->pVm,&sObj,pNode->xKey.iKey);` |
|      41 | 3204 | `		}else{` |
|     305 | 3205 | `			SyStringInitFromBuf(&sKey,SyBlobData(&pNode->xKey.sKey),SyBlobLength(&pNode->xKey.sKey));` |
|     305 | 3206 | `			PH7_MemObjInitFromString(pMap->pVm,&sObj,&sKey);` |
|       - | 3207 | `		}` |
|     385 | 3208 | `		rc = 0;` |
|     385 | 3209 | `		if( nArg > 1 ){` |
|     ! 0 | 3210 | `			ph7_value *pValue = HashmapExtractNodeValue(pNode);` |
|     ! 0 | 3211 | `			if( pValue ){` |
|     ! 0 | 3212 | `				PH7_MemObjLoad(pValue,&sVal);` |
|       - | 3213 | `				/* Filter key */` |
|     ! 0 | 3214 | `				rc = ph7_value_compare(&sVal,apArg[1],bStrict);` |
|     ! 0 | 3215 | `				PH7_MemObjRelease(pValue);` |
|     ! 0 | 3216 | `			}` |
|     ! 0 | 3217 | `		}` |
|     385 | 3218 | `		if( rc == 0 ){` |
|       - | 3219 | `			/* Perform the insertion */` |
|     385 | 3220 | `			ph7_array_add_elem(pArray,0,&sObj);` |
|     192 | 3221 | `		}` |
|     385 | 3222 | `		PH7_MemObjRelease(&sObj);` |
|       - | 3223 | `		/* Point to the next entry */` |
|     385 | 3224 | `		pNode = pNode->pPrev; /* Reverse link */` |
|     193 | 3225 | `	}` |
|       - | 3226 | `	/* return the new array */` |
|      97 | 3227 | `	ph7_result_value(pCtx,pArray);` |
|      97 | 3228 | `	return PH7_OK;` |
|      50 | 3229 |  |
|       - | 3230 | `/*` |
|       - | 3231 | ` * bool array_same(array $arr1,array $arr2)` |
|       - | 3232 | ` *  Return TRUE if the given arrays are the same instance.` |
|       - | 3233 | ` *  This function is useful under PH7 since arrays are passed` |
|       - | 3234 | ` *  by reference unlike the zend engine which use pass by values.` |
|       - | 3235 | ` * Parameters` |
|       - | 3236 | ` *  $arr1` |
|       - | 3237 | ` *   First array` |
|       - | 3238 | ` *  $arr2` |
|       - | 3239 | ` *   Second array` |
|       - | 3240 | ` * Return` |
|       - | 3241 | ` *  TRUE if the arrays are the same instance.FALSE otherwise.` |
|       - | 3242 | ` * Note` |
|       - | 3243 | ` *  This function is a symisc eXtension.` |
|       - | 3244 | ` */` |
|       4 | 3245 | `static int ph7_hashmap_same(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       1 | 3246 |  |
|       - | 3247 | `	ph7_hashmap *p1,*p2;` |
|       - | 3248 | `	int rc;` |
|       5 | 3249 | `	if( nArg < 2 \|\| !ph7_value_is_array(apArg[0]) \|\| !ph7_value_is_array(apArg[1]) ){` |
|       - | 3250 | `		/* Missing or invalid arguments,return FALSE*/` |
|     ! 0 | 3251 | `		ph7_result_bool(pCtx,0);` |
|     ! 0 | 3252 | `		return PH7_OK;` |
|       - | 3253 | `	}` |
|       - | 3254 | `	/* Point to the hashmaps */` |
|       5 | 3255 | `	p1 = (ph7_hashmap *)apArg[0]->x.pOther;` |
|       5 | 3256 | `	p2 = (ph7_hashmap *)apArg[1]->x.pOther;` |
|       5 | 3257 | `	rc = (p1 == p2);` |
|       - | 3258 | `	/* Same instance? */` |
|       5 | 3259 | `	ph7_result_bool(pCtx,rc);` |
|       5 | 3260 | `	return PH7_OK;` |
|       3 | 3261 |  |
|       - | 3262 | `/*` |
|       - | 3263 | ` * array array_merge(array $array1,...)` |
|       - | 3264 | ` *  Merge one or more arrays.` |
|       - | 3265 | ` * Parameters` |
|       - | 3266 | ` *  $array1` |
|       - | 3267 | ` *    Initial array to merge.` |
|       - | 3268 | ` *  ...` |
|       - | 3269 | ` *   More array to merge.` |
|       - | 3270 | ` * Return` |
|       - | 3271 | ` *  The resulting array.` |
|       - | 3272 | ` */` |
|     794 | 3273 | `static int ph7_hashmap_merge(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       2 | 3274 |  |
|       - | 3275 | `	ph7_hashmap *pMap,*pSrc;` |
|       - | 3276 | `	ph7_value *pArray;` |
|       - | 3277 | `	int i;` |
|     796 | 3278 | `	if( nArg < 1 ){` |
|       - | 3279 | `		/* Missing arguments,return NULL */` |
|     ! 0 | 3280 | `		ph7_result_null(pCtx);` |
|     ! 0 | 3281 | `		return PH7_OK;` |
|       - | 3282 | `	}` |
|       - | 3283 | `	/* Create a new array */` |
|     796 | 3284 | `	pArray = ph7_context_new_array(pCtx);` |
|     796 | 3285 | `	if( pArray == 0 ){` |
|     ! 0 | 3286 | `		ph7_result_null(pCtx);` |
|     ! 0 | 3287 | `		return PH7_OK;` |
|       - | 3288 | `	}` |
|       - | 3289 | `	/* Point to the internal representation of the hashmap */` |
|     796 | 3290 | `	pMap = (ph7_hashmap *)pArray->x.pOther;` |
|       - | 3291 | `	/* Start merging */` |
|    2384 | 3292 | `	for( i = 0 ; i < nArg ; i++ ){` |
|       - | 3293 | `		/* Make sure we are dealing with a valid hashmap */` |
|    1590 | 3294 | `		if( !ph7_value_is_array(apArg[i]) ){` |
|       - | 3295 | `			/* Insert scalar value */` |
|       5 | 3296 | `			ph7_array_add_elem(pArray,0,apArg[i]);` |
|       3 | 3297 | `		}else{` |
|    1586 | 3298 | `			pSrc = (ph7_hashmap *)apArg[i]->x.pOther;` |
|       - | 3299 | `			/* Merge the two hashmaps */` |
|    1586 | 3300 | `			HashmapMerge(pSrc,pMap);` |
|       - | 3301 | `		}` |
|     796 | 3302 | `	}` |
|       - | 3303 | `	/* Return the freshly created array */` |
|     796 | 3304 | `	ph7_result_value(pCtx,pArray);` |
|     796 | 3305 | `	return PH7_OK;` |
|     399 | 3306 |  |
|       - | 3307 | `/*` |
|       - | 3308 | ` * array array_copy(array $source)` |
|       - | 3309 | ` *  Make a blind copy of the target array.` |
|       - | 3310 | ` * Parameters` |
|       - | 3311 | ` *  $source` |
|       - | 3312 | ` *   Target array` |
|       - | 3313 | ` * Return` |
|       - | 3314 | ` *  Copy of the target array on success.NULL otherwise.` |
|       - | 3315 | ` * Note` |
|       - | 3316 | ` *  This function is a symisc eXtension.` |
|       - | 3317 | ` */` |
|       2 | 3318 | `static int ph7_hashmap_copy(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       1 | 3319 |  |
|       - | 3320 | `	ph7_hashmap *pMap;` |
|       - | 3321 | `	ph7_value *pArray;` |
|       3 | 3322 | `	if( nArg < 1 ){` |
|       - | 3323 | `		/* Missing arguments,return NULL */` |
|     ! 0 | 3324 | `		ph7_result_null(pCtx);` |
|     ! 0 | 3325 | `		return PH7_OK;` |
|       - | 3326 | `	}` |
|       - | 3327 | `	/* Create a new array */` |
|       3 | 3328 | `	pArray = ph7_context_new_array(pCtx);` |
|       3 | 3329 | `	if( pArray == 0 ){` |
|     ! 0 | 3330 | `		ph7_result_null(pCtx);` |
|     ! 0 | 3331 | `		return PH7_OK;` |
|       - | 3332 | `	}` |
|       - | 3333 | `	/* Point to the internal representation of the hashmap */` |
|       3 | 3334 | `	pMap = (ph7_hashmap *)pArray->x.pOther;` |
|       3 | 3335 | `	if( ph7_value_is_array(apArg[0])){` |
|       - | 3336 | `		/* Point to the internal representation of the source */` |
|       3 | 3337 | `		ph7_hashmap *pSrc = (ph7_hashmap *)apArg[0]->x.pOther;` |
|       - | 3338 | `		/* Perform the copy */` |
|       3 | 3339 | `		PH7_HashmapDup(pSrc,pMap);` |
|       2 | 3340 | `	}else{` |
|       - | 3341 | `		/* Simple insertion */` |
|     ! 0 | 3342 | `		PH7_HashmapInsert(pMap,0/* Automatic index assign*/,apArg[0]);` |
|       - | 3343 | `	}` |
|       - | 3344 | `	/* Return the duplicated array */` |
|       3 | 3345 | `	ph7_result_value(pCtx,pArray);` |
|       3 | 3346 | `	return PH7_OK;` |
|       2 | 3347 |  |
|       - | 3348 | `/*` |
|       - | 3349 | ` * bool array_erase(array $source)` |
|       - | 3350 | ` *  Remove all elements from a given array.` |
|       - | 3351 | ` * Parameters` |
|       - | 3352 | ` *  $source` |
|       - | 3353 | ` *   Target array` |
|       - | 3354 | ` * Return` |
|       - | 3355 | ` *  TRUE on success.FALSE otherwise.` |
|       - | 3356 | ` * Note` |
|       - | 3357 | ` *  This function is a symisc eXtension.` |
|       - | 3358 | ` */` |
|       2 | 3359 | `static int ph7_hashmap_erase(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       1 | 3360 |  |
|       - | 3361 | `	ph7_hashmap *pMap;` |
|       3 | 3362 | `	if( nArg < 1 ){` |
|       - | 3363 | `		/* Missing arguments */` |
|     ! 0 | 3364 | `		ph7_result_bool(pCtx,0);` |
|     ! 0 | 3365 | `		return PH7_OK;` |
|       - | 3366 | `	}` |
|       - | 3367 | `	/* Point to the target hashmap */` |
|       3 | 3368 | `	pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|       - | 3369 | `	/* Erase */` |
|       3 | 3370 | `	PH7_HashmapRelease(pMap,FALSE);` |
|       3 | 3371 | `	return PH7_OK;` |
|       2 | 3372 |  |
|       - | 3373 | `/*` |
|       - | 3374 | ` * array array_slice(array $array,int $offset [,int $length [, bool $preserve_keys = false ]])` |
|       - | 3375 | ` *  Extract a slice of the array.` |
|       - | 3376 | ` * Parameters` |
|       - | 3377 | ` *  $array` |
|       - | 3378 | ` *    The input array.` |
|       - | 3379 | ` * $offset` |
|       - | 3380 | ` *    If offset is non-negative, the sequence will start at that offset in the array.` |
|       - | 3381 | ` *    If offset is negative, the sequence will start that far from the end of the array.` |
|       - | 3382 | ` * $length (optional)` |
|       - | 3383 | ` *    If length is given and is positive, then the sequence will have that many elements` |
|       - | 3384 | ` *    in it. If length is given and is negative then the sequence will stop that many` |
|       - | 3385 | ` *   elements from the end of the array. If it is omitted, then the sequence will have` |
|       - | 3386 | ` *   everything from offset up until the end of the array.` |
|       - | 3387 | ` * $preserve_keys (optional)` |
|       - | 3388 | ` *    Note that array_slice() will reorder and reset the array indices by default.` |
|       - | 3389 | ` *    You can change this behaviour by setting preserve_keys to TRUE.` |
|       - | 3390 | ` * Return` |
|       - | 3391 | ` *   The new slice.` |
|       - | 3392 | ` */` |
|       8 | 3393 | `static int ph7_hashmap_slice(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       1 | 3394 |  |
|       - | 3395 | `	ph7_hashmap *pMap,*pSrc;` |
|       - | 3396 | `	ph7_hashmap_node *pCur;` |
|       - | 3397 | `	ph7_value *pArray;` |
|       - | 3398 | `	int iLength,iOfft;` |
|       - | 3399 | `	int bPreserve;` |
|       - | 3400 | `	sxi32 rc;` |
|       9 | 3401 | `	if( nArg < 2 \|\| !ph7_value_is_array(apArg[0]) ){` |
|       - | 3402 | `		/* Missing/Invalid arguments,return NULL */` |
|     ! 0 | 3403 | `		ph7_result_null(pCtx);` |
|     ! 0 | 3404 | `		return PH7_OK;` |
|       - | 3405 | `	}` |
|       - | 3406 | `	/* Point the internal representation of the target array */` |
|       9 | 3407 | `	pSrc = (ph7_hashmap *)apArg[0]->x.pOther;` |
|       9 | 3408 | `	bPreserve = FALSE;` |
|       - | 3409 | `	/* Get the offset */` |
|       9 | 3410 | `	iOfft = ph7_value_to_int(apArg[1]);` |
|       9 | 3411 | `	if( iOfft < 0 ){` |
|       3 | 3412 | `		iOfft = (int)pSrc->nEntry + iOfft;` |
|       1 | 3413 | `	}` |
|       9 | 3414 | `	if( iOfft < 0 \|\| iOfft > (int)pSrc->nEntry ){` |
|       - | 3415 | `		/* Invalid offset,return the last entry */` |
|     ! 0 | 3416 | `		iOfft = (int)pSrc->nEntry - 1;` |
|     ! 0 | 3417 | `	}` |
|       - | 3418 | `	/* Get the length */` |
|       9 | 3419 | `	iLength = (int)pSrc->nEntry - iOfft;` |
|       9 | 3420 | `	if( nArg > 2 ){` |
|       7 | 3421 | `		iLength = ph7_value_to_int(apArg[2]);` |
|       7 | 3422 | `		if( iLength < 0 ){` |
|     ! 0 | 3423 | `			iLength = ((int)pSrc->nEntry + iLength) - iOfft;` |
|     ! 0 | 3424 | `		}` |
|       7 | 3425 | `		if( iLength < 0 \|\| iOfft + iLength >= (int)pSrc->nEntry ){` |
|     ! 0 | 3426 | `			iLength = (int)pSrc->nEntry - iOfft;` |
|     ! 0 | 3427 | `		}` |
|       7 | 3428 | `		if( nArg > 3 && ph7_value_is_bool(apArg[3]) ){` |
|       3 | 3429 | `			bPreserve = ph7_value_to_bool(apArg[3]);` |
|       1 | 3430 | `		}` |
|       3 | 3431 | `	}` |
|       - | 3432 | `	/* Create a new array */` |
|       9 | 3433 | `	pArray = ph7_context_new_array(pCtx);` |
|       9 | 3434 | `	if( pArray == 0 ){` |
|     ! 0 | 3435 | `		ph7_result_null(pCtx);` |
|     ! 0 | 3436 | `		return PH7_OK;` |
|       - | 3437 | `	}` |
|       9 | 3438 | `	if( iLength < 1 ){` |
|       - | 3439 | `		/* Don't bother processing,return the empty array */` |
|     ! 0 | 3440 | `		ph7_result_value(pCtx,pArray);` |
|     ! 0 | 3441 | `		return PH7_OK;` |
|       - | 3442 | `	}` |
|       - | 3443 | `	/* Point to the desired entry */` |
|       9 | 3444 | `	pCur = pSrc->pFirst;` |
|       9 | 3445 | `	for(;;){` |
|      19 | 3446 | `		if( iOfft < 1 ){` |
|       9 | 3447 | `			break;` |
|       - | 3448 | `		}` |
|       - | 3449 | `		/* Point to the next entry */` |
|      11 | 3450 | `		pCur = pCur->pPrev; /* Reverse link */` |
|      11 | 3451 | `		iOfft--;` |
|       1 | 3452 | `	}` |
|       - | 3453 | `	/* Point to the internal representation of the hashmap */` |
|       9 | 3454 | `	pMap = (ph7_hashmap *)pArray->x.pOther;` |
|      12 | 3455 | `	for(;;){` |
|      25 | 3456 | `		if( iLength < 1 ){` |
|       9 | 3457 | `			break;` |
|       - | 3458 | `		}` |
|      17 | 3459 | `		rc = HashmapInsertNode(pMap,pCur,bPreserve);` |
|      17 | 3460 | `		if( rc != SXRET_OK ){` |
|     ! 0 | 3461 | `			break;` |
|       - | 3462 | `		}` |
|       - | 3463 | `		/* Point to the next entry */` |
|      17 | 3464 | `		pCur = pCur->pPrev; /* Reverse link */` |
|      17 | 3465 | `		iLength--;` |
|       1 | 3466 | `	}` |
|       - | 3467 | `	/* Return the freshly created array */` |
|       9 | 3468 | `	ph7_result_value(pCtx,pArray);` |
|       9 | 3469 | `	return PH7_OK;` |
|       5 | 3470 |  |
|       - | 3471 | `/*` |
|       - | 3472 | ` * array array_splice(array $array,int $offset [,int $length [,value $replacement ]])` |
|       - | 3473 | ` *  Remove a portion of the array and replace it with something else.` |
|       - | 3474 | ` * Parameters` |
|       - | 3475 | ` *  $array` |
|       - | 3476 | ` *    The input array.` |
|       - | 3477 | ` * $offset` |
|       - | 3478 | ` *    If offset is positive then the start of removed portion is at that offset from` |
|       - | 3479 | ` *    the beginning of the input array. If offset is negative then it starts that far` |
|       - | 3480 | ` *    from the end of the input array.` |
|       - | 3481 | ` * $length (optional)` |
|       - | 3482 | ` *    If length is omitted, removes everything from offset to the end of the array.` |
|       - | 3483 | ` *    If length is specified and is positive, then that many elements will be removed.` |
|       - | 3484 | ` *    If length is specified and is negative then the end of the removed portion will` |
|       - | 3485 | ` *    be that many elements from the end of the array.` |
|       - | 3486 | ` * $replacement (optional)` |
|       - | 3487 | ` *  If replacement array is specified, then the removed elements are replaced` |
|       - | 3488 | ` *  with elements from this array.` |
|       - | 3489 | ` *  If offset and length are such that nothing is removed, then the elements` |
|       - | 3490 | ` *  from the replacement array are inserted in the place specified by the offset.` |
|       - | 3491 | ` *  Note that keys in replacement array are not preserved.` |
|       - | 3492 | ` *  If replacement is just one element it is not necessary to put array() around` |
|       - | 3493 | ` *  it, unless the element is an array itself, an object or NULL.` |
|       - | 3494 | ` * Return` |
|       - | 3495 | ` *   A new array consisting of the extracted elements.` |
|       - | 3496 | ` */` |
|       2 | 3497 | `static int ph7_hashmap_splice(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       1 | 3498 |  |
|       - | 3499 | `	ph7_hashmap_node *pCur,*pPrev,*pRnode;` |
|       - | 3500 | `	ph7_value *pArray,*pRvalue,*pOld;` |
|       - | 3501 | `	ph7_hashmap *pMap,*pSrc,*pRep;` |
|       - | 3502 | `	int iLength,iOfft;` |
|       - | 3503 | `	sxi32 rc;` |
|       3 | 3504 | `	if( nArg < 2 \|\| !ph7_value_is_array(apArg[0]) ){` |
|       - | 3505 | `		/* Missing/Invalid arguments,return NULL */` |
|     ! 0 | 3506 | `		ph7_result_null(pCtx);` |
|     ! 0 | 3507 | `		return PH7_OK;` |
|       - | 3508 | `	}` |
|       - | 3509 | `	/* Point the internal representation of the target array */` |
|       3 | 3510 | `	pSrc = (ph7_hashmap *)apArg[0]->x.pOther;` |
|       - | 3511 | `	/* Get the offset */` |
|       3 | 3512 | `	iOfft = ph7_value_to_int(apArg[1]);` |
|       3 | 3513 | `	if( iOfft < 0 ){` |
|     ! 0 | 3514 | `		iOfft = (int)pSrc->nEntry + iOfft;` |
|     ! 0 | 3515 | `	}` |
|       3 | 3516 | `	if( iOfft < 0 \|\| iOfft > (int)pSrc->nEntry ){` |
|       - | 3517 | `		/* Invalid offset,remove the last entry */` |
|     ! 0 | 3518 | `		iOfft = (int)pSrc->nEntry - 1;` |
|     ! 0 | 3519 | `	}` |
|       - | 3520 | `	/* Get the length */` |
|       3 | 3521 | `	iLength = (int)pSrc->nEntry - iOfft;` |
|       3 | 3522 | `	if( nArg > 2 ){` |
|       3 | 3523 | `		iLength = ph7_value_to_int(apArg[2]);` |
|       3 | 3524 | `		if( iLength < 0 ){` |
|     ! 0 | 3525 | `			iLength = ((int)pSrc->nEntry + iLength) - iOfft;` |
|     ! 0 | 3526 | `		}` |
|       3 | 3527 | `		if( iLength < 0 \|\| iOfft + iLength >= (int)pSrc->nEntry ){` |
|     ! 0 | 3528 | `			iLength = (int)pSrc->nEntry - iOfft;` |
|     ! 0 | 3529 | `		}` |
|       1 | 3530 | `	}` |
|       - | 3531 | `	/* Create a new array */` |
|       3 | 3532 | `	pArray = ph7_context_new_array(pCtx);` |
|       3 | 3533 | `	if( pArray == 0 ){` |
|     ! 0 | 3534 | `		ph7_result_null(pCtx);` |
|     ! 0 | 3535 | `		return PH7_OK;` |
|       - | 3536 | `	}` |
|       3 | 3537 | `	if( iLength < 1 ){` |
|       - | 3538 | `		/* Don't bother processing,return the empty array */` |
|     ! 0 | 3539 | `		ph7_result_value(pCtx,pArray);` |
|     ! 0 | 3540 | `		return PH7_OK;` |
|       - | 3541 | `	}` |
|       - | 3542 | `	/* Point to the desired entry */` |
|       3 | 3543 | `	pCur = pSrc->pFirst;` |
|       2 | 3544 | `	for(;;){` |
|       5 | 3545 | `		if( iOfft < 1 ){` |
|       3 | 3546 | `			break;` |
|       - | 3547 | `		}` |
|       - | 3548 | `		/* Point to the next entry */` |
|       3 | 3549 | `		pCur = pCur->pPrev; /* Reverse link */` |
|       3 | 3550 | `		iOfft--;` |
|       1 | 3551 | `	}` |
|       3 | 3552 | `	pRep = 0;` |
|       3 | 3553 | `	if( nArg > 3 ){` |
|       3 | 3554 | `		if( !ph7_value_is_array(apArg[3]) ){` |
|       - | 3555 | `			/* Perform an array cast */` |
|     ! 0 | 3556 | `			PH7_MemObjToHashmap(apArg[3]);` |
|     ! 0 | 3557 | `			if(ph7_value_is_array(apArg[3])){` |
|     ! 0 | 3558 | `				pRep = (ph7_hashmap *)apArg[3]->x.pOther;` |
|     ! 0 | 3559 | `			}` |
|     ! 0 | 3560 | `		}else{` |
|       3 | 3561 | `			pRep = (ph7_hashmap *)apArg[3]->x.pOther;` |
|       - | 3562 | `		}` |
|       3 | 3563 | `		if( pRep ){` |
|       - | 3564 | `			/* Reset the loop cursor */` |
|       3 | 3565 | `			pRep->pCur = pRep->pFirst;` |
|       1 | 3566 | `		}` |
|       1 | 3567 | `	}` |
|       - | 3568 | `	/* Point to the internal representation of the hashmap */` |
|       3 | 3569 | `	pMap = (ph7_hashmap *)pArray->x.pOther;` |
|       3 | 3570 | `	for(;;){` |
|       7 | 3571 | `		if( iLength < 1 ){` |
|       3 | 3572 | `			break;` |
|       - | 3573 | `		}` |
|       5 | 3574 | `		pPrev = pCur->pPrev;` |
|       5 | 3575 | `		rc = HashmapInsertNode(pMap,pCur,FALSE);` |
|       5 | 3576 | `		if( pRep && (pRnode = PH7_HashmapGetNextEntry(pRep)) != 0 ){` |
|       - | 3577 | `			/* Extract node value */` |
|       5 | 3578 | `			pRvalue = HashmapExtractNodeValue(pRnode);` |
|       - | 3579 | `			/* Replace the old node */` |
|       5 | 3580 | `			pOld = HashmapExtractNodeValue(pCur);` |
|       5 | 3581 | `			if( pRvalue && pOld ){` |
|       5 | 3582 | `				PH7_MemObjStore(pRvalue,pOld);` |
|       2 | 3583 | `			}` |
|       3 | 3584 | `		}else{` |
|       - | 3585 | `			/* Unlink the node from the source hashmap */` |
|     ! 0 | 3586 | `			PH7_HashmapUnlinkNode(pCur,TRUE);` |
|       - | 3587 | `		}` |
|       5 | 3588 | `		if( rc != SXRET_OK ){` |
|     ! 0 | 3589 | `			break;` |
|       - | 3590 | `		}` |
|       - | 3591 | `		/* Point to the next entry */` |
|       5 | 3592 | `		pCur = pPrev; /* Reverse link */` |
|       5 | 3593 | `		iLength--;` |
|       1 | 3594 | `	}` |
|       3 | 3595 | `	if( pRep ){` |
|       3 | 3596 | `		while((pRnode = PH7_HashmapGetNextEntry(pRep)) != 0 ){` |
|     ! 0 | 3597 | `			HashmapInsertNode(pSrc,pRnode,FALSE);` |
|     ! 0 | 3598 | `		}` |
|       1 | 3599 | `	}` |
|       - | 3600 | `	/* Return the freshly created array */` |
|       3 | 3601 | `	ph7_result_value(pCtx,pArray);` |
|       3 | 3602 | `	return PH7_OK;` |
|       2 | 3603 |  |
|       - | 3604 | `/*` |
|       - | 3605 | ` * bool in_array(value $needle,array $haystack[,bool $strict = FALSE ])` |
|       - | 3606 | ` *  Checks if a value exists in an array.` |
|       - | 3607 | ` * Parameters` |
|       - | 3608 | ` *  $needle` |
|       - | 3609 | ` *   The searched value.` |
|       - | 3610 | ` *   Note:` |
|       - | 3611 | ` *    If needle is a string, the comparison is done in a case-sensitive manner.` |
|       - | 3612 | ` * $haystack` |
|       - | 3613 | ` *  The target array.` |
|       - | 3614 | ` * $strict` |
|       - | 3615 | ` *  If the third parameter strict is set to TRUE then the in_array() function` |
|       - | 3616 | ` *  will also check the types of the needle in the haystack.` |
|       - | 3617 | ` */` |
|   18604 | 3618 | `static int ph7_hashmap_in_array(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       2 | 3619 |  |
|       - | 3620 | `	ph7_value *pNeedle;` |
|       - | 3621 | `	int bStrict;` |
|       - | 3622 | `	int rc;` |
|   18606 | 3623 | `	if( nArg < 2 ){` |
|       - | 3624 | `		/* Missing argument,return FALSE */` |
|     ! 0 | 3625 | `		ph7_result_bool(pCtx,0);` |
|     ! 0 | 3626 | `		return PH7_OK;` |
|       - | 3627 | `	}` |
|   18606 | 3628 | `	pNeedle = apArg[0];` |
|   18606 | 3629 | `	bStrict = 0;` |
|   18606 | 3630 | `	if( nArg > 2 ){` |
|       5 | 3631 | `		bStrict = ph7_value_to_bool(apArg[2]);` |
|       2 | 3632 | `	}` |
|   18606 | 3633 | `	if( !ph7_value_is_array(apArg[1]) ){` |
|       - | 3634 | `		/* haystack must be an array,perform a standard comparison */` |
|     ! 0 | 3635 | `		rc = ph7_value_compare(pNeedle,apArg[1],bStrict);` |
|       - | 3636 | `		/* Set the comparison result */` |
|     ! 0 | 3637 | `		ph7_result_bool(pCtx,rc == 0);` |
|     ! 0 | 3638 | `		return PH7_OK;` |
|       - | 3639 | `	}` |
|       - | 3640 | `	/* Perform the lookup */` |
|   18606 | 3641 | `	rc = HashmapFindValue((ph7_hashmap *)apArg[1]->x.pOther,pNeedle,0,bStrict);` |
|       - | 3642 | `	/* Lookup result */` |
|   18606 | 3643 | `	ph7_result_bool(pCtx,rc == SXRET_OK);` |
|   18606 | 3644 | `	return PH7_OK;` |
|    9304 | 3645 |  |
|       - | 3646 | `/*` |
|       - | 3647 | ` * value array_search(value $needle,array $haystack[,bool $strict = false ])` |
|       - | 3648 | ` *  Searches the array for a given value and returns the corresponding key if successful.` |
|       - | 3649 | ` * Parameters` |
|       - | 3650 | ` * $needle` |
|       - | 3651 | ` *   The searched value.` |
|       - | 3652 | ` * $haystack` |
|       - | 3653 | ` *   The array.` |
|       - | 3654 | ` * $strict` |
|       - | 3655 | ` *  If the third parameter strict is set to TRUE then the array_search() function` |
|       - | 3656 | ` *  will search for identical elements in the haystack. This means it will also check` |
|       - | 3657 | ` *  the types of the needle in the haystack, and objects must be the same instance.` |
|       - | 3658 | ` * Return` |
|       - | 3659 | ` *  Returns the key for needle if it is found in the array, FALSE otherwise.` |
|       - | 3660 | ` */` |
|      26 | 3661 | `static int ph7_hashmap_search(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       1 | 3662 |  |
|       - | 3663 | `	ph7_hashmap_node *pEntry;` |
|       - | 3664 | `	ph7_value *pVal,sNeedle;` |
|       - | 3665 | `	ph7_hashmap *pMap;` |
|       - | 3666 | `	ph7_value sVal;` |
|       - | 3667 | `	int bStrict;` |
|       - | 3668 | `	sxu32 n;` |
|       - | 3669 | `	int rc;` |
|      27 | 3670 | `	if( nArg < 2 ){` |
|       - | 3671 | `		/* Missing argument,return FALSE*/` |
|     ! 0 | 3672 | `		ph7_result_bool(pCtx,0);` |
|     ! 0 | 3673 | `		return PH7_OK;` |
|       - | 3674 | `	}` |
|      27 | 3675 | `	bStrict = FALSE;` |
|      27 | 3676 | `	if( !ph7_value_is_array(apArg[1]) ){` |
|       - | 3677 | `		/* hasystack must be an array,return FALSE */` |
|       3 | 3678 | `		ph7_result_bool(pCtx,0);` |
|       3 | 3679 | `		return PH7_OK;` |
|       - | 3680 | `	}` |
|      25 | 3681 | `	if( nArg > 2 && ph7_value_is_bool(apArg[2]) ){` |
|      19 | 3682 | `		bStrict = ph7_value_to_bool(apArg[2]);` |
|       9 | 3683 | `	}` |
|       - | 3684 | `	/* Point to the internal representation of the internal hashmap */` |
|      25 | 3685 | `	pMap = (ph7_hashmap *)apArg[1]->x.pOther;` |
|       - | 3686 | `	/* Perform a linear search since we cannot sort the hashmap based on values */` |
|      25 | 3687 | `	PH7_MemObjInit(pMap->pVm,&sVal);` |
|      25 | 3688 | `	PH7_MemObjInit(pMap->pVm,&sNeedle);` |
|      25 | 3689 | `	pEntry = pMap->pFirst;` |
|      25 | 3690 | `	n = pMap->nEntry;` |
|      39 | 3691 | `	for(;;){` |
|      79 | 3692 | `		if( !n ){` |
|       7 | 3693 | `			break;` |
|       - | 3694 | `		}` |
|       - | 3695 | `		/* Extract node value */` |
|      73 | 3696 | `		pVal = HashmapExtractNodeValue(pEntry);` |
|      73 | 3697 | `		if( pVal ){` |
|       - | 3698 | `			/* Make a copy of the vuurent values since the comparison routine` |
|       - | 3699 | `			 * can change their type.` |
|       - | 3700 | `			 */` |
|      73 | 3701 | `			PH7_MemObjLoad(pVal,&sVal);` |
|      73 | 3702 | `			PH7_MemObjLoad(apArg[0],&sNeedle);` |
|      73 | 3703 | `			rc = PH7_MemObjCmp(&sNeedle,&sVal,bStrict,0);` |
|      73 | 3704 | `			PH7_MemObjRelease(&sVal);` |
|      73 | 3705 | `			PH7_MemObjRelease(&sNeedle);` |
|      73 | 3706 | `			if( rc == 0 ){` |
|       - | 3707 | `				/* Match found,return key */` |
|      19 | 3708 | `				if( pEntry->iType == HASHMAP_INT_NODE){` |
|       - | 3709 | `					/* INT key */` |
|      13 | 3710 | `					ph7_result_int64(pCtx,pEntry->xKey.iKey);` |
|       7 | 3711 | `				}else{` |
|       7 | 3712 | `					SyBlob *pKey = &pEntry->xKey.sKey;` |
|       - | 3713 | `					/* Blob key */` |
|       7 | 3714 | `					ph7_result_string(pCtx,(const char *)SyBlobData(pKey),(int)SyBlobLength(pKey));` |
|       - | 3715 | `				}` |
|      19 | 3716 | `				return PH7_OK;` |
|       - | 3717 | `			}` |
|      27 | 3718 | `		}` |
|       - | 3719 | `		/* Point to the next entry */` |
|      55 | 3720 | `		pEntry = pEntry->pPrev; /* Reverse link */` |
|      55 | 3721 | `		n--;` |
|       1 | 3722 | `	}` |
|       - | 3723 | `	/* No such value,return FALSE */` |
|       7 | 3724 | `	ph7_result_bool(pCtx,0);` |
|       7 | 3725 | `	return PH7_OK;` |
|      14 | 3726 |  |
|       - | 3727 | `/*` |
|       - | 3728 | ` * array array_diff(array $array1,array $array2,...)` |
|       - | 3729 | ` *  Computes the difference of arrays.` |
|       - | 3730 | ` * Parameters` |
|       - | 3731 | ` *  $array1` |
|       - | 3732 | ` *    The array to compare from` |
|       - | 3733 | ` *  $array2` |
|       - | 3734 | ` *    An array to compare against` |
|       - | 3735 | ` *  $...` |
|       - | 3736 | ` *   More arrays to compare against` |
|       - | 3737 | ` * Return` |
|       - | 3738 | ` *  Returns an array containing all the entries from array1 that` |
|       - | 3739 | ` *  are not present in any of the other arrays.` |
|       - | 3740 | ` */` |
|      10 | 3741 | `static int ph7_hashmap_diff(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       2 | 3742 |  |
|       - | 3743 | `	ph7_hashmap_node *pEntry;` |
|       - | 3744 | `	ph7_hashmap *pSrc,*pMap;` |
|       - | 3745 | `	ph7_value *pArray;` |
|       - | 3746 | `	ph7_value *pVal;` |
|       - | 3747 | `	sxi32 rc;` |
|       - | 3748 | `	sxu32 n;` |
|       - | 3749 | `	int i;` |
|       - | 3750 | `	/* Validate arguments to mimic PHP behaviour. Earlier versions simply` |
|       - | 3751 | `	 * returned NULL when the caller passed invalid parameters which made` |
|       - | 3752 | `	 * debugging difficult. */` |
|      12 | 3753 | `	if( nArg < 1 ){` |
|       4 | 3754 | `		return PH7_VmThrowException(pCtx,` |
|       - | 3755 | `			"ArgumentCountError",` |
|       - | 3756 | `			"array_diff() expects at least 1 argument, %d given",` |
|       1 | 3757 | `			nArg` |
|       - | 3758 | `			);` |
|       - | 3759 | `	}` |
|      10 | 3760 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|       4 | 3761 | `		return PH7_VmThrowException(pCtx,` |
|       - | 3762 | `			"TypeError",` |
|       - | 3763 | `			"array_diff(): Argument #1 ($array) must be of type array, %s given",` |
|       1 | 3764 | `			ph7_type_name(apArg[0])` |
|       - | 3765 | `			);` |
|       - | 3766 | `	}` |
|      14 | 3767 | `	for(i = 1 ; i < nArg ; i++){` |
|      10 | 3768 | `		if( !ph7_value_is_array(apArg[i]) ){` |
|       4 | 3769 | `			return PH7_VmThrowException(pCtx,` |
|       - | 3770 | `				"TypeError",` |
|       - | 3771 | `				"array_diff(): Argument #%d must be of type array, %s given",` |
|       1 | 3772 | `				i + 1,` |
|       2 | 3773 | `				ph7_type_name(apArg[i])` |
|       - | 3774 | `				);` |
|       - | 3775 | `		}` |
|       4 | 3776 | `	}` |
|       5 | 3777 | `	if( nArg == 1 ){` |
|       - | 3778 | `		/* Return the first array since we cannot perform a diff */` |
|     ! 0 | 3779 | `		ph7_result_value(pCtx,apArg[0]);` |
|     ! 0 | 3780 | `		return PH7_OK;` |
|       - | 3781 | `	}` |
|       - | 3782 | `	/* Create a new array */` |
|       5 | 3783 | `	pArray = ph7_context_new_array(pCtx);` |
|       5 | 3784 | `	if( pArray == 0 ){` |
|     ! 0 | 3785 | `		ph7_result_null(pCtx);` |
|     ! 0 | 3786 | `		return PH7_OK;` |
|       - | 3787 | `	}` |
|       - | 3788 | `	/* Point to the internal representation of the source hashmap */` |
|       5 | 3789 | `	pSrc = (ph7_hashmap *)apArg[0]->x.pOther;` |
|       - | 3790 | `	/* Perform the diff */` |
|       5 | 3791 | `	pEntry = pSrc->pFirst;` |
|       5 | 3792 | `	n = pSrc->nEntry;` |
|       8 | 3793 | `	for(;;){` |
|      17 | 3794 | `		if( n < 1 ){` |
|       5 | 3795 | `			break;` |
|       - | 3796 | `		}` |
|       - | 3797 | `		/* Extract the node value */` |
|      13 | 3798 | `		pVal = HashmapExtractNodeValue(pEntry);` |
|      13 | 3799 | `		if( pVal ){` |
|      23 | 3800 | `			for( i = 1 ; i < nArg ; i++ ){` |
|      17 | 3801 | `				if( !ph7_value_is_array(apArg[i])) {` |
|       - | 3802 | `					/* ignore */` |
|     ! 0 | 3803 | `					continue;` |
|       - | 3804 | `				}` |
|       - | 3805 | `				/* Point to the internal representation of the hashmap */` |
|      17 | 3806 | `				pMap = (ph7_hashmap *)apArg[i]->x.pOther;` |
|       - | 3807 | `				/* Perform the lookup */` |
|      17 | 3808 | `				rc = HashmapFindValue(pMap,pVal,0,TRUE);` |
|      17 | 3809 | `				if( rc == SXRET_OK ){` |
|       - | 3810 | `					/* Value exist */` |
|       7 | 3811 | `					break;` |
|       - | 3812 | `				}` |
|       6 | 3813 | `			}` |
|      13 | 3814 | `			if( i >= nArg ){` |
|       - | 3815 | `				/* Perform the insertion */` |
|       7 | 3816 | `				HashmapInsertNode((ph7_hashmap *)pArray->x.pOther,pEntry,TRUE);` |
|       3 | 3817 | `			}` |
|       6 | 3818 | `		}` |
|       - | 3819 | `		/* Point to the next entry */` |
|      13 | 3820 | `		pEntry = pEntry->pPrev; /* Reverse link */` |
|      13 | 3821 | `		n--;` |
|       1 | 3822 | `	}` |
|       - | 3823 | `	/* Return the freshly created array */` |
|       5 | 3824 | `	ph7_result_value(pCtx,pArray);` |
|       5 | 3825 | `	return PH7_OK;` |
|       7 | 3826 |  |
|       - | 3827 | `/*` |
|       - | 3828 | ` * array array_udiff(array $array1,array $array2,...,$callback)` |
|       - | 3829 | ` *  Computes the difference of arrays by using a callback function for data comparison.` |
|       - | 3830 | ` * Parameters` |
|       - | 3831 | ` *  $array1` |
|       - | 3832 | ` *    The array to compare from` |
|       - | 3833 | ` *  $array2` |
|       - | 3834 | ` *    An array to compare against` |
|       - | 3835 | ` *  $...` |
|       - | 3836 | ` *   More arrays to compare against.` |
|       - | 3837 | ` * $callback` |
|       - | 3838 | ` *  The callback comparison function.` |
|       - | 3839 | ` *  The comparison function must return an integer less than, equal to, or greater than zero` |
|       - | 3840 | ` *  if the first argument is considered to be respectively less than, equal to, or greater` |
|       - | 3841 | ` *  than the second.` |
|       - | 3842 | ` *     int callback ( mixed $a, mixed $b )` |
|       - | 3843 | ` * Return` |
|       - | 3844 | ` *  Returns an array containing all the entries from array1 that` |
|       - | 3845 | ` *  are not present in any of the other arrays.` |
|       - | 3846 | ` */` |
|       2 | 3847 | `static int ph7_hashmap_udiff(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       1 | 3848 |  |
|       - | 3849 | `	ph7_hashmap_node *pEntry;` |
|       - | 3850 | `	ph7_hashmap *pSrc,*pMap;` |
|       - | 3851 | `	ph7_value *pCallback;` |
|       - | 3852 | `	ph7_value *pArray;` |
|       - | 3853 | `	ph7_value *pVal;` |
|       - | 3854 | `	sxi32 rc;` |
|       - | 3855 | `	sxu32 n;` |
|       - | 3856 | `	int i;` |
|       3 | 3857 | `	if( nArg < 2 \|\| !ph7_value_is_array(apArg[0]) ){` |
|       - | 3858 | `		/* Missing/Invalid arguments,return NULL */` |
|     ! 0 | 3859 | `		ph7_result_null(pCtx);` |
|     ! 0 | 3860 | `		return PH7_OK;` |
|       - | 3861 | `	}` |
|       - | 3862 | `	/* Point to the callback */` |
|       3 | 3863 | `	pCallback = apArg[nArg - 1];` |
|       3 | 3864 | `	if( nArg == 2 ){` |
|       - | 3865 | `		/* Return the first array since we cannot perform a diff */` |
|     ! 0 | 3866 | `		ph7_result_value(pCtx,apArg[0]);` |
|     ! 0 | 3867 | `		return PH7_OK;` |
|       - | 3868 | `	}` |
|       - | 3869 | `	/* Create a new array */` |
|       3 | 3870 | `	pArray = ph7_context_new_array(pCtx);` |
|       3 | 3871 | `	if( pArray == 0 ){` |
|     ! 0 | 3872 | `		ph7_result_null(pCtx);` |
|     ! 0 | 3873 | `		return PH7_OK;` |
|       - | 3874 | `	}` |
|       - | 3875 | `	/* Point to the internal representation of the source hashmap */` |
|       3 | 3876 | `	pSrc = (ph7_hashmap *)apArg[0]->x.pOther;` |
|       - | 3877 | `	/* Perform the diff */` |
|       3 | 3878 | `	pEntry = pSrc->pFirst;` |
|       3 | 3879 | `	n = pSrc->nEntry;` |
|       4 | 3880 | `	for(;;){` |
|       9 | 3881 | `		if( n < 1 ){` |
|       3 | 3882 | `			break;` |
|       - | 3883 | `		}` |
|       - | 3884 | `		/* Extract the node value */` |
|       7 | 3885 | `		pVal = HashmapExtractNodeValue(pEntry);` |
|       7 | 3886 | `		if( pVal ){` |
|      11 | 3887 | `			for( i = 1 ; i < nArg - 1; i++ ){` |
|       7 | 3888 | `				if( !ph7_value_is_array(apArg[i])) {` |
|       - | 3889 | `					/* ignore */` |
|     ! 0 | 3890 | `					continue;` |
|       - | 3891 | `				}` |
|       - | 3892 | `				/* Point to the internal representation of the hashmap */` |
|       7 | 3893 | `				pMap = (ph7_hashmap *)apArg[i]->x.pOther;` |
|       - | 3894 | `				/* Perform the lookup */` |
|       7 | 3895 | `				rc = HashmapFindValueByCallback(pMap,pVal,pCallback,0);` |
|       7 | 3896 | `				if( rc == SXRET_OK ){` |
|       - | 3897 | `					/* Value exist */` |
|       3 | 3898 | `					break;` |
|       - | 3899 | `				}` |
|       3 | 3900 | `			}` |
|       7 | 3901 | `			if( i >= (nArg - 1)){` |
|       - | 3902 | `				/* Perform the insertion */` |
|       5 | 3903 | `				HashmapInsertNode((ph7_hashmap *)pArray->x.pOther,pEntry,TRUE);` |
|       2 | 3904 | `			}` |
|       3 | 3905 | `		}` |
|       - | 3906 | `		/* Point to the next entry */` |
|       7 | 3907 | `		pEntry = pEntry->pPrev; /* Reverse link */` |
|       7 | 3908 | `		n--;` |
|       1 | 3909 | `	}` |
|       - | 3910 | `	/* Return the freshly created array */` |
|       3 | 3911 | `	ph7_result_value(pCtx,pArray);` |
|       3 | 3912 | `	return PH7_OK;` |
|       2 | 3913 |  |
|       - | 3914 | `/*` |
|       - | 3915 | ` * array array_diff_assoc(array $array1,array $array2,...)` |
|       - | 3916 | ` *  Computes the difference of arrays with additional index check.` |
|       - | 3917 | ` * Parameters` |
|       - | 3918 | ` *  $array1` |
|       - | 3919 | ` *    The array to compare from` |
|       - | 3920 | ` *  $array2` |
|       - | 3921 | ` *    An array to compare against` |
|       - | 3922 | ` *  $...` |
|       - | 3923 | ` *   More arrays to compare against` |
|       - | 3924 | ` * Return` |
|       - | 3925 | ` *  Returns an array containing all the entries from array1 that` |
|       - | 3926 | ` *  are not present in any of the other arrays.` |
|       - | 3927 | ` */` |
|      20 | 3928 | `static int ph7_hashmap_diff_assoc(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       2 | 3929 |  |
|       - | 3930 | `	ph7_hashmap_node *pN1,*pN2,*pEntry;` |
|       - | 3931 | `	ph7_hashmap *pSrc,*pMap;` |
|       - | 3932 | `	ph7_value *pArray;` |
|       - | 3933 | `	ph7_value *pVal;` |
|       - | 3934 | `	sxi32 rc;` |
|       - | 3935 | `	sxu32 n;` |
|       - | 3936 | `	int i;` |
|       - | 3937 | `	/* Ensure the argument list is valid, emitting the same errors PHP` |
|       - | 3938 | `	 * would produce. This makes behaviour predictable and allows the` |
|       - | 3939 | `	 * accompanying integration tests to pass. */` |
|      22 | 3940 | `	if( nArg < 1 ){` |
|       4 | 3941 | `		return PH7_VmThrowException(pCtx,` |
|       - | 3942 | `			"ArgumentCountError",` |
|       - | 3943 | `			"array_diff_assoc() expects at least 1 argument, %d given",` |
|       1 | 3944 | `			nArg` |
|       - | 3945 | `			);` |
|       - | 3946 | `	}` |
|      20 | 3947 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|       4 | 3948 | `		return PH7_VmThrowException(pCtx,` |
|       - | 3949 | `			"TypeError",` |
|       - | 3950 | `			"array_diff_assoc(): Argument #1 ($array) must be of type array, %s given",` |
|       1 | 3951 | `			ph7_type_name(apArg[0])` |
|       - | 3952 | `			);` |
|       - | 3953 | `	}` |
|      32 | 3954 | `	for(i = 1 ; i < nArg ; i++){` |
|      20 | 3955 | `		if( !ph7_value_is_array(apArg[i]) ){` |
|       7 | 3956 | `			return PH7_VmThrowException(pCtx,` |
|       - | 3957 | `				"TypeError",` |
|       - | 3958 | `				"array_diff_assoc(): Argument #%d must be of type array, %s given",` |
|       2 | 3959 | `				i + 1,` |
|       4 | 3960 | `				ph7_type_name(apArg[i])` |
|       - | 3961 | `				);` |
|       - | 3962 | `		}` |
|       9 | 3963 | `	}` |
|      13 | 3964 | `	if( nArg == 1 ){` |
|       - | 3965 | `		/* Return the first array since we cannot perform a diff */` |
|       3 | 3966 | `		ph7_result_value(pCtx,apArg[0]);` |
|       3 | 3967 | `		return PH7_OK;` |
|       - | 3968 | `	}` |
|       - | 3969 | `	/* Create a new array */` |
|      11 | 3970 | `	pArray = ph7_context_new_array(pCtx);` |
|      11 | 3971 | `	if( pArray == 0 ){` |
|     ! 0 | 3972 | `		ph7_result_null(pCtx);` |
|     ! 0 | 3973 | `		return PH7_OK;` |
|       - | 3974 | `	}` |
|       - | 3975 | `	/* Point to the internal representation of the source hashmap */` |
|      11 | 3976 | `	pSrc = (ph7_hashmap *)apArg[0]->x.pOther;` |
|       - | 3977 | `	/* Perform the diff */` |
|      11 | 3978 | `	pEntry = pSrc->pFirst;` |
|      11 | 3979 | `	n = pSrc->nEntry;` |
|      11 | 3980 | `	pN1 = pN2 = 0;` |
|      29 | 3981 | `	for(;;){` |
|       - | 3982 | `		int keep;` |
|      35 | 3983 | `		if( n < 1 ){` |
|      11 | 3984 | `			break;` |
|       - | 3985 | `		}` |
|       - | 3986 | `		/* assume the element should be kept until we find a match */` |
|      25 | 3987 | `		keep = 1;` |
|      41 | 3988 | `		for( i = 1 ; i < nArg ; i++ ){` |
|       - | 3989 | `			/* all arguments have been validated already, so cast directly */` |
|      29 | 3990 | `			pMap = (ph7_hashmap *)apArg[i]->x.pOther;` |
|       - | 3991 | `			/* Perform a key lookup first */` |
|      29 | 3992 | `			if( pEntry->iType == HASHMAP_INT_NODE ){` |
|      13 | 3993 | `				rc = HashmapLookupIntKey(pMap,pEntry->xKey.iKey,&pN1);` |
|       7 | 3994 | `			}else{` |
|      17 | 3995 | `				rc = HashmapLookupBlobKey(pMap,SyBlobData(&pEntry->xKey.sKey),SyBlobLength(&pEntry->xKey.sKey),&pN1);` |
|       - | 3996 | `			}` |
|      29 | 3997 | `			if( rc != SXRET_OK ){` |
|       - | 3998 | `				/* this array does not contain the key, continue checking others */` |
|      15 | 3999 | `				continue;` |
|       - | 4000 | `			}` |
|       - | 4001 | `			/* key exists; check that value stored in the matching node is equal */` |
|      15 | 4002 | `			pVal = HashmapExtractNodeValue(pEntry);` |
|      15 | 4003 | `			if( pVal ){` |
|       - | 4004 | `				/* directly compare with value at pN1 rather than searching again */` |
|      15 | 4005 | `				ph7_value *pVal2 = HashmapExtractNodeValue(pN1);` |
|      15 | 4006 | `				if( pVal2 ){` |
|      15 | 4007 | `					sxi32 cmp = PH7_MemObjCmp(pVal, pVal2, TRUE, 0);` |
|      15 | 4008 | `					if( cmp == 0 ){` |
|       - | 4009 | `						/* identical key+value found in one of the arrays => drop it */` |
|      13 | 4010 | `						keep = 0;` |
|      13 | 4011 | `						break;` |
|       - | 4012 | `					}` |
|       1 | 4013 | `				}` |
|       1 | 4014 | `			}` |
|       2 | 4015 | `		}` |
|      25 | 4016 | `		if( keep ){` |
|       - | 4017 | `			/* Perform the insertion */` |
|      13 | 4018 | `			HashmapInsertNode((ph7_hashmap *)pArray->x.pOther,pEntry,TRUE);` |
|       6 | 4019 | `		}` |
|       - | 4020 | `		/* Point to the next entry */` |
|      25 | 4021 | `		pEntry = pEntry->pPrev; /* Reverse link */` |
|      25 | 4022 | `		n--;` |
|       1 | 4023 | `	}` |
|       - | 4024 | `	/* Return the freshly created array */` |
|      11 | 4025 | `	ph7_result_value(pCtx,pArray);` |
|      11 | 4026 | `	return PH7_OK;` |
|      12 | 4027 |  |
|       - | 4028 | `/*` |
|       - | 4029 | ` * array array_diff_uassoc(array $array1,array $array2,...,callback $key_compare_func)` |
|       - | 4030 | ` *  Computes the difference of arrays with additional index check which is performed` |
|       - | 4031 | ` *  by a user supplied callback function.` |
|       - | 4032 | ` * Parameters` |
|       - | 4033 | ` *  $array1` |
|       - | 4034 | ` *    The array to compare from` |
|       - | 4035 | ` *  $array2` |
|       - | 4036 | ` *    An array to compare against` |
|       - | 4037 | ` *  $...` |
|       - | 4038 | ` *   More arrays to compare against.` |
|       - | 4039 | ` *  $key_compare_func` |
|       - | 4040 | ` *   Callback function to use. The callback function must return an integer` |
|       - | 4041 | ` *   less than, equal to, or greater than zero if the first argument is considered` |
|       - | 4042 | ` *   to be respectively less than, equal to, or greater than the second.` |
|       - | 4043 | ` * Return` |
|       - | 4044 | ` *  Returns an array containing all the entries from array1 that` |
|       - | 4045 | ` *  are not present in any of the other arrays.` |
|       - | 4046 | ` */` |
|      22 | 4047 | `static int ph7_hashmap_diff_uassoc(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       2 | 4048 |  |
|       - | 4049 | `	ph7_hashmap_node *pEntry;` |
|       - | 4050 | `	ph7_hashmap *pSrc,*pMap;` |
|       - | 4051 | `	ph7_value *pCallback;` |
|       - | 4052 | `	ph7_value *pArray;` |
|       - | 4053 | `	sxi32 rc;` |
|       - | 4054 | `	sxu32 n;` |
|       - | 4055 | `	int i;` |
|       - | 4056 |  |
|       - | 4057 | `	/* Argument validation mimicking PHP errors. */` |
|      24 | 4058 | `	if( nArg < 2 ){` |
|       4 | 4059 | `		return PH7_VmThrowException(pCtx,` |
|       - | 4060 | `			"ArgumentCountError",` |
|       - | 4061 | `			"array_diff_uassoc() expects at least 2 arguments, %d given",` |
|       1 | 4062 | `			nArg` |
|       - | 4063 | `			);` |
|       - | 4064 | `	}` |
|      22 | 4065 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|       4 | 4066 | `		return PH7_VmThrowException(pCtx,` |
|       - | 4067 | `			"TypeError",` |
|       - | 4068 | `			"array_diff_uassoc(): Argument #1 ($array) must be of type array, %s given",` |
|       1 | 4069 | `			ph7_type_name(apArg[0])` |
|       - | 4070 | `			);` |
|       - | 4071 | `	}` |
|       - | 4072 | `	/* Intermediate arguments (except last) must be arrays. Last argument is` |
|       - | 4073 | `	 * expected to be a callback. */` |
|      32 | 4074 | `	for(i = 1 ; i < nArg - 1; i++){` |
|      16 | 4075 | `		if( !ph7_value_is_array(apArg[i]) ){` |
|       4 | 4076 | `			return PH7_VmThrowException(pCtx,` |
|       - | 4077 | `				"TypeError",` |
|       - | 4078 | `				"array_diff_uassoc(): Argument #%d must be of type array, %s given",` |
|       1 | 4079 | `				i + 1,` |
|       2 | 4080 | `				ph7_type_name(apArg[i])` |
|       - | 4081 | `				);` |
|       - | 4082 | `		}` |
|       8 | 4083 | `	}` |
|       - | 4084 | `	/* Point to the callback value */` |
|      18 | 4085 | `	pCallback = apArg[nArg - 1];` |
|      18 | 4086 | `	if( !ph7_value_is_callable(pCallback) ){` |
|       - | 4087 | `		/* Compose an error message that closely matches PHP output. When the` |
|       - | 4088 | `		 * argument is an array of the wrong shape we include an extra clause.` |
|       - | 4089 | `		 * If the value is neither array nor string, PHP says "no array or` |
|       - | 4090 | `		 * string given" which we also reproduce. */` |
|       7 | 4091 | `		if( ph7_value_is_array(pCallback) ){` |
|       - | 4092 | `			/* ARRAY CALLBACK must have exactly two members */` |
|       4 | 4093 | `			return PH7_VmThrowException(pCtx,` |
|       - | 4094 | `				"TypeError",` |
|       - | 4095 | `				"array_diff_uassoc(): Argument #%d must be a valid callback, array callback must have exactly two members",` |
|       1 | 4096 | `				nArg` |
|       - | 4097 | `				);` |
|       - | 4098 | `		}` |
|       5 | 4099 | `		if( !ph7_value_is_string(pCallback) ){` |
|       - | 4100 | `			/* neither array nor string */` |
|       7 | 4101 | `			return PH7_VmThrowException(pCtx,` |
|       - | 4102 | `				"TypeError",` |
|       - | 4103 | `				"array_diff_uassoc(): Argument #%d must be a valid callback, no array or string given",` |
|       2 | 4104 | `				nArg` |
|       - | 4105 | `				);` |
|       - | 4106 | `		}` |
|       - | 4107 | `		/* Fallback for string (non-callable) or other leftover cases */` |
|     ! 0 | 4108 | `		return PH7_VmThrowException(pCtx,` |
|       - | 4109 | `			"TypeError",` |
|       - | 4110 | `			"array_diff_uassoc(): Argument #%d must be a valid callback, %s given",` |
|     ! 0 | 4111 | `			nArg,` |
|     ! 0 | 4112 | `			ph7_type_name(pCallback)` |
|       - | 4113 | `			);` |
|       - | 4114 | `	}` |
|      11 | 4115 | `	if( nArg == 2 ){` |
|       - | 4116 | `		/* If we only have the first array and the callback, just return the` |
|       - | 4117 | `		 * input array. */` |
|       3 | 4118 | `		ph7_result_value(pCtx,apArg[0]);` |
|       3 | 4119 | `		return PH7_OK;` |
|       - | 4120 | `	}` |
|       - | 4121 | `	/* Create a new array */` |
|       9 | 4122 | `	pArray = ph7_context_new_array(pCtx);` |
|       9 | 4123 | `	if( pArray == 0 ){` |
|     ! 0 | 4124 | `		ph7_result_null(pCtx);` |
|     ! 0 | 4125 | `		return PH7_OK;` |
|       - | 4126 | `	}` |
|       - | 4127 | `	/* Point to the internal representation of the source hashmap */` |
|       9 | 4128 | `	pSrc = (ph7_hashmap *)apArg[0]->x.pOther;` |
|       - | 4129 | `	/* Perform the diff */` |
|       9 | 4130 | `	pEntry = pSrc->pFirst;` |
|       9 | 4131 | `	n = pSrc->nEntry;` |
|      20 | 4132 | `	for(;;){` |
|       - | 4133 | `		int keep;` |
|      25 | 4134 | `		if( n < 1 ){` |
|       9 | 4135 | `			break;` |
|       - | 4136 | `		}` |
|      17 | 4137 | `		keep = 1;` |
|      29 | 4138 | `		for( i = 1 ; i < nArg - 1; i++ ){` |
|       - | 4139 | `			/* each of these must already be arrays thanks to earlier validation */` |
|      21 | 4140 | `			pMap = (ph7_hashmap *)apArg[i]->x.pOther;` |
|       - | 4141 | `			/* we must compare keys via callback, not by direct lookup */` |
|      21 | 4142 | `			ph7_hashmap_node *pIt = pMap->pFirst;` |
|      43 | 4143 | `			while( pIt ){` |
|       - | 4144 | `				/* build temporary key values for callback */` |
|       - | 4145 | `				ph7_value key1, key2, result;` |
|       - | 4146 | `				/* initialise only once using the appropriate helper */` |
|      31 | 4147 | `				if( pEntry->iType == HASHMAP_INT_NODE ){` |
|     ! 0 | 4148 | `					PH7_MemObjInitFromInt(pMap->pVm,&key1,pEntry->xKey.iKey);` |
|     ! 0 | 4149 | `				}else{` |
|       - | 4150 | `					SyString sStr;` |
|      31 | 4151 | `					SyStringInitFromBuf(&sStr,` |
|       - | 4152 | `						SyBlobData(&pEntry->xKey.sKey),` |
|       - | 4153 | `						SyBlobLength(&pEntry->xKey.sKey));` |
|      31 | 4154 | `					PH7_MemObjInitFromString(pMap->pVm,&key1,&sStr);` |
|       - | 4155 | `				}` |
|      31 | 4156 | `				if( pIt->iType == HASHMAP_INT_NODE ){` |
|     ! 0 | 4157 | `					PH7_MemObjInitFromInt(pMap->pVm,&key2,pIt->xKey.iKey);` |
|     ! 0 | 4158 | `				}else{` |
|       - | 4159 | `					SyString sStr;` |
|      31 | 4160 | `					SyStringInitFromBuf(&sStr,` |
|       - | 4161 | `						SyBlobData(&pIt->xKey.sKey),` |
|       - | 4162 | `						SyBlobLength(&pIt->xKey.sKey));` |
|      31 | 4163 | `					PH7_MemObjInitFromString(pMap->pVm,&key2,&sStr);` |
|       - | 4164 | `				}` |
|      31 | 4165 | `				PH7_MemObjInit(pMap->pVm,&result);` |
|       - | 4166 | `				/* call user callback with (key1, key2) */` |
|       - | 4167 | `				{` |
|       - | 4168 | `					ph7_value *apK[2];` |
|      31 | 4169 | `					apK[0] = &key1;` |
|      31 | 4170 | `					apK[1] = &key2;` |
|      31 | 4171 | `					rc = PH7_VmCallUserFunction(pMap->pVm,pCallback,2,apK,&result);` |
|       - | 4172 | `				}` |
|      31 | 4173 | `				if( rc == SXRET_OK ){` |
|      31 | 4174 | `					if( (result.iFlags & MEMOBJ_INT) == 0 ){` |
|     ! 0 | 4175 | `						PH7_MemObjToInteger(&result);` |
|     ! 0 | 4176 | `					}` |
|      31 | 4177 | `					if( result.x.iVal == 0 ){` |
|       - | 4178 | `						/* keys considered equal by callback; now compare values */` |
|      13 | 4179 | `						ph7_value *pVal1 = HashmapExtractNodeValue(pEntry);` |
|      13 | 4180 | `						ph7_value *pVal2 = HashmapExtractNodeValue(pIt);` |
|      13 | 4181 | `						if( pVal1 && pVal2 ){` |
|      13 | 4182 | `							if( PH7_MemObjCmp(pVal1,pVal2,TRUE,0) == 0 ){` |
|       9 | 4183 | `								keep = 0;` |
|       9 | 4184 | `								PH7_MemObjRelease(&result);` |
|       - | 4185 | `								/* release keys too before breaking */` |
|       9 | 4186 | `								PH7_MemObjRelease(&key1);` |
|       9 | 4187 | `								PH7_MemObjRelease(&key2);` |
|       9 | 4188 | `								break;` |
|       - | 4189 | `							}` |
|       2 | 4190 | `						}` |
|       2 | 4191 | `					}` |
|      11 | 4192 | `				}` |
|      23 | 4193 | `				PH7_MemObjRelease(&result);` |
|      23 | 4194 | `				PH7_MemObjRelease(&key1);` |
|      23 | 4195 | `				PH7_MemObjRelease(&key2);` |
|       - | 4196 | `				/* move to next node */` |
|      23 | 4197 | `				pIt = pIt->pPrev;` |
|      23 | 4198 | `				if( keep == 0 ) break;` |
|       1 | 4199 | `			}` |
|      21 | 4200 | `			if( keep == 0 ) break;` |
|       7 | 4201 | `		}` |
|      17 | 4202 | `		if( keep ){` |
|       - | 4203 | `			/* Perform the insertion */` |
|       9 | 4204 | `			HashmapInsertNode((ph7_hashmap *)pArray->x.pOther,pEntry,TRUE);` |
|       4 | 4205 | `		}` |
|       - | 4206 | `		/* Point to the next entry */` |
|      17 | 4207 | `		pEntry = pEntry->pPrev; /* Reverse link */` |
|      17 | 4208 | `		n--;` |
|       1 | 4209 | `	}` |
|       - | 4210 | `	/* Return the freshly created array */` |
|       9 | 4211 | `	ph7_result_value(pCtx,pArray);` |
|       9 | 4212 | `	return PH7_OK;` |
|      13 | 4213 |  |
|       - | 4214 | `/*` |
|       - | 4215 | ` * array array_diff_key(array $array1 ,array $array2,...)` |
|       - | 4216 | ` *  Computes the difference of arrays using keys for comparison.` |
|       - | 4217 | ` * Parameters` |
|       - | 4218 | ` *  $array1` |
|       - | 4219 | ` *    The array to compare from` |
|       - | 4220 | ` *  $array2` |
|       - | 4221 | ` *    An array to compare against` |
|       - | 4222 | ` *  $...` |
|       - | 4223 | ` *   More arrays to compare against` |
|       - | 4224 | ` * Return` |
|       - | 4225 | ` *  Returns an array containing all the entries from array1 whose keys are not present` |
|       - | 4226 | ` *  in any of the other arrays.` |
|       - | 4227 | ` * Note that NULL is returned on failure.` |
|       - | 4228 | ` */` |
|      14 | 4229 | `static int ph7_hashmap_diff_key(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       2 | 4230 |  |
|       - | 4231 | `	ph7_hashmap_node *pEntry;` |
|       - | 4232 | `	ph7_hashmap *pSrc,*pMap;` |
|       - | 4233 | `	ph7_value *pArray;` |
|       - | 4234 | `	sxi32 rc;` |
|       - | 4235 | `	sxu32 n;` |
|       - | 4236 | `	int i;` |
|       - | 4237 | `	/* Validate arguments to mirror PHP behaviour. Previously invalid inputs` |
|       - | 4238 | `	 * would quietly return NULL which is inconsistent with other hashmap` |
|       - | 4239 | `	 * helpers. */` |
|      16 | 4240 | `	if( nArg < 1 ){` |
|       4 | 4241 | `		return PH7_VmThrowException(pCtx,` |
|       - | 4242 | `			"ArgumentCountError",` |
|       - | 4243 | `			"array_diff_key() expects at least 1 argument, %d given",` |
|       1 | 4244 | `			nArg` |
|       - | 4245 | `			);` |
|       - | 4246 | `	}` |
|      14 | 4247 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|       4 | 4248 | `		return PH7_VmThrowException(pCtx,` |
|       - | 4249 | `			"TypeError",` |
|       - | 4250 | `			"array_diff_key(): Argument #1 ($array) must be of type array, %s given",` |
|       1 | 4251 | `			ph7_type_name(apArg[0])` |
|       - | 4252 | `			);` |
|       - | 4253 | `	}` |
|      20 | 4254 | `	for(i = 1 ; i < nArg ; i++){` |
|      12 | 4255 | `		if( !ph7_value_is_array(apArg[i]) ){` |
|       4 | 4256 | `			return PH7_VmThrowException(pCtx,` |
|       - | 4257 | `				"TypeError",` |
|       - | 4258 | `				"array_diff_key(): Argument #%d must be of type array, %s given",` |
|       1 | 4259 | `				i + 1,` |
|       2 | 4260 | `				ph7_type_name(apArg[i])` |
|       - | 4261 | `				);` |
|       - | 4262 | `		}` |
|       5 | 4263 | `	}` |
|       9 | 4264 | `	if( nArg == 1 ){` |
|       - | 4265 | `		/* Return the first array since we cannot perform a diff */` |
|       3 | 4266 | `		ph7_result_value(pCtx,apArg[0]);` |
|       3 | 4267 | `		return PH7_OK;` |
|       - | 4268 | `	}` |
|       - | 4269 | `	/* Create a new array */` |
|       7 | 4270 | `	pArray = ph7_context_new_array(pCtx);` |
|       7 | 4271 | `	if( pArray == 0 ){` |
|     ! 0 | 4272 | `		ph7_result_null(pCtx);` |
|     ! 0 | 4273 | `		return PH7_OK;` |
|       - | 4274 | `	}` |
|       - | 4275 | `	/* Point to the internal representation of the main hashmap */` |
|       7 | 4276 | `	pSrc = (ph7_hashmap *)apArg[0]->x.pOther;` |
|       - | 4277 | `	/* Perfrom the diff */` |
|       7 | 4278 | `	pEntry = pSrc->pFirst;` |
|       7 | 4279 | `	n = pSrc->nEntry;` |
|      12 | 4280 | `	for(;;){` |
|      25 | 4281 | `		if( n < 1 ){` |
|       7 | 4282 | `			break;` |
|       - | 4283 | `		}` |
|      31 | 4284 | `		for( i = 1 ; i < nArg ; i++ ){` |
|      23 | 4285 | `			if( !ph7_value_is_array(apArg[i])) {` |
|       - | 4286 | `				/* ignore */` |
|     ! 0 | 4287 | `				continue;` |
|       - | 4288 | `			}` |
|      23 | 4289 | `			pMap = (ph7_hashmap *)apArg[i]->x.pOther;` |
|      23 | 4290 | `			if( pEntry->iType == HASHMAP_BLOB_NODE ){` |
|      17 | 4291 | `				SyBlob *pKey = &pEntry->xKey.sKey;` |
|       - | 4292 | `				/* Blob lookup */` |
|      17 | 4293 | `				rc = HashmapLookupBlobKey(pMap,SyBlobData(pKey),SyBlobLength(pKey),0);` |
|       9 | 4294 | `			}else{` |
|       - | 4295 | `				/* Int lookup */` |
|       7 | 4296 | `				rc = HashmapLookupIntKey(pMap,pEntry->xKey.iKey,0);` |
|       - | 4297 | `			}` |
|      23 | 4298 | `			if( rc == SXRET_OK ){` |
|       - | 4299 | `				/* Key exists,break immediately */` |
|      11 | 4300 | `				break;` |
|       - | 4301 | `			}` |
|       7 | 4302 | `		}` |
|      19 | 4303 | `		if( i >= nArg ){` |
|       - | 4304 | `			/* Perform the insertion */` |
|       9 | 4305 | `			HashmapInsertNode((ph7_hashmap *)pArray->x.pOther,pEntry,TRUE);` |
|       4 | 4306 | `		}` |
|       - | 4307 | `		/* Point to the next entry */` |
|      19 | 4308 | `		pEntry = pEntry->pPrev; /* Reverse link */` |
|      19 | 4309 | `		n--;` |
|       1 | 4310 | `	}` |
|       - | 4311 | `	/* Return the freshly created array */` |
|       7 | 4312 | `	ph7_result_value(pCtx,pArray);` |
|       7 | 4313 | `	return PH7_OK;` |
|       9 | 4314 |  |
|       - | 4315 | `/*` |
|       - | 4316 | ` * array array_intersect(array $array1 ,array $array2,...)` |
|       - | 4317 | ` *  Computes the intersection of arrays.` |
|       - | 4318 | ` * Parameters` |
|       - | 4319 | ` *  $array1` |
|       - | 4320 | ` *    The array to compare from` |
|       - | 4321 | ` *  $array2` |
|       - | 4322 | ` *    An array to compare against` |
|       - | 4323 | ` *  $...` |
|       - | 4324 | ` *   More arrays to compare against` |
|       - | 4325 | ` * Return` |
|       - | 4326 | ` *  Returns an array containing all of the values in array1 whose values exist` |
|       - | 4327 | ` *  in all of the parameters. .` |
|       - | 4328 | ` * Note that NULL is returned on failure.` |
|       - | 4329 | ` */` |
|       2 | 4330 | `static int ph7_hashmap_intersect(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       1 | 4331 |  |
|       - | 4332 | `	ph7_hashmap_node *pEntry;` |
|       - | 4333 | `	ph7_hashmap *pSrc,*pMap;` |
|       - | 4334 | `	ph7_value *pArray;` |
|       - | 4335 | `	ph7_value *pVal;` |
|       - | 4336 | `	sxi32 rc;` |
|       - | 4337 | `	sxu32 n;` |
|       - | 4338 | `	int i;` |
|       3 | 4339 | `	if( nArg < 1 \|\| !ph7_value_is_array(apArg[0]) ){` |
|       - | 4340 | `		/* Missing arguments,return NULL */` |
|     ! 0 | 4341 | `		ph7_result_null(pCtx);` |
|     ! 0 | 4342 | `		return PH7_OK;` |
|       - | 4343 | `	}` |
|       3 | 4344 | `	if( nArg == 1 ){` |
|       - | 4345 | `		/* Return the first array since we cannot perform a diff */` |
|     ! 0 | 4346 | `		ph7_result_value(pCtx,apArg[0]);` |
|     ! 0 | 4347 | `		return PH7_OK;` |
|       - | 4348 | `	}` |
|       - | 4349 | `	/* Create a new array */` |
|       3 | 4350 | `	pArray = ph7_context_new_array(pCtx);` |
|       3 | 4351 | `	if( pArray == 0 ){` |
|     ! 0 | 4352 | `		ph7_result_null(pCtx);` |
|     ! 0 | 4353 | `		return PH7_OK;` |
|       - | 4354 | `	}` |
|       - | 4355 | `	/* Point to the internal representation of the source hashmap */` |
|       3 | 4356 | `	pSrc = (ph7_hashmap *)apArg[0]->x.pOther;` |
|       - | 4357 | `	/* Perform the intersection */` |
|       3 | 4358 | `	pEntry = pSrc->pFirst;` |
|       3 | 4359 | `	n = pSrc->nEntry;` |
|       5 | 4360 | `	for(;;){` |
|      11 | 4361 | `		if( n < 1 ){` |
|       3 | 4362 | `			break;` |
|       - | 4363 | `		}` |
|       - | 4364 | `		/* Extract the node value */` |
|       9 | 4365 | `		pVal = HashmapExtractNodeValue(pEntry);` |
|       9 | 4366 | `		if( pVal ){` |
|      13 | 4367 | `			for( i = 1 ; i < nArg ; i++ ){` |
|       9 | 4368 | `				if( !ph7_value_is_array(apArg[i])) {` |
|       - | 4369 | `					/* ignore */` |
|     ! 0 | 4370 | `					continue;` |
|       - | 4371 | `				}` |
|       - | 4372 | `				/* Point to the internal representation of the hashmap */` |
|       9 | 4373 | `				pMap = (ph7_hashmap *)apArg[i]->x.pOther;` |
|       - | 4374 | `				/* Perform the lookup */` |
|       9 | 4375 | `				rc = HashmapFindValue(pMap,pVal,0,TRUE);` |
|       9 | 4376 | `				if( rc != SXRET_OK ){` |
|       - | 4377 | `					/* Value does not exist */` |
|       5 | 4378 | `					break;` |
|       - | 4379 | `				}` |
|       3 | 4380 | `			}` |
|       9 | 4381 | `			if( i >= nArg ){` |
|       - | 4382 | `				/* Perform the insertion */` |
|       5 | 4383 | `				HashmapInsertNode((ph7_hashmap *)pArray->x.pOther,pEntry,TRUE);` |
|       2 | 4384 | `			}` |
|       4 | 4385 | `		}` |
|       - | 4386 | `		/* Point to the next entry */` |
|       9 | 4387 | `		pEntry = pEntry->pPrev; /* Reverse link */` |
|       9 | 4388 | `		n--;` |
|       1 | 4389 | `	}` |
|       - | 4390 | `	/* Return the freshly created array */` |
|       3 | 4391 | `	ph7_result_value(pCtx,pArray);` |
|       3 | 4392 | `	return PH7_OK;` |
|       2 | 4393 |  |
|       - | 4394 | `/*` |
|       - | 4395 | ` * array array_intersect_assoc(array $array1 ,array $array2,...)` |
|       - | 4396 | ` *  Computes the intersection of arrays.` |
|       - | 4397 | ` * Parameters` |
|       - | 4398 | ` *  $array1` |
|       - | 4399 | ` *    The array to compare from` |
|       - | 4400 | ` *  $array2` |
|       - | 4401 | ` *    An array to compare against` |
|       - | 4402 | ` *  $...` |
|       - | 4403 | ` *   More arrays to compare against` |
|       - | 4404 | ` * Return` |
|       - | 4405 | ` *  Returns an array containing all of the values in array1 whose values exist` |
|       - | 4406 | ` *  in all of the parameters. .` |
|       - | 4407 | ` * Note that NULL is returned on failure.` |
|       - | 4408 | ` */` |
|       2 | 4409 | `static int ph7_hashmap_intersect_assoc(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       1 | 4410 |  |
|       - | 4411 | `	ph7_hashmap_node *pEntry,*pN1,*pN2;` |
|       - | 4412 | `	ph7_hashmap *pSrc,*pMap;` |
|       - | 4413 | `	ph7_value *pArray;` |
|       - | 4414 | `	ph7_value *pVal;` |
|       - | 4415 | `	sxi32 rc;` |
|       - | 4416 | `	sxu32 n;` |
|       - | 4417 | `	int i;` |
|       3 | 4418 | `	if( nArg < 1 \|\| !ph7_value_is_array(apArg[0]) ){` |
|       - | 4419 | `		/* Missing arguments,return NULL */` |
|     ! 0 | 4420 | `		ph7_result_null(pCtx);` |
|     ! 0 | 4421 | `		return PH7_OK;` |
|       - | 4422 | `	}` |
|       3 | 4423 | `	if( nArg == 1 ){` |
|       - | 4424 | `		/* Return the first array since we cannot perform a diff */` |
|     ! 0 | 4425 | `		ph7_result_value(pCtx,apArg[0]);` |
|     ! 0 | 4426 | `		return PH7_OK;` |
|       - | 4427 | `	}` |
|       - | 4428 | `	/* Create a new array */` |
|       3 | 4429 | `	pArray = ph7_context_new_array(pCtx);` |
|       3 | 4430 | `	if( pArray == 0 ){` |
|     ! 0 | 4431 | `		ph7_result_null(pCtx);` |
|     ! 0 | 4432 | `		return PH7_OK;` |
|       - | 4433 | `	}` |
|       - | 4434 | `	/* Point to the internal representation of the source hashmap */` |
|       3 | 4435 | `	pSrc = (ph7_hashmap *)apArg[0]->x.pOther;` |
|       - | 4436 | `	/* Perform the intersection */` |
|       3 | 4437 | `	pEntry = pSrc->pFirst;` |
|       3 | 4438 | `	n = pSrc->nEntry;` |
|       3 | 4439 | `	pN1 = pN2 = 0; /* cc warning */` |
|       4 | 4440 | `	for(;;){` |
|       9 | 4441 | `		if( n < 1 ){` |
|       3 | 4442 | `			break;` |
|       - | 4443 | `		}` |
|       - | 4444 | `		/* Extract the node value */` |
|       7 | 4445 | `		pVal = HashmapExtractNodeValue(pEntry);` |
|       7 | 4446 | `		if( pVal ){` |
|       9 | 4447 | `			for( i = 1 ; i < nArg ; i++ ){` |
|       7 | 4448 | `				if( !ph7_value_is_array(apArg[i])) {` |
|       - | 4449 | `					/* ignore */` |
|     ! 0 | 4450 | `					continue;` |
|       - | 4451 | `				}` |
|       - | 4452 | `				/* Point to the internal representation of the hashmap */` |
|       7 | 4453 | `				pMap = (ph7_hashmap *)apArg[i]->x.pOther;` |
|       - | 4454 | `				/* Perform a key lookup first */` |
|       7 | 4455 | `				if( pEntry->iType == HASHMAP_INT_NODE ){` |
|     ! 0 | 4456 | `					rc = HashmapLookupIntKey(pMap,pEntry->xKey.iKey,&pN1);` |
|     ! 0 | 4457 | `				}else{` |
|       7 | 4458 | `					rc = HashmapLookupBlobKey(pMap,SyBlobData(&pEntry->xKey.sKey),SyBlobLength(&pEntry->xKey.sKey),&pN1);` |
|       - | 4459 | `				}` |
|       7 | 4460 | `				if( rc != SXRET_OK ){` |
|       - | 4461 | `					/* No such key,break immediately */` |
|       3 | 4462 | `					break;` |
|       - | 4463 | `				}` |
|       - | 4464 | `				/* Perform the lookup */` |
|       5 | 4465 | `				rc = HashmapFindValue(pMap,pVal,&pN2,TRUE);` |
|       5 | 4466 | `				if( rc != SXRET_OK \|\| pN1 != pN2 ){` |
|       - | 4467 | `					/* Value does not exist */` |
|       2 | 4468 | `					break;` |
|       - | 4469 | `				}` |
|       2 | 4470 | `			}` |
|       7 | 4471 | `			if( i >= nArg ){` |
|       - | 4472 | `				/* Perform the insertion */` |
|       3 | 4473 | `				HashmapInsertNode((ph7_hashmap *)pArray->x.pOther,pEntry,TRUE);` |
|       1 | 4474 | `			}` |
|       3 | 4475 | `		}` |
|       - | 4476 | `		/* Point to the next entry */` |
|       7 | 4477 | `		pEntry = pEntry->pPrev; /* Reverse link */` |
|       7 | 4478 | `		n--;` |
|       1 | 4479 | `	}` |
|       - | 4480 | `	/* Return the freshly created array */` |
|       3 | 4481 | `	ph7_result_value(pCtx,pArray);` |
|       3 | 4482 | `	return PH7_OK;` |
|       2 | 4483 |  |
|       - | 4484 | `/*` |
|       - | 4485 | ` * array array_intersect_key(array $array1 ,array $array2,...)` |
|       - | 4486 | ` *  Computes the intersection of arrays using keys for comparison.` |
|       - | 4487 | ` * Parameters` |
|       - | 4488 | ` *  $array1` |
|       - | 4489 | ` *    The array to compare from` |
|       - | 4490 | ` *  $array2` |
|       - | 4491 | ` *    An array to compare against` |
|       - | 4492 | ` *  $...` |
|       - | 4493 | ` *   More arrays to compare against` |
|       - | 4494 | ` * Return` |
|       - | 4495 | ` *  Returns an associative array containing all the entries of array1 which` |
|       - | 4496 | ` *  have keys that are present in all arguments.` |
|       - | 4497 | ` * Note that NULL is returned on failure.` |
|       - | 4498 | ` */` |
|       4 | 4499 | `static int ph7_hashmap_intersect_key(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       1 | 4500 |  |
|       - | 4501 | `	ph7_hashmap_node *pEntry;` |
|       - | 4502 | `	ph7_hashmap *pSrc,*pMap;` |
|       - | 4503 | `	ph7_value *pArray;` |
|       - | 4504 | `	sxi32 rc;` |
|       - | 4505 | `	sxu32 n;` |
|       - | 4506 | `	int i;` |
|       5 | 4507 | `	if( nArg < 1 \|\| !ph7_value_is_array(apArg[0]) ){` |
|       - | 4508 | `		/* Missing arguments,return NULL */` |
|     ! 0 | 4509 | `		ph7_result_null(pCtx);` |
|     ! 0 | 4510 | `		return PH7_OK;` |
|       - | 4511 | `	}` |
|       5 | 4512 | `	if( nArg == 1 ){` |
|       - | 4513 | `		/* Return the first array since we cannot perform a diff */` |
|     ! 0 | 4514 | `		ph7_result_value(pCtx,apArg[0]);` |
|     ! 0 | 4515 | `		return PH7_OK;` |
|       - | 4516 | `	}` |
|       - | 4517 | `	/* Create a new array */` |
|       5 | 4518 | `	pArray = ph7_context_new_array(pCtx);` |
|       5 | 4519 | `	if( pArray == 0 ){` |
|     ! 0 | 4520 | `		ph7_result_null(pCtx);` |
|     ! 0 | 4521 | `		return PH7_OK;` |
|       - | 4522 | `	}` |
|       - | 4523 | `	/* Point to the internal representation of the main hashmap */` |
|       5 | 4524 | `	pSrc = (ph7_hashmap *)apArg[0]->x.pOther;` |
|       - | 4525 | `	/* Perfrom the intersection */` |
|       5 | 4526 | `	pEntry = pSrc->pFirst;` |
|       5 | 4527 | `	n = pSrc->nEntry;` |
|       8 | 4528 | `	for(;;){` |
|      17 | 4529 | `		if( n < 1 ){` |
|       5 | 4530 | `			break;` |
|       - | 4531 | `		}` |
|      19 | 4532 | `		for( i = 1 ; i < nArg ; i++ ){` |
|      13 | 4533 | `			if( !ph7_value_is_array(apArg[i])) {` |
|       - | 4534 | `				/* ignore */` |
|     ! 0 | 4535 | `				continue;` |
|       - | 4536 | `			}` |
|      13 | 4537 | `			pMap = (ph7_hashmap *)apArg[i]->x.pOther;` |
|      13 | 4538 | `			if( pEntry->iType == HASHMAP_BLOB_NODE ){` |
|       7 | 4539 | `				SyBlob *pKey = &pEntry->xKey.sKey;` |
|       - | 4540 | `				/* Blob lookup */` |
|       7 | 4541 | `				rc = HashmapLookupBlobKey(pMap,SyBlobData(pKey),SyBlobLength(pKey),0);` |
|       4 | 4542 | `			}else{` |
|       - | 4543 | `				/* Int key */` |
|       7 | 4544 | `				rc = HashmapLookupIntKey(pMap,pEntry->xKey.iKey,0);` |
|       - | 4545 | `			}` |
|      13 | 4546 | `			if( rc != SXRET_OK ){` |
|       - | 4547 | `				/* Key does not exists,break immediately */` |
|       7 | 4548 | `				break;` |
|       - | 4549 | `			}` |
|       4 | 4550 | `		}` |
|      13 | 4551 | `		if( i >= nArg ){` |
|       - | 4552 | `			/* Perform the insertion */` |
|       7 | 4553 | `			HashmapInsertNode((ph7_hashmap *)pArray->x.pOther,pEntry,TRUE);` |
|       3 | 4554 | `		}` |
|       - | 4555 | `		/* Point to the next entry */` |
|      13 | 4556 | `		pEntry = pEntry->pPrev; /* Reverse link */` |
|      13 | 4557 | `		n--;` |
|       1 | 4558 | `	}` |
|       - | 4559 | `	/* Return the freshly created array */` |
|       5 | 4560 | `	ph7_result_value(pCtx,pArray);` |
|       5 | 4561 | `	return PH7_OK;` |
|       3 | 4562 |  |
|       - | 4563 | `/*` |
|       - | 4564 | ` * array array_uintersect(array $array1 ,array $array2,...,$callback)` |
|       - | 4565 | ` *  Computes the intersection of arrays.` |
|       - | 4566 | ` * Parameters` |
|       - | 4567 | ` *  $array1` |
|       - | 4568 | ` *    The array to compare from` |
|       - | 4569 | ` *  $array2` |
|       - | 4570 | ` *    An array to compare against` |
|       - | 4571 | ` *  $...` |
|       - | 4572 | ` *   More arrays to compare against` |
|       - | 4573 | ` * $callback` |
|       - | 4574 | ` *  The callback comparison function.` |
|       - | 4575 | ` *  The comparison function must return an integer less than, equal to, or greater than zero` |
|       - | 4576 | ` *  if the first argument is considered to be respectively less than, equal to, or greater` |
|       - | 4577 | ` *  than the second.` |
|       - | 4578 | ` *     int callback ( mixed $a, mixed $b )` |
|       - | 4579 | ` * Return` |
|       - | 4580 | ` *  Returns an array containing all of the values in array1 whose values exist` |
|       - | 4581 | ` *  in all of the parameters. .` |
|       - | 4582 | ` * Note that NULL is returned on failure.` |
|       - | 4583 | ` */` |
|       2 | 4584 | `static int ph7_hashmap_uintersect(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       1 | 4585 |  |
|       - | 4586 | `	ph7_hashmap_node *pEntry;` |
|       - | 4587 | `	ph7_hashmap *pSrc,*pMap;` |
|       - | 4588 | `	ph7_value *pCallback;` |
|       - | 4589 | `	ph7_value *pArray;` |
|       - | 4590 | `	ph7_value *pVal;` |
|       - | 4591 | `	sxi32 rc;` |
|       - | 4592 | `	sxu32 n;` |
|       - | 4593 | `	int i;` |
|       - | 4594 |  |
|       3 | 4595 | `	if( nArg < 2 \|\| !ph7_value_is_array(apArg[0]) ){` |
|       - | 4596 | `		/* Missing/Invalid arguments,return NULL */` |
|     ! 0 | 4597 | `		ph7_result_null(pCtx);` |
|     ! 0 | 4598 | `		return PH7_OK;` |
|       - | 4599 | `	}` |
|       - | 4600 | `	/* Point to the callback */` |
|       3 | 4601 | `	pCallback = apArg[nArg - 1];` |
|       3 | 4602 | `	if( nArg == 2 ){` |
|       - | 4603 | `		/* Return the first array since we cannot perform a diff */` |
|     ! 0 | 4604 | `		ph7_result_value(pCtx,apArg[0]);` |
|     ! 0 | 4605 | `		return PH7_OK;` |
|       - | 4606 | `	}` |
|       - | 4607 | `	/* Create a new array */` |
|       3 | 4608 | `	pArray = ph7_context_new_array(pCtx);` |
|       3 | 4609 | `	if( pArray == 0 ){` |
|     ! 0 | 4610 | `		ph7_result_null(pCtx);` |
|     ! 0 | 4611 | `		return PH7_OK;` |
|       - | 4612 | `	}` |
|       - | 4613 | `	/* Point to the internal representation of the source hashmap */` |
|       3 | 4614 | `	pSrc = (ph7_hashmap *)apArg[0]->x.pOther;` |
|       - | 4615 | `	/* Perform the intersection */` |
|       3 | 4616 | `	pEntry = pSrc->pFirst;` |
|       3 | 4617 | `	n = pSrc->nEntry;` |
|       4 | 4618 | `	for(;;){` |
|       9 | 4619 | `		if( n < 1 ){` |
|       3 | 4620 | `			break;` |
|       - | 4621 | `		}` |
|       - | 4622 | `		/* Extract the node value */` |
|       7 | 4623 | `		pVal = HashmapExtractNodeValue(pEntry);` |
|       7 | 4624 | `		if( pVal ){` |
|      11 | 4625 | `			for( i = 1 ; i < nArg - 1; i++ ){` |
|       7 | 4626 | `				if( !ph7_value_is_array(apArg[i])) {` |
|       - | 4627 | `					/* ignore */` |
|     ! 0 | 4628 | `					continue;` |
|       - | 4629 | `				}` |
|       - | 4630 | `				/* Point to the internal representation of the hashmap */` |
|       7 | 4631 | `				pMap = (ph7_hashmap *)apArg[i]->x.pOther;` |
|       - | 4632 | `				/* Perform the lookup */` |
|       7 | 4633 | `				rc = HashmapFindValueByCallback(pMap,pVal,pCallback,0);` |
|       7 | 4634 | `				if( rc != SXRET_OK ){` |
|       - | 4635 | `					/* Value does not exist */` |
|       3 | 4636 | `					break;` |
|       - | 4637 | `				}` |
|       3 | 4638 | `			}` |
|       7 | 4639 | `			if( i >= (nArg-1) ){` |
|       - | 4640 | `				/* Perform the insertion */` |
|       5 | 4641 | `				HashmapInsertNode((ph7_hashmap *)pArray->x.pOther,pEntry,TRUE);` |
|       2 | 4642 | `			}` |
|       3 | 4643 | `		}` |
|       - | 4644 | `		/* Point to the next entry */` |
|       7 | 4645 | `		pEntry = pEntry->pPrev; /* Reverse link */` |
|       7 | 4646 | `		n--;` |
|       1 | 4647 | `	}` |
|       - | 4648 | `	/* Return the freshly created array */` |
|       3 | 4649 | `	ph7_result_value(pCtx,pArray);` |
|       3 | 4650 | `	return PH7_OK;` |
|       2 | 4651 |  |
|       - | 4652 | `/*` |
|       - | 4653 | ` * array array_fill(int $start_index,int $num,var $value)` |
|       - | 4654 | ` *  Fill an array with values.` |
|       - | 4655 | ` * Parameters` |
|       - | 4656 | ` *  $start_index` |
|       - | 4657 | ` *    The first index of the returned array.` |
|       - | 4658 | ` *  $num` |
|       - | 4659 | ` *   Number of elements to insert.` |
|       - | 4660 | ` *  $value` |
|       - | 4661 | ` *    Value to use for filling.` |
|       - | 4662 | ` * Return` |
|       - | 4663 | ` *  The filled array or null on failure.` |
|       - | 4664 | ` */` |
|     238 | 4665 | `static int ph7_hashmap_fill(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       2 | 4666 |  |
|       - | 4667 | `	ph7_value *pArray;` |
|       - | 4668 | `	int i,nEntry;` |
|       - | 4669 |  |
|       - | 4670 | `	/* PHP enforces argument count and type checks. */` |
|     240 | 4671 | `	if( nArg != 3 ){` |
|       - | 4672 | `		/* wrong number of arguments -> ArgumentCountError */` |
|       7 | 4673 | `		return PH7_VmThrowException(pCtx,` |
|       - | 4674 | `			"ArgumentCountError",` |
|       - | 4675 | `			"array_fill() expects exactly 3 arguments, %d given",` |
|       2 | 4676 | `			nArg` |
|       - | 4677 | `			);` |
|       - | 4678 | `	}` |
|       - | 4679 |  |
|       - | 4680 | `	/* Argument #1: start index must be convertible to int.  Accept booleans,` |
|       - | 4681 | `	 * floats, and numeric strings (including those with decimal point) by` |
|       - | 4682 | `	 * allowing them through the conversion.  Only arrays, objects, resources` |
|       - | 4683 | `	 * and NULLs are rejected outright. */` |
|     466 | 4684 | `	if( ph7_value_is_array(apArg[0]) \|\| ph7_value_is_object(apArg[0]) \|\|` |
|     350 | 4685 | `		ph7_value_is_resource(apArg[0]) \|\| ph7_value_is_null(apArg[0]) ){` |
|       4 | 4686 | `		return PH7_VmThrowException(pCtx,` |
|       - | 4687 | `			"TypeError",` |
|       - | 4688 | `			"array_fill(): Argument #1 ($start_index) must be of type int, %s given",` |
|       1 | 4689 | `			ph7_type_name(apArg[0])` |
|       - | 4690 | `			);` |
|       - | 4691 | `	}` |
|     234 | 4692 | `	if( ph7_value_is_string(apArg[0]) ){` |
|       - | 4693 | `		int len;` |
|       8 | 4694 | `		sxu8 bReal = FALSE;` |
|       8 | 4695 | `		const char *zStr = ph7_value_to_string(apArg[0], &len);` |
|       8 | 4696 | `		if( SyStrIsNumeric(zStr, len, &bReal, 0) != SXRET_OK ){` |
|       - | 4697 | `			/* Non‑numeric string is an error. */` |
|       3 | 4698 | `			return PH7_VmThrowException(pCtx,` |
|       - | 4699 | `				"TypeError",` |
|       - | 4700 | `				"array_fill(): Argument #1 ($start_index) must be of type int, string given"` |
|       - | 4701 | `				);` |
|       - | 4702 | `		}` |
|       5 | 4703 | `		if( bReal ){` |
|       - | 4704 | `			/* float-string -> deprecation warning */` |
|       4 | 4705 | `			ph7_context_throw_error_format(pCtx, E_DEPRECATED,` |
|       - | 4706 | `				"Implicit conversion from float-string \"%s\" to int loses precision",` |
|       1 | 4707 | `				zStr` |
|       - | 4708 | `				);` |
|       1 | 4709 | `		}` |
|       2 | 4710 | `	}` |
|       - | 4711 |  |
|       - | 4712 | `	/* Argument #2: count must be convertible to non-negative int.  Allow booleans,` |
|       - | 4713 | `	 * floats and numeric strings; reject arrays, objects, resources and NULL. */` |
|     460 | 4714 | `	if( ph7_value_is_array(apArg[1]) \|\| ph7_value_is_object(apArg[1]) \|\|` |
|     347 | 4715 | `		ph7_value_is_resource(apArg[1]) \|\| ph7_value_is_null(apArg[1]) ){` |
|     ! 0 | 4716 | `		return PH7_VmThrowException(pCtx,` |
|       - | 4717 | `			"TypeError",` |
|       - | 4718 | `			"array_fill(): Argument #2 ($count) must be of type int, %s given",` |
|     ! 0 | 4719 | `			ph7_type_name(apArg[1])` |
|       - | 4720 | `			);` |
|       - | 4721 | `	}` |
|     232 | 4722 | `	if( ph7_value_is_string(apArg[1]) ){` |
|       - | 4723 | `		int len;` |
|       3 | 4724 | `		sxu8 bReal = FALSE;` |
|       3 | 4725 | `		const char *zStr = ph7_value_to_string(apArg[1], &len);` |
|       3 | 4726 | `		if( SyStrIsNumeric(zStr, len, &bReal, 0) != SXRET_OK ){` |
|       3 | 4727 | `			return PH7_VmThrowException(pCtx,` |
|       - | 4728 | `				"TypeError",` |
|       - | 4729 | `				"array_fill(): Argument #2 ($count) must be of type int, string given"` |
|       - | 4730 | `				);` |
|       - | 4731 | `		}` |
|     ! 0 | 4732 | `	}` |
|       - | 4733 | `	/* Note: booleans and floats (including fractional) are now accepted; they` |
|       - | 4734 | `	 * will be converted by ph7_value_to_int below. */` |
|     230 | 4735 | `	if( ph7_value_is_float(apArg[1]) ){` |
|       5 | 4736 | `		double d = ph7_value_to_double(apArg[1]);` |
|       - | 4737 | `		/* avoid hiding outer 'i' (loop index) */` |
|       5 | 4738 | `		sxi64 i64 = (sxi64)d;` |
|       5 | 4739 | `		if( d != (double)i64 ){` |
|       7 | 4740 | `			ph7_context_throw_error_format(pCtx, E_DEPRECATED,` |
|       - | 4741 | `				"Implicit conversion from float %g to int loses precision",` |
|       2 | 4742 | `				d` |
|       - | 4743 | `				);` |
|       2 | 4744 | `		}` |
|       2 | 4745 | `	}` |
|       - | 4746 |  |
|       - | 4747 | `	/* Total number of entries to insert */` |
|     230 | 4748 | `	nEntry = ph7_value_to_int(apArg[1]);` |
|       - | 4749 | `	/* Reject negative counts with a ValueError like PHP. */` |
|     230 | 4750 | `	if( nEntry < 0 ){` |
|       3 | 4751 | `		return PH7_VmThrowException(pCtx,` |
|       - | 4752 | `			"ValueError",` |
|       - | 4753 | `			"array_fill(): Argument #2 ($count) must be greater than or equal to 0"` |
|       - | 4754 | `			);` |
|       - | 4755 | `	}` |
|       - | 4756 |  |
|       - | 4757 | `	/* If zero elements were requested, return an empty array without allocating */` |
|     227 | 4758 | `	if( nEntry == 0 ){` |
|       7 | 4759 | `		ph7_result_value(pCtx, ph7_context_new_array(pCtx));` |
|       7 | 4760 | `		return PH7_OK;` |
|       - | 4761 | `	}` |
|       - | 4762 |  |
|       - | 4763 | `	/* Create a new array */` |
|     221 | 4764 | `	pArray = ph7_context_new_array(pCtx);` |
|     221 | 4765 | `	if( pArray == 0 ){` |
|     ! 0 | 4766 | `		ph7_result_null(pCtx);` |
|     ! 0 | 4767 | `		return PH7_OK;` |
|       - | 4768 | `	}` |
|       - | 4769 |  |
|       - | 4770 | `	/* Insert the first entry alone because it has its own key */` |
|     221 | 4771 | `	ph7_array_add_intkey_elem(pArray, ph7_value_to_int(apArg[0]), apArg[2]);` |
|       - | 4772 | `	/* Repeat insertion of the desired value */` |
| 2117583 | 4773 | `	for( i = 1 ; i < nEntry ; i++ ){` |
| 2117363 | 4774 | `		ph7_array_add_elem(pArray, 0/*Automatic index assign */, apArg[2]);` |
| 1058682 | 4775 | `	}` |
|       - | 4776 | `	/* Return the filled array */` |
|     221 | 4777 | `	ph7_result_value(pCtx, pArray);` |
|     221 | 4778 | `	return PH7_OK;` |
|     121 | 4779 |  |
|       - | 4780 | `/*` |
|       - | 4781 | ` * array array_fill_keys(array $input,mixed $value)` |
|       - | 4782 | ` *  Fill an array with values, specifying keys.` |
|       - | 4783 | ` * Parameters` |
|       - | 4784 | ` *  $input` |
|       - | 4785 | ` *   Array of values that will be used as key.` |
|       - | 4786 | ` *  $value` |
|       - | 4787 | ` *    Value to use for filling.` |
|       - | 4788 | ` * Return` |
|       - | 4789 | ` *  The filled array.` |
|       - | 4790 | ` * Throws` |
|       - | 4791 | ` *  ValueError if $input is not an array.` |
|       - | 4792 | ` */` |
|      26 | 4793 | `static int ph7_hashmap_fill_keys(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       2 | 4794 |  |
|       - | 4795 | `	ph7_hashmap_node *pEntry;` |
|       - | 4796 | `	ph7_hashmap *pSrc;` |
|       - | 4797 | `	ph7_value *pArray;` |
|       - | 4798 | `	sxu32 n;` |
|       - | 4799 | `	/* PHP enforces exactly 2 arguments. */` |
|      28 | 4800 | `	if( nArg != 2 ){` |
|      10 | 4801 | `		return PH7_VmThrowException(pCtx,` |
|       - | 4802 | `			"ArgumentCountError",` |
|       - | 4803 | `			"array_fill_keys() expects exactly 2 arguments, %d given",` |
|       3 | 4804 | `			nArg` |
|       - | 4805 | `			);` |
|       - | 4806 | `	}` |
|       - | 4807 | `	/* Make sure we are dealing with a valid hashmap */` |
|      22 | 4808 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|       7 | 4809 | `		return PH7_VmThrowException(pCtx,` |
|       - | 4810 | `			"TypeError",` |
|       - | 4811 | `			"array_fill_keys(): Argument #1 ($keys) must be of type array, %s given",` |
|       2 | 4812 | `			ph7_type_name(apArg[0])` |
|       - | 4813 | `			);` |
|       - | 4814 | `	}` |
|       - | 4815 | `	/* Point to the internal representation of the input hashmap */` |
|      17 | 4816 | `	pSrc = (ph7_hashmap *)apArg[0]->x.pOther;` |
|       - | 4817 | `	/* Create a new array */` |
|      17 | 4818 | `	pArray = ph7_context_new_array(pCtx);` |
|      17 | 4819 | `	if( pArray == 0 ){` |
|     ! 0 | 4820 | `		ph7_result_null(pCtx);` |
|     ! 0 | 4821 | `		return PH7_OK;` |
|       - | 4822 | `	}` |
|       - | 4823 | `	/* Perform the requested operation */` |
|      17 | 4824 | `	pEntry = pSrc->pFirst;` |
|      45 | 4825 | `	for( n = 0 ; n < pSrc->nEntry ; n++ ){` |
|      29 | 4826 | `		ph7_array_add_elem(pArray,HashmapExtractNodeValue(pEntry),apArg[1]);` |
|       - | 4827 | `		/* Point to the next entry */` |
|      29 | 4828 | `		pEntry = pEntry->pPrev; /* Reverse link */` |
|      15 | 4829 | `	}` |
|       - | 4830 | `	/* Return the filled array */` |
|      17 | 4831 | `	ph7_result_value(pCtx,pArray);` |
|      17 | 4832 | `	return PH7_OK;` |
|      15 | 4833 |  |
|       - | 4834 | `/*` |
|       - | 4835 | ` * array array_combine(array $keys,array $values)` |
|       - | 4836 | ` *  Creates an array by using one array for keys and another for its values.` |
|       - | 4837 | ` * Parameters` |
|       - | 4838 | ` *  $keys` |
|       - | 4839 | ` *    Array of keys to be used.` |
|       - | 4840 | ` * $values` |
|       - | 4841 | ` *   Array of values to be used.` |
|       - | 4842 | ` * Return` |
|       - | 4843 | ` *  Returns the combined array. Otherwise FALSE if the number of elements` |
|       - | 4844 | ` *  for each array isn't equal or if one of the given arguments is` |
|       - | 4845 | ` *  not an array.` |
|       - | 4846 | ` */` |
|      18 | 4847 | `static int ph7_hashmap_combine(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       2 | 4848 |  |
|       - | 4849 | `	ph7_hashmap_node *pKe,*pVe;` |
|       - | 4850 | `	ph7_hashmap *pKey,*pValue;` |
|       - | 4851 | `	ph7_value *pArray;` |
|       - | 4852 | `	sxu32 n;` |
|       - | 4853 | `	/* PHP enforces argument count and type checks. */` |
|      20 | 4854 | `	if( nArg != 2 ){` |
|       - | 4855 | `		/* wrong number of arguments -> ArgumentCountError */` |
|       4 | 4856 | `		return PH7_VmThrowException(pCtx,` |
|       - | 4857 | `			"ArgumentCountError",` |
|       - | 4858 | `			"array_combine() expects exactly 2 arguments, %d given",` |
|       1 | 4859 | `			nArg` |
|       - | 4860 | `			);` |
|       - | 4861 | `	}` |
|       - | 4862 | `	/* Validate argument types individually so we can report the correct` |
|       - | 4863 | `	 * argument index in the error message. */` |
|      18 | 4864 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|       4 | 4865 | `		return PH7_VmThrowException(pCtx,` |
|       - | 4866 | `			"TypeError",` |
|       - | 4867 | `			"array_combine(): Argument #1 ($keys) must be of type array, %s given",` |
|       1 | 4868 | `			ph7_type_name(apArg[0])` |
|       - | 4869 | `			);` |
|       - | 4870 | `	}` |
|      16 | 4871 | `	if( !ph7_value_is_array(apArg[1]) ){` |
|       4 | 4872 | `		return PH7_VmThrowException(pCtx,` |
|       - | 4873 | `			"TypeError",` |
|       - | 4874 | `			"array_combine(): Argument #2 ($values) must be of type array, %s given",` |
|       2 | 4875 | `			ph7_type_name(apArg[1])` |
|       - | 4876 | `			);` |
|       - | 4877 | `	}` |
|       - | 4878 | `	/* Point to the internal representation of the input hashmaps */` |
|      14 | 4879 | `	pKey   = (ph7_hashmap *)apArg[0]->x.pOther;` |
|      14 | 4880 | `	pValue = (ph7_hashmap *)apArg[1]->x.pOther;` |
|      14 | 4881 | `	if( pKey->nEntry != pValue->nEntry ){` |
|       - | 4882 | `		/* Length mismatch -> ValueError */` |
|       3 | 4883 | `		return PH7_VmThrowException(pCtx,` |
|       - | 4884 | `			"ValueError",` |
|       - | 4885 | `			"array_combine(): Argument #1 ($keys) and argument #2 ($values) must have the same number of elements"` |
|       - | 4886 | `			);` |
|       - | 4887 | `	}` |
|       - | 4888 | `	/* Create a new array */` |
|      11 | 4889 | `	pArray = ph7_context_new_array(pCtx);` |
|      11 | 4890 | `	if( pArray == 0 ){` |
|     ! 0 | 4891 | `		ph7_result_bool(pCtx,0);` |
|     ! 0 | 4892 | `		return PH7_OK;` |
|       - | 4893 | `	}` |
|       - | 4894 | `	/* Perform the requested operation */` |
|      11 | 4895 | `	pKe = pKey->pFirst;` |
|      11 | 4896 | `	pVe = pValue->pFirst;` |
|      33 | 4897 | `	for( n = 0 ; n < pKey->nEntry ; n++ ){` |
|      23 | 4898 | `		ph7_value *pKeyVal = HashmapExtractNodeValue(pKe);` |
|      23 | 4899 | `		ph7_value *pValVal = HashmapExtractNodeValue(pVe);` |
|       - | 4900 | `		/* PHP treats floats used as keys in array_combine differently than` |
|       - | 4901 | `		 * ordinary offset access: the float is stringified rather than` |
|       - | 4902 | `		 * truncated.  To emulate this behavior we create a temporary copy of` |
|       - | 4903 | `		 * the value when it is a float and convert the copy to string.  The` |
|       - | 4904 | `		 * original array must not be mutated. */` |
|      23 | 4905 | `		ph7_value *pKeyCopy = pKeyVal;` |
|      23 | 4906 | `		if( ph7_value_is_float(pKeyVal) ){` |
|       5 | 4907 | `			ph7_value *pTmpKey = ph7_context_new_scalar(pCtx);` |
|       5 | 4908 | `			if( pTmpKey ){` |
|       5 | 4909 | `				PH7_MemObjStore(pKeyVal,pTmpKey);` |
|       - | 4910 | `				/* Convert copy to string so it becomes "1.5" or "2" etc. */` |
|       5 | 4911 | `				PH7_MemObjToString(pTmpKey);` |
|       5 | 4912 | `				pKeyCopy = pTmpKey;` |
|       2 | 4913 | `			}` |
|       2 | 4914 | `		}` |
|      23 | 4915 | `		ph7_array_add_elem(pArray,pKeyCopy,pValVal);` |
|       - | 4916 | `		/* Point to the next entry */` |
|      23 | 4917 | `		pKe = pKe->pPrev; /* Reverse link */` |
|      23 | 4918 | `		pVe = pVe->pPrev;` |
|      12 | 4919 | `	}` |
|       - | 4920 | `	/* Return the filled array */` |
|      11 | 4921 | `	ph7_result_value(pCtx,pArray);` |
|      11 | 4922 | `	return PH7_OK;` |
|      11 | 4923 |  |
|       - | 4924 | `/*` |
|       - | 4925 | ` * array array_reverse(array $array [,bool $preserve_keys = false ])` |
|       - | 4926 | ` *  Return an array with elements in reverse order.` |
|       - | 4927 | ` * Parameters` |
|       - | 4928 | ` *  $array` |
|       - | 4929 | ` *   The input array.` |
|       - | 4930 | ` *  $preserve_keys (optional)` |
|       - | 4931 | ` *   If set to TRUE keys are preserved.` |
|       - | 4932 | ` * Return` |
|       - | 4933 | ` *  The reversed array.` |
|       - | 4934 | ` */` |
|       6 | 4935 | `static int ph7_hashmap_reverse(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       1 | 4936 |  |
|       - | 4937 | `	ph7_hashmap_node *pEntry;` |
|       - | 4938 | `	ph7_hashmap *pSrc;` |
|       - | 4939 | `	ph7_value *pArray;` |
|       - | 4940 | `	int bPreserve;` |
|       - | 4941 | `	sxu32 n;` |
|       7 | 4942 | `	if( nArg < 1 ){` |
|       - | 4943 | `		/* Missing arguments,return NULL */` |
|     ! 0 | 4944 | `		ph7_result_null(pCtx);` |
|     ! 0 | 4945 | `		return PH7_OK;` |
|       - | 4946 | `	}` |
|       - | 4947 | `	/* Make sure we are dealing with a valid hashmap */` |
|       7 | 4948 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|       - | 4949 | `		/* Invalid argument,return NULL */` |
|     ! 0 | 4950 | `		ph7_result_null(pCtx);` |
|     ! 0 | 4951 | `		return PH7_OK;` |
|       - | 4952 | `	}` |
|       7 | 4953 | `	bPreserve = FALSE;` |
|       7 | 4954 | `	if( nArg > 1 && ph7_value_is_bool(apArg[1]) ){` |
|       3 | 4955 | `		bPreserve = ph7_value_to_bool(apArg[1]);` |
|       1 | 4956 | `	}` |
|       - | 4957 | `	/* Point to the internal representation of the input hashmap */` |
|       7 | 4958 | `	pSrc = (ph7_hashmap *)apArg[0]->x.pOther;` |
|       - | 4959 | `	/* Create a new array */` |
|       7 | 4960 | `	pArray = ph7_context_new_array(pCtx);` |
|       7 | 4961 | `	if( pArray == 0 ){` |
|     ! 0 | 4962 | `		ph7_result_null(pCtx);` |
|     ! 0 | 4963 | `		return PH7_OK;` |
|       - | 4964 | `	}` |
|       - | 4965 | `	/* Perform the requested operation */` |
|       7 | 4966 | `	pEntry = pSrc->pLast;` |
|      23 | 4967 | `	for( n = 0 ; n < pSrc->nEntry ; n++ ){` |
|      17 | 4968 | `		HashmapInsertNode((ph7_hashmap *)pArray->x.pOther,pEntry,bPreserve);` |
|       - | 4969 | `		/* Point to the previous entry */` |
|      17 | 4970 | `		pEntry = pEntry->pNext; /* Reverse link */` |
|       9 | 4971 | `	}` |
|       7 | 4972 | `	ph7_result_value(pCtx,pArray);` |
|       7 | 4973 | `	return PH7_OK;` |
|       4 | 4974 |  |
|       - | 4975 | `/*` |
|       - | 4976 | ` * array array_unique(array $array[,int $sort_flags = SORT_STRING ])` |
|       - | 4977 | ` *  Removes duplicate values from an array` |
|       - | 4978 | ` * Parameter` |
|       - | 4979 | ` *  $array` |
|       - | 4980 | ` *   The input array.` |
|       - | 4981 | ` *  $sort_flags` |
|       - | 4982 | ` *    The optional second parameter sort_flags may be used to modify the sorting behavior using these values:` |
|       - | 4983 | ` *    Sorting type flags:` |
|       - | 4984 | ` *       SORT_REGULAR - compare items normally (don't change types)` |
|       - | 4985 | ` *       SORT_NUMERIC - compare items numerically` |
|       - | 4986 | ` *       SORT_STRING - compare items as strings` |
|       - | 4987 | ` *       SORT_LOCALE_STRING - compare items as` |
|       - | 4988 | ` * Return` |
|       - | 4989 | ` *  Filtered array or NULL on failure.` |
|       - | 4990 | ` */` |
|       2 | 4991 | `static int ph7_hashmap_unique(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       1 | 4992 |  |
|       - | 4993 | `	ph7_hashmap_node *pEntry;` |
|       - | 4994 | `	ph7_value *pNeedle;` |
|       - | 4995 | `	ph7_hashmap *pSrc;` |
|       - | 4996 | `	ph7_value *pArray;` |
|       - | 4997 | `	int bStrict;` |
|       - | 4998 | `	sxi32 rc;` |
|       - | 4999 | `	sxu32 n;` |
|       3 | 5000 | `	if( nArg < 1 ){` |
|       - | 5001 | `		/* Missing arguments,return NULL */` |
|     ! 0 | 5002 | `		ph7_result_null(pCtx);` |
|     ! 0 | 5003 | `		return PH7_OK;` |
|       - | 5004 | `	}` |
|       - | 5005 | `	/* Make sure we are dealing with a valid hashmap */` |
|       3 | 5006 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|       - | 5007 | `		/* Invalid argument,return NULL */` |
|     ! 0 | 5008 | `		ph7_result_null(pCtx);` |
|     ! 0 | 5009 | `		return PH7_OK;` |
|       - | 5010 | `	}` |
|       3 | 5011 | `	bStrict = FALSE;` |
|       3 | 5012 | `	if( nArg > 1 ){` |
|     ! 0 | 5013 | `		bStrict = ph7_value_to_int(apArg[1]) == 3 /* SORT_REGULAR */ ? 1 : 0;` |
|     ! 0 | 5014 | `	}` |
|       - | 5015 | `	/* Point to the internal representation of the input hashmap */` |
|       3 | 5016 | `	pSrc = (ph7_hashmap *)apArg[0]->x.pOther;` |
|       - | 5017 | `	/* Create a new array */` |
|       3 | 5018 | `	pArray = ph7_context_new_array(pCtx);` |
|       3 | 5019 | `	if( pArray == 0 ){` |
|     ! 0 | 5020 | `		ph7_result_null(pCtx);` |
|     ! 0 | 5021 | `		return PH7_OK;` |
|       - | 5022 | `	}` |
|       - | 5023 | `	/* Perform the requested operation */` |
|       3 | 5024 | `	pEntry = pSrc->pFirst;` |
|      13 | 5025 | `	for( n = 0 ; n < pSrc->nEntry ; n++ ){` |
|      11 | 5026 | `		pNeedle = HashmapExtractNodeValue(pEntry);` |
|      11 | 5027 | `		rc = SXERR_NOTFOUND;` |
|      11 | 5028 | `		if( pNeedle ){` |
|      11 | 5029 | `			rc = HashmapFindValue((ph7_hashmap *)pArray->x.pOther,pNeedle,0,bStrict);` |
|       5 | 5030 | `		}` |
|      11 | 5031 | `		if( rc != SXRET_OK ){` |
|       - | 5032 | `			/* Perform the insertion */` |
|       7 | 5033 | `			HashmapInsertNode((ph7_hashmap *)pArray->x.pOther,pEntry,TRUE);` |
|       3 | 5034 | `		}` |
|       - | 5035 | `		/* Point to the next entry */` |
|      11 | 5036 | `		pEntry = pEntry->pPrev; /* Reverse link */` |
|       6 | 5037 | `	}` |
|       - | 5038 | `	/* Return the freshly created array */` |
|       3 | 5039 | `	ph7_result_value(pCtx,pArray);` |
|       3 | 5040 | `	return PH7_OK;` |
|       2 | 5041 |  |
|       - | 5042 | `/*` |
|       - | 5043 | ` * array array_flip(array $input)` |
|       - | 5044 | ` *  Exchanges all keys with their associated values in an array.` |
|       - | 5045 | ` * Parameter` |
|       - | 5046 | ` *  $input` |
|       - | 5047 | ` *   Input array.` |
|       - | 5048 | ` * Return` |
|       - | 5049 | ` *   The flipped array on success or NULL on failure.` |
|       - | 5050 | ` */` |
|      34 | 5051 | `static int ph7_hashmap_flip(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       2 | 5052 |  |
|       - | 5053 | `	ph7_hashmap_node *pEntry;` |
|       - | 5054 | `	ph7_hashmap *pSrc;` |
|       - | 5055 | `	ph7_value *pArray;` |
|       - | 5056 | `	ph7_value *pKey;` |
|       - | 5057 | `	ph7_value sVal;` |
|       - | 5058 | `	sxu32 n;` |
|       - | 5059 |  |
|       - | 5060 | `	/* PHP requires exactly one argument */` |
|      36 | 5061 | `	if( nArg != 1 ){` |
|       - | 5062 | `		/* Use ArgumentCountError like other array helpers */` |
|       7 | 5063 | `		return PH7_VmThrowException(pCtx,` |
|       - | 5064 | `			"ArgumentCountError",` |
|       - | 5065 | `			"array_flip() expects exactly 1 argument, %d given",` |
|       2 | 5066 | `			nArg` |
|       - | 5067 | `			);` |
|       - | 5068 | `	}` |
|       - | 5069 | `	/* Make sure we are dealing with a valid hashmap */` |
|      32 | 5070 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|       - | 5071 | `		/* Type mismatch -> TypeError */` |
|       7 | 5072 | `		return PH7_VmThrowException(pCtx,` |
|       - | 5073 | `			"TypeError",` |
|       - | 5074 | `			"array_flip(): Argument #1 ($array) must be of type array, %s given",` |
|       2 | 5075 | `			ph7_type_name(apArg[0])` |
|       - | 5076 | `			);` |
|       - | 5077 | `	}` |
|       - | 5078 | `	/* Point to the internal representation of the input hashmap */` |
|      27 | 5079 | `	pSrc = (ph7_hashmap *)apArg[0]->x.pOther;` |
|       - | 5080 | `	/* Create a new array */` |
|      27 | 5081 | `	pArray = ph7_context_new_array(pCtx);` |
|      27 | 5082 | `	if( pArray == 0 ){` |
|     ! 0 | 5083 | `		ph7_result_null(pCtx);` |
|     ! 0 | 5084 | `		return PH7_OK;` |
|       - | 5085 | `	}` |
|       - | 5086 | `	/* Start processing */` |
|      27 | 5087 | `	pEntry = pSrc->pFirst;` |
|   22263 | 5088 | `	for( n = 0 ; n < pSrc->nEntry ; n++ ){` |
|       - | 5089 | `		/* Extract the node value (will become a key in the result) */` |
|   22237 | 5090 | `		pKey = HashmapExtractNodeValue(pEntry);` |
|   22237 | 5091 | `		if( pKey ){` |
|       - | 5092 | `			/* NULL values are not valid keys either, PHP emits a warning */` |
|   22237 | 5093 | `			if( pKey->iFlags & MEMOBJ_NULL ){` |
|       3 | 5094 | `				PH7_VmThrowError(pCtx->pVm,0,PH7_CTX_WARNING,` |
|       - | 5095 | `					"array_flip(): Can only flip string and integer values, entry skipped"` |
|       - | 5096 | `					);` |
|   22236 | 5097 | `			}else if( (pKey->iFlags & MEMOBJ_INT) \|\| (pKey->iFlags & MEMOBJ_STRING) ){` |
|       - | 5098 | `				/* Prepare the value for insertion (original key) */` |
|   22227 | 5099 | `				if( pEntry->iType == HASHMAP_INT_NODE ){` |
|   20001 | 5100 | `					PH7_MemObjInitFromInt(pSrc->pVm,&sVal,pEntry->xKey.iKey);` |
|   10001 | 5101 | `				}else{` |
|       - | 5102 | `					SyString sStr;` |
|    2227 | 5103 | `					SyStringInitFromBuf(&sStr,SyBlobData(&pEntry->xKey.sKey),SyBlobLength(&pEntry->xKey.sKey));` |
|    2227 | 5104 | `					PH7_MemObjInitFromString(pSrc->pVm,&sVal,&sStr);` |
|       - | 5105 | `				}` |
|       - | 5106 | `				/* Perform the insertion */` |
|   22227 | 5107 | `				ph7_array_add_elem(pArray,pKey,&sVal);` |
|       - | 5108 | `				/* Safely release the value because each inserted entry` |
|       - | 5109 | `				 * has its own private copy of the value.` |
|       - | 5110 | `				 */` |
|   22227 | 5111 | `				PH7_MemObjRelease(&sVal);` |
|   11114 | 5112 | `			}else{` |
|       - | 5113 | `				/* Unsupported value type -> emit warning and skip the entry */` |
|       9 | 5114 | `				PH7_VmThrowError(pCtx->pVm,0,PH7_CTX_WARNING,` |
|       - | 5115 | `					"array_flip(): Can only flip string and integer values, entry skipped"` |
|       - | 5116 | `					);` |
|       - | 5117 | `			}` |
|   11118 | 5118 | `		}` |
|       - | 5119 | `		/* Point to the next entry */` |
|   22237 | 5120 | `		pEntry = pEntry->pPrev; /* Reverse link */` |
|   11119 | 5121 | `	}` |
|       - | 5122 | `	/* Return the freshly created array */` |
|      27 | 5123 | `	ph7_result_value(pCtx,pArray);` |
|      27 | 5124 | `	return PH7_OK;` |
|      19 | 5125 |  |
|       - | 5126 | `/*` |
|       - | 5127 | ` * number array_sum(array $array )` |
|       - | 5128 | ` *  Calculate the sum of values in an array.` |
|       - | 5129 | ` * Parameters` |
|       - | 5130 | ` *  $array: The input array.` |
|       - | 5131 | ` * Return` |
|       - | 5132 | ` *  Returns the sum of values as an integer or float.` |
|       - | 5133 | ` */` |
|      24 | 5134 | `static void DoubleSum(ph7_context *pCtx,ph7_hashmap *pMap)` |
|       1 | 5135 |  |
|       - | 5136 | `	ph7_hashmap_node *pEntry;` |
|       - | 5137 | `	ph7_value *pObj;` |
|      25 | 5138 | `	double dSum = 0;` |
|       - | 5139 | `	sxu32 n;` |
|      25 | 5140 | `	pEntry = pMap->pFirst;` |
|      91 | 5141 | `	for( n = 0 ; n < pMap->nEntry ; n++ ){` |
|      67 | 5142 | `		pObj = HashmapExtractNodeValue(pEntry);` |
|      67 | 5143 | `		if( pObj ){` |
|      67 | 5144 | `			if( pObj->iFlags & MEMOBJ_REAL ){` |
|      29 | 5145 | `				dSum += pObj->rVal;` |
|      53 | 5146 | `			}else if( pObj->iFlags & (MEMOBJ_INT\|MEMOBJ_BOOL) ){` |
|      21 | 5147 | `				dSum += (double)pObj->x.iVal;` |
|      29 | 5148 | `			}else if( pObj->iFlags & MEMOBJ_STRING ){` |
|      15 | 5149 | `				if( SyBlobLength(&pObj->sBlob) > 0 ){` |
|      13 | 5150 | `					double dv = 0;` |
|      13 | 5151 | `					SyStrToReal((const char *)SyBlobData(&pObj->sBlob),SyBlobLength(&pObj->sBlob),(void *)&dv,0);` |
|      13 | 5152 | `					dSum += dv;` |
|       7 | 5153 | `				}` |
|      12 | 5154 | `			}else if( pObj->iFlags & MEMOBJ_HASHMAP ){` |
|       3 | 5155 | `				PH7_VmThrowError(pCtx->pVm,0,PH7_CTX_WARNING,` |
|       - | 5156 | `					"array_sum(): Addition is not supported on type array");` |
|       4 | 5157 | `			}else if( pObj->iFlags & MEMOBJ_OBJ ){` |
|     ! 0 | 5158 | `				PH7_VmThrowError(pCtx->pVm,0,PH7_CTX_WARNING,` |
|       - | 5159 | `					"array_sum(): Addition is not supported on type object");` |
|       3 | 5160 | `			}else if( pObj->iFlags & MEMOBJ_RES ){` |
|     ! 0 | 5161 | `				PH7_VmThrowError(pCtx->pVm,0,PH7_CTX_WARNING,` |
|       - | 5162 | `					"array_sum(): Addition is not supported on type resource");` |
|     ! 0 | 5163 | `			}` |
|       - | 5164 | `			/* NULL is silently treated as 0 (matches PHP) */` |
|      33 | 5165 | `		}` |
|       - | 5166 | `		/* Point to the next entry */` |
|      67 | 5167 | `		pEntry = pEntry->pPrev; /* Reverse link */` |
|      34 | 5168 | `	}` |
|       - | 5169 | `	/* Return sum */` |
|      25 | 5170 | `	ph7_result_double(pCtx,dSum);` |
|      25 | 5171 |  |
|      18 | 5172 | `static void Int64Sum(ph7_context *pCtx,ph7_hashmap *pMap)` |
|       2 | 5173 |  |
|       - | 5174 | `	ph7_hashmap_node *pEntry;` |
|       - | 5175 | `	ph7_value *pObj;` |
|      20 | 5176 | `	sxi64 nSum = 0;` |
|       - | 5177 | `	sxu32 n;` |
|      20 | 5178 | `	pEntry = pMap->pFirst;` |
|      80 | 5179 | `	for( n = 0 ; n < pMap->nEntry ; n++ ){` |
|      62 | 5180 | `		pObj = HashmapExtractNodeValue(pEntry);` |
|      62 | 5181 | `		if( pObj ){` |
|      62 | 5182 | `			if( pObj->iFlags & (MEMOBJ_INT\|MEMOBJ_BOOL) ){` |
|      52 | 5183 | `				nSum += pObj->x.iVal;` |
|      36 | 5184 | `			}else if( pObj->iFlags & MEMOBJ_STRING ){` |
|       7 | 5185 | `				if( SyBlobLength(&pObj->sBlob) > 0 ){` |
|       5 | 5186 | `					sxi64 nv = 0;` |
|       5 | 5187 | `					SyStrToInt64((const char *)SyBlobData(&pObj->sBlob),SyBlobLength(&pObj->sBlob),(void *)&nv,0);` |
|       5 | 5188 | `					nSum += nv;` |
|       3 | 5189 | `				}` |
|       8 | 5190 | `			}else if( pObj->iFlags & MEMOBJ_HASHMAP ){` |
|       3 | 5191 | `				PH7_VmThrowError(pCtx->pVm,0,PH7_CTX_WARNING,` |
|       - | 5192 | `					"array_sum(): Addition is not supported on type array");` |
|       4 | 5193 | `			}else if( pObj->iFlags & MEMOBJ_OBJ ){` |
|     ! 0 | 5194 | `				PH7_VmThrowError(pCtx->pVm,0,PH7_CTX_WARNING,` |
|       - | 5195 | `					"array_sum(): Addition is not supported on type object");` |
|       3 | 5196 | `			}else if( pObj->iFlags & MEMOBJ_RES ){` |
|     ! 0 | 5197 | `				PH7_VmThrowError(pCtx->pVm,0,PH7_CTX_WARNING,` |
|       - | 5198 | `					"array_sum(): Addition is not supported on type resource");` |
|     ! 0 | 5199 | `			}` |
|       - | 5200 | `			/* NULL is silently treated as 0 (matches PHP) */` |
|      30 | 5201 | `		}` |
|       - | 5202 | `		/* Point to the next entry */` |
|      62 | 5203 | `		pEntry = pEntry->pPrev; /* Reverse link */` |
|      32 | 5204 | `	}` |
|       - | 5205 | `	/* Return sum */` |
|      20 | 5206 | `	ph7_result_int64(pCtx,nSum);` |
|      20 | 5207 |  |
|       - | 5208 | `/* number array_sum(array $array )` |
|       - | 5209 | ` * (See block-coment above)` |
|       - | 5210 | ` */` |
|      52 | 5211 | `static int ph7_hashmap_sum(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       2 | 5212 |  |
|       - | 5213 | `	ph7_hashmap_node *pEntry;` |
|       - | 5214 | `	ph7_hashmap *pMap;` |
|       - | 5215 | `	ph7_value *pObj;` |
|      54 | 5216 | `	int useDouble = 0;` |
|       - | 5217 | `	sxu32 n;` |
|       - | 5218 | `	/* PHP requires exactly one argument */` |
|      54 | 5219 | `	if( nArg != 1 ){` |
|       7 | 5220 | `		return PH7_VmThrowException(pCtx,` |
|       - | 5221 | `			"ArgumentCountError",` |
|       - | 5222 | `			"array_sum() expects exactly 1 argument, %d given",` |
|       2 | 5223 | `			nArg` |
|       - | 5224 | `			);` |
|       - | 5225 | `	}` |
|       - | 5226 | `	/* Make sure we are dealing with a valid hashmap */` |
|      50 | 5227 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|       - | 5228 | `		/* Type mismatch -> TypeError */` |
|       7 | 5229 | `		return PH7_VmThrowException(pCtx,` |
|       - | 5230 | `			"TypeError",` |
|       - | 5231 | `			"array_sum(): Argument #1 ($array) must be of type array, %s given",` |
|       2 | 5232 | `			ph7_type_name(apArg[0])` |
|       - | 5233 | `			);` |
|       - | 5234 | `	}` |
|      46 | 5235 | `	pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|      46 | 5236 | `	if( pMap->nEntry < 1 ){` |
|       - | 5237 | `		/* Nothing to compute,return 0 */` |
|       3 | 5238 | `		ph7_result_int(pCtx,0);` |
|       3 | 5239 | `		return PH7_OK;` |
|       - | 5240 | `	}` |
|       - | 5241 | `	/* Scan all elements: if any value is a float, use floating-point` |
|       - | 5242 | `	 * arithmetic for the entire sum (matches PHP behaviour).` |
|       - | 5243 | `	 */` |
|      44 | 5244 | `	pEntry = pMap->pFirst;` |
|     112 | 5245 | `	for( n = 0 ; n < pMap->nEntry ; n++ ){` |
|      94 | 5246 | `		pObj = HashmapExtractNodeValue(pEntry);` |
|      94 | 5247 | `		if( pObj ){` |
|      94 | 5248 | `			if( pObj->iFlags & MEMOBJ_REAL ){` |
|      19 | 5249 | `				useDouble = 1;` |
|      19 | 5250 | `				break;` |
|       - | 5251 | `			}` |
|      76 | 5252 | `			if( pObj->iFlags & MEMOBJ_STRING ){` |
|      13 | 5253 | `				const char *zStr = (const char *)SyBlobData(&pObj->sBlob);` |
|      13 | 5254 | `				sxu32 nLen = SyBlobLength(&pObj->sBlob);` |
|       - | 5255 | `				sxu32 i;` |
|      23 | 5256 | `				for( i = 0 ; i < nLen ; i++ ){` |
|      17 | 5257 | `					if( zStr[i] == '.' \|\| zStr[i] == 'e' \|\| zStr[i] == 'E' ){` |
|       7 | 5258 | `						useDouble = 1;` |
|       7 | 5259 | `						break;` |
|       - | 5260 | `					}` |
|       6 | 5261 | `				}` |
|      13 | 5262 | `				if( useDouble ){` |
|       7 | 5263 | `					break;` |
|       - | 5264 | `				}` |
|       3 | 5265 | `			}` |
|      34 | 5266 | `		}` |
|      70 | 5267 | `		pEntry = pEntry->pPrev;` |
|      36 | 5268 | `	}` |
|      44 | 5269 | `	if( useDouble ){` |
|      25 | 5270 | `		DoubleSum(pCtx,pMap);` |
|      13 | 5271 | `	}else{` |
|      20 | 5272 | `		Int64Sum(pCtx,pMap);` |
|       - | 5273 | `	}` |
|      44 | 5274 | `	return PH7_OK;` |
|      28 | 5275 |  |
|       - | 5276 | `/*` |
|       - | 5277 | ` * number array_product(array $array )` |
|       - | 5278 | ` *  Calculate the product of values in an array.` |
|       - | 5279 | ` * Parameters` |
|       - | 5280 | ` *  $array: The input array.` |
|       - | 5281 | ` * Return` |
|       - | 5282 | ` *  Returns the product of values as an integer or float.` |
|       - | 5283 | ` */` |
|     ! 0 | 5284 | `static void DoubleProd(ph7_context *pCtx,ph7_hashmap *pMap)` |
|     ! 0 | 5285 |  |
|       - | 5286 | `	ph7_hashmap_node *pEntry;` |
|       - | 5287 | `	ph7_value *pObj;` |
|       - | 5288 | `	double dProd;` |
|       - | 5289 | `	sxu32 n;` |
|     ! 0 | 5290 | `	pEntry = pMap->pFirst;` |
|     ! 0 | 5291 | `	dProd = 1;` |
|     ! 0 | 5292 | `	for( n = 0 ; n < pMap->nEntry ; n++ ){` |
|     ! 0 | 5293 | `		pObj = HashmapExtractNodeValue(pEntry);` |
|     ! 0 | 5294 | `		if( pObj && (pObj->iFlags & (MEMOBJ_NULL\|MEMOBJ_HASHMAP\|MEMOBJ_OBJ\|MEMOBJ_RES)) == 0){` |
|     ! 0 | 5295 | `			if( pObj->iFlags & MEMOBJ_REAL ){` |
|     ! 0 | 5296 | `				dProd *= pObj->rVal;` |
|     ! 0 | 5297 | `			}else if( pObj->iFlags & (MEMOBJ_INT\|MEMOBJ_BOOL) ){` |
|     ! 0 | 5298 | `				dProd *= (double)pObj->x.iVal;` |
|     ! 0 | 5299 | `			}else if( pObj->iFlags & MEMOBJ_STRING ){` |
|     ! 0 | 5300 | `				if( SyBlobLength(&pObj->sBlob) > 0 ){` |
|     ! 0 | 5301 | `					double dv = 0;` |
|     ! 0 | 5302 | `					SyStrToReal((const char *)SyBlobData(&pObj->sBlob),SyBlobLength(&pObj->sBlob),(void *)&dv,0);` |
|     ! 0 | 5303 | `					dProd *= dv;` |
|     ! 0 | 5304 | `				}` |
|     ! 0 | 5305 | `			}` |
|     ! 0 | 5306 | `		}` |
|       - | 5307 | `		/* Point to the next entry */` |
|     ! 0 | 5308 | `		pEntry = pEntry->pPrev; /* Reverse link */` |
|     ! 0 | 5309 | `	}` |
|       - | 5310 | `	/* Return product */` |
|     ! 0 | 5311 | `	ph7_result_double(pCtx,dProd);` |
|     ! 0 | 5312 |  |
|     ! 0 | 5313 | `static void Int64Prod(ph7_context *pCtx,ph7_hashmap *pMap)` |
|     ! 0 | 5314 |  |
|       - | 5315 | `	ph7_hashmap_node *pEntry;` |
|       - | 5316 | `	ph7_value *pObj;` |
|       - | 5317 | `	sxi64 nProd;` |
|       - | 5318 | `	sxu32 n;` |
|     ! 0 | 5319 | `	pEntry = pMap->pFirst;` |
|     ! 0 | 5320 | `	nProd = 1;` |
|     ! 0 | 5321 | `	for( n = 0 ; n < pMap->nEntry ; n++ ){` |
|     ! 0 | 5322 | `		pObj = HashmapExtractNodeValue(pEntry);` |
|     ! 0 | 5323 | `		if( pObj && (pObj->iFlags & (MEMOBJ_NULL\|MEMOBJ_HASHMAP\|MEMOBJ_OBJ\|MEMOBJ_RES)) == 0){` |
|     ! 0 | 5324 | `			if( pObj->iFlags & MEMOBJ_REAL ){` |
|     ! 0 | 5325 | `				nProd *= (sxi64)pObj->rVal;` |
|     ! 0 | 5326 | `			}else if( pObj->iFlags & (MEMOBJ_INT\|MEMOBJ_BOOL) ){` |
|     ! 0 | 5327 | `				nProd *= pObj->x.iVal;` |
|     ! 0 | 5328 | `			}else if( pObj->iFlags & MEMOBJ_STRING ){` |
|     ! 0 | 5329 | `				if( SyBlobLength(&pObj->sBlob) > 0 ){` |
|     ! 0 | 5330 | `					sxi64 nv = 0;` |
|     ! 0 | 5331 | `					SyStrToInt64((const char *)SyBlobData(&pObj->sBlob),SyBlobLength(&pObj->sBlob),(void *)&nv,0);` |
|     ! 0 | 5332 | `					nProd *= nv;` |
|     ! 0 | 5333 | `				}` |
|     ! 0 | 5334 | `			}` |
|     ! 0 | 5335 | `		}` |
|       - | 5336 | `		/* Point to the next entry */` |
|     ! 0 | 5337 | `		pEntry = pEntry->pPrev; /* Reverse link */` |
|     ! 0 | 5338 | `	}` |
|       - | 5339 | `	/* Return product */` |
|     ! 0 | 5340 | `	ph7_result_int64(pCtx,nProd);` |
|     ! 0 | 5341 |  |
|       - | 5342 | `/* number array_product(array $array )` |
|       - | 5343 | ` * (See block-block comment above)` |
|       - | 5344 | ` */` |
|     ! 0 | 5345 | `static int ph7_hashmap_product(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     ! 0 | 5346 |  |
|       - | 5347 | `	ph7_hashmap *pMap;` |
|       - | 5348 | `	ph7_value *pObj;` |
|     ! 0 | 5349 | `	if( nArg < 1 ){` |
|       - | 5350 | `		/* Missing arguments,return 0 */` |
|     ! 0 | 5351 | `		ph7_result_int(pCtx,0);` |
|     ! 0 | 5352 | `		return PH7_OK;` |
|       - | 5353 | `	}` |
|       - | 5354 | `	/* Make sure we are dealing with a valid hashmap */` |
|     ! 0 | 5355 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|       - | 5356 | `		/* Invalid argument,return 0 */` |
|     ! 0 | 5357 | `		ph7_result_int(pCtx,0);` |
|     ! 0 | 5358 | `		return PH7_OK;` |
|       - | 5359 | `	}` |
|     ! 0 | 5360 | `	pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|     ! 0 | 5361 | `	if( pMap->nEntry < 1 ){` |
|       - | 5362 | `		/* Nothing to compute,return 0 */` |
|     ! 0 | 5363 | `		ph7_result_int(pCtx,0);` |
|     ! 0 | 5364 | `		return PH7_OK;` |
|       - | 5365 | `	}` |
|       - | 5366 | `	/* If the first element is of type float,then perform floating` |
|       - | 5367 | `	 * point computaion.Otherwise switch to int64 computaion.` |
|       - | 5368 | `	 */` |
|     ! 0 | 5369 | `	pObj = HashmapExtractNodeValue(pMap->pFirst);` |
|     ! 0 | 5370 | `	if( pObj == 0 ){` |
|     ! 0 | 5371 | `		ph7_result_int(pCtx,0);` |
|     ! 0 | 5372 | `		return PH7_OK;` |
|       - | 5373 | `	}` |
|     ! 0 | 5374 | `	if( pObj->iFlags & MEMOBJ_REAL ){` |
|     ! 0 | 5375 | `		DoubleProd(pCtx,pMap);` |
|     ! 0 | 5376 | `	}else{` |
|     ! 0 | 5377 | `		Int64Prod(pCtx,pMap);` |
|       - | 5378 | `	}` |
|     ! 0 | 5379 | `	return PH7_OK;` |
|     ! 0 | 5380 |  |
|       - | 5381 | `/*` |
|       - | 5382 | ` * value array_rand(array $input[,int $num_req = 1 ])` |
|       - | 5383 | ` *  Pick one or more random entries out of an array.` |
|       - | 5384 | ` * Parameters` |
|       - | 5385 | ` * $input` |
|       - | 5386 | ` *  The input array.` |
|       - | 5387 | ` * $num_req` |
|       - | 5388 | ` *  Specifies how many entries you want to pick.` |
|       - | 5389 | ` * Return` |
|       - | 5390 | ` *  If you are picking only one entry, array_rand() returns the key for a random entry.` |
|       - | 5391 | ` *  Otherwise, it returns an array of keys for the random entries.` |
|       - | 5392 | ` *  NULL is returned on failure.` |
|       - | 5393 | ` */` |
|       6 | 5394 | `static int ph7_hashmap_rand(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       1 | 5395 |  |
|       - | 5396 | `	ph7_hashmap_node *pNode;` |
|       - | 5397 | `	ph7_hashmap *pMap;` |
|       7 | 5398 | `	int nItem = 1;` |
|       7 | 5399 | `	if( nArg < 1 ){` |
|       - | 5400 | `		/* Missing argument,return NULL */` |
|     ! 0 | 5401 | `		ph7_result_null(pCtx);` |
|     ! 0 | 5402 | `		return PH7_OK;` |
|       - | 5403 | `	}` |
|       - | 5404 | `	/* Make sure we are dealing with an array */` |
|       7 | 5405 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|     ! 0 | 5406 | `		ph7_result_null(pCtx);` |
|     ! 0 | 5407 | `		return PH7_OK;` |
|       - | 5408 | `	}` |
|       - | 5409 | `	/* Point to the internal representation of the input hashmap */` |
|       7 | 5410 | `	pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|       7 | 5411 | `	if(pMap->nEntry < 1 ){` |
|       - | 5412 | `		/* Empty hashmap,return NULL */` |
|     ! 0 | 5413 | `		ph7_result_null(pCtx);` |
|     ! 0 | 5414 | `		return PH7_OK;` |
|       - | 5415 | `	}` |
|       7 | 5416 | `	if( nArg > 1 ){` |
|       3 | 5417 | `		nItem = ph7_value_to_int(apArg[1]);` |
|       1 | 5418 | `	}` |
|       7 | 5419 | `	if( nItem < 2 ){` |
|       - | 5420 | `		sxu32 nEntry;` |
|       - | 5421 | `		/* Select a random number */` |
|       5 | 5422 | `		nEntry = PH7_VmRandomNum(pMap->pVm) % pMap->nEntry;` |
|       - | 5423 | `		/* Extract the desired entry.` |
|       - | 5424 | `		 * Note that we perform a linear lookup here (later version must change this)` |
|       - | 5425 | `		 */` |
|       5 | 5426 | `		if( nEntry > pMap->nEntry / 2 ){` |
|       1 | 5427 | `			pNode = pMap->pLast;` |
|       1 | 5428 | `			nEntry = pMap->nEntry - nEntry;` |
|       1 | 5429 | `			if( nEntry > 1 ){` |
|     ! 0 | 5430 | `				for(;;){` |
|     ! 0 | 5431 | `					if( nEntry == 0 ){` |
|     ! 0 | 5432 | `						break;` |
|       - | 5433 | `					}` |
|       - | 5434 | `					/* Point to the previous entry */` |
|     ! 0 | 5435 | `					pNode = pNode->pNext; /* Reverse link */` |
|     ! 0 | 5436 | `					nEntry--;` |
|     ! 0 | 5437 | `				}` |
|     ! 0 | 5438 | `			}` |
|       1 | 5439 | `		}else{` |
|       4 | 5440 | `			pNode = pMap->pFirst;` |
|       1 | 5441 | `			for(;;){` |
|       4 | 5442 | `				if( nEntry == 0 ){` |
|       4 | 5443 | `					break;` |
|       - | 5444 | `				}` |
|       - | 5445 | `				/* Point to the next entry */` |
|       1 | 5446 | `				pNode = pNode->pPrev; /* Reverse link */` |
|       1 | 5447 | `				nEntry--;` |
|       1 | 5448 | `			}` |
|       - | 5449 | `		}` |
|       5 | 5450 | `		if( pNode->iType == HASHMAP_INT_NODE ){` |
|       - | 5451 | `			/* Int key */` |
|       3 | 5452 | `			ph7_result_int64(pCtx,pNode->xKey.iKey);` |
|       2 | 5453 | `		}else{` |
|       - | 5454 | `			/* Blob key */` |
|       3 | 5455 | `			ph7_result_string(pCtx,(const char *)SyBlobData(&pNode->xKey.sKey),(int)SyBlobLength(&pNode->xKey.sKey));` |
|       - | 5456 | `		}` |
|       3 | 5457 | `	}else{` |
|       - | 5458 | `		ph7_value sKey,*pArray;` |
|       - | 5459 | `		ph7_hashmap *pDest;` |
|       - | 5460 | `		/* Create a new array */` |
|       3 | 5461 | `		pArray = ph7_context_new_array(pCtx);` |
|       3 | 5462 | `		if( pArray == 0 ){` |
|     ! 0 | 5463 | `			ph7_result_null(pCtx);` |
|     ! 0 | 5464 | `			return PH7_OK;` |
|       - | 5465 | `		}` |
|       - | 5466 | `		/* Point to the internal representation of the hashmap */` |
|       3 | 5467 | `		pDest = (ph7_hashmap *)pArray->x.pOther;` |
|       3 | 5468 | `		PH7_MemObjInit(pDest->pVm,&sKey);` |
|       - | 5469 | `		/* Copy the first n items */` |
|       3 | 5470 | `		pNode = pMap->pFirst;` |
|       3 | 5471 | `		if( nItem > (int)pMap->nEntry ){` |
|     ! 0 | 5472 | `			nItem = (int)pMap->nEntry;` |
|     ! 0 | 5473 | `		}` |
|       7 | 5474 | `		while( nItem > 0){` |
|       5 | 5475 | `			PH7_HashmapExtractNodeKey(pNode,&sKey);` |
|       5 | 5476 | `			PH7_HashmapInsert(pDest,0/* Automatic index assign*/,&sKey);` |
|       5 | 5477 | `			PH7_MemObjRelease(&sKey);` |
|       - | 5478 | `			/* Point to the next entry */` |
|       5 | 5479 | `			pNode = pNode->pPrev; /* Reverse link */` |
|       5 | 5480 | `			nItem--;` |
|       1 | 5481 | `		}` |
|       - | 5482 | `		/* Shuffle the array */` |
|       3 | 5483 | `		HashmapMergeSort(pDest,HashmapCmpCallback7,0);` |
|       - | 5484 | `		/* Rehash node */` |
|       3 | 5485 | `		HashmapSortRehash(pDest);` |
|       - | 5486 | `		/* Return the random array */` |
|       3 | 5487 | `		ph7_result_value(pCtx,pArray);` |
|       - | 5488 | `	}` |
|       7 | 5489 | `	return PH7_OK;` |
|       4 | 5490 |  |
|       - | 5491 | `/*` |
|       - | 5492 | ` * array array_chunk (array $input,int $size [,bool $preserve_keys = false ])` |
|       - | 5493 | ` *  Split an array into chunks.` |
|       - | 5494 | ` * Parameters` |
|       - | 5495 | ` * $input` |
|       - | 5496 | ` *   The array to work on` |
|       - | 5497 | ` * $size` |
|       - | 5498 | ` *   The size of each chunk` |
|       - | 5499 | ` * $preserve_keys` |
|       - | 5500 | ` *   When set to TRUE keys will be preserved. Default is FALSE which will reindex` |
|       - | 5501 | ` *   the chunk numerically.` |
|       - | 5502 | ` * Return` |
|       - | 5503 | ` *  Returns a multidimensional numerically indexed array, starting with` |
|       - | 5504 | ` *  zero, with each dimension containing size elements.` |
|       - | 5505 | ` */` |
|      42 | 5506 | `static int ph7_hashmap_chunk(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       2 | 5507 |  |
|       - | 5508 | `	ph7_value *pArray,*pChunk;` |
|       - | 5509 | `	ph7_hashmap_node *pEntry;` |
|       - | 5510 | `	ph7_hashmap *pMap;` |
|       - | 5511 | `	int bPreserve;` |
|       - | 5512 | `	sxu32 nChunk;` |
|       - | 5513 | `	sxu32 nSize;` |
|       - | 5514 | `	sxu32 n;` |
|       - | 5515 | `	/* Argument count and types follow PHP semantics. */` |
|      44 | 5516 | `	if( nArg < 2 ){` |
|       - | 5517 | `		/* fewer than required arguments -> ArgumentCountError */` |
|       4 | 5518 | `		return PH7_VmThrowException(pCtx,` |
|       - | 5519 | `			"ArgumentCountError",` |
|       - | 5520 | `			"array_chunk() expects at least 2 arguments, %d given",` |
|       1 | 5521 | `			nArg` |
|       - | 5522 | `			);` |
|       - | 5523 | `	}` |
|      42 | 5524 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|       4 | 5525 | `		return PH7_VmThrowException(pCtx,` |
|       - | 5526 | `			"TypeError",` |
|       - | 5527 | `			"array_chunk(): Argument #1 ($array) must be of type array, %s given",` |
|       1 | 5528 | `			ph7_type_name(apArg[0])` |
|       - | 5529 | `			);` |
|       - | 5530 | `	}` |
|       - | 5531 | `	/* Create a new array */` |
|      40 | 5532 | `	pArray = ph7_context_new_array(pCtx);` |
|      40 | 5533 | `	if( pArray == 0 ){` |
|     ! 0 | 5534 | `		ph7_result_null(pCtx);` |
|     ! 0 | 5535 | `		return PH7_OK;` |
|       - | 5536 | `	}` |
|       - | 5537 | `	/* Point to the internal representation of the input hashmap */` |
|      40 | 5538 | `	pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|       - | 5539 | `	/* Extract and validate the chunk size argument. */` |
|       - | 5540 | `	/* Reject types that cannot be sensibly converted to an integer. */` |
|      76 | 5541 | `	if( ph7_value_is_array(apArg[1]) \|\| ph7_value_is_object(apArg[1]) \|\|` |
|      78 | 5542 | `		ph7_value_is_resource(apArg[1]) \|\| ph7_value_is_null(apArg[1]) \|\|` |
|      38 | 5543 | `		ph7_value_is_bool(apArg[1]) ){` |
|     ! 0 | 5544 | `		return PH7_VmThrowException(pCtx,` |
|       - | 5545 | `			"TypeError",` |
|       - | 5546 | `			"array_chunk(): Argument #2 ($length) must be of type int, %s given",` |
|     ! 0 | 5547 | `			ph7_type_name(apArg[1])` |
|       - | 5548 | `			);` |
|       - | 5549 | `	}` |
|       - | 5550 | `	/* Strings that are non-numeric produce a TypeError.  Numeric` |
|       - | 5551 | `	 * strings are permitted; however those representing floats lose` |
|       - | 5552 | `	 * precision and PHP emits a deprecation warning. */` |
|      40 | 5553 | `	if( ph7_value_is_string(apArg[1]) ){` |
|       - | 5554 | `		int len;` |
|       3 | 5555 | `		sxu8 bReal = FALSE;` |
|       3 | 5556 | `		const char *zStr = ph7_value_to_string(apArg[1], &len);` |
|       3 | 5557 | `		if( SyStrIsNumeric(zStr, len, &bReal, 0) != SXRET_OK ){` |
|       3 | 5558 | `			return PH7_VmThrowException(pCtx,` |
|       - | 5559 | `				"TypeError",` |
|       - | 5560 | `				"array_chunk(): Argument #2 ($length) must be of type int, string given"` |
|       - | 5561 | `				);` |
|       - | 5562 | `		}` |
|     ! 0 | 5563 | `		if( bReal ){` |
|       - | 5564 | `			/* float-string -> warn but allow */` |
|     ! 0 | 5565 | `			ph7_context_throw_error_format(pCtx, E_DEPRECATED,` |
|       - | 5566 | `				"Implicit conversion from float-string \"%s\" to int loses precision",` |
|     ! 0 | 5567 | `				zStr` |
|       - | 5568 | `				);` |
|     ! 0 | 5569 | `		}` |
|     ! 0 | 5570 | `	}` |
|       - | 5571 | `	/* If the value is a float with a fractional component, emit a` |
|       - | 5572 | `	 * deprecation warning but continue.  The following conversion occurs` |
|       - | 5573 | `	 * later via ph7_value_to_int. */` |
|      38 | 5574 | `	if( ph7_value_is_float(apArg[1]) ){` |
|       3 | 5575 | `		double d = ph7_value_to_double(apArg[1]);` |
|       3 | 5576 | `		sxi64 i = (sxi64)d;` |
|       3 | 5577 | `		if( d != (double)i ){` |
|       4 | 5578 | `			ph7_context_throw_error_format(pCtx, E_DEPRECATED,` |
|       - | 5579 | `				"Implicit conversion from float %g to int loses precision",` |
|       1 | 5580 | `				d` |
|       - | 5581 | `				);` |
|       1 | 5582 | `		}` |
|       1 | 5583 | `	}` |
|       - | 5584 | `	/* Convert using ph7_value_to_int; now that float fractions are` |
|       - | 5585 | `	 * eliminated, this will not produce a warning. */` |
|       - | 5586 | `	{` |
|      38 | 5587 | `		sxi64 nSizeSigned = ph7_value_to_int(apArg[1]);` |
|      38 | 5588 | `		if( nSizeSigned < 1 ){` |
|       - | 5589 | `			/* size <= 0 -> ValueError */` |
|       5 | 5590 | `			return PH7_VmThrowException(pCtx,` |
|       - | 5591 | `				"ValueError",` |
|       - | 5592 | `				"array_chunk(): Argument #2 ($length) must be greater than 0"` |
|       - | 5593 | `				);` |
|       - | 5594 | `		}` |
|      34 | 5595 | `		nSize = (sxu32)nSizeSigned;` |
|       - | 5596 | `	}` |
|      34 | 5597 | `	if( nSize >= pMap->nEntry ){` |
|       - | 5598 | `		/* Return the whole array */` |
|       3 | 5599 | `		ph7_array_add_elem(pArray,0,apArg[0]);` |
|       3 | 5600 | `		ph7_result_value(pCtx,pArray);` |
|       3 | 5601 | `		return PH7_OK;` |
|       - | 5602 | `	}` |
|      32 | 5603 | `	bPreserve = 0;` |
|      32 | 5604 | `	if( nArg > 2 ){` |
|       - | 5605 | `		/* The third argument has a bool type hint in PHP.  Values that` |
|       - | 5606 | `		 * cannot be sensibly converted (arrays, objects, resources) are` |
|       - | 5607 | `		 * rejected with a TypeError.  Scalars and null coerce to bool` |
|       - | 5608 | `		 * normally, matching PHP behaviour. */` |
|      45 | 5609 | `		if( ph7_value_is_array(apArg[2]) \|\|` |
|      34 | 5610 | `			ph7_value_is_object(apArg[2]) \|\|` |
|      20 | 5611 | `			ph7_value_is_resource(apArg[2]) ){` |
|       7 | 5612 | `			return PH7_VmThrowException(pCtx,` |
|       - | 5613 | `				"TypeError",` |
|       - | 5614 | `				"array_chunk(): Argument #3 ($preserve_keys) must be of type bool, %s given",` |
|       4 | 5615 | `				ph7_type_name(apArg[2])` |
|       - | 5616 | `				);` |
|       - | 5617 | `		}` |
|      21 | 5618 | `		bPreserve = ph7_value_to_bool(apArg[2]);` |
|      10 | 5619 | `	}` |
|       - | 5620 | `	/* Start processing */` |
|      27 | 5621 | `	pEntry = pMap->pFirst;` |
|      27 | 5622 | `	nChunk = 0;` |
|      27 | 5623 | `	pChunk = 0;` |
|      27 | 5624 | `	n = pMap->nEntry;` |
|      56 | 5625 | `	for( ;; ){` |
|     113 | 5626 | `		if( n < 1 ){` |
|       - | 5627 | `			/* When the loop terminates we may still have a current chunk` |
|       - | 5628 | `			 * that hasn't been added to the result array.  The previous` |
|       - | 5629 | `			 * implementation only pushed it if nChunk>0 which dropped the` |
|       - | 5630 | `			 * final chunk when the input size was an exact multiple of` |
|       - | 5631 | `			 * the chunk length.  Always append the pending chunk if it` |
|       - | 5632 | `			 * exists. */` |
|      27 | 5633 | `			if( pChunk ){` |
|      27 | 5634 | `				ph7_array_add_elem(pArray,0,pChunk); /* Will have its own copy */` |
|      13 | 5635 | `			}` |
|      27 | 5636 | `			break;` |
|       - | 5637 | `		}` |
|      87 | 5638 | `		if( nChunk < 1 ){` |
|      71 | 5639 | `			if( pChunk ){` |
|       - | 5640 | `				/* Put the first chunk */` |
|      45 | 5641 | `				ph7_array_add_elem(pArray,0,pChunk); /* Will have it's own copy */` |
|      22 | 5642 | `			}` |
|       - | 5643 | `			/* Create a new dimension */` |
|      71 | 5644 | `			pChunk = ph7_context_new_array(pCtx); /* Don't worry about freeing memory here,everything` |
|       - | 5645 | `												   * will be automatically released as soon we return` |
|       - | 5646 | `												   * from this function */` |
|      71 | 5647 | `			if( pChunk == 0 ){` |
|     ! 0 | 5648 | `				break;` |
|       - | 5649 | `			}` |
|      71 | 5650 | `			nChunk = nSize;` |
|      35 | 5651 | `		}` |
|       - | 5652 | `		/* Insert the entry */` |
|      87 | 5653 | `		HashmapInsertNode((ph7_hashmap *)pChunk->x.pOther,pEntry,bPreserve);` |
|       - | 5654 | `		/* Point to the next entry */` |
|      87 | 5655 | `		pEntry = pEntry->pPrev; /* Reverse link */` |
|      87 | 5656 | `		nChunk--;` |
|      87 | 5657 | `		n--;` |
|       1 | 5658 | `	}` |
|       - | 5659 | `	/* Return the multidimensional array */` |
|      27 | 5660 | `	ph7_result_value(pCtx,pArray);` |
|      27 | 5661 | `	return PH7_OK;` |
|      23 | 5662 |  |
|       - | 5663 | `/*` |
|       - | 5664 | ` * array array_pad(array $input,int $pad_size,value $pad_value)` |
|       - | 5665 | ` *  Pad array to the specified length with a value.` |
|       - | 5666 | ` * $input` |
|       - | 5667 | ` *   Initial array of values to pad.` |
|       - | 5668 | ` * $pad_size` |
|       - | 5669 | ` *   New size of the array.` |
|       - | 5670 | ` * $pad_value` |
|       - | 5671 | ` *   Value to pad if input is less than pad_size.` |
|       - | 5672 | ` */` |
|       8 | 5673 | `static int ph7_hashmap_pad(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       1 | 5674 |  |
|       - | 5675 | `	ph7_hashmap *pMap;` |
|       - | 5676 | `	ph7_value *pArray;` |
|       - | 5677 | `	int nEntry;` |
|       9 | 5678 | `	if( nArg < 3 \|\| !ph7_value_is_array(apArg[0]) ){` |
|       - | 5679 | `		/* Invalid arguments,return NULL */` |
|     ! 0 | 5680 | `		ph7_result_null(pCtx);` |
|     ! 0 | 5681 | `		return PH7_OK;` |
|       - | 5682 | `	}` |
|       - | 5683 | `	/* Create a new array */` |
|       9 | 5684 | `	pArray = ph7_context_new_array(pCtx);` |
|       9 | 5685 | `	if( pArray == 0 ){` |
|     ! 0 | 5686 | `		ph7_result_null(pCtx);` |
|     ! 0 | 5687 | `		return PH7_OK;` |
|       - | 5688 | `	}` |
|       - | 5689 | `	/* Point to the internal representation of the input hashmap */` |
|       9 | 5690 | `	pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|       - | 5691 | `	/* Extract the total number of desired entry to insert */` |
|       9 | 5692 | `	nEntry = ph7_value_to_int(apArg[1]);` |
|       9 | 5693 | `	if( nEntry < 0 ){` |
|       5 | 5694 | `		nEntry = -nEntry;` |
|       5 | 5695 | `		if( nEntry > 1048576 ){` |
|     ! 0 | 5696 | `			nEntry = 1048576; /* Limit imposed by PHP */` |
|     ! 0 | 5697 | `		}` |
|       5 | 5698 | `		if( nEntry > (int)pMap->nEntry ){` |
|       3 | 5699 | `			nEntry -= (int)pMap->nEntry;` |
|       - | 5700 | `			/* Insert given items first */` |
|       7 | 5701 | `			while( nEntry > 0 ){` |
|       5 | 5702 | `				ph7_array_add_elem(pArray,0,apArg[2]);` |
|       5 | 5703 | `				nEntry--;` |
|       1 | 5704 | `			}` |
|       - | 5705 | `			/* Merge the two arrays */` |
|       3 | 5706 | `			HashmapMerge(pMap,(ph7_hashmap *)pArray->x.pOther);` |
|       2 | 5707 | `		}else{` |
|       3 | 5708 | `			PH7_HashmapDup(pMap,(ph7_hashmap *)pArray->x.pOther);` |
|       1 | 5709 | `		}` |
|       7 | 5710 | `	}else if( nEntry > 0 ){` |
|       5 | 5711 | `		if( nEntry > 1048576 ){` |
|     ! 0 | 5712 | `			nEntry = 1048576; /* Limit imposed by PHP */` |
|     ! 0 | 5713 | `		}` |
|       5 | 5714 | `		if( nEntry > (int)pMap->nEntry ){` |
|       3 | 5715 | `			nEntry -= (int)pMap->nEntry;` |
|       - | 5716 | `			/* Merge the two arrays first */` |
|       3 | 5717 | `			HashmapMerge(pMap,(ph7_hashmap *)pArray->x.pOther);` |
|       - | 5718 | `			/* Insert given items */` |
|       7 | 5719 | `			while( nEntry > 0 ){` |
|       5 | 5720 | `				ph7_array_add_elem(pArray,0,apArg[2]);` |
|       5 | 5721 | `				nEntry--;` |
|       1 | 5722 | `			}` |
|       2 | 5723 | `		}else{` |
|       3 | 5724 | `			PH7_HashmapDup(pMap,(ph7_hashmap *)pArray->x.pOther);` |
|       - | 5725 | `		}` |
|       2 | 5726 | `	}` |
|       - | 5727 | `	/* Return the new array */` |
|       9 | 5728 | `	ph7_result_value(pCtx,pArray);` |
|       9 | 5729 | `	return PH7_OK;` |
|       5 | 5730 |  |
|       - | 5731 | `/*` |
|       - | 5732 | ` * array array_replace(array &$array,array &$array1,...)` |
|       - | 5733 | ` *  Replaces elements from passed arrays into the first array.` |
|       - | 5734 | ` * Parameters` |
|       - | 5735 | ` * $array` |
|       - | 5736 | ` *   The array in which elements are replaced.` |
|       - | 5737 | ` * $array1` |
|       - | 5738 | ` *   The array from which elements will be extracted.` |
|       - | 5739 | ` * ....` |
|       - | 5740 | ` *  More arrays from which elements will be extracted.` |
|       - | 5741 | ` *  Values from later arrays overwrite the previous values.` |
|       - | 5742 | ` * Return` |
|       - | 5743 | ` *  Returns an array, or NULL if an error occurs.` |
|       - | 5744 | ` */` |
|       2 | 5745 | `static int ph7_hashmap_replace(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       1 | 5746 |  |
|       - | 5747 | `	ph7_hashmap *pMap;` |
|       - | 5748 | `	ph7_value *pArray;` |
|       - | 5749 | `	int i;` |
|       3 | 5750 | `	if( nArg < 1 ){` |
|       - | 5751 | `		/* Invalid arguments,return NULL */` |
|     ! 0 | 5752 | `		ph7_result_null(pCtx);` |
|     ! 0 | 5753 | `		return PH7_OK;` |
|       - | 5754 | `	}` |
|       - | 5755 | `	/* Create a new array */` |
|       3 | 5756 | `	pArray = ph7_context_new_array(pCtx);` |
|       3 | 5757 | `	if( pArray == 0 ){` |
|     ! 0 | 5758 | `		ph7_result_null(pCtx);` |
|     ! 0 | 5759 | `		return PH7_OK;` |
|       - | 5760 | `	}` |
|       - | 5761 | `	/* Perform the requested operation */` |
|       7 | 5762 | `	for( i = 0 ; i < nArg ; i++ ){` |
|       5 | 5763 | `		if( !ph7_value_is_array(apArg[i]) ){` |
|     ! 0 | 5764 | `			continue;` |
|       - | 5765 | `		}` |
|       - | 5766 | `		/* Point to the internal representation of the input hashmap */` |
|       5 | 5767 | `		pMap = (ph7_hashmap *)apArg[i]->x.pOther;` |
|       5 | 5768 | `		HashmapOverwrite(pMap,(ph7_hashmap *)pArray->x.pOther);` |
|       3 | 5769 | `	}` |
|       - | 5770 | `	/* Return the new array */` |
|       3 | 5771 | `	ph7_result_value(pCtx,pArray);` |
|       3 | 5772 | `	return PH7_OK;` |
|       2 | 5773 |  |
|       - | 5774 | `/*` |
|       - | 5775 | ` * array array_filter(array $input [,callback $callback ])` |
|       - | 5776 | ` *  Filters elements of an array using a callback function.` |
|       - | 5777 | ` * Parameters` |
|       - | 5778 | ` *  $input` |
|       - | 5779 | ` *    The array to iterate over` |
|       - | 5780 | ` * $callback` |
|       - | 5781 | ` *    The callback function to use` |
|       - | 5782 | ` *    If no callback is supplied, all entries of input equal to FALSE (see converting to boolean)` |
|       - | 5783 | ` *    will be removed.` |
|       - | 5784 | ` * Return` |
|       - | 5785 | ` *  The filtered array.` |
|       - | 5786 | ` */` |
|      18 | 5787 | `static int ph7_hashmap_filter(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       2 | 5788 |  |
|       - | 5789 | `	ph7_hashmap_node *pEntry;` |
|       - | 5790 | `	ph7_hashmap *pMap;` |
|       - | 5791 | `	ph7_value *pArray;` |
|       - | 5792 | `	ph7_value sResult;   /* Callback result */` |
|       - | 5793 | `	ph7_value *pValue;` |
|       - | 5794 | `	sxi32 rc;` |
|       - | 5795 | `	int keep;` |
|       - | 5796 | `	sxu32 n;` |
|      20 | 5797 | `	if( nArg < 1 \|\| !ph7_value_is_array(apArg[0]) ){` |
|       - | 5798 | `		/* Invalid arguments,return NULL */` |
|       5 | 5799 | `		ph7_result_null(pCtx);` |
|       5 | 5800 | `		return PH7_OK;` |
|       - | 5801 | `	}` |
|       - | 5802 | `	/* Create a new array */` |
|      16 | 5803 | `	pArray = ph7_context_new_array(pCtx);` |
|      16 | 5804 | `	if( pArray == 0 ){` |
|     ! 0 | 5805 | `		ph7_result_null(pCtx);` |
|     ! 0 | 5806 | `		return PH7_OK;` |
|       - | 5807 | `	}` |
|       - | 5808 | `	/* Point to the internal representation of the input hashmap */` |
|      16 | 5809 | `	pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|      16 | 5810 | `	pEntry = pMap->pFirst;` |
|      16 | 5811 | `	PH7_MemObjInit(pMap->pVm,&sResult);` |
|      16 | 5812 | `	sResult.nIdx = SXU32_HIGH; /* Mark as constant */` |
|       - | 5813 | `	/* Perform the requested operation */` |
|      66 | 5814 | `	for( n = 0 ; n < pMap->nEntry ; n++ ){` |
|       - | 5815 | `		/* Extract node value (may be NULL if allocation failed) */` |
|      54 | 5816 | `		pValue = HashmapExtractNodeValue(pEntry);` |
|      54 | 5817 | `		if( pValue == 0 ){` |
|       - | 5818 | `			/* Can happen if SySetAt() failed earlier; drop the entry. */` |
|     ! 0 | 5819 | `			keep = FALSE;` |
|      54 | 5820 | `		}else if( nArg > 1 && !ph7_value_is_null(apArg[1]) ){` |
|       - | 5821 | `			/* Callback was supplied (not NULL).  PHP 8 throws a` |
|       - | 5822 | `				* TypeError when the value is not callable or null; prior PH7` |
|       - | 5823 | `				* silently dropped the element.  Emit similar message. */` |
|      26 | 5824 | `			if( !ph7_value_is_callable(apArg[1]) ){` |
|       3 | 5825 | `				if( ph7_value_is_string(apArg[1]) ){` |
|       - | 5826 | `					int len;` |
|       3 | 5827 | `					const char *zName = ph7_value_to_string(apArg[1], &len);` |
|       4 | 5828 | `					return PH7_VmThrowException(pCtx,` |
|       - | 5829 | `						"TypeError",` |
|       - | 5830 | `						"array_filter(): Argument #2 ($callback) must be a valid callback or null, function \"%s\" not found or invalid function name",` |
|       1 | 5831 | `						zName` |
|       - | 5832 | `						);` |
|     ! 0 | 5833 | `				}else{` |
|     ! 0 | 5834 | `					return PH7_VmThrowException(pCtx,` |
|       - | 5835 | `						"TypeError",` |
|       - | 5836 | `						"array_filter(): Argument #2 ($callback) must be a valid callback or null, %s given",` |
|     ! 0 | 5837 | `						ph7_type_name(apArg[1])` |
|       - | 5838 | `						);` |
|       - | 5839 | `				}` |
|       - | 5840 | `			}` |
|      23 | 5841 | `			keep = FALSE;` |
|      23 | 5842 | `			rc = PH7_VmCallUserFunction(pMap->pVm,apArg[1],1,&pValue,&sResult);` |
|      23 | 5843 | `			if( rc == SXRET_OK ){` |
|       - | 5844 | `				/* Perform a boolean cast */` |
|      23 | 5845 | `				keep = ph7_value_to_bool(&sResult);` |
|      11 | 5846 | `			}` |
|      23 | 5847 | `			PH7_MemObjRelease(&sResult);` |
|      12 | 5848 | `		}else{` |
|       - | 5849 | `			/* No callback provided or callback explicitly NULL: use default` |
|       - | 5850 | `			 * behaviour where "empty" values are removed. This also covers` |
|       - | 5851 | `			 * the case where the callback argument is missing entirely.` |
|       - | 5852 | `			 */` |
|      29 | 5853 | `			keep = !PH7_MemObjIsEmpty(pValue);` |
|       - | 5854 | `		}` |
|      51 | 5855 | `		if( keep ){` |
|       - | 5856 | `			/* Perform the insertion,now the callback returned true */` |
|      17 | 5857 | `			HashmapInsertNode((ph7_hashmap *)pArray->x.pOther,pEntry,TRUE);` |
|       8 | 5858 | `		}` |
|       - | 5859 | `		/* Point to the next entry */` |
|      51 | 5860 | `		pEntry = pEntry->pPrev; /* Reverse link */` |
|      26 | 5861 | `	}` |
|      13 | 5862 | `	ph7_result_value(pCtx,pArray);` |
|      13 | 5863 | `	return PH7_OK;` |
|      11 | 5864 |  |
|       - | 5865 | `/*` |
|       - | 5866 | ` * array array_map(callback $callback,array $arr1)` |
|       - | 5867 | ` *  Applies the callback to the elements of the given arrays.` |
|       - | 5868 | ` * Parameters` |
|       - | 5869 | ` *  $callback` |
|       - | 5870 | ` *   Callback function to run for each element in each array.` |
|       - | 5871 | ` * $arr1` |
|       - | 5872 | ` *   An array to run through the callback function.` |
|       - | 5873 | ` * Return` |
|       - | 5874 | ` *  Returns an array containing all the elements of arr1 after applying` |
|       - | 5875 | ` *  the callback function to each one.` |
|       - | 5876 | ` * NOTE:` |
|       - | 5877 | ` *  array_map() passes only a single value to the callback.` |
|       - | 5878 | ` */` |
|      10 | 5879 | `static int ph7_hashmap_map(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       1 | 5880 |  |
|       - | 5881 | `	ph7_value *pArray,*pValue,sKey,sResult;` |
|       - | 5882 | `	ph7_hashmap_node *pEntry;` |
|       - | 5883 | `	ph7_hashmap *pMap;` |
|       - | 5884 | `	sxu32 n;` |
|      11 | 5885 | `	if( nArg < 2 \|\| !ph7_value_is_array(apArg[1]) ){` |
|       - | 5886 | `		/* Invalid arguments,return NULL */` |
|       5 | 5887 | `		ph7_result_null(pCtx);` |
|       5 | 5888 | `		return PH7_OK;` |
|       - | 5889 | `	}` |
|       - | 5890 | `	/* Create a new array */` |
|       7 | 5891 | `	pArray = ph7_context_new_array(pCtx);` |
|       7 | 5892 | `	if( pArray == 0 ){` |
|     ! 0 | 5893 | `		ph7_result_null(pCtx);` |
|     ! 0 | 5894 | `		return PH7_OK;` |
|       - | 5895 | `	}` |
|       - | 5896 | `	/* Point to the internal representation of the input hashmap */` |
|       7 | 5897 | `	pMap = (ph7_hashmap *)apArg[1]->x.pOther;` |
|       7 | 5898 | `	PH7_MemObjInit(pMap->pVm,&sResult);` |
|       7 | 5899 | `	PH7_MemObjInit(pMap->pVm,&sKey);` |
|       7 | 5900 | `	sResult.nIdx = SXU32_HIGH; /* Mark as constant */` |
|       7 | 5901 | `	sKey.nIdx    = SXU32_HIGH; /* Mark as constant */` |
|       - | 5902 | `	/* Perform the requested operation */` |
|       7 | 5903 | `	pEntry = pMap->pFirst;` |
|      21 | 5904 | `	for( n = 0 ; n < pMap->nEntry ; n++ ){` |
|       - | 5905 | `		/* Extrcat the node value */` |
|      15 | 5906 | `		pValue = HashmapExtractNodeValue(pEntry);` |
|      15 | 5907 | `		if( pValue ){` |
|       - | 5908 | `			sxi32 rc;` |
|       - | 5909 | `			/* Invoke the supplied callback */` |
|      15 | 5910 | `			rc = PH7_VmCallUserFunction(pMap->pVm,apArg[0],1,&pValue,&sResult);` |
|       - | 5911 | `			/* Extract the node key */` |
|      15 | 5912 | `			PH7_HashmapExtractNodeKey(pEntry,&sKey);` |
|      15 | 5913 | `			if( rc != SXRET_OK ){` |
|       - | 5914 | `				/* An error occured while invoking the supplied callback [i.e: not defined] */` |
|     ! 0 | 5915 | `				ph7_array_add_elem(pArray,&sKey,pValue); /* Keep the same value */` |
|     ! 0 | 5916 | `			}else{` |
|       - | 5917 | `				/* Insert the callback return value */` |
|      15 | 5918 | `				ph7_array_add_elem(pArray,&sKey,&sResult);` |
|       - | 5919 | `			}` |
|      15 | 5920 | `			PH7_MemObjRelease(&sKey);` |
|      15 | 5921 | `			PH7_MemObjRelease(&sResult);` |
|       7 | 5922 | `		}` |
|       - | 5923 | `		/* Point to the next entry */` |
|      15 | 5924 | `		pEntry = pEntry->pPrev; /* Reverse link */` |
|       8 | 5925 | `	}` |
|       7 | 5926 | `	ph7_result_value(pCtx,pArray);` |
|       7 | 5927 | `	return PH7_OK;` |
|       6 | 5928 |  |
|       - | 5929 | `/*` |
|       - | 5930 | ` * value array_reduce(array $input,callback $function[, value $initial = NULL ])` |
|       - | 5931 | ` *  Iteratively reduce the array to a single value using a callback function.` |
|       - | 5932 | ` * Parameters` |
|       - | 5933 | ` *  $input` |
|       - | 5934 | ` *   The input array.` |
|       - | 5935 | ` *  $function` |
|       - | 5936 | ` *  The callback function.` |
|       - | 5937 | ` * $initial` |
|       - | 5938 | ` *  If the optional initial is available, it will be used at the beginning` |
|       - | 5939 | ` *  of the process, or as a final result in case the array is empty.` |
|       - | 5940 | ` * Return` |
|       - | 5941 | ` *  Returns the resulting value.` |
|       - | 5942 | ` *  If the array is empty and initial is not passed, array_reduce() returns NULL.` |
|       - | 5943 | ` */` |
|       4 | 5944 | `static int ph7_hashmap_reduce(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       1 | 5945 |  |
|       - | 5946 | `	ph7_hashmap_node *pEntry;` |
|       - | 5947 | `	ph7_hashmap *pMap;` |
|       - | 5948 | `	ph7_value *pValue;` |
|       - | 5949 | `	ph7_value sResult;` |
|       - | 5950 | `	sxu32 n;` |
|       5 | 5951 | `	if( nArg < 2 \|\| !ph7_value_is_array(apArg[0]) ){` |
|       - | 5952 | `		/* Invalid/Missing arguments,return NULL */` |
|     ! 0 | 5953 | `		ph7_result_null(pCtx);` |
|     ! 0 | 5954 | `		return PH7_OK;` |
|       - | 5955 | `	}` |
|       - | 5956 | `	/* Point to the internal representation of the input hashmap */` |
|       5 | 5957 | `	pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|       - | 5958 | `	/* Assume a NULL initial value */` |
|       5 | 5959 | `	PH7_MemObjInit(pMap->pVm,&sResult);` |
|       5 | 5960 | `	sResult.nIdx = SXU32_HIGH; /* Mark as constant */` |
|       5 | 5961 | `	if( nArg > 2 ){` |
|       - | 5962 | `		/* Set the initial value */` |
|       5 | 5963 | `		PH7_MemObjLoad(apArg[2],&sResult);` |
|       2 | 5964 | `	}` |
|       - | 5965 | `	/* Perform the requested operation */` |
|       5 | 5966 | `	pEntry = pMap->pFirst;` |
|      19 | 5967 | `	for( n = 0 ; n < pMap->nEntry ; n++ ){` |
|       - | 5968 | `		/* Extract the node value */` |
|      15 | 5969 | `		pValue = HashmapExtractNodeValue(pEntry);` |
|       - | 5970 | `		/* Invoke the supplied callback */` |
|      15 | 5971 | `		PH7_VmCallUserFunctionAp(pMap->pVm,apArg[1],&sResult,&sResult,pValue,0);` |
|       - | 5972 | `		/* Point to the next entry */` |
|      15 | 5973 | `		pEntry = pEntry->pPrev; /* Reverse link */` |
|       8 | 5974 | `	}` |
|       5 | 5975 | `	ph7_result_value(pCtx,&sResult); /* Will make it's own copy */` |
|       5 | 5976 | `	PH7_MemObjRelease(&sResult);` |
|       5 | 5977 | `	return PH7_OK;` |
|       3 | 5978 |  |
|       - | 5979 | `/*` |
|       - | 5980 | ` * bool array_walk(array &$array,callback $funcname [, value $userdata ] )` |
|       - | 5981 | ` *  Apply a user function to every member of an array.` |
|       - | 5982 | ` * Parameters` |
|       - | 5983 | ` *  $array` |
|       - | 5984 | ` *   The input array.` |
|       - | 5985 | ` * $funcname` |
|       - | 5986 | ` *  Typically, funcname takes on two parameters.The array parameter's value being` |
|       - | 5987 | ` *  the first, and the key/index second.` |
|       - | 5988 | ` * Note:` |
|       - | 5989 | ` *  If funcname needs to be working with the actual values of the array,specify the first` |
|       - | 5990 | ` *  parameter of funcname as a reference. Then, any changes made to those elements will` |
|       - | 5991 | ` *  be made in the original array itself.` |
|       - | 5992 | ` * $userdata` |
|       - | 5993 | ` *  If the optional userdata parameter is supplied, it will be passed as the third parameter` |
|       - | 5994 | ` *  to the callback funcname.` |
|       - | 5995 | ` * Return` |
|       - | 5996 | ` *  Returns TRUE on success or FALSE on failure.` |
|       - | 5997 | ` */` |
|      12 | 5998 | `static int ph7_hashmap_walk(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       1 | 5999 |  |
|       - | 6000 | `	ph7_value *pValue,*pUserData,sKey;` |
|       - | 6001 | `	ph7_hashmap_node *pEntry;` |
|       - | 6002 | `	ph7_hashmap *pMap;` |
|       - | 6003 | `	sxi32 rc;` |
|       - | 6004 | `	sxu32 n;` |
|      13 | 6005 | `	if( nArg < 2 \|\| !ph7_value_is_array(apArg[0]) ){` |
|       - | 6006 | `		/* Invalid/Missing arguments,return FALSE */` |
|     ! 0 | 6007 | `		ph7_result_bool(pCtx,0);` |
|     ! 0 | 6008 | `		return PH7_OK;` |
|       - | 6009 | `	}` |
|      13 | 6010 | `	pUserData = nArg > 2 ? apArg[2] : 0;` |
|       - | 6011 | `	/* Point to the internal representation of the input hashmap */` |
|      13 | 6012 | `	pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|      13 | 6013 | `	PH7_MemObjInit(pMap->pVm,&sKey);` |
|      13 | 6014 | `	sKey.nIdx = SXU32_HIGH; /* Mark as constant */` |
|       - | 6015 | `	/* Perform the desired operation */` |
|      13 | 6016 | `	pEntry = pMap->pFirst;` |
|      41 | 6017 | `	for( n = 0 ; n < pMap->nEntry ; n++ ){` |
|       - | 6018 | `		/* Extract the node value */` |
|      29 | 6019 | `		pValue = HashmapExtractNodeValue(pEntry);` |
|      29 | 6020 | `		if( pValue ){` |
|       - | 6021 | `			/* Extract the entry key */` |
|      29 | 6022 | `			PH7_HashmapExtractNodeKey(pEntry,&sKey);` |
|       - | 6023 | `			/* Invoke the supplied callback */` |
|      29 | 6024 | `			rc = PH7_VmCallUserFunctionAp(pMap->pVm,apArg[1],0,pValue,&sKey,pUserData,0);` |
|      29 | 6025 | `			PH7_MemObjRelease(&sKey);` |
|      29 | 6026 | `			if( rc != SXRET_OK ){` |
|       - | 6027 | `				/* An error occured while invoking the supplied callback [i.e: not defined] */` |
|     ! 0 | 6028 | `				ph7_result_bool(pCtx,0); /* return FALSE */` |
|     ! 0 | 6029 | `				return PH7_OK;` |
|       - | 6030 | `			}` |
|      14 | 6031 | `		}` |
|       - | 6032 | `		/* Point to the next entry */` |
|      29 | 6033 | `		pEntry = pEntry->pPrev; /* Reverse link */` |
|      15 | 6034 | `	}` |
|       - | 6035 | `	/* All done,return TRUE */` |
|      13 | 6036 | `	ph7_result_bool(pCtx,1);` |
|      13 | 6037 | `	return PH7_OK;` |
|       7 | 6038 |  |
|       - | 6039 | `/*` |
|       - | 6040 | ` * Apply a user function to every member of an array.(Recurse on array's).` |
|       - | 6041 | ` * Refer to the [array_walk_recursive()] implementation for more information.` |
|       - | 6042 | ` */` |
|       6 | 6043 | `static int HashmapWalkRecursive(` |
|       - | 6044 | `	ph7_hashmap *pMap,    /* Target hashmap */` |
|       - | 6045 | `	ph7_value *pCallback, /* User callback */` |
|       - | 6046 | `	ph7_value *pUserData, /* Callback private data */` |
|       - | 6047 | `	int iNest             /* Nesting level */` |
|       - | 6048 | `	)` |
|       1 | 6049 |  |
|       - | 6050 | `	ph7_hashmap_node *pEntry;` |
|       - | 6051 | `	ph7_value *pValue,sKey;` |
|       - | 6052 | `	sxi32 rc;` |
|       - | 6053 | `	sxu32 n;` |
|       - | 6054 | `	/* Iterate throw hashmap entries */` |
|       7 | 6055 | `	PH7_MemObjInit(pMap->pVm,&sKey);` |
|       7 | 6056 | `	sKey.nIdx = SXU32_HIGH; /* Mark as constant */` |
|       7 | 6057 | `	pEntry = pMap->pFirst;` |
|      17 | 6058 | `	for( n = 0 ; n < pMap->nEntry ; n++ ){` |
|       - | 6059 | `		/* Extract the node value */` |
|      11 | 6060 | `		pValue = HashmapExtractNodeValue(pEntry);` |
|      11 | 6061 | `		if( pValue ){` |
|      11 | 6062 | `			if( pValue->iFlags & MEMOBJ_HASHMAP ){` |
|       5 | 6063 | `				if( iNest < 32 ){` |
|       - | 6064 | `					/* Recurse */` |
|       5 | 6065 | `					iNest++;` |
|       5 | 6066 | `					HashmapWalkRecursive((ph7_hashmap *)pValue->x.pOther,pCallback,pUserData,iNest);` |
|       5 | 6067 | `					iNest--;` |
|       2 | 6068 | `				}` |
|       3 | 6069 | `			}else{` |
|       - | 6070 | `				/* Extract the node key */` |
|       7 | 6071 | `				PH7_HashmapExtractNodeKey(pEntry,&sKey);` |
|       - | 6072 | `				/* Invoke the supplied callback */` |
|       7 | 6073 | `				rc = PH7_VmCallUserFunctionAp(pMap->pVm,pCallback,0,pValue,&sKey,pUserData,0);` |
|       7 | 6074 | `				PH7_MemObjRelease(&sKey);` |
|       7 | 6075 | `				if( rc != SXRET_OK ){` |
|     ! 0 | 6076 | `					return rc;` |
|       - | 6077 | `				}` |
|       - | 6078 | `			}` |
|       5 | 6079 | `		}` |
|       - | 6080 | `		/* Point to the next entry */` |
|      11 | 6081 | `		pEntry = pEntry->pPrev; /* Reverse link */` |
|       6 | 6082 | `	}` |
|       7 | 6083 | `	return SXRET_OK;` |
|       4 | 6084 |  |
|       - | 6085 | `/*` |
|       - | 6086 | ` * bool array_walk_recursive(array &$array,callback $funcname [, value $userdata ] )` |
|       - | 6087 | ` *  Apply a user function recursively to every member of an array.` |
|       - | 6088 | ` * Parameters` |
|       - | 6089 | ` *  $array` |
|       - | 6090 | ` *   The input array.` |
|       - | 6091 | ` * $funcname` |
|       - | 6092 | ` *  Typically, funcname takes on two parameters.The array parameter's value being` |
|       - | 6093 | ` *  the first, and the key/index second.` |
|       - | 6094 | ` * Note:` |
|       - | 6095 | ` *  If funcname needs to be working with the actual values of the array,specify the first` |
|       - | 6096 | ` *  parameter of funcname as a reference. Then, any changes made to those elements will` |
|       - | 6097 | ` *  be made in the original array itself.` |
|       - | 6098 | ` * $userdata` |
|       - | 6099 | ` *  If the optional userdata parameter is supplied, it will be passed as the third parameter` |
|       - | 6100 | ` *  to the callback funcname.` |
|       - | 6101 | ` * Return` |
|       - | 6102 | ` *  Returns TRUE on success or FALSE on failure.` |
|       - | 6103 | ` */` |
|       2 | 6104 | `static int ph7_hashmap_walk_recursive(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       1 | 6105 |  |
|       - | 6106 | `	ph7_hashmap *pMap;` |
|       - | 6107 | `	sxi32 rc;` |
|       3 | 6108 | `	if( nArg < 2 \|\| !ph7_value_is_array(apArg[0]) ){` |
|       - | 6109 | `		/* Invalid/Missing arguments,return FALSE */` |
|     ! 0 | 6110 | `		ph7_result_bool(pCtx,0);` |
|     ! 0 | 6111 | `		return PH7_OK;` |
|       - | 6112 | `	}` |
|       - | 6113 | `	/* Point to the internal representation of the input hashmap */` |
|       3 | 6114 | `	pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|       - | 6115 | `	/* Perform the desired operation */` |
|       3 | 6116 | `	rc = HashmapWalkRecursive(pMap,apArg[1],nArg > 2 ? apArg[2] : 0,0);` |
|       - | 6117 | `	/* All done */` |
|       3 | 6118 | `	ph7_result_bool(pCtx,rc == SXRET_OK);` |
|       3 | 6119 | `	return PH7_OK;` |
|       2 | 6120 |  |
|       - | 6121 | `/*` |
|       - | 6122 | ` * Table of hashmap functions.` |
|       - | 6123 | ` */` |
|       - | 6124 | `static const ph7_builtin_func aHashmapFunc[] = {` |
|       - | 6125 | `	{"count",             ph7_hashmap_count },` |
|       - | 6126 | `	{"sizeof",            ph7_hashmap_count },` |
|       - | 6127 | `	{"array_key_exists",  ph7_hashmap_key_exists },` |
|       - | 6128 | `	{"array_pop",         ph7_hashmap_pop     },` |
|       - | 6129 | `	{"array_push",        ph7_hashmap_push    },` |
|       - | 6130 | `	{"array_shift",       ph7_hashmap_shift   },` |
|       - | 6131 | `	{"array_product",     ph7_hashmap_product },` |
|       - | 6132 | `	{"array_sum",         ph7_hashmap_sum     },` |
|       - | 6133 | `	{"array_keys",        ph7_hashmap_keys    },` |
|       - | 6134 | `	{"array_values",      ph7_hashmap_values  },` |
|       - | 6135 | `	{"array_same",        ph7_hashmap_same    },  /* Symisc eXtension */` |
|       - | 6136 | `	{"array_merge",       ph7_hashmap_merge   },` |
|       - | 6137 | `	{"array_slice",       ph7_hashmap_slice   },` |
|       - | 6138 | `	{"array_splice",      ph7_hashmap_splice  },` |
|       - | 6139 | `	{"array_search",      ph7_hashmap_search  },` |
|       - | 6140 | `	{"array_diff",        ph7_hashmap_diff    },` |
|       - | 6141 | `	{"array_udiff",       ph7_hashmap_udiff   },` |
|       - | 6142 | `	{"array_diff_assoc",  ph7_hashmap_diff_assoc },` |
|       - | 6143 | `	{"array_diff_uassoc", ph7_hashmap_diff_uassoc },` |
|       - | 6144 | `	{"array_diff_key",    ph7_hashmap_diff_key },` |
|       - | 6145 | `	{"array_intersect",   ph7_hashmap_intersect},` |
|       - | 6146 | `	{"array_intersect_assoc", ph7_hashmap_intersect_assoc},` |
|       - | 6147 | `	{"array_uintersect",  ph7_hashmap_uintersect},` |
|       - | 6148 | `	{"array_intersect_key",   ph7_hashmap_intersect_key},` |
|       - | 6149 | `	{"array_copy",        ph7_hashmap_copy    },` |
|       - | 6150 | `	{"array_erase",       ph7_hashmap_erase   },` |
|       - | 6151 | `	{"array_fill",        ph7_hashmap_fill    },` |
|       - | 6152 | `	{"array_fill_keys",   ph7_hashmap_fill_keys},` |
|       - | 6153 | `	{"array_combine",     ph7_hashmap_combine },` |
|       - | 6154 | `	{"array_reverse",     ph7_hashmap_reverse },` |
|       - | 6155 | `	{"array_unique",      ph7_hashmap_unique  },` |
|       - | 6156 | `	{"array_flip",        ph7_hashmap_flip    },` |
|       - | 6157 | `	{"array_rand",        ph7_hashmap_rand    },` |
|       - | 6158 | `	{"array_chunk",       ph7_hashmap_chunk   },` |
|       - | 6159 | `	{"array_pad",         ph7_hashmap_pad     },` |
|       - | 6160 | `	{"array_replace",     ph7_hashmap_replace },` |
|       - | 6161 | `	{"array_filter",      ph7_hashmap_filter  },` |
|       - | 6162 | `	{"array_map",         ph7_hashmap_map     },` |
|       - | 6163 | `	{"array_reduce",      ph7_hashmap_reduce  },` |
|       - | 6164 | `	{"array_walk",        ph7_hashmap_walk    },` |
|       - | 6165 | `	{"array_walk_recursive", ph7_hashmap_walk_recursive },` |
|       - | 6166 | `	{"in_array",          ph7_hashmap_in_array},` |
|       - | 6167 | `	{"sort",              ph7_hashmap_sort    },` |
|       - | 6168 | `	{"asort",             ph7_hashmap_asort   },` |
|       - | 6169 | `	{"arsort",            ph7_hashmap_arsort  },` |
|       - | 6170 | `	{"ksort",             ph7_hashmap_ksort   },` |
|       - | 6171 | `	{"krsort",            ph7_hashmap_krsort  },` |
|       - | 6172 | `	{"rsort",             ph7_hashmap_rsort   },` |
|       - | 6173 | `	{"usort",             ph7_hashmap_usort   },` |
|       - | 6174 | `	{"uasort",            ph7_hashmap_uasort  },` |
|       - | 6175 | `	{"uksort",            ph7_hashmap_uksort  },` |
|       - | 6176 | `	{"shuffle",           ph7_hashmap_shuffle },` |
|       - | 6177 | `	{"range",             ph7_hashmap_range   },` |
|       - | 6178 | `	{"current",           ph7_hashmap_current },` |
|       - | 6179 | `	{"each",              ph7_hashmap_each    },` |
|       - | 6180 | `	{"pos",               ph7_hashmap_current },` |
|       - | 6181 | `	{"next",              ph7_hashmap_next    },` |
|       - | 6182 | `	{"prev",              ph7_hashmap_prev    },` |
|       - | 6183 | `	{"end",               ph7_hashmap_end     },` |
|       - | 6184 | `	{"reset",             ph7_hashmap_reset   },` |
|       - | 6185 | `	{"key",               ph7_hashmap_simple_key }` |
|       - | 6186 | `};` |
|       - | 6187 | `/*` |
|       - | 6188 | ` * Register the built-in hashmap functions defined above.` |
|       - | 6189 | ` */` |
|    1280 | 6190 | `PH7_PRIVATE void PH7_RegisterHashmapFunctions(ph7_vm *pVm)` |
|       2 | 6191 |  |
|       - | 6192 | `	sxu32 n;` |
|   79362 | 6193 | `	for( n = 0 ; n < SX_ARRAYSIZE(aHashmapFunc) ; n++ ){` |
|   78082 | 6194 | `		ph7_create_function(&(*pVm),aHashmapFunc[n].zName,aHashmapFunc[n].xFunc,0);` |
|   39042 | 6195 | `	}` |
|    1282 | 6196 |  |
|       - | 6197 | `/*` |
|       - | 6198 | ` * Dump a hashmap instance and it's entries and the store the dump in` |
|       - | 6199 | ` * the BLOB given as the first argument.` |
|       - | 6200 | ` * This function is typically invoked when the user issue a call to` |
|       - | 6201 | ` * [var_dump(),var_export(),print_r(),...]` |
|       - | 6202 | ` * This function SXRET_OK on success. Any other return value including` |
|       - | 6203 | ` * SXERR_LIMIT(infinite recursion) indicates failure.` |
|       - | 6204 | ` */` |
|      28 | 6205 | `PH7_PRIVATE sxi32 PH7_HashmapDump(SyBlob *pOut,ph7_hashmap *pMap,int ShowType,int nTab,int nDepth)` |
|       2 | 6206 |  |
|       - | 6207 | `	ph7_hashmap_node *pEntry;` |
|       - | 6208 | `	ph7_value *pObj;` |
|      30 | 6209 | `	sxu32 n = 0;` |
|       - | 6210 | `	int isRef;` |
|       - | 6211 | `	sxi32 rc;` |
|       - | 6212 | `	int i;` |
|      30 | 6213 | `	if( nDepth > 31 ){` |
|       - | 6214 | `		static const char zInfinite[] = "Nesting limit reached: Infinite recursion?";` |
|       - | 6215 | `		/* Nesting limit reached */` |
|     ! 0 | 6216 | `		SyBlobAppend(&(*pOut),zInfinite,sizeof(zInfinite)-1);` |
|     ! 0 | 6217 | `		if( ShowType ){` |
|     ! 0 | 6218 | `			SyBlobAppend(&(*pOut),")",sizeof(char));` |
|     ! 0 | 6219 | `		}` |
|     ! 0 | 6220 | `		return SXERR_LIMIT;` |
|       - | 6221 | `	}` |
|       - | 6222 | `	/* Point to the first inserted entry */` |
|      30 | 6223 | `	pEntry = pMap->pFirst;` |
|      30 | 6224 | `	rc = SXRET_OK;` |
|      30 | 6225 | `	if( !ShowType ){` |
|      15 | 6226 | `		SyBlobAppend(&(*pOut),"Array(",sizeof("Array(")-1);` |
|       7 | 6227 | `	}` |
|       - | 6228 | `	/* Total entries */` |
|      30 | 6229 | `	SyBlobFormat(&(*pOut),"%u) {",pMap->nEntry);` |
|       - | 6230 | `#ifdef __WINNT__` |
|       2 | 6231 | `	SyBlobAppend(&(*pOut),"\r\n",sizeof("\r\n")-1);` |
|       - | 6232 | `#else` |
|      28 | 6233 | `	SyBlobAppend(&(*pOut),"\n",sizeof(char));` |
|       - | 6234 | `#endif` |
|      65 | 6235 | `	for(;;){` |
|     132 | 6236 | `		if( n >= pMap->nEntry ){` |
|      30 | 6237 | `			break;` |
|       - | 6238 | `		}` |
|     206 | 6239 | `		for( i = 0 ; i < nTab ; i++ ){` |
|     104 | 6240 | `			SyBlobAppend(&(*pOut)," ",sizeof(char));` |
|      53 | 6241 | `		}` |
|       - | 6242 | `		/* Dump key */` |
|     104 | 6243 | `		if( pEntry->iType == HASHMAP_INT_NODE){` |
|      37 | 6244 | `			SyBlobFormat(&(*pOut),"[%qd] =>",pEntry->xKey.iKey);` |
|      19 | 6245 | `		}else{` |
|     101 | 6246 | `			SyBlobFormat(&(*pOut),"[%.*s] =>",` |
|      33 | 6247 | `				SyBlobLength(&pEntry->xKey.sKey),SyBlobData(&pEntry->xKey.sKey));` |
|       - | 6248 | `		}` |
|       - | 6249 | `#ifdef __WINNT__` |
|       2 | 6250 | `		SyBlobAppend(&(*pOut),"\r\n",sizeof("\r\n")-1);` |
|       - | 6251 | `#else` |
|     102 | 6252 | `		SyBlobAppend(&(*pOut),"\n",sizeof(char));` |
|       - | 6253 | `#endif` |
|       - | 6254 | `		/* Dump node value */` |
|     104 | 6255 | `		pObj = HashmapExtractNodeValue(pEntry);` |
|     104 | 6256 | `		isRef = 0;` |
|     104 | 6257 | `		if( pObj ){` |
|     104 | 6258 | `			if( pEntry->iFlags & HASHMAP_NODE_FOREIGN_OBJ ){` |
|       - | 6259 | `				/* Referenced object */` |
|     ! 0 | 6260 | `				isRef = 1;` |
|     ! 0 | 6261 | `			}` |
|     104 | 6262 | `			rc = PH7_MemObjDump(&(*pOut),pObj,ShowType,nTab+1,nDepth,isRef);` |
|     104 | 6263 | `			if( rc == SXERR_LIMIT ){` |
|     ! 0 | 6264 | `				break;` |
|       - | 6265 | `			}` |
|      51 | 6266 | `		}` |
|       - | 6267 | `		/* Point to the next entry */` |
|     104 | 6268 | `		n++;` |
|     104 | 6269 | `		pEntry = pEntry->pPrev; /* Reverse link */` |
|       2 | 6270 | `	}` |
|      58 | 6271 | `	for( i = 0 ; i < nTab ; i++ ){` |
|      30 | 6272 | `		SyBlobAppend(&(*pOut)," ",sizeof(char));` |
|      16 | 6273 | `	}` |
|      30 | 6274 | `	SyBlobAppend(&(*pOut),"}",sizeof(char));` |
|      30 | 6275 | `	return rc;` |
|      16 | 6276 |  |
|       - | 6277 | `/*` |
|       - | 6278 | ` * Iterate throw hashmap entries and invoke the given callback [i.e: xWalk()] for each` |
|       - | 6279 | ` * retrieved entry.` |
|       - | 6280 | ` * Note that argument are passed to the callback by copy. That is,any modification to` |
|       - | 6281 | ` * the entry value in the callback body will not alter the real value.` |
|       - | 6282 | ` * If the callback wishes to abort processing [i.e: it's invocation] it must return` |
|       - | 6283 | ` * a value different from PH7_OK.` |
|       - | 6284 | ` * Refer to [ph7_array_walk()] for more information.` |
|       - | 6285 | ` */` |
|   18814 | 6286 | `PH7_PRIVATE sxi32 PH7_HashmapWalk(` |
|       - | 6287 | `	ph7_hashmap *pMap, /* Target hashmap */` |
|       - | 6288 | `	int (*xWalk)(ph7_value *,ph7_value *,void *), /* Walker callback */` |
|       - | 6289 | `	void *pUserData /* Last argument to xWalk() */` |
|       - | 6290 | `	)` |
|       2 | 6291 |  |
|       - | 6292 | `	ph7_hashmap_node *pEntry;` |
|       - | 6293 | `	ph7_value sKey,sValue;` |
|       - | 6294 | `	sxi32 rc;` |
|       - | 6295 | `	sxu32 n;` |
|       - | 6296 | `	/* Initialize walker parameter */` |
|   18816 | 6297 | `	rc = SXRET_OK;` |
|   18816 | 6298 | `	PH7_MemObjInit(pMap->pVm,&sKey);` |
|   18816 | 6299 | `	PH7_MemObjInit(pMap->pVm,&sValue);` |
|   18816 | 6300 | `	n = pMap->nEntry;` |
|   18816 | 6301 | `	pEntry = pMap->pFirst;` |
|       - | 6302 | `	/* Start the iteration process */` |
|   50724 | 6303 | `	for(;;){` |
|  101450 | 6304 | `		if( n < 1 ){` |
|   18816 | 6305 | `			break;` |
|       - | 6306 | `		}` |
|       - | 6307 | `		/* Extract a copy of the key and a copy the current value */` |
|   82636 | 6308 | `		PH7_HashmapExtractNodeKey(pEntry,&sKey);` |
|   82636 | 6309 | `		PH7_HashmapExtractNodeValue(pEntry,&sValue,FALSE);` |
|       - | 6310 | `		/* Invoke the user callback */` |
|   82636 | 6311 | `		rc = xWalk(&sKey,&sValue,pUserData);` |
|       - | 6312 | `		/* Release the copy of the key and the value */` |
|   82636 | 6313 | `		PH7_MemObjRelease(&sKey);` |
|   82636 | 6314 | `		PH7_MemObjRelease(&sValue);` |
|   82636 | 6315 | `		if( rc != PH7_OK ){` |
|       - | 6316 | `			/* Callback request an operation abort */` |
|     ! 0 | 6317 | `			return SXERR_ABORT;` |
|       - | 6318 | `		}` |
|       - | 6319 | `		/* Point to the next entry */` |
|   82636 | 6320 | `		pEntry = pEntry->pPrev; /* Reverse link */` |
|   82636 | 6321 | `		n--;` |
|       2 | 6322 | `	}` |
|       - | 6323 | `	/* All done */` |
|   18816 | 6324 | `	return SXRET_OK;` |
|    9409 | 6325 |  |
|       - | 6326 |  |
