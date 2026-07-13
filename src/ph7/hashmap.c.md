# src/ph7/hashmap.c

<style>code, pre { background: none !important; white-space: pre !important; width: 100% !important; display: inline-block !important; } td { border: none !important; margin-top: 0 !important; margin-bottom: 0 !important; padding-top: 0 !important; padding-bottom: 0 !important; }</style>

Coverage: 3848/4301 lines (89.47%)

[Root index](../../index.md) | [Directory index](index.md)

|      Hits | Line | Source |
| --------: | ---: | :--- |
|         - |    1 | `/**` |
|         - |    2 | ` * SPDX-FileCopyrightText: 2011, 2012, 2013, 2014 Symisc Systems <licensing@symisc.net>` |
|         - |    3 | ` * SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>` |
|         - |    4 | ` * SPDX-License-Identifier: BSD-3-Clause` |
|         - |    5 | ` */` |
|         - |    6 | `#include "ph7int.h"` |
|         - |    7 | `/* range() formats the float variant of its max-array-size ValueError with libc` |
|         - |    8 | ` * snprintf and parses numeric strings with libc strtod — the byte-exact-floats` |
|         - |    9 | ` * rule (see builtin_math.c): SyBufferFormat/SyStrToReal are not correctly` |
|         - |   10 | ` * rounded at extreme magnitudes. */` |
|         - |   11 | `#include <stdio.h>  /* snprintf */` |
|         - |   12 | `#include <stdlib.h> /* strtod */` |
|         - |   13 | `/* This file implement generic hashmaps known as 'array' in the PHP world */` |
|         - |   14 | `/* HASHMAP_INT_NODE / HASHMAP_BLOB_NODE (node key types) are declared in ph7int.h` |
|         - |   15 | ` * alongside ph7_hashmap_node so name-forwarding builtins can classify keys. */` |
|         - |   16 | `/* Node control flags */` |
|         - |   17 | `#define HASHMAP_NODE_FOREIGN_OBJ 0x001 /* Node hold a reference to a foreign ph7_value` |
|         - |   18 | `                                        * [i.e: array(&var)/$a[] =& $var ]` |
|         - |   19 | `										*/` |
|         - |   20 | `/*` |
|         - |   21 | ` * Default hash function for int [i.e; 64-bit integer] keys.` |
|         - |   22 | ` */` |
|   7417158 |   23 | `static sxu32 IntHash(sxi64 iKey)` |
|         5 |   24 | `{` |
|   7417163 |   25 | `	sxu64 uKey = (sxu64)iKey; /* unsigned mixing: shifting a negative key is UB */` |
|   7417163 |   26 | `	return (sxu32)(uKey ^ (uKey << 8) ^ (uKey >> 8));` |
|         5 |   27 | `}` |
|         - |   28 | `/*` |
|         - |   29 | ` * Default hash function for string/BLOB keys.` |
|         - |   30 | ` */` |
|    577168 |   31 | `static sxu32 BinHash(const void *pSrc,sxu32 nLen)` |
|         5 |   32 | `{` |
|    577173 |   33 | `	register unsigned char *zIn = (unsigned char *)pSrc;` |
|         - |   34 | `	unsigned char *zEnd;` |
|    577173 |   35 | `	sxu32 nH = 5381;` |
|    577173 |   36 | `	zEnd = &zIn[nLen];` |
|    658507 |   37 | `	for(;;){` |
|   1317019 |   38 | `		if( zIn >= zEnd ){ break; } nH = nH * 33 + zIn[0] ; zIn++;` |
|   1118341 |   39 | `		if( zIn >= zEnd ){ break; } nH = nH * 33 + zIn[0] ; zIn++;` |
|   1000967 |   40 | `		if( zIn >= zEnd ){ break; } nH = nH * 33 + zIn[0] ; zIn++;` |
|    864031 |   41 | `		if( zIn >= zEnd ){ break; } nH = nH * 33 + zIn[0] ; zIn++;` |
|         5 |   42 | `	}` |
|    577173 |   43 | `	return nH;` |
|         5 |   44 | `}` |
|         - |   45 | `/*` |
|         - |   46 | ` * Return the total number of entries in a given hashmap.` |
|         - |   47 | ` * If bRecursive is set to TRUE then recurse on hashmap entries.` |
|         - |   48 | ` * Self-referential arrays are detected via the HASHMAP_COUNTING flag;` |
|         - |   49 | ` * when a cycle is found the nested array is skipped and *pCycleDetected` |
|         - |   50 | ` * is set to TRUE so the caller can emit a warning.` |
|         - |   51 | ` */` |
|      1254 |   52 | `static sxi64 HashmapCount(ph7_hashmap *pMap,int bRecursive,int *pCycleDetected)` |
|         5 |   53 | `{` |
|      1259 |   54 | `	sxi64 iCount = 0;` |
|      1259 |   55 | `	if( !bRecursive ){` |
|      1085 |   56 | `		iCount = pMap->nEntry;` |
|       545 |   57 | `	}else{` |
|         - |   58 | `		/* Recursive hashmap walk */` |
|       175 |   59 | `		ph7_hashmap_node *pEntry = pMap->pLast;` |
|         - |   60 | `		ph7_value *pElem;` |
|       175 |   61 | `		sxu32 n = 0;` |
|         - |   62 | `		/* Mark this map as being counted */` |
|       175 |   63 | `		pMap->iFlags \|= HASHMAP_COUNTING;` |
|       209 |   64 | `		for(;;){` |
|       419 |   65 | `			if( n >= pMap->nEntry ){` |
|       175 |   66 | `				break;` |
|         - |   67 | `			}` |
|         - |   68 | `			/* Point to the element value */` |
|       245 |   69 | `			pElem = (ph7_value *)SySetAt(&pMap->pVm->aMemObj,pEntry->nValIdx);` |
|       245 |   70 | `			if( pElem ){` |
|       245 |   71 | `				if( pElem->iFlags & MEMOBJ_HASHMAP ){` |
|       151 |   72 | `					ph7_hashmap *pSub = (ph7_hashmap *)pElem->x.pOther;` |
|       151 |   73 | `					if( pSub->iFlags & HASHMAP_COUNTING ){` |
|         - |   74 | `						/* Cycle detected — skip this entry */` |
|         3 |   75 | `						if( pCycleDetected ){` |
|         3 |   76 | `							*pCycleDetected = TRUE;` |
|         1 |   77 | `						}` |
|         2 |   78 | `					}else{` |
|       149 |   79 | `						iCount += HashmapCount(pSub,TRUE,pCycleDetected);` |
|         - |   80 | `					}` |
|        75 |   81 | `				}` |
|       122 |   82 | `			}` |
|         - |   83 | `			/* Point to the next entry */` |
|       245 |   84 | `			pEntry = pEntry->pNext;` |
|       245 |   85 | `			++n;` |
|         1 |   86 | `		}` |
|         - |   87 | `		/* Clear the counting flag */` |
|       175 |   88 | `		pMap->iFlags &= ~HASHMAP_COUNTING;` |
|         - |   89 | `		/* Update count */` |
|       175 |   90 | `		iCount += pMap->nEntry;` |
|         - |   91 | `	}` |
|      1259 |   92 | `	return iCount;` |
|         5 |   93 | `}` |
|         - |   94 | `/*` |
|         - |   95 | ` * Allocate a new hashmap node with a 64-bit integer key.` |
|         - |   96 | ` * If something goes wrong [i.e: out of memory],this function return NULL.` |
|         - |   97 | ` * Otherwise a fresh [ph7_hashmap_node] instance is returned.` |
|         - |   98 | ` */` |
|   3119868 |   99 | `static ph7_hashmap_node * HashmapNewIntNode(ph7_hashmap *pMap,sxi64 iKey,sxu32 nHash,sxu32 nValIdx)` |
|         5 |  100 | `{` |
|         - |  101 | `	ph7_hashmap_node *pNode;` |
|         - |  102 | `	/* Allocate a new node */` |
|   3119873 |  103 | `	pNode = (ph7_hashmap_node *)SyMemBackendPoolAlloc(&pMap->pVm->sAllocator,sizeof(ph7_hashmap_node));` |
|   3119873 |  104 | `	if( pNode == 0 ){` |
|       ! 0 |  105 | `		return 0;` |
|         - |  106 | `	}` |
|         - |  107 | `	/* Zero the stucture */` |
|   3119873 |  108 | `	SyZero(pNode,sizeof(ph7_hashmap_node));` |
|         - |  109 | `	/* Fill in the structure */` |
|   3119873 |  110 | `	pNode->pMap  = &(*pMap);` |
|   3119873 |  111 | `	pNode->iType = HASHMAP_INT_NODE;` |
|   3119873 |  112 | `	pNode->nHash = nHash;` |
|   3119873 |  113 | `	pNode->xKey.iKey = iKey;` |
|   3119873 |  114 | `	pNode->nValIdx  = nValIdx;` |
|   3119873 |  115 | `	return pNode;` |
|   1559939 |  116 | `}` |
|         - |  117 | `/*` |
|         - |  118 | ` * Allocate a new hashmap node with a BLOB key.` |
|         - |  119 | ` * If something goes wrong [i.e: out of memory],this function return NULL.` |
|         - |  120 | ` * Otherwise a fresh [ph7_hashmap_node] instance is returned.` |
|         - |  121 | ` */` |
|    239144 |  122 | `static ph7_hashmap_node * HashmapNewBlobNode(ph7_hashmap *pMap,const void *pKey,sxu32 nKeyLen,sxu32 nHash,sxu32 nValIdx)` |
|         5 |  123 | `{` |
|         - |  124 | `	ph7_hashmap_node *pNode;` |
|         - |  125 | `	/* Allocate a new node */` |
|    239149 |  126 | `	pNode = (ph7_hashmap_node *)SyMemBackendPoolAlloc(&pMap->pVm->sAllocator,sizeof(ph7_hashmap_node));` |
|    239149 |  127 | `	if( pNode == 0 ){` |
|       ! 0 |  128 | `		return 0;` |
|         - |  129 | `	}` |
|         - |  130 | `	/* Zero the stucture */` |
|    239149 |  131 | `	SyZero(pNode,sizeof(ph7_hashmap_node));` |
|         - |  132 | `	/* Fill in the structure */` |
|    239149 |  133 | `	pNode->pMap  = &(*pMap);` |
|    239149 |  134 | `	pNode->iType = HASHMAP_BLOB_NODE;` |
|    239149 |  135 | `	pNode->nHash = nHash;` |
|    239149 |  136 | `	SyBlobInit(&pNode->xKey.sKey,&pMap->pVm->sAllocator);` |
|    239149 |  137 | `	SyBlobAppend(&pNode->xKey.sKey,pKey,nKeyLen);` |
|    239149 |  138 | `	pNode->nValIdx = nValIdx;` |
|    239149 |  139 | `	return pNode;` |
|    119577 |  140 | `}` |
|         - |  141 | `/*` |
|         - |  142 | ` * link a hashmap node to the given bucket index (last argument to this function).` |
|         - |  143 | ` */` |
|   3359012 |  144 | `static void HashmapNodeLink(ph7_hashmap *pMap,ph7_hashmap_node *pNode,sxu32 nBucketIdx)` |
|         5 |  145 | `{` |
|         - |  146 | `	/* Link */` |
|   3359017 |  147 | `	if( pMap->apBucket[nBucketIdx] != 0 ){` |
|   2909369 |  148 | `		pNode->pNextCollide = pMap->apBucket[nBucketIdx];` |
|   2909369 |  149 | `		pMap->apBucket[nBucketIdx]->pPrevCollide = pNode;` |
|   1454682 |  150 | `	}` |
|   3359017 |  151 | `	pMap->apBucket[nBucketIdx] = pNode;` |
|         - |  152 | `	/* Link to the map list */` |
|   3359017 |  153 | `	if( pMap->pFirst == 0 ){` |
|     86565 |  154 | `		pMap->pFirst = pMap->pLast = pNode;` |
|         - |  155 | `		/* Point to the first inserted node */` |
|     86565 |  156 | `		pMap->pCur = pNode;` |
|     43285 |  157 | `	}else{` |
|   3272457 |  158 | `		MACRO_LD_PUSH(pMap->pLast,pNode);` |
|         - |  159 | `	}` |
|   3359017 |  160 | `	++pMap->nEntry;` |
|   3359017 |  161 | `}` |
|         - |  162 | `/*` |
|         - |  163 | ` * Unlink a node from the hashmap.` |
|         - |  164 | ` * If the node count reaches zero then release the whole hash-bucket.` |
|         - |  165 | ` */` |
|      7424 |  166 | `PH7_PRIVATE void PH7_HashmapUnlinkNode(ph7_hashmap_node *pNode,int bRestore)` |
|         5 |  167 | `{` |
|      7429 |  168 | `	ph7_hashmap *pMap = pNode->pMap;` |
|      7429 |  169 | `	ph7_vm *pVm = pMap->pVm;` |
|         - |  170 | `	/* Unlink from the corresponding bucket */` |
|      7429 |  171 | `	if( pNode->pPrevCollide == 0 ){` |
|      6957 |  172 | `		pMap->apBucket[pNode->nHash & (pMap->nSize - 1)] = pNode->pNextCollide;` |
|      3481 |  173 | `	}else{` |
|       474 |  174 | `		pNode->pPrevCollide->pNextCollide = pNode->pNextCollide;` |
|         - |  175 | `	}` |
|      7429 |  176 | `	if( pNode->pNextCollide ){` |
|      4517 |  177 | `		pNode->pNextCollide->pPrevCollide = pNode->pPrevCollide;` |
|      2257 |  178 | `	}` |
|      7429 |  179 | `	if( pMap->pFirst == pNode ){` |
|       131 |  180 | `		pMap->pFirst = pNode->pPrev;` |
|        63 |  181 | `	}` |
|      7429 |  182 | `	if( pMap->pCur == pNode ){` |
|         - |  183 | `		/* Advance the node cursor */` |
|       133 |  184 | `		pMap->pCur = pMap->pCur->pPrev; /* Reverse link */` |
|        64 |  185 | `	}` |
|         - |  186 | `	/* Unlink from the map list */` |
|      7429 |  187 | `	MACRO_LD_REMOVE(pMap->pLast,pNode);` |
|      7429 |  188 | `	if( bRestore ){` |
|         - |  189 | `		/* Remove the ph7_value associated with this node from the reference table */` |
|       107 |  190 | `		PH7_VmRefObjRemove(pVm,pNode->nValIdx,0,pNode);` |
|         - |  191 | `		/* Restore to the freelist */` |
|       107 |  192 | `		if( (pNode->iFlags & HASHMAP_NODE_FOREIGN_OBJ) == 0 ){` |
|       107 |  193 | `			PH7_VmUnsetMemObj(pVm,pNode->nValIdx,FALSE);` |
|        51 |  194 | `		}` |
|        51 |  195 | `	}` |
|      7429 |  196 | `	if( pNode->iType == HASHMAP_BLOB_NODE ){` |
|      7292 |  197 | `		SyBlobRelease(&pNode->xKey.sKey);` |
|      3644 |  198 | `	}` |
|      7429 |  199 | `	SyMemBackendPoolFree(&pVm->sAllocator,pNode);` |
|      7429 |  200 | `	pMap->nEntry--;` |
|      7429 |  201 | `	if( pMap->nEntry < 1 && pMap != pVm->pGlobal ){` |
|         - |  202 | `		/* Free the hash-bucket */` |
|        75 |  203 | `		SyMemBackendFree(&pVm->sAllocator,pMap->apBucket);` |
|        75 |  204 | `		pMap->apBucket = 0;` |
|        75 |  205 | `		pMap->nSize = 0;` |
|        75 |  206 | `		pMap->pFirst = pMap->pLast = pMap->pCur = 0;` |
|        35 |  207 | `	}` |
|      7429 |  208 | `}` |
|         - |  209 | `#define HASHMAP_FILL_FACTOR 3` |
|         - |  210 | `/*` |
|         - |  211 | ` * Grow the hash-table and rehash all entries.` |
|         - |  212 | ` */` |
|   3359012 |  213 | `static sxi32 HashmapGrowBucket(ph7_hashmap *pMap)` |
|         5 |  214 | `{` |
|   3359017 |  215 | `	if( pMap->nEntry >= pMap->nSize * HASHMAP_FILL_FACTOR ){` |
|     91423 |  216 | `		ph7_hashmap_node **apOld = pMap->apBucket;` |
|         - |  217 | `		ph7_hashmap_node *pEntry,**apNew;` |
|     91423 |  218 | `		sxu32 nNew = pMap->nSize << 1;` |
|         - |  219 | `		sxu32 nBucket;` |
|         - |  220 | `		sxu32 n;` |
|     91423 |  221 | `		if( nNew < 1 ){` |
|     86565 |  222 | `			nNew = 16;` |
|     43280 |  223 | `		}` |
|         - |  224 | `		/* Allocate a new bucket */` |
|     91423 |  225 | `		apNew = (ph7_hashmap_node **)SyMemBackendAlloc(&pMap->pVm->sAllocator,nNew * sizeof(ph7_hashmap_node *));` |
|     91423 |  226 | `		if( apNew == 0 ){` |
|       ! 0 |  227 | `			if( pMap->nSize < 1 ){` |
|       ! 0 |  228 | `				return SXERR_MEM; /* Fatal */` |
|         - |  229 | `			}` |
|         - |  230 | `			/* Not so fatal here,simply a performance hit */` |
|       ! 0 |  231 | `			return SXRET_OK;` |
|         - |  232 | `		}` |
|         - |  233 | `		/* Zero the table */` |
|     91423 |  234 | `		SyZero((void *)apNew,nNew * sizeof(ph7_hashmap_node *));` |
|         - |  235 | `		/* Reflect the change */` |
|     91423 |  236 | `		pMap->apBucket = apNew;` |
|     91423 |  237 | `		pMap->nSize = nNew;` |
|     91423 |  238 | `		if( apOld == 0 ){` |
|         - |  239 | `			/* First allocated table [i.e: no entry],return immediately */` |
|     86565 |  240 | `			return SXRET_OK;` |
|         - |  241 | `		}` |
|         - |  242 | `		/* Rehash old entries */` |
|      4863 |  243 | `		pEntry = pMap->pFirst;` |
|      4863 |  244 | `		n = 0;` |
|   2097341 |  245 | `		for( ;; ){` |
|   4194687 |  246 | `			if( n >= pMap->nEntry ){` |
|      4863 |  247 | `				break;` |
|         - |  248 | `			}` |
|         - |  249 | `			/* Clear the old collision link */` |
|   4189829 |  250 | `			pEntry->pNextCollide = pEntry->pPrevCollide = 0;` |
|         - |  251 | `			/* Link to the new bucket */` |
|   4189829 |  252 | `			nBucket = pEntry->nHash & (nNew - 1);` |
|   4189829 |  253 | `			if( pMap->apBucket[nBucket] != 0 ){` |
|   3584551 |  254 | `				pEntry->pNextCollide = pMap->apBucket[nBucket];` |
|   3584551 |  255 | `				pMap->apBucket[nBucket]->pPrevCollide = pEntry;` |
|   1792273 |  256 | `			}` |
|   4189829 |  257 | `			pMap->apBucket[nBucket] = pEntry;` |
|         - |  258 | `			/* Point to the next entry */` |
|   4189829 |  259 | `			pEntry = pEntry->pPrev; /* Reverse link */` |
|   4189829 |  260 | `			n++;` |
|         5 |  261 | `		}` |
|         - |  262 | `		/* Free the old table */` |
|      4863 |  263 | `		SyMemBackendFree(&pMap->pVm->sAllocator,(void *)apOld);` |
|      2429 |  264 | `	}` |
|   3272457 |  265 | `	return SXRET_OK;` |
|   1679511 |  266 | `}` |
|         - |  267 | `/*` |
|         - |  268 | ` * Insert a 64-bit integer key and it's associated value (if any) in the given` |
|         - |  269 | ` * hashmap.` |
|         - |  270 | ` */` |
|   3119868 |  271 | `static sxi32 HashmapInsertIntKey(ph7_hashmap *pMap,sxi64 iKey,ph7_value *pValue,sxu32 nRefIdx,int isForeign)` |
|         5 |  272 | `{` |
|         - |  273 | `	ph7_hashmap_node *pNode;` |
|         - |  274 | `	sxu32 nIdx;` |
|         - |  275 | `	sxu32 nHash;` |
|         - |  276 | `	sxi32 rc;` |
|   3119873 |  277 | `	if( !isForeign ){` |
|         - |  278 | `		ph7_value *pObj;` |
|         - |  279 | `		ph7_value sSafeVal;` |
|         - |  280 | `		/* Snapshot the source BEFORE reserving: PH7_ReserveMemObj can grow (move)` |
|         - |  281 | `		 * pVm->aMemObj, which would dangle pValue when it points into the pool` |
|         - |  282 | `		 * (e.g. get_defined_vars/func_get_args/get_class_vars/get_object_vars pass` |
|         - |  283 | `		 * a pool slot). A shallow copy is a safe PH7_MemObjStore source — the` |
|         - |  284 | `		 * referent and the heap-resident blob data survive the move; only the` |
|         - |  285 | `		 * ph7_value struct relocates (same sSafeVal idiom used by PH7_HashmapDup). */` |
|   3119835 |  286 | `		if( pValue ){` |
|   3119833 |  287 | `			sSafeVal = *pValue;` |
|   3119833 |  288 | `			pValue = &sSafeVal;` |
|   1559914 |  289 | `		}` |
|         - |  290 | `		/* Reserve a ph7_value for the value */` |
|   3119835 |  291 | `		pObj = PH7_ReserveMemObj(pMap->pVm);` |
|   3119835 |  292 | `		if( pObj == 0 ){` |
|       ! 0 |  293 | `			return SXERR_MEM;` |
|         - |  294 | `		}` |
|   3119835 |  295 | `		if( pValue ){` |
|         - |  296 | `			/* Duplicate the value */` |
|   3119833 |  297 | `			PH7_MemObjStore(pValue,pObj);` |
|   1559914 |  298 | `		}` |
|   3119835 |  299 | `		nIdx = pObj->nIdx;` |
|   1559920 |  300 | `	}else{` |
|        39 |  301 | `		nIdx = nRefIdx;` |
|         - |  302 | `	}` |
|         - |  303 | `	/* Hash the key */` |
|   3119873 |  304 | `	nHash = pMap->xIntHash(iKey);` |
|         - |  305 | `	/* Allocate a new int node */` |
|   3119873 |  306 | `	pNode = HashmapNewIntNode(&(*pMap),iKey,nHash,nIdx);` |
|   3119873 |  307 | `	if( pNode == 0 ){` |
|       ! 0 |  308 | `		return SXERR_MEM;` |
|         - |  309 | `	}` |
|   3119873 |  310 | `	if( isForeign ){` |
|         - |  311 | `		/* Mark as a foregin entry */` |
|        39 |  312 | `		pNode->iFlags \|= HASHMAP_NODE_FOREIGN_OBJ;` |
|        19 |  313 | `	}` |
|         - |  314 | `	/* Make sure the bucket is big enough to hold the new entry */` |
|   3119873 |  315 | `	rc = HashmapGrowBucket(&(*pMap));` |
|   3119873 |  316 | `	if( rc != SXRET_OK ){` |
|       ! 0 |  317 | `		SyMemBackendPoolFree(&pMap->pVm->sAllocator,pNode);` |
|       ! 0 |  318 | `		return rc;` |
|         - |  319 | `	}` |
|         - |  320 | `	/* Perform the insertion */` |
|   3119873 |  321 | `	HashmapNodeLink(&(*pMap),pNode,nHash & (pMap->nSize - 1));` |
|         - |  322 | `	/* Install in the reference table */` |
|   3119873 |  323 | `	PH7_VmRefObjInstall(pMap->pVm,nIdx,0,pNode,0);` |
|         - |  324 | `	/* All done */` |
|   3119873 |  325 | `	return SXRET_OK;` |
|   1559939 |  326 | `}` |
|         - |  327 | `/*` |
|         - |  328 | ` * Insert a BLOB key and it's associated value (if any) in the given` |
|         - |  329 | ` * hashmap.` |
|         - |  330 | ` */` |
|    239144 |  331 | `static sxi32 HashmapInsertBlobKey(ph7_hashmap *pMap,const void *pKey,sxu32 nKeyLen,ph7_value *pValue,sxu32 nRefIdx,int isForeign)` |
|         5 |  332 | `{` |
|         - |  333 | `	ph7_hashmap_node *pNode;` |
|         - |  334 | `	sxu32 nHash;` |
|         - |  335 | `	sxu32 nIdx;` |
|         - |  336 | `	sxi32 rc;` |
|    239149 |  337 | `	if( !isForeign ){` |
|         - |  338 | `		ph7_value *pObj;` |
|         - |  339 | `		ph7_value sSafeVal;` |
|         - |  340 | `		/* Snapshot the source BEFORE reserving: PH7_ReserveMemObj can grow (move)` |
|         - |  341 | `		 * pVm->aMemObj, which would dangle pValue when it points into the pool` |
|         - |  342 | `		 * (e.g. get_defined_vars/func_get_args/get_class_vars/get_object_vars pass` |
|         - |  343 | `		 * a pool slot). A shallow copy is a safe PH7_MemObjStore source — the` |
|         - |  344 | `		 * referent and the heap-resident blob data survive the move; only the` |
|         - |  345 | `		 * ph7_value struct relocates (same sSafeVal idiom used by PH7_HashmapDup). */` |
|    192691 |  346 | `		if( pValue ){` |
|    192401 |  347 | `			sSafeVal = *pValue;` |
|    192401 |  348 | `			pValue = &sSafeVal;` |
|     96198 |  349 | `		}` |
|         - |  350 | `		/* Reserve a ph7_value for the value */` |
|    192691 |  351 | `		pObj = PH7_ReserveMemObj(pMap->pVm);` |
|    192691 |  352 | `		if( pObj == 0 ){` |
|       ! 0 |  353 | `			return SXERR_MEM;` |
|         - |  354 | `		}` |
|    192691 |  355 | `		if( pValue ){` |
|         - |  356 | `			/* Duplicate the value */` |
|    192401 |  357 | `			PH7_MemObjStore(pValue,pObj);` |
|     96198 |  358 | `		}` |
|    192691 |  359 | `		nIdx = pObj->nIdx;` |
|     96348 |  360 | `	}else{` |
|     46463 |  361 | `		nIdx = nRefIdx;` |
|         - |  362 | `	}` |
|         - |  363 | `	/* Hash the key */` |
|    239149 |  364 | `	nHash = pMap->xBlobHash(pKey,nKeyLen);` |
|         - |  365 | `	/* Allocate a new blob node */` |
|    239149 |  366 | `	pNode = HashmapNewBlobNode(&(*pMap),pKey,nKeyLen,nHash,nIdx);` |
|    239149 |  367 | `	if( pNode == 0 ){` |
|       ! 0 |  368 | `		return SXERR_MEM;` |
|         - |  369 | `	}` |
|    239149 |  370 | `	if( isForeign ){` |
|         - |  371 | `		/* Mark as a foregin entry */` |
|     46463 |  372 | `		pNode->iFlags \|= HASHMAP_NODE_FOREIGN_OBJ;` |
|     23229 |  373 | `	}` |
|         - |  374 | `	/* Make sure the bucket is big enough to hold the new entry */` |
|    239149 |  375 | `	rc = HashmapGrowBucket(&(*pMap));` |
|    239149 |  376 | `	if( rc != SXRET_OK ){` |
|       ! 0 |  377 | `		SyMemBackendPoolFree(&pMap->pVm->sAllocator,pNode);` |
|       ! 0 |  378 | `		return rc;` |
|         - |  379 | `	}` |
|         - |  380 | `	/* Perform the insertion */` |
|    239149 |  381 | `	HashmapNodeLink(&(*pMap),pNode,nHash & (pMap->nSize - 1));` |
|         - |  382 | `	/* Install in the reference table */` |
|    239149 |  383 | `	PH7_VmRefObjInstall(pMap->pVm,nIdx,0,pNode,0);` |
|         - |  384 | `	/* All done */` |
|    239149 |  385 | `	return SXRET_OK;` |
|    119577 |  386 | `}` |
|         - |  387 | `/*` |
|         - |  388 | ` * Check if a given 64-bit integer key exists in the given hashmap.` |
|         - |  389 | ` * Write a pointer to the target node on success. Otherwise` |
|         - |  390 | ` * SXERR_NOTFOUND is returned on failure.` |
|         - |  391 | ` */` |
|   4284330 |  392 | `static sxi32 HashmapLookupIntKey(` |
|         - |  393 | `	ph7_hashmap *pMap,         /* Target hashmap */` |
|         - |  394 | `	sxi64 iKey,                /* lookup key */` |
|         - |  395 | `	ph7_hashmap_node **ppNode  /* OUT: target node on success */` |
|         - |  396 | `	)` |
|         5 |  397 | `{` |
|         - |  398 | `	ph7_hashmap_node *pNode;` |
|         - |  399 | `	sxu32 nHash;` |
|   4284335 |  400 | `	if( pMap->nEntry < 1 ){` |
|         - |  401 | `		/* Don't bother hashing,there is no entry anyway */` |
|       587 |  402 | `		return SXERR_NOTFOUND;` |
|         - |  403 | `	}` |
|         - |  404 | `	/* Hash the key first */` |
|   4283753 |  405 | `	nHash = pMap->xIntHash(iKey);` |
|         - |  406 | `	/* Point to the appropriate bucket */` |
|   4283753 |  407 | `	pNode = pMap->apBucket[nHash & (pMap->nSize - 1)];` |
|         - |  408 | `	/* Perform the lookup */` |
| 110562405 |  409 | `	for(;;){` |
| 221124815 |  410 | `		if( pNode == 0 ){` |
|   4281163 |  411 | `			break;` |
|         - |  412 | `		}` |
| 216843652 |  413 | `		if( pNode->iType == HASHMAP_INT_NODE` |
| 216840642 |  414 | `			&& pNode->nHash == nHash` |
| 108420116 |  415 | `			&& pNode->xKey.iKey == iKey ){` |
|         - |  416 | `				/* Node found */` |
|      2595 |  417 | `				if( ppNode ){` |
|      2577 |  418 | `					*ppNode = pNode;` |
|      1286 |  419 | `				}` |
|      2595 |  420 | `				return SXRET_OK;` |
|         - |  421 | `		}` |
|         - |  422 | `		/* Follow the collision link */` |
| 216841063 |  423 | `		pNode = pNode->pNextCollide;` |
|         1 |  424 | `	}` |
|         - |  425 | `	/* No such entry */` |
|   4281163 |  426 | `	return SXERR_NOTFOUND;` |
|   2142170 |  427 | `}` |
|         - |  428 | `/*` |
|         - |  429 | ` * Check if a given BLOB key exists in the given hashmap.` |
|         - |  430 | ` * Write a pointer to the target node on success. Otherwise` |
|         - |  431 | ` * SXERR_NOTFOUND is returned on failure.` |
|         - |  432 | ` */` |
|    371748 |  433 | `static sxi32 HashmapLookupBlobKey(` |
|         - |  434 | `	ph7_hashmap *pMap,          /* Target hashmap */` |
|         - |  435 | `	const void *pKey,           /* Lookup key */` |
|         - |  436 | `	sxu32 nKeyLen,              /* Key length in bytes */` |
|         - |  437 | `	ph7_hashmap_node **ppNode   /* OUT: target node on success */` |
|         - |  438 | `	)` |
|         5 |  439 | `{` |
|         - |  440 | `	ph7_hashmap_node *pNode;` |
|         - |  441 | `	sxu32 nHash;` |
|    371753 |  442 | `	if( pMap->nEntry < 1 ){` |
|         - |  443 | `		/* Don't bother hashing,there is no entry anyway */` |
|     33729 |  444 | `		return SXERR_NOTFOUND;` |
|         - |  445 | `	}` |
|         - |  446 | `	/* Hash the key first */` |
|    338029 |  447 | `	nHash = pMap->xBlobHash(pKey,nKeyLen);` |
|         - |  448 | `	/* Point to the appropriate bucket */` |
|    338029 |  449 | `	pNode = pMap->apBucket[nHash & (pMap->nSize - 1)];` |
|         - |  450 | `	/* Perform the lookup */` |
|    277646 |  451 | `	for(;;){` |
|    555297 |  452 | `		if( pNode == 0 ){` |
|    280513 |  453 | `			break;` |
|         - |  454 | `		}` |
|    274784 |  455 | `		if( pNode->iType == HASHMAP_BLOB_NODE` |
|    273279 |  456 | `			&& pNode->nHash == nHash` |
|    164693 |  457 | `			&& SyBlobLength(&pNode->xKey.sKey) == nKeyLen` |
|     57617 |  458 | `			&& SyMemcmp(SyBlobData(&pNode->xKey.sKey),pKey,nKeyLen) == 0 ){` |
|         - |  459 | `				/* Node found */` |
|     57521 |  460 | `				if( ppNode ){` |
|     57493 |  461 | `					*ppNode = pNode;` |
|     28744 |  462 | `				}` |
|     57521 |  463 | `				return SXRET_OK;` |
|         - |  464 | `		}` |
|         - |  465 | `		/* Follow the collision link */` |
|    217273 |  466 | `		pNode = pNode->pNextCollide;` |
|         5 |  467 | `	}` |
|         - |  468 | `	/* No such entry */` |
|    280513 |  469 | `	return SXERR_NOTFOUND;` |
|    185879 |  470 | `}` |
|         - |  471 | `/*` |
|         - |  472 | ` * Check if the given BLOB key looks like a decimal number.` |
|         - |  473 | ` * Retrurn TRUE on success.FALSE otherwise.` |
|         - |  474 | ` */` |
|    371882 |  475 | `static int HashmapIsIntKey(SyBlob *pKey)` |
|         5 |  476 | `{` |
|    371887 |  477 | `	const char *zIn  = (const char *)SyBlobData(pKey);` |
|    371887 |  478 | `	const char *zEnd = &zIn[SyBlobLength(pKey)];` |
|         - |  479 | `	const char *zDigit;` |
|    371887 |  480 | `	int isNeg = FALSE, nDigit;` |
|    371887 |  481 | `	if( zIn >= zEnd ){` |
|       ! 0 |  482 | `		return FALSE;` |
|         - |  483 | `	}` |
|    371887 |  484 | `	if( (int)(zEnd-zIn) > 1 && zIn[0] == '0' ){` |
|         - |  485 | `		/* Octal not decimal number */` |
|         5 |  486 | `		return FALSE;` |
|         - |  487 | `	}` |
|    371883 |  488 | `	if( (zIn[0] == '-' \|\| zIn[0] == '+') && &zIn[1] < zEnd ){` |
|         5 |  489 | `		isNeg = (zIn[0] == '-');` |
|         5 |  490 | `		zIn++;` |
|         2 |  491 | `	}` |
|    371883 |  492 | `	zDigit = zIn;` |
|    186371 |  493 | `	for(;;){` |
|    372747 |  494 | `		if( zIn >= zEnd ){` |
|       249 |  495 | `			break;` |
|         - |  496 | `		}` |
|    372499 |  497 | `		if( (unsigned char)zIn[0] >= 0xc0 /* UTF-8 stream */  \|\| !SyisDigit(zIn[0]) ){` |
|         - |  498 | `			/* Key does not look like a decimal number */` |
|    371635 |  499 | `			return FALSE;` |
|         - |  500 | `		}` |
|       865 |  501 | `		zIn++;` |
|         1 |  502 | `	}` |
|         - |  503 | `	/* An all-digit key that overflows the signed 64-bit range is NOT an integer` |
|         - |  504 | `	 * key: php keeps it a string key (its (string)(int)$k === $k round-trip` |
|         - |  505 | `	 * fails). Treating it as an int would let PH7_MemObjToInteger saturate it to` |
|         - |  506 | `	 * PHP_INT_MAX/MIN and collide with the genuine boundary key. */` |
|       249 |  507 | `	nDigit = (int)(zEnd - zDigit);` |
|       249 |  508 | `	if( nDigit < 1 ){` |
|         - |  509 | `		/* A lone sign ("-"/"+") */` |
|       ! 0 |  510 | `		return FALSE;` |
|         - |  511 | `	}` |
|       253 |  512 | `	if( nDigit > 19 \|\|` |
|       127 |  513 | `		(nDigit == 19 && SyMemcmp(zDigit, isNeg ? "9223372036854775808" : "9223372036854775807", 19) > 0) ){` |
|         7 |  514 | `		return FALSE;` |
|         - |  515 | `	}` |
|       243 |  516 | `	return TRUE;` |
|    185946 |  517 | `}` |
|         - |  518 | `/*` |
|         - |  519 | ` * Check if a given key exists in the given hashmap.` |
|         - |  520 | ` * Write a pointer to the target node on success.` |
|         - |  521 | ` * Otherwise SXERR_NOTFOUND is returned on failure.` |
|         - |  522 | ` */` |
|    135206 |  523 | `static sxi32 HashmapLookup(` |
|         - |  524 | `	ph7_hashmap *pMap,          /* Target hashmap */` |
|         - |  525 | `	ph7_value *pKey,            /* Lookup key */` |
|         - |  526 | `	ph7_hashmap_node **ppNode   /* OUT: target node on success */` |
|         - |  527 | `	)` |
|         5 |  528 | `{` |
|    135211 |  529 | `	ph7_hashmap_node *pNode = 0; /* cc -O6 warning */` |
|         - |  530 | `	sxi32 rc;` |
|    135211 |  531 | `	if( pKey->iFlags & (MEMOBJ_STRING\|MEMOBJ_HASHMAP\|MEMOBJ_OBJ\|MEMOBJ_RES) ){` |
|    132847 |  532 | `		if( (pKey->iFlags & MEMOBJ_STRING) == 0 ){` |
|         - |  533 | `			/* Force a string cast */` |
|       ! 0 |  534 | `			PH7_MemObjToString(&(*pKey));` |
|       ! 0 |  535 | `		}` |
|    132847 |  536 | `		if( SyBlobLength(&pKey->sBlob) > 0 && !HashmapIsIntKey(&pKey->sBlob) ){` |
|         - |  537 | `			/* Perform a blob lookup */` |
|    132827 |  538 | `			rc = HashmapLookupBlobKey(&(*pMap),SyBlobData(&pKey->sBlob),SyBlobLength(&pKey->sBlob),&pNode);` |
|    132827 |  539 | `			goto result;` |
|         - |  540 | `		}` |
|        10 |  541 | `	}` |
|         - |  542 | `	/* Perform an int lookup */` |
|      2389 |  543 | `	if((pKey->iFlags & MEMOBJ_INT) == 0 ){` |
|         - |  544 | `		/* Force an integer cast */` |
|        35 |  545 | `		PH7_MemObjToInteger(pKey);` |
|        17 |  546 | `	}` |
|         - |  547 | `	/* Perform an int lookup */` |
|      2389 |  548 | `	rc = HashmapLookupIntKey(&(*pMap),pKey->x.iVal,&pNode);` |
|     67603 |  549 | `result:` |
|    135211 |  550 | `	if( rc == SXRET_OK ){` |
|         - |  551 | `		/* Node found */` |
|     59465 |  552 | `		if( ppNode ){` |
|     59419 |  553 | `			*ppNode = pNode;` |
|     29707 |  554 | `		}` |
|     59465 |  555 | `		return SXRET_OK;` |
|         - |  556 | `	}` |
|         - |  557 | `	/* No such entry */` |
|     75751 |  558 | `	return SXERR_NOTFOUND;` |
|     67608 |  559 | `}` |
|         - |  560 | `/*` |
|         - |  561 | ` * Advance the auto-index after a successful insertion of int key iKey.` |
|         - |  562 | ` * Mirrors Zend's nNextFreeElement: saturates at PHP_INT_MAX (incrementing` |
|         - |  563 | ` * past it is signed overflow); the occupied-slot case errors at append time` |
|         - |  564 | ` * via HashmapAppendIndexBusy.` |
|         - |  565 | ` */` |
|   2140986 |  566 | `static void HashmapAdvanceAutoIndex(ph7_hashmap *pMap,sxi64 iKey)` |
|         5 |  567 | `{` |
|   2140991 |  568 | `	if( iKey >= pMap->iNextIdx ){` |
|   2140729 |  569 | `		pMap->iNextIdx = iKey < SXI64_HIGH ? iKey + 1 : SXI64_HIGH;` |
|         - |  570 | `		/* Make sure the automatic index is not reserved */` |
|   2140729 |  571 | `		while( pMap->iNextIdx < SXI64_HIGH && SXRET_OK == HashmapLookupIntKey(&(*pMap),pMap->iNextIdx,0) ){` |
|       ! 0 |  572 | `			pMap->iNextIdx++;` |
|       ! 0 |  573 | `		}` |
|   1070362 |  574 | `	}` |
|   2140991 |  575 | `}` |
|         - |  576 | `/*` |
|         - |  577 | `` * TRUE when an append (`$a[] = v`) cannot proceed because the saturated`` |
|         - |  578 | ` * auto-index slot (PHP_INT_MAX) is already occupied. Throws php's catchable` |
|         - |  579 | ` * Error and stores the rc the insert function must return (PH7_EXCEPTION,` |
|         - |  580 | ` * or PH7_ABORT when the Error class itself cannot be built).` |
|         - |  581 | ` */` |
|    978542 |  582 | `static sxi32 HashmapAppendIndexBusy(ph7_hashmap *pMap,sxi32 *pRc)` |
|         5 |  583 | `{` |
|    978547 |  584 | `	if( pMap->iNextIdx == SXI64_HIGH && SXRET_OK == HashmapLookupIntKey(&(*pMap),pMap->iNextIdx,0) ){` |
|         7 |  585 | `		*pRc = PH7_VmThrowArrayNextIndexError(pMap->pVm);` |
|         7 |  586 | `		return TRUE;` |
|         - |  587 | `	}` |
|    978541 |  588 | `	return FALSE;` |
|    489276 |  589 | `}` |
|         - |  590 | `/*` |
|         - |  591 | ` * Insert a given key and it's associated value (if any) in the given` |
|         - |  592 | ` * hashmap.` |
|         - |  593 | ` * If a node with the given key already exists in the database` |
|         - |  594 | ` * then this function overwrite the old value.` |
|         - |  595 | ` */` |
|   3311934 |  596 | `static sxi32 HashmapInsert(` |
|         - |  597 | `	ph7_hashmap *pMap, /* Target hashmap */` |
|         - |  598 | `	ph7_value *pKey,   /* Lookup key  */` |
|         - |  599 | `	ph7_value *pVal    /* Node value */` |
|         - |  600 | `	)` |
|         5 |  601 | `{` |
|   3311939 |  602 | `	ph7_hashmap_node *pNode = 0;` |
|   3311939 |  603 | `	sxi32 rc = SXRET_OK;` |
|   3311939 |  604 | `	if( pKey && pKey->iFlags & (MEMOBJ_STRING\|MEMOBJ_HASHMAP\|MEMOBJ_OBJ\|MEMOBJ_RES) ){` |
|    196075 |  605 | `		if( (pKey->iFlags & MEMOBJ_STRING) == 0 ){` |
|         - |  606 | `			/* Force a string cast */` |
|         3 |  607 | `			PH7_MemObjToString(&(*pKey));` |
|         1 |  608 | `		}` |
|    196075 |  609 | `		if( SyBlobLength(&pKey->sBlob) < 1 \|\| HashmapIsIntKey(&pKey->sBlob) ){` |
|      3721 |  610 | `			if(SyBlobLength(&pKey->sBlob) < 1){` |
|         - |  611 | `				/* Automatic index assign */` |
|      3495 |  612 | `				pKey = 0;` |
|      1745 |  613 | `			}` |
|      3721 |  614 | `			goto IntKey;` |
|         - |  615 | `		}` |
|    288536 |  616 | `		if( SXRET_OK == HashmapLookupBlobKey(&(*pMap),SyBlobData(&pKey->sBlob),` |
|     96177 |  617 | `			SyBlobLength(&pKey->sBlob),&pNode) ){` |
|         - |  618 | `				/* Overwrite the old value */` |
|         - |  619 | `				ph7_value *pElem;` |
|       371 |  620 | `				pElem = (ph7_value *)SySetAt(&pMap->pVm->aMemObj,pNode->nValIdx);` |
|       371 |  621 | `				if( pElem ){` |
|       371 |  622 | `					if( pVal ){` |
|       371 |  623 | `						PH7_MemObjStore(pVal,pElem);` |
|       187 |  624 | `					}else{` |
|         - |  625 | `						/* Nullify the entry */` |
|       ! 0 |  626 | `						PH7_MemObjToNull(pElem);` |
|         - |  627 | `					}` |
|       184 |  628 | `				}` |
|       371 |  629 | `				return SXRET_OK;` |
|         - |  630 | `		}` |
|    191991 |  631 | `		if( pMap == pMap->pVm->pGlobal ){` |
|         - |  632 | `			/* php 8.1: writing a new key into $GLOBALS creates a real global` |
|         - |  633 | `			 * variable ($GLOBALS stays a live view of the symbol table). */` |
|       131 |  634 | `			if( SyBlobLength(&pKey->sBlob) < 1 ){` |
|         - |  635 | `				/* Pathological empty name: keep the legacy diagnostic */` |
|       ! 0 |  636 | `				PH7_VmThrowError(pMap->pVm,0,PH7_CTX_NOTICE,"$GLOBALS is a read-only array,insertion is forbidden");` |
|       ! 0 |  637 | `				return SXRET_OK;` |
|         - |  638 | `			}` |
|       196 |  639 | `			return PH7_VmInstallGlobalVar(pMap->pVm,` |
|       130 |  640 | `				(const char *)SyBlobData(&pKey->sBlob),SyBlobLength(&pKey->sBlob),` |
|        65 |  641 | `				pVal,SXU32_HIGH);` |
|         - |  642 | `		}` |
|         - |  643 | `		/* Perform a blob-key insertion */` |
|    191861 |  644 | `		rc = HashmapInsertBlobKey(&(*pMap),SyBlobData(&pKey->sBlob),SyBlobLength(&pKey->sBlob),&(*pVal),0,FALSE);` |
|    191861 |  645 | `		return rc;` |
|         - |  646 | `	}` |
|   1557932 |  647 | `IntKey:` |
|   3119585 |  648 | `	if( pKey ){` |
|   2141073 |  649 | `		if((pKey->iFlags & MEMOBJ_INT) == 0 ){` |
|         - |  650 | `			/* Force an integer cast */` |
|       259 |  651 | `			PH7_MemObjToInteger(pKey);` |
|       129 |  652 | `		}` |
|   2141073 |  653 | `		if( SXRET_OK == HashmapLookupIntKey(&(*pMap),pKey->x.iVal,&pNode) ){` |
|         - |  654 | `			/* Overwrite the old value */` |
|         - |  655 | `			ph7_value *pElem;` |
|        87 |  656 | `			pElem = (ph7_value *)SySetAt(&pMap->pVm->aMemObj,pNode->nValIdx);` |
|        87 |  657 | `			if( pElem ){` |
|        87 |  658 | `				if( pVal ){` |
|        87 |  659 | `					PH7_MemObjStore(pVal,pElem);` |
|        44 |  660 | `				}else{` |
|         - |  661 | `					/* Nullify the entry */` |
|       ! 0 |  662 | `					PH7_MemObjToNull(pElem);` |
|         - |  663 | `				}` |
|        43 |  664 | `			}` |
|        87 |  665 | `			return SXRET_OK;` |
|         - |  666 | `		}` |
|   2140987 |  667 | `		if( pMap == pMap->pVm->pGlobal ){` |
|         - |  668 | `			/* php 8.1: an int key creates the global named by its decimal` |
|         - |  669 | `			 * form ($GLOBALS[7] = ... behaves like $GLOBALS['7'] = ...). */` |
|         - |  670 | `			char zKey[24];` |
|         3 |  671 | `			sxu32 nKey = SyBufferFormat(zKey,sizeof(zKey),"%qd",pKey->x.iVal);` |
|         3 |  672 | `			return PH7_VmInstallGlobalVar(pMap->pVm,zKey,nKey,pVal,SXU32_HIGH);` |
|         - |  673 | `		}` |
|         - |  674 | `		/* Perform a 64-bit-int-key insertion */` |
|   2140985 |  675 | `		rc = HashmapInsertIntKey(&(*pMap),pKey->x.iVal,&(*pVal),0,FALSE);` |
|   2140985 |  676 | `		if( rc == SXRET_OK ){` |
|   2140985 |  677 | `			HashmapAdvanceAutoIndex(&(*pMap),pKey->x.iVal);` |
|   1070490 |  678 | `		}` |
|   1070495 |  679 | `	}else{` |
|    978517 |  680 | `		if( pMap == pMap->pVm->pGlobal ){` |
|         - |  681 | `			/* php's catchable Error: Cannot append to $GLOBALS */` |
|         3 |  682 | `			return PH7_VmThrowGlobalsAppendError(pMap->pVm);` |
|         - |  683 | `		}` |
|    978515 |  684 | `		if( HashmapAppendIndexBusy(&(*pMap),&rc) ){` |
|         7 |  685 | `			return rc; /* PH7_EXCEPTION/PH7_ABORT: php's catchable Error was thrown */` |
|         - |  686 | `		}` |
|         - |  687 | `		/* Assign an automatic index */` |
|    978509 |  688 | `		rc = HashmapInsertIntKey(&(*pMap),pMap->iNextIdx,&(*pVal),0,FALSE);` |
|    978509 |  689 | `		if( rc == SXRET_OK && pMap->iNextIdx < SXI64_HIGH ){` |
|    978507 |  690 | `			++pMap->iNextIdx;` |
|    489251 |  691 | `		}` |
|         - |  692 | `	}` |
|         - |  693 | `	/* Insertion result */` |
|   3119489 |  694 | `	return rc;` |
|   1655972 |  695 | `}` |
|         - |  696 | `/*` |
|         - |  697 | ` * Insert a given key and it's associated value (foreign index) in the given` |
|         - |  698 | ` * hashmap.` |
|         - |  699 | ` * This is insertion by reference so be careful to mark the node` |
|         - |  700 | ` * with the HASHMAP_NODE_FOREIGN_OBJ flag being set.` |
|         - |  701 | ` * The insertion by reference is triggered when the following` |
|         - |  702 | ` * expression is encountered.` |
|         - |  703 | ` * $var = 10;` |
|         - |  704 | ` *  $a = array(&var);` |
|         - |  705 | ` * OR` |
|         - |  706 | ` *  $a[] =& $var;` |
|         - |  707 | ` * That is,$var is a foreign ph7_value and the $a array have no control` |
|         - |  708 | ` * over it's contents.` |
|         - |  709 | ` * Note that the node that hold the foreign ph7_value is automatically` |
|         - |  710 | ` * removed when the foreign ph7_value is unset.` |
|         - |  711 | ` * Example:` |
|         - |  712 | ` *  $var = 10;` |
|         - |  713 | ` *  $a[] =& $var;` |
|         - |  714 | ` *  echo count($a).PHP_EOL; //1` |
|         - |  715 | ` *  //Unset the foreign ph7_value now` |
|         - |  716 | ` *  unset($var);` |
|         - |  717 | ` *  echo count($a); //0` |
|         - |  718 | ` * Note that this is a PH7 eXtension.` |
|         - |  719 | ` * Refer to the official documentation for more information.` |
|         - |  720 | ` * If a node with the given key already exists in the database` |
|         - |  721 | ` * then this function overwrite the old value.` |
|         - |  722 | ` */` |
|     46502 |  723 | `static sxi32 HashmapInsertByRef(` |
|         - |  724 | `	ph7_hashmap *pMap,   /* Target hashmap */` |
|         - |  725 | `	ph7_value *pKey,     /* Lookup key */` |
|         - |  726 | `	sxu32 nRefIdx        /* Foreign ph7_value index */` |
|         - |  727 | `	)` |
|         5 |  728 | `{` |
|     46507 |  729 | `	ph7_hashmap_node *pNode = 0;` |
|     46507 |  730 | `	sxi32 rc = SXRET_OK;` |
|     46507 |  731 | `	if( pKey && pKey->iFlags & (MEMOBJ_STRING\|MEMOBJ_HASHMAP\|MEMOBJ_OBJ\|MEMOBJ_RES) ){` |
|     46471 |  732 | `		if( (pKey->iFlags & MEMOBJ_STRING) == 0 ){` |
|         - |  733 | `			/* Force a string cast */` |
|       ! 0 |  734 | `			PH7_MemObjToString(&(*pKey));` |
|       ! 0 |  735 | `		}` |
|     46471 |  736 | `		if( SyBlobLength(&pKey->sBlob) < 1 \|\| HashmapIsIntKey(&pKey->sBlob) ){` |
|         3 |  737 | `			if(SyBlobLength(&pKey->sBlob) < 1){` |
|         - |  738 | `				/* Automatic index assign */` |
|       ! 0 |  739 | `				pKey = 0;` |
|       ! 0 |  740 | `			}` |
|         3 |  741 | `			goto IntKey;` |
|         - |  742 | `		}` |
|     69701 |  743 | `		if( SXRET_OK == HashmapLookupBlobKey(&(*pMap),SyBlobData(&pKey->sBlob),` |
|     23232 |  744 | `			SyBlobLength(&pKey->sBlob),&pNode) ){` |
|         - |  745 | `				/* Overwrite */` |
|         7 |  746 | `				PH7_VmRefObjRemove(pMap->pVm,pNode->nValIdx,0,pNode);` |
|         7 |  747 | `				pNode->nValIdx = nRefIdx;` |
|         - |  748 | `				/* Install in the reference table */` |
|         7 |  749 | `				PH7_VmRefObjInstall(pMap->pVm,nRefIdx,0,pNode,0);` |
|         7 |  750 | `				return SXRET_OK;` |
|         - |  751 | `		}` |
|         - |  752 | `		/* Perform a blob-key insertion */` |
|     46463 |  753 | `		rc = HashmapInsertBlobKey(&(*pMap),SyBlobData(&pKey->sBlob),SyBlobLength(&pKey->sBlob),0,nRefIdx,TRUE);` |
|     46463 |  754 | `		return rc;` |
|         - |  755 | `	}` |
|        18 |  756 | `IntKey:` |
|        39 |  757 | `	if( pKey ){` |
|         7 |  758 | `		if((pKey->iFlags & MEMOBJ_INT) == 0 ){` |
|         - |  759 | `			/* Force an integer cast */` |
|         3 |  760 | `			PH7_MemObjToInteger(pKey);` |
|         1 |  761 | `		}` |
|         7 |  762 | `		if( SXRET_OK == HashmapLookupIntKey(&(*pMap),pKey->x.iVal,&pNode) ){` |
|         - |  763 | `			/* Overwrite */` |
|       ! 0 |  764 | `			PH7_VmRefObjRemove(pMap->pVm,pNode->nValIdx,0,pNode);` |
|       ! 0 |  765 | `			pNode->nValIdx = nRefIdx;` |
|         - |  766 | `			/* Install in the reference table */` |
|       ! 0 |  767 | `			PH7_VmRefObjInstall(pMap->pVm,nRefIdx,0,pNode,0);` |
|       ! 0 |  768 | `			return SXRET_OK;` |
|         - |  769 | `		}` |
|         - |  770 | `		/* Perform a 64-bit-int-key insertion */` |
|         7 |  771 | `		rc = HashmapInsertIntKey(&(*pMap),pKey->x.iVal,0,nRefIdx,TRUE);` |
|         7 |  772 | `		if( rc == SXRET_OK ){` |
|         7 |  773 | `			HashmapAdvanceAutoIndex(&(*pMap),pKey->x.iVal);` |
|         3 |  774 | `		}` |
|         4 |  775 | `	}else{` |
|        33 |  776 | `		if( HashmapAppendIndexBusy(&(*pMap),&rc) ){` |
|       ! 0 |  777 | `			return rc; /* PH7_EXCEPTION/PH7_ABORT: php's catchable Error was thrown */` |
|         - |  778 | `		}` |
|         - |  779 | `		/* Assign an automatic index */` |
|        33 |  780 | `		rc = HashmapInsertIntKey(&(*pMap),pMap->iNextIdx,0,nRefIdx,TRUE);` |
|        33 |  781 | `		if( rc == SXRET_OK && pMap->iNextIdx < SXI64_HIGH ){` |
|        33 |  782 | `			++pMap->iNextIdx;` |
|        16 |  783 | `		}` |
|         - |  784 | `	}` |
|         - |  785 | `	/* Insertion result */` |
|        39 |  786 | `	return rc;` |
|     23256 |  787 | `}` |
|         - |  788 | `/*` |
|         - |  789 | ` * Extract node value.` |
|         - |  790 | ` */` |
|   1394851 |  791 | `static ph7_value * HashmapExtractNodeValue(ph7_hashmap_node *pNode)` |
|         5 |  792 | `{` |
|         - |  793 | `	/* Point to the desired object */` |
|         - |  794 | `	ph7_value *pObj;` |
|   1394856 |  795 | `	pObj = (ph7_value *)SySetAt(&pNode->pMap->pVm->aMemObj,pNode->nValIdx);` |
|   1394856 |  796 | `	return pObj;` |
|         5 |  797 | `}` |
|         - |  798 | `/*` |
|         - |  799 | ` * Insert a node in the given hashmap.` |
|         - |  800 | ` * If a node with the given key already exists in the database` |
|         - |  801 | ` * then this function overwrite the old value.` |
|         - |  802 | ` */` |
|       446 |  803 | `static sxi32 HashmapInsertNode(ph7_hashmap *pMap,ph7_hashmap_node *pNode,int bPreserve)` |
|         5 |  804 | `{` |
|         - |  805 | `	ph7_value *pObj;` |
|         - |  806 | `	sxi32 rc;` |
|         - |  807 | `	/* Extract the node value */` |
|       451 |  808 | `	pObj = HashmapExtractNodeValue(&(*pNode));` |
|       451 |  809 | `	if( pObj == 0 ){` |
|       ! 0 |  810 | `		return SXERR_EMPTY;` |
|         - |  811 | `	}` |
|         - |  812 | `	/* Preserve key */` |
|       451 |  813 | `	if( pNode->iType == HASHMAP_INT_NODE){` |
|         - |  814 | `		/* Int64 key */` |
|       321 |  815 | `		if( !bPreserve ){` |
|         - |  816 | `			/* Assign an automatic index */` |
|       173 |  817 | `			rc = HashmapInsert(&(*pMap),0,pObj);` |
|        89 |  818 | `		}else{` |
|       149 |  819 | `			rc = HashmapInsertIntKey(&(*pMap),pNode->xKey.iKey,pObj,0,FALSE);` |
|         - |  820 | `		}` |
|       163 |  821 | `	}else{` |
|         - |  822 | `		/* Blob key */` |
|       131 |  823 | `		if( !bPreserve ){` |
|         - |  824 | `			/* treat it like an automatically-indexed element, drop the` |
|         - |  825 | `			 * original string key entirely */` |
|        35 |  826 | `			rc = HashmapInsert(&(*pMap),0,pObj);` |
|        18 |  827 | `		}else{` |
|       145 |  828 | `			rc = HashmapInsertBlobKey(&(*pMap),SyBlobData(&pNode->xKey.sKey),` |
|        48 |  829 | `				SyBlobLength(&pNode->xKey.sKey),pObj,0,FALSE);` |
|         - |  830 | `		}` |
|         - |  831 | `	}` |
|       451 |  832 | `	return rc;` |
|       228 |  833 | `}` |
|         - |  834 | `/*` |
|         - |  835 | ` * Compare two node values.` |
|         - |  836 | ` * Return 0 if the node values are equals, > 0 if pLeft is greater than pRight` |
|         - |  837 | ` * or < 0 if pRight is greater than pLeft.` |
|         - |  838 | ` * For a full description on ph7_values comparison,refer to the implementation` |
|         - |  839 | ` * of the [PH7_MemObjCmp()] function defined in memobj.c or the official` |
|         - |  840 | ` * documenation.` |
|         - |  841 | ` */` |
|     70469 |  842 | `static sxi32 HashmapNodeCmp(ph7_hashmap_node *pLeft,ph7_hashmap_node *pRight,int bStrict)` |
|         5 |  843 | `{` |
|         - |  844 | `	ph7_value sObj1,sObj2;` |
|         - |  845 | `	sxi32 rc;` |
|     70474 |  846 | `	if( pLeft == pRight ){` |
|         - |  847 | `		/*` |
|         - |  848 | `		 * Same node.Refer to the sort() implementation defined` |
|         - |  849 | `		 * below for more information on this sceanario.` |
|         - |  850 | `		 */` |
|       ! 0 |  851 | `		return 0;` |
|         - |  852 | `	}` |
|         - |  853 | `	/* Do the comparison */` |
|     70474 |  854 | `	PH7_MemObjInit(pLeft->pMap->pVm,&sObj1);` |
|     70474 |  855 | `	PH7_MemObjInit(pLeft->pMap->pVm,&sObj2);` |
|     70474 |  856 | `	PH7_HashmapExtractNodeValue(pLeft,&sObj1,FALSE);` |
|     70474 |  857 | `	PH7_HashmapExtractNodeValue(pRight,&sObj2,FALSE);` |
|     70474 |  858 | `	rc = PH7_MemObjCmp(&sObj1,&sObj2,bStrict,0);` |
|     70474 |  859 | `	PH7_MemObjRelease(&sObj1);` |
|     70474 |  860 | `	PH7_MemObjRelease(&sObj2);` |
|     70474 |  861 | `	return rc;` |
|     35264 |  862 | `}` |
|         - |  863 | `/*` |
|         - |  864 | ` * Rehash a node with a 64-bit integer key.` |
|         - |  865 | ` * Refer to [merge_sort(),array_shift()] implementations for more information.` |
|         - |  866 | ` */` |
|     13542 |  867 | `static void HashmapRehashIntNode(ph7_hashmap_node *pEntry)` |
|         5 |  868 | `{` |
|     13547 |  869 | `	ph7_hashmap *pMap = pEntry->pMap;` |
|         - |  870 | `	sxu32 nBucket;` |
|         - |  871 | `	/* Remove old collision links */` |
|     13547 |  872 | `	if( pEntry->pPrevCollide ){` |
|     11080 |  873 | `		pEntry->pPrevCollide->pNextCollide = pEntry->pNextCollide;` |
|      5546 |  874 | `	}else{` |
|      2472 |  875 | `		pMap->apBucket[pEntry->nHash & (pMap->nSize - 1)] = pEntry->pNextCollide;` |
|         - |  876 | `	}` |
|     13547 |  877 | `	if( pEntry->pNextCollide ){` |
|      1101 |  878 | `		pEntry->pNextCollide->pPrevCollide = pEntry->pPrevCollide;` |
|       563 |  879 | `	}` |
|     13547 |  880 | `	pEntry->pNextCollide = pEntry->pPrevCollide = 0;` |
|         - |  881 | `	/* Compute the new hash */` |
|     13547 |  882 | `	pEntry->nHash = pMap->xIntHash(pMap->iNextIdx);` |
|     13547 |  883 | `	pEntry->xKey.iKey = pMap->iNextIdx;` |
|     13547 |  884 | `	nBucket = pEntry->nHash & (pMap->nSize - 1);` |
|         - |  885 | `	/* Link to the new bucket */` |
|     13547 |  886 | `	pEntry->pNextCollide = pMap->apBucket[nBucket];` |
|     13547 |  887 | `	if( pMap->apBucket[nBucket] ){` |
|     11411 |  888 | `		pMap->apBucket[nBucket]->pPrevCollide = pEntry;` |
|      5705 |  889 | `	}` |
|     13547 |  890 | `	pEntry->pNextCollide = pMap->apBucket[nBucket];` |
|     13547 |  891 | `	pMap->apBucket[nBucket] = pEntry;` |
|         - |  892 | `	/* Increment the automatic index (saturating, like every other advance —` |
|         - |  893 | `	 * unreachable in practice since renumbering assigns 0..nEntry-1, but keep` |
|         - |  894 | `	 * the no-overflow invariant uniform). */` |
|     13547 |  895 | `	if( pMap->iNextIdx < SXI64_HIGH ){` |
|     13547 |  896 | `		pMap->iNextIdx++;` |
|      6771 |  897 | `	}` |
|     13547 |  898 | `}` |
|         - |  899 | `/*` |
|         - |  900 | ` * Perform a linear search on a given hashmap.` |
|         - |  901 | ` * Write a pointer to the target node on success.` |
|         - |  902 | ` * Otherwise SXERR_NOTFOUND is returned on failure.` |
|         - |  903 | ` * Refer to [array_intersect(),array_diff(),in_array(),...] implementations` |
|         - |  904 | ` * for more information.` |
|         - |  905 | ` */` |
|     32874 |  906 | `static int HashmapFindValue(` |
|         - |  907 | `	ph7_hashmap *pMap,   /* Target hashmap */` |
|         - |  908 | `	ph7_value *pNeedle,  /* Lookup key */` |
|         - |  909 | `	ph7_hashmap_node **ppNode, /* OUT: target node on success  */` |
|         - |  910 | `	int bStrict      /* TRUE for strict comparison */` |
|         - |  911 | `	)` |
|         5 |  912 | `{` |
|         - |  913 | `	ph7_hashmap_node *pEntry;` |
|         - |  914 | `	ph7_value sVal,*pVal;` |
|         - |  915 | `	ph7_value sNeedle;` |
|         - |  916 | `	sxi32 rc;` |
|         - |  917 | `	sxu32 n;` |
|         - |  918 | `	/* Perform a linear search since we cannot sort the hashmap based on values */` |
|     32879 |  919 | `	pEntry = pMap->pFirst;` |
|     32879 |  920 | `	n = pMap->nEntry;` |
|     32879 |  921 | `	PH7_MemObjInit(pMap->pVm,&sVal);` |
|     32879 |  922 | `	PH7_MemObjInit(pMap->pVm,&sNeedle);` |
|     78030 |  923 | `	for(;;){` |
|    156068 |  924 | `		if( n < 1 ){` |
|       107 |  925 | `			break;` |
|         - |  926 | `		}` |
|         - |  927 | `		/* Extract node value */` |
|    155962 |  928 | `		pVal = HashmapExtractNodeValue(pEntry);` |
|    155962 |  929 | `		if( pVal ){` |
|    155962 |  930 | `			if( (pVal->iFlags\|pNeedle->iFlags) & MEMOBJ_NULL ){` |
|       ! 0 |  931 | `				sxi32 iF1 = pVal->iFlags&~MEMOBJ_AUX;` |
|       ! 0 |  932 | `				sxi32 iF2 = pNeedle->iFlags&~MEMOBJ_AUX;` |
|       ! 0 |  933 | `				if( iF1 == iF2 ){` |
|         - |  934 | `					/* NULL values are equals */` |
|       ! 0 |  935 | `					if( ppNode ){` |
|       ! 0 |  936 | `						*ppNode = pEntry;` |
|       ! 0 |  937 | `					}` |
|       ! 0 |  938 | `					return SXRET_OK;` |
|         - |  939 | `				}` |
|       ! 0 |  940 | `			}else{` |
|         - |  941 | `				/* Duplicate value */` |
|    155962 |  942 | `				PH7_MemObjLoad(pVal,&sVal);` |
|    155962 |  943 | `				PH7_MemObjLoad(pNeedle,&sNeedle);` |
|    155962 |  944 | `				rc = PH7_MemObjCmp(&sNeedle,&sVal,bStrict,0);` |
|    155962 |  945 | `				PH7_MemObjRelease(&sVal);` |
|    155962 |  946 | `				PH7_MemObjRelease(&sNeedle);` |
|    155962 |  947 | `				if( rc == 0 ){` |
|     32773 |  948 | `					if( ppNode ){` |
|        23 |  949 | `						*ppNode = pEntry;` |
|        11 |  950 | `					}` |
|         - |  951 | `					/* Match found*/` |
|     32773 |  952 | `					return SXRET_OK;` |
|         - |  953 | `				}` |
|         - |  954 | `			}` |
|     61593 |  955 | `		}` |
|         - |  956 | `		/* Point to the next entry */` |
|    123194 |  957 | `		pEntry = pEntry->pPrev; /* Reverse link */` |
|    123194 |  958 | `		n--;` |
|         5 |  959 | `	}` |
|         - |  960 | `	/* No such entry */` |
|       107 |  961 | `	return SXERR_NOTFOUND;` |
|     16442 |  962 | `}` |
|         - |  963 | `/*` |
|         - |  964 | ` * Perform a linear search on a given hashmap but use an user-defined callback` |
|         - |  965 | ` * for values comparison.` |
|         - |  966 | ` * Write a pointer to the target node on success.` |
|         - |  967 | ` * Otherwise SXERR_NOTFOUND is returned on failure.` |
|         - |  968 | ` * Refer to [array_uintersect(),array_udiff()...] implementations` |
|         - |  969 | ` * for more information.` |
|         - |  970 | ` */` |
|        22 |  971 | `static int HashmapFindValueByCallback(` |
|         - |  972 | `	ph7_hashmap *pMap,     /* Target hashmap */` |
|         - |  973 | `	ph7_value *pNeedle,    /* Lookup key */` |
|         - |  974 | `	ph7_value *pCallback,  /* User defined callback */` |
|         - |  975 | `	ph7_hashmap_node **ppNode /* OUT: target node on success */` |
|         - |  976 | `	)` |
|         1 |  977 | `{` |
|         - |  978 | `	ph7_hashmap_node *pEntry;` |
|         - |  979 | `	ph7_value sResult,*pVal;` |
|         - |  980 | `	ph7_value *apArg[2];    /* Callback arguments */` |
|         - |  981 | `	sxi32 rc;` |
|         - |  982 | `	sxu32 n;` |
|        23 |  983 | `	if( pMap->pVm->iCmpCallbackExc ){` |
|         - |  984 | `		/* A previous comparison already raised: stop invoking the callback so the` |
|         - |  985 | `		 * exception is not thrown again, and let the caller wind down. */` |
|       ! 0 |  986 | `		return SXERR_NOTFOUND;` |
|         - |  987 | `	}` |
|         - |  988 | `	/* Perform a linear search since we cannot sort the array based on values */` |
|        23 |  989 | `	pEntry = pMap->pFirst;` |
|        23 |  990 | `	n = pMap->nEntry;` |
|         - |  991 | `	/* Store callback result here */` |
|        23 |  992 | `	PH7_MemObjInit(pMap->pVm,&sResult);` |
|         - |  993 | `	/* First argument to the callback */` |
|        23 |  994 | `	apArg[0] = pNeedle;` |
|        25 |  995 | `	for(;;){` |
|        51 |  996 | `		if( n < 1 ){` |
|         9 |  997 | `			break;` |
|         - |  998 | `		}` |
|         - |  999 | `		/* Extract node value */` |
|        43 | 1000 | `		pVal = HashmapExtractNodeValue(pEntry);` |
|        43 | 1001 | `		if( pVal ){` |
|         - | 1002 | `			/* Invoke the user callback */` |
|        43 | 1003 | `			apArg[1] = pVal; /* Second argument to the callback */` |
|        43 | 1004 | `			rc = PH7_VmCallUserFunction(pMap->pVm,pCallback,2,apArg,&sResult);` |
|        43 | 1005 | `			if( rc == PH7_EXCEPTION ){` |
|         - | 1006 | `				/* The callback raised: flag it so the caller aborts and propagates,` |
|         - | 1007 | `				 * and report no match for the rest of the run. */` |
|         5 | 1008 | `				pMap->pVm->iCmpCallbackExc = 1;` |
|         5 | 1009 | `				PH7_MemObjRelease(&sResult);` |
|         5 | 1010 | `				return SXERR_NOTFOUND;` |
|         - | 1011 | `			}` |
|        39 | 1012 | `			if( rc == SXRET_OK ){` |
|         - | 1013 | `				/* Extract callback result */` |
|        39 | 1014 | `				if( (sResult.iFlags & MEMOBJ_INT) == 0 ){` |
|         - | 1015 | `					/* Perform an int cast */` |
|       ! 0 | 1016 | `					PH7_MemObjToInteger(&sResult);` |
|       ! 0 | 1017 | `				}` |
|        39 | 1018 | `				rc = (sxi32)sResult.x.iVal;` |
|        39 | 1019 | `				PH7_MemObjRelease(&sResult);` |
|        39 | 1020 | `				if( rc == 0 ){` |
|         - | 1021 | `					/* Match found*/` |
|        11 | 1022 | `					if( ppNode ){` |
|       ! 0 | 1023 | `						*ppNode = pEntry;` |
|       ! 0 | 1024 | `					}` |
|        11 | 1025 | `					return SXRET_OK;` |
|         - | 1026 | `				}` |
|        14 | 1027 | `			}` |
|        14 | 1028 | `		}` |
|         - | 1029 | `		/* Point to the next entry */` |
|        29 | 1030 | `		pEntry = pEntry->pPrev; /* Reverse link */` |
|        29 | 1031 | `		n--;` |
|         1 | 1032 | `	}` |
|         - | 1033 | `	/* No such entry */` |
|         9 | 1034 | `	return SXERR_NOTFOUND;` |
|        12 | 1035 | `}` |
|         - | 1036 | `/*` |
|         - | 1037 | ` * Compare two hashmaps.` |
|         - | 1038 | ` * Return 0 if the hashmaps are equals.Any other value indicates inequality.` |
|         - | 1039 | ` * Note on array comparison operators.` |
|         - | 1040 | ` *  According to the PHP language reference manual.` |
|         - | 1041 | ` *  Array Operators Example 	Name 	Result` |
|         - | 1042 | ` *  $a + $b 	Union 	Union of $a and $b.` |
|         - | 1043 | ` *  $a == $b 	Equality 	TRUE if $a and $b have the same key/value pairs.` |
|         - | 1044 | ` *  $a === $b 	Identity 	TRUE if $a and $b have the same key/value pairs in the same` |
|         - | 1045 | ` *                          order and of the same types.` |
|         - | 1046 | ` *  $a != $b 	Inequality 	TRUE if $a is not equal to $b.` |
|         - | 1047 | ` *  $a <> $b 	Inequality 	TRUE if $a is not equal to $b.` |
|         - | 1048 | ` *  $a !== $b 	Non-identity 	TRUE if $a is not identical to $b.` |
|         - | 1049 | ` * The + operator returns the right-hand array appended to the left-hand array;` |
|         - | 1050 | ` * For keys that exist in both arrays, the elements from the left-hand array will be used` |
|         - | 1051 | ` * and the matching elements from the right-hand array will be ignored.` |
|         - | 1052 | ` * <?php` |
|         - | 1053 | ` * $a = array("a" => "apple", "b" => "banana");` |
|         - | 1054 | ` * $b = array("a" => "pear", "b" => "strawberry", "c" => "cherry");` |
|         - | 1055 | ` * $c = $a + $b; // Union of $a and $b` |
|         - | 1056 | ` * echo "Union of \$a and \$b: \n";` |
|         - | 1057 | ` * var_dump($c);` |
|         - | 1058 | ` * $c = $b + $a; // Union of $b and $a` |
|         - | 1059 | ` * echo "Union of \$b and \$a: \n";` |
|         - | 1060 | ` * var_dump($c);` |
|         - | 1061 | ` * ?>` |
|         - | 1062 | ` * When executed, this script will print the following:` |
|         - | 1063 | ` * Union of $a and $b:` |
|         - | 1064 | ` * array(3) {` |
|         - | 1065 | ` *  ["a"]=>` |
|         - | 1066 | ` *  string(5) "apple"` |
|         - | 1067 | ` *  ["b"]=>` |
|         - | 1068 | ` * string(6) "banana"` |
|         - | 1069 | ` *  ["c"]=>` |
|         - | 1070 | ` * string(6) "cherry"` |
|         - | 1071 | ` * }` |
|         - | 1072 | ` * Union of $b and $a:` |
|         - | 1073 | ` * array(3) {` |
|         - | 1074 | ` * ["a"]=>` |
|         - | 1075 | ` * string(4) "pear"` |
|         - | 1076 | ` * ["b"]=>` |
|         - | 1077 | ` * string(10) "strawberry"` |
|         - | 1078 | ` * ["c"]=>` |
|         - | 1079 | ` * string(6) "cherry"` |
|         - | 1080 | ` * }` |
|         - | 1081 | ` * Elements of arrays are equal for the comparison if they have the same key and value.` |
|         - | 1082 | ` */` |
|        30 | 1083 | `PH7_PRIVATE sxi32 PH7_HashmapCmp(` |
|         - | 1084 | `	ph7_hashmap *pLeft,  /* Left hashmap */` |
|         - | 1085 | `	ph7_hashmap *pRight, /* Right hashmap */` |
|         - | 1086 | `	int bStrict          /* TRUE for strict comparison */` |
|         - | 1087 | `	)` |
|         1 | 1088 | `{` |
|         - | 1089 | `	ph7_hashmap_node *pLe,*pRe;` |
|         - | 1090 | `	sxi32 rc;` |
|         - | 1091 | `	sxu32 n;` |
|        31 | 1092 | `	if( pLeft == pRight ){` |
|         - | 1093 | `		/* Same hashmap instance. This can easily happen since hashmaps are passed by reference.` |
|         - | 1094 | `		 * Unlike the zend engine.` |
|         - | 1095 | `		 */` |
|         3 | 1096 | `		return 0;` |
|         - | 1097 | `	}` |
|        29 | 1098 | `	if( pLeft->nEntry != pRight->nEntry ){` |
|         - | 1099 | `		/* Must have the same number of entries */` |
|         5 | 1100 | `		return pLeft->nEntry > pRight->nEntry ? 1 : -1;` |
|         - | 1101 | `	}` |
|         - | 1102 | `	/* Point to the first inserted entry of the left hashmap */` |
|        25 | 1103 | `	pLe = pLeft->pFirst;` |
|        25 | 1104 | `	pRe = 0; /* cc warning */` |
|         - | 1105 | `	/* Perform the comparison */` |
|        25 | 1106 | `	n = pLeft->nEntry;` |
|        59 | 1107 | `	for(;;){` |
|       119 | 1108 | `		if( n < 1 ){` |
|        23 | 1109 | `			break;` |
|         - | 1110 | `		}` |
|        97 | 1111 | `		if( pLe->iType == HASHMAP_INT_NODE){` |
|         - | 1112 | `			/* Int key */` |
|        89 | 1113 | `			rc = HashmapLookupIntKey(&(*pRight),pLe->xKey.iKey,&pRe);` |
|        45 | 1114 | `		}else{` |
|         9 | 1115 | `			SyBlob *pKey = &pLe->xKey.sKey;` |
|         - | 1116 | `			/* Blob key */` |
|         9 | 1117 | `			rc = HashmapLookupBlobKey(&(*pRight),SyBlobData(pKey),SyBlobLength(pKey),&pRe);` |
|         - | 1118 | `		}` |
|        97 | 1119 | `		if( rc != SXRET_OK ){` |
|         - | 1120 | `			/* No such entry in the right side */` |
|       ! 0 | 1121 | `			return 1;` |
|         - | 1122 | `		}` |
|        97 | 1123 | `		rc = 0;` |
|        97 | 1124 | `		if( bStrict ){` |
|         - | 1125 | `			/* Make sure,the keys are of the same type */` |
|        81 | 1126 | `			if( pLe->iType != pRe->iType ){` |
|       ! 0 | 1127 | `				rc = 1;` |
|       ! 0 | 1128 | `			}` |
|        40 | 1129 | `		}` |
|        97 | 1130 | `		if( !rc ){` |
|         - | 1131 | `			/* Compare nodes */` |
|        97 | 1132 | `			rc = HashmapNodeCmp(pLe,pRe,bStrict);` |
|        48 | 1133 | `		}` |
|        97 | 1134 | `		if( rc != 0 ){` |
|         - | 1135 | `			/* Nodes key/value differ */` |
|         3 | 1136 | `			return rc;` |
|         - | 1137 | `		}` |
|         - | 1138 | `		/* Point to the next entry */` |
|        95 | 1139 | `		pLe = pLe->pPrev; /* Reverse link */` |
|        95 | 1140 | `		n--;` |
|         1 | 1141 | `	}` |
|        23 | 1142 | `	return 0; /* Hashmaps are equals */` |
|        16 | 1143 | `}` |
|         - | 1144 | `/*` |
|         - | 1145 | ` * Duplicate a hashmap node.` |
|         - | 1146 | ` * This function is used by HashmapMerge, HashmapOverwrite and PH7_HashmapDup.` |
|         - | 1147 | ` */` |
|    647106 | 1148 | `static sxi32 HashmapDuplicateNode(` |
|         - | 1149 | `	ph7_hashmap *pDest,` |
|         - | 1150 | `	ph7_hashmap_node *pEntry,` |
|         - | 1151 | `	ph7_value *pVal,` |
|         - | 1152 | `	int iAction /* 0: Merge, 1: Overwrite, 2: Dup */` |
|         - | 1153 | `	)` |
|         5 | 1154 | `{` |
|         - | 1155 | `	ph7_value sSafeVal;` |
|         - | 1156 | `	ph7_value sKey;` |
|         - | 1157 | `	sxi32 rc;` |
|         - | 1158 |  |
|    647111 | 1159 | `	if( pEntry->iFlags & HASHMAP_NODE_FOREIGN_OBJ ){` |
|         - | 1160 | `		/* The source node holds a reference to a foreign ph7_value (e.g: [&$x]).` |
|         - | 1161 | `		 * Re-insert it by reference so the reference survives the duplication` |
|         - | 1162 | `		 * instead of being flattened to a value copy. This keeps spread` |
|         - | 1163 | `		 * ([...$a]), array_merge(), array_replace() and array copies in sync` |
|         - | 1164 | `		 * with PHP semantics. */` |
|         7 | 1165 | `		sxu32 nRefIdx = pEntry->nValIdx;` |
|         7 | 1166 | `		if( pEntry->iType == HASHMAP_BLOB_NODE ){` |
|         3 | 1167 | `			PH7_MemObjInitFromString(pDest->pVm,&sKey,0);` |
|         3 | 1168 | `			PH7_MemObjStringAppend(&sKey,(const char *)SyBlobData(&pEntry->xKey.sKey),SyBlobLength(&pEntry->xKey.sKey));` |
|         3 | 1169 | `			rc = HashmapInsertByRef(pDest,&sKey,nRefIdx);` |
|         3 | 1170 | `			PH7_MemObjRelease(&sKey);` |
|         2 | 1171 | `		}else{` |
|         5 | 1172 | `			if( iAction == 0 ){ /* Merge: automatic index assign */` |
|         5 | 1173 | `				rc = HashmapInsertByRef(pDest,0,nRefIdx);` |
|         2 | 1174 | `			}else if( iAction == 1 ){ /* Overwrite: keep the int key */` |
|       ! 0 | 1175 | `				PH7_MemObjInitFromInt(pDest->pVm,&sKey,pEntry->xKey.iKey);` |
|       ! 0 | 1176 | `				rc = HashmapInsertByRef(pDest,&sKey,nRefIdx);` |
|       ! 0 | 1177 | `				PH7_MemObjRelease(&sKey);` |
|       ! 0 | 1178 | `			}else{ /* Dup: preserve the int key */` |
|       ! 0 | 1179 | `				rc = HashmapInsertIntKey(pDest,pEntry->xKey.iKey,0,nRefIdx,TRUE);` |
|         - | 1180 | `			}` |
|         - | 1181 | `		}` |
|         7 | 1182 | `		return rc;` |
|         - | 1183 | `	}` |
|    647105 | 1184 | `	sSafeVal = *pVal;` |
|         - | 1185 |  |
|    647105 | 1186 | `	if( pEntry->iType == HASHMAP_BLOB_NODE ){` |
|         - | 1187 | `		/* Blob key insertion */` |
|      4033 | 1188 | `		PH7_MemObjInitFromString(pDest->pVm,&sKey,0);` |
|      4033 | 1189 | `		PH7_MemObjStringAppend(&sKey,(const char *)SyBlobData(&pEntry->xKey.sKey),SyBlobLength(&pEntry->xKey.sKey));` |
|      4033 | 1190 | `		rc = PH7_HashmapInsert(pDest,&sKey,&sSafeVal);` |
|      4033 | 1191 | `		PH7_MemObjRelease(&sKey);` |
|      2019 | 1192 | `	}else{` |
|         - | 1193 | `		/* Int key */` |
|    643077 | 1194 | `		if( iAction == 0 ){ /* Merge */` |
|    642863 | 1195 | `			rc = HashmapInsert(pDest,0/* Automatic index assign */,&sSafeVal);` |
|    321646 | 1196 | `		}else if( iAction == 1 ){ /* Overwrite */` |
|        32 | 1197 | `			PH7_MemObjInitFromInt(pDest->pVm,&sKey,pEntry->xKey.iKey);` |
|        32 | 1198 | `			rc = PH7_HashmapInsert(pDest,&sKey,&sSafeVal);` |
|        32 | 1199 | `			PH7_MemObjRelease(&sKey);` |
|        17 | 1200 | `		}else{ /* Dup */` |
|       187 | 1201 | `			rc = HashmapInsertIntKey(pDest,pEntry->xKey.iKey,&sSafeVal,0,FALSE);` |
|         - | 1202 | `		}` |
|         - | 1203 | `	}` |
|    647105 | 1204 | `	return rc;` |
|    323558 | 1205 | `}` |
|         - | 1206 | `/*` |
|         - | 1207 | ` * Merge two hashmaps.` |
|         - | 1208 | ` * Note on the merge process` |
|         - | 1209 | ` * According to the PHP language reference manual.` |
|         - | 1210 | ` *  Merges the elements of two arrays together so that the values of one are appended` |
|         - | 1211 | ` *  to the end of the previous one. It returns the resulting array (pDest).` |
|         - | 1212 | ` *  If the input arrays have the same string keys, then the later value for that key` |
|         - | 1213 | ` *  will overwrite the previous one. If, however, the arrays contain numeric keys` |
|         - | 1214 | ` *  the later value will not overwrite the original value, but will be appended.` |
|         - | 1215 | ` *  Values in the input array with numeric keys will be renumbered with incrementing` |
|         - | 1216 | ` *  keys starting from zero in the result array.` |
|         - | 1217 | ` */` |
|      2136 | 1218 | `static sxi32 HashmapMerge(ph7_hashmap *pSrc,ph7_hashmap *pDest)` |
|         5 | 1219 | `{` |
|         - | 1220 | `	ph7_hashmap_node *pEntry;` |
|         - | 1221 | `	ph7_value *pVal;` |
|         - | 1222 | `	sxi32 rc;` |
|         - | 1223 | `	sxu32 n;` |
|      2141 | 1224 | `	if( pSrc == pDest ){` |
|         - | 1225 | `		/* Same map. This can easily happen since hashmaps are passed by reference.` |
|         - | 1226 | `		 * Unlike the zend engine.` |
|         - | 1227 | `		 */` |
|       ! 0 | 1228 | `		return SXRET_OK;` |
|         - | 1229 | `	}` |
|         - | 1230 | `	/* Point to the first inserted entry in the source */` |
|      2141 | 1231 | `	pEntry = pSrc->pFirst;` |
|         - | 1232 | `	/* Perform the merge */` |
|    645057 | 1233 | `	for( n = 0 ; n < pSrc->nEntry ; ++n ){` |
|         - | 1234 | `		/* Extract the node value */` |
|    642921 | 1235 | `		pVal = HashmapExtractNodeValue(pEntry);` |
|    642921 | 1236 | `		if( pVal ){` |
|         - | 1237 | `			/* Make a local copy of the value.` |
|         - | 1238 | `			 * The insertion call below may trigger a memory pool reallocation` |
|         - | 1239 | `			 * which will invalidate the 'pVal' pointer since it points` |
|         - | 1240 | `			 * to the old pool.` |
|         - | 1241 | `			 */` |
|    642921 | 1242 | `			rc = HashmapDuplicateNode(pDest,pEntry,pVal,0);` |
|    321463 | 1243 | `		}else{` |
|       ! 0 | 1244 | `			rc = SXRET_OK;` |
|         - | 1245 | `		}` |
|    642921 | 1246 | `		if( rc != SXRET_OK ){` |
|       ! 0 | 1247 | `			return rc;` |
|         - | 1248 | `		}` |
|         - | 1249 | `		/* Point to the next entry */` |
|    642921 | 1250 | `		pEntry = pEntry->pPrev; /* Reverse link */` |
|    321463 | 1251 | `	}` |
|      2141 | 1252 | `	return SXRET_OK;` |
|      1073 | 1253 | `}` |
|         - | 1254 | `/*` |
|         - | 1255 | ` * Overwrite entries with the same key.` |
|         - | 1256 | ` * Refer to the [array_replace()] implementation for more information.` |
|         - | 1257 | ` *  According to the PHP language reference manual.` |
|         - | 1258 | ` *  array_replace() replaces the values of the first array with the same values` |
|         - | 1259 | ` *  from all the following arrays. If a key from the first array exists in the second` |
|         - | 1260 | ` *  array, its value will be replaced by the value from the second array. If the key` |
|         - | 1261 | ` *  exists in the second array, and not the first, it will be created in the first array.` |
|         - | 1262 | ` *  If a key only exists in the first array, it will be left as is. If several arrays` |
|         - | 1263 | ` *  are passed for replacement, they will be processed in order, the later arrays` |
|         - | 1264 | ` *  overwriting the previous values.` |
|         - | 1265 | ` *  array_replace() is not recursive : it will replace values in the first array` |
|         - | 1266 | ` *  by whatever type is in the second array.` |
|         - | 1267 | ` */` |
|        34 | 1268 | `static sxi32 HashmapOverwrite(ph7_hashmap *pSrc,ph7_hashmap *pDest)` |
|         2 | 1269 | `{` |
|         - | 1270 | `	ph7_hashmap_node *pEntry;` |
|         - | 1271 | `	ph7_value *pVal;` |
|         - | 1272 | `	sxi32 rc;` |
|         - | 1273 | `	sxu32 n;` |
|        36 | 1274 | `	if( pSrc == pDest ){` |
|         - | 1275 | `		/* Same map. This can easily happen since hashmaps are passed by reference.` |
|         - | 1276 | `		 * Unlike the zend engine.` |
|         - | 1277 | `		 */` |
|       ! 0 | 1278 | `		return SXRET_OK;` |
|         - | 1279 | `	}` |
|         - | 1280 | `	/* Point to the first inserted entry in the source */` |
|        36 | 1281 | `	pEntry = pSrc->pFirst;` |
|         - | 1282 | `	/* Perform the merge */` |
|        80 | 1283 | `	for( n = 0 ; n < pSrc->nEntry ; ++n ){` |
|         - | 1284 | `		/* Extract the node value */` |
|        46 | 1285 | `		pVal = HashmapExtractNodeValue(pEntry);` |
|        46 | 1286 | `		if( pVal ){` |
|        46 | 1287 | `			rc = HashmapDuplicateNode(pDest,pEntry,pVal,1);` |
|        24 | 1288 | `		}else{` |
|       ! 0 | 1289 | `			rc = SXRET_OK;` |
|         - | 1290 | `		}` |
|        46 | 1291 | `		if( rc != SXRET_OK ){` |
|       ! 0 | 1292 | `			return rc;` |
|         - | 1293 | `		}` |
|         - | 1294 | `		/* Point to the next entry */` |
|        46 | 1295 | `		pEntry = pEntry->pPrev; /* Reverse link */` |
|        24 | 1296 | `	}` |
|        36 | 1297 | `	return SXRET_OK;` |
|        19 | 1298 | `}` |
|         - | 1299 | `/*` |
|         - | 1300 | ` * Duplicate the contents of a hashmap. Store the copy in pDest.` |
|         - | 1301 | ` * Refer to the [array_pad(),array_copy(),...] implementation for more information.` |
|         - | 1302 | ` */` |
|      3922 | 1303 | `PH7_PRIVATE sxi32 PH7_HashmapDup(ph7_hashmap *pSrc,ph7_hashmap *pDest)` |
|         5 | 1304 | `{` |
|         - | 1305 | `	ph7_hashmap_node *pEntry;` |
|         - | 1306 | `	ph7_value *pVal;` |
|         - | 1307 | `	sxi32 rc;` |
|         - | 1308 | `	sxu32 n;` |
|      3927 | 1309 | `	if( pSrc == pDest ){` |
|         - | 1310 | `		/* Same map. This can easily happen since hashmaps are passed by reference.` |
|         - | 1311 | `		 * Unlike the zend engine.` |
|         - | 1312 | `		 */` |
|       ! 0 | 1313 | `		return SXRET_OK;` |
|         - | 1314 | `	}` |
|         - | 1315 | `	/* Point to the first inserted entry in the source */` |
|      3927 | 1316 | `	pEntry = pSrc->pFirst;` |
|         - | 1317 | `	/* Perform the duplication */` |
|      8073 | 1318 | `	for( n = 0 ; n < pSrc->nEntry ; ++n ){` |
|         - | 1319 | `		/* Extract the node value */` |
|      4151 | 1320 | `		pVal = HashmapExtractNodeValue(pEntry);` |
|      4151 | 1321 | `		if( pVal ){` |
|      4151 | 1322 | `			rc = HashmapDuplicateNode(pDest,pEntry,pVal,2);` |
|      2078 | 1323 | `		}else{` |
|       ! 0 | 1324 | `			rc = SXRET_OK;` |
|         - | 1325 | `		}` |
|      4151 | 1326 | `		if( rc != SXRET_OK ){` |
|       ! 0 | 1327 | `			return rc;` |
|         - | 1328 | `		}` |
|         - | 1329 | `		/* Point to the next entry */` |
|      4151 | 1330 | `		pEntry = pEntry->pPrev; /* Reverse link */` |
|      2078 | 1331 | `	}` |
|      3927 | 1332 | `	return SXRET_OK;` |
|      1966 | 1333 | `}` |
|         - | 1334 | `/*` |
|         - | 1335 | ` * Duplicate a hashmap, flattening every foreign (by-reference) node into a` |
|         - | 1336 | ` * plain value copy. php 8.1 gives a COPY of $GLOBALS pure value semantics` |
|         - | 1337 | ` * ($snap = $GLOBALS snapshots the symbol table: later writes on either side` |
|         - | 1338 | ` * never affect the other) — unlike ordinary array copies, where reference` |
|         - | 1339 | ` * elements stay live — so the $GLOBALS store path (PH7_MemObjStore) uses` |
|         - | 1340 | ` * this instead of PH7_HashmapDup.` |
|         - | 1341 | ` */` |
|        12 | 1342 | `PH7_PRIVATE sxi32 PH7_HashmapDupMaterialized(ph7_hashmap *pSrc,ph7_hashmap *pDest)` |
|         1 | 1343 | `{` |
|         - | 1344 | `	ph7_hashmap_node *pEntry;` |
|         - | 1345 | `	ph7_value *pVal;` |
|         - | 1346 | `	sxi32 rc;` |
|         - | 1347 | `	sxu32 n;` |
|        13 | 1348 | `	if( pSrc == pDest ){` |
|       ! 0 | 1349 | `		return SXRET_OK;` |
|         - | 1350 | `	}` |
|        13 | 1351 | `	pEntry = pSrc->pFirst;` |
|       739 | 1352 | `	for( n = 0 ; n < pSrc->nEntry ; ++n ){` |
|         - | 1353 | `		/* Extract the node value (resolves foreign references) */` |
|       727 | 1354 | `		pVal = HashmapExtractNodeValue(pEntry);` |
|       726 | 1355 | `		if( pVal && (pVal->iFlags & MEMOBJ_HASHMAP)` |
|       485 | 1356 | `		 && (ph7_hashmap *)pVal->x.pOther == pSrc->pVm->pGlobal ){` |
|         - | 1357 | `			/* A global still holding the live $GLOBALS map is the snapshot's` |
|         - | 1358 | `			 * own destination mid-store ($snap = $GLOBALS registers $snap` |
|         - | 1359 | `			 * before the value lands). php's snapshot — taken when $GLOBALS` |
|         - | 1360 | `			 * is READ, before the assignment — has no such entry, so skip it` |
|         - | 1361 | `			 * (also breaks the would-be infinite recursion). */` |
|         5 | 1362 | `			pVal = 0;` |
|         2 | 1363 | `		}` |
|       727 | 1364 | `		if( pVal ){` |
|       723 | 1365 | `			if( pEntry->iType == HASHMAP_BLOB_NODE ){` |
|      1078 | 1366 | `				rc = HashmapInsertBlobKey(&(*pDest),SyBlobData(&pEntry->xKey.sKey),` |
|       359 | 1367 | `					SyBlobLength(&pEntry->xKey.sKey),pVal,0,FALSE);` |
|       360 | 1368 | `			}else{` |
|         5 | 1369 | `				rc = HashmapInsertIntKey(&(*pDest),pEntry->xKey.iKey,pVal,0,FALSE);` |
|         - | 1370 | `			}` |
|       723 | 1371 | `			if( rc != SXRET_OK ){` |
|       ! 0 | 1372 | `				return rc;` |
|         - | 1373 | `			}` |
|       361 | 1374 | `		}` |
|         - | 1375 | `		/* Point to the next entry */` |
|       727 | 1376 | `		pEntry = pEntry->pPrev; /* Reverse link */` |
|       364 | 1377 | `	}` |
|        13 | 1378 | `	return SXRET_OK;` |
|         7 | 1379 | `}` |
|         - | 1380 | `/*` |
|         - | 1381 | ` * Copy-on-write separation for arrays.` |
|         - | 1382 | ` * If the hashmap inside pValue has iRef > 1 (shared), duplicate it so that` |
|         - | 1383 | ` * pValue owns a private copy. The original map's refcount is decremented.` |
|         - | 1384 | ` * Returns the (possibly new) hashmap pointer.` |
|         - | 1385 | ` */` |
|    224056 | 1386 | `PH7_PRIVATE ph7_hashmap * PH7_HashmapCowSeparate(ph7_vm *pVm,ph7_value *pValue)` |
|         5 | 1387 | `{` |
|    224061 | 1388 | `	ph7_hashmap *pMap = (ph7_hashmap *)pValue->x.pOther;` |
|         - | 1389 | `	ph7_hashmap *pNew;` |
|         - | 1390 | `	ph7_value *pBacking;` |
|         - | 1391 | `	sxu32 nValIdx;` |
|         - | 1392 | `	int bValueInPool;` |
|    224061 | 1393 | `	if( pMap->iRef < 2 ){` |
|         - | 1394 | `		/* Sole owner, no separation needed */` |
|    221729 | 1395 | `		return pMap;` |
|         - | 1396 | `	}` |
|      2337 | 1397 | `	if( pMap == pVm->pGlobal ){` |
|         - | 1398 | `		/* Never separate $GLOBALS — it is a live view of the symbol table.` |
|         - | 1399 | `		 * (A COPY of $GLOBALS never shares this map: PH7_MemObjStore` |
|         - | 1400 | `		 * materializes a by-value snapshot at assignment, php 8.1.) */` |
|       127 | 1401 | `		return pMap;` |
|         - | 1402 | `	}` |
|         - | 1403 | `	/* If this value is a stack copy of a named variable, separate the` |
|         - | 1404 | `	 * backing variable instead so the change persists after the stack` |
|         - | 1405 | `	 * frame is popped. */` |
|      2211 | 1406 | `	if( pValue->nIdx != SXU32_HIGH ){` |
|      2211 | 1407 | `		pBacking = (ph7_value *)SySetAt(&pVm->aMemObj,pValue->nIdx);` |
|      2206 | 1408 | `		if( pBacking && pBacking != pValue` |
|      2186 | 1409 | `			&& (pBacking->iFlags & MEMOBJ_HASHMAP)` |
|      2171 | 1410 | `			&& (ph7_hashmap *)pBacking->x.pOther == pMap ){` |
|         - | 1411 | `			/* Undo the stack ref to reveal true sharing count */` |
|      2171 | 1412 | `			pMap->iRef--;` |
|      2171 | 1413 | `			if( pMap->iRef < 2 ){` |
|         - | 1414 | `				/* After undoing stack ref, sole owner — no separation */` |
|      2129 | 1415 | `				pMap->iRef++;` |
|      2129 | 1416 | `				return pMap;` |
|         - | 1417 | `			}` |
|        44 | 1418 | `			pNew = PH7_NewHashmap(pVm,0,0);` |
|        44 | 1419 | `			if( pNew == 0 ){` |
|       ! 0 | 1420 | `				pMap->iRef++;` |
|       ! 0 | 1421 | `				return pMap;` |
|         - | 1422 | `			}` |
|        44 | 1423 | `			if( PH7_HashmapDup(pMap,pNew) != SXRET_OK ){` |
|         - | 1424 | `				/* Dup failed (OOM) — discard partial copy, restore state */` |
|       ! 0 | 1425 | `				PH7_HashmapRelease(pNew,TRUE);` |
|       ! 0 | 1426 | `				pMap->iRef++;` |
|       ! 0 | 1427 | `				return pMap;` |
|         - | 1428 | `			}` |
|        44 | 1429 | `			pNew->iNextIdx = pMap->iNextIdx;` |
|        44 | 1430 | `			pMap->iRef--;  /* Backing variable no longer references old map */` |
|         - | 1431 | `			/* PH7_HashmapDup reserves a memory object per duplicated entry, which` |
|         - | 1432 | `			 * can grow — and therefore reallocate (move) — pVm->aMemObj. That` |
|         - | 1433 | `			 * invalidates the pBacking pointer captured above, so re-resolve it` |
|         - | 1434 | `			 * from the (stable) slot index before writing. Using the stale pointer` |
|         - | 1435 | `			 * dereferences the freed old buffer, which is a hard SIGSEGV on` |
|         - | 1436 | `			 * glibc/x86_64 once aMemObj is large enough to be mmap-backed (the old` |
|         - | 1437 | `			 * mapping is munmap'd on move) and a silent use-after-free elsewhere. */` |
|        44 | 1438 | `			pBacking = (ph7_value *)SySetAt(&pVm->aMemObj,pValue->nIdx);` |
|        44 | 1439 | `			if( pBacking ){` |
|        44 | 1440 | `				pBacking->x.pOther = pNew;` |
|        21 | 1441 | `			}` |
|         - | 1442 | `			/* Update the stack value to match */` |
|        44 | 1443 | `			pValue->x.pOther = pNew;` |
|        44 | 1444 | `			pNew->iRef++;  /* +1 for stack (pValue); iRef=1 from NewHashmap covers pBacking */` |
|        44 | 1445 | `			return pNew;` |
|         - | 1446 | `		}` |
|        20 | 1447 | `	}` |
|         - | 1448 | `	/* Some callers (e.g. OP_STORE_IDX, by-ref foreach) pass a pValue that points` |
|         - | 1449 | `	 * directly into pVm->aMemObj. PH7_HashmapDup below reserves a memory object` |
|         - | 1450 | `	 * per duplicated entry, which can grow — and therefore reallocate (move) —` |
|         - | 1451 | `	 * pVm->aMemObj, leaving such a pValue dangling. Capture its slot identity now,` |
|         - | 1452 | `	 * before the dup, so the write-back can re-resolve from the (stable) index` |
|         - | 1453 | `	 * rather than dereference the captured pointer (the same hazard handled for` |
|         - | 1454 | `	 * pBacking in the backing-variable branch above). */` |
|        41 | 1455 | `	nValIdx = pValue->nIdx;` |
|        61 | 1456 | `	bValueInPool = ( nValIdx != SXU32_HIGH` |
|        40 | 1457 | `		&& (ph7_value *)SySetAt(&pVm->aMemObj,nValIdx) == pValue );` |
|        41 | 1458 | `	pNew = PH7_NewHashmap(pVm,0,0);` |
|        41 | 1459 | `	if( pNew == 0 ){` |
|         - | 1460 | `		/* Allocation failure — fall through with shared map */` |
|       ! 0 | 1461 | `		return pMap;` |
|         - | 1462 | `	}` |
|        41 | 1463 | `	if( PH7_HashmapDup(pMap,pNew) != SXRET_OK ){` |
|         - | 1464 | `		/* Dup failed (OOM) — discard partial copy, keep original */` |
|       ! 0 | 1465 | `		PH7_HashmapRelease(pNew,TRUE);` |
|       ! 0 | 1466 | `		return pMap;` |
|         - | 1467 | `	}` |
|        41 | 1468 | `	pNew->iNextIdx = pMap->iNextIdx;` |
|        41 | 1469 | `	pMap->iRef--;` |
|        41 | 1470 | `	if( bValueInPool ){` |
|         - | 1471 | `		/* aMemObj may have moved during the dup — re-resolve pValue's slot. */` |
|        41 | 1472 | `		pValue = (ph7_value *)SySetAt(&pVm->aMemObj,nValIdx);` |
|        41 | 1473 | `		if( pValue == 0 ){` |
|       ! 0 | 1474 | `			return pNew;` |
|         - | 1475 | `		}` |
|        20 | 1476 | `	}` |
|        41 | 1477 | `	pValue->x.pOther = pNew;` |
|        41 | 1478 | `	return pNew;` |
|    112033 | 1479 | `}` |
|         - | 1480 | `/*` |
|         - | 1481 | ` * Perform the union of two hashmaps.` |
|         - | 1482 | ` * This operation is performed only if the user uses the '+' operator` |
|         - | 1483 | ` * with a variable holding an array as follows:` |
|         - | 1484 | ` * <?php` |
|         - | 1485 | ` * $a = array("a" => "apple", "b" => "banana");` |
|         - | 1486 | ` * $b = array("a" => "pear", "b" => "strawberry", "c" => "cherry");` |
|         - | 1487 | ` * $c = $a + $b; // Union of $a and $b` |
|         - | 1488 | ` * echo "Union of \$a and \$b: \n";` |
|         - | 1489 | ` * var_dump($c);` |
|         - | 1490 | ` * $c = $b + $a; // Union of $b and $a` |
|         - | 1491 | ` * echo "Union of \$b and \$a: \n";` |
|         - | 1492 | ` * var_dump($c);` |
|         - | 1493 | ` * ?>` |
|         - | 1494 | ` * When executed, this script will print the following:` |
|         - | 1495 | ` * Union of $a and $b:` |
|         - | 1496 | ` * array(3) {` |
|         - | 1497 | ` *  ["a"]=>` |
|         - | 1498 | ` *  string(5) "apple"` |
|         - | 1499 | ` *  ["b"]=>` |
|         - | 1500 | ` * string(6) "banana"` |
|         - | 1501 | ` *  ["c"]=>` |
|         - | 1502 | ` * string(6) "cherry"` |
|         - | 1503 | ` * }` |
|         - | 1504 | ` * Union of $b and $a:` |
|         - | 1505 | ` * array(3) {` |
|         - | 1506 | ` * ["a"]=>` |
|         - | 1507 | ` * string(4) "pear"` |
|         - | 1508 | ` * ["b"]=>` |
|         - | 1509 | ` * string(10) "strawberry"` |
|         - | 1510 | ` * ["c"]=>` |
|         - | 1511 | ` * string(6) "cherry"` |
|         - | 1512 | ` * }` |
|         - | 1513 | ` * The + operator returns the right-hand array appended to the left-hand array;` |
|         - | 1514 | ` * For keys that exist in both arrays, the elements from the left-hand array will be used` |
|         - | 1515 | ` * and the matching elements from the right-hand array will be ignored.` |
|         - | 1516 | ` */` |
|      3816 | 1517 | `PH7_PRIVATE sxi32 PH7_HashmapUnion(ph7_hashmap *pLeft,ph7_hashmap *pRight)` |
|         5 | 1518 | `{` |
|         - | 1519 | `	ph7_hashmap_node *pEntry;` |
|      3821 | 1520 | `	sxi32 rc = SXRET_OK;` |
|         - | 1521 | `	ph7_value *pObj;` |
|         - | 1522 | `	sxu32 n;` |
|      3821 | 1523 | `	if( pLeft == pRight ){` |
|         - | 1524 | `		/* Same map. This can easily happen since hashmaps are passed by reference.` |
|         - | 1525 | `		 * Unlike the zend engine.` |
|         - | 1526 | `		 */` |
|       ! 0 | 1527 | `		return SXRET_OK;` |
|         - | 1528 | `	}` |
|         - | 1529 | `	/* Perform the union */` |
|      3821 | 1530 | `	pEntry = pRight->pFirst;` |
|      3855 | 1531 | `	for(n = 0 ; n < pRight->nEntry ; ++n ){` |
|         - | 1532 | `		/* Make sure the given key does not exists in the left array */` |
|        38 | 1533 | `		if( pEntry->iType == HASHMAP_BLOB_NODE ){` |
|         - | 1534 | `			/* BLOB key */` |
|        24 | 1535 | `			if( SXRET_OK !=` |
|        20 | 1536 | `				HashmapLookupBlobKey(&(*pLeft),SyBlobData(&pEntry->xKey.sKey),SyBlobLength(&pEntry->xKey.sKey),0) ){` |
|        20 | 1537 | `					pObj = HashmapExtractNodeValue(pEntry);` |
|        20 | 1538 | `					if( pObj ){` |
|        20 | 1539 | `						ph7_value sSafeVal = *pObj;` |
|         - | 1540 | `						/* Perform the insertion */` |
|        20 | 1541 | `						rc = HashmapInsertBlobKey(&(*pLeft),SyBlobData(&pEntry->xKey.sKey),SyBlobLength(&pEntry->xKey.sKey),` |
|         - | 1542 | `							&sSafeVal,0,FALSE);` |
|        20 | 1543 | `						if( rc != SXRET_OK ){` |
|       ! 0 | 1544 | `							return rc;` |
|         - | 1545 | `						}` |
|         8 | 1546 | `					}` |
|         8 | 1547 | `			}` |
|        14 | 1548 | `		}else{` |
|         - | 1549 | `			/* INT key */` |
|        16 | 1550 | `			if( SXRET_OK != HashmapLookupIntKey(&(*pLeft),pEntry->xKey.iKey,0) ){` |
|        11 | 1551 | `				pObj = HashmapExtractNodeValue(pEntry);` |
|        11 | 1552 | `				if( pObj ){` |
|        11 | 1553 | `					ph7_value sSafeVal = *pObj;` |
|         - | 1554 | `					/* Perform the insertion */` |
|        11 | 1555 | `					rc = HashmapInsertIntKey(&(*pLeft),pEntry->xKey.iKey,&sSafeVal,0,FALSE);` |
|        11 | 1556 | `					if( rc != SXRET_OK ){` |
|       ! 0 | 1557 | `						return rc;` |
|         - | 1558 | `					}` |
|         5 | 1559 | `				}` |
|         5 | 1560 | `			}` |
|         - | 1561 | `		}` |
|         - | 1562 | `		/* Point to the next entry */` |
|        38 | 1563 | `		pEntry = pEntry->pPrev; /* Reverse link */` |
|        21 | 1564 | `	}` |
|      3821 | 1565 | `	return SXRET_OK;` |
|      1913 | 1566 | `}` |
|         - | 1567 | `/*` |
|         - | 1568 | ` * Allocate a new hashmap.` |
|         - | 1569 | ` * Return a pointer to the freshly allocated hashmap on success.NULL otherwise.` |
|         - | 1570 | ` */` |
|    136574 | 1571 | `PH7_PRIVATE ph7_hashmap * PH7_NewHashmap(` |
|         - | 1572 | `	ph7_vm *pVm,              /* VM that trigger the hashmap creation */` |
|         - | 1573 | `	sxu32 (*xIntHash)(sxi64), /* Hash function for int keys.NULL otherwise*/` |
|         - | 1574 | `	sxu32 (*xBlobHash)(const void *,sxu32) /* Hash function for BLOB keys.NULL otherwise */` |
|         - | 1575 | `	)` |
|         5 | 1576 | `{` |
|         - | 1577 | `	ph7_hashmap *pMap;` |
|         - | 1578 | `	/* Allocate a new instance */` |
|    136579 | 1579 | `	pMap = (ph7_hashmap *)SyMemBackendPoolAlloc(&pVm->sAllocator,sizeof(ph7_hashmap));` |
|    136579 | 1580 | `	if( pMap == 0 ){` |
|       ! 0 | 1581 | `		return 0;` |
|         - | 1582 | `	}` |
|         - | 1583 | `	/* Zero the structure */` |
|    136579 | 1584 | `	SyZero(pMap,sizeof(ph7_hashmap));` |
|         - | 1585 | `	/* Fill in the structure */` |
|    136579 | 1586 | `	pMap->pVm = &(*pVm);` |
|    136579 | 1587 | `	pMap->iRef = 1;` |
|         - | 1588 | `	/* Default hash functions */` |
|    136579 | 1589 | `	pMap->xIntHash  = xIntHash ? xIntHash : IntHash;` |
|    136579 | 1590 | `	pMap->xBlobHash = xBlobHash ? xBlobHash : BinHash;` |
|    136579 | 1591 | `	return pMap;` |
|     68292 | 1592 | `}` |
|         - | 1593 | `/*` |
|         - | 1594 | ` * Install superglobals in the given virtual machine.` |
|         - | 1595 | ` * Note on superglobals.` |
|         - | 1596 | ` *  According to the PHP language reference manual.` |
|         - | 1597 | ` *  Superglobals are built-in variables that are always available in all scopes.` |
|         - | 1598 | `*   Description` |
|         - | 1599 | `*   Several predefined variables in PHP are "superglobals", which means they` |
|         - | 1600 | `*   are available in all scopes throughout a script. There is no need to do` |
|         - | 1601 | `*   global $variable; to access them within functions or methods.` |
|         - | 1602 | `*   These superglobal variables are:` |
|         - | 1603 | `*    $GLOBALS` |
|         - | 1604 | `*    $_SERVER` |
|         - | 1605 | `*    $_GET` |
|         - | 1606 | `*    $_POST` |
|         - | 1607 | `*    $_FILES` |
|         - | 1608 | `*    $_COOKIE` |
|         - | 1609 | `*    $_SESSION` |
|         - | 1610 | `*    $_REQUEST` |
|         - | 1611 | `*    $_ENV` |
|         - | 1612 | `*/` |
|      3480 | 1613 | `PH7_PRIVATE sxi32 PH7_HashmapCreateSuper(ph7_vm *pVm)` |
|         5 | 1614 | `{` |
|         - | 1615 | `	static const char * azSuper[] = {` |
|         - | 1616 | `		"_SERVER",   /* $_SERVER */` |
|         - | 1617 | `		"_GET",      /* $_GET */` |
|         - | 1618 | `		"_POST",     /* $_POST */` |
|         - | 1619 | `		"_FILES",    /* $_FILES */` |
|         - | 1620 | `		"_COOKIE",   /* $_COOKIE */` |
|         - | 1621 | `		"_SESSION",  /* $_SESSION */` |
|         - | 1622 | `		"_REQUEST",  /* $_REQUEST */` |
|         - | 1623 | `		"_ENV",      /* $_ENV */` |
|         - | 1624 | `		"_HEADER",   /* $_HEADER */` |
|         - | 1625 | `		"argv"       /* $argv */` |
|         - | 1626 | `	};` |
|         - | 1627 | `	ph7_hashmap *pMap;` |
|         - | 1628 | `	ph7_value *pObj;` |
|         - | 1629 | `	SyString *pFile;` |
|         - | 1630 | `	sxi32 rc;` |
|         - | 1631 | `	sxu32 n;` |
|         - | 1632 | `	/* Allocate a new hashmap for the $GLOBALS array */` |
|      3485 | 1633 | `	pMap = PH7_NewHashmap(&(*pVm),0,0);` |
|      3485 | 1634 | `	if( pMap == 0 ){` |
|       ! 0 | 1635 | `		return SXERR_MEM;` |
|         - | 1636 | `	}` |
|      3485 | 1637 | `	pVm->pGlobal = pMap;` |
|         - | 1638 | `	/* Reserve a ph7_value for the $GLOBALS array*/` |
|      3485 | 1639 | `	pObj = PH7_ReserveMemObj(&(*pVm));` |
|      3485 | 1640 | `	if( pObj == 0 ){` |
|       ! 0 | 1641 | `		return SXERR_MEM;` |
|         - | 1642 | `	}` |
|      3485 | 1643 | `	PH7_MemObjInitFromArray(&(*pVm),pObj,pMap);` |
|         - | 1644 | `	/* Record object index */` |
|      3485 | 1645 | `	pVm->nGlobalIdx = pObj->nIdx;` |
|         - | 1646 | `	/* Install the special $GLOBALS array */` |
|      3485 | 1647 | `	rc = SyHashInsert(&pVm->hSuper,(const void *)"GLOBALS",sizeof("GLOBALS")-1,SX_INT_TO_PTR(pVm->nGlobalIdx));` |
|      3485 | 1648 | `	if( rc != SXRET_OK ){` |
|       ! 0 | 1649 | `		return rc;` |
|         - | 1650 | `	}` |
|         - | 1651 | `	/* Install superglobals now */` |
|     38285 | 1652 | `	for( n =  0 ; n < SX_ARRAYSIZE(azSuper)  ; n++ ){` |
|         - | 1653 | `		ph7_value *pSuper;` |
|         - | 1654 | `		/* Request an empty array */` |
|     34805 | 1655 | `		pSuper = ph7_new_array(&(*pVm));` |
|     34805 | 1656 | `		if( pSuper == 0 ){` |
|       ! 0 | 1657 | `			return SXERR_MEM;` |
|         - | 1658 | `		}` |
|         - | 1659 | `		/* Install */` |
|     34805 | 1660 | `		rc = ph7_vm_config(&(*pVm),PH7_VM_CONFIG_CREATE_SUPER,azSuper[n]/* Super-global name*/,pSuper/* Super-global value */);` |
|     34805 | 1661 | `		if( rc != SXRET_OK ){` |
|       ! 0 | 1662 | `			return rc;` |
|         - | 1663 | `		}` |
|         - | 1664 | `		/* Release the value now it have been installed */` |
|     34805 | 1665 | `		ph7_release_value(&(*pVm),pSuper);` |
|     17405 | 1666 | `	}` |
|         - | 1667 | `	/* Set some $_SERVER entries */` |
|      3485 | 1668 | `	pFile = (SyString *)SySetPeek(&pVm->aFiles);` |
|         - | 1669 | `	/*` |
|         - | 1670 | `	 * 'SCRIPT_FILENAME'` |
|         - | 1671 | `	 * The absolute pathname of the currently executing script.` |
|         - | 1672 | `	 */` |
|      6961 | 1673 | `	ph7_vm_config(pVm,PH7_VM_CONFIG_SERVER_ATTR,` |
|         - | 1674 | `		"SCRIPT_FILENAME",` |
|      1740 | 1675 | `		pFile ? pFile->zString : ":Memory:",` |
|      3476 | 1676 | `		pFile ? pFile->nByte : sizeof(":Memory:") - 1` |
|         - | 1677 | `		);` |
|         - | 1678 | `	/* All done,all super-global are installed now */` |
|      3485 | 1679 | `	return SXRET_OK;` |
|      1745 | 1680 | `}` |
|         - | 1681 | `/*` |
|         - | 1682 | ` * Release a hashmap.` |
|         - | 1683 | ` */` |
|     93624 | 1684 | `PH7_PRIVATE sxi32 PH7_HashmapRelease(ph7_hashmap *pMap,int FreeDS)` |
|         5 | 1685 | `{` |
|         - | 1686 | `	ph7_hashmap_node *pEntry,*pNext;` |
|     93629 | 1687 | `	ph7_vm *pVm = pMap->pVm;` |
|         - | 1688 | `	sxu32 n;` |
|     93629 | 1689 | `	if( pMap == pVm->pGlobal ){` |
|         - | 1690 | `		/* Cannot delete the $GLOBALS array */` |
|       ! 0 | 1691 | `		PH7_VmThrowError(pMap->pVm,0,PH7_CTX_NOTICE,"$GLOBALS is a read-only array,deletion is forbidden");` |
|       ! 0 | 1692 | `		return SXRET_OK;` |
|         - | 1693 | `	}` |
|         - | 1694 | `	/* Start the release process */` |
|     93629 | 1695 | `	n = 0;` |
|     93629 | 1696 | `	pEntry = pMap->pFirst;` |
|   1683433 | 1697 | `	for(;;){` |
|   3366871 | 1698 | `		if( n >= pMap->nEntry ){` |
|     93629 | 1699 | `			break;` |
|         - | 1700 | `		}` |
|   3273247 | 1701 | `		pNext = pEntry->pPrev; /* Reverse link */` |
|         - | 1702 | `		/* Remove the reference from the foreign table */` |
|   3273247 | 1703 | `		PH7_VmRefObjRemove(pVm,pEntry->nValIdx,0,pEntry);` |
|   3273247 | 1704 | `		if( (pEntry->iFlags & HASHMAP_NODE_FOREIGN_OBJ) == 0 ){` |
|         - | 1705 | `			/* Restore the ph7_value to the free list */` |
|   3273237 | 1706 | `			PH7_VmUnsetMemObj(pVm,pEntry->nValIdx,FALSE);` |
|   1636616 | 1707 | `		}` |
|         - | 1708 | `		/* Release the node */` |
|   3273247 | 1709 | `		if( pEntry->iType == HASHMAP_BLOB_NODE ){` |
|    164819 | 1710 | `			SyBlobRelease(&pEntry->xKey.sKey);` |
|     82407 | 1711 | `		}` |
|   3273247 | 1712 | `		SyMemBackendPoolFree(&pVm->sAllocator,pEntry);` |
|         - | 1713 | `		/* Point to the next entry */` |
|   3273247 | 1714 | `		pEntry = pNext;` |
|   3273247 | 1715 | `		n++;` |
|         5 | 1716 | `	}` |
|     93629 | 1717 | `	if( pMap->nEntry > 0 ){` |
|         - | 1718 | `		/* Release the hash bucket */` |
|     71361 | 1719 | `		SyMemBackendFree(&pVm->sAllocator,pMap->apBucket);` |
|     35678 | 1720 | `	}` |
|     93629 | 1721 | `	if( FreeDS ){` |
|         - | 1722 | `		/* Free the whole instance */` |
|     93613 | 1723 | `		SyMemBackendPoolFree(&pVm->sAllocator,pMap);` |
|     46809 | 1724 | `	}else{` |
|         - | 1725 | `		/* Keep the instance but reset it's fields */` |
|        17 | 1726 | `		pMap->apBucket = 0;` |
|        17 | 1727 | `		pMap->iNextIdx = 0;` |
|        17 | 1728 | `		pMap->nEntry = pMap->nSize = 0;` |
|        17 | 1729 | `		pMap->pFirst = pMap->pLast = pMap->pCur = 0;` |
|         - | 1730 | `	}` |
|     93629 | 1731 | `	return SXRET_OK;` |
|     46817 | 1732 | `}` |
|         - | 1733 | `/*` |
|         - | 1734 | ` * Decrement the reference count of a given hashmap.` |
|         - | 1735 | ` * If the count reaches zero which mean no more variables` |
|         - | 1736 | ` * are pointing to this hashmap,then release the whole instance.` |
|         - | 1737 | ` */` |
|    802172 | 1738 | `PH7_PRIVATE void  PH7_HashmapUnref(ph7_hashmap *pMap)` |
|         5 | 1739 | `{` |
|    802177 | 1740 | `	ph7_vm *pVm = pMap->pVm;` |
|         - | 1741 | `	/* TICKET 1432-49: $GLOBALS is not subject to garbage collection */` |
|    802177 | 1742 | `	pMap->iRef--;` |
|    802177 | 1743 | `	if( pMap->iRef < 1 && pMap != pVm->pGlobal){` |
|     93593 | 1744 | `		PH7_HashmapRelease(pMap,TRUE);` |
|     46794 | 1745 | `	}` |
|    802177 | 1746 | `}` |
|         - | 1747 | `/*` |
|         - | 1748 | ` * Check if a given key exists in the given hashmap.` |
|         - | 1749 | ` * Write a pointer to the target node on success.` |
|         - | 1750 | ` * Otherwise SXERR_NOTFOUND is returned on failure.` |
|         - | 1751 | ` */` |
|    135322 | 1752 | `PH7_PRIVATE sxi32 PH7_HashmapLookup(` |
|         - | 1753 | `	ph7_hashmap *pMap,        /* Target hashmap */` |
|         - | 1754 | `	ph7_value *pKey,          /* Lookup key */` |
|         - | 1755 | `	ph7_hashmap_node **ppNode /* OUT: Target node on success */` |
|         - | 1756 | `	)` |
|         5 | 1757 | `{` |
|         - | 1758 | `	sxi32 rc;` |
|    135327 | 1759 | `	if( pMap->nEntry < 1 ){` |
|         - | 1760 | `		/* TICKET 1433-25: Don't bother hashing,the hashmap is empty anyway.` |
|         - | 1761 | `		 */` |
|       120 | 1762 | `		return SXERR_NOTFOUND;` |
|         - | 1763 | `	}` |
|    135211 | 1764 | `	rc = HashmapLookup(&(*pMap),&(*pKey),ppNode);` |
|    135211 | 1765 | `	return rc;` |
|     67666 | 1766 | `}` |
|         - | 1767 | `/*` |
|         - | 1768 | ` * Insert a given key and it's associated value (if any) in the given` |
|         - | 1769 | ` * hashmap.` |
|         - | 1770 | ` * If a node with the given key already exists in the database` |
|         - | 1771 | ` * then this function overwrite the old value.` |
|         - | 1772 | ` */` |
|   2668844 | 1773 | `PH7_PRIVATE sxi32 PH7_HashmapInsert(` |
|         - | 1774 | `	ph7_hashmap *pMap, /* Target hashmap */` |
|         - | 1775 | `	ph7_value *pKey,   /* Lookup key */` |
|         - | 1776 | `	ph7_value *pVal    /* Node value.NULL otherwise */` |
|         - | 1777 | `	)` |
|         5 | 1778 | `{` |
|         - | 1779 | `	sxi32 rc;` |
|         - | 1780 | `	/* Storing the $GLOBALS array itself as a VALUE is fine in php ($a[] =` |
|         - | 1781 | `	 * $GLOBALS copies the symbol table); the old TICKET 1433-35 guard that` |
|         - | 1782 | `	 * forbade it was a PH7-ism. Writes INTO $GLOBALS are handled inside` |
|         - | 1783 | `	 * HashmapInsert (they create real global variables, php 8.1). */` |
|   2668849 | 1784 | `	rc = HashmapInsert(&(*pMap),&(*pKey),&(*pVal));` |
|   2668849 | 1785 | `	return rc;` |
|         5 | 1786 | `}` |
|         - | 1787 | `/*` |
|         - | 1788 | ` * Merge entries of pSrc into pDest using PHP merge semantics:` |
|         - | 1789 | ` *   - String keys overwrite same-key entries in pDest.` |
|         - | 1790 | ` *   - Integer keys are renumbered with the destination's auto-index.` |
|         - | 1791 | ` * This is the same routine that backs array_merge().` |
|         - | 1792 | ` */` |
|        52 | 1793 | `PH7_PRIVATE sxi32 PH7_HashmapMerge(ph7_hashmap *pSrc,ph7_hashmap *pDest)` |
|         1 | 1794 | `{` |
|        53 | 1795 | `	return HashmapMerge(&(*pSrc),&(*pDest));` |
|         1 | 1796 | `}` |
|         - | 1797 | `/*` |
|         - | 1798 | ` * Insert a given key and it's associated value (foreign index) in the given` |
|         - | 1799 | ` * hashmap.` |
|         - | 1800 | ` * This is insertion by reference so be careful to mark the node` |
|         - | 1801 | ` * with the HASHMAP_NODE_FOREIGN_OBJ flag being set.` |
|         - | 1802 | ` * The insertion by reference is triggered when the following` |
|         - | 1803 | ` * expression is encountered.` |
|         - | 1804 | ` * $var = 10;` |
|         - | 1805 | ` *  $a = array(&var);` |
|         - | 1806 | ` * OR` |
|         - | 1807 | ` *  $a[] =& $var;` |
|         - | 1808 | ` * That is,$var is a foreign ph7_value and the $a array have no control` |
|         - | 1809 | ` * over it's contents.` |
|         - | 1810 | ` * Note that the node that hold the foreign ph7_value is automatically` |
|         - | 1811 | ` * removed when the foreign ph7_value is unset.` |
|         - | 1812 | ` * Example:` |
|         - | 1813 | ` *  $var = 10;` |
|         - | 1814 | ` *  $a[] =& $var;` |
|         - | 1815 | ` *  echo count($a).PHP_EOL; //1` |
|         - | 1816 | ` *  //Unset the foreign ph7_value now` |
|         - | 1817 | ` *  unset($var);` |
|         - | 1818 | ` *  echo count($a); //0` |
|         - | 1819 | ` * Note that this is a PH7 eXtension.` |
|         - | 1820 | ` * Refer to the official documentation for more information.` |
|         - | 1821 | ` * If a node with the given key already exists in the database` |
|         - | 1822 | ` * then this function overwrite the old value.` |
|         - | 1823 | ` */` |
|     46496 | 1824 | `PH7_PRIVATE sxi32 PH7_HashmapInsertByRef(` |
|         - | 1825 | `	ph7_hashmap *pMap, /* Target hashmap */` |
|         - | 1826 | `	ph7_value *pKey,   /* Lookup key */` |
|         - | 1827 | `	sxu32 nRefIdx      /* Foreign ph7_value index */` |
|         - | 1828 | `	)` |
|         5 | 1829 | `{` |
|         - | 1830 | `	sxi32 rc;` |
|     46501 | 1831 | `	if( nRefIdx == pMap->pVm->nGlobalIdx ){` |
|         - | 1832 | `		/* php's non-catchable fatal: $a[] =& $GLOBALS is forbidden (8.1) */` |
|       ! 0 | 1833 | `		PH7_VmThrowError(pMap->pVm,0,PH7_CTX_ERR,"Cannot acquire reference to $GLOBALS");` |
|       ! 0 | 1834 | `		pMap->pVm->iExitStatus = 255;` |
|       ! 0 | 1835 | `		pMap->pVm->bHaltRequested = 1;` |
|       ! 0 | 1836 | `		return PH7_ABORT;` |
|         - | 1837 | `	}` |
|     46501 | 1838 | `	rc = HashmapInsertByRef(&(*pMap),&(*pKey),nRefIdx);` |
|     46501 | 1839 | `	return rc;` |
|     23253 | 1840 | `}` |
|         - | 1841 | `/*` |
|         - | 1842 | ` * Reset the node cursor of a given hashmap.` |
|         - | 1843 | ` */` |
|     36776 | 1844 | `PH7_PRIVATE void PH7_HashmapResetLoopCursor(ph7_hashmap *pMap)` |
|         5 | 1845 | `{` |
|         - | 1846 | `	/* Reset the loop cursor */` |
|     36781 | 1847 | `	pMap->pCur = pMap->pFirst;` |
|     36781 | 1848 | `}` |
|         - | 1849 | `/*` |
|         - | 1850 | ` * Return a pointer to the node currently pointed by the node cursor.` |
|         - | 1851 | ` * If the cursor reaches the end of the list,then this function` |
|         - | 1852 | ` * return NULL.` |
|         - | 1853 | ` * Note that the node cursor is automatically advanced by this function.` |
|         - | 1854 | ` */` |
|    242416 | 1855 | `PH7_PRIVATE ph7_hashmap_node * PH7_HashmapGetNextEntry(ph7_hashmap *pMap)` |
|         5 | 1856 | `{` |
|    242421 | 1857 | `	ph7_hashmap_node *pCur = pMap->pCur;` |
|    242421 | 1858 | `	if( pCur == 0 ){` |
|         - | 1859 | `		/* End of the list,return null */` |
|     18381 | 1860 | `		return 0;` |
|         - | 1861 | `	}` |
|         - | 1862 | `	/* Advance the node cursor */` |
|    224045 | 1863 | `	pMap->pCur = pCur->pPrev; /* Reverse link */` |
|    224045 | 1864 | `	return pCur;` |
|    121213 | 1865 | `}` |
|         - | 1866 | `/*` |
|         - | 1867 | ` * Extract a node value.` |
|         - | 1868 | ` */` |
|    566726 | 1869 | `PH7_PRIVATE void PH7_HashmapExtractNodeValue(ph7_hashmap_node *pNode,ph7_value *pValue,int bStore)` |
|         5 | 1870 | `{` |
|    566731 | 1871 | `	ph7_value *pEntry = HashmapExtractNodeValue(pNode);` |
|    566731 | 1872 | `	if( pEntry ){` |
|    566731 | 1873 | `		if( bStore ){` |
|    224457 | 1874 | `			PH7_MemObjStore(pEntry,pValue);` |
|    112231 | 1875 | `		}else{` |
|    342279 | 1876 | `			PH7_MemObjLoad(pEntry,pValue);` |
|         - | 1877 | `		}` |
|    283417 | 1878 | `	}else{` |
|       ! 0 | 1879 | `		PH7_MemObjRelease(pValue);` |
|         - | 1880 | `	}` |
|    566731 | 1881 | `}` |
|         - | 1882 | `/*` |
|         - | 1883 | ` * Extract a node key.` |
|         - | 1884 | ` */` |
|    147162 | 1885 | `PH7_PRIVATE void PH7_HashmapExtractNodeKey(ph7_hashmap_node *pNode,ph7_value *pKey)` |
|         5 | 1886 | `{` |
|         - | 1887 | `	/* Fill with the current key */` |
|    147167 | 1888 | `	if( pNode->iType == HASHMAP_INT_NODE ){` |
|    142317 | 1889 | `		if( SyBlobLength(&pKey->sBlob) > 0 ){` |
|        31 | 1890 | `			SyBlobRelease(&pKey->sBlob);` |
|        15 | 1891 | `		}` |
|    142317 | 1892 | `		pKey->x.iVal = pNode->xKey.iKey;` |
|    142317 | 1893 | `		MemObjSetType(pKey,MEMOBJ_INT);` |
|     71161 | 1894 | `	}else{` |
|      4855 | 1895 | `		SyBlobReset(&pKey->sBlob);` |
|      4855 | 1896 | `		SyBlobAppend(&pKey->sBlob,SyBlobData(&pNode->xKey.sKey),SyBlobLength(&pNode->xKey.sKey));` |
|      4855 | 1897 | `		MemObjSetType(pKey,MEMOBJ_STRING);` |
|         - | 1898 | `	}` |
|    147167 | 1899 | `}` |
|         - | 1900 | `#ifndef PH7_DISABLE_DISK_IO` |
|         - | 1901 | `/*` |
|         - | 1902 | ` * Store the address of nodes value in the given container.` |
|         - | 1903 | ` * Refer to the [vfprintf(),vprintf(),vsprintf()] implementations` |
|         - | 1904 | ` * defined in 'builtin.c' for more information.` |
|         - | 1905 | ` */` |
|        12 | 1906 | `PH7_PRIVATE int PH7_HashmapValuesToSet(ph7_hashmap *pMap,SySet *pOut)` |
|         1 | 1907 | `{` |
|        13 | 1908 | `	ph7_hashmap_node *pEntry = pMap->pFirst;` |
|         - | 1909 | `	ph7_value *pValue;` |
|         - | 1910 | `	sxu32 n;` |
|         - | 1911 | `	/* Initialize the container */` |
|        13 | 1912 | `	SySetInit(pOut,&pMap->pVm->sAllocator,sizeof(ph7_value *));` |
|        33 | 1913 | `	for(n = 0 ; n < pMap->nEntry ; n++ ){` |
|         - | 1914 | `		/* Extract node value */` |
|        21 | 1915 | `		pValue = HashmapExtractNodeValue(pEntry);` |
|        21 | 1916 | `		if( pValue ){` |
|        21 | 1917 | `			SySetPut(pOut,(const void *)&pValue);` |
|        10 | 1918 | `		}` |
|         - | 1919 | `		/* Point to the next entry */` |
|        21 | 1920 | `		pEntry = pEntry->pPrev; /* Reverse link */` |
|        11 | 1921 | `	}` |
|         - | 1922 | `	/* Total inserted entries */` |
|        13 | 1923 | `	return (int)SySetUsed(pOut);` |
|         1 | 1924 | `}` |
|         - | 1925 | `#endif /* PH7_DISABLE_BUILTIN_FUNC \|\| PH7_DISABLE_DISK_IO */` |
|         - | 1926 | `/* SPDX-SnippetBegin */` |
|         - | 1927 | `/* SPDX-SnippetCopyrightText: D. Richard Hipp and the SQLite authors <https://sqlite.org/> */` |
|         - | 1928 | `/* SPDX-License-Identifier: blessing */` |
|         - | 1929 | `/*` |
|         - | 1930 | ` * Merge sort.` |
|         - | 1931 | ` * The merge sort implementation is based on the one found in the SQLite3 source tree.` |
|         - | 1932 | ` * Status: Public domain` |
|         - | 1933 | ` */` |
|         - | 1934 | `/* Node comparison callback signature */` |
|         - | 1935 | `typedef sxi32 (*ProcNodeCmp)(ph7_hashmap_node *,ph7_hashmap_node *,void *);` |
|         - | 1936 | `/*` |
|         - | 1937 | `** Inputs:` |
|         - | 1938 | `**   a:       A sorted, null-terminated linked list.  (May be null).` |
|         - | 1939 | `**   b:       A sorted, null-terminated linked list.  (May be null).` |
|         - | 1940 | `**   cmp:     A pointer to the comparison function.` |
|         - | 1941 | `**` |
|         - | 1942 | `** Return Value:` |
|         - | 1943 | `**   A pointer to the head of a sorted list containing the elements` |
|         - | 1944 | `**   of both a and b.` |
|         - | 1945 | `**` |
|         - | 1946 | `** Side effects:` |
|         - | 1947 | `**   The "next","prev" pointers for elements in the lists a and b are` |
|         - | 1948 | `**   changed.` |
|         - | 1949 | `*/` |
|     34774 | 1950 | `static ph7_hashmap_node * HashmapNodeMerge(ph7_hashmap_node *pA,ph7_hashmap_node *pB,ProcNodeCmp xCmp,void *pCmpData)` |
|         5 | 1951 | `{` |
|         - | 1952 | `	ph7_hashmap_node result,*pTail;` |
|         - | 1953 | `    /* Prevent compiler warning */` |
|     34779 | 1954 | `	result.pNext = result.pPrev = 0;` |
|     34779 | 1955 | `	pTail = &result;` |
|    105335 | 1956 | `	while( pA && pB ){` |
|     70561 | 1957 | `		if( xCmp(pA,pB,pCmpData) <= 0 ){` |
|     46814 | 1958 | `			pTail->pPrev = pA;` |
|     46814 | 1959 | `			pA->pNext = pTail;` |
|     46814 | 1960 | `			pTail = pA;` |
|     46814 | 1961 | `			pA = pA->pPrev;` |
|     23409 | 1962 | `		}else{` |
|     23752 | 1963 | `			pTail->pPrev = pB;` |
|     23752 | 1964 | `			pB->pNext = pTail;` |
|     23752 | 1965 | `			pTail = pB;` |
|     23752 | 1966 | `			pB = pB->pPrev;` |
|         - | 1967 | `		}` |
|         5 | 1968 | `	}` |
|     34779 | 1969 | `	if( pA ){` |
|     24435 | 1970 | `		pTail->pPrev = pA;` |
|     24435 | 1971 | `		pA->pNext = pTail;` |
|     22573 | 1972 | `	}else if( pB ){` |
|     10125 | 1973 | `		pTail->pPrev = pB;` |
|     10125 | 1974 | `		pB->pNext = pTail;` |
|      5056 | 1975 | `	}else{` |
|       229 | 1976 | `		pTail->pPrev = pTail->pNext = 0;` |
|         - | 1977 | `	}` |
|     34779 | 1978 | `	return result.pPrev;` |
|         5 | 1979 | `}` |
|         - | 1980 | `/*` |
|         - | 1981 | `** Inputs:` |
|         - | 1982 | `**   Map:       Input hashmap` |
|         - | 1983 | `**   cmp:       A comparison function.` |
|         - | 1984 | `**` |
|         - | 1985 | `** Return Value:` |
|         - | 1986 | `**   Sorted hashmap.` |
|         - | 1987 | `**` |
|         - | 1988 | `** Side effects:` |
|         - | 1989 | `**   The "next" pointers for elements in list are changed.` |
|         - | 1990 | `*/` |
|         - | 1991 | `#define N_SORT_BUCKET  32` |
|       722 | 1992 | `static sxi32 HashmapMergeSort(ph7_hashmap *pMap,ProcNodeCmp xCmp,void *pCmpData)` |
|         5 | 1993 | `{` |
|         - | 1994 | `	ph7_hashmap_node *a[N_SORT_BUCKET], *p,*pIn;` |
|         - | 1995 | `	sxu32 i;` |
|       727 | 1996 | `	SyZero(a,sizeof(a));` |
|         - | 1997 | `	/* Point to the first inserted entry */` |
|       727 | 1998 | `	pIn = pMap->pFirst;` |
|     14389 | 1999 | `	while( pIn ){` |
|     13667 | 2000 | `		p = pIn;` |
|     13667 | 2001 | `		pIn = p->pPrev;` |
|     13667 | 2002 | `		p->pPrev = 0;` |
|     26059 | 2003 | `		for(i=0; i<N_SORT_BUCKET-1; i++){` |
|     26059 | 2004 | `			if( a[i]==0 ){` |
|     13667 | 2005 | `				a[i] = p;` |
|     13667 | 2006 | `				break;` |
|       ! 0 | 2007 | `			}else{` |
|     12397 | 2008 | `				p = HashmapNodeMerge(a[i],p,xCmp,pCmpData);` |
|     12397 | 2009 | `				a[i] = 0;` |
|         - | 2010 | `			}` |
|      6201 | 2011 | `		}` |
|     13667 | 2012 | `		if( i==N_SORT_BUCKET-1 ){` |
|         - | 2013 | `			/* To get here, there need to be 2^(N_SORT_BUCKET) elements in he input list.` |
|         - | 2014 | `			 * But that is impossible.` |
|         - | 2015 | `			 */` |
|       ! 0 | 2016 | `			a[i] = HashmapNodeMerge(a[i], p,xCmp,pCmpData);` |
|       ! 0 | 2017 | `		}` |
|         5 | 2018 | `	}` |
|       727 | 2019 | `	p = a[0];` |
|     23109 | 2020 | `	for(i=1; i<N_SORT_BUCKET; i++){` |
|     22387 | 2021 | `		p = HashmapNodeMerge(p,a[i],xCmp,pCmpData);` |
|     11196 | 2022 | `	}` |
|       727 | 2023 | `	p->pNext = 0;` |
|         - | 2024 | `	/* Reflect the change */` |
|       727 | 2025 | `	pMap->pFirst = p;` |
|         - | 2026 | `	/* Reset the loop cursor */` |
|       727 | 2027 | `	pMap->pCur = pMap->pFirst;` |
|       727 | 2028 | `	return SXRET_OK;` |
|         5 | 2029 | `}` |
|         - | 2030 | `/* SPDX-SnippetEnd */` |
|         - | 2031 | `/*` |
|         - | 2032 | ` * Node comparison callback.` |
|         - | 2033 | ` * used-by: [sort(),asort(),...]` |
|         - | 2034 | ` */` |
|     70339 | 2035 | `static sxi32 HashmapCmpCallback1(ph7_hashmap_node *pA,ph7_hashmap_node *pB,void *pCmpData)` |
|         5 | 2036 | `{` |
|         - | 2037 | `	ph7_value sA,sB;` |
|         - | 2038 | `	sxi32 iFlags;` |
|         - | 2039 | `	int rc;` |
|     70344 | 2040 | `	if( pCmpData == 0 ){` |
|         - | 2041 | `		/* Perform a standard comparison */` |
|     70320 | 2042 | `		rc = HashmapNodeCmp(pA,pB,FALSE);` |
|     70320 | 2043 | `		return rc;` |
|         - | 2044 | `	}` |
|        25 | 2045 | `	iFlags = SX_PTR_TO_INT(pCmpData);` |
|         - | 2046 | `	/* Duplicate node values */` |
|        25 | 2047 | `	PH7_MemObjInit(pA->pMap->pVm,&sA);` |
|        25 | 2048 | `	PH7_MemObjInit(pA->pMap->pVm,&sB);` |
|        25 | 2049 | `	PH7_HashmapExtractNodeValue(pA,&sA,FALSE);` |
|        25 | 2050 | `	PH7_HashmapExtractNodeValue(pB,&sB,FALSE);` |
|        25 | 2051 | `	if( iFlags == 5 ){` |
|         - | 2052 | `		/* String cast */` |
|         - | 2053 | `		const char *zA,*zB;` |
|         - | 2054 | `		sxu32 nA,nB,nMin;` |
|        15 | 2055 | `		if( (sA.iFlags & MEMOBJ_STRING) == 0 ){` |
|       ! 0 | 2056 | `			PH7_MemObjToString(&sA);` |
|       ! 0 | 2057 | `		}` |
|        15 | 2058 | `		if( (sB.iFlags & MEMOBJ_STRING) == 0 ){` |
|       ! 0 | 2059 | `			PH7_MemObjToString(&sB);` |
|       ! 0 | 2060 | `		}` |
|         - | 2061 | `		/* Lexicographic string comparison to avoid numeric string coercion */` |
|        15 | 2062 | `		zA = (const char *)SyBlobData(&sA.sBlob);` |
|        15 | 2063 | `		zB = (const char *)SyBlobData(&sB.sBlob);` |
|        15 | 2064 | `		nA = SyBlobLength(&sA.sBlob);` |
|        15 | 2065 | `		nB = SyBlobLength(&sB.sBlob);` |
|        15 | 2066 | `		nMin = nA < nB ? nA : nB;` |
|        15 | 2067 | `		rc = SyMemcmp(zA,zB,nMin);` |
|        15 | 2068 | `		if( rc == 0 ){` |
|         5 | 2069 | `			if( nA < nB ) rc = -1;` |
|       ! 0 | 2070 | `			else if( nA > nB ) rc = 1;` |
|         2 | 2071 | `		}` |
|         8 | 2072 | `	}else{` |
|         - | 2073 | `		/* Numeric cast */` |
|        11 | 2074 | `		PH7_MemObjToNumeric(&sA);` |
|        11 | 2075 | `		PH7_MemObjToNumeric(&sB);` |
|        11 | 2076 | `		rc = PH7_MemObjCmp(&sA,&sB,FALSE,0);` |
|         - | 2077 | `	}` |
|        25 | 2078 | `	PH7_MemObjRelease(&sA);` |
|        25 | 2079 | `	PH7_MemObjRelease(&sB);` |
|        25 | 2080 | `	return rc;` |
|     35199 | 2081 | `}` |
|         - | 2082 | `/*` |
|         - | 2083 | ` * Node comparison callback: Compare nodes by keys only.` |
|         - | 2084 | ` * used-by: [ksort()]` |
|         - | 2085 | ` */` |
|        16 | 2086 | `static sxi32 HashmapCmpCallback2(ph7_hashmap_node *pA,ph7_hashmap_node *pB,void *pCmpData)` |
|         1 | 2087 | `{` |
|         - | 2088 | `	sxi32 rc;` |
|         8 | 2089 | `	SXUNUSED(pCmpData); /* cc warning */` |
|        17 | 2090 | `	if( pA->iType == HASHMAP_BLOB_NODE && pB->iType == HASHMAP_BLOB_NODE ){` |
|         - | 2091 | `		/* Perform a string comparison */` |
|         7 | 2092 | `		rc = SyBlobCmp(&pA->xKey.sKey,&pB->xKey.sKey);` |
|         4 | 2093 | `	}else{` |
|         - | 2094 | `		SyString sStr;` |
|         - | 2095 | `		sxi64 iA,iB;` |
|         - | 2096 | `		/* Perform a numeric comparison */` |
|        11 | 2097 | `		if( pA->iType == HASHMAP_BLOB_NODE ){` |
|         - | 2098 | `			/* Cast to 64-bit integer */` |
|       ! 0 | 2099 | `			SyStringInitFromBuf(&sStr,SyBlobData(&pA->xKey.sKey),SyBlobLength(&pA->xKey.sKey));` |
|       ! 0 | 2100 | `			if( sStr.nByte < 1 ){` |
|       ! 0 | 2101 | `				iA = 0;` |
|       ! 0 | 2102 | `			}else{` |
|       ! 0 | 2103 | `				SyStrToInt64(sStr.zString,sStr.nByte,(void *)&iA,0);` |
|         - | 2104 | `			}` |
|       ! 0 | 2105 | `		}else{` |
|        11 | 2106 | `			iA = pA->xKey.iKey;` |
|         - | 2107 | `		}` |
|        11 | 2108 | `		if( pB->iType == HASHMAP_BLOB_NODE ){` |
|         - | 2109 | `			/* Cast to 64-bit integer */` |
|       ! 0 | 2110 | `			SyStringInitFromBuf(&sStr,SyBlobData(&pB->xKey.sKey),SyBlobLength(&pB->xKey.sKey));` |
|       ! 0 | 2111 | `			if( sStr.nByte < 1 ){` |
|       ! 0 | 2112 | `				iB = 0;` |
|       ! 0 | 2113 | `			}else{` |
|       ! 0 | 2114 | `				SyStrToInt64(sStr.zString,sStr.nByte,(void *)&iB,0);` |
|         - | 2115 | `			}` |
|       ! 0 | 2116 | `		}else{` |
|        11 | 2117 | `			iB = pB->xKey.iKey;` |
|         - | 2118 | `		}` |
|        11 | 2119 | `		rc = (sxi32)(iA-iB);` |
|         - | 2120 | `	}` |
|         - | 2121 | `	/* Comparison result */` |
|        17 | 2122 | `	return rc;` |
|         1 | 2123 | `}` |
|         - | 2124 | `/*` |
|         - | 2125 | ` * Node comparison callback.` |
|         - | 2126 | ` * Used by: [rsort(),arsort()];` |
|         - | 2127 | ` */` |
|        78 | 2128 | `static sxi32 HashmapCmpCallback3(ph7_hashmap_node *pA,ph7_hashmap_node *pB,void *pCmpData)` |
|         1 | 2129 | `{` |
|         - | 2130 | `	ph7_value sA,sB;` |
|         - | 2131 | `	sxi32 iFlags;` |
|         - | 2132 | `	int rc;` |
|        79 | 2133 | `	if( pCmpData == 0 ){` |
|         - | 2134 | `		/* Perform a standard comparison */` |
|        59 | 2135 | `		rc = HashmapNodeCmp(pA,pB,FALSE);` |
|        59 | 2136 | `		return -rc;` |
|         - | 2137 | `	}` |
|        21 | 2138 | `	iFlags = SX_PTR_TO_INT(pCmpData);` |
|         - | 2139 | `	/* Duplicate node values */` |
|        21 | 2140 | `	PH7_MemObjInit(pA->pMap->pVm,&sA);` |
|        21 | 2141 | `	PH7_MemObjInit(pA->pMap->pVm,&sB);` |
|        21 | 2142 | `	PH7_HashmapExtractNodeValue(pA,&sA,FALSE);` |
|        21 | 2143 | `	PH7_HashmapExtractNodeValue(pB,&sB,FALSE);` |
|        21 | 2144 | `	if( iFlags == 5 ){` |
|         - | 2145 | `		/* String cast */` |
|         - | 2146 | `		const char *zA,*zB;` |
|         - | 2147 | `		sxu32 nA,nB,nMin;` |
|        11 | 2148 | `		if( (sA.iFlags & MEMOBJ_STRING) == 0 ){` |
|       ! 0 | 2149 | `			PH7_MemObjToString(&sA);` |
|       ! 0 | 2150 | `		}` |
|        11 | 2151 | `		if( (sB.iFlags & MEMOBJ_STRING) == 0 ){` |
|       ! 0 | 2152 | `			PH7_MemObjToString(&sB);` |
|       ! 0 | 2153 | `		}` |
|         - | 2154 | `		/* Lexicographic string comparison to avoid numeric string coercion */` |
|        11 | 2155 | `		zA = (const char *)SyBlobData(&sA.sBlob);` |
|        11 | 2156 | `		zB = (const char *)SyBlobData(&sB.sBlob);` |
|        11 | 2157 | `		nA = SyBlobLength(&sA.sBlob);` |
|        11 | 2158 | `		nB = SyBlobLength(&sB.sBlob);` |
|        11 | 2159 | `		nMin = nA < nB ? nA : nB;` |
|        11 | 2160 | `		rc = SyMemcmp(zA,zB,nMin);` |
|        11 | 2161 | `		if( rc == 0 ){` |
|         3 | 2162 | `			if( nA < nB ) rc = -1;` |
|       ! 0 | 2163 | `			else if( nA > nB ) rc = 1;` |
|         1 | 2164 | `		}` |
|         6 | 2165 | `	}else{` |
|         - | 2166 | `		/* Numeric cast */` |
|        11 | 2167 | `		PH7_MemObjToNumeric(&sA);` |
|        11 | 2168 | `		PH7_MemObjToNumeric(&sB);` |
|        11 | 2169 | `		rc = PH7_MemObjCmp(&sA,&sB,FALSE,0);` |
|         - | 2170 | `	}` |
|        21 | 2171 | `	PH7_MemObjRelease(&sA);` |
|        21 | 2172 | `	PH7_MemObjRelease(&sB);` |
|        21 | 2173 | `	return -rc;` |
|        40 | 2174 | `}` |
|         - | 2175 | `/*` |
|         - | 2176 | ` * Node comparison callback: Invoke an user-defined callback for the purpose of node comparison.` |
|         - | 2177 | ` * used-by: [usort(),uasort()]` |
|         - | 2178 | ` */` |
|        94 | 2179 | `static sxi32 HashmapCmpCallback4(ph7_hashmap_node *pA,ph7_hashmap_node *pB,void *pCmpData)` |
|         3 | 2180 | `{` |
|         - | 2181 | `	ph7_value sResult,*pCallback;` |
|         - | 2182 | `	ph7_value *pV1,*pV2;` |
|         - | 2183 | `	ph7_value *apArg[2];  /* Callback arguments */` |
|         - | 2184 | `	sxi32 rc;` |
|         - | 2185 | `	/* Point to the desired callback */` |
|        97 | 2186 | `	pCallback = (ph7_value *)pCmpData;` |
|        97 | 2187 | `	if( pA->pMap->pVm->iCmpCallbackExc ){` |
|         - | 2188 | `		/* A previous comparison already raised: stop invoking the callback so` |
|         - | 2189 | `		 * the exception is not thrown again, and let the sort wind down. */` |
|         8 | 2190 | `		return 0;` |
|         - | 2191 | `	}` |
|         - | 2192 | `	/* initialize the result value */` |
|        91 | 2193 | `	PH7_MemObjInit(pA->pMap->pVm,&sResult);` |
|         - | 2194 | `	/* Extract nodes values */` |
|        91 | 2195 | `	pV1 = HashmapExtractNodeValue(pA);` |
|        91 | 2196 | `	pV2 = HashmapExtractNodeValue(pB);` |
|        91 | 2197 | `	apArg[0] = pV1;` |
|        91 | 2198 | `	apArg[1] = pV2;` |
|         - | 2199 | `	/* Invoke the callback */` |
|        91 | 2200 | `	rc = PH7_VmCallUserFunction(pA->pMap->pVm,pCallback,2,apArg,&sResult);` |
|        91 | 2201 | `	if( rc == PH7_EXCEPTION ){` |
|         - | 2202 | `		/* The comparator raised: flag it so the sort driver aborts and` |
|         - | 2203 | `		 * propagates, and order this pair arbitrarily for the rest of the run. */` |
|        10 | 2204 | `		pA->pMap->pVm->iCmpCallbackExc = 1;` |
|        10 | 2205 | `		rc = 0;` |
|        86 | 2206 | `	}else if( rc != SXRET_OK ){` |
|         - | 2207 | `		/* An error occured while calling user defined function [i.e: not defined] */` |
|       ! 0 | 2208 | `		rc = -1; /* Set a dummy result */` |
|       ! 0 | 2209 | `	}else{` |
|         - | 2210 | `		/* Extract callback result */` |
|        82 | 2211 | `		if((sResult.iFlags & MEMOBJ_INT) == 0 ){` |
|         - | 2212 | `			/* Perform an int cast */` |
|       ! 0 | 2213 | `			PH7_MemObjToInteger(&sResult);` |
|       ! 0 | 2214 | `		}` |
|        82 | 2215 | `		rc = (sxi32)sResult.x.iVal;` |
|         - | 2216 | `	}` |
|        91 | 2217 | `	PH7_MemObjRelease(&sResult);` |
|         - | 2218 | `	/* Callback result */` |
|        91 | 2219 | `	return rc;` |
|        50 | 2220 | `}` |
|         - | 2221 | `/*` |
|         - | 2222 | ` * Node comparison callback: Compare nodes by keys only.` |
|         - | 2223 | ` * used-by: [krsort()]` |
|         - | 2224 | ` */` |
|         4 | 2225 | `static sxi32 HashmapCmpCallback5(ph7_hashmap_node *pA,ph7_hashmap_node *pB,void *pCmpData)` |
|         1 | 2226 | `{` |
|         - | 2227 | `	sxi32 rc;` |
|         2 | 2228 | `	SXUNUSED(pCmpData); /* cc warning */` |
|         5 | 2229 | `	if( pA->iType == HASHMAP_BLOB_NODE && pB->iType == HASHMAP_BLOB_NODE ){` |
|         - | 2230 | `		/* Perform a string comparison */` |
|         5 | 2231 | `		rc = SyBlobCmp(&pA->xKey.sKey,&pB->xKey.sKey);` |
|         3 | 2232 | `	}else{` |
|         - | 2233 | `		SyString sStr;` |
|         - | 2234 | `		sxi64 iA,iB;` |
|         - | 2235 | `		/* Perform a numeric comparison */` |
|       ! 0 | 2236 | `		if( pA->iType == HASHMAP_BLOB_NODE ){` |
|         - | 2237 | `			/* Cast to 64-bit integer */` |
|       ! 0 | 2238 | `			SyStringInitFromBuf(&sStr,SyBlobData(&pA->xKey.sKey),SyBlobLength(&pA->xKey.sKey));` |
|       ! 0 | 2239 | `			if( sStr.nByte < 1 ){` |
|       ! 0 | 2240 | `				iA = 0;` |
|       ! 0 | 2241 | `			}else{` |
|       ! 0 | 2242 | `				SyStrToInt64(sStr.zString,sStr.nByte,(void *)&iA,0);` |
|         - | 2243 | `			}` |
|       ! 0 | 2244 | `		}else{` |
|       ! 0 | 2245 | `			iA = pA->xKey.iKey;` |
|         - | 2246 | `		}` |
|       ! 0 | 2247 | `		if( pB->iType == HASHMAP_BLOB_NODE ){` |
|         - | 2248 | `			/* Cast to 64-bit integer */` |
|       ! 0 | 2249 | `			SyStringInitFromBuf(&sStr,SyBlobData(&pB->xKey.sKey),SyBlobLength(&pB->xKey.sKey));` |
|       ! 0 | 2250 | `			if( sStr.nByte < 1 ){` |
|       ! 0 | 2251 | `				iB = 0;` |
|       ! 0 | 2252 | `			}else{` |
|       ! 0 | 2253 | `				SyStrToInt64(sStr.zString,sStr.nByte,(void *)&iB,0);` |
|         - | 2254 | `			}` |
|       ! 0 | 2255 | `		}else{` |
|       ! 0 | 2256 | `			iB = pB->xKey.iKey;` |
|         - | 2257 | `		}` |
|       ! 0 | 2258 | `		rc = (sxi32)(iA-iB);` |
|         - | 2259 | `	}` |
|         5 | 2260 | `	return -rc; /* Reverse result */` |
|         1 | 2261 | `}` |
|         - | 2262 | `/*` |
|         - | 2263 | ` * Node comparison callback: Invoke an user-defined callback for the purpose of node comparison.` |
|         - | 2264 | ` * used-by: [uksort()]` |
|         - | 2265 | ` */` |
|         6 | 2266 | `static sxi32 HashmapCmpCallback6(ph7_hashmap_node *pA,ph7_hashmap_node *pB,void *pCmpData)` |
|         1 | 2267 | `{` |
|         - | 2268 | `	ph7_value sResult,*pCallback;` |
|         - | 2269 | `	ph7_value *apArg[2];  /* Callback arguments */` |
|         - | 2270 | `	ph7_value sK1,sK2;` |
|         - | 2271 | `	sxi32 rc;` |
|         - | 2272 | `	/* Point to the desired callback */` |
|         7 | 2273 | `	pCallback = (ph7_value *)pCmpData;` |
|         7 | 2274 | `	if( pA->pMap->pVm->iCmpCallbackExc ){` |
|         - | 2275 | `		/* A previous comparison already raised: stop invoking the callback so` |
|         - | 2276 | `		 * the exception is not thrown again, and let the sort wind down. */` |
|       ! 0 | 2277 | `		return 0;` |
|         - | 2278 | `	}` |
|         - | 2279 | `	/* initialize the result value */` |
|         7 | 2280 | `	PH7_MemObjInit(pA->pMap->pVm,&sResult);` |
|         7 | 2281 | `	PH7_MemObjInit(pA->pMap->pVm,&sK1);` |
|         7 | 2282 | `	PH7_MemObjInit(pA->pMap->pVm,&sK2);` |
|         - | 2283 | `	/* Extract nodes keys */` |
|         7 | 2284 | `	PH7_HashmapExtractNodeKey(pA,&sK1);` |
|         7 | 2285 | `	PH7_HashmapExtractNodeKey(pB,&sK2);` |
|         7 | 2286 | `	apArg[0] = &sK1;` |
|         7 | 2287 | `	apArg[1] = &sK2;` |
|         - | 2288 | `	/* Mark keys as constants */` |
|         7 | 2289 | `	sK1.nIdx = SXU32_HIGH;` |
|         7 | 2290 | `	sK2.nIdx = SXU32_HIGH;` |
|         - | 2291 | `	/* Invoke the callback */` |
|         7 | 2292 | `	rc = PH7_VmCallUserFunction(pA->pMap->pVm,pCallback,2,apArg,&sResult);` |
|         7 | 2293 | `	if( rc == PH7_EXCEPTION ){` |
|         - | 2294 | `		/* The comparator raised: flag it so the sort driver aborts and` |
|         - | 2295 | `		 * propagates, and order this pair arbitrarily for the rest of the run. */` |
|       ! 0 | 2296 | `		pA->pMap->pVm->iCmpCallbackExc = 1;` |
|       ! 0 | 2297 | `		rc = 0;` |
|         7 | 2298 | `	}else if( rc != SXRET_OK ){` |
|         - | 2299 | `		/* An error occured while calling user defined function [i.e: not defined] */` |
|       ! 0 | 2300 | `		rc = -1; /* Set a dummy result */` |
|       ! 0 | 2301 | `	}else{` |
|         - | 2302 | `		/* Extract callback result */` |
|         7 | 2303 | `		if((sResult.iFlags & MEMOBJ_INT) == 0 ){` |
|         - | 2304 | `			/* Perform an int cast */` |
|       ! 0 | 2305 | `			PH7_MemObjToInteger(&sResult);` |
|       ! 0 | 2306 | `		}` |
|         7 | 2307 | `		rc = (sxi32)sResult.x.iVal;` |
|         - | 2308 | `	}` |
|         7 | 2309 | `	PH7_MemObjRelease(&sResult);` |
|         7 | 2310 | `	PH7_MemObjRelease(&sK1);` |
|         7 | 2311 | `	PH7_MemObjRelease(&sK2);` |
|         - | 2312 | `	/* Callback result */` |
|         7 | 2313 | `	return rc;` |
|         4 | 2314 | `}` |
|         - | 2315 | `/*` |
|         - | 2316 | ` * Node comparison callback: Random node comparison.` |
|         - | 2317 | ` * used-by: [shuffle()]` |
|         - | 2318 | ` */` |
|        19 | 2319 | `static sxi32 HashmapCmpCallback7(ph7_hashmap_node *pA,ph7_hashmap_node *pB,void *pCmpData)` |
|         1 | 2320 | `{` |
|         - | 2321 | `	sxu32 n;` |
|        10 | 2322 | `	SXUNUSED(pB); /* cc warning */` |
|        10 | 2323 | `	SXUNUSED(pCmpData);` |
|         - | 2324 | `	/* Grab a random number */` |
|        20 | 2325 | `	n = PH7_VmRandomNum(pA->pMap->pVm);` |
|         - | 2326 | `	/* if the random number is odd then the first node 'pA' is greater then` |
|         - | 2327 | `	 * the second node 'pB'. Otherwise the reverse is assumed.` |
|         - | 2328 | `	 */` |
|        20 | 2329 | `	return n&1 ? 1 : -1;` |
|         1 | 2330 | `}` |
|         - | 2331 | `/*` |
|         - | 2332 | ` * Rehash all nodes keys after a merge-sort have been applied.` |
|         - | 2333 | ` * Used by [sort(),usort() and rsort()].` |
|         - | 2334 | ` */` |
|       672 | 2335 | `static void HashmapSortRehash(ph7_hashmap *pMap)` |
|         5 | 2336 | `{` |
|         - | 2337 | `	ph7_hashmap_node *p,*pLast;` |
|         - | 2338 | `	sxu32 i;` |
|         - | 2339 | `	/* Rehash all entries */` |
|       677 | 2340 | `	pLast = p = pMap->pFirst;` |
|       677 | 2341 | `	pMap->iNextIdx = 0; /* Reset the automatic index */` |
|       677 | 2342 | `	i = 0;` |
|      7080 | 2343 | `	for( ;; ){` |
|     14165 | 2344 | `		if( i >= pMap->nEntry ){` |
|       677 | 2345 | `			pMap->pLast = pLast; /* Fix the last link broken by the merge-sort */` |
|       677 | 2346 | `			break;` |
|         - | 2347 | `		}` |
|     13493 | 2348 | `		if( p->iType == HASHMAP_BLOB_NODE ){` |
|         - | 2349 | `			/* Do not maintain index association as requested by the PHP specification */` |
|         5 | 2350 | `			SyBlobRelease(&p->xKey.sKey);` |
|         - | 2351 | `			/* Change key type */` |
|         5 | 2352 | `			p->iType = HASHMAP_INT_NODE;` |
|         2 | 2353 | `		}` |
|     13493 | 2354 | `		HashmapRehashIntNode(p);` |
|         - | 2355 | `		/* Point to the next entry */` |
|     13493 | 2356 | `		i++;` |
|     13493 | 2357 | `		pLast = p;` |
|     13493 | 2358 | `		p = p->pPrev; /* Reverse link */` |
|         5 | 2359 | `	}` |
|       677 | 2360 | `}` |
|         - | 2361 | `/*` |
|         - | 2362 | ` * Array functions implementation.` |
|         - | 2363 | ` * Status:` |
|         - | 2364 | ` *  Stable.` |
|         - | 2365 | ` */` |
|         - | 2366 | `/*` |
|         - | 2367 | ` * bool sort(array &$array[,int $sort_flags = SORT_REGULAR ] )` |
|         - | 2368 | ` * Sort an array.` |
|         - | 2369 | ` * Parameters` |
|         - | 2370 | ` *  $array` |
|         - | 2371 | ` *   The input array.` |
|         - | 2372 | ` * $sort_flags` |
|         - | 2373 | ` *  The optional second parameter sort_flags may be used to modify the sorting behavior using these values:` |
|         - | 2374 | ` *  Sorting type flags:` |
|         - | 2375 | ` *   SORT_REGULAR - compare items normally (don't change types)` |
|         - | 2376 | ` *   SORT_NUMERIC - compare items numerically` |
|         - | 2377 | ` *   SORT_STRING - compare items as strings` |
|         - | 2378 | ` * Return` |
|         - | 2379 | ` *  TRUE on success or FALSE on failure.` |
|         - | 2380 | ` *` |
|         - | 2381 | ` */` |
|      1000 | 2382 | `static int ph7_hashmap_sort(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|         5 | 2383 | `{` |
|         - | 2384 | `	ph7_hashmap *pMap;` |
|         - | 2385 | `	/* Make sure we are dealing with a valid hashmap */` |
|      1005 | 2386 | `	if( nArg < 1 \|\| !ph7_value_is_array(apArg[0]) ){` |
|         - | 2387 | `		/* Missing/Invalid arguments,return FALSE */` |
|       ! 0 | 2388 | `		ph7_result_bool(pCtx,0);` |
|       ! 0 | 2389 | `		return PH7_OK;` |
|         - | 2390 | `	}` |
|         - | 2391 | `	/* Point to the internal representation of the input hashmap */` |
|      1005 | 2392 | `	PH7_HashmapCowSeparate(pCtx->pVm, apArg[0]);` |
|      1005 | 2393 | `	pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|      1005 | 2394 | `	if( pMap->nEntry > 1 ){` |
|       655 | 2395 | `		sxi32 iCmpFlags = 0;` |
|       655 | 2396 | `		if( nArg > 1 ){` |
|         - | 2397 | `			/* Extract comparison flags */` |
|         3 | 2398 | `			iCmpFlags = ph7_value_to_int(apArg[1]);` |
|         3 | 2399 | `			if( iCmpFlags == 3 /* SORT_REGULAR */ ){` |
|       ! 0 | 2400 | `				iCmpFlags = 0; /* Standard comparison */` |
|       ! 0 | 2401 | `			}` |
|         1 | 2402 | `		}` |
|         - | 2403 | `		/* Do the merge sort */` |
|       655 | 2404 | `		HashmapMergeSort(pMap,HashmapCmpCallback1,SX_INT_TO_PTR(iCmpFlags));` |
|         - | 2405 | `		/* Rehash [Do not maintain index association as requested by the PHP specification] */` |
|       655 | 2406 | `		HashmapSortRehash(pMap);` |
|       325 | 2407 | `	}` |
|         - | 2408 | `	/* All done,return TRUE */` |
|      1005 | 2409 | `	ph7_result_bool(pCtx,1);` |
|      1005 | 2410 | `	return PH7_OK;` |
|       505 | 2411 | `}` |
|         - | 2412 | `/*` |
|         - | 2413 | ` * bool asort(array &$array[,int $sort_flags = SORT_REGULAR ] )` |
|         - | 2414 | ` *  Sort an array and maintain index association.` |
|         - | 2415 | ` * Parameters` |
|         - | 2416 | ` *  $array` |
|         - | 2417 | ` *   The input array.` |
|         - | 2418 | ` * $sort_flags` |
|         - | 2419 | ` *  The optional second parameter sort_flags may be used to modify the sorting behavior using these values:` |
|         - | 2420 | ` *  Sorting type flags:` |
|         - | 2421 | ` *   SORT_REGULAR - compare items normally (don't change types)` |
|         - | 2422 | ` *   SORT_NUMERIC - compare items numerically` |
|         - | 2423 | ` *   SORT_STRING - compare items as strings` |
|         - | 2424 | ` * Return` |
|         - | 2425 | ` *  TRUE on success or FALSE on failure.` |
|         - | 2426 | ` */` |
|        32 | 2427 | `static int ph7_hashmap_asort(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|         5 | 2428 | `{` |
|         - | 2429 | `	ph7_hashmap *pMap;` |
|         - | 2430 | `	/* PHP 8: ArgumentCountError if no arguments */` |
|        37 | 2431 | `	if( nArg < 1 ){` |
|         3 | 2432 | `		return PH7_VmThrowException(pCtx,` |
|         - | 2433 | `			"ArgumentCountError",` |
|         - | 2434 | `			"asort() expects at least 1 argument, 0 given"` |
|         - | 2435 | `			);` |
|         - | 2436 | `	}` |
|         - | 2437 | `	/* PHP 8: TypeError if first argument is not an array */` |
|        35 | 2438 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|        16 | 2439 | `		return PH7_VmThrowException(pCtx,` |
|         - | 2440 | `			"TypeError",` |
|         - | 2441 | `			"asort(): Argument #1 ($array) must be of type array, %s given",` |
|         4 | 2442 | `			ph7_type_name(apArg[0])` |
|         - | 2443 | `			);` |
|         - | 2444 | `	}` |
|         - | 2445 | `	/* Point to the internal representation of the input hashmap */` |
|        23 | 2446 | `	PH7_HashmapCowSeparate(pCtx->pVm, apArg[0]);` |
|        23 | 2447 | `	pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|        23 | 2448 | `	if( pMap->nEntry > 1 ){` |
|        19 | 2449 | `		sxi32 iCmpFlags = 0;` |
|        19 | 2450 | `		if( nArg > 1 ){` |
|         - | 2451 | `			/* Extract comparison flags */` |
|         5 | 2452 | `			iCmpFlags = ph7_value_to_int(apArg[1]);` |
|         5 | 2453 | `			if( iCmpFlags == 3 /* SORT_REGULAR */ ){` |
|       ! 0 | 2454 | `				iCmpFlags = 0; /* Standard comparison */` |
|       ! 0 | 2455 | `			}` |
|         2 | 2456 | `		}` |
|         - | 2457 | `		/* Do the merge sort */` |
|        19 | 2458 | `		HashmapMergeSort(pMap,HashmapCmpCallback1,SX_INT_TO_PTR(iCmpFlags));` |
|         - | 2459 | `		/* Fix the last link broken by the merge */` |
|        45 | 2460 | `		while(pMap->pLast->pPrev){` |
|        27 | 2461 | `			pMap->pLast = pMap->pLast->pPrev;` |
|         1 | 2462 | `		}` |
|         9 | 2463 | `	}` |
|         - | 2464 | `	/* All done,return TRUE */` |
|        23 | 2465 | `	ph7_result_bool(pCtx,1);` |
|        23 | 2466 | `	return PH7_OK;` |
|        21 | 2467 | `}` |
|         - | 2468 | `/*` |
|         - | 2469 | ` * bool arsort(array &$array[,int $sort_flags = SORT_REGULAR ] )` |
|         - | 2470 | ` *  Sort an array in reverse order and maintain index association.` |
|         - | 2471 | ` * Parameters` |
|         - | 2472 | ` *  $array` |
|         - | 2473 | ` *   The input array.` |
|         - | 2474 | ` * $sort_flags` |
|         - | 2475 | ` *  The optional second parameter sort_flags may be used to modify the sorting behavior using these values:` |
|         - | 2476 | ` *  Sorting type flags:` |
|         - | 2477 | ` *   SORT_REGULAR - compare items normally (don't change types)` |
|         - | 2478 | ` *   SORT_NUMERIC - compare items numerically` |
|         - | 2479 | ` *   SORT_STRING - compare items as strings` |
|         - | 2480 | ` * Return` |
|         - | 2481 | ` *  TRUE on success or FALSE on failure.` |
|         - | 2482 | ` */` |
|        32 | 2483 | `static int ph7_hashmap_arsort(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|         5 | 2484 | `{` |
|         - | 2485 | `	ph7_hashmap *pMap;` |
|         - | 2486 | `	/* PHP 8: ArgumentCountError if no arguments */` |
|        37 | 2487 | `	if( nArg < 1 ){` |
|         3 | 2488 | `		return PH7_VmThrowException(pCtx,` |
|         - | 2489 | `			"ArgumentCountError",` |
|         - | 2490 | `			"arsort() expects at least 1 argument, 0 given"` |
|         - | 2491 | `			);` |
|         - | 2492 | `	}` |
|         - | 2493 | `	/* PHP 8: TypeError if first argument is not an array */` |
|        35 | 2494 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|        16 | 2495 | `		return PH7_VmThrowException(pCtx,` |
|         - | 2496 | `			"TypeError",` |
|         - | 2497 | `			"arsort(): Argument #1 ($array) must be of type array, %s given",` |
|         4 | 2498 | `			ph7_type_name(apArg[0])` |
|         - | 2499 | `			);` |
|         - | 2500 | `	}` |
|         - | 2501 | `	/* Point to the internal representation of the input hashmap */` |
|        23 | 2502 | `	PH7_HashmapCowSeparate(pCtx->pVm, apArg[0]);` |
|        23 | 2503 | `	pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|        23 | 2504 | `	if( pMap->nEntry > 1 ){` |
|        19 | 2505 | `		sxi32 iCmpFlags = 0;` |
|        19 | 2506 | `		if( nArg > 1 ){` |
|         - | 2507 | `			/* Extract comparison flags */` |
|         5 | 2508 | `			iCmpFlags = ph7_value_to_int(apArg[1]);` |
|         5 | 2509 | `			if( iCmpFlags == 3 /* SORT_REGULAR */ ){` |
|       ! 0 | 2510 | `				iCmpFlags = 0; /* Standard comparison */` |
|       ! 0 | 2511 | `			}` |
|         2 | 2512 | `		}` |
|         - | 2513 | `		/* Do the merge sort */` |
|        19 | 2514 | `		HashmapMergeSort(pMap,HashmapCmpCallback3,SX_INT_TO_PTR(iCmpFlags));` |
|         - | 2515 | `		/* Fix the last link broken by the merge */` |
|        35 | 2516 | `		while(pMap->pLast->pPrev){` |
|        17 | 2517 | `			pMap->pLast = pMap->pLast->pPrev;` |
|         1 | 2518 | `		}` |
|         9 | 2519 | `	}` |
|         - | 2520 | `	/* All done,return TRUE */` |
|        23 | 2521 | `	ph7_result_bool(pCtx,1);` |
|        23 | 2522 | `	return PH7_OK;` |
|        21 | 2523 | `}` |
|         - | 2524 | `/*` |
|         - | 2525 | ` * bool ksort(array &$array[,int $sort_flags = SORT_REGULAR ] )` |
|         - | 2526 | ` *  Sort an array by key.` |
|         - | 2527 | ` * Parameters` |
|         - | 2528 | ` *  $array` |
|         - | 2529 | ` *   The input array.` |
|         - | 2530 | ` * $sort_flags` |
|         - | 2531 | ` *  The optional second parameter sort_flags may be used to modify the sorting behavior using these values:` |
|         - | 2532 | ` *  Sorting type flags:` |
|         - | 2533 | ` *   SORT_REGULAR - compare items normally (don't change types)` |
|         - | 2534 | ` *   SORT_NUMERIC - compare items numerically` |
|         - | 2535 | ` *   SORT_STRING - compare items as strings` |
|         - | 2536 | ` * Return` |
|         - | 2537 | ` *  TRUE on success or FALSE on failure.` |
|         - | 2538 | ` */` |
|         6 | 2539 | `static int ph7_hashmap_ksort(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|         1 | 2540 | `{` |
|         - | 2541 | `	ph7_hashmap *pMap;` |
|         - | 2542 | `	/* Make sure we are dealing with a valid hashmap */` |
|         7 | 2543 | `	if( nArg < 1 \|\| !ph7_value_is_array(apArg[0]) ){` |
|         - | 2544 | `		/* Missing/Invalid arguments,return FALSE */` |
|       ! 0 | 2545 | `		ph7_result_bool(pCtx,0);` |
|       ! 0 | 2546 | `		return PH7_OK;` |
|         - | 2547 | `	}` |
|         - | 2548 | `	/* Point to the internal representation of the input hashmap */` |
|         7 | 2549 | `	PH7_HashmapCowSeparate(pCtx->pVm, apArg[0]);` |
|         7 | 2550 | `	pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|         7 | 2551 | `	if( pMap->nEntry > 1 ){` |
|         7 | 2552 | `		sxi32 iCmpFlags = 0;` |
|         7 | 2553 | `		if( nArg > 1 ){` |
|         - | 2554 | `			/* Extract comparison flags */` |
|       ! 0 | 2555 | `			iCmpFlags = ph7_value_to_int(apArg[1]);` |
|       ! 0 | 2556 | `			if( iCmpFlags == 3 /* SORT_REGULAR */ ){` |
|       ! 0 | 2557 | `				iCmpFlags = 0; /* Standard comparison */` |
|       ! 0 | 2558 | `			}` |
|       ! 0 | 2559 | `		}` |
|         - | 2560 | `		/* Do the merge sort */` |
|         7 | 2561 | `		HashmapMergeSort(pMap,HashmapCmpCallback2,SX_INT_TO_PTR(iCmpFlags));` |
|         - | 2562 | `		/* Fix the last link broken by the merge */` |
|        17 | 2563 | `		while(pMap->pLast->pPrev){` |
|        11 | 2564 | `			pMap->pLast = pMap->pLast->pPrev;` |
|         1 | 2565 | `		}` |
|         3 | 2566 | `	}` |
|         - | 2567 | `	/* All done,return TRUE */` |
|         7 | 2568 | `	ph7_result_bool(pCtx,1);` |
|         7 | 2569 | `	return PH7_OK;` |
|         4 | 2570 | `}` |
|         - | 2571 | `/*` |
|         - | 2572 | ` * bool krsort(array &$array[,int $sort_flags = SORT_REGULAR ] )` |
|         - | 2573 | ` *  Sort an array by key in reverse order.` |
|         - | 2574 | ` * Parameters` |
|         - | 2575 | ` *  $array` |
|         - | 2576 | ` *   The input array.` |
|         - | 2577 | ` * $sort_flags` |
|         - | 2578 | ` *  The optional second parameter sort_flags may be used to modify the sorting behavior using these values:` |
|         - | 2579 | ` *  Sorting type flags:` |
|         - | 2580 | ` *   SORT_REGULAR - compare items normally (don't change types)` |
|         - | 2581 | ` *   SORT_NUMERIC - compare items numerically` |
|         - | 2582 | ` *   SORT_STRING - compare items as strings` |
|         - | 2583 | ` * Return` |
|         - | 2584 | ` *  TRUE on success or FALSE on failure.` |
|         - | 2585 | ` */` |
|         2 | 2586 | `static int ph7_hashmap_krsort(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|         1 | 2587 | `{` |
|         - | 2588 | `	ph7_hashmap *pMap;` |
|         - | 2589 | `	/* Make sure we are dealing with a valid hashmap */` |
|         3 | 2590 | `	if( nArg < 1 \|\| !ph7_value_is_array(apArg[0]) ){` |
|         - | 2591 | `		/* Missing/Invalid arguments,return FALSE */` |
|       ! 0 | 2592 | `		ph7_result_bool(pCtx,0);` |
|       ! 0 | 2593 | `		return PH7_OK;` |
|         - | 2594 | `	}` |
|         - | 2595 | `	/* Point to the internal representation of the input hashmap */` |
|         3 | 2596 | `	PH7_HashmapCowSeparate(pCtx->pVm, apArg[0]);` |
|         3 | 2597 | `	pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|         3 | 2598 | `	if( pMap->nEntry > 1 ){` |
|         3 | 2599 | `		sxi32 iCmpFlags = 0;` |
|         3 | 2600 | `		if( nArg > 1 ){` |
|         - | 2601 | `			/* Extract comparison flags */` |
|       ! 0 | 2602 | `			iCmpFlags = ph7_value_to_int(apArg[1]);` |
|       ! 0 | 2603 | `			if( iCmpFlags == 3 /* SORT_REGULAR */ ){` |
|       ! 0 | 2604 | `				iCmpFlags = 0; /* Standard comparison */` |
|       ! 0 | 2605 | `			}` |
|       ! 0 | 2606 | `		}` |
|         - | 2607 | `		/* Do the merge sort */` |
|         3 | 2608 | `		HashmapMergeSort(pMap,HashmapCmpCallback5,SX_INT_TO_PTR(iCmpFlags));` |
|         - | 2609 | `		/* Fix the last link broken by the merge */` |
|         7 | 2610 | `		while(pMap->pLast->pPrev){` |
|         5 | 2611 | `			pMap->pLast = pMap->pLast->pPrev;` |
|         1 | 2612 | `		}` |
|         1 | 2613 | `	}` |
|         - | 2614 | `	/* All done,return TRUE */` |
|         3 | 2615 | `	ph7_result_bool(pCtx,1);` |
|         3 | 2616 | `	return PH7_OK;` |
|         2 | 2617 | `}` |
|         - | 2618 | `/*` |
|         - | 2619 | ` * bool rsort(array &$array[,int $sort_flags = SORT_REGULAR ] )` |
|         - | 2620 | ` * Sort an array in reverse order.` |
|         - | 2621 | ` * Parameters` |
|         - | 2622 | ` *  $array` |
|         - | 2623 | ` *   The input array.` |
|         - | 2624 | ` * $sort_flags` |
|         - | 2625 | ` *  The optional second parameter sort_flags may be used to modify the sorting behavior using these values:` |
|         - | 2626 | ` *  Sorting type flags:` |
|         - | 2627 | ` *   SORT_REGULAR - compare items normally (don't change types)` |
|         - | 2628 | ` *   SORT_NUMERIC - compare items numerically` |
|         - | 2629 | ` *   SORT_STRING - compare items as strings` |
|         - | 2630 | ` * Return` |
|         - | 2631 | ` *  TRUE on success or FALSE on failure.` |
|         - | 2632 | ` */` |
|         2 | 2633 | `static int ph7_hashmap_rsort(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|         1 | 2634 | `{` |
|         - | 2635 | `	ph7_hashmap *pMap;` |
|         - | 2636 | `	/* Make sure we are dealing with a valid hashmap */` |
|         3 | 2637 | `	if( nArg < 1 \|\| !ph7_value_is_array(apArg[0]) ){` |
|         - | 2638 | `		/* Missing/Invalid arguments,return FALSE */` |
|       ! 0 | 2639 | `		ph7_result_bool(pCtx,0);` |
|       ! 0 | 2640 | `		return PH7_OK;` |
|         - | 2641 | `	}` |
|         - | 2642 | `	/* Point to the internal representation of the input hashmap */` |
|         3 | 2643 | `	PH7_HashmapCowSeparate(pCtx->pVm, apArg[0]);` |
|         3 | 2644 | `	pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|         3 | 2645 | `	if( pMap->nEntry > 1 ){` |
|         3 | 2646 | `		sxi32 iCmpFlags = 0;` |
|         3 | 2647 | `		if( nArg > 1 ){` |
|         - | 2648 | `			/* Extract comparison flags */` |
|       ! 0 | 2649 | `			iCmpFlags = ph7_value_to_int(apArg[1]);` |
|       ! 0 | 2650 | `			if( iCmpFlags == 3 /* SORT_REGULAR */ ){` |
|       ! 0 | 2651 | `				iCmpFlags = 0; /* Standard comparison */` |
|       ! 0 | 2652 | `			}` |
|       ! 0 | 2653 | `		}` |
|         - | 2654 | `		/* Do the merge sort */` |
|         3 | 2655 | `		HashmapMergeSort(pMap,HashmapCmpCallback3,SX_INT_TO_PTR(iCmpFlags));` |
|         - | 2656 | `		/* Rehash [Do not maintain index association as requested by the PHP specification] */` |
|         3 | 2657 | `		HashmapSortRehash(pMap);` |
|         1 | 2658 | `	}` |
|         - | 2659 | `	/* All done,return TRUE */` |
|         3 | 2660 | `	ph7_result_bool(pCtx,1);` |
|         3 | 2661 | `	return PH7_OK;` |
|         2 | 2662 | `}` |
|         - | 2663 | `/*` |
|         - | 2664 | ` * bool usort(array &$array,callable $cmp_function)` |
|         - | 2665 | ` *  Sort an array by values using a user-defined comparison function.` |
|         - | 2666 | ` * Parameters` |
|         - | 2667 | ` *  $array` |
|         - | 2668 | ` *   The input array.` |
|         - | 2669 | ` * $cmp_function` |
|         - | 2670 | ` *  The comparison function must return an integer less than, equal to, or greater` |
|         - | 2671 | ` *  than zero if the first argument is considered to be respectively less than, equal` |
|         - | 2672 | ` *  to, or greater than the second.` |
|         - | 2673 | ` *    int callback ( mixed $a, mixed $b )` |
|         - | 2674 | ` * Return` |
|         - | 2675 | ` *  TRUE on success or FALSE on failure.` |
|         - | 2676 | ` */` |
|        16 | 2677 | `static int ph7_hashmap_usort(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|         3 | 2678 | `{` |
|         - | 2679 | `	ph7_hashmap *pMap;` |
|         - | 2680 | `	/* Make sure we are dealing with a valid hashmap */` |
|        19 | 2681 | `	if( nArg < 1 \|\| !ph7_value_is_array(apArg[0]) ){` |
|         - | 2682 | `		/* Missing/Invalid arguments,return FALSE */` |
|       ! 0 | 2683 | `		ph7_result_bool(pCtx,0);` |
|       ! 0 | 2684 | `		return PH7_OK;` |
|         - | 2685 | `	}` |
|         - | 2686 | `	/* Point to the internal representation of the input hashmap */` |
|        19 | 2687 | `	PH7_HashmapCowSeparate(pCtx->pVm, apArg[0]);` |
|        19 | 2688 | `	pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|        19 | 2689 | `	if( pMap->nEntry > 1 ){` |
|        19 | 2690 | `		ph7_value *pCallback = 0;` |
|         - | 2691 | `		ProcNodeCmp xCmp;` |
|        19 | 2692 | `		xCmp = HashmapCmpCallback4; /* User-defined function as the comparison callback */` |
|        19 | 2693 | `		if( nArg > 1 && ph7_value_is_callable(apArg[1]) ){` |
|         - | 2694 | `			/* Point to the desired callback */` |
|        19 | 2695 | `			pCallback = apArg[1];` |
|        11 | 2696 | `		}else{` |
|         - | 2697 | `			/* Use the default comparison function */` |
|       ! 0 | 2698 | `			xCmp = HashmapCmpCallback1;` |
|         - | 2699 | `		}` |
|         - | 2700 | `		/* Do the merge sort */` |
|        19 | 2701 | `		pCtx->pVm->iCmpCallbackExc = 0;` |
|        19 | 2702 | `		HashmapMergeSort(pMap,xCmp,pCallback);` |
|         - | 2703 | `		/* Rehash [Do not maintain index association as requested by the PHP specification] */` |
|        19 | 2704 | `		HashmapSortRehash(pMap);` |
|        19 | 2705 | `		if( pCtx->pVm->iCmpCallbackExc ){` |
|         - | 2706 | `			/* The comparison callback raised: propagate so the dispatcher unwinds. */` |
|        10 | 2707 | `			pCtx->pVm->iCmpCallbackExc = 0;` |
|        10 | 2708 | `			return PH7_EXCEPTION;` |
|         - | 2709 | `		}` |
|         4 | 2710 | `	}` |
|         - | 2711 | `	/* All done,return TRUE */` |
|        10 | 2712 | `	ph7_result_bool(pCtx,1);` |
|        10 | 2713 | `	return PH7_OK;` |
|        11 | 2714 | `}` |
|         - | 2715 | `/*` |
|         - | 2716 | ` * bool uasort(array &$array,callable $cmp_function)` |
|         - | 2717 | ` *  Sort an array by values using a user-defined comparison function` |
|         - | 2718 | ` *  and maintain index association.` |
|         - | 2719 | ` * Parameters` |
|         - | 2720 | ` *  $array` |
|         - | 2721 | ` *   The input array.` |
|         - | 2722 | ` * $cmp_function` |
|         - | 2723 | ` *  The comparison function must return an integer less than, equal to, or greater` |
|         - | 2724 | ` *  than zero if the first argument is considered to be respectively less than, equal` |
|         - | 2725 | ` *  to, or greater than the second.` |
|         - | 2726 | ` *    int callback ( mixed $a, mixed $b )` |
|         - | 2727 | ` * Return` |
|         - | 2728 | ` *  TRUE on success or FALSE on failure.` |
|         - | 2729 | ` */` |
|         2 | 2730 | `static int ph7_hashmap_uasort(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|         1 | 2731 | `{` |
|         - | 2732 | `	ph7_hashmap *pMap;` |
|         - | 2733 | `	/* Make sure we are dealing with a valid hashmap */` |
|         3 | 2734 | `	if( nArg < 1 \|\| !ph7_value_is_array(apArg[0]) ){` |
|         - | 2735 | `		/* Missing/Invalid arguments,return FALSE */` |
|       ! 0 | 2736 | `		ph7_result_bool(pCtx,0);` |
|       ! 0 | 2737 | `		return PH7_OK;` |
|         - | 2738 | `	}` |
|         - | 2739 | `	/* Point to the internal representation of the input hashmap */` |
|         3 | 2740 | `	PH7_HashmapCowSeparate(pCtx->pVm, apArg[0]);` |
|         3 | 2741 | `	pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|         3 | 2742 | `	if( pMap->nEntry > 1 ){` |
|         3 | 2743 | `		ph7_value *pCallback = 0;` |
|         - | 2744 | `		ProcNodeCmp xCmp;` |
|         3 | 2745 | `		xCmp = HashmapCmpCallback4; /* User-defined function as the comparison callback */` |
|         3 | 2746 | `		if( nArg > 1 && ph7_value_is_callable(apArg[1]) ){` |
|         - | 2747 | `			/* Point to the desired callback */` |
|         3 | 2748 | `			pCallback = apArg[1];` |
|         2 | 2749 | `		}else{` |
|         - | 2750 | `			/* Use the default comparison function */` |
|       ! 0 | 2751 | `			xCmp = HashmapCmpCallback1;` |
|         - | 2752 | `		}` |
|         - | 2753 | `		/* Do the merge sort */` |
|         3 | 2754 | `		pCtx->pVm->iCmpCallbackExc = 0;` |
|         3 | 2755 | `		HashmapMergeSort(pMap,xCmp,pCallback);` |
|         - | 2756 | `		/* Fix the last link broken by the merge */` |
|         5 | 2757 | `		while(pMap->pLast->pPrev){` |
|         3 | 2758 | `			pMap->pLast = pMap->pLast->pPrev;` |
|         1 | 2759 | `		}` |
|         3 | 2760 | `		if( pCtx->pVm->iCmpCallbackExc ){` |
|         - | 2761 | `			/* The comparison callback raised: propagate so the dispatcher unwinds. */` |
|       ! 0 | 2762 | `			pCtx->pVm->iCmpCallbackExc = 0;` |
|       ! 0 | 2763 | `			return PH7_EXCEPTION;` |
|         - | 2764 | `		}` |
|         1 | 2765 | `	}` |
|         - | 2766 | `	/* All done,return TRUE */` |
|         3 | 2767 | `	ph7_result_bool(pCtx,1);` |
|         3 | 2768 | `	return PH7_OK;` |
|         2 | 2769 | `}` |
|         - | 2770 | `/*` |
|         - | 2771 | ` * bool uksort(array &$array,callable $cmp_function)` |
|         - | 2772 | ` *  Sort an array by keys using a user-defined comparison` |
|         - | 2773 | ` *  function and maintain index association.` |
|         - | 2774 | ` * Parameters` |
|         - | 2775 | ` *  $array` |
|         - | 2776 | ` *   The input array.` |
|         - | 2777 | ` * $cmp_function` |
|         - | 2778 | ` *  The comparison function must return an integer less than, equal to, or greater` |
|         - | 2779 | ` *  than zero if the first argument is considered to be respectively less than, equal` |
|         - | 2780 | ` *  to, or greater than the second.` |
|         - | 2781 | ` *    int callback ( mixed $a, mixed $b )` |
|         - | 2782 | ` * Return` |
|         - | 2783 | ` *  TRUE on success or FALSE on failure.` |
|         - | 2784 | ` */` |
|         2 | 2785 | `static int ph7_hashmap_uksort(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|         1 | 2786 | `{` |
|         - | 2787 | `	ph7_hashmap *pMap;` |
|         - | 2788 | `	/* Make sure we are dealing with a valid hashmap */` |
|         3 | 2789 | `	if( nArg < 1 \|\| !ph7_value_is_array(apArg[0]) ){` |
|         - | 2790 | `		/* Missing/Invalid arguments,return FALSE */` |
|       ! 0 | 2791 | `		ph7_result_bool(pCtx,0);` |
|       ! 0 | 2792 | `		return PH7_OK;` |
|         - | 2793 | `	}` |
|         - | 2794 | `	/* Point to the internal representation of the input hashmap */` |
|         3 | 2795 | `	PH7_HashmapCowSeparate(pCtx->pVm, apArg[0]);` |
|         3 | 2796 | `	pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|         3 | 2797 | `	if( pMap->nEntry > 1 ){` |
|         3 | 2798 | `		ph7_value *pCallback = 0;` |
|         - | 2799 | `		ProcNodeCmp xCmp;` |
|         3 | 2800 | `		xCmp = HashmapCmpCallback6; /* User-defined function as the comparison callback */` |
|         3 | 2801 | `		if( nArg > 1 && ph7_value_is_callable(apArg[1]) ){` |
|         - | 2802 | `			/* Point to the desired callback */` |
|         3 | 2803 | `			pCallback = apArg[1];` |
|         2 | 2804 | `		}else{` |
|         - | 2805 | `			/* Use the default comparison function */` |
|       ! 0 | 2806 | `			xCmp = HashmapCmpCallback2;` |
|         - | 2807 | `		}` |
|         - | 2808 | `		/* Do the merge sort */` |
|         3 | 2809 | `		pCtx->pVm->iCmpCallbackExc = 0;` |
|         3 | 2810 | `		HashmapMergeSort(pMap,xCmp,pCallback);` |
|         - | 2811 | `		/* Fix the last link broken by the merge */` |
|         3 | 2812 | `		while(pMap->pLast->pPrev){` |
|       ! 0 | 2813 | `			pMap->pLast = pMap->pLast->pPrev;` |
|       ! 0 | 2814 | `		}` |
|         3 | 2815 | `		if( pCtx->pVm->iCmpCallbackExc ){` |
|         - | 2816 | `			/* The comparison callback raised: propagate so the dispatcher unwinds. */` |
|       ! 0 | 2817 | `			pCtx->pVm->iCmpCallbackExc = 0;` |
|       ! 0 | 2818 | `			return PH7_EXCEPTION;` |
|         - | 2819 | `		}` |
|         1 | 2820 | `	}` |
|         - | 2821 | `	/* All done,return TRUE */` |
|         3 | 2822 | `	ph7_result_bool(pCtx,1);` |
|         3 | 2823 | `	return PH7_OK;` |
|         2 | 2824 | `}` |
|         - | 2825 | `/*` |
|         - | 2826 | ` * bool shuffle(array &$array)` |
|         - | 2827 | ` *  shuffles (randomizes the order of the elements in) an array.` |
|         - | 2828 | ` * Parameters` |
|         - | 2829 | ` *  $array` |
|         - | 2830 | ` *   The input array.` |
|         - | 2831 | ` * Return` |
|         - | 2832 | ` *  TRUE on success or FALSE on failure.` |
|         - | 2833 | ` *` |
|         - | 2834 | ` */` |
|         2 | 2835 | `static int ph7_hashmap_shuffle(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|         1 | 2836 | `{` |
|         - | 2837 | `	ph7_hashmap *pMap;` |
|         - | 2838 | `	/* Make sure we are dealing with a valid hashmap */` |
|         3 | 2839 | `	if( nArg < 1 \|\| !ph7_value_is_array(apArg[0]) ){` |
|         - | 2840 | `		/* Missing/Invalid arguments,return FALSE */` |
|       ! 0 | 2841 | `		ph7_result_bool(pCtx,0);` |
|       ! 0 | 2842 | `		return PH7_OK;` |
|         - | 2843 | `	}` |
|         - | 2844 | `	/* Point to the internal representation of the input hashmap */` |
|         3 | 2845 | `	PH7_HashmapCowSeparate(pCtx->pVm, apArg[0]);` |
|         3 | 2846 | `	pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|         3 | 2847 | `	if( pMap->nEntry > 1 ){` |
|         - | 2848 | `		/* Do the merge sort */` |
|         3 | 2849 | `		HashmapMergeSort(pMap,HashmapCmpCallback7,0);` |
|         - | 2850 | `		/* Fix the last link broken by the merge */` |
|        10 | 2851 | `		while(pMap->pLast->pPrev){` |
|         8 | 2852 | `			pMap->pLast = pMap->pLast->pPrev;` |
|         1 | 2853 | `		}` |
|         1 | 2854 | `	}` |
|         - | 2855 | `	/* All done,return TRUE */` |
|         3 | 2856 | `	ph7_result_bool(pCtx,1);` |
|         3 | 2857 | `	return PH7_OK;` |
|         2 | 2858 | `}` |
|         - | 2859 | `/*` |
|         - | 2860 | ` * int count(array $var [, int $mode = COUNT_NORMAL ])` |
|         - | 2861 | ` *   Count all elements in an array, or something in an object.` |
|         - | 2862 | ` * Parameters` |
|         - | 2863 | ` *  $var` |
|         - | 2864 | ` *   The array or the object.` |
|         - | 2865 | ` * $mode` |
|         - | 2866 | ` *  If the optional mode parameter is set to COUNT_RECURSIVE (or 1), count()` |
|         - | 2867 | ` *  will recursively count the array. This is particularly useful for counting` |
|         - | 2868 | ` *  all the elements of a multidimensional array.` |
|         - | 2869 | ` * Return` |
|         - | 2870 | ` *  Returns the number of elements in the array.` |
|         - | 2871 | ` */` |
|      1150 | 2872 | `static int ph7_hashmap_count(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|         5 | 2873 | `{` |
|      1155 | 2874 | `	int bRecursive = FALSE;` |
|      1155 | 2875 | `	int bCycleDetected = FALSE;` |
|         - | 2876 | `	sxi64 iCount;` |
|      1155 | 2877 | `	if( nArg < 1 ){` |
|         3 | 2878 | `		return PH7_VmThrowException(pCtx,` |
|         - | 2879 | `			"ArgumentCountError",` |
|         - | 2880 | `			"count() expects at least 1 argument, 0 given"` |
|         - | 2881 | `			);` |
|         - | 2882 | `	}` |
|      1153 | 2883 | `	if( nArg > 2 ){` |
|         4 | 2884 | `		return PH7_VmThrowException(pCtx,` |
|         - | 2885 | `			"ArgumentCountError",` |
|         - | 2886 | `			"count() expects at most 2 arguments, %d given",` |
|         1 | 2887 | `			nArg` |
|         - | 2888 | `			);` |
|         - | 2889 | `	}` |
|         - | 2890 | `	/* PHP validates $mode right after parsing, before the type dispatch, so` |
|         - | 2891 | `	 * an invalid mode raises ValueError whether $value is an array or a` |
|         - | 2892 | `	 * Countable object (the mode is then ignored for the Countable path). */` |
|      1151 | 2893 | `	if( nArg > 1 ){` |
|        45 | 2894 | `		sxi32 iMode = ph7_value_to_int(apArg[1]);` |
|        45 | 2895 | `		if( iMode != 0 /* COUNT_NORMAL */ && iMode != 1 /* COUNT_RECURSIVE */ ){` |
|        12 | 2896 | `			return PH7_VmThrowException(pCtx,` |
|         - | 2897 | `				"ValueError",` |
|         - | 2898 | `				"count(): Argument #2 ($mode) must be either COUNT_NORMAL or COUNT_RECURSIVE"` |
|         - | 2899 | `				);` |
|         - | 2900 | `		}` |
|        34 | 2901 | `		bRecursive = iMode == 1;` |
|        16 | 2902 | `	}` |
|      1143 | 2903 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|         - | 2904 | `		/* Countable object: dispatch to ->count() */` |
|        37 | 2905 | `		if( apArg[0]->iFlags & MEMOBJ_OBJ ){` |
|        26 | 2906 | `			ph7_class_instance *pInst = (ph7_class_instance *)apArg[0]->x.pOther;` |
|        26 | 2907 | `			ph7_class *pCountable = pCtx->pVm->pCountableClass;` |
|        26 | 2908 | `			if( pCountable && PH7_VmInstanceOf(pInst->pClass,pCountable) ){` |
|        23 | 2909 | `				ph7_class_method *pMeth = PH7_ClassExtractMethod(pInst->pClass,` |
|         - | 2910 | `					"count",sizeof("count")-1);` |
|        23 | 2911 | `				if( pMeth ){` |
|         - | 2912 | `					ph7_value sResult;` |
|        23 | 2913 | `					PH7_MemObjInit(pCtx->pVm,&sResult);` |
|        23 | 2914 | `					PH7_VmCallClassMethod(pCtx->pVm,pInst,pMeth,&sResult,0,0);` |
|        23 | 2915 | `					ph7_result_int64(pCtx,ph7_value_to_int64(&sResult));` |
|        23 | 2916 | `					PH7_MemObjRelease(&sResult);` |
|        23 | 2917 | `					return PH7_OK;` |
|         - | 2918 | `				}` |
|       ! 0 | 2919 | `			}` |
|         1 | 2920 | `		}` |
|        22 | 2921 | `		return PH7_VmThrowException(pCtx,` |
|         - | 2922 | `			"TypeError",` |
|         - | 2923 | `			"count(): Argument #1 ($value) must be of type Countable\|array, %s given",` |
|         6 | 2924 | `			ph7_type_name(apArg[0])` |
|         - | 2925 | `			);` |
|         - | 2926 | `	}` |
|         - | 2927 | `	/* Count */` |
|      1111 | 2928 | `	iCount = HashmapCount((ph7_hashmap *)apArg[0]->x.pOther,bRecursive,&bCycleDetected);` |
|      1111 | 2929 | `	if( bCycleDetected ){` |
|         3 | 2930 | `		PH7_VmThrowError(pCtx->pVm,0,PH7_CTX_WARNING,"count(): Recursion detected");` |
|         1 | 2931 | `	}` |
|      1111 | 2932 | `	ph7_result_int64(pCtx,iCount);` |
|      1111 | 2933 | `	return PH7_OK;` |
|       580 | 2934 | `}` |
|         - | 2935 | `/*` |
|         - | 2936 | ` * bool array_key_exists(value $key,array $search)` |
|         - | 2937 | ` *  Checks if the given key or index exists in the array.` |
|         - | 2938 | ` * Parameters` |
|         - | 2939 | ` * $key` |
|         - | 2940 | ` *   Value to check.` |
|         - | 2941 | ` * $search` |
|         - | 2942 | ` *  An array with keys to check.` |
|         - | 2943 | ` * Return` |
|         - | 2944 | ` *  TRUE on success or FALSE on failure.` |
|         - | 2945 | ` */` |
|        86 | 2946 | `static int ph7_hashmap_key_exists(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|         5 | 2947 | `{` |
|         - | 2948 | `	sxi32 rc;` |
|        91 | 2949 | `	if( nArg != 2 ){` |
|         - | 2950 | `		/* PHP requires exactly two arguments */` |
|        12 | 2951 | `		return PH7_VmThrowException(pCtx,` |
|         - | 2952 | `			"ArgumentCountError",` |
|         - | 2953 | `			"array_key_exists() expects exactly 2 arguments, %d given",` |
|         3 | 2954 | `			nArg` |
|         - | 2955 | `			);` |
|         - | 2956 | `	}` |
|         - | 2957 | `	/* Make sure we are dealing with a valid hashmap */` |
|        85 | 2958 | `	if( !ph7_value_is_array(apArg[1]) ){` |
|         - | 2959 | `		/* Type mismatch -> TypeError */` |
|         8 | 2960 | `		return PH7_VmThrowException(pCtx,` |
|         - | 2961 | `			"TypeError",` |
|         - | 2962 | `			"array_key_exists(): Argument #2 ($array) must be of type array, %s given",` |
|         4 | 2963 | `			ph7_type_name(apArg[1])` |
|         - | 2964 | `			);` |
|         - | 2965 | `	}` |
|         - | 2966 | `	/* Emit deprecation warnings matching PHP behaviour */` |
|        80 | 2967 | `	if( apArg[0]->iFlags & MEMOBJ_NULL ){` |
|         3 | 2968 | `		ph7_context_throw_error_format(pCtx,8192,` |
|         - | 2969 | `			"Using null as the key parameter for array_key_exists() is deprecated, "` |
|         - | 2970 | `			"use an empty string instead"` |
|         - | 2971 | `			);` |
|        79 | 2972 | `	}else if( apArg[0]->iFlags & MEMOBJ_REAL ){` |
|         3 | 2973 | `		ph7_real rVal = apArg[0]->rVal;` |
|         3 | 2974 | `		if( rVal != (ph7_real)(sxi64)rVal ){` |
|         4 | 2975 | `			ph7_context_throw_error_format(pCtx,8192,` |
|         - | 2976 | `				"Implicit conversion from float %g to int loses precision"` |
|         1 | 2977 | `				,rVal` |
|         - | 2978 | `				);` |
|         1 | 2979 | `		}` |
|         1 | 2980 | `	}` |
|         - | 2981 | `	/* Perform the lookup */` |
|        80 | 2982 | `	rc = PH7_HashmapLookup((ph7_hashmap *)apArg[1]->x.pOther,apArg[0],0);` |
|         - | 2983 | `	/* lookup result */` |
|        80 | 2984 | `	ph7_result_bool(pCtx,rc == SXRET_OK ? 1 : 0);` |
|        80 | 2985 | `	return PH7_OK;` |
|        48 | 2986 | `}` |
|         - | 2987 | `/*` |
|         - | 2988 | ` * value array_pop(array $array)` |
|         - | 2989 | ` *   POP the last inserted element from the array.` |
|         - | 2990 | ` * Parameter` |
|         - | 2991 | ` *  The array to get the value from.` |
|         - | 2992 | ` * Return` |
|         - | 2993 | ` *  Poped value or NULL on failure.` |
|         - | 2994 | ` */` |
|        18 | 2995 | `static int ph7_hashmap_pop(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|         5 | 2996 | `{` |
|         - | 2997 | `	ph7_hashmap *pMap;` |
|         - | 2998 | `	/* PHP requires exactly one argument and it must be passed by reference */` |
|        23 | 2999 | `	if( nArg != 1 ){` |
|         8 | 3000 | `		return PH7_VmThrowException(pCtx,` |
|         - | 3001 | `			"ArgumentCountError",` |
|         - | 3002 | `			"array_pop() expects exactly 1 argument, %d given",` |
|         2 | 3003 | `			nArg` |
|         - | 3004 | `			);` |
|         - | 3005 | `	}` |
|         - | 3006 | `	/* Passing a constant (including literals) or non-variable triggers the same` |
|         - | 3007 | `	 * error message as official PHP. Check the index to detect constants. */` |
|        18 | 3008 | `	if( apArg[0]->nIdx == SXU32_HIGH ){` |
|         6 | 3009 | `		return PH7_VmThrowException(pCtx,` |
|         - | 3010 | `			"Error",` |
|         - | 3011 | `			"array_pop(): Argument #1 ($array) could not be passed by reference"` |
|         - | 3012 | `			);` |
|         - | 3013 | `	}` |
|         - | 3014 | `	/* Make sure we are dealing with a valid hashmap */` |
|        12 | 3015 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|         4 | 3016 | `		return PH7_VmThrowException(pCtx,` |
|         - | 3017 | `			"TypeError",` |
|         - | 3018 | `			"array_pop(): Argument #1 ($array) must be of type array, %s given",` |
|         1 | 3019 | `			ph7_type_name(apArg[0])` |
|         - | 3020 | `			);` |
|         - | 3021 | `	}` |
|         9 | 3022 | `	PH7_HashmapCowSeparate(pCtx->pVm, apArg[0]);` |
|         9 | 3023 | `	pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|         9 | 3024 | `	if( pMap->nEntry < 1 ){` |
|         - | 3025 | `		/* Nothing to pop,return NULL */` |
|         3 | 3026 | `		ph7_result_null(pCtx);` |
|         2 | 3027 | `	}else{` |
|         7 | 3028 | `		ph7_hashmap_node *pLast = pMap->pLast;` |
|         - | 3029 | `		ph7_value *pObj;` |
|         7 | 3030 | `		pObj = HashmapExtractNodeValue(pLast);` |
|         7 | 3031 | `		if( pObj ){` |
|         - | 3032 | `			/* Node value */` |
|         7 | 3033 | `			ph7_result_value(pCtx,pObj);` |
|         - | 3034 | `			/* Unlink the node */` |
|         7 | 3035 | `			PH7_HashmapUnlinkNode(pLast,TRUE);` |
|         4 | 3036 | `		}else{` |
|       ! 0 | 3037 | `			ph7_result_null(pCtx);` |
|         - | 3038 | `		}` |
|         - | 3039 | `		/* Reset the cursor */` |
|         7 | 3040 | `		pMap->pCur = pMap->pFirst;` |
|         - | 3041 | `	}` |
|         9 | 3042 | `	return PH7_OK;` |
|        14 | 3043 | `}` |
|         - | 3044 | `/*` |
|         - | 3045 | ` * int array_push($array,$var,...)` |
|         - | 3046 | ` *   Push one or more elements onto the end of array. (Stack insertion)` |
|         - | 3047 | ` * Parameters` |
|         - | 3048 | ` *  array` |
|         - | 3049 | ` *    The input array.` |
|         - | 3050 | ` *  var` |
|         - | 3051 | ` *   On or more value to push.` |
|         - | 3052 | ` * Return` |
|         - | 3053 | ` *  New array count (including old items).` |
|         - | 3054 | ` */` |
|        24 | 3055 | `static int ph7_hashmap_push(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|         5 | 3056 | `{` |
|         - | 3057 | `	ph7_hashmap *pMap;` |
|         - | 3058 | `	sxi32 rc;` |
|         - | 3059 | `	int i;` |
|        29 | 3060 | `	if( nArg < 1 ){` |
|         4 | 3061 | `		return PH7_VmThrowException(pCtx,` |
|         - | 3062 | `			"ArgumentCountError",` |
|         - | 3063 | `			"array_push() expects at least 1 argument, %d given",` |
|         1 | 3064 | `			nArg` |
|         - | 3065 | `			);` |
|         - | 3066 | `	}` |
|         - | 3067 | `	/* Passing a constant (including literals) or non-variable triggers the same` |
|         - | 3068 | `	 * error message as official PHP. Check the index to detect constants. */` |
|        26 | 3069 | `	if( apArg[0]->nIdx == SXU32_HIGH ){` |
|         6 | 3070 | `		return PH7_VmThrowException(pCtx,` |
|         - | 3071 | `			"Error",` |
|         - | 3072 | `			"array_push(): Argument #1 ($array) could not be passed by reference"` |
|         - | 3073 | `			);` |
|         - | 3074 | `	}` |
|         - | 3075 | `	/* Make sure we are dealing with a valid hashmap */` |
|        20 | 3076 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|         4 | 3077 | `		return PH7_VmThrowException(pCtx,` |
|         - | 3078 | `			"TypeError",` |
|         - | 3079 | `			"array_push(): Argument #1 ($array) must be of type array, %s given",` |
|         1 | 3080 | `			ph7_type_name(apArg[0])` |
|         - | 3081 | `			);` |
|         - | 3082 | `	}` |
|         - | 3083 | `	/* Point to the internal representation of the input hashmap */` |
|        18 | 3084 | `	PH7_HashmapCowSeparate(pCtx->pVm, apArg[0]);` |
|        18 | 3085 | `	pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|         - | 3086 | `	/* Start pushing given values */` |
|        34 | 3087 | `	for( i = 1 ; i < nArg ; ++i ){` |
|        20 | 3088 | `		rc = PH7_HashmapInsert(pMap,0,apArg[i]);` |
|        20 | 3089 | `		if( rc != SXRET_OK ){` |
|         3 | 3090 | `			if( rc == PH7_EXCEPTION \|\| rc == PH7_ABORT ){` |
|         - | 3091 | `				/* Saturated-append Error (php: array_push throws, no result) */` |
|         3 | 3092 | `				return rc;` |
|         - | 3093 | `			}` |
|       ! 0 | 3094 | `			break;` |
|         - | 3095 | `		}` |
|         9 | 3096 | `	}` |
|         - | 3097 | `	/* Return the new count */` |
|        15 | 3098 | `	ph7_result_int64(pCtx,(sxi64)pMap->nEntry);` |
|        15 | 3099 | `	return PH7_OK;` |
|        17 | 3100 | `}` |
|         - | 3101 | `/*` |
|         - | 3102 | ` * value array_shift(array $array)` |
|         - | 3103 | ` *   Shift an element off the beginning of array.` |
|         - | 3104 | ` * Parameter` |
|         - | 3105 | ` *  The array to get the value from.` |
|         - | 3106 | ` * Return` |
|         - | 3107 | ` *  Shifted value or NULL on failure.` |
|         - | 3108 | ` */` |
|        38 | 3109 | `static int ph7_hashmap_shift(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|         5 | 3110 | `{` |
|         - | 3111 | `	ph7_hashmap *pMap;` |
|         - | 3112 | `	/* PHP requires exactly one argument and it must be passed by reference */` |
|        43 | 3113 | `	if( nArg != 1 ){` |
|         8 | 3114 | `		return PH7_VmThrowException(pCtx,` |
|         - | 3115 | `			"ArgumentCountError",` |
|         - | 3116 | `			"array_shift() expects exactly 1 argument, %d given",` |
|         2 | 3117 | `			nArg` |
|         - | 3118 | `			);` |
|         - | 3119 | `	}` |
|         - | 3120 | `	/* Detect constants or literals, which cannot be passed by reference. */` |
|        39 | 3121 | `	if( apArg[0]->nIdx == SXU32_HIGH ){` |
|         6 | 3122 | `		return PH7_VmThrowException(pCtx,` |
|         - | 3123 | `			"Error",` |
|         - | 3124 | `			"array_shift(): Argument #1 ($array) could not be passed by reference"` |
|         - | 3125 | `			);` |
|         - | 3126 | `	}` |
|         - | 3127 | `	/* Make sure we are dealing with a valid hashmap */` |
|        35 | 3128 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|         4 | 3129 | `		return PH7_VmThrowException(pCtx,` |
|         - | 3130 | `			"TypeError",` |
|         - | 3131 | `			"array_shift(): Argument #1 ($array) must be of type array, %s given",` |
|         1 | 3132 | `			ph7_type_name(apArg[0])` |
|         - | 3133 | `			);` |
|         - | 3134 | `	}` |
|         - | 3135 | `	/* Point to the internal representation of the hashmap */` |
|        33 | 3136 | `	PH7_HashmapCowSeparate(pCtx->pVm, apArg[0]);` |
|        33 | 3137 | `	pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|        33 | 3138 | `	if( pMap->nEntry < 1 ){` |
|         - | 3139 | `		/* Empty hashmap,return NULL */` |
|         3 | 3140 | `		ph7_result_null(pCtx);` |
|         2 | 3141 | `	}else{` |
|        31 | 3142 | `		ph7_hashmap_node *pEntry = pMap->pFirst;` |
|         - | 3143 | `		ph7_value *pObj;` |
|         - | 3144 | `		sxu32 n;` |
|        31 | 3145 | `		pObj = HashmapExtractNodeValue(pEntry);` |
|        31 | 3146 | `		if( pObj ){` |
|         - | 3147 | `			/* Node value */` |
|        31 | 3148 | `			ph7_result_value(pCtx,pObj);` |
|         - | 3149 | `			/* Unlink the first node */` |
|        31 | 3150 | `			PH7_HashmapUnlinkNode(pEntry,TRUE);` |
|        18 | 3151 | `		}else{` |
|       ! 0 | 3152 | `			ph7_result_null(pCtx);` |
|         - | 3153 | `		}` |
|         - | 3154 | `		/* Rehash all int keys */` |
|        31 | 3155 | `		n = pMap->nEntry;` |
|        31 | 3156 | `		pEntry = pMap->pFirst;` |
|        31 | 3157 | `		pMap->iNextIdx = 0; /* Reset the automatic index */` |
|        40 | 3158 | `		for(;;){` |
|        85 | 3159 | `			if( n < 1 ){` |
|        31 | 3160 | `				break;` |
|         - | 3161 | `			}` |
|        59 | 3162 | `			if( pEntry->iType == HASHMAP_INT_NODE ){` |
|        59 | 3163 | `				HashmapRehashIntNode(pEntry);` |
|        27 | 3164 | `			}` |
|         - | 3165 | `			/* Point to the next entry */` |
|        59 | 3166 | `			pEntry = pEntry->pPrev; /* Reverse link */` |
|        59 | 3167 | `			n--;` |
|         5 | 3168 | `		}` |
|         - | 3169 | `		/* Reset the cursor */` |
|        31 | 3170 | `		pMap->pCur = pMap->pFirst;` |
|         - | 3171 | `	}` |
|        33 | 3172 | `	return PH7_OK;` |
|        24 | 3173 | `}` |
|         - | 3174 | `/*` |
|         - | 3175 | ` * Extract the node cursor value.` |
|         - | 3176 | ` */` |
|        32 | 3177 | `static sxi32 HashmapCurrentValue(ph7_context *pCtx,ph7_hashmap *pMap,int iDirection)` |
|         1 | 3178 | `{` |
|        33 | 3179 | `	ph7_hashmap_node *pCur = pMap->pCur;` |
|         - | 3180 | `	ph7_value *pVal;` |
|        33 | 3181 | `	if( pCur == 0 ){` |
|         - | 3182 | `		/* Cursor does not point to anything,return FALSE */` |
|       ! 0 | 3183 | `		ph7_result_bool(pCtx,0);` |
|       ! 0 | 3184 | `		return PH7_OK;` |
|         - | 3185 | `	}` |
|        33 | 3186 | `	if( iDirection != 0 ){` |
|        13 | 3187 | `		if( iDirection > 0 ){` |
|         - | 3188 | `			/* Point to the next entry */` |
|        11 | 3189 | `			pMap->pCur = pCur->pPrev; /* Reverse link */` |
|        11 | 3190 | `			pCur = pMap->pCur;` |
|         6 | 3191 | `		}else{` |
|         - | 3192 | `			/* Point to the previous entry */` |
|         3 | 3193 | `			pMap->pCur = pCur->pNext; /* Reverse link */` |
|         3 | 3194 | `			pCur = pMap->pCur;` |
|         - | 3195 | `		}` |
|        13 | 3196 | `		if( pCur == 0 ){` |
|         - | 3197 | `			/* End of input reached,return FALSE */` |
|       ! 0 | 3198 | `			ph7_result_bool(pCtx,0);` |
|       ! 0 | 3199 | `			return PH7_OK;` |
|         - | 3200 | `		}` |
|         6 | 3201 | `	}` |
|         - | 3202 | `	/* Point to the desired element */` |
|        33 | 3203 | `	pVal = HashmapExtractNodeValue(pCur);` |
|        33 | 3204 | `	if( pVal ){` |
|        33 | 3205 | `		ph7_result_value(pCtx,pVal);` |
|        17 | 3206 | `	}else{` |
|       ! 0 | 3207 | `		ph7_result_bool(pCtx,0);` |
|         - | 3208 | `	}` |
|        33 | 3209 | `	return PH7_OK;` |
|        17 | 3210 | `}` |
|         - | 3211 | `/*` |
|         - | 3212 | ` * value current(array $array)` |
|         - | 3213 | ` *  Return the current element in an array.` |
|         - | 3214 | ` * Parameter` |
|         - | 3215 | ` *  $input: The input array.` |
|         - | 3216 | ` * Return` |
|         - | 3217 | ` *  The current() function simply returns the value of the array element that's currently` |
|         - | 3218 | ` *  being pointed to by the internal pointer. It does not move the pointer in any way.` |
|         - | 3219 | ` *  If the internal pointer points beyond the end of the elements list or the array` |
|         - | 3220 | ` *  is empty, current() returns FALSE.` |
|         - | 3221 | ` */` |
|        14 | 3222 | `static int ph7_hashmap_current(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|         1 | 3223 | `{` |
|        15 | 3224 | `	if( nArg < 1 ){` |
|         - | 3225 | `		/* Missing arguments,return FALSE */` |
|       ! 0 | 3226 | `		ph7_result_bool(pCtx,0);` |
|       ! 0 | 3227 | `		return PH7_OK;` |
|         - | 3228 | `	}` |
|         - | 3229 | `	/* Make sure we are dealing with a valid hashmap */` |
|        15 | 3230 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|         - | 3231 | `		/* Invalid argument,return FALSE */` |
|       ! 0 | 3232 | `		ph7_result_bool(pCtx,0);` |
|       ! 0 | 3233 | `		return PH7_OK;` |
|         - | 3234 | `	}` |
|        15 | 3235 | `	HashmapCurrentValue(&(*pCtx),(ph7_hashmap *)apArg[0]->x.pOther,0);` |
|        15 | 3236 | `	return PH7_OK;` |
|         8 | 3237 | `}` |
|         - | 3238 | `/*` |
|         - | 3239 | ` * value next(array $input)` |
|         - | 3240 | ` *  Advance the internal array pointer of an array.` |
|         - | 3241 | ` * Parameter` |
|         - | 3242 | ` *  $input: The input array.` |
|         - | 3243 | ` * Return` |
|         - | 3244 | ` *  next() behaves like current(), with one difference. It advances the internal array` |
|         - | 3245 | ` *  pointer one place forward before returning the element value. That means it returns` |
|         - | 3246 | ` *  the next array value and advances the internal array pointer by one.` |
|         - | 3247 | ` */` |
|        10 | 3248 | `static int ph7_hashmap_next(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|         1 | 3249 | `{` |
|        11 | 3250 | `	if( nArg < 1 ){` |
|         - | 3251 | `		/* Missing arguments,return FALSE */` |
|       ! 0 | 3252 | `		ph7_result_bool(pCtx,0);` |
|       ! 0 | 3253 | `		return PH7_OK;` |
|         - | 3254 | `	}` |
|         - | 3255 | `	/* Make sure we are dealing with a valid hashmap */` |
|        11 | 3256 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|         - | 3257 | `		/* Invalid argument,return FALSE */` |
|       ! 0 | 3258 | `		ph7_result_bool(pCtx,0);` |
|       ! 0 | 3259 | `		return PH7_OK;` |
|         - | 3260 | `	}` |
|        11 | 3261 | `	HashmapCurrentValue(&(*pCtx),(ph7_hashmap *)apArg[0]->x.pOther,1);` |
|        11 | 3262 | `	return PH7_OK;` |
|         6 | 3263 | `}` |
|         - | 3264 | `/*` |
|         - | 3265 | ` * value prev(array $input)` |
|         - | 3266 | ` *  Rewind the internal array pointer.` |
|         - | 3267 | ` * Parameter` |
|         - | 3268 | ` *  $input: The input array.` |
|         - | 3269 | ` * Return` |
|         - | 3270 | ` *  Returns the array value in the previous place that's pointed` |
|         - | 3271 | ` *  to by the internal array pointer, or FALSE if there are no more` |
|         - | 3272 | ` *  elements.` |
|         - | 3273 | ` */` |
|         2 | 3274 | `static int ph7_hashmap_prev(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|         1 | 3275 | `{` |
|         3 | 3276 | `	if( nArg < 1 ){` |
|         - | 3277 | `		/* Missing arguments,return FALSE */` |
|       ! 0 | 3278 | `		ph7_result_bool(pCtx,0);` |
|       ! 0 | 3279 | `		return PH7_OK;` |
|         - | 3280 | `	}` |
|         - | 3281 | `	/* Make sure we are dealing with a valid hashmap */` |
|         3 | 3282 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|         - | 3283 | `		/* Invalid argument,return FALSE */` |
|       ! 0 | 3284 | `		ph7_result_bool(pCtx,0);` |
|       ! 0 | 3285 | `		return PH7_OK;` |
|         - | 3286 | `	}` |
|         3 | 3287 | `	HashmapCurrentValue(&(*pCtx),(ph7_hashmap *)apArg[0]->x.pOther,-1);` |
|         3 | 3288 | `	return PH7_OK;` |
|         2 | 3289 | `}` |
|         - | 3290 | `/*` |
|         - | 3291 | ` * value end(array $input)` |
|         - | 3292 | ` *  Set the internal pointer of an array to its last element.` |
|         - | 3293 | ` * Parameter` |
|         - | 3294 | ` *  $input: The input array.` |
|         - | 3295 | ` * Return` |
|         - | 3296 | ` *  Returns the value of the last element or FALSE for empty array.` |
|         - | 3297 | ` */` |
|         2 | 3298 | `static int ph7_hashmap_end(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|         1 | 3299 | `{` |
|         - | 3300 | `	ph7_hashmap *pMap;` |
|         3 | 3301 | `	if( nArg < 1 ){` |
|         - | 3302 | `		/* Missing arguments,return FALSE */` |
|       ! 0 | 3303 | `		ph7_result_bool(pCtx,0);` |
|       ! 0 | 3304 | `		return PH7_OK;` |
|         - | 3305 | `	}` |
|         - | 3306 | `	/* Make sure we are dealing with a valid hashmap */` |
|         3 | 3307 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|         - | 3308 | `		/* Invalid argument,return FALSE */` |
|       ! 0 | 3309 | `		ph7_result_bool(pCtx,0);` |
|       ! 0 | 3310 | `		return PH7_OK;` |
|         - | 3311 | `	}` |
|         - | 3312 | `	/* Point to the internal representation of the input hashmap */` |
|         3 | 3313 | `	pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|         - | 3314 | `	/* Point to the last node */` |
|         3 | 3315 | `	pMap->pCur = pMap->pLast;` |
|         - | 3316 | `	/* Return the last node value */` |
|         3 | 3317 | `	HashmapCurrentValue(&(*pCtx),pMap,0);` |
|         3 | 3318 | `	return PH7_OK;` |
|         2 | 3319 | `}` |
|         - | 3320 | `/*` |
|         - | 3321 | ` * value reset(array $array )` |
|         - | 3322 | ` *  Set the internal pointer of an array to its first element.` |
|         - | 3323 | ` * Parameter` |
|         - | 3324 | ` *  $input: The input array.` |
|         - | 3325 | ` * Return` |
|         - | 3326 | ` *  Returns the value of the first array element,or FALSE if the array is empty.` |
|         - | 3327 | ` */` |
|         4 | 3328 | `static int ph7_hashmap_reset(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|         1 | 3329 | `{` |
|         - | 3330 | `	ph7_hashmap *pMap;` |
|         5 | 3331 | `	if( nArg < 1 ){` |
|         - | 3332 | `		/* Missing arguments,return FALSE */` |
|       ! 0 | 3333 | `		ph7_result_bool(pCtx,0);` |
|       ! 0 | 3334 | `		return PH7_OK;` |
|         - | 3335 | `	}` |
|         - | 3336 | `	/* Make sure we are dealing with a valid hashmap */` |
|         5 | 3337 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|         - | 3338 | `		/* Invalid argument,return FALSE */` |
|       ! 0 | 3339 | `		ph7_result_bool(pCtx,0);` |
|       ! 0 | 3340 | `		return PH7_OK;` |
|         - | 3341 | `	}` |
|         - | 3342 | `	/* Point to the internal representation of the input hashmap */` |
|         5 | 3343 | `	pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|         - | 3344 | `	/* Point to the first node */` |
|         5 | 3345 | `	pMap->pCur = pMap->pFirst;` |
|         - | 3346 | `	/* Return the last node value if available */` |
|         5 | 3347 | `	HashmapCurrentValue(&(*pCtx),pMap,0);` |
|         5 | 3348 | `	return PH7_OK;` |
|         3 | 3349 | `}` |
|         - | 3350 | `/*` |
|         - | 3351 | ` * Emit a node's key (integer or blob) as the call result — shared by key(),` |
|         - | 3352 | ` * array_key_first() and array_key_last().` |
|         - | 3353 | ` */` |
|        20 | 3354 | `static void HashmapResultNodeKey(ph7_context *pCtx,ph7_hashmap_node *pNode)` |
|         1 | 3355 | `{` |
|        21 | 3356 | `	if( pNode->iType == HASHMAP_INT_NODE ){` |
|         - | 3357 | `		/* Key is integer */` |
|        15 | 3358 | `		ph7_result_int64(pCtx,pNode->xKey.iKey);` |
|         8 | 3359 | `	}else{` |
|         - | 3360 | `		/* Key is blob */` |
|        10 | 3361 | `		ph7_result_string(pCtx,` |
|         6 | 3362 | `			(const char *)SyBlobData(&pNode->xKey.sKey),(int)SyBlobLength(&pNode->xKey.sKey));` |
|         - | 3363 | `	}` |
|        21 | 3364 | `}` |
|         - | 3365 | `/*` |
|         - | 3366 | ` * value key(array $array)` |
|         - | 3367 | ` *   Fetch a key from an array` |
|         - | 3368 | ` * Parameter` |
|         - | 3369 | ` *  $input` |
|         - | 3370 | ` *   The input array.` |
|         - | 3371 | ` * Return` |
|         - | 3372 | ` *  The key() function simply returns the key of the array element that's currently` |
|         - | 3373 | ` *  being pointed to by the internal pointer. It does not move the pointer in any way.` |
|         - | 3374 | ` *  If the internal pointer points beyond the end of the elements list or the array` |
|         - | 3375 | ` *  is empty, key() returns NULL.` |
|         - | 3376 | ` */` |
|         4 | 3377 | `static int ph7_hashmap_simple_key(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|         1 | 3378 | `{` |
|         - | 3379 | `	ph7_hashmap_node *pCur;` |
|         - | 3380 | `	ph7_hashmap *pMap;` |
|         5 | 3381 | `	if( nArg < 1 ){` |
|         - | 3382 | `		/* Missing arguments,return NULL */` |
|       ! 0 | 3383 | `		ph7_result_null(pCtx);` |
|       ! 0 | 3384 | `		return PH7_OK;` |
|         - | 3385 | `	}` |
|         - | 3386 | `	/* Make sure we are dealing with a valid hashmap */` |
|         5 | 3387 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|         - | 3388 | `		/* Invalid argument,return NULL */` |
|       ! 0 | 3389 | `		ph7_result_null(pCtx);` |
|       ! 0 | 3390 | `		return PH7_OK;` |
|         - | 3391 | `	}` |
|         5 | 3392 | `	pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|         5 | 3393 | `	pCur = pMap->pCur;` |
|         5 | 3394 | `	if( pCur == 0 ){` |
|         - | 3395 | `		/* Cursor does not point to anything,return NULL */` |
|       ! 0 | 3396 | `		ph7_result_null(pCtx);` |
|       ! 0 | 3397 | `		return PH7_OK;` |
|         - | 3398 | `	}` |
|         5 | 3399 | `	HashmapResultNodeKey(pCtx,pCur);` |
|         5 | 3400 | `	return PH7_OK;` |
|         3 | 3401 | `}` |
|         - | 3402 | `/*` |
|         - | 3403 | ` * array each(array $input)` |
|         - | 3404 | ` *  Return the current key and value pair from an array and advance the array cursor.` |
|         - | 3405 | ` * Parameter` |
|         - | 3406 | ` *  $input` |
|         - | 3407 | ` *    The input array.` |
|         - | 3408 | ` * Return` |
|         - | 3409 | ` *  Returns the current key and value pair from the array array. This pair is returned` |
|         - | 3410 | ` *  in a four-element array, with the keys 0, 1, key, and value. Elements 0 and key` |
|         - | 3411 | ` *  contain the key name of the array element, and 1 and value contain the data.` |
|         - | 3412 | ` *  If the internal pointer for the array points past the end of the array contents` |
|         - | 3413 | ` *  each() returns FALSE.` |
|         - | 3414 | ` */` |
|        22 | 3415 | `static int ph7_hashmap_each(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|         1 | 3416 | `{` |
|         - | 3417 | `	ph7_hashmap_node *pCur;` |
|         - | 3418 | `	ph7_hashmap *pMap;` |
|         - | 3419 | `	ph7_value *pArray;` |
|         - | 3420 | `	ph7_value *pVal;` |
|         - | 3421 | `	ph7_value sKey;` |
|        23 | 3422 | `	if( nArg < 1 ){` |
|         - | 3423 | `		/* Missing arguments,return FALSE */` |
|       ! 0 | 3424 | `		ph7_result_bool(pCtx,0);` |
|       ! 0 | 3425 | `		return PH7_OK;` |
|         - | 3426 | `	}` |
|         - | 3427 | `	/* Make sure we are dealing with a valid hashmap */` |
|        23 | 3428 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|         - | 3429 | `		/* Invalid argument,return FALSE */` |
|       ! 0 | 3430 | `		ph7_result_bool(pCtx,0);` |
|       ! 0 | 3431 | `		return PH7_OK;` |
|         - | 3432 | `	}` |
|         - | 3433 | `	/* Point to the internal representation that describe the input hashmap */` |
|        23 | 3434 | `	pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|        23 | 3435 | `	if( pMap->pCur == 0 ){` |
|         - | 3436 | `		/* Cursor does not point to anything,return FALSE */` |
|         9 | 3437 | `		ph7_result_bool(pCtx,0);` |
|         9 | 3438 | `		return PH7_OK;` |
|         - | 3439 | `	}` |
|        15 | 3440 | `	pCur = pMap->pCur;` |
|         - | 3441 | `	/* Create a new array */` |
|        15 | 3442 | `	pArray = ph7_context_new_array(pCtx);` |
|        15 | 3443 | `	if( pArray == 0 ){` |
|       ! 0 | 3444 | `		ph7_result_bool(pCtx,0);` |
|       ! 0 | 3445 | `		return PH7_OK;` |
|         - | 3446 | `	}` |
|        15 | 3447 | `	pVal = HashmapExtractNodeValue(pCur);` |
|         - | 3448 | `	/* Insert the current value */` |
|        15 | 3449 | `	ph7_array_add_intkey_elem(pArray,1,pVal);` |
|        15 | 3450 | `	ph7_array_add_strkey_elem(pArray,"value",pVal);` |
|         - | 3451 | `	/* Make the key */` |
|        15 | 3452 | `	if( pCur->iType == HASHMAP_INT_NODE ){` |
|         7 | 3453 | `		PH7_MemObjInitFromInt(pMap->pVm,&sKey,pCur->xKey.iKey);` |
|         4 | 3454 | `	}else{` |
|         9 | 3455 | `		PH7_MemObjInitFromString(pMap->pVm,&sKey,0);` |
|         9 | 3456 | `		PH7_MemObjStringAppend(&sKey,(const char *)SyBlobData(&pCur->xKey.sKey),SyBlobLength(&pCur->xKey.sKey));` |
|         - | 3457 | `	}` |
|         - | 3458 | `	/* Insert the current key */` |
|        15 | 3459 | `	ph7_array_add_intkey_elem(pArray,0,&sKey);` |
|        15 | 3460 | `	ph7_array_add_strkey_elem(pArray,"key",&sKey);` |
|        15 | 3461 | `	PH7_MemObjRelease(&sKey);` |
|         - | 3462 | `	/* Advance the cursor */` |
|        15 | 3463 | `	pMap->pCur = pCur->pPrev; /* Reverse link */` |
|         - | 3464 | `	/* Return the current entry */` |
|        15 | 3465 | `	ph7_result_value(pCtx,pArray);` |
|        15 | 3466 | `	return PH7_OK;` |
|        12 | 3467 | `}` |
|         - | 3468 | `/*` |
|         - | 3469 | ` * range() — a faithful port of php 8.5's ext/standard/array.c implementation` |
|         - | 3470 | ` * (php_range_process_input + PHP_FUNCTION(range)), so the value semantics,` |
|         - | 3471 | ` * diagnostics, and their ordering are byte-exact: decreasing ranges, float` |
|         - | 3472 | ` * ranges, character ranges, the step/endpoint ValueErrors, the ZPP TypeErrors` |
|         - | 3473 | ` * and null deprecations, and the string-endpoint warnings.` |
|         - | 3474 | ` */` |
|         - | 3475 | `#define PH7_RANGE_HT_MAX_SIZE 1073741824 /* php's HT_MAX_SIZE (2^30 entries) */` |
|         - | 3476 | `/*` |
|         - | 3477 | ` * Endpoint classification, mirroring php_range_process_input's return` |
|         - | 3478 | ` * contract. php returns zval type tags whose ORDER encodes the logic` |
|         - | 3479 | ` * (IS_LONG < IS_DOUBLE < IS_STRING < IS_ARRAY); the >=/< comparisons in` |
|         - | 3480 | ` * ph7_hashmap_range depend on the same ordering here.` |
|         - | 3481 | ` *   RANGE_IN_LONG/DOUBLE : only interpretable as int / float` |
|         - | 3482 | ` *   RANGE_IN_STRING      : only interpretable as a (char-range) string` |
|         - | 3483 | ` *   RANGE_IN_DIGIT       : single-byte numeric string — valid as both a char` |
|         - | 3484 | ` *                          and a number (php returns IS_ARRAY for this)` |
|         - | 3485 | ` * The RANGE_IN_* codes and RangeStrToNumber are declared in ph7int.h so the` |
|         - | 3486 | ` * stage-2 ZPP domain-error sweep can reuse the classifier (PLAN §3.9(a)).` |
|         - | 3487 | ` */` |
|         - | 3488 | `/* IEEE special-value tests: the engine-wide bit-pattern macros from` |
|         - | 3489 | ` * sxtypes.h (via ph7int.h) — same ones the printf/serialize paths use. */` |
|         - | 3490 | `/*` |
|         - | 3491 | ` * The type name php's ZPP prints after "must be of type ..., X given":` |
|         - | 3492 | ` * the concrete class name for objects, the usual type name otherwise.` |
|         - | 3493 | ` */` |
|         8 | 3494 | `static const char * RangeArgTypeName(ph7_value *pVal,char *zBuf,sxu32 nBufLen)` |
|         1 | 3495 | `{` |
|         9 | 3496 | `	if( pVal->iFlags & MEMOBJ_OBJ ){` |
|         3 | 3497 | `		ph7_class_instance *pThis = (ph7_class_instance *)pVal->x.pOther;` |
|         3 | 3498 | `		sxu32 n = SXMIN(pThis->pClass->sName.nByte,nBufLen - 1);` |
|         3 | 3499 | `		SyMemcpy((const void *)pThis->pClass->sName.zString,zBuf,n);` |
|         3 | 3500 | `		zBuf[n] = 0;` |
|         3 | 3501 | `		return zBuf;` |
|         - | 3502 | `	}` |
|         7 | 3503 | `	return ph7_type_name(pVal);` |
|         5 | 3504 | `}` |
|         - | 3505 | `/*` |
|         - | 3506 | ` * Classify a string with php's is_numeric_string() grammar:` |
|         - | 3507 | ` *   [ws] [sign] ( D+ [ . D* ] \| . D+ ) [ (e\|E) [sign] D+ ] [ws]` |
|         - | 3508 | ` * — the whole string must be consumed; hex/binary/"INF"/"NAN" are NOT` |
|         - | 3509 | ` * numeric. Returns RANGE_IN_LONG with *pLong set, RANGE_IN_DOUBLE with` |
|         - | 3510 | ` * *pDouble set (a fractional/exponent form, or an integer too wide for an` |
|         - | 3511 | ` * sxi64 — php reclassifies those as float), or RANGE_IN_ERROR when the` |
|         - | 3512 | ` * string is not numeric. The float value comes from libc strtod, like` |
|         - | 3513 | ` * php's zend_strtod (byte-exact-floats rule). zIn must be NUL-terminated` |
|         - | 3514 | ` * at zIn[nLen] — ph7_value_to_string guarantees this (SyBlobNullAppend) —` |
|         - | 3515 | ` * so strtod can parse it in place once the grammar has validated it.` |
|         - | 3516 | ` */` |
|       156 | 3517 | `PH7_PRIVATE sxu8 RangeStrToNumber(const char *zIn,sxu32 nLen,sxi64 *pLong,double *pDouble)` |
|         1 | 3518 | `{` |
|       157 | 3519 | `	const char *z = zIn,*zEnd = &zIn[nLen];` |
|       157 | 3520 | `	sxu64 uVal = 0;` |
|       157 | 3521 | `	int bNeg = 0,bDigit = 0,bReal = 0,bOverflow = 0;` |
|       167 | 3522 | `	while( z < zEnd && (unsigned char)z[0] < 0xc0 && SyisSpace(z[0]) ){ z++; }` |
|       157 | 3523 | `	if( z < zEnd && (z[0] == '+' \|\| z[0] == '-') ){` |
|         3 | 3524 | `		bNeg = (z[0] == '-');` |
|         3 | 3525 | `		z++;` |
|         1 | 3526 | `	}` |
|       237 | 3527 | `	while( z < zEnd && (unsigned char)z[0] < 0xc0 && SyisDigit(z[0]) ){` |
|        81 | 3528 | `		int d = z[0] - '0';` |
|         - | 3529 | `		/* Track overflow past 2^63, the widest magnitude an sxi64 can carry` |
|         - | 3530 | `		 * (as LONG_MIN); overflowing integers become floats like in php. */` |
|        81 | 3531 | `		if( uVal > 922337203685477580ULL \|\| (uVal == 922337203685477580ULL && d > 8) ){` |
|       ! 0 | 3532 | `			bOverflow = 1;` |
|       ! 0 | 3533 | `		}else{` |
|        81 | 3534 | `			uVal = uVal * 10 + (sxu64)d;` |
|         - | 3535 | `		}` |
|        81 | 3536 | `		bDigit = 1;` |
|        81 | 3537 | `		z++;` |
|         1 | 3538 | `	}` |
|       157 | 3539 | `	if( z < zEnd && z[0] == '.' ){` |
|         3 | 3540 | `		bReal = 1;` |
|         3 | 3541 | `		z++;` |
|         5 | 3542 | `		while( z < zEnd && (unsigned char)z[0] < 0xc0 && SyisDigit(z[0]) ){` |
|         3 | 3543 | `			bDigit = 1;` |
|         3 | 3544 | `			z++;` |
|         1 | 3545 | `		}` |
|         1 | 3546 | `	}` |
|         - | 3547 | `	/* At least one mantissa digit required (rejects "", ".", "+", "e5"). */` |
|       157 | 3548 | `	if( !bDigit ){` |
|        61 | 3549 | `		return RANGE_IN_ERROR;` |
|         - | 3550 | `	}` |
|         - | 3551 | `	/* Optional exponent — needs at least one digit (rejects "1e", "1e+"). */` |
|        97 | 3552 | `	if( z < zEnd && (z[0] == 'e' \|\| z[0] == 'E') ){` |
|         9 | 3553 | `		z++;` |
|         9 | 3554 | `		if( z < zEnd && (z[0] == '+' \|\| z[0] == '-') ){ z++; }` |
|         9 | 3555 | `		if( z >= zEnd \|\| (unsigned char)z[0] >= 0xc0 \|\| !SyisDigit(z[0]) ){` |
|       ! 0 | 3556 | `			return RANGE_IN_ERROR;` |
|         - | 3557 | `		}` |
|         9 | 3558 | `		bReal = 1;` |
|        17 | 3559 | `		while( z < zEnd && (unsigned char)z[0] < 0xc0 && SyisDigit(z[0]) ){ z++; }` |
|         4 | 3560 | `	}` |
|         - | 3561 | `	/* Trailing whitespace allowed; anything else means not numeric. */` |
|       101 | 3562 | `	while( z < zEnd && (unsigned char)z[0] < 0xc0 && SyisSpace(z[0]) ){ z++; }` |
|        97 | 3563 | `	if( z != zEnd ){` |
|        13 | 3564 | `		return RANGE_IN_ERROR;` |
|         - | 3565 | `	}` |
|        84 | 3566 | `	if( bOverflow \|\| (!bNeg && uVal > (sxu64)LARGEST_INT64)` |
|        43 | 3567 | `	 \|\| (bNeg && uVal > (sxu64)LARGEST_INT64 + 1) ){` |
|        84 | 3568 | `		bReal = 1;` |
|        84 | 3569 | `	}` |
|        43 | 3570 | `	if( bReal ){` |
|        11 | 3571 | `		*pDouble = strtod(zIn,0);` |
|        11 | 3572 | `		return RANGE_IN_DOUBLE;` |
|         - | 3573 | `	}` |
|         - | 3574 | `	/* Negate in unsigned space so 2^63 lands on LONG_MIN without overflow. */` |
|        33 | 3575 | `	*pLong = bNeg ? (sxi64)((sxu64)0 - uVal) : (sxi64)uVal;` |
|        33 | 3576 | `	return RANGE_IN_LONG;` |
|        58 | 3577 | `}` |
|         - | 3578 | `/*` |
|         - | 3579 | ` * ZPP emulation for $start/$end (php's Z_PARAM_NUMBER_OR_STR, weak mode):` |
|         - | 3580 | ` * reject array/object/resource with php's TypeError, deprecate null (the` |
|         - | 3581 | ` * value then reads as int 0 — *pbNullCoerced). php runs this for all` |
|         - | 3582 | ` * arguments BEFORE any value/domain check, hence the split from` |
|         - | 3583 | ` * RangeProcessInput below. Returns FALSE after throwing (*pRc set).` |
|         - | 3584 | ` */` |
|       262 | 3585 | `static int RangeEndpointZpp(ph7_context *pCtx,ph7_value *pIn,int iArg,const char *zName,int *pbNullCoerced,sxi32 *pRc)` |
|         1 | 3586 | `{` |
|         - | 3587 | `	char zMsg[160];` |
|       263 | 3588 | `	*pRc = PH7_OK;` |
|       263 | 3589 | `	if( pIn->iFlags & (MEMOBJ_HASHMAP\|MEMOBJ_OBJ\|MEMOBJ_RES) ){` |
|         - | 3590 | `		char zType[80];` |
|        10 | 3591 | `		*pRc = PH7_VmThrowException(pCtx,"TypeError",` |
|         - | 3592 | `			"range(): Argument #%d ($%s) must be of type string\|int\|float, %s given",` |
|         3 | 3593 | `			iArg,zName,RangeArgTypeName(pIn,zType,sizeof(zType)));` |
|         7 | 3594 | `		return FALSE;` |
|         - | 3595 | `	}` |
|       257 | 3596 | `	if( pIn->iFlags & MEMOBJ_NULL ){` |
|         7 | 3597 | `		SyBufferFormat(zMsg,sizeof(zMsg),` |
|         - | 3598 | `			"range(): Passing null to parameter #%d ($%s) of type string\|int\|float is deprecated",` |
|         2 | 3599 | `			iArg,zName);` |
|         5 | 3600 | `		PH7_VmThrowError(pCtx->pVm,0,E_DEPRECATED,zMsg);` |
|         5 | 3601 | `		*pbNullCoerced = TRUE;` |
|         2 | 3602 | `	}` |
|       257 | 3603 | `	return TRUE;` |
|       132 | 3604 | `}` |
|         - | 3605 | `/*` |
|         - | 3606 | ` * ZPP emulation for $step (php's Z_PARAM_NUMBER, weak mode): int/float pass` |
|         - | 3607 | ` * through, bool coerces to int, null deprecates to int 0 (which then trips` |
|         - | 3608 | ` * the "cannot be 0" ValueError like php), a numeric string coerces to its` |
|         - | 3609 | ` * number, anything else is a TypeError. Returns RANGE_IN_LONG/DOUBLE, or` |
|         - | 3610 | ` * RANGE_IN_ERROR after throwing (*pRc set).` |
|         - | 3611 | ` */` |
|        62 | 3612 | `static sxu8 RangeStepInput(ph7_context *pCtx,ph7_value *pIn,sxi64 *pLong,double *pDouble,sxi32 *pRc)` |
|         1 | 3613 | `{` |
|        63 | 3614 | `	*pRc = PH7_OK;` |
|        63 | 3615 | `	if( pIn->iFlags & (MEMOBJ_HASHMAP\|MEMOBJ_OBJ\|MEMOBJ_RES) ){` |
|         - | 3616 | `		char zType[80];` |
|         4 | 3617 | `		*pRc = PH7_VmThrowException(pCtx,"TypeError",` |
|         - | 3618 | `			"range(): Argument #3 ($step) must be of type int\|float, %s given",` |
|         1 | 3619 | `			RangeArgTypeName(pIn,zType,sizeof(zType)));` |
|         3 | 3620 | `		return RANGE_IN_ERROR;` |
|         - | 3621 | `	}` |
|        61 | 3622 | `	if( pIn->iFlags & MEMOBJ_NULL ){` |
|         3 | 3623 | `		PH7_VmThrowError(pCtx->pVm,0,E_DEPRECATED,` |
|         - | 3624 | `			"range(): Passing null to parameter #3 ($step) of type int\|float is deprecated");` |
|         3 | 3625 | `		*pLong = 0;` |
|         3 | 3626 | `		return RANGE_IN_LONG;` |
|         - | 3627 | `	}` |
|        59 | 3628 | `	if( pIn->iFlags & MEMOBJ_REAL ){` |
|        25 | 3629 | `		*pDouble = ph7_value_to_double(pIn);` |
|        25 | 3630 | `		return RANGE_IN_DOUBLE;` |
|         - | 3631 | `	}` |
|        35 | 3632 | `	if( pIn->iFlags & MEMOBJ_STRING ){` |
|         - | 3633 | `		const char *zStr;` |
|         - | 3634 | `		int nLen;` |
|         - | 3635 | `		sxu8 iKind;` |
|         3 | 3636 | `		zStr = ph7_value_to_string(pIn,&nLen);` |
|         3 | 3637 | `		iKind = RangeStrToNumber(zStr,(sxu32)nLen,pLong,pDouble);` |
|         3 | 3638 | `		if( iKind == RANGE_IN_ERROR ){` |
|         3 | 3639 | `			*pRc = PH7_VmThrowException(pCtx,"TypeError",` |
|         - | 3640 | `				"range(): Argument #3 ($step) must be of type int\|float, string given");` |
|         1 | 3641 | `		}` |
|         3 | 3642 | `		return iKind;` |
|         - | 3643 | `	}` |
|         - | 3644 | `	/* int / bool */` |
|        33 | 3645 | `	*pLong = ph7_value_to_int64(pIn);` |
|        33 | 3646 | `	return RANGE_IN_LONG;` |
|        32 | 3647 | `}` |
|         - | 3648 | `/*` |
|         - | 3649 | ` * php_range_process_input port: resolve $start/$end into a number and/or a` |
|         - | 3650 | ` * char-range byte, emitting php's exact warnings (empty string, multi-byte` |
|         - | 3651 | ` * string) and ValueErrors (INF/NAN). Returns a RANGE_IN_* code, or` |
|         - | 3652 | ` * RANGE_IN_ERROR after throwing (*pRc set).` |
|         - | 3653 | ` */` |
|       220 | 3654 | `static sxu8 RangeProcessInput(ph7_context *pCtx,ph7_value *pIn,int iArg,const char *zName,` |
|         - | 3655 | `	int bNullCoerced,sxi64 *pLong,double *pDouble,unsigned char *pChar,sxi32 *pRc)` |
|         1 | 3656 | `{` |
|         - | 3657 | `	char zMsg[160];` |
|         - | 3658 | `	double r;` |
|       221 | 3659 | `	*pRc = PH7_OK;` |
|       221 | 3660 | `	if( bNullCoerced ){` |
|         - | 3661 | `		/* ZPP already deprecated the null; it reads as int 0. */` |
|         5 | 3662 | `		*pLong = 0;` |
|         5 | 3663 | `		*pDouble = 0.0;` |
|         5 | 3664 | `		return RANGE_IN_LONG;` |
|         - | 3665 | `	}` |
|       217 | 3666 | `	if( pIn->iFlags & MEMOBJ_REAL ){` |
|        21 | 3667 | `		r = ph7_value_to_double(pIn);` |
|        12 | 3668 | `check_dval:` |
|        25 | 3669 | `		if( PH7_IS_INF(r) ){` |
|         7 | 3670 | `			*pRc = PH7_VmThrowException(pCtx,"ValueError",` |
|         2 | 3671 | `				"range(): Argument #%d ($%s) must be a finite number, INF provided",iArg,zName);` |
|         5 | 3672 | `			return RANGE_IN_ERROR;` |
|         - | 3673 | `		}` |
|        21 | 3674 | `		if( PH7_IS_NAN(r) ){` |
|         7 | 3675 | `			*pRc = PH7_VmThrowException(pCtx,"ValueError",` |
|         2 | 3676 | `				"range(): Argument #%d ($%s) must be a finite number, NAN provided",iArg,zName);` |
|         5 | 3677 | `			return RANGE_IN_ERROR;` |
|         - | 3678 | `		}` |
|        17 | 3679 | `		*pDouble = r;` |
|        17 | 3680 | `		return RANGE_IN_DOUBLE;` |
|         - | 3681 | `	}` |
|       197 | 3682 | `	if( pIn->iFlags & MEMOBJ_STRING ){` |
|         - | 3683 | `		const char *zStr;` |
|         - | 3684 | `		int nLen;` |
|         - | 3685 | `		sxu8 iKind;` |
|        81 | 3686 | `		zStr = ph7_value_to_string(pIn,&nLen);` |
|        81 | 3687 | `		if( nLen == 0 ){` |
|         7 | 3688 | `			SyBufferFormat(zMsg,sizeof(zMsg),` |
|         2 | 3689 | `				"range(): Argument #%d ($%s) must not be empty, casted to 0",iArg,zName);` |
|         5 | 3690 | `			PH7_VmThrowError(pCtx->pVm,0,PH7_CTX_WARNING,zMsg);` |
|         5 | 3691 | `			*pLong = 0;` |
|         5 | 3692 | `			*pDouble = 0.0;` |
|        41 | 3693 | `			return RANGE_IN_LONG;` |
|         - | 3694 | `		}` |
|        77 | 3695 | `		iKind = RangeStrToNumber(zStr,(sxu32)nLen,pLong,pDouble);` |
|        77 | 3696 | `		if( iKind == RANGE_IN_DOUBLE ){` |
|         5 | 3697 | `			r = *pDouble;` |
|         5 | 3698 | `			goto check_dval;` |
|         - | 3699 | `		}` |
|        73 | 3700 | `		if( iKind == RANGE_IN_LONG ){` |
|        23 | 3701 | `			*pDouble = (double)*pLong;` |
|        23 | 3702 | `			if( nLen == 1 ){` |
|         - | 3703 | `				/* A single numeric digit works as both a char and a number. */` |
|         9 | 3704 | `				*pChar = (unsigned char)zStr[0];` |
|         9 | 3705 | `				return RANGE_IN_DIGIT;` |
|         - | 3706 | `			}` |
|        15 | 3707 | `			return RANGE_IN_LONG;` |
|         - | 3708 | `		}` |
|        51 | 3709 | `		if( nLen != 1 ){` |
|        10 | 3710 | `			SyBufferFormat(zMsg,sizeof(zMsg),` |
|         3 | 3711 | `				"range(): Argument #%d ($%s) must be a single byte, subsequent bytes are ignored",iArg,zName);` |
|         7 | 3712 | `			PH7_VmThrowError(pCtx->pVm,0,PH7_CTX_WARNING,zMsg);` |
|         3 | 3713 | `		}` |
|        51 | 3714 | `		*pChar = (unsigned char)zStr[0];` |
|         - | 3715 | `		/* Fall-back numeric value in case the other argument is not a string. */` |
|        51 | 3716 | `		*pLong = 0;` |
|        51 | 3717 | `		*pDouble = 0.0;` |
|        51 | 3718 | `		return RANGE_IN_STRING;` |
|         - | 3719 | `	}` |
|         - | 3720 | `	/* int / bool */` |
|       117 | 3721 | `	*pLong = ph7_value_to_int64(pIn);` |
|       117 | 3722 | `	*pDouble = (double)*pLong;` |
|       117 | 3723 | `	return RANGE_IN_LONG;` |
|       111 | 3724 | `}` |
|         - | 3725 | `/*` |
|         - | 3726 | ` * The two "supplied range exceeds the maximum array size" ValueErrors.` |
|         - | 3727 | ` * Both php messages print the macro's (start,end) parameters, which its` |
|         - | 3728 | ` * callers pass SWAPPED for a decreasing range — a php quirk kept for` |
|         - | 3729 | ` * byte-parity (callers below pass the values to *print*). The int and` |
|         - | 3730 | ` * float variants differ in wording ("Maximum size: N." vs "Max size: N")` |
|         - | 3731 | ` * exactly like php's two macros.` |
|         - | 3732 | ` */` |
|         6 | 3733 | `static sxi32 RangeLongSizeError(ph7_context *pCtx,sxu64 nCalc,sxi64 iStart,sxi64 iEnd,sxi64 iStep)` |
|         1 | 3734 | `{` |
|        10 | 3735 | `	return PH7_VmThrowException(pCtx,"ValueError",` |
|         - | 3736 | `		"The supplied range exceeds the maximum array size by %qu elements: "` |
|         - | 3737 | `		"start=%qd, end=%qd, step=%qd. Calculated size: %qu. Maximum size: %qu.",` |
|         3 | 3738 | `		nCalc - (sxu64)(PH7_RANGE_HT_MAX_SIZE - 1),iStart,iEnd,iStep,` |
|         3 | 3739 | `		nCalc,(sxu64)PH7_RANGE_HT_MAX_SIZE);` |
|         1 | 3740 | `}` |
|         6 | 3741 | `static sxi32 RangeDoubleSizeError(ph7_context *pCtx,double rCalc,double rStart,double rEnd,double rStep)` |
|         1 | 3742 | `{` |
|         - | 3743 | `	/* Four %.1f doubles can reach ~313 bytes each near DBL_MAX, so format on` |
|         - | 3744 | `	 * the VM heap (auto-released with the call context) rather than parking` |
|         - | 3745 | `	 * ~1.5 KB on the native stack of a small-stack embedded port. */` |
|         7 | 3746 | `	const unsigned int nBuf = 1500;` |
|         7 | 3747 | `	char *zMsg = (char *)ph7_context_alloc_chunk(pCtx,nBuf,FALSE,TRUE/* Auto-release */);` |
|         7 | 3748 | `	if( zMsg == 0 ){` |
|       ! 0 | 3749 | `		return PH7_ContextMemoryError(pCtx);` |
|         - | 3750 | `	}` |
|         7 | 3751 | `	snprintf(zMsg,nBuf,` |
|         - | 3752 | `		"The supplied range exceeds the maximum array size by %.1f elements: "` |
|         - | 3753 | `		"start=%.1f, end=%.1f, step=%.1f. Max size: 1073741824",` |
|         - | 3754 | `		rCalc - (double)PH7_RANGE_HT_MAX_SIZE,rStart,rEnd,rStep);` |
|         7 | 3755 | `	return PH7_VmThrowException(pCtx,"ValueError","%s",zMsg);` |
|         4 | 3756 | `}` |
|         - | 3757 | `/*` |
|         - | 3758 | ` * Set the element container to the next range element and append it to the` |
|         - | 3759 | ` * result array, surfacing allocation failure as the OOM fatal (never a` |
|         - | 3760 | ` * silently-truncated array). One helper per element type so the fill loops` |
|         - | 3761 | ` * below stay one line per iteration.` |
|         - | 3762 | ` */` |
|       334 | 3763 | `static sxi32 RangeAppendInt(ph7_context *pCtx,ph7_value *pArray,ph7_value *pValue,sxi64 iVal)` |
|         1 | 3764 | `{` |
|       335 | 3765 | `	ph7_value_int64(pValue,iVal);` |
|       335 | 3766 | `	if( ph7_array_add_elem(pArray,0/* Automatic index assign*/,pValue) != SXRET_OK ){` |
|       ! 0 | 3767 | `		return PH7_ContextMemoryError(pCtx);` |
|         - | 3768 | `	}` |
|       335 | 3769 | `	return PH7_OK;` |
|       168 | 3770 | `}` |
|        70 | 3771 | `static sxi32 RangeAppendDouble(ph7_context *pCtx,ph7_value *pArray,ph7_value *pValue,double rVal)` |
|         1 | 3772 | `{` |
|        71 | 3773 | `	ph7_value_double(pValue,rVal);` |
|        71 | 3774 | `	if( ph7_array_add_elem(pArray,0,pValue) != SXRET_OK ){` |
|       ! 0 | 3775 | `		return PH7_ContextMemoryError(pCtx);` |
|         - | 3776 | `	}` |
|        71 | 3777 | `	return PH7_OK;` |
|        36 | 3778 | `}` |
|       168 | 3779 | `static sxi32 RangeAppendChar(ph7_context *pCtx,ph7_value *pArray,ph7_value *pValue,char c)` |
|         1 | 3780 | `{` |
|       169 | 3781 | `	ph7_value_string(pValue,&c,1);` |
|       169 | 3782 | `	if( ph7_array_add_elem(pArray,0,pValue) != SXRET_OK ){` |
|       ! 0 | 3783 | `		return PH7_ContextMemoryError(pCtx);` |
|         - | 3784 | `	}` |
|       169 | 3785 | `	ph7_value_reset_string_cursor(pValue);` |
|       169 | 3786 | `	return PH7_OK;` |
|        85 | 3787 | `}` |
|         - | 3788 | `/*` |
|         - | 3789 | ` * array range(string\|int\|float $start,string\|int\|float $end,int\|float $step = 1)` |
|         - | 3790 | ` *  Create an array containing a range of elements.` |
|         - | 3791 | ` * Return` |
|         - | 3792 | ` *  An array of elements from start to end, inclusive; int, float, or` |
|         - | 3793 | ` *  single-character string elements depending on the inputs, like php 8.` |
|         - | 3794 | ` */` |
|       136 | 3795 | `static int ph7_hashmap_range(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|         1 | 3796 | `{` |
|         - | 3797 | `	ph7_value *pValue,*pArray;` |
|       137 | 3798 | `	sxi32 rc = PH7_OK;` |
|       137 | 3799 | `	int is_step_double = 0,is_step_negative = 0;` |
|       137 | 3800 | `	double step_double = 1.0;` |
|       137 | 3801 | `	sxi64 step = 1;` |
|         - | 3802 | `	sxu8 start_type,end_type;` |
|       137 | 3803 | `	sxi64 start_long = 0,end_long = 0;` |
|       137 | 3804 | `	double start_double = 0.0,end_double = 0.0;` |
|       137 | 3805 | `	unsigned char cStart = 0,cEnd = 0;` |
|       137 | 3806 | `	int bStartNull = FALSE,bEndNull = FALSE;` |
|         - | 3807 | `	sxu32 i,size;` |
|         - | 3808 |  |
|         - | 3809 | `	/* php ZPP arity: at least 2 (enforced centrally, aBuiltinArity), at most 3. */` |
|       137 | 3810 | `	if( nArg > 3 ){` |
|         4 | 3811 | `		return PH7_VmThrowException(pCtx,"ArgumentCountError",` |
|         1 | 3812 | `			"range() expects at most 3 arguments, %d given",nArg);` |
|         - | 3813 | `	}` |
|       135 | 3814 | `	if( nArg < 2 ){` |
|         - | 3815 | `		/* Defensive only: the central arity table throws before we run. */` |
|       ! 0 | 3816 | `		return PH7_VmThrowException(pCtx,"ArgumentCountError",` |
|       ! 0 | 3817 | `			"range() expects at least 2 arguments, %d given",nArg);` |
|         - | 3818 | `	}` |
|         - | 3819 | `	/* ZPP pass in argument order: type errors and null deprecations fire` |
|         - | 3820 | `	 * before any value/domain check, like php's zend_parse_parameters. */` |
|       135 | 3821 | `	if( !RangeEndpointZpp(pCtx,apArg[0],1,"start",&bStartNull,&rc) ){` |
|         7 | 3822 | `		return rc;` |
|         - | 3823 | `	}` |
|       129 | 3824 | `	if( !RangeEndpointZpp(pCtx,apArg[1],2,"end",&bEndNull,&rc) ){` |
|       ! 0 | 3825 | `		return rc;` |
|         - | 3826 | `	}` |
|       129 | 3827 | `	if( nArg > 2 ){` |
|        63 | 3828 | `		sxu8 iStepKind = RangeStepInput(pCtx,apArg[2],&step,&step_double,&rc);` |
|        63 | 3829 | `		if( iStepKind == RANGE_IN_ERROR ){` |
|         5 | 3830 | `			return rc;` |
|         - | 3831 | `		}` |
|        59 | 3832 | `		if( iStepKind == RANGE_IN_DOUBLE ){` |
|        25 | 3833 | `			if( PH7_IS_INF(step_double) ){` |
|         3 | 3834 | `				return PH7_VmThrowException(pCtx,"ValueError",` |
|         - | 3835 | `					"range(): Argument #3 ($step) must be a finite number, INF provided");` |
|         - | 3836 | `			}` |
|        23 | 3837 | `			if( PH7_IS_NAN(step_double) ){` |
|         3 | 3838 | `				return PH7_VmThrowException(pCtx,"ValueError",` |
|         - | 3839 | `					"range(): Argument #3 ($step) must be a finite number, NAN provided");` |
|         - | 3840 | `			}` |
|         - | 3841 | `			/* We only want positive step values. */` |
|        21 | 3842 | `			if( step_double < 0.0 ){` |
|       ! 0 | 3843 | `				is_step_negative = 1;` |
|       ! 0 | 3844 | `				step_double *= -1;` |
|       ! 0 | 3845 | `			}` |
|         - | 3846 | `			/* zend_dval_to_lval_silent + zend_is_long_compatible: an integral` |
|         - | 3847 | `			 * in-sxi64-range float step behaves as an int (char ranges accept` |
|         - | 3848 | `			 * it, int endpoints stay int); anything else is a float step. */` |
|        21 | 3849 | `			if( step_double < 9223372036854775808.0 ){` |
|        19 | 3850 | `				step = (sxi64)step_double;` |
|        19 | 3851 | `				if( (double)step != step_double ){` |
|        17 | 3852 | `					is_step_double = 1;` |
|         8 | 3853 | `				}` |
|        10 | 3854 | `			}else{` |
|         - | 3855 | ``				/* Casting out-of-range would be UB; `step` stays unread —`` |
|         - | 3856 | `				 * every reader is gated behind !is_step_double. */` |
|         3 | 3857 | `				is_step_double = 1;` |
|         - | 3858 | `			}` |
|        11 | 3859 | `		}else{` |
|         - | 3860 | `			/* We only want positive step values. */` |
|        35 | 3861 | `			if( step < 0 ){` |
|        11 | 3862 | `				if( step == SMALLEST_INT64 ){` |
|         - | 3863 | `					/* -step would overflow */` |
|         4 | 3864 | `					return PH7_VmThrowException(pCtx,"ValueError",` |
|         1 | 3865 | `						"range(): Argument #3 ($step) must be greater than %qd",step);` |
|         - | 3866 | `				}` |
|         9 | 3867 | `				is_step_negative = 1;` |
|         9 | 3868 | `				step = -step;` |
|         4 | 3869 | `			}` |
|        33 | 3870 | `			step_double = (double)step;` |
|         - | 3871 | `		}` |
|        53 | 3872 | `		if( step_double == 0.0 ){` |
|         7 | 3873 | `			return PH7_VmThrowException(pCtx,"ValueError",` |
|         - | 3874 | `				"range(): Argument #3 ($step) cannot be 0");` |
|         - | 3875 | `		}` |
|        23 | 3876 | `	}` |
|       113 | 3877 | `	start_type = RangeProcessInput(pCtx,apArg[0],1,"start",bStartNull,&start_long,&start_double,&cStart,&rc);` |
|       113 | 3878 | `	if( start_type == RANGE_IN_ERROR ){` |
|         5 | 3879 | `		return rc;` |
|         - | 3880 | `	}` |
|       109 | 3881 | `	end_type = RangeProcessInput(pCtx,apArg[1],2,"end",bEndNull,&end_long,&end_double,&cEnd,&rc);` |
|       109 | 3882 | `	if( end_type == RANGE_IN_ERROR ){` |
|         5 | 3883 | `		return rc;` |
|         - | 3884 | `	}` |
|         - | 3885 | `	/* Element container + result array */` |
|       105 | 3886 | `	pValue = ph7_context_new_scalar(pCtx);` |
|       105 | 3887 | `	pArray = ph7_context_new_array(pCtx);` |
|       105 | 3888 | `	if( pValue == 0 \|\| pArray == 0 ){` |
|       ! 0 | 3889 | `		return PH7_ContextMemoryError(pCtx);` |
|         - | 3890 | `	}` |
|         - | 3891 | `	/* If the range is given as strings, generate an array of characters. */` |
|       105 | 3892 | `	if( start_type >= RANGE_IN_STRING \|\| end_type >= RANGE_IN_STRING ){` |
|        37 | 3893 | `		if( start_type < RANGE_IN_STRING \|\| end_type < RANGE_IN_STRING ){` |
|         - | 3894 | `			/* Only one side is a string: the char side converts to 0 (with a` |
|         - | 3895 | `			 * warning unless the numeric side is an ambiguous single digit)` |
|         - | 3896 | `			 * and the range is numeric. */` |
|        15 | 3897 | `			if( start_type < RANGE_IN_STRING ){` |
|         7 | 3898 | `				if( end_type != RANGE_IN_DIGIT ){` |
|         7 | 3899 | `					PH7_VmThrowError(pCtx->pVm,0,PH7_CTX_WARNING,` |
|         - | 3900 | `						"range(): Argument #1 ($start) must be a single byte string if"` |
|         - | 3901 | `						" argument #2 ($end) is a single byte string, argument #2 ($end) converted to 0");` |
|         3 | 3902 | `				}` |
|         7 | 3903 | `				end_type = RANGE_IN_LONG;` |
|         4 | 3904 | `			}else{` |
|         9 | 3905 | `				if( start_type != RANGE_IN_DIGIT ){` |
|         9 | 3906 | `					PH7_VmThrowError(pCtx->pVm,0,PH7_CTX_WARNING,` |
|         - | 3907 | `						"range(): Argument #2 ($end) must be a single byte string if"` |
|         - | 3908 | `						" argument #1 ($start) is a single byte string, argument #1 ($start) converted to 0");` |
|         4 | 3909 | `				}` |
|         9 | 3910 | `				start_type = RANGE_IN_LONG;` |
|         - | 3911 | `			}` |
|        15 | 3912 | `			goto handle_numeric_inputs;` |
|         - | 3913 | `		}` |
|        23 | 3914 | `		if( is_step_double ){` |
|         - | 3915 | `			/* Only emit the warning if one of the inputs is not a numeric digit. */` |
|         5 | 3916 | `			if( start_type == RANGE_IN_STRING \|\| end_type == RANGE_IN_STRING ){` |
|         3 | 3917 | `				PH7_VmThrowError(pCtx->pVm,0,PH7_CTX_WARNING,` |
|         - | 3918 | `					"range(): Argument #3 ($step) must be of type int when generating an array"` |
|         - | 3919 | `					" of characters, inputs converted to 0");` |
|         1 | 3920 | `			}` |
|         5 | 3921 | `			start_type = RANGE_IN_LONG;` |
|         5 | 3922 | `			end_type = RANGE_IN_LONG;` |
|         5 | 3923 | `			goto handle_numeric_inputs;` |
|         - | 3924 | `		}` |
|         - | 3925 | `		/* Generate an array of characters */` |
|        19 | 3926 | `		if( cStart > cEnd ){` |
|         - | 3927 | `			/* Decreasing char range */` |
|         - | 3928 | `			int iCur;` |
|         3 | 3929 | `			if( (sxi64)(cStart - cEnd) < step ){` |
|       ! 0 | 3930 | `				goto boundary_error;` |
|         - | 3931 | `			}` |
|        17 | 3932 | `			for( iCur = (int)cStart ; iCur >= (int)cEnd ; iCur -= (int)step ){` |
|        15 | 3933 | `				if( (rc = RangeAppendChar(pCtx,pArray,pValue,(char)iCur)) != PH7_OK ){` |
|       ! 0 | 3934 | `					return rc;` |
|         - | 3935 | `				}` |
|         8 | 3936 | `			}` |
|        18 | 3937 | `		}else if( cEnd > cStart ){` |
|         - | 3938 | `			/* Increasing char range */` |
|         - | 3939 | `			int iCur;` |
|        15 | 3940 | `			if( is_step_negative ){` |
|         3 | 3941 | `				goto negative_step_error;` |
|         - | 3942 | `			}` |
|        13 | 3943 | `			if( (sxi64)(cEnd - cStart) < step ){` |
|         3 | 3944 | `				goto boundary_error;` |
|         - | 3945 | `			}` |
|       163 | 3946 | `			for( iCur = (int)cStart ; iCur <= (int)cEnd ; iCur += (int)step ){` |
|       153 | 3947 | `				if( (rc = RangeAppendChar(pCtx,pArray,pValue,(char)iCur)) != PH7_OK ){` |
|       ! 0 | 3948 | `					return rc;` |
|         - | 3949 | `				}` |
|        77 | 3950 | `			}` |
|         6 | 3951 | `		}else{` |
|         3 | 3952 | `			if( (rc = RangeAppendChar(pCtx,pArray,pValue,(char)cStart)) != PH7_OK ){` |
|       ! 0 | 3953 | `				return rc;` |
|         - | 3954 | `			}` |
|         - | 3955 | `		}` |
|        15 | 3956 | `		ph7_result_value(pCtx,pArray);` |
|        15 | 3957 | `		return PH7_OK;` |
|         - | 3958 | `	}` |
|        34 | 3959 | `handle_numeric_inputs:` |
|        95 | 3960 | `	if( start_type == RANGE_IN_DOUBLE \|\| end_type == RANGE_IN_DOUBLE \|\| is_step_double ){` |
|         - | 3961 | `		/* Float range */` |
|         - | 3962 | `		double elem,calc;` |
|        25 | 3963 | `		if( start_double > end_double ){` |
|         - | 3964 | `			/* Decreasing float range */` |
|         7 | 3965 | `			if( start_double - end_double < step_double ){` |
|       ! 0 | 3966 | `				goto boundary_error;` |
|         - | 3967 | `			}` |
|         7 | 3968 | `			calc = ((start_double - end_double) / step_double) + 1;` |
|         7 | 3969 | `			if( calc >= (double)PH7_RANGE_HT_MAX_SIZE ){` |
|         - | 3970 | `				/* php prints start/end swapped here (see RangeDoubleSizeError). */` |
|         3 | 3971 | `				return RangeDoubleSizeError(pCtx,calc,end_double,start_double,step_double);` |
|         - | 3972 | `			}` |
|         5 | 3973 | `			size = (sxu32)(calc + 0.5); /* _php_math_round(...,0,HALF_UP) */` |
|        19 | 3974 | `			for( i = 0,elem = start_double ; i < size && elem >= end_double ; ++i,elem = start_double - ((double)i * step_double) ){` |
|        15 | 3975 | `				if( (rc = RangeAppendDouble(pCtx,pArray,pValue,elem)) != PH7_OK ){` |
|       ! 0 | 3976 | `					return rc;` |
|         - | 3977 | `				}` |
|         8 | 3978 | `			}` |
|        21 | 3979 | `		}else if( end_double > start_double ){` |
|         - | 3980 | `			/* Increasing float range */` |
|        17 | 3981 | `			if( is_step_negative ){` |
|       ! 0 | 3982 | `				goto negative_step_error;` |
|         - | 3983 | `			}` |
|        17 | 3984 | `			if( end_double - start_double < step_double ){` |
|         3 | 3985 | `				goto boundary_error;` |
|         - | 3986 | `			}` |
|        15 | 3987 | `			calc = ((end_double - start_double) / step_double) + 1;` |
|        15 | 3988 | `			if( calc >= (double)PH7_RANGE_HT_MAX_SIZE ){` |
|         5 | 3989 | `				return RangeDoubleSizeError(pCtx,calc,start_double,end_double,step_double);` |
|         - | 3990 | `			}` |
|        11 | 3991 | `			size = (sxu32)(calc + 0.5);` |
|        65 | 3992 | `			for( i = 0,elem = start_double ; i < size && elem <= end_double ; ++i,elem = start_double + ((double)i * step_double) ){` |
|        55 | 3993 | `				if( (rc = RangeAppendDouble(pCtx,pArray,pValue,elem)) != PH7_OK ){` |
|       ! 0 | 3994 | `					return rc;` |
|         - | 3995 | `				}` |
|        28 | 3996 | `			}` |
|         6 | 3997 | `		}else{` |
|         3 | 3998 | `			if( (rc = RangeAppendDouble(pCtx,pArray,pValue,start_double)) != PH7_OK ){` |
|       ! 0 | 3999 | `				return rc;` |
|         - | 4000 | `			}` |
|         - | 4001 | `		}` |
|         9 | 4002 | `	}else{` |
|         - | 4003 | `		/* Int range. All arithmetic in unsigned space so a span wider than` |
|         - | 4004 | `		 * LARGEST_INT64 (e.g. -PHP_INT_MAX..PHP_INT_MAX) wraps correctly` |
|         - | 4005 | `		 * instead of overflowing, exactly like php's zend_ulong math. */` |
|        63 | 4006 | `		sxu64 ustep = (sxu64)step;` |
|         - | 4007 | `		sxu64 calc;` |
|        63 | 4008 | `		if( start_long > end_long ){` |
|         - | 4009 | `			/* Decreasing int range */` |
|        19 | 4010 | `			if( (sxu64)start_long - (sxu64)end_long < ustep ){` |
|         3 | 4011 | `				goto boundary_error;` |
|         - | 4012 | `			}` |
|        17 | 4013 | `			calc = ((sxu64)start_long - (sxu64)end_long) / ustep;` |
|        17 | 4014 | `			if( calc >= (sxu64)(PH7_RANGE_HT_MAX_SIZE - 1) ){` |
|         - | 4015 | `				/* php prints start/end swapped here (see RangeLongSizeError). */` |
|         3 | 4016 | `				return RangeLongSizeError(pCtx,calc,end_long,start_long,step);` |
|         - | 4017 | `			}` |
|        15 | 4018 | `			size = (sxu32)(calc + 1);` |
|       101 | 4019 | `			for( i = 0 ; i < size ; ++i ){` |
|        87 | 4020 | `				if( (rc = RangeAppendInt(pCtx,pArray,pValue,(sxi64)((sxu64)start_long - (sxu64)i * ustep))) != PH7_OK ){` |
|       ! 0 | 4021 | `					return rc;` |
|         - | 4022 | `				}` |
|        44 | 4023 | `			}` |
|        52 | 4024 | `		}else if( end_long > start_long ){` |
|         - | 4025 | `			/* Increasing int range */` |
|        39 | 4026 | `			if( is_step_negative ){` |
|         3 | 4027 | `				goto negative_step_error;` |
|         - | 4028 | `			}` |
|        37 | 4029 | `			if( (sxu64)end_long - (sxu64)start_long < ustep ){` |
|         3 | 4030 | `				goto boundary_error;` |
|         - | 4031 | `			}` |
|        35 | 4032 | `			calc = ((sxu64)end_long - (sxu64)start_long) / ustep;` |
|        35 | 4033 | `			if( calc >= (sxu64)(PH7_RANGE_HT_MAX_SIZE - 1) ){` |
|         5 | 4034 | `				return RangeLongSizeError(pCtx,calc,start_long,end_long,step);` |
|         - | 4035 | `			}` |
|        31 | 4036 | `			size = (sxu32)(calc + 1);` |
|       273 | 4037 | `			for( i = 0 ; i < size ; ++i ){` |
|       243 | 4038 | `				if( (rc = RangeAppendInt(pCtx,pArray,pValue,(sxi64)((sxu64)start_long + (sxu64)i * ustep))) != PH7_OK ){` |
|       ! 0 | 4039 | `					return rc;` |
|         - | 4040 | `				}` |
|       122 | 4041 | `			}` |
|        16 | 4042 | `		}else{` |
|         7 | 4043 | `			if( (rc = RangeAppendInt(pCtx,pArray,pValue,start_long)) != PH7_OK ){` |
|       ! 0 | 4044 | `				return rc;` |
|         - | 4045 | `			}` |
|         - | 4046 | `		}` |
|         - | 4047 | `	}` |
|         - | 4048 | `	/* Return the new array. 'pValue' is released automatically by the` |
|         - | 4049 | `	 * virtual machine as soon as we return from this foreign function. */` |
|        67 | 4050 | `	ph7_result_value(pCtx,pArray);` |
|        67 | 4051 | `	return PH7_OK;` |
|         2 | 4052 | `negative_step_error:` |
|         5 | 4053 | `	return PH7_VmThrowException(pCtx,"ValueError",` |
|         - | 4054 | `		"range(): Argument #3 ($step) must be greater than 0 for increasing ranges");` |
|         4 | 4055 | `boundary_error:` |
|         9 | 4056 | `	return PH7_VmThrowException(pCtx,"ValueError",` |
|         - | 4057 | `		"range(): Argument #3 ($step) must be less than the range spanned by argument #1 ($start) and argument #2 ($end)");` |
|        69 | 4058 | `}` |
|         - | 4059 | `/*` |
|         - | 4060 | ` * array array_values(array $array)` |
|         - | 4061 | ` *  Return all the values of an array, indexed numerically.` |
|         - | 4062 | ` * Parameters` |
|         - | 4063 | ` *  $array` |
|         - | 4064 | ` *   The input array.` |
|         - | 4065 | ` * Return` |
|         - | 4066 | ` *  An indexed array of values or NULL on allocation failure.` |
|         - | 4067 | ` */` |
|        36 | 4068 | `static int ph7_hashmap_values(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|         4 | 4069 | `{` |
|         - | 4070 | `	ph7_hashmap_node *pNode;` |
|         - | 4071 | `	ph7_hashmap *pMap;` |
|         - | 4072 | `	ph7_value *pArray;` |
|         - | 4073 | `	ph7_value *pObj;` |
|         - | 4074 | `	sxu32 n;` |
|        40 | 4075 | `	if( nArg != 1 ){` |
|         - | 4076 | `		/* Wrong argument count, throw ArgumentCountError */` |
|         8 | 4077 | `		return PH7_VmThrowException(pCtx,` |
|         - | 4078 | `			"ArgumentCountError",` |
|         - | 4079 | `			"array_values() expects exactly 1 argument, %d given",` |
|         2 | 4080 | `			nArg` |
|         - | 4081 | `			);` |
|         - | 4082 | `	}` |
|         - | 4083 | `	/* Make sure we are dealing with a valid hashmap */` |
|        35 | 4084 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|         - | 4085 | `		/* Type mismatch, throw TypeError */` |
|         4 | 4086 | `		return PH7_VmThrowException(pCtx,` |
|         - | 4087 | `			"TypeError",` |
|         - | 4088 | `			"array_values(): Argument #1 ($array) must be of type array, %s given",` |
|         1 | 4089 | `			ph7_type_name(apArg[0])` |
|         - | 4090 | `			);` |
|         - | 4091 | `	}` |
|         - | 4092 | `	/* Point to the internal representation that describe the input hashmap */` |
|        32 | 4093 | `	pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|         - | 4094 | `	/* Create a new array */` |
|        32 | 4095 | `	pArray = ph7_context_new_array(pCtx);` |
|        32 | 4096 | `	if( pArray == 0 ){` |
|       ! 0 | 4097 | `		ph7_result_null(pCtx);` |
|       ! 0 | 4098 | `		return PH7_OK;` |
|         - | 4099 | `	}` |
|         - | 4100 | `	/* Perform the requested operation */` |
|        32 | 4101 | `	pNode = pMap->pFirst;` |
|       104 | 4102 | `	for( n = 0 ; n < pMap->nEntry ; ++n ){` |
|        74 | 4103 | `		pObj = HashmapExtractNodeValue(pNode);` |
|        74 | 4104 | `		if( pObj ){` |
|         - | 4105 | `			/* perform the insertion */` |
|        74 | 4106 | `			ph7_array_add_elem(pArray,0/* Automatic index assign */,pObj);` |
|        36 | 4107 | `		}` |
|         - | 4108 | `		/* Point to the next entry */` |
|        74 | 4109 | `		pNode = pNode->pPrev; /* Reverse link */` |
|        38 | 4110 | `	}` |
|         - | 4111 | `	/* return the new array */` |
|        32 | 4112 | `	ph7_result_value(pCtx,pArray);` |
|        32 | 4113 | `	return PH7_OK;` |
|        22 | 4114 | `}` |
|         - | 4115 | `/*` |
|         - | 4116 | ` * array array_keys(array $input [, val $search_value [, bool $strict = false ]] )` |
|         - | 4117 | ` *  Return all the keys or a subset of the keys of an array.` |
|         - | 4118 | ` * Parameters` |
|         - | 4119 | ` *  $input` |
|         - | 4120 | ` *   An array containing keys to return.` |
|         - | 4121 | ` * $search_value` |
|         - | 4122 | ` *   If specified, then only keys containing these values are returned.` |
|         - | 4123 | ` * $strict` |
|         - | 4124 | ` *   Determines if strict comparison (===) should be used during the search.` |
|         - | 4125 | ` * Return` |
|         - | 4126 | ` *  An array of all the keys in input or NULL on failure.` |
|         - | 4127 | ` */` |
|       142 | 4128 | `static int ph7_hashmap_keys(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|         5 | 4129 | `{` |
|         - | 4130 | `	ph7_hashmap_node *pNode;` |
|         - | 4131 | `	ph7_hashmap *pMap;` |
|         - | 4132 | `	ph7_value *pArray;` |
|         - | 4133 | `	ph7_value sObj;` |
|         - | 4134 | `	ph7_value sVal;` |
|         - | 4135 | `	SyString sKey;` |
|         - | 4136 | `	int bStrict;` |
|         - | 4137 | `	sxi32 rc;` |
|         - | 4138 | `	sxu32 n;` |
|       147 | 4139 | `	if( nArg < 1 ){` |
|         - | 4140 | `		/* Missing argument,throw ArgumentCountError */` |
|         3 | 4141 | `		return PH7_VmThrowException(pCtx,` |
|         - | 4142 | `			"ArgumentCountError",` |
|         - | 4143 | `			"array_keys() expects at least 1 argument, 0 given"` |
|         - | 4144 | `			);` |
|         - | 4145 | `	}` |
|         - | 4146 | `	/* Make sure we are dealing with a valid hashmap */` |
|       144 | 4147 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|         - | 4148 | `		/* haystack must be an array,throw TypeError */` |
|         4 | 4149 | `		return PH7_VmThrowException(pCtx,` |
|         - | 4150 | `			"TypeError",` |
|         - | 4151 | `			"array_keys(): Argument #1 ($array) must be of type array, %s given",` |
|         1 | 4152 | `			ph7_type_name(apArg[0])` |
|         - | 4153 | `			);` |
|         - | 4154 | `	}` |
|         - | 4155 | `	/* Point to the internal representation of the input hashmap */` |
|       142 | 4156 | `	pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|         - | 4157 | `	/* Create a new array */` |
|       142 | 4158 | `	pArray = ph7_context_new_array(pCtx);` |
|       142 | 4159 | `	if( pArray == 0 ){` |
|       ! 0 | 4160 | `		ph7_result_null(pCtx);` |
|       ! 0 | 4161 | `		return PH7_OK;` |
|         - | 4162 | `	}` |
|       142 | 4163 | `	bStrict = FALSE;` |
|       142 | 4164 | `	if( nArg > 2 ){` |
|         - | 4165 | `		/* In PHP, non-scalar values for a bool-hinted parameter raise TypeError */` |
|         8 | 4166 | `		if( ph7_value_is_array(apArg[2]) \|\| ph7_value_is_object(apArg[2]) \|\| ph7_value_is_resource(apArg[2]) ){` |
|         4 | 4167 | `			return PH7_VmThrowException(pCtx,` |
|         - | 4168 | `				"TypeError",` |
|         - | 4169 | `				"array_keys(): Argument #3 ($strict) must be of type bool, %s given",` |
|         2 | 4170 | `				ph7_type_name(apArg[2])` |
|         - | 4171 | `				);` |
|         - | 4172 | `		}` |
|         5 | 4173 | `		bStrict = ph7_value_to_bool(apArg[2]);` |
|         2 | 4174 | `	}` |
|         - | 4175 | `	/* Perform the requested operation */` |
|       139 | 4176 | `	pNode = pMap->pFirst;` |
|       139 | 4177 | `	PH7_MemObjInit(pMap->pVm,&sVal);` |
|      1369 | 4178 | `	for( n = 0 ; n < pMap->nEntry ; ++n ){` |
|      1233 | 4179 | `		if( pNode->iType == HASHMAP_INT_NODE ){` |
|       129 | 4180 | `			PH7_MemObjInitFromInt(pMap->pVm,&sObj,pNode->xKey.iKey);` |
|        66 | 4181 | `		}else{` |
|      1106 | 4182 | `			SyStringInitFromBuf(&sKey,SyBlobData(&pNode->xKey.sKey),SyBlobLength(&pNode->xKey.sKey));` |
|      1106 | 4183 | `			PH7_MemObjInitFromString(pMap->pVm,&sObj,&sKey);` |
|         - | 4184 | `		}` |
|      1233 | 4185 | `		rc = 0;` |
|      1233 | 4186 | `		if( nArg > 1 ){` |
|        31 | 4187 | `			ph7_value *pValue = HashmapExtractNodeValue(pNode);` |
|        31 | 4188 | `			if( pValue ){` |
|        31 | 4189 | `				PH7_MemObjLoad(pValue,&sVal);` |
|         - | 4190 | `				/* Filter key */` |
|        31 | 4191 | `				rc = ph7_value_compare(&sVal,apArg[1],bStrict);` |
|        31 | 4192 | `				PH7_MemObjRelease(&sVal);` |
|        15 | 4193 | `			}` |
|        15 | 4194 | `		}` |
|      1233 | 4195 | `		if( rc == 0 ){` |
|         - | 4196 | `			/* Perform the insertion */` |
|      1215 | 4197 | `			ph7_array_add_elem(pArray,0,&sObj);` |
|       606 | 4198 | `		}` |
|      1233 | 4199 | `		PH7_MemObjRelease(&sObj);` |
|         - | 4200 | `		/* Point to the next entry */` |
|      1233 | 4201 | `		pNode = pNode->pPrev; /* Reverse link */` |
|       618 | 4202 | `	}` |
|         - | 4203 | `	/* return the new array */` |
|       139 | 4204 | `	ph7_result_value(pCtx,pArray);` |
|       139 | 4205 | `	return PH7_OK;` |
|        76 | 4206 | `}` |
|         - | 4207 | `/*` |
|         - | 4208 | ` * bool array_same(array $arr1,array $arr2)` |
|         - | 4209 | ` *  Return TRUE if the given arrays are the same instance.` |
|         - | 4210 | ` *  This function is useful under PH7 since arrays are passed` |
|         - | 4211 | ` *  by reference unlike the zend engine which use pass by values.` |
|         - | 4212 | ` * Parameters` |
|         - | 4213 | ` *  $arr1` |
|         - | 4214 | ` *   First array` |
|         - | 4215 | ` *  $arr2` |
|         - | 4216 | ` *   Second array` |
|         - | 4217 | ` * Return` |
|         - | 4218 | ` *  TRUE if the arrays are the same instance.FALSE otherwise.` |
|         - | 4219 | ` * Note` |
|         - | 4220 | ` *  This function is a symisc eXtension.` |
|         - | 4221 | ` */` |
|         4 | 4222 | `static int ph7_hashmap_same(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|         1 | 4223 | `{` |
|         - | 4224 | `	ph7_hashmap *p1,*p2;` |
|         - | 4225 | `	int rc;` |
|         5 | 4226 | `	if( nArg < 2 \|\| !ph7_value_is_array(apArg[0]) \|\| !ph7_value_is_array(apArg[1]) ){` |
|         - | 4227 | `		/* Missing or invalid arguments,return FALSE*/` |
|       ! 0 | 4228 | `		ph7_result_bool(pCtx,0);` |
|       ! 0 | 4229 | `		return PH7_OK;` |
|         - | 4230 | `	}` |
|         - | 4231 | `	/* Point to the hashmaps */` |
|         5 | 4232 | `	p1 = (ph7_hashmap *)apArg[0]->x.pOther;` |
|         5 | 4233 | `	p2 = (ph7_hashmap *)apArg[1]->x.pOther;` |
|         5 | 4234 | `	rc = (p1 == p2);` |
|         - | 4235 | `	/* Same instance? */` |
|         5 | 4236 | `	ph7_result_bool(pCtx,rc);` |
|         5 | 4237 | `	return PH7_OK;` |
|         3 | 4238 | `}` |
|         - | 4239 | `/*` |
|         - | 4240 | ` * array array_merge(array ...$arrays)` |
|         - | 4241 | ` *  Merge one or more arrays.` |
|         - | 4242 | ` * Parameters` |
|         - | 4243 | ` *  ...$arrays` |
|         - | 4244 | ` *   Variable list of arrays to merge. Each argument must be an array;` |
|         - | 4245 | ` *   passing a non-array argument throws a TypeError.` |
|         - | 4246 | ` * Return` |
|         - | 4247 | ` *  The resulting merged array. Returns an empty array when called` |
|         - | 4248 | ` *  with no arguments.` |
|         - | 4249 | ` */` |
|      1038 | 4250 | `static int ph7_hashmap_merge(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|         5 | 4251 | `{` |
|         - | 4252 | `	ph7_hashmap *pMap,*pSrc;` |
|         - | 4253 | `	ph7_value *pArray;` |
|         - | 4254 | `	int i;` |
|         - | 4255 | `	/* Create a new array */` |
|      1043 | 4256 | `	pArray = ph7_context_new_array(pCtx);` |
|      1043 | 4257 | `	if( pArray == 0 ){` |
|       ! 0 | 4258 | `		ph7_result_null(pCtx);` |
|       ! 0 | 4259 | `		return PH7_OK;` |
|         - | 4260 | `	}` |
|         - | 4261 | `	/* Point to the internal representation of the hashmap */` |
|      1043 | 4262 | `	pMap = (ph7_hashmap *)pArray->x.pOther;` |
|         - | 4263 | `	/* Start merging */` |
|      3109 | 4264 | `	for( i = 0 ; i < nArg ; i++ ){` |
|         - | 4265 | `		/* Make sure we are dealing with a valid hashmap */` |
|      2075 | 4266 | `		if( !ph7_value_is_array(apArg[i]) ){` |
|         - | 4267 | `			/* Type mismatch -> TypeError */` |
|         8 | 4268 | `			return PH7_VmThrowException(pCtx,` |
|         - | 4269 | `				"TypeError",` |
|         - | 4270 | `				"array_merge(): Argument #%d must be of type array, %s given",` |
|         2 | 4271 | `				i + 1,` |
|         4 | 4272 | `				ph7_type_name(apArg[i])` |
|         - | 4273 | `				);` |
|       ! 0 | 4274 | `		}else{` |
|      2071 | 4275 | `			pSrc = (ph7_hashmap *)apArg[i]->x.pOther;` |
|         - | 4276 | `			/* Merge the two hashmaps */` |
|      2071 | 4277 | `			HashmapMerge(pSrc,pMap);` |
|         - | 4278 | `		}` |
|      1038 | 4279 | `	}` |
|         - | 4280 | `	/* Return the freshly created array */` |
|      1039 | 4281 | `	ph7_result_value(pCtx,pArray);` |
|      1039 | 4282 | `	return PH7_OK;` |
|       524 | 4283 | `}` |
|         - | 4284 | `/*` |
|         - | 4285 | ` * array array_copy(array $source)` |
|         - | 4286 | ` *  Make a blind copy of the target array.` |
|         - | 4287 | ` * Parameters` |
|         - | 4288 | ` *  $source` |
|         - | 4289 | ` *   Target array` |
|         - | 4290 | ` * Return` |
|         - | 4291 | ` *  Copy of the target array on success.NULL otherwise.` |
|         - | 4292 | ` * Note` |
|         - | 4293 | ` *  This function is a symisc eXtension.` |
|         - | 4294 | ` */` |
|        16 | 4295 | `static int ph7_hashmap_copy(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|         1 | 4296 | `{` |
|         - | 4297 | `	ph7_hashmap *pMap;` |
|         - | 4298 | `	ph7_value *pArray;` |
|        17 | 4299 | `	if( nArg < 1 ){` |
|         - | 4300 | `		/* Missing arguments,return NULL */` |
|       ! 0 | 4301 | `		ph7_result_null(pCtx);` |
|       ! 0 | 4302 | `		return PH7_OK;` |
|         - | 4303 | `	}` |
|         - | 4304 | `	/* Create a new array */` |
|        17 | 4305 | `	pArray = ph7_context_new_array(pCtx);` |
|        17 | 4306 | `	if( pArray == 0 ){` |
|       ! 0 | 4307 | `		ph7_result_null(pCtx);` |
|       ! 0 | 4308 | `		return PH7_OK;` |
|         - | 4309 | `	}` |
|         - | 4310 | `	/* Point to the internal representation of the hashmap */` |
|        17 | 4311 | `	pMap = (ph7_hashmap *)pArray->x.pOther;` |
|        17 | 4312 | `	if( ph7_value_is_array(apArg[0])){` |
|         - | 4313 | `		/* Point to the internal representation of the source */` |
|        17 | 4314 | `		ph7_hashmap *pSrc = (ph7_hashmap *)apArg[0]->x.pOther;` |
|         - | 4315 | `		/* Perform the copy */` |
|        17 | 4316 | `		PH7_HashmapDup(pSrc,pMap);` |
|         9 | 4317 | `	}else{` |
|         - | 4318 | `		/* Simple insertion */` |
|       ! 0 | 4319 | `		PH7_HashmapInsert(pMap,0/* Automatic index assign*/,apArg[0]);` |
|         - | 4320 | `	}` |
|         - | 4321 | `	/* Return the duplicated array */` |
|        17 | 4322 | `	ph7_result_value(pCtx,pArray);` |
|        17 | 4323 | `	return PH7_OK;` |
|         9 | 4324 | `}` |
|         - | 4325 | `/*` |
|         - | 4326 | ` * bool array_erase(array $source)` |
|         - | 4327 | ` *  Remove all elements from a given array.` |
|         - | 4328 | ` * Parameters` |
|         - | 4329 | ` *  $source` |
|         - | 4330 | ` *   Target array` |
|         - | 4331 | ` * Return` |
|         - | 4332 | ` *  TRUE on success.FALSE otherwise.` |
|         - | 4333 | ` * Note` |
|         - | 4334 | ` *  This function is a symisc eXtension.` |
|         - | 4335 | ` */` |
|        16 | 4336 | `static int ph7_hashmap_erase(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|         1 | 4337 | `{` |
|         - | 4338 | `	ph7_hashmap *pMap;` |
|        17 | 4339 | `	if( nArg < 1 ){` |
|         - | 4340 | `		/* Missing arguments */` |
|       ! 0 | 4341 | `		ph7_result_bool(pCtx,0);` |
|       ! 0 | 4342 | `		return PH7_OK;` |
|         - | 4343 | `	}` |
|         - | 4344 | `	/* Point to the target hashmap */` |
|        17 | 4345 | `	PH7_HashmapCowSeparate(pCtx->pVm, apArg[0]);` |
|        17 | 4346 | `	pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|         - | 4347 | `	/* Erase */` |
|        17 | 4348 | `	PH7_HashmapRelease(pMap,FALSE);` |
|        17 | 4349 | `	return PH7_OK;` |
|         9 | 4350 | `}` |
|         - | 4351 | `/*` |
|         - | 4352 | ` * array array_slice(array $array, int $offset [, ?int $length = null [, bool $preserve_keys = false ]])` |
|         - | 4353 | ` *  Extract a slice of the array.` |
|         - | 4354 | ` * Parameters` |
|         - | 4355 | ` *  $array` |
|         - | 4356 | ` *    The input array.` |
|         - | 4357 | ` * $offset` |
|         - | 4358 | ` *    If offset is non-negative, the sequence will start at that offset in the array.` |
|         - | 4359 | ` *    If offset is negative, the sequence will start that far from the end of the array.` |
|         - | 4360 | ` * $length (optional, nullable)` |
|         - | 4361 | ` *    If length is given and is positive, then the sequence will have that many elements` |
|         - | 4362 | ` *    in it. If length is given and is negative then the sequence will stop that many` |
|         - | 4363 | ` *    elements from the end of the array. If it is omitted or NULL, then the sequence` |
|         - | 4364 | ` *    will have everything from offset up until the end of the array.` |
|         - | 4365 | ` * $preserve_keys (optional)` |
|         - | 4366 | ` *    Note that array_slice() will reorder and reset the array indices by default.` |
|         - | 4367 | ` *    You can change this behaviour by setting preserve_keys to TRUE.` |
|         - | 4368 | ` * Return` |
|         - | 4369 | ` *   The new slice.` |
|         - | 4370 | ` */` |
|        50 | 4371 | `static int ph7_hashmap_slice(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|         5 | 4372 | `{` |
|         - | 4373 | `	ph7_hashmap *pMap,*pSrc;` |
|         - | 4374 | `	ph7_hashmap_node *pCur;` |
|         - | 4375 | `	ph7_value *pArray;` |
|         - | 4376 | `	int iLength,iOfft;` |
|         - | 4377 | `	int bPreserve;` |
|         - | 4378 | `	sxi32 rc;` |
|        55 | 4379 | `	if( nArg < 2 ){` |
|         8 | 4380 | `		return PH7_VmThrowException(pCtx,` |
|         - | 4381 | `			"ArgumentCountError",` |
|         - | 4382 | `			"array_slice() expects at least 2 arguments, %d given",` |
|         2 | 4383 | `			nArg` |
|         - | 4384 | `			);` |
|         - | 4385 | `	}` |
|        51 | 4386 | `	if( nArg > 4 ){` |
|         4 | 4387 | `		return PH7_VmThrowException(pCtx,` |
|         - | 4388 | `			"ArgumentCountError",` |
|         - | 4389 | `			"array_slice() expects at most 4 arguments, %d given",` |
|         1 | 4390 | `			nArg` |
|         - | 4391 | `			);` |
|         - | 4392 | `	}` |
|        49 | 4393 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|         4 | 4394 | `		return PH7_VmThrowException(pCtx,` |
|         - | 4395 | `			"TypeError",` |
|         - | 4396 | `			"array_slice(): Argument #1 ($array) must be of type array, %s given",` |
|         1 | 4397 | `			ph7_type_name(apArg[0])` |
|         - | 4398 | `			);` |
|         - | 4399 | `	}` |
|         - | 4400 | `	/* Validate $offset type: reject string, array, object, resource */` |
|        62 | 4401 | `	if( ph7_value_is_string(apArg[1]) \|\| ph7_value_is_array(apArg[1]) \|\|` |
|        65 | 4402 | `		ph7_value_is_object(apArg[1]) \|\| ph7_value_is_resource(apArg[1]) ){` |
|         4 | 4403 | `		return PH7_VmThrowException(pCtx,` |
|         - | 4404 | `			"TypeError",` |
|         - | 4405 | `			"array_slice(): Argument #2 ($offset) must be of type int, %s given",` |
|         2 | 4406 | `			ph7_type_name(apArg[1])` |
|         - | 4407 | `			);` |
|         - | 4408 | `	}` |
|         - | 4409 | `	/* Validate $length type if provided: nullable int */` |
|        45 | 4410 | `	if( nArg > 2 && !ph7_value_is_null(apArg[2]) ){` |
|        26 | 4411 | `		if( ph7_value_is_string(apArg[2]) \|\| ph7_value_is_array(apArg[2]) \|\|` |
|        26 | 4412 | `			ph7_value_is_object(apArg[2]) \|\| ph7_value_is_resource(apArg[2]) ){` |
|         4 | 4413 | `			return PH7_VmThrowException(pCtx,` |
|         - | 4414 | `				"TypeError",` |
|         - | 4415 | `				"array_slice(): Argument #3 ($length) must be of type ?int, %s given",` |
|         2 | 4416 | `				ph7_type_name(apArg[2])` |
|         - | 4417 | `				);` |
|         - | 4418 | `		}` |
|         8 | 4419 | `	}` |
|         - | 4420 | `	/* Validate $preserve_keys type if provided: reject array, object, resource */` |
|        43 | 4421 | `	if( nArg > 3 ){` |
|        10 | 4422 | `		if( ph7_value_is_array(apArg[3]) \|\| ph7_value_is_object(apArg[3]) \|\|` |
|         4 | 4423 | `			ph7_value_is_resource(apArg[3]) ){` |
|         4 | 4424 | `			return PH7_VmThrowException(pCtx,` |
|         - | 4425 | `				"TypeError",` |
|         - | 4426 | `				"array_slice(): Argument #4 ($preserve_keys) must be of type bool, %s given",` |
|         2 | 4427 | `				ph7_type_name(apArg[3])` |
|         - | 4428 | `				);` |
|         - | 4429 | `		}` |
|         2 | 4430 | `	}` |
|         - | 4431 | `	/* Point the internal representation of the target array */` |
|        41 | 4432 | `	pSrc = (ph7_hashmap *)apArg[0]->x.pOther;` |
|        41 | 4433 | `	bPreserve = FALSE;` |
|         - | 4434 | `	/* Get the offset */` |
|        41 | 4435 | `	iOfft = ph7_value_to_int(apArg[1]);` |
|        41 | 4436 | `	if( iOfft < 0 ){` |
|         5 | 4437 | `		iOfft = (int)pSrc->nEntry + iOfft;` |
|         5 | 4438 | `		if( iOfft < 0 ){` |
|         3 | 4439 | `			iOfft = 0;` |
|         1 | 4440 | `		}` |
|         2 | 4441 | `	}` |
|        41 | 4442 | `	if( iOfft >= (int)pSrc->nEntry ){` |
|         - | 4443 | `		/* Offset past end of array, return empty array */` |
|         5 | 4444 | `		pArray = ph7_context_new_array(pCtx);` |
|         5 | 4445 | `		if( pArray == 0 ){` |
|       ! 0 | 4446 | `			ph7_result_null(pCtx);` |
|       ! 0 | 4447 | `			return PH7_OK;` |
|         - | 4448 | `		}` |
|         5 | 4449 | `		ph7_result_value(pCtx,pArray);` |
|         5 | 4450 | `		return PH7_OK;` |
|         - | 4451 | `	}` |
|         - | 4452 | `	/* Get the length: NULL means "all remaining" (same as omitting) */` |
|        37 | 4453 | `	iLength = (int)pSrc->nEntry - iOfft;` |
|        37 | 4454 | `	if( nArg > 2 && !ph7_value_is_null(apArg[2]) ){` |
|        15 | 4455 | `		iLength = ph7_value_to_int(apArg[2]);` |
|        15 | 4456 | `		if( iLength < 0 ){` |
|         5 | 4457 | `			iLength = ((int)pSrc->nEntry + iLength) - iOfft;` |
|         2 | 4458 | `		}` |
|        15 | 4459 | `		if( iLength < 0 ){` |
|         3 | 4460 | `			iLength = 0;` |
|         1 | 4461 | `		}` |
|        15 | 4462 | `		if( iOfft + iLength > (int)pSrc->nEntry ){` |
|         3 | 4463 | `			iLength = (int)pSrc->nEntry - iOfft;` |
|         1 | 4464 | `		}` |
|         7 | 4465 | `	}` |
|        37 | 4466 | `	if( nArg > 3 ){` |
|         5 | 4467 | `		bPreserve = ph7_value_to_bool(apArg[3]);` |
|         2 | 4468 | `	}` |
|         - | 4469 | `	/* Create a new array */` |
|        37 | 4470 | `	pArray = ph7_context_new_array(pCtx);` |
|        37 | 4471 | `	if( pArray == 0 ){` |
|       ! 0 | 4472 | `		ph7_result_null(pCtx);` |
|       ! 0 | 4473 | `		return PH7_OK;` |
|         - | 4474 | `	}` |
|        37 | 4475 | `	if( iLength < 1 ){` |
|         - | 4476 | `		/* Don't bother processing,return the empty array */` |
|         5 | 4477 | `		ph7_result_value(pCtx,pArray);` |
|         5 | 4478 | `		return PH7_OK;` |
|         - | 4479 | `	}` |
|         - | 4480 | `	/* Point to the desired entry */` |
|        33 | 4481 | `	pCur = pSrc->pFirst;` |
|        28 | 4482 | `	for(;;){` |
|        61 | 4483 | `		if( iOfft < 1 ){` |
|        33 | 4484 | `			break;` |
|         - | 4485 | `		}` |
|         - | 4486 | `		/* Point to the next entry */` |
|        33 | 4487 | `		pCur = pCur->pPrev; /* Reverse link */` |
|        33 | 4488 | `		iOfft--;` |
|         5 | 4489 | `	}` |
|         - | 4490 | `	/* Point to the internal representation of the hashmap */` |
|        33 | 4491 | `	pMap = (ph7_hashmap *)pArray->x.pOther;` |
|        51 | 4492 | `	for(;;){` |
|       107 | 4493 | `		if( iLength < 1 ){` |
|        33 | 4494 | `			break;` |
|         - | 4495 | `		}` |
|         - | 4496 | `		/* String keys are always preserved; preserve_keys only affects int keys */` |
|         - | 4497 | `		{` |
|        79 | 4498 | `			int bKeep = (pCur->iType == HASHMAP_INT_NODE) ? bPreserve : TRUE;` |
|        79 | 4499 | `			rc = HashmapInsertNode(pMap,pCur,bKeep);` |
|         - | 4500 | `		}` |
|        79 | 4501 | `		if( rc != SXRET_OK ){` |
|       ! 0 | 4502 | `			break;` |
|         - | 4503 | `		}` |
|         - | 4504 | `		/* Point to the next entry */` |
|        79 | 4505 | `		pCur = pCur->pPrev; /* Reverse link */` |
|        79 | 4506 | `		iLength--;` |
|         5 | 4507 | `	}` |
|         - | 4508 | `	/* Return the freshly created array */` |
|        33 | 4509 | `	ph7_result_value(pCtx,pArray);` |
|        33 | 4510 | `	return PH7_OK;` |
|        30 | 4511 | `}` |
|         - | 4512 | `/*` |
|         - | 4513 | ` * Move the last node in the hashmap linked list to immediately after pAfter` |
|         - | 4514 | ` * in iteration order.  If pAfter is NULL the node is moved to the very` |
|         - | 4515 | ` * beginning (becomes the new pFirst).` |
|         - | 4516 | ` */` |
|        30 | 4517 | `static void HashmapMoveLastAfter(ph7_hashmap *pMap,ph7_hashmap_node *pAfter)` |
|         1 | 4518 | `{` |
|         - | 4519 | `	ph7_hashmap_node *pNode;` |
|         - | 4520 | `	ph7_hashmap_node *pOldNext;` |
|        31 | 4521 | `	pNode = pMap->pLast;` |
|        31 | 4522 | `	if( pNode == 0 ){` |
|       ! 0 | 4523 | `		return;` |
|         - | 4524 | `	}` |
|        31 | 4525 | `	if( pNode->pNext == 0 ){` |
|         - | 4526 | `		/* Only node in the list, nothing to move */` |
|         5 | 4527 | `		return;` |
|         - | 4528 | `	}` |
|        27 | 4529 | `	if( pAfter != 0 && pAfter->pPrev == pNode ){` |
|         - | 4530 | `		/* Already in the correct position */` |
|         9 | 4531 | `		return;` |
|         - | 4532 | `	}` |
|         - | 4533 | `	/* Unlink pNode from the end of the list */` |
|        19 | 4534 | `	pMap->pLast = pNode->pNext;` |
|        19 | 4535 | `	pMap->pLast->pPrev = 0;` |
|         - | 4536 | `	/* Insert pNode after pAfter in iteration order */` |
|        19 | 4537 | `	if( pAfter == 0 ){` |
|         - | 4538 | `		/* Insert at the very beginning, before pFirst */` |
|         3 | 4539 | `		pNode->pNext = 0;` |
|         3 | 4540 | `		pNode->pPrev = pMap->pFirst;` |
|         3 | 4541 | `		if( pMap->pFirst ){` |
|         3 | 4542 | `			pMap->pFirst->pNext = pNode;` |
|         1 | 4543 | `		}` |
|         3 | 4544 | `		pMap->pFirst = pNode;` |
|         2 | 4545 | `	}else{` |
|        17 | 4546 | `		pOldNext = pAfter->pPrev;` |
|        17 | 4547 | `		pNode->pPrev = pOldNext;` |
|        17 | 4548 | `		pNode->pNext = pAfter;` |
|        17 | 4549 | `		pAfter->pPrev = pNode;` |
|        17 | 4550 | `		if( pOldNext ){` |
|        17 | 4551 | `			pOldNext->pNext = pNode;` |
|         9 | 4552 | `		}else{` |
|       ! 0 | 4553 | `			pMap->pLast = pNode;` |
|         - | 4554 | `		}` |
|         - | 4555 | `	}` |
|        16 | 4556 | `}` |
|         - | 4557 | `/*` |
|         - | 4558 | ` * array array_splice(array $array, int $offset [, int $length [, value $replacement]])` |
|         - | 4559 | ` *  Remove a portion of the array and replace it with something else.` |
|         - | 4560 | ` * Parameters` |
|         - | 4561 | ` *  $array` |
|         - | 4562 | ` *    The input array.` |
|         - | 4563 | ` *  $offset` |
|         - | 4564 | ` *    If offset is positive then the start of removed portion is at that offset` |
|         - | 4565 | ` *    from the beginning of the input array.  If offset is negative then it` |
|         - | 4566 | ` *    starts that far from the end of the input array.  If the absolute value of` |
|         - | 4567 | ` *    a negative offset exceeds the array length, offset is clamped to 0.  If a` |
|         - | 4568 | ` *    positive offset exceeds the array length, offset is clamped to the array` |
|         - | 4569 | ` *    length (i.e. nothing is removed, but replacement is appended).` |
|         - | 4570 | ` *  $length (optional)` |
|         - | 4571 | ` *    If length is omitted, removes everything from offset to the end of the` |
|         - | 4572 | ` *    array.  If length is specified and is positive, then that many elements` |
|         - | 4573 | ` *    will be removed.  If length is specified and is negative then the end of` |
|         - | 4574 | ` *    the removed portion will be that many elements from the end of the array.` |
|         - | 4575 | ` *    If the resulting length is negative it is clamped to 0.` |
|         - | 4576 | ` *  $replacement (optional)` |
|         - | 4577 | ` *    If replacement array is specified, then the removed elements are replaced` |
|         - | 4578 | ` *    with elements from this array.` |
|         - | 4579 | ` *    If offset and length are such that nothing is removed, then the elements` |
|         - | 4580 | ` *    from the replacement array are inserted in the place specified by the` |
|         - | 4581 | ` *    offset.` |
|         - | 4582 | ` *    Note that keys in replacement array are not preserved.` |
|         - | 4583 | ` *    If replacement is just one element it is not necessary to put array()` |
|         - | 4584 | ` *    around it, unless the element is an array itself, an object or NULL.` |
|         - | 4585 | ` * Return` |
|         - | 4586 | ` *   A new array consisting of the extracted elements.` |
|         - | 4587 | ` */` |
|        54 | 4588 | `static int ph7_hashmap_splice(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|         4 | 4589 | `{` |
|         - | 4590 | `	ph7_hashmap_node *pCur,*pPrev,*pRnode,*pInsertAfter,*pNewNode;` |
|         - | 4591 | `	ph7_value *pArray,*pRvalue;` |
|         - | 4592 | `	ph7_hashmap *pMap,*pSrc,*pRep;` |
|         - | 4593 | `	int iLength,iOfft,i;` |
|         - | 4594 | `	sxi32 rc;` |
|        58 | 4595 | `	if( nArg < 2 ){` |
|         8 | 4596 | `		return PH7_VmThrowException(pCtx,` |
|         - | 4597 | `			"ArgumentCountError",` |
|         - | 4598 | `			"array_splice() expects at least 2 arguments, %d given",` |
|         2 | 4599 | `			nArg` |
|         - | 4600 | `			);` |
|         - | 4601 | `	}` |
|        52 | 4602 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|         4 | 4603 | `		return PH7_VmThrowException(pCtx,` |
|         - | 4604 | `			"TypeError",` |
|         - | 4605 | `			"array_splice(): Argument #1 ($array) must be of type array, %s given",` |
|         1 | 4606 | `			ph7_type_name(apArg[0])` |
|         - | 4607 | `			);` |
|         - | 4608 | `	}` |
|         - | 4609 | `	/* Point to the internal representation of the target array */` |
|        49 | 4610 | `	PH7_HashmapCowSeparate(pCtx->pVm, apArg[0]);` |
|        49 | 4611 | `	pSrc = (ph7_hashmap *)apArg[0]->x.pOther;` |
|         - | 4612 | `	/* Get the offset and clamp to valid range */` |
|        49 | 4613 | `	iOfft = ph7_value_to_int(apArg[1]);` |
|        49 | 4614 | `	if( iOfft < 0 ){` |
|         7 | 4615 | `		iOfft = (int)pSrc->nEntry + iOfft;` |
|         7 | 4616 | `		if( iOfft < 0 ){` |
|         3 | 4617 | `			iOfft = 0;` |
|         2 | 4618 | `		}` |
|        46 | 4619 | `	}else if( iOfft > (int)pSrc->nEntry ){` |
|         3 | 4620 | `		iOfft = (int)pSrc->nEntry;` |
|         1 | 4621 | `	}` |
|         - | 4622 | `	/* Get the length and clamp to valid range.` |
|         - | 4623 | `	 * NULL means "all remaining" (same as omitting the argument). */` |
|        49 | 4624 | `	iLength = (int)pSrc->nEntry - iOfft;` |
|        49 | 4625 | `	if( nArg > 2 && !ph7_value_is_null(apArg[2]) ){` |
|        31 | 4626 | `		iLength = ph7_value_to_int(apArg[2]);` |
|        31 | 4627 | `		if( iLength < 0 ){` |
|         7 | 4628 | `			iLength = ((int)pSrc->nEntry + iLength) - iOfft;` |
|         7 | 4629 | `			if( iLength < 0 ){` |
|         3 | 4630 | `				iLength = 0;` |
|         1 | 4631 | `			}` |
|         3 | 4632 | `		}` |
|        31 | 4633 | `		if( iOfft + iLength > (int)pSrc->nEntry ){` |
|         3 | 4634 | `			iLength = (int)pSrc->nEntry - iOfft;` |
|         1 | 4635 | `		}` |
|        15 | 4636 | `	}` |
|         - | 4637 | `	/* Create the result array for removed elements */` |
|        49 | 4638 | `	pArray = ph7_context_new_array(pCtx);` |
|        49 | 4639 | `	if( pArray == 0 ){` |
|       ! 0 | 4640 | `		ph7_result_null(pCtx);` |
|       ! 0 | 4641 | `		return PH7_OK;` |
|         - | 4642 | `	}` |
|         - | 4643 | `	/* Get replacement array if provided */` |
|        49 | 4644 | `	pRep = 0;` |
|        49 | 4645 | `	if( nArg > 3 ){` |
|        21 | 4646 | `		if( !ph7_value_is_array(apArg[3]) ){` |
|         - | 4647 | `			/* Perform an array cast */` |
|         3 | 4648 | `			PH7_MemObjToHashmap(apArg[3]);` |
|         3 | 4649 | `			if( ph7_value_is_array(apArg[3]) ){` |
|         3 | 4650 | `				pRep = (ph7_hashmap *)apArg[3]->x.pOther;` |
|         1 | 4651 | `			}` |
|         2 | 4652 | `		}else{` |
|        19 | 4653 | `			pRep = (ph7_hashmap *)apArg[3]->x.pOther;` |
|         - | 4654 | `		}` |
|        21 | 4655 | `		if( pRep ){` |
|         - | 4656 | `			/* Reset the loop cursor */` |
|        21 | 4657 | `			pRep->pCur = pRep->pFirst;` |
|        10 | 4658 | `		}` |
|        10 | 4659 | `	}` |
|         - | 4660 | `	/* Early return if nothing to remove and no replacement */` |
|        49 | 4661 | `	if( iLength < 1 && pRep == 0 ){` |
|         9 | 4662 | `		ph7_result_value(pCtx,pArray);` |
|         9 | 4663 | `		return PH7_OK;` |
|         - | 4664 | `	}` |
|         - | 4665 | `	/* Navigate to the offset position */` |
|        41 | 4666 | `	pCur = pSrc->pFirst;` |
|        85 | 4667 | `	for( i = 0 ; i < iOfft && pCur ; i++ ){` |
|        45 | 4668 | `		pCur = pCur->pPrev; /* Reverse link */` |
|        23 | 4669 | `	}` |
|         - | 4670 | `	/* Save the node just before the splice range as the insertion anchor.` |
|         - | 4671 | `	 * pCur->pNext is the backward link (previous node in iteration order).` |
|         - | 4672 | `	 * If pCur is NULL (offset == nEntry), the anchor is the last node. */` |
|        41 | 4673 | `	pInsertAfter = (pCur != 0) ? pCur->pNext : pSrc->pLast;` |
|         - | 4674 | `	/* Remove nodes in the splice range and copy them to the result array */` |
|        41 | 4675 | `	pMap = (ph7_hashmap *)pArray->x.pOther;` |
|       111 | 4676 | `	for( i = 0 ; i < iLength && pCur ; i++ ){` |
|        71 | 4677 | `		pPrev = pCur->pPrev;` |
|        71 | 4678 | `		rc = HashmapInsertNode(pMap,pCur,FALSE);` |
|        71 | 4679 | `		PH7_HashmapUnlinkNode(pCur,TRUE);` |
|        71 | 4680 | `		if( rc != SXRET_OK ){` |
|       ! 0 | 4681 | `			break;` |
|         - | 4682 | `		}` |
|        71 | 4683 | `		pCur = pPrev; /* Reverse link */` |
|        36 | 4684 | `	}` |
|         - | 4685 | `	/* Insert replacement elements at the correct position */` |
|        41 | 4686 | `	if( pRep ){` |
|         - | 4687 | `		ph7_value sSafeVal;` |
|        61 | 4688 | `		while( (pRnode = PH7_HashmapGetNextEntry(pRep)) != 0 ){` |
|        31 | 4689 | `			pRvalue = HashmapExtractNodeValue(pRnode);` |
|        31 | 4690 | `			if( pRvalue ){` |
|         - | 4691 | `				/* Make a stack copy before inserting.  HashmapInsert() may` |
|         - | 4692 | `				 * grow the VM memobj pool, which would invalidate pRvalue` |
|         - | 4693 | `				 * since it points into that same pool. */` |
|        31 | 4694 | `				sSafeVal = *pRvalue;` |
|        31 | 4695 | `				rc = HashmapInsert(pSrc,0,&sSafeVal);` |
|        31 | 4696 | `				if( rc == SXRET_OK && pSrc->pLast != 0 ){` |
|        31 | 4697 | `					pNewNode = pSrc->pLast;` |
|        31 | 4698 | `					HashmapMoveLastAfter(pSrc,pInsertAfter);` |
|        31 | 4699 | `					pInsertAfter = pNewNode;` |
|        15 | 4700 | `				}` |
|        15 | 4701 | `			}` |
|         1 | 4702 | `		}` |
|        10 | 4703 | `	}` |
|         - | 4704 | `	/* Return the freshly created array */` |
|        41 | 4705 | `	ph7_result_value(pCtx,pArray);` |
|        41 | 4706 | `	return PH7_OK;` |
|        31 | 4707 | `}` |
|         - | 4708 | `/*` |
|         - | 4709 | ` * bool in_array(value $needle,array $haystack[,bool $strict = FALSE ])` |
|         - | 4710 | ` *  Checks if a value exists in an array.` |
|         - | 4711 | ` * Parameters` |
|         - | 4712 | ` *  $needle` |
|         - | 4713 | ` *   The searched value.` |
|         - | 4714 | ` *   Note:` |
|         - | 4715 | ` *    If needle is a string, the comparison is done in a case-sensitive manner.` |
|         - | 4716 | ` * $haystack` |
|         - | 4717 | ` *  The target array.` |
|         - | 4718 | ` * $strict` |
|         - | 4719 | ` *  If the third parameter strict is set to TRUE then the in_array() function` |
|         - | 4720 | ` *  will also check the types of the needle in the haystack.` |
|         - | 4721 | ` */` |
|     32682 | 4722 | `static int ph7_hashmap_in_array(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|         5 | 4723 | `{` |
|         - | 4724 | `	ph7_value *pNeedle;` |
|         - | 4725 | `	int bStrict;` |
|         - | 4726 | `	int rc;` |
|     32687 | 4727 | `	if( nArg < 2 ){` |
|         - | 4728 | `		/* Missing argument,return FALSE */` |
|       ! 0 | 4729 | `		ph7_result_bool(pCtx,0);` |
|       ! 0 | 4730 | `		return PH7_OK;` |
|         - | 4731 | `	}` |
|     32687 | 4732 | `	pNeedle = apArg[0];` |
|     32687 | 4733 | `	bStrict = 0;` |
|     32687 | 4734 | `	if( nArg > 2 ){` |
|        41 | 4735 | `		bStrict = ph7_value_to_bool(apArg[2]);` |
|        20 | 4736 | `	}` |
|     32687 | 4737 | `	if( !ph7_value_is_array(apArg[1]) ){` |
|         - | 4738 | `		/* haystack must be an array,perform a standard comparison */` |
|       ! 0 | 4739 | `		rc = ph7_value_compare(pNeedle,apArg[1],bStrict);` |
|         - | 4740 | `		/* Set the comparison result */` |
|       ! 0 | 4741 | `		ph7_result_bool(pCtx,rc == 0);` |
|       ! 0 | 4742 | `		return PH7_OK;` |
|         - | 4743 | `	}` |
|         - | 4744 | `	/* Perform the lookup */` |
|     32687 | 4745 | `	rc = HashmapFindValue((ph7_hashmap *)apArg[1]->x.pOther,pNeedle,0,bStrict);` |
|         - | 4746 | `	/* Lookup result */` |
|     32687 | 4747 | `	ph7_result_bool(pCtx,rc == SXRET_OK);` |
|     32687 | 4748 | `	return PH7_OK;` |
|     16346 | 4749 | `}` |
|         - | 4750 | `/*` |
|         - | 4751 | ` * value array_search(value $needle,array $haystack[,bool $strict = false ])` |
|         - | 4752 | ` *  Searches the array for a given value and returns the corresponding key if successful.` |
|         - | 4753 | ` * Parameters` |
|         - | 4754 | ` * $needle` |
|         - | 4755 | ` *   The searched value.` |
|         - | 4756 | ` * $haystack` |
|         - | 4757 | ` *   The array.` |
|         - | 4758 | ` * $strict` |
|         - | 4759 | ` *  If the third parameter strict is set to TRUE then the array_search() function` |
|         - | 4760 | ` *  will search for identical elements in the haystack. This means it will also check` |
|         - | 4761 | ` *  the types of the needle in the haystack, and objects must be the same instance.` |
|         - | 4762 | ` * Return` |
|         - | 4763 | ` *  Returns the key for needle if it is found in the array, FALSE otherwise.` |
|         - | 4764 | ` */` |
|        28 | 4765 | `static int ph7_hashmap_search(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|         5 | 4766 | `{` |
|         - | 4767 | `	ph7_hashmap_node *pEntry;` |
|         - | 4768 | `	ph7_value *pVal,sNeedle;` |
|         - | 4769 | `	ph7_hashmap *pMap;` |
|         - | 4770 | `	ph7_value sVal;` |
|         - | 4771 | `	int bStrict;` |
|         - | 4772 | `	sxu32 n;` |
|         - | 4773 | `	int rc;` |
|        33 | 4774 | `	if( nArg < 2 ){` |
|         - | 4775 | `		/* Missing argument,throw ArgumentCountError */` |
|         8 | 4776 | `		return PH7_VmThrowException(pCtx,` |
|         - | 4777 | `			"ArgumentCountError",` |
|         - | 4778 | `			"array_search() expects at least 2 arguments, %d given",` |
|         2 | 4779 | `			nArg` |
|         - | 4780 | `			);` |
|         - | 4781 | `	}` |
|        27 | 4782 | `	bStrict = FALSE;` |
|        27 | 4783 | `	if( !ph7_value_is_array(apArg[1]) ){` |
|         - | 4784 | `		/* haystack must be an array,throw TypeError */` |
|         4 | 4785 | `		return PH7_VmThrowException(pCtx,` |
|         - | 4786 | `			"TypeError",` |
|         - | 4787 | `			"array_search(): Argument #2 ($haystack) must be of type array, %s given",` |
|         2 | 4788 | `			ph7_type_name(apArg[1])` |
|         - | 4789 | `			);` |
|         - | 4790 | `	}` |
|        24 | 4791 | `	if( nArg > 2 ){` |
|         - | 4792 | `		/* In PHP, non-scalar values for a bool-hinted parameter raise TypeError */` |
|        12 | 4793 | `		if( ph7_value_is_array(apArg[2]) \|\| ph7_value_is_object(apArg[2]) \|\| ph7_value_is_resource(apArg[2]) ){` |
|         4 | 4794 | `			return PH7_VmThrowException(pCtx,` |
|         - | 4795 | `				"TypeError",` |
|         - | 4796 | `				"array_search(): Argument #3 ($strict) must be of type bool, %s given",` |
|         2 | 4797 | `				ph7_type_name(apArg[2])` |
|         - | 4798 | `				);` |
|         - | 4799 | `		}` |
|         9 | 4800 | `		bStrict = ph7_value_to_bool(apArg[2]);` |
|         4 | 4801 | `	}` |
|         - | 4802 | `	/* Point to the internal representation of the internal hashmap */` |
|        21 | 4803 | `	pMap = (ph7_hashmap *)apArg[1]->x.pOther;` |
|         - | 4804 | `	/* Perform a linear search since we cannot sort the hashmap based on values */` |
|        21 | 4805 | `	PH7_MemObjInit(pMap->pVm,&sVal);` |
|        21 | 4806 | `	PH7_MemObjInit(pMap->pVm,&sNeedle);` |
|        21 | 4807 | `	pEntry = pMap->pFirst;` |
|        21 | 4808 | `	n = pMap->nEntry;` |
|        23 | 4809 | `	for(;;){` |
|        47 | 4810 | `		if( !n ){` |
|         9 | 4811 | `			break;` |
|         - | 4812 | `		}` |
|         - | 4813 | `		/* Extract node value */` |
|        39 | 4814 | `		pVal = HashmapExtractNodeValue(pEntry);` |
|        39 | 4815 | `		if( pVal ){` |
|         - | 4816 | `			/* Make a copy of the vuurent values since the comparison routine` |
|         - | 4817 | `			 * can change their type.` |
|         - | 4818 | `			 */` |
|        39 | 4819 | `			PH7_MemObjLoad(pVal,&sVal);` |
|        39 | 4820 | `			PH7_MemObjLoad(apArg[0],&sNeedle);` |
|        39 | 4821 | `			rc = PH7_MemObjCmp(&sNeedle,&sVal,bStrict,0);` |
|        39 | 4822 | `			PH7_MemObjRelease(&sVal);` |
|        39 | 4823 | `			PH7_MemObjRelease(&sNeedle);` |
|        39 | 4824 | `			if( rc == 0 ){` |
|         - | 4825 | `				/* Match found,return key */` |
|        13 | 4826 | `				if( pEntry->iType == HASHMAP_INT_NODE){` |
|         - | 4827 | `					/* INT key */` |
|         7 | 4828 | `					ph7_result_int64(pCtx,pEntry->xKey.iKey);` |
|         4 | 4829 | `				}else{` |
|         7 | 4830 | `					SyBlob *pKey = &pEntry->xKey.sKey;` |
|         - | 4831 | `					/* Blob key */` |
|         7 | 4832 | `					ph7_result_string(pCtx,(const char *)SyBlobData(pKey),(int)SyBlobLength(pKey));` |
|         - | 4833 | `				}` |
|        13 | 4834 | `				return PH7_OK;` |
|         - | 4835 | `			}` |
|        13 | 4836 | `		}` |
|         - | 4837 | `		/* Point to the next entry */` |
|        27 | 4838 | `		pEntry = pEntry->pPrev; /* Reverse link */` |
|        27 | 4839 | `		n--;` |
|         1 | 4840 | `	}` |
|         - | 4841 | `	/* No such value,return FALSE */` |
|         9 | 4842 | `	ph7_result_bool(pCtx,0);` |
|         9 | 4843 | `	return PH7_OK;` |
|        19 | 4844 | `}` |
|         - | 4845 | `/*` |
|         - | 4846 | ` * array array_diff(array $array1,array $array2,...)` |
|         - | 4847 | ` *  Computes the difference of arrays.` |
|         - | 4848 | ` * Parameters` |
|         - | 4849 | ` *  $array1` |
|         - | 4850 | ` *    The array to compare from` |
|         - | 4851 | ` *  $array2` |
|         - | 4852 | ` *    An array to compare against` |
|         - | 4853 | ` *  $...` |
|         - | 4854 | ` *   More arrays to compare against` |
|         - | 4855 | ` * Return` |
|         - | 4856 | ` *  Returns an array containing all the entries from array1 that` |
|         - | 4857 | ` *  are not present in any of the other arrays.` |
|         - | 4858 | ` */` |
|        22 | 4859 | `static int ph7_hashmap_diff(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|         4 | 4860 | `{` |
|         - | 4861 | `	ph7_hashmap_node *pEntry;` |
|         - | 4862 | `	ph7_hashmap *pSrc,*pMap;` |
|         - | 4863 | `	ph7_value *pArray;` |
|         - | 4864 | `	ph7_value *pVal;` |
|         - | 4865 | `	sxi32 rc;` |
|         - | 4866 | `	sxu32 n;` |
|         - | 4867 | `	int i;` |
|         - | 4868 | `	/* Validate arguments to mimic PHP behaviour. Earlier versions simply` |
|         - | 4869 | `	 * returned NULL when the caller passed invalid parameters which made` |
|         - | 4870 | `	 * debugging difficult. */` |
|        26 | 4871 | `	if( nArg < 1 ){` |
|         4 | 4872 | `		return PH7_VmThrowException(pCtx,` |
|         - | 4873 | `			"ArgumentCountError",` |
|         - | 4874 | `			"array_diff() expects at least 1 argument, %d given",` |
|         1 | 4875 | `			nArg` |
|         - | 4876 | `			);` |
|         - | 4877 | `	}` |
|        23 | 4878 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|         4 | 4879 | `		return PH7_VmThrowException(pCtx,` |
|         - | 4880 | `			"TypeError",` |
|         - | 4881 | `			"array_diff(): Argument #1 ($array) must be of type array, %s given",` |
|         1 | 4882 | `			ph7_type_name(apArg[0])` |
|         - | 4883 | `			);` |
|         - | 4884 | `	}` |
|        36 | 4885 | `	for(i = 1 ; i < nArg ; i++){` |
|        20 | 4886 | `		if( !ph7_value_is_array(apArg[i]) ){` |
|         4 | 4887 | `			return PH7_VmThrowException(pCtx,` |
|         - | 4888 | `				"TypeError",` |
|         - | 4889 | `				"array_diff(): Argument #%d must be of type array, %s given",` |
|         1 | 4890 | `				i + 1,` |
|         2 | 4891 | `				ph7_type_name(apArg[i])` |
|         - | 4892 | `				);` |
|         - | 4893 | `		}` |
|         9 | 4894 | `	}` |
|        17 | 4895 | `	if( nArg == 1 ){` |
|         - | 4896 | `		/* Return the first array since we cannot perform a diff */` |
|         3 | 4897 | `		ph7_result_value(pCtx,apArg[0]);` |
|         3 | 4898 | `		return PH7_OK;` |
|         - | 4899 | `	}` |
|         - | 4900 | `	/* Create a new array */` |
|        15 | 4901 | `	pArray = ph7_context_new_array(pCtx);` |
|        15 | 4902 | `	if( pArray == 0 ){` |
|       ! 0 | 4903 | `		ph7_result_null(pCtx);` |
|       ! 0 | 4904 | `		return PH7_OK;` |
|         - | 4905 | `	}` |
|         - | 4906 | `	/* Point to the internal representation of the source hashmap */` |
|        15 | 4907 | `	pSrc = (ph7_hashmap *)apArg[0]->x.pOther;` |
|         - | 4908 | `	/* Perform the diff */` |
|        15 | 4909 | `	pEntry = pSrc->pFirst;` |
|        15 | 4910 | `	n = pSrc->nEntry;` |
|        27 | 4911 | `	for(;;){` |
|        55 | 4912 | `		if( n < 1 ){` |
|        15 | 4913 | `			break;` |
|         - | 4914 | `		}` |
|         - | 4915 | `		/* Extract the node value */` |
|        41 | 4916 | `		pVal = HashmapExtractNodeValue(pEntry);` |
|        41 | 4917 | `		if( pVal ){` |
|        69 | 4918 | `			for( i = 1 ; i < nArg ; i++ ){` |
|         - | 4919 | `				/* Point to the internal representation of the hashmap */` |
|        45 | 4920 | `				pMap = (ph7_hashmap *)apArg[i]->x.pOther;` |
|         - | 4921 | `				/* Perform the lookup */` |
|        45 | 4922 | `				rc = HashmapFindValue(pMap,pVal,0,TRUE);` |
|        45 | 4923 | `				if( rc == SXRET_OK ){` |
|         - | 4924 | `					/* Value exist */` |
|        17 | 4925 | `					break;` |
|         - | 4926 | `				}` |
|        15 | 4927 | `			}` |
|        41 | 4928 | `			if( i >= nArg ){` |
|         - | 4929 | `				/* Perform the insertion */` |
|        25 | 4930 | `				HashmapInsertNode((ph7_hashmap *)pArray->x.pOther,pEntry,TRUE);` |
|        12 | 4931 | `			}` |
|        20 | 4932 | `		}` |
|         - | 4933 | `		/* Point to the next entry */` |
|        41 | 4934 | `		pEntry = pEntry->pPrev; /* Reverse link */` |
|        41 | 4935 | `		n--;` |
|         1 | 4936 | `	}` |
|         - | 4937 | `	/* Return the freshly created array */` |
|        15 | 4938 | `	ph7_result_value(pCtx,pArray);` |
|        15 | 4939 | `	return PH7_OK;` |
|        15 | 4940 | `}` |
|         - | 4941 | `/*` |
|         - | 4942 | ` * array array_udiff(array $array1,array $array2,...,$callback)` |
|         - | 4943 | ` *  Computes the difference of arrays by using a callback function for data comparison.` |
|         - | 4944 | ` * Parameters` |
|         - | 4945 | ` *  $array1` |
|         - | 4946 | ` *    The array to compare from` |
|         - | 4947 | ` *  $array2` |
|         - | 4948 | ` *    An array to compare against` |
|         - | 4949 | ` *  $...` |
|         - | 4950 | ` *   More arrays to compare against.` |
|         - | 4951 | ` * $callback` |
|         - | 4952 | ` *  The callback comparison function.` |
|         - | 4953 | ` *  The comparison function must return an integer less than, equal to, or greater than zero` |
|         - | 4954 | ` *  if the first argument is considered to be respectively less than, equal to, or greater` |
|         - | 4955 | ` *  than the second.` |
|         - | 4956 | ` *     int callback ( mixed $a, mixed $b )` |
|         - | 4957 | ` * Return` |
|         - | 4958 | ` *  Returns an array containing all the entries from array1 that` |
|         - | 4959 | ` *  are not present in any of the other arrays.` |
|         - | 4960 | ` */` |
|        22 | 4961 | `static int ph7_hashmap_udiff(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|         5 | 4962 | `{` |
|         - | 4963 | `	ph7_hashmap_node *pEntry;` |
|         - | 4964 | `	ph7_hashmap *pSrc,*pMap;` |
|         - | 4965 | `	ph7_value *pCallback;` |
|         - | 4966 | `	ph7_value *pArray;` |
|         - | 4967 | `	ph7_value *pVal;` |
|         - | 4968 | `	sxi32 rc;` |
|         - | 4969 | `	sxu32 n;` |
|         - | 4970 | `	int i;` |
|         - | 4971 |  |
|         - | 4972 | `	/* Ensure the argument count matches PHP behaviour. */` |
|        27 | 4973 | `	if( nArg < 2 ){` |
|         4 | 4974 | `		return PH7_VmThrowException(pCtx,` |
|         - | 4975 | `			"ArgumentCountError",` |
|         - | 4976 | `			"array_udiff() expects at least 2 arguments, %d given",` |
|         1 | 4977 | `			nArg` |
|         - | 4978 | `			);` |
|         - | 4979 | `	}` |
|        25 | 4980 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|         4 | 4981 | `		return PH7_VmThrowException(pCtx,` |
|         - | 4982 | `			"TypeError",` |
|         - | 4983 | `			"array_udiff(): Argument #1 ($array) must be of type array, %s given",` |
|         1 | 4984 | `			ph7_type_name(apArg[0])` |
|         - | 4985 | `			);` |
|         - | 4986 | `	}` |
|         - | 4987 |  |
|        23 | 4988 | `	if( nArg == 2 ){` |
|         - | 4989 | `		/* Only the original array and the callback were provided. */` |
|         - | 4990 | `		/* Nevertheless, we still validate the callback after verifying any` |
|         - | 4991 | `		 * intermediate array arguments to match PHP's left-to-right parameter` |
|         - | 4992 | `		 * validation order.` |
|         - | 4993 | `		 */` |
|         4 | 4994 | `	} else {` |
|         - | 4995 | `		/* Ensure intermediary arguments are arrays (matches PHP strict typing). */` |
|        27 | 4996 | `		for( i = 1 ; i < nArg - 1; i++ ){` |
|        19 | 4997 | `			if( !ph7_value_is_array(apArg[i]) ){` |
|        11 | 4998 | `				return PH7_VmThrowException(pCtx,` |
|         - | 4999 | `					"TypeError",` |
|         - | 5000 | `					"array_udiff(): Argument #%d must be of type array, %s given",` |
|         3 | 5001 | `					i + 1,` |
|         6 | 5002 | `					ph7_type_name(apArg[i])` |
|         - | 5003 | `					);` |
|         - | 5004 | `			}` |
|         7 | 5005 | `		}` |
|         - | 5006 | `	}` |
|         - | 5007 |  |
|         - | 5008 | `	/* Identify the callback (always expected as the last argument). */` |
|        16 | 5009 | `	pCallback = apArg[nArg - 1];` |
|         - | 5010 | `	/* Validate the callback to match PHP's error messages. */` |
|        16 | 5011 | `	if( !ph7_value_is_callable(pCallback) ){` |
|         9 | 5012 | `		if( ph7_value_is_array(pCallback) ){` |
|         4 | 5013 | `			return PH7_VmThrowException(pCtx,` |
|         - | 5014 | `				"TypeError",` |
|         - | 5015 | `				"array_udiff(): Argument #%d must be a valid callback, array callback must have exactly two members",` |
|         1 | 5016 | `				nArg` |
|         - | 5017 | `				);` |
|         - | 5018 | `		}` |
|         6 | 5019 | `		if( ph7_value_is_string(pCallback) ){` |
|         - | 5020 | `			int len;` |
|         3 | 5021 | `			const char *zName = ph7_value_to_string(pCallback, &len);` |
|         4 | 5022 | `			return PH7_VmThrowException(pCtx,` |
|         - | 5023 | `				"TypeError",` |
|         - | 5024 | `				"array_udiff(): Argument #%d must be a valid callback, function \"%s\" not found or invalid function name",` |
|         1 | 5025 | `				nArg,` |
|         1 | 5026 | `				zName` |
|         - | 5027 | `				);` |
|         - | 5028 | `		}` |
|         4 | 5029 | `		return PH7_VmThrowException(pCtx,` |
|         - | 5030 | `			"TypeError",` |
|         - | 5031 | `			"array_udiff(): Argument #%d must be a valid callback, no array or string given",` |
|         1 | 5032 | `			nArg` |
|         - | 5033 | `			);` |
|         - | 5034 | `	}` |
|         - | 5035 |  |
|         7 | 5036 | `	if( nArg == 2 ){` |
|         - | 5037 | `		/* Only the original array and the callback were provided. */` |
|         3 | 5038 | `		ph7_result_value(pCtx,apArg[0]);` |
|         3 | 5039 | `		return PH7_OK;` |
|         - | 5040 | `	}` |
|         - | 5041 |  |
|         - | 5042 | `	/* Create a new array */` |
|         5 | 5043 | `	pArray = ph7_context_new_array(pCtx);` |
|         5 | 5044 | `	if( pArray == 0 ){` |
|       ! 0 | 5045 | `		ph7_result_null(pCtx);` |
|       ! 0 | 5046 | `		return PH7_OK;` |
|         - | 5047 | `	}` |
|         - | 5048 | `	/* Point to the internal representation of the source hashmap */` |
|         5 | 5049 | `	pSrc = (ph7_hashmap *)apArg[0]->x.pOther;` |
|         - | 5050 | `	/* Perform the diff */` |
|         5 | 5051 | `	pEntry = pSrc->pFirst;` |
|         5 | 5052 | `	n = pSrc->nEntry;` |
|         5 | 5053 | `	pCtx->pVm->iCmpCallbackExc = 0;` |
|         5 | 5054 | `	for(;;){` |
|        11 | 5055 | `		if( n < 1 ){` |
|         3 | 5056 | `			break;` |
|         - | 5057 | `		}` |
|         - | 5058 | `		/* Extract the node value */` |
|         9 | 5059 | `		pVal = HashmapExtractNodeValue(pEntry);` |
|         9 | 5060 | `		if( pVal ){` |
|        15 | 5061 | `			for( i = 1 ; i < nArg - 1; i++ ){` |
|         - | 5062 | `				/* Point to the internal representation of the hashmap */` |
|         9 | 5063 | `				pMap = (ph7_hashmap *)apArg[i]->x.pOther;` |
|         - | 5064 | `				/* Perform the lookup */` |
|         9 | 5065 | `				rc = HashmapFindValueByCallback(pMap,pVal,pCallback,0);` |
|         9 | 5066 | `				if( rc == SXRET_OK ){` |
|         - | 5067 | `					/* Value exist */` |
|         3 | 5068 | `					break;` |
|         - | 5069 | `				}` |
|         4 | 5070 | `			}` |
|         9 | 5071 | `			if( pCtx->pVm->iCmpCallbackExc ){` |
|         - | 5072 | `				/* The comparison callback raised: propagate so the dispatcher` |
|         - | 5073 | `				 * unwinds, before any spurious insertion into the result. */` |
|         3 | 5074 | `				pCtx->pVm->iCmpCallbackExc = 0;` |
|         3 | 5075 | `				return PH7_EXCEPTION;` |
|         - | 5076 | `			}` |
|         7 | 5077 | `			if( i >= (nArg - 1)){` |
|         - | 5078 | `				/* Perform the insertion */` |
|         5 | 5079 | `				HashmapInsertNode((ph7_hashmap *)pArray->x.pOther,pEntry,TRUE);` |
|         2 | 5080 | `			}` |
|         3 | 5081 | `		}` |
|         - | 5082 | `		/* Point to the next entry */` |
|         7 | 5083 | `		pEntry = pEntry->pPrev; /* Reverse link */` |
|         7 | 5084 | `		n--;` |
|         1 | 5085 | `	}` |
|         - | 5086 | `	/* Return the freshly created array */` |
|         3 | 5087 | `	ph7_result_value(pCtx,pArray);` |
|         3 | 5088 | `	return PH7_OK;` |
|        16 | 5089 | `}` |
|         - | 5090 | `/*` |
|         - | 5091 | ` * array array_diff_assoc(array $array1,array $array2,...)` |
|         - | 5092 | ` *  Computes the difference of arrays with additional index check.` |
|         - | 5093 | ` * Parameters` |
|         - | 5094 | ` *  $array1` |
|         - | 5095 | ` *    The array to compare from` |
|         - | 5096 | ` *  $array2` |
|         - | 5097 | ` *    An array to compare against` |
|         - | 5098 | ` *  $...` |
|         - | 5099 | ` *   More arrays to compare against` |
|         - | 5100 | ` * Return` |
|         - | 5101 | ` *  Returns an array containing all the entries from array1 that` |
|         - | 5102 | ` *  are not present in any of the other arrays.` |
|         - | 5103 | ` */` |
|        20 | 5104 | `static int ph7_hashmap_diff_assoc(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|         5 | 5105 | `{` |
|         - | 5106 | `	ph7_hashmap_node *pN1,*pN2,*pEntry;` |
|         - | 5107 | `	ph7_hashmap *pSrc,*pMap;` |
|         - | 5108 | `	ph7_value *pArray;` |
|         - | 5109 | `	ph7_value *pVal;` |
|         - | 5110 | `	sxi32 rc;` |
|         - | 5111 | `	sxu32 n;` |
|         - | 5112 | `	int i;` |
|         - | 5113 | `	/* Ensure the argument list is valid, emitting the same errors PHP` |
|         - | 5114 | `	 * would produce. This makes behaviour predictable and allows the` |
|         - | 5115 | `	 * accompanying integration tests to pass. */` |
|        25 | 5116 | `	if( nArg < 1 ){` |
|         4 | 5117 | `		return PH7_VmThrowException(pCtx,` |
|         - | 5118 | `			"ArgumentCountError",` |
|         - | 5119 | `			"array_diff_assoc() expects at least 1 argument, %d given",` |
|         1 | 5120 | `			nArg` |
|         - | 5121 | `			);` |
|         - | 5122 | `	}` |
|        22 | 5123 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|         4 | 5124 | `		return PH7_VmThrowException(pCtx,` |
|         - | 5125 | `			"TypeError",` |
|         - | 5126 | `			"array_diff_assoc(): Argument #1 ($array) must be of type array, %s given",` |
|         1 | 5127 | `			ph7_type_name(apArg[0])` |
|         - | 5128 | `			);` |
|         - | 5129 | `	}` |
|        33 | 5130 | `	for(i = 1 ; i < nArg ; i++){` |
|        21 | 5131 | `		if( !ph7_value_is_array(apArg[i]) ){` |
|         8 | 5132 | `			return PH7_VmThrowException(pCtx,` |
|         - | 5133 | `				"TypeError",` |
|         - | 5134 | `				"array_diff_assoc(): Argument #%d must be of type array, %s given",` |
|         2 | 5135 | `				i + 1,` |
|         4 | 5136 | `				ph7_type_name(apArg[i])` |
|         - | 5137 | `				);` |
|         - | 5138 | `		}` |
|         9 | 5139 | `	}` |
|        13 | 5140 | `	if( nArg == 1 ){` |
|         - | 5141 | `		/* Return the first array since we cannot perform a diff */` |
|         3 | 5142 | `		ph7_result_value(pCtx,apArg[0]);` |
|         3 | 5143 | `		return PH7_OK;` |
|         - | 5144 | `	}` |
|         - | 5145 | `	/* Create a new array */` |
|        11 | 5146 | `	pArray = ph7_context_new_array(pCtx);` |
|        11 | 5147 | `	if( pArray == 0 ){` |
|       ! 0 | 5148 | `		ph7_result_null(pCtx);` |
|       ! 0 | 5149 | `		return PH7_OK;` |
|         - | 5150 | `	}` |
|         - | 5151 | `	/* Point to the internal representation of the source hashmap */` |
|        11 | 5152 | `	pSrc = (ph7_hashmap *)apArg[0]->x.pOther;` |
|         - | 5153 | `	/* Perform the diff */` |
|        11 | 5154 | `	pEntry = pSrc->pFirst;` |
|        11 | 5155 | `	n = pSrc->nEntry;` |
|        11 | 5156 | `	pN1 = pN2 = 0;` |
|        29 | 5157 | `	for(;;){` |
|         - | 5158 | `		int keep;` |
|        35 | 5159 | `		if( n < 1 ){` |
|        11 | 5160 | `			break;` |
|         - | 5161 | `		}` |
|         - | 5162 | `		/* assume the element should be kept until we find a match */` |
|        25 | 5163 | `		keep = 1;` |
|        41 | 5164 | `		for( i = 1 ; i < nArg ; i++ ){` |
|         - | 5165 | `			/* all arguments have been validated already, so cast directly */` |
|        29 | 5166 | `			pMap = (ph7_hashmap *)apArg[i]->x.pOther;` |
|         - | 5167 | `			/* Perform a key lookup first */` |
|        29 | 5168 | `			if( pEntry->iType == HASHMAP_INT_NODE ){` |
|        13 | 5169 | `				rc = HashmapLookupIntKey(pMap,pEntry->xKey.iKey,&pN1);` |
|         7 | 5170 | `			}else{` |
|        17 | 5171 | `				rc = HashmapLookupBlobKey(pMap,SyBlobData(&pEntry->xKey.sKey),SyBlobLength(&pEntry->xKey.sKey),&pN1);` |
|         - | 5172 | `			}` |
|        29 | 5173 | `			if( rc != SXRET_OK ){` |
|         - | 5174 | `				/* this array does not contain the key, continue checking others */` |
|        15 | 5175 | `				continue;` |
|         - | 5176 | `			}` |
|         - | 5177 | `			/* key exists; check that value stored in the matching node is equal */` |
|        15 | 5178 | `			pVal = HashmapExtractNodeValue(pEntry);` |
|        15 | 5179 | `			if( pVal ){` |
|         - | 5180 | `				/* directly compare with value at pN1 rather than searching again */` |
|        15 | 5181 | `				ph7_value *pVal2 = HashmapExtractNodeValue(pN1);` |
|        15 | 5182 | `				if( pVal2 ){` |
|        15 | 5183 | `					sxi32 cmp = PH7_MemObjCmp(pVal, pVal2, TRUE, 0);` |
|        15 | 5184 | `					if( cmp == 0 ){` |
|         - | 5185 | `						/* identical key+value found in one of the arrays => drop it */` |
|        13 | 5186 | `						keep = 0;` |
|        13 | 5187 | `						break;` |
|         - | 5188 | `					}` |
|         1 | 5189 | `				}` |
|         1 | 5190 | `			}` |
|         2 | 5191 | `		}` |
|        25 | 5192 | `		if( keep ){` |
|         - | 5193 | `			/* Perform the insertion */` |
|        13 | 5194 | `			HashmapInsertNode((ph7_hashmap *)pArray->x.pOther,pEntry,TRUE);` |
|         6 | 5195 | `		}` |
|         - | 5196 | `		/* Point to the next entry */` |
|        25 | 5197 | `		pEntry = pEntry->pPrev; /* Reverse link */` |
|        25 | 5198 | `		n--;` |
|         1 | 5199 | `	}` |
|         - | 5200 | `	/* Return the freshly created array */` |
|        11 | 5201 | `	ph7_result_value(pCtx,pArray);` |
|        11 | 5202 | `	return PH7_OK;` |
|        15 | 5203 | `}` |
|         - | 5204 | `/*` |
|         - | 5205 | ` * array array_diff_uassoc(array $array1,array $array2,...,callback $key_compare_func)` |
|         - | 5206 | ` *  Computes the difference of arrays with additional index check which is performed` |
|         - | 5207 | ` *  by a user supplied callback function.` |
|         - | 5208 | ` * Parameters` |
|         - | 5209 | ` *  $array1` |
|         - | 5210 | ` *    The array to compare from` |
|         - | 5211 | ` *  $array2` |
|         - | 5212 | ` *    An array to compare against` |
|         - | 5213 | ` *  $...` |
|         - | 5214 | ` *   More arrays to compare against.` |
|         - | 5215 | ` *  $key_compare_func` |
|         - | 5216 | ` *   Callback function to use. The callback function must return an integer` |
|         - | 5217 | ` *   less than, equal to, or greater than zero if the first argument is considered` |
|         - | 5218 | ` *   to be respectively less than, equal to, or greater than the second.` |
|         - | 5219 | ` * Return` |
|         - | 5220 | ` *  Returns an array containing all the entries from array1 that` |
|         - | 5221 | ` *  are not present in any of the other arrays.` |
|         - | 5222 | ` */` |
|        24 | 5223 | `static int ph7_hashmap_diff_uassoc(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|         5 | 5224 | `{` |
|         - | 5225 | `	ph7_hashmap_node *pEntry;` |
|         - | 5226 | `	ph7_hashmap *pSrc,*pMap;` |
|         - | 5227 | `	ph7_value *pCallback;` |
|         - | 5228 | `	ph7_value *pArray;` |
|         - | 5229 | `	sxi32 rc;` |
|         - | 5230 | `	sxu32 n;` |
|         - | 5231 | `	int i;` |
|         - | 5232 |  |
|         - | 5233 | `	/* Argument validation mimicking PHP errors. */` |
|        29 | 5234 | `	if( nArg < 2 ){` |
|         4 | 5235 | `		return PH7_VmThrowException(pCtx,` |
|         - | 5236 | `			"ArgumentCountError",` |
|         - | 5237 | `			"array_diff_uassoc() expects at least 2 arguments, %d given",` |
|         1 | 5238 | `			nArg` |
|         - | 5239 | `			);` |
|         - | 5240 | `	}` |
|        26 | 5241 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|         4 | 5242 | `		return PH7_VmThrowException(pCtx,` |
|         - | 5243 | `			"TypeError",` |
|         - | 5244 | `			"array_diff_uassoc(): Argument #1 ($array) must be of type array, %s given",` |
|         1 | 5245 | `			ph7_type_name(apArg[0])` |
|         - | 5246 | `			);` |
|         - | 5247 | `	}` |
|         - | 5248 | `	/* Intermediate arguments (except last) must be arrays. Last argument is` |
|         - | 5249 | `	 * expected to be a callback. */` |
|        38 | 5250 | `	for(i = 1 ; i < nArg - 1; i++){` |
|        19 | 5251 | `		if( !ph7_value_is_array(apArg[i]) ){` |
|         4 | 5252 | `			return PH7_VmThrowException(pCtx,` |
|         - | 5253 | `				"TypeError",` |
|         - | 5254 | `				"array_diff_uassoc(): Argument #%d must be of type array, %s given",` |
|         1 | 5255 | `				i + 1,` |
|         2 | 5256 | `				ph7_type_name(apArg[i])` |
|         - | 5257 | `				);` |
|         - | 5258 | `		}` |
|         9 | 5259 | `	}` |
|         - | 5260 | `	/* Point to the callback value */` |
|        22 | 5261 | `	pCallback = apArg[nArg - 1];` |
|        22 | 5262 | `	if( !ph7_value_is_callable(pCallback) ){` |
|         - | 5263 | `		/* Compose an error message that closely matches PHP output. When the` |
|         - | 5264 | `		 * argument is an array of the wrong shape we include an extra clause.` |
|         - | 5265 | `		 * If the value is neither array nor string, PHP says "no array or` |
|         - | 5266 | `		 * string given" which we also reproduce. */` |
|         9 | 5267 | `		if( ph7_value_is_array(pCallback) ){` |
|         - | 5268 | `			/* ARRAY CALLBACK must have exactly two members */` |
|         4 | 5269 | `			return PH7_VmThrowException(pCtx,` |
|         - | 5270 | `				"TypeError",` |
|         - | 5271 | `				"array_diff_uassoc(): Argument #%d must be a valid callback, array callback must have exactly two members",` |
|         1 | 5272 | `				nArg` |
|         - | 5273 | `				);` |
|         - | 5274 | `		}` |
|         6 | 5275 | `		if( !ph7_value_is_string(pCallback) ){` |
|         - | 5276 | `			/* neither array nor string */` |
|         8 | 5277 | `			return PH7_VmThrowException(pCtx,` |
|         - | 5278 | `				"TypeError",` |
|         - | 5279 | `				"array_diff_uassoc(): Argument #%d must be a valid callback, no array or string given",` |
|         2 | 5280 | `				nArg` |
|         - | 5281 | `				);` |
|         - | 5282 | `		}` |
|         - | 5283 | `		/* Fallback for string (non-callable) or other leftover cases */` |
|       ! 0 | 5284 | `		return PH7_VmThrowException(pCtx,` |
|         - | 5285 | `			"TypeError",` |
|         - | 5286 | `			"array_diff_uassoc(): Argument #%d must be a valid callback, %s given",` |
|       ! 0 | 5287 | `			nArg,` |
|       ! 0 | 5288 | `			ph7_type_name(pCallback)` |
|         - | 5289 | `			);` |
|         - | 5290 | `	}` |
|        13 | 5291 | `	if( nArg == 2 ){` |
|         - | 5292 | `		/* If we only have the first array and the callback, just return the` |
|         - | 5293 | `		 * input array. */` |
|         3 | 5294 | `		ph7_result_value(pCtx,apArg[0]);` |
|         3 | 5295 | `		return PH7_OK;` |
|         - | 5296 | `	}` |
|         - | 5297 | `	/* Create a new array */` |
|        11 | 5298 | `	pArray = ph7_context_new_array(pCtx);` |
|        11 | 5299 | `	if( pArray == 0 ){` |
|       ! 0 | 5300 | `		ph7_result_null(pCtx);` |
|       ! 0 | 5301 | `		return PH7_OK;` |
|         - | 5302 | `	}` |
|         - | 5303 | `	/* Point to the internal representation of the source hashmap */` |
|        11 | 5304 | `	pSrc = (ph7_hashmap *)apArg[0]->x.pOther;` |
|         - | 5305 | `	/* Perform the diff */` |
|        11 | 5306 | `	pEntry = pSrc->pFirst;` |
|        11 | 5307 | `	n = pSrc->nEntry;` |
|        21 | 5308 | `	for(;;){` |
|         - | 5309 | `		int keep;` |
|        27 | 5310 | `		if( n < 1 ){` |
|         9 | 5311 | `			break;` |
|         - | 5312 | `		}` |
|        19 | 5313 | `		keep = 1;` |
|        31 | 5314 | `		for( i = 1 ; i < nArg - 1; i++ ){` |
|         - | 5315 | `			/* each of these must already be arrays thanks to earlier validation */` |
|        23 | 5316 | `			pMap = (ph7_hashmap *)apArg[i]->x.pOther;` |
|         - | 5317 | `			/* we must compare keys via callback, not by direct lookup */` |
|        23 | 5318 | `			ph7_hashmap_node *pIt = pMap->pFirst;` |
|        45 | 5319 | `			while( pIt ){` |
|         - | 5320 | `				/* build temporary key values for callback */` |
|         - | 5321 | `				ph7_value key1, key2, result;` |
|         - | 5322 | `				/* initialise only once using the appropriate helper */` |
|        33 | 5323 | `				if( pEntry->iType == HASHMAP_INT_NODE ){` |
|       ! 0 | 5324 | `					PH7_MemObjInitFromInt(pMap->pVm,&key1,pEntry->xKey.iKey);` |
|       ! 0 | 5325 | `				}else{` |
|         - | 5326 | `					SyString sStr;` |
|        33 | 5327 | `					SyStringInitFromBuf(&sStr,` |
|         - | 5328 | `						SyBlobData(&pEntry->xKey.sKey),` |
|         - | 5329 | `						SyBlobLength(&pEntry->xKey.sKey));` |
|        33 | 5330 | `					PH7_MemObjInitFromString(pMap->pVm,&key1,&sStr);` |
|         - | 5331 | `				}` |
|        33 | 5332 | `				if( pIt->iType == HASHMAP_INT_NODE ){` |
|       ! 0 | 5333 | `					PH7_MemObjInitFromInt(pMap->pVm,&key2,pIt->xKey.iKey);` |
|       ! 0 | 5334 | `				}else{` |
|         - | 5335 | `					SyString sStr;` |
|        33 | 5336 | `					SyStringInitFromBuf(&sStr,` |
|         - | 5337 | `						SyBlobData(&pIt->xKey.sKey),` |
|         - | 5338 | `						SyBlobLength(&pIt->xKey.sKey));` |
|        33 | 5339 | `					PH7_MemObjInitFromString(pMap->pVm,&key2,&sStr);` |
|         - | 5340 | `				}` |
|        33 | 5341 | `				PH7_MemObjInit(pMap->pVm,&result);` |
|         - | 5342 | `				/* call user callback with (key1, key2) */` |
|         - | 5343 | `				{` |
|         - | 5344 | `					ph7_value *apK[2];` |
|        33 | 5345 | `					apK[0] = &key1;` |
|        33 | 5346 | `					apK[1] = &key2;` |
|        33 | 5347 | `					rc = PH7_VmCallUserFunction(pMap->pVm,pCallback,2,apK,&result);` |
|         - | 5348 | `				}` |
|        33 | 5349 | `				if( rc == PH7_EXCEPTION ){` |
|         - | 5350 | `					/* The key comparison callback raised. Unlike array_udiff/` |
|         - | 5351 | `					 * array_uintersect (which signal back from` |
|         - | 5352 | `					 * HashmapFindValueByCallback via pVm->iCmpCallbackExc), this` |
|         - | 5353 | `					 * function invokes the callback inline, so it cleans up its own` |
|         - | 5354 | `					 * temporaries and propagates the exception directly. */` |
|         3 | 5355 | `					PH7_MemObjRelease(&result);` |
|         3 | 5356 | `					PH7_MemObjRelease(&key1);` |
|         3 | 5357 | `					PH7_MemObjRelease(&key2);` |
|         3 | 5358 | `					return PH7_EXCEPTION;` |
|         - | 5359 | `				}` |
|        31 | 5360 | `				if( rc == SXRET_OK ){` |
|        31 | 5361 | `					if( (result.iFlags & MEMOBJ_INT) == 0 ){` |
|       ! 0 | 5362 | `						PH7_MemObjToInteger(&result);` |
|       ! 0 | 5363 | `					}` |
|        31 | 5364 | `					if( result.x.iVal == 0 ){` |
|         - | 5365 | `						/* keys considered equal by callback; now compare values */` |
|        13 | 5366 | `						ph7_value *pVal1 = HashmapExtractNodeValue(pEntry);` |
|        13 | 5367 | `						ph7_value *pVal2 = HashmapExtractNodeValue(pIt);` |
|        13 | 5368 | `						if( pVal1 && pVal2 ){` |
|        13 | 5369 | `							if( PH7_MemObjCmp(pVal1,pVal2,TRUE,0) == 0 ){` |
|         9 | 5370 | `								keep = 0;` |
|         9 | 5371 | `								PH7_MemObjRelease(&result);` |
|         - | 5372 | `								/* release keys too before breaking */` |
|         9 | 5373 | `								PH7_MemObjRelease(&key1);` |
|         9 | 5374 | `								PH7_MemObjRelease(&key2);` |
|         9 | 5375 | `								break;` |
|         - | 5376 | `							}` |
|         2 | 5377 | `						}` |
|         2 | 5378 | `					}` |
|        11 | 5379 | `				}` |
|        23 | 5380 | `				PH7_MemObjRelease(&result);` |
|        23 | 5381 | `				PH7_MemObjRelease(&key1);` |
|        23 | 5382 | `				PH7_MemObjRelease(&key2);` |
|         - | 5383 | `				/* move to next node */` |
|        23 | 5384 | `				pIt = pIt->pPrev;` |
|        23 | 5385 | `				if( keep == 0 ) break;` |
|         1 | 5386 | `			}` |
|        21 | 5387 | `			if( keep == 0 ) break;` |
|         7 | 5388 | `		}` |
|        17 | 5389 | `		if( keep ){` |
|         - | 5390 | `			/* Perform the insertion */` |
|         9 | 5391 | `			HashmapInsertNode((ph7_hashmap *)pArray->x.pOther,pEntry,TRUE);` |
|         4 | 5392 | `		}` |
|         - | 5393 | `		/* Point to the next entry */` |
|        17 | 5394 | `		pEntry = pEntry->pPrev; /* Reverse link */` |
|        17 | 5395 | `		n--;` |
|         1 | 5396 | `	}` |
|         - | 5397 | `	/* Return the freshly created array */` |
|         9 | 5398 | `	ph7_result_value(pCtx,pArray);` |
|         9 | 5399 | `	return PH7_OK;` |
|        17 | 5400 | `}` |
|         - | 5401 | `/*` |
|         - | 5402 | ` * array array_diff_key(array $array1 ,array $array2,...)` |
|         - | 5403 | ` *  Computes the difference of arrays using keys for comparison.` |
|         - | 5404 | ` * Parameters` |
|         - | 5405 | ` *  $array1` |
|         - | 5406 | ` *    The array to compare from` |
|         - | 5407 | ` *  $array2` |
|         - | 5408 | ` *    An array to compare against` |
|         - | 5409 | ` *  $...` |
|         - | 5410 | ` *   More arrays to compare against` |
|         - | 5411 | ` * Return` |
|         - | 5412 | ` *  Returns an array containing all the entries from array1 whose keys are not present` |
|         - | 5413 | ` *  in any of the other arrays.` |
|         - | 5414 | ` * Note that NULL is returned on failure.` |
|         - | 5415 | ` */` |
|        14 | 5416 | `static int ph7_hashmap_diff_key(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|         4 | 5417 | `{` |
|         - | 5418 | `	ph7_hashmap_node *pEntry;` |
|         - | 5419 | `	ph7_hashmap *pSrc,*pMap;` |
|         - | 5420 | `	ph7_value *pArray;` |
|         - | 5421 | `	sxi32 rc;` |
|         - | 5422 | `	sxu32 n;` |
|         - | 5423 | `	int i;` |
|         - | 5424 | `	/* Validate arguments to mirror PHP behaviour. Previously invalid inputs` |
|         - | 5425 | `	 * would quietly return NULL which is inconsistent with other hashmap` |
|         - | 5426 | `	 * helpers. */` |
|        18 | 5427 | `	if( nArg < 1 ){` |
|         4 | 5428 | `		return PH7_VmThrowException(pCtx,` |
|         - | 5429 | `			"ArgumentCountError",` |
|         - | 5430 | `			"array_diff_key() expects at least 1 argument, %d given",` |
|         1 | 5431 | `			nArg` |
|         - | 5432 | `			);` |
|         - | 5433 | `	}` |
|        15 | 5434 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|         4 | 5435 | `		return PH7_VmThrowException(pCtx,` |
|         - | 5436 | `			"TypeError",` |
|         - | 5437 | `			"array_diff_key(): Argument #1 ($array) must be of type array, %s given",` |
|         1 | 5438 | `			ph7_type_name(apArg[0])` |
|         - | 5439 | `			);` |
|         - | 5440 | `	}` |
|        20 | 5441 | `	for(i = 1 ; i < nArg ; i++){` |
|        12 | 5442 | `		if( !ph7_value_is_array(apArg[i]) ){` |
|         4 | 5443 | `			return PH7_VmThrowException(pCtx,` |
|         - | 5444 | `				"TypeError",` |
|         - | 5445 | `				"array_diff_key(): Argument #%d must be of type array, %s given",` |
|         1 | 5446 | `				i + 1,` |
|         2 | 5447 | `				ph7_type_name(apArg[i])` |
|         - | 5448 | `				);` |
|         - | 5449 | `		}` |
|         5 | 5450 | `	}` |
|         9 | 5451 | `	if( nArg == 1 ){` |
|         - | 5452 | `		/* Return the first array since we cannot perform a diff */` |
|         3 | 5453 | `		ph7_result_value(pCtx,apArg[0]);` |
|         3 | 5454 | `		return PH7_OK;` |
|         - | 5455 | `	}` |
|         - | 5456 | `	/* Create a new array */` |
|         7 | 5457 | `	pArray = ph7_context_new_array(pCtx);` |
|         7 | 5458 | `	if( pArray == 0 ){` |
|       ! 0 | 5459 | `		ph7_result_null(pCtx);` |
|       ! 0 | 5460 | `		return PH7_OK;` |
|         - | 5461 | `	}` |
|         - | 5462 | `	/* Point to the internal representation of the main hashmap */` |
|         7 | 5463 | `	pSrc = (ph7_hashmap *)apArg[0]->x.pOther;` |
|         - | 5464 | `	/* Perfrom the diff */` |
|         7 | 5465 | `	pEntry = pSrc->pFirst;` |
|         7 | 5466 | `	n = pSrc->nEntry;` |
|        12 | 5467 | `	for(;;){` |
|        25 | 5468 | `		if( n < 1 ){` |
|         7 | 5469 | `			break;` |
|         - | 5470 | `		}` |
|        31 | 5471 | `		for( i = 1 ; i < nArg ; i++ ){` |
|        23 | 5472 | `			if( !ph7_value_is_array(apArg[i])) {` |
|         - | 5473 | `				/* ignore */` |
|       ! 0 | 5474 | `				continue;` |
|         - | 5475 | `			}` |
|        23 | 5476 | `			pMap = (ph7_hashmap *)apArg[i]->x.pOther;` |
|        23 | 5477 | `			if( pEntry->iType == HASHMAP_BLOB_NODE ){` |
|        17 | 5478 | `				SyBlob *pKey = &pEntry->xKey.sKey;` |
|         - | 5479 | `				/* Blob lookup */` |
|        17 | 5480 | `				rc = HashmapLookupBlobKey(pMap,SyBlobData(pKey),SyBlobLength(pKey),0);` |
|         9 | 5481 | `			}else{` |
|         - | 5482 | `				/* Int lookup */` |
|         7 | 5483 | `				rc = HashmapLookupIntKey(pMap,pEntry->xKey.iKey,0);` |
|         - | 5484 | `			}` |
|        23 | 5485 | `			if( rc == SXRET_OK ){` |
|         - | 5486 | `				/* Key exists,break immediately */` |
|        11 | 5487 | `				break;` |
|         - | 5488 | `			}` |
|         7 | 5489 | `		}` |
|        19 | 5490 | `		if( i >= nArg ){` |
|         - | 5491 | `			/* Perform the insertion */` |
|         9 | 5492 | `			HashmapInsertNode((ph7_hashmap *)pArray->x.pOther,pEntry,TRUE);` |
|         4 | 5493 | `		}` |
|         - | 5494 | `		/* Point to the next entry */` |
|        19 | 5495 | `		pEntry = pEntry->pPrev; /* Reverse link */` |
|        19 | 5496 | `		n--;` |
|         1 | 5497 | `	}` |
|         - | 5498 | `	/* Return the freshly created array */` |
|         7 | 5499 | `	ph7_result_value(pCtx,pArray);` |
|         7 | 5500 | `	return PH7_OK;` |
|        11 | 5501 | `}` |
|         - | 5502 | `/*` |
|         - | 5503 | ` * array array_intersect(array $array1 ,array $array2,...)` |
|         - | 5504 | ` *  Computes the intersection of arrays.` |
|         - | 5505 | ` * Parameters` |
|         - | 5506 | ` *  $array1` |
|         - | 5507 | ` *    The array to compare from` |
|         - | 5508 | ` *  $array2` |
|         - | 5509 | ` *    An array to compare against` |
|         - | 5510 | ` *  $...` |
|         - | 5511 | ` *   More arrays to compare against` |
|         - | 5512 | ` * Return` |
|         - | 5513 | ` *  Returns an array containing all of the values in array1 whose values exist` |
|         - | 5514 | ` *  in all of the parameters.` |
|         - | 5515 | ` * Throws ArgumentCountError if no arguments are given.` |
|         - | 5516 | ` * Throws TypeError if any argument is not an array.` |
|         - | 5517 | ` */` |
|        22 | 5518 | `static int ph7_hashmap_intersect(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|         4 | 5519 | `{` |
|         - | 5520 | `	ph7_hashmap_node *pEntry;` |
|         - | 5521 | `	ph7_hashmap *pSrc,*pMap;` |
|         - | 5522 | `	ph7_value *pArray;` |
|         - | 5523 | `	ph7_value *pVal;` |
|         - | 5524 | `	sxi32 rc;` |
|         - | 5525 | `	sxu32 n;` |
|         - | 5526 | `	int i;` |
|        26 | 5527 | `	if( nArg < 1 ){` |
|         4 | 5528 | `		return PH7_VmThrowException(pCtx,` |
|         - | 5529 | `			"ArgumentCountError",` |
|         - | 5530 | `			"array_intersect() expects at least 1 argument, %d given",` |
|         1 | 5531 | `			nArg` |
|         - | 5532 | `			);` |
|         - | 5533 | `	}` |
|        23 | 5534 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|         4 | 5535 | `		return PH7_VmThrowException(pCtx,` |
|         - | 5536 | `			"TypeError",` |
|         - | 5537 | `			"array_intersect(): Argument #1 ($array) must be of type array, %s given",` |
|         1 | 5538 | `			ph7_type_name(apArg[0])` |
|         - | 5539 | `			);` |
|         - | 5540 | `	}` |
|        36 | 5541 | `	for( i = 1 ; i < nArg ; i++ ){` |
|        20 | 5542 | `		if( !ph7_value_is_array(apArg[i]) ){` |
|         4 | 5543 | `			return PH7_VmThrowException(pCtx,` |
|         - | 5544 | `				"TypeError",` |
|         - | 5545 | `				"array_intersect(): Argument #%d must be of type array, %s given",` |
|         1 | 5546 | `				i + 1,` |
|         2 | 5547 | `				ph7_type_name(apArg[i])` |
|         - | 5548 | `				);` |
|         - | 5549 | `		}` |
|         9 | 5550 | `	}` |
|        17 | 5551 | `	if( nArg == 1 ){` |
|         - | 5552 | `		/* Return the first array since we cannot perform a diff */` |
|         3 | 5553 | `		ph7_result_value(pCtx,apArg[0]);` |
|         3 | 5554 | `		return PH7_OK;` |
|         - | 5555 | `	}` |
|         - | 5556 | `	/* Create a new array */` |
|        15 | 5557 | `	pArray = ph7_context_new_array(pCtx);` |
|        15 | 5558 | `	if( pArray == 0 ){` |
|       ! 0 | 5559 | `		ph7_result_null(pCtx);` |
|       ! 0 | 5560 | `		return PH7_OK;` |
|         - | 5561 | `	}` |
|         - | 5562 | `	/* Point to the internal representation of the source hashmap */` |
|        15 | 5563 | `	pSrc = (ph7_hashmap *)apArg[0]->x.pOther;` |
|         - | 5564 | `	/* Perform the intersection */` |
|        15 | 5565 | `	pEntry = pSrc->pFirst;` |
|        15 | 5566 | `	n = pSrc->nEntry;` |
|        31 | 5567 | `	for(;;){` |
|        63 | 5568 | `		if( n < 1 ){` |
|        15 | 5569 | `			break;` |
|         - | 5570 | `		}` |
|         - | 5571 | `		/* Extract the node value */` |
|        49 | 5572 | `		pVal = HashmapExtractNodeValue(pEntry);` |
|        49 | 5573 | `		if( pVal ){` |
|        79 | 5574 | `			for( i = 1 ; i < nArg ; i++ ){` |
|         - | 5575 | `				/* Point to the internal representation of the hashmap */` |
|        55 | 5576 | `				pMap = (ph7_hashmap *)apArg[i]->x.pOther;` |
|         - | 5577 | `				/* Perform the lookup */` |
|        55 | 5578 | `				rc = HashmapFindValue(pMap,pVal,0,TRUE);` |
|        55 | 5579 | `				if( rc != SXRET_OK ){` |
|         - | 5580 | `					/* Value does not exist */` |
|        25 | 5581 | `					break;` |
|         - | 5582 | `				}` |
|        16 | 5583 | `			}` |
|        49 | 5584 | `			if( i >= nArg ){` |
|         - | 5585 | `				/* Perform the insertion */` |
|        25 | 5586 | `				HashmapInsertNode((ph7_hashmap *)pArray->x.pOther,pEntry,TRUE);` |
|        12 | 5587 | `			}` |
|        24 | 5588 | `		}` |
|         - | 5589 | `		/* Point to the next entry */` |
|        49 | 5590 | `		pEntry = pEntry->pPrev; /* Reverse link */` |
|        49 | 5591 | `		n--;` |
|         1 | 5592 | `	}` |
|         - | 5593 | `	/* Return the freshly created array */` |
|        15 | 5594 | `	ph7_result_value(pCtx,pArray);` |
|        15 | 5595 | `	return PH7_OK;` |
|        15 | 5596 | `}` |
|         - | 5597 | `/*` |
|         - | 5598 | ` * array array_intersect_assoc(array $array1 ,array $array2,...)` |
|         - | 5599 | ` *  Computes the intersection of arrays with additional index check.` |
|         - | 5600 | ` * Parameters` |
|         - | 5601 | ` *  $array1` |
|         - | 5602 | ` *    The array to compare from` |
|         - | 5603 | ` *  $array2` |
|         - | 5604 | ` *    An array to compare against` |
|         - | 5605 | ` *  $...` |
|         - | 5606 | ` *   More arrays to compare against` |
|         - | 5607 | ` * Return` |
|         - | 5608 | ` *  Returns an array containing all the values of array1 that are present` |
|         - | 5609 | ` *  in all the arguments, with matching keys.` |
|         - | 5610 | ` */` |
|        22 | 5611 | `static int ph7_hashmap_intersect_assoc(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|         4 | 5612 | `{` |
|         - | 5613 | `	ph7_hashmap_node *pEntry,*pN1,*pN2;` |
|         - | 5614 | `	ph7_hashmap *pSrc,*pMap;` |
|         - | 5615 | `	ph7_value *pArray;` |
|         - | 5616 | `	ph7_value *pVal;` |
|         - | 5617 | `	sxi32 rc;` |
|         - | 5618 | `	sxu32 n;` |
|         - | 5619 | `	int i;` |
|        26 | 5620 | `	if( nArg < 1 ){` |
|         4 | 5621 | `		return PH7_VmThrowException(pCtx,` |
|         - | 5622 | `			"ArgumentCountError",` |
|         - | 5623 | `			"array_intersect_assoc() expects at least 1 argument, %d given",` |
|         1 | 5624 | `			nArg` |
|         - | 5625 | `			);` |
|         - | 5626 | `	}` |
|        23 | 5627 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|         4 | 5628 | `		return PH7_VmThrowException(pCtx,` |
|         - | 5629 | `			"TypeError",` |
|         - | 5630 | `			"array_intersect_assoc(): Argument #1 ($array) must be of type array, %s given",` |
|         1 | 5631 | `			ph7_type_name(apArg[0])` |
|         - | 5632 | `			);` |
|         - | 5633 | `	}` |
|        36 | 5634 | `	for( i = 1 ; i < nArg ; i++ ){` |
|        20 | 5635 | `		if( !ph7_value_is_array(apArg[i]) ){` |
|         4 | 5636 | `			return PH7_VmThrowException(pCtx,` |
|         - | 5637 | `				"TypeError",` |
|         - | 5638 | `				"array_intersect_assoc(): Argument #%d must be of type array, %s given",` |
|         1 | 5639 | `				i + 1,` |
|         2 | 5640 | `				ph7_type_name(apArg[i])` |
|         - | 5641 | `				);` |
|         - | 5642 | `		}` |
|         9 | 5643 | `	}` |
|        17 | 5644 | `	if( nArg == 1 ){` |
|         - | 5645 | `		/* Return the first array since we cannot perform an intersection */` |
|         3 | 5646 | `		ph7_result_value(pCtx,apArg[0]);` |
|         3 | 5647 | `		return PH7_OK;` |
|         - | 5648 | `	}` |
|         - | 5649 | `	/* Create a new array */` |
|        15 | 5650 | `	pArray = ph7_context_new_array(pCtx);` |
|        15 | 5651 | `	if( pArray == 0 ){` |
|       ! 0 | 5652 | `		ph7_result_null(pCtx);` |
|       ! 0 | 5653 | `		return PH7_OK;` |
|         - | 5654 | `	}` |
|         - | 5655 | `	/* Point to the internal representation of the source hashmap */` |
|        15 | 5656 | `	pSrc = (ph7_hashmap *)apArg[0]->x.pOther;` |
|         - | 5657 | `	/* Perform the intersection */` |
|        15 | 5658 | `	pEntry = pSrc->pFirst;` |
|        15 | 5659 | `	n = pSrc->nEntry;` |
|        15 | 5660 | `	pN1 = pN2 = 0; /* cc warning */` |
|        23 | 5661 | `	for(;;){` |
|        47 | 5662 | `		if( n < 1 ){` |
|        15 | 5663 | `			break;` |
|         - | 5664 | `		}` |
|         - | 5665 | `		/* Extract the node value */` |
|        33 | 5666 | `		pVal = HashmapExtractNodeValue(pEntry);` |
|        33 | 5667 | `		if( pVal ){` |
|        53 | 5668 | `			for( i = 1 ; i < nArg ; i++ ){` |
|         - | 5669 | `				/* Point to the internal representation of the hashmap */` |
|        37 | 5670 | `				pMap = (ph7_hashmap *)apArg[i]->x.pOther;` |
|         - | 5671 | `				/* Perform a key lookup first */` |
|        37 | 5672 | `				if( pEntry->iType == HASHMAP_INT_NODE ){` |
|        15 | 5673 | `					rc = HashmapLookupIntKey(pMap,pEntry->xKey.iKey,&pN1);` |
|         8 | 5674 | `				}else{` |
|        23 | 5675 | `					rc = HashmapLookupBlobKey(pMap,SyBlobData(&pEntry->xKey.sKey),SyBlobLength(&pEntry->xKey.sKey),&pN1);` |
|         - | 5676 | `				}` |
|        37 | 5677 | `				if( rc != SXRET_OK ){` |
|         - | 5678 | `					/* No such key,break immediately */` |
|         7 | 5679 | `					break;` |
|         - | 5680 | `				}` |
|         - | 5681 | `				/* Perform the lookup */` |
|        31 | 5682 | `				rc = HashmapFindValue(pMap,pVal,&pN2,TRUE);` |
|        31 | 5683 | `				if( rc != SXRET_OK \|\| pN1 != pN2 ){` |
|         - | 5684 | `					/* Value does not exist */` |
|         6 | 5685 | `					break;` |
|         - | 5686 | `				}` |
|        11 | 5687 | `			}` |
|        33 | 5688 | `			if( i >= nArg ){` |
|         - | 5689 | `				/* Perform the insertion */` |
|        17 | 5690 | `				HashmapInsertNode((ph7_hashmap *)pArray->x.pOther,pEntry,TRUE);` |
|         8 | 5691 | `			}` |
|        16 | 5692 | `		}` |
|         - | 5693 | `		/* Point to the next entry */` |
|        33 | 5694 | `		pEntry = pEntry->pPrev; /* Reverse link */` |
|        33 | 5695 | `		n--;` |
|         1 | 5696 | `	}` |
|         - | 5697 | `	/* Return the freshly created array */` |
|        15 | 5698 | `	ph7_result_value(pCtx,pArray);` |
|        15 | 5699 | `	return PH7_OK;` |
|        15 | 5700 | `}` |
|         - | 5701 | `/*` |
|         - | 5702 | ` * array array_intersect_key(array $array1 ,...)` |
|         - | 5703 | ` *  Computes the intersection of arrays using keys for comparison.` |
|         - | 5704 | ` * Parameters` |
|         - | 5705 | ` *  $array1` |
|         - | 5706 | ` *    The array to compare from` |
|         - | 5707 | ` *  $...` |
|         - | 5708 | ` *   More arrays to compare against` |
|         - | 5709 | ` * Return` |
|         - | 5710 | ` *  Returns an associative array containing all the entries of array1 which` |
|         - | 5711 | ` *  have keys that are present in all arguments.` |
|         - | 5712 | ` * Note that NULL is returned on failure.` |
|         - | 5713 | ` */` |
|        22 | 5714 | `static int ph7_hashmap_intersect_key(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|         4 | 5715 | `{` |
|         - | 5716 | `	ph7_hashmap_node *pEntry;` |
|         - | 5717 | `	ph7_hashmap *pSrc,*pMap;` |
|         - | 5718 | `	ph7_value *pArray;` |
|         - | 5719 | `	sxi32 rc;` |
|         - | 5720 | `	sxu32 n;` |
|         - | 5721 | `	int i;` |
|        26 | 5722 | `	if( nArg < 1 ){` |
|         4 | 5723 | `		return PH7_VmThrowException(pCtx,` |
|         - | 5724 | `			"ArgumentCountError",` |
|         - | 5725 | `			"array_intersect_key() expects at least 1 argument, %d given",` |
|         1 | 5726 | `			nArg` |
|         - | 5727 | `			);` |
|         - | 5728 | `	}` |
|        23 | 5729 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|         4 | 5730 | `		return PH7_VmThrowException(pCtx,` |
|         - | 5731 | `			"TypeError",` |
|         - | 5732 | `			"array_intersect_key(): Argument #1 ($array) must be of type array, %s given",` |
|         1 | 5733 | `			ph7_type_name(apArg[0])` |
|         - | 5734 | `			);` |
|         - | 5735 | `	}` |
|        36 | 5736 | `	for( i = 1 ; i < nArg ; i++ ){` |
|        20 | 5737 | `		if( !ph7_value_is_array(apArg[i]) ){` |
|         4 | 5738 | `			return PH7_VmThrowException(pCtx,` |
|         - | 5739 | `				"TypeError",` |
|         - | 5740 | `				"array_intersect_key(): Argument #%d must be of type array, %s given",` |
|         1 | 5741 | `				i + 1,` |
|         2 | 5742 | `				ph7_type_name(apArg[i])` |
|         - | 5743 | `				);` |
|         - | 5744 | `		}` |
|         9 | 5745 | `	}` |
|        17 | 5746 | `	if( nArg == 1 ){` |
|         - | 5747 | `		/* Return the first array since we cannot perform an intersection */` |
|         3 | 5748 | `		ph7_result_value(pCtx,apArg[0]);` |
|         3 | 5749 | `		return PH7_OK;` |
|         - | 5750 | `	}` |
|         - | 5751 | `	/* Create a new array */` |
|        15 | 5752 | `	pArray = ph7_context_new_array(pCtx);` |
|        15 | 5753 | `	if( pArray == 0 ){` |
|       ! 0 | 5754 | `		ph7_result_null(pCtx);` |
|       ! 0 | 5755 | `		return PH7_OK;` |
|         - | 5756 | `	}` |
|         - | 5757 | `	/* Point to the internal representation of the main hashmap */` |
|        15 | 5758 | `	pSrc = (ph7_hashmap *)apArg[0]->x.pOther;` |
|         - | 5759 | `	/* Perform the intersection */` |
|        15 | 5760 | `	pEntry = pSrc->pFirst;` |
|        15 | 5761 | `	n = pSrc->nEntry;` |
|        24 | 5762 | `	for(;;){` |
|        49 | 5763 | `		if( n < 1 ){` |
|        15 | 5764 | `			break;` |
|         - | 5765 | `		}` |
|        57 | 5766 | `		for( i = 1 ; i < nArg ; i++ ){` |
|        39 | 5767 | `			pMap = (ph7_hashmap *)apArg[i]->x.pOther;` |
|        39 | 5768 | `			if( pEntry->iType == HASHMAP_BLOB_NODE ){` |
|        27 | 5769 | `				SyBlob *pKey = &pEntry->xKey.sKey;` |
|         - | 5770 | `				/* Blob lookup */` |
|        27 | 5771 | `				rc = HashmapLookupBlobKey(pMap,SyBlobData(pKey),SyBlobLength(pKey),0);` |
|        14 | 5772 | `			}else{` |
|         - | 5773 | `				/* Int key */` |
|        13 | 5774 | `				rc = HashmapLookupIntKey(pMap,pEntry->xKey.iKey,0);` |
|         - | 5775 | `			}` |
|        39 | 5776 | `			if( rc != SXRET_OK ){` |
|         - | 5777 | `				/* Key does not exist, break immediately */` |
|        17 | 5778 | `				break;` |
|         - | 5779 | `			}` |
|        12 | 5780 | `		}` |
|        35 | 5781 | `		if( i >= nArg ){` |
|         - | 5782 | `			/* Perform the insertion */` |
|        19 | 5783 | `			HashmapInsertNode((ph7_hashmap *)pArray->x.pOther,pEntry,TRUE);` |
|         9 | 5784 | `		}` |
|         - | 5785 | `		/* Point to the next entry */` |
|        35 | 5786 | `		pEntry = pEntry->pPrev; /* Reverse link */` |
|        35 | 5787 | `		n--;` |
|         1 | 5788 | `	}` |
|         - | 5789 | `	/* Return the freshly created array */` |
|        15 | 5790 | `	ph7_result_value(pCtx,pArray);` |
|        15 | 5791 | `	return PH7_OK;` |
|        15 | 5792 | `}` |
|         - | 5793 | `/*` |
|         - | 5794 | ` * array array_uintersect(array $array1 ,array $array2,...,$callback)` |
|         - | 5795 | ` *  Computes the intersection of arrays.` |
|         - | 5796 | ` * Parameters` |
|         - | 5797 | ` *  $array1` |
|         - | 5798 | ` *    The array to compare from` |
|         - | 5799 | ` *  $array2` |
|         - | 5800 | ` *    An array to compare against` |
|         - | 5801 | ` *  $...` |
|         - | 5802 | ` *   More arrays to compare against` |
|         - | 5803 | ` * $callback` |
|         - | 5804 | ` *  The callback comparison function.` |
|         - | 5805 | ` *  The comparison function must return an integer less than, equal to, or greater than zero` |
|         - | 5806 | ` *  if the first argument is considered to be respectively less than, equal to, or greater` |
|         - | 5807 | ` *  than the second.` |
|         - | 5808 | ` *     int callback ( mixed $a, mixed $b )` |
|         - | 5809 | ` * Return` |
|         - | 5810 | ` *  Returns an array containing all of the values in array1 whose values exist` |
|         - | 5811 | ` *  in all of the parameters. .` |
|         - | 5812 | ` * Note that NULL is returned on failure.` |
|         - | 5813 | ` */` |
|        26 | 5814 | `static int ph7_hashmap_uintersect(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|         5 | 5815 | `{` |
|         - | 5816 | `	ph7_hashmap_node *pEntry;` |
|         - | 5817 | `	ph7_hashmap *pSrc,*pMap;` |
|         - | 5818 | `	ph7_value *pCallback;` |
|         - | 5819 | `	ph7_value *pArray;` |
|         - | 5820 | `	ph7_value *pVal;` |
|         - | 5821 | `	sxi32 rc;` |
|         - | 5822 | `	sxu32 n;` |
|         - | 5823 | `	int i;` |
|         - | 5824 |  |
|         - | 5825 | `	/* Ensure the argument count matches PHP behaviour. */` |
|        31 | 5826 | `	if( nArg < 2 ){` |
|         4 | 5827 | `		return PH7_VmThrowException(pCtx,` |
|         - | 5828 | `			"ArgumentCountError",` |
|         - | 5829 | `			"array_uintersect() expects at least 2 arguments, %d given",` |
|         1 | 5830 | `			nArg` |
|         - | 5831 | `			);` |
|         - | 5832 | `	}` |
|        29 | 5833 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|         4 | 5834 | `		return PH7_VmThrowException(pCtx,` |
|         - | 5835 | `			"TypeError",` |
|         - | 5836 | `			"array_uintersect(): Argument #1 ($array) must be of type array, %s given",` |
|         1 | 5837 | `			ph7_type_name(apArg[0])` |
|         - | 5838 | `			);` |
|         - | 5839 | `	}` |
|         - | 5840 |  |
|        27 | 5841 | `	if( nArg == 2 ){` |
|         - | 5842 | `		/* Only the original array and the callback were provided. */` |
|         - | 5843 | `		/* Validate the callback below in order to match PHP's parameter` |
|         - | 5844 | `		 * validation ordering. */` |
|         3 | 5845 | `	} else {` |
|         - | 5846 | `		/* Ensure intermediary arguments are arrays (matches PHP strict typing). */` |
|        39 | 5847 | `		for( i = 1 ; i < nArg - 1; i++ ){` |
|        23 | 5848 | `			if( !ph7_value_is_array(apArg[i]) ){` |
|         4 | 5849 | `				return PH7_VmThrowException(pCtx,` |
|         - | 5850 | `					"TypeError",` |
|         - | 5851 | `					"array_uintersect(): Argument #%d must be of type array, %s given",` |
|         1 | 5852 | `					i + 1,` |
|         2 | 5853 | `					ph7_type_name(apArg[i])` |
|         - | 5854 | `					);` |
|         - | 5855 | `			}` |
|        13 | 5856 | `		}` |
|         - | 5857 | `	}` |
|         - | 5858 |  |
|         - | 5859 | `	/* Identify the callback (always expected as the last argument). */` |
|        25 | 5860 | `	pCallback = apArg[nArg - 1];` |
|         - | 5861 | `	/* Validate the callback to match PHP's error messages. */` |
|        25 | 5862 | `	if( !ph7_value_is_callable(pCallback) ){` |
|        14 | 5863 | `		if( ph7_value_is_array(pCallback) ){` |
|         - | 5864 | `			/* PHP emits a special message when the array length is wrong.` |
|         - | 5865 | `			 * If the array has two elements but is still not callable (e.g. missing` |
|         - | 5866 | `			 * method / missing class), we must emit a more general error instead.` |
|         - | 5867 | `			 */` |
|         9 | 5868 | `			ph7_hashmap *pCb = (ph7_hashmap *)pCallback->x.pOther;` |
|         9 | 5869 | `			if( pCb->nEntry != 2 ){` |
|         4 | 5870 | `				return PH7_VmThrowException(pCtx,` |
|         - | 5871 | `					"TypeError",` |
|         - | 5872 | `					"array_uintersect(): Argument #%d must be a valid callback, array callback must have exactly two members",` |
|         1 | 5873 | `					nArg` |
|         - | 5874 | `					);` |
|         - | 5875 | `			}` |
|         - | 5876 | `			/* Try to provide a more precise error like PHP does for missing classes/methods. */` |
|         - | 5877 | `			{` |
|         6 | 5878 | `				ph7_value *pKey = (ph7_value *)SySetAt(&pCtx->pVm->aMemObj,pCb->pFirst->nValIdx);` |
|         6 | 5879 | `				ph7_value *pMethod = (ph7_value *)SySetAt(&pCtx->pVm->aMemObj,pCb->pFirst->pPrev->nValIdx);` |
|         6 | 5880 | `				if( pKey && pMethod && (pMethod->iFlags & MEMOBJ_STRING) ){` |
|         - | 5881 | `					int nMethodLen;` |
|         6 | 5882 | `					const char *zMethod = ph7_value_to_string(pMethod,&nMethodLen);` |
|         6 | 5883 | `					ph7_class *pClass = PH7_VmExtractClassFromValue(pCtx->pVm,pKey);` |
|         6 | 5884 | `					if( pClass ){` |
|         - | 5885 | `						/* Class exists but method is missing. */` |
|         4 | 5886 | `						return PH7_VmThrowException(pCtx,` |
|         - | 5887 | `							"TypeError",` |
|         - | 5888 | `							"array_uintersect(): Argument #%d must be a valid callback, class %s does not have a method \"%s\"",` |
|         1 | 5889 | `							nArg,` |
|         1 | 5890 | `							(const char *)SyStringData(&pClass->sName),` |
|         1 | 5891 | `							zMethod` |
|         - | 5892 | `							);` |
|         - | 5893 | `					}` |
|         - | 5894 | `					/* Class not found */` |
|         - | 5895 | `					{` |
|         - | 5896 | `						int nName;` |
|         3 | 5897 | `						const char *zName = ph7_value_to_string(pKey,&nName);` |
|         4 | 5898 | `						return PH7_VmThrowException(pCtx,` |
|         - | 5899 | `							"TypeError",` |
|         - | 5900 | `							"array_uintersect(): Argument #%d must be a valid callback, class \"%s\" not found",` |
|         1 | 5901 | `							nArg,` |
|         1 | 5902 | `							zName` |
|         - | 5903 | `							);` |
|         - | 5904 | `					}` |
|         - | 5905 | `				}` |
|         - | 5906 | `			}` |
|         - | 5907 | `			/* Fallback message */` |
|       ! 0 | 5908 | `			return PH7_VmThrowException(pCtx,` |
|         - | 5909 | `				"TypeError",` |
|         - | 5910 | `				"array_uintersect(): Argument #%d must be a valid callback, no array or string given",` |
|       ! 0 | 5911 | `				nArg` |
|         - | 5912 | `				);` |
|         - | 5913 | `		}` |
|         6 | 5914 | `		if( ph7_value_is_string(pCallback) ){` |
|         - | 5915 | `			int len;` |
|         3 | 5916 | `			const char *zName = ph7_value_to_string(pCallback, &len);` |
|         4 | 5917 | `			return PH7_VmThrowException(pCtx,` |
|         - | 5918 | `				"TypeError",` |
|         - | 5919 | `				"array_uintersect(): Argument #%d must be a valid callback, function \"%s\" not found or invalid function name",` |
|         1 | 5920 | `				nArg,` |
|         1 | 5921 | `				zName` |
|         - | 5922 | `				);` |
|         - | 5923 | `		}` |
|         4 | 5924 | `		return PH7_VmThrowException(pCtx,` |
|         - | 5925 | `			"TypeError",` |
|         - | 5926 | `			"array_uintersect(): Argument #%d must be a valid callback, no array or string given",` |
|         1 | 5927 | `			nArg` |
|         - | 5928 | `			);` |
|         - | 5929 | `	}` |
|         - | 5930 |  |
|        11 | 5931 | `	if( nArg == 2 ){` |
|         - | 5932 | `		/* Only the original array and the callback were provided. */` |
|         5 | 5933 | `		ph7_result_value(pCtx,apArg[0]);` |
|         5 | 5934 | `		return PH7_OK;` |
|         - | 5935 | `	}` |
|         - | 5936 |  |
|         - | 5937 | `	/* Create a new array */` |
|         7 | 5938 | `	pArray = ph7_context_new_array(pCtx);` |
|         7 | 5939 | `	if( pArray == 0 ){` |
|       ! 0 | 5940 | `		ph7_result_null(pCtx);` |
|       ! 0 | 5941 | `		return PH7_OK;` |
|         - | 5942 | `	}` |
|         - | 5943 | `	/* Point to the internal representation of the source hashmap */` |
|         7 | 5944 | `	pSrc = (ph7_hashmap *)apArg[0]->x.pOther;` |
|         - | 5945 | `	/* Perform the intersection */` |
|         7 | 5946 | `	pEntry = pSrc->pFirst;` |
|         7 | 5947 | `	n = pSrc->nEntry;` |
|         7 | 5948 | `	pCtx->pVm->iCmpCallbackExc = 0;` |
|         9 | 5949 | `	for(;;){` |
|        19 | 5950 | `		if( n < 1 ){` |
|         5 | 5951 | `			break;` |
|         - | 5952 | `		}` |
|         - | 5953 | `		/* Extract the node value */` |
|        15 | 5954 | `		pVal = HashmapExtractNodeValue(pEntry);` |
|        15 | 5955 | `		if( pVal ){` |
|        23 | 5956 | `			for( i = 1 ; i < nArg - 1; i++ ){` |
|        15 | 5957 | `				if( !ph7_value_is_array(apArg[i])) {` |
|         - | 5958 | `					/* ignore */` |
|       ! 0 | 5959 | `					continue;` |
|         - | 5960 | `				}` |
|         - | 5961 | `				/* Point to the internal representation of the hashmap */` |
|        15 | 5962 | `				pMap = (ph7_hashmap *)apArg[i]->x.pOther;` |
|         - | 5963 | `				/* Perform the lookup */` |
|        15 | 5964 | `				rc = HashmapFindValueByCallback(pMap,pVal,pCallback,0);` |
|        15 | 5965 | `				if( rc != SXRET_OK ){` |
|         - | 5966 | `					/* Value does not exist */` |
|         7 | 5967 | `					break;` |
|         - | 5968 | `				}` |
|         5 | 5969 | `			}` |
|        15 | 5970 | `			if( i >= (nArg-1) ){` |
|         - | 5971 | `				/* Perform the insertion */` |
|         9 | 5972 | `				HashmapInsertNode((ph7_hashmap *)pArray->x.pOther,pEntry,TRUE);` |
|         4 | 5973 | `			}` |
|         7 | 5974 | `		}` |
|        15 | 5975 | `		if( pCtx->pVm->iCmpCallbackExc ){` |
|         - | 5976 | `			/* The comparison callback raised: propagate so the dispatcher unwinds. */` |
|         3 | 5977 | `			pCtx->pVm->iCmpCallbackExc = 0;` |
|         3 | 5978 | `			return PH7_EXCEPTION;` |
|         - | 5979 | `		}` |
|         - | 5980 | `		/* Point to the next entry */` |
|        13 | 5981 | `		pEntry = pEntry->pPrev; /* Reverse link */` |
|        13 | 5982 | `		n--;` |
|         1 | 5983 | `	}` |
|         - | 5984 | `	/* Return the freshly created array */` |
|         5 | 5985 | `	ph7_result_value(pCtx,pArray);` |
|         5 | 5986 | `	return PH7_OK;` |
|        18 | 5987 | `}` |
|         - | 5988 | `/*` |
|         - | 5989 | ` * array array_fill(int $start_index,int $num,var $value)` |
|         - | 5990 | ` *  Fill an array with values.` |
|         - | 5991 | ` * Parameters` |
|         - | 5992 | ` *  $start_index` |
|         - | 5993 | ` *    The first index of the returned array.` |
|         - | 5994 | ` *  $num` |
|         - | 5995 | ` *   Number of elements to insert.` |
|         - | 5996 | ` *  $value` |
|         - | 5997 | ` *    Value to use for filling.` |
|         - | 5998 | ` * Return` |
|         - | 5999 | ` *  The filled array or null on failure.` |
|         - | 6000 | ` */` |
|       244 | 6001 | `static int ph7_hashmap_fill(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|         5 | 6002 | `{` |
|         - | 6003 | `	ph7_value *pArray;` |
|         - | 6004 | `	int i,nEntry;` |
|         - | 6005 |  |
|         - | 6006 | `	/* PHP enforces argument count and type checks. */` |
|       249 | 6007 | `	if( nArg != 3 ){` |
|         - | 6008 | `		/* wrong number of arguments -> ArgumentCountError */` |
|         8 | 6009 | `		return PH7_VmThrowException(pCtx,` |
|         - | 6010 | `			"ArgumentCountError",` |
|         - | 6011 | `			"array_fill() expects exactly 3 arguments, %d given",` |
|         2 | 6012 | `			nArg` |
|         - | 6013 | `			);` |
|         - | 6014 | `	}` |
|         - | 6015 |  |
|         - | 6016 | `	/* Argument #1: start index must be convertible to int.  Accept booleans,` |
|         - | 6017 | `	 * floats, and numeric strings (including those with decimal point) by` |
|         - | 6018 | `	 * allowing them through the conversion.  Only arrays, objects, resources` |
|         - | 6019 | `	 * and NULLs are rejected outright. */` |
|       359 | 6020 | `	if( ph7_value_is_array(apArg[0]) \|\| ph7_value_is_object(apArg[0]) \|\|` |
|       362 | 6021 | `		ph7_value_is_resource(apArg[0]) \|\| ph7_value_is_null(apArg[0]) ){` |
|         4 | 6022 | `		return PH7_VmThrowException(pCtx,` |
|         - | 6023 | `			"TypeError",` |
|         - | 6024 | `			"array_fill(): Argument #1 ($start_index) must be of type int, %s given",` |
|         1 | 6025 | `			ph7_type_name(apArg[0])` |
|         - | 6026 | `			);` |
|         - | 6027 | `	}` |
|       242 | 6028 | `	if( ph7_value_is_string(apArg[0]) ){` |
|         - | 6029 | `		int len;` |
|         8 | 6030 | `		sxu8 bReal = FALSE;` |
|         8 | 6031 | `		const char *zStr = ph7_value_to_string(apArg[0], &len);` |
|         8 | 6032 | `		if( SyStrIsNumeric(zStr, len, &bReal, 0) != SXRET_OK ){` |
|         - | 6033 | `			/* Non‑numeric string is an error. */` |
|         3 | 6034 | `			return PH7_VmThrowException(pCtx,` |
|         - | 6035 | `				"TypeError",` |
|         - | 6036 | `				"array_fill(): Argument #1 ($start_index) must be of type int, string given"` |
|         - | 6037 | `				);` |
|         - | 6038 | `		}` |
|         5 | 6039 | `		if( bReal ){` |
|         - | 6040 | `			/* float-string -> deprecation warning */` |
|         4 | 6041 | `			ph7_context_throw_error_format(pCtx, E_DEPRECATED,` |
|         - | 6042 | `				"Implicit conversion from float-string \"%s\" to int loses precision",` |
|         1 | 6043 | `				zStr` |
|         - | 6044 | `				);` |
|         1 | 6045 | `		}` |
|         2 | 6046 | `	}` |
|         - | 6047 |  |
|         - | 6048 | `	/* Argument #2: count must be convertible to non-negative int.  Allow booleans,` |
|         - | 6049 | `	 * floats and numeric strings; reject arrays, objects, resources and NULL. */` |
|       354 | 6050 | `	if( ph7_value_is_array(apArg[1]) \|\| ph7_value_is_object(apArg[1]) \|\|` |
|       357 | 6051 | `		ph7_value_is_resource(apArg[1]) \|\| ph7_value_is_null(apArg[1]) ){` |
|       ! 0 | 6052 | `		return PH7_VmThrowException(pCtx,` |
|         - | 6053 | `			"TypeError",` |
|         - | 6054 | `			"array_fill(): Argument #2 ($count) must be of type int, %s given",` |
|       ! 0 | 6055 | `			ph7_type_name(apArg[1])` |
|         - | 6056 | `			);` |
|         - | 6057 | `	}` |
|       239 | 6058 | `	if( ph7_value_is_string(apArg[1]) ){` |
|         - | 6059 | `		int len;` |
|         3 | 6060 | `		sxu8 bReal = FALSE;` |
|         3 | 6061 | `		const char *zStr = ph7_value_to_string(apArg[1], &len);` |
|         3 | 6062 | `		if( SyStrIsNumeric(zStr, len, &bReal, 0) != SXRET_OK ){` |
|         3 | 6063 | `			return PH7_VmThrowException(pCtx,` |
|         - | 6064 | `				"TypeError",` |
|         - | 6065 | `				"array_fill(): Argument #2 ($count) must be of type int, string given"` |
|         - | 6066 | `				);` |
|         - | 6067 | `		}` |
|       ! 0 | 6068 | `	}` |
|         - | 6069 | `	/* Note: booleans and floats (including fractional) are now accepted; they` |
|         - | 6070 | `	 * will be converted by ph7_value_to_int below. */` |
|       236 | 6071 | `	if( ph7_value_is_float(apArg[1]) ){` |
|         5 | 6072 | `		double d = ph7_value_to_double(apArg[1]);` |
|         - | 6073 | `		/* avoid hiding outer 'i' (loop index) */` |
|         5 | 6074 | `		sxi64 i64 = (sxi64)d;` |
|         5 | 6075 | `		if( d != (double)i64 ){` |
|         7 | 6076 | `			ph7_context_throw_error_format(pCtx, E_DEPRECATED,` |
|         - | 6077 | `				"Implicit conversion from float %g to int loses precision",` |
|         2 | 6078 | `				d` |
|         - | 6079 | `				);` |
|         2 | 6080 | `		}` |
|         2 | 6081 | `	}` |
|         - | 6082 |  |
|         - | 6083 | `	/* Total number of entries to insert */` |
|       236 | 6084 | `	nEntry = ph7_value_to_int(apArg[1]);` |
|         - | 6085 | `	/* Reject negative counts with a ValueError like PHP. */` |
|       236 | 6086 | `	if( nEntry < 0 ){` |
|         3 | 6087 | `		return PH7_VmThrowException(pCtx,` |
|         - | 6088 | `			"ValueError",` |
|         - | 6089 | `			"array_fill(): Argument #2 ($count) must be greater than or equal to 0"` |
|         - | 6090 | `			);` |
|         - | 6091 | `	}` |
|         - | 6092 |  |
|         - | 6093 | `	/* If zero elements were requested, return an empty array without allocating */` |
|       233 | 6094 | `	if( nEntry == 0 ){` |
|         7 | 6095 | `		ph7_result_value(pCtx, ph7_context_new_array(pCtx));` |
|         7 | 6096 | `		return PH7_OK;` |
|         - | 6097 | `	}` |
|         - | 6098 |  |
|         - | 6099 | `	/* Create a new array */` |
|       227 | 6100 | `	pArray = ph7_context_new_array(pCtx);` |
|       227 | 6101 | `	if( pArray == 0 ){` |
|       ! 0 | 6102 | `		return PH7_ContextMemoryError(pCtx);` |
|         - | 6103 | `	}` |
|         - | 6104 |  |
|         - | 6105 | `	/* PHP 8 fills consecutive integer keys start_index, start_index+1, … even` |
|         - | 6106 | `	 * when start_index is negative (PHP 7 restarted the remaining keys from 0,` |
|         - | 6107 | `	 * so array_fill(-5,3) gave -5,0,1 instead of -5,-4,-3). Assign each key` |
|         - | 6108 | `	 * explicitly rather than relying on automatic (append) indexing. */` |
|       227 | 6109 | `	int iStart = ph7_value_to_int(apArg[0]);` |
|   2117831 | 6110 | `	for( i = 0 ; i < nEntry ; i++ ){` |
|   2117605 | 6111 | `		if( ph7_array_add_intkey_elem(pArray, iStart + i, apArg[2]) != SXRET_OK ){` |
|         - | 6112 | `			/* Allocation failure: surface a fatal instead of a partial array */` |
|       ! 0 | 6113 | `			return PH7_ContextMemoryError(pCtx);` |
|         - | 6114 | `		}` |
|   1058803 | 6115 | `	}` |
|         - | 6116 | `	/* Return the filled array */` |
|       227 | 6117 | `	ph7_result_value(pCtx, pArray);` |
|       227 | 6118 | `	return PH7_OK;` |
|       127 | 6119 | `}` |
|         - | 6120 | `/*` |
|         - | 6121 | ` * array array_fill_keys(array $input,mixed $value)` |
|         - | 6122 | ` *  Fill an array with values, specifying keys.` |
|         - | 6123 | ` * Parameters` |
|         - | 6124 | ` *  $input` |
|         - | 6125 | ` *   Array of values that will be used as key.` |
|         - | 6126 | ` *  $value` |
|         - | 6127 | ` *    Value to use for filling.` |
|         - | 6128 | ` * Return` |
|         - | 6129 | ` *  The filled array.` |
|         - | 6130 | ` * Throws` |
|         - | 6131 | ` *  ValueError if $input is not an array.` |
|         - | 6132 | ` */` |
|        26 | 6133 | `static int ph7_hashmap_fill_keys(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|         5 | 6134 | `{` |
|         - | 6135 | `	ph7_hashmap_node *pEntry;` |
|         - | 6136 | `	ph7_hashmap *pSrc;` |
|         - | 6137 | `	ph7_value *pArray;` |
|         - | 6138 | `	sxu32 n;` |
|         - | 6139 | `	/* PHP enforces exactly 2 arguments. */` |
|        31 | 6140 | `	if( nArg != 2 ){` |
|        12 | 6141 | `		return PH7_VmThrowException(pCtx,` |
|         - | 6142 | `			"ArgumentCountError",` |
|         - | 6143 | `			"array_fill_keys() expects exactly 2 arguments, %d given",` |
|         3 | 6144 | `			nArg` |
|         - | 6145 | `			);` |
|         - | 6146 | `	}` |
|         - | 6147 | `	/* Make sure we are dealing with a valid hashmap */` |
|        23 | 6148 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|         8 | 6149 | `		return PH7_VmThrowException(pCtx,` |
|         - | 6150 | `			"TypeError",` |
|         - | 6151 | `			"array_fill_keys(): Argument #1 ($keys) must be of type array, %s given",` |
|         2 | 6152 | `			ph7_type_name(apArg[0])` |
|         - | 6153 | `			);` |
|         - | 6154 | `	}` |
|         - | 6155 | `	/* Point to the internal representation of the input hashmap */` |
|        17 | 6156 | `	pSrc = (ph7_hashmap *)apArg[0]->x.pOther;` |
|         - | 6157 | `	/* Create a new array */` |
|        17 | 6158 | `	pArray = ph7_context_new_array(pCtx);` |
|        17 | 6159 | `	if( pArray == 0 ){` |
|       ! 0 | 6160 | `		ph7_result_null(pCtx);` |
|       ! 0 | 6161 | `		return PH7_OK;` |
|         - | 6162 | `	}` |
|         - | 6163 | `	/* Perform the requested operation */` |
|        17 | 6164 | `	pEntry = pSrc->pFirst;` |
|        45 | 6165 | `	for( n = 0 ; n < pSrc->nEntry ; n++ ){` |
|        29 | 6166 | `		ph7_array_add_elem(pArray,HashmapExtractNodeValue(pEntry),apArg[1]);` |
|         - | 6167 | `		/* Point to the next entry */` |
|        29 | 6168 | `		pEntry = pEntry->pPrev; /* Reverse link */` |
|        15 | 6169 | `	}` |
|         - | 6170 | `	/* Return the filled array */` |
|        17 | 6171 | `	ph7_result_value(pCtx,pArray);` |
|        17 | 6172 | `	return PH7_OK;` |
|        18 | 6173 | `}` |
|         - | 6174 | `/*` |
|         - | 6175 | ` * array array_combine(array $keys,array $values)` |
|         - | 6176 | ` *  Creates an array by using one array for keys and another for its values.` |
|         - | 6177 | ` * Parameters` |
|         - | 6178 | ` *  $keys` |
|         - | 6179 | ` *    Array of keys to be used.` |
|         - | 6180 | ` * $values` |
|         - | 6181 | ` *   Array of values to be used.` |
|         - | 6182 | ` * Return` |
|         - | 6183 | ` *  Returns the combined array. Otherwise FALSE if the number of elements` |
|         - | 6184 | ` *  for each array isn't equal or if one of the given arguments is` |
|         - | 6185 | ` *  not an array.` |
|         - | 6186 | ` */` |
|        18 | 6187 | `static int ph7_hashmap_combine(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|         5 | 6188 | `{` |
|         - | 6189 | `	ph7_hashmap_node *pKe,*pVe;` |
|         - | 6190 | `	ph7_hashmap *pKey,*pValue;` |
|         - | 6191 | `	ph7_value *pArray;` |
|         - | 6192 | `	sxu32 n;` |
|         - | 6193 | `	/* PHP enforces argument count and type checks. */` |
|        23 | 6194 | `	if( nArg != 2 ){` |
|         - | 6195 | `		/* wrong number of arguments -> ArgumentCountError */` |
|         4 | 6196 | `		return PH7_VmThrowException(pCtx,` |
|         - | 6197 | `			"ArgumentCountError",` |
|         - | 6198 | `			"array_combine() expects exactly 2 arguments, %d given",` |
|         1 | 6199 | `			nArg` |
|         - | 6200 | `			);` |
|         - | 6201 | `	}` |
|         - | 6202 | `	/* Validate argument types individually so we can report the correct` |
|         - | 6203 | `	 * argument index in the error message. */` |
|        20 | 6204 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|         4 | 6205 | `		return PH7_VmThrowException(pCtx,` |
|         - | 6206 | `			"TypeError",` |
|         - | 6207 | `			"array_combine(): Argument #1 ($keys) must be of type array, %s given",` |
|         1 | 6208 | `			ph7_type_name(apArg[0])` |
|         - | 6209 | `			);` |
|         - | 6210 | `	}` |
|        17 | 6211 | `	if( !ph7_value_is_array(apArg[1]) ){` |
|         4 | 6212 | `		return PH7_VmThrowException(pCtx,` |
|         - | 6213 | `			"TypeError",` |
|         - | 6214 | `			"array_combine(): Argument #2 ($values) must be of type array, %s given",` |
|         2 | 6215 | `			ph7_type_name(apArg[1])` |
|         - | 6216 | `			);` |
|         - | 6217 | `	}` |
|         - | 6218 | `	/* Point to the internal representation of the input hashmaps */` |
|        14 | 6219 | `	pKey   = (ph7_hashmap *)apArg[0]->x.pOther;` |
|        14 | 6220 | `	pValue = (ph7_hashmap *)apArg[1]->x.pOther;` |
|        14 | 6221 | `	if( pKey->nEntry != pValue->nEntry ){` |
|         - | 6222 | `		/* Length mismatch -> ValueError */` |
|         3 | 6223 | `		return PH7_VmThrowException(pCtx,` |
|         - | 6224 | `			"ValueError",` |
|         - | 6225 | `			"array_combine(): Argument #1 ($keys) and argument #2 ($values) must have the same number of elements"` |
|         - | 6226 | `			);` |
|         - | 6227 | `	}` |
|         - | 6228 | `	/* Create a new array */` |
|        11 | 6229 | `	pArray = ph7_context_new_array(pCtx);` |
|        11 | 6230 | `	if( pArray == 0 ){` |
|       ! 0 | 6231 | `		ph7_result_bool(pCtx,0);` |
|       ! 0 | 6232 | `		return PH7_OK;` |
|         - | 6233 | `	}` |
|         - | 6234 | `	/* Perform the requested operation */` |
|        11 | 6235 | `	pKe = pKey->pFirst;` |
|        11 | 6236 | `	pVe = pValue->pFirst;` |
|        33 | 6237 | `	for( n = 0 ; n < pKey->nEntry ; n++ ){` |
|        23 | 6238 | `		ph7_value *pKeyVal = HashmapExtractNodeValue(pKe);` |
|        23 | 6239 | `		ph7_value *pValVal = HashmapExtractNodeValue(pVe);` |
|         - | 6240 | `		/* PHP treats floats used as keys in array_combine differently than` |
|         - | 6241 | `		 * ordinary offset access: the float is stringified rather than` |
|         - | 6242 | `		 * truncated.  To emulate this behavior we create a temporary copy of` |
|         - | 6243 | `		 * the value when it is a float and convert the copy to string.  The` |
|         - | 6244 | `		 * original array must not be mutated. */` |
|        23 | 6245 | `		ph7_value *pKeyCopy = pKeyVal;` |
|        23 | 6246 | `		if( ph7_value_is_float(pKeyVal) ){` |
|         5 | 6247 | `			ph7_value *pTmpKey = ph7_context_new_scalar(pCtx);` |
|         5 | 6248 | `			if( pTmpKey ){` |
|         5 | 6249 | `				PH7_MemObjStore(pKeyVal,pTmpKey);` |
|         - | 6250 | `				/* Convert copy to string so it becomes "1.5" or "2" etc. */` |
|         5 | 6251 | `				PH7_MemObjToString(pTmpKey);` |
|         5 | 6252 | `				pKeyCopy = pTmpKey;` |
|         2 | 6253 | `			}` |
|         2 | 6254 | `		}` |
|        23 | 6255 | `		ph7_array_add_elem(pArray,pKeyCopy,pValVal);` |
|         - | 6256 | `		/* Point to the next entry */` |
|        23 | 6257 | `		pKe = pKe->pPrev; /* Reverse link */` |
|        23 | 6258 | `		pVe = pVe->pPrev;` |
|        12 | 6259 | `	}` |
|         - | 6260 | `	/* Return the filled array */` |
|        11 | 6261 | `	ph7_result_value(pCtx,pArray);` |
|        11 | 6262 | `	return PH7_OK;` |
|        14 | 6263 | `}` |
|         - | 6264 | `/*` |
|         - | 6265 | ` * array array_reverse(array $array [,bool $preserve_keys = false ])` |
|         - | 6266 | ` *  Return an array with elements in reverse order.` |
|         - | 6267 | ` * Parameters` |
|         - | 6268 | ` *  $array` |
|         - | 6269 | ` *   The input array.` |
|         - | 6270 | ` *  $preserve_keys (optional)` |
|         - | 6271 | ` *   If set to TRUE keys are preserved.` |
|         - | 6272 | ` * Return` |
|         - | 6273 | ` *  The reversed array.` |
|         - | 6274 | ` */` |
|        20 | 6275 | `static int ph7_hashmap_reverse(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|         3 | 6276 | `{` |
|         - | 6277 | `	ph7_hashmap_node *pEntry;` |
|         - | 6278 | `	ph7_hashmap *pSrc;` |
|         - | 6279 | `	ph7_value *pArray;` |
|         - | 6280 | `	int bPreserve;` |
|         - | 6281 | `	sxu32 n;` |
|        23 | 6282 | `	if( nArg < 1 ){` |
|         4 | 6283 | `		return PH7_VmThrowException(pCtx,` |
|         - | 6284 | `			"ArgumentCountError",` |
|         - | 6285 | `			"array_reverse() expects at least 1 argument, %d given",` |
|         1 | 6286 | `			nArg` |
|         - | 6287 | `			);` |
|         - | 6288 | `	}` |
|         - | 6289 | `	/* Make sure we are dealing with a valid hashmap */` |
|        20 | 6290 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|         4 | 6291 | `		return PH7_VmThrowException(pCtx,` |
|         - | 6292 | `			"TypeError",` |
|         - | 6293 | `			"array_reverse(): Argument #1 ($array) must be of type array, %s given",` |
|         1 | 6294 | `			ph7_type_name(apArg[0])` |
|         - | 6295 | `			);` |
|         - | 6296 | `	}` |
|        17 | 6297 | `	bPreserve = FALSE;` |
|        17 | 6298 | `	if( nArg > 1 ){` |
|         7 | 6299 | `		bPreserve = ph7_value_to_bool(apArg[1]);` |
|         3 | 6300 | `	}` |
|         - | 6301 | `	/* Point to the internal representation of the input hashmap */` |
|        17 | 6302 | `	pSrc = (ph7_hashmap *)apArg[0]->x.pOther;` |
|         - | 6303 | `	/* Create a new array */` |
|        17 | 6304 | `	pArray = ph7_context_new_array(pCtx);` |
|        17 | 6305 | `	if( pArray == 0 ){` |
|       ! 0 | 6306 | `		ph7_result_null(pCtx);` |
|       ! 0 | 6307 | `		return PH7_OK;` |
|         - | 6308 | `	}` |
|         - | 6309 | `	/* Perform the requested operation */` |
|        17 | 6310 | `	pEntry = pSrc->pLast;` |
|        55 | 6311 | `	for( n = 0 ; n < pSrc->nEntry ; n++ ){` |
|         - | 6312 | `		/* String keys are always preserved; preserve_keys only affects int keys */` |
|        39 | 6313 | `		int bKeep = (pEntry->iType == HASHMAP_INT_NODE) ? bPreserve : TRUE;` |
|        39 | 6314 | `		HashmapInsertNode((ph7_hashmap *)pArray->x.pOther,pEntry,bKeep);` |
|         - | 6315 | `		/* Point to the previous entry */` |
|        39 | 6316 | `		pEntry = pEntry->pNext; /* Reverse link */` |
|        20 | 6317 | `	}` |
|        17 | 6318 | `	ph7_result_value(pCtx,pArray);` |
|        17 | 6319 | `	return PH7_OK;` |
|        13 | 6320 | `}` |
|         - | 6321 | `/*` |
|         - | 6322 | ` * array array_unique(array $array, int $flags = SORT_STRING)` |
|         - | 6323 | ` *  Removes duplicate values from an array.` |
|         - | 6324 | ` * Parameters` |
|         - | 6325 | ` *  $array` |
|         - | 6326 | ` *   The input array.` |
|         - | 6327 | ` *  $flags` |
|         - | 6328 | ` *   The optional second parameter may be used to modify the comparison` |
|         - | 6329 | ` *   behavior using these values:` |
|         - | 6330 | ` *     SORT_REGULAR - compare items normally (don't change types)` |
|         - | 6331 | ` *     SORT_NUMERIC - compare items numerically` |
|         - | 6332 | ` *     SORT_STRING  - compare items as strings` |
|         - | 6333 | ` * Return` |
|         - | 6334 | ` *  The filtered array.` |
|         - | 6335 | ` */` |
|        24 | 6336 | `static int ph7_hashmap_unique(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|         4 | 6337 | `{` |
|         - | 6338 | `	ph7_hashmap_node *pEntry;` |
|         - | 6339 | `	ph7_value *pNeedle;` |
|         - | 6340 | `	ph7_hashmap *pSrc;` |
|         - | 6341 | `	ph7_value *pArray;` |
|         - | 6342 | `	int bStrict;` |
|         - | 6343 | `	sxi32 rc;` |
|         - | 6344 | `	sxu32 n;` |
|        28 | 6345 | `	if( nArg < 1 ){` |
|         - | 6346 | `		/* Missing arguments, throw ArgumentCountError */` |
|         3 | 6347 | `		return PH7_VmThrowException(pCtx,` |
|         - | 6348 | `			"ArgumentCountError",` |
|         - | 6349 | `			"array_unique() expects at least 1 argument, 0 given"` |
|         - | 6350 | `			);` |
|         - | 6351 | `	}` |
|        25 | 6352 | `	if( nArg > 2 ){` |
|         - | 6353 | `		/* Too many arguments, throw ArgumentCountError */` |
|         4 | 6354 | `		return PH7_VmThrowException(pCtx,` |
|         - | 6355 | `			"ArgumentCountError",` |
|         - | 6356 | `			"array_unique() expects at most 2 arguments, %d given",` |
|         1 | 6357 | `			nArg` |
|         - | 6358 | `			);` |
|         - | 6359 | `	}` |
|         - | 6360 | `	/* Make sure we are dealing with a valid hashmap */` |
|        22 | 6361 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|         - | 6362 | `		/* Type mismatch, throw TypeError */` |
|         4 | 6363 | `		return PH7_VmThrowException(pCtx,` |
|         - | 6364 | `			"TypeError",` |
|         - | 6365 | `			"array_unique(): Argument #1 ($array) must be of type array, %s given",` |
|         1 | 6366 | `			ph7_type_name(apArg[0])` |
|         - | 6367 | `			);` |
|         - | 6368 | `	}` |
|        19 | 6369 | `	bStrict = FALSE;` |
|         - | 6370 | `	/* Point to the internal representation of the input hashmap */` |
|        19 | 6371 | `	pSrc = (ph7_hashmap *)apArg[0]->x.pOther;` |
|         - | 6372 | `	/* Create a new array */` |
|        19 | 6373 | `	pArray = ph7_context_new_array(pCtx);` |
|        19 | 6374 | `	if( pArray == 0 ){` |
|       ! 0 | 6375 | `		ph7_result_null(pCtx);` |
|       ! 0 | 6376 | `		return PH7_OK;` |
|         - | 6377 | `	}` |
|         - | 6378 | `	/* Perform the requested operation */` |
|        19 | 6379 | `	pEntry = pSrc->pFirst;` |
|        83 | 6380 | `	for( n = 0 ; n < pSrc->nEntry ; n++ ){` |
|        65 | 6381 | `		pNeedle = HashmapExtractNodeValue(pEntry);` |
|        65 | 6382 | `		rc = SXERR_NOTFOUND;` |
|        65 | 6383 | `		if( pNeedle ){` |
|        65 | 6384 | `			rc = HashmapFindValue((ph7_hashmap *)pArray->x.pOther,pNeedle,0,bStrict);` |
|        32 | 6385 | `		}` |
|        65 | 6386 | `		if( rc != SXRET_OK ){` |
|         - | 6387 | `			/* Perform the insertion */` |
|        37 | 6388 | `			HashmapInsertNode((ph7_hashmap *)pArray->x.pOther,pEntry,TRUE);` |
|        18 | 6389 | `		}` |
|         - | 6390 | `		/* Point to the next entry */` |
|        65 | 6391 | `		pEntry = pEntry->pPrev; /* Reverse link */` |
|        33 | 6392 | `	}` |
|         - | 6393 | `	/* Return the freshly created array */` |
|        19 | 6394 | `	ph7_result_value(pCtx,pArray);` |
|        19 | 6395 | `	return PH7_OK;` |
|        16 | 6396 | `}` |
|         - | 6397 | `/*` |
|         - | 6398 | ` * array array_flip(array $input)` |
|         - | 6399 | ` *  Exchanges all keys with their associated values in an array.` |
|         - | 6400 | ` * Parameter` |
|         - | 6401 | ` *  $input` |
|         - | 6402 | ` *   Input array.` |
|         - | 6403 | ` * Return` |
|         - | 6404 | ` *   The flipped array on success or NULL on failure.` |
|         - | 6405 | ` */` |
|        34 | 6406 | `static int ph7_hashmap_flip(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|         5 | 6407 | `{` |
|         - | 6408 | `	ph7_hashmap_node *pEntry;` |
|         - | 6409 | `	ph7_hashmap *pSrc;` |
|         - | 6410 | `	ph7_value *pArray;` |
|         - | 6411 | `	ph7_value *pKey;` |
|         - | 6412 | `	ph7_value sVal;` |
|         - | 6413 | `	sxu32 n;` |
|         - | 6414 |  |
|         - | 6415 | `	/* PHP requires exactly one argument */` |
|        39 | 6416 | `	if( nArg != 1 ){` |
|         - | 6417 | `		/* Use ArgumentCountError like other array helpers */` |
|         8 | 6418 | `		return PH7_VmThrowException(pCtx,` |
|         - | 6419 | `			"ArgumentCountError",` |
|         - | 6420 | `			"array_flip() expects exactly 1 argument, %d given",` |
|         2 | 6421 | `			nArg` |
|         - | 6422 | `			);` |
|         - | 6423 | `	}` |
|         - | 6424 | `	/* Make sure we are dealing with a valid hashmap */` |
|        33 | 6425 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|         - | 6426 | `		/* Type mismatch -> TypeError */` |
|         8 | 6427 | `		return PH7_VmThrowException(pCtx,` |
|         - | 6428 | `			"TypeError",` |
|         - | 6429 | `			"array_flip(): Argument #1 ($array) must be of type array, %s given",` |
|         2 | 6430 | `			ph7_type_name(apArg[0])` |
|         - | 6431 | `			);` |
|         - | 6432 | `	}` |
|         - | 6433 | `	/* Point to the internal representation of the input hashmap */` |
|        27 | 6434 | `	pSrc = (ph7_hashmap *)apArg[0]->x.pOther;` |
|         - | 6435 | `	/* Create a new array */` |
|        27 | 6436 | `	pArray = ph7_context_new_array(pCtx);` |
|        27 | 6437 | `	if( pArray == 0 ){` |
|       ! 0 | 6438 | `		ph7_result_null(pCtx);` |
|       ! 0 | 6439 | `		return PH7_OK;` |
|         - | 6440 | `	}` |
|         - | 6441 | `	/* Start processing */` |
|        27 | 6442 | `	pEntry = pSrc->pFirst;` |
|     22263 | 6443 | `	for( n = 0 ; n < pSrc->nEntry ; n++ ){` |
|         - | 6444 | `		/* Extract the node value (will become a key in the result) */` |
|     22237 | 6445 | `		pKey = HashmapExtractNodeValue(pEntry);` |
|     22237 | 6446 | `		if( pKey ){` |
|         - | 6447 | `			/* NULL values are not valid keys either, PHP emits a warning */` |
|     22237 | 6448 | `			if( pKey->iFlags & MEMOBJ_NULL ){` |
|         3 | 6449 | `				PH7_VmThrowError(pCtx->pVm,0,PH7_CTX_WARNING,` |
|         - | 6450 | `					"array_flip(): Can only flip string and integer values, entry skipped"` |
|         - | 6451 | `					);` |
|     22236 | 6452 | `			}else if( (pKey->iFlags & MEMOBJ_INT) \|\| (pKey->iFlags & MEMOBJ_STRING) ){` |
|         - | 6453 | `				/* Prepare the value for insertion (original key) */` |
|     22227 | 6454 | `				if( pEntry->iType == HASHMAP_INT_NODE ){` |
|     20001 | 6455 | `					PH7_MemObjInitFromInt(pSrc->pVm,&sVal,pEntry->xKey.iKey);` |
|     10001 | 6456 | `				}else{` |
|         - | 6457 | `					SyString sStr;` |
|      2227 | 6458 | `					SyStringInitFromBuf(&sStr,SyBlobData(&pEntry->xKey.sKey),SyBlobLength(&pEntry->xKey.sKey));` |
|      2227 | 6459 | `					PH7_MemObjInitFromString(pSrc->pVm,&sVal,&sStr);` |
|         - | 6460 | `				}` |
|         - | 6461 | `				/* Perform the insertion */` |
|     22227 | 6462 | `				ph7_array_add_elem(pArray,pKey,&sVal);` |
|         - | 6463 | `				/* Safely release the value because each inserted entry` |
|         - | 6464 | `				 * has its own private copy of the value.` |
|         - | 6465 | `				 */` |
|     22227 | 6466 | `				PH7_MemObjRelease(&sVal);` |
|     11114 | 6467 | `			}else{` |
|         - | 6468 | `				/* Unsupported value type -> emit warning and skip the entry */` |
|         9 | 6469 | `				PH7_VmThrowError(pCtx->pVm,0,PH7_CTX_WARNING,` |
|         - | 6470 | `					"array_flip(): Can only flip string and integer values, entry skipped"` |
|         - | 6471 | `					);` |
|         - | 6472 | `			}` |
|     11118 | 6473 | `		}` |
|         - | 6474 | `		/* Point to the next entry */` |
|     22237 | 6475 | `		pEntry = pEntry->pPrev; /* Reverse link */` |
|     11119 | 6476 | `	}` |
|         - | 6477 | `	/* Return the freshly created array */` |
|        27 | 6478 | `	ph7_result_value(pCtx,pArray);` |
|        27 | 6479 | `	return PH7_OK;` |
|        22 | 6480 | `}` |
|         - | 6481 | `/*` |
|         - | 6482 | ` * number array_sum(array $array )` |
|         - | 6483 | ` *  Calculate the sum of values in an array.` |
|         - | 6484 | ` * Parameters` |
|         - | 6485 | ` *  $array: The input array.` |
|         - | 6486 | ` * Return` |
|         - | 6487 | ` *  Returns the sum of values as an integer or float.` |
|         - | 6488 | ` */` |
|        24 | 6489 | `static void DoubleSum(ph7_context *pCtx,ph7_hashmap *pMap)` |
|         1 | 6490 | `{` |
|         - | 6491 | `	ph7_hashmap_node *pEntry;` |
|         - | 6492 | `	ph7_value *pObj;` |
|        25 | 6493 | `	double dSum = 0;` |
|         - | 6494 | `	sxu32 n;` |
|        25 | 6495 | `	pEntry = pMap->pFirst;` |
|        91 | 6496 | `	for( n = 0 ; n < pMap->nEntry ; n++ ){` |
|        67 | 6497 | `		pObj = HashmapExtractNodeValue(pEntry);` |
|        67 | 6498 | `		if( pObj ){` |
|        67 | 6499 | `			if( pObj->iFlags & MEMOBJ_REAL ){` |
|        29 | 6500 | `				dSum += pObj->rVal;` |
|        53 | 6501 | `			}else if( pObj->iFlags & (MEMOBJ_INT\|MEMOBJ_BOOL) ){` |
|        21 | 6502 | `				dSum += (double)pObj->x.iVal;` |
|        29 | 6503 | `			}else if( pObj->iFlags & MEMOBJ_STRING ){` |
|        15 | 6504 | `				if( SyBlobLength(&pObj->sBlob) > 0 ){` |
|        13 | 6505 | `					double dv = 0;` |
|        13 | 6506 | `					SyStrToReal((const char *)SyBlobData(&pObj->sBlob),SyBlobLength(&pObj->sBlob),(void *)&dv,0);` |
|        13 | 6507 | `					dSum += dv;` |
|         7 | 6508 | `				}` |
|        12 | 6509 | `			}else if( pObj->iFlags & MEMOBJ_HASHMAP ){` |
|         3 | 6510 | `				PH7_VmThrowError(pCtx->pVm,0,PH7_CTX_WARNING,` |
|         - | 6511 | `					"array_sum(): Addition is not supported on type array");` |
|         4 | 6512 | `			}else if( pObj->iFlags & MEMOBJ_OBJ ){` |
|       ! 0 | 6513 | `				PH7_VmThrowError(pCtx->pVm,0,PH7_CTX_WARNING,` |
|         - | 6514 | `					"array_sum(): Addition is not supported on type object");` |
|         3 | 6515 | `			}else if( pObj->iFlags & MEMOBJ_RES ){` |
|       ! 0 | 6516 | `				PH7_VmThrowError(pCtx->pVm,0,PH7_CTX_WARNING,` |
|         - | 6517 | `					"array_sum(): Addition is not supported on type resource");` |
|       ! 0 | 6518 | `			}` |
|         - | 6519 | `			/* NULL is silently treated as 0 (matches PHP) */` |
|        33 | 6520 | `		}` |
|         - | 6521 | `		/* Point to the next entry */` |
|        67 | 6522 | `		pEntry = pEntry->pPrev; /* Reverse link */` |
|        34 | 6523 | `	}` |
|         - | 6524 | `	/* Return sum */` |
|        25 | 6525 | `	ph7_result_double(pCtx,dSum);` |
|        25 | 6526 | `}` |
|        34 | 6527 | `static void Int64Sum(ph7_context *pCtx,ph7_hashmap *pMap)` |
|         2 | 6528 | `{` |
|         - | 6529 | `	ph7_hashmap_node *pEntry;` |
|         - | 6530 | `	ph7_value *pObj;` |
|        36 | 6531 | `	sxi64 nSum = 0;` |
|         - | 6532 | `	sxu32 n;` |
|        36 | 6533 | `	pEntry = pMap->pFirst;` |
|       144 | 6534 | `	for( n = 0 ; n < pMap->nEntry ; n++ ){` |
|       110 | 6535 | `		pObj = HashmapExtractNodeValue(pEntry);` |
|       110 | 6536 | `		if( pObj ){` |
|       110 | 6537 | `			if( pObj->iFlags & (MEMOBJ_INT\|MEMOBJ_BOOL) ){` |
|       100 | 6538 | `				nSum += pObj->x.iVal;` |
|        60 | 6539 | `			}else if( pObj->iFlags & MEMOBJ_STRING ){` |
|         7 | 6540 | `				if( SyBlobLength(&pObj->sBlob) > 0 ){` |
|         5 | 6541 | `					sxi64 nv = 0;` |
|         5 | 6542 | `					SyStrToInt64((const char *)SyBlobData(&pObj->sBlob),SyBlobLength(&pObj->sBlob),(void *)&nv,0);` |
|         5 | 6543 | `					nSum += nv;` |
|         3 | 6544 | `				}` |
|         8 | 6545 | `			}else if( pObj->iFlags & MEMOBJ_HASHMAP ){` |
|         3 | 6546 | `				PH7_VmThrowError(pCtx->pVm,0,PH7_CTX_WARNING,` |
|         - | 6547 | `					"array_sum(): Addition is not supported on type array");` |
|         4 | 6548 | `			}else if( pObj->iFlags & MEMOBJ_OBJ ){` |
|       ! 0 | 6549 | `				PH7_VmThrowError(pCtx->pVm,0,PH7_CTX_WARNING,` |
|         - | 6550 | `					"array_sum(): Addition is not supported on type object");` |
|         3 | 6551 | `			}else if( pObj->iFlags & MEMOBJ_RES ){` |
|       ! 0 | 6552 | `				PH7_VmThrowError(pCtx->pVm,0,PH7_CTX_WARNING,` |
|         - | 6553 | `					"array_sum(): Addition is not supported on type resource");` |
|       ! 0 | 6554 | `			}` |
|         - | 6555 | `			/* NULL is silently treated as 0 (matches PHP) */` |
|        54 | 6556 | `		}` |
|         - | 6557 | `		/* Point to the next entry */` |
|       110 | 6558 | `		pEntry = pEntry->pPrev; /* Reverse link */` |
|        56 | 6559 | `	}` |
|         - | 6560 | `	/* Return sum */` |
|        36 | 6561 | `	ph7_result_int64(pCtx,nSum);` |
|        36 | 6562 | `}` |
|         - | 6563 | `/* number array_sum(array $array )` |
|         - | 6564 | ` * (See block-coment above)` |
|         - | 6565 | ` */` |
|        72 | 6566 | `static int ph7_hashmap_sum(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|         5 | 6567 | `{` |
|         - | 6568 | `	ph7_hashmap_node *pEntry;` |
|         - | 6569 | `	ph7_hashmap *pMap;` |
|         - | 6570 | `	ph7_value *pObj;` |
|        77 | 6571 | `	int useDouble = 0;` |
|         - | 6572 | `	sxu32 n;` |
|         - | 6573 | `	/* PHP requires exactly one argument */` |
|        77 | 6574 | `	if( nArg != 1 ){` |
|         8 | 6575 | `		return PH7_VmThrowException(pCtx,` |
|         - | 6576 | `			"ArgumentCountError",` |
|         - | 6577 | `			"array_sum() expects exactly 1 argument, %d given",` |
|         2 | 6578 | `			nArg` |
|         - | 6579 | `			);` |
|         - | 6580 | `	}` |
|         - | 6581 | `	/* Make sure we are dealing with a valid hashmap */` |
|        71 | 6582 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|         - | 6583 | `		/* Type mismatch -> TypeError (php's true/false/class-name convention). */` |
|         - | 6584 | `		char zBuf[64];` |
|         8 | 6585 | `		return PH7_VmThrowException(pCtx,` |
|         - | 6586 | `			"TypeError",` |
|         - | 6587 | `			"array_sum(): Argument #1 ($array) must be of type array, %s given",` |
|         2 | 6588 | `			VmValueGivenName(apArg[0],zBuf,sizeof(zBuf))` |
|         - | 6589 | `			);` |
|         - | 6590 | `	}` |
|        66 | 6591 | `	pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|        66 | 6592 | `	if( pMap->nEntry < 1 ){` |
|         - | 6593 | `		/* Nothing to compute,return 0 */` |
|         7 | 6594 | `		ph7_result_int(pCtx,0);` |
|         7 | 6595 | `		return PH7_OK;` |
|         - | 6596 | `	}` |
|         - | 6597 | `	/* Scan all elements: if any value is a float, use floating-point` |
|         - | 6598 | `	 * arithmetic for the entire sum (matches PHP behaviour).` |
|         - | 6599 | `	 */` |
|        60 | 6600 | `	pEntry = pMap->pFirst;` |
|       176 | 6601 | `	for( n = 0 ; n < pMap->nEntry ; n++ ){` |
|       142 | 6602 | `		pObj = HashmapExtractNodeValue(pEntry);` |
|       142 | 6603 | `		if( pObj ){` |
|       142 | 6604 | `			if( pObj->iFlags & MEMOBJ_REAL ){` |
|        19 | 6605 | `				useDouble = 1;` |
|        19 | 6606 | `				break;` |
|         - | 6607 | `			}` |
|       124 | 6608 | `			if( pObj->iFlags & MEMOBJ_STRING ){` |
|        13 | 6609 | `				const char *zStr = (const char *)SyBlobData(&pObj->sBlob);` |
|        13 | 6610 | `				sxu32 nLen = SyBlobLength(&pObj->sBlob);` |
|         - | 6611 | `				sxu32 i;` |
|        23 | 6612 | `				for( i = 0 ; i < nLen ; i++ ){` |
|        17 | 6613 | `					if( zStr[i] == '.' \|\| zStr[i] == 'e' \|\| zStr[i] == 'E' ){` |
|         7 | 6614 | `						useDouble = 1;` |
|         7 | 6615 | `						break;` |
|         - | 6616 | `					}` |
|         6 | 6617 | `				}` |
|        13 | 6618 | `				if( useDouble ){` |
|         7 | 6619 | `					break;` |
|         - | 6620 | `				}` |
|         3 | 6621 | `			}` |
|        58 | 6622 | `		}` |
|       118 | 6623 | `		pEntry = pEntry->pPrev;` |
|        60 | 6624 | `	}` |
|        60 | 6625 | `	if( useDouble ){` |
|        25 | 6626 | `		DoubleSum(pCtx,pMap);` |
|        13 | 6627 | `	}else{` |
|        36 | 6628 | `		Int64Sum(pCtx,pMap);` |
|         - | 6629 | `	}` |
|        60 | 6630 | `	return PH7_OK;` |
|        41 | 6631 | `}` |
|         - | 6632 | `/*` |
|         - | 6633 | ` * number array_product(array $array )` |
|         - | 6634 | ` *  Calculate the product of values in an array.` |
|         - | 6635 | ` * Parameters` |
|         - | 6636 | ` *  $array: The input array.` |
|         - | 6637 | ` * Return` |
|         - | 6638 | ` *  Returns the product of values as an integer or float.` |
|         - | 6639 | ` */` |
|         2 | 6640 | `static void DoubleProd(ph7_context *pCtx,ph7_hashmap *pMap)` |
|         1 | 6641 | `{` |
|         - | 6642 | `	ph7_hashmap_node *pEntry;` |
|         - | 6643 | `	ph7_value *pObj;` |
|         - | 6644 | `	double dProd;` |
|         - | 6645 | `	sxu32 n;` |
|         3 | 6646 | `	pEntry = pMap->pFirst;` |
|         3 | 6647 | `	dProd = 1;` |
|         7 | 6648 | `	for( n = 0 ; n < pMap->nEntry ; n++ ){` |
|         5 | 6649 | `		pObj = HashmapExtractNodeValue(pEntry);` |
|         5 | 6650 | `		if( pObj && (pObj->iFlags & (MEMOBJ_NULL\|MEMOBJ_HASHMAP\|MEMOBJ_OBJ\|MEMOBJ_RES)) == 0){` |
|         5 | 6651 | `			if( pObj->iFlags & MEMOBJ_REAL ){` |
|         3 | 6652 | `				dProd *= pObj->rVal;` |
|         4 | 6653 | `			}else if( pObj->iFlags & (MEMOBJ_INT\|MEMOBJ_BOOL) ){` |
|         3 | 6654 | `				dProd *= (double)pObj->x.iVal;` |
|         1 | 6655 | `			}else if( pObj->iFlags & MEMOBJ_STRING ){` |
|       ! 0 | 6656 | `				if( SyBlobLength(&pObj->sBlob) > 0 ){` |
|       ! 0 | 6657 | `					double dv = 0;` |
|       ! 0 | 6658 | `					SyStrToReal((const char *)SyBlobData(&pObj->sBlob),SyBlobLength(&pObj->sBlob),(void *)&dv,0);` |
|       ! 0 | 6659 | `					dProd *= dv;` |
|       ! 0 | 6660 | `				}` |
|       ! 0 | 6661 | `			}` |
|         2 | 6662 | `		}` |
|         - | 6663 | `		/* Point to the next entry */` |
|         5 | 6664 | `		pEntry = pEntry->pPrev; /* Reverse link */` |
|         3 | 6665 | `	}` |
|         - | 6666 | `	/* Return product */` |
|         3 | 6667 | `	ph7_result_double(pCtx,dProd);` |
|         3 | 6668 | `}` |
|         2 | 6669 | `static void Int64Prod(ph7_context *pCtx,ph7_hashmap *pMap)` |
|         1 | 6670 | `{` |
|         - | 6671 | `	ph7_hashmap_node *pEntry;` |
|         - | 6672 | `	ph7_value *pObj;` |
|         - | 6673 | `	sxi64 nProd;` |
|         - | 6674 | `	sxu32 n;` |
|         3 | 6675 | `	pEntry = pMap->pFirst;` |
|         3 | 6676 | `	nProd = 1;` |
|         9 | 6677 | `	for( n = 0 ; n < pMap->nEntry ; n++ ){` |
|         7 | 6678 | `		pObj = HashmapExtractNodeValue(pEntry);` |
|         7 | 6679 | `		if( pObj && (pObj->iFlags & (MEMOBJ_NULL\|MEMOBJ_HASHMAP\|MEMOBJ_OBJ\|MEMOBJ_RES)) == 0){` |
|         7 | 6680 | `			if( pObj->iFlags & MEMOBJ_REAL ){` |
|       ! 0 | 6681 | `				nProd *= (sxi64)pObj->rVal;` |
|         7 | 6682 | `			}else if( pObj->iFlags & (MEMOBJ_INT\|MEMOBJ_BOOL) ){` |
|         7 | 6683 | `				nProd *= pObj->x.iVal;` |
|         3 | 6684 | `			}else if( pObj->iFlags & MEMOBJ_STRING ){` |
|       ! 0 | 6685 | `				if( SyBlobLength(&pObj->sBlob) > 0 ){` |
|       ! 0 | 6686 | `					sxi64 nv = 0;` |
|       ! 0 | 6687 | `					SyStrToInt64((const char *)SyBlobData(&pObj->sBlob),SyBlobLength(&pObj->sBlob),(void *)&nv,0);` |
|       ! 0 | 6688 | `					nProd *= nv;` |
|       ! 0 | 6689 | `				}` |
|       ! 0 | 6690 | `			}` |
|         3 | 6691 | `		}` |
|         - | 6692 | `		/* Point to the next entry */` |
|         7 | 6693 | `		pEntry = pEntry->pPrev; /* Reverse link */` |
|         4 | 6694 | `	}` |
|         - | 6695 | `	/* Return product */` |
|         3 | 6696 | `	ph7_result_int64(pCtx,nProd);` |
|         3 | 6697 | `}` |
|         - | 6698 | `/* number array_product(array $array )` |
|         - | 6699 | ` * (See block-block comment above)` |
|         - | 6700 | ` */` |
|        18 | 6701 | `static int ph7_hashmap_product(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|         1 | 6702 | `{` |
|         - | 6703 | `	ph7_hashmap *pMap;` |
|         - | 6704 | `	ph7_value *pObj;` |
|        19 | 6705 | `	if( nArg < 1 ){` |
|         - | 6706 | `		/* Missing arguments (arity is enforced upstream; defensive). */` |
|       ! 0 | 6707 | `		ph7_result_int(pCtx,1);` |
|       ! 0 | 6708 | `		return PH7_OK;` |
|         - | 6709 | `	}` |
|         - | 6710 | `	/* PHP 8: a non-array $array is a catchable TypeError, not a silent 0. */` |
|        19 | 6711 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|         - | 6712 | `		char zBuf[64];` |
|        19 | 6713 | `		return PH7_VmThrowException(pCtx,` |
|         - | 6714 | `			"TypeError",` |
|         - | 6715 | `			"array_product(): Argument #1 ($array) must be of type array, %s given",` |
|         6 | 6716 | `			VmValueGivenName(apArg[0],zBuf,sizeof(zBuf))` |
|         - | 6717 | `			);` |
|         - | 6718 | `	}` |
|         7 | 6719 | `	pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|         7 | 6720 | `	if( pMap->nEntry < 1 ){` |
|         - | 6721 | `		/* The product of an empty array is the multiplicative identity 1 (PHP). */` |
|         3 | 6722 | `		ph7_result_int(pCtx,1);` |
|         3 | 6723 | `		return PH7_OK;` |
|         - | 6724 | `	}` |
|         - | 6725 | `	/* If the first element is of type float,then perform floating` |
|         - | 6726 | `	 * point computaion.Otherwise switch to int64 computaion.` |
|         - | 6727 | `	 */` |
|         5 | 6728 | `	pObj = HashmapExtractNodeValue(pMap->pFirst);` |
|         5 | 6729 | `	if( pObj == 0 ){` |
|       ! 0 | 6730 | `		ph7_result_int(pCtx,0);` |
|       ! 0 | 6731 | `		return PH7_OK;` |
|         - | 6732 | `	}` |
|         5 | 6733 | `	if( pObj->iFlags & MEMOBJ_REAL ){` |
|         3 | 6734 | `		DoubleProd(pCtx,pMap);` |
|         2 | 6735 | `	}else{` |
|         3 | 6736 | `		Int64Prod(pCtx,pMap);` |
|         - | 6737 | `	}` |
|         5 | 6738 | `	return PH7_OK;` |
|        10 | 6739 | `}` |
|         - | 6740 | `/*` |
|         - | 6741 | ` * value array_rand(array $input[,int $num_req = 1 ])` |
|         - | 6742 | ` *  Pick one or more random entries out of an array.` |
|         - | 6743 | ` * Parameters` |
|         - | 6744 | ` * $input` |
|         - | 6745 | ` *  The input array.` |
|         - | 6746 | ` * $num_req` |
|         - | 6747 | ` *  Specifies how many entries you want to pick.` |
|         - | 6748 | ` * Return` |
|         - | 6749 | ` *  If you are picking only one entry, array_rand() returns the key for a random entry.` |
|         - | 6750 | ` *  Otherwise, it returns an array of keys for the random entries.` |
|         - | 6751 | ` *  NULL is returned on failure.` |
|         - | 6752 | ` */` |
|        42 | 6753 | `static int ph7_hashmap_rand(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|         1 | 6754 | `{` |
|         - | 6755 | `	ph7_hashmap_node *pNode;` |
|         - | 6756 | `	ph7_hashmap *pMap;` |
|        43 | 6757 | `	int nItem = 1;` |
|        43 | 6758 | `	if( nArg < 1 ){` |
|         - | 6759 | `		/* Missing argument (arity is enforced upstream; defensive) */` |
|       ! 0 | 6760 | `		ph7_result_null(pCtx);` |
|       ! 0 | 6761 | `		return PH7_OK;` |
|         - | 6762 | `	}` |
|         - | 6763 | `	/* php 8: $array must be an array (TypeError, not a silent NULL return) */` |
|        43 | 6764 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|         - | 6765 | `		char zBuf[64];` |
|        10 | 6766 | `		return PH7_VmThrowException(pCtx,` |
|         - | 6767 | `			"TypeError",` |
|         - | 6768 | `			"array_rand(): Argument #1 ($array) must be of type array, %s given",` |
|         3 | 6769 | `			VmValueGivenName(apArg[0],zBuf,sizeof(zBuf))` |
|         - | 6770 | `			);` |
|         - | 6771 | `	}` |
|         - | 6772 | `	/* php validates $num (and weak-coerces it) BEFORE the empty-array body` |
|         - | 6773 | `	 * check, matching its ZPP-before-body ordering. */` |
|        37 | 6774 | `	if( nArg > 1 ){` |
|        29 | 6775 | `		ph7_value *pNum = apArg[1];` |
|        28 | 6776 | `		if( ph7_value_is_array(pNum) \|\| ph7_value_is_object(pNum)` |
|        24 | 6777 | `			\|\| ph7_value_is_resource(pNum) ){` |
|         - | 6778 | `			char zBuf[64];` |
|        10 | 6779 | `			return PH7_VmThrowException(pCtx,` |
|         - | 6780 | `				"TypeError",` |
|         - | 6781 | `				"array_rand(): Argument #2 ($num) must be of type int, %s given",` |
|         3 | 6782 | `				VmValueGivenName(pNum,zBuf,sizeof(zBuf))` |
|         - | 6783 | `				);` |
|         - | 6784 | `		}` |
|        23 | 6785 | `		if( ph7_value_is_string(pNum) ){` |
|         - | 6786 | `			/* Weak int coercion of a string $num follows php's numeric-string` |
|         - | 6787 | `			 * grammar (whole string, int or float): a non-numeric string` |
|         - | 6788 | `			 * (incl. leading-numeric junk like "2abc" or "0x1A") is a TypeError,` |
|         - | 6789 | `			 * a well-formed float-string ("1e3") coerces like a float value.` |
|         - | 6790 | `			 * Reuses the range() ZPP number parser (§3.9 shared-helper note). */` |
|         - | 6791 | `			int len;` |
|         9 | 6792 | `			const char *zStr = ph7_value_to_string(pNum, &len);` |
|         - | 6793 | `			sxi64 iLong; double dReal;` |
|         9 | 6794 | `			sxu8 iKind = RangeStrToNumber(zStr, (sxu32)len, &iLong, &dReal);` |
|         9 | 6795 | `			if( iKind == RANGE_IN_ERROR ){` |
|         7 | 6796 | `				return PH7_VmThrowException(pCtx,` |
|         - | 6797 | `					"TypeError",` |
|         - | 6798 | `					"array_rand(): Argument #2 ($num) must be of type int, string given"` |
|         - | 6799 | `					);` |
|         - | 6800 | `			}` |
|         - | 6801 | `			/* Clamp into a signed-int band so an absurd magnitude still yields` |
|         - | 6802 | `			 * the out-of-range ValueError below without an out-of-int cast. */` |
|         3 | 6803 | `			if( iKind == RANGE_IN_DOUBLE ){` |
|         3 | 6804 | `				iLong = dReal <= 0.0 ? 0 : (dReal >= 2147483647.0 ? 2147483647 : (sxi64)dReal);` |
|         1 | 6805 | `			}` |
|         3 | 6806 | `			if( iLong > 2147483647 ){ iLong = 2147483647; }` |
|         3 | 6807 | `			else if( iLong < -2147483647 ){ iLong = -2147483647; }` |
|         3 | 6808 | `			nItem = (int)iLong;` |
|         2 | 6809 | `		}else{` |
|        15 | 6810 | `			nItem = ph7_value_to_int(pNum);` |
|         - | 6811 | `		}` |
|         8 | 6812 | `	}` |
|         - | 6813 | `	/* Point to the internal representation of the input hashmap */` |
|        25 | 6814 | `	pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|         - | 6815 | `	/* php 8: an empty array is a ValueError, not a NULL return */` |
|        25 | 6816 | `	if( pMap->nEntry < 1 ){` |
|         5 | 6817 | `		return PH7_VmThrowException(pCtx,` |
|         - | 6818 | `			"ValueError",` |
|         - | 6819 | `			"array_rand(): Argument #1 ($array) must not be empty"` |
|         - | 6820 | `			);` |
|         - | 6821 | `	}` |
|         - | 6822 | `	/* php 8: $num outside [1, count] is a ValueError, not a clamp/wrong value */` |
|        21 | 6823 | `	if( nItem < 1 \|\| nItem > (int)pMap->nEntry ){` |
|         9 | 6824 | `		return PH7_VmThrowException(pCtx,` |
|         - | 6825 | `			"ValueError",` |
|         - | 6826 | `			"array_rand(): Argument #2 ($num) must be between 1 and the number of elements in argument #1 ($array)"` |
|         - | 6827 | `			);` |
|         - | 6828 | `	}` |
|        13 | 6829 | `	if( nItem < 2 ){` |
|         - | 6830 | `		sxu32 nEntry;` |
|         - | 6831 | `		/* Select a random number */` |
|         9 | 6832 | `		nEntry = PH7_VmRandomNum(pMap->pVm) % pMap->nEntry;` |
|         - | 6833 | `		/* Extract the desired entry.` |
|         - | 6834 | `		 * Note that we perform a linear lookup here (later version must change this)` |
|         - | 6835 | `		 */` |
|         9 | 6836 | `		if( nEntry > pMap->nEntry / 2 ){` |
|         3 | 6837 | `			pNode = pMap->pLast;` |
|         3 | 6838 | `			nEntry = pMap->nEntry - nEntry;` |
|         3 | 6839 | `			if( nEntry > 1 ){` |
|       ! 0 | 6840 | `				for(;;){` |
|       ! 0 | 6841 | `					if( nEntry == 0 ){` |
|       ! 0 | 6842 | `						break;` |
|         - | 6843 | `					}` |
|         - | 6844 | `					/* Point to the previous entry */` |
|       ! 0 | 6845 | `					pNode = pNode->pNext; /* Reverse link */` |
|       ! 0 | 6846 | `					nEntry--;` |
|       ! 0 | 6847 | `				}` |
|       ! 0 | 6848 | `			}` |
|         2 | 6849 | `		}else{` |
|         6 | 6850 | `			pNode = pMap->pFirst;` |
|         3 | 6851 | `			for(;;){` |
|         8 | 6852 | `				if( nEntry == 0 ){` |
|         6 | 6853 | `					break;` |
|         - | 6854 | `				}` |
|         - | 6855 | `				/* Point to the next entry */` |
|         2 | 6856 | `				pNode = pNode->pPrev; /* Reverse link */` |
|         2 | 6857 | `				nEntry--;` |
|       ! 0 | 6858 | `			}` |
|         - | 6859 | `		}` |
|         9 | 6860 | `		if( pNode->iType == HASHMAP_INT_NODE ){` |
|         - | 6861 | `			/* Int key */` |
|         7 | 6862 | `			ph7_result_int64(pCtx,pNode->xKey.iKey);` |
|         4 | 6863 | `		}else{` |
|         - | 6864 | `			/* Blob key */` |
|         3 | 6865 | `			ph7_result_string(pCtx,(const char *)SyBlobData(&pNode->xKey.sKey),(int)SyBlobLength(&pNode->xKey.sKey));` |
|         - | 6866 | `		}` |
|         5 | 6867 | `	}else{` |
|         - | 6868 | `		ph7_value sKey,*pArray;` |
|         - | 6869 | `		ph7_hashmap *pDest;` |
|         - | 6870 | `		/* Create a new array */` |
|         5 | 6871 | `		pArray = ph7_context_new_array(pCtx);` |
|         5 | 6872 | `		if( pArray == 0 ){` |
|       ! 0 | 6873 | `			ph7_result_null(pCtx);` |
|       ! 0 | 6874 | `			return PH7_OK;` |
|         - | 6875 | `		}` |
|         - | 6876 | `		/* Point to the internal representation of the hashmap */` |
|         5 | 6877 | `		pDest = (ph7_hashmap *)pArray->x.pOther;` |
|         5 | 6878 | `		PH7_MemObjInit(pDest->pVm,&sKey);` |
|         - | 6879 | `		/* Copy the first n items */` |
|         5 | 6880 | `		pNode = pMap->pFirst;` |
|         5 | 6881 | `		if( nItem > (int)pMap->nEntry ){` |
|       ! 0 | 6882 | `			nItem = (int)pMap->nEntry;` |
|       ! 0 | 6883 | `		}` |
|        15 | 6884 | `		while( nItem > 0){` |
|        11 | 6885 | `			PH7_HashmapExtractNodeKey(pNode,&sKey);` |
|        11 | 6886 | `			PH7_HashmapInsert(pDest,0/* Automatic index assign*/,&sKey);` |
|        11 | 6887 | `			PH7_MemObjRelease(&sKey);` |
|         - | 6888 | `			/* Point to the next entry */` |
|        11 | 6889 | `			pNode = pNode->pPrev; /* Reverse link */` |
|        11 | 6890 | `			nItem--;` |
|         1 | 6891 | `		}` |
|         - | 6892 | `		/* Shuffle the array */` |
|         5 | 6893 | `		HashmapMergeSort(pDest,HashmapCmpCallback7,0);` |
|         - | 6894 | `		/* Rehash node */` |
|         5 | 6895 | `		HashmapSortRehash(pDest);` |
|         - | 6896 | `		/* Return the random array */` |
|         5 | 6897 | `		ph7_result_value(pCtx,pArray);` |
|         - | 6898 | `	}` |
|        13 | 6899 | `	return PH7_OK;` |
|        22 | 6900 | `}` |
|         - | 6901 | `/*` |
|         - | 6902 | ` * array array_chunk (array $input,int $size [,bool $preserve_keys = false ])` |
|         - | 6903 | ` *  Split an array into chunks.` |
|         - | 6904 | ` * Parameters` |
|         - | 6905 | ` * $input` |
|         - | 6906 | ` *   The array to work on` |
|         - | 6907 | ` * $size` |
|         - | 6908 | ` *   The size of each chunk` |
|         - | 6909 | ` * $preserve_keys` |
|         - | 6910 | ` *   When set to TRUE keys will be preserved. Default is FALSE which will reindex` |
|         - | 6911 | ` *   the chunk numerically.` |
|         - | 6912 | ` * Return` |
|         - | 6913 | ` *  Returns a multidimensional numerically indexed array, starting with` |
|         - | 6914 | ` *  zero, with each dimension containing size elements.` |
|         - | 6915 | ` */` |
|        42 | 6916 | `static int ph7_hashmap_chunk(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|         5 | 6917 | `{` |
|         - | 6918 | `	ph7_value *pArray,*pChunk;` |
|         - | 6919 | `	ph7_hashmap_node *pEntry;` |
|         - | 6920 | `	ph7_hashmap *pMap;` |
|         - | 6921 | `	int bPreserve;` |
|         - | 6922 | `	sxu32 nChunk;` |
|         - | 6923 | `	sxu32 nSize;` |
|         - | 6924 | `	sxu32 n;` |
|         - | 6925 | `	/* Argument count and types follow PHP semantics. */` |
|        47 | 6926 | `	if( nArg < 2 ){` |
|         - | 6927 | `		/* fewer than required arguments -> ArgumentCountError */` |
|         4 | 6928 | `		return PH7_VmThrowException(pCtx,` |
|         - | 6929 | `			"ArgumentCountError",` |
|         - | 6930 | `			"array_chunk() expects at least 2 arguments, %d given",` |
|         1 | 6931 | `			nArg` |
|         - | 6932 | `			);` |
|         - | 6933 | `	}` |
|        45 | 6934 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|         4 | 6935 | `		return PH7_VmThrowException(pCtx,` |
|         - | 6936 | `			"TypeError",` |
|         - | 6937 | `			"array_chunk(): Argument #1 ($array) must be of type array, %s given",` |
|         1 | 6938 | `			ph7_type_name(apArg[0])` |
|         - | 6939 | `			);` |
|         - | 6940 | `	}` |
|         - | 6941 | `	/* Create a new array */` |
|        43 | 6942 | `	pArray = ph7_context_new_array(pCtx);` |
|        43 | 6943 | `	if( pArray == 0 ){` |
|       ! 0 | 6944 | `		ph7_result_null(pCtx);` |
|       ! 0 | 6945 | `		return PH7_OK;` |
|         - | 6946 | `	}` |
|         - | 6947 | `	/* Point to the internal representation of the input hashmap */` |
|        43 | 6948 | `	pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|         - | 6949 | `	/* Extract and validate the chunk size argument. */` |
|         - | 6950 | `	/* Reject types that cannot be sensibly converted to an integer. */` |
|        57 | 6951 | `	if( ph7_value_is_array(apArg[1]) \|\| ph7_value_is_object(apArg[1]) \|\|` |
|        81 | 6952 | `		ph7_value_is_resource(apArg[1]) \|\| ph7_value_is_null(apArg[1]) \|\|` |
|        38 | 6953 | `		ph7_value_is_bool(apArg[1]) ){` |
|       ! 0 | 6954 | `		return PH7_VmThrowException(pCtx,` |
|         - | 6955 | `			"TypeError",` |
|         - | 6956 | `			"array_chunk(): Argument #2 ($length) must be of type int, %s given",` |
|       ! 0 | 6957 | `			ph7_type_name(apArg[1])` |
|         - | 6958 | `			);` |
|         - | 6959 | `	}` |
|         - | 6960 | `	/* Strings that are non-numeric produce a TypeError.  Numeric` |
|         - | 6961 | `	 * strings are permitted; however those representing floats lose` |
|         - | 6962 | `	 * precision and PHP emits a deprecation warning. */` |
|        43 | 6963 | `	if( ph7_value_is_string(apArg[1]) ){` |
|         - | 6964 | `		int len;` |
|         3 | 6965 | `		sxu8 bReal = FALSE;` |
|         3 | 6966 | `		const char *zStr = ph7_value_to_string(apArg[1], &len);` |
|         3 | 6967 | `		if( SyStrIsNumeric(zStr, len, &bReal, 0) != SXRET_OK ){` |
|         3 | 6968 | `			return PH7_VmThrowException(pCtx,` |
|         - | 6969 | `				"TypeError",` |
|         - | 6970 | `				"array_chunk(): Argument #2 ($length) must be of type int, string given"` |
|         - | 6971 | `				);` |
|         - | 6972 | `		}` |
|       ! 0 | 6973 | `		if( bReal ){` |
|         - | 6974 | `			/* float-string -> warn but allow */` |
|       ! 0 | 6975 | `			ph7_context_throw_error_format(pCtx, E_DEPRECATED,` |
|         - | 6976 | `				"Implicit conversion from float-string \"%s\" to int loses precision",` |
|       ! 0 | 6977 | `				zStr` |
|         - | 6978 | `				);` |
|       ! 0 | 6979 | `		}` |
|       ! 0 | 6980 | `	}` |
|         - | 6981 | `	/* If the value is a float with a fractional component, emit a` |
|         - | 6982 | `	 * deprecation warning but continue.  The following conversion occurs` |
|         - | 6983 | `	 * later via ph7_value_to_int. */` |
|        40 | 6984 | `	if( ph7_value_is_float(apArg[1]) ){` |
|         3 | 6985 | `		double d = ph7_value_to_double(apArg[1]);` |
|         3 | 6986 | `		sxi64 i = (sxi64)d;` |
|         3 | 6987 | `		if( d != (double)i ){` |
|         4 | 6988 | `			ph7_context_throw_error_format(pCtx, E_DEPRECATED,` |
|         - | 6989 | `				"Implicit conversion from float %g to int loses precision",` |
|         1 | 6990 | `				d` |
|         - | 6991 | `				);` |
|         1 | 6992 | `		}` |
|         1 | 6993 | `	}` |
|         - | 6994 | `	/* Convert using ph7_value_to_int; now that float fractions are` |
|         - | 6995 | `	 * eliminated, this will not produce a warning. */` |
|         - | 6996 | `	{` |
|        40 | 6997 | `		sxi64 nSizeSigned = ph7_value_to_int(apArg[1]);` |
|        40 | 6998 | `		if( nSizeSigned < 1 ){` |
|         - | 6999 | `			/* size <= 0 -> ValueError */` |
|         6 | 7000 | `			return PH7_VmThrowException(pCtx,` |
|         - | 7001 | `				"ValueError",` |
|         - | 7002 | `				"array_chunk(): Argument #2 ($length) must be greater than 0"` |
|         - | 7003 | `				);` |
|         - | 7004 | `		}` |
|        35 | 7005 | `		nSize = (sxu32)nSizeSigned;` |
|         - | 7006 | `	}` |
|        35 | 7007 | `	if( nSize >= pMap->nEntry ){` |
|         - | 7008 | `		/* Return the whole array */` |
|         3 | 7009 | `		ph7_array_add_elem(pArray,0,apArg[0]);` |
|         3 | 7010 | `		ph7_result_value(pCtx,pArray);` |
|         3 | 7011 | `		return PH7_OK;` |
|         - | 7012 | `	}` |
|        33 | 7013 | `	bPreserve = 0;` |
|        33 | 7014 | `	if( nArg > 2 ){` |
|         - | 7015 | `		/* The third argument has a bool type hint in PHP.  Values that` |
|         - | 7016 | `		 * cannot be sensibly converted (arrays, objects, resources) are` |
|         - | 7017 | `		 * rejected with a TypeError.  Scalars and null coerce to bool` |
|         - | 7018 | `		 * normally, matching PHP behaviour. */` |
|        35 | 7019 | `		if( ph7_value_is_array(apArg[2]) \|\|` |
|        35 | 7020 | `			ph7_value_is_object(apArg[2]) \|\|` |
|        20 | 7021 | `			ph7_value_is_resource(apArg[2]) ){` |
|         8 | 7022 | `			return PH7_VmThrowException(pCtx,` |
|         - | 7023 | `				"TypeError",` |
|         - | 7024 | `				"array_chunk(): Argument #3 ($preserve_keys) must be of type bool, %s given",` |
|         4 | 7025 | `				ph7_type_name(apArg[2])` |
|         - | 7026 | `				);` |
|         - | 7027 | `		}` |
|        21 | 7028 | `		bPreserve = ph7_value_to_bool(apArg[2]);` |
|        10 | 7029 | `	}` |
|         - | 7030 | `	/* Start processing */` |
|        27 | 7031 | `	pEntry = pMap->pFirst;` |
|        27 | 7032 | `	nChunk = 0;` |
|        27 | 7033 | `	pChunk = 0;` |
|        27 | 7034 | `	n = pMap->nEntry;` |
|        56 | 7035 | `	for( ;; ){` |
|       113 | 7036 | `		if( n < 1 ){` |
|         - | 7037 | `			/* When the loop terminates we may still have a current chunk` |
|         - | 7038 | `			 * that hasn't been added to the result array.  The previous` |
|         - | 7039 | `			 * implementation only pushed it if nChunk>0 which dropped the` |
|         - | 7040 | `			 * final chunk when the input size was an exact multiple of` |
|         - | 7041 | `			 * the chunk length.  Always append the pending chunk if it` |
|         - | 7042 | `			 * exists. */` |
|        27 | 7043 | `			if( pChunk ){` |
|        27 | 7044 | `				ph7_array_add_elem(pArray,0,pChunk); /* Will have its own copy */` |
|        13 | 7045 | `			}` |
|        27 | 7046 | `			break;` |
|         - | 7047 | `		}` |
|        87 | 7048 | `		if( nChunk < 1 ){` |
|        71 | 7049 | `			if( pChunk ){` |
|         - | 7050 | `				/* Put the first chunk */` |
|        45 | 7051 | `				ph7_array_add_elem(pArray,0,pChunk); /* Will have it's own copy */` |
|        22 | 7052 | `			}` |
|         - | 7053 | `			/* Create a new dimension */` |
|        71 | 7054 | `			pChunk = ph7_context_new_array(pCtx); /* Don't worry about freeing memory here,everything` |
|         - | 7055 | `												   * will be automatically released as soon we return` |
|         - | 7056 | `												   * from this function */` |
|        71 | 7057 | `			if( pChunk == 0 ){` |
|       ! 0 | 7058 | `				break;` |
|         - | 7059 | `			}` |
|        71 | 7060 | `			nChunk = nSize;` |
|        35 | 7061 | `		}` |
|         - | 7062 | `		/* Insert the entry */` |
|        87 | 7063 | `		HashmapInsertNode((ph7_hashmap *)pChunk->x.pOther,pEntry,bPreserve);` |
|         - | 7064 | `		/* Point to the next entry */` |
|        87 | 7065 | `		pEntry = pEntry->pPrev; /* Reverse link */` |
|        87 | 7066 | `		nChunk--;` |
|        87 | 7067 | `		n--;` |
|         1 | 7068 | `	}` |
|         - | 7069 | `	/* Return the multidimensional array */` |
|        27 | 7070 | `	ph7_result_value(pCtx,pArray);` |
|        27 | 7071 | `	return PH7_OK;` |
|        26 | 7072 | `}` |
|         - | 7073 | `/*` |
|         - | 7074 | ` * array array_pad(array $input,int $pad_size,value $pad_value)` |
|         - | 7075 | ` *  Pad array to the specified length with a value.` |
|         - | 7076 | ` * $input` |
|         - | 7077 | ` *   Initial array of values to pad.` |
|         - | 7078 | ` * $pad_size` |
|         - | 7079 | ` *   New size of the array.` |
|         - | 7080 | ` * $pad_value` |
|         - | 7081 | ` *   Value to pad if input is less than pad_size.` |
|         - | 7082 | ` */` |
|        50 | 7083 | `static int ph7_hashmap_pad(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|         5 | 7084 | `{` |
|         - | 7085 | `	ph7_hashmap *pMap;` |
|         - | 7086 | `	ph7_value *pArray;` |
|         - | 7087 | `	int nEntry;` |
|        55 | 7088 | `	if( nArg != 3 ){` |
|        12 | 7089 | `		return PH7_VmThrowException(pCtx,` |
|         - | 7090 | `			"ArgumentCountError",` |
|         - | 7091 | `			"array_pad() expects exactly 3 arguments, %d given",` |
|         3 | 7092 | `			nArg` |
|         - | 7093 | `			);` |
|         - | 7094 | `	}` |
|        46 | 7095 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|         - | 7096 | `		char zBuf[64];` |
|        14 | 7097 | `		return PH7_VmThrowException(pCtx,` |
|         - | 7098 | `			"TypeError",` |
|         - | 7099 | `			"array_pad(): Argument #1 ($array) must be of type array, %s given",` |
|         4 | 7100 | `			VmValueGivenName(apArg[0],zBuf,sizeof(zBuf))` |
|         - | 7101 | `			);` |
|         - | 7102 | `	}` |
|         - | 7103 | `	/* php 8: $length must be int-coercible. An array/object/resource or a` |
|         - | 7104 | `	 * non-numeric string throws a TypeError instead of silently padding to 0;` |
|         - | 7105 | `	 * a numeric string is weak-coerced via php's is_numeric_string grammar` |
|         - | 7106 | `	 * (reusing the shared RangeStrToNumber, like array_rand's $num). */` |
|        36 | 7107 | `	if( ph7_value_is_array(apArg[1]) \|\| ph7_value_is_object(apArg[1])` |
|        34 | 7108 | `		\|\| ph7_value_is_resource(apArg[1]) ){` |
|         - | 7109 | `		char zBuf[64];` |
|         7 | 7110 | `		return PH7_VmThrowException(pCtx,` |
|         - | 7111 | `			"TypeError",` |
|         - | 7112 | `			"array_pad(): Argument #2 ($length) must be of type int, %s given",` |
|         4 | 7113 | `			VmValueGivenName(apArg[1],zBuf,sizeof(zBuf))` |
|         - | 7114 | `			);` |
|         - | 7115 | `	}` |
|        33 | 7116 | `	if( ph7_value_is_string(apArg[1]) ){` |
|         - | 7117 | `		int nStr;` |
|        11 | 7118 | `		const char *zStr = ph7_value_to_string(apArg[1],&nStr);` |
|         - | 7119 | `		sxi64 iLong; double dReal;` |
|        11 | 7120 | `		sxu8 iKind = RangeStrToNumber(zStr,(sxu32)nStr,&iLong,&dReal);` |
|        11 | 7121 | `		if( iKind == RANGE_IN_ERROR ){` |
|         5 | 7122 | `			return PH7_VmThrowException(pCtx,` |
|         - | 7123 | `				"TypeError",` |
|         - | 7124 | `				"array_pad(): Argument #2 ($length) must be of type int, string given"` |
|         - | 7125 | `				);` |
|         - | 7126 | `		}` |
|         7 | 7127 | `		nEntry = (int)(iKind == RANGE_IN_DOUBLE ? (sxi64)dReal : iLong);` |
|         4 | 7128 | `	}else{` |
|        23 | 7129 | `		nEntry = ph7_value_to_int(apArg[1]);` |
|         - | 7130 | `	}` |
|         - | 7131 | `	/* Create a new array */` |
|        29 | 7132 | `	pArray = ph7_context_new_array(pCtx);` |
|        29 | 7133 | `	if( pArray == 0 ){` |
|       ! 0 | 7134 | `		return PH7_ContextMemoryError(pCtx);` |
|         - | 7135 | `	}` |
|         - | 7136 | `	/* Point to the internal representation of the input hashmap */` |
|        29 | 7137 | `	pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|        29 | 7138 | `	if( nEntry < 0 ){` |
|         9 | 7139 | `		nEntry = -nEntry;` |
|         9 | 7140 | `		if( nEntry > (int)pMap->nEntry ){` |
|         5 | 7141 | `			nEntry -= (int)pMap->nEntry;` |
|         - | 7142 | `			/* Insert given items first */` |
|        17 | 7143 | `			while( nEntry > 0 ){` |
|        13 | 7144 | `				if( ph7_array_add_elem(pArray,0,apArg[2]) != SXRET_OK ){` |
|       ! 0 | 7145 | `					return PH7_ContextMemoryError(pCtx);` |
|         - | 7146 | `				}` |
|        13 | 7147 | `				nEntry--;` |
|         1 | 7148 | `			}` |
|         - | 7149 | `			/* Merge the two arrays */` |
|         5 | 7150 | `			HashmapMerge(pMap,(ph7_hashmap *)pArray->x.pOther);` |
|         3 | 7151 | `		}else{` |
|         5 | 7152 | `			PH7_HashmapDup(pMap,(ph7_hashmap *)pArray->x.pOther);` |
|         1 | 7153 | `		}` |
|        25 | 7154 | `	}else if( nEntry > 0 ){` |
|        19 | 7155 | `		if( nEntry > (int)pMap->nEntry ){` |
|        15 | 7156 | `			nEntry -= (int)pMap->nEntry;` |
|         - | 7157 | `			/* Merge the two arrays first */` |
|        15 | 7158 | `			HashmapMerge(pMap,(ph7_hashmap *)pArray->x.pOther);` |
|         - | 7159 | `			/* Insert given items */` |
|        65 | 7160 | `			while( nEntry > 0 ){` |
|        51 | 7161 | `				if( ph7_array_add_elem(pArray,0,apArg[2]) != SXRET_OK ){` |
|       ! 0 | 7162 | `					return PH7_ContextMemoryError(pCtx);` |
|         - | 7163 | `				}` |
|        51 | 7164 | `				nEntry--;` |
|         1 | 7165 | `			}` |
|         8 | 7166 | `		}else{` |
|         5 | 7167 | `			PH7_HashmapDup(pMap,(ph7_hashmap *)pArray->x.pOther);` |
|         - | 7168 | `		}` |
|        10 | 7169 | `	}else{` |
|         - | 7170 | `		/* nEntry == 0: return a copy of the input array */` |
|         3 | 7171 | `		PH7_HashmapDup(pMap,(ph7_hashmap *)pArray->x.pOther);` |
|         - | 7172 | `	}` |
|         - | 7173 | `	/* Return the new array */` |
|        29 | 7174 | `	ph7_result_value(pCtx,pArray);` |
|        29 | 7175 | `	return PH7_OK;` |
|        30 | 7176 | `}` |
|         - | 7177 | `/*` |
|         - | 7178 | ` * array array_replace(array &$array,array &$array1,...)` |
|         - | 7179 | ` *  Replaces elements from passed arrays into the first array.` |
|         - | 7180 | ` * Parameters` |
|         - | 7181 | ` * $array` |
|         - | 7182 | ` *   The array in which elements are replaced.` |
|         - | 7183 | ` * $array1` |
|         - | 7184 | ` *   The array from which elements will be extracted.` |
|         - | 7185 | ` * ....` |
|         - | 7186 | ` *  More arrays from which elements will be extracted.` |
|         - | 7187 | ` *  Values from later arrays overwrite the previous values.` |
|         - | 7188 | ` * Return` |
|         - | 7189 | ` *  Returns an array.` |
|         - | 7190 | ` *  Throws ArgumentCountError if no arguments are given.` |
|         - | 7191 | ` *  Throws TypeError if any argument is not an array.` |
|         - | 7192 | ` */` |
|        22 | 7193 | `static int ph7_hashmap_replace(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|         4 | 7194 | `{` |
|         - | 7195 | `	ph7_hashmap *pMap;` |
|         - | 7196 | `	ph7_value *pArray;` |
|         - | 7197 | `	int i;` |
|        26 | 7198 | `	if( nArg < 1 ){` |
|         3 | 7199 | `		return PH7_VmThrowException(pCtx,` |
|         - | 7200 | `			"ArgumentCountError",` |
|         - | 7201 | `			"array_replace() expects at least 1 argument, 0 given"` |
|         - | 7202 | `			);` |
|         - | 7203 | `	}` |
|        23 | 7204 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|         4 | 7205 | `		return PH7_VmThrowException(pCtx,` |
|         - | 7206 | `			"TypeError",` |
|         - | 7207 | `			"array_replace(): Argument #1 ($array) must be of type array, %s given",` |
|         1 | 7208 | `			ph7_type_name(apArg[0])` |
|         - | 7209 | `			);` |
|         - | 7210 | `	}` |
|         - | 7211 | `	/* Create a new array */` |
|        20 | 7212 | `	pArray = ph7_context_new_array(pCtx);` |
|        20 | 7213 | `	if( pArray == 0 ){` |
|       ! 0 | 7214 | `		ph7_result_null(pCtx);` |
|       ! 0 | 7215 | `		return PH7_OK;` |
|         - | 7216 | `	}` |
|         - | 7217 | `	/* Overwrite from the first array */` |
|        20 | 7218 | `	pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|        20 | 7219 | `	HashmapOverwrite(pMap,(ph7_hashmap *)pArray->x.pOther);` |
|         - | 7220 | `	/* Perform the requested operation for remaining arrays */` |
|        36 | 7221 | `	for( i = 1 ; i < nArg ; i++ ){` |
|        20 | 7222 | `		if( !ph7_value_is_array(apArg[i]) ){` |
|         - | 7223 | `			/* Type mismatch -> TypeError */` |
|         4 | 7224 | `			return PH7_VmThrowException(pCtx,` |
|         - | 7225 | `				"TypeError",` |
|         - | 7226 | `				"array_replace(): Argument #%d must be of type array, %s given",` |
|         1 | 7227 | `				i + 1,` |
|         2 | 7228 | `				ph7_type_name(apArg[i])` |
|         - | 7229 | `				);` |
|         - | 7230 | `		}` |
|         - | 7231 | `		/* Point to the internal representation of the input hashmap */` |
|        17 | 7232 | `		pMap = (ph7_hashmap *)apArg[i]->x.pOther;` |
|        17 | 7233 | `		HashmapOverwrite(pMap,(ph7_hashmap *)pArray->x.pOther);` |
|         9 | 7234 | `	}` |
|         - | 7235 | `	/* Return the new array */` |
|        17 | 7236 | `	ph7_result_value(pCtx,pArray);` |
|        17 | 7237 | `	return PH7_OK;` |
|        15 | 7238 | `}` |
|         - | 7239 | `/*` |
|         - | 7240 | ` * array array_filter(array $input [,callback $callback ])` |
|         - | 7241 | ` *  Filters elements of an array using a callback function.` |
|         - | 7242 | ` * Parameters` |
|         - | 7243 | ` *  $input` |
|         - | 7244 | ` *    The array to iterate over` |
|         - | 7245 | ` * $callback` |
|         - | 7246 | ` *    The callback function to use` |
|         - | 7247 | ` *    If no callback is supplied, all entries of input equal to FALSE (see converting to boolean)` |
|         - | 7248 | ` *    will be removed.` |
|         - | 7249 | ` * Return` |
|         - | 7250 | ` *  The filtered array.` |
|         - | 7251 | ` */` |
|        32 | 7252 | `static int ph7_hashmap_filter(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|         2 | 7253 | `{` |
|         - | 7254 | `	ph7_hashmap_node *pEntry;` |
|         - | 7255 | `	ph7_hashmap *pMap;` |
|         - | 7256 | `	ph7_value *pArray;` |
|         - | 7257 | `	ph7_value sResult;   /* Callback result */` |
|         - | 7258 | `	ph7_value *pValue;` |
|         - | 7259 | `	sxi32 rc;` |
|         - | 7260 | `	int keep;` |
|         - | 7261 | `	sxu32 n;` |
|        34 | 7262 | `	if( nArg < 1 ){` |
|         - | 7263 | `		/* Missing argument (arity is enforced upstream; defensive) */` |
|       ! 0 | 7264 | `		ph7_result_null(pCtx);` |
|       ! 0 | 7265 | `		return PH7_OK;` |
|         - | 7266 | `	}` |
|         - | 7267 | `	/* php 8: $array must be an array (TypeError, not a silent NULL return) */` |
|        34 | 7268 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|         - | 7269 | `		char zBuf[64];` |
|        22 | 7270 | `		return PH7_VmThrowException(pCtx,` |
|         - | 7271 | `			"TypeError",` |
|         - | 7272 | `			"array_filter(): Argument #1 ($array) must be of type array, %s given",` |
|         7 | 7273 | `			VmValueGivenName(apArg[0],zBuf,sizeof(zBuf))` |
|         - | 7274 | `			);` |
|         - | 7275 | `	}` |
|         - | 7276 | `	/* Create a new array */` |
|        20 | 7277 | `	pArray = ph7_context_new_array(pCtx);` |
|        20 | 7278 | `	if( pArray == 0 ){` |
|       ! 0 | 7279 | `		ph7_result_null(pCtx);` |
|       ! 0 | 7280 | `		return PH7_OK;` |
|         - | 7281 | `	}` |
|         - | 7282 | `	/* Point to the internal representation of the input hashmap */` |
|        20 | 7283 | `	pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|        20 | 7284 | `	pEntry = pMap->pFirst;` |
|        20 | 7285 | `	PH7_MemObjInit(pMap->pVm,&sResult);` |
|        20 | 7286 | `	sResult.nIdx = SXU32_HIGH; /* Mark as constant */` |
|         - | 7287 | `	/* Perform the requested operation */` |
|        78 | 7288 | `	for( n = 0 ; n < pMap->nEntry ; n++ ){` |
|         - | 7289 | `		/* Extract node value (may be NULL if allocation failed) */` |
|        64 | 7290 | `		pValue = HashmapExtractNodeValue(pEntry);` |
|        64 | 7291 | `		if( pValue == 0 ){` |
|         - | 7292 | `			/* Can happen if SySetAt() failed earlier; drop the entry. */` |
|       ! 0 | 7293 | `			keep = FALSE;` |
|        64 | 7294 | `		}else if( nArg > 1 && !ph7_value_is_null(apArg[1]) ){` |
|         - | 7295 | `			/* Callback was supplied (not NULL).  PHP 8 throws a` |
|         - | 7296 | `				* TypeError when the value is not callable or null; prior PH7` |
|         - | 7297 | `				* silently dropped the element.  Emit similar message. */` |
|        36 | 7298 | `			if( !ph7_value_is_callable(apArg[1]) ){` |
|         3 | 7299 | `				if( ph7_value_is_string(apArg[1]) ){` |
|         - | 7300 | `					int len;` |
|         3 | 7301 | `					const char *zName = ph7_value_to_string(apArg[1], &len);` |
|         4 | 7302 | `					return PH7_VmThrowException(pCtx,` |
|         - | 7303 | `						"TypeError",` |
|         - | 7304 | `						"array_filter(): Argument #2 ($callback) must be a valid callback or null, function \"%s\" not found or invalid function name",` |
|         1 | 7305 | `						zName` |
|         - | 7306 | `						);` |
|       ! 0 | 7307 | `				}else{` |
|       ! 0 | 7308 | `					return PH7_VmThrowException(pCtx,` |
|         - | 7309 | `						"TypeError",` |
|         - | 7310 | `						"array_filter(): Argument #2 ($callback) must be a valid callback or null, %s given",` |
|       ! 0 | 7311 | `						ph7_type_name(apArg[1])` |
|         - | 7312 | `						);` |
|         - | 7313 | `				}` |
|         - | 7314 | `			}` |
|        33 | 7315 | `			keep = FALSE;` |
|        33 | 7316 | `			rc = PH7_VmCallUserFunction(pMap->pVm,apArg[1],1,&pValue,&sResult);` |
|        33 | 7317 | `			if( rc == PH7_EXCEPTION ){` |
|         - | 7318 | `				/* The callback raised: propagate so the dispatcher unwinds. */` |
|         3 | 7319 | `				PH7_MemObjRelease(&sResult);` |
|         3 | 7320 | `				return PH7_EXCEPTION;` |
|         - | 7321 | `			}` |
|        31 | 7322 | `			if( rc == SXRET_OK ){` |
|         - | 7323 | `				/* Perform a boolean cast */` |
|        31 | 7324 | `				keep = ph7_value_to_bool(&sResult);` |
|        15 | 7325 | `			}` |
|        31 | 7326 | `			PH7_MemObjRelease(&sResult);` |
|        16 | 7327 | `		}else{` |
|         - | 7328 | `			/* No callback provided or callback explicitly NULL: use default` |
|         - | 7329 | `			 * behaviour where "empty" values are removed. This also covers` |
|         - | 7330 | `			 * the case where the callback argument is missing entirely.` |
|         - | 7331 | `			 */` |
|        29 | 7332 | `			keep = !PH7_MemObjIsEmpty(pValue);` |
|         - | 7333 | `		}` |
|        59 | 7334 | `		if( keep ){` |
|         - | 7335 | `			/* Perform the insertion,now the callback returned true */` |
|        21 | 7336 | `			HashmapInsertNode((ph7_hashmap *)pArray->x.pOther,pEntry,TRUE);` |
|        10 | 7337 | `		}` |
|         - | 7338 | `		/* Point to the next entry */` |
|        59 | 7339 | `		pEntry = pEntry->pPrev; /* Reverse link */` |
|        30 | 7340 | `	}` |
|        15 | 7341 | `	ph7_result_value(pCtx,pArray);` |
|        15 | 7342 | `	return PH7_OK;` |
|        18 | 7343 | `}` |
|         - | 7344 | `/*` |
|         - | 7345 | ` * array array_map(?callable $callback, array $array, array ...$arrays)` |
|         - | 7346 | ` *  Applies the callback to the elements of the given arrays.` |
|         - | 7347 | ` * Parameters` |
|         - | 7348 | ` *  $callback` |
|         - | 7349 | ` *   A callable to run for each element in each array, or NULL. With a single` |
|         - | 7350 | ` *   array and a NULL callback this is the identity function (the array is` |
|         - | 7351 | ` *   returned unchanged); with several arrays and a NULL callback the arrays` |
|         - | 7352 | ` *   are zipped together.` |
|         - | 7353 | ` *  $array` |
|         - | 7354 | ` *   The first array to run through the callback function.` |
|         - | 7355 | ` *  $arrays` |
|         - | 7356 | ` *   Zero or more additional arrays to process in parallel.` |
|         - | 7357 | ` * Return` |
|         - | 7358 | ` *  Returns an array containing the results of applying the callback function.` |
|         - | 7359 | ` *  With a single array the keys are preserved; with several arrays the result` |
|         - | 7360 | ` *  is re-indexed and the iteration runs to the length of the longest array,` |
|         - | 7361 | ` *  padding shorter arrays with NULL.` |
|         - | 7362 | ` */` |
|        56 | 7363 | `static int ph7_hashmap_map(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|         5 | 7364 | `{` |
|         - | 7365 | `	ph7_value *pArray,*pValue,sKey,sResult;` |
|         - | 7366 | `	ph7_hashmap_node *pEntry;` |
|         - | 7367 | `	ph7_hashmap *pMap;` |
|         - | 7368 | `	ph7_vm *pVm;` |
|         - | 7369 | `	int bNullCallback;` |
|         - | 7370 | `	sxi32 rc;` |
|         - | 7371 | `	int i;` |
|         - | 7372 | `	sxu32 n;` |
|        61 | 7373 | `	if( nArg < 2 ){` |
|         8 | 7374 | `		return PH7_VmThrowException(pCtx,` |
|         - | 7375 | `			"ArgumentCountError",` |
|         - | 7376 | `			"array_map() expects at least 2 arguments, %d given",` |
|         2 | 7377 | `			nArg` |
|         - | 7378 | `			);` |
|         - | 7379 | `	}` |
|        56 | 7380 | `	bNullCallback = ph7_value_is_null(apArg[0]);` |
|        56 | 7381 | `	if( !bNullCallback && !ph7_value_is_callable(apArg[0]) ){` |
|         5 | 7382 | `		if( ph7_value_is_string(apArg[0]) ){` |
|         3 | 7383 | `			const char *zFunc = ph7_value_to_string(apArg[0],0);` |
|         4 | 7384 | `			return PH7_VmThrowException(pCtx,` |
|         - | 7385 | `				"TypeError",` |
|         - | 7386 | `				"array_map(): Argument #1 ($callback) must be a valid callback or null, "` |
|         - | 7387 | `				"function \"%s\" not found or invalid function name",` |
|         1 | 7388 | `				zFunc` |
|         - | 7389 | `				);` |
|         - | 7390 | `		}` |
|         3 | 7391 | `		return PH7_VmThrowException(pCtx,` |
|         - | 7392 | `			"TypeError",` |
|         - | 7393 | `			"array_map(): Argument #1 ($callback) must be a valid callback or null, "` |
|         - | 7394 | `			"no array or string given"` |
|         - | 7395 | `			);` |
|         - | 7396 | `	}` |
|         - | 7397 | `	/* Every remaining argument must be an array */` |
|       109 | 7398 | `	for( i = 1 ; i < nArg ; i++ ){` |
|        63 | 7399 | `		if( !ph7_value_is_array(apArg[i]) ){` |
|         3 | 7400 | `			if( i == 1 ){` |
|         4 | 7401 | `				return PH7_VmThrowException(pCtx,` |
|         - | 7402 | `					"TypeError",` |
|         - | 7403 | `					"array_map(): Argument #2 ($array) must be of type array, %s given",` |
|         2 | 7404 | `					ph7_type_name(apArg[1])` |
|         - | 7405 | `					);` |
|         - | 7406 | `			}` |
|       ! 0 | 7407 | `			return PH7_VmThrowException(pCtx,` |
|         - | 7408 | `				"TypeError",` |
|         - | 7409 | `				"array_map(): Argument #%d must be of type array, %s given",` |
|       ! 0 | 7410 | `				i+1,ph7_type_name(apArg[i])` |
|         - | 7411 | `				);` |
|         - | 7412 | `		}` |
|        31 | 7413 | `	}` |
|        48 | 7414 | `	pVm = pCtx->pVm;` |
|         - | 7415 | `	/* Create a new array */` |
|        48 | 7416 | `	pArray = ph7_context_new_array(pCtx);` |
|        48 | 7417 | `	if( pArray == 0 ){` |
|       ! 0 | 7418 | `		ph7_result_null(pCtx);` |
|       ! 0 | 7419 | `		return PH7_OK;` |
|         - | 7420 | `	}` |
|        48 | 7421 | `	PH7_MemObjInit(pVm,&sResult);` |
|        48 | 7422 | `	PH7_MemObjInit(pVm,&sKey);` |
|        48 | 7423 | `	sResult.nIdx = SXU32_HIGH; /* Mark as constant */` |
|        48 | 7424 | `	sKey.nIdx    = SXU32_HIGH; /* Mark as constant */` |
|        48 | 7425 | `	if( nArg == 2 ){` |
|         - | 7426 | `		/* Single-array mode: keys are preserved (PHP semantics). */` |
|        38 | 7427 | `		pMap = (ph7_hashmap *)apArg[1]->x.pOther;` |
|        38 | 7428 | `		pEntry = pMap->pFirst;` |
|       112 | 7429 | `		for( n = 0 ; n < pMap->nEntry ; n++ ){` |
|         - | 7430 | `			/* Extract the node value */` |
|        80 | 7431 | `			pValue = HashmapExtractNodeValue(pEntry);` |
|        80 | 7432 | `			if( pValue ){` |
|         - | 7433 | `				/* Extract the node key */` |
|        80 | 7434 | `				PH7_HashmapExtractNodeKey(pEntry,&sKey);` |
|        80 | 7435 | `				if( bNullCallback ){` |
|         - | 7436 | `					/* NULL callback: identity function, keep original value */` |
|        11 | 7437 | `					ph7_array_add_elem(pArray,&sKey,pValue);` |
|         6 | 7438 | `				}else{` |
|         - | 7439 | `					/* Invoke the supplied callback */` |
|        70 | 7440 | `					rc = PH7_VmCallUserFunction(pVm,apArg[0],1,&pValue,&sResult);` |
|        70 | 7441 | `					if( rc == PH7_EXCEPTION ){` |
|         - | 7442 | `						/* Callback raised: abort and let the foreign-function` |
|         - | 7443 | `						 * dispatcher unwind through the nearest try/catch. */` |
|         5 | 7444 | `						PH7_MemObjRelease(&sKey);` |
|         5 | 7445 | `						PH7_MemObjRelease(&sResult);` |
|         5 | 7446 | `						return PH7_EXCEPTION;` |
|         - | 7447 | `					}` |
|         - | 7448 | `					/* Insert the callback return value */` |
|        66 | 7449 | `					ph7_array_add_elem(pArray,&sKey,&sResult);` |
|         - | 7450 | `				}` |
|        76 | 7451 | `				PH7_MemObjRelease(&sKey);` |
|        76 | 7452 | `				PH7_MemObjRelease(&sResult);` |
|        37 | 7453 | `			}` |
|         - | 7454 | `			/* Point to the next entry */` |
|        76 | 7455 | `			pEntry = pEntry->pPrev; /* Reverse link */` |
|        39 | 7456 | `		}` |
|        18 | 7457 | `	}else{` |
|         - | 7458 | `		/* Multi-array mode: walk every array in parallel to the length of the` |
|         - | 7459 | `		 * longest one, pad shorter arrays with NULL, and re-index the result. */` |
|        11 | 7460 | `		int nArrays = nArg - 1;` |
|         - | 7461 | `		ph7_hashmap_node **apCur;` |
|         - | 7462 | `		ph7_value **apCallArg;` |
|         - | 7463 | `		ph7_value sNull;` |
|        11 | 7464 | `		sxu32 nMax = 0;` |
|        11 | 7465 | `		apCur     = (ph7_hashmap_node **)SyMemBackendAlloc(&pVm->sAllocator,(sxu32)(nArrays*sizeof(ph7_hashmap_node *)));` |
|        11 | 7466 | `		apCallArg = (ph7_value **)SyMemBackendAlloc(&pVm->sAllocator,(sxu32)(nArrays*sizeof(ph7_value *)));` |
|        11 | 7467 | `		if( apCur == 0 \|\| apCallArg == 0 ){` |
|       ! 0 | 7468 | `			if( apCur ){ SyMemBackendFree(&pVm->sAllocator,apCur); }` |
|       ! 0 | 7469 | `			if( apCallArg ){ SyMemBackendFree(&pVm->sAllocator,apCallArg); }` |
|       ! 0 | 7470 | `			PH7_MemObjRelease(&sKey);` |
|       ! 0 | 7471 | `			PH7_MemObjRelease(&sResult);` |
|       ! 0 | 7472 | `			ph7_result_value(pCtx,pArray);` |
|       ! 0 | 7473 | `			return PH7_OK;` |
|         - | 7474 | `		}` |
|        11 | 7475 | `		PH7_MemObjInit(pVm,&sNull); /* shared NULL pad for short arrays */` |
|        11 | 7476 | `		sNull.nIdx = SXU32_HIGH;` |
|        33 | 7477 | `		for( i = 0 ; i < nArrays ; i++ ){` |
|        23 | 7478 | `			pMap = (ph7_hashmap *)apArg[i+1]->x.pOther;` |
|        23 | 7479 | `			apCur[i] = pMap->pFirst;` |
|        23 | 7480 | `			if( pMap->nEntry > nMax ){` |
|        13 | 7481 | `				nMax = pMap->nEntry;` |
|         6 | 7482 | `			}` |
|        12 | 7483 | `		}` |
|        35 | 7484 | `		for( n = 0 ; n < nMax ; n++ ){` |
|        25 | 7485 | `			ph7_value *pZip = 0;` |
|        25 | 7486 | `			if( bNullCallback ){` |
|         - | 7487 | `				/* zip: each result element is an array of the i-th values */` |
|         5 | 7488 | `				pZip = ph7_context_new_array(pCtx);` |
|         2 | 7489 | `			}` |
|        79 | 7490 | `			for( i = 0 ; i < nArrays ; i++ ){` |
|        55 | 7491 | `				ph7_value *pv = &sNull;` |
|        55 | 7492 | `				if( apCur[i] ){` |
|        53 | 7493 | `					ph7_value *pNodeVal = HashmapExtractNodeValue(apCur[i]);` |
|        53 | 7494 | `					if( pNodeVal ){` |
|        53 | 7495 | `						pv = pNodeVal;` |
|        26 | 7496 | `					}` |
|        53 | 7497 | `					apCur[i] = apCur[i]->pPrev; /* Reverse link */` |
|        26 | 7498 | `				}` |
|        55 | 7499 | `				if( bNullCallback ){` |
|         9 | 7500 | `					if( pZip ){` |
|         9 | 7501 | `						ph7_array_add_elem(pZip,0,pv);` |
|         4 | 7502 | `					}` |
|         5 | 7503 | `				}else{` |
|        47 | 7504 | `					apCallArg[i] = pv;` |
|         - | 7505 | `				}` |
|        28 | 7506 | `			}` |
|        25 | 7507 | `			if( bNullCallback ){` |
|         5 | 7508 | `				if( pZip ){` |
|         5 | 7509 | `					ph7_array_add_elem(pArray,0,pZip);` |
|         2 | 7510 | `				}` |
|         3 | 7511 | `			}else{` |
|        21 | 7512 | `				rc = PH7_VmCallUserFunction(pVm,apArg[0],nArrays,apCallArg,&sResult);` |
|        21 | 7513 | `				if( rc == PH7_EXCEPTION ){` |
|       ! 0 | 7514 | `					SyMemBackendFree(&pVm->sAllocator,apCur);` |
|       ! 0 | 7515 | `					SyMemBackendFree(&pVm->sAllocator,apCallArg);` |
|       ! 0 | 7516 | `					PH7_MemObjRelease(&sNull);` |
|       ! 0 | 7517 | `					PH7_MemObjRelease(&sKey);` |
|       ! 0 | 7518 | `					PH7_MemObjRelease(&sResult);` |
|       ! 0 | 7519 | `					return PH7_EXCEPTION;` |
|         - | 7520 | `				}` |
|        21 | 7521 | `				ph7_array_add_elem(pArray,0,&sResult);` |
|        21 | 7522 | `				PH7_MemObjRelease(&sResult);` |
|         - | 7523 | `			}` |
|        13 | 7524 | `		}` |
|        11 | 7525 | `		SyMemBackendFree(&pVm->sAllocator,apCur);` |
|        11 | 7526 | `		SyMemBackendFree(&pVm->sAllocator,apCallArg);` |
|        11 | 7527 | `		PH7_MemObjRelease(&sNull);` |
|         - | 7528 | `	}` |
|        44 | 7529 | `	PH7_MemObjRelease(&sKey);` |
|        44 | 7530 | `	PH7_MemObjRelease(&sResult);` |
|        44 | 7531 | `	ph7_result_value(pCtx,pArray);` |
|        44 | 7532 | `	return PH7_OK;` |
|        33 | 7533 | `}` |
|         - | 7534 | `/*` |
|         - | 7535 | ` * value array_reduce(array $array, callable $callback[, value $initial = NULL])` |
|         - | 7536 | ` *  Iteratively reduce the array to a single value using a callback function.` |
|         - | 7537 | ` * Parameters` |
|         - | 7538 | ` *  $array` |
|         - | 7539 | ` *   The input array.` |
|         - | 7540 | ` *  $callback` |
|         - | 7541 | ` *   The callback function. Signature: callback(mixed $carry, mixed $item): mixed` |
|         - | 7542 | ` *  $initial` |
|         - | 7543 | ` *   If the optional initial is available, it will be used at the beginning` |
|         - | 7544 | ` *   of the process, or as a final result in case the array is empty.` |
|         - | 7545 | ` * Return` |
|         - | 7546 | ` *  Returns the resulting value.` |
|         - | 7547 | ` *  If the array is empty and initial is not passed, array_reduce() returns NULL.` |
|         - | 7548 | ` */` |
|        34 | 7549 | `static int ph7_hashmap_reduce(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|         5 | 7550 | `{` |
|         - | 7551 | `	ph7_hashmap_node *pEntry;` |
|         - | 7552 | `	ph7_hashmap *pMap;` |
|         - | 7553 | `	ph7_value *pValue;` |
|         - | 7554 | `	ph7_value sResult;` |
|         - | 7555 | `	sxi32 rc;` |
|         - | 7556 | `	sxu32 n;` |
|        39 | 7557 | `	if( nArg < 2 ){` |
|         8 | 7558 | `		return PH7_VmThrowException(pCtx,` |
|         - | 7559 | `			"ArgumentCountError",` |
|         - | 7560 | `			"array_reduce() expects at least 2 arguments, %d given",` |
|         2 | 7561 | `			nArg` |
|         - | 7562 | `			);` |
|         - | 7563 | `	}` |
|        35 | 7564 | `	if( nArg > 3 ){` |
|         4 | 7565 | `		return PH7_VmThrowException(pCtx,` |
|         - | 7566 | `			"ArgumentCountError",` |
|         - | 7567 | `			"array_reduce() expects at most 3 arguments, %d given",` |
|         1 | 7568 | `			nArg` |
|         - | 7569 | `			);` |
|         - | 7570 | `	}` |
|        33 | 7571 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|         4 | 7572 | `		return PH7_VmThrowException(pCtx,` |
|         - | 7573 | `			"TypeError",` |
|         - | 7574 | `			"array_reduce(): Argument #1 ($array) must be of type array, %s given",` |
|         1 | 7575 | `			ph7_type_name(apArg[0])` |
|         - | 7576 | `			);` |
|         - | 7577 | `	}` |
|        31 | 7578 | `	if( !ph7_value_is_callable(apArg[1]) ){` |
|        12 | 7579 | `		if( ph7_value_is_string(apArg[1]) ){` |
|         3 | 7580 | `			const char *zFunc = ph7_value_to_string(apArg[1],0);` |
|         4 | 7581 | `			return PH7_VmThrowException(pCtx,` |
|         - | 7582 | `				"TypeError",` |
|         - | 7583 | `				"array_reduce(): Argument #2 ($callback) must be a valid callback, "` |
|         - | 7584 | `				"function \"%s\" not found or invalid function name",` |
|         1 | 7585 | `				zFunc` |
|         - | 7586 | `				);` |
|         - | 7587 | `		}` |
|         9 | 7588 | `		if( ph7_value_is_array(apArg[1]) ){` |
|         3 | 7589 | `			return PH7_VmThrowException(pCtx,` |
|         - | 7590 | `				"TypeError",` |
|         - | 7591 | `				"array_reduce(): Argument #2 ($callback) must be a valid callback, "` |
|         - | 7592 | `				"array callback must have exactly two members"` |
|         - | 7593 | `				);` |
|         - | 7594 | `		}` |
|         6 | 7595 | `		return PH7_VmThrowException(pCtx,` |
|         - | 7596 | `			"TypeError",` |
|         - | 7597 | `			"array_reduce(): Argument #2 ($callback) must be a valid callback, "` |
|         - | 7598 | `			"no array or string given"` |
|         - | 7599 | `			);` |
|         - | 7600 | `	}` |
|         - | 7601 | `	/* Point to the internal representation of the input hashmap */` |
|        19 | 7602 | `	pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|         - | 7603 | `	/* Assume a NULL initial value */` |
|        19 | 7604 | `	PH7_MemObjInit(pMap->pVm,&sResult);` |
|        19 | 7605 | `	sResult.nIdx = SXU32_HIGH; /* Mark as constant */` |
|        19 | 7606 | `	if( nArg > 2 ){` |
|         - | 7607 | `		/* Set the initial value */` |
|        13 | 7608 | `		PH7_MemObjLoad(apArg[2],&sResult);` |
|         6 | 7609 | `	}` |
|         - | 7610 | `	/* Perform the requested operation */` |
|        19 | 7611 | `	pEntry = pMap->pFirst;` |
|        55 | 7612 | `	for( n = 0 ; n < pMap->nEntry ; n++ ){` |
|         - | 7613 | `		/* Extract the node value */` |
|        39 | 7614 | `		pValue = HashmapExtractNodeValue(pEntry);` |
|         - | 7615 | `		/* Invoke the supplied callback */` |
|        39 | 7616 | `		rc = PH7_VmCallUserFunctionAp(pMap->pVm,apArg[1],&sResult,&sResult,pValue,0);` |
|        39 | 7617 | `		if( rc == PH7_EXCEPTION ){` |
|         - | 7618 | `			/* The callback raised: propagate so the dispatcher unwinds. */` |
|         3 | 7619 | `			PH7_MemObjRelease(&sResult);` |
|         3 | 7620 | `			return PH7_EXCEPTION;` |
|         - | 7621 | `		}` |
|         - | 7622 | `		/* Point to the next entry */` |
|        37 | 7623 | `		pEntry = pEntry->pPrev; /* Reverse link */` |
|        19 | 7624 | `	}` |
|        17 | 7625 | `	ph7_result_value(pCtx,&sResult); /* Will make it's own copy */` |
|        17 | 7626 | `	PH7_MemObjRelease(&sResult);` |
|        17 | 7627 | `	return PH7_OK;` |
|        22 | 7628 | `}` |
|         - | 7629 | `/*` |
|         - | 7630 | ` * bool array_walk(array &$array, callback $funcname [, mixed $userdata])` |
|         - | 7631 | ` *  Apply a user function to every member of an array.` |
|         - | 7632 | ` * Parameters` |
|         - | 7633 | ` *  $array` |
|         - | 7634 | ` *   The input array.` |
|         - | 7635 | ` *  $funcname` |
|         - | 7636 | ` *   Typically, funcname takes on two parameters. The array parameter's value being` |
|         - | 7637 | ` *   the first, and the key/index second.` |
|         - | 7638 | ` * Note:` |
|         - | 7639 | ` *  If funcname needs to be working with the actual values of the array, specify the first` |
|         - | 7640 | ` *  parameter of funcname as a reference. Then, any changes made to those elements will` |
|         - | 7641 | ` *  be made in the original array itself.` |
|         - | 7642 | ` *  $userdata` |
|         - | 7643 | ` *   If the optional userdata parameter is supplied, it will be passed as the third parameter` |
|         - | 7644 | ` *   to the callback funcname.` |
|         - | 7645 | ` * Return` |
|         - | 7646 | ` *  Returns TRUE on success or FALSE on failure.` |
|         - | 7647 | ` */` |
|        38 | 7648 | `static int ph7_hashmap_walk(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|         5 | 7649 | `{` |
|         - | 7650 | `	ph7_value *pValue,*pUserData,sKey;` |
|         - | 7651 | `	ph7_hashmap_node *pEntry;` |
|         - | 7652 | `	ph7_hashmap *pMap;` |
|         - | 7653 | `	sxu32 n;` |
|        43 | 7654 | `	if( nArg < 2 ){` |
|         8 | 7655 | `		return PH7_VmThrowException(pCtx,` |
|         - | 7656 | `			"ArgumentCountError",` |
|         - | 7657 | `			"array_walk() expects at least 2 arguments, %d given",` |
|         2 | 7658 | `			nArg` |
|         - | 7659 | `			);` |
|         - | 7660 | `	}` |
|        39 | 7661 | `	if( nArg > 3 ){` |
|         4 | 7662 | `		return PH7_VmThrowException(pCtx,` |
|         - | 7663 | `			"ArgumentCountError",` |
|         - | 7664 | `			"array_walk() expects at most 3 arguments, %d given",` |
|         1 | 7665 | `			nArg` |
|         - | 7666 | `			);` |
|         - | 7667 | `	}` |
|        37 | 7668 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|         4 | 7669 | `		return PH7_VmThrowException(pCtx,` |
|         - | 7670 | `			"TypeError",` |
|         - | 7671 | `			"array_walk(): Argument #1 ($array) must be of type array, %s given",` |
|         1 | 7672 | `			ph7_type_name(apArg[0])` |
|         - | 7673 | `			);` |
|         - | 7674 | `	}` |
|        35 | 7675 | `	if( !ph7_value_is_callable(apArg[1]) ){` |
|        14 | 7676 | `		if( ph7_value_is_string(apArg[1]) ){` |
|         3 | 7677 | `			const char *zFunc = ph7_value_to_string(apArg[1],0);` |
|         4 | 7678 | `			return PH7_VmThrowException(pCtx,` |
|         - | 7679 | `				"TypeError",` |
|         - | 7680 | `				"array_walk(): Argument #2 ($callback) must be a valid callback, "` |
|         - | 7681 | `				"function \"%s\" not found or invalid function name",` |
|         1 | 7682 | `				zFunc` |
|         - | 7683 | `				);` |
|         - | 7684 | `		}` |
|        12 | 7685 | `		if( ph7_value_is_array(apArg[1]) ){` |
|         6 | 7686 | `			return PH7_VmThrowException(pCtx,` |
|         - | 7687 | `				"TypeError",` |
|         - | 7688 | `				"array_walk(): Argument #2 ($callback) must be a valid callback, "` |
|         - | 7689 | `				"array callback must have exactly two members"` |
|         - | 7690 | `				);` |
|         - | 7691 | `		}` |
|         6 | 7692 | `		return PH7_VmThrowException(pCtx,` |
|         - | 7693 | `			"TypeError",` |
|         - | 7694 | `			"array_walk(): Argument #2 ($callback) must be a valid callback, "` |
|         - | 7695 | `			"no array or string given"` |
|         - | 7696 | `			);` |
|         - | 7697 | `	}` |
|        21 | 7698 | `	pUserData = nArg > 2 ? apArg[2] : 0;` |
|         - | 7699 | `	/* Point to the internal representation of the input hashmap */` |
|        21 | 7700 | `	PH7_HashmapCowSeparate(pCtx->pVm, apArg[0]);` |
|        21 | 7701 | `	pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|        21 | 7702 | `	PH7_MemObjInit(pMap->pVm,&sKey);` |
|        21 | 7703 | `	sKey.nIdx = SXU32_HIGH; /* Mark as constant */` |
|         - | 7704 | `	/* Perform the desired operation */` |
|        21 | 7705 | `	pEntry = pMap->pFirst;` |
|        61 | 7706 | `	for( n = 0 ; n < pMap->nEntry ; n++ ){` |
|         - | 7707 | `		/* Extract the node value */` |
|        43 | 7708 | `		pValue = HashmapExtractNodeValue(pEntry);` |
|        43 | 7709 | `		if( pValue ){` |
|         - | 7710 | `			sxi32 rcW;` |
|         - | 7711 | `			/* Extract the entry key */` |
|        43 | 7712 | `			PH7_HashmapExtractNodeKey(pEntry,&sKey);` |
|         - | 7713 | `			/* Invoke the supplied callback */` |
|        43 | 7714 | `			rcW = PH7_VmCallUserFunctionAp(pMap->pVm,apArg[1],0,pValue,&sKey,pUserData,0);` |
|        43 | 7715 | `			PH7_MemObjRelease(&sKey);` |
|        43 | 7716 | `			if( rcW == PH7_EXCEPTION ){` |
|         - | 7717 | `				/* The callback raised: propagate so the dispatcher unwinds. */` |
|         3 | 7718 | `				return PH7_EXCEPTION;` |
|         - | 7719 | `			}` |
|        20 | 7720 | `		}` |
|         - | 7721 | `		/* Point to the next entry */` |
|        41 | 7722 | `		pEntry = pEntry->pPrev; /* Reverse link */` |
|        21 | 7723 | `	}` |
|         - | 7724 | `	/* All done, return TRUE */` |
|        19 | 7725 | `	ph7_result_bool(pCtx,1);` |
|        19 | 7726 | `	return PH7_OK;` |
|        24 | 7727 | `}` |
|         - | 7728 | `/*` |
|         - | 7729 | ` * Apply a user function to every member of an array.(Recurse on array's).` |
|         - | 7730 | ` * Refer to the [array_walk_recursive()] implementation for more information.` |
|         - | 7731 | ` */` |
|        22 | 7732 | `static sxi32 HashmapWalkRecursive(` |
|         - | 7733 | `	ph7_hashmap *pMap,    /* Target hashmap */` |
|         - | 7734 | `	ph7_value *pCallback, /* User callback */` |
|         - | 7735 | `	ph7_value *pUserData, /* Callback private data */` |
|         - | 7736 | `	int iNest             /* Nesting level */` |
|         - | 7737 | `	)` |
|         1 | 7738 | `{` |
|         - | 7739 | `	ph7_hashmap_node *pEntry;` |
|         - | 7740 | `	ph7_value *pValue,sKey;` |
|         - | 7741 | `	sxi32 rc;` |
|         - | 7742 | `	sxu32 n;` |
|         - | 7743 | `	/* Iterate through hashmap entries */` |
|        23 | 7744 | `	PH7_MemObjInit(pMap->pVm,&sKey);` |
|        23 | 7745 | `	sKey.nIdx = SXU32_HIGH; /* Mark as constant */` |
|        23 | 7746 | `	pEntry = pMap->pFirst;` |
|        59 | 7747 | `	for( n = 0 ; n < pMap->nEntry ; n++ ){` |
|         - | 7748 | `		/* Extract the node value */` |
|        37 | 7749 | `		pValue = HashmapExtractNodeValue(pEntry);` |
|        37 | 7750 | `		if( pValue ){` |
|        37 | 7751 | `			if( pValue->iFlags & MEMOBJ_HASHMAP ){` |
|        11 | 7752 | `				if( iNest < 32 ){` |
|         - | 7753 | `					/* Recurse */` |
|        11 | 7754 | `					iNest++;` |
|        11 | 7755 | `					rc = HashmapWalkRecursive((ph7_hashmap *)pValue->x.pOther,pCallback,pUserData,iNest);` |
|        11 | 7756 | `					iNest--;` |
|        11 | 7757 | `					if( rc == PH7_EXCEPTION ){` |
|       ! 0 | 7758 | `						return PH7_EXCEPTION;` |
|         - | 7759 | `					}` |
|         5 | 7760 | `				}` |
|         6 | 7761 | `			}else{` |
|         - | 7762 | `				/* Extract the node key */` |
|        27 | 7763 | `				PH7_HashmapExtractNodeKey(pEntry,&sKey);` |
|         - | 7764 | `				/* Invoke the supplied callback */` |
|        27 | 7765 | `				rc = PH7_VmCallUserFunctionAp(pMap->pVm,pCallback,0,pValue,&sKey,pUserData,0);` |
|        27 | 7766 | `				PH7_MemObjRelease(&sKey);` |
|        27 | 7767 | `				if( rc == PH7_EXCEPTION ){` |
|         - | 7768 | `					/* The callback raised: propagate so the dispatcher unwinds. */` |
|       ! 0 | 7769 | `					return PH7_EXCEPTION;` |
|         - | 7770 | `				}` |
|         - | 7771 | `			}` |
|        18 | 7772 | `		}` |
|         - | 7773 | `		/* Point to the next entry */` |
|        37 | 7774 | `		pEntry = pEntry->pPrev; /* Reverse link */` |
|        19 | 7775 | `	}` |
|        23 | 7776 | `	return PH7_OK;` |
|        12 | 7777 | `}` |
|         - | 7778 | `/*` |
|         - | 7779 | ` * bool array_walk_recursive(array &$array, callback $funcname [, mixed $userdata])` |
|         - | 7780 | ` *  Apply a user function recursively to every member of an array.` |
|         - | 7781 | ` * Parameters` |
|         - | 7782 | ` *  $array` |
|         - | 7783 | ` *   The input array.` |
|         - | 7784 | ` *  $funcname` |
|         - | 7785 | ` *   Typically, funcname takes on two parameters. The array parameter's value being` |
|         - | 7786 | ` *   the first, and the key/index second.` |
|         - | 7787 | ` * Note:` |
|         - | 7788 | ` *  If funcname needs to be working with the actual values of the array, specify the first` |
|         - | 7789 | ` *  parameter of funcname as a reference. Then, any changes made to those elements will` |
|         - | 7790 | ` *  be made in the original array itself.` |
|         - | 7791 | ` *  $userdata` |
|         - | 7792 | ` *   If the optional userdata parameter is supplied, it will be passed as the third parameter` |
|         - | 7793 | ` *   to the callback funcname.` |
|         - | 7794 | ` * Return` |
|         - | 7795 | ` *  Returns TRUE on success or FALSE on failure.` |
|         - | 7796 | ` */` |
|        30 | 7797 | `static int ph7_hashmap_walk_recursive(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|         5 | 7798 | `{` |
|         - | 7799 | `	ph7_hashmap *pMap;` |
|        35 | 7800 | `	if( nArg < 2 ){` |
|         8 | 7801 | `		return PH7_VmThrowException(pCtx,` |
|         - | 7802 | `			"ArgumentCountError",` |
|         - | 7803 | `			"array_walk_recursive() expects at least 2 arguments, %d given",` |
|         2 | 7804 | `			nArg` |
|         - | 7805 | `			);` |
|         - | 7806 | `	}` |
|        31 | 7807 | `	if( nArg > 3 ){` |
|         4 | 7808 | `		return PH7_VmThrowException(pCtx,` |
|         - | 7809 | `			"ArgumentCountError",` |
|         - | 7810 | `			"array_walk_recursive() expects at most 3 arguments, %d given",` |
|         1 | 7811 | `			nArg` |
|         - | 7812 | `			);` |
|         - | 7813 | `	}` |
|        29 | 7814 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|         4 | 7815 | `		return PH7_VmThrowException(pCtx,` |
|         - | 7816 | `			"TypeError",` |
|         - | 7817 | `			"array_walk_recursive(): Argument #1 ($array) must be of type array, %s given",` |
|         1 | 7818 | `			ph7_type_name(apArg[0])` |
|         - | 7819 | `			);` |
|         - | 7820 | `	}` |
|        27 | 7821 | `	if( !ph7_value_is_callable(apArg[1]) ){` |
|        14 | 7822 | `		if( ph7_value_is_string(apArg[1]) ){` |
|         3 | 7823 | `			const char *zFunc = ph7_value_to_string(apArg[1],0);` |
|         4 | 7824 | `			return PH7_VmThrowException(pCtx,` |
|         - | 7825 | `				"TypeError",` |
|         - | 7826 | `				"array_walk_recursive(): Argument #2 ($callback) must be a valid callback, "` |
|         - | 7827 | `				"function \"%s\" not found or invalid function name",` |
|         1 | 7828 | `				zFunc` |
|         - | 7829 | `				);` |
|         - | 7830 | `		}` |
|        12 | 7831 | `		if( ph7_value_is_array(apArg[1]) ){` |
|         6 | 7832 | `			return PH7_VmThrowException(pCtx,` |
|         - | 7833 | `				"TypeError",` |
|         - | 7834 | `				"array_walk_recursive(): Argument #2 ($callback) must be a valid callback, "` |
|         - | 7835 | `				"array callback must have exactly two members"` |
|         - | 7836 | `				);` |
|         - | 7837 | `		}` |
|         6 | 7838 | `		return PH7_VmThrowException(pCtx,` |
|         - | 7839 | `			"TypeError",` |
|         - | 7840 | `			"array_walk_recursive(): Argument #2 ($callback) must be a valid callback, "` |
|         - | 7841 | `			"no array or string given"` |
|         - | 7842 | `			);` |
|         - | 7843 | `	}` |
|         - | 7844 | `	/* Point to the internal representation of the input hashmap */` |
|        13 | 7845 | `	PH7_HashmapCowSeparate(pCtx->pVm, apArg[0]);` |
|        13 | 7846 | `	pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|         - | 7847 | `	/* Perform the desired operation */` |
|        13 | 7848 | `	if( HashmapWalkRecursive(pMap,apArg[1],nArg > 2 ? apArg[2] : 0,0) == PH7_EXCEPTION ){` |
|         - | 7849 | `		/* A callback raised: propagate so the dispatcher unwinds. */` |
|       ! 0 | 7850 | `		return PH7_EXCEPTION;` |
|         - | 7851 | `	}` |
|         - | 7852 | `	/* All done, return TRUE */` |
|        13 | 7853 | `	ph7_result_bool(pCtx,1);` |
|        13 | 7854 | `	return PH7_OK;` |
|        20 | 7855 | `}` |
|         - | 7856 | `/*` |
|         - | 7857 | ` * bool array_is_list(array $array)` |
|         - | 7858 | ` *  Checks whether a given array is a list: its keys consist of consecutive` |
|         - | 7859 | ` *  integers starting at 0. An empty array is a list.` |
|         - | 7860 | ` * Return` |
|         - | 7861 | ` *  TRUE if the array is a list, FALSE otherwise.` |
|         - | 7862 | ` */` |
|         - | 7863 | `/*` |
|         - | 7864 | ` * Return TRUE if the given hashmap is a "list" [i.e: its keys are the` |
|         - | 7865 | ` * consecutive integers 0,1,2,... with no gaps]. An empty map is a list.` |
|         - | 7866 | ` * Shared by array_is_list() and the JSON encoder (vm_json.c).` |
|         - | 7867 | ` */` |
|       208 | 7868 | `PH7_PRIVATE int PH7_HashmapIsList(ph7_hashmap *pMap)` |
|         1 | 7869 | `{` |
|       209 | 7870 | `	ph7_hashmap_node *pNode = pMap->pFirst;` |
|       209 | 7871 | `	sxi64 iExpect = 0;` |
|         - | 7872 | `	sxu32 n;` |
|       445 | 7873 | `	for( n = 0 ; n < pMap->nEntry ; ++n ){` |
|       319 | 7874 | `		if( pNode->iType != HASHMAP_INT_NODE \|\| pNode->xKey.iKey != iExpect ){` |
|         - | 7875 | `			/* A non-integer key or a gap in the sequence: not a list */` |
|        83 | 7876 | `			return 0;` |
|         - | 7877 | `		}` |
|       237 | 7878 | `		++iExpect;` |
|       237 | 7879 | `		pNode = pNode->pPrev; /* Reverse link */` |
|       119 | 7880 | `	}` |
|       127 | 7881 | `	return 1;` |
|       105 | 7882 | `}` |
|        12 | 7883 | `static int ph7_hashmap_is_list(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|         1 | 7884 | `{` |
|        13 | 7885 | `	if( nArg < 1 ){` |
|       ! 0 | 7886 | `		return PH7_VmThrowException(pCtx,` |
|         - | 7887 | `			"ArgumentCountError",` |
|         - | 7888 | `			"array_is_list() expects exactly 1 argument, 0 given"` |
|         - | 7889 | `			);` |
|         - | 7890 | `	}` |
|        13 | 7891 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|       ! 0 | 7892 | `		return PH7_VmThrowException(pCtx,` |
|         - | 7893 | `			"TypeError",` |
|         - | 7894 | `			"array_is_list(): Argument #1 ($array) must be of type array, %s given",` |
|       ! 0 | 7895 | `			ph7_type_name(apArg[0])` |
|         - | 7896 | `			);` |
|         - | 7897 | `	}` |
|        13 | 7898 | `	ph7_result_bool(pCtx,PH7_HashmapIsList((ph7_hashmap *)apArg[0]->x.pOther));` |
|        13 | 7899 | `	return PH7_OK;` |
|         7 | 7900 | `}` |
|         - | 7901 | `/*` |
|         - | 7902 | ` * mixed array_first(array $array)` |
|         - | 7903 | ` * mixed array_last(array $array)` |
|         - | 7904 | ` *  Return the value of the first (respectively last) element of the array,` |
|         - | 7905 | ` *  or NULL when the array is empty. The internal array pointer is left` |
|         - | 7906 | ` *  untouched (unlike reset()/end()).` |
|         - | 7907 | ` */` |
|        20 | 7908 | `static int HashmapFirstLast(ph7_context *pCtx,int nArg,ph7_value **apArg,int bLast)` |
|         1 | 7909 | `{` |
|         - | 7910 | `	ph7_hashmap *pMap;` |
|         - | 7911 | `	ph7_hashmap_node *pNode;` |
|         - | 7912 | `	ph7_value *pVal;` |
|        21 | 7913 | `	const char *zName = bLast ? "array_last" : "array_first";` |
|        21 | 7914 | `	if( nArg < 1 ){` |
|         4 | 7915 | `		return PH7_VmThrowException(pCtx,` |
|         - | 7916 | `			"ArgumentCountError",` |
|         - | 7917 | `			"%s() expects exactly 1 argument, 0 given",` |
|         1 | 7918 | `			zName` |
|         - | 7919 | `			);` |
|         - | 7920 | `	}` |
|        19 | 7921 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|         4 | 7922 | `		return PH7_VmThrowException(pCtx,` |
|         - | 7923 | `			"TypeError",` |
|         - | 7924 | `			"%s(): Argument #1 ($array) must be of type array, %s given",` |
|         1 | 7925 | `			zName,` |
|         1 | 7926 | `			ph7_type_name(apArg[0])` |
|         - | 7927 | `			);` |
|         - | 7928 | `	}` |
|        17 | 7929 | `	pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|        17 | 7930 | `	pNode = bLast ? pMap->pLast : pMap->pFirst;` |
|        17 | 7931 | `	if( pNode == 0 ){` |
|         - | 7932 | `		/* Empty array: PHP returns NULL */` |
|         5 | 7933 | `		ph7_result_null(pCtx);` |
|         5 | 7934 | `		return PH7_OK;` |
|         - | 7935 | `	}` |
|        13 | 7936 | `	pVal = HashmapExtractNodeValue(pNode);` |
|        13 | 7937 | `	if( pVal ){` |
|        13 | 7938 | `		ph7_result_value(pCtx,pVal);` |
|         7 | 7939 | `	}else{` |
|       ! 0 | 7940 | `		ph7_result_null(pCtx);` |
|         - | 7941 | `	}` |
|        13 | 7942 | `	return PH7_OK;` |
|        11 | 7943 | `}` |
|        10 | 7944 | `static int ph7_hashmap_first(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|         1 | 7945 | `{` |
|        11 | 7946 | `	return HashmapFirstLast(pCtx,nArg,apArg,0);` |
|         1 | 7947 | `}` |
|        10 | 7948 | `static int ph7_hashmap_last(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|         1 | 7949 | `{` |
|        11 | 7950 | `	return HashmapFirstLast(pCtx,nArg,apArg,1);` |
|         1 | 7951 | `}` |
|         - | 7952 | `/*` |
|         - | 7953 | ` * int\|string\|null array_key_first(array $array)` |
|         - | 7954 | ` * int\|string\|null array_key_last(array $array)` |
|         - | 7955 | ` *  Return the key of the first (respectively last) element of the array,` |
|         - | 7956 | ` *  or NULL when the array is empty. The internal array pointer is left` |
|         - | 7957 | ` *  untouched.` |
|         - | 7958 | ` */` |
|        24 | 7959 | `static int HashmapKeyFirstLast(ph7_context *pCtx,int nArg,ph7_value **apArg,int bLast)` |
|         1 | 7960 | `{` |
|         - | 7961 | `	ph7_hashmap *pMap;` |
|         - | 7962 | `	ph7_hashmap_node *pNode;` |
|        25 | 7963 | `	const char *zName = bLast ? "array_key_last" : "array_key_first";` |
|        25 | 7964 | `	if( nArg < 1 ){` |
|         4 | 7965 | `		return PH7_VmThrowException(pCtx,` |
|         - | 7966 | `			"ArgumentCountError",` |
|         - | 7967 | `			"%s() expects exactly 1 argument, 0 given",` |
|         1 | 7968 | `			zName` |
|         - | 7969 | `			);` |
|         - | 7970 | `	}` |
|        23 | 7971 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|         4 | 7972 | `		return PH7_VmThrowException(pCtx,` |
|         - | 7973 | `			"TypeError",` |
|         - | 7974 | `			"%s(): Argument #1 ($array) must be of type array, %s given",` |
|         1 | 7975 | `			zName,` |
|         1 | 7976 | `			ph7_type_name(apArg[0])` |
|         - | 7977 | `			);` |
|         - | 7978 | `	}` |
|        21 | 7979 | `	pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|        21 | 7980 | `	pNode = bLast ? pMap->pLast : pMap->pFirst;` |
|        21 | 7981 | `	if( pNode == 0 ){` |
|         - | 7982 | `		/* Empty array: PHP returns NULL */` |
|         5 | 7983 | `		ph7_result_null(pCtx);` |
|         5 | 7984 | `		return PH7_OK;` |
|         - | 7985 | `	}` |
|        17 | 7986 | `	HashmapResultNodeKey(pCtx,pNode);` |
|        17 | 7987 | `	return PH7_OK;` |
|        13 | 7988 | `}` |
|        12 | 7989 | `static int ph7_hashmap_key_first(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|         1 | 7990 | `{` |
|        13 | 7991 | `	return HashmapKeyFirstLast(pCtx,nArg,apArg,0);` |
|         1 | 7992 | `}` |
|        12 | 7993 | `static int ph7_hashmap_key_last(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|         1 | 7994 | `{` |
|        13 | 7995 | `	return HashmapKeyFirstLast(pCtx,nArg,apArg,1);` |
|         1 | 7996 | `}` |
|         - | 7997 | `/*` |
|         - | 7998 | ` * Fetch the element identified by 'pKey' from 'pRow' which may be either an` |
|         - | 7999 | ` * array (hashmap lookup) or an object (public attribute lookup). Used by` |
|         - | 8000 | ` * array_column() for both the column value and the index key.` |
|         - | 8001 | ` * Returns a borrowed pointer to the value, or NULL when the row is not a` |
|         - | 8002 | ` * container or the key is absent.` |
|         - | 8003 | ` */` |
|        32 | 8004 | `static ph7_value * HashmapColumnFetch(ph7_vm *pVm,ph7_value *pRow,ph7_value *pKey)` |
|         1 | 8005 | `{` |
|        33 | 8006 | `	if( ph7_value_is_array(pRow) ){` |
|         - | 8007 | `		ph7_hashmap_node *pNode;` |
|        25 | 8008 | `		if( PH7_HashmapLookup((ph7_hashmap *)pRow->x.pOther,pKey,&pNode) == SXRET_OK ){` |
|        21 | 8009 | `			return HashmapExtractNodeValue(pNode);` |
|         1 | 8010 | `		}` |
|        11 | 8011 | `	}else if( ph7_value_is_object(pRow) ){` |
|         - | 8012 | `		ph7_value sName;` |
|         - | 8013 | `		const char *zName;` |
|         - | 8014 | `		ph7_value *pAttr;` |
|         - | 8015 | `		/* Stringify a *copy* of the key (objects address attributes by name);` |
|         - | 8016 | `		 * never mutate pKey itself or the array-lookup path would break. */` |
|         9 | 8017 | `		PH7_MemObjInit(pVm,&sName);` |
|         9 | 8018 | `		PH7_MemObjStore(pKey,&sName);` |
|         9 | 8019 | `		zName = ph7_value_to_string(&sName,0); /* NUL-terminated */` |
|         9 | 8020 | `		pAttr = ph7_object_fetch_attr(pRow,zName);` |
|         9 | 8021 | `		PH7_MemObjRelease(&sName);` |
|         9 | 8022 | `		return pAttr;` |
|         - | 8023 | `	}` |
|         5 | 8024 | `	return 0;` |
|        17 | 8025 | `}` |
|         - | 8026 | `/*` |
|         - | 8027 | ` * array array_column(array $array, int\|string\|null $column_key, int\|string\|null $index_key = null)` |
|         - | 8028 | ` *  Returns the values from a single column of the input, identified by` |
|         - | 8029 | ` *  $column_key. Optionally indexes the result by the $index_key column.` |
|         - | 8030 | ` *  A NULL $column_key collects the whole row. Rows missing the column are` |
|         - | 8031 | ` *  skipped; rows missing the index key are appended with a numeric key.` |
|         - | 8032 | ` *  Each row may be an array or an object.` |
|         - | 8033 | ` */` |
|        12 | 8034 | `static int ph7_hashmap_column(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|         1 | 8035 | `{` |
|         - | 8036 | `	ph7_hashmap_node *pNode;` |
|         - | 8037 | `	ph7_hashmap *pMap;` |
|         - | 8038 | `	ph7_value *pArray;` |
|         - | 8039 | `	ph7_value *pRow;` |
|         - | 8040 | `	ph7_value *pCol;` |
|         - | 8041 | `	ph7_value *pIdx;` |
|         - | 8042 | `	int bWantCol;` |
|         - | 8043 | `	int bWantIdx;` |
|         - | 8044 | `	sxu32 n;` |
|        13 | 8045 | `	if( nArg < 2 ){` |
|       ! 0 | 8046 | `		return PH7_VmThrowException(pCtx,` |
|         - | 8047 | `			"ArgumentCountError",` |
|         - | 8048 | `			"array_column() expects at least 2 arguments, %d given",` |
|       ! 0 | 8049 | `			nArg` |
|         - | 8050 | `			);` |
|         - | 8051 | `	}` |
|        13 | 8052 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|       ! 0 | 8053 | `		return PH7_VmThrowException(pCtx,` |
|         - | 8054 | `			"TypeError",` |
|         - | 8055 | `			"array_column(): Argument #1 ($array) must be of type array, %s given",` |
|       ! 0 | 8056 | `			ph7_type_name(apArg[0])` |
|         - | 8057 | `			);` |
|         - | 8058 | `	}` |
|        13 | 8059 | `	pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|        13 | 8060 | `	pArray = ph7_context_new_array(pCtx);` |
|        13 | 8061 | `	if( pArray == 0 ){` |
|       ! 0 | 8062 | `		ph7_result_null(pCtx);` |
|       ! 0 | 8063 | `		return PH7_OK;` |
|         - | 8064 | `	}` |
|         - | 8065 | `	/* A NULL column_key means "collect the entire row". */` |
|        13 | 8066 | `	bWantCol = !ph7_value_is_null(apArg[1]);` |
|        13 | 8067 | `	bWantIdx = (nArg > 2 && !ph7_value_is_null(apArg[2]));` |
|        13 | 8068 | `	pNode = pMap->pFirst;` |
|        33 | 8069 | `	for( n = 0 ; n < pMap->nEntry ; ++n ){` |
|        21 | 8070 | `		pRow = HashmapExtractNodeValue(pNode);` |
|        21 | 8071 | `		pNode = pNode->pPrev; /* Advance now so 'continue' is safe */` |
|        21 | 8072 | `		if( pRow == 0 ){` |
|       ! 0 | 8073 | `			continue;` |
|         - | 8074 | `		}` |
|        21 | 8075 | `		if( bWantCol ){` |
|        19 | 8076 | `			pCol = HashmapColumnFetch(pMap->pVm,pRow,apArg[1]);` |
|        19 | 8077 | `			if( pCol == 0 ){` |
|         - | 8078 | `				/* Row lacks the requested column: skip it (PHP semantics). */` |
|         3 | 8079 | `				continue;` |
|         - | 8080 | `			}` |
|         9 | 8081 | `		}else{` |
|         3 | 8082 | `			pCol = pRow;` |
|         - | 8083 | `		}` |
|        19 | 8084 | `		pIdx = bWantIdx ? HashmapColumnFetch(pMap->pVm,pRow,apArg[2]) : 0;` |
|        19 | 8085 | `		if( pIdx ){` |
|        13 | 8086 | `			ph7_array_add_elem(pArray,pIdx,pCol);` |
|         7 | 8087 | `		}else{` |
|         7 | 8088 | `			ph7_array_add_elem(pArray,0,pCol); /* Auto-index */` |
|         - | 8089 | `		}` |
|        10 | 8090 | `	}` |
|        13 | 8091 | `	ph7_result_value(pCtx,pArray);` |
|        13 | 8092 | `	return PH7_OK;` |
|         7 | 8093 | `}` |
|         - | 8094 | `/*` |
|         - | 8095 | ` * Shared core for array_find/array_find_key/array_any/array_all (PHP 8.4).` |
|         - | 8096 | ` * Invokes $callback($value, $key) over each entry and reports the first node` |
|         - | 8097 | ` * whose truthiness equals 'bWant'. Propagates a callback exception as` |
|         - | 8098 | ` * PH7_EXCEPTION; sets *ppMatch to the matching node (or NULL if none).` |
|         - | 8099 | ` */` |
|        28 | 8100 | `static sxi32 HashmapCallbackSearch(` |
|         - | 8101 | `	ph7_context *pCtx,int nArg,ph7_value **apArg,` |
|         - | 8102 | `	const char *zName,            /* Function name for diagnostics */` |
|         - | 8103 | `	int bWant,                    /* Truthiness being hunted for */` |
|         - | 8104 | `	ph7_hashmap_node **ppMatch    /* OUT: first matching node or NULL */` |
|         - | 8105 | `	)` |
|         1 | 8106 | `{` |
|         - | 8107 | `	ph7_hashmap_node *pEntry;` |
|         - | 8108 | `	ph7_hashmap *pMap;` |
|         - | 8109 | `	ph7_value *pValue;` |
|         - | 8110 | `	ph7_value *apCbArg[2];` |
|         - | 8111 | `	ph7_value sKey;` |
|         - | 8112 | `	ph7_value sResult;` |
|         - | 8113 | `	sxi32 rc;` |
|         - | 8114 | `	sxu32 n;` |
|        29 | 8115 | `	*ppMatch = 0;` |
|        29 | 8116 | `	if( nArg < 2 ){` |
|       ! 0 | 8117 | `		return PH7_VmThrowException(pCtx,` |
|         - | 8118 | `			"ArgumentCountError",` |
|         - | 8119 | `			"%s() expects exactly 2 arguments, %d given",` |
|       ! 0 | 8120 | `			zName,nArg` |
|         - | 8121 | `			);` |
|         - | 8122 | `	}` |
|        29 | 8123 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|       ! 0 | 8124 | `		return PH7_VmThrowException(pCtx,` |
|         - | 8125 | `			"TypeError",` |
|         - | 8126 | `			"%s(): Argument #1 ($array) must be of type array, %s given",` |
|       ! 0 | 8127 | `			zName,ph7_type_name(apArg[0])` |
|         - | 8128 | `			);` |
|         - | 8129 | `	}` |
|        29 | 8130 | `	if( !ph7_value_is_callable(apArg[1]) ){` |
|       ! 0 | 8131 | `		return PH7_VmThrowException(pCtx,` |
|         - | 8132 | `			"TypeError",` |
|         - | 8133 | `			"%s(): Argument #2 ($callback) must be a valid callback, %s given",` |
|       ! 0 | 8134 | `			zName,ph7_type_name(apArg[1])` |
|         - | 8135 | `			);` |
|         - | 8136 | `	}` |
|        29 | 8137 | `	pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|        29 | 8138 | `	pEntry = pMap->pFirst;` |
|        29 | 8139 | `	PH7_MemObjInit(pMap->pVm,&sKey);` |
|        29 | 8140 | `	sKey.nIdx = SXU32_HIGH;    /* Mark as constant */` |
|        29 | 8141 | `	PH7_MemObjInit(pMap->pVm,&sResult);` |
|        29 | 8142 | `	sResult.nIdx = SXU32_HIGH; /* Mark as constant */` |
|        73 | 8143 | `	for( n = 0 ; n < pMap->nEntry ; ++n ){` |
|        59 | 8144 | `		pValue = HashmapExtractNodeValue(pEntry);` |
|        59 | 8145 | `		if( pValue ){` |
|         - | 8146 | `			/* The callback receives ($value, $key). */` |
|        59 | 8147 | `			PH7_HashmapExtractNodeKey(pEntry,&sKey);` |
|        59 | 8148 | `			apCbArg[0] = pValue;` |
|        59 | 8149 | `			apCbArg[1] = &sKey;` |
|        59 | 8150 | `			rc = PH7_VmCallUserFunction(pMap->pVm,apArg[1],2,apCbArg,&sResult);` |
|        59 | 8151 | `			if( rc == PH7_EXCEPTION ){` |
|         - | 8152 | `				/* The callback raised: propagate so the dispatcher unwinds. */` |
|       ! 0 | 8153 | `				PH7_MemObjRelease(&sKey);` |
|       ! 0 | 8154 | `				PH7_MemObjRelease(&sResult);` |
|       ! 0 | 8155 | `				return PH7_EXCEPTION;` |
|         - | 8156 | `			}` |
|        59 | 8157 | `			if( rc == SXRET_OK && (ph7_value_to_bool(&sResult) ? 1 : 0) == bWant ){` |
|        15 | 8158 | `				*ppMatch = pEntry;` |
|        15 | 8159 | `				break;` |
|         - | 8160 | `			}` |
|        22 | 8161 | `		}` |
|        45 | 8162 | `		pEntry = pEntry->pPrev; /* Reverse link */` |
|        23 | 8163 | `	}` |
|        29 | 8164 | `	PH7_MemObjRelease(&sKey);` |
|        29 | 8165 | `	PH7_MemObjRelease(&sResult);` |
|        29 | 8166 | `	return PH7_OK;` |
|        15 | 8167 | `}` |
|         - | 8168 | `/*` |
|         - | 8169 | ` * mixed array_find(array $array, callable $callback)` |
|         - | 8170 | ` *  Returns the value of the first element for which $callback($value,$key)` |
|         - | 8171 | ` *  is truthy, or NULL if none match.` |
|         - | 8172 | ` */` |
|         6 | 8173 | `static int ph7_hashmap_find(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|         1 | 8174 | `{` |
|         - | 8175 | `	ph7_hashmap_node *pMatch;` |
|         - | 8176 | `	ph7_value *pVal;` |
|         - | 8177 | `	sxi32 rc;` |
|         7 | 8178 | `	rc = HashmapCallbackSearch(pCtx,nArg,apArg,"array_find",1,&pMatch);` |
|         7 | 8179 | `	if( rc != PH7_OK ){` |
|       ! 0 | 8180 | `		return rc;` |
|         - | 8181 | `	}` |
|         7 | 8182 | `	if( pMatch && (pVal = HashmapExtractNodeValue(pMatch)) != 0 ){` |
|         5 | 8183 | `		ph7_result_value(pCtx,pVal);` |
|         3 | 8184 | `	}else{` |
|         3 | 8185 | `		ph7_result_null(pCtx);` |
|         - | 8186 | `	}` |
|         7 | 8187 | `	return PH7_OK;` |
|         4 | 8188 | `}` |
|         - | 8189 | `/*` |
|         - | 8190 | ` * mixed array_find_key(array $array, callable $callback)` |
|         - | 8191 | ` *  Returns the key of the first element for which $callback($value,$key)` |
|         - | 8192 | ` *  is truthy, or NULL if none match.` |
|         - | 8193 | ` */` |
|         6 | 8194 | `static int ph7_hashmap_find_key(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|         1 | 8195 | `{` |
|         - | 8196 | `	ph7_hashmap_node *pMatch;` |
|         - | 8197 | `	sxi32 rc;` |
|         7 | 8198 | `	rc = HashmapCallbackSearch(pCtx,nArg,apArg,"array_find_key",1,&pMatch);` |
|         7 | 8199 | `	if( rc != PH7_OK ){` |
|       ! 0 | 8200 | `		return rc;` |
|         - | 8201 | `	}` |
|         7 | 8202 | `	if( pMatch == 0 ){` |
|         3 | 8203 | `		ph7_result_null(pCtx);` |
|         6 | 8204 | `	}else if( pMatch->iType == HASHMAP_INT_NODE ){` |
|         3 | 8205 | `		ph7_result_int64(pCtx,pMatch->xKey.iKey);` |
|         2 | 8206 | `	}else{` |
|         4 | 8207 | `		ph7_result_string(pCtx,` |
|         2 | 8208 | `			(const char *)SyBlobData(&pMatch->xKey.sKey),` |
|         2 | 8209 | `			(int)SyBlobLength(&pMatch->xKey.sKey));` |
|         - | 8210 | `	}` |
|         7 | 8211 | `	return PH7_OK;` |
|         4 | 8212 | `}` |
|         - | 8213 | `/*` |
|         - | 8214 | ` * bool array_any(array $array, callable $callback)` |
|         - | 8215 | ` *  Returns TRUE if $callback($value,$key) is truthy for at least one element.` |
|         - | 8216 | ` *  FALSE for an empty array.` |
|         - | 8217 | ` */` |
|         8 | 8218 | `static int ph7_hashmap_any(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|         1 | 8219 | `{` |
|         - | 8220 | `	ph7_hashmap_node *pMatch;` |
|         - | 8221 | `	sxi32 rc;` |
|         9 | 8222 | `	rc = HashmapCallbackSearch(pCtx,nArg,apArg,"array_any",1,&pMatch);` |
|         9 | 8223 | `	if( rc != PH7_OK ){` |
|       ! 0 | 8224 | `		return rc;` |
|         - | 8225 | `	}` |
|         9 | 8226 | `	ph7_result_bool(pCtx,pMatch != 0);` |
|         9 | 8227 | `	return PH7_OK;` |
|         5 | 8228 | `}` |
|         - | 8229 | `/*` |
|         - | 8230 | ` * bool array_all(array $array, callable $callback)` |
|         - | 8231 | ` *  Returns TRUE if $callback($value,$key) is truthy for every element (and for` |
|         - | 8232 | ` *  an empty array). Hunts for the first falsy element: its absence means "all".` |
|         - | 8233 | ` */` |
|         8 | 8234 | `static int ph7_hashmap_all(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|         1 | 8235 | `{` |
|         - | 8236 | `	ph7_hashmap_node *pMatch;` |
|         - | 8237 | `	sxi32 rc;` |
|         9 | 8238 | `	rc = HashmapCallbackSearch(pCtx,nArg,apArg,"array_all",0,&pMatch);` |
|         9 | 8239 | `	if( rc != PH7_OK ){` |
|       ! 0 | 8240 | `		return rc;` |
|         - | 8241 | `	}` |
|         9 | 8242 | `	ph7_result_bool(pCtx,pMatch == 0);` |
|         9 | 8243 | `	return PH7_OK;` |
|         5 | 8244 | `}` |
|         - | 8245 | `/*` |
|         - | 8246 | ` * The iterator_*() family — walk a Traversable via the shared PH7_VmIteratorWalk` |
|         - | 8247 | ` * helper (the reusable form of the foreach Iterator protocol).` |
|         - | 8248 | ` */` |
|         - | 8249 | `/* Step shared by iterator_to_array (pArray set) and iterator_count (pArray NULL). */` |
|         - | 8250 | `struct IterCollect { ph7_value *pArray; int bPreserve; sxi64 nCount; };` |
|        70 | 8251 | `static sxi32 IterCollectStep(ph7_vm *pVm, ph7_value *pKey, ph7_value *pValue, void *pUserData)` |
|         4 | 8252 | `{` |
|        74 | 8253 | `	struct IterCollect *p = (struct IterCollect *)pUserData;` |
|        35 | 8254 | `	(void)pVm;` |
|        74 | 8255 | `	p->nCount++;` |
|        74 | 8256 | `	if( p->pArray ){` |
|         - | 8257 | `		/* preserve_keys: insert with the iterator key (later wins on collision);` |
|         - | 8258 | `		 * otherwise append with an auto-assigned int index. */` |
|        66 | 8259 | `		ph7_array_add_elem(p->pArray, p->bPreserve ? pKey : 0, pValue);` |
|        31 | 8260 | `	}` |
|        74 | 8261 | `	return SXRET_OK;` |
|         4 | 8262 | `}` |
|         - | 8263 | `/*` |
|         - | 8264 | ` * array iterator_to_array(Traversable\|array $iterator, bool $preserve_keys = true)` |
|         - | 8265 | ` */` |
|        26 | 8266 | `static int ph7_iterator_to_array(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|         4 | 8267 | `{` |
|         - | 8268 | `	struct IterCollect sCol;` |
|         - | 8269 | `	ph7_value *pArray;` |
|         - | 8270 | `	sxi32 rc;` |
|        30 | 8271 | `	if( nArg < 1 ){ ph7_result_null(pCtx); return PH7_OK; }` |
|        30 | 8272 | `	pArray = ph7_context_new_array(pCtx);` |
|        30 | 8273 | `	if( pArray == 0 ){ ph7_result_null(pCtx); return PH7_OK; }` |
|        30 | 8274 | `	sCol.pArray = pArray;` |
|        30 | 8275 | `	sCol.bPreserve = (nArg > 1) ? ph7_value_to_bool(apArg[1]) : 1;` |
|        30 | 8276 | `	sCol.nCount = 0;` |
|        30 | 8277 | `	if( ph7_value_is_array(apArg[0]) ){` |
|         - | 8278 | `		/* PHP 8.2 accepts a plain array: copy it (preserving or renumbering keys). */` |
|         3 | 8279 | `		ph7_hashmap *pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|         3 | 8280 | `		ph7_hashmap_node *pEntry = pMap->pFirst;` |
|         - | 8281 | `		sxu32 n;` |
|         9 | 8282 | `		for( n = 0 ; n < pMap->nEntry ; n++ ){` |
|         - | 8283 | `			ph7_value sKey, *pVal;` |
|         7 | 8284 | `			PH7_MemObjInit(pCtx->pVm,&sKey);` |
|         7 | 8285 | `			PH7_HashmapExtractNodeKey(pEntry,&sKey);` |
|         7 | 8286 | `			pVal = (ph7_value *)SySetAt(&pCtx->pVm->aMemObj,pEntry->nValIdx);` |
|         7 | 8287 | `			if( pVal ){ ph7_array_add_elem(pArray, sCol.bPreserve ? &sKey : 0, pVal); }` |
|         7 | 8288 | `			PH7_MemObjRelease(&sKey);` |
|         7 | 8289 | `			pEntry = pEntry->pPrev;` |
|         4 | 8290 | `		}` |
|         3 | 8291 | `		ph7_result_value(pCtx,pArray);` |
|         3 | 8292 | `		return PH7_OK;` |
|         - | 8293 | `	}` |
|        28 | 8294 | `	rc = PH7_VmIteratorWalk(pCtx->pVm, apArg[0], IterCollectStep, &sCol);` |
|        28 | 8295 | `	if( rc == PH7_EXCEPTION \|\| rc == PH7_ABORT ){ return rc; }` |
|        26 | 8296 | `	if( rc == SXERR_NOTIMPLEMENTED ){` |
|       ! 0 | 8297 | `		return PH7_VmThrowException(pCtx,"TypeError",` |
|         - | 8298 | `			"iterator_to_array(): Argument #1 ($iterator) must be of type Traversable\|array, %s given",` |
|       ! 0 | 8299 | `			ph7_type_name(apArg[0]));` |
|         - | 8300 | `	}` |
|        26 | 8301 | `	ph7_result_value(pCtx,pArray);` |
|        26 | 8302 | `	return PH7_OK;` |
|        17 | 8303 | `}` |
|         - | 8304 | `/*` |
|         - | 8305 | ` * int iterator_count(Traversable\|array $iterator)` |
|         - | 8306 | ` */` |
|         6 | 8307 | `static int ph7_iterator_count(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|         1 | 8308 | `{` |
|         - | 8309 | `	struct IterCollect sCol;` |
|         - | 8310 | `	sxi32 rc;` |
|         7 | 8311 | `	if( nArg < 1 ){ ph7_result_int(pCtx,0); return PH7_OK; }` |
|         7 | 8312 | `	if( ph7_value_is_array(apArg[0]) ){` |
|         3 | 8313 | `		ph7_result_int64(pCtx, (ph7_int64)((ph7_hashmap *)apArg[0]->x.pOther)->nEntry);` |
|         3 | 8314 | `		return PH7_OK;` |
|         - | 8315 | `	}` |
|         5 | 8316 | `	sCol.pArray = 0; sCol.bPreserve = 0; sCol.nCount = 0;` |
|         5 | 8317 | `	rc = PH7_VmIteratorWalk(pCtx->pVm, apArg[0], IterCollectStep, &sCol);` |
|         5 | 8318 | `	if( rc == PH7_EXCEPTION \|\| rc == PH7_ABORT ){ return rc; }` |
|         5 | 8319 | `	if( rc == SXERR_NOTIMPLEMENTED ){` |
|       ! 0 | 8320 | `		return PH7_VmThrowException(pCtx,"TypeError",` |
|         - | 8321 | `			"iterator_count(): Argument #1 ($iterator) must be of type Traversable\|array, %s given",` |
|       ! 0 | 8322 | `			ph7_type_name(apArg[0]));` |
|         - | 8323 | `	}` |
|         5 | 8324 | `	ph7_result_int64(pCtx, sCol.nCount);` |
|         5 | 8325 | `	return PH7_OK;` |
|         4 | 8326 | `}` |
|         - | 8327 | `/* iterator_apply step: call the fixed callback with $args each iteration. The` |
|         - | 8328 | ` * arg pointers are resolved fresh per step because the iterator's own methods` |
|         - | 8329 | ` * run user code between iterations and may reallocate the aMemObj pool. */` |
|         - | 8330 | `struct IterApply { ph7_value *pCallback; ph7_value *pArgsArray; sxi64 nCount; };` |
|        24 | 8331 | `static sxi32 IterApplyStep(ph7_vm *pVm, ph7_value *pKey, ph7_value *pValue, void *pUserData)` |
|         1 | 8332 | `{` |
|        25 | 8333 | `	struct IterApply *p = (struct IterApply *)pUserData;` |
|         - | 8334 | `	ph7_value sResult;` |
|         - | 8335 | `	SySet aArg;` |
|         - | 8336 | `	sxi32 rc;` |
|         - | 8337 | `	int bContinue;` |
|        12 | 8338 | `	(void)pKey; (void)pValue; /* iterator_apply does NOT pass the element to the callback */` |
|        25 | 8339 | `	SySetInit(&aArg,&pVm->sAllocator,sizeof(ph7_value *));` |
|        25 | 8340 | `	if( p->pArgsArray && (p->pArgsArray->iFlags & MEMOBJ_HASHMAP) ){` |
|         9 | 8341 | `		ph7_hashmap *pMap = (ph7_hashmap *)p->pArgsArray->x.pOther;` |
|         9 | 8342 | `		ph7_hashmap_node *pEntry = pMap->pFirst;` |
|         - | 8343 | `		sxu32 n;` |
|        17 | 8344 | `		for( n = 0 ; n < pMap->nEntry ; n++ ){` |
|         9 | 8345 | `			ph7_value *pVal = (ph7_value *)SySetAt(&pVm->aMemObj,pEntry->nValIdx);` |
|         9 | 8346 | `			if( pVal ){ SySetPut(&aArg,(const void *)&pVal); }` |
|         9 | 8347 | `			pEntry = pEntry->pPrev;` |
|         5 | 8348 | `		}` |
|         4 | 8349 | `	}` |
|        25 | 8350 | `	PH7_MemObjInit(pVm,&sResult);` |
|        37 | 8351 | `	rc = PH7_VmCallUserFunction(pVm, p->pCallback, (int)SySetUsed(&aArg),` |
|        24 | 8352 | `		(ph7_value **)SySetBasePtr(&aArg), &sResult);` |
|        25 | 8353 | `	SySetRelease(&aArg);` |
|        25 | 8354 | `	if( rc == PH7_EXCEPTION \|\| rc == PH7_ABORT ){ PH7_MemObjRelease(&sResult); return rc; }` |
|        23 | 8355 | `	p->nCount++;` |
|        23 | 8356 | `	PH7_MemObjToBool(&sResult);` |
|        23 | 8357 | `	bContinue = (sResult.x.iVal != 0);` |
|        23 | 8358 | `	PH7_MemObjRelease(&sResult);` |
|        23 | 8359 | `	return bContinue ? SXRET_OK : SXERR_EOF; /* falsy return stops iteration */` |
|        13 | 8360 | `}` |
|         - | 8361 | `/*` |
|         - | 8362 | ` * int iterator_apply(Traversable $iterator, callable $callback, array $args = [])` |
|         - | 8363 | ` */` |
|         8 | 8364 | `static int ph7_iterator_apply(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|         1 | 8365 | `{` |
|         - | 8366 | `	struct IterApply sApp;` |
|         - | 8367 | `	sxi32 rc;` |
|         9 | 8368 | `	if( nArg < 2 ){ ph7_result_int(pCtx,0); return PH7_OK; }` |
|         9 | 8369 | `	if( !ph7_value_is_callable(apArg[1]) ){` |
|       ! 0 | 8370 | `		return PH7_VmThrowException(pCtx,"TypeError",` |
|         - | 8371 | `			"iterator_apply(): Argument #2 ($callback) must be a valid callback");` |
|         - | 8372 | `	}` |
|         9 | 8373 | `	sApp.pCallback = apArg[1];` |
|         9 | 8374 | `	sApp.pArgsArray = (nArg > 2 && ph7_value_is_array(apArg[2])) ? apArg[2] : 0;` |
|         9 | 8375 | `	sApp.nCount = 0;` |
|         9 | 8376 | `	rc = PH7_VmIteratorWalk(pCtx->pVm, apArg[0], IterApplyStep, &sApp);` |
|         9 | 8377 | `	if( rc == PH7_EXCEPTION \|\| rc == PH7_ABORT ){ return rc; }` |
|         7 | 8378 | `	if( rc == SXERR_NOTIMPLEMENTED ){` |
|       ! 0 | 8379 | `		return PH7_VmThrowException(pCtx,"TypeError",` |
|         - | 8380 | `			"iterator_apply(): Argument #1 ($iterator) must be of type Traversable, %s given",` |
|       ! 0 | 8381 | `			ph7_type_name(apArg[0]));` |
|         - | 8382 | `	}` |
|         7 | 8383 | `	ph7_result_int64(pCtx, sApp.nCount);` |
|         7 | 8384 | `	return PH7_OK;` |
|         5 | 8385 | `}` |
|         - | 8386 | `/*` |
|         - | 8387 | ` * Table of hashmap functions.` |
|         - | 8388 | ` */` |
|         - | 8389 | `static const ph7_builtin_func aHashmapFunc[] = {` |
|         - | 8390 | `	{"iterator_to_array",  ph7_iterator_to_array },` |
|         - | 8391 | `	{"iterator_count",     ph7_iterator_count },` |
|         - | 8392 | `	{"iterator_apply",     ph7_iterator_apply },` |
|         - | 8393 | `	{"count",             ph7_hashmap_count },` |
|         - | 8394 | `	{"sizeof",            ph7_hashmap_count },` |
|         - | 8395 | `	{"array_key_exists",  ph7_hashmap_key_exists },` |
|         - | 8396 | `	{"array_pop",         ph7_hashmap_pop     },` |
|         - | 8397 | `	{"array_push",        ph7_hashmap_push    },` |
|         - | 8398 | `	{"array_shift",       ph7_hashmap_shift   },` |
|         - | 8399 | `	{"array_product",     ph7_hashmap_product },` |
|         - | 8400 | `	{"array_sum",         ph7_hashmap_sum     },` |
|         - | 8401 | `	{"array_keys",        ph7_hashmap_keys    },` |
|         - | 8402 | `	{"array_values",      ph7_hashmap_values  },` |
|         - | 8403 | `	{"array_same",        ph7_hashmap_same    },  /* Symisc eXtension */` |
|         - | 8404 | `	{"array_merge",       ph7_hashmap_merge   },` |
|         - | 8405 | `	{"array_slice",       ph7_hashmap_slice   },` |
|         - | 8406 | `	{"array_splice",      ph7_hashmap_splice  },` |
|         - | 8407 | `	{"array_search",      ph7_hashmap_search  },` |
|         - | 8408 | `	{"array_diff",        ph7_hashmap_diff    },` |
|         - | 8409 | `	{"array_udiff",       ph7_hashmap_udiff   },` |
|         - | 8410 | `	{"array_diff_assoc",  ph7_hashmap_diff_assoc },` |
|         - | 8411 | `	{"array_diff_uassoc", ph7_hashmap_diff_uassoc },` |
|         - | 8412 | `	{"array_diff_key",    ph7_hashmap_diff_key },` |
|         - | 8413 | `	{"array_intersect",   ph7_hashmap_intersect},` |
|         - | 8414 | `	{"array_intersect_assoc", ph7_hashmap_intersect_assoc},` |
|         - | 8415 | `	{"array_uintersect",  ph7_hashmap_uintersect},` |
|         - | 8416 | `	{"array_intersect_key",   ph7_hashmap_intersect_key},` |
|         - | 8417 | `	{"array_copy",        ph7_hashmap_copy    },` |
|         - | 8418 | `	{"array_erase",       ph7_hashmap_erase   },` |
|         - | 8419 | `	{"array_fill",        ph7_hashmap_fill    },` |
|         - | 8420 | `	{"array_fill_keys",   ph7_hashmap_fill_keys},` |
|         - | 8421 | `	{"array_combine",     ph7_hashmap_combine },` |
|         - | 8422 | `	{"array_reverse",     ph7_hashmap_reverse },` |
|         - | 8423 | `	{"array_unique",      ph7_hashmap_unique  },` |
|         - | 8424 | `	{"array_flip",        ph7_hashmap_flip    },` |
|         - | 8425 | `	{"array_rand",        ph7_hashmap_rand    },` |
|         - | 8426 | `	{"array_chunk",       ph7_hashmap_chunk   },` |
|         - | 8427 | `	{"array_pad",         ph7_hashmap_pad     },` |
|         - | 8428 | `	{"array_replace",     ph7_hashmap_replace },` |
|         - | 8429 | `	{"array_filter",      ph7_hashmap_filter  },` |
|         - | 8430 | `	{"array_map",         ph7_hashmap_map     },` |
|         - | 8431 | `	{"array_column",      ph7_hashmap_column  },` |
|         - | 8432 | `	{"array_is_list",     ph7_hashmap_is_list },` |
|         - | 8433 | `	{"array_first",       ph7_hashmap_first   },` |
|         - | 8434 | `	{"array_last",        ph7_hashmap_last    },` |
|         - | 8435 | `	{"array_key_first",   ph7_hashmap_key_first },` |
|         - | 8436 | `	{"array_key_last",    ph7_hashmap_key_last  },` |
|         - | 8437 | `	{"array_find",        ph7_hashmap_find    },` |
|         - | 8438 | `	{"array_find_key",    ph7_hashmap_find_key},` |
|         - | 8439 | `	{"array_any",         ph7_hashmap_any     },` |
|         - | 8440 | `	{"array_all",         ph7_hashmap_all     },` |
|         - | 8441 | `	{"array_reduce",      ph7_hashmap_reduce  },` |
|         - | 8442 | `	{"array_walk",        ph7_hashmap_walk    },` |
|         - | 8443 | `	{"array_walk_recursive", ph7_hashmap_walk_recursive },` |
|         - | 8444 | `	{"in_array",          ph7_hashmap_in_array},` |
|         - | 8445 | `	{"sort",              ph7_hashmap_sort    },` |
|         - | 8446 | `	{"asort",             ph7_hashmap_asort   },` |
|         - | 8447 | `	{"arsort",            ph7_hashmap_arsort  },` |
|         - | 8448 | `	{"ksort",             ph7_hashmap_ksort   },` |
|         - | 8449 | `	{"krsort",            ph7_hashmap_krsort  },` |
|         - | 8450 | `	{"rsort",             ph7_hashmap_rsort   },` |
|         - | 8451 | `	{"usort",             ph7_hashmap_usort   },` |
|         - | 8452 | `	{"uasort",            ph7_hashmap_uasort  },` |
|         - | 8453 | `	{"uksort",            ph7_hashmap_uksort  },` |
|         - | 8454 | `	{"shuffle",           ph7_hashmap_shuffle },` |
|         - | 8455 | `	{"range",             ph7_hashmap_range   },` |
|         - | 8456 | `	{"current",           ph7_hashmap_current },` |
|         - | 8457 | `	{"each",              ph7_hashmap_each    },` |
|         - | 8458 | `	{"pos",               ph7_hashmap_current },` |
|         - | 8459 | `	{"next",              ph7_hashmap_next    },` |
|         - | 8460 | `	{"prev",              ph7_hashmap_prev    },` |
|         - | 8461 | `	{"end",               ph7_hashmap_end     },` |
|         - | 8462 | `	{"reset",             ph7_hashmap_reset   },` |
|         - | 8463 | `	{"key",               ph7_hashmap_simple_key }` |
|         - | 8464 | `};` |
|         - | 8465 | `/*` |
|         - | 8466 | ` * Register the built-in hashmap functions defined above.` |
|         - | 8467 | ` */` |
|      3474 | 8468 | `PH7_PRIVATE void PH7_RegisterHashmapFunctions(ph7_vm *pVm)` |
|         5 | 8469 | `{` |
|         - | 8470 | `	sxu32 n;` |
|    260555 | 8471 | `	for( n = 0 ; n < SX_ARRAYSIZE(aHashmapFunc) ; n++ ){` |
|    257081 | 8472 | `		ph7_create_function(&(*pVm),aHashmapFunc[n].zName,aHashmapFunc[n].xFunc,0);` |
|    128543 | 8473 | `	}` |
|      3479 | 8474 | `}` |
|         - | 8475 | `/*` |
|         - | 8476 | ` * Dump a hashmap instance and it's entries and the store the dump in` |
|         - | 8477 | ` * the BLOB given as the first argument.` |
|         - | 8478 | ` * This function is typically invoked when the user issue a call to` |
|         - | 8479 | ` * [var_dump(),var_export(),print_r(),...]` |
|         - | 8480 | ` * This function SXRET_OK on success. Any other return value including` |
|         - | 8481 | ` * SXERR_LIMIT(infinite recursion) indicates failure.` |
|         - | 8482 | ` */` |
|         - | 8483 | `/*` |
|         - | 8484 | ` * Dump the entries of a hashmap [i.e: the key/value lines between the opening` |
|         - | 8485 | ` * '{' and the closing '}'] in the var_dump/print_r style. Factored out of` |
|         - | 8486 | ` * PH7_HashmapDump so the var_dump object renderer can reuse it for a` |
|         - | 8487 | ` * __debugInfo() array body (which carries an object header, not "array(N)").` |
|         - | 8488 | ` * Returns SXERR_LIMIT if a nested value hit the depth cap.` |
|         - | 8489 | ` */` |
|        26 | 8490 | `PH7_PRIVATE sxi32 PH7_HashmapDumpEntries(SyBlob *pOut,ph7_hashmap *pMap,int ShowType,int nTab,int nDepth)` |
|         2 | 8491 | `{` |
|        28 | 8492 | `	ph7_hashmap_node *pEntry = pMap->pFirst;` |
|         - | 8493 | `	ph7_value *pObj;` |
|        28 | 8494 | `	sxu32 n = 0;` |
|         - | 8495 | `	int isRef;` |
|        28 | 8496 | `	sxi32 rc = SXRET_OK;` |
|         - | 8497 | `	int i;` |
|        44 | 8498 | `	for(;;){` |
|        90 | 8499 | `		if( n >= pMap->nEntry ){` |
|        28 | 8500 | `			break;` |
|         - | 8501 | `		}` |
|       126 | 8502 | `		for( i = 0 ; i < nTab ; i++ ){` |
|        64 | 8503 | `			SyBlobAppend(&(*pOut)," ",sizeof(char));` |
|        33 | 8504 | `		}` |
|         - | 8505 | `		/* Dump key */` |
|        64 | 8506 | `		if( pEntry->iType == HASHMAP_INT_NODE){` |
|        33 | 8507 | `			SyBlobFormat(&(*pOut),"[%qd] =>",pEntry->xKey.iKey);` |
|        17 | 8508 | `		}else{` |
|        47 | 8509 | `			SyBlobFormat(&(*pOut),"[%.*s] =>",` |
|        15 | 8510 | `				SyBlobLength(&pEntry->xKey.sKey),SyBlobData(&pEntry->xKey.sKey));` |
|         - | 8511 | `		}` |
|         - | 8512 | `#ifdef __WINNT__` |
|         2 | 8513 | `		SyBlobAppend(&(*pOut),"\r\n",sizeof("\r\n")-1);` |
|         - | 8514 | `#else` |
|        62 | 8515 | `		SyBlobAppend(&(*pOut),"\n",sizeof(char));` |
|         - | 8516 | `#endif` |
|         - | 8517 | `		/* Dump node value */` |
|        64 | 8518 | `		pObj = HashmapExtractNodeValue(pEntry);` |
|        64 | 8519 | `		isRef = 0;` |
|        64 | 8520 | `		if( pObj ){` |
|        64 | 8521 | `			if( pEntry->iFlags & HASHMAP_NODE_FOREIGN_OBJ ){` |
|         - | 8522 | `				/* Referenced object */` |
|       ! 0 | 8523 | `				isRef = 1;` |
|       ! 0 | 8524 | `			}` |
|        64 | 8525 | `			rc = PH7_MemObjDump(&(*pOut),pObj,ShowType,nTab+1,nDepth,isRef);` |
|        64 | 8526 | `			if( rc == SXERR_LIMIT ){` |
|       ! 0 | 8527 | `				break;` |
|         - | 8528 | `			}` |
|        31 | 8529 | `		}` |
|         - | 8530 | `		/* Point to the next entry */` |
|        64 | 8531 | `		n++;` |
|        64 | 8532 | `		pEntry = pEntry->pPrev; /* Reverse link */` |
|         2 | 8533 | `	}` |
|        28 | 8534 | `	return rc;` |
|         2 | 8535 | `}` |
|        22 | 8536 | `PH7_PRIVATE sxi32 PH7_HashmapDump(SyBlob *pOut,ph7_hashmap *pMap,int ShowType,int nTab,int nDepth)` |
|         2 | 8537 | `{` |
|         - | 8538 | `	sxi32 rc;` |
|         - | 8539 | `	int i;` |
|        24 | 8540 | `	if( nDepth > 31 ){` |
|         - | 8541 | `		static const char zInfinite[] = "Nesting limit reached: Infinite recursion?";` |
|         - | 8542 | `		/* Nesting limit reached */` |
|       ! 0 | 8543 | `		SyBlobAppend(&(*pOut),zInfinite,sizeof(zInfinite)-1);` |
|       ! 0 | 8544 | `		if( ShowType ){` |
|       ! 0 | 8545 | `			SyBlobAppend(&(*pOut),")",sizeof(char));` |
|       ! 0 | 8546 | `		}` |
|       ! 0 | 8547 | `		return SXERR_LIMIT;` |
|         - | 8548 | `	}` |
|        24 | 8549 | `	if( !ShowType ){` |
|        11 | 8550 | `		SyBlobAppend(&(*pOut),"Array(",sizeof("Array(")-1);` |
|         5 | 8551 | `	}` |
|         - | 8552 | `	/* Total entries */` |
|        24 | 8553 | `	SyBlobFormat(&(*pOut),"%u) {",pMap->nEntry);` |
|         - | 8554 | `#ifdef __WINNT__` |
|         2 | 8555 | `	SyBlobAppend(&(*pOut),"\r\n",sizeof("\r\n")-1);` |
|         - | 8556 | `#else` |
|        22 | 8557 | `	SyBlobAppend(&(*pOut),"\n",sizeof(char));` |
|         - | 8558 | `#endif` |
|        24 | 8559 | `	rc = PH7_HashmapDumpEntries(&(*pOut),pMap,ShowType,nTab,nDepth);` |
|        46 | 8560 | `	for( i = 0 ; i < nTab ; i++ ){` |
|        24 | 8561 | `		SyBlobAppend(&(*pOut)," ",sizeof(char));` |
|        13 | 8562 | `	}` |
|        24 | 8563 | `	SyBlobAppend(&(*pOut),"}",sizeof(char));` |
|        24 | 8564 | `	return rc;` |
|        13 | 8565 | `}` |
|         - | 8566 | `/*` |
|         - | 8567 | ` * Iterate throw hashmap entries and invoke the given callback [i.e: xWalk()] for each` |
|         - | 8568 | ` * retrieved entry.` |
|         - | 8569 | ` * Note that argument are passed to the callback by copy. That is,any modification to` |
|         - | 8570 | ` * the entry value in the callback body will not alter the real value.` |
|         - | 8571 | ` * If the callback wishes to abort processing [i.e: it's invocation] it must return` |
|         - | 8572 | ` * a value different from PH7_OK.` |
|         - | 8573 | ` * Refer to [ph7_array_walk()] for more information.` |
|         - | 8574 | ` */` |
|     33488 | 8575 | `PH7_PRIVATE sxi32 PH7_HashmapWalk(` |
|         - | 8576 | `	ph7_hashmap *pMap, /* Target hashmap */` |
|         - | 8577 | `	int (*xWalk)(ph7_value *,ph7_value *,void *), /* Walker callback */` |
|         - | 8578 | `	void *pUserData /* Last argument to xWalk() */` |
|         - | 8579 | `	)` |
|         5 | 8580 | `{` |
|         - | 8581 | `	ph7_hashmap_node *pEntry;` |
|         - | 8582 | `	ph7_value sKey,sValue;` |
|         - | 8583 | `	sxi32 rc;` |
|         - | 8584 | `	sxu32 n;` |
|         - | 8585 | `	/* Initialize walker parameter */` |
|     33493 | 8586 | `	rc = SXRET_OK;` |
|     33493 | 8587 | `	PH7_MemObjInit(pMap->pVm,&sKey);` |
|     33493 | 8588 | `	PH7_MemObjInit(pMap->pVm,&sValue);` |
|     33493 | 8589 | `	n = pMap->nEntry;` |
|     33493 | 8590 | `	pEntry = pMap->pFirst;` |
|         - | 8591 | `	/* Start the iteration process */` |
|     87756 | 8592 | `	for(;;){` |
|    175517 | 8593 | `		if( n < 1 ){` |
|     33493 | 8594 | `			break;` |
|         - | 8595 | `		}` |
|         - | 8596 | `		/* Extract a copy of the key and a copy the current value */` |
|    142029 | 8597 | `		PH7_HashmapExtractNodeKey(pEntry,&sKey);` |
|    142029 | 8598 | `		PH7_HashmapExtractNodeValue(pEntry,&sValue,FALSE);` |
|         - | 8599 | `		/* Invoke the user callback */` |
|    142029 | 8600 | `		rc = xWalk(&sKey,&sValue,pUserData);` |
|         - | 8601 | `		/* Release the copy of the key and the value */` |
|    142029 | 8602 | `		PH7_MemObjRelease(&sKey);` |
|    142029 | 8603 | `		PH7_MemObjRelease(&sValue);` |
|    142029 | 8604 | `		if( rc != PH7_OK ){` |
|         - | 8605 | `			/* Callback request an operation abort */` |
|       ! 0 | 8606 | `			return SXERR_ABORT;` |
|         - | 8607 | `		}` |
|         - | 8608 | `		/* Point to the next entry */` |
|    142029 | 8609 | `		pEntry = pEntry->pPrev; /* Reverse link */` |
|    142029 | 8610 | `		n--;` |
|         5 | 8611 | `	}` |
|         - | 8612 | `	/* All done */` |
|     33493 | 8613 | `	return SXRET_OK;` |
|     16749 | 8614 | `}` |
|         - | 8615 |  |
