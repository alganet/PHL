# src/ph7/hashmap.c

<style>code, pre { background: none !important; white-space: pre !important; width: 100% !important; display: inline-block !important; } td { border: none !important; margin-top: 0 !important; margin-bottom: 0 !important; padding-top: 0 !important; padding-bottom: 0 !important; }</style>

Coverage: 3717/4229 lines (87.89%)

[Root index](../../index.md) | [Directory index](index.md)

|    Hits | Line | Source |
| ------: | ---: | :--- |
|       - |    1 | `/**` |
|       - |    2 | ` * SPDX-FileCopyrightText: 2011, 2012, 2013, 2014 Symisc Systems <licensing@symisc.net>` |
|       - |    3 | ` * SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>` |
|       - |    4 | ` * SPDX-License-Identifier: BSD-3-Clause` |
|       - |    5 | ` */` |
|       - |    6 | `#include "ph7int.h"` |
|       - |    7 | `/* range() formats the float variant of its max-array-size ValueError with libc` |
|       - |    8 | ` * snprintf and parses numeric strings with libc strtod — the byte-exact-floats` |
|       - |    9 | ` * rule (see builtin_math.c): SyBufferFormat/SyStrToReal are not correctly` |
|       - |   10 | ` * rounded at extreme magnitudes. */` |
|       - |   11 | `#include <stdio.h>  /* snprintf */` |
|       - |   12 | `#include <stdlib.h> /* strtod */` |
|       - |   13 | `/* This file implement generic hashmaps known as 'array' in the PHP world */` |
|       - |   14 | `/* HASHMAP_INT_NODE / HASHMAP_BLOB_NODE (node key types) are declared in ph7int.h` |
|       - |   15 | ` * alongside ph7_hashmap_node so name-forwarding builtins can classify keys. */` |
|       - |   16 | `/* Node control flags */` |
|       - |   17 | `#define HASHMAP_NODE_FOREIGN_OBJ 0x001 /* Node hold a reference to a foreign ph7_value` |
|       - |   18 | `                                        * [i.e: array(&var)/$a[] =& $var ]` |
|       - |   19 | `										*/` |
|       - |   20 | `/*` |
|       - |   21 | ` * Default hash function for int [i.e; 64-bit integer] keys.` |
|       - |   22 | ` */` |
| 3135494 |   23 | `static sxu32 IntHash(sxi64 iKey)` |
|       5 |   24 | `{` |
| 3135499 |   25 | `	sxu64 uKey = (sxu64)iKey; /* unsigned mixing: shifting a negative key is UB */` |
| 3135499 |   26 | `	return (sxu32)(uKey ^ (uKey << 8) ^ (uKey >> 8));` |
|       5 |   27 | `}` |
|       - |   28 | `/*` |
|       - |   29 | ` * Default hash function for string/BLOB keys.` |
|       - |   30 | ` */` |
|  408004 |   31 | `static sxu32 BinHash(const void *pSrc,sxu32 nLen)` |
|       5 |   32 | `{` |
|  408009 |   33 | `	register unsigned char *zIn = (unsigned char *)pSrc;` |
|       - |   34 | `	unsigned char *zEnd;` |
|  408009 |   35 | `	sxu32 nH = 5381;` |
|  408009 |   36 | `	zEnd = &zIn[nLen];` |
|  477416 |   37 | `	for(;;){` |
|  954837 |   38 | `		if( zIn >= zEnd ){ break; } nH = nH * 33 + zIn[0] ; zIn++;` |
|  826723 |   39 | `		if( zIn >= zEnd ){ break; } nH = nH * 33 + zIn[0] ; zIn++;` |
|  748389 |   40 | `		if( zIn >= zEnd ){ break; } nH = nH * 33 + zIn[0] ; zIn++;` |
|  639569 |   41 | `		if( zIn >= zEnd ){ break; } nH = nH * 33 + zIn[0] ; zIn++;` |
|       5 |   42 | `	}` |
|  408009 |   43 | `	return nH;` |
|       5 |   44 | `}` |
|       - |   45 | `/*` |
|       - |   46 | ` * Return the total number of entries in a given hashmap.` |
|       - |   47 | ` * If bRecursive is set to TRUE then recurse on hashmap entries.` |
|       - |   48 | ` * Self-referential arrays are detected via the HASHMAP_COUNTING flag;` |
|       - |   49 | ` * when a cycle is found the nested array is skipped and *pCycleDetected` |
|       - |   50 | ` * is set to TRUE so the caller can emit a warning.` |
|       - |   51 | ` */` |
|     950 |   52 | `static sxi64 HashmapCount(ph7_hashmap *pMap,int bRecursive,int *pCycleDetected)` |
|       5 |   53 | `{` |
|     955 |   54 | `	sxi64 iCount = 0;` |
|     955 |   55 | `	if( !bRecursive ){` |
|     781 |   56 | `		iCount = pMap->nEntry;` |
|     393 |   57 | `	}else{` |
|       - |   58 | `		/* Recursive hashmap walk */` |
|     175 |   59 | `		ph7_hashmap_node *pEntry = pMap->pLast;` |
|       - |   60 | `		ph7_value *pElem;` |
|     175 |   61 | `		sxu32 n = 0;` |
|       - |   62 | `		/* Mark this map as being counted */` |
|     175 |   63 | `		pMap->iFlags \|= HASHMAP_COUNTING;` |
|     209 |   64 | `		for(;;){` |
|     419 |   65 | `			if( n >= pMap->nEntry ){` |
|     175 |   66 | `				break;` |
|       - |   67 | `			}` |
|       - |   68 | `			/* Point to the element value */` |
|     245 |   69 | `			pElem = (ph7_value *)SySetAt(&pMap->pVm->aMemObj,pEntry->nValIdx);` |
|     245 |   70 | `			if( pElem ){` |
|     245 |   71 | `				if( pElem->iFlags & MEMOBJ_HASHMAP ){` |
|     151 |   72 | `					ph7_hashmap *pSub = (ph7_hashmap *)pElem->x.pOther;` |
|     151 |   73 | `					if( pSub->iFlags & HASHMAP_COUNTING ){` |
|       - |   74 | `						/* Cycle detected — skip this entry */` |
|       3 |   75 | `						if( pCycleDetected ){` |
|       3 |   76 | `							*pCycleDetected = TRUE;` |
|       1 |   77 | `						}` |
|       2 |   78 | `					}else{` |
|     149 |   79 | `						iCount += HashmapCount(pSub,TRUE,pCycleDetected);` |
|       - |   80 | `					}` |
|      75 |   81 | `				}` |
|     122 |   82 | `			}` |
|       - |   83 | `			/* Point to the next entry */` |
|     245 |   84 | `			pEntry = pEntry->pNext;` |
|     245 |   85 | `			++n;` |
|       1 |   86 | `		}` |
|       - |   87 | `		/* Clear the counting flag */` |
|     175 |   88 | `		pMap->iFlags &= ~HASHMAP_COUNTING;` |
|       - |   89 | `		/* Update count */` |
|     175 |   90 | `		iCount += pMap->nEntry;` |
|       - |   91 | `	}` |
|     955 |   92 | `	return iCount;` |
|       5 |   93 | `}` |
|       - |   94 | `/*` |
|       - |   95 | ` * Allocate a new hashmap node with a 64-bit integer key.` |
|       - |   96 | ` * If something goes wrong [i.e: out of memory],this function return NULL.` |
|       - |   97 | ` * Otherwise a fresh [ph7_hashmap_node] instance is returned.` |
|       - |   98 | ` */` |
| 3074188 |   99 | `static ph7_hashmap_node * HashmapNewIntNode(ph7_hashmap *pMap,sxi64 iKey,sxu32 nHash,sxu32 nValIdx)` |
|       5 |  100 | `{` |
|       - |  101 | `	ph7_hashmap_node *pNode;` |
|       - |  102 | `	/* Allocate a new node */` |
| 3074193 |  103 | `	pNode = (ph7_hashmap_node *)SyMemBackendPoolAlloc(&pMap->pVm->sAllocator,sizeof(ph7_hashmap_node));` |
| 3074193 |  104 | `	if( pNode == 0 ){` |
|     ! 0 |  105 | `		return 0;` |
|       - |  106 | `	}` |
|       - |  107 | `	/* Zero the stucture */` |
| 3074193 |  108 | `	SyZero(pNode,sizeof(ph7_hashmap_node));` |
|       - |  109 | `	/* Fill in the structure */` |
| 3074193 |  110 | `	pNode->pMap  = &(*pMap);` |
| 3074193 |  111 | `	pNode->iType = HASHMAP_INT_NODE;` |
| 3074193 |  112 | `	pNode->nHash = nHash;` |
| 3074193 |  113 | `	pNode->xKey.iKey = iKey;` |
| 3074193 |  114 | `	pNode->nValIdx  = nValIdx;` |
| 3074193 |  115 | `	return pNode;` |
| 1537099 |  116 | `}` |
|       - |  117 | `/*` |
|       - |  118 | ` * Allocate a new hashmap node with a BLOB key.` |
|       - |  119 | ` * If something goes wrong [i.e: out of memory],this function return NULL.` |
|       - |  120 | ` * Otherwise a fresh [ph7_hashmap_node] instance is returned.` |
|       - |  121 | ` */` |
|  153972 |  122 | `static ph7_hashmap_node * HashmapNewBlobNode(ph7_hashmap *pMap,const void *pKey,sxu32 nKeyLen,sxu32 nHash,sxu32 nValIdx)` |
|       5 |  123 | `{` |
|       - |  124 | `	ph7_hashmap_node *pNode;` |
|       - |  125 | `	/* Allocate a new node */` |
|  153977 |  126 | `	pNode = (ph7_hashmap_node *)SyMemBackendPoolAlloc(&pMap->pVm->sAllocator,sizeof(ph7_hashmap_node));` |
|  153977 |  127 | `	if( pNode == 0 ){` |
|     ! 0 |  128 | `		return 0;` |
|       - |  129 | `	}` |
|       - |  130 | `	/* Zero the stucture */` |
|  153977 |  131 | `	SyZero(pNode,sizeof(ph7_hashmap_node));` |
|       - |  132 | `	/* Fill in the structure */` |
|  153977 |  133 | `	pNode->pMap  = &(*pMap);` |
|  153977 |  134 | `	pNode->iType = HASHMAP_BLOB_NODE;` |
|  153977 |  135 | `	pNode->nHash = nHash;` |
|  153977 |  136 | `	SyBlobInit(&pNode->xKey.sKey,&pMap->pVm->sAllocator);` |
|  153977 |  137 | `	SyBlobAppend(&pNode->xKey.sKey,pKey,nKeyLen);` |
|  153977 |  138 | `	pNode->nValIdx = nValIdx;` |
|  153977 |  139 | `	return pNode;` |
|   76991 |  140 | `}` |
|       - |  141 | `/*` |
|       - |  142 | ` * link a hashmap node to the given bucket index (last argument to this function).` |
|       - |  143 | ` */` |
| 3228160 |  144 | `static void HashmapNodeLink(ph7_hashmap *pMap,ph7_hashmap_node *pNode,sxu32 nBucketIdx)` |
|       5 |  145 | `{` |
|       - |  146 | `	/* Link */` |
| 3228165 |  147 | `	if( pMap->apBucket[nBucketIdx] != 0 ){` |
| 2850515 |  148 | `		pNode->pNextCollide = pMap->apBucket[nBucketIdx];` |
| 2850515 |  149 | `		pMap->apBucket[nBucketIdx]->pPrevCollide = pNode;` |
| 1425255 |  150 | `	}` |
| 3228165 |  151 | `	pMap->apBucket[nBucketIdx] = pNode;` |
|       - |  152 | `	/* Link to the map list */` |
| 3228165 |  153 | `	if( pMap->pFirst == 0 ){` |
|   74327 |  154 | `		pMap->pFirst = pMap->pLast = pNode;` |
|       - |  155 | `		/* Point to the first inserted node */` |
|   74327 |  156 | `		pMap->pCur = pNode;` |
|   37166 |  157 | `	}else{` |
| 3153843 |  158 | `		MACRO_LD_PUSH(pMap->pLast,pNode);` |
|       - |  159 | `	}` |
| 3228165 |  160 | `	++pMap->nEntry;` |
| 3228165 |  161 | `}` |
|       - |  162 | `/*` |
|       - |  163 | ` * Unlink a node from the hashmap.` |
|       - |  164 | ` * If the node count reaches zero then release the whole hash-bucket.` |
|       - |  165 | ` */` |
|    7434 |  166 | `PH7_PRIVATE void PH7_HashmapUnlinkNode(ph7_hashmap_node *pNode,int bRestore)` |
|       5 |  167 | `{` |
|    7439 |  168 | `	ph7_hashmap *pMap = pNode->pMap;` |
|    7439 |  169 | `	ph7_vm *pVm = pMap->pVm;` |
|       - |  170 | `	/* Unlink from the corresponding bucket */` |
|    7439 |  171 | `	if( pNode->pPrevCollide == 0 ){` |
|    6975 |  172 | `		pMap->apBucket[pNode->nHash & (pMap->nSize - 1)] = pNode->pNextCollide;` |
|    3490 |  173 | `	}else{` |
|     466 |  174 | `		pNode->pPrevCollide->pNextCollide = pNode->pNextCollide;` |
|       - |  175 | `	}` |
|    7439 |  176 | `	if( pNode->pNextCollide ){` |
|    4359 |  177 | `		pNode->pNextCollide->pPrevCollide = pNode->pPrevCollide;` |
|    2178 |  178 | `	}` |
|    7439 |  179 | `	if( pMap->pFirst == pNode ){` |
|     131 |  180 | `		pMap->pFirst = pNode->pPrev;` |
|      63 |  181 | `	}` |
|    7439 |  182 | `	if( pMap->pCur == pNode ){` |
|       - |  183 | `		/* Advance the node cursor */` |
|     133 |  184 | `		pMap->pCur = pMap->pCur->pPrev; /* Reverse link */` |
|      64 |  185 | `	}` |
|       - |  186 | `	/* Unlink from the map list */` |
|    7439 |  187 | `	MACRO_LD_REMOVE(pMap->pLast,pNode);` |
|    7439 |  188 | `	if( bRestore ){` |
|       - |  189 | `		/* Remove the ph7_value associated with this node from the reference table */` |
|     107 |  190 | `		PH7_VmRefObjRemove(pVm,pNode->nValIdx,0,pNode);` |
|       - |  191 | `		/* Restore to the freelist */` |
|     107 |  192 | `		if( (pNode->iFlags & HASHMAP_NODE_FOREIGN_OBJ) == 0 ){` |
|     107 |  193 | `			PH7_VmUnsetMemObj(pVm,pNode->nValIdx,FALSE);` |
|      51 |  194 | `		}` |
|      51 |  195 | `	}` |
|    7439 |  196 | `	if( pNode->iType == HASHMAP_BLOB_NODE ){` |
|    7302 |  197 | `		SyBlobRelease(&pNode->xKey.sKey);` |
|    3649 |  198 | `	}` |
|    7439 |  199 | `	SyMemBackendPoolFree(&pVm->sAllocator,pNode);` |
|    7439 |  200 | `	pMap->nEntry--;` |
|    7439 |  201 | `	if( pMap->nEntry < 1 && pMap != pVm->pGlobal ){` |
|       - |  202 | `		/* Free the hash-bucket */` |
|      75 |  203 | `		SyMemBackendFree(&pVm->sAllocator,pMap->apBucket);` |
|      75 |  204 | `		pMap->apBucket = 0;` |
|      75 |  205 | `		pMap->nSize = 0;` |
|      75 |  206 | `		pMap->pFirst = pMap->pLast = pMap->pCur = 0;` |
|      35 |  207 | `	}` |
|    7439 |  208 | `}` |
|       - |  209 | `#define HASHMAP_FILL_FACTOR 3` |
|       - |  210 | `/*` |
|       - |  211 | ` * Grow the hash-table and rehash all entries.` |
|       - |  212 | ` */` |
| 3228160 |  213 | `static sxi32 HashmapGrowBucket(ph7_hashmap *pMap)` |
|       5 |  214 | `{` |
| 3228165 |  215 | `	if( pMap->nEntry >= pMap->nSize * HASHMAP_FILL_FACTOR ){` |
|   79023 |  216 | `		ph7_hashmap_node **apOld = pMap->apBucket;` |
|       - |  217 | `		ph7_hashmap_node *pEntry,**apNew;` |
|   79023 |  218 | `		sxu32 nNew = pMap->nSize << 1;` |
|       - |  219 | `		sxu32 nBucket;` |
|       - |  220 | `		sxu32 n;` |
|   79023 |  221 | `		if( nNew < 1 ){` |
|   74327 |  222 | `			nNew = 16;` |
|   37161 |  223 | `		}` |
|       - |  224 | `		/* Allocate a new bucket */` |
|   79023 |  225 | `		apNew = (ph7_hashmap_node **)SyMemBackendAlloc(&pMap->pVm->sAllocator,nNew * sizeof(ph7_hashmap_node *));` |
|   79023 |  226 | `		if( apNew == 0 ){` |
|     ! 0 |  227 | `			if( pMap->nSize < 1 ){` |
|     ! 0 |  228 | `				return SXERR_MEM; /* Fatal */` |
|       - |  229 | `			}` |
|       - |  230 | `			/* Not so fatal here,simply a performance hit */` |
|     ! 0 |  231 | `			return SXRET_OK;` |
|       - |  232 | `		}` |
|       - |  233 | `		/* Zero the table */` |
|   79023 |  234 | `		SyZero((void *)apNew,nNew * sizeof(ph7_hashmap_node *));` |
|       - |  235 | `		/* Reflect the change */` |
|   79023 |  236 | `		pMap->apBucket = apNew;` |
|   79023 |  237 | `		pMap->nSize = nNew;` |
|   79023 |  238 | `		if( apOld == 0 ){` |
|       - |  239 | `			/* First allocated table [i.e: no entry],return immediately */` |
|   74327 |  240 | `			return SXRET_OK;` |
|       - |  241 | `		}` |
|       - |  242 | `		/* Rehash old entries */` |
|    4701 |  243 | `		pEntry = pMap->pFirst;` |
|    4701 |  244 | `		n = 0;` |
| 2077916 |  245 | `		for( ;; ){` |
| 4155837 |  246 | `			if( n >= pMap->nEntry ){` |
|    4701 |  247 | `				break;` |
|       - |  248 | `			}` |
|       - |  249 | `			/* Clear the old collision link */` |
| 4151141 |  250 | `			pEntry->pNextCollide = pEntry->pPrevCollide = 0;` |
|       - |  251 | `			/* Link to the new bucket */` |
| 4151141 |  252 | `			nBucket = pEntry->nHash & (nNew - 1);` |
| 4151141 |  253 | `			if( pMap->apBucket[nBucket] != 0 ){` |
| 3562437 |  254 | `				pEntry->pNextCollide = pMap->apBucket[nBucket];` |
| 3562437 |  255 | `				pMap->apBucket[nBucket]->pPrevCollide = pEntry;` |
| 1781216 |  256 | `			}` |
| 4151141 |  257 | `			pMap->apBucket[nBucket] = pEntry;` |
|       - |  258 | `			/* Point to the next entry */` |
| 4151141 |  259 | `			pEntry = pEntry->pPrev; /* Reverse link */` |
| 4151141 |  260 | `			n++;` |
|       5 |  261 | `		}` |
|       - |  262 | `		/* Free the old table */` |
|    4701 |  263 | `		SyMemBackendFree(&pMap->pVm->sAllocator,(void *)apOld);` |
|    2348 |  264 | `	}` |
| 3153843 |  265 | `	return SXRET_OK;` |
| 1614085 |  266 | `}` |
|       - |  267 | `/*` |
|       - |  268 | ` * Insert a 64-bit integer key and it's associated value (if any) in the given` |
|       - |  269 | ` * hashmap.` |
|       - |  270 | ` */` |
| 3074188 |  271 | `static sxi32 HashmapInsertIntKey(ph7_hashmap *pMap,sxi64 iKey,ph7_value *pValue,sxu32 nRefIdx,int isForeign)` |
|       5 |  272 | `{` |
|       - |  273 | `	ph7_hashmap_node *pNode;` |
|       - |  274 | `	sxu32 nIdx;` |
|       - |  275 | `	sxu32 nHash;` |
|       - |  276 | `	sxi32 rc;` |
| 3074193 |  277 | `	if( !isForeign ){` |
|       - |  278 | `		ph7_value *pObj;` |
|       - |  279 | `		ph7_value sSafeVal;` |
|       - |  280 | `		/* Snapshot the source BEFORE reserving: PH7_ReserveMemObj can grow (move)` |
|       - |  281 | `		 * pVm->aMemObj, which would dangle pValue when it points into the pool` |
|       - |  282 | `		 * (e.g. get_defined_vars/func_get_args/get_class_vars/get_object_vars pass` |
|       - |  283 | `		 * a pool slot). A shallow copy is a safe PH7_MemObjStore source — the` |
|       - |  284 | `		 * referent and the heap-resident blob data survive the move; only the` |
|       - |  285 | `		 * ph7_value struct relocates (same sSafeVal idiom used by PH7_HashmapDup). */` |
| 3074155 |  286 | `		if( pValue ){` |
| 3074153 |  287 | `			sSafeVal = *pValue;` |
| 3074153 |  288 | `			pValue = &sSafeVal;` |
| 1537074 |  289 | `		}` |
|       - |  290 | `		/* Reserve a ph7_value for the value */` |
| 3074155 |  291 | `		pObj = PH7_ReserveMemObj(pMap->pVm);` |
| 3074155 |  292 | `		if( pObj == 0 ){` |
|     ! 0 |  293 | `			return SXERR_MEM;` |
|       - |  294 | `		}` |
| 3074155 |  295 | `		if( pValue ){` |
|       - |  296 | `			/* Duplicate the value */` |
| 3074153 |  297 | `			PH7_MemObjStore(pValue,pObj);` |
| 1537074 |  298 | `		}` |
| 3074155 |  299 | `		nIdx = pObj->nIdx;` |
| 1537080 |  300 | `	}else{` |
|      39 |  301 | `		nIdx = nRefIdx;` |
|       - |  302 | `	}` |
|       - |  303 | `	/* Hash the key */` |
| 3074193 |  304 | `	nHash = pMap->xIntHash(iKey);` |
|       - |  305 | `	/* Allocate a new int node */` |
| 3074193 |  306 | `	pNode = HashmapNewIntNode(&(*pMap),iKey,nHash,nIdx);` |
| 3074193 |  307 | `	if( pNode == 0 ){` |
|     ! 0 |  308 | `		return SXERR_MEM;` |
|       - |  309 | `	}` |
| 3074193 |  310 | `	if( isForeign ){` |
|       - |  311 | `		/* Mark as a foregin entry */` |
|      39 |  312 | `		pNode->iFlags \|= HASHMAP_NODE_FOREIGN_OBJ;` |
|      19 |  313 | `	}` |
|       - |  314 | `	/* Make sure the bucket is big enough to hold the new entry */` |
| 3074193 |  315 | `	rc = HashmapGrowBucket(&(*pMap));` |
| 3074193 |  316 | `	if( rc != SXRET_OK ){` |
|     ! 0 |  317 | `		SyMemBackendPoolFree(&pMap->pVm->sAllocator,pNode);` |
|     ! 0 |  318 | `		return rc;` |
|       - |  319 | `	}` |
|       - |  320 | `	/* Perform the insertion */` |
| 3074193 |  321 | `	HashmapNodeLink(&(*pMap),pNode,nHash & (pMap->nSize - 1));` |
|       - |  322 | `	/* Install in the reference table */` |
| 3074193 |  323 | `	PH7_VmRefObjInstall(pMap->pVm,nIdx,0,pNode,0);` |
|       - |  324 | `	/* All done */` |
| 3074193 |  325 | `	return SXRET_OK;` |
| 1537099 |  326 | `}` |
|       - |  327 | `/*` |
|       - |  328 | ` * Insert a BLOB key and it's associated value (if any) in the given` |
|       - |  329 | ` * hashmap.` |
|       - |  330 | ` */` |
|  153972 |  331 | `static sxi32 HashmapInsertBlobKey(ph7_hashmap *pMap,const void *pKey,sxu32 nKeyLen,ph7_value *pValue,sxu32 nRefIdx,int isForeign)` |
|       5 |  332 | `{` |
|       - |  333 | `	ph7_hashmap_node *pNode;` |
|       - |  334 | `	sxu32 nHash;` |
|       - |  335 | `	sxu32 nIdx;` |
|       - |  336 | `	sxi32 rc;` |
|  153977 |  337 | `	if( !isForeign ){` |
|       - |  338 | `		ph7_value *pObj;` |
|       - |  339 | `		ph7_value sSafeVal;` |
|       - |  340 | `		/* Snapshot the source BEFORE reserving: PH7_ReserveMemObj can grow (move)` |
|       - |  341 | `		 * pVm->aMemObj, which would dangle pValue when it points into the pool` |
|       - |  342 | `		 * (e.g. get_defined_vars/func_get_args/get_class_vars/get_object_vars pass` |
|       - |  343 | `		 * a pool slot). A shallow copy is a safe PH7_MemObjStore source — the` |
|       - |  344 | `		 * referent and the heap-resident blob data survive the move; only the` |
|       - |  345 | `		 * ph7_value struct relocates (same sSafeVal idiom used by PH7_HashmapDup). */` |
|  107677 |  346 | `		if( pValue ){` |
|  107387 |  347 | `			sSafeVal = *pValue;` |
|  107387 |  348 | `			pValue = &sSafeVal;` |
|   53691 |  349 | `		}` |
|       - |  350 | `		/* Reserve a ph7_value for the value */` |
|  107677 |  351 | `		pObj = PH7_ReserveMemObj(pMap->pVm);` |
|  107677 |  352 | `		if( pObj == 0 ){` |
|     ! 0 |  353 | `			return SXERR_MEM;` |
|       - |  354 | `		}` |
|  107677 |  355 | `		if( pValue ){` |
|       - |  356 | `			/* Duplicate the value */` |
|  107387 |  357 | `			PH7_MemObjStore(pValue,pObj);` |
|   53691 |  358 | `		}` |
|  107677 |  359 | `		nIdx = pObj->nIdx;` |
|   53841 |  360 | `	}else{` |
|   46305 |  361 | `		nIdx = nRefIdx;` |
|       - |  362 | `	}` |
|       - |  363 | `	/* Hash the key */` |
|  153977 |  364 | `	nHash = pMap->xBlobHash(pKey,nKeyLen);` |
|       - |  365 | `	/* Allocate a new blob node */` |
|  153977 |  366 | `	pNode = HashmapNewBlobNode(&(*pMap),pKey,nKeyLen,nHash,nIdx);` |
|  153977 |  367 | `	if( pNode == 0 ){` |
|     ! 0 |  368 | `		return SXERR_MEM;` |
|       - |  369 | `	}` |
|  153977 |  370 | `	if( isForeign ){` |
|       - |  371 | `		/* Mark as a foregin entry */` |
|   46305 |  372 | `		pNode->iFlags \|= HASHMAP_NODE_FOREIGN_OBJ;` |
|   23150 |  373 | `	}` |
|       - |  374 | `	/* Make sure the bucket is big enough to hold the new entry */` |
|  153977 |  375 | `	rc = HashmapGrowBucket(&(*pMap));` |
|  153977 |  376 | `	if( rc != SXRET_OK ){` |
|     ! 0 |  377 | `		SyMemBackendPoolFree(&pMap->pVm->sAllocator,pNode);` |
|     ! 0 |  378 | `		return rc;` |
|       - |  379 | `	}` |
|       - |  380 | `	/* Perform the insertion */` |
|  153977 |  381 | `	HashmapNodeLink(&(*pMap),pNode,nHash & (pMap->nSize - 1));` |
|       - |  382 | `	/* Install in the reference table */` |
|  153977 |  383 | `	PH7_VmRefObjInstall(pMap->pVm,nIdx,0,pNode,0);` |
|       - |  384 | `	/* All done */` |
|  153977 |  385 | `	return SXRET_OK;` |
|   76991 |  386 | `}` |
|       - |  387 | `/*` |
|       - |  388 | ` * Check if a given 64-bit integer key exists in the given hashmap.` |
|       - |  389 | ` * Write a pointer to the target node on success. Otherwise` |
|       - |  390 | ` * SXERR_NOTFOUND is returned on failure.` |
|       - |  391 | ` */` |
|   48670 |  392 | `static sxi32 HashmapLookupIntKey(` |
|       - |  393 | `	ph7_hashmap *pMap,         /* Target hashmap */` |
|       - |  394 | `	sxi64 iKey,                /* lookup key */` |
|       - |  395 | `	ph7_hashmap_node **ppNode  /* OUT: target node on success */` |
|       - |  396 | `	)` |
|       5 |  397 | `{` |
|       - |  398 | `	ph7_hashmap_node *pNode;` |
|       - |  399 | `	sxu32 nHash;` |
|   48675 |  400 | `	if( pMap->nEntry < 1 ){` |
|       - |  401 | `		/* Don't bother hashing,there is no entry anyway */` |
|     561 |  402 | `		return SXERR_NOTFOUND;` |
|       - |  403 | `	}` |
|       - |  404 | `	/* Hash the key first */` |
|   48119 |  405 | `	nHash = pMap->xIntHash(iKey);` |
|       - |  406 | `	/* Point to the appropriate bucket */` |
|   48119 |  407 | `	pNode = pMap->apBucket[nHash & (pMap->nSize - 1)];` |
|       - |  408 | `	/* Perform the lookup */` |
|  412423 |  409 | `	for(;;){` |
|  824851 |  410 | `		if( pNode == 0 ){` |
|   46319 |  411 | `			break;` |
|       - |  412 | `		}` |
|  778532 |  413 | `		if( pNode->iType == HASHMAP_INT_NODE` |
|  775522 |  414 | `			&& pNode->nHash == nHash` |
|  387161 |  415 | `			&& pNode->xKey.iKey == iKey ){` |
|       - |  416 | `				/* Node found */` |
|    1805 |  417 | `				if( ppNode ){` |
|    1787 |  418 | `					*ppNode = pNode;` |
|     891 |  419 | `				}` |
|    1805 |  420 | `				return SXRET_OK;` |
|       - |  421 | `		}` |
|       - |  422 | `		/* Follow the collision link */` |
|  776733 |  423 | `		pNode = pNode->pNextCollide;` |
|       1 |  424 | `	}` |
|       - |  425 | `	/* No such entry */` |
|   46319 |  426 | `	return SXERR_NOTFOUND;` |
|   24340 |  427 | `}` |
|       - |  428 | `/*` |
|       - |  429 | ` * Check if a given BLOB key exists in the given hashmap.` |
|       - |  430 | ` * Write a pointer to the target node on success. Otherwise` |
|       - |  431 | ` * SXERR_NOTFOUND is returned on failure.` |
|       - |  432 | ` */` |
|  278344 |  433 | `static sxi32 HashmapLookupBlobKey(` |
|       - |  434 | `	ph7_hashmap *pMap,          /* Target hashmap */` |
|       - |  435 | `	const void *pKey,           /* Lookup key */` |
|       - |  436 | `	sxu32 nKeyLen,              /* Key length in bytes */` |
|       - |  437 | `	ph7_hashmap_node **ppNode   /* OUT: target node on success */` |
|       - |  438 | `	)` |
|       5 |  439 | `{` |
|       - |  440 | `	ph7_hashmap_node *pNode;` |
|       - |  441 | `	sxu32 nHash;` |
|  278349 |  442 | `	if( pMap->nEntry < 1 ){` |
|       - |  443 | `		/* Don't bother hashing,there is no entry anyway */` |
|   24317 |  444 | `		return SXERR_NOTFOUND;` |
|       - |  445 | `	}` |
|       - |  446 | `	/* Hash the key first */` |
|  254037 |  447 | `	nHash = pMap->xBlobHash(pKey,nKeyLen);` |
|       - |  448 | `	/* Point to the appropriate bucket */` |
|  254037 |  449 | `	pNode = pMap->apBucket[nHash & (pMap->nSize - 1)];` |
|       - |  450 | `	/* Perform the lookup */` |
|  215419 |  451 | `	for(;;){` |
|  430843 |  452 | `		if( pNode == 0 ){` |
|  202097 |  453 | `			break;` |
|       - |  454 | `		}` |
|  228746 |  455 | `		if( pNode->iType == HASHMAP_BLOB_NODE` |
|  227241 |  456 | `			&& pNode->nHash == nHash` |
|  138884 |  457 | `			&& SyBlobLength(&pNode->xKey.sKey) == nKeyLen` |
|   52037 |  458 | `			&& SyMemcmp(SyBlobData(&pNode->xKey.sKey),pKey,nKeyLen) == 0 ){` |
|       - |  459 | `				/* Node found */` |
|   51945 |  460 | `				if( ppNode ){` |
|   51917 |  461 | `					*ppNode = pNode;` |
|   25956 |  462 | `				}` |
|   51945 |  463 | `				return SXRET_OK;` |
|       - |  464 | `		}` |
|       - |  465 | `		/* Follow the collision link */` |
|  176811 |  466 | `		pNode = pNode->pNextCollide;` |
|       5 |  467 | `	}` |
|       - |  468 | `	/* No such entry */` |
|  202097 |  469 | `	return SXERR_NOTFOUND;` |
|  139177 |  470 | `}` |
|       - |  471 | `/*` |
|       - |  472 | ` * Check if the given BLOB key looks like a decimal number.` |
|       - |  473 | ` * Retrurn TRUE on success.FALSE otherwise.` |
|       - |  474 | ` */` |
|  278474 |  475 | `static int HashmapIsIntKey(SyBlob *pKey)` |
|       5 |  476 | `{` |
|  278479 |  477 | `	const char *zIn  = (const char *)SyBlobData(pKey);` |
|  278479 |  478 | `	const char *zEnd = &zIn[SyBlobLength(pKey)];` |
|  278479 |  479 | `	if( (int)(zEnd-zIn) > 1 && zIn[0] == '0' ){` |
|       - |  480 | `		/* Octal not decimal number */` |
|       5 |  481 | `		return FALSE;` |
|       - |  482 | `	}` |
|  278475 |  483 | `	if( (zIn[0] == '-' \|\| zIn[0] == '+') && &zIn[1] < zEnd ){` |
|     ! 0 |  484 | `		zIn++;` |
|     ! 0 |  485 | `	}` |
|  139571 |  486 | `	for(;;){` |
|  279147 |  487 | `		if( zIn >= zEnd ){` |
|     239 |  488 | `			return TRUE;` |
|       - |  489 | `		}` |
|  278909 |  490 | `		if( (unsigned char)zIn[0] >= 0xc0 /* UTF-8 stream */  \|\| !SyisDigit(zIn[0]) ){` |
|  139121 |  491 | `			break;` |
|       - |  492 | `		}` |
|     673 |  493 | `		zIn++;` |
|       1 |  494 | `	}` |
|       - |  495 | `	/* Key does not look like a decimal number */` |
|  278237 |  496 | `	return FALSE;` |
|  139242 |  497 | `}` |
|       - |  498 | `/*` |
|       - |  499 | ` * Check if a given key exists in the given hashmap.` |
|       - |  500 | ` * Write a pointer to the target node on success.` |
|       - |  501 | ` * Otherwise SXERR_NOTFOUND is returned on failure.` |
|       - |  502 | ` */` |
|  126448 |  503 | `static sxi32 HashmapLookup(` |
|       - |  504 | `	ph7_hashmap *pMap,          /* Target hashmap */` |
|       - |  505 | `	ph7_value *pKey,            /* Lookup key */` |
|       - |  506 | `	ph7_hashmap_node **ppNode   /* OUT: target node on success */` |
|       - |  507 | `	)` |
|       5 |  508 | `{` |
|  126453 |  509 | `	ph7_hashmap_node *pNode = 0; /* cc -O6 warning */` |
|       - |  510 | `	sxi32 rc;` |
|  126453 |  511 | `	if( pKey->iFlags & (MEMOBJ_STRING\|MEMOBJ_HASHMAP\|MEMOBJ_OBJ\|MEMOBJ_RES) ){` |
|  124881 |  512 | `		if( (pKey->iFlags & MEMOBJ_STRING) == 0 ){` |
|       - |  513 | `			/* Force a string cast */` |
|     ! 0 |  514 | `			PH7_MemObjToString(&(*pKey));` |
|     ! 0 |  515 | `		}` |
|  124881 |  516 | `		if( SyBlobLength(&pKey->sBlob) > 0 && !HashmapIsIntKey(&pKey->sBlob) ){` |
|       - |  517 | `			/* Perform a blob lookup */` |
|  124861 |  518 | `			rc = HashmapLookupBlobKey(&(*pMap),SyBlobData(&pKey->sBlob),SyBlobLength(&pKey->sBlob),&pNode);` |
|  124861 |  519 | `			goto result;` |
|       - |  520 | `		}` |
|      10 |  521 | `	}` |
|       - |  522 | `	/* Perform an int lookup */` |
|    1597 |  523 | `	if((pKey->iFlags & MEMOBJ_INT) == 0 ){` |
|       - |  524 | `		/* Force an integer cast */` |
|      31 |  525 | `		PH7_MemObjToInteger(pKey);` |
|      15 |  526 | `	}` |
|       - |  527 | `	/* Perform an int lookup */` |
|    1597 |  528 | `	rc = HashmapLookupIntKey(&(*pMap),pKey->x.iVal,&pNode);` |
|   63224 |  529 | `result:` |
|  126453 |  530 | `	if( rc == SXRET_OK ){` |
|       - |  531 | `		/* Node found */` |
|   53385 |  532 | `		if( ppNode ){` |
|   53339 |  533 | `			*ppNode = pNode;` |
|   26667 |  534 | `		}` |
|   53385 |  535 | `		return SXRET_OK;` |
|       - |  536 | `	}` |
|       - |  537 | `	/* No such entry */` |
|   73073 |  538 | `	return SXERR_NOTFOUND;` |
|   63229 |  539 | `}` |
|       - |  540 | `/*` |
|       - |  541 | ` * Advance the auto-index after a successful insertion of int key iKey.` |
|       - |  542 | ` * Mirrors Zend's nNextFreeElement: saturates at PHP_INT_MAX (incrementing` |
|       - |  543 | ` * past it is signed overflow); the occupied-slot case errors at append time` |
|       - |  544 | ` * via HashmapAppendIndexBusy.` |
|       - |  545 | ` */` |
|   23542 |  546 | `static void HashmapAdvanceAutoIndex(ph7_hashmap *pMap,sxi64 iKey)` |
|       5 |  547 | `{` |
|   23547 |  548 | `	if( iKey >= pMap->iNextIdx ){` |
|   23303 |  549 | `		pMap->iNextIdx = iKey < SXI64_HIGH ? iKey + 1 : SXI64_HIGH;` |
|       - |  550 | `		/* Make sure the automatic index is not reserved */` |
|   23303 |  551 | `		while( pMap->iNextIdx < SXI64_HIGH && SXRET_OK == HashmapLookupIntKey(&(*pMap),pMap->iNextIdx,0) ){` |
|     ! 0 |  552 | `			pMap->iNextIdx++;` |
|     ! 0 |  553 | `		}` |
|   11649 |  554 | `	}` |
|   23547 |  555 | `}` |
|       - |  556 | `/*` |
|       - |  557 | `` * TRUE when an append (`$a[] = v`) cannot proceed because the saturated`` |
|       - |  558 | ` * auto-index slot (PHP_INT_MAX) is already occupied. Throws php's catchable` |
|       - |  559 | ` * Error and stores the rc the insert function must return (PH7_EXCEPTION,` |
|       - |  560 | ` * or PH7_ABORT when the Error class itself cannot be built).` |
|       - |  561 | ` */` |
| 3050306 |  562 | `static sxi32 HashmapAppendIndexBusy(ph7_hashmap *pMap,sxi32 *pRc)` |
|       5 |  563 | `{` |
| 3050311 |  564 | `	if( pMap->iNextIdx == SXI64_HIGH && SXRET_OK == HashmapLookupIntKey(&(*pMap),pMap->iNextIdx,0) ){` |
|       7 |  565 | `		*pRc = PH7_VmThrowArrayNextIndexError(pMap->pVm);` |
|       7 |  566 | `		return TRUE;` |
|       - |  567 | `	}` |
| 3050305 |  568 | `	return FALSE;` |
| 1525158 |  569 | `}` |
|       - |  570 | `/*` |
|       - |  571 | ` * Insert a given key and it's associated value (if any) in the given` |
|       - |  572 | ` * hashmap.` |
|       - |  573 | ` * If a node with the given key already exists in the database` |
|       - |  574 | ` * then this function overwrite the old value.` |
|       - |  575 | ` */` |
| 3180974 |  576 | `static sxi32 HashmapInsert(` |
|       - |  577 | `	ph7_hashmap *pMap, /* Target hashmap */` |
|       - |  578 | `	ph7_value *pKey,   /* Lookup key  */` |
|       - |  579 | `	ph7_value *pVal    /* Node value */` |
|       - |  580 | `	)` |
|       5 |  581 | `{` |
| 3180979 |  582 | `	ph7_hashmap_node *pNode = 0;` |
| 3180979 |  583 | `	sxi32 rc = SXRET_OK;` |
| 3180979 |  584 | `	if( pKey && pKey->iFlags & (MEMOBJ_STRING\|MEMOBJ_HASHMAP\|MEMOBJ_OBJ\|MEMOBJ_RES) ){` |
|  110793 |  585 | `		if( (pKey->iFlags & MEMOBJ_STRING) == 0 ){` |
|       - |  586 | `			/* Force a string cast */` |
|       3 |  587 | `			PH7_MemObjToString(&(*pKey));` |
|       1 |  588 | `		}` |
|  110793 |  589 | `		if( SyBlobLength(&pKey->sBlob) < 1 \|\| HashmapIsIntKey(&pKey->sBlob) ){` |
|    3719 |  590 | `			if(SyBlobLength(&pKey->sBlob) < 1){` |
|       - |  591 | `				/* Automatic index assign */` |
|    3497 |  592 | `				pKey = 0;` |
|    1746 |  593 | `			}` |
|    3719 |  594 | `			goto IntKey;` |
|       - |  595 | `		}` |
|  160616 |  596 | `		if( SXRET_OK == HashmapLookupBlobKey(&(*pMap),SyBlobData(&pKey->sBlob),` |
|   53537 |  597 | `			SyBlobLength(&pKey->sBlob),&pNode) ){` |
|       - |  598 | `				/* Overwrite the old value */` |
|       - |  599 | `				ph7_value *pElem;` |
|      85 |  600 | `				pElem = (ph7_value *)SySetAt(&pMap->pVm->aMemObj,pNode->nValIdx);` |
|      85 |  601 | `				if( pElem ){` |
|      85 |  602 | `					if( pVal ){` |
|      85 |  603 | `						PH7_MemObjStore(pVal,pElem);` |
|      44 |  604 | `					}else{` |
|       - |  605 | `						/* Nullify the entry */` |
|     ! 0 |  606 | `						PH7_MemObjToNull(pElem);` |
|       - |  607 | `					}` |
|      41 |  608 | `				}` |
|      85 |  609 | `				return SXRET_OK;` |
|       - |  610 | `		}` |
|  106997 |  611 | `		if( pMap == pMap->pVm->pGlobal ){` |
|       - |  612 | `			/* php 8.1: writing a new key into $GLOBALS creates a real global` |
|       - |  613 | `			 * variable ($GLOBALS stays a live view of the symbol table). */` |
|     123 |  614 | `			if( SyBlobLength(&pKey->sBlob) < 1 ){` |
|       - |  615 | `				/* Pathological empty name: keep the legacy diagnostic */` |
|     ! 0 |  616 | `				PH7_VmThrowError(pMap->pVm,0,PH7_CTX_NOTICE,"$GLOBALS is a read-only array,insertion is forbidden");` |
|     ! 0 |  617 | `				return SXRET_OK;` |
|       - |  618 | `			}` |
|     184 |  619 | `			return PH7_VmInstallGlobalVar(pMap->pVm,` |
|     122 |  620 | `				(const char *)SyBlobData(&pKey->sBlob),SyBlobLength(&pKey->sBlob),` |
|      61 |  621 | `				pVal,SXU32_HIGH);` |
|       - |  622 | `		}` |
|       - |  623 | `		/* Perform a blob-key insertion */` |
|  106875 |  624 | `		rc = HashmapInsertBlobKey(&(*pMap),SyBlobData(&pKey->sBlob),SyBlobLength(&pKey->sBlob),&(*pVal),0,FALSE);` |
|  106875 |  625 | `		return rc;` |
|       - |  626 | `	}` |
| 1535093 |  627 | `IntKey:` |
| 3073905 |  628 | `	if( pKey ){` |
|   23629 |  629 | `		if((pKey->iFlags & MEMOBJ_INT) == 0 ){` |
|       - |  630 | `			/* Force an integer cast */` |
|     251 |  631 | `			PH7_MemObjToInteger(pKey);` |
|     125 |  632 | `		}` |
|   23629 |  633 | `		if( SXRET_OK == HashmapLookupIntKey(&(*pMap),pKey->x.iVal,&pNode) ){` |
|       - |  634 | `			/* Overwrite the old value */` |
|       - |  635 | `			ph7_value *pElem;` |
|      87 |  636 | `			pElem = (ph7_value *)SySetAt(&pMap->pVm->aMemObj,pNode->nValIdx);` |
|      87 |  637 | `			if( pElem ){` |
|      87 |  638 | `				if( pVal ){` |
|      87 |  639 | `					PH7_MemObjStore(pVal,pElem);` |
|      44 |  640 | `				}else{` |
|       - |  641 | `					/* Nullify the entry */` |
|     ! 0 |  642 | `					PH7_MemObjToNull(pElem);` |
|       - |  643 | `				}` |
|      43 |  644 | `			}` |
|      87 |  645 | `			return SXRET_OK;` |
|       - |  646 | `		}` |
|   23543 |  647 | `		if( pMap == pMap->pVm->pGlobal ){` |
|       - |  648 | `			/* php 8.1: an int key creates the global named by its decimal` |
|       - |  649 | `			 * form ($GLOBALS[7] = ... behaves like $GLOBALS['7'] = ...). */` |
|       - |  650 | `			char zKey[24];` |
|       3 |  651 | `			sxu32 nKey = SyBufferFormat(zKey,sizeof(zKey),"%qd",pKey->x.iVal);` |
|       3 |  652 | `			return PH7_VmInstallGlobalVar(pMap->pVm,zKey,nKey,pVal,SXU32_HIGH);` |
|       - |  653 | `		}` |
|       - |  654 | `		/* Perform a 64-bit-int-key insertion */` |
|   23541 |  655 | `		rc = HashmapInsertIntKey(&(*pMap),pKey->x.iVal,&(*pVal),0,FALSE);` |
|   23541 |  656 | `		if( rc == SXRET_OK ){` |
|   23541 |  657 | `			HashmapAdvanceAutoIndex(&(*pMap),pKey->x.iVal);` |
|   11768 |  658 | `		}` |
|   11773 |  659 | `	}else{` |
| 3050281 |  660 | `		if( pMap == pMap->pVm->pGlobal ){` |
|       - |  661 | `			/* php's catchable Error: Cannot append to $GLOBALS */` |
|       3 |  662 | `			return PH7_VmThrowGlobalsAppendError(pMap->pVm);` |
|       - |  663 | `		}` |
| 3050279 |  664 | `		if( HashmapAppendIndexBusy(&(*pMap),&rc) ){` |
|       7 |  665 | `			return rc; /* PH7_EXCEPTION/PH7_ABORT: php's catchable Error was thrown */` |
|       - |  666 | `		}` |
|       - |  667 | `		/* Assign an automatic index */` |
| 3050273 |  668 | `		rc = HashmapInsertIntKey(&(*pMap),pMap->iNextIdx,&(*pVal),0,FALSE);` |
| 3050273 |  669 | `		if( rc == SXRET_OK && pMap->iNextIdx < SXI64_HIGH ){` |
| 3050271 |  670 | `			++pMap->iNextIdx;` |
| 1525133 |  671 | `		}` |
|       - |  672 | `	}` |
|       - |  673 | `	/* Insertion result */` |
| 3073809 |  674 | `	return rc;` |
| 1590492 |  675 | `}` |
|       - |  676 | `/*` |
|       - |  677 | ` * Insert a given key and it's associated value (foreign index) in the given` |
|       - |  678 | ` * hashmap.` |
|       - |  679 | ` * This is insertion by reference so be careful to mark the node` |
|       - |  680 | ` * with the HASHMAP_NODE_FOREIGN_OBJ flag being set.` |
|       - |  681 | ` * The insertion by reference is triggered when the following` |
|       - |  682 | ` * expression is encountered.` |
|       - |  683 | ` * $var = 10;` |
|       - |  684 | ` *  $a = array(&var);` |
|       - |  685 | ` * OR` |
|       - |  686 | ` *  $a[] =& $var;` |
|       - |  687 | ` * That is,$var is a foreign ph7_value and the $a array have no control` |
|       - |  688 | ` * over it's contents.` |
|       - |  689 | ` * Note that the node that hold the foreign ph7_value is automatically` |
|       - |  690 | ` * removed when the foreign ph7_value is unset.` |
|       - |  691 | ` * Example:` |
|       - |  692 | ` *  $var = 10;` |
|       - |  693 | ` *  $a[] =& $var;` |
|       - |  694 | ` *  echo count($a).PHP_EOL; //1` |
|       - |  695 | ` *  //Unset the foreign ph7_value now` |
|       - |  696 | ` *  unset($var);` |
|       - |  697 | ` *  echo count($a); //0` |
|       - |  698 | ` * Note that this is a PH7 eXtension.` |
|       - |  699 | ` * Refer to the official documentation for more information.` |
|       - |  700 | ` * If a node with the given key already exists in the database` |
|       - |  701 | ` * then this function overwrite the old value.` |
|       - |  702 | ` */` |
|   46344 |  703 | `static sxi32 HashmapInsertByRef(` |
|       - |  704 | `	ph7_hashmap *pMap,   /* Target hashmap */` |
|       - |  705 | `	ph7_value *pKey,     /* Lookup key */` |
|       - |  706 | `	sxu32 nRefIdx        /* Foreign ph7_value index */` |
|       - |  707 | `	)` |
|       5 |  708 | `{` |
|   46349 |  709 | `	ph7_hashmap_node *pNode = 0;` |
|   46349 |  710 | `	sxi32 rc = SXRET_OK;` |
|   46349 |  711 | `	if( pKey && pKey->iFlags & (MEMOBJ_STRING\|MEMOBJ_HASHMAP\|MEMOBJ_OBJ\|MEMOBJ_RES) ){` |
|   46313 |  712 | `		if( (pKey->iFlags & MEMOBJ_STRING) == 0 ){` |
|       - |  713 | `			/* Force a string cast */` |
|     ! 0 |  714 | `			PH7_MemObjToString(&(*pKey));` |
|     ! 0 |  715 | `		}` |
|   46313 |  716 | `		if( SyBlobLength(&pKey->sBlob) < 1 \|\| HashmapIsIntKey(&pKey->sBlob) ){` |
|       3 |  717 | `			if(SyBlobLength(&pKey->sBlob) < 1){` |
|       - |  718 | `				/* Automatic index assign */` |
|     ! 0 |  719 | `				pKey = 0;` |
|     ! 0 |  720 | `			}` |
|       3 |  721 | `			goto IntKey;` |
|       - |  722 | `		}` |
|   69464 |  723 | `		if( SXRET_OK == HashmapLookupBlobKey(&(*pMap),SyBlobData(&pKey->sBlob),` |
|   23153 |  724 | `			SyBlobLength(&pKey->sBlob),&pNode) ){` |
|       - |  725 | `				/* Overwrite */` |
|       7 |  726 | `				PH7_VmRefObjRemove(pMap->pVm,pNode->nValIdx,0,pNode);` |
|       7 |  727 | `				pNode->nValIdx = nRefIdx;` |
|       - |  728 | `				/* Install in the reference table */` |
|       7 |  729 | `				PH7_VmRefObjInstall(pMap->pVm,nRefIdx,0,pNode,0);` |
|       7 |  730 | `				return SXRET_OK;` |
|       - |  731 | `		}` |
|       - |  732 | `		/* Perform a blob-key insertion */` |
|   46305 |  733 | `		rc = HashmapInsertBlobKey(&(*pMap),SyBlobData(&pKey->sBlob),SyBlobLength(&pKey->sBlob),0,nRefIdx,TRUE);` |
|   46305 |  734 | `		return rc;` |
|       - |  735 | `	}` |
|      18 |  736 | `IntKey:` |
|      39 |  737 | `	if( pKey ){` |
|       7 |  738 | `		if((pKey->iFlags & MEMOBJ_INT) == 0 ){` |
|       - |  739 | `			/* Force an integer cast */` |
|       3 |  740 | `			PH7_MemObjToInteger(pKey);` |
|       1 |  741 | `		}` |
|       7 |  742 | `		if( SXRET_OK == HashmapLookupIntKey(&(*pMap),pKey->x.iVal,&pNode) ){` |
|       - |  743 | `			/* Overwrite */` |
|     ! 0 |  744 | `			PH7_VmRefObjRemove(pMap->pVm,pNode->nValIdx,0,pNode);` |
|     ! 0 |  745 | `			pNode->nValIdx = nRefIdx;` |
|       - |  746 | `			/* Install in the reference table */` |
|     ! 0 |  747 | `			PH7_VmRefObjInstall(pMap->pVm,nRefIdx,0,pNode,0);` |
|     ! 0 |  748 | `			return SXRET_OK;` |
|       - |  749 | `		}` |
|       - |  750 | `		/* Perform a 64-bit-int-key insertion */` |
|       7 |  751 | `		rc = HashmapInsertIntKey(&(*pMap),pKey->x.iVal,0,nRefIdx,TRUE);` |
|       7 |  752 | `		if( rc == SXRET_OK ){` |
|       7 |  753 | `			HashmapAdvanceAutoIndex(&(*pMap),pKey->x.iVal);` |
|       3 |  754 | `		}` |
|       4 |  755 | `	}else{` |
|      33 |  756 | `		if( HashmapAppendIndexBusy(&(*pMap),&rc) ){` |
|     ! 0 |  757 | `			return rc; /* PH7_EXCEPTION/PH7_ABORT: php's catchable Error was thrown */` |
|       - |  758 | `		}` |
|       - |  759 | `		/* Assign an automatic index */` |
|      33 |  760 | `		rc = HashmapInsertIntKey(&(*pMap),pMap->iNextIdx,0,nRefIdx,TRUE);` |
|      33 |  761 | `		if( rc == SXRET_OK && pMap->iNextIdx < SXI64_HIGH ){` |
|      33 |  762 | `			++pMap->iNextIdx;` |
|      16 |  763 | `		}` |
|       - |  764 | `	}` |
|       - |  765 | `	/* Insertion result */` |
|      39 |  766 | `	return rc;` |
|   23177 |  767 | `}` |
|       - |  768 | `/*` |
|       - |  769 | ` * Extract node value.` |
|       - |  770 | ` */` |
| 1340086 |  771 | `static ph7_value * HashmapExtractNodeValue(ph7_hashmap_node *pNode)` |
|       5 |  772 | `{` |
|       - |  773 | `	/* Point to the desired object */` |
|       - |  774 | `	ph7_value *pObj;` |
| 1340091 |  775 | `	pObj = (ph7_value *)SySetAt(&pNode->pMap->pVm->aMemObj,pNode->nValIdx);` |
| 1340091 |  776 | `	return pObj;` |
|       5 |  777 | `}` |
|       - |  778 | `/*` |
|       - |  779 | ` * Insert a node in the given hashmap.` |
|       - |  780 | ` * If a node with the given key already exists in the database` |
|       - |  781 | ` * then this function overwrite the old value.` |
|       - |  782 | ` */` |
|     446 |  783 | `static sxi32 HashmapInsertNode(ph7_hashmap *pMap,ph7_hashmap_node *pNode,int bPreserve)` |
|       5 |  784 | `{` |
|       - |  785 | `	ph7_value *pObj;` |
|       - |  786 | `	sxi32 rc;` |
|       - |  787 | `	/* Extract the node value */` |
|     451 |  788 | `	pObj = HashmapExtractNodeValue(&(*pNode));` |
|     451 |  789 | `	if( pObj == 0 ){` |
|     ! 0 |  790 | `		return SXERR_EMPTY;` |
|       - |  791 | `	}` |
|       - |  792 | `	/* Preserve key */` |
|     451 |  793 | `	if( pNode->iType == HASHMAP_INT_NODE){` |
|       - |  794 | `		/* Int64 key */` |
|     321 |  795 | `		if( !bPreserve ){` |
|       - |  796 | `			/* Assign an automatic index */` |
|     173 |  797 | `			rc = HashmapInsert(&(*pMap),0,pObj);` |
|      89 |  798 | `		}else{` |
|     149 |  799 | `			rc = HashmapInsertIntKey(&(*pMap),pNode->xKey.iKey,pObj,0,FALSE);` |
|       - |  800 | `		}` |
|     163 |  801 | `	}else{` |
|       - |  802 | `		/* Blob key */` |
|     131 |  803 | `		if( !bPreserve ){` |
|       - |  804 | `			/* treat it like an automatically-indexed element, drop the` |
|       - |  805 | `			 * original string key entirely */` |
|      35 |  806 | `			rc = HashmapInsert(&(*pMap),0,pObj);` |
|      18 |  807 | `		}else{` |
|     145 |  808 | `			rc = HashmapInsertBlobKey(&(*pMap),SyBlobData(&pNode->xKey.sKey),` |
|      48 |  809 | `				SyBlobLength(&pNode->xKey.sKey),pObj,0,FALSE);` |
|       - |  810 | `		}` |
|       - |  811 | `	}` |
|     451 |  812 | `	return rc;` |
|     228 |  813 | `}` |
|       - |  814 | `/*` |
|       - |  815 | ` * Compare two node values.` |
|       - |  816 | ` * Return 0 if the node values are equals, > 0 if pLeft is greater than pRight` |
|       - |  817 | ` * or < 0 if pRight is greater than pLeft.` |
|       - |  818 | ` * For a full description on ph7_values comparison,refer to the implementation` |
|       - |  819 | ` * of the [PH7_MemObjCmp()] function defined in memobj.c or the official` |
|       - |  820 | ` * documenation.` |
|       - |  821 | ` */` |
|   68904 |  822 | `static sxi32 HashmapNodeCmp(ph7_hashmap_node *pLeft,ph7_hashmap_node *pRight,int bStrict)` |
|       5 |  823 | `{` |
|       - |  824 | `	ph7_value sObj1,sObj2;` |
|       - |  825 | `	sxi32 rc;` |
|   68909 |  826 | `	if( pLeft == pRight ){` |
|       - |  827 | `		/*` |
|       - |  828 | `		 * Same node.Refer to the sort() implementation defined` |
|       - |  829 | `		 * below for more information on this sceanario.` |
|       - |  830 | `		 */` |
|     ! 0 |  831 | `		return 0;` |
|       - |  832 | `	}` |
|       - |  833 | `	/* Do the comparison */` |
|   68909 |  834 | `	PH7_MemObjInit(pLeft->pMap->pVm,&sObj1);` |
|   68909 |  835 | `	PH7_MemObjInit(pLeft->pMap->pVm,&sObj2);` |
|   68909 |  836 | `	PH7_HashmapExtractNodeValue(pLeft,&sObj1,FALSE);` |
|   68909 |  837 | `	PH7_HashmapExtractNodeValue(pRight,&sObj2,FALSE);` |
|   68909 |  838 | `	rc = PH7_MemObjCmp(&sObj1,&sObj2,bStrict,0);` |
|   68909 |  839 | `	PH7_MemObjRelease(&sObj1);` |
|   68909 |  840 | `	PH7_MemObjRelease(&sObj2);` |
|   68909 |  841 | `	return rc;` |
|   34474 |  842 | `}` |
|       - |  843 | `/*` |
|       - |  844 | ` * Rehash a node with a 64-bit integer key.` |
|       - |  845 | ` * Refer to [merge_sort(),array_shift()] implementations for more information.` |
|       - |  846 | ` */` |
|   13192 |  847 | `static void HashmapRehashIntNode(ph7_hashmap_node *pEntry)` |
|       5 |  848 | `{` |
|   13197 |  849 | `	ph7_hashmap *pMap = pEntry->pMap;` |
|       - |  850 | `	sxu32 nBucket;` |
|       - |  851 | `	/* Remove old collision links */` |
|   13197 |  852 | `	if( pEntry->pPrevCollide ){` |
|   10807 |  853 | `		pEntry->pPrevCollide->pNextCollide = pEntry->pNextCollide;` |
|    5412 |  854 | `	}else{` |
|    2395 |  855 | `		pMap->apBucket[pEntry->nHash & (pMap->nSize - 1)] = pEntry->pNextCollide;` |
|       - |  856 | `	}` |
|   13197 |  857 | `	if( pEntry->pNextCollide ){` |
|    1075 |  858 | `		pEntry->pNextCollide->pPrevCollide = pEntry->pPrevCollide;` |
|     542 |  859 | `	}` |
|   13197 |  860 | `	pEntry->pNextCollide = pEntry->pPrevCollide = 0;` |
|       - |  861 | `	/* Compute the new hash */` |
|   13197 |  862 | `	pEntry->nHash = pMap->xIntHash(pMap->iNextIdx);` |
|   13197 |  863 | `	pEntry->xKey.iKey = pMap->iNextIdx;` |
|   13197 |  864 | `	nBucket = pEntry->nHash & (pMap->nSize - 1);` |
|       - |  865 | `	/* Link to the new bucket */` |
|   13197 |  866 | `	pEntry->pNextCollide = pMap->apBucket[nBucket];` |
|   13197 |  867 | `	if( pMap->apBucket[nBucket] ){` |
|   11122 |  868 | `		pMap->apBucket[nBucket]->pPrevCollide = pEntry;` |
|    5563 |  869 | `	}` |
|   13197 |  870 | `	pEntry->pNextCollide = pMap->apBucket[nBucket];` |
|   13197 |  871 | `	pMap->apBucket[nBucket] = pEntry;` |
|       - |  872 | `	/* Increment the automatic index (saturating, like every other advance —` |
|       - |  873 | `	 * unreachable in practice since renumbering assigns 0..nEntry-1, but keep` |
|       - |  874 | `	 * the no-overflow invariant uniform). */` |
|   13197 |  875 | `	if( pMap->iNextIdx < SXI64_HIGH ){` |
|   13197 |  876 | `		pMap->iNextIdx++;` |
|    6596 |  877 | `	}` |
|   13197 |  878 | `}` |
|       - |  879 | `/*` |
|       - |  880 | ` * Perform a linear search on a given hashmap.` |
|       - |  881 | ` * Write a pointer to the target node on success.` |
|       - |  882 | ` * Otherwise SXERR_NOTFOUND is returned on failure.` |
|       - |  883 | ` * Refer to [array_intersect(),array_diff(),in_array(),...] implementations` |
|       - |  884 | ` * for more information.` |
|       - |  885 | ` */` |
|   32200 |  886 | `static int HashmapFindValue(` |
|       - |  887 | `	ph7_hashmap *pMap,   /* Target hashmap */` |
|       - |  888 | `	ph7_value *pNeedle,  /* Lookup key */` |
|       - |  889 | `	ph7_hashmap_node **ppNode, /* OUT: target node on success  */` |
|       - |  890 | `	int bStrict      /* TRUE for strict comparison */` |
|       - |  891 | `	)` |
|       5 |  892 | `{` |
|       - |  893 | `	ph7_hashmap_node *pEntry;` |
|       - |  894 | `	ph7_value sVal,*pVal;` |
|       - |  895 | `	ph7_value sNeedle;` |
|       - |  896 | `	sxi32 rc;` |
|       - |  897 | `	sxu32 n;` |
|       - |  898 | `	/* Perform a linear search since we cannot sort the hashmap based on values */` |
|   32205 |  899 | `	pEntry = pMap->pFirst;` |
|   32205 |  900 | `	n = pMap->nEntry;` |
|   32205 |  901 | `	PH7_MemObjInit(pMap->pVm,&sVal);` |
|   32205 |  902 | `	PH7_MemObjInit(pMap->pVm,&sNeedle);` |
|   76519 |  903 | `	for(;;){` |
|  153045 |  904 | `		if( n < 1 ){` |
|      99 |  905 | `			break;` |
|       - |  906 | `		}` |
|       - |  907 | `		/* Extract node value */` |
|  152947 |  908 | `		pVal = HashmapExtractNodeValue(pEntry);` |
|  152947 |  909 | `		if( pVal ){` |
|  152947 |  910 | `			if( (pVal->iFlags\|pNeedle->iFlags) & MEMOBJ_NULL ){` |
|     ! 0 |  911 | `				sxi32 iF1 = pVal->iFlags&~MEMOBJ_AUX;` |
|     ! 0 |  912 | `				sxi32 iF2 = pNeedle->iFlags&~MEMOBJ_AUX;` |
|     ! 0 |  913 | `				if( iF1 == iF2 ){` |
|       - |  914 | `					/* NULL values are equals */` |
|     ! 0 |  915 | `					if( ppNode ){` |
|     ! 0 |  916 | `						*ppNode = pEntry;` |
|     ! 0 |  917 | `					}` |
|     ! 0 |  918 | `					return SXRET_OK;` |
|       - |  919 | `				}` |
|     ! 0 |  920 | `			}else{` |
|       - |  921 | `				/* Duplicate value */` |
|  152947 |  922 | `				PH7_MemObjLoad(pVal,&sVal);` |
|  152947 |  923 | `				PH7_MemObjLoad(pNeedle,&sNeedle);` |
|  152947 |  924 | `				rc = PH7_MemObjCmp(&sNeedle,&sVal,bStrict,0);` |
|  152947 |  925 | `				PH7_MemObjRelease(&sVal);` |
|  152947 |  926 | `				PH7_MemObjRelease(&sNeedle);` |
|  152947 |  927 | `				if( rc == 0 ){` |
|   32107 |  928 | `					if( ppNode ){` |
|      23 |  929 | `						*ppNode = pEntry;` |
|      11 |  930 | `					}` |
|       - |  931 | `					/* Match found*/` |
|   32107 |  932 | `					return SXRET_OK;` |
|       - |  933 | `				}` |
|       - |  934 | `			}` |
|   60419 |  935 | `		}` |
|       - |  936 | `		/* Point to the next entry */` |
|  120845 |  937 | `		pEntry = pEntry->pPrev; /* Reverse link */` |
|  120845 |  938 | `		n--;` |
|       5 |  939 | `	}` |
|       - |  940 | `	/* No such entry */` |
|      99 |  941 | `	return SXERR_NOTFOUND;` |
|   16105 |  942 | `}` |
|       - |  943 | `/*` |
|       - |  944 | ` * Perform a linear search on a given hashmap but use an user-defined callback` |
|       - |  945 | ` * for values comparison.` |
|       - |  946 | ` * Write a pointer to the target node on success.` |
|       - |  947 | ` * Otherwise SXERR_NOTFOUND is returned on failure.` |
|       - |  948 | ` * Refer to [array_uintersect(),array_udiff()...] implementations` |
|       - |  949 | ` * for more information.` |
|       - |  950 | ` */` |
|      22 |  951 | `static int HashmapFindValueByCallback(` |
|       - |  952 | `	ph7_hashmap *pMap,     /* Target hashmap */` |
|       - |  953 | `	ph7_value *pNeedle,    /* Lookup key */` |
|       - |  954 | `	ph7_value *pCallback,  /* User defined callback */` |
|       - |  955 | `	ph7_hashmap_node **ppNode /* OUT: target node on success */` |
|       - |  956 | `	)` |
|       1 |  957 | `{` |
|       - |  958 | `	ph7_hashmap_node *pEntry;` |
|       - |  959 | `	ph7_value sResult,*pVal;` |
|       - |  960 | `	ph7_value *apArg[2];    /* Callback arguments */` |
|       - |  961 | `	sxi32 rc;` |
|       - |  962 | `	sxu32 n;` |
|      23 |  963 | `	if( pMap->pVm->iCmpCallbackExc ){` |
|       - |  964 | `		/* A previous comparison already raised: stop invoking the callback so the` |
|       - |  965 | `		 * exception is not thrown again, and let the caller wind down. */` |
|     ! 0 |  966 | `		return SXERR_NOTFOUND;` |
|       - |  967 | `	}` |
|       - |  968 | `	/* Perform a linear search since we cannot sort the array based on values */` |
|      23 |  969 | `	pEntry = pMap->pFirst;` |
|      23 |  970 | `	n = pMap->nEntry;` |
|       - |  971 | `	/* Store callback result here */` |
|      23 |  972 | `	PH7_MemObjInit(pMap->pVm,&sResult);` |
|       - |  973 | `	/* First argument to the callback */` |
|      23 |  974 | `	apArg[0] = pNeedle;` |
|      25 |  975 | `	for(;;){` |
|      51 |  976 | `		if( n < 1 ){` |
|       9 |  977 | `			break;` |
|       - |  978 | `		}` |
|       - |  979 | `		/* Extract node value */` |
|      43 |  980 | `		pVal = HashmapExtractNodeValue(pEntry);` |
|      43 |  981 | `		if( pVal ){` |
|       - |  982 | `			/* Invoke the user callback */` |
|      43 |  983 | `			apArg[1] = pVal; /* Second argument to the callback */` |
|      43 |  984 | `			rc = PH7_VmCallUserFunction(pMap->pVm,pCallback,2,apArg,&sResult);` |
|      43 |  985 | `			if( rc == PH7_EXCEPTION ){` |
|       - |  986 | `				/* The callback raised: flag it so the caller aborts and propagates,` |
|       - |  987 | `				 * and report no match for the rest of the run. */` |
|       5 |  988 | `				pMap->pVm->iCmpCallbackExc = 1;` |
|       5 |  989 | `				PH7_MemObjRelease(&sResult);` |
|       5 |  990 | `				return SXERR_NOTFOUND;` |
|       - |  991 | `			}` |
|      39 |  992 | `			if( rc == SXRET_OK ){` |
|       - |  993 | `				/* Extract callback result */` |
|      39 |  994 | `				if( (sResult.iFlags & MEMOBJ_INT) == 0 ){` |
|       - |  995 | `					/* Perform an int cast */` |
|     ! 0 |  996 | `					PH7_MemObjToInteger(&sResult);` |
|     ! 0 |  997 | `				}` |
|      39 |  998 | `				rc = (sxi32)sResult.x.iVal;` |
|      39 |  999 | `				PH7_MemObjRelease(&sResult);` |
|      39 | 1000 | `				if( rc == 0 ){` |
|       - | 1001 | `					/* Match found*/` |
|      11 | 1002 | `					if( ppNode ){` |
|     ! 0 | 1003 | `						*ppNode = pEntry;` |
|     ! 0 | 1004 | `					}` |
|      11 | 1005 | `					return SXRET_OK;` |
|       - | 1006 | `				}` |
|      14 | 1007 | `			}` |
|      14 | 1008 | `		}` |
|       - | 1009 | `		/* Point to the next entry */` |
|      29 | 1010 | `		pEntry = pEntry->pPrev; /* Reverse link */` |
|      29 | 1011 | `		n--;` |
|       1 | 1012 | `	}` |
|       - | 1013 | `	/* No such entry */` |
|       9 | 1014 | `	return SXERR_NOTFOUND;` |
|      12 | 1015 | `}` |
|       - | 1016 | `/*` |
|       - | 1017 | ` * Compare two hashmaps.` |
|       - | 1018 | ` * Return 0 if the hashmaps are equals.Any other value indicates inequality.` |
|       - | 1019 | ` * Note on array comparison operators.` |
|       - | 1020 | ` *  According to the PHP language reference manual.` |
|       - | 1021 | ` *  Array Operators Example 	Name 	Result` |
|       - | 1022 | ` *  $a + $b 	Union 	Union of $a and $b.` |
|       - | 1023 | ` *  $a == $b 	Equality 	TRUE if $a and $b have the same key/value pairs.` |
|       - | 1024 | ` *  $a === $b 	Identity 	TRUE if $a and $b have the same key/value pairs in the same` |
|       - | 1025 | ` *                          order and of the same types.` |
|       - | 1026 | ` *  $a != $b 	Inequality 	TRUE if $a is not equal to $b.` |
|       - | 1027 | ` *  $a <> $b 	Inequality 	TRUE if $a is not equal to $b.` |
|       - | 1028 | ` *  $a !== $b 	Non-identity 	TRUE if $a is not identical to $b.` |
|       - | 1029 | ` * The + operator returns the right-hand array appended to the left-hand array;` |
|       - | 1030 | ` * For keys that exist in both arrays, the elements from the left-hand array will be used` |
|       - | 1031 | ` * and the matching elements from the right-hand array will be ignored.` |
|       - | 1032 | ` * <?php` |
|       - | 1033 | ` * $a = array("a" => "apple", "b" => "banana");` |
|       - | 1034 | ` * $b = array("a" => "pear", "b" => "strawberry", "c" => "cherry");` |
|       - | 1035 | ` * $c = $a + $b; // Union of $a and $b` |
|       - | 1036 | ` * echo "Union of \$a and \$b: \n";` |
|       - | 1037 | ` * var_dump($c);` |
|       - | 1038 | ` * $c = $b + $a; // Union of $b and $a` |
|       - | 1039 | ` * echo "Union of \$b and \$a: \n";` |
|       - | 1040 | ` * var_dump($c);` |
|       - | 1041 | ` * ?>` |
|       - | 1042 | ` * When executed, this script will print the following:` |
|       - | 1043 | ` * Union of $a and $b:` |
|       - | 1044 | ` * array(3) {` |
|       - | 1045 | ` *  ["a"]=>` |
|       - | 1046 | ` *  string(5) "apple"` |
|       - | 1047 | ` *  ["b"]=>` |
|       - | 1048 | ` * string(6) "banana"` |
|       - | 1049 | ` *  ["c"]=>` |
|       - | 1050 | ` * string(6) "cherry"` |
|       - | 1051 | ` * }` |
|       - | 1052 | ` * Union of $b and $a:` |
|       - | 1053 | ` * array(3) {` |
|       - | 1054 | ` * ["a"]=>` |
|       - | 1055 | ` * string(4) "pear"` |
|       - | 1056 | ` * ["b"]=>` |
|       - | 1057 | ` * string(10) "strawberry"` |
|       - | 1058 | ` * ["c"]=>` |
|       - | 1059 | ` * string(6) "cherry"` |
|       - | 1060 | ` * }` |
|       - | 1061 | ` * Elements of arrays are equal for the comparison if they have the same key and value.` |
|       - | 1062 | ` */` |
|      28 | 1063 | `PH7_PRIVATE sxi32 PH7_HashmapCmp(` |
|       - | 1064 | `	ph7_hashmap *pLeft,  /* Left hashmap */` |
|       - | 1065 | `	ph7_hashmap *pRight, /* Right hashmap */` |
|       - | 1066 | `	int bStrict          /* TRUE for strict comparison */` |
|       - | 1067 | `	)` |
|       1 | 1068 | `{` |
|       - | 1069 | `	ph7_hashmap_node *pLe,*pRe;` |
|       - | 1070 | `	sxi32 rc;` |
|       - | 1071 | `	sxu32 n;` |
|      29 | 1072 | `	if( pLeft == pRight ){` |
|       - | 1073 | `		/* Same hashmap instance. This can easily happen since hashmaps are passed by reference.` |
|       - | 1074 | `		 * Unlike the zend engine.` |
|       - | 1075 | `		 */` |
|     ! 0 | 1076 | `		return 0;` |
|       - | 1077 | `	}` |
|      29 | 1078 | `	if( pLeft->nEntry != pRight->nEntry ){` |
|       - | 1079 | `		/* Must have the same number of entries */` |
|       5 | 1080 | `		return pLeft->nEntry > pRight->nEntry ? 1 : -1;` |
|       - | 1081 | `	}` |
|       - | 1082 | `	/* Point to the first inserted entry of the left hashmap */` |
|      25 | 1083 | `	pLe = pLeft->pFirst;` |
|      25 | 1084 | `	pRe = 0; /* cc warning */` |
|       - | 1085 | `	/* Perform the comparison */` |
|      25 | 1086 | `	n = pLeft->nEntry;` |
|      59 | 1087 | `	for(;;){` |
|     119 | 1088 | `		if( n < 1 ){` |
|      23 | 1089 | `			break;` |
|       - | 1090 | `		}` |
|      97 | 1091 | `		if( pLe->iType == HASHMAP_INT_NODE){` |
|       - | 1092 | `			/* Int key */` |
|      89 | 1093 | `			rc = HashmapLookupIntKey(&(*pRight),pLe->xKey.iKey,&pRe);` |
|      45 | 1094 | `		}else{` |
|       9 | 1095 | `			SyBlob *pKey = &pLe->xKey.sKey;` |
|       - | 1096 | `			/* Blob key */` |
|       9 | 1097 | `			rc = HashmapLookupBlobKey(&(*pRight),SyBlobData(pKey),SyBlobLength(pKey),&pRe);` |
|       - | 1098 | `		}` |
|      97 | 1099 | `		if( rc != SXRET_OK ){` |
|       - | 1100 | `			/* No such entry in the right side */` |
|     ! 0 | 1101 | `			return 1;` |
|       - | 1102 | `		}` |
|      97 | 1103 | `		rc = 0;` |
|      97 | 1104 | `		if( bStrict ){` |
|       - | 1105 | `			/* Make sure,the keys are of the same type */` |
|      81 | 1106 | `			if( pLe->iType != pRe->iType ){` |
|     ! 0 | 1107 | `				rc = 1;` |
|     ! 0 | 1108 | `			}` |
|      40 | 1109 | `		}` |
|      97 | 1110 | `		if( !rc ){` |
|       - | 1111 | `			/* Compare nodes */` |
|      97 | 1112 | `			rc = HashmapNodeCmp(pLe,pRe,bStrict);` |
|      48 | 1113 | `		}` |
|      97 | 1114 | `		if( rc != 0 ){` |
|       - | 1115 | `			/* Nodes key/value differ */` |
|       3 | 1116 | `			return rc;` |
|       - | 1117 | `		}` |
|       - | 1118 | `		/* Point to the next entry */` |
|      95 | 1119 | `		pLe = pLe->pPrev; /* Reverse link */` |
|      95 | 1120 | `		n--;` |
|       1 | 1121 | `	}` |
|      23 | 1122 | `	return 0; /* Hashmaps are equals */` |
|      15 | 1123 | `}` |
|       - | 1124 | `/*` |
|       - | 1125 | ` * Duplicate a hashmap node.` |
|       - | 1126 | ` * This function is used by HashmapMerge, HashmapOverwrite and PH7_HashmapDup.` |
|       - | 1127 | ` */` |
|  621228 | 1128 | `static sxi32 HashmapDuplicateNode(` |
|       - | 1129 | `	ph7_hashmap *pDest,` |
|       - | 1130 | `	ph7_hashmap_node *pEntry,` |
|       - | 1131 | `	ph7_value *pVal,` |
|       - | 1132 | `	int iAction /* 0: Merge, 1: Overwrite, 2: Dup */` |
|       - | 1133 | `	)` |
|       5 | 1134 | `{` |
|       - | 1135 | `	ph7_value sSafeVal;` |
|       - | 1136 | `	ph7_value sKey;` |
|       - | 1137 | `	sxi32 rc;` |
|       - | 1138 |  |
|  621233 | 1139 | `	if( pEntry->iFlags & HASHMAP_NODE_FOREIGN_OBJ ){` |
|       - | 1140 | `		/* The source node holds a reference to a foreign ph7_value (e.g: [&$x]).` |
|       - | 1141 | `		 * Re-insert it by reference so the reference survives the duplication` |
|       - | 1142 | `		 * instead of being flattened to a value copy. This keeps spread` |
|       - | 1143 | `		 * ([...$a]), array_merge(), array_replace() and array copies in sync` |
|       - | 1144 | `		 * with PHP semantics. */` |
|       7 | 1145 | `		sxu32 nRefIdx = pEntry->nValIdx;` |
|       7 | 1146 | `		if( pEntry->iType == HASHMAP_BLOB_NODE ){` |
|       3 | 1147 | `			PH7_MemObjInitFromString(pDest->pVm,&sKey,0);` |
|       3 | 1148 | `			PH7_MemObjStringAppend(&sKey,(const char *)SyBlobData(&pEntry->xKey.sKey),SyBlobLength(&pEntry->xKey.sKey));` |
|       3 | 1149 | `			rc = HashmapInsertByRef(pDest,&sKey,nRefIdx);` |
|       3 | 1150 | `			PH7_MemObjRelease(&sKey);` |
|       2 | 1151 | `		}else{` |
|       5 | 1152 | `			if( iAction == 0 ){ /* Merge: automatic index assign */` |
|       5 | 1153 | `				rc = HashmapInsertByRef(pDest,0,nRefIdx);` |
|       2 | 1154 | `			}else if( iAction == 1 ){ /* Overwrite: keep the int key */` |
|     ! 0 | 1155 | `				PH7_MemObjInitFromInt(pDest->pVm,&sKey,pEntry->xKey.iKey);` |
|     ! 0 | 1156 | `				rc = HashmapInsertByRef(pDest,&sKey,nRefIdx);` |
|     ! 0 | 1157 | `				PH7_MemObjRelease(&sKey);` |
|     ! 0 | 1158 | `			}else{ /* Dup: preserve the int key */` |
|     ! 0 | 1159 | `				rc = HashmapInsertIntKey(pDest,pEntry->xKey.iKey,0,nRefIdx,TRUE);` |
|       - | 1160 | `			}` |
|       - | 1161 | `		}` |
|       7 | 1162 | `		return rc;` |
|       - | 1163 | `	}` |
|  621227 | 1164 | `	sSafeVal = *pVal;` |
|       - | 1165 |  |
|  621227 | 1166 | `	if( pEntry->iType == HASHMAP_BLOB_NODE ){` |
|       - | 1167 | `		/* Blob key insertion */` |
|    4031 | 1168 | `		PH7_MemObjInitFromString(pDest->pVm,&sKey,0);` |
|    4031 | 1169 | `		PH7_MemObjStringAppend(&sKey,(const char *)SyBlobData(&pEntry->xKey.sKey),SyBlobLength(&pEntry->xKey.sKey));` |
|    4031 | 1170 | `		rc = PH7_HashmapInsert(pDest,&sKey,&sSafeVal);` |
|    4031 | 1171 | `		PH7_MemObjRelease(&sKey);` |
|    2018 | 1172 | `	}else{` |
|       - | 1173 | `		/* Int key */` |
|  617201 | 1174 | `		if( iAction == 0 ){ /* Merge */` |
|  616987 | 1175 | `			rc = HashmapInsert(pDest,0/* Automatic index assign */,&sSafeVal);` |
|  308708 | 1176 | `		}else if( iAction == 1 ){ /* Overwrite */` |
|      32 | 1177 | `			PH7_MemObjInitFromInt(pDest->pVm,&sKey,pEntry->xKey.iKey);` |
|      32 | 1178 | `			rc = PH7_HashmapInsert(pDest,&sKey,&sSafeVal);` |
|      32 | 1179 | `			PH7_MemObjRelease(&sKey);` |
|      17 | 1180 | `		}else{ /* Dup */` |
|     186 | 1181 | `			rc = HashmapInsertIntKey(pDest,pEntry->xKey.iKey,&sSafeVal,0,FALSE);` |
|       - | 1182 | `		}` |
|       - | 1183 | `	}` |
|  621227 | 1184 | `	return rc;` |
|  310619 | 1185 | `}` |
|       - | 1186 | `/*` |
|       - | 1187 | ` * Merge two hashmaps.` |
|       - | 1188 | ` * Note on the merge process` |
|       - | 1189 | ` * According to the PHP language reference manual.` |
|       - | 1190 | ` *  Merges the elements of two arrays together so that the values of one are appended` |
|       - | 1191 | ` *  to the end of the previous one. It returns the resulting array (pDest).` |
|       - | 1192 | ` *  If the input arrays have the same string keys, then the later value for that key` |
|       - | 1193 | ` *  will overwrite the previous one. If, however, the arrays contain numeric keys` |
|       - | 1194 | ` *  the later value will not overwrite the original value, but will be appended.` |
|       - | 1195 | ` *  Values in the input array with numeric keys will be renumbered with incrementing` |
|       - | 1196 | ` *  keys starting from zero in the result array.` |
|       - | 1197 | ` */` |
|    2104 | 1198 | `static sxi32 HashmapMerge(ph7_hashmap *pSrc,ph7_hashmap *pDest)` |
|       5 | 1199 | `{` |
|       - | 1200 | `	ph7_hashmap_node *pEntry;` |
|       - | 1201 | `	ph7_value *pVal;` |
|       - | 1202 | `	sxi32 rc;` |
|       - | 1203 | `	sxu32 n;` |
|    2109 | 1204 | `	if( pSrc == pDest ){` |
|       - | 1205 | `		/* Same map. This can easily happen since hashmaps are passed by reference.` |
|       - | 1206 | `		 * Unlike the zend engine.` |
|       - | 1207 | `		 */` |
|     ! 0 | 1208 | `		return SXRET_OK;` |
|       - | 1209 | `	}` |
|       - | 1210 | `	/* Point to the first inserted entry in the source */` |
|    2109 | 1211 | `	pEntry = pSrc->pFirst;` |
|       - | 1212 | `	/* Perform the merge */` |
|  619149 | 1213 | `	for( n = 0 ; n < pSrc->nEntry ; ++n ){` |
|       - | 1214 | `		/* Extract the node value */` |
|  617045 | 1215 | `		pVal = HashmapExtractNodeValue(pEntry);` |
|  617045 | 1216 | `		if( pVal ){` |
|       - | 1217 | `			/* Make a local copy of the value.` |
|       - | 1218 | `			 * The insertion call below may trigger a memory pool reallocation` |
|       - | 1219 | `			 * which will invalidate the 'pVal' pointer since it points` |
|       - | 1220 | `			 * to the old pool.` |
|       - | 1221 | `			 */` |
|  617045 | 1222 | `			rc = HashmapDuplicateNode(pDest,pEntry,pVal,0);` |
|  308525 | 1223 | `		}else{` |
|     ! 0 | 1224 | `			rc = SXRET_OK;` |
|       - | 1225 | `		}` |
|  617045 | 1226 | `		if( rc != SXRET_OK ){` |
|     ! 0 | 1227 | `			return rc;` |
|       - | 1228 | `		}` |
|       - | 1229 | `		/* Point to the next entry */` |
|  617045 | 1230 | `		pEntry = pEntry->pPrev; /* Reverse link */` |
|  308525 | 1231 | `	}` |
|    2109 | 1232 | `	return SXRET_OK;` |
|    1057 | 1233 | `}` |
|       - | 1234 | `/*` |
|       - | 1235 | ` * Overwrite entries with the same key.` |
|       - | 1236 | ` * Refer to the [array_replace()] implementation for more information.` |
|       - | 1237 | ` *  According to the PHP language reference manual.` |
|       - | 1238 | ` *  array_replace() replaces the values of the first array with the same values` |
|       - | 1239 | ` *  from all the following arrays. If a key from the first array exists in the second` |
|       - | 1240 | ` *  array, its value will be replaced by the value from the second array. If the key` |
|       - | 1241 | ` *  exists in the second array, and not the first, it will be created in the first array.` |
|       - | 1242 | ` *  If a key only exists in the first array, it will be left as is. If several arrays` |
|       - | 1243 | ` *  are passed for replacement, they will be processed in order, the later arrays` |
|       - | 1244 | ` *  overwriting the previous values.` |
|       - | 1245 | ` *  array_replace() is not recursive : it will replace values in the first array` |
|       - | 1246 | ` *  by whatever type is in the second array.` |
|       - | 1247 | ` */` |
|      34 | 1248 | `static sxi32 HashmapOverwrite(ph7_hashmap *pSrc,ph7_hashmap *pDest)` |
|       2 | 1249 | `{` |
|       - | 1250 | `	ph7_hashmap_node *pEntry;` |
|       - | 1251 | `	ph7_value *pVal;` |
|       - | 1252 | `	sxi32 rc;` |
|       - | 1253 | `	sxu32 n;` |
|      36 | 1254 | `	if( pSrc == pDest ){` |
|       - | 1255 | `		/* Same map. This can easily happen since hashmaps are passed by reference.` |
|       - | 1256 | `		 * Unlike the zend engine.` |
|       - | 1257 | `		 */` |
|     ! 0 | 1258 | `		return SXRET_OK;` |
|       - | 1259 | `	}` |
|       - | 1260 | `	/* Point to the first inserted entry in the source */` |
|      36 | 1261 | `	pEntry = pSrc->pFirst;` |
|       - | 1262 | `	/* Perform the merge */` |
|      80 | 1263 | `	for( n = 0 ; n < pSrc->nEntry ; ++n ){` |
|       - | 1264 | `		/* Extract the node value */` |
|      46 | 1265 | `		pVal = HashmapExtractNodeValue(pEntry);` |
|      46 | 1266 | `		if( pVal ){` |
|      46 | 1267 | `			rc = HashmapDuplicateNode(pDest,pEntry,pVal,1);` |
|      24 | 1268 | `		}else{` |
|     ! 0 | 1269 | `			rc = SXRET_OK;` |
|       - | 1270 | `		}` |
|      46 | 1271 | `		if( rc != SXRET_OK ){` |
|     ! 0 | 1272 | `			return rc;` |
|       - | 1273 | `		}` |
|       - | 1274 | `		/* Point to the next entry */` |
|      46 | 1275 | `		pEntry = pEntry->pPrev; /* Reverse link */` |
|      24 | 1276 | `	}` |
|      36 | 1277 | `	return SXRET_OK;` |
|      19 | 1278 | `}` |
|       - | 1279 | `/*` |
|       - | 1280 | ` * Duplicate the contents of a hashmap. Store the copy in pDest.` |
|       - | 1281 | ` * Refer to the [array_pad(),array_copy(),...] implementation for more information.` |
|       - | 1282 | ` */` |
|    3924 | 1283 | `PH7_PRIVATE sxi32 PH7_HashmapDup(ph7_hashmap *pSrc,ph7_hashmap *pDest)` |
|       5 | 1284 | `{` |
|       - | 1285 | `	ph7_hashmap_node *pEntry;` |
|       - | 1286 | `	ph7_value *pVal;` |
|       - | 1287 | `	sxi32 rc;` |
|       - | 1288 | `	sxu32 n;` |
|    3929 | 1289 | `	if( pSrc == pDest ){` |
|       - | 1290 | `		/* Same map. This can easily happen since hashmaps are passed by reference.` |
|       - | 1291 | `		 * Unlike the zend engine.` |
|       - | 1292 | `		 */` |
|     ! 0 | 1293 | `		return SXRET_OK;` |
|       - | 1294 | `	}` |
|       - | 1295 | `	/* Point to the first inserted entry in the source */` |
|    3929 | 1296 | `	pEntry = pSrc->pFirst;` |
|       - | 1297 | `	/* Perform the duplication */` |
|    8073 | 1298 | `	for( n = 0 ; n < pSrc->nEntry ; ++n ){` |
|       - | 1299 | `		/* Extract the node value */` |
|    4149 | 1300 | `		pVal = HashmapExtractNodeValue(pEntry);` |
|    4149 | 1301 | `		if( pVal ){` |
|    4149 | 1302 | `			rc = HashmapDuplicateNode(pDest,pEntry,pVal,2);` |
|    2077 | 1303 | `		}else{` |
|     ! 0 | 1304 | `			rc = SXRET_OK;` |
|       - | 1305 | `		}` |
|    4149 | 1306 | `		if( rc != SXRET_OK ){` |
|     ! 0 | 1307 | `			return rc;` |
|       - | 1308 | `		}` |
|       - | 1309 | `		/* Point to the next entry */` |
|    4149 | 1310 | `		pEntry = pEntry->pPrev; /* Reverse link */` |
|    2077 | 1311 | `	}` |
|    3929 | 1312 | `	return SXRET_OK;` |
|    1967 | 1313 | `}` |
|       - | 1314 | `/*` |
|       - | 1315 | ` * Duplicate a hashmap, flattening every foreign (by-reference) node into a` |
|       - | 1316 | ` * plain value copy. php 8.1 gives a COPY of $GLOBALS pure value semantics` |
|       - | 1317 | ` * ($snap = $GLOBALS snapshots the symbol table: later writes on either side` |
|       - | 1318 | ` * never affect the other) — unlike ordinary array copies, where reference` |
|       - | 1319 | ` * elements stay live — so the $GLOBALS store path (PH7_MemObjStore) uses` |
|       - | 1320 | ` * this instead of PH7_HashmapDup.` |
|       - | 1321 | ` */` |
|      12 | 1322 | `PH7_PRIVATE sxi32 PH7_HashmapDupMaterialized(ph7_hashmap *pSrc,ph7_hashmap *pDest)` |
|       1 | 1323 | `{` |
|       - | 1324 | `	ph7_hashmap_node *pEntry;` |
|       - | 1325 | `	ph7_value *pVal;` |
|       - | 1326 | `	sxi32 rc;` |
|       - | 1327 | `	sxu32 n;` |
|      13 | 1328 | `	if( pSrc == pDest ){` |
|     ! 0 | 1329 | `		return SXRET_OK;` |
|       - | 1330 | `	}` |
|      13 | 1331 | `	pEntry = pSrc->pFirst;` |
|     711 | 1332 | `	for( n = 0 ; n < pSrc->nEntry ; ++n ){` |
|       - | 1333 | `		/* Extract the node value (resolves foreign references) */` |
|     699 | 1334 | `		pVal = HashmapExtractNodeValue(pEntry);` |
|     698 | 1335 | `		if( pVal && (pVal->iFlags & MEMOBJ_HASHMAP)` |
|     459 | 1336 | `		 && (ph7_hashmap *)pVal->x.pOther == pSrc->pVm->pGlobal ){` |
|       - | 1337 | `			/* A global still holding the live $GLOBALS map is the snapshot's` |
|       - | 1338 | `			 * own destination mid-store ($snap = $GLOBALS registers $snap` |
|       - | 1339 | `			 * before the value lands). php's snapshot — taken when $GLOBALS` |
|       - | 1340 | `			 * is READ, before the assignment — has no such entry, so skip it` |
|       - | 1341 | `			 * (also breaks the would-be infinite recursion). */` |
|       5 | 1342 | `			pVal = 0;` |
|       2 | 1343 | `		}` |
|     699 | 1344 | `		if( pVal ){` |
|     695 | 1345 | `			if( pEntry->iType == HASHMAP_BLOB_NODE ){` |
|    1036 | 1346 | `				rc = HashmapInsertBlobKey(&(*pDest),SyBlobData(&pEntry->xKey.sKey),` |
|     345 | 1347 | `					SyBlobLength(&pEntry->xKey.sKey),pVal,0,FALSE);` |
|     346 | 1348 | `			}else{` |
|       5 | 1349 | `				rc = HashmapInsertIntKey(&(*pDest),pEntry->xKey.iKey,pVal,0,FALSE);` |
|       - | 1350 | `			}` |
|     695 | 1351 | `			if( rc != SXRET_OK ){` |
|     ! 0 | 1352 | `				return rc;` |
|       - | 1353 | `			}` |
|     347 | 1354 | `		}` |
|       - | 1355 | `		/* Point to the next entry */` |
|     699 | 1356 | `		pEntry = pEntry->pPrev; /* Reverse link */` |
|     350 | 1357 | `	}` |
|      13 | 1358 | `	return SXRET_OK;` |
|       7 | 1359 | `}` |
|       - | 1360 | `/*` |
|       - | 1361 | ` * Copy-on-write separation for arrays.` |
|       - | 1362 | ` * If the hashmap inside pValue has iRef > 1 (shared), duplicate it so that` |
|       - | 1363 | ` * pValue owns a private copy. The original map's refcount is decremented.` |
|       - | 1364 | ` * Returns the (possibly new) hashmap pointer.` |
|       - | 1365 | ` */` |
|  215548 | 1366 | `PH7_PRIVATE ph7_hashmap * PH7_HashmapCowSeparate(ph7_vm *pVm,ph7_value *pValue)` |
|       5 | 1367 | `{` |
|  215553 | 1368 | `	ph7_hashmap *pMap = (ph7_hashmap *)pValue->x.pOther;` |
|       - | 1369 | `	ph7_hashmap *pNew;` |
|       - | 1370 | `	ph7_value *pBacking;` |
|       - | 1371 | `	sxu32 nValIdx;` |
|       - | 1372 | `	int bValueInPool;` |
|  215553 | 1373 | `	if( pMap->iRef < 2 ){` |
|       - | 1374 | `		/* Sole owner, no separation needed */` |
|  213255 | 1375 | `		return pMap;` |
|       - | 1376 | `	}` |
|    2303 | 1377 | `	if( pMap == pVm->pGlobal ){` |
|       - | 1378 | `		/* Never separate $GLOBALS — it is a live view of the symbol table.` |
|       - | 1379 | `		 * (A COPY of $GLOBALS never shares this map: PH7_MemObjStore` |
|       - | 1380 | `		 * materializes a by-value snapshot at assignment, php 8.1.) */` |
|     119 | 1381 | `		return pMap;` |
|       - | 1382 | `	}` |
|       - | 1383 | `	/* If this value is a stack copy of a named variable, separate the` |
|       - | 1384 | `	 * backing variable instead so the change persists after the stack` |
|       - | 1385 | `	 * frame is popped. */` |
|    2185 | 1386 | `	if( pValue->nIdx != SXU32_HIGH ){` |
|    2185 | 1387 | `		pBacking = (ph7_value *)SySetAt(&pVm->aMemObj,pValue->nIdx);` |
|    2180 | 1388 | `		if( pBacking && pBacking != pValue` |
|    2160 | 1389 | `			&& (pBacking->iFlags & MEMOBJ_HASHMAP)` |
|    2145 | 1390 | `			&& (ph7_hashmap *)pBacking->x.pOther == pMap ){` |
|       - | 1391 | `			/* Undo the stack ref to reveal true sharing count */` |
|    2145 | 1392 | `			pMap->iRef--;` |
|    2145 | 1393 | `			if( pMap->iRef < 2 ){` |
|       - | 1394 | `				/* After undoing stack ref, sole owner — no separation */` |
|    2103 | 1395 | `				pMap->iRef++;` |
|    2103 | 1396 | `				return pMap;` |
|       - | 1397 | `			}` |
|      44 | 1398 | `			pNew = PH7_NewHashmap(pVm,0,0);` |
|      44 | 1399 | `			if( pNew == 0 ){` |
|     ! 0 | 1400 | `				pMap->iRef++;` |
|     ! 0 | 1401 | `				return pMap;` |
|       - | 1402 | `			}` |
|      44 | 1403 | `			if( PH7_HashmapDup(pMap,pNew) != SXRET_OK ){` |
|       - | 1404 | `				/* Dup failed (OOM) — discard partial copy, restore state */` |
|     ! 0 | 1405 | `				PH7_HashmapRelease(pNew,TRUE);` |
|     ! 0 | 1406 | `				pMap->iRef++;` |
|     ! 0 | 1407 | `				return pMap;` |
|       - | 1408 | `			}` |
|      44 | 1409 | `			pNew->iNextIdx = pMap->iNextIdx;` |
|      44 | 1410 | `			pMap->iRef--;  /* Backing variable no longer references old map */` |
|       - | 1411 | `			/* PH7_HashmapDup reserves a memory object per duplicated entry, which` |
|       - | 1412 | `			 * can grow — and therefore reallocate (move) — pVm->aMemObj. That` |
|       - | 1413 | `			 * invalidates the pBacking pointer captured above, so re-resolve it` |
|       - | 1414 | `			 * from the (stable) slot index before writing. Using the stale pointer` |
|       - | 1415 | `			 * dereferences the freed old buffer, which is a hard SIGSEGV on` |
|       - | 1416 | `			 * glibc/x86_64 once aMemObj is large enough to be mmap-backed (the old` |
|       - | 1417 | `			 * mapping is munmap'd on move) and a silent use-after-free elsewhere. */` |
|      44 | 1418 | `			pBacking = (ph7_value *)SySetAt(&pVm->aMemObj,pValue->nIdx);` |
|      44 | 1419 | `			if( pBacking ){` |
|      44 | 1420 | `				pBacking->x.pOther = pNew;` |
|      21 | 1421 | `			}` |
|       - | 1422 | `			/* Update the stack value to match */` |
|      44 | 1423 | `			pValue->x.pOther = pNew;` |
|      44 | 1424 | `			pNew->iRef++;  /* +1 for stack (pValue); iRef=1 from NewHashmap covers pBacking */` |
|      44 | 1425 | `			return pNew;` |
|       - | 1426 | `		}` |
|      20 | 1427 | `	}` |
|       - | 1428 | `	/* Some callers (e.g. OP_STORE_IDX, by-ref foreach) pass a pValue that points` |
|       - | 1429 | `	 * directly into pVm->aMemObj. PH7_HashmapDup below reserves a memory object` |
|       - | 1430 | `	 * per duplicated entry, which can grow — and therefore reallocate (move) —` |
|       - | 1431 | `	 * pVm->aMemObj, leaving such a pValue dangling. Capture its slot identity now,` |
|       - | 1432 | `	 * before the dup, so the write-back can re-resolve from the (stable) index` |
|       - | 1433 | `	 * rather than dereference the captured pointer (the same hazard handled for` |
|       - | 1434 | `	 * pBacking in the backing-variable branch above). */` |
|      41 | 1435 | `	nValIdx = pValue->nIdx;` |
|      61 | 1436 | `	bValueInPool = ( nValIdx != SXU32_HIGH` |
|      40 | 1437 | `		&& (ph7_value *)SySetAt(&pVm->aMemObj,nValIdx) == pValue );` |
|      41 | 1438 | `	pNew = PH7_NewHashmap(pVm,0,0);` |
|      41 | 1439 | `	if( pNew == 0 ){` |
|       - | 1440 | `		/* Allocation failure — fall through with shared map */` |
|     ! 0 | 1441 | `		return pMap;` |
|       - | 1442 | `	}` |
|      41 | 1443 | `	if( PH7_HashmapDup(pMap,pNew) != SXRET_OK ){` |
|       - | 1444 | `		/* Dup failed (OOM) — discard partial copy, keep original */` |
|     ! 0 | 1445 | `		PH7_HashmapRelease(pNew,TRUE);` |
|     ! 0 | 1446 | `		return pMap;` |
|       - | 1447 | `	}` |
|      41 | 1448 | `	pNew->iNextIdx = pMap->iNextIdx;` |
|      41 | 1449 | `	pMap->iRef--;` |
|      41 | 1450 | `	if( bValueInPool ){` |
|       - | 1451 | `		/* aMemObj may have moved during the dup — re-resolve pValue's slot. */` |
|      41 | 1452 | `		pValue = (ph7_value *)SySetAt(&pVm->aMemObj,nValIdx);` |
|      41 | 1453 | `		if( pValue == 0 ){` |
|     ! 0 | 1454 | `			return pNew;` |
|       - | 1455 | `		}` |
|      20 | 1456 | `	}` |
|      41 | 1457 | `	pValue->x.pOther = pNew;` |
|      41 | 1458 | `	return pNew;` |
|  107779 | 1459 | `}` |
|       - | 1460 | `/*` |
|       - | 1461 | ` * Perform the union of two hashmaps.` |
|       - | 1462 | ` * This operation is performed only if the user uses the '+' operator` |
|       - | 1463 | ` * with a variable holding an array as follows:` |
|       - | 1464 | ` * <?php` |
|       - | 1465 | ` * $a = array("a" => "apple", "b" => "banana");` |
|       - | 1466 | ` * $b = array("a" => "pear", "b" => "strawberry", "c" => "cherry");` |
|       - | 1467 | ` * $c = $a + $b; // Union of $a and $b` |
|       - | 1468 | ` * echo "Union of \$a and \$b: \n";` |
|       - | 1469 | ` * var_dump($c);` |
|       - | 1470 | ` * $c = $b + $a; // Union of $b and $a` |
|       - | 1471 | ` * echo "Union of \$b and \$a: \n";` |
|       - | 1472 | ` * var_dump($c);` |
|       - | 1473 | ` * ?>` |
|       - | 1474 | ` * When executed, this script will print the following:` |
|       - | 1475 | ` * Union of $a and $b:` |
|       - | 1476 | ` * array(3) {` |
|       - | 1477 | ` *  ["a"]=>` |
|       - | 1478 | ` *  string(5) "apple"` |
|       - | 1479 | ` *  ["b"]=>` |
|       - | 1480 | ` * string(6) "banana"` |
|       - | 1481 | ` *  ["c"]=>` |
|       - | 1482 | ` * string(6) "cherry"` |
|       - | 1483 | ` * }` |
|       - | 1484 | ` * Union of $b and $a:` |
|       - | 1485 | ` * array(3) {` |
|       - | 1486 | ` * ["a"]=>` |
|       - | 1487 | ` * string(4) "pear"` |
|       - | 1488 | ` * ["b"]=>` |
|       - | 1489 | ` * string(10) "strawberry"` |
|       - | 1490 | ` * ["c"]=>` |
|       - | 1491 | ` * string(6) "cherry"` |
|       - | 1492 | ` * }` |
|       - | 1493 | ` * The + operator returns the right-hand array appended to the left-hand array;` |
|       - | 1494 | ` * For keys that exist in both arrays, the elements from the left-hand array will be used` |
|       - | 1495 | ` * and the matching elements from the right-hand array will be ignored.` |
|       - | 1496 | ` */` |
|    3818 | 1497 | `PH7_PRIVATE sxi32 PH7_HashmapUnion(ph7_hashmap *pLeft,ph7_hashmap *pRight)` |
|       5 | 1498 | `{` |
|       - | 1499 | `	ph7_hashmap_node *pEntry;` |
|    3823 | 1500 | `	sxi32 rc = SXRET_OK;` |
|       - | 1501 | `	ph7_value *pObj;` |
|       - | 1502 | `	sxu32 n;` |
|    3823 | 1503 | `	if( pLeft == pRight ){` |
|       - | 1504 | `		/* Same map. This can easily happen since hashmaps are passed by reference.` |
|       - | 1505 | `		 * Unlike the zend engine.` |
|       - | 1506 | `		 */` |
|     ! 0 | 1507 | `		return SXRET_OK;` |
|       - | 1508 | `	}` |
|       - | 1509 | `	/* Perform the union */` |
|    3823 | 1510 | `	pEntry = pRight->pFirst;` |
|    3857 | 1511 | `	for(n = 0 ; n < pRight->nEntry ; ++n ){` |
|       - | 1512 | `		/* Make sure the given key does not exists in the left array */` |
|      38 | 1513 | `		if( pEntry->iType == HASHMAP_BLOB_NODE ){` |
|       - | 1514 | `			/* BLOB key */` |
|      24 | 1515 | `			if( SXRET_OK !=` |
|      20 | 1516 | `				HashmapLookupBlobKey(&(*pLeft),SyBlobData(&pEntry->xKey.sKey),SyBlobLength(&pEntry->xKey.sKey),0) ){` |
|      20 | 1517 | `					pObj = HashmapExtractNodeValue(pEntry);` |
|      20 | 1518 | `					if( pObj ){` |
|      20 | 1519 | `						ph7_value sSafeVal = *pObj;` |
|       - | 1520 | `						/* Perform the insertion */` |
|      20 | 1521 | `						rc = HashmapInsertBlobKey(&(*pLeft),SyBlobData(&pEntry->xKey.sKey),SyBlobLength(&pEntry->xKey.sKey),` |
|       - | 1522 | `							&sSafeVal,0,FALSE);` |
|      20 | 1523 | `						if( rc != SXRET_OK ){` |
|     ! 0 | 1524 | `							return rc;` |
|       - | 1525 | `						}` |
|       8 | 1526 | `					}` |
|       8 | 1527 | `			}` |
|      14 | 1528 | `		}else{` |
|       - | 1529 | `			/* INT key */` |
|      16 | 1530 | `			if( SXRET_OK != HashmapLookupIntKey(&(*pLeft),pEntry->xKey.iKey,0) ){` |
|      11 | 1531 | `				pObj = HashmapExtractNodeValue(pEntry);` |
|      11 | 1532 | `				if( pObj ){` |
|      11 | 1533 | `					ph7_value sSafeVal = *pObj;` |
|       - | 1534 | `					/* Perform the insertion */` |
|      11 | 1535 | `					rc = HashmapInsertIntKey(&(*pLeft),pEntry->xKey.iKey,&sSafeVal,0,FALSE);` |
|      11 | 1536 | `					if( rc != SXRET_OK ){` |
|     ! 0 | 1537 | `						return rc;` |
|       - | 1538 | `					}` |
|       5 | 1539 | `				}` |
|       5 | 1540 | `			}` |
|       - | 1541 | `		}` |
|       - | 1542 | `		/* Point to the next entry */` |
|      38 | 1543 | `		pEntry = pEntry->pPrev; /* Reverse link */` |
|      21 | 1544 | `	}` |
|    3823 | 1545 | `	return SXRET_OK;` |
|    1914 | 1546 | `}` |
|       - | 1547 | `/*` |
|       - | 1548 | ` * Allocate a new hashmap.` |
|       - | 1549 | ` * Return a pointer to the freshly allocated hashmap on success.NULL otherwise.` |
|       - | 1550 | ` */` |
|  115424 | 1551 | `PH7_PRIVATE ph7_hashmap * PH7_NewHashmap(` |
|       - | 1552 | `	ph7_vm *pVm,              /* VM that trigger the hashmap creation */` |
|       - | 1553 | `	sxu32 (*xIntHash)(sxi64), /* Hash function for int keys.NULL otherwise*/` |
|       - | 1554 | `	sxu32 (*xBlobHash)(const void *,sxu32) /* Hash function for BLOB keys.NULL otherwise */` |
|       - | 1555 | `	)` |
|       5 | 1556 | `{` |
|       - | 1557 | `	ph7_hashmap *pMap;` |
|       - | 1558 | `	/* Allocate a new instance */` |
|  115429 | 1559 | `	pMap = (ph7_hashmap *)SyMemBackendPoolAlloc(&pVm->sAllocator,sizeof(ph7_hashmap));` |
|  115429 | 1560 | `	if( pMap == 0 ){` |
|     ! 0 | 1561 | `		return 0;` |
|       - | 1562 | `	}` |
|       - | 1563 | `	/* Zero the structure */` |
|  115429 | 1564 | `	SyZero(pMap,sizeof(ph7_hashmap));` |
|       - | 1565 | `	/* Fill in the structure */` |
|  115429 | 1566 | `	pMap->pVm = &(*pVm);` |
|  115429 | 1567 | `	pMap->iRef = 1;` |
|       - | 1568 | `	/* Default hash functions */` |
|  115429 | 1569 | `	pMap->xIntHash  = xIntHash ? xIntHash : IntHash;` |
|  115429 | 1570 | `	pMap->xBlobHash = xBlobHash ? xBlobHash : BinHash;` |
|  115429 | 1571 | `	return pMap;` |
|   57717 | 1572 | `}` |
|       - | 1573 | `/*` |
|       - | 1574 | ` * Install superglobals in the given virtual machine.` |
|       - | 1575 | ` * Note on superglobals.` |
|       - | 1576 | ` *  According to the PHP language reference manual.` |
|       - | 1577 | ` *  Superglobals are built-in variables that are always available in all scopes.` |
|       - | 1578 | `*   Description` |
|       - | 1579 | `*   Several predefined variables in PHP are "superglobals", which means they` |
|       - | 1580 | `*   are available in all scopes throughout a script. There is no need to do` |
|       - | 1581 | `*   global $variable; to access them within functions or methods.` |
|       - | 1582 | `*   These superglobal variables are:` |
|       - | 1583 | `*    $GLOBALS` |
|       - | 1584 | `*    $_SERVER` |
|       - | 1585 | `*    $_GET` |
|       - | 1586 | `*    $_POST` |
|       - | 1587 | `*    $_FILES` |
|       - | 1588 | `*    $_COOKIE` |
|       - | 1589 | `*    $_SESSION` |
|       - | 1590 | `*    $_REQUEST` |
|       - | 1591 | `*    $_ENV` |
|       - | 1592 | `*/` |
|    3482 | 1593 | `PH7_PRIVATE sxi32 PH7_HashmapCreateSuper(ph7_vm *pVm)` |
|       5 | 1594 | `{` |
|       - | 1595 | `	static const char * azSuper[] = {` |
|       - | 1596 | `		"_SERVER",   /* $_SERVER */` |
|       - | 1597 | `		"_GET",      /* $_GET */` |
|       - | 1598 | `		"_POST",     /* $_POST */` |
|       - | 1599 | `		"_FILES",    /* $_FILES */` |
|       - | 1600 | `		"_COOKIE",   /* $_COOKIE */` |
|       - | 1601 | `		"_SESSION",  /* $_SESSION */` |
|       - | 1602 | `		"_REQUEST",  /* $_REQUEST */` |
|       - | 1603 | `		"_ENV",      /* $_ENV */` |
|       - | 1604 | `		"_HEADER",   /* $_HEADER */` |
|       - | 1605 | `		"argv"       /* $argv */` |
|       - | 1606 | `	};` |
|       - | 1607 | `	ph7_hashmap *pMap;` |
|       - | 1608 | `	ph7_value *pObj;` |
|       - | 1609 | `	SyString *pFile;` |
|       - | 1610 | `	sxi32 rc;` |
|       - | 1611 | `	sxu32 n;` |
|       - | 1612 | `	/* Allocate a new hashmap for the $GLOBALS array */` |
|    3487 | 1613 | `	pMap = PH7_NewHashmap(&(*pVm),0,0);` |
|    3487 | 1614 | `	if( pMap == 0 ){` |
|     ! 0 | 1615 | `		return SXERR_MEM;` |
|       - | 1616 | `	}` |
|    3487 | 1617 | `	pVm->pGlobal = pMap;` |
|       - | 1618 | `	/* Reserve a ph7_value for the $GLOBALS array*/` |
|    3487 | 1619 | `	pObj = PH7_ReserveMemObj(&(*pVm));` |
|    3487 | 1620 | `	if( pObj == 0 ){` |
|     ! 0 | 1621 | `		return SXERR_MEM;` |
|       - | 1622 | `	}` |
|    3487 | 1623 | `	PH7_MemObjInitFromArray(&(*pVm),pObj,pMap);` |
|       - | 1624 | `	/* Record object index */` |
|    3487 | 1625 | `	pVm->nGlobalIdx = pObj->nIdx;` |
|       - | 1626 | `	/* Install the special $GLOBALS array */` |
|    3487 | 1627 | `	rc = SyHashInsert(&pVm->hSuper,(const void *)"GLOBALS",sizeof("GLOBALS")-1,SX_INT_TO_PTR(pVm->nGlobalIdx));` |
|    3487 | 1628 | `	if( rc != SXRET_OK ){` |
|     ! 0 | 1629 | `		return rc;` |
|       - | 1630 | `	}` |
|       - | 1631 | `	/* Install superglobals now */` |
|   38307 | 1632 | `	for( n =  0 ; n < SX_ARRAYSIZE(azSuper)  ; n++ ){` |
|       - | 1633 | `		ph7_value *pSuper;` |
|       - | 1634 | `		/* Request an empty array */` |
|   34825 | 1635 | `		pSuper = ph7_new_array(&(*pVm));` |
|   34825 | 1636 | `		if( pSuper == 0 ){` |
|     ! 0 | 1637 | `			return SXERR_MEM;` |
|       - | 1638 | `		}` |
|       - | 1639 | `		/* Install */` |
|   34825 | 1640 | `		rc = ph7_vm_config(&(*pVm),PH7_VM_CONFIG_CREATE_SUPER,azSuper[n]/* Super-global name*/,pSuper/* Super-global value */);` |
|   34825 | 1641 | `		if( rc != SXRET_OK ){` |
|     ! 0 | 1642 | `			return rc;` |
|       - | 1643 | `		}` |
|       - | 1644 | `		/* Release the value now it have been installed */` |
|   34825 | 1645 | `		ph7_release_value(&(*pVm),pSuper);` |
|   17415 | 1646 | `	}` |
|       - | 1647 | `	/* Set some $_SERVER entries */` |
|    3487 | 1648 | `	pFile = (SyString *)SySetPeek(&pVm->aFiles);` |
|       - | 1649 | `	/*` |
|       - | 1650 | `	 * 'SCRIPT_FILENAME'` |
|       - | 1651 | `	 * The absolute pathname of the currently executing script.` |
|       - | 1652 | `	 */` |
|    6965 | 1653 | `	ph7_vm_config(pVm,PH7_VM_CONFIG_SERVER_ATTR,` |
|       - | 1654 | `		"SCRIPT_FILENAME",` |
|    1741 | 1655 | `		pFile ? pFile->zString : ":Memory:",` |
|    3478 | 1656 | `		pFile ? pFile->nByte : sizeof(":Memory:") - 1` |
|       - | 1657 | `		);` |
|       - | 1658 | `	/* All done,all super-global are installed now */` |
|    3487 | 1659 | `	return SXRET_OK;` |
|    1746 | 1660 | `}` |
|       - | 1661 | `/*` |
|       - | 1662 | ` * Release a hashmap.` |
|       - | 1663 | ` */` |
|   72658 | 1664 | `PH7_PRIVATE sxi32 PH7_HashmapRelease(ph7_hashmap *pMap,int FreeDS)` |
|       5 | 1665 | `{` |
|       - | 1666 | `	ph7_hashmap_node *pEntry,*pNext;` |
|   72663 | 1667 | `	ph7_vm *pVm = pMap->pVm;` |
|       - | 1668 | `	sxu32 n;` |
|   72663 | 1669 | `	if( pMap == pVm->pGlobal ){` |
|       - | 1670 | `		/* Cannot delete the $GLOBALS array */` |
|     ! 0 | 1671 | `		PH7_VmThrowError(pMap->pVm,0,PH7_CTX_NOTICE,"$GLOBALS is a read-only array,deletion is forbidden");` |
|     ! 0 | 1672 | `		return SXRET_OK;` |
|       - | 1673 | `	}` |
|       - | 1674 | `	/* Start the release process */` |
|   72663 | 1675 | `	n = 0;` |
|   72663 | 1676 | `	pEntry = pMap->pFirst;` |
| 1608046 | 1677 | `	for(;;){` |
| 3216097 | 1678 | `		if( n >= pMap->nEntry ){` |
|   72663 | 1679 | `			break;` |
|       - | 1680 | `		}` |
| 3143439 | 1681 | `		pNext = pEntry->pPrev; /* Reverse link */` |
|       - | 1682 | `		/* Remove the reference from the foreign table */` |
| 3143439 | 1683 | `		PH7_VmRefObjRemove(pVm,pEntry->nValIdx,0,pEntry);` |
| 3143439 | 1684 | `		if( (pEntry->iFlags & HASHMAP_NODE_FOREIGN_OBJ) == 0 ){` |
|       - | 1685 | `			/* Restore the ph7_value to the free list */` |
| 3143429 | 1686 | `			PH7_VmUnsetMemObj(pVm,pEntry->nValIdx,FALSE);` |
| 1571712 | 1687 | `		}` |
|       - | 1688 | `		/* Release the node */` |
| 3143439 | 1689 | `		if( pEntry->iType == HASHMAP_BLOB_NODE ){` |
|   80377 | 1690 | `			SyBlobRelease(&pEntry->xKey.sKey);` |
|   40186 | 1691 | `		}` |
| 3143439 | 1692 | `		SyMemBackendPoolFree(&pVm->sAllocator,pEntry);` |
|       - | 1693 | `		/* Point to the next entry */` |
| 3143439 | 1694 | `		pEntry = pNext;` |
| 3143439 | 1695 | `		n++;` |
|       5 | 1696 | `	}` |
|   72663 | 1697 | `	if( pMap->nEntry > 0 ){` |
|       - | 1698 | `		/* Release the hash bucket */` |
|   59307 | 1699 | `		SyMemBackendFree(&pVm->sAllocator,pMap->apBucket);` |
|   29651 | 1700 | `	}` |
|   72663 | 1701 | `	if( FreeDS ){` |
|       - | 1702 | `		/* Free the whole instance */` |
|   72647 | 1703 | `		SyMemBackendPoolFree(&pVm->sAllocator,pMap);` |
|   36326 | 1704 | `	}else{` |
|       - | 1705 | `		/* Keep the instance but reset it's fields */` |
|      17 | 1706 | `		pMap->apBucket = 0;` |
|      17 | 1707 | `		pMap->iNextIdx = 0;` |
|      17 | 1708 | `		pMap->nEntry = pMap->nSize = 0;` |
|      17 | 1709 | `		pMap->pFirst = pMap->pLast = pMap->pCur = 0;` |
|       - | 1710 | `	}` |
|   72663 | 1711 | `	return SXRET_OK;` |
|   36334 | 1712 | `}` |
|       - | 1713 | `/*` |
|       - | 1714 | ` * Decrement the reference count of a given hashmap.` |
|       - | 1715 | ` * If the count reaches zero which mean no more variables` |
|       - | 1716 | ` * are pointing to this hashmap,then release the whole instance.` |
|       - | 1717 | ` */` |
|  727826 | 1718 | `PH7_PRIVATE void  PH7_HashmapUnref(ph7_hashmap *pMap)` |
|       5 | 1719 | `{` |
|  727831 | 1720 | `	ph7_vm *pVm = pMap->pVm;` |
|       - | 1721 | `	/* TICKET 1432-49: $GLOBALS is not subject to garbage collection */` |
|  727831 | 1722 | `	pMap->iRef--;` |
|  727831 | 1723 | `	if( pMap->iRef < 1 && pMap != pVm->pGlobal){` |
|   72627 | 1724 | `		PH7_HashmapRelease(pMap,TRUE);` |
|   36311 | 1725 | `	}` |
|  727831 | 1726 | `}` |
|       - | 1727 | `/*` |
|       - | 1728 | ` * Check if a given key exists in the given hashmap.` |
|       - | 1729 | ` * Write a pointer to the target node on success.` |
|       - | 1730 | ` * Otherwise SXERR_NOTFOUND is returned on failure.` |
|       - | 1731 | ` */` |
|  126508 | 1732 | `PH7_PRIVATE sxi32 PH7_HashmapLookup(` |
|       - | 1733 | `	ph7_hashmap *pMap,        /* Target hashmap */` |
|       - | 1734 | `	ph7_value *pKey,          /* Lookup key */` |
|       - | 1735 | `	ph7_hashmap_node **ppNode /* OUT: Target node on success */` |
|       - | 1736 | `	)` |
|       5 | 1737 | `{` |
|       - | 1738 | `	sxi32 rc;` |
|  126513 | 1739 | `	if( pMap->nEntry < 1 ){` |
|       - | 1740 | `		/* TICKET 1433-25: Don't bother hashing,the hashmap is empty anyway.` |
|       - | 1741 | `		 */` |
|      64 | 1742 | `		return SXERR_NOTFOUND;` |
|       - | 1743 | `	}` |
|  126453 | 1744 | `	rc = HashmapLookup(&(*pMap),&(*pKey),ppNode);` |
|  126453 | 1745 | `	return rc;` |
|   63259 | 1746 | `}` |
|       - | 1747 | `/*` |
|       - | 1748 | ` * Insert a given key and it's associated value (if any) in the given` |
|       - | 1749 | ` * hashmap.` |
|       - | 1750 | ` * If a node with the given key already exists in the database` |
|       - | 1751 | ` * then this function overwrite the old value.` |
|       - | 1752 | ` */` |
| 2563760 | 1753 | `PH7_PRIVATE sxi32 PH7_HashmapInsert(` |
|       - | 1754 | `	ph7_hashmap *pMap, /* Target hashmap */` |
|       - | 1755 | `	ph7_value *pKey,   /* Lookup key */` |
|       - | 1756 | `	ph7_value *pVal    /* Node value.NULL otherwise */` |
|       - | 1757 | `	)` |
|       5 | 1758 | `{` |
|       - | 1759 | `	sxi32 rc;` |
|       - | 1760 | `	/* Storing the $GLOBALS array itself as a VALUE is fine in php ($a[] =` |
|       - | 1761 | `	 * $GLOBALS copies the symbol table); the old TICKET 1433-35 guard that` |
|       - | 1762 | `	 * forbade it was a PH7-ism. Writes INTO $GLOBALS are handled inside` |
|       - | 1763 | `	 * HashmapInsert (they create real global variables, php 8.1). */` |
| 2563765 | 1764 | `	rc = HashmapInsert(&(*pMap),&(*pKey),&(*pVal));` |
| 2563765 | 1765 | `	return rc;` |
|       5 | 1766 | `}` |
|       - | 1767 | `/*` |
|       - | 1768 | ` * Merge entries of pSrc into pDest using PHP merge semantics:` |
|       - | 1769 | ` *   - String keys overwrite same-key entries in pDest.` |
|       - | 1770 | ` *   - Integer keys are renumbered with the destination's auto-index.` |
|       - | 1771 | ` * This is the same routine that backs array_merge().` |
|       - | 1772 | ` */` |
|      52 | 1773 | `PH7_PRIVATE sxi32 PH7_HashmapMerge(ph7_hashmap *pSrc,ph7_hashmap *pDest)` |
|       1 | 1774 | `{` |
|      53 | 1775 | `	return HashmapMerge(&(*pSrc),&(*pDest));` |
|       1 | 1776 | `}` |
|       - | 1777 | `/*` |
|       - | 1778 | ` * Insert a given key and it's associated value (foreign index) in the given` |
|       - | 1779 | ` * hashmap.` |
|       - | 1780 | ` * This is insertion by reference so be careful to mark the node` |
|       - | 1781 | ` * with the HASHMAP_NODE_FOREIGN_OBJ flag being set.` |
|       - | 1782 | ` * The insertion by reference is triggered when the following` |
|       - | 1783 | ` * expression is encountered.` |
|       - | 1784 | ` * $var = 10;` |
|       - | 1785 | ` *  $a = array(&var);` |
|       - | 1786 | ` * OR` |
|       - | 1787 | ` *  $a[] =& $var;` |
|       - | 1788 | ` * That is,$var is a foreign ph7_value and the $a array have no control` |
|       - | 1789 | ` * over it's contents.` |
|       - | 1790 | ` * Note that the node that hold the foreign ph7_value is automatically` |
|       - | 1791 | ` * removed when the foreign ph7_value is unset.` |
|       - | 1792 | ` * Example:` |
|       - | 1793 | ` *  $var = 10;` |
|       - | 1794 | ` *  $a[] =& $var;` |
|       - | 1795 | ` *  echo count($a).PHP_EOL; //1` |
|       - | 1796 | ` *  //Unset the foreign ph7_value now` |
|       - | 1797 | ` *  unset($var);` |
|       - | 1798 | ` *  echo count($a); //0` |
|       - | 1799 | ` * Note that this is a PH7 eXtension.` |
|       - | 1800 | ` * Refer to the official documentation for more information.` |
|       - | 1801 | ` * If a node with the given key already exists in the database` |
|       - | 1802 | ` * then this function overwrite the old value.` |
|       - | 1803 | ` */` |
|   46338 | 1804 | `PH7_PRIVATE sxi32 PH7_HashmapInsertByRef(` |
|       - | 1805 | `	ph7_hashmap *pMap, /* Target hashmap */` |
|       - | 1806 | `	ph7_value *pKey,   /* Lookup key */` |
|       - | 1807 | `	sxu32 nRefIdx      /* Foreign ph7_value index */` |
|       - | 1808 | `	)` |
|       5 | 1809 | `{` |
|       - | 1810 | `	sxi32 rc;` |
|   46343 | 1811 | `	if( nRefIdx == pMap->pVm->nGlobalIdx ){` |
|       - | 1812 | `		/* php's non-catchable fatal: $a[] =& $GLOBALS is forbidden (8.1) */` |
|     ! 0 | 1813 | `		PH7_VmThrowError(pMap->pVm,0,PH7_CTX_ERR,"Cannot acquire reference to $GLOBALS");` |
|     ! 0 | 1814 | `		pMap->pVm->iExitStatus = 255;` |
|     ! 0 | 1815 | `		pMap->pVm->bHaltRequested = 1;` |
|     ! 0 | 1816 | `		return PH7_ABORT;` |
|       - | 1817 | `	}` |
|   46343 | 1818 | `	rc = HashmapInsertByRef(&(*pMap),&(*pKey),nRefIdx);` |
|   46343 | 1819 | `	return rc;` |
|   23174 | 1820 | `}` |
|       - | 1821 | `/*` |
|       - | 1822 | ` * Reset the node cursor of a given hashmap.` |
|       - | 1823 | ` */` |
|   35138 | 1824 | `PH7_PRIVATE void PH7_HashmapResetLoopCursor(ph7_hashmap *pMap)` |
|       5 | 1825 | `{` |
|       - | 1826 | `	/* Reset the loop cursor */` |
|   35143 | 1827 | `	pMap->pCur = pMap->pFirst;` |
|   35143 | 1828 | `}` |
|       - | 1829 | `/*` |
|       - | 1830 | ` * Return a pointer to the node currently pointed by the node cursor.` |
|       - | 1831 | ` * If the cursor reaches the end of the list,then this function` |
|       - | 1832 | ` * return NULL.` |
|       - | 1833 | ` * Note that the node cursor is automatically advanced by this function.` |
|       - | 1834 | ` */` |
|  231770 | 1835 | `PH7_PRIVATE ph7_hashmap_node * PH7_HashmapGetNextEntry(ph7_hashmap *pMap)` |
|       5 | 1836 | `{` |
|  231775 | 1837 | `	ph7_hashmap_node *pCur = pMap->pCur;` |
|  231775 | 1838 | `	if( pCur == 0 ){` |
|       - | 1839 | `		/* End of the list,return null */` |
|   17593 | 1840 | `		return 0;` |
|       - | 1841 | `	}` |
|       - | 1842 | `	/* Advance the node cursor */` |
|  214187 | 1843 | `	pMap->pCur = pCur->pPrev; /* Reverse link */` |
|  214187 | 1844 | `	return pCur;` |
|  115890 | 1845 | `}` |
|       - | 1846 | `/*` |
|       - | 1847 | ` * Extract a node value.` |
|       - | 1848 | ` */` |
|  540926 | 1849 | `PH7_PRIVATE void PH7_HashmapExtractNodeValue(ph7_hashmap_node *pNode,ph7_value *pValue,int bStore)` |
|       5 | 1850 | `{` |
|  540931 | 1851 | `	ph7_value *pEntry = HashmapExtractNodeValue(pNode);` |
|  540931 | 1852 | `	if( pEntry ){` |
|  540931 | 1853 | `		if( bStore ){` |
|  214567 | 1854 | `			PH7_MemObjStore(pEntry,pValue);` |
|  107286 | 1855 | `		}else{` |
|  326369 | 1856 | `			PH7_MemObjLoad(pEntry,pValue);` |
|       - | 1857 | `		}` |
|  270502 | 1858 | `	}else{` |
|     ! 0 | 1859 | `		PH7_MemObjRelease(pValue);` |
|       - | 1860 | `	}` |
|  540931 | 1861 | `}` |
|       - | 1862 | `/*` |
|       - | 1863 | ` * Extract a node key.` |
|       - | 1864 | ` */` |
|  140160 | 1865 | `PH7_PRIVATE void PH7_HashmapExtractNodeKey(ph7_hashmap_node *pNode,ph7_value *pKey)` |
|       5 | 1866 | `{` |
|       - | 1867 | `	/* Fill with the current key */` |
|  140165 | 1868 | `	if( pNode->iType == HASHMAP_INT_NODE ){` |
|  135615 | 1869 | `		if( SyBlobLength(&pKey->sBlob) > 0 ){` |
|      27 | 1870 | `			SyBlobRelease(&pKey->sBlob);` |
|      13 | 1871 | `		}` |
|  135615 | 1872 | `		pKey->x.iVal = pNode->xKey.iKey;` |
|  135615 | 1873 | `		MemObjSetType(pKey,MEMOBJ_INT);` |
|   67810 | 1874 | `	}else{` |
|    4555 | 1875 | `		SyBlobReset(&pKey->sBlob);` |
|    4555 | 1876 | `		SyBlobAppend(&pKey->sBlob,SyBlobData(&pNode->xKey.sKey),SyBlobLength(&pNode->xKey.sKey));` |
|    4555 | 1877 | `		MemObjSetType(pKey,MEMOBJ_STRING);` |
|       - | 1878 | `	}` |
|  140165 | 1879 | `}` |
|       - | 1880 | `#ifndef PH7_DISABLE_DISK_IO` |
|       - | 1881 | `/*` |
|       - | 1882 | ` * Store the address of nodes value in the given container.` |
|       - | 1883 | ` * Refer to the [vfprintf(),vprintf(),vsprintf()] implementations` |
|       - | 1884 | ` * defined in 'builtin.c' for more information.` |
|       - | 1885 | ` */` |
|      10 | 1886 | `PH7_PRIVATE int PH7_HashmapValuesToSet(ph7_hashmap *pMap,SySet *pOut)` |
|       1 | 1887 | `{` |
|      11 | 1888 | `	ph7_hashmap_node *pEntry = pMap->pFirst;` |
|       - | 1889 | `	ph7_value *pValue;` |
|       - | 1890 | `	sxu32 n;` |
|       - | 1891 | `	/* Initialize the container */` |
|      11 | 1892 | `	SySetInit(pOut,&pMap->pVm->sAllocator,sizeof(ph7_value *));` |
|      27 | 1893 | `	for(n = 0 ; n < pMap->nEntry ; n++ ){` |
|       - | 1894 | `		/* Extract node value */` |
|      17 | 1895 | `		pValue = HashmapExtractNodeValue(pEntry);` |
|      17 | 1896 | `		if( pValue ){` |
|      17 | 1897 | `			SySetPut(pOut,(const void *)&pValue);` |
|       8 | 1898 | `		}` |
|       - | 1899 | `		/* Point to the next entry */` |
|      17 | 1900 | `		pEntry = pEntry->pPrev; /* Reverse link */` |
|       9 | 1901 | `	}` |
|       - | 1902 | `	/* Total inserted entries */` |
|      11 | 1903 | `	return (int)SySetUsed(pOut);` |
|       1 | 1904 | `}` |
|       - | 1905 | `#endif /* PH7_DISABLE_BUILTIN_FUNC \|\| PH7_DISABLE_DISK_IO */` |
|       - | 1906 | `/* SPDX-SnippetBegin */` |
|       - | 1907 | `/* SPDX-SnippetCopyrightText: D. Richard Hipp and the SQLite authors <https://sqlite.org/> */` |
|       - | 1908 | `/* SPDX-License-Identifier: blessing */` |
|       - | 1909 | `/*` |
|       - | 1910 | ` * Merge sort.` |
|       - | 1911 | ` * The merge sort implementation is based on the one found in the SQLite3 source tree.` |
|       - | 1912 | ` * Status: Public domain` |
|       - | 1913 | ` */` |
|       - | 1914 | `/* Node comparison callback signature */` |
|       - | 1915 | `typedef sxi32 (*ProcNodeCmp)(ph7_hashmap_node *,ph7_hashmap_node *,void *);` |
|       - | 1916 | `/*` |
|       - | 1917 | `** Inputs:` |
|       - | 1918 | `**   a:       A sorted, null-terminated linked list.  (May be null).` |
|       - | 1919 | `**   b:       A sorted, null-terminated linked list.  (May be null).` |
|       - | 1920 | `**   cmp:     A pointer to the comparison function.` |
|       - | 1921 | `**` |
|       - | 1922 | `** Return Value:` |
|       - | 1923 | `**   A pointer to the head of a sorted list containing the elements` |
|       - | 1924 | `**   of both a and b.` |
|       - | 1925 | `**` |
|       - | 1926 | `** Side effects:` |
|       - | 1927 | `**   The "next","prev" pointers for elements in the lists a and b are` |
|       - | 1928 | `**   changed.` |
|       - | 1929 | `*/` |
|   33428 | 1930 | `static ph7_hashmap_node * HashmapNodeMerge(ph7_hashmap_node *pA,ph7_hashmap_node *pB,ProcNodeCmp xCmp,void *pCmpData)` |
|       5 | 1931 | `{` |
|       - | 1932 | `	ph7_hashmap_node result,*pTail;` |
|       - | 1933 | `    /* Prevent compiler warning */` |
|   33433 | 1934 | `	result.pNext = result.pPrev = 0;` |
|   33433 | 1935 | `	pTail = &result;` |
|  102412 | 1936 | `	while( pA && pB ){` |
|   68984 | 1937 | `		if( xCmp(pA,pB,pCmpData) <= 0 ){` |
|   45673 | 1938 | `			pTail->pPrev = pA;` |
|   45673 | 1939 | `			pA->pNext = pTail;` |
|   45673 | 1940 | `			pTail = pA;` |
|   45673 | 1941 | `			pA = pA->pPrev;` |
|   22814 | 1942 | `		}else{` |
|   23316 | 1943 | `			pTail->pPrev = pB;` |
|   23316 | 1944 | `			pB->pNext = pTail;` |
|   23316 | 1945 | `			pTail = pB;` |
|   23316 | 1946 | `			pB = pB->pPrev;` |
|       - | 1947 | `		}` |
|       5 | 1948 | `	}` |
|   33433 | 1949 | `	if( pA ){` |
|   23365 | 1950 | `		pTail->pPrev = pA;` |
|   23365 | 1951 | `		pA->pNext = pTail;` |
|   21779 | 1952 | `	}else if( pB ){` |
|    9843 | 1953 | `		pTail->pPrev = pB;` |
|    9843 | 1954 | `		pB->pNext = pTail;` |
|    4898 | 1955 | `	}else{` |
|     235 | 1956 | `		pTail->pPrev = pTail->pNext = 0;` |
|       - | 1957 | `	}` |
|   33433 | 1958 | `	return result.pPrev;` |
|       5 | 1959 | `}` |
|       - | 1960 | `/*` |
|       - | 1961 | `** Inputs:` |
|       - | 1962 | `**   Map:       Input hashmap` |
|       - | 1963 | `**   cmp:       A comparison function.` |
|       - | 1964 | `**` |
|       - | 1965 | `** Return Value:` |
|       - | 1966 | `**   Sorted hashmap.` |
|       - | 1967 | `**` |
|       - | 1968 | `** Side effects:` |
|       - | 1969 | `**   The "next" pointers for elements in list are changed.` |
|       - | 1970 | `*/` |
|       - | 1971 | `#define N_SORT_BUCKET  32` |
|     688 | 1972 | `static sxi32 HashmapMergeSort(ph7_hashmap *pMap,ProcNodeCmp xCmp,void *pCmpData)` |
|       5 | 1973 | `{` |
|       - | 1974 | `	ph7_hashmap_node *a[N_SORT_BUCKET], *p,*pIn;` |
|       - | 1975 | `	sxu32 i;` |
|     693 | 1976 | `	SyZero(a,sizeof(a));` |
|       - | 1977 | `	/* Point to the first inserted entry */` |
|     693 | 1978 | `	pIn = pMap->pFirst;` |
|   14001 | 1979 | `	while( pIn ){` |
|   13313 | 1980 | `		p = pIn;` |
|   13313 | 1981 | `		pIn = p->pPrev;` |
|   13313 | 1982 | `		p->pPrev = 0;` |
|   25413 | 1983 | `		for(i=0; i<N_SORT_BUCKET-1; i++){` |
|   25413 | 1984 | `			if( a[i]==0 ){` |
|   13313 | 1985 | `				a[i] = p;` |
|   13313 | 1986 | `				break;` |
|     ! 0 | 1987 | `			}else{` |
|   12105 | 1988 | `				p = HashmapNodeMerge(a[i],p,xCmp,pCmpData);` |
|   12105 | 1989 | `				a[i] = 0;` |
|       - | 1990 | `			}` |
|    6055 | 1991 | `		}` |
|   13313 | 1992 | `		if( i==N_SORT_BUCKET-1 ){` |
|       - | 1993 | `			/* To get here, there need to be 2^(N_SORT_BUCKET) elements in he input list.` |
|       - | 1994 | `			 * But that is impossible.` |
|       - | 1995 | `			 */` |
|     ! 0 | 1996 | `			a[i] = HashmapNodeMerge(a[i], p,xCmp,pCmpData);` |
|     ! 0 | 1997 | `		}` |
|       5 | 1998 | `	}` |
|     693 | 1999 | `	p = a[0];` |
|   22021 | 2000 | `	for(i=1; i<N_SORT_BUCKET; i++){` |
|   21333 | 2001 | `		p = HashmapNodeMerge(p,a[i],xCmp,pCmpData);` |
|   10669 | 2002 | `	}` |
|     693 | 2003 | `	p->pNext = 0;` |
|       - | 2004 | `	/* Reflect the change */` |
|     693 | 2005 | `	pMap->pFirst = p;` |
|       - | 2006 | `	/* Reset the loop cursor */` |
|     693 | 2007 | `	pMap->pCur = pMap->pFirst;` |
|     693 | 2008 | `	return SXRET_OK;` |
|       5 | 2009 | `}` |
|       - | 2010 | `/* SPDX-SnippetEnd */` |
|       - | 2011 | `/*` |
|       - | 2012 | ` * Node comparison callback.` |
|       - | 2013 | ` * used-by: [sort(),asort(),...]` |
|       - | 2014 | ` */` |
|   68774 | 2015 | `static sxi32 HashmapCmpCallback1(ph7_hashmap_node *pA,ph7_hashmap_node *pB,void *pCmpData)` |
|       5 | 2016 | `{` |
|       - | 2017 | `	ph7_value sA,sB;` |
|       - | 2018 | `	sxi32 iFlags;` |
|       - | 2019 | `	int rc;` |
|   68779 | 2020 | `	if( pCmpData == 0 ){` |
|       - | 2021 | `		/* Perform a standard comparison */` |
|   68755 | 2022 | `		rc = HashmapNodeCmp(pA,pB,FALSE);` |
|   68755 | 2023 | `		return rc;` |
|       - | 2024 | `	}` |
|      25 | 2025 | `	iFlags = SX_PTR_TO_INT(pCmpData);` |
|       - | 2026 | `	/* Duplicate node values */` |
|      25 | 2027 | `	PH7_MemObjInit(pA->pMap->pVm,&sA);` |
|      25 | 2028 | `	PH7_MemObjInit(pA->pMap->pVm,&sB);` |
|      25 | 2029 | `	PH7_HashmapExtractNodeValue(pA,&sA,FALSE);` |
|      25 | 2030 | `	PH7_HashmapExtractNodeValue(pB,&sB,FALSE);` |
|      25 | 2031 | `	if( iFlags == 5 ){` |
|       - | 2032 | `		/* String cast */` |
|       - | 2033 | `		const char *zA,*zB;` |
|       - | 2034 | `		sxu32 nA,nB,nMin;` |
|      15 | 2035 | `		if( (sA.iFlags & MEMOBJ_STRING) == 0 ){` |
|     ! 0 | 2036 | `			PH7_MemObjToString(&sA);` |
|     ! 0 | 2037 | `		}` |
|      15 | 2038 | `		if( (sB.iFlags & MEMOBJ_STRING) == 0 ){` |
|     ! 0 | 2039 | `			PH7_MemObjToString(&sB);` |
|     ! 0 | 2040 | `		}` |
|       - | 2041 | `		/* Lexicographic string comparison to avoid numeric string coercion */` |
|      15 | 2042 | `		zA = (const char *)SyBlobData(&sA.sBlob);` |
|      15 | 2043 | `		zB = (const char *)SyBlobData(&sB.sBlob);` |
|      15 | 2044 | `		nA = SyBlobLength(&sA.sBlob);` |
|      15 | 2045 | `		nB = SyBlobLength(&sB.sBlob);` |
|      15 | 2046 | `		nMin = nA < nB ? nA : nB;` |
|      15 | 2047 | `		rc = SyMemcmp(zA,zB,nMin);` |
|      15 | 2048 | `		if( rc == 0 ){` |
|       5 | 2049 | `			if( nA < nB ) rc = -1;` |
|     ! 0 | 2050 | `			else if( nA > nB ) rc = 1;` |
|       2 | 2051 | `		}` |
|       8 | 2052 | `	}else{` |
|       - | 2053 | `		/* Numeric cast */` |
|      11 | 2054 | `		PH7_MemObjToNumeric(&sA);` |
|      11 | 2055 | `		PH7_MemObjToNumeric(&sB);` |
|      11 | 2056 | `		rc = PH7_MemObjCmp(&sA,&sB,FALSE,0);` |
|       - | 2057 | `	}` |
|      25 | 2058 | `	PH7_MemObjRelease(&sA);` |
|      25 | 2059 | `	PH7_MemObjRelease(&sB);` |
|      25 | 2060 | `	return rc;` |
|   34409 | 2061 | `}` |
|       - | 2062 | `/*` |
|       - | 2063 | ` * Node comparison callback: Compare nodes by keys only.` |
|       - | 2064 | ` * used-by: [ksort()]` |
|       - | 2065 | ` */` |
|      14 | 2066 | `static sxi32 HashmapCmpCallback2(ph7_hashmap_node *pA,ph7_hashmap_node *pB,void *pCmpData)` |
|       1 | 2067 | `{` |
|       - | 2068 | `	sxi32 rc;` |
|       7 | 2069 | `	SXUNUSED(pCmpData); /* cc warning */` |
|      15 | 2070 | `	if( pA->iType == HASHMAP_BLOB_NODE && pB->iType == HASHMAP_BLOB_NODE ){` |
|       - | 2071 | `		/* Perform a string comparison */` |
|       5 | 2072 | `		rc = SyBlobCmp(&pA->xKey.sKey,&pB->xKey.sKey);` |
|       3 | 2073 | `	}else{` |
|       - | 2074 | `		SyString sStr;` |
|       - | 2075 | `		sxi64 iA,iB;` |
|       - | 2076 | `		/* Perform a numeric comparison */` |
|      11 | 2077 | `		if( pA->iType == HASHMAP_BLOB_NODE ){` |
|       - | 2078 | `			/* Cast to 64-bit integer */` |
|     ! 0 | 2079 | `			SyStringInitFromBuf(&sStr,SyBlobData(&pA->xKey.sKey),SyBlobLength(&pA->xKey.sKey));` |
|     ! 0 | 2080 | `			if( sStr.nByte < 1 ){` |
|     ! 0 | 2081 | `				iA = 0;` |
|     ! 0 | 2082 | `			}else{` |
|     ! 0 | 2083 | `				SyStrToInt64(sStr.zString,sStr.nByte,(void *)&iA,0);` |
|       - | 2084 | `			}` |
|     ! 0 | 2085 | `		}else{` |
|      11 | 2086 | `			iA = pA->xKey.iKey;` |
|       - | 2087 | `		}` |
|      11 | 2088 | `		if( pB->iType == HASHMAP_BLOB_NODE ){` |
|       - | 2089 | `			/* Cast to 64-bit integer */` |
|     ! 0 | 2090 | `			SyStringInitFromBuf(&sStr,SyBlobData(&pB->xKey.sKey),SyBlobLength(&pB->xKey.sKey));` |
|     ! 0 | 2091 | `			if( sStr.nByte < 1 ){` |
|     ! 0 | 2092 | `				iB = 0;` |
|     ! 0 | 2093 | `			}else{` |
|     ! 0 | 2094 | `				SyStrToInt64(sStr.zString,sStr.nByte,(void *)&iB,0);` |
|       - | 2095 | `			}` |
|     ! 0 | 2096 | `		}else{` |
|      11 | 2097 | `			iB = pB->xKey.iKey;` |
|       - | 2098 | `		}` |
|      11 | 2099 | `		rc = (sxi32)(iA-iB);` |
|       - | 2100 | `	}` |
|       - | 2101 | `	/* Comparison result */` |
|      15 | 2102 | `	return rc;` |
|       1 | 2103 | `}` |
|       - | 2104 | `/*` |
|       - | 2105 | ` * Node comparison callback.` |
|       - | 2106 | ` * Used by: [rsort(),arsort()];` |
|       - | 2107 | ` */` |
|      78 | 2108 | `static sxi32 HashmapCmpCallback3(ph7_hashmap_node *pA,ph7_hashmap_node *pB,void *pCmpData)` |
|       1 | 2109 | `{` |
|       - | 2110 | `	ph7_value sA,sB;` |
|       - | 2111 | `	sxi32 iFlags;` |
|       - | 2112 | `	int rc;` |
|      79 | 2113 | `	if( pCmpData == 0 ){` |
|       - | 2114 | `		/* Perform a standard comparison */` |
|      59 | 2115 | `		rc = HashmapNodeCmp(pA,pB,FALSE);` |
|      59 | 2116 | `		return -rc;` |
|       - | 2117 | `	}` |
|      21 | 2118 | `	iFlags = SX_PTR_TO_INT(pCmpData);` |
|       - | 2119 | `	/* Duplicate node values */` |
|      21 | 2120 | `	PH7_MemObjInit(pA->pMap->pVm,&sA);` |
|      21 | 2121 | `	PH7_MemObjInit(pA->pMap->pVm,&sB);` |
|      21 | 2122 | `	PH7_HashmapExtractNodeValue(pA,&sA,FALSE);` |
|      21 | 2123 | `	PH7_HashmapExtractNodeValue(pB,&sB,FALSE);` |
|      21 | 2124 | `	if( iFlags == 5 ){` |
|       - | 2125 | `		/* String cast */` |
|       - | 2126 | `		const char *zA,*zB;` |
|       - | 2127 | `		sxu32 nA,nB,nMin;` |
|      11 | 2128 | `		if( (sA.iFlags & MEMOBJ_STRING) == 0 ){` |
|     ! 0 | 2129 | `			PH7_MemObjToString(&sA);` |
|     ! 0 | 2130 | `		}` |
|      11 | 2131 | `		if( (sB.iFlags & MEMOBJ_STRING) == 0 ){` |
|     ! 0 | 2132 | `			PH7_MemObjToString(&sB);` |
|     ! 0 | 2133 | `		}` |
|       - | 2134 | `		/* Lexicographic string comparison to avoid numeric string coercion */` |
|      11 | 2135 | `		zA = (const char *)SyBlobData(&sA.sBlob);` |
|      11 | 2136 | `		zB = (const char *)SyBlobData(&sB.sBlob);` |
|      11 | 2137 | `		nA = SyBlobLength(&sA.sBlob);` |
|      11 | 2138 | `		nB = SyBlobLength(&sB.sBlob);` |
|      11 | 2139 | `		nMin = nA < nB ? nA : nB;` |
|      11 | 2140 | `		rc = SyMemcmp(zA,zB,nMin);` |
|      11 | 2141 | `		if( rc == 0 ){` |
|       3 | 2142 | `			if( nA < nB ) rc = -1;` |
|     ! 0 | 2143 | `			else if( nA > nB ) rc = 1;` |
|       1 | 2144 | `		}` |
|       6 | 2145 | `	}else{` |
|       - | 2146 | `		/* Numeric cast */` |
|      11 | 2147 | `		PH7_MemObjToNumeric(&sA);` |
|      11 | 2148 | `		PH7_MemObjToNumeric(&sB);` |
|      11 | 2149 | `		rc = PH7_MemObjCmp(&sA,&sB,FALSE,0);` |
|       - | 2150 | `	}` |
|      21 | 2151 | `	PH7_MemObjRelease(&sA);` |
|      21 | 2152 | `	PH7_MemObjRelease(&sB);` |
|      21 | 2153 | `	return -rc;` |
|      40 | 2154 | `}` |
|       - | 2155 | `/*` |
|       - | 2156 | ` * Node comparison callback: Invoke an user-defined callback for the purpose of node comparison.` |
|       - | 2157 | ` * used-by: [usort(),uasort()]` |
|       - | 2158 | ` */` |
|      88 | 2159 | `static sxi32 HashmapCmpCallback4(ph7_hashmap_node *pA,ph7_hashmap_node *pB,void *pCmpData)` |
|       3 | 2160 | `{` |
|       - | 2161 | `	ph7_value sResult,*pCallback;` |
|       - | 2162 | `	ph7_value *pV1,*pV2;` |
|       - | 2163 | `	ph7_value *apArg[2];  /* Callback arguments */` |
|       - | 2164 | `	sxi32 rc;` |
|       - | 2165 | `	/* Point to the desired callback */` |
|      91 | 2166 | `	pCallback = (ph7_value *)pCmpData;` |
|      91 | 2167 | `	if( pA->pMap->pVm->iCmpCallbackExc ){` |
|       - | 2168 | `		/* A previous comparison already raised: stop invoking the callback so` |
|       - | 2169 | `		 * the exception is not thrown again, and let the sort wind down. */` |
|       6 | 2170 | `		return 0;` |
|       - | 2171 | `	}` |
|       - | 2172 | `	/* initialize the result value */` |
|      87 | 2173 | `	PH7_MemObjInit(pA->pMap->pVm,&sResult);` |
|       - | 2174 | `	/* Extract nodes values */` |
|      87 | 2175 | `	pV1 = HashmapExtractNodeValue(pA);` |
|      87 | 2176 | `	pV2 = HashmapExtractNodeValue(pB);` |
|      87 | 2177 | `	apArg[0] = pV1;` |
|      87 | 2178 | `	apArg[1] = pV2;` |
|       - | 2179 | `	/* Invoke the callback */` |
|      87 | 2180 | `	rc = PH7_VmCallUserFunction(pA->pMap->pVm,pCallback,2,apArg,&sResult);` |
|      87 | 2181 | `	if( rc == PH7_EXCEPTION ){` |
|       - | 2182 | `		/* The comparator raised: flag it so the sort driver aborts and` |
|       - | 2183 | `		 * propagates, and order this pair arbitrarily for the rest of the run. */` |
|       6 | 2184 | `		pA->pMap->pVm->iCmpCallbackExc = 1;` |
|       6 | 2185 | `		rc = 0;` |
|      84 | 2186 | `	}else if( rc != SXRET_OK ){` |
|       - | 2187 | `		/* An error occured while calling user defined function [i.e: not defined] */` |
|     ! 0 | 2188 | `		rc = -1; /* Set a dummy result */` |
|     ! 0 | 2189 | `	}else{` |
|       - | 2190 | `		/* Extract callback result */` |
|      82 | 2191 | `		if((sResult.iFlags & MEMOBJ_INT) == 0 ){` |
|       - | 2192 | `			/* Perform an int cast */` |
|     ! 0 | 2193 | `			PH7_MemObjToInteger(&sResult);` |
|     ! 0 | 2194 | `		}` |
|      82 | 2195 | `		rc = (sxi32)sResult.x.iVal;` |
|       - | 2196 | `	}` |
|      87 | 2197 | `	PH7_MemObjRelease(&sResult);` |
|       - | 2198 | `	/* Callback result */` |
|      87 | 2199 | `	return rc;` |
|      47 | 2200 | `}` |
|       - | 2201 | `/*` |
|       - | 2202 | ` * Node comparison callback: Compare nodes by keys only.` |
|       - | 2203 | ` * used-by: [krsort()]` |
|       - | 2204 | ` */` |
|       4 | 2205 | `static sxi32 HashmapCmpCallback5(ph7_hashmap_node *pA,ph7_hashmap_node *pB,void *pCmpData)` |
|       1 | 2206 | `{` |
|       - | 2207 | `	sxi32 rc;` |
|       2 | 2208 | `	SXUNUSED(pCmpData); /* cc warning */` |
|       5 | 2209 | `	if( pA->iType == HASHMAP_BLOB_NODE && pB->iType == HASHMAP_BLOB_NODE ){` |
|       - | 2210 | `		/* Perform a string comparison */` |
|       5 | 2211 | `		rc = SyBlobCmp(&pA->xKey.sKey,&pB->xKey.sKey);` |
|       3 | 2212 | `	}else{` |
|       - | 2213 | `		SyString sStr;` |
|       - | 2214 | `		sxi64 iA,iB;` |
|       - | 2215 | `		/* Perform a numeric comparison */` |
|     ! 0 | 2216 | `		if( pA->iType == HASHMAP_BLOB_NODE ){` |
|       - | 2217 | `			/* Cast to 64-bit integer */` |
|     ! 0 | 2218 | `			SyStringInitFromBuf(&sStr,SyBlobData(&pA->xKey.sKey),SyBlobLength(&pA->xKey.sKey));` |
|     ! 0 | 2219 | `			if( sStr.nByte < 1 ){` |
|     ! 0 | 2220 | `				iA = 0;` |
|     ! 0 | 2221 | `			}else{` |
|     ! 0 | 2222 | `				SyStrToInt64(sStr.zString,sStr.nByte,(void *)&iA,0);` |
|       - | 2223 | `			}` |
|     ! 0 | 2224 | `		}else{` |
|     ! 0 | 2225 | `			iA = pA->xKey.iKey;` |
|       - | 2226 | `		}` |
|     ! 0 | 2227 | `		if( pB->iType == HASHMAP_BLOB_NODE ){` |
|       - | 2228 | `			/* Cast to 64-bit integer */` |
|     ! 0 | 2229 | `			SyStringInitFromBuf(&sStr,SyBlobData(&pB->xKey.sKey),SyBlobLength(&pB->xKey.sKey));` |
|     ! 0 | 2230 | `			if( sStr.nByte < 1 ){` |
|     ! 0 | 2231 | `				iB = 0;` |
|     ! 0 | 2232 | `			}else{` |
|     ! 0 | 2233 | `				SyStrToInt64(sStr.zString,sStr.nByte,(void *)&iB,0);` |
|       - | 2234 | `			}` |
|     ! 0 | 2235 | `		}else{` |
|     ! 0 | 2236 | `			iB = pB->xKey.iKey;` |
|       - | 2237 | `		}` |
|     ! 0 | 2238 | `		rc = (sxi32)(iA-iB);` |
|       - | 2239 | `	}` |
|       5 | 2240 | `	return -rc; /* Reverse result */` |
|       1 | 2241 | `}` |
|       - | 2242 | `/*` |
|       - | 2243 | ` * Node comparison callback: Invoke an user-defined callback for the purpose of node comparison.` |
|       - | 2244 | ` * used-by: [uksort()]` |
|       - | 2245 | ` */` |
|       6 | 2246 | `static sxi32 HashmapCmpCallback6(ph7_hashmap_node *pA,ph7_hashmap_node *pB,void *pCmpData)` |
|       1 | 2247 | `{` |
|       - | 2248 | `	ph7_value sResult,*pCallback;` |
|       - | 2249 | `	ph7_value *apArg[2];  /* Callback arguments */` |
|       - | 2250 | `	ph7_value sK1,sK2;` |
|       - | 2251 | `	sxi32 rc;` |
|       - | 2252 | `	/* Point to the desired callback */` |
|       7 | 2253 | `	pCallback = (ph7_value *)pCmpData;` |
|       7 | 2254 | `	if( pA->pMap->pVm->iCmpCallbackExc ){` |
|       - | 2255 | `		/* A previous comparison already raised: stop invoking the callback so` |
|       - | 2256 | `		 * the exception is not thrown again, and let the sort wind down. */` |
|     ! 0 | 2257 | `		return 0;` |
|       - | 2258 | `	}` |
|       - | 2259 | `	/* initialize the result value */` |
|       7 | 2260 | `	PH7_MemObjInit(pA->pMap->pVm,&sResult);` |
|       7 | 2261 | `	PH7_MemObjInit(pA->pMap->pVm,&sK1);` |
|       7 | 2262 | `	PH7_MemObjInit(pA->pMap->pVm,&sK2);` |
|       - | 2263 | `	/* Extract nodes keys */` |
|       7 | 2264 | `	PH7_HashmapExtractNodeKey(pA,&sK1);` |
|       7 | 2265 | `	PH7_HashmapExtractNodeKey(pB,&sK2);` |
|       7 | 2266 | `	apArg[0] = &sK1;` |
|       7 | 2267 | `	apArg[1] = &sK2;` |
|       - | 2268 | `	/* Mark keys as constants */` |
|       7 | 2269 | `	sK1.nIdx = SXU32_HIGH;` |
|       7 | 2270 | `	sK2.nIdx = SXU32_HIGH;` |
|       - | 2271 | `	/* Invoke the callback */` |
|       7 | 2272 | `	rc = PH7_VmCallUserFunction(pA->pMap->pVm,pCallback,2,apArg,&sResult);` |
|       7 | 2273 | `	if( rc == PH7_EXCEPTION ){` |
|       - | 2274 | `		/* The comparator raised: flag it so the sort driver aborts and` |
|       - | 2275 | `		 * propagates, and order this pair arbitrarily for the rest of the run. */` |
|     ! 0 | 2276 | `		pA->pMap->pVm->iCmpCallbackExc = 1;` |
|     ! 0 | 2277 | `		rc = 0;` |
|       7 | 2278 | `	}else if( rc != SXRET_OK ){` |
|       - | 2279 | `		/* An error occured while calling user defined function [i.e: not defined] */` |
|     ! 0 | 2280 | `		rc = -1; /* Set a dummy result */` |
|     ! 0 | 2281 | `	}else{` |
|       - | 2282 | `		/* Extract callback result */` |
|       7 | 2283 | `		if((sResult.iFlags & MEMOBJ_INT) == 0 ){` |
|       - | 2284 | `			/* Perform an int cast */` |
|     ! 0 | 2285 | `			PH7_MemObjToInteger(&sResult);` |
|     ! 0 | 2286 | `		}` |
|       7 | 2287 | `		rc = (sxi32)sResult.x.iVal;` |
|       - | 2288 | `	}` |
|       7 | 2289 | `	PH7_MemObjRelease(&sResult);` |
|       7 | 2290 | `	PH7_MemObjRelease(&sK1);` |
|       7 | 2291 | `	PH7_MemObjRelease(&sK2);` |
|       - | 2292 | `	/* Callback result */` |
|       7 | 2293 | `	return rc;` |
|       4 | 2294 | `}` |
|       - | 2295 | `/*` |
|       - | 2296 | ` * Node comparison callback: Random node comparison.` |
|       - | 2297 | ` * used-by: [shuffle()]` |
|       - | 2298 | ` */` |
|      15 | 2299 | `static sxi32 HashmapCmpCallback7(ph7_hashmap_node *pA,ph7_hashmap_node *pB,void *pCmpData)` |
|       1 | 2300 | `{` |
|       - | 2301 | `	sxu32 n;` |
|       9 | 2302 | `	SXUNUSED(pB); /* cc warning */` |
|       9 | 2303 | `	SXUNUSED(pCmpData);` |
|       - | 2304 | `	/* Grab a random number */` |
|      16 | 2305 | `	n = PH7_VmRandomNum(pA->pMap->pVm);` |
|       - | 2306 | `	/* if the random number is odd then the first node 'pA' is greater then` |
|       - | 2307 | `	 * the second node 'pB'. Otherwise the reverse is assumed.` |
|       - | 2308 | `	 */` |
|      16 | 2309 | `	return n&1 ? 1 : -1;` |
|       1 | 2310 | `}` |
|       - | 2311 | `/*` |
|       - | 2312 | ` * Rehash all nodes keys after a merge-sort have been applied.` |
|       - | 2313 | ` * Used by [sort(),usort() and rsort()].` |
|       - | 2314 | ` */` |
|     640 | 2315 | `static void HashmapSortRehash(ph7_hashmap *pMap)` |
|       5 | 2316 | `{` |
|       - | 2317 | `	ph7_hashmap_node *p,*pLast;` |
|       - | 2318 | `	sxu32 i;` |
|       - | 2319 | `	/* Rehash all entries */` |
|     645 | 2320 | `	pLast = p = pMap->pFirst;` |
|     645 | 2321 | `	pMap->iNextIdx = 0; /* Reset the automatic index */` |
|     645 | 2322 | `	i = 0;` |
|    6889 | 2323 | `	for( ;; ){` |
|   13783 | 2324 | `		if( i >= pMap->nEntry ){` |
|     645 | 2325 | `			pMap->pLast = pLast; /* Fix the last link broken by the merge-sort */` |
|     645 | 2326 | `			break;` |
|       - | 2327 | `		}` |
|   13143 | 2328 | `		if( p->iType == HASHMAP_BLOB_NODE ){` |
|       - | 2329 | `			/* Do not maintain index association as requested by the PHP specification */` |
|       5 | 2330 | `			SyBlobRelease(&p->xKey.sKey);` |
|       - | 2331 | `			/* Change key type */` |
|       5 | 2332 | `			p->iType = HASHMAP_INT_NODE;` |
|       2 | 2333 | `		}` |
|   13143 | 2334 | `		HashmapRehashIntNode(p);` |
|       - | 2335 | `		/* Point to the next entry */` |
|   13143 | 2336 | `		i++;` |
|   13143 | 2337 | `		pLast = p;` |
|   13143 | 2338 | `		p = p->pPrev; /* Reverse link */` |
|       5 | 2339 | `	}` |
|     645 | 2340 | `}` |
|       - | 2341 | `/*` |
|       - | 2342 | ` * Array functions implementation.` |
|       - | 2343 | ` * Status:` |
|       - | 2344 | ` *  Stable.` |
|       - | 2345 | ` */` |
|       - | 2346 | `/*` |
|       - | 2347 | ` * bool sort(array &$array[,int $sort_flags = SORT_REGULAR ] )` |
|       - | 2348 | ` * Sort an array.` |
|       - | 2349 | ` * Parameters` |
|       - | 2350 | ` *  $array` |
|       - | 2351 | ` *   The input array.` |
|       - | 2352 | ` * $sort_flags` |
|       - | 2353 | ` *  The optional second parameter sort_flags may be used to modify the sorting behavior using these values:` |
|       - | 2354 | ` *  Sorting type flags:` |
|       - | 2355 | ` *   SORT_REGULAR - compare items normally (don't change types)` |
|       - | 2356 | ` *   SORT_NUMERIC - compare items numerically` |
|       - | 2357 | ` *   SORT_STRING - compare items as strings` |
|       - | 2358 | ` * Return` |
|       - | 2359 | ` *  TRUE on success or FALSE on failure.` |
|       - | 2360 | ` *` |
|       - | 2361 | ` */` |
|     982 | 2362 | `static int ph7_hashmap_sort(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       5 | 2363 | `{` |
|       - | 2364 | `	ph7_hashmap *pMap;` |
|       - | 2365 | `	/* Make sure we are dealing with a valid hashmap */` |
|     987 | 2366 | `	if( nArg < 1 \|\| !ph7_value_is_array(apArg[0]) ){` |
|       - | 2367 | `		/* Missing/Invalid arguments,return FALSE */` |
|     ! 0 | 2368 | `		ph7_result_bool(pCtx,0);` |
|     ! 0 | 2369 | `		return PH7_OK;` |
|       - | 2370 | `	}` |
|       - | 2371 | `	/* Point to the internal representation of the input hashmap */` |
|     987 | 2372 | `	PH7_HashmapCowSeparate(pCtx->pVm, apArg[0]);` |
|     987 | 2373 | `	pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|     987 | 2374 | `	if( pMap->nEntry > 1 ){` |
|     629 | 2375 | `		sxi32 iCmpFlags = 0;` |
|     629 | 2376 | `		if( nArg > 1 ){` |
|       - | 2377 | `			/* Extract comparison flags */` |
|       3 | 2378 | `			iCmpFlags = ph7_value_to_int(apArg[1]);` |
|       3 | 2379 | `			if( iCmpFlags == 3 /* SORT_REGULAR */ ){` |
|     ! 0 | 2380 | `				iCmpFlags = 0; /* Standard comparison */` |
|     ! 0 | 2381 | `			}` |
|       1 | 2382 | `		}` |
|       - | 2383 | `		/* Do the merge sort */` |
|     629 | 2384 | `		HashmapMergeSort(pMap,HashmapCmpCallback1,SX_INT_TO_PTR(iCmpFlags));` |
|       - | 2385 | `		/* Rehash [Do not maintain index association as requested by the PHP specification] */` |
|     629 | 2386 | `		HashmapSortRehash(pMap);` |
|     312 | 2387 | `	}` |
|       - | 2388 | `	/* All done,return TRUE */` |
|     987 | 2389 | `	ph7_result_bool(pCtx,1);` |
|     987 | 2390 | `	return PH7_OK;` |
|     496 | 2391 | `}` |
|       - | 2392 | `/*` |
|       - | 2393 | ` * bool asort(array &$array[,int $sort_flags = SORT_REGULAR ] )` |
|       - | 2394 | ` *  Sort an array and maintain index association.` |
|       - | 2395 | ` * Parameters` |
|       - | 2396 | ` *  $array` |
|       - | 2397 | ` *   The input array.` |
|       - | 2398 | ` * $sort_flags` |
|       - | 2399 | ` *  The optional second parameter sort_flags may be used to modify the sorting behavior using these values:` |
|       - | 2400 | ` *  Sorting type flags:` |
|       - | 2401 | ` *   SORT_REGULAR - compare items normally (don't change types)` |
|       - | 2402 | ` *   SORT_NUMERIC - compare items numerically` |
|       - | 2403 | ` *   SORT_STRING - compare items as strings` |
|       - | 2404 | ` * Return` |
|       - | 2405 | ` *  TRUE on success or FALSE on failure.` |
|       - | 2406 | ` */` |
|      32 | 2407 | `static int ph7_hashmap_asort(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       5 | 2408 | `{` |
|       - | 2409 | `	ph7_hashmap *pMap;` |
|       - | 2410 | `	/* PHP 8: ArgumentCountError if no arguments */` |
|      37 | 2411 | `	if( nArg < 1 ){` |
|       3 | 2412 | `		return PH7_VmThrowException(pCtx,` |
|       - | 2413 | `			"ArgumentCountError",` |
|       - | 2414 | `			"asort() expects at least 1 argument, 0 given"` |
|       - | 2415 | `			);` |
|       - | 2416 | `	}` |
|       - | 2417 | `	/* PHP 8: TypeError if first argument is not an array */` |
|      35 | 2418 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|      16 | 2419 | `		return PH7_VmThrowException(pCtx,` |
|       - | 2420 | `			"TypeError",` |
|       - | 2421 | `			"asort(): Argument #1 ($array) must be of type array, %s given",` |
|       4 | 2422 | `			ph7_type_name(apArg[0])` |
|       - | 2423 | `			);` |
|       - | 2424 | `	}` |
|       - | 2425 | `	/* Point to the internal representation of the input hashmap */` |
|      23 | 2426 | `	PH7_HashmapCowSeparate(pCtx->pVm, apArg[0]);` |
|      23 | 2427 | `	pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|      23 | 2428 | `	if( pMap->nEntry > 1 ){` |
|      19 | 2429 | `		sxi32 iCmpFlags = 0;` |
|      19 | 2430 | `		if( nArg > 1 ){` |
|       - | 2431 | `			/* Extract comparison flags */` |
|       5 | 2432 | `			iCmpFlags = ph7_value_to_int(apArg[1]);` |
|       5 | 2433 | `			if( iCmpFlags == 3 /* SORT_REGULAR */ ){` |
|     ! 0 | 2434 | `				iCmpFlags = 0; /* Standard comparison */` |
|     ! 0 | 2435 | `			}` |
|       2 | 2436 | `		}` |
|       - | 2437 | `		/* Do the merge sort */` |
|      19 | 2438 | `		HashmapMergeSort(pMap,HashmapCmpCallback1,SX_INT_TO_PTR(iCmpFlags));` |
|       - | 2439 | `		/* Fix the last link broken by the merge */` |
|      45 | 2440 | `		while(pMap->pLast->pPrev){` |
|      27 | 2441 | `			pMap->pLast = pMap->pLast->pPrev;` |
|       1 | 2442 | `		}` |
|       9 | 2443 | `	}` |
|       - | 2444 | `	/* All done,return TRUE */` |
|      23 | 2445 | `	ph7_result_bool(pCtx,1);` |
|      23 | 2446 | `	return PH7_OK;` |
|      21 | 2447 | `}` |
|       - | 2448 | `/*` |
|       - | 2449 | ` * bool arsort(array &$array[,int $sort_flags = SORT_REGULAR ] )` |
|       - | 2450 | ` *  Sort an array in reverse order and maintain index association.` |
|       - | 2451 | ` * Parameters` |
|       - | 2452 | ` *  $array` |
|       - | 2453 | ` *   The input array.` |
|       - | 2454 | ` * $sort_flags` |
|       - | 2455 | ` *  The optional second parameter sort_flags may be used to modify the sorting behavior using these values:` |
|       - | 2456 | ` *  Sorting type flags:` |
|       - | 2457 | ` *   SORT_REGULAR - compare items normally (don't change types)` |
|       - | 2458 | ` *   SORT_NUMERIC - compare items numerically` |
|       - | 2459 | ` *   SORT_STRING - compare items as strings` |
|       - | 2460 | ` * Return` |
|       - | 2461 | ` *  TRUE on success or FALSE on failure.` |
|       - | 2462 | ` */` |
|      32 | 2463 | `static int ph7_hashmap_arsort(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       5 | 2464 | `{` |
|       - | 2465 | `	ph7_hashmap *pMap;` |
|       - | 2466 | `	/* PHP 8: ArgumentCountError if no arguments */` |
|      37 | 2467 | `	if( nArg < 1 ){` |
|       3 | 2468 | `		return PH7_VmThrowException(pCtx,` |
|       - | 2469 | `			"ArgumentCountError",` |
|       - | 2470 | `			"arsort() expects at least 1 argument, 0 given"` |
|       - | 2471 | `			);` |
|       - | 2472 | `	}` |
|       - | 2473 | `	/* PHP 8: TypeError if first argument is not an array */` |
|      35 | 2474 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|      16 | 2475 | `		return PH7_VmThrowException(pCtx,` |
|       - | 2476 | `			"TypeError",` |
|       - | 2477 | `			"arsort(): Argument #1 ($array) must be of type array, %s given",` |
|       4 | 2478 | `			ph7_type_name(apArg[0])` |
|       - | 2479 | `			);` |
|       - | 2480 | `	}` |
|       - | 2481 | `	/* Point to the internal representation of the input hashmap */` |
|      23 | 2482 | `	PH7_HashmapCowSeparate(pCtx->pVm, apArg[0]);` |
|      23 | 2483 | `	pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|      23 | 2484 | `	if( pMap->nEntry > 1 ){` |
|      19 | 2485 | `		sxi32 iCmpFlags = 0;` |
|      19 | 2486 | `		if( nArg > 1 ){` |
|       - | 2487 | `			/* Extract comparison flags */` |
|       5 | 2488 | `			iCmpFlags = ph7_value_to_int(apArg[1]);` |
|       5 | 2489 | `			if( iCmpFlags == 3 /* SORT_REGULAR */ ){` |
|     ! 0 | 2490 | `				iCmpFlags = 0; /* Standard comparison */` |
|     ! 0 | 2491 | `			}` |
|       2 | 2492 | `		}` |
|       - | 2493 | `		/* Do the merge sort */` |
|      19 | 2494 | `		HashmapMergeSort(pMap,HashmapCmpCallback3,SX_INT_TO_PTR(iCmpFlags));` |
|       - | 2495 | `		/* Fix the last link broken by the merge */` |
|      35 | 2496 | `		while(pMap->pLast->pPrev){` |
|      17 | 2497 | `			pMap->pLast = pMap->pLast->pPrev;` |
|       1 | 2498 | `		}` |
|       9 | 2499 | `	}` |
|       - | 2500 | `	/* All done,return TRUE */` |
|      23 | 2501 | `	ph7_result_bool(pCtx,1);` |
|      23 | 2502 | `	return PH7_OK;` |
|      21 | 2503 | `}` |
|       - | 2504 | `/*` |
|       - | 2505 | ` * bool ksort(array &$array[,int $sort_flags = SORT_REGULAR ] )` |
|       - | 2506 | ` *  Sort an array by key.` |
|       - | 2507 | ` * Parameters` |
|       - | 2508 | ` *  $array` |
|       - | 2509 | ` *   The input array.` |
|       - | 2510 | ` * $sort_flags` |
|       - | 2511 | ` *  The optional second parameter sort_flags may be used to modify the sorting behavior using these values:` |
|       - | 2512 | ` *  Sorting type flags:` |
|       - | 2513 | ` *   SORT_REGULAR - compare items normally (don't change types)` |
|       - | 2514 | ` *   SORT_NUMERIC - compare items numerically` |
|       - | 2515 | ` *   SORT_STRING - compare items as strings` |
|       - | 2516 | ` * Return` |
|       - | 2517 | ` *  TRUE on success or FALSE on failure.` |
|       - | 2518 | ` */` |
|       4 | 2519 | `static int ph7_hashmap_ksort(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       1 | 2520 | `{` |
|       - | 2521 | `	ph7_hashmap *pMap;` |
|       - | 2522 | `	/* Make sure we are dealing with a valid hashmap */` |
|       5 | 2523 | `	if( nArg < 1 \|\| !ph7_value_is_array(apArg[0]) ){` |
|       - | 2524 | `		/* Missing/Invalid arguments,return FALSE */` |
|     ! 0 | 2525 | `		ph7_result_bool(pCtx,0);` |
|     ! 0 | 2526 | `		return PH7_OK;` |
|       - | 2527 | `	}` |
|       - | 2528 | `	/* Point to the internal representation of the input hashmap */` |
|       5 | 2529 | `	PH7_HashmapCowSeparate(pCtx->pVm, apArg[0]);` |
|       5 | 2530 | `	pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|       5 | 2531 | `	if( pMap->nEntry > 1 ){` |
|       5 | 2532 | `		sxi32 iCmpFlags = 0;` |
|       5 | 2533 | `		if( nArg > 1 ){` |
|       - | 2534 | `			/* Extract comparison flags */` |
|     ! 0 | 2535 | `			iCmpFlags = ph7_value_to_int(apArg[1]);` |
|     ! 0 | 2536 | `			if( iCmpFlags == 3 /* SORT_REGULAR */ ){` |
|     ! 0 | 2537 | `				iCmpFlags = 0; /* Standard comparison */` |
|     ! 0 | 2538 | `			}` |
|     ! 0 | 2539 | `		}` |
|       - | 2540 | `		/* Do the merge sort */` |
|       5 | 2541 | `		HashmapMergeSort(pMap,HashmapCmpCallback2,SX_INT_TO_PTR(iCmpFlags));` |
|       - | 2542 | `		/* Fix the last link broken by the merge */` |
|      15 | 2543 | `		while(pMap->pLast->pPrev){` |
|      11 | 2544 | `			pMap->pLast = pMap->pLast->pPrev;` |
|       1 | 2545 | `		}` |
|       2 | 2546 | `	}` |
|       - | 2547 | `	/* All done,return TRUE */` |
|       5 | 2548 | `	ph7_result_bool(pCtx,1);` |
|       5 | 2549 | `	return PH7_OK;` |
|       3 | 2550 | `}` |
|       - | 2551 | `/*` |
|       - | 2552 | ` * bool krsort(array &$array[,int $sort_flags = SORT_REGULAR ] )` |
|       - | 2553 | ` *  Sort an array by key in reverse order.` |
|       - | 2554 | ` * Parameters` |
|       - | 2555 | ` *  $array` |
|       - | 2556 | ` *   The input array.` |
|       - | 2557 | ` * $sort_flags` |
|       - | 2558 | ` *  The optional second parameter sort_flags may be used to modify the sorting behavior using these values:` |
|       - | 2559 | ` *  Sorting type flags:` |
|       - | 2560 | ` *   SORT_REGULAR - compare items normally (don't change types)` |
|       - | 2561 | ` *   SORT_NUMERIC - compare items numerically` |
|       - | 2562 | ` *   SORT_STRING - compare items as strings` |
|       - | 2563 | ` * Return` |
|       - | 2564 | ` *  TRUE on success or FALSE on failure.` |
|       - | 2565 | ` */` |
|       2 | 2566 | `static int ph7_hashmap_krsort(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       1 | 2567 | `{` |
|       - | 2568 | `	ph7_hashmap *pMap;` |
|       - | 2569 | `	/* Make sure we are dealing with a valid hashmap */` |
|       3 | 2570 | `	if( nArg < 1 \|\| !ph7_value_is_array(apArg[0]) ){` |
|       - | 2571 | `		/* Missing/Invalid arguments,return FALSE */` |
|     ! 0 | 2572 | `		ph7_result_bool(pCtx,0);` |
|     ! 0 | 2573 | `		return PH7_OK;` |
|       - | 2574 | `	}` |
|       - | 2575 | `	/* Point to the internal representation of the input hashmap */` |
|       3 | 2576 | `	PH7_HashmapCowSeparate(pCtx->pVm, apArg[0]);` |
|       3 | 2577 | `	pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|       3 | 2578 | `	if( pMap->nEntry > 1 ){` |
|       3 | 2579 | `		sxi32 iCmpFlags = 0;` |
|       3 | 2580 | `		if( nArg > 1 ){` |
|       - | 2581 | `			/* Extract comparison flags */` |
|     ! 0 | 2582 | `			iCmpFlags = ph7_value_to_int(apArg[1]);` |
|     ! 0 | 2583 | `			if( iCmpFlags == 3 /* SORT_REGULAR */ ){` |
|     ! 0 | 2584 | `				iCmpFlags = 0; /* Standard comparison */` |
|     ! 0 | 2585 | `			}` |
|     ! 0 | 2586 | `		}` |
|       - | 2587 | `		/* Do the merge sort */` |
|       3 | 2588 | `		HashmapMergeSort(pMap,HashmapCmpCallback5,SX_INT_TO_PTR(iCmpFlags));` |
|       - | 2589 | `		/* Fix the last link broken by the merge */` |
|       7 | 2590 | `		while(pMap->pLast->pPrev){` |
|       5 | 2591 | `			pMap->pLast = pMap->pLast->pPrev;` |
|       1 | 2592 | `		}` |
|       1 | 2593 | `	}` |
|       - | 2594 | `	/* All done,return TRUE */` |
|       3 | 2595 | `	ph7_result_bool(pCtx,1);` |
|       3 | 2596 | `	return PH7_OK;` |
|       2 | 2597 | `}` |
|       - | 2598 | `/*` |
|       - | 2599 | ` * bool rsort(array &$array[,int $sort_flags = SORT_REGULAR ] )` |
|       - | 2600 | ` * Sort an array in reverse order.` |
|       - | 2601 | ` * Parameters` |
|       - | 2602 | ` *  $array` |
|       - | 2603 | ` *   The input array.` |
|       - | 2604 | ` * $sort_flags` |
|       - | 2605 | ` *  The optional second parameter sort_flags may be used to modify the sorting behavior using these values:` |
|       - | 2606 | ` *  Sorting type flags:` |
|       - | 2607 | ` *   SORT_REGULAR - compare items normally (don't change types)` |
|       - | 2608 | ` *   SORT_NUMERIC - compare items numerically` |
|       - | 2609 | ` *   SORT_STRING - compare items as strings` |
|       - | 2610 | ` * Return` |
|       - | 2611 | ` *  TRUE on success or FALSE on failure.` |
|       - | 2612 | ` */` |
|       2 | 2613 | `static int ph7_hashmap_rsort(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       1 | 2614 | `{` |
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
|       3 | 2626 | `		sxi32 iCmpFlags = 0;` |
|       3 | 2627 | `		if( nArg > 1 ){` |
|       - | 2628 | `			/* Extract comparison flags */` |
|     ! 0 | 2629 | `			iCmpFlags = ph7_value_to_int(apArg[1]);` |
|     ! 0 | 2630 | `			if( iCmpFlags == 3 /* SORT_REGULAR */ ){` |
|     ! 0 | 2631 | `				iCmpFlags = 0; /* Standard comparison */` |
|     ! 0 | 2632 | `			}` |
|     ! 0 | 2633 | `		}` |
|       - | 2634 | `		/* Do the merge sort */` |
|       3 | 2635 | `		HashmapMergeSort(pMap,HashmapCmpCallback3,SX_INT_TO_PTR(iCmpFlags));` |
|       - | 2636 | `		/* Rehash [Do not maintain index association as requested by the PHP specification] */` |
|       3 | 2637 | `		HashmapSortRehash(pMap);` |
|       1 | 2638 | `	}` |
|       - | 2639 | `	/* All done,return TRUE */` |
|       3 | 2640 | `	ph7_result_bool(pCtx,1);` |
|       3 | 2641 | `	return PH7_OK;` |
|       2 | 2642 | `}` |
|       - | 2643 | `/*` |
|       - | 2644 | ` * bool usort(array &$array,callable $cmp_function)` |
|       - | 2645 | ` *  Sort an array by values using a user-defined comparison function.` |
|       - | 2646 | ` * Parameters` |
|       - | 2647 | ` *  $array` |
|       - | 2648 | ` *   The input array.` |
|       - | 2649 | ` * $cmp_function` |
|       - | 2650 | ` *  The comparison function must return an integer less than, equal to, or greater` |
|       - | 2651 | ` *  than zero if the first argument is considered to be respectively less than, equal` |
|       - | 2652 | ` *  to, or greater than the second.` |
|       - | 2653 | ` *    int callback ( mixed $a, mixed $b )` |
|       - | 2654 | ` * Return` |
|       - | 2655 | ` *  TRUE on success or FALSE on failure.` |
|       - | 2656 | ` */` |
|      12 | 2657 | `static int ph7_hashmap_usort(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       3 | 2658 | `{` |
|       - | 2659 | `	ph7_hashmap *pMap;` |
|       - | 2660 | `	/* Make sure we are dealing with a valid hashmap */` |
|      15 | 2661 | `	if( nArg < 1 \|\| !ph7_value_is_array(apArg[0]) ){` |
|       - | 2662 | `		/* Missing/Invalid arguments,return FALSE */` |
|     ! 0 | 2663 | `		ph7_result_bool(pCtx,0);` |
|     ! 0 | 2664 | `		return PH7_OK;` |
|       - | 2665 | `	}` |
|       - | 2666 | `	/* Point to the internal representation of the input hashmap */` |
|      15 | 2667 | `	PH7_HashmapCowSeparate(pCtx->pVm, apArg[0]);` |
|      15 | 2668 | `	pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|      15 | 2669 | `	if( pMap->nEntry > 1 ){` |
|      15 | 2670 | `		ph7_value *pCallback = 0;` |
|       - | 2671 | `		ProcNodeCmp xCmp;` |
|      15 | 2672 | `		xCmp = HashmapCmpCallback4; /* User-defined function as the comparison callback */` |
|      15 | 2673 | `		if( nArg > 1 && ph7_value_is_callable(apArg[1]) ){` |
|       - | 2674 | `			/* Point to the desired callback */` |
|      15 | 2675 | `			pCallback = apArg[1];` |
|       9 | 2676 | `		}else{` |
|       - | 2677 | `			/* Use the default comparison function */` |
|     ! 0 | 2678 | `			xCmp = HashmapCmpCallback1;` |
|       - | 2679 | `		}` |
|       - | 2680 | `		/* Do the merge sort */` |
|      15 | 2681 | `		pCtx->pVm->iCmpCallbackExc = 0;` |
|      15 | 2682 | `		HashmapMergeSort(pMap,xCmp,pCallback);` |
|       - | 2683 | `		/* Rehash [Do not maintain index association as requested by the PHP specification] */` |
|      15 | 2684 | `		HashmapSortRehash(pMap);` |
|      15 | 2685 | `		if( pCtx->pVm->iCmpCallbackExc ){` |
|       - | 2686 | `			/* The comparison callback raised: propagate so the dispatcher unwinds. */` |
|       6 | 2687 | `			pCtx->pVm->iCmpCallbackExc = 0;` |
|       6 | 2688 | `			return PH7_EXCEPTION;` |
|       - | 2689 | `		}` |
|       4 | 2690 | `	}` |
|       - | 2691 | `	/* All done,return TRUE */` |
|      10 | 2692 | `	ph7_result_bool(pCtx,1);` |
|      10 | 2693 | `	return PH7_OK;` |
|       9 | 2694 | `}` |
|       - | 2695 | `/*` |
|       - | 2696 | ` * bool uasort(array &$array,callable $cmp_function)` |
|       - | 2697 | ` *  Sort an array by values using a user-defined comparison function` |
|       - | 2698 | ` *  and maintain index association.` |
|       - | 2699 | ` * Parameters` |
|       - | 2700 | ` *  $array` |
|       - | 2701 | ` *   The input array.` |
|       - | 2702 | ` * $cmp_function` |
|       - | 2703 | ` *  The comparison function must return an integer less than, equal to, or greater` |
|       - | 2704 | ` *  than zero if the first argument is considered to be respectively less than, equal` |
|       - | 2705 | ` *  to, or greater than the second.` |
|       - | 2706 | ` *    int callback ( mixed $a, mixed $b )` |
|       - | 2707 | ` * Return` |
|       - | 2708 | ` *  TRUE on success or FALSE on failure.` |
|       - | 2709 | ` */` |
|       2 | 2710 | `static int ph7_hashmap_uasort(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       1 | 2711 | `{` |
|       - | 2712 | `	ph7_hashmap *pMap;` |
|       - | 2713 | `	/* Make sure we are dealing with a valid hashmap */` |
|       3 | 2714 | `	if( nArg < 1 \|\| !ph7_value_is_array(apArg[0]) ){` |
|       - | 2715 | `		/* Missing/Invalid arguments,return FALSE */` |
|     ! 0 | 2716 | `		ph7_result_bool(pCtx,0);` |
|     ! 0 | 2717 | `		return PH7_OK;` |
|       - | 2718 | `	}` |
|       - | 2719 | `	/* Point to the internal representation of the input hashmap */` |
|       3 | 2720 | `	PH7_HashmapCowSeparate(pCtx->pVm, apArg[0]);` |
|       3 | 2721 | `	pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|       3 | 2722 | `	if( pMap->nEntry > 1 ){` |
|       3 | 2723 | `		ph7_value *pCallback = 0;` |
|       - | 2724 | `		ProcNodeCmp xCmp;` |
|       3 | 2725 | `		xCmp = HashmapCmpCallback4; /* User-defined function as the comparison callback */` |
|       3 | 2726 | `		if( nArg > 1 && ph7_value_is_callable(apArg[1]) ){` |
|       - | 2727 | `			/* Point to the desired callback */` |
|       3 | 2728 | `			pCallback = apArg[1];` |
|       2 | 2729 | `		}else{` |
|       - | 2730 | `			/* Use the default comparison function */` |
|     ! 0 | 2731 | `			xCmp = HashmapCmpCallback1;` |
|       - | 2732 | `		}` |
|       - | 2733 | `		/* Do the merge sort */` |
|       3 | 2734 | `		pCtx->pVm->iCmpCallbackExc = 0;` |
|       3 | 2735 | `		HashmapMergeSort(pMap,xCmp,pCallback);` |
|       - | 2736 | `		/* Fix the last link broken by the merge */` |
|       5 | 2737 | `		while(pMap->pLast->pPrev){` |
|       3 | 2738 | `			pMap->pLast = pMap->pLast->pPrev;` |
|       1 | 2739 | `		}` |
|       3 | 2740 | `		if( pCtx->pVm->iCmpCallbackExc ){` |
|       - | 2741 | `			/* The comparison callback raised: propagate so the dispatcher unwinds. */` |
|     ! 0 | 2742 | `			pCtx->pVm->iCmpCallbackExc = 0;` |
|     ! 0 | 2743 | `			return PH7_EXCEPTION;` |
|       - | 2744 | `		}` |
|       1 | 2745 | `	}` |
|       - | 2746 | `	/* All done,return TRUE */` |
|       3 | 2747 | `	ph7_result_bool(pCtx,1);` |
|       3 | 2748 | `	return PH7_OK;` |
|       2 | 2749 | `}` |
|       - | 2750 | `/*` |
|       - | 2751 | ` * bool uksort(array &$array,callable $cmp_function)` |
|       - | 2752 | ` *  Sort an array by keys using a user-defined comparison` |
|       - | 2753 | ` *  function and maintain index association.` |
|       - | 2754 | ` * Parameters` |
|       - | 2755 | ` *  $array` |
|       - | 2756 | ` *   The input array.` |
|       - | 2757 | ` * $cmp_function` |
|       - | 2758 | ` *  The comparison function must return an integer less than, equal to, or greater` |
|       - | 2759 | ` *  than zero if the first argument is considered to be respectively less than, equal` |
|       - | 2760 | ` *  to, or greater than the second.` |
|       - | 2761 | ` *    int callback ( mixed $a, mixed $b )` |
|       - | 2762 | ` * Return` |
|       - | 2763 | ` *  TRUE on success or FALSE on failure.` |
|       - | 2764 | ` */` |
|       2 | 2765 | `static int ph7_hashmap_uksort(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       1 | 2766 | `{` |
|       - | 2767 | `	ph7_hashmap *pMap;` |
|       - | 2768 | `	/* Make sure we are dealing with a valid hashmap */` |
|       3 | 2769 | `	if( nArg < 1 \|\| !ph7_value_is_array(apArg[0]) ){` |
|       - | 2770 | `		/* Missing/Invalid arguments,return FALSE */` |
|     ! 0 | 2771 | `		ph7_result_bool(pCtx,0);` |
|     ! 0 | 2772 | `		return PH7_OK;` |
|       - | 2773 | `	}` |
|       - | 2774 | `	/* Point to the internal representation of the input hashmap */` |
|       3 | 2775 | `	PH7_HashmapCowSeparate(pCtx->pVm, apArg[0]);` |
|       3 | 2776 | `	pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|       3 | 2777 | `	if( pMap->nEntry > 1 ){` |
|       3 | 2778 | `		ph7_value *pCallback = 0;` |
|       - | 2779 | `		ProcNodeCmp xCmp;` |
|       3 | 2780 | `		xCmp = HashmapCmpCallback6; /* User-defined function as the comparison callback */` |
|       3 | 2781 | `		if( nArg > 1 && ph7_value_is_callable(apArg[1]) ){` |
|       - | 2782 | `			/* Point to the desired callback */` |
|       3 | 2783 | `			pCallback = apArg[1];` |
|       2 | 2784 | `		}else{` |
|       - | 2785 | `			/* Use the default comparison function */` |
|     ! 0 | 2786 | `			xCmp = HashmapCmpCallback2;` |
|       - | 2787 | `		}` |
|       - | 2788 | `		/* Do the merge sort */` |
|       3 | 2789 | `		pCtx->pVm->iCmpCallbackExc = 0;` |
|       3 | 2790 | `		HashmapMergeSort(pMap,xCmp,pCallback);` |
|       - | 2791 | `		/* Fix the last link broken by the merge */` |
|       3 | 2792 | `		while(pMap->pLast->pPrev){` |
|     ! 0 | 2793 | `			pMap->pLast = pMap->pLast->pPrev;` |
|     ! 0 | 2794 | `		}` |
|       3 | 2795 | `		if( pCtx->pVm->iCmpCallbackExc ){` |
|       - | 2796 | `			/* The comparison callback raised: propagate so the dispatcher unwinds. */` |
|     ! 0 | 2797 | `			pCtx->pVm->iCmpCallbackExc = 0;` |
|     ! 0 | 2798 | `			return PH7_EXCEPTION;` |
|       - | 2799 | `		}` |
|       1 | 2800 | `	}` |
|       - | 2801 | `	/* All done,return TRUE */` |
|       3 | 2802 | `	ph7_result_bool(pCtx,1);` |
|       3 | 2803 | `	return PH7_OK;` |
|       2 | 2804 | `}` |
|       - | 2805 | `/*` |
|       - | 2806 | ` * bool shuffle(array &$array)` |
|       - | 2807 | ` *  shuffles (randomizes the order of the elements in) an array.` |
|       - | 2808 | ` * Parameters` |
|       - | 2809 | ` *  $array` |
|       - | 2810 | ` *   The input array.` |
|       - | 2811 | ` * Return` |
|       - | 2812 | ` *  TRUE on success or FALSE on failure.` |
|       - | 2813 | ` *` |
|       - | 2814 | ` */` |
|       2 | 2815 | `static int ph7_hashmap_shuffle(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       1 | 2816 | `{` |
|       - | 2817 | `	ph7_hashmap *pMap;` |
|       - | 2818 | `	/* Make sure we are dealing with a valid hashmap */` |
|       3 | 2819 | `	if( nArg < 1 \|\| !ph7_value_is_array(apArg[0]) ){` |
|       - | 2820 | `		/* Missing/Invalid arguments,return FALSE */` |
|     ! 0 | 2821 | `		ph7_result_bool(pCtx,0);` |
|     ! 0 | 2822 | `		return PH7_OK;` |
|       - | 2823 | `	}` |
|       - | 2824 | `	/* Point to the internal representation of the input hashmap */` |
|       3 | 2825 | `	PH7_HashmapCowSeparate(pCtx->pVm, apArg[0]);` |
|       3 | 2826 | `	pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|       3 | 2827 | `	if( pMap->nEntry > 1 ){` |
|       - | 2828 | `		/* Do the merge sort */` |
|       3 | 2829 | `		HashmapMergeSort(pMap,HashmapCmpCallback7,0);` |
|       - | 2830 | `		/* Fix the last link broken by the merge */` |
|       8 | 2831 | `		while(pMap->pLast->pPrev){` |
|       6 | 2832 | `			pMap->pLast = pMap->pLast->pPrev;` |
|       1 | 2833 | `		}` |
|       1 | 2834 | `	}` |
|       - | 2835 | `	/* All done,return TRUE */` |
|       3 | 2836 | `	ph7_result_bool(pCtx,1);` |
|       3 | 2837 | `	return PH7_OK;` |
|       2 | 2838 | `}` |
|       - | 2839 | `/*` |
|       - | 2840 | ` * int count(array $var [, int $mode = COUNT_NORMAL ])` |
|       - | 2841 | ` *   Count all elements in an array, or something in an object.` |
|       - | 2842 | ` * Parameters` |
|       - | 2843 | ` *  $var` |
|       - | 2844 | ` *   The array or the object.` |
|       - | 2845 | ` * $mode` |
|       - | 2846 | ` *  If the optional mode parameter is set to COUNT_RECURSIVE (or 1), count()` |
|       - | 2847 | ` *  will recursively count the array. This is particularly useful for counting` |
|       - | 2848 | ` *  all the elements of a multidimensional array.` |
|       - | 2849 | ` * Return` |
|       - | 2850 | ` *  Returns the number of elements in the array.` |
|       - | 2851 | ` */` |
|     844 | 2852 | `static int ph7_hashmap_count(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       5 | 2853 | `{` |
|     849 | 2854 | `	int bRecursive = FALSE;` |
|     849 | 2855 | `	int bCycleDetected = FALSE;` |
|       - | 2856 | `	sxi64 iCount;` |
|     849 | 2857 | `	if( nArg < 1 ){` |
|       3 | 2858 | `		return PH7_VmThrowException(pCtx,` |
|       - | 2859 | `			"ArgumentCountError",` |
|       - | 2860 | `			"count() expects at least 1 argument, 0 given"` |
|       - | 2861 | `			);` |
|       - | 2862 | `	}` |
|     847 | 2863 | `	if( nArg > 2 ){` |
|       4 | 2864 | `		return PH7_VmThrowException(pCtx,` |
|       - | 2865 | `			"ArgumentCountError",` |
|       - | 2866 | `			"count() expects at most 2 arguments, %d given",` |
|       1 | 2867 | `			nArg` |
|       - | 2868 | `			);` |
|       - | 2869 | `	}` |
|       - | 2870 | `	/* PHP validates $mode right after parsing, before the type dispatch, so` |
|       - | 2871 | `	 * an invalid mode raises ValueError whether $value is an array or a` |
|       - | 2872 | `	 * Countable object (the mode is then ignored for the Countable path). */` |
|     845 | 2873 | `	if( nArg > 1 ){` |
|      45 | 2874 | `		sxi32 iMode = ph7_value_to_int(apArg[1]);` |
|      45 | 2875 | `		if( iMode != 0 /* COUNT_NORMAL */ && iMode != 1 /* COUNT_RECURSIVE */ ){` |
|      12 | 2876 | `			return PH7_VmThrowException(pCtx,` |
|       - | 2877 | `				"ValueError",` |
|       - | 2878 | `				"count(): Argument #2 ($mode) must be either COUNT_NORMAL or COUNT_RECURSIVE"` |
|       - | 2879 | `				);` |
|       - | 2880 | `		}` |
|      34 | 2881 | `		bRecursive = iMode == 1;` |
|      16 | 2882 | `	}` |
|     837 | 2883 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|       - | 2884 | `		/* Countable object: dispatch to ->count() */` |
|      35 | 2885 | `		if( apArg[0]->iFlags & MEMOBJ_OBJ ){` |
|      24 | 2886 | `			ph7_class_instance *pInst = (ph7_class_instance *)apArg[0]->x.pOther;` |
|      24 | 2887 | `			ph7_class *pCountable = pCtx->pVm->pCountableClass;` |
|      24 | 2888 | `			if( pCountable && PH7_VmInstanceOf(pInst->pClass,pCountable) ){` |
|      21 | 2889 | `				ph7_class_method *pMeth = PH7_ClassExtractMethod(pInst->pClass,` |
|       - | 2890 | `					"count",sizeof("count")-1);` |
|      21 | 2891 | `				if( pMeth ){` |
|       - | 2892 | `					ph7_value sResult;` |
|      21 | 2893 | `					PH7_MemObjInit(pCtx->pVm,&sResult);` |
|      21 | 2894 | `					PH7_VmCallClassMethod(pCtx->pVm,pInst,pMeth,&sResult,0,0);` |
|      21 | 2895 | `					ph7_result_int64(pCtx,ph7_value_to_int64(&sResult));` |
|      21 | 2896 | `					PH7_MemObjRelease(&sResult);` |
|      21 | 2897 | `					return PH7_OK;` |
|       - | 2898 | `				}` |
|     ! 0 | 2899 | `			}` |
|       1 | 2900 | `		}` |
|      22 | 2901 | `		return PH7_VmThrowException(pCtx,` |
|       - | 2902 | `			"TypeError",` |
|       - | 2903 | `			"count(): Argument #1 ($value) must be of type Countable\|array, %s given",` |
|       6 | 2904 | `			ph7_type_name(apArg[0])` |
|       - | 2905 | `			);` |
|       - | 2906 | `	}` |
|       - | 2907 | `	/* Count */` |
|     807 | 2908 | `	iCount = HashmapCount((ph7_hashmap *)apArg[0]->x.pOther,bRecursive,&bCycleDetected);` |
|     807 | 2909 | `	if( bCycleDetected ){` |
|       3 | 2910 | `		PH7_VmThrowError(pCtx->pVm,0,PH7_CTX_WARNING,"count(): Recursion detected");` |
|       1 | 2911 | `	}` |
|     807 | 2912 | `	ph7_result_int64(pCtx,iCount);` |
|     807 | 2913 | `	return PH7_OK;` |
|     427 | 2914 | `}` |
|       - | 2915 | `/*` |
|       - | 2916 | ` * bool array_key_exists(value $key,array $search)` |
|       - | 2917 | ` *  Checks if the given key or index exists in the array.` |
|       - | 2918 | ` * Parameters` |
|       - | 2919 | ` * $key` |
|       - | 2920 | ` *   Value to check.` |
|       - | 2921 | ` * $search` |
|       - | 2922 | ` *  An array with keys to check.` |
|       - | 2923 | ` * Return` |
|       - | 2924 | ` *  TRUE on success or FALSE on failure.` |
|       - | 2925 | ` */` |
|      86 | 2926 | `static int ph7_hashmap_key_exists(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       5 | 2927 | `{` |
|       - | 2928 | `	sxi32 rc;` |
|      91 | 2929 | `	if( nArg != 2 ){` |
|       - | 2930 | `		/* PHP requires exactly two arguments */` |
|      12 | 2931 | `		return PH7_VmThrowException(pCtx,` |
|       - | 2932 | `			"ArgumentCountError",` |
|       - | 2933 | `			"array_key_exists() expects exactly 2 arguments, %d given",` |
|       3 | 2934 | `			nArg` |
|       - | 2935 | `			);` |
|       - | 2936 | `	}` |
|       - | 2937 | `	/* Make sure we are dealing with a valid hashmap */` |
|      85 | 2938 | `	if( !ph7_value_is_array(apArg[1]) ){` |
|       - | 2939 | `		/* Type mismatch -> TypeError */` |
|       8 | 2940 | `		return PH7_VmThrowException(pCtx,` |
|       - | 2941 | `			"TypeError",` |
|       - | 2942 | `			"array_key_exists(): Argument #2 ($array) must be of type array, %s given",` |
|       4 | 2943 | `			ph7_type_name(apArg[1])` |
|       - | 2944 | `			);` |
|       - | 2945 | `	}` |
|       - | 2946 | `	/* Emit deprecation warnings matching PHP behaviour */` |
|      80 | 2947 | `	if( apArg[0]->iFlags & MEMOBJ_NULL ){` |
|       3 | 2948 | `		ph7_context_throw_error_format(pCtx,8192,` |
|       - | 2949 | `			"Using null as the key parameter for array_key_exists() is deprecated, "` |
|       - | 2950 | `			"use an empty string instead"` |
|       - | 2951 | `			);` |
|      79 | 2952 | `	}else if( apArg[0]->iFlags & MEMOBJ_REAL ){` |
|       3 | 2953 | `		ph7_real rVal = apArg[0]->rVal;` |
|       3 | 2954 | `		if( rVal != (ph7_real)(sxi64)rVal ){` |
|       4 | 2955 | `			ph7_context_throw_error_format(pCtx,8192,` |
|       - | 2956 | `				"Implicit conversion from float %g to int loses precision"` |
|       1 | 2957 | `				,rVal` |
|       - | 2958 | `				);` |
|       1 | 2959 | `		}` |
|       1 | 2960 | `	}` |
|       - | 2961 | `	/* Perform the lookup */` |
|      80 | 2962 | `	rc = PH7_HashmapLookup((ph7_hashmap *)apArg[1]->x.pOther,apArg[0],0);` |
|       - | 2963 | `	/* lookup result */` |
|      80 | 2964 | `	ph7_result_bool(pCtx,rc == SXRET_OK ? 1 : 0);` |
|      80 | 2965 | `	return PH7_OK;` |
|      48 | 2966 | `}` |
|       - | 2967 | `/*` |
|       - | 2968 | ` * value array_pop(array $array)` |
|       - | 2969 | ` *   POP the last inserted element from the array.` |
|       - | 2970 | ` * Parameter` |
|       - | 2971 | ` *  The array to get the value from.` |
|       - | 2972 | ` * Return` |
|       - | 2973 | ` *  Poped value or NULL on failure.` |
|       - | 2974 | ` */` |
|      18 | 2975 | `static int ph7_hashmap_pop(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       5 | 2976 | `{` |
|       - | 2977 | `	ph7_hashmap *pMap;` |
|       - | 2978 | `	/* PHP requires exactly one argument and it must be passed by reference */` |
|      23 | 2979 | `	if( nArg != 1 ){` |
|       8 | 2980 | `		return PH7_VmThrowException(pCtx,` |
|       - | 2981 | `			"ArgumentCountError",` |
|       - | 2982 | `			"array_pop() expects exactly 1 argument, %d given",` |
|       2 | 2983 | `			nArg` |
|       - | 2984 | `			);` |
|       - | 2985 | `	}` |
|       - | 2986 | `	/* Passing a constant (including literals) or non-variable triggers the same` |
|       - | 2987 | `	 * error message as official PHP. Check the index to detect constants. */` |
|      18 | 2988 | `	if( apArg[0]->nIdx == SXU32_HIGH ){` |
|       6 | 2989 | `		return PH7_VmThrowException(pCtx,` |
|       - | 2990 | `			"Error",` |
|       - | 2991 | `			"array_pop(): Argument #1 ($array) could not be passed by reference"` |
|       - | 2992 | `			);` |
|       - | 2993 | `	}` |
|       - | 2994 | `	/* Make sure we are dealing with a valid hashmap */` |
|      12 | 2995 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|       4 | 2996 | `		return PH7_VmThrowException(pCtx,` |
|       - | 2997 | `			"TypeError",` |
|       - | 2998 | `			"array_pop(): Argument #1 ($array) must be of type array, %s given",` |
|       1 | 2999 | `			ph7_type_name(apArg[0])` |
|       - | 3000 | `			);` |
|       - | 3001 | `	}` |
|       9 | 3002 | `	PH7_HashmapCowSeparate(pCtx->pVm, apArg[0]);` |
|       9 | 3003 | `	pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|       9 | 3004 | `	if( pMap->nEntry < 1 ){` |
|       - | 3005 | `		/* Nothing to pop,return NULL */` |
|       3 | 3006 | `		ph7_result_null(pCtx);` |
|       2 | 3007 | `	}else{` |
|       7 | 3008 | `		ph7_hashmap_node *pLast = pMap->pLast;` |
|       - | 3009 | `		ph7_value *pObj;` |
|       7 | 3010 | `		pObj = HashmapExtractNodeValue(pLast);` |
|       7 | 3011 | `		if( pObj ){` |
|       - | 3012 | `			/* Node value */` |
|       7 | 3013 | `			ph7_result_value(pCtx,pObj);` |
|       - | 3014 | `			/* Unlink the node */` |
|       7 | 3015 | `			PH7_HashmapUnlinkNode(pLast,TRUE);` |
|       4 | 3016 | `		}else{` |
|     ! 0 | 3017 | `			ph7_result_null(pCtx);` |
|       - | 3018 | `		}` |
|       - | 3019 | `		/* Reset the cursor */` |
|       7 | 3020 | `		pMap->pCur = pMap->pFirst;` |
|       - | 3021 | `	}` |
|       9 | 3022 | `	return PH7_OK;` |
|      14 | 3023 | `}` |
|       - | 3024 | `/*` |
|       - | 3025 | ` * int array_push($array,$var,...)` |
|       - | 3026 | ` *   Push one or more elements onto the end of array. (Stack insertion)` |
|       - | 3027 | ` * Parameters` |
|       - | 3028 | ` *  array` |
|       - | 3029 | ` *    The input array.` |
|       - | 3030 | ` *  var` |
|       - | 3031 | ` *   On or more value to push.` |
|       - | 3032 | ` * Return` |
|       - | 3033 | ` *  New array count (including old items).` |
|       - | 3034 | ` */` |
|      24 | 3035 | `static int ph7_hashmap_push(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       5 | 3036 | `{` |
|       - | 3037 | `	ph7_hashmap *pMap;` |
|       - | 3038 | `	sxi32 rc;` |
|       - | 3039 | `	int i;` |
|      29 | 3040 | `	if( nArg < 1 ){` |
|       4 | 3041 | `		return PH7_VmThrowException(pCtx,` |
|       - | 3042 | `			"ArgumentCountError",` |
|       - | 3043 | `			"array_push() expects at least 1 argument, %d given",` |
|       1 | 3044 | `			nArg` |
|       - | 3045 | `			);` |
|       - | 3046 | `	}` |
|       - | 3047 | `	/* Passing a constant (including literals) or non-variable triggers the same` |
|       - | 3048 | `	 * error message as official PHP. Check the index to detect constants. */` |
|      26 | 3049 | `	if( apArg[0]->nIdx == SXU32_HIGH ){` |
|       6 | 3050 | `		return PH7_VmThrowException(pCtx,` |
|       - | 3051 | `			"Error",` |
|       - | 3052 | `			"array_push(): Argument #1 ($array) could not be passed by reference"` |
|       - | 3053 | `			);` |
|       - | 3054 | `	}` |
|       - | 3055 | `	/* Make sure we are dealing with a valid hashmap */` |
|      20 | 3056 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|       4 | 3057 | `		return PH7_VmThrowException(pCtx,` |
|       - | 3058 | `			"TypeError",` |
|       - | 3059 | `			"array_push(): Argument #1 ($array) must be of type array, %s given",` |
|       1 | 3060 | `			ph7_type_name(apArg[0])` |
|       - | 3061 | `			);` |
|       - | 3062 | `	}` |
|       - | 3063 | `	/* Point to the internal representation of the input hashmap */` |
|      18 | 3064 | `	PH7_HashmapCowSeparate(pCtx->pVm, apArg[0]);` |
|      18 | 3065 | `	pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|       - | 3066 | `	/* Start pushing given values */` |
|      34 | 3067 | `	for( i = 1 ; i < nArg ; ++i ){` |
|      20 | 3068 | `		rc = PH7_HashmapInsert(pMap,0,apArg[i]);` |
|      20 | 3069 | `		if( rc != SXRET_OK ){` |
|       3 | 3070 | `			if( rc == PH7_EXCEPTION \|\| rc == PH7_ABORT ){` |
|       - | 3071 | `				/* Saturated-append Error (php: array_push throws, no result) */` |
|       3 | 3072 | `				return rc;` |
|       - | 3073 | `			}` |
|     ! 0 | 3074 | `			break;` |
|       - | 3075 | `		}` |
|       9 | 3076 | `	}` |
|       - | 3077 | `	/* Return the new count */` |
|      15 | 3078 | `	ph7_result_int64(pCtx,(sxi64)pMap->nEntry);` |
|      15 | 3079 | `	return PH7_OK;` |
|      17 | 3080 | `}` |
|       - | 3081 | `/*` |
|       - | 3082 | ` * value array_shift(array $array)` |
|       - | 3083 | ` *   Shift an element off the beginning of array.` |
|       - | 3084 | ` * Parameter` |
|       - | 3085 | ` *  The array to get the value from.` |
|       - | 3086 | ` * Return` |
|       - | 3087 | ` *  Shifted value or NULL on failure.` |
|       - | 3088 | ` */` |
|      38 | 3089 | `static int ph7_hashmap_shift(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       5 | 3090 | `{` |
|       - | 3091 | `	ph7_hashmap *pMap;` |
|       - | 3092 | `	/* PHP requires exactly one argument and it must be passed by reference */` |
|      43 | 3093 | `	if( nArg != 1 ){` |
|       8 | 3094 | `		return PH7_VmThrowException(pCtx,` |
|       - | 3095 | `			"ArgumentCountError",` |
|       - | 3096 | `			"array_shift() expects exactly 1 argument, %d given",` |
|       2 | 3097 | `			nArg` |
|       - | 3098 | `			);` |
|       - | 3099 | `	}` |
|       - | 3100 | `	/* Detect constants or literals, which cannot be passed by reference. */` |
|      39 | 3101 | `	if( apArg[0]->nIdx == SXU32_HIGH ){` |
|       6 | 3102 | `		return PH7_VmThrowException(pCtx,` |
|       - | 3103 | `			"Error",` |
|       - | 3104 | `			"array_shift(): Argument #1 ($array) could not be passed by reference"` |
|       - | 3105 | `			);` |
|       - | 3106 | `	}` |
|       - | 3107 | `	/* Make sure we are dealing with a valid hashmap */` |
|      35 | 3108 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|       4 | 3109 | `		return PH7_VmThrowException(pCtx,` |
|       - | 3110 | `			"TypeError",` |
|       - | 3111 | `			"array_shift(): Argument #1 ($array) must be of type array, %s given",` |
|       1 | 3112 | `			ph7_type_name(apArg[0])` |
|       - | 3113 | `			);` |
|       - | 3114 | `	}` |
|       - | 3115 | `	/* Point to the internal representation of the hashmap */` |
|      33 | 3116 | `	PH7_HashmapCowSeparate(pCtx->pVm, apArg[0]);` |
|      33 | 3117 | `	pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|      33 | 3118 | `	if( pMap->nEntry < 1 ){` |
|       - | 3119 | `		/* Empty hashmap,return NULL */` |
|       3 | 3120 | `		ph7_result_null(pCtx);` |
|       2 | 3121 | `	}else{` |
|      31 | 3122 | `		ph7_hashmap_node *pEntry = pMap->pFirst;` |
|       - | 3123 | `		ph7_value *pObj;` |
|       - | 3124 | `		sxu32 n;` |
|      31 | 3125 | `		pObj = HashmapExtractNodeValue(pEntry);` |
|      31 | 3126 | `		if( pObj ){` |
|       - | 3127 | `			/* Node value */` |
|      31 | 3128 | `			ph7_result_value(pCtx,pObj);` |
|       - | 3129 | `			/* Unlink the first node */` |
|      31 | 3130 | `			PH7_HashmapUnlinkNode(pEntry,TRUE);` |
|      18 | 3131 | `		}else{` |
|     ! 0 | 3132 | `			ph7_result_null(pCtx);` |
|       - | 3133 | `		}` |
|       - | 3134 | `		/* Rehash all int keys */` |
|      31 | 3135 | `		n = pMap->nEntry;` |
|      31 | 3136 | `		pEntry = pMap->pFirst;` |
|      31 | 3137 | `		pMap->iNextIdx = 0; /* Reset the automatic index */` |
|      40 | 3138 | `		for(;;){` |
|      85 | 3139 | `			if( n < 1 ){` |
|      31 | 3140 | `				break;` |
|       - | 3141 | `			}` |
|      59 | 3142 | `			if( pEntry->iType == HASHMAP_INT_NODE ){` |
|      59 | 3143 | `				HashmapRehashIntNode(pEntry);` |
|      27 | 3144 | `			}` |
|       - | 3145 | `			/* Point to the next entry */` |
|      59 | 3146 | `			pEntry = pEntry->pPrev; /* Reverse link */` |
|      59 | 3147 | `			n--;` |
|       5 | 3148 | `		}` |
|       - | 3149 | `		/* Reset the cursor */` |
|      31 | 3150 | `		pMap->pCur = pMap->pFirst;` |
|       - | 3151 | `	}` |
|      33 | 3152 | `	return PH7_OK;` |
|      24 | 3153 | `}` |
|       - | 3154 | `/*` |
|       - | 3155 | ` * Extract the node cursor value.` |
|       - | 3156 | ` */` |
|      28 | 3157 | `static sxi32 HashmapCurrentValue(ph7_context *pCtx,ph7_hashmap *pMap,int iDirection)` |
|       1 | 3158 | `{` |
|      29 | 3159 | `	ph7_hashmap_node *pCur = pMap->pCur;` |
|       - | 3160 | `	ph7_value *pVal;` |
|      29 | 3161 | `	if( pCur == 0 ){` |
|       - | 3162 | `		/* Cursor does not point to anything,return FALSE */` |
|     ! 0 | 3163 | `		ph7_result_bool(pCtx,0);` |
|     ! 0 | 3164 | `		return PH7_OK;` |
|       - | 3165 | `	}` |
|      29 | 3166 | `	if( iDirection != 0 ){` |
|      11 | 3167 | `		if( iDirection > 0 ){` |
|       - | 3168 | `			/* Point to the next entry */` |
|       9 | 3169 | `			pMap->pCur = pCur->pPrev; /* Reverse link */` |
|       9 | 3170 | `			pCur = pMap->pCur;` |
|       5 | 3171 | `		}else{` |
|       - | 3172 | `			/* Point to the previous entry */` |
|       3 | 3173 | `			pMap->pCur = pCur->pNext; /* Reverse link */` |
|       3 | 3174 | `			pCur = pMap->pCur;` |
|       - | 3175 | `		}` |
|      11 | 3176 | `		if( pCur == 0 ){` |
|       - | 3177 | `			/* End of input reached,return FALSE */` |
|     ! 0 | 3178 | `			ph7_result_bool(pCtx,0);` |
|     ! 0 | 3179 | `			return PH7_OK;` |
|       - | 3180 | `		}` |
|       5 | 3181 | `	}` |
|       - | 3182 | `	/* Point to the desired element */` |
|      29 | 3183 | `	pVal = HashmapExtractNodeValue(pCur);` |
|      29 | 3184 | `	if( pVal ){` |
|      29 | 3185 | `		ph7_result_value(pCtx,pVal);` |
|      15 | 3186 | `	}else{` |
|     ! 0 | 3187 | `		ph7_result_bool(pCtx,0);` |
|       - | 3188 | `	}` |
|      29 | 3189 | `	return PH7_OK;` |
|      15 | 3190 | `}` |
|       - | 3191 | `/*` |
|       - | 3192 | ` * value current(array $array)` |
|       - | 3193 | ` *  Return the current element in an array.` |
|       - | 3194 | ` * Parameter` |
|       - | 3195 | ` *  $input: The input array.` |
|       - | 3196 | ` * Return` |
|       - | 3197 | ` *  The current() function simply returns the value of the array element that's currently` |
|       - | 3198 | ` *  being pointed to by the internal pointer. It does not move the pointer in any way.` |
|       - | 3199 | ` *  If the internal pointer points beyond the end of the elements list or the array` |
|       - | 3200 | ` *  is empty, current() returns FALSE.` |
|       - | 3201 | ` */` |
|      12 | 3202 | `static int ph7_hashmap_current(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       1 | 3203 | `{` |
|      13 | 3204 | `	if( nArg < 1 ){` |
|       - | 3205 | `		/* Missing arguments,return FALSE */` |
|     ! 0 | 3206 | `		ph7_result_bool(pCtx,0);` |
|     ! 0 | 3207 | `		return PH7_OK;` |
|       - | 3208 | `	}` |
|       - | 3209 | `	/* Make sure we are dealing with a valid hashmap */` |
|      13 | 3210 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|       - | 3211 | `		/* Invalid argument,return FALSE */` |
|     ! 0 | 3212 | `		ph7_result_bool(pCtx,0);` |
|     ! 0 | 3213 | `		return PH7_OK;` |
|       - | 3214 | `	}` |
|      13 | 3215 | `	HashmapCurrentValue(&(*pCtx),(ph7_hashmap *)apArg[0]->x.pOther,0);` |
|      13 | 3216 | `	return PH7_OK;` |
|       7 | 3217 | `}` |
|       - | 3218 | `/*` |
|       - | 3219 | ` * value next(array $input)` |
|       - | 3220 | ` *  Advance the internal array pointer of an array.` |
|       - | 3221 | ` * Parameter` |
|       - | 3222 | ` *  $input: The input array.` |
|       - | 3223 | ` * Return` |
|       - | 3224 | ` *  next() behaves like current(), with one difference. It advances the internal array` |
|       - | 3225 | ` *  pointer one place forward before returning the element value. That means it returns` |
|       - | 3226 | ` *  the next array value and advances the internal array pointer by one.` |
|       - | 3227 | ` */` |
|       8 | 3228 | `static int ph7_hashmap_next(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       1 | 3229 | `{` |
|       9 | 3230 | `	if( nArg < 1 ){` |
|       - | 3231 | `		/* Missing arguments,return FALSE */` |
|     ! 0 | 3232 | `		ph7_result_bool(pCtx,0);` |
|     ! 0 | 3233 | `		return PH7_OK;` |
|       - | 3234 | `	}` |
|       - | 3235 | `	/* Make sure we are dealing with a valid hashmap */` |
|       9 | 3236 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|       - | 3237 | `		/* Invalid argument,return FALSE */` |
|     ! 0 | 3238 | `		ph7_result_bool(pCtx,0);` |
|     ! 0 | 3239 | `		return PH7_OK;` |
|       - | 3240 | `	}` |
|       9 | 3241 | `	HashmapCurrentValue(&(*pCtx),(ph7_hashmap *)apArg[0]->x.pOther,1);` |
|       9 | 3242 | `	return PH7_OK;` |
|       5 | 3243 | `}` |
|       - | 3244 | `/*` |
|       - | 3245 | ` * value prev(array $input)` |
|       - | 3246 | ` *  Rewind the internal array pointer.` |
|       - | 3247 | ` * Parameter` |
|       - | 3248 | ` *  $input: The input array.` |
|       - | 3249 | ` * Return` |
|       - | 3250 | ` *  Returns the array value in the previous place that's pointed` |
|       - | 3251 | ` *  to by the internal array pointer, or FALSE if there are no more` |
|       - | 3252 | ` *  elements.` |
|       - | 3253 | ` */` |
|       2 | 3254 | `static int ph7_hashmap_prev(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       1 | 3255 | `{` |
|       3 | 3256 | `	if( nArg < 1 ){` |
|       - | 3257 | `		/* Missing arguments,return FALSE */` |
|     ! 0 | 3258 | `		ph7_result_bool(pCtx,0);` |
|     ! 0 | 3259 | `		return PH7_OK;` |
|       - | 3260 | `	}` |
|       - | 3261 | `	/* Make sure we are dealing with a valid hashmap */` |
|       3 | 3262 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|       - | 3263 | `		/* Invalid argument,return FALSE */` |
|     ! 0 | 3264 | `		ph7_result_bool(pCtx,0);` |
|     ! 0 | 3265 | `		return PH7_OK;` |
|       - | 3266 | `	}` |
|       3 | 3267 | `	HashmapCurrentValue(&(*pCtx),(ph7_hashmap *)apArg[0]->x.pOther,-1);` |
|       3 | 3268 | `	return PH7_OK;` |
|       2 | 3269 | `}` |
|       - | 3270 | `/*` |
|       - | 3271 | ` * value end(array $input)` |
|       - | 3272 | ` *  Set the internal pointer of an array to its last element.` |
|       - | 3273 | ` * Parameter` |
|       - | 3274 | ` *  $input: The input array.` |
|       - | 3275 | ` * Return` |
|       - | 3276 | ` *  Returns the value of the last element or FALSE for empty array.` |
|       - | 3277 | ` */` |
|       2 | 3278 | `static int ph7_hashmap_end(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       1 | 3279 | `{` |
|       - | 3280 | `	ph7_hashmap *pMap;` |
|       3 | 3281 | `	if( nArg < 1 ){` |
|       - | 3282 | `		/* Missing arguments,return FALSE */` |
|     ! 0 | 3283 | `		ph7_result_bool(pCtx,0);` |
|     ! 0 | 3284 | `		return PH7_OK;` |
|       - | 3285 | `	}` |
|       - | 3286 | `	/* Make sure we are dealing with a valid hashmap */` |
|       3 | 3287 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|       - | 3288 | `		/* Invalid argument,return FALSE */` |
|     ! 0 | 3289 | `		ph7_result_bool(pCtx,0);` |
|     ! 0 | 3290 | `		return PH7_OK;` |
|       - | 3291 | `	}` |
|       - | 3292 | `	/* Point to the internal representation of the input hashmap */` |
|       3 | 3293 | `	pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|       - | 3294 | `	/* Point to the last node */` |
|       3 | 3295 | `	pMap->pCur = pMap->pLast;` |
|       - | 3296 | `	/* Return the last node value */` |
|       3 | 3297 | `	HashmapCurrentValue(&(*pCtx),pMap,0);` |
|       3 | 3298 | `	return PH7_OK;` |
|       2 | 3299 | `}` |
|       - | 3300 | `/*` |
|       - | 3301 | ` * value reset(array $array )` |
|       - | 3302 | ` *  Set the internal pointer of an array to its first element.` |
|       - | 3303 | ` * Parameter` |
|       - | 3304 | ` *  $input: The input array.` |
|       - | 3305 | ` * Return` |
|       - | 3306 | ` *  Returns the value of the first array element,or FALSE if the array is empty.` |
|       - | 3307 | ` */` |
|       4 | 3308 | `static int ph7_hashmap_reset(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       1 | 3309 | `{` |
|       - | 3310 | `	ph7_hashmap *pMap;` |
|       5 | 3311 | `	if( nArg < 1 ){` |
|       - | 3312 | `		/* Missing arguments,return FALSE */` |
|     ! 0 | 3313 | `		ph7_result_bool(pCtx,0);` |
|     ! 0 | 3314 | `		return PH7_OK;` |
|       - | 3315 | `	}` |
|       - | 3316 | `	/* Make sure we are dealing with a valid hashmap */` |
|       5 | 3317 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|       - | 3318 | `		/* Invalid argument,return FALSE */` |
|     ! 0 | 3319 | `		ph7_result_bool(pCtx,0);` |
|     ! 0 | 3320 | `		return PH7_OK;` |
|       - | 3321 | `	}` |
|       - | 3322 | `	/* Point to the internal representation of the input hashmap */` |
|       5 | 3323 | `	pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|       - | 3324 | `	/* Point to the first node */` |
|       5 | 3325 | `	pMap->pCur = pMap->pFirst;` |
|       - | 3326 | `	/* Return the last node value if available */` |
|       5 | 3327 | `	HashmapCurrentValue(&(*pCtx),pMap,0);` |
|       5 | 3328 | `	return PH7_OK;` |
|       3 | 3329 | `}` |
|       - | 3330 | `/*` |
|       - | 3331 | ` * value key(array $array)` |
|       - | 3332 | ` *   Fetch a key from an array` |
|       - | 3333 | ` * Parameter` |
|       - | 3334 | ` *  $input` |
|       - | 3335 | ` *   The input array.` |
|       - | 3336 | ` * Return` |
|       - | 3337 | ` *  The key() function simply returns the key of the array element that's currently` |
|       - | 3338 | ` *  being pointed to by the internal pointer. It does not move the pointer in any way.` |
|       - | 3339 | ` *  If the internal pointer points beyond the end of the elements list or the array` |
|       - | 3340 | ` *  is empty, key() returns NULL.` |
|       - | 3341 | ` */` |
|       4 | 3342 | `static int ph7_hashmap_simple_key(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       1 | 3343 | `{` |
|       - | 3344 | `	ph7_hashmap_node *pCur;` |
|       - | 3345 | `	ph7_hashmap *pMap;` |
|       5 | 3346 | `	if( nArg < 1 ){` |
|       - | 3347 | `		/* Missing arguments,return NULL */` |
|     ! 0 | 3348 | `		ph7_result_null(pCtx);` |
|     ! 0 | 3349 | `		return PH7_OK;` |
|       - | 3350 | `	}` |
|       - | 3351 | `	/* Make sure we are dealing with a valid hashmap */` |
|       5 | 3352 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|       - | 3353 | `		/* Invalid argument,return NULL */` |
|     ! 0 | 3354 | `		ph7_result_null(pCtx);` |
|     ! 0 | 3355 | `		return PH7_OK;` |
|       - | 3356 | `	}` |
|       5 | 3357 | `	pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|       5 | 3358 | `	pCur = pMap->pCur;` |
|       5 | 3359 | `	if( pCur == 0 ){` |
|       - | 3360 | `		/* Cursor does not point to anything,return NULL */` |
|     ! 0 | 3361 | `		ph7_result_null(pCtx);` |
|     ! 0 | 3362 | `		return PH7_OK;` |
|       - | 3363 | `	}` |
|       5 | 3364 | `	if( pCur->iType == HASHMAP_INT_NODE){` |
|       - | 3365 | `		/* Key is integer */` |
|     ! 0 | 3366 | `		ph7_result_int64(pCtx,pCur->xKey.iKey);` |
|     ! 0 | 3367 | `	}else{` |
|       - | 3368 | `		/* Key is blob */` |
|       7 | 3369 | `		ph7_result_string(pCtx,` |
|       4 | 3370 | `			(const char *)SyBlobData(&pCur->xKey.sKey),(int)SyBlobLength(&pCur->xKey.sKey));` |
|       - | 3371 | `	}` |
|       5 | 3372 | `	return PH7_OK;` |
|       3 | 3373 | `}` |
|       - | 3374 | `/*` |
|       - | 3375 | ` * array each(array $input)` |
|       - | 3376 | ` *  Return the current key and value pair from an array and advance the array cursor.` |
|       - | 3377 | ` * Parameter` |
|       - | 3378 | ` *  $input` |
|       - | 3379 | ` *    The input array.` |
|       - | 3380 | ` * Return` |
|       - | 3381 | ` *  Returns the current key and value pair from the array array. This pair is returned` |
|       - | 3382 | ` *  in a four-element array, with the keys 0, 1, key, and value. Elements 0 and key` |
|       - | 3383 | ` *  contain the key name of the array element, and 1 and value contain the data.` |
|       - | 3384 | ` *  If the internal pointer for the array points past the end of the array contents` |
|       - | 3385 | ` *  each() returns FALSE.` |
|       - | 3386 | ` */` |
|      22 | 3387 | `static int ph7_hashmap_each(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       1 | 3388 | `{` |
|       - | 3389 | `	ph7_hashmap_node *pCur;` |
|       - | 3390 | `	ph7_hashmap *pMap;` |
|       - | 3391 | `	ph7_value *pArray;` |
|       - | 3392 | `	ph7_value *pVal;` |
|       - | 3393 | `	ph7_value sKey;` |
|      23 | 3394 | `	if( nArg < 1 ){` |
|       - | 3395 | `		/* Missing arguments,return FALSE */` |
|     ! 0 | 3396 | `		ph7_result_bool(pCtx,0);` |
|     ! 0 | 3397 | `		return PH7_OK;` |
|       - | 3398 | `	}` |
|       - | 3399 | `	/* Make sure we are dealing with a valid hashmap */` |
|      23 | 3400 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|       - | 3401 | `		/* Invalid argument,return FALSE */` |
|     ! 0 | 3402 | `		ph7_result_bool(pCtx,0);` |
|     ! 0 | 3403 | `		return PH7_OK;` |
|       - | 3404 | `	}` |
|       - | 3405 | `	/* Point to the internal representation that describe the input hashmap */` |
|      23 | 3406 | `	pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|      23 | 3407 | `	if( pMap->pCur == 0 ){` |
|       - | 3408 | `		/* Cursor does not point to anything,return FALSE */` |
|       9 | 3409 | `		ph7_result_bool(pCtx,0);` |
|       9 | 3410 | `		return PH7_OK;` |
|       - | 3411 | `	}` |
|      15 | 3412 | `	pCur = pMap->pCur;` |
|       - | 3413 | `	/* Create a new array */` |
|      15 | 3414 | `	pArray = ph7_context_new_array(pCtx);` |
|      15 | 3415 | `	if( pArray == 0 ){` |
|     ! 0 | 3416 | `		ph7_result_bool(pCtx,0);` |
|     ! 0 | 3417 | `		return PH7_OK;` |
|       - | 3418 | `	}` |
|      15 | 3419 | `	pVal = HashmapExtractNodeValue(pCur);` |
|       - | 3420 | `	/* Insert the current value */` |
|      15 | 3421 | `	ph7_array_add_intkey_elem(pArray,1,pVal);` |
|      15 | 3422 | `	ph7_array_add_strkey_elem(pArray,"value",pVal);` |
|       - | 3423 | `	/* Make the key */` |
|      15 | 3424 | `	if( pCur->iType == HASHMAP_INT_NODE ){` |
|       7 | 3425 | `		PH7_MemObjInitFromInt(pMap->pVm,&sKey,pCur->xKey.iKey);` |
|       4 | 3426 | `	}else{` |
|       9 | 3427 | `		PH7_MemObjInitFromString(pMap->pVm,&sKey,0);` |
|       9 | 3428 | `		PH7_MemObjStringAppend(&sKey,(const char *)SyBlobData(&pCur->xKey.sKey),SyBlobLength(&pCur->xKey.sKey));` |
|       - | 3429 | `	}` |
|       - | 3430 | `	/* Insert the current key */` |
|      15 | 3431 | `	ph7_array_add_intkey_elem(pArray,0,&sKey);` |
|      15 | 3432 | `	ph7_array_add_strkey_elem(pArray,"key",&sKey);` |
|      15 | 3433 | `	PH7_MemObjRelease(&sKey);` |
|       - | 3434 | `	/* Advance the cursor */` |
|      15 | 3435 | `	pMap->pCur = pCur->pPrev; /* Reverse link */` |
|       - | 3436 | `	/* Return the current entry */` |
|      15 | 3437 | `	ph7_result_value(pCtx,pArray);` |
|      15 | 3438 | `	return PH7_OK;` |
|      12 | 3439 | `}` |
|       - | 3440 | `/*` |
|       - | 3441 | ` * range() — a faithful port of php 8.5's ext/standard/array.c implementation` |
|       - | 3442 | ` * (php_range_process_input + PHP_FUNCTION(range)), so the value semantics,` |
|       - | 3443 | ` * diagnostics, and their ordering are byte-exact: decreasing ranges, float` |
|       - | 3444 | ` * ranges, character ranges, the step/endpoint ValueErrors, the ZPP TypeErrors` |
|       - | 3445 | ` * and null deprecations, and the string-endpoint warnings.` |
|       - | 3446 | ` */` |
|       - | 3447 | `#define PH7_RANGE_HT_MAX_SIZE 1073741824 /* php's HT_MAX_SIZE (2^30 entries) */` |
|       - | 3448 | `/*` |
|       - | 3449 | ` * Endpoint classification, mirroring php_range_process_input's return` |
|       - | 3450 | ` * contract. php returns zval type tags whose ORDER encodes the logic` |
|       - | 3451 | ` * (IS_LONG < IS_DOUBLE < IS_STRING < IS_ARRAY); the >=/< comparisons in` |
|       - | 3452 | ` * ph7_hashmap_range depend on the same ordering here.` |
|       - | 3453 | ` *   RANGE_IN_LONG/DOUBLE : only interpretable as int / float` |
|       - | 3454 | ` *   RANGE_IN_STRING      : only interpretable as a (char-range) string` |
|       - | 3455 | ` *   RANGE_IN_DIGIT       : single-byte numeric string — valid as both a char` |
|       - | 3456 | ` *                          and a number (php returns IS_ARRAY for this)` |
|       - | 3457 | ` */` |
|       - | 3458 | `#define RANGE_IN_ERROR   0` |
|       - | 3459 | `#define RANGE_IN_LONG    1` |
|       - | 3460 | `#define RANGE_IN_DOUBLE  2` |
|       - | 3461 | `#define RANGE_IN_STRING  3` |
|       - | 3462 | `#define RANGE_IN_DIGIT   4` |
|       - | 3463 | `/* IEEE special-value tests: the engine-wide bit-pattern macros from` |
|       - | 3464 | ` * sxtypes.h (via ph7int.h) — same ones the printf/serialize paths use. */` |
|       - | 3465 | `/*` |
|       - | 3466 | ` * The type name php's ZPP prints after "must be of type ..., X given":` |
|       - | 3467 | ` * the concrete class name for objects, the usual type name otherwise.` |
|       - | 3468 | ` */` |
|       8 | 3469 | `static const char * RangeArgTypeName(ph7_value *pVal,char *zBuf,sxu32 nBufLen)` |
|       1 | 3470 | `{` |
|       9 | 3471 | `	if( pVal->iFlags & MEMOBJ_OBJ ){` |
|       3 | 3472 | `		ph7_class_instance *pThis = (ph7_class_instance *)pVal->x.pOther;` |
|       3 | 3473 | `		sxu32 n = SXMIN(pThis->pClass->sName.nByte,nBufLen - 1);` |
|       3 | 3474 | `		SyMemcpy((const void *)pThis->pClass->sName.zString,zBuf,n);` |
|       3 | 3475 | `		zBuf[n] = 0;` |
|       3 | 3476 | `		return zBuf;` |
|       - | 3477 | `	}` |
|       7 | 3478 | `	return ph7_type_name(pVal);` |
|       5 | 3479 | `}` |
|       - | 3480 | `/*` |
|       - | 3481 | ` * Classify a string with php's is_numeric_string() grammar:` |
|       - | 3482 | ` *   [ws] [sign] ( D+ [ . D* ] \| . D+ ) [ (e\|E) [sign] D+ ] [ws]` |
|       - | 3483 | ` * — the whole string must be consumed; hex/binary/"INF"/"NAN" are NOT` |
|       - | 3484 | ` * numeric. Returns RANGE_IN_LONG with *pLong set, RANGE_IN_DOUBLE with` |
|       - | 3485 | ` * *pDouble set (a fractional/exponent form, or an integer too wide for an` |
|       - | 3486 | ` * sxi64 — php reclassifies those as float), or RANGE_IN_ERROR when the` |
|       - | 3487 | ` * string is not numeric. The float value comes from libc strtod, like` |
|       - | 3488 | ` * php's zend_strtod (byte-exact-floats rule). zIn must be NUL-terminated` |
|       - | 3489 | ` * at zIn[nLen] — ph7_value_to_string guarantees this (SyBlobNullAppend) —` |
|       - | 3490 | ` * so strtod can parse it in place once the grammar has validated it.` |
|       - | 3491 | ` */` |
|     104 | 3492 | `static sxu8 RangeStrToNumber(const char *zIn,sxu32 nLen,sxi64 *pLong,double *pDouble)` |
|       1 | 3493 | `{` |
|     105 | 3494 | `	const char *z = zIn,*zEnd = &zIn[nLen];` |
|     105 | 3495 | `	sxu64 uVal = 0;` |
|     105 | 3496 | `	int bNeg = 0,bDigit = 0,bReal = 0,bOverflow = 0;` |
|     107 | 3497 | `	while( z < zEnd && (unsigned char)z[0] < 0xc0 && SyisSpace(z[0]) ){ z++; }` |
|     105 | 3498 | `	if( z < zEnd && (z[0] == '+' \|\| z[0] == '-') ){` |
|     ! 0 | 3499 | `		bNeg = (z[0] == '-');` |
|     ! 0 | 3500 | `		z++;` |
|     ! 0 | 3501 | `	}` |
|     147 | 3502 | `	while( z < zEnd && (unsigned char)z[0] < 0xc0 && SyisDigit(z[0]) ){` |
|      43 | 3503 | `		int d = z[0] - '0';` |
|       - | 3504 | `		/* Track overflow past 2^63, the widest magnitude an sxi64 can carry` |
|       - | 3505 | `		 * (as LONG_MIN); overflowing integers become floats like in php. */` |
|      43 | 3506 | `		if( uVal > 922337203685477580ULL \|\| (uVal == 922337203685477580ULL && d > 8) ){` |
|     ! 0 | 3507 | `			bOverflow = 1;` |
|     ! 0 | 3508 | `		}else{` |
|      43 | 3509 | `			uVal = uVal * 10 + (sxu64)d;` |
|       - | 3510 | `		}` |
|      43 | 3511 | `		bDigit = 1;` |
|      43 | 3512 | `		z++;` |
|       1 | 3513 | `	}` |
|     105 | 3514 | `	if( z < zEnd && z[0] == '.' ){` |
|       3 | 3515 | `		bReal = 1;` |
|       3 | 3516 | `		z++;` |
|       5 | 3517 | `		while( z < zEnd && (unsigned char)z[0] < 0xc0 && SyisDigit(z[0]) ){` |
|       3 | 3518 | `			bDigit = 1;` |
|       3 | 3519 | `			z++;` |
|       1 | 3520 | `		}` |
|       1 | 3521 | `	}` |
|       - | 3522 | `	/* At least one mantissa digit required (rejects "", ".", "+", "e5"). */` |
|     105 | 3523 | `	if( !bDigit ){` |
|      51 | 3524 | `		return RANGE_IN_ERROR;` |
|       - | 3525 | `	}` |
|       - | 3526 | `	/* Optional exponent — needs at least one digit (rejects "1e", "1e+"). */` |
|      55 | 3527 | `	if( z < zEnd && (z[0] == 'e' \|\| z[0] == 'E') ){` |
|       3 | 3528 | `		z++;` |
|       3 | 3529 | `		if( z < zEnd && (z[0] == '+' \|\| z[0] == '-') ){ z++; }` |
|       3 | 3530 | `		if( z >= zEnd \|\| (unsigned char)z[0] >= 0xc0 \|\| !SyisDigit(z[0]) ){` |
|     ! 0 | 3531 | `			return RANGE_IN_ERROR;` |
|       - | 3532 | `		}` |
|       3 | 3533 | `		bReal = 1;` |
|       5 | 3534 | `		while( z < zEnd && (unsigned char)z[0] < 0xc0 && SyisDigit(z[0]) ){ z++; }` |
|       1 | 3535 | `	}` |
|       - | 3536 | `	/* Trailing whitespace allowed; anything else means not numeric. */` |
|      55 | 3537 | `	while( z < zEnd && (unsigned char)z[0] < 0xc0 && SyisSpace(z[0]) ){ z++; }` |
|      55 | 3538 | `	if( z != zEnd ){` |
|       3 | 3539 | `		return RANGE_IN_ERROR;` |
|       - | 3540 | `	}` |
|      52 | 3541 | `	if( bOverflow \|\| (!bNeg && uVal > (sxu64)LARGEST_INT64)` |
|      27 | 3542 | `	 \|\| (bNeg && uVal > (sxu64)LARGEST_INT64 + 1) ){` |
|      52 | 3543 | `		bReal = 1;` |
|      52 | 3544 | `	}` |
|      27 | 3545 | `	if( bReal ){` |
|       5 | 3546 | `		*pDouble = strtod(zIn,0);` |
|       5 | 3547 | `		return RANGE_IN_DOUBLE;` |
|       - | 3548 | `	}` |
|       - | 3549 | `	/* Negate in unsigned space so 2^63 lands on LONG_MIN without overflow. */` |
|      23 | 3550 | `	*pLong = bNeg ? (sxi64)((sxu64)0 - uVal) : (sxi64)uVal;` |
|      23 | 3551 | `	return RANGE_IN_LONG;` |
|      40 | 3552 | `}` |
|       - | 3553 | `/*` |
|       - | 3554 | ` * ZPP emulation for $start/$end (php's Z_PARAM_NUMBER_OR_STR, weak mode):` |
|       - | 3555 | ` * reject array/object/resource with php's TypeError, deprecate null (the` |
|       - | 3556 | ` * value then reads as int 0 — *pbNullCoerced). php runs this for all` |
|       - | 3557 | ` * arguments BEFORE any value/domain check, hence the split from` |
|       - | 3558 | ` * RangeProcessInput below. Returns FALSE after throwing (*pRc set).` |
|       - | 3559 | ` */` |
|     262 | 3560 | `static int RangeEndpointZpp(ph7_context *pCtx,ph7_value *pIn,int iArg,const char *zName,int *pbNullCoerced,sxi32 *pRc)` |
|       1 | 3561 | `{` |
|       - | 3562 | `	char zMsg[160];` |
|     263 | 3563 | `	*pRc = PH7_OK;` |
|     263 | 3564 | `	if( pIn->iFlags & (MEMOBJ_HASHMAP\|MEMOBJ_OBJ\|MEMOBJ_RES) ){` |
|       - | 3565 | `		char zType[80];` |
|      10 | 3566 | `		*pRc = PH7_VmThrowException(pCtx,"TypeError",` |
|       - | 3567 | `			"range(): Argument #%d ($%s) must be of type string\|int\|float, %s given",` |
|       3 | 3568 | `			iArg,zName,RangeArgTypeName(pIn,zType,sizeof(zType)));` |
|       7 | 3569 | `		return FALSE;` |
|       - | 3570 | `	}` |
|     257 | 3571 | `	if( pIn->iFlags & MEMOBJ_NULL ){` |
|       7 | 3572 | `		SyBufferFormat(zMsg,sizeof(zMsg),` |
|       - | 3573 | `			"range(): Passing null to parameter #%d ($%s) of type string\|int\|float is deprecated",` |
|       2 | 3574 | `			iArg,zName);` |
|       5 | 3575 | `		PH7_VmThrowError(pCtx->pVm,0,E_DEPRECATED,zMsg);` |
|       5 | 3576 | `		*pbNullCoerced = TRUE;` |
|       2 | 3577 | `	}` |
|     257 | 3578 | `	return TRUE;` |
|     132 | 3579 | `}` |
|       - | 3580 | `/*` |
|       - | 3581 | ` * ZPP emulation for $step (php's Z_PARAM_NUMBER, weak mode): int/float pass` |
|       - | 3582 | ` * through, bool coerces to int, null deprecates to int 0 (which then trips` |
|       - | 3583 | ` * the "cannot be 0" ValueError like php), a numeric string coerces to its` |
|       - | 3584 | ` * number, anything else is a TypeError. Returns RANGE_IN_LONG/DOUBLE, or` |
|       - | 3585 | ` * RANGE_IN_ERROR after throwing (*pRc set).` |
|       - | 3586 | ` */` |
|      62 | 3587 | `static sxu8 RangeStepInput(ph7_context *pCtx,ph7_value *pIn,sxi64 *pLong,double *pDouble,sxi32 *pRc)` |
|       1 | 3588 | `{` |
|      63 | 3589 | `	*pRc = PH7_OK;` |
|      63 | 3590 | `	if( pIn->iFlags & (MEMOBJ_HASHMAP\|MEMOBJ_OBJ\|MEMOBJ_RES) ){` |
|       - | 3591 | `		char zType[80];` |
|       4 | 3592 | `		*pRc = PH7_VmThrowException(pCtx,"TypeError",` |
|       - | 3593 | `			"range(): Argument #3 ($step) must be of type int\|float, %s given",` |
|       1 | 3594 | `			RangeArgTypeName(pIn,zType,sizeof(zType)));` |
|       3 | 3595 | `		return RANGE_IN_ERROR;` |
|       - | 3596 | `	}` |
|      61 | 3597 | `	if( pIn->iFlags & MEMOBJ_NULL ){` |
|       3 | 3598 | `		PH7_VmThrowError(pCtx->pVm,0,E_DEPRECATED,` |
|       - | 3599 | `			"range(): Passing null to parameter #3 ($step) of type int\|float is deprecated");` |
|       3 | 3600 | `		*pLong = 0;` |
|       3 | 3601 | `		return RANGE_IN_LONG;` |
|       - | 3602 | `	}` |
|      59 | 3603 | `	if( pIn->iFlags & MEMOBJ_REAL ){` |
|      25 | 3604 | `		*pDouble = ph7_value_to_double(pIn);` |
|      25 | 3605 | `		return RANGE_IN_DOUBLE;` |
|       - | 3606 | `	}` |
|      35 | 3607 | `	if( pIn->iFlags & MEMOBJ_STRING ){` |
|       - | 3608 | `		const char *zStr;` |
|       - | 3609 | `		int nLen;` |
|       - | 3610 | `		sxu8 iKind;` |
|       3 | 3611 | `		zStr = ph7_value_to_string(pIn,&nLen);` |
|       3 | 3612 | `		iKind = RangeStrToNumber(zStr,(sxu32)nLen,pLong,pDouble);` |
|       3 | 3613 | `		if( iKind == RANGE_IN_ERROR ){` |
|       3 | 3614 | `			*pRc = PH7_VmThrowException(pCtx,"TypeError",` |
|       - | 3615 | `				"range(): Argument #3 ($step) must be of type int\|float, string given");` |
|       1 | 3616 | `		}` |
|       3 | 3617 | `		return iKind;` |
|       - | 3618 | `	}` |
|       - | 3619 | `	/* int / bool */` |
|      33 | 3620 | `	*pLong = ph7_value_to_int64(pIn);` |
|      33 | 3621 | `	return RANGE_IN_LONG;` |
|      32 | 3622 | `}` |
|       - | 3623 | `/*` |
|       - | 3624 | ` * php_range_process_input port: resolve $start/$end into a number and/or a` |
|       - | 3625 | ` * char-range byte, emitting php's exact warnings (empty string, multi-byte` |
|       - | 3626 | ` * string) and ValueErrors (INF/NAN). Returns a RANGE_IN_* code, or` |
|       - | 3627 | ` * RANGE_IN_ERROR after throwing (*pRc set).` |
|       - | 3628 | ` */` |
|     220 | 3629 | `static sxu8 RangeProcessInput(ph7_context *pCtx,ph7_value *pIn,int iArg,const char *zName,` |
|       - | 3630 | `	int bNullCoerced,sxi64 *pLong,double *pDouble,unsigned char *pChar,sxi32 *pRc)` |
|       1 | 3631 | `{` |
|       - | 3632 | `	char zMsg[160];` |
|       - | 3633 | `	double r;` |
|     221 | 3634 | `	*pRc = PH7_OK;` |
|     221 | 3635 | `	if( bNullCoerced ){` |
|       - | 3636 | `		/* ZPP already deprecated the null; it reads as int 0. */` |
|       5 | 3637 | `		*pLong = 0;` |
|       5 | 3638 | `		*pDouble = 0.0;` |
|       5 | 3639 | `		return RANGE_IN_LONG;` |
|       - | 3640 | `	}` |
|     217 | 3641 | `	if( pIn->iFlags & MEMOBJ_REAL ){` |
|      21 | 3642 | `		r = ph7_value_to_double(pIn);` |
|      12 | 3643 | `check_dval:` |
|      25 | 3644 | `		if( PH7_IS_INF(r) ){` |
|       7 | 3645 | `			*pRc = PH7_VmThrowException(pCtx,"ValueError",` |
|       2 | 3646 | `				"range(): Argument #%d ($%s) must be a finite number, INF provided",iArg,zName);` |
|       5 | 3647 | `			return RANGE_IN_ERROR;` |
|       - | 3648 | `		}` |
|      21 | 3649 | `		if( PH7_IS_NAN(r) ){` |
|       7 | 3650 | `			*pRc = PH7_VmThrowException(pCtx,"ValueError",` |
|       2 | 3651 | `				"range(): Argument #%d ($%s) must be a finite number, NAN provided",iArg,zName);` |
|       5 | 3652 | `			return RANGE_IN_ERROR;` |
|       - | 3653 | `		}` |
|      17 | 3654 | `		*pDouble = r;` |
|      17 | 3655 | `		return RANGE_IN_DOUBLE;` |
|       - | 3656 | `	}` |
|     197 | 3657 | `	if( pIn->iFlags & MEMOBJ_STRING ){` |
|       - | 3658 | `		const char *zStr;` |
|       - | 3659 | `		int nLen;` |
|       - | 3660 | `		sxu8 iKind;` |
|      81 | 3661 | `		zStr = ph7_value_to_string(pIn,&nLen);` |
|      81 | 3662 | `		if( nLen == 0 ){` |
|       7 | 3663 | `			SyBufferFormat(zMsg,sizeof(zMsg),` |
|       2 | 3664 | `				"range(): Argument #%d ($%s) must not be empty, casted to 0",iArg,zName);` |
|       5 | 3665 | `			PH7_VmThrowError(pCtx->pVm,0,PH7_CTX_WARNING,zMsg);` |
|       5 | 3666 | `			*pLong = 0;` |
|       5 | 3667 | `			*pDouble = 0.0;` |
|      41 | 3668 | `			return RANGE_IN_LONG;` |
|       - | 3669 | `		}` |
|      77 | 3670 | `		iKind = RangeStrToNumber(zStr,(sxu32)nLen,pLong,pDouble);` |
|      77 | 3671 | `		if( iKind == RANGE_IN_DOUBLE ){` |
|       5 | 3672 | `			r = *pDouble;` |
|       5 | 3673 | `			goto check_dval;` |
|       - | 3674 | `		}` |
|      73 | 3675 | `		if( iKind == RANGE_IN_LONG ){` |
|      23 | 3676 | `			*pDouble = (double)*pLong;` |
|      23 | 3677 | `			if( nLen == 1 ){` |
|       - | 3678 | `				/* A single numeric digit works as both a char and a number. */` |
|       9 | 3679 | `				*pChar = (unsigned char)zStr[0];` |
|       9 | 3680 | `				return RANGE_IN_DIGIT;` |
|       - | 3681 | `			}` |
|      15 | 3682 | `			return RANGE_IN_LONG;` |
|       - | 3683 | `		}` |
|      51 | 3684 | `		if( nLen != 1 ){` |
|      10 | 3685 | `			SyBufferFormat(zMsg,sizeof(zMsg),` |
|       3 | 3686 | `				"range(): Argument #%d ($%s) must be a single byte, subsequent bytes are ignored",iArg,zName);` |
|       7 | 3687 | `			PH7_VmThrowError(pCtx->pVm,0,PH7_CTX_WARNING,zMsg);` |
|       3 | 3688 | `		}` |
|      51 | 3689 | `		*pChar = (unsigned char)zStr[0];` |
|       - | 3690 | `		/* Fall-back numeric value in case the other argument is not a string. */` |
|      51 | 3691 | `		*pLong = 0;` |
|      51 | 3692 | `		*pDouble = 0.0;` |
|      51 | 3693 | `		return RANGE_IN_STRING;` |
|       - | 3694 | `	}` |
|       - | 3695 | `	/* int / bool */` |
|     117 | 3696 | `	*pLong = ph7_value_to_int64(pIn);` |
|     117 | 3697 | `	*pDouble = (double)*pLong;` |
|     117 | 3698 | `	return RANGE_IN_LONG;` |
|     111 | 3699 | `}` |
|       - | 3700 | `/*` |
|       - | 3701 | ` * The two "supplied range exceeds the maximum array size" ValueErrors.` |
|       - | 3702 | ` * Both php messages print the macro's (start,end) parameters, which its` |
|       - | 3703 | ` * callers pass SWAPPED for a decreasing range — a php quirk kept for` |
|       - | 3704 | ` * byte-parity (callers below pass the values to *print*). The int and` |
|       - | 3705 | ` * float variants differ in wording ("Maximum size: N." vs "Max size: N")` |
|       - | 3706 | ` * exactly like php's two macros.` |
|       - | 3707 | ` */` |
|       6 | 3708 | `static sxi32 RangeLongSizeError(ph7_context *pCtx,sxu64 nCalc,sxi64 iStart,sxi64 iEnd,sxi64 iStep)` |
|       1 | 3709 | `{` |
|      10 | 3710 | `	return PH7_VmThrowException(pCtx,"ValueError",` |
|       - | 3711 | `		"The supplied range exceeds the maximum array size by %qu elements: "` |
|       - | 3712 | `		"start=%qd, end=%qd, step=%qd. Calculated size: %qu. Maximum size: %qu.",` |
|       3 | 3713 | `		nCalc - (sxu64)(PH7_RANGE_HT_MAX_SIZE - 1),iStart,iEnd,iStep,` |
|       3 | 3714 | `		nCalc,(sxu64)PH7_RANGE_HT_MAX_SIZE);` |
|       1 | 3715 | `}` |
|       6 | 3716 | `static sxi32 RangeDoubleSizeError(ph7_context *pCtx,double rCalc,double rStart,double rEnd,double rStep)` |
|       1 | 3717 | `{` |
|       - | 3718 | `	/* Four %.1f doubles can reach ~313 bytes each near DBL_MAX, so format on` |
|       - | 3719 | `	 * the VM heap (auto-released with the call context) rather than parking` |
|       - | 3720 | `	 * ~1.5 KB on the native stack of a small-stack embedded port. */` |
|       7 | 3721 | `	const unsigned int nBuf = 1500;` |
|       7 | 3722 | `	char *zMsg = (char *)ph7_context_alloc_chunk(pCtx,nBuf,FALSE,TRUE/* Auto-release */);` |
|       7 | 3723 | `	if( zMsg == 0 ){` |
|     ! 0 | 3724 | `		return PH7_ContextMemoryError(pCtx);` |
|       - | 3725 | `	}` |
|       7 | 3726 | `	snprintf(zMsg,nBuf,` |
|       - | 3727 | `		"The supplied range exceeds the maximum array size by %.1f elements: "` |
|       - | 3728 | `		"start=%.1f, end=%.1f, step=%.1f. Max size: 1073741824",` |
|       - | 3729 | `		rCalc - (double)PH7_RANGE_HT_MAX_SIZE,rStart,rEnd,rStep);` |
|       7 | 3730 | `	return PH7_VmThrowException(pCtx,"ValueError","%s",zMsg);` |
|       4 | 3731 | `}` |
|       - | 3732 | `/*` |
|       - | 3733 | ` * Set the element container to the next range element and append it to the` |
|       - | 3734 | ` * result array, surfacing allocation failure as the OOM fatal (never a` |
|       - | 3735 | ` * silently-truncated array). One helper per element type so the fill loops` |
|       - | 3736 | ` * below stay one line per iteration.` |
|       - | 3737 | ` */` |
|     334 | 3738 | `static sxi32 RangeAppendInt(ph7_context *pCtx,ph7_value *pArray,ph7_value *pValue,sxi64 iVal)` |
|       1 | 3739 | `{` |
|     335 | 3740 | `	ph7_value_int64(pValue,iVal);` |
|     335 | 3741 | `	if( ph7_array_add_elem(pArray,0/* Automatic index assign*/,pValue) != SXRET_OK ){` |
|     ! 0 | 3742 | `		return PH7_ContextMemoryError(pCtx);` |
|       - | 3743 | `	}` |
|     335 | 3744 | `	return PH7_OK;` |
|     168 | 3745 | `}` |
|      70 | 3746 | `static sxi32 RangeAppendDouble(ph7_context *pCtx,ph7_value *pArray,ph7_value *pValue,double rVal)` |
|       1 | 3747 | `{` |
|      71 | 3748 | `	ph7_value_double(pValue,rVal);` |
|      71 | 3749 | `	if( ph7_array_add_elem(pArray,0,pValue) != SXRET_OK ){` |
|     ! 0 | 3750 | `		return PH7_ContextMemoryError(pCtx);` |
|       - | 3751 | `	}` |
|      71 | 3752 | `	return PH7_OK;` |
|      36 | 3753 | `}` |
|     168 | 3754 | `static sxi32 RangeAppendChar(ph7_context *pCtx,ph7_value *pArray,ph7_value *pValue,char c)` |
|       1 | 3755 | `{` |
|     169 | 3756 | `	ph7_value_string(pValue,&c,1);` |
|     169 | 3757 | `	if( ph7_array_add_elem(pArray,0,pValue) != SXRET_OK ){` |
|     ! 0 | 3758 | `		return PH7_ContextMemoryError(pCtx);` |
|       - | 3759 | `	}` |
|     169 | 3760 | `	ph7_value_reset_string_cursor(pValue);` |
|     169 | 3761 | `	return PH7_OK;` |
|      85 | 3762 | `}` |
|       - | 3763 | `/*` |
|       - | 3764 | ` * array range(string\|int\|float $start,string\|int\|float $end,int\|float $step = 1)` |
|       - | 3765 | ` *  Create an array containing a range of elements.` |
|       - | 3766 | ` * Return` |
|       - | 3767 | ` *  An array of elements from start to end, inclusive; int, float, or` |
|       - | 3768 | ` *  single-character string elements depending on the inputs, like php 8.` |
|       - | 3769 | ` */` |
|     136 | 3770 | `static int ph7_hashmap_range(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       1 | 3771 | `{` |
|       - | 3772 | `	ph7_value *pValue,*pArray;` |
|     137 | 3773 | `	sxi32 rc = PH7_OK;` |
|     137 | 3774 | `	int is_step_double = 0,is_step_negative = 0;` |
|     137 | 3775 | `	double step_double = 1.0;` |
|     137 | 3776 | `	sxi64 step = 1;` |
|       - | 3777 | `	sxu8 start_type,end_type;` |
|     137 | 3778 | `	sxi64 start_long = 0,end_long = 0;` |
|     137 | 3779 | `	double start_double = 0.0,end_double = 0.0;` |
|     137 | 3780 | `	unsigned char cStart = 0,cEnd = 0;` |
|     137 | 3781 | `	int bStartNull = FALSE,bEndNull = FALSE;` |
|       - | 3782 | `	sxu32 i,size;` |
|       - | 3783 |  |
|       - | 3784 | `	/* php ZPP arity: at least 2 (enforced centrally, aBuiltinArity), at most 3. */` |
|     137 | 3785 | `	if( nArg > 3 ){` |
|       4 | 3786 | `		return PH7_VmThrowException(pCtx,"ArgumentCountError",` |
|       1 | 3787 | `			"range() expects at most 3 arguments, %d given",nArg);` |
|       - | 3788 | `	}` |
|     135 | 3789 | `	if( nArg < 2 ){` |
|       - | 3790 | `		/* Defensive only: the central arity table throws before we run. */` |
|     ! 0 | 3791 | `		return PH7_VmThrowException(pCtx,"ArgumentCountError",` |
|     ! 0 | 3792 | `			"range() expects at least 2 arguments, %d given",nArg);` |
|       - | 3793 | `	}` |
|       - | 3794 | `	/* ZPP pass in argument order: type errors and null deprecations fire` |
|       - | 3795 | `	 * before any value/domain check, like php's zend_parse_parameters. */` |
|     135 | 3796 | `	if( !RangeEndpointZpp(pCtx,apArg[0],1,"start",&bStartNull,&rc) ){` |
|       7 | 3797 | `		return rc;` |
|       - | 3798 | `	}` |
|     129 | 3799 | `	if( !RangeEndpointZpp(pCtx,apArg[1],2,"end",&bEndNull,&rc) ){` |
|     ! 0 | 3800 | `		return rc;` |
|       - | 3801 | `	}` |
|     129 | 3802 | `	if( nArg > 2 ){` |
|      63 | 3803 | `		sxu8 iStepKind = RangeStepInput(pCtx,apArg[2],&step,&step_double,&rc);` |
|      63 | 3804 | `		if( iStepKind == RANGE_IN_ERROR ){` |
|       5 | 3805 | `			return rc;` |
|       - | 3806 | `		}` |
|      59 | 3807 | `		if( iStepKind == RANGE_IN_DOUBLE ){` |
|      25 | 3808 | `			if( PH7_IS_INF(step_double) ){` |
|       3 | 3809 | `				return PH7_VmThrowException(pCtx,"ValueError",` |
|       - | 3810 | `					"range(): Argument #3 ($step) must be a finite number, INF provided");` |
|       - | 3811 | `			}` |
|      23 | 3812 | `			if( PH7_IS_NAN(step_double) ){` |
|       3 | 3813 | `				return PH7_VmThrowException(pCtx,"ValueError",` |
|       - | 3814 | `					"range(): Argument #3 ($step) must be a finite number, NAN provided");` |
|       - | 3815 | `			}` |
|       - | 3816 | `			/* We only want positive step values. */` |
|      21 | 3817 | `			if( step_double < 0.0 ){` |
|     ! 0 | 3818 | `				is_step_negative = 1;` |
|     ! 0 | 3819 | `				step_double *= -1;` |
|     ! 0 | 3820 | `			}` |
|       - | 3821 | `			/* zend_dval_to_lval_silent + zend_is_long_compatible: an integral` |
|       - | 3822 | `			 * in-sxi64-range float step behaves as an int (char ranges accept` |
|       - | 3823 | `			 * it, int endpoints stay int); anything else is a float step. */` |
|      21 | 3824 | `			if( step_double < 9223372036854775808.0 ){` |
|      19 | 3825 | `				step = (sxi64)step_double;` |
|      19 | 3826 | `				if( (double)step != step_double ){` |
|      17 | 3827 | `					is_step_double = 1;` |
|       8 | 3828 | `				}` |
|      10 | 3829 | `			}else{` |
|       - | 3830 | ``				/* Casting out-of-range would be UB; `step` stays unread —`` |
|       - | 3831 | `				 * every reader is gated behind !is_step_double. */` |
|       3 | 3832 | `				is_step_double = 1;` |
|       - | 3833 | `			}` |
|      11 | 3834 | `		}else{` |
|       - | 3835 | `			/* We only want positive step values. */` |
|      35 | 3836 | `			if( step < 0 ){` |
|      11 | 3837 | `				if( step == SMALLEST_INT64 ){` |
|       - | 3838 | `					/* -step would overflow */` |
|       4 | 3839 | `					return PH7_VmThrowException(pCtx,"ValueError",` |
|       1 | 3840 | `						"range(): Argument #3 ($step) must be greater than %qd",step);` |
|       - | 3841 | `				}` |
|       9 | 3842 | `				is_step_negative = 1;` |
|       9 | 3843 | `				step = -step;` |
|       4 | 3844 | `			}` |
|      33 | 3845 | `			step_double = (double)step;` |
|       - | 3846 | `		}` |
|      53 | 3847 | `		if( step_double == 0.0 ){` |
|       7 | 3848 | `			return PH7_VmThrowException(pCtx,"ValueError",` |
|       - | 3849 | `				"range(): Argument #3 ($step) cannot be 0");` |
|       - | 3850 | `		}` |
|      23 | 3851 | `	}` |
|     113 | 3852 | `	start_type = RangeProcessInput(pCtx,apArg[0],1,"start",bStartNull,&start_long,&start_double,&cStart,&rc);` |
|     113 | 3853 | `	if( start_type == RANGE_IN_ERROR ){` |
|       5 | 3854 | `		return rc;` |
|       - | 3855 | `	}` |
|     109 | 3856 | `	end_type = RangeProcessInput(pCtx,apArg[1],2,"end",bEndNull,&end_long,&end_double,&cEnd,&rc);` |
|     109 | 3857 | `	if( end_type == RANGE_IN_ERROR ){` |
|       5 | 3858 | `		return rc;` |
|       - | 3859 | `	}` |
|       - | 3860 | `	/* Element container + result array */` |
|     105 | 3861 | `	pValue = ph7_context_new_scalar(pCtx);` |
|     105 | 3862 | `	pArray = ph7_context_new_array(pCtx);` |
|     105 | 3863 | `	if( pValue == 0 \|\| pArray == 0 ){` |
|     ! 0 | 3864 | `		return PH7_ContextMemoryError(pCtx);` |
|       - | 3865 | `	}` |
|       - | 3866 | `	/* If the range is given as strings, generate an array of characters. */` |
|     105 | 3867 | `	if( start_type >= RANGE_IN_STRING \|\| end_type >= RANGE_IN_STRING ){` |
|      37 | 3868 | `		if( start_type < RANGE_IN_STRING \|\| end_type < RANGE_IN_STRING ){` |
|       - | 3869 | `			/* Only one side is a string: the char side converts to 0 (with a` |
|       - | 3870 | `			 * warning unless the numeric side is an ambiguous single digit)` |
|       - | 3871 | `			 * and the range is numeric. */` |
|      15 | 3872 | `			if( start_type < RANGE_IN_STRING ){` |
|       7 | 3873 | `				if( end_type != RANGE_IN_DIGIT ){` |
|       7 | 3874 | `					PH7_VmThrowError(pCtx->pVm,0,PH7_CTX_WARNING,` |
|       - | 3875 | `						"range(): Argument #1 ($start) must be a single byte string if"` |
|       - | 3876 | `						" argument #2 ($end) is a single byte string, argument #2 ($end) converted to 0");` |
|       3 | 3877 | `				}` |
|       7 | 3878 | `				end_type = RANGE_IN_LONG;` |
|       4 | 3879 | `			}else{` |
|       9 | 3880 | `				if( start_type != RANGE_IN_DIGIT ){` |
|       9 | 3881 | `					PH7_VmThrowError(pCtx->pVm,0,PH7_CTX_WARNING,` |
|       - | 3882 | `						"range(): Argument #2 ($end) must be a single byte string if"` |
|       - | 3883 | `						" argument #1 ($start) is a single byte string, argument #1 ($start) converted to 0");` |
|       4 | 3884 | `				}` |
|       9 | 3885 | `				start_type = RANGE_IN_LONG;` |
|       - | 3886 | `			}` |
|      15 | 3887 | `			goto handle_numeric_inputs;` |
|       - | 3888 | `		}` |
|      23 | 3889 | `		if( is_step_double ){` |
|       - | 3890 | `			/* Only emit the warning if one of the inputs is not a numeric digit. */` |
|       5 | 3891 | `			if( start_type == RANGE_IN_STRING \|\| end_type == RANGE_IN_STRING ){` |
|       3 | 3892 | `				PH7_VmThrowError(pCtx->pVm,0,PH7_CTX_WARNING,` |
|       - | 3893 | `					"range(): Argument #3 ($step) must be of type int when generating an array"` |
|       - | 3894 | `					" of characters, inputs converted to 0");` |
|       1 | 3895 | `			}` |
|       5 | 3896 | `			start_type = RANGE_IN_LONG;` |
|       5 | 3897 | `			end_type = RANGE_IN_LONG;` |
|       5 | 3898 | `			goto handle_numeric_inputs;` |
|       - | 3899 | `		}` |
|       - | 3900 | `		/* Generate an array of characters */` |
|      19 | 3901 | `		if( cStart > cEnd ){` |
|       - | 3902 | `			/* Decreasing char range */` |
|       - | 3903 | `			int iCur;` |
|       3 | 3904 | `			if( (sxi64)(cStart - cEnd) < step ){` |
|     ! 0 | 3905 | `				goto boundary_error;` |
|       - | 3906 | `			}` |
|      17 | 3907 | `			for( iCur = (int)cStart ; iCur >= (int)cEnd ; iCur -= (int)step ){` |
|      15 | 3908 | `				if( (rc = RangeAppendChar(pCtx,pArray,pValue,(char)iCur)) != PH7_OK ){` |
|     ! 0 | 3909 | `					return rc;` |
|       - | 3910 | `				}` |
|       8 | 3911 | `			}` |
|      18 | 3912 | `		}else if( cEnd > cStart ){` |
|       - | 3913 | `			/* Increasing char range */` |
|       - | 3914 | `			int iCur;` |
|      15 | 3915 | `			if( is_step_negative ){` |
|       3 | 3916 | `				goto negative_step_error;` |
|       - | 3917 | `			}` |
|      13 | 3918 | `			if( (sxi64)(cEnd - cStart) < step ){` |
|       3 | 3919 | `				goto boundary_error;` |
|       - | 3920 | `			}` |
|     163 | 3921 | `			for( iCur = (int)cStart ; iCur <= (int)cEnd ; iCur += (int)step ){` |
|     153 | 3922 | `				if( (rc = RangeAppendChar(pCtx,pArray,pValue,(char)iCur)) != PH7_OK ){` |
|     ! 0 | 3923 | `					return rc;` |
|       - | 3924 | `				}` |
|      77 | 3925 | `			}` |
|       6 | 3926 | `		}else{` |
|       3 | 3927 | `			if( (rc = RangeAppendChar(pCtx,pArray,pValue,(char)cStart)) != PH7_OK ){` |
|     ! 0 | 3928 | `				return rc;` |
|       - | 3929 | `			}` |
|       - | 3930 | `		}` |
|      15 | 3931 | `		ph7_result_value(pCtx,pArray);` |
|      15 | 3932 | `		return PH7_OK;` |
|       - | 3933 | `	}` |
|      34 | 3934 | `handle_numeric_inputs:` |
|      95 | 3935 | `	if( start_type == RANGE_IN_DOUBLE \|\| end_type == RANGE_IN_DOUBLE \|\| is_step_double ){` |
|       - | 3936 | `		/* Float range */` |
|       - | 3937 | `		double elem,calc;` |
|      25 | 3938 | `		if( start_double > end_double ){` |
|       - | 3939 | `			/* Decreasing float range */` |
|       7 | 3940 | `			if( start_double - end_double < step_double ){` |
|     ! 0 | 3941 | `				goto boundary_error;` |
|       - | 3942 | `			}` |
|       7 | 3943 | `			calc = ((start_double - end_double) / step_double) + 1;` |
|       7 | 3944 | `			if( calc >= (double)PH7_RANGE_HT_MAX_SIZE ){` |
|       - | 3945 | `				/* php prints start/end swapped here (see RangeDoubleSizeError). */` |
|       3 | 3946 | `				return RangeDoubleSizeError(pCtx,calc,end_double,start_double,step_double);` |
|       - | 3947 | `			}` |
|       5 | 3948 | `			size = (sxu32)(calc + 0.5); /* _php_math_round(...,0,HALF_UP) */` |
|      19 | 3949 | `			for( i = 0,elem = start_double ; i < size && elem >= end_double ; ++i,elem = start_double - ((double)i * step_double) ){` |
|      15 | 3950 | `				if( (rc = RangeAppendDouble(pCtx,pArray,pValue,elem)) != PH7_OK ){` |
|     ! 0 | 3951 | `					return rc;` |
|       - | 3952 | `				}` |
|       8 | 3953 | `			}` |
|      21 | 3954 | `		}else if( end_double > start_double ){` |
|       - | 3955 | `			/* Increasing float range */` |
|      17 | 3956 | `			if( is_step_negative ){` |
|     ! 0 | 3957 | `				goto negative_step_error;` |
|       - | 3958 | `			}` |
|      17 | 3959 | `			if( end_double - start_double < step_double ){` |
|       3 | 3960 | `				goto boundary_error;` |
|       - | 3961 | `			}` |
|      15 | 3962 | `			calc = ((end_double - start_double) / step_double) + 1;` |
|      15 | 3963 | `			if( calc >= (double)PH7_RANGE_HT_MAX_SIZE ){` |
|       5 | 3964 | `				return RangeDoubleSizeError(pCtx,calc,start_double,end_double,step_double);` |
|       - | 3965 | `			}` |
|      11 | 3966 | `			size = (sxu32)(calc + 0.5);` |
|      65 | 3967 | `			for( i = 0,elem = start_double ; i < size && elem <= end_double ; ++i,elem = start_double + ((double)i * step_double) ){` |
|      55 | 3968 | `				if( (rc = RangeAppendDouble(pCtx,pArray,pValue,elem)) != PH7_OK ){` |
|     ! 0 | 3969 | `					return rc;` |
|       - | 3970 | `				}` |
|      28 | 3971 | `			}` |
|       6 | 3972 | `		}else{` |
|       3 | 3973 | `			if( (rc = RangeAppendDouble(pCtx,pArray,pValue,start_double)) != PH7_OK ){` |
|     ! 0 | 3974 | `				return rc;` |
|       - | 3975 | `			}` |
|       - | 3976 | `		}` |
|       9 | 3977 | `	}else{` |
|       - | 3978 | `		/* Int range. All arithmetic in unsigned space so a span wider than` |
|       - | 3979 | `		 * LARGEST_INT64 (e.g. -PHP_INT_MAX..PHP_INT_MAX) wraps correctly` |
|       - | 3980 | `		 * instead of overflowing, exactly like php's zend_ulong math. */` |
|      63 | 3981 | `		sxu64 ustep = (sxu64)step;` |
|       - | 3982 | `		sxu64 calc;` |
|      63 | 3983 | `		if( start_long > end_long ){` |
|       - | 3984 | `			/* Decreasing int range */` |
|      19 | 3985 | `			if( (sxu64)start_long - (sxu64)end_long < ustep ){` |
|       3 | 3986 | `				goto boundary_error;` |
|       - | 3987 | `			}` |
|      17 | 3988 | `			calc = ((sxu64)start_long - (sxu64)end_long) / ustep;` |
|      17 | 3989 | `			if( calc >= (sxu64)(PH7_RANGE_HT_MAX_SIZE - 1) ){` |
|       - | 3990 | `				/* php prints start/end swapped here (see RangeLongSizeError). */` |
|       3 | 3991 | `				return RangeLongSizeError(pCtx,calc,end_long,start_long,step);` |
|       - | 3992 | `			}` |
|      15 | 3993 | `			size = (sxu32)(calc + 1);` |
|     101 | 3994 | `			for( i = 0 ; i < size ; ++i ){` |
|      87 | 3995 | `				if( (rc = RangeAppendInt(pCtx,pArray,pValue,(sxi64)((sxu64)start_long - (sxu64)i * ustep))) != PH7_OK ){` |
|     ! 0 | 3996 | `					return rc;` |
|       - | 3997 | `				}` |
|      44 | 3998 | `			}` |
|      52 | 3999 | `		}else if( end_long > start_long ){` |
|       - | 4000 | `			/* Increasing int range */` |
|      39 | 4001 | `			if( is_step_negative ){` |
|       3 | 4002 | `				goto negative_step_error;` |
|       - | 4003 | `			}` |
|      37 | 4004 | `			if( (sxu64)end_long - (sxu64)start_long < ustep ){` |
|       3 | 4005 | `				goto boundary_error;` |
|       - | 4006 | `			}` |
|      35 | 4007 | `			calc = ((sxu64)end_long - (sxu64)start_long) / ustep;` |
|      35 | 4008 | `			if( calc >= (sxu64)(PH7_RANGE_HT_MAX_SIZE - 1) ){` |
|       5 | 4009 | `				return RangeLongSizeError(pCtx,calc,start_long,end_long,step);` |
|       - | 4010 | `			}` |
|      31 | 4011 | `			size = (sxu32)(calc + 1);` |
|     273 | 4012 | `			for( i = 0 ; i < size ; ++i ){` |
|     243 | 4013 | `				if( (rc = RangeAppendInt(pCtx,pArray,pValue,(sxi64)((sxu64)start_long + (sxu64)i * ustep))) != PH7_OK ){` |
|     ! 0 | 4014 | `					return rc;` |
|       - | 4015 | `				}` |
|     122 | 4016 | `			}` |
|      16 | 4017 | `		}else{` |
|       7 | 4018 | `			if( (rc = RangeAppendInt(pCtx,pArray,pValue,start_long)) != PH7_OK ){` |
|     ! 0 | 4019 | `				return rc;` |
|       - | 4020 | `			}` |
|       - | 4021 | `		}` |
|       - | 4022 | `	}` |
|       - | 4023 | `	/* Return the new array. 'pValue' is released automatically by the` |
|       - | 4024 | `	 * virtual machine as soon as we return from this foreign function. */` |
|      67 | 4025 | `	ph7_result_value(pCtx,pArray);` |
|      67 | 4026 | `	return PH7_OK;` |
|       2 | 4027 | `negative_step_error:` |
|       5 | 4028 | `	return PH7_VmThrowException(pCtx,"ValueError",` |
|       - | 4029 | `		"range(): Argument #3 ($step) must be greater than 0 for increasing ranges");` |
|       4 | 4030 | `boundary_error:` |
|       9 | 4031 | `	return PH7_VmThrowException(pCtx,"ValueError",` |
|       - | 4032 | `		"range(): Argument #3 ($step) must be less than the range spanned by argument #1 ($start) and argument #2 ($end)");` |
|      69 | 4033 | `}` |
|       - | 4034 | `/*` |
|       - | 4035 | ` * array array_values(array $array)` |
|       - | 4036 | ` *  Return all the values of an array, indexed numerically.` |
|       - | 4037 | ` * Parameters` |
|       - | 4038 | ` *  $array` |
|       - | 4039 | ` *   The input array.` |
|       - | 4040 | ` * Return` |
|       - | 4041 | ` *  An indexed array of values or NULL on allocation failure.` |
|       - | 4042 | ` */` |
|      36 | 4043 | `static int ph7_hashmap_values(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       4 | 4044 | `{` |
|       - | 4045 | `	ph7_hashmap_node *pNode;` |
|       - | 4046 | `	ph7_hashmap *pMap;` |
|       - | 4047 | `	ph7_value *pArray;` |
|       - | 4048 | `	ph7_value *pObj;` |
|       - | 4049 | `	sxu32 n;` |
|      40 | 4050 | `	if( nArg != 1 ){` |
|       - | 4051 | `		/* Wrong argument count, throw ArgumentCountError */` |
|       8 | 4052 | `		return PH7_VmThrowException(pCtx,` |
|       - | 4053 | `			"ArgumentCountError",` |
|       - | 4054 | `			"array_values() expects exactly 1 argument, %d given",` |
|       2 | 4055 | `			nArg` |
|       - | 4056 | `			);` |
|       - | 4057 | `	}` |
|       - | 4058 | `	/* Make sure we are dealing with a valid hashmap */` |
|      35 | 4059 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|       - | 4060 | `		/* Type mismatch, throw TypeError */` |
|       4 | 4061 | `		return PH7_VmThrowException(pCtx,` |
|       - | 4062 | `			"TypeError",` |
|       - | 4063 | `			"array_values(): Argument #1 ($array) must be of type array, %s given",` |
|       1 | 4064 | `			ph7_type_name(apArg[0])` |
|       - | 4065 | `			);` |
|       - | 4066 | `	}` |
|       - | 4067 | `	/* Point to the internal representation that describe the input hashmap */` |
|      32 | 4068 | `	pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|       - | 4069 | `	/* Create a new array */` |
|      32 | 4070 | `	pArray = ph7_context_new_array(pCtx);` |
|      32 | 4071 | `	if( pArray == 0 ){` |
|     ! 0 | 4072 | `		ph7_result_null(pCtx);` |
|     ! 0 | 4073 | `		return PH7_OK;` |
|       - | 4074 | `	}` |
|       - | 4075 | `	/* Perform the requested operation */` |
|      32 | 4076 | `	pNode = pMap->pFirst;` |
|     104 | 4077 | `	for( n = 0 ; n < pMap->nEntry ; ++n ){` |
|      74 | 4078 | `		pObj = HashmapExtractNodeValue(pNode);` |
|      74 | 4079 | `		if( pObj ){` |
|       - | 4080 | `			/* perform the insertion */` |
|      74 | 4081 | `			ph7_array_add_elem(pArray,0/* Automatic index assign */,pObj);` |
|      36 | 4082 | `		}` |
|       - | 4083 | `		/* Point to the next entry */` |
|      74 | 4084 | `		pNode = pNode->pPrev; /* Reverse link */` |
|      38 | 4085 | `	}` |
|       - | 4086 | `	/* return the new array */` |
|      32 | 4087 | `	ph7_result_value(pCtx,pArray);` |
|      32 | 4088 | `	return PH7_OK;` |
|      22 | 4089 | `}` |
|       - | 4090 | `/*` |
|       - | 4091 | ` * array array_keys(array $input [, val $search_value [, bool $strict = false ]] )` |
|       - | 4092 | ` *  Return all the keys or a subset of the keys of an array.` |
|       - | 4093 | ` * Parameters` |
|       - | 4094 | ` *  $input` |
|       - | 4095 | ` *   An array containing keys to return.` |
|       - | 4096 | ` * $search_value` |
|       - | 4097 | ` *   If specified, then only keys containing these values are returned.` |
|       - | 4098 | ` * $strict` |
|       - | 4099 | ` *   Determines if strict comparison (===) should be used during the search.` |
|       - | 4100 | ` * Return` |
|       - | 4101 | ` *  An array of all the keys in input or NULL on failure.` |
|       - | 4102 | ` */` |
|     144 | 4103 | `static int ph7_hashmap_keys(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       5 | 4104 | `{` |
|       - | 4105 | `	ph7_hashmap_node *pNode;` |
|       - | 4106 | `	ph7_hashmap *pMap;` |
|       - | 4107 | `	ph7_value *pArray;` |
|       - | 4108 | `	ph7_value sObj;` |
|       - | 4109 | `	ph7_value sVal;` |
|       - | 4110 | `	SyString sKey;` |
|       - | 4111 | `	int bStrict;` |
|       - | 4112 | `	sxi32 rc;` |
|       - | 4113 | `	sxu32 n;` |
|     149 | 4114 | `	if( nArg < 1 ){` |
|       - | 4115 | `		/* Missing argument,throw ArgumentCountError */` |
|       3 | 4116 | `		return PH7_VmThrowException(pCtx,` |
|       - | 4117 | `			"ArgumentCountError",` |
|       - | 4118 | `			"array_keys() expects at least 1 argument, 0 given"` |
|       - | 4119 | `			);` |
|       - | 4120 | `	}` |
|       - | 4121 | `	/* Make sure we are dealing with a valid hashmap */` |
|     146 | 4122 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|       - | 4123 | `		/* haystack must be an array,throw TypeError */` |
|       4 | 4124 | `		return PH7_VmThrowException(pCtx,` |
|       - | 4125 | `			"TypeError",` |
|       - | 4126 | `			"array_keys(): Argument #1 ($array) must be of type array, %s given",` |
|       1 | 4127 | `			ph7_type_name(apArg[0])` |
|       - | 4128 | `			);` |
|       - | 4129 | `	}` |
|       - | 4130 | `	/* Point to the internal representation of the input hashmap */` |
|     144 | 4131 | `	pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|       - | 4132 | `	/* Create a new array */` |
|     144 | 4133 | `	pArray = ph7_context_new_array(pCtx);` |
|     144 | 4134 | `	if( pArray == 0 ){` |
|     ! 0 | 4135 | `		ph7_result_null(pCtx);` |
|     ! 0 | 4136 | `		return PH7_OK;` |
|       - | 4137 | `	}` |
|     144 | 4138 | `	bStrict = FALSE;` |
|     144 | 4139 | `	if( nArg > 2 ){` |
|       - | 4140 | `		/* In PHP, non-scalar values for a bool-hinted parameter raise TypeError */` |
|       8 | 4141 | `		if( ph7_value_is_array(apArg[2]) \|\| ph7_value_is_object(apArg[2]) \|\| ph7_value_is_resource(apArg[2]) ){` |
|       4 | 4142 | `			return PH7_VmThrowException(pCtx,` |
|       - | 4143 | `				"TypeError",` |
|       - | 4144 | `				"array_keys(): Argument #3 ($strict) must be of type bool, %s given",` |
|       2 | 4145 | `				ph7_type_name(apArg[2])` |
|       - | 4146 | `				);` |
|       - | 4147 | `		}` |
|       5 | 4148 | `		bStrict = ph7_value_to_bool(apArg[2]);` |
|       2 | 4149 | `	}` |
|       - | 4150 | `	/* Perform the requested operation */` |
|     141 | 4151 | `	pNode = pMap->pFirst;` |
|     141 | 4152 | `	PH7_MemObjInit(pMap->pVm,&sVal);` |
|    1363 | 4153 | `	for( n = 0 ; n < pMap->nEntry ; ++n ){` |
|    1225 | 4154 | `		if( pNode->iType == HASHMAP_INT_NODE ){` |
|     133 | 4155 | `			PH7_MemObjInitFromInt(pMap->pVm,&sObj,pNode->xKey.iKey);` |
|      68 | 4156 | `		}else{` |
|    1094 | 4157 | `			SyStringInitFromBuf(&sKey,SyBlobData(&pNode->xKey.sKey),SyBlobLength(&pNode->xKey.sKey));` |
|    1094 | 4158 | `			PH7_MemObjInitFromString(pMap->pVm,&sObj,&sKey);` |
|       - | 4159 | `		}` |
|    1225 | 4160 | `		rc = 0;` |
|    1225 | 4161 | `		if( nArg > 1 ){` |
|      31 | 4162 | `			ph7_value *pValue = HashmapExtractNodeValue(pNode);` |
|      31 | 4163 | `			if( pValue ){` |
|      31 | 4164 | `				PH7_MemObjLoad(pValue,&sVal);` |
|       - | 4165 | `				/* Filter key */` |
|      31 | 4166 | `				rc = ph7_value_compare(&sVal,apArg[1],bStrict);` |
|      31 | 4167 | `				PH7_MemObjRelease(&sVal);` |
|      15 | 4168 | `			}` |
|      15 | 4169 | `		}` |
|    1225 | 4170 | `		if( rc == 0 ){` |
|       - | 4171 | `			/* Perform the insertion */` |
|    1207 | 4172 | `			ph7_array_add_elem(pArray,0,&sObj);` |
|     602 | 4173 | `		}` |
|    1225 | 4174 | `		PH7_MemObjRelease(&sObj);` |
|       - | 4175 | `		/* Point to the next entry */` |
|    1225 | 4176 | `		pNode = pNode->pPrev; /* Reverse link */` |
|     614 | 4177 | `	}` |
|       - | 4178 | `	/* return the new array */` |
|     141 | 4179 | `	ph7_result_value(pCtx,pArray);` |
|     141 | 4180 | `	return PH7_OK;` |
|      77 | 4181 | `}` |
|       - | 4182 | `/*` |
|       - | 4183 | ` * bool array_same(array $arr1,array $arr2)` |
|       - | 4184 | ` *  Return TRUE if the given arrays are the same instance.` |
|       - | 4185 | ` *  This function is useful under PH7 since arrays are passed` |
|       - | 4186 | ` *  by reference unlike the zend engine which use pass by values.` |
|       - | 4187 | ` * Parameters` |
|       - | 4188 | ` *  $arr1` |
|       - | 4189 | ` *   First array` |
|       - | 4190 | ` *  $arr2` |
|       - | 4191 | ` *   Second array` |
|       - | 4192 | ` * Return` |
|       - | 4193 | ` *  TRUE if the arrays are the same instance.FALSE otherwise.` |
|       - | 4194 | ` * Note` |
|       - | 4195 | ` *  This function is a symisc eXtension.` |
|       - | 4196 | ` */` |
|       4 | 4197 | `static int ph7_hashmap_same(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       1 | 4198 | `{` |
|       - | 4199 | `	ph7_hashmap *p1,*p2;` |
|       - | 4200 | `	int rc;` |
|       5 | 4201 | `	if( nArg < 2 \|\| !ph7_value_is_array(apArg[0]) \|\| !ph7_value_is_array(apArg[1]) ){` |
|       - | 4202 | `		/* Missing or invalid arguments,return FALSE*/` |
|     ! 0 | 4203 | `		ph7_result_bool(pCtx,0);` |
|     ! 0 | 4204 | `		return PH7_OK;` |
|       - | 4205 | `	}` |
|       - | 4206 | `	/* Point to the hashmaps */` |
|       5 | 4207 | `	p1 = (ph7_hashmap *)apArg[0]->x.pOther;` |
|       5 | 4208 | `	p2 = (ph7_hashmap *)apArg[1]->x.pOther;` |
|       5 | 4209 | `	rc = (p1 == p2);` |
|       - | 4210 | `	/* Same instance? */` |
|       5 | 4211 | `	ph7_result_bool(pCtx,rc);` |
|       5 | 4212 | `	return PH7_OK;` |
|       3 | 4213 | `}` |
|       - | 4214 | `/*` |
|       - | 4215 | ` * array array_merge(array ...$arrays)` |
|       - | 4216 | ` *  Merge one or more arrays.` |
|       - | 4217 | ` * Parameters` |
|       - | 4218 | ` *  ...$arrays` |
|       - | 4219 | ` *   Variable list of arrays to merge. Each argument must be an array;` |
|       - | 4220 | ` *   passing a non-array argument throws a TypeError.` |
|       - | 4221 | ` * Return` |
|       - | 4222 | ` *  The resulting merged array. Returns an empty array when called` |
|       - | 4223 | ` *  with no arguments.` |
|       - | 4224 | ` */` |
|    1026 | 4225 | `static int ph7_hashmap_merge(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       5 | 4226 | `{` |
|       - | 4227 | `	ph7_hashmap *pMap,*pSrc;` |
|       - | 4228 | `	ph7_value *pArray;` |
|       - | 4229 | `	int i;` |
|       - | 4230 | `	/* Create a new array */` |
|    1031 | 4231 | `	pArray = ph7_context_new_array(pCtx);` |
|    1031 | 4232 | `	if( pArray == 0 ){` |
|     ! 0 | 4233 | `		ph7_result_null(pCtx);` |
|     ! 0 | 4234 | `		return PH7_OK;` |
|       - | 4235 | `	}` |
|       - | 4236 | `	/* Point to the internal representation of the hashmap */` |
|    1031 | 4237 | `	pMap = (ph7_hashmap *)pArray->x.pOther;` |
|       - | 4238 | `	/* Start merging */` |
|    3073 | 4239 | `	for( i = 0 ; i < nArg ; i++ ){` |
|       - | 4240 | `		/* Make sure we are dealing with a valid hashmap */` |
|    2051 | 4241 | `		if( !ph7_value_is_array(apArg[i]) ){` |
|       - | 4242 | `			/* Type mismatch -> TypeError */` |
|       8 | 4243 | `			return PH7_VmThrowException(pCtx,` |
|       - | 4244 | `				"TypeError",` |
|       - | 4245 | `				"array_merge(): Argument #%d must be of type array, %s given",` |
|       2 | 4246 | `				i + 1,` |
|       4 | 4247 | `				ph7_type_name(apArg[i])` |
|       - | 4248 | `				);` |
|     ! 0 | 4249 | `		}else{` |
|    2047 | 4250 | `			pSrc = (ph7_hashmap *)apArg[i]->x.pOther;` |
|       - | 4251 | `			/* Merge the two hashmaps */` |
|    2047 | 4252 | `			HashmapMerge(pSrc,pMap);` |
|       - | 4253 | `		}` |
|    1026 | 4254 | `	}` |
|       - | 4255 | `	/* Return the freshly created array */` |
|    1027 | 4256 | `	ph7_result_value(pCtx,pArray);` |
|    1027 | 4257 | `	return PH7_OK;` |
|     518 | 4258 | `}` |
|       - | 4259 | `/*` |
|       - | 4260 | ` * array array_copy(array $source)` |
|       - | 4261 | ` *  Make a blind copy of the target array.` |
|       - | 4262 | ` * Parameters` |
|       - | 4263 | ` *  $source` |
|       - | 4264 | ` *   Target array` |
|       - | 4265 | ` * Return` |
|       - | 4266 | ` *  Copy of the target array on success.NULL otherwise.` |
|       - | 4267 | ` * Note` |
|       - | 4268 | ` *  This function is a symisc eXtension.` |
|       - | 4269 | ` */` |
|      16 | 4270 | `static int ph7_hashmap_copy(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       1 | 4271 | `{` |
|       - | 4272 | `	ph7_hashmap *pMap;` |
|       - | 4273 | `	ph7_value *pArray;` |
|      17 | 4274 | `	if( nArg < 1 ){` |
|       - | 4275 | `		/* Missing arguments,return NULL */` |
|     ! 0 | 4276 | `		ph7_result_null(pCtx);` |
|     ! 0 | 4277 | `		return PH7_OK;` |
|       - | 4278 | `	}` |
|       - | 4279 | `	/* Create a new array */` |
|      17 | 4280 | `	pArray = ph7_context_new_array(pCtx);` |
|      17 | 4281 | `	if( pArray == 0 ){` |
|     ! 0 | 4282 | `		ph7_result_null(pCtx);` |
|     ! 0 | 4283 | `		return PH7_OK;` |
|       - | 4284 | `	}` |
|       - | 4285 | `	/* Point to the internal representation of the hashmap */` |
|      17 | 4286 | `	pMap = (ph7_hashmap *)pArray->x.pOther;` |
|      17 | 4287 | `	if( ph7_value_is_array(apArg[0])){` |
|       - | 4288 | `		/* Point to the internal representation of the source */` |
|      17 | 4289 | `		ph7_hashmap *pSrc = (ph7_hashmap *)apArg[0]->x.pOther;` |
|       - | 4290 | `		/* Perform the copy */` |
|      17 | 4291 | `		PH7_HashmapDup(pSrc,pMap);` |
|       9 | 4292 | `	}else{` |
|       - | 4293 | `		/* Simple insertion */` |
|     ! 0 | 4294 | `		PH7_HashmapInsert(pMap,0/* Automatic index assign*/,apArg[0]);` |
|       - | 4295 | `	}` |
|       - | 4296 | `	/* Return the duplicated array */` |
|      17 | 4297 | `	ph7_result_value(pCtx,pArray);` |
|      17 | 4298 | `	return PH7_OK;` |
|       9 | 4299 | `}` |
|       - | 4300 | `/*` |
|       - | 4301 | ` * bool array_erase(array $source)` |
|       - | 4302 | ` *  Remove all elements from a given array.` |
|       - | 4303 | ` * Parameters` |
|       - | 4304 | ` *  $source` |
|       - | 4305 | ` *   Target array` |
|       - | 4306 | ` * Return` |
|       - | 4307 | ` *  TRUE on success.FALSE otherwise.` |
|       - | 4308 | ` * Note` |
|       - | 4309 | ` *  This function is a symisc eXtension.` |
|       - | 4310 | ` */` |
|      16 | 4311 | `static int ph7_hashmap_erase(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       1 | 4312 | `{` |
|       - | 4313 | `	ph7_hashmap *pMap;` |
|      17 | 4314 | `	if( nArg < 1 ){` |
|       - | 4315 | `		/* Missing arguments */` |
|     ! 0 | 4316 | `		ph7_result_bool(pCtx,0);` |
|     ! 0 | 4317 | `		return PH7_OK;` |
|       - | 4318 | `	}` |
|       - | 4319 | `	/* Point to the target hashmap */` |
|      17 | 4320 | `	PH7_HashmapCowSeparate(pCtx->pVm, apArg[0]);` |
|      17 | 4321 | `	pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|       - | 4322 | `	/* Erase */` |
|      17 | 4323 | `	PH7_HashmapRelease(pMap,FALSE);` |
|      17 | 4324 | `	return PH7_OK;` |
|       9 | 4325 | `}` |
|       - | 4326 | `/*` |
|       - | 4327 | ` * array array_slice(array $array, int $offset [, ?int $length = null [, bool $preserve_keys = false ]])` |
|       - | 4328 | ` *  Extract a slice of the array.` |
|       - | 4329 | ` * Parameters` |
|       - | 4330 | ` *  $array` |
|       - | 4331 | ` *    The input array.` |
|       - | 4332 | ` * $offset` |
|       - | 4333 | ` *    If offset is non-negative, the sequence will start at that offset in the array.` |
|       - | 4334 | ` *    If offset is negative, the sequence will start that far from the end of the array.` |
|       - | 4335 | ` * $length (optional, nullable)` |
|       - | 4336 | ` *    If length is given and is positive, then the sequence will have that many elements` |
|       - | 4337 | ` *    in it. If length is given and is negative then the sequence will stop that many` |
|       - | 4338 | ` *    elements from the end of the array. If it is omitted or NULL, then the sequence` |
|       - | 4339 | ` *    will have everything from offset up until the end of the array.` |
|       - | 4340 | ` * $preserve_keys (optional)` |
|       - | 4341 | ` *    Note that array_slice() will reorder and reset the array indices by default.` |
|       - | 4342 | ` *    You can change this behaviour by setting preserve_keys to TRUE.` |
|       - | 4343 | ` * Return` |
|       - | 4344 | ` *   The new slice.` |
|       - | 4345 | ` */` |
|      50 | 4346 | `static int ph7_hashmap_slice(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       5 | 4347 | `{` |
|       - | 4348 | `	ph7_hashmap *pMap,*pSrc;` |
|       - | 4349 | `	ph7_hashmap_node *pCur;` |
|       - | 4350 | `	ph7_value *pArray;` |
|       - | 4351 | `	int iLength,iOfft;` |
|       - | 4352 | `	int bPreserve;` |
|       - | 4353 | `	sxi32 rc;` |
|      55 | 4354 | `	if( nArg < 2 ){` |
|       8 | 4355 | `		return PH7_VmThrowException(pCtx,` |
|       - | 4356 | `			"ArgumentCountError",` |
|       - | 4357 | `			"array_slice() expects at least 2 arguments, %d given",` |
|       2 | 4358 | `			nArg` |
|       - | 4359 | `			);` |
|       - | 4360 | `	}` |
|      51 | 4361 | `	if( nArg > 4 ){` |
|       4 | 4362 | `		return PH7_VmThrowException(pCtx,` |
|       - | 4363 | `			"ArgumentCountError",` |
|       - | 4364 | `			"array_slice() expects at most 4 arguments, %d given",` |
|       1 | 4365 | `			nArg` |
|       - | 4366 | `			);` |
|       - | 4367 | `	}` |
|      49 | 4368 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|       4 | 4369 | `		return PH7_VmThrowException(pCtx,` |
|       - | 4370 | `			"TypeError",` |
|       - | 4371 | `			"array_slice(): Argument #1 ($array) must be of type array, %s given",` |
|       1 | 4372 | `			ph7_type_name(apArg[0])` |
|       - | 4373 | `			);` |
|       - | 4374 | `	}` |
|       - | 4375 | `	/* Validate $offset type: reject string, array, object, resource */` |
|      62 | 4376 | `	if( ph7_value_is_string(apArg[1]) \|\| ph7_value_is_array(apArg[1]) \|\|` |
|      65 | 4377 | `		ph7_value_is_object(apArg[1]) \|\| ph7_value_is_resource(apArg[1]) ){` |
|       4 | 4378 | `		return PH7_VmThrowException(pCtx,` |
|       - | 4379 | `			"TypeError",` |
|       - | 4380 | `			"array_slice(): Argument #2 ($offset) must be of type int, %s given",` |
|       2 | 4381 | `			ph7_type_name(apArg[1])` |
|       - | 4382 | `			);` |
|       - | 4383 | `	}` |
|       - | 4384 | `	/* Validate $length type if provided: nullable int */` |
|      45 | 4385 | `	if( nArg > 2 && !ph7_value_is_null(apArg[2]) ){` |
|      26 | 4386 | `		if( ph7_value_is_string(apArg[2]) \|\| ph7_value_is_array(apArg[2]) \|\|` |
|      26 | 4387 | `			ph7_value_is_object(apArg[2]) \|\| ph7_value_is_resource(apArg[2]) ){` |
|       4 | 4388 | `			return PH7_VmThrowException(pCtx,` |
|       - | 4389 | `				"TypeError",` |
|       - | 4390 | `				"array_slice(): Argument #3 ($length) must be of type ?int, %s given",` |
|       2 | 4391 | `				ph7_type_name(apArg[2])` |
|       - | 4392 | `				);` |
|       - | 4393 | `		}` |
|       8 | 4394 | `	}` |
|       - | 4395 | `	/* Validate $preserve_keys type if provided: reject array, object, resource */` |
|      43 | 4396 | `	if( nArg > 3 ){` |
|      10 | 4397 | `		if( ph7_value_is_array(apArg[3]) \|\| ph7_value_is_object(apArg[3]) \|\|` |
|       4 | 4398 | `			ph7_value_is_resource(apArg[3]) ){` |
|       4 | 4399 | `			return PH7_VmThrowException(pCtx,` |
|       - | 4400 | `				"TypeError",` |
|       - | 4401 | `				"array_slice(): Argument #4 ($preserve_keys) must be of type bool, %s given",` |
|       2 | 4402 | `				ph7_type_name(apArg[3])` |
|       - | 4403 | `				);` |
|       - | 4404 | `		}` |
|       2 | 4405 | `	}` |
|       - | 4406 | `	/* Point the internal representation of the target array */` |
|      41 | 4407 | `	pSrc = (ph7_hashmap *)apArg[0]->x.pOther;` |
|      41 | 4408 | `	bPreserve = FALSE;` |
|       - | 4409 | `	/* Get the offset */` |
|      41 | 4410 | `	iOfft = ph7_value_to_int(apArg[1]);` |
|      41 | 4411 | `	if( iOfft < 0 ){` |
|       5 | 4412 | `		iOfft = (int)pSrc->nEntry + iOfft;` |
|       5 | 4413 | `		if( iOfft < 0 ){` |
|       3 | 4414 | `			iOfft = 0;` |
|       1 | 4415 | `		}` |
|       2 | 4416 | `	}` |
|      41 | 4417 | `	if( iOfft >= (int)pSrc->nEntry ){` |
|       - | 4418 | `		/* Offset past end of array, return empty array */` |
|       5 | 4419 | `		pArray = ph7_context_new_array(pCtx);` |
|       5 | 4420 | `		if( pArray == 0 ){` |
|     ! 0 | 4421 | `			ph7_result_null(pCtx);` |
|     ! 0 | 4422 | `			return PH7_OK;` |
|       - | 4423 | `		}` |
|       5 | 4424 | `		ph7_result_value(pCtx,pArray);` |
|       5 | 4425 | `		return PH7_OK;` |
|       - | 4426 | `	}` |
|       - | 4427 | `	/* Get the length: NULL means "all remaining" (same as omitting) */` |
|      37 | 4428 | `	iLength = (int)pSrc->nEntry - iOfft;` |
|      37 | 4429 | `	if( nArg > 2 && !ph7_value_is_null(apArg[2]) ){` |
|      15 | 4430 | `		iLength = ph7_value_to_int(apArg[2]);` |
|      15 | 4431 | `		if( iLength < 0 ){` |
|       5 | 4432 | `			iLength = ((int)pSrc->nEntry + iLength) - iOfft;` |
|       2 | 4433 | `		}` |
|      15 | 4434 | `		if( iLength < 0 ){` |
|       3 | 4435 | `			iLength = 0;` |
|       1 | 4436 | `		}` |
|      15 | 4437 | `		if( iOfft + iLength > (int)pSrc->nEntry ){` |
|       3 | 4438 | `			iLength = (int)pSrc->nEntry - iOfft;` |
|       1 | 4439 | `		}` |
|       7 | 4440 | `	}` |
|      37 | 4441 | `	if( nArg > 3 ){` |
|       5 | 4442 | `		bPreserve = ph7_value_to_bool(apArg[3]);` |
|       2 | 4443 | `	}` |
|       - | 4444 | `	/* Create a new array */` |
|      37 | 4445 | `	pArray = ph7_context_new_array(pCtx);` |
|      37 | 4446 | `	if( pArray == 0 ){` |
|     ! 0 | 4447 | `		ph7_result_null(pCtx);` |
|     ! 0 | 4448 | `		return PH7_OK;` |
|       - | 4449 | `	}` |
|      37 | 4450 | `	if( iLength < 1 ){` |
|       - | 4451 | `		/* Don't bother processing,return the empty array */` |
|       5 | 4452 | `		ph7_result_value(pCtx,pArray);` |
|       5 | 4453 | `		return PH7_OK;` |
|       - | 4454 | `	}` |
|       - | 4455 | `	/* Point to the desired entry */` |
|      33 | 4456 | `	pCur = pSrc->pFirst;` |
|      28 | 4457 | `	for(;;){` |
|      61 | 4458 | `		if( iOfft < 1 ){` |
|      33 | 4459 | `			break;` |
|       - | 4460 | `		}` |
|       - | 4461 | `		/* Point to the next entry */` |
|      33 | 4462 | `		pCur = pCur->pPrev; /* Reverse link */` |
|      33 | 4463 | `		iOfft--;` |
|       5 | 4464 | `	}` |
|       - | 4465 | `	/* Point to the internal representation of the hashmap */` |
|      33 | 4466 | `	pMap = (ph7_hashmap *)pArray->x.pOther;` |
|      51 | 4467 | `	for(;;){` |
|     107 | 4468 | `		if( iLength < 1 ){` |
|      33 | 4469 | `			break;` |
|       - | 4470 | `		}` |
|       - | 4471 | `		/* String keys are always preserved; preserve_keys only affects int keys */` |
|       - | 4472 | `		{` |
|      79 | 4473 | `			int bKeep = (pCur->iType == HASHMAP_INT_NODE) ? bPreserve : TRUE;` |
|      79 | 4474 | `			rc = HashmapInsertNode(pMap,pCur,bKeep);` |
|       - | 4475 | `		}` |
|      79 | 4476 | `		if( rc != SXRET_OK ){` |
|     ! 0 | 4477 | `			break;` |
|       - | 4478 | `		}` |
|       - | 4479 | `		/* Point to the next entry */` |
|      79 | 4480 | `		pCur = pCur->pPrev; /* Reverse link */` |
|      79 | 4481 | `		iLength--;` |
|       5 | 4482 | `	}` |
|       - | 4483 | `	/* Return the freshly created array */` |
|      33 | 4484 | `	ph7_result_value(pCtx,pArray);` |
|      33 | 4485 | `	return PH7_OK;` |
|      30 | 4486 | `}` |
|       - | 4487 | `/*` |
|       - | 4488 | ` * Move the last node in the hashmap linked list to immediately after pAfter` |
|       - | 4489 | ` * in iteration order.  If pAfter is NULL the node is moved to the very` |
|       - | 4490 | ` * beginning (becomes the new pFirst).` |
|       - | 4491 | ` */` |
|      30 | 4492 | `static void HashmapMoveLastAfter(ph7_hashmap *pMap,ph7_hashmap_node *pAfter)` |
|       1 | 4493 | `{` |
|       - | 4494 | `	ph7_hashmap_node *pNode;` |
|       - | 4495 | `	ph7_hashmap_node *pOldNext;` |
|      31 | 4496 | `	pNode = pMap->pLast;` |
|      31 | 4497 | `	if( pNode == 0 ){` |
|     ! 0 | 4498 | `		return;` |
|       - | 4499 | `	}` |
|      31 | 4500 | `	if( pNode->pNext == 0 ){` |
|       - | 4501 | `		/* Only node in the list, nothing to move */` |
|       5 | 4502 | `		return;` |
|       - | 4503 | `	}` |
|      27 | 4504 | `	if( pAfter != 0 && pAfter->pPrev == pNode ){` |
|       - | 4505 | `		/* Already in the correct position */` |
|       9 | 4506 | `		return;` |
|       - | 4507 | `	}` |
|       - | 4508 | `	/* Unlink pNode from the end of the list */` |
|      19 | 4509 | `	pMap->pLast = pNode->pNext;` |
|      19 | 4510 | `	pMap->pLast->pPrev = 0;` |
|       - | 4511 | `	/* Insert pNode after pAfter in iteration order */` |
|      19 | 4512 | `	if( pAfter == 0 ){` |
|       - | 4513 | `		/* Insert at the very beginning, before pFirst */` |
|       3 | 4514 | `		pNode->pNext = 0;` |
|       3 | 4515 | `		pNode->pPrev = pMap->pFirst;` |
|       3 | 4516 | `		if( pMap->pFirst ){` |
|       3 | 4517 | `			pMap->pFirst->pNext = pNode;` |
|       1 | 4518 | `		}` |
|       3 | 4519 | `		pMap->pFirst = pNode;` |
|       2 | 4520 | `	}else{` |
|      17 | 4521 | `		pOldNext = pAfter->pPrev;` |
|      17 | 4522 | `		pNode->pPrev = pOldNext;` |
|      17 | 4523 | `		pNode->pNext = pAfter;` |
|      17 | 4524 | `		pAfter->pPrev = pNode;` |
|      17 | 4525 | `		if( pOldNext ){` |
|      17 | 4526 | `			pOldNext->pNext = pNode;` |
|       9 | 4527 | `		}else{` |
|     ! 0 | 4528 | `			pMap->pLast = pNode;` |
|       - | 4529 | `		}` |
|       - | 4530 | `	}` |
|      16 | 4531 | `}` |
|       - | 4532 | `/*` |
|       - | 4533 | ` * array array_splice(array $array, int $offset [, int $length [, value $replacement]])` |
|       - | 4534 | ` *  Remove a portion of the array and replace it with something else.` |
|       - | 4535 | ` * Parameters` |
|       - | 4536 | ` *  $array` |
|       - | 4537 | ` *    The input array.` |
|       - | 4538 | ` *  $offset` |
|       - | 4539 | ` *    If offset is positive then the start of removed portion is at that offset` |
|       - | 4540 | ` *    from the beginning of the input array.  If offset is negative then it` |
|       - | 4541 | ` *    starts that far from the end of the input array.  If the absolute value of` |
|       - | 4542 | ` *    a negative offset exceeds the array length, offset is clamped to 0.  If a` |
|       - | 4543 | ` *    positive offset exceeds the array length, offset is clamped to the array` |
|       - | 4544 | ` *    length (i.e. nothing is removed, but replacement is appended).` |
|       - | 4545 | ` *  $length (optional)` |
|       - | 4546 | ` *    If length is omitted, removes everything from offset to the end of the` |
|       - | 4547 | ` *    array.  If length is specified and is positive, then that many elements` |
|       - | 4548 | ` *    will be removed.  If length is specified and is negative then the end of` |
|       - | 4549 | ` *    the removed portion will be that many elements from the end of the array.` |
|       - | 4550 | ` *    If the resulting length is negative it is clamped to 0.` |
|       - | 4551 | ` *  $replacement (optional)` |
|       - | 4552 | ` *    If replacement array is specified, then the removed elements are replaced` |
|       - | 4553 | ` *    with elements from this array.` |
|       - | 4554 | ` *    If offset and length are such that nothing is removed, then the elements` |
|       - | 4555 | ` *    from the replacement array are inserted in the place specified by the` |
|       - | 4556 | ` *    offset.` |
|       - | 4557 | ` *    Note that keys in replacement array are not preserved.` |
|       - | 4558 | ` *    If replacement is just one element it is not necessary to put array()` |
|       - | 4559 | ` *    around it, unless the element is an array itself, an object or NULL.` |
|       - | 4560 | ` * Return` |
|       - | 4561 | ` *   A new array consisting of the extracted elements.` |
|       - | 4562 | ` */` |
|      54 | 4563 | `static int ph7_hashmap_splice(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       4 | 4564 | `{` |
|       - | 4565 | `	ph7_hashmap_node *pCur,*pPrev,*pRnode,*pInsertAfter,*pNewNode;` |
|       - | 4566 | `	ph7_value *pArray,*pRvalue;` |
|       - | 4567 | `	ph7_hashmap *pMap,*pSrc,*pRep;` |
|       - | 4568 | `	int iLength,iOfft,i;` |
|       - | 4569 | `	sxi32 rc;` |
|      58 | 4570 | `	if( nArg < 2 ){` |
|       8 | 4571 | `		return PH7_VmThrowException(pCtx,` |
|       - | 4572 | `			"ArgumentCountError",` |
|       - | 4573 | `			"array_splice() expects at least 2 arguments, %d given",` |
|       2 | 4574 | `			nArg` |
|       - | 4575 | `			);` |
|       - | 4576 | `	}` |
|      52 | 4577 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|       4 | 4578 | `		return PH7_VmThrowException(pCtx,` |
|       - | 4579 | `			"TypeError",` |
|       - | 4580 | `			"array_splice(): Argument #1 ($array) must be of type array, %s given",` |
|       1 | 4581 | `			ph7_type_name(apArg[0])` |
|       - | 4582 | `			);` |
|       - | 4583 | `	}` |
|       - | 4584 | `	/* Point to the internal representation of the target array */` |
|      49 | 4585 | `	PH7_HashmapCowSeparate(pCtx->pVm, apArg[0]);` |
|      49 | 4586 | `	pSrc = (ph7_hashmap *)apArg[0]->x.pOther;` |
|       - | 4587 | `	/* Get the offset and clamp to valid range */` |
|      49 | 4588 | `	iOfft = ph7_value_to_int(apArg[1]);` |
|      49 | 4589 | `	if( iOfft < 0 ){` |
|       7 | 4590 | `		iOfft = (int)pSrc->nEntry + iOfft;` |
|       7 | 4591 | `		if( iOfft < 0 ){` |
|       3 | 4592 | `			iOfft = 0;` |
|       2 | 4593 | `		}` |
|      46 | 4594 | `	}else if( iOfft > (int)pSrc->nEntry ){` |
|       3 | 4595 | `		iOfft = (int)pSrc->nEntry;` |
|       1 | 4596 | `	}` |
|       - | 4597 | `	/* Get the length and clamp to valid range.` |
|       - | 4598 | `	 * NULL means "all remaining" (same as omitting the argument). */` |
|      49 | 4599 | `	iLength = (int)pSrc->nEntry - iOfft;` |
|      49 | 4600 | `	if( nArg > 2 && !ph7_value_is_null(apArg[2]) ){` |
|      31 | 4601 | `		iLength = ph7_value_to_int(apArg[2]);` |
|      31 | 4602 | `		if( iLength < 0 ){` |
|       7 | 4603 | `			iLength = ((int)pSrc->nEntry + iLength) - iOfft;` |
|       7 | 4604 | `			if( iLength < 0 ){` |
|       3 | 4605 | `				iLength = 0;` |
|       1 | 4606 | `			}` |
|       3 | 4607 | `		}` |
|      31 | 4608 | `		if( iOfft + iLength > (int)pSrc->nEntry ){` |
|       3 | 4609 | `			iLength = (int)pSrc->nEntry - iOfft;` |
|       1 | 4610 | `		}` |
|      15 | 4611 | `	}` |
|       - | 4612 | `	/* Create the result array for removed elements */` |
|      49 | 4613 | `	pArray = ph7_context_new_array(pCtx);` |
|      49 | 4614 | `	if( pArray == 0 ){` |
|     ! 0 | 4615 | `		ph7_result_null(pCtx);` |
|     ! 0 | 4616 | `		return PH7_OK;` |
|       - | 4617 | `	}` |
|       - | 4618 | `	/* Get replacement array if provided */` |
|      49 | 4619 | `	pRep = 0;` |
|      49 | 4620 | `	if( nArg > 3 ){` |
|      21 | 4621 | `		if( !ph7_value_is_array(apArg[3]) ){` |
|       - | 4622 | `			/* Perform an array cast */` |
|       3 | 4623 | `			PH7_MemObjToHashmap(apArg[3]);` |
|       3 | 4624 | `			if( ph7_value_is_array(apArg[3]) ){` |
|       3 | 4625 | `				pRep = (ph7_hashmap *)apArg[3]->x.pOther;` |
|       1 | 4626 | `			}` |
|       2 | 4627 | `		}else{` |
|      19 | 4628 | `			pRep = (ph7_hashmap *)apArg[3]->x.pOther;` |
|       - | 4629 | `		}` |
|      21 | 4630 | `		if( pRep ){` |
|       - | 4631 | `			/* Reset the loop cursor */` |
|      21 | 4632 | `			pRep->pCur = pRep->pFirst;` |
|      10 | 4633 | `		}` |
|      10 | 4634 | `	}` |
|       - | 4635 | `	/* Early return if nothing to remove and no replacement */` |
|      49 | 4636 | `	if( iLength < 1 && pRep == 0 ){` |
|       9 | 4637 | `		ph7_result_value(pCtx,pArray);` |
|       9 | 4638 | `		return PH7_OK;` |
|       - | 4639 | `	}` |
|       - | 4640 | `	/* Navigate to the offset position */` |
|      41 | 4641 | `	pCur = pSrc->pFirst;` |
|      85 | 4642 | `	for( i = 0 ; i < iOfft && pCur ; i++ ){` |
|      45 | 4643 | `		pCur = pCur->pPrev; /* Reverse link */` |
|      23 | 4644 | `	}` |
|       - | 4645 | `	/* Save the node just before the splice range as the insertion anchor.` |
|       - | 4646 | `	 * pCur->pNext is the backward link (previous node in iteration order).` |
|       - | 4647 | `	 * If pCur is NULL (offset == nEntry), the anchor is the last node. */` |
|      41 | 4648 | `	pInsertAfter = (pCur != 0) ? pCur->pNext : pSrc->pLast;` |
|       - | 4649 | `	/* Remove nodes in the splice range and copy them to the result array */` |
|      41 | 4650 | `	pMap = (ph7_hashmap *)pArray->x.pOther;` |
|     111 | 4651 | `	for( i = 0 ; i < iLength && pCur ; i++ ){` |
|      71 | 4652 | `		pPrev = pCur->pPrev;` |
|      71 | 4653 | `		rc = HashmapInsertNode(pMap,pCur,FALSE);` |
|      71 | 4654 | `		PH7_HashmapUnlinkNode(pCur,TRUE);` |
|      71 | 4655 | `		if( rc != SXRET_OK ){` |
|     ! 0 | 4656 | `			break;` |
|       - | 4657 | `		}` |
|      71 | 4658 | `		pCur = pPrev; /* Reverse link */` |
|      36 | 4659 | `	}` |
|       - | 4660 | `	/* Insert replacement elements at the correct position */` |
|      41 | 4661 | `	if( pRep ){` |
|       - | 4662 | `		ph7_value sSafeVal;` |
|      61 | 4663 | `		while( (pRnode = PH7_HashmapGetNextEntry(pRep)) != 0 ){` |
|      31 | 4664 | `			pRvalue = HashmapExtractNodeValue(pRnode);` |
|      31 | 4665 | `			if( pRvalue ){` |
|       - | 4666 | `				/* Make a stack copy before inserting.  HashmapInsert() may` |
|       - | 4667 | `				 * grow the VM memobj pool, which would invalidate pRvalue` |
|       - | 4668 | `				 * since it points into that same pool. */` |
|      31 | 4669 | `				sSafeVal = *pRvalue;` |
|      31 | 4670 | `				rc = HashmapInsert(pSrc,0,&sSafeVal);` |
|      31 | 4671 | `				if( rc == SXRET_OK && pSrc->pLast != 0 ){` |
|      31 | 4672 | `					pNewNode = pSrc->pLast;` |
|      31 | 4673 | `					HashmapMoveLastAfter(pSrc,pInsertAfter);` |
|      31 | 4674 | `					pInsertAfter = pNewNode;` |
|      15 | 4675 | `				}` |
|      15 | 4676 | `			}` |
|       1 | 4677 | `		}` |
|      10 | 4678 | `	}` |
|       - | 4679 | `	/* Return the freshly created array */` |
|      41 | 4680 | `	ph7_result_value(pCtx,pArray);` |
|      41 | 4681 | `	return PH7_OK;` |
|      31 | 4682 | `}` |
|       - | 4683 | `/*` |
|       - | 4684 | ` * bool in_array(value $needle,array $haystack[,bool $strict = FALSE ])` |
|       - | 4685 | ` *  Checks if a value exists in an array.` |
|       - | 4686 | ` * Parameters` |
|       - | 4687 | ` *  $needle` |
|       - | 4688 | ` *   The searched value.` |
|       - | 4689 | ` *   Note:` |
|       - | 4690 | ` *    If needle is a string, the comparison is done in a case-sensitive manner.` |
|       - | 4691 | ` * $haystack` |
|       - | 4692 | ` *  The target array.` |
|       - | 4693 | ` * $strict` |
|       - | 4694 | ` *  If the third parameter strict is set to TRUE then the in_array() function` |
|       - | 4695 | ` *  will also check the types of the needle in the haystack.` |
|       - | 4696 | ` */` |
|   32008 | 4697 | `static int ph7_hashmap_in_array(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       5 | 4698 | `{` |
|       - | 4699 | `	ph7_value *pNeedle;` |
|       - | 4700 | `	int bStrict;` |
|       - | 4701 | `	int rc;` |
|   32013 | 4702 | `	if( nArg < 2 ){` |
|       - | 4703 | `		/* Missing argument,return FALSE */` |
|     ! 0 | 4704 | `		ph7_result_bool(pCtx,0);` |
|     ! 0 | 4705 | `		return PH7_OK;` |
|       - | 4706 | `	}` |
|   32013 | 4707 | `	pNeedle = apArg[0];` |
|   32013 | 4708 | `	bStrict = 0;` |
|   32013 | 4709 | `	if( nArg > 2 ){` |
|      17 | 4710 | `		bStrict = ph7_value_to_bool(apArg[2]);` |
|       8 | 4711 | `	}` |
|   32013 | 4712 | `	if( !ph7_value_is_array(apArg[1]) ){` |
|       - | 4713 | `		/* haystack must be an array,perform a standard comparison */` |
|     ! 0 | 4714 | `		rc = ph7_value_compare(pNeedle,apArg[1],bStrict);` |
|       - | 4715 | `		/* Set the comparison result */` |
|     ! 0 | 4716 | `		ph7_result_bool(pCtx,rc == 0);` |
|     ! 0 | 4717 | `		return PH7_OK;` |
|       - | 4718 | `	}` |
|       - | 4719 | `	/* Perform the lookup */` |
|   32013 | 4720 | `	rc = HashmapFindValue((ph7_hashmap *)apArg[1]->x.pOther,pNeedle,0,bStrict);` |
|       - | 4721 | `	/* Lookup result */` |
|   32013 | 4722 | `	ph7_result_bool(pCtx,rc == SXRET_OK);` |
|   32013 | 4723 | `	return PH7_OK;` |
|   16009 | 4724 | `}` |
|       - | 4725 | `/*` |
|       - | 4726 | ` * value array_search(value $needle,array $haystack[,bool $strict = false ])` |
|       - | 4727 | ` *  Searches the array for a given value and returns the corresponding key if successful.` |
|       - | 4728 | ` * Parameters` |
|       - | 4729 | ` * $needle` |
|       - | 4730 | ` *   The searched value.` |
|       - | 4731 | ` * $haystack` |
|       - | 4732 | ` *   The array.` |
|       - | 4733 | ` * $strict` |
|       - | 4734 | ` *  If the third parameter strict is set to TRUE then the array_search() function` |
|       - | 4735 | ` *  will search for identical elements in the haystack. This means it will also check` |
|       - | 4736 | ` *  the types of the needle in the haystack, and objects must be the same instance.` |
|       - | 4737 | ` * Return` |
|       - | 4738 | ` *  Returns the key for needle if it is found in the array, FALSE otherwise.` |
|       - | 4739 | ` */` |
|      28 | 4740 | `static int ph7_hashmap_search(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       5 | 4741 | `{` |
|       - | 4742 | `	ph7_hashmap_node *pEntry;` |
|       - | 4743 | `	ph7_value *pVal,sNeedle;` |
|       - | 4744 | `	ph7_hashmap *pMap;` |
|       - | 4745 | `	ph7_value sVal;` |
|       - | 4746 | `	int bStrict;` |
|       - | 4747 | `	sxu32 n;` |
|       - | 4748 | `	int rc;` |
|      33 | 4749 | `	if( nArg < 2 ){` |
|       - | 4750 | `		/* Missing argument,throw ArgumentCountError */` |
|       8 | 4751 | `		return PH7_VmThrowException(pCtx,` |
|       - | 4752 | `			"ArgumentCountError",` |
|       - | 4753 | `			"array_search() expects at least 2 arguments, %d given",` |
|       2 | 4754 | `			nArg` |
|       - | 4755 | `			);` |
|       - | 4756 | `	}` |
|      27 | 4757 | `	bStrict = FALSE;` |
|      27 | 4758 | `	if( !ph7_value_is_array(apArg[1]) ){` |
|       - | 4759 | `		/* haystack must be an array,throw TypeError */` |
|       4 | 4760 | `		return PH7_VmThrowException(pCtx,` |
|       - | 4761 | `			"TypeError",` |
|       - | 4762 | `			"array_search(): Argument #2 ($haystack) must be of type array, %s given",` |
|       2 | 4763 | `			ph7_type_name(apArg[1])` |
|       - | 4764 | `			);` |
|       - | 4765 | `	}` |
|      24 | 4766 | `	if( nArg > 2 ){` |
|       - | 4767 | `		/* In PHP, non-scalar values for a bool-hinted parameter raise TypeError */` |
|      12 | 4768 | `		if( ph7_value_is_array(apArg[2]) \|\| ph7_value_is_object(apArg[2]) \|\| ph7_value_is_resource(apArg[2]) ){` |
|       4 | 4769 | `			return PH7_VmThrowException(pCtx,` |
|       - | 4770 | `				"TypeError",` |
|       - | 4771 | `				"array_search(): Argument #3 ($strict) must be of type bool, %s given",` |
|       2 | 4772 | `				ph7_type_name(apArg[2])` |
|       - | 4773 | `				);` |
|       - | 4774 | `		}` |
|       9 | 4775 | `		bStrict = ph7_value_to_bool(apArg[2]);` |
|       4 | 4776 | `	}` |
|       - | 4777 | `	/* Point to the internal representation of the internal hashmap */` |
|      21 | 4778 | `	pMap = (ph7_hashmap *)apArg[1]->x.pOther;` |
|       - | 4779 | `	/* Perform a linear search since we cannot sort the hashmap based on values */` |
|      21 | 4780 | `	PH7_MemObjInit(pMap->pVm,&sVal);` |
|      21 | 4781 | `	PH7_MemObjInit(pMap->pVm,&sNeedle);` |
|      21 | 4782 | `	pEntry = pMap->pFirst;` |
|      21 | 4783 | `	n = pMap->nEntry;` |
|      23 | 4784 | `	for(;;){` |
|      47 | 4785 | `		if( !n ){` |
|       9 | 4786 | `			break;` |
|       - | 4787 | `		}` |
|       - | 4788 | `		/* Extract node value */` |
|      39 | 4789 | `		pVal = HashmapExtractNodeValue(pEntry);` |
|      39 | 4790 | `		if( pVal ){` |
|       - | 4791 | `			/* Make a copy of the vuurent values since the comparison routine` |
|       - | 4792 | `			 * can change their type.` |
|       - | 4793 | `			 */` |
|      39 | 4794 | `			PH7_MemObjLoad(pVal,&sVal);` |
|      39 | 4795 | `			PH7_MemObjLoad(apArg[0],&sNeedle);` |
|      39 | 4796 | `			rc = PH7_MemObjCmp(&sNeedle,&sVal,bStrict,0);` |
|      39 | 4797 | `			PH7_MemObjRelease(&sVal);` |
|      39 | 4798 | `			PH7_MemObjRelease(&sNeedle);` |
|      39 | 4799 | `			if( rc == 0 ){` |
|       - | 4800 | `				/* Match found,return key */` |
|      13 | 4801 | `				if( pEntry->iType == HASHMAP_INT_NODE){` |
|       - | 4802 | `					/* INT key */` |
|       7 | 4803 | `					ph7_result_int64(pCtx,pEntry->xKey.iKey);` |
|       4 | 4804 | `				}else{` |
|       7 | 4805 | `					SyBlob *pKey = &pEntry->xKey.sKey;` |
|       - | 4806 | `					/* Blob key */` |
|       7 | 4807 | `					ph7_result_string(pCtx,(const char *)SyBlobData(pKey),(int)SyBlobLength(pKey));` |
|       - | 4808 | `				}` |
|      13 | 4809 | `				return PH7_OK;` |
|       - | 4810 | `			}` |
|      13 | 4811 | `		}` |
|       - | 4812 | `		/* Point to the next entry */` |
|      27 | 4813 | `		pEntry = pEntry->pPrev; /* Reverse link */` |
|      27 | 4814 | `		n--;` |
|       1 | 4815 | `	}` |
|       - | 4816 | `	/* No such value,return FALSE */` |
|       9 | 4817 | `	ph7_result_bool(pCtx,0);` |
|       9 | 4818 | `	return PH7_OK;` |
|      19 | 4819 | `}` |
|       - | 4820 | `/*` |
|       - | 4821 | ` * array array_diff(array $array1,array $array2,...)` |
|       - | 4822 | ` *  Computes the difference of arrays.` |
|       - | 4823 | ` * Parameters` |
|       - | 4824 | ` *  $array1` |
|       - | 4825 | ` *    The array to compare from` |
|       - | 4826 | ` *  $array2` |
|       - | 4827 | ` *    An array to compare against` |
|       - | 4828 | ` *  $...` |
|       - | 4829 | ` *   More arrays to compare against` |
|       - | 4830 | ` * Return` |
|       - | 4831 | ` *  Returns an array containing all the entries from array1 that` |
|       - | 4832 | ` *  are not present in any of the other arrays.` |
|       - | 4833 | ` */` |
|      22 | 4834 | `static int ph7_hashmap_diff(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       4 | 4835 | `{` |
|       - | 4836 | `	ph7_hashmap_node *pEntry;` |
|       - | 4837 | `	ph7_hashmap *pSrc,*pMap;` |
|       - | 4838 | `	ph7_value *pArray;` |
|       - | 4839 | `	ph7_value *pVal;` |
|       - | 4840 | `	sxi32 rc;` |
|       - | 4841 | `	sxu32 n;` |
|       - | 4842 | `	int i;` |
|       - | 4843 | `	/* Validate arguments to mimic PHP behaviour. Earlier versions simply` |
|       - | 4844 | `	 * returned NULL when the caller passed invalid parameters which made` |
|       - | 4845 | `	 * debugging difficult. */` |
|      26 | 4846 | `	if( nArg < 1 ){` |
|       4 | 4847 | `		return PH7_VmThrowException(pCtx,` |
|       - | 4848 | `			"ArgumentCountError",` |
|       - | 4849 | `			"array_diff() expects at least 1 argument, %d given",` |
|       1 | 4850 | `			nArg` |
|       - | 4851 | `			);` |
|       - | 4852 | `	}` |
|      23 | 4853 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|       4 | 4854 | `		return PH7_VmThrowException(pCtx,` |
|       - | 4855 | `			"TypeError",` |
|       - | 4856 | `			"array_diff(): Argument #1 ($array) must be of type array, %s given",` |
|       1 | 4857 | `			ph7_type_name(apArg[0])` |
|       - | 4858 | `			);` |
|       - | 4859 | `	}` |
|      36 | 4860 | `	for(i = 1 ; i < nArg ; i++){` |
|      20 | 4861 | `		if( !ph7_value_is_array(apArg[i]) ){` |
|       4 | 4862 | `			return PH7_VmThrowException(pCtx,` |
|       - | 4863 | `				"TypeError",` |
|       - | 4864 | `				"array_diff(): Argument #%d must be of type array, %s given",` |
|       1 | 4865 | `				i + 1,` |
|       2 | 4866 | `				ph7_type_name(apArg[i])` |
|       - | 4867 | `				);` |
|       - | 4868 | `		}` |
|       9 | 4869 | `	}` |
|      17 | 4870 | `	if( nArg == 1 ){` |
|       - | 4871 | `		/* Return the first array since we cannot perform a diff */` |
|       3 | 4872 | `		ph7_result_value(pCtx,apArg[0]);` |
|       3 | 4873 | `		return PH7_OK;` |
|       - | 4874 | `	}` |
|       - | 4875 | `	/* Create a new array */` |
|      15 | 4876 | `	pArray = ph7_context_new_array(pCtx);` |
|      15 | 4877 | `	if( pArray == 0 ){` |
|     ! 0 | 4878 | `		ph7_result_null(pCtx);` |
|     ! 0 | 4879 | `		return PH7_OK;` |
|       - | 4880 | `	}` |
|       - | 4881 | `	/* Point to the internal representation of the source hashmap */` |
|      15 | 4882 | `	pSrc = (ph7_hashmap *)apArg[0]->x.pOther;` |
|       - | 4883 | `	/* Perform the diff */` |
|      15 | 4884 | `	pEntry = pSrc->pFirst;` |
|      15 | 4885 | `	n = pSrc->nEntry;` |
|      27 | 4886 | `	for(;;){` |
|      55 | 4887 | `		if( n < 1 ){` |
|      15 | 4888 | `			break;` |
|       - | 4889 | `		}` |
|       - | 4890 | `		/* Extract the node value */` |
|      41 | 4891 | `		pVal = HashmapExtractNodeValue(pEntry);` |
|      41 | 4892 | `		if( pVal ){` |
|      69 | 4893 | `			for( i = 1 ; i < nArg ; i++ ){` |
|       - | 4894 | `				/* Point to the internal representation of the hashmap */` |
|      45 | 4895 | `				pMap = (ph7_hashmap *)apArg[i]->x.pOther;` |
|       - | 4896 | `				/* Perform the lookup */` |
|      45 | 4897 | `				rc = HashmapFindValue(pMap,pVal,0,TRUE);` |
|      45 | 4898 | `				if( rc == SXRET_OK ){` |
|       - | 4899 | `					/* Value exist */` |
|      17 | 4900 | `					break;` |
|       - | 4901 | `				}` |
|      15 | 4902 | `			}` |
|      41 | 4903 | `			if( i >= nArg ){` |
|       - | 4904 | `				/* Perform the insertion */` |
|      25 | 4905 | `				HashmapInsertNode((ph7_hashmap *)pArray->x.pOther,pEntry,TRUE);` |
|      12 | 4906 | `			}` |
|      20 | 4907 | `		}` |
|       - | 4908 | `		/* Point to the next entry */` |
|      41 | 4909 | `		pEntry = pEntry->pPrev; /* Reverse link */` |
|      41 | 4910 | `		n--;` |
|       1 | 4911 | `	}` |
|       - | 4912 | `	/* Return the freshly created array */` |
|      15 | 4913 | `	ph7_result_value(pCtx,pArray);` |
|      15 | 4914 | `	return PH7_OK;` |
|      15 | 4915 | `}` |
|       - | 4916 | `/*` |
|       - | 4917 | ` * array array_udiff(array $array1,array $array2,...,$callback)` |
|       - | 4918 | ` *  Computes the difference of arrays by using a callback function for data comparison.` |
|       - | 4919 | ` * Parameters` |
|       - | 4920 | ` *  $array1` |
|       - | 4921 | ` *    The array to compare from` |
|       - | 4922 | ` *  $array2` |
|       - | 4923 | ` *    An array to compare against` |
|       - | 4924 | ` *  $...` |
|       - | 4925 | ` *   More arrays to compare against.` |
|       - | 4926 | ` * $callback` |
|       - | 4927 | ` *  The callback comparison function.` |
|       - | 4928 | ` *  The comparison function must return an integer less than, equal to, or greater than zero` |
|       - | 4929 | ` *  if the first argument is considered to be respectively less than, equal to, or greater` |
|       - | 4930 | ` *  than the second.` |
|       - | 4931 | ` *     int callback ( mixed $a, mixed $b )` |
|       - | 4932 | ` * Return` |
|       - | 4933 | ` *  Returns an array containing all the entries from array1 that` |
|       - | 4934 | ` *  are not present in any of the other arrays.` |
|       - | 4935 | ` */` |
|      22 | 4936 | `static int ph7_hashmap_udiff(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       5 | 4937 | `{` |
|       - | 4938 | `	ph7_hashmap_node *pEntry;` |
|       - | 4939 | `	ph7_hashmap *pSrc,*pMap;` |
|       - | 4940 | `	ph7_value *pCallback;` |
|       - | 4941 | `	ph7_value *pArray;` |
|       - | 4942 | `	ph7_value *pVal;` |
|       - | 4943 | `	sxi32 rc;` |
|       - | 4944 | `	sxu32 n;` |
|       - | 4945 | `	int i;` |
|       - | 4946 |  |
|       - | 4947 | `	/* Ensure the argument count matches PHP behaviour. */` |
|      27 | 4948 | `	if( nArg < 2 ){` |
|       4 | 4949 | `		return PH7_VmThrowException(pCtx,` |
|       - | 4950 | `			"ArgumentCountError",` |
|       - | 4951 | `			"array_udiff() expects at least 2 arguments, %d given",` |
|       1 | 4952 | `			nArg` |
|       - | 4953 | `			);` |
|       - | 4954 | `	}` |
|      25 | 4955 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|       4 | 4956 | `		return PH7_VmThrowException(pCtx,` |
|       - | 4957 | `			"TypeError",` |
|       - | 4958 | `			"array_udiff(): Argument #1 ($array) must be of type array, %s given",` |
|       1 | 4959 | `			ph7_type_name(apArg[0])` |
|       - | 4960 | `			);` |
|       - | 4961 | `	}` |
|       - | 4962 |  |
|      23 | 4963 | `	if( nArg == 2 ){` |
|       - | 4964 | `		/* Only the original array and the callback were provided. */` |
|       - | 4965 | `		/* Nevertheless, we still validate the callback after verifying any` |
|       - | 4966 | `		 * intermediate array arguments to match PHP's left-to-right parameter` |
|       - | 4967 | `		 * validation order.` |
|       - | 4968 | `		 */` |
|       4 | 4969 | `	} else {` |
|       - | 4970 | `		/* Ensure intermediary arguments are arrays (matches PHP strict typing). */` |
|      27 | 4971 | `		for( i = 1 ; i < nArg - 1; i++ ){` |
|      19 | 4972 | `			if( !ph7_value_is_array(apArg[i]) ){` |
|      11 | 4973 | `				return PH7_VmThrowException(pCtx,` |
|       - | 4974 | `					"TypeError",` |
|       - | 4975 | `					"array_udiff(): Argument #%d must be of type array, %s given",` |
|       3 | 4976 | `					i + 1,` |
|       6 | 4977 | `					ph7_type_name(apArg[i])` |
|       - | 4978 | `					);` |
|       - | 4979 | `			}` |
|       7 | 4980 | `		}` |
|       - | 4981 | `	}` |
|       - | 4982 |  |
|       - | 4983 | `	/* Identify the callback (always expected as the last argument). */` |
|      16 | 4984 | `	pCallback = apArg[nArg - 1];` |
|       - | 4985 | `	/* Validate the callback to match PHP's error messages. */` |
|      16 | 4986 | `	if( !ph7_value_is_callable(pCallback) ){` |
|       9 | 4987 | `		if( ph7_value_is_array(pCallback) ){` |
|       4 | 4988 | `			return PH7_VmThrowException(pCtx,` |
|       - | 4989 | `				"TypeError",` |
|       - | 4990 | `				"array_udiff(): Argument #%d must be a valid callback, array callback must have exactly two members",` |
|       1 | 4991 | `				nArg` |
|       - | 4992 | `				);` |
|       - | 4993 | `		}` |
|       6 | 4994 | `		if( ph7_value_is_string(pCallback) ){` |
|       - | 4995 | `			int len;` |
|       3 | 4996 | `			const char *zName = ph7_value_to_string(pCallback, &len);` |
|       4 | 4997 | `			return PH7_VmThrowException(pCtx,` |
|       - | 4998 | `				"TypeError",` |
|       - | 4999 | `				"array_udiff(): Argument #%d must be a valid callback, function \"%s\" not found or invalid function name",` |
|       1 | 5000 | `				nArg,` |
|       1 | 5001 | `				zName` |
|       - | 5002 | `				);` |
|       - | 5003 | `		}` |
|       4 | 5004 | `		return PH7_VmThrowException(pCtx,` |
|       - | 5005 | `			"TypeError",` |
|       - | 5006 | `			"array_udiff(): Argument #%d must be a valid callback, no array or string given",` |
|       1 | 5007 | `			nArg` |
|       - | 5008 | `			);` |
|       - | 5009 | `	}` |
|       - | 5010 |  |
|       7 | 5011 | `	if( nArg == 2 ){` |
|       - | 5012 | `		/* Only the original array and the callback were provided. */` |
|       3 | 5013 | `		ph7_result_value(pCtx,apArg[0]);` |
|       3 | 5014 | `		return PH7_OK;` |
|       - | 5015 | `	}` |
|       - | 5016 |  |
|       - | 5017 | `	/* Create a new array */` |
|       5 | 5018 | `	pArray = ph7_context_new_array(pCtx);` |
|       5 | 5019 | `	if( pArray == 0 ){` |
|     ! 0 | 5020 | `		ph7_result_null(pCtx);` |
|     ! 0 | 5021 | `		return PH7_OK;` |
|       - | 5022 | `	}` |
|       - | 5023 | `	/* Point to the internal representation of the source hashmap */` |
|       5 | 5024 | `	pSrc = (ph7_hashmap *)apArg[0]->x.pOther;` |
|       - | 5025 | `	/* Perform the diff */` |
|       5 | 5026 | `	pEntry = pSrc->pFirst;` |
|       5 | 5027 | `	n = pSrc->nEntry;` |
|       5 | 5028 | `	pCtx->pVm->iCmpCallbackExc = 0;` |
|       5 | 5029 | `	for(;;){` |
|      11 | 5030 | `		if( n < 1 ){` |
|       3 | 5031 | `			break;` |
|       - | 5032 | `		}` |
|       - | 5033 | `		/* Extract the node value */` |
|       9 | 5034 | `		pVal = HashmapExtractNodeValue(pEntry);` |
|       9 | 5035 | `		if( pVal ){` |
|      15 | 5036 | `			for( i = 1 ; i < nArg - 1; i++ ){` |
|       - | 5037 | `				/* Point to the internal representation of the hashmap */` |
|       9 | 5038 | `				pMap = (ph7_hashmap *)apArg[i]->x.pOther;` |
|       - | 5039 | `				/* Perform the lookup */` |
|       9 | 5040 | `				rc = HashmapFindValueByCallback(pMap,pVal,pCallback,0);` |
|       9 | 5041 | `				if( rc == SXRET_OK ){` |
|       - | 5042 | `					/* Value exist */` |
|       3 | 5043 | `					break;` |
|       - | 5044 | `				}` |
|       4 | 5045 | `			}` |
|       9 | 5046 | `			if( pCtx->pVm->iCmpCallbackExc ){` |
|       - | 5047 | `				/* The comparison callback raised: propagate so the dispatcher` |
|       - | 5048 | `				 * unwinds, before any spurious insertion into the result. */` |
|       3 | 5049 | `				pCtx->pVm->iCmpCallbackExc = 0;` |
|       3 | 5050 | `				return PH7_EXCEPTION;` |
|       - | 5051 | `			}` |
|       7 | 5052 | `			if( i >= (nArg - 1)){` |
|       - | 5053 | `				/* Perform the insertion */` |
|       5 | 5054 | `				HashmapInsertNode((ph7_hashmap *)pArray->x.pOther,pEntry,TRUE);` |
|       2 | 5055 | `			}` |
|       3 | 5056 | `		}` |
|       - | 5057 | `		/* Point to the next entry */` |
|       7 | 5058 | `		pEntry = pEntry->pPrev; /* Reverse link */` |
|       7 | 5059 | `		n--;` |
|       1 | 5060 | `	}` |
|       - | 5061 | `	/* Return the freshly created array */` |
|       3 | 5062 | `	ph7_result_value(pCtx,pArray);` |
|       3 | 5063 | `	return PH7_OK;` |
|      16 | 5064 | `}` |
|       - | 5065 | `/*` |
|       - | 5066 | ` * array array_diff_assoc(array $array1,array $array2,...)` |
|       - | 5067 | ` *  Computes the difference of arrays with additional index check.` |
|       - | 5068 | ` * Parameters` |
|       - | 5069 | ` *  $array1` |
|       - | 5070 | ` *    The array to compare from` |
|       - | 5071 | ` *  $array2` |
|       - | 5072 | ` *    An array to compare against` |
|       - | 5073 | ` *  $...` |
|       - | 5074 | ` *   More arrays to compare against` |
|       - | 5075 | ` * Return` |
|       - | 5076 | ` *  Returns an array containing all the entries from array1 that` |
|       - | 5077 | ` *  are not present in any of the other arrays.` |
|       - | 5078 | ` */` |
|      20 | 5079 | `static int ph7_hashmap_diff_assoc(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       5 | 5080 | `{` |
|       - | 5081 | `	ph7_hashmap_node *pN1,*pN2,*pEntry;` |
|       - | 5082 | `	ph7_hashmap *pSrc,*pMap;` |
|       - | 5083 | `	ph7_value *pArray;` |
|       - | 5084 | `	ph7_value *pVal;` |
|       - | 5085 | `	sxi32 rc;` |
|       - | 5086 | `	sxu32 n;` |
|       - | 5087 | `	int i;` |
|       - | 5088 | `	/* Ensure the argument list is valid, emitting the same errors PHP` |
|       - | 5089 | `	 * would produce. This makes behaviour predictable and allows the` |
|       - | 5090 | `	 * accompanying integration tests to pass. */` |
|      25 | 5091 | `	if( nArg < 1 ){` |
|       4 | 5092 | `		return PH7_VmThrowException(pCtx,` |
|       - | 5093 | `			"ArgumentCountError",` |
|       - | 5094 | `			"array_diff_assoc() expects at least 1 argument, %d given",` |
|       1 | 5095 | `			nArg` |
|       - | 5096 | `			);` |
|       - | 5097 | `	}` |
|      22 | 5098 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|       4 | 5099 | `		return PH7_VmThrowException(pCtx,` |
|       - | 5100 | `			"TypeError",` |
|       - | 5101 | `			"array_diff_assoc(): Argument #1 ($array) must be of type array, %s given",` |
|       1 | 5102 | `			ph7_type_name(apArg[0])` |
|       - | 5103 | `			);` |
|       - | 5104 | `	}` |
|      33 | 5105 | `	for(i = 1 ; i < nArg ; i++){` |
|      21 | 5106 | `		if( !ph7_value_is_array(apArg[i]) ){` |
|       8 | 5107 | `			return PH7_VmThrowException(pCtx,` |
|       - | 5108 | `				"TypeError",` |
|       - | 5109 | `				"array_diff_assoc(): Argument #%d must be of type array, %s given",` |
|       2 | 5110 | `				i + 1,` |
|       4 | 5111 | `				ph7_type_name(apArg[i])` |
|       - | 5112 | `				);` |
|       - | 5113 | `		}` |
|       9 | 5114 | `	}` |
|      13 | 5115 | `	if( nArg == 1 ){` |
|       - | 5116 | `		/* Return the first array since we cannot perform a diff */` |
|       3 | 5117 | `		ph7_result_value(pCtx,apArg[0]);` |
|       3 | 5118 | `		return PH7_OK;` |
|       - | 5119 | `	}` |
|       - | 5120 | `	/* Create a new array */` |
|      11 | 5121 | `	pArray = ph7_context_new_array(pCtx);` |
|      11 | 5122 | `	if( pArray == 0 ){` |
|     ! 0 | 5123 | `		ph7_result_null(pCtx);` |
|     ! 0 | 5124 | `		return PH7_OK;` |
|       - | 5125 | `	}` |
|       - | 5126 | `	/* Point to the internal representation of the source hashmap */` |
|      11 | 5127 | `	pSrc = (ph7_hashmap *)apArg[0]->x.pOther;` |
|       - | 5128 | `	/* Perform the diff */` |
|      11 | 5129 | `	pEntry = pSrc->pFirst;` |
|      11 | 5130 | `	n = pSrc->nEntry;` |
|      11 | 5131 | `	pN1 = pN2 = 0;` |
|      29 | 5132 | `	for(;;){` |
|       - | 5133 | `		int keep;` |
|      35 | 5134 | `		if( n < 1 ){` |
|      11 | 5135 | `			break;` |
|       - | 5136 | `		}` |
|       - | 5137 | `		/* assume the element should be kept until we find a match */` |
|      25 | 5138 | `		keep = 1;` |
|      41 | 5139 | `		for( i = 1 ; i < nArg ; i++ ){` |
|       - | 5140 | `			/* all arguments have been validated already, so cast directly */` |
|      29 | 5141 | `			pMap = (ph7_hashmap *)apArg[i]->x.pOther;` |
|       - | 5142 | `			/* Perform a key lookup first */` |
|      29 | 5143 | `			if( pEntry->iType == HASHMAP_INT_NODE ){` |
|      13 | 5144 | `				rc = HashmapLookupIntKey(pMap,pEntry->xKey.iKey,&pN1);` |
|       7 | 5145 | `			}else{` |
|      17 | 5146 | `				rc = HashmapLookupBlobKey(pMap,SyBlobData(&pEntry->xKey.sKey),SyBlobLength(&pEntry->xKey.sKey),&pN1);` |
|       - | 5147 | `			}` |
|      29 | 5148 | `			if( rc != SXRET_OK ){` |
|       - | 5149 | `				/* this array does not contain the key, continue checking others */` |
|      15 | 5150 | `				continue;` |
|       - | 5151 | `			}` |
|       - | 5152 | `			/* key exists; check that value stored in the matching node is equal */` |
|      15 | 5153 | `			pVal = HashmapExtractNodeValue(pEntry);` |
|      15 | 5154 | `			if( pVal ){` |
|       - | 5155 | `				/* directly compare with value at pN1 rather than searching again */` |
|      15 | 5156 | `				ph7_value *pVal2 = HashmapExtractNodeValue(pN1);` |
|      15 | 5157 | `				if( pVal2 ){` |
|      15 | 5158 | `					sxi32 cmp = PH7_MemObjCmp(pVal, pVal2, TRUE, 0);` |
|      15 | 5159 | `					if( cmp == 0 ){` |
|       - | 5160 | `						/* identical key+value found in one of the arrays => drop it */` |
|      13 | 5161 | `						keep = 0;` |
|      13 | 5162 | `						break;` |
|       - | 5163 | `					}` |
|       1 | 5164 | `				}` |
|       1 | 5165 | `			}` |
|       2 | 5166 | `		}` |
|      25 | 5167 | `		if( keep ){` |
|       - | 5168 | `			/* Perform the insertion */` |
|      13 | 5169 | `			HashmapInsertNode((ph7_hashmap *)pArray->x.pOther,pEntry,TRUE);` |
|       6 | 5170 | `		}` |
|       - | 5171 | `		/* Point to the next entry */` |
|      25 | 5172 | `		pEntry = pEntry->pPrev; /* Reverse link */` |
|      25 | 5173 | `		n--;` |
|       1 | 5174 | `	}` |
|       - | 5175 | `	/* Return the freshly created array */` |
|      11 | 5176 | `	ph7_result_value(pCtx,pArray);` |
|      11 | 5177 | `	return PH7_OK;` |
|      15 | 5178 | `}` |
|       - | 5179 | `/*` |
|       - | 5180 | ` * array array_diff_uassoc(array $array1,array $array2,...,callback $key_compare_func)` |
|       - | 5181 | ` *  Computes the difference of arrays with additional index check which is performed` |
|       - | 5182 | ` *  by a user supplied callback function.` |
|       - | 5183 | ` * Parameters` |
|       - | 5184 | ` *  $array1` |
|       - | 5185 | ` *    The array to compare from` |
|       - | 5186 | ` *  $array2` |
|       - | 5187 | ` *    An array to compare against` |
|       - | 5188 | ` *  $...` |
|       - | 5189 | ` *   More arrays to compare against.` |
|       - | 5190 | ` *  $key_compare_func` |
|       - | 5191 | ` *   Callback function to use. The callback function must return an integer` |
|       - | 5192 | ` *   less than, equal to, or greater than zero if the first argument is considered` |
|       - | 5193 | ` *   to be respectively less than, equal to, or greater than the second.` |
|       - | 5194 | ` * Return` |
|       - | 5195 | ` *  Returns an array containing all the entries from array1 that` |
|       - | 5196 | ` *  are not present in any of the other arrays.` |
|       - | 5197 | ` */` |
|      24 | 5198 | `static int ph7_hashmap_diff_uassoc(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       5 | 5199 | `{` |
|       - | 5200 | `	ph7_hashmap_node *pEntry;` |
|       - | 5201 | `	ph7_hashmap *pSrc,*pMap;` |
|       - | 5202 | `	ph7_value *pCallback;` |
|       - | 5203 | `	ph7_value *pArray;` |
|       - | 5204 | `	sxi32 rc;` |
|       - | 5205 | `	sxu32 n;` |
|       - | 5206 | `	int i;` |
|       - | 5207 |  |
|       - | 5208 | `	/* Argument validation mimicking PHP errors. */` |
|      29 | 5209 | `	if( nArg < 2 ){` |
|       4 | 5210 | `		return PH7_VmThrowException(pCtx,` |
|       - | 5211 | `			"ArgumentCountError",` |
|       - | 5212 | `			"array_diff_uassoc() expects at least 2 arguments, %d given",` |
|       1 | 5213 | `			nArg` |
|       - | 5214 | `			);` |
|       - | 5215 | `	}` |
|      26 | 5216 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|       4 | 5217 | `		return PH7_VmThrowException(pCtx,` |
|       - | 5218 | `			"TypeError",` |
|       - | 5219 | `			"array_diff_uassoc(): Argument #1 ($array) must be of type array, %s given",` |
|       1 | 5220 | `			ph7_type_name(apArg[0])` |
|       - | 5221 | `			);` |
|       - | 5222 | `	}` |
|       - | 5223 | `	/* Intermediate arguments (except last) must be arrays. Last argument is` |
|       - | 5224 | `	 * expected to be a callback. */` |
|      38 | 5225 | `	for(i = 1 ; i < nArg - 1; i++){` |
|      19 | 5226 | `		if( !ph7_value_is_array(apArg[i]) ){` |
|       4 | 5227 | `			return PH7_VmThrowException(pCtx,` |
|       - | 5228 | `				"TypeError",` |
|       - | 5229 | `				"array_diff_uassoc(): Argument #%d must be of type array, %s given",` |
|       1 | 5230 | `				i + 1,` |
|       2 | 5231 | `				ph7_type_name(apArg[i])` |
|       - | 5232 | `				);` |
|       - | 5233 | `		}` |
|       9 | 5234 | `	}` |
|       - | 5235 | `	/* Point to the callback value */` |
|      22 | 5236 | `	pCallback = apArg[nArg - 1];` |
|      22 | 5237 | `	if( !ph7_value_is_callable(pCallback) ){` |
|       - | 5238 | `		/* Compose an error message that closely matches PHP output. When the` |
|       - | 5239 | `		 * argument is an array of the wrong shape we include an extra clause.` |
|       - | 5240 | `		 * If the value is neither array nor string, PHP says "no array or` |
|       - | 5241 | `		 * string given" which we also reproduce. */` |
|       9 | 5242 | `		if( ph7_value_is_array(pCallback) ){` |
|       - | 5243 | `			/* ARRAY CALLBACK must have exactly two members */` |
|       4 | 5244 | `			return PH7_VmThrowException(pCtx,` |
|       - | 5245 | `				"TypeError",` |
|       - | 5246 | `				"array_diff_uassoc(): Argument #%d must be a valid callback, array callback must have exactly two members",` |
|       1 | 5247 | `				nArg` |
|       - | 5248 | `				);` |
|       - | 5249 | `		}` |
|       6 | 5250 | `		if( !ph7_value_is_string(pCallback) ){` |
|       - | 5251 | `			/* neither array nor string */` |
|       8 | 5252 | `			return PH7_VmThrowException(pCtx,` |
|       - | 5253 | `				"TypeError",` |
|       - | 5254 | `				"array_diff_uassoc(): Argument #%d must be a valid callback, no array or string given",` |
|       2 | 5255 | `				nArg` |
|       - | 5256 | `				);` |
|       - | 5257 | `		}` |
|       - | 5258 | `		/* Fallback for string (non-callable) or other leftover cases */` |
|     ! 0 | 5259 | `		return PH7_VmThrowException(pCtx,` |
|       - | 5260 | `			"TypeError",` |
|       - | 5261 | `			"array_diff_uassoc(): Argument #%d must be a valid callback, %s given",` |
|     ! 0 | 5262 | `			nArg,` |
|     ! 0 | 5263 | `			ph7_type_name(pCallback)` |
|       - | 5264 | `			);` |
|       - | 5265 | `	}` |
|      13 | 5266 | `	if( nArg == 2 ){` |
|       - | 5267 | `		/* If we only have the first array and the callback, just return the` |
|       - | 5268 | `		 * input array. */` |
|       3 | 5269 | `		ph7_result_value(pCtx,apArg[0]);` |
|       3 | 5270 | `		return PH7_OK;` |
|       - | 5271 | `	}` |
|       - | 5272 | `	/* Create a new array */` |
|      11 | 5273 | `	pArray = ph7_context_new_array(pCtx);` |
|      11 | 5274 | `	if( pArray == 0 ){` |
|     ! 0 | 5275 | `		ph7_result_null(pCtx);` |
|     ! 0 | 5276 | `		return PH7_OK;` |
|       - | 5277 | `	}` |
|       - | 5278 | `	/* Point to the internal representation of the source hashmap */` |
|      11 | 5279 | `	pSrc = (ph7_hashmap *)apArg[0]->x.pOther;` |
|       - | 5280 | `	/* Perform the diff */` |
|      11 | 5281 | `	pEntry = pSrc->pFirst;` |
|      11 | 5282 | `	n = pSrc->nEntry;` |
|      21 | 5283 | `	for(;;){` |
|       - | 5284 | `		int keep;` |
|      27 | 5285 | `		if( n < 1 ){` |
|       9 | 5286 | `			break;` |
|       - | 5287 | `		}` |
|      19 | 5288 | `		keep = 1;` |
|      31 | 5289 | `		for( i = 1 ; i < nArg - 1; i++ ){` |
|       - | 5290 | `			/* each of these must already be arrays thanks to earlier validation */` |
|      23 | 5291 | `			pMap = (ph7_hashmap *)apArg[i]->x.pOther;` |
|       - | 5292 | `			/* we must compare keys via callback, not by direct lookup */` |
|      23 | 5293 | `			ph7_hashmap_node *pIt = pMap->pFirst;` |
|      45 | 5294 | `			while( pIt ){` |
|       - | 5295 | `				/* build temporary key values for callback */` |
|       - | 5296 | `				ph7_value key1, key2, result;` |
|       - | 5297 | `				/* initialise only once using the appropriate helper */` |
|      33 | 5298 | `				if( pEntry->iType == HASHMAP_INT_NODE ){` |
|     ! 0 | 5299 | `					PH7_MemObjInitFromInt(pMap->pVm,&key1,pEntry->xKey.iKey);` |
|     ! 0 | 5300 | `				}else{` |
|       - | 5301 | `					SyString sStr;` |
|      33 | 5302 | `					SyStringInitFromBuf(&sStr,` |
|       - | 5303 | `						SyBlobData(&pEntry->xKey.sKey),` |
|       - | 5304 | `						SyBlobLength(&pEntry->xKey.sKey));` |
|      33 | 5305 | `					PH7_MemObjInitFromString(pMap->pVm,&key1,&sStr);` |
|       - | 5306 | `				}` |
|      33 | 5307 | `				if( pIt->iType == HASHMAP_INT_NODE ){` |
|     ! 0 | 5308 | `					PH7_MemObjInitFromInt(pMap->pVm,&key2,pIt->xKey.iKey);` |
|     ! 0 | 5309 | `				}else{` |
|       - | 5310 | `					SyString sStr;` |
|      33 | 5311 | `					SyStringInitFromBuf(&sStr,` |
|       - | 5312 | `						SyBlobData(&pIt->xKey.sKey),` |
|       - | 5313 | `						SyBlobLength(&pIt->xKey.sKey));` |
|      33 | 5314 | `					PH7_MemObjInitFromString(pMap->pVm,&key2,&sStr);` |
|       - | 5315 | `				}` |
|      33 | 5316 | `				PH7_MemObjInit(pMap->pVm,&result);` |
|       - | 5317 | `				/* call user callback with (key1, key2) */` |
|       - | 5318 | `				{` |
|       - | 5319 | `					ph7_value *apK[2];` |
|      33 | 5320 | `					apK[0] = &key1;` |
|      33 | 5321 | `					apK[1] = &key2;` |
|      33 | 5322 | `					rc = PH7_VmCallUserFunction(pMap->pVm,pCallback,2,apK,&result);` |
|       - | 5323 | `				}` |
|      33 | 5324 | `				if( rc == PH7_EXCEPTION ){` |
|       - | 5325 | `					/* The key comparison callback raised. Unlike array_udiff/` |
|       - | 5326 | `					 * array_uintersect (which signal back from` |
|       - | 5327 | `					 * HashmapFindValueByCallback via pVm->iCmpCallbackExc), this` |
|       - | 5328 | `					 * function invokes the callback inline, so it cleans up its own` |
|       - | 5329 | `					 * temporaries and propagates the exception directly. */` |
|       3 | 5330 | `					PH7_MemObjRelease(&result);` |
|       3 | 5331 | `					PH7_MemObjRelease(&key1);` |
|       3 | 5332 | `					PH7_MemObjRelease(&key2);` |
|       3 | 5333 | `					return PH7_EXCEPTION;` |
|       - | 5334 | `				}` |
|      31 | 5335 | `				if( rc == SXRET_OK ){` |
|      31 | 5336 | `					if( (result.iFlags & MEMOBJ_INT) == 0 ){` |
|     ! 0 | 5337 | `						PH7_MemObjToInteger(&result);` |
|     ! 0 | 5338 | `					}` |
|      31 | 5339 | `					if( result.x.iVal == 0 ){` |
|       - | 5340 | `						/* keys considered equal by callback; now compare values */` |
|      13 | 5341 | `						ph7_value *pVal1 = HashmapExtractNodeValue(pEntry);` |
|      13 | 5342 | `						ph7_value *pVal2 = HashmapExtractNodeValue(pIt);` |
|      13 | 5343 | `						if( pVal1 && pVal2 ){` |
|      13 | 5344 | `							if( PH7_MemObjCmp(pVal1,pVal2,TRUE,0) == 0 ){` |
|       9 | 5345 | `								keep = 0;` |
|       9 | 5346 | `								PH7_MemObjRelease(&result);` |
|       - | 5347 | `								/* release keys too before breaking */` |
|       9 | 5348 | `								PH7_MemObjRelease(&key1);` |
|       9 | 5349 | `								PH7_MemObjRelease(&key2);` |
|       9 | 5350 | `								break;` |
|       - | 5351 | `							}` |
|       2 | 5352 | `						}` |
|       2 | 5353 | `					}` |
|      11 | 5354 | `				}` |
|      23 | 5355 | `				PH7_MemObjRelease(&result);` |
|      23 | 5356 | `				PH7_MemObjRelease(&key1);` |
|      23 | 5357 | `				PH7_MemObjRelease(&key2);` |
|       - | 5358 | `				/* move to next node */` |
|      23 | 5359 | `				pIt = pIt->pPrev;` |
|      23 | 5360 | `				if( keep == 0 ) break;` |
|       1 | 5361 | `			}` |
|      21 | 5362 | `			if( keep == 0 ) break;` |
|       7 | 5363 | `		}` |
|      17 | 5364 | `		if( keep ){` |
|       - | 5365 | `			/* Perform the insertion */` |
|       9 | 5366 | `			HashmapInsertNode((ph7_hashmap *)pArray->x.pOther,pEntry,TRUE);` |
|       4 | 5367 | `		}` |
|       - | 5368 | `		/* Point to the next entry */` |
|      17 | 5369 | `		pEntry = pEntry->pPrev; /* Reverse link */` |
|      17 | 5370 | `		n--;` |
|       1 | 5371 | `	}` |
|       - | 5372 | `	/* Return the freshly created array */` |
|       9 | 5373 | `	ph7_result_value(pCtx,pArray);` |
|       9 | 5374 | `	return PH7_OK;` |
|      17 | 5375 | `}` |
|       - | 5376 | `/*` |
|       - | 5377 | ` * array array_diff_key(array $array1 ,array $array2,...)` |
|       - | 5378 | ` *  Computes the difference of arrays using keys for comparison.` |
|       - | 5379 | ` * Parameters` |
|       - | 5380 | ` *  $array1` |
|       - | 5381 | ` *    The array to compare from` |
|       - | 5382 | ` *  $array2` |
|       - | 5383 | ` *    An array to compare against` |
|       - | 5384 | ` *  $...` |
|       - | 5385 | ` *   More arrays to compare against` |
|       - | 5386 | ` * Return` |
|       - | 5387 | ` *  Returns an array containing all the entries from array1 whose keys are not present` |
|       - | 5388 | ` *  in any of the other arrays.` |
|       - | 5389 | ` * Note that NULL is returned on failure.` |
|       - | 5390 | ` */` |
|      14 | 5391 | `static int ph7_hashmap_diff_key(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       4 | 5392 | `{` |
|       - | 5393 | `	ph7_hashmap_node *pEntry;` |
|       - | 5394 | `	ph7_hashmap *pSrc,*pMap;` |
|       - | 5395 | `	ph7_value *pArray;` |
|       - | 5396 | `	sxi32 rc;` |
|       - | 5397 | `	sxu32 n;` |
|       - | 5398 | `	int i;` |
|       - | 5399 | `	/* Validate arguments to mirror PHP behaviour. Previously invalid inputs` |
|       - | 5400 | `	 * would quietly return NULL which is inconsistent with other hashmap` |
|       - | 5401 | `	 * helpers. */` |
|      18 | 5402 | `	if( nArg < 1 ){` |
|       4 | 5403 | `		return PH7_VmThrowException(pCtx,` |
|       - | 5404 | `			"ArgumentCountError",` |
|       - | 5405 | `			"array_diff_key() expects at least 1 argument, %d given",` |
|       1 | 5406 | `			nArg` |
|       - | 5407 | `			);` |
|       - | 5408 | `	}` |
|      15 | 5409 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|       4 | 5410 | `		return PH7_VmThrowException(pCtx,` |
|       - | 5411 | `			"TypeError",` |
|       - | 5412 | `			"array_diff_key(): Argument #1 ($array) must be of type array, %s given",` |
|       1 | 5413 | `			ph7_type_name(apArg[0])` |
|       - | 5414 | `			);` |
|       - | 5415 | `	}` |
|      20 | 5416 | `	for(i = 1 ; i < nArg ; i++){` |
|      12 | 5417 | `		if( !ph7_value_is_array(apArg[i]) ){` |
|       4 | 5418 | `			return PH7_VmThrowException(pCtx,` |
|       - | 5419 | `				"TypeError",` |
|       - | 5420 | `				"array_diff_key(): Argument #%d must be of type array, %s given",` |
|       1 | 5421 | `				i + 1,` |
|       2 | 5422 | `				ph7_type_name(apArg[i])` |
|       - | 5423 | `				);` |
|       - | 5424 | `		}` |
|       5 | 5425 | `	}` |
|       9 | 5426 | `	if( nArg == 1 ){` |
|       - | 5427 | `		/* Return the first array since we cannot perform a diff */` |
|       3 | 5428 | `		ph7_result_value(pCtx,apArg[0]);` |
|       3 | 5429 | `		return PH7_OK;` |
|       - | 5430 | `	}` |
|       - | 5431 | `	/* Create a new array */` |
|       7 | 5432 | `	pArray = ph7_context_new_array(pCtx);` |
|       7 | 5433 | `	if( pArray == 0 ){` |
|     ! 0 | 5434 | `		ph7_result_null(pCtx);` |
|     ! 0 | 5435 | `		return PH7_OK;` |
|       - | 5436 | `	}` |
|       - | 5437 | `	/* Point to the internal representation of the main hashmap */` |
|       7 | 5438 | `	pSrc = (ph7_hashmap *)apArg[0]->x.pOther;` |
|       - | 5439 | `	/* Perfrom the diff */` |
|       7 | 5440 | `	pEntry = pSrc->pFirst;` |
|       7 | 5441 | `	n = pSrc->nEntry;` |
|      12 | 5442 | `	for(;;){` |
|      25 | 5443 | `		if( n < 1 ){` |
|       7 | 5444 | `			break;` |
|       - | 5445 | `		}` |
|      31 | 5446 | `		for( i = 1 ; i < nArg ; i++ ){` |
|      23 | 5447 | `			if( !ph7_value_is_array(apArg[i])) {` |
|       - | 5448 | `				/* ignore */` |
|     ! 0 | 5449 | `				continue;` |
|       - | 5450 | `			}` |
|      23 | 5451 | `			pMap = (ph7_hashmap *)apArg[i]->x.pOther;` |
|      23 | 5452 | `			if( pEntry->iType == HASHMAP_BLOB_NODE ){` |
|      17 | 5453 | `				SyBlob *pKey = &pEntry->xKey.sKey;` |
|       - | 5454 | `				/* Blob lookup */` |
|      17 | 5455 | `				rc = HashmapLookupBlobKey(pMap,SyBlobData(pKey),SyBlobLength(pKey),0);` |
|       9 | 5456 | `			}else{` |
|       - | 5457 | `				/* Int lookup */` |
|       7 | 5458 | `				rc = HashmapLookupIntKey(pMap,pEntry->xKey.iKey,0);` |
|       - | 5459 | `			}` |
|      23 | 5460 | `			if( rc == SXRET_OK ){` |
|       - | 5461 | `				/* Key exists,break immediately */` |
|      11 | 5462 | `				break;` |
|       - | 5463 | `			}` |
|       7 | 5464 | `		}` |
|      19 | 5465 | `		if( i >= nArg ){` |
|       - | 5466 | `			/* Perform the insertion */` |
|       9 | 5467 | `			HashmapInsertNode((ph7_hashmap *)pArray->x.pOther,pEntry,TRUE);` |
|       4 | 5468 | `		}` |
|       - | 5469 | `		/* Point to the next entry */` |
|      19 | 5470 | `		pEntry = pEntry->pPrev; /* Reverse link */` |
|      19 | 5471 | `		n--;` |
|       1 | 5472 | `	}` |
|       - | 5473 | `	/* Return the freshly created array */` |
|       7 | 5474 | `	ph7_result_value(pCtx,pArray);` |
|       7 | 5475 | `	return PH7_OK;` |
|      11 | 5476 | `}` |
|       - | 5477 | `/*` |
|       - | 5478 | ` * array array_intersect(array $array1 ,array $array2,...)` |
|       - | 5479 | ` *  Computes the intersection of arrays.` |
|       - | 5480 | ` * Parameters` |
|       - | 5481 | ` *  $array1` |
|       - | 5482 | ` *    The array to compare from` |
|       - | 5483 | ` *  $array2` |
|       - | 5484 | ` *    An array to compare against` |
|       - | 5485 | ` *  $...` |
|       - | 5486 | ` *   More arrays to compare against` |
|       - | 5487 | ` * Return` |
|       - | 5488 | ` *  Returns an array containing all of the values in array1 whose values exist` |
|       - | 5489 | ` *  in all of the parameters.` |
|       - | 5490 | ` * Throws ArgumentCountError if no arguments are given.` |
|       - | 5491 | ` * Throws TypeError if any argument is not an array.` |
|       - | 5492 | ` */` |
|      22 | 5493 | `static int ph7_hashmap_intersect(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       4 | 5494 | `{` |
|       - | 5495 | `	ph7_hashmap_node *pEntry;` |
|       - | 5496 | `	ph7_hashmap *pSrc,*pMap;` |
|       - | 5497 | `	ph7_value *pArray;` |
|       - | 5498 | `	ph7_value *pVal;` |
|       - | 5499 | `	sxi32 rc;` |
|       - | 5500 | `	sxu32 n;` |
|       - | 5501 | `	int i;` |
|      26 | 5502 | `	if( nArg < 1 ){` |
|       4 | 5503 | `		return PH7_VmThrowException(pCtx,` |
|       - | 5504 | `			"ArgumentCountError",` |
|       - | 5505 | `			"array_intersect() expects at least 1 argument, %d given",` |
|       1 | 5506 | `			nArg` |
|       - | 5507 | `			);` |
|       - | 5508 | `	}` |
|      23 | 5509 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|       4 | 5510 | `		return PH7_VmThrowException(pCtx,` |
|       - | 5511 | `			"TypeError",` |
|       - | 5512 | `			"array_intersect(): Argument #1 ($array) must be of type array, %s given",` |
|       1 | 5513 | `			ph7_type_name(apArg[0])` |
|       - | 5514 | `			);` |
|       - | 5515 | `	}` |
|      36 | 5516 | `	for( i = 1 ; i < nArg ; i++ ){` |
|      20 | 5517 | `		if( !ph7_value_is_array(apArg[i]) ){` |
|       4 | 5518 | `			return PH7_VmThrowException(pCtx,` |
|       - | 5519 | `				"TypeError",` |
|       - | 5520 | `				"array_intersect(): Argument #%d must be of type array, %s given",` |
|       1 | 5521 | `				i + 1,` |
|       2 | 5522 | `				ph7_type_name(apArg[i])` |
|       - | 5523 | `				);` |
|       - | 5524 | `		}` |
|       9 | 5525 | `	}` |
|      17 | 5526 | `	if( nArg == 1 ){` |
|       - | 5527 | `		/* Return the first array since we cannot perform a diff */` |
|       3 | 5528 | `		ph7_result_value(pCtx,apArg[0]);` |
|       3 | 5529 | `		return PH7_OK;` |
|       - | 5530 | `	}` |
|       - | 5531 | `	/* Create a new array */` |
|      15 | 5532 | `	pArray = ph7_context_new_array(pCtx);` |
|      15 | 5533 | `	if( pArray == 0 ){` |
|     ! 0 | 5534 | `		ph7_result_null(pCtx);` |
|     ! 0 | 5535 | `		return PH7_OK;` |
|       - | 5536 | `	}` |
|       - | 5537 | `	/* Point to the internal representation of the source hashmap */` |
|      15 | 5538 | `	pSrc = (ph7_hashmap *)apArg[0]->x.pOther;` |
|       - | 5539 | `	/* Perform the intersection */` |
|      15 | 5540 | `	pEntry = pSrc->pFirst;` |
|      15 | 5541 | `	n = pSrc->nEntry;` |
|      31 | 5542 | `	for(;;){` |
|      63 | 5543 | `		if( n < 1 ){` |
|      15 | 5544 | `			break;` |
|       - | 5545 | `		}` |
|       - | 5546 | `		/* Extract the node value */` |
|      49 | 5547 | `		pVal = HashmapExtractNodeValue(pEntry);` |
|      49 | 5548 | `		if( pVal ){` |
|      79 | 5549 | `			for( i = 1 ; i < nArg ; i++ ){` |
|       - | 5550 | `				/* Point to the internal representation of the hashmap */` |
|      55 | 5551 | `				pMap = (ph7_hashmap *)apArg[i]->x.pOther;` |
|       - | 5552 | `				/* Perform the lookup */` |
|      55 | 5553 | `				rc = HashmapFindValue(pMap,pVal,0,TRUE);` |
|      55 | 5554 | `				if( rc != SXRET_OK ){` |
|       - | 5555 | `					/* Value does not exist */` |
|      25 | 5556 | `					break;` |
|       - | 5557 | `				}` |
|      16 | 5558 | `			}` |
|      49 | 5559 | `			if( i >= nArg ){` |
|       - | 5560 | `				/* Perform the insertion */` |
|      25 | 5561 | `				HashmapInsertNode((ph7_hashmap *)pArray->x.pOther,pEntry,TRUE);` |
|      12 | 5562 | `			}` |
|      24 | 5563 | `		}` |
|       - | 5564 | `		/* Point to the next entry */` |
|      49 | 5565 | `		pEntry = pEntry->pPrev; /* Reverse link */` |
|      49 | 5566 | `		n--;` |
|       1 | 5567 | `	}` |
|       - | 5568 | `	/* Return the freshly created array */` |
|      15 | 5569 | `	ph7_result_value(pCtx,pArray);` |
|      15 | 5570 | `	return PH7_OK;` |
|      15 | 5571 | `}` |
|       - | 5572 | `/*` |
|       - | 5573 | ` * array array_intersect_assoc(array $array1 ,array $array2,...)` |
|       - | 5574 | ` *  Computes the intersection of arrays with additional index check.` |
|       - | 5575 | ` * Parameters` |
|       - | 5576 | ` *  $array1` |
|       - | 5577 | ` *    The array to compare from` |
|       - | 5578 | ` *  $array2` |
|       - | 5579 | ` *    An array to compare against` |
|       - | 5580 | ` *  $...` |
|       - | 5581 | ` *   More arrays to compare against` |
|       - | 5582 | ` * Return` |
|       - | 5583 | ` *  Returns an array containing all the values of array1 that are present` |
|       - | 5584 | ` *  in all the arguments, with matching keys.` |
|       - | 5585 | ` */` |
|      22 | 5586 | `static int ph7_hashmap_intersect_assoc(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       4 | 5587 | `{` |
|       - | 5588 | `	ph7_hashmap_node *pEntry,*pN1,*pN2;` |
|       - | 5589 | `	ph7_hashmap *pSrc,*pMap;` |
|       - | 5590 | `	ph7_value *pArray;` |
|       - | 5591 | `	ph7_value *pVal;` |
|       - | 5592 | `	sxi32 rc;` |
|       - | 5593 | `	sxu32 n;` |
|       - | 5594 | `	int i;` |
|      26 | 5595 | `	if( nArg < 1 ){` |
|       4 | 5596 | `		return PH7_VmThrowException(pCtx,` |
|       - | 5597 | `			"ArgumentCountError",` |
|       - | 5598 | `			"array_intersect_assoc() expects at least 1 argument, %d given",` |
|       1 | 5599 | `			nArg` |
|       - | 5600 | `			);` |
|       - | 5601 | `	}` |
|      23 | 5602 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|       4 | 5603 | `		return PH7_VmThrowException(pCtx,` |
|       - | 5604 | `			"TypeError",` |
|       - | 5605 | `			"array_intersect_assoc(): Argument #1 ($array) must be of type array, %s given",` |
|       1 | 5606 | `			ph7_type_name(apArg[0])` |
|       - | 5607 | `			);` |
|       - | 5608 | `	}` |
|      36 | 5609 | `	for( i = 1 ; i < nArg ; i++ ){` |
|      20 | 5610 | `		if( !ph7_value_is_array(apArg[i]) ){` |
|       4 | 5611 | `			return PH7_VmThrowException(pCtx,` |
|       - | 5612 | `				"TypeError",` |
|       - | 5613 | `				"array_intersect_assoc(): Argument #%d must be of type array, %s given",` |
|       1 | 5614 | `				i + 1,` |
|       2 | 5615 | `				ph7_type_name(apArg[i])` |
|       - | 5616 | `				);` |
|       - | 5617 | `		}` |
|       9 | 5618 | `	}` |
|      17 | 5619 | `	if( nArg == 1 ){` |
|       - | 5620 | `		/* Return the first array since we cannot perform an intersection */` |
|       3 | 5621 | `		ph7_result_value(pCtx,apArg[0]);` |
|       3 | 5622 | `		return PH7_OK;` |
|       - | 5623 | `	}` |
|       - | 5624 | `	/* Create a new array */` |
|      15 | 5625 | `	pArray = ph7_context_new_array(pCtx);` |
|      15 | 5626 | `	if( pArray == 0 ){` |
|     ! 0 | 5627 | `		ph7_result_null(pCtx);` |
|     ! 0 | 5628 | `		return PH7_OK;` |
|       - | 5629 | `	}` |
|       - | 5630 | `	/* Point to the internal representation of the source hashmap */` |
|      15 | 5631 | `	pSrc = (ph7_hashmap *)apArg[0]->x.pOther;` |
|       - | 5632 | `	/* Perform the intersection */` |
|      15 | 5633 | `	pEntry = pSrc->pFirst;` |
|      15 | 5634 | `	n = pSrc->nEntry;` |
|      15 | 5635 | `	pN1 = pN2 = 0; /* cc warning */` |
|      23 | 5636 | `	for(;;){` |
|      47 | 5637 | `		if( n < 1 ){` |
|      15 | 5638 | `			break;` |
|       - | 5639 | `		}` |
|       - | 5640 | `		/* Extract the node value */` |
|      33 | 5641 | `		pVal = HashmapExtractNodeValue(pEntry);` |
|      33 | 5642 | `		if( pVal ){` |
|      53 | 5643 | `			for( i = 1 ; i < nArg ; i++ ){` |
|       - | 5644 | `				/* Point to the internal representation of the hashmap */` |
|      37 | 5645 | `				pMap = (ph7_hashmap *)apArg[i]->x.pOther;` |
|       - | 5646 | `				/* Perform a key lookup first */` |
|      37 | 5647 | `				if( pEntry->iType == HASHMAP_INT_NODE ){` |
|      15 | 5648 | `					rc = HashmapLookupIntKey(pMap,pEntry->xKey.iKey,&pN1);` |
|       8 | 5649 | `				}else{` |
|      23 | 5650 | `					rc = HashmapLookupBlobKey(pMap,SyBlobData(&pEntry->xKey.sKey),SyBlobLength(&pEntry->xKey.sKey),&pN1);` |
|       - | 5651 | `				}` |
|      37 | 5652 | `				if( rc != SXRET_OK ){` |
|       - | 5653 | `					/* No such key,break immediately */` |
|       7 | 5654 | `					break;` |
|       - | 5655 | `				}` |
|       - | 5656 | `				/* Perform the lookup */` |
|      31 | 5657 | `				rc = HashmapFindValue(pMap,pVal,&pN2,TRUE);` |
|      31 | 5658 | `				if( rc != SXRET_OK \|\| pN1 != pN2 ){` |
|       - | 5659 | `					/* Value does not exist */` |
|       6 | 5660 | `					break;` |
|       - | 5661 | `				}` |
|      11 | 5662 | `			}` |
|      33 | 5663 | `			if( i >= nArg ){` |
|       - | 5664 | `				/* Perform the insertion */` |
|      17 | 5665 | `				HashmapInsertNode((ph7_hashmap *)pArray->x.pOther,pEntry,TRUE);` |
|       8 | 5666 | `			}` |
|      16 | 5667 | `		}` |
|       - | 5668 | `		/* Point to the next entry */` |
|      33 | 5669 | `		pEntry = pEntry->pPrev; /* Reverse link */` |
|      33 | 5670 | `		n--;` |
|       1 | 5671 | `	}` |
|       - | 5672 | `	/* Return the freshly created array */` |
|      15 | 5673 | `	ph7_result_value(pCtx,pArray);` |
|      15 | 5674 | `	return PH7_OK;` |
|      15 | 5675 | `}` |
|       - | 5676 | `/*` |
|       - | 5677 | ` * array array_intersect_key(array $array1 ,...)` |
|       - | 5678 | ` *  Computes the intersection of arrays using keys for comparison.` |
|       - | 5679 | ` * Parameters` |
|       - | 5680 | ` *  $array1` |
|       - | 5681 | ` *    The array to compare from` |
|       - | 5682 | ` *  $...` |
|       - | 5683 | ` *   More arrays to compare against` |
|       - | 5684 | ` * Return` |
|       - | 5685 | ` *  Returns an associative array containing all the entries of array1 which` |
|       - | 5686 | ` *  have keys that are present in all arguments.` |
|       - | 5687 | ` * Note that NULL is returned on failure.` |
|       - | 5688 | ` */` |
|      22 | 5689 | `static int ph7_hashmap_intersect_key(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       4 | 5690 | `{` |
|       - | 5691 | `	ph7_hashmap_node *pEntry;` |
|       - | 5692 | `	ph7_hashmap *pSrc,*pMap;` |
|       - | 5693 | `	ph7_value *pArray;` |
|       - | 5694 | `	sxi32 rc;` |
|       - | 5695 | `	sxu32 n;` |
|       - | 5696 | `	int i;` |
|      26 | 5697 | `	if( nArg < 1 ){` |
|       4 | 5698 | `		return PH7_VmThrowException(pCtx,` |
|       - | 5699 | `			"ArgumentCountError",` |
|       - | 5700 | `			"array_intersect_key() expects at least 1 argument, %d given",` |
|       1 | 5701 | `			nArg` |
|       - | 5702 | `			);` |
|       - | 5703 | `	}` |
|      23 | 5704 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|       4 | 5705 | `		return PH7_VmThrowException(pCtx,` |
|       - | 5706 | `			"TypeError",` |
|       - | 5707 | `			"array_intersect_key(): Argument #1 ($array) must be of type array, %s given",` |
|       1 | 5708 | `			ph7_type_name(apArg[0])` |
|       - | 5709 | `			);` |
|       - | 5710 | `	}` |
|      36 | 5711 | `	for( i = 1 ; i < nArg ; i++ ){` |
|      20 | 5712 | `		if( !ph7_value_is_array(apArg[i]) ){` |
|       4 | 5713 | `			return PH7_VmThrowException(pCtx,` |
|       - | 5714 | `				"TypeError",` |
|       - | 5715 | `				"array_intersect_key(): Argument #%d must be of type array, %s given",` |
|       1 | 5716 | `				i + 1,` |
|       2 | 5717 | `				ph7_type_name(apArg[i])` |
|       - | 5718 | `				);` |
|       - | 5719 | `		}` |
|       9 | 5720 | `	}` |
|      17 | 5721 | `	if( nArg == 1 ){` |
|       - | 5722 | `		/* Return the first array since we cannot perform an intersection */` |
|       3 | 5723 | `		ph7_result_value(pCtx,apArg[0]);` |
|       3 | 5724 | `		return PH7_OK;` |
|       - | 5725 | `	}` |
|       - | 5726 | `	/* Create a new array */` |
|      15 | 5727 | `	pArray = ph7_context_new_array(pCtx);` |
|      15 | 5728 | `	if( pArray == 0 ){` |
|     ! 0 | 5729 | `		ph7_result_null(pCtx);` |
|     ! 0 | 5730 | `		return PH7_OK;` |
|       - | 5731 | `	}` |
|       - | 5732 | `	/* Point to the internal representation of the main hashmap */` |
|      15 | 5733 | `	pSrc = (ph7_hashmap *)apArg[0]->x.pOther;` |
|       - | 5734 | `	/* Perform the intersection */` |
|      15 | 5735 | `	pEntry = pSrc->pFirst;` |
|      15 | 5736 | `	n = pSrc->nEntry;` |
|      24 | 5737 | `	for(;;){` |
|      49 | 5738 | `		if( n < 1 ){` |
|      15 | 5739 | `			break;` |
|       - | 5740 | `		}` |
|      57 | 5741 | `		for( i = 1 ; i < nArg ; i++ ){` |
|      39 | 5742 | `			pMap = (ph7_hashmap *)apArg[i]->x.pOther;` |
|      39 | 5743 | `			if( pEntry->iType == HASHMAP_BLOB_NODE ){` |
|      27 | 5744 | `				SyBlob *pKey = &pEntry->xKey.sKey;` |
|       - | 5745 | `				/* Blob lookup */` |
|      27 | 5746 | `				rc = HashmapLookupBlobKey(pMap,SyBlobData(pKey),SyBlobLength(pKey),0);` |
|      14 | 5747 | `			}else{` |
|       - | 5748 | `				/* Int key */` |
|      13 | 5749 | `				rc = HashmapLookupIntKey(pMap,pEntry->xKey.iKey,0);` |
|       - | 5750 | `			}` |
|      39 | 5751 | `			if( rc != SXRET_OK ){` |
|       - | 5752 | `				/* Key does not exist, break immediately */` |
|      17 | 5753 | `				break;` |
|       - | 5754 | `			}` |
|      12 | 5755 | `		}` |
|      35 | 5756 | `		if( i >= nArg ){` |
|       - | 5757 | `			/* Perform the insertion */` |
|      19 | 5758 | `			HashmapInsertNode((ph7_hashmap *)pArray->x.pOther,pEntry,TRUE);` |
|       9 | 5759 | `		}` |
|       - | 5760 | `		/* Point to the next entry */` |
|      35 | 5761 | `		pEntry = pEntry->pPrev; /* Reverse link */` |
|      35 | 5762 | `		n--;` |
|       1 | 5763 | `	}` |
|       - | 5764 | `	/* Return the freshly created array */` |
|      15 | 5765 | `	ph7_result_value(pCtx,pArray);` |
|      15 | 5766 | `	return PH7_OK;` |
|      15 | 5767 | `}` |
|       - | 5768 | `/*` |
|       - | 5769 | ` * array array_uintersect(array $array1 ,array $array2,...,$callback)` |
|       - | 5770 | ` *  Computes the intersection of arrays.` |
|       - | 5771 | ` * Parameters` |
|       - | 5772 | ` *  $array1` |
|       - | 5773 | ` *    The array to compare from` |
|       - | 5774 | ` *  $array2` |
|       - | 5775 | ` *    An array to compare against` |
|       - | 5776 | ` *  $...` |
|       - | 5777 | ` *   More arrays to compare against` |
|       - | 5778 | ` * $callback` |
|       - | 5779 | ` *  The callback comparison function.` |
|       - | 5780 | ` *  The comparison function must return an integer less than, equal to, or greater than zero` |
|       - | 5781 | ` *  if the first argument is considered to be respectively less than, equal to, or greater` |
|       - | 5782 | ` *  than the second.` |
|       - | 5783 | ` *     int callback ( mixed $a, mixed $b )` |
|       - | 5784 | ` * Return` |
|       - | 5785 | ` *  Returns an array containing all of the values in array1 whose values exist` |
|       - | 5786 | ` *  in all of the parameters. .` |
|       - | 5787 | ` * Note that NULL is returned on failure.` |
|       - | 5788 | ` */` |
|      26 | 5789 | `static int ph7_hashmap_uintersect(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       5 | 5790 | `{` |
|       - | 5791 | `	ph7_hashmap_node *pEntry;` |
|       - | 5792 | `	ph7_hashmap *pSrc,*pMap;` |
|       - | 5793 | `	ph7_value *pCallback;` |
|       - | 5794 | `	ph7_value *pArray;` |
|       - | 5795 | `	ph7_value *pVal;` |
|       - | 5796 | `	sxi32 rc;` |
|       - | 5797 | `	sxu32 n;` |
|       - | 5798 | `	int i;` |
|       - | 5799 |  |
|       - | 5800 | `	/* Ensure the argument count matches PHP behaviour. */` |
|      31 | 5801 | `	if( nArg < 2 ){` |
|       4 | 5802 | `		return PH7_VmThrowException(pCtx,` |
|       - | 5803 | `			"ArgumentCountError",` |
|       - | 5804 | `			"array_uintersect() expects at least 2 arguments, %d given",` |
|       1 | 5805 | `			nArg` |
|       - | 5806 | `			);` |
|       - | 5807 | `	}` |
|      29 | 5808 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|       4 | 5809 | `		return PH7_VmThrowException(pCtx,` |
|       - | 5810 | `			"TypeError",` |
|       - | 5811 | `			"array_uintersect(): Argument #1 ($array) must be of type array, %s given",` |
|       1 | 5812 | `			ph7_type_name(apArg[0])` |
|       - | 5813 | `			);` |
|       - | 5814 | `	}` |
|       - | 5815 |  |
|      27 | 5816 | `	if( nArg == 2 ){` |
|       - | 5817 | `		/* Only the original array and the callback were provided. */` |
|       - | 5818 | `		/* Validate the callback below in order to match PHP's parameter` |
|       - | 5819 | `		 * validation ordering. */` |
|       3 | 5820 | `	} else {` |
|       - | 5821 | `		/* Ensure intermediary arguments are arrays (matches PHP strict typing). */` |
|      39 | 5822 | `		for( i = 1 ; i < nArg - 1; i++ ){` |
|      23 | 5823 | `			if( !ph7_value_is_array(apArg[i]) ){` |
|       4 | 5824 | `				return PH7_VmThrowException(pCtx,` |
|       - | 5825 | `					"TypeError",` |
|       - | 5826 | `					"array_uintersect(): Argument #%d must be of type array, %s given",` |
|       1 | 5827 | `					i + 1,` |
|       2 | 5828 | `					ph7_type_name(apArg[i])` |
|       - | 5829 | `					);` |
|       - | 5830 | `			}` |
|      13 | 5831 | `		}` |
|       - | 5832 | `	}` |
|       - | 5833 |  |
|       - | 5834 | `	/* Identify the callback (always expected as the last argument). */` |
|      25 | 5835 | `	pCallback = apArg[nArg - 1];` |
|       - | 5836 | `	/* Validate the callback to match PHP's error messages. */` |
|      25 | 5837 | `	if( !ph7_value_is_callable(pCallback) ){` |
|      14 | 5838 | `		if( ph7_value_is_array(pCallback) ){` |
|       - | 5839 | `			/* PHP emits a special message when the array length is wrong.` |
|       - | 5840 | `			 * If the array has two elements but is still not callable (e.g. missing` |
|       - | 5841 | `			 * method / missing class), we must emit a more general error instead.` |
|       - | 5842 | `			 */` |
|       9 | 5843 | `			ph7_hashmap *pCb = (ph7_hashmap *)pCallback->x.pOther;` |
|       9 | 5844 | `			if( pCb->nEntry != 2 ){` |
|       4 | 5845 | `				return PH7_VmThrowException(pCtx,` |
|       - | 5846 | `					"TypeError",` |
|       - | 5847 | `					"array_uintersect(): Argument #%d must be a valid callback, array callback must have exactly two members",` |
|       1 | 5848 | `					nArg` |
|       - | 5849 | `					);` |
|       - | 5850 | `			}` |
|       - | 5851 | `			/* Try to provide a more precise error like PHP does for missing classes/methods. */` |
|       - | 5852 | `			{` |
|       6 | 5853 | `				ph7_value *pKey = (ph7_value *)SySetAt(&pCtx->pVm->aMemObj,pCb->pFirst->nValIdx);` |
|       6 | 5854 | `				ph7_value *pMethod = (ph7_value *)SySetAt(&pCtx->pVm->aMemObj,pCb->pFirst->pPrev->nValIdx);` |
|       6 | 5855 | `				if( pKey && pMethod && (pMethod->iFlags & MEMOBJ_STRING) ){` |
|       - | 5856 | `					int nMethodLen;` |
|       6 | 5857 | `					const char *zMethod = ph7_value_to_string(pMethod,&nMethodLen);` |
|       6 | 5858 | `					ph7_class *pClass = PH7_VmExtractClassFromValue(pCtx->pVm,pKey);` |
|       6 | 5859 | `					if( pClass ){` |
|       - | 5860 | `						/* Class exists but method is missing. */` |
|       4 | 5861 | `						return PH7_VmThrowException(pCtx,` |
|       - | 5862 | `							"TypeError",` |
|       - | 5863 | `							"array_uintersect(): Argument #%d must be a valid callback, class %s does not have a method \"%s\"",` |
|       1 | 5864 | `							nArg,` |
|       1 | 5865 | `							(const char *)SyStringData(&pClass->sName),` |
|       1 | 5866 | `							zMethod` |
|       - | 5867 | `							);` |
|       - | 5868 | `					}` |
|       - | 5869 | `					/* Class not found */` |
|       - | 5870 | `					{` |
|       - | 5871 | `						int nName;` |
|       3 | 5872 | `						const char *zName = ph7_value_to_string(pKey,&nName);` |
|       4 | 5873 | `						return PH7_VmThrowException(pCtx,` |
|       - | 5874 | `							"TypeError",` |
|       - | 5875 | `							"array_uintersect(): Argument #%d must be a valid callback, class \"%s\" not found",` |
|       1 | 5876 | `							nArg,` |
|       1 | 5877 | `							zName` |
|       - | 5878 | `							);` |
|       - | 5879 | `					}` |
|       - | 5880 | `				}` |
|       - | 5881 | `			}` |
|       - | 5882 | `			/* Fallback message */` |
|     ! 0 | 5883 | `			return PH7_VmThrowException(pCtx,` |
|       - | 5884 | `				"TypeError",` |
|       - | 5885 | `				"array_uintersect(): Argument #%d must be a valid callback, no array or string given",` |
|     ! 0 | 5886 | `				nArg` |
|       - | 5887 | `				);` |
|       - | 5888 | `		}` |
|       6 | 5889 | `		if( ph7_value_is_string(pCallback) ){` |
|       - | 5890 | `			int len;` |
|       3 | 5891 | `			const char *zName = ph7_value_to_string(pCallback, &len);` |
|       4 | 5892 | `			return PH7_VmThrowException(pCtx,` |
|       - | 5893 | `				"TypeError",` |
|       - | 5894 | `				"array_uintersect(): Argument #%d must be a valid callback, function \"%s\" not found or invalid function name",` |
|       1 | 5895 | `				nArg,` |
|       1 | 5896 | `				zName` |
|       - | 5897 | `				);` |
|       - | 5898 | `		}` |
|       4 | 5899 | `		return PH7_VmThrowException(pCtx,` |
|       - | 5900 | `			"TypeError",` |
|       - | 5901 | `			"array_uintersect(): Argument #%d must be a valid callback, no array or string given",` |
|       1 | 5902 | `			nArg` |
|       - | 5903 | `			);` |
|       - | 5904 | `	}` |
|       - | 5905 |  |
|      11 | 5906 | `	if( nArg == 2 ){` |
|       - | 5907 | `		/* Only the original array and the callback were provided. */` |
|       5 | 5908 | `		ph7_result_value(pCtx,apArg[0]);` |
|       5 | 5909 | `		return PH7_OK;` |
|       - | 5910 | `	}` |
|       - | 5911 |  |
|       - | 5912 | `	/* Create a new array */` |
|       7 | 5913 | `	pArray = ph7_context_new_array(pCtx);` |
|       7 | 5914 | `	if( pArray == 0 ){` |
|     ! 0 | 5915 | `		ph7_result_null(pCtx);` |
|     ! 0 | 5916 | `		return PH7_OK;` |
|       - | 5917 | `	}` |
|       - | 5918 | `	/* Point to the internal representation of the source hashmap */` |
|       7 | 5919 | `	pSrc = (ph7_hashmap *)apArg[0]->x.pOther;` |
|       - | 5920 | `	/* Perform the intersection */` |
|       7 | 5921 | `	pEntry = pSrc->pFirst;` |
|       7 | 5922 | `	n = pSrc->nEntry;` |
|       7 | 5923 | `	pCtx->pVm->iCmpCallbackExc = 0;` |
|       9 | 5924 | `	for(;;){` |
|      19 | 5925 | `		if( n < 1 ){` |
|       5 | 5926 | `			break;` |
|       - | 5927 | `		}` |
|       - | 5928 | `		/* Extract the node value */` |
|      15 | 5929 | `		pVal = HashmapExtractNodeValue(pEntry);` |
|      15 | 5930 | `		if( pVal ){` |
|      23 | 5931 | `			for( i = 1 ; i < nArg - 1; i++ ){` |
|      15 | 5932 | `				if( !ph7_value_is_array(apArg[i])) {` |
|       - | 5933 | `					/* ignore */` |
|     ! 0 | 5934 | `					continue;` |
|       - | 5935 | `				}` |
|       - | 5936 | `				/* Point to the internal representation of the hashmap */` |
|      15 | 5937 | `				pMap = (ph7_hashmap *)apArg[i]->x.pOther;` |
|       - | 5938 | `				/* Perform the lookup */` |
|      15 | 5939 | `				rc = HashmapFindValueByCallback(pMap,pVal,pCallback,0);` |
|      15 | 5940 | `				if( rc != SXRET_OK ){` |
|       - | 5941 | `					/* Value does not exist */` |
|       7 | 5942 | `					break;` |
|       - | 5943 | `				}` |
|       5 | 5944 | `			}` |
|      15 | 5945 | `			if( i >= (nArg-1) ){` |
|       - | 5946 | `				/* Perform the insertion */` |
|       9 | 5947 | `				HashmapInsertNode((ph7_hashmap *)pArray->x.pOther,pEntry,TRUE);` |
|       4 | 5948 | `			}` |
|       7 | 5949 | `		}` |
|      15 | 5950 | `		if( pCtx->pVm->iCmpCallbackExc ){` |
|       - | 5951 | `			/* The comparison callback raised: propagate so the dispatcher unwinds. */` |
|       3 | 5952 | `			pCtx->pVm->iCmpCallbackExc = 0;` |
|       3 | 5953 | `			return PH7_EXCEPTION;` |
|       - | 5954 | `		}` |
|       - | 5955 | `		/* Point to the next entry */` |
|      13 | 5956 | `		pEntry = pEntry->pPrev; /* Reverse link */` |
|      13 | 5957 | `		n--;` |
|       1 | 5958 | `	}` |
|       - | 5959 | `	/* Return the freshly created array */` |
|       5 | 5960 | `	ph7_result_value(pCtx,pArray);` |
|       5 | 5961 | `	return PH7_OK;` |
|      18 | 5962 | `}` |
|       - | 5963 | `/*` |
|       - | 5964 | ` * array array_fill(int $start_index,int $num,var $value)` |
|       - | 5965 | ` *  Fill an array with values.` |
|       - | 5966 | ` * Parameters` |
|       - | 5967 | ` *  $start_index` |
|       - | 5968 | ` *    The first index of the returned array.` |
|       - | 5969 | ` *  $num` |
|       - | 5970 | ` *   Number of elements to insert.` |
|       - | 5971 | ` *  $value` |
|       - | 5972 | ` *    Value to use for filling.` |
|       - | 5973 | ` * Return` |
|       - | 5974 | ` *  The filled array or null on failure.` |
|       - | 5975 | ` */` |
|     238 | 5976 | `static int ph7_hashmap_fill(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       5 | 5977 | `{` |
|       - | 5978 | `	ph7_value *pArray;` |
|       - | 5979 | `	int i,nEntry;` |
|       - | 5980 |  |
|       - | 5981 | `	/* PHP enforces argument count and type checks. */` |
|     243 | 5982 | `	if( nArg != 3 ){` |
|       - | 5983 | `		/* wrong number of arguments -> ArgumentCountError */` |
|       8 | 5984 | `		return PH7_VmThrowException(pCtx,` |
|       - | 5985 | `			"ArgumentCountError",` |
|       - | 5986 | `			"array_fill() expects exactly 3 arguments, %d given",` |
|       2 | 5987 | `			nArg` |
|       - | 5988 | `			);` |
|       - | 5989 | `	}` |
|       - | 5990 |  |
|       - | 5991 | `	/* Argument #1: start index must be convertible to int.  Accept booleans,` |
|       - | 5992 | `	 * floats, and numeric strings (including those with decimal point) by` |
|       - | 5993 | `	 * allowing them through the conversion.  Only arrays, objects, resources` |
|       - | 5994 | `	 * and NULLs are rejected outright. */` |
|     350 | 5995 | `	if( ph7_value_is_array(apArg[0]) \|\| ph7_value_is_object(apArg[0]) \|\|` |
|     353 | 5996 | `		ph7_value_is_resource(apArg[0]) \|\| ph7_value_is_null(apArg[0]) ){` |
|       4 | 5997 | `		return PH7_VmThrowException(pCtx,` |
|       - | 5998 | `			"TypeError",` |
|       - | 5999 | `			"array_fill(): Argument #1 ($start_index) must be of type int, %s given",` |
|       1 | 6000 | `			ph7_type_name(apArg[0])` |
|       - | 6001 | `			);` |
|       - | 6002 | `	}` |
|     236 | 6003 | `	if( ph7_value_is_string(apArg[0]) ){` |
|       - | 6004 | `		int len;` |
|       8 | 6005 | `		sxu8 bReal = FALSE;` |
|       8 | 6006 | `		const char *zStr = ph7_value_to_string(apArg[0], &len);` |
|       8 | 6007 | `		if( SyStrIsNumeric(zStr, len, &bReal, 0) != SXRET_OK ){` |
|       - | 6008 | `			/* Non‑numeric string is an error. */` |
|       3 | 6009 | `			return PH7_VmThrowException(pCtx,` |
|       - | 6010 | `				"TypeError",` |
|       - | 6011 | `				"array_fill(): Argument #1 ($start_index) must be of type int, string given"` |
|       - | 6012 | `				);` |
|       - | 6013 | `		}` |
|       5 | 6014 | `		if( bReal ){` |
|       - | 6015 | `			/* float-string -> deprecation warning */` |
|       4 | 6016 | `			ph7_context_throw_error_format(pCtx, E_DEPRECATED,` |
|       - | 6017 | `				"Implicit conversion from float-string \"%s\" to int loses precision",` |
|       1 | 6018 | `				zStr` |
|       - | 6019 | `				);` |
|       1 | 6020 | `		}` |
|       2 | 6021 | `	}` |
|       - | 6022 |  |
|       - | 6023 | `	/* Argument #2: count must be convertible to non-negative int.  Allow booleans,` |
|       - | 6024 | `	 * floats and numeric strings; reject arrays, objects, resources and NULL. */` |
|     345 | 6025 | `	if( ph7_value_is_array(apArg[1]) \|\| ph7_value_is_object(apArg[1]) \|\|` |
|     348 | 6026 | `		ph7_value_is_resource(apArg[1]) \|\| ph7_value_is_null(apArg[1]) ){` |
|     ! 0 | 6027 | `		return PH7_VmThrowException(pCtx,` |
|       - | 6028 | `			"TypeError",` |
|       - | 6029 | `			"array_fill(): Argument #2 ($count) must be of type int, %s given",` |
|     ! 0 | 6030 | `			ph7_type_name(apArg[1])` |
|       - | 6031 | `			);` |
|       - | 6032 | `	}` |
|     233 | 6033 | `	if( ph7_value_is_string(apArg[1]) ){` |
|       - | 6034 | `		int len;` |
|       3 | 6035 | `		sxu8 bReal = FALSE;` |
|       3 | 6036 | `		const char *zStr = ph7_value_to_string(apArg[1], &len);` |
|       3 | 6037 | `		if( SyStrIsNumeric(zStr, len, &bReal, 0) != SXRET_OK ){` |
|       3 | 6038 | `			return PH7_VmThrowException(pCtx,` |
|       - | 6039 | `				"TypeError",` |
|       - | 6040 | `				"array_fill(): Argument #2 ($count) must be of type int, string given"` |
|       - | 6041 | `				);` |
|       - | 6042 | `		}` |
|     ! 0 | 6043 | `	}` |
|       - | 6044 | `	/* Note: booleans and floats (including fractional) are now accepted; they` |
|       - | 6045 | `	 * will be converted by ph7_value_to_int below. */` |
|     230 | 6046 | `	if( ph7_value_is_float(apArg[1]) ){` |
|       5 | 6047 | `		double d = ph7_value_to_double(apArg[1]);` |
|       - | 6048 | `		/* avoid hiding outer 'i' (loop index) */` |
|       5 | 6049 | `		sxi64 i64 = (sxi64)d;` |
|       5 | 6050 | `		if( d != (double)i64 ){` |
|       7 | 6051 | `			ph7_context_throw_error_format(pCtx, E_DEPRECATED,` |
|       - | 6052 | `				"Implicit conversion from float %g to int loses precision",` |
|       2 | 6053 | `				d` |
|       - | 6054 | `				);` |
|       2 | 6055 | `		}` |
|       2 | 6056 | `	}` |
|       - | 6057 |  |
|       - | 6058 | `	/* Total number of entries to insert */` |
|     230 | 6059 | `	nEntry = ph7_value_to_int(apArg[1]);` |
|       - | 6060 | `	/* Reject negative counts with a ValueError like PHP. */` |
|     230 | 6061 | `	if( nEntry < 0 ){` |
|       3 | 6062 | `		return PH7_VmThrowException(pCtx,` |
|       - | 6063 | `			"ValueError",` |
|       - | 6064 | `			"array_fill(): Argument #2 ($count) must be greater than or equal to 0"` |
|       - | 6065 | `			);` |
|       - | 6066 | `	}` |
|       - | 6067 |  |
|       - | 6068 | `	/* If zero elements were requested, return an empty array without allocating */` |
|     227 | 6069 | `	if( nEntry == 0 ){` |
|       7 | 6070 | `		ph7_result_value(pCtx, ph7_context_new_array(pCtx));` |
|       7 | 6071 | `		return PH7_OK;` |
|       - | 6072 | `	}` |
|       - | 6073 |  |
|       - | 6074 | `	/* Create a new array */` |
|     221 | 6075 | `	pArray = ph7_context_new_array(pCtx);` |
|     221 | 6076 | `	if( pArray == 0 ){` |
|     ! 0 | 6077 | `		return PH7_ContextMemoryError(pCtx);` |
|       - | 6078 | `	}` |
|       - | 6079 |  |
|       - | 6080 | `	/* Insert the first entry alone because it has its own key */` |
|     221 | 6081 | `	if( ph7_array_add_intkey_elem(pArray, ph7_value_to_int(apArg[0]), apArg[2]) != SXRET_OK ){` |
|     ! 0 | 6082 | `		return PH7_ContextMemoryError(pCtx);` |
|       - | 6083 | `	}` |
|       - | 6084 | `	/* Repeat insertion of the desired value */` |
| 2117583 | 6085 | `	for( i = 1 ; i < nEntry ; i++ ){` |
| 2117363 | 6086 | `		if( ph7_array_add_elem(pArray, 0/*Automatic index assign */, apArg[2]) != SXRET_OK ){` |
|       - | 6087 | `			/* Allocation failure: surface a fatal instead of a partial array */` |
|     ! 0 | 6088 | `			return PH7_ContextMemoryError(pCtx);` |
|       - | 6089 | `		}` |
| 1058682 | 6090 | `	}` |
|       - | 6091 | `	/* Return the filled array */` |
|     221 | 6092 | `	ph7_result_value(pCtx, pArray);` |
|     221 | 6093 | `	return PH7_OK;` |
|     124 | 6094 | `}` |
|       - | 6095 | `/*` |
|       - | 6096 | ` * array array_fill_keys(array $input,mixed $value)` |
|       - | 6097 | ` *  Fill an array with values, specifying keys.` |
|       - | 6098 | ` * Parameters` |
|       - | 6099 | ` *  $input` |
|       - | 6100 | ` *   Array of values that will be used as key.` |
|       - | 6101 | ` *  $value` |
|       - | 6102 | ` *    Value to use for filling.` |
|       - | 6103 | ` * Return` |
|       - | 6104 | ` *  The filled array.` |
|       - | 6105 | ` * Throws` |
|       - | 6106 | ` *  ValueError if $input is not an array.` |
|       - | 6107 | ` */` |
|      26 | 6108 | `static int ph7_hashmap_fill_keys(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       5 | 6109 | `{` |
|       - | 6110 | `	ph7_hashmap_node *pEntry;` |
|       - | 6111 | `	ph7_hashmap *pSrc;` |
|       - | 6112 | `	ph7_value *pArray;` |
|       - | 6113 | `	sxu32 n;` |
|       - | 6114 | `	/* PHP enforces exactly 2 arguments. */` |
|      31 | 6115 | `	if( nArg != 2 ){` |
|      12 | 6116 | `		return PH7_VmThrowException(pCtx,` |
|       - | 6117 | `			"ArgumentCountError",` |
|       - | 6118 | `			"array_fill_keys() expects exactly 2 arguments, %d given",` |
|       3 | 6119 | `			nArg` |
|       - | 6120 | `			);` |
|       - | 6121 | `	}` |
|       - | 6122 | `	/* Make sure we are dealing with a valid hashmap */` |
|      23 | 6123 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|       8 | 6124 | `		return PH7_VmThrowException(pCtx,` |
|       - | 6125 | `			"TypeError",` |
|       - | 6126 | `			"array_fill_keys(): Argument #1 ($keys) must be of type array, %s given",` |
|       2 | 6127 | `			ph7_type_name(apArg[0])` |
|       - | 6128 | `			);` |
|       - | 6129 | `	}` |
|       - | 6130 | `	/* Point to the internal representation of the input hashmap */` |
|      17 | 6131 | `	pSrc = (ph7_hashmap *)apArg[0]->x.pOther;` |
|       - | 6132 | `	/* Create a new array */` |
|      17 | 6133 | `	pArray = ph7_context_new_array(pCtx);` |
|      17 | 6134 | `	if( pArray == 0 ){` |
|     ! 0 | 6135 | `		ph7_result_null(pCtx);` |
|     ! 0 | 6136 | `		return PH7_OK;` |
|       - | 6137 | `	}` |
|       - | 6138 | `	/* Perform the requested operation */` |
|      17 | 6139 | `	pEntry = pSrc->pFirst;` |
|      45 | 6140 | `	for( n = 0 ; n < pSrc->nEntry ; n++ ){` |
|      29 | 6141 | `		ph7_array_add_elem(pArray,HashmapExtractNodeValue(pEntry),apArg[1]);` |
|       - | 6142 | `		/* Point to the next entry */` |
|      29 | 6143 | `		pEntry = pEntry->pPrev; /* Reverse link */` |
|      15 | 6144 | `	}` |
|       - | 6145 | `	/* Return the filled array */` |
|      17 | 6146 | `	ph7_result_value(pCtx,pArray);` |
|      17 | 6147 | `	return PH7_OK;` |
|      18 | 6148 | `}` |
|       - | 6149 | `/*` |
|       - | 6150 | ` * array array_combine(array $keys,array $values)` |
|       - | 6151 | ` *  Creates an array by using one array for keys and another for its values.` |
|       - | 6152 | ` * Parameters` |
|       - | 6153 | ` *  $keys` |
|       - | 6154 | ` *    Array of keys to be used.` |
|       - | 6155 | ` * $values` |
|       - | 6156 | ` *   Array of values to be used.` |
|       - | 6157 | ` * Return` |
|       - | 6158 | ` *  Returns the combined array. Otherwise FALSE if the number of elements` |
|       - | 6159 | ` *  for each array isn't equal or if one of the given arguments is` |
|       - | 6160 | ` *  not an array.` |
|       - | 6161 | ` */` |
|      18 | 6162 | `static int ph7_hashmap_combine(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       5 | 6163 | `{` |
|       - | 6164 | `	ph7_hashmap_node *pKe,*pVe;` |
|       - | 6165 | `	ph7_hashmap *pKey,*pValue;` |
|       - | 6166 | `	ph7_value *pArray;` |
|       - | 6167 | `	sxu32 n;` |
|       - | 6168 | `	/* PHP enforces argument count and type checks. */` |
|      23 | 6169 | `	if( nArg != 2 ){` |
|       - | 6170 | `		/* wrong number of arguments -> ArgumentCountError */` |
|       4 | 6171 | `		return PH7_VmThrowException(pCtx,` |
|       - | 6172 | `			"ArgumentCountError",` |
|       - | 6173 | `			"array_combine() expects exactly 2 arguments, %d given",` |
|       1 | 6174 | `			nArg` |
|       - | 6175 | `			);` |
|       - | 6176 | `	}` |
|       - | 6177 | `	/* Validate argument types individually so we can report the correct` |
|       - | 6178 | `	 * argument index in the error message. */` |
|      20 | 6179 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|       4 | 6180 | `		return PH7_VmThrowException(pCtx,` |
|       - | 6181 | `			"TypeError",` |
|       - | 6182 | `			"array_combine(): Argument #1 ($keys) must be of type array, %s given",` |
|       1 | 6183 | `			ph7_type_name(apArg[0])` |
|       - | 6184 | `			);` |
|       - | 6185 | `	}` |
|      17 | 6186 | `	if( !ph7_value_is_array(apArg[1]) ){` |
|       4 | 6187 | `		return PH7_VmThrowException(pCtx,` |
|       - | 6188 | `			"TypeError",` |
|       - | 6189 | `			"array_combine(): Argument #2 ($values) must be of type array, %s given",` |
|       2 | 6190 | `			ph7_type_name(apArg[1])` |
|       - | 6191 | `			);` |
|       - | 6192 | `	}` |
|       - | 6193 | `	/* Point to the internal representation of the input hashmaps */` |
|      14 | 6194 | `	pKey   = (ph7_hashmap *)apArg[0]->x.pOther;` |
|      14 | 6195 | `	pValue = (ph7_hashmap *)apArg[1]->x.pOther;` |
|      14 | 6196 | `	if( pKey->nEntry != pValue->nEntry ){` |
|       - | 6197 | `		/* Length mismatch -> ValueError */` |
|       3 | 6198 | `		return PH7_VmThrowException(pCtx,` |
|       - | 6199 | `			"ValueError",` |
|       - | 6200 | `			"array_combine(): Argument #1 ($keys) and argument #2 ($values) must have the same number of elements"` |
|       - | 6201 | `			);` |
|       - | 6202 | `	}` |
|       - | 6203 | `	/* Create a new array */` |
|      11 | 6204 | `	pArray = ph7_context_new_array(pCtx);` |
|      11 | 6205 | `	if( pArray == 0 ){` |
|     ! 0 | 6206 | `		ph7_result_bool(pCtx,0);` |
|     ! 0 | 6207 | `		return PH7_OK;` |
|       - | 6208 | `	}` |
|       - | 6209 | `	/* Perform the requested operation */` |
|      11 | 6210 | `	pKe = pKey->pFirst;` |
|      11 | 6211 | `	pVe = pValue->pFirst;` |
|      33 | 6212 | `	for( n = 0 ; n < pKey->nEntry ; n++ ){` |
|      23 | 6213 | `		ph7_value *pKeyVal = HashmapExtractNodeValue(pKe);` |
|      23 | 6214 | `		ph7_value *pValVal = HashmapExtractNodeValue(pVe);` |
|       - | 6215 | `		/* PHP treats floats used as keys in array_combine differently than` |
|       - | 6216 | `		 * ordinary offset access: the float is stringified rather than` |
|       - | 6217 | `		 * truncated.  To emulate this behavior we create a temporary copy of` |
|       - | 6218 | `		 * the value when it is a float and convert the copy to string.  The` |
|       - | 6219 | `		 * original array must not be mutated. */` |
|      23 | 6220 | `		ph7_value *pKeyCopy = pKeyVal;` |
|      23 | 6221 | `		if( ph7_value_is_float(pKeyVal) ){` |
|       5 | 6222 | `			ph7_value *pTmpKey = ph7_context_new_scalar(pCtx);` |
|       5 | 6223 | `			if( pTmpKey ){` |
|       5 | 6224 | `				PH7_MemObjStore(pKeyVal,pTmpKey);` |
|       - | 6225 | `				/* Convert copy to string so it becomes "1.5" or "2" etc. */` |
|       5 | 6226 | `				PH7_MemObjToString(pTmpKey);` |
|       5 | 6227 | `				pKeyCopy = pTmpKey;` |
|       2 | 6228 | `			}` |
|       2 | 6229 | `		}` |
|      23 | 6230 | `		ph7_array_add_elem(pArray,pKeyCopy,pValVal);` |
|       - | 6231 | `		/* Point to the next entry */` |
|      23 | 6232 | `		pKe = pKe->pPrev; /* Reverse link */` |
|      23 | 6233 | `		pVe = pVe->pPrev;` |
|      12 | 6234 | `	}` |
|       - | 6235 | `	/* Return the filled array */` |
|      11 | 6236 | `	ph7_result_value(pCtx,pArray);` |
|      11 | 6237 | `	return PH7_OK;` |
|      14 | 6238 | `}` |
|       - | 6239 | `/*` |
|       - | 6240 | ` * array array_reverse(array $array [,bool $preserve_keys = false ])` |
|       - | 6241 | ` *  Return an array with elements in reverse order.` |
|       - | 6242 | ` * Parameters` |
|       - | 6243 | ` *  $array` |
|       - | 6244 | ` *   The input array.` |
|       - | 6245 | ` *  $preserve_keys (optional)` |
|       - | 6246 | ` *   If set to TRUE keys are preserved.` |
|       - | 6247 | ` * Return` |
|       - | 6248 | ` *  The reversed array.` |
|       - | 6249 | ` */` |
|      20 | 6250 | `static int ph7_hashmap_reverse(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       3 | 6251 | `{` |
|       - | 6252 | `	ph7_hashmap_node *pEntry;` |
|       - | 6253 | `	ph7_hashmap *pSrc;` |
|       - | 6254 | `	ph7_value *pArray;` |
|       - | 6255 | `	int bPreserve;` |
|       - | 6256 | `	sxu32 n;` |
|      23 | 6257 | `	if( nArg < 1 ){` |
|       4 | 6258 | `		return PH7_VmThrowException(pCtx,` |
|       - | 6259 | `			"ArgumentCountError",` |
|       - | 6260 | `			"array_reverse() expects at least 1 argument, %d given",` |
|       1 | 6261 | `			nArg` |
|       - | 6262 | `			);` |
|       - | 6263 | `	}` |
|       - | 6264 | `	/* Make sure we are dealing with a valid hashmap */` |
|      20 | 6265 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|       4 | 6266 | `		return PH7_VmThrowException(pCtx,` |
|       - | 6267 | `			"TypeError",` |
|       - | 6268 | `			"array_reverse(): Argument #1 ($array) must be of type array, %s given",` |
|       1 | 6269 | `			ph7_type_name(apArg[0])` |
|       - | 6270 | `			);` |
|       - | 6271 | `	}` |
|      17 | 6272 | `	bPreserve = FALSE;` |
|      17 | 6273 | `	if( nArg > 1 ){` |
|       7 | 6274 | `		bPreserve = ph7_value_to_bool(apArg[1]);` |
|       3 | 6275 | `	}` |
|       - | 6276 | `	/* Point to the internal representation of the input hashmap */` |
|      17 | 6277 | `	pSrc = (ph7_hashmap *)apArg[0]->x.pOther;` |
|       - | 6278 | `	/* Create a new array */` |
|      17 | 6279 | `	pArray = ph7_context_new_array(pCtx);` |
|      17 | 6280 | `	if( pArray == 0 ){` |
|     ! 0 | 6281 | `		ph7_result_null(pCtx);` |
|     ! 0 | 6282 | `		return PH7_OK;` |
|       - | 6283 | `	}` |
|       - | 6284 | `	/* Perform the requested operation */` |
|      17 | 6285 | `	pEntry = pSrc->pLast;` |
|      55 | 6286 | `	for( n = 0 ; n < pSrc->nEntry ; n++ ){` |
|       - | 6287 | `		/* String keys are always preserved; preserve_keys only affects int keys */` |
|      39 | 6288 | `		int bKeep = (pEntry->iType == HASHMAP_INT_NODE) ? bPreserve : TRUE;` |
|      39 | 6289 | `		HashmapInsertNode((ph7_hashmap *)pArray->x.pOther,pEntry,bKeep);` |
|       - | 6290 | `		/* Point to the previous entry */` |
|      39 | 6291 | `		pEntry = pEntry->pNext; /* Reverse link */` |
|      20 | 6292 | `	}` |
|      17 | 6293 | `	ph7_result_value(pCtx,pArray);` |
|      17 | 6294 | `	return PH7_OK;` |
|      13 | 6295 | `}` |
|       - | 6296 | `/*` |
|       - | 6297 | ` * array array_unique(array $array, int $flags = SORT_STRING)` |
|       - | 6298 | ` *  Removes duplicate values from an array.` |
|       - | 6299 | ` * Parameters` |
|       - | 6300 | ` *  $array` |
|       - | 6301 | ` *   The input array.` |
|       - | 6302 | ` *  $flags` |
|       - | 6303 | ` *   The optional second parameter may be used to modify the comparison` |
|       - | 6304 | ` *   behavior using these values:` |
|       - | 6305 | ` *     SORT_REGULAR - compare items normally (don't change types)` |
|       - | 6306 | ` *     SORT_NUMERIC - compare items numerically` |
|       - | 6307 | ` *     SORT_STRING  - compare items as strings` |
|       - | 6308 | ` * Return` |
|       - | 6309 | ` *  The filtered array.` |
|       - | 6310 | ` */` |
|      24 | 6311 | `static int ph7_hashmap_unique(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       4 | 6312 | `{` |
|       - | 6313 | `	ph7_hashmap_node *pEntry;` |
|       - | 6314 | `	ph7_value *pNeedle;` |
|       - | 6315 | `	ph7_hashmap *pSrc;` |
|       - | 6316 | `	ph7_value *pArray;` |
|       - | 6317 | `	int bStrict;` |
|       - | 6318 | `	sxi32 rc;` |
|       - | 6319 | `	sxu32 n;` |
|      28 | 6320 | `	if( nArg < 1 ){` |
|       - | 6321 | `		/* Missing arguments, throw ArgumentCountError */` |
|       3 | 6322 | `		return PH7_VmThrowException(pCtx,` |
|       - | 6323 | `			"ArgumentCountError",` |
|       - | 6324 | `			"array_unique() expects at least 1 argument, 0 given"` |
|       - | 6325 | `			);` |
|       - | 6326 | `	}` |
|      25 | 6327 | `	if( nArg > 2 ){` |
|       - | 6328 | `		/* Too many arguments, throw ArgumentCountError */` |
|       4 | 6329 | `		return PH7_VmThrowException(pCtx,` |
|       - | 6330 | `			"ArgumentCountError",` |
|       - | 6331 | `			"array_unique() expects at most 2 arguments, %d given",` |
|       1 | 6332 | `			nArg` |
|       - | 6333 | `			);` |
|       - | 6334 | `	}` |
|       - | 6335 | `	/* Make sure we are dealing with a valid hashmap */` |
|      22 | 6336 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|       - | 6337 | `		/* Type mismatch, throw TypeError */` |
|       4 | 6338 | `		return PH7_VmThrowException(pCtx,` |
|       - | 6339 | `			"TypeError",` |
|       - | 6340 | `			"array_unique(): Argument #1 ($array) must be of type array, %s given",` |
|       1 | 6341 | `			ph7_type_name(apArg[0])` |
|       - | 6342 | `			);` |
|       - | 6343 | `	}` |
|      19 | 6344 | `	bStrict = FALSE;` |
|       - | 6345 | `	/* Point to the internal representation of the input hashmap */` |
|      19 | 6346 | `	pSrc = (ph7_hashmap *)apArg[0]->x.pOther;` |
|       - | 6347 | `	/* Create a new array */` |
|      19 | 6348 | `	pArray = ph7_context_new_array(pCtx);` |
|      19 | 6349 | `	if( pArray == 0 ){` |
|     ! 0 | 6350 | `		ph7_result_null(pCtx);` |
|     ! 0 | 6351 | `		return PH7_OK;` |
|       - | 6352 | `	}` |
|       - | 6353 | `	/* Perform the requested operation */` |
|      19 | 6354 | `	pEntry = pSrc->pFirst;` |
|      83 | 6355 | `	for( n = 0 ; n < pSrc->nEntry ; n++ ){` |
|      65 | 6356 | `		pNeedle = HashmapExtractNodeValue(pEntry);` |
|      65 | 6357 | `		rc = SXERR_NOTFOUND;` |
|      65 | 6358 | `		if( pNeedle ){` |
|      65 | 6359 | `			rc = HashmapFindValue((ph7_hashmap *)pArray->x.pOther,pNeedle,0,bStrict);` |
|      32 | 6360 | `		}` |
|      65 | 6361 | `		if( rc != SXRET_OK ){` |
|       - | 6362 | `			/* Perform the insertion */` |
|      37 | 6363 | `			HashmapInsertNode((ph7_hashmap *)pArray->x.pOther,pEntry,TRUE);` |
|      18 | 6364 | `		}` |
|       - | 6365 | `		/* Point to the next entry */` |
|      65 | 6366 | `		pEntry = pEntry->pPrev; /* Reverse link */` |
|      33 | 6367 | `	}` |
|       - | 6368 | `	/* Return the freshly created array */` |
|      19 | 6369 | `	ph7_result_value(pCtx,pArray);` |
|      19 | 6370 | `	return PH7_OK;` |
|      16 | 6371 | `}` |
|       - | 6372 | `/*` |
|       - | 6373 | ` * array array_flip(array $input)` |
|       - | 6374 | ` *  Exchanges all keys with their associated values in an array.` |
|       - | 6375 | ` * Parameter` |
|       - | 6376 | ` *  $input` |
|       - | 6377 | ` *   Input array.` |
|       - | 6378 | ` * Return` |
|       - | 6379 | ` *   The flipped array on success or NULL on failure.` |
|       - | 6380 | ` */` |
|      34 | 6381 | `static int ph7_hashmap_flip(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       5 | 6382 | `{` |
|       - | 6383 | `	ph7_hashmap_node *pEntry;` |
|       - | 6384 | `	ph7_hashmap *pSrc;` |
|       - | 6385 | `	ph7_value *pArray;` |
|       - | 6386 | `	ph7_value *pKey;` |
|       - | 6387 | `	ph7_value sVal;` |
|       - | 6388 | `	sxu32 n;` |
|       - | 6389 |  |
|       - | 6390 | `	/* PHP requires exactly one argument */` |
|      39 | 6391 | `	if( nArg != 1 ){` |
|       - | 6392 | `		/* Use ArgumentCountError like other array helpers */` |
|       8 | 6393 | `		return PH7_VmThrowException(pCtx,` |
|       - | 6394 | `			"ArgumentCountError",` |
|       - | 6395 | `			"array_flip() expects exactly 1 argument, %d given",` |
|       2 | 6396 | `			nArg` |
|       - | 6397 | `			);` |
|       - | 6398 | `	}` |
|       - | 6399 | `	/* Make sure we are dealing with a valid hashmap */` |
|      33 | 6400 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|       - | 6401 | `		/* Type mismatch -> TypeError */` |
|       8 | 6402 | `		return PH7_VmThrowException(pCtx,` |
|       - | 6403 | `			"TypeError",` |
|       - | 6404 | `			"array_flip(): Argument #1 ($array) must be of type array, %s given",` |
|       2 | 6405 | `			ph7_type_name(apArg[0])` |
|       - | 6406 | `			);` |
|       - | 6407 | `	}` |
|       - | 6408 | `	/* Point to the internal representation of the input hashmap */` |
|      27 | 6409 | `	pSrc = (ph7_hashmap *)apArg[0]->x.pOther;` |
|       - | 6410 | `	/* Create a new array */` |
|      27 | 6411 | `	pArray = ph7_context_new_array(pCtx);` |
|      27 | 6412 | `	if( pArray == 0 ){` |
|     ! 0 | 6413 | `		ph7_result_null(pCtx);` |
|     ! 0 | 6414 | `		return PH7_OK;` |
|       - | 6415 | `	}` |
|       - | 6416 | `	/* Start processing */` |
|      27 | 6417 | `	pEntry = pSrc->pFirst;` |
|   22263 | 6418 | `	for( n = 0 ; n < pSrc->nEntry ; n++ ){` |
|       - | 6419 | `		/* Extract the node value (will become a key in the result) */` |
|   22237 | 6420 | `		pKey = HashmapExtractNodeValue(pEntry);` |
|   22237 | 6421 | `		if( pKey ){` |
|       - | 6422 | `			/* NULL values are not valid keys either, PHP emits a warning */` |
|   22237 | 6423 | `			if( pKey->iFlags & MEMOBJ_NULL ){` |
|       3 | 6424 | `				PH7_VmThrowError(pCtx->pVm,0,PH7_CTX_WARNING,` |
|       - | 6425 | `					"array_flip(): Can only flip string and integer values, entry skipped"` |
|       - | 6426 | `					);` |
|   22236 | 6427 | `			}else if( (pKey->iFlags & MEMOBJ_INT) \|\| (pKey->iFlags & MEMOBJ_STRING) ){` |
|       - | 6428 | `				/* Prepare the value for insertion (original key) */` |
|   22227 | 6429 | `				if( pEntry->iType == HASHMAP_INT_NODE ){` |
|   20001 | 6430 | `					PH7_MemObjInitFromInt(pSrc->pVm,&sVal,pEntry->xKey.iKey);` |
|   10001 | 6431 | `				}else{` |
|       - | 6432 | `					SyString sStr;` |
|    2227 | 6433 | `					SyStringInitFromBuf(&sStr,SyBlobData(&pEntry->xKey.sKey),SyBlobLength(&pEntry->xKey.sKey));` |
|    2227 | 6434 | `					PH7_MemObjInitFromString(pSrc->pVm,&sVal,&sStr);` |
|       - | 6435 | `				}` |
|       - | 6436 | `				/* Perform the insertion */` |
|   22227 | 6437 | `				ph7_array_add_elem(pArray,pKey,&sVal);` |
|       - | 6438 | `				/* Safely release the value because each inserted entry` |
|       - | 6439 | `				 * has its own private copy of the value.` |
|       - | 6440 | `				 */` |
|   22227 | 6441 | `				PH7_MemObjRelease(&sVal);` |
|   11114 | 6442 | `			}else{` |
|       - | 6443 | `				/* Unsupported value type -> emit warning and skip the entry */` |
|       9 | 6444 | `				PH7_VmThrowError(pCtx->pVm,0,PH7_CTX_WARNING,` |
|       - | 6445 | `					"array_flip(): Can only flip string and integer values, entry skipped"` |
|       - | 6446 | `					);` |
|       - | 6447 | `			}` |
|   11118 | 6448 | `		}` |
|       - | 6449 | `		/* Point to the next entry */` |
|   22237 | 6450 | `		pEntry = pEntry->pPrev; /* Reverse link */` |
|   11119 | 6451 | `	}` |
|       - | 6452 | `	/* Return the freshly created array */` |
|      27 | 6453 | `	ph7_result_value(pCtx,pArray);` |
|      27 | 6454 | `	return PH7_OK;` |
|      22 | 6455 | `}` |
|       - | 6456 | `/*` |
|       - | 6457 | ` * number array_sum(array $array )` |
|       - | 6458 | ` *  Calculate the sum of values in an array.` |
|       - | 6459 | ` * Parameters` |
|       - | 6460 | ` *  $array: The input array.` |
|       - | 6461 | ` * Return` |
|       - | 6462 | ` *  Returns the sum of values as an integer or float.` |
|       - | 6463 | ` */` |
|      24 | 6464 | `static void DoubleSum(ph7_context *pCtx,ph7_hashmap *pMap)` |
|       1 | 6465 | `{` |
|       - | 6466 | `	ph7_hashmap_node *pEntry;` |
|       - | 6467 | `	ph7_value *pObj;` |
|      25 | 6468 | `	double dSum = 0;` |
|       - | 6469 | `	sxu32 n;` |
|      25 | 6470 | `	pEntry = pMap->pFirst;` |
|      91 | 6471 | `	for( n = 0 ; n < pMap->nEntry ; n++ ){` |
|      67 | 6472 | `		pObj = HashmapExtractNodeValue(pEntry);` |
|      67 | 6473 | `		if( pObj ){` |
|      67 | 6474 | `			if( pObj->iFlags & MEMOBJ_REAL ){` |
|      29 | 6475 | `				dSum += pObj->rVal;` |
|      53 | 6476 | `			}else if( pObj->iFlags & (MEMOBJ_INT\|MEMOBJ_BOOL) ){` |
|      21 | 6477 | `				dSum += (double)pObj->x.iVal;` |
|      29 | 6478 | `			}else if( pObj->iFlags & MEMOBJ_STRING ){` |
|      15 | 6479 | `				if( SyBlobLength(&pObj->sBlob) > 0 ){` |
|      13 | 6480 | `					double dv = 0;` |
|      13 | 6481 | `					SyStrToReal((const char *)SyBlobData(&pObj->sBlob),SyBlobLength(&pObj->sBlob),(void *)&dv,0);` |
|      13 | 6482 | `					dSum += dv;` |
|       7 | 6483 | `				}` |
|      12 | 6484 | `			}else if( pObj->iFlags & MEMOBJ_HASHMAP ){` |
|       3 | 6485 | `				PH7_VmThrowError(pCtx->pVm,0,PH7_CTX_WARNING,` |
|       - | 6486 | `					"array_sum(): Addition is not supported on type array");` |
|       4 | 6487 | `			}else if( pObj->iFlags & MEMOBJ_OBJ ){` |
|     ! 0 | 6488 | `				PH7_VmThrowError(pCtx->pVm,0,PH7_CTX_WARNING,` |
|       - | 6489 | `					"array_sum(): Addition is not supported on type object");` |
|       3 | 6490 | `			}else if( pObj->iFlags & MEMOBJ_RES ){` |
|     ! 0 | 6491 | `				PH7_VmThrowError(pCtx->pVm,0,PH7_CTX_WARNING,` |
|       - | 6492 | `					"array_sum(): Addition is not supported on type resource");` |
|     ! 0 | 6493 | `			}` |
|       - | 6494 | `			/* NULL is silently treated as 0 (matches PHP) */` |
|      33 | 6495 | `		}` |
|       - | 6496 | `		/* Point to the next entry */` |
|      67 | 6497 | `		pEntry = pEntry->pPrev; /* Reverse link */` |
|      34 | 6498 | `	}` |
|       - | 6499 | `	/* Return sum */` |
|      25 | 6500 | `	ph7_result_double(pCtx,dSum);` |
|      25 | 6501 | `}` |
|      32 | 6502 | `static void Int64Sum(ph7_context *pCtx,ph7_hashmap *pMap)` |
|       2 | 6503 | `{` |
|       - | 6504 | `	ph7_hashmap_node *pEntry;` |
|       - | 6505 | `	ph7_value *pObj;` |
|      34 | 6506 | `	sxi64 nSum = 0;` |
|       - | 6507 | `	sxu32 n;` |
|      34 | 6508 | `	pEntry = pMap->pFirst;` |
|     136 | 6509 | `	for( n = 0 ; n < pMap->nEntry ; n++ ){` |
|     104 | 6510 | `		pObj = HashmapExtractNodeValue(pEntry);` |
|     104 | 6511 | `		if( pObj ){` |
|     104 | 6512 | `			if( pObj->iFlags & (MEMOBJ_INT\|MEMOBJ_BOOL) ){` |
|      94 | 6513 | `				nSum += pObj->x.iVal;` |
|      57 | 6514 | `			}else if( pObj->iFlags & MEMOBJ_STRING ){` |
|       7 | 6515 | `				if( SyBlobLength(&pObj->sBlob) > 0 ){` |
|       5 | 6516 | `					sxi64 nv = 0;` |
|       5 | 6517 | `					SyStrToInt64((const char *)SyBlobData(&pObj->sBlob),SyBlobLength(&pObj->sBlob),(void *)&nv,0);` |
|       5 | 6518 | `					nSum += nv;` |
|       3 | 6519 | `				}` |
|       8 | 6520 | `			}else if( pObj->iFlags & MEMOBJ_HASHMAP ){` |
|       3 | 6521 | `				PH7_VmThrowError(pCtx->pVm,0,PH7_CTX_WARNING,` |
|       - | 6522 | `					"array_sum(): Addition is not supported on type array");` |
|       4 | 6523 | `			}else if( pObj->iFlags & MEMOBJ_OBJ ){` |
|     ! 0 | 6524 | `				PH7_VmThrowError(pCtx->pVm,0,PH7_CTX_WARNING,` |
|       - | 6525 | `					"array_sum(): Addition is not supported on type object");` |
|       3 | 6526 | `			}else if( pObj->iFlags & MEMOBJ_RES ){` |
|     ! 0 | 6527 | `				PH7_VmThrowError(pCtx->pVm,0,PH7_CTX_WARNING,` |
|       - | 6528 | `					"array_sum(): Addition is not supported on type resource");` |
|     ! 0 | 6529 | `			}` |
|       - | 6530 | `			/* NULL is silently treated as 0 (matches PHP) */` |
|      51 | 6531 | `		}` |
|       - | 6532 | `		/* Point to the next entry */` |
|     104 | 6533 | `		pEntry = pEntry->pPrev; /* Reverse link */` |
|      53 | 6534 | `	}` |
|       - | 6535 | `	/* Return sum */` |
|      34 | 6536 | `	ph7_result_int64(pCtx,nSum);` |
|      34 | 6537 | `}` |
|       - | 6538 | `/* number array_sum(array $array )` |
|       - | 6539 | ` * (See block-coment above)` |
|       - | 6540 | ` */` |
|      70 | 6541 | `static int ph7_hashmap_sum(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       5 | 6542 | `{` |
|       - | 6543 | `	ph7_hashmap_node *pEntry;` |
|       - | 6544 | `	ph7_hashmap *pMap;` |
|       - | 6545 | `	ph7_value *pObj;` |
|      75 | 6546 | `	int useDouble = 0;` |
|       - | 6547 | `	sxu32 n;` |
|       - | 6548 | `	/* PHP requires exactly one argument */` |
|      75 | 6549 | `	if( nArg != 1 ){` |
|       8 | 6550 | `		return PH7_VmThrowException(pCtx,` |
|       - | 6551 | `			"ArgumentCountError",` |
|       - | 6552 | `			"array_sum() expects exactly 1 argument, %d given",` |
|       2 | 6553 | `			nArg` |
|       - | 6554 | `			);` |
|       - | 6555 | `	}` |
|       - | 6556 | `	/* Make sure we are dealing with a valid hashmap */` |
|      70 | 6557 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|       - | 6558 | `		/* Type mismatch -> TypeError */` |
|       8 | 6559 | `		return PH7_VmThrowException(pCtx,` |
|       - | 6560 | `			"TypeError",` |
|       - | 6561 | `			"array_sum(): Argument #1 ($array) must be of type array, %s given",` |
|       2 | 6562 | `			ph7_type_name(apArg[0])` |
|       - | 6563 | `			);` |
|       - | 6564 | `	}` |
|      64 | 6565 | `	pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|      64 | 6566 | `	if( pMap->nEntry < 1 ){` |
|       - | 6567 | `		/* Nothing to compute,return 0 */` |
|       7 | 6568 | `		ph7_result_int(pCtx,0);` |
|       7 | 6569 | `		return PH7_OK;` |
|       - | 6570 | `	}` |
|       - | 6571 | `	/* Scan all elements: if any value is a float, use floating-point` |
|       - | 6572 | `	 * arithmetic for the entire sum (matches PHP behaviour).` |
|       - | 6573 | `	 */` |
|      58 | 6574 | `	pEntry = pMap->pFirst;` |
|     168 | 6575 | `	for( n = 0 ; n < pMap->nEntry ; n++ ){` |
|     136 | 6576 | `		pObj = HashmapExtractNodeValue(pEntry);` |
|     136 | 6577 | `		if( pObj ){` |
|     136 | 6578 | `			if( pObj->iFlags & MEMOBJ_REAL ){` |
|      19 | 6579 | `				useDouble = 1;` |
|      19 | 6580 | `				break;` |
|       - | 6581 | `			}` |
|     118 | 6582 | `			if( pObj->iFlags & MEMOBJ_STRING ){` |
|      13 | 6583 | `				const char *zStr = (const char *)SyBlobData(&pObj->sBlob);` |
|      13 | 6584 | `				sxu32 nLen = SyBlobLength(&pObj->sBlob);` |
|       - | 6585 | `				sxu32 i;` |
|      23 | 6586 | `				for( i = 0 ; i < nLen ; i++ ){` |
|      17 | 6587 | `					if( zStr[i] == '.' \|\| zStr[i] == 'e' \|\| zStr[i] == 'E' ){` |
|       7 | 6588 | `						useDouble = 1;` |
|       7 | 6589 | `						break;` |
|       - | 6590 | `					}` |
|       6 | 6591 | `				}` |
|      13 | 6592 | `				if( useDouble ){` |
|       7 | 6593 | `					break;` |
|       - | 6594 | `				}` |
|       3 | 6595 | `			}` |
|      55 | 6596 | `		}` |
|     112 | 6597 | `		pEntry = pEntry->pPrev;` |
|      57 | 6598 | `	}` |
|      58 | 6599 | `	if( useDouble ){` |
|      25 | 6600 | `		DoubleSum(pCtx,pMap);` |
|      13 | 6601 | `	}else{` |
|      34 | 6602 | `		Int64Sum(pCtx,pMap);` |
|       - | 6603 | `	}` |
|      58 | 6604 | `	return PH7_OK;` |
|      40 | 6605 | `}` |
|       - | 6606 | `/*` |
|       - | 6607 | ` * number array_product(array $array )` |
|       - | 6608 | ` *  Calculate the product of values in an array.` |
|       - | 6609 | ` * Parameters` |
|       - | 6610 | ` *  $array: The input array.` |
|       - | 6611 | ` * Return` |
|       - | 6612 | ` *  Returns the product of values as an integer or float.` |
|       - | 6613 | ` */` |
|     ! 0 | 6614 | `static void DoubleProd(ph7_context *pCtx,ph7_hashmap *pMap)` |
|     ! 0 | 6615 | `{` |
|       - | 6616 | `	ph7_hashmap_node *pEntry;` |
|       - | 6617 | `	ph7_value *pObj;` |
|       - | 6618 | `	double dProd;` |
|       - | 6619 | `	sxu32 n;` |
|     ! 0 | 6620 | `	pEntry = pMap->pFirst;` |
|     ! 0 | 6621 | `	dProd = 1;` |
|     ! 0 | 6622 | `	for( n = 0 ; n < pMap->nEntry ; n++ ){` |
|     ! 0 | 6623 | `		pObj = HashmapExtractNodeValue(pEntry);` |
|     ! 0 | 6624 | `		if( pObj && (pObj->iFlags & (MEMOBJ_NULL\|MEMOBJ_HASHMAP\|MEMOBJ_OBJ\|MEMOBJ_RES)) == 0){` |
|     ! 0 | 6625 | `			if( pObj->iFlags & MEMOBJ_REAL ){` |
|     ! 0 | 6626 | `				dProd *= pObj->rVal;` |
|     ! 0 | 6627 | `			}else if( pObj->iFlags & (MEMOBJ_INT\|MEMOBJ_BOOL) ){` |
|     ! 0 | 6628 | `				dProd *= (double)pObj->x.iVal;` |
|     ! 0 | 6629 | `			}else if( pObj->iFlags & MEMOBJ_STRING ){` |
|     ! 0 | 6630 | `				if( SyBlobLength(&pObj->sBlob) > 0 ){` |
|     ! 0 | 6631 | `					double dv = 0;` |
|     ! 0 | 6632 | `					SyStrToReal((const char *)SyBlobData(&pObj->sBlob),SyBlobLength(&pObj->sBlob),(void *)&dv,0);` |
|     ! 0 | 6633 | `					dProd *= dv;` |
|     ! 0 | 6634 | `				}` |
|     ! 0 | 6635 | `			}` |
|     ! 0 | 6636 | `		}` |
|       - | 6637 | `		/* Point to the next entry */` |
|     ! 0 | 6638 | `		pEntry = pEntry->pPrev; /* Reverse link */` |
|     ! 0 | 6639 | `	}` |
|       - | 6640 | `	/* Return product */` |
|     ! 0 | 6641 | `	ph7_result_double(pCtx,dProd);` |
|     ! 0 | 6642 | `}` |
|     ! 0 | 6643 | `static void Int64Prod(ph7_context *pCtx,ph7_hashmap *pMap)` |
|     ! 0 | 6644 | `{` |
|       - | 6645 | `	ph7_hashmap_node *pEntry;` |
|       - | 6646 | `	ph7_value *pObj;` |
|       - | 6647 | `	sxi64 nProd;` |
|       - | 6648 | `	sxu32 n;` |
|     ! 0 | 6649 | `	pEntry = pMap->pFirst;` |
|     ! 0 | 6650 | `	nProd = 1;` |
|     ! 0 | 6651 | `	for( n = 0 ; n < pMap->nEntry ; n++ ){` |
|     ! 0 | 6652 | `		pObj = HashmapExtractNodeValue(pEntry);` |
|     ! 0 | 6653 | `		if( pObj && (pObj->iFlags & (MEMOBJ_NULL\|MEMOBJ_HASHMAP\|MEMOBJ_OBJ\|MEMOBJ_RES)) == 0){` |
|     ! 0 | 6654 | `			if( pObj->iFlags & MEMOBJ_REAL ){` |
|     ! 0 | 6655 | `				nProd *= (sxi64)pObj->rVal;` |
|     ! 0 | 6656 | `			}else if( pObj->iFlags & (MEMOBJ_INT\|MEMOBJ_BOOL) ){` |
|     ! 0 | 6657 | `				nProd *= pObj->x.iVal;` |
|     ! 0 | 6658 | `			}else if( pObj->iFlags & MEMOBJ_STRING ){` |
|     ! 0 | 6659 | `				if( SyBlobLength(&pObj->sBlob) > 0 ){` |
|     ! 0 | 6660 | `					sxi64 nv = 0;` |
|     ! 0 | 6661 | `					SyStrToInt64((const char *)SyBlobData(&pObj->sBlob),SyBlobLength(&pObj->sBlob),(void *)&nv,0);` |
|     ! 0 | 6662 | `					nProd *= nv;` |
|     ! 0 | 6663 | `				}` |
|     ! 0 | 6664 | `			}` |
|     ! 0 | 6665 | `		}` |
|       - | 6666 | `		/* Point to the next entry */` |
|     ! 0 | 6667 | `		pEntry = pEntry->pPrev; /* Reverse link */` |
|     ! 0 | 6668 | `	}` |
|       - | 6669 | `	/* Return product */` |
|     ! 0 | 6670 | `	ph7_result_int64(pCtx,nProd);` |
|     ! 0 | 6671 | `}` |
|       - | 6672 | `/* number array_product(array $array )` |
|       - | 6673 | ` * (See block-block comment above)` |
|       - | 6674 | ` */` |
|     ! 0 | 6675 | `static int ph7_hashmap_product(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     ! 0 | 6676 | `{` |
|       - | 6677 | `	ph7_hashmap *pMap;` |
|       - | 6678 | `	ph7_value *pObj;` |
|     ! 0 | 6679 | `	if( nArg < 1 ){` |
|       - | 6680 | `		/* Missing arguments,return 0 */` |
|     ! 0 | 6681 | `		ph7_result_int(pCtx,0);` |
|     ! 0 | 6682 | `		return PH7_OK;` |
|       - | 6683 | `	}` |
|       - | 6684 | `	/* Make sure we are dealing with a valid hashmap */` |
|     ! 0 | 6685 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|       - | 6686 | `		/* Invalid argument,return 0 */` |
|     ! 0 | 6687 | `		ph7_result_int(pCtx,0);` |
|     ! 0 | 6688 | `		return PH7_OK;` |
|       - | 6689 | `	}` |
|     ! 0 | 6690 | `	pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|     ! 0 | 6691 | `	if( pMap->nEntry < 1 ){` |
|       - | 6692 | `		/* Nothing to compute,return 0 */` |
|     ! 0 | 6693 | `		ph7_result_int(pCtx,0);` |
|     ! 0 | 6694 | `		return PH7_OK;` |
|       - | 6695 | `	}` |
|       - | 6696 | `	/* If the first element is of type float,then perform floating` |
|       - | 6697 | `	 * point computaion.Otherwise switch to int64 computaion.` |
|       - | 6698 | `	 */` |
|     ! 0 | 6699 | `	pObj = HashmapExtractNodeValue(pMap->pFirst);` |
|     ! 0 | 6700 | `	if( pObj == 0 ){` |
|     ! 0 | 6701 | `		ph7_result_int(pCtx,0);` |
|     ! 0 | 6702 | `		return PH7_OK;` |
|       - | 6703 | `	}` |
|     ! 0 | 6704 | `	if( pObj->iFlags & MEMOBJ_REAL ){` |
|     ! 0 | 6705 | `		DoubleProd(pCtx,pMap);` |
|     ! 0 | 6706 | `	}else{` |
|     ! 0 | 6707 | `		Int64Prod(pCtx,pMap);` |
|       - | 6708 | `	}` |
|     ! 0 | 6709 | `	return PH7_OK;` |
|     ! 0 | 6710 | `}` |
|       - | 6711 | `/*` |
|       - | 6712 | ` * value array_rand(array $input[,int $num_req = 1 ])` |
|       - | 6713 | ` *  Pick one or more random entries out of an array.` |
|       - | 6714 | ` * Parameters` |
|       - | 6715 | ` * $input` |
|       - | 6716 | ` *  The input array.` |
|       - | 6717 | ` * $num_req` |
|       - | 6718 | ` *  Specifies how many entries you want to pick.` |
|       - | 6719 | ` * Return` |
|       - | 6720 | ` *  If you are picking only one entry, array_rand() returns the key for a random entry.` |
|       - | 6721 | ` *  Otherwise, it returns an array of keys for the random entries.` |
|       - | 6722 | ` *  NULL is returned on failure.` |
|       - | 6723 | ` */` |
|       6 | 6724 | `static int ph7_hashmap_rand(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       1 | 6725 | `{` |
|       - | 6726 | `	ph7_hashmap_node *pNode;` |
|       - | 6727 | `	ph7_hashmap *pMap;` |
|       7 | 6728 | `	int nItem = 1;` |
|       7 | 6729 | `	if( nArg < 1 ){` |
|       - | 6730 | `		/* Missing argument,return NULL */` |
|     ! 0 | 6731 | `		ph7_result_null(pCtx);` |
|     ! 0 | 6732 | `		return PH7_OK;` |
|       - | 6733 | `	}` |
|       - | 6734 | `	/* Make sure we are dealing with an array */` |
|       7 | 6735 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|     ! 0 | 6736 | `		ph7_result_null(pCtx);` |
|     ! 0 | 6737 | `		return PH7_OK;` |
|       - | 6738 | `	}` |
|       - | 6739 | `	/* Point to the internal representation of the input hashmap */` |
|       7 | 6740 | `	pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|       7 | 6741 | `	if(pMap->nEntry < 1 ){` |
|       - | 6742 | `		/* Empty hashmap,return NULL */` |
|     ! 0 | 6743 | `		ph7_result_null(pCtx);` |
|     ! 0 | 6744 | `		return PH7_OK;` |
|       - | 6745 | `	}` |
|       7 | 6746 | `	if( nArg > 1 ){` |
|       3 | 6747 | `		nItem = ph7_value_to_int(apArg[1]);` |
|       1 | 6748 | `	}` |
|       7 | 6749 | `	if( nItem < 2 ){` |
|       - | 6750 | `		sxu32 nEntry;` |
|       - | 6751 | `		/* Select a random number */` |
|       5 | 6752 | `		nEntry = PH7_VmRandomNum(pMap->pVm) % pMap->nEntry;` |
|       - | 6753 | `		/* Extract the desired entry.` |
|       - | 6754 | `		 * Note that we perform a linear lookup here (later version must change this)` |
|       - | 6755 | `		 */` |
|       5 | 6756 | `		if( nEntry > pMap->nEntry / 2 ){` |
|       2 | 6757 | `			pNode = pMap->pLast;` |
|       2 | 6758 | `			nEntry = pMap->nEntry - nEntry;` |
|       2 | 6759 | `			if( nEntry > 1 ){` |
|     ! 0 | 6760 | `				for(;;){` |
|     ! 0 | 6761 | `					if( nEntry == 0 ){` |
|     ! 0 | 6762 | `						break;` |
|       - | 6763 | `					}` |
|       - | 6764 | `					/* Point to the previous entry */` |
|     ! 0 | 6765 | `					pNode = pNode->pNext; /* Reverse link */` |
|     ! 0 | 6766 | `					nEntry--;` |
|     ! 0 | 6767 | `				}` |
|     ! 0 | 6768 | `			}` |
|       1 | 6769 | `		}else{` |
|       4 | 6770 | `			pNode = pMap->pFirst;` |
|       3 | 6771 | `			for(;;){` |
|       5 | 6772 | `				if( nEntry == 0 ){` |
|       4 | 6773 | `					break;` |
|       - | 6774 | `				}` |
|       - | 6775 | `				/* Point to the next entry */` |
|       2 | 6776 | `				pNode = pNode->pPrev; /* Reverse link */` |
|       2 | 6777 | `				nEntry--;` |
|       1 | 6778 | `			}` |
|       - | 6779 | `		}` |
|       5 | 6780 | `		if( pNode->iType == HASHMAP_INT_NODE ){` |
|       - | 6781 | `			/* Int key */` |
|       3 | 6782 | `			ph7_result_int64(pCtx,pNode->xKey.iKey);` |
|       2 | 6783 | `		}else{` |
|       - | 6784 | `			/* Blob key */` |
|       3 | 6785 | `			ph7_result_string(pCtx,(const char *)SyBlobData(&pNode->xKey.sKey),(int)SyBlobLength(&pNode->xKey.sKey));` |
|       - | 6786 | `		}` |
|       3 | 6787 | `	}else{` |
|       - | 6788 | `		ph7_value sKey,*pArray;` |
|       - | 6789 | `		ph7_hashmap *pDest;` |
|       - | 6790 | `		/* Create a new array */` |
|       3 | 6791 | `		pArray = ph7_context_new_array(pCtx);` |
|       3 | 6792 | `		if( pArray == 0 ){` |
|     ! 0 | 6793 | `			ph7_result_null(pCtx);` |
|     ! 0 | 6794 | `			return PH7_OK;` |
|       - | 6795 | `		}` |
|       - | 6796 | `		/* Point to the internal representation of the hashmap */` |
|       3 | 6797 | `		pDest = (ph7_hashmap *)pArray->x.pOther;` |
|       3 | 6798 | `		PH7_MemObjInit(pDest->pVm,&sKey);` |
|       - | 6799 | `		/* Copy the first n items */` |
|       3 | 6800 | `		pNode = pMap->pFirst;` |
|       3 | 6801 | `		if( nItem > (int)pMap->nEntry ){` |
|     ! 0 | 6802 | `			nItem = (int)pMap->nEntry;` |
|     ! 0 | 6803 | `		}` |
|       7 | 6804 | `		while( nItem > 0){` |
|       5 | 6805 | `			PH7_HashmapExtractNodeKey(pNode,&sKey);` |
|       5 | 6806 | `			PH7_HashmapInsert(pDest,0/* Automatic index assign*/,&sKey);` |
|       5 | 6807 | `			PH7_MemObjRelease(&sKey);` |
|       - | 6808 | `			/* Point to the next entry */` |
|       5 | 6809 | `			pNode = pNode->pPrev; /* Reverse link */` |
|       5 | 6810 | `			nItem--;` |
|       1 | 6811 | `		}` |
|       - | 6812 | `		/* Shuffle the array */` |
|       3 | 6813 | `		HashmapMergeSort(pDest,HashmapCmpCallback7,0);` |
|       - | 6814 | `		/* Rehash node */` |
|       3 | 6815 | `		HashmapSortRehash(pDest);` |
|       - | 6816 | `		/* Return the random array */` |
|       3 | 6817 | `		ph7_result_value(pCtx,pArray);` |
|       - | 6818 | `	}` |
|       7 | 6819 | `	return PH7_OK;` |
|       4 | 6820 | `}` |
|       - | 6821 | `/*` |
|       - | 6822 | ` * array array_chunk (array $input,int $size [,bool $preserve_keys = false ])` |
|       - | 6823 | ` *  Split an array into chunks.` |
|       - | 6824 | ` * Parameters` |
|       - | 6825 | ` * $input` |
|       - | 6826 | ` *   The array to work on` |
|       - | 6827 | ` * $size` |
|       - | 6828 | ` *   The size of each chunk` |
|       - | 6829 | ` * $preserve_keys` |
|       - | 6830 | ` *   When set to TRUE keys will be preserved. Default is FALSE which will reindex` |
|       - | 6831 | ` *   the chunk numerically.` |
|       - | 6832 | ` * Return` |
|       - | 6833 | ` *  Returns a multidimensional numerically indexed array, starting with` |
|       - | 6834 | ` *  zero, with each dimension containing size elements.` |
|       - | 6835 | ` */` |
|      42 | 6836 | `static int ph7_hashmap_chunk(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       5 | 6837 | `{` |
|       - | 6838 | `	ph7_value *pArray,*pChunk;` |
|       - | 6839 | `	ph7_hashmap_node *pEntry;` |
|       - | 6840 | `	ph7_hashmap *pMap;` |
|       - | 6841 | `	int bPreserve;` |
|       - | 6842 | `	sxu32 nChunk;` |
|       - | 6843 | `	sxu32 nSize;` |
|       - | 6844 | `	sxu32 n;` |
|       - | 6845 | `	/* Argument count and types follow PHP semantics. */` |
|      47 | 6846 | `	if( nArg < 2 ){` |
|       - | 6847 | `		/* fewer than required arguments -> ArgumentCountError */` |
|       4 | 6848 | `		return PH7_VmThrowException(pCtx,` |
|       - | 6849 | `			"ArgumentCountError",` |
|       - | 6850 | `			"array_chunk() expects at least 2 arguments, %d given",` |
|       1 | 6851 | `			nArg` |
|       - | 6852 | `			);` |
|       - | 6853 | `	}` |
|      45 | 6854 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|       4 | 6855 | `		return PH7_VmThrowException(pCtx,` |
|       - | 6856 | `			"TypeError",` |
|       - | 6857 | `			"array_chunk(): Argument #1 ($array) must be of type array, %s given",` |
|       1 | 6858 | `			ph7_type_name(apArg[0])` |
|       - | 6859 | `			);` |
|       - | 6860 | `	}` |
|       - | 6861 | `	/* Create a new array */` |
|      43 | 6862 | `	pArray = ph7_context_new_array(pCtx);` |
|      43 | 6863 | `	if( pArray == 0 ){` |
|     ! 0 | 6864 | `		ph7_result_null(pCtx);` |
|     ! 0 | 6865 | `		return PH7_OK;` |
|       - | 6866 | `	}` |
|       - | 6867 | `	/* Point to the internal representation of the input hashmap */` |
|      43 | 6868 | `	pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|       - | 6869 | `	/* Extract and validate the chunk size argument. */` |
|       - | 6870 | `	/* Reject types that cannot be sensibly converted to an integer. */` |
|      57 | 6871 | `	if( ph7_value_is_array(apArg[1]) \|\| ph7_value_is_object(apArg[1]) \|\|` |
|      81 | 6872 | `		ph7_value_is_resource(apArg[1]) \|\| ph7_value_is_null(apArg[1]) \|\|` |
|      38 | 6873 | `		ph7_value_is_bool(apArg[1]) ){` |
|     ! 0 | 6874 | `		return PH7_VmThrowException(pCtx,` |
|       - | 6875 | `			"TypeError",` |
|       - | 6876 | `			"array_chunk(): Argument #2 ($length) must be of type int, %s given",` |
|     ! 0 | 6877 | `			ph7_type_name(apArg[1])` |
|       - | 6878 | `			);` |
|       - | 6879 | `	}` |
|       - | 6880 | `	/* Strings that are non-numeric produce a TypeError.  Numeric` |
|       - | 6881 | `	 * strings are permitted; however those representing floats lose` |
|       - | 6882 | `	 * precision and PHP emits a deprecation warning. */` |
|      43 | 6883 | `	if( ph7_value_is_string(apArg[1]) ){` |
|       - | 6884 | `		int len;` |
|       3 | 6885 | `		sxu8 bReal = FALSE;` |
|       3 | 6886 | `		const char *zStr = ph7_value_to_string(apArg[1], &len);` |
|       3 | 6887 | `		if( SyStrIsNumeric(zStr, len, &bReal, 0) != SXRET_OK ){` |
|       3 | 6888 | `			return PH7_VmThrowException(pCtx,` |
|       - | 6889 | `				"TypeError",` |
|       - | 6890 | `				"array_chunk(): Argument #2 ($length) must be of type int, string given"` |
|       - | 6891 | `				);` |
|       - | 6892 | `		}` |
|     ! 0 | 6893 | `		if( bReal ){` |
|       - | 6894 | `			/* float-string -> warn but allow */` |
|     ! 0 | 6895 | `			ph7_context_throw_error_format(pCtx, E_DEPRECATED,` |
|       - | 6896 | `				"Implicit conversion from float-string \"%s\" to int loses precision",` |
|     ! 0 | 6897 | `				zStr` |
|       - | 6898 | `				);` |
|     ! 0 | 6899 | `		}` |
|     ! 0 | 6900 | `	}` |
|       - | 6901 | `	/* If the value is a float with a fractional component, emit a` |
|       - | 6902 | `	 * deprecation warning but continue.  The following conversion occurs` |
|       - | 6903 | `	 * later via ph7_value_to_int. */` |
|      40 | 6904 | `	if( ph7_value_is_float(apArg[1]) ){` |
|       3 | 6905 | `		double d = ph7_value_to_double(apArg[1]);` |
|       3 | 6906 | `		sxi64 i = (sxi64)d;` |
|       3 | 6907 | `		if( d != (double)i ){` |
|       4 | 6908 | `			ph7_context_throw_error_format(pCtx, E_DEPRECATED,` |
|       - | 6909 | `				"Implicit conversion from float %g to int loses precision",` |
|       1 | 6910 | `				d` |
|       - | 6911 | `				);` |
|       1 | 6912 | `		}` |
|       1 | 6913 | `	}` |
|       - | 6914 | `	/* Convert using ph7_value_to_int; now that float fractions are` |
|       - | 6915 | `	 * eliminated, this will not produce a warning. */` |
|       - | 6916 | `	{` |
|      40 | 6917 | `		sxi64 nSizeSigned = ph7_value_to_int(apArg[1]);` |
|      40 | 6918 | `		if( nSizeSigned < 1 ){` |
|       - | 6919 | `			/* size <= 0 -> ValueError */` |
|       6 | 6920 | `			return PH7_VmThrowException(pCtx,` |
|       - | 6921 | `				"ValueError",` |
|       - | 6922 | `				"array_chunk(): Argument #2 ($length) must be greater than 0"` |
|       - | 6923 | `				);` |
|       - | 6924 | `		}` |
|      35 | 6925 | `		nSize = (sxu32)nSizeSigned;` |
|       - | 6926 | `	}` |
|      35 | 6927 | `	if( nSize >= pMap->nEntry ){` |
|       - | 6928 | `		/* Return the whole array */` |
|       3 | 6929 | `		ph7_array_add_elem(pArray,0,apArg[0]);` |
|       3 | 6930 | `		ph7_result_value(pCtx,pArray);` |
|       3 | 6931 | `		return PH7_OK;` |
|       - | 6932 | `	}` |
|      33 | 6933 | `	bPreserve = 0;` |
|      33 | 6934 | `	if( nArg > 2 ){` |
|       - | 6935 | `		/* The third argument has a bool type hint in PHP.  Values that` |
|       - | 6936 | `		 * cannot be sensibly converted (arrays, objects, resources) are` |
|       - | 6937 | `		 * rejected with a TypeError.  Scalars and null coerce to bool` |
|       - | 6938 | `		 * normally, matching PHP behaviour. */` |
|      35 | 6939 | `		if( ph7_value_is_array(apArg[2]) \|\|` |
|      35 | 6940 | `			ph7_value_is_object(apArg[2]) \|\|` |
|      20 | 6941 | `			ph7_value_is_resource(apArg[2]) ){` |
|       8 | 6942 | `			return PH7_VmThrowException(pCtx,` |
|       - | 6943 | `				"TypeError",` |
|       - | 6944 | `				"array_chunk(): Argument #3 ($preserve_keys) must be of type bool, %s given",` |
|       4 | 6945 | `				ph7_type_name(apArg[2])` |
|       - | 6946 | `				);` |
|       - | 6947 | `		}` |
|      21 | 6948 | `		bPreserve = ph7_value_to_bool(apArg[2]);` |
|      10 | 6949 | `	}` |
|       - | 6950 | `	/* Start processing */` |
|      27 | 6951 | `	pEntry = pMap->pFirst;` |
|      27 | 6952 | `	nChunk = 0;` |
|      27 | 6953 | `	pChunk = 0;` |
|      27 | 6954 | `	n = pMap->nEntry;` |
|      56 | 6955 | `	for( ;; ){` |
|     113 | 6956 | `		if( n < 1 ){` |
|       - | 6957 | `			/* When the loop terminates we may still have a current chunk` |
|       - | 6958 | `			 * that hasn't been added to the result array.  The previous` |
|       - | 6959 | `			 * implementation only pushed it if nChunk>0 which dropped the` |
|       - | 6960 | `			 * final chunk when the input size was an exact multiple of` |
|       - | 6961 | `			 * the chunk length.  Always append the pending chunk if it` |
|       - | 6962 | `			 * exists. */` |
|      27 | 6963 | `			if( pChunk ){` |
|      27 | 6964 | `				ph7_array_add_elem(pArray,0,pChunk); /* Will have its own copy */` |
|      13 | 6965 | `			}` |
|      27 | 6966 | `			break;` |
|       - | 6967 | `		}` |
|      87 | 6968 | `		if( nChunk < 1 ){` |
|      71 | 6969 | `			if( pChunk ){` |
|       - | 6970 | `				/* Put the first chunk */` |
|      45 | 6971 | `				ph7_array_add_elem(pArray,0,pChunk); /* Will have it's own copy */` |
|      22 | 6972 | `			}` |
|       - | 6973 | `			/* Create a new dimension */` |
|      71 | 6974 | `			pChunk = ph7_context_new_array(pCtx); /* Don't worry about freeing memory here,everything` |
|       - | 6975 | `												   * will be automatically released as soon we return` |
|       - | 6976 | `												   * from this function */` |
|      71 | 6977 | `			if( pChunk == 0 ){` |
|     ! 0 | 6978 | `				break;` |
|       - | 6979 | `			}` |
|      71 | 6980 | `			nChunk = nSize;` |
|      35 | 6981 | `		}` |
|       - | 6982 | `		/* Insert the entry */` |
|      87 | 6983 | `		HashmapInsertNode((ph7_hashmap *)pChunk->x.pOther,pEntry,bPreserve);` |
|       - | 6984 | `		/* Point to the next entry */` |
|      87 | 6985 | `		pEntry = pEntry->pPrev; /* Reverse link */` |
|      87 | 6986 | `		nChunk--;` |
|      87 | 6987 | `		n--;` |
|       1 | 6988 | `	}` |
|       - | 6989 | `	/* Return the multidimensional array */` |
|      27 | 6990 | `	ph7_result_value(pCtx,pArray);` |
|      27 | 6991 | `	return PH7_OK;` |
|      26 | 6992 | `}` |
|       - | 6993 | `/*` |
|       - | 6994 | ` * array array_pad(array $input,int $pad_size,value $pad_value)` |
|       - | 6995 | ` *  Pad array to the specified length with a value.` |
|       - | 6996 | ` * $input` |
|       - | 6997 | ` *   Initial array of values to pad.` |
|       - | 6998 | ` * $pad_size` |
|       - | 6999 | ` *   New size of the array.` |
|       - | 7000 | ` * $pad_value` |
|       - | 7001 | ` *   Value to pad if input is less than pad_size.` |
|       - | 7002 | ` */` |
|      28 | 7003 | `static int ph7_hashmap_pad(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       5 | 7004 | `{` |
|       - | 7005 | `	ph7_hashmap *pMap;` |
|       - | 7006 | `	ph7_value *pArray;` |
|       - | 7007 | `	int nEntry;` |
|      33 | 7008 | `	if( nArg != 3 ){` |
|      12 | 7009 | `		return PH7_VmThrowException(pCtx,` |
|       - | 7010 | `			"ArgumentCountError",` |
|       - | 7011 | `			"array_pad() expects exactly 3 arguments, %d given",` |
|       3 | 7012 | `			nArg` |
|       - | 7013 | `			);` |
|       - | 7014 | `	}` |
|      24 | 7015 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|       4 | 7016 | `		return PH7_VmThrowException(pCtx,` |
|       - | 7017 | `			"TypeError",` |
|       - | 7018 | `			"array_pad(): Argument #1 ($array) must be of type array, %s given",` |
|       1 | 7019 | `			ph7_type_name(apArg[0])` |
|       - | 7020 | `			);` |
|       - | 7021 | `	}` |
|       - | 7022 | `	/* Create a new array */` |
|      21 | 7023 | `	pArray = ph7_context_new_array(pCtx);` |
|      21 | 7024 | `	if( pArray == 0 ){` |
|     ! 0 | 7025 | `		return PH7_ContextMemoryError(pCtx);` |
|       - | 7026 | `	}` |
|       - | 7027 | `	/* Point to the internal representation of the input hashmap */` |
|      21 | 7028 | `	pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|       - | 7029 | `	/* Extract the total number of desired entry to insert */` |
|      21 | 7030 | `	nEntry = ph7_value_to_int(apArg[1]);` |
|      21 | 7031 | `	if( nEntry < 0 ){` |
|       9 | 7032 | `		nEntry = -nEntry;` |
|       9 | 7033 | `		if( nEntry > (int)pMap->nEntry ){` |
|       5 | 7034 | `			nEntry -= (int)pMap->nEntry;` |
|       - | 7035 | `			/* Insert given items first */` |
|      17 | 7036 | `			while( nEntry > 0 ){` |
|      13 | 7037 | `				if( ph7_array_add_elem(pArray,0,apArg[2]) != SXRET_OK ){` |
|     ! 0 | 7038 | `					return PH7_ContextMemoryError(pCtx);` |
|       - | 7039 | `				}` |
|      13 | 7040 | `				nEntry--;` |
|       1 | 7041 | `			}` |
|       - | 7042 | `			/* Merge the two arrays */` |
|       5 | 7043 | `			HashmapMerge(pMap,(ph7_hashmap *)pArray->x.pOther);` |
|       3 | 7044 | `		}else{` |
|       5 | 7045 | `			PH7_HashmapDup(pMap,(ph7_hashmap *)pArray->x.pOther);` |
|       1 | 7046 | `		}` |
|      17 | 7047 | `	}else if( nEntry > 0 ){` |
|      11 | 7048 | `		if( nEntry > (int)pMap->nEntry ){` |
|       7 | 7049 | `			nEntry -= (int)pMap->nEntry;` |
|       - | 7050 | `			/* Merge the two arrays first */` |
|       7 | 7051 | `			HashmapMerge(pMap,(ph7_hashmap *)pArray->x.pOther);` |
|       - | 7052 | `			/* Insert given items */` |
|      25 | 7053 | `			while( nEntry > 0 ){` |
|      19 | 7054 | `				if( ph7_array_add_elem(pArray,0,apArg[2]) != SXRET_OK ){` |
|     ! 0 | 7055 | `					return PH7_ContextMemoryError(pCtx);` |
|       - | 7056 | `				}` |
|      19 | 7057 | `				nEntry--;` |
|       1 | 7058 | `			}` |
|       4 | 7059 | `		}else{` |
|       5 | 7060 | `			PH7_HashmapDup(pMap,(ph7_hashmap *)pArray->x.pOther);` |
|       - | 7061 | `		}` |
|       6 | 7062 | `	}else{` |
|       - | 7063 | `		/* nEntry == 0: return a copy of the input array */` |
|       3 | 7064 | `		PH7_HashmapDup(pMap,(ph7_hashmap *)pArray->x.pOther);` |
|       - | 7065 | `	}` |
|       - | 7066 | `	/* Return the new array */` |
|      21 | 7067 | `	ph7_result_value(pCtx,pArray);` |
|      21 | 7068 | `	return PH7_OK;` |
|      19 | 7069 | `}` |
|       - | 7070 | `/*` |
|       - | 7071 | ` * array array_replace(array &$array,array &$array1,...)` |
|       - | 7072 | ` *  Replaces elements from passed arrays into the first array.` |
|       - | 7073 | ` * Parameters` |
|       - | 7074 | ` * $array` |
|       - | 7075 | ` *   The array in which elements are replaced.` |
|       - | 7076 | ` * $array1` |
|       - | 7077 | ` *   The array from which elements will be extracted.` |
|       - | 7078 | ` * ....` |
|       - | 7079 | ` *  More arrays from which elements will be extracted.` |
|       - | 7080 | ` *  Values from later arrays overwrite the previous values.` |
|       - | 7081 | ` * Return` |
|       - | 7082 | ` *  Returns an array.` |
|       - | 7083 | ` *  Throws ArgumentCountError if no arguments are given.` |
|       - | 7084 | ` *  Throws TypeError if any argument is not an array.` |
|       - | 7085 | ` */` |
|      22 | 7086 | `static int ph7_hashmap_replace(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       4 | 7087 | `{` |
|       - | 7088 | `	ph7_hashmap *pMap;` |
|       - | 7089 | `	ph7_value *pArray;` |
|       - | 7090 | `	int i;` |
|      26 | 7091 | `	if( nArg < 1 ){` |
|       3 | 7092 | `		return PH7_VmThrowException(pCtx,` |
|       - | 7093 | `			"ArgumentCountError",` |
|       - | 7094 | `			"array_replace() expects at least 1 argument, 0 given"` |
|       - | 7095 | `			);` |
|       - | 7096 | `	}` |
|      23 | 7097 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|       4 | 7098 | `		return PH7_VmThrowException(pCtx,` |
|       - | 7099 | `			"TypeError",` |
|       - | 7100 | `			"array_replace(): Argument #1 ($array) must be of type array, %s given",` |
|       1 | 7101 | `			ph7_type_name(apArg[0])` |
|       - | 7102 | `			);` |
|       - | 7103 | `	}` |
|       - | 7104 | `	/* Create a new array */` |
|      20 | 7105 | `	pArray = ph7_context_new_array(pCtx);` |
|      20 | 7106 | `	if( pArray == 0 ){` |
|     ! 0 | 7107 | `		ph7_result_null(pCtx);` |
|     ! 0 | 7108 | `		return PH7_OK;` |
|       - | 7109 | `	}` |
|       - | 7110 | `	/* Overwrite from the first array */` |
|      20 | 7111 | `	pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|      20 | 7112 | `	HashmapOverwrite(pMap,(ph7_hashmap *)pArray->x.pOther);` |
|       - | 7113 | `	/* Perform the requested operation for remaining arrays */` |
|      36 | 7114 | `	for( i = 1 ; i < nArg ; i++ ){` |
|      20 | 7115 | `		if( !ph7_value_is_array(apArg[i]) ){` |
|       - | 7116 | `			/* Type mismatch -> TypeError */` |
|       4 | 7117 | `			return PH7_VmThrowException(pCtx,` |
|       - | 7118 | `				"TypeError",` |
|       - | 7119 | `				"array_replace(): Argument #%d must be of type array, %s given",` |
|       1 | 7120 | `				i + 1,` |
|       2 | 7121 | `				ph7_type_name(apArg[i])` |
|       - | 7122 | `				);` |
|       - | 7123 | `		}` |
|       - | 7124 | `		/* Point to the internal representation of the input hashmap */` |
|      17 | 7125 | `		pMap = (ph7_hashmap *)apArg[i]->x.pOther;` |
|      17 | 7126 | `		HashmapOverwrite(pMap,(ph7_hashmap *)pArray->x.pOther);` |
|       9 | 7127 | `	}` |
|       - | 7128 | `	/* Return the new array */` |
|      17 | 7129 | `	ph7_result_value(pCtx,pArray);` |
|      17 | 7130 | `	return PH7_OK;` |
|      15 | 7131 | `}` |
|       - | 7132 | `/*` |
|       - | 7133 | ` * array array_filter(array $input [,callback $callback ])` |
|       - | 7134 | ` *  Filters elements of an array using a callback function.` |
|       - | 7135 | ` * Parameters` |
|       - | 7136 | ` *  $input` |
|       - | 7137 | ` *    The array to iterate over` |
|       - | 7138 | ` * $callback` |
|       - | 7139 | ` *    The callback function to use` |
|       - | 7140 | ` *    If no callback is supplied, all entries of input equal to FALSE (see converting to boolean)` |
|       - | 7141 | ` *    will be removed.` |
|       - | 7142 | ` * Return` |
|       - | 7143 | ` *  The filtered array.` |
|       - | 7144 | ` */` |
|      20 | 7145 | `static int ph7_hashmap_filter(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       2 | 7146 | `{` |
|       - | 7147 | `	ph7_hashmap_node *pEntry;` |
|       - | 7148 | `	ph7_hashmap *pMap;` |
|       - | 7149 | `	ph7_value *pArray;` |
|       - | 7150 | `	ph7_value sResult;   /* Callback result */` |
|       - | 7151 | `	ph7_value *pValue;` |
|       - | 7152 | `	sxi32 rc;` |
|       - | 7153 | `	int keep;` |
|       - | 7154 | `	sxu32 n;` |
|      22 | 7155 | `	if( nArg < 1 \|\| !ph7_value_is_array(apArg[0]) ){` |
|       - | 7156 | `		/* Invalid arguments,return NULL */` |
|       3 | 7157 | `		ph7_result_null(pCtx);` |
|       3 | 7158 | `		return PH7_OK;` |
|       - | 7159 | `	}` |
|       - | 7160 | `	/* Create a new array */` |
|      20 | 7161 | `	pArray = ph7_context_new_array(pCtx);` |
|      20 | 7162 | `	if( pArray == 0 ){` |
|     ! 0 | 7163 | `		ph7_result_null(pCtx);` |
|     ! 0 | 7164 | `		return PH7_OK;` |
|       - | 7165 | `	}` |
|       - | 7166 | `	/* Point to the internal representation of the input hashmap */` |
|      20 | 7167 | `	pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|      20 | 7168 | `	pEntry = pMap->pFirst;` |
|      20 | 7169 | `	PH7_MemObjInit(pMap->pVm,&sResult);` |
|      20 | 7170 | `	sResult.nIdx = SXU32_HIGH; /* Mark as constant */` |
|       - | 7171 | `	/* Perform the requested operation */` |
|      78 | 7172 | `	for( n = 0 ; n < pMap->nEntry ; n++ ){` |
|       - | 7173 | `		/* Extract node value (may be NULL if allocation failed) */` |
|      64 | 7174 | `		pValue = HashmapExtractNodeValue(pEntry);` |
|      64 | 7175 | `		if( pValue == 0 ){` |
|       - | 7176 | `			/* Can happen if SySetAt() failed earlier; drop the entry. */` |
|     ! 0 | 7177 | `			keep = FALSE;` |
|      64 | 7178 | `		}else if( nArg > 1 && !ph7_value_is_null(apArg[1]) ){` |
|       - | 7179 | `			/* Callback was supplied (not NULL).  PHP 8 throws a` |
|       - | 7180 | `				* TypeError when the value is not callable or null; prior PH7` |
|       - | 7181 | `				* silently dropped the element.  Emit similar message. */` |
|      36 | 7182 | `			if( !ph7_value_is_callable(apArg[1]) ){` |
|       3 | 7183 | `				if( ph7_value_is_string(apArg[1]) ){` |
|       - | 7184 | `					int len;` |
|       3 | 7185 | `					const char *zName = ph7_value_to_string(apArg[1], &len);` |
|       4 | 7186 | `					return PH7_VmThrowException(pCtx,` |
|       - | 7187 | `						"TypeError",` |
|       - | 7188 | `						"array_filter(): Argument #2 ($callback) must be a valid callback or null, function \"%s\" not found or invalid function name",` |
|       1 | 7189 | `						zName` |
|       - | 7190 | `						);` |
|     ! 0 | 7191 | `				}else{` |
|     ! 0 | 7192 | `					return PH7_VmThrowException(pCtx,` |
|       - | 7193 | `						"TypeError",` |
|       - | 7194 | `						"array_filter(): Argument #2 ($callback) must be a valid callback or null, %s given",` |
|     ! 0 | 7195 | `						ph7_type_name(apArg[1])` |
|       - | 7196 | `						);` |
|       - | 7197 | `				}` |
|       - | 7198 | `			}` |
|      33 | 7199 | `			keep = FALSE;` |
|      33 | 7200 | `			rc = PH7_VmCallUserFunction(pMap->pVm,apArg[1],1,&pValue,&sResult);` |
|      33 | 7201 | `			if( rc == PH7_EXCEPTION ){` |
|       - | 7202 | `				/* The callback raised: propagate so the dispatcher unwinds. */` |
|       3 | 7203 | `				PH7_MemObjRelease(&sResult);` |
|       3 | 7204 | `				return PH7_EXCEPTION;` |
|       - | 7205 | `			}` |
|      31 | 7206 | `			if( rc == SXRET_OK ){` |
|       - | 7207 | `				/* Perform a boolean cast */` |
|      31 | 7208 | `				keep = ph7_value_to_bool(&sResult);` |
|      15 | 7209 | `			}` |
|      31 | 7210 | `			PH7_MemObjRelease(&sResult);` |
|      16 | 7211 | `		}else{` |
|       - | 7212 | `			/* No callback provided or callback explicitly NULL: use default` |
|       - | 7213 | `			 * behaviour where "empty" values are removed. This also covers` |
|       - | 7214 | `			 * the case where the callback argument is missing entirely.` |
|       - | 7215 | `			 */` |
|      29 | 7216 | `			keep = !PH7_MemObjIsEmpty(pValue);` |
|       - | 7217 | `		}` |
|      59 | 7218 | `		if( keep ){` |
|       - | 7219 | `			/* Perform the insertion,now the callback returned true */` |
|      21 | 7220 | `			HashmapInsertNode((ph7_hashmap *)pArray->x.pOther,pEntry,TRUE);` |
|      10 | 7221 | `		}` |
|       - | 7222 | `		/* Point to the next entry */` |
|      59 | 7223 | `		pEntry = pEntry->pPrev; /* Reverse link */` |
|      30 | 7224 | `	}` |
|      15 | 7225 | `	ph7_result_value(pCtx,pArray);` |
|      15 | 7226 | `	return PH7_OK;` |
|      12 | 7227 | `}` |
|       - | 7228 | `/*` |
|       - | 7229 | ` * array array_map(?callable $callback, array $array, array ...$arrays)` |
|       - | 7230 | ` *  Applies the callback to the elements of the given arrays.` |
|       - | 7231 | ` * Parameters` |
|       - | 7232 | ` *  $callback` |
|       - | 7233 | ` *   A callable to run for each element in each array, or NULL. With a single` |
|       - | 7234 | ` *   array and a NULL callback this is the identity function (the array is` |
|       - | 7235 | ` *   returned unchanged); with several arrays and a NULL callback the arrays` |
|       - | 7236 | ` *   are zipped together.` |
|       - | 7237 | ` *  $array` |
|       - | 7238 | ` *   The first array to run through the callback function.` |
|       - | 7239 | ` *  $arrays` |
|       - | 7240 | ` *   Zero or more additional arrays to process in parallel.` |
|       - | 7241 | ` * Return` |
|       - | 7242 | ` *  Returns an array containing the results of applying the callback function.` |
|       - | 7243 | ` *  With a single array the keys are preserved; with several arrays the result` |
|       - | 7244 | ` *  is re-indexed and the iteration runs to the length of the longest array,` |
|       - | 7245 | ` *  padding shorter arrays with NULL.` |
|       - | 7246 | ` */` |
|      54 | 7247 | `static int ph7_hashmap_map(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       5 | 7248 | `{` |
|       - | 7249 | `	ph7_value *pArray,*pValue,sKey,sResult;` |
|       - | 7250 | `	ph7_hashmap_node *pEntry;` |
|       - | 7251 | `	ph7_hashmap *pMap;` |
|       - | 7252 | `	ph7_vm *pVm;` |
|       - | 7253 | `	int bNullCallback;` |
|       - | 7254 | `	sxi32 rc;` |
|       - | 7255 | `	int i;` |
|       - | 7256 | `	sxu32 n;` |
|      59 | 7257 | `	if( nArg < 2 ){` |
|       8 | 7258 | `		return PH7_VmThrowException(pCtx,` |
|       - | 7259 | `			"ArgumentCountError",` |
|       - | 7260 | `			"array_map() expects at least 2 arguments, %d given",` |
|       2 | 7261 | `			nArg` |
|       - | 7262 | `			);` |
|       - | 7263 | `	}` |
|      54 | 7264 | `	bNullCallback = ph7_value_is_null(apArg[0]);` |
|      54 | 7265 | `	if( !bNullCallback && !ph7_value_is_callable(apArg[0]) ){` |
|       5 | 7266 | `		if( ph7_value_is_string(apArg[0]) ){` |
|       3 | 7267 | `			const char *zFunc = ph7_value_to_string(apArg[0],0);` |
|       4 | 7268 | `			return PH7_VmThrowException(pCtx,` |
|       - | 7269 | `				"TypeError",` |
|       - | 7270 | `				"array_map(): Argument #1 ($callback) must be a valid callback or null, "` |
|       - | 7271 | `				"function \"%s\" not found or invalid function name",` |
|       1 | 7272 | `				zFunc` |
|       - | 7273 | `				);` |
|       - | 7274 | `		}` |
|       3 | 7275 | `		return PH7_VmThrowException(pCtx,` |
|       - | 7276 | `			"TypeError",` |
|       - | 7277 | `			"array_map(): Argument #1 ($callback) must be a valid callback or null, "` |
|       - | 7278 | `			"no array or string given"` |
|       - | 7279 | `			);` |
|       - | 7280 | `	}` |
|       - | 7281 | `	/* Every remaining argument must be an array */` |
|     105 | 7282 | `	for( i = 1 ; i < nArg ; i++ ){` |
|      61 | 7283 | `		if( !ph7_value_is_array(apArg[i]) ){` |
|       3 | 7284 | `			if( i == 1 ){` |
|       4 | 7285 | `				return PH7_VmThrowException(pCtx,` |
|       - | 7286 | `					"TypeError",` |
|       - | 7287 | `					"array_map(): Argument #2 ($array) must be of type array, %s given",` |
|       2 | 7288 | `					ph7_type_name(apArg[1])` |
|       - | 7289 | `					);` |
|       - | 7290 | `			}` |
|     ! 0 | 7291 | `			return PH7_VmThrowException(pCtx,` |
|       - | 7292 | `				"TypeError",` |
|       - | 7293 | `				"array_map(): Argument #%d must be of type array, %s given",` |
|     ! 0 | 7294 | `				i+1,ph7_type_name(apArg[i])` |
|       - | 7295 | `				);` |
|       - | 7296 | `		}` |
|      30 | 7297 | `	}` |
|      46 | 7298 | `	pVm = pCtx->pVm;` |
|       - | 7299 | `	/* Create a new array */` |
|      46 | 7300 | `	pArray = ph7_context_new_array(pCtx);` |
|      46 | 7301 | `	if( pArray == 0 ){` |
|     ! 0 | 7302 | `		ph7_result_null(pCtx);` |
|     ! 0 | 7303 | `		return PH7_OK;` |
|       - | 7304 | `	}` |
|      46 | 7305 | `	PH7_MemObjInit(pVm,&sResult);` |
|      46 | 7306 | `	PH7_MemObjInit(pVm,&sKey);` |
|      46 | 7307 | `	sResult.nIdx = SXU32_HIGH; /* Mark as constant */` |
|      46 | 7308 | `	sKey.nIdx    = SXU32_HIGH; /* Mark as constant */` |
|      46 | 7309 | `	if( nArg == 2 ){` |
|       - | 7310 | `		/* Single-array mode: keys are preserved (PHP semantics). */` |
|      36 | 7311 | `		pMap = (ph7_hashmap *)apArg[1]->x.pOther;` |
|      36 | 7312 | `		pEntry = pMap->pFirst;` |
|     110 | 7313 | `		for( n = 0 ; n < pMap->nEntry ; n++ ){` |
|       - | 7314 | `			/* Extract the node value */` |
|      78 | 7315 | `			pValue = HashmapExtractNodeValue(pEntry);` |
|      78 | 7316 | `			if( pValue ){` |
|       - | 7317 | `				/* Extract the node key */` |
|      78 | 7318 | `				PH7_HashmapExtractNodeKey(pEntry,&sKey);` |
|      78 | 7319 | `				if( bNullCallback ){` |
|       - | 7320 | `					/* NULL callback: identity function, keep original value */` |
|      11 | 7321 | `					ph7_array_add_elem(pArray,&sKey,pValue);` |
|       6 | 7322 | `				}else{` |
|       - | 7323 | `					/* Invoke the supplied callback */` |
|      68 | 7324 | `					rc = PH7_VmCallUserFunction(pVm,apArg[0],1,&pValue,&sResult);` |
|      68 | 7325 | `					if( rc == PH7_EXCEPTION ){` |
|       - | 7326 | `						/* Callback raised: abort and let the foreign-function` |
|       - | 7327 | `						 * dispatcher unwind through the nearest try/catch. */` |
|       3 | 7328 | `						PH7_MemObjRelease(&sKey);` |
|       3 | 7329 | `						PH7_MemObjRelease(&sResult);` |
|       3 | 7330 | `						return PH7_EXCEPTION;` |
|       - | 7331 | `					}` |
|       - | 7332 | `					/* Insert the callback return value */` |
|      66 | 7333 | `					ph7_array_add_elem(pArray,&sKey,&sResult);` |
|       - | 7334 | `				}` |
|      76 | 7335 | `				PH7_MemObjRelease(&sKey);` |
|      76 | 7336 | `				PH7_MemObjRelease(&sResult);` |
|      37 | 7337 | `			}` |
|       - | 7338 | `			/* Point to the next entry */` |
|      76 | 7339 | `			pEntry = pEntry->pPrev; /* Reverse link */` |
|      39 | 7340 | `		}` |
|      18 | 7341 | `	}else{` |
|       - | 7342 | `		/* Multi-array mode: walk every array in parallel to the length of the` |
|       - | 7343 | `		 * longest one, pad shorter arrays with NULL, and re-index the result. */` |
|      11 | 7344 | `		int nArrays = nArg - 1;` |
|       - | 7345 | `		ph7_hashmap_node **apCur;` |
|       - | 7346 | `		ph7_value **apCallArg;` |
|       - | 7347 | `		ph7_value sNull;` |
|      11 | 7348 | `		sxu32 nMax = 0;` |
|      11 | 7349 | `		apCur     = (ph7_hashmap_node **)SyMemBackendAlloc(&pVm->sAllocator,(sxu32)(nArrays*sizeof(ph7_hashmap_node *)));` |
|      11 | 7350 | `		apCallArg = (ph7_value **)SyMemBackendAlloc(&pVm->sAllocator,(sxu32)(nArrays*sizeof(ph7_value *)));` |
|      11 | 7351 | `		if( apCur == 0 \|\| apCallArg == 0 ){` |
|     ! 0 | 7352 | `			if( apCur ){ SyMemBackendFree(&pVm->sAllocator,apCur); }` |
|     ! 0 | 7353 | `			if( apCallArg ){ SyMemBackendFree(&pVm->sAllocator,apCallArg); }` |
|     ! 0 | 7354 | `			PH7_MemObjRelease(&sKey);` |
|     ! 0 | 7355 | `			PH7_MemObjRelease(&sResult);` |
|     ! 0 | 7356 | `			ph7_result_value(pCtx,pArray);` |
|     ! 0 | 7357 | `			return PH7_OK;` |
|       - | 7358 | `		}` |
|      11 | 7359 | `		PH7_MemObjInit(pVm,&sNull); /* shared NULL pad for short arrays */` |
|      11 | 7360 | `		sNull.nIdx = SXU32_HIGH;` |
|      33 | 7361 | `		for( i = 0 ; i < nArrays ; i++ ){` |
|      23 | 7362 | `			pMap = (ph7_hashmap *)apArg[i+1]->x.pOther;` |
|      23 | 7363 | `			apCur[i] = pMap->pFirst;` |
|      23 | 7364 | `			if( pMap->nEntry > nMax ){` |
|      13 | 7365 | `				nMax = pMap->nEntry;` |
|       6 | 7366 | `			}` |
|      12 | 7367 | `		}` |
|      35 | 7368 | `		for( n = 0 ; n < nMax ; n++ ){` |
|      25 | 7369 | `			ph7_value *pZip = 0;` |
|      25 | 7370 | `			if( bNullCallback ){` |
|       - | 7371 | `				/* zip: each result element is an array of the i-th values */` |
|       5 | 7372 | `				pZip = ph7_context_new_array(pCtx);` |
|       2 | 7373 | `			}` |
|      79 | 7374 | `			for( i = 0 ; i < nArrays ; i++ ){` |
|      55 | 7375 | `				ph7_value *pv = &sNull;` |
|      55 | 7376 | `				if( apCur[i] ){` |
|      53 | 7377 | `					ph7_value *pNodeVal = HashmapExtractNodeValue(apCur[i]);` |
|      53 | 7378 | `					if( pNodeVal ){` |
|      53 | 7379 | `						pv = pNodeVal;` |
|      26 | 7380 | `					}` |
|      53 | 7381 | `					apCur[i] = apCur[i]->pPrev; /* Reverse link */` |
|      26 | 7382 | `				}` |
|      55 | 7383 | `				if( bNullCallback ){` |
|       9 | 7384 | `					if( pZip ){` |
|       9 | 7385 | `						ph7_array_add_elem(pZip,0,pv);` |
|       4 | 7386 | `					}` |
|       5 | 7387 | `				}else{` |
|      47 | 7388 | `					apCallArg[i] = pv;` |
|       - | 7389 | `				}` |
|      28 | 7390 | `			}` |
|      25 | 7391 | `			if( bNullCallback ){` |
|       5 | 7392 | `				if( pZip ){` |
|       5 | 7393 | `					ph7_array_add_elem(pArray,0,pZip);` |
|       2 | 7394 | `				}` |
|       3 | 7395 | `			}else{` |
|      21 | 7396 | `				rc = PH7_VmCallUserFunction(pVm,apArg[0],nArrays,apCallArg,&sResult);` |
|      21 | 7397 | `				if( rc == PH7_EXCEPTION ){` |
|     ! 0 | 7398 | `					SyMemBackendFree(&pVm->sAllocator,apCur);` |
|     ! 0 | 7399 | `					SyMemBackendFree(&pVm->sAllocator,apCallArg);` |
|     ! 0 | 7400 | `					PH7_MemObjRelease(&sNull);` |
|     ! 0 | 7401 | `					PH7_MemObjRelease(&sKey);` |
|     ! 0 | 7402 | `					PH7_MemObjRelease(&sResult);` |
|     ! 0 | 7403 | `					return PH7_EXCEPTION;` |
|       - | 7404 | `				}` |
|      21 | 7405 | `				ph7_array_add_elem(pArray,0,&sResult);` |
|      21 | 7406 | `				PH7_MemObjRelease(&sResult);` |
|       - | 7407 | `			}` |
|      13 | 7408 | `		}` |
|      11 | 7409 | `		SyMemBackendFree(&pVm->sAllocator,apCur);` |
|      11 | 7410 | `		SyMemBackendFree(&pVm->sAllocator,apCallArg);` |
|      11 | 7411 | `		PH7_MemObjRelease(&sNull);` |
|       - | 7412 | `	}` |
|      44 | 7413 | `	PH7_MemObjRelease(&sKey);` |
|      44 | 7414 | `	PH7_MemObjRelease(&sResult);` |
|      44 | 7415 | `	ph7_result_value(pCtx,pArray);` |
|      44 | 7416 | `	return PH7_OK;` |
|      32 | 7417 | `}` |
|       - | 7418 | `/*` |
|       - | 7419 | ` * value array_reduce(array $array, callable $callback[, value $initial = NULL])` |
|       - | 7420 | ` *  Iteratively reduce the array to a single value using a callback function.` |
|       - | 7421 | ` * Parameters` |
|       - | 7422 | ` *  $array` |
|       - | 7423 | ` *   The input array.` |
|       - | 7424 | ` *  $callback` |
|       - | 7425 | ` *   The callback function. Signature: callback(mixed $carry, mixed $item): mixed` |
|       - | 7426 | ` *  $initial` |
|       - | 7427 | ` *   If the optional initial is available, it will be used at the beginning` |
|       - | 7428 | ` *   of the process, or as a final result in case the array is empty.` |
|       - | 7429 | ` * Return` |
|       - | 7430 | ` *  Returns the resulting value.` |
|       - | 7431 | ` *  If the array is empty and initial is not passed, array_reduce() returns NULL.` |
|       - | 7432 | ` */` |
|      34 | 7433 | `static int ph7_hashmap_reduce(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       5 | 7434 | `{` |
|       - | 7435 | `	ph7_hashmap_node *pEntry;` |
|       - | 7436 | `	ph7_hashmap *pMap;` |
|       - | 7437 | `	ph7_value *pValue;` |
|       - | 7438 | `	ph7_value sResult;` |
|       - | 7439 | `	sxi32 rc;` |
|       - | 7440 | `	sxu32 n;` |
|      39 | 7441 | `	if( nArg < 2 ){` |
|       8 | 7442 | `		return PH7_VmThrowException(pCtx,` |
|       - | 7443 | `			"ArgumentCountError",` |
|       - | 7444 | `			"array_reduce() expects at least 2 arguments, %d given",` |
|       2 | 7445 | `			nArg` |
|       - | 7446 | `			);` |
|       - | 7447 | `	}` |
|      35 | 7448 | `	if( nArg > 3 ){` |
|       4 | 7449 | `		return PH7_VmThrowException(pCtx,` |
|       - | 7450 | `			"ArgumentCountError",` |
|       - | 7451 | `			"array_reduce() expects at most 3 arguments, %d given",` |
|       1 | 7452 | `			nArg` |
|       - | 7453 | `			);` |
|       - | 7454 | `	}` |
|      33 | 7455 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|       4 | 7456 | `		return PH7_VmThrowException(pCtx,` |
|       - | 7457 | `			"TypeError",` |
|       - | 7458 | `			"array_reduce(): Argument #1 ($array) must be of type array, %s given",` |
|       1 | 7459 | `			ph7_type_name(apArg[0])` |
|       - | 7460 | `			);` |
|       - | 7461 | `	}` |
|      31 | 7462 | `	if( !ph7_value_is_callable(apArg[1]) ){` |
|      12 | 7463 | `		if( ph7_value_is_string(apArg[1]) ){` |
|       3 | 7464 | `			const char *zFunc = ph7_value_to_string(apArg[1],0);` |
|       4 | 7465 | `			return PH7_VmThrowException(pCtx,` |
|       - | 7466 | `				"TypeError",` |
|       - | 7467 | `				"array_reduce(): Argument #2 ($callback) must be a valid callback, "` |
|       - | 7468 | `				"function \"%s\" not found or invalid function name",` |
|       1 | 7469 | `				zFunc` |
|       - | 7470 | `				);` |
|       - | 7471 | `		}` |
|       9 | 7472 | `		if( ph7_value_is_array(apArg[1]) ){` |
|       3 | 7473 | `			return PH7_VmThrowException(pCtx,` |
|       - | 7474 | `				"TypeError",` |
|       - | 7475 | `				"array_reduce(): Argument #2 ($callback) must be a valid callback, "` |
|       - | 7476 | `				"array callback must have exactly two members"` |
|       - | 7477 | `				);` |
|       - | 7478 | `		}` |
|       6 | 7479 | `		return PH7_VmThrowException(pCtx,` |
|       - | 7480 | `			"TypeError",` |
|       - | 7481 | `			"array_reduce(): Argument #2 ($callback) must be a valid callback, "` |
|       - | 7482 | `			"no array or string given"` |
|       - | 7483 | `			);` |
|       - | 7484 | `	}` |
|       - | 7485 | `	/* Point to the internal representation of the input hashmap */` |
|      19 | 7486 | `	pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|       - | 7487 | `	/* Assume a NULL initial value */` |
|      19 | 7488 | `	PH7_MemObjInit(pMap->pVm,&sResult);` |
|      19 | 7489 | `	sResult.nIdx = SXU32_HIGH; /* Mark as constant */` |
|      19 | 7490 | `	if( nArg > 2 ){` |
|       - | 7491 | `		/* Set the initial value */` |
|      13 | 7492 | `		PH7_MemObjLoad(apArg[2],&sResult);` |
|       6 | 7493 | `	}` |
|       - | 7494 | `	/* Perform the requested operation */` |
|      19 | 7495 | `	pEntry = pMap->pFirst;` |
|      55 | 7496 | `	for( n = 0 ; n < pMap->nEntry ; n++ ){` |
|       - | 7497 | `		/* Extract the node value */` |
|      39 | 7498 | `		pValue = HashmapExtractNodeValue(pEntry);` |
|       - | 7499 | `		/* Invoke the supplied callback */` |
|      39 | 7500 | `		rc = PH7_VmCallUserFunctionAp(pMap->pVm,apArg[1],&sResult,&sResult,pValue,0);` |
|      39 | 7501 | `		if( rc == PH7_EXCEPTION ){` |
|       - | 7502 | `			/* The callback raised: propagate so the dispatcher unwinds. */` |
|       3 | 7503 | `			PH7_MemObjRelease(&sResult);` |
|       3 | 7504 | `			return PH7_EXCEPTION;` |
|       - | 7505 | `		}` |
|       - | 7506 | `		/* Point to the next entry */` |
|      37 | 7507 | `		pEntry = pEntry->pPrev; /* Reverse link */` |
|      19 | 7508 | `	}` |
|      17 | 7509 | `	ph7_result_value(pCtx,&sResult); /* Will make it's own copy */` |
|      17 | 7510 | `	PH7_MemObjRelease(&sResult);` |
|      17 | 7511 | `	return PH7_OK;` |
|      22 | 7512 | `}` |
|       - | 7513 | `/*` |
|       - | 7514 | ` * bool array_walk(array &$array, callback $funcname [, mixed $userdata])` |
|       - | 7515 | ` *  Apply a user function to every member of an array.` |
|       - | 7516 | ` * Parameters` |
|       - | 7517 | ` *  $array` |
|       - | 7518 | ` *   The input array.` |
|       - | 7519 | ` *  $funcname` |
|       - | 7520 | ` *   Typically, funcname takes on two parameters. The array parameter's value being` |
|       - | 7521 | ` *   the first, and the key/index second.` |
|       - | 7522 | ` * Note:` |
|       - | 7523 | ` *  If funcname needs to be working with the actual values of the array, specify the first` |
|       - | 7524 | ` *  parameter of funcname as a reference. Then, any changes made to those elements will` |
|       - | 7525 | ` *  be made in the original array itself.` |
|       - | 7526 | ` *  $userdata` |
|       - | 7527 | ` *   If the optional userdata parameter is supplied, it will be passed as the third parameter` |
|       - | 7528 | ` *   to the callback funcname.` |
|       - | 7529 | ` * Return` |
|       - | 7530 | ` *  Returns TRUE on success or FALSE on failure.` |
|       - | 7531 | ` */` |
|      38 | 7532 | `static int ph7_hashmap_walk(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       5 | 7533 | `{` |
|       - | 7534 | `	ph7_value *pValue,*pUserData,sKey;` |
|       - | 7535 | `	ph7_hashmap_node *pEntry;` |
|       - | 7536 | `	ph7_hashmap *pMap;` |
|       - | 7537 | `	sxu32 n;` |
|      43 | 7538 | `	if( nArg < 2 ){` |
|       8 | 7539 | `		return PH7_VmThrowException(pCtx,` |
|       - | 7540 | `			"ArgumentCountError",` |
|       - | 7541 | `			"array_walk() expects at least 2 arguments, %d given",` |
|       2 | 7542 | `			nArg` |
|       - | 7543 | `			);` |
|       - | 7544 | `	}` |
|      39 | 7545 | `	if( nArg > 3 ){` |
|       4 | 7546 | `		return PH7_VmThrowException(pCtx,` |
|       - | 7547 | `			"ArgumentCountError",` |
|       - | 7548 | `			"array_walk() expects at most 3 arguments, %d given",` |
|       1 | 7549 | `			nArg` |
|       - | 7550 | `			);` |
|       - | 7551 | `	}` |
|      37 | 7552 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|       4 | 7553 | `		return PH7_VmThrowException(pCtx,` |
|       - | 7554 | `			"TypeError",` |
|       - | 7555 | `			"array_walk(): Argument #1 ($array) must be of type array, %s given",` |
|       1 | 7556 | `			ph7_type_name(apArg[0])` |
|       - | 7557 | `			);` |
|       - | 7558 | `	}` |
|      35 | 7559 | `	if( !ph7_value_is_callable(apArg[1]) ){` |
|      14 | 7560 | `		if( ph7_value_is_string(apArg[1]) ){` |
|       3 | 7561 | `			const char *zFunc = ph7_value_to_string(apArg[1],0);` |
|       4 | 7562 | `			return PH7_VmThrowException(pCtx,` |
|       - | 7563 | `				"TypeError",` |
|       - | 7564 | `				"array_walk(): Argument #2 ($callback) must be a valid callback, "` |
|       - | 7565 | `				"function \"%s\" not found or invalid function name",` |
|       1 | 7566 | `				zFunc` |
|       - | 7567 | `				);` |
|       - | 7568 | `		}` |
|      12 | 7569 | `		if( ph7_value_is_array(apArg[1]) ){` |
|       6 | 7570 | `			return PH7_VmThrowException(pCtx,` |
|       - | 7571 | `				"TypeError",` |
|       - | 7572 | `				"array_walk(): Argument #2 ($callback) must be a valid callback, "` |
|       - | 7573 | `				"array callback must have exactly two members"` |
|       - | 7574 | `				);` |
|       - | 7575 | `		}` |
|       6 | 7576 | `		return PH7_VmThrowException(pCtx,` |
|       - | 7577 | `			"TypeError",` |
|       - | 7578 | `			"array_walk(): Argument #2 ($callback) must be a valid callback, "` |
|       - | 7579 | `			"no array or string given"` |
|       - | 7580 | `			);` |
|       - | 7581 | `	}` |
|      21 | 7582 | `	pUserData = nArg > 2 ? apArg[2] : 0;` |
|       - | 7583 | `	/* Point to the internal representation of the input hashmap */` |
|      21 | 7584 | `	PH7_HashmapCowSeparate(pCtx->pVm, apArg[0]);` |
|      21 | 7585 | `	pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|      21 | 7586 | `	PH7_MemObjInit(pMap->pVm,&sKey);` |
|      21 | 7587 | `	sKey.nIdx = SXU32_HIGH; /* Mark as constant */` |
|       - | 7588 | `	/* Perform the desired operation */` |
|      21 | 7589 | `	pEntry = pMap->pFirst;` |
|      61 | 7590 | `	for( n = 0 ; n < pMap->nEntry ; n++ ){` |
|       - | 7591 | `		/* Extract the node value */` |
|      43 | 7592 | `		pValue = HashmapExtractNodeValue(pEntry);` |
|      43 | 7593 | `		if( pValue ){` |
|       - | 7594 | `			sxi32 rcW;` |
|       - | 7595 | `			/* Extract the entry key */` |
|      43 | 7596 | `			PH7_HashmapExtractNodeKey(pEntry,&sKey);` |
|       - | 7597 | `			/* Invoke the supplied callback */` |
|      43 | 7598 | `			rcW = PH7_VmCallUserFunctionAp(pMap->pVm,apArg[1],0,pValue,&sKey,pUserData,0);` |
|      43 | 7599 | `			PH7_MemObjRelease(&sKey);` |
|      43 | 7600 | `			if( rcW == PH7_EXCEPTION ){` |
|       - | 7601 | `				/* The callback raised: propagate so the dispatcher unwinds. */` |
|       3 | 7602 | `				return PH7_EXCEPTION;` |
|       - | 7603 | `			}` |
|      20 | 7604 | `		}` |
|       - | 7605 | `		/* Point to the next entry */` |
|      41 | 7606 | `		pEntry = pEntry->pPrev; /* Reverse link */` |
|      21 | 7607 | `	}` |
|       - | 7608 | `	/* All done, return TRUE */` |
|      19 | 7609 | `	ph7_result_bool(pCtx,1);` |
|      19 | 7610 | `	return PH7_OK;` |
|      24 | 7611 | `}` |
|       - | 7612 | `/*` |
|       - | 7613 | ` * Apply a user function to every member of an array.(Recurse on array's).` |
|       - | 7614 | ` * Refer to the [array_walk_recursive()] implementation for more information.` |
|       - | 7615 | ` */` |
|      22 | 7616 | `static sxi32 HashmapWalkRecursive(` |
|       - | 7617 | `	ph7_hashmap *pMap,    /* Target hashmap */` |
|       - | 7618 | `	ph7_value *pCallback, /* User callback */` |
|       - | 7619 | `	ph7_value *pUserData, /* Callback private data */` |
|       - | 7620 | `	int iNest             /* Nesting level */` |
|       - | 7621 | `	)` |
|       1 | 7622 | `{` |
|       - | 7623 | `	ph7_hashmap_node *pEntry;` |
|       - | 7624 | `	ph7_value *pValue,sKey;` |
|       - | 7625 | `	sxi32 rc;` |
|       - | 7626 | `	sxu32 n;` |
|       - | 7627 | `	/* Iterate through hashmap entries */` |
|      23 | 7628 | `	PH7_MemObjInit(pMap->pVm,&sKey);` |
|      23 | 7629 | `	sKey.nIdx = SXU32_HIGH; /* Mark as constant */` |
|      23 | 7630 | `	pEntry = pMap->pFirst;` |
|      59 | 7631 | `	for( n = 0 ; n < pMap->nEntry ; n++ ){` |
|       - | 7632 | `		/* Extract the node value */` |
|      37 | 7633 | `		pValue = HashmapExtractNodeValue(pEntry);` |
|      37 | 7634 | `		if( pValue ){` |
|      37 | 7635 | `			if( pValue->iFlags & MEMOBJ_HASHMAP ){` |
|      11 | 7636 | `				if( iNest < 32 ){` |
|       - | 7637 | `					/* Recurse */` |
|      11 | 7638 | `					iNest++;` |
|      11 | 7639 | `					rc = HashmapWalkRecursive((ph7_hashmap *)pValue->x.pOther,pCallback,pUserData,iNest);` |
|      11 | 7640 | `					iNest--;` |
|      11 | 7641 | `					if( rc == PH7_EXCEPTION ){` |
|     ! 0 | 7642 | `						return PH7_EXCEPTION;` |
|       - | 7643 | `					}` |
|       5 | 7644 | `				}` |
|       6 | 7645 | `			}else{` |
|       - | 7646 | `				/* Extract the node key */` |
|      27 | 7647 | `				PH7_HashmapExtractNodeKey(pEntry,&sKey);` |
|       - | 7648 | `				/* Invoke the supplied callback */` |
|      27 | 7649 | `				rc = PH7_VmCallUserFunctionAp(pMap->pVm,pCallback,0,pValue,&sKey,pUserData,0);` |
|      27 | 7650 | `				PH7_MemObjRelease(&sKey);` |
|      27 | 7651 | `				if( rc == PH7_EXCEPTION ){` |
|       - | 7652 | `					/* The callback raised: propagate so the dispatcher unwinds. */` |
|     ! 0 | 7653 | `					return PH7_EXCEPTION;` |
|       - | 7654 | `				}` |
|       - | 7655 | `			}` |
|      18 | 7656 | `		}` |
|       - | 7657 | `		/* Point to the next entry */` |
|      37 | 7658 | `		pEntry = pEntry->pPrev; /* Reverse link */` |
|      19 | 7659 | `	}` |
|      23 | 7660 | `	return PH7_OK;` |
|      12 | 7661 | `}` |
|       - | 7662 | `/*` |
|       - | 7663 | ` * bool array_walk_recursive(array &$array, callback $funcname [, mixed $userdata])` |
|       - | 7664 | ` *  Apply a user function recursively to every member of an array.` |
|       - | 7665 | ` * Parameters` |
|       - | 7666 | ` *  $array` |
|       - | 7667 | ` *   The input array.` |
|       - | 7668 | ` *  $funcname` |
|       - | 7669 | ` *   Typically, funcname takes on two parameters. The array parameter's value being` |
|       - | 7670 | ` *   the first, and the key/index second.` |
|       - | 7671 | ` * Note:` |
|       - | 7672 | ` *  If funcname needs to be working with the actual values of the array, specify the first` |
|       - | 7673 | ` *  parameter of funcname as a reference. Then, any changes made to those elements will` |
|       - | 7674 | ` *  be made in the original array itself.` |
|       - | 7675 | ` *  $userdata` |
|       - | 7676 | ` *   If the optional userdata parameter is supplied, it will be passed as the third parameter` |
|       - | 7677 | ` *   to the callback funcname.` |
|       - | 7678 | ` * Return` |
|       - | 7679 | ` *  Returns TRUE on success or FALSE on failure.` |
|       - | 7680 | ` */` |
|      30 | 7681 | `static int ph7_hashmap_walk_recursive(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       5 | 7682 | `{` |
|       - | 7683 | `	ph7_hashmap *pMap;` |
|      35 | 7684 | `	if( nArg < 2 ){` |
|       8 | 7685 | `		return PH7_VmThrowException(pCtx,` |
|       - | 7686 | `			"ArgumentCountError",` |
|       - | 7687 | `			"array_walk_recursive() expects at least 2 arguments, %d given",` |
|       2 | 7688 | `			nArg` |
|       - | 7689 | `			);` |
|       - | 7690 | `	}` |
|      31 | 7691 | `	if( nArg > 3 ){` |
|       4 | 7692 | `		return PH7_VmThrowException(pCtx,` |
|       - | 7693 | `			"ArgumentCountError",` |
|       - | 7694 | `			"array_walk_recursive() expects at most 3 arguments, %d given",` |
|       1 | 7695 | `			nArg` |
|       - | 7696 | `			);` |
|       - | 7697 | `	}` |
|      29 | 7698 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|       4 | 7699 | `		return PH7_VmThrowException(pCtx,` |
|       - | 7700 | `			"TypeError",` |
|       - | 7701 | `			"array_walk_recursive(): Argument #1 ($array) must be of type array, %s given",` |
|       1 | 7702 | `			ph7_type_name(apArg[0])` |
|       - | 7703 | `			);` |
|       - | 7704 | `	}` |
|      27 | 7705 | `	if( !ph7_value_is_callable(apArg[1]) ){` |
|      14 | 7706 | `		if( ph7_value_is_string(apArg[1]) ){` |
|       3 | 7707 | `			const char *zFunc = ph7_value_to_string(apArg[1],0);` |
|       4 | 7708 | `			return PH7_VmThrowException(pCtx,` |
|       - | 7709 | `				"TypeError",` |
|       - | 7710 | `				"array_walk_recursive(): Argument #2 ($callback) must be a valid callback, "` |
|       - | 7711 | `				"function \"%s\" not found or invalid function name",` |
|       1 | 7712 | `				zFunc` |
|       - | 7713 | `				);` |
|       - | 7714 | `		}` |
|      12 | 7715 | `		if( ph7_value_is_array(apArg[1]) ){` |
|       6 | 7716 | `			return PH7_VmThrowException(pCtx,` |
|       - | 7717 | `				"TypeError",` |
|       - | 7718 | `				"array_walk_recursive(): Argument #2 ($callback) must be a valid callback, "` |
|       - | 7719 | `				"array callback must have exactly two members"` |
|       - | 7720 | `				);` |
|       - | 7721 | `		}` |
|       6 | 7722 | `		return PH7_VmThrowException(pCtx,` |
|       - | 7723 | `			"TypeError",` |
|       - | 7724 | `			"array_walk_recursive(): Argument #2 ($callback) must be a valid callback, "` |
|       - | 7725 | `			"no array or string given"` |
|       - | 7726 | `			);` |
|       - | 7727 | `	}` |
|       - | 7728 | `	/* Point to the internal representation of the input hashmap */` |
|      13 | 7729 | `	PH7_HashmapCowSeparate(pCtx->pVm, apArg[0]);` |
|      13 | 7730 | `	pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|       - | 7731 | `	/* Perform the desired operation */` |
|      13 | 7732 | `	if( HashmapWalkRecursive(pMap,apArg[1],nArg > 2 ? apArg[2] : 0,0) == PH7_EXCEPTION ){` |
|       - | 7733 | `		/* A callback raised: propagate so the dispatcher unwinds. */` |
|     ! 0 | 7734 | `		return PH7_EXCEPTION;` |
|       - | 7735 | `	}` |
|       - | 7736 | `	/* All done, return TRUE */` |
|      13 | 7737 | `	ph7_result_bool(pCtx,1);` |
|      13 | 7738 | `	return PH7_OK;` |
|      20 | 7739 | `}` |
|       - | 7740 | `/*` |
|       - | 7741 | ` * bool array_is_list(array $array)` |
|       - | 7742 | ` *  Checks whether a given array is a list: its keys consist of consecutive` |
|       - | 7743 | ` *  integers starting at 0. An empty array is a list.` |
|       - | 7744 | ` * Return` |
|       - | 7745 | ` *  TRUE if the array is a list, FALSE otherwise.` |
|       - | 7746 | ` */` |
|       - | 7747 | `/*` |
|       - | 7748 | ` * Return TRUE if the given hashmap is a "list" [i.e: its keys are the` |
|       - | 7749 | ` * consecutive integers 0,1,2,... with no gaps]. An empty map is a list.` |
|       - | 7750 | ` * Shared by array_is_list() and the JSON encoder (vm_json.c).` |
|       - | 7751 | ` */` |
|     118 | 7752 | `PH7_PRIVATE int PH7_HashmapIsList(ph7_hashmap *pMap)` |
|       1 | 7753 | `{` |
|     119 | 7754 | `	ph7_hashmap_node *pNode = pMap->pFirst;` |
|     119 | 7755 | `	sxi64 iExpect = 0;` |
|       - | 7756 | `	sxu32 n;` |
|     253 | 7757 | `	for( n = 0 ; n < pMap->nEntry ; ++n ){` |
|     187 | 7758 | `		if( pNode->iType != HASHMAP_INT_NODE \|\| pNode->xKey.iKey != iExpect ){` |
|       - | 7759 | `			/* A non-integer key or a gap in the sequence: not a list */` |
|      53 | 7760 | `			return 0;` |
|       - | 7761 | `		}` |
|     135 | 7762 | `		++iExpect;` |
|     135 | 7763 | `		pNode = pNode->pPrev; /* Reverse link */` |
|      68 | 7764 | `	}` |
|      67 | 7765 | `	return 1;` |
|      60 | 7766 | `}` |
|      12 | 7767 | `static int ph7_hashmap_is_list(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       1 | 7768 | `{` |
|      13 | 7769 | `	if( nArg < 1 ){` |
|     ! 0 | 7770 | `		return PH7_VmThrowException(pCtx,` |
|       - | 7771 | `			"ArgumentCountError",` |
|       - | 7772 | `			"array_is_list() expects exactly 1 argument, 0 given"` |
|       - | 7773 | `			);` |
|       - | 7774 | `	}` |
|      13 | 7775 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|     ! 0 | 7776 | `		return PH7_VmThrowException(pCtx,` |
|       - | 7777 | `			"TypeError",` |
|       - | 7778 | `			"array_is_list(): Argument #1 ($array) must be of type array, %s given",` |
|     ! 0 | 7779 | `			ph7_type_name(apArg[0])` |
|       - | 7780 | `			);` |
|       - | 7781 | `	}` |
|      13 | 7782 | `	ph7_result_bool(pCtx,PH7_HashmapIsList((ph7_hashmap *)apArg[0]->x.pOther));` |
|      13 | 7783 | `	return PH7_OK;` |
|       7 | 7784 | `}` |
|       - | 7785 | `/*` |
|       - | 7786 | ` * mixed array_first(array $array)` |
|       - | 7787 | ` * mixed array_last(array $array)` |
|       - | 7788 | ` *  Return the value of the first (respectively last) element of the array,` |
|       - | 7789 | ` *  or NULL when the array is empty. The internal array pointer is left` |
|       - | 7790 | ` *  untouched (unlike reset()/end()).` |
|       - | 7791 | ` */` |
|      20 | 7792 | `static int HashmapFirstLast(ph7_context *pCtx,int nArg,ph7_value **apArg,int bLast)` |
|       1 | 7793 | `{` |
|       - | 7794 | `	ph7_hashmap *pMap;` |
|       - | 7795 | `	ph7_hashmap_node *pNode;` |
|       - | 7796 | `	ph7_value *pVal;` |
|      21 | 7797 | `	const char *zName = bLast ? "array_last" : "array_first";` |
|      21 | 7798 | `	if( nArg < 1 ){` |
|       4 | 7799 | `		return PH7_VmThrowException(pCtx,` |
|       - | 7800 | `			"ArgumentCountError",` |
|       - | 7801 | `			"%s() expects exactly 1 argument, 0 given",` |
|       1 | 7802 | `			zName` |
|       - | 7803 | `			);` |
|       - | 7804 | `	}` |
|      19 | 7805 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|       4 | 7806 | `		return PH7_VmThrowException(pCtx,` |
|       - | 7807 | `			"TypeError",` |
|       - | 7808 | `			"%s(): Argument #1 ($array) must be of type array, %s given",` |
|       1 | 7809 | `			zName,` |
|       1 | 7810 | `			ph7_type_name(apArg[0])` |
|       - | 7811 | `			);` |
|       - | 7812 | `	}` |
|      17 | 7813 | `	pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|      17 | 7814 | `	pNode = bLast ? pMap->pLast : pMap->pFirst;` |
|      17 | 7815 | `	if( pNode == 0 ){` |
|       - | 7816 | `		/* Empty array: PHP returns NULL */` |
|       5 | 7817 | `		ph7_result_null(pCtx);` |
|       5 | 7818 | `		return PH7_OK;` |
|       - | 7819 | `	}` |
|      13 | 7820 | `	pVal = HashmapExtractNodeValue(pNode);` |
|      13 | 7821 | `	if( pVal ){` |
|      13 | 7822 | `		ph7_result_value(pCtx,pVal);` |
|       7 | 7823 | `	}else{` |
|     ! 0 | 7824 | `		ph7_result_null(pCtx);` |
|       - | 7825 | `	}` |
|      13 | 7826 | `	return PH7_OK;` |
|      11 | 7827 | `}` |
|      10 | 7828 | `static int ph7_hashmap_first(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       1 | 7829 | `{` |
|      11 | 7830 | `	return HashmapFirstLast(pCtx,nArg,apArg,0);` |
|       1 | 7831 | `}` |
|      10 | 7832 | `static int ph7_hashmap_last(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       1 | 7833 | `{` |
|      11 | 7834 | `	return HashmapFirstLast(pCtx,nArg,apArg,1);` |
|       1 | 7835 | `}` |
|       - | 7836 | `/*` |
|       - | 7837 | ` * Fetch the element identified by 'pKey' from 'pRow' which may be either an` |
|       - | 7838 | ` * array (hashmap lookup) or an object (public attribute lookup). Used by` |
|       - | 7839 | ` * array_column() for both the column value and the index key.` |
|       - | 7840 | ` * Returns a borrowed pointer to the value, or NULL when the row is not a` |
|       - | 7841 | ` * container or the key is absent.` |
|       - | 7842 | ` */` |
|      32 | 7843 | `static ph7_value * HashmapColumnFetch(ph7_vm *pVm,ph7_value *pRow,ph7_value *pKey)` |
|       1 | 7844 | `{` |
|      33 | 7845 | `	if( ph7_value_is_array(pRow) ){` |
|       - | 7846 | `		ph7_hashmap_node *pNode;` |
|      25 | 7847 | `		if( PH7_HashmapLookup((ph7_hashmap *)pRow->x.pOther,pKey,&pNode) == SXRET_OK ){` |
|      21 | 7848 | `			return HashmapExtractNodeValue(pNode);` |
|       1 | 7849 | `		}` |
|      11 | 7850 | `	}else if( ph7_value_is_object(pRow) ){` |
|       - | 7851 | `		ph7_value sName;` |
|       - | 7852 | `		const char *zName;` |
|       - | 7853 | `		ph7_value *pAttr;` |
|       - | 7854 | `		/* Stringify a *copy* of the key (objects address attributes by name);` |
|       - | 7855 | `		 * never mutate pKey itself or the array-lookup path would break. */` |
|       9 | 7856 | `		PH7_MemObjInit(pVm,&sName);` |
|       9 | 7857 | `		PH7_MemObjStore(pKey,&sName);` |
|       9 | 7858 | `		zName = ph7_value_to_string(&sName,0); /* NUL-terminated */` |
|       9 | 7859 | `		pAttr = ph7_object_fetch_attr(pRow,zName);` |
|       9 | 7860 | `		PH7_MemObjRelease(&sName);` |
|       9 | 7861 | `		return pAttr;` |
|       - | 7862 | `	}` |
|       5 | 7863 | `	return 0;` |
|      17 | 7864 | `}` |
|       - | 7865 | `/*` |
|       - | 7866 | ` * array array_column(array $array, int\|string\|null $column_key, int\|string\|null $index_key = null)` |
|       - | 7867 | ` *  Returns the values from a single column of the input, identified by` |
|       - | 7868 | ` *  $column_key. Optionally indexes the result by the $index_key column.` |
|       - | 7869 | ` *  A NULL $column_key collects the whole row. Rows missing the column are` |
|       - | 7870 | ` *  skipped; rows missing the index key are appended with a numeric key.` |
|       - | 7871 | ` *  Each row may be an array or an object.` |
|       - | 7872 | ` */` |
|      12 | 7873 | `static int ph7_hashmap_column(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       1 | 7874 | `{` |
|       - | 7875 | `	ph7_hashmap_node *pNode;` |
|       - | 7876 | `	ph7_hashmap *pMap;` |
|       - | 7877 | `	ph7_value *pArray;` |
|       - | 7878 | `	ph7_value *pRow;` |
|       - | 7879 | `	ph7_value *pCol;` |
|       - | 7880 | `	ph7_value *pIdx;` |
|       - | 7881 | `	int bWantCol;` |
|       - | 7882 | `	int bWantIdx;` |
|       - | 7883 | `	sxu32 n;` |
|      13 | 7884 | `	if( nArg < 2 ){` |
|     ! 0 | 7885 | `		return PH7_VmThrowException(pCtx,` |
|       - | 7886 | `			"ArgumentCountError",` |
|       - | 7887 | `			"array_column() expects at least 2 arguments, %d given",` |
|     ! 0 | 7888 | `			nArg` |
|       - | 7889 | `			);` |
|       - | 7890 | `	}` |
|      13 | 7891 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|     ! 0 | 7892 | `		return PH7_VmThrowException(pCtx,` |
|       - | 7893 | `			"TypeError",` |
|       - | 7894 | `			"array_column(): Argument #1 ($array) must be of type array, %s given",` |
|     ! 0 | 7895 | `			ph7_type_name(apArg[0])` |
|       - | 7896 | `			);` |
|       - | 7897 | `	}` |
|      13 | 7898 | `	pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|      13 | 7899 | `	pArray = ph7_context_new_array(pCtx);` |
|      13 | 7900 | `	if( pArray == 0 ){` |
|     ! 0 | 7901 | `		ph7_result_null(pCtx);` |
|     ! 0 | 7902 | `		return PH7_OK;` |
|       - | 7903 | `	}` |
|       - | 7904 | `	/* A NULL column_key means "collect the entire row". */` |
|      13 | 7905 | `	bWantCol = !ph7_value_is_null(apArg[1]);` |
|      13 | 7906 | `	bWantIdx = (nArg > 2 && !ph7_value_is_null(apArg[2]));` |
|      13 | 7907 | `	pNode = pMap->pFirst;` |
|      33 | 7908 | `	for( n = 0 ; n < pMap->nEntry ; ++n ){` |
|      21 | 7909 | `		pRow = HashmapExtractNodeValue(pNode);` |
|      21 | 7910 | `		pNode = pNode->pPrev; /* Advance now so 'continue' is safe */` |
|      21 | 7911 | `		if( pRow == 0 ){` |
|     ! 0 | 7912 | `			continue;` |
|       - | 7913 | `		}` |
|      21 | 7914 | `		if( bWantCol ){` |
|      19 | 7915 | `			pCol = HashmapColumnFetch(pMap->pVm,pRow,apArg[1]);` |
|      19 | 7916 | `			if( pCol == 0 ){` |
|       - | 7917 | `				/* Row lacks the requested column: skip it (PHP semantics). */` |
|       3 | 7918 | `				continue;` |
|       - | 7919 | `			}` |
|       9 | 7920 | `		}else{` |
|       3 | 7921 | `			pCol = pRow;` |
|       - | 7922 | `		}` |
|      19 | 7923 | `		pIdx = bWantIdx ? HashmapColumnFetch(pMap->pVm,pRow,apArg[2]) : 0;` |
|      19 | 7924 | `		if( pIdx ){` |
|      13 | 7925 | `			ph7_array_add_elem(pArray,pIdx,pCol);` |
|       7 | 7926 | `		}else{` |
|       7 | 7927 | `			ph7_array_add_elem(pArray,0,pCol); /* Auto-index */` |
|       - | 7928 | `		}` |
|      10 | 7929 | `	}` |
|      13 | 7930 | `	ph7_result_value(pCtx,pArray);` |
|      13 | 7931 | `	return PH7_OK;` |
|       7 | 7932 | `}` |
|       - | 7933 | `/*` |
|       - | 7934 | ` * Shared core for array_find/array_find_key/array_any/array_all (PHP 8.4).` |
|       - | 7935 | ` * Invokes $callback($value, $key) over each entry and reports the first node` |
|       - | 7936 | ` * whose truthiness equals 'bWant'. Propagates a callback exception as` |
|       - | 7937 | ` * PH7_EXCEPTION; sets *ppMatch to the matching node (or NULL if none).` |
|       - | 7938 | ` */` |
|      28 | 7939 | `static sxi32 HashmapCallbackSearch(` |
|       - | 7940 | `	ph7_context *pCtx,int nArg,ph7_value **apArg,` |
|       - | 7941 | `	const char *zName,            /* Function name for diagnostics */` |
|       - | 7942 | `	int bWant,                    /* Truthiness being hunted for */` |
|       - | 7943 | `	ph7_hashmap_node **ppMatch    /* OUT: first matching node or NULL */` |
|       - | 7944 | `	)` |
|       1 | 7945 | `{` |
|       - | 7946 | `	ph7_hashmap_node *pEntry;` |
|       - | 7947 | `	ph7_hashmap *pMap;` |
|       - | 7948 | `	ph7_value *pValue;` |
|       - | 7949 | `	ph7_value *apCbArg[2];` |
|       - | 7950 | `	ph7_value sKey;` |
|       - | 7951 | `	ph7_value sResult;` |
|       - | 7952 | `	sxi32 rc;` |
|       - | 7953 | `	sxu32 n;` |
|      29 | 7954 | `	*ppMatch = 0;` |
|      29 | 7955 | `	if( nArg < 2 ){` |
|     ! 0 | 7956 | `		return PH7_VmThrowException(pCtx,` |
|       - | 7957 | `			"ArgumentCountError",` |
|       - | 7958 | `			"%s() expects exactly 2 arguments, %d given",` |
|     ! 0 | 7959 | `			zName,nArg` |
|       - | 7960 | `			);` |
|       - | 7961 | `	}` |
|      29 | 7962 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|     ! 0 | 7963 | `		return PH7_VmThrowException(pCtx,` |
|       - | 7964 | `			"TypeError",` |
|       - | 7965 | `			"%s(): Argument #1 ($array) must be of type array, %s given",` |
|     ! 0 | 7966 | `			zName,ph7_type_name(apArg[0])` |
|       - | 7967 | `			);` |
|       - | 7968 | `	}` |
|      29 | 7969 | `	if( !ph7_value_is_callable(apArg[1]) ){` |
|     ! 0 | 7970 | `		return PH7_VmThrowException(pCtx,` |
|       - | 7971 | `			"TypeError",` |
|       - | 7972 | `			"%s(): Argument #2 ($callback) must be a valid callback, %s given",` |
|     ! 0 | 7973 | `			zName,ph7_type_name(apArg[1])` |
|       - | 7974 | `			);` |
|       - | 7975 | `	}` |
|      29 | 7976 | `	pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|      29 | 7977 | `	pEntry = pMap->pFirst;` |
|      29 | 7978 | `	PH7_MemObjInit(pMap->pVm,&sKey);` |
|      29 | 7979 | `	sKey.nIdx = SXU32_HIGH;    /* Mark as constant */` |
|      29 | 7980 | `	PH7_MemObjInit(pMap->pVm,&sResult);` |
|      29 | 7981 | `	sResult.nIdx = SXU32_HIGH; /* Mark as constant */` |
|      73 | 7982 | `	for( n = 0 ; n < pMap->nEntry ; ++n ){` |
|      59 | 7983 | `		pValue = HashmapExtractNodeValue(pEntry);` |
|      59 | 7984 | `		if( pValue ){` |
|       - | 7985 | `			/* The callback receives ($value, $key). */` |
|      59 | 7986 | `			PH7_HashmapExtractNodeKey(pEntry,&sKey);` |
|      59 | 7987 | `			apCbArg[0] = pValue;` |
|      59 | 7988 | `			apCbArg[1] = &sKey;` |
|      59 | 7989 | `			rc = PH7_VmCallUserFunction(pMap->pVm,apArg[1],2,apCbArg,&sResult);` |
|      59 | 7990 | `			if( rc == PH7_EXCEPTION ){` |
|       - | 7991 | `				/* The callback raised: propagate so the dispatcher unwinds. */` |
|     ! 0 | 7992 | `				PH7_MemObjRelease(&sKey);` |
|     ! 0 | 7993 | `				PH7_MemObjRelease(&sResult);` |
|     ! 0 | 7994 | `				return PH7_EXCEPTION;` |
|       - | 7995 | `			}` |
|      59 | 7996 | `			if( rc == SXRET_OK && (ph7_value_to_bool(&sResult) ? 1 : 0) == bWant ){` |
|      15 | 7997 | `				*ppMatch = pEntry;` |
|      15 | 7998 | `				break;` |
|       - | 7999 | `			}` |
|      22 | 8000 | `		}` |
|      45 | 8001 | `		pEntry = pEntry->pPrev; /* Reverse link */` |
|      23 | 8002 | `	}` |
|      29 | 8003 | `	PH7_MemObjRelease(&sKey);` |
|      29 | 8004 | `	PH7_MemObjRelease(&sResult);` |
|      29 | 8005 | `	return PH7_OK;` |
|      15 | 8006 | `}` |
|       - | 8007 | `/*` |
|       - | 8008 | ` * mixed array_find(array $array, callable $callback)` |
|       - | 8009 | ` *  Returns the value of the first element for which $callback($value,$key)` |
|       - | 8010 | ` *  is truthy, or NULL if none match.` |
|       - | 8011 | ` */` |
|       6 | 8012 | `static int ph7_hashmap_find(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       1 | 8013 | `{` |
|       - | 8014 | `	ph7_hashmap_node *pMatch;` |
|       - | 8015 | `	ph7_value *pVal;` |
|       - | 8016 | `	sxi32 rc;` |
|       7 | 8017 | `	rc = HashmapCallbackSearch(pCtx,nArg,apArg,"array_find",1,&pMatch);` |
|       7 | 8018 | `	if( rc != PH7_OK ){` |
|     ! 0 | 8019 | `		return rc;` |
|       - | 8020 | `	}` |
|       7 | 8021 | `	if( pMatch && (pVal = HashmapExtractNodeValue(pMatch)) != 0 ){` |
|       5 | 8022 | `		ph7_result_value(pCtx,pVal);` |
|       3 | 8023 | `	}else{` |
|       3 | 8024 | `		ph7_result_null(pCtx);` |
|       - | 8025 | `	}` |
|       7 | 8026 | `	return PH7_OK;` |
|       4 | 8027 | `}` |
|       - | 8028 | `/*` |
|       - | 8029 | ` * mixed array_find_key(array $array, callable $callback)` |
|       - | 8030 | ` *  Returns the key of the first element for which $callback($value,$key)` |
|       - | 8031 | ` *  is truthy, or NULL if none match.` |
|       - | 8032 | ` */` |
|       6 | 8033 | `static int ph7_hashmap_find_key(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       1 | 8034 | `{` |
|       - | 8035 | `	ph7_hashmap_node *pMatch;` |
|       - | 8036 | `	sxi32 rc;` |
|       7 | 8037 | `	rc = HashmapCallbackSearch(pCtx,nArg,apArg,"array_find_key",1,&pMatch);` |
|       7 | 8038 | `	if( rc != PH7_OK ){` |
|     ! 0 | 8039 | `		return rc;` |
|       - | 8040 | `	}` |
|       7 | 8041 | `	if( pMatch == 0 ){` |
|       3 | 8042 | `		ph7_result_null(pCtx);` |
|       6 | 8043 | `	}else if( pMatch->iType == HASHMAP_INT_NODE ){` |
|       3 | 8044 | `		ph7_result_int64(pCtx,pMatch->xKey.iKey);` |
|       2 | 8045 | `	}else{` |
|       4 | 8046 | `		ph7_result_string(pCtx,` |
|       2 | 8047 | `			(const char *)SyBlobData(&pMatch->xKey.sKey),` |
|       2 | 8048 | `			(int)SyBlobLength(&pMatch->xKey.sKey));` |
|       - | 8049 | `	}` |
|       7 | 8050 | `	return PH7_OK;` |
|       4 | 8051 | `}` |
|       - | 8052 | `/*` |
|       - | 8053 | ` * bool array_any(array $array, callable $callback)` |
|       - | 8054 | ` *  Returns TRUE if $callback($value,$key) is truthy for at least one element.` |
|       - | 8055 | ` *  FALSE for an empty array.` |
|       - | 8056 | ` */` |
|       8 | 8057 | `static int ph7_hashmap_any(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       1 | 8058 | `{` |
|       - | 8059 | `	ph7_hashmap_node *pMatch;` |
|       - | 8060 | `	sxi32 rc;` |
|       9 | 8061 | `	rc = HashmapCallbackSearch(pCtx,nArg,apArg,"array_any",1,&pMatch);` |
|       9 | 8062 | `	if( rc != PH7_OK ){` |
|     ! 0 | 8063 | `		return rc;` |
|       - | 8064 | `	}` |
|       9 | 8065 | `	ph7_result_bool(pCtx,pMatch != 0);` |
|       9 | 8066 | `	return PH7_OK;` |
|       5 | 8067 | `}` |
|       - | 8068 | `/*` |
|       - | 8069 | ` * bool array_all(array $array, callable $callback)` |
|       - | 8070 | ` *  Returns TRUE if $callback($value,$key) is truthy for every element (and for` |
|       - | 8071 | ` *  an empty array). Hunts for the first falsy element: its absence means "all".` |
|       - | 8072 | ` */` |
|       8 | 8073 | `static int ph7_hashmap_all(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       1 | 8074 | `{` |
|       - | 8075 | `	ph7_hashmap_node *pMatch;` |
|       - | 8076 | `	sxi32 rc;` |
|       9 | 8077 | `	rc = HashmapCallbackSearch(pCtx,nArg,apArg,"array_all",0,&pMatch);` |
|       9 | 8078 | `	if( rc != PH7_OK ){` |
|     ! 0 | 8079 | `		return rc;` |
|       - | 8080 | `	}` |
|       9 | 8081 | `	ph7_result_bool(pCtx,pMatch == 0);` |
|       9 | 8082 | `	return PH7_OK;` |
|       5 | 8083 | `}` |
|       - | 8084 | `/*` |
|       - | 8085 | ` * The iterator_*() family — walk a Traversable via the shared PH7_VmIteratorWalk` |
|       - | 8086 | ` * helper (the reusable form of the foreach Iterator protocol).` |
|       - | 8087 | ` */` |
|       - | 8088 | `/* Step shared by iterator_to_array (pArray set) and iterator_count (pArray NULL). */` |
|       - | 8089 | `struct IterCollect { ph7_value *pArray; int bPreserve; sxi64 nCount; };` |
|      70 | 8090 | `static sxi32 IterCollectStep(ph7_vm *pVm, ph7_value *pKey, ph7_value *pValue, void *pUserData)` |
|       4 | 8091 | `{` |
|      74 | 8092 | `	struct IterCollect *p = (struct IterCollect *)pUserData;` |
|      35 | 8093 | `	(void)pVm;` |
|      74 | 8094 | `	p->nCount++;` |
|      74 | 8095 | `	if( p->pArray ){` |
|       - | 8096 | `		/* preserve_keys: insert with the iterator key (later wins on collision);` |
|       - | 8097 | `		 * otherwise append with an auto-assigned int index. */` |
|      66 | 8098 | `		ph7_array_add_elem(p->pArray, p->bPreserve ? pKey : 0, pValue);` |
|      31 | 8099 | `	}` |
|      74 | 8100 | `	return SXRET_OK;` |
|       4 | 8101 | `}` |
|       - | 8102 | `/*` |
|       - | 8103 | ` * array iterator_to_array(Traversable\|array $iterator, bool $preserve_keys = true)` |
|       - | 8104 | ` */` |
|      26 | 8105 | `static int ph7_iterator_to_array(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|       4 | 8106 | `{` |
|       - | 8107 | `	struct IterCollect sCol;` |
|       - | 8108 | `	ph7_value *pArray;` |
|       - | 8109 | `	sxi32 rc;` |
|      30 | 8110 | `	if( nArg < 1 ){ ph7_result_null(pCtx); return PH7_OK; }` |
|      30 | 8111 | `	pArray = ph7_context_new_array(pCtx);` |
|      30 | 8112 | `	if( pArray == 0 ){ ph7_result_null(pCtx); return PH7_OK; }` |
|      30 | 8113 | `	sCol.pArray = pArray;` |
|      30 | 8114 | `	sCol.bPreserve = (nArg > 1) ? ph7_value_to_bool(apArg[1]) : 1;` |
|      30 | 8115 | `	sCol.nCount = 0;` |
|      30 | 8116 | `	if( ph7_value_is_array(apArg[0]) ){` |
|       - | 8117 | `		/* PHP 8.2 accepts a plain array: copy it (preserving or renumbering keys). */` |
|       3 | 8118 | `		ph7_hashmap *pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|       3 | 8119 | `		ph7_hashmap_node *pEntry = pMap->pFirst;` |
|       - | 8120 | `		sxu32 n;` |
|       9 | 8121 | `		for( n = 0 ; n < pMap->nEntry ; n++ ){` |
|       - | 8122 | `			ph7_value sKey, *pVal;` |
|       7 | 8123 | `			PH7_MemObjInit(pCtx->pVm,&sKey);` |
|       7 | 8124 | `			PH7_HashmapExtractNodeKey(pEntry,&sKey);` |
|       7 | 8125 | `			pVal = (ph7_value *)SySetAt(&pCtx->pVm->aMemObj,pEntry->nValIdx);` |
|       7 | 8126 | `			if( pVal ){ ph7_array_add_elem(pArray, sCol.bPreserve ? &sKey : 0, pVal); }` |
|       7 | 8127 | `			PH7_MemObjRelease(&sKey);` |
|       7 | 8128 | `			pEntry = pEntry->pPrev;` |
|       4 | 8129 | `		}` |
|       3 | 8130 | `		ph7_result_value(pCtx,pArray);` |
|       3 | 8131 | `		return PH7_OK;` |
|       - | 8132 | `	}` |
|      28 | 8133 | `	rc = PH7_VmIteratorWalk(pCtx->pVm, apArg[0], IterCollectStep, &sCol);` |
|      28 | 8134 | `	if( rc == PH7_EXCEPTION \|\| rc == PH7_ABORT ){ return rc; }` |
|      26 | 8135 | `	if( rc == SXERR_NOTIMPLEMENTED ){` |
|     ! 0 | 8136 | `		return PH7_VmThrowException(pCtx,"TypeError",` |
|       - | 8137 | `			"iterator_to_array(): Argument #1 ($iterator) must be of type Traversable\|array, %s given",` |
|     ! 0 | 8138 | `			ph7_type_name(apArg[0]));` |
|       - | 8139 | `	}` |
|      26 | 8140 | `	ph7_result_value(pCtx,pArray);` |
|      26 | 8141 | `	return PH7_OK;` |
|      17 | 8142 | `}` |
|       - | 8143 | `/*` |
|       - | 8144 | ` * int iterator_count(Traversable\|array $iterator)` |
|       - | 8145 | ` */` |
|       6 | 8146 | `static int ph7_iterator_count(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|       1 | 8147 | `{` |
|       - | 8148 | `	struct IterCollect sCol;` |
|       - | 8149 | `	sxi32 rc;` |
|       7 | 8150 | `	if( nArg < 1 ){ ph7_result_int(pCtx,0); return PH7_OK; }` |
|       7 | 8151 | `	if( ph7_value_is_array(apArg[0]) ){` |
|       3 | 8152 | `		ph7_result_int64(pCtx, (ph7_int64)((ph7_hashmap *)apArg[0]->x.pOther)->nEntry);` |
|       3 | 8153 | `		return PH7_OK;` |
|       - | 8154 | `	}` |
|       5 | 8155 | `	sCol.pArray = 0; sCol.bPreserve = 0; sCol.nCount = 0;` |
|       5 | 8156 | `	rc = PH7_VmIteratorWalk(pCtx->pVm, apArg[0], IterCollectStep, &sCol);` |
|       5 | 8157 | `	if( rc == PH7_EXCEPTION \|\| rc == PH7_ABORT ){ return rc; }` |
|       5 | 8158 | `	if( rc == SXERR_NOTIMPLEMENTED ){` |
|     ! 0 | 8159 | `		return PH7_VmThrowException(pCtx,"TypeError",` |
|       - | 8160 | `			"iterator_count(): Argument #1 ($iterator) must be of type Traversable\|array, %s given",` |
|     ! 0 | 8161 | `			ph7_type_name(apArg[0]));` |
|       - | 8162 | `	}` |
|       5 | 8163 | `	ph7_result_int64(pCtx, sCol.nCount);` |
|       5 | 8164 | `	return PH7_OK;` |
|       4 | 8165 | `}` |
|       - | 8166 | `/* iterator_apply step: call the fixed callback with $args each iteration. The` |
|       - | 8167 | ` * arg pointers are resolved fresh per step because the iterator's own methods` |
|       - | 8168 | ` * run user code between iterations and may reallocate the aMemObj pool. */` |
|       - | 8169 | `struct IterApply { ph7_value *pCallback; ph7_value *pArgsArray; sxi64 nCount; };` |
|      24 | 8170 | `static sxi32 IterApplyStep(ph7_vm *pVm, ph7_value *pKey, ph7_value *pValue, void *pUserData)` |
|       1 | 8171 | `{` |
|      25 | 8172 | `	struct IterApply *p = (struct IterApply *)pUserData;` |
|       - | 8173 | `	ph7_value sResult;` |
|       - | 8174 | `	SySet aArg;` |
|       - | 8175 | `	sxi32 rc;` |
|       - | 8176 | `	int bContinue;` |
|      12 | 8177 | `	(void)pKey; (void)pValue; /* iterator_apply does NOT pass the element to the callback */` |
|      25 | 8178 | `	SySetInit(&aArg,&pVm->sAllocator,sizeof(ph7_value *));` |
|      25 | 8179 | `	if( p->pArgsArray && (p->pArgsArray->iFlags & MEMOBJ_HASHMAP) ){` |
|       9 | 8180 | `		ph7_hashmap *pMap = (ph7_hashmap *)p->pArgsArray->x.pOther;` |
|       9 | 8181 | `		ph7_hashmap_node *pEntry = pMap->pFirst;` |
|       - | 8182 | `		sxu32 n;` |
|      17 | 8183 | `		for( n = 0 ; n < pMap->nEntry ; n++ ){` |
|       9 | 8184 | `			ph7_value *pVal = (ph7_value *)SySetAt(&pVm->aMemObj,pEntry->nValIdx);` |
|       9 | 8185 | `			if( pVal ){ SySetPut(&aArg,(const void *)&pVal); }` |
|       9 | 8186 | `			pEntry = pEntry->pPrev;` |
|       5 | 8187 | `		}` |
|       4 | 8188 | `	}` |
|      25 | 8189 | `	PH7_MemObjInit(pVm,&sResult);` |
|      37 | 8190 | `	rc = PH7_VmCallUserFunction(pVm, p->pCallback, (int)SySetUsed(&aArg),` |
|      24 | 8191 | `		(ph7_value **)SySetBasePtr(&aArg), &sResult);` |
|      25 | 8192 | `	SySetRelease(&aArg);` |
|      25 | 8193 | `	if( rc == PH7_EXCEPTION \|\| rc == PH7_ABORT ){ PH7_MemObjRelease(&sResult); return rc; }` |
|      23 | 8194 | `	p->nCount++;` |
|      23 | 8195 | `	PH7_MemObjToBool(&sResult);` |
|      23 | 8196 | `	bContinue = (sResult.x.iVal != 0);` |
|      23 | 8197 | `	PH7_MemObjRelease(&sResult);` |
|      23 | 8198 | `	return bContinue ? SXRET_OK : SXERR_EOF; /* falsy return stops iteration */` |
|      13 | 8199 | `}` |
|       - | 8200 | `/*` |
|       - | 8201 | ` * int iterator_apply(Traversable $iterator, callable $callback, array $args = [])` |
|       - | 8202 | ` */` |
|       8 | 8203 | `static int ph7_iterator_apply(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|       1 | 8204 | `{` |
|       - | 8205 | `	struct IterApply sApp;` |
|       - | 8206 | `	sxi32 rc;` |
|       9 | 8207 | `	if( nArg < 2 ){ ph7_result_int(pCtx,0); return PH7_OK; }` |
|       9 | 8208 | `	if( !ph7_value_is_callable(apArg[1]) ){` |
|     ! 0 | 8209 | `		return PH7_VmThrowException(pCtx,"TypeError",` |
|       - | 8210 | `			"iterator_apply(): Argument #2 ($callback) must be a valid callback");` |
|       - | 8211 | `	}` |
|       9 | 8212 | `	sApp.pCallback = apArg[1];` |
|       9 | 8213 | `	sApp.pArgsArray = (nArg > 2 && ph7_value_is_array(apArg[2])) ? apArg[2] : 0;` |
|       9 | 8214 | `	sApp.nCount = 0;` |
|       9 | 8215 | `	rc = PH7_VmIteratorWalk(pCtx->pVm, apArg[0], IterApplyStep, &sApp);` |
|       9 | 8216 | `	if( rc == PH7_EXCEPTION \|\| rc == PH7_ABORT ){ return rc; }` |
|       7 | 8217 | `	if( rc == SXERR_NOTIMPLEMENTED ){` |
|     ! 0 | 8218 | `		return PH7_VmThrowException(pCtx,"TypeError",` |
|       - | 8219 | `			"iterator_apply(): Argument #1 ($iterator) must be of type Traversable, %s given",` |
|     ! 0 | 8220 | `			ph7_type_name(apArg[0]));` |
|       - | 8221 | `	}` |
|       7 | 8222 | `	ph7_result_int64(pCtx, sApp.nCount);` |
|       7 | 8223 | `	return PH7_OK;` |
|       5 | 8224 | `}` |
|       - | 8225 | `/*` |
|       - | 8226 | ` * Table of hashmap functions.` |
|       - | 8227 | ` */` |
|       - | 8228 | `static const ph7_builtin_func aHashmapFunc[] = {` |
|       - | 8229 | `	{"iterator_to_array",  ph7_iterator_to_array },` |
|       - | 8230 | `	{"iterator_count",     ph7_iterator_count },` |
|       - | 8231 | `	{"iterator_apply",     ph7_iterator_apply },` |
|       - | 8232 | `	{"count",             ph7_hashmap_count },` |
|       - | 8233 | `	{"sizeof",            ph7_hashmap_count },` |
|       - | 8234 | `	{"array_key_exists",  ph7_hashmap_key_exists },` |
|       - | 8235 | `	{"array_pop",         ph7_hashmap_pop     },` |
|       - | 8236 | `	{"array_push",        ph7_hashmap_push    },` |
|       - | 8237 | `	{"array_shift",       ph7_hashmap_shift   },` |
|       - | 8238 | `	{"array_product",     ph7_hashmap_product },` |
|       - | 8239 | `	{"array_sum",         ph7_hashmap_sum     },` |
|       - | 8240 | `	{"array_keys",        ph7_hashmap_keys    },` |
|       - | 8241 | `	{"array_values",      ph7_hashmap_values  },` |
|       - | 8242 | `	{"array_same",        ph7_hashmap_same    },  /* Symisc eXtension */` |
|       - | 8243 | `	{"array_merge",       ph7_hashmap_merge   },` |
|       - | 8244 | `	{"array_slice",       ph7_hashmap_slice   },` |
|       - | 8245 | `	{"array_splice",      ph7_hashmap_splice  },` |
|       - | 8246 | `	{"array_search",      ph7_hashmap_search  },` |
|       - | 8247 | `	{"array_diff",        ph7_hashmap_diff    },` |
|       - | 8248 | `	{"array_udiff",       ph7_hashmap_udiff   },` |
|       - | 8249 | `	{"array_diff_assoc",  ph7_hashmap_diff_assoc },` |
|       - | 8250 | `	{"array_diff_uassoc", ph7_hashmap_diff_uassoc },` |
|       - | 8251 | `	{"array_diff_key",    ph7_hashmap_diff_key },` |
|       - | 8252 | `	{"array_intersect",   ph7_hashmap_intersect},` |
|       - | 8253 | `	{"array_intersect_assoc", ph7_hashmap_intersect_assoc},` |
|       - | 8254 | `	{"array_uintersect",  ph7_hashmap_uintersect},` |
|       - | 8255 | `	{"array_intersect_key",   ph7_hashmap_intersect_key},` |
|       - | 8256 | `	{"array_copy",        ph7_hashmap_copy    },` |
|       - | 8257 | `	{"array_erase",       ph7_hashmap_erase   },` |
|       - | 8258 | `	{"array_fill",        ph7_hashmap_fill    },` |
|       - | 8259 | `	{"array_fill_keys",   ph7_hashmap_fill_keys},` |
|       - | 8260 | `	{"array_combine",     ph7_hashmap_combine },` |
|       - | 8261 | `	{"array_reverse",     ph7_hashmap_reverse },` |
|       - | 8262 | `	{"array_unique",      ph7_hashmap_unique  },` |
|       - | 8263 | `	{"array_flip",        ph7_hashmap_flip    },` |
|       - | 8264 | `	{"array_rand",        ph7_hashmap_rand    },` |
|       - | 8265 | `	{"array_chunk",       ph7_hashmap_chunk   },` |
|       - | 8266 | `	{"array_pad",         ph7_hashmap_pad     },` |
|       - | 8267 | `	{"array_replace",     ph7_hashmap_replace },` |
|       - | 8268 | `	{"array_filter",      ph7_hashmap_filter  },` |
|       - | 8269 | `	{"array_map",         ph7_hashmap_map     },` |
|       - | 8270 | `	{"array_column",      ph7_hashmap_column  },` |
|       - | 8271 | `	{"array_is_list",     ph7_hashmap_is_list },` |
|       - | 8272 | `	{"array_first",       ph7_hashmap_first   },` |
|       - | 8273 | `	{"array_last",        ph7_hashmap_last    },` |
|       - | 8274 | `	{"array_find",        ph7_hashmap_find    },` |
|       - | 8275 | `	{"array_find_key",    ph7_hashmap_find_key},` |
|       - | 8276 | `	{"array_any",         ph7_hashmap_any     },` |
|       - | 8277 | `	{"array_all",         ph7_hashmap_all     },` |
|       - | 8278 | `	{"array_reduce",      ph7_hashmap_reduce  },` |
|       - | 8279 | `	{"array_walk",        ph7_hashmap_walk    },` |
|       - | 8280 | `	{"array_walk_recursive", ph7_hashmap_walk_recursive },` |
|       - | 8281 | `	{"in_array",          ph7_hashmap_in_array},` |
|       - | 8282 | `	{"sort",              ph7_hashmap_sort    },` |
|       - | 8283 | `	{"asort",             ph7_hashmap_asort   },` |
|       - | 8284 | `	{"arsort",            ph7_hashmap_arsort  },` |
|       - | 8285 | `	{"ksort",             ph7_hashmap_ksort   },` |
|       - | 8286 | `	{"krsort",            ph7_hashmap_krsort  },` |
|       - | 8287 | `	{"rsort",             ph7_hashmap_rsort   },` |
|       - | 8288 | `	{"usort",             ph7_hashmap_usort   },` |
|       - | 8289 | `	{"uasort",            ph7_hashmap_uasort  },` |
|       - | 8290 | `	{"uksort",            ph7_hashmap_uksort  },` |
|       - | 8291 | `	{"shuffle",           ph7_hashmap_shuffle },` |
|       - | 8292 | `	{"range",             ph7_hashmap_range   },` |
|       - | 8293 | `	{"current",           ph7_hashmap_current },` |
|       - | 8294 | `	{"each",              ph7_hashmap_each    },` |
|       - | 8295 | `	{"pos",               ph7_hashmap_current },` |
|       - | 8296 | `	{"next",              ph7_hashmap_next    },` |
|       - | 8297 | `	{"prev",              ph7_hashmap_prev    },` |
|       - | 8298 | `	{"end",               ph7_hashmap_end     },` |
|       - | 8299 | `	{"reset",             ph7_hashmap_reset   },` |
|       - | 8300 | `	{"key",               ph7_hashmap_simple_key }` |
|       - | 8301 | `};` |
|       - | 8302 | `/*` |
|       - | 8303 | ` * Register the built-in hashmap functions defined above.` |
|       - | 8304 | ` */` |
|    3476 | 8305 | `PH7_PRIVATE void PH7_RegisterHashmapFunctions(ph7_vm *pVm)` |
|       5 | 8306 | `{` |
|       - | 8307 | `	sxu32 n;` |
|  253753 | 8308 | `	for( n = 0 ; n < SX_ARRAYSIZE(aHashmapFunc) ; n++ ){` |
|  250277 | 8309 | `		ph7_create_function(&(*pVm),aHashmapFunc[n].zName,aHashmapFunc[n].xFunc,0);` |
|  125141 | 8310 | `	}` |
|    3481 | 8311 | `}` |
|       - | 8312 | `/*` |
|       - | 8313 | ` * Dump a hashmap instance and it's entries and the store the dump in` |
|       - | 8314 | ` * the BLOB given as the first argument.` |
|       - | 8315 | ` * This function is typically invoked when the user issue a call to` |
|       - | 8316 | ` * [var_dump(),var_export(),print_r(),...]` |
|       - | 8317 | ` * This function SXRET_OK on success. Any other return value including` |
|       - | 8318 | ` * SXERR_LIMIT(infinite recursion) indicates failure.` |
|       - | 8319 | ` */` |
|       - | 8320 | `/*` |
|       - | 8321 | ` * Dump the entries of a hashmap [i.e: the key/value lines between the opening` |
|       - | 8322 | ` * '{' and the closing '}'] in the var_dump/print_r style. Factored out of` |
|       - | 8323 | ` * PH7_HashmapDump so the var_dump object renderer can reuse it for a` |
|       - | 8324 | ` * __debugInfo() array body (which carries an object header, not "array(N)").` |
|       - | 8325 | ` * Returns SXERR_LIMIT if a nested value hit the depth cap.` |
|       - | 8326 | ` */` |
|      26 | 8327 | `PH7_PRIVATE sxi32 PH7_HashmapDumpEntries(SyBlob *pOut,ph7_hashmap *pMap,int ShowType,int nTab,int nDepth)` |
|       3 | 8328 | `{` |
|      29 | 8329 | `	ph7_hashmap_node *pEntry = pMap->pFirst;` |
|       - | 8330 | `	ph7_value *pObj;` |
|      29 | 8331 | `	sxu32 n = 0;` |
|       - | 8332 | `	int isRef;` |
|      29 | 8333 | `	sxi32 rc = SXRET_OK;` |
|       - | 8334 | `	int i;` |
|      44 | 8335 | `	for(;;){` |
|      91 | 8336 | `		if( n >= pMap->nEntry ){` |
|      29 | 8337 | `			break;` |
|       - | 8338 | `		}` |
|     127 | 8339 | `		for( i = 0 ; i < nTab ; i++ ){` |
|      65 | 8340 | `			SyBlobAppend(&(*pOut)," ",sizeof(char));` |
|      34 | 8341 | `		}` |
|       - | 8342 | `		/* Dump key */` |
|      65 | 8343 | `		if( pEntry->iType == HASHMAP_INT_NODE){` |
|      33 | 8344 | `			SyBlobFormat(&(*pOut),"[%qd] =>",pEntry->xKey.iKey);` |
|      17 | 8345 | `		}else{` |
|      48 | 8346 | `			SyBlobFormat(&(*pOut),"[%.*s] =>",` |
|      15 | 8347 | `				SyBlobLength(&pEntry->xKey.sKey),SyBlobData(&pEntry->xKey.sKey));` |
|       - | 8348 | `		}` |
|       - | 8349 | `#ifdef __WINNT__` |
|       3 | 8350 | `		SyBlobAppend(&(*pOut),"\r\n",sizeof("\r\n")-1);` |
|       - | 8351 | `#else` |
|      62 | 8352 | `		SyBlobAppend(&(*pOut),"\n",sizeof(char));` |
|       - | 8353 | `#endif` |
|       - | 8354 | `		/* Dump node value */` |
|      65 | 8355 | `		pObj = HashmapExtractNodeValue(pEntry);` |
|      65 | 8356 | `		isRef = 0;` |
|      65 | 8357 | `		if( pObj ){` |
|      65 | 8358 | `			if( pEntry->iFlags & HASHMAP_NODE_FOREIGN_OBJ ){` |
|       - | 8359 | `				/* Referenced object */` |
|     ! 0 | 8360 | `				isRef = 1;` |
|     ! 0 | 8361 | `			}` |
|      65 | 8362 | `			rc = PH7_MemObjDump(&(*pOut),pObj,ShowType,nTab+1,nDepth,isRef);` |
|      65 | 8363 | `			if( rc == SXERR_LIMIT ){` |
|     ! 0 | 8364 | `				break;` |
|       - | 8365 | `			}` |
|      31 | 8366 | `		}` |
|       - | 8367 | `		/* Point to the next entry */` |
|      65 | 8368 | `		n++;` |
|      65 | 8369 | `		pEntry = pEntry->pPrev; /* Reverse link */` |
|       3 | 8370 | `	}` |
|      29 | 8371 | `	return rc;` |
|       3 | 8372 | `}` |
|      22 | 8373 | `PH7_PRIVATE sxi32 PH7_HashmapDump(SyBlob *pOut,ph7_hashmap *pMap,int ShowType,int nTab,int nDepth)` |
|       2 | 8374 | `{` |
|       - | 8375 | `	sxi32 rc;` |
|       - | 8376 | `	int i;` |
|      24 | 8377 | `	if( nDepth > 31 ){` |
|       - | 8378 | `		static const char zInfinite[] = "Nesting limit reached: Infinite recursion?";` |
|       - | 8379 | `		/* Nesting limit reached */` |
|     ! 0 | 8380 | `		SyBlobAppend(&(*pOut),zInfinite,sizeof(zInfinite)-1);` |
|     ! 0 | 8381 | `		if( ShowType ){` |
|     ! 0 | 8382 | `			SyBlobAppend(&(*pOut),")",sizeof(char));` |
|     ! 0 | 8383 | `		}` |
|     ! 0 | 8384 | `		return SXERR_LIMIT;` |
|       - | 8385 | `	}` |
|      24 | 8386 | `	if( !ShowType ){` |
|      11 | 8387 | `		SyBlobAppend(&(*pOut),"Array(",sizeof("Array(")-1);` |
|       5 | 8388 | `	}` |
|       - | 8389 | `	/* Total entries */` |
|      24 | 8390 | `	SyBlobFormat(&(*pOut),"%u) {",pMap->nEntry);` |
|       - | 8391 | `#ifdef __WINNT__` |
|       2 | 8392 | `	SyBlobAppend(&(*pOut),"\r\n",sizeof("\r\n")-1);` |
|       - | 8393 | `#else` |
|      22 | 8394 | `	SyBlobAppend(&(*pOut),"\n",sizeof(char));` |
|       - | 8395 | `#endif` |
|      24 | 8396 | `	rc = PH7_HashmapDumpEntries(&(*pOut),pMap,ShowType,nTab,nDepth);` |
|      46 | 8397 | `	for( i = 0 ; i < nTab ; i++ ){` |
|      24 | 8398 | `		SyBlobAppend(&(*pOut)," ",sizeof(char));` |
|      13 | 8399 | `	}` |
|      24 | 8400 | `	SyBlobAppend(&(*pOut),"}",sizeof(char));` |
|      24 | 8401 | `	return rc;` |
|      13 | 8402 | `}` |
|       - | 8403 | `/*` |
|       - | 8404 | ` * Iterate throw hashmap entries and invoke the given callback [i.e: xWalk()] for each` |
|       - | 8405 | ` * retrieved entry.` |
|       - | 8406 | ` * Note that argument are passed to the callback by copy. That is,any modification to` |
|       - | 8407 | ` * the entry value in the callback body will not alter the real value.` |
|       - | 8408 | ` * If the callback wishes to abort processing [i.e: it's invocation] it must return` |
|       - | 8409 | ` * a value different from PH7_OK.` |
|       - | 8410 | ` * Refer to [ph7_array_walk()] for more information.` |
|       - | 8411 | ` */` |
|   32676 | 8412 | `PH7_PRIVATE sxi32 PH7_HashmapWalk(` |
|       - | 8413 | `	ph7_hashmap *pMap, /* Target hashmap */` |
|       - | 8414 | `	int (*xWalk)(ph7_value *,ph7_value *,void *), /* Walker callback */` |
|       - | 8415 | `	void *pUserData /* Last argument to xWalk() */` |
|       - | 8416 | `	)` |
|       5 | 8417 | `{` |
|       - | 8418 | `	ph7_hashmap_node *pEntry;` |
|       - | 8419 | `	ph7_value sKey,sValue;` |
|       - | 8420 | `	sxi32 rc;` |
|       - | 8421 | `	sxu32 n;` |
|       - | 8422 | `	/* Initialize walker parameter */` |
|   32681 | 8423 | `	rc = SXRET_OK;` |
|   32681 | 8424 | `	PH7_MemObjInit(pMap->pVm,&sKey);` |
|   32681 | 8425 | `	PH7_MemObjInit(pMap->pVm,&sValue);` |
|   32681 | 8426 | `	n = pMap->nEntry;` |
|   32681 | 8427 | `	pEntry = pMap->pFirst;` |
|       - | 8428 | `	/* Start the iteration process */` |
|   83981 | 8429 | `	for(;;){` |
|  167967 | 8430 | `		if( n < 1 ){` |
|   32681 | 8431 | `			break;` |
|       - | 8432 | `		}` |
|       - | 8433 | `		/* Extract a copy of the key and a copy the current value */` |
|  135291 | 8434 | `		PH7_HashmapExtractNodeKey(pEntry,&sKey);` |
|  135291 | 8435 | `		PH7_HashmapExtractNodeValue(pEntry,&sValue,FALSE);` |
|       - | 8436 | `		/* Invoke the user callback */` |
|  135291 | 8437 | `		rc = xWalk(&sKey,&sValue,pUserData);` |
|       - | 8438 | `		/* Release the copy of the key and the value */` |
|  135291 | 8439 | `		PH7_MemObjRelease(&sKey);` |
|  135291 | 8440 | `		PH7_MemObjRelease(&sValue);` |
|  135291 | 8441 | `		if( rc != PH7_OK ){` |
|       - | 8442 | `			/* Callback request an operation abort */` |
|     ! 0 | 8443 | `			return SXERR_ABORT;` |
|       - | 8444 | `		}` |
|       - | 8445 | `		/* Point to the next entry */` |
|  135291 | 8446 | `		pEntry = pEntry->pPrev; /* Reverse link */` |
|  135291 | 8447 | `		n--;` |
|       5 | 8448 | `	}` |
|       - | 8449 | `	/* All done */` |
|   32681 | 8450 | `	return SXRET_OK;` |
|   16343 | 8451 | `}` |
|       - | 8452 |  |
