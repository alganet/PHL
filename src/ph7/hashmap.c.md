# src/ph7/hashmap.c

<style>code, pre { background: none !important; white-space: pre !important; width: 100% !important; display: inline-block !important; } td { border: none !important; margin-top: 0 !important; margin-bottom: 0 !important; padding-top: 0 !important; padding-bottom: 0 !important; }</style>

Coverage: 2538/3090 lines (82.14%)

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
| 2710410 |   18 | `static sxu32 IntHash(sxi64 iKey)` |
|       2 |   19 |  |
| 2710412 |   20 | `	return (sxu32)(iKey ^ (iKey << 8) ^ (iKey >> 8));` |
|       2 |   21 |  |
|       - |   22 | `/*` |
|       - |   23 | ` * Default hash function for string/BLOB keys.` |
|       - |   24 | ` */` |
|  210328 |   25 | `static sxu32 BinHash(const void *pSrc,sxu32 nLen)` |
|       2 |   26 |  |
|  210330 |   27 | `	register unsigned char *zIn = (unsigned char *)pSrc;` |
|       - |   28 | `	unsigned char *zEnd;` |
|  210330 |   29 | `	sxu32 nH = 5381;` |
|  210330 |   30 | `	zEnd = &zIn[nLen];` |
|  243530 |   31 | `	for(;;){` |
|  487062 |   32 | `		if( zIn >= zEnd ){ break; } nH = nH * 33 + zIn[0] ; zIn++;` |
|  439596 |   33 | `		if( zIn >= zEnd ){ break; } nH = nH * 33 + zIn[0] ; zIn++;` |
|  398602 |   34 | `		if( zIn >= zEnd ){ break; } nH = nH * 33 + zIn[0] ; zIn++;` |
|  321218 |   35 | `		if( zIn >= zEnd ){ break; } nH = nH * 33 + zIn[0] ; zIn++;` |
|       2 |   36 | `	}` |
|  210330 |   37 | `	return nH;` |
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
| 2657082 |   86 | `static ph7_hashmap_node * HashmapNewIntNode(ph7_hashmap *pMap,sxi64 iKey,sxu32 nHash,sxu32 nValIdx)` |
|       2 |   87 |  |
|       - |   88 | `	ph7_hashmap_node *pNode;` |
|       - |   89 | `	/* Allocate a new node */` |
| 2657084 |   90 | `	pNode = (ph7_hashmap_node *)SyMemBackendPoolAlloc(&pMap->pVm->sAllocator,sizeof(ph7_hashmap_node));` |
| 2657084 |   91 | `	if( pNode == 0 ){` |
|     ! 0 |   92 | `		return 0;` |
|       - |   93 | `	}` |
|       - |   94 | `	/* Zero the stucture */` |
| 2657084 |   95 | `	SyZero(pNode,sizeof(ph7_hashmap_node));` |
|       - |   96 | `	/* Fill in the structure */` |
| 2657084 |   97 | `	pNode->pMap  = &(*pMap);` |
| 2657084 |   98 | `	pNode->iType = HASHMAP_INT_NODE;` |
| 2657084 |   99 | `	pNode->nHash = nHash;` |
| 2657084 |  100 | `	pNode->xKey.iKey = iKey;` |
| 2657084 |  101 | `	pNode->nValIdx  = nValIdx;` |
| 2657084 |  102 | `	return pNode;` |
| 1328543 |  103 |  |
|       - |  104 | `/*` |
|       - |  105 | ` * Allocate a new hashmap node with a BLOB key.` |
|       - |  106 | ` * If something goes wrong [i.e: out of memory],this function return NULL.` |
|       - |  107 | ` * Otherwise a fresh [ph7_hashmap_node] instance is returned.` |
|       - |  108 | ` */` |
|   73426 |  109 | `static ph7_hashmap_node * HashmapNewBlobNode(ph7_hashmap *pMap,const void *pKey,sxu32 nKeyLen,sxu32 nHash,sxu32 nValIdx)` |
|       2 |  110 |  |
|       - |  111 | `	ph7_hashmap_node *pNode;` |
|       - |  112 | `	/* Allocate a new node */` |
|   73428 |  113 | `	pNode = (ph7_hashmap_node *)SyMemBackendPoolAlloc(&pMap->pVm->sAllocator,sizeof(ph7_hashmap_node));` |
|   73428 |  114 | `	if( pNode == 0 ){` |
|     ! 0 |  115 | `		return 0;` |
|       - |  116 | `	}` |
|       - |  117 | `	/* Zero the stucture */` |
|   73428 |  118 | `	SyZero(pNode,sizeof(ph7_hashmap_node));` |
|       - |  119 | `	/* Fill in the structure */` |
|   73428 |  120 | `	pNode->pMap  = &(*pMap);` |
|   73428 |  121 | `	pNode->iType = HASHMAP_BLOB_NODE;` |
|   73428 |  122 | `	pNode->nHash = nHash;` |
|   73428 |  123 | `	SyBlobInit(&pNode->xKey.sKey,&pMap->pVm->sAllocator);` |
|   73428 |  124 | `	SyBlobAppend(&pNode->xKey.sKey,pKey,nKeyLen);` |
|   73428 |  125 | `	pNode->nValIdx = nValIdx;` |
|   73428 |  126 | `	return pNode;` |
|   36715 |  127 |  |
|       - |  128 | `/*` |
|       - |  129 | ` * link a hashmap node to the given bucket index (last argument to this function).` |
|       - |  130 | ` */` |
| 2730508 |  131 | `static void HashmapNodeLink(ph7_hashmap *pMap,ph7_hashmap_node *pNode,sxu32 nBucketIdx)` |
|       2 |  132 |  |
|       - |  133 | `	/* Link */` |
| 2730510 |  134 | `	if( pMap->apBucket[nBucketIdx] != 0 ){` |
| 2530106 |  135 | `		pNode->pNextCollide = pMap->apBucket[nBucketIdx];` |
| 2530106 |  136 | `		pMap->apBucket[nBucketIdx]->pPrevCollide = pNode;` |
| 1265052 |  137 | `	}` |
| 2730510 |  138 | `	pMap->apBucket[nBucketIdx] = pNode;` |
|       - |  139 | `	/* Link to the map list */` |
| 2730510 |  140 | `	if( pMap->pFirst == 0 ){` |
|   32902 |  141 | `		pMap->pFirst = pMap->pLast = pNode;` |
|       - |  142 | `		/* Point to the first inserted node */` |
|   32902 |  143 | `		pMap->pCur = pNode;` |
|   16452 |  144 | `	}else{` |
| 2697610 |  145 | `		MACRO_LD_PUSH(pMap->pLast,pNode);` |
|       - |  146 | `	}` |
| 2730510 |  147 | `	++pMap->nEntry;` |
| 2730510 |  148 |  |
|       - |  149 | `/*` |
|       - |  150 | ` * Unlink a node from the hashmap.` |
|       - |  151 | ` * If the node count reaches zero then release the whole hash-bucket.` |
|       - |  152 | ` */` |
|    5110 |  153 | `PH7_PRIVATE void PH7_HashmapUnlinkNode(ph7_hashmap_node *pNode,int bRestore)` |
|       2 |  154 |  |
|    5112 |  155 | `	ph7_hashmap *pMap = pNode->pMap;` |
|    5112 |  156 | `	ph7_vm *pVm = pMap->pVm;` |
|       - |  157 | `	/* Unlink from the corresponding bucket */` |
|    5112 |  158 | `	if( pNode->pPrevCollide == 0 ){` |
|    4686 |  159 | `		pMap->apBucket[pNode->nHash & (pMap->nSize - 1)] = pNode->pNextCollide;` |
|    2344 |  160 | `	}else{` |
|     427 |  161 | `		pNode->pPrevCollide->pNextCollide = pNode->pNextCollide;` |
|       - |  162 | `	}` |
|    5112 |  163 | `	if( pNode->pNextCollide ){` |
|    3905 |  164 | `		pNode->pNextCollide->pPrevCollide = pNode->pPrevCollide;` |
|    1952 |  165 | `	}` |
|    5112 |  166 | `	if( pMap->pFirst == pNode ){` |
|      58 |  167 | `		pMap->pFirst = pNode->pPrev;` |
|      28 |  168 | `	}` |
|    5112 |  169 | `	if( pMap->pCur == pNode ){` |
|       - |  170 | `		/* Advance the node cursor */` |
|      60 |  171 | `		pMap->pCur = pMap->pCur->pPrev; /* Reverse link */` |
|      29 |  172 | `	}` |
|       - |  173 | `	/* Unlink from the map list */` |
|    5112 |  174 | `	MACRO_LD_REMOVE(pMap->pLast,pNode);` |
|    5112 |  175 | `	if( bRestore ){` |
|       - |  176 | `		/* Remove the ph7_value associated with this node from the reference table */` |
|      30 |  177 | `		PH7_VmRefObjRemove(pVm,pNode->nValIdx,0,pNode);` |
|       - |  178 | `		/* Restore to the freelist */` |
|      30 |  179 | `		if( (pNode->iFlags & HASHMAP_NODE_FOREIGN_OBJ) == 0 ){` |
|      30 |  180 | `			PH7_VmUnsetMemObj(pVm,pNode->nValIdx,FALSE);` |
|      14 |  181 | `		}` |
|      14 |  182 | `	}` |
|    5112 |  183 | `	if( pNode->iType == HASHMAP_BLOB_NODE ){` |
|    5063 |  184 | `		SyBlobRelease(&pNode->xKey.sKey);` |
|    2531 |  185 | `	}` |
|    5112 |  186 | `	SyMemBackendPoolFree(&pVm->sAllocator,pNode);` |
|    5112 |  187 | `	pMap->nEntry--;` |
|    5112 |  188 | `	if( pMap->nEntry < 1 && pMap != pVm->pGlobal ){` |
|       - |  189 | `		/* Free the hash-bucket */` |
|      26 |  190 | `		SyMemBackendFree(&pVm->sAllocator,pMap->apBucket);` |
|      26 |  191 | `		pMap->apBucket = 0;` |
|      26 |  192 | `		pMap->nSize = 0;` |
|      26 |  193 | `		pMap->pFirst = pMap->pLast = pMap->pCur = 0;` |
|      12 |  194 | `	}` |
|    5112 |  195 |  |
|       - |  196 | `#define HASHMAP_FILL_FACTOR 3` |
|       - |  197 | `/*` |
|       - |  198 | ` * Grow the hash-table and rehash all entries.` |
|       - |  199 | ` */` |
| 2730508 |  200 | `static sxi32 HashmapGrowBucket(ph7_hashmap *pMap)` |
|       2 |  201 |  |
| 2730510 |  202 | `	if( pMap->nEntry >= pMap->nSize * HASHMAP_FILL_FACTOR ){` |
|   36230 |  203 | `		ph7_hashmap_node **apOld = pMap->apBucket;` |
|       - |  204 | `		ph7_hashmap_node *pEntry,**apNew;` |
|   36230 |  205 | `		sxu32 nNew = pMap->nSize << 1;` |
|       - |  206 | `		sxu32 nBucket;` |
|       - |  207 | `		sxu32 n;` |
|   36230 |  208 | `		if( nNew < 1 ){` |
|   32902 |  209 | `			nNew = 16;` |
|   16450 |  210 | `		}` |
|       - |  211 | `		/* Allocate a new bucket */` |
|   36230 |  212 | `		apNew = (ph7_hashmap_node **)SyMemBackendAlloc(&pMap->pVm->sAllocator,nNew * sizeof(ph7_hashmap_node *));` |
|   36230 |  213 | `		if( apNew == 0 ){` |
|     ! 0 |  214 | `			if( pMap->nSize < 1 ){` |
|     ! 0 |  215 | `				return SXERR_MEM; /* Fatal */` |
|       - |  216 | `			}` |
|       - |  217 | `			/* Not so fatal here,simply a performance hit */` |
|     ! 0 |  218 | `			return SXRET_OK;` |
|       - |  219 | `		}` |
|       - |  220 | `		/* Zero the table */` |
|   36230 |  221 | `		SyZero((void *)apNew,nNew * sizeof(ph7_hashmap_node *));` |
|       - |  222 | `		/* Reflect the change */` |
|   36230 |  223 | `		pMap->apBucket = apNew;` |
|   36230 |  224 | `		pMap->nSize = nNew;` |
|   36230 |  225 | `		if( apOld == 0 ){` |
|       - |  226 | `			/* First allocated table [i.e: no entry],return immediately */` |
|   32902 |  227 | `			return SXRET_OK;` |
|       - |  228 | `		}` |
|       - |  229 | `		/* Rehash old entries */` |
|    3330 |  230 | `		pEntry = pMap->pFirst;` |
|    3330 |  231 | `		n = 0;` |
| 1869920 |  232 | `		for( ;; ){` |
| 3739842 |  233 | `			if( n >= pMap->nEntry ){` |
|    3330 |  234 | `				break;` |
|       - |  235 | `			}` |
|       - |  236 | `			/* Clear the old collision link */` |
| 3736514 |  237 | `			pEntry->pNextCollide = pEntry->pPrevCollide = 0;` |
|       - |  238 | `			/* Link to the new bucket */` |
| 3736514 |  239 | `			nBucket = pEntry->nHash & (nNew - 1);` |
| 3736514 |  240 | `			if( pMap->apBucket[nBucket] != 0 ){` |
| 3343076 |  241 | `				pEntry->pNextCollide = pMap->apBucket[nBucket];` |
| 3343076 |  242 | `				pMap->apBucket[nBucket]->pPrevCollide = pEntry;` |
| 1671537 |  243 | `			}` |
| 3736514 |  244 | `			pMap->apBucket[nBucket] = pEntry;` |
|       - |  245 | `			/* Point to the next entry */` |
| 3736514 |  246 | `			pEntry = pEntry->pPrev; /* Reverse link */` |
| 3736514 |  247 | `			n++;` |
|       2 |  248 | `		}` |
|       - |  249 | `		/* Free the old table */` |
|    3330 |  250 | `		SyMemBackendFree(&pMap->pVm->sAllocator,(void *)apOld);` |
|    1664 |  251 | `	}` |
| 2697610 |  252 | `	return SXRET_OK;` |
| 1365256 |  253 |  |
|       - |  254 | `/*` |
|       - |  255 | ` * Insert a 64-bit integer key and it's associated value (if any) in the given` |
|       - |  256 | ` * hashmap.` |
|       - |  257 | ` */` |
| 2657082 |  258 | `static sxi32 HashmapInsertIntKey(ph7_hashmap *pMap,sxi64 iKey,ph7_value *pValue,sxu32 nRefIdx,int isForeign)` |
|       2 |  259 |  |
|       - |  260 | `	ph7_hashmap_node *pNode;` |
|       - |  261 | `	sxu32 nIdx;` |
|       - |  262 | `	sxu32 nHash;` |
|       - |  263 | `	sxi32 rc;` |
| 2657084 |  264 | `	if( !isForeign ){` |
|       - |  265 | `		ph7_value *pObj;` |
|       - |  266 | `		/* Reserve a ph7_value for the value */` |
| 2657060 |  267 | `		pObj = PH7_ReserveMemObj(pMap->pVm);` |
| 2657060 |  268 | `		if( pObj == 0 ){` |
|     ! 0 |  269 | `			return SXERR_MEM;` |
|       - |  270 | `		}` |
| 2657060 |  271 | `		if( pValue ){` |
|       - |  272 | `			/* Duplicate the value */` |
| 2657060 |  273 | `			PH7_MemObjStore(pValue,pObj);` |
| 1328529 |  274 | `		}` |
| 2657060 |  275 | `		nIdx = pObj->nIdx;` |
| 1328531 |  276 | `	}else{` |
|      25 |  277 | `		nIdx = nRefIdx;` |
|       - |  278 | `	}` |
|       - |  279 | `	/* Hash the key */` |
| 2657084 |  280 | `	nHash = pMap->xIntHash(iKey);` |
|       - |  281 | `	/* Allocate a new int node */` |
| 2657084 |  282 | `	pNode = HashmapNewIntNode(&(*pMap),iKey,nHash,nIdx);` |
| 2657084 |  283 | `	if( pNode == 0 ){` |
|     ! 0 |  284 | `		return SXERR_MEM;` |
|       - |  285 | `	}` |
| 2657084 |  286 | `	if( isForeign ){` |
|       - |  287 | `		/* Mark as a foregin entry */` |
|      25 |  288 | `		pNode->iFlags \|= HASHMAP_NODE_FOREIGN_OBJ;` |
|      12 |  289 | `	}` |
|       - |  290 | `	/* Make sure the bucket is big enough to hold the new entry */` |
| 2657084 |  291 | `	rc = HashmapGrowBucket(&(*pMap));` |
| 2657084 |  292 | `	if( rc != SXRET_OK ){` |
|     ! 0 |  293 | `		SyMemBackendPoolFree(&pMap->pVm->sAllocator,pNode);` |
|     ! 0 |  294 | `		return rc;` |
|       - |  295 | `	}` |
|       - |  296 | `	/* Perform the insertion */` |
| 2657084 |  297 | `	HashmapNodeLink(&(*pMap),pNode,nHash & (pMap->nSize - 1));` |
|       - |  298 | `	/* Install in the reference table */` |
| 2657084 |  299 | `	PH7_VmRefObjInstall(pMap->pVm,nIdx,0,pNode,0);` |
|       - |  300 | `	/* All done */` |
| 2657084 |  301 | `	return SXRET_OK;` |
| 1328543 |  302 |  |
|       - |  303 | `/*` |
|       - |  304 | ` * Insert a BLOB key and it's associated value (if any) in the given` |
|       - |  305 | ` * hashmap.` |
|       - |  306 | ` */` |
|   73426 |  307 | `static sxi32 HashmapInsertBlobKey(ph7_hashmap *pMap,const void *pKey,sxu32 nKeyLen,ph7_value *pValue,sxu32 nRefIdx,int isForeign)` |
|       2 |  308 |  |
|       - |  309 | `	ph7_hashmap_node *pNode;` |
|       - |  310 | `	sxu32 nHash;` |
|       - |  311 | `	sxu32 nIdx;` |
|       - |  312 | `	sxi32 rc;` |
|   73428 |  313 | `	if( !isForeign ){` |
|       - |  314 | `		ph7_value *pObj;` |
|       - |  315 | `		/* Reserve a ph7_value for the value */` |
|   55908 |  316 | `		pObj = PH7_ReserveMemObj(pMap->pVm);` |
|   55908 |  317 | `		if( pObj == 0 ){` |
|     ! 0 |  318 | `			return SXERR_MEM;` |
|       - |  319 | `		}` |
|   55908 |  320 | `		if( pValue ){` |
|       - |  321 | `			/* Duplicate the value */` |
|   55908 |  322 | `			PH7_MemObjStore(pValue,pObj);` |
|   27953 |  323 | `		}` |
|   55908 |  324 | `		nIdx = pObj->nIdx;` |
|   27955 |  325 | `	}else{` |
|   17522 |  326 | `		nIdx = nRefIdx;` |
|       - |  327 | `	}` |
|       - |  328 | `	/* Hash the key */` |
|   73428 |  329 | `	nHash = pMap->xBlobHash(pKey,nKeyLen);` |
|       - |  330 | `	/* Allocate a new blob node */` |
|   73428 |  331 | `	pNode = HashmapNewBlobNode(&(*pMap),pKey,nKeyLen,nHash,nIdx);` |
|   73428 |  332 | `	if( pNode == 0 ){` |
|     ! 0 |  333 | `		return SXERR_MEM;` |
|       - |  334 | `	}` |
|   73428 |  335 | `	if( isForeign ){` |
|       - |  336 | `		/* Mark as a foregin entry */` |
|   17522 |  337 | `		pNode->iFlags \|= HASHMAP_NODE_FOREIGN_OBJ;` |
|    8760 |  338 | `	}` |
|       - |  339 | `	/* Make sure the bucket is big enough to hold the new entry */` |
|   73428 |  340 | `	rc = HashmapGrowBucket(&(*pMap));` |
|   73428 |  341 | `	if( rc != SXRET_OK ){` |
|     ! 0 |  342 | `		SyMemBackendPoolFree(&pMap->pVm->sAllocator,pNode);` |
|     ! 0 |  343 | `		return rc;` |
|       - |  344 | `	}` |
|       - |  345 | `	/* Perform the insertion */` |
|   73428 |  346 | `	HashmapNodeLink(&(*pMap),pNode,nHash & (pMap->nSize - 1));` |
|       - |  347 | `	/* Install in the reference table */` |
|   73428 |  348 | `	PH7_VmRefObjInstall(pMap->pVm,nIdx,0,pNode,0);` |
|       - |  349 | `	/* All done */` |
|   73428 |  350 | `	return SXRET_OK;` |
|   36715 |  351 |  |
|       - |  352 | `/*` |
|       - |  353 | ` * Check if a given 64-bit integer key exists in the given hashmap.` |
|       - |  354 | ` * Write a pointer to the target node on success. Otherwise` |
|       - |  355 | ` * SXERR_NOTFOUND is returned on failure.` |
|       - |  356 | ` */` |
|   46362 |  357 | `static sxi32 HashmapLookupIntKey(` |
|       - |  358 | `	ph7_hashmap *pMap,         /* Target hashmap */` |
|       - |  359 | `	sxi64 iKey,                /* lookup key */` |
|       - |  360 | `	ph7_hashmap_node **ppNode  /* OUT: target node on success */` |
|       - |  361 | `	)` |
|       2 |  362 |  |
|       - |  363 | `	ph7_hashmap_node *pNode;` |
|       - |  364 | `	sxu32 nHash;` |
|   46364 |  365 | `	if( pMap->nEntry < 1 ){` |
|       - |  366 | `		/* Don't bother hashing,there is no entry anyway */` |
|     325 |  367 | `		return SXERR_NOTFOUND;` |
|       - |  368 | `	}` |
|       - |  369 | `	/* Hash the key first */` |
|   46040 |  370 | `	nHash = pMap->xIntHash(iKey);` |
|       - |  371 | `	/* Point to the appropriate bucket */` |
|   46040 |  372 | `	pNode = pMap->apBucket[nHash & (pMap->nSize - 1)];` |
|       - |  373 | `	/* Perform the lookup */` |
|  411376 |  374 | `	for(;;){` |
|  822754 |  375 | `		if( pNode == 0 ){` |
|   45615 |  376 | `			break;` |
|       - |  377 | `		}` |
|  777350 |  378 | `		if( pNode->iType == HASHMAP_INT_NODE` |
|  774120 |  379 | `			&& pNode->nHash == nHash` |
|  385765 |  380 | `			&& pNode->xKey.iKey == iKey ){` |
|       - |  381 | `				/* Node found */` |
|     426 |  382 | `				if( ppNode ){` |
|     418 |  383 | `					*ppNode = pNode;` |
|     208 |  384 | `				}` |
|     426 |  385 | `				return SXRET_OK;` |
|       - |  386 | `		}` |
|       - |  387 | `		/* Follow the collision link */` |
|  776715 |  388 | `		pNode = pNode->pNextCollide;` |
|       1 |  389 | `	}` |
|       - |  390 | `	/* No such entry */` |
|   45615 |  391 | `	return SXERR_NOTFOUND;` |
|   23183 |  392 |  |
|       - |  393 | `/*` |
|       - |  394 | ` * Check if a given BLOB key exists in the given hashmap.` |
|       - |  395 | ` * Write a pointer to the target node on success. Otherwise` |
|       - |  396 | ` * SXERR_NOTFOUND is returned on failure.` |
|       - |  397 | ` */` |
|  144086 |  398 | `static sxi32 HashmapLookupBlobKey(` |
|       - |  399 | `	ph7_hashmap *pMap,          /* Target hashmap */` |
|       - |  400 | `	const void *pKey,           /* Lookup key */` |
|       - |  401 | `	sxu32 nKeyLen,              /* Key length in bytes */` |
|       - |  402 | `	ph7_hashmap_node **ppNode   /* OUT: target node on success */` |
|       - |  403 | `	)` |
|       2 |  404 |  |
|       - |  405 | `	ph7_hashmap_node *pNode;` |
|       - |  406 | `	sxu32 nHash;` |
|  144088 |  407 | `	if( pMap->nEntry < 1 ){` |
|       - |  408 | `		/* Don't bother hashing,there is no entry anyway */` |
|    7186 |  409 | `		return SXERR_NOTFOUND;` |
|       - |  410 | `	}` |
|       - |  411 | `	/* Hash the key first */` |
|  136904 |  412 | `	nHash = pMap->xBlobHash(pKey,nKeyLen);` |
|       - |  413 | `	/* Point to the appropriate bucket */` |
|  136904 |  414 | `	pNode = pMap->apBucket[nHash & (pMap->nSize - 1)];` |
|       - |  415 | `	/* Perform the lookup */` |
|  140008 |  416 | `	for(;;){` |
|  280018 |  417 | `		if( pNode == 0 ){` |
|  103482 |  418 | `			break;` |
|       - |  419 | `		}` |
|  193247 |  420 | `		if( pNode->iType == HASHMAP_BLOB_NODE` |
|  175036 |  421 | `			&& pNode->nHash == nHash` |
|  103479 |  422 | `			&& SyBlobLength(&pNode->xKey.sKey) == nKeyLen` |
|   33424 |  423 | `			&& SyMemcmp(SyBlobData(&pNode->xKey.sKey),pKey,nKeyLen) == 0 ){` |
|       - |  424 | `				/* Node found */` |
|   33424 |  425 | `				if( ppNode ){` |
|   33408 |  426 | `					*ppNode = pNode;` |
|   16703 |  427 | `				}` |
|   33424 |  428 | `				return SXRET_OK;` |
|       - |  429 | `		}` |
|       - |  430 | `		/* Follow the collision link */` |
|  143116 |  431 | `		pNode = pNode->pNextCollide;` |
|       2 |  432 | `	}` |
|       - |  433 | `	/* No such entry */` |
|  103482 |  434 | `	return SXERR_NOTFOUND;` |
|   72045 |  435 |  |
|       - |  436 | `/*` |
|       - |  437 | ` * Check if the given BLOB key looks like a decimal number.` |
|       - |  438 | ` * Retrurn TRUE on success.FALSE otherwise.` |
|       - |  439 | ` */` |
|  144264 |  440 | `static int HashmapIsIntKey(SyBlob *pKey)` |
|       2 |  441 |  |
|  144266 |  442 | `	const char *zIn  = (const char *)SyBlobData(pKey);` |
|  144266 |  443 | `	const char *zEnd = &zIn[SyBlobLength(pKey)];` |
|  144266 |  444 | `	if( (int)(zEnd-zIn) > 1 && zIn[0] == '0' ){` |
|       - |  445 | `		/* Octal not decimal number */` |
|       5 |  446 | `		return FALSE;` |
|       - |  447 | `	}` |
|  144262 |  448 | `	if( (zIn[0] == '-' \|\| zIn[0] == '+') && &zIn[1] < zEnd ){` |
|     ! 0 |  449 | `		zIn++;` |
|     ! 0 |  450 | `	}` |
|   72463 |  451 | `	for(;;){` |
|  144928 |  452 | `		if( zIn >= zEnd ){` |
|     233 |  453 | `			return TRUE;` |
|       - |  454 | `		}` |
|  144696 |  455 | `		if( (unsigned char)zIn[0] >= 0xc0 /* UTF-8 stream */  \|\| !SyisDigit(zIn[0]) ){` |
|   72016 |  456 | `			break;` |
|       - |  457 | `		}` |
|     667 |  458 | `		zIn++;` |
|       1 |  459 | `	}` |
|       - |  460 | `	/* Key does not look like a decimal number */` |
|  144030 |  461 | `	return FALSE;` |
|   72134 |  462 |  |
|       - |  463 | `/*` |
|       - |  464 | ` * Check if a given key exists in the given hashmap.` |
|       - |  465 | ` * Write a pointer to the target node on success.` |
|       - |  466 | ` * Otherwise SXERR_NOTFOUND is returned on failure.` |
|       - |  467 | ` */` |
|   71030 |  468 | `static sxi32 HashmapLookup(` |
|       - |  469 | `	ph7_hashmap *pMap,          /* Target hashmap */` |
|       - |  470 | `	ph7_value *pKey,            /* Lookup key */` |
|       - |  471 | `	ph7_hashmap_node **ppNode   /* OUT: target node on success */` |
|       - |  472 | `	)` |
|       2 |  473 |  |
|   71032 |  474 | `	ph7_hashmap_node *pNode = 0; /* cc -O6 warning */` |
|       - |  475 | `	sxi32 rc;` |
|   71032 |  476 | `	if( pKey->iFlags & (MEMOBJ_STRING\|MEMOBJ_HASHMAP\|MEMOBJ_OBJ\|MEMOBJ_RES) ){` |
|   70674 |  477 | `		if( (pKey->iFlags & MEMOBJ_STRING) == 0 ){` |
|       - |  478 | `			/* Force a string cast */` |
|     ! 0 |  479 | `			PH7_MemObjToString(&(*pKey));` |
|     ! 0 |  480 | `		}` |
|   70674 |  481 | `		if( SyBlobLength(&pKey->sBlob) > 0 && !HashmapIsIntKey(&pKey->sBlob) ){` |
|       - |  482 | `			/* Perform a blob lookup */` |
|   70658 |  483 | `			rc = HashmapLookupBlobKey(&(*pMap),SyBlobData(&pKey->sBlob),SyBlobLength(&pKey->sBlob),&pNode);` |
|   70658 |  484 | `			goto result;` |
|       - |  485 | `		}` |
|       8 |  486 | `	}` |
|       - |  487 | `	/* Perform an int lookup */` |
|     376 |  488 | `	if((pKey->iFlags & MEMOBJ_INT) == 0 ){` |
|       - |  489 | `		/* Force an integer cast */` |
|      19 |  490 | `		PH7_MemObjToInteger(pKey);` |
|       9 |  491 | `	}` |
|       - |  492 | `	/* Perform an int lookup */` |
|     376 |  493 | `	rc = HashmapLookupIntKey(&(*pMap),pKey->x.iVal,&pNode);` |
|   35515 |  494 | `result:` |
|   71032 |  495 | `	if( rc == SXRET_OK ){` |
|       - |  496 | `		/* Node found */` |
|   33732 |  497 | `		if( ppNode ){` |
|   33716 |  498 | `			*ppNode = pNode;` |
|   16857 |  499 | `		}` |
|   33732 |  500 | `		return SXRET_OK;` |
|       - |  501 | `	}` |
|       - |  502 | `	/* No such entry */` |
|   37302 |  503 | `	return SXERR_NOTFOUND;` |
|   35517 |  504 |  |
|       - |  505 | `/*` |
|       - |  506 | ` * Insert a given key and it's associated value (if any) in the given` |
|       - |  507 | ` * hashmap.` |
|       - |  508 | ` * If a node with the given key already exists in the database` |
|       - |  509 | ` * then this function overwrite the old value.` |
|       - |  510 | ` */` |
| 2712894 |  511 | `static sxi32 HashmapInsert(` |
|       - |  512 | `	ph7_hashmap *pMap, /* Target hashmap */` |
|       - |  513 | `	ph7_value *pKey,   /* Lookup key  */` |
|       - |  514 | `	ph7_value *pVal    /* Node value */` |
|       - |  515 | `	)` |
|       2 |  516 |  |
| 2712896 |  517 | `	ph7_hashmap_node *pNode = 0;` |
| 2712896 |  518 | `	sxi32 rc = SXRET_OK;` |
| 2712896 |  519 | `	if( pKey && pKey->iFlags & (MEMOBJ_STRING\|MEMOBJ_HASHMAP\|MEMOBJ_OBJ\|MEMOBJ_RES) ){` |
|   56104 |  520 | `		if( (pKey->iFlags & MEMOBJ_STRING) == 0 ){` |
|       - |  521 | `			/* Force a string cast */` |
|       3 |  522 | `			PH7_MemObjToString(&(*pKey));` |
|       1 |  523 | `		}` |
|   56104 |  524 | `		if( SyBlobLength(&pKey->sBlob) < 1 \|\| HashmapIsIntKey(&pKey->sBlob) ){` |
|     254 |  525 | `			if(SyBlobLength(&pKey->sBlob) < 1){` |
|       - |  526 | `				/* Automatic index assign */` |
|      32 |  527 | `				pKey = 0;` |
|      15 |  528 | `			}` |
|     254 |  529 | `			goto IntKey;` |
|       - |  530 | `		}` |
|   83777 |  531 | `		if( SXRET_OK == HashmapLookupBlobKey(&(*pMap),SyBlobData(&pKey->sBlob),` |
|   27925 |  532 | `			SyBlobLength(&pKey->sBlob),&pNode) ){` |
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
|   55830 |  546 | `		if( pMap == pMap->pVm->pGlobal ){` |
|       - |  547 | `			/* Forbidden */` |
|       3 |  548 | `			PH7_VmThrowError(pMap->pVm,0,PH7_CTX_NOTICE,"$GLOBALS is a read-only array,insertion is forbidden");` |
|       3 |  549 | `			return SXRET_OK;` |
|       - |  550 | `		}` |
|       - |  551 | `		/* Perform a blob-key insertion */` |
|   55828 |  552 | `		rc = HashmapInsertBlobKey(&(*pMap),SyBlobData(&pKey->sBlob),SyBlobLength(&pKey->sBlob),&(*pVal),0,FALSE);` |
|   55828 |  553 | `		return rc;` |
|       - |  554 | `	}` |
| 1328396 |  555 | `IntKey:` |
| 2657046 |  556 | `	if( pKey ){` |
|   23109 |  557 | `		if((pKey->iFlags & MEMOBJ_INT) == 0 ){` |
|       - |  558 | `			/* Force an integer cast */` |
|     251 |  559 | `			PH7_MemObjToInteger(pKey);` |
|     125 |  560 | `		}` |
|   23109 |  561 | `		if( SXRET_OK == HashmapLookupIntKey(&(*pMap),pKey->x.iVal,&pNode) ){` |
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
|   23073 |  575 | `		if( pMap == pMap->pVm->pGlobal ){` |
|       - |  576 | `			/* Forbidden */` |
|       3 |  577 | `			PH7_VmThrowError(pMap->pVm,0,PH7_CTX_NOTICE,"$GLOBALS is a read-only array,insertion is forbidden");` |
|       3 |  578 | `			return SXRET_OK;` |
|       - |  579 | `		}` |
|       - |  580 | `		/* Perform a 64-bit-int-key insertion */` |
|   23071 |  581 | `		rc = HashmapInsertIntKey(&(*pMap),pKey->x.iVal,&(*pVal),0,FALSE);` |
|   23071 |  582 | `		if( rc == SXRET_OK ){` |
|   23071 |  583 | `			if( pKey->x.iVal >= pMap->iNextIdx ){` |
|       - |  584 | `				/* Increment the automatic index */` |
|   22843 |  585 | `				pMap->iNextIdx = pKey->x.iVal + 1;` |
|       - |  586 | `				/* Make sure the automatic index is not reserved */` |
|   22843 |  587 | `				while( SXRET_OK == HashmapLookupIntKey(&(*pMap),pMap->iNextIdx,0) ){` |
|     ! 0 |  588 | `					pMap->iNextIdx++;` |
|     ! 0 |  589 | `				}` |
|   11421 |  590 | `			}` |
|   11535 |  591 | `		}` |
|   11536 |  592 | `	}else{` |
| 2633938 |  593 | `		if( pMap == pMap->pVm->pGlobal ){` |
|       - |  594 | `			/* Forbidden */` |
|       3 |  595 | `			PH7_VmThrowError(pMap->pVm,0,PH7_CTX_NOTICE,"$GLOBALS is a read-only array,insertion is forbidden");` |
|       3 |  596 | `			return SXRET_OK;` |
|       - |  597 | `		}` |
|       - |  598 | `		/* Assign an automatic index */` |
| 2633936 |  599 | `		rc = HashmapInsertIntKey(&(*pMap),pMap->iNextIdx,&(*pVal),0,FALSE);` |
| 2633936 |  600 | `		if( rc == SXRET_OK ){` |
| 2633936 |  601 | `			++pMap->iNextIdx;` |
| 1316967 |  602 | `		}` |
|       - |  603 | `	}` |
|       - |  604 | `	/* Insertion result */` |
| 2657006 |  605 | `	return rc;` |
| 1356449 |  606 |  |
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
|   17550 |  634 | `static sxi32 HashmapInsertByRef(` |
|       - |  635 | `	ph7_hashmap *pMap,   /* Target hashmap */` |
|       - |  636 | `	ph7_value *pKey,     /* Lookup key */` |
|       - |  637 | `	sxu32 nRefIdx        /* Foreign ph7_value index */` |
|       - |  638 | `	)` |
|       2 |  639 |  |
|   17552 |  640 | `	ph7_hashmap_node *pNode = 0;` |
|   17552 |  641 | `	sxi32 rc = SXRET_OK;` |
|   17552 |  642 | `	if( pKey && pKey->iFlags & (MEMOBJ_STRING\|MEMOBJ_HASHMAP\|MEMOBJ_OBJ\|MEMOBJ_RES) ){` |
|   17528 |  643 | `		if( (pKey->iFlags & MEMOBJ_STRING) == 0 ){` |
|       - |  644 | `			/* Force a string cast */` |
|     ! 0 |  645 | `			PH7_MemObjToString(&(*pKey));` |
|     ! 0 |  646 | `		}` |
|   17528 |  647 | `		if( SyBlobLength(&pKey->sBlob) < 1 \|\| HashmapIsIntKey(&pKey->sBlob) ){` |
|     ! 0 |  648 | `			if(SyBlobLength(&pKey->sBlob) < 1){` |
|       - |  649 | `				/* Automatic index assign */` |
|     ! 0 |  650 | `				pKey = 0;` |
|     ! 0 |  651 | `			}` |
|     ! 0 |  652 | `			goto IntKey;` |
|       - |  653 | `		}` |
|   26291 |  654 | `		if( SXRET_OK == HashmapLookupBlobKey(&(*pMap),SyBlobData(&pKey->sBlob),` |
|    8763 |  655 | `			SyBlobLength(&pKey->sBlob),&pNode) ){` |
|       - |  656 | `				/* Overwrite */` |
|       7 |  657 | `				PH7_VmRefObjRemove(pMap->pVm,pNode->nValIdx,0,pNode);` |
|       7 |  658 | `				pNode->nValIdx = nRefIdx;` |
|       - |  659 | `				/* Install in the reference table */` |
|       7 |  660 | `				PH7_VmRefObjInstall(pMap->pVm,nRefIdx,0,pNode,0);` |
|       7 |  661 | `				return SXRET_OK;` |
|       - |  662 | `		}` |
|       - |  663 | `		/* Perform a blob-key insertion */` |
|   17522 |  664 | `		rc = HashmapInsertBlobKey(&(*pMap),SyBlobData(&pKey->sBlob),SyBlobLength(&pKey->sBlob),0,nRefIdx,TRUE);` |
|   17522 |  665 | `		return rc;` |
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
|    8777 |  702 |  |
|       - |  703 | `/*` |
|       - |  704 | ` * Extract node value.` |
|       - |  705 | ` */` |
|  752928 |  706 | `static ph7_value * HashmapExtractNodeValue(ph7_hashmap_node *pNode)` |
|       2 |  707 |  |
|       - |  708 | `	/* Point to the desired object */` |
|       - |  709 | `	ph7_value *pObj;` |
|  752930 |  710 | `	pObj = (ph7_value *)SySetAt(&pNode->pMap->pVm->aMemObj,pNode->nValIdx);` |
|  752930 |  711 | `	return pObj;` |
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
|   33631 |  757 | `static sxi32 HashmapNodeCmp(ph7_hashmap_node *pLeft,ph7_hashmap_node *pRight,int bStrict)` |
|       2 |  758 |  |
|       - |  759 | `	ph7_value sObj1,sObj2;` |
|       - |  760 | `	sxi32 rc;` |
|   33633 |  761 | `	if( pLeft == pRight ){` |
|       - |  762 | `		/*` |
|       - |  763 | `		 * Same node.Refer to the sort() implementation defined` |
|       - |  764 | `		 * below for more information on this sceanario.` |
|       - |  765 | `		 */` |
|     ! 0 |  766 | `		return 0;` |
|       - |  767 | `	}` |
|       - |  768 | `	/* Do the comparison */` |
|   33633 |  769 | `	PH7_MemObjInit(pLeft->pMap->pVm,&sObj1);` |
|   33633 |  770 | `	PH7_MemObjInit(pLeft->pMap->pVm,&sObj2);` |
|   33633 |  771 | `	PH7_HashmapExtractNodeValue(pLeft,&sObj1,FALSE);` |
|   33633 |  772 | `	PH7_HashmapExtractNodeValue(pRight,&sObj2,FALSE);` |
|   33633 |  773 | `	rc = PH7_MemObjCmp(&sObj1,&sObj2,bStrict,0);` |
|   33633 |  774 | `	PH7_MemObjRelease(&sObj1);` |
|   33633 |  775 | `	PH7_MemObjRelease(&sObj2);` |
|   33633 |  776 | `	return rc;` |
|   16842 |  777 |  |
|       - |  778 | `/*` |
|       - |  779 | ` * Rehash a node with a 64-bit integer key.` |
|       - |  780 | ` * Refer to [merge_sort(),array_shift()] implementations for more information.` |
|       - |  781 | ` */` |
|    7290 |  782 | `static void HashmapRehashIntNode(ph7_hashmap_node *pEntry)` |
|       2 |  783 |  |
|    7292 |  784 | `	ph7_hashmap *pMap = pEntry->pMap;` |
|       - |  785 | `	sxu32 nBucket;` |
|       - |  786 | `	/* Remove old collision links */` |
|    7292 |  787 | `	if( pEntry->pPrevCollide ){` |
|    5855 |  788 | `		pEntry->pPrevCollide->pNextCollide = pEntry->pNextCollide;` |
|    2923 |  789 | `	}else{` |
|    1439 |  790 | `		pMap->apBucket[pEntry->nHash & (pMap->nSize - 1)] = pEntry->pNextCollide;` |
|       - |  791 | `	}` |
|    7292 |  792 | `	if( pEntry->pNextCollide ){` |
|     624 |  793 | `		pEntry->pNextCollide->pPrevCollide = pEntry->pPrevCollide;` |
|     304 |  794 | `	}` |
|    7292 |  795 | `	pEntry->pNextCollide = pEntry->pPrevCollide = 0;` |
|       - |  796 | `	/* Compute the new hash */` |
|    7292 |  797 | `	pEntry->nHash = pMap->xIntHash(pMap->iNextIdx);` |
|    7292 |  798 | `	pEntry->xKey.iKey = pMap->iNextIdx;` |
|    7292 |  799 | `	nBucket = pEntry->nHash & (pMap->nSize - 1);` |
|       - |  800 | `	/* Link to the new bucket */` |
|    7292 |  801 | `	pEntry->pNextCollide = pMap->apBucket[nBucket];` |
|    7292 |  802 | `	if( pMap->apBucket[nBucket] ){` |
|    6015 |  803 | `		pMap->apBucket[nBucket]->pPrevCollide = pEntry;` |
|    3005 |  804 | `	}` |
|    7292 |  805 | `	pEntry->pNextCollide = pMap->apBucket[nBucket];` |
|    7292 |  806 | `	pMap->apBucket[nBucket] = pEntry;` |
|       - |  807 | `	/* Increment the automatic index */` |
|    7292 |  808 | `	pMap->iNextIdx++;` |
|    7292 |  809 |  |
|       - |  810 | `/*` |
|       - |  811 | ` * Perform a linear search on a given hashmap.` |
|       - |  812 | ` * Write a pointer to the target node on success.` |
|       - |  813 | ` * Otherwise SXERR_NOTFOUND is returned on failure.` |
|       - |  814 | ` * Refer to [array_intersect(),array_diff(),in_array(),...] implementations` |
|       - |  815 | ` * for more information.` |
|       - |  816 | ` */` |
|   18534 |  817 | `static int HashmapFindValue(` |
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
|   18536 |  830 | `	pEntry = pMap->pFirst;` |
|   18536 |  831 | `	n = pMap->nEntry;` |
|   18536 |  832 | `	PH7_MemObjInit(pMap->pVm,&sVal);` |
|   18536 |  833 | `	PH7_MemObjInit(pMap->pVm,&sNeedle);` |
|   44439 |  834 | `	for(;;){` |
|   88880 |  835 | `		if( n < 1 ){` |
|      25 |  836 | `			break;` |
|       - |  837 | `		}` |
|       - |  838 | `		/* Extract node value */` |
|   88856 |  839 | `		pVal = HashmapExtractNodeValue(pEntry);` |
|   88856 |  840 | `		if( pVal ){` |
|   88856 |  841 | `			if( (pVal->iFlags\|pNeedle->iFlags) & MEMOBJ_NULL ){` |
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
|   88856 |  853 | `				PH7_MemObjLoad(pVal,&sVal);` |
|   88856 |  854 | `				PH7_MemObjLoad(pNeedle,&sNeedle);` |
|   88856 |  855 | `				rc = PH7_MemObjCmp(&sNeedle,&sVal,bStrict,0);` |
|   88856 |  856 | `				PH7_MemObjRelease(&sVal);` |
|   88856 |  857 | `				PH7_MemObjRelease(&sNeedle);` |
|   88856 |  858 | `				if( rc == 0 ){` |
|   18512 |  859 | `					if( ppNode ){` |
|       3 |  860 | `						*ppNode = pEntry;` |
|       1 |  861 | `					}` |
|       - |  862 | `					/* Match found*/` |
|   18512 |  863 | `					return SXRET_OK;` |
|       - |  864 | `				}` |
|       - |  865 | `			}` |
|   35172 |  866 | `		}` |
|       - |  867 | `		/* Point to the next entry */` |
|   70346 |  868 | `		pEntry = pEntry->pPrev; /* Reverse link */` |
|   70346 |  869 | `		n--;` |
|       2 |  870 | `	}` |
|       - |  871 | `	/* No such entry */` |
|      25 |  872 | `	return SXERR_NOTFOUND;` |
|    9269 |  873 |  |
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
|  339096 | 1047 | `static sxi32 HashmapDuplicateNode(` |
|       - | 1048 | `	ph7_hashmap *pDest,` |
|       - | 1049 | `	ph7_hashmap_node *pEntry,` |
|       - | 1050 | `	ph7_value *pVal,` |
|       - | 1051 | `	int iAction /* 0: Merge, 1: Overwrite, 2: Dup */` |
|       - | 1052 | `	)` |
|       2 | 1053 |  |
|  339098 | 1054 | `	ph7_value sSafeVal = *pVal;` |
|       - | 1055 | `	ph7_value sKey;` |
|       - | 1056 | `	sxi32 rc;` |
|       - | 1057 |  |
|  339098 | 1058 | `	if( pEntry->iType == HASHMAP_BLOB_NODE ){` |
|       - | 1059 | `		/* Blob key insertion */` |
|      19 | 1060 | `		PH7_MemObjInitFromString(pDest->pVm,&sKey,0);` |
|      19 | 1061 | `		PH7_MemObjStringAppend(&sKey,(const char *)SyBlobData(&pEntry->xKey.sKey),SyBlobLength(&pEntry->xKey.sKey));` |
|      19 | 1062 | `		rc = PH7_HashmapInsert(pDest,&sKey,&sSafeVal);` |
|      19 | 1063 | `		PH7_MemObjRelease(&sKey);` |
|      10 | 1064 | `	}else{` |
|       - | 1065 | `		/* Int key */` |
|  339080 | 1066 | `		if( iAction == 0 ){ /* Merge */` |
|  339064 | 1067 | `			rc = HashmapInsert(pDest,0/* Automatic index assign */,&sSafeVal);` |
|  169549 | 1068 | `		}else if( iAction == 1 ){ /* Overwrite */` |
|       5 | 1069 | `			PH7_MemObjInitFromInt(pDest->pVm,&sKey,pEntry->xKey.iKey);` |
|       5 | 1070 | `			rc = PH7_HashmapInsert(pDest,&sKey,&sSafeVal);` |
|       5 | 1071 | `			PH7_MemObjRelease(&sKey);` |
|       3 | 1072 | `		}else{ /* Dup */` |
|      14 | 1073 | `			rc = HashmapInsertIntKey(pDest,pEntry->xKey.iKey,&sSafeVal,0,FALSE);` |
|       - | 1074 | `		}` |
|       - | 1075 | `	}` |
|  339098 | 1076 | `	return rc;` |
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
|    1584 | 1090 | `static sxi32 HashmapMerge(ph7_hashmap *pSrc,ph7_hashmap *pDest)` |
|       2 | 1091 |  |
|       - | 1092 | `	ph7_hashmap_node *pEntry;` |
|       - | 1093 | `	ph7_value *pVal;` |
|       - | 1094 | `	sxi32 rc;` |
|       - | 1095 | `	sxu32 n;` |
|    1586 | 1096 | `	if( pSrc == pDest ){` |
|       - | 1097 | `		/* Same map. This can easily happen since hashmaps are passed by reference.` |
|       - | 1098 | `		 * Unlike the zend engine.` |
|       - | 1099 | `		 */` |
|     ! 0 | 1100 | `		return SXRET_OK;` |
|       - | 1101 | `	}` |
|       - | 1102 | `	/* Point to the first inserted entry in the source */` |
|    1586 | 1103 | `	pEntry = pSrc->pFirst;` |
|       - | 1104 | `	/* Perform the merge */` |
|  340654 | 1105 | `	for( n = 0 ; n < pSrc->nEntry ; ++n ){` |
|       - | 1106 | `		/* Extract the node value */` |
|  339070 | 1107 | `		pVal = HashmapExtractNodeValue(pEntry);` |
|  339070 | 1108 | `		if( pVal ){` |
|       - | 1109 | `			/* Make a local copy of the value.` |
|       - | 1110 | `			 * The insertion call below may trigger a memory pool reallocation` |
|       - | 1111 | `			 * which will invalidate the 'pVal' pointer since it points` |
|       - | 1112 | `			 * to the old pool.` |
|       - | 1113 | `			 */` |
|  339070 | 1114 | `			rc = HashmapDuplicateNode(pDest,pEntry,pVal,0);` |
|  169536 | 1115 | `		}else{` |
|     ! 0 | 1116 | `			rc = SXRET_OK;` |
|       - | 1117 | `		}` |
|  339070 | 1118 | `		if( rc != SXRET_OK ){` |
|     ! 0 | 1119 | `			return rc;` |
|       - | 1120 | `		}` |
|       - | 1121 | `		/* Point to the next entry */` |
|  339070 | 1122 | `		pEntry = pEntry->pPrev; /* Reverse link */` |
|  169536 | 1123 | `	}` |
|    1586 | 1124 | `	return SXRET_OK;` |
|     794 | 1125 |  |
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
|   47880 | 1297 | `PH7_PRIVATE ph7_hashmap * PH7_NewHashmap(` |
|       - | 1298 | `	ph7_vm *pVm,              /* VM that trigger the hashmap creation */` |
|       - | 1299 | `	sxu32 (*xIntHash)(sxi64), /* Hash function for int keys.NULL otherwise*/` |
|       - | 1300 | `	sxu32 (*xBlobHash)(const void *,sxu32) /* Hash function for BLOB keys.NULL otherwise */` |
|       - | 1301 | `	)` |
|       2 | 1302 |  |
|       - | 1303 | `	ph7_hashmap *pMap;` |
|       - | 1304 | `	/* Allocate a new instance */` |
|   47882 | 1305 | `	pMap = (ph7_hashmap *)SyMemBackendPoolAlloc(&pVm->sAllocator,sizeof(ph7_hashmap));` |
|   47882 | 1306 | `	if( pMap == 0 ){` |
|     ! 0 | 1307 | `		return 0;` |
|       - | 1308 | `	}` |
|       - | 1309 | `	/* Zero the structure */` |
|   47882 | 1310 | `	SyZero(pMap,sizeof(ph7_hashmap));` |
|       - | 1311 | `	/* Fill in the structure */` |
|   47882 | 1312 | `	pMap->pVm = &(*pVm);` |
|   47882 | 1313 | `	pMap->iRef = 1;` |
|       - | 1314 | `	/* Default hash functions */` |
|   47882 | 1315 | `	pMap->xIntHash  = xIntHash ? xIntHash : IntHash;` |
|   47882 | 1316 | `	pMap->xBlobHash = xBlobHash ? xBlobHash : BinHash;` |
|   47882 | 1317 | `	return pMap;` |
|   23942 | 1318 |  |
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
|    1260 | 1339 | `PH7_PRIVATE sxi32 PH7_HashmapCreateSuper(ph7_vm *pVm)` |
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
|    1262 | 1359 | `	pMap = PH7_NewHashmap(&(*pVm),0,0);` |
|    1262 | 1360 | `	if( pMap == 0 ){` |
|     ! 0 | 1361 | `		return SXERR_MEM;` |
|       - | 1362 | `	}` |
|    1262 | 1363 | `	pVm->pGlobal = pMap;` |
|       - | 1364 | `	/* Reserve a ph7_value for the $GLOBALS array*/` |
|    1262 | 1365 | `	pObj = PH7_ReserveMemObj(&(*pVm));` |
|    1262 | 1366 | `	if( pObj == 0 ){` |
|     ! 0 | 1367 | `		return SXERR_MEM;` |
|       - | 1368 | `	}` |
|    1262 | 1369 | `	PH7_MemObjInitFromArray(&(*pVm),pObj,pMap);` |
|       - | 1370 | `	/* Record object index */` |
|    1262 | 1371 | `	pVm->nGlobalIdx = pObj->nIdx;` |
|       - | 1372 | `	/* Install the special $GLOBALS array */` |
|    1262 | 1373 | `	rc = SyHashInsert(&pVm->hSuper,(const void *)"GLOBALS",sizeof("GLOBALS")-1,SX_INT_TO_PTR(pVm->nGlobalIdx));` |
|    1262 | 1374 | `	if( rc != SXRET_OK ){` |
|     ! 0 | 1375 | `		return rc;` |
|       - | 1376 | `	}` |
|       - | 1377 | `	/* Install superglobals now */` |
|   13862 | 1378 | `	for( n =  0 ; n < SX_ARRAYSIZE(azSuper)  ; n++ ){` |
|       - | 1379 | `		ph7_value *pSuper;` |
|       - | 1380 | `		/* Request an empty array */` |
|   12602 | 1381 | `		pSuper = ph7_new_array(&(*pVm));` |
|   12602 | 1382 | `		if( pSuper == 0 ){` |
|     ! 0 | 1383 | `			return SXERR_MEM;` |
|       - | 1384 | `		}` |
|       - | 1385 | `		/* Install */` |
|   12602 | 1386 | `		rc = ph7_vm_config(&(*pVm),PH7_VM_CONFIG_CREATE_SUPER,azSuper[n]/* Super-global name*/,pSuper/* Super-global value */);` |
|   12602 | 1387 | `		if( rc != SXRET_OK ){` |
|     ! 0 | 1388 | `			return rc;` |
|       - | 1389 | `		}` |
|       - | 1390 | `		/* Release the value now it have been installed */` |
|   12602 | 1391 | `		ph7_release_value(&(*pVm),pSuper);` |
|    6302 | 1392 | `	}` |
|       - | 1393 | `	/* Set some $_SERVER entries */` |
|    1262 | 1394 | `	pFile = (SyString *)SySetPeek(&pVm->aFiles);` |
|       - | 1395 | `	/*` |
|       - | 1396 | `	 * 'SCRIPT_FILENAME'` |
|       - | 1397 | `	 * The absolute pathname of the currently executing script.` |
|       - | 1398 | `	 */` |
|    2518 | 1399 | `	ph7_vm_config(pVm,PH7_VM_CONFIG_SERVER_ATTR,` |
|       - | 1400 | `		"SCRIPT_FILENAME",` |
|     630 | 1401 | `		pFile ? pFile->zString : ":Memory:",` |
|    1256 | 1402 | `		pFile ? pFile->nByte : sizeof(":Memory:") - 1` |
|       - | 1403 | `		);` |
|       - | 1404 | `	/* All done,all super-global are installed now */` |
|    1262 | 1405 | `	return SXRET_OK;` |
|     632 | 1406 |  |
|       - | 1407 | `/*` |
|       - | 1408 | ` * Release a hashmap.` |
|       - | 1409 | ` */` |
|   33962 | 1410 | `PH7_PRIVATE sxi32 PH7_HashmapRelease(ph7_hashmap *pMap,int FreeDS)` |
|       2 | 1411 |  |
|       - | 1412 | `	ph7_hashmap_node *pEntry,*pNext;` |
|   33964 | 1413 | `	ph7_vm *pVm = pMap->pVm;` |
|       - | 1414 | `	sxu32 n;` |
|   33964 | 1415 | `	if( pMap == pVm->pGlobal ){` |
|       - | 1416 | `		/* Cannot delete the $GLOBALS array */` |
|     ! 0 | 1417 | `		PH7_VmThrowError(pMap->pVm,0,PH7_CTX_NOTICE,"$GLOBALS is a read-only array,deletion is forbidden");` |
|     ! 0 | 1418 | `		return SXRET_OK;` |
|       - | 1419 | `	}` |
|       - | 1420 | `	/* Start the release process */` |
|   33964 | 1421 | `	n = 0;` |
|   33964 | 1422 | `	pEntry = pMap->pFirst;` |
| 1370725 | 1423 | `	for(;;){` |
| 2741452 | 1424 | `		if( n >= pMap->nEntry ){` |
|   33964 | 1425 | `			break;` |
|       - | 1426 | `		}` |
| 2707490 | 1427 | `		pNext = pEntry->pPrev; /* Reverse link */` |
|       - | 1428 | `		/* Remove the reference from the foreign table */` |
| 2707490 | 1429 | `		PH7_VmRefObjRemove(pVm,pEntry->nValIdx,0,pEntry);` |
| 2707490 | 1430 | `		if( (pEntry->iFlags & HASHMAP_NODE_FOREIGN_OBJ) == 0 ){` |
|       - | 1431 | `			/* Restore the ph7_value to the free list */` |
| 2707482 | 1432 | `			PH7_VmUnsetMemObj(pVm,pEntry->nValIdx,FALSE);` |
| 1353740 | 1433 | `		}` |
|       - | 1434 | `		/* Release the node */` |
| 2707490 | 1435 | `		if( pEntry->iType == HASHMAP_BLOB_NODE ){` |
|   54012 | 1436 | `			SyBlobRelease(&pEntry->xKey.sKey);` |
|   27005 | 1437 | `		}` |
| 2707490 | 1438 | `		SyMemBackendPoolFree(&pVm->sAllocator,pEntry);` |
|       - | 1439 | `		/* Point to the next entry */` |
| 2707490 | 1440 | `		pEntry = pNext;` |
| 2707490 | 1441 | `		n++;` |
|       2 | 1442 | `	}` |
|   33964 | 1443 | `	if( pMap->nEntry > 0 ){` |
|       - | 1444 | `		/* Release the hash bucket */` |
|   30296 | 1445 | `		SyMemBackendFree(&pVm->sAllocator,pMap->apBucket);` |
|   15147 | 1446 | `	}` |
|   33964 | 1447 | `	if( FreeDS ){` |
|       - | 1448 | `		/* Free the whole instance */` |
|   33962 | 1449 | `		SyMemBackendPoolFree(&pVm->sAllocator,pMap);` |
|   16982 | 1450 | `	}else{` |
|       - | 1451 | `		/* Keep the instance but reset it's fields */` |
|       3 | 1452 | `		pMap->apBucket = 0;` |
|       3 | 1453 | `		pMap->iNextIdx = 0;` |
|       3 | 1454 | `		pMap->nEntry = pMap->nSize = 0;` |
|       3 | 1455 | `		pMap->pFirst = pMap->pLast = pMap->pCur = 0;` |
|       - | 1456 | `	}` |
|   33964 | 1457 | `	return SXRET_OK;` |
|   16983 | 1458 |  |
|       - | 1459 | `/*` |
|       - | 1460 | ` * Decrement the reference count of a given hashmap.` |
|       - | 1461 | ` * If the count reaches zero which mean no more variables` |
|       - | 1462 | ` * are pointing to this hashmap,then release the whole instance.` |
|       - | 1463 | ` */` |
|  401008 | 1464 | `PH7_PRIVATE void  PH7_HashmapUnref(ph7_hashmap *pMap)` |
|       2 | 1465 |  |
|  401010 | 1466 | `	ph7_vm *pVm = pMap->pVm;` |
|       - | 1467 | `	/* TICKET 1432-49: $GLOBALS is not subject to garbage collection */` |
|  401010 | 1468 | `	pMap->iRef--;` |
|  401010 | 1469 | `	if( pMap->iRef < 1 && pMap != pVm->pGlobal){` |
|   33962 | 1470 | `		PH7_HashmapRelease(pMap,TRUE);` |
|   16980 | 1471 | `	}` |
|  401010 | 1472 |  |
|       - | 1473 | `/*` |
|       - | 1474 | ` * Check if a given key exists in the given hashmap.` |
|       - | 1475 | ` * Write a pointer to the target node on success.` |
|       - | 1476 | ` * Otherwise SXERR_NOTFOUND is returned on failure.` |
|       - | 1477 | ` */` |
|   71036 | 1478 | `PH7_PRIVATE sxi32 PH7_HashmapLookup(` |
|       - | 1479 | `	ph7_hashmap *pMap,        /* Target hashmap */` |
|       - | 1480 | `	ph7_value *pKey,          /* Lookup key */` |
|       - | 1481 | `	ph7_hashmap_node **ppNode /* OUT: Target node on success */` |
|       - | 1482 | `	)` |
|       2 | 1483 |  |
|       - | 1484 | `	sxi32 rc;` |
|   71038 | 1485 | `	if( pMap->nEntry < 1 ){` |
|       - | 1486 | `		/* TICKET 1433-25: Don't bother hashing,the hashmap is empty anyway.` |
|       - | 1487 | `		 */` |
|       7 | 1488 | `		return SXERR_NOTFOUND;` |
|       - | 1489 | `	}` |
|   71032 | 1490 | `	rc = HashmapLookup(&(*pMap),&(*pKey),ppNode);` |
|   71032 | 1491 | `	return rc;` |
|   35520 | 1492 |  |
|       - | 1493 | `/*` |
|       - | 1494 | ` * Insert a given key and it's associated value (if any) in the given` |
|       - | 1495 | ` * hashmap.` |
|       - | 1496 | ` * If a node with the given key already exists in the database` |
|       - | 1497 | ` * then this function overwrite the old value.` |
|       - | 1498 | ` */` |
| 2373754 | 1499 | `PH7_PRIVATE sxi32 PH7_HashmapInsert(` |
|       - | 1500 | `	ph7_hashmap *pMap, /* Target hashmap */` |
|       - | 1501 | `	ph7_value *pKey,   /* Lookup key */` |
|       - | 1502 | `	ph7_value *pVal    /* Node value.NULL otherwise */` |
|       - | 1503 | `	)` |
|       2 | 1504 |  |
|       - | 1505 | `	sxi32 rc;` |
| 2373756 | 1506 | `	if( pVal && (pVal->iFlags & MEMOBJ_HASHMAP) && (ph7_hashmap *)pVal->x.pOther == pMap->pVm->pGlobal ){` |
|       - | 1507 | `		/*` |
|       - | 1508 | `		 * TICKET 1433-35: Insertion in the $GLOBALS array is forbidden.` |
|       - | 1509 | `		 */` |
|     ! 0 | 1510 | `		PH7_VmThrowError(pMap->pVm,0,PH7_CTX_ERR,"$GLOBALS is a read-only array,insertion is forbidden");` |
|     ! 0 | 1511 | `		return SXRET_OK;` |
|       - | 1512 | `	}` |
| 2373756 | 1513 | `	rc = HashmapInsert(&(*pMap),&(*pKey),&(*pVal));` |
| 2373756 | 1514 | `	return rc;` |
| 1186879 | 1515 |  |
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
|   17550 | 1543 | `PH7_PRIVATE sxi32 PH7_HashmapInsertByRef(` |
|       - | 1544 | `	ph7_hashmap *pMap, /* Target hashmap */` |
|       - | 1545 | `	ph7_value *pKey,   /* Lookup key */` |
|       - | 1546 | `	sxu32 nRefIdx      /* Foreign ph7_value index */` |
|       - | 1547 | `	)` |
|       2 | 1548 |  |
|       - | 1549 | `	sxi32 rc;` |
|   17552 | 1550 | `	if( nRefIdx == pMap->pVm->nGlobalIdx ){` |
|       - | 1551 | `		/*` |
|       - | 1552 | `		 * TICKET 1433-35: Insertion in the $GLOBALS array is forbidden.` |
|       - | 1553 | `		 */` |
|     ! 0 | 1554 | `		PH7_VmThrowError(pMap->pVm,0,PH7_CTX_ERR,"$GLOBALS is a read-only array,insertion is forbidden");` |
|     ! 0 | 1555 | `		return SXRET_OK;` |
|       - | 1556 | `	}` |
|   17552 | 1557 | `	rc = HashmapInsertByRef(&(*pMap),&(*pKey),nRefIdx);` |
|   17552 | 1558 | `	return rc;` |
|    8777 | 1559 |  |
|       - | 1560 | `/*` |
|       - | 1561 | ` * Reset the node cursor of a given hashmap.` |
|       - | 1562 | ` */` |
|   15160 | 1563 | `PH7_PRIVATE void PH7_HashmapResetLoopCursor(ph7_hashmap *pMap)` |
|       2 | 1564 |  |
|       - | 1565 | `	/* Reset the loop cursor */` |
|   15162 | 1566 | `	pMap->pCur = pMap->pFirst;` |
|   15162 | 1567 |  |
|       - | 1568 | `/*` |
|       - | 1569 | ` * Return a pointer to the node currently pointed by the node cursor.` |
|       - | 1570 | ` * If the cursor reaches the end of the list,then this function` |
|       - | 1571 | ` * return NULL.` |
|       - | 1572 | ` * Note that the node cursor is automatically advanced by this function.` |
|       - | 1573 | ` */` |
|  125864 | 1574 | `PH7_PRIVATE ph7_hashmap_node * PH7_HashmapGetNextEntry(ph7_hashmap *pMap)` |
|       2 | 1575 |  |
|  125866 | 1576 | `	ph7_hashmap_node *pCur = pMap->pCur;` |
|  125866 | 1577 | `	if( pCur == 0 ){` |
|       - | 1578 | `		/* End of the list,return null */` |
|    7584 | 1579 | `		return 0;` |
|       - | 1580 | `	}` |
|       - | 1581 | `	/* Advance the node cursor */` |
|  118284 | 1582 | `	pMap->pCur = pCur->pPrev; /* Reverse link */` |
|  118284 | 1583 | `	return pCur;` |
|   62934 | 1584 |  |
|       - | 1585 | `/*` |
|       - | 1586 | ` * Extract a node value.` |
|       - | 1587 | ` */` |
|  301664 | 1588 | `PH7_PRIVATE void PH7_HashmapExtractNodeValue(ph7_hashmap_node *pNode,ph7_value *pValue,int bStore)` |
|       2 | 1589 |  |
|  301666 | 1590 | `	ph7_value *pEntry = HashmapExtractNodeValue(pNode);` |
|  301666 | 1591 | `	if( pEntry ){` |
|  301666 | 1592 | `		if( bStore ){` |
|  118338 | 1593 | `			PH7_MemObjStore(pEntry,pValue);` |
|   59170 | 1594 | `		}else{` |
|  183330 | 1595 | `			PH7_MemObjLoad(pEntry,pValue);` |
|       - | 1596 | `		}` |
|  150883 | 1597 | `	}else{` |
|     ! 0 | 1598 | `		PH7_MemObjRelease(pValue);` |
|       - | 1599 | `	}` |
|  301666 | 1600 |  |
|       - | 1601 | `/*` |
|       - | 1602 | ` * Extract a node key.` |
|       - | 1603 | ` */` |
|   82616 | 1604 | `PH7_PRIVATE void PH7_HashmapExtractNodeKey(ph7_hashmap_node *pNode,ph7_value *pKey)` |
|       2 | 1605 |  |
|       - | 1606 | `	/* Fill with the current key */` |
|   82618 | 1607 | `	if( pNode->iType == HASHMAP_INT_NODE ){` |
|   82484 | 1608 | `		if( SyBlobLength(&pKey->sBlob) > 0 ){` |
|       3 | 1609 | `			SyBlobRelease(&pKey->sBlob);` |
|       1 | 1610 | `		}` |
|   82484 | 1611 | `		pKey->x.iVal = pNode->xKey.iKey;` |
|   82484 | 1612 | `		MemObjSetType(pKey,MEMOBJ_INT);` |
|   41243 | 1613 | `	}else{` |
|     135 | 1614 | `		SyBlobReset(&pKey->sBlob);` |
|     135 | 1615 | `		SyBlobAppend(&pKey->sBlob,SyBlobData(&pNode->xKey.sKey),SyBlobLength(&pNode->xKey.sKey));` |
|     135 | 1616 | `		MemObjSetType(pKey,MEMOBJ_STRING);` |
|       - | 1617 | `	}` |
|   82618 | 1618 |  |
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
|   21548 | 1666 | `static ph7_hashmap_node * HashmapNodeMerge(ph7_hashmap_node *pA,ph7_hashmap_node *pB,ProcNodeCmp xCmp,void *pCmpData)` |
|       2 | 1667 |  |
|       - | 1668 | `	ph7_hashmap_node result,*pTail;` |
|       - | 1669 | `    /* Prevent compiler warning */` |
|   21550 | 1670 | `	result.pNext = result.pPrev = 0;` |
|   21550 | 1671 | `	pTail = &result;` |
|   55225 | 1672 | `	while( pA && pB ){` |
|   33677 | 1673 | `		if( xCmp(pA,pB,pCmpData) < 0 ){` |
|   22052 | 1674 | `			pTail->pPrev = pA;` |
|   22052 | 1675 | `			pA->pNext = pTail;` |
|   22052 | 1676 | `			pTail = pA;` |
|   22052 | 1677 | `			pA = pA->pPrev;` |
|   11032 | 1678 | `		}else{` |
|   11627 | 1679 | `			pTail->pPrev = pB;` |
|   11627 | 1680 | `			pB->pNext = pTail;` |
|   11627 | 1681 | `			pTail = pB;` |
|   11627 | 1682 | `			pB = pB->pPrev;` |
|       - | 1683 | `		}` |
|       2 | 1684 | `	}` |
|   21550 | 1685 | `	if( pA ){` |
|   16012 | 1686 | `		pTail->pPrev = pA;` |
|   16012 | 1687 | `		pA->pNext = pTail;` |
|   13551 | 1688 | `	}else if( pB ){` |
|    5404 | 1689 | `		pTail->pPrev = pB;` |
|    5404 | 1690 | `		pB->pNext = pTail;` |
|    2697 | 1691 | `	}else{` |
|     138 | 1692 | `		pTail->pPrev = pTail->pNext = 0;` |
|       - | 1693 | `	}` |
|   21550 | 1694 | `	return result.pPrev;` |
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
|     486 | 1708 | `static sxi32 HashmapMergeSort(ph7_hashmap *pMap,ProcNodeCmp xCmp,void *pCmpData)` |
|       2 | 1709 |  |
|       - | 1710 | `	ph7_hashmap_node *a[N_SORT_BUCKET], *p,*pIn;` |
|       - | 1711 | `	sxu32 i;` |
|     488 | 1712 | `	SyZero(a,sizeof(a));` |
|       - | 1713 | `	/* Point to the first inserted entry */` |
|     488 | 1714 | `	pIn = pMap->pFirst;` |
|    7782 | 1715 | `	while( pIn ){` |
|    7296 | 1716 | `		p = pIn;` |
|    7296 | 1717 | `		pIn = p->pPrev;` |
|    7296 | 1718 | `		p->pPrev = 0;` |
|   13778 | 1719 | `		for(i=0; i<N_SORT_BUCKET-1; i++){` |
|   13778 | 1720 | `			if( a[i]==0 ){` |
|    7296 | 1721 | `				a[i] = p;` |
|    7296 | 1722 | `				break;` |
|     ! 0 | 1723 | `			}else{` |
|    6484 | 1724 | `				p = HashmapNodeMerge(a[i],p,xCmp,pCmpData);` |
|    6484 | 1725 | `				a[i] = 0;` |
|       - | 1726 | `			}` |
|    3243 | 1727 | `		}` |
|    7296 | 1728 | `		if( i==N_SORT_BUCKET-1 ){` |
|       - | 1729 | `			/* To get here, there need to be 2^(N_SORT_BUCKET) elements in he input list.` |
|       - | 1730 | `			 * But that is impossible.` |
|       - | 1731 | `			 */` |
|     ! 0 | 1732 | `			a[i] = HashmapNodeMerge(a[i], p,xCmp,pCmpData);` |
|     ! 0 | 1733 | `		}` |
|       2 | 1734 | `	}` |
|     488 | 1735 | `	p = a[0];` |
|   15554 | 1736 | `	for(i=1; i<N_SORT_BUCKET; i++){` |
|   15068 | 1737 | `		p = HashmapNodeMerge(p,a[i],xCmp,pCmpData);` |
|    7535 | 1738 | `	}` |
|     488 | 1739 | `	p->pNext = 0;` |
|       - | 1740 | `	/* Reflect the change */` |
|     488 | 1741 | `	pMap->pFirst = p;` |
|       - | 1742 | `	/* Reset the loop cursor */` |
|     488 | 1743 | `	pMap->pCur = pMap->pFirst;` |
|     488 | 1744 | `	return SXRET_OK;` |
|       2 | 1745 |  |
|       - | 1746 | `/*` |
|       - | 1747 | ` * Node comparison callback.` |
|       - | 1748 | ` * used-by: [sort(),asort(),...]` |
|       - | 1749 | ` */` |
|   33613 | 1750 | `static sxi32 HashmapCmpCallback1(ph7_hashmap_node *pA,ph7_hashmap_node *pB,void *pCmpData)` |
|       2 | 1751 |  |
|       - | 1752 | `	ph7_value sA,sB;` |
|       - | 1753 | `	sxi32 iFlags;` |
|       - | 1754 | `	int rc;` |
|   33615 | 1755 | `	if( pCmpData == 0 ){` |
|       - | 1756 | `		/* Perform a standard comparison */` |
|   33611 | 1757 | `		rc = HashmapNodeCmp(pA,pB,FALSE);` |
|   33611 | 1758 | `		return rc;` |
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
|   16833 | 1784 |  |
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
|      14 | 1990 | `static sxi32 HashmapCmpCallback7(ph7_hashmap_node *pA,ph7_hashmap_node *pB,void *pCmpData)` |
|       1 | 1991 |  |
|       - | 1992 | `	sxu32 n;` |
|       7 | 1993 | `	SXUNUSED(pB); /* cc warning */` |
|       7 | 1994 | `	SXUNUSED(pCmpData);` |
|       - | 1995 | `	/* Grab a random number */` |
|      15 | 1996 | `	n = PH7_VmRandomNum(pA->pMap->pVm);` |
|       - | 1997 | `	/* if the random number is odd then the first node 'pA' is greater then` |
|       - | 1998 | `	 * the second node 'pB'. Otherwise the reverse is assumed.` |
|       - | 1999 | `	 */` |
|      15 | 2000 | `	return n&1 ? 1 : -1;` |
|       1 | 2001 |  |
|       - | 2002 | `/*` |
|       - | 2003 | ` * Rehash all nodes keys after a merge-sort have been applied.` |
|       - | 2004 | ` * Used by [sort(),usort() and rsort()].` |
|       - | 2005 | ` */` |
|     470 | 2006 | `static void HashmapSortRehash(ph7_hashmap *pMap)` |
|       2 | 2007 |  |
|       - | 2008 | `	ph7_hashmap_node *p,*pLast;` |
|       - | 2009 | `	sxu32 i;` |
|       - | 2010 | `	/* Rehash all entries */` |
|     472 | 2011 | `	pLast = p = pMap->pFirst;` |
|     472 | 2012 | `	pMap->iNextIdx = 0; /* Reset the automatic index */` |
|     472 | 2013 | `	i = 0;` |
|    3855 | 2014 | `	for( ;; ){` |
|    7712 | 2015 | `		if( i >= pMap->nEntry ){` |
|     472 | 2016 | `			pMap->pLast = pLast; /* Fix the last link broken by the merge-sort */` |
|     472 | 2017 | `			break;` |
|       - | 2018 | `		}` |
|    7242 | 2019 | `		if( p->iType == HASHMAP_BLOB_NODE ){` |
|       - | 2020 | `			/* Do not maintain index association as requested by the PHP specification */` |
|       5 | 2021 | `			SyBlobRelease(&p->xKey.sKey);` |
|       - | 2022 | `			/* Change key type */` |
|       5 | 2023 | `			p->iType = HASHMAP_INT_NODE;` |
|       2 | 2024 | `		}` |
|    7242 | 2025 | `		HashmapRehashIntNode(p);` |
|       - | 2026 | `		/* Point to the next entry */` |
|    7242 | 2027 | `		i++;` |
|    7242 | 2028 | `		pLast = p;` |
|    7242 | 2029 | `		p = p->pPrev; /* Reverse link */` |
|       2 | 2030 | `	}` |
|     472 | 2031 |  |
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
|     796 | 2053 | `static int ph7_hashmap_sort(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       2 | 2054 |  |
|       - | 2055 | `	ph7_hashmap *pMap;` |
|       - | 2056 | `	/* Make sure we are dealing with a valid hashmap */` |
|     798 | 2057 | `	if( nArg < 1 \|\| !ph7_value_is_array(apArg[0]) ){` |
|       - | 2058 | `		/* Missing/Invalid arguments,return FALSE */` |
|     ! 0 | 2059 | `		ph7_result_bool(pCtx,0);` |
|     ! 0 | 2060 | `		return PH7_OK;` |
|       - | 2061 | `	}` |
|       - | 2062 | `	/* Point to the internal representation of the input hashmap */` |
|     798 | 2063 | `	pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|     798 | 2064 | `	if( pMap->nEntry > 1 ){` |
|     466 | 2065 | `		sxi32 iCmpFlags = 0;` |
|     466 | 2066 | `		if( nArg > 1 ){` |
|       - | 2067 | `			/* Extract comparison flags */` |
|       3 | 2068 | `			iCmpFlags = ph7_value_to_int(apArg[1]);` |
|       3 | 2069 | `			if( iCmpFlags == 3 /* SORT_REGULAR */ ){` |
|     ! 0 | 2070 | `				iCmpFlags = 0; /* Standard comparison */` |
|     ! 0 | 2071 | `			}` |
|       1 | 2072 | `		}` |
|       - | 2073 | `		/* Do the merge sort */` |
|     466 | 2074 | `		HashmapMergeSort(pMap,HashmapCmpCallback1,SX_INT_TO_PTR(iCmpFlags));` |
|       - | 2075 | `		/* Rehash [Do not maintain index association as requested by the PHP specification] */` |
|     466 | 2076 | `		HashmapSortRehash(pMap);` |
|     232 | 2077 | `	}` |
|       - | 2078 | `	/* All done,return TRUE */` |
|     798 | 2079 | `	ph7_result_bool(pCtx,1);` |
|     798 | 2080 | `	return PH7_OK;` |
|     400 | 2081 |  |
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
|      10 | 2476 | `		while(pMap->pLast->pPrev){` |
|       8 | 2477 | `			pMap->pLast = pMap->pLast->pPrev;` |
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
|      32 | 2533 | `static int ph7_hashmap_key_exists(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       1 | 2534 |  |
|       - | 2535 | `	sxi32 rc;` |
|      33 | 2536 | `	if( nArg < 2 ){` |
|       - | 2537 | `		/* Missing arguments,return FALSE */` |
|       7 | 2538 | `		ph7_result_bool(pCtx,0);` |
|       7 | 2539 | `		return PH7_OK;` |
|       - | 2540 | `	}` |
|       - | 2541 | `	/* Make sure we are dealing with a valid hashmap */` |
|      27 | 2542 | `	if( !ph7_value_is_array(apArg[1]) ){` |
|       - | 2543 | `		/* Invalid argument,return FALSE */` |
|       3 | 2544 | `		ph7_result_bool(pCtx,0);` |
|       3 | 2545 | `		return PH7_OK;` |
|       - | 2546 | `	}` |
|       - | 2547 | `	/* Perform the lookup */` |
|      25 | 2548 | `	rc = PH7_HashmapLookup((ph7_hashmap *)apArg[1]->x.pOther,apArg[0],0);` |
|       - | 2549 | `	/* lookup result */` |
|      25 | 2550 | `	ph7_result_bool(pCtx,rc == SXRET_OK ? 1 : 0);` |
|      25 | 2551 | `	return PH7_OK;` |
|      17 | 2552 |  |
|       - | 2553 | `/*` |
|       - | 2554 | ` * value array_pop(array $array)` |
|       - | 2555 | ` *   POP the last inserted element from the array.` |
|       - | 2556 | ` * Parameter` |
|       - | 2557 | ` *  The array to get the value from.` |
|       - | 2558 | ` * Return` |
|       - | 2559 | ` *  Poped value or NULL on failure.` |
|       - | 2560 | ` */` |
|      16 | 2561 | `static int ph7_hashmap_pop(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       2 | 2562 |  |
|       - | 2563 | `	ph7_hashmap *pMap;` |
|       - | 2564 | `	/* PHP requires exactly one argument and it must be passed by reference */` |
|      18 | 2565 | `	if( nArg != 1 ){` |
|       7 | 2566 | `		return PH7_VmThrowException(pCtx,` |
|       - | 2567 | `			"ArgumentCountError",` |
|       - | 2568 | `			"array_pop() expects exactly 1 argument, %d given",` |
|       2 | 2569 | `			nArg` |
|       - | 2570 | `			);` |
|       - | 2571 | `	}` |
|       - | 2572 | `	/* Passing a constant (including literals) or non-variable triggers the same` |
|       - | 2573 | `	 * error message as official PHP. Check the index to detect constants. */` |
|      14 | 2574 | `	if( apArg[0]->nIdx == SXU32_HIGH ){` |
|       5 | 2575 | `		return PH7_VmThrowException(pCtx,` |
|       - | 2576 | `			"Error",` |
|       - | 2577 | `			"array_pop(): Argument #1 ($array) could not be passed by reference"` |
|       - | 2578 | `			);` |
|       - | 2579 | `	}` |
|       - | 2580 | `	/* Make sure we are dealing with a valid hashmap */` |
|      10 | 2581 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|       4 | 2582 | `		return PH7_VmThrowException(pCtx,` |
|       - | 2583 | `			"TypeError",` |
|       - | 2584 | `			"array_pop(): Argument #1 ($array) must be of type array, %s given",` |
|       1 | 2585 | `			ph7_type_name(apArg[0])` |
|       - | 2586 | `			);` |
|       - | 2587 | `	}` |
|       7 | 2588 | `	pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|       7 | 2589 | `	if( pMap->nEntry < 1 ){` |
|       - | 2590 | `		/* Nothing to pop,return NULL */` |
|       3 | 2591 | `		ph7_result_null(pCtx);` |
|       2 | 2592 | `	}else{` |
|       5 | 2593 | `		ph7_hashmap_node *pLast = pMap->pLast;` |
|       - | 2594 | `		ph7_value *pObj;` |
|       5 | 2595 | `		pObj = HashmapExtractNodeValue(pLast);` |
|       5 | 2596 | `		if( pObj ){` |
|       - | 2597 | `			/* Node value */` |
|       5 | 2598 | `			ph7_result_value(pCtx,pObj);` |
|       - | 2599 | `			/* Unlink the node */` |
|       5 | 2600 | `			PH7_HashmapUnlinkNode(pLast,TRUE);` |
|       3 | 2601 | `		}else{` |
|     ! 0 | 2602 | `			ph7_result_null(pCtx);` |
|       - | 2603 | `		}` |
|       - | 2604 | `		/* Reset the cursor */` |
|       5 | 2605 | `		pMap->pCur = pMap->pFirst;` |
|       - | 2606 | `	}` |
|       7 | 2607 | `	return PH7_OK;` |
|      10 | 2608 |  |
|       - | 2609 | `/*` |
|       - | 2610 | ` * int array_push($array,$var,...)` |
|       - | 2611 | ` *   Push one or more elements onto the end of array. (Stack insertion)` |
|       - | 2612 | ` * Parameters` |
|       - | 2613 | ` *  array` |
|       - | 2614 | ` *    The input array.` |
|       - | 2615 | ` *  var` |
|       - | 2616 | ` *   On or more value to push.` |
|       - | 2617 | ` * Return` |
|       - | 2618 | ` *  New array count (including old items).` |
|       - | 2619 | ` */` |
|      20 | 2620 | `static int ph7_hashmap_push(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       2 | 2621 |  |
|       - | 2622 | `	ph7_hashmap *pMap;` |
|       - | 2623 | `	sxi32 rc;` |
|       - | 2624 | `	int i;` |
|      22 | 2625 | `	if( nArg < 1 ){` |
|       4 | 2626 | `		return PH7_VmThrowException(pCtx,` |
|       - | 2627 | `			"ArgumentCountError",` |
|       - | 2628 | `			"array_push() expects at least 1 argument, %d given",` |
|       1 | 2629 | `			nArg` |
|       - | 2630 | `			);` |
|       - | 2631 | `	}` |
|       - | 2632 | `	/* Passing a constant (including literals) or non-variable triggers the same` |
|       - | 2633 | `	 * error message as official PHP. Check the index to detect constants. */` |
|      20 | 2634 | `	if( apArg[0]->nIdx == SXU32_HIGH ){` |
|       5 | 2635 | `		return PH7_VmThrowException(pCtx,` |
|       - | 2636 | `			"Error",` |
|       - | 2637 | `			"array_push(): Argument #1 ($array) could not be passed by reference"` |
|       - | 2638 | `			);` |
|       - | 2639 | `	}` |
|       - | 2640 | `	/* Make sure we are dealing with a valid hashmap */` |
|      16 | 2641 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|       4 | 2642 | `		return PH7_VmThrowException(pCtx,` |
|       - | 2643 | `			"TypeError",` |
|       - | 2644 | `			"array_push(): Argument #1 ($array) must be of type array, %s given",` |
|       1 | 2645 | `			ph7_type_name(apArg[0])` |
|       - | 2646 | `			);` |
|       - | 2647 | `	}` |
|       - | 2648 | `	/* Point to the internal representation of the input hashmap */` |
|      13 | 2649 | `	pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|       - | 2650 | `	/* Start pushing given values */` |
|      27 | 2651 | `	for( i = 1 ; i < nArg ; ++i ){` |
|      15 | 2652 | `		rc = PH7_HashmapInsert(pMap,0,apArg[i]);` |
|      15 | 2653 | `		if( rc != SXRET_OK ){` |
|     ! 0 | 2654 | `			break;` |
|       - | 2655 | `		}` |
|       8 | 2656 | `	}` |
|       - | 2657 | `	/* Return the new count */` |
|      13 | 2658 | `	ph7_result_int64(pCtx,(sxi64)pMap->nEntry);` |
|      13 | 2659 | `	return PH7_OK;` |
|      12 | 2660 |  |
|       - | 2661 | `/*` |
|       - | 2662 | ` * value array_shift(array $array)` |
|       - | 2663 | ` *   Shift an element off the beginning of array.` |
|       - | 2664 | ` * Parameter` |
|       - | 2665 | ` *  The array to get the value from.` |
|       - | 2666 | ` * Return` |
|       - | 2667 | ` *  Shifted value or NULL on failure.` |
|       - | 2668 | ` */` |
|      36 | 2669 | `static int ph7_hashmap_shift(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       2 | 2670 |  |
|       - | 2671 | `	ph7_hashmap *pMap;` |
|       - | 2672 | `	/* PHP requires exactly one argument and it must be passed by reference */` |
|      38 | 2673 | `	if( nArg != 1 ){` |
|       7 | 2674 | `		return PH7_VmThrowException(pCtx,` |
|       - | 2675 | `			"ArgumentCountError",` |
|       - | 2676 | `			"array_shift() expects exactly 1 argument, %d given",` |
|       2 | 2677 | `			nArg` |
|       - | 2678 | `			);` |
|       - | 2679 | `	}` |
|       - | 2680 | `	/* Detect constants or literals, which cannot be passed by reference. */` |
|      34 | 2681 | `	if( apArg[0]->nIdx == SXU32_HIGH ){` |
|       5 | 2682 | `		return PH7_VmThrowException(pCtx,` |
|       - | 2683 | `			"Error",` |
|       - | 2684 | `			"array_shift(): Argument #1 ($array) could not be passed by reference"` |
|       - | 2685 | `			);` |
|       - | 2686 | `	}` |
|       - | 2687 | `	/* Make sure we are dealing with a valid hashmap */` |
|      30 | 2688 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|       4 | 2689 | `		return PH7_VmThrowException(pCtx,` |
|       - | 2690 | `			"TypeError",` |
|       - | 2691 | `			"array_shift(): Argument #1 ($array) must be of type array, %s given",` |
|       1 | 2692 | `			ph7_type_name(apArg[0])` |
|       - | 2693 | `			);` |
|       - | 2694 | `	}` |
|       - | 2695 | `	/* Point to the internal representation of the hashmap */` |
|      28 | 2696 | `	pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|      28 | 2697 | `	if( pMap->nEntry < 1 ){` |
|       - | 2698 | `		/* Empty hashmap,return NULL */` |
|       3 | 2699 | `		ph7_result_null(pCtx);` |
|       2 | 2700 | `	}else{` |
|      26 | 2701 | `		ph7_hashmap_node *pEntry = pMap->pFirst;` |
|       - | 2702 | `		ph7_value *pObj;` |
|       - | 2703 | `		sxu32 n;` |
|      26 | 2704 | `		pObj = HashmapExtractNodeValue(pEntry);` |
|      26 | 2705 | `		if( pObj ){` |
|       - | 2706 | `			/* Node value */` |
|      26 | 2707 | `			ph7_result_value(pCtx,pObj);` |
|       - | 2708 | `			/* Unlink the first node */` |
|      26 | 2709 | `			PH7_HashmapUnlinkNode(pEntry,TRUE);` |
|      14 | 2710 | `		}else{` |
|     ! 0 | 2711 | `			ph7_result_null(pCtx);` |
|       - | 2712 | `		}` |
|       - | 2713 | `		/* Rehash all int keys */` |
|      26 | 2714 | `		n = pMap->nEntry;` |
|      26 | 2715 | `		pEntry = pMap->pFirst;` |
|      26 | 2716 | `		pMap->iNextIdx = 0; /* Reset the automatic index */` |
|      37 | 2717 | `		for(;;){` |
|      76 | 2718 | `			if( n < 1 ){` |
|      26 | 2719 | `				break;` |
|       - | 2720 | `			}` |
|      52 | 2721 | `			if( pEntry->iType == HASHMAP_INT_NODE ){` |
|      52 | 2722 | `				HashmapRehashIntNode(pEntry);` |
|      25 | 2723 | `			}` |
|       - | 2724 | `			/* Point to the next entry */` |
|      52 | 2725 | `			pEntry = pEntry->pPrev; /* Reverse link */` |
|      52 | 2726 | `			n--;` |
|       2 | 2727 | `		}` |
|       - | 2728 | `		/* Reset the cursor */` |
|      26 | 2729 | `		pMap->pCur = pMap->pFirst;` |
|       - | 2730 | `	}` |
|      28 | 2731 | `	return PH7_OK;` |
|      20 | 2732 |  |
|       - | 2733 | `/*` |
|       - | 2734 | ` * Extract the node cursor value.` |
|       - | 2735 | ` */` |
|      24 | 2736 | `static sxi32 HashmapCurrentValue(ph7_context *pCtx,ph7_hashmap *pMap,int iDirection)` |
|       1 | 2737 |  |
|      25 | 2738 | `	ph7_hashmap_node *pCur = pMap->pCur;` |
|       - | 2739 | `	ph7_value *pVal;` |
|      25 | 2740 | `	if( pCur == 0 ){` |
|       - | 2741 | `		/* Cursor does not point to anything,return FALSE */` |
|     ! 0 | 2742 | `		ph7_result_bool(pCtx,0);` |
|     ! 0 | 2743 | `		return PH7_OK;` |
|       - | 2744 | `	}` |
|      25 | 2745 | `	if( iDirection != 0 ){` |
|       9 | 2746 | `		if( iDirection > 0 ){` |
|       - | 2747 | `			/* Point to the next entry */` |
|       7 | 2748 | `			pMap->pCur = pCur->pPrev; /* Reverse link */` |
|       7 | 2749 | `			pCur = pMap->pCur;` |
|       4 | 2750 | `		}else{` |
|       - | 2751 | `			/* Point to the previous entry */` |
|       3 | 2752 | `			pMap->pCur = pCur->pNext; /* Reverse link */` |
|       3 | 2753 | `			pCur = pMap->pCur;` |
|       - | 2754 | `		}` |
|       9 | 2755 | `		if( pCur == 0 ){` |
|       - | 2756 | `			/* End of input reached,return FALSE */` |
|     ! 0 | 2757 | `			ph7_result_bool(pCtx,0);` |
|     ! 0 | 2758 | `			return PH7_OK;` |
|       - | 2759 | `		}` |
|       4 | 2760 | `	}` |
|       - | 2761 | `	/* Point to the desired element */` |
|      25 | 2762 | `	pVal = HashmapExtractNodeValue(pCur);` |
|      25 | 2763 | `	if( pVal ){` |
|      25 | 2764 | `		ph7_result_value(pCtx,pVal);` |
|      13 | 2765 | `	}else{` |
|     ! 0 | 2766 | `		ph7_result_bool(pCtx,0);` |
|       - | 2767 | `	}` |
|      25 | 2768 | `	return PH7_OK;` |
|      13 | 2769 |  |
|       - | 2770 | `/*` |
|       - | 2771 | ` * value current(array $array)` |
|       - | 2772 | ` *  Return the current element in an array.` |
|       - | 2773 | ` * Parameter` |
|       - | 2774 | ` *  $input: The input array.` |
|       - | 2775 | ` * Return` |
|       - | 2776 | ` *  The current() function simply returns the value of the array element that's currently` |
|       - | 2777 | ` *  being pointed to by the internal pointer. It does not move the pointer in any way.` |
|       - | 2778 | ` *  If the internal pointer points beyond the end of the elements list or the array` |
|       - | 2779 | ` *  is empty, current() returns FALSE.` |
|       - | 2780 | ` */` |
|      10 | 2781 | `static int ph7_hashmap_current(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       1 | 2782 |  |
|      11 | 2783 | `	if( nArg < 1 ){` |
|       - | 2784 | `		/* Missing arguments,return FALSE */` |
|     ! 0 | 2785 | `		ph7_result_bool(pCtx,0);` |
|     ! 0 | 2786 | `		return PH7_OK;` |
|       - | 2787 | `	}` |
|       - | 2788 | `	/* Make sure we are dealing with a valid hashmap */` |
|      11 | 2789 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|       - | 2790 | `		/* Invalid argument,return FALSE */` |
|     ! 0 | 2791 | `		ph7_result_bool(pCtx,0);` |
|     ! 0 | 2792 | `		return PH7_OK;` |
|       - | 2793 | `	}` |
|      11 | 2794 | `	HashmapCurrentValue(&(*pCtx),(ph7_hashmap *)apArg[0]->x.pOther,0);` |
|      11 | 2795 | `	return PH7_OK;` |
|       6 | 2796 |  |
|       - | 2797 | `/*` |
|       - | 2798 | ` * value next(array $input)` |
|       - | 2799 | ` *  Advance the internal array pointer of an array.` |
|       - | 2800 | ` * Parameter` |
|       - | 2801 | ` *  $input: The input array.` |
|       - | 2802 | ` * Return` |
|       - | 2803 | ` *  next() behaves like current(), with one difference. It advances the internal array` |
|       - | 2804 | ` *  pointer one place forward before returning the element value. That means it returns` |
|       - | 2805 | ` *  the next array value and advances the internal array pointer by one.` |
|       - | 2806 | ` */` |
|       6 | 2807 | `static int ph7_hashmap_next(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       1 | 2808 |  |
|       7 | 2809 | `	if( nArg < 1 ){` |
|       - | 2810 | `		/* Missing arguments,return FALSE */` |
|     ! 0 | 2811 | `		ph7_result_bool(pCtx,0);` |
|     ! 0 | 2812 | `		return PH7_OK;` |
|       - | 2813 | `	}` |
|       - | 2814 | `	/* Make sure we are dealing with a valid hashmap */` |
|       7 | 2815 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|       - | 2816 | `		/* Invalid argument,return FALSE */` |
|     ! 0 | 2817 | `		ph7_result_bool(pCtx,0);` |
|     ! 0 | 2818 | `		return PH7_OK;` |
|       - | 2819 | `	}` |
|       7 | 2820 | `	HashmapCurrentValue(&(*pCtx),(ph7_hashmap *)apArg[0]->x.pOther,1);` |
|       7 | 2821 | `	return PH7_OK;` |
|       4 | 2822 |  |
|       - | 2823 | `/*` |
|       - | 2824 | ` * value prev(array $input)` |
|       - | 2825 | ` *  Rewind the internal array pointer.` |
|       - | 2826 | ` * Parameter` |
|       - | 2827 | ` *  $input: The input array.` |
|       - | 2828 | ` * Return` |
|       - | 2829 | ` *  Returns the array value in the previous place that's pointed` |
|       - | 2830 | ` *  to by the internal array pointer, or FALSE if there are no more` |
|       - | 2831 | ` *  elements.` |
|       - | 2832 | ` */` |
|       2 | 2833 | `static int ph7_hashmap_prev(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       1 | 2834 |  |
|       3 | 2835 | `	if( nArg < 1 ){` |
|       - | 2836 | `		/* Missing arguments,return FALSE */` |
|     ! 0 | 2837 | `		ph7_result_bool(pCtx,0);` |
|     ! 0 | 2838 | `		return PH7_OK;` |
|       - | 2839 | `	}` |
|       - | 2840 | `	/* Make sure we are dealing with a valid hashmap */` |
|       3 | 2841 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|       - | 2842 | `		/* Invalid argument,return FALSE */` |
|     ! 0 | 2843 | `		ph7_result_bool(pCtx,0);` |
|     ! 0 | 2844 | `		return PH7_OK;` |
|       - | 2845 | `	}` |
|       3 | 2846 | `	HashmapCurrentValue(&(*pCtx),(ph7_hashmap *)apArg[0]->x.pOther,-1);` |
|       3 | 2847 | `	return PH7_OK;` |
|       2 | 2848 |  |
|       - | 2849 | `/*` |
|       - | 2850 | ` * value end(array $input)` |
|       - | 2851 | ` *  Set the internal pointer of an array to its last element.` |
|       - | 2852 | ` * Parameter` |
|       - | 2853 | ` *  $input: The input array.` |
|       - | 2854 | ` * Return` |
|       - | 2855 | ` *  Returns the value of the last element or FALSE for empty array.` |
|       - | 2856 | ` */` |
|       2 | 2857 | `static int ph7_hashmap_end(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       1 | 2858 |  |
|       - | 2859 | `	ph7_hashmap *pMap;` |
|       3 | 2860 | `	if( nArg < 1 ){` |
|       - | 2861 | `		/* Missing arguments,return FALSE */` |
|     ! 0 | 2862 | `		ph7_result_bool(pCtx,0);` |
|     ! 0 | 2863 | `		return PH7_OK;` |
|       - | 2864 | `	}` |
|       - | 2865 | `	/* Make sure we are dealing with a valid hashmap */` |
|       3 | 2866 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|       - | 2867 | `		/* Invalid argument,return FALSE */` |
|     ! 0 | 2868 | `		ph7_result_bool(pCtx,0);` |
|     ! 0 | 2869 | `		return PH7_OK;` |
|       - | 2870 | `	}` |
|       - | 2871 | `	/* Point to the internal representation of the input hashmap */` |
|       3 | 2872 | `	pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|       - | 2873 | `	/* Point to the last node */` |
|       3 | 2874 | `	pMap->pCur = pMap->pLast;` |
|       - | 2875 | `	/* Return the last node value */` |
|       3 | 2876 | `	HashmapCurrentValue(&(*pCtx),pMap,0);` |
|       3 | 2877 | `	return PH7_OK;` |
|       2 | 2878 |  |
|       - | 2879 | `/*` |
|       - | 2880 | ` * value reset(array $array )` |
|       - | 2881 | ` *  Set the internal pointer of an array to its first element.` |
|       - | 2882 | ` * Parameter` |
|       - | 2883 | ` *  $input: The input array.` |
|       - | 2884 | ` * Return` |
|       - | 2885 | ` *  Returns the value of the first array element,or FALSE if the array is empty.` |
|       - | 2886 | ` */` |
|       4 | 2887 | `static int ph7_hashmap_reset(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       1 | 2888 |  |
|       - | 2889 | `	ph7_hashmap *pMap;` |
|       5 | 2890 | `	if( nArg < 1 ){` |
|       - | 2891 | `		/* Missing arguments,return FALSE */` |
|     ! 0 | 2892 | `		ph7_result_bool(pCtx,0);` |
|     ! 0 | 2893 | `		return PH7_OK;` |
|       - | 2894 | `	}` |
|       - | 2895 | `	/* Make sure we are dealing with a valid hashmap */` |
|       5 | 2896 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|       - | 2897 | `		/* Invalid argument,return FALSE */` |
|     ! 0 | 2898 | `		ph7_result_bool(pCtx,0);` |
|     ! 0 | 2899 | `		return PH7_OK;` |
|       - | 2900 | `	}` |
|       - | 2901 | `	/* Point to the internal representation of the input hashmap */` |
|       5 | 2902 | `	pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|       - | 2903 | `	/* Point to the first node */` |
|       5 | 2904 | `	pMap->pCur = pMap->pFirst;` |
|       - | 2905 | `	/* Return the last node value if available */` |
|       5 | 2906 | `	HashmapCurrentValue(&(*pCtx),pMap,0);` |
|       5 | 2907 | `	return PH7_OK;` |
|       3 | 2908 |  |
|       - | 2909 | `/*` |
|       - | 2910 | ` * value key(array $array)` |
|       - | 2911 | ` *   Fetch a key from an array` |
|       - | 2912 | ` * Parameter` |
|       - | 2913 | ` *  $input` |
|       - | 2914 | ` *   The input array.` |
|       - | 2915 | ` * Return` |
|       - | 2916 | ` *  The key() function simply returns the key of the array element that's currently` |
|       - | 2917 | ` *  being pointed to by the internal pointer. It does not move the pointer in any way.` |
|       - | 2918 | ` *  If the internal pointer points beyond the end of the elements list or the array` |
|       - | 2919 | ` *  is empty, key() returns NULL.` |
|       - | 2920 | ` */` |
|       4 | 2921 | `static int ph7_hashmap_simple_key(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       1 | 2922 |  |
|       - | 2923 | `	ph7_hashmap_node *pCur;` |
|       - | 2924 | `	ph7_hashmap *pMap;` |
|       5 | 2925 | `	if( nArg < 1 ){` |
|       - | 2926 | `		/* Missing arguments,return NULL */` |
|     ! 0 | 2927 | `		ph7_result_null(pCtx);` |
|     ! 0 | 2928 | `		return PH7_OK;` |
|       - | 2929 | `	}` |
|       - | 2930 | `	/* Make sure we are dealing with a valid hashmap */` |
|       5 | 2931 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|       - | 2932 | `		/* Invalid argument,return NULL */` |
|     ! 0 | 2933 | `		ph7_result_null(pCtx);` |
|     ! 0 | 2934 | `		return PH7_OK;` |
|       - | 2935 | `	}` |
|       5 | 2936 | `	pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|       5 | 2937 | `	pCur = pMap->pCur;` |
|       5 | 2938 | `	if( pCur == 0 ){` |
|       - | 2939 | `		/* Cursor does not point to anything,return NULL */` |
|     ! 0 | 2940 | `		ph7_result_null(pCtx);` |
|     ! 0 | 2941 | `		return PH7_OK;` |
|       - | 2942 | `	}` |
|       5 | 2943 | `	if( pCur->iType == HASHMAP_INT_NODE){` |
|       - | 2944 | `		/* Key is integer */` |
|     ! 0 | 2945 | `		ph7_result_int64(pCtx,pCur->xKey.iKey);` |
|     ! 0 | 2946 | `	}else{` |
|       - | 2947 | `		/* Key is blob */` |
|       7 | 2948 | `		ph7_result_string(pCtx,` |
|       4 | 2949 | `			(const char *)SyBlobData(&pCur->xKey.sKey),(int)SyBlobLength(&pCur->xKey.sKey));` |
|       - | 2950 | `	}` |
|       5 | 2951 | `	return PH7_OK;` |
|       3 | 2952 |  |
|       - | 2953 | `/*` |
|       - | 2954 | ` * array each(array $input)` |
|       - | 2955 | ` *  Return the current key and value pair from an array and advance the array cursor.` |
|       - | 2956 | ` * Parameter` |
|       - | 2957 | ` *  $input` |
|       - | 2958 | ` *    The input array.` |
|       - | 2959 | ` * Return` |
|       - | 2960 | ` *  Returns the current key and value pair from the array array. This pair is returned` |
|       - | 2961 | ` *  in a four-element array, with the keys 0, 1, key, and value. Elements 0 and key` |
|       - | 2962 | ` *  contain the key name of the array element, and 1 and value contain the data.` |
|       - | 2963 | ` *  If the internal pointer for the array points past the end of the array contents` |
|       - | 2964 | ` *  each() returns FALSE.` |
|       - | 2965 | ` */` |
|      22 | 2966 | `static int ph7_hashmap_each(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       1 | 2967 |  |
|       - | 2968 | `	ph7_hashmap_node *pCur;` |
|       - | 2969 | `	ph7_hashmap *pMap;` |
|       - | 2970 | `	ph7_value *pArray;` |
|       - | 2971 | `	ph7_value *pVal;` |
|       - | 2972 | `	ph7_value sKey;` |
|      23 | 2973 | `	if( nArg < 1 ){` |
|       - | 2974 | `		/* Missing arguments,return FALSE */` |
|     ! 0 | 2975 | `		ph7_result_bool(pCtx,0);` |
|     ! 0 | 2976 | `		return PH7_OK;` |
|       - | 2977 | `	}` |
|       - | 2978 | `	/* Make sure we are dealing with a valid hashmap */` |
|      23 | 2979 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|       - | 2980 | `		/* Invalid argument,return FALSE */` |
|     ! 0 | 2981 | `		ph7_result_bool(pCtx,0);` |
|     ! 0 | 2982 | `		return PH7_OK;` |
|       - | 2983 | `	}` |
|       - | 2984 | `	/* Point to the internal representation that describe the input hashmap */` |
|      23 | 2985 | `	pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|      23 | 2986 | `	if( pMap->pCur == 0 ){` |
|       - | 2987 | `		/* Cursor does not point to anything,return FALSE */` |
|       9 | 2988 | `		ph7_result_bool(pCtx,0);` |
|       9 | 2989 | `		return PH7_OK;` |
|       - | 2990 | `	}` |
|      15 | 2991 | `	pCur = pMap->pCur;` |
|       - | 2992 | `	/* Create a new array */` |
|      15 | 2993 | `	pArray = ph7_context_new_array(pCtx);` |
|      15 | 2994 | `	if( pArray == 0 ){` |
|     ! 0 | 2995 | `		ph7_result_bool(pCtx,0);` |
|     ! 0 | 2996 | `		return PH7_OK;` |
|       - | 2997 | `	}` |
|      15 | 2998 | `	pVal = HashmapExtractNodeValue(pCur);` |
|       - | 2999 | `	/* Insert the current value */` |
|      15 | 3000 | `	ph7_array_add_intkey_elem(pArray,1,pVal);` |
|      15 | 3001 | `	ph7_array_add_strkey_elem(pArray,"value",pVal);` |
|       - | 3002 | `	/* Make the key */` |
|      15 | 3003 | `	if( pCur->iType == HASHMAP_INT_NODE ){` |
|       7 | 3004 | `		PH7_MemObjInitFromInt(pMap->pVm,&sKey,pCur->xKey.iKey);` |
|       4 | 3005 | `	}else{` |
|       9 | 3006 | `		PH7_MemObjInitFromString(pMap->pVm,&sKey,0);` |
|       9 | 3007 | `		PH7_MemObjStringAppend(&sKey,(const char *)SyBlobData(&pCur->xKey.sKey),SyBlobLength(&pCur->xKey.sKey));` |
|       - | 3008 | `	}` |
|       - | 3009 | `	/* Insert the current key */` |
|      15 | 3010 | `	ph7_array_add_intkey_elem(pArray,0,&sKey);` |
|      15 | 3011 | `	ph7_array_add_strkey_elem(pArray,"key",&sKey);` |
|      15 | 3012 | `	PH7_MemObjRelease(&sKey);` |
|       - | 3013 | `	/* Advance the cursor */` |
|      15 | 3014 | `	pMap->pCur = pCur->pPrev; /* Reverse link */` |
|       - | 3015 | `	/* Return the current entry */` |
|      15 | 3016 | `	ph7_result_value(pCtx,pArray);` |
|      15 | 3017 | `	return PH7_OK;` |
|      12 | 3018 |  |
|       - | 3019 | `/*` |
|       - | 3020 | ` * array range(int $start,int $limit,int $step)` |
|       - | 3021 | ` *  Create an array containing a range of elements` |
|       - | 3022 | ` * Parameter` |
|       - | 3023 | ` *  start` |
|       - | 3024 | ` *   First value of the sequence.` |
|       - | 3025 | ` *  limit` |
|       - | 3026 | ` *   The sequence is ended upon reaching the limit value.` |
|       - | 3027 | ` *  step` |
|       - | 3028 | ` *  If a step value is given, it will be used as the increment between elements in the sequence.` |
|       - | 3029 | ` *  step should be given as a positive number. If not specified, step will default to 1.` |
|       - | 3030 | ` * Return` |
|       - | 3031 | ` *  An array of elements from start to limit, inclusive.` |
|       - | 3032 | ` * NOTE:` |
|       - | 3033 | ` *  Only 32/64 bit integer key is supported.` |
|       - | 3034 | ` */` |
|       2 | 3035 | `static int ph7_hashmap_range(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       1 | 3036 |  |
|       - | 3037 | `	ph7_value *pValue,*pArray;` |
|       - | 3038 | `	sxi64 iOfft,iLimit;` |
|       3 | 3039 | `	int iStep = 1;` |
|       - | 3040 |  |
|       3 | 3041 | `	iOfft = iLimit = 0; /* cc -O6 */` |
|       3 | 3042 | `	if( nArg > 0 ){` |
|       - | 3043 | `		/* Extract the offset */` |
|       3 | 3044 | `		iOfft = ph7_value_to_int64(apArg[0]);` |
|       3 | 3045 | `		if( nArg > 1 ){` |
|       - | 3046 | `			/* Extract the limit */` |
|       3 | 3047 | `			iLimit = ph7_value_to_int64(apArg[1]);` |
|       3 | 3048 | `			if( nArg > 2 ){` |
|       - | 3049 | `				/* Extract the increment */` |
|       3 | 3050 | `				iStep = ph7_value_to_int(apArg[2]);` |
|       3 | 3051 | `				if( iStep < 1 ){` |
|       - | 3052 | `					/* Only positive number are allowed */` |
|       3 | 3053 | `					iStep = 1;` |
|       1 | 3054 | `				}` |
|       1 | 3055 | `			}` |
|       1 | 3056 | `		}` |
|       1 | 3057 | `	}` |
|       - | 3058 | `	/* Element container */` |
|       3 | 3059 | `	pValue = ph7_context_new_scalar(pCtx);` |
|       - | 3060 | `	/* Create the new array */` |
|       3 | 3061 | `	pArray = ph7_context_new_array(pCtx);` |
|       3 | 3062 | `	if( pArray == 0 ){` |
|     ! 0 | 3063 | `		ph7_result_null(pCtx);` |
|     ! 0 | 3064 | `		return PH7_OK;` |
|       - | 3065 | `	}` |
|       - | 3066 | `	/* Start filling */` |
|       3 | 3067 | `	while( iOfft <= iLimit ){` |
|     ! 0 | 3068 | `		ph7_value_int64(pValue,iOfft);` |
|       - | 3069 | `		/* Perform the insertion */` |
|     ! 0 | 3070 | `		ph7_array_add_elem(pArray,0/* Automatic index assign*/,pValue);` |
|       - | 3071 | `		/* Increment */` |
|     ! 0 | 3072 | `		iOfft += iStep;` |
|     ! 0 | 3073 | `	}` |
|       - | 3074 | `	/* Return the new array */` |
|       3 | 3075 | `	ph7_result_value(pCtx,pArray);` |
|       - | 3076 | `	/* Dont'worry about freeing 'pValue',it will be released automatically` |
|       - | 3077 | `	 * by the virtual machine as soon we return from this foreign function.` |
|       - | 3078 | `	 */` |
|       3 | 3079 | `	return PH7_OK;` |
|       2 | 3080 |  |
|       - | 3081 | `/*` |
|       - | 3082 | ` * array array_values(array $input)` |
|       - | 3083 | ` *   Returns all the values from the input array and indexes numerically the array.` |
|       - | 3084 | ` * Parameters` |
|       - | 3085 | ` *   input: The input array.` |
|       - | 3086 | ` * Return` |
|       - | 3087 | ` *  An indexed array of values or NULL on failure.` |
|       - | 3088 | ` */` |
|      24 | 3089 | `static int ph7_hashmap_values(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       1 | 3090 |  |
|       - | 3091 | `	ph7_hashmap_node *pNode;` |
|       - | 3092 | `	ph7_hashmap *pMap;` |
|       - | 3093 | `	ph7_value *pArray;` |
|       - | 3094 | `	ph7_value *pObj;` |
|       - | 3095 | `	sxu32 n;` |
|      25 | 3096 | `	if( nArg < 1 ){` |
|       - | 3097 | `		/* Missing arguments,return NULL */` |
|       3 | 3098 | `		ph7_result_null(pCtx);` |
|       3 | 3099 | `		return PH7_OK;` |
|       - | 3100 | `	}` |
|       - | 3101 | `	/* Make sure we are dealing with a valid hashmap */` |
|      23 | 3102 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|       - | 3103 | `		/* Invalid argument,return NULL */` |
|     ! 0 | 3104 | `		ph7_result_null(pCtx);` |
|     ! 0 | 3105 | `		return PH7_OK;` |
|       - | 3106 | `	}` |
|       - | 3107 | `	/* Point to the internal representation that describe the input hashmap */` |
|      23 | 3108 | `	pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|       - | 3109 | `	/* Create a new array */` |
|      23 | 3110 | `	pArray = ph7_context_new_array(pCtx);` |
|      23 | 3111 | `	if( pArray == 0 ){` |
|     ! 0 | 3112 | `		ph7_result_null(pCtx);` |
|     ! 0 | 3113 | `		return PH7_OK;` |
|       - | 3114 | `	}` |
|       - | 3115 | `	/* Perform the requested operation */` |
|      23 | 3116 | `	pNode = pMap->pFirst;` |
|      81 | 3117 | `	for( n = 0 ; n < pMap->nEntry ; ++n ){` |
|      59 | 3118 | `		pObj = HashmapExtractNodeValue(pNode);` |
|      59 | 3119 | `		if( pObj ){` |
|       - | 3120 | `			/* perform the insertion */` |
|      59 | 3121 | `			ph7_array_add_elem(pArray,0/* Automatic index assign */,pObj);` |
|      29 | 3122 | `		}` |
|       - | 3123 | `		/* Point to the next entry */` |
|      59 | 3124 | `		pNode = pNode->pPrev; /* Reverse link */` |
|      30 | 3125 | `	}` |
|       - | 3126 | `	/* return the new array */` |
|      23 | 3127 | `	ph7_result_value(pCtx,pArray);` |
|      23 | 3128 | `	return PH7_OK;` |
|      13 | 3129 |  |
|       - | 3130 | `/*` |
|       - | 3131 | ` * array array_keys(array $input [, val $search_value [, bool $strict = false ]] )` |
|       - | 3132 | ` *  Return all the keys or a subset of the keys of an array.` |
|       - | 3133 | ` * Parameters` |
|       - | 3134 | ` *  $input` |
|       - | 3135 | ` *   An array containing keys to return.` |
|       - | 3136 | ` * $search_value` |
|       - | 3137 | ` *   If specified, then only keys containing these values are returned.` |
|       - | 3138 | ` * $strict` |
|       - | 3139 | ` *   Determines if strict comparison (===) should be used during the search.` |
|       - | 3140 | ` * Return` |
|       - | 3141 | ` *  An array of all the keys in input or NULL on failure.` |
|       - | 3142 | ` */` |
|      98 | 3143 | `static int ph7_hashmap_keys(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       1 | 3144 |  |
|       - | 3145 | `	ph7_hashmap_node *pNode;` |
|       - | 3146 | `	ph7_hashmap *pMap;` |
|       - | 3147 | `	ph7_value *pArray;` |
|       - | 3148 | `	ph7_value sObj;` |
|       - | 3149 | `	ph7_value sVal;` |
|       - | 3150 | `	SyString sKey;` |
|       - | 3151 | `	int bStrict;` |
|       - | 3152 | `	sxi32 rc;` |
|       - | 3153 | `	sxu32 n;` |
|      99 | 3154 | `	if( nArg < 1 ){` |
|       - | 3155 | `		/* Missing arguments,return NULL */` |
|       3 | 3156 | `		ph7_result_null(pCtx);` |
|       3 | 3157 | `		return PH7_OK;` |
|       - | 3158 | `	}` |
|       - | 3159 | `	/* Make sure we are dealing with a valid hashmap */` |
|      97 | 3160 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|       - | 3161 | `		/* Invalid argument,return NULL */` |
|     ! 0 | 3162 | `		ph7_result_null(pCtx);` |
|     ! 0 | 3163 | `		return PH7_OK;` |
|       - | 3164 | `	}` |
|       - | 3165 | `	/* Point to the internal representation of the input hashmap */` |
|      97 | 3166 | `	pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|       - | 3167 | `	/* Create a new array */` |
|      97 | 3168 | `	pArray = ph7_context_new_array(pCtx);` |
|      97 | 3169 | `	if( pArray == 0 ){` |
|     ! 0 | 3170 | `		ph7_result_null(pCtx);` |
|     ! 0 | 3171 | `		return PH7_OK;` |
|       - | 3172 | `	}` |
|      97 | 3173 | `	bStrict = FALSE;` |
|      97 | 3174 | `	if( nArg > 2 && ph7_value_is_bool(apArg[2]) ){` |
|     ! 0 | 3175 | `		bStrict = ph7_value_to_bool(apArg[2]);` |
|     ! 0 | 3176 | `	}` |
|       - | 3177 | `	/* Perform the requested operation */` |
|      97 | 3178 | `	pNode = pMap->pFirst;` |
|      97 | 3179 | `	PH7_MemObjInit(pMap->pVm,&sVal);` |
|     481 | 3180 | `	for( n = 0 ; n < pMap->nEntry ; ++n ){` |
|     385 | 3181 | `		if( pNode->iType == HASHMAP_INT_NODE ){` |
|      81 | 3182 | `			PH7_MemObjInitFromInt(pMap->pVm,&sObj,pNode->xKey.iKey);` |
|      41 | 3183 | `		}else{` |
|     305 | 3184 | `			SyStringInitFromBuf(&sKey,SyBlobData(&pNode->xKey.sKey),SyBlobLength(&pNode->xKey.sKey));` |
|     305 | 3185 | `			PH7_MemObjInitFromString(pMap->pVm,&sObj,&sKey);` |
|       - | 3186 | `		}` |
|     385 | 3187 | `		rc = 0;` |
|     385 | 3188 | `		if( nArg > 1 ){` |
|     ! 0 | 3189 | `			ph7_value *pValue = HashmapExtractNodeValue(pNode);` |
|     ! 0 | 3190 | `			if( pValue ){` |
|     ! 0 | 3191 | `				PH7_MemObjLoad(pValue,&sVal);` |
|       - | 3192 | `				/* Filter key */` |
|     ! 0 | 3193 | `				rc = ph7_value_compare(&sVal,apArg[1],bStrict);` |
|     ! 0 | 3194 | `				PH7_MemObjRelease(pValue);` |
|     ! 0 | 3195 | `			}` |
|     ! 0 | 3196 | `		}` |
|     385 | 3197 | `		if( rc == 0 ){` |
|       - | 3198 | `			/* Perform the insertion */` |
|     385 | 3199 | `			ph7_array_add_elem(pArray,0,&sObj);` |
|     192 | 3200 | `		}` |
|     385 | 3201 | `		PH7_MemObjRelease(&sObj);` |
|       - | 3202 | `		/* Point to the next entry */` |
|     385 | 3203 | `		pNode = pNode->pPrev; /* Reverse link */` |
|     193 | 3204 | `	}` |
|       - | 3205 | `	/* return the new array */` |
|      97 | 3206 | `	ph7_result_value(pCtx,pArray);` |
|      97 | 3207 | `	return PH7_OK;` |
|      50 | 3208 |  |
|       - | 3209 | `/*` |
|       - | 3210 | ` * bool array_same(array $arr1,array $arr2)` |
|       - | 3211 | ` *  Return TRUE if the given arrays are the same instance.` |
|       - | 3212 | ` *  This function is useful under PH7 since arrays are passed` |
|       - | 3213 | ` *  by reference unlike the zend engine which use pass by values.` |
|       - | 3214 | ` * Parameters` |
|       - | 3215 | ` *  $arr1` |
|       - | 3216 | ` *   First array` |
|       - | 3217 | ` *  $arr2` |
|       - | 3218 | ` *   Second array` |
|       - | 3219 | ` * Return` |
|       - | 3220 | ` *  TRUE if the arrays are the same instance.FALSE otherwise.` |
|       - | 3221 | ` * Note` |
|       - | 3222 | ` *  This function is a symisc eXtension.` |
|       - | 3223 | ` */` |
|       4 | 3224 | `static int ph7_hashmap_same(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       1 | 3225 |  |
|       - | 3226 | `	ph7_hashmap *p1,*p2;` |
|       - | 3227 | `	int rc;` |
|       5 | 3228 | `	if( nArg < 2 \|\| !ph7_value_is_array(apArg[0]) \|\| !ph7_value_is_array(apArg[1]) ){` |
|       - | 3229 | `		/* Missing or invalid arguments,return FALSE*/` |
|     ! 0 | 3230 | `		ph7_result_bool(pCtx,0);` |
|     ! 0 | 3231 | `		return PH7_OK;` |
|       - | 3232 | `	}` |
|       - | 3233 | `	/* Point to the hashmaps */` |
|       5 | 3234 | `	p1 = (ph7_hashmap *)apArg[0]->x.pOther;` |
|       5 | 3235 | `	p2 = (ph7_hashmap *)apArg[1]->x.pOther;` |
|       5 | 3236 | `	rc = (p1 == p2);` |
|       - | 3237 | `	/* Same instance? */` |
|       5 | 3238 | `	ph7_result_bool(pCtx,rc);` |
|       5 | 3239 | `	return PH7_OK;` |
|       3 | 3240 |  |
|       - | 3241 | `/*` |
|       - | 3242 | ` * array array_merge(array $array1,...)` |
|       - | 3243 | ` *  Merge one or more arrays.` |
|       - | 3244 | ` * Parameters` |
|       - | 3245 | ` *  $array1` |
|       - | 3246 | ` *    Initial array to merge.` |
|       - | 3247 | ` *  ...` |
|       - | 3248 | ` *   More array to merge.` |
|       - | 3249 | ` * Return` |
|       - | 3250 | ` *  The resulting array.` |
|       - | 3251 | ` */` |
|     792 | 3252 | `static int ph7_hashmap_merge(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       2 | 3253 |  |
|       - | 3254 | `	ph7_hashmap *pMap,*pSrc;` |
|       - | 3255 | `	ph7_value *pArray;` |
|       - | 3256 | `	int i;` |
|     794 | 3257 | `	if( nArg < 1 ){` |
|       - | 3258 | `		/* Missing arguments,return NULL */` |
|     ! 0 | 3259 | `		ph7_result_null(pCtx);` |
|     ! 0 | 3260 | `		return PH7_OK;` |
|       - | 3261 | `	}` |
|       - | 3262 | `	/* Create a new array */` |
|     794 | 3263 | `	pArray = ph7_context_new_array(pCtx);` |
|     794 | 3264 | `	if( pArray == 0 ){` |
|     ! 0 | 3265 | `		ph7_result_null(pCtx);` |
|     ! 0 | 3266 | `		return PH7_OK;` |
|       - | 3267 | `	}` |
|       - | 3268 | `	/* Point to the internal representation of the hashmap */` |
|     794 | 3269 | `	pMap = (ph7_hashmap *)pArray->x.pOther;` |
|       - | 3270 | `	/* Start merging */` |
|    2378 | 3271 | `	for( i = 0 ; i < nArg ; i++ ){` |
|       - | 3272 | `		/* Make sure we are dealing with a valid hashmap */` |
|    1586 | 3273 | `		if( !ph7_value_is_array(apArg[i]) ){` |
|       - | 3274 | `			/* Insert scalar value */` |
|       5 | 3275 | `			ph7_array_add_elem(pArray,0,apArg[i]);` |
|       3 | 3276 | `		}else{` |
|    1582 | 3277 | `			pSrc = (ph7_hashmap *)apArg[i]->x.pOther;` |
|       - | 3278 | `			/* Merge the two hashmaps */` |
|    1582 | 3279 | `			HashmapMerge(pSrc,pMap);` |
|       - | 3280 | `		}` |
|     794 | 3281 | `	}` |
|       - | 3282 | `	/* Return the freshly created array */` |
|     794 | 3283 | `	ph7_result_value(pCtx,pArray);` |
|     794 | 3284 | `	return PH7_OK;` |
|     398 | 3285 |  |
|       - | 3286 | `/*` |
|       - | 3287 | ` * array array_copy(array $source)` |
|       - | 3288 | ` *  Make a blind copy of the target array.` |
|       - | 3289 | ` * Parameters` |
|       - | 3290 | ` *  $source` |
|       - | 3291 | ` *   Target array` |
|       - | 3292 | ` * Return` |
|       - | 3293 | ` *  Copy of the target array on success.NULL otherwise.` |
|       - | 3294 | ` * Note` |
|       - | 3295 | ` *  This function is a symisc eXtension.` |
|       - | 3296 | ` */` |
|       2 | 3297 | `static int ph7_hashmap_copy(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       1 | 3298 |  |
|       - | 3299 | `	ph7_hashmap *pMap;` |
|       - | 3300 | `	ph7_value *pArray;` |
|       3 | 3301 | `	if( nArg < 1 ){` |
|       - | 3302 | `		/* Missing arguments,return NULL */` |
|     ! 0 | 3303 | `		ph7_result_null(pCtx);` |
|     ! 0 | 3304 | `		return PH7_OK;` |
|       - | 3305 | `	}` |
|       - | 3306 | `	/* Create a new array */` |
|       3 | 3307 | `	pArray = ph7_context_new_array(pCtx);` |
|       3 | 3308 | `	if( pArray == 0 ){` |
|     ! 0 | 3309 | `		ph7_result_null(pCtx);` |
|     ! 0 | 3310 | `		return PH7_OK;` |
|       - | 3311 | `	}` |
|       - | 3312 | `	/* Point to the internal representation of the hashmap */` |
|       3 | 3313 | `	pMap = (ph7_hashmap *)pArray->x.pOther;` |
|       3 | 3314 | `	if( ph7_value_is_array(apArg[0])){` |
|       - | 3315 | `		/* Point to the internal representation of the source */` |
|       3 | 3316 | `		ph7_hashmap *pSrc = (ph7_hashmap *)apArg[0]->x.pOther;` |
|       - | 3317 | `		/* Perform the copy */` |
|       3 | 3318 | `		PH7_HashmapDup(pSrc,pMap);` |
|       2 | 3319 | `	}else{` |
|       - | 3320 | `		/* Simple insertion */` |
|     ! 0 | 3321 | `		PH7_HashmapInsert(pMap,0/* Automatic index assign*/,apArg[0]);` |
|       - | 3322 | `	}` |
|       - | 3323 | `	/* Return the duplicated array */` |
|       3 | 3324 | `	ph7_result_value(pCtx,pArray);` |
|       3 | 3325 | `	return PH7_OK;` |
|       2 | 3326 |  |
|       - | 3327 | `/*` |
|       - | 3328 | ` * bool array_erase(array $source)` |
|       - | 3329 | ` *  Remove all elements from a given array.` |
|       - | 3330 | ` * Parameters` |
|       - | 3331 | ` *  $source` |
|       - | 3332 | ` *   Target array` |
|       - | 3333 | ` * Return` |
|       - | 3334 | ` *  TRUE on success.FALSE otherwise.` |
|       - | 3335 | ` * Note` |
|       - | 3336 | ` *  This function is a symisc eXtension.` |
|       - | 3337 | ` */` |
|       2 | 3338 | `static int ph7_hashmap_erase(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       1 | 3339 |  |
|       - | 3340 | `	ph7_hashmap *pMap;` |
|       3 | 3341 | `	if( nArg < 1 ){` |
|       - | 3342 | `		/* Missing arguments */` |
|     ! 0 | 3343 | `		ph7_result_bool(pCtx,0);` |
|     ! 0 | 3344 | `		return PH7_OK;` |
|       - | 3345 | `	}` |
|       - | 3346 | `	/* Point to the target hashmap */` |
|       3 | 3347 | `	pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|       - | 3348 | `	/* Erase */` |
|       3 | 3349 | `	PH7_HashmapRelease(pMap,FALSE);` |
|       3 | 3350 | `	return PH7_OK;` |
|       2 | 3351 |  |
|       - | 3352 | `/*` |
|       - | 3353 | ` * array array_slice(array $array,int $offset [,int $length [, bool $preserve_keys = false ]])` |
|       - | 3354 | ` *  Extract a slice of the array.` |
|       - | 3355 | ` * Parameters` |
|       - | 3356 | ` *  $array` |
|       - | 3357 | ` *    The input array.` |
|       - | 3358 | ` * $offset` |
|       - | 3359 | ` *    If offset is non-negative, the sequence will start at that offset in the array.` |
|       - | 3360 | ` *    If offset is negative, the sequence will start that far from the end of the array.` |
|       - | 3361 | ` * $length (optional)` |
|       - | 3362 | ` *    If length is given and is positive, then the sequence will have that many elements` |
|       - | 3363 | ` *    in it. If length is given and is negative then the sequence will stop that many` |
|       - | 3364 | ` *   elements from the end of the array. If it is omitted, then the sequence will have` |
|       - | 3365 | ` *   everything from offset up until the end of the array.` |
|       - | 3366 | ` * $preserve_keys (optional)` |
|       - | 3367 | ` *    Note that array_slice() will reorder and reset the array indices by default.` |
|       - | 3368 | ` *    You can change this behaviour by setting preserve_keys to TRUE.` |
|       - | 3369 | ` * Return` |
|       - | 3370 | ` *   The new slice.` |
|       - | 3371 | ` */` |
|       8 | 3372 | `static int ph7_hashmap_slice(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       1 | 3373 |  |
|       - | 3374 | `	ph7_hashmap *pMap,*pSrc;` |
|       - | 3375 | `	ph7_hashmap_node *pCur;` |
|       - | 3376 | `	ph7_value *pArray;` |
|       - | 3377 | `	int iLength,iOfft;` |
|       - | 3378 | `	int bPreserve;` |
|       - | 3379 | `	sxi32 rc;` |
|       9 | 3380 | `	if( nArg < 2 \|\| !ph7_value_is_array(apArg[0]) ){` |
|       - | 3381 | `		/* Missing/Invalid arguments,return NULL */` |
|     ! 0 | 3382 | `		ph7_result_null(pCtx);` |
|     ! 0 | 3383 | `		return PH7_OK;` |
|       - | 3384 | `	}` |
|       - | 3385 | `	/* Point the internal representation of the target array */` |
|       9 | 3386 | `	pSrc = (ph7_hashmap *)apArg[0]->x.pOther;` |
|       9 | 3387 | `	bPreserve = FALSE;` |
|       - | 3388 | `	/* Get the offset */` |
|       9 | 3389 | `	iOfft = ph7_value_to_int(apArg[1]);` |
|       9 | 3390 | `	if( iOfft < 0 ){` |
|       3 | 3391 | `		iOfft = (int)pSrc->nEntry + iOfft;` |
|       1 | 3392 | `	}` |
|       9 | 3393 | `	if( iOfft < 0 \|\| iOfft > (int)pSrc->nEntry ){` |
|       - | 3394 | `		/* Invalid offset,return the last entry */` |
|     ! 0 | 3395 | `		iOfft = (int)pSrc->nEntry - 1;` |
|     ! 0 | 3396 | `	}` |
|       - | 3397 | `	/* Get the length */` |
|       9 | 3398 | `	iLength = (int)pSrc->nEntry - iOfft;` |
|       9 | 3399 | `	if( nArg > 2 ){` |
|       7 | 3400 | `		iLength = ph7_value_to_int(apArg[2]);` |
|       7 | 3401 | `		if( iLength < 0 ){` |
|     ! 0 | 3402 | `			iLength = ((int)pSrc->nEntry + iLength) - iOfft;` |
|     ! 0 | 3403 | `		}` |
|       7 | 3404 | `		if( iLength < 0 \|\| iOfft + iLength >= (int)pSrc->nEntry ){` |
|     ! 0 | 3405 | `			iLength = (int)pSrc->nEntry - iOfft;` |
|     ! 0 | 3406 | `		}` |
|       7 | 3407 | `		if( nArg > 3 && ph7_value_is_bool(apArg[3]) ){` |
|       3 | 3408 | `			bPreserve = ph7_value_to_bool(apArg[3]);` |
|       1 | 3409 | `		}` |
|       3 | 3410 | `	}` |
|       - | 3411 | `	/* Create a new array */` |
|       9 | 3412 | `	pArray = ph7_context_new_array(pCtx);` |
|       9 | 3413 | `	if( pArray == 0 ){` |
|     ! 0 | 3414 | `		ph7_result_null(pCtx);` |
|     ! 0 | 3415 | `		return PH7_OK;` |
|       - | 3416 | `	}` |
|       9 | 3417 | `	if( iLength < 1 ){` |
|       - | 3418 | `		/* Don't bother processing,return the empty array */` |
|     ! 0 | 3419 | `		ph7_result_value(pCtx,pArray);` |
|     ! 0 | 3420 | `		return PH7_OK;` |
|       - | 3421 | `	}` |
|       - | 3422 | `	/* Point to the desired entry */` |
|       9 | 3423 | `	pCur = pSrc->pFirst;` |
|       9 | 3424 | `	for(;;){` |
|      19 | 3425 | `		if( iOfft < 1 ){` |
|       9 | 3426 | `			break;` |
|       - | 3427 | `		}` |
|       - | 3428 | `		/* Point to the next entry */` |
|      11 | 3429 | `		pCur = pCur->pPrev; /* Reverse link */` |
|      11 | 3430 | `		iOfft--;` |
|       1 | 3431 | `	}` |
|       - | 3432 | `	/* Point to the internal representation of the hashmap */` |
|       9 | 3433 | `	pMap = (ph7_hashmap *)pArray->x.pOther;` |
|      12 | 3434 | `	for(;;){` |
|      25 | 3435 | `		if( iLength < 1 ){` |
|       9 | 3436 | `			break;` |
|       - | 3437 | `		}` |
|      17 | 3438 | `		rc = HashmapInsertNode(pMap,pCur,bPreserve);` |
|      17 | 3439 | `		if( rc != SXRET_OK ){` |
|     ! 0 | 3440 | `			break;` |
|       - | 3441 | `		}` |
|       - | 3442 | `		/* Point to the next entry */` |
|      17 | 3443 | `		pCur = pCur->pPrev; /* Reverse link */` |
|      17 | 3444 | `		iLength--;` |
|       1 | 3445 | `	}` |
|       - | 3446 | `	/* Return the freshly created array */` |
|       9 | 3447 | `	ph7_result_value(pCtx,pArray);` |
|       9 | 3448 | `	return PH7_OK;` |
|       5 | 3449 |  |
|       - | 3450 | `/*` |
|       - | 3451 | ` * array array_splice(array $array,int $offset [,int $length [,value $replacement ]])` |
|       - | 3452 | ` *  Remove a portion of the array and replace it with something else.` |
|       - | 3453 | ` * Parameters` |
|       - | 3454 | ` *  $array` |
|       - | 3455 | ` *    The input array.` |
|       - | 3456 | ` * $offset` |
|       - | 3457 | ` *    If offset is positive then the start of removed portion is at that offset from` |
|       - | 3458 | ` *    the beginning of the input array. If offset is negative then it starts that far` |
|       - | 3459 | ` *    from the end of the input array.` |
|       - | 3460 | ` * $length (optional)` |
|       - | 3461 | ` *    If length is omitted, removes everything from offset to the end of the array.` |
|       - | 3462 | ` *    If length is specified and is positive, then that many elements will be removed.` |
|       - | 3463 | ` *    If length is specified and is negative then the end of the removed portion will` |
|       - | 3464 | ` *    be that many elements from the end of the array.` |
|       - | 3465 | ` * $replacement (optional)` |
|       - | 3466 | ` *  If replacement array is specified, then the removed elements are replaced` |
|       - | 3467 | ` *  with elements from this array.` |
|       - | 3468 | ` *  If offset and length are such that nothing is removed, then the elements` |
|       - | 3469 | ` *  from the replacement array are inserted in the place specified by the offset.` |
|       - | 3470 | ` *  Note that keys in replacement array are not preserved.` |
|       - | 3471 | ` *  If replacement is just one element it is not necessary to put array() around` |
|       - | 3472 | ` *  it, unless the element is an array itself, an object or NULL.` |
|       - | 3473 | ` * Return` |
|       - | 3474 | ` *   A new array consisting of the extracted elements.` |
|       - | 3475 | ` */` |
|       2 | 3476 | `static int ph7_hashmap_splice(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       1 | 3477 |  |
|       - | 3478 | `	ph7_hashmap_node *pCur,*pPrev,*pRnode;` |
|       - | 3479 | `	ph7_value *pArray,*pRvalue,*pOld;` |
|       - | 3480 | `	ph7_hashmap *pMap,*pSrc,*pRep;` |
|       - | 3481 | `	int iLength,iOfft;` |
|       - | 3482 | `	sxi32 rc;` |
|       3 | 3483 | `	if( nArg < 2 \|\| !ph7_value_is_array(apArg[0]) ){` |
|       - | 3484 | `		/* Missing/Invalid arguments,return NULL */` |
|     ! 0 | 3485 | `		ph7_result_null(pCtx);` |
|     ! 0 | 3486 | `		return PH7_OK;` |
|       - | 3487 | `	}` |
|       - | 3488 | `	/* Point the internal representation of the target array */` |
|       3 | 3489 | `	pSrc = (ph7_hashmap *)apArg[0]->x.pOther;` |
|       - | 3490 | `	/* Get the offset */` |
|       3 | 3491 | `	iOfft = ph7_value_to_int(apArg[1]);` |
|       3 | 3492 | `	if( iOfft < 0 ){` |
|     ! 0 | 3493 | `		iOfft = (int)pSrc->nEntry + iOfft;` |
|     ! 0 | 3494 | `	}` |
|       3 | 3495 | `	if( iOfft < 0 \|\| iOfft > (int)pSrc->nEntry ){` |
|       - | 3496 | `		/* Invalid offset,remove the last entry */` |
|     ! 0 | 3497 | `		iOfft = (int)pSrc->nEntry - 1;` |
|     ! 0 | 3498 | `	}` |
|       - | 3499 | `	/* Get the length */` |
|       3 | 3500 | `	iLength = (int)pSrc->nEntry - iOfft;` |
|       3 | 3501 | `	if( nArg > 2 ){` |
|       3 | 3502 | `		iLength = ph7_value_to_int(apArg[2]);` |
|       3 | 3503 | `		if( iLength < 0 ){` |
|     ! 0 | 3504 | `			iLength = ((int)pSrc->nEntry + iLength) - iOfft;` |
|     ! 0 | 3505 | `		}` |
|       3 | 3506 | `		if( iLength < 0 \|\| iOfft + iLength >= (int)pSrc->nEntry ){` |
|     ! 0 | 3507 | `			iLength = (int)pSrc->nEntry - iOfft;` |
|     ! 0 | 3508 | `		}` |
|       1 | 3509 | `	}` |
|       - | 3510 | `	/* Create a new array */` |
|       3 | 3511 | `	pArray = ph7_context_new_array(pCtx);` |
|       3 | 3512 | `	if( pArray == 0 ){` |
|     ! 0 | 3513 | `		ph7_result_null(pCtx);` |
|     ! 0 | 3514 | `		return PH7_OK;` |
|       - | 3515 | `	}` |
|       3 | 3516 | `	if( iLength < 1 ){` |
|       - | 3517 | `		/* Don't bother processing,return the empty array */` |
|     ! 0 | 3518 | `		ph7_result_value(pCtx,pArray);` |
|     ! 0 | 3519 | `		return PH7_OK;` |
|       - | 3520 | `	}` |
|       - | 3521 | `	/* Point to the desired entry */` |
|       3 | 3522 | `	pCur = pSrc->pFirst;` |
|       2 | 3523 | `	for(;;){` |
|       5 | 3524 | `		if( iOfft < 1 ){` |
|       3 | 3525 | `			break;` |
|       - | 3526 | `		}` |
|       - | 3527 | `		/* Point to the next entry */` |
|       3 | 3528 | `		pCur = pCur->pPrev; /* Reverse link */` |
|       3 | 3529 | `		iOfft--;` |
|       1 | 3530 | `	}` |
|       3 | 3531 | `	pRep = 0;` |
|       3 | 3532 | `	if( nArg > 3 ){` |
|       3 | 3533 | `		if( !ph7_value_is_array(apArg[3]) ){` |
|       - | 3534 | `			/* Perform an array cast */` |
|     ! 0 | 3535 | `			PH7_MemObjToHashmap(apArg[3]);` |
|     ! 0 | 3536 | `			if(ph7_value_is_array(apArg[3])){` |
|     ! 0 | 3537 | `				pRep = (ph7_hashmap *)apArg[3]->x.pOther;` |
|     ! 0 | 3538 | `			}` |
|     ! 0 | 3539 | `		}else{` |
|       3 | 3540 | `			pRep = (ph7_hashmap *)apArg[3]->x.pOther;` |
|       - | 3541 | `		}` |
|       3 | 3542 | `		if( pRep ){` |
|       - | 3543 | `			/* Reset the loop cursor */` |
|       3 | 3544 | `			pRep->pCur = pRep->pFirst;` |
|       1 | 3545 | `		}` |
|       1 | 3546 | `	}` |
|       - | 3547 | `	/* Point to the internal representation of the hashmap */` |
|       3 | 3548 | `	pMap = (ph7_hashmap *)pArray->x.pOther;` |
|       3 | 3549 | `	for(;;){` |
|       7 | 3550 | `		if( iLength < 1 ){` |
|       3 | 3551 | `			break;` |
|       - | 3552 | `		}` |
|       5 | 3553 | `		pPrev = pCur->pPrev;` |
|       5 | 3554 | `		rc = HashmapInsertNode(pMap,pCur,FALSE);` |
|       5 | 3555 | `		if( pRep && (pRnode = PH7_HashmapGetNextEntry(pRep)) != 0 ){` |
|       - | 3556 | `			/* Extract node value */` |
|       5 | 3557 | `			pRvalue = HashmapExtractNodeValue(pRnode);` |
|       - | 3558 | `			/* Replace the old node */` |
|       5 | 3559 | `			pOld = HashmapExtractNodeValue(pCur);` |
|       5 | 3560 | `			if( pRvalue && pOld ){` |
|       5 | 3561 | `				PH7_MemObjStore(pRvalue,pOld);` |
|       2 | 3562 | `			}` |
|       3 | 3563 | `		}else{` |
|       - | 3564 | `			/* Unlink the node from the source hashmap */` |
|     ! 0 | 3565 | `			PH7_HashmapUnlinkNode(pCur,TRUE);` |
|       - | 3566 | `		}` |
|       5 | 3567 | `		if( rc != SXRET_OK ){` |
|     ! 0 | 3568 | `			break;` |
|       - | 3569 | `		}` |
|       - | 3570 | `		/* Point to the next entry */` |
|       5 | 3571 | `		pCur = pPrev; /* Reverse link */` |
|       5 | 3572 | `		iLength--;` |
|       1 | 3573 | `	}` |
|       3 | 3574 | `	if( pRep ){` |
|       3 | 3575 | `		while((pRnode = PH7_HashmapGetNextEntry(pRep)) != 0 ){` |
|     ! 0 | 3576 | `			HashmapInsertNode(pSrc,pRnode,FALSE);` |
|     ! 0 | 3577 | `		}` |
|       1 | 3578 | `	}` |
|       - | 3579 | `	/* Return the freshly created array */` |
|       3 | 3580 | `	ph7_result_value(pCtx,pArray);` |
|       3 | 3581 | `	return PH7_OK;` |
|       2 | 3582 |  |
|       - | 3583 | `/*` |
|       - | 3584 | ` * bool in_array(value $needle,array $haystack[,bool $strict = FALSE ])` |
|       - | 3585 | ` *  Checks if a value exists in an array.` |
|       - | 3586 | ` * Parameters` |
|       - | 3587 | ` *  $needle` |
|       - | 3588 | ` *   The searched value.` |
|       - | 3589 | ` *   Note:` |
|       - | 3590 | ` *    If needle is a string, the comparison is done in a case-sensitive manner.` |
|       - | 3591 | ` * $haystack` |
|       - | 3592 | ` *  The target array.` |
|       - | 3593 | ` * $strict` |
|       - | 3594 | ` *  If the third parameter strict is set to TRUE then the in_array() function` |
|       - | 3595 | ` *  will also check the types of the needle in the haystack.` |
|       - | 3596 | ` */` |
|   18496 | 3597 | `static int ph7_hashmap_in_array(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       2 | 3598 |  |
|       - | 3599 | `	ph7_value *pNeedle;` |
|       - | 3600 | `	int bStrict;` |
|       - | 3601 | `	int rc;` |
|   18498 | 3602 | `	if( nArg < 2 ){` |
|       - | 3603 | `		/* Missing argument,return FALSE */` |
|     ! 0 | 3604 | `		ph7_result_bool(pCtx,0);` |
|     ! 0 | 3605 | `		return PH7_OK;` |
|       - | 3606 | `	}` |
|   18498 | 3607 | `	pNeedle = apArg[0];` |
|   18498 | 3608 | `	bStrict = 0;` |
|   18498 | 3609 | `	if( nArg > 2 ){` |
|       5 | 3610 | `		bStrict = ph7_value_to_bool(apArg[2]);` |
|       2 | 3611 | `	}` |
|   18498 | 3612 | `	if( !ph7_value_is_array(apArg[1]) ){` |
|       - | 3613 | `		/* haystack must be an array,perform a standard comparison */` |
|     ! 0 | 3614 | `		rc = ph7_value_compare(pNeedle,apArg[1],bStrict);` |
|       - | 3615 | `		/* Set the comparison result */` |
|     ! 0 | 3616 | `		ph7_result_bool(pCtx,rc == 0);` |
|     ! 0 | 3617 | `		return PH7_OK;` |
|       - | 3618 | `	}` |
|       - | 3619 | `	/* Perform the lookup */` |
|   18498 | 3620 | `	rc = HashmapFindValue((ph7_hashmap *)apArg[1]->x.pOther,pNeedle,0,bStrict);` |
|       - | 3621 | `	/* Lookup result */` |
|   18498 | 3622 | `	ph7_result_bool(pCtx,rc == SXRET_OK);` |
|   18498 | 3623 | `	return PH7_OK;` |
|    9250 | 3624 |  |
|       - | 3625 | `/*` |
|       - | 3626 | ` * value array_search(value $needle,array $haystack[,bool $strict = false ])` |
|       - | 3627 | ` *  Searches the array for a given value and returns the corresponding key if successful.` |
|       - | 3628 | ` * Parameters` |
|       - | 3629 | ` * $needle` |
|       - | 3630 | ` *   The searched value.` |
|       - | 3631 | ` * $haystack` |
|       - | 3632 | ` *   The array.` |
|       - | 3633 | ` * $strict` |
|       - | 3634 | ` *  If the third parameter strict is set to TRUE then the array_search() function` |
|       - | 3635 | ` *  will search for identical elements in the haystack. This means it will also check` |
|       - | 3636 | ` *  the types of the needle in the haystack, and objects must be the same instance.` |
|       - | 3637 | ` * Return` |
|       - | 3638 | ` *  Returns the key for needle if it is found in the array, FALSE otherwise.` |
|       - | 3639 | ` */` |
|      26 | 3640 | `static int ph7_hashmap_search(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       1 | 3641 |  |
|       - | 3642 | `	ph7_hashmap_node *pEntry;` |
|       - | 3643 | `	ph7_value *pVal,sNeedle;` |
|       - | 3644 | `	ph7_hashmap *pMap;` |
|       - | 3645 | `	ph7_value sVal;` |
|       - | 3646 | `	int bStrict;` |
|       - | 3647 | `	sxu32 n;` |
|       - | 3648 | `	int rc;` |
|      27 | 3649 | `	if( nArg < 2 ){` |
|       - | 3650 | `		/* Missing argument,return FALSE*/` |
|     ! 0 | 3651 | `		ph7_result_bool(pCtx,0);` |
|     ! 0 | 3652 | `		return PH7_OK;` |
|       - | 3653 | `	}` |
|      27 | 3654 | `	bStrict = FALSE;` |
|      27 | 3655 | `	if( !ph7_value_is_array(apArg[1]) ){` |
|       - | 3656 | `		/* hasystack must be an array,return FALSE */` |
|       3 | 3657 | `		ph7_result_bool(pCtx,0);` |
|       3 | 3658 | `		return PH7_OK;` |
|       - | 3659 | `	}` |
|      25 | 3660 | `	if( nArg > 2 && ph7_value_is_bool(apArg[2]) ){` |
|      19 | 3661 | `		bStrict = ph7_value_to_bool(apArg[2]);` |
|       9 | 3662 | `	}` |
|       - | 3663 | `	/* Point to the internal representation of the internal hashmap */` |
|      25 | 3664 | `	pMap = (ph7_hashmap *)apArg[1]->x.pOther;` |
|       - | 3665 | `	/* Perform a linear search since we cannot sort the hashmap based on values */` |
|      25 | 3666 | `	PH7_MemObjInit(pMap->pVm,&sVal);` |
|      25 | 3667 | `	PH7_MemObjInit(pMap->pVm,&sNeedle);` |
|      25 | 3668 | `	pEntry = pMap->pFirst;` |
|      25 | 3669 | `	n = pMap->nEntry;` |
|      39 | 3670 | `	for(;;){` |
|      79 | 3671 | `		if( !n ){` |
|       7 | 3672 | `			break;` |
|       - | 3673 | `		}` |
|       - | 3674 | `		/* Extract node value */` |
|      73 | 3675 | `		pVal = HashmapExtractNodeValue(pEntry);` |
|      73 | 3676 | `		if( pVal ){` |
|       - | 3677 | `			/* Make a copy of the vuurent values since the comparison routine` |
|       - | 3678 | `			 * can change their type.` |
|       - | 3679 | `			 */` |
|      73 | 3680 | `			PH7_MemObjLoad(pVal,&sVal);` |
|      73 | 3681 | `			PH7_MemObjLoad(apArg[0],&sNeedle);` |
|      73 | 3682 | `			rc = PH7_MemObjCmp(&sNeedle,&sVal,bStrict,0);` |
|      73 | 3683 | `			PH7_MemObjRelease(&sVal);` |
|      73 | 3684 | `			PH7_MemObjRelease(&sNeedle);` |
|      73 | 3685 | `			if( rc == 0 ){` |
|       - | 3686 | `				/* Match found,return key */` |
|      19 | 3687 | `				if( pEntry->iType == HASHMAP_INT_NODE){` |
|       - | 3688 | `					/* INT key */` |
|      13 | 3689 | `					ph7_result_int64(pCtx,pEntry->xKey.iKey);` |
|       7 | 3690 | `				}else{` |
|       7 | 3691 | `					SyBlob *pKey = &pEntry->xKey.sKey;` |
|       - | 3692 | `					/* Blob key */` |
|       7 | 3693 | `					ph7_result_string(pCtx,(const char *)SyBlobData(pKey),(int)SyBlobLength(pKey));` |
|       - | 3694 | `				}` |
|      19 | 3695 | `				return PH7_OK;` |
|       - | 3696 | `			}` |
|      27 | 3697 | `		}` |
|       - | 3698 | `		/* Point to the next entry */` |
|      55 | 3699 | `		pEntry = pEntry->pPrev; /* Reverse link */` |
|      55 | 3700 | `		n--;` |
|       1 | 3701 | `	}` |
|       - | 3702 | `	/* No such value,return FALSE */` |
|       7 | 3703 | `	ph7_result_bool(pCtx,0);` |
|       7 | 3704 | `	return PH7_OK;` |
|      14 | 3705 |  |
|       - | 3706 | `/*` |
|       - | 3707 | ` * array array_diff(array $array1,array $array2,...)` |
|       - | 3708 | ` *  Computes the difference of arrays.` |
|       - | 3709 | ` * Parameters` |
|       - | 3710 | ` *  $array1` |
|       - | 3711 | ` *    The array to compare from` |
|       - | 3712 | ` *  $array2` |
|       - | 3713 | ` *    An array to compare against` |
|       - | 3714 | ` *  $...` |
|       - | 3715 | ` *   More arrays to compare against` |
|       - | 3716 | ` * Return` |
|       - | 3717 | ` *  Returns an array containing all the entries from array1 that` |
|       - | 3718 | ` *  are not present in any of the other arrays.` |
|       - | 3719 | ` */` |
|      10 | 3720 | `static int ph7_hashmap_diff(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       2 | 3721 |  |
|       - | 3722 | `	ph7_hashmap_node *pEntry;` |
|       - | 3723 | `	ph7_hashmap *pSrc,*pMap;` |
|       - | 3724 | `	ph7_value *pArray;` |
|       - | 3725 | `	ph7_value *pVal;` |
|       - | 3726 | `	sxi32 rc;` |
|       - | 3727 | `	sxu32 n;` |
|       - | 3728 | `	int i;` |
|       - | 3729 | `	/* Validate arguments to mimic PHP behaviour. Earlier versions simply` |
|       - | 3730 | `	 * returned NULL when the caller passed invalid parameters which made` |
|       - | 3731 | `	 * debugging difficult. */` |
|      12 | 3732 | `	if( nArg < 1 ){` |
|       4 | 3733 | `		return PH7_VmThrowException(pCtx,` |
|       - | 3734 | `			"ArgumentCountError",` |
|       - | 3735 | `			"array_diff() expects at least 1 argument, %d given",` |
|       1 | 3736 | `			nArg` |
|       - | 3737 | `			);` |
|       - | 3738 | `	}` |
|      10 | 3739 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|       4 | 3740 | `		return PH7_VmThrowException(pCtx,` |
|       - | 3741 | `			"TypeError",` |
|       - | 3742 | `			"array_diff(): Argument #1 ($array) must be of type array, %s given",` |
|       1 | 3743 | `			ph7_type_name(apArg[0])` |
|       - | 3744 | `			);` |
|       - | 3745 | `	}` |
|      14 | 3746 | `	for(i = 1 ; i < nArg ; i++){` |
|      10 | 3747 | `		if( !ph7_value_is_array(apArg[i]) ){` |
|       4 | 3748 | `			return PH7_VmThrowException(pCtx,` |
|       - | 3749 | `				"TypeError",` |
|       - | 3750 | `				"array_diff(): Argument #%d must be of type array, %s given",` |
|       1 | 3751 | `				i + 1,` |
|       2 | 3752 | `				ph7_type_name(apArg[i])` |
|       - | 3753 | `				);` |
|       - | 3754 | `		}` |
|       4 | 3755 | `	}` |
|       5 | 3756 | `	if( nArg == 1 ){` |
|       - | 3757 | `		/* Return the first array since we cannot perform a diff */` |
|     ! 0 | 3758 | `		ph7_result_value(pCtx,apArg[0]);` |
|     ! 0 | 3759 | `		return PH7_OK;` |
|       - | 3760 | `	}` |
|       - | 3761 | `	/* Create a new array */` |
|       5 | 3762 | `	pArray = ph7_context_new_array(pCtx);` |
|       5 | 3763 | `	if( pArray == 0 ){` |
|     ! 0 | 3764 | `		ph7_result_null(pCtx);` |
|     ! 0 | 3765 | `		return PH7_OK;` |
|       - | 3766 | `	}` |
|       - | 3767 | `	/* Point to the internal representation of the source hashmap */` |
|       5 | 3768 | `	pSrc = (ph7_hashmap *)apArg[0]->x.pOther;` |
|       - | 3769 | `	/* Perform the diff */` |
|       5 | 3770 | `	pEntry = pSrc->pFirst;` |
|       5 | 3771 | `	n = pSrc->nEntry;` |
|       8 | 3772 | `	for(;;){` |
|      17 | 3773 | `		if( n < 1 ){` |
|       5 | 3774 | `			break;` |
|       - | 3775 | `		}` |
|       - | 3776 | `		/* Extract the node value */` |
|      13 | 3777 | `		pVal = HashmapExtractNodeValue(pEntry);` |
|      13 | 3778 | `		if( pVal ){` |
|      23 | 3779 | `			for( i = 1 ; i < nArg ; i++ ){` |
|      17 | 3780 | `				if( !ph7_value_is_array(apArg[i])) {` |
|       - | 3781 | `					/* ignore */` |
|     ! 0 | 3782 | `					continue;` |
|       - | 3783 | `				}` |
|       - | 3784 | `				/* Point to the internal representation of the hashmap */` |
|      17 | 3785 | `				pMap = (ph7_hashmap *)apArg[i]->x.pOther;` |
|       - | 3786 | `				/* Perform the lookup */` |
|      17 | 3787 | `				rc = HashmapFindValue(pMap,pVal,0,TRUE);` |
|      17 | 3788 | `				if( rc == SXRET_OK ){` |
|       - | 3789 | `					/* Value exist */` |
|       7 | 3790 | `					break;` |
|       - | 3791 | `				}` |
|       6 | 3792 | `			}` |
|      13 | 3793 | `			if( i >= nArg ){` |
|       - | 3794 | `				/* Perform the insertion */` |
|       7 | 3795 | `				HashmapInsertNode((ph7_hashmap *)pArray->x.pOther,pEntry,TRUE);` |
|       3 | 3796 | `			}` |
|       6 | 3797 | `		}` |
|       - | 3798 | `		/* Point to the next entry */` |
|      13 | 3799 | `		pEntry = pEntry->pPrev; /* Reverse link */` |
|      13 | 3800 | `		n--;` |
|       1 | 3801 | `	}` |
|       - | 3802 | `	/* Return the freshly created array */` |
|       5 | 3803 | `	ph7_result_value(pCtx,pArray);` |
|       5 | 3804 | `	return PH7_OK;` |
|       7 | 3805 |  |
|       - | 3806 | `/*` |
|       - | 3807 | ` * array array_udiff(array $array1,array $array2,...,$callback)` |
|       - | 3808 | ` *  Computes the difference of arrays by using a callback function for data comparison.` |
|       - | 3809 | ` * Parameters` |
|       - | 3810 | ` *  $array1` |
|       - | 3811 | ` *    The array to compare from` |
|       - | 3812 | ` *  $array2` |
|       - | 3813 | ` *    An array to compare against` |
|       - | 3814 | ` *  $...` |
|       - | 3815 | ` *   More arrays to compare against.` |
|       - | 3816 | ` * $callback` |
|       - | 3817 | ` *  The callback comparison function.` |
|       - | 3818 | ` *  The comparison function must return an integer less than, equal to, or greater than zero` |
|       - | 3819 | ` *  if the first argument is considered to be respectively less than, equal to, or greater` |
|       - | 3820 | ` *  than the second.` |
|       - | 3821 | ` *     int callback ( mixed $a, mixed $b )` |
|       - | 3822 | ` * Return` |
|       - | 3823 | ` *  Returns an array containing all the entries from array1 that` |
|       - | 3824 | ` *  are not present in any of the other arrays.` |
|       - | 3825 | ` */` |
|       2 | 3826 | `static int ph7_hashmap_udiff(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       1 | 3827 |  |
|       - | 3828 | `	ph7_hashmap_node *pEntry;` |
|       - | 3829 | `	ph7_hashmap *pSrc,*pMap;` |
|       - | 3830 | `	ph7_value *pCallback;` |
|       - | 3831 | `	ph7_value *pArray;` |
|       - | 3832 | `	ph7_value *pVal;` |
|       - | 3833 | `	sxi32 rc;` |
|       - | 3834 | `	sxu32 n;` |
|       - | 3835 | `	int i;` |
|       3 | 3836 | `	if( nArg < 2 \|\| !ph7_value_is_array(apArg[0]) ){` |
|       - | 3837 | `		/* Missing/Invalid arguments,return NULL */` |
|     ! 0 | 3838 | `		ph7_result_null(pCtx);` |
|     ! 0 | 3839 | `		return PH7_OK;` |
|       - | 3840 | `	}` |
|       - | 3841 | `	/* Point to the callback */` |
|       3 | 3842 | `	pCallback = apArg[nArg - 1];` |
|       3 | 3843 | `	if( nArg == 2 ){` |
|       - | 3844 | `		/* Return the first array since we cannot perform a diff */` |
|     ! 0 | 3845 | `		ph7_result_value(pCtx,apArg[0]);` |
|     ! 0 | 3846 | `		return PH7_OK;` |
|       - | 3847 | `	}` |
|       - | 3848 | `	/* Create a new array */` |
|       3 | 3849 | `	pArray = ph7_context_new_array(pCtx);` |
|       3 | 3850 | `	if( pArray == 0 ){` |
|     ! 0 | 3851 | `		ph7_result_null(pCtx);` |
|     ! 0 | 3852 | `		return PH7_OK;` |
|       - | 3853 | `	}` |
|       - | 3854 | `	/* Point to the internal representation of the source hashmap */` |
|       3 | 3855 | `	pSrc = (ph7_hashmap *)apArg[0]->x.pOther;` |
|       - | 3856 | `	/* Perform the diff */` |
|       3 | 3857 | `	pEntry = pSrc->pFirst;` |
|       3 | 3858 | `	n = pSrc->nEntry;` |
|       4 | 3859 | `	for(;;){` |
|       9 | 3860 | `		if( n < 1 ){` |
|       3 | 3861 | `			break;` |
|       - | 3862 | `		}` |
|       - | 3863 | `		/* Extract the node value */` |
|       7 | 3864 | `		pVal = HashmapExtractNodeValue(pEntry);` |
|       7 | 3865 | `		if( pVal ){` |
|      11 | 3866 | `			for( i = 1 ; i < nArg - 1; i++ ){` |
|       7 | 3867 | `				if( !ph7_value_is_array(apArg[i])) {` |
|       - | 3868 | `					/* ignore */` |
|     ! 0 | 3869 | `					continue;` |
|       - | 3870 | `				}` |
|       - | 3871 | `				/* Point to the internal representation of the hashmap */` |
|       7 | 3872 | `				pMap = (ph7_hashmap *)apArg[i]->x.pOther;` |
|       - | 3873 | `				/* Perform the lookup */` |
|       7 | 3874 | `				rc = HashmapFindValueByCallback(pMap,pVal,pCallback,0);` |
|       7 | 3875 | `				if( rc == SXRET_OK ){` |
|       - | 3876 | `					/* Value exist */` |
|       3 | 3877 | `					break;` |
|       - | 3878 | `				}` |
|       3 | 3879 | `			}` |
|       7 | 3880 | `			if( i >= (nArg - 1)){` |
|       - | 3881 | `				/* Perform the insertion */` |
|       5 | 3882 | `				HashmapInsertNode((ph7_hashmap *)pArray->x.pOther,pEntry,TRUE);` |
|       2 | 3883 | `			}` |
|       3 | 3884 | `		}` |
|       - | 3885 | `		/* Point to the next entry */` |
|       7 | 3886 | `		pEntry = pEntry->pPrev; /* Reverse link */` |
|       7 | 3887 | `		n--;` |
|       1 | 3888 | `	}` |
|       - | 3889 | `	/* Return the freshly created array */` |
|       3 | 3890 | `	ph7_result_value(pCtx,pArray);` |
|       3 | 3891 | `	return PH7_OK;` |
|       2 | 3892 |  |
|       - | 3893 | `/*` |
|       - | 3894 | ` * array array_diff_assoc(array $array1,array $array2,...)` |
|       - | 3895 | ` *  Computes the difference of arrays with additional index check.` |
|       - | 3896 | ` * Parameters` |
|       - | 3897 | ` *  $array1` |
|       - | 3898 | ` *    The array to compare from` |
|       - | 3899 | ` *  $array2` |
|       - | 3900 | ` *    An array to compare against` |
|       - | 3901 | ` *  $...` |
|       - | 3902 | ` *   More arrays to compare against` |
|       - | 3903 | ` * Return` |
|       - | 3904 | ` *  Returns an array containing all the entries from array1 that` |
|       - | 3905 | ` *  are not present in any of the other arrays.` |
|       - | 3906 | ` */` |
|      20 | 3907 | `static int ph7_hashmap_diff_assoc(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       2 | 3908 |  |
|       - | 3909 | `	ph7_hashmap_node *pN1,*pN2,*pEntry;` |
|       - | 3910 | `	ph7_hashmap *pSrc,*pMap;` |
|       - | 3911 | `	ph7_value *pArray;` |
|       - | 3912 | `	ph7_value *pVal;` |
|       - | 3913 | `	sxi32 rc;` |
|       - | 3914 | `	sxu32 n;` |
|       - | 3915 | `	int i;` |
|       - | 3916 | `	/* Ensure the argument list is valid, emitting the same errors PHP` |
|       - | 3917 | `	 * would produce. This makes behaviour predictable and allows the` |
|       - | 3918 | `	 * accompanying integration tests to pass. */` |
|      22 | 3919 | `	if( nArg < 1 ){` |
|       4 | 3920 | `		return PH7_VmThrowException(pCtx,` |
|       - | 3921 | `			"ArgumentCountError",` |
|       - | 3922 | `			"array_diff_assoc() expects at least 1 argument, %d given",` |
|       1 | 3923 | `			nArg` |
|       - | 3924 | `			);` |
|       - | 3925 | `	}` |
|      20 | 3926 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|       4 | 3927 | `		return PH7_VmThrowException(pCtx,` |
|       - | 3928 | `			"TypeError",` |
|       - | 3929 | `			"array_diff_assoc(): Argument #1 ($array) must be of type array, %s given",` |
|       1 | 3930 | `			ph7_type_name(apArg[0])` |
|       - | 3931 | `			);` |
|       - | 3932 | `	}` |
|      32 | 3933 | `	for(i = 1 ; i < nArg ; i++){` |
|      20 | 3934 | `		if( !ph7_value_is_array(apArg[i]) ){` |
|       7 | 3935 | `			return PH7_VmThrowException(pCtx,` |
|       - | 3936 | `				"TypeError",` |
|       - | 3937 | `				"array_diff_assoc(): Argument #%d must be of type array, %s given",` |
|       2 | 3938 | `				i + 1,` |
|       4 | 3939 | `				ph7_type_name(apArg[i])` |
|       - | 3940 | `				);` |
|       - | 3941 | `		}` |
|       9 | 3942 | `	}` |
|      13 | 3943 | `	if( nArg == 1 ){` |
|       - | 3944 | `		/* Return the first array since we cannot perform a diff */` |
|       3 | 3945 | `		ph7_result_value(pCtx,apArg[0]);` |
|       3 | 3946 | `		return PH7_OK;` |
|       - | 3947 | `	}` |
|       - | 3948 | `	/* Create a new array */` |
|      11 | 3949 | `	pArray = ph7_context_new_array(pCtx);` |
|      11 | 3950 | `	if( pArray == 0 ){` |
|     ! 0 | 3951 | `		ph7_result_null(pCtx);` |
|     ! 0 | 3952 | `		return PH7_OK;` |
|       - | 3953 | `	}` |
|       - | 3954 | `	/* Point to the internal representation of the source hashmap */` |
|      11 | 3955 | `	pSrc = (ph7_hashmap *)apArg[0]->x.pOther;` |
|       - | 3956 | `	/* Perform the diff */` |
|      11 | 3957 | `	pEntry = pSrc->pFirst;` |
|      11 | 3958 | `	n = pSrc->nEntry;` |
|      11 | 3959 | `	pN1 = pN2 = 0;` |
|      29 | 3960 | `	for(;;){` |
|       - | 3961 | `		int keep;` |
|      35 | 3962 | `		if( n < 1 ){` |
|      11 | 3963 | `			break;` |
|       - | 3964 | `		}` |
|       - | 3965 | `		/* assume the element should be kept until we find a match */` |
|      25 | 3966 | `		keep = 1;` |
|      41 | 3967 | `		for( i = 1 ; i < nArg ; i++ ){` |
|       - | 3968 | `			/* all arguments have been validated already, so cast directly */` |
|      29 | 3969 | `			pMap = (ph7_hashmap *)apArg[i]->x.pOther;` |
|       - | 3970 | `			/* Perform a key lookup first */` |
|      29 | 3971 | `			if( pEntry->iType == HASHMAP_INT_NODE ){` |
|      13 | 3972 | `				rc = HashmapLookupIntKey(pMap,pEntry->xKey.iKey,&pN1);` |
|       7 | 3973 | `			}else{` |
|      17 | 3974 | `				rc = HashmapLookupBlobKey(pMap,SyBlobData(&pEntry->xKey.sKey),SyBlobLength(&pEntry->xKey.sKey),&pN1);` |
|       - | 3975 | `			}` |
|      29 | 3976 | `			if( rc != SXRET_OK ){` |
|       - | 3977 | `				/* this array does not contain the key, continue checking others */` |
|      15 | 3978 | `				continue;` |
|       - | 3979 | `			}` |
|       - | 3980 | `			/* key exists; check that value stored in the matching node is equal */` |
|      15 | 3981 | `			pVal = HashmapExtractNodeValue(pEntry);` |
|      15 | 3982 | `			if( pVal ){` |
|       - | 3983 | `				/* directly compare with value at pN1 rather than searching again */` |
|      15 | 3984 | `				ph7_value *pVal2 = HashmapExtractNodeValue(pN1);` |
|      15 | 3985 | `				if( pVal2 ){` |
|      15 | 3986 | `					sxi32 cmp = PH7_MemObjCmp(pVal, pVal2, TRUE, 0);` |
|      15 | 3987 | `					if( cmp == 0 ){` |
|       - | 3988 | `						/* identical key+value found in one of the arrays => drop it */` |
|      13 | 3989 | `						keep = 0;` |
|      13 | 3990 | `						break;` |
|       - | 3991 | `					}` |
|       1 | 3992 | `				}` |
|       1 | 3993 | `			}` |
|       2 | 3994 | `		}` |
|      25 | 3995 | `		if( keep ){` |
|       - | 3996 | `			/* Perform the insertion */` |
|      13 | 3997 | `			HashmapInsertNode((ph7_hashmap *)pArray->x.pOther,pEntry,TRUE);` |
|       6 | 3998 | `		}` |
|       - | 3999 | `		/* Point to the next entry */` |
|      25 | 4000 | `		pEntry = pEntry->pPrev; /* Reverse link */` |
|      25 | 4001 | `		n--;` |
|       1 | 4002 | `	}` |
|       - | 4003 | `	/* Return the freshly created array */` |
|      11 | 4004 | `	ph7_result_value(pCtx,pArray);` |
|      11 | 4005 | `	return PH7_OK;` |
|      12 | 4006 |  |
|       - | 4007 | `/*` |
|       - | 4008 | ` * array array_diff_uassoc(array $array1,array $array2,...,callback $key_compare_func)` |
|       - | 4009 | ` *  Computes the difference of arrays with additional index check which is performed` |
|       - | 4010 | ` *  by a user supplied callback function.` |
|       - | 4011 | ` * Parameters` |
|       - | 4012 | ` *  $array1` |
|       - | 4013 | ` *    The array to compare from` |
|       - | 4014 | ` *  $array2` |
|       - | 4015 | ` *    An array to compare against` |
|       - | 4016 | ` *  $...` |
|       - | 4017 | ` *   More arrays to compare against.` |
|       - | 4018 | ` *  $key_compare_func` |
|       - | 4019 | ` *   Callback function to use. The callback function must return an integer` |
|       - | 4020 | ` *   less than, equal to, or greater than zero if the first argument is considered` |
|       - | 4021 | ` *   to be respectively less than, equal to, or greater than the second.` |
|       - | 4022 | ` * Return` |
|       - | 4023 | ` *  Returns an array containing all the entries from array1 that` |
|       - | 4024 | ` *  are not present in any of the other arrays.` |
|       - | 4025 | ` */` |
|      22 | 4026 | `static int ph7_hashmap_diff_uassoc(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       2 | 4027 |  |
|       - | 4028 | `	ph7_hashmap_node *pEntry;` |
|       - | 4029 | `	ph7_hashmap *pSrc,*pMap;` |
|       - | 4030 | `	ph7_value *pCallback;` |
|       - | 4031 | `	ph7_value *pArray;` |
|       - | 4032 | `	sxi32 rc;` |
|       - | 4033 | `	sxu32 n;` |
|       - | 4034 | `	int i;` |
|       - | 4035 |  |
|       - | 4036 | `	/* Argument validation mimicking PHP errors. */` |
|      24 | 4037 | `	if( nArg < 2 ){` |
|       4 | 4038 | `		return PH7_VmThrowException(pCtx,` |
|       - | 4039 | `			"ArgumentCountError",` |
|       - | 4040 | `			"array_diff_uassoc() expects at least 2 arguments, %d given",` |
|       1 | 4041 | `			nArg` |
|       - | 4042 | `			);` |
|       - | 4043 | `	}` |
|      22 | 4044 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|       4 | 4045 | `		return PH7_VmThrowException(pCtx,` |
|       - | 4046 | `			"TypeError",` |
|       - | 4047 | `			"array_diff_uassoc(): Argument #1 ($array) must be of type array, %s given",` |
|       1 | 4048 | `			ph7_type_name(apArg[0])` |
|       - | 4049 | `			);` |
|       - | 4050 | `	}` |
|       - | 4051 | `	/* Intermediate arguments (except last) must be arrays. Last argument is` |
|       - | 4052 | `	 * expected to be a callback. */` |
|      32 | 4053 | `	for(i = 1 ; i < nArg - 1; i++){` |
|      16 | 4054 | `		if( !ph7_value_is_array(apArg[i]) ){` |
|       4 | 4055 | `			return PH7_VmThrowException(pCtx,` |
|       - | 4056 | `				"TypeError",` |
|       - | 4057 | `				"array_diff_uassoc(): Argument #%d must be of type array, %s given",` |
|       1 | 4058 | `				i + 1,` |
|       2 | 4059 | `				ph7_type_name(apArg[i])` |
|       - | 4060 | `				);` |
|       - | 4061 | `		}` |
|       8 | 4062 | `	}` |
|       - | 4063 | `	/* Point to the callback value */` |
|      18 | 4064 | `	pCallback = apArg[nArg - 1];` |
|      18 | 4065 | `	if( !ph7_value_is_callable(pCallback) ){` |
|       - | 4066 | `		/* Compose an error message that closely matches PHP output. When the` |
|       - | 4067 | `		 * argument is an array of the wrong shape we include an extra clause.` |
|       - | 4068 | `		 * If the value is neither array nor string, PHP says "no array or` |
|       - | 4069 | `		 * string given" which we also reproduce. */` |
|       7 | 4070 | `		if( ph7_value_is_array(pCallback) ){` |
|       - | 4071 | `			/* ARRAY CALLBACK must have exactly two members */` |
|       4 | 4072 | `			return PH7_VmThrowException(pCtx,` |
|       - | 4073 | `				"TypeError",` |
|       - | 4074 | `				"array_diff_uassoc(): Argument #%d must be a valid callback, array callback must have exactly two members",` |
|       1 | 4075 | `				nArg` |
|       - | 4076 | `				);` |
|       - | 4077 | `		}` |
|       5 | 4078 | `		if( !ph7_value_is_string(pCallback) ){` |
|       - | 4079 | `			/* neither array nor string */` |
|       7 | 4080 | `			return PH7_VmThrowException(pCtx,` |
|       - | 4081 | `				"TypeError",` |
|       - | 4082 | `				"array_diff_uassoc(): Argument #%d must be a valid callback, no array or string given",` |
|       2 | 4083 | `				nArg` |
|       - | 4084 | `				);` |
|       - | 4085 | `		}` |
|       - | 4086 | `		/* Fallback for string (non-callable) or other leftover cases */` |
|     ! 0 | 4087 | `		return PH7_VmThrowException(pCtx,` |
|       - | 4088 | `			"TypeError",` |
|       - | 4089 | `			"array_diff_uassoc(): Argument #%d must be a valid callback, %s given",` |
|     ! 0 | 4090 | `			nArg,` |
|     ! 0 | 4091 | `			ph7_type_name(pCallback)` |
|       - | 4092 | `			);` |
|       - | 4093 | `	}` |
|      11 | 4094 | `	if( nArg == 2 ){` |
|       - | 4095 | `		/* If we only have the first array and the callback, just return the` |
|       - | 4096 | `		 * input array. */` |
|       3 | 4097 | `		ph7_result_value(pCtx,apArg[0]);` |
|       3 | 4098 | `		return PH7_OK;` |
|       - | 4099 | `	}` |
|       - | 4100 | `	/* Create a new array */` |
|       9 | 4101 | `	pArray = ph7_context_new_array(pCtx);` |
|       9 | 4102 | `	if( pArray == 0 ){` |
|     ! 0 | 4103 | `		ph7_result_null(pCtx);` |
|     ! 0 | 4104 | `		return PH7_OK;` |
|       - | 4105 | `	}` |
|       - | 4106 | `	/* Point to the internal representation of the source hashmap */` |
|       9 | 4107 | `	pSrc = (ph7_hashmap *)apArg[0]->x.pOther;` |
|       - | 4108 | `	/* Perform the diff */` |
|       9 | 4109 | `	pEntry = pSrc->pFirst;` |
|       9 | 4110 | `	n = pSrc->nEntry;` |
|      20 | 4111 | `	for(;;){` |
|       - | 4112 | `		int keep;` |
|      25 | 4113 | `		if( n < 1 ){` |
|       9 | 4114 | `			break;` |
|       - | 4115 | `		}` |
|      17 | 4116 | `		keep = 1;` |
|      29 | 4117 | `		for( i = 1 ; i < nArg - 1; i++ ){` |
|       - | 4118 | `			/* each of these must already be arrays thanks to earlier validation */` |
|      21 | 4119 | `			pMap = (ph7_hashmap *)apArg[i]->x.pOther;` |
|       - | 4120 | `			/* we must compare keys via callback, not by direct lookup */` |
|      21 | 4121 | `			ph7_hashmap_node *pIt = pMap->pFirst;` |
|      43 | 4122 | `			while( pIt ){` |
|       - | 4123 | `				/* build temporary key values for callback */` |
|       - | 4124 | `				ph7_value key1, key2, result;` |
|       - | 4125 | `				/* initialise only once using the appropriate helper */` |
|      31 | 4126 | `				if( pEntry->iType == HASHMAP_INT_NODE ){` |
|     ! 0 | 4127 | `					PH7_MemObjInitFromInt(pMap->pVm,&key1,pEntry->xKey.iKey);` |
|     ! 0 | 4128 | `				}else{` |
|       - | 4129 | `					SyString sStr;` |
|      31 | 4130 | `					SyStringInitFromBuf(&sStr,` |
|       - | 4131 | `						SyBlobData(&pEntry->xKey.sKey),` |
|       - | 4132 | `						SyBlobLength(&pEntry->xKey.sKey));` |
|      31 | 4133 | `					PH7_MemObjInitFromString(pMap->pVm,&key1,&sStr);` |
|       - | 4134 | `				}` |
|      31 | 4135 | `				if( pIt->iType == HASHMAP_INT_NODE ){` |
|     ! 0 | 4136 | `					PH7_MemObjInitFromInt(pMap->pVm,&key2,pIt->xKey.iKey);` |
|     ! 0 | 4137 | `				}else{` |
|       - | 4138 | `					SyString sStr;` |
|      31 | 4139 | `					SyStringInitFromBuf(&sStr,` |
|       - | 4140 | `						SyBlobData(&pIt->xKey.sKey),` |
|       - | 4141 | `						SyBlobLength(&pIt->xKey.sKey));` |
|      31 | 4142 | `					PH7_MemObjInitFromString(pMap->pVm,&key2,&sStr);` |
|       - | 4143 | `				}` |
|      31 | 4144 | `				PH7_MemObjInit(pMap->pVm,&result);` |
|       - | 4145 | `				/* call user callback with (key1, key2) */` |
|       - | 4146 | `				{` |
|       - | 4147 | `					ph7_value *apK[2];` |
|      31 | 4148 | `					apK[0] = &key1;` |
|      31 | 4149 | `					apK[1] = &key2;` |
|      31 | 4150 | `					rc = PH7_VmCallUserFunction(pMap->pVm,pCallback,2,apK,&result);` |
|       - | 4151 | `				}` |
|      31 | 4152 | `				if( rc == SXRET_OK ){` |
|      31 | 4153 | `					if( (result.iFlags & MEMOBJ_INT) == 0 ){` |
|     ! 0 | 4154 | `						PH7_MemObjToInteger(&result);` |
|     ! 0 | 4155 | `					}` |
|      31 | 4156 | `					if( result.x.iVal == 0 ){` |
|       - | 4157 | `						/* keys considered equal by callback; now compare values */` |
|      13 | 4158 | `						ph7_value *pVal1 = HashmapExtractNodeValue(pEntry);` |
|      13 | 4159 | `						ph7_value *pVal2 = HashmapExtractNodeValue(pIt);` |
|      13 | 4160 | `						if( pVal1 && pVal2 ){` |
|      13 | 4161 | `							if( PH7_MemObjCmp(pVal1,pVal2,TRUE,0) == 0 ){` |
|       9 | 4162 | `								keep = 0;` |
|       9 | 4163 | `								PH7_MemObjRelease(&result);` |
|       - | 4164 | `								/* release keys too before breaking */` |
|       9 | 4165 | `								PH7_MemObjRelease(&key1);` |
|       9 | 4166 | `								PH7_MemObjRelease(&key2);` |
|       9 | 4167 | `								break;` |
|       - | 4168 | `							}` |
|       2 | 4169 | `						}` |
|       2 | 4170 | `					}` |
|      11 | 4171 | `				}` |
|      23 | 4172 | `				PH7_MemObjRelease(&result);` |
|      23 | 4173 | `				PH7_MemObjRelease(&key1);` |
|      23 | 4174 | `				PH7_MemObjRelease(&key2);` |
|       - | 4175 | `				/* move to next node */` |
|      23 | 4176 | `				pIt = pIt->pPrev;` |
|      23 | 4177 | `				if( keep == 0 ) break;` |
|       1 | 4178 | `			}` |
|      21 | 4179 | `			if( keep == 0 ) break;` |
|       7 | 4180 | `		}` |
|      17 | 4181 | `		if( keep ){` |
|       - | 4182 | `			/* Perform the insertion */` |
|       9 | 4183 | `			HashmapInsertNode((ph7_hashmap *)pArray->x.pOther,pEntry,TRUE);` |
|       4 | 4184 | `		}` |
|       - | 4185 | `		/* Point to the next entry */` |
|      17 | 4186 | `		pEntry = pEntry->pPrev; /* Reverse link */` |
|      17 | 4187 | `		n--;` |
|       1 | 4188 | `	}` |
|       - | 4189 | `	/* Return the freshly created array */` |
|       9 | 4190 | `	ph7_result_value(pCtx,pArray);` |
|       9 | 4191 | `	return PH7_OK;` |
|      13 | 4192 |  |
|       - | 4193 | `/*` |
|       - | 4194 | ` * array array_diff_key(array $array1 ,array $array2,...)` |
|       - | 4195 | ` *  Computes the difference of arrays using keys for comparison.` |
|       - | 4196 | ` * Parameters` |
|       - | 4197 | ` *  $array1` |
|       - | 4198 | ` *    The array to compare from` |
|       - | 4199 | ` *  $array2` |
|       - | 4200 | ` *    An array to compare against` |
|       - | 4201 | ` *  $...` |
|       - | 4202 | ` *   More arrays to compare against` |
|       - | 4203 | ` * Return` |
|       - | 4204 | ` *  Returns an array containing all the entries from array1 whose keys are not present` |
|       - | 4205 | ` *  in any of the other arrays.` |
|       - | 4206 | ` * Note that NULL is returned on failure.` |
|       - | 4207 | ` */` |
|      14 | 4208 | `static int ph7_hashmap_diff_key(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       2 | 4209 |  |
|       - | 4210 | `	ph7_hashmap_node *pEntry;` |
|       - | 4211 | `	ph7_hashmap *pSrc,*pMap;` |
|       - | 4212 | `	ph7_value *pArray;` |
|       - | 4213 | `	sxi32 rc;` |
|       - | 4214 | `	sxu32 n;` |
|       - | 4215 | `	int i;` |
|       - | 4216 | `	/* Validate arguments to mirror PHP behaviour. Previously invalid inputs` |
|       - | 4217 | `	 * would quietly return NULL which is inconsistent with other hashmap` |
|       - | 4218 | `	 * helpers. */` |
|      16 | 4219 | `	if( nArg < 1 ){` |
|       4 | 4220 | `		return PH7_VmThrowException(pCtx,` |
|       - | 4221 | `			"ArgumentCountError",` |
|       - | 4222 | `			"array_diff_key() expects at least 1 argument, %d given",` |
|       1 | 4223 | `			nArg` |
|       - | 4224 | `			);` |
|       - | 4225 | `	}` |
|      14 | 4226 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|       4 | 4227 | `		return PH7_VmThrowException(pCtx,` |
|       - | 4228 | `			"TypeError",` |
|       - | 4229 | `			"array_diff_key(): Argument #1 ($array) must be of type array, %s given",` |
|       1 | 4230 | `			ph7_type_name(apArg[0])` |
|       - | 4231 | `			);` |
|       - | 4232 | `	}` |
|      20 | 4233 | `	for(i = 1 ; i < nArg ; i++){` |
|      12 | 4234 | `		if( !ph7_value_is_array(apArg[i]) ){` |
|       4 | 4235 | `			return PH7_VmThrowException(pCtx,` |
|       - | 4236 | `				"TypeError",` |
|       - | 4237 | `				"array_diff_key(): Argument #%d must be of type array, %s given",` |
|       1 | 4238 | `				i + 1,` |
|       2 | 4239 | `				ph7_type_name(apArg[i])` |
|       - | 4240 | `				);` |
|       - | 4241 | `		}` |
|       5 | 4242 | `	}` |
|       9 | 4243 | `	if( nArg == 1 ){` |
|       - | 4244 | `		/* Return the first array since we cannot perform a diff */` |
|       3 | 4245 | `		ph7_result_value(pCtx,apArg[0]);` |
|       3 | 4246 | `		return PH7_OK;` |
|       - | 4247 | `	}` |
|       - | 4248 | `	/* Create a new array */` |
|       7 | 4249 | `	pArray = ph7_context_new_array(pCtx);` |
|       7 | 4250 | `	if( pArray == 0 ){` |
|     ! 0 | 4251 | `		ph7_result_null(pCtx);` |
|     ! 0 | 4252 | `		return PH7_OK;` |
|       - | 4253 | `	}` |
|       - | 4254 | `	/* Point to the internal representation of the main hashmap */` |
|       7 | 4255 | `	pSrc = (ph7_hashmap *)apArg[0]->x.pOther;` |
|       - | 4256 | `	/* Perfrom the diff */` |
|       7 | 4257 | `	pEntry = pSrc->pFirst;` |
|       7 | 4258 | `	n = pSrc->nEntry;` |
|      12 | 4259 | `	for(;;){` |
|      25 | 4260 | `		if( n < 1 ){` |
|       7 | 4261 | `			break;` |
|       - | 4262 | `		}` |
|      31 | 4263 | `		for( i = 1 ; i < nArg ; i++ ){` |
|      23 | 4264 | `			if( !ph7_value_is_array(apArg[i])) {` |
|       - | 4265 | `				/* ignore */` |
|     ! 0 | 4266 | `				continue;` |
|       - | 4267 | `			}` |
|      23 | 4268 | `			pMap = (ph7_hashmap *)apArg[i]->x.pOther;` |
|      23 | 4269 | `			if( pEntry->iType == HASHMAP_BLOB_NODE ){` |
|      17 | 4270 | `				SyBlob *pKey = &pEntry->xKey.sKey;` |
|       - | 4271 | `				/* Blob lookup */` |
|      17 | 4272 | `				rc = HashmapLookupBlobKey(pMap,SyBlobData(pKey),SyBlobLength(pKey),0);` |
|       9 | 4273 | `			}else{` |
|       - | 4274 | `				/* Int lookup */` |
|       7 | 4275 | `				rc = HashmapLookupIntKey(pMap,pEntry->xKey.iKey,0);` |
|       - | 4276 | `			}` |
|      23 | 4277 | `			if( rc == SXRET_OK ){` |
|       - | 4278 | `				/* Key exists,break immediately */` |
|      11 | 4279 | `				break;` |
|       - | 4280 | `			}` |
|       7 | 4281 | `		}` |
|      19 | 4282 | `		if( i >= nArg ){` |
|       - | 4283 | `			/* Perform the insertion */` |
|       9 | 4284 | `			HashmapInsertNode((ph7_hashmap *)pArray->x.pOther,pEntry,TRUE);` |
|       4 | 4285 | `		}` |
|       - | 4286 | `		/* Point to the next entry */` |
|      19 | 4287 | `		pEntry = pEntry->pPrev; /* Reverse link */` |
|      19 | 4288 | `		n--;` |
|       1 | 4289 | `	}` |
|       - | 4290 | `	/* Return the freshly created array */` |
|       7 | 4291 | `	ph7_result_value(pCtx,pArray);` |
|       7 | 4292 | `	return PH7_OK;` |
|       9 | 4293 |  |
|       - | 4294 | `/*` |
|       - | 4295 | ` * array array_intersect(array $array1 ,array $array2,...)` |
|       - | 4296 | ` *  Computes the intersection of arrays.` |
|       - | 4297 | ` * Parameters` |
|       - | 4298 | ` *  $array1` |
|       - | 4299 | ` *    The array to compare from` |
|       - | 4300 | ` *  $array2` |
|       - | 4301 | ` *    An array to compare against` |
|       - | 4302 | ` *  $...` |
|       - | 4303 | ` *   More arrays to compare against` |
|       - | 4304 | ` * Return` |
|       - | 4305 | ` *  Returns an array containing all of the values in array1 whose values exist` |
|       - | 4306 | ` *  in all of the parameters. .` |
|       - | 4307 | ` * Note that NULL is returned on failure.` |
|       - | 4308 | ` */` |
|       2 | 4309 | `static int ph7_hashmap_intersect(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       1 | 4310 |  |
|       - | 4311 | `	ph7_hashmap_node *pEntry;` |
|       - | 4312 | `	ph7_hashmap *pSrc,*pMap;` |
|       - | 4313 | `	ph7_value *pArray;` |
|       - | 4314 | `	ph7_value *pVal;` |
|       - | 4315 | `	sxi32 rc;` |
|       - | 4316 | `	sxu32 n;` |
|       - | 4317 | `	int i;` |
|       3 | 4318 | `	if( nArg < 1 \|\| !ph7_value_is_array(apArg[0]) ){` |
|       - | 4319 | `		/* Missing arguments,return NULL */` |
|     ! 0 | 4320 | `		ph7_result_null(pCtx);` |
|     ! 0 | 4321 | `		return PH7_OK;` |
|       - | 4322 | `	}` |
|       3 | 4323 | `	if( nArg == 1 ){` |
|       - | 4324 | `		/* Return the first array since we cannot perform a diff */` |
|     ! 0 | 4325 | `		ph7_result_value(pCtx,apArg[0]);` |
|     ! 0 | 4326 | `		return PH7_OK;` |
|       - | 4327 | `	}` |
|       - | 4328 | `	/* Create a new array */` |
|       3 | 4329 | `	pArray = ph7_context_new_array(pCtx);` |
|       3 | 4330 | `	if( pArray == 0 ){` |
|     ! 0 | 4331 | `		ph7_result_null(pCtx);` |
|     ! 0 | 4332 | `		return PH7_OK;` |
|       - | 4333 | `	}` |
|       - | 4334 | `	/* Point to the internal representation of the source hashmap */` |
|       3 | 4335 | `	pSrc = (ph7_hashmap *)apArg[0]->x.pOther;` |
|       - | 4336 | `	/* Perform the intersection */` |
|       3 | 4337 | `	pEntry = pSrc->pFirst;` |
|       3 | 4338 | `	n = pSrc->nEntry;` |
|       5 | 4339 | `	for(;;){` |
|      11 | 4340 | `		if( n < 1 ){` |
|       3 | 4341 | `			break;` |
|       - | 4342 | `		}` |
|       - | 4343 | `		/* Extract the node value */` |
|       9 | 4344 | `		pVal = HashmapExtractNodeValue(pEntry);` |
|       9 | 4345 | `		if( pVal ){` |
|      13 | 4346 | `			for( i = 1 ; i < nArg ; i++ ){` |
|       9 | 4347 | `				if( !ph7_value_is_array(apArg[i])) {` |
|       - | 4348 | `					/* ignore */` |
|     ! 0 | 4349 | `					continue;` |
|       - | 4350 | `				}` |
|       - | 4351 | `				/* Point to the internal representation of the hashmap */` |
|       9 | 4352 | `				pMap = (ph7_hashmap *)apArg[i]->x.pOther;` |
|       - | 4353 | `				/* Perform the lookup */` |
|       9 | 4354 | `				rc = HashmapFindValue(pMap,pVal,0,TRUE);` |
|       9 | 4355 | `				if( rc != SXRET_OK ){` |
|       - | 4356 | `					/* Value does not exist */` |
|       5 | 4357 | `					break;` |
|       - | 4358 | `				}` |
|       3 | 4359 | `			}` |
|       9 | 4360 | `			if( i >= nArg ){` |
|       - | 4361 | `				/* Perform the insertion */` |
|       5 | 4362 | `				HashmapInsertNode((ph7_hashmap *)pArray->x.pOther,pEntry,TRUE);` |
|       2 | 4363 | `			}` |
|       4 | 4364 | `		}` |
|       - | 4365 | `		/* Point to the next entry */` |
|       9 | 4366 | `		pEntry = pEntry->pPrev; /* Reverse link */` |
|       9 | 4367 | `		n--;` |
|       1 | 4368 | `	}` |
|       - | 4369 | `	/* Return the freshly created array */` |
|       3 | 4370 | `	ph7_result_value(pCtx,pArray);` |
|       3 | 4371 | `	return PH7_OK;` |
|       2 | 4372 |  |
|       - | 4373 | `/*` |
|       - | 4374 | ` * array array_intersect_assoc(array $array1 ,array $array2,...)` |
|       - | 4375 | ` *  Computes the intersection of arrays.` |
|       - | 4376 | ` * Parameters` |
|       - | 4377 | ` *  $array1` |
|       - | 4378 | ` *    The array to compare from` |
|       - | 4379 | ` *  $array2` |
|       - | 4380 | ` *    An array to compare against` |
|       - | 4381 | ` *  $...` |
|       - | 4382 | ` *   More arrays to compare against` |
|       - | 4383 | ` * Return` |
|       - | 4384 | ` *  Returns an array containing all of the values in array1 whose values exist` |
|       - | 4385 | ` *  in all of the parameters. .` |
|       - | 4386 | ` * Note that NULL is returned on failure.` |
|       - | 4387 | ` */` |
|       2 | 4388 | `static int ph7_hashmap_intersect_assoc(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       1 | 4389 |  |
|       - | 4390 | `	ph7_hashmap_node *pEntry,*pN1,*pN2;` |
|       - | 4391 | `	ph7_hashmap *pSrc,*pMap;` |
|       - | 4392 | `	ph7_value *pArray;` |
|       - | 4393 | `	ph7_value *pVal;` |
|       - | 4394 | `	sxi32 rc;` |
|       - | 4395 | `	sxu32 n;` |
|       - | 4396 | `	int i;` |
|       3 | 4397 | `	if( nArg < 1 \|\| !ph7_value_is_array(apArg[0]) ){` |
|       - | 4398 | `		/* Missing arguments,return NULL */` |
|     ! 0 | 4399 | `		ph7_result_null(pCtx);` |
|     ! 0 | 4400 | `		return PH7_OK;` |
|       - | 4401 | `	}` |
|       3 | 4402 | `	if( nArg == 1 ){` |
|       - | 4403 | `		/* Return the first array since we cannot perform a diff */` |
|     ! 0 | 4404 | `		ph7_result_value(pCtx,apArg[0]);` |
|     ! 0 | 4405 | `		return PH7_OK;` |
|       - | 4406 | `	}` |
|       - | 4407 | `	/* Create a new array */` |
|       3 | 4408 | `	pArray = ph7_context_new_array(pCtx);` |
|       3 | 4409 | `	if( pArray == 0 ){` |
|     ! 0 | 4410 | `		ph7_result_null(pCtx);` |
|     ! 0 | 4411 | `		return PH7_OK;` |
|       - | 4412 | `	}` |
|       - | 4413 | `	/* Point to the internal representation of the source hashmap */` |
|       3 | 4414 | `	pSrc = (ph7_hashmap *)apArg[0]->x.pOther;` |
|       - | 4415 | `	/* Perform the intersection */` |
|       3 | 4416 | `	pEntry = pSrc->pFirst;` |
|       3 | 4417 | `	n = pSrc->nEntry;` |
|       3 | 4418 | `	pN1 = pN2 = 0; /* cc warning */` |
|       4 | 4419 | `	for(;;){` |
|       9 | 4420 | `		if( n < 1 ){` |
|       3 | 4421 | `			break;` |
|       - | 4422 | `		}` |
|       - | 4423 | `		/* Extract the node value */` |
|       7 | 4424 | `		pVal = HashmapExtractNodeValue(pEntry);` |
|       7 | 4425 | `		if( pVal ){` |
|       9 | 4426 | `			for( i = 1 ; i < nArg ; i++ ){` |
|       7 | 4427 | `				if( !ph7_value_is_array(apArg[i])) {` |
|       - | 4428 | `					/* ignore */` |
|     ! 0 | 4429 | `					continue;` |
|       - | 4430 | `				}` |
|       - | 4431 | `				/* Point to the internal representation of the hashmap */` |
|       7 | 4432 | `				pMap = (ph7_hashmap *)apArg[i]->x.pOther;` |
|       - | 4433 | `				/* Perform a key lookup first */` |
|       7 | 4434 | `				if( pEntry->iType == HASHMAP_INT_NODE ){` |
|     ! 0 | 4435 | `					rc = HashmapLookupIntKey(pMap,pEntry->xKey.iKey,&pN1);` |
|     ! 0 | 4436 | `				}else{` |
|       7 | 4437 | `					rc = HashmapLookupBlobKey(pMap,SyBlobData(&pEntry->xKey.sKey),SyBlobLength(&pEntry->xKey.sKey),&pN1);` |
|       - | 4438 | `				}` |
|       7 | 4439 | `				if( rc != SXRET_OK ){` |
|       - | 4440 | `					/* No such key,break immediately */` |
|       3 | 4441 | `					break;` |
|       - | 4442 | `				}` |
|       - | 4443 | `				/* Perform the lookup */` |
|       5 | 4444 | `				rc = HashmapFindValue(pMap,pVal,&pN2,TRUE);` |
|       5 | 4445 | `				if( rc != SXRET_OK \|\| pN1 != pN2 ){` |
|       - | 4446 | `					/* Value does not exist */` |
|       2 | 4447 | `					break;` |
|       - | 4448 | `				}` |
|       2 | 4449 | `			}` |
|       7 | 4450 | `			if( i >= nArg ){` |
|       - | 4451 | `				/* Perform the insertion */` |
|       3 | 4452 | `				HashmapInsertNode((ph7_hashmap *)pArray->x.pOther,pEntry,TRUE);` |
|       1 | 4453 | `			}` |
|       3 | 4454 | `		}` |
|       - | 4455 | `		/* Point to the next entry */` |
|       7 | 4456 | `		pEntry = pEntry->pPrev; /* Reverse link */` |
|       7 | 4457 | `		n--;` |
|       1 | 4458 | `	}` |
|       - | 4459 | `	/* Return the freshly created array */` |
|       3 | 4460 | `	ph7_result_value(pCtx,pArray);` |
|       3 | 4461 | `	return PH7_OK;` |
|       2 | 4462 |  |
|       - | 4463 | `/*` |
|       - | 4464 | ` * array array_intersect_key(array $array1 ,array $array2,...)` |
|       - | 4465 | ` *  Computes the intersection of arrays using keys for comparison.` |
|       - | 4466 | ` * Parameters` |
|       - | 4467 | ` *  $array1` |
|       - | 4468 | ` *    The array to compare from` |
|       - | 4469 | ` *  $array2` |
|       - | 4470 | ` *    An array to compare against` |
|       - | 4471 | ` *  $...` |
|       - | 4472 | ` *   More arrays to compare against` |
|       - | 4473 | ` * Return` |
|       - | 4474 | ` *  Returns an associative array containing all the entries of array1 which` |
|       - | 4475 | ` *  have keys that are present in all arguments.` |
|       - | 4476 | ` * Note that NULL is returned on failure.` |
|       - | 4477 | ` */` |
|       4 | 4478 | `static int ph7_hashmap_intersect_key(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       1 | 4479 |  |
|       - | 4480 | `	ph7_hashmap_node *pEntry;` |
|       - | 4481 | `	ph7_hashmap *pSrc,*pMap;` |
|       - | 4482 | `	ph7_value *pArray;` |
|       - | 4483 | `	sxi32 rc;` |
|       - | 4484 | `	sxu32 n;` |
|       - | 4485 | `	int i;` |
|       5 | 4486 | `	if( nArg < 1 \|\| !ph7_value_is_array(apArg[0]) ){` |
|       - | 4487 | `		/* Missing arguments,return NULL */` |
|     ! 0 | 4488 | `		ph7_result_null(pCtx);` |
|     ! 0 | 4489 | `		return PH7_OK;` |
|       - | 4490 | `	}` |
|       5 | 4491 | `	if( nArg == 1 ){` |
|       - | 4492 | `		/* Return the first array since we cannot perform a diff */` |
|     ! 0 | 4493 | `		ph7_result_value(pCtx,apArg[0]);` |
|     ! 0 | 4494 | `		return PH7_OK;` |
|       - | 4495 | `	}` |
|       - | 4496 | `	/* Create a new array */` |
|       5 | 4497 | `	pArray = ph7_context_new_array(pCtx);` |
|       5 | 4498 | `	if( pArray == 0 ){` |
|     ! 0 | 4499 | `		ph7_result_null(pCtx);` |
|     ! 0 | 4500 | `		return PH7_OK;` |
|       - | 4501 | `	}` |
|       - | 4502 | `	/* Point to the internal representation of the main hashmap */` |
|       5 | 4503 | `	pSrc = (ph7_hashmap *)apArg[0]->x.pOther;` |
|       - | 4504 | `	/* Perfrom the intersection */` |
|       5 | 4505 | `	pEntry = pSrc->pFirst;` |
|       5 | 4506 | `	n = pSrc->nEntry;` |
|       8 | 4507 | `	for(;;){` |
|      17 | 4508 | `		if( n < 1 ){` |
|       5 | 4509 | `			break;` |
|       - | 4510 | `		}` |
|      19 | 4511 | `		for( i = 1 ; i < nArg ; i++ ){` |
|      13 | 4512 | `			if( !ph7_value_is_array(apArg[i])) {` |
|       - | 4513 | `				/* ignore */` |
|     ! 0 | 4514 | `				continue;` |
|       - | 4515 | `			}` |
|      13 | 4516 | `			pMap = (ph7_hashmap *)apArg[i]->x.pOther;` |
|      13 | 4517 | `			if( pEntry->iType == HASHMAP_BLOB_NODE ){` |
|       7 | 4518 | `				SyBlob *pKey = &pEntry->xKey.sKey;` |
|       - | 4519 | `				/* Blob lookup */` |
|       7 | 4520 | `				rc = HashmapLookupBlobKey(pMap,SyBlobData(pKey),SyBlobLength(pKey),0);` |
|       4 | 4521 | `			}else{` |
|       - | 4522 | `				/* Int key */` |
|       7 | 4523 | `				rc = HashmapLookupIntKey(pMap,pEntry->xKey.iKey,0);` |
|       - | 4524 | `			}` |
|      13 | 4525 | `			if( rc != SXRET_OK ){` |
|       - | 4526 | `				/* Key does not exists,break immediately */` |
|       7 | 4527 | `				break;` |
|       - | 4528 | `			}` |
|       4 | 4529 | `		}` |
|      13 | 4530 | `		if( i >= nArg ){` |
|       - | 4531 | `			/* Perform the insertion */` |
|       7 | 4532 | `			HashmapInsertNode((ph7_hashmap *)pArray->x.pOther,pEntry,TRUE);` |
|       3 | 4533 | `		}` |
|       - | 4534 | `		/* Point to the next entry */` |
|      13 | 4535 | `		pEntry = pEntry->pPrev; /* Reverse link */` |
|      13 | 4536 | `		n--;` |
|       1 | 4537 | `	}` |
|       - | 4538 | `	/* Return the freshly created array */` |
|       5 | 4539 | `	ph7_result_value(pCtx,pArray);` |
|       5 | 4540 | `	return PH7_OK;` |
|       3 | 4541 |  |
|       - | 4542 | `/*` |
|       - | 4543 | ` * array array_uintersect(array $array1 ,array $array2,...,$callback)` |
|       - | 4544 | ` *  Computes the intersection of arrays.` |
|       - | 4545 | ` * Parameters` |
|       - | 4546 | ` *  $array1` |
|       - | 4547 | ` *    The array to compare from` |
|       - | 4548 | ` *  $array2` |
|       - | 4549 | ` *    An array to compare against` |
|       - | 4550 | ` *  $...` |
|       - | 4551 | ` *   More arrays to compare against` |
|       - | 4552 | ` * $callback` |
|       - | 4553 | ` *  The callback comparison function.` |
|       - | 4554 | ` *  The comparison function must return an integer less than, equal to, or greater than zero` |
|       - | 4555 | ` *  if the first argument is considered to be respectively less than, equal to, or greater` |
|       - | 4556 | ` *  than the second.` |
|       - | 4557 | ` *     int callback ( mixed $a, mixed $b )` |
|       - | 4558 | ` * Return` |
|       - | 4559 | ` *  Returns an array containing all of the values in array1 whose values exist` |
|       - | 4560 | ` *  in all of the parameters. .` |
|       - | 4561 | ` * Note that NULL is returned on failure.` |
|       - | 4562 | ` */` |
|       2 | 4563 | `static int ph7_hashmap_uintersect(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       1 | 4564 |  |
|       - | 4565 | `	ph7_hashmap_node *pEntry;` |
|       - | 4566 | `	ph7_hashmap *pSrc,*pMap;` |
|       - | 4567 | `	ph7_value *pCallback;` |
|       - | 4568 | `	ph7_value *pArray;` |
|       - | 4569 | `	ph7_value *pVal;` |
|       - | 4570 | `	sxi32 rc;` |
|       - | 4571 | `	sxu32 n;` |
|       - | 4572 | `	int i;` |
|       - | 4573 |  |
|       3 | 4574 | `	if( nArg < 2 \|\| !ph7_value_is_array(apArg[0]) ){` |
|       - | 4575 | `		/* Missing/Invalid arguments,return NULL */` |
|     ! 0 | 4576 | `		ph7_result_null(pCtx);` |
|     ! 0 | 4577 | `		return PH7_OK;` |
|       - | 4578 | `	}` |
|       - | 4579 | `	/* Point to the callback */` |
|       3 | 4580 | `	pCallback = apArg[nArg - 1];` |
|       3 | 4581 | `	if( nArg == 2 ){` |
|       - | 4582 | `		/* Return the first array since we cannot perform a diff */` |
|     ! 0 | 4583 | `		ph7_result_value(pCtx,apArg[0]);` |
|     ! 0 | 4584 | `		return PH7_OK;` |
|       - | 4585 | `	}` |
|       - | 4586 | `	/* Create a new array */` |
|       3 | 4587 | `	pArray = ph7_context_new_array(pCtx);` |
|       3 | 4588 | `	if( pArray == 0 ){` |
|     ! 0 | 4589 | `		ph7_result_null(pCtx);` |
|     ! 0 | 4590 | `		return PH7_OK;` |
|       - | 4591 | `	}` |
|       - | 4592 | `	/* Point to the internal representation of the source hashmap */` |
|       3 | 4593 | `	pSrc = (ph7_hashmap *)apArg[0]->x.pOther;` |
|       - | 4594 | `	/* Perform the intersection */` |
|       3 | 4595 | `	pEntry = pSrc->pFirst;` |
|       3 | 4596 | `	n = pSrc->nEntry;` |
|       4 | 4597 | `	for(;;){` |
|       9 | 4598 | `		if( n < 1 ){` |
|       3 | 4599 | `			break;` |
|       - | 4600 | `		}` |
|       - | 4601 | `		/* Extract the node value */` |
|       7 | 4602 | `		pVal = HashmapExtractNodeValue(pEntry);` |
|       7 | 4603 | `		if( pVal ){` |
|      11 | 4604 | `			for( i = 1 ; i < nArg - 1; i++ ){` |
|       7 | 4605 | `				if( !ph7_value_is_array(apArg[i])) {` |
|       - | 4606 | `					/* ignore */` |
|     ! 0 | 4607 | `					continue;` |
|       - | 4608 | `				}` |
|       - | 4609 | `				/* Point to the internal representation of the hashmap */` |
|       7 | 4610 | `				pMap = (ph7_hashmap *)apArg[i]->x.pOther;` |
|       - | 4611 | `				/* Perform the lookup */` |
|       7 | 4612 | `				rc = HashmapFindValueByCallback(pMap,pVal,pCallback,0);` |
|       7 | 4613 | `				if( rc != SXRET_OK ){` |
|       - | 4614 | `					/* Value does not exist */` |
|       3 | 4615 | `					break;` |
|       - | 4616 | `				}` |
|       3 | 4617 | `			}` |
|       7 | 4618 | `			if( i >= (nArg-1) ){` |
|       - | 4619 | `				/* Perform the insertion */` |
|       5 | 4620 | `				HashmapInsertNode((ph7_hashmap *)pArray->x.pOther,pEntry,TRUE);` |
|       2 | 4621 | `			}` |
|       3 | 4622 | `		}` |
|       - | 4623 | `		/* Point to the next entry */` |
|       7 | 4624 | `		pEntry = pEntry->pPrev; /* Reverse link */` |
|       7 | 4625 | `		n--;` |
|       1 | 4626 | `	}` |
|       - | 4627 | `	/* Return the freshly created array */` |
|       3 | 4628 | `	ph7_result_value(pCtx,pArray);` |
|       3 | 4629 | `	return PH7_OK;` |
|       2 | 4630 |  |
|       - | 4631 | `/*` |
|       - | 4632 | ` * array array_fill(int $start_index,int $num,var $value)` |
|       - | 4633 | ` *  Fill an array with values.` |
|       - | 4634 | ` * Parameters` |
|       - | 4635 | ` *  $start_index` |
|       - | 4636 | ` *    The first index of the returned array.` |
|       - | 4637 | ` *  $num` |
|       - | 4638 | ` *   Number of elements to insert.` |
|       - | 4639 | ` *  $value` |
|       - | 4640 | ` *    Value to use for filling.` |
|       - | 4641 | ` * Return` |
|       - | 4642 | ` *  The filled array or null on failure.` |
|       - | 4643 | ` */` |
|     238 | 4644 | `static int ph7_hashmap_fill(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       2 | 4645 |  |
|       - | 4646 | `	ph7_value *pArray;` |
|       - | 4647 | `	int i,nEntry;` |
|       - | 4648 |  |
|       - | 4649 | `	/* PHP enforces argument count and type checks. */` |
|     240 | 4650 | `	if( nArg != 3 ){` |
|       - | 4651 | `		/* wrong number of arguments -> ArgumentCountError */` |
|       7 | 4652 | `		return PH7_VmThrowException(pCtx,` |
|       - | 4653 | `			"ArgumentCountError",` |
|       - | 4654 | `			"array_fill() expects exactly 3 arguments, %d given",` |
|       2 | 4655 | `			nArg` |
|       - | 4656 | `			);` |
|       - | 4657 | `	}` |
|       - | 4658 |  |
|       - | 4659 | `	/* Argument #1: start index must be convertible to int.  Accept booleans,` |
|       - | 4660 | `	 * floats, and numeric strings (including those with decimal point) by` |
|       - | 4661 | `	 * allowing them through the conversion.  Only arrays, objects, resources` |
|       - | 4662 | `	 * and NULLs are rejected outright. */` |
|     466 | 4663 | `	if( ph7_value_is_array(apArg[0]) \|\| ph7_value_is_object(apArg[0]) \|\|` |
|     350 | 4664 | `		ph7_value_is_resource(apArg[0]) \|\| ph7_value_is_null(apArg[0]) ){` |
|       4 | 4665 | `		return PH7_VmThrowException(pCtx,` |
|       - | 4666 | `			"TypeError",` |
|       - | 4667 | `			"array_fill(): Argument #1 ($start_index) must be of type int, %s given",` |
|       1 | 4668 | `			ph7_type_name(apArg[0])` |
|       - | 4669 | `			);` |
|       - | 4670 | `	}` |
|     234 | 4671 | `	if( ph7_value_is_string(apArg[0]) ){` |
|       - | 4672 | `		int len;` |
|       8 | 4673 | `		sxu8 bReal = FALSE;` |
|       8 | 4674 | `		const char *zStr = ph7_value_to_string(apArg[0], &len);` |
|       8 | 4675 | `		if( SyStrIsNumeric(zStr, len, &bReal, 0) != SXRET_OK ){` |
|       - | 4676 | `			/* Non‑numeric string is an error. */` |
|       3 | 4677 | `			return PH7_VmThrowException(pCtx,` |
|       - | 4678 | `				"TypeError",` |
|       - | 4679 | `				"array_fill(): Argument #1 ($start_index) must be of type int, string given"` |
|       - | 4680 | `				);` |
|       - | 4681 | `		}` |
|       5 | 4682 | `		if( bReal ){` |
|       - | 4683 | `			/* float-string -> deprecation warning */` |
|       4 | 4684 | `			ph7_context_throw_error_format(pCtx, E_DEPRECATED,` |
|       - | 4685 | `				"Implicit conversion from float-string \"%s\" to int loses precision",` |
|       1 | 4686 | `				zStr` |
|       - | 4687 | `				);` |
|       1 | 4688 | `		}` |
|       2 | 4689 | `	}` |
|       - | 4690 |  |
|       - | 4691 | `	/* Argument #2: count must be convertible to non-negative int.  Allow booleans,` |
|       - | 4692 | `	 * floats and numeric strings; reject arrays, objects, resources and NULL. */` |
|     460 | 4693 | `	if( ph7_value_is_array(apArg[1]) \|\| ph7_value_is_object(apArg[1]) \|\|` |
|     347 | 4694 | `		ph7_value_is_resource(apArg[1]) \|\| ph7_value_is_null(apArg[1]) ){` |
|     ! 0 | 4695 | `		return PH7_VmThrowException(pCtx,` |
|       - | 4696 | `			"TypeError",` |
|       - | 4697 | `			"array_fill(): Argument #2 ($count) must be of type int, %s given",` |
|     ! 0 | 4698 | `			ph7_type_name(apArg[1])` |
|       - | 4699 | `			);` |
|       - | 4700 | `	}` |
|     232 | 4701 | `	if( ph7_value_is_string(apArg[1]) ){` |
|       - | 4702 | `		int len;` |
|       3 | 4703 | `		sxu8 bReal = FALSE;` |
|       3 | 4704 | `		const char *zStr = ph7_value_to_string(apArg[1], &len);` |
|       3 | 4705 | `		if( SyStrIsNumeric(zStr, len, &bReal, 0) != SXRET_OK ){` |
|       3 | 4706 | `			return PH7_VmThrowException(pCtx,` |
|       - | 4707 | `				"TypeError",` |
|       - | 4708 | `				"array_fill(): Argument #2 ($count) must be of type int, string given"` |
|       - | 4709 | `				);` |
|       - | 4710 | `		}` |
|     ! 0 | 4711 | `	}` |
|       - | 4712 | `	/* Note: booleans and floats (including fractional) are now accepted; they` |
|       - | 4713 | `	 * will be converted by ph7_value_to_int below. */` |
|     230 | 4714 | `	if( ph7_value_is_float(apArg[1]) ){` |
|       5 | 4715 | `		double d = ph7_value_to_double(apArg[1]);` |
|       - | 4716 | `		/* avoid hiding outer 'i' (loop index) */` |
|       5 | 4717 | `		sxi64 i64 = (sxi64)d;` |
|       5 | 4718 | `		if( d != (double)i64 ){` |
|       7 | 4719 | `			ph7_context_throw_error_format(pCtx, E_DEPRECATED,` |
|       - | 4720 | `				"Implicit conversion from float %g to int loses precision",` |
|       2 | 4721 | `				d` |
|       - | 4722 | `				);` |
|       2 | 4723 | `		}` |
|       2 | 4724 | `	}` |
|       - | 4725 |  |
|       - | 4726 | `	/* Total number of entries to insert */` |
|     230 | 4727 | `	nEntry = ph7_value_to_int(apArg[1]);` |
|       - | 4728 | `	/* Reject negative counts with a ValueError like PHP. */` |
|     230 | 4729 | `	if( nEntry < 0 ){` |
|       3 | 4730 | `		return PH7_VmThrowException(pCtx,` |
|       - | 4731 | `			"ValueError",` |
|       - | 4732 | `			"array_fill(): Argument #2 ($count) must be greater than or equal to 0"` |
|       - | 4733 | `			);` |
|       - | 4734 | `	}` |
|       - | 4735 |  |
|       - | 4736 | `	/* If zero elements were requested, return an empty array without allocating */` |
|     227 | 4737 | `	if( nEntry == 0 ){` |
|       7 | 4738 | `		ph7_result_value(pCtx, ph7_context_new_array(pCtx));` |
|       7 | 4739 | `		return PH7_OK;` |
|       - | 4740 | `	}` |
|       - | 4741 |  |
|       - | 4742 | `	/* Create a new array */` |
|     221 | 4743 | `	pArray = ph7_context_new_array(pCtx);` |
|     221 | 4744 | `	if( pArray == 0 ){` |
|     ! 0 | 4745 | `		ph7_result_null(pCtx);` |
|     ! 0 | 4746 | `		return PH7_OK;` |
|       - | 4747 | `	}` |
|       - | 4748 |  |
|       - | 4749 | `	/* Insert the first entry alone because it has its own key */` |
|     221 | 4750 | `	ph7_array_add_intkey_elem(pArray, ph7_value_to_int(apArg[0]), apArg[2]);` |
|       - | 4751 | `	/* Repeat insertion of the desired value */` |
| 2117583 | 4752 | `	for( i = 1 ; i < nEntry ; i++ ){` |
| 2117363 | 4753 | `		ph7_array_add_elem(pArray, 0/*Automatic index assign */, apArg[2]);` |
| 1058682 | 4754 | `	}` |
|       - | 4755 | `	/* Return the filled array */` |
|     221 | 4756 | `	ph7_result_value(pCtx, pArray);` |
|     221 | 4757 | `	return PH7_OK;` |
|     121 | 4758 |  |
|       - | 4759 | `/*` |
|       - | 4760 | ` * array array_fill_keys(array $input,mixed $value)` |
|       - | 4761 | ` *  Fill an array with values, specifying keys.` |
|       - | 4762 | ` * Parameters` |
|       - | 4763 | ` *  $input` |
|       - | 4764 | ` *   Array of values that will be used as key.` |
|       - | 4765 | ` *  $value` |
|       - | 4766 | ` *    Value to use for filling.` |
|       - | 4767 | ` * Return` |
|       - | 4768 | ` *  The filled array.` |
|       - | 4769 | ` * Throws` |
|       - | 4770 | ` *  ValueError if $input is not an array.` |
|       - | 4771 | ` */` |
|      26 | 4772 | `static int ph7_hashmap_fill_keys(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       2 | 4773 |  |
|       - | 4774 | `	ph7_hashmap_node *pEntry;` |
|       - | 4775 | `	ph7_hashmap *pSrc;` |
|       - | 4776 | `	ph7_value *pArray;` |
|       - | 4777 | `	sxu32 n;` |
|       - | 4778 | `	/* PHP enforces exactly 2 arguments. */` |
|      28 | 4779 | `	if( nArg != 2 ){` |
|      10 | 4780 | `		return PH7_VmThrowException(pCtx,` |
|       - | 4781 | `			"ArgumentCountError",` |
|       - | 4782 | `			"array_fill_keys() expects exactly 2 arguments, %d given",` |
|       3 | 4783 | `			nArg` |
|       - | 4784 | `			);` |
|       - | 4785 | `	}` |
|       - | 4786 | `	/* Make sure we are dealing with a valid hashmap */` |
|      22 | 4787 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|       7 | 4788 | `		return PH7_VmThrowException(pCtx,` |
|       - | 4789 | `			"TypeError",` |
|       - | 4790 | `			"array_fill_keys(): Argument #1 ($keys) must be of type array, %s given",` |
|       2 | 4791 | `			ph7_type_name(apArg[0])` |
|       - | 4792 | `			);` |
|       - | 4793 | `	}` |
|       - | 4794 | `	/* Point to the internal representation of the input hashmap */` |
|      17 | 4795 | `	pSrc = (ph7_hashmap *)apArg[0]->x.pOther;` |
|       - | 4796 | `	/* Create a new array */` |
|      17 | 4797 | `	pArray = ph7_context_new_array(pCtx);` |
|      17 | 4798 | `	if( pArray == 0 ){` |
|     ! 0 | 4799 | `		ph7_result_null(pCtx);` |
|     ! 0 | 4800 | `		return PH7_OK;` |
|       - | 4801 | `	}` |
|       - | 4802 | `	/* Perform the requested operation */` |
|      17 | 4803 | `	pEntry = pSrc->pFirst;` |
|      45 | 4804 | `	for( n = 0 ; n < pSrc->nEntry ; n++ ){` |
|      29 | 4805 | `		ph7_array_add_elem(pArray,HashmapExtractNodeValue(pEntry),apArg[1]);` |
|       - | 4806 | `		/* Point to the next entry */` |
|      29 | 4807 | `		pEntry = pEntry->pPrev; /* Reverse link */` |
|      15 | 4808 | `	}` |
|       - | 4809 | `	/* Return the filled array */` |
|      17 | 4810 | `	ph7_result_value(pCtx,pArray);` |
|      17 | 4811 | `	return PH7_OK;` |
|      15 | 4812 |  |
|       - | 4813 | `/*` |
|       - | 4814 | ` * array array_combine(array $keys,array $values)` |
|       - | 4815 | ` *  Creates an array by using one array for keys and another for its values.` |
|       - | 4816 | ` * Parameters` |
|       - | 4817 | ` *  $keys` |
|       - | 4818 | ` *    Array of keys to be used.` |
|       - | 4819 | ` * $values` |
|       - | 4820 | ` *   Array of values to be used.` |
|       - | 4821 | ` * Return` |
|       - | 4822 | ` *  Returns the combined array. Otherwise FALSE if the number of elements` |
|       - | 4823 | ` *  for each array isn't equal or if one of the given arguments is` |
|       - | 4824 | ` *  not an array.` |
|       - | 4825 | ` */` |
|      18 | 4826 | `static int ph7_hashmap_combine(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       2 | 4827 |  |
|       - | 4828 | `	ph7_hashmap_node *pKe,*pVe;` |
|       - | 4829 | `	ph7_hashmap *pKey,*pValue;` |
|       - | 4830 | `	ph7_value *pArray;` |
|       - | 4831 | `	sxu32 n;` |
|       - | 4832 | `	/* PHP enforces argument count and type checks. */` |
|      20 | 4833 | `	if( nArg != 2 ){` |
|       - | 4834 | `		/* wrong number of arguments -> ArgumentCountError */` |
|       4 | 4835 | `		return PH7_VmThrowException(pCtx,` |
|       - | 4836 | `			"ArgumentCountError",` |
|       - | 4837 | `			"array_combine() expects exactly 2 arguments, %d given",` |
|       1 | 4838 | `			nArg` |
|       - | 4839 | `			);` |
|       - | 4840 | `	}` |
|       - | 4841 | `	/* Validate argument types individually so we can report the correct` |
|       - | 4842 | `	 * argument index in the error message. */` |
|      18 | 4843 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|       4 | 4844 | `		return PH7_VmThrowException(pCtx,` |
|       - | 4845 | `			"TypeError",` |
|       - | 4846 | `			"array_combine(): Argument #1 ($keys) must be of type array, %s given",` |
|       1 | 4847 | `			ph7_type_name(apArg[0])` |
|       - | 4848 | `			);` |
|       - | 4849 | `	}` |
|      16 | 4850 | `	if( !ph7_value_is_array(apArg[1]) ){` |
|       4 | 4851 | `		return PH7_VmThrowException(pCtx,` |
|       - | 4852 | `			"TypeError",` |
|       - | 4853 | `			"array_combine(): Argument #2 ($values) must be of type array, %s given",` |
|       2 | 4854 | `			ph7_type_name(apArg[1])` |
|       - | 4855 | `			);` |
|       - | 4856 | `	}` |
|       - | 4857 | `	/* Point to the internal representation of the input hashmaps */` |
|      14 | 4858 | `	pKey   = (ph7_hashmap *)apArg[0]->x.pOther;` |
|      14 | 4859 | `	pValue = (ph7_hashmap *)apArg[1]->x.pOther;` |
|      14 | 4860 | `	if( pKey->nEntry != pValue->nEntry ){` |
|       - | 4861 | `		/* Length mismatch -> ValueError */` |
|       3 | 4862 | `		return PH7_VmThrowException(pCtx,` |
|       - | 4863 | `			"ValueError",` |
|       - | 4864 | `			"array_combine(): Argument #1 ($keys) and argument #2 ($values) must have the same number of elements"` |
|       - | 4865 | `			);` |
|       - | 4866 | `	}` |
|       - | 4867 | `	/* Create a new array */` |
|      11 | 4868 | `	pArray = ph7_context_new_array(pCtx);` |
|      11 | 4869 | `	if( pArray == 0 ){` |
|     ! 0 | 4870 | `		ph7_result_bool(pCtx,0);` |
|     ! 0 | 4871 | `		return PH7_OK;` |
|       - | 4872 | `	}` |
|       - | 4873 | `	/* Perform the requested operation */` |
|      11 | 4874 | `	pKe = pKey->pFirst;` |
|      11 | 4875 | `	pVe = pValue->pFirst;` |
|      33 | 4876 | `	for( n = 0 ; n < pKey->nEntry ; n++ ){` |
|      23 | 4877 | `		ph7_value *pKeyVal = HashmapExtractNodeValue(pKe);` |
|      23 | 4878 | `		ph7_value *pValVal = HashmapExtractNodeValue(pVe);` |
|       - | 4879 | `		/* PHP treats floats used as keys in array_combine differently than` |
|       - | 4880 | `		 * ordinary offset access: the float is stringified rather than` |
|       - | 4881 | `		 * truncated.  To emulate this behavior we create a temporary copy of` |
|       - | 4882 | `		 * the value when it is a float and convert the copy to string.  The` |
|       - | 4883 | `		 * original array must not be mutated. */` |
|      23 | 4884 | `		ph7_value *pKeyCopy = pKeyVal;` |
|      23 | 4885 | `		if( ph7_value_is_float(pKeyVal) ){` |
|       5 | 4886 | `			ph7_value *pTmpKey = ph7_context_new_scalar(pCtx);` |
|       5 | 4887 | `			if( pTmpKey ){` |
|       5 | 4888 | `				PH7_MemObjStore(pKeyVal,pTmpKey);` |
|       - | 4889 | `				/* Convert copy to string so it becomes "1.5" or "2" etc. */` |
|       5 | 4890 | `				PH7_MemObjToString(pTmpKey);` |
|       5 | 4891 | `				pKeyCopy = pTmpKey;` |
|       2 | 4892 | `			}` |
|       2 | 4893 | `		}` |
|      23 | 4894 | `		ph7_array_add_elem(pArray,pKeyCopy,pValVal);` |
|       - | 4895 | `		/* Point to the next entry */` |
|      23 | 4896 | `		pKe = pKe->pPrev; /* Reverse link */` |
|      23 | 4897 | `		pVe = pVe->pPrev;` |
|      12 | 4898 | `	}` |
|       - | 4899 | `	/* Return the filled array */` |
|      11 | 4900 | `	ph7_result_value(pCtx,pArray);` |
|      11 | 4901 | `	return PH7_OK;` |
|      11 | 4902 |  |
|       - | 4903 | `/*` |
|       - | 4904 | ` * array array_reverse(array $array [,bool $preserve_keys = false ])` |
|       - | 4905 | ` *  Return an array with elements in reverse order.` |
|       - | 4906 | ` * Parameters` |
|       - | 4907 | ` *  $array` |
|       - | 4908 | ` *   The input array.` |
|       - | 4909 | ` *  $preserve_keys (optional)` |
|       - | 4910 | ` *   If set to TRUE keys are preserved.` |
|       - | 4911 | ` * Return` |
|       - | 4912 | ` *  The reversed array.` |
|       - | 4913 | ` */` |
|       6 | 4914 | `static int ph7_hashmap_reverse(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       1 | 4915 |  |
|       - | 4916 | `	ph7_hashmap_node *pEntry;` |
|       - | 4917 | `	ph7_hashmap *pSrc;` |
|       - | 4918 | `	ph7_value *pArray;` |
|       - | 4919 | `	int bPreserve;` |
|       - | 4920 | `	sxu32 n;` |
|       7 | 4921 | `	if( nArg < 1 ){` |
|       - | 4922 | `		/* Missing arguments,return NULL */` |
|     ! 0 | 4923 | `		ph7_result_null(pCtx);` |
|     ! 0 | 4924 | `		return PH7_OK;` |
|       - | 4925 | `	}` |
|       - | 4926 | `	/* Make sure we are dealing with a valid hashmap */` |
|       7 | 4927 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|       - | 4928 | `		/* Invalid argument,return NULL */` |
|     ! 0 | 4929 | `		ph7_result_null(pCtx);` |
|     ! 0 | 4930 | `		return PH7_OK;` |
|       - | 4931 | `	}` |
|       7 | 4932 | `	bPreserve = FALSE;` |
|       7 | 4933 | `	if( nArg > 1 && ph7_value_is_bool(apArg[1]) ){` |
|       3 | 4934 | `		bPreserve = ph7_value_to_bool(apArg[1]);` |
|       1 | 4935 | `	}` |
|       - | 4936 | `	/* Point to the internal representation of the input hashmap */` |
|       7 | 4937 | `	pSrc = (ph7_hashmap *)apArg[0]->x.pOther;` |
|       - | 4938 | `	/* Create a new array */` |
|       7 | 4939 | `	pArray = ph7_context_new_array(pCtx);` |
|       7 | 4940 | `	if( pArray == 0 ){` |
|     ! 0 | 4941 | `		ph7_result_null(pCtx);` |
|     ! 0 | 4942 | `		return PH7_OK;` |
|       - | 4943 | `	}` |
|       - | 4944 | `	/* Perform the requested operation */` |
|       7 | 4945 | `	pEntry = pSrc->pLast;` |
|      23 | 4946 | `	for( n = 0 ; n < pSrc->nEntry ; n++ ){` |
|      17 | 4947 | `		HashmapInsertNode((ph7_hashmap *)pArray->x.pOther,pEntry,bPreserve);` |
|       - | 4948 | `		/* Point to the previous entry */` |
|      17 | 4949 | `		pEntry = pEntry->pNext; /* Reverse link */` |
|       9 | 4950 | `	}` |
|       7 | 4951 | `	ph7_result_value(pCtx,pArray);` |
|       7 | 4952 | `	return PH7_OK;` |
|       4 | 4953 |  |
|       - | 4954 | `/*` |
|       - | 4955 | ` * array array_unique(array $array[,int $sort_flags = SORT_STRING ])` |
|       - | 4956 | ` *  Removes duplicate values from an array` |
|       - | 4957 | ` * Parameter` |
|       - | 4958 | ` *  $array` |
|       - | 4959 | ` *   The input array.` |
|       - | 4960 | ` *  $sort_flags` |
|       - | 4961 | ` *    The optional second parameter sort_flags may be used to modify the sorting behavior using these values:` |
|       - | 4962 | ` *    Sorting type flags:` |
|       - | 4963 | ` *       SORT_REGULAR - compare items normally (don't change types)` |
|       - | 4964 | ` *       SORT_NUMERIC - compare items numerically` |
|       - | 4965 | ` *       SORT_STRING - compare items as strings` |
|       - | 4966 | ` *       SORT_LOCALE_STRING - compare items as` |
|       - | 4967 | ` * Return` |
|       - | 4968 | ` *  Filtered array or NULL on failure.` |
|       - | 4969 | ` */` |
|       2 | 4970 | `static int ph7_hashmap_unique(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       1 | 4971 |  |
|       - | 4972 | `	ph7_hashmap_node *pEntry;` |
|       - | 4973 | `	ph7_value *pNeedle;` |
|       - | 4974 | `	ph7_hashmap *pSrc;` |
|       - | 4975 | `	ph7_value *pArray;` |
|       - | 4976 | `	int bStrict;` |
|       - | 4977 | `	sxi32 rc;` |
|       - | 4978 | `	sxu32 n;` |
|       3 | 4979 | `	if( nArg < 1 ){` |
|       - | 4980 | `		/* Missing arguments,return NULL */` |
|     ! 0 | 4981 | `		ph7_result_null(pCtx);` |
|     ! 0 | 4982 | `		return PH7_OK;` |
|       - | 4983 | `	}` |
|       - | 4984 | `	/* Make sure we are dealing with a valid hashmap */` |
|       3 | 4985 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|       - | 4986 | `		/* Invalid argument,return NULL */` |
|     ! 0 | 4987 | `		ph7_result_null(pCtx);` |
|     ! 0 | 4988 | `		return PH7_OK;` |
|       - | 4989 | `	}` |
|       3 | 4990 | `	bStrict = FALSE;` |
|       3 | 4991 | `	if( nArg > 1 ){` |
|     ! 0 | 4992 | `		bStrict = ph7_value_to_int(apArg[1]) == 3 /* SORT_REGULAR */ ? 1 : 0;` |
|     ! 0 | 4993 | `	}` |
|       - | 4994 | `	/* Point to the internal representation of the input hashmap */` |
|       3 | 4995 | `	pSrc = (ph7_hashmap *)apArg[0]->x.pOther;` |
|       - | 4996 | `	/* Create a new array */` |
|       3 | 4997 | `	pArray = ph7_context_new_array(pCtx);` |
|       3 | 4998 | `	if( pArray == 0 ){` |
|     ! 0 | 4999 | `		ph7_result_null(pCtx);` |
|     ! 0 | 5000 | `		return PH7_OK;` |
|       - | 5001 | `	}` |
|       - | 5002 | `	/* Perform the requested operation */` |
|       3 | 5003 | `	pEntry = pSrc->pFirst;` |
|      13 | 5004 | `	for( n = 0 ; n < pSrc->nEntry ; n++ ){` |
|      11 | 5005 | `		pNeedle = HashmapExtractNodeValue(pEntry);` |
|      11 | 5006 | `		rc = SXERR_NOTFOUND;` |
|      11 | 5007 | `		if( pNeedle ){` |
|      11 | 5008 | `			rc = HashmapFindValue((ph7_hashmap *)pArray->x.pOther,pNeedle,0,bStrict);` |
|       5 | 5009 | `		}` |
|      11 | 5010 | `		if( rc != SXRET_OK ){` |
|       - | 5011 | `			/* Perform the insertion */` |
|       7 | 5012 | `			HashmapInsertNode((ph7_hashmap *)pArray->x.pOther,pEntry,TRUE);` |
|       3 | 5013 | `		}` |
|       - | 5014 | `		/* Point to the next entry */` |
|      11 | 5015 | `		pEntry = pEntry->pPrev; /* Reverse link */` |
|       6 | 5016 | `	}` |
|       - | 5017 | `	/* Return the freshly created array */` |
|       3 | 5018 | `	ph7_result_value(pCtx,pArray);` |
|       3 | 5019 | `	return PH7_OK;` |
|       2 | 5020 |  |
|       - | 5021 | `/*` |
|       - | 5022 | ` * array array_flip(array $input)` |
|       - | 5023 | ` *  Exchanges all keys with their associated values in an array.` |
|       - | 5024 | ` * Parameter` |
|       - | 5025 | ` *  $input` |
|       - | 5026 | ` *   Input array.` |
|       - | 5027 | ` * Return` |
|       - | 5028 | ` *   The flipped array on success or NULL on failure.` |
|       - | 5029 | ` */` |
|      34 | 5030 | `static int ph7_hashmap_flip(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       2 | 5031 |  |
|       - | 5032 | `	ph7_hashmap_node *pEntry;` |
|       - | 5033 | `	ph7_hashmap *pSrc;` |
|       - | 5034 | `	ph7_value *pArray;` |
|       - | 5035 | `	ph7_value *pKey;` |
|       - | 5036 | `	ph7_value sVal;` |
|       - | 5037 | `	sxu32 n;` |
|       - | 5038 |  |
|       - | 5039 | `	/* PHP requires exactly one argument */` |
|      36 | 5040 | `	if( nArg != 1 ){` |
|       - | 5041 | `		/* Use ArgumentCountError like other array helpers */` |
|       7 | 5042 | `		return PH7_VmThrowException(pCtx,` |
|       - | 5043 | `			"ArgumentCountError",` |
|       - | 5044 | `			"array_flip() expects exactly 1 argument, %d given",` |
|       2 | 5045 | `			nArg` |
|       - | 5046 | `			);` |
|       - | 5047 | `	}` |
|       - | 5048 | `	/* Make sure we are dealing with a valid hashmap */` |
|      32 | 5049 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|       - | 5050 | `		/* Type mismatch -> TypeError */` |
|       7 | 5051 | `		return PH7_VmThrowException(pCtx,` |
|       - | 5052 | `			"TypeError",` |
|       - | 5053 | `			"array_flip(): Argument #1 ($array) must be of type array, %s given",` |
|       2 | 5054 | `			ph7_type_name(apArg[0])` |
|       - | 5055 | `			);` |
|       - | 5056 | `	}` |
|       - | 5057 | `	/* Point to the internal representation of the input hashmap */` |
|      27 | 5058 | `	pSrc = (ph7_hashmap *)apArg[0]->x.pOther;` |
|       - | 5059 | `	/* Create a new array */` |
|      27 | 5060 | `	pArray = ph7_context_new_array(pCtx);` |
|      27 | 5061 | `	if( pArray == 0 ){` |
|     ! 0 | 5062 | `		ph7_result_null(pCtx);` |
|     ! 0 | 5063 | `		return PH7_OK;` |
|       - | 5064 | `	}` |
|       - | 5065 | `	/* Start processing */` |
|      27 | 5066 | `	pEntry = pSrc->pFirst;` |
|   22263 | 5067 | `	for( n = 0 ; n < pSrc->nEntry ; n++ ){` |
|       - | 5068 | `		/* Extract the node value (will become a key in the result) */` |
|   22237 | 5069 | `		pKey = HashmapExtractNodeValue(pEntry);` |
|   22237 | 5070 | `		if( pKey ){` |
|       - | 5071 | `			/* NULL values are not valid keys either, PHP emits a warning */` |
|   22237 | 5072 | `			if( pKey->iFlags & MEMOBJ_NULL ){` |
|       3 | 5073 | `				PH7_VmThrowError(pCtx->pVm,0,PH7_CTX_WARNING,` |
|       - | 5074 | `					"array_flip(): Can only flip string and integer values, entry skipped"` |
|       - | 5075 | `					);` |
|   22236 | 5076 | `			}else if( (pKey->iFlags & MEMOBJ_INT) \|\| (pKey->iFlags & MEMOBJ_STRING) ){` |
|       - | 5077 | `				/* Prepare the value for insertion (original key) */` |
|   22227 | 5078 | `				if( pEntry->iType == HASHMAP_INT_NODE ){` |
|   20001 | 5079 | `					PH7_MemObjInitFromInt(pSrc->pVm,&sVal,pEntry->xKey.iKey);` |
|   10001 | 5080 | `				}else{` |
|       - | 5081 | `					SyString sStr;` |
|    2227 | 5082 | `					SyStringInitFromBuf(&sStr,SyBlobData(&pEntry->xKey.sKey),SyBlobLength(&pEntry->xKey.sKey));` |
|    2227 | 5083 | `					PH7_MemObjInitFromString(pSrc->pVm,&sVal,&sStr);` |
|       - | 5084 | `				}` |
|       - | 5085 | `				/* Perform the insertion */` |
|   22227 | 5086 | `				ph7_array_add_elem(pArray,pKey,&sVal);` |
|       - | 5087 | `				/* Safely release the value because each inserted entry` |
|       - | 5088 | `				 * has its own private copy of the value.` |
|       - | 5089 | `				 */` |
|   22227 | 5090 | `				PH7_MemObjRelease(&sVal);` |
|   11114 | 5091 | `			}else{` |
|       - | 5092 | `				/* Unsupported value type -> emit warning and skip the entry */` |
|       9 | 5093 | `				PH7_VmThrowError(pCtx->pVm,0,PH7_CTX_WARNING,` |
|       - | 5094 | `					"array_flip(): Can only flip string and integer values, entry skipped"` |
|       - | 5095 | `					);` |
|       - | 5096 | `			}` |
|   11118 | 5097 | `		}` |
|       - | 5098 | `		/* Point to the next entry */` |
|   22237 | 5099 | `		pEntry = pEntry->pPrev; /* Reverse link */` |
|   11119 | 5100 | `	}` |
|       - | 5101 | `	/* Return the freshly created array */` |
|      27 | 5102 | `	ph7_result_value(pCtx,pArray);` |
|      27 | 5103 | `	return PH7_OK;` |
|      19 | 5104 |  |
|       - | 5105 | `/*` |
|       - | 5106 | ` * number array_sum(array $array )` |
|       - | 5107 | ` *  Calculate the sum of values in an array.` |
|       - | 5108 | ` * Parameters` |
|       - | 5109 | ` *  $array: The input array.` |
|       - | 5110 | ` * Return` |
|       - | 5111 | ` *  Returns the sum of values as an integer or float.` |
|       - | 5112 | ` */` |
|      24 | 5113 | `static void DoubleSum(ph7_context *pCtx,ph7_hashmap *pMap)` |
|       1 | 5114 |  |
|       - | 5115 | `	ph7_hashmap_node *pEntry;` |
|       - | 5116 | `	ph7_value *pObj;` |
|      25 | 5117 | `	double dSum = 0;` |
|       - | 5118 | `	sxu32 n;` |
|      25 | 5119 | `	pEntry = pMap->pFirst;` |
|      91 | 5120 | `	for( n = 0 ; n < pMap->nEntry ; n++ ){` |
|      67 | 5121 | `		pObj = HashmapExtractNodeValue(pEntry);` |
|      67 | 5122 | `		if( pObj ){` |
|      67 | 5123 | `			if( pObj->iFlags & MEMOBJ_REAL ){` |
|      29 | 5124 | `				dSum += pObj->rVal;` |
|      53 | 5125 | `			}else if( pObj->iFlags & (MEMOBJ_INT\|MEMOBJ_BOOL) ){` |
|      21 | 5126 | `				dSum += (double)pObj->x.iVal;` |
|      29 | 5127 | `			}else if( pObj->iFlags & MEMOBJ_STRING ){` |
|      15 | 5128 | `				if( SyBlobLength(&pObj->sBlob) > 0 ){` |
|      13 | 5129 | `					double dv = 0;` |
|      13 | 5130 | `					SyStrToReal((const char *)SyBlobData(&pObj->sBlob),SyBlobLength(&pObj->sBlob),(void *)&dv,0);` |
|      13 | 5131 | `					dSum += dv;` |
|       7 | 5132 | `				}` |
|      12 | 5133 | `			}else if( pObj->iFlags & MEMOBJ_HASHMAP ){` |
|       3 | 5134 | `				PH7_VmThrowError(pCtx->pVm,0,PH7_CTX_WARNING,` |
|       - | 5135 | `					"array_sum(): Addition is not supported on type array");` |
|       4 | 5136 | `			}else if( pObj->iFlags & MEMOBJ_OBJ ){` |
|     ! 0 | 5137 | `				PH7_VmThrowError(pCtx->pVm,0,PH7_CTX_WARNING,` |
|       - | 5138 | `					"array_sum(): Addition is not supported on type object");` |
|       3 | 5139 | `			}else if( pObj->iFlags & MEMOBJ_RES ){` |
|     ! 0 | 5140 | `				PH7_VmThrowError(pCtx->pVm,0,PH7_CTX_WARNING,` |
|       - | 5141 | `					"array_sum(): Addition is not supported on type resource");` |
|     ! 0 | 5142 | `			}` |
|       - | 5143 | `			/* NULL is silently treated as 0 (matches PHP) */` |
|      33 | 5144 | `		}` |
|       - | 5145 | `		/* Point to the next entry */` |
|      67 | 5146 | `		pEntry = pEntry->pPrev; /* Reverse link */` |
|      34 | 5147 | `	}` |
|       - | 5148 | `	/* Return sum */` |
|      25 | 5149 | `	ph7_result_double(pCtx,dSum);` |
|      25 | 5150 |  |
|      18 | 5151 | `static void Int64Sum(ph7_context *pCtx,ph7_hashmap *pMap)` |
|       2 | 5152 |  |
|       - | 5153 | `	ph7_hashmap_node *pEntry;` |
|       - | 5154 | `	ph7_value *pObj;` |
|      20 | 5155 | `	sxi64 nSum = 0;` |
|       - | 5156 | `	sxu32 n;` |
|      20 | 5157 | `	pEntry = pMap->pFirst;` |
|      80 | 5158 | `	for( n = 0 ; n < pMap->nEntry ; n++ ){` |
|      62 | 5159 | `		pObj = HashmapExtractNodeValue(pEntry);` |
|      62 | 5160 | `		if( pObj ){` |
|      62 | 5161 | `			if( pObj->iFlags & (MEMOBJ_INT\|MEMOBJ_BOOL) ){` |
|      52 | 5162 | `				nSum += pObj->x.iVal;` |
|      36 | 5163 | `			}else if( pObj->iFlags & MEMOBJ_STRING ){` |
|       7 | 5164 | `				if( SyBlobLength(&pObj->sBlob) > 0 ){` |
|       5 | 5165 | `					sxi64 nv = 0;` |
|       5 | 5166 | `					SyStrToInt64((const char *)SyBlobData(&pObj->sBlob),SyBlobLength(&pObj->sBlob),(void *)&nv,0);` |
|       5 | 5167 | `					nSum += nv;` |
|       3 | 5168 | `				}` |
|       8 | 5169 | `			}else if( pObj->iFlags & MEMOBJ_HASHMAP ){` |
|       3 | 5170 | `				PH7_VmThrowError(pCtx->pVm,0,PH7_CTX_WARNING,` |
|       - | 5171 | `					"array_sum(): Addition is not supported on type array");` |
|       4 | 5172 | `			}else if( pObj->iFlags & MEMOBJ_OBJ ){` |
|     ! 0 | 5173 | `				PH7_VmThrowError(pCtx->pVm,0,PH7_CTX_WARNING,` |
|       - | 5174 | `					"array_sum(): Addition is not supported on type object");` |
|       3 | 5175 | `			}else if( pObj->iFlags & MEMOBJ_RES ){` |
|     ! 0 | 5176 | `				PH7_VmThrowError(pCtx->pVm,0,PH7_CTX_WARNING,` |
|       - | 5177 | `					"array_sum(): Addition is not supported on type resource");` |
|     ! 0 | 5178 | `			}` |
|       - | 5179 | `			/* NULL is silently treated as 0 (matches PHP) */` |
|      30 | 5180 | `		}` |
|       - | 5181 | `		/* Point to the next entry */` |
|      62 | 5182 | `		pEntry = pEntry->pPrev; /* Reverse link */` |
|      32 | 5183 | `	}` |
|       - | 5184 | `	/* Return sum */` |
|      20 | 5185 | `	ph7_result_int64(pCtx,nSum);` |
|      20 | 5186 |  |
|       - | 5187 | `/* number array_sum(array $array )` |
|       - | 5188 | ` * (See block-coment above)` |
|       - | 5189 | ` */` |
|      52 | 5190 | `static int ph7_hashmap_sum(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       2 | 5191 |  |
|       - | 5192 | `	ph7_hashmap_node *pEntry;` |
|       - | 5193 | `	ph7_hashmap *pMap;` |
|       - | 5194 | `	ph7_value *pObj;` |
|      54 | 5195 | `	int useDouble = 0;` |
|       - | 5196 | `	sxu32 n;` |
|       - | 5197 | `	/* PHP requires exactly one argument */` |
|      54 | 5198 | `	if( nArg != 1 ){` |
|       7 | 5199 | `		return PH7_VmThrowException(pCtx,` |
|       - | 5200 | `			"ArgumentCountError",` |
|       - | 5201 | `			"array_sum() expects exactly 1 argument, %d given",` |
|       2 | 5202 | `			nArg` |
|       - | 5203 | `			);` |
|       - | 5204 | `	}` |
|       - | 5205 | `	/* Make sure we are dealing with a valid hashmap */` |
|      50 | 5206 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|       - | 5207 | `		/* Type mismatch -> TypeError */` |
|       7 | 5208 | `		return PH7_VmThrowException(pCtx,` |
|       - | 5209 | `			"TypeError",` |
|       - | 5210 | `			"array_sum(): Argument #1 ($array) must be of type array, %s given",` |
|       2 | 5211 | `			ph7_type_name(apArg[0])` |
|       - | 5212 | `			);` |
|       - | 5213 | `	}` |
|      46 | 5214 | `	pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|      46 | 5215 | `	if( pMap->nEntry < 1 ){` |
|       - | 5216 | `		/* Nothing to compute,return 0 */` |
|       3 | 5217 | `		ph7_result_int(pCtx,0);` |
|       3 | 5218 | `		return PH7_OK;` |
|       - | 5219 | `	}` |
|       - | 5220 | `	/* Scan all elements: if any value is a float, use floating-point` |
|       - | 5221 | `	 * arithmetic for the entire sum (matches PHP behaviour).` |
|       - | 5222 | `	 */` |
|      44 | 5223 | `	pEntry = pMap->pFirst;` |
|     112 | 5224 | `	for( n = 0 ; n < pMap->nEntry ; n++ ){` |
|      94 | 5225 | `		pObj = HashmapExtractNodeValue(pEntry);` |
|      94 | 5226 | `		if( pObj ){` |
|      94 | 5227 | `			if( pObj->iFlags & MEMOBJ_REAL ){` |
|      19 | 5228 | `				useDouble = 1;` |
|      19 | 5229 | `				break;` |
|       - | 5230 | `			}` |
|      76 | 5231 | `			if( pObj->iFlags & MEMOBJ_STRING ){` |
|      13 | 5232 | `				const char *zStr = (const char *)SyBlobData(&pObj->sBlob);` |
|      13 | 5233 | `				sxu32 nLen = SyBlobLength(&pObj->sBlob);` |
|       - | 5234 | `				sxu32 i;` |
|      23 | 5235 | `				for( i = 0 ; i < nLen ; i++ ){` |
|      17 | 5236 | `					if( zStr[i] == '.' \|\| zStr[i] == 'e' \|\| zStr[i] == 'E' ){` |
|       7 | 5237 | `						useDouble = 1;` |
|       7 | 5238 | `						break;` |
|       - | 5239 | `					}` |
|       6 | 5240 | `				}` |
|      13 | 5241 | `				if( useDouble ){` |
|       7 | 5242 | `					break;` |
|       - | 5243 | `				}` |
|       3 | 5244 | `			}` |
|      34 | 5245 | `		}` |
|      70 | 5246 | `		pEntry = pEntry->pPrev;` |
|      36 | 5247 | `	}` |
|      44 | 5248 | `	if( useDouble ){` |
|      25 | 5249 | `		DoubleSum(pCtx,pMap);` |
|      13 | 5250 | `	}else{` |
|      20 | 5251 | `		Int64Sum(pCtx,pMap);` |
|       - | 5252 | `	}` |
|      44 | 5253 | `	return PH7_OK;` |
|      28 | 5254 |  |
|       - | 5255 | `/*` |
|       - | 5256 | ` * number array_product(array $array )` |
|       - | 5257 | ` *  Calculate the product of values in an array.` |
|       - | 5258 | ` * Parameters` |
|       - | 5259 | ` *  $array: The input array.` |
|       - | 5260 | ` * Return` |
|       - | 5261 | ` *  Returns the product of values as an integer or float.` |
|       - | 5262 | ` */` |
|     ! 0 | 5263 | `static void DoubleProd(ph7_context *pCtx,ph7_hashmap *pMap)` |
|     ! 0 | 5264 |  |
|       - | 5265 | `	ph7_hashmap_node *pEntry;` |
|       - | 5266 | `	ph7_value *pObj;` |
|       - | 5267 | `	double dProd;` |
|       - | 5268 | `	sxu32 n;` |
|     ! 0 | 5269 | `	pEntry = pMap->pFirst;` |
|     ! 0 | 5270 | `	dProd = 1;` |
|     ! 0 | 5271 | `	for( n = 0 ; n < pMap->nEntry ; n++ ){` |
|     ! 0 | 5272 | `		pObj = HashmapExtractNodeValue(pEntry);` |
|     ! 0 | 5273 | `		if( pObj && (pObj->iFlags & (MEMOBJ_NULL\|MEMOBJ_HASHMAP\|MEMOBJ_OBJ\|MEMOBJ_RES)) == 0){` |
|     ! 0 | 5274 | `			if( pObj->iFlags & MEMOBJ_REAL ){` |
|     ! 0 | 5275 | `				dProd *= pObj->rVal;` |
|     ! 0 | 5276 | `			}else if( pObj->iFlags & (MEMOBJ_INT\|MEMOBJ_BOOL) ){` |
|     ! 0 | 5277 | `				dProd *= (double)pObj->x.iVal;` |
|     ! 0 | 5278 | `			}else if( pObj->iFlags & MEMOBJ_STRING ){` |
|     ! 0 | 5279 | `				if( SyBlobLength(&pObj->sBlob) > 0 ){` |
|     ! 0 | 5280 | `					double dv = 0;` |
|     ! 0 | 5281 | `					SyStrToReal((const char *)SyBlobData(&pObj->sBlob),SyBlobLength(&pObj->sBlob),(void *)&dv,0);` |
|     ! 0 | 5282 | `					dProd *= dv;` |
|     ! 0 | 5283 | `				}` |
|     ! 0 | 5284 | `			}` |
|     ! 0 | 5285 | `		}` |
|       - | 5286 | `		/* Point to the next entry */` |
|     ! 0 | 5287 | `		pEntry = pEntry->pPrev; /* Reverse link */` |
|     ! 0 | 5288 | `	}` |
|       - | 5289 | `	/* Return product */` |
|     ! 0 | 5290 | `	ph7_result_double(pCtx,dProd);` |
|     ! 0 | 5291 |  |
|     ! 0 | 5292 | `static void Int64Prod(ph7_context *pCtx,ph7_hashmap *pMap)` |
|     ! 0 | 5293 |  |
|       - | 5294 | `	ph7_hashmap_node *pEntry;` |
|       - | 5295 | `	ph7_value *pObj;` |
|       - | 5296 | `	sxi64 nProd;` |
|       - | 5297 | `	sxu32 n;` |
|     ! 0 | 5298 | `	pEntry = pMap->pFirst;` |
|     ! 0 | 5299 | `	nProd = 1;` |
|     ! 0 | 5300 | `	for( n = 0 ; n < pMap->nEntry ; n++ ){` |
|     ! 0 | 5301 | `		pObj = HashmapExtractNodeValue(pEntry);` |
|     ! 0 | 5302 | `		if( pObj && (pObj->iFlags & (MEMOBJ_NULL\|MEMOBJ_HASHMAP\|MEMOBJ_OBJ\|MEMOBJ_RES)) == 0){` |
|     ! 0 | 5303 | `			if( pObj->iFlags & MEMOBJ_REAL ){` |
|     ! 0 | 5304 | `				nProd *= (sxi64)pObj->rVal;` |
|     ! 0 | 5305 | `			}else if( pObj->iFlags & (MEMOBJ_INT\|MEMOBJ_BOOL) ){` |
|     ! 0 | 5306 | `				nProd *= pObj->x.iVal;` |
|     ! 0 | 5307 | `			}else if( pObj->iFlags & MEMOBJ_STRING ){` |
|     ! 0 | 5308 | `				if( SyBlobLength(&pObj->sBlob) > 0 ){` |
|     ! 0 | 5309 | `					sxi64 nv = 0;` |
|     ! 0 | 5310 | `					SyStrToInt64((const char *)SyBlobData(&pObj->sBlob),SyBlobLength(&pObj->sBlob),(void *)&nv,0);` |
|     ! 0 | 5311 | `					nProd *= nv;` |
|     ! 0 | 5312 | `				}` |
|     ! 0 | 5313 | `			}` |
|     ! 0 | 5314 | `		}` |
|       - | 5315 | `		/* Point to the next entry */` |
|     ! 0 | 5316 | `		pEntry = pEntry->pPrev; /* Reverse link */` |
|     ! 0 | 5317 | `	}` |
|       - | 5318 | `	/* Return product */` |
|     ! 0 | 5319 | `	ph7_result_int64(pCtx,nProd);` |
|     ! 0 | 5320 |  |
|       - | 5321 | `/* number array_product(array $array )` |
|       - | 5322 | ` * (See block-block comment above)` |
|       - | 5323 | ` */` |
|     ! 0 | 5324 | `static int ph7_hashmap_product(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     ! 0 | 5325 |  |
|       - | 5326 | `	ph7_hashmap *pMap;` |
|       - | 5327 | `	ph7_value *pObj;` |
|     ! 0 | 5328 | `	if( nArg < 1 ){` |
|       - | 5329 | `		/* Missing arguments,return 0 */` |
|     ! 0 | 5330 | `		ph7_result_int(pCtx,0);` |
|     ! 0 | 5331 | `		return PH7_OK;` |
|       - | 5332 | `	}` |
|       - | 5333 | `	/* Make sure we are dealing with a valid hashmap */` |
|     ! 0 | 5334 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|       - | 5335 | `		/* Invalid argument,return 0 */` |
|     ! 0 | 5336 | `		ph7_result_int(pCtx,0);` |
|     ! 0 | 5337 | `		return PH7_OK;` |
|       - | 5338 | `	}` |
|     ! 0 | 5339 | `	pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|     ! 0 | 5340 | `	if( pMap->nEntry < 1 ){` |
|       - | 5341 | `		/* Nothing to compute,return 0 */` |
|     ! 0 | 5342 | `		ph7_result_int(pCtx,0);` |
|     ! 0 | 5343 | `		return PH7_OK;` |
|       - | 5344 | `	}` |
|       - | 5345 | `	/* If the first element is of type float,then perform floating` |
|       - | 5346 | `	 * point computaion.Otherwise switch to int64 computaion.` |
|       - | 5347 | `	 */` |
|     ! 0 | 5348 | `	pObj = HashmapExtractNodeValue(pMap->pFirst);` |
|     ! 0 | 5349 | `	if( pObj == 0 ){` |
|     ! 0 | 5350 | `		ph7_result_int(pCtx,0);` |
|     ! 0 | 5351 | `		return PH7_OK;` |
|       - | 5352 | `	}` |
|     ! 0 | 5353 | `	if( pObj->iFlags & MEMOBJ_REAL ){` |
|     ! 0 | 5354 | `		DoubleProd(pCtx,pMap);` |
|     ! 0 | 5355 | `	}else{` |
|     ! 0 | 5356 | `		Int64Prod(pCtx,pMap);` |
|       - | 5357 | `	}` |
|     ! 0 | 5358 | `	return PH7_OK;` |
|     ! 0 | 5359 |  |
|       - | 5360 | `/*` |
|       - | 5361 | ` * value array_rand(array $input[,int $num_req = 1 ])` |
|       - | 5362 | ` *  Pick one or more random entries out of an array.` |
|       - | 5363 | ` * Parameters` |
|       - | 5364 | ` * $input` |
|       - | 5365 | ` *  The input array.` |
|       - | 5366 | ` * $num_req` |
|       - | 5367 | ` *  Specifies how many entries you want to pick.` |
|       - | 5368 | ` * Return` |
|       - | 5369 | ` *  If you are picking only one entry, array_rand() returns the key for a random entry.` |
|       - | 5370 | ` *  Otherwise, it returns an array of keys for the random entries.` |
|       - | 5371 | ` *  NULL is returned on failure.` |
|       - | 5372 | ` */` |
|       6 | 5373 | `static int ph7_hashmap_rand(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       1 | 5374 |  |
|       - | 5375 | `	ph7_hashmap_node *pNode;` |
|       - | 5376 | `	ph7_hashmap *pMap;` |
|       7 | 5377 | `	int nItem = 1;` |
|       7 | 5378 | `	if( nArg < 1 ){` |
|       - | 5379 | `		/* Missing argument,return NULL */` |
|     ! 0 | 5380 | `		ph7_result_null(pCtx);` |
|     ! 0 | 5381 | `		return PH7_OK;` |
|       - | 5382 | `	}` |
|       - | 5383 | `	/* Make sure we are dealing with an array */` |
|       7 | 5384 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|     ! 0 | 5385 | `		ph7_result_null(pCtx);` |
|     ! 0 | 5386 | `		return PH7_OK;` |
|       - | 5387 | `	}` |
|       - | 5388 | `	/* Point to the internal representation of the input hashmap */` |
|       7 | 5389 | `	pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|       7 | 5390 | `	if(pMap->nEntry < 1 ){` |
|       - | 5391 | `		/* Empty hashmap,return NULL */` |
|     ! 0 | 5392 | `		ph7_result_null(pCtx);` |
|     ! 0 | 5393 | `		return PH7_OK;` |
|       - | 5394 | `	}` |
|       7 | 5395 | `	if( nArg > 1 ){` |
|       3 | 5396 | `		nItem = ph7_value_to_int(apArg[1]);` |
|       1 | 5397 | `	}` |
|       7 | 5398 | `	if( nItem < 2 ){` |
|       - | 5399 | `		sxu32 nEntry;` |
|       - | 5400 | `		/* Select a random number */` |
|       5 | 5401 | `		nEntry = PH7_VmRandomNum(pMap->pVm) % pMap->nEntry;` |
|       - | 5402 | `		/* Extract the desired entry.` |
|       - | 5403 | `		 * Note that we perform a linear lookup here (later version must change this)` |
|       - | 5404 | `		 */` |
|       5 | 5405 | `		if( nEntry > pMap->nEntry / 2 ){` |
|       4 | 5406 | `			pNode = pMap->pLast;` |
|       4 | 5407 | `			nEntry = pMap->nEntry - nEntry;` |
|       4 | 5408 | `			if( nEntry > 1 ){` |
|     ! 0 | 5409 | `				for(;;){` |
|     ! 0 | 5410 | `					if( nEntry == 0 ){` |
|     ! 0 | 5411 | `						break;` |
|       - | 5412 | `					}` |
|       - | 5413 | `					/* Point to the previous entry */` |
|     ! 0 | 5414 | `					pNode = pNode->pNext; /* Reverse link */` |
|     ! 0 | 5415 | `					nEntry--;` |
|     ! 0 | 5416 | `				}` |
|     ! 0 | 5417 | `			}` |
|       2 | 5418 | `		}else{` |
|       1 | 5419 | `			pNode = pMap->pFirst;` |
|     ! 0 | 5420 | `			for(;;){` |
|       1 | 5421 | `				if( nEntry == 0 ){` |
|       1 | 5422 | `					break;` |
|       - | 5423 | `				}` |
|       - | 5424 | `				/* Point to the next entry */` |
|       1 | 5425 | `				pNode = pNode->pPrev; /* Reverse link */` |
|       1 | 5426 | `				nEntry--;` |
|       1 | 5427 | `			}` |
|       - | 5428 | `		}` |
|       5 | 5429 | `		if( pNode->iType == HASHMAP_INT_NODE ){` |
|       - | 5430 | `			/* Int key */` |
|       3 | 5431 | `			ph7_result_int64(pCtx,pNode->xKey.iKey);` |
|       2 | 5432 | `		}else{` |
|       - | 5433 | `			/* Blob key */` |
|       3 | 5434 | `			ph7_result_string(pCtx,(const char *)SyBlobData(&pNode->xKey.sKey),(int)SyBlobLength(&pNode->xKey.sKey));` |
|       - | 5435 | `		}` |
|       3 | 5436 | `	}else{` |
|       - | 5437 | `		ph7_value sKey,*pArray;` |
|       - | 5438 | `		ph7_hashmap *pDest;` |
|       - | 5439 | `		/* Create a new array */` |
|       3 | 5440 | `		pArray = ph7_context_new_array(pCtx);` |
|       3 | 5441 | `		if( pArray == 0 ){` |
|     ! 0 | 5442 | `			ph7_result_null(pCtx);` |
|     ! 0 | 5443 | `			return PH7_OK;` |
|       - | 5444 | `		}` |
|       - | 5445 | `		/* Point to the internal representation of the hashmap */` |
|       3 | 5446 | `		pDest = (ph7_hashmap *)pArray->x.pOther;` |
|       3 | 5447 | `		PH7_MemObjInit(pDest->pVm,&sKey);` |
|       - | 5448 | `		/* Copy the first n items */` |
|       3 | 5449 | `		pNode = pMap->pFirst;` |
|       3 | 5450 | `		if( nItem > (int)pMap->nEntry ){` |
|     ! 0 | 5451 | `			nItem = (int)pMap->nEntry;` |
|     ! 0 | 5452 | `		}` |
|       7 | 5453 | `		while( nItem > 0){` |
|       5 | 5454 | `			PH7_HashmapExtractNodeKey(pNode,&sKey);` |
|       5 | 5455 | `			PH7_HashmapInsert(pDest,0/* Automatic index assign*/,&sKey);` |
|       5 | 5456 | `			PH7_MemObjRelease(&sKey);` |
|       - | 5457 | `			/* Point to the next entry */` |
|       5 | 5458 | `			pNode = pNode->pPrev; /* Reverse link */` |
|       5 | 5459 | `			nItem--;` |
|       1 | 5460 | `		}` |
|       - | 5461 | `		/* Shuffle the array */` |
|       3 | 5462 | `		HashmapMergeSort(pDest,HashmapCmpCallback7,0);` |
|       - | 5463 | `		/* Rehash node */` |
|       3 | 5464 | `		HashmapSortRehash(pDest);` |
|       - | 5465 | `		/* Return the random array */` |
|       3 | 5466 | `		ph7_result_value(pCtx,pArray);` |
|       - | 5467 | `	}` |
|       7 | 5468 | `	return PH7_OK;` |
|       4 | 5469 |  |
|       - | 5470 | `/*` |
|       - | 5471 | ` * array array_chunk (array $input,int $size [,bool $preserve_keys = false ])` |
|       - | 5472 | ` *  Split an array into chunks.` |
|       - | 5473 | ` * Parameters` |
|       - | 5474 | ` * $input` |
|       - | 5475 | ` *   The array to work on` |
|       - | 5476 | ` * $size` |
|       - | 5477 | ` *   The size of each chunk` |
|       - | 5478 | ` * $preserve_keys` |
|       - | 5479 | ` *   When set to TRUE keys will be preserved. Default is FALSE which will reindex` |
|       - | 5480 | ` *   the chunk numerically.` |
|       - | 5481 | ` * Return` |
|       - | 5482 | ` *  Returns a multidimensional numerically indexed array, starting with` |
|       - | 5483 | ` *  zero, with each dimension containing size elements.` |
|       - | 5484 | ` */` |
|      42 | 5485 | `static int ph7_hashmap_chunk(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       2 | 5486 |  |
|       - | 5487 | `	ph7_value *pArray,*pChunk;` |
|       - | 5488 | `	ph7_hashmap_node *pEntry;` |
|       - | 5489 | `	ph7_hashmap *pMap;` |
|       - | 5490 | `	int bPreserve;` |
|       - | 5491 | `	sxu32 nChunk;` |
|       - | 5492 | `	sxu32 nSize;` |
|       - | 5493 | `	sxu32 n;` |
|       - | 5494 | `	/* Argument count and types follow PHP semantics. */` |
|      44 | 5495 | `	if( nArg < 2 ){` |
|       - | 5496 | `		/* fewer than required arguments -> ArgumentCountError */` |
|       4 | 5497 | `		return PH7_VmThrowException(pCtx,` |
|       - | 5498 | `			"ArgumentCountError",` |
|       - | 5499 | `			"array_chunk() expects at least 2 arguments, %d given",` |
|       1 | 5500 | `			nArg` |
|       - | 5501 | `			);` |
|       - | 5502 | `	}` |
|      42 | 5503 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|       4 | 5504 | `		return PH7_VmThrowException(pCtx,` |
|       - | 5505 | `			"TypeError",` |
|       - | 5506 | `			"array_chunk(): Argument #1 ($array) must be of type array, %s given",` |
|       1 | 5507 | `			ph7_type_name(apArg[0])` |
|       - | 5508 | `			);` |
|       - | 5509 | `	}` |
|       - | 5510 | `	/* Create a new array */` |
|      40 | 5511 | `	pArray = ph7_context_new_array(pCtx);` |
|      40 | 5512 | `	if( pArray == 0 ){` |
|     ! 0 | 5513 | `		ph7_result_null(pCtx);` |
|     ! 0 | 5514 | `		return PH7_OK;` |
|       - | 5515 | `	}` |
|       - | 5516 | `	/* Point to the internal representation of the input hashmap */` |
|      40 | 5517 | `	pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|       - | 5518 | `	/* Extract and validate the chunk size argument. */` |
|       - | 5519 | `	/* Reject types that cannot be sensibly converted to an integer. */` |
|      76 | 5520 | `	if( ph7_value_is_array(apArg[1]) \|\| ph7_value_is_object(apArg[1]) \|\|` |
|      78 | 5521 | `		ph7_value_is_resource(apArg[1]) \|\| ph7_value_is_null(apArg[1]) \|\|` |
|      38 | 5522 | `		ph7_value_is_bool(apArg[1]) ){` |
|     ! 0 | 5523 | `		return PH7_VmThrowException(pCtx,` |
|       - | 5524 | `			"TypeError",` |
|       - | 5525 | `			"array_chunk(): Argument #2 ($length) must be of type int, %s given",` |
|     ! 0 | 5526 | `			ph7_type_name(apArg[1])` |
|       - | 5527 | `			);` |
|       - | 5528 | `	}` |
|       - | 5529 | `	/* Strings that are non-numeric produce a TypeError.  Numeric` |
|       - | 5530 | `	 * strings are permitted; however those representing floats lose` |
|       - | 5531 | `	 * precision and PHP emits a deprecation warning. */` |
|      40 | 5532 | `	if( ph7_value_is_string(apArg[1]) ){` |
|       - | 5533 | `		int len;` |
|       3 | 5534 | `		sxu8 bReal = FALSE;` |
|       3 | 5535 | `		const char *zStr = ph7_value_to_string(apArg[1], &len);` |
|       3 | 5536 | `		if( SyStrIsNumeric(zStr, len, &bReal, 0) != SXRET_OK ){` |
|       3 | 5537 | `			return PH7_VmThrowException(pCtx,` |
|       - | 5538 | `				"TypeError",` |
|       - | 5539 | `				"array_chunk(): Argument #2 ($length) must be of type int, string given"` |
|       - | 5540 | `				);` |
|       - | 5541 | `		}` |
|     ! 0 | 5542 | `		if( bReal ){` |
|       - | 5543 | `			/* float-string -> warn but allow */` |
|     ! 0 | 5544 | `			ph7_context_throw_error_format(pCtx, E_DEPRECATED,` |
|       - | 5545 | `				"Implicit conversion from float-string \"%s\" to int loses precision",` |
|     ! 0 | 5546 | `				zStr` |
|       - | 5547 | `				);` |
|     ! 0 | 5548 | `		}` |
|     ! 0 | 5549 | `	}` |
|       - | 5550 | `	/* If the value is a float with a fractional component, emit a` |
|       - | 5551 | `	 * deprecation warning but continue.  The following conversion occurs` |
|       - | 5552 | `	 * later via ph7_value_to_int. */` |
|      38 | 5553 | `	if( ph7_value_is_float(apArg[1]) ){` |
|       3 | 5554 | `		double d = ph7_value_to_double(apArg[1]);` |
|       3 | 5555 | `		sxi64 i = (sxi64)d;` |
|       3 | 5556 | `		if( d != (double)i ){` |
|       4 | 5557 | `			ph7_context_throw_error_format(pCtx, E_DEPRECATED,` |
|       - | 5558 | `				"Implicit conversion from float %g to int loses precision",` |
|       1 | 5559 | `				d` |
|       - | 5560 | `				);` |
|       1 | 5561 | `		}` |
|       1 | 5562 | `	}` |
|       - | 5563 | `	/* Convert using ph7_value_to_int; now that float fractions are` |
|       - | 5564 | `	 * eliminated, this will not produce a warning. */` |
|       - | 5565 | `	{` |
|      38 | 5566 | `		sxi64 nSizeSigned = ph7_value_to_int(apArg[1]);` |
|      38 | 5567 | `		if( nSizeSigned < 1 ){` |
|       - | 5568 | `			/* size <= 0 -> ValueError */` |
|       5 | 5569 | `			return PH7_VmThrowException(pCtx,` |
|       - | 5570 | `				"ValueError",` |
|       - | 5571 | `				"array_chunk(): Argument #2 ($length) must be greater than 0"` |
|       - | 5572 | `				);` |
|       - | 5573 | `		}` |
|      34 | 5574 | `		nSize = (sxu32)nSizeSigned;` |
|       - | 5575 | `	}` |
|      34 | 5576 | `	if( nSize >= pMap->nEntry ){` |
|       - | 5577 | `		/* Return the whole array */` |
|       3 | 5578 | `		ph7_array_add_elem(pArray,0,apArg[0]);` |
|       3 | 5579 | `		ph7_result_value(pCtx,pArray);` |
|       3 | 5580 | `		return PH7_OK;` |
|       - | 5581 | `	}` |
|      32 | 5582 | `	bPreserve = 0;` |
|      32 | 5583 | `	if( nArg > 2 ){` |
|       - | 5584 | `		/* The third argument has a bool type hint in PHP.  Values that` |
|       - | 5585 | `		 * cannot be sensibly converted (arrays, objects, resources) are` |
|       - | 5586 | `		 * rejected with a TypeError.  Scalars and null coerce to bool` |
|       - | 5587 | `		 * normally, matching PHP behaviour. */` |
|      45 | 5588 | `		if( ph7_value_is_array(apArg[2]) \|\|` |
|      34 | 5589 | `			ph7_value_is_object(apArg[2]) \|\|` |
|      20 | 5590 | `			ph7_value_is_resource(apArg[2]) ){` |
|       7 | 5591 | `			return PH7_VmThrowException(pCtx,` |
|       - | 5592 | `				"TypeError",` |
|       - | 5593 | `				"array_chunk(): Argument #3 ($preserve_keys) must be of type bool, %s given",` |
|       4 | 5594 | `				ph7_type_name(apArg[2])` |
|       - | 5595 | `				);` |
|       - | 5596 | `		}` |
|      21 | 5597 | `		bPreserve = ph7_value_to_bool(apArg[2]);` |
|      10 | 5598 | `	}` |
|       - | 5599 | `	/* Start processing */` |
|      27 | 5600 | `	pEntry = pMap->pFirst;` |
|      27 | 5601 | `	nChunk = 0;` |
|      27 | 5602 | `	pChunk = 0;` |
|      27 | 5603 | `	n = pMap->nEntry;` |
|      56 | 5604 | `	for( ;; ){` |
|     113 | 5605 | `		if( n < 1 ){` |
|       - | 5606 | `			/* When the loop terminates we may still have a current chunk` |
|       - | 5607 | `			 * that hasn't been added to the result array.  The previous` |
|       - | 5608 | `			 * implementation only pushed it if nChunk>0 which dropped the` |
|       - | 5609 | `			 * final chunk when the input size was an exact multiple of` |
|       - | 5610 | `			 * the chunk length.  Always append the pending chunk if it` |
|       - | 5611 | `			 * exists. */` |
|      27 | 5612 | `			if( pChunk ){` |
|      27 | 5613 | `				ph7_array_add_elem(pArray,0,pChunk); /* Will have its own copy */` |
|      13 | 5614 | `			}` |
|      27 | 5615 | `			break;` |
|       - | 5616 | `		}` |
|      87 | 5617 | `		if( nChunk < 1 ){` |
|      71 | 5618 | `			if( pChunk ){` |
|       - | 5619 | `				/* Put the first chunk */` |
|      45 | 5620 | `				ph7_array_add_elem(pArray,0,pChunk); /* Will have it's own copy */` |
|      22 | 5621 | `			}` |
|       - | 5622 | `			/* Create a new dimension */` |
|      71 | 5623 | `			pChunk = ph7_context_new_array(pCtx); /* Don't worry about freeing memory here,everything` |
|       - | 5624 | `												   * will be automatically released as soon we return` |
|       - | 5625 | `												   * from this function */` |
|      71 | 5626 | `			if( pChunk == 0 ){` |
|     ! 0 | 5627 | `				break;` |
|       - | 5628 | `			}` |
|      71 | 5629 | `			nChunk = nSize;` |
|      35 | 5630 | `		}` |
|       - | 5631 | `		/* Insert the entry */` |
|      87 | 5632 | `		HashmapInsertNode((ph7_hashmap *)pChunk->x.pOther,pEntry,bPreserve);` |
|       - | 5633 | `		/* Point to the next entry */` |
|      87 | 5634 | `		pEntry = pEntry->pPrev; /* Reverse link */` |
|      87 | 5635 | `		nChunk--;` |
|      87 | 5636 | `		n--;` |
|       1 | 5637 | `	}` |
|       - | 5638 | `	/* Return the multidimensional array */` |
|      27 | 5639 | `	ph7_result_value(pCtx,pArray);` |
|      27 | 5640 | `	return PH7_OK;` |
|      23 | 5641 |  |
|       - | 5642 | `/*` |
|       - | 5643 | ` * array array_pad(array $input,int $pad_size,value $pad_value)` |
|       - | 5644 | ` *  Pad array to the specified length with a value.` |
|       - | 5645 | ` * $input` |
|       - | 5646 | ` *   Initial array of values to pad.` |
|       - | 5647 | ` * $pad_size` |
|       - | 5648 | ` *   New size of the array.` |
|       - | 5649 | ` * $pad_value` |
|       - | 5650 | ` *   Value to pad if input is less than pad_size.` |
|       - | 5651 | ` */` |
|       8 | 5652 | `static int ph7_hashmap_pad(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       1 | 5653 |  |
|       - | 5654 | `	ph7_hashmap *pMap;` |
|       - | 5655 | `	ph7_value *pArray;` |
|       - | 5656 | `	int nEntry;` |
|       9 | 5657 | `	if( nArg < 3 \|\| !ph7_value_is_array(apArg[0]) ){` |
|       - | 5658 | `		/* Invalid arguments,return NULL */` |
|     ! 0 | 5659 | `		ph7_result_null(pCtx);` |
|     ! 0 | 5660 | `		return PH7_OK;` |
|       - | 5661 | `	}` |
|       - | 5662 | `	/* Create a new array */` |
|       9 | 5663 | `	pArray = ph7_context_new_array(pCtx);` |
|       9 | 5664 | `	if( pArray == 0 ){` |
|     ! 0 | 5665 | `		ph7_result_null(pCtx);` |
|     ! 0 | 5666 | `		return PH7_OK;` |
|       - | 5667 | `	}` |
|       - | 5668 | `	/* Point to the internal representation of the input hashmap */` |
|       9 | 5669 | `	pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|       - | 5670 | `	/* Extract the total number of desired entry to insert */` |
|       9 | 5671 | `	nEntry = ph7_value_to_int(apArg[1]);` |
|       9 | 5672 | `	if( nEntry < 0 ){` |
|       5 | 5673 | `		nEntry = -nEntry;` |
|       5 | 5674 | `		if( nEntry > 1048576 ){` |
|     ! 0 | 5675 | `			nEntry = 1048576; /* Limit imposed by PHP */` |
|     ! 0 | 5676 | `		}` |
|       5 | 5677 | `		if( nEntry > (int)pMap->nEntry ){` |
|       3 | 5678 | `			nEntry -= (int)pMap->nEntry;` |
|       - | 5679 | `			/* Insert given items first */` |
|       7 | 5680 | `			while( nEntry > 0 ){` |
|       5 | 5681 | `				ph7_array_add_elem(pArray,0,apArg[2]);` |
|       5 | 5682 | `				nEntry--;` |
|       1 | 5683 | `			}` |
|       - | 5684 | `			/* Merge the two arrays */` |
|       3 | 5685 | `			HashmapMerge(pMap,(ph7_hashmap *)pArray->x.pOther);` |
|       2 | 5686 | `		}else{` |
|       3 | 5687 | `			PH7_HashmapDup(pMap,(ph7_hashmap *)pArray->x.pOther);` |
|       1 | 5688 | `		}` |
|       7 | 5689 | `	}else if( nEntry > 0 ){` |
|       5 | 5690 | `		if( nEntry > 1048576 ){` |
|     ! 0 | 5691 | `			nEntry = 1048576; /* Limit imposed by PHP */` |
|     ! 0 | 5692 | `		}` |
|       5 | 5693 | `		if( nEntry > (int)pMap->nEntry ){` |
|       3 | 5694 | `			nEntry -= (int)pMap->nEntry;` |
|       - | 5695 | `			/* Merge the two arrays first */` |
|       3 | 5696 | `			HashmapMerge(pMap,(ph7_hashmap *)pArray->x.pOther);` |
|       - | 5697 | `			/* Insert given items */` |
|       7 | 5698 | `			while( nEntry > 0 ){` |
|       5 | 5699 | `				ph7_array_add_elem(pArray,0,apArg[2]);` |
|       5 | 5700 | `				nEntry--;` |
|       1 | 5701 | `			}` |
|       2 | 5702 | `		}else{` |
|       3 | 5703 | `			PH7_HashmapDup(pMap,(ph7_hashmap *)pArray->x.pOther);` |
|       - | 5704 | `		}` |
|       2 | 5705 | `	}` |
|       - | 5706 | `	/* Return the new array */` |
|       9 | 5707 | `	ph7_result_value(pCtx,pArray);` |
|       9 | 5708 | `	return PH7_OK;` |
|       5 | 5709 |  |
|       - | 5710 | `/*` |
|       - | 5711 | ` * array array_replace(array &$array,array &$array1,...)` |
|       - | 5712 | ` *  Replaces elements from passed arrays into the first array.` |
|       - | 5713 | ` * Parameters` |
|       - | 5714 | ` * $array` |
|       - | 5715 | ` *   The array in which elements are replaced.` |
|       - | 5716 | ` * $array1` |
|       - | 5717 | ` *   The array from which elements will be extracted.` |
|       - | 5718 | ` * ....` |
|       - | 5719 | ` *  More arrays from which elements will be extracted.` |
|       - | 5720 | ` *  Values from later arrays overwrite the previous values.` |
|       - | 5721 | ` * Return` |
|       - | 5722 | ` *  Returns an array, or NULL if an error occurs.` |
|       - | 5723 | ` */` |
|       2 | 5724 | `static int ph7_hashmap_replace(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       1 | 5725 |  |
|       - | 5726 | `	ph7_hashmap *pMap;` |
|       - | 5727 | `	ph7_value *pArray;` |
|       - | 5728 | `	int i;` |
|       3 | 5729 | `	if( nArg < 1 ){` |
|       - | 5730 | `		/* Invalid arguments,return NULL */` |
|     ! 0 | 5731 | `		ph7_result_null(pCtx);` |
|     ! 0 | 5732 | `		return PH7_OK;` |
|       - | 5733 | `	}` |
|       - | 5734 | `	/* Create a new array */` |
|       3 | 5735 | `	pArray = ph7_context_new_array(pCtx);` |
|       3 | 5736 | `	if( pArray == 0 ){` |
|     ! 0 | 5737 | `		ph7_result_null(pCtx);` |
|     ! 0 | 5738 | `		return PH7_OK;` |
|       - | 5739 | `	}` |
|       - | 5740 | `	/* Perform the requested operation */` |
|       7 | 5741 | `	for( i = 0 ; i < nArg ; i++ ){` |
|       5 | 5742 | `		if( !ph7_value_is_array(apArg[i]) ){` |
|     ! 0 | 5743 | `			continue;` |
|       - | 5744 | `		}` |
|       - | 5745 | `		/* Point to the internal representation of the input hashmap */` |
|       5 | 5746 | `		pMap = (ph7_hashmap *)apArg[i]->x.pOther;` |
|       5 | 5747 | `		HashmapOverwrite(pMap,(ph7_hashmap *)pArray->x.pOther);` |
|       3 | 5748 | `	}` |
|       - | 5749 | `	/* Return the new array */` |
|       3 | 5750 | `	ph7_result_value(pCtx,pArray);` |
|       3 | 5751 | `	return PH7_OK;` |
|       2 | 5752 |  |
|       - | 5753 | `/*` |
|       - | 5754 | ` * array array_filter(array $input [,callback $callback ])` |
|       - | 5755 | ` *  Filters elements of an array using a callback function.` |
|       - | 5756 | ` * Parameters` |
|       - | 5757 | ` *  $input` |
|       - | 5758 | ` *    The array to iterate over` |
|       - | 5759 | ` * $callback` |
|       - | 5760 | ` *    The callback function to use` |
|       - | 5761 | ` *    If no callback is supplied, all entries of input equal to FALSE (see converting to boolean)` |
|       - | 5762 | ` *    will be removed.` |
|       - | 5763 | ` * Return` |
|       - | 5764 | ` *  The filtered array.` |
|       - | 5765 | ` */` |
|      18 | 5766 | `static int ph7_hashmap_filter(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       2 | 5767 |  |
|       - | 5768 | `	ph7_hashmap_node *pEntry;` |
|       - | 5769 | `	ph7_hashmap *pMap;` |
|       - | 5770 | `	ph7_value *pArray;` |
|       - | 5771 | `	ph7_value sResult;   /* Callback result */` |
|       - | 5772 | `	ph7_value *pValue;` |
|       - | 5773 | `	sxi32 rc;` |
|       - | 5774 | `	int keep;` |
|       - | 5775 | `	sxu32 n;` |
|      20 | 5776 | `	if( nArg < 1 \|\| !ph7_value_is_array(apArg[0]) ){` |
|       - | 5777 | `		/* Invalid arguments,return NULL */` |
|       5 | 5778 | `		ph7_result_null(pCtx);` |
|       5 | 5779 | `		return PH7_OK;` |
|       - | 5780 | `	}` |
|       - | 5781 | `	/* Create a new array */` |
|      16 | 5782 | `	pArray = ph7_context_new_array(pCtx);` |
|      16 | 5783 | `	if( pArray == 0 ){` |
|     ! 0 | 5784 | `		ph7_result_null(pCtx);` |
|     ! 0 | 5785 | `		return PH7_OK;` |
|       - | 5786 | `	}` |
|       - | 5787 | `	/* Point to the internal representation of the input hashmap */` |
|      16 | 5788 | `	pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|      16 | 5789 | `	pEntry = pMap->pFirst;` |
|      16 | 5790 | `	PH7_MemObjInit(pMap->pVm,&sResult);` |
|      16 | 5791 | `	sResult.nIdx = SXU32_HIGH; /* Mark as constant */` |
|       - | 5792 | `	/* Perform the requested operation */` |
|      66 | 5793 | `	for( n = 0 ; n < pMap->nEntry ; n++ ){` |
|       - | 5794 | `		/* Extract node value (may be NULL if allocation failed) */` |
|      54 | 5795 | `		pValue = HashmapExtractNodeValue(pEntry);` |
|      54 | 5796 | `		if( pValue == 0 ){` |
|       - | 5797 | `			/* Can happen if SySetAt() failed earlier; drop the entry. */` |
|     ! 0 | 5798 | `			keep = FALSE;` |
|      54 | 5799 | `		}else if( nArg > 1 && !ph7_value_is_null(apArg[1]) ){` |
|       - | 5800 | `			/* Callback was supplied (not NULL).  PHP 8 throws a` |
|       - | 5801 | `				* TypeError when the value is not callable or null; prior PH7` |
|       - | 5802 | `				* silently dropped the element.  Emit similar message. */` |
|      26 | 5803 | `			if( !ph7_value_is_callable(apArg[1]) ){` |
|       3 | 5804 | `				if( ph7_value_is_string(apArg[1]) ){` |
|       - | 5805 | `					int len;` |
|       3 | 5806 | `					const char *zName = ph7_value_to_string(apArg[1], &len);` |
|       4 | 5807 | `					return PH7_VmThrowException(pCtx,` |
|       - | 5808 | `						"TypeError",` |
|       - | 5809 | `						"array_filter(): Argument #2 ($callback) must be a valid callback or null, function \"%s\" not found or invalid function name",` |
|       1 | 5810 | `						zName` |
|       - | 5811 | `						);` |
|     ! 0 | 5812 | `				}else{` |
|     ! 0 | 5813 | `					return PH7_VmThrowException(pCtx,` |
|       - | 5814 | `						"TypeError",` |
|       - | 5815 | `						"array_filter(): Argument #2 ($callback) must be a valid callback or null, %s given",` |
|     ! 0 | 5816 | `						ph7_type_name(apArg[1])` |
|       - | 5817 | `						);` |
|       - | 5818 | `				}` |
|       - | 5819 | `			}` |
|      23 | 5820 | `			keep = FALSE;` |
|      23 | 5821 | `			rc = PH7_VmCallUserFunction(pMap->pVm,apArg[1],1,&pValue,&sResult);` |
|      23 | 5822 | `			if( rc == SXRET_OK ){` |
|       - | 5823 | `				/* Perform a boolean cast */` |
|      23 | 5824 | `				keep = ph7_value_to_bool(&sResult);` |
|      11 | 5825 | `			}` |
|      23 | 5826 | `			PH7_MemObjRelease(&sResult);` |
|      12 | 5827 | `		}else{` |
|       - | 5828 | `			/* No callback provided or callback explicitly NULL: use default` |
|       - | 5829 | `			 * behaviour where "empty" values are removed. This also covers` |
|       - | 5830 | `			 * the case where the callback argument is missing entirely.` |
|       - | 5831 | `			 */` |
|      29 | 5832 | `			keep = !PH7_MemObjIsEmpty(pValue);` |
|       - | 5833 | `		}` |
|      51 | 5834 | `		if( keep ){` |
|       - | 5835 | `			/* Perform the insertion,now the callback returned true */` |
|      17 | 5836 | `			HashmapInsertNode((ph7_hashmap *)pArray->x.pOther,pEntry,TRUE);` |
|       8 | 5837 | `		}` |
|       - | 5838 | `		/* Point to the next entry */` |
|      51 | 5839 | `		pEntry = pEntry->pPrev; /* Reverse link */` |
|      26 | 5840 | `	}` |
|      13 | 5841 | `	ph7_result_value(pCtx,pArray);` |
|      13 | 5842 | `	return PH7_OK;` |
|      11 | 5843 |  |
|       - | 5844 | `/*` |
|       - | 5845 | ` * array array_map(callback $callback,array $arr1)` |
|       - | 5846 | ` *  Applies the callback to the elements of the given arrays.` |
|       - | 5847 | ` * Parameters` |
|       - | 5848 | ` *  $callback` |
|       - | 5849 | ` *   Callback function to run for each element in each array.` |
|       - | 5850 | ` * $arr1` |
|       - | 5851 | ` *   An array to run through the callback function.` |
|       - | 5852 | ` * Return` |
|       - | 5853 | ` *  Returns an array containing all the elements of arr1 after applying` |
|       - | 5854 | ` *  the callback function to each one.` |
|       - | 5855 | ` * NOTE:` |
|       - | 5856 | ` *  array_map() passes only a single value to the callback.` |
|       - | 5857 | ` */` |
|      10 | 5858 | `static int ph7_hashmap_map(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       1 | 5859 |  |
|       - | 5860 | `	ph7_value *pArray,*pValue,sKey,sResult;` |
|       - | 5861 | `	ph7_hashmap_node *pEntry;` |
|       - | 5862 | `	ph7_hashmap *pMap;` |
|       - | 5863 | `	sxu32 n;` |
|      11 | 5864 | `	if( nArg < 2 \|\| !ph7_value_is_array(apArg[1]) ){` |
|       - | 5865 | `		/* Invalid arguments,return NULL */` |
|       5 | 5866 | `		ph7_result_null(pCtx);` |
|       5 | 5867 | `		return PH7_OK;` |
|       - | 5868 | `	}` |
|       - | 5869 | `	/* Create a new array */` |
|       7 | 5870 | `	pArray = ph7_context_new_array(pCtx);` |
|       7 | 5871 | `	if( pArray == 0 ){` |
|     ! 0 | 5872 | `		ph7_result_null(pCtx);` |
|     ! 0 | 5873 | `		return PH7_OK;` |
|       - | 5874 | `	}` |
|       - | 5875 | `	/* Point to the internal representation of the input hashmap */` |
|       7 | 5876 | `	pMap = (ph7_hashmap *)apArg[1]->x.pOther;` |
|       7 | 5877 | `	PH7_MemObjInit(pMap->pVm,&sResult);` |
|       7 | 5878 | `	PH7_MemObjInit(pMap->pVm,&sKey);` |
|       7 | 5879 | `	sResult.nIdx = SXU32_HIGH; /* Mark as constant */` |
|       7 | 5880 | `	sKey.nIdx    = SXU32_HIGH; /* Mark as constant */` |
|       - | 5881 | `	/* Perform the requested operation */` |
|       7 | 5882 | `	pEntry = pMap->pFirst;` |
|      21 | 5883 | `	for( n = 0 ; n < pMap->nEntry ; n++ ){` |
|       - | 5884 | `		/* Extrcat the node value */` |
|      15 | 5885 | `		pValue = HashmapExtractNodeValue(pEntry);` |
|      15 | 5886 | `		if( pValue ){` |
|       - | 5887 | `			sxi32 rc;` |
|       - | 5888 | `			/* Invoke the supplied callback */` |
|      15 | 5889 | `			rc = PH7_VmCallUserFunction(pMap->pVm,apArg[0],1,&pValue,&sResult);` |
|       - | 5890 | `			/* Extract the node key */` |
|      15 | 5891 | `			PH7_HashmapExtractNodeKey(pEntry,&sKey);` |
|      15 | 5892 | `			if( rc != SXRET_OK ){` |
|       - | 5893 | `				/* An error occured while invoking the supplied callback [i.e: not defined] */` |
|     ! 0 | 5894 | `				ph7_array_add_elem(pArray,&sKey,pValue); /* Keep the same value */` |
|     ! 0 | 5895 | `			}else{` |
|       - | 5896 | `				/* Insert the callback return value */` |
|      15 | 5897 | `				ph7_array_add_elem(pArray,&sKey,&sResult);` |
|       - | 5898 | `			}` |
|      15 | 5899 | `			PH7_MemObjRelease(&sKey);` |
|      15 | 5900 | `			PH7_MemObjRelease(&sResult);` |
|       7 | 5901 | `		}` |
|       - | 5902 | `		/* Point to the next entry */` |
|      15 | 5903 | `		pEntry = pEntry->pPrev; /* Reverse link */` |
|       8 | 5904 | `	}` |
|       7 | 5905 | `	ph7_result_value(pCtx,pArray);` |
|       7 | 5906 | `	return PH7_OK;` |
|       6 | 5907 |  |
|       - | 5908 | `/*` |
|       - | 5909 | ` * value array_reduce(array $input,callback $function[, value $initial = NULL ])` |
|       - | 5910 | ` *  Iteratively reduce the array to a single value using a callback function.` |
|       - | 5911 | ` * Parameters` |
|       - | 5912 | ` *  $input` |
|       - | 5913 | ` *   The input array.` |
|       - | 5914 | ` *  $function` |
|       - | 5915 | ` *  The callback function.` |
|       - | 5916 | ` * $initial` |
|       - | 5917 | ` *  If the optional initial is available, it will be used at the beginning` |
|       - | 5918 | ` *  of the process, or as a final result in case the array is empty.` |
|       - | 5919 | ` * Return` |
|       - | 5920 | ` *  Returns the resulting value.` |
|       - | 5921 | ` *  If the array is empty and initial is not passed, array_reduce() returns NULL.` |
|       - | 5922 | ` */` |
|       4 | 5923 | `static int ph7_hashmap_reduce(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       1 | 5924 |  |
|       - | 5925 | `	ph7_hashmap_node *pEntry;` |
|       - | 5926 | `	ph7_hashmap *pMap;` |
|       - | 5927 | `	ph7_value *pValue;` |
|       - | 5928 | `	ph7_value sResult;` |
|       - | 5929 | `	sxu32 n;` |
|       5 | 5930 | `	if( nArg < 2 \|\| !ph7_value_is_array(apArg[0]) ){` |
|       - | 5931 | `		/* Invalid/Missing arguments,return NULL */` |
|     ! 0 | 5932 | `		ph7_result_null(pCtx);` |
|     ! 0 | 5933 | `		return PH7_OK;` |
|       - | 5934 | `	}` |
|       - | 5935 | `	/* Point to the internal representation of the input hashmap */` |
|       5 | 5936 | `	pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|       - | 5937 | `	/* Assume a NULL initial value */` |
|       5 | 5938 | `	PH7_MemObjInit(pMap->pVm,&sResult);` |
|       5 | 5939 | `	sResult.nIdx = SXU32_HIGH; /* Mark as constant */` |
|       5 | 5940 | `	if( nArg > 2 ){` |
|       - | 5941 | `		/* Set the initial value */` |
|       5 | 5942 | `		PH7_MemObjLoad(apArg[2],&sResult);` |
|       2 | 5943 | `	}` |
|       - | 5944 | `	/* Perform the requested operation */` |
|       5 | 5945 | `	pEntry = pMap->pFirst;` |
|      19 | 5946 | `	for( n = 0 ; n < pMap->nEntry ; n++ ){` |
|       - | 5947 | `		/* Extract the node value */` |
|      15 | 5948 | `		pValue = HashmapExtractNodeValue(pEntry);` |
|       - | 5949 | `		/* Invoke the supplied callback */` |
|      15 | 5950 | `		PH7_VmCallUserFunctionAp(pMap->pVm,apArg[1],&sResult,&sResult,pValue,0);` |
|       - | 5951 | `		/* Point to the next entry */` |
|      15 | 5952 | `		pEntry = pEntry->pPrev; /* Reverse link */` |
|       8 | 5953 | `	}` |
|       5 | 5954 | `	ph7_result_value(pCtx,&sResult); /* Will make it's own copy */` |
|       5 | 5955 | `	PH7_MemObjRelease(&sResult);` |
|       5 | 5956 | `	return PH7_OK;` |
|       3 | 5957 |  |
|       - | 5958 | `/*` |
|       - | 5959 | ` * bool array_walk(array &$array,callback $funcname [, value $userdata ] )` |
|       - | 5960 | ` *  Apply a user function to every member of an array.` |
|       - | 5961 | ` * Parameters` |
|       - | 5962 | ` *  $array` |
|       - | 5963 | ` *   The input array.` |
|       - | 5964 | ` * $funcname` |
|       - | 5965 | ` *  Typically, funcname takes on two parameters.The array parameter's value being` |
|       - | 5966 | ` *  the first, and the key/index second.` |
|       - | 5967 | ` * Note:` |
|       - | 5968 | ` *  If funcname needs to be working with the actual values of the array,specify the first` |
|       - | 5969 | ` *  parameter of funcname as a reference. Then, any changes made to those elements will` |
|       - | 5970 | ` *  be made in the original array itself.` |
|       - | 5971 | ` * $userdata` |
|       - | 5972 | ` *  If the optional userdata parameter is supplied, it will be passed as the third parameter` |
|       - | 5973 | ` *  to the callback funcname.` |
|       - | 5974 | ` * Return` |
|       - | 5975 | ` *  Returns TRUE on success or FALSE on failure.` |
|       - | 5976 | ` */` |
|      12 | 5977 | `static int ph7_hashmap_walk(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       1 | 5978 |  |
|       - | 5979 | `	ph7_value *pValue,*pUserData,sKey;` |
|       - | 5980 | `	ph7_hashmap_node *pEntry;` |
|       - | 5981 | `	ph7_hashmap *pMap;` |
|       - | 5982 | `	sxi32 rc;` |
|       - | 5983 | `	sxu32 n;` |
|      13 | 5984 | `	if( nArg < 2 \|\| !ph7_value_is_array(apArg[0]) ){` |
|       - | 5985 | `		/* Invalid/Missing arguments,return FALSE */` |
|     ! 0 | 5986 | `		ph7_result_bool(pCtx,0);` |
|     ! 0 | 5987 | `		return PH7_OK;` |
|       - | 5988 | `	}` |
|      13 | 5989 | `	pUserData = nArg > 2 ? apArg[2] : 0;` |
|       - | 5990 | `	/* Point to the internal representation of the input hashmap */` |
|      13 | 5991 | `	pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|      13 | 5992 | `	PH7_MemObjInit(pMap->pVm,&sKey);` |
|      13 | 5993 | `	sKey.nIdx = SXU32_HIGH; /* Mark as constant */` |
|       - | 5994 | `	/* Perform the desired operation */` |
|      13 | 5995 | `	pEntry = pMap->pFirst;` |
|      41 | 5996 | `	for( n = 0 ; n < pMap->nEntry ; n++ ){` |
|       - | 5997 | `		/* Extract the node value */` |
|      29 | 5998 | `		pValue = HashmapExtractNodeValue(pEntry);` |
|      29 | 5999 | `		if( pValue ){` |
|       - | 6000 | `			/* Extract the entry key */` |
|      29 | 6001 | `			PH7_HashmapExtractNodeKey(pEntry,&sKey);` |
|       - | 6002 | `			/* Invoke the supplied callback */` |
|      29 | 6003 | `			rc = PH7_VmCallUserFunctionAp(pMap->pVm,apArg[1],0,pValue,&sKey,pUserData,0);` |
|      29 | 6004 | `			PH7_MemObjRelease(&sKey);` |
|      29 | 6005 | `			if( rc != SXRET_OK ){` |
|       - | 6006 | `				/* An error occured while invoking the supplied callback [i.e: not defined] */` |
|     ! 0 | 6007 | `				ph7_result_bool(pCtx,0); /* return FALSE */` |
|     ! 0 | 6008 | `				return PH7_OK;` |
|       - | 6009 | `			}` |
|      14 | 6010 | `		}` |
|       - | 6011 | `		/* Point to the next entry */` |
|      29 | 6012 | `		pEntry = pEntry->pPrev; /* Reverse link */` |
|      15 | 6013 | `	}` |
|       - | 6014 | `	/* All done,return TRUE */` |
|      13 | 6015 | `	ph7_result_bool(pCtx,1);` |
|      13 | 6016 | `	return PH7_OK;` |
|       7 | 6017 |  |
|       - | 6018 | `/*` |
|       - | 6019 | ` * Apply a user function to every member of an array.(Recurse on array's).` |
|       - | 6020 | ` * Refer to the [array_walk_recursive()] implementation for more information.` |
|       - | 6021 | ` */` |
|       6 | 6022 | `static int HashmapWalkRecursive(` |
|       - | 6023 | `	ph7_hashmap *pMap,    /* Target hashmap */` |
|       - | 6024 | `	ph7_value *pCallback, /* User callback */` |
|       - | 6025 | `	ph7_value *pUserData, /* Callback private data */` |
|       - | 6026 | `	int iNest             /* Nesting level */` |
|       - | 6027 | `	)` |
|       1 | 6028 |  |
|       - | 6029 | `	ph7_hashmap_node *pEntry;` |
|       - | 6030 | `	ph7_value *pValue,sKey;` |
|       - | 6031 | `	sxi32 rc;` |
|       - | 6032 | `	sxu32 n;` |
|       - | 6033 | `	/* Iterate throw hashmap entries */` |
|       7 | 6034 | `	PH7_MemObjInit(pMap->pVm,&sKey);` |
|       7 | 6035 | `	sKey.nIdx = SXU32_HIGH; /* Mark as constant */` |
|       7 | 6036 | `	pEntry = pMap->pFirst;` |
|      17 | 6037 | `	for( n = 0 ; n < pMap->nEntry ; n++ ){` |
|       - | 6038 | `		/* Extract the node value */` |
|      11 | 6039 | `		pValue = HashmapExtractNodeValue(pEntry);` |
|      11 | 6040 | `		if( pValue ){` |
|      11 | 6041 | `			if( pValue->iFlags & MEMOBJ_HASHMAP ){` |
|       5 | 6042 | `				if( iNest < 32 ){` |
|       - | 6043 | `					/* Recurse */` |
|       5 | 6044 | `					iNest++;` |
|       5 | 6045 | `					HashmapWalkRecursive((ph7_hashmap *)pValue->x.pOther,pCallback,pUserData,iNest);` |
|       5 | 6046 | `					iNest--;` |
|       2 | 6047 | `				}` |
|       3 | 6048 | `			}else{` |
|       - | 6049 | `				/* Extract the node key */` |
|       7 | 6050 | `				PH7_HashmapExtractNodeKey(pEntry,&sKey);` |
|       - | 6051 | `				/* Invoke the supplied callback */` |
|       7 | 6052 | `				rc = PH7_VmCallUserFunctionAp(pMap->pVm,pCallback,0,pValue,&sKey,pUserData,0);` |
|       7 | 6053 | `				PH7_MemObjRelease(&sKey);` |
|       7 | 6054 | `				if( rc != SXRET_OK ){` |
|     ! 0 | 6055 | `					return rc;` |
|       - | 6056 | `				}` |
|       - | 6057 | `			}` |
|       5 | 6058 | `		}` |
|       - | 6059 | `		/* Point to the next entry */` |
|      11 | 6060 | `		pEntry = pEntry->pPrev; /* Reverse link */` |
|       6 | 6061 | `	}` |
|       7 | 6062 | `	return SXRET_OK;` |
|       4 | 6063 |  |
|       - | 6064 | `/*` |
|       - | 6065 | ` * bool array_walk_recursive(array &$array,callback $funcname [, value $userdata ] )` |
|       - | 6066 | ` *  Apply a user function recursively to every member of an array.` |
|       - | 6067 | ` * Parameters` |
|       - | 6068 | ` *  $array` |
|       - | 6069 | ` *   The input array.` |
|       - | 6070 | ` * $funcname` |
|       - | 6071 | ` *  Typically, funcname takes on two parameters.The array parameter's value being` |
|       - | 6072 | ` *  the first, and the key/index second.` |
|       - | 6073 | ` * Note:` |
|       - | 6074 | ` *  If funcname needs to be working with the actual values of the array,specify the first` |
|       - | 6075 | ` *  parameter of funcname as a reference. Then, any changes made to those elements will` |
|       - | 6076 | ` *  be made in the original array itself.` |
|       - | 6077 | ` * $userdata` |
|       - | 6078 | ` *  If the optional userdata parameter is supplied, it will be passed as the third parameter` |
|       - | 6079 | ` *  to the callback funcname.` |
|       - | 6080 | ` * Return` |
|       - | 6081 | ` *  Returns TRUE on success or FALSE on failure.` |
|       - | 6082 | ` */` |
|       2 | 6083 | `static int ph7_hashmap_walk_recursive(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       1 | 6084 |  |
|       - | 6085 | `	ph7_hashmap *pMap;` |
|       - | 6086 | `	sxi32 rc;` |
|       3 | 6087 | `	if( nArg < 2 \|\| !ph7_value_is_array(apArg[0]) ){` |
|       - | 6088 | `		/* Invalid/Missing arguments,return FALSE */` |
|     ! 0 | 6089 | `		ph7_result_bool(pCtx,0);` |
|     ! 0 | 6090 | `		return PH7_OK;` |
|       - | 6091 | `	}` |
|       - | 6092 | `	/* Point to the internal representation of the input hashmap */` |
|       3 | 6093 | `	pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|       - | 6094 | `	/* Perform the desired operation */` |
|       3 | 6095 | `	rc = HashmapWalkRecursive(pMap,apArg[1],nArg > 2 ? apArg[2] : 0,0);` |
|       - | 6096 | `	/* All done */` |
|       3 | 6097 | `	ph7_result_bool(pCtx,rc == SXRET_OK);` |
|       3 | 6098 | `	return PH7_OK;` |
|       2 | 6099 |  |
|       - | 6100 | `/*` |
|       - | 6101 | ` * Table of hashmap functions.` |
|       - | 6102 | ` */` |
|       - | 6103 | `static const ph7_builtin_func aHashmapFunc[] = {` |
|       - | 6104 | `	{"count",             ph7_hashmap_count },` |
|       - | 6105 | `	{"sizeof",            ph7_hashmap_count },` |
|       - | 6106 | `	{"array_key_exists",  ph7_hashmap_key_exists },` |
|       - | 6107 | `	{"array_pop",         ph7_hashmap_pop     },` |
|       - | 6108 | `	{"array_push",        ph7_hashmap_push    },` |
|       - | 6109 | `	{"array_shift",       ph7_hashmap_shift   },` |
|       - | 6110 | `	{"array_product",     ph7_hashmap_product },` |
|       - | 6111 | `	{"array_sum",         ph7_hashmap_sum     },` |
|       - | 6112 | `	{"array_keys",        ph7_hashmap_keys    },` |
|       - | 6113 | `	{"array_values",      ph7_hashmap_values  },` |
|       - | 6114 | `	{"array_same",        ph7_hashmap_same    },  /* Symisc eXtension */` |
|       - | 6115 | `	{"array_merge",       ph7_hashmap_merge   },` |
|       - | 6116 | `	{"array_slice",       ph7_hashmap_slice   },` |
|       - | 6117 | `	{"array_splice",      ph7_hashmap_splice  },` |
|       - | 6118 | `	{"array_search",      ph7_hashmap_search  },` |
|       - | 6119 | `	{"array_diff",        ph7_hashmap_diff    },` |
|       - | 6120 | `	{"array_udiff",       ph7_hashmap_udiff   },` |
|       - | 6121 | `	{"array_diff_assoc",  ph7_hashmap_diff_assoc },` |
|       - | 6122 | `	{"array_diff_uassoc", ph7_hashmap_diff_uassoc },` |
|       - | 6123 | `	{"array_diff_key",    ph7_hashmap_diff_key },` |
|       - | 6124 | `	{"array_intersect",   ph7_hashmap_intersect},` |
|       - | 6125 | `	{"array_intersect_assoc", ph7_hashmap_intersect_assoc},` |
|       - | 6126 | `	{"array_uintersect",  ph7_hashmap_uintersect},` |
|       - | 6127 | `	{"array_intersect_key",   ph7_hashmap_intersect_key},` |
|       - | 6128 | `	{"array_copy",        ph7_hashmap_copy    },` |
|       - | 6129 | `	{"array_erase",       ph7_hashmap_erase   },` |
|       - | 6130 | `	{"array_fill",        ph7_hashmap_fill    },` |
|       - | 6131 | `	{"array_fill_keys",   ph7_hashmap_fill_keys},` |
|       - | 6132 | `	{"array_combine",     ph7_hashmap_combine },` |
|       - | 6133 | `	{"array_reverse",     ph7_hashmap_reverse },` |
|       - | 6134 | `	{"array_unique",      ph7_hashmap_unique  },` |
|       - | 6135 | `	{"array_flip",        ph7_hashmap_flip    },` |
|       - | 6136 | `	{"array_rand",        ph7_hashmap_rand    },` |
|       - | 6137 | `	{"array_chunk",       ph7_hashmap_chunk   },` |
|       - | 6138 | `	{"array_pad",         ph7_hashmap_pad     },` |
|       - | 6139 | `	{"array_replace",     ph7_hashmap_replace },` |
|       - | 6140 | `	{"array_filter",      ph7_hashmap_filter  },` |
|       - | 6141 | `	{"array_map",         ph7_hashmap_map     },` |
|       - | 6142 | `	{"array_reduce",      ph7_hashmap_reduce  },` |
|       - | 6143 | `	{"array_walk",        ph7_hashmap_walk    },` |
|       - | 6144 | `	{"array_walk_recursive", ph7_hashmap_walk_recursive },` |
|       - | 6145 | `	{"in_array",          ph7_hashmap_in_array},` |
|       - | 6146 | `	{"sort",              ph7_hashmap_sort    },` |
|       - | 6147 | `	{"asort",             ph7_hashmap_asort   },` |
|       - | 6148 | `	{"arsort",            ph7_hashmap_arsort  },` |
|       - | 6149 | `	{"ksort",             ph7_hashmap_ksort   },` |
|       - | 6150 | `	{"krsort",            ph7_hashmap_krsort  },` |
|       - | 6151 | `	{"rsort",             ph7_hashmap_rsort   },` |
|       - | 6152 | `	{"usort",             ph7_hashmap_usort   },` |
|       - | 6153 | `	{"uasort",            ph7_hashmap_uasort  },` |
|       - | 6154 | `	{"uksort",            ph7_hashmap_uksort  },` |
|       - | 6155 | `	{"shuffle",           ph7_hashmap_shuffle },` |
|       - | 6156 | `	{"range",             ph7_hashmap_range   },` |
|       - | 6157 | `	{"current",           ph7_hashmap_current },` |
|       - | 6158 | `	{"each",              ph7_hashmap_each    },` |
|       - | 6159 | `	{"pos",               ph7_hashmap_current },` |
|       - | 6160 | `	{"next",              ph7_hashmap_next    },` |
|       - | 6161 | `	{"prev",              ph7_hashmap_prev    },` |
|       - | 6162 | `	{"end",               ph7_hashmap_end     },` |
|       - | 6163 | `	{"reset",             ph7_hashmap_reset   },` |
|       - | 6164 | `	{"key",               ph7_hashmap_simple_key }` |
|       - | 6165 | `};` |
|       - | 6166 | `/*` |
|       - | 6167 | ` * Register the built-in hashmap functions defined above.` |
|       - | 6168 | ` */` |
|    1260 | 6169 | `PH7_PRIVATE void PH7_RegisterHashmapFunctions(ph7_vm *pVm)` |
|       2 | 6170 |  |
|       - | 6171 | `	sxu32 n;` |
|   78122 | 6172 | `	for( n = 0 ; n < SX_ARRAYSIZE(aHashmapFunc) ; n++ ){` |
|   76862 | 6173 | `		ph7_create_function(&(*pVm),aHashmapFunc[n].zName,aHashmapFunc[n].xFunc,0);` |
|   38432 | 6174 | `	}` |
|    1262 | 6175 |  |
|       - | 6176 | `/*` |
|       - | 6177 | ` * Dump a hashmap instance and it's entries and the store the dump in` |
|       - | 6178 | ` * the BLOB given as the first argument.` |
|       - | 6179 | ` * This function is typically invoked when the user issue a call to` |
|       - | 6180 | ` * [var_dump(),var_export(),print_r(),...]` |
|       - | 6181 | ` * This function SXRET_OK on success. Any other return value including` |
|       - | 6182 | ` * SXERR_LIMIT(infinite recursion) indicates failure.` |
|       - | 6183 | ` */` |
|      28 | 6184 | `PH7_PRIVATE sxi32 PH7_HashmapDump(SyBlob *pOut,ph7_hashmap *pMap,int ShowType,int nTab,int nDepth)` |
|       2 | 6185 |  |
|       - | 6186 | `	ph7_hashmap_node *pEntry;` |
|       - | 6187 | `	ph7_value *pObj;` |
|      30 | 6188 | `	sxu32 n = 0;` |
|       - | 6189 | `	int isRef;` |
|       - | 6190 | `	sxi32 rc;` |
|       - | 6191 | `	int i;` |
|      30 | 6192 | `	if( nDepth > 31 ){` |
|       - | 6193 | `		static const char zInfinite[] = "Nesting limit reached: Infinite recursion?";` |
|       - | 6194 | `		/* Nesting limit reached */` |
|     ! 0 | 6195 | `		SyBlobAppend(&(*pOut),zInfinite,sizeof(zInfinite)-1);` |
|     ! 0 | 6196 | `		if( ShowType ){` |
|     ! 0 | 6197 | `			SyBlobAppend(&(*pOut),")",sizeof(char));` |
|     ! 0 | 6198 | `		}` |
|     ! 0 | 6199 | `		return SXERR_LIMIT;` |
|       - | 6200 | `	}` |
|       - | 6201 | `	/* Point to the first inserted entry */` |
|      30 | 6202 | `	pEntry = pMap->pFirst;` |
|      30 | 6203 | `	rc = SXRET_OK;` |
|      30 | 6204 | `	if( !ShowType ){` |
|      15 | 6205 | `		SyBlobAppend(&(*pOut),"Array(",sizeof("Array(")-1);` |
|       7 | 6206 | `	}` |
|       - | 6207 | `	/* Total entries */` |
|      30 | 6208 | `	SyBlobFormat(&(*pOut),"%u) {",pMap->nEntry);` |
|       - | 6209 | `#ifdef __WINNT__` |
|       2 | 6210 | `	SyBlobAppend(&(*pOut),"\r\n",sizeof("\r\n")-1);` |
|       - | 6211 | `#else` |
|      28 | 6212 | `	SyBlobAppend(&(*pOut),"\n",sizeof(char));` |
|       - | 6213 | `#endif` |
|      65 | 6214 | `	for(;;){` |
|     132 | 6215 | `		if( n >= pMap->nEntry ){` |
|      30 | 6216 | `			break;` |
|       - | 6217 | `		}` |
|     206 | 6218 | `		for( i = 0 ; i < nTab ; i++ ){` |
|     104 | 6219 | `			SyBlobAppend(&(*pOut)," ",sizeof(char));` |
|      53 | 6220 | `		}` |
|       - | 6221 | `		/* Dump key */` |
|     104 | 6222 | `		if( pEntry->iType == HASHMAP_INT_NODE){` |
|      37 | 6223 | `			SyBlobFormat(&(*pOut),"[%qd] =>",pEntry->xKey.iKey);` |
|      19 | 6224 | `		}else{` |
|     101 | 6225 | `			SyBlobFormat(&(*pOut),"[%.*s] =>",` |
|      33 | 6226 | `				SyBlobLength(&pEntry->xKey.sKey),SyBlobData(&pEntry->xKey.sKey));` |
|       - | 6227 | `		}` |
|       - | 6228 | `#ifdef __WINNT__` |
|       2 | 6229 | `		SyBlobAppend(&(*pOut),"\r\n",sizeof("\r\n")-1);` |
|       - | 6230 | `#else` |
|     102 | 6231 | `		SyBlobAppend(&(*pOut),"\n",sizeof(char));` |
|       - | 6232 | `#endif` |
|       - | 6233 | `		/* Dump node value */` |
|     104 | 6234 | `		pObj = HashmapExtractNodeValue(pEntry);` |
|     104 | 6235 | `		isRef = 0;` |
|     104 | 6236 | `		if( pObj ){` |
|     104 | 6237 | `			if( pEntry->iFlags & HASHMAP_NODE_FOREIGN_OBJ ){` |
|       - | 6238 | `				/* Referenced object */` |
|     ! 0 | 6239 | `				isRef = 1;` |
|     ! 0 | 6240 | `			}` |
|     104 | 6241 | `			rc = PH7_MemObjDump(&(*pOut),pObj,ShowType,nTab+1,nDepth,isRef);` |
|     104 | 6242 | `			if( rc == SXERR_LIMIT ){` |
|     ! 0 | 6243 | `				break;` |
|       - | 6244 | `			}` |
|      51 | 6245 | `		}` |
|       - | 6246 | `		/* Point to the next entry */` |
|     104 | 6247 | `		n++;` |
|     104 | 6248 | `		pEntry = pEntry->pPrev; /* Reverse link */` |
|       2 | 6249 | `	}` |
|      58 | 6250 | `	for( i = 0 ; i < nTab ; i++ ){` |
|      30 | 6251 | `		SyBlobAppend(&(*pOut)," ",sizeof(char));` |
|      16 | 6252 | `	}` |
|      30 | 6253 | `	SyBlobAppend(&(*pOut),"}",sizeof(char));` |
|      30 | 6254 | `	return rc;` |
|      16 | 6255 |  |
|       - | 6256 | `/*` |
|       - | 6257 | ` * Iterate throw hashmap entries and invoke the given callback [i.e: xWalk()] for each` |
|       - | 6258 | ` * retrieved entry.` |
|       - | 6259 | ` * Note that argument are passed to the callback by copy. That is,any modification to` |
|       - | 6260 | ` * the entry value in the callback body will not alter the real value.` |
|       - | 6261 | ` * If the callback wishes to abort processing [i.e: it's invocation] it must return` |
|       - | 6262 | ` * a value different from PH7_OK.` |
|       - | 6263 | ` * Refer to [ph7_array_walk()] for more information.` |
|       - | 6264 | ` */` |
|   18706 | 6265 | `PH7_PRIVATE sxi32 PH7_HashmapWalk(` |
|       - | 6266 | `	ph7_hashmap *pMap, /* Target hashmap */` |
|       - | 6267 | `	int (*xWalk)(ph7_value *,ph7_value *,void *), /* Walker callback */` |
|       - | 6268 | `	void *pUserData /* Last argument to xWalk() */` |
|       - | 6269 | `	)` |
|       2 | 6270 |  |
|       - | 6271 | `	ph7_hashmap_node *pEntry;` |
|       - | 6272 | `	ph7_value sKey,sValue;` |
|       - | 6273 | `	sxi32 rc;` |
|       - | 6274 | `	sxu32 n;` |
|       - | 6275 | `	/* Initialize walker parameter */` |
|   18708 | 6276 | `	rc = SXRET_OK;` |
|   18708 | 6277 | `	PH7_MemObjInit(pMap->pVm,&sKey);` |
|   18708 | 6278 | `	PH7_MemObjInit(pMap->pVm,&sValue);` |
|   18708 | 6279 | `	n = pMap->nEntry;` |
|   18708 | 6280 | `	pEntry = pMap->pFirst;` |
|       - | 6281 | `	/* Start the iteration process */` |
|   50560 | 6282 | `	for(;;){` |
|  101122 | 6283 | `		if( n < 1 ){` |
|   18708 | 6284 | `			break;` |
|       - | 6285 | `		}` |
|       - | 6286 | `		/* Extract a copy of the key and a copy the current value */` |
|   82416 | 6287 | `		PH7_HashmapExtractNodeKey(pEntry,&sKey);` |
|   82416 | 6288 | `		PH7_HashmapExtractNodeValue(pEntry,&sValue,FALSE);` |
|       - | 6289 | `		/* Invoke the user callback */` |
|   82416 | 6290 | `		rc = xWalk(&sKey,&sValue,pUserData);` |
|       - | 6291 | `		/* Release the copy of the key and the value */` |
|   82416 | 6292 | `		PH7_MemObjRelease(&sKey);` |
|   82416 | 6293 | `		PH7_MemObjRelease(&sValue);` |
|   82416 | 6294 | `		if( rc != PH7_OK ){` |
|       - | 6295 | `			/* Callback request an operation abort */` |
|     ! 0 | 6296 | `			return SXERR_ABORT;` |
|       - | 6297 | `		}` |
|       - | 6298 | `		/* Point to the next entry */` |
|   82416 | 6299 | `		pEntry = pEntry->pPrev; /* Reverse link */` |
|   82416 | 6300 | `		n--;` |
|       2 | 6301 | `	}` |
|       - | 6302 | `	/* All done */` |
|   18708 | 6303 | `	return SXRET_OK;` |
|    9355 | 6304 |  |
|       - | 6305 |  |
