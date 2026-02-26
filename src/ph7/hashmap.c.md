# src/ph7/hashmap.c

<style>code, pre { background: none !important; white-space: pre !important; width: 100% !important; display: inline-block !important; } td { border: none !important; margin-top: 0 !important; margin-bottom: 0 !important; padding-top: 0 !important; padding-bottom: 0 !important; }</style>

Coverage: 2498/3038 lines (82.23%)

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
| 2684200 |   18 | `static sxu32 IntHash(sxi64 iKey)` |
|       2 |   19 |  |
| 2684202 |   20 | `	return (sxu32)(iKey ^ (iKey << 8) ^ (iKey >> 8));` |
|       2 |   21 |  |
|       - |   22 | `/*` |
|       - |   23 | ` * Default hash function for string/BLOB keys.` |
|       - |   24 | ` */` |
|  206672 |   25 | `static sxu32 BinHash(const void *pSrc,sxu32 nLen)` |
|       2 |   26 |  |
|  206674 |   27 | `	register unsigned char *zIn = (unsigned char *)pSrc;` |
|       - |   28 | `	unsigned char *zEnd;` |
|  206674 |   29 | `	sxu32 nH = 5381;` |
|  206674 |   30 | `	zEnd = &zIn[nLen];` |
|  239866 |   31 | `	for(;;){` |
|  479734 |   32 | `		if( zIn >= zEnd ){ break; } nH = nH * 33 + zIn[0] ; zIn++;` |
|  433658 |   33 | `		if( zIn >= zEnd ){ break; } nH = nH * 33 + zIn[0] ; zIn++;` |
|  393448 |   34 | `		if( zIn >= zEnd ){ break; } nH = nH * 33 + zIn[0] ; zIn++;` |
|  316606 |   35 | `		if( zIn >= zEnd ){ break; } nH = nH * 33 + zIn[0] ; zIn++;` |
|       2 |   36 | `	}` |
|  206674 |   37 | `	return nH;` |
|       2 |   38 |  |
|       - |   39 | `/*` |
|       - |   40 | ` * Return the total number of entries in a given hashmap.` |
|       - |   41 | ` * If bRecurisve is set to TRUE then recurse on hashmap entries.` |
|       - |   42 | ` * If the nesting limit is reached,this function abort immediately.` |
|       - |   43 | ` */` |
|     702 |   44 | `static sxi64 HashmapCount(ph7_hashmap *pMap,int bRecursive,int iRecCount)` |
|       2 |   45 |  |
|     704 |   46 | `	sxi64 iCount = 0;` |
|     704 |   47 | `	if( !bRecursive ){` |
|     428 |   48 | `		iCount = pMap->nEntry;` |
|     215 |   49 | `	}else{` |
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
|     700 |   79 | `	return iCount;` |
|     353 |   80 |  |
|       - |   81 | `/*` |
|       - |   82 | ` * Allocate a new hashmap node with a 64-bit integer key.` |
|       - |   83 | ` * If something goes wrong [i.e: out of memory],this function return NULL.` |
|       - |   84 | ` * Otherwise a fresh [ph7_hashmap_node] instance is returned.` |
|       - |   85 | ` */` |
| 2631100 |   86 | `static ph7_hashmap_node * HashmapNewIntNode(ph7_hashmap *pMap,sxi64 iKey,sxu32 nHash,sxu32 nValIdx)` |
|       2 |   87 |  |
|       - |   88 | `	ph7_hashmap_node *pNode;` |
|       - |   89 | `	/* Allocate a new node */` |
| 2631102 |   90 | `	pNode = (ph7_hashmap_node *)SyMemBackendPoolAlloc(&pMap->pVm->sAllocator,sizeof(ph7_hashmap_node));` |
| 2631102 |   91 | `	if( pNode == 0 ){` |
|     ! 0 |   92 | `		return 0;` |
|       - |   93 | `	}` |
|       - |   94 | `	/* Zero the stucture */` |
| 2631102 |   95 | `	SyZero(pNode,sizeof(ph7_hashmap_node));` |
|       - |   96 | `	/* Fill in the structure */` |
| 2631102 |   97 | `	pNode->pMap  = &(*pMap);` |
| 2631102 |   98 | `	pNode->iType = HASHMAP_INT_NODE;` |
| 2631102 |   99 | `	pNode->nHash = nHash;` |
| 2631102 |  100 | `	pNode->xKey.iKey = iKey;` |
| 2631102 |  101 | `	pNode->nValIdx  = nValIdx;` |
| 2631102 |  102 | `	return pNode;` |
| 1315552 |  103 |  |
|       - |  104 | `/*` |
|       - |  105 | ` * Allocate a new hashmap node with a BLOB key.` |
|       - |  106 | ` * If something goes wrong [i.e: out of memory],this function return NULL.` |
|       - |  107 | ` * Otherwise a fresh [ph7_hashmap_node] instance is returned.` |
|       - |  108 | ` */` |
|   72386 |  109 | `static ph7_hashmap_node * HashmapNewBlobNode(ph7_hashmap *pMap,const void *pKey,sxu32 nKeyLen,sxu32 nHash,sxu32 nValIdx)` |
|       2 |  110 |  |
|       - |  111 | `	ph7_hashmap_node *pNode;` |
|       - |  112 | `	/* Allocate a new node */` |
|   72388 |  113 | `	pNode = (ph7_hashmap_node *)SyMemBackendPoolAlloc(&pMap->pVm->sAllocator,sizeof(ph7_hashmap_node));` |
|   72388 |  114 | `	if( pNode == 0 ){` |
|     ! 0 |  115 | `		return 0;` |
|       - |  116 | `	}` |
|       - |  117 | `	/* Zero the stucture */` |
|   72388 |  118 | `	SyZero(pNode,sizeof(ph7_hashmap_node));` |
|       - |  119 | `	/* Fill in the structure */` |
|   72388 |  120 | `	pNode->pMap  = &(*pMap);` |
|   72388 |  121 | `	pNode->iType = HASHMAP_BLOB_NODE;` |
|   72388 |  122 | `	pNode->nHash = nHash;` |
|   72388 |  123 | `	SyBlobInit(&pNode->xKey.sKey,&pMap->pVm->sAllocator);` |
|   72388 |  124 | `	SyBlobAppend(&pNode->xKey.sKey,pKey,nKeyLen);` |
|   72388 |  125 | `	pNode->nValIdx = nValIdx;` |
|   72388 |  126 | `	return pNode;` |
|   36195 |  127 |  |
|       - |  128 | `/*` |
|       - |  129 | ` * link a hashmap node to the given bucket index (last argument to this function).` |
|       - |  130 | ` */` |
| 2703486 |  131 | `static void HashmapNodeLink(ph7_hashmap *pMap,ph7_hashmap_node *pNode,sxu32 nBucketIdx)` |
|       2 |  132 |  |
|       - |  133 | `	/* Link */` |
| 2703488 |  134 | `	if( pMap->apBucket[nBucketIdx] != 0 ){` |
| 2506824 |  135 | `		pNode->pNextCollide = pMap->apBucket[nBucketIdx];` |
| 2506824 |  136 | `		pMap->apBucket[nBucketIdx]->pPrevCollide = pNode;` |
| 1253411 |  137 | `	}` |
| 2703488 |  138 | `	pMap->apBucket[nBucketIdx] = pNode;` |
|       - |  139 | `	/* Link to the map list */` |
| 2703488 |  140 | `	if( pMap->pFirst == 0 ){` |
|   32024 |  141 | `		pMap->pFirst = pMap->pLast = pNode;` |
|       - |  142 | `		/* Point to the first inserted node */` |
|   32024 |  143 | `		pMap->pCur = pNode;` |
|   16013 |  144 | `	}else{` |
| 2671466 |  145 | `		MACRO_LD_PUSH(pMap->pLast,pNode);` |
|       - |  146 | `	}` |
| 2703488 |  147 | `	++pMap->nEntry;` |
| 2703488 |  148 |  |
|       - |  149 | `/*` |
|       - |  150 | ` * Unlink a node from the hashmap.` |
|       - |  151 | ` * If the node count reaches zero then release the whole hash-bucket.` |
|       - |  152 | ` */` |
|    5058 |  153 | `PH7_PRIVATE void PH7_HashmapUnlinkNode(ph7_hashmap_node *pNode,int bRestore)` |
|       2 |  154 |  |
|    5060 |  155 | `	ph7_hashmap *pMap = pNode->pMap;` |
|    5060 |  156 | `	ph7_vm *pVm = pMap->pVm;` |
|       - |  157 | `	/* Unlink from the corresponding bucket */` |
|    5060 |  158 | `	if( pNode->pPrevCollide == 0 ){` |
|    4634 |  159 | `		pMap->apBucket[pNode->nHash & (pMap->nSize - 1)] = pNode->pNextCollide;` |
|    2318 |  160 | `	}else{` |
|     427 |  161 | `		pNode->pPrevCollide->pNextCollide = pNode->pNextCollide;` |
|       - |  162 | `	}` |
|    5060 |  163 | `	if( pNode->pNextCollide ){` |
|    3855 |  164 | `		pNode->pNextCollide->pPrevCollide = pNode->pPrevCollide;` |
|    1927 |  165 | `	}` |
|    5060 |  166 | `	if( pMap->pFirst == pNode ){` |
|      58 |  167 | `		pMap->pFirst = pNode->pPrev;` |
|      28 |  168 | `	}` |
|    5060 |  169 | `	if( pMap->pCur == pNode ){` |
|       - |  170 | `		/* Advance the node cursor */` |
|      60 |  171 | `		pMap->pCur = pMap->pCur->pPrev; /* Reverse link */` |
|      29 |  172 | `	}` |
|       - |  173 | `	/* Unlink from the map list */` |
|    5060 |  174 | `	MACRO_LD_REMOVE(pMap->pLast,pNode);` |
|    5060 |  175 | `	if( bRestore ){` |
|       - |  176 | `		/* Remove the ph7_value associated with this node from the reference table */` |
|      30 |  177 | `		PH7_VmRefObjRemove(pVm,pNode->nValIdx,0,pNode);` |
|       - |  178 | `		/* Restore to the freelist */` |
|      30 |  179 | `		if( (pNode->iFlags & HASHMAP_NODE_FOREIGN_OBJ) == 0 ){` |
|      30 |  180 | `			PH7_VmUnsetMemObj(pVm,pNode->nValIdx,FALSE);` |
|      14 |  181 | `		}` |
|      14 |  182 | `	}` |
|    5060 |  183 | `	if( pNode->iType == HASHMAP_BLOB_NODE ){` |
|    5011 |  184 | `		SyBlobRelease(&pNode->xKey.sKey);` |
|    2505 |  185 | `	}` |
|    5060 |  186 | `	SyMemBackendPoolFree(&pVm->sAllocator,pNode);` |
|    5060 |  187 | `	pMap->nEntry--;` |
|    5060 |  188 | `	if( pMap->nEntry < 1 && pMap != pVm->pGlobal ){` |
|       - |  189 | `		/* Free the hash-bucket */` |
|      26 |  190 | `		SyMemBackendFree(&pVm->sAllocator,pMap->apBucket);` |
|      26 |  191 | `		pMap->apBucket = 0;` |
|      26 |  192 | `		pMap->nSize = 0;` |
|      26 |  193 | `		pMap->pFirst = pMap->pLast = pMap->pCur = 0;` |
|      12 |  194 | `	}` |
|    5060 |  195 |  |
|       - |  196 | `#define HASHMAP_FILL_FACTOR 3` |
|       - |  197 | `/*` |
|       - |  198 | ` * Grow the hash-table and rehash all entries.` |
|       - |  199 | ` */` |
| 2703486 |  200 | `static sxi32 HashmapGrowBucket(ph7_hashmap *pMap)` |
|       2 |  201 |  |
| 2703488 |  202 | `	if( pMap->nEntry >= pMap->nSize * HASHMAP_FILL_FACTOR ){` |
|   35226 |  203 | `		ph7_hashmap_node **apOld = pMap->apBucket;` |
|       - |  204 | `		ph7_hashmap_node *pEntry,**apNew;` |
|   35226 |  205 | `		sxu32 nNew = pMap->nSize << 1;` |
|       - |  206 | `		sxu32 nBucket;` |
|       - |  207 | `		sxu32 n;` |
|   35226 |  208 | `		if( nNew < 1 ){` |
|   32024 |  209 | `			nNew = 16;` |
|   16011 |  210 | `		}` |
|       - |  211 | `		/* Allocate a new bucket */` |
|   35226 |  212 | `		apNew = (ph7_hashmap_node **)SyMemBackendAlloc(&pMap->pVm->sAllocator,nNew * sizeof(ph7_hashmap_node *));` |
|   35226 |  213 | `		if( apNew == 0 ){` |
|     ! 0 |  214 | `			if( pMap->nSize < 1 ){` |
|     ! 0 |  215 | `				return SXERR_MEM; /* Fatal */` |
|       - |  216 | `			}` |
|       - |  217 | `			/* Not so fatal here,simply a performance hit */` |
|     ! 0 |  218 | `			return SXRET_OK;` |
|       - |  219 | `		}` |
|       - |  220 | `		/* Zero the table */` |
|   35226 |  221 | `		SyZero((void *)apNew,nNew * sizeof(ph7_hashmap_node *));` |
|       - |  222 | `		/* Reflect the change */` |
|   35226 |  223 | `		pMap->apBucket = apNew;` |
|   35226 |  224 | `		pMap->nSize = nNew;` |
|   35226 |  225 | `		if( apOld == 0 ){` |
|       - |  226 | `			/* First allocated table [i.e: no entry],return immediately */` |
|   32024 |  227 | `			return SXRET_OK;` |
|       - |  228 | `		}` |
|       - |  229 | `		/* Rehash old entries */` |
|    3204 |  230 | `		pEntry = pMap->pFirst;` |
|    3204 |  231 | `		n = 0;` |
| 1850801 |  232 | `		for( ;; ){` |
| 3701604 |  233 | `			if( n >= pMap->nEntry ){` |
|    3204 |  234 | `				break;` |
|       - |  235 | `			}` |
|       - |  236 | `			/* Clear the old collision link */` |
| 3698402 |  237 | `			pEntry->pNextCollide = pEntry->pPrevCollide = 0;` |
|       - |  238 | `			/* Link to the new bucket */` |
| 3698402 |  239 | `			nBucket = pEntry->nHash & (nNew - 1);` |
| 3698402 |  240 | `			if( pMap->apBucket[nBucket] != 0 ){` |
| 3327812 |  241 | `				pEntry->pNextCollide = pMap->apBucket[nBucket];` |
| 3327812 |  242 | `				pMap->apBucket[nBucket]->pPrevCollide = pEntry;` |
| 1663905 |  243 | `			}` |
| 3698402 |  244 | `			pMap->apBucket[nBucket] = pEntry;` |
|       - |  245 | `			/* Point to the next entry */` |
| 3698402 |  246 | `			pEntry = pEntry->pPrev; /* Reverse link */` |
| 3698402 |  247 | `			n++;` |
|       2 |  248 | `		}` |
|       - |  249 | `		/* Free the old table */` |
|    3204 |  250 | `		SyMemBackendFree(&pMap->pVm->sAllocator,(void *)apOld);` |
|    1601 |  251 | `	}` |
| 2671466 |  252 | `	return SXRET_OK;` |
| 1351745 |  253 |  |
|       - |  254 | `/*` |
|       - |  255 | ` * Insert a 64-bit integer key and it's associated value (if any) in the given` |
|       - |  256 | ` * hashmap.` |
|       - |  257 | ` */` |
| 2631100 |  258 | `static sxi32 HashmapInsertIntKey(ph7_hashmap *pMap,sxi64 iKey,ph7_value *pValue,sxu32 nRefIdx,int isForeign)` |
|       2 |  259 |  |
|       - |  260 | `	ph7_hashmap_node *pNode;` |
|       - |  261 | `	sxu32 nIdx;` |
|       - |  262 | `	sxu32 nHash;` |
|       - |  263 | `	sxi32 rc;` |
| 2631102 |  264 | `	if( !isForeign ){` |
|       - |  265 | `		ph7_value *pObj;` |
|       - |  266 | `		/* Reserve a ph7_value for the value */` |
| 2631078 |  267 | `		pObj = PH7_ReserveMemObj(pMap->pVm);` |
| 2631078 |  268 | `		if( pObj == 0 ){` |
|     ! 0 |  269 | `			return SXERR_MEM;` |
|       - |  270 | `		}` |
| 2631078 |  271 | `		if( pValue ){` |
|       - |  272 | `			/* Duplicate the value */` |
| 2631078 |  273 | `			PH7_MemObjStore(pValue,pObj);` |
| 1315538 |  274 | `		}` |
| 2631078 |  275 | `		nIdx = pObj->nIdx;` |
| 1315540 |  276 | `	}else{` |
|      25 |  277 | `		nIdx = nRefIdx;` |
|       - |  278 | `	}` |
|       - |  279 | `	/* Hash the key */` |
| 2631102 |  280 | `	nHash = pMap->xIntHash(iKey);` |
|       - |  281 | `	/* Allocate a new int node */` |
| 2631102 |  282 | `	pNode = HashmapNewIntNode(&(*pMap),iKey,nHash,nIdx);` |
| 2631102 |  283 | `	if( pNode == 0 ){` |
|     ! 0 |  284 | `		return SXERR_MEM;` |
|       - |  285 | `	}` |
| 2631102 |  286 | `	if( isForeign ){` |
|       - |  287 | `		/* Mark as a foregin entry */` |
|      25 |  288 | `		pNode->iFlags \|= HASHMAP_NODE_FOREIGN_OBJ;` |
|      12 |  289 | `	}` |
|       - |  290 | `	/* Make sure the bucket is big enough to hold the new entry */` |
| 2631102 |  291 | `	rc = HashmapGrowBucket(&(*pMap));` |
| 2631102 |  292 | `	if( rc != SXRET_OK ){` |
|     ! 0 |  293 | `		SyMemBackendPoolFree(&pMap->pVm->sAllocator,pNode);` |
|     ! 0 |  294 | `		return rc;` |
|       - |  295 | `	}` |
|       - |  296 | `	/* Perform the insertion */` |
| 2631102 |  297 | `	HashmapNodeLink(&(*pMap),pNode,nHash & (pMap->nSize - 1));` |
|       - |  298 | `	/* Install in the reference table */` |
| 2631102 |  299 | `	PH7_VmRefObjInstall(pMap->pVm,nIdx,0,pNode,0);` |
|       - |  300 | `	/* All done */` |
| 2631102 |  301 | `	return SXRET_OK;` |
| 1315552 |  302 |  |
|       - |  303 | `/*` |
|       - |  304 | ` * Insert a BLOB key and it's associated value (if any) in the given` |
|       - |  305 | ` * hashmap.` |
|       - |  306 | ` */` |
|   72386 |  307 | `static sxi32 HashmapInsertBlobKey(ph7_hashmap *pMap,const void *pKey,sxu32 nKeyLen,ph7_value *pValue,sxu32 nRefIdx,int isForeign)` |
|       2 |  308 |  |
|       - |  309 | `	ph7_hashmap_node *pNode;` |
|       - |  310 | `	sxu32 nHash;` |
|       - |  311 | `	sxu32 nIdx;` |
|       - |  312 | `	sxi32 rc;` |
|   72388 |  313 | `	if( !isForeign ){` |
|       - |  314 | `		ph7_value *pObj;` |
|       - |  315 | `		/* Reserve a ph7_value for the value */` |
|   55288 |  316 | `		pObj = PH7_ReserveMemObj(pMap->pVm);` |
|   55288 |  317 | `		if( pObj == 0 ){` |
|     ! 0 |  318 | `			return SXERR_MEM;` |
|       - |  319 | `		}` |
|   55288 |  320 | `		if( pValue ){` |
|       - |  321 | `			/* Duplicate the value */` |
|   55288 |  322 | `			PH7_MemObjStore(pValue,pObj);` |
|   27643 |  323 | `		}` |
|   55288 |  324 | `		nIdx = pObj->nIdx;` |
|   27645 |  325 | `	}else{` |
|   17102 |  326 | `		nIdx = nRefIdx;` |
|       - |  327 | `	}` |
|       - |  328 | `	/* Hash the key */` |
|   72388 |  329 | `	nHash = pMap->xBlobHash(pKey,nKeyLen);` |
|       - |  330 | `	/* Allocate a new blob node */` |
|   72388 |  331 | `	pNode = HashmapNewBlobNode(&(*pMap),pKey,nKeyLen,nHash,nIdx);` |
|   72388 |  332 | `	if( pNode == 0 ){` |
|     ! 0 |  333 | `		return SXERR_MEM;` |
|       - |  334 | `	}` |
|   72388 |  335 | `	if( isForeign ){` |
|       - |  336 | `		/* Mark as a foregin entry */` |
|   17102 |  337 | `		pNode->iFlags \|= HASHMAP_NODE_FOREIGN_OBJ;` |
|    8550 |  338 | `	}` |
|       - |  339 | `	/* Make sure the bucket is big enough to hold the new entry */` |
|   72388 |  340 | `	rc = HashmapGrowBucket(&(*pMap));` |
|   72388 |  341 | `	if( rc != SXRET_OK ){` |
|     ! 0 |  342 | `		SyMemBackendPoolFree(&pMap->pVm->sAllocator,pNode);` |
|     ! 0 |  343 | `		return rc;` |
|       - |  344 | `	}` |
|       - |  345 | `	/* Perform the insertion */` |
|   72388 |  346 | `	HashmapNodeLink(&(*pMap),pNode,nHash & (pMap->nSize - 1));` |
|       - |  347 | `	/* Install in the reference table */` |
|   72388 |  348 | `	PH7_VmRefObjInstall(pMap->pVm,nIdx,0,pNode,0);` |
|       - |  349 | `	/* All done */` |
|   72388 |  350 | `	return SXRET_OK;` |
|   36195 |  351 |  |
|       - |  352 | `/*` |
|       - |  353 | ` * Check if a given 64-bit integer key exists in the given hashmap.` |
|       - |  354 | ` * Write a pointer to the target node on success. Otherwise` |
|       - |  355 | ` * SXERR_NOTFOUND is returned on failure.` |
|       - |  356 | ` */` |
|   46324 |  357 | `static sxi32 HashmapLookupIntKey(` |
|       - |  358 | `	ph7_hashmap *pMap,         /* Target hashmap */` |
|       - |  359 | `	sxi64 iKey,                /* lookup key */` |
|       - |  360 | `	ph7_hashmap_node **ppNode  /* OUT: target node on success */` |
|       - |  361 | `	)` |
|       2 |  362 |  |
|       - |  363 | `	ph7_hashmap_node *pNode;` |
|       - |  364 | `	sxu32 nHash;` |
|   46326 |  365 | `	if( pMap->nEntry < 1 ){` |
|       - |  366 | `		/* Don't bother hashing,there is no entry anyway */` |
|     315 |  367 | `		return SXERR_NOTFOUND;` |
|       - |  368 | `	}` |
|       - |  369 | `	/* Hash the key first */` |
|   46012 |  370 | `	nHash = pMap->xIntHash(iKey);` |
|       - |  371 | `	/* Point to the appropriate bucket */` |
|   46012 |  372 | `	pNode = pMap->apBucket[nHash & (pMap->nSize - 1)];` |
|       - |  373 | `	/* Perform the lookup */` |
|  411362 |  374 | `	for(;;){` |
|  822726 |  375 | `		if( pNode == 0 ){` |
|   45595 |  376 | `			break;` |
|       - |  377 | `		}` |
|  777338 |  378 | `		if( pNode->iType == HASHMAP_INT_NODE` |
|  774112 |  379 | `			&& pNode->nHash == nHash` |
|  385757 |  380 | `			&& pNode->xKey.iKey == iKey ){` |
|       - |  381 | `				/* Node found */` |
|     418 |  382 | `				if( ppNode ){` |
|     410 |  383 | `					*ppNode = pNode;` |
|     204 |  384 | `				}` |
|     418 |  385 | `				return SXRET_OK;` |
|       - |  386 | `		}` |
|       - |  387 | `		/* Follow the collision link */` |
|  776715 |  388 | `		pNode = pNode->pNextCollide;` |
|       1 |  389 | `	}` |
|       - |  390 | `	/* No such entry */` |
|   45595 |  391 | `	return SXERR_NOTFOUND;` |
|   23164 |  392 |  |
|       - |  393 | `/*` |
|       - |  394 | ` * Check if a given BLOB key exists in the given hashmap.` |
|       - |  395 | ` * Write a pointer to the target node on success. Otherwise` |
|       - |  396 | ` * SXERR_NOTFOUND is returned on failure.` |
|       - |  397 | ` */` |
|  141264 |  398 | `static sxi32 HashmapLookupBlobKey(` |
|       - |  399 | `	ph7_hashmap *pMap,          /* Target hashmap */` |
|       - |  400 | `	const void *pKey,           /* Lookup key */` |
|       - |  401 | `	sxu32 nKeyLen,              /* Key length in bytes */` |
|       - |  402 | `	ph7_hashmap_node **ppNode   /* OUT: target node on success */` |
|       - |  403 | `	)` |
|       2 |  404 |  |
|       - |  405 | `	ph7_hashmap_node *pNode;` |
|       - |  406 | `	sxu32 nHash;` |
|  141266 |  407 | `	if( pMap->nEntry < 1 ){` |
|       - |  408 | `		/* Don't bother hashing,there is no entry anyway */` |
|    6980 |  409 | `		return SXERR_NOTFOUND;` |
|       - |  410 | `	}` |
|       - |  411 | `	/* Hash the key first */` |
|  134288 |  412 | `	nHash = pMap->xBlobHash(pKey,nKeyLen);` |
|       - |  413 | `	/* Point to the appropriate bucket */` |
|  134288 |  414 | `	pNode = pMap->apBucket[nHash & (pMap->nSize - 1)];` |
|       - |  415 | `	/* Perform the lookup */` |
|  138322 |  416 | `	for(;;){` |
|  276646 |  417 | `		if( pNode == 0 ){` |
|  101472 |  418 | `			break;` |
|       - |  419 | `		}` |
|  191582 |  420 | `		if( pNode->iType == HASHMAP_BLOB_NODE` |
|  173674 |  421 | `			&& pNode->nHash == nHash` |
|  102495 |  422 | `			&& SyBlobLength(&pNode->xKey.sKey) == nKeyLen` |
|   32818 |  423 | `			&& SyMemcmp(SyBlobData(&pNode->xKey.sKey),pKey,nKeyLen) == 0 ){` |
|       - |  424 | `				/* Node found */` |
|   32818 |  425 | `				if( ppNode ){` |
|   32802 |  426 | `					*ppNode = pNode;` |
|   16400 |  427 | `				}` |
|   32818 |  428 | `				return SXRET_OK;` |
|       - |  429 | `		}` |
|       - |  430 | `		/* Follow the collision link */` |
|  142360 |  431 | `		pNode = pNode->pNextCollide;` |
|       2 |  432 | `	}` |
|       - |  433 | `	/* No such entry */` |
|  101472 |  434 | `	return SXERR_NOTFOUND;` |
|   70634 |  435 |  |
|       - |  436 | `/*` |
|       - |  437 | ` * Check if the given BLOB key looks like a decimal number.` |
|       - |  438 | ` * Retrurn TRUE on success.FALSE otherwise.` |
|       - |  439 | ` */` |
|  141442 |  440 | `static int HashmapIsIntKey(SyBlob *pKey)` |
|       2 |  441 |  |
|  141444 |  442 | `	const char *zIn  = (const char *)SyBlobData(pKey);` |
|  141444 |  443 | `	const char *zEnd = &zIn[SyBlobLength(pKey)];` |
|  141444 |  444 | `	if( (int)(zEnd-zIn) > 1 && zIn[0] == '0' ){` |
|       - |  445 | `		/* Octal not decimal number */` |
|       5 |  446 | `		return FALSE;` |
|       - |  447 | `	}` |
|  141440 |  448 | `	if( (zIn[0] == '-' \|\| zIn[0] == '+') && &zIn[1] < zEnd ){` |
|     ! 0 |  449 | `		zIn++;` |
|     ! 0 |  450 | `	}` |
|   71052 |  451 | `	for(;;){` |
|  142106 |  452 | `		if( zIn >= zEnd ){` |
|     233 |  453 | `			return TRUE;` |
|       - |  454 | `		}` |
|  141874 |  455 | `		if( (unsigned char)zIn[0] >= 0xc0 /* UTF-8 stream */  \|\| !SyisDigit(zIn[0]) ){` |
|   70605 |  456 | `			break;` |
|       - |  457 | `		}` |
|     667 |  458 | `		zIn++;` |
|       1 |  459 | `	}` |
|       - |  460 | `	/* Key does not look like a decimal number */` |
|  141208 |  461 | `	return FALSE;` |
|   70723 |  462 |  |
|       - |  463 | `/*` |
|       - |  464 | ` * Check if a given key exists in the given hashmap.` |
|       - |  465 | ` * Write a pointer to the target node on success.` |
|       - |  466 | ` * Otherwise SXERR_NOTFOUND is returned on failure.` |
|       - |  467 | ` */` |
|   69240 |  468 | `static sxi32 HashmapLookup(` |
|       - |  469 | `	ph7_hashmap *pMap,          /* Target hashmap */` |
|       - |  470 | `	ph7_value *pKey,            /* Lookup key */` |
|       - |  471 | `	ph7_hashmap_node **ppNode   /* OUT: target node on success */` |
|       - |  472 | `	)` |
|       2 |  473 |  |
|   69242 |  474 | `	ph7_hashmap_node *pNode = 0; /* cc -O6 warning */` |
|       - |  475 | `	sxi32 rc;` |
|   69242 |  476 | `	if( pKey->iFlags & (MEMOBJ_STRING\|MEMOBJ_HASHMAP\|MEMOBJ_OBJ\|MEMOBJ_RES) ){` |
|   68892 |  477 | `		if( (pKey->iFlags & MEMOBJ_STRING) == 0 ){` |
|       - |  478 | `			/* Force a string cast */` |
|     ! 0 |  479 | `			PH7_MemObjToString(&(*pKey));` |
|     ! 0 |  480 | `		}` |
|   68892 |  481 | `		if( SyBlobLength(&pKey->sBlob) > 0 && !HashmapIsIntKey(&pKey->sBlob) ){` |
|       - |  482 | `			/* Perform a blob lookup */` |
|   68876 |  483 | `			rc = HashmapLookupBlobKey(&(*pMap),SyBlobData(&pKey->sBlob),SyBlobLength(&pKey->sBlob),&pNode);` |
|   68876 |  484 | `			goto result;` |
|       - |  485 | `		}` |
|       8 |  486 | `	}` |
|       - |  487 | `	/* Perform an int lookup */` |
|     368 |  488 | `	if((pKey->iFlags & MEMOBJ_INT) == 0 ){` |
|       - |  489 | `		/* Force an integer cast */` |
|      19 |  490 | `		PH7_MemObjToInteger(pKey);` |
|       9 |  491 | `	}` |
|       - |  492 | `	/* Perform an int lookup */` |
|     368 |  493 | `	rc = HashmapLookupIntKey(&(*pMap),pKey->x.iVal,&pNode);` |
|   34620 |  494 | `result:` |
|   69242 |  495 | `	if( rc == SXRET_OK ){` |
|       - |  496 | `		/* Node found */` |
|   33118 |  497 | `		if( ppNode ){` |
|   33102 |  498 | `			*ppNode = pNode;` |
|   16550 |  499 | `		}` |
|   33118 |  500 | `		return SXRET_OK;` |
|       - |  501 | `	}` |
|       - |  502 | `	/* No such entry */` |
|   36126 |  503 | `	return SXERR_NOTFOUND;` |
|   34622 |  504 |  |
|       - |  505 | `/*` |
|       - |  506 | ` * Insert a given key and it's associated value (if any) in the given` |
|       - |  507 | ` * hashmap.` |
|       - |  508 | ` * If a node with the given key already exists in the database` |
|       - |  509 | ` * then this function overwrite the old value.` |
|       - |  510 | ` */` |
| 2686292 |  511 | `static sxi32 HashmapInsert(` |
|       - |  512 | `	ph7_hashmap *pMap, /* Target hashmap */` |
|       - |  513 | `	ph7_value *pKey,   /* Lookup key  */` |
|       - |  514 | `	ph7_value *pVal    /* Node value */` |
|       - |  515 | `	)` |
|       2 |  516 |  |
| 2686294 |  517 | `	ph7_hashmap_node *pNode = 0;` |
| 2686294 |  518 | `	sxi32 rc = SXRET_OK;` |
| 2686294 |  519 | `	if( pKey && pKey->iFlags & (MEMOBJ_STRING\|MEMOBJ_HASHMAP\|MEMOBJ_OBJ\|MEMOBJ_RES) ){` |
|   55484 |  520 | `		if( (pKey->iFlags & MEMOBJ_STRING) == 0 ){` |
|       - |  521 | `			/* Force a string cast */` |
|       8 |  522 | `			PH7_MemObjToString(&(*pKey));` |
|       3 |  523 | `		}` |
|   55484 |  524 | `		if( SyBlobLength(&pKey->sBlob) < 1 \|\| HashmapIsIntKey(&pKey->sBlob) ){` |
|     254 |  525 | `			if(SyBlobLength(&pKey->sBlob) < 1){` |
|       - |  526 | `				/* Automatic index assign */` |
|      32 |  527 | `				pKey = 0;` |
|      15 |  528 | `			}` |
|     254 |  529 | `			goto IntKey;` |
|       - |  530 | `		}` |
|   82847 |  531 | `		if( SXRET_OK == HashmapLookupBlobKey(&(*pMap),SyBlobData(&pKey->sBlob),` |
|   27615 |  532 | `			SyBlobLength(&pKey->sBlob),&pNode) ){` |
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
|   55210 |  546 | `		if( pMap == pMap->pVm->pGlobal ){` |
|       - |  547 | `			/* Forbidden */` |
|       3 |  548 | `			PH7_VmThrowError(pMap->pVm,0,PH7_CTX_NOTICE,"$GLOBALS is a read-only array,insertion is forbidden");` |
|       3 |  549 | `			return SXRET_OK;` |
|       - |  550 | `		}` |
|       - |  551 | `		/* Perform a blob-key insertion */` |
|   55208 |  552 | `		rc = HashmapInsertBlobKey(&(*pMap),SyBlobData(&pKey->sBlob),SyBlobLength(&pKey->sBlob),&(*pVal),0,FALSE);` |
|   55208 |  553 | `		return rc;` |
|       - |  554 | `	}` |
| 1315405 |  555 | `IntKey:` |
| 2631064 |  556 | `	if( pKey ){` |
|   23093 |  557 | `		if((pKey->iFlags & MEMOBJ_INT) == 0 ){` |
|       - |  558 | `			/* Force an integer cast */` |
|     247 |  559 | `			PH7_MemObjToInteger(pKey);` |
|     123 |  560 | `		}` |
|   23093 |  561 | `		if( SXRET_OK == HashmapLookupIntKey(&(*pMap),pKey->x.iVal,&pNode) ){` |
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
|   23057 |  575 | `		if( pMap == pMap->pVm->pGlobal ){` |
|       - |  576 | `			/* Forbidden */` |
|       3 |  577 | `			PH7_VmThrowError(pMap->pVm,0,PH7_CTX_NOTICE,"$GLOBALS is a read-only array,insertion is forbidden");` |
|       3 |  578 | `			return SXRET_OK;` |
|       - |  579 | `		}` |
|       - |  580 | `		/* Perform a 64-bit-int-key insertion */` |
|   23055 |  581 | `		rc = HashmapInsertIntKey(&(*pMap),pKey->x.iVal,&(*pVal),0,FALSE);` |
|   23055 |  582 | `		if( rc == SXRET_OK ){` |
|   23055 |  583 | `			if( pKey->x.iVal >= pMap->iNextIdx ){` |
|       - |  584 | `				/* Increment the automatic index */` |
|   22829 |  585 | `				pMap->iNextIdx = pKey->x.iVal + 1;` |
|       - |  586 | `				/* Make sure the automatic index is not reserved */` |
|   22829 |  587 | `				while( SXRET_OK == HashmapLookupIntKey(&(*pMap),pMap->iNextIdx,0) ){` |
|     ! 0 |  588 | `					pMap->iNextIdx++;` |
|     ! 0 |  589 | `				}` |
|   11414 |  590 | `			}` |
|   11527 |  591 | `		}` |
|   11528 |  592 | `	}else{` |
| 2607972 |  593 | `		if( pMap == pMap->pVm->pGlobal ){` |
|       - |  594 | `			/* Forbidden */` |
|       3 |  595 | `			PH7_VmThrowError(pMap->pVm,0,PH7_CTX_NOTICE,"$GLOBALS is a read-only array,insertion is forbidden");` |
|       3 |  596 | `			return SXRET_OK;` |
|       - |  597 | `		}` |
|       - |  598 | `		/* Assign an automatic index */` |
| 2607970 |  599 | `		rc = HashmapInsertIntKey(&(*pMap),pMap->iNextIdx,&(*pVal),0,FALSE);` |
| 2607970 |  600 | `		if( rc == SXRET_OK ){` |
| 2607970 |  601 | `			++pMap->iNextIdx;` |
| 1303984 |  602 | `		}` |
|       - |  603 | `	}` |
|       - |  604 | `	/* Insertion result */` |
| 2631024 |  605 | `	return rc;` |
| 1343148 |  606 |  |
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
|   17130 |  634 | `static sxi32 HashmapInsertByRef(` |
|       - |  635 | `	ph7_hashmap *pMap,   /* Target hashmap */` |
|       - |  636 | `	ph7_value *pKey,     /* Lookup key */` |
|       - |  637 | `	sxu32 nRefIdx        /* Foreign ph7_value index */` |
|       - |  638 | `	)` |
|       2 |  639 |  |
|   17132 |  640 | `	ph7_hashmap_node *pNode = 0;` |
|   17132 |  641 | `	sxi32 rc = SXRET_OK;` |
|   17132 |  642 | `	if( pKey && pKey->iFlags & (MEMOBJ_STRING\|MEMOBJ_HASHMAP\|MEMOBJ_OBJ\|MEMOBJ_RES) ){` |
|   17108 |  643 | `		if( (pKey->iFlags & MEMOBJ_STRING) == 0 ){` |
|       - |  644 | `			/* Force a string cast */` |
|     ! 0 |  645 | `			PH7_MemObjToString(&(*pKey));` |
|     ! 0 |  646 | `		}` |
|   17108 |  647 | `		if( SyBlobLength(&pKey->sBlob) < 1 \|\| HashmapIsIntKey(&pKey->sBlob) ){` |
|     ! 0 |  648 | `			if(SyBlobLength(&pKey->sBlob) < 1){` |
|       - |  649 | `				/* Automatic index assign */` |
|     ! 0 |  650 | `				pKey = 0;` |
|     ! 0 |  651 | `			}` |
|     ! 0 |  652 | `			goto IntKey;` |
|       - |  653 | `		}` |
|   25661 |  654 | `		if( SXRET_OK == HashmapLookupBlobKey(&(*pMap),SyBlobData(&pKey->sBlob),` |
|    8553 |  655 | `			SyBlobLength(&pKey->sBlob),&pNode) ){` |
|       - |  656 | `				/* Overwrite */` |
|       7 |  657 | `				PH7_VmRefObjRemove(pMap->pVm,pNode->nValIdx,0,pNode);` |
|       7 |  658 | `				pNode->nValIdx = nRefIdx;` |
|       - |  659 | `				/* Install in the reference table */` |
|       7 |  660 | `				PH7_VmRefObjInstall(pMap->pVm,nRefIdx,0,pNode,0);` |
|       7 |  661 | `				return SXRET_OK;` |
|       - |  662 | `		}` |
|       - |  663 | `		/* Perform a blob-key insertion */` |
|   17102 |  664 | `		rc = HashmapInsertBlobKey(&(*pMap),SyBlobData(&pKey->sBlob),SyBlobLength(&pKey->sBlob),0,nRefIdx,TRUE);` |
|   17102 |  665 | `		return rc;` |
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
|    8567 |  702 |  |
|       - |  703 | `/*` |
|       - |  704 | ` * Extract node value.` |
|       - |  705 | ` */` |
|  722239 |  706 | `static ph7_value * HashmapExtractNodeValue(ph7_hashmap_node *pNode)` |
|       2 |  707 |  |
|       - |  708 | `	/* Point to the desired object */` |
|       - |  709 | `	ph7_value *pObj;` |
|  722241 |  710 | `	pObj = (ph7_value *)SySetAt(&pNode->pMap->pVm->aMemObj,pNode->nValIdx);` |
|  722241 |  711 | `	return pObj;` |
|       2 |  712 |  |
|       - |  713 | `/*` |
|       - |  714 | ` * Insert a node in the given hashmap.` |
|       - |  715 | ` * If a node with the given key already exists in the database` |
|       - |  716 | ` * then this function overwrite the old value.` |
|       - |  717 | ` */` |
|     190 |  718 | `static sxi32 HashmapInsertNode(ph7_hashmap *pMap,ph7_hashmap_node *pNode,int bPreserve)` |
|       1 |  719 |  |
|       - |  720 | `	ph7_value *pObj;` |
|       - |  721 | `	sxi32 rc;` |
|       - |  722 | `	/* Extract the node value */` |
|     191 |  723 | `	pObj = HashmapExtractNodeValue(&(*pNode));` |
|     191 |  724 | `	if( pObj == 0 ){` |
|     ! 0 |  725 | `		return SXERR_EMPTY;` |
|       - |  726 | `	}` |
|       - |  727 | `	/* Preserve key */` |
|     191 |  728 | `	if( pNode->iType == HASHMAP_INT_NODE){` |
|       - |  729 | `		/* Int64 key */` |
|      81 |  730 | `		if( !bPreserve ){` |
|       - |  731 | `			/* Assign an automatic index */` |
|      39 |  732 | `			rc = HashmapInsert(&(*pMap),0,pObj);` |
|      20 |  733 | `		}else{` |
|      43 |  734 | `			rc = HashmapInsertIntKey(&(*pMap),pNode->xKey.iKey,pObj,0,FALSE);` |
|       - |  735 | `		}` |
|      41 |  736 | `	}else{` |
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
|     191 |  747 | `	return rc;` |
|      96 |  748 |  |
|       - |  749 | `/*` |
|       - |  750 | ` * Compare two node values.` |
|       - |  751 | ` * Return 0 if the node values are equals, > 0 if pLeft is greater than pRight` |
|       - |  752 | ` * or < 0 if pRight is greater than pLeft.` |
|       - |  753 | ` * For a full description on ph7_values comparison,refer to the implementation` |
|       - |  754 | ` * of the [PH7_MemObjCmp()] function defined in memobj.c or the official` |
|       - |  755 | ` * documenation.` |
|       - |  756 | ` */` |
|   33020 |  757 | `static sxi32 HashmapNodeCmp(ph7_hashmap_node *pLeft,ph7_hashmap_node *pRight,int bStrict)` |
|       2 |  758 |  |
|       - |  759 | `	ph7_value sObj1,sObj2;` |
|       - |  760 | `	sxi32 rc;` |
|   33022 |  761 | `	if( pLeft == pRight ){` |
|       - |  762 | `		/*` |
|       - |  763 | `		 * Same node.Refer to the sort() implementation defined` |
|       - |  764 | `		 * below for more information on this sceanario.` |
|       - |  765 | `		 */` |
|     ! 0 |  766 | `		return 0;` |
|       - |  767 | `	}` |
|       - |  768 | `	/* Do the comparison */` |
|   33022 |  769 | `	PH7_MemObjInit(pLeft->pMap->pVm,&sObj1);` |
|   33022 |  770 | `	PH7_MemObjInit(pLeft->pMap->pVm,&sObj2);` |
|   33022 |  771 | `	PH7_HashmapExtractNodeValue(pLeft,&sObj1,FALSE);` |
|   33022 |  772 | `	PH7_HashmapExtractNodeValue(pRight,&sObj2,FALSE);` |
|   33022 |  773 | `	rc = PH7_MemObjCmp(&sObj1,&sObj2,bStrict,0);` |
|   33022 |  774 | `	PH7_MemObjRelease(&sObj1);` |
|   33022 |  775 | `	PH7_MemObjRelease(&sObj2);` |
|   33022 |  776 | `	return rc;` |
|   16532 |  777 |  |
|       - |  778 | `/*` |
|       - |  779 | ` * Rehash a node with a 64-bit integer key.` |
|       - |  780 | ` * Refer to [merge_sort(),array_shift()] implementations for more information.` |
|       - |  781 | ` */` |
|    7090 |  782 | `static void HashmapRehashIntNode(ph7_hashmap_node *pEntry)` |
|       2 |  783 |  |
|    7092 |  784 | `	ph7_hashmap *pMap = pEntry->pMap;` |
|       - |  785 | `	sxu32 nBucket;` |
|       - |  786 | `	/* Remove old collision links */` |
|    7092 |  787 | `	if( pEntry->pPrevCollide ){` |
|    5702 |  788 | `		pEntry->pPrevCollide->pNextCollide = pEntry->pNextCollide;` |
|    2850 |  789 | `	}else{` |
|    1392 |  790 | `		pMap->apBucket[pEntry->nHash & (pMap->nSize - 1)] = pEntry->pNextCollide;` |
|       - |  791 | `	}` |
|    7092 |  792 | `	if( pEntry->pNextCollide ){` |
|     644 |  793 | `		pEntry->pNextCollide->pPrevCollide = pEntry->pPrevCollide;` |
|     320 |  794 | `	}` |
|    7092 |  795 | `	pEntry->pNextCollide = pEntry->pPrevCollide = 0;` |
|       - |  796 | `	/* Compute the new hash */` |
|    7092 |  797 | `	pEntry->nHash = pMap->xIntHash(pMap->iNextIdx);` |
|    7092 |  798 | `	pEntry->xKey.iKey = pMap->iNextIdx;` |
|    7092 |  799 | `	nBucket = pEntry->nHash & (pMap->nSize - 1);` |
|       - |  800 | `	/* Link to the new bucket */` |
|    7092 |  801 | `	pEntry->pNextCollide = pMap->apBucket[nBucket];` |
|    7092 |  802 | `	if( pMap->apBucket[nBucket] ){` |
|    5860 |  803 | `		pMap->apBucket[nBucket]->pPrevCollide = pEntry;` |
|    2932 |  804 | `	}` |
|    7092 |  805 | `	pEntry->pNextCollide = pMap->apBucket[nBucket];` |
|    7092 |  806 | `	pMap->apBucket[nBucket] = pEntry;` |
|       - |  807 | `	/* Increment the automatic index */` |
|    7092 |  808 | `	pMap->iNextIdx++;` |
|    7092 |  809 |  |
|       - |  810 | `/*` |
|       - |  811 | ` * Perform a linear search on a given hashmap.` |
|       - |  812 | ` * Write a pointer to the target node on success.` |
|       - |  813 | ` * Otherwise SXERR_NOTFOUND is returned on failure.` |
|       - |  814 | ` * Refer to [array_intersect(),array_diff(),in_array(),...] implementations` |
|       - |  815 | ` * for more information.` |
|       - |  816 | ` */` |
|   18088 |  817 | `static int HashmapFindValue(` |
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
|   18090 |  830 | `	pEntry = pMap->pFirst;` |
|   18090 |  831 | `	n = pMap->nEntry;` |
|   18090 |  832 | `	PH7_MemObjInit(pMap->pVm,&sVal);` |
|   18090 |  833 | `	PH7_MemObjInit(pMap->pVm,&sNeedle);` |
|   43333 |  834 | `	for(;;){` |
|   86669 |  835 | `		if( n < 1 ){` |
|      25 |  836 | `			break;` |
|       - |  837 | `		}` |
|       - |  838 | `		/* Extract node value */` |
|   86645 |  839 | `		pVal = HashmapExtractNodeValue(pEntry);` |
|   86645 |  840 | `		if( pVal ){` |
|   86645 |  841 | `			if( (pVal->iFlags\|pNeedle->iFlags) & MEMOBJ_NULL ){` |
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
|   86645 |  853 | `				PH7_MemObjLoad(pVal,&sVal);` |
|   86645 |  854 | `				PH7_MemObjLoad(pNeedle,&sNeedle);` |
|   86645 |  855 | `				rc = PH7_MemObjCmp(&sNeedle,&sVal,bStrict,0);` |
|   86645 |  856 | `				PH7_MemObjRelease(&sVal);` |
|   86645 |  857 | `				PH7_MemObjRelease(&sNeedle);` |
|   86645 |  858 | `				if( rc == 0 ){` |
|   18066 |  859 | `					if( ppNode ){` |
|       3 |  860 | `						*ppNode = pEntry;` |
|       1 |  861 | `					}` |
|       - |  862 | `					/* Match found*/` |
|   18066 |  863 | `					return SXRET_OK;` |
|       - |  864 | `				}` |
|       - |  865 | `			}` |
|   34289 |  866 | `		}` |
|       - |  867 | `		/* Point to the next entry */` |
|   68581 |  868 | `		pEntry = pEntry->pPrev; /* Reverse link */` |
|   68581 |  869 | `		n--;` |
|       2 |  870 | `	}` |
|       - |  871 | `	/* No such entry */` |
|      25 |  872 | `	return SXERR_NOTFOUND;` |
|    9046 |  873 |  |
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
|  315998 | 1047 | `static sxi32 HashmapDuplicateNode(` |
|       - | 1048 | `	ph7_hashmap *pDest,` |
|       - | 1049 | `	ph7_hashmap_node *pEntry,` |
|       - | 1050 | `	ph7_value *pVal,` |
|       - | 1051 | `	int iAction /* 0: Merge, 1: Overwrite, 2: Dup */` |
|       - | 1052 | `	)` |
|       2 | 1053 |  |
|  316000 | 1054 | `	ph7_value sSafeVal = *pVal;` |
|       - | 1055 | `	ph7_value sKey;` |
|       - | 1056 | `	sxi32 rc;` |
|       - | 1057 |  |
|  316000 | 1058 | `	if( pEntry->iType == HASHMAP_BLOB_NODE ){` |
|       - | 1059 | `		/* Blob key insertion */` |
|      19 | 1060 | `		PH7_MemObjInitFromString(pDest->pVm,&sKey,0);` |
|      19 | 1061 | `		PH7_MemObjStringAppend(&sKey,(const char *)SyBlobData(&pEntry->xKey.sKey),SyBlobLength(&pEntry->xKey.sKey));` |
|      19 | 1062 | `		rc = PH7_HashmapInsert(pDest,&sKey,&sSafeVal);` |
|      19 | 1063 | `		PH7_MemObjRelease(&sKey);` |
|      10 | 1064 | `	}else{` |
|       - | 1065 | `		/* Int key */` |
|  315982 | 1066 | `		if( iAction == 0 ){ /* Merge */` |
|  315966 | 1067 | `			rc = HashmapInsert(pDest,0/* Automatic index assign */,&sSafeVal);` |
|  158000 | 1068 | `		}else if( iAction == 1 ){ /* Overwrite */` |
|       5 | 1069 | `			PH7_MemObjInitFromInt(pDest->pVm,&sKey,pEntry->xKey.iKey);` |
|       5 | 1070 | `			rc = PH7_HashmapInsert(pDest,&sKey,&sSafeVal);` |
|       5 | 1071 | `			PH7_MemObjRelease(&sKey);` |
|       3 | 1072 | `		}else{ /* Dup */` |
|      14 | 1073 | `			rc = HashmapInsertIntKey(pDest,pEntry->xKey.iKey,&sSafeVal,0,FALSE);` |
|       - | 1074 | `		}` |
|       - | 1075 | `	}` |
|  316000 | 1076 | `	return rc;` |
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
|    1568 | 1090 | `static sxi32 HashmapMerge(ph7_hashmap *pSrc,ph7_hashmap *pDest)` |
|       2 | 1091 |  |
|       - | 1092 | `	ph7_hashmap_node *pEntry;` |
|       - | 1093 | `	ph7_value *pVal;` |
|       - | 1094 | `	sxi32 rc;` |
|       - | 1095 | `	sxu32 n;` |
|    1570 | 1096 | `	if( pSrc == pDest ){` |
|       - | 1097 | `		/* Same map. This can easily happen since hashmaps are passed by reference.` |
|       - | 1098 | `		 * Unlike the zend engine.` |
|       - | 1099 | `		 */` |
|     ! 0 | 1100 | `		return SXRET_OK;` |
|       - | 1101 | `	}` |
|       - | 1102 | `	/* Point to the first inserted entry in the source */` |
|    1570 | 1103 | `	pEntry = pSrc->pFirst;` |
|       - | 1104 | `	/* Perform the merge */` |
|  317540 | 1105 | `	for( n = 0 ; n < pSrc->nEntry ; ++n ){` |
|       - | 1106 | `		/* Extract the node value */` |
|  315972 | 1107 | `		pVal = HashmapExtractNodeValue(pEntry);` |
|  315972 | 1108 | `		if( pVal ){` |
|       - | 1109 | `			/* Make a local copy of the value.` |
|       - | 1110 | `			 * The insertion call below may trigger a memory pool reallocation` |
|       - | 1111 | `			 * which will invalidate the 'pVal' pointer since it points` |
|       - | 1112 | `			 * to the old pool.` |
|       - | 1113 | `			 */` |
|  315972 | 1114 | `			rc = HashmapDuplicateNode(pDest,pEntry,pVal,0);` |
|  157987 | 1115 | `		}else{` |
|     ! 0 | 1116 | `			rc = SXRET_OK;` |
|       - | 1117 | `		}` |
|  315972 | 1118 | `		if( rc != SXRET_OK ){` |
|     ! 0 | 1119 | `			return rc;` |
|       - | 1120 | `		}` |
|       - | 1121 | `		/* Point to the next entry */` |
|  315972 | 1122 | `		pEntry = pEntry->pPrev; /* Reverse link */` |
|  157987 | 1123 | `	}` |
|    1570 | 1124 | `	return SXRET_OK;` |
|     786 | 1125 |  |
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
|   46574 | 1297 | `PH7_PRIVATE ph7_hashmap * PH7_NewHashmap(` |
|       - | 1298 | `	ph7_vm *pVm,              /* VM that trigger the hashmap creation */` |
|       - | 1299 | `	sxu32 (*xIntHash)(sxi64), /* Hash function for int keys.NULL otherwise*/` |
|       - | 1300 | `	sxu32 (*xBlobHash)(const void *,sxu32) /* Hash function for BLOB keys.NULL otherwise */` |
|       - | 1301 | `	)` |
|       2 | 1302 |  |
|       - | 1303 | `	ph7_hashmap *pMap;` |
|       - | 1304 | `	/* Allocate a new instance */` |
|   46576 | 1305 | `	pMap = (ph7_hashmap *)SyMemBackendPoolAlloc(&pVm->sAllocator,sizeof(ph7_hashmap));` |
|   46576 | 1306 | `	if( pMap == 0 ){` |
|     ! 0 | 1307 | `		return 0;` |
|       - | 1308 | `	}` |
|       - | 1309 | `	/* Zero the structure */` |
|   46576 | 1310 | `	SyZero(pMap,sizeof(ph7_hashmap));` |
|       - | 1311 | `	/* Fill in the structure */` |
|   46576 | 1312 | `	pMap->pVm = &(*pVm);` |
|   46576 | 1313 | `	pMap->iRef = 1;` |
|       - | 1314 | `	/* Default hash functions */` |
|   46576 | 1315 | `	pMap->xIntHash  = xIntHash ? xIntHash : IntHash;` |
|   46576 | 1316 | `	pMap->xBlobHash = xBlobHash ? xBlobHash : BinHash;` |
|   46576 | 1317 | `	return pMap;` |
|   23289 | 1318 |  |
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
|    1224 | 1339 | `PH7_PRIVATE sxi32 PH7_HashmapCreateSuper(ph7_vm *pVm)` |
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
|    1226 | 1359 | `	pMap = PH7_NewHashmap(&(*pVm),0,0);` |
|    1226 | 1360 | `	if( pMap == 0 ){` |
|     ! 0 | 1361 | `		return SXERR_MEM;` |
|       - | 1362 | `	}` |
|    1226 | 1363 | `	pVm->pGlobal = pMap;` |
|       - | 1364 | `	/* Reserve a ph7_value for the $GLOBALS array*/` |
|    1226 | 1365 | `	pObj = PH7_ReserveMemObj(&(*pVm));` |
|    1226 | 1366 | `	if( pObj == 0 ){` |
|     ! 0 | 1367 | `		return SXERR_MEM;` |
|       - | 1368 | `	}` |
|    1226 | 1369 | `	PH7_MemObjInitFromArray(&(*pVm),pObj,pMap);` |
|       - | 1370 | `	/* Record object index */` |
|    1226 | 1371 | `	pVm->nGlobalIdx = pObj->nIdx;` |
|       - | 1372 | `	/* Install the special $GLOBALS array */` |
|    1226 | 1373 | `	rc = SyHashInsert(&pVm->hSuper,(const void *)"GLOBALS",sizeof("GLOBALS")-1,SX_INT_TO_PTR(pVm->nGlobalIdx));` |
|    1226 | 1374 | `	if( rc != SXRET_OK ){` |
|     ! 0 | 1375 | `		return rc;` |
|       - | 1376 | `	}` |
|       - | 1377 | `	/* Install superglobals now */` |
|   13466 | 1378 | `	for( n =  0 ; n < SX_ARRAYSIZE(azSuper)  ; n++ ){` |
|       - | 1379 | `		ph7_value *pSuper;` |
|       - | 1380 | `		/* Request an empty array */` |
|   12242 | 1381 | `		pSuper = ph7_new_array(&(*pVm));` |
|   12242 | 1382 | `		if( pSuper == 0 ){` |
|     ! 0 | 1383 | `			return SXERR_MEM;` |
|       - | 1384 | `		}` |
|       - | 1385 | `		/* Install */` |
|   12242 | 1386 | `		rc = ph7_vm_config(&(*pVm),PH7_VM_CONFIG_CREATE_SUPER,azSuper[n]/* Super-global name*/,pSuper/* Super-global value */);` |
|   12242 | 1387 | `		if( rc != SXRET_OK ){` |
|     ! 0 | 1388 | `			return rc;` |
|       - | 1389 | `		}` |
|       - | 1390 | `		/* Release the value now it have been installed */` |
|   12242 | 1391 | `		ph7_release_value(&(*pVm),pSuper);` |
|    6122 | 1392 | `	}` |
|       - | 1393 | `	/* Set some $_SERVER entries */` |
|    1226 | 1394 | `	pFile = (SyString *)SySetPeek(&pVm->aFiles);` |
|       - | 1395 | `	/*` |
|       - | 1396 | `	 * 'SCRIPT_FILENAME'` |
|       - | 1397 | `	 * The absolute pathname of the currently executing script.` |
|       - | 1398 | `	 */` |
|    2446 | 1399 | `	ph7_vm_config(pVm,PH7_VM_CONFIG_SERVER_ATTR,` |
|       - | 1400 | `		"SCRIPT_FILENAME",` |
|     612 | 1401 | `		pFile ? pFile->zString : ":Memory:",` |
|    1220 | 1402 | `		pFile ? pFile->nByte : sizeof(":Memory:") - 1` |
|       - | 1403 | `		);` |
|       - | 1404 | `	/* All done,all super-global are installed now */` |
|    1226 | 1405 | `	return SXRET_OK;` |
|     614 | 1406 |  |
|       - | 1407 | `/*` |
|       - | 1408 | ` * Release a hashmap.` |
|       - | 1409 | ` */` |
|   33058 | 1410 | `PH7_PRIVATE sxi32 PH7_HashmapRelease(ph7_hashmap *pMap,int FreeDS)` |
|       2 | 1411 |  |
|       - | 1412 | `	ph7_hashmap_node *pEntry,*pNext;` |
|   33060 | 1413 | `	ph7_vm *pVm = pMap->pVm;` |
|       - | 1414 | `	sxu32 n;` |
|   33060 | 1415 | `	if( pMap == pVm->pGlobal ){` |
|       - | 1416 | `		/* Cannot delete the $GLOBALS array */` |
|     ! 0 | 1417 | `		PH7_VmThrowError(pMap->pVm,0,PH7_CTX_NOTICE,"$GLOBALS is a read-only array,deletion is forbidden");` |
|     ! 0 | 1418 | `		return SXRET_OK;` |
|       - | 1419 | `	}` |
|       - | 1420 | `	/* Start the release process */` |
|   33060 | 1421 | `	n = 0;` |
|   33060 | 1422 | `	pEntry = pMap->pFirst;` |
| 1357043 | 1423 | `	for(;;){` |
| 2714088 | 1424 | `		if( n >= pMap->nEntry ){` |
|   33060 | 1425 | `			break;` |
|       - | 1426 | `		}` |
| 2681030 | 1427 | `		pNext = pEntry->pPrev; /* Reverse link */` |
|       - | 1428 | `		/* Remove the reference from the foreign table */` |
| 2681030 | 1429 | `		PH7_VmRefObjRemove(pVm,pEntry->nValIdx,0,pEntry);` |
| 2681030 | 1430 | `		if( (pEntry->iFlags & HASHMAP_NODE_FOREIGN_OBJ) == 0 ){` |
|       - | 1431 | `			/* Restore the ph7_value to the free list */` |
| 2681022 | 1432 | `			PH7_VmUnsetMemObj(pVm,pEntry->nValIdx,FALSE);` |
| 1340510 | 1433 | `		}` |
|       - | 1434 | `		/* Release the node */` |
| 2681030 | 1435 | `		if( pEntry->iType == HASHMAP_BLOB_NODE ){` |
|   53430 | 1436 | `			SyBlobRelease(&pEntry->xKey.sKey);` |
|   26714 | 1437 | `		}` |
| 2681030 | 1438 | `		SyMemBackendPoolFree(&pVm->sAllocator,pEntry);` |
|       - | 1439 | `		/* Point to the next entry */` |
| 2681030 | 1440 | `		pEntry = pNext;` |
| 2681030 | 1441 | `		n++;` |
|       2 | 1442 | `	}` |
|   33060 | 1443 | `	if( pMap->nEntry > 0 ){` |
|       - | 1444 | `		/* Release the hash bucket */` |
|   29498 | 1445 | `		SyMemBackendFree(&pVm->sAllocator,pMap->apBucket);` |
|   14748 | 1446 | `	}` |
|   33060 | 1447 | `	if( FreeDS ){` |
|       - | 1448 | `		/* Free the whole instance */` |
|   33058 | 1449 | `		SyMemBackendPoolFree(&pVm->sAllocator,pMap);` |
|   16530 | 1450 | `	}else{` |
|       - | 1451 | `		/* Keep the instance but reset it's fields */` |
|       3 | 1452 | `		pMap->apBucket = 0;` |
|       3 | 1453 | `		pMap->iNextIdx = 0;` |
|       3 | 1454 | `		pMap->nEntry = pMap->nSize = 0;` |
|       3 | 1455 | `		pMap->pFirst = pMap->pLast = pMap->pCur = 0;` |
|       - | 1456 | `	}` |
|   33060 | 1457 | `	return SXRET_OK;` |
|   16531 | 1458 |  |
|       - | 1459 | `/*` |
|       - | 1460 | ` * Decrement the reference count of a given hashmap.` |
|       - | 1461 | ` * If the count reaches zero which mean no more variables` |
|       - | 1462 | ` * are pointing to this hashmap,then release the whole instance.` |
|       - | 1463 | ` */` |
|  393344 | 1464 | `PH7_PRIVATE void  PH7_HashmapUnref(ph7_hashmap *pMap)` |
|       2 | 1465 |  |
|  393346 | 1466 | `	ph7_vm *pVm = pMap->pVm;` |
|       - | 1467 | `	/* TICKET 1432-49: $GLOBALS is not subject to garbage collection */` |
|  393346 | 1468 | `	pMap->iRef--;` |
|  393346 | 1469 | `	if( pMap->iRef < 1 && pMap != pVm->pGlobal){` |
|   33058 | 1470 | `		PH7_HashmapRelease(pMap,TRUE);` |
|   16528 | 1471 | `	}` |
|  393346 | 1472 |  |
|       - | 1473 | `/*` |
|       - | 1474 | ` * Check if a given key exists in the given hashmap.` |
|       - | 1475 | ` * Write a pointer to the target node on success.` |
|       - | 1476 | ` * Otherwise SXERR_NOTFOUND is returned on failure.` |
|       - | 1477 | ` */` |
|   69246 | 1478 | `PH7_PRIVATE sxi32 PH7_HashmapLookup(` |
|       - | 1479 | `	ph7_hashmap *pMap,        /* Target hashmap */` |
|       - | 1480 | `	ph7_value *pKey,          /* Lookup key */` |
|       - | 1481 | `	ph7_hashmap_node **ppNode /* OUT: Target node on success */` |
|       - | 1482 | `	)` |
|       2 | 1483 |  |
|       - | 1484 | `	sxi32 rc;` |
|   69248 | 1485 | `	if( pMap->nEntry < 1 ){` |
|       - | 1486 | `		/* TICKET 1433-25: Don't bother hashing,the hashmap is empty anyway.` |
|       - | 1487 | `		 */` |
|       7 | 1488 | `		return SXERR_NOTFOUND;` |
|       - | 1489 | `	}` |
|   69242 | 1490 | `	rc = HashmapLookup(&(*pMap),&(*pKey),ppNode);` |
|   69242 | 1491 | `	return rc;` |
|   34625 | 1492 |  |
|       - | 1493 | `/*` |
|       - | 1494 | ` * Insert a given key and it's associated value (if any) in the given` |
|       - | 1495 | ` * hashmap.` |
|       - | 1496 | ` * If a node with the given key already exists in the database` |
|       - | 1497 | ` * then this function overwrite the old value.` |
|       - | 1498 | ` */` |
| 2370258 | 1499 | `PH7_PRIVATE sxi32 PH7_HashmapInsert(` |
|       - | 1500 | `	ph7_hashmap *pMap, /* Target hashmap */` |
|       - | 1501 | `	ph7_value *pKey,   /* Lookup key */` |
|       - | 1502 | `	ph7_value *pVal    /* Node value.NULL otherwise */` |
|       - | 1503 | `	)` |
|       2 | 1504 |  |
|       - | 1505 | `	sxi32 rc;` |
| 2370260 | 1506 | `	if( pVal && (pVal->iFlags & MEMOBJ_HASHMAP) && (ph7_hashmap *)pVal->x.pOther == pMap->pVm->pGlobal ){` |
|       - | 1507 | `		/*` |
|       - | 1508 | `		 * TICKET 1433-35: Insertion in the $GLOBALS array is forbidden.` |
|       - | 1509 | `		 */` |
|     ! 0 | 1510 | `		PH7_VmThrowError(pMap->pVm,0,PH7_CTX_ERR,"$GLOBALS is a read-only array,insertion is forbidden");` |
|     ! 0 | 1511 | `		return SXRET_OK;` |
|       - | 1512 | `	}` |
| 2370260 | 1513 | `	rc = HashmapInsert(&(*pMap),&(*pKey),&(*pVal));` |
| 2370260 | 1514 | `	return rc;` |
| 1185131 | 1515 |  |
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
|   17130 | 1543 | `PH7_PRIVATE sxi32 PH7_HashmapInsertByRef(` |
|       - | 1544 | `	ph7_hashmap *pMap, /* Target hashmap */` |
|       - | 1545 | `	ph7_value *pKey,   /* Lookup key */` |
|       - | 1546 | `	sxu32 nRefIdx      /* Foreign ph7_value index */` |
|       - | 1547 | `	)` |
|       2 | 1548 |  |
|       - | 1549 | `	sxi32 rc;` |
|   17132 | 1550 | `	if( nRefIdx == pMap->pVm->nGlobalIdx ){` |
|       - | 1551 | `		/*` |
|       - | 1552 | `		 * TICKET 1433-35: Insertion in the $GLOBALS array is forbidden.` |
|       - | 1553 | `		 */` |
|     ! 0 | 1554 | `		PH7_VmThrowError(pMap->pVm,0,PH7_CTX_ERR,"$GLOBALS is a read-only array,insertion is forbidden");` |
|     ! 0 | 1555 | `		return SXRET_OK;` |
|       - | 1556 | `	}` |
|   17132 | 1557 | `	rc = HashmapInsertByRef(&(*pMap),&(*pKey),nRefIdx);` |
|   17132 | 1558 | `	return rc;` |
|    8567 | 1559 |  |
|       - | 1560 | `/*` |
|       - | 1561 | ` * Reset the node cursor of a given hashmap.` |
|       - | 1562 | ` */` |
|   14768 | 1563 | `PH7_PRIVATE void PH7_HashmapResetLoopCursor(ph7_hashmap *pMap)` |
|       2 | 1564 |  |
|       - | 1565 | `	/* Reset the loop cursor */` |
|   14770 | 1566 | `	pMap->pCur = pMap->pFirst;` |
|   14770 | 1567 |  |
|       - | 1568 | `/*` |
|       - | 1569 | ` * Return a pointer to the node currently pointed by the node cursor.` |
|       - | 1570 | ` * If the cursor reaches the end of the list,then this function` |
|       - | 1571 | ` * return NULL.` |
|       - | 1572 | ` * Note that the node cursor is automatically advanced by this function.` |
|       - | 1573 | ` */` |
|  123250 | 1574 | `PH7_PRIVATE ph7_hashmap_node * PH7_HashmapGetNextEntry(ph7_hashmap *pMap)` |
|       2 | 1575 |  |
|  123252 | 1576 | `	ph7_hashmap_node *pCur = pMap->pCur;` |
|  123252 | 1577 | `	if( pCur == 0 ){` |
|       - | 1578 | `		/* End of the list,return null */` |
|    7388 | 1579 | `		return 0;` |
|       - | 1580 | `	}` |
|       - | 1581 | `	/* Advance the node cursor */` |
|  115866 | 1582 | `	pMap->pCur = pCur->pPrev; /* Reverse link */` |
|  115866 | 1583 | `	return pCur;` |
|   61627 | 1584 |  |
|       - | 1585 | `/*` |
|       - | 1586 | ` * Extract a node value.` |
|       - | 1587 | ` */` |
|  296474 | 1588 | `PH7_PRIVATE void PH7_HashmapExtractNodeValue(ph7_hashmap_node *pNode,ph7_value *pValue,int bStore)` |
|       2 | 1589 |  |
|  296476 | 1590 | `	ph7_value *pEntry = HashmapExtractNodeValue(pNode);` |
|  296476 | 1591 | `	if( pEntry ){` |
|  296476 | 1592 | `		if( bStore ){` |
|  115918 | 1593 | `			PH7_MemObjStore(pEntry,pValue);` |
|   57960 | 1594 | `		}else{` |
|  180560 | 1595 | `			PH7_MemObjLoad(pEntry,pValue);` |
|       - | 1596 | `		}` |
|  148279 | 1597 | `	}else{` |
|     ! 0 | 1598 | `		PH7_MemObjRelease(pValue);` |
|       - | 1599 | `	}` |
|  296476 | 1600 |  |
|       - | 1601 | `/*` |
|       - | 1602 | ` * Extract a node key.` |
|       - | 1603 | ` */` |
|   81680 | 1604 | `PH7_PRIVATE void PH7_HashmapExtractNodeKey(ph7_hashmap_node *pNode,ph7_value *pKey)` |
|       2 | 1605 |  |
|       - | 1606 | `	/* Fill with the current key */` |
|   81682 | 1607 | `	if( pNode->iType == HASHMAP_INT_NODE ){` |
|   81548 | 1608 | `		if( SyBlobLength(&pKey->sBlob) > 0 ){` |
|       3 | 1609 | `			SyBlobRelease(&pKey->sBlob);` |
|       1 | 1610 | `		}` |
|   81548 | 1611 | `		pKey->x.iVal = pNode->xKey.iKey;` |
|   81548 | 1612 | `		MemObjSetType(pKey,MEMOBJ_INT);` |
|   40775 | 1613 | `	}else{` |
|     135 | 1614 | `		SyBlobReset(&pKey->sBlob);` |
|     135 | 1615 | `		SyBlobAppend(&pKey->sBlob,SyBlobData(&pNode->xKey.sKey),SyBlobLength(&pNode->xKey.sKey));` |
|     135 | 1616 | `		MemObjSetType(pKey,MEMOBJ_STRING);` |
|       - | 1617 | `	}` |
|   81682 | 1618 |  |
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
|   20996 | 1666 | `static ph7_hashmap_node * HashmapNodeMerge(ph7_hashmap_node *pA,ph7_hashmap_node *pB,ProcNodeCmp xCmp,void *pCmpData)` |
|       2 | 1667 |  |
|       - | 1668 | `	ph7_hashmap_node result,*pTail;` |
|       - | 1669 | `    /* Prevent compiler warning */` |
|   20998 | 1670 | `	result.pNext = result.pPrev = 0;` |
|   20998 | 1671 | `	pTail = &result;` |
|   54068 | 1672 | `	while( pA && pB ){` |
|   33072 | 1673 | `		if( xCmp(pA,pB,pCmpData) < 0 ){` |
|   21705 | 1674 | `			pTail->pPrev = pA;` |
|   21705 | 1675 | `			pA->pNext = pTail;` |
|   21705 | 1676 | `			pTail = pA;` |
|   21705 | 1677 | `			pA = pA->pPrev;` |
|   10863 | 1678 | `		}else{` |
|   11369 | 1679 | `			pTail->pPrev = pB;` |
|   11369 | 1680 | `			pB->pNext = pTail;` |
|   11369 | 1681 | `			pTail = pB;` |
|   11369 | 1682 | `			pB = pB->pPrev;` |
|       - | 1683 | `		}` |
|       2 | 1684 | `	}` |
|   20998 | 1685 | `	if( pA ){` |
|   15607 | 1686 | `		pTail->pPrev = pA;` |
|   15607 | 1687 | `		pA->pNext = pTail;` |
|   13201 | 1688 | `	}else if( pB ){` |
|    5261 | 1689 | `		pTail->pPrev = pB;` |
|    5261 | 1690 | `		pB->pNext = pTail;` |
|    2626 | 1691 | `	}else{` |
|     134 | 1692 | `		pTail->pPrev = pTail->pNext = 0;` |
|       - | 1693 | `	}` |
|   20998 | 1694 | `	return result.pPrev;` |
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
|     474 | 1708 | `static sxi32 HashmapMergeSort(ph7_hashmap *pMap,ProcNodeCmp xCmp,void *pCmpData)` |
|       2 | 1709 |  |
|       - | 1710 | `	ph7_hashmap_node *a[N_SORT_BUCKET], *p,*pIn;` |
|       - | 1711 | `	sxu32 i;` |
|     476 | 1712 | `	SyZero(a,sizeof(a));` |
|       - | 1713 | `	/* Point to the first inserted entry */` |
|     476 | 1714 | `	pIn = pMap->pFirst;` |
|    7570 | 1715 | `	while( pIn ){` |
|    7096 | 1716 | `		p = pIn;` |
|    7096 | 1717 | `		pIn = p->pPrev;` |
|    7096 | 1718 | `		p->pPrev = 0;` |
|   13398 | 1719 | `		for(i=0; i<N_SORT_BUCKET-1; i++){` |
|   13398 | 1720 | `			if( a[i]==0 ){` |
|    7096 | 1721 | `				a[i] = p;` |
|    7096 | 1722 | `				break;` |
|     ! 0 | 1723 | `			}else{` |
|    6304 | 1724 | `				p = HashmapNodeMerge(a[i],p,xCmp,pCmpData);` |
|    6304 | 1725 | `				a[i] = 0;` |
|       - | 1726 | `			}` |
|    3153 | 1727 | `		}` |
|    7096 | 1728 | `		if( i==N_SORT_BUCKET-1 ){` |
|       - | 1729 | `			/* To get here, there need to be 2^(N_SORT_BUCKET) elements in he input list.` |
|       - | 1730 | `			 * But that is impossible.` |
|       - | 1731 | `			 */` |
|     ! 0 | 1732 | `			a[i] = HashmapNodeMerge(a[i], p,xCmp,pCmpData);` |
|     ! 0 | 1733 | `		}` |
|       2 | 1734 | `	}` |
|     476 | 1735 | `	p = a[0];` |
|   15170 | 1736 | `	for(i=1; i<N_SORT_BUCKET; i++){` |
|   14696 | 1737 | `		p = HashmapNodeMerge(p,a[i],xCmp,pCmpData);` |
|    7349 | 1738 | `	}` |
|     476 | 1739 | `	p->pNext = 0;` |
|       - | 1740 | `	/* Reflect the change */` |
|     476 | 1741 | `	pMap->pFirst = p;` |
|       - | 1742 | `	/* Reset the loop cursor */` |
|     476 | 1743 | `	pMap->pCur = pMap->pFirst;` |
|     476 | 1744 | `	return SXRET_OK;` |
|       2 | 1745 |  |
|       - | 1746 | `/*` |
|       - | 1747 | ` * Node comparison callback.` |
|       - | 1748 | ` * used-by: [sort(),asort(),...]` |
|       - | 1749 | ` */` |
|   33002 | 1750 | `static sxi32 HashmapCmpCallback1(ph7_hashmap_node *pA,ph7_hashmap_node *pB,void *pCmpData)` |
|       2 | 1751 |  |
|       - | 1752 | `	ph7_value sA,sB;` |
|       - | 1753 | `	sxi32 iFlags;` |
|       - | 1754 | `	int rc;` |
|   33004 | 1755 | `	if( pCmpData == 0 ){` |
|       - | 1756 | `		/* Perform a standard comparison */` |
|   33000 | 1757 | `		rc = HashmapNodeCmp(pA,pB,FALSE);` |
|   33000 | 1758 | `		return rc;` |
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
|   16523 | 1784 |  |
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
|      20 | 1990 | `static sxi32 HashmapCmpCallback7(ph7_hashmap_node *pA,ph7_hashmap_node *pB,void *pCmpData)` |
|       1 | 1991 |  |
|       - | 1992 | `	sxu32 n;` |
|      10 | 1993 | `	SXUNUSED(pB); /* cc warning */` |
|      10 | 1994 | `	SXUNUSED(pCmpData);` |
|       - | 1995 | `	/* Grab a random number */` |
|      21 | 1996 | `	n = PH7_VmRandomNum(pA->pMap->pVm);` |
|       - | 1997 | `	/* if the random number is odd then the first node 'pA' is greater then` |
|       - | 1998 | `	 * the second node 'pB'. Otherwise the reverse is assumed.` |
|       - | 1999 | `	 */` |
|      21 | 2000 | `	return n&1 ? 1 : -1;` |
|       1 | 2001 |  |
|       - | 2002 | `/*` |
|       - | 2003 | ` * Rehash all nodes keys after a merge-sort have been applied.` |
|       - | 2004 | ` * Used by [sort(),usort() and rsort()].` |
|       - | 2005 | ` */` |
|     458 | 2006 | `static void HashmapSortRehash(ph7_hashmap *pMap)` |
|       2 | 2007 |  |
|       - | 2008 | `	ph7_hashmap_node *p,*pLast;` |
|       - | 2009 | `	sxu32 i;` |
|       - | 2010 | `	/* Rehash all entries */` |
|     460 | 2011 | `	pLast = p = pMap->pFirst;` |
|     460 | 2012 | `	pMap->iNextIdx = 0; /* Reset the automatic index */` |
|     460 | 2013 | `	i = 0;` |
|    3749 | 2014 | `	for( ;; ){` |
|    7500 | 2015 | `		if( i >= pMap->nEntry ){` |
|     460 | 2016 | `			pMap->pLast = pLast; /* Fix the last link broken by the merge-sort */` |
|     460 | 2017 | `			break;` |
|       - | 2018 | `		}` |
|    7042 | 2019 | `		if( p->iType == HASHMAP_BLOB_NODE ){` |
|       - | 2020 | `			/* Do not maintain index association as requested by the PHP specification */` |
|       5 | 2021 | `			SyBlobRelease(&p->xKey.sKey);` |
|       - | 2022 | `			/* Change key type */` |
|       5 | 2023 | `			p->iType = HASHMAP_INT_NODE;` |
|       2 | 2024 | `		}` |
|    7042 | 2025 | `		HashmapRehashIntNode(p);` |
|       - | 2026 | `		/* Point to the next entry */` |
|    7042 | 2027 | `		i++;` |
|    7042 | 2028 | `		pLast = p;` |
|    7042 | 2029 | `		p = p->pPrev; /* Reverse link */` |
|       2 | 2030 | `	}` |
|     460 | 2031 |  |
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
|     788 | 2053 | `static int ph7_hashmap_sort(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       2 | 2054 |  |
|       - | 2055 | `	ph7_hashmap *pMap;` |
|       - | 2056 | `	/* Make sure we are dealing with a valid hashmap */` |
|     790 | 2057 | `	if( nArg < 1 \|\| !ph7_value_is_array(apArg[0]) ){` |
|       - | 2058 | `		/* Missing/Invalid arguments,return FALSE */` |
|     ! 0 | 2059 | `		ph7_result_bool(pCtx,0);` |
|     ! 0 | 2060 | `		return PH7_OK;` |
|       - | 2061 | `	}` |
|       - | 2062 | `	/* Point to the internal representation of the input hashmap */` |
|     790 | 2063 | `	pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|     790 | 2064 | `	if( pMap->nEntry > 1 ){` |
|     454 | 2065 | `		sxi32 iCmpFlags = 0;` |
|     454 | 2066 | `		if( nArg > 1 ){` |
|       - | 2067 | `			/* Extract comparison flags */` |
|       3 | 2068 | `			iCmpFlags = ph7_value_to_int(apArg[1]);` |
|       3 | 2069 | `			if( iCmpFlags == 3 /* SORT_REGULAR */ ){` |
|     ! 0 | 2070 | `				iCmpFlags = 0; /* Standard comparison */` |
|     ! 0 | 2071 | `			}` |
|       1 | 2072 | `		}` |
|       - | 2073 | `		/* Do the merge sort */` |
|     454 | 2074 | `		HashmapMergeSort(pMap,HashmapCmpCallback1,SX_INT_TO_PTR(iCmpFlags));` |
|       - | 2075 | `		/* Rehash [Do not maintain index association as requested by the PHP specification] */` |
|     454 | 2076 | `		HashmapSortRehash(pMap);` |
|     226 | 2077 | `	}` |
|       - | 2078 | `	/* All done,return TRUE */` |
|     790 | 2079 | `	ph7_result_bool(pCtx,1);` |
|     790 | 2080 | `	return PH7_OK;` |
|     396 | 2081 |  |
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
|       4 | 2476 | `		while(pMap->pLast->pPrev){` |
|       2 | 2477 | `			pMap->pLast = pMap->pLast->pPrev;` |
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
|     458 | 2498 | `static int ph7_hashmap_count(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       2 | 2499 |  |
|     460 | 2500 | `	int bRecursive = FALSE;` |
|       - | 2501 | `	sxi64 iCount;` |
|     460 | 2502 | `	if( nArg < 1 ){` |
|       - | 2503 | `		/* Missing arguments,return 0 */` |
|     ! 0 | 2504 | `		ph7_result_int(pCtx,0);` |
|     ! 0 | 2505 | `		return PH7_OK;` |
|       - | 2506 | `	}` |
|     460 | 2507 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|       - | 2508 | `		/* TICKET 1433-19: Handle objects */` |
|       3 | 2509 | `		int res = !ph7_value_is_null(apArg[0]);` |
|       3 | 2510 | `		ph7_result_int(pCtx,res);` |
|       3 | 2511 | `		return PH7_OK;` |
|       - | 2512 | `	}` |
|     458 | 2513 | `	if( nArg > 1 ){` |
|       - | 2514 | `		/* Recursive count? */` |
|      31 | 2515 | `		bRecursive = ph7_value_to_int(apArg[1]) == 1 /* COUNT_RECURSIVE */;` |
|      15 | 2516 | `	}` |
|       - | 2517 | `	/* Count */` |
|     458 | 2518 | `	iCount = HashmapCount((ph7_hashmap *)apArg[0]->x.pOther,bRecursive,0);` |
|     458 | 2519 | `	ph7_result_int64(pCtx,iCount);` |
|     458 | 2520 | `	return PH7_OK;` |
|     231 | 2521 |  |
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
|       2 | 2620 | `static int ph7_hashmap_push(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       1 | 2621 |  |
|       - | 2622 | `	ph7_hashmap *pMap;` |
|       - | 2623 | `	sxi32 rc;` |
|       - | 2624 | `	int i;` |
|       3 | 2625 | `	if( nArg < 1 ){` |
|       - | 2626 | `		/* Missing arguments,return 0 */` |
|     ! 0 | 2627 | `		ph7_result_int(pCtx,0);` |
|     ! 0 | 2628 | `		return PH7_OK;` |
|       - | 2629 | `	}` |
|       - | 2630 | `	/* Make sure we are dealing with a valid hashmap */` |
|       3 | 2631 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|       - | 2632 | `		/* Invalid argument,return 0 */` |
|     ! 0 | 2633 | `		ph7_result_int(pCtx,0);` |
|     ! 0 | 2634 | `		return PH7_OK;` |
|       - | 2635 | `	}` |
|       - | 2636 | `	/* Point to the internal representation of the input hashmap */` |
|       3 | 2637 | `	pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|       - | 2638 | `	/* Start pushing given values */` |
|       7 | 2639 | `	for( i = 1 ; i < nArg ; ++i ){` |
|       5 | 2640 | `		rc = PH7_HashmapInsert(pMap,0,apArg[i]);` |
|       5 | 2641 | `		if( rc != SXRET_OK ){` |
|     ! 0 | 2642 | `			break;` |
|       - | 2643 | `		}` |
|       3 | 2644 | `	}` |
|       - | 2645 | `	/* Return the new count */` |
|       3 | 2646 | `	ph7_result_int64(pCtx,(sxi64)pMap->nEntry);` |
|       3 | 2647 | `	return PH7_OK;` |
|       2 | 2648 |  |
|       - | 2649 | `/*` |
|       - | 2650 | ` * value array_shift(array $array)` |
|       - | 2651 | ` *   Shift an element off the beginning of array.` |
|       - | 2652 | ` * Parameter` |
|       - | 2653 | ` *  The array to get the value from.` |
|       - | 2654 | ` * Return` |
|       - | 2655 | ` *  Shifted value or NULL on failure.` |
|       - | 2656 | ` */` |
|      36 | 2657 | `static int ph7_hashmap_shift(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       2 | 2658 |  |
|       - | 2659 | `	ph7_hashmap *pMap;` |
|       - | 2660 | `	/* PHP requires exactly one argument and it must be passed by reference */` |
|      38 | 2661 | `	if( nArg != 1 ){` |
|       7 | 2662 | `		return PH7_VmThrowException(pCtx,` |
|       - | 2663 | `			"ArgumentCountError",` |
|       - | 2664 | `			"array_shift() expects exactly 1 argument, %d given",` |
|       2 | 2665 | `			nArg` |
|       - | 2666 | `			);` |
|       - | 2667 | `	}` |
|       - | 2668 | `	/* Detect constants or literals, which cannot be passed by reference. */` |
|      34 | 2669 | `	if( apArg[0]->nIdx == SXU32_HIGH ){` |
|       5 | 2670 | `		return PH7_VmThrowException(pCtx,` |
|       - | 2671 | `			"Error",` |
|       - | 2672 | `			"array_shift(): Argument #1 ($array) could not be passed by reference"` |
|       - | 2673 | `			);` |
|       - | 2674 | `	}` |
|       - | 2675 | `	/* Make sure we are dealing with a valid hashmap */` |
|      30 | 2676 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|       4 | 2677 | `		return PH7_VmThrowException(pCtx,` |
|       - | 2678 | `			"TypeError",` |
|       - | 2679 | `			"array_shift(): Argument #1 ($array) must be of type array, %s given",` |
|       1 | 2680 | `			ph7_type_name(apArg[0])` |
|       - | 2681 | `			);` |
|       - | 2682 | `	}` |
|       - | 2683 | `	/* Point to the internal representation of the hashmap */` |
|      28 | 2684 | `	pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|      28 | 2685 | `	if( pMap->nEntry < 1 ){` |
|       - | 2686 | `		/* Empty hashmap,return NULL */` |
|       3 | 2687 | `		ph7_result_null(pCtx);` |
|       2 | 2688 | `	}else{` |
|      26 | 2689 | `		ph7_hashmap_node *pEntry = pMap->pFirst;` |
|       - | 2690 | `		ph7_value *pObj;` |
|       - | 2691 | `		sxu32 n;` |
|      26 | 2692 | `		pObj = HashmapExtractNodeValue(pEntry);` |
|      26 | 2693 | `		if( pObj ){` |
|       - | 2694 | `			/* Node value */` |
|      26 | 2695 | `			ph7_result_value(pCtx,pObj);` |
|       - | 2696 | `			/* Unlink the first node */` |
|      26 | 2697 | `			PH7_HashmapUnlinkNode(pEntry,TRUE);` |
|      14 | 2698 | `		}else{` |
|     ! 0 | 2699 | `			ph7_result_null(pCtx);` |
|       - | 2700 | `		}` |
|       - | 2701 | `		/* Rehash all int keys */` |
|      26 | 2702 | `		n = pMap->nEntry;` |
|      26 | 2703 | `		pEntry = pMap->pFirst;` |
|      26 | 2704 | `		pMap->iNextIdx = 0; /* Reset the automatic index */` |
|      37 | 2705 | `		for(;;){` |
|      76 | 2706 | `			if( n < 1 ){` |
|      26 | 2707 | `				break;` |
|       - | 2708 | `			}` |
|      52 | 2709 | `			if( pEntry->iType == HASHMAP_INT_NODE ){` |
|      52 | 2710 | `				HashmapRehashIntNode(pEntry);` |
|      25 | 2711 | `			}` |
|       - | 2712 | `			/* Point to the next entry */` |
|      52 | 2713 | `			pEntry = pEntry->pPrev; /* Reverse link */` |
|      52 | 2714 | `			n--;` |
|       2 | 2715 | `		}` |
|       - | 2716 | `		/* Reset the cursor */` |
|      26 | 2717 | `		pMap->pCur = pMap->pFirst;` |
|       - | 2718 | `	}` |
|      28 | 2719 | `	return PH7_OK;` |
|      20 | 2720 |  |
|       - | 2721 | `/*` |
|       - | 2722 | ` * Extract the node cursor value.` |
|       - | 2723 | ` */` |
|      24 | 2724 | `static sxi32 HashmapCurrentValue(ph7_context *pCtx,ph7_hashmap *pMap,int iDirection)` |
|       1 | 2725 |  |
|      25 | 2726 | `	ph7_hashmap_node *pCur = pMap->pCur;` |
|       - | 2727 | `	ph7_value *pVal;` |
|      25 | 2728 | `	if( pCur == 0 ){` |
|       - | 2729 | `		/* Cursor does not point to anything,return FALSE */` |
|     ! 0 | 2730 | `		ph7_result_bool(pCtx,0);` |
|     ! 0 | 2731 | `		return PH7_OK;` |
|       - | 2732 | `	}` |
|      25 | 2733 | `	if( iDirection != 0 ){` |
|       9 | 2734 | `		if( iDirection > 0 ){` |
|       - | 2735 | `			/* Point to the next entry */` |
|       7 | 2736 | `			pMap->pCur = pCur->pPrev; /* Reverse link */` |
|       7 | 2737 | `			pCur = pMap->pCur;` |
|       4 | 2738 | `		}else{` |
|       - | 2739 | `			/* Point to the previous entry */` |
|       3 | 2740 | `			pMap->pCur = pCur->pNext; /* Reverse link */` |
|       3 | 2741 | `			pCur = pMap->pCur;` |
|       - | 2742 | `		}` |
|       9 | 2743 | `		if( pCur == 0 ){` |
|       - | 2744 | `			/* End of input reached,return FALSE */` |
|     ! 0 | 2745 | `			ph7_result_bool(pCtx,0);` |
|     ! 0 | 2746 | `			return PH7_OK;` |
|       - | 2747 | `		}` |
|       4 | 2748 | `	}` |
|       - | 2749 | `	/* Point to the desired element */` |
|      25 | 2750 | `	pVal = HashmapExtractNodeValue(pCur);` |
|      25 | 2751 | `	if( pVal ){` |
|      25 | 2752 | `		ph7_result_value(pCtx,pVal);` |
|      13 | 2753 | `	}else{` |
|     ! 0 | 2754 | `		ph7_result_bool(pCtx,0);` |
|       - | 2755 | `	}` |
|      25 | 2756 | `	return PH7_OK;` |
|      13 | 2757 |  |
|       - | 2758 | `/*` |
|       - | 2759 | ` * value current(array $array)` |
|       - | 2760 | ` *  Return the current element in an array.` |
|       - | 2761 | ` * Parameter` |
|       - | 2762 | ` *  $input: The input array.` |
|       - | 2763 | ` * Return` |
|       - | 2764 | ` *  The current() function simply returns the value of the array element that's currently` |
|       - | 2765 | ` *  being pointed to by the internal pointer. It does not move the pointer in any way.` |
|       - | 2766 | ` *  If the internal pointer points beyond the end of the elements list or the array` |
|       - | 2767 | ` *  is empty, current() returns FALSE.` |
|       - | 2768 | ` */` |
|      10 | 2769 | `static int ph7_hashmap_current(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       1 | 2770 |  |
|      11 | 2771 | `	if( nArg < 1 ){` |
|       - | 2772 | `		/* Missing arguments,return FALSE */` |
|     ! 0 | 2773 | `		ph7_result_bool(pCtx,0);` |
|     ! 0 | 2774 | `		return PH7_OK;` |
|       - | 2775 | `	}` |
|       - | 2776 | `	/* Make sure we are dealing with a valid hashmap */` |
|      11 | 2777 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|       - | 2778 | `		/* Invalid argument,return FALSE */` |
|     ! 0 | 2779 | `		ph7_result_bool(pCtx,0);` |
|     ! 0 | 2780 | `		return PH7_OK;` |
|       - | 2781 | `	}` |
|      11 | 2782 | `	HashmapCurrentValue(&(*pCtx),(ph7_hashmap *)apArg[0]->x.pOther,0);` |
|      11 | 2783 | `	return PH7_OK;` |
|       6 | 2784 |  |
|       - | 2785 | `/*` |
|       - | 2786 | ` * value next(array $input)` |
|       - | 2787 | ` *  Advance the internal array pointer of an array.` |
|       - | 2788 | ` * Parameter` |
|       - | 2789 | ` *  $input: The input array.` |
|       - | 2790 | ` * Return` |
|       - | 2791 | ` *  next() behaves like current(), with one difference. It advances the internal array` |
|       - | 2792 | ` *  pointer one place forward before returning the element value. That means it returns` |
|       - | 2793 | ` *  the next array value and advances the internal array pointer by one.` |
|       - | 2794 | ` */` |
|       6 | 2795 | `static int ph7_hashmap_next(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       1 | 2796 |  |
|       7 | 2797 | `	if( nArg < 1 ){` |
|       - | 2798 | `		/* Missing arguments,return FALSE */` |
|     ! 0 | 2799 | `		ph7_result_bool(pCtx,0);` |
|     ! 0 | 2800 | `		return PH7_OK;` |
|       - | 2801 | `	}` |
|       - | 2802 | `	/* Make sure we are dealing with a valid hashmap */` |
|       7 | 2803 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|       - | 2804 | `		/* Invalid argument,return FALSE */` |
|     ! 0 | 2805 | `		ph7_result_bool(pCtx,0);` |
|     ! 0 | 2806 | `		return PH7_OK;` |
|       - | 2807 | `	}` |
|       7 | 2808 | `	HashmapCurrentValue(&(*pCtx),(ph7_hashmap *)apArg[0]->x.pOther,1);` |
|       7 | 2809 | `	return PH7_OK;` |
|       4 | 2810 |  |
|       - | 2811 | `/*` |
|       - | 2812 | ` * value prev(array $input)` |
|       - | 2813 | ` *  Rewind the internal array pointer.` |
|       - | 2814 | ` * Parameter` |
|       - | 2815 | ` *  $input: The input array.` |
|       - | 2816 | ` * Return` |
|       - | 2817 | ` *  Returns the array value in the previous place that's pointed` |
|       - | 2818 | ` *  to by the internal array pointer, or FALSE if there are no more` |
|       - | 2819 | ` *  elements.` |
|       - | 2820 | ` */` |
|       2 | 2821 | `static int ph7_hashmap_prev(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       1 | 2822 |  |
|       3 | 2823 | `	if( nArg < 1 ){` |
|       - | 2824 | `		/* Missing arguments,return FALSE */` |
|     ! 0 | 2825 | `		ph7_result_bool(pCtx,0);` |
|     ! 0 | 2826 | `		return PH7_OK;` |
|       - | 2827 | `	}` |
|       - | 2828 | `	/* Make sure we are dealing with a valid hashmap */` |
|       3 | 2829 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|       - | 2830 | `		/* Invalid argument,return FALSE */` |
|     ! 0 | 2831 | `		ph7_result_bool(pCtx,0);` |
|     ! 0 | 2832 | `		return PH7_OK;` |
|       - | 2833 | `	}` |
|       3 | 2834 | `	HashmapCurrentValue(&(*pCtx),(ph7_hashmap *)apArg[0]->x.pOther,-1);` |
|       3 | 2835 | `	return PH7_OK;` |
|       2 | 2836 |  |
|       - | 2837 | `/*` |
|       - | 2838 | ` * value end(array $input)` |
|       - | 2839 | ` *  Set the internal pointer of an array to its last element.` |
|       - | 2840 | ` * Parameter` |
|       - | 2841 | ` *  $input: The input array.` |
|       - | 2842 | ` * Return` |
|       - | 2843 | ` *  Returns the value of the last element or FALSE for empty array.` |
|       - | 2844 | ` */` |
|       2 | 2845 | `static int ph7_hashmap_end(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       1 | 2846 |  |
|       - | 2847 | `	ph7_hashmap *pMap;` |
|       3 | 2848 | `	if( nArg < 1 ){` |
|       - | 2849 | `		/* Missing arguments,return FALSE */` |
|     ! 0 | 2850 | `		ph7_result_bool(pCtx,0);` |
|     ! 0 | 2851 | `		return PH7_OK;` |
|       - | 2852 | `	}` |
|       - | 2853 | `	/* Make sure we are dealing with a valid hashmap */` |
|       3 | 2854 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|       - | 2855 | `		/* Invalid argument,return FALSE */` |
|     ! 0 | 2856 | `		ph7_result_bool(pCtx,0);` |
|     ! 0 | 2857 | `		return PH7_OK;` |
|       - | 2858 | `	}` |
|       - | 2859 | `	/* Point to the internal representation of the input hashmap */` |
|       3 | 2860 | `	pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|       - | 2861 | `	/* Point to the last node */` |
|       3 | 2862 | `	pMap->pCur = pMap->pLast;` |
|       - | 2863 | `	/* Return the last node value */` |
|       3 | 2864 | `	HashmapCurrentValue(&(*pCtx),pMap,0);` |
|       3 | 2865 | `	return PH7_OK;` |
|       2 | 2866 |  |
|       - | 2867 | `/*` |
|       - | 2868 | ` * value reset(array $array )` |
|       - | 2869 | ` *  Set the internal pointer of an array to its first element.` |
|       - | 2870 | ` * Parameter` |
|       - | 2871 | ` *  $input: The input array.` |
|       - | 2872 | ` * Return` |
|       - | 2873 | ` *  Returns the value of the first array element,or FALSE if the array is empty.` |
|       - | 2874 | ` */` |
|       4 | 2875 | `static int ph7_hashmap_reset(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       1 | 2876 |  |
|       - | 2877 | `	ph7_hashmap *pMap;` |
|       5 | 2878 | `	if( nArg < 1 ){` |
|       - | 2879 | `		/* Missing arguments,return FALSE */` |
|     ! 0 | 2880 | `		ph7_result_bool(pCtx,0);` |
|     ! 0 | 2881 | `		return PH7_OK;` |
|       - | 2882 | `	}` |
|       - | 2883 | `	/* Make sure we are dealing with a valid hashmap */` |
|       5 | 2884 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|       - | 2885 | `		/* Invalid argument,return FALSE */` |
|     ! 0 | 2886 | `		ph7_result_bool(pCtx,0);` |
|     ! 0 | 2887 | `		return PH7_OK;` |
|       - | 2888 | `	}` |
|       - | 2889 | `	/* Point to the internal representation of the input hashmap */` |
|       5 | 2890 | `	pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|       - | 2891 | `	/* Point to the first node */` |
|       5 | 2892 | `	pMap->pCur = pMap->pFirst;` |
|       - | 2893 | `	/* Return the last node value if available */` |
|       5 | 2894 | `	HashmapCurrentValue(&(*pCtx),pMap,0);` |
|       5 | 2895 | `	return PH7_OK;` |
|       3 | 2896 |  |
|       - | 2897 | `/*` |
|       - | 2898 | ` * value key(array $array)` |
|       - | 2899 | ` *   Fetch a key from an array` |
|       - | 2900 | ` * Parameter` |
|       - | 2901 | ` *  $input` |
|       - | 2902 | ` *   The input array.` |
|       - | 2903 | ` * Return` |
|       - | 2904 | ` *  The key() function simply returns the key of the array element that's currently` |
|       - | 2905 | ` *  being pointed to by the internal pointer. It does not move the pointer in any way.` |
|       - | 2906 | ` *  If the internal pointer points beyond the end of the elements list or the array` |
|       - | 2907 | ` *  is empty, key() returns NULL.` |
|       - | 2908 | ` */` |
|       4 | 2909 | `static int ph7_hashmap_simple_key(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       1 | 2910 |  |
|       - | 2911 | `	ph7_hashmap_node *pCur;` |
|       - | 2912 | `	ph7_hashmap *pMap;` |
|       5 | 2913 | `	if( nArg < 1 ){` |
|       - | 2914 | `		/* Missing arguments,return NULL */` |
|     ! 0 | 2915 | `		ph7_result_null(pCtx);` |
|     ! 0 | 2916 | `		return PH7_OK;` |
|       - | 2917 | `	}` |
|       - | 2918 | `	/* Make sure we are dealing with a valid hashmap */` |
|       5 | 2919 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|       - | 2920 | `		/* Invalid argument,return NULL */` |
|     ! 0 | 2921 | `		ph7_result_null(pCtx);` |
|     ! 0 | 2922 | `		return PH7_OK;` |
|       - | 2923 | `	}` |
|       5 | 2924 | `	pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|       5 | 2925 | `	pCur = pMap->pCur;` |
|       5 | 2926 | `	if( pCur == 0 ){` |
|       - | 2927 | `		/* Cursor does not point to anything,return NULL */` |
|     ! 0 | 2928 | `		ph7_result_null(pCtx);` |
|     ! 0 | 2929 | `		return PH7_OK;` |
|       - | 2930 | `	}` |
|       5 | 2931 | `	if( pCur->iType == HASHMAP_INT_NODE){` |
|       - | 2932 | `		/* Key is integer */` |
|     ! 0 | 2933 | `		ph7_result_int64(pCtx,pCur->xKey.iKey);` |
|     ! 0 | 2934 | `	}else{` |
|       - | 2935 | `		/* Key is blob */` |
|       7 | 2936 | `		ph7_result_string(pCtx,` |
|       4 | 2937 | `			(const char *)SyBlobData(&pCur->xKey.sKey),(int)SyBlobLength(&pCur->xKey.sKey));` |
|       - | 2938 | `	}` |
|       5 | 2939 | `	return PH7_OK;` |
|       3 | 2940 |  |
|       - | 2941 | `/*` |
|       - | 2942 | ` * array each(array $input)` |
|       - | 2943 | ` *  Return the current key and value pair from an array and advance the array cursor.` |
|       - | 2944 | ` * Parameter` |
|       - | 2945 | ` *  $input` |
|       - | 2946 | ` *    The input array.` |
|       - | 2947 | ` * Return` |
|       - | 2948 | ` *  Returns the current key and value pair from the array array. This pair is returned` |
|       - | 2949 | ` *  in a four-element array, with the keys 0, 1, key, and value. Elements 0 and key` |
|       - | 2950 | ` *  contain the key name of the array element, and 1 and value contain the data.` |
|       - | 2951 | ` *  If the internal pointer for the array points past the end of the array contents` |
|       - | 2952 | ` *  each() returns FALSE.` |
|       - | 2953 | ` */` |
|      22 | 2954 | `static int ph7_hashmap_each(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       1 | 2955 |  |
|       - | 2956 | `	ph7_hashmap_node *pCur;` |
|       - | 2957 | `	ph7_hashmap *pMap;` |
|       - | 2958 | `	ph7_value *pArray;` |
|       - | 2959 | `	ph7_value *pVal;` |
|       - | 2960 | `	ph7_value sKey;` |
|      23 | 2961 | `	if( nArg < 1 ){` |
|       - | 2962 | `		/* Missing arguments,return FALSE */` |
|     ! 0 | 2963 | `		ph7_result_bool(pCtx,0);` |
|     ! 0 | 2964 | `		return PH7_OK;` |
|       - | 2965 | `	}` |
|       - | 2966 | `	/* Make sure we are dealing with a valid hashmap */` |
|      23 | 2967 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|       - | 2968 | `		/* Invalid argument,return FALSE */` |
|     ! 0 | 2969 | `		ph7_result_bool(pCtx,0);` |
|     ! 0 | 2970 | `		return PH7_OK;` |
|       - | 2971 | `	}` |
|       - | 2972 | `	/* Point to the internal representation that describe the input hashmap */` |
|      23 | 2973 | `	pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|      23 | 2974 | `	if( pMap->pCur == 0 ){` |
|       - | 2975 | `		/* Cursor does not point to anything,return FALSE */` |
|       9 | 2976 | `		ph7_result_bool(pCtx,0);` |
|       9 | 2977 | `		return PH7_OK;` |
|       - | 2978 | `	}` |
|      15 | 2979 | `	pCur = pMap->pCur;` |
|       - | 2980 | `	/* Create a new array */` |
|      15 | 2981 | `	pArray = ph7_context_new_array(pCtx);` |
|      15 | 2982 | `	if( pArray == 0 ){` |
|     ! 0 | 2983 | `		ph7_result_bool(pCtx,0);` |
|     ! 0 | 2984 | `		return PH7_OK;` |
|       - | 2985 | `	}` |
|      15 | 2986 | `	pVal = HashmapExtractNodeValue(pCur);` |
|       - | 2987 | `	/* Insert the current value */` |
|      15 | 2988 | `	ph7_array_add_intkey_elem(pArray,1,pVal);` |
|      15 | 2989 | `	ph7_array_add_strkey_elem(pArray,"value",pVal);` |
|       - | 2990 | `	/* Make the key */` |
|      15 | 2991 | `	if( pCur->iType == HASHMAP_INT_NODE ){` |
|       7 | 2992 | `		PH7_MemObjInitFromInt(pMap->pVm,&sKey,pCur->xKey.iKey);` |
|       4 | 2993 | `	}else{` |
|       9 | 2994 | `		PH7_MemObjInitFromString(pMap->pVm,&sKey,0);` |
|       9 | 2995 | `		PH7_MemObjStringAppend(&sKey,(const char *)SyBlobData(&pCur->xKey.sKey),SyBlobLength(&pCur->xKey.sKey));` |
|       - | 2996 | `	}` |
|       - | 2997 | `	/* Insert the current key */` |
|      15 | 2998 | `	ph7_array_add_intkey_elem(pArray,0,&sKey);` |
|      15 | 2999 | `	ph7_array_add_strkey_elem(pArray,"key",&sKey);` |
|      15 | 3000 | `	PH7_MemObjRelease(&sKey);` |
|       - | 3001 | `	/* Advance the cursor */` |
|      15 | 3002 | `	pMap->pCur = pCur->pPrev; /* Reverse link */` |
|       - | 3003 | `	/* Return the current entry */` |
|      15 | 3004 | `	ph7_result_value(pCtx,pArray);` |
|      15 | 3005 | `	return PH7_OK;` |
|      12 | 3006 |  |
|       - | 3007 | `/*` |
|       - | 3008 | ` * array range(int $start,int $limit,int $step)` |
|       - | 3009 | ` *  Create an array containing a range of elements` |
|       - | 3010 | ` * Parameter` |
|       - | 3011 | ` *  start` |
|       - | 3012 | ` *   First value of the sequence.` |
|       - | 3013 | ` *  limit` |
|       - | 3014 | ` *   The sequence is ended upon reaching the limit value.` |
|       - | 3015 | ` *  step` |
|       - | 3016 | ` *  If a step value is given, it will be used as the increment between elements in the sequence.` |
|       - | 3017 | ` *  step should be given as a positive number. If not specified, step will default to 1.` |
|       - | 3018 | ` * Return` |
|       - | 3019 | ` *  An array of elements from start to limit, inclusive.` |
|       - | 3020 | ` * NOTE:` |
|       - | 3021 | ` *  Only 32/64 bit integer key is supported.` |
|       - | 3022 | ` */` |
|       2 | 3023 | `static int ph7_hashmap_range(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       1 | 3024 |  |
|       - | 3025 | `	ph7_value *pValue,*pArray;` |
|       - | 3026 | `	sxi64 iOfft,iLimit;` |
|       3 | 3027 | `	int iStep = 1;` |
|       - | 3028 |  |
|       3 | 3029 | `	iOfft = iLimit = 0; /* cc -O6 */` |
|       3 | 3030 | `	if( nArg > 0 ){` |
|       - | 3031 | `		/* Extract the offset */` |
|       3 | 3032 | `		iOfft = ph7_value_to_int64(apArg[0]);` |
|       3 | 3033 | `		if( nArg > 1 ){` |
|       - | 3034 | `			/* Extract the limit */` |
|       3 | 3035 | `			iLimit = ph7_value_to_int64(apArg[1]);` |
|       3 | 3036 | `			if( nArg > 2 ){` |
|       - | 3037 | `				/* Extract the increment */` |
|       3 | 3038 | `				iStep = ph7_value_to_int(apArg[2]);` |
|       3 | 3039 | `				if( iStep < 1 ){` |
|       - | 3040 | `					/* Only positive number are allowed */` |
|       3 | 3041 | `					iStep = 1;` |
|       1 | 3042 | `				}` |
|       1 | 3043 | `			}` |
|       1 | 3044 | `		}` |
|       1 | 3045 | `	}` |
|       - | 3046 | `	/* Element container */` |
|       3 | 3047 | `	pValue = ph7_context_new_scalar(pCtx);` |
|       - | 3048 | `	/* Create the new array */` |
|       3 | 3049 | `	pArray = ph7_context_new_array(pCtx);` |
|       3 | 3050 | `	if( pArray == 0 ){` |
|     ! 0 | 3051 | `		ph7_result_null(pCtx);` |
|     ! 0 | 3052 | `		return PH7_OK;` |
|       - | 3053 | `	}` |
|       - | 3054 | `	/* Start filling */` |
|       3 | 3055 | `	while( iOfft <= iLimit ){` |
|     ! 0 | 3056 | `		ph7_value_int64(pValue,iOfft);` |
|       - | 3057 | `		/* Perform the insertion */` |
|     ! 0 | 3058 | `		ph7_array_add_elem(pArray,0/* Automatic index assign*/,pValue);` |
|       - | 3059 | `		/* Increment */` |
|     ! 0 | 3060 | `		iOfft += iStep;` |
|     ! 0 | 3061 | `	}` |
|       - | 3062 | `	/* Return the new array */` |
|       3 | 3063 | `	ph7_result_value(pCtx,pArray);` |
|       - | 3064 | `	/* Dont'worry about freeing 'pValue',it will be released automatically` |
|       - | 3065 | `	 * by the virtual machine as soon we return from this foreign function.` |
|       - | 3066 | `	 */` |
|       3 | 3067 | `	return PH7_OK;` |
|       2 | 3068 |  |
|       - | 3069 | `/*` |
|       - | 3070 | ` * array array_values(array $input)` |
|       - | 3071 | ` *   Returns all the values from the input array and indexes numerically the array.` |
|       - | 3072 | ` * Parameters` |
|       - | 3073 | ` *   input: The input array.` |
|       - | 3074 | ` * Return` |
|       - | 3075 | ` *  An indexed array of values or NULL on failure.` |
|       - | 3076 | ` */` |
|      24 | 3077 | `static int ph7_hashmap_values(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       1 | 3078 |  |
|       - | 3079 | `	ph7_hashmap_node *pNode;` |
|       - | 3080 | `	ph7_hashmap *pMap;` |
|       - | 3081 | `	ph7_value *pArray;` |
|       - | 3082 | `	ph7_value *pObj;` |
|       - | 3083 | `	sxu32 n;` |
|      25 | 3084 | `	if( nArg < 1 ){` |
|       - | 3085 | `		/* Missing arguments,return NULL */` |
|       3 | 3086 | `		ph7_result_null(pCtx);` |
|       3 | 3087 | `		return PH7_OK;` |
|       - | 3088 | `	}` |
|       - | 3089 | `	/* Make sure we are dealing with a valid hashmap */` |
|      23 | 3090 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|       - | 3091 | `		/* Invalid argument,return NULL */` |
|     ! 0 | 3092 | `		ph7_result_null(pCtx);` |
|     ! 0 | 3093 | `		return PH7_OK;` |
|       - | 3094 | `	}` |
|       - | 3095 | `	/* Point to the internal representation that describe the input hashmap */` |
|      23 | 3096 | `	pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|       - | 3097 | `	/* Create a new array */` |
|      23 | 3098 | `	pArray = ph7_context_new_array(pCtx);` |
|      23 | 3099 | `	if( pArray == 0 ){` |
|     ! 0 | 3100 | `		ph7_result_null(pCtx);` |
|     ! 0 | 3101 | `		return PH7_OK;` |
|       - | 3102 | `	}` |
|       - | 3103 | `	/* Perform the requested operation */` |
|      23 | 3104 | `	pNode = pMap->pFirst;` |
|      81 | 3105 | `	for( n = 0 ; n < pMap->nEntry ; ++n ){` |
|      59 | 3106 | `		pObj = HashmapExtractNodeValue(pNode);` |
|      59 | 3107 | `		if( pObj ){` |
|       - | 3108 | `			/* perform the insertion */` |
|      59 | 3109 | `			ph7_array_add_elem(pArray,0/* Automatic index assign */,pObj);` |
|      29 | 3110 | `		}` |
|       - | 3111 | `		/* Point to the next entry */` |
|      59 | 3112 | `		pNode = pNode->pPrev; /* Reverse link */` |
|      30 | 3113 | `	}` |
|       - | 3114 | `	/* return the new array */` |
|      23 | 3115 | `	ph7_result_value(pCtx,pArray);` |
|      23 | 3116 | `	return PH7_OK;` |
|      13 | 3117 |  |
|       - | 3118 | `/*` |
|       - | 3119 | ` * array array_keys(array $input [, val $search_value [, bool $strict = false ]] )` |
|       - | 3120 | ` *  Return all the keys or a subset of the keys of an array.` |
|       - | 3121 | ` * Parameters` |
|       - | 3122 | ` *  $input` |
|       - | 3123 | ` *   An array containing keys to return.` |
|       - | 3124 | ` * $search_value` |
|       - | 3125 | ` *   If specified, then only keys containing these values are returned.` |
|       - | 3126 | ` * $strict` |
|       - | 3127 | ` *   Determines if strict comparison (===) should be used during the search.` |
|       - | 3128 | ` * Return` |
|       - | 3129 | ` *  An array of all the keys in input or NULL on failure.` |
|       - | 3130 | ` */` |
|      94 | 3131 | `static int ph7_hashmap_keys(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       1 | 3132 |  |
|       - | 3133 | `	ph7_hashmap_node *pNode;` |
|       - | 3134 | `	ph7_hashmap *pMap;` |
|       - | 3135 | `	ph7_value *pArray;` |
|       - | 3136 | `	ph7_value sObj;` |
|       - | 3137 | `	ph7_value sVal;` |
|       - | 3138 | `	SyString sKey;` |
|       - | 3139 | `	int bStrict;` |
|       - | 3140 | `	sxi32 rc;` |
|       - | 3141 | `	sxu32 n;` |
|      95 | 3142 | `	if( nArg < 1 ){` |
|       - | 3143 | `		/* Missing arguments,return NULL */` |
|       3 | 3144 | `		ph7_result_null(pCtx);` |
|       3 | 3145 | `		return PH7_OK;` |
|       - | 3146 | `	}` |
|       - | 3147 | `	/* Make sure we are dealing with a valid hashmap */` |
|      93 | 3148 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|       - | 3149 | `		/* Invalid argument,return NULL */` |
|     ! 0 | 3150 | `		ph7_result_null(pCtx);` |
|     ! 0 | 3151 | `		return PH7_OK;` |
|       - | 3152 | `	}` |
|       - | 3153 | `	/* Point to the internal representation of the input hashmap */` |
|      93 | 3154 | `	pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|       - | 3155 | `	/* Create a new array */` |
|      93 | 3156 | `	pArray = ph7_context_new_array(pCtx);` |
|      93 | 3157 | `	if( pArray == 0 ){` |
|     ! 0 | 3158 | `		ph7_result_null(pCtx);` |
|     ! 0 | 3159 | `		return PH7_OK;` |
|       - | 3160 | `	}` |
|      93 | 3161 | `	bStrict = FALSE;` |
|      93 | 3162 | `	if( nArg > 2 && ph7_value_is_bool(apArg[2]) ){` |
|     ! 0 | 3163 | `		bStrict = ph7_value_to_bool(apArg[2]);` |
|     ! 0 | 3164 | `	}` |
|       - | 3165 | `	/* Perform the requested operation */` |
|      93 | 3166 | `	pNode = pMap->pFirst;` |
|      93 | 3167 | `	PH7_MemObjInit(pMap->pVm,&sVal);` |
|     467 | 3168 | `	for( n = 0 ; n < pMap->nEntry ; ++n ){` |
|     375 | 3169 | `		if( pNode->iType == HASHMAP_INT_NODE ){` |
|      71 | 3170 | `			PH7_MemObjInitFromInt(pMap->pVm,&sObj,pNode->xKey.iKey);` |
|      36 | 3171 | `		}else{` |
|     305 | 3172 | `			SyStringInitFromBuf(&sKey,SyBlobData(&pNode->xKey.sKey),SyBlobLength(&pNode->xKey.sKey));` |
|     305 | 3173 | `			PH7_MemObjInitFromString(pMap->pVm,&sObj,&sKey);` |
|       - | 3174 | `		}` |
|     375 | 3175 | `		rc = 0;` |
|     375 | 3176 | `		if( nArg > 1 ){` |
|     ! 0 | 3177 | `			ph7_value *pValue = HashmapExtractNodeValue(pNode);` |
|     ! 0 | 3178 | `			if( pValue ){` |
|     ! 0 | 3179 | `				PH7_MemObjLoad(pValue,&sVal);` |
|       - | 3180 | `				/* Filter key */` |
|     ! 0 | 3181 | `				rc = ph7_value_compare(&sVal,apArg[1],bStrict);` |
|     ! 0 | 3182 | `				PH7_MemObjRelease(pValue);` |
|     ! 0 | 3183 | `			}` |
|     ! 0 | 3184 | `		}` |
|     375 | 3185 | `		if( rc == 0 ){` |
|       - | 3186 | `			/* Perform the insertion */` |
|     375 | 3187 | `			ph7_array_add_elem(pArray,0,&sObj);` |
|     187 | 3188 | `		}` |
|     375 | 3189 | `		PH7_MemObjRelease(&sObj);` |
|       - | 3190 | `		/* Point to the next entry */` |
|     375 | 3191 | `		pNode = pNode->pPrev; /* Reverse link */` |
|     188 | 3192 | `	}` |
|       - | 3193 | `	/* return the new array */` |
|      93 | 3194 | `	ph7_result_value(pCtx,pArray);` |
|      93 | 3195 | `	return PH7_OK;` |
|      48 | 3196 |  |
|       - | 3197 | `/*` |
|       - | 3198 | ` * bool array_same(array $arr1,array $arr2)` |
|       - | 3199 | ` *  Return TRUE if the given arrays are the same instance.` |
|       - | 3200 | ` *  This function is useful under PH7 since arrays are passed` |
|       - | 3201 | ` *  by reference unlike the zend engine which use pass by values.` |
|       - | 3202 | ` * Parameters` |
|       - | 3203 | ` *  $arr1` |
|       - | 3204 | ` *   First array` |
|       - | 3205 | ` *  $arr2` |
|       - | 3206 | ` *   Second array` |
|       - | 3207 | ` * Return` |
|       - | 3208 | ` *  TRUE if the arrays are the same instance.FALSE otherwise.` |
|       - | 3209 | ` * Note` |
|       - | 3210 | ` *  This function is a symisc eXtension.` |
|       - | 3211 | ` */` |
|       4 | 3212 | `static int ph7_hashmap_same(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       1 | 3213 |  |
|       - | 3214 | `	ph7_hashmap *p1,*p2;` |
|       - | 3215 | `	int rc;` |
|       5 | 3216 | `	if( nArg < 2 \|\| !ph7_value_is_array(apArg[0]) \|\| !ph7_value_is_array(apArg[1]) ){` |
|       - | 3217 | `		/* Missing or invalid arguments,return FALSE*/` |
|     ! 0 | 3218 | `		ph7_result_bool(pCtx,0);` |
|     ! 0 | 3219 | `		return PH7_OK;` |
|       - | 3220 | `	}` |
|       - | 3221 | `	/* Point to the hashmaps */` |
|       5 | 3222 | `	p1 = (ph7_hashmap *)apArg[0]->x.pOther;` |
|       5 | 3223 | `	p2 = (ph7_hashmap *)apArg[1]->x.pOther;` |
|       5 | 3224 | `	rc = (p1 == p2);` |
|       - | 3225 | `	/* Same instance? */` |
|       5 | 3226 | `	ph7_result_bool(pCtx,rc);` |
|       5 | 3227 | `	return PH7_OK;` |
|       3 | 3228 |  |
|       - | 3229 | `/*` |
|       - | 3230 | ` * array array_merge(array $array1,...)` |
|       - | 3231 | ` *  Merge one or more arrays.` |
|       - | 3232 | ` * Parameters` |
|       - | 3233 | ` *  $array1` |
|       - | 3234 | ` *    Initial array to merge.` |
|       - | 3235 | ` *  ...` |
|       - | 3236 | ` *   More array to merge.` |
|       - | 3237 | ` * Return` |
|       - | 3238 | ` *  The resulting array.` |
|       - | 3239 | ` */` |
|     784 | 3240 | `static int ph7_hashmap_merge(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       2 | 3241 |  |
|       - | 3242 | `	ph7_hashmap *pMap,*pSrc;` |
|       - | 3243 | `	ph7_value *pArray;` |
|       - | 3244 | `	int i;` |
|     786 | 3245 | `	if( nArg < 1 ){` |
|       - | 3246 | `		/* Missing arguments,return NULL */` |
|     ! 0 | 3247 | `		ph7_result_null(pCtx);` |
|     ! 0 | 3248 | `		return PH7_OK;` |
|       - | 3249 | `	}` |
|       - | 3250 | `	/* Create a new array */` |
|     786 | 3251 | `	pArray = ph7_context_new_array(pCtx);` |
|     786 | 3252 | `	if( pArray == 0 ){` |
|     ! 0 | 3253 | `		ph7_result_null(pCtx);` |
|     ! 0 | 3254 | `		return PH7_OK;` |
|       - | 3255 | `	}` |
|       - | 3256 | `	/* Point to the internal representation of the hashmap */` |
|     786 | 3257 | `	pMap = (ph7_hashmap *)pArray->x.pOther;` |
|       - | 3258 | `	/* Start merging */` |
|    2354 | 3259 | `	for( i = 0 ; i < nArg ; i++ ){` |
|       - | 3260 | `		/* Make sure we are dealing with a valid hashmap */` |
|    1570 | 3261 | `		if( !ph7_value_is_array(apArg[i]) ){` |
|       - | 3262 | `			/* Insert scalar value */` |
|       5 | 3263 | `			ph7_array_add_elem(pArray,0,apArg[i]);` |
|       3 | 3264 | `		}else{` |
|    1566 | 3265 | `			pSrc = (ph7_hashmap *)apArg[i]->x.pOther;` |
|       - | 3266 | `			/* Merge the two hashmaps */` |
|    1566 | 3267 | `			HashmapMerge(pSrc,pMap);` |
|       - | 3268 | `		}` |
|     786 | 3269 | `	}` |
|       - | 3270 | `	/* Return the freshly created array */` |
|     786 | 3271 | `	ph7_result_value(pCtx,pArray);` |
|     786 | 3272 | `	return PH7_OK;` |
|     394 | 3273 |  |
|       - | 3274 | `/*` |
|       - | 3275 | ` * array array_copy(array $source)` |
|       - | 3276 | ` *  Make a blind copy of the target array.` |
|       - | 3277 | ` * Parameters` |
|       - | 3278 | ` *  $source` |
|       - | 3279 | ` *   Target array` |
|       - | 3280 | ` * Return` |
|       - | 3281 | ` *  Copy of the target array on success.NULL otherwise.` |
|       - | 3282 | ` * Note` |
|       - | 3283 | ` *  This function is a symisc eXtension.` |
|       - | 3284 | ` */` |
|       2 | 3285 | `static int ph7_hashmap_copy(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       1 | 3286 |  |
|       - | 3287 | `	ph7_hashmap *pMap;` |
|       - | 3288 | `	ph7_value *pArray;` |
|       3 | 3289 | `	if( nArg < 1 ){` |
|       - | 3290 | `		/* Missing arguments,return NULL */` |
|     ! 0 | 3291 | `		ph7_result_null(pCtx);` |
|     ! 0 | 3292 | `		return PH7_OK;` |
|       - | 3293 | `	}` |
|       - | 3294 | `	/* Create a new array */` |
|       3 | 3295 | `	pArray = ph7_context_new_array(pCtx);` |
|       3 | 3296 | `	if( pArray == 0 ){` |
|     ! 0 | 3297 | `		ph7_result_null(pCtx);` |
|     ! 0 | 3298 | `		return PH7_OK;` |
|       - | 3299 | `	}` |
|       - | 3300 | `	/* Point to the internal representation of the hashmap */` |
|       3 | 3301 | `	pMap = (ph7_hashmap *)pArray->x.pOther;` |
|       3 | 3302 | `	if( ph7_value_is_array(apArg[0])){` |
|       - | 3303 | `		/* Point to the internal representation of the source */` |
|       3 | 3304 | `		ph7_hashmap *pSrc = (ph7_hashmap *)apArg[0]->x.pOther;` |
|       - | 3305 | `		/* Perform the copy */` |
|       3 | 3306 | `		PH7_HashmapDup(pSrc,pMap);` |
|       2 | 3307 | `	}else{` |
|       - | 3308 | `		/* Simple insertion */` |
|     ! 0 | 3309 | `		PH7_HashmapInsert(pMap,0/* Automatic index assign*/,apArg[0]);` |
|       - | 3310 | `	}` |
|       - | 3311 | `	/* Return the duplicated array */` |
|       3 | 3312 | `	ph7_result_value(pCtx,pArray);` |
|       3 | 3313 | `	return PH7_OK;` |
|       2 | 3314 |  |
|       - | 3315 | `/*` |
|       - | 3316 | ` * bool array_erase(array $source)` |
|       - | 3317 | ` *  Remove all elements from a given array.` |
|       - | 3318 | ` * Parameters` |
|       - | 3319 | ` *  $source` |
|       - | 3320 | ` *   Target array` |
|       - | 3321 | ` * Return` |
|       - | 3322 | ` *  TRUE on success.FALSE otherwise.` |
|       - | 3323 | ` * Note` |
|       - | 3324 | ` *  This function is a symisc eXtension.` |
|       - | 3325 | ` */` |
|       2 | 3326 | `static int ph7_hashmap_erase(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       1 | 3327 |  |
|       - | 3328 | `	ph7_hashmap *pMap;` |
|       3 | 3329 | `	if( nArg < 1 ){` |
|       - | 3330 | `		/* Missing arguments */` |
|     ! 0 | 3331 | `		ph7_result_bool(pCtx,0);` |
|     ! 0 | 3332 | `		return PH7_OK;` |
|       - | 3333 | `	}` |
|       - | 3334 | `	/* Point to the target hashmap */` |
|       3 | 3335 | `	pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|       - | 3336 | `	/* Erase */` |
|       3 | 3337 | `	PH7_HashmapRelease(pMap,FALSE);` |
|       3 | 3338 | `	return PH7_OK;` |
|       2 | 3339 |  |
|       - | 3340 | `/*` |
|       - | 3341 | ` * array array_slice(array $array,int $offset [,int $length [, bool $preserve_keys = false ]])` |
|       - | 3342 | ` *  Extract a slice of the array.` |
|       - | 3343 | ` * Parameters` |
|       - | 3344 | ` *  $array` |
|       - | 3345 | ` *    The input array.` |
|       - | 3346 | ` * $offset` |
|       - | 3347 | ` *    If offset is non-negative, the sequence will start at that offset in the array.` |
|       - | 3348 | ` *    If offset is negative, the sequence will start that far from the end of the array.` |
|       - | 3349 | ` * $length (optional)` |
|       - | 3350 | ` *    If length is given and is positive, then the sequence will have that many elements` |
|       - | 3351 | ` *    in it. If length is given and is negative then the sequence will stop that many` |
|       - | 3352 | ` *   elements from the end of the array. If it is omitted, then the sequence will have` |
|       - | 3353 | ` *   everything from offset up until the end of the array.` |
|       - | 3354 | ` * $preserve_keys (optional)` |
|       - | 3355 | ` *    Note that array_slice() will reorder and reset the array indices by default.` |
|       - | 3356 | ` *    You can change this behaviour by setting preserve_keys to TRUE.` |
|       - | 3357 | ` * Return` |
|       - | 3358 | ` *   The new slice.` |
|       - | 3359 | ` */` |
|       8 | 3360 | `static int ph7_hashmap_slice(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       1 | 3361 |  |
|       - | 3362 | `	ph7_hashmap *pMap,*pSrc;` |
|       - | 3363 | `	ph7_hashmap_node *pCur;` |
|       - | 3364 | `	ph7_value *pArray;` |
|       - | 3365 | `	int iLength,iOfft;` |
|       - | 3366 | `	int bPreserve;` |
|       - | 3367 | `	sxi32 rc;` |
|       9 | 3368 | `	if( nArg < 2 \|\| !ph7_value_is_array(apArg[0]) ){` |
|       - | 3369 | `		/* Missing/Invalid arguments,return NULL */` |
|     ! 0 | 3370 | `		ph7_result_null(pCtx);` |
|     ! 0 | 3371 | `		return PH7_OK;` |
|       - | 3372 | `	}` |
|       - | 3373 | `	/* Point the internal representation of the target array */` |
|       9 | 3374 | `	pSrc = (ph7_hashmap *)apArg[0]->x.pOther;` |
|       9 | 3375 | `	bPreserve = FALSE;` |
|       - | 3376 | `	/* Get the offset */` |
|       9 | 3377 | `	iOfft = ph7_value_to_int(apArg[1]);` |
|       9 | 3378 | `	if( iOfft < 0 ){` |
|       3 | 3379 | `		iOfft = (int)pSrc->nEntry + iOfft;` |
|       1 | 3380 | `	}` |
|       9 | 3381 | `	if( iOfft < 0 \|\| iOfft > (int)pSrc->nEntry ){` |
|       - | 3382 | `		/* Invalid offset,return the last entry */` |
|     ! 0 | 3383 | `		iOfft = (int)pSrc->nEntry - 1;` |
|     ! 0 | 3384 | `	}` |
|       - | 3385 | `	/* Get the length */` |
|       9 | 3386 | `	iLength = (int)pSrc->nEntry - iOfft;` |
|       9 | 3387 | `	if( nArg > 2 ){` |
|       7 | 3388 | `		iLength = ph7_value_to_int(apArg[2]);` |
|       7 | 3389 | `		if( iLength < 0 ){` |
|     ! 0 | 3390 | `			iLength = ((int)pSrc->nEntry + iLength) - iOfft;` |
|     ! 0 | 3391 | `		}` |
|       7 | 3392 | `		if( iLength < 0 \|\| iOfft + iLength >= (int)pSrc->nEntry ){` |
|     ! 0 | 3393 | `			iLength = (int)pSrc->nEntry - iOfft;` |
|     ! 0 | 3394 | `		}` |
|       7 | 3395 | `		if( nArg > 3 && ph7_value_is_bool(apArg[3]) ){` |
|       3 | 3396 | `			bPreserve = ph7_value_to_bool(apArg[3]);` |
|       1 | 3397 | `		}` |
|       3 | 3398 | `	}` |
|       - | 3399 | `	/* Create a new array */` |
|       9 | 3400 | `	pArray = ph7_context_new_array(pCtx);` |
|       9 | 3401 | `	if( pArray == 0 ){` |
|     ! 0 | 3402 | `		ph7_result_null(pCtx);` |
|     ! 0 | 3403 | `		return PH7_OK;` |
|       - | 3404 | `	}` |
|       9 | 3405 | `	if( iLength < 1 ){` |
|       - | 3406 | `		/* Don't bother processing,return the empty array */` |
|     ! 0 | 3407 | `		ph7_result_value(pCtx,pArray);` |
|     ! 0 | 3408 | `		return PH7_OK;` |
|       - | 3409 | `	}` |
|       - | 3410 | `	/* Point to the desired entry */` |
|       9 | 3411 | `	pCur = pSrc->pFirst;` |
|       9 | 3412 | `	for(;;){` |
|      19 | 3413 | `		if( iOfft < 1 ){` |
|       9 | 3414 | `			break;` |
|       - | 3415 | `		}` |
|       - | 3416 | `		/* Point to the next entry */` |
|      11 | 3417 | `		pCur = pCur->pPrev; /* Reverse link */` |
|      11 | 3418 | `		iOfft--;` |
|       1 | 3419 | `	}` |
|       - | 3420 | `	/* Point to the internal representation of the hashmap */` |
|       9 | 3421 | `	pMap = (ph7_hashmap *)pArray->x.pOther;` |
|      12 | 3422 | `	for(;;){` |
|      25 | 3423 | `		if( iLength < 1 ){` |
|       9 | 3424 | `			break;` |
|       - | 3425 | `		}` |
|      17 | 3426 | `		rc = HashmapInsertNode(pMap,pCur,bPreserve);` |
|      17 | 3427 | `		if( rc != SXRET_OK ){` |
|     ! 0 | 3428 | `			break;` |
|       - | 3429 | `		}` |
|       - | 3430 | `		/* Point to the next entry */` |
|      17 | 3431 | `		pCur = pCur->pPrev; /* Reverse link */` |
|      17 | 3432 | `		iLength--;` |
|       1 | 3433 | `	}` |
|       - | 3434 | `	/* Return the freshly created array */` |
|       9 | 3435 | `	ph7_result_value(pCtx,pArray);` |
|       9 | 3436 | `	return PH7_OK;` |
|       5 | 3437 |  |
|       - | 3438 | `/*` |
|       - | 3439 | ` * array array_splice(array $array,int $offset [,int $length [,value $replacement ]])` |
|       - | 3440 | ` *  Remove a portion of the array and replace it with something else.` |
|       - | 3441 | ` * Parameters` |
|       - | 3442 | ` *  $array` |
|       - | 3443 | ` *    The input array.` |
|       - | 3444 | ` * $offset` |
|       - | 3445 | ` *    If offset is positive then the start of removed portion is at that offset from` |
|       - | 3446 | ` *    the beginning of the input array. If offset is negative then it starts that far` |
|       - | 3447 | ` *    from the end of the input array.` |
|       - | 3448 | ` * $length (optional)` |
|       - | 3449 | ` *    If length is omitted, removes everything from offset to the end of the array.` |
|       - | 3450 | ` *    If length is specified and is positive, then that many elements will be removed.` |
|       - | 3451 | ` *    If length is specified and is negative then the end of the removed portion will` |
|       - | 3452 | ` *    be that many elements from the end of the array.` |
|       - | 3453 | ` * $replacement (optional)` |
|       - | 3454 | ` *  If replacement array is specified, then the removed elements are replaced` |
|       - | 3455 | ` *  with elements from this array.` |
|       - | 3456 | ` *  If offset and length are such that nothing is removed, then the elements` |
|       - | 3457 | ` *  from the replacement array are inserted in the place specified by the offset.` |
|       - | 3458 | ` *  Note that keys in replacement array are not preserved.` |
|       - | 3459 | ` *  If replacement is just one element it is not necessary to put array() around` |
|       - | 3460 | ` *  it, unless the element is an array itself, an object or NULL.` |
|       - | 3461 | ` * Return` |
|       - | 3462 | ` *   A new array consisting of the extracted elements.` |
|       - | 3463 | ` */` |
|       2 | 3464 | `static int ph7_hashmap_splice(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       1 | 3465 |  |
|       - | 3466 | `	ph7_hashmap_node *pCur,*pPrev,*pRnode;` |
|       - | 3467 | `	ph7_value *pArray,*pRvalue,*pOld;` |
|       - | 3468 | `	ph7_hashmap *pMap,*pSrc,*pRep;` |
|       - | 3469 | `	int iLength,iOfft;` |
|       - | 3470 | `	sxi32 rc;` |
|       3 | 3471 | `	if( nArg < 2 \|\| !ph7_value_is_array(apArg[0]) ){` |
|       - | 3472 | `		/* Missing/Invalid arguments,return NULL */` |
|     ! 0 | 3473 | `		ph7_result_null(pCtx);` |
|     ! 0 | 3474 | `		return PH7_OK;` |
|       - | 3475 | `	}` |
|       - | 3476 | `	/* Point the internal representation of the target array */` |
|       3 | 3477 | `	pSrc = (ph7_hashmap *)apArg[0]->x.pOther;` |
|       - | 3478 | `	/* Get the offset */` |
|       3 | 3479 | `	iOfft = ph7_value_to_int(apArg[1]);` |
|       3 | 3480 | `	if( iOfft < 0 ){` |
|     ! 0 | 3481 | `		iOfft = (int)pSrc->nEntry + iOfft;` |
|     ! 0 | 3482 | `	}` |
|       3 | 3483 | `	if( iOfft < 0 \|\| iOfft > (int)pSrc->nEntry ){` |
|       - | 3484 | `		/* Invalid offset,remove the last entry */` |
|     ! 0 | 3485 | `		iOfft = (int)pSrc->nEntry - 1;` |
|     ! 0 | 3486 | `	}` |
|       - | 3487 | `	/* Get the length */` |
|       3 | 3488 | `	iLength = (int)pSrc->nEntry - iOfft;` |
|       3 | 3489 | `	if( nArg > 2 ){` |
|       3 | 3490 | `		iLength = ph7_value_to_int(apArg[2]);` |
|       3 | 3491 | `		if( iLength < 0 ){` |
|     ! 0 | 3492 | `			iLength = ((int)pSrc->nEntry + iLength) - iOfft;` |
|     ! 0 | 3493 | `		}` |
|       3 | 3494 | `		if( iLength < 0 \|\| iOfft + iLength >= (int)pSrc->nEntry ){` |
|     ! 0 | 3495 | `			iLength = (int)pSrc->nEntry - iOfft;` |
|     ! 0 | 3496 | `		}` |
|       1 | 3497 | `	}` |
|       - | 3498 | `	/* Create a new array */` |
|       3 | 3499 | `	pArray = ph7_context_new_array(pCtx);` |
|       3 | 3500 | `	if( pArray == 0 ){` |
|     ! 0 | 3501 | `		ph7_result_null(pCtx);` |
|     ! 0 | 3502 | `		return PH7_OK;` |
|       - | 3503 | `	}` |
|       3 | 3504 | `	if( iLength < 1 ){` |
|       - | 3505 | `		/* Don't bother processing,return the empty array */` |
|     ! 0 | 3506 | `		ph7_result_value(pCtx,pArray);` |
|     ! 0 | 3507 | `		return PH7_OK;` |
|       - | 3508 | `	}` |
|       - | 3509 | `	/* Point to the desired entry */` |
|       3 | 3510 | `	pCur = pSrc->pFirst;` |
|       2 | 3511 | `	for(;;){` |
|       5 | 3512 | `		if( iOfft < 1 ){` |
|       3 | 3513 | `			break;` |
|       - | 3514 | `		}` |
|       - | 3515 | `		/* Point to the next entry */` |
|       3 | 3516 | `		pCur = pCur->pPrev; /* Reverse link */` |
|       3 | 3517 | `		iOfft--;` |
|       1 | 3518 | `	}` |
|       3 | 3519 | `	pRep = 0;` |
|       3 | 3520 | `	if( nArg > 3 ){` |
|       3 | 3521 | `		if( !ph7_value_is_array(apArg[3]) ){` |
|       - | 3522 | `			/* Perform an array cast */` |
|     ! 0 | 3523 | `			PH7_MemObjToHashmap(apArg[3]);` |
|     ! 0 | 3524 | `			if(ph7_value_is_array(apArg[3])){` |
|     ! 0 | 3525 | `				pRep = (ph7_hashmap *)apArg[3]->x.pOther;` |
|     ! 0 | 3526 | `			}` |
|     ! 0 | 3527 | `		}else{` |
|       3 | 3528 | `			pRep = (ph7_hashmap *)apArg[3]->x.pOther;` |
|       - | 3529 | `		}` |
|       3 | 3530 | `		if( pRep ){` |
|       - | 3531 | `			/* Reset the loop cursor */` |
|       3 | 3532 | `			pRep->pCur = pRep->pFirst;` |
|       1 | 3533 | `		}` |
|       1 | 3534 | `	}` |
|       - | 3535 | `	/* Point to the internal representation of the hashmap */` |
|       3 | 3536 | `	pMap = (ph7_hashmap *)pArray->x.pOther;` |
|       3 | 3537 | `	for(;;){` |
|       7 | 3538 | `		if( iLength < 1 ){` |
|       3 | 3539 | `			break;` |
|       - | 3540 | `		}` |
|       5 | 3541 | `		pPrev = pCur->pPrev;` |
|       5 | 3542 | `		rc = HashmapInsertNode(pMap,pCur,FALSE);` |
|       5 | 3543 | `		if( pRep && (pRnode = PH7_HashmapGetNextEntry(pRep)) != 0 ){` |
|       - | 3544 | `			/* Extract node value */` |
|       5 | 3545 | `			pRvalue = HashmapExtractNodeValue(pRnode);` |
|       - | 3546 | `			/* Replace the old node */` |
|       5 | 3547 | `			pOld = HashmapExtractNodeValue(pCur);` |
|       5 | 3548 | `			if( pRvalue && pOld ){` |
|       5 | 3549 | `				PH7_MemObjStore(pRvalue,pOld);` |
|       2 | 3550 | `			}` |
|       3 | 3551 | `		}else{` |
|       - | 3552 | `			/* Unlink the node from the source hashmap */` |
|     ! 0 | 3553 | `			PH7_HashmapUnlinkNode(pCur,TRUE);` |
|       - | 3554 | `		}` |
|       5 | 3555 | `		if( rc != SXRET_OK ){` |
|     ! 0 | 3556 | `			break;` |
|       - | 3557 | `		}` |
|       - | 3558 | `		/* Point to the next entry */` |
|       5 | 3559 | `		pCur = pPrev; /* Reverse link */` |
|       5 | 3560 | `		iLength--;` |
|       1 | 3561 | `	}` |
|       3 | 3562 | `	if( pRep ){` |
|       3 | 3563 | `		while((pRnode = PH7_HashmapGetNextEntry(pRep)) != 0 ){` |
|     ! 0 | 3564 | `			HashmapInsertNode(pSrc,pRnode,FALSE);` |
|     ! 0 | 3565 | `		}` |
|       1 | 3566 | `	}` |
|       - | 3567 | `	/* Return the freshly created array */` |
|       3 | 3568 | `	ph7_result_value(pCtx,pArray);` |
|       3 | 3569 | `	return PH7_OK;` |
|       2 | 3570 |  |
|       - | 3571 | `/*` |
|       - | 3572 | ` * bool in_array(value $needle,array $haystack[,bool $strict = FALSE ])` |
|       - | 3573 | ` *  Checks if a value exists in an array.` |
|       - | 3574 | ` * Parameters` |
|       - | 3575 | ` *  $needle` |
|       - | 3576 | ` *   The searched value.` |
|       - | 3577 | ` *   Note:` |
|       - | 3578 | ` *    If needle is a string, the comparison is done in a case-sensitive manner.` |
|       - | 3579 | ` * $haystack` |
|       - | 3580 | ` *  The target array.` |
|       - | 3581 | ` * $strict` |
|       - | 3582 | ` *  If the third parameter strict is set to TRUE then the in_array() function` |
|       - | 3583 | ` *  will also check the types of the needle in the haystack.` |
|       - | 3584 | ` */` |
|   18050 | 3585 | `static int ph7_hashmap_in_array(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       2 | 3586 |  |
|       - | 3587 | `	ph7_value *pNeedle;` |
|       - | 3588 | `	int bStrict;` |
|       - | 3589 | `	int rc;` |
|   18052 | 3590 | `	if( nArg < 2 ){` |
|       - | 3591 | `		/* Missing argument,return FALSE */` |
|     ! 0 | 3592 | `		ph7_result_bool(pCtx,0);` |
|     ! 0 | 3593 | `		return PH7_OK;` |
|       - | 3594 | `	}` |
|   18052 | 3595 | `	pNeedle = apArg[0];` |
|   18052 | 3596 | `	bStrict = 0;` |
|   18052 | 3597 | `	if( nArg > 2 ){` |
|       5 | 3598 | `		bStrict = ph7_value_to_bool(apArg[2]);` |
|       2 | 3599 | `	}` |
|   18052 | 3600 | `	if( !ph7_value_is_array(apArg[1]) ){` |
|       - | 3601 | `		/* haystack must be an array,perform a standard comparison */` |
|     ! 0 | 3602 | `		rc = ph7_value_compare(pNeedle,apArg[1],bStrict);` |
|       - | 3603 | `		/* Set the comparison result */` |
|     ! 0 | 3604 | `		ph7_result_bool(pCtx,rc == 0);` |
|     ! 0 | 3605 | `		return PH7_OK;` |
|       - | 3606 | `	}` |
|       - | 3607 | `	/* Perform the lookup */` |
|   18052 | 3608 | `	rc = HashmapFindValue((ph7_hashmap *)apArg[1]->x.pOther,pNeedle,0,bStrict);` |
|       - | 3609 | `	/* Lookup result */` |
|   18052 | 3610 | `	ph7_result_bool(pCtx,rc == SXRET_OK);` |
|   18052 | 3611 | `	return PH7_OK;` |
|    9027 | 3612 |  |
|       - | 3613 | `/*` |
|       - | 3614 | ` * value array_search(value $needle,array $haystack[,bool $strict = false ])` |
|       - | 3615 | ` *  Searches the array for a given value and returns the corresponding key if successful.` |
|       - | 3616 | ` * Parameters` |
|       - | 3617 | ` * $needle` |
|       - | 3618 | ` *   The searched value.` |
|       - | 3619 | ` * $haystack` |
|       - | 3620 | ` *   The array.` |
|       - | 3621 | ` * $strict` |
|       - | 3622 | ` *  If the third parameter strict is set to TRUE then the array_search() function` |
|       - | 3623 | ` *  will search for identical elements in the haystack. This means it will also check` |
|       - | 3624 | ` *  the types of the needle in the haystack, and objects must be the same instance.` |
|       - | 3625 | ` * Return` |
|       - | 3626 | ` *  Returns the key for needle if it is found in the array, FALSE otherwise.` |
|       - | 3627 | ` */` |
|      26 | 3628 | `static int ph7_hashmap_search(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       1 | 3629 |  |
|       - | 3630 | `	ph7_hashmap_node *pEntry;` |
|       - | 3631 | `	ph7_value *pVal,sNeedle;` |
|       - | 3632 | `	ph7_hashmap *pMap;` |
|       - | 3633 | `	ph7_value sVal;` |
|       - | 3634 | `	int bStrict;` |
|       - | 3635 | `	sxu32 n;` |
|       - | 3636 | `	int rc;` |
|      27 | 3637 | `	if( nArg < 2 ){` |
|       - | 3638 | `		/* Missing argument,return FALSE*/` |
|     ! 0 | 3639 | `		ph7_result_bool(pCtx,0);` |
|     ! 0 | 3640 | `		return PH7_OK;` |
|       - | 3641 | `	}` |
|      27 | 3642 | `	bStrict = FALSE;` |
|      27 | 3643 | `	if( !ph7_value_is_array(apArg[1]) ){` |
|       - | 3644 | `		/* hasystack must be an array,return FALSE */` |
|       3 | 3645 | `		ph7_result_bool(pCtx,0);` |
|       3 | 3646 | `		return PH7_OK;` |
|       - | 3647 | `	}` |
|      25 | 3648 | `	if( nArg > 2 && ph7_value_is_bool(apArg[2]) ){` |
|      19 | 3649 | `		bStrict = ph7_value_to_bool(apArg[2]);` |
|       9 | 3650 | `	}` |
|       - | 3651 | `	/* Point to the internal representation of the internal hashmap */` |
|      25 | 3652 | `	pMap = (ph7_hashmap *)apArg[1]->x.pOther;` |
|       - | 3653 | `	/* Perform a linear search since we cannot sort the hashmap based on values */` |
|      25 | 3654 | `	PH7_MemObjInit(pMap->pVm,&sVal);` |
|      25 | 3655 | `	PH7_MemObjInit(pMap->pVm,&sNeedle);` |
|      25 | 3656 | `	pEntry = pMap->pFirst;` |
|      25 | 3657 | `	n = pMap->nEntry;` |
|      39 | 3658 | `	for(;;){` |
|      79 | 3659 | `		if( !n ){` |
|       7 | 3660 | `			break;` |
|       - | 3661 | `		}` |
|       - | 3662 | `		/* Extract node value */` |
|      73 | 3663 | `		pVal = HashmapExtractNodeValue(pEntry);` |
|      73 | 3664 | `		if( pVal ){` |
|       - | 3665 | `			/* Make a copy of the vuurent values since the comparison routine` |
|       - | 3666 | `			 * can change their type.` |
|       - | 3667 | `			 */` |
|      73 | 3668 | `			PH7_MemObjLoad(pVal,&sVal);` |
|      73 | 3669 | `			PH7_MemObjLoad(apArg[0],&sNeedle);` |
|      73 | 3670 | `			rc = PH7_MemObjCmp(&sNeedle,&sVal,bStrict,0);` |
|      73 | 3671 | `			PH7_MemObjRelease(&sVal);` |
|      73 | 3672 | `			PH7_MemObjRelease(&sNeedle);` |
|      73 | 3673 | `			if( rc == 0 ){` |
|       - | 3674 | `				/* Match found,return key */` |
|      19 | 3675 | `				if( pEntry->iType == HASHMAP_INT_NODE){` |
|       - | 3676 | `					/* INT key */` |
|      13 | 3677 | `					ph7_result_int64(pCtx,pEntry->xKey.iKey);` |
|       7 | 3678 | `				}else{` |
|       7 | 3679 | `					SyBlob *pKey = &pEntry->xKey.sKey;` |
|       - | 3680 | `					/* Blob key */` |
|       7 | 3681 | `					ph7_result_string(pCtx,(const char *)SyBlobData(pKey),(int)SyBlobLength(pKey));` |
|       - | 3682 | `				}` |
|      19 | 3683 | `				return PH7_OK;` |
|       - | 3684 | `			}` |
|      27 | 3685 | `		}` |
|       - | 3686 | `		/* Point to the next entry */` |
|      55 | 3687 | `		pEntry = pEntry->pPrev; /* Reverse link */` |
|      55 | 3688 | `		n--;` |
|       1 | 3689 | `	}` |
|       - | 3690 | `	/* No such value,return FALSE */` |
|       7 | 3691 | `	ph7_result_bool(pCtx,0);` |
|       7 | 3692 | `	return PH7_OK;` |
|      14 | 3693 |  |
|       - | 3694 | `/*` |
|       - | 3695 | ` * array array_diff(array $array1,array $array2,...)` |
|       - | 3696 | ` *  Computes the difference of arrays.` |
|       - | 3697 | ` * Parameters` |
|       - | 3698 | ` *  $array1` |
|       - | 3699 | ` *    The array to compare from` |
|       - | 3700 | ` *  $array2` |
|       - | 3701 | ` *    An array to compare against` |
|       - | 3702 | ` *  $...` |
|       - | 3703 | ` *   More arrays to compare against` |
|       - | 3704 | ` * Return` |
|       - | 3705 | ` *  Returns an array containing all the entries from array1 that` |
|       - | 3706 | ` *  are not present in any of the other arrays.` |
|       - | 3707 | ` */` |
|      10 | 3708 | `static int ph7_hashmap_diff(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       2 | 3709 |  |
|       - | 3710 | `	ph7_hashmap_node *pEntry;` |
|       - | 3711 | `	ph7_hashmap *pSrc,*pMap;` |
|       - | 3712 | `	ph7_value *pArray;` |
|       - | 3713 | `	ph7_value *pVal;` |
|       - | 3714 | `	sxi32 rc;` |
|       - | 3715 | `	sxu32 n;` |
|       - | 3716 | `	int i;` |
|       - | 3717 | `	/* Validate arguments to mimic PHP behaviour. Earlier versions simply` |
|       - | 3718 | `	 * returned NULL when the caller passed invalid parameters which made` |
|       - | 3719 | `	 * debugging difficult. */` |
|      12 | 3720 | `	if( nArg < 1 ){` |
|       4 | 3721 | `		return PH7_VmThrowException(pCtx,` |
|       - | 3722 | `			"ArgumentCountError",` |
|       - | 3723 | `			"array_diff() expects at least 1 argument, %d given",` |
|       1 | 3724 | `			nArg` |
|       - | 3725 | `			);` |
|       - | 3726 | `	}` |
|      10 | 3727 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|       4 | 3728 | `		return PH7_VmThrowException(pCtx,` |
|       - | 3729 | `			"TypeError",` |
|       - | 3730 | `			"array_diff(): Argument #1 ($array) must be of type array, %s given",` |
|       1 | 3731 | `			ph7_type_name(apArg[0])` |
|       - | 3732 | `			);` |
|       - | 3733 | `	}` |
|      14 | 3734 | `	for(i = 1 ; i < nArg ; i++){` |
|      10 | 3735 | `		if( !ph7_value_is_array(apArg[i]) ){` |
|       4 | 3736 | `			return PH7_VmThrowException(pCtx,` |
|       - | 3737 | `				"TypeError",` |
|       - | 3738 | `				"array_diff(): Argument #%d must be of type array, %s given",` |
|       1 | 3739 | `				i + 1,` |
|       2 | 3740 | `				ph7_type_name(apArg[i])` |
|       - | 3741 | `				);` |
|       - | 3742 | `		}` |
|       4 | 3743 | `	}` |
|       5 | 3744 | `	if( nArg == 1 ){` |
|       - | 3745 | `		/* Return the first array since we cannot perform a diff */` |
|     ! 0 | 3746 | `		ph7_result_value(pCtx,apArg[0]);` |
|     ! 0 | 3747 | `		return PH7_OK;` |
|       - | 3748 | `	}` |
|       - | 3749 | `	/* Create a new array */` |
|       5 | 3750 | `	pArray = ph7_context_new_array(pCtx);` |
|       5 | 3751 | `	if( pArray == 0 ){` |
|     ! 0 | 3752 | `		ph7_result_null(pCtx);` |
|     ! 0 | 3753 | `		return PH7_OK;` |
|       - | 3754 | `	}` |
|       - | 3755 | `	/* Point to the internal representation of the source hashmap */` |
|       5 | 3756 | `	pSrc = (ph7_hashmap *)apArg[0]->x.pOther;` |
|       - | 3757 | `	/* Perform the diff */` |
|       5 | 3758 | `	pEntry = pSrc->pFirst;` |
|       5 | 3759 | `	n = pSrc->nEntry;` |
|       8 | 3760 | `	for(;;){` |
|      17 | 3761 | `		if( n < 1 ){` |
|       5 | 3762 | `			break;` |
|       - | 3763 | `		}` |
|       - | 3764 | `		/* Extract the node value */` |
|      13 | 3765 | `		pVal = HashmapExtractNodeValue(pEntry);` |
|      13 | 3766 | `		if( pVal ){` |
|      23 | 3767 | `			for( i = 1 ; i < nArg ; i++ ){` |
|      17 | 3768 | `				if( !ph7_value_is_array(apArg[i])) {` |
|       - | 3769 | `					/* ignore */` |
|     ! 0 | 3770 | `					continue;` |
|       - | 3771 | `				}` |
|       - | 3772 | `				/* Point to the internal representation of the hashmap */` |
|      17 | 3773 | `				pMap = (ph7_hashmap *)apArg[i]->x.pOther;` |
|       - | 3774 | `				/* Perform the lookup */` |
|      17 | 3775 | `				rc = HashmapFindValue(pMap,pVal,0,TRUE);` |
|      17 | 3776 | `				if( rc == SXRET_OK ){` |
|       - | 3777 | `					/* Value exist */` |
|       7 | 3778 | `					break;` |
|       - | 3779 | `				}` |
|       6 | 3780 | `			}` |
|      13 | 3781 | `			if( i >= nArg ){` |
|       - | 3782 | `				/* Perform the insertion */` |
|       7 | 3783 | `				HashmapInsertNode((ph7_hashmap *)pArray->x.pOther,pEntry,TRUE);` |
|       3 | 3784 | `			}` |
|       6 | 3785 | `		}` |
|       - | 3786 | `		/* Point to the next entry */` |
|      13 | 3787 | `		pEntry = pEntry->pPrev; /* Reverse link */` |
|      13 | 3788 | `		n--;` |
|       1 | 3789 | `	}` |
|       - | 3790 | `	/* Return the freshly created array */` |
|       5 | 3791 | `	ph7_result_value(pCtx,pArray);` |
|       5 | 3792 | `	return PH7_OK;` |
|       7 | 3793 |  |
|       - | 3794 | `/*` |
|       - | 3795 | ` * array array_udiff(array $array1,array $array2,...,$callback)` |
|       - | 3796 | ` *  Computes the difference of arrays by using a callback function for data comparison.` |
|       - | 3797 | ` * Parameters` |
|       - | 3798 | ` *  $array1` |
|       - | 3799 | ` *    The array to compare from` |
|       - | 3800 | ` *  $array2` |
|       - | 3801 | ` *    An array to compare against` |
|       - | 3802 | ` *  $...` |
|       - | 3803 | ` *   More arrays to compare against.` |
|       - | 3804 | ` * $callback` |
|       - | 3805 | ` *  The callback comparison function.` |
|       - | 3806 | ` *  The comparison function must return an integer less than, equal to, or greater than zero` |
|       - | 3807 | ` *  if the first argument is considered to be respectively less than, equal to, or greater` |
|       - | 3808 | ` *  than the second.` |
|       - | 3809 | ` *     int callback ( mixed $a, mixed $b )` |
|       - | 3810 | ` * Return` |
|       - | 3811 | ` *  Returns an array containing all the entries from array1 that` |
|       - | 3812 | ` *  are not present in any of the other arrays.` |
|       - | 3813 | ` */` |
|       2 | 3814 | `static int ph7_hashmap_udiff(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       1 | 3815 |  |
|       - | 3816 | `	ph7_hashmap_node *pEntry;` |
|       - | 3817 | `	ph7_hashmap *pSrc,*pMap;` |
|       - | 3818 | `	ph7_value *pCallback;` |
|       - | 3819 | `	ph7_value *pArray;` |
|       - | 3820 | `	ph7_value *pVal;` |
|       - | 3821 | `	sxi32 rc;` |
|       - | 3822 | `	sxu32 n;` |
|       - | 3823 | `	int i;` |
|       3 | 3824 | `	if( nArg < 2 \|\| !ph7_value_is_array(apArg[0]) ){` |
|       - | 3825 | `		/* Missing/Invalid arguments,return NULL */` |
|     ! 0 | 3826 | `		ph7_result_null(pCtx);` |
|     ! 0 | 3827 | `		return PH7_OK;` |
|       - | 3828 | `	}` |
|       - | 3829 | `	/* Point to the callback */` |
|       3 | 3830 | `	pCallback = apArg[nArg - 1];` |
|       3 | 3831 | `	if( nArg == 2 ){` |
|       - | 3832 | `		/* Return the first array since we cannot perform a diff */` |
|     ! 0 | 3833 | `		ph7_result_value(pCtx,apArg[0]);` |
|     ! 0 | 3834 | `		return PH7_OK;` |
|       - | 3835 | `	}` |
|       - | 3836 | `	/* Create a new array */` |
|       3 | 3837 | `	pArray = ph7_context_new_array(pCtx);` |
|       3 | 3838 | `	if( pArray == 0 ){` |
|     ! 0 | 3839 | `		ph7_result_null(pCtx);` |
|     ! 0 | 3840 | `		return PH7_OK;` |
|       - | 3841 | `	}` |
|       - | 3842 | `	/* Point to the internal representation of the source hashmap */` |
|       3 | 3843 | `	pSrc = (ph7_hashmap *)apArg[0]->x.pOther;` |
|       - | 3844 | `	/* Perform the diff */` |
|       3 | 3845 | `	pEntry = pSrc->pFirst;` |
|       3 | 3846 | `	n = pSrc->nEntry;` |
|       4 | 3847 | `	for(;;){` |
|       9 | 3848 | `		if( n < 1 ){` |
|       3 | 3849 | `			break;` |
|       - | 3850 | `		}` |
|       - | 3851 | `		/* Extract the node value */` |
|       7 | 3852 | `		pVal = HashmapExtractNodeValue(pEntry);` |
|       7 | 3853 | `		if( pVal ){` |
|      11 | 3854 | `			for( i = 1 ; i < nArg - 1; i++ ){` |
|       7 | 3855 | `				if( !ph7_value_is_array(apArg[i])) {` |
|       - | 3856 | `					/* ignore */` |
|     ! 0 | 3857 | `					continue;` |
|       - | 3858 | `				}` |
|       - | 3859 | `				/* Point to the internal representation of the hashmap */` |
|       7 | 3860 | `				pMap = (ph7_hashmap *)apArg[i]->x.pOther;` |
|       - | 3861 | `				/* Perform the lookup */` |
|       7 | 3862 | `				rc = HashmapFindValueByCallback(pMap,pVal,pCallback,0);` |
|       7 | 3863 | `				if( rc == SXRET_OK ){` |
|       - | 3864 | `					/* Value exist */` |
|       3 | 3865 | `					break;` |
|       - | 3866 | `				}` |
|       3 | 3867 | `			}` |
|       7 | 3868 | `			if( i >= (nArg - 1)){` |
|       - | 3869 | `				/* Perform the insertion */` |
|       5 | 3870 | `				HashmapInsertNode((ph7_hashmap *)pArray->x.pOther,pEntry,TRUE);` |
|       2 | 3871 | `			}` |
|       3 | 3872 | `		}` |
|       - | 3873 | `		/* Point to the next entry */` |
|       7 | 3874 | `		pEntry = pEntry->pPrev; /* Reverse link */` |
|       7 | 3875 | `		n--;` |
|       1 | 3876 | `	}` |
|       - | 3877 | `	/* Return the freshly created array */` |
|       3 | 3878 | `	ph7_result_value(pCtx,pArray);` |
|       3 | 3879 | `	return PH7_OK;` |
|       2 | 3880 |  |
|       - | 3881 | `/*` |
|       - | 3882 | ` * array array_diff_assoc(array $array1,array $array2,...)` |
|       - | 3883 | ` *  Computes the difference of arrays with additional index check.` |
|       - | 3884 | ` * Parameters` |
|       - | 3885 | ` *  $array1` |
|       - | 3886 | ` *    The array to compare from` |
|       - | 3887 | ` *  $array2` |
|       - | 3888 | ` *    An array to compare against` |
|       - | 3889 | ` *  $...` |
|       - | 3890 | ` *   More arrays to compare against` |
|       - | 3891 | ` * Return` |
|       - | 3892 | ` *  Returns an array containing all the entries from array1 that` |
|       - | 3893 | ` *  are not present in any of the other arrays.` |
|       - | 3894 | ` */` |
|      20 | 3895 | `static int ph7_hashmap_diff_assoc(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       2 | 3896 |  |
|       - | 3897 | `	ph7_hashmap_node *pN1,*pN2,*pEntry;` |
|       - | 3898 | `	ph7_hashmap *pSrc,*pMap;` |
|       - | 3899 | `	ph7_value *pArray;` |
|       - | 3900 | `	ph7_value *pVal;` |
|       - | 3901 | `	sxi32 rc;` |
|       - | 3902 | `	sxu32 n;` |
|       - | 3903 | `	int i;` |
|       - | 3904 | `	/* Ensure the argument list is valid, emitting the same errors PHP` |
|       - | 3905 | `	 * would produce. This makes behaviour predictable and allows the` |
|       - | 3906 | `	 * accompanying integration tests to pass. */` |
|      22 | 3907 | `	if( nArg < 1 ){` |
|       4 | 3908 | `		return PH7_VmThrowException(pCtx,` |
|       - | 3909 | `			"ArgumentCountError",` |
|       - | 3910 | `			"array_diff_assoc() expects at least 1 argument, %d given",` |
|       1 | 3911 | `			nArg` |
|       - | 3912 | `			);` |
|       - | 3913 | `	}` |
|      20 | 3914 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|       4 | 3915 | `		return PH7_VmThrowException(pCtx,` |
|       - | 3916 | `			"TypeError",` |
|       - | 3917 | `			"array_diff_assoc(): Argument #1 ($array) must be of type array, %s given",` |
|       1 | 3918 | `			ph7_type_name(apArg[0])` |
|       - | 3919 | `			);` |
|       - | 3920 | `	}` |
|      32 | 3921 | `	for(i = 1 ; i < nArg ; i++){` |
|      20 | 3922 | `		if( !ph7_value_is_array(apArg[i]) ){` |
|       7 | 3923 | `			return PH7_VmThrowException(pCtx,` |
|       - | 3924 | `				"TypeError",` |
|       - | 3925 | `				"array_diff_assoc(): Argument #%d must be of type array, %s given",` |
|       2 | 3926 | `				i + 1,` |
|       4 | 3927 | `				ph7_type_name(apArg[i])` |
|       - | 3928 | `				);` |
|       - | 3929 | `		}` |
|       9 | 3930 | `	}` |
|      13 | 3931 | `	if( nArg == 1 ){` |
|       - | 3932 | `		/* Return the first array since we cannot perform a diff */` |
|       3 | 3933 | `		ph7_result_value(pCtx,apArg[0]);` |
|       3 | 3934 | `		return PH7_OK;` |
|       - | 3935 | `	}` |
|       - | 3936 | `	/* Create a new array */` |
|      11 | 3937 | `	pArray = ph7_context_new_array(pCtx);` |
|      11 | 3938 | `	if( pArray == 0 ){` |
|     ! 0 | 3939 | `		ph7_result_null(pCtx);` |
|     ! 0 | 3940 | `		return PH7_OK;` |
|       - | 3941 | `	}` |
|       - | 3942 | `	/* Point to the internal representation of the source hashmap */` |
|      11 | 3943 | `	pSrc = (ph7_hashmap *)apArg[0]->x.pOther;` |
|       - | 3944 | `	/* Perform the diff */` |
|      11 | 3945 | `	pEntry = pSrc->pFirst;` |
|      11 | 3946 | `	n = pSrc->nEntry;` |
|      11 | 3947 | `	pN1 = pN2 = 0;` |
|      29 | 3948 | `	for(;;){` |
|       - | 3949 | `		int keep;` |
|      35 | 3950 | `		if( n < 1 ){` |
|      11 | 3951 | `			break;` |
|       - | 3952 | `		}` |
|       - | 3953 | `		/* assume the element should be kept until we find a match */` |
|      25 | 3954 | `		keep = 1;` |
|      41 | 3955 | `		for( i = 1 ; i < nArg ; i++ ){` |
|       - | 3956 | `			/* all arguments have been validated already, so cast directly */` |
|      29 | 3957 | `			pMap = (ph7_hashmap *)apArg[i]->x.pOther;` |
|       - | 3958 | `			/* Perform a key lookup first */` |
|      29 | 3959 | `			if( pEntry->iType == HASHMAP_INT_NODE ){` |
|      13 | 3960 | `				rc = HashmapLookupIntKey(pMap,pEntry->xKey.iKey,&pN1);` |
|       7 | 3961 | `			}else{` |
|      17 | 3962 | `				rc = HashmapLookupBlobKey(pMap,SyBlobData(&pEntry->xKey.sKey),SyBlobLength(&pEntry->xKey.sKey),&pN1);` |
|       - | 3963 | `			}` |
|      29 | 3964 | `			if( rc != SXRET_OK ){` |
|       - | 3965 | `				/* this array does not contain the key, continue checking others */` |
|      15 | 3966 | `				continue;` |
|       - | 3967 | `			}` |
|       - | 3968 | `			/* key exists; check that value stored in the matching node is equal */` |
|      15 | 3969 | `			pVal = HashmapExtractNodeValue(pEntry);` |
|      15 | 3970 | `			if( pVal ){` |
|       - | 3971 | `				/* directly compare with value at pN1 rather than searching again */` |
|      15 | 3972 | `				ph7_value *pVal2 = HashmapExtractNodeValue(pN1);` |
|      15 | 3973 | `				if( pVal2 ){` |
|      15 | 3974 | `					sxi32 cmp = PH7_MemObjCmp(pVal, pVal2, TRUE, 0);` |
|      15 | 3975 | `					if( cmp == 0 ){` |
|       - | 3976 | `						/* identical key+value found in one of the arrays => drop it */` |
|      13 | 3977 | `						keep = 0;` |
|      13 | 3978 | `						break;` |
|       - | 3979 | `					}` |
|       1 | 3980 | `				}` |
|       1 | 3981 | `			}` |
|       2 | 3982 | `		}` |
|      25 | 3983 | `		if( keep ){` |
|       - | 3984 | `			/* Perform the insertion */` |
|      13 | 3985 | `			HashmapInsertNode((ph7_hashmap *)pArray->x.pOther,pEntry,TRUE);` |
|       6 | 3986 | `		}` |
|       - | 3987 | `		/* Point to the next entry */` |
|      25 | 3988 | `		pEntry = pEntry->pPrev; /* Reverse link */` |
|      25 | 3989 | `		n--;` |
|       1 | 3990 | `	}` |
|       - | 3991 | `	/* Return the freshly created array */` |
|      11 | 3992 | `	ph7_result_value(pCtx,pArray);` |
|      11 | 3993 | `	return PH7_OK;` |
|      12 | 3994 |  |
|       - | 3995 | `/*` |
|       - | 3996 | ` * array array_diff_uassoc(array $array1,array $array2,...,callback $key_compare_func)` |
|       - | 3997 | ` *  Computes the difference of arrays with additional index check which is performed` |
|       - | 3998 | ` *  by a user supplied callback function.` |
|       - | 3999 | ` * Parameters` |
|       - | 4000 | ` *  $array1` |
|       - | 4001 | ` *    The array to compare from` |
|       - | 4002 | ` *  $array2` |
|       - | 4003 | ` *    An array to compare against` |
|       - | 4004 | ` *  $...` |
|       - | 4005 | ` *   More arrays to compare against.` |
|       - | 4006 | ` *  $key_compare_func` |
|       - | 4007 | ` *   Callback function to use. The callback function must return an integer` |
|       - | 4008 | ` *   less than, equal to, or greater than zero if the first argument is considered` |
|       - | 4009 | ` *   to be respectively less than, equal to, or greater than the second.` |
|       - | 4010 | ` * Return` |
|       - | 4011 | ` *  Returns an array containing all the entries from array1 that` |
|       - | 4012 | ` *  are not present in any of the other arrays.` |
|       - | 4013 | ` */` |
|      22 | 4014 | `static int ph7_hashmap_diff_uassoc(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       2 | 4015 |  |
|       - | 4016 | `	ph7_hashmap_node *pEntry;` |
|       - | 4017 | `	ph7_hashmap *pSrc,*pMap;` |
|       - | 4018 | `	ph7_value *pCallback;` |
|       - | 4019 | `	ph7_value *pArray;` |
|       - | 4020 | `	sxi32 rc;` |
|       - | 4021 | `	sxu32 n;` |
|       - | 4022 | `	int i;` |
|       - | 4023 |  |
|       - | 4024 | `	/* Argument validation mimicking PHP errors. */` |
|      24 | 4025 | `	if( nArg < 2 ){` |
|       4 | 4026 | `		return PH7_VmThrowException(pCtx,` |
|       - | 4027 | `			"ArgumentCountError",` |
|       - | 4028 | `			"array_diff_uassoc() expects at least 2 arguments, %d given",` |
|       1 | 4029 | `			nArg` |
|       - | 4030 | `			);` |
|       - | 4031 | `	}` |
|      22 | 4032 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|       4 | 4033 | `		return PH7_VmThrowException(pCtx,` |
|       - | 4034 | `			"TypeError",` |
|       - | 4035 | `			"array_diff_uassoc(): Argument #1 ($array) must be of type array, %s given",` |
|       1 | 4036 | `			ph7_type_name(apArg[0])` |
|       - | 4037 | `			);` |
|       - | 4038 | `	}` |
|       - | 4039 | `	/* Intermediate arguments (except last) must be arrays. Last argument is` |
|       - | 4040 | `	 * expected to be a callback. */` |
|      32 | 4041 | `	for(i = 1 ; i < nArg - 1; i++){` |
|      16 | 4042 | `		if( !ph7_value_is_array(apArg[i]) ){` |
|       4 | 4043 | `			return PH7_VmThrowException(pCtx,` |
|       - | 4044 | `				"TypeError",` |
|       - | 4045 | `				"array_diff_uassoc(): Argument #%d must be of type array, %s given",` |
|       1 | 4046 | `				i + 1,` |
|       2 | 4047 | `				ph7_type_name(apArg[i])` |
|       - | 4048 | `				);` |
|       - | 4049 | `		}` |
|       8 | 4050 | `	}` |
|       - | 4051 | `	/* Point to the callback value */` |
|      18 | 4052 | `	pCallback = apArg[nArg - 1];` |
|      18 | 4053 | `	if( !ph7_value_is_callable(pCallback) ){` |
|       - | 4054 | `		/* Compose an error message that closely matches PHP output. When the` |
|       - | 4055 | `		 * argument is an array of the wrong shape we include an extra clause.` |
|       - | 4056 | `		 * If the value is neither array nor string, PHP says "no array or` |
|       - | 4057 | `		 * string given" which we also reproduce. */` |
|       7 | 4058 | `		if( ph7_value_is_array(pCallback) ){` |
|       - | 4059 | `			/* ARRAY CALLBACK must have exactly two members */` |
|       4 | 4060 | `			return PH7_VmThrowException(pCtx,` |
|       - | 4061 | `				"TypeError",` |
|       - | 4062 | `				"array_diff_uassoc(): Argument #%d must be a valid callback, array callback must have exactly two members",` |
|       1 | 4063 | `				nArg` |
|       - | 4064 | `				);` |
|       - | 4065 | `		}` |
|       5 | 4066 | `		if( !ph7_value_is_string(pCallback) ){` |
|       - | 4067 | `			/* neither array nor string */` |
|       7 | 4068 | `			return PH7_VmThrowException(pCtx,` |
|       - | 4069 | `				"TypeError",` |
|       - | 4070 | `				"array_diff_uassoc(): Argument #%d must be a valid callback, no array or string given",` |
|       2 | 4071 | `				nArg` |
|       - | 4072 | `				);` |
|       - | 4073 | `		}` |
|       - | 4074 | `		/* Fallback for string (non-callable) or other leftover cases */` |
|     ! 0 | 4075 | `		return PH7_VmThrowException(pCtx,` |
|       - | 4076 | `			"TypeError",` |
|       - | 4077 | `			"array_diff_uassoc(): Argument #%d must be a valid callback, %s given",` |
|     ! 0 | 4078 | `			nArg,` |
|     ! 0 | 4079 | `			ph7_type_name(pCallback)` |
|       - | 4080 | `			);` |
|       - | 4081 | `	}` |
|      11 | 4082 | `	if( nArg == 2 ){` |
|       - | 4083 | `		/* If we only have the first array and the callback, just return the` |
|       - | 4084 | `		 * input array. */` |
|       3 | 4085 | `		ph7_result_value(pCtx,apArg[0]);` |
|       3 | 4086 | `		return PH7_OK;` |
|       - | 4087 | `	}` |
|       - | 4088 | `	/* Create a new array */` |
|       9 | 4089 | `	pArray = ph7_context_new_array(pCtx);` |
|       9 | 4090 | `	if( pArray == 0 ){` |
|     ! 0 | 4091 | `		ph7_result_null(pCtx);` |
|     ! 0 | 4092 | `		return PH7_OK;` |
|       - | 4093 | `	}` |
|       - | 4094 | `	/* Point to the internal representation of the source hashmap */` |
|       9 | 4095 | `	pSrc = (ph7_hashmap *)apArg[0]->x.pOther;` |
|       - | 4096 | `	/* Perform the diff */` |
|       9 | 4097 | `	pEntry = pSrc->pFirst;` |
|       9 | 4098 | `	n = pSrc->nEntry;` |
|      20 | 4099 | `	for(;;){` |
|       - | 4100 | `		int keep;` |
|      25 | 4101 | `		if( n < 1 ){` |
|       9 | 4102 | `			break;` |
|       - | 4103 | `		}` |
|      17 | 4104 | `		keep = 1;` |
|      29 | 4105 | `		for( i = 1 ; i < nArg - 1; i++ ){` |
|       - | 4106 | `			/* each of these must already be arrays thanks to earlier validation */` |
|      21 | 4107 | `			pMap = (ph7_hashmap *)apArg[i]->x.pOther;` |
|       - | 4108 | `			/* we must compare keys via callback, not by direct lookup */` |
|      21 | 4109 | `			ph7_hashmap_node *pIt = pMap->pFirst;` |
|      43 | 4110 | `			while( pIt ){` |
|       - | 4111 | `				/* build temporary key values for callback */` |
|       - | 4112 | `				ph7_value key1, key2, result;` |
|       - | 4113 | `				/* initialise only once using the appropriate helper */` |
|      31 | 4114 | `				if( pEntry->iType == HASHMAP_INT_NODE ){` |
|     ! 0 | 4115 | `					PH7_MemObjInitFromInt(pMap->pVm,&key1,pEntry->xKey.iKey);` |
|     ! 0 | 4116 | `				}else{` |
|       - | 4117 | `					SyString sStr;` |
|      31 | 4118 | `					SyStringInitFromBuf(&sStr,` |
|       - | 4119 | `						SyBlobData(&pEntry->xKey.sKey),` |
|       - | 4120 | `						SyBlobLength(&pEntry->xKey.sKey));` |
|      31 | 4121 | `					PH7_MemObjInitFromString(pMap->pVm,&key1,&sStr);` |
|       - | 4122 | `				}` |
|      31 | 4123 | `				if( pIt->iType == HASHMAP_INT_NODE ){` |
|     ! 0 | 4124 | `					PH7_MemObjInitFromInt(pMap->pVm,&key2,pIt->xKey.iKey);` |
|     ! 0 | 4125 | `				}else{` |
|       - | 4126 | `					SyString sStr;` |
|      31 | 4127 | `					SyStringInitFromBuf(&sStr,` |
|       - | 4128 | `						SyBlobData(&pIt->xKey.sKey),` |
|       - | 4129 | `						SyBlobLength(&pIt->xKey.sKey));` |
|      31 | 4130 | `					PH7_MemObjInitFromString(pMap->pVm,&key2,&sStr);` |
|       - | 4131 | `				}` |
|      31 | 4132 | `				PH7_MemObjInit(pMap->pVm,&result);` |
|       - | 4133 | `				/* call user callback with (key1, key2) */` |
|       - | 4134 | `				{` |
|       - | 4135 | `					ph7_value *apK[2];` |
|      31 | 4136 | `					apK[0] = &key1;` |
|      31 | 4137 | `					apK[1] = &key2;` |
|      31 | 4138 | `					rc = PH7_VmCallUserFunction(pMap->pVm,pCallback,2,apK,&result);` |
|       - | 4139 | `				}` |
|      31 | 4140 | `				if( rc == SXRET_OK ){` |
|      31 | 4141 | `					if( (result.iFlags & MEMOBJ_INT) == 0 ){` |
|     ! 0 | 4142 | `						PH7_MemObjToInteger(&result);` |
|     ! 0 | 4143 | `					}` |
|      31 | 4144 | `					if( result.x.iVal == 0 ){` |
|       - | 4145 | `						/* keys considered equal by callback; now compare values */` |
|      13 | 4146 | `						ph7_value *pVal1 = HashmapExtractNodeValue(pEntry);` |
|      13 | 4147 | `						ph7_value *pVal2 = HashmapExtractNodeValue(pIt);` |
|      13 | 4148 | `						if( pVal1 && pVal2 ){` |
|      13 | 4149 | `							if( PH7_MemObjCmp(pVal1,pVal2,TRUE,0) == 0 ){` |
|       9 | 4150 | `								keep = 0;` |
|       9 | 4151 | `								PH7_MemObjRelease(&result);` |
|       - | 4152 | `								/* release keys too before breaking */` |
|       9 | 4153 | `								PH7_MemObjRelease(&key1);` |
|       9 | 4154 | `								PH7_MemObjRelease(&key2);` |
|       9 | 4155 | `								break;` |
|       - | 4156 | `							}` |
|       2 | 4157 | `						}` |
|       2 | 4158 | `					}` |
|      11 | 4159 | `				}` |
|      23 | 4160 | `				PH7_MemObjRelease(&result);` |
|      23 | 4161 | `				PH7_MemObjRelease(&key1);` |
|      23 | 4162 | `				PH7_MemObjRelease(&key2);` |
|       - | 4163 | `				/* move to next node */` |
|      23 | 4164 | `				pIt = pIt->pPrev;` |
|      23 | 4165 | `				if( keep == 0 ) break;` |
|       1 | 4166 | `			}` |
|      21 | 4167 | `			if( keep == 0 ) break;` |
|       7 | 4168 | `		}` |
|      17 | 4169 | `		if( keep ){` |
|       - | 4170 | `			/* Perform the insertion */` |
|       9 | 4171 | `			HashmapInsertNode((ph7_hashmap *)pArray->x.pOther,pEntry,TRUE);` |
|       4 | 4172 | `		}` |
|       - | 4173 | `		/* Point to the next entry */` |
|      17 | 4174 | `		pEntry = pEntry->pPrev; /* Reverse link */` |
|      17 | 4175 | `		n--;` |
|       1 | 4176 | `	}` |
|       - | 4177 | `	/* Return the freshly created array */` |
|       9 | 4178 | `	ph7_result_value(pCtx,pArray);` |
|       9 | 4179 | `	return PH7_OK;` |
|      13 | 4180 |  |
|       - | 4181 | `/*` |
|       - | 4182 | ` * array array_diff_key(array $array1 ,array $array2,...)` |
|       - | 4183 | ` *  Computes the difference of arrays using keys for comparison.` |
|       - | 4184 | ` * Parameters` |
|       - | 4185 | ` *  $array1` |
|       - | 4186 | ` *    The array to compare from` |
|       - | 4187 | ` *  $array2` |
|       - | 4188 | ` *    An array to compare against` |
|       - | 4189 | ` *  $...` |
|       - | 4190 | ` *   More arrays to compare against` |
|       - | 4191 | ` * Return` |
|       - | 4192 | ` *  Returns an array containing all the entries from array1 whose keys are not present` |
|       - | 4193 | ` *  in any of the other arrays.` |
|       - | 4194 | ` * Note that NULL is returned on failure.` |
|       - | 4195 | ` */` |
|      14 | 4196 | `static int ph7_hashmap_diff_key(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       2 | 4197 |  |
|       - | 4198 | `	ph7_hashmap_node *pEntry;` |
|       - | 4199 | `	ph7_hashmap *pSrc,*pMap;` |
|       - | 4200 | `	ph7_value *pArray;` |
|       - | 4201 | `	sxi32 rc;` |
|       - | 4202 | `	sxu32 n;` |
|       - | 4203 | `	int i;` |
|       - | 4204 | `	/* Validate arguments to mirror PHP behaviour. Previously invalid inputs` |
|       - | 4205 | `	 * would quietly return NULL which is inconsistent with other hashmap` |
|       - | 4206 | `	 * helpers. */` |
|      16 | 4207 | `	if( nArg < 1 ){` |
|       4 | 4208 | `		return PH7_VmThrowException(pCtx,` |
|       - | 4209 | `			"ArgumentCountError",` |
|       - | 4210 | `			"array_diff_key() expects at least 1 argument, %d given",` |
|       1 | 4211 | `			nArg` |
|       - | 4212 | `			);` |
|       - | 4213 | `	}` |
|      14 | 4214 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|       4 | 4215 | `		return PH7_VmThrowException(pCtx,` |
|       - | 4216 | `			"TypeError",` |
|       - | 4217 | `			"array_diff_key(): Argument #1 ($array) must be of type array, %s given",` |
|       1 | 4218 | `			ph7_type_name(apArg[0])` |
|       - | 4219 | `			);` |
|       - | 4220 | `	}` |
|      20 | 4221 | `	for(i = 1 ; i < nArg ; i++){` |
|      12 | 4222 | `		if( !ph7_value_is_array(apArg[i]) ){` |
|       4 | 4223 | `			return PH7_VmThrowException(pCtx,` |
|       - | 4224 | `				"TypeError",` |
|       - | 4225 | `				"array_diff_key(): Argument #%d must be of type array, %s given",` |
|       1 | 4226 | `				i + 1,` |
|       2 | 4227 | `				ph7_type_name(apArg[i])` |
|       - | 4228 | `				);` |
|       - | 4229 | `		}` |
|       5 | 4230 | `	}` |
|       9 | 4231 | `	if( nArg == 1 ){` |
|       - | 4232 | `		/* Return the first array since we cannot perform a diff */` |
|       3 | 4233 | `		ph7_result_value(pCtx,apArg[0]);` |
|       3 | 4234 | `		return PH7_OK;` |
|       - | 4235 | `	}` |
|       - | 4236 | `	/* Create a new array */` |
|       7 | 4237 | `	pArray = ph7_context_new_array(pCtx);` |
|       7 | 4238 | `	if( pArray == 0 ){` |
|     ! 0 | 4239 | `		ph7_result_null(pCtx);` |
|     ! 0 | 4240 | `		return PH7_OK;` |
|       - | 4241 | `	}` |
|       - | 4242 | `	/* Point to the internal representation of the main hashmap */` |
|       7 | 4243 | `	pSrc = (ph7_hashmap *)apArg[0]->x.pOther;` |
|       - | 4244 | `	/* Perfrom the diff */` |
|       7 | 4245 | `	pEntry = pSrc->pFirst;` |
|       7 | 4246 | `	n = pSrc->nEntry;` |
|      12 | 4247 | `	for(;;){` |
|      25 | 4248 | `		if( n < 1 ){` |
|       7 | 4249 | `			break;` |
|       - | 4250 | `		}` |
|      31 | 4251 | `		for( i = 1 ; i < nArg ; i++ ){` |
|      23 | 4252 | `			if( !ph7_value_is_array(apArg[i])) {` |
|       - | 4253 | `				/* ignore */` |
|     ! 0 | 4254 | `				continue;` |
|       - | 4255 | `			}` |
|      23 | 4256 | `			pMap = (ph7_hashmap *)apArg[i]->x.pOther;` |
|      23 | 4257 | `			if( pEntry->iType == HASHMAP_BLOB_NODE ){` |
|      17 | 4258 | `				SyBlob *pKey = &pEntry->xKey.sKey;` |
|       - | 4259 | `				/* Blob lookup */` |
|      17 | 4260 | `				rc = HashmapLookupBlobKey(pMap,SyBlobData(pKey),SyBlobLength(pKey),0);` |
|       9 | 4261 | `			}else{` |
|       - | 4262 | `				/* Int lookup */` |
|       7 | 4263 | `				rc = HashmapLookupIntKey(pMap,pEntry->xKey.iKey,0);` |
|       - | 4264 | `			}` |
|      23 | 4265 | `			if( rc == SXRET_OK ){` |
|       - | 4266 | `				/* Key exists,break immediately */` |
|      11 | 4267 | `				break;` |
|       - | 4268 | `			}` |
|       7 | 4269 | `		}` |
|      19 | 4270 | `		if( i >= nArg ){` |
|       - | 4271 | `			/* Perform the insertion */` |
|       9 | 4272 | `			HashmapInsertNode((ph7_hashmap *)pArray->x.pOther,pEntry,TRUE);` |
|       4 | 4273 | `		}` |
|       - | 4274 | `		/* Point to the next entry */` |
|      19 | 4275 | `		pEntry = pEntry->pPrev; /* Reverse link */` |
|      19 | 4276 | `		n--;` |
|       1 | 4277 | `	}` |
|       - | 4278 | `	/* Return the freshly created array */` |
|       7 | 4279 | `	ph7_result_value(pCtx,pArray);` |
|       7 | 4280 | `	return PH7_OK;` |
|       9 | 4281 |  |
|       - | 4282 | `/*` |
|       - | 4283 | ` * array array_intersect(array $array1 ,array $array2,...)` |
|       - | 4284 | ` *  Computes the intersection of arrays.` |
|       - | 4285 | ` * Parameters` |
|       - | 4286 | ` *  $array1` |
|       - | 4287 | ` *    The array to compare from` |
|       - | 4288 | ` *  $array2` |
|       - | 4289 | ` *    An array to compare against` |
|       - | 4290 | ` *  $...` |
|       - | 4291 | ` *   More arrays to compare against` |
|       - | 4292 | ` * Return` |
|       - | 4293 | ` *  Returns an array containing all of the values in array1 whose values exist` |
|       - | 4294 | ` *  in all of the parameters. .` |
|       - | 4295 | ` * Note that NULL is returned on failure.` |
|       - | 4296 | ` */` |
|       2 | 4297 | `static int ph7_hashmap_intersect(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       1 | 4298 |  |
|       - | 4299 | `	ph7_hashmap_node *pEntry;` |
|       - | 4300 | `	ph7_hashmap *pSrc,*pMap;` |
|       - | 4301 | `	ph7_value *pArray;` |
|       - | 4302 | `	ph7_value *pVal;` |
|       - | 4303 | `	sxi32 rc;` |
|       - | 4304 | `	sxu32 n;` |
|       - | 4305 | `	int i;` |
|       3 | 4306 | `	if( nArg < 1 \|\| !ph7_value_is_array(apArg[0]) ){` |
|       - | 4307 | `		/* Missing arguments,return NULL */` |
|     ! 0 | 4308 | `		ph7_result_null(pCtx);` |
|     ! 0 | 4309 | `		return PH7_OK;` |
|       - | 4310 | `	}` |
|       3 | 4311 | `	if( nArg == 1 ){` |
|       - | 4312 | `		/* Return the first array since we cannot perform a diff */` |
|     ! 0 | 4313 | `		ph7_result_value(pCtx,apArg[0]);` |
|     ! 0 | 4314 | `		return PH7_OK;` |
|       - | 4315 | `	}` |
|       - | 4316 | `	/* Create a new array */` |
|       3 | 4317 | `	pArray = ph7_context_new_array(pCtx);` |
|       3 | 4318 | `	if( pArray == 0 ){` |
|     ! 0 | 4319 | `		ph7_result_null(pCtx);` |
|     ! 0 | 4320 | `		return PH7_OK;` |
|       - | 4321 | `	}` |
|       - | 4322 | `	/* Point to the internal representation of the source hashmap */` |
|       3 | 4323 | `	pSrc = (ph7_hashmap *)apArg[0]->x.pOther;` |
|       - | 4324 | `	/* Perform the intersection */` |
|       3 | 4325 | `	pEntry = pSrc->pFirst;` |
|       3 | 4326 | `	n = pSrc->nEntry;` |
|       5 | 4327 | `	for(;;){` |
|      11 | 4328 | `		if( n < 1 ){` |
|       3 | 4329 | `			break;` |
|       - | 4330 | `		}` |
|       - | 4331 | `		/* Extract the node value */` |
|       9 | 4332 | `		pVal = HashmapExtractNodeValue(pEntry);` |
|       9 | 4333 | `		if( pVal ){` |
|      13 | 4334 | `			for( i = 1 ; i < nArg ; i++ ){` |
|       9 | 4335 | `				if( !ph7_value_is_array(apArg[i])) {` |
|       - | 4336 | `					/* ignore */` |
|     ! 0 | 4337 | `					continue;` |
|       - | 4338 | `				}` |
|       - | 4339 | `				/* Point to the internal representation of the hashmap */` |
|       9 | 4340 | `				pMap = (ph7_hashmap *)apArg[i]->x.pOther;` |
|       - | 4341 | `				/* Perform the lookup */` |
|       9 | 4342 | `				rc = HashmapFindValue(pMap,pVal,0,TRUE);` |
|       9 | 4343 | `				if( rc != SXRET_OK ){` |
|       - | 4344 | `					/* Value does not exist */` |
|       5 | 4345 | `					break;` |
|       - | 4346 | `				}` |
|       3 | 4347 | `			}` |
|       9 | 4348 | `			if( i >= nArg ){` |
|       - | 4349 | `				/* Perform the insertion */` |
|       5 | 4350 | `				HashmapInsertNode((ph7_hashmap *)pArray->x.pOther,pEntry,TRUE);` |
|       2 | 4351 | `			}` |
|       4 | 4352 | `		}` |
|       - | 4353 | `		/* Point to the next entry */` |
|       9 | 4354 | `		pEntry = pEntry->pPrev; /* Reverse link */` |
|       9 | 4355 | `		n--;` |
|       1 | 4356 | `	}` |
|       - | 4357 | `	/* Return the freshly created array */` |
|       3 | 4358 | `	ph7_result_value(pCtx,pArray);` |
|       3 | 4359 | `	return PH7_OK;` |
|       2 | 4360 |  |
|       - | 4361 | `/*` |
|       - | 4362 | ` * array array_intersect_assoc(array $array1 ,array $array2,...)` |
|       - | 4363 | ` *  Computes the intersection of arrays.` |
|       - | 4364 | ` * Parameters` |
|       - | 4365 | ` *  $array1` |
|       - | 4366 | ` *    The array to compare from` |
|       - | 4367 | ` *  $array2` |
|       - | 4368 | ` *    An array to compare against` |
|       - | 4369 | ` *  $...` |
|       - | 4370 | ` *   More arrays to compare against` |
|       - | 4371 | ` * Return` |
|       - | 4372 | ` *  Returns an array containing all of the values in array1 whose values exist` |
|       - | 4373 | ` *  in all of the parameters. .` |
|       - | 4374 | ` * Note that NULL is returned on failure.` |
|       - | 4375 | ` */` |
|       2 | 4376 | `static int ph7_hashmap_intersect_assoc(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       1 | 4377 |  |
|       - | 4378 | `	ph7_hashmap_node *pEntry,*pN1,*pN2;` |
|       - | 4379 | `	ph7_hashmap *pSrc,*pMap;` |
|       - | 4380 | `	ph7_value *pArray;` |
|       - | 4381 | `	ph7_value *pVal;` |
|       - | 4382 | `	sxi32 rc;` |
|       - | 4383 | `	sxu32 n;` |
|       - | 4384 | `	int i;` |
|       3 | 4385 | `	if( nArg < 1 \|\| !ph7_value_is_array(apArg[0]) ){` |
|       - | 4386 | `		/* Missing arguments,return NULL */` |
|     ! 0 | 4387 | `		ph7_result_null(pCtx);` |
|     ! 0 | 4388 | `		return PH7_OK;` |
|       - | 4389 | `	}` |
|       3 | 4390 | `	if( nArg == 1 ){` |
|       - | 4391 | `		/* Return the first array since we cannot perform a diff */` |
|     ! 0 | 4392 | `		ph7_result_value(pCtx,apArg[0]);` |
|     ! 0 | 4393 | `		return PH7_OK;` |
|       - | 4394 | `	}` |
|       - | 4395 | `	/* Create a new array */` |
|       3 | 4396 | `	pArray = ph7_context_new_array(pCtx);` |
|       3 | 4397 | `	if( pArray == 0 ){` |
|     ! 0 | 4398 | `		ph7_result_null(pCtx);` |
|     ! 0 | 4399 | `		return PH7_OK;` |
|       - | 4400 | `	}` |
|       - | 4401 | `	/* Point to the internal representation of the source hashmap */` |
|       3 | 4402 | `	pSrc = (ph7_hashmap *)apArg[0]->x.pOther;` |
|       - | 4403 | `	/* Perform the intersection */` |
|       3 | 4404 | `	pEntry = pSrc->pFirst;` |
|       3 | 4405 | `	n = pSrc->nEntry;` |
|       3 | 4406 | `	pN1 = pN2 = 0; /* cc warning */` |
|       4 | 4407 | `	for(;;){` |
|       9 | 4408 | `		if( n < 1 ){` |
|       3 | 4409 | `			break;` |
|       - | 4410 | `		}` |
|       - | 4411 | `		/* Extract the node value */` |
|       7 | 4412 | `		pVal = HashmapExtractNodeValue(pEntry);` |
|       7 | 4413 | `		if( pVal ){` |
|       9 | 4414 | `			for( i = 1 ; i < nArg ; i++ ){` |
|       7 | 4415 | `				if( !ph7_value_is_array(apArg[i])) {` |
|       - | 4416 | `					/* ignore */` |
|     ! 0 | 4417 | `					continue;` |
|       - | 4418 | `				}` |
|       - | 4419 | `				/* Point to the internal representation of the hashmap */` |
|       7 | 4420 | `				pMap = (ph7_hashmap *)apArg[i]->x.pOther;` |
|       - | 4421 | `				/* Perform a key lookup first */` |
|       7 | 4422 | `				if( pEntry->iType == HASHMAP_INT_NODE ){` |
|     ! 0 | 4423 | `					rc = HashmapLookupIntKey(pMap,pEntry->xKey.iKey,&pN1);` |
|     ! 0 | 4424 | `				}else{` |
|       7 | 4425 | `					rc = HashmapLookupBlobKey(pMap,SyBlobData(&pEntry->xKey.sKey),SyBlobLength(&pEntry->xKey.sKey),&pN1);` |
|       - | 4426 | `				}` |
|       7 | 4427 | `				if( rc != SXRET_OK ){` |
|       - | 4428 | `					/* No such key,break immediately */` |
|       3 | 4429 | `					break;` |
|       - | 4430 | `				}` |
|       - | 4431 | `				/* Perform the lookup */` |
|       5 | 4432 | `				rc = HashmapFindValue(pMap,pVal,&pN2,TRUE);` |
|       5 | 4433 | `				if( rc != SXRET_OK \|\| pN1 != pN2 ){` |
|       - | 4434 | `					/* Value does not exist */` |
|       2 | 4435 | `					break;` |
|       - | 4436 | `				}` |
|       2 | 4437 | `			}` |
|       7 | 4438 | `			if( i >= nArg ){` |
|       - | 4439 | `				/* Perform the insertion */` |
|       3 | 4440 | `				HashmapInsertNode((ph7_hashmap *)pArray->x.pOther,pEntry,TRUE);` |
|       1 | 4441 | `			}` |
|       3 | 4442 | `		}` |
|       - | 4443 | `		/* Point to the next entry */` |
|       7 | 4444 | `		pEntry = pEntry->pPrev; /* Reverse link */` |
|       7 | 4445 | `		n--;` |
|       1 | 4446 | `	}` |
|       - | 4447 | `	/* Return the freshly created array */` |
|       3 | 4448 | `	ph7_result_value(pCtx,pArray);` |
|       3 | 4449 | `	return PH7_OK;` |
|       2 | 4450 |  |
|       - | 4451 | `/*` |
|       - | 4452 | ` * array array_intersect_key(array $array1 ,array $array2,...)` |
|       - | 4453 | ` *  Computes the intersection of arrays using keys for comparison.` |
|       - | 4454 | ` * Parameters` |
|       - | 4455 | ` *  $array1` |
|       - | 4456 | ` *    The array to compare from` |
|       - | 4457 | ` *  $array2` |
|       - | 4458 | ` *    An array to compare against` |
|       - | 4459 | ` *  $...` |
|       - | 4460 | ` *   More arrays to compare against` |
|       - | 4461 | ` * Return` |
|       - | 4462 | ` *  Returns an associative array containing all the entries of array1 which` |
|       - | 4463 | ` *  have keys that are present in all arguments.` |
|       - | 4464 | ` * Note that NULL is returned on failure.` |
|       - | 4465 | ` */` |
|       4 | 4466 | `static int ph7_hashmap_intersect_key(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       1 | 4467 |  |
|       - | 4468 | `	ph7_hashmap_node *pEntry;` |
|       - | 4469 | `	ph7_hashmap *pSrc,*pMap;` |
|       - | 4470 | `	ph7_value *pArray;` |
|       - | 4471 | `	sxi32 rc;` |
|       - | 4472 | `	sxu32 n;` |
|       - | 4473 | `	int i;` |
|       5 | 4474 | `	if( nArg < 1 \|\| !ph7_value_is_array(apArg[0]) ){` |
|       - | 4475 | `		/* Missing arguments,return NULL */` |
|     ! 0 | 4476 | `		ph7_result_null(pCtx);` |
|     ! 0 | 4477 | `		return PH7_OK;` |
|       - | 4478 | `	}` |
|       5 | 4479 | `	if( nArg == 1 ){` |
|       - | 4480 | `		/* Return the first array since we cannot perform a diff */` |
|     ! 0 | 4481 | `		ph7_result_value(pCtx,apArg[0]);` |
|     ! 0 | 4482 | `		return PH7_OK;` |
|       - | 4483 | `	}` |
|       - | 4484 | `	/* Create a new array */` |
|       5 | 4485 | `	pArray = ph7_context_new_array(pCtx);` |
|       5 | 4486 | `	if( pArray == 0 ){` |
|     ! 0 | 4487 | `		ph7_result_null(pCtx);` |
|     ! 0 | 4488 | `		return PH7_OK;` |
|       - | 4489 | `	}` |
|       - | 4490 | `	/* Point to the internal representation of the main hashmap */` |
|       5 | 4491 | `	pSrc = (ph7_hashmap *)apArg[0]->x.pOther;` |
|       - | 4492 | `	/* Perfrom the intersection */` |
|       5 | 4493 | `	pEntry = pSrc->pFirst;` |
|       5 | 4494 | `	n = pSrc->nEntry;` |
|       8 | 4495 | `	for(;;){` |
|      17 | 4496 | `		if( n < 1 ){` |
|       5 | 4497 | `			break;` |
|       - | 4498 | `		}` |
|      19 | 4499 | `		for( i = 1 ; i < nArg ; i++ ){` |
|      13 | 4500 | `			if( !ph7_value_is_array(apArg[i])) {` |
|       - | 4501 | `				/* ignore */` |
|     ! 0 | 4502 | `				continue;` |
|       - | 4503 | `			}` |
|      13 | 4504 | `			pMap = (ph7_hashmap *)apArg[i]->x.pOther;` |
|      13 | 4505 | `			if( pEntry->iType == HASHMAP_BLOB_NODE ){` |
|       7 | 4506 | `				SyBlob *pKey = &pEntry->xKey.sKey;` |
|       - | 4507 | `				/* Blob lookup */` |
|       7 | 4508 | `				rc = HashmapLookupBlobKey(pMap,SyBlobData(pKey),SyBlobLength(pKey),0);` |
|       4 | 4509 | `			}else{` |
|       - | 4510 | `				/* Int key */` |
|       7 | 4511 | `				rc = HashmapLookupIntKey(pMap,pEntry->xKey.iKey,0);` |
|       - | 4512 | `			}` |
|      13 | 4513 | `			if( rc != SXRET_OK ){` |
|       - | 4514 | `				/* Key does not exists,break immediately */` |
|       7 | 4515 | `				break;` |
|       - | 4516 | `			}` |
|       4 | 4517 | `		}` |
|      13 | 4518 | `		if( i >= nArg ){` |
|       - | 4519 | `			/* Perform the insertion */` |
|       7 | 4520 | `			HashmapInsertNode((ph7_hashmap *)pArray->x.pOther,pEntry,TRUE);` |
|       3 | 4521 | `		}` |
|       - | 4522 | `		/* Point to the next entry */` |
|      13 | 4523 | `		pEntry = pEntry->pPrev; /* Reverse link */` |
|      13 | 4524 | `		n--;` |
|       1 | 4525 | `	}` |
|       - | 4526 | `	/* Return the freshly created array */` |
|       5 | 4527 | `	ph7_result_value(pCtx,pArray);` |
|       5 | 4528 | `	return PH7_OK;` |
|       3 | 4529 |  |
|       - | 4530 | `/*` |
|       - | 4531 | ` * array array_uintersect(array $array1 ,array $array2,...,$callback)` |
|       - | 4532 | ` *  Computes the intersection of arrays.` |
|       - | 4533 | ` * Parameters` |
|       - | 4534 | ` *  $array1` |
|       - | 4535 | ` *    The array to compare from` |
|       - | 4536 | ` *  $array2` |
|       - | 4537 | ` *    An array to compare against` |
|       - | 4538 | ` *  $...` |
|       - | 4539 | ` *   More arrays to compare against` |
|       - | 4540 | ` * $callback` |
|       - | 4541 | ` *  The callback comparison function.` |
|       - | 4542 | ` *  The comparison function must return an integer less than, equal to, or greater than zero` |
|       - | 4543 | ` *  if the first argument is considered to be respectively less than, equal to, or greater` |
|       - | 4544 | ` *  than the second.` |
|       - | 4545 | ` *     int callback ( mixed $a, mixed $b )` |
|       - | 4546 | ` * Return` |
|       - | 4547 | ` *  Returns an array containing all of the values in array1 whose values exist` |
|       - | 4548 | ` *  in all of the parameters. .` |
|       - | 4549 | ` * Note that NULL is returned on failure.` |
|       - | 4550 | ` */` |
|       2 | 4551 | `static int ph7_hashmap_uintersect(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       1 | 4552 |  |
|       - | 4553 | `	ph7_hashmap_node *pEntry;` |
|       - | 4554 | `	ph7_hashmap *pSrc,*pMap;` |
|       - | 4555 | `	ph7_value *pCallback;` |
|       - | 4556 | `	ph7_value *pArray;` |
|       - | 4557 | `	ph7_value *pVal;` |
|       - | 4558 | `	sxi32 rc;` |
|       - | 4559 | `	sxu32 n;` |
|       - | 4560 | `	int i;` |
|       - | 4561 |  |
|       3 | 4562 | `	if( nArg < 2 \|\| !ph7_value_is_array(apArg[0]) ){` |
|       - | 4563 | `		/* Missing/Invalid arguments,return NULL */` |
|     ! 0 | 4564 | `		ph7_result_null(pCtx);` |
|     ! 0 | 4565 | `		return PH7_OK;` |
|       - | 4566 | `	}` |
|       - | 4567 | `	/* Point to the callback */` |
|       3 | 4568 | `	pCallback = apArg[nArg - 1];` |
|       3 | 4569 | `	if( nArg == 2 ){` |
|       - | 4570 | `		/* Return the first array since we cannot perform a diff */` |
|     ! 0 | 4571 | `		ph7_result_value(pCtx,apArg[0]);` |
|     ! 0 | 4572 | `		return PH7_OK;` |
|       - | 4573 | `	}` |
|       - | 4574 | `	/* Create a new array */` |
|       3 | 4575 | `	pArray = ph7_context_new_array(pCtx);` |
|       3 | 4576 | `	if( pArray == 0 ){` |
|     ! 0 | 4577 | `		ph7_result_null(pCtx);` |
|     ! 0 | 4578 | `		return PH7_OK;` |
|       - | 4579 | `	}` |
|       - | 4580 | `	/* Point to the internal representation of the source hashmap */` |
|       3 | 4581 | `	pSrc = (ph7_hashmap *)apArg[0]->x.pOther;` |
|       - | 4582 | `	/* Perform the intersection */` |
|       3 | 4583 | `	pEntry = pSrc->pFirst;` |
|       3 | 4584 | `	n = pSrc->nEntry;` |
|       4 | 4585 | `	for(;;){` |
|       9 | 4586 | `		if( n < 1 ){` |
|       3 | 4587 | `			break;` |
|       - | 4588 | `		}` |
|       - | 4589 | `		/* Extract the node value */` |
|       7 | 4590 | `		pVal = HashmapExtractNodeValue(pEntry);` |
|       7 | 4591 | `		if( pVal ){` |
|      11 | 4592 | `			for( i = 1 ; i < nArg - 1; i++ ){` |
|       7 | 4593 | `				if( !ph7_value_is_array(apArg[i])) {` |
|       - | 4594 | `					/* ignore */` |
|     ! 0 | 4595 | `					continue;` |
|       - | 4596 | `				}` |
|       - | 4597 | `				/* Point to the internal representation of the hashmap */` |
|       7 | 4598 | `				pMap = (ph7_hashmap *)apArg[i]->x.pOther;` |
|       - | 4599 | `				/* Perform the lookup */` |
|       7 | 4600 | `				rc = HashmapFindValueByCallback(pMap,pVal,pCallback,0);` |
|       7 | 4601 | `				if( rc != SXRET_OK ){` |
|       - | 4602 | `					/* Value does not exist */` |
|       3 | 4603 | `					break;` |
|       - | 4604 | `				}` |
|       3 | 4605 | `			}` |
|       7 | 4606 | `			if( i >= (nArg-1) ){` |
|       - | 4607 | `				/* Perform the insertion */` |
|       5 | 4608 | `				HashmapInsertNode((ph7_hashmap *)pArray->x.pOther,pEntry,TRUE);` |
|       2 | 4609 | `			}` |
|       3 | 4610 | `		}` |
|       - | 4611 | `		/* Point to the next entry */` |
|       7 | 4612 | `		pEntry = pEntry->pPrev; /* Reverse link */` |
|       7 | 4613 | `		n--;` |
|       1 | 4614 | `	}` |
|       - | 4615 | `	/* Return the freshly created array */` |
|       3 | 4616 | `	ph7_result_value(pCtx,pArray);` |
|       3 | 4617 | `	return PH7_OK;` |
|       2 | 4618 |  |
|       - | 4619 | `/*` |
|       - | 4620 | ` * array array_fill(int $start_index,int $num,var $value)` |
|       - | 4621 | ` *  Fill an array with values.` |
|       - | 4622 | ` * Parameters` |
|       - | 4623 | ` *  $start_index` |
|       - | 4624 | ` *    The first index of the returned array.` |
|       - | 4625 | ` *  $num` |
|       - | 4626 | ` *   Number of elements to insert.` |
|       - | 4627 | ` *  $value` |
|       - | 4628 | ` *    Value to use for filling.` |
|       - | 4629 | ` * Return` |
|       - | 4630 | ` *  The filled array or null on failure.` |
|       - | 4631 | ` */` |
|     232 | 4632 | `static int ph7_hashmap_fill(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       2 | 4633 |  |
|       - | 4634 | `	ph7_value *pArray;` |
|       - | 4635 | `	int i,nEntry;` |
|       - | 4636 |  |
|       - | 4637 | `	/* PHP enforces argument count and type checks. */` |
|     234 | 4638 | `	if( nArg != 3 ){` |
|       - | 4639 | `		/* wrong number of arguments -> ArgumentCountError */` |
|       7 | 4640 | `		return PH7_VmThrowException(pCtx,` |
|       - | 4641 | `			"ArgumentCountError",` |
|       - | 4642 | `			"array_fill() expects exactly 3 arguments, %d given",` |
|       2 | 4643 | `			nArg` |
|       - | 4644 | `			);` |
|       - | 4645 | `	}` |
|       - | 4646 |  |
|       - | 4647 | `	/* Argument #1: start index must be convertible to int. */` |
|     454 | 4648 | `	if( ph7_value_is_array(apArg[0]) \|\| ph7_value_is_object(apArg[0]) \|\|` |
|     341 | 4649 | `		ph7_value_is_resource(apArg[0]) \|\| ph7_value_is_null(apArg[0]) ){` |
|       4 | 4650 | `		return PH7_VmThrowException(pCtx,` |
|       - | 4651 | `			"TypeError",` |
|       - | 4652 | `			"array_fill(): Argument #1 ($start_index) must be of type int, %s given",` |
|       1 | 4653 | `			ph7_type_name(apArg[0])` |
|       - | 4654 | `			);` |
|       - | 4655 | `	}` |
|     228 | 4656 | `	if( ph7_value_is_string(apArg[0]) ){` |
|       - | 4657 | `		int len;` |
|       6 | 4658 | `		sxu8 bReal = FALSE;` |
|       6 | 4659 | `		const char *zStr = ph7_value_to_string(apArg[0], &len);` |
|       6 | 4660 | `		if( SyStrIsNumeric(zStr, len, &bReal, 0) != SXRET_OK \|\| bReal ){` |
|       3 | 4661 | `			return PH7_VmThrowException(pCtx,` |
|       - | 4662 | `				"TypeError",` |
|       - | 4663 | `				"array_fill(): Argument #1 ($start_index) must be of type int, string given"` |
|       - | 4664 | `				);` |
|       - | 4665 | `		}` |
|       1 | 4666 | `	}` |
|       - | 4667 |  |
|       - | 4668 | `	/* Argument #2: count must be convertible to non-negative int. */` |
|     448 | 4669 | `	if( ph7_value_is_array(apArg[1]) \|\| ph7_value_is_object(apArg[1]) \|\|` |
|     450 | 4670 | `		ph7_value_is_resource(apArg[1]) \|\| ph7_value_is_null(apArg[1]) \|\|` |
|     224 | 4671 | `		ph7_value_is_bool(apArg[1]) ){` |
|       4 | 4672 | `		return PH7_VmThrowException(pCtx,` |
|       - | 4673 | `			"TypeError",` |
|       - | 4674 | `			"array_fill(): Argument #2 ($count) must be of type int, %s given",` |
|       2 | 4675 | `			ph7_type_name(apArg[1])` |
|       - | 4676 | `			);` |
|       - | 4677 | `	}` |
|     224 | 4678 | `	if( ph7_value_is_string(apArg[1]) ){` |
|       - | 4679 | `		int len;` |
|       3 | 4680 | `		sxu8 bReal = FALSE;` |
|       3 | 4681 | `		const char *zStr = ph7_value_to_string(apArg[1], &len);` |
|       3 | 4682 | `		if( SyStrIsNumeric(zStr, len, &bReal, 0) != SXRET_OK \|\| bReal ){` |
|       3 | 4683 | `			return PH7_VmThrowException(pCtx,` |
|       - | 4684 | `				"TypeError",` |
|       - | 4685 | `				"array_fill(): Argument #2 ($count) must be of type int, string given"` |
|       - | 4686 | `				);` |
|       - | 4687 | `		}` |
|     ! 0 | 4688 | `	}` |
|       - | 4689 | `	/* Floats with a fractional component are rejected like PHP warns.` |
|       - | 4690 | `	 * We treat them as TypeError for stricter compliance. */` |
|     222 | 4691 | `	if( ph7_value_is_float(apArg[1]) ){` |
|       3 | 4692 | `		double d = ph7_value_to_double(apArg[1]);` |
|       3 | 4693 | `		sxi64 iv = (sxi64)d;` |
|       3 | 4694 | `		if( d != (double)iv ){` |
|       3 | 4695 | `			return PH7_VmThrowException(pCtx,` |
|       - | 4696 | `				"TypeError",` |
|       - | 4697 | `				"array_fill(): Argument #2 ($count) must be of type int, float given"` |
|       - | 4698 | `				);` |
|       - | 4699 | `		}` |
|     ! 0 | 4700 | `	}` |
|       - | 4701 |  |
|       - | 4702 | `	/* Total number of entries to insert */` |
|     220 | 4703 | `	nEntry = ph7_value_to_int(apArg[1]);` |
|       - | 4704 | `	/* Reject negative counts with a ValueError like PHP. */` |
|     220 | 4705 | `	if( nEntry < 0 ){` |
|       3 | 4706 | `		return PH7_VmThrowException(pCtx,` |
|       - | 4707 | `			"ValueError",` |
|       - | 4708 | `			"array_fill(): Argument #2 ($count) must be greater than or equal to 0"` |
|       - | 4709 | `			);` |
|       - | 4710 | `	}` |
|       - | 4711 |  |
|       - | 4712 | `	/* If zero elements were requested, return an empty array without allocating */` |
|     217 | 4713 | `	if( nEntry == 0 ){` |
|       3 | 4714 | `		ph7_result_value(pCtx, ph7_context_new_array(pCtx));` |
|       3 | 4715 | `		return PH7_OK;` |
|       - | 4716 | `	}` |
|       - | 4717 |  |
|       - | 4718 | `	/* Create a new array */` |
|     215 | 4719 | `	pArray = ph7_context_new_array(pCtx);` |
|     215 | 4720 | `	if( pArray == 0 ){` |
|     ! 0 | 4721 | `		ph7_result_null(pCtx);` |
|     ! 0 | 4722 | `		return PH7_OK;` |
|       - | 4723 | `	}` |
|       - | 4724 |  |
|       - | 4725 | `	/* Insert the first entry alone because it has its own key */` |
|     215 | 4726 | `	ph7_array_add_intkey_elem(pArray, ph7_value_to_int(apArg[0]), apArg[2]);` |
|       - | 4727 | `	/* Repeat insertion of the desired value */` |
| 2117573 | 4728 | `	for( i = 1 ; i < nEntry ; i++ ){` |
| 2117359 | 4729 | `		ph7_array_add_elem(pArray, 0/*Automatic index assign */, apArg[2]);` |
| 1058680 | 4730 | `	}` |
|       - | 4731 | `	/* Return the filled array */` |
|     215 | 4732 | `	ph7_result_value(pCtx, pArray);` |
|     215 | 4733 | `	return PH7_OK;` |
|     118 | 4734 |  |
|       - | 4735 | `/*` |
|       - | 4736 | ` * array array_fill_keys(array $input,var $value)` |
|       - | 4737 | ` *  Fill an array with values, specifying keys.` |
|       - | 4738 | ` * Parameters` |
|       - | 4739 | ` *  $input` |
|       - | 4740 | ` *   Array of values that will be used as key.` |
|       - | 4741 | ` *  $value` |
|       - | 4742 | ` *    Value to use for filling.` |
|       - | 4743 | ` * Return` |
|       - | 4744 | ` *  The filled array or null on failure.` |
|       - | 4745 | ` */` |
|       2 | 4746 | `static int ph7_hashmap_fill_keys(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       1 | 4747 |  |
|       - | 4748 | `	ph7_hashmap_node *pEntry;` |
|       - | 4749 | `	ph7_hashmap *pSrc;` |
|       - | 4750 | `	ph7_value *pArray;` |
|       - | 4751 | `	sxu32 n;` |
|       3 | 4752 | `	if( nArg < 2 ){` |
|       - | 4753 | `		/* Missing arguments,return NULL */` |
|     ! 0 | 4754 | `		ph7_result_null(pCtx);` |
|     ! 0 | 4755 | `		return PH7_OK;` |
|       - | 4756 | `	}` |
|       - | 4757 | `	/* Make sure we are dealing with a valid hashmap */` |
|       3 | 4758 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|       - | 4759 | `		/* Invalid argument,return NULL */` |
|     ! 0 | 4760 | `		ph7_result_null(pCtx);` |
|     ! 0 | 4761 | `		return PH7_OK;` |
|       - | 4762 | `	}` |
|       - | 4763 | `	/* Point to the internal representation of the input hashmap */` |
|       3 | 4764 | `	pSrc = (ph7_hashmap *)apArg[0]->x.pOther;` |
|       - | 4765 | `	/* Create a new array */` |
|       3 | 4766 | `	pArray = ph7_context_new_array(pCtx);` |
|       3 | 4767 | `	if( pArray == 0 ){` |
|     ! 0 | 4768 | `		ph7_result_null(pCtx);` |
|     ! 0 | 4769 | `		return PH7_OK;` |
|       - | 4770 | `	}` |
|       - | 4771 | `	/* Perform the requested operation */` |
|       3 | 4772 | `	pEntry = pSrc->pFirst;` |
|       7 | 4773 | `	for( n = 0 ; n < pSrc->nEntry ; n++ ){` |
|       5 | 4774 | `		ph7_array_add_elem(pArray,HashmapExtractNodeValue(pEntry),apArg[1]);` |
|       - | 4775 | `		/* Point to the next entry */` |
|       5 | 4776 | `		pEntry = pEntry->pPrev; /* Reverse link */` |
|       3 | 4777 | `	}` |
|       - | 4778 | `	/* Return the filled array */` |
|       3 | 4779 | `	ph7_result_value(pCtx,pArray);` |
|       3 | 4780 | `	return PH7_OK;` |
|       2 | 4781 |  |
|       - | 4782 | `/*` |
|       - | 4783 | ` * array array_combine(array $keys,array $values)` |
|       - | 4784 | ` *  Creates an array by using one array for keys and another for its values.` |
|       - | 4785 | ` * Parameters` |
|       - | 4786 | ` *  $keys` |
|       - | 4787 | ` *    Array of keys to be used.` |
|       - | 4788 | ` * $values` |
|       - | 4789 | ` *   Array of values to be used.` |
|       - | 4790 | ` * Return` |
|       - | 4791 | ` *  Returns the combined array. Otherwise FALSE if the number of elements` |
|       - | 4792 | ` *  for each array isn't equal or if one of the given arguments is` |
|       - | 4793 | ` *  not an array.` |
|       - | 4794 | ` */` |
|      18 | 4795 | `static int ph7_hashmap_combine(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       2 | 4796 |  |
|       - | 4797 | `	ph7_hashmap_node *pKe,*pVe;` |
|       - | 4798 | `	ph7_hashmap *pKey,*pValue;` |
|       - | 4799 | `	ph7_value *pArray;` |
|       - | 4800 | `	sxu32 n;` |
|       - | 4801 | `	/* PHP enforces argument count and type checks. */` |
|      20 | 4802 | `	if( nArg != 2 ){` |
|       - | 4803 | `		/* wrong number of arguments -> ArgumentCountError */` |
|       4 | 4804 | `		return PH7_VmThrowException(pCtx,` |
|       - | 4805 | `			"ArgumentCountError",` |
|       - | 4806 | `			"array_combine() expects exactly 2 arguments, %d given",` |
|       1 | 4807 | `			nArg` |
|       - | 4808 | `			);` |
|       - | 4809 | `	}` |
|       - | 4810 | `	/* Validate argument types individually so we can report the correct` |
|       - | 4811 | `	 * argument index in the error message. */` |
|      18 | 4812 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|       4 | 4813 | `		return PH7_VmThrowException(pCtx,` |
|       - | 4814 | `			"TypeError",` |
|       - | 4815 | `			"array_combine(): Argument #1 ($keys) must be of type array, %s given",` |
|       1 | 4816 | `			ph7_type_name(apArg[0])` |
|       - | 4817 | `			);` |
|       - | 4818 | `	}` |
|      16 | 4819 | `	if( !ph7_value_is_array(apArg[1]) ){` |
|       4 | 4820 | `		return PH7_VmThrowException(pCtx,` |
|       - | 4821 | `			"TypeError",` |
|       - | 4822 | `			"array_combine(): Argument #2 ($values) must be of type array, %s given",` |
|       2 | 4823 | `			ph7_type_name(apArg[1])` |
|       - | 4824 | `			);` |
|       - | 4825 | `	}` |
|       - | 4826 | `	/* Point to the internal representation of the input hashmaps */` |
|      14 | 4827 | `	pKey   = (ph7_hashmap *)apArg[0]->x.pOther;` |
|      14 | 4828 | `	pValue = (ph7_hashmap *)apArg[1]->x.pOther;` |
|      14 | 4829 | `	if( pKey->nEntry != pValue->nEntry ){` |
|       - | 4830 | `		/* Length mismatch -> ValueError */` |
|       3 | 4831 | `		return PH7_VmThrowException(pCtx,` |
|       - | 4832 | `			"ValueError",` |
|       - | 4833 | `			"array_combine(): Argument #1 ($keys) and argument #2 ($values) must have the same number of elements"` |
|       - | 4834 | `			);` |
|       - | 4835 | `	}` |
|       - | 4836 | `	/* Create a new array */` |
|      11 | 4837 | `	pArray = ph7_context_new_array(pCtx);` |
|      11 | 4838 | `	if( pArray == 0 ){` |
|     ! 0 | 4839 | `		ph7_result_bool(pCtx,0);` |
|     ! 0 | 4840 | `		return PH7_OK;` |
|       - | 4841 | `	}` |
|       - | 4842 | `	/* Perform the requested operation */` |
|      11 | 4843 | `	pKe = pKey->pFirst;` |
|      11 | 4844 | `	pVe = pValue->pFirst;` |
|      33 | 4845 | `	for( n = 0 ; n < pKey->nEntry ; n++ ){` |
|      23 | 4846 | `		ph7_value *pKeyVal = HashmapExtractNodeValue(pKe);` |
|      23 | 4847 | `		ph7_value *pValVal = HashmapExtractNodeValue(pVe);` |
|       - | 4848 | `		/* PHP treats floats used as keys in array_combine differently than` |
|       - | 4849 | `		 * ordinary offset access: the float is stringified rather than` |
|       - | 4850 | `		 * truncated.  To emulate this behavior we create a temporary copy of` |
|       - | 4851 | `		 * the value when it is a float and convert the copy to string.  The` |
|       - | 4852 | `		 * original array must not be mutated. */` |
|      23 | 4853 | `		ph7_value *pKeyCopy = pKeyVal;` |
|      23 | 4854 | `		if( ph7_value_is_float(pKeyVal) ){` |
|       5 | 4855 | `			ph7_value *pTmpKey = ph7_context_new_scalar(pCtx);` |
|       5 | 4856 | `			if( pTmpKey ){` |
|       5 | 4857 | `				PH7_MemObjStore(pKeyVal,pTmpKey);` |
|       - | 4858 | `				/* Convert copy to string so it becomes "1.5" or "2" etc. */` |
|       5 | 4859 | `				PH7_MemObjToString(pTmpKey);` |
|       5 | 4860 | `				pKeyCopy = pTmpKey;` |
|       2 | 4861 | `			}` |
|       2 | 4862 | `		}` |
|      23 | 4863 | `		ph7_array_add_elem(pArray,pKeyCopy,pValVal);` |
|       - | 4864 | `		/* Point to the next entry */` |
|      23 | 4865 | `		pKe = pKe->pPrev; /* Reverse link */` |
|      23 | 4866 | `		pVe = pVe->pPrev;` |
|      12 | 4867 | `	}` |
|       - | 4868 | `	/* Return the filled array */` |
|      11 | 4869 | `	ph7_result_value(pCtx,pArray);` |
|      11 | 4870 | `	return PH7_OK;` |
|      11 | 4871 |  |
|       - | 4872 | `/*` |
|       - | 4873 | ` * array array_reverse(array $array [,bool $preserve_keys = false ])` |
|       - | 4874 | ` *  Return an array with elements in reverse order.` |
|       - | 4875 | ` * Parameters` |
|       - | 4876 | ` *  $array` |
|       - | 4877 | ` *   The input array.` |
|       - | 4878 | ` *  $preserve_keys (optional)` |
|       - | 4879 | ` *   If set to TRUE keys are preserved.` |
|       - | 4880 | ` * Return` |
|       - | 4881 | ` *  The reversed array.` |
|       - | 4882 | ` */` |
|       6 | 4883 | `static int ph7_hashmap_reverse(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       1 | 4884 |  |
|       - | 4885 | `	ph7_hashmap_node *pEntry;` |
|       - | 4886 | `	ph7_hashmap *pSrc;` |
|       - | 4887 | `	ph7_value *pArray;` |
|       - | 4888 | `	int bPreserve;` |
|       - | 4889 | `	sxu32 n;` |
|       7 | 4890 | `	if( nArg < 1 ){` |
|       - | 4891 | `		/* Missing arguments,return NULL */` |
|     ! 0 | 4892 | `		ph7_result_null(pCtx);` |
|     ! 0 | 4893 | `		return PH7_OK;` |
|       - | 4894 | `	}` |
|       - | 4895 | `	/* Make sure we are dealing with a valid hashmap */` |
|       7 | 4896 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|       - | 4897 | `		/* Invalid argument,return NULL */` |
|     ! 0 | 4898 | `		ph7_result_null(pCtx);` |
|     ! 0 | 4899 | `		return PH7_OK;` |
|       - | 4900 | `	}` |
|       7 | 4901 | `	bPreserve = FALSE;` |
|       7 | 4902 | `	if( nArg > 1 && ph7_value_is_bool(apArg[1]) ){` |
|       3 | 4903 | `		bPreserve = ph7_value_to_bool(apArg[1]);` |
|       1 | 4904 | `	}` |
|       - | 4905 | `	/* Point to the internal representation of the input hashmap */` |
|       7 | 4906 | `	pSrc = (ph7_hashmap *)apArg[0]->x.pOther;` |
|       - | 4907 | `	/* Create a new array */` |
|       7 | 4908 | `	pArray = ph7_context_new_array(pCtx);` |
|       7 | 4909 | `	if( pArray == 0 ){` |
|     ! 0 | 4910 | `		ph7_result_null(pCtx);` |
|     ! 0 | 4911 | `		return PH7_OK;` |
|       - | 4912 | `	}` |
|       - | 4913 | `	/* Perform the requested operation */` |
|       7 | 4914 | `	pEntry = pSrc->pLast;` |
|      23 | 4915 | `	for( n = 0 ; n < pSrc->nEntry ; n++ ){` |
|      17 | 4916 | `		HashmapInsertNode((ph7_hashmap *)pArray->x.pOther,pEntry,bPreserve);` |
|       - | 4917 | `		/* Point to the previous entry */` |
|      17 | 4918 | `		pEntry = pEntry->pNext; /* Reverse link */` |
|       9 | 4919 | `	}` |
|       7 | 4920 | `	ph7_result_value(pCtx,pArray);` |
|       7 | 4921 | `	return PH7_OK;` |
|       4 | 4922 |  |
|       - | 4923 | `/*` |
|       - | 4924 | ` * array array_unique(array $array[,int $sort_flags = SORT_STRING ])` |
|       - | 4925 | ` *  Removes duplicate values from an array` |
|       - | 4926 | ` * Parameter` |
|       - | 4927 | ` *  $array` |
|       - | 4928 | ` *   The input array.` |
|       - | 4929 | ` *  $sort_flags` |
|       - | 4930 | ` *    The optional second parameter sort_flags may be used to modify the sorting behavior using these values:` |
|       - | 4931 | ` *    Sorting type flags:` |
|       - | 4932 | ` *       SORT_REGULAR - compare items normally (don't change types)` |
|       - | 4933 | ` *       SORT_NUMERIC - compare items numerically` |
|       - | 4934 | ` *       SORT_STRING - compare items as strings` |
|       - | 4935 | ` *       SORT_LOCALE_STRING - compare items as` |
|       - | 4936 | ` * Return` |
|       - | 4937 | ` *  Filtered array or NULL on failure.` |
|       - | 4938 | ` */` |
|       2 | 4939 | `static int ph7_hashmap_unique(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       1 | 4940 |  |
|       - | 4941 | `	ph7_hashmap_node *pEntry;` |
|       - | 4942 | `	ph7_value *pNeedle;` |
|       - | 4943 | `	ph7_hashmap *pSrc;` |
|       - | 4944 | `	ph7_value *pArray;` |
|       - | 4945 | `	int bStrict;` |
|       - | 4946 | `	sxi32 rc;` |
|       - | 4947 | `	sxu32 n;` |
|       3 | 4948 | `	if( nArg < 1 ){` |
|       - | 4949 | `		/* Missing arguments,return NULL */` |
|     ! 0 | 4950 | `		ph7_result_null(pCtx);` |
|     ! 0 | 4951 | `		return PH7_OK;` |
|       - | 4952 | `	}` |
|       - | 4953 | `	/* Make sure we are dealing with a valid hashmap */` |
|       3 | 4954 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|       - | 4955 | `		/* Invalid argument,return NULL */` |
|     ! 0 | 4956 | `		ph7_result_null(pCtx);` |
|     ! 0 | 4957 | `		return PH7_OK;` |
|       - | 4958 | `	}` |
|       3 | 4959 | `	bStrict = FALSE;` |
|       3 | 4960 | `	if( nArg > 1 ){` |
|     ! 0 | 4961 | `		bStrict = ph7_value_to_int(apArg[1]) == 3 /* SORT_REGULAR */ ? 1 : 0;` |
|     ! 0 | 4962 | `	}` |
|       - | 4963 | `	/* Point to the internal representation of the input hashmap */` |
|       3 | 4964 | `	pSrc = (ph7_hashmap *)apArg[0]->x.pOther;` |
|       - | 4965 | `	/* Create a new array */` |
|       3 | 4966 | `	pArray = ph7_context_new_array(pCtx);` |
|       3 | 4967 | `	if( pArray == 0 ){` |
|     ! 0 | 4968 | `		ph7_result_null(pCtx);` |
|     ! 0 | 4969 | `		return PH7_OK;` |
|       - | 4970 | `	}` |
|       - | 4971 | `	/* Perform the requested operation */` |
|       3 | 4972 | `	pEntry = pSrc->pFirst;` |
|      13 | 4973 | `	for( n = 0 ; n < pSrc->nEntry ; n++ ){` |
|      11 | 4974 | `		pNeedle = HashmapExtractNodeValue(pEntry);` |
|      11 | 4975 | `		rc = SXERR_NOTFOUND;` |
|      11 | 4976 | `		if( pNeedle ){` |
|      11 | 4977 | `			rc = HashmapFindValue((ph7_hashmap *)pArray->x.pOther,pNeedle,0,bStrict);` |
|       5 | 4978 | `		}` |
|      11 | 4979 | `		if( rc != SXRET_OK ){` |
|       - | 4980 | `			/* Perform the insertion */` |
|       7 | 4981 | `			HashmapInsertNode((ph7_hashmap *)pArray->x.pOther,pEntry,TRUE);` |
|       3 | 4982 | `		}` |
|       - | 4983 | `		/* Point to the next entry */` |
|      11 | 4984 | `		pEntry = pEntry->pPrev; /* Reverse link */` |
|       6 | 4985 | `	}` |
|       - | 4986 | `	/* Return the freshly created array */` |
|       3 | 4987 | `	ph7_result_value(pCtx,pArray);` |
|       3 | 4988 | `	return PH7_OK;` |
|       2 | 4989 |  |
|       - | 4990 | `/*` |
|       - | 4991 | ` * array array_flip(array $input)` |
|       - | 4992 | ` *  Exchanges all keys with their associated values in an array.` |
|       - | 4993 | ` * Parameter` |
|       - | 4994 | ` *  $input` |
|       - | 4995 | ` *   Input array.` |
|       - | 4996 | ` * Return` |
|       - | 4997 | ` *   The flipped array on success or NULL on failure.` |
|       - | 4998 | ` */` |
|      28 | 4999 | `static int ph7_hashmap_flip(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       1 | 5000 |  |
|       - | 5001 | `	ph7_hashmap_node *pEntry;` |
|       - | 5002 | `	ph7_hashmap *pSrc;` |
|       - | 5003 | `	ph7_value *pArray;` |
|       - | 5004 | `	ph7_value *pKey;` |
|       - | 5005 | `	ph7_value sVal;` |
|       - | 5006 | `	sxu32 n;` |
|      29 | 5007 | `	if( nArg < 1 ){` |
|       - | 5008 | `		/* Missing arguments,return NULL */` |
|     ! 0 | 5009 | `		ph7_result_null(pCtx);` |
|     ! 0 | 5010 | `		return PH7_OK;` |
|       - | 5011 | `	}` |
|       - | 5012 | `	/* Make sure we are dealing with a valid hashmap */` |
|      29 | 5013 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|       - | 5014 | `		/* Invalid argument,return NULL */` |
|       5 | 5015 | `		ph7_result_null(pCtx);` |
|       5 | 5016 | `		return PH7_OK;` |
|       - | 5017 | `	}` |
|       - | 5018 | `	/* Point to the internal representation of the input hashmap */` |
|      25 | 5019 | `	pSrc = (ph7_hashmap *)apArg[0]->x.pOther;` |
|       - | 5020 | `	/* Create a new array */` |
|      25 | 5021 | `	pArray = ph7_context_new_array(pCtx);` |
|      25 | 5022 | `	if( pArray == 0 ){` |
|     ! 0 | 5023 | `		ph7_result_null(pCtx);` |
|     ! 0 | 5024 | `		return PH7_OK;` |
|       - | 5025 | `	}` |
|       - | 5026 | `	/* Start processing */` |
|      25 | 5027 | `	pEntry = pSrc->pFirst;` |
|   22259 | 5028 | `	for( n = 0 ; n < pSrc->nEntry ; n++ ){` |
|       - | 5029 | `		/* Extract the node value */` |
|   22235 | 5030 | `		pKey = HashmapExtractNodeValue(pEntry);` |
|   22235 | 5031 | `		if( pKey && (pKey->iFlags & MEMOBJ_NULL) == 0){` |
|       - | 5032 | `			/* Prepare the value for insertion */` |
|   22233 | 5033 | `			if( pEntry->iType == HASHMAP_INT_NODE ){` |
|   20001 | 5034 | `				PH7_MemObjInitFromInt(pSrc->pVm,&sVal,pEntry->xKey.iKey);` |
|   10001 | 5035 | `			}else{` |
|       - | 5036 | `				SyString sStr;` |
|    2233 | 5037 | `				SyStringInitFromBuf(&sStr,SyBlobData(&pEntry->xKey.sKey),SyBlobLength(&pEntry->xKey.sKey));` |
|    2233 | 5038 | `				PH7_MemObjInitFromString(pSrc->pVm,&sVal,&sStr);` |
|       - | 5039 | `			}` |
|       - | 5040 | `			/* Perform the insertion */` |
|   22233 | 5041 | `			ph7_array_add_elem(pArray,pKey,&sVal);` |
|       - | 5042 | `			/* Safely release the value because each inserted entry` |
|       - | 5043 | `			 * have it's own private copy of the value.` |
|       - | 5044 | `			 */` |
|   22233 | 5045 | `			PH7_MemObjRelease(&sVal);` |
|   11116 | 5046 | `		}` |
|       - | 5047 | `		/* Point to the next entry */` |
|   22235 | 5048 | `		pEntry = pEntry->pPrev; /* Reverse link */` |
|   11118 | 5049 | `	}` |
|       - | 5050 | `	/* Return the freshly created array */` |
|      25 | 5051 | `	ph7_result_value(pCtx,pArray);` |
|      25 | 5052 | `	return PH7_OK;` |
|      15 | 5053 |  |
|       - | 5054 | `/*` |
|       - | 5055 | ` * number array_sum(array $array )` |
|       - | 5056 | ` *  Calculate the sum of values in an array.` |
|       - | 5057 | ` * Parameters` |
|       - | 5058 | ` *  $array: The input array.` |
|       - | 5059 | ` * Return` |
|       - | 5060 | ` *  Returns the sum of values as an integer or float.` |
|       - | 5061 | ` */` |
|       4 | 5062 | `static void DoubleSum(ph7_context *pCtx,ph7_hashmap *pMap)` |
|       1 | 5063 |  |
|       - | 5064 | `	ph7_hashmap_node *pEntry;` |
|       - | 5065 | `	ph7_value *pObj;` |
|       5 | 5066 | `	double dSum = 0;` |
|       - | 5067 | `	sxu32 n;` |
|       5 | 5068 | `	pEntry = pMap->pFirst;` |
|      19 | 5069 | `	for( n = 0 ; n < pMap->nEntry ; n++ ){` |
|      15 | 5070 | `		pObj = HashmapExtractNodeValue(pEntry);` |
|      15 | 5071 | `		if( pObj && (pObj->iFlags & (MEMOBJ_NULL\|MEMOBJ_HASHMAP\|MEMOBJ_OBJ\|MEMOBJ_RES)) == 0){` |
|      15 | 5072 | `			if( pObj->iFlags & MEMOBJ_REAL ){` |
|      15 | 5073 | `				dSum += pObj->rVal;` |
|       7 | 5074 | `			}else if( pObj->iFlags & (MEMOBJ_INT\|MEMOBJ_BOOL) ){` |
|     ! 0 | 5075 | `				dSum += (double)pObj->x.iVal;` |
|     ! 0 | 5076 | `			}else if( pObj->iFlags & MEMOBJ_STRING ){` |
|     ! 0 | 5077 | `				if( SyBlobLength(&pObj->sBlob) > 0 ){` |
|     ! 0 | 5078 | `					double dv = 0;` |
|     ! 0 | 5079 | `					SyStrToReal((const char *)SyBlobData(&pObj->sBlob),SyBlobLength(&pObj->sBlob),(void *)&dv,0);` |
|     ! 0 | 5080 | `					dSum += dv;` |
|     ! 0 | 5081 | `				}` |
|     ! 0 | 5082 | `			}` |
|       7 | 5083 | `		}` |
|       - | 5084 | `		/* Point to the next entry */` |
|      15 | 5085 | `		pEntry = pEntry->pPrev; /* Reverse link */` |
|       8 | 5086 | `	}` |
|       - | 5087 | `	/* Return sum */` |
|       5 | 5088 | `	ph7_result_double(pCtx,dSum);` |
|       5 | 5089 |  |
|       6 | 5090 | `static void Int64Sum(ph7_context *pCtx,ph7_hashmap *pMap)` |
|       2 | 5091 |  |
|       - | 5092 | `	ph7_hashmap_node *pEntry;` |
|       - | 5093 | `	ph7_value *pObj;` |
|       8 | 5094 | `	sxi64 nSum = 0;` |
|       - | 5095 | `	sxu32 n;` |
|       8 | 5096 | `	pEntry = pMap->pFirst;` |
|      34 | 5097 | `	for( n = 0 ; n < pMap->nEntry ; n++ ){` |
|      28 | 5098 | `		pObj = HashmapExtractNodeValue(pEntry);` |
|      28 | 5099 | `		if( pObj && (pObj->iFlags & (MEMOBJ_NULL\|MEMOBJ_HASHMAP\|MEMOBJ_OBJ\|MEMOBJ_RES)) == 0){` |
|      28 | 5100 | `			if( pObj->iFlags & MEMOBJ_REAL ){` |
|     ! 0 | 5101 | `				nSum += (sxi64)pObj->rVal;` |
|      28 | 5102 | `			}else if( pObj->iFlags & (MEMOBJ_INT\|MEMOBJ_BOOL) ){` |
|      28 | 5103 | `				nSum += pObj->x.iVal;` |
|      13 | 5104 | `			}else if( pObj->iFlags & MEMOBJ_STRING ){` |
|     ! 0 | 5105 | `				if( SyBlobLength(&pObj->sBlob) > 0 ){` |
|     ! 0 | 5106 | `					sxi64 nv = 0;` |
|     ! 0 | 5107 | `					SyStrToInt64((const char *)SyBlobData(&pObj->sBlob),SyBlobLength(&pObj->sBlob),(void *)&nv,0);` |
|     ! 0 | 5108 | `					nSum += nv;` |
|     ! 0 | 5109 | `				}` |
|     ! 0 | 5110 | `			}` |
|      13 | 5111 | `		}` |
|       - | 5112 | `		/* Point to the next entry */` |
|      28 | 5113 | `		pEntry = pEntry->pPrev; /* Reverse link */` |
|      15 | 5114 | `	}` |
|       - | 5115 | `	/* Return sum */` |
|       8 | 5116 | `	ph7_result_int64(pCtx,nSum);` |
|       8 | 5117 |  |
|       - | 5118 | `/* number array_sum(array $array )` |
|       - | 5119 | ` * (See block-coment above)` |
|       - | 5120 | ` */` |
|      16 | 5121 | `static int ph7_hashmap_sum(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       2 | 5122 |  |
|       - | 5123 | `	ph7_hashmap *pMap;` |
|       - | 5124 | `	ph7_value *pObj;` |
|      18 | 5125 | `	if( nArg < 1 ){` |
|       - | 5126 | `		/* Missing arguments,return 0 */` |
|       3 | 5127 | `		ph7_result_int(pCtx,0);` |
|       3 | 5128 | `		return PH7_OK;` |
|       - | 5129 | `	}` |
|       - | 5130 | `	/* Make sure we are dealing with a valid hashmap */` |
|      16 | 5131 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|       - | 5132 | `		/* Invalid argument,return 0 */` |
|       5 | 5133 | `		ph7_result_int(pCtx,0);` |
|       5 | 5134 | `		return PH7_OK;` |
|       - | 5135 | `	}` |
|      12 | 5136 | `	pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|      12 | 5137 | `	if( pMap->nEntry < 1 ){` |
|       - | 5138 | `		/* Nothing to compute,return 0 */` |
|     ! 0 | 5139 | `		ph7_result_int(pCtx,0);` |
|     ! 0 | 5140 | `		return PH7_OK;` |
|       - | 5141 | `	}` |
|       - | 5142 | `	/* If the first element is of type float,then perform floating` |
|       - | 5143 | `	 * point computaion.Otherwise switch to int64 computaion.` |
|       - | 5144 | `	 */` |
|      12 | 5145 | `	pObj = HashmapExtractNodeValue(pMap->pFirst);` |
|      12 | 5146 | `	if( pObj == 0 ){` |
|     ! 0 | 5147 | `		ph7_result_int(pCtx,0);` |
|     ! 0 | 5148 | `		return PH7_OK;` |
|       - | 5149 | `	}` |
|      12 | 5150 | `	if( pObj->iFlags & MEMOBJ_REAL ){` |
|       5 | 5151 | `		DoubleSum(pCtx,pMap);` |
|       3 | 5152 | `	}else{` |
|       8 | 5153 | `		Int64Sum(pCtx,pMap);` |
|       - | 5154 | `	}` |
|      12 | 5155 | `	return PH7_OK;` |
|      10 | 5156 |  |
|       - | 5157 | `/*` |
|       - | 5158 | ` * number array_product(array $array )` |
|       - | 5159 | ` *  Calculate the product of values in an array.` |
|       - | 5160 | ` * Parameters` |
|       - | 5161 | ` *  $array: The input array.` |
|       - | 5162 | ` * Return` |
|       - | 5163 | ` *  Returns the product of values as an integer or float.` |
|       - | 5164 | ` */` |
|     ! 0 | 5165 | `static void DoubleProd(ph7_context *pCtx,ph7_hashmap *pMap)` |
|     ! 0 | 5166 |  |
|       - | 5167 | `	ph7_hashmap_node *pEntry;` |
|       - | 5168 | `	ph7_value *pObj;` |
|       - | 5169 | `	double dProd;` |
|       - | 5170 | `	sxu32 n;` |
|     ! 0 | 5171 | `	pEntry = pMap->pFirst;` |
|     ! 0 | 5172 | `	dProd = 1;` |
|     ! 0 | 5173 | `	for( n = 0 ; n < pMap->nEntry ; n++ ){` |
|     ! 0 | 5174 | `		pObj = HashmapExtractNodeValue(pEntry);` |
|     ! 0 | 5175 | `		if( pObj && (pObj->iFlags & (MEMOBJ_NULL\|MEMOBJ_HASHMAP\|MEMOBJ_OBJ\|MEMOBJ_RES)) == 0){` |
|     ! 0 | 5176 | `			if( pObj->iFlags & MEMOBJ_REAL ){` |
|     ! 0 | 5177 | `				dProd *= pObj->rVal;` |
|     ! 0 | 5178 | `			}else if( pObj->iFlags & (MEMOBJ_INT\|MEMOBJ_BOOL) ){` |
|     ! 0 | 5179 | `				dProd *= (double)pObj->x.iVal;` |
|     ! 0 | 5180 | `			}else if( pObj->iFlags & MEMOBJ_STRING ){` |
|     ! 0 | 5181 | `				if( SyBlobLength(&pObj->sBlob) > 0 ){` |
|     ! 0 | 5182 | `					double dv = 0;` |
|     ! 0 | 5183 | `					SyStrToReal((const char *)SyBlobData(&pObj->sBlob),SyBlobLength(&pObj->sBlob),(void *)&dv,0);` |
|     ! 0 | 5184 | `					dProd *= dv;` |
|     ! 0 | 5185 | `				}` |
|     ! 0 | 5186 | `			}` |
|     ! 0 | 5187 | `		}` |
|       - | 5188 | `		/* Point to the next entry */` |
|     ! 0 | 5189 | `		pEntry = pEntry->pPrev; /* Reverse link */` |
|     ! 0 | 5190 | `	}` |
|       - | 5191 | `	/* Return product */` |
|     ! 0 | 5192 | `	ph7_result_double(pCtx,dProd);` |
|     ! 0 | 5193 |  |
|       2 | 5194 | `static void Int64Prod(ph7_context *pCtx,ph7_hashmap *pMap)` |
|       1 | 5195 |  |
|       - | 5196 | `	ph7_hashmap_node *pEntry;` |
|       - | 5197 | `	ph7_value *pObj;` |
|       - | 5198 | `	sxi64 nProd;` |
|       - | 5199 | `	sxu32 n;` |
|       3 | 5200 | `	pEntry = pMap->pFirst;` |
|       3 | 5201 | `	nProd = 1;` |
|       9 | 5202 | `	for( n = 0 ; n < pMap->nEntry ; n++ ){` |
|       7 | 5203 | `		pObj = HashmapExtractNodeValue(pEntry);` |
|       7 | 5204 | `		if( pObj && (pObj->iFlags & (MEMOBJ_NULL\|MEMOBJ_HASHMAP\|MEMOBJ_OBJ\|MEMOBJ_RES)) == 0){` |
|       7 | 5205 | `			if( pObj->iFlags & MEMOBJ_REAL ){` |
|     ! 0 | 5206 | `				nProd *= (sxi64)pObj->rVal;` |
|       7 | 5207 | `			}else if( pObj->iFlags & (MEMOBJ_INT\|MEMOBJ_BOOL) ){` |
|       7 | 5208 | `				nProd *= pObj->x.iVal;` |
|       3 | 5209 | `			}else if( pObj->iFlags & MEMOBJ_STRING ){` |
|     ! 0 | 5210 | `				if( SyBlobLength(&pObj->sBlob) > 0 ){` |
|     ! 0 | 5211 | `					sxi64 nv = 0;` |
|     ! 0 | 5212 | `					SyStrToInt64((const char *)SyBlobData(&pObj->sBlob),SyBlobLength(&pObj->sBlob),(void *)&nv,0);` |
|     ! 0 | 5213 | `					nProd *= nv;` |
|     ! 0 | 5214 | `				}` |
|     ! 0 | 5215 | `			}` |
|       3 | 5216 | `		}` |
|       - | 5217 | `		/* Point to the next entry */` |
|       7 | 5218 | `		pEntry = pEntry->pPrev; /* Reverse link */` |
|       4 | 5219 | `	}` |
|       - | 5220 | `	/* Return product */` |
|       3 | 5221 | `	ph7_result_int64(pCtx,nProd);` |
|       3 | 5222 |  |
|       - | 5223 | `/* number array_product(array $array )` |
|       - | 5224 | ` * (See block-block comment above)` |
|       - | 5225 | ` */` |
|       2 | 5226 | `static int ph7_hashmap_product(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       1 | 5227 |  |
|       - | 5228 | `	ph7_hashmap *pMap;` |
|       - | 5229 | `	ph7_value *pObj;` |
|       3 | 5230 | `	if( nArg < 1 ){` |
|       - | 5231 | `		/* Missing arguments,return 0 */` |
|     ! 0 | 5232 | `		ph7_result_int(pCtx,0);` |
|     ! 0 | 5233 | `		return PH7_OK;` |
|       - | 5234 | `	}` |
|       - | 5235 | `	/* Make sure we are dealing with a valid hashmap */` |
|       3 | 5236 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|       - | 5237 | `		/* Invalid argument,return 0 */` |
|     ! 0 | 5238 | `		ph7_result_int(pCtx,0);` |
|     ! 0 | 5239 | `		return PH7_OK;` |
|       - | 5240 | `	}` |
|       3 | 5241 | `	pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|       3 | 5242 | `	if( pMap->nEntry < 1 ){` |
|       - | 5243 | `		/* Nothing to compute,return 0 */` |
|     ! 0 | 5244 | `		ph7_result_int(pCtx,0);` |
|     ! 0 | 5245 | `		return PH7_OK;` |
|       - | 5246 | `	}` |
|       - | 5247 | `	/* If the first element is of type float,then perform floating` |
|       - | 5248 | `	 * point computaion.Otherwise switch to int64 computaion.` |
|       - | 5249 | `	 */` |
|       3 | 5250 | `	pObj = HashmapExtractNodeValue(pMap->pFirst);` |
|       3 | 5251 | `	if( pObj == 0 ){` |
|     ! 0 | 5252 | `		ph7_result_int(pCtx,0);` |
|     ! 0 | 5253 | `		return PH7_OK;` |
|       - | 5254 | `	}` |
|       3 | 5255 | `	if( pObj->iFlags & MEMOBJ_REAL ){` |
|     ! 0 | 5256 | `		DoubleProd(pCtx,pMap);` |
|     ! 0 | 5257 | `	}else{` |
|       3 | 5258 | `		Int64Prod(pCtx,pMap);` |
|       - | 5259 | `	}` |
|       3 | 5260 | `	return PH7_OK;` |
|       2 | 5261 |  |
|       - | 5262 | `/*` |
|       - | 5263 | ` * value array_rand(array $input[,int $num_req = 1 ])` |
|       - | 5264 | ` *  Pick one or more random entries out of an array.` |
|       - | 5265 | ` * Parameters` |
|       - | 5266 | ` * $input` |
|       - | 5267 | ` *  The input array.` |
|       - | 5268 | ` * $num_req` |
|       - | 5269 | ` *  Specifies how many entries you want to pick.` |
|       - | 5270 | ` * Return` |
|       - | 5271 | ` *  If you are picking only one entry, array_rand() returns the key for a random entry.` |
|       - | 5272 | ` *  Otherwise, it returns an array of keys for the random entries.` |
|       - | 5273 | ` *  NULL is returned on failure.` |
|       - | 5274 | ` */` |
|       6 | 5275 | `static int ph7_hashmap_rand(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       1 | 5276 |  |
|       - | 5277 | `	ph7_hashmap_node *pNode;` |
|       - | 5278 | `	ph7_hashmap *pMap;` |
|       7 | 5279 | `	int nItem = 1;` |
|       7 | 5280 | `	if( nArg < 1 ){` |
|       - | 5281 | `		/* Missing argument,return NULL */` |
|     ! 0 | 5282 | `		ph7_result_null(pCtx);` |
|     ! 0 | 5283 | `		return PH7_OK;` |
|       - | 5284 | `	}` |
|       - | 5285 | `	/* Make sure we are dealing with an array */` |
|       7 | 5286 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|     ! 0 | 5287 | `		ph7_result_null(pCtx);` |
|     ! 0 | 5288 | `		return PH7_OK;` |
|       - | 5289 | `	}` |
|       - | 5290 | `	/* Point to the internal representation of the input hashmap */` |
|       7 | 5291 | `	pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|       7 | 5292 | `	if(pMap->nEntry < 1 ){` |
|       - | 5293 | `		/* Empty hashmap,return NULL */` |
|     ! 0 | 5294 | `		ph7_result_null(pCtx);` |
|     ! 0 | 5295 | `		return PH7_OK;` |
|       - | 5296 | `	}` |
|       7 | 5297 | `	if( nArg > 1 ){` |
|       3 | 5298 | `		nItem = ph7_value_to_int(apArg[1]);` |
|       1 | 5299 | `	}` |
|       7 | 5300 | `	if( nItem < 2 ){` |
|       - | 5301 | `		sxu32 nEntry;` |
|       - | 5302 | `		/* Select a random number */` |
|       5 | 5303 | `		nEntry = PH7_VmRandomNum(pMap->pVm) % pMap->nEntry;` |
|       - | 5304 | `		/* Extract the desired entry.` |
|       - | 5305 | `		 * Note that we perform a linear lookup here (later version must change this)` |
|       - | 5306 | `		 */` |
|       5 | 5307 | `		if( nEntry > pMap->nEntry / 2 ){` |
|       4 | 5308 | `			pNode = pMap->pLast;` |
|       4 | 5309 | `			nEntry = pMap->nEntry - nEntry;` |
|       4 | 5310 | `			if( nEntry > 1 ){` |
|     ! 0 | 5311 | `				for(;;){` |
|     ! 0 | 5312 | `					if( nEntry == 0 ){` |
|     ! 0 | 5313 | `						break;` |
|       - | 5314 | `					}` |
|       - | 5315 | `					/* Point to the previous entry */` |
|     ! 0 | 5316 | `					pNode = pNode->pNext; /* Reverse link */` |
|     ! 0 | 5317 | `					nEntry--;` |
|     ! 0 | 5318 | `				}` |
|     ! 0 | 5319 | `			}` |
|       2 | 5320 | `		}else{` |
|       2 | 5321 | `			pNode = pMap->pFirst;` |
|       2 | 5322 | `			for(;;){` |
|       3 | 5323 | `				if( nEntry == 0 ){` |
|       2 | 5324 | `					break;` |
|       - | 5325 | `				}` |
|       - | 5326 | `				/* Point to the next entry */` |
|       1 | 5327 | `				pNode = pNode->pPrev; /* Reverse link */` |
|       1 | 5328 | `				nEntry--;` |
|     ! 0 | 5329 | `			}` |
|       - | 5330 | `		}` |
|       5 | 5331 | `		if( pNode->iType == HASHMAP_INT_NODE ){` |
|       - | 5332 | `			/* Int key */` |
|       3 | 5333 | `			ph7_result_int64(pCtx,pNode->xKey.iKey);` |
|       2 | 5334 | `		}else{` |
|       - | 5335 | `			/* Blob key */` |
|       3 | 5336 | `			ph7_result_string(pCtx,(const char *)SyBlobData(&pNode->xKey.sKey),(int)SyBlobLength(&pNode->xKey.sKey));` |
|       - | 5337 | `		}` |
|       3 | 5338 | `	}else{` |
|       - | 5339 | `		ph7_value sKey,*pArray;` |
|       - | 5340 | `		ph7_hashmap *pDest;` |
|       - | 5341 | `		/* Create a new array */` |
|       3 | 5342 | `		pArray = ph7_context_new_array(pCtx);` |
|       3 | 5343 | `		if( pArray == 0 ){` |
|     ! 0 | 5344 | `			ph7_result_null(pCtx);` |
|     ! 0 | 5345 | `			return PH7_OK;` |
|       - | 5346 | `		}` |
|       - | 5347 | `		/* Point to the internal representation of the hashmap */` |
|       3 | 5348 | `		pDest = (ph7_hashmap *)pArray->x.pOther;` |
|       3 | 5349 | `		PH7_MemObjInit(pDest->pVm,&sKey);` |
|       - | 5350 | `		/* Copy the first n items */` |
|       3 | 5351 | `		pNode = pMap->pFirst;` |
|       3 | 5352 | `		if( nItem > (int)pMap->nEntry ){` |
|     ! 0 | 5353 | `			nItem = (int)pMap->nEntry;` |
|     ! 0 | 5354 | `		}` |
|       7 | 5355 | `		while( nItem > 0){` |
|       5 | 5356 | `			PH7_HashmapExtractNodeKey(pNode,&sKey);` |
|       5 | 5357 | `			PH7_HashmapInsert(pDest,0/* Automatic index assign*/,&sKey);` |
|       5 | 5358 | `			PH7_MemObjRelease(&sKey);` |
|       - | 5359 | `			/* Point to the next entry */` |
|       5 | 5360 | `			pNode = pNode->pPrev; /* Reverse link */` |
|       5 | 5361 | `			nItem--;` |
|       1 | 5362 | `		}` |
|       - | 5363 | `		/* Shuffle the array */` |
|       3 | 5364 | `		HashmapMergeSort(pDest,HashmapCmpCallback7,0);` |
|       - | 5365 | `		/* Rehash node */` |
|       3 | 5366 | `		HashmapSortRehash(pDest);` |
|       - | 5367 | `		/* Return the random array */` |
|       3 | 5368 | `		ph7_result_value(pCtx,pArray);` |
|       - | 5369 | `	}` |
|       7 | 5370 | `	return PH7_OK;` |
|       4 | 5371 |  |
|       - | 5372 | `/*` |
|       - | 5373 | ` * array array_chunk (array $input,int $size [,bool $preserve_keys = false ])` |
|       - | 5374 | ` *  Split an array into chunks.` |
|       - | 5375 | ` * Parameters` |
|       - | 5376 | ` * $input` |
|       - | 5377 | ` *   The array to work on` |
|       - | 5378 | ` * $size` |
|       - | 5379 | ` *   The size of each chunk` |
|       - | 5380 | ` * $preserve_keys` |
|       - | 5381 | ` *   When set to TRUE keys will be preserved. Default is FALSE which will reindex` |
|       - | 5382 | ` *   the chunk numerically.` |
|       - | 5383 | ` * Return` |
|       - | 5384 | ` *  Returns a multidimensional numerically indexed array, starting with` |
|       - | 5385 | ` *  zero, with each dimension containing size elements.` |
|       - | 5386 | ` */` |
|      42 | 5387 | `static int ph7_hashmap_chunk(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       2 | 5388 |  |
|       - | 5389 | `	ph7_value *pArray,*pChunk;` |
|       - | 5390 | `	ph7_hashmap_node *pEntry;` |
|       - | 5391 | `	ph7_hashmap *pMap;` |
|       - | 5392 | `	int bPreserve;` |
|       - | 5393 | `	sxu32 nChunk;` |
|       - | 5394 | `	sxu32 nSize;` |
|       - | 5395 | `	sxu32 n;` |
|       - | 5396 | `	/* Argument count and types follow PHP semantics. */` |
|      44 | 5397 | `	if( nArg < 2 ){` |
|       - | 5398 | `		/* fewer than required arguments -> ArgumentCountError */` |
|       4 | 5399 | `		return PH7_VmThrowException(pCtx,` |
|       - | 5400 | `			"ArgumentCountError",` |
|       - | 5401 | `			"array_chunk() expects at least 2 arguments, %d given",` |
|       1 | 5402 | `			nArg` |
|       - | 5403 | `			);` |
|       - | 5404 | `	}` |
|      42 | 5405 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|       4 | 5406 | `		return PH7_VmThrowException(pCtx,` |
|       - | 5407 | `			"TypeError",` |
|       - | 5408 | `			"array_chunk(): Argument #1 ($array) must be of type array, %s given",` |
|       1 | 5409 | `			ph7_type_name(apArg[0])` |
|       - | 5410 | `			);` |
|       - | 5411 | `	}` |
|       - | 5412 | `	/* Create a new array */` |
|      40 | 5413 | `	pArray = ph7_context_new_array(pCtx);` |
|      40 | 5414 | `	if( pArray == 0 ){` |
|     ! 0 | 5415 | `		ph7_result_null(pCtx);` |
|     ! 0 | 5416 | `		return PH7_OK;` |
|       - | 5417 | `	}` |
|       - | 5418 | `	/* Point to the internal representation of the input hashmap */` |
|      40 | 5419 | `	pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|       - | 5420 | `	/* Extract and validate the chunk size argument. */` |
|       - | 5421 | `	/* Reject types that cannot be sensibly converted to an integer. */` |
|      76 | 5422 | `	if( ph7_value_is_array(apArg[1]) \|\| ph7_value_is_object(apArg[1]) \|\|` |
|      78 | 5423 | `		ph7_value_is_resource(apArg[1]) \|\| ph7_value_is_null(apArg[1]) \|\|` |
|      38 | 5424 | `		ph7_value_is_bool(apArg[1]) ){` |
|     ! 0 | 5425 | `		return PH7_VmThrowException(pCtx,` |
|       - | 5426 | `			"TypeError",` |
|       - | 5427 | `			"array_chunk(): Argument #2 ($length) must be of type int, %s given",` |
|     ! 0 | 5428 | `			ph7_type_name(apArg[1])` |
|       - | 5429 | `			);` |
|       - | 5430 | `	}` |
|       - | 5431 | `	/* Strings that are non-numeric also produce a TypeError. */` |
|      40 | 5432 | `	if( ph7_value_is_string(apArg[1]) ){` |
|       - | 5433 | `		int len;` |
|       3 | 5434 | `		sxu8 bReal = FALSE;` |
|       3 | 5435 | `		const char *zStr = ph7_value_to_string(apArg[1], &len);` |
|       3 | 5436 | `			if( SyStrIsNumeric(zStr, len, &bReal, 0) != SXRET_OK \|\| bReal ){` |
|       3 | 5437 | `			return PH7_VmThrowException(pCtx,` |
|       - | 5438 | `				"TypeError",` |
|       - | 5439 | `				"array_chunk(): Argument #2 ($length) must be of type int, string given"` |
|       - | 5440 | `				);` |
|       - | 5441 | `		}` |
|     ! 0 | 5442 | `	}` |
|       - | 5443 | `	/* If the value is a float with a fractional component, refuse it.` |
|       - | 5444 | `	 * PHP currently warns but may become an error in the future; we` |
|       - | 5445 | `	 * enforce that policy now so PHL behaviour is strict. */` |
|      38 | 5446 | `	if( ph7_value_is_float(apArg[1]) ){` |
|       3 | 5447 | `		double d = ph7_value_to_double(apArg[1]);` |
|       3 | 5448 | `		sxi64 i = (sxi64)d;` |
|       3 | 5449 | `		if( d != (double)i ){` |
|       3 | 5450 | `			return PH7_VmThrowException(pCtx,` |
|       - | 5451 | `				"TypeError",` |
|       - | 5452 | `				"array_chunk(): Argument #2 ($length) must be of type int, float given"` |
|       - | 5453 | `				);` |
|       - | 5454 | `		}` |
|     ! 0 | 5455 | `	}` |
|       - | 5456 | `	/* Convert using ph7_value_to_int; now that float fractions are` |
|       - | 5457 | `	 * eliminated, this will not produce a warning. */` |
|       - | 5458 | `	{` |
|      36 | 5459 | `		sxi64 nSizeSigned = ph7_value_to_int(apArg[1]);` |
|      36 | 5460 | `		if( nSizeSigned < 1 ){` |
|       - | 5461 | `			/* size <= 0 -> ValueError */` |
|       5 | 5462 | `			return PH7_VmThrowException(pCtx,` |
|       - | 5463 | `				"ValueError",` |
|       - | 5464 | `				"array_chunk(): Argument #2 ($length) must be greater than 0"` |
|       - | 5465 | `				);` |
|       - | 5466 | `		}` |
|      32 | 5467 | `		nSize = (sxu32)nSizeSigned;` |
|       - | 5468 | `	}` |
|      32 | 5469 | `	if( nSize >= pMap->nEntry ){` |
|       - | 5470 | `		/* Return the whole array */` |
|       3 | 5471 | `		ph7_array_add_elem(pArray,0,apArg[0]);` |
|       3 | 5472 | `		ph7_result_value(pCtx,pArray);` |
|       3 | 5473 | `		return PH7_OK;` |
|       - | 5474 | `	}` |
|      30 | 5475 | `	bPreserve = 0;` |
|      30 | 5476 | `	if( nArg > 2 ){` |
|       - | 5477 | `		/* The third argument has a bool type hint in PHP.  Values that` |
|       - | 5478 | `		 * cannot be sensibly converted (arrays, objects, resources) are` |
|       - | 5479 | `		 * rejected with a TypeError.  Scalars and null coerce to bool` |
|       - | 5480 | `		 * normally, matching PHP behaviour. */` |
|      45 | 5481 | `		if( ph7_value_is_array(apArg[2]) \|\|` |
|      34 | 5482 | `			ph7_value_is_object(apArg[2]) \|\|` |
|      20 | 5483 | `			ph7_value_is_resource(apArg[2]) ){` |
|       7 | 5484 | `			return PH7_VmThrowException(pCtx,` |
|       - | 5485 | `				"TypeError",` |
|       - | 5486 | `				"array_chunk(): Argument #3 ($preserve_keys) must be of type bool, %s given",` |
|       4 | 5487 | `				ph7_type_name(apArg[2])` |
|       - | 5488 | `				);` |
|       - | 5489 | `		}` |
|      21 | 5490 | `		bPreserve = ph7_value_to_bool(apArg[2]);` |
|      10 | 5491 | `	}` |
|       - | 5492 | `	/* Start processing */` |
|      25 | 5493 | `	pEntry = pMap->pFirst;` |
|      25 | 5494 | `	nChunk = 0;` |
|      25 | 5495 | `	pChunk = 0;` |
|      25 | 5496 | `	n = pMap->nEntry;` |
|      51 | 5497 | `	for( ;; ){` |
|     103 | 5498 | `		if( n < 1 ){` |
|       - | 5499 | `			/* When the loop terminates we may still have a current chunk` |
|       - | 5500 | `			 * that hasn't been added to the result array.  The previous` |
|       - | 5501 | `			 * implementation only pushed it if nChunk>0 which dropped the` |
|       - | 5502 | `			 * final chunk when the input size was an exact multiple of` |
|       - | 5503 | `			 * the chunk length.  Always append the pending chunk if it` |
|       - | 5504 | `			 * exists. */` |
|      25 | 5505 | `			if( pChunk ){` |
|      25 | 5506 | `				ph7_array_add_elem(pArray,0,pChunk); /* Will have its own copy */` |
|      12 | 5507 | `			}` |
|      25 | 5508 | `			break;` |
|       - | 5509 | `		}` |
|      79 | 5510 | `		if( nChunk < 1 ){` |
|      67 | 5511 | `			if( pChunk ){` |
|       - | 5512 | `				/* Put the first chunk */` |
|      43 | 5513 | `				ph7_array_add_elem(pArray,0,pChunk); /* Will have it's own copy */` |
|      21 | 5514 | `			}` |
|       - | 5515 | `			/* Create a new dimension */` |
|      67 | 5516 | `			pChunk = ph7_context_new_array(pCtx); /* Don't worry about freeing memory here,everything` |
|       - | 5517 | `												   * will be automatically released as soon we return` |
|       - | 5518 | `												   * from this function */` |
|      67 | 5519 | `			if( pChunk == 0 ){` |
|     ! 0 | 5520 | `				break;` |
|       - | 5521 | `			}` |
|      67 | 5522 | `			nChunk = nSize;` |
|      33 | 5523 | `		}` |
|       - | 5524 | `		/* Insert the entry */` |
|      79 | 5525 | `		HashmapInsertNode((ph7_hashmap *)pChunk->x.pOther,pEntry,bPreserve);` |
|       - | 5526 | `		/* Point to the next entry */` |
|      79 | 5527 | `		pEntry = pEntry->pPrev; /* Reverse link */` |
|      79 | 5528 | `		nChunk--;` |
|      79 | 5529 | `		n--;` |
|       1 | 5530 | `	}` |
|       - | 5531 | `	/* Return the multidimensional array */` |
|      25 | 5532 | `	ph7_result_value(pCtx,pArray);` |
|      25 | 5533 | `	return PH7_OK;` |
|      23 | 5534 |  |
|       - | 5535 | `/*` |
|       - | 5536 | ` * array array_pad(array $input,int $pad_size,value $pad_value)` |
|       - | 5537 | ` *  Pad array to the specified length with a value.` |
|       - | 5538 | ` * $input` |
|       - | 5539 | ` *   Initial array of values to pad.` |
|       - | 5540 | ` * $pad_size` |
|       - | 5541 | ` *   New size of the array.` |
|       - | 5542 | ` * $pad_value` |
|       - | 5543 | ` *   Value to pad if input is less than pad_size.` |
|       - | 5544 | ` */` |
|       8 | 5545 | `static int ph7_hashmap_pad(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       1 | 5546 |  |
|       - | 5547 | `	ph7_hashmap *pMap;` |
|       - | 5548 | `	ph7_value *pArray;` |
|       - | 5549 | `	int nEntry;` |
|       9 | 5550 | `	if( nArg < 3 \|\| !ph7_value_is_array(apArg[0]) ){` |
|       - | 5551 | `		/* Invalid arguments,return NULL */` |
|     ! 0 | 5552 | `		ph7_result_null(pCtx);` |
|     ! 0 | 5553 | `		return PH7_OK;` |
|       - | 5554 | `	}` |
|       - | 5555 | `	/* Create a new array */` |
|       9 | 5556 | `	pArray = ph7_context_new_array(pCtx);` |
|       9 | 5557 | `	if( pArray == 0 ){` |
|     ! 0 | 5558 | `		ph7_result_null(pCtx);` |
|     ! 0 | 5559 | `		return PH7_OK;` |
|       - | 5560 | `	}` |
|       - | 5561 | `	/* Point to the internal representation of the input hashmap */` |
|       9 | 5562 | `	pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|       - | 5563 | `	/* Extract the total number of desired entry to insert */` |
|       9 | 5564 | `	nEntry = ph7_value_to_int(apArg[1]);` |
|       9 | 5565 | `	if( nEntry < 0 ){` |
|       5 | 5566 | `		nEntry = -nEntry;` |
|       5 | 5567 | `		if( nEntry > 1048576 ){` |
|     ! 0 | 5568 | `			nEntry = 1048576; /* Limit imposed by PHP */` |
|     ! 0 | 5569 | `		}` |
|       5 | 5570 | `		if( nEntry > (int)pMap->nEntry ){` |
|       3 | 5571 | `			nEntry -= (int)pMap->nEntry;` |
|       - | 5572 | `			/* Insert given items first */` |
|       7 | 5573 | `			while( nEntry > 0 ){` |
|       5 | 5574 | `				ph7_array_add_elem(pArray,0,apArg[2]);` |
|       5 | 5575 | `				nEntry--;` |
|       1 | 5576 | `			}` |
|       - | 5577 | `			/* Merge the two arrays */` |
|       3 | 5578 | `			HashmapMerge(pMap,(ph7_hashmap *)pArray->x.pOther);` |
|       2 | 5579 | `		}else{` |
|       3 | 5580 | `			PH7_HashmapDup(pMap,(ph7_hashmap *)pArray->x.pOther);` |
|       1 | 5581 | `		}` |
|       7 | 5582 | `	}else if( nEntry > 0 ){` |
|       5 | 5583 | `		if( nEntry > 1048576 ){` |
|     ! 0 | 5584 | `			nEntry = 1048576; /* Limit imposed by PHP */` |
|     ! 0 | 5585 | `		}` |
|       5 | 5586 | `		if( nEntry > (int)pMap->nEntry ){` |
|       3 | 5587 | `			nEntry -= (int)pMap->nEntry;` |
|       - | 5588 | `			/* Merge the two arrays first */` |
|       3 | 5589 | `			HashmapMerge(pMap,(ph7_hashmap *)pArray->x.pOther);` |
|       - | 5590 | `			/* Insert given items */` |
|       7 | 5591 | `			while( nEntry > 0 ){` |
|       5 | 5592 | `				ph7_array_add_elem(pArray,0,apArg[2]);` |
|       5 | 5593 | `				nEntry--;` |
|       1 | 5594 | `			}` |
|       2 | 5595 | `		}else{` |
|       3 | 5596 | `			PH7_HashmapDup(pMap,(ph7_hashmap *)pArray->x.pOther);` |
|       - | 5597 | `		}` |
|       2 | 5598 | `	}` |
|       - | 5599 | `	/* Return the new array */` |
|       9 | 5600 | `	ph7_result_value(pCtx,pArray);` |
|       9 | 5601 | `	return PH7_OK;` |
|       5 | 5602 |  |
|       - | 5603 | `/*` |
|       - | 5604 | ` * array array_replace(array &$array,array &$array1,...)` |
|       - | 5605 | ` *  Replaces elements from passed arrays into the first array.` |
|       - | 5606 | ` * Parameters` |
|       - | 5607 | ` * $array` |
|       - | 5608 | ` *   The array in which elements are replaced.` |
|       - | 5609 | ` * $array1` |
|       - | 5610 | ` *   The array from which elements will be extracted.` |
|       - | 5611 | ` * ....` |
|       - | 5612 | ` *  More arrays from which elements will be extracted.` |
|       - | 5613 | ` *  Values from later arrays overwrite the previous values.` |
|       - | 5614 | ` * Return` |
|       - | 5615 | ` *  Returns an array, or NULL if an error occurs.` |
|       - | 5616 | ` */` |
|       2 | 5617 | `static int ph7_hashmap_replace(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       1 | 5618 |  |
|       - | 5619 | `	ph7_hashmap *pMap;` |
|       - | 5620 | `	ph7_value *pArray;` |
|       - | 5621 | `	int i;` |
|       3 | 5622 | `	if( nArg < 1 ){` |
|       - | 5623 | `		/* Invalid arguments,return NULL */` |
|     ! 0 | 5624 | `		ph7_result_null(pCtx);` |
|     ! 0 | 5625 | `		return PH7_OK;` |
|       - | 5626 | `	}` |
|       - | 5627 | `	/* Create a new array */` |
|       3 | 5628 | `	pArray = ph7_context_new_array(pCtx);` |
|       3 | 5629 | `	if( pArray == 0 ){` |
|     ! 0 | 5630 | `		ph7_result_null(pCtx);` |
|     ! 0 | 5631 | `		return PH7_OK;` |
|       - | 5632 | `	}` |
|       - | 5633 | `	/* Perform the requested operation */` |
|       7 | 5634 | `	for( i = 0 ; i < nArg ; i++ ){` |
|       5 | 5635 | `		if( !ph7_value_is_array(apArg[i]) ){` |
|     ! 0 | 5636 | `			continue;` |
|       - | 5637 | `		}` |
|       - | 5638 | `		/* Point to the internal representation of the input hashmap */` |
|       5 | 5639 | `		pMap = (ph7_hashmap *)apArg[i]->x.pOther;` |
|       5 | 5640 | `		HashmapOverwrite(pMap,(ph7_hashmap *)pArray->x.pOther);` |
|       3 | 5641 | `	}` |
|       - | 5642 | `	/* Return the new array */` |
|       3 | 5643 | `	ph7_result_value(pCtx,pArray);` |
|       3 | 5644 | `	return PH7_OK;` |
|       2 | 5645 |  |
|       - | 5646 | `/*` |
|       - | 5647 | ` * array array_filter(array $input [,callback $callback ])` |
|       - | 5648 | ` *  Filters elements of an array using a callback function.` |
|       - | 5649 | ` * Parameters` |
|       - | 5650 | ` *  $input` |
|       - | 5651 | ` *    The array to iterate over` |
|       - | 5652 | ` * $callback` |
|       - | 5653 | ` *    The callback function to use` |
|       - | 5654 | ` *    If no callback is supplied, all entries of input equal to FALSE (see converting to boolean)` |
|       - | 5655 | ` *    will be removed.` |
|       - | 5656 | ` * Return` |
|       - | 5657 | ` *  The filtered array.` |
|       - | 5658 | ` */` |
|      18 | 5659 | `static int ph7_hashmap_filter(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       2 | 5660 |  |
|       - | 5661 | `	ph7_hashmap_node *pEntry;` |
|       - | 5662 | `	ph7_hashmap *pMap;` |
|       - | 5663 | `	ph7_value *pArray;` |
|       - | 5664 | `	ph7_value sResult;   /* Callback result */` |
|       - | 5665 | `	ph7_value *pValue;` |
|       - | 5666 | `	sxi32 rc;` |
|       - | 5667 | `	int keep;` |
|       - | 5668 | `	sxu32 n;` |
|      20 | 5669 | `	if( nArg < 1 \|\| !ph7_value_is_array(apArg[0]) ){` |
|       - | 5670 | `		/* Invalid arguments,return NULL */` |
|       5 | 5671 | `		ph7_result_null(pCtx);` |
|       5 | 5672 | `		return PH7_OK;` |
|       - | 5673 | `	}` |
|       - | 5674 | `	/* Create a new array */` |
|      16 | 5675 | `	pArray = ph7_context_new_array(pCtx);` |
|      16 | 5676 | `	if( pArray == 0 ){` |
|     ! 0 | 5677 | `		ph7_result_null(pCtx);` |
|     ! 0 | 5678 | `		return PH7_OK;` |
|       - | 5679 | `	}` |
|       - | 5680 | `	/* Point to the internal representation of the input hashmap */` |
|      16 | 5681 | `	pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|      16 | 5682 | `	pEntry = pMap->pFirst;` |
|      16 | 5683 | `	PH7_MemObjInit(pMap->pVm,&sResult);` |
|      16 | 5684 | `	sResult.nIdx = SXU32_HIGH; /* Mark as constant */` |
|       - | 5685 | `	/* Perform the requested operation */` |
|      72 | 5686 | `	for( n = 0 ; n < pMap->nEntry ; n++ ){` |
|       - | 5687 | `		/* Extract node value (may be NULL if allocation failed) */` |
|      58 | 5688 | `		pValue = HashmapExtractNodeValue(pEntry);` |
|      58 | 5689 | `		if( pValue == 0 ){` |
|       - | 5690 | `			/* Can happen if SySetAt() failed earlier; drop the entry. */` |
|     ! 0 | 5691 | `			keep = FALSE;` |
|      58 | 5692 | `		}else if( nArg > 1 && !ph7_value_is_null(apArg[1]) ){` |
|       - | 5693 | `			/* A callback was supplied (and is not NULL). Only invoke it` |
|       - | 5694 | `			 * if it is actually callable. Otherwise we simply drop the` |
|       - | 5695 | `			 * element which matches the legacy behaviour where` |
|       - | 5696 | `			 * PH7_VmCallUserFunction() would return SXERR_INVALID.` |
|       - | 5697 | `			 */` |
|      30 | 5698 | `			keep = FALSE;` |
|      41 | 5699 | `			if( ph7_value_is_callable(apArg[1]) ){` |
|      23 | 5700 | `				rc = PH7_VmCallUserFunction(pMap->pVm,apArg[1],1,&pValue,&sResult);` |
|      23 | 5701 | `				if( rc == SXRET_OK ){` |
|       - | 5702 | `					/* Perform a boolean cast */` |
|      23 | 5703 | `					keep = ph7_value_to_bool(&sResult);` |
|      11 | 5704 | `				}` |
|      23 | 5705 | `				PH7_MemObjRelease(&sResult);` |
|      11 | 5706 | `			}` |
|      16 | 5707 | `		}else{` |
|       - | 5708 | `			/* No callback provided or callback explicitly NULL: use default` |
|       - | 5709 | `			 * behaviour where "empty" values are removed. This also covers` |
|       - | 5710 | `			 * the case where the callback argument is missing entirely.` |
|       - | 5711 | `			 */` |
|      29 | 5712 | `			keep = !PH7_MemObjIsEmpty(pValue);` |
|       - | 5713 | `		}` |
|      58 | 5714 | `		if( keep ){` |
|       - | 5715 | `			/* Perform the insertion,now the callback returned true */` |
|      17 | 5716 | `			HashmapInsertNode((ph7_hashmap *)pArray->x.pOther,pEntry,TRUE);` |
|       8 | 5717 | `		}` |
|       - | 5718 | `		/* Point to the next entry */` |
|      58 | 5719 | `		pEntry = pEntry->pPrev; /* Reverse link */` |
|      30 | 5720 | `	}` |
|      16 | 5721 | `	ph7_result_value(pCtx,pArray);` |
|      16 | 5722 | `	return PH7_OK;` |
|      11 | 5723 |  |
|       - | 5724 | `/*` |
|       - | 5725 | ` * array array_map(callback $callback,array $arr1)` |
|       - | 5726 | ` *  Applies the callback to the elements of the given arrays.` |
|       - | 5727 | ` * Parameters` |
|       - | 5728 | ` *  $callback` |
|       - | 5729 | ` *   Callback function to run for each element in each array.` |
|       - | 5730 | ` * $arr1` |
|       - | 5731 | ` *   An array to run through the callback function.` |
|       - | 5732 | ` * Return` |
|       - | 5733 | ` *  Returns an array containing all the elements of arr1 after applying` |
|       - | 5734 | ` *  the callback function to each one.` |
|       - | 5735 | ` * NOTE:` |
|       - | 5736 | ` *  array_map() passes only a single value to the callback.` |
|       - | 5737 | ` */` |
|      10 | 5738 | `static int ph7_hashmap_map(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       1 | 5739 |  |
|       - | 5740 | `	ph7_value *pArray,*pValue,sKey,sResult;` |
|       - | 5741 | `	ph7_hashmap_node *pEntry;` |
|       - | 5742 | `	ph7_hashmap *pMap;` |
|       - | 5743 | `	sxu32 n;` |
|      11 | 5744 | `	if( nArg < 2 \|\| !ph7_value_is_array(apArg[1]) ){` |
|       - | 5745 | `		/* Invalid arguments,return NULL */` |
|       5 | 5746 | `		ph7_result_null(pCtx);` |
|       5 | 5747 | `		return PH7_OK;` |
|       - | 5748 | `	}` |
|       - | 5749 | `	/* Create a new array */` |
|       7 | 5750 | `	pArray = ph7_context_new_array(pCtx);` |
|       7 | 5751 | `	if( pArray == 0 ){` |
|     ! 0 | 5752 | `		ph7_result_null(pCtx);` |
|     ! 0 | 5753 | `		return PH7_OK;` |
|       - | 5754 | `	}` |
|       - | 5755 | `	/* Point to the internal representation of the input hashmap */` |
|       7 | 5756 | `	pMap = (ph7_hashmap *)apArg[1]->x.pOther;` |
|       7 | 5757 | `	PH7_MemObjInit(pMap->pVm,&sResult);` |
|       7 | 5758 | `	PH7_MemObjInit(pMap->pVm,&sKey);` |
|       7 | 5759 | `	sResult.nIdx = SXU32_HIGH; /* Mark as constant */` |
|       7 | 5760 | `	sKey.nIdx    = SXU32_HIGH; /* Mark as constant */` |
|       - | 5761 | `	/* Perform the requested operation */` |
|       7 | 5762 | `	pEntry = pMap->pFirst;` |
|      21 | 5763 | `	for( n = 0 ; n < pMap->nEntry ; n++ ){` |
|       - | 5764 | `		/* Extrcat the node value */` |
|      15 | 5765 | `		pValue = HashmapExtractNodeValue(pEntry);` |
|      15 | 5766 | `		if( pValue ){` |
|       - | 5767 | `			sxi32 rc;` |
|       - | 5768 | `			/* Invoke the supplied callback */` |
|      15 | 5769 | `			rc = PH7_VmCallUserFunction(pMap->pVm,apArg[0],1,&pValue,&sResult);` |
|       - | 5770 | `			/* Extract the node key */` |
|      15 | 5771 | `			PH7_HashmapExtractNodeKey(pEntry,&sKey);` |
|      15 | 5772 | `			if( rc != SXRET_OK ){` |
|       - | 5773 | `				/* An error occured while invoking the supplied callback [i.e: not defined] */` |
|     ! 0 | 5774 | `				ph7_array_add_elem(pArray,&sKey,pValue); /* Keep the same value */` |
|     ! 0 | 5775 | `			}else{` |
|       - | 5776 | `				/* Insert the callback return value */` |
|      15 | 5777 | `				ph7_array_add_elem(pArray,&sKey,&sResult);` |
|       - | 5778 | `			}` |
|      15 | 5779 | `			PH7_MemObjRelease(&sKey);` |
|      15 | 5780 | `			PH7_MemObjRelease(&sResult);` |
|       7 | 5781 | `		}` |
|       - | 5782 | `		/* Point to the next entry */` |
|      15 | 5783 | `		pEntry = pEntry->pPrev; /* Reverse link */` |
|       8 | 5784 | `	}` |
|       7 | 5785 | `	ph7_result_value(pCtx,pArray);` |
|       7 | 5786 | `	return PH7_OK;` |
|       6 | 5787 |  |
|       - | 5788 | `/*` |
|       - | 5789 | ` * value array_reduce(array $input,callback $function[, value $initial = NULL ])` |
|       - | 5790 | ` *  Iteratively reduce the array to a single value using a callback function.` |
|       - | 5791 | ` * Parameters` |
|       - | 5792 | ` *  $input` |
|       - | 5793 | ` *   The input array.` |
|       - | 5794 | ` *  $function` |
|       - | 5795 | ` *  The callback function.` |
|       - | 5796 | ` * $initial` |
|       - | 5797 | ` *  If the optional initial is available, it will be used at the beginning` |
|       - | 5798 | ` *  of the process, or as a final result in case the array is empty.` |
|       - | 5799 | ` * Return` |
|       - | 5800 | ` *  Returns the resulting value.` |
|       - | 5801 | ` *  If the array is empty and initial is not passed, array_reduce() returns NULL.` |
|       - | 5802 | ` */` |
|       4 | 5803 | `static int ph7_hashmap_reduce(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       1 | 5804 |  |
|       - | 5805 | `	ph7_hashmap_node *pEntry;` |
|       - | 5806 | `	ph7_hashmap *pMap;` |
|       - | 5807 | `	ph7_value *pValue;` |
|       - | 5808 | `	ph7_value sResult;` |
|       - | 5809 | `	sxu32 n;` |
|       5 | 5810 | `	if( nArg < 2 \|\| !ph7_value_is_array(apArg[0]) ){` |
|       - | 5811 | `		/* Invalid/Missing arguments,return NULL */` |
|     ! 0 | 5812 | `		ph7_result_null(pCtx);` |
|     ! 0 | 5813 | `		return PH7_OK;` |
|       - | 5814 | `	}` |
|       - | 5815 | `	/* Point to the internal representation of the input hashmap */` |
|       5 | 5816 | `	pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|       - | 5817 | `	/* Assume a NULL initial value */` |
|       5 | 5818 | `	PH7_MemObjInit(pMap->pVm,&sResult);` |
|       5 | 5819 | `	sResult.nIdx = SXU32_HIGH; /* Mark as constant */` |
|       5 | 5820 | `	if( nArg > 2 ){` |
|       - | 5821 | `		/* Set the initial value */` |
|       5 | 5822 | `		PH7_MemObjLoad(apArg[2],&sResult);` |
|       2 | 5823 | `	}` |
|       - | 5824 | `	/* Perform the requested operation */` |
|       5 | 5825 | `	pEntry = pMap->pFirst;` |
|      19 | 5826 | `	for( n = 0 ; n < pMap->nEntry ; n++ ){` |
|       - | 5827 | `		/* Extract the node value */` |
|      15 | 5828 | `		pValue = HashmapExtractNodeValue(pEntry);` |
|       - | 5829 | `		/* Invoke the supplied callback */` |
|      15 | 5830 | `		PH7_VmCallUserFunctionAp(pMap->pVm,apArg[1],&sResult,&sResult,pValue,0);` |
|       - | 5831 | `		/* Point to the next entry */` |
|      15 | 5832 | `		pEntry = pEntry->pPrev; /* Reverse link */` |
|       8 | 5833 | `	}` |
|       5 | 5834 | `	ph7_result_value(pCtx,&sResult); /* Will make it's own copy */` |
|       5 | 5835 | `	PH7_MemObjRelease(&sResult);` |
|       5 | 5836 | `	return PH7_OK;` |
|       3 | 5837 |  |
|       - | 5838 | `/*` |
|       - | 5839 | ` * bool array_walk(array &$array,callback $funcname [, value $userdata ] )` |
|       - | 5840 | ` *  Apply a user function to every member of an array.` |
|       - | 5841 | ` * Parameters` |
|       - | 5842 | ` *  $array` |
|       - | 5843 | ` *   The input array.` |
|       - | 5844 | ` * $funcname` |
|       - | 5845 | ` *  Typically, funcname takes on two parameters.The array parameter's value being` |
|       - | 5846 | ` *  the first, and the key/index second.` |
|       - | 5847 | ` * Note:` |
|       - | 5848 | ` *  If funcname needs to be working with the actual values of the array,specify the first` |
|       - | 5849 | ` *  parameter of funcname as a reference. Then, any changes made to those elements will` |
|       - | 5850 | ` *  be made in the original array itself.` |
|       - | 5851 | ` * $userdata` |
|       - | 5852 | ` *  If the optional userdata parameter is supplied, it will be passed as the third parameter` |
|       - | 5853 | ` *  to the callback funcname.` |
|       - | 5854 | ` * Return` |
|       - | 5855 | ` *  Returns TRUE on success or FALSE on failure.` |
|       - | 5856 | ` */` |
|      12 | 5857 | `static int ph7_hashmap_walk(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       1 | 5858 |  |
|       - | 5859 | `	ph7_value *pValue,*pUserData,sKey;` |
|       - | 5860 | `	ph7_hashmap_node *pEntry;` |
|       - | 5861 | `	ph7_hashmap *pMap;` |
|       - | 5862 | `	sxi32 rc;` |
|       - | 5863 | `	sxu32 n;` |
|      13 | 5864 | `	if( nArg < 2 \|\| !ph7_value_is_array(apArg[0]) ){` |
|       - | 5865 | `		/* Invalid/Missing arguments,return FALSE */` |
|     ! 0 | 5866 | `		ph7_result_bool(pCtx,0);` |
|     ! 0 | 5867 | `		return PH7_OK;` |
|       - | 5868 | `	}` |
|      13 | 5869 | `	pUserData = nArg > 2 ? apArg[2] : 0;` |
|       - | 5870 | `	/* Point to the internal representation of the input hashmap */` |
|      13 | 5871 | `	pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|      13 | 5872 | `	PH7_MemObjInit(pMap->pVm,&sKey);` |
|      13 | 5873 | `	sKey.nIdx = SXU32_HIGH; /* Mark as constant */` |
|       - | 5874 | `	/* Perform the desired operation */` |
|      13 | 5875 | `	pEntry = pMap->pFirst;` |
|      41 | 5876 | `	for( n = 0 ; n < pMap->nEntry ; n++ ){` |
|       - | 5877 | `		/* Extract the node value */` |
|      29 | 5878 | `		pValue = HashmapExtractNodeValue(pEntry);` |
|      29 | 5879 | `		if( pValue ){` |
|       - | 5880 | `			/* Extract the entry key */` |
|      29 | 5881 | `			PH7_HashmapExtractNodeKey(pEntry,&sKey);` |
|       - | 5882 | `			/* Invoke the supplied callback */` |
|      29 | 5883 | `			rc = PH7_VmCallUserFunctionAp(pMap->pVm,apArg[1],0,pValue,&sKey,pUserData,0);` |
|      29 | 5884 | `			PH7_MemObjRelease(&sKey);` |
|      29 | 5885 | `			if( rc != SXRET_OK ){` |
|       - | 5886 | `				/* An error occured while invoking the supplied callback [i.e: not defined] */` |
|     ! 0 | 5887 | `				ph7_result_bool(pCtx,0); /* return FALSE */` |
|     ! 0 | 5888 | `				return PH7_OK;` |
|       - | 5889 | `			}` |
|      14 | 5890 | `		}` |
|       - | 5891 | `		/* Point to the next entry */` |
|      29 | 5892 | `		pEntry = pEntry->pPrev; /* Reverse link */` |
|      15 | 5893 | `	}` |
|       - | 5894 | `	/* All done,return TRUE */` |
|      13 | 5895 | `	ph7_result_bool(pCtx,1);` |
|      13 | 5896 | `	return PH7_OK;` |
|       7 | 5897 |  |
|       - | 5898 | `/*` |
|       - | 5899 | ` * Apply a user function to every member of an array.(Recurse on array's).` |
|       - | 5900 | ` * Refer to the [array_walk_recursive()] implementation for more information.` |
|       - | 5901 | ` */` |
|       6 | 5902 | `static int HashmapWalkRecursive(` |
|       - | 5903 | `	ph7_hashmap *pMap,    /* Target hashmap */` |
|       - | 5904 | `	ph7_value *pCallback, /* User callback */` |
|       - | 5905 | `	ph7_value *pUserData, /* Callback private data */` |
|       - | 5906 | `	int iNest             /* Nesting level */` |
|       - | 5907 | `	)` |
|       1 | 5908 |  |
|       - | 5909 | `	ph7_hashmap_node *pEntry;` |
|       - | 5910 | `	ph7_value *pValue,sKey;` |
|       - | 5911 | `	sxi32 rc;` |
|       - | 5912 | `	sxu32 n;` |
|       - | 5913 | `	/* Iterate throw hashmap entries */` |
|       7 | 5914 | `	PH7_MemObjInit(pMap->pVm,&sKey);` |
|       7 | 5915 | `	sKey.nIdx = SXU32_HIGH; /* Mark as constant */` |
|       7 | 5916 | `	pEntry = pMap->pFirst;` |
|      17 | 5917 | `	for( n = 0 ; n < pMap->nEntry ; n++ ){` |
|       - | 5918 | `		/* Extract the node value */` |
|      11 | 5919 | `		pValue = HashmapExtractNodeValue(pEntry);` |
|      11 | 5920 | `		if( pValue ){` |
|      11 | 5921 | `			if( pValue->iFlags & MEMOBJ_HASHMAP ){` |
|       5 | 5922 | `				if( iNest < 32 ){` |
|       - | 5923 | `					/* Recurse */` |
|       5 | 5924 | `					iNest++;` |
|       5 | 5925 | `					HashmapWalkRecursive((ph7_hashmap *)pValue->x.pOther,pCallback,pUserData,iNest);` |
|       5 | 5926 | `					iNest--;` |
|       2 | 5927 | `				}` |
|       3 | 5928 | `			}else{` |
|       - | 5929 | `				/* Extract the node key */` |
|       7 | 5930 | `				PH7_HashmapExtractNodeKey(pEntry,&sKey);` |
|       - | 5931 | `				/* Invoke the supplied callback */` |
|       7 | 5932 | `				rc = PH7_VmCallUserFunctionAp(pMap->pVm,pCallback,0,pValue,&sKey,pUserData,0);` |
|       7 | 5933 | `				PH7_MemObjRelease(&sKey);` |
|       7 | 5934 | `				if( rc != SXRET_OK ){` |
|     ! 0 | 5935 | `					return rc;` |
|       - | 5936 | `				}` |
|       - | 5937 | `			}` |
|       5 | 5938 | `		}` |
|       - | 5939 | `		/* Point to the next entry */` |
|      11 | 5940 | `		pEntry = pEntry->pPrev; /* Reverse link */` |
|       6 | 5941 | `	}` |
|       7 | 5942 | `	return SXRET_OK;` |
|       4 | 5943 |  |
|       - | 5944 | `/*` |
|       - | 5945 | ` * bool array_walk_recursive(array &$array,callback $funcname [, value $userdata ] )` |
|       - | 5946 | ` *  Apply a user function recursively to every member of an array.` |
|       - | 5947 | ` * Parameters` |
|       - | 5948 | ` *  $array` |
|       - | 5949 | ` *   The input array.` |
|       - | 5950 | ` * $funcname` |
|       - | 5951 | ` *  Typically, funcname takes on two parameters.The array parameter's value being` |
|       - | 5952 | ` *  the first, and the key/index second.` |
|       - | 5953 | ` * Note:` |
|       - | 5954 | ` *  If funcname needs to be working with the actual values of the array,specify the first` |
|       - | 5955 | ` *  parameter of funcname as a reference. Then, any changes made to those elements will` |
|       - | 5956 | ` *  be made in the original array itself.` |
|       - | 5957 | ` * $userdata` |
|       - | 5958 | ` *  If the optional userdata parameter is supplied, it will be passed as the third parameter` |
|       - | 5959 | ` *  to the callback funcname.` |
|       - | 5960 | ` * Return` |
|       - | 5961 | ` *  Returns TRUE on success or FALSE on failure.` |
|       - | 5962 | ` */` |
|       2 | 5963 | `static int ph7_hashmap_walk_recursive(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       1 | 5964 |  |
|       - | 5965 | `	ph7_hashmap *pMap;` |
|       - | 5966 | `	sxi32 rc;` |
|       3 | 5967 | `	if( nArg < 2 \|\| !ph7_value_is_array(apArg[0]) ){` |
|       - | 5968 | `		/* Invalid/Missing arguments,return FALSE */` |
|     ! 0 | 5969 | `		ph7_result_bool(pCtx,0);` |
|     ! 0 | 5970 | `		return PH7_OK;` |
|       - | 5971 | `	}` |
|       - | 5972 | `	/* Point to the internal representation of the input hashmap */` |
|       3 | 5973 | `	pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|       - | 5974 | `	/* Perform the desired operation */` |
|       3 | 5975 | `	rc = HashmapWalkRecursive(pMap,apArg[1],nArg > 2 ? apArg[2] : 0,0);` |
|       - | 5976 | `	/* All done */` |
|       3 | 5977 | `	ph7_result_bool(pCtx,rc == SXRET_OK);` |
|       3 | 5978 | `	return PH7_OK;` |
|       2 | 5979 |  |
|       - | 5980 | `/*` |
|       - | 5981 | ` * Table of hashmap functions.` |
|       - | 5982 | ` */` |
|       - | 5983 | `static const ph7_builtin_func aHashmapFunc[] = {` |
|       - | 5984 | `	{"count",             ph7_hashmap_count },` |
|       - | 5985 | `	{"sizeof",            ph7_hashmap_count },` |
|       - | 5986 | `	{"array_key_exists",  ph7_hashmap_key_exists },` |
|       - | 5987 | `	{"array_pop",         ph7_hashmap_pop     },` |
|       - | 5988 | `	{"array_push",        ph7_hashmap_push    },` |
|       - | 5989 | `	{"array_shift",       ph7_hashmap_shift   },` |
|       - | 5990 | `	{"array_product",     ph7_hashmap_product },` |
|       - | 5991 | `	{"array_sum",         ph7_hashmap_sum     },` |
|       - | 5992 | `	{"array_keys",        ph7_hashmap_keys    },` |
|       - | 5993 | `	{"array_values",      ph7_hashmap_values  },` |
|       - | 5994 | `	{"array_same",        ph7_hashmap_same    },  /* Symisc eXtension */` |
|       - | 5995 | `	{"array_merge",       ph7_hashmap_merge   },` |
|       - | 5996 | `	{"array_slice",       ph7_hashmap_slice   },` |
|       - | 5997 | `	{"array_splice",      ph7_hashmap_splice  },` |
|       - | 5998 | `	{"array_search",      ph7_hashmap_search  },` |
|       - | 5999 | `	{"array_diff",        ph7_hashmap_diff    },` |
|       - | 6000 | `	{"array_udiff",       ph7_hashmap_udiff   },` |
|       - | 6001 | `	{"array_diff_assoc",  ph7_hashmap_diff_assoc },` |
|       - | 6002 | `	{"array_diff_uassoc", ph7_hashmap_diff_uassoc },` |
|       - | 6003 | `	{"array_diff_key",    ph7_hashmap_diff_key },` |
|       - | 6004 | `	{"array_intersect",   ph7_hashmap_intersect},` |
|       - | 6005 | `	{"array_intersect_assoc", ph7_hashmap_intersect_assoc},` |
|       - | 6006 | `	{"array_uintersect",  ph7_hashmap_uintersect},` |
|       - | 6007 | `	{"array_intersect_key",   ph7_hashmap_intersect_key},` |
|       - | 6008 | `	{"array_copy",        ph7_hashmap_copy    },` |
|       - | 6009 | `	{"array_erase",       ph7_hashmap_erase   },` |
|       - | 6010 | `	{"array_fill",        ph7_hashmap_fill    },` |
|       - | 6011 | `	{"array_fill_keys",   ph7_hashmap_fill_keys},` |
|       - | 6012 | `	{"array_combine",     ph7_hashmap_combine },` |
|       - | 6013 | `	{"array_reverse",     ph7_hashmap_reverse },` |
|       - | 6014 | `	{"array_unique",      ph7_hashmap_unique  },` |
|       - | 6015 | `	{"array_flip",        ph7_hashmap_flip    },` |
|       - | 6016 | `	{"array_rand",        ph7_hashmap_rand    },` |
|       - | 6017 | `	{"array_chunk",       ph7_hashmap_chunk   },` |
|       - | 6018 | `	{"array_pad",         ph7_hashmap_pad     },` |
|       - | 6019 | `	{"array_replace",     ph7_hashmap_replace },` |
|       - | 6020 | `	{"array_filter",      ph7_hashmap_filter  },` |
|       - | 6021 | `	{"array_map",         ph7_hashmap_map     },` |
|       - | 6022 | `	{"array_reduce",      ph7_hashmap_reduce  },` |
|       - | 6023 | `	{"array_walk",        ph7_hashmap_walk    },` |
|       - | 6024 | `	{"array_walk_recursive", ph7_hashmap_walk_recursive },` |
|       - | 6025 | `	{"in_array",          ph7_hashmap_in_array},` |
|       - | 6026 | `	{"sort",              ph7_hashmap_sort    },` |
|       - | 6027 | `	{"asort",             ph7_hashmap_asort   },` |
|       - | 6028 | `	{"arsort",            ph7_hashmap_arsort  },` |
|       - | 6029 | `	{"ksort",             ph7_hashmap_ksort   },` |
|       - | 6030 | `	{"krsort",            ph7_hashmap_krsort  },` |
|       - | 6031 | `	{"rsort",             ph7_hashmap_rsort   },` |
|       - | 6032 | `	{"usort",             ph7_hashmap_usort   },` |
|       - | 6033 | `	{"uasort",            ph7_hashmap_uasort  },` |
|       - | 6034 | `	{"uksort",            ph7_hashmap_uksort  },` |
|       - | 6035 | `	{"shuffle",           ph7_hashmap_shuffle },` |
|       - | 6036 | `	{"range",             ph7_hashmap_range   },` |
|       - | 6037 | `	{"current",           ph7_hashmap_current },` |
|       - | 6038 | `	{"each",              ph7_hashmap_each    },` |
|       - | 6039 | `	{"pos",               ph7_hashmap_current },` |
|       - | 6040 | `	{"next",              ph7_hashmap_next    },` |
|       - | 6041 | `	{"prev",              ph7_hashmap_prev    },` |
|       - | 6042 | `	{"end",               ph7_hashmap_end     },` |
|       - | 6043 | `	{"reset",             ph7_hashmap_reset   },` |
|       - | 6044 | `	{"key",               ph7_hashmap_simple_key }` |
|       - | 6045 | `};` |
|       - | 6046 | `/*` |
|       - | 6047 | ` * Register the built-in hashmap functions defined above.` |
|       - | 6048 | ` */` |
|    1224 | 6049 | `PH7_PRIVATE void PH7_RegisterHashmapFunctions(ph7_vm *pVm)` |
|       2 | 6050 |  |
|       - | 6051 | `	sxu32 n;` |
|   75890 | 6052 | `	for( n = 0 ; n < SX_ARRAYSIZE(aHashmapFunc) ; n++ ){` |
|   74666 | 6053 | `		ph7_create_function(&(*pVm),aHashmapFunc[n].zName,aHashmapFunc[n].xFunc,0);` |
|   37334 | 6054 | `	}` |
|    1226 | 6055 |  |
|       - | 6056 | `/*` |
|       - | 6057 | ` * Dump a hashmap instance and it's entries and the store the dump in` |
|       - | 6058 | ` * the BLOB given as the first argument.` |
|       - | 6059 | ` * This function is typically invoked when the user issue a call to` |
|       - | 6060 | ` * [var_dump(),var_export(),print_r(),...]` |
|       - | 6061 | ` * This function SXRET_OK on success. Any other return value including` |
|       - | 6062 | ` * SXERR_LIMIT(infinite recursion) indicates failure.` |
|       - | 6063 | ` */` |
|      28 | 6064 | `PH7_PRIVATE sxi32 PH7_HashmapDump(SyBlob *pOut,ph7_hashmap *pMap,int ShowType,int nTab,int nDepth)` |
|       2 | 6065 |  |
|       - | 6066 | `	ph7_hashmap_node *pEntry;` |
|       - | 6067 | `	ph7_value *pObj;` |
|      30 | 6068 | `	sxu32 n = 0;` |
|       - | 6069 | `	int isRef;` |
|       - | 6070 | `	sxi32 rc;` |
|       - | 6071 | `	int i;` |
|      30 | 6072 | `	if( nDepth > 31 ){` |
|       - | 6073 | `		static const char zInfinite[] = "Nesting limit reached: Infinite recursion?";` |
|       - | 6074 | `		/* Nesting limit reached */` |
|     ! 0 | 6075 | `		SyBlobAppend(&(*pOut),zInfinite,sizeof(zInfinite)-1);` |
|     ! 0 | 6076 | `		if( ShowType ){` |
|     ! 0 | 6077 | `			SyBlobAppend(&(*pOut),")",sizeof(char));` |
|     ! 0 | 6078 | `		}` |
|     ! 0 | 6079 | `		return SXERR_LIMIT;` |
|       - | 6080 | `	}` |
|       - | 6081 | `	/* Point to the first inserted entry */` |
|      30 | 6082 | `	pEntry = pMap->pFirst;` |
|      30 | 6083 | `	rc = SXRET_OK;` |
|      30 | 6084 | `	if( !ShowType ){` |
|      15 | 6085 | `		SyBlobAppend(&(*pOut),"Array(",sizeof("Array(")-1);` |
|       7 | 6086 | `	}` |
|       - | 6087 | `	/* Total entries */` |
|      30 | 6088 | `	SyBlobFormat(&(*pOut),"%u) {",pMap->nEntry);` |
|       - | 6089 | `#ifdef __WINNT__` |
|       2 | 6090 | `	SyBlobAppend(&(*pOut),"\r\n",sizeof("\r\n")-1);` |
|       - | 6091 | `#else` |
|      28 | 6092 | `	SyBlobAppend(&(*pOut),"\n",sizeof(char));` |
|       - | 6093 | `#endif` |
|      65 | 6094 | `	for(;;){` |
|     132 | 6095 | `		if( n >= pMap->nEntry ){` |
|      30 | 6096 | `			break;` |
|       - | 6097 | `		}` |
|     206 | 6098 | `		for( i = 0 ; i < nTab ; i++ ){` |
|     104 | 6099 | `			SyBlobAppend(&(*pOut)," ",sizeof(char));` |
|      53 | 6100 | `		}` |
|       - | 6101 | `		/* Dump key */` |
|     104 | 6102 | `		if( pEntry->iType == HASHMAP_INT_NODE){` |
|      37 | 6103 | `			SyBlobFormat(&(*pOut),"[%qd] =>",pEntry->xKey.iKey);` |
|      19 | 6104 | `		}else{` |
|     101 | 6105 | `			SyBlobFormat(&(*pOut),"[%.*s] =>",` |
|      33 | 6106 | `				SyBlobLength(&pEntry->xKey.sKey),SyBlobData(&pEntry->xKey.sKey));` |
|       - | 6107 | `		}` |
|       - | 6108 | `#ifdef __WINNT__` |
|       2 | 6109 | `		SyBlobAppend(&(*pOut),"\r\n",sizeof("\r\n")-1);` |
|       - | 6110 | `#else` |
|     102 | 6111 | `		SyBlobAppend(&(*pOut),"\n",sizeof(char));` |
|       - | 6112 | `#endif` |
|       - | 6113 | `		/* Dump node value */` |
|     104 | 6114 | `		pObj = HashmapExtractNodeValue(pEntry);` |
|     104 | 6115 | `		isRef = 0;` |
|     104 | 6116 | `		if( pObj ){` |
|     104 | 6117 | `			if( pEntry->iFlags & HASHMAP_NODE_FOREIGN_OBJ ){` |
|       - | 6118 | `				/* Referenced object */` |
|     ! 0 | 6119 | `				isRef = 1;` |
|     ! 0 | 6120 | `			}` |
|     104 | 6121 | `			rc = PH7_MemObjDump(&(*pOut),pObj,ShowType,nTab+1,nDepth,isRef);` |
|     104 | 6122 | `			if( rc == SXERR_LIMIT ){` |
|     ! 0 | 6123 | `				break;` |
|       - | 6124 | `			}` |
|      51 | 6125 | `		}` |
|       - | 6126 | `		/* Point to the next entry */` |
|     104 | 6127 | `		n++;` |
|     104 | 6128 | `		pEntry = pEntry->pPrev; /* Reverse link */` |
|       2 | 6129 | `	}` |
|      58 | 6130 | `	for( i = 0 ; i < nTab ; i++ ){` |
|      30 | 6131 | `		SyBlobAppend(&(*pOut)," ",sizeof(char));` |
|      16 | 6132 | `	}` |
|      30 | 6133 | `	SyBlobAppend(&(*pOut),"}",sizeof(char));` |
|      30 | 6134 | `	return rc;` |
|      16 | 6135 |  |
|       - | 6136 | `/*` |
|       - | 6137 | ` * Iterate throw hashmap entries and invoke the given callback [i.e: xWalk()] for each` |
|       - | 6138 | ` * retrieved entry.` |
|       - | 6139 | ` * Note that argument are passed to the callback by copy. That is,any modification to` |
|       - | 6140 | ` * the entry value in the callback body will not alter the real value.` |
|       - | 6141 | ` * If the callback wishes to abort processing [i.e: it's invocation] it must return` |
|       - | 6142 | ` * a value different from PH7_OK.` |
|       - | 6143 | ` * Refer to [ph7_array_walk()] for more information.` |
|       - | 6144 | ` */` |
|   18258 | 6145 | `PH7_PRIVATE sxi32 PH7_HashmapWalk(` |
|       - | 6146 | `	ph7_hashmap *pMap, /* Target hashmap */` |
|       - | 6147 | `	int (*xWalk)(ph7_value *,ph7_value *,void *), /* Walker callback */` |
|       - | 6148 | `	void *pUserData /* Last argument to xWalk() */` |
|       - | 6149 | `	)` |
|       2 | 6150 |  |
|       - | 6151 | `	ph7_hashmap_node *pEntry;` |
|       - | 6152 | `	ph7_value sKey,sValue;` |
|       - | 6153 | `	sxi32 rc;` |
|       - | 6154 | `	sxu32 n;` |
|       - | 6155 | `	/* Initialize walker parameter */` |
|   18260 | 6156 | `	rc = SXRET_OK;` |
|   18260 | 6157 | `	PH7_MemObjInit(pMap->pVm,&sKey);` |
|   18260 | 6158 | `	PH7_MemObjInit(pMap->pVm,&sValue);` |
|   18260 | 6159 | `	n = pMap->nEntry;` |
|   18260 | 6160 | `	pEntry = pMap->pFirst;` |
|       - | 6161 | `	/* Start the iteration process */` |
|   49868 | 6162 | `	for(;;){` |
|   99738 | 6163 | `		if( n < 1 ){` |
|   18260 | 6164 | `			break;` |
|       - | 6165 | `		}` |
|       - | 6166 | `		/* Extract a copy of the key and a copy the current value */` |
|   81480 | 6167 | `		PH7_HashmapExtractNodeKey(pEntry,&sKey);` |
|   81480 | 6168 | `		PH7_HashmapExtractNodeValue(pEntry,&sValue,FALSE);` |
|       - | 6169 | `		/* Invoke the user callback */` |
|   81480 | 6170 | `		rc = xWalk(&sKey,&sValue,pUserData);` |
|       - | 6171 | `		/* Release the copy of the key and the value */` |
|   81480 | 6172 | `		PH7_MemObjRelease(&sKey);` |
|   81480 | 6173 | `		PH7_MemObjRelease(&sValue);` |
|   81480 | 6174 | `		if( rc != PH7_OK ){` |
|       - | 6175 | `			/* Callback request an operation abort */` |
|     ! 0 | 6176 | `			return SXERR_ABORT;` |
|       - | 6177 | `		}` |
|       - | 6178 | `		/* Point to the next entry */` |
|   81480 | 6179 | `		pEntry = pEntry->pPrev; /* Reverse link */` |
|   81480 | 6180 | `		n--;` |
|       2 | 6181 | `	}` |
|       - | 6182 | `	/* All done */` |
|   18260 | 6183 | `	return SXRET_OK;` |
|    9131 | 6184 |  |
|       - | 6185 |  |
