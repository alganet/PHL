# src/ph7/hashmap.c

<style>code, pre { background: none !important; white-space: pre !important; width: 100% !important; display: inline-block !important; } td { border: none !important; margin-top: 0 !important; margin-bottom: 0 !important; padding-top: 0 !important; padding-bottom: 0 !important; }</style>

Coverage: 2377/2927 lines (81.21%)

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
| 565520 |   18 | `static sxu32 IntHash(sxi64 iKey)` |
|      2 |   19 |  |
| 565522 |   20 | `	return (sxu32)(iKey ^ (iKey << 8) ^ (iKey >> 8));` |
|      2 |   21 |  |
|      - |   22 | `/*` |
|      - |   23 | ` * Default hash function for string/BLOB keys.` |
|      - |   24 | ` */` |
| 200498 |   25 | `static sxu32 BinHash(const void *pSrc,sxu32 nLen)` |
|      2 |   26 |  |
| 200500 |   27 | `	register unsigned char *zIn = (unsigned char *)pSrc;` |
|      - |   28 | `	unsigned char *zEnd;` |
| 200500 |   29 | `	sxu32 nH = 5381;` |
| 200500 |   30 | `	zEnd = &zIn[nLen];` |
| 233507 |   31 | `	for(;;){` |
| 467016 |   32 | `		if( zIn >= zEnd ){ break; } nH = nH * 33 + zIn[0] ; zIn++;` |
| 423422 |   33 | `		if( zIn >= zEnd ){ break; } nH = nH * 33 + zIn[0] ; zIn++;` |
| 384552 |   34 | `		if( zIn >= zEnd ){ break; } nH = nH * 33 + zIn[0] ; zIn++;` |
| 308546 |   35 | `		if( zIn >= zEnd ){ break; } nH = nH * 33 + zIn[0] ; zIn++;` |
|      2 |   36 | `	}` |
| 200500 |   37 | `	return nH;` |
|      2 |   38 |  |
|      - |   39 | `/*` |
|      - |   40 | ` * Return the total number of entries in a given hashmap.` |
|      - |   41 | ` * If bRecurisve is set to TRUE then recurse on hashmap entries.` |
|      - |   42 | ` * If the nesting limit is reached,this function abort immediately.` |
|      - |   43 | ` */` |
|    682 |   44 | `static sxi64 HashmapCount(ph7_hashmap *pMap,int bRecursive,int iRecCount)` |
|      2 |   45 |  |
|    684 |   46 | `	sxi64 iCount = 0;` |
|    684 |   47 | `	if( !bRecursive ){` |
|    408 |   48 | `		iCount = pMap->nEntry;` |
|    205 |   49 | `	}else{` |
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
|    680 |   79 | `	return iCount;` |
|    343 |   80 |  |
|      - |   81 | `/*` |
|      - |   82 | ` * Allocate a new hashmap node with a 64-bit integer key.` |
|      - |   83 | ` * If something goes wrong [i.e: out of memory],this function return NULL.` |
|      - |   84 | ` * Otherwise a fresh [ph7_hashmap_node] instance is returned.` |
|      - |   85 | ` */` |
| 512704 |   86 | `static ph7_hashmap_node * HashmapNewIntNode(ph7_hashmap *pMap,sxi64 iKey,sxu32 nHash,sxu32 nValIdx)` |
|      2 |   87 |  |
|      - |   88 | `	ph7_hashmap_node *pNode;` |
|      - |   89 | `	/* Allocate a new node */` |
| 512706 |   90 | `	pNode = (ph7_hashmap_node *)SyMemBackendPoolAlloc(&pMap->pVm->sAllocator,sizeof(ph7_hashmap_node));` |
| 512706 |   91 | `	if( pNode == 0 ){` |
|    ! 0 |   92 | `		return 0;` |
|      - |   93 | `	}` |
|      - |   94 | `	/* Zero the stucture */` |
| 512706 |   95 | `	SyZero(pNode,sizeof(ph7_hashmap_node));` |
|      - |   96 | `	/* Fill in the structure */` |
| 512706 |   97 | `	pNode->pMap  = &(*pMap);` |
| 512706 |   98 | `	pNode->iType = HASHMAP_INT_NODE;` |
| 512706 |   99 | `	pNode->nHash = nHash;` |
| 512706 |  100 | `	pNode->xKey.iKey = iKey;` |
| 512706 |  101 | `	pNode->nValIdx  = nValIdx;` |
| 512706 |  102 | `	return pNode;` |
| 256354 |  103 |  |
|      - |  104 | `/*` |
|      - |  105 | ` * Allocate a new hashmap node with a BLOB key.` |
|      - |  106 | ` * If something goes wrong [i.e: out of memory],this function return NULL.` |
|      - |  107 | ` * Otherwise a fresh [ph7_hashmap_node] instance is returned.` |
|      - |  108 | ` */` |
|  70034 |  109 | `static ph7_hashmap_node * HashmapNewBlobNode(ph7_hashmap *pMap,const void *pKey,sxu32 nKeyLen,sxu32 nHash,sxu32 nValIdx)` |
|      2 |  110 |  |
|      - |  111 | `	ph7_hashmap_node *pNode;` |
|      - |  112 | `	/* Allocate a new node */` |
|  70036 |  113 | `	pNode = (ph7_hashmap_node *)SyMemBackendPoolAlloc(&pMap->pVm->sAllocator,sizeof(ph7_hashmap_node));` |
|  70036 |  114 | `	if( pNode == 0 ){` |
|    ! 0 |  115 | `		return 0;` |
|      - |  116 | `	}` |
|      - |  117 | `	/* Zero the stucture */` |
|  70036 |  118 | `	SyZero(pNode,sizeof(ph7_hashmap_node));` |
|      - |  119 | `	/* Fill in the structure */` |
|  70036 |  120 | `	pNode->pMap  = &(*pMap);` |
|  70036 |  121 | `	pNode->iType = HASHMAP_BLOB_NODE;` |
|  70036 |  122 | `	pNode->nHash = nHash;` |
|  70036 |  123 | `	SyBlobInit(&pNode->xKey.sKey,&pMap->pVm->sAllocator);` |
|  70036 |  124 | `	SyBlobAppend(&pNode->xKey.sKey,pKey,nKeyLen);` |
|  70036 |  125 | `	pNode->nValIdx = nValIdx;` |
|  70036 |  126 | `	return pNode;` |
|  35019 |  127 |  |
|      - |  128 | `/*` |
|      - |  129 | ` * link a hashmap node to the given bucket index (last argument to this function).` |
|      - |  130 | ` */` |
| 582738 |  131 | `static void HashmapNodeLink(ph7_hashmap *pMap,ph7_hashmap_node *pNode,sxu32 nBucketIdx)` |
|      2 |  132 |  |
|      - |  133 | `	/* Link */` |
| 582740 |  134 | `	if( pMap->apBucket[nBucketIdx] != 0 ){` |
| 424224 |  135 | `		pNode->pNextCollide = pMap->apBucket[nBucketIdx];` |
| 424224 |  136 | `		pMap->apBucket[nBucketIdx]->pPrevCollide = pNode;` |
| 212111 |  137 | `	}` |
| 582740 |  138 | `	pMap->apBucket[nBucketIdx] = pNode;` |
|      - |  139 | `	/* Link to the map list */` |
| 582740 |  140 | `	if( pMap->pFirst == 0 ){` |
|  30788 |  141 | `		pMap->pFirst = pMap->pLast = pNode;` |
|      - |  142 | `		/* Point to the first inserted node */` |
|  30788 |  143 | `		pMap->pCur = pNode;` |
|  15395 |  144 | `	}else{` |
| 551954 |  145 | `		MACRO_LD_PUSH(pMap->pLast,pNode);` |
|      - |  146 | `	}` |
| 582740 |  147 | `	++pMap->nEntry;` |
| 582740 |  148 |  |
|      - |  149 | `/*` |
|      - |  150 | ` * Unlink a node from the hashmap.` |
|      - |  151 | ` * If the node count reaches zero then release the whole hash-bucket.` |
|      - |  152 | ` */` |
|   4936 |  153 | `PH7_PRIVATE void PH7_HashmapUnlinkNode(ph7_hashmap_node *pNode,int bRestore)` |
|      2 |  154 |  |
|   4938 |  155 | `	ph7_hashmap *pMap = pNode->pMap;` |
|   4938 |  156 | `	ph7_vm *pVm = pMap->pVm;` |
|      - |  157 | `	/* Unlink from the corresponding bucket */` |
|   4938 |  158 | `	if( pNode->pPrevCollide == 0 ){` |
|   4512 |  159 | `		pMap->apBucket[pNode->nHash & (pMap->nSize - 1)] = pNode->pNextCollide;` |
|   2257 |  160 | `	}else{` |
|    427 |  161 | `		pNode->pPrevCollide->pNextCollide = pNode->pNextCollide;` |
|      - |  162 | `	}` |
|   4938 |  163 | `	if( pNode->pNextCollide ){` |
|   3737 |  164 | `		pNode->pNextCollide->pPrevCollide = pNode->pPrevCollide;` |
|   1868 |  165 | `	}` |
|   4938 |  166 | `	if( pMap->pFirst == pNode ){` |
|     58 |  167 | `		pMap->pFirst = pNode->pPrev;` |
|     28 |  168 | `	}` |
|   4938 |  169 | `	if( pMap->pCur == pNode ){` |
|      - |  170 | `		/* Advance the node cursor */` |
|     60 |  171 | `		pMap->pCur = pMap->pCur->pPrev; /* Reverse link */` |
|     29 |  172 | `	}` |
|      - |  173 | `	/* Unlink from the map list */` |
|   4938 |  174 | `	MACRO_LD_REMOVE(pMap->pLast,pNode);` |
|   4938 |  175 | `	if( bRestore ){` |
|      - |  176 | `		/* Remove the ph7_value associated with this node from the reference table */` |
|     30 |  177 | `		PH7_VmRefObjRemove(pVm,pNode->nValIdx,0,pNode);` |
|      - |  178 | `		/* Restore to the freelist */` |
|     30 |  179 | `		if( (pNode->iFlags & HASHMAP_NODE_FOREIGN_OBJ) == 0 ){` |
|     30 |  180 | `			PH7_VmUnsetMemObj(pVm,pNode->nValIdx,FALSE);` |
|     14 |  181 | `		}` |
|     14 |  182 | `	}` |
|   4938 |  183 | `	if( pNode->iType == HASHMAP_BLOB_NODE ){` |
|   4889 |  184 | `		SyBlobRelease(&pNode->xKey.sKey);` |
|   2444 |  185 | `	}` |
|   4938 |  186 | `	SyMemBackendPoolFree(&pVm->sAllocator,pNode);` |
|   4938 |  187 | `	pMap->nEntry--;` |
|   4938 |  188 | `	if( pMap->nEntry < 1 && pMap != pVm->pGlobal ){` |
|      - |  189 | `		/* Free the hash-bucket */` |
|     26 |  190 | `		SyMemBackendFree(&pVm->sAllocator,pMap->apBucket);` |
|     26 |  191 | `		pMap->apBucket = 0;` |
|     26 |  192 | `		pMap->nSize = 0;` |
|     26 |  193 | `		pMap->pFirst = pMap->pLast = pMap->pCur = 0;` |
|     12 |  194 | `	}` |
|   4938 |  195 |  |
|      - |  196 | `#define HASHMAP_FILL_FACTOR 3` |
|      - |  197 | `/*` |
|      - |  198 | ` * Grow the hash-table and rehash all entries.` |
|      - |  199 | ` */` |
| 582738 |  200 | `static sxi32 HashmapGrowBucket(ph7_hashmap *pMap)` |
|      2 |  201 |  |
| 582740 |  202 | `	if( pMap->nEntry >= pMap->nSize * HASHMAP_FILL_FACTOR ){` |
|  33840 |  203 | `		ph7_hashmap_node **apOld = pMap->apBucket;` |
|      - |  204 | `		ph7_hashmap_node *pEntry,**apNew;` |
|  33840 |  205 | `		sxu32 nNew = pMap->nSize << 1;` |
|      - |  206 | `		sxu32 nBucket;` |
|      - |  207 | `		sxu32 n;` |
|  33840 |  208 | `		if( nNew < 1 ){` |
|  30788 |  209 | `			nNew = 16;` |
|  15393 |  210 | `		}` |
|      - |  211 | `		/* Allocate a new bucket */` |
|  33840 |  212 | `		apNew = (ph7_hashmap_node **)SyMemBackendAlloc(&pMap->pVm->sAllocator,nNew * sizeof(ph7_hashmap_node *));` |
|  33840 |  213 | `		if( apNew == 0 ){` |
|    ! 0 |  214 | `			if( pMap->nSize < 1 ){` |
|    ! 0 |  215 | `				return SXERR_MEM; /* Fatal */` |
|      - |  216 | `			}` |
|      - |  217 | `			/* Not so fatal here,simply a performance hit */` |
|    ! 0 |  218 | `			return SXRET_OK;` |
|      - |  219 | `		}` |
|      - |  220 | `		/* Zero the table */` |
|  33840 |  221 | `		SyZero((void *)apNew,nNew * sizeof(ph7_hashmap_node *));` |
|      - |  222 | `		/* Reflect the change */` |
|  33840 |  223 | `		pMap->apBucket = apNew;` |
|  33840 |  224 | `		pMap->nSize = nNew;` |
|  33840 |  225 | `		if( apOld == 0 ){` |
|      - |  226 | `			/* First allocated table [i.e: no entry],return immediately */` |
|  30788 |  227 | `			return SXRET_OK;` |
|      - |  228 | `		}` |
|      - |  229 | `		/* Rehash old entries */` |
|   3054 |  230 | `		pEntry = pMap->pFirst;` |
|   3054 |  231 | `		n = 0;` |
| 260630 |  232 | `		for( ;; ){` |
| 521262 |  233 | `			if( n >= pMap->nEntry ){` |
|   3054 |  234 | `				break;` |
|      - |  235 | `			}` |
|      - |  236 | `			/* Clear the old collision link */` |
| 518210 |  237 | `			pEntry->pNextCollide = pEntry->pPrevCollide = 0;` |
|      - |  238 | `			/* Link to the new bucket */` |
| 518210 |  239 | `			nBucket = pEntry->nHash & (nNew - 1);` |
| 518210 |  240 | `			if( pMap->apBucket[nBucket] != 0 ){` |
| 233060 |  241 | `				pEntry->pNextCollide = pMap->apBucket[nBucket];` |
| 233060 |  242 | `				pMap->apBucket[nBucket]->pPrevCollide = pEntry;` |
| 116529 |  243 | `			}` |
| 518210 |  244 | `			pMap->apBucket[nBucket] = pEntry;` |
|      - |  245 | `			/* Point to the next entry */` |
| 518210 |  246 | `			pEntry = pEntry->pPrev; /* Reverse link */` |
| 518210 |  247 | `			n++;` |
|      2 |  248 | `		}` |
|      - |  249 | `		/* Free the old table */` |
|   3054 |  250 | `		SyMemBackendFree(&pMap->pVm->sAllocator,(void *)apOld);` |
|   1526 |  251 | `	}` |
| 551954 |  252 | `	return SXRET_OK;` |
| 291371 |  253 |  |
|      - |  254 | `/*` |
|      - |  255 | ` * Insert a 64-bit integer key and it's associated value (if any) in the given` |
|      - |  256 | ` * hashmap.` |
|      - |  257 | ` */` |
| 512704 |  258 | `static sxi32 HashmapInsertIntKey(ph7_hashmap *pMap,sxi64 iKey,ph7_value *pValue,sxu32 nRefIdx,int isForeign)` |
|      2 |  259 |  |
|      - |  260 | `	ph7_hashmap_node *pNode;` |
|      - |  261 | `	sxu32 nIdx;` |
|      - |  262 | `	sxu32 nHash;` |
|      - |  263 | `	sxi32 rc;` |
| 512706 |  264 | `	if( !isForeign ){` |
|      - |  265 | `		ph7_value *pObj;` |
|      - |  266 | `		/* Reserve a ph7_value for the value */` |
| 512682 |  267 | `		pObj = PH7_ReserveMemObj(pMap->pVm);` |
| 512682 |  268 | `		if( pObj == 0 ){` |
|    ! 0 |  269 | `			return SXERR_MEM;` |
|      - |  270 | `		}` |
| 512682 |  271 | `		if( pValue ){` |
|      - |  272 | `			/* Duplicate the value */` |
| 512682 |  273 | `			PH7_MemObjStore(pValue,pObj);` |
| 256340 |  274 | `		}` |
| 512682 |  275 | `		nIdx = pObj->nIdx;` |
| 256342 |  276 | `	}else{` |
|     25 |  277 | `		nIdx = nRefIdx;` |
|      - |  278 | `	}` |
|      - |  279 | `	/* Hash the key */` |
| 512706 |  280 | `	nHash = pMap->xIntHash(iKey);` |
|      - |  281 | `	/* Allocate a new int node */` |
| 512706 |  282 | `	pNode = HashmapNewIntNode(&(*pMap),iKey,nHash,nIdx);` |
| 512706 |  283 | `	if( pNode == 0 ){` |
|    ! 0 |  284 | `		return SXERR_MEM;` |
|      - |  285 | `	}` |
| 512706 |  286 | `	if( isForeign ){` |
|      - |  287 | `		/* Mark as a foregin entry */` |
|     25 |  288 | `		pNode->iFlags \|= HASHMAP_NODE_FOREIGN_OBJ;` |
|     12 |  289 | `	}` |
|      - |  290 | `	/* Make sure the bucket is big enough to hold the new entry */` |
| 512706 |  291 | `	rc = HashmapGrowBucket(&(*pMap));` |
| 512706 |  292 | `	if( rc != SXRET_OK ){` |
|    ! 0 |  293 | `		SyMemBackendPoolFree(&pMap->pVm->sAllocator,pNode);` |
|    ! 0 |  294 | `		return rc;` |
|      - |  295 | `	}` |
|      - |  296 | `	/* Perform the insertion */` |
| 512706 |  297 | `	HashmapNodeLink(&(*pMap),pNode,nHash & (pMap->nSize - 1));` |
|      - |  298 | `	/* Install in the reference table */` |
| 512706 |  299 | `	PH7_VmRefObjInstall(pMap->pVm,nIdx,0,pNode,0);` |
|      - |  300 | `	/* All done */` |
| 512706 |  301 | `	return SXRET_OK;` |
| 256354 |  302 |  |
|      - |  303 | `/*` |
|      - |  304 | ` * Insert a BLOB key and it's associated value (if any) in the given` |
|      - |  305 | ` * hashmap.` |
|      - |  306 | ` */` |
|  70034 |  307 | `static sxi32 HashmapInsertBlobKey(ph7_hashmap *pMap,const void *pKey,sxu32 nKeyLen,ph7_value *pValue,sxu32 nRefIdx,int isForeign)` |
|      2 |  308 |  |
|      - |  309 | `	ph7_hashmap_node *pNode;` |
|      - |  310 | `	sxu32 nHash;` |
|      - |  311 | `	sxu32 nIdx;` |
|      - |  312 | `	sxi32 rc;` |
|  70036 |  313 | `	if( !isForeign ){` |
|      - |  314 | `		ph7_value *pObj;` |
|      - |  315 | `		/* Reserve a ph7_value for the value */` |
|  54320 |  316 | `		pObj = PH7_ReserveMemObj(pMap->pVm);` |
|  54320 |  317 | `		if( pObj == 0 ){` |
|    ! 0 |  318 | `			return SXERR_MEM;` |
|      - |  319 | `		}` |
|  54320 |  320 | `		if( pValue ){` |
|      - |  321 | `			/* Duplicate the value */` |
|  54320 |  322 | `			PH7_MemObjStore(pValue,pObj);` |
|  27159 |  323 | `		}` |
|  54320 |  324 | `		nIdx = pObj->nIdx;` |
|  27161 |  325 | `	}else{` |
|  15718 |  326 | `		nIdx = nRefIdx;` |
|      - |  327 | `	}` |
|      - |  328 | `	/* Hash the key */` |
|  70036 |  329 | `	nHash = pMap->xBlobHash(pKey,nKeyLen);` |
|      - |  330 | `	/* Allocate a new blob node */` |
|  70036 |  331 | `	pNode = HashmapNewBlobNode(&(*pMap),pKey,nKeyLen,nHash,nIdx);` |
|  70036 |  332 | `	if( pNode == 0 ){` |
|    ! 0 |  333 | `		return SXERR_MEM;` |
|      - |  334 | `	}` |
|  70036 |  335 | `	if( isForeign ){` |
|      - |  336 | `		/* Mark as a foregin entry */` |
|  15718 |  337 | `		pNode->iFlags \|= HASHMAP_NODE_FOREIGN_OBJ;` |
|   7858 |  338 | `	}` |
|      - |  339 | `	/* Make sure the bucket is big enough to hold the new entry */` |
|  70036 |  340 | `	rc = HashmapGrowBucket(&(*pMap));` |
|  70036 |  341 | `	if( rc != SXRET_OK ){` |
|    ! 0 |  342 | `		SyMemBackendPoolFree(&pMap->pVm->sAllocator,pNode);` |
|    ! 0 |  343 | `		return rc;` |
|      - |  344 | `	}` |
|      - |  345 | `	/* Perform the insertion */` |
|  70036 |  346 | `	HashmapNodeLink(&(*pMap),pNode,nHash & (pMap->nSize - 1));` |
|      - |  347 | `	/* Install in the reference table */` |
|  70036 |  348 | `	PH7_VmRefObjInstall(pMap->pVm,nIdx,0,pNode,0);` |
|      - |  349 | `	/* All done */` |
|  70036 |  350 | `	return SXRET_OK;` |
|  35019 |  351 |  |
|      - |  352 | `/*` |
|      - |  353 | ` * Check if a given 64-bit integer key exists in the given hashmap.` |
|      - |  354 | ` * Write a pointer to the target node on success. Otherwise` |
|      - |  355 | ` * SXERR_NOTFOUND is returned on failure.` |
|      - |  356 | ` */` |
|  46238 |  357 | `static sxi32 HashmapLookupIntKey(` |
|      - |  358 | `	ph7_hashmap *pMap,         /* Target hashmap */` |
|      - |  359 | `	sxi64 iKey,                /* lookup key */` |
|      - |  360 | `	ph7_hashmap_node **ppNode  /* OUT: target node on success */` |
|      - |  361 | `	)` |
|      2 |  362 |  |
|      - |  363 | `	ph7_hashmap_node *pNode;` |
|      - |  364 | `	sxu32 nHash;` |
|  46240 |  365 | `	if( pMap->nEntry < 1 ){` |
|      - |  366 | `		/* Don't bother hashing,there is no entry anyway */` |
|    299 |  367 | `		return SXERR_NOTFOUND;` |
|      - |  368 | `	}` |
|      - |  369 | `	/* Hash the key first */` |
|  45942 |  370 | `	nHash = pMap->xIntHash(iKey);` |
|      - |  371 | `	/* Point to the appropriate bucket */` |
|  45942 |  372 | `	pNode = pMap->apBucket[nHash & (pMap->nSize - 1)];` |
|      - |  373 | `	/* Perform the lookup */` |
| 411327 |  374 | `	for(;;){` |
| 822656 |  375 | `		if( pNode == 0 ){` |
|  45543 |  376 | `			break;` |
|      - |  377 | `		}` |
| 777311 |  378 | `		if( pNode->iType == HASHMAP_INT_NODE` |
| 774094 |  379 | `			&& pNode->nHash == nHash` |
| 385739 |  380 | `			&& pNode->xKey.iKey == iKey ){` |
|      - |  381 | `				/* Node found */` |
|    400 |  382 | `				if( ppNode ){` |
|    394 |  383 | `					*ppNode = pNode;` |
|    196 |  384 | `				}` |
|    400 |  385 | `				return SXRET_OK;` |
|      - |  386 | `		}` |
|      - |  387 | `		/* Follow the collision link */` |
| 776715 |  388 | `		pNode = pNode->pNextCollide;` |
|      1 |  389 | `	}` |
|      - |  390 | `	/* No such entry */` |
|  45543 |  391 | `	return SXERR_NOTFOUND;` |
|  23121 |  392 |  |
|      - |  393 | `/*` |
|      - |  394 | ` * Check if a given BLOB key exists in the given hashmap.` |
|      - |  395 | ` * Write a pointer to the target node on success. Otherwise` |
|      - |  396 | ` * SXERR_NOTFOUND is returned on failure.` |
|      - |  397 | ` */` |
| 137006 |  398 | `static sxi32 HashmapLookupBlobKey(` |
|      - |  399 | `	ph7_hashmap *pMap,          /* Target hashmap */` |
|      - |  400 | `	const void *pKey,           /* Lookup key */` |
|      - |  401 | `	sxu32 nKeyLen,              /* Key length in bytes */` |
|      - |  402 | `	ph7_hashmap_node **ppNode   /* OUT: target node on success */` |
|      - |  403 | `	)` |
|      2 |  404 |  |
|      - |  405 | `	ph7_hashmap_node *pNode;` |
|      - |  406 | `	sxu32 nHash;` |
| 137008 |  407 | `	if( pMap->nEntry < 1 ){` |
|      - |  408 | `		/* Don't bother hashing,there is no entry anyway */` |
|   6544 |  409 | `		return SXERR_NOTFOUND;` |
|      - |  410 | `	}` |
|      - |  411 | `	/* Hash the key first */` |
| 130466 |  412 | `	nHash = pMap->xBlobHash(pKey,nKeyLen);` |
|      - |  413 | `	/* Point to the appropriate bucket */` |
| 130466 |  414 | `	pNode = pMap->apBucket[nHash & (pMap->nSize - 1)];` |
|      - |  415 | `	/* Perform the lookup */` |
| 135803 |  416 | `	for(;;){` |
| 271608 |  417 | `		if( pNode == 0 ){` |
|  98460 |  418 | `			break;` |
|      - |  419 | `		}` |
| 189151 |  420 | `		if( pNode->iType == HASHMAP_BLOB_NODE` |
| 171648 |  421 | `			&& pNode->nHash == nHash` |
| 101077 |  422 | `			&& SyBlobLength(&pNode->xKey.sKey) == nKeyLen` |
|  32008 |  423 | `			&& SyMemcmp(SyBlobData(&pNode->xKey.sKey),pKey,nKeyLen) == 0 ){` |
|      - |  424 | `				/* Node found */` |
|  32008 |  425 | `				if( ppNode ){` |
|  31996 |  426 | `					*ppNode = pNode;` |
|  15997 |  427 | `				}` |
|  32008 |  428 | `				return SXRET_OK;` |
|      - |  429 | `		}` |
|      - |  430 | `		/* Follow the collision link */` |
| 141144 |  431 | `		pNode = pNode->pNextCollide;` |
|      2 |  432 | `	}` |
|      - |  433 | `	/* No such entry */` |
|  98460 |  434 | `	return SXERR_NOTFOUND;` |
|  68505 |  435 |  |
|      - |  436 | `/*` |
|      - |  437 | ` * Check if the given BLOB key looks like a decimal number.` |
|      - |  438 | ` * Retrurn TRUE on success.FALSE otherwise.` |
|      - |  439 | ` */` |
| 137200 |  440 | `static int HashmapIsIntKey(SyBlob *pKey)` |
|      2 |  441 |  |
| 137202 |  442 | `	const char *zIn  = (const char *)SyBlobData(pKey);` |
| 137202 |  443 | `	const char *zEnd = &zIn[SyBlobLength(pKey)];` |
| 137202 |  444 | `	if( (int)(zEnd-zIn) > 1 && zIn[0] == '0' ){` |
|      - |  445 | `		/* Octal not decimal number */` |
|      5 |  446 | `		return FALSE;` |
|      - |  447 | `	}` |
| 137198 |  448 | `	if( (zIn[0] == '-' \|\| zIn[0] == '+') && &zIn[1] < zEnd ){` |
|    ! 0 |  449 | `		zIn++;` |
|    ! 0 |  450 | `	}` |
|  68931 |  451 | `	for(;;){` |
| 137864 |  452 | `		if( zIn >= zEnd ){` |
|    233 |  453 | `			return TRUE;` |
|      - |  454 | `		}` |
| 137632 |  455 | `		if( (unsigned char)zIn[0] >= 0xc0 /* UTF-8 stream */  \|\| !SyisDigit(zIn[0]) ){` |
|  68484 |  456 | `			break;` |
|      - |  457 | `		}` |
|    667 |  458 | `		zIn++;` |
|      1 |  459 | `	}` |
|      - |  460 | `	/* Key does not look like a decimal number */` |
| 136966 |  461 | `	return FALSE;` |
|  68602 |  462 |  |
|      - |  463 | `/*` |
|      - |  464 | ` * Check if a given key exists in the given hashmap.` |
|      - |  465 | ` * Write a pointer to the target node on success.` |
|      - |  466 | ` * Otherwise SXERR_NOTFOUND is returned on failure.` |
|      - |  467 | ` */` |
|  67326 |  468 | `static sxi32 HashmapLookup(` |
|      - |  469 | `	ph7_hashmap *pMap,          /* Target hashmap */` |
|      - |  470 | `	ph7_value *pKey,            /* Lookup key */` |
|      - |  471 | `	ph7_hashmap_node **ppNode   /* OUT: target node on success */` |
|      - |  472 | `	)` |
|      2 |  473 |  |
|  67328 |  474 | `	ph7_hashmap_node *pNode = 0; /* cc -O6 warning */` |
|      - |  475 | `	sxi32 rc;` |
|  67328 |  476 | `	if( pKey->iFlags & (MEMOBJ_STRING\|MEMOBJ_HASHMAP\|MEMOBJ_OBJ\|MEMOBJ_RES) ){` |
|  66988 |  477 | `		if( (pKey->iFlags & MEMOBJ_STRING) == 0 ){` |
|      - |  478 | `			/* Force a string cast */` |
|    ! 0 |  479 | `			PH7_MemObjToString(&(*pKey));` |
|    ! 0 |  480 | `		}` |
|  66988 |  481 | `		if( SyBlobLength(&pKey->sBlob) > 0 && !HashmapIsIntKey(&pKey->sBlob) ){` |
|      - |  482 | `			/* Perform a blob lookup */` |
|  66972 |  483 | `			rc = HashmapLookupBlobKey(&(*pMap),SyBlobData(&pKey->sBlob),SyBlobLength(&pKey->sBlob),&pNode);` |
|  66972 |  484 | `			goto result;` |
|      - |  485 | `		}` |
|      8 |  486 | `	}` |
|      - |  487 | `	/* Perform an int lookup */` |
|    358 |  488 | `	if((pKey->iFlags & MEMOBJ_INT) == 0 ){` |
|      - |  489 | `		/* Force an integer cast */` |
|     19 |  490 | `		PH7_MemObjToInteger(pKey);` |
|      9 |  491 | `	}` |
|      - |  492 | `	/* Perform an int lookup */` |
|    358 |  493 | `	rc = HashmapLookupIntKey(&(*pMap),pKey->x.iVal,&pNode);` |
|  33663 |  494 | `result:` |
|  67328 |  495 | `	if( rc == SXRET_OK ){` |
|      - |  496 | `		/* Node found */` |
|  32304 |  497 | `		if( ppNode ){` |
|  32288 |  498 | `			*ppNode = pNode;` |
|  16143 |  499 | `		}` |
|  32304 |  500 | `		return SXRET_OK;` |
|      - |  501 | `	}` |
|      - |  502 | `	/* No such entry */` |
|  35026 |  503 | `	return SXERR_NOTFOUND;` |
|  33665 |  504 |  |
|      - |  505 | `/*` |
|      - |  506 | ` * Insert a given key and it's associated value (if any) in the given` |
|      - |  507 | ` * hashmap.` |
|      - |  508 | ` * If a node with the given key already exists in the database` |
|      - |  509 | ` * then this function overwrite the old value.` |
|      - |  510 | ` */` |
| 566962 |  511 | `static sxi32 HashmapInsert(` |
|      - |  512 | `	ph7_hashmap *pMap, /* Target hashmap */` |
|      - |  513 | `	ph7_value *pKey,   /* Lookup key  */` |
|      - |  514 | `	ph7_value *pVal    /* Node value */` |
|      - |  515 | `	)` |
|      2 |  516 |  |
| 566964 |  517 | `	ph7_hashmap_node *pNode = 0;` |
| 566964 |  518 | `	sxi32 rc = SXRET_OK;` |
| 566964 |  519 | `	if( pKey && pKey->iFlags & (MEMOBJ_STRING\|MEMOBJ_HASHMAP\|MEMOBJ_OBJ\|MEMOBJ_RES) ){` |
|  54530 |  520 | `		if( (pKey->iFlags & MEMOBJ_STRING) == 0 ){` |
|      - |  521 | `			/* Force a string cast */` |
|      8 |  522 | `			PH7_MemObjToString(&(*pKey));` |
|      3 |  523 | `		}` |
|  54530 |  524 | `		if( SyBlobLength(&pKey->sBlob) < 1 \|\| HashmapIsIntKey(&pKey->sBlob) ){` |
|    254 |  525 | `			if(SyBlobLength(&pKey->sBlob) < 1){` |
|      - |  526 | `				/* Automatic index assign */` |
|     32 |  527 | `				pKey = 0;` |
|     15 |  528 | `			}` |
|    254 |  529 | `			goto IntKey;` |
|      - |  530 | `		}` |
|  81416 |  531 | `		if( SXRET_OK == HashmapLookupBlobKey(&(*pMap),SyBlobData(&pKey->sBlob),` |
|  27138 |  532 | `			SyBlobLength(&pKey->sBlob),&pNode) ){` |
|      - |  533 | `				/* Overwrite the old value */` |
|      - |  534 | `				ph7_value *pElem;` |
|     23 |  535 | `				pElem = (ph7_value *)SySetAt(&pMap->pVm->aMemObj,pNode->nValIdx);` |
|     23 |  536 | `				if( pElem ){` |
|     23 |  537 | `					if( pVal ){` |
|     23 |  538 | `						PH7_MemObjStore(pVal,pElem);` |
|     12 |  539 | `					}else{` |
|      - |  540 | `						/* Nullify the entry */` |
|    ! 0 |  541 | `						PH7_MemObjToNull(pElem);` |
|      - |  542 | `					}` |
|     11 |  543 | `				}` |
|     23 |  544 | `				return SXRET_OK;` |
|      - |  545 | `		}` |
|  54256 |  546 | `		if( pMap == pMap->pVm->pGlobal ){` |
|      - |  547 | `			/* Forbidden */` |
|      3 |  548 | `			PH7_VmThrowError(pMap->pVm,0,PH7_CTX_NOTICE,"$GLOBALS is a read-only array,insertion is forbidden");` |
|      3 |  549 | `			return SXRET_OK;` |
|      - |  550 | `		}` |
|      - |  551 | `		/* Perform a blob-key insertion */` |
|  54254 |  552 | `		rc = HashmapInsertBlobKey(&(*pMap),SyBlobData(&pKey->sBlob),SyBlobLength(&pKey->sBlob),&(*pVal),0,FALSE);` |
|  54254 |  553 | `		return rc;` |
|      - |  554 | `	}` |
| 256217 |  555 | `IntKey:` |
| 512688 |  556 | `	if( pKey ){` |
|  23063 |  557 | `		if((pKey->iFlags & MEMOBJ_INT) == 0 ){` |
|      - |  558 | `			/* Force an integer cast */` |
|    247 |  559 | `			PH7_MemObjToInteger(pKey);` |
|    123 |  560 | `		}` |
|  23063 |  561 | `		if( SXRET_OK == HashmapLookupIntKey(&(*pMap),pKey->x.iVal,&pNode) ){` |
|      - |  562 | `			/* Overwrite the old value */` |
|      - |  563 | `			ph7_value *pElem;` |
|     37 |  564 | `			pElem = (ph7_value *)SySetAt(&pMap->pVm->aMemObj,pNode->nValIdx);` |
|     37 |  565 | `			if( pElem ){` |
|     37 |  566 | `				if( pVal ){` |
|     37 |  567 | `					PH7_MemObjStore(pVal,pElem);` |
|     19 |  568 | `				}else{` |
|      - |  569 | `					/* Nullify the entry */` |
|    ! 0 |  570 | `					PH7_MemObjToNull(pElem);` |
|      - |  571 | `				}` |
|     18 |  572 | `			}` |
|     37 |  573 | `			return SXRET_OK;` |
|      - |  574 | `		}` |
|  23027 |  575 | `		if( pMap == pMap->pVm->pGlobal ){` |
|      - |  576 | `			/* Forbidden */` |
|      3 |  577 | `			PH7_VmThrowError(pMap->pVm,0,PH7_CTX_NOTICE,"$GLOBALS is a read-only array,insertion is forbidden");` |
|      3 |  578 | `			return SXRET_OK;` |
|      - |  579 | `		}` |
|      - |  580 | `		/* Perform a 64-bit-int-key insertion */` |
|  23025 |  581 | `		rc = HashmapInsertIntKey(&(*pMap),pKey->x.iVal,&(*pVal),0,FALSE);` |
|  23025 |  582 | `		if( rc == SXRET_OK ){` |
|  23025 |  583 | `			if( pKey->x.iVal >= pMap->iNextIdx ){` |
|      - |  584 | `				/* Increment the automatic index */` |
|  22801 |  585 | `				pMap->iNextIdx = pKey->x.iVal + 1;` |
|      - |  586 | `				/* Make sure the automatic index is not reserved */` |
|  22801 |  587 | `				while( SXRET_OK == HashmapLookupIntKey(&(*pMap),pMap->iNextIdx,0) ){` |
|    ! 0 |  588 | `					pMap->iNextIdx++;` |
|    ! 0 |  589 | `				}` |
|  11400 |  590 | `			}` |
|  11512 |  591 | `		}` |
|  11513 |  592 | `	}else{` |
| 489626 |  593 | `		if( pMap == pMap->pVm->pGlobal ){` |
|      - |  594 | `			/* Forbidden */` |
|      3 |  595 | `			PH7_VmThrowError(pMap->pVm,0,PH7_CTX_NOTICE,"$GLOBALS is a read-only array,insertion is forbidden");` |
|      3 |  596 | `			return SXRET_OK;` |
|      - |  597 | `		}` |
|      - |  598 | `		/* Assign an automatic index */` |
| 489624 |  599 | `		rc = HashmapInsertIntKey(&(*pMap),pMap->iNextIdx,&(*pVal),0,FALSE);` |
| 489624 |  600 | `		if( rc == SXRET_OK ){` |
| 489624 |  601 | `			++pMap->iNextIdx;` |
| 244811 |  602 | `		}` |
|      - |  603 | `	}` |
|      - |  604 | `	/* Insertion result */` |
| 512648 |  605 | `	return rc;` |
| 283483 |  606 |  |
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
|  15746 |  634 | `static sxi32 HashmapInsertByRef(` |
|      - |  635 | `	ph7_hashmap *pMap,   /* Target hashmap */` |
|      - |  636 | `	ph7_value *pKey,     /* Lookup key */` |
|      - |  637 | `	sxu32 nRefIdx        /* Foreign ph7_value index */` |
|      - |  638 | `	)` |
|      2 |  639 |  |
|  15748 |  640 | `	ph7_hashmap_node *pNode = 0;` |
|  15748 |  641 | `	sxi32 rc = SXRET_OK;` |
|  15748 |  642 | `	if( pKey && pKey->iFlags & (MEMOBJ_STRING\|MEMOBJ_HASHMAP\|MEMOBJ_OBJ\|MEMOBJ_RES) ){` |
|  15724 |  643 | `		if( (pKey->iFlags & MEMOBJ_STRING) == 0 ){` |
|      - |  644 | `			/* Force a string cast */` |
|    ! 0 |  645 | `			PH7_MemObjToString(&(*pKey));` |
|    ! 0 |  646 | `		}` |
|  15724 |  647 | `		if( SyBlobLength(&pKey->sBlob) < 1 \|\| HashmapIsIntKey(&pKey->sBlob) ){` |
|    ! 0 |  648 | `			if(SyBlobLength(&pKey->sBlob) < 1){` |
|      - |  649 | `				/* Automatic index assign */` |
|    ! 0 |  650 | `				pKey = 0;` |
|    ! 0 |  651 | `			}` |
|    ! 0 |  652 | `			goto IntKey;` |
|      - |  653 | `		}` |
|  23585 |  654 | `		if( SXRET_OK == HashmapLookupBlobKey(&(*pMap),SyBlobData(&pKey->sBlob),` |
|   7861 |  655 | `			SyBlobLength(&pKey->sBlob),&pNode) ){` |
|      - |  656 | `				/* Overwrite */` |
|      7 |  657 | `				PH7_VmRefObjRemove(pMap->pVm,pNode->nValIdx,0,pNode);` |
|      7 |  658 | `				pNode->nValIdx = nRefIdx;` |
|      - |  659 | `				/* Install in the reference table */` |
|      7 |  660 | `				PH7_VmRefObjInstall(pMap->pVm,nRefIdx,0,pNode,0);` |
|      7 |  661 | `				return SXRET_OK;` |
|      - |  662 | `		}` |
|      - |  663 | `		/* Perform a blob-key insertion */` |
|  15718 |  664 | `		rc = HashmapInsertBlobKey(&(*pMap),SyBlobData(&pKey->sBlob),SyBlobLength(&pKey->sBlob),0,nRefIdx,TRUE);` |
|  15718 |  665 | `		return rc;` |
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
|   7875 |  702 |  |
|      - |  703 | `/*` |
|      - |  704 | ` * Extract node value.` |
|      - |  705 | ` */` |
| 694850 |  706 | `static ph7_value * HashmapExtractNodeValue(ph7_hashmap_node *pNode)` |
|      2 |  707 |  |
|      - |  708 | `	/* Point to the desired object */` |
|      - |  709 | `	ph7_value *pObj;` |
| 694852 |  710 | `	pObj = (ph7_value *)SySetAt(&pNode->pMap->pVm->aMemObj,pNode->nValIdx);` |
| 694852 |  711 | `	return pObj;` |
|      2 |  712 |  |
|      - |  713 | `/*` |
|      - |  714 | ` * Insert a node in the given hashmap.` |
|      - |  715 | ` * If a node with the given key already exists in the database` |
|      - |  716 | ` * then this function overwrite the old value.` |
|      - |  717 | ` */` |
|    156 |  718 | `static sxi32 HashmapInsertNode(ph7_hashmap *pMap,ph7_hashmap_node *pNode,int bPreserve)` |
|      1 |  719 |  |
|      - |  720 | `	ph7_value *pObj;` |
|      - |  721 | `	sxi32 rc;` |
|      - |  722 | `	/* Extract the node value */` |
|    157 |  723 | `	pObj = HashmapExtractNodeValue(&(*pNode));` |
|    157 |  724 | `	if( pObj == 0 ){` |
|    ! 0 |  725 | `		return SXERR_EMPTY;` |
|      - |  726 | `	}` |
|      - |  727 | `	/* Preserve key */` |
|    157 |  728 | `	if( pNode->iType == HASHMAP_INT_NODE){` |
|      - |  729 | `		/* Int64 key */` |
|     61 |  730 | `		if( !bPreserve ){` |
|      - |  731 | `			/* Assign an automatic index */` |
|     39 |  732 | `			rc = HashmapInsert(&(*pMap),0,pObj);` |
|     20 |  733 | `		}else{` |
|     23 |  734 | `			rc = HashmapInsertIntKey(&(*pMap),pNode->xKey.iKey,pObj,0,FALSE);` |
|      - |  735 | `		}` |
|     31 |  736 | `	}else{` |
|      - |  737 | `		/* Blob key */` |
|     97 |  738 | `		if( !bPreserve ){` |
|      - |  739 | `			/* treat it like an automatically-indexed element, drop the` |
|      - |  740 | `			 * original string key entirely */` |
|     33 |  741 | `			rc = HashmapInsert(&(*pMap),0,pObj);` |
|     17 |  742 | `		}else{` |
|     97 |  743 | `			rc = HashmapInsertBlobKey(&(*pMap),SyBlobData(&pNode->xKey.sKey),` |
|     32 |  744 | `				SyBlobLength(&pNode->xKey.sKey),pObj,0,FALSE);` |
|      - |  745 | `		}` |
|      - |  746 | `	}` |
|    157 |  747 | `	return rc;` |
|     79 |  748 |  |
|      - |  749 | `/*` |
|      - |  750 | ` * Compare two node values.` |
|      - |  751 | ` * Return 0 if the node values are equals, > 0 if pLeft is greater than pRight` |
|      - |  752 | ` * or < 0 if pRight is greater than pLeft.` |
|      - |  753 | ` * For a full description on ph7_values comparison,refer to the implementation` |
|      - |  754 | ` * of the [PH7_MemObjCmp()] function defined in memobj.c or the official` |
|      - |  755 | ` * documenation.` |
|      - |  756 | ` */` |
|  32026 |  757 | `static sxi32 HashmapNodeCmp(ph7_hashmap_node *pLeft,ph7_hashmap_node *pRight,int bStrict)` |
|      2 |  758 |  |
|      - |  759 | `	ph7_value sObj1,sObj2;` |
|      - |  760 | `	sxi32 rc;` |
|  32028 |  761 | `	if( pLeft == pRight ){` |
|      - |  762 | `		/*` |
|      - |  763 | `		 * Same node.Refer to the sort() implementation defined` |
|      - |  764 | `		 * below for more information on this sceanario.` |
|      - |  765 | `		 */` |
|    ! 0 |  766 | `		return 0;` |
|      - |  767 | `	}` |
|      - |  768 | `	/* Do the comparison */` |
|  32028 |  769 | `	PH7_MemObjInit(pLeft->pMap->pVm,&sObj1);` |
|  32028 |  770 | `	PH7_MemObjInit(pLeft->pMap->pVm,&sObj2);` |
|  32028 |  771 | `	PH7_HashmapExtractNodeValue(pLeft,&sObj1,FALSE);` |
|  32028 |  772 | `	PH7_HashmapExtractNodeValue(pRight,&sObj2,FALSE);` |
|  32028 |  773 | `	rc = PH7_MemObjCmp(&sObj1,&sObj2,bStrict,0);` |
|  32028 |  774 | `	PH7_MemObjRelease(&sObj1);` |
|  32028 |  775 | `	PH7_MemObjRelease(&sObj2);` |
|  32028 |  776 | `	return rc;` |
|  16032 |  777 |  |
|      - |  778 | `/*` |
|      - |  779 | ` * Rehash a node with a 64-bit integer key.` |
|      - |  780 | ` * Refer to [merge_sort(),array_shift()] implementations for more information.` |
|      - |  781 | ` */` |
|   6876 |  782 | `static void HashmapRehashIntNode(ph7_hashmap_node *pEntry)` |
|      2 |  783 |  |
|   6878 |  784 | `	ph7_hashmap *pMap = pEntry->pMap;` |
|      - |  785 | `	sxu32 nBucket;` |
|      - |  786 | `	/* Remove old collision links */` |
|   6878 |  787 | `	if( pEntry->pPrevCollide ){` |
|   5533 |  788 | `		pEntry->pPrevCollide->pNextCollide = pEntry->pNextCollide;` |
|   2768 |  789 | `	}else{` |
|   1347 |  790 | `		pMap->apBucket[pEntry->nHash & (pMap->nSize - 1)] = pEntry->pNextCollide;` |
|      - |  791 | `	}` |
|   6878 |  792 | `	if( pEntry->pNextCollide ){` |
|    629 |  793 | `		pEntry->pNextCollide->pPrevCollide = pEntry->pPrevCollide;` |
|    315 |  794 | `	}` |
|   6878 |  795 | `	pEntry->pNextCollide = pEntry->pPrevCollide = 0;` |
|      - |  796 | `	/* Compute the new hash */` |
|   6878 |  797 | `	pEntry->nHash = pMap->xIntHash(pMap->iNextIdx);` |
|   6878 |  798 | `	pEntry->xKey.iKey = pMap->iNextIdx;` |
|   6878 |  799 | `	nBucket = pEntry->nHash & (pMap->nSize - 1);` |
|      - |  800 | `	/* Link to the new bucket */` |
|   6878 |  801 | `	pEntry->pNextCollide = pMap->apBucket[nBucket];` |
|   6878 |  802 | `	if( pMap->apBucket[nBucket] ){` |
|   5688 |  803 | `		pMap->apBucket[nBucket]->pPrevCollide = pEntry;` |
|   2848 |  804 | `	}` |
|   6878 |  805 | `	pEntry->pNextCollide = pMap->apBucket[nBucket];` |
|   6878 |  806 | `	pMap->apBucket[nBucket] = pEntry;` |
|      - |  807 | `	/* Increment the automatic index */` |
|   6878 |  808 | `	pMap->iNextIdx++;` |
|   6878 |  809 |  |
|      - |  810 | `/*` |
|      - |  811 | ` * Perform a linear search on a given hashmap.` |
|      - |  812 | ` * Write a pointer to the target node on success.` |
|      - |  813 | ` * Otherwise SXERR_NOTFOUND is returned on failure.` |
|      - |  814 | ` * Refer to [array_intersect(),array_diff(),in_array(),...] implementations` |
|      - |  815 | ` * for more information.` |
|      - |  816 | ` */` |
|  17552 |  817 | `static int HashmapFindValue(` |
|      - |  818 | `	ph7_hashmap *pMap,   /* Target hashmap */` |
|      - |  819 | `	ph7_value *pNeedle,  /* Lookup key */` |
|      - |  820 | `	ph7_hashmap_node **ppNode, /* OUT: target node on success  */` |
|      - |  821 | `	int bStrict      /* TRUE for strict comparison */` |
|      - |  822 | `	)` |
|      2 |  823 |  |
|      - |  824 | `	ph7_hashmap_node *pEntry;` |
|      - |  825 | `	ph7_value sVal,*pVal;` |
|      - |  826 | `	ph7_value sNeedle;` |
|      - |  827 | `	sxi32 rc;` |
|      - |  828 | `	sxu32 n;` |
|      - |  829 | `	/* Perform a linear search since we cannot sort the hashmap based on values */` |
|  17554 |  830 | `	pEntry = pMap->pFirst;` |
|  17554 |  831 | `	n = pMap->nEntry;` |
|  17554 |  832 | `	PH7_MemObjInit(pMap->pVm,&sVal);` |
|  17554 |  833 | `	PH7_MemObjInit(pMap->pVm,&sNeedle);` |
|  41969 |  834 | `	for(;;){` |
|  83940 |  835 | `		if( n < 1 ){` |
|     19 |  836 | `			break;` |
|      - |  837 | `		}` |
|      - |  838 | `		/* Extract node value */` |
|  83922 |  839 | `		pVal = HashmapExtractNodeValue(pEntry);` |
|  83922 |  840 | `		if( pVal ){` |
|  83922 |  841 | `			if( (pVal->iFlags\|pNeedle->iFlags) & MEMOBJ_NULL ){` |
|    ! 0 |  842 | `				sxi32 iF1 = pVal->iFlags&~MEMOBJ_AUX;` |
|    ! 0 |  843 | `				sxi32 iF2 = pNeedle->iFlags&~MEMOBJ_AUX;` |
|    ! 0 |  844 | `				if( iF1 == iF2 ){` |
|      - |  845 | `					/* NULL values are equals */` |
|    ! 0 |  846 | `					if( ppNode ){` |
|    ! 0 |  847 | `						*ppNode = pEntry;` |
|    ! 0 |  848 | `					}` |
|    ! 0 |  849 | `					return SXRET_OK;` |
|      - |  850 | `				}` |
|    ! 0 |  851 | `			}else{` |
|      - |  852 | `				/* Duplicate value */` |
|  83922 |  853 | `				PH7_MemObjLoad(pVal,&sVal);` |
|  83922 |  854 | `				PH7_MemObjLoad(pNeedle,&sNeedle);` |
|  83922 |  855 | `				rc = PH7_MemObjCmp(&sNeedle,&sVal,bStrict,0);` |
|  83922 |  856 | `				PH7_MemObjRelease(&sVal);` |
|  83922 |  857 | `				PH7_MemObjRelease(&sNeedle);` |
|  83922 |  858 | `				if( rc == 0 ){` |
|  17536 |  859 | `					if( ppNode ){` |
|      5 |  860 | `						*ppNode = pEntry;` |
|      2 |  861 | `					}` |
|      - |  862 | `					/* Match found*/` |
|  17536 |  863 | `					return SXRET_OK;` |
|      - |  864 | `				}` |
|      - |  865 | `			}` |
|  33193 |  866 | `		}` |
|      - |  867 | `		/* Point to the next entry */` |
|  66388 |  868 | `		pEntry = pEntry->pPrev; /* Reverse link */` |
|  66388 |  869 | `		n--;` |
|      2 |  870 | `	}` |
|      - |  871 | `	/* No such entry */` |
|     19 |  872 | `	return SXERR_NOTFOUND;` |
|   8778 |  873 |  |
|      - |  874 | `/*` |
|      - |  875 | ` * Perform a linear search on a given hashmap but use an user-defined callback` |
|      - |  876 | ` * for values comparison.` |
|      - |  877 | ` * Write a pointer to the target node on success.` |
|      - |  878 | ` * Otherwise SXERR_NOTFOUND is returned on failure.` |
|      - |  879 | ` * Refer to [array_uintersect(),array_udiff()...] implementations` |
|      - |  880 | ` * for more information.` |
|      - |  881 | ` */` |
|     16 |  882 | `static int HashmapFindValueByCallback(` |
|      - |  883 | `	ph7_hashmap *pMap,     /* Target hashmap */` |
|      - |  884 | `	ph7_value *pNeedle,    /* Lookup key */` |
|      - |  885 | `	ph7_value *pCallback,  /* User defined callback */` |
|      - |  886 | `	ph7_hashmap_node **ppNode /* OUT: target node on success */` |
|      - |  887 | `	)` |
|      1 |  888 |  |
|      - |  889 | `	ph7_hashmap_node *pEntry;` |
|      - |  890 | `	ph7_value sResult,*pVal;` |
|      - |  891 | `	ph7_value *apArg[2];    /* Callback arguments */` |
|      - |  892 | `	sxi32 rc;` |
|      - |  893 | `	sxu32 n;` |
|      - |  894 | `	/* Perform a linear search since we cannot sort the array based on values */` |
|     17 |  895 | `	pEntry = pMap->pFirst;` |
|     17 |  896 | `	n = pMap->nEntry;` |
|      - |  897 | `	/* Store callback result here */` |
|     17 |  898 | `	PH7_MemObjInit(pMap->pVm,&sResult);` |
|      - |  899 | `	/* First argument to the callback */` |
|     17 |  900 | `	apArg[0] = pNeedle;` |
|     21 |  901 | `	for(;;){` |
|     43 |  902 | `		if( n < 1 ){` |
|      9 |  903 | `			break;` |
|      - |  904 | `		}` |
|      - |  905 | `		/* Extract node value */` |
|     35 |  906 | `		pVal = HashmapExtractNodeValue(pEntry);` |
|     35 |  907 | `		if( pVal ){` |
|      - |  908 | `			/* Invoke the user callback */` |
|     35 |  909 | `			apArg[1] = pVal; /* Second argument to the callback */` |
|     35 |  910 | `			rc = PH7_VmCallUserFunction(pMap->pVm,pCallback,2,apArg,&sResult);` |
|     35 |  911 | `			if( rc == SXRET_OK ){` |
|      - |  912 | `				/* Extract callback result */` |
|     35 |  913 | `				if( (sResult.iFlags & MEMOBJ_INT) == 0 ){` |
|      - |  914 | `					/* Perform an int cast */` |
|    ! 0 |  915 | `					PH7_MemObjToInteger(&sResult);` |
|    ! 0 |  916 | `				}` |
|     35 |  917 | `				rc = (sxi32)sResult.x.iVal;` |
|     35 |  918 | `				PH7_MemObjRelease(&sResult);` |
|     35 |  919 | `				if( rc == 0 ){` |
|      - |  920 | `					/* Match found*/` |
|      9 |  921 | `					if( ppNode ){` |
|      3 |  922 | `						*ppNode = pEntry;` |
|      1 |  923 | `					}` |
|      9 |  924 | `					return SXRET_OK;` |
|      - |  925 | `				}` |
|     13 |  926 | `			}` |
|     13 |  927 | `		}` |
|      - |  928 | `		/* Point to the next entry */` |
|     27 |  929 | `		pEntry = pEntry->pPrev; /* Reverse link */` |
|     27 |  930 | `		n--;` |
|      1 |  931 | `	}` |
|      - |  932 | `	/* No such entry */` |
|      9 |  933 | `	return SXERR_NOTFOUND;` |
|      9 |  934 |  |
|      - |  935 | `/*` |
|      - |  936 | ` * Compare two hashmaps.` |
|      - |  937 | ` * Return 0 if the hashmaps are equals.Any other value indicates inequality.` |
|      - |  938 | ` * Note on array comparison operators.` |
|      - |  939 | ` *  According to the PHP language reference manual.` |
|      - |  940 | ` *  Array Operators Example 	Name 	Result` |
|      - |  941 | ` *  $a + $b 	Union 	Union of $a and $b.` |
|      - |  942 | ` *  $a == $b 	Equality 	TRUE if $a and $b have the same key/value pairs.` |
|      - |  943 | ` *  $a === $b 	Identity 	TRUE if $a and $b have the same key/value pairs in the same` |
|      - |  944 | ` *                          order and of the same types.` |
|      - |  945 | ` *  $a != $b 	Inequality 	TRUE if $a is not equal to $b.` |
|      - |  946 | ` *  $a <> $b 	Inequality 	TRUE if $a is not equal to $b.` |
|      - |  947 | ` *  $a !== $b 	Non-identity 	TRUE if $a is not identical to $b.` |
|      - |  948 | ` * The + operator returns the right-hand array appended to the left-hand array;` |
|      - |  949 | ` * For keys that exist in both arrays, the elements from the left-hand array will be used` |
|      - |  950 | ` * and the matching elements from the right-hand array will be ignored.` |
|      - |  951 | ` * <?php` |
|      - |  952 | ` * $a = array("a" => "apple", "b" => "banana");` |
|      - |  953 | ` * $b = array("a" => "pear", "b" => "strawberry", "c" => "cherry");` |
|      - |  954 | ` * $c = $a + $b; // Union of $a and $b` |
|      - |  955 | ` * echo "Union of \$a and \$b: \n";` |
|      - |  956 | ` * var_dump($c);` |
|      - |  957 | ` * $c = $b + $a; // Union of $b and $a` |
|      - |  958 | ` * echo "Union of \$b and \$a: \n";` |
|      - |  959 | ` * var_dump($c);` |
|      - |  960 | ` * ?>` |
|      - |  961 | ` * When executed, this script will print the following:` |
|      - |  962 | ` * Union of $a and $b:` |
|      - |  963 | ` * array(3) {` |
|      - |  964 | ` *  ["a"]=>` |
|      - |  965 | ` *  string(5) "apple"` |
|      - |  966 | ` *  ["b"]=>` |
|      - |  967 | ` * string(6) "banana"` |
|      - |  968 | ` *  ["c"]=>` |
|      - |  969 | ` * string(6) "cherry"` |
|      - |  970 | ` * }` |
|      - |  971 | ` * Union of $b and $a:` |
|      - |  972 | ` * array(3) {` |
|      - |  973 | ` * ["a"]=>` |
|      - |  974 | ` * string(4) "pear"` |
|      - |  975 | ` * ["b"]=>` |
|      - |  976 | ` * string(10) "strawberry"` |
|      - |  977 | ` * ["c"]=>` |
|      - |  978 | ` * string(6) "cherry"` |
|      - |  979 | ` * }` |
|      - |  980 | ` * Elements of arrays are equal for the comparison if they have the same key and value.` |
|      - |  981 | ` */` |
|      8 |  982 | `PH7_PRIVATE sxi32 PH7_HashmapCmp(` |
|      - |  983 | `	ph7_hashmap *pLeft,  /* Left hashmap */` |
|      - |  984 | `	ph7_hashmap *pRight, /* Right hashmap */` |
|      - |  985 | `	int bStrict          /* TRUE for strict comparison */` |
|      - |  986 | `	)` |
|      1 |  987 |  |
|      - |  988 | `	ph7_hashmap_node *pLe,*pRe;` |
|      - |  989 | `	sxi32 rc;` |
|      - |  990 | `	sxu32 n;` |
|      9 |  991 | `	if( pLeft == pRight ){` |
|      - |  992 | `		/* Same hashmap instance. This can easily happen since hashmaps are passed by reference.` |
|      - |  993 | `		 * Unlike the zend engine.` |
|      - |  994 | `		 */` |
|    ! 0 |  995 | `		return 0;` |
|      - |  996 | `	}` |
|      9 |  997 | `	if( pLeft->nEntry != pRight->nEntry ){` |
|      - |  998 | `		/* Must have the same number of entries */` |
|    ! 0 |  999 | `		return pLeft->nEntry > pRight->nEntry ? 1 : -1;` |
|      - | 1000 | `	}` |
|      - | 1001 | `	/* Point to the first inserted entry of the left hashmap */` |
|      9 | 1002 | `	pLe = pLeft->pFirst;` |
|      9 | 1003 | `	pRe = 0; /* cc warning */` |
|      - | 1004 | `	/* Perform the comparison */` |
|      9 | 1005 | `	n = pLeft->nEntry;` |
|      8 | 1006 | `	for(;;){` |
|     17 | 1007 | `		if( n < 1 ){` |
|      7 | 1008 | `			break;` |
|      - | 1009 | `		}` |
|     11 | 1010 | `		if( pLe->iType == HASHMAP_INT_NODE){` |
|      - | 1011 | `			/* Int key */` |
|      7 | 1012 | `			rc = HashmapLookupIntKey(&(*pRight),pLe->xKey.iKey,&pRe);` |
|      4 | 1013 | `		}else{` |
|      5 | 1014 | `			SyBlob *pKey = &pLe->xKey.sKey;` |
|      - | 1015 | `			/* Blob key */` |
|      5 | 1016 | `			rc = HashmapLookupBlobKey(&(*pRight),SyBlobData(pKey),SyBlobLength(pKey),&pRe);` |
|      - | 1017 | `		}` |
|     11 | 1018 | `		if( rc != SXRET_OK ){` |
|      - | 1019 | `			/* No such entry in the right side */` |
|    ! 0 | 1020 | `			return 1;` |
|      - | 1021 | `		}` |
|     11 | 1022 | `		rc = 0;` |
|     11 | 1023 | `		if( bStrict ){` |
|      - | 1024 | `			/* Make sure,the keys are of the same type */` |
|      3 | 1025 | `			if( pLe->iType != pRe->iType ){` |
|    ! 0 | 1026 | `				rc = 1;` |
|    ! 0 | 1027 | `			}` |
|      1 | 1028 | `		}` |
|     11 | 1029 | `		if( !rc ){` |
|      - | 1030 | `			/* Compare nodes */` |
|     11 | 1031 | `			rc = HashmapNodeCmp(pLe,pRe,bStrict);` |
|      5 | 1032 | `		}` |
|     11 | 1033 | `		if( rc != 0 ){` |
|      - | 1034 | `			/* Nodes key/value differ */` |
|      3 | 1035 | `			return rc;` |
|      - | 1036 | `		}` |
|      - | 1037 | `		/* Point to the next entry */` |
|      9 | 1038 | `		pLe = pLe->pPrev; /* Reverse link */` |
|      9 | 1039 | `		n--;` |
|      1 | 1040 | `	}` |
|      7 | 1041 | `	return 0; /* Hashmaps are equals */` |
|      5 | 1042 |  |
|      - | 1043 | `/*` |
|      - | 1044 | ` * Duplicate a hashmap node.` |
|      - | 1045 | ` * This function is used by HashmapMerge, HashmapOverwrite and PH7_HashmapDup.` |
|      - | 1046 | ` */` |
| 298252 | 1047 | `static sxi32 HashmapDuplicateNode(` |
|      - | 1048 | `	ph7_hashmap *pDest,` |
|      - | 1049 | `	ph7_hashmap_node *pEntry,` |
|      - | 1050 | `	ph7_value *pVal,` |
|      - | 1051 | `	int iAction /* 0: Merge, 1: Overwrite, 2: Dup */` |
|      - | 1052 | `	)` |
|      2 | 1053 |  |
| 298254 | 1054 | `	ph7_value sSafeVal = *pVal;` |
|      - | 1055 | `	ph7_value sKey;` |
|      - | 1056 | `	sxi32 rc;` |
|      - | 1057 |  |
| 298254 | 1058 | `	if( pEntry->iType == HASHMAP_BLOB_NODE ){` |
|      - | 1059 | `		/* Blob key insertion */` |
|     19 | 1060 | `		PH7_MemObjInitFromString(pDest->pVm,&sKey,0);` |
|     19 | 1061 | `		PH7_MemObjStringAppend(&sKey,(const char *)SyBlobData(&pEntry->xKey.sKey),SyBlobLength(&pEntry->xKey.sKey));` |
|     19 | 1062 | `		rc = PH7_HashmapInsert(pDest,&sKey,&sSafeVal);` |
|     19 | 1063 | `		PH7_MemObjRelease(&sKey);` |
|     10 | 1064 | `	}else{` |
|      - | 1065 | `		/* Int key */` |
| 298236 | 1066 | `		if( iAction == 0 ){ /* Merge */` |
| 298220 | 1067 | `			rc = HashmapInsert(pDest,0/* Automatic index assign */,&sSafeVal);` |
| 149127 | 1068 | `		}else if( iAction == 1 ){ /* Overwrite */` |
|      5 | 1069 | `			PH7_MemObjInitFromInt(pDest->pVm,&sKey,pEntry->xKey.iKey);` |
|      5 | 1070 | `			rc = PH7_HashmapInsert(pDest,&sKey,&sSafeVal);` |
|      5 | 1071 | `			PH7_MemObjRelease(&sKey);` |
|      3 | 1072 | `		}else{ /* Dup */` |
|     14 | 1073 | `			rc = HashmapInsertIntKey(pDest,pEntry->xKey.iKey,&sSafeVal,0,FALSE);` |
|      - | 1074 | `		}` |
|      - | 1075 | `	}` |
| 298254 | 1076 | `	return rc;` |
|      2 | 1077 |  |
|      - | 1078 | `/*` |
|      - | 1079 | ` * Merge two hashmaps.` |
|      - | 1080 | ` * Note on the merge process` |
|      - | 1081 | ` * According to the PHP language reference manual.` |
|      - | 1082 | ` *  Merges the elements of two arrays together so that the values of one are appended` |
|      - | 1083 | ` *  to the end of the previous one. It returns the resulting array (pDest).` |
|      - | 1084 | ` *  If the input arrays have the same string keys, then the later value for that key` |
|      - | 1085 | ` *  will overwrite the previous one. If, however, the arrays contain numeric keys` |
|      - | 1086 | ` *  the later value will not overwrite the original value, but will be appended.` |
|      - | 1087 | ` *  Values in the input array with numeric keys will be renumbered with incrementing` |
|      - | 1088 | ` *  keys starting from zero in the result array.` |
|      - | 1089 | ` */` |
|   1540 | 1090 | `static sxi32 HashmapMerge(ph7_hashmap *pSrc,ph7_hashmap *pDest)` |
|      2 | 1091 |  |
|      - | 1092 | `	ph7_hashmap_node *pEntry;` |
|      - | 1093 | `	ph7_value *pVal;` |
|      - | 1094 | `	sxi32 rc;` |
|      - | 1095 | `	sxu32 n;` |
|   1542 | 1096 | `	if( pSrc == pDest ){` |
|      - | 1097 | `		/* Same map. This can easily happen since hashmaps are passed by reference.` |
|      - | 1098 | `		 * Unlike the zend engine.` |
|      - | 1099 | `		 */` |
|    ! 0 | 1100 | `		return SXRET_OK;` |
|      - | 1101 | `	}` |
|      - | 1102 | `	/* Point to the first inserted entry in the source */` |
|   1542 | 1103 | `	pEntry = pSrc->pFirst;` |
|      - | 1104 | `	/* Perform the merge */` |
| 299766 | 1105 | `	for( n = 0 ; n < pSrc->nEntry ; ++n ){` |
|      - | 1106 | `		/* Extract the node value */` |
| 298226 | 1107 | `		pVal = HashmapExtractNodeValue(pEntry);` |
| 298226 | 1108 | `		if( pVal ){` |
|      - | 1109 | `			/* Make a local copy of the value.` |
|      - | 1110 | `			 * The insertion call below may trigger a memory pool reallocation` |
|      - | 1111 | `			 * which will invalidate the 'pVal' pointer since it points` |
|      - | 1112 | `			 * to the old pool.` |
|      - | 1113 | `			 */` |
| 298226 | 1114 | `			rc = HashmapDuplicateNode(pDest,pEntry,pVal,0);` |
| 149114 | 1115 | `		}else{` |
|    ! 0 | 1116 | `			rc = SXRET_OK;` |
|      - | 1117 | `		}` |
| 298226 | 1118 | `		if( rc != SXRET_OK ){` |
|    ! 0 | 1119 | `			return rc;` |
|      - | 1120 | `		}` |
|      - | 1121 | `		/* Point to the next entry */` |
| 298226 | 1122 | `		pEntry = pEntry->pPrev; /* Reverse link */` |
| 149114 | 1123 | `	}` |
|   1542 | 1124 | `	return SXRET_OK;` |
|    772 | 1125 |  |
|      - | 1126 | `/*` |
|      - | 1127 | ` * Overwrite entries with the same key.` |
|      - | 1128 | ` * Refer to the [array_replace()] implementation for more information.` |
|      - | 1129 | ` *  According to the PHP language reference manual.` |
|      - | 1130 | ` *  array_replace() replaces the values of the first array with the same values` |
|      - | 1131 | ` *  from all the following arrays. If a key from the first array exists in the second` |
|      - | 1132 | ` *  array, its value will be replaced by the value from the second array. If the key` |
|      - | 1133 | ` *  exists in the second array, and not the first, it will be created in the first array.` |
|      - | 1134 | ` *  If a key only exists in the first array, it will be left as is. If several arrays` |
|      - | 1135 | ` *  are passed for replacement, they will be processed in order, the later arrays` |
|      - | 1136 | ` *  overwriting the previous values.` |
|      - | 1137 | ` *  array_replace() is not recursive : it will replace values in the first array` |
|      - | 1138 | ` *  by whatever type is in the second array.` |
|      - | 1139 | ` */` |
|      4 | 1140 | `static sxi32 HashmapOverwrite(ph7_hashmap *pSrc,ph7_hashmap *pDest)` |
|      1 | 1141 |  |
|      - | 1142 | `	ph7_hashmap_node *pEntry;` |
|      - | 1143 | `	ph7_value *pVal;` |
|      - | 1144 | `	sxi32 rc;` |
|      - | 1145 | `	sxu32 n;` |
|      5 | 1146 | `	if( pSrc == pDest ){` |
|      - | 1147 | `		/* Same map. This can easily happen since hashmaps are passed by reference.` |
|      - | 1148 | `		 * Unlike the zend engine.` |
|      - | 1149 | `		 */` |
|    ! 0 | 1150 | `		return SXRET_OK;` |
|      - | 1151 | `	}` |
|      - | 1152 | `	/* Point to the first inserted entry in the source */` |
|      5 | 1153 | `	pEntry = pSrc->pFirst;` |
|      - | 1154 | `	/* Perform the merge */` |
|     13 | 1155 | `	for( n = 0 ; n < pSrc->nEntry ; ++n ){` |
|      - | 1156 | `		/* Extract the node value */` |
|      9 | 1157 | `		pVal = HashmapExtractNodeValue(pEntry);` |
|      9 | 1158 | `		if( pVal ){` |
|      9 | 1159 | `			rc = HashmapDuplicateNode(pDest,pEntry,pVal,1);` |
|      5 | 1160 | `		}else{` |
|    ! 0 | 1161 | `			rc = SXRET_OK;` |
|      - | 1162 | `		}` |
|      9 | 1163 | `		if( rc != SXRET_OK ){` |
|    ! 0 | 1164 | `			return rc;` |
|      - | 1165 | `		}` |
|      - | 1166 | `		/* Point to the next entry */` |
|      9 | 1167 | `		pEntry = pEntry->pPrev; /* Reverse link */` |
|      5 | 1168 | `	}` |
|      5 | 1169 | `	return SXRET_OK;` |
|      3 | 1170 |  |
|      - | 1171 | `/*` |
|      - | 1172 | ` * Duplicate the contents of a hashmap. Store the copy in pDest.` |
|      - | 1173 | ` * Refer to the [array_pad(),array_copy(),...] implementation for more information.` |
|      - | 1174 | ` */` |
|     10 | 1175 | `PH7_PRIVATE sxi32 PH7_HashmapDup(ph7_hashmap *pSrc,ph7_hashmap *pDest)` |
|      2 | 1176 |  |
|      - | 1177 | `	ph7_hashmap_node *pEntry;` |
|      - | 1178 | `	ph7_value *pVal;` |
|      - | 1179 | `	sxi32 rc;` |
|      - | 1180 | `	sxu32 n;` |
|     12 | 1181 | `	if( pSrc == pDest ){` |
|      - | 1182 | `		/* Same map. This can easily happen since hashmaps are passed by reference.` |
|      - | 1183 | `		 * Unlike the zend engine.` |
|      - | 1184 | `		 */` |
|    ! 0 | 1185 | `		return SXRET_OK;` |
|      - | 1186 | `	}` |
|      - | 1187 | `	/* Point to the first inserted entry in the source */` |
|     12 | 1188 | `	pEntry = pSrc->pFirst;` |
|      - | 1189 | `	/* Perform the duplication */` |
|     32 | 1190 | `	for( n = 0 ; n < pSrc->nEntry ; ++n ){` |
|      - | 1191 | `		/* Extract the node value */` |
|     22 | 1192 | `		pVal = HashmapExtractNodeValue(pEntry);` |
|     22 | 1193 | `		if( pVal ){` |
|     22 | 1194 | `			rc = HashmapDuplicateNode(pDest,pEntry,pVal,2);` |
|     12 | 1195 | `		}else{` |
|    ! 0 | 1196 | `			rc = SXRET_OK;` |
|      - | 1197 | `		}` |
|     22 | 1198 | `		if( rc != SXRET_OK ){` |
|    ! 0 | 1199 | `			return rc;` |
|      - | 1200 | `		}` |
|      - | 1201 | `		/* Point to the next entry */` |
|     22 | 1202 | `		pEntry = pEntry->pPrev; /* Reverse link */` |
|     12 | 1203 | `	}` |
|     12 | 1204 | `	return SXRET_OK;` |
|      7 | 1205 |  |
|      - | 1206 | `/*` |
|      - | 1207 | ` * Perform the union of two hashmaps.` |
|      - | 1208 | ` * This operation is performed only if the user uses the '+' operator` |
|      - | 1209 | ` * with a variable holding an array as follows:` |
|      - | 1210 | ` * <?php` |
|      - | 1211 | ` * $a = array("a" => "apple", "b" => "banana");` |
|      - | 1212 | ` * $b = array("a" => "pear", "b" => "strawberry", "c" => "cherry");` |
|      - | 1213 | ` * $c = $a + $b; // Union of $a and $b` |
|      - | 1214 | ` * echo "Union of \$a and \$b: \n";` |
|      - | 1215 | ` * var_dump($c);` |
|      - | 1216 | ` * $c = $b + $a; // Union of $b and $a` |
|      - | 1217 | ` * echo "Union of \$b and \$a: \n";` |
|      - | 1218 | ` * var_dump($c);` |
|      - | 1219 | ` * ?>` |
|      - | 1220 | ` * When executed, this script will print the following:` |
|      - | 1221 | ` * Union of $a and $b:` |
|      - | 1222 | ` * array(3) {` |
|      - | 1223 | ` *  ["a"]=>` |
|      - | 1224 | ` *  string(5) "apple"` |
|      - | 1225 | ` *  ["b"]=>` |
|      - | 1226 | ` * string(6) "banana"` |
|      - | 1227 | ` *  ["c"]=>` |
|      - | 1228 | ` * string(6) "cherry"` |
|      - | 1229 | ` * }` |
|      - | 1230 | ` * Union of $b and $a:` |
|      - | 1231 | ` * array(3) {` |
|      - | 1232 | ` * ["a"]=>` |
|      - | 1233 | ` * string(4) "pear"` |
|      - | 1234 | ` * ["b"]=>` |
|      - | 1235 | ` * string(10) "strawberry"` |
|      - | 1236 | ` * ["c"]=>` |
|      - | 1237 | ` * string(6) "cherry"` |
|      - | 1238 | ` * }` |
|      - | 1239 | ` * The + operator returns the right-hand array appended to the left-hand array;` |
|      - | 1240 | ` * For keys that exist in both arrays, the elements from the left-hand array will be used` |
|      - | 1241 | ` * and the matching elements from the right-hand array will be ignored.` |
|      - | 1242 | ` */` |
|      4 | 1243 | `PH7_PRIVATE sxi32 PH7_HashmapUnion(ph7_hashmap *pLeft,ph7_hashmap *pRight)` |
|      2 | 1244 |  |
|      - | 1245 | `	ph7_hashmap_node *pEntry;` |
|      6 | 1246 | `	sxi32 rc = SXRET_OK;` |
|      - | 1247 | `	ph7_value *pObj;` |
|      - | 1248 | `	sxu32 n;` |
|      6 | 1249 | `	if( pLeft == pRight ){` |
|      - | 1250 | `		/* Same map. This can easily happen since hashmaps are passed by reference.` |
|      - | 1251 | `		 * Unlike the zend engine.` |
|      - | 1252 | `		 */` |
|    ! 0 | 1253 | `		return SXRET_OK;` |
|      - | 1254 | `	}` |
|      - | 1255 | `	/* Perform the union */` |
|      6 | 1256 | `	pEntry = pRight->pFirst;` |
|     16 | 1257 | `	for(n = 0 ; n < pRight->nEntry ; ++n ){` |
|      - | 1258 | `		/* Make sure the given key does not exists in the left array */` |
|     12 | 1259 | `		if( pEntry->iType == HASHMAP_BLOB_NODE ){` |
|      - | 1260 | `			/* BLOB key */` |
|      7 | 1261 | `			if( SXRET_OK !=` |
|      6 | 1262 | `				HashmapLookupBlobKey(&(*pLeft),SyBlobData(&pEntry->xKey.sKey),SyBlobLength(&pEntry->xKey.sKey),0) ){` |
|      3 | 1263 | `					pObj = HashmapExtractNodeValue(pEntry);` |
|      3 | 1264 | `					if( pObj ){` |
|      3 | 1265 | `						ph7_value sSafeVal = *pObj;` |
|      - | 1266 | `						/* Perform the insertion */` |
|      3 | 1267 | `						rc = HashmapInsertBlobKey(&(*pLeft),SyBlobData(&pEntry->xKey.sKey),SyBlobLength(&pEntry->xKey.sKey),` |
|      - | 1268 | `							&sSafeVal,0,FALSE);` |
|      3 | 1269 | `						if( rc != SXRET_OK ){` |
|    ! 0 | 1270 | `							return rc;` |
|      - | 1271 | `						}` |
|      1 | 1272 | `					}` |
|      1 | 1273 | `			}` |
|      4 | 1274 | `		}else{` |
|      - | 1275 | `			/* INT key */` |
|      5 | 1276 | `			if( SXRET_OK != HashmapLookupIntKey(&(*pLeft),pEntry->xKey.iKey,0) ){` |
|    ! 0 | 1277 | `				pObj = HashmapExtractNodeValue(pEntry);` |
|    ! 0 | 1278 | `				if( pObj ){` |
|    ! 0 | 1279 | `					ph7_value sSafeVal = *pObj;` |
|      - | 1280 | `					/* Perform the insertion */` |
|    ! 0 | 1281 | `					rc = HashmapInsertIntKey(&(*pLeft),pEntry->xKey.iKey,&sSafeVal,0,FALSE);` |
|    ! 0 | 1282 | `					if( rc != SXRET_OK ){` |
|    ! 0 | 1283 | `						return rc;` |
|      - | 1284 | `					}` |
|    ! 0 | 1285 | `				}` |
|    ! 0 | 1286 | `			}` |
|      - | 1287 | `		}` |
|      - | 1288 | `		/* Point to the next entry */` |
|     12 | 1289 | `		pEntry = pEntry->pPrev; /* Reverse link */` |
|      7 | 1290 | `	}` |
|      6 | 1291 | `	return SXRET_OK;` |
|      4 | 1292 |  |
|      - | 1293 | `/*` |
|      - | 1294 | ` * Allocate a new hashmap.` |
|      - | 1295 | ` * Return a pointer to the freshly allocated hashmap on success.NULL otherwise.` |
|      - | 1296 | ` */` |
|  44062 | 1297 | `PH7_PRIVATE ph7_hashmap * PH7_NewHashmap(` |
|      - | 1298 | `	ph7_vm *pVm,              /* VM that trigger the hashmap creation */` |
|      - | 1299 | `	sxu32 (*xIntHash)(sxi64), /* Hash function for int keys.NULL otherwise*/` |
|      - | 1300 | `	sxu32 (*xBlobHash)(const void *,sxu32) /* Hash function for BLOB keys.NULL otherwise */` |
|      - | 1301 | `	)` |
|      2 | 1302 |  |
|      - | 1303 | `	ph7_hashmap *pMap;` |
|      - | 1304 | `	/* Allocate a new instance */` |
|  44064 | 1305 | `	pMap = (ph7_hashmap *)SyMemBackendPoolAlloc(&pVm->sAllocator,sizeof(ph7_hashmap));` |
|  44064 | 1306 | `	if( pMap == 0 ){` |
|    ! 0 | 1307 | `		return 0;` |
|      - | 1308 | `	}` |
|      - | 1309 | `	/* Zero the structure */` |
|  44064 | 1310 | `	SyZero(pMap,sizeof(ph7_hashmap));` |
|      - | 1311 | `	/* Fill in the structure */` |
|  44064 | 1312 | `	pMap->pVm = &(*pVm);` |
|  44064 | 1313 | `	pMap->iRef = 1;` |
|      - | 1314 | `	/* Default hash functions */` |
|  44064 | 1315 | `	pMap->xIntHash  = xIntHash ? xIntHash : IntHash;` |
|  44064 | 1316 | `	pMap->xBlobHash = xBlobHash ? xBlobHash : BinHash;` |
|  44064 | 1317 | `	return pMap;` |
|  22033 | 1318 |  |
|      - | 1319 | `/*` |
|      - | 1320 | ` * Install superglobals in the given virtual machine.` |
|      - | 1321 | ` * Note on superglobals.` |
|      - | 1322 | ` *  According to the PHP language reference manual.` |
|      - | 1323 | ` *  Superglobals are built-in variables that are always available in all scopes.` |
|      - | 1324 | `*   Description` |
|      - | 1325 | `*   Several predefined variables in PHP are "superglobals", which means they` |
|      - | 1326 | `*   are available in all scopes throughout a script. There is no need to do` |
|      - | 1327 | `*   global $variable; to access them within functions or methods.` |
|      - | 1328 | `*   These superglobal variables are:` |
|      - | 1329 | `*    $GLOBALS` |
|      - | 1330 | `*    $_SERVER` |
|      - | 1331 | `*    $_GET` |
|      - | 1332 | `*    $_POST` |
|      - | 1333 | `*    $_FILES` |
|      - | 1334 | `*    $_COOKIE` |
|      - | 1335 | `*    $_SESSION` |
|      - | 1336 | `*    $_REQUEST` |
|      - | 1337 | `*    $_ENV` |
|      - | 1338 | `*/` |
|   1098 | 1339 | `PH7_PRIVATE sxi32 PH7_HashmapCreateSuper(ph7_vm *pVm)` |
|      2 | 1340 |  |
|      - | 1341 | `	static const char * azSuper[] = {` |
|      - | 1342 | `		"_SERVER",   /* $_SERVER */` |
|      - | 1343 | `		"_GET",      /* $_GET */` |
|      - | 1344 | `		"_POST",     /* $_POST */` |
|      - | 1345 | `		"_FILES",    /* $_FILES */` |
|      - | 1346 | `		"_COOKIE",   /* $_COOKIE */` |
|      - | 1347 | `		"_SESSION",  /* $_SESSION */` |
|      - | 1348 | `		"_REQUEST",  /* $_REQUEST */` |
|      - | 1349 | `		"_ENV",      /* $_ENV */` |
|      - | 1350 | `		"_HEADER",   /* $_HEADER */` |
|      - | 1351 | `		"argv"       /* $argv */` |
|      - | 1352 | `	};` |
|      - | 1353 | `	ph7_hashmap *pMap;` |
|      - | 1354 | `	ph7_value *pObj;` |
|      - | 1355 | `	SyString *pFile;` |
|      - | 1356 | `	sxi32 rc;` |
|      - | 1357 | `	sxu32 n;` |
|      - | 1358 | `	/* Allocate a new hashmap for the $GLOBALS array */` |
|   1100 | 1359 | `	pMap = PH7_NewHashmap(&(*pVm),0,0);` |
|   1100 | 1360 | `	if( pMap == 0 ){` |
|    ! 0 | 1361 | `		return SXERR_MEM;` |
|      - | 1362 | `	}` |
|   1100 | 1363 | `	pVm->pGlobal = pMap;` |
|      - | 1364 | `	/* Reserve a ph7_value for the $GLOBALS array*/` |
|   1100 | 1365 | `	pObj = PH7_ReserveMemObj(&(*pVm));` |
|   1100 | 1366 | `	if( pObj == 0 ){` |
|    ! 0 | 1367 | `		return SXERR_MEM;` |
|      - | 1368 | `	}` |
|   1100 | 1369 | `	PH7_MemObjInitFromArray(&(*pVm),pObj,pMap);` |
|      - | 1370 | `	/* Record object index */` |
|   1100 | 1371 | `	pVm->nGlobalIdx = pObj->nIdx;` |
|      - | 1372 | `	/* Install the special $GLOBALS array */` |
|   1100 | 1373 | `	rc = SyHashInsert(&pVm->hSuper,(const void *)"GLOBALS",sizeof("GLOBALS")-1,SX_INT_TO_PTR(pVm->nGlobalIdx));` |
|   1100 | 1374 | `	if( rc != SXRET_OK ){` |
|    ! 0 | 1375 | `		return rc;` |
|      - | 1376 | `	}` |
|      - | 1377 | `	/* Install superglobals now */` |
|  12080 | 1378 | `	for( n =  0 ; n < SX_ARRAYSIZE(azSuper)  ; n++ ){` |
|      - | 1379 | `		ph7_value *pSuper;` |
|      - | 1380 | `		/* Request an empty array */` |
|  10982 | 1381 | `		pSuper = ph7_new_array(&(*pVm));` |
|  10982 | 1382 | `		if( pSuper == 0 ){` |
|    ! 0 | 1383 | `			return SXERR_MEM;` |
|      - | 1384 | `		}` |
|      - | 1385 | `		/* Install */` |
|  10982 | 1386 | `		rc = ph7_vm_config(&(*pVm),PH7_VM_CONFIG_CREATE_SUPER,azSuper[n]/* Super-global name*/,pSuper/* Super-global value */);` |
|  10982 | 1387 | `		if( rc != SXRET_OK ){` |
|    ! 0 | 1388 | `			return rc;` |
|      - | 1389 | `		}` |
|      - | 1390 | `		/* Release the value now it have been installed */` |
|  10982 | 1391 | `		ph7_release_value(&(*pVm),pSuper);` |
|   5492 | 1392 | `	}` |
|      - | 1393 | `	/* Set some $_SERVER entries */` |
|   1100 | 1394 | `	pFile = (SyString *)SySetPeek(&pVm->aFiles);` |
|      - | 1395 | `	/*` |
|      - | 1396 | `	 * 'SCRIPT_FILENAME'` |
|      - | 1397 | `	 * The absolute pathname of the currently executing script.` |
|      - | 1398 | `	 */` |
|   2194 | 1399 | `	ph7_vm_config(pVm,PH7_VM_CONFIG_SERVER_ATTR,` |
|      - | 1400 | `		"SCRIPT_FILENAME",` |
|    549 | 1401 | `		pFile ? pFile->zString : ":Memory:",` |
|   1094 | 1402 | `		pFile ? pFile->nByte : sizeof(":Memory:") - 1` |
|      - | 1403 | `		);` |
|      - | 1404 | `	/* All done,all super-global are installed now */` |
|   1100 | 1405 | `	return SXRET_OK;` |
|    551 | 1406 |  |
|      - | 1407 | `/*` |
|      - | 1408 | ` * Release a hashmap.` |
|      - | 1409 | ` */` |
|  31934 | 1410 | `PH7_PRIVATE sxi32 PH7_HashmapRelease(ph7_hashmap *pMap,int FreeDS)` |
|      2 | 1411 |  |
|      - | 1412 | `	ph7_hashmap_node *pEntry,*pNext;` |
|  31936 | 1413 | `	ph7_vm *pVm = pMap->pVm;` |
|      - | 1414 | `	sxu32 n;` |
|  31936 | 1415 | `	if( pMap == pVm->pGlobal ){` |
|      - | 1416 | `		/* Cannot delete the $GLOBALS array */` |
|    ! 0 | 1417 | `		PH7_VmThrowError(pMap->pVm,0,PH7_CTX_NOTICE,"$GLOBALS is a read-only array,deletion is forbidden");` |
|    ! 0 | 1418 | `		return SXRET_OK;` |
|      - | 1419 | `	}` |
|      - | 1420 | `	/* Start the release process */` |
|  31936 | 1421 | `	n = 0;` |
|  31936 | 1422 | `	pEntry = pMap->pFirst;` |
| 296911 | 1423 | `	for(;;){` |
| 593824 | 1424 | `		if( n >= pMap->nEntry ){` |
|  31936 | 1425 | `			break;` |
|      - | 1426 | `		}` |
| 561890 | 1427 | `		pNext = pEntry->pPrev; /* Reverse link */` |
|      - | 1428 | `		/* Remove the reference from the foreign table */` |
| 561890 | 1429 | `		PH7_VmRefObjRemove(pVm,pEntry->nValIdx,0,pEntry);` |
| 561890 | 1430 | `		if( (pEntry->iFlags & HASHMAP_NODE_FOREIGN_OBJ) == 0 ){` |
|      - | 1431 | `			/* Restore the ph7_value to the free list */` |
| 561882 | 1432 | `			PH7_VmUnsetMemObj(pVm,pEntry->nValIdx,FALSE);` |
| 280940 | 1433 | `		}` |
|      - | 1434 | `		/* Release the node */` |
| 561890 | 1435 | `		if( pEntry->iType == HASHMAP_BLOB_NODE ){` |
|  52588 | 1436 | `			SyBlobRelease(&pEntry->xKey.sKey);` |
|  26293 | 1437 | `		}` |
| 561890 | 1438 | `		SyMemBackendPoolFree(&pVm->sAllocator,pEntry);` |
|      - | 1439 | `		/* Point to the next entry */` |
| 561890 | 1440 | `		pEntry = pNext;` |
| 561890 | 1441 | `		n++;` |
|      2 | 1442 | `	}` |
|  31936 | 1443 | `	if( pMap->nEntry > 0 ){` |
|      - | 1444 | `		/* Release the hash bucket */` |
|  28514 | 1445 | `		SyMemBackendFree(&pVm->sAllocator,pMap->apBucket);` |
|  14256 | 1446 | `	}` |
|  31936 | 1447 | `	if( FreeDS ){` |
|      - | 1448 | `		/* Free the whole instance */` |
|  31934 | 1449 | `		SyMemBackendPoolFree(&pVm->sAllocator,pMap);` |
|  15968 | 1450 | `	}else{` |
|      - | 1451 | `		/* Keep the instance but reset it's fields */` |
|      3 | 1452 | `		pMap->apBucket = 0;` |
|      3 | 1453 | `		pMap->iNextIdx = 0;` |
|      3 | 1454 | `		pMap->nEntry = pMap->nSize = 0;` |
|      3 | 1455 | `		pMap->pFirst = pMap->pLast = pMap->pCur = 0;` |
|      - | 1456 | `	}` |
|  31936 | 1457 | `	return SXRET_OK;` |
|  15969 | 1458 |  |
|      - | 1459 | `/*` |
|      - | 1460 | ` * Decrement the reference count of a given hashmap.` |
|      - | 1461 | ` * If the count reaches zero which mean no more variables` |
|      - | 1462 | ` * are pointing to this hashmap,then release the whole instance.` |
|      - | 1463 | ` */` |
| 383306 | 1464 | `PH7_PRIVATE void  PH7_HashmapUnref(ph7_hashmap *pMap)` |
|      2 | 1465 |  |
| 383308 | 1466 | `	ph7_vm *pVm = pMap->pVm;` |
|      - | 1467 | `	/* TICKET 1432-49: $GLOBALS is not subject to garbage collection */` |
| 383308 | 1468 | `	pMap->iRef--;` |
| 383308 | 1469 | `	if( pMap->iRef < 1 && pMap != pVm->pGlobal){` |
|  31934 | 1470 | `		PH7_HashmapRelease(pMap,TRUE);` |
|  15966 | 1471 | `	}` |
| 383308 | 1472 |  |
|      - | 1473 | `/*` |
|      - | 1474 | ` * Check if a given key exists in the given hashmap.` |
|      - | 1475 | ` * Write a pointer to the target node on success.` |
|      - | 1476 | ` * Otherwise SXERR_NOTFOUND is returned on failure.` |
|      - | 1477 | ` */` |
|  67332 | 1478 | `PH7_PRIVATE sxi32 PH7_HashmapLookup(` |
|      - | 1479 | `	ph7_hashmap *pMap,        /* Target hashmap */` |
|      - | 1480 | `	ph7_value *pKey,          /* Lookup key */` |
|      - | 1481 | `	ph7_hashmap_node **ppNode /* OUT: Target node on success */` |
|      - | 1482 | `	)` |
|      2 | 1483 |  |
|      - | 1484 | `	sxi32 rc;` |
|  67334 | 1485 | `	if( pMap->nEntry < 1 ){` |
|      - | 1486 | `		/* TICKET 1433-25: Don't bother hashing,the hashmap is empty anyway.` |
|      - | 1487 | `		 */` |
|      7 | 1488 | `		return SXERR_NOTFOUND;` |
|      - | 1489 | `	}` |
|  67328 | 1490 | `	rc = HashmapLookup(&(*pMap),&(*pKey),ppNode);` |
|  67328 | 1491 | `	return rc;` |
|  33668 | 1492 |  |
|      - | 1493 | `/*` |
|      - | 1494 | ` * Insert a given key and it's associated value (if any) in the given` |
|      - | 1495 | ` * hashmap.` |
|      - | 1496 | ` * If a node with the given key already exists in the database` |
|      - | 1497 | ` * then this function overwrite the old value.` |
|      - | 1498 | ` */` |
| 268674 | 1499 | `PH7_PRIVATE sxi32 PH7_HashmapInsert(` |
|      - | 1500 | `	ph7_hashmap *pMap, /* Target hashmap */` |
|      - | 1501 | `	ph7_value *pKey,   /* Lookup key */` |
|      - | 1502 | `	ph7_value *pVal    /* Node value.NULL otherwise */` |
|      - | 1503 | `	)` |
|      2 | 1504 |  |
|      - | 1505 | `	sxi32 rc;` |
| 268676 | 1506 | `	if( pVal && (pVal->iFlags & MEMOBJ_HASHMAP) && (ph7_hashmap *)pVal->x.pOther == pMap->pVm->pGlobal ){` |
|      - | 1507 | `		/*` |
|      - | 1508 | `		 * TICKET 1433-35: Insertion in the $GLOBALS array is forbidden.` |
|      - | 1509 | `		 */` |
|    ! 0 | 1510 | `		PH7_VmThrowError(pMap->pVm,0,PH7_CTX_ERR,"$GLOBALS is a read-only array,insertion is forbidden");` |
|    ! 0 | 1511 | `		return SXRET_OK;` |
|      - | 1512 | `	}` |
| 268676 | 1513 | `	rc = HashmapInsert(&(*pMap),&(*pKey),&(*pVal));` |
| 268676 | 1514 | `	return rc;` |
| 134339 | 1515 |  |
|      - | 1516 | `/*` |
|      - | 1517 | ` * Insert a given key and it's associated value (foreign index) in the given` |
|      - | 1518 | ` * hashmap.` |
|      - | 1519 | ` * This is insertion by reference so be careful to mark the node` |
|      - | 1520 | ` * with the HASHMAP_NODE_FOREIGN_OBJ flag being set.` |
|      - | 1521 | ` * The insertion by reference is triggered when the following` |
|      - | 1522 | ` * expression is encountered.` |
|      - | 1523 | ` * $var = 10;` |
|      - | 1524 | ` *  $a = array(&var);` |
|      - | 1525 | ` * OR` |
|      - | 1526 | ` *  $a[] =& $var;` |
|      - | 1527 | ` * That is,$var is a foreign ph7_value and the $a array have no control` |
|      - | 1528 | ` * over it's contents.` |
|      - | 1529 | ` * Note that the node that hold the foreign ph7_value is automatically` |
|      - | 1530 | ` * removed when the foreign ph7_value is unset.` |
|      - | 1531 | ` * Example:` |
|      - | 1532 | ` *  $var = 10;` |
|      - | 1533 | ` *  $a[] =& $var;` |
|      - | 1534 | ` *  echo count($a).PHP_EOL; //1` |
|      - | 1535 | ` *  //Unset the foreign ph7_value now` |
|      - | 1536 | ` *  unset($var);` |
|      - | 1537 | ` *  echo count($a); //0` |
|      - | 1538 | ` * Note that this is a PH7 eXtension.` |
|      - | 1539 | ` * Refer to the official documentation for more information.` |
|      - | 1540 | ` * If a node with the given key already exists in the database` |
|      - | 1541 | ` * then this function overwrite the old value.` |
|      - | 1542 | ` */` |
|  15746 | 1543 | `PH7_PRIVATE sxi32 PH7_HashmapInsertByRef(` |
|      - | 1544 | `	ph7_hashmap *pMap, /* Target hashmap */` |
|      - | 1545 | `	ph7_value *pKey,   /* Lookup key */` |
|      - | 1546 | `	sxu32 nRefIdx      /* Foreign ph7_value index */` |
|      - | 1547 | `	)` |
|      2 | 1548 |  |
|      - | 1549 | `	sxi32 rc;` |
|  15748 | 1550 | `	if( nRefIdx == pMap->pVm->nGlobalIdx ){` |
|      - | 1551 | `		/*` |
|      - | 1552 | `		 * TICKET 1433-35: Insertion in the $GLOBALS array is forbidden.` |
|      - | 1553 | `		 */` |
|    ! 0 | 1554 | `		PH7_VmThrowError(pMap->pVm,0,PH7_CTX_ERR,"$GLOBALS is a read-only array,insertion is forbidden");` |
|    ! 0 | 1555 | `		return SXRET_OK;` |
|      - | 1556 | `	}` |
|  15748 | 1557 | `	rc = HashmapInsertByRef(&(*pMap),&(*pKey),nRefIdx);` |
|  15748 | 1558 | `	return rc;` |
|   7875 | 1559 |  |
|      - | 1560 | `/*` |
|      - | 1561 | ` * Reset the node cursor of a given hashmap.` |
|      - | 1562 | ` */` |
|  14348 | 1563 | `PH7_PRIVATE void PH7_HashmapResetLoopCursor(ph7_hashmap *pMap)` |
|      2 | 1564 |  |
|      - | 1565 | `	/* Reset the loop cursor */` |
|  14350 | 1566 | `	pMap->pCur = pMap->pFirst;` |
|  14350 | 1567 |  |
|      - | 1568 | `/*` |
|      - | 1569 | ` * Return a pointer to the node currently pointed by the node cursor.` |
|      - | 1570 | ` * If the cursor reaches the end of the list,then this function` |
|      - | 1571 | ` * return NULL.` |
|      - | 1572 | ` * Note that the node cursor is automatically advanced by this function.` |
|      - | 1573 | ` */` |
| 120252 | 1574 | `PH7_PRIVATE ph7_hashmap_node * PH7_HashmapGetNextEntry(ph7_hashmap *pMap)` |
|      2 | 1575 |  |
| 120254 | 1576 | `	ph7_hashmap_node *pCur = pMap->pCur;` |
| 120254 | 1577 | `	if( pCur == 0 ){` |
|      - | 1578 | `		/* End of the list,return null */` |
|   7178 | 1579 | `		return 0;` |
|      - | 1580 | `	}` |
|      - | 1581 | `	/* Advance the node cursor */` |
| 113078 | 1582 | `	pMap->pCur = pCur->pPrev; /* Reverse link */` |
| 113078 | 1583 | `	return pCur;` |
|  60128 | 1584 |  |
|      - | 1585 | `/*` |
|      - | 1586 | ` * Extract a node value.` |
|      - | 1587 | ` */` |
| 289678 | 1588 | `PH7_PRIVATE void PH7_HashmapExtractNodeValue(ph7_hashmap_node *pNode,ph7_value *pValue,int bStore)` |
|      2 | 1589 |  |
| 289680 | 1590 | `	ph7_value *pEntry = HashmapExtractNodeValue(pNode);` |
| 289680 | 1591 | `	if( pEntry ){` |
| 289680 | 1592 | `		if( bStore ){` |
| 113126 | 1593 | `			PH7_MemObjStore(pEntry,pValue);` |
|  56564 | 1594 | `		}else{` |
| 176556 | 1595 | `			PH7_MemObjLoad(pEntry,pValue);` |
|      - | 1596 | `		}` |
| 144875 | 1597 | `	}else{` |
|    ! 0 | 1598 | `		PH7_MemObjRelease(pValue);` |
|      - | 1599 | `	}` |
| 289680 | 1600 |  |
|      - | 1601 | `/*` |
|      - | 1602 | ` * Extract a node key.` |
|      - | 1603 | ` */` |
|  80474 | 1604 | `PH7_PRIVATE void PH7_HashmapExtractNodeKey(ph7_hashmap_node *pNode,ph7_value *pKey)` |
|      2 | 1605 |  |
|      - | 1606 | `	/* Fill with the current key */` |
|  80476 | 1607 | `	if( pNode->iType == HASHMAP_INT_NODE ){` |
|  80342 | 1608 | `		if( SyBlobLength(&pKey->sBlob) > 0 ){` |
|      3 | 1609 | `			SyBlobRelease(&pKey->sBlob);` |
|      1 | 1610 | `		}` |
|  80342 | 1611 | `		pKey->x.iVal = pNode->xKey.iKey;` |
|  80342 | 1612 | `		MemObjSetType(pKey,MEMOBJ_INT);` |
|  40172 | 1613 | `	}else{` |
|    135 | 1614 | `		SyBlobReset(&pKey->sBlob);` |
|    135 | 1615 | `		SyBlobAppend(&pKey->sBlob,SyBlobData(&pNode->xKey.sKey),SyBlobLength(&pNode->xKey.sKey));` |
|    135 | 1616 | `		MemObjSetType(pKey,MEMOBJ_STRING);` |
|      - | 1617 | `	}` |
|  80476 | 1618 |  |
|      - | 1619 | `#ifndef PH7_DISABLE_DISK_IO` |
|      - | 1620 | `/*` |
|      - | 1621 | ` * Store the address of nodes value in the given container.` |
|      - | 1622 | ` * Refer to the [vfprintf(),vprintf(),vsprintf()] implementations` |
|      - | 1623 | ` * defined in 'builtin.c' for more information.` |
|      - | 1624 | ` */` |
|     10 | 1625 | `PH7_PRIVATE int PH7_HashmapValuesToSet(ph7_hashmap *pMap,SySet *pOut)` |
|      1 | 1626 |  |
|     11 | 1627 | `	ph7_hashmap_node *pEntry = pMap->pFirst;` |
|      - | 1628 | `	ph7_value *pValue;` |
|      - | 1629 | `	sxu32 n;` |
|      - | 1630 | `	/* Initialize the container */` |
|     11 | 1631 | `	SySetInit(pOut,&pMap->pVm->sAllocator,sizeof(ph7_value *));` |
|     27 | 1632 | `	for(n = 0 ; n < pMap->nEntry ; n++ ){` |
|      - | 1633 | `		/* Extract node value */` |
|     17 | 1634 | `		pValue = HashmapExtractNodeValue(pEntry);` |
|     17 | 1635 | `		if( pValue ){` |
|     17 | 1636 | `			SySetPut(pOut,(const void *)&pValue);` |
|      8 | 1637 | `		}` |
|      - | 1638 | `		/* Point to the next entry */` |
|     17 | 1639 | `		pEntry = pEntry->pPrev; /* Reverse link */` |
|      9 | 1640 | `	}` |
|      - | 1641 | `	/* Total inserted entries */` |
|     11 | 1642 | `	return (int)SySetUsed(pOut);` |
|      1 | 1643 |  |
|      - | 1644 | `#endif /* PH7_DISABLE_BUILTIN_FUNC \|\| PH7_DISABLE_DISK_IO */` |
|      - | 1645 | `/*` |
|      - | 1646 | ` * Merge sort.` |
|      - | 1647 | ` * The merge sort implementation is based on the one found in the SQLite3 source tree.` |
|      - | 1648 | ` * Status: Public domain` |
|      - | 1649 | ` */` |
|      - | 1650 | `/* Node comparison callback signature */` |
|      - | 1651 | `typedef sxi32 (*ProcNodeCmp)(ph7_hashmap_node *,ph7_hashmap_node *,void *);` |
|      - | 1652 | `/*` |
|      - | 1653 | `** Inputs:` |
|      - | 1654 | `**   a:       A sorted, null-terminated linked list.  (May be null).` |
|      - | 1655 | `**   b:       A sorted, null-terminated linked list.  (May be null).` |
|      - | 1656 | `**   cmp:     A pointer to the comparison function.` |
|      - | 1657 | `**` |
|      - | 1658 | `** Return Value:` |
|      - | 1659 | `**   A pointer to the head of a sorted list containing the elements` |
|      - | 1660 | `**   of both a and b.` |
|      - | 1661 | `**` |
|      - | 1662 | `** Side effects:` |
|      - | 1663 | `**   The "next","prev" pointers for elements in the lists a and b are` |
|      - | 1664 | `**   changed.` |
|      - | 1665 | `*/` |
|  20248 | 1666 | `static ph7_hashmap_node * HashmapNodeMerge(ph7_hashmap_node *pA,ph7_hashmap_node *pB,ProcNodeCmp xCmp,void *pCmpData)` |
|      2 | 1667 |  |
|      - | 1668 | `	ph7_hashmap_node result,*pTail;` |
|      - | 1669 | `    /* Prevent compiler warning */` |
|  20250 | 1670 | `	result.pNext = result.pPrev = 0;` |
|  20250 | 1671 | `	pTail = &result;` |
|  52321 | 1672 | `	while( pA && pB ){` |
|  32073 | 1673 | `		if( xCmp(pA,pB,pCmpData) < 0 ){` |
|  20913 | 1674 | `			pTail->pPrev = pA;` |
|  20913 | 1675 | `			pA->pNext = pTail;` |
|  20913 | 1676 | `			pTail = pA;` |
|  20913 | 1677 | `			pA = pA->pPrev;` |
|  10459 | 1678 | `		}else{` |
|  11162 | 1679 | `			pTail->pPrev = pB;` |
|  11162 | 1680 | `			pB->pNext = pTail;` |
|  11162 | 1681 | `			pTail = pB;` |
|  11162 | 1682 | `			pB = pB->pPrev;` |
|      - | 1683 | `		}` |
|      2 | 1684 | `	}` |
|  20250 | 1685 | `	if( pA ){` |
|  15052 | 1686 | `		pTail->pPrev = pA;` |
|  15052 | 1687 | `		pA->pNext = pTail;` |
|  12733 | 1688 | `	}else if( pB ){` |
|   5090 | 1689 | `		pTail->pPrev = pB;` |
|   5090 | 1690 | `		pB->pNext = pTail;` |
|   2538 | 1691 | `	}else{` |
|    112 | 1692 | `		pTail->pPrev = pTail->pNext = 0;` |
|      - | 1693 | `	}` |
|  20250 | 1694 | `	return result.pPrev;` |
|      2 | 1695 |  |
|      - | 1696 | `/*` |
|      - | 1697 | `** Inputs:` |
|      - | 1698 | `**   Map:       Input hashmap` |
|      - | 1699 | `**   cmp:       A comparison function.` |
|      - | 1700 | `**` |
|      - | 1701 | `** Return Value:` |
|      - | 1702 | `**   Sorted hashmap.` |
|      - | 1703 | `**` |
|      - | 1704 | `** Side effects:` |
|      - | 1705 | `**   The "next" pointers for elements in list are changed.` |
|      - | 1706 | `*/` |
|      - | 1707 | `#define N_SORT_BUCKET  32` |
|    456 | 1708 | `static sxi32 HashmapMergeSort(ph7_hashmap *pMap,ProcNodeCmp xCmp,void *pCmpData)` |
|      2 | 1709 |  |
|      - | 1710 | `	ph7_hashmap_node *a[N_SORT_BUCKET], *p,*pIn;` |
|      - | 1711 | `	sxu32 i;` |
|    458 | 1712 | `	SyZero(a,sizeof(a));` |
|      - | 1713 | `	/* Point to the first inserted entry */` |
|    458 | 1714 | `	pIn = pMap->pFirst;` |
|   7338 | 1715 | `	while( pIn ){` |
|   6882 | 1716 | `		p = pIn;` |
|   6882 | 1717 | `		pIn = p->pPrev;` |
|   6882 | 1718 | `		p->pPrev = 0;` |
|  12994 | 1719 | `		for(i=0; i<N_SORT_BUCKET-1; i++){` |
|  12994 | 1720 | `			if( a[i]==0 ){` |
|   6882 | 1721 | `				a[i] = p;` |
|   6882 | 1722 | `				break;` |
|    ! 0 | 1723 | `			}else{` |
|   6114 | 1724 | `				p = HashmapNodeMerge(a[i],p,xCmp,pCmpData);` |
|   6114 | 1725 | `				a[i] = 0;` |
|      - | 1726 | `			}` |
|   3058 | 1727 | `		}` |
|   6882 | 1728 | `		if( i==N_SORT_BUCKET-1 ){` |
|      - | 1729 | `			/* To get here, there need to be 2^(N_SORT_BUCKET) elements in he input list.` |
|      - | 1730 | `			 * But that is impossible.` |
|      - | 1731 | `			 */` |
|    ! 0 | 1732 | `			a[i] = HashmapNodeMerge(a[i], p,xCmp,pCmpData);` |
|    ! 0 | 1733 | `		}` |
|      2 | 1734 | `	}` |
|    458 | 1735 | `	p = a[0];` |
|  14594 | 1736 | `	for(i=1; i<N_SORT_BUCKET; i++){` |
|  14138 | 1737 | `		p = HashmapNodeMerge(p,a[i],xCmp,pCmpData);` |
|   7070 | 1738 | `	}` |
|    458 | 1739 | `	p->pNext = 0;` |
|      - | 1740 | `	/* Reflect the change */` |
|    458 | 1741 | `	pMap->pFirst = p;` |
|      - | 1742 | `	/* Reset the loop cursor */` |
|    458 | 1743 | `	pMap->pCur = pMap->pFirst;` |
|    458 | 1744 | `	return SXRET_OK;` |
|      2 | 1745 |  |
|      - | 1746 | `/*` |
|      - | 1747 | ` * Node comparison callback.` |
|      - | 1748 | ` * used-by: [sort(),asort(),...]` |
|      - | 1749 | ` */` |
|  32008 | 1750 | `static sxi32 HashmapCmpCallback1(ph7_hashmap_node *pA,ph7_hashmap_node *pB,void *pCmpData)` |
|      2 | 1751 |  |
|      - | 1752 | `	ph7_value sA,sB;` |
|      - | 1753 | `	sxi32 iFlags;` |
|      - | 1754 | `	int rc;` |
|  32010 | 1755 | `	if( pCmpData == 0 ){` |
|      - | 1756 | `		/* Perform a standard comparison */` |
|  32006 | 1757 | `		rc = HashmapNodeCmp(pA,pB,FALSE);` |
|  32006 | 1758 | `		return rc;` |
|      - | 1759 | `	}` |
|      5 | 1760 | `	iFlags = SX_PTR_TO_INT(pCmpData);` |
|      - | 1761 | `	/* Duplicate node values */` |
|      5 | 1762 | `	PH7_MemObjInit(pA->pMap->pVm,&sA);` |
|      5 | 1763 | `	PH7_MemObjInit(pA->pMap->pVm,&sB);` |
|      5 | 1764 | `	PH7_HashmapExtractNodeValue(pA,&sA,FALSE);` |
|      5 | 1765 | `	PH7_HashmapExtractNodeValue(pB,&sB,FALSE);` |
|      5 | 1766 | `	if( iFlags == 5 ){` |
|      - | 1767 | `		/* String cast */` |
|      5 | 1768 | `		if( (sA.iFlags & MEMOBJ_STRING) == 0 ){` |
|    ! 0 | 1769 | `			PH7_MemObjToString(&sA);` |
|    ! 0 | 1770 | `		}` |
|      5 | 1771 | `		if( (sB.iFlags & MEMOBJ_STRING) == 0 ){` |
|    ! 0 | 1772 | `			PH7_MemObjToString(&sB);` |
|    ! 0 | 1773 | `		}` |
|      3 | 1774 | `	}else{` |
|      - | 1775 | `		/* Numeric cast */` |
|    ! 0 | 1776 | `		PH7_MemObjToNumeric(&sA);` |
|    ! 0 | 1777 | `		PH7_MemObjToNumeric(&sB);` |
|      - | 1778 | `	}` |
|      - | 1779 | `	/* Perform the comparison */` |
|      5 | 1780 | `	rc = PH7_MemObjCmp(&sA,&sB,FALSE,0);` |
|      5 | 1781 | `	PH7_MemObjRelease(&sA);` |
|      5 | 1782 | `	PH7_MemObjRelease(&sB);` |
|      5 | 1783 | `	return rc;` |
|  16023 | 1784 |  |
|      - | 1785 | `/*` |
|      - | 1786 | ` * Node comparison callback: Compare nodes by keys only.` |
|      - | 1787 | ` * used-by: [ksort()]` |
|      - | 1788 | ` */` |
|     14 | 1789 | `static sxi32 HashmapCmpCallback2(ph7_hashmap_node *pA,ph7_hashmap_node *pB,void *pCmpData)` |
|      1 | 1790 |  |
|      - | 1791 | `	sxi32 rc;` |
|      7 | 1792 | `	SXUNUSED(pCmpData); /* cc warning */` |
|     15 | 1793 | `	if( pA->iType == HASHMAP_BLOB_NODE && pB->iType == HASHMAP_BLOB_NODE ){` |
|      - | 1794 | `		/* Perform a string comparison */` |
|      5 | 1795 | `		rc = SyBlobCmp(&pA->xKey.sKey,&pB->xKey.sKey);` |
|      3 | 1796 | `	}else{` |
|      - | 1797 | `		SyString sStr;` |
|      - | 1798 | `		sxi64 iA,iB;` |
|      - | 1799 | `		/* Perform a numeric comparison */` |
|     11 | 1800 | `		if( pA->iType == HASHMAP_BLOB_NODE ){` |
|      - | 1801 | `			/* Cast to 64-bit integer */` |
|    ! 0 | 1802 | `			SyStringInitFromBuf(&sStr,SyBlobData(&pA->xKey.sKey),SyBlobLength(&pA->xKey.sKey));` |
|    ! 0 | 1803 | `			if( sStr.nByte < 1 ){` |
|    ! 0 | 1804 | `				iA = 0;` |
|    ! 0 | 1805 | `			}else{` |
|    ! 0 | 1806 | `				SyStrToInt64(sStr.zString,sStr.nByte,(void *)&iA,0);` |
|      - | 1807 | `			}` |
|    ! 0 | 1808 | `		}else{` |
|     11 | 1809 | `			iA = pA->xKey.iKey;` |
|      - | 1810 | `		}` |
|     11 | 1811 | `		if( pB->iType == HASHMAP_BLOB_NODE ){` |
|      - | 1812 | `			/* Cast to 64-bit integer */` |
|    ! 0 | 1813 | `			SyStringInitFromBuf(&sStr,SyBlobData(&pB->xKey.sKey),SyBlobLength(&pB->xKey.sKey));` |
|    ! 0 | 1814 | `			if( sStr.nByte < 1 ){` |
|    ! 0 | 1815 | `				iB = 0;` |
|    ! 0 | 1816 | `			}else{` |
|    ! 0 | 1817 | `				SyStrToInt64(sStr.zString,sStr.nByte,(void *)&iB,0);` |
|      - | 1818 | `			}` |
|    ! 0 | 1819 | `		}else{` |
|     11 | 1820 | `			iB = pB->xKey.iKey;` |
|      - | 1821 | `		}` |
|     11 | 1822 | `		rc = (sxi32)(iA-iB);` |
|      - | 1823 | `	}` |
|      - | 1824 | `	/* Comparison result */` |
|     15 | 1825 | `	return rc;` |
|      1 | 1826 |  |
|      - | 1827 | `/*` |
|      - | 1828 | ` * Node comparison callback.` |
|      - | 1829 | ` * Used by: [rsort(),arsort()];` |
|      - | 1830 | ` */` |
|     12 | 1831 | `static sxi32 HashmapCmpCallback3(ph7_hashmap_node *pA,ph7_hashmap_node *pB,void *pCmpData)` |
|      1 | 1832 |  |
|      - | 1833 | `	ph7_value sA,sB;` |
|      - | 1834 | `	sxi32 iFlags;` |
|      - | 1835 | `	int rc;` |
|     13 | 1836 | `	if( pCmpData == 0 ){` |
|      - | 1837 | `		/* Perform a standard comparison */` |
|     13 | 1838 | `		rc = HashmapNodeCmp(pA,pB,FALSE);` |
|     13 | 1839 | `		return -rc;` |
|      - | 1840 | `	}` |
|    ! 0 | 1841 | `	iFlags = SX_PTR_TO_INT(pCmpData);` |
|      - | 1842 | `	/* Duplicate node values */` |
|    ! 0 | 1843 | `	PH7_MemObjInit(pA->pMap->pVm,&sA);` |
|    ! 0 | 1844 | `	PH7_MemObjInit(pA->pMap->pVm,&sB);` |
|    ! 0 | 1845 | `	PH7_HashmapExtractNodeValue(pA,&sA,FALSE);` |
|    ! 0 | 1846 | `	PH7_HashmapExtractNodeValue(pB,&sB,FALSE);` |
|    ! 0 | 1847 | `	if( iFlags == 5 ){` |
|      - | 1848 | `		/* String cast */` |
|    ! 0 | 1849 | `		if( (sA.iFlags & MEMOBJ_STRING) == 0 ){` |
|    ! 0 | 1850 | `			PH7_MemObjToString(&sA);` |
|    ! 0 | 1851 | `		}` |
|    ! 0 | 1852 | `		if( (sB.iFlags & MEMOBJ_STRING) == 0 ){` |
|    ! 0 | 1853 | `			PH7_MemObjToString(&sB);` |
|    ! 0 | 1854 | `		}` |
|    ! 0 | 1855 | `	}else{` |
|      - | 1856 | `		/* Numeric cast */` |
|    ! 0 | 1857 | `		PH7_MemObjToNumeric(&sA);` |
|    ! 0 | 1858 | `		PH7_MemObjToNumeric(&sB);` |
|      - | 1859 | `	}` |
|      - | 1860 | `	/* Perform the comparison */` |
|    ! 0 | 1861 | `	rc = PH7_MemObjCmp(&sA,&sB,FALSE,0);` |
|    ! 0 | 1862 | `	PH7_MemObjRelease(&sA);` |
|    ! 0 | 1863 | `	PH7_MemObjRelease(&sB);` |
|    ! 0 | 1864 | `	return -rc;` |
|      7 | 1865 |  |
|      - | 1866 | `/*` |
|      - | 1867 | ` * Node comparison callback: Invoke an user-defined callback for the purpose of node comparison.` |
|      - | 1868 | ` * used-by: [usort(),uasort()]` |
|      - | 1869 | ` */` |
|     12 | 1870 | `static sxi32 HashmapCmpCallback4(ph7_hashmap_node *pA,ph7_hashmap_node *pB,void *pCmpData)` |
|      1 | 1871 |  |
|      - | 1872 | `	ph7_value sResult,*pCallback;` |
|      - | 1873 | `	ph7_value *pV1,*pV2;` |
|      - | 1874 | `	ph7_value *apArg[2];  /* Callback arguments */` |
|      - | 1875 | `	sxi32 rc;` |
|      - | 1876 | `	/* Point to the desired callback */` |
|     13 | 1877 | `	pCallback = (ph7_value *)pCmpData;` |
|      - | 1878 | `	/* initialize the result value */` |
|     13 | 1879 | `	PH7_MemObjInit(pA->pMap->pVm,&sResult);` |
|      - | 1880 | `	/* Extract nodes values */` |
|     13 | 1881 | `	pV1 = HashmapExtractNodeValue(pA);` |
|     13 | 1882 | `	pV2 = HashmapExtractNodeValue(pB);` |
|     13 | 1883 | `	apArg[0] = pV1;` |
|     13 | 1884 | `	apArg[1] = pV2;` |
|      - | 1885 | `	/* Invoke the callback */` |
|     13 | 1886 | `	rc = PH7_VmCallUserFunction(pA->pMap->pVm,pCallback,2,apArg,&sResult);` |
|     13 | 1887 | `	if( rc != SXRET_OK ){` |
|      - | 1888 | `		/* An error occured while calling user defined function [i.e: not defined] */` |
|    ! 0 | 1889 | `		rc = -1; /* Set a dummy result */` |
|    ! 0 | 1890 | `	}else{` |
|      - | 1891 | `		/* Extract callback result */` |
|     13 | 1892 | `		if((sResult.iFlags & MEMOBJ_INT) == 0 ){` |
|      - | 1893 | `			/* Perform an int cast */` |
|    ! 0 | 1894 | `			PH7_MemObjToInteger(&sResult);` |
|    ! 0 | 1895 | `		}` |
|     13 | 1896 | `		rc = (sxi32)sResult.x.iVal;` |
|      - | 1897 | `	}` |
|     13 | 1898 | `	PH7_MemObjRelease(&sResult);` |
|      - | 1899 | `	/* Callback result */` |
|     13 | 1900 | `	return rc;` |
|      1 | 1901 |  |
|      - | 1902 | `/*` |
|      - | 1903 | ` * Node comparison callback: Compare nodes by keys only.` |
|      - | 1904 | ` * used-by: [krsort()]` |
|      - | 1905 | ` */` |
|      4 | 1906 | `static sxi32 HashmapCmpCallback5(ph7_hashmap_node *pA,ph7_hashmap_node *pB,void *pCmpData)` |
|      1 | 1907 |  |
|      - | 1908 | `	sxi32 rc;` |
|      2 | 1909 | `	SXUNUSED(pCmpData); /* cc warning */` |
|      5 | 1910 | `	if( pA->iType == HASHMAP_BLOB_NODE && pB->iType == HASHMAP_BLOB_NODE ){` |
|      - | 1911 | `		/* Perform a string comparison */` |
|      5 | 1912 | `		rc = SyBlobCmp(&pA->xKey.sKey,&pB->xKey.sKey);` |
|      3 | 1913 | `	}else{` |
|      - | 1914 | `		SyString sStr;` |
|      - | 1915 | `		sxi64 iA,iB;` |
|      - | 1916 | `		/* Perform a numeric comparison */` |
|    ! 0 | 1917 | `		if( pA->iType == HASHMAP_BLOB_NODE ){` |
|      - | 1918 | `			/* Cast to 64-bit integer */` |
|    ! 0 | 1919 | `			SyStringInitFromBuf(&sStr,SyBlobData(&pA->xKey.sKey),SyBlobLength(&pA->xKey.sKey));` |
|    ! 0 | 1920 | `			if( sStr.nByte < 1 ){` |
|    ! 0 | 1921 | `				iA = 0;` |
|    ! 0 | 1922 | `			}else{` |
|    ! 0 | 1923 | `				SyStrToInt64(sStr.zString,sStr.nByte,(void *)&iA,0);` |
|      - | 1924 | `			}` |
|    ! 0 | 1925 | `		}else{` |
|    ! 0 | 1926 | `			iA = pA->xKey.iKey;` |
|      - | 1927 | `		}` |
|    ! 0 | 1928 | `		if( pB->iType == HASHMAP_BLOB_NODE ){` |
|      - | 1929 | `			/* Cast to 64-bit integer */` |
|    ! 0 | 1930 | `			SyStringInitFromBuf(&sStr,SyBlobData(&pB->xKey.sKey),SyBlobLength(&pB->xKey.sKey));` |
|    ! 0 | 1931 | `			if( sStr.nByte < 1 ){` |
|    ! 0 | 1932 | `				iB = 0;` |
|    ! 0 | 1933 | `			}else{` |
|    ! 0 | 1934 | `				SyStrToInt64(sStr.zString,sStr.nByte,(void *)&iB,0);` |
|      - | 1935 | `			}` |
|    ! 0 | 1936 | `		}else{` |
|    ! 0 | 1937 | `			iB = pB->xKey.iKey;` |
|      - | 1938 | `		}` |
|    ! 0 | 1939 | `		rc = (sxi32)(iA-iB);` |
|      - | 1940 | `	}` |
|      5 | 1941 | `	return -rc; /* Reverse result */` |
|      1 | 1942 |  |
|      - | 1943 | `/*` |
|      - | 1944 | ` * Node comparison callback: Invoke an user-defined callback for the purpose of node comparison.` |
|      - | 1945 | ` * used-by: [uksort()]` |
|      - | 1946 | ` */` |
|      6 | 1947 | `static sxi32 HashmapCmpCallback6(ph7_hashmap_node *pA,ph7_hashmap_node *pB,void *pCmpData)` |
|      1 | 1948 |  |
|      - | 1949 | `	ph7_value sResult,*pCallback;` |
|      - | 1950 | `	ph7_value *apArg[2];  /* Callback arguments */` |
|      - | 1951 | `	ph7_value sK1,sK2;` |
|      - | 1952 | `	sxi32 rc;` |
|      - | 1953 | `	/* Point to the desired callback */` |
|      7 | 1954 | `	pCallback = (ph7_value *)pCmpData;` |
|      - | 1955 | `	/* initialize the result value */` |
|      7 | 1956 | `	PH7_MemObjInit(pA->pMap->pVm,&sResult);` |
|      7 | 1957 | `	PH7_MemObjInit(pA->pMap->pVm,&sK1);` |
|      7 | 1958 | `	PH7_MemObjInit(pA->pMap->pVm,&sK2);` |
|      - | 1959 | `	/* Extract nodes keys */` |
|      7 | 1960 | `	PH7_HashmapExtractNodeKey(pA,&sK1);` |
|      7 | 1961 | `	PH7_HashmapExtractNodeKey(pB,&sK2);` |
|      7 | 1962 | `	apArg[0] = &sK1;` |
|      7 | 1963 | `	apArg[1] = &sK2;` |
|      - | 1964 | `	/* Mark keys as constants */` |
|      7 | 1965 | `	sK1.nIdx = SXU32_HIGH;` |
|      7 | 1966 | `	sK2.nIdx = SXU32_HIGH;` |
|      - | 1967 | `	/* Invoke the callback */` |
|      7 | 1968 | `	rc = PH7_VmCallUserFunction(pA->pMap->pVm,pCallback,2,apArg,&sResult);` |
|      7 | 1969 | `	if( rc != SXRET_OK ){` |
|      - | 1970 | `		/* An error occured while calling user defined function [i.e: not defined] */` |
|    ! 0 | 1971 | `		rc = -1; /* Set a dummy result */` |
|    ! 0 | 1972 | `	}else{` |
|      - | 1973 | `		/* Extract callback result */` |
|      7 | 1974 | `		if((sResult.iFlags & MEMOBJ_INT) == 0 ){` |
|      - | 1975 | `			/* Perform an int cast */` |
|    ! 0 | 1976 | `			PH7_MemObjToInteger(&sResult);` |
|    ! 0 | 1977 | `		}` |
|      7 | 1978 | `		rc = (sxi32)sResult.x.iVal;` |
|      - | 1979 | `	}` |
|      7 | 1980 | `	PH7_MemObjRelease(&sResult);` |
|      7 | 1981 | `	PH7_MemObjRelease(&sK1);` |
|      7 | 1982 | `	PH7_MemObjRelease(&sK2);` |
|      - | 1983 | `	/* Callback result */` |
|      7 | 1984 | `	return rc;` |
|      1 | 1985 |  |
|      - | 1986 | `/*` |
|      - | 1987 | ` * Node comparison callback: Random node comparison.` |
|      - | 1988 | ` * used-by: [shuffle()]` |
|      - | 1989 | ` */` |
|     15 | 1990 | `static sxi32 HashmapCmpCallback7(ph7_hashmap_node *pA,ph7_hashmap_node *pB,void *pCmpData)` |
|      1 | 1991 |  |
|      - | 1992 | `	sxu32 n;` |
|      7 | 1993 | `	SXUNUSED(pB); /* cc warning */` |
|      7 | 1994 | `	SXUNUSED(pCmpData);` |
|      - | 1995 | `	/* Grab a random number */` |
|     16 | 1996 | `	n = PH7_VmRandomNum(pA->pMap->pVm);` |
|      - | 1997 | `	/* if the random number is odd then the first node 'pA' is greater then` |
|      - | 1998 | `	 * the second node 'pB'. Otherwise the reverse is assumed.` |
|      - | 1999 | `	 */` |
|     16 | 2000 | `	return n&1 ? 1 : -1;` |
|      1 | 2001 |  |
|      - | 2002 | `/*` |
|      - | 2003 | ` * Rehash all nodes keys after a merge-sort have been applied.` |
|      - | 2004 | ` * Used by [sort(),usort() and rsort()].` |
|      - | 2005 | ` */` |
|    440 | 2006 | `static void HashmapSortRehash(ph7_hashmap *pMap)` |
|      2 | 2007 |  |
|      - | 2008 | `	ph7_hashmap_node *p,*pLast;` |
|      - | 2009 | `	sxu32 i;` |
|      - | 2010 | `	/* Rehash all entries */` |
|    442 | 2011 | `	pLast = p = pMap->pFirst;` |
|    442 | 2012 | `	pMap->iNextIdx = 0; /* Reset the automatic index */` |
|    442 | 2013 | `	i = 0;` |
|   3633 | 2014 | `	for( ;; ){` |
|   7268 | 2015 | `		if( i >= pMap->nEntry ){` |
|    442 | 2016 | `			pMap->pLast = pLast; /* Fix the last link broken by the merge-sort */` |
|    442 | 2017 | `			break;` |
|      - | 2018 | `		}` |
|   6828 | 2019 | `		if( p->iType == HASHMAP_BLOB_NODE ){` |
|      - | 2020 | `			/* Do not maintain index association as requested by the PHP specification */` |
|      5 | 2021 | `			SyBlobRelease(&p->xKey.sKey);` |
|      - | 2022 | `			/* Change key type */` |
|      5 | 2023 | `			p->iType = HASHMAP_INT_NODE;` |
|      2 | 2024 | `		}` |
|   6828 | 2025 | `		HashmapRehashIntNode(p);` |
|      - | 2026 | `		/* Point to the next entry */` |
|   6828 | 2027 | `		i++;` |
|   6828 | 2028 | `		pLast = p;` |
|   6828 | 2029 | `		p = p->pPrev; /* Reverse link */` |
|      2 | 2030 | `	}` |
|    442 | 2031 |  |
|      - | 2032 | `/*` |
|      - | 2033 | ` * Array functions implementation.` |
|      - | 2034 | ` * Status:` |
|      - | 2035 | ` *  Stable.` |
|      - | 2036 | ` */` |
|      - | 2037 | `/*` |
|      - | 2038 | ` * bool sort(array &$array[,int $sort_flags = SORT_REGULAR ] )` |
|      - | 2039 | ` * Sort an array.` |
|      - | 2040 | ` * Parameters` |
|      - | 2041 | ` *  $array` |
|      - | 2042 | ` *   The input array.` |
|      - | 2043 | ` * $sort_flags` |
|      - | 2044 | ` *  The optional second parameter sort_flags may be used to modify the sorting behavior using these values:` |
|      - | 2045 | ` *  Sorting type flags:` |
|      - | 2046 | ` *   SORT_REGULAR - compare items normally (don't change types)` |
|      - | 2047 | ` *   SORT_NUMERIC - compare items numerically` |
|      - | 2048 | ` *   SORT_STRING - compare items as strings` |
|      - | 2049 | ` * Return` |
|      - | 2050 | ` *  TRUE on success or FALSE on failure.` |
|      - | 2051 | ` *` |
|      - | 2052 | ` */` |
|    774 | 2053 | `static int ph7_hashmap_sort(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      2 | 2054 |  |
|      - | 2055 | `	ph7_hashmap *pMap;` |
|      - | 2056 | `	/* Make sure we are dealing with a valid hashmap */` |
|    776 | 2057 | `	if( nArg < 1 \|\| !ph7_value_is_array(apArg[0]) ){` |
|      - | 2058 | `		/* Missing/Invalid arguments,return FALSE */` |
|    ! 0 | 2059 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 2060 | `		return PH7_OK;` |
|      - | 2061 | `	}` |
|      - | 2062 | `	/* Point to the internal representation of the input hashmap */` |
|    776 | 2063 | `	pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|    776 | 2064 | `	if( pMap->nEntry > 1 ){` |
|    436 | 2065 | `		sxi32 iCmpFlags = 0;` |
|    436 | 2066 | `		if( nArg > 1 ){` |
|      - | 2067 | `			/* Extract comparison flags */` |
|      3 | 2068 | `			iCmpFlags = ph7_value_to_int(apArg[1]);` |
|      3 | 2069 | `			if( iCmpFlags == 3 /* SORT_REGULAR */ ){` |
|    ! 0 | 2070 | `				iCmpFlags = 0; /* Standard comparison */` |
|    ! 0 | 2071 | `			}` |
|      1 | 2072 | `		}` |
|      - | 2073 | `		/* Do the merge sort */` |
|    436 | 2074 | `		HashmapMergeSort(pMap,HashmapCmpCallback1,SX_INT_TO_PTR(iCmpFlags));` |
|      - | 2075 | `		/* Rehash [Do not maintain index association as requested by the PHP specification] */` |
|    436 | 2076 | `		HashmapSortRehash(pMap);` |
|    217 | 2077 | `	}` |
|      - | 2078 | `	/* All done,return TRUE */` |
|    776 | 2079 | `	ph7_result_bool(pCtx,1);` |
|    776 | 2080 | `	return PH7_OK;` |
|    389 | 2081 |  |
|      - | 2082 | `/*` |
|      - | 2083 | ` * bool asort(array &$array[,int $sort_flags = SORT_REGULAR ] )` |
|      - | 2084 | ` *  Sort an array and maintain index association.` |
|      - | 2085 | ` * Parameters` |
|      - | 2086 | ` *  $array` |
|      - | 2087 | ` *   The input array.` |
|      - | 2088 | ` * $sort_flags` |
|      - | 2089 | ` *  The optional second parameter sort_flags may be used to modify the sorting behavior using these values:` |
|      - | 2090 | ` *  Sorting type flags:` |
|      - | 2091 | ` *   SORT_REGULAR - compare items normally (don't change types)` |
|      - | 2092 | ` *   SORT_NUMERIC - compare items numerically` |
|      - | 2093 | ` *   SORT_STRING - compare items as strings` |
|      - | 2094 | ` * Return` |
|      - | 2095 | ` *  TRUE on success or FALSE on failure.` |
|      - | 2096 | ` */` |
|      2 | 2097 | `static int ph7_hashmap_asort(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 2098 |  |
|      - | 2099 | `	ph7_hashmap *pMap;` |
|      - | 2100 | `	/* Make sure we are dealing with a valid hashmap */` |
|      3 | 2101 | `	if( nArg < 1 \|\| !ph7_value_is_array(apArg[0]) ){` |
|      - | 2102 | `		/* Missing/Invalid arguments,return FALSE */` |
|    ! 0 | 2103 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 2104 | `		return PH7_OK;` |
|      - | 2105 | `	}` |
|      - | 2106 | `	/* Point to the internal representation of the input hashmap */` |
|      3 | 2107 | `	pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|      3 | 2108 | `	if( pMap->nEntry > 1 ){` |
|      3 | 2109 | `		sxi32 iCmpFlags = 0;` |
|      3 | 2110 | `		if( nArg > 1 ){` |
|      - | 2111 | `			/* Extract comparison flags */` |
|    ! 0 | 2112 | `			iCmpFlags = ph7_value_to_int(apArg[1]);` |
|    ! 0 | 2113 | `			if( iCmpFlags == 3 /* SORT_REGULAR */ ){` |
|    ! 0 | 2114 | `				iCmpFlags = 0; /* Standard comparison */` |
|    ! 0 | 2115 | `			}` |
|    ! 0 | 2116 | `		}` |
|      - | 2117 | `		/* Do the merge sort */` |
|      3 | 2118 | `		HashmapMergeSort(pMap,HashmapCmpCallback1,SX_INT_TO_PTR(iCmpFlags));` |
|      - | 2119 | `		/* Fix the last link broken by the merge */` |
|      5 | 2120 | `		while(pMap->pLast->pPrev){` |
|      3 | 2121 | `			pMap->pLast = pMap->pLast->pPrev;` |
|      1 | 2122 | `		}` |
|      1 | 2123 | `	}` |
|      - | 2124 | `	/* All done,return TRUE */` |
|      3 | 2125 | `	ph7_result_bool(pCtx,1);` |
|      3 | 2126 | `	return PH7_OK;` |
|      2 | 2127 |  |
|      - | 2128 | `/*` |
|      - | 2129 | ` * bool arsort(array &$array[,int $sort_flags = SORT_REGULAR ] )` |
|      - | 2130 | ` *  Sort an array in reverse order and maintain index association.` |
|      - | 2131 | ` * Parameters` |
|      - | 2132 | ` *  $array` |
|      - | 2133 | ` *   The input array.` |
|      - | 2134 | ` * $sort_flags` |
|      - | 2135 | ` *  The optional second parameter sort_flags may be used to modify the sorting behavior using these values:` |
|      - | 2136 | ` *  Sorting type flags:` |
|      - | 2137 | ` *   SORT_REGULAR - compare items normally (don't change types)` |
|      - | 2138 | ` *   SORT_NUMERIC - compare items numerically` |
|      - | 2139 | ` *   SORT_STRING - compare items as strings` |
|      - | 2140 | ` * Return` |
|      - | 2141 | ` *  TRUE on success or FALSE on failure.` |
|      - | 2142 | ` */` |
|      2 | 2143 | `static int ph7_hashmap_arsort(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 2144 |  |
|      - | 2145 | `	ph7_hashmap *pMap;` |
|      - | 2146 | `	/* Make sure we are dealing with a valid hashmap */` |
|      3 | 2147 | `	if( nArg < 1 \|\| !ph7_value_is_array(apArg[0]) ){` |
|      - | 2148 | `		/* Missing/Invalid arguments,return FALSE */` |
|    ! 0 | 2149 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 2150 | `		return PH7_OK;` |
|      - | 2151 | `	}` |
|      - | 2152 | `	/* Point to the internal representation of the input hashmap */` |
|      3 | 2153 | `	pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|      3 | 2154 | `	if( pMap->nEntry > 1 ){` |
|      3 | 2155 | `		sxi32 iCmpFlags = 0;` |
|      3 | 2156 | `		if( nArg > 1 ){` |
|      - | 2157 | `			/* Extract comparison flags */` |
|    ! 0 | 2158 | `			iCmpFlags = ph7_value_to_int(apArg[1]);` |
|    ! 0 | 2159 | `			if( iCmpFlags == 3 /* SORT_REGULAR */ ){` |
|    ! 0 | 2160 | `				iCmpFlags = 0; /* Standard comparison */` |
|    ! 0 | 2161 | `			}` |
|    ! 0 | 2162 | `		}` |
|      - | 2163 | `		/* Do the merge sort */` |
|      3 | 2164 | `		HashmapMergeSort(pMap,HashmapCmpCallback3,SX_INT_TO_PTR(iCmpFlags));` |
|      - | 2165 | `		/* Fix the last link broken by the merge */` |
|      5 | 2166 | `		while(pMap->pLast->pPrev){` |
|      3 | 2167 | `			pMap->pLast = pMap->pLast->pPrev;` |
|      1 | 2168 | `		}` |
|      1 | 2169 | `	}` |
|      - | 2170 | `	/* All done,return TRUE */` |
|      3 | 2171 | `	ph7_result_bool(pCtx,1);` |
|      3 | 2172 | `	return PH7_OK;` |
|      2 | 2173 |  |
|      - | 2174 | `/*` |
|      - | 2175 | ` * bool ksort(array &$array[,int $sort_flags = SORT_REGULAR ] )` |
|      - | 2176 | ` *  Sort an array by key.` |
|      - | 2177 | ` * Parameters` |
|      - | 2178 | ` *  $array` |
|      - | 2179 | ` *   The input array.` |
|      - | 2180 | ` * $sort_flags` |
|      - | 2181 | ` *  The optional second parameter sort_flags may be used to modify the sorting behavior using these values:` |
|      - | 2182 | ` *  Sorting type flags:` |
|      - | 2183 | ` *   SORT_REGULAR - compare items normally (don't change types)` |
|      - | 2184 | ` *   SORT_NUMERIC - compare items numerically` |
|      - | 2185 | ` *   SORT_STRING - compare items as strings` |
|      - | 2186 | ` * Return` |
|      - | 2187 | ` *  TRUE on success or FALSE on failure.` |
|      - | 2188 | ` */` |
|      4 | 2189 | `static int ph7_hashmap_ksort(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 2190 |  |
|      - | 2191 | `	ph7_hashmap *pMap;` |
|      - | 2192 | `	/* Make sure we are dealing with a valid hashmap */` |
|      5 | 2193 | `	if( nArg < 1 \|\| !ph7_value_is_array(apArg[0]) ){` |
|      - | 2194 | `		/* Missing/Invalid arguments,return FALSE */` |
|    ! 0 | 2195 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 2196 | `		return PH7_OK;` |
|      - | 2197 | `	}` |
|      - | 2198 | `	/* Point to the internal representation of the input hashmap */` |
|      5 | 2199 | `	pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|      5 | 2200 | `	if( pMap->nEntry > 1 ){` |
|      5 | 2201 | `		sxi32 iCmpFlags = 0;` |
|      5 | 2202 | `		if( nArg > 1 ){` |
|      - | 2203 | `			/* Extract comparison flags */` |
|    ! 0 | 2204 | `			iCmpFlags = ph7_value_to_int(apArg[1]);` |
|    ! 0 | 2205 | `			if( iCmpFlags == 3 /* SORT_REGULAR */ ){` |
|    ! 0 | 2206 | `				iCmpFlags = 0; /* Standard comparison */` |
|    ! 0 | 2207 | `			}` |
|    ! 0 | 2208 | `		}` |
|      - | 2209 | `		/* Do the merge sort */` |
|      5 | 2210 | `		HashmapMergeSort(pMap,HashmapCmpCallback2,SX_INT_TO_PTR(iCmpFlags));` |
|      - | 2211 | `		/* Fix the last link broken by the merge */` |
|     15 | 2212 | `		while(pMap->pLast->pPrev){` |
|     11 | 2213 | `			pMap->pLast = pMap->pLast->pPrev;` |
|      1 | 2214 | `		}` |
|      2 | 2215 | `	}` |
|      - | 2216 | `	/* All done,return TRUE */` |
|      5 | 2217 | `	ph7_result_bool(pCtx,1);` |
|      5 | 2218 | `	return PH7_OK;` |
|      3 | 2219 |  |
|      - | 2220 | `/*` |
|      - | 2221 | ` * bool krsort(array &$array[,int $sort_flags = SORT_REGULAR ] )` |
|      - | 2222 | ` *  Sort an array by key in reverse order.` |
|      - | 2223 | ` * Parameters` |
|      - | 2224 | ` *  $array` |
|      - | 2225 | ` *   The input array.` |
|      - | 2226 | ` * $sort_flags` |
|      - | 2227 | ` *  The optional second parameter sort_flags may be used to modify the sorting behavior using these values:` |
|      - | 2228 | ` *  Sorting type flags:` |
|      - | 2229 | ` *   SORT_REGULAR - compare items normally (don't change types)` |
|      - | 2230 | ` *   SORT_NUMERIC - compare items numerically` |
|      - | 2231 | ` *   SORT_STRING - compare items as strings` |
|      - | 2232 | ` * Return` |
|      - | 2233 | ` *  TRUE on success or FALSE on failure.` |
|      - | 2234 | ` */` |
|      2 | 2235 | `static int ph7_hashmap_krsort(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 2236 |  |
|      - | 2237 | `	ph7_hashmap *pMap;` |
|      - | 2238 | `	/* Make sure we are dealing with a valid hashmap */` |
|      3 | 2239 | `	if( nArg < 1 \|\| !ph7_value_is_array(apArg[0]) ){` |
|      - | 2240 | `		/* Missing/Invalid arguments,return FALSE */` |
|    ! 0 | 2241 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 2242 | `		return PH7_OK;` |
|      - | 2243 | `	}` |
|      - | 2244 | `	/* Point to the internal representation of the input hashmap */` |
|      3 | 2245 | `	pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|      3 | 2246 | `	if( pMap->nEntry > 1 ){` |
|      3 | 2247 | `		sxi32 iCmpFlags = 0;` |
|      3 | 2248 | `		if( nArg > 1 ){` |
|      - | 2249 | `			/* Extract comparison flags */` |
|    ! 0 | 2250 | `			iCmpFlags = ph7_value_to_int(apArg[1]);` |
|    ! 0 | 2251 | `			if( iCmpFlags == 3 /* SORT_REGULAR */ ){` |
|    ! 0 | 2252 | `				iCmpFlags = 0; /* Standard comparison */` |
|    ! 0 | 2253 | `			}` |
|    ! 0 | 2254 | `		}` |
|      - | 2255 | `		/* Do the merge sort */` |
|      3 | 2256 | `		HashmapMergeSort(pMap,HashmapCmpCallback5,SX_INT_TO_PTR(iCmpFlags));` |
|      - | 2257 | `		/* Fix the last link broken by the merge */` |
|      7 | 2258 | `		while(pMap->pLast->pPrev){` |
|      5 | 2259 | `			pMap->pLast = pMap->pLast->pPrev;` |
|      1 | 2260 | `		}` |
|      1 | 2261 | `	}` |
|      - | 2262 | `	/* All done,return TRUE */` |
|      3 | 2263 | `	ph7_result_bool(pCtx,1);` |
|      3 | 2264 | `	return PH7_OK;` |
|      2 | 2265 |  |
|      - | 2266 | `/*` |
|      - | 2267 | ` * bool rsort(array &$array[,int $sort_flags = SORT_REGULAR ] )` |
|      - | 2268 | ` * Sort an array in reverse order.` |
|      - | 2269 | ` * Parameters` |
|      - | 2270 | ` *  $array` |
|      - | 2271 | ` *   The input array.` |
|      - | 2272 | ` * $sort_flags` |
|      - | 2273 | ` *  The optional second parameter sort_flags may be used to modify the sorting behavior using these values:` |
|      - | 2274 | ` *  Sorting type flags:` |
|      - | 2275 | ` *   SORT_REGULAR - compare items normally (don't change types)` |
|      - | 2276 | ` *   SORT_NUMERIC - compare items numerically` |
|      - | 2277 | ` *   SORT_STRING - compare items as strings` |
|      - | 2278 | ` * Return` |
|      - | 2279 | ` *  TRUE on success or FALSE on failure.` |
|      - | 2280 | ` */` |
|      2 | 2281 | `static int ph7_hashmap_rsort(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 2282 |  |
|      - | 2283 | `	ph7_hashmap *pMap;` |
|      - | 2284 | `	/* Make sure we are dealing with a valid hashmap */` |
|      3 | 2285 | `	if( nArg < 1 \|\| !ph7_value_is_array(apArg[0]) ){` |
|      - | 2286 | `		/* Missing/Invalid arguments,return FALSE */` |
|    ! 0 | 2287 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 2288 | `		return PH7_OK;` |
|      - | 2289 | `	}` |
|      - | 2290 | `	/* Point to the internal representation of the input hashmap */` |
|      3 | 2291 | `	pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|      3 | 2292 | `	if( pMap->nEntry > 1 ){` |
|      3 | 2293 | `		sxi32 iCmpFlags = 0;` |
|      3 | 2294 | `		if( nArg > 1 ){` |
|      - | 2295 | `			/* Extract comparison flags */` |
|    ! 0 | 2296 | `			iCmpFlags = ph7_value_to_int(apArg[1]);` |
|    ! 0 | 2297 | `			if( iCmpFlags == 3 /* SORT_REGULAR */ ){` |
|    ! 0 | 2298 | `				iCmpFlags = 0; /* Standard comparison */` |
|    ! 0 | 2299 | `			}` |
|    ! 0 | 2300 | `		}` |
|      - | 2301 | `		/* Do the merge sort */` |
|      3 | 2302 | `		HashmapMergeSort(pMap,HashmapCmpCallback3,SX_INT_TO_PTR(iCmpFlags));` |
|      - | 2303 | `		/* Rehash [Do not maintain index association as requested by the PHP specification] */` |
|      3 | 2304 | `		HashmapSortRehash(pMap);` |
|      1 | 2305 | `	}` |
|      - | 2306 | `	/* All done,return TRUE */` |
|      3 | 2307 | `	ph7_result_bool(pCtx,1);` |
|      3 | 2308 | `	return PH7_OK;` |
|      2 | 2309 |  |
|      - | 2310 | `/*` |
|      - | 2311 | ` * bool usort(array &$array,callable $cmp_function)` |
|      - | 2312 | ` *  Sort an array by values using a user-defined comparison function.` |
|      - | 2313 | ` * Parameters` |
|      - | 2314 | ` *  $array` |
|      - | 2315 | ` *   The input array.` |
|      - | 2316 | ` * $cmp_function` |
|      - | 2317 | ` *  The comparison function must return an integer less than, equal to, or greater` |
|      - | 2318 | ` *  than zero if the first argument is considered to be respectively less than, equal` |
|      - | 2319 | ` *  to, or greater than the second.` |
|      - | 2320 | ` *    int callback ( mixed $a, mixed $b )` |
|      - | 2321 | ` * Return` |
|      - | 2322 | ` *  TRUE on success or FALSE on failure.` |
|      - | 2323 | ` */` |
|      2 | 2324 | `static int ph7_hashmap_usort(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 2325 |  |
|      - | 2326 | `	ph7_hashmap *pMap;` |
|      - | 2327 | `	/* Make sure we are dealing with a valid hashmap */` |
|      3 | 2328 | `	if( nArg < 1 \|\| !ph7_value_is_array(apArg[0]) ){` |
|      - | 2329 | `		/* Missing/Invalid arguments,return FALSE */` |
|    ! 0 | 2330 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 2331 | `		return PH7_OK;` |
|      - | 2332 | `	}` |
|      - | 2333 | `	/* Point to the internal representation of the input hashmap */` |
|      3 | 2334 | `	pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|      3 | 2335 | `	if( pMap->nEntry > 1 ){` |
|      3 | 2336 | `		ph7_value *pCallback = 0;` |
|      - | 2337 | `		ProcNodeCmp xCmp;` |
|      3 | 2338 | `		xCmp = HashmapCmpCallback4; /* User-defined function as the comparison callback */` |
|      3 | 2339 | `		if( nArg > 1 && ph7_value_is_callable(apArg[1]) ){` |
|      - | 2340 | `			/* Point to the desired callback */` |
|      3 | 2341 | `			pCallback = apArg[1];` |
|      2 | 2342 | `		}else{` |
|      - | 2343 | `			/* Use the default comparison function */` |
|    ! 0 | 2344 | `			xCmp = HashmapCmpCallback1;` |
|      - | 2345 | `		}` |
|      - | 2346 | `		/* Do the merge sort */` |
|      3 | 2347 | `		HashmapMergeSort(pMap,xCmp,pCallback);` |
|      - | 2348 | `		/* Rehash [Do not maintain index association as requested by the PHP specification] */` |
|      3 | 2349 | `		HashmapSortRehash(pMap);` |
|      1 | 2350 | `	}` |
|      - | 2351 | `	/* All done,return TRUE */` |
|      3 | 2352 | `	ph7_result_bool(pCtx,1);` |
|      3 | 2353 | `	return PH7_OK;` |
|      2 | 2354 |  |
|      - | 2355 | `/*` |
|      - | 2356 | ` * bool uasort(array &$array,callable $cmp_function)` |
|      - | 2357 | ` *  Sort an array by values using a user-defined comparison function` |
|      - | 2358 | ` *  and maintain index association.` |
|      - | 2359 | ` * Parameters` |
|      - | 2360 | ` *  $array` |
|      - | 2361 | ` *   The input array.` |
|      - | 2362 | ` * $cmp_function` |
|      - | 2363 | ` *  The comparison function must return an integer less than, equal to, or greater` |
|      - | 2364 | ` *  than zero if the first argument is considered to be respectively less than, equal` |
|      - | 2365 | ` *  to, or greater than the second.` |
|      - | 2366 | ` *    int callback ( mixed $a, mixed $b )` |
|      - | 2367 | ` * Return` |
|      - | 2368 | ` *  TRUE on success or FALSE on failure.` |
|      - | 2369 | ` */` |
|      2 | 2370 | `static int ph7_hashmap_uasort(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 2371 |  |
|      - | 2372 | `	ph7_hashmap *pMap;` |
|      - | 2373 | `	/* Make sure we are dealing with a valid hashmap */` |
|      3 | 2374 | `	if( nArg < 1 \|\| !ph7_value_is_array(apArg[0]) ){` |
|      - | 2375 | `		/* Missing/Invalid arguments,return FALSE */` |
|    ! 0 | 2376 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 2377 | `		return PH7_OK;` |
|      - | 2378 | `	}` |
|      - | 2379 | `	/* Point to the internal representation of the input hashmap */` |
|      3 | 2380 | `	pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|      3 | 2381 | `	if( pMap->nEntry > 1 ){` |
|      3 | 2382 | `		ph7_value *pCallback = 0;` |
|      - | 2383 | `		ProcNodeCmp xCmp;` |
|      3 | 2384 | `		xCmp = HashmapCmpCallback4; /* User-defined function as the comparison callback */` |
|      3 | 2385 | `		if( nArg > 1 && ph7_value_is_callable(apArg[1]) ){` |
|      - | 2386 | `			/* Point to the desired callback */` |
|      3 | 2387 | `			pCallback = apArg[1];` |
|      2 | 2388 | `		}else{` |
|      - | 2389 | `			/* Use the default comparison function */` |
|    ! 0 | 2390 | `			xCmp = HashmapCmpCallback1;` |
|      - | 2391 | `		}` |
|      - | 2392 | `		/* Do the merge sort */` |
|      3 | 2393 | `		HashmapMergeSort(pMap,xCmp,pCallback);` |
|      - | 2394 | `		/* Fix the last link broken by the merge */` |
|      5 | 2395 | `		while(pMap->pLast->pPrev){` |
|      3 | 2396 | `			pMap->pLast = pMap->pLast->pPrev;` |
|      1 | 2397 | `		}` |
|      1 | 2398 | `	}` |
|      - | 2399 | `	/* All done,return TRUE */` |
|      3 | 2400 | `	ph7_result_bool(pCtx,1);` |
|      3 | 2401 | `	return PH7_OK;` |
|      2 | 2402 |  |
|      - | 2403 | `/*` |
|      - | 2404 | ` * bool uksort(array &$array,callable $cmp_function)` |
|      - | 2405 | ` *  Sort an array by keys using a user-defined comparison` |
|      - | 2406 | ` *  function and maintain index association.` |
|      - | 2407 | ` * Parameters` |
|      - | 2408 | ` *  $array` |
|      - | 2409 | ` *   The input array.` |
|      - | 2410 | ` * $cmp_function` |
|      - | 2411 | ` *  The comparison function must return an integer less than, equal to, or greater` |
|      - | 2412 | ` *  than zero if the first argument is considered to be respectively less than, equal` |
|      - | 2413 | ` *  to, or greater than the second.` |
|      - | 2414 | ` *    int callback ( mixed $a, mixed $b )` |
|      - | 2415 | ` * Return` |
|      - | 2416 | ` *  TRUE on success or FALSE on failure.` |
|      - | 2417 | ` */` |
|      2 | 2418 | `static int ph7_hashmap_uksort(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 2419 |  |
|      - | 2420 | `	ph7_hashmap *pMap;` |
|      - | 2421 | `	/* Make sure we are dealing with a valid hashmap */` |
|      3 | 2422 | `	if( nArg < 1 \|\| !ph7_value_is_array(apArg[0]) ){` |
|      - | 2423 | `		/* Missing/Invalid arguments,return FALSE */` |
|    ! 0 | 2424 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 2425 | `		return PH7_OK;` |
|      - | 2426 | `	}` |
|      - | 2427 | `	/* Point to the internal representation of the input hashmap */` |
|      3 | 2428 | `	pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|      3 | 2429 | `	if( pMap->nEntry > 1 ){` |
|      3 | 2430 | `		ph7_value *pCallback = 0;` |
|      - | 2431 | `		ProcNodeCmp xCmp;` |
|      3 | 2432 | `		xCmp = HashmapCmpCallback6; /* User-defined function as the comparison callback */` |
|      3 | 2433 | `		if( nArg > 1 && ph7_value_is_callable(apArg[1]) ){` |
|      - | 2434 | `			/* Point to the desired callback */` |
|      3 | 2435 | `			pCallback = apArg[1];` |
|      2 | 2436 | `		}else{` |
|      - | 2437 | `			/* Use the default comparison function */` |
|    ! 0 | 2438 | `			xCmp = HashmapCmpCallback2;` |
|      - | 2439 | `		}` |
|      - | 2440 | `		/* Do the merge sort */` |
|      3 | 2441 | `		HashmapMergeSort(pMap,xCmp,pCallback);` |
|      - | 2442 | `		/* Fix the last link broken by the merge */` |
|      3 | 2443 | `		while(pMap->pLast->pPrev){` |
|    ! 0 | 2444 | `			pMap->pLast = pMap->pLast->pPrev;` |
|    ! 0 | 2445 | `		}` |
|      1 | 2446 | `	}` |
|      - | 2447 | `	/* All done,return TRUE */` |
|      3 | 2448 | `	ph7_result_bool(pCtx,1);` |
|      3 | 2449 | `	return PH7_OK;` |
|      2 | 2450 |  |
|      - | 2451 | `/*` |
|      - | 2452 | ` * bool shuffle(array &$array)` |
|      - | 2453 | ` *  shuffles (randomizes the order of the elements in) an array.` |
|      - | 2454 | ` * Parameters` |
|      - | 2455 | ` *  $array` |
|      - | 2456 | ` *   The input array.` |
|      - | 2457 | ` * Return` |
|      - | 2458 | ` *  TRUE on success or FALSE on failure.` |
|      - | 2459 | ` *` |
|      - | 2460 | ` */` |
|      2 | 2461 | `static int ph7_hashmap_shuffle(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 2462 |  |
|      - | 2463 | `	ph7_hashmap *pMap;` |
|      - | 2464 | `	/* Make sure we are dealing with a valid hashmap */` |
|      3 | 2465 | `	if( nArg < 1 \|\| !ph7_value_is_array(apArg[0]) ){` |
|      - | 2466 | `		/* Missing/Invalid arguments,return FALSE */` |
|    ! 0 | 2467 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 2468 | `		return PH7_OK;` |
|      - | 2469 | `	}` |
|      - | 2470 | `	/* Point to the internal representation of the input hashmap */` |
|      3 | 2471 | `	pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|      3 | 2472 | `	if( pMap->nEntry > 1 ){` |
|      - | 2473 | `		/* Do the merge sort */` |
|      3 | 2474 | `		HashmapMergeSort(pMap,HashmapCmpCallback7,0);` |
|      - | 2475 | `		/* Fix the last link broken by the merge */` |
|      9 | 2476 | `		while(pMap->pLast->pPrev){` |
|      7 | 2477 | `			pMap->pLast = pMap->pLast->pPrev;` |
|      1 | 2478 | `		}` |
|      1 | 2479 | `	}` |
|      - | 2480 | `	/* All done,return TRUE */` |
|      3 | 2481 | `	ph7_result_bool(pCtx,1);` |
|      3 | 2482 | `	return PH7_OK;` |
|      2 | 2483 |  |
|      - | 2484 | `/*` |
|      - | 2485 | ` * int count(array $var [, int $mode = COUNT_NORMAL ])` |
|      - | 2486 | ` *   Count all elements in an array, or something in an object.` |
|      - | 2487 | ` * Parameters` |
|      - | 2488 | ` *  $var` |
|      - | 2489 | ` *   The array or the object.` |
|      - | 2490 | ` * $mode` |
|      - | 2491 | ` *  If the optional mode parameter is set to COUNT_RECURSIVE (or 1), count()` |
|      - | 2492 | ` *  will recursively count the array. This is particularly useful for counting` |
|      - | 2493 | ` *  all the elements of a multidimensional array. count() does not detect infinite` |
|      - | 2494 | ` *  recursion.` |
|      - | 2495 | ` * Return` |
|      - | 2496 | ` *  Returns the number of elements in the array.` |
|      - | 2497 | ` */` |
|    438 | 2498 | `static int ph7_hashmap_count(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      2 | 2499 |  |
|    440 | 2500 | `	int bRecursive = FALSE;` |
|      - | 2501 | `	sxi64 iCount;` |
|    440 | 2502 | `	if( nArg < 1 ){` |
|      - | 2503 | `		/* Missing arguments,return 0 */` |
|    ! 0 | 2504 | `		ph7_result_int(pCtx,0);` |
|    ! 0 | 2505 | `		return PH7_OK;` |
|      - | 2506 | `	}` |
|    440 | 2507 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|      - | 2508 | `		/* TICKET 1433-19: Handle objects */` |
|      3 | 2509 | `		int res = !ph7_value_is_null(apArg[0]);` |
|      3 | 2510 | `		ph7_result_int(pCtx,res);` |
|      3 | 2511 | `		return PH7_OK;` |
|      - | 2512 | `	}` |
|    438 | 2513 | `	if( nArg > 1 ){` |
|      - | 2514 | `		/* Recursive count? */` |
|     31 | 2515 | `		bRecursive = ph7_value_to_int(apArg[1]) == 1 /* COUNT_RECURSIVE */;` |
|     15 | 2516 | `	}` |
|      - | 2517 | `	/* Count */` |
|    438 | 2518 | `	iCount = HashmapCount((ph7_hashmap *)apArg[0]->x.pOther,bRecursive,0);` |
|    438 | 2519 | `	ph7_result_int64(pCtx,iCount);` |
|    438 | 2520 | `	return PH7_OK;` |
|    221 | 2521 |  |
|      - | 2522 | `/*` |
|      - | 2523 | ` * bool array_key_exists(value $key,array $search)` |
|      - | 2524 | ` *  Checks if the given key or index exists in the array.` |
|      - | 2525 | ` * Parameters` |
|      - | 2526 | ` * $key` |
|      - | 2527 | ` *   Value to check.` |
|      - | 2528 | ` * $search` |
|      - | 2529 | ` *  An array with keys to check.` |
|      - | 2530 | ` * Return` |
|      - | 2531 | ` *  TRUE on success or FALSE on failure.` |
|      - | 2532 | ` */` |
|     32 | 2533 | `static int ph7_hashmap_key_exists(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 2534 |  |
|      - | 2535 | `	sxi32 rc;` |
|     33 | 2536 | `	if( nArg < 2 ){` |
|      - | 2537 | `		/* Missing arguments,return FALSE */` |
|      7 | 2538 | `		ph7_result_bool(pCtx,0);` |
|      7 | 2539 | `		return PH7_OK;` |
|      - | 2540 | `	}` |
|      - | 2541 | `	/* Make sure we are dealing with a valid hashmap */` |
|     27 | 2542 | `	if( !ph7_value_is_array(apArg[1]) ){` |
|      - | 2543 | `		/* Invalid argument,return FALSE */` |
|      3 | 2544 | `		ph7_result_bool(pCtx,0);` |
|      3 | 2545 | `		return PH7_OK;` |
|      - | 2546 | `	}` |
|      - | 2547 | `	/* Perform the lookup */` |
|     25 | 2548 | `	rc = PH7_HashmapLookup((ph7_hashmap *)apArg[1]->x.pOther,apArg[0],0);` |
|      - | 2549 | `	/* lookup result */` |
|     25 | 2550 | `	ph7_result_bool(pCtx,rc == SXRET_OK ? 1 : 0);` |
|     25 | 2551 | `	return PH7_OK;` |
|     17 | 2552 |  |
|      - | 2553 | `/*` |
|      - | 2554 | ` * value array_pop(array $array)` |
|      - | 2555 | ` *   POP the last inserted element from the array.` |
|      - | 2556 | ` * Parameter` |
|      - | 2557 | ` *  The array to get the value from.` |
|      - | 2558 | ` * Return` |
|      - | 2559 | ` *  Poped value or NULL on failure.` |
|      - | 2560 | ` */` |
|     16 | 2561 | `static int ph7_hashmap_pop(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      2 | 2562 |  |
|      - | 2563 | `	ph7_hashmap *pMap;` |
|      - | 2564 | `	/* PHP requires exactly one argument and it must be passed by reference */` |
|     18 | 2565 | `	if( nArg != 1 ){` |
|      7 | 2566 | `		return PH7_VmThrowException(pCtx,` |
|      - | 2567 | `			"ArgumentCountError",` |
|      - | 2568 | `			"array_pop() expects exactly 1 argument, %d given",` |
|      2 | 2569 | `			nArg` |
|      - | 2570 | `			);` |
|      - | 2571 | `	}` |
|      - | 2572 | `	/* Passing a constant (including literals) or non-variable triggers the same` |
|      - | 2573 | `	 * error message as official PHP. Check the index to detect constants. */` |
|     14 | 2574 | `	if( apArg[0]->nIdx == SXU32_HIGH ){` |
|      5 | 2575 | `		return PH7_VmThrowException(pCtx,` |
|      - | 2576 | `			"Error",` |
|      - | 2577 | `			"array_pop(): Argument #1 ($array) could not be passed by reference"` |
|      - | 2578 | `			);` |
|      - | 2579 | `	}` |
|      - | 2580 | `	/* Make sure we are dealing with a valid hashmap */` |
|     10 | 2581 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|      4 | 2582 | `		return PH7_VmThrowException(pCtx,` |
|      - | 2583 | `			"TypeError",` |
|      - | 2584 | `			"array_pop(): Argument #1 ($array) must be of type array, %s given",` |
|      1 | 2585 | `			ph7_type_name(apArg[0])` |
|      - | 2586 | `			);` |
|      - | 2587 | `	}` |
|      7 | 2588 | `	pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|      7 | 2589 | `	if( pMap->nEntry < 1 ){` |
|      - | 2590 | `		/* Nothing to pop,return NULL */` |
|      3 | 2591 | `		ph7_result_null(pCtx);` |
|      2 | 2592 | `	}else{` |
|      5 | 2593 | `		ph7_hashmap_node *pLast = pMap->pLast;` |
|      - | 2594 | `		ph7_value *pObj;` |
|      5 | 2595 | `		pObj = HashmapExtractNodeValue(pLast);` |
|      5 | 2596 | `		if( pObj ){` |
|      - | 2597 | `			/* Node value */` |
|      5 | 2598 | `			ph7_result_value(pCtx,pObj);` |
|      - | 2599 | `			/* Unlink the node */` |
|      5 | 2600 | `			PH7_HashmapUnlinkNode(pLast,TRUE);` |
|      3 | 2601 | `		}else{` |
|    ! 0 | 2602 | `			ph7_result_null(pCtx);` |
|      - | 2603 | `		}` |
|      - | 2604 | `		/* Reset the cursor */` |
|      5 | 2605 | `		pMap->pCur = pMap->pFirst;` |
|      - | 2606 | `	}` |
|      7 | 2607 | `	return PH7_OK;` |
|     10 | 2608 |  |
|      - | 2609 | `/*` |
|      - | 2610 | ` * int array_push($array,$var,...)` |
|      - | 2611 | ` *   Push one or more elements onto the end of array. (Stack insertion)` |
|      - | 2612 | ` * Parameters` |
|      - | 2613 | ` *  array` |
|      - | 2614 | ` *    The input array.` |
|      - | 2615 | ` *  var` |
|      - | 2616 | ` *   On or more value to push.` |
|      - | 2617 | ` * Return` |
|      - | 2618 | ` *  New array count (including old items).` |
|      - | 2619 | ` */` |
|      2 | 2620 | `static int ph7_hashmap_push(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 2621 |  |
|      - | 2622 | `	ph7_hashmap *pMap;` |
|      - | 2623 | `	sxi32 rc;` |
|      - | 2624 | `	int i;` |
|      3 | 2625 | `	if( nArg < 1 ){` |
|      - | 2626 | `		/* Missing arguments,return 0 */` |
|    ! 0 | 2627 | `		ph7_result_int(pCtx,0);` |
|    ! 0 | 2628 | `		return PH7_OK;` |
|      - | 2629 | `	}` |
|      - | 2630 | `	/* Make sure we are dealing with a valid hashmap */` |
|      3 | 2631 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|      - | 2632 | `		/* Invalid argument,return 0 */` |
|    ! 0 | 2633 | `		ph7_result_int(pCtx,0);` |
|    ! 0 | 2634 | `		return PH7_OK;` |
|      - | 2635 | `	}` |
|      - | 2636 | `	/* Point to the internal representation of the input hashmap */` |
|      3 | 2637 | `	pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|      - | 2638 | `	/* Start pushing given values */` |
|      7 | 2639 | `	for( i = 1 ; i < nArg ; ++i ){` |
|      5 | 2640 | `		rc = PH7_HashmapInsert(pMap,0,apArg[i]);` |
|      5 | 2641 | `		if( rc != SXRET_OK ){` |
|    ! 0 | 2642 | `			break;` |
|      - | 2643 | `		}` |
|      3 | 2644 | `	}` |
|      - | 2645 | `	/* Return the new count */` |
|      3 | 2646 | `	ph7_result_int64(pCtx,(sxi64)pMap->nEntry);` |
|      3 | 2647 | `	return PH7_OK;` |
|      2 | 2648 |  |
|      - | 2649 | `/*` |
|      - | 2650 | ` * value array_shift(array $array)` |
|      - | 2651 | ` *   Shift an element off the beginning of array.` |
|      - | 2652 | ` * Parameter` |
|      - | 2653 | ` *  The array to get the value from.` |
|      - | 2654 | ` * Return` |
|      - | 2655 | ` *  Shifted value or NULL on failure.` |
|      - | 2656 | ` */` |
|     36 | 2657 | `static int ph7_hashmap_shift(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      2 | 2658 |  |
|      - | 2659 | `	ph7_hashmap *pMap;` |
|      - | 2660 | `	/* PHP requires exactly one argument and it must be passed by reference */` |
|     38 | 2661 | `	if( nArg != 1 ){` |
|      7 | 2662 | `		return PH7_VmThrowException(pCtx,` |
|      - | 2663 | `			"ArgumentCountError",` |
|      - | 2664 | `			"array_shift() expects exactly 1 argument, %d given",` |
|      2 | 2665 | `			nArg` |
|      - | 2666 | `			);` |
|      - | 2667 | `	}` |
|      - | 2668 | `	/* Detect constants or literals, which cannot be passed by reference. */` |
|     34 | 2669 | `	if( apArg[0]->nIdx == SXU32_HIGH ){` |
|      5 | 2670 | `		return PH7_VmThrowException(pCtx,` |
|      - | 2671 | `			"Error",` |
|      - | 2672 | `			"array_shift(): Argument #1 ($array) could not be passed by reference"` |
|      - | 2673 | `			);` |
|      - | 2674 | `	}` |
|      - | 2675 | `	/* Make sure we are dealing with a valid hashmap */` |
|     30 | 2676 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|      4 | 2677 | `		return PH7_VmThrowException(pCtx,` |
|      - | 2678 | `			"TypeError",` |
|      - | 2679 | `			"array_shift(): Argument #1 ($array) must be of type array, %s given",` |
|      1 | 2680 | `			ph7_type_name(apArg[0])` |
|      - | 2681 | `			);` |
|      - | 2682 | `	}` |
|      - | 2683 | `	/* Point to the internal representation of the hashmap */` |
|     28 | 2684 | `	pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|     28 | 2685 | `	if( pMap->nEntry < 1 ){` |
|      - | 2686 | `		/* Empty hashmap,return NULL */` |
|      3 | 2687 | `		ph7_result_null(pCtx);` |
|      2 | 2688 | `	}else{` |
|     26 | 2689 | `		ph7_hashmap_node *pEntry = pMap->pFirst;` |
|      - | 2690 | `		ph7_value *pObj;` |
|      - | 2691 | `		sxu32 n;` |
|     26 | 2692 | `		pObj = HashmapExtractNodeValue(pEntry);` |
|     26 | 2693 | `		if( pObj ){` |
|      - | 2694 | `			/* Node value */` |
|     26 | 2695 | `			ph7_result_value(pCtx,pObj);` |
|      - | 2696 | `			/* Unlink the first node */` |
|     26 | 2697 | `			PH7_HashmapUnlinkNode(pEntry,TRUE);` |
|     14 | 2698 | `		}else{` |
|    ! 0 | 2699 | `			ph7_result_null(pCtx);` |
|      - | 2700 | `		}` |
|      - | 2701 | `		/* Rehash all int keys */` |
|     26 | 2702 | `		n = pMap->nEntry;` |
|     26 | 2703 | `		pEntry = pMap->pFirst;` |
|     26 | 2704 | `		pMap->iNextIdx = 0; /* Reset the automatic index */` |
|     37 | 2705 | `		for(;;){` |
|     76 | 2706 | `			if( n < 1 ){` |
|     26 | 2707 | `				break;` |
|      - | 2708 | `			}` |
|     52 | 2709 | `			if( pEntry->iType == HASHMAP_INT_NODE ){` |
|     52 | 2710 | `				HashmapRehashIntNode(pEntry);` |
|     25 | 2711 | `			}` |
|      - | 2712 | `			/* Point to the next entry */` |
|     52 | 2713 | `			pEntry = pEntry->pPrev; /* Reverse link */` |
|     52 | 2714 | `			n--;` |
|      2 | 2715 | `		}` |
|      - | 2716 | `		/* Reset the cursor */` |
|     26 | 2717 | `		pMap->pCur = pMap->pFirst;` |
|      - | 2718 | `	}` |
|     28 | 2719 | `	return PH7_OK;` |
|     20 | 2720 |  |
|      - | 2721 | `/*` |
|      - | 2722 | ` * Extract the node cursor value.` |
|      - | 2723 | ` */` |
|     24 | 2724 | `static sxi32 HashmapCurrentValue(ph7_context *pCtx,ph7_hashmap *pMap,int iDirection)` |
|      1 | 2725 |  |
|     25 | 2726 | `	ph7_hashmap_node *pCur = pMap->pCur;` |
|      - | 2727 | `	ph7_value *pVal;` |
|     25 | 2728 | `	if( pCur == 0 ){` |
|      - | 2729 | `		/* Cursor does not point to anything,return FALSE */` |
|    ! 0 | 2730 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 2731 | `		return PH7_OK;` |
|      - | 2732 | `	}` |
|     25 | 2733 | `	if( iDirection != 0 ){` |
|      9 | 2734 | `		if( iDirection > 0 ){` |
|      - | 2735 | `			/* Point to the next entry */` |
|      7 | 2736 | `			pMap->pCur = pCur->pPrev; /* Reverse link */` |
|      7 | 2737 | `			pCur = pMap->pCur;` |
|      4 | 2738 | `		}else{` |
|      - | 2739 | `			/* Point to the previous entry */` |
|      3 | 2740 | `			pMap->pCur = pCur->pNext; /* Reverse link */` |
|      3 | 2741 | `			pCur = pMap->pCur;` |
|      - | 2742 | `		}` |
|      9 | 2743 | `		if( pCur == 0 ){` |
|      - | 2744 | `			/* End of input reached,return FALSE */` |
|    ! 0 | 2745 | `			ph7_result_bool(pCtx,0);` |
|    ! 0 | 2746 | `			return PH7_OK;` |
|      - | 2747 | `		}` |
|      4 | 2748 | `	}` |
|      - | 2749 | `	/* Point to the desired element */` |
|     25 | 2750 | `	pVal = HashmapExtractNodeValue(pCur);` |
|     25 | 2751 | `	if( pVal ){` |
|     25 | 2752 | `		ph7_result_value(pCtx,pVal);` |
|     13 | 2753 | `	}else{` |
|    ! 0 | 2754 | `		ph7_result_bool(pCtx,0);` |
|      - | 2755 | `	}` |
|     25 | 2756 | `	return PH7_OK;` |
|     13 | 2757 |  |
|      - | 2758 | `/*` |
|      - | 2759 | ` * value current(array $array)` |
|      - | 2760 | ` *  Return the current element in an array.` |
|      - | 2761 | ` * Parameter` |
|      - | 2762 | ` *  $input: The input array.` |
|      - | 2763 | ` * Return` |
|      - | 2764 | ` *  The current() function simply returns the value of the array element that's currently` |
|      - | 2765 | ` *  being pointed to by the internal pointer. It does not move the pointer in any way.` |
|      - | 2766 | ` *  If the internal pointer points beyond the end of the elements list or the array` |
|      - | 2767 | ` *  is empty, current() returns FALSE.` |
|      - | 2768 | ` */` |
|     10 | 2769 | `static int ph7_hashmap_current(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 2770 |  |
|     11 | 2771 | `	if( nArg < 1 ){` |
|      - | 2772 | `		/* Missing arguments,return FALSE */` |
|    ! 0 | 2773 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 2774 | `		return PH7_OK;` |
|      - | 2775 | `	}` |
|      - | 2776 | `	/* Make sure we are dealing with a valid hashmap */` |
|     11 | 2777 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|      - | 2778 | `		/* Invalid argument,return FALSE */` |
|    ! 0 | 2779 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 2780 | `		return PH7_OK;` |
|      - | 2781 | `	}` |
|     11 | 2782 | `	HashmapCurrentValue(&(*pCtx),(ph7_hashmap *)apArg[0]->x.pOther,0);` |
|     11 | 2783 | `	return PH7_OK;` |
|      6 | 2784 |  |
|      - | 2785 | `/*` |
|      - | 2786 | ` * value next(array $input)` |
|      - | 2787 | ` *  Advance the internal array pointer of an array.` |
|      - | 2788 | ` * Parameter` |
|      - | 2789 | ` *  $input: The input array.` |
|      - | 2790 | ` * Return` |
|      - | 2791 | ` *  next() behaves like current(), with one difference. It advances the internal array` |
|      - | 2792 | ` *  pointer one place forward before returning the element value. That means it returns` |
|      - | 2793 | ` *  the next array value and advances the internal array pointer by one.` |
|      - | 2794 | ` */` |
|      6 | 2795 | `static int ph7_hashmap_next(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 2796 |  |
|      7 | 2797 | `	if( nArg < 1 ){` |
|      - | 2798 | `		/* Missing arguments,return FALSE */` |
|    ! 0 | 2799 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 2800 | `		return PH7_OK;` |
|      - | 2801 | `	}` |
|      - | 2802 | `	/* Make sure we are dealing with a valid hashmap */` |
|      7 | 2803 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|      - | 2804 | `		/* Invalid argument,return FALSE */` |
|    ! 0 | 2805 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 2806 | `		return PH7_OK;` |
|      - | 2807 | `	}` |
|      7 | 2808 | `	HashmapCurrentValue(&(*pCtx),(ph7_hashmap *)apArg[0]->x.pOther,1);` |
|      7 | 2809 | `	return PH7_OK;` |
|      4 | 2810 |  |
|      - | 2811 | `/*` |
|      - | 2812 | ` * value prev(array $input)` |
|      - | 2813 | ` *  Rewind the internal array pointer.` |
|      - | 2814 | ` * Parameter` |
|      - | 2815 | ` *  $input: The input array.` |
|      - | 2816 | ` * Return` |
|      - | 2817 | ` *  Returns the array value in the previous place that's pointed` |
|      - | 2818 | ` *  to by the internal array pointer, or FALSE if there are no more` |
|      - | 2819 | ` *  elements.` |
|      - | 2820 | ` */` |
|      2 | 2821 | `static int ph7_hashmap_prev(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 2822 |  |
|      3 | 2823 | `	if( nArg < 1 ){` |
|      - | 2824 | `		/* Missing arguments,return FALSE */` |
|    ! 0 | 2825 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 2826 | `		return PH7_OK;` |
|      - | 2827 | `	}` |
|      - | 2828 | `	/* Make sure we are dealing with a valid hashmap */` |
|      3 | 2829 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|      - | 2830 | `		/* Invalid argument,return FALSE */` |
|    ! 0 | 2831 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 2832 | `		return PH7_OK;` |
|      - | 2833 | `	}` |
|      3 | 2834 | `	HashmapCurrentValue(&(*pCtx),(ph7_hashmap *)apArg[0]->x.pOther,-1);` |
|      3 | 2835 | `	return PH7_OK;` |
|      2 | 2836 |  |
|      - | 2837 | `/*` |
|      - | 2838 | ` * value end(array $input)` |
|      - | 2839 | ` *  Set the internal pointer of an array to its last element.` |
|      - | 2840 | ` * Parameter` |
|      - | 2841 | ` *  $input: The input array.` |
|      - | 2842 | ` * Return` |
|      - | 2843 | ` *  Returns the value of the last element or FALSE for empty array.` |
|      - | 2844 | ` */` |
|      2 | 2845 | `static int ph7_hashmap_end(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 2846 |  |
|      - | 2847 | `	ph7_hashmap *pMap;` |
|      3 | 2848 | `	if( nArg < 1 ){` |
|      - | 2849 | `		/* Missing arguments,return FALSE */` |
|    ! 0 | 2850 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 2851 | `		return PH7_OK;` |
|      - | 2852 | `	}` |
|      - | 2853 | `	/* Make sure we are dealing with a valid hashmap */` |
|      3 | 2854 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|      - | 2855 | `		/* Invalid argument,return FALSE */` |
|    ! 0 | 2856 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 2857 | `		return PH7_OK;` |
|      - | 2858 | `	}` |
|      - | 2859 | `	/* Point to the internal representation of the input hashmap */` |
|      3 | 2860 | `	pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|      - | 2861 | `	/* Point to the last node */` |
|      3 | 2862 | `	pMap->pCur = pMap->pLast;` |
|      - | 2863 | `	/* Return the last node value */` |
|      3 | 2864 | `	HashmapCurrentValue(&(*pCtx),pMap,0);` |
|      3 | 2865 | `	return PH7_OK;` |
|      2 | 2866 |  |
|      - | 2867 | `/*` |
|      - | 2868 | ` * value reset(array $array )` |
|      - | 2869 | ` *  Set the internal pointer of an array to its first element.` |
|      - | 2870 | ` * Parameter` |
|      - | 2871 | ` *  $input: The input array.` |
|      - | 2872 | ` * Return` |
|      - | 2873 | ` *  Returns the value of the first array element,or FALSE if the array is empty.` |
|      - | 2874 | ` */` |
|      4 | 2875 | `static int ph7_hashmap_reset(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 2876 |  |
|      - | 2877 | `	ph7_hashmap *pMap;` |
|      5 | 2878 | `	if( nArg < 1 ){` |
|      - | 2879 | `		/* Missing arguments,return FALSE */` |
|    ! 0 | 2880 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 2881 | `		return PH7_OK;` |
|      - | 2882 | `	}` |
|      - | 2883 | `	/* Make sure we are dealing with a valid hashmap */` |
|      5 | 2884 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|      - | 2885 | `		/* Invalid argument,return FALSE */` |
|    ! 0 | 2886 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 2887 | `		return PH7_OK;` |
|      - | 2888 | `	}` |
|      - | 2889 | `	/* Point to the internal representation of the input hashmap */` |
|      5 | 2890 | `	pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|      - | 2891 | `	/* Point to the first node */` |
|      5 | 2892 | `	pMap->pCur = pMap->pFirst;` |
|      - | 2893 | `	/* Return the last node value if available */` |
|      5 | 2894 | `	HashmapCurrentValue(&(*pCtx),pMap,0);` |
|      5 | 2895 | `	return PH7_OK;` |
|      3 | 2896 |  |
|      - | 2897 | `/*` |
|      - | 2898 | ` * value key(array $array)` |
|      - | 2899 | ` *   Fetch a key from an array` |
|      - | 2900 | ` * Parameter` |
|      - | 2901 | ` *  $input` |
|      - | 2902 | ` *   The input array.` |
|      - | 2903 | ` * Return` |
|      - | 2904 | ` *  The key() function simply returns the key of the array element that's currently` |
|      - | 2905 | ` *  being pointed to by the internal pointer. It does not move the pointer in any way.` |
|      - | 2906 | ` *  If the internal pointer points beyond the end of the elements list or the array` |
|      - | 2907 | ` *  is empty, key() returns NULL.` |
|      - | 2908 | ` */` |
|      4 | 2909 | `static int ph7_hashmap_simple_key(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 2910 |  |
|      - | 2911 | `	ph7_hashmap_node *pCur;` |
|      - | 2912 | `	ph7_hashmap *pMap;` |
|      5 | 2913 | `	if( nArg < 1 ){` |
|      - | 2914 | `		/* Missing arguments,return NULL */` |
|    ! 0 | 2915 | `		ph7_result_null(pCtx);` |
|    ! 0 | 2916 | `		return PH7_OK;` |
|      - | 2917 | `	}` |
|      - | 2918 | `	/* Make sure we are dealing with a valid hashmap */` |
|      5 | 2919 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|      - | 2920 | `		/* Invalid argument,return NULL */` |
|    ! 0 | 2921 | `		ph7_result_null(pCtx);` |
|    ! 0 | 2922 | `		return PH7_OK;` |
|      - | 2923 | `	}` |
|      5 | 2924 | `	pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|      5 | 2925 | `	pCur = pMap->pCur;` |
|      5 | 2926 | `	if( pCur == 0 ){` |
|      - | 2927 | `		/* Cursor does not point to anything,return NULL */` |
|    ! 0 | 2928 | `		ph7_result_null(pCtx);` |
|    ! 0 | 2929 | `		return PH7_OK;` |
|      - | 2930 | `	}` |
|      5 | 2931 | `	if( pCur->iType == HASHMAP_INT_NODE){` |
|      - | 2932 | `		/* Key is integer */` |
|    ! 0 | 2933 | `		ph7_result_int64(pCtx,pCur->xKey.iKey);` |
|    ! 0 | 2934 | `	}else{` |
|      - | 2935 | `		/* Key is blob */` |
|      7 | 2936 | `		ph7_result_string(pCtx,` |
|      4 | 2937 | `			(const char *)SyBlobData(&pCur->xKey.sKey),(int)SyBlobLength(&pCur->xKey.sKey));` |
|      - | 2938 | `	}` |
|      5 | 2939 | `	return PH7_OK;` |
|      3 | 2940 |  |
|      - | 2941 | `/*` |
|      - | 2942 | ` * array each(array $input)` |
|      - | 2943 | ` *  Return the current key and value pair from an array and advance the array cursor.` |
|      - | 2944 | ` * Parameter` |
|      - | 2945 | ` *  $input` |
|      - | 2946 | ` *    The input array.` |
|      - | 2947 | ` * Return` |
|      - | 2948 | ` *  Returns the current key and value pair from the array array. This pair is returned` |
|      - | 2949 | ` *  in a four-element array, with the keys 0, 1, key, and value. Elements 0 and key` |
|      - | 2950 | ` *  contain the key name of the array element, and 1 and value contain the data.` |
|      - | 2951 | ` *  If the internal pointer for the array points past the end of the array contents` |
|      - | 2952 | ` *  each() returns FALSE.` |
|      - | 2953 | ` */` |
|     22 | 2954 | `static int ph7_hashmap_each(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 2955 |  |
|      - | 2956 | `	ph7_hashmap_node *pCur;` |
|      - | 2957 | `	ph7_hashmap *pMap;` |
|      - | 2958 | `	ph7_value *pArray;` |
|      - | 2959 | `	ph7_value *pVal;` |
|      - | 2960 | `	ph7_value sKey;` |
|     23 | 2961 | `	if( nArg < 1 ){` |
|      - | 2962 | `		/* Missing arguments,return FALSE */` |
|    ! 0 | 2963 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 2964 | `		return PH7_OK;` |
|      - | 2965 | `	}` |
|      - | 2966 | `	/* Make sure we are dealing with a valid hashmap */` |
|     23 | 2967 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|      - | 2968 | `		/* Invalid argument,return FALSE */` |
|    ! 0 | 2969 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 2970 | `		return PH7_OK;` |
|      - | 2971 | `	}` |
|      - | 2972 | `	/* Point to the internal representation that describe the input hashmap */` |
|     23 | 2973 | `	pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|     23 | 2974 | `	if( pMap->pCur == 0 ){` |
|      - | 2975 | `		/* Cursor does not point to anything,return FALSE */` |
|      9 | 2976 | `		ph7_result_bool(pCtx,0);` |
|      9 | 2977 | `		return PH7_OK;` |
|      - | 2978 | `	}` |
|     15 | 2979 | `	pCur = pMap->pCur;` |
|      - | 2980 | `	/* Create a new array */` |
|     15 | 2981 | `	pArray = ph7_context_new_array(pCtx);` |
|     15 | 2982 | `	if( pArray == 0 ){` |
|    ! 0 | 2983 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 2984 | `		return PH7_OK;` |
|      - | 2985 | `	}` |
|     15 | 2986 | `	pVal = HashmapExtractNodeValue(pCur);` |
|      - | 2987 | `	/* Insert the current value */` |
|     15 | 2988 | `	ph7_array_add_intkey_elem(pArray,1,pVal);` |
|     15 | 2989 | `	ph7_array_add_strkey_elem(pArray,"value",pVal);` |
|      - | 2990 | `	/* Make the key */` |
|     15 | 2991 | `	if( pCur->iType == HASHMAP_INT_NODE ){` |
|      7 | 2992 | `		PH7_MemObjInitFromInt(pMap->pVm,&sKey,pCur->xKey.iKey);` |
|      4 | 2993 | `	}else{` |
|      9 | 2994 | `		PH7_MemObjInitFromString(pMap->pVm,&sKey,0);` |
|      9 | 2995 | `		PH7_MemObjStringAppend(&sKey,(const char *)SyBlobData(&pCur->xKey.sKey),SyBlobLength(&pCur->xKey.sKey));` |
|      - | 2996 | `	}` |
|      - | 2997 | `	/* Insert the current key */` |
|     15 | 2998 | `	ph7_array_add_intkey_elem(pArray,0,&sKey);` |
|     15 | 2999 | `	ph7_array_add_strkey_elem(pArray,"key",&sKey);` |
|     15 | 3000 | `	PH7_MemObjRelease(&sKey);` |
|      - | 3001 | `	/* Advance the cursor */` |
|     15 | 3002 | `	pMap->pCur = pCur->pPrev; /* Reverse link */` |
|      - | 3003 | `	/* Return the current entry */` |
|     15 | 3004 | `	ph7_result_value(pCtx,pArray);` |
|     15 | 3005 | `	return PH7_OK;` |
|     12 | 3006 |  |
|      - | 3007 | `/*` |
|      - | 3008 | ` * array range(int $start,int $limit,int $step)` |
|      - | 3009 | ` *  Create an array containing a range of elements` |
|      - | 3010 | ` * Parameter` |
|      - | 3011 | ` *  start` |
|      - | 3012 | ` *   First value of the sequence.` |
|      - | 3013 | ` *  limit` |
|      - | 3014 | ` *   The sequence is ended upon reaching the limit value.` |
|      - | 3015 | ` *  step` |
|      - | 3016 | ` *  If a step value is given, it will be used as the increment between elements in the sequence.` |
|      - | 3017 | ` *  step should be given as a positive number. If not specified, step will default to 1.` |
|      - | 3018 | ` * Return` |
|      - | 3019 | ` *  An array of elements from start to limit, inclusive.` |
|      - | 3020 | ` * NOTE:` |
|      - | 3021 | ` *  Only 32/64 bit integer key is supported.` |
|      - | 3022 | ` */` |
|      2 | 3023 | `static int ph7_hashmap_range(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 3024 |  |
|      - | 3025 | `	ph7_value *pValue,*pArray;` |
|      - | 3026 | `	sxi64 iOfft,iLimit;` |
|      3 | 3027 | `	int iStep = 1;` |
|      - | 3028 |  |
|      3 | 3029 | `	iOfft = iLimit = 0; /* cc -O6 */` |
|      3 | 3030 | `	if( nArg > 0 ){` |
|      - | 3031 | `		/* Extract the offset */` |
|      3 | 3032 | `		iOfft = ph7_value_to_int64(apArg[0]);` |
|      3 | 3033 | `		if( nArg > 1 ){` |
|      - | 3034 | `			/* Extract the limit */` |
|      3 | 3035 | `			iLimit = ph7_value_to_int64(apArg[1]);` |
|      3 | 3036 | `			if( nArg > 2 ){` |
|      - | 3037 | `				/* Extract the increment */` |
|      3 | 3038 | `				iStep = ph7_value_to_int(apArg[2]);` |
|      3 | 3039 | `				if( iStep < 1 ){` |
|      - | 3040 | `					/* Only positive number are allowed */` |
|      3 | 3041 | `					iStep = 1;` |
|      1 | 3042 | `				}` |
|      1 | 3043 | `			}` |
|      1 | 3044 | `		}` |
|      1 | 3045 | `	}` |
|      - | 3046 | `	/* Element container */` |
|      3 | 3047 | `	pValue = ph7_context_new_scalar(pCtx);` |
|      - | 3048 | `	/* Create the new array */` |
|      3 | 3049 | `	pArray = ph7_context_new_array(pCtx);` |
|      3 | 3050 | `	if( pArray == 0 ){` |
|    ! 0 | 3051 | `		ph7_result_null(pCtx);` |
|    ! 0 | 3052 | `		return PH7_OK;` |
|      - | 3053 | `	}` |
|      - | 3054 | `	/* Start filling */` |
|      3 | 3055 | `	while( iOfft <= iLimit ){` |
|    ! 0 | 3056 | `		ph7_value_int64(pValue,iOfft);` |
|      - | 3057 | `		/* Perform the insertion */` |
|    ! 0 | 3058 | `		ph7_array_add_elem(pArray,0/* Automatic index assign*/,pValue);` |
|      - | 3059 | `		/* Increment */` |
|    ! 0 | 3060 | `		iOfft += iStep;` |
|    ! 0 | 3061 | `	}` |
|      - | 3062 | `	/* Return the new array */` |
|      3 | 3063 | `	ph7_result_value(pCtx,pArray);` |
|      - | 3064 | `	/* Dont'worry about freeing 'pValue',it will be released automatically` |
|      - | 3065 | `	 * by the virtual machine as soon we return from this foreign function.` |
|      - | 3066 | `	 */` |
|      3 | 3067 | `	return PH7_OK;` |
|      2 | 3068 |  |
|      - | 3069 | `/*` |
|      - | 3070 | ` * array array_values(array $input)` |
|      - | 3071 | ` *   Returns all the values from the input array and indexes numerically the array.` |
|      - | 3072 | ` * Parameters` |
|      - | 3073 | ` *   input: The input array.` |
|      - | 3074 | ` * Return` |
|      - | 3075 | ` *  An indexed array of values or NULL on failure.` |
|      - | 3076 | ` */` |
|     20 | 3077 | `static int ph7_hashmap_values(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 3078 |  |
|      - | 3079 | `	ph7_hashmap_node *pNode;` |
|      - | 3080 | `	ph7_hashmap *pMap;` |
|      - | 3081 | `	ph7_value *pArray;` |
|      - | 3082 | `	ph7_value *pObj;` |
|      - | 3083 | `	sxu32 n;` |
|     21 | 3084 | `	if( nArg < 1 ){` |
|      - | 3085 | `		/* Missing arguments,return NULL */` |
|      3 | 3086 | `		ph7_result_null(pCtx);` |
|      3 | 3087 | `		return PH7_OK;` |
|      - | 3088 | `	}` |
|      - | 3089 | `	/* Make sure we are dealing with a valid hashmap */` |
|     19 | 3090 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|      - | 3091 | `		/* Invalid argument,return NULL */` |
|    ! 0 | 3092 | `		ph7_result_null(pCtx);` |
|    ! 0 | 3093 | `		return PH7_OK;` |
|      - | 3094 | `	}` |
|      - | 3095 | `	/* Point to the internal representation that describe the input hashmap */` |
|     19 | 3096 | `	pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|      - | 3097 | `	/* Create a new array */` |
|     19 | 3098 | `	pArray = ph7_context_new_array(pCtx);` |
|     19 | 3099 | `	if( pArray == 0 ){` |
|    ! 0 | 3100 | `		ph7_result_null(pCtx);` |
|    ! 0 | 3101 | `		return PH7_OK;` |
|      - | 3102 | `	}` |
|      - | 3103 | `	/* Perform the requested operation */` |
|     19 | 3104 | `	pNode = pMap->pFirst;` |
|     69 | 3105 | `	for( n = 0 ; n < pMap->nEntry ; ++n ){` |
|     51 | 3106 | `		pObj = HashmapExtractNodeValue(pNode);` |
|     51 | 3107 | `		if( pObj ){` |
|      - | 3108 | `			/* perform the insertion */` |
|     51 | 3109 | `			ph7_array_add_elem(pArray,0/* Automatic index assign */,pObj);` |
|     25 | 3110 | `		}` |
|      - | 3111 | `		/* Point to the next entry */` |
|     51 | 3112 | `		pNode = pNode->pPrev; /* Reverse link */` |
|     26 | 3113 | `	}` |
|      - | 3114 | `	/* return the new array */` |
|     19 | 3115 | `	ph7_result_value(pCtx,pArray);` |
|     19 | 3116 | `	return PH7_OK;` |
|     11 | 3117 |  |
|      - | 3118 | `/*` |
|      - | 3119 | ` * array array_keys(array $input [, val $search_value [, bool $strict = false ]] )` |
|      - | 3120 | ` *  Return all the keys or a subset of the keys of an array.` |
|      - | 3121 | ` * Parameters` |
|      - | 3122 | ` *  $input` |
|      - | 3123 | ` *   An array containing keys to return.` |
|      - | 3124 | ` * $search_value` |
|      - | 3125 | ` *   If specified, then only keys containing these values are returned.` |
|      - | 3126 | ` * $strict` |
|      - | 3127 | ` *   Determines if strict comparison (===) should be used during the search.` |
|      - | 3128 | ` * Return` |
|      - | 3129 | ` *  An array of all the keys in input or NULL on failure.` |
|      - | 3130 | ` */` |
|     68 | 3131 | `static int ph7_hashmap_keys(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 3132 |  |
|      - | 3133 | `	ph7_hashmap_node *pNode;` |
|      - | 3134 | `	ph7_hashmap *pMap;` |
|      - | 3135 | `	ph7_value *pArray;` |
|      - | 3136 | `	ph7_value sObj;` |
|      - | 3137 | `	ph7_value sVal;` |
|      - | 3138 | `	SyString sKey;` |
|      - | 3139 | `	int bStrict;` |
|      - | 3140 | `	sxi32 rc;` |
|      - | 3141 | `	sxu32 n;` |
|     69 | 3142 | `	if( nArg < 1 ){` |
|      - | 3143 | `		/* Missing arguments,return NULL */` |
|      3 | 3144 | `		ph7_result_null(pCtx);` |
|      3 | 3145 | `		return PH7_OK;` |
|      - | 3146 | `	}` |
|      - | 3147 | `	/* Make sure we are dealing with a valid hashmap */` |
|     67 | 3148 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|      - | 3149 | `		/* Invalid argument,return NULL */` |
|    ! 0 | 3150 | `		ph7_result_null(pCtx);` |
|    ! 0 | 3151 | `		return PH7_OK;` |
|      - | 3152 | `	}` |
|      - | 3153 | `	/* Point to the internal representation of the input hashmap */` |
|     67 | 3154 | `	pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|      - | 3155 | `	/* Create a new array */` |
|     67 | 3156 | `	pArray = ph7_context_new_array(pCtx);` |
|     67 | 3157 | `	if( pArray == 0 ){` |
|    ! 0 | 3158 | `		ph7_result_null(pCtx);` |
|    ! 0 | 3159 | `		return PH7_OK;` |
|      - | 3160 | `	}` |
|     67 | 3161 | `	bStrict = FALSE;` |
|     67 | 3162 | `	if( nArg > 2 && ph7_value_is_bool(apArg[2]) ){` |
|    ! 0 | 3163 | `		bStrict = ph7_value_to_bool(apArg[2]);` |
|    ! 0 | 3164 | `	}` |
|      - | 3165 | `	/* Perform the requested operation */` |
|     67 | 3166 | `	pNode = pMap->pFirst;` |
|     67 | 3167 | `	PH7_MemObjInit(pMap->pVm,&sVal);` |
|    395 | 3168 | `	for( n = 0 ; n < pMap->nEntry ; ++n ){` |
|    329 | 3169 | `		if( pNode->iType == HASHMAP_INT_NODE ){` |
|     49 | 3170 | `			PH7_MemObjInitFromInt(pMap->pVm,&sObj,pNode->xKey.iKey);` |
|     25 | 3171 | `		}else{` |
|    281 | 3172 | `			SyStringInitFromBuf(&sKey,SyBlobData(&pNode->xKey.sKey),SyBlobLength(&pNode->xKey.sKey));` |
|    281 | 3173 | `			PH7_MemObjInitFromString(pMap->pVm,&sObj,&sKey);` |
|      - | 3174 | `		}` |
|    329 | 3175 | `		rc = 0;` |
|    329 | 3176 | `		if( nArg > 1 ){` |
|    ! 0 | 3177 | `			ph7_value *pValue = HashmapExtractNodeValue(pNode);` |
|    ! 0 | 3178 | `			if( pValue ){` |
|    ! 0 | 3179 | `				PH7_MemObjLoad(pValue,&sVal);` |
|      - | 3180 | `				/* Filter key */` |
|    ! 0 | 3181 | `				rc = ph7_value_compare(&sVal,apArg[1],bStrict);` |
|    ! 0 | 3182 | `				PH7_MemObjRelease(pValue);` |
|    ! 0 | 3183 | `			}` |
|    ! 0 | 3184 | `		}` |
|    329 | 3185 | `		if( rc == 0 ){` |
|      - | 3186 | `			/* Perform the insertion */` |
|    329 | 3187 | `			ph7_array_add_elem(pArray,0,&sObj);` |
|    164 | 3188 | `		}` |
|    329 | 3189 | `		PH7_MemObjRelease(&sObj);` |
|      - | 3190 | `		/* Point to the next entry */` |
|    329 | 3191 | `		pNode = pNode->pPrev; /* Reverse link */` |
|    165 | 3192 | `	}` |
|      - | 3193 | `	/* return the new array */` |
|     67 | 3194 | `	ph7_result_value(pCtx,pArray);` |
|     67 | 3195 | `	return PH7_OK;` |
|     35 | 3196 |  |
|      - | 3197 | `/*` |
|      - | 3198 | ` * bool array_same(array $arr1,array $arr2)` |
|      - | 3199 | ` *  Return TRUE if the given arrays are the same instance.` |
|      - | 3200 | ` *  This function is useful under PH7 since arrays are passed` |
|      - | 3201 | ` *  by reference unlike the zend engine which use pass by values.` |
|      - | 3202 | ` * Parameters` |
|      - | 3203 | ` *  $arr1` |
|      - | 3204 | ` *   First array` |
|      - | 3205 | ` *  $arr2` |
|      - | 3206 | ` *   Second array` |
|      - | 3207 | ` * Return` |
|      - | 3208 | ` *  TRUE if the arrays are the same instance.FALSE otherwise.` |
|      - | 3209 | ` * Note` |
|      - | 3210 | ` *  This function is a symisc eXtension.` |
|      - | 3211 | ` */` |
|      4 | 3212 | `static int ph7_hashmap_same(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 3213 |  |
|      - | 3214 | `	ph7_hashmap *p1,*p2;` |
|      - | 3215 | `	int rc;` |
|      5 | 3216 | `	if( nArg < 2 \|\| !ph7_value_is_array(apArg[0]) \|\| !ph7_value_is_array(apArg[1]) ){` |
|      - | 3217 | `		/* Missing or invalid arguments,return FALSE*/` |
|    ! 0 | 3218 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 3219 | `		return PH7_OK;` |
|      - | 3220 | `	}` |
|      - | 3221 | `	/* Point to the hashmaps */` |
|      5 | 3222 | `	p1 = (ph7_hashmap *)apArg[0]->x.pOther;` |
|      5 | 3223 | `	p2 = (ph7_hashmap *)apArg[1]->x.pOther;` |
|      5 | 3224 | `	rc = (p1 == p2);` |
|      - | 3225 | `	/* Same instance? */` |
|      5 | 3226 | `	ph7_result_bool(pCtx,rc);` |
|      5 | 3227 | `	return PH7_OK;` |
|      3 | 3228 |  |
|      - | 3229 | `/*` |
|      - | 3230 | ` * array array_merge(array $array1,...)` |
|      - | 3231 | ` *  Merge one or more arrays.` |
|      - | 3232 | ` * Parameters` |
|      - | 3233 | ` *  $array1` |
|      - | 3234 | ` *    Initial array to merge.` |
|      - | 3235 | ` *  ...` |
|      - | 3236 | ` *   More array to merge.` |
|      - | 3237 | ` * Return` |
|      - | 3238 | ` *  The resulting array.` |
|      - | 3239 | ` */` |
|    770 | 3240 | `static int ph7_hashmap_merge(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      2 | 3241 |  |
|      - | 3242 | `	ph7_hashmap *pMap,*pSrc;` |
|      - | 3243 | `	ph7_value *pArray;` |
|      - | 3244 | `	int i;` |
|    772 | 3245 | `	if( nArg < 1 ){` |
|      - | 3246 | `		/* Missing arguments,return NULL */` |
|    ! 0 | 3247 | `		ph7_result_null(pCtx);` |
|    ! 0 | 3248 | `		return PH7_OK;` |
|      - | 3249 | `	}` |
|      - | 3250 | `	/* Create a new array */` |
|    772 | 3251 | `	pArray = ph7_context_new_array(pCtx);` |
|    772 | 3252 | `	if( pArray == 0 ){` |
|    ! 0 | 3253 | `		ph7_result_null(pCtx);` |
|    ! 0 | 3254 | `		return PH7_OK;` |
|      - | 3255 | `	}` |
|      - | 3256 | `	/* Point to the internal representation of the hashmap */` |
|    772 | 3257 | `	pMap = (ph7_hashmap *)pArray->x.pOther;` |
|      - | 3258 | `	/* Start merging */` |
|   2312 | 3259 | `	for( i = 0 ; i < nArg ; i++ ){` |
|      - | 3260 | `		/* Make sure we are dealing with a valid hashmap */` |
|   1542 | 3261 | `		if( !ph7_value_is_array(apArg[i]) ){` |
|      - | 3262 | `			/* Insert scalar value */` |
|      5 | 3263 | `			ph7_array_add_elem(pArray,0,apArg[i]);` |
|      3 | 3264 | `		}else{` |
|   1538 | 3265 | `			pSrc = (ph7_hashmap *)apArg[i]->x.pOther;` |
|      - | 3266 | `			/* Merge the two hashmaps */` |
|   1538 | 3267 | `			HashmapMerge(pSrc,pMap);` |
|      - | 3268 | `		}` |
|    772 | 3269 | `	}` |
|      - | 3270 | `	/* Return the freshly created array */` |
|    772 | 3271 | `	ph7_result_value(pCtx,pArray);` |
|    772 | 3272 | `	return PH7_OK;` |
|    387 | 3273 |  |
|      - | 3274 | `/*` |
|      - | 3275 | ` * array array_copy(array $source)` |
|      - | 3276 | ` *  Make a blind copy of the target array.` |
|      - | 3277 | ` * Parameters` |
|      - | 3278 | ` *  $source` |
|      - | 3279 | ` *   Target array` |
|      - | 3280 | ` * Return` |
|      - | 3281 | ` *  Copy of the target array on success.NULL otherwise.` |
|      - | 3282 | ` * Note` |
|      - | 3283 | ` *  This function is a symisc eXtension.` |
|      - | 3284 | ` */` |
|      2 | 3285 | `static int ph7_hashmap_copy(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 3286 |  |
|      - | 3287 | `	ph7_hashmap *pMap;` |
|      - | 3288 | `	ph7_value *pArray;` |
|      3 | 3289 | `	if( nArg < 1 ){` |
|      - | 3290 | `		/* Missing arguments,return NULL */` |
|    ! 0 | 3291 | `		ph7_result_null(pCtx);` |
|    ! 0 | 3292 | `		return PH7_OK;` |
|      - | 3293 | `	}` |
|      - | 3294 | `	/* Create a new array */` |
|      3 | 3295 | `	pArray = ph7_context_new_array(pCtx);` |
|      3 | 3296 | `	if( pArray == 0 ){` |
|    ! 0 | 3297 | `		ph7_result_null(pCtx);` |
|    ! 0 | 3298 | `		return PH7_OK;` |
|      - | 3299 | `	}` |
|      - | 3300 | `	/* Point to the internal representation of the hashmap */` |
|      3 | 3301 | `	pMap = (ph7_hashmap *)pArray->x.pOther;` |
|      3 | 3302 | `	if( ph7_value_is_array(apArg[0])){` |
|      - | 3303 | `		/* Point to the internal representation of the source */` |
|      3 | 3304 | `		ph7_hashmap *pSrc = (ph7_hashmap *)apArg[0]->x.pOther;` |
|      - | 3305 | `		/* Perform the copy */` |
|      3 | 3306 | `		PH7_HashmapDup(pSrc,pMap);` |
|      2 | 3307 | `	}else{` |
|      - | 3308 | `		/* Simple insertion */` |
|    ! 0 | 3309 | `		PH7_HashmapInsert(pMap,0/* Automatic index assign*/,apArg[0]);` |
|      - | 3310 | `	}` |
|      - | 3311 | `	/* Return the duplicated array */` |
|      3 | 3312 | `	ph7_result_value(pCtx,pArray);` |
|      3 | 3313 | `	return PH7_OK;` |
|      2 | 3314 |  |
|      - | 3315 | `/*` |
|      - | 3316 | ` * bool array_erase(array $source)` |
|      - | 3317 | ` *  Remove all elements from a given array.` |
|      - | 3318 | ` * Parameters` |
|      - | 3319 | ` *  $source` |
|      - | 3320 | ` *   Target array` |
|      - | 3321 | ` * Return` |
|      - | 3322 | ` *  TRUE on success.FALSE otherwise.` |
|      - | 3323 | ` * Note` |
|      - | 3324 | ` *  This function is a symisc eXtension.` |
|      - | 3325 | ` */` |
|      2 | 3326 | `static int ph7_hashmap_erase(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 3327 |  |
|      - | 3328 | `	ph7_hashmap *pMap;` |
|      3 | 3329 | `	if( nArg < 1 ){` |
|      - | 3330 | `		/* Missing arguments */` |
|    ! 0 | 3331 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 3332 | `		return PH7_OK;` |
|      - | 3333 | `	}` |
|      - | 3334 | `	/* Point to the target hashmap */` |
|      3 | 3335 | `	pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|      - | 3336 | `	/* Erase */` |
|      3 | 3337 | `	PH7_HashmapRelease(pMap,FALSE);` |
|      3 | 3338 | `	return PH7_OK;` |
|      2 | 3339 |  |
|      - | 3340 | `/*` |
|      - | 3341 | ` * array array_slice(array $array,int $offset [,int $length [, bool $preserve_keys = false ]])` |
|      - | 3342 | ` *  Extract a slice of the array.` |
|      - | 3343 | ` * Parameters` |
|      - | 3344 | ` *  $array` |
|      - | 3345 | ` *    The input array.` |
|      - | 3346 | ` * $offset` |
|      - | 3347 | ` *    If offset is non-negative, the sequence will start at that offset in the array.` |
|      - | 3348 | ` *    If offset is negative, the sequence will start that far from the end of the array.` |
|      - | 3349 | ` * $length (optional)` |
|      - | 3350 | ` *    If length is given and is positive, then the sequence will have that many elements` |
|      - | 3351 | ` *    in it. If length is given and is negative then the sequence will stop that many` |
|      - | 3352 | ` *   elements from the end of the array. If it is omitted, then the sequence will have` |
|      - | 3353 | ` *   everything from offset up until the end of the array.` |
|      - | 3354 | ` * $preserve_keys (optional)` |
|      - | 3355 | ` *    Note that array_slice() will reorder and reset the array indices by default.` |
|      - | 3356 | ` *    You can change this behaviour by setting preserve_keys to TRUE.` |
|      - | 3357 | ` * Return` |
|      - | 3358 | ` *   The new slice.` |
|      - | 3359 | ` */` |
|      8 | 3360 | `static int ph7_hashmap_slice(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 3361 |  |
|      - | 3362 | `	ph7_hashmap *pMap,*pSrc;` |
|      - | 3363 | `	ph7_hashmap_node *pCur;` |
|      - | 3364 | `	ph7_value *pArray;` |
|      - | 3365 | `	int iLength,iOfft;` |
|      - | 3366 | `	int bPreserve;` |
|      - | 3367 | `	sxi32 rc;` |
|      9 | 3368 | `	if( nArg < 2 \|\| !ph7_value_is_array(apArg[0]) ){` |
|      - | 3369 | `		/* Missing/Invalid arguments,return NULL */` |
|    ! 0 | 3370 | `		ph7_result_null(pCtx);` |
|    ! 0 | 3371 | `		return PH7_OK;` |
|      - | 3372 | `	}` |
|      - | 3373 | `	/* Point the internal representation of the target array */` |
|      9 | 3374 | `	pSrc = (ph7_hashmap *)apArg[0]->x.pOther;` |
|      9 | 3375 | `	bPreserve = FALSE;` |
|      - | 3376 | `	/* Get the offset */` |
|      9 | 3377 | `	iOfft = ph7_value_to_int(apArg[1]);` |
|      9 | 3378 | `	if( iOfft < 0 ){` |
|      3 | 3379 | `		iOfft = (int)pSrc->nEntry + iOfft;` |
|      1 | 3380 | `	}` |
|      9 | 3381 | `	if( iOfft < 0 \|\| iOfft > (int)pSrc->nEntry ){` |
|      - | 3382 | `		/* Invalid offset,return the last entry */` |
|    ! 0 | 3383 | `		iOfft = (int)pSrc->nEntry - 1;` |
|    ! 0 | 3384 | `	}` |
|      - | 3385 | `	/* Get the length */` |
|      9 | 3386 | `	iLength = (int)pSrc->nEntry - iOfft;` |
|      9 | 3387 | `	if( nArg > 2 ){` |
|      7 | 3388 | `		iLength = ph7_value_to_int(apArg[2]);` |
|      7 | 3389 | `		if( iLength < 0 ){` |
|    ! 0 | 3390 | `			iLength = ((int)pSrc->nEntry + iLength) - iOfft;` |
|    ! 0 | 3391 | `		}` |
|      7 | 3392 | `		if( iLength < 0 \|\| iOfft + iLength >= (int)pSrc->nEntry ){` |
|    ! 0 | 3393 | `			iLength = (int)pSrc->nEntry - iOfft;` |
|    ! 0 | 3394 | `		}` |
|      7 | 3395 | `		if( nArg > 3 && ph7_value_is_bool(apArg[3]) ){` |
|      3 | 3396 | `			bPreserve = ph7_value_to_bool(apArg[3]);` |
|      1 | 3397 | `		}` |
|      3 | 3398 | `	}` |
|      - | 3399 | `	/* Create a new array */` |
|      9 | 3400 | `	pArray = ph7_context_new_array(pCtx);` |
|      9 | 3401 | `	if( pArray == 0 ){` |
|    ! 0 | 3402 | `		ph7_result_null(pCtx);` |
|    ! 0 | 3403 | `		return PH7_OK;` |
|      - | 3404 | `	}` |
|      9 | 3405 | `	if( iLength < 1 ){` |
|      - | 3406 | `		/* Don't bother processing,return the empty array */` |
|    ! 0 | 3407 | `		ph7_result_value(pCtx,pArray);` |
|    ! 0 | 3408 | `		return PH7_OK;` |
|      - | 3409 | `	}` |
|      - | 3410 | `	/* Point to the desired entry */` |
|      9 | 3411 | `	pCur = pSrc->pFirst;` |
|      9 | 3412 | `	for(;;){` |
|     19 | 3413 | `		if( iOfft < 1 ){` |
|      9 | 3414 | `			break;` |
|      - | 3415 | `		}` |
|      - | 3416 | `		/* Point to the next entry */` |
|     11 | 3417 | `		pCur = pCur->pPrev; /* Reverse link */` |
|     11 | 3418 | `		iOfft--;` |
|      1 | 3419 | `	}` |
|      - | 3420 | `	/* Point to the internal representation of the hashmap */` |
|      9 | 3421 | `	pMap = (ph7_hashmap *)pArray->x.pOther;` |
|     12 | 3422 | `	for(;;){` |
|     25 | 3423 | `		if( iLength < 1 ){` |
|      9 | 3424 | `			break;` |
|      - | 3425 | `		}` |
|     17 | 3426 | `		rc = HashmapInsertNode(pMap,pCur,bPreserve);` |
|     17 | 3427 | `		if( rc != SXRET_OK ){` |
|    ! 0 | 3428 | `			break;` |
|      - | 3429 | `		}` |
|      - | 3430 | `		/* Point to the next entry */` |
|     17 | 3431 | `		pCur = pCur->pPrev; /* Reverse link */` |
|     17 | 3432 | `		iLength--;` |
|      1 | 3433 | `	}` |
|      - | 3434 | `	/* Return the freshly created array */` |
|      9 | 3435 | `	ph7_result_value(pCtx,pArray);` |
|      9 | 3436 | `	return PH7_OK;` |
|      5 | 3437 |  |
|      - | 3438 | `/*` |
|      - | 3439 | ` * array array_splice(array $array,int $offset [,int $length [,value $replacement ]])` |
|      - | 3440 | ` *  Remove a portion of the array and replace it with something else.` |
|      - | 3441 | ` * Parameters` |
|      - | 3442 | ` *  $array` |
|      - | 3443 | ` *    The input array.` |
|      - | 3444 | ` * $offset` |
|      - | 3445 | ` *    If offset is positive then the start of removed portion is at that offset from` |
|      - | 3446 | ` *    the beginning of the input array. If offset is negative then it starts that far` |
|      - | 3447 | ` *    from the end of the input array.` |
|      - | 3448 | ` * $length (optional)` |
|      - | 3449 | ` *    If length is omitted, removes everything from offset to the end of the array.` |
|      - | 3450 | ` *    If length is specified and is positive, then that many elements will be removed.` |
|      - | 3451 | ` *    If length is specified and is negative then the end of the removed portion will` |
|      - | 3452 | ` *    be that many elements from the end of the array.` |
|      - | 3453 | ` * $replacement (optional)` |
|      - | 3454 | ` *  If replacement array is specified, then the removed elements are replaced` |
|      - | 3455 | ` *  with elements from this array.` |
|      - | 3456 | ` *  If offset and length are such that nothing is removed, then the elements` |
|      - | 3457 | ` *  from the replacement array are inserted in the place specified by the offset.` |
|      - | 3458 | ` *  Note that keys in replacement array are not preserved.` |
|      - | 3459 | ` *  If replacement is just one element it is not necessary to put array() around` |
|      - | 3460 | ` *  it, unless the element is an array itself, an object or NULL.` |
|      - | 3461 | ` * Return` |
|      - | 3462 | ` *   A new array consisting of the extracted elements.` |
|      - | 3463 | ` */` |
|      2 | 3464 | `static int ph7_hashmap_splice(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 3465 |  |
|      - | 3466 | `	ph7_hashmap_node *pCur,*pPrev,*pRnode;` |
|      - | 3467 | `	ph7_value *pArray,*pRvalue,*pOld;` |
|      - | 3468 | `	ph7_hashmap *pMap,*pSrc,*pRep;` |
|      - | 3469 | `	int iLength,iOfft;` |
|      - | 3470 | `	sxi32 rc;` |
|      3 | 3471 | `	if( nArg < 2 \|\| !ph7_value_is_array(apArg[0]) ){` |
|      - | 3472 | `		/* Missing/Invalid arguments,return NULL */` |
|    ! 0 | 3473 | `		ph7_result_null(pCtx);` |
|    ! 0 | 3474 | `		return PH7_OK;` |
|      - | 3475 | `	}` |
|      - | 3476 | `	/* Point the internal representation of the target array */` |
|      3 | 3477 | `	pSrc = (ph7_hashmap *)apArg[0]->x.pOther;` |
|      - | 3478 | `	/* Get the offset */` |
|      3 | 3479 | `	iOfft = ph7_value_to_int(apArg[1]);` |
|      3 | 3480 | `	if( iOfft < 0 ){` |
|    ! 0 | 3481 | `		iOfft = (int)pSrc->nEntry + iOfft;` |
|    ! 0 | 3482 | `	}` |
|      3 | 3483 | `	if( iOfft < 0 \|\| iOfft > (int)pSrc->nEntry ){` |
|      - | 3484 | `		/* Invalid offset,remove the last entry */` |
|    ! 0 | 3485 | `		iOfft = (int)pSrc->nEntry - 1;` |
|    ! 0 | 3486 | `	}` |
|      - | 3487 | `	/* Get the length */` |
|      3 | 3488 | `	iLength = (int)pSrc->nEntry - iOfft;` |
|      3 | 3489 | `	if( nArg > 2 ){` |
|      3 | 3490 | `		iLength = ph7_value_to_int(apArg[2]);` |
|      3 | 3491 | `		if( iLength < 0 ){` |
|    ! 0 | 3492 | `			iLength = ((int)pSrc->nEntry + iLength) - iOfft;` |
|    ! 0 | 3493 | `		}` |
|      3 | 3494 | `		if( iLength < 0 \|\| iOfft + iLength >= (int)pSrc->nEntry ){` |
|    ! 0 | 3495 | `			iLength = (int)pSrc->nEntry - iOfft;` |
|    ! 0 | 3496 | `		}` |
|      1 | 3497 | `	}` |
|      - | 3498 | `	/* Create a new array */` |
|      3 | 3499 | `	pArray = ph7_context_new_array(pCtx);` |
|      3 | 3500 | `	if( pArray == 0 ){` |
|    ! 0 | 3501 | `		ph7_result_null(pCtx);` |
|    ! 0 | 3502 | `		return PH7_OK;` |
|      - | 3503 | `	}` |
|      3 | 3504 | `	if( iLength < 1 ){` |
|      - | 3505 | `		/* Don't bother processing,return the empty array */` |
|    ! 0 | 3506 | `		ph7_result_value(pCtx,pArray);` |
|    ! 0 | 3507 | `		return PH7_OK;` |
|      - | 3508 | `	}` |
|      - | 3509 | `	/* Point to the desired entry */` |
|      3 | 3510 | `	pCur = pSrc->pFirst;` |
|      2 | 3511 | `	for(;;){` |
|      5 | 3512 | `		if( iOfft < 1 ){` |
|      3 | 3513 | `			break;` |
|      - | 3514 | `		}` |
|      - | 3515 | `		/* Point to the next entry */` |
|      3 | 3516 | `		pCur = pCur->pPrev; /* Reverse link */` |
|      3 | 3517 | `		iOfft--;` |
|      1 | 3518 | `	}` |
|      3 | 3519 | `	pRep = 0;` |
|      3 | 3520 | `	if( nArg > 3 ){` |
|      3 | 3521 | `		if( !ph7_value_is_array(apArg[3]) ){` |
|      - | 3522 | `			/* Perform an array cast */` |
|    ! 0 | 3523 | `			PH7_MemObjToHashmap(apArg[3]);` |
|    ! 0 | 3524 | `			if(ph7_value_is_array(apArg[3])){` |
|    ! 0 | 3525 | `				pRep = (ph7_hashmap *)apArg[3]->x.pOther;` |
|    ! 0 | 3526 | `			}` |
|    ! 0 | 3527 | `		}else{` |
|      3 | 3528 | `			pRep = (ph7_hashmap *)apArg[3]->x.pOther;` |
|      - | 3529 | `		}` |
|      3 | 3530 | `		if( pRep ){` |
|      - | 3531 | `			/* Reset the loop cursor */` |
|      3 | 3532 | `			pRep->pCur = pRep->pFirst;` |
|      1 | 3533 | `		}` |
|      1 | 3534 | `	}` |
|      - | 3535 | `	/* Point to the internal representation of the hashmap */` |
|      3 | 3536 | `	pMap = (ph7_hashmap *)pArray->x.pOther;` |
|      3 | 3537 | `	for(;;){` |
|      7 | 3538 | `		if( iLength < 1 ){` |
|      3 | 3539 | `			break;` |
|      - | 3540 | `		}` |
|      5 | 3541 | `		pPrev = pCur->pPrev;` |
|      5 | 3542 | `		rc = HashmapInsertNode(pMap,pCur,FALSE);` |
|      5 | 3543 | `		if( pRep && (pRnode = PH7_HashmapGetNextEntry(pRep)) != 0 ){` |
|      - | 3544 | `			/* Extract node value */` |
|      5 | 3545 | `			pRvalue = HashmapExtractNodeValue(pRnode);` |
|      - | 3546 | `			/* Replace the old node */` |
|      5 | 3547 | `			pOld = HashmapExtractNodeValue(pCur);` |
|      5 | 3548 | `			if( pRvalue && pOld ){` |
|      5 | 3549 | `				PH7_MemObjStore(pRvalue,pOld);` |
|      2 | 3550 | `			}` |
|      3 | 3551 | `		}else{` |
|      - | 3552 | `			/* Unlink the node from the source hashmap */` |
|    ! 0 | 3553 | `			PH7_HashmapUnlinkNode(pCur,TRUE);` |
|      - | 3554 | `		}` |
|      5 | 3555 | `		if( rc != SXRET_OK ){` |
|    ! 0 | 3556 | `			break;` |
|      - | 3557 | `		}` |
|      - | 3558 | `		/* Point to the next entry */` |
|      5 | 3559 | `		pCur = pPrev; /* Reverse link */` |
|      5 | 3560 | `		iLength--;` |
|      1 | 3561 | `	}` |
|      3 | 3562 | `	if( pRep ){` |
|      3 | 3563 | `		while((pRnode = PH7_HashmapGetNextEntry(pRep)) != 0 ){` |
|    ! 0 | 3564 | `			HashmapInsertNode(pSrc,pRnode,FALSE);` |
|    ! 0 | 3565 | `		}` |
|      1 | 3566 | `	}` |
|      - | 3567 | `	/* Return the freshly created array */` |
|      3 | 3568 | `	ph7_result_value(pCtx,pArray);` |
|      3 | 3569 | `	return PH7_OK;` |
|      2 | 3570 |  |
|      - | 3571 | `/*` |
|      - | 3572 | ` * bool in_array(value $needle,array $haystack[,bool $strict = FALSE ])` |
|      - | 3573 | ` *  Checks if a value exists in an array.` |
|      - | 3574 | ` * Parameters` |
|      - | 3575 | ` *  $needle` |
|      - | 3576 | ` *   The searched value.` |
|      - | 3577 | ` *   Note:` |
|      - | 3578 | ` *    If needle is a string, the comparison is done in a case-sensitive manner.` |
|      - | 3579 | ` * $haystack` |
|      - | 3580 | ` *  The target array.` |
|      - | 3581 | ` * $strict` |
|      - | 3582 | ` *  If the third parameter strict is set to TRUE then the in_array() function` |
|      - | 3583 | ` *  will also check the types of the needle in the haystack.` |
|      - | 3584 | ` */` |
|  17522 | 3585 | `static int ph7_hashmap_in_array(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      2 | 3586 |  |
|      - | 3587 | `	ph7_value *pNeedle;` |
|      - | 3588 | `	int bStrict;` |
|      - | 3589 | `	int rc;` |
|  17524 | 3590 | `	if( nArg < 2 ){` |
|      - | 3591 | `		/* Missing argument,return FALSE */` |
|    ! 0 | 3592 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 3593 | `		return PH7_OK;` |
|      - | 3594 | `	}` |
|  17524 | 3595 | `	pNeedle = apArg[0];` |
|  17524 | 3596 | `	bStrict = 0;` |
|  17524 | 3597 | `	if( nArg > 2 ){` |
|      5 | 3598 | `		bStrict = ph7_value_to_bool(apArg[2]);` |
|      2 | 3599 | `	}` |
|  17524 | 3600 | `	if( !ph7_value_is_array(apArg[1]) ){` |
|      - | 3601 | `		/* haystack must be an array,perform a standard comparison */` |
|    ! 0 | 3602 | `		rc = ph7_value_compare(pNeedle,apArg[1],bStrict);` |
|      - | 3603 | `		/* Set the comparison result */` |
|    ! 0 | 3604 | `		ph7_result_bool(pCtx,rc == 0);` |
|    ! 0 | 3605 | `		return PH7_OK;` |
|      - | 3606 | `	}` |
|      - | 3607 | `	/* Perform the lookup */` |
|  17524 | 3608 | `	rc = HashmapFindValue((ph7_hashmap *)apArg[1]->x.pOther,pNeedle,0,bStrict);` |
|      - | 3609 | `	/* Lookup result */` |
|  17524 | 3610 | `	ph7_result_bool(pCtx,rc == SXRET_OK);` |
|  17524 | 3611 | `	return PH7_OK;` |
|   8763 | 3612 |  |
|      - | 3613 | `/*` |
|      - | 3614 | ` * value array_search(value $needle,array $haystack[,bool $strict = false ])` |
|      - | 3615 | ` *  Searches the array for a given value and returns the corresponding key if successful.` |
|      - | 3616 | ` * Parameters` |
|      - | 3617 | ` * $needle` |
|      - | 3618 | ` *   The searched value.` |
|      - | 3619 | ` * $haystack` |
|      - | 3620 | ` *   The array.` |
|      - | 3621 | ` * $strict` |
|      - | 3622 | ` *  If the third parameter strict is set to TRUE then the array_search() function` |
|      - | 3623 | ` *  will search for identical elements in the haystack. This means it will also check` |
|      - | 3624 | ` *  the types of the needle in the haystack, and objects must be the same instance.` |
|      - | 3625 | ` * Return` |
|      - | 3626 | ` *  Returns the key for needle if it is found in the array, FALSE otherwise.` |
|      - | 3627 | ` */` |
|     26 | 3628 | `static int ph7_hashmap_search(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 3629 |  |
|      - | 3630 | `	ph7_hashmap_node *pEntry;` |
|      - | 3631 | `	ph7_value *pVal,sNeedle;` |
|      - | 3632 | `	ph7_hashmap *pMap;` |
|      - | 3633 | `	ph7_value sVal;` |
|      - | 3634 | `	int bStrict;` |
|      - | 3635 | `	sxu32 n;` |
|      - | 3636 | `	int rc;` |
|     27 | 3637 | `	if( nArg < 2 ){` |
|      - | 3638 | `		/* Missing argument,return FALSE*/` |
|    ! 0 | 3639 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 3640 | `		return PH7_OK;` |
|      - | 3641 | `	}` |
|     27 | 3642 | `	bStrict = FALSE;` |
|     27 | 3643 | `	if( !ph7_value_is_array(apArg[1]) ){` |
|      - | 3644 | `		/* hasystack must be an array,return FALSE */` |
|      3 | 3645 | `		ph7_result_bool(pCtx,0);` |
|      3 | 3646 | `		return PH7_OK;` |
|      - | 3647 | `	}` |
|     25 | 3648 | `	if( nArg > 2 && ph7_value_is_bool(apArg[2]) ){` |
|     19 | 3649 | `		bStrict = ph7_value_to_bool(apArg[2]);` |
|      9 | 3650 | `	}` |
|      - | 3651 | `	/* Point to the internal representation of the internal hashmap */` |
|     25 | 3652 | `	pMap = (ph7_hashmap *)apArg[1]->x.pOther;` |
|      - | 3653 | `	/* Perform a linear search since we cannot sort the hashmap based on values */` |
|     25 | 3654 | `	PH7_MemObjInit(pMap->pVm,&sVal);` |
|     25 | 3655 | `	PH7_MemObjInit(pMap->pVm,&sNeedle);` |
|     25 | 3656 | `	pEntry = pMap->pFirst;` |
|     25 | 3657 | `	n = pMap->nEntry;` |
|     39 | 3658 | `	for(;;){` |
|     79 | 3659 | `		if( !n ){` |
|      7 | 3660 | `			break;` |
|      - | 3661 | `		}` |
|      - | 3662 | `		/* Extract node value */` |
|     73 | 3663 | `		pVal = HashmapExtractNodeValue(pEntry);` |
|     73 | 3664 | `		if( pVal ){` |
|      - | 3665 | `			/* Make a copy of the vuurent values since the comparison routine` |
|      - | 3666 | `			 * can change their type.` |
|      - | 3667 | `			 */` |
|     73 | 3668 | `			PH7_MemObjLoad(pVal,&sVal);` |
|     73 | 3669 | `			PH7_MemObjLoad(apArg[0],&sNeedle);` |
|     73 | 3670 | `			rc = PH7_MemObjCmp(&sNeedle,&sVal,bStrict,0);` |
|     73 | 3671 | `			PH7_MemObjRelease(&sVal);` |
|     73 | 3672 | `			PH7_MemObjRelease(&sNeedle);` |
|     73 | 3673 | `			if( rc == 0 ){` |
|      - | 3674 | `				/* Match found,return key */` |
|     19 | 3675 | `				if( pEntry->iType == HASHMAP_INT_NODE){` |
|      - | 3676 | `					/* INT key */` |
|     13 | 3677 | `					ph7_result_int64(pCtx,pEntry->xKey.iKey);` |
|      7 | 3678 | `				}else{` |
|      7 | 3679 | `					SyBlob *pKey = &pEntry->xKey.sKey;` |
|      - | 3680 | `					/* Blob key */` |
|      7 | 3681 | `					ph7_result_string(pCtx,(const char *)SyBlobData(pKey),(int)SyBlobLength(pKey));` |
|      - | 3682 | `				}` |
|     19 | 3683 | `				return PH7_OK;` |
|      - | 3684 | `			}` |
|     27 | 3685 | `		}` |
|      - | 3686 | `		/* Point to the next entry */` |
|     55 | 3687 | `		pEntry = pEntry->pPrev; /* Reverse link */` |
|     55 | 3688 | `		n--;` |
|      1 | 3689 | `	}` |
|      - | 3690 | `	/* No such value,return FALSE */` |
|      7 | 3691 | `	ph7_result_bool(pCtx,0);` |
|      7 | 3692 | `	return PH7_OK;` |
|     14 | 3693 |  |
|      - | 3694 | `/*` |
|      - | 3695 | ` * array array_diff(array $array1,array $array2,...)` |
|      - | 3696 | ` *  Computes the difference of arrays.` |
|      - | 3697 | ` * Parameters` |
|      - | 3698 | ` *  $array1` |
|      - | 3699 | ` *    The array to compare from` |
|      - | 3700 | ` *  $array2` |
|      - | 3701 | ` *    An array to compare against` |
|      - | 3702 | ` *  $...` |
|      - | 3703 | ` *   More arrays to compare against` |
|      - | 3704 | ` * Return` |
|      - | 3705 | ` *  Returns an array containing all the entries from array1 that` |
|      - | 3706 | ` *  are not present in any of the other arrays.` |
|      - | 3707 | ` */` |
|      2 | 3708 | `static int ph7_hashmap_diff(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 3709 |  |
|      - | 3710 | `	ph7_hashmap_node *pEntry;` |
|      - | 3711 | `	ph7_hashmap *pSrc,*pMap;` |
|      - | 3712 | `	ph7_value *pArray;` |
|      - | 3713 | `	ph7_value *pVal;` |
|      - | 3714 | `	sxi32 rc;` |
|      - | 3715 | `	sxu32 n;` |
|      - | 3716 | `	int i;` |
|      3 | 3717 | `	if( nArg < 1 \|\| !ph7_value_is_array(apArg[0]) ){` |
|      - | 3718 | `		/* Missing arguments,return NULL */` |
|    ! 0 | 3719 | `		ph7_result_null(pCtx);` |
|    ! 0 | 3720 | `		return PH7_OK;` |
|      - | 3721 | `	}` |
|      3 | 3722 | `	if( nArg == 1 ){` |
|      - | 3723 | `		/* Return the first array since we cannot perform a diff */` |
|    ! 0 | 3724 | `		ph7_result_value(pCtx,apArg[0]);` |
|    ! 0 | 3725 | `		return PH7_OK;` |
|      - | 3726 | `	}` |
|      - | 3727 | `	/* Create a new array */` |
|      3 | 3728 | `	pArray = ph7_context_new_array(pCtx);` |
|      3 | 3729 | `	if( pArray == 0 ){` |
|    ! 0 | 3730 | `		ph7_result_null(pCtx);` |
|    ! 0 | 3731 | `		return PH7_OK;` |
|      - | 3732 | `	}` |
|      - | 3733 | `	/* Point to the internal representation of the source hashmap */` |
|      3 | 3734 | `	pSrc = (ph7_hashmap *)apArg[0]->x.pOther;` |
|      - | 3735 | `	/* Perform the diff */` |
|      3 | 3736 | `	pEntry = pSrc->pFirst;` |
|      3 | 3737 | `	n = pSrc->nEntry;` |
|      4 | 3738 | `	for(;;){` |
|      9 | 3739 | `		if( n < 1 ){` |
|      3 | 3740 | `			break;` |
|      - | 3741 | `		}` |
|      - | 3742 | `		/* Extract the node value */` |
|      7 | 3743 | `		pVal = HashmapExtractNodeValue(pEntry);` |
|      7 | 3744 | `		if( pVal ){` |
|     11 | 3745 | `			for( i = 1 ; i < nArg ; i++ ){` |
|      7 | 3746 | `				if( !ph7_value_is_array(apArg[i])) {` |
|      - | 3747 | `					/* ignore */` |
|    ! 0 | 3748 | `					continue;` |
|      - | 3749 | `				}` |
|      - | 3750 | `				/* Point to the internal representation of the hashmap */` |
|      7 | 3751 | `				pMap = (ph7_hashmap *)apArg[i]->x.pOther;` |
|      - | 3752 | `				/* Perform the lookup */` |
|      7 | 3753 | `				rc = HashmapFindValue(pMap,pVal,0,TRUE);` |
|      7 | 3754 | `				if( rc == SXRET_OK ){` |
|      - | 3755 | `					/* Value exist */` |
|      3 | 3756 | `					break;` |
|      - | 3757 | `				}` |
|      3 | 3758 | `			}` |
|      7 | 3759 | `			if( i >= nArg ){` |
|      - | 3760 | `				/* Perform the insertion */` |
|      5 | 3761 | `				HashmapInsertNode((ph7_hashmap *)pArray->x.pOther,pEntry,TRUE);` |
|      2 | 3762 | `			}` |
|      3 | 3763 | `		}` |
|      - | 3764 | `		/* Point to the next entry */` |
|      7 | 3765 | `		pEntry = pEntry->pPrev; /* Reverse link */` |
|      7 | 3766 | `		n--;` |
|      1 | 3767 | `	}` |
|      - | 3768 | `	/* Return the freshly created array */` |
|      3 | 3769 | `	ph7_result_value(pCtx,pArray);` |
|      3 | 3770 | `	return PH7_OK;` |
|      2 | 3771 |  |
|      - | 3772 | `/*` |
|      - | 3773 | ` * array array_udiff(array $array1,array $array2,...,$callback)` |
|      - | 3774 | ` *  Computes the difference of arrays by using a callback function for data comparison.` |
|      - | 3775 | ` * Parameters` |
|      - | 3776 | ` *  $array1` |
|      - | 3777 | ` *    The array to compare from` |
|      - | 3778 | ` *  $array2` |
|      - | 3779 | ` *    An array to compare against` |
|      - | 3780 | ` *  $...` |
|      - | 3781 | ` *   More arrays to compare against.` |
|      - | 3782 | ` * $callback` |
|      - | 3783 | ` *  The callback comparison function.` |
|      - | 3784 | ` *  The comparison function must return an integer less than, equal to, or greater than zero` |
|      - | 3785 | ` *  if the first argument is considered to be respectively less than, equal to, or greater` |
|      - | 3786 | ` *  than the second.` |
|      - | 3787 | ` *     int callback ( mixed $a, mixed $b )` |
|      - | 3788 | ` * Return` |
|      - | 3789 | ` *  Returns an array containing all the entries from array1 that` |
|      - | 3790 | ` *  are not present in any of the other arrays.` |
|      - | 3791 | ` */` |
|      2 | 3792 | `static int ph7_hashmap_udiff(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 3793 |  |
|      - | 3794 | `	ph7_hashmap_node *pEntry;` |
|      - | 3795 | `	ph7_hashmap *pSrc,*pMap;` |
|      - | 3796 | `	ph7_value *pCallback;` |
|      - | 3797 | `	ph7_value *pArray;` |
|      - | 3798 | `	ph7_value *pVal;` |
|      - | 3799 | `	sxi32 rc;` |
|      - | 3800 | `	sxu32 n;` |
|      - | 3801 | `	int i;` |
|      3 | 3802 | `	if( nArg < 2 \|\| !ph7_value_is_array(apArg[0]) ){` |
|      - | 3803 | `		/* Missing/Invalid arguments,return NULL */` |
|    ! 0 | 3804 | `		ph7_result_null(pCtx);` |
|    ! 0 | 3805 | `		return PH7_OK;` |
|      - | 3806 | `	}` |
|      - | 3807 | `	/* Point to the callback */` |
|      3 | 3808 | `	pCallback = apArg[nArg - 1];` |
|      3 | 3809 | `	if( nArg == 2 ){` |
|      - | 3810 | `		/* Return the first array since we cannot perform a diff */` |
|    ! 0 | 3811 | `		ph7_result_value(pCtx,apArg[0]);` |
|    ! 0 | 3812 | `		return PH7_OK;` |
|      - | 3813 | `	}` |
|      - | 3814 | `	/* Create a new array */` |
|      3 | 3815 | `	pArray = ph7_context_new_array(pCtx);` |
|      3 | 3816 | `	if( pArray == 0 ){` |
|    ! 0 | 3817 | `		ph7_result_null(pCtx);` |
|    ! 0 | 3818 | `		return PH7_OK;` |
|      - | 3819 | `	}` |
|      - | 3820 | `	/* Point to the internal representation of the source hashmap */` |
|      3 | 3821 | `	pSrc = (ph7_hashmap *)apArg[0]->x.pOther;` |
|      - | 3822 | `	/* Perform the diff */` |
|      3 | 3823 | `	pEntry = pSrc->pFirst;` |
|      3 | 3824 | `	n = pSrc->nEntry;` |
|      4 | 3825 | `	for(;;){` |
|      9 | 3826 | `		if( n < 1 ){` |
|      3 | 3827 | `			break;` |
|      - | 3828 | `		}` |
|      - | 3829 | `		/* Extract the node value */` |
|      7 | 3830 | `		pVal = HashmapExtractNodeValue(pEntry);` |
|      7 | 3831 | `		if( pVal ){` |
|     11 | 3832 | `			for( i = 1 ; i < nArg - 1; i++ ){` |
|      7 | 3833 | `				if( !ph7_value_is_array(apArg[i])) {` |
|      - | 3834 | `					/* ignore */` |
|    ! 0 | 3835 | `					continue;` |
|      - | 3836 | `				}` |
|      - | 3837 | `				/* Point to the internal representation of the hashmap */` |
|      7 | 3838 | `				pMap = (ph7_hashmap *)apArg[i]->x.pOther;` |
|      - | 3839 | `				/* Perform the lookup */` |
|      7 | 3840 | `				rc = HashmapFindValueByCallback(pMap,pVal,pCallback,0);` |
|      7 | 3841 | `				if( rc == SXRET_OK ){` |
|      - | 3842 | `					/* Value exist */` |
|      3 | 3843 | `					break;` |
|      - | 3844 | `				}` |
|      3 | 3845 | `			}` |
|      7 | 3846 | `			if( i >= (nArg - 1)){` |
|      - | 3847 | `				/* Perform the insertion */` |
|      5 | 3848 | `				HashmapInsertNode((ph7_hashmap *)pArray->x.pOther,pEntry,TRUE);` |
|      2 | 3849 | `			}` |
|      3 | 3850 | `		}` |
|      - | 3851 | `		/* Point to the next entry */` |
|      7 | 3852 | `		pEntry = pEntry->pPrev; /* Reverse link */` |
|      7 | 3853 | `		n--;` |
|      1 | 3854 | `	}` |
|      - | 3855 | `	/* Return the freshly created array */` |
|      3 | 3856 | `	ph7_result_value(pCtx,pArray);` |
|      3 | 3857 | `	return PH7_OK;` |
|      2 | 3858 |  |
|      - | 3859 | `/*` |
|      - | 3860 | ` * array array_diff_assoc(array $array1,array $array2,...)` |
|      - | 3861 | ` *  Computes the difference of arrays with additional index check.` |
|      - | 3862 | ` * Parameters` |
|      - | 3863 | ` *  $array1` |
|      - | 3864 | ` *    The array to compare from` |
|      - | 3865 | ` *  $array2` |
|      - | 3866 | ` *    An array to compare against` |
|      - | 3867 | ` *  $...` |
|      - | 3868 | ` *   More arrays to compare against` |
|      - | 3869 | ` * Return` |
|      - | 3870 | ` *  Returns an array containing all the entries from array1 that` |
|      - | 3871 | ` *  are not present in any of the other arrays.` |
|      - | 3872 | ` */` |
|      2 | 3873 | `static int ph7_hashmap_diff_assoc(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 3874 |  |
|      - | 3875 | `	ph7_hashmap_node *pN1,*pN2,*pEntry;` |
|      - | 3876 | `	ph7_hashmap *pSrc,*pMap;` |
|      - | 3877 | `	ph7_value *pArray;` |
|      - | 3878 | `	ph7_value *pVal;` |
|      - | 3879 | `	sxi32 rc;` |
|      - | 3880 | `	sxu32 n;` |
|      - | 3881 | `	int i;` |
|      3 | 3882 | `	if( nArg < 1 \|\| !ph7_value_is_array(apArg[0]) ){` |
|      - | 3883 | `		/* Missing arguments,return NULL */` |
|    ! 0 | 3884 | `		ph7_result_null(pCtx);` |
|    ! 0 | 3885 | `		return PH7_OK;` |
|      - | 3886 | `	}` |
|      3 | 3887 | `	if( nArg == 1 ){` |
|      - | 3888 | `		/* Return the first array since we cannot perform a diff */` |
|    ! 0 | 3889 | `		ph7_result_value(pCtx,apArg[0]);` |
|    ! 0 | 3890 | `		return PH7_OK;` |
|      - | 3891 | `	}` |
|      - | 3892 | `	/* Create a new array */` |
|      3 | 3893 | `	pArray = ph7_context_new_array(pCtx);` |
|      3 | 3894 | `	if( pArray == 0 ){` |
|    ! 0 | 3895 | `		ph7_result_null(pCtx);` |
|    ! 0 | 3896 | `		return PH7_OK;` |
|      - | 3897 | `	}` |
|      - | 3898 | `	/* Point to the internal representation of the source hashmap */` |
|      3 | 3899 | `	pSrc = (ph7_hashmap *)apArg[0]->x.pOther;` |
|      - | 3900 | `	/* Perform the diff */` |
|      3 | 3901 | `	pEntry = pSrc->pFirst;` |
|      3 | 3902 | `	n = pSrc->nEntry;` |
|      3 | 3903 | `	pN1 = pN2 = 0;` |
|      3 | 3904 | `	for(;;){` |
|      7 | 3905 | `		if( n < 1 ){` |
|      3 | 3906 | `			break;` |
|      - | 3907 | `		}` |
|      7 | 3908 | `		for( i = 1 ; i < nArg ; i++ ){` |
|      5 | 3909 | `			if( !ph7_value_is_array(apArg[i])) {` |
|      - | 3910 | `				/* ignore */` |
|    ! 0 | 3911 | `				continue;` |
|      - | 3912 | `			}` |
|      - | 3913 | `			/* Point to the internal representation of the hashmap */` |
|      5 | 3914 | `			pMap = (ph7_hashmap *)apArg[i]->x.pOther;` |
|      - | 3915 | `			/* Perform a key lookup first */` |
|      5 | 3916 | `			if( pEntry->iType == HASHMAP_INT_NODE ){` |
|    ! 0 | 3917 | `				rc = HashmapLookupIntKey(pMap,pEntry->xKey.iKey,&pN1);` |
|    ! 0 | 3918 | `			}else{` |
|      5 | 3919 | `				rc = HashmapLookupBlobKey(pMap,SyBlobData(&pEntry->xKey.sKey),SyBlobLength(&pEntry->xKey.sKey),&pN1);` |
|      - | 3920 | `			}` |
|      5 | 3921 | `			if( rc != SXRET_OK ){` |
|      - | 3922 | `				/* No such key,break immediately */` |
|      3 | 3923 | `				break;` |
|      - | 3924 | `			}` |
|      - | 3925 | `			/* Extract node value */` |
|      3 | 3926 | `			pVal = HashmapExtractNodeValue(pEntry);` |
|      3 | 3927 | `			if( pVal ){` |
|      - | 3928 | `				/* Perform the lookup */` |
|      3 | 3929 | `				rc = HashmapFindValue(pMap,pVal,&pN2,TRUE);` |
|      3 | 3930 | `				if( rc != SXRET_OK \|\| pN1 != pN2 ){` |
|      - | 3931 | `					/* Value does not exist */` |
|    ! 0 | 3932 | `					break;` |
|      - | 3933 | `				}` |
|      1 | 3934 | `			}` |
|      2 | 3935 | `		}` |
|      5 | 3936 | `		if( i < nArg ){` |
|      - | 3937 | `			/* Perform the insertion */` |
|      3 | 3938 | `			HashmapInsertNode((ph7_hashmap *)pArray->x.pOther,pEntry,TRUE);` |
|      1 | 3939 | `		}` |
|      - | 3940 | `		/* Point to the next entry */` |
|      5 | 3941 | `		pEntry = pEntry->pPrev; /* Reverse link */` |
|      5 | 3942 | `		n--;` |
|      1 | 3943 | `	}` |
|      - | 3944 | `	/* Return the freshly created array */` |
|      3 | 3945 | `	ph7_result_value(pCtx,pArray);` |
|      3 | 3946 | `	return PH7_OK;` |
|      2 | 3947 |  |
|      - | 3948 | `/*` |
|      - | 3949 | ` * array array_diff_uassoc(array $array1,array $array2,...,callback $key_compare_func)` |
|      - | 3950 | ` *  Computes the difference of arrays with additional index check which is performed` |
|      - | 3951 | ` *  by a user supplied callback function.` |
|      - | 3952 | ` * Parameters` |
|      - | 3953 | ` *  $array1` |
|      - | 3954 | ` *    The array to compare from` |
|      - | 3955 | ` *  $array2` |
|      - | 3956 | ` *    An array to compare against` |
|      - | 3957 | ` *  $...` |
|      - | 3958 | ` *   More arrays to compare against.` |
|      - | 3959 | ` *  $key_compare_func` |
|      - | 3960 | ` *   Callback function to use. The callback function must return an integer` |
|      - | 3961 | ` *   less than, equal to, or greater than zero if the first argument is considered` |
|      - | 3962 | ` *   to be respectively less than, equal to, or greater than the second.` |
|      - | 3963 | ` * Return` |
|      - | 3964 | ` *  Returns an array containing all the entries from array1 that` |
|      - | 3965 | ` *  are not present in any of the other arrays.` |
|      - | 3966 | ` */` |
|      2 | 3967 | `static int ph7_hashmap_diff_uassoc(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 3968 |  |
|      - | 3969 | `	ph7_hashmap_node *pN1,*pN2,*pEntry;` |
|      - | 3970 | `	ph7_hashmap *pSrc,*pMap;` |
|      - | 3971 | `	ph7_value *pCallback;` |
|      - | 3972 | `	ph7_value *pArray;` |
|      - | 3973 | `	ph7_value *pVal;` |
|      - | 3974 | `	sxi32 rc;` |
|      - | 3975 | `	sxu32 n;` |
|      - | 3976 | `	int i;` |
|      - | 3977 |  |
|      3 | 3978 | `	if( nArg < 2 \|\| !ph7_value_is_array(apArg[0]) ){` |
|      - | 3979 | `		/* Missing/Invalid arguments,return NULL */` |
|    ! 0 | 3980 | `		ph7_result_null(pCtx);` |
|    ! 0 | 3981 | `		return PH7_OK;` |
|      - | 3982 | `	}` |
|      - | 3983 | `	/* Point to the callback */` |
|      3 | 3984 | `	pCallback = apArg[nArg - 1];` |
|      3 | 3985 | `	if( nArg == 2 ){` |
|      - | 3986 | `		/* Return the first array since we cannot perform a diff */` |
|    ! 0 | 3987 | `		ph7_result_value(pCtx,apArg[0]);` |
|    ! 0 | 3988 | `		return PH7_OK;` |
|      - | 3989 | `	}` |
|      - | 3990 | `	/* Create a new array */` |
|      3 | 3991 | `	pArray = ph7_context_new_array(pCtx);` |
|      3 | 3992 | `	if( pArray == 0 ){` |
|    ! 0 | 3993 | `		ph7_result_null(pCtx);` |
|    ! 0 | 3994 | `		return PH7_OK;` |
|      - | 3995 | `	}` |
|      - | 3996 | `	/* Point to the internal representation of the source hashmap */` |
|      3 | 3997 | `	pSrc = (ph7_hashmap *)apArg[0]->x.pOther;` |
|      - | 3998 | `	/* Perform the diff */` |
|      3 | 3999 | `	pEntry = pSrc->pFirst;` |
|      3 | 4000 | `	n = pSrc->nEntry;` |
|      3 | 4001 | `	pN1 = pN2 = 0; /* cc warning */` |
|      4 | 4002 | `	for(;;){` |
|      9 | 4003 | `		if( n < 1 ){` |
|      3 | 4004 | `			break;` |
|      - | 4005 | `		}` |
|      9 | 4006 | `		for( i = 1 ; i < nArg - 1; i++ ){` |
|      7 | 4007 | `			if( !ph7_value_is_array(apArg[i])) {` |
|      - | 4008 | `				/* ignore */` |
|    ! 0 | 4009 | `				continue;` |
|      - | 4010 | `			}` |
|      - | 4011 | `			/* Point to the internal representation of the hashmap */` |
|      7 | 4012 | `			pMap = (ph7_hashmap *)apArg[i]->x.pOther;` |
|      - | 4013 | `			/* Perform a key lookup first */` |
|      7 | 4014 | `			if( pEntry->iType == HASHMAP_INT_NODE ){` |
|    ! 0 | 4015 | `				rc = HashmapLookupIntKey(pMap,pEntry->xKey.iKey,&pN1);` |
|    ! 0 | 4016 | `			}else{` |
|      7 | 4017 | `				rc = HashmapLookupBlobKey(pMap,SyBlobData(&pEntry->xKey.sKey),SyBlobLength(&pEntry->xKey.sKey),&pN1);` |
|      - | 4018 | `			}` |
|      7 | 4019 | `			if( rc != SXRET_OK ){` |
|      - | 4020 | `				/* No such key,break immediately */` |
|      3 | 4021 | `				break;` |
|      - | 4022 | `			}` |
|      - | 4023 | `			/* Extract node value */` |
|      5 | 4024 | `			pVal = HashmapExtractNodeValue(pEntry);` |
|      5 | 4025 | `			if( pVal ){` |
|      - | 4026 | `				/* Invoke the user callback */` |
|      5 | 4027 | `				rc = HashmapFindValueByCallback(pMap,pVal,pCallback,&pN2);` |
|      5 | 4028 | `				if( rc != SXRET_OK \|\| pN1 != pN2 ){` |
|      - | 4029 | `					/* Value does not exist */` |
|      2 | 4030 | `					break;` |
|      - | 4031 | `				}` |
|      1 | 4032 | `			}` |
|      2 | 4033 | `		}` |
|      7 | 4034 | `		if( i < (nArg-1) ){` |
|      - | 4035 | `			/* Perform the insertion */` |
|      5 | 4036 | `			HashmapInsertNode((ph7_hashmap *)pArray->x.pOther,pEntry,TRUE);` |
|      2 | 4037 | `		}` |
|      - | 4038 | `		/* Point to the next entry */` |
|      7 | 4039 | `		pEntry = pEntry->pPrev; /* Reverse link */` |
|      7 | 4040 | `		n--;` |
|      1 | 4041 | `	}` |
|      - | 4042 | `	/* Return the freshly created array */` |
|      3 | 4043 | `	ph7_result_value(pCtx,pArray);` |
|      3 | 4044 | `	return PH7_OK;` |
|      2 | 4045 |  |
|      - | 4046 | `/*` |
|      - | 4047 | ` * array array_diff_key(array $array1 ,array $array2,...)` |
|      - | 4048 | ` *  Computes the difference of arrays using keys for comparison.` |
|      - | 4049 | ` * Parameters` |
|      - | 4050 | ` *  $array1` |
|      - | 4051 | ` *    The array to compare from` |
|      - | 4052 | ` *  $array2` |
|      - | 4053 | ` *    An array to compare against` |
|      - | 4054 | ` *  $...` |
|      - | 4055 | ` *   More arrays to compare against` |
|      - | 4056 | ` * Return` |
|      - | 4057 | ` *  Returns an array containing all the entries from array1 whose keys are not present` |
|      - | 4058 | ` *  in any of the other arrays.` |
|      - | 4059 | ` * Note that NULL is returned on failure.` |
|      - | 4060 | ` */` |
|      2 | 4061 | `static int ph7_hashmap_diff_key(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 4062 |  |
|      - | 4063 | `	ph7_hashmap_node *pEntry;` |
|      - | 4064 | `	ph7_hashmap *pSrc,*pMap;` |
|      - | 4065 | `	ph7_value *pArray;` |
|      - | 4066 | `	sxi32 rc;` |
|      - | 4067 | `	sxu32 n;` |
|      - | 4068 | `	int i;` |
|      3 | 4069 | `	if( nArg < 1 \|\| !ph7_value_is_array(apArg[0]) ){` |
|      - | 4070 | `		/* Missing arguments,return NULL */` |
|    ! 0 | 4071 | `		ph7_result_null(pCtx);` |
|    ! 0 | 4072 | `		return PH7_OK;` |
|      - | 4073 | `	}` |
|      3 | 4074 | `	if( nArg == 1 ){` |
|      - | 4075 | `		/* Return the first array since we cannot perform a diff */` |
|    ! 0 | 4076 | `		ph7_result_value(pCtx,apArg[0]);` |
|    ! 0 | 4077 | `		return PH7_OK;` |
|      - | 4078 | `	}` |
|      - | 4079 | `	/* Create a new array */` |
|      3 | 4080 | `	pArray = ph7_context_new_array(pCtx);` |
|      3 | 4081 | `	if( pArray == 0 ){` |
|    ! 0 | 4082 | `		ph7_result_null(pCtx);` |
|    ! 0 | 4083 | `		return PH7_OK;` |
|      - | 4084 | `	}` |
|      - | 4085 | `	/* Point to the internal representation of the main hashmap */` |
|      3 | 4086 | `	pSrc = (ph7_hashmap *)apArg[0]->x.pOther;` |
|      - | 4087 | `	/* Perfrom the diff */` |
|      3 | 4088 | `	pEntry = pSrc->pFirst;` |
|      3 | 4089 | `	n = pSrc->nEntry;` |
|      4 | 4090 | `	for(;;){` |
|      9 | 4091 | `		if( n < 1 ){` |
|      3 | 4092 | `			break;` |
|      - | 4093 | `		}` |
|      9 | 4094 | `		for( i = 1 ; i < nArg ; i++ ){` |
|      7 | 4095 | `			if( !ph7_value_is_array(apArg[i])) {` |
|      - | 4096 | `				/* ignore */` |
|    ! 0 | 4097 | `				continue;` |
|      - | 4098 | `			}` |
|      7 | 4099 | `			pMap = (ph7_hashmap *)apArg[i]->x.pOther;` |
|      7 | 4100 | `			if( pEntry->iType == HASHMAP_BLOB_NODE ){` |
|      7 | 4101 | `				SyBlob *pKey = &pEntry->xKey.sKey;` |
|      - | 4102 | `				/* Blob lookup */` |
|      7 | 4103 | `				rc = HashmapLookupBlobKey(pMap,SyBlobData(pKey),SyBlobLength(pKey),0);` |
|      4 | 4104 | `			}else{` |
|      - | 4105 | `				/* Int lookup */` |
|    ! 0 | 4106 | `				rc = HashmapLookupIntKey(pMap,pEntry->xKey.iKey,0);` |
|      - | 4107 | `			}` |
|      7 | 4108 | `			if( rc == SXRET_OK ){` |
|      - | 4109 | `				/* Key exists,break immediately */` |
|      5 | 4110 | `				break;` |
|      - | 4111 | `			}` |
|      2 | 4112 | `		}` |
|      7 | 4113 | `		if( i >= nArg ){` |
|      - | 4114 | `			/* Perform the insertion */` |
|      3 | 4115 | `			HashmapInsertNode((ph7_hashmap *)pArray->x.pOther,pEntry,TRUE);` |
|      1 | 4116 | `		}` |
|      - | 4117 | `		/* Point to the next entry */` |
|      7 | 4118 | `		pEntry = pEntry->pPrev; /* Reverse link */` |
|      7 | 4119 | `		n--;` |
|      1 | 4120 | `	}` |
|      - | 4121 | `	/* Return the freshly created array */` |
|      3 | 4122 | `	ph7_result_value(pCtx,pArray);` |
|      3 | 4123 | `	return PH7_OK;` |
|      2 | 4124 |  |
|      - | 4125 | `/*` |
|      - | 4126 | ` * array array_intersect(array $array1 ,array $array2,...)` |
|      - | 4127 | ` *  Computes the intersection of arrays.` |
|      - | 4128 | ` * Parameters` |
|      - | 4129 | ` *  $array1` |
|      - | 4130 | ` *    The array to compare from` |
|      - | 4131 | ` *  $array2` |
|      - | 4132 | ` *    An array to compare against` |
|      - | 4133 | ` *  $...` |
|      - | 4134 | ` *   More arrays to compare against` |
|      - | 4135 | ` * Return` |
|      - | 4136 | ` *  Returns an array containing all of the values in array1 whose values exist` |
|      - | 4137 | ` *  in all of the parameters. .` |
|      - | 4138 | ` * Note that NULL is returned on failure.` |
|      - | 4139 | ` */` |
|      2 | 4140 | `static int ph7_hashmap_intersect(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 4141 |  |
|      - | 4142 | `	ph7_hashmap_node *pEntry;` |
|      - | 4143 | `	ph7_hashmap *pSrc,*pMap;` |
|      - | 4144 | `	ph7_value *pArray;` |
|      - | 4145 | `	ph7_value *pVal;` |
|      - | 4146 | `	sxi32 rc;` |
|      - | 4147 | `	sxu32 n;` |
|      - | 4148 | `	int i;` |
|      3 | 4149 | `	if( nArg < 1 \|\| !ph7_value_is_array(apArg[0]) ){` |
|      - | 4150 | `		/* Missing arguments,return NULL */` |
|    ! 0 | 4151 | `		ph7_result_null(pCtx);` |
|    ! 0 | 4152 | `		return PH7_OK;` |
|      - | 4153 | `	}` |
|      3 | 4154 | `	if( nArg == 1 ){` |
|      - | 4155 | `		/* Return the first array since we cannot perform a diff */` |
|    ! 0 | 4156 | `		ph7_result_value(pCtx,apArg[0]);` |
|    ! 0 | 4157 | `		return PH7_OK;` |
|      - | 4158 | `	}` |
|      - | 4159 | `	/* Create a new array */` |
|      3 | 4160 | `	pArray = ph7_context_new_array(pCtx);` |
|      3 | 4161 | `	if( pArray == 0 ){` |
|    ! 0 | 4162 | `		ph7_result_null(pCtx);` |
|    ! 0 | 4163 | `		return PH7_OK;` |
|      - | 4164 | `	}` |
|      - | 4165 | `	/* Point to the internal representation of the source hashmap */` |
|      3 | 4166 | `	pSrc = (ph7_hashmap *)apArg[0]->x.pOther;` |
|      - | 4167 | `	/* Perform the intersection */` |
|      3 | 4168 | `	pEntry = pSrc->pFirst;` |
|      3 | 4169 | `	n = pSrc->nEntry;` |
|      5 | 4170 | `	for(;;){` |
|     11 | 4171 | `		if( n < 1 ){` |
|      3 | 4172 | `			break;` |
|      - | 4173 | `		}` |
|      - | 4174 | `		/* Extract the node value */` |
|      9 | 4175 | `		pVal = HashmapExtractNodeValue(pEntry);` |
|      9 | 4176 | `		if( pVal ){` |
|     13 | 4177 | `			for( i = 1 ; i < nArg ; i++ ){` |
|      9 | 4178 | `				if( !ph7_value_is_array(apArg[i])) {` |
|      - | 4179 | `					/* ignore */` |
|    ! 0 | 4180 | `					continue;` |
|      - | 4181 | `				}` |
|      - | 4182 | `				/* Point to the internal representation of the hashmap */` |
|      9 | 4183 | `				pMap = (ph7_hashmap *)apArg[i]->x.pOther;` |
|      - | 4184 | `				/* Perform the lookup */` |
|      9 | 4185 | `				rc = HashmapFindValue(pMap,pVal,0,TRUE);` |
|      9 | 4186 | `				if( rc != SXRET_OK ){` |
|      - | 4187 | `					/* Value does not exist */` |
|      5 | 4188 | `					break;` |
|      - | 4189 | `				}` |
|      3 | 4190 | `			}` |
|      9 | 4191 | `			if( i >= nArg ){` |
|      - | 4192 | `				/* Perform the insertion */` |
|      5 | 4193 | `				HashmapInsertNode((ph7_hashmap *)pArray->x.pOther,pEntry,TRUE);` |
|      2 | 4194 | `			}` |
|      4 | 4195 | `		}` |
|      - | 4196 | `		/* Point to the next entry */` |
|      9 | 4197 | `		pEntry = pEntry->pPrev; /* Reverse link */` |
|      9 | 4198 | `		n--;` |
|      1 | 4199 | `	}` |
|      - | 4200 | `	/* Return the freshly created array */` |
|      3 | 4201 | `	ph7_result_value(pCtx,pArray);` |
|      3 | 4202 | `	return PH7_OK;` |
|      2 | 4203 |  |
|      - | 4204 | `/*` |
|      - | 4205 | ` * array array_intersect_assoc(array $array1 ,array $array2,...)` |
|      - | 4206 | ` *  Computes the intersection of arrays.` |
|      - | 4207 | ` * Parameters` |
|      - | 4208 | ` *  $array1` |
|      - | 4209 | ` *    The array to compare from` |
|      - | 4210 | ` *  $array2` |
|      - | 4211 | ` *    An array to compare against` |
|      - | 4212 | ` *  $...` |
|      - | 4213 | ` *   More arrays to compare against` |
|      - | 4214 | ` * Return` |
|      - | 4215 | ` *  Returns an array containing all of the values in array1 whose values exist` |
|      - | 4216 | ` *  in all of the parameters. .` |
|      - | 4217 | ` * Note that NULL is returned on failure.` |
|      - | 4218 | ` */` |
|      2 | 4219 | `static int ph7_hashmap_intersect_assoc(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 4220 |  |
|      - | 4221 | `	ph7_hashmap_node *pEntry,*pN1,*pN2;` |
|      - | 4222 | `	ph7_hashmap *pSrc,*pMap;` |
|      - | 4223 | `	ph7_value *pArray;` |
|      - | 4224 | `	ph7_value *pVal;` |
|      - | 4225 | `	sxi32 rc;` |
|      - | 4226 | `	sxu32 n;` |
|      - | 4227 | `	int i;` |
|      3 | 4228 | `	if( nArg < 1 \|\| !ph7_value_is_array(apArg[0]) ){` |
|      - | 4229 | `		/* Missing arguments,return NULL */` |
|    ! 0 | 4230 | `		ph7_result_null(pCtx);` |
|    ! 0 | 4231 | `		return PH7_OK;` |
|      - | 4232 | `	}` |
|      3 | 4233 | `	if( nArg == 1 ){` |
|      - | 4234 | `		/* Return the first array since we cannot perform a diff */` |
|    ! 0 | 4235 | `		ph7_result_value(pCtx,apArg[0]);` |
|    ! 0 | 4236 | `		return PH7_OK;` |
|      - | 4237 | `	}` |
|      - | 4238 | `	/* Create a new array */` |
|      3 | 4239 | `	pArray = ph7_context_new_array(pCtx);` |
|      3 | 4240 | `	if( pArray == 0 ){` |
|    ! 0 | 4241 | `		ph7_result_null(pCtx);` |
|    ! 0 | 4242 | `		return PH7_OK;` |
|      - | 4243 | `	}` |
|      - | 4244 | `	/* Point to the internal representation of the source hashmap */` |
|      3 | 4245 | `	pSrc = (ph7_hashmap *)apArg[0]->x.pOther;` |
|      - | 4246 | `	/* Perform the intersection */` |
|      3 | 4247 | `	pEntry = pSrc->pFirst;` |
|      3 | 4248 | `	n = pSrc->nEntry;` |
|      3 | 4249 | `	pN1 = pN2 = 0; /* cc warning */` |
|      4 | 4250 | `	for(;;){` |
|      9 | 4251 | `		if( n < 1 ){` |
|      3 | 4252 | `			break;` |
|      - | 4253 | `		}` |
|      - | 4254 | `		/* Extract the node value */` |
|      7 | 4255 | `		pVal = HashmapExtractNodeValue(pEntry);` |
|      7 | 4256 | `		if( pVal ){` |
|      9 | 4257 | `			for( i = 1 ; i < nArg ; i++ ){` |
|      7 | 4258 | `				if( !ph7_value_is_array(apArg[i])) {` |
|      - | 4259 | `					/* ignore */` |
|    ! 0 | 4260 | `					continue;` |
|      - | 4261 | `				}` |
|      - | 4262 | `				/* Point to the internal representation of the hashmap */` |
|      7 | 4263 | `				pMap = (ph7_hashmap *)apArg[i]->x.pOther;` |
|      - | 4264 | `				/* Perform a key lookup first */` |
|      7 | 4265 | `				if( pEntry->iType == HASHMAP_INT_NODE ){` |
|    ! 0 | 4266 | `					rc = HashmapLookupIntKey(pMap,pEntry->xKey.iKey,&pN1);` |
|    ! 0 | 4267 | `				}else{` |
|      7 | 4268 | `					rc = HashmapLookupBlobKey(pMap,SyBlobData(&pEntry->xKey.sKey),SyBlobLength(&pEntry->xKey.sKey),&pN1);` |
|      - | 4269 | `				}` |
|      7 | 4270 | `				if( rc != SXRET_OK ){` |
|      - | 4271 | `					/* No such key,break immediately */` |
|      3 | 4272 | `					break;` |
|      - | 4273 | `				}` |
|      - | 4274 | `				/* Perform the lookup */` |
|      5 | 4275 | `				rc = HashmapFindValue(pMap,pVal,&pN2,TRUE);` |
|      5 | 4276 | `				if( rc != SXRET_OK \|\| pN1 != pN2 ){` |
|      - | 4277 | `					/* Value does not exist */` |
|      2 | 4278 | `					break;` |
|      - | 4279 | `				}` |
|      2 | 4280 | `			}` |
|      7 | 4281 | `			if( i >= nArg ){` |
|      - | 4282 | `				/* Perform the insertion */` |
|      3 | 4283 | `				HashmapInsertNode((ph7_hashmap *)pArray->x.pOther,pEntry,TRUE);` |
|      1 | 4284 | `			}` |
|      3 | 4285 | `		}` |
|      - | 4286 | `		/* Point to the next entry */` |
|      7 | 4287 | `		pEntry = pEntry->pPrev; /* Reverse link */` |
|      7 | 4288 | `		n--;` |
|      1 | 4289 | `	}` |
|      - | 4290 | `	/* Return the freshly created array */` |
|      3 | 4291 | `	ph7_result_value(pCtx,pArray);` |
|      3 | 4292 | `	return PH7_OK;` |
|      2 | 4293 |  |
|      - | 4294 | `/*` |
|      - | 4295 | ` * array array_intersect_key(array $array1 ,array $array2,...)` |
|      - | 4296 | ` *  Computes the intersection of arrays using keys for comparison.` |
|      - | 4297 | ` * Parameters` |
|      - | 4298 | ` *  $array1` |
|      - | 4299 | ` *    The array to compare from` |
|      - | 4300 | ` *  $array2` |
|      - | 4301 | ` *    An array to compare against` |
|      - | 4302 | ` *  $...` |
|      - | 4303 | ` *   More arrays to compare against` |
|      - | 4304 | ` * Return` |
|      - | 4305 | ` *  Returns an associative array containing all the entries of array1 which` |
|      - | 4306 | ` *  have keys that are present in all arguments.` |
|      - | 4307 | ` * Note that NULL is returned on failure.` |
|      - | 4308 | ` */` |
|      4 | 4309 | `static int ph7_hashmap_intersect_key(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 4310 |  |
|      - | 4311 | `	ph7_hashmap_node *pEntry;` |
|      - | 4312 | `	ph7_hashmap *pSrc,*pMap;` |
|      - | 4313 | `	ph7_value *pArray;` |
|      - | 4314 | `	sxi32 rc;` |
|      - | 4315 | `	sxu32 n;` |
|      - | 4316 | `	int i;` |
|      5 | 4317 | `	if( nArg < 1 \|\| !ph7_value_is_array(apArg[0]) ){` |
|      - | 4318 | `		/* Missing arguments,return NULL */` |
|    ! 0 | 4319 | `		ph7_result_null(pCtx);` |
|    ! 0 | 4320 | `		return PH7_OK;` |
|      - | 4321 | `	}` |
|      5 | 4322 | `	if( nArg == 1 ){` |
|      - | 4323 | `		/* Return the first array since we cannot perform a diff */` |
|    ! 0 | 4324 | `		ph7_result_value(pCtx,apArg[0]);` |
|    ! 0 | 4325 | `		return PH7_OK;` |
|      - | 4326 | `	}` |
|      - | 4327 | `	/* Create a new array */` |
|      5 | 4328 | `	pArray = ph7_context_new_array(pCtx);` |
|      5 | 4329 | `	if( pArray == 0 ){` |
|    ! 0 | 4330 | `		ph7_result_null(pCtx);` |
|    ! 0 | 4331 | `		return PH7_OK;` |
|      - | 4332 | `	}` |
|      - | 4333 | `	/* Point to the internal representation of the main hashmap */` |
|      5 | 4334 | `	pSrc = (ph7_hashmap *)apArg[0]->x.pOther;` |
|      - | 4335 | `	/* Perfrom the intersection */` |
|      5 | 4336 | `	pEntry = pSrc->pFirst;` |
|      5 | 4337 | `	n = pSrc->nEntry;` |
|      8 | 4338 | `	for(;;){` |
|     17 | 4339 | `		if( n < 1 ){` |
|      5 | 4340 | `			break;` |
|      - | 4341 | `		}` |
|     19 | 4342 | `		for( i = 1 ; i < nArg ; i++ ){` |
|     13 | 4343 | `			if( !ph7_value_is_array(apArg[i])) {` |
|      - | 4344 | `				/* ignore */` |
|    ! 0 | 4345 | `				continue;` |
|      - | 4346 | `			}` |
|     13 | 4347 | `			pMap = (ph7_hashmap *)apArg[i]->x.pOther;` |
|     13 | 4348 | `			if( pEntry->iType == HASHMAP_BLOB_NODE ){` |
|      7 | 4349 | `				SyBlob *pKey = &pEntry->xKey.sKey;` |
|      - | 4350 | `				/* Blob lookup */` |
|      7 | 4351 | `				rc = HashmapLookupBlobKey(pMap,SyBlobData(pKey),SyBlobLength(pKey),0);` |
|      4 | 4352 | `			}else{` |
|      - | 4353 | `				/* Int key */` |
|      7 | 4354 | `				rc = HashmapLookupIntKey(pMap,pEntry->xKey.iKey,0);` |
|      - | 4355 | `			}` |
|     13 | 4356 | `			if( rc != SXRET_OK ){` |
|      - | 4357 | `				/* Key does not exists,break immediately */` |
|      7 | 4358 | `				break;` |
|      - | 4359 | `			}` |
|      4 | 4360 | `		}` |
|     13 | 4361 | `		if( i >= nArg ){` |
|      - | 4362 | `			/* Perform the insertion */` |
|      7 | 4363 | `			HashmapInsertNode((ph7_hashmap *)pArray->x.pOther,pEntry,TRUE);` |
|      3 | 4364 | `		}` |
|      - | 4365 | `		/* Point to the next entry */` |
|     13 | 4366 | `		pEntry = pEntry->pPrev; /* Reverse link */` |
|     13 | 4367 | `		n--;` |
|      1 | 4368 | `	}` |
|      - | 4369 | `	/* Return the freshly created array */` |
|      5 | 4370 | `	ph7_result_value(pCtx,pArray);` |
|      5 | 4371 | `	return PH7_OK;` |
|      3 | 4372 |  |
|      - | 4373 | `/*` |
|      - | 4374 | ` * array array_uintersect(array $array1 ,array $array2,...,$callback)` |
|      - | 4375 | ` *  Computes the intersection of arrays.` |
|      - | 4376 | ` * Parameters` |
|      - | 4377 | ` *  $array1` |
|      - | 4378 | ` *    The array to compare from` |
|      - | 4379 | ` *  $array2` |
|      - | 4380 | ` *    An array to compare against` |
|      - | 4381 | ` *  $...` |
|      - | 4382 | ` *   More arrays to compare against` |
|      - | 4383 | ` * $callback` |
|      - | 4384 | ` *  The callback comparison function.` |
|      - | 4385 | ` *  The comparison function must return an integer less than, equal to, or greater than zero` |
|      - | 4386 | ` *  if the first argument is considered to be respectively less than, equal to, or greater` |
|      - | 4387 | ` *  than the second.` |
|      - | 4388 | ` *     int callback ( mixed $a, mixed $b )` |
|      - | 4389 | ` * Return` |
|      - | 4390 | ` *  Returns an array containing all of the values in array1 whose values exist` |
|      - | 4391 | ` *  in all of the parameters. .` |
|      - | 4392 | ` * Note that NULL is returned on failure.` |
|      - | 4393 | ` */` |
|      2 | 4394 | `static int ph7_hashmap_uintersect(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 4395 |  |
|      - | 4396 | `	ph7_hashmap_node *pEntry;` |
|      - | 4397 | `	ph7_hashmap *pSrc,*pMap;` |
|      - | 4398 | `	ph7_value *pCallback;` |
|      - | 4399 | `	ph7_value *pArray;` |
|      - | 4400 | `	ph7_value *pVal;` |
|      - | 4401 | `	sxi32 rc;` |
|      - | 4402 | `	sxu32 n;` |
|      - | 4403 | `	int i;` |
|      - | 4404 |  |
|      3 | 4405 | `	if( nArg < 2 \|\| !ph7_value_is_array(apArg[0]) ){` |
|      - | 4406 | `		/* Missing/Invalid arguments,return NULL */` |
|    ! 0 | 4407 | `		ph7_result_null(pCtx);` |
|    ! 0 | 4408 | `		return PH7_OK;` |
|      - | 4409 | `	}` |
|      - | 4410 | `	/* Point to the callback */` |
|      3 | 4411 | `	pCallback = apArg[nArg - 1];` |
|      3 | 4412 | `	if( nArg == 2 ){` |
|      - | 4413 | `		/* Return the first array since we cannot perform a diff */` |
|    ! 0 | 4414 | `		ph7_result_value(pCtx,apArg[0]);` |
|    ! 0 | 4415 | `		return PH7_OK;` |
|      - | 4416 | `	}` |
|      - | 4417 | `	/* Create a new array */` |
|      3 | 4418 | `	pArray = ph7_context_new_array(pCtx);` |
|      3 | 4419 | `	if( pArray == 0 ){` |
|    ! 0 | 4420 | `		ph7_result_null(pCtx);` |
|    ! 0 | 4421 | `		return PH7_OK;` |
|      - | 4422 | `	}` |
|      - | 4423 | `	/* Point to the internal representation of the source hashmap */` |
|      3 | 4424 | `	pSrc = (ph7_hashmap *)apArg[0]->x.pOther;` |
|      - | 4425 | `	/* Perform the intersection */` |
|      3 | 4426 | `	pEntry = pSrc->pFirst;` |
|      3 | 4427 | `	n = pSrc->nEntry;` |
|      4 | 4428 | `	for(;;){` |
|      9 | 4429 | `		if( n < 1 ){` |
|      3 | 4430 | `			break;` |
|      - | 4431 | `		}` |
|      - | 4432 | `		/* Extract the node value */` |
|      7 | 4433 | `		pVal = HashmapExtractNodeValue(pEntry);` |
|      7 | 4434 | `		if( pVal ){` |
|     11 | 4435 | `			for( i = 1 ; i < nArg - 1; i++ ){` |
|      7 | 4436 | `				if( !ph7_value_is_array(apArg[i])) {` |
|      - | 4437 | `					/* ignore */` |
|    ! 0 | 4438 | `					continue;` |
|      - | 4439 | `				}` |
|      - | 4440 | `				/* Point to the internal representation of the hashmap */` |
|      7 | 4441 | `				pMap = (ph7_hashmap *)apArg[i]->x.pOther;` |
|      - | 4442 | `				/* Perform the lookup */` |
|      7 | 4443 | `				rc = HashmapFindValueByCallback(pMap,pVal,pCallback,0);` |
|      7 | 4444 | `				if( rc != SXRET_OK ){` |
|      - | 4445 | `					/* Value does not exist */` |
|      3 | 4446 | `					break;` |
|      - | 4447 | `				}` |
|      3 | 4448 | `			}` |
|      7 | 4449 | `			if( i >= (nArg-1) ){` |
|      - | 4450 | `				/* Perform the insertion */` |
|      5 | 4451 | `				HashmapInsertNode((ph7_hashmap *)pArray->x.pOther,pEntry,TRUE);` |
|      2 | 4452 | `			}` |
|      3 | 4453 | `		}` |
|      - | 4454 | `		/* Point to the next entry */` |
|      7 | 4455 | `		pEntry = pEntry->pPrev; /* Reverse link */` |
|      7 | 4456 | `		n--;` |
|      1 | 4457 | `	}` |
|      - | 4458 | `	/* Return the freshly created array */` |
|      3 | 4459 | `	ph7_result_value(pCtx,pArray);` |
|      3 | 4460 | `	return PH7_OK;` |
|      2 | 4461 |  |
|      - | 4462 | `/*` |
|      - | 4463 | ` * array array_fill(int $start_index,int $num,var $value)` |
|      - | 4464 | ` *  Fill an array with values.` |
|      - | 4465 | ` * Parameters` |
|      - | 4466 | ` *  $start_index` |
|      - | 4467 | ` *    The first index of the returned array.` |
|      - | 4468 | ` *  $num` |
|      - | 4469 | ` *   Number of elements to insert.` |
|      - | 4470 | ` *  $value` |
|      - | 4471 | ` *    Value to use for filling.` |
|      - | 4472 | ` * Return` |
|      - | 4473 | ` *  The filled array or null on failure.` |
|      - | 4474 | ` */` |
|    208 | 4475 | `static int ph7_hashmap_fill(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 4476 |  |
|      - | 4477 | `	ph7_value *pArray;` |
|      - | 4478 | `	int i,nEntry;` |
|    209 | 4479 | `	if( nArg < 3 ){` |
|      - | 4480 | `		/* Missing arguments,return NULL */` |
|    ! 0 | 4481 | `		ph7_result_null(pCtx);` |
|    ! 0 | 4482 | `		return PH7_OK;` |
|      - | 4483 | `	}` |
|      - | 4484 | `	/* Create a new array */` |
|    209 | 4485 | `	pArray = ph7_context_new_array(pCtx);` |
|    209 | 4486 | `	if( pArray == 0 ){` |
|    ! 0 | 4487 | `		ph7_result_null(pCtx);` |
|    ! 0 | 4488 | `		return PH7_OK;` |
|      - | 4489 | `	}` |
|      - | 4490 | `	/* Total number of entries to insert */` |
|    209 | 4491 | `	nEntry = ph7_value_to_int(apArg[1]);` |
|      - | 4492 | `	/* Insert the first entry alone because it have it's own key */` |
|    209 | 4493 | `	ph7_array_add_intkey_elem(pArray,ph7_value_to_int(apArg[0]),apArg[2]);` |
|      - | 4494 | `	/* Repeat insertion of the desired value */` |
|  20409 | 4495 | `	for( i = 1 ; i < nEntry ; i++ ){` |
|  20201 | 4496 | `		ph7_array_add_elem(pArray,0/*Automatic index assign */,apArg[2]);` |
|  10101 | 4497 | `	}` |
|      - | 4498 | `	/* Return the filled array */` |
|    209 | 4499 | `	ph7_result_value(pCtx,pArray);` |
|    209 | 4500 | `	return PH7_OK;` |
|    105 | 4501 |  |
|      - | 4502 | `/*` |
|      - | 4503 | ` * array array_fill_keys(array $input,var $value)` |
|      - | 4504 | ` *  Fill an array with values, specifying keys.` |
|      - | 4505 | ` * Parameters` |
|      - | 4506 | ` *  $input` |
|      - | 4507 | ` *   Array of values that will be used as key.` |
|      - | 4508 | ` *  $value` |
|      - | 4509 | ` *    Value to use for filling.` |
|      - | 4510 | ` * Return` |
|      - | 4511 | ` *  The filled array or null on failure.` |
|      - | 4512 | ` */` |
|      2 | 4513 | `static int ph7_hashmap_fill_keys(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 4514 |  |
|      - | 4515 | `	ph7_hashmap_node *pEntry;` |
|      - | 4516 | `	ph7_hashmap *pSrc;` |
|      - | 4517 | `	ph7_value *pArray;` |
|      - | 4518 | `	sxu32 n;` |
|      3 | 4519 | `	if( nArg < 2 ){` |
|      - | 4520 | `		/* Missing arguments,return NULL */` |
|    ! 0 | 4521 | `		ph7_result_null(pCtx);` |
|    ! 0 | 4522 | `		return PH7_OK;` |
|      - | 4523 | `	}` |
|      - | 4524 | `	/* Make sure we are dealing with a valid hashmap */` |
|      3 | 4525 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|      - | 4526 | `		/* Invalid argument,return NULL */` |
|    ! 0 | 4527 | `		ph7_result_null(pCtx);` |
|    ! 0 | 4528 | `		return PH7_OK;` |
|      - | 4529 | `	}` |
|      - | 4530 | `	/* Point to the internal representation of the input hashmap */` |
|      3 | 4531 | `	pSrc = (ph7_hashmap *)apArg[0]->x.pOther;` |
|      - | 4532 | `	/* Create a new array */` |
|      3 | 4533 | `	pArray = ph7_context_new_array(pCtx);` |
|      3 | 4534 | `	if( pArray == 0 ){` |
|    ! 0 | 4535 | `		ph7_result_null(pCtx);` |
|    ! 0 | 4536 | `		return PH7_OK;` |
|      - | 4537 | `	}` |
|      - | 4538 | `	/* Perform the requested operation */` |
|      3 | 4539 | `	pEntry = pSrc->pFirst;` |
|      7 | 4540 | `	for( n = 0 ; n < pSrc->nEntry ; n++ ){` |
|      5 | 4541 | `		ph7_array_add_elem(pArray,HashmapExtractNodeValue(pEntry),apArg[1]);` |
|      - | 4542 | `		/* Point to the next entry */` |
|      5 | 4543 | `		pEntry = pEntry->pPrev; /* Reverse link */` |
|      3 | 4544 | `	}` |
|      - | 4545 | `	/* Return the filled array */` |
|      3 | 4546 | `	ph7_result_value(pCtx,pArray);` |
|      3 | 4547 | `	return PH7_OK;` |
|      2 | 4548 |  |
|      - | 4549 | `/*` |
|      - | 4550 | ` * array array_combine(array $keys,array $values)` |
|      - | 4551 | ` *  Creates an array by using one array for keys and another for its values.` |
|      - | 4552 | ` * Parameters` |
|      - | 4553 | ` *  $keys` |
|      - | 4554 | ` *    Array of keys to be used.` |
|      - | 4555 | ` * $values` |
|      - | 4556 | ` *   Array of values to be used.` |
|      - | 4557 | ` * Return` |
|      - | 4558 | ` *  Returns the combined array. Otherwise FALSE if the number of elements` |
|      - | 4559 | ` *  for each array isn't equal or if one of the given arguments is` |
|      - | 4560 | ` *  not an array.` |
|      - | 4561 | ` */` |
|     18 | 4562 | `static int ph7_hashmap_combine(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      2 | 4563 |  |
|      - | 4564 | `	ph7_hashmap_node *pKe,*pVe;` |
|      - | 4565 | `	ph7_hashmap *pKey,*pValue;` |
|      - | 4566 | `	ph7_value *pArray;` |
|      - | 4567 | `	sxu32 n;` |
|      - | 4568 | `	/* PHP enforces argument count and type checks. */` |
|     20 | 4569 | `	if( nArg != 2 ){` |
|      - | 4570 | `		/* wrong number of arguments -> ArgumentCountError */` |
|      4 | 4571 | `		return PH7_VmThrowException(pCtx,` |
|      - | 4572 | `			"ArgumentCountError",` |
|      - | 4573 | `			"array_combine() expects exactly 2 arguments, %d given",` |
|      1 | 4574 | `			nArg` |
|      - | 4575 | `			);` |
|      - | 4576 | `	}` |
|      - | 4577 | `	/* Validate argument types individually so we can report the correct` |
|      - | 4578 | `	 * argument index in the error message. */` |
|     18 | 4579 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|      4 | 4580 | `		return PH7_VmThrowException(pCtx,` |
|      - | 4581 | `			"TypeError",` |
|      - | 4582 | `			"array_combine(): Argument #1 ($keys) must be of type array, %s given",` |
|      1 | 4583 | `			ph7_type_name(apArg[0])` |
|      - | 4584 | `			);` |
|      - | 4585 | `	}` |
|     16 | 4586 | `	if( !ph7_value_is_array(apArg[1]) ){` |
|      4 | 4587 | `		return PH7_VmThrowException(pCtx,` |
|      - | 4588 | `			"TypeError",` |
|      - | 4589 | `			"array_combine(): Argument #2 ($values) must be of type array, %s given",` |
|      2 | 4590 | `			ph7_type_name(apArg[1])` |
|      - | 4591 | `			);` |
|      - | 4592 | `	}` |
|      - | 4593 | `	/* Point to the internal representation of the input hashmaps */` |
|     14 | 4594 | `	pKey   = (ph7_hashmap *)apArg[0]->x.pOther;` |
|     14 | 4595 | `	pValue = (ph7_hashmap *)apArg[1]->x.pOther;` |
|     14 | 4596 | `	if( pKey->nEntry != pValue->nEntry ){` |
|      - | 4597 | `		/* Length mismatch -> ValueError */` |
|      3 | 4598 | `		return PH7_VmThrowException(pCtx,` |
|      - | 4599 | `			"ValueError",` |
|      - | 4600 | `			"array_combine(): Argument #1 ($keys) and argument #2 ($values) must have the same number of elements"` |
|      - | 4601 | `			);` |
|      - | 4602 | `	}` |
|      - | 4603 | `	/* Create a new array */` |
|     11 | 4604 | `	pArray = ph7_context_new_array(pCtx);` |
|     11 | 4605 | `	if( pArray == 0 ){` |
|    ! 0 | 4606 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 4607 | `		return PH7_OK;` |
|      - | 4608 | `	}` |
|      - | 4609 | `	/* Perform the requested operation */` |
|     11 | 4610 | `	pKe = pKey->pFirst;` |
|     11 | 4611 | `	pVe = pValue->pFirst;` |
|     33 | 4612 | `	for( n = 0 ; n < pKey->nEntry ; n++ ){` |
|     23 | 4613 | `		ph7_value *pKeyVal = HashmapExtractNodeValue(pKe);` |
|     23 | 4614 | `		ph7_value *pValVal = HashmapExtractNodeValue(pVe);` |
|      - | 4615 | `		/* PHP treats floats used as keys in array_combine differently than` |
|      - | 4616 | `		 * ordinary offset access: the float is stringified rather than` |
|      - | 4617 | `		 * truncated.  To emulate this behavior we create a temporary copy of` |
|      - | 4618 | `		 * the value when it is a float and convert the copy to string.  The` |
|      - | 4619 | `		 * original array must not be mutated. */` |
|     23 | 4620 | `		ph7_value *pKeyCopy = pKeyVal;` |
|     23 | 4621 | `		if( ph7_value_is_float(pKeyVal) ){` |
|      5 | 4622 | `			ph7_value *pTmpKey = ph7_context_new_scalar(pCtx);` |
|      5 | 4623 | `			if( pTmpKey ){` |
|      5 | 4624 | `				PH7_MemObjStore(pKeyVal,pTmpKey);` |
|      - | 4625 | `				/* Convert copy to string so it becomes "1.5" or "2" etc. */` |
|      5 | 4626 | `				PH7_MemObjToString(pTmpKey);` |
|      5 | 4627 | `				pKeyCopy = pTmpKey;` |
|      2 | 4628 | `			}` |
|      2 | 4629 | `		}` |
|     23 | 4630 | `		ph7_array_add_elem(pArray,pKeyCopy,pValVal);` |
|      - | 4631 | `		/* Point to the next entry */` |
|     23 | 4632 | `		pKe = pKe->pPrev; /* Reverse link */` |
|     23 | 4633 | `		pVe = pVe->pPrev;` |
|     12 | 4634 | `	}` |
|      - | 4635 | `	/* Return the filled array */` |
|     11 | 4636 | `	ph7_result_value(pCtx,pArray);` |
|     11 | 4637 | `	return PH7_OK;` |
|     11 | 4638 |  |
|      - | 4639 | `/*` |
|      - | 4640 | ` * array array_reverse(array $array [,bool $preserve_keys = false ])` |
|      - | 4641 | ` *  Return an array with elements in reverse order.` |
|      - | 4642 | ` * Parameters` |
|      - | 4643 | ` *  $array` |
|      - | 4644 | ` *   The input array.` |
|      - | 4645 | ` *  $preserve_keys (optional)` |
|      - | 4646 | ` *   If set to TRUE keys are preserved.` |
|      - | 4647 | ` * Return` |
|      - | 4648 | ` *  The reversed array.` |
|      - | 4649 | ` */` |
|      6 | 4650 | `static int ph7_hashmap_reverse(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 4651 |  |
|      - | 4652 | `	ph7_hashmap_node *pEntry;` |
|      - | 4653 | `	ph7_hashmap *pSrc;` |
|      - | 4654 | `	ph7_value *pArray;` |
|      - | 4655 | `	int bPreserve;` |
|      - | 4656 | `	sxu32 n;` |
|      7 | 4657 | `	if( nArg < 1 ){` |
|      - | 4658 | `		/* Missing arguments,return NULL */` |
|    ! 0 | 4659 | `		ph7_result_null(pCtx);` |
|    ! 0 | 4660 | `		return PH7_OK;` |
|      - | 4661 | `	}` |
|      - | 4662 | `	/* Make sure we are dealing with a valid hashmap */` |
|      7 | 4663 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|      - | 4664 | `		/* Invalid argument,return NULL */` |
|    ! 0 | 4665 | `		ph7_result_null(pCtx);` |
|    ! 0 | 4666 | `		return PH7_OK;` |
|      - | 4667 | `	}` |
|      7 | 4668 | `	bPreserve = FALSE;` |
|      7 | 4669 | `	if( nArg > 1 && ph7_value_is_bool(apArg[1]) ){` |
|      3 | 4670 | `		bPreserve = ph7_value_to_bool(apArg[1]);` |
|      1 | 4671 | `	}` |
|      - | 4672 | `	/* Point to the internal representation of the input hashmap */` |
|      7 | 4673 | `	pSrc = (ph7_hashmap *)apArg[0]->x.pOther;` |
|      - | 4674 | `	/* Create a new array */` |
|      7 | 4675 | `	pArray = ph7_context_new_array(pCtx);` |
|      7 | 4676 | `	if( pArray == 0 ){` |
|    ! 0 | 4677 | `		ph7_result_null(pCtx);` |
|    ! 0 | 4678 | `		return PH7_OK;` |
|      - | 4679 | `	}` |
|      - | 4680 | `	/* Perform the requested operation */` |
|      7 | 4681 | `	pEntry = pSrc->pLast;` |
|     23 | 4682 | `	for( n = 0 ; n < pSrc->nEntry ; n++ ){` |
|     17 | 4683 | `		HashmapInsertNode((ph7_hashmap *)pArray->x.pOther,pEntry,bPreserve);` |
|      - | 4684 | `		/* Point to the previous entry */` |
|     17 | 4685 | `		pEntry = pEntry->pNext; /* Reverse link */` |
|      9 | 4686 | `	}` |
|      7 | 4687 | `	ph7_result_value(pCtx,pArray);` |
|      7 | 4688 | `	return PH7_OK;` |
|      4 | 4689 |  |
|      - | 4690 | `/*` |
|      - | 4691 | ` * array array_unique(array $array[,int $sort_flags = SORT_STRING ])` |
|      - | 4692 | ` *  Removes duplicate values from an array` |
|      - | 4693 | ` * Parameter` |
|      - | 4694 | ` *  $array` |
|      - | 4695 | ` *   The input array.` |
|      - | 4696 | ` *  $sort_flags` |
|      - | 4697 | ` *    The optional second parameter sort_flags may be used to modify the sorting behavior using these values:` |
|      - | 4698 | ` *    Sorting type flags:` |
|      - | 4699 | ` *       SORT_REGULAR - compare items normally (don't change types)` |
|      - | 4700 | ` *       SORT_NUMERIC - compare items numerically` |
|      - | 4701 | ` *       SORT_STRING - compare items as strings` |
|      - | 4702 | ` *       SORT_LOCALE_STRING - compare items as` |
|      - | 4703 | ` * Return` |
|      - | 4704 | ` *  Filtered array or NULL on failure.` |
|      - | 4705 | ` */` |
|      2 | 4706 | `static int ph7_hashmap_unique(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 4707 |  |
|      - | 4708 | `	ph7_hashmap_node *pEntry;` |
|      - | 4709 | `	ph7_value *pNeedle;` |
|      - | 4710 | `	ph7_hashmap *pSrc;` |
|      - | 4711 | `	ph7_value *pArray;` |
|      - | 4712 | `	int bStrict;` |
|      - | 4713 | `	sxi32 rc;` |
|      - | 4714 | `	sxu32 n;` |
|      3 | 4715 | `	if( nArg < 1 ){` |
|      - | 4716 | `		/* Missing arguments,return NULL */` |
|    ! 0 | 4717 | `		ph7_result_null(pCtx);` |
|    ! 0 | 4718 | `		return PH7_OK;` |
|      - | 4719 | `	}` |
|      - | 4720 | `	/* Make sure we are dealing with a valid hashmap */` |
|      3 | 4721 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|      - | 4722 | `		/* Invalid argument,return NULL */` |
|    ! 0 | 4723 | `		ph7_result_null(pCtx);` |
|    ! 0 | 4724 | `		return PH7_OK;` |
|      - | 4725 | `	}` |
|      3 | 4726 | `	bStrict = FALSE;` |
|      3 | 4727 | `	if( nArg > 1 ){` |
|    ! 0 | 4728 | `		bStrict = ph7_value_to_int(apArg[1]) == 3 /* SORT_REGULAR */ ? 1 : 0;` |
|    ! 0 | 4729 | `	}` |
|      - | 4730 | `	/* Point to the internal representation of the input hashmap */` |
|      3 | 4731 | `	pSrc = (ph7_hashmap *)apArg[0]->x.pOther;` |
|      - | 4732 | `	/* Create a new array */` |
|      3 | 4733 | `	pArray = ph7_context_new_array(pCtx);` |
|      3 | 4734 | `	if( pArray == 0 ){` |
|    ! 0 | 4735 | `		ph7_result_null(pCtx);` |
|    ! 0 | 4736 | `		return PH7_OK;` |
|      - | 4737 | `	}` |
|      - | 4738 | `	/* Perform the requested operation */` |
|      3 | 4739 | `	pEntry = pSrc->pFirst;` |
|     13 | 4740 | `	for( n = 0 ; n < pSrc->nEntry ; n++ ){` |
|     11 | 4741 | `		pNeedle = HashmapExtractNodeValue(pEntry);` |
|     11 | 4742 | `		rc = SXERR_NOTFOUND;` |
|     11 | 4743 | `		if( pNeedle ){` |
|     11 | 4744 | `			rc = HashmapFindValue((ph7_hashmap *)pArray->x.pOther,pNeedle,0,bStrict);` |
|      5 | 4745 | `		}` |
|     11 | 4746 | `		if( rc != SXRET_OK ){` |
|      - | 4747 | `			/* Perform the insertion */` |
|      7 | 4748 | `			HashmapInsertNode((ph7_hashmap *)pArray->x.pOther,pEntry,TRUE);` |
|      3 | 4749 | `		}` |
|      - | 4750 | `		/* Point to the next entry */` |
|     11 | 4751 | `		pEntry = pEntry->pPrev; /* Reverse link */` |
|      6 | 4752 | `	}` |
|      - | 4753 | `	/* Return the freshly created array */` |
|      3 | 4754 | `	ph7_result_value(pCtx,pArray);` |
|      3 | 4755 | `	return PH7_OK;` |
|      2 | 4756 |  |
|      - | 4757 | `/*` |
|      - | 4758 | ` * array array_flip(array $input)` |
|      - | 4759 | ` *  Exchanges all keys with their associated values in an array.` |
|      - | 4760 | ` * Parameter` |
|      - | 4761 | ` *  $input` |
|      - | 4762 | ` *   Input array.` |
|      - | 4763 | ` * Return` |
|      - | 4764 | ` *   The flipped array on success or NULL on failure.` |
|      - | 4765 | ` */` |
|     28 | 4766 | `static int ph7_hashmap_flip(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 4767 |  |
|      - | 4768 | `	ph7_hashmap_node *pEntry;` |
|      - | 4769 | `	ph7_hashmap *pSrc;` |
|      - | 4770 | `	ph7_value *pArray;` |
|      - | 4771 | `	ph7_value *pKey;` |
|      - | 4772 | `	ph7_value sVal;` |
|      - | 4773 | `	sxu32 n;` |
|     29 | 4774 | `	if( nArg < 1 ){` |
|      - | 4775 | `		/* Missing arguments,return NULL */` |
|    ! 0 | 4776 | `		ph7_result_null(pCtx);` |
|    ! 0 | 4777 | `		return PH7_OK;` |
|      - | 4778 | `	}` |
|      - | 4779 | `	/* Make sure we are dealing with a valid hashmap */` |
|     29 | 4780 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|      - | 4781 | `		/* Invalid argument,return NULL */` |
|      5 | 4782 | `		ph7_result_null(pCtx);` |
|      5 | 4783 | `		return PH7_OK;` |
|      - | 4784 | `	}` |
|      - | 4785 | `	/* Point to the internal representation of the input hashmap */` |
|     25 | 4786 | `	pSrc = (ph7_hashmap *)apArg[0]->x.pOther;` |
|      - | 4787 | `	/* Create a new array */` |
|     25 | 4788 | `	pArray = ph7_context_new_array(pCtx);` |
|     25 | 4789 | `	if( pArray == 0 ){` |
|    ! 0 | 4790 | `		ph7_result_null(pCtx);` |
|    ! 0 | 4791 | `		return PH7_OK;` |
|      - | 4792 | `	}` |
|      - | 4793 | `	/* Start processing */` |
|     25 | 4794 | `	pEntry = pSrc->pFirst;` |
|  22259 | 4795 | `	for( n = 0 ; n < pSrc->nEntry ; n++ ){` |
|      - | 4796 | `		/* Extract the node value */` |
|  22235 | 4797 | `		pKey = HashmapExtractNodeValue(pEntry);` |
|  22235 | 4798 | `		if( pKey && (pKey->iFlags & MEMOBJ_NULL) == 0){` |
|      - | 4799 | `			/* Prepare the value for insertion */` |
|  22233 | 4800 | `			if( pEntry->iType == HASHMAP_INT_NODE ){` |
|  20001 | 4801 | `				PH7_MemObjInitFromInt(pSrc->pVm,&sVal,pEntry->xKey.iKey);` |
|  10001 | 4802 | `			}else{` |
|      - | 4803 | `				SyString sStr;` |
|   2233 | 4804 | `				SyStringInitFromBuf(&sStr,SyBlobData(&pEntry->xKey.sKey),SyBlobLength(&pEntry->xKey.sKey));` |
|   2233 | 4805 | `				PH7_MemObjInitFromString(pSrc->pVm,&sVal,&sStr);` |
|      - | 4806 | `			}` |
|      - | 4807 | `			/* Perform the insertion */` |
|  22233 | 4808 | `			ph7_array_add_elem(pArray,pKey,&sVal);` |
|      - | 4809 | `			/* Safely release the value because each inserted entry` |
|      - | 4810 | `			 * have it's own private copy of the value.` |
|      - | 4811 | `			 */` |
|  22233 | 4812 | `			PH7_MemObjRelease(&sVal);` |
|  11116 | 4813 | `		}` |
|      - | 4814 | `		/* Point to the next entry */` |
|  22235 | 4815 | `		pEntry = pEntry->pPrev; /* Reverse link */` |
|  11118 | 4816 | `	}` |
|      - | 4817 | `	/* Return the freshly created array */` |
|     25 | 4818 | `	ph7_result_value(pCtx,pArray);` |
|     25 | 4819 | `	return PH7_OK;` |
|     15 | 4820 |  |
|      - | 4821 | `/*` |
|      - | 4822 | ` * number array_sum(array $array )` |
|      - | 4823 | ` *  Calculate the sum of values in an array.` |
|      - | 4824 | ` * Parameters` |
|      - | 4825 | ` *  $array: The input array.` |
|      - | 4826 | ` * Return` |
|      - | 4827 | ` *  Returns the sum of values as an integer or float.` |
|      - | 4828 | ` */` |
|      4 | 4829 | `static void DoubleSum(ph7_context *pCtx,ph7_hashmap *pMap)` |
|      1 | 4830 |  |
|      - | 4831 | `	ph7_hashmap_node *pEntry;` |
|      - | 4832 | `	ph7_value *pObj;` |
|      5 | 4833 | `	double dSum = 0;` |
|      - | 4834 | `	sxu32 n;` |
|      5 | 4835 | `	pEntry = pMap->pFirst;` |
|     19 | 4836 | `	for( n = 0 ; n < pMap->nEntry ; n++ ){` |
|     15 | 4837 | `		pObj = HashmapExtractNodeValue(pEntry);` |
|     15 | 4838 | `		if( pObj && (pObj->iFlags & (MEMOBJ_NULL\|MEMOBJ_HASHMAP\|MEMOBJ_OBJ\|MEMOBJ_RES)) == 0){` |
|     15 | 4839 | `			if( pObj->iFlags & MEMOBJ_REAL ){` |
|     15 | 4840 | `				dSum += pObj->rVal;` |
|      7 | 4841 | `			}else if( pObj->iFlags & (MEMOBJ_INT\|MEMOBJ_BOOL) ){` |
|    ! 0 | 4842 | `				dSum += (double)pObj->x.iVal;` |
|    ! 0 | 4843 | `			}else if( pObj->iFlags & MEMOBJ_STRING ){` |
|    ! 0 | 4844 | `				if( SyBlobLength(&pObj->sBlob) > 0 ){` |
|    ! 0 | 4845 | `					double dv = 0;` |
|    ! 0 | 4846 | `					SyStrToReal((const char *)SyBlobData(&pObj->sBlob),SyBlobLength(&pObj->sBlob),(void *)&dv,0);` |
|    ! 0 | 4847 | `					dSum += dv;` |
|    ! 0 | 4848 | `				}` |
|    ! 0 | 4849 | `			}` |
|      7 | 4850 | `		}` |
|      - | 4851 | `		/* Point to the next entry */` |
|     15 | 4852 | `		pEntry = pEntry->pPrev; /* Reverse link */` |
|      8 | 4853 | `	}` |
|      - | 4854 | `	/* Return sum */` |
|      5 | 4855 | `	ph7_result_double(pCtx,dSum);` |
|      5 | 4856 |  |
|      6 | 4857 | `static void Int64Sum(ph7_context *pCtx,ph7_hashmap *pMap)` |
|      2 | 4858 |  |
|      - | 4859 | `	ph7_hashmap_node *pEntry;` |
|      - | 4860 | `	ph7_value *pObj;` |
|      8 | 4861 | `	sxi64 nSum = 0;` |
|      - | 4862 | `	sxu32 n;` |
|      8 | 4863 | `	pEntry = pMap->pFirst;` |
|     34 | 4864 | `	for( n = 0 ; n < pMap->nEntry ; n++ ){` |
|     28 | 4865 | `		pObj = HashmapExtractNodeValue(pEntry);` |
|     28 | 4866 | `		if( pObj && (pObj->iFlags & (MEMOBJ_NULL\|MEMOBJ_HASHMAP\|MEMOBJ_OBJ\|MEMOBJ_RES)) == 0){` |
|     28 | 4867 | `			if( pObj->iFlags & MEMOBJ_REAL ){` |
|    ! 0 | 4868 | `				nSum += (sxi64)pObj->rVal;` |
|     28 | 4869 | `			}else if( pObj->iFlags & (MEMOBJ_INT\|MEMOBJ_BOOL) ){` |
|     28 | 4870 | `				nSum += pObj->x.iVal;` |
|     13 | 4871 | `			}else if( pObj->iFlags & MEMOBJ_STRING ){` |
|    ! 0 | 4872 | `				if( SyBlobLength(&pObj->sBlob) > 0 ){` |
|    ! 0 | 4873 | `					sxi64 nv = 0;` |
|    ! 0 | 4874 | `					SyStrToInt64((const char *)SyBlobData(&pObj->sBlob),SyBlobLength(&pObj->sBlob),(void *)&nv,0);` |
|    ! 0 | 4875 | `					nSum += nv;` |
|    ! 0 | 4876 | `				}` |
|    ! 0 | 4877 | `			}` |
|     13 | 4878 | `		}` |
|      - | 4879 | `		/* Point to the next entry */` |
|     28 | 4880 | `		pEntry = pEntry->pPrev; /* Reverse link */` |
|     15 | 4881 | `	}` |
|      - | 4882 | `	/* Return sum */` |
|      8 | 4883 | `	ph7_result_int64(pCtx,nSum);` |
|      8 | 4884 |  |
|      - | 4885 | `/* number array_sum(array $array )` |
|      - | 4886 | ` * (See block-coment above)` |
|      - | 4887 | ` */` |
|     16 | 4888 | `static int ph7_hashmap_sum(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      2 | 4889 |  |
|      - | 4890 | `	ph7_hashmap *pMap;` |
|      - | 4891 | `	ph7_value *pObj;` |
|     18 | 4892 | `	if( nArg < 1 ){` |
|      - | 4893 | `		/* Missing arguments,return 0 */` |
|      3 | 4894 | `		ph7_result_int(pCtx,0);` |
|      3 | 4895 | `		return PH7_OK;` |
|      - | 4896 | `	}` |
|      - | 4897 | `	/* Make sure we are dealing with a valid hashmap */` |
|     16 | 4898 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|      - | 4899 | `		/* Invalid argument,return 0 */` |
|      5 | 4900 | `		ph7_result_int(pCtx,0);` |
|      5 | 4901 | `		return PH7_OK;` |
|      - | 4902 | `	}` |
|     12 | 4903 | `	pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|     12 | 4904 | `	if( pMap->nEntry < 1 ){` |
|      - | 4905 | `		/* Nothing to compute,return 0 */` |
|    ! 0 | 4906 | `		ph7_result_int(pCtx,0);` |
|    ! 0 | 4907 | `		return PH7_OK;` |
|      - | 4908 | `	}` |
|      - | 4909 | `	/* If the first element is of type float,then perform floating` |
|      - | 4910 | `	 * point computaion.Otherwise switch to int64 computaion.` |
|      - | 4911 | `	 */` |
|     12 | 4912 | `	pObj = HashmapExtractNodeValue(pMap->pFirst);` |
|     12 | 4913 | `	if( pObj == 0 ){` |
|    ! 0 | 4914 | `		ph7_result_int(pCtx,0);` |
|    ! 0 | 4915 | `		return PH7_OK;` |
|      - | 4916 | `	}` |
|     12 | 4917 | `	if( pObj->iFlags & MEMOBJ_REAL ){` |
|      5 | 4918 | `		DoubleSum(pCtx,pMap);` |
|      3 | 4919 | `	}else{` |
|      8 | 4920 | `		Int64Sum(pCtx,pMap);` |
|      - | 4921 | `	}` |
|     12 | 4922 | `	return PH7_OK;` |
|     10 | 4923 |  |
|      - | 4924 | `/*` |
|      - | 4925 | ` * number array_product(array $array )` |
|      - | 4926 | ` *  Calculate the product of values in an array.` |
|      - | 4927 | ` * Parameters` |
|      - | 4928 | ` *  $array: The input array.` |
|      - | 4929 | ` * Return` |
|      - | 4930 | ` *  Returns the product of values as an integer or float.` |
|      - | 4931 | ` */` |
|    ! 0 | 4932 | `static void DoubleProd(ph7_context *pCtx,ph7_hashmap *pMap)` |
|    ! 0 | 4933 |  |
|      - | 4934 | `	ph7_hashmap_node *pEntry;` |
|      - | 4935 | `	ph7_value *pObj;` |
|      - | 4936 | `	double dProd;` |
|      - | 4937 | `	sxu32 n;` |
|    ! 0 | 4938 | `	pEntry = pMap->pFirst;` |
|    ! 0 | 4939 | `	dProd = 1;` |
|    ! 0 | 4940 | `	for( n = 0 ; n < pMap->nEntry ; n++ ){` |
|    ! 0 | 4941 | `		pObj = HashmapExtractNodeValue(pEntry);` |
|    ! 0 | 4942 | `		if( pObj && (pObj->iFlags & (MEMOBJ_NULL\|MEMOBJ_HASHMAP\|MEMOBJ_OBJ\|MEMOBJ_RES)) == 0){` |
|    ! 0 | 4943 | `			if( pObj->iFlags & MEMOBJ_REAL ){` |
|    ! 0 | 4944 | `				dProd *= pObj->rVal;` |
|    ! 0 | 4945 | `			}else if( pObj->iFlags & (MEMOBJ_INT\|MEMOBJ_BOOL) ){` |
|    ! 0 | 4946 | `				dProd *= (double)pObj->x.iVal;` |
|    ! 0 | 4947 | `			}else if( pObj->iFlags & MEMOBJ_STRING ){` |
|    ! 0 | 4948 | `				if( SyBlobLength(&pObj->sBlob) > 0 ){` |
|    ! 0 | 4949 | `					double dv = 0;` |
|    ! 0 | 4950 | `					SyStrToReal((const char *)SyBlobData(&pObj->sBlob),SyBlobLength(&pObj->sBlob),(void *)&dv,0);` |
|    ! 0 | 4951 | `					dProd *= dv;` |
|    ! 0 | 4952 | `				}` |
|    ! 0 | 4953 | `			}` |
|    ! 0 | 4954 | `		}` |
|      - | 4955 | `		/* Point to the next entry */` |
|    ! 0 | 4956 | `		pEntry = pEntry->pPrev; /* Reverse link */` |
|    ! 0 | 4957 | `	}` |
|      - | 4958 | `	/* Return product */` |
|    ! 0 | 4959 | `	ph7_result_double(pCtx,dProd);` |
|    ! 0 | 4960 |  |
|      2 | 4961 | `static void Int64Prod(ph7_context *pCtx,ph7_hashmap *pMap)` |
|      1 | 4962 |  |
|      - | 4963 | `	ph7_hashmap_node *pEntry;` |
|      - | 4964 | `	ph7_value *pObj;` |
|      - | 4965 | `	sxi64 nProd;` |
|      - | 4966 | `	sxu32 n;` |
|      3 | 4967 | `	pEntry = pMap->pFirst;` |
|      3 | 4968 | `	nProd = 1;` |
|      9 | 4969 | `	for( n = 0 ; n < pMap->nEntry ; n++ ){` |
|      7 | 4970 | `		pObj = HashmapExtractNodeValue(pEntry);` |
|      7 | 4971 | `		if( pObj && (pObj->iFlags & (MEMOBJ_NULL\|MEMOBJ_HASHMAP\|MEMOBJ_OBJ\|MEMOBJ_RES)) == 0){` |
|      7 | 4972 | `			if( pObj->iFlags & MEMOBJ_REAL ){` |
|    ! 0 | 4973 | `				nProd *= (sxi64)pObj->rVal;` |
|      7 | 4974 | `			}else if( pObj->iFlags & (MEMOBJ_INT\|MEMOBJ_BOOL) ){` |
|      7 | 4975 | `				nProd *= pObj->x.iVal;` |
|      3 | 4976 | `			}else if( pObj->iFlags & MEMOBJ_STRING ){` |
|    ! 0 | 4977 | `				if( SyBlobLength(&pObj->sBlob) > 0 ){` |
|    ! 0 | 4978 | `					sxi64 nv = 0;` |
|    ! 0 | 4979 | `					SyStrToInt64((const char *)SyBlobData(&pObj->sBlob),SyBlobLength(&pObj->sBlob),(void *)&nv,0);` |
|    ! 0 | 4980 | `					nProd *= nv;` |
|    ! 0 | 4981 | `				}` |
|    ! 0 | 4982 | `			}` |
|      3 | 4983 | `		}` |
|      - | 4984 | `		/* Point to the next entry */` |
|      7 | 4985 | `		pEntry = pEntry->pPrev; /* Reverse link */` |
|      4 | 4986 | `	}` |
|      - | 4987 | `	/* Return product */` |
|      3 | 4988 | `	ph7_result_int64(pCtx,nProd);` |
|      3 | 4989 |  |
|      - | 4990 | `/* number array_product(array $array )` |
|      - | 4991 | ` * (See block-block comment above)` |
|      - | 4992 | ` */` |
|      2 | 4993 | `static int ph7_hashmap_product(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 4994 |  |
|      - | 4995 | `	ph7_hashmap *pMap;` |
|      - | 4996 | `	ph7_value *pObj;` |
|      3 | 4997 | `	if( nArg < 1 ){` |
|      - | 4998 | `		/* Missing arguments,return 0 */` |
|    ! 0 | 4999 | `		ph7_result_int(pCtx,0);` |
|    ! 0 | 5000 | `		return PH7_OK;` |
|      - | 5001 | `	}` |
|      - | 5002 | `	/* Make sure we are dealing with a valid hashmap */` |
|      3 | 5003 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|      - | 5004 | `		/* Invalid argument,return 0 */` |
|    ! 0 | 5005 | `		ph7_result_int(pCtx,0);` |
|    ! 0 | 5006 | `		return PH7_OK;` |
|      - | 5007 | `	}` |
|      3 | 5008 | `	pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|      3 | 5009 | `	if( pMap->nEntry < 1 ){` |
|      - | 5010 | `		/* Nothing to compute,return 0 */` |
|    ! 0 | 5011 | `		ph7_result_int(pCtx,0);` |
|    ! 0 | 5012 | `		return PH7_OK;` |
|      - | 5013 | `	}` |
|      - | 5014 | `	/* If the first element is of type float,then perform floating` |
|      - | 5015 | `	 * point computaion.Otherwise switch to int64 computaion.` |
|      - | 5016 | `	 */` |
|      3 | 5017 | `	pObj = HashmapExtractNodeValue(pMap->pFirst);` |
|      3 | 5018 | `	if( pObj == 0 ){` |
|    ! 0 | 5019 | `		ph7_result_int(pCtx,0);` |
|    ! 0 | 5020 | `		return PH7_OK;` |
|      - | 5021 | `	}` |
|      3 | 5022 | `	if( pObj->iFlags & MEMOBJ_REAL ){` |
|    ! 0 | 5023 | `		DoubleProd(pCtx,pMap);` |
|    ! 0 | 5024 | `	}else{` |
|      3 | 5025 | `		Int64Prod(pCtx,pMap);` |
|      - | 5026 | `	}` |
|      3 | 5027 | `	return PH7_OK;` |
|      2 | 5028 |  |
|      - | 5029 | `/*` |
|      - | 5030 | ` * value array_rand(array $input[,int $num_req = 1 ])` |
|      - | 5031 | ` *  Pick one or more random entries out of an array.` |
|      - | 5032 | ` * Parameters` |
|      - | 5033 | ` * $input` |
|      - | 5034 | ` *  The input array.` |
|      - | 5035 | ` * $num_req` |
|      - | 5036 | ` *  Specifies how many entries you want to pick.` |
|      - | 5037 | ` * Return` |
|      - | 5038 | ` *  If you are picking only one entry, array_rand() returns the key for a random entry.` |
|      - | 5039 | ` *  Otherwise, it returns an array of keys for the random entries.` |
|      - | 5040 | ` *  NULL is returned on failure.` |
|      - | 5041 | ` */` |
|      6 | 5042 | `static int ph7_hashmap_rand(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 5043 |  |
|      - | 5044 | `	ph7_hashmap_node *pNode;` |
|      - | 5045 | `	ph7_hashmap *pMap;` |
|      7 | 5046 | `	int nItem = 1;` |
|      7 | 5047 | `	if( nArg < 1 ){` |
|      - | 5048 | `		/* Missing argument,return NULL */` |
|    ! 0 | 5049 | `		ph7_result_null(pCtx);` |
|    ! 0 | 5050 | `		return PH7_OK;` |
|      - | 5051 | `	}` |
|      - | 5052 | `	/* Make sure we are dealing with an array */` |
|      7 | 5053 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|    ! 0 | 5054 | `		ph7_result_null(pCtx);` |
|    ! 0 | 5055 | `		return PH7_OK;` |
|      - | 5056 | `	}` |
|      - | 5057 | `	/* Point to the internal representation of the input hashmap */` |
|      7 | 5058 | `	pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|      7 | 5059 | `	if(pMap->nEntry < 1 ){` |
|      - | 5060 | `		/* Empty hashmap,return NULL */` |
|    ! 0 | 5061 | `		ph7_result_null(pCtx);` |
|    ! 0 | 5062 | `		return PH7_OK;` |
|      - | 5063 | `	}` |
|      7 | 5064 | `	if( nArg > 1 ){` |
|      3 | 5065 | `		nItem = ph7_value_to_int(apArg[1]);` |
|      1 | 5066 | `	}` |
|      7 | 5067 | `	if( nItem < 2 ){` |
|      - | 5068 | `		sxu32 nEntry;` |
|      - | 5069 | `		/* Select a random number */` |
|      5 | 5070 | `		nEntry = PH7_VmRandomNum(pMap->pVm) % pMap->nEntry;` |
|      - | 5071 | `		/* Extract the desired entry.` |
|      - | 5072 | `		 * Note that we perform a linear lookup here (later version must change this)` |
|      - | 5073 | `		 */` |
|      5 | 5074 | `		if( nEntry > pMap->nEntry / 2 ){` |
|      1 | 5075 | `			pNode = pMap->pLast;` |
|      1 | 5076 | `			nEntry = pMap->nEntry - nEntry;` |
|      1 | 5077 | `			if( nEntry > 1 ){` |
|    ! 0 | 5078 | `				for(;;){` |
|    ! 0 | 5079 | `					if( nEntry == 0 ){` |
|    ! 0 | 5080 | `						break;` |
|      - | 5081 | `					}` |
|      - | 5082 | `					/* Point to the previous entry */` |
|    ! 0 | 5083 | `					pNode = pNode->pNext; /* Reverse link */` |
|    ! 0 | 5084 | `					nEntry--;` |
|    ! 0 | 5085 | `				}` |
|    ! 0 | 5086 | `			}` |
|      1 | 5087 | `		}else{` |
|      5 | 5088 | `			pNode = pMap->pFirst;` |
|      3 | 5089 | `			for(;;){` |
|      7 | 5090 | `				if( nEntry == 0 ){` |
|      5 | 5091 | `					break;` |
|      - | 5092 | `				}` |
|      - | 5093 | `				/* Point to the next entry */` |
|      3 | 5094 | `				pNode = pNode->pPrev; /* Reverse link */` |
|      3 | 5095 | `				nEntry--;` |
|      1 | 5096 | `			}` |
|      - | 5097 | `		}` |
|      5 | 5098 | `		if( pNode->iType == HASHMAP_INT_NODE ){` |
|      - | 5099 | `			/* Int key */` |
|      3 | 5100 | `			ph7_result_int64(pCtx,pNode->xKey.iKey);` |
|      2 | 5101 | `		}else{` |
|      - | 5102 | `			/* Blob key */` |
|      3 | 5103 | `			ph7_result_string(pCtx,(const char *)SyBlobData(&pNode->xKey.sKey),(int)SyBlobLength(&pNode->xKey.sKey));` |
|      - | 5104 | `		}` |
|      3 | 5105 | `	}else{` |
|      - | 5106 | `		ph7_value sKey,*pArray;` |
|      - | 5107 | `		ph7_hashmap *pDest;` |
|      - | 5108 | `		/* Create a new array */` |
|      3 | 5109 | `		pArray = ph7_context_new_array(pCtx);` |
|      3 | 5110 | `		if( pArray == 0 ){` |
|    ! 0 | 5111 | `			ph7_result_null(pCtx);` |
|    ! 0 | 5112 | `			return PH7_OK;` |
|      - | 5113 | `		}` |
|      - | 5114 | `		/* Point to the internal representation of the hashmap */` |
|      3 | 5115 | `		pDest = (ph7_hashmap *)pArray->x.pOther;` |
|      3 | 5116 | `		PH7_MemObjInit(pDest->pVm,&sKey);` |
|      - | 5117 | `		/* Copy the first n items */` |
|      3 | 5118 | `		pNode = pMap->pFirst;` |
|      3 | 5119 | `		if( nItem > (int)pMap->nEntry ){` |
|    ! 0 | 5120 | `			nItem = (int)pMap->nEntry;` |
|    ! 0 | 5121 | `		}` |
|      7 | 5122 | `		while( nItem > 0){` |
|      5 | 5123 | `			PH7_HashmapExtractNodeKey(pNode,&sKey);` |
|      5 | 5124 | `			PH7_HashmapInsert(pDest,0/* Automatic index assign*/,&sKey);` |
|      5 | 5125 | `			PH7_MemObjRelease(&sKey);` |
|      - | 5126 | `			/* Point to the next entry */` |
|      5 | 5127 | `			pNode = pNode->pPrev; /* Reverse link */` |
|      5 | 5128 | `			nItem--;` |
|      1 | 5129 | `		}` |
|      - | 5130 | `		/* Shuffle the array */` |
|      3 | 5131 | `		HashmapMergeSort(pDest,HashmapCmpCallback7,0);` |
|      - | 5132 | `		/* Rehash node */` |
|      3 | 5133 | `		HashmapSortRehash(pDest);` |
|      - | 5134 | `		/* Return the random array */` |
|      3 | 5135 | `		ph7_result_value(pCtx,pArray);` |
|      - | 5136 | `	}` |
|      7 | 5137 | `	return PH7_OK;` |
|      4 | 5138 |  |
|      - | 5139 | `/*` |
|      - | 5140 | ` * array array_chunk (array $input,int $size [,bool $preserve_keys = false ])` |
|      - | 5141 | ` *  Split an array into chunks.` |
|      - | 5142 | ` * Parameters` |
|      - | 5143 | ` * $input` |
|      - | 5144 | ` *   The array to work on` |
|      - | 5145 | ` * $size` |
|      - | 5146 | ` *   The size of each chunk` |
|      - | 5147 | ` * $preserve_keys` |
|      - | 5148 | ` *   When set to TRUE keys will be preserved. Default is FALSE which will reindex` |
|      - | 5149 | ` *   the chunk numerically.` |
|      - | 5150 | ` * Return` |
|      - | 5151 | ` *  Returns a multidimensional numerically indexed array, starting with` |
|      - | 5152 | ` *  zero, with each dimension containing size elements.` |
|      - | 5153 | ` */` |
|     42 | 5154 | `static int ph7_hashmap_chunk(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      2 | 5155 |  |
|      - | 5156 | `	ph7_value *pArray,*pChunk;` |
|      - | 5157 | `	ph7_hashmap_node *pEntry;` |
|      - | 5158 | `	ph7_hashmap *pMap;` |
|      - | 5159 | `	int bPreserve;` |
|      - | 5160 | `	sxu32 nChunk;` |
|      - | 5161 | `	sxu32 nSize;` |
|      - | 5162 | `	sxu32 n;` |
|      - | 5163 | `	/* Argument count and types follow PHP semantics. */` |
|     44 | 5164 | `	if( nArg < 2 ){` |
|      - | 5165 | `		/* fewer than required arguments -> ArgumentCountError */` |
|      4 | 5166 | `		return PH7_VmThrowException(pCtx,` |
|      - | 5167 | `			"ArgumentCountError",` |
|      - | 5168 | `			"array_chunk() expects at least 2 arguments, %d given",` |
|      1 | 5169 | `			nArg` |
|      - | 5170 | `			);` |
|      - | 5171 | `	}` |
|     42 | 5172 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|      4 | 5173 | `		return PH7_VmThrowException(pCtx,` |
|      - | 5174 | `			"TypeError",` |
|      - | 5175 | `			"array_chunk(): Argument #1 ($array) must be of type array, %s given",` |
|      1 | 5176 | `			ph7_type_name(apArg[0])` |
|      - | 5177 | `			);` |
|      - | 5178 | `	}` |
|      - | 5179 | `	/* Create a new array */` |
|     40 | 5180 | `	pArray = ph7_context_new_array(pCtx);` |
|     40 | 5181 | `	if( pArray == 0 ){` |
|    ! 0 | 5182 | `		ph7_result_null(pCtx);` |
|    ! 0 | 5183 | `		return PH7_OK;` |
|      - | 5184 | `	}` |
|      - | 5185 | `	/* Point to the internal representation of the input hashmap */` |
|     40 | 5186 | `	pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|      - | 5187 | `	/* Extract and validate the chunk size argument. */` |
|      - | 5188 | `	/* Reject types that cannot be sensibly converted to an integer. */` |
|     76 | 5189 | `	if( ph7_value_is_array(apArg[1]) \|\| ph7_value_is_object(apArg[1]) \|\|` |
|     78 | 5190 | `		ph7_value_is_resource(apArg[1]) \|\| ph7_value_is_null(apArg[1]) \|\|` |
|     38 | 5191 | `		ph7_value_is_bool(apArg[1]) ){` |
|    ! 0 | 5192 | `		return PH7_VmThrowException(pCtx,` |
|      - | 5193 | `			"TypeError",` |
|      - | 5194 | `			"array_chunk(): Argument #2 ($length) must be of type int, %s given",` |
|    ! 0 | 5195 | `			ph7_type_name(apArg[1])` |
|      - | 5196 | `			);` |
|      - | 5197 | `	}` |
|      - | 5198 | `	/* Strings that are non-numeric also produce a TypeError. */` |
|     40 | 5199 | `	if( ph7_value_is_string(apArg[1]) ){` |
|      - | 5200 | `		int len;` |
|      3 | 5201 | `		sxu8 bReal = FALSE;` |
|      3 | 5202 | `		const char *zStr = ph7_value_to_string(apArg[1], &len);` |
|      3 | 5203 | `			if( SyStrIsNumeric(zStr, len, &bReal, 0) != SXRET_OK \|\| bReal ){` |
|      3 | 5204 | `			return PH7_VmThrowException(pCtx,` |
|      - | 5205 | `				"TypeError",` |
|      - | 5206 | `				"array_chunk(): Argument #2 ($length) must be of type int, string given"` |
|      - | 5207 | `				);` |
|      - | 5208 | `		}` |
|    ! 0 | 5209 | `	}` |
|      - | 5210 | `	/* If the value is a float with a fractional component, refuse it.` |
|      - | 5211 | `	 * PHP currently warns but may become an error in the future; we` |
|      - | 5212 | `	 * enforce that policy now so PHL behaviour is strict. */` |
|     38 | 5213 | `	if( ph7_value_is_float(apArg[1]) ){` |
|      3 | 5214 | `		double d = ph7_value_to_double(apArg[1]);` |
|      3 | 5215 | `		sxi64 i = (sxi64)d;` |
|      3 | 5216 | `		if( d != (double)i ){` |
|      3 | 5217 | `			return PH7_VmThrowException(pCtx,` |
|      - | 5218 | `				"TypeError",` |
|      - | 5219 | `				"array_chunk(): Argument #2 ($length) must be of type int, float given"` |
|      - | 5220 | `				);` |
|      - | 5221 | `		}` |
|    ! 0 | 5222 | `	}` |
|      - | 5223 | `	/* Convert using ph7_value_to_int; now that float fractions are` |
|      - | 5224 | `	 * eliminated, this will not produce a warning. */` |
|      - | 5225 | `	{` |
|     36 | 5226 | `		sxi64 nSizeSigned = ph7_value_to_int(apArg[1]);` |
|     36 | 5227 | `		if( nSizeSigned < 1 ){` |
|      - | 5228 | `			/* size <= 0 -> ValueError */` |
|      5 | 5229 | `			return PH7_VmThrowException(pCtx,` |
|      - | 5230 | `				"ValueError",` |
|      - | 5231 | `				"array_chunk(): Argument #2 ($length) must be greater than 0"` |
|      - | 5232 | `				);` |
|      - | 5233 | `		}` |
|     32 | 5234 | `		nSize = (sxu32)nSizeSigned;` |
|      - | 5235 | `	}` |
|     32 | 5236 | `	if( nSize >= pMap->nEntry ){` |
|      - | 5237 | `		/* Return the whole array */` |
|      3 | 5238 | `		ph7_array_add_elem(pArray,0,apArg[0]);` |
|      3 | 5239 | `		ph7_result_value(pCtx,pArray);` |
|      3 | 5240 | `		return PH7_OK;` |
|      - | 5241 | `	}` |
|     30 | 5242 | `	bPreserve = 0;` |
|     30 | 5243 | `	if( nArg > 2 ){` |
|      - | 5244 | `		/* The third argument has a bool type hint in PHP.  Values that` |
|      - | 5245 | `		 * cannot be sensibly converted (arrays, objects, resources) are` |
|      - | 5246 | `		 * rejected with a TypeError.  Scalars and null coerce to bool` |
|      - | 5247 | `		 * normally, matching PHP behaviour. */` |
|     45 | 5248 | `		if( ph7_value_is_array(apArg[2]) \|\|` |
|     34 | 5249 | `			ph7_value_is_object(apArg[2]) \|\|` |
|     20 | 5250 | `			ph7_value_is_resource(apArg[2]) ){` |
|      7 | 5251 | `			return PH7_VmThrowException(pCtx,` |
|      - | 5252 | `				"TypeError",` |
|      - | 5253 | `				"array_chunk(): Argument #3 ($preserve_keys) must be of type bool, %s given",` |
|      4 | 5254 | `				ph7_type_name(apArg[2])` |
|      - | 5255 | `				);` |
|      - | 5256 | `		}` |
|     21 | 5257 | `		bPreserve = ph7_value_to_bool(apArg[2]);` |
|     10 | 5258 | `	}` |
|      - | 5259 | `	/* Start processing */` |
|     25 | 5260 | `	pEntry = pMap->pFirst;` |
|     25 | 5261 | `	nChunk = 0;` |
|     25 | 5262 | `	pChunk = 0;` |
|     25 | 5263 | `	n = pMap->nEntry;` |
|     51 | 5264 | `	for( ;; ){` |
|    103 | 5265 | `		if( n < 1 ){` |
|      - | 5266 | `			/* When the loop terminates we may still have a current chunk` |
|      - | 5267 | `			 * that hasn't been added to the result array.  The previous` |
|      - | 5268 | `			 * implementation only pushed it if nChunk>0 which dropped the` |
|      - | 5269 | `			 * final chunk when the input size was an exact multiple of` |
|      - | 5270 | `			 * the chunk length.  Always append the pending chunk if it` |
|      - | 5271 | `			 * exists. */` |
|     25 | 5272 | `			if( pChunk ){` |
|     25 | 5273 | `				ph7_array_add_elem(pArray,0,pChunk); /* Will have its own copy */` |
|     12 | 5274 | `			}` |
|     25 | 5275 | `			break;` |
|      - | 5276 | `		}` |
|     79 | 5277 | `		if( nChunk < 1 ){` |
|     67 | 5278 | `			if( pChunk ){` |
|      - | 5279 | `				/* Put the first chunk */` |
|     43 | 5280 | `				ph7_array_add_elem(pArray,0,pChunk); /* Will have it's own copy */` |
|     21 | 5281 | `			}` |
|      - | 5282 | `			/* Create a new dimension */` |
|     67 | 5283 | `			pChunk = ph7_context_new_array(pCtx); /* Don't worry about freeing memory here,everything` |
|      - | 5284 | `												   * will be automatically released as soon we return` |
|      - | 5285 | `												   * from this function */` |
|     67 | 5286 | `			if( pChunk == 0 ){` |
|    ! 0 | 5287 | `				break;` |
|      - | 5288 | `			}` |
|     67 | 5289 | `			nChunk = nSize;` |
|     33 | 5290 | `		}` |
|      - | 5291 | `		/* Insert the entry */` |
|     79 | 5292 | `		HashmapInsertNode((ph7_hashmap *)pChunk->x.pOther,pEntry,bPreserve);` |
|      - | 5293 | `		/* Point to the next entry */` |
|     79 | 5294 | `		pEntry = pEntry->pPrev; /* Reverse link */` |
|     79 | 5295 | `		nChunk--;` |
|     79 | 5296 | `		n--;` |
|      1 | 5297 | `	}` |
|      - | 5298 | `	/* Return the multidimensional array */` |
|     25 | 5299 | `	ph7_result_value(pCtx,pArray);` |
|     25 | 5300 | `	return PH7_OK;` |
|     23 | 5301 |  |
|      - | 5302 | `/*` |
|      - | 5303 | ` * array array_pad(array $input,int $pad_size,value $pad_value)` |
|      - | 5304 | ` *  Pad array to the specified length with a value.` |
|      - | 5305 | ` * $input` |
|      - | 5306 | ` *   Initial array of values to pad.` |
|      - | 5307 | ` * $pad_size` |
|      - | 5308 | ` *   New size of the array.` |
|      - | 5309 | ` * $pad_value` |
|      - | 5310 | ` *   Value to pad if input is less than pad_size.` |
|      - | 5311 | ` */` |
|      8 | 5312 | `static int ph7_hashmap_pad(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 5313 |  |
|      - | 5314 | `	ph7_hashmap *pMap;` |
|      - | 5315 | `	ph7_value *pArray;` |
|      - | 5316 | `	int nEntry;` |
|      9 | 5317 | `	if( nArg < 3 \|\| !ph7_value_is_array(apArg[0]) ){` |
|      - | 5318 | `		/* Invalid arguments,return NULL */` |
|    ! 0 | 5319 | `		ph7_result_null(pCtx);` |
|    ! 0 | 5320 | `		return PH7_OK;` |
|      - | 5321 | `	}` |
|      - | 5322 | `	/* Create a new array */` |
|      9 | 5323 | `	pArray = ph7_context_new_array(pCtx);` |
|      9 | 5324 | `	if( pArray == 0 ){` |
|    ! 0 | 5325 | `		ph7_result_null(pCtx);` |
|    ! 0 | 5326 | `		return PH7_OK;` |
|      - | 5327 | `	}` |
|      - | 5328 | `	/* Point to the internal representation of the input hashmap */` |
|      9 | 5329 | `	pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|      - | 5330 | `	/* Extract the total number of desired entry to insert */` |
|      9 | 5331 | `	nEntry = ph7_value_to_int(apArg[1]);` |
|      9 | 5332 | `	if( nEntry < 0 ){` |
|      5 | 5333 | `		nEntry = -nEntry;` |
|      5 | 5334 | `		if( nEntry > 1048576 ){` |
|    ! 0 | 5335 | `			nEntry = 1048576; /* Limit imposed by PHP */` |
|    ! 0 | 5336 | `		}` |
|      5 | 5337 | `		if( nEntry > (int)pMap->nEntry ){` |
|      3 | 5338 | `			nEntry -= (int)pMap->nEntry;` |
|      - | 5339 | `			/* Insert given items first */` |
|      7 | 5340 | `			while( nEntry > 0 ){` |
|      5 | 5341 | `				ph7_array_add_elem(pArray,0,apArg[2]);` |
|      5 | 5342 | `				nEntry--;` |
|      1 | 5343 | `			}` |
|      - | 5344 | `			/* Merge the two arrays */` |
|      3 | 5345 | `			HashmapMerge(pMap,(ph7_hashmap *)pArray->x.pOther);` |
|      2 | 5346 | `		}else{` |
|      3 | 5347 | `			PH7_HashmapDup(pMap,(ph7_hashmap *)pArray->x.pOther);` |
|      1 | 5348 | `		}` |
|      7 | 5349 | `	}else if( nEntry > 0 ){` |
|      5 | 5350 | `		if( nEntry > 1048576 ){` |
|    ! 0 | 5351 | `			nEntry = 1048576; /* Limit imposed by PHP */` |
|    ! 0 | 5352 | `		}` |
|      5 | 5353 | `		if( nEntry > (int)pMap->nEntry ){` |
|      3 | 5354 | `			nEntry -= (int)pMap->nEntry;` |
|      - | 5355 | `			/* Merge the two arrays first */` |
|      3 | 5356 | `			HashmapMerge(pMap,(ph7_hashmap *)pArray->x.pOther);` |
|      - | 5357 | `			/* Insert given items */` |
|      7 | 5358 | `			while( nEntry > 0 ){` |
|      5 | 5359 | `				ph7_array_add_elem(pArray,0,apArg[2]);` |
|      5 | 5360 | `				nEntry--;` |
|      1 | 5361 | `			}` |
|      2 | 5362 | `		}else{` |
|      3 | 5363 | `			PH7_HashmapDup(pMap,(ph7_hashmap *)pArray->x.pOther);` |
|      - | 5364 | `		}` |
|      2 | 5365 | `	}` |
|      - | 5366 | `	/* Return the new array */` |
|      9 | 5367 | `	ph7_result_value(pCtx,pArray);` |
|      9 | 5368 | `	return PH7_OK;` |
|      5 | 5369 |  |
|      - | 5370 | `/*` |
|      - | 5371 | ` * array array_replace(array &$array,array &$array1,...)` |
|      - | 5372 | ` *  Replaces elements from passed arrays into the first array.` |
|      - | 5373 | ` * Parameters` |
|      - | 5374 | ` * $array` |
|      - | 5375 | ` *   The array in which elements are replaced.` |
|      - | 5376 | ` * $array1` |
|      - | 5377 | ` *   The array from which elements will be extracted.` |
|      - | 5378 | ` * ....` |
|      - | 5379 | ` *  More arrays from which elements will be extracted.` |
|      - | 5380 | ` *  Values from later arrays overwrite the previous values.` |
|      - | 5381 | ` * Return` |
|      - | 5382 | ` *  Returns an array, or NULL if an error occurs.` |
|      - | 5383 | ` */` |
|      2 | 5384 | `static int ph7_hashmap_replace(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 5385 |  |
|      - | 5386 | `	ph7_hashmap *pMap;` |
|      - | 5387 | `	ph7_value *pArray;` |
|      - | 5388 | `	int i;` |
|      3 | 5389 | `	if( nArg < 1 ){` |
|      - | 5390 | `		/* Invalid arguments,return NULL */` |
|    ! 0 | 5391 | `		ph7_result_null(pCtx);` |
|    ! 0 | 5392 | `		return PH7_OK;` |
|      - | 5393 | `	}` |
|      - | 5394 | `	/* Create a new array */` |
|      3 | 5395 | `	pArray = ph7_context_new_array(pCtx);` |
|      3 | 5396 | `	if( pArray == 0 ){` |
|    ! 0 | 5397 | `		ph7_result_null(pCtx);` |
|    ! 0 | 5398 | `		return PH7_OK;` |
|      - | 5399 | `	}` |
|      - | 5400 | `	/* Perform the requested operation */` |
|      7 | 5401 | `	for( i = 0 ; i < nArg ; i++ ){` |
|      5 | 5402 | `		if( !ph7_value_is_array(apArg[i]) ){` |
|    ! 0 | 5403 | `			continue;` |
|      - | 5404 | `		}` |
|      - | 5405 | `		/* Point to the internal representation of the input hashmap */` |
|      5 | 5406 | `		pMap = (ph7_hashmap *)apArg[i]->x.pOther;` |
|      5 | 5407 | `		HashmapOverwrite(pMap,(ph7_hashmap *)pArray->x.pOther);` |
|      3 | 5408 | `	}` |
|      - | 5409 | `	/* Return the new array */` |
|      3 | 5410 | `	ph7_result_value(pCtx,pArray);` |
|      3 | 5411 | `	return PH7_OK;` |
|      2 | 5412 |  |
|      - | 5413 | `/*` |
|      - | 5414 | ` * array array_filter(array $input [,callback $callback ])` |
|      - | 5415 | ` *  Filters elements of an array using a callback function.` |
|      - | 5416 | ` * Parameters` |
|      - | 5417 | ` *  $input` |
|      - | 5418 | ` *    The array to iterate over` |
|      - | 5419 | ` * $callback` |
|      - | 5420 | ` *    The callback function to use` |
|      - | 5421 | ` *    If no callback is supplied, all entries of input equal to FALSE (see converting to boolean)` |
|      - | 5422 | ` *    will be removed.` |
|      - | 5423 | ` * Return` |
|      - | 5424 | ` *  The filtered array.` |
|      - | 5425 | ` */` |
|      8 | 5426 | `static int ph7_hashmap_filter(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 5427 |  |
|      - | 5428 | `	ph7_hashmap_node *pEntry;` |
|      - | 5429 | `	ph7_hashmap *pMap;` |
|      - | 5430 | `	ph7_value *pArray;` |
|      - | 5431 | `	ph7_value sResult;   /* Callback result */` |
|      - | 5432 | `	ph7_value *pValue;` |
|      - | 5433 | `	sxi32 rc;` |
|      - | 5434 | `	int keep;` |
|      - | 5435 | `	sxu32 n;` |
|      9 | 5436 | `	if( nArg < 1 \|\| !ph7_value_is_array(apArg[0]) ){` |
|      - | 5437 | `		/* Invalid arguments,return NULL */` |
|      5 | 5438 | `		ph7_result_null(pCtx);` |
|      5 | 5439 | `		return PH7_OK;` |
|      - | 5440 | `	}` |
|      - | 5441 | `	/* Create a new array */` |
|      5 | 5442 | `	pArray = ph7_context_new_array(pCtx);` |
|      5 | 5443 | `	if( pArray == 0 ){` |
|    ! 0 | 5444 | `		ph7_result_null(pCtx);` |
|    ! 0 | 5445 | `		return PH7_OK;` |
|      - | 5446 | `	}` |
|      - | 5447 | `	/* Point to the internal representation of the input hashmap */` |
|      5 | 5448 | `	pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|      5 | 5449 | `	pEntry = pMap->pFirst;` |
|      5 | 5450 | `	PH7_MemObjInit(pMap->pVm,&sResult);` |
|      5 | 5451 | `	sResult.nIdx = SXU32_HIGH; /* Mark as constant */` |
|      - | 5452 | `	/* Perform the requested operation */` |
|     21 | 5453 | `	for( n = 0 ; n < pMap->nEntry ; n++ ){` |
|      - | 5454 | `		/* Extract node value */` |
|     17 | 5455 | `		pValue = HashmapExtractNodeValue(pEntry);` |
|     17 | 5456 | `		if( nArg > 1 && pValue ){` |
|      - | 5457 | `			/* Invoke the given callback */` |
|     17 | 5458 | `			keep = FALSE;` |
|     17 | 5459 | `			rc = PH7_VmCallUserFunction(pMap->pVm,apArg[1],1,&pValue,&sResult);` |
|     17 | 5460 | `			if( rc == SXRET_OK ){` |
|      - | 5461 | `				/* Perform a boolean cast */` |
|     17 | 5462 | `				keep = ph7_value_to_bool(&sResult);` |
|      8 | 5463 | `			}` |
|     17 | 5464 | `			PH7_MemObjRelease(&sResult);` |
|      9 | 5465 | `		}else{` |
|      - | 5466 | `			/* No available callback,check for empty item */` |
|    ! 0 | 5467 | `			keep = !PH7_MemObjIsEmpty(pValue);` |
|      - | 5468 | `		}` |
|     17 | 5469 | `		if( keep ){` |
|      - | 5470 | `			/* Perform the insertion,now the callback returned true */` |
|      5 | 5471 | `			HashmapInsertNode((ph7_hashmap *)pArray->x.pOther,pEntry,TRUE);` |
|      2 | 5472 | `		}` |
|      - | 5473 | `		/* Point to the next entry */` |
|     17 | 5474 | `		pEntry = pEntry->pPrev; /* Reverse link */` |
|      9 | 5475 | `	}` |
|      5 | 5476 | `	ph7_result_value(pCtx,pArray);` |
|      5 | 5477 | `	return PH7_OK;` |
|      5 | 5478 |  |
|      - | 5479 | `/*` |
|      - | 5480 | ` * array array_map(callback $callback,array $arr1)` |
|      - | 5481 | ` *  Applies the callback to the elements of the given arrays.` |
|      - | 5482 | ` * Parameters` |
|      - | 5483 | ` *  $callback` |
|      - | 5484 | ` *   Callback function to run for each element in each array.` |
|      - | 5485 | ` * $arr1` |
|      - | 5486 | ` *   An array to run through the callback function.` |
|      - | 5487 | ` * Return` |
|      - | 5488 | ` *  Returns an array containing all the elements of arr1 after applying` |
|      - | 5489 | ` *  the callback function to each one.` |
|      - | 5490 | ` * NOTE:` |
|      - | 5491 | ` *  array_map() passes only a single value to the callback.` |
|      - | 5492 | ` */` |
|     10 | 5493 | `static int ph7_hashmap_map(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 5494 |  |
|      - | 5495 | `	ph7_value *pArray,*pValue,sKey,sResult;` |
|      - | 5496 | `	ph7_hashmap_node *pEntry;` |
|      - | 5497 | `	ph7_hashmap *pMap;` |
|      - | 5498 | `	sxu32 n;` |
|     11 | 5499 | `	if( nArg < 2 \|\| !ph7_value_is_array(apArg[1]) ){` |
|      - | 5500 | `		/* Invalid arguments,return NULL */` |
|      5 | 5501 | `		ph7_result_null(pCtx);` |
|      5 | 5502 | `		return PH7_OK;` |
|      - | 5503 | `	}` |
|      - | 5504 | `	/* Create a new array */` |
|      7 | 5505 | `	pArray = ph7_context_new_array(pCtx);` |
|      7 | 5506 | `	if( pArray == 0 ){` |
|    ! 0 | 5507 | `		ph7_result_null(pCtx);` |
|    ! 0 | 5508 | `		return PH7_OK;` |
|      - | 5509 | `	}` |
|      - | 5510 | `	/* Point to the internal representation of the input hashmap */` |
|      7 | 5511 | `	pMap = (ph7_hashmap *)apArg[1]->x.pOther;` |
|      7 | 5512 | `	PH7_MemObjInit(pMap->pVm,&sResult);` |
|      7 | 5513 | `	PH7_MemObjInit(pMap->pVm,&sKey);` |
|      7 | 5514 | `	sResult.nIdx = SXU32_HIGH; /* Mark as constant */` |
|      7 | 5515 | `	sKey.nIdx    = SXU32_HIGH; /* Mark as constant */` |
|      - | 5516 | `	/* Perform the requested operation */` |
|      7 | 5517 | `	pEntry = pMap->pFirst;` |
|     21 | 5518 | `	for( n = 0 ; n < pMap->nEntry ; n++ ){` |
|      - | 5519 | `		/* Extrcat the node value */` |
|     15 | 5520 | `		pValue = HashmapExtractNodeValue(pEntry);` |
|     15 | 5521 | `		if( pValue ){` |
|      - | 5522 | `			sxi32 rc;` |
|      - | 5523 | `			/* Invoke the supplied callback */` |
|     15 | 5524 | `			rc = PH7_VmCallUserFunction(pMap->pVm,apArg[0],1,&pValue,&sResult);` |
|      - | 5525 | `			/* Extract the node key */` |
|     15 | 5526 | `			PH7_HashmapExtractNodeKey(pEntry,&sKey);` |
|     15 | 5527 | `			if( rc != SXRET_OK ){` |
|      - | 5528 | `				/* An error occured while invoking the supplied callback [i.e: not defined] */` |
|    ! 0 | 5529 | `				ph7_array_add_elem(pArray,&sKey,pValue); /* Keep the same value */` |
|    ! 0 | 5530 | `			}else{` |
|      - | 5531 | `				/* Insert the callback return value */` |
|     15 | 5532 | `				ph7_array_add_elem(pArray,&sKey,&sResult);` |
|      - | 5533 | `			}` |
|     15 | 5534 | `			PH7_MemObjRelease(&sKey);` |
|     15 | 5535 | `			PH7_MemObjRelease(&sResult);` |
|      7 | 5536 | `		}` |
|      - | 5537 | `		/* Point to the next entry */` |
|     15 | 5538 | `		pEntry = pEntry->pPrev; /* Reverse link */` |
|      8 | 5539 | `	}` |
|      7 | 5540 | `	ph7_result_value(pCtx,pArray);` |
|      7 | 5541 | `	return PH7_OK;` |
|      6 | 5542 |  |
|      - | 5543 | `/*` |
|      - | 5544 | ` * value array_reduce(array $input,callback $function[, value $initial = NULL ])` |
|      - | 5545 | ` *  Iteratively reduce the array to a single value using a callback function.` |
|      - | 5546 | ` * Parameters` |
|      - | 5547 | ` *  $input` |
|      - | 5548 | ` *   The input array.` |
|      - | 5549 | ` *  $function` |
|      - | 5550 | ` *  The callback function.` |
|      - | 5551 | ` * $initial` |
|      - | 5552 | ` *  If the optional initial is available, it will be used at the beginning` |
|      - | 5553 | ` *  of the process, or as a final result in case the array is empty.` |
|      - | 5554 | ` * Return` |
|      - | 5555 | ` *  Returns the resulting value.` |
|      - | 5556 | ` *  If the array is empty and initial is not passed, array_reduce() returns NULL.` |
|      - | 5557 | ` */` |
|      4 | 5558 | `static int ph7_hashmap_reduce(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 5559 |  |
|      - | 5560 | `	ph7_hashmap_node *pEntry;` |
|      - | 5561 | `	ph7_hashmap *pMap;` |
|      - | 5562 | `	ph7_value *pValue;` |
|      - | 5563 | `	ph7_value sResult;` |
|      - | 5564 | `	sxu32 n;` |
|      5 | 5565 | `	if( nArg < 2 \|\| !ph7_value_is_array(apArg[0]) ){` |
|      - | 5566 | `		/* Invalid/Missing arguments,return NULL */` |
|    ! 0 | 5567 | `		ph7_result_null(pCtx);` |
|    ! 0 | 5568 | `		return PH7_OK;` |
|      - | 5569 | `	}` |
|      - | 5570 | `	/* Point to the internal representation of the input hashmap */` |
|      5 | 5571 | `	pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|      - | 5572 | `	/* Assume a NULL initial value */` |
|      5 | 5573 | `	PH7_MemObjInit(pMap->pVm,&sResult);` |
|      5 | 5574 | `	sResult.nIdx = SXU32_HIGH; /* Mark as constant */` |
|      5 | 5575 | `	if( nArg > 2 ){` |
|      - | 5576 | `		/* Set the initial value */` |
|      5 | 5577 | `		PH7_MemObjLoad(apArg[2],&sResult);` |
|      2 | 5578 | `	}` |
|      - | 5579 | `	/* Perform the requested operation */` |
|      5 | 5580 | `	pEntry = pMap->pFirst;` |
|     19 | 5581 | `	for( n = 0 ; n < pMap->nEntry ; n++ ){` |
|      - | 5582 | `		/* Extract the node value */` |
|     15 | 5583 | `		pValue = HashmapExtractNodeValue(pEntry);` |
|      - | 5584 | `		/* Invoke the supplied callback */` |
|     15 | 5585 | `		PH7_VmCallUserFunctionAp(pMap->pVm,apArg[1],&sResult,&sResult,pValue,0);` |
|      - | 5586 | `		/* Point to the next entry */` |
|     15 | 5587 | `		pEntry = pEntry->pPrev; /* Reverse link */` |
|      8 | 5588 | `	}` |
|      5 | 5589 | `	ph7_result_value(pCtx,&sResult); /* Will make it's own copy */` |
|      5 | 5590 | `	PH7_MemObjRelease(&sResult);` |
|      5 | 5591 | `	return PH7_OK;` |
|      3 | 5592 |  |
|      - | 5593 | `/*` |
|      - | 5594 | ` * bool array_walk(array &$array,callback $funcname [, value $userdata ] )` |
|      - | 5595 | ` *  Apply a user function to every member of an array.` |
|      - | 5596 | ` * Parameters` |
|      - | 5597 | ` *  $array` |
|      - | 5598 | ` *   The input array.` |
|      - | 5599 | ` * $funcname` |
|      - | 5600 | ` *  Typically, funcname takes on two parameters.The array parameter's value being` |
|      - | 5601 | ` *  the first, and the key/index second.` |
|      - | 5602 | ` * Note:` |
|      - | 5603 | ` *  If funcname needs to be working with the actual values of the array,specify the first` |
|      - | 5604 | ` *  parameter of funcname as a reference. Then, any changes made to those elements will` |
|      - | 5605 | ` *  be made in the original array itself.` |
|      - | 5606 | ` * $userdata` |
|      - | 5607 | ` *  If the optional userdata parameter is supplied, it will be passed as the third parameter` |
|      - | 5608 | ` *  to the callback funcname.` |
|      - | 5609 | ` * Return` |
|      - | 5610 | ` *  Returns TRUE on success or FALSE on failure.` |
|      - | 5611 | ` */` |
|     12 | 5612 | `static int ph7_hashmap_walk(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 5613 |  |
|      - | 5614 | `	ph7_value *pValue,*pUserData,sKey;` |
|      - | 5615 | `	ph7_hashmap_node *pEntry;` |
|      - | 5616 | `	ph7_hashmap *pMap;` |
|      - | 5617 | `	sxi32 rc;` |
|      - | 5618 | `	sxu32 n;` |
|     13 | 5619 | `	if( nArg < 2 \|\| !ph7_value_is_array(apArg[0]) ){` |
|      - | 5620 | `		/* Invalid/Missing arguments,return FALSE */` |
|    ! 0 | 5621 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 5622 | `		return PH7_OK;` |
|      - | 5623 | `	}` |
|     13 | 5624 | `	pUserData = nArg > 2 ? apArg[2] : 0;` |
|      - | 5625 | `	/* Point to the internal representation of the input hashmap */` |
|     13 | 5626 | `	pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|     13 | 5627 | `	PH7_MemObjInit(pMap->pVm,&sKey);` |
|     13 | 5628 | `	sKey.nIdx = SXU32_HIGH; /* Mark as constant */` |
|      - | 5629 | `	/* Perform the desired operation */` |
|     13 | 5630 | `	pEntry = pMap->pFirst;` |
|     41 | 5631 | `	for( n = 0 ; n < pMap->nEntry ; n++ ){` |
|      - | 5632 | `		/* Extract the node value */` |
|     29 | 5633 | `		pValue = HashmapExtractNodeValue(pEntry);` |
|     29 | 5634 | `		if( pValue ){` |
|      - | 5635 | `			/* Extract the entry key */` |
|     29 | 5636 | `			PH7_HashmapExtractNodeKey(pEntry,&sKey);` |
|      - | 5637 | `			/* Invoke the supplied callback */` |
|     29 | 5638 | `			rc = PH7_VmCallUserFunctionAp(pMap->pVm,apArg[1],0,pValue,&sKey,pUserData,0);` |
|     29 | 5639 | `			PH7_MemObjRelease(&sKey);` |
|     29 | 5640 | `			if( rc != SXRET_OK ){` |
|      - | 5641 | `				/* An error occured while invoking the supplied callback [i.e: not defined] */` |
|    ! 0 | 5642 | `				ph7_result_bool(pCtx,0); /* return FALSE */` |
|    ! 0 | 5643 | `				return PH7_OK;` |
|      - | 5644 | `			}` |
|     14 | 5645 | `		}` |
|      - | 5646 | `		/* Point to the next entry */` |
|     29 | 5647 | `		pEntry = pEntry->pPrev; /* Reverse link */` |
|     15 | 5648 | `	}` |
|      - | 5649 | `	/* All done,return TRUE */` |
|     13 | 5650 | `	ph7_result_bool(pCtx,1);` |
|     13 | 5651 | `	return PH7_OK;` |
|      7 | 5652 |  |
|      - | 5653 | `/*` |
|      - | 5654 | ` * Apply a user function to every member of an array.(Recurse on array's).` |
|      - | 5655 | ` * Refer to the [array_walk_recursive()] implementation for more information.` |
|      - | 5656 | ` */` |
|      6 | 5657 | `static int HashmapWalkRecursive(` |
|      - | 5658 | `	ph7_hashmap *pMap,    /* Target hashmap */` |
|      - | 5659 | `	ph7_value *pCallback, /* User callback */` |
|      - | 5660 | `	ph7_value *pUserData, /* Callback private data */` |
|      - | 5661 | `	int iNest             /* Nesting level */` |
|      - | 5662 | `	)` |
|      1 | 5663 |  |
|      - | 5664 | `	ph7_hashmap_node *pEntry;` |
|      - | 5665 | `	ph7_value *pValue,sKey;` |
|      - | 5666 | `	sxi32 rc;` |
|      - | 5667 | `	sxu32 n;` |
|      - | 5668 | `	/* Iterate throw hashmap entries */` |
|      7 | 5669 | `	PH7_MemObjInit(pMap->pVm,&sKey);` |
|      7 | 5670 | `	sKey.nIdx = SXU32_HIGH; /* Mark as constant */` |
|      7 | 5671 | `	pEntry = pMap->pFirst;` |
|     17 | 5672 | `	for( n = 0 ; n < pMap->nEntry ; n++ ){` |
|      - | 5673 | `		/* Extract the node value */` |
|     11 | 5674 | `		pValue = HashmapExtractNodeValue(pEntry);` |
|     11 | 5675 | `		if( pValue ){` |
|     11 | 5676 | `			if( pValue->iFlags & MEMOBJ_HASHMAP ){` |
|      5 | 5677 | `				if( iNest < 32 ){` |
|      - | 5678 | `					/* Recurse */` |
|      5 | 5679 | `					iNest++;` |
|      5 | 5680 | `					HashmapWalkRecursive((ph7_hashmap *)pValue->x.pOther,pCallback,pUserData,iNest);` |
|      5 | 5681 | `					iNest--;` |
|      2 | 5682 | `				}` |
|      3 | 5683 | `			}else{` |
|      - | 5684 | `				/* Extract the node key */` |
|      7 | 5685 | `				PH7_HashmapExtractNodeKey(pEntry,&sKey);` |
|      - | 5686 | `				/* Invoke the supplied callback */` |
|      7 | 5687 | `				rc = PH7_VmCallUserFunctionAp(pMap->pVm,pCallback,0,pValue,&sKey,pUserData,0);` |
|      7 | 5688 | `				PH7_MemObjRelease(&sKey);` |
|      7 | 5689 | `				if( rc != SXRET_OK ){` |
|    ! 0 | 5690 | `					return rc;` |
|      - | 5691 | `				}` |
|      - | 5692 | `			}` |
|      5 | 5693 | `		}` |
|      - | 5694 | `		/* Point to the next entry */` |
|     11 | 5695 | `		pEntry = pEntry->pPrev; /* Reverse link */` |
|      6 | 5696 | `	}` |
|      7 | 5697 | `	return SXRET_OK;` |
|      4 | 5698 |  |
|      - | 5699 | `/*` |
|      - | 5700 | ` * bool array_walk_recursive(array &$array,callback $funcname [, value $userdata ] )` |
|      - | 5701 | ` *  Apply a user function recursively to every member of an array.` |
|      - | 5702 | ` * Parameters` |
|      - | 5703 | ` *  $array` |
|      - | 5704 | ` *   The input array.` |
|      - | 5705 | ` * $funcname` |
|      - | 5706 | ` *  Typically, funcname takes on two parameters.The array parameter's value being` |
|      - | 5707 | ` *  the first, and the key/index second.` |
|      - | 5708 | ` * Note:` |
|      - | 5709 | ` *  If funcname needs to be working with the actual values of the array,specify the first` |
|      - | 5710 | ` *  parameter of funcname as a reference. Then, any changes made to those elements will` |
|      - | 5711 | ` *  be made in the original array itself.` |
|      - | 5712 | ` * $userdata` |
|      - | 5713 | ` *  If the optional userdata parameter is supplied, it will be passed as the third parameter` |
|      - | 5714 | ` *  to the callback funcname.` |
|      - | 5715 | ` * Return` |
|      - | 5716 | ` *  Returns TRUE on success or FALSE on failure.` |
|      - | 5717 | ` */` |
|      2 | 5718 | `static int ph7_hashmap_walk_recursive(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 5719 |  |
|      - | 5720 | `	ph7_hashmap *pMap;` |
|      - | 5721 | `	sxi32 rc;` |
|      3 | 5722 | `	if( nArg < 2 \|\| !ph7_value_is_array(apArg[0]) ){` |
|      - | 5723 | `		/* Invalid/Missing arguments,return FALSE */` |
|    ! 0 | 5724 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 5725 | `		return PH7_OK;` |
|      - | 5726 | `	}` |
|      - | 5727 | `	/* Point to the internal representation of the input hashmap */` |
|      3 | 5728 | `	pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|      - | 5729 | `	/* Perform the desired operation */` |
|      3 | 5730 | `	rc = HashmapWalkRecursive(pMap,apArg[1],nArg > 2 ? apArg[2] : 0,0);` |
|      - | 5731 | `	/* All done */` |
|      3 | 5732 | `	ph7_result_bool(pCtx,rc == SXRET_OK);` |
|      3 | 5733 | `	return PH7_OK;` |
|      2 | 5734 |  |
|      - | 5735 | `/*` |
|      - | 5736 | ` * Table of hashmap functions.` |
|      - | 5737 | ` */` |
|      - | 5738 | `static const ph7_builtin_func aHashmapFunc[] = {` |
|      - | 5739 | `	{"count",             ph7_hashmap_count },` |
|      - | 5740 | `	{"sizeof",            ph7_hashmap_count },` |
|      - | 5741 | `	{"array_key_exists",  ph7_hashmap_key_exists },` |
|      - | 5742 | `	{"array_pop",         ph7_hashmap_pop     },` |
|      - | 5743 | `	{"array_push",        ph7_hashmap_push    },` |
|      - | 5744 | `	{"array_shift",       ph7_hashmap_shift   },` |
|      - | 5745 | `	{"array_product",     ph7_hashmap_product },` |
|      - | 5746 | `	{"array_sum",         ph7_hashmap_sum     },` |
|      - | 5747 | `	{"array_keys",        ph7_hashmap_keys    },` |
|      - | 5748 | `	{"array_values",      ph7_hashmap_values  },` |
|      - | 5749 | `	{"array_same",        ph7_hashmap_same    },  /* Symisc eXtension */` |
|      - | 5750 | `	{"array_merge",       ph7_hashmap_merge   },` |
|      - | 5751 | `	{"array_slice",       ph7_hashmap_slice   },` |
|      - | 5752 | `	{"array_splice",      ph7_hashmap_splice  },` |
|      - | 5753 | `	{"array_search",      ph7_hashmap_search  },` |
|      - | 5754 | `	{"array_diff",        ph7_hashmap_diff    },` |
|      - | 5755 | `	{"array_udiff",       ph7_hashmap_udiff   },` |
|      - | 5756 | `	{"array_diff_assoc",  ph7_hashmap_diff_assoc },` |
|      - | 5757 | `	{"array_diff_uassoc", ph7_hashmap_diff_uassoc },` |
|      - | 5758 | `	{"array_diff_key",    ph7_hashmap_diff_key },` |
|      - | 5759 | `	{"array_intersect",   ph7_hashmap_intersect},` |
|      - | 5760 | `	{"array_intersect_assoc", ph7_hashmap_intersect_assoc},` |
|      - | 5761 | `	{"array_uintersect",  ph7_hashmap_uintersect},` |
|      - | 5762 | `	{"array_intersect_key",   ph7_hashmap_intersect_key},` |
|      - | 5763 | `	{"array_copy",        ph7_hashmap_copy    },` |
|      - | 5764 | `	{"array_erase",       ph7_hashmap_erase   },` |
|      - | 5765 | `	{"array_fill",        ph7_hashmap_fill    },` |
|      - | 5766 | `	{"array_fill_keys",   ph7_hashmap_fill_keys},` |
|      - | 5767 | `	{"array_combine",     ph7_hashmap_combine },` |
|      - | 5768 | `	{"array_reverse",     ph7_hashmap_reverse },` |
|      - | 5769 | `	{"array_unique",      ph7_hashmap_unique  },` |
|      - | 5770 | `	{"array_flip",        ph7_hashmap_flip    },` |
|      - | 5771 | `	{"array_rand",        ph7_hashmap_rand    },` |
|      - | 5772 | `	{"array_chunk",       ph7_hashmap_chunk   },` |
|      - | 5773 | `	{"array_pad",         ph7_hashmap_pad     },` |
|      - | 5774 | `	{"array_replace",     ph7_hashmap_replace },` |
|      - | 5775 | `	{"array_filter",      ph7_hashmap_filter  },` |
|      - | 5776 | `	{"array_map",         ph7_hashmap_map     },` |
|      - | 5777 | `	{"array_reduce",      ph7_hashmap_reduce  },` |
|      - | 5778 | `	{"array_walk",        ph7_hashmap_walk    },` |
|      - | 5779 | `	{"array_walk_recursive", ph7_hashmap_walk_recursive },` |
|      - | 5780 | `	{"in_array",          ph7_hashmap_in_array},` |
|      - | 5781 | `	{"sort",              ph7_hashmap_sort    },` |
|      - | 5782 | `	{"asort",             ph7_hashmap_asort   },` |
|      - | 5783 | `	{"arsort",            ph7_hashmap_arsort  },` |
|      - | 5784 | `	{"ksort",             ph7_hashmap_ksort   },` |
|      - | 5785 | `	{"krsort",            ph7_hashmap_krsort  },` |
|      - | 5786 | `	{"rsort",             ph7_hashmap_rsort   },` |
|      - | 5787 | `	{"usort",             ph7_hashmap_usort   },` |
|      - | 5788 | `	{"uasort",            ph7_hashmap_uasort  },` |
|      - | 5789 | `	{"uksort",            ph7_hashmap_uksort  },` |
|      - | 5790 | `	{"shuffle",           ph7_hashmap_shuffle },` |
|      - | 5791 | `	{"range",             ph7_hashmap_range   },` |
|      - | 5792 | `	{"current",           ph7_hashmap_current },` |
|      - | 5793 | `	{"each",              ph7_hashmap_each    },` |
|      - | 5794 | `	{"pos",               ph7_hashmap_current },` |
|      - | 5795 | `	{"next",              ph7_hashmap_next    },` |
|      - | 5796 | `	{"prev",              ph7_hashmap_prev    },` |
|      - | 5797 | `	{"end",               ph7_hashmap_end     },` |
|      - | 5798 | `	{"reset",             ph7_hashmap_reset   },` |
|      - | 5799 | `	{"key",               ph7_hashmap_simple_key }` |
|      - | 5800 | `};` |
|      - | 5801 | `/*` |
|      - | 5802 | ` * Register the built-in hashmap functions defined above.` |
|      - | 5803 | ` */` |
|   1098 | 5804 | `PH7_PRIVATE void PH7_RegisterHashmapFunctions(ph7_vm *pVm)` |
|      2 | 5805 |  |
|      - | 5806 | `	sxu32 n;` |
|  68078 | 5807 | `	for( n = 0 ; n < SX_ARRAYSIZE(aHashmapFunc) ; n++ ){` |
|  66980 | 5808 | `		ph7_create_function(&(*pVm),aHashmapFunc[n].zName,aHashmapFunc[n].xFunc,0);` |
|  33491 | 5809 | `	}` |
|   1100 | 5810 |  |
|      - | 5811 | `/*` |
|      - | 5812 | ` * Dump a hashmap instance and it's entries and the store the dump in` |
|      - | 5813 | ` * the BLOB given as the first argument.` |
|      - | 5814 | ` * This function is typically invoked when the user issue a call to` |
|      - | 5815 | ` * [var_dump(),var_export(),print_r(),...]` |
|      - | 5816 | ` * This function SXRET_OK on success. Any other return value including` |
|      - | 5817 | ` * SXERR_LIMIT(infinite recursion) indicates failure.` |
|      - | 5818 | ` */` |
|     28 | 5819 | `PH7_PRIVATE sxi32 PH7_HashmapDump(SyBlob *pOut,ph7_hashmap *pMap,int ShowType,int nTab,int nDepth)` |
|      2 | 5820 |  |
|      - | 5821 | `	ph7_hashmap_node *pEntry;` |
|      - | 5822 | `	ph7_value *pObj;` |
|     30 | 5823 | `	sxu32 n = 0;` |
|      - | 5824 | `	int isRef;` |
|      - | 5825 | `	sxi32 rc;` |
|      - | 5826 | `	int i;` |
|     30 | 5827 | `	if( nDepth > 31 ){` |
|      - | 5828 | `		static const char zInfinite[] = "Nesting limit reached: Infinite recursion?";` |
|      - | 5829 | `		/* Nesting limit reached */` |
|    ! 0 | 5830 | `		SyBlobAppend(&(*pOut),zInfinite,sizeof(zInfinite)-1);` |
|    ! 0 | 5831 | `		if( ShowType ){` |
|    ! 0 | 5832 | `			SyBlobAppend(&(*pOut),")",sizeof(char));` |
|    ! 0 | 5833 | `		}` |
|    ! 0 | 5834 | `		return SXERR_LIMIT;` |
|      - | 5835 | `	}` |
|      - | 5836 | `	/* Point to the first inserted entry */` |
|     30 | 5837 | `	pEntry = pMap->pFirst;` |
|     30 | 5838 | `	rc = SXRET_OK;` |
|     30 | 5839 | `	if( !ShowType ){` |
|     15 | 5840 | `		SyBlobAppend(&(*pOut),"Array(",sizeof("Array(")-1);` |
|      7 | 5841 | `	}` |
|      - | 5842 | `	/* Total entries */` |
|     30 | 5843 | `	SyBlobFormat(&(*pOut),"%u) {",pMap->nEntry);` |
|      - | 5844 | `#ifdef __WINNT__` |
|      2 | 5845 | `	SyBlobAppend(&(*pOut),"\r\n",sizeof("\r\n")-1);` |
|      - | 5846 | `#else` |
|     28 | 5847 | `	SyBlobAppend(&(*pOut),"\n",sizeof(char));` |
|      - | 5848 | `#endif` |
|     65 | 5849 | `	for(;;){` |
|    132 | 5850 | `		if( n >= pMap->nEntry ){` |
|     30 | 5851 | `			break;` |
|      - | 5852 | `		}` |
|    206 | 5853 | `		for( i = 0 ; i < nTab ; i++ ){` |
|    104 | 5854 | `			SyBlobAppend(&(*pOut)," ",sizeof(char));` |
|     53 | 5855 | `		}` |
|      - | 5856 | `		/* Dump key */` |
|    104 | 5857 | `		if( pEntry->iType == HASHMAP_INT_NODE){` |
|     37 | 5858 | `			SyBlobFormat(&(*pOut),"[%qd] =>",pEntry->xKey.iKey);` |
|     19 | 5859 | `		}else{` |
|    101 | 5860 | `			SyBlobFormat(&(*pOut),"[%.*s] =>",` |
|     33 | 5861 | `				SyBlobLength(&pEntry->xKey.sKey),SyBlobData(&pEntry->xKey.sKey));` |
|      - | 5862 | `		}` |
|      - | 5863 | `#ifdef __WINNT__` |
|      2 | 5864 | `		SyBlobAppend(&(*pOut),"\r\n",sizeof("\r\n")-1);` |
|      - | 5865 | `#else` |
|    102 | 5866 | `		SyBlobAppend(&(*pOut),"\n",sizeof(char));` |
|      - | 5867 | `#endif` |
|      - | 5868 | `		/* Dump node value */` |
|    104 | 5869 | `		pObj = HashmapExtractNodeValue(pEntry);` |
|    104 | 5870 | `		isRef = 0;` |
|    104 | 5871 | `		if( pObj ){` |
|    104 | 5872 | `			if( pEntry->iFlags & HASHMAP_NODE_FOREIGN_OBJ ){` |
|      - | 5873 | `				/* Referenced object */` |
|    ! 0 | 5874 | `				isRef = 1;` |
|    ! 0 | 5875 | `			}` |
|    104 | 5876 | `			rc = PH7_MemObjDump(&(*pOut),pObj,ShowType,nTab+1,nDepth,isRef);` |
|    104 | 5877 | `			if( rc == SXERR_LIMIT ){` |
|    ! 0 | 5878 | `				break;` |
|      - | 5879 | `			}` |
|     51 | 5880 | `		}` |
|      - | 5881 | `		/* Point to the next entry */` |
|    104 | 5882 | `		n++;` |
|    104 | 5883 | `		pEntry = pEntry->pPrev; /* Reverse link */` |
|      2 | 5884 | `	}` |
|     58 | 5885 | `	for( i = 0 ; i < nTab ; i++ ){` |
|     30 | 5886 | `		SyBlobAppend(&(*pOut)," ",sizeof(char));` |
|     16 | 5887 | `	}` |
|     30 | 5888 | `	SyBlobAppend(&(*pOut),"}",sizeof(char));` |
|     30 | 5889 | `	return rc;` |
|     16 | 5890 |  |
|      - | 5891 | `/*` |
|      - | 5892 | ` * Iterate throw hashmap entries and invoke the given callback [i.e: xWalk()] for each` |
|      - | 5893 | ` * retrieved entry.` |
|      - | 5894 | ` * Note that argument are passed to the callback by copy. That is,any modification to` |
|      - | 5895 | ` * the entry value in the callback body will not alter the real value.` |
|      - | 5896 | ` * If the callback wishes to abort processing [i.e: it's invocation] it must return` |
|      - | 5897 | ` * a value different from PH7_OK.` |
|      - | 5898 | ` * Refer to [ph7_array_walk()] for more information.` |
|      - | 5899 | ` */` |
|  17706 | 5900 | `PH7_PRIVATE sxi32 PH7_HashmapWalk(` |
|      - | 5901 | `	ph7_hashmap *pMap, /* Target hashmap */` |
|      - | 5902 | `	int (*xWalk)(ph7_value *,ph7_value *,void *), /* Walker callback */` |
|      - | 5903 | `	void *pUserData /* Last argument to xWalk() */` |
|      - | 5904 | `	)` |
|      2 | 5905 |  |
|      - | 5906 | `	ph7_hashmap_node *pEntry;` |
|      - | 5907 | `	ph7_value sKey,sValue;` |
|      - | 5908 | `	sxi32 rc;` |
|      - | 5909 | `	sxu32 n;` |
|      - | 5910 | `	/* Initialize walker parameter */` |
|  17708 | 5911 | `	rc = SXRET_OK;` |
|  17708 | 5912 | `	PH7_MemObjInit(pMap->pVm,&sKey);` |
|  17708 | 5913 | `	PH7_MemObjInit(pMap->pVm,&sValue);` |
|  17708 | 5914 | `	n = pMap->nEntry;` |
|  17708 | 5915 | `	pEntry = pMap->pFirst;` |
|      - | 5916 | `	/* Start the iteration process */` |
|  48989 | 5917 | `	for(;;){` |
|  97980 | 5918 | `		if( n < 1 ){` |
|  17708 | 5919 | `			break;` |
|      - | 5920 | `		}` |
|      - | 5921 | `		/* Extract a copy of the key and a copy the current value */` |
|  80274 | 5922 | `		PH7_HashmapExtractNodeKey(pEntry,&sKey);` |
|  80274 | 5923 | `		PH7_HashmapExtractNodeValue(pEntry,&sValue,FALSE);` |
|      - | 5924 | `		/* Invoke the user callback */` |
|  80274 | 5925 | `		rc = xWalk(&sKey,&sValue,pUserData);` |
|      - | 5926 | `		/* Release the copy of the key and the value */` |
|  80274 | 5927 | `		PH7_MemObjRelease(&sKey);` |
|  80274 | 5928 | `		PH7_MemObjRelease(&sValue);` |
|  80274 | 5929 | `		if( rc != PH7_OK ){` |
|      - | 5930 | `			/* Callback request an operation abort */` |
|    ! 0 | 5931 | `			return SXERR_ABORT;` |
|      - | 5932 | `		}` |
|      - | 5933 | `		/* Point to the next entry */` |
|  80274 | 5934 | `		pEntry = pEntry->pPrev; /* Reverse link */` |
|  80274 | 5935 | `		n--;` |
|      2 | 5936 | `	}` |
|      - | 5937 | `	/* All done */` |
|  17708 | 5938 | `	return SXRET_OK;` |
|   8855 | 5939 |  |
|      - | 5940 |  |
