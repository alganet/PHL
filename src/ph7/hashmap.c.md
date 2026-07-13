# src/ph7/hashmap.c

<style>code, pre { background: none !important; white-space: pre !important; width: 100% !important; display: inline-block !important; } td { border: none !important; margin-top: 0 !important; margin-bottom: 0 !important; padding-top: 0 !important; padding-bottom: 0 !important; }</style>

Coverage: 3805/4260 lines (89.32%)

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
|   7384234 |   23 | `static sxu32 IntHash(sxi64 iKey)` |
|         5 |   24 | `{` |
|   7384239 |   25 | `	sxu64 uKey = (sxu64)iKey; /* unsigned mixing: shifting a negative key is UB */` |
|   7384239 |   26 | `	return (sxu32)(uKey ^ (uKey << 8) ^ (uKey >> 8));` |
|         5 |   27 | `}` |
|         - |   28 | `/*` |
|         - |   29 | ` * Default hash function for string/BLOB keys.` |
|         - |   30 | ` */` |
|    411070 |   31 | `static sxu32 BinHash(const void *pSrc,sxu32 nLen)` |
|         5 |   32 | `{` |
|    411075 |   33 | `	register unsigned char *zIn = (unsigned char *)pSrc;` |
|         - |   34 | `	unsigned char *zEnd;` |
|    411075 |   35 | `	sxu32 nH = 5381;` |
|    411075 |   36 | `	zEnd = &zIn[nLen];` |
|    480508 |   37 | `	for(;;){` |
|    961021 |   38 | `		if( zIn >= zEnd ){ break; } nH = nH * 33 + zIn[0] ; zIn++;` |
|    831027 |   39 | `		if( zIn >= zEnd ){ break; } nH = nH * 33 + zIn[0] ; zIn++;` |
|    751981 |   40 | `		if( zIn >= zEnd ){ break; } nH = nH * 33 + zIn[0] ; zIn++;` |
|    642915 |   41 | `		if( zIn >= zEnd ){ break; } nH = nH * 33 + zIn[0] ; zIn++;` |
|         5 |   42 | `	}` |
|    411075 |   43 | `	return nH;` |
|         5 |   44 | `}` |
|         - |   45 | `/*` |
|         - |   46 | ` * Return the total number of entries in a given hashmap.` |
|         - |   47 | ` * If bRecursive is set to TRUE then recurse on hashmap entries.` |
|         - |   48 | ` * Self-referential arrays are detected via the HASHMAP_COUNTING flag;` |
|         - |   49 | ` * when a cycle is found the nested array is skipped and *pCycleDetected` |
|         - |   50 | ` * is set to TRUE so the caller can emit a warning.` |
|         - |   51 | ` */` |
|      1070 |   52 | `static sxi64 HashmapCount(ph7_hashmap *pMap,int bRecursive,int *pCycleDetected)` |
|         5 |   53 | `{` |
|      1075 |   54 | `	sxi64 iCount = 0;` |
|      1075 |   55 | `	if( !bRecursive ){` |
|       901 |   56 | `		iCount = pMap->nEntry;` |
|       453 |   57 | `	}else{` |
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
|      1075 |   92 | `	return iCount;` |
|         5 |   93 | `}` |
|         - |   94 | `/*` |
|         - |   95 | ` * Allocate a new hashmap node with a 64-bit integer key.` |
|         - |   96 | ` * If something goes wrong [i.e: out of memory],this function return NULL.` |
|         - |   97 | ` * Otherwise a fresh [ph7_hashmap_node] instance is returned.` |
|         - |   98 | ` */` |
|   3088004 |   99 | `static ph7_hashmap_node * HashmapNewIntNode(ph7_hashmap *pMap,sxi64 iKey,sxu32 nHash,sxu32 nValIdx)` |
|         5 |  100 | `{` |
|         - |  101 | `	ph7_hashmap_node *pNode;` |
|         - |  102 | `	/* Allocate a new node */` |
|   3088009 |  103 | `	pNode = (ph7_hashmap_node *)SyMemBackendPoolAlloc(&pMap->pVm->sAllocator,sizeof(ph7_hashmap_node));` |
|   3088009 |  104 | `	if( pNode == 0 ){` |
|       ! 0 |  105 | `		return 0;` |
|         - |  106 | `	}` |
|         - |  107 | `	/* Zero the stucture */` |
|   3088009 |  108 | `	SyZero(pNode,sizeof(ph7_hashmap_node));` |
|         - |  109 | `	/* Fill in the structure */` |
|   3088009 |  110 | `	pNode->pMap  = &(*pMap);` |
|   3088009 |  111 | `	pNode->iType = HASHMAP_INT_NODE;` |
|   3088009 |  112 | `	pNode->nHash = nHash;` |
|   3088009 |  113 | `	pNode->xKey.iKey = iKey;` |
|   3088009 |  114 | `	pNode->nValIdx  = nValIdx;` |
|   3088009 |  115 | `	return pNode;` |
|   1544007 |  116 | `}` |
|         - |  117 | `/*` |
|         - |  118 | ` * Allocate a new hashmap node with a BLOB key.` |
|         - |  119 | ` * If something goes wrong [i.e: out of memory],this function return NULL.` |
|         - |  120 | ` * Otherwise a fresh [ph7_hashmap_node] instance is returned.` |
|         - |  121 | ` */` |
|    155184 |  122 | `static ph7_hashmap_node * HashmapNewBlobNode(ph7_hashmap *pMap,const void *pKey,sxu32 nKeyLen,sxu32 nHash,sxu32 nValIdx)` |
|         5 |  123 | `{` |
|         - |  124 | `	ph7_hashmap_node *pNode;` |
|         - |  125 | `	/* Allocate a new node */` |
|    155189 |  126 | `	pNode = (ph7_hashmap_node *)SyMemBackendPoolAlloc(&pMap->pVm->sAllocator,sizeof(ph7_hashmap_node));` |
|    155189 |  127 | `	if( pNode == 0 ){` |
|       ! 0 |  128 | `		return 0;` |
|         - |  129 | `	}` |
|         - |  130 | `	/* Zero the stucture */` |
|    155189 |  131 | `	SyZero(pNode,sizeof(ph7_hashmap_node));` |
|         - |  132 | `	/* Fill in the structure */` |
|    155189 |  133 | `	pNode->pMap  = &(*pMap);` |
|    155189 |  134 | `	pNode->iType = HASHMAP_BLOB_NODE;` |
|    155189 |  135 | `	pNode->nHash = nHash;` |
|    155189 |  136 | `	SyBlobInit(&pNode->xKey.sKey,&pMap->pVm->sAllocator);` |
|    155189 |  137 | `	SyBlobAppend(&pNode->xKey.sKey,pKey,nKeyLen);` |
|    155189 |  138 | `	pNode->nValIdx = nValIdx;` |
|    155189 |  139 | `	return pNode;` |
|     77597 |  140 | `}` |
|         - |  141 | `/*` |
|         - |  142 | ` * link a hashmap node to the given bucket index (last argument to this function).` |
|         - |  143 | ` */` |
|   3243188 |  144 | `static void HashmapNodeLink(ph7_hashmap *pMap,ph7_hashmap_node *pNode,sxu32 nBucketIdx)` |
|         5 |  145 | `{` |
|         - |  146 | `	/* Link */` |
|   3243193 |  147 | `	if( pMap->apBucket[nBucketIdx] != 0 ){` |
|   2861531 |  148 | `		pNode->pNextCollide = pMap->apBucket[nBucketIdx];` |
|   2861531 |  149 | `		pMap->apBucket[nBucketIdx]->pPrevCollide = pNode;` |
|   1430763 |  150 | `	}` |
|   3243193 |  151 | `	pMap->apBucket[nBucketIdx] = pNode;` |
|         - |  152 | `	/* Link to the map list */` |
|   3243193 |  153 | `	if( pMap->pFirst == 0 ){` |
|     75209 |  154 | `		pMap->pFirst = pMap->pLast = pNode;` |
|         - |  155 | `		/* Point to the first inserted node */` |
|     75209 |  156 | `		pMap->pCur = pNode;` |
|     37607 |  157 | `	}else{` |
|   3167989 |  158 | `		MACRO_LD_PUSH(pMap->pLast,pNode);` |
|         - |  159 | `	}` |
|   3243193 |  160 | `	++pMap->nEntry;` |
|   3243193 |  161 | `}` |
|         - |  162 | `/*` |
|         - |  163 | ` * Unlink a node from the hashmap.` |
|         - |  164 | ` * If the node count reaches zero then release the whole hash-bucket.` |
|         - |  165 | ` */` |
|      7416 |  166 | `PH7_PRIVATE void PH7_HashmapUnlinkNode(ph7_hashmap_node *pNode,int bRestore)` |
|         5 |  167 | `{` |
|      7421 |  168 | `	ph7_hashmap *pMap = pNode->pMap;` |
|      7421 |  169 | `	ph7_vm *pVm = pMap->pVm;` |
|         - |  170 | `	/* Unlink from the corresponding bucket */` |
|      7421 |  171 | `	if( pNode->pPrevCollide == 0 ){` |
|      6957 |  172 | `		pMap->apBucket[pNode->nHash & (pMap->nSize - 1)] = pNode->pNextCollide;` |
|      3481 |  173 | `	}else{` |
|       466 |  174 | `		pNode->pPrevCollide->pNextCollide = pNode->pNextCollide;` |
|         - |  175 | `	}` |
|      7421 |  176 | `	if( pNode->pNextCollide ){` |
|      4379 |  177 | `		pNode->pNextCollide->pPrevCollide = pNode->pPrevCollide;` |
|      2188 |  178 | `	}` |
|      7421 |  179 | `	if( pMap->pFirst == pNode ){` |
|       131 |  180 | `		pMap->pFirst = pNode->pPrev;` |
|        63 |  181 | `	}` |
|      7421 |  182 | `	if( pMap->pCur == pNode ){` |
|         - |  183 | `		/* Advance the node cursor */` |
|       133 |  184 | `		pMap->pCur = pMap->pCur->pPrev; /* Reverse link */` |
|        64 |  185 | `	}` |
|         - |  186 | `	/* Unlink from the map list */` |
|      7421 |  187 | `	MACRO_LD_REMOVE(pMap->pLast,pNode);` |
|      7421 |  188 | `	if( bRestore ){` |
|         - |  189 | `		/* Remove the ph7_value associated with this node from the reference table */` |
|       107 |  190 | `		PH7_VmRefObjRemove(pVm,pNode->nValIdx,0,pNode);` |
|         - |  191 | `		/* Restore to the freelist */` |
|       107 |  192 | `		if( (pNode->iFlags & HASHMAP_NODE_FOREIGN_OBJ) == 0 ){` |
|       107 |  193 | `			PH7_VmUnsetMemObj(pVm,pNode->nValIdx,FALSE);` |
|        51 |  194 | `		}` |
|        51 |  195 | `	}` |
|      7421 |  196 | `	if( pNode->iType == HASHMAP_BLOB_NODE ){` |
|      7284 |  197 | `		SyBlobRelease(&pNode->xKey.sKey);` |
|      3640 |  198 | `	}` |
|      7421 |  199 | `	SyMemBackendPoolFree(&pVm->sAllocator,pNode);` |
|      7421 |  200 | `	pMap->nEntry--;` |
|      7421 |  201 | `	if( pMap->nEntry < 1 && pMap != pVm->pGlobal ){` |
|         - |  202 | `		/* Free the hash-bucket */` |
|        75 |  203 | `		SyMemBackendFree(&pVm->sAllocator,pMap->apBucket);` |
|        75 |  204 | `		pMap->apBucket = 0;` |
|        75 |  205 | `		pMap->nSize = 0;` |
|        75 |  206 | `		pMap->pFirst = pMap->pLast = pMap->pCur = 0;` |
|        35 |  207 | `	}` |
|      7421 |  208 | `}` |
|         - |  209 | `#define HASHMAP_FILL_FACTOR 3` |
|         - |  210 | `/*` |
|         - |  211 | ` * Grow the hash-table and rehash all entries.` |
|         - |  212 | ` */` |
|   3243188 |  213 | `static sxi32 HashmapGrowBucket(ph7_hashmap *pMap)` |
|         5 |  214 | `{` |
|   3243193 |  215 | `	if( pMap->nEntry >= pMap->nSize * HASHMAP_FILL_FACTOR ){` |
|     79933 |  216 | `		ph7_hashmap_node **apOld = pMap->apBucket;` |
|         - |  217 | `		ph7_hashmap_node *pEntry,**apNew;` |
|     79933 |  218 | `		sxu32 nNew = pMap->nSize << 1;` |
|         - |  219 | `		sxu32 nBucket;` |
|         - |  220 | `		sxu32 n;` |
|     79933 |  221 | `		if( nNew < 1 ){` |
|     75209 |  222 | `			nNew = 16;` |
|     37602 |  223 | `		}` |
|         - |  224 | `		/* Allocate a new bucket */` |
|     79933 |  225 | `		apNew = (ph7_hashmap_node **)SyMemBackendAlloc(&pMap->pVm->sAllocator,nNew * sizeof(ph7_hashmap_node *));` |
|     79933 |  226 | `		if( apNew == 0 ){` |
|       ! 0 |  227 | `			if( pMap->nSize < 1 ){` |
|       ! 0 |  228 | `				return SXERR_MEM; /* Fatal */` |
|         - |  229 | `			}` |
|         - |  230 | `			/* Not so fatal here,simply a performance hit */` |
|       ! 0 |  231 | `			return SXRET_OK;` |
|         - |  232 | `		}` |
|         - |  233 | `		/* Zero the table */` |
|     79933 |  234 | `		SyZero((void *)apNew,nNew * sizeof(ph7_hashmap_node *));` |
|         - |  235 | `		/* Reflect the change */` |
|     79933 |  236 | `		pMap->apBucket = apNew;` |
|     79933 |  237 | `		pMap->nSize = nNew;` |
|     79933 |  238 | `		if( apOld == 0 ){` |
|         - |  239 | `			/* First allocated table [i.e: no entry],return immediately */` |
|     75209 |  240 | `			return SXRET_OK;` |
|         - |  241 | `		}` |
|         - |  242 | `		/* Rehash old entries */` |
|      4729 |  243 | `		pEntry = pMap->pFirst;` |
|      4729 |  244 | `		n = 0;` |
|   2084362 |  245 | `		for( ;; ){` |
|   4168729 |  246 | `			if( n >= pMap->nEntry ){` |
|      4729 |  247 | `				break;` |
|         - |  248 | `			}` |
|         - |  249 | `			/* Clear the old collision link */` |
|   4164005 |  250 | `			pEntry->pNextCollide = pEntry->pPrevCollide = 0;` |
|         - |  251 | `			/* Link to the new bucket */` |
|   4164005 |  252 | `			nBucket = pEntry->nHash & (nNew - 1);` |
|   4164005 |  253 | `			if( pMap->apBucket[nBucket] != 0 ){` |
|   3570819 |  254 | `				pEntry->pNextCollide = pMap->apBucket[nBucket];` |
|   3570819 |  255 | `				pMap->apBucket[nBucket]->pPrevCollide = pEntry;` |
|   1785407 |  256 | `			}` |
|   4164005 |  257 | `			pMap->apBucket[nBucket] = pEntry;` |
|         - |  258 | `			/* Point to the next entry */` |
|   4164005 |  259 | `			pEntry = pEntry->pPrev; /* Reverse link */` |
|   4164005 |  260 | `			n++;` |
|         5 |  261 | `		}` |
|         - |  262 | `		/* Free the old table */` |
|      4729 |  263 | `		SyMemBackendFree(&pMap->pVm->sAllocator,(void *)apOld);` |
|      2362 |  264 | `	}` |
|   3167989 |  265 | `	return SXRET_OK;` |
|   1621599 |  266 | `}` |
|         - |  267 | `/*` |
|         - |  268 | ` * Insert a 64-bit integer key and it's associated value (if any) in the given` |
|         - |  269 | ` * hashmap.` |
|         - |  270 | ` */` |
|   3088004 |  271 | `static sxi32 HashmapInsertIntKey(ph7_hashmap *pMap,sxi64 iKey,ph7_value *pValue,sxu32 nRefIdx,int isForeign)` |
|         5 |  272 | `{` |
|         - |  273 | `	ph7_hashmap_node *pNode;` |
|         - |  274 | `	sxu32 nIdx;` |
|         - |  275 | `	sxu32 nHash;` |
|         - |  276 | `	sxi32 rc;` |
|   3088009 |  277 | `	if( !isForeign ){` |
|         - |  278 | `		ph7_value *pObj;` |
|         - |  279 | `		ph7_value sSafeVal;` |
|         - |  280 | `		/* Snapshot the source BEFORE reserving: PH7_ReserveMemObj can grow (move)` |
|         - |  281 | `		 * pVm->aMemObj, which would dangle pValue when it points into the pool` |
|         - |  282 | `		 * (e.g. get_defined_vars/func_get_args/get_class_vars/get_object_vars pass` |
|         - |  283 | `		 * a pool slot). A shallow copy is a safe PH7_MemObjStore source — the` |
|         - |  284 | `		 * referent and the heap-resident blob data survive the move; only the` |
|         - |  285 | `		 * ph7_value struct relocates (same sSafeVal idiom used by PH7_HashmapDup). */` |
|   3087971 |  286 | `		if( pValue ){` |
|   3087969 |  287 | `			sSafeVal = *pValue;` |
|   3087969 |  288 | `			pValue = &sSafeVal;` |
|   1543982 |  289 | `		}` |
|         - |  290 | `		/* Reserve a ph7_value for the value */` |
|   3087971 |  291 | `		pObj = PH7_ReserveMemObj(pMap->pVm);` |
|   3087971 |  292 | `		if( pObj == 0 ){` |
|       ! 0 |  293 | `			return SXERR_MEM;` |
|         - |  294 | `		}` |
|   3087971 |  295 | `		if( pValue ){` |
|         - |  296 | `			/* Duplicate the value */` |
|   3087969 |  297 | `			PH7_MemObjStore(pValue,pObj);` |
|   1543982 |  298 | `		}` |
|   3087971 |  299 | `		nIdx = pObj->nIdx;` |
|   1543988 |  300 | `	}else{` |
|        39 |  301 | `		nIdx = nRefIdx;` |
|         - |  302 | `	}` |
|         - |  303 | `	/* Hash the key */` |
|   3088009 |  304 | `	nHash = pMap->xIntHash(iKey);` |
|         - |  305 | `	/* Allocate a new int node */` |
|   3088009 |  306 | `	pNode = HashmapNewIntNode(&(*pMap),iKey,nHash,nIdx);` |
|   3088009 |  307 | `	if( pNode == 0 ){` |
|       ! 0 |  308 | `		return SXERR_MEM;` |
|         - |  309 | `	}` |
|   3088009 |  310 | `	if( isForeign ){` |
|         - |  311 | `		/* Mark as a foregin entry */` |
|        39 |  312 | `		pNode->iFlags \|= HASHMAP_NODE_FOREIGN_OBJ;` |
|        19 |  313 | `	}` |
|         - |  314 | `	/* Make sure the bucket is big enough to hold the new entry */` |
|   3088009 |  315 | `	rc = HashmapGrowBucket(&(*pMap));` |
|   3088009 |  316 | `	if( rc != SXRET_OK ){` |
|       ! 0 |  317 | `		SyMemBackendPoolFree(&pMap->pVm->sAllocator,pNode);` |
|       ! 0 |  318 | `		return rc;` |
|         - |  319 | `	}` |
|         - |  320 | `	/* Perform the insertion */` |
|   3088009 |  321 | `	HashmapNodeLink(&(*pMap),pNode,nHash & (pMap->nSize - 1));` |
|         - |  322 | `	/* Install in the reference table */` |
|   3088009 |  323 | `	PH7_VmRefObjInstall(pMap->pVm,nIdx,0,pNode,0);` |
|         - |  324 | `	/* All done */` |
|   3088009 |  325 | `	return SXRET_OK;` |
|   1544007 |  326 | `}` |
|         - |  327 | `/*` |
|         - |  328 | ` * Insert a BLOB key and it's associated value (if any) in the given` |
|         - |  329 | ` * hashmap.` |
|         - |  330 | ` */` |
|    155184 |  331 | `static sxi32 HashmapInsertBlobKey(ph7_hashmap *pMap,const void *pKey,sxu32 nKeyLen,ph7_value *pValue,sxu32 nRefIdx,int isForeign)` |
|         5 |  332 | `{` |
|         - |  333 | `	ph7_hashmap_node *pNode;` |
|         - |  334 | `	sxu32 nHash;` |
|         - |  335 | `	sxu32 nIdx;` |
|         - |  336 | `	sxi32 rc;` |
|    155189 |  337 | `	if( !isForeign ){` |
|         - |  338 | `		ph7_value *pObj;` |
|         - |  339 | `		ph7_value sSafeVal;` |
|         - |  340 | `		/* Snapshot the source BEFORE reserving: PH7_ReserveMemObj can grow (move)` |
|         - |  341 | `		 * pVm->aMemObj, which would dangle pValue when it points into the pool` |
|         - |  342 | `		 * (e.g. get_defined_vars/func_get_args/get_class_vars/get_object_vars pass` |
|         - |  343 | `		 * a pool slot). A shallow copy is a safe PH7_MemObjStore source — the` |
|         - |  344 | `		 * referent and the heap-resident blob data survive the move; only the` |
|         - |  345 | `		 * ph7_value struct relocates (same sSafeVal idiom used by PH7_HashmapDup). */` |
|    108947 |  346 | `		if( pValue ){` |
|    108657 |  347 | `			sSafeVal = *pValue;` |
|    108657 |  348 | `			pValue = &sSafeVal;` |
|     54326 |  349 | `		}` |
|         - |  350 | `		/* Reserve a ph7_value for the value */` |
|    108947 |  351 | `		pObj = PH7_ReserveMemObj(pMap->pVm);` |
|    108947 |  352 | `		if( pObj == 0 ){` |
|       ! 0 |  353 | `			return SXERR_MEM;` |
|         - |  354 | `		}` |
|    108947 |  355 | `		if( pValue ){` |
|         - |  356 | `			/* Duplicate the value */` |
|    108657 |  357 | `			PH7_MemObjStore(pValue,pObj);` |
|     54326 |  358 | `		}` |
|    108947 |  359 | `		nIdx = pObj->nIdx;` |
|     54476 |  360 | `	}else{` |
|     46247 |  361 | `		nIdx = nRefIdx;` |
|         - |  362 | `	}` |
|         - |  363 | `	/* Hash the key */` |
|    155189 |  364 | `	nHash = pMap->xBlobHash(pKey,nKeyLen);` |
|         - |  365 | `	/* Allocate a new blob node */` |
|    155189 |  366 | `	pNode = HashmapNewBlobNode(&(*pMap),pKey,nKeyLen,nHash,nIdx);` |
|    155189 |  367 | `	if( pNode == 0 ){` |
|       ! 0 |  368 | `		return SXERR_MEM;` |
|         - |  369 | `	}` |
|    155189 |  370 | `	if( isForeign ){` |
|         - |  371 | `		/* Mark as a foregin entry */` |
|     46247 |  372 | `		pNode->iFlags \|= HASHMAP_NODE_FOREIGN_OBJ;` |
|     23121 |  373 | `	}` |
|         - |  374 | `	/* Make sure the bucket is big enough to hold the new entry */` |
|    155189 |  375 | `	rc = HashmapGrowBucket(&(*pMap));` |
|    155189 |  376 | `	if( rc != SXRET_OK ){` |
|       ! 0 |  377 | `		SyMemBackendPoolFree(&pMap->pVm->sAllocator,pNode);` |
|       ! 0 |  378 | `		return rc;` |
|         - |  379 | `	}` |
|         - |  380 | `	/* Perform the insertion */` |
|    155189 |  381 | `	HashmapNodeLink(&(*pMap),pNode,nHash & (pMap->nSize - 1));` |
|         - |  382 | `	/* Install in the reference table */` |
|    155189 |  383 | `	PH7_VmRefObjInstall(pMap->pVm,nIdx,0,pNode,0);` |
|         - |  384 | `	/* All done */` |
|    155189 |  385 | `	return SXRET_OK;` |
|     77597 |  386 | `}` |
|         - |  387 | `/*` |
|         - |  388 | ` * Check if a given 64-bit integer key exists in the given hashmap.` |
|         - |  389 | ` * Write a pointer to the target node on success. Otherwise` |
|         - |  390 | ` * SXERR_NOTFOUND is returned on failure.` |
|         - |  391 | ` */` |
|   4283482 |  392 | `static sxi32 HashmapLookupIntKey(` |
|         - |  393 | `	ph7_hashmap *pMap,         /* Target hashmap */` |
|         - |  394 | `	sxi64 iKey,                /* lookup key */` |
|         - |  395 | `	ph7_hashmap_node **ppNode  /* OUT: target node on success */` |
|         - |  396 | `	)` |
|         5 |  397 | `{` |
|         - |  398 | `	ph7_hashmap_node *pNode;` |
|         - |  399 | `	sxu32 nHash;` |
|   4283487 |  400 | `	if( pMap->nEntry < 1 ){` |
|         - |  401 | `		/* Don't bother hashing,there is no entry anyway */` |
|       567 |  402 | `		return SXERR_NOTFOUND;` |
|         - |  403 | `	}` |
|         - |  404 | `	/* Hash the key first */` |
|   4282925 |  405 | `	nHash = pMap->xIntHash(iKey);` |
|         - |  406 | `	/* Point to the appropriate bucket */` |
|   4282925 |  407 | `	pNode = pMap->apBucket[nHash & (pMap->nSize - 1)];` |
|         - |  408 | `	/* Perform the lookup */` |
| 110561980 |  409 | `	for(;;){` |
| 221123965 |  410 | `		if( pNode == 0 ){` |
|   4281067 |  411 | `			break;` |
|         - |  412 | `		}` |
| 216842898 |  413 | `		if( pNode->iType == HASHMAP_INT_NODE` |
| 216839888 |  414 | `			&& pNode->nHash == nHash` |
| 108419373 |  415 | `			&& pNode->xKey.iKey == iKey ){` |
|         - |  416 | `				/* Node found */` |
|      1863 |  417 | `				if( ppNode ){` |
|      1845 |  418 | `					*ppNode = pNode;` |
|       920 |  419 | `				}` |
|      1863 |  420 | `				return SXRET_OK;` |
|         - |  421 | `		}` |
|         - |  422 | `		/* Follow the collision link */` |
| 216841041 |  423 | `		pNode = pNode->pNextCollide;` |
|         1 |  424 | `	}` |
|         - |  425 | `	/* No such entry */` |
|   4281067 |  426 | `	return SXERR_NOTFOUND;` |
|   2141746 |  427 | `}` |
|         - |  428 | `/*` |
|         - |  429 | ` * Check if a given BLOB key exists in the given hashmap.` |
|         - |  430 | ` * Write a pointer to the target node on success. Otherwise` |
|         - |  431 | ` * SXERR_NOTFOUND is returned on failure.` |
|         - |  432 | ` */` |
|    280446 |  433 | `static sxi32 HashmapLookupBlobKey(` |
|         - |  434 | `	ph7_hashmap *pMap,          /* Target hashmap */` |
|         - |  435 | `	const void *pKey,           /* Lookup key */` |
|         - |  436 | `	sxu32 nKeyLen,              /* Key length in bytes */` |
|         - |  437 | `	ph7_hashmap_node **ppNode   /* OUT: target node on success */` |
|         - |  438 | `	)` |
|         5 |  439 | `{` |
|         - |  440 | `	ph7_hashmap_node *pNode;` |
|         - |  441 | `	sxu32 nHash;` |
|    280451 |  442 | `	if( pMap->nEntry < 1 ){` |
|         - |  443 | `		/* Don't bother hashing,there is no entry anyway */` |
|     24565 |  444 | `		return SXERR_NOTFOUND;` |
|         - |  445 | `	}` |
|         - |  446 | `	/* Hash the key first */` |
|    255891 |  447 | `	nHash = pMap->xBlobHash(pKey,nKeyLen);` |
|         - |  448 | `	/* Point to the appropriate bucket */` |
|    255891 |  449 | `	pNode = pMap->apBucket[nHash & (pMap->nSize - 1)];` |
|         - |  450 | `	/* Perform the lookup */` |
|    216618 |  451 | `	for(;;){` |
|    433241 |  452 | `		if( pNode == 0 ){` |
|    203681 |  453 | `			break;` |
|         - |  454 | `		}` |
|    229560 |  455 | `		if( pNode->iType == HASHMAP_BLOB_NODE` |
|    228055 |  456 | `			&& pNode->nHash == nHash` |
|    139426 |  457 | `			&& SyBlobLength(&pNode->xKey.sKey) == nKeyLen` |
|     52307 |  458 | `			&& SyMemcmp(SyBlobData(&pNode->xKey.sKey),pKey,nKeyLen) == 0 ){` |
|         - |  459 | `				/* Node found */` |
|     52215 |  460 | `				if( ppNode ){` |
|     52187 |  461 | `					*ppNode = pNode;` |
|     26091 |  462 | `				}` |
|     52215 |  463 | `				return SXRET_OK;` |
|         - |  464 | `		}` |
|         - |  465 | `		/* Follow the collision link */` |
|    177355 |  466 | `		pNode = pNode->pNextCollide;` |
|         5 |  467 | `	}` |
|         - |  468 | `	/* No such entry */` |
|    203681 |  469 | `	return SXERR_NOTFOUND;` |
|    140228 |  470 | `}` |
|         - |  471 | `/*` |
|         - |  472 | ` * Check if the given BLOB key looks like a decimal number.` |
|         - |  473 | ` * Retrurn TRUE on success.FALSE otherwise.` |
|         - |  474 | ` */` |
|    280576 |  475 | `static int HashmapIsIntKey(SyBlob *pKey)` |
|         5 |  476 | `{` |
|    280581 |  477 | `	const char *zIn  = (const char *)SyBlobData(pKey);` |
|    280581 |  478 | `	const char *zEnd = &zIn[SyBlobLength(pKey)];` |
|    280581 |  479 | `	if( (int)(zEnd-zIn) > 1 && zIn[0] == '0' ){` |
|         - |  480 | `		/* Octal not decimal number */` |
|         5 |  481 | `		return FALSE;` |
|         - |  482 | `	}` |
|    280577 |  483 | `	if( (zIn[0] == '-' \|\| zIn[0] == '+') && &zIn[1] < zEnd ){` |
|       ! 0 |  484 | `		zIn++;` |
|       ! 0 |  485 | `	}` |
|    140622 |  486 | `	for(;;){` |
|    281249 |  487 | `		if( zIn >= zEnd ){` |
|       239 |  488 | `			return TRUE;` |
|         - |  489 | `		}` |
|    281011 |  490 | `		if( (unsigned char)zIn[0] >= 0xc0 /* UTF-8 stream */  \|\| !SyisDigit(zIn[0]) ){` |
|    140172 |  491 | `			break;` |
|         - |  492 | `		}` |
|       673 |  493 | `		zIn++;` |
|         1 |  494 | `	}` |
|         - |  495 | `	/* Key does not look like a decimal number */` |
|    280339 |  496 | `	return FALSE;` |
|    140293 |  497 | `}` |
|         - |  498 | `/*` |
|         - |  499 | ` * Check if a given key exists in the given hashmap.` |
|         - |  500 | ` * Write a pointer to the target node on success.` |
|         - |  501 | ` * Otherwise SXERR_NOTFOUND is returned on failure.` |
|         - |  502 | ` */` |
|    127396 |  503 | `static sxi32 HashmapLookup(` |
|         - |  504 | `	ph7_hashmap *pMap,          /* Target hashmap */` |
|         - |  505 | `	ph7_value *pKey,            /* Lookup key */` |
|         - |  506 | `	ph7_hashmap_node **ppNode   /* OUT: target node on success */` |
|         - |  507 | `	)` |
|         5 |  508 | `{` |
|    127401 |  509 | `	ph7_hashmap_node *pNode = 0; /* cc -O6 warning */` |
|         - |  510 | `	sxi32 rc;` |
|    127401 |  511 | `	if( pKey->iFlags & (MEMOBJ_STRING\|MEMOBJ_HASHMAP\|MEMOBJ_OBJ\|MEMOBJ_RES) ){` |
|    125771 |  512 | `		if( (pKey->iFlags & MEMOBJ_STRING) == 0 ){` |
|         - |  513 | `			/* Force a string cast */` |
|       ! 0 |  514 | `			PH7_MemObjToString(&(*pKey));` |
|       ! 0 |  515 | `		}` |
|    125771 |  516 | `		if( SyBlobLength(&pKey->sBlob) > 0 && !HashmapIsIntKey(&pKey->sBlob) ){` |
|         - |  517 | `			/* Perform a blob lookup */` |
|    125751 |  518 | `			rc = HashmapLookupBlobKey(&(*pMap),SyBlobData(&pKey->sBlob),SyBlobLength(&pKey->sBlob),&pNode);` |
|    125751 |  519 | `			goto result;` |
|         - |  520 | `		}` |
|        10 |  521 | `	}` |
|         - |  522 | `	/* Perform an int lookup */` |
|      1655 |  523 | `	if((pKey->iFlags & MEMOBJ_INT) == 0 ){` |
|         - |  524 | `		/* Force an integer cast */` |
|        31 |  525 | `		PH7_MemObjToInteger(pKey);` |
|        15 |  526 | `	}` |
|         - |  527 | `	/* Perform an int lookup */` |
|      1655 |  528 | `	rc = HashmapLookupIntKey(&(*pMap),pKey->x.iVal,&pNode);` |
|     63698 |  529 | `result:` |
|    127401 |  530 | `	if( rc == SXRET_OK ){` |
|         - |  531 | `		/* Node found */` |
|     53713 |  532 | `		if( ppNode ){` |
|     53667 |  533 | `			*ppNode = pNode;` |
|     26831 |  534 | `		}` |
|     53713 |  535 | `		return SXRET_OK;` |
|         - |  536 | `	}` |
|         - |  537 | `	/* No such entry */` |
|     73693 |  538 | `	return SXERR_NOTFOUND;` |
|     63703 |  539 | `}` |
|         - |  540 | `/*` |
|         - |  541 | ` * Advance the auto-index after a successful insertion of int key iKey.` |
|         - |  542 | ` * Mirrors Zend's nNextFreeElement: saturates at PHP_INT_MAX (incrementing` |
|         - |  543 | ` * past it is signed overflow); the occupied-slot case errors at append time` |
|         - |  544 | ` * via HashmapAppendIndexBusy.` |
|         - |  545 | ` */` |
|   2140926 |  546 | `static void HashmapAdvanceAutoIndex(ph7_hashmap *pMap,sxi64 iKey)` |
|         5 |  547 | `{` |
|   2140931 |  548 | `	if( iKey >= pMap->iNextIdx ){` |
|   2140673 |  549 | `		pMap->iNextIdx = iKey < SXI64_HIGH ? iKey + 1 : SXI64_HIGH;` |
|         - |  550 | `		/* Make sure the automatic index is not reserved */` |
|   2140673 |  551 | `		while( pMap->iNextIdx < SXI64_HIGH && SXRET_OK == HashmapLookupIntKey(&(*pMap),pMap->iNextIdx,0) ){` |
|       ! 0 |  552 | `			pMap->iNextIdx++;` |
|       ! 0 |  553 | `		}` |
|   1070334 |  554 | `	}` |
|   2140931 |  555 | `}` |
|         - |  556 | `/*` |
|         - |  557 | `` * TRUE when an append (`$a[] = v`) cannot proceed because the saturated`` |
|         - |  558 | ` * auto-index slot (PHP_INT_MAX) is already occupied. Throws php's catchable` |
|         - |  559 | ` * Error and stores the rc the insert function must return (PH7_EXCEPTION,` |
|         - |  560 | ` * or PH7_ABORT when the Error class itself cannot be built).` |
|         - |  561 | ` */` |
|    946738 |  562 | `static sxi32 HashmapAppendIndexBusy(ph7_hashmap *pMap,sxi32 *pRc)` |
|         5 |  563 | `{` |
|    946743 |  564 | `	if( pMap->iNextIdx == SXI64_HIGH && SXRET_OK == HashmapLookupIntKey(&(*pMap),pMap->iNextIdx,0) ){` |
|         7 |  565 | `		*pRc = PH7_VmThrowArrayNextIndexError(pMap->pVm);` |
|         7 |  566 | `		return TRUE;` |
|         - |  567 | `	}` |
|    946737 |  568 | `	return FALSE;` |
|    473374 |  569 | `}` |
|         - |  570 | `/*` |
|         - |  571 | ` * Insert a given key and it's associated value (if any) in the given` |
|         - |  572 | ` * hashmap.` |
|         - |  573 | ` * If a node with the given key already exists in the database` |
|         - |  574 | ` * then this function overwrite the old value.` |
|         - |  575 | ` */` |
|   3196060 |  576 | `static sxi32 HashmapInsert(` |
|         - |  577 | `	ph7_hashmap *pMap, /* Target hashmap */` |
|         - |  578 | `	ph7_value *pKey,   /* Lookup key  */` |
|         - |  579 | `	ph7_value *pVal    /* Node value */` |
|         - |  580 | `	)` |
|         5 |  581 | `{` |
|   3196065 |  582 | `	ph7_hashmap_node *pNode = 0;` |
|   3196065 |  583 | `	sxi32 rc = SXRET_OK;` |
|   3196065 |  584 | `	if( pKey && pKey->iFlags & (MEMOBJ_STRING\|MEMOBJ_HASHMAP\|MEMOBJ_OBJ\|MEMOBJ_RES) ){` |
|    112059 |  585 | `		if( (pKey->iFlags & MEMOBJ_STRING) == 0 ){` |
|         - |  586 | `			/* Force a string cast */` |
|         3 |  587 | `			PH7_MemObjToString(&(*pKey));` |
|         1 |  588 | `		}` |
|    112059 |  589 | `		if( SyBlobLength(&pKey->sBlob) < 1 \|\| HashmapIsIntKey(&pKey->sBlob) ){` |
|      3715 |  590 | `			if(SyBlobLength(&pKey->sBlob) < 1){` |
|         - |  591 | `				/* Automatic index assign */` |
|      3493 |  592 | `				pKey = 0;` |
|      1744 |  593 | `			}` |
|      3715 |  594 | `			goto IntKey;` |
|         - |  595 | `		}` |
|    162521 |  596 | `		if( SXRET_OK == HashmapLookupBlobKey(&(*pMap),SyBlobData(&pKey->sBlob),` |
|     54172 |  597 | `			SyBlobLength(&pKey->sBlob),&pNode) ){` |
|         - |  598 | `				/* Overwrite the old value */` |
|         - |  599 | `				ph7_value *pElem;` |
|        85 |  600 | `				pElem = (ph7_value *)SySetAt(&pMap->pVm->aMemObj,pNode->nValIdx);` |
|        85 |  601 | `				if( pElem ){` |
|        85 |  602 | `					if( pVal ){` |
|        85 |  603 | `						PH7_MemObjStore(pVal,pElem);` |
|        44 |  604 | `					}else{` |
|         - |  605 | `						/* Nullify the entry */` |
|       ! 0 |  606 | `						PH7_MemObjToNull(pElem);` |
|         - |  607 | `					}` |
|        41 |  608 | `				}` |
|        85 |  609 | `				return SXRET_OK;` |
|         - |  610 | `		}` |
|    108267 |  611 | `		if( pMap == pMap->pVm->pGlobal ){` |
|         - |  612 | `			/* php 8.1: writing a new key into $GLOBALS creates a real global` |
|         - |  613 | `			 * variable ($GLOBALS stays a live view of the symbol table). */` |
|       123 |  614 | `			if( SyBlobLength(&pKey->sBlob) < 1 ){` |
|         - |  615 | `				/* Pathological empty name: keep the legacy diagnostic */` |
|       ! 0 |  616 | `				PH7_VmThrowError(pMap->pVm,0,PH7_CTX_NOTICE,"$GLOBALS is a read-only array,insertion is forbidden");` |
|       ! 0 |  617 | `				return SXRET_OK;` |
|         - |  618 | `			}` |
|       184 |  619 | `			return PH7_VmInstallGlobalVar(pMap->pVm,` |
|       122 |  620 | `				(const char *)SyBlobData(&pKey->sBlob),SyBlobLength(&pKey->sBlob),` |
|        61 |  621 | `				pVal,SXU32_HIGH);` |
|         - |  622 | `		}` |
|         - |  623 | `		/* Perform a blob-key insertion */` |
|    108145 |  624 | `		rc = HashmapInsertBlobKey(&(*pMap),SyBlobData(&pKey->sBlob),SyBlobLength(&pKey->sBlob),&(*pVal),0,FALSE);` |
|    108145 |  625 | `		return rc;` |
|         - |  626 | `	}` |
|   1542003 |  627 | `IntKey:` |
|   3087721 |  628 | `	if( pKey ){` |
|   2141013 |  629 | `		if((pKey->iFlags & MEMOBJ_INT) == 0 ){` |
|         - |  630 | `			/* Force an integer cast */` |
|       251 |  631 | `			PH7_MemObjToInteger(pKey);` |
|       125 |  632 | `		}` |
|   2141013 |  633 | `		if( SXRET_OK == HashmapLookupIntKey(&(*pMap),pKey->x.iVal,&pNode) ){` |
|         - |  634 | `			/* Overwrite the old value */` |
|         - |  635 | `			ph7_value *pElem;` |
|        87 |  636 | `			pElem = (ph7_value *)SySetAt(&pMap->pVm->aMemObj,pNode->nValIdx);` |
|        87 |  637 | `			if( pElem ){` |
|        87 |  638 | `				if( pVal ){` |
|        87 |  639 | `					PH7_MemObjStore(pVal,pElem);` |
|        44 |  640 | `				}else{` |
|         - |  641 | `					/* Nullify the entry */` |
|       ! 0 |  642 | `					PH7_MemObjToNull(pElem);` |
|         - |  643 | `				}` |
|        43 |  644 | `			}` |
|        87 |  645 | `			return SXRET_OK;` |
|         - |  646 | `		}` |
|   2140927 |  647 | `		if( pMap == pMap->pVm->pGlobal ){` |
|         - |  648 | `			/* php 8.1: an int key creates the global named by its decimal` |
|         - |  649 | `			 * form ($GLOBALS[7] = ... behaves like $GLOBALS['7'] = ...). */` |
|         - |  650 | `			char zKey[24];` |
|         3 |  651 | `			sxu32 nKey = SyBufferFormat(zKey,sizeof(zKey),"%qd",pKey->x.iVal);` |
|         3 |  652 | `			return PH7_VmInstallGlobalVar(pMap->pVm,zKey,nKey,pVal,SXU32_HIGH);` |
|         - |  653 | `		}` |
|         - |  654 | `		/* Perform a 64-bit-int-key insertion */` |
|   2140925 |  655 | `		rc = HashmapInsertIntKey(&(*pMap),pKey->x.iVal,&(*pVal),0,FALSE);` |
|   2140925 |  656 | `		if( rc == SXRET_OK ){` |
|   2140925 |  657 | `			HashmapAdvanceAutoIndex(&(*pMap),pKey->x.iVal);` |
|   1070460 |  658 | `		}` |
|   1070465 |  659 | `	}else{` |
|    946713 |  660 | `		if( pMap == pMap->pVm->pGlobal ){` |
|         - |  661 | `			/* php's catchable Error: Cannot append to $GLOBALS */` |
|         3 |  662 | `			return PH7_VmThrowGlobalsAppendError(pMap->pVm);` |
|         - |  663 | `		}` |
|    946711 |  664 | `		if( HashmapAppendIndexBusy(&(*pMap),&rc) ){` |
|         7 |  665 | `			return rc; /* PH7_EXCEPTION/PH7_ABORT: php's catchable Error was thrown */` |
|         - |  666 | `		}` |
|         - |  667 | `		/* Assign an automatic index */` |
|    946705 |  668 | `		rc = HashmapInsertIntKey(&(*pMap),pMap->iNextIdx,&(*pVal),0,FALSE);` |
|    946705 |  669 | `		if( rc == SXRET_OK && pMap->iNextIdx < SXI64_HIGH ){` |
|    946703 |  670 | `			++pMap->iNextIdx;` |
|    473349 |  671 | `		}` |
|         - |  672 | `	}` |
|         - |  673 | `	/* Insertion result */` |
|   3087625 |  674 | `	return rc;` |
|   1598035 |  675 | `}` |
|         - |  676 | `/*` |
|         - |  677 | ` * Insert a given key and it's associated value (foreign index) in the given` |
|         - |  678 | ` * hashmap.` |
|         - |  679 | ` * This is insertion by reference so be careful to mark the node` |
|         - |  680 | ` * with the HASHMAP_NODE_FOREIGN_OBJ flag being set.` |
|         - |  681 | ` * The insertion by reference is triggered when the following` |
|         - |  682 | ` * expression is encountered.` |
|         - |  683 | ` * $var = 10;` |
|         - |  684 | ` *  $a = array(&var);` |
|         - |  685 | ` * OR` |
|         - |  686 | ` *  $a[] =& $var;` |
|         - |  687 | ` * That is,$var is a foreign ph7_value and the $a array have no control` |
|         - |  688 | ` * over it's contents.` |
|         - |  689 | ` * Note that the node that hold the foreign ph7_value is automatically` |
|         - |  690 | ` * removed when the foreign ph7_value is unset.` |
|         - |  691 | ` * Example:` |
|         - |  692 | ` *  $var = 10;` |
|         - |  693 | ` *  $a[] =& $var;` |
|         - |  694 | ` *  echo count($a).PHP_EOL; //1` |
|         - |  695 | ` *  //Unset the foreign ph7_value now` |
|         - |  696 | ` *  unset($var);` |
|         - |  697 | ` *  echo count($a); //0` |
|         - |  698 | ` * Note that this is a PH7 eXtension.` |
|         - |  699 | ` * Refer to the official documentation for more information.` |
|         - |  700 | ` * If a node with the given key already exists in the database` |
|         - |  701 | ` * then this function overwrite the old value.` |
|         - |  702 | ` */` |
|     46286 |  703 | `static sxi32 HashmapInsertByRef(` |
|         - |  704 | `	ph7_hashmap *pMap,   /* Target hashmap */` |
|         - |  705 | `	ph7_value *pKey,     /* Lookup key */` |
|         - |  706 | `	sxu32 nRefIdx        /* Foreign ph7_value index */` |
|         - |  707 | `	)` |
|         5 |  708 | `{` |
|     46291 |  709 | `	ph7_hashmap_node *pNode = 0;` |
|     46291 |  710 | `	sxi32 rc = SXRET_OK;` |
|     46291 |  711 | `	if( pKey && pKey->iFlags & (MEMOBJ_STRING\|MEMOBJ_HASHMAP\|MEMOBJ_OBJ\|MEMOBJ_RES) ){` |
|     46255 |  712 | `		if( (pKey->iFlags & MEMOBJ_STRING) == 0 ){` |
|         - |  713 | `			/* Force a string cast */` |
|       ! 0 |  714 | `			PH7_MemObjToString(&(*pKey));` |
|       ! 0 |  715 | `		}` |
|     46255 |  716 | `		if( SyBlobLength(&pKey->sBlob) < 1 \|\| HashmapIsIntKey(&pKey->sBlob) ){` |
|         3 |  717 | `			if(SyBlobLength(&pKey->sBlob) < 1){` |
|         - |  718 | `				/* Automatic index assign */` |
|       ! 0 |  719 | `				pKey = 0;` |
|       ! 0 |  720 | `			}` |
|         3 |  721 | `			goto IntKey;` |
|         - |  722 | `		}` |
|     69377 |  723 | `		if( SXRET_OK == HashmapLookupBlobKey(&(*pMap),SyBlobData(&pKey->sBlob),` |
|     23124 |  724 | `			SyBlobLength(&pKey->sBlob),&pNode) ){` |
|         - |  725 | `				/* Overwrite */` |
|         7 |  726 | `				PH7_VmRefObjRemove(pMap->pVm,pNode->nValIdx,0,pNode);` |
|         7 |  727 | `				pNode->nValIdx = nRefIdx;` |
|         - |  728 | `				/* Install in the reference table */` |
|         7 |  729 | `				PH7_VmRefObjInstall(pMap->pVm,nRefIdx,0,pNode,0);` |
|         7 |  730 | `				return SXRET_OK;` |
|         - |  731 | `		}` |
|         - |  732 | `		/* Perform a blob-key insertion */` |
|     46247 |  733 | `		rc = HashmapInsertBlobKey(&(*pMap),SyBlobData(&pKey->sBlob),SyBlobLength(&pKey->sBlob),0,nRefIdx,TRUE);` |
|     46247 |  734 | `		return rc;` |
|         - |  735 | `	}` |
|        18 |  736 | `IntKey:` |
|        39 |  737 | `	if( pKey ){` |
|         7 |  738 | `		if((pKey->iFlags & MEMOBJ_INT) == 0 ){` |
|         - |  739 | `			/* Force an integer cast */` |
|         3 |  740 | `			PH7_MemObjToInteger(pKey);` |
|         1 |  741 | `		}` |
|         7 |  742 | `		if( SXRET_OK == HashmapLookupIntKey(&(*pMap),pKey->x.iVal,&pNode) ){` |
|         - |  743 | `			/* Overwrite */` |
|       ! 0 |  744 | `			PH7_VmRefObjRemove(pMap->pVm,pNode->nValIdx,0,pNode);` |
|       ! 0 |  745 | `			pNode->nValIdx = nRefIdx;` |
|         - |  746 | `			/* Install in the reference table */` |
|       ! 0 |  747 | `			PH7_VmRefObjInstall(pMap->pVm,nRefIdx,0,pNode,0);` |
|       ! 0 |  748 | `			return SXRET_OK;` |
|         - |  749 | `		}` |
|         - |  750 | `		/* Perform a 64-bit-int-key insertion */` |
|         7 |  751 | `		rc = HashmapInsertIntKey(&(*pMap),pKey->x.iVal,0,nRefIdx,TRUE);` |
|         7 |  752 | `		if( rc == SXRET_OK ){` |
|         7 |  753 | `			HashmapAdvanceAutoIndex(&(*pMap),pKey->x.iVal);` |
|         3 |  754 | `		}` |
|         4 |  755 | `	}else{` |
|        33 |  756 | `		if( HashmapAppendIndexBusy(&(*pMap),&rc) ){` |
|       ! 0 |  757 | `			return rc; /* PH7_EXCEPTION/PH7_ABORT: php's catchable Error was thrown */` |
|         - |  758 | `		}` |
|         - |  759 | `		/* Assign an automatic index */` |
|        33 |  760 | `		rc = HashmapInsertIntKey(&(*pMap),pMap->iNextIdx,0,nRefIdx,TRUE);` |
|        33 |  761 | `		if( rc == SXRET_OK && pMap->iNextIdx < SXI64_HIGH ){` |
|        33 |  762 | `			++pMap->iNextIdx;` |
|        16 |  763 | `		}` |
|         - |  764 | `	}` |
|         - |  765 | `	/* Insertion result */` |
|        39 |  766 | `	return rc;` |
|     23148 |  767 | `}` |
|         - |  768 | `/*` |
|         - |  769 | ` * Extract node value.` |
|         - |  770 | ` */` |
|   1355437 |  771 | `static ph7_value * HashmapExtractNodeValue(ph7_hashmap_node *pNode)` |
|         5 |  772 | `{` |
|         - |  773 | `	/* Point to the desired object */` |
|         - |  774 | `	ph7_value *pObj;` |
|   1355442 |  775 | `	pObj = (ph7_value *)SySetAt(&pNode->pMap->pVm->aMemObj,pNode->nValIdx);` |
|   1355442 |  776 | `	return pObj;` |
|         5 |  777 | `}` |
|         - |  778 | `/*` |
|         - |  779 | ` * Insert a node in the given hashmap.` |
|         - |  780 | ` * If a node with the given key already exists in the database` |
|         - |  781 | ` * then this function overwrite the old value.` |
|         - |  782 | ` */` |
|       446 |  783 | `static sxi32 HashmapInsertNode(ph7_hashmap *pMap,ph7_hashmap_node *pNode,int bPreserve)` |
|         5 |  784 | `{` |
|         - |  785 | `	ph7_value *pObj;` |
|         - |  786 | `	sxi32 rc;` |
|         - |  787 | `	/* Extract the node value */` |
|       451 |  788 | `	pObj = HashmapExtractNodeValue(&(*pNode));` |
|       451 |  789 | `	if( pObj == 0 ){` |
|       ! 0 |  790 | `		return SXERR_EMPTY;` |
|         - |  791 | `	}` |
|         - |  792 | `	/* Preserve key */` |
|       451 |  793 | `	if( pNode->iType == HASHMAP_INT_NODE){` |
|         - |  794 | `		/* Int64 key */` |
|       321 |  795 | `		if( !bPreserve ){` |
|         - |  796 | `			/* Assign an automatic index */` |
|       173 |  797 | `			rc = HashmapInsert(&(*pMap),0,pObj);` |
|        89 |  798 | `		}else{` |
|       149 |  799 | `			rc = HashmapInsertIntKey(&(*pMap),pNode->xKey.iKey,pObj,0,FALSE);` |
|         - |  800 | `		}` |
|       163 |  801 | `	}else{` |
|         - |  802 | `		/* Blob key */` |
|       131 |  803 | `		if( !bPreserve ){` |
|         - |  804 | `			/* treat it like an automatically-indexed element, drop the` |
|         - |  805 | `			 * original string key entirely */` |
|        35 |  806 | `			rc = HashmapInsert(&(*pMap),0,pObj);` |
|        18 |  807 | `		}else{` |
|       145 |  808 | `			rc = HashmapInsertBlobKey(&(*pMap),SyBlobData(&pNode->xKey.sKey),` |
|        48 |  809 | `				SyBlobLength(&pNode->xKey.sKey),pObj,0,FALSE);` |
|         - |  810 | `		}` |
|         - |  811 | `	}` |
|       451 |  812 | `	return rc;` |
|       228 |  813 | `}` |
|         - |  814 | `/*` |
|         - |  815 | ` * Compare two node values.` |
|         - |  816 | ` * Return 0 if the node values are equals, > 0 if pLeft is greater than pRight` |
|         - |  817 | ` * or < 0 if pRight is greater than pLeft.` |
|         - |  818 | ` * For a full description on ph7_values comparison,refer to the implementation` |
|         - |  819 | ` * of the [PH7_MemObjCmp()] function defined in memobj.c or the official` |
|         - |  820 | ` * documenation.` |
|         - |  821 | ` */` |
|     69235 |  822 | `static sxi32 HashmapNodeCmp(ph7_hashmap_node *pLeft,ph7_hashmap_node *pRight,int bStrict)` |
|         5 |  823 | `{` |
|         - |  824 | `	ph7_value sObj1,sObj2;` |
|         - |  825 | `	sxi32 rc;` |
|     69240 |  826 | `	if( pLeft == pRight ){` |
|         - |  827 | `		/*` |
|         - |  828 | `		 * Same node.Refer to the sort() implementation defined` |
|         - |  829 | `		 * below for more information on this sceanario.` |
|         - |  830 | `		 */` |
|       ! 0 |  831 | `		return 0;` |
|         - |  832 | `	}` |
|         - |  833 | `	/* Do the comparison */` |
|     69240 |  834 | `	PH7_MemObjInit(pLeft->pMap->pVm,&sObj1);` |
|     69240 |  835 | `	PH7_MemObjInit(pLeft->pMap->pVm,&sObj2);` |
|     69240 |  836 | `	PH7_HashmapExtractNodeValue(pLeft,&sObj1,FALSE);` |
|     69240 |  837 | `	PH7_HashmapExtractNodeValue(pRight,&sObj2,FALSE);` |
|     69240 |  838 | `	rc = PH7_MemObjCmp(&sObj1,&sObj2,bStrict,0);` |
|     69240 |  839 | `	PH7_MemObjRelease(&sObj1);` |
|     69240 |  840 | `	PH7_MemObjRelease(&sObj2);` |
|     69240 |  841 | `	return rc;` |
|     34636 |  842 | `}` |
|         - |  843 | `/*` |
|         - |  844 | ` * Rehash a node with a 64-bit integer key.` |
|         - |  845 | ` * Refer to [merge_sort(),array_shift()] implementations for more information.` |
|         - |  846 | ` */` |
|     13310 |  847 | `static void HashmapRehashIntNode(ph7_hashmap_node *pEntry)` |
|         5 |  848 | `{` |
|     13315 |  849 | `	ph7_hashmap *pMap = pEntry->pMap;` |
|         - |  850 | `	sxu32 nBucket;` |
|         - |  851 | `	/* Remove old collision links */` |
|     13315 |  852 | `	if( pEntry->pPrevCollide ){` |
|     10902 |  853 | `		pEntry->pPrevCollide->pNextCollide = pEntry->pNextCollide;` |
|      5455 |  854 | `	}else{` |
|      2418 |  855 | `		pMap->apBucket[pEntry->nHash & (pMap->nSize - 1)] = pEntry->pNextCollide;` |
|         - |  856 | `	}` |
|     13315 |  857 | `	if( pEntry->pNextCollide ){` |
|      1081 |  858 | `		pEntry->pNextCollide->pPrevCollide = pEntry->pPrevCollide;` |
|       552 |  859 | `	}` |
|     13315 |  860 | `	pEntry->pNextCollide = pEntry->pPrevCollide = 0;` |
|         - |  861 | `	/* Compute the new hash */` |
|     13315 |  862 | `	pEntry->nHash = pMap->xIntHash(pMap->iNextIdx);` |
|     13315 |  863 | `	pEntry->xKey.iKey = pMap->iNextIdx;` |
|     13315 |  864 | `	nBucket = pEntry->nHash & (pMap->nSize - 1);` |
|         - |  865 | `	/* Link to the new bucket */` |
|     13315 |  866 | `	pEntry->pNextCollide = pMap->apBucket[nBucket];` |
|     13315 |  867 | `	if( pMap->apBucket[nBucket] ){` |
|     11214 |  868 | `		pMap->apBucket[nBucket]->pPrevCollide = pEntry;` |
|      5606 |  869 | `	}` |
|     13315 |  870 | `	pEntry->pNextCollide = pMap->apBucket[nBucket];` |
|     13315 |  871 | `	pMap->apBucket[nBucket] = pEntry;` |
|         - |  872 | `	/* Increment the automatic index (saturating, like every other advance —` |
|         - |  873 | `	 * unreachable in practice since renumbering assigns 0..nEntry-1, but keep` |
|         - |  874 | `	 * the no-overflow invariant uniform). */` |
|     13315 |  875 | `	if( pMap->iNextIdx < SXI64_HIGH ){` |
|     13315 |  876 | `		pMap->iNextIdx++;` |
|      6655 |  877 | `	}` |
|     13315 |  878 | `}` |
|         - |  879 | `/*` |
|         - |  880 | ` * Perform a linear search on a given hashmap.` |
|         - |  881 | ` * Write a pointer to the target node on success.` |
|         - |  882 | ` * Otherwise SXERR_NOTFOUND is returned on failure.` |
|         - |  883 | ` * Refer to [array_intersect(),array_diff(),in_array(),...] implementations` |
|         - |  884 | ` * for more information.` |
|         - |  885 | ` */` |
|     32420 |  886 | `static int HashmapFindValue(` |
|         - |  887 | `	ph7_hashmap *pMap,   /* Target hashmap */` |
|         - |  888 | `	ph7_value *pNeedle,  /* Lookup key */` |
|         - |  889 | `	ph7_hashmap_node **ppNode, /* OUT: target node on success  */` |
|         - |  890 | `	int bStrict      /* TRUE for strict comparison */` |
|         - |  891 | `	)` |
|         5 |  892 | `{` |
|         - |  893 | `	ph7_hashmap_node *pEntry;` |
|         - |  894 | `	ph7_value sVal,*pVal;` |
|         - |  895 | `	ph7_value sNeedle;` |
|         - |  896 | `	sxi32 rc;` |
|         - |  897 | `	sxu32 n;` |
|         - |  898 | `	/* Perform a linear search since we cannot sort the hashmap based on values */` |
|     32425 |  899 | `	pEntry = pMap->pFirst;` |
|     32425 |  900 | `	n = pMap->nEntry;` |
|     32425 |  901 | `	PH7_MemObjInit(pMap->pVm,&sVal);` |
|     32425 |  902 | `	PH7_MemObjInit(pMap->pVm,&sNeedle);` |
|     77050 |  903 | `	for(;;){` |
|    154108 |  904 | `		if( n < 1 ){` |
|        99 |  905 | `			break;` |
|         - |  906 | `		}` |
|         - |  907 | `		/* Extract node value */` |
|    154010 |  908 | `		pVal = HashmapExtractNodeValue(pEntry);` |
|    154010 |  909 | `		if( pVal ){` |
|    154010 |  910 | `			if( (pVal->iFlags\|pNeedle->iFlags) & MEMOBJ_NULL ){` |
|       ! 0 |  911 | `				sxi32 iF1 = pVal->iFlags&~MEMOBJ_AUX;` |
|       ! 0 |  912 | `				sxi32 iF2 = pNeedle->iFlags&~MEMOBJ_AUX;` |
|       ! 0 |  913 | `				if( iF1 == iF2 ){` |
|         - |  914 | `					/* NULL values are equals */` |
|       ! 0 |  915 | `					if( ppNode ){` |
|       ! 0 |  916 | `						*ppNode = pEntry;` |
|       ! 0 |  917 | `					}` |
|       ! 0 |  918 | `					return SXRET_OK;` |
|         - |  919 | `				}` |
|       ! 0 |  920 | `			}else{` |
|         - |  921 | `				/* Duplicate value */` |
|    154010 |  922 | `				PH7_MemObjLoad(pVal,&sVal);` |
|    154010 |  923 | `				PH7_MemObjLoad(pNeedle,&sNeedle);` |
|    154010 |  924 | `				rc = PH7_MemObjCmp(&sNeedle,&sVal,bStrict,0);` |
|    154010 |  925 | `				PH7_MemObjRelease(&sVal);` |
|    154010 |  926 | `				PH7_MemObjRelease(&sNeedle);` |
|    154010 |  927 | `				if( rc == 0 ){` |
|     32327 |  928 | `					if( ppNode ){` |
|        23 |  929 | `						*ppNode = pEntry;` |
|        11 |  930 | `					}` |
|         - |  931 | `					/* Match found*/` |
|     32327 |  932 | `					return SXRET_OK;` |
|         - |  933 | `				}` |
|         - |  934 | `			}` |
|     60840 |  935 | `		}` |
|         - |  936 | `		/* Point to the next entry */` |
|    121688 |  937 | `		pEntry = pEntry->pPrev; /* Reverse link */` |
|    121688 |  938 | `		n--;` |
|         5 |  939 | `	}` |
|         - |  940 | `	/* No such entry */` |
|        99 |  941 | `	return SXERR_NOTFOUND;` |
|     16215 |  942 | `}` |
|         - |  943 | `/*` |
|         - |  944 | ` * Perform a linear search on a given hashmap but use an user-defined callback` |
|         - |  945 | ` * for values comparison.` |
|         - |  946 | ` * Write a pointer to the target node on success.` |
|         - |  947 | ` * Otherwise SXERR_NOTFOUND is returned on failure.` |
|         - |  948 | ` * Refer to [array_uintersect(),array_udiff()...] implementations` |
|         - |  949 | ` * for more information.` |
|         - |  950 | ` */` |
|        22 |  951 | `static int HashmapFindValueByCallback(` |
|         - |  952 | `	ph7_hashmap *pMap,     /* Target hashmap */` |
|         - |  953 | `	ph7_value *pNeedle,    /* Lookup key */` |
|         - |  954 | `	ph7_value *pCallback,  /* User defined callback */` |
|         - |  955 | `	ph7_hashmap_node **ppNode /* OUT: target node on success */` |
|         - |  956 | `	)` |
|         1 |  957 | `{` |
|         - |  958 | `	ph7_hashmap_node *pEntry;` |
|         - |  959 | `	ph7_value sResult,*pVal;` |
|         - |  960 | `	ph7_value *apArg[2];    /* Callback arguments */` |
|         - |  961 | `	sxi32 rc;` |
|         - |  962 | `	sxu32 n;` |
|        23 |  963 | `	if( pMap->pVm->iCmpCallbackExc ){` |
|         - |  964 | `		/* A previous comparison already raised: stop invoking the callback so the` |
|         - |  965 | `		 * exception is not thrown again, and let the caller wind down. */` |
|       ! 0 |  966 | `		return SXERR_NOTFOUND;` |
|         - |  967 | `	}` |
|         - |  968 | `	/* Perform a linear search since we cannot sort the array based on values */` |
|        23 |  969 | `	pEntry = pMap->pFirst;` |
|        23 |  970 | `	n = pMap->nEntry;` |
|         - |  971 | `	/* Store callback result here */` |
|        23 |  972 | `	PH7_MemObjInit(pMap->pVm,&sResult);` |
|         - |  973 | `	/* First argument to the callback */` |
|        23 |  974 | `	apArg[0] = pNeedle;` |
|        25 |  975 | `	for(;;){` |
|        51 |  976 | `		if( n < 1 ){` |
|         9 |  977 | `			break;` |
|         - |  978 | `		}` |
|         - |  979 | `		/* Extract node value */` |
|        43 |  980 | `		pVal = HashmapExtractNodeValue(pEntry);` |
|        43 |  981 | `		if( pVal ){` |
|         - |  982 | `			/* Invoke the user callback */` |
|        43 |  983 | `			apArg[1] = pVal; /* Second argument to the callback */` |
|        43 |  984 | `			rc = PH7_VmCallUserFunction(pMap->pVm,pCallback,2,apArg,&sResult);` |
|        43 |  985 | `			if( rc == PH7_EXCEPTION ){` |
|         - |  986 | `				/* The callback raised: flag it so the caller aborts and propagates,` |
|         - |  987 | `				 * and report no match for the rest of the run. */` |
|         5 |  988 | `				pMap->pVm->iCmpCallbackExc = 1;` |
|         5 |  989 | `				PH7_MemObjRelease(&sResult);` |
|         5 |  990 | `				return SXERR_NOTFOUND;` |
|         - |  991 | `			}` |
|        39 |  992 | `			if( rc == SXRET_OK ){` |
|         - |  993 | `				/* Extract callback result */` |
|        39 |  994 | `				if( (sResult.iFlags & MEMOBJ_INT) == 0 ){` |
|         - |  995 | `					/* Perform an int cast */` |
|       ! 0 |  996 | `					PH7_MemObjToInteger(&sResult);` |
|       ! 0 |  997 | `				}` |
|        39 |  998 | `				rc = (sxi32)sResult.x.iVal;` |
|        39 |  999 | `				PH7_MemObjRelease(&sResult);` |
|        39 | 1000 | `				if( rc == 0 ){` |
|         - | 1001 | `					/* Match found*/` |
|        11 | 1002 | `					if( ppNode ){` |
|       ! 0 | 1003 | `						*ppNode = pEntry;` |
|       ! 0 | 1004 | `					}` |
|        11 | 1005 | `					return SXRET_OK;` |
|         - | 1006 | `				}` |
|        14 | 1007 | `			}` |
|        14 | 1008 | `		}` |
|         - | 1009 | `		/* Point to the next entry */` |
|        29 | 1010 | `		pEntry = pEntry->pPrev; /* Reverse link */` |
|        29 | 1011 | `		n--;` |
|         1 | 1012 | `	}` |
|         - | 1013 | `	/* No such entry */` |
|         9 | 1014 | `	return SXERR_NOTFOUND;` |
|        12 | 1015 | `}` |
|         - | 1016 | `/*` |
|         - | 1017 | ` * Compare two hashmaps.` |
|         - | 1018 | ` * Return 0 if the hashmaps are equals.Any other value indicates inequality.` |
|         - | 1019 | ` * Note on array comparison operators.` |
|         - | 1020 | ` *  According to the PHP language reference manual.` |
|         - | 1021 | ` *  Array Operators Example 	Name 	Result` |
|         - | 1022 | ` *  $a + $b 	Union 	Union of $a and $b.` |
|         - | 1023 | ` *  $a == $b 	Equality 	TRUE if $a and $b have the same key/value pairs.` |
|         - | 1024 | ` *  $a === $b 	Identity 	TRUE if $a and $b have the same key/value pairs in the same` |
|         - | 1025 | ` *                          order and of the same types.` |
|         - | 1026 | ` *  $a != $b 	Inequality 	TRUE if $a is not equal to $b.` |
|         - | 1027 | ` *  $a <> $b 	Inequality 	TRUE if $a is not equal to $b.` |
|         - | 1028 | ` *  $a !== $b 	Non-identity 	TRUE if $a is not identical to $b.` |
|         - | 1029 | ` * The + operator returns the right-hand array appended to the left-hand array;` |
|         - | 1030 | ` * For keys that exist in both arrays, the elements from the left-hand array will be used` |
|         - | 1031 | ` * and the matching elements from the right-hand array will be ignored.` |
|         - | 1032 | ` * <?php` |
|         - | 1033 | ` * $a = array("a" => "apple", "b" => "banana");` |
|         - | 1034 | ` * $b = array("a" => "pear", "b" => "strawberry", "c" => "cherry");` |
|         - | 1035 | ` * $c = $a + $b; // Union of $a and $b` |
|         - | 1036 | ` * echo "Union of \$a and \$b: \n";` |
|         - | 1037 | ` * var_dump($c);` |
|         - | 1038 | ` * $c = $b + $a; // Union of $b and $a` |
|         - | 1039 | ` * echo "Union of \$b and \$a: \n";` |
|         - | 1040 | ` * var_dump($c);` |
|         - | 1041 | ` * ?>` |
|         - | 1042 | ` * When executed, this script will print the following:` |
|         - | 1043 | ` * Union of $a and $b:` |
|         - | 1044 | ` * array(3) {` |
|         - | 1045 | ` *  ["a"]=>` |
|         - | 1046 | ` *  string(5) "apple"` |
|         - | 1047 | ` *  ["b"]=>` |
|         - | 1048 | ` * string(6) "banana"` |
|         - | 1049 | ` *  ["c"]=>` |
|         - | 1050 | ` * string(6) "cherry"` |
|         - | 1051 | ` * }` |
|         - | 1052 | ` * Union of $b and $a:` |
|         - | 1053 | ` * array(3) {` |
|         - | 1054 | ` * ["a"]=>` |
|         - | 1055 | ` * string(4) "pear"` |
|         - | 1056 | ` * ["b"]=>` |
|         - | 1057 | ` * string(10) "strawberry"` |
|         - | 1058 | ` * ["c"]=>` |
|         - | 1059 | ` * string(6) "cherry"` |
|         - | 1060 | ` * }` |
|         - | 1061 | ` * Elements of arrays are equal for the comparison if they have the same key and value.` |
|         - | 1062 | ` */` |
|        28 | 1063 | `PH7_PRIVATE sxi32 PH7_HashmapCmp(` |
|         - | 1064 | `	ph7_hashmap *pLeft,  /* Left hashmap */` |
|         - | 1065 | `	ph7_hashmap *pRight, /* Right hashmap */` |
|         - | 1066 | `	int bStrict          /* TRUE for strict comparison */` |
|         - | 1067 | `	)` |
|         1 | 1068 | `{` |
|         - | 1069 | `	ph7_hashmap_node *pLe,*pRe;` |
|         - | 1070 | `	sxi32 rc;` |
|         - | 1071 | `	sxu32 n;` |
|        29 | 1072 | `	if( pLeft == pRight ){` |
|         - | 1073 | `		/* Same hashmap instance. This can easily happen since hashmaps are passed by reference.` |
|         - | 1074 | `		 * Unlike the zend engine.` |
|         - | 1075 | `		 */` |
|       ! 0 | 1076 | `		return 0;` |
|         - | 1077 | `	}` |
|        29 | 1078 | `	if( pLeft->nEntry != pRight->nEntry ){` |
|         - | 1079 | `		/* Must have the same number of entries */` |
|         5 | 1080 | `		return pLeft->nEntry > pRight->nEntry ? 1 : -1;` |
|         - | 1081 | `	}` |
|         - | 1082 | `	/* Point to the first inserted entry of the left hashmap */` |
|        25 | 1083 | `	pLe = pLeft->pFirst;` |
|        25 | 1084 | `	pRe = 0; /* cc warning */` |
|         - | 1085 | `	/* Perform the comparison */` |
|        25 | 1086 | `	n = pLeft->nEntry;` |
|        59 | 1087 | `	for(;;){` |
|       119 | 1088 | `		if( n < 1 ){` |
|        23 | 1089 | `			break;` |
|         - | 1090 | `		}` |
|        97 | 1091 | `		if( pLe->iType == HASHMAP_INT_NODE){` |
|         - | 1092 | `			/* Int key */` |
|        89 | 1093 | `			rc = HashmapLookupIntKey(&(*pRight),pLe->xKey.iKey,&pRe);` |
|        45 | 1094 | `		}else{` |
|         9 | 1095 | `			SyBlob *pKey = &pLe->xKey.sKey;` |
|         - | 1096 | `			/* Blob key */` |
|         9 | 1097 | `			rc = HashmapLookupBlobKey(&(*pRight),SyBlobData(pKey),SyBlobLength(pKey),&pRe);` |
|         - | 1098 | `		}` |
|        97 | 1099 | `		if( rc != SXRET_OK ){` |
|         - | 1100 | `			/* No such entry in the right side */` |
|       ! 0 | 1101 | `			return 1;` |
|         - | 1102 | `		}` |
|        97 | 1103 | `		rc = 0;` |
|        97 | 1104 | `		if( bStrict ){` |
|         - | 1105 | `			/* Make sure,the keys are of the same type */` |
|        81 | 1106 | `			if( pLe->iType != pRe->iType ){` |
|       ! 0 | 1107 | `				rc = 1;` |
|       ! 0 | 1108 | `			}` |
|        40 | 1109 | `		}` |
|        97 | 1110 | `		if( !rc ){` |
|         - | 1111 | `			/* Compare nodes */` |
|        97 | 1112 | `			rc = HashmapNodeCmp(pLe,pRe,bStrict);` |
|        48 | 1113 | `		}` |
|        97 | 1114 | `		if( rc != 0 ){` |
|         - | 1115 | `			/* Nodes key/value differ */` |
|         3 | 1116 | `			return rc;` |
|         - | 1117 | `		}` |
|         - | 1118 | `		/* Point to the next entry */` |
|        95 | 1119 | `		pLe = pLe->pPrev; /* Reverse link */` |
|        95 | 1120 | `		n--;` |
|         1 | 1121 | `	}` |
|        23 | 1122 | `	return 0; /* Hashmaps are equals */` |
|        15 | 1123 | `}` |
|         - | 1124 | `/*` |
|         - | 1125 | ` * Duplicate a hashmap node.` |
|         - | 1126 | ` * This function is used by HashmapMerge, HashmapOverwrite and PH7_HashmapDup.` |
|         - | 1127 | ` */` |
|    630934 | 1128 | `static sxi32 HashmapDuplicateNode(` |
|         - | 1129 | `	ph7_hashmap *pDest,` |
|         - | 1130 | `	ph7_hashmap_node *pEntry,` |
|         - | 1131 | `	ph7_value *pVal,` |
|         - | 1132 | `	int iAction /* 0: Merge, 1: Overwrite, 2: Dup */` |
|         - | 1133 | `	)` |
|         5 | 1134 | `{` |
|         - | 1135 | `	ph7_value sSafeVal;` |
|         - | 1136 | `	ph7_value sKey;` |
|         - | 1137 | `	sxi32 rc;` |
|         - | 1138 |  |
|    630939 | 1139 | `	if( pEntry->iFlags & HASHMAP_NODE_FOREIGN_OBJ ){` |
|         - | 1140 | `		/* The source node holds a reference to a foreign ph7_value (e.g: [&$x]).` |
|         - | 1141 | `		 * Re-insert it by reference so the reference survives the duplication` |
|         - | 1142 | `		 * instead of being flattened to a value copy. This keeps spread` |
|         - | 1143 | `		 * ([...$a]), array_merge(), array_replace() and array copies in sync` |
|         - | 1144 | `		 * with PHP semantics. */` |
|         7 | 1145 | `		sxu32 nRefIdx = pEntry->nValIdx;` |
|         7 | 1146 | `		if( pEntry->iType == HASHMAP_BLOB_NODE ){` |
|         3 | 1147 | `			PH7_MemObjInitFromString(pDest->pVm,&sKey,0);` |
|         3 | 1148 | `			PH7_MemObjStringAppend(&sKey,(const char *)SyBlobData(&pEntry->xKey.sKey),SyBlobLength(&pEntry->xKey.sKey));` |
|         3 | 1149 | `			rc = HashmapInsertByRef(pDest,&sKey,nRefIdx);` |
|         3 | 1150 | `			PH7_MemObjRelease(&sKey);` |
|         2 | 1151 | `		}else{` |
|         5 | 1152 | `			if( iAction == 0 ){ /* Merge: automatic index assign */` |
|         5 | 1153 | `				rc = HashmapInsertByRef(pDest,0,nRefIdx);` |
|         2 | 1154 | `			}else if( iAction == 1 ){ /* Overwrite: keep the int key */` |
|       ! 0 | 1155 | `				PH7_MemObjInitFromInt(pDest->pVm,&sKey,pEntry->xKey.iKey);` |
|       ! 0 | 1156 | `				rc = HashmapInsertByRef(pDest,&sKey,nRefIdx);` |
|       ! 0 | 1157 | `				PH7_MemObjRelease(&sKey);` |
|       ! 0 | 1158 | `			}else{ /* Dup: preserve the int key */` |
|       ! 0 | 1159 | `				rc = HashmapInsertIntKey(pDest,pEntry->xKey.iKey,0,nRefIdx,TRUE);` |
|         - | 1160 | `			}` |
|         - | 1161 | `		}` |
|         7 | 1162 | `		return rc;` |
|         - | 1163 | `	}` |
|    630933 | 1164 | `	sSafeVal = *pVal;` |
|         - | 1165 |  |
|    630933 | 1166 | `	if( pEntry->iType == HASHMAP_BLOB_NODE ){` |
|         - | 1167 | `		/* Blob key insertion */` |
|      4027 | 1168 | `		PH7_MemObjInitFromString(pDest->pVm,&sKey,0);` |
|      4027 | 1169 | `		PH7_MemObjStringAppend(&sKey,(const char *)SyBlobData(&pEntry->xKey.sKey),SyBlobLength(&pEntry->xKey.sKey));` |
|      4027 | 1170 | `		rc = PH7_HashmapInsert(pDest,&sKey,&sSafeVal);` |
|      4027 | 1171 | `		PH7_MemObjRelease(&sKey);` |
|      2016 | 1172 | `	}else{` |
|         - | 1173 | `		/* Int key */` |
|    626911 | 1174 | `		if( iAction == 0 ){ /* Merge */` |
|    626697 | 1175 | `			rc = HashmapInsert(pDest,0/* Automatic index assign */,&sSafeVal);` |
|    313563 | 1176 | `		}else if( iAction == 1 ){ /* Overwrite */` |
|        32 | 1177 | `			PH7_MemObjInitFromInt(pDest->pVm,&sKey,pEntry->xKey.iKey);` |
|        32 | 1178 | `			rc = PH7_HashmapInsert(pDest,&sKey,&sSafeVal);` |
|        32 | 1179 | `			PH7_MemObjRelease(&sKey);` |
|        17 | 1180 | `		}else{ /* Dup */` |
|       186 | 1181 | `			rc = HashmapInsertIntKey(pDest,pEntry->xKey.iKey,&sSafeVal,0,FALSE);` |
|         - | 1182 | `		}` |
|         - | 1183 | `	}` |
|    630933 | 1184 | `	return rc;` |
|    315472 | 1185 | `}` |
|         - | 1186 | `/*` |
|         - | 1187 | ` * Merge two hashmaps.` |
|         - | 1188 | ` * Note on the merge process` |
|         - | 1189 | ` * According to the PHP language reference manual.` |
|         - | 1190 | ` *  Merges the elements of two arrays together so that the values of one are appended` |
|         - | 1191 | ` *  to the end of the previous one. It returns the resulting array (pDest).` |
|         - | 1192 | ` *  If the input arrays have the same string keys, then the later value for that key` |
|         - | 1193 | ` *  will overwrite the previous one. If, however, the arrays contain numeric keys` |
|         - | 1194 | ` *  the later value will not overwrite the original value, but will be appended.` |
|         - | 1195 | ` *  Values in the input array with numeric keys will be renumbered with incrementing` |
|         - | 1196 | ` *  keys starting from zero in the result array.` |
|         - | 1197 | ` */` |
|      2116 | 1198 | `static sxi32 HashmapMerge(ph7_hashmap *pSrc,ph7_hashmap *pDest)` |
|         5 | 1199 | `{` |
|         - | 1200 | `	ph7_hashmap_node *pEntry;` |
|         - | 1201 | `	ph7_value *pVal;` |
|         - | 1202 | `	sxi32 rc;` |
|         - | 1203 | `	sxu32 n;` |
|      2121 | 1204 | `	if( pSrc == pDest ){` |
|         - | 1205 | `		/* Same map. This can easily happen since hashmaps are passed by reference.` |
|         - | 1206 | `		 * Unlike the zend engine.` |
|         - | 1207 | `		 */` |
|       ! 0 | 1208 | `		return SXRET_OK;` |
|         - | 1209 | `	}` |
|         - | 1210 | `	/* Point to the first inserted entry in the source */` |
|      2121 | 1211 | `	pEntry = pSrc->pFirst;` |
|         - | 1212 | `	/* Perform the merge */` |
|    628871 | 1213 | `	for( n = 0 ; n < pSrc->nEntry ; ++n ){` |
|         - | 1214 | `		/* Extract the node value */` |
|    626755 | 1215 | `		pVal = HashmapExtractNodeValue(pEntry);` |
|    626755 | 1216 | `		if( pVal ){` |
|         - | 1217 | `			/* Make a local copy of the value.` |
|         - | 1218 | `			 * The insertion call below may trigger a memory pool reallocation` |
|         - | 1219 | `			 * which will invalidate the 'pVal' pointer since it points` |
|         - | 1220 | `			 * to the old pool.` |
|         - | 1221 | `			 */` |
|    626755 | 1222 | `			rc = HashmapDuplicateNode(pDest,pEntry,pVal,0);` |
|    313380 | 1223 | `		}else{` |
|       ! 0 | 1224 | `			rc = SXRET_OK;` |
|         - | 1225 | `		}` |
|    626755 | 1226 | `		if( rc != SXRET_OK ){` |
|       ! 0 | 1227 | `			return rc;` |
|         - | 1228 | `		}` |
|         - | 1229 | `		/* Point to the next entry */` |
|    626755 | 1230 | `		pEntry = pEntry->pPrev; /* Reverse link */` |
|    313380 | 1231 | `	}` |
|      2121 | 1232 | `	return SXRET_OK;` |
|      1063 | 1233 | `}` |
|         - | 1234 | `/*` |
|         - | 1235 | ` * Overwrite entries with the same key.` |
|         - | 1236 | ` * Refer to the [array_replace()] implementation for more information.` |
|         - | 1237 | ` *  According to the PHP language reference manual.` |
|         - | 1238 | ` *  array_replace() replaces the values of the first array with the same values` |
|         - | 1239 | ` *  from all the following arrays. If a key from the first array exists in the second` |
|         - | 1240 | ` *  array, its value will be replaced by the value from the second array. If the key` |
|         - | 1241 | ` *  exists in the second array, and not the first, it will be created in the first array.` |
|         - | 1242 | ` *  If a key only exists in the first array, it will be left as is. If several arrays` |
|         - | 1243 | ` *  are passed for replacement, they will be processed in order, the later arrays` |
|         - | 1244 | ` *  overwriting the previous values.` |
|         - | 1245 | ` *  array_replace() is not recursive : it will replace values in the first array` |
|         - | 1246 | ` *  by whatever type is in the second array.` |
|         - | 1247 | ` */` |
|        34 | 1248 | `static sxi32 HashmapOverwrite(ph7_hashmap *pSrc,ph7_hashmap *pDest)` |
|         2 | 1249 | `{` |
|         - | 1250 | `	ph7_hashmap_node *pEntry;` |
|         - | 1251 | `	ph7_value *pVal;` |
|         - | 1252 | `	sxi32 rc;` |
|         - | 1253 | `	sxu32 n;` |
|        36 | 1254 | `	if( pSrc == pDest ){` |
|         - | 1255 | `		/* Same map. This can easily happen since hashmaps are passed by reference.` |
|         - | 1256 | `		 * Unlike the zend engine.` |
|         - | 1257 | `		 */` |
|       ! 0 | 1258 | `		return SXRET_OK;` |
|         - | 1259 | `	}` |
|         - | 1260 | `	/* Point to the first inserted entry in the source */` |
|        36 | 1261 | `	pEntry = pSrc->pFirst;` |
|         - | 1262 | `	/* Perform the merge */` |
|        80 | 1263 | `	for( n = 0 ; n < pSrc->nEntry ; ++n ){` |
|         - | 1264 | `		/* Extract the node value */` |
|        46 | 1265 | `		pVal = HashmapExtractNodeValue(pEntry);` |
|        46 | 1266 | `		if( pVal ){` |
|        46 | 1267 | `			rc = HashmapDuplicateNode(pDest,pEntry,pVal,1);` |
|        24 | 1268 | `		}else{` |
|       ! 0 | 1269 | `			rc = SXRET_OK;` |
|         - | 1270 | `		}` |
|        46 | 1271 | `		if( rc != SXRET_OK ){` |
|       ! 0 | 1272 | `			return rc;` |
|         - | 1273 | `		}` |
|         - | 1274 | `		/* Point to the next entry */` |
|        46 | 1275 | `		pEntry = pEntry->pPrev; /* Reverse link */` |
|        24 | 1276 | `	}` |
|        36 | 1277 | `	return SXRET_OK;` |
|        19 | 1278 | `}` |
|         - | 1279 | `/*` |
|         - | 1280 | ` * Duplicate the contents of a hashmap. Store the copy in pDest.` |
|         - | 1281 | ` * Refer to the [array_pad(),array_copy(),...] implementation for more information.` |
|         - | 1282 | ` */` |
|      3920 | 1283 | `PH7_PRIVATE sxi32 PH7_HashmapDup(ph7_hashmap *pSrc,ph7_hashmap *pDest)` |
|         5 | 1284 | `{` |
|         - | 1285 | `	ph7_hashmap_node *pEntry;` |
|         - | 1286 | `	ph7_value *pVal;` |
|         - | 1287 | `	sxi32 rc;` |
|         - | 1288 | `	sxu32 n;` |
|      3925 | 1289 | `	if( pSrc == pDest ){` |
|         - | 1290 | `		/* Same map. This can easily happen since hashmaps are passed by reference.` |
|         - | 1291 | `		 * Unlike the zend engine.` |
|         - | 1292 | `		 */` |
|       ! 0 | 1293 | `		return SXRET_OK;` |
|         - | 1294 | `	}` |
|         - | 1295 | `	/* Point to the first inserted entry in the source */` |
|      3925 | 1296 | `	pEntry = pSrc->pFirst;` |
|         - | 1297 | `	/* Perform the duplication */` |
|      8065 | 1298 | `	for( n = 0 ; n < pSrc->nEntry ; ++n ){` |
|         - | 1299 | `		/* Extract the node value */` |
|      4145 | 1300 | `		pVal = HashmapExtractNodeValue(pEntry);` |
|      4145 | 1301 | `		if( pVal ){` |
|      4145 | 1302 | `			rc = HashmapDuplicateNode(pDest,pEntry,pVal,2);` |
|      2075 | 1303 | `		}else{` |
|       ! 0 | 1304 | `			rc = SXRET_OK;` |
|         - | 1305 | `		}` |
|      4145 | 1306 | `		if( rc != SXRET_OK ){` |
|       ! 0 | 1307 | `			return rc;` |
|         - | 1308 | `		}` |
|         - | 1309 | `		/* Point to the next entry */` |
|      4145 | 1310 | `		pEntry = pEntry->pPrev; /* Reverse link */` |
|      2075 | 1311 | `	}` |
|      3925 | 1312 | `	return SXRET_OK;` |
|      1965 | 1313 | `}` |
|         - | 1314 | `/*` |
|         - | 1315 | ` * Duplicate a hashmap, flattening every foreign (by-reference) node into a` |
|         - | 1316 | ` * plain value copy. php 8.1 gives a COPY of $GLOBALS pure value semantics` |
|         - | 1317 | ` * ($snap = $GLOBALS snapshots the symbol table: later writes on either side` |
|         - | 1318 | ` * never affect the other) — unlike ordinary array copies, where reference` |
|         - | 1319 | ` * elements stay live — so the $GLOBALS store path (PH7_MemObjStore) uses` |
|         - | 1320 | ` * this instead of PH7_HashmapDup.` |
|         - | 1321 | ` */` |
|        12 | 1322 | `PH7_PRIVATE sxi32 PH7_HashmapDupMaterialized(ph7_hashmap *pSrc,ph7_hashmap *pDest)` |
|         1 | 1323 | `{` |
|         - | 1324 | `	ph7_hashmap_node *pEntry;` |
|         - | 1325 | `	ph7_value *pVal;` |
|         - | 1326 | `	sxi32 rc;` |
|         - | 1327 | `	sxu32 n;` |
|        13 | 1328 | `	if( pSrc == pDest ){` |
|       ! 0 | 1329 | `		return SXRET_OK;` |
|         - | 1330 | `	}` |
|        13 | 1331 | `	pEntry = pSrc->pFirst;` |
|       711 | 1332 | `	for( n = 0 ; n < pSrc->nEntry ; ++n ){` |
|         - | 1333 | `		/* Extract the node value (resolves foreign references) */` |
|       699 | 1334 | `		pVal = HashmapExtractNodeValue(pEntry);` |
|       698 | 1335 | `		if( pVal && (pVal->iFlags & MEMOBJ_HASHMAP)` |
|       459 | 1336 | `		 && (ph7_hashmap *)pVal->x.pOther == pSrc->pVm->pGlobal ){` |
|         - | 1337 | `			/* A global still holding the live $GLOBALS map is the snapshot's` |
|         - | 1338 | `			 * own destination mid-store ($snap = $GLOBALS registers $snap` |
|         - | 1339 | `			 * before the value lands). php's snapshot — taken when $GLOBALS` |
|         - | 1340 | `			 * is READ, before the assignment — has no such entry, so skip it` |
|         - | 1341 | `			 * (also breaks the would-be infinite recursion). */` |
|         5 | 1342 | `			pVal = 0;` |
|         2 | 1343 | `		}` |
|       699 | 1344 | `		if( pVal ){` |
|       695 | 1345 | `			if( pEntry->iType == HASHMAP_BLOB_NODE ){` |
|      1036 | 1346 | `				rc = HashmapInsertBlobKey(&(*pDest),SyBlobData(&pEntry->xKey.sKey),` |
|       345 | 1347 | `					SyBlobLength(&pEntry->xKey.sKey),pVal,0,FALSE);` |
|       346 | 1348 | `			}else{` |
|         5 | 1349 | `				rc = HashmapInsertIntKey(&(*pDest),pEntry->xKey.iKey,pVal,0,FALSE);` |
|         - | 1350 | `			}` |
|       695 | 1351 | `			if( rc != SXRET_OK ){` |
|       ! 0 | 1352 | `				return rc;` |
|         - | 1353 | `			}` |
|       347 | 1354 | `		}` |
|         - | 1355 | `		/* Point to the next entry */` |
|       699 | 1356 | `		pEntry = pEntry->pPrev; /* Reverse link */` |
|       350 | 1357 | `	}` |
|        13 | 1358 | `	return SXRET_OK;` |
|         7 | 1359 | `}` |
|         - | 1360 | `/*` |
|         - | 1361 | ` * Copy-on-write separation for arrays.` |
|         - | 1362 | ` * If the hashmap inside pValue has iRef > 1 (shared), duplicate it so that` |
|         - | 1363 | ` * pValue owns a private copy. The original map's refcount is decremented.` |
|         - | 1364 | ` * Returns the (possibly new) hashmap pointer.` |
|         - | 1365 | ` */` |
|    217224 | 1366 | `PH7_PRIVATE ph7_hashmap * PH7_HashmapCowSeparate(ph7_vm *pVm,ph7_value *pValue)` |
|         5 | 1367 | `{` |
|    217229 | 1368 | `	ph7_hashmap *pMap = (ph7_hashmap *)pValue->x.pOther;` |
|         - | 1369 | `	ph7_hashmap *pNew;` |
|         - | 1370 | `	ph7_value *pBacking;` |
|         - | 1371 | `	sxu32 nValIdx;` |
|         - | 1372 | `	int bValueInPool;` |
|    217229 | 1373 | `	if( pMap->iRef < 2 ){` |
|         - | 1374 | `		/* Sole owner, no separation needed */` |
|    214925 | 1375 | `		return pMap;` |
|         - | 1376 | `	}` |
|      2309 | 1377 | `	if( pMap == pVm->pGlobal ){` |
|         - | 1378 | `		/* Never separate $GLOBALS — it is a live view of the symbol table.` |
|         - | 1379 | `		 * (A COPY of $GLOBALS never shares this map: PH7_MemObjStore` |
|         - | 1380 | `		 * materializes a by-value snapshot at assignment, php 8.1.) */` |
|       119 | 1381 | `		return pMap;` |
|         - | 1382 | `	}` |
|         - | 1383 | `	/* If this value is a stack copy of a named variable, separate the` |
|         - | 1384 | `	 * backing variable instead so the change persists after the stack` |
|         - | 1385 | `	 * frame is popped. */` |
|      2191 | 1386 | `	if( pValue->nIdx != SXU32_HIGH ){` |
|      2191 | 1387 | `		pBacking = (ph7_value *)SySetAt(&pVm->aMemObj,pValue->nIdx);` |
|      2186 | 1388 | `		if( pBacking && pBacking != pValue` |
|      2166 | 1389 | `			&& (pBacking->iFlags & MEMOBJ_HASHMAP)` |
|      2151 | 1390 | `			&& (ph7_hashmap *)pBacking->x.pOther == pMap ){` |
|         - | 1391 | `			/* Undo the stack ref to reveal true sharing count */` |
|      2151 | 1392 | `			pMap->iRef--;` |
|      2151 | 1393 | `			if( pMap->iRef < 2 ){` |
|         - | 1394 | `				/* After undoing stack ref, sole owner — no separation */` |
|      2109 | 1395 | `				pMap->iRef++;` |
|      2109 | 1396 | `				return pMap;` |
|         - | 1397 | `			}` |
|        44 | 1398 | `			pNew = PH7_NewHashmap(pVm,0,0);` |
|        44 | 1399 | `			if( pNew == 0 ){` |
|       ! 0 | 1400 | `				pMap->iRef++;` |
|       ! 0 | 1401 | `				return pMap;` |
|         - | 1402 | `			}` |
|        44 | 1403 | `			if( PH7_HashmapDup(pMap,pNew) != SXRET_OK ){` |
|         - | 1404 | `				/* Dup failed (OOM) — discard partial copy, restore state */` |
|       ! 0 | 1405 | `				PH7_HashmapRelease(pNew,TRUE);` |
|       ! 0 | 1406 | `				pMap->iRef++;` |
|       ! 0 | 1407 | `				return pMap;` |
|         - | 1408 | `			}` |
|        44 | 1409 | `			pNew->iNextIdx = pMap->iNextIdx;` |
|        44 | 1410 | `			pMap->iRef--;  /* Backing variable no longer references old map */` |
|         - | 1411 | `			/* PH7_HashmapDup reserves a memory object per duplicated entry, which` |
|         - | 1412 | `			 * can grow — and therefore reallocate (move) — pVm->aMemObj. That` |
|         - | 1413 | `			 * invalidates the pBacking pointer captured above, so re-resolve it` |
|         - | 1414 | `			 * from the (stable) slot index before writing. Using the stale pointer` |
|         - | 1415 | `			 * dereferences the freed old buffer, which is a hard SIGSEGV on` |
|         - | 1416 | `			 * glibc/x86_64 once aMemObj is large enough to be mmap-backed (the old` |
|         - | 1417 | `			 * mapping is munmap'd on move) and a silent use-after-free elsewhere. */` |
|        44 | 1418 | `			pBacking = (ph7_value *)SySetAt(&pVm->aMemObj,pValue->nIdx);` |
|        44 | 1419 | `			if( pBacking ){` |
|        44 | 1420 | `				pBacking->x.pOther = pNew;` |
|        21 | 1421 | `			}` |
|         - | 1422 | `			/* Update the stack value to match */` |
|        44 | 1423 | `			pValue->x.pOther = pNew;` |
|        44 | 1424 | `			pNew->iRef++;  /* +1 for stack (pValue); iRef=1 from NewHashmap covers pBacking */` |
|        44 | 1425 | `			return pNew;` |
|         - | 1426 | `		}` |
|        20 | 1427 | `	}` |
|         - | 1428 | `	/* Some callers (e.g. OP_STORE_IDX, by-ref foreach) pass a pValue that points` |
|         - | 1429 | `	 * directly into pVm->aMemObj. PH7_HashmapDup below reserves a memory object` |
|         - | 1430 | `	 * per duplicated entry, which can grow — and therefore reallocate (move) —` |
|         - | 1431 | `	 * pVm->aMemObj, leaving such a pValue dangling. Capture its slot identity now,` |
|         - | 1432 | `	 * before the dup, so the write-back can re-resolve from the (stable) index` |
|         - | 1433 | `	 * rather than dereference the captured pointer (the same hazard handled for` |
|         - | 1434 | `	 * pBacking in the backing-variable branch above). */` |
|        41 | 1435 | `	nValIdx = pValue->nIdx;` |
|        61 | 1436 | `	bValueInPool = ( nValIdx != SXU32_HIGH` |
|        40 | 1437 | `		&& (ph7_value *)SySetAt(&pVm->aMemObj,nValIdx) == pValue );` |
|        41 | 1438 | `	pNew = PH7_NewHashmap(pVm,0,0);` |
|        41 | 1439 | `	if( pNew == 0 ){` |
|         - | 1440 | `		/* Allocation failure — fall through with shared map */` |
|       ! 0 | 1441 | `		return pMap;` |
|         - | 1442 | `	}` |
|        41 | 1443 | `	if( PH7_HashmapDup(pMap,pNew) != SXRET_OK ){` |
|         - | 1444 | `		/* Dup failed (OOM) — discard partial copy, keep original */` |
|       ! 0 | 1445 | `		PH7_HashmapRelease(pNew,TRUE);` |
|       ! 0 | 1446 | `		return pMap;` |
|         - | 1447 | `	}` |
|        41 | 1448 | `	pNew->iNextIdx = pMap->iNextIdx;` |
|        41 | 1449 | `	pMap->iRef--;` |
|        41 | 1450 | `	if( bValueInPool ){` |
|         - | 1451 | `		/* aMemObj may have moved during the dup — re-resolve pValue's slot. */` |
|        41 | 1452 | `		pValue = (ph7_value *)SySetAt(&pVm->aMemObj,nValIdx);` |
|        41 | 1453 | `		if( pValue == 0 ){` |
|       ! 0 | 1454 | `			return pNew;` |
|         - | 1455 | `		}` |
|        20 | 1456 | `	}` |
|        41 | 1457 | `	pValue->x.pOther = pNew;` |
|        41 | 1458 | `	return pNew;` |
|    108617 | 1459 | `}` |
|         - | 1460 | `/*` |
|         - | 1461 | ` * Perform the union of two hashmaps.` |
|         - | 1462 | ` * This operation is performed only if the user uses the '+' operator` |
|         - | 1463 | ` * with a variable holding an array as follows:` |
|         - | 1464 | ` * <?php` |
|         - | 1465 | ` * $a = array("a" => "apple", "b" => "banana");` |
|         - | 1466 | ` * $b = array("a" => "pear", "b" => "strawberry", "c" => "cherry");` |
|         - | 1467 | ` * $c = $a + $b; // Union of $a and $b` |
|         - | 1468 | ` * echo "Union of \$a and \$b: \n";` |
|         - | 1469 | ` * var_dump($c);` |
|         - | 1470 | ` * $c = $b + $a; // Union of $b and $a` |
|         - | 1471 | ` * echo "Union of \$b and \$a: \n";` |
|         - | 1472 | ` * var_dump($c);` |
|         - | 1473 | ` * ?>` |
|         - | 1474 | ` * When executed, this script will print the following:` |
|         - | 1475 | ` * Union of $a and $b:` |
|         - | 1476 | ` * array(3) {` |
|         - | 1477 | ` *  ["a"]=>` |
|         - | 1478 | ` *  string(5) "apple"` |
|         - | 1479 | ` *  ["b"]=>` |
|         - | 1480 | ` * string(6) "banana"` |
|         - | 1481 | ` *  ["c"]=>` |
|         - | 1482 | ` * string(6) "cherry"` |
|         - | 1483 | ` * }` |
|         - | 1484 | ` * Union of $b and $a:` |
|         - | 1485 | ` * array(3) {` |
|         - | 1486 | ` * ["a"]=>` |
|         - | 1487 | ` * string(4) "pear"` |
|         - | 1488 | ` * ["b"]=>` |
|         - | 1489 | ` * string(10) "strawberry"` |
|         - | 1490 | ` * ["c"]=>` |
|         - | 1491 | ` * string(6) "cherry"` |
|         - | 1492 | ` * }` |
|         - | 1493 | ` * The + operator returns the right-hand array appended to the left-hand array;` |
|         - | 1494 | ` * For keys that exist in both arrays, the elements from the left-hand array will be used` |
|         - | 1495 | ` * and the matching elements from the right-hand array will be ignored.` |
|         - | 1496 | ` */` |
|      3814 | 1497 | `PH7_PRIVATE sxi32 PH7_HashmapUnion(ph7_hashmap *pLeft,ph7_hashmap *pRight)` |
|         5 | 1498 | `{` |
|         - | 1499 | `	ph7_hashmap_node *pEntry;` |
|      3819 | 1500 | `	sxi32 rc = SXRET_OK;` |
|         - | 1501 | `	ph7_value *pObj;` |
|         - | 1502 | `	sxu32 n;` |
|      3819 | 1503 | `	if( pLeft == pRight ){` |
|         - | 1504 | `		/* Same map. This can easily happen since hashmaps are passed by reference.` |
|         - | 1505 | `		 * Unlike the zend engine.` |
|         - | 1506 | `		 */` |
|       ! 0 | 1507 | `		return SXRET_OK;` |
|         - | 1508 | `	}` |
|         - | 1509 | `	/* Perform the union */` |
|      3819 | 1510 | `	pEntry = pRight->pFirst;` |
|      3853 | 1511 | `	for(n = 0 ; n < pRight->nEntry ; ++n ){` |
|         - | 1512 | `		/* Make sure the given key does not exists in the left array */` |
|        38 | 1513 | `		if( pEntry->iType == HASHMAP_BLOB_NODE ){` |
|         - | 1514 | `			/* BLOB key */` |
|        24 | 1515 | `			if( SXRET_OK !=` |
|        20 | 1516 | `				HashmapLookupBlobKey(&(*pLeft),SyBlobData(&pEntry->xKey.sKey),SyBlobLength(&pEntry->xKey.sKey),0) ){` |
|        20 | 1517 | `					pObj = HashmapExtractNodeValue(pEntry);` |
|        20 | 1518 | `					if( pObj ){` |
|        20 | 1519 | `						ph7_value sSafeVal = *pObj;` |
|         - | 1520 | `						/* Perform the insertion */` |
|        20 | 1521 | `						rc = HashmapInsertBlobKey(&(*pLeft),SyBlobData(&pEntry->xKey.sKey),SyBlobLength(&pEntry->xKey.sKey),` |
|         - | 1522 | `							&sSafeVal,0,FALSE);` |
|        20 | 1523 | `						if( rc != SXRET_OK ){` |
|       ! 0 | 1524 | `							return rc;` |
|         - | 1525 | `						}` |
|         8 | 1526 | `					}` |
|         8 | 1527 | `			}` |
|        14 | 1528 | `		}else{` |
|         - | 1529 | `			/* INT key */` |
|        16 | 1530 | `			if( SXRET_OK != HashmapLookupIntKey(&(*pLeft),pEntry->xKey.iKey,0) ){` |
|        11 | 1531 | `				pObj = HashmapExtractNodeValue(pEntry);` |
|        11 | 1532 | `				if( pObj ){` |
|        11 | 1533 | `					ph7_value sSafeVal = *pObj;` |
|         - | 1534 | `					/* Perform the insertion */` |
|        11 | 1535 | `					rc = HashmapInsertIntKey(&(*pLeft),pEntry->xKey.iKey,&sSafeVal,0,FALSE);` |
|        11 | 1536 | `					if( rc != SXRET_OK ){` |
|       ! 0 | 1537 | `						return rc;` |
|         - | 1538 | `					}` |
|         5 | 1539 | `				}` |
|         5 | 1540 | `			}` |
|         - | 1541 | `		}` |
|         - | 1542 | `		/* Point to the next entry */` |
|        38 | 1543 | `		pEntry = pEntry->pPrev; /* Reverse link */` |
|        21 | 1544 | `	}` |
|      3819 | 1545 | `	return SXRET_OK;` |
|      1912 | 1546 | `}` |
|         - | 1547 | `/*` |
|         - | 1548 | ` * Allocate a new hashmap.` |
|         - | 1549 | ` * Return a pointer to the freshly allocated hashmap on success.NULL otherwise.` |
|         - | 1550 | ` */` |
|    116402 | 1551 | `PH7_PRIVATE ph7_hashmap * PH7_NewHashmap(` |
|         - | 1552 | `	ph7_vm *pVm,              /* VM that trigger the hashmap creation */` |
|         - | 1553 | `	sxu32 (*xIntHash)(sxi64), /* Hash function for int keys.NULL otherwise*/` |
|         - | 1554 | `	sxu32 (*xBlobHash)(const void *,sxu32) /* Hash function for BLOB keys.NULL otherwise */` |
|         - | 1555 | `	)` |
|         5 | 1556 | `{` |
|         - | 1557 | `	ph7_hashmap *pMap;` |
|         - | 1558 | `	/* Allocate a new instance */` |
|    116407 | 1559 | `	pMap = (ph7_hashmap *)SyMemBackendPoolAlloc(&pVm->sAllocator,sizeof(ph7_hashmap));` |
|    116407 | 1560 | `	if( pMap == 0 ){` |
|       ! 0 | 1561 | `		return 0;` |
|         - | 1562 | `	}` |
|         - | 1563 | `	/* Zero the structure */` |
|    116407 | 1564 | `	SyZero(pMap,sizeof(ph7_hashmap));` |
|         - | 1565 | `	/* Fill in the structure */` |
|    116407 | 1566 | `	pMap->pVm = &(*pVm);` |
|    116407 | 1567 | `	pMap->iRef = 1;` |
|         - | 1568 | `	/* Default hash functions */` |
|    116407 | 1569 | `	pMap->xIntHash  = xIntHash ? xIntHash : IntHash;` |
|    116407 | 1570 | `	pMap->xBlobHash = xBlobHash ? xBlobHash : BinHash;` |
|    116407 | 1571 | `	return pMap;` |
|     58206 | 1572 | `}` |
|         - | 1573 | `/*` |
|         - | 1574 | ` * Install superglobals in the given virtual machine.` |
|         - | 1575 | ` * Note on superglobals.` |
|         - | 1576 | ` *  According to the PHP language reference manual.` |
|         - | 1577 | ` *  Superglobals are built-in variables that are always available in all scopes.` |
|         - | 1578 | `*   Description` |
|         - | 1579 | `*   Several predefined variables in PHP are "superglobals", which means they` |
|         - | 1580 | `*   are available in all scopes throughout a script. There is no need to do` |
|         - | 1581 | `*   global $variable; to access them within functions or methods.` |
|         - | 1582 | `*   These superglobal variables are:` |
|         - | 1583 | `*    $GLOBALS` |
|         - | 1584 | `*    $_SERVER` |
|         - | 1585 | `*    $_GET` |
|         - | 1586 | `*    $_POST` |
|         - | 1587 | `*    $_FILES` |
|         - | 1588 | `*    $_COOKIE` |
|         - | 1589 | `*    $_SESSION` |
|         - | 1590 | `*    $_REQUEST` |
|         - | 1591 | `*    $_ENV` |
|         - | 1592 | `*/` |
|      3478 | 1593 | `PH7_PRIVATE sxi32 PH7_HashmapCreateSuper(ph7_vm *pVm)` |
|         5 | 1594 | `{` |
|         - | 1595 | `	static const char * azSuper[] = {` |
|         - | 1596 | `		"_SERVER",   /* $_SERVER */` |
|         - | 1597 | `		"_GET",      /* $_GET */` |
|         - | 1598 | `		"_POST",     /* $_POST */` |
|         - | 1599 | `		"_FILES",    /* $_FILES */` |
|         - | 1600 | `		"_COOKIE",   /* $_COOKIE */` |
|         - | 1601 | `		"_SESSION",  /* $_SESSION */` |
|         - | 1602 | `		"_REQUEST",  /* $_REQUEST */` |
|         - | 1603 | `		"_ENV",      /* $_ENV */` |
|         - | 1604 | `		"_HEADER",   /* $_HEADER */` |
|         - | 1605 | `		"argv"       /* $argv */` |
|         - | 1606 | `	};` |
|         - | 1607 | `	ph7_hashmap *pMap;` |
|         - | 1608 | `	ph7_value *pObj;` |
|         - | 1609 | `	SyString *pFile;` |
|         - | 1610 | `	sxi32 rc;` |
|         - | 1611 | `	sxu32 n;` |
|         - | 1612 | `	/* Allocate a new hashmap for the $GLOBALS array */` |
|      3483 | 1613 | `	pMap = PH7_NewHashmap(&(*pVm),0,0);` |
|      3483 | 1614 | `	if( pMap == 0 ){` |
|       ! 0 | 1615 | `		return SXERR_MEM;` |
|         - | 1616 | `	}` |
|      3483 | 1617 | `	pVm->pGlobal = pMap;` |
|         - | 1618 | `	/* Reserve a ph7_value for the $GLOBALS array*/` |
|      3483 | 1619 | `	pObj = PH7_ReserveMemObj(&(*pVm));` |
|      3483 | 1620 | `	if( pObj == 0 ){` |
|       ! 0 | 1621 | `		return SXERR_MEM;` |
|         - | 1622 | `	}` |
|      3483 | 1623 | `	PH7_MemObjInitFromArray(&(*pVm),pObj,pMap);` |
|         - | 1624 | `	/* Record object index */` |
|      3483 | 1625 | `	pVm->nGlobalIdx = pObj->nIdx;` |
|         - | 1626 | `	/* Install the special $GLOBALS array */` |
|      3483 | 1627 | `	rc = SyHashInsert(&pVm->hSuper,(const void *)"GLOBALS",sizeof("GLOBALS")-1,SX_INT_TO_PTR(pVm->nGlobalIdx));` |
|      3483 | 1628 | `	if( rc != SXRET_OK ){` |
|       ! 0 | 1629 | `		return rc;` |
|         - | 1630 | `	}` |
|         - | 1631 | `	/* Install superglobals now */` |
|     38263 | 1632 | `	for( n =  0 ; n < SX_ARRAYSIZE(azSuper)  ; n++ ){` |
|         - | 1633 | `		ph7_value *pSuper;` |
|         - | 1634 | `		/* Request an empty array */` |
|     34785 | 1635 | `		pSuper = ph7_new_array(&(*pVm));` |
|     34785 | 1636 | `		if( pSuper == 0 ){` |
|       ! 0 | 1637 | `			return SXERR_MEM;` |
|         - | 1638 | `		}` |
|         - | 1639 | `		/* Install */` |
|     34785 | 1640 | `		rc = ph7_vm_config(&(*pVm),PH7_VM_CONFIG_CREATE_SUPER,azSuper[n]/* Super-global name*/,pSuper/* Super-global value */);` |
|     34785 | 1641 | `		if( rc != SXRET_OK ){` |
|       ! 0 | 1642 | `			return rc;` |
|         - | 1643 | `		}` |
|         - | 1644 | `		/* Release the value now it have been installed */` |
|     34785 | 1645 | `		ph7_release_value(&(*pVm),pSuper);` |
|     17395 | 1646 | `	}` |
|         - | 1647 | `	/* Set some $_SERVER entries */` |
|      3483 | 1648 | `	pFile = (SyString *)SySetPeek(&pVm->aFiles);` |
|         - | 1649 | `	/*` |
|         - | 1650 | `	 * 'SCRIPT_FILENAME'` |
|         - | 1651 | `	 * The absolute pathname of the currently executing script.` |
|         - | 1652 | `	 */` |
|      6957 | 1653 | `	ph7_vm_config(pVm,PH7_VM_CONFIG_SERVER_ATTR,` |
|         - | 1654 | `		"SCRIPT_FILENAME",` |
|      1739 | 1655 | `		pFile ? pFile->zString : ":Memory:",` |
|      3474 | 1656 | `		pFile ? pFile->nByte : sizeof(":Memory:") - 1` |
|         - | 1657 | `		);` |
|         - | 1658 | `	/* All done,all super-global are installed now */` |
|      3483 | 1659 | `	return SXRET_OK;` |
|      1744 | 1660 | `}` |
|         - | 1661 | `/*` |
|         - | 1662 | ` * Release a hashmap.` |
|         - | 1663 | ` */` |
|     73676 | 1664 | `PH7_PRIVATE sxi32 PH7_HashmapRelease(ph7_hashmap *pMap,int FreeDS)` |
|         5 | 1665 | `{` |
|         - | 1666 | `	ph7_hashmap_node *pEntry,*pNext;` |
|     73681 | 1667 | `	ph7_vm *pVm = pMap->pVm;` |
|         - | 1668 | `	sxu32 n;` |
|     73681 | 1669 | `	if( pMap == pVm->pGlobal ){` |
|         - | 1670 | `		/* Cannot delete the $GLOBALS array */` |
|       ! 0 | 1671 | `		PH7_VmThrowError(pMap->pVm,0,PH7_CTX_NOTICE,"$GLOBALS is a read-only array,deletion is forbidden");` |
|       ! 0 | 1672 | `		return SXRET_OK;` |
|         - | 1673 | `	}` |
|         - | 1674 | `	/* Start the release process */` |
|     73681 | 1675 | `	n = 0;` |
|     73681 | 1676 | `	pEntry = pMap->pFirst;` |
|   1616074 | 1677 | `	for(;;){` |
|   3232153 | 1678 | `		if( n >= pMap->nEntry ){` |
|     73681 | 1679 | `			break;` |
|         - | 1680 | `		}` |
|   3158477 | 1681 | `		pNext = pEntry->pPrev; /* Reverse link */` |
|         - | 1682 | `		/* Remove the reference from the foreign table */` |
|   3158477 | 1683 | `		PH7_VmRefObjRemove(pVm,pEntry->nValIdx,0,pEntry);` |
|   3158477 | 1684 | `		if( (pEntry->iFlags & HASHMAP_NODE_FOREIGN_OBJ) == 0 ){` |
|         - | 1685 | `			/* Restore the ph7_value to the free list */` |
|   3158467 | 1686 | `			PH7_VmUnsetMemObj(pVm,pEntry->nValIdx,FALSE);` |
|   1579231 | 1687 | `		}` |
|         - | 1688 | `		/* Release the node */` |
|   3158477 | 1689 | `		if( pEntry->iType == HASHMAP_BLOB_NODE ){` |
|     81655 | 1690 | `			SyBlobRelease(&pEntry->xKey.sKey);` |
|     40825 | 1691 | `		}` |
|   3158477 | 1692 | `		SyMemBackendPoolFree(&pVm->sAllocator,pEntry);` |
|         - | 1693 | `		/* Point to the next entry */` |
|   3158477 | 1694 | `		pEntry = pNext;` |
|   3158477 | 1695 | `		n++;` |
|         5 | 1696 | `	}` |
|     73681 | 1697 | `	if( pMap->nEntry > 0 ){` |
|         - | 1698 | `		/* Release the hash bucket */` |
|     60197 | 1699 | `		SyMemBackendFree(&pVm->sAllocator,pMap->apBucket);` |
|     30096 | 1700 | `	}` |
|     73681 | 1701 | `	if( FreeDS ){` |
|         - | 1702 | `		/* Free the whole instance */` |
|     73665 | 1703 | `		SyMemBackendPoolFree(&pVm->sAllocator,pMap);` |
|     36835 | 1704 | `	}else{` |
|         - | 1705 | `		/* Keep the instance but reset it's fields */` |
|        17 | 1706 | `		pMap->apBucket = 0;` |
|        17 | 1707 | `		pMap->iNextIdx = 0;` |
|        17 | 1708 | `		pMap->nEntry = pMap->nSize = 0;` |
|        17 | 1709 | `		pMap->pFirst = pMap->pLast = pMap->pCur = 0;` |
|         - | 1710 | `	}` |
|     73681 | 1711 | `	return SXRET_OK;` |
|     36843 | 1712 | `}` |
|         - | 1713 | `/*` |
|         - | 1714 | ` * Decrement the reference count of a given hashmap.` |
|         - | 1715 | ` * If the count reaches zero which mean no more variables` |
|         - | 1716 | ` * are pointing to this hashmap,then release the whole instance.` |
|         - | 1717 | ` */` |
|    734124 | 1718 | `PH7_PRIVATE void  PH7_HashmapUnref(ph7_hashmap *pMap)` |
|         5 | 1719 | `{` |
|    734129 | 1720 | `	ph7_vm *pVm = pMap->pVm;` |
|         - | 1721 | `	/* TICKET 1432-49: $GLOBALS is not subject to garbage collection */` |
|    734129 | 1722 | `	pMap->iRef--;` |
|    734129 | 1723 | `	if( pMap->iRef < 1 && pMap != pVm->pGlobal){` |
|     73645 | 1724 | `		PH7_HashmapRelease(pMap,TRUE);` |
|     36820 | 1725 | `	}` |
|    734129 | 1726 | `}` |
|         - | 1727 | `/*` |
|         - | 1728 | ` * Check if a given key exists in the given hashmap.` |
|         - | 1729 | ` * Write a pointer to the target node on success.` |
|         - | 1730 | ` * Otherwise SXERR_NOTFOUND is returned on failure.` |
|         - | 1731 | ` */` |
|    127456 | 1732 | `PH7_PRIVATE sxi32 PH7_HashmapLookup(` |
|         - | 1733 | `	ph7_hashmap *pMap,        /* Target hashmap */` |
|         - | 1734 | `	ph7_value *pKey,          /* Lookup key */` |
|         - | 1735 | `	ph7_hashmap_node **ppNode /* OUT: Target node on success */` |
|         - | 1736 | `	)` |
|         5 | 1737 | `{` |
|         - | 1738 | `	sxi32 rc;` |
|    127461 | 1739 | `	if( pMap->nEntry < 1 ){` |
|         - | 1740 | `		/* TICKET 1433-25: Don't bother hashing,the hashmap is empty anyway.` |
|         - | 1741 | `		 */` |
|        64 | 1742 | `		return SXERR_NOTFOUND;` |
|         - | 1743 | `	}` |
|    127401 | 1744 | `	rc = HashmapLookup(&(*pMap),&(*pKey),ppNode);` |
|    127401 | 1745 | `	return rc;` |
|     63733 | 1746 | `}` |
|         - | 1747 | `/*` |
|         - | 1748 | ` * Insert a given key and it's associated value (if any) in the given` |
|         - | 1749 | ` * hashmap.` |
|         - | 1750 | ` * If a node with the given key already exists in the database` |
|         - | 1751 | ` * then this function overwrite the old value.` |
|         - | 1752 | ` */` |
|   2569136 | 1753 | `PH7_PRIVATE sxi32 PH7_HashmapInsert(` |
|         - | 1754 | `	ph7_hashmap *pMap, /* Target hashmap */` |
|         - | 1755 | `	ph7_value *pKey,   /* Lookup key */` |
|         - | 1756 | `	ph7_value *pVal    /* Node value.NULL otherwise */` |
|         - | 1757 | `	)` |
|         5 | 1758 | `{` |
|         - | 1759 | `	sxi32 rc;` |
|         - | 1760 | `	/* Storing the $GLOBALS array itself as a VALUE is fine in php ($a[] =` |
|         - | 1761 | `	 * $GLOBALS copies the symbol table); the old TICKET 1433-35 guard that` |
|         - | 1762 | `	 * forbade it was a PH7-ism. Writes INTO $GLOBALS are handled inside` |
|         - | 1763 | `	 * HashmapInsert (they create real global variables, php 8.1). */` |
|   2569141 | 1764 | `	rc = HashmapInsert(&(*pMap),&(*pKey),&(*pVal));` |
|   2569141 | 1765 | `	return rc;` |
|         5 | 1766 | `}` |
|         - | 1767 | `/*` |
|         - | 1768 | ` * Merge entries of pSrc into pDest using PHP merge semantics:` |
|         - | 1769 | ` *   - String keys overwrite same-key entries in pDest.` |
|         - | 1770 | ` *   - Integer keys are renumbered with the destination's auto-index.` |
|         - | 1771 | ` * This is the same routine that backs array_merge().` |
|         - | 1772 | ` */` |
|        52 | 1773 | `PH7_PRIVATE sxi32 PH7_HashmapMerge(ph7_hashmap *pSrc,ph7_hashmap *pDest)` |
|         1 | 1774 | `{` |
|        53 | 1775 | `	return HashmapMerge(&(*pSrc),&(*pDest));` |
|         1 | 1776 | `}` |
|         - | 1777 | `/*` |
|         - | 1778 | ` * Insert a given key and it's associated value (foreign index) in the given` |
|         - | 1779 | ` * hashmap.` |
|         - | 1780 | ` * This is insertion by reference so be careful to mark the node` |
|         - | 1781 | ` * with the HASHMAP_NODE_FOREIGN_OBJ flag being set.` |
|         - | 1782 | ` * The insertion by reference is triggered when the following` |
|         - | 1783 | ` * expression is encountered.` |
|         - | 1784 | ` * $var = 10;` |
|         - | 1785 | ` *  $a = array(&var);` |
|         - | 1786 | ` * OR` |
|         - | 1787 | ` *  $a[] =& $var;` |
|         - | 1788 | ` * That is,$var is a foreign ph7_value and the $a array have no control` |
|         - | 1789 | ` * over it's contents.` |
|         - | 1790 | ` * Note that the node that hold the foreign ph7_value is automatically` |
|         - | 1791 | ` * removed when the foreign ph7_value is unset.` |
|         - | 1792 | ` * Example:` |
|         - | 1793 | ` *  $var = 10;` |
|         - | 1794 | ` *  $a[] =& $var;` |
|         - | 1795 | ` *  echo count($a).PHP_EOL; //1` |
|         - | 1796 | ` *  //Unset the foreign ph7_value now` |
|         - | 1797 | ` *  unset($var);` |
|         - | 1798 | ` *  echo count($a); //0` |
|         - | 1799 | ` * Note that this is a PH7 eXtension.` |
|         - | 1800 | ` * Refer to the official documentation for more information.` |
|         - | 1801 | ` * If a node with the given key already exists in the database` |
|         - | 1802 | ` * then this function overwrite the old value.` |
|         - | 1803 | ` */` |
|     46280 | 1804 | `PH7_PRIVATE sxi32 PH7_HashmapInsertByRef(` |
|         - | 1805 | `	ph7_hashmap *pMap, /* Target hashmap */` |
|         - | 1806 | `	ph7_value *pKey,   /* Lookup key */` |
|         - | 1807 | `	sxu32 nRefIdx      /* Foreign ph7_value index */` |
|         - | 1808 | `	)` |
|         5 | 1809 | `{` |
|         - | 1810 | `	sxi32 rc;` |
|     46285 | 1811 | `	if( nRefIdx == pMap->pVm->nGlobalIdx ){` |
|         - | 1812 | `		/* php's non-catchable fatal: $a[] =& $GLOBALS is forbidden (8.1) */` |
|       ! 0 | 1813 | `		PH7_VmThrowError(pMap->pVm,0,PH7_CTX_ERR,"Cannot acquire reference to $GLOBALS");` |
|       ! 0 | 1814 | `		pMap->pVm->iExitStatus = 255;` |
|       ! 0 | 1815 | `		pMap->pVm->bHaltRequested = 1;` |
|       ! 0 | 1816 | `		return PH7_ABORT;` |
|         - | 1817 | `	}` |
|     46285 | 1818 | `	rc = HashmapInsertByRef(&(*pMap),&(*pKey),nRefIdx);` |
|     46285 | 1819 | `	return rc;` |
|     23145 | 1820 | `}` |
|         - | 1821 | `/*` |
|         - | 1822 | ` * Reset the node cursor of a given hashmap.` |
|         - | 1823 | ` */` |
|     35402 | 1824 | `PH7_PRIVATE void PH7_HashmapResetLoopCursor(ph7_hashmap *pMap)` |
|         5 | 1825 | `{` |
|         - | 1826 | `	/* Reset the loop cursor */` |
|     35407 | 1827 | `	pMap->pCur = pMap->pFirst;` |
|     35407 | 1828 | `}` |
|         - | 1829 | `/*` |
|         - | 1830 | ` * Return a pointer to the node currently pointed by the node cursor.` |
|         - | 1831 | ` * If the cursor reaches the end of the list,then this function` |
|         - | 1832 | ` * return NULL.` |
|         - | 1833 | ` * Note that the node cursor is automatically advanced by this function.` |
|         - | 1834 | ` */` |
|    234088 | 1835 | `PH7_PRIVATE ph7_hashmap_node * PH7_HashmapGetNextEntry(ph7_hashmap *pMap)` |
|         5 | 1836 | `{` |
|    234093 | 1837 | `	ph7_hashmap_node *pCur = pMap->pCur;` |
|    234093 | 1838 | `	if( pCur == 0 ){` |
|         - | 1839 | `		/* End of the list,return null */` |
|     17725 | 1840 | `		return 0;` |
|         - | 1841 | `	}` |
|         - | 1842 | `	/* Advance the node cursor */` |
|    216373 | 1843 | `	pMap->pCur = pCur->pPrev; /* Reverse link */` |
|    216373 | 1844 | `	return pCur;` |
|    117049 | 1845 | `}` |
|         - | 1846 | `/*` |
|         - | 1847 | ` * Extract a node value.` |
|         - | 1848 | ` */` |
|    545490 | 1849 | `PH7_PRIVATE void PH7_HashmapExtractNodeValue(ph7_hashmap_node *pNode,ph7_value *pValue,int bStore)` |
|         5 | 1850 | `{` |
|    545495 | 1851 | `	ph7_value *pEntry = HashmapExtractNodeValue(pNode);` |
|    545495 | 1852 | `	if( pEntry ){` |
|    545495 | 1853 | `		if( bStore ){` |
|    216751 | 1854 | `			PH7_MemObjStore(pEntry,pValue);` |
|    108378 | 1855 | `		}else{` |
|    328749 | 1856 | `			PH7_MemObjLoad(pEntry,pValue);` |
|         - | 1857 | `		}` |
|    272777 | 1858 | `	}else{` |
|       ! 0 | 1859 | `		PH7_MemObjRelease(pValue);` |
|         - | 1860 | `	}` |
|    545495 | 1861 | `}` |
|         - | 1862 | `/*` |
|         - | 1863 | ` * Extract a node key.` |
|         - | 1864 | ` */` |
|    141550 | 1865 | `PH7_PRIVATE void PH7_HashmapExtractNodeKey(ph7_hashmap_node *pNode,ph7_value *pKey)` |
|         5 | 1866 | `{` |
|         - | 1867 | `	/* Fill with the current key */` |
|    141555 | 1868 | `	if( pNode->iType == HASHMAP_INT_NODE ){` |
|    137009 | 1869 | `		if( SyBlobLength(&pKey->sBlob) > 0 ){` |
|        27 | 1870 | `			SyBlobRelease(&pKey->sBlob);` |
|        13 | 1871 | `		}` |
|    137009 | 1872 | `		pKey->x.iVal = pNode->xKey.iKey;` |
|    137009 | 1873 | `		MemObjSetType(pKey,MEMOBJ_INT);` |
|     68507 | 1874 | `	}else{` |
|      4551 | 1875 | `		SyBlobReset(&pKey->sBlob);` |
|      4551 | 1876 | `		SyBlobAppend(&pKey->sBlob,SyBlobData(&pNode->xKey.sKey),SyBlobLength(&pNode->xKey.sKey));` |
|      4551 | 1877 | `		MemObjSetType(pKey,MEMOBJ_STRING);` |
|         - | 1878 | `	}` |
|    141555 | 1879 | `}` |
|         - | 1880 | `#ifndef PH7_DISABLE_DISK_IO` |
|         - | 1881 | `/*` |
|         - | 1882 | ` * Store the address of nodes value in the given container.` |
|         - | 1883 | ` * Refer to the [vfprintf(),vprintf(),vsprintf()] implementations` |
|         - | 1884 | ` * defined in 'builtin.c' for more information.` |
|         - | 1885 | ` */` |
|        12 | 1886 | `PH7_PRIVATE int PH7_HashmapValuesToSet(ph7_hashmap *pMap,SySet *pOut)` |
|         1 | 1887 | `{` |
|        13 | 1888 | `	ph7_hashmap_node *pEntry = pMap->pFirst;` |
|         - | 1889 | `	ph7_value *pValue;` |
|         - | 1890 | `	sxu32 n;` |
|         - | 1891 | `	/* Initialize the container */` |
|        13 | 1892 | `	SySetInit(pOut,&pMap->pVm->sAllocator,sizeof(ph7_value *));` |
|        33 | 1893 | `	for(n = 0 ; n < pMap->nEntry ; n++ ){` |
|         - | 1894 | `		/* Extract node value */` |
|        21 | 1895 | `		pValue = HashmapExtractNodeValue(pEntry);` |
|        21 | 1896 | `		if( pValue ){` |
|        21 | 1897 | `			SySetPut(pOut,(const void *)&pValue);` |
|        10 | 1898 | `		}` |
|         - | 1899 | `		/* Point to the next entry */` |
|        21 | 1900 | `		pEntry = pEntry->pPrev; /* Reverse link */` |
|        11 | 1901 | `	}` |
|         - | 1902 | `	/* Total inserted entries */` |
|        13 | 1903 | `	return (int)SySetUsed(pOut);` |
|         1 | 1904 | `}` |
|         - | 1905 | `#endif /* PH7_DISABLE_BUILTIN_FUNC \|\| PH7_DISABLE_DISK_IO */` |
|         - | 1906 | `/* SPDX-SnippetBegin */` |
|         - | 1907 | `/* SPDX-SnippetCopyrightText: D. Richard Hipp and the SQLite authors <https://sqlite.org/> */` |
|         - | 1908 | `/* SPDX-License-Identifier: blessing */` |
|         - | 1909 | `/*` |
|         - | 1910 | ` * Merge sort.` |
|         - | 1911 | ` * The merge sort implementation is based on the one found in the SQLite3 source tree.` |
|         - | 1912 | ` * Status: Public domain` |
|         - | 1913 | ` */` |
|         - | 1914 | `/* Node comparison callback signature */` |
|         - | 1915 | `typedef sxi32 (*ProcNodeCmp)(ph7_hashmap_node *,ph7_hashmap_node *,void *);` |
|         - | 1916 | `/*` |
|         - | 1917 | `** Inputs:` |
|         - | 1918 | `**   a:       A sorted, null-terminated linked list.  (May be null).` |
|         - | 1919 | `**   b:       A sorted, null-terminated linked list.  (May be null).` |
|         - | 1920 | `**   cmp:     A pointer to the comparison function.` |
|         - | 1921 | `**` |
|         - | 1922 | `** Return Value:` |
|         - | 1923 | `**   A pointer to the head of a sorted list containing the elements` |
|         - | 1924 | `**   of both a and b.` |
|         - | 1925 | `**` |
|         - | 1926 | `** Side effects:` |
|         - | 1927 | `**   The "next","prev" pointers for elements in the lists a and b are` |
|         - | 1928 | `**   changed.` |
|         - | 1929 | `*/` |
|     33950 | 1930 | `static ph7_hashmap_node * HashmapNodeMerge(ph7_hashmap_node *pA,ph7_hashmap_node *pB,ProcNodeCmp xCmp,void *pCmpData)` |
|         5 | 1931 | `{` |
|         - | 1932 | `	ph7_hashmap_node result,*pTail;` |
|         - | 1933 | `    /* Prevent compiler warning */` |
|     33955 | 1934 | `	result.pNext = result.pPrev = 0;` |
|     33955 | 1935 | `	pTail = &result;` |
|    103270 | 1936 | `	while( pA && pB ){` |
|     69320 | 1937 | `		if( xCmp(pA,pB,pCmpData) <= 0 ){` |
|     45911 | 1938 | `			pTail->pPrev = pA;` |
|     45911 | 1939 | `			pA->pNext = pTail;` |
|     45911 | 1940 | `			pTail = pA;` |
|     45911 | 1941 | `			pA = pA->pPrev;` |
|     22941 | 1942 | `		}else{` |
|     23414 | 1943 | `			pTail->pPrev = pB;` |
|     23414 | 1944 | `			pB->pNext = pTail;` |
|     23414 | 1945 | `			pTail = pB;` |
|     23414 | 1946 | `			pB = pB->pPrev;` |
|         - | 1947 | `		}` |
|         5 | 1948 | `	}` |
|     33955 | 1949 | `	if( pA ){` |
|     23779 | 1950 | `		pTail->pPrev = pA;` |
|     23779 | 1951 | `		pA->pNext = pTail;` |
|     22090 | 1952 | `	}else if( pB ){` |
|      9953 | 1953 | `		pTail->pPrev = pB;` |
|      9953 | 1954 | `		pB->pNext = pTail;` |
|      4957 | 1955 | `	}else{` |
|       233 | 1956 | `		pTail->pPrev = pTail->pNext = 0;` |
|         - | 1957 | `	}` |
|     33955 | 1958 | `	return result.pPrev;` |
|         5 | 1959 | `}` |
|         - | 1960 | `/*` |
|         - | 1961 | `** Inputs:` |
|         - | 1962 | `**   Map:       Input hashmap` |
|         - | 1963 | `**   cmp:       A comparison function.` |
|         - | 1964 | `**` |
|         - | 1965 | `** Return Value:` |
|         - | 1966 | `**   Sorted hashmap.` |
|         - | 1967 | `**` |
|         - | 1968 | `** Side effects:` |
|         - | 1969 | `**   The "next" pointers for elements in list are changed.` |
|         - | 1970 | `*/` |
|         - | 1971 | `#define N_SORT_BUCKET  32` |
|       702 | 1972 | `static sxi32 HashmapMergeSort(ph7_hashmap *pMap,ProcNodeCmp xCmp,void *pCmpData)` |
|         5 | 1973 | `{` |
|         - | 1974 | `	ph7_hashmap_node *a[N_SORT_BUCKET], *p,*pIn;` |
|         - | 1975 | `	sxu32 i;` |
|       707 | 1976 | `	SyZero(a,sizeof(a));` |
|         - | 1977 | `	/* Point to the first inserted entry */` |
|       707 | 1978 | `	pIn = pMap->pFirst;` |
|     14133 | 1979 | `	while( pIn ){` |
|     13431 | 1980 | `		p = pIn;` |
|     13431 | 1981 | `		pIn = p->pPrev;` |
|     13431 | 1982 | `		p->pPrev = 0;` |
|     25619 | 1983 | `		for(i=0; i<N_SORT_BUCKET-1; i++){` |
|     25619 | 1984 | `			if( a[i]==0 ){` |
|     13431 | 1985 | `				a[i] = p;` |
|     13431 | 1986 | `				break;` |
|       ! 0 | 1987 | `			}else{` |
|     12193 | 1988 | `				p = HashmapNodeMerge(a[i],p,xCmp,pCmpData);` |
|     12193 | 1989 | `				a[i] = 0;` |
|         - | 1990 | `			}` |
|      6099 | 1991 | `		}` |
|     13431 | 1992 | `		if( i==N_SORT_BUCKET-1 ){` |
|         - | 1993 | `			/* To get here, there need to be 2^(N_SORT_BUCKET) elements in he input list.` |
|         - | 1994 | `			 * But that is impossible.` |
|         - | 1995 | `			 */` |
|       ! 0 | 1996 | `			a[i] = HashmapNodeMerge(a[i], p,xCmp,pCmpData);` |
|       ! 0 | 1997 | `		}` |
|         5 | 1998 | `	}` |
|       707 | 1999 | `	p = a[0];` |
|     22469 | 2000 | `	for(i=1; i<N_SORT_BUCKET; i++){` |
|     21767 | 2001 | `		p = HashmapNodeMerge(p,a[i],xCmp,pCmpData);` |
|     10886 | 2002 | `	}` |
|       707 | 2003 | `	p->pNext = 0;` |
|         - | 2004 | `	/* Reflect the change */` |
|       707 | 2005 | `	pMap->pFirst = p;` |
|         - | 2006 | `	/* Reset the loop cursor */` |
|       707 | 2007 | `	pMap->pCur = pMap->pFirst;` |
|       707 | 2008 | `	return SXRET_OK;` |
|         5 | 2009 | `}` |
|         - | 2010 | `/* SPDX-SnippetEnd */` |
|         - | 2011 | `/*` |
|         - | 2012 | ` * Node comparison callback.` |
|         - | 2013 | ` * used-by: [sort(),asort(),...]` |
|         - | 2014 | ` */` |
|     69105 | 2015 | `static sxi32 HashmapCmpCallback1(ph7_hashmap_node *pA,ph7_hashmap_node *pB,void *pCmpData)` |
|         5 | 2016 | `{` |
|         - | 2017 | `	ph7_value sA,sB;` |
|         - | 2018 | `	sxi32 iFlags;` |
|         - | 2019 | `	int rc;` |
|     69110 | 2020 | `	if( pCmpData == 0 ){` |
|         - | 2021 | `		/* Perform a standard comparison */` |
|     69086 | 2022 | `		rc = HashmapNodeCmp(pA,pB,FALSE);` |
|     69086 | 2023 | `		return rc;` |
|         - | 2024 | `	}` |
|        25 | 2025 | `	iFlags = SX_PTR_TO_INT(pCmpData);` |
|         - | 2026 | `	/* Duplicate node values */` |
|        25 | 2027 | `	PH7_MemObjInit(pA->pMap->pVm,&sA);` |
|        25 | 2028 | `	PH7_MemObjInit(pA->pMap->pVm,&sB);` |
|        25 | 2029 | `	PH7_HashmapExtractNodeValue(pA,&sA,FALSE);` |
|        25 | 2030 | `	PH7_HashmapExtractNodeValue(pB,&sB,FALSE);` |
|        25 | 2031 | `	if( iFlags == 5 ){` |
|         - | 2032 | `		/* String cast */` |
|         - | 2033 | `		const char *zA,*zB;` |
|         - | 2034 | `		sxu32 nA,nB,nMin;` |
|        15 | 2035 | `		if( (sA.iFlags & MEMOBJ_STRING) == 0 ){` |
|       ! 0 | 2036 | `			PH7_MemObjToString(&sA);` |
|       ! 0 | 2037 | `		}` |
|        15 | 2038 | `		if( (sB.iFlags & MEMOBJ_STRING) == 0 ){` |
|       ! 0 | 2039 | `			PH7_MemObjToString(&sB);` |
|       ! 0 | 2040 | `		}` |
|         - | 2041 | `		/* Lexicographic string comparison to avoid numeric string coercion */` |
|        15 | 2042 | `		zA = (const char *)SyBlobData(&sA.sBlob);` |
|        15 | 2043 | `		zB = (const char *)SyBlobData(&sB.sBlob);` |
|        15 | 2044 | `		nA = SyBlobLength(&sA.sBlob);` |
|        15 | 2045 | `		nB = SyBlobLength(&sB.sBlob);` |
|        15 | 2046 | `		nMin = nA < nB ? nA : nB;` |
|        15 | 2047 | `		rc = SyMemcmp(zA,zB,nMin);` |
|        15 | 2048 | `		if( rc == 0 ){` |
|         5 | 2049 | `			if( nA < nB ) rc = -1;` |
|       ! 0 | 2050 | `			else if( nA > nB ) rc = 1;` |
|         2 | 2051 | `		}` |
|         8 | 2052 | `	}else{` |
|         - | 2053 | `		/* Numeric cast */` |
|        11 | 2054 | `		PH7_MemObjToNumeric(&sA);` |
|        11 | 2055 | `		PH7_MemObjToNumeric(&sB);` |
|        11 | 2056 | `		rc = PH7_MemObjCmp(&sA,&sB,FALSE,0);` |
|         - | 2057 | `	}` |
|        25 | 2058 | `	PH7_MemObjRelease(&sA);` |
|        25 | 2059 | `	PH7_MemObjRelease(&sB);` |
|        25 | 2060 | `	return rc;` |
|     34571 | 2061 | `}` |
|         - | 2062 | `/*` |
|         - | 2063 | ` * Node comparison callback: Compare nodes by keys only.` |
|         - | 2064 | ` * used-by: [ksort()]` |
|         - | 2065 | ` */` |
|        14 | 2066 | `static sxi32 HashmapCmpCallback2(ph7_hashmap_node *pA,ph7_hashmap_node *pB,void *pCmpData)` |
|         1 | 2067 | `{` |
|         - | 2068 | `	sxi32 rc;` |
|         7 | 2069 | `	SXUNUSED(pCmpData); /* cc warning */` |
|        15 | 2070 | `	if( pA->iType == HASHMAP_BLOB_NODE && pB->iType == HASHMAP_BLOB_NODE ){` |
|         - | 2071 | `		/* Perform a string comparison */` |
|         5 | 2072 | `		rc = SyBlobCmp(&pA->xKey.sKey,&pB->xKey.sKey);` |
|         3 | 2073 | `	}else{` |
|         - | 2074 | `		SyString sStr;` |
|         - | 2075 | `		sxi64 iA,iB;` |
|         - | 2076 | `		/* Perform a numeric comparison */` |
|        11 | 2077 | `		if( pA->iType == HASHMAP_BLOB_NODE ){` |
|         - | 2078 | `			/* Cast to 64-bit integer */` |
|       ! 0 | 2079 | `			SyStringInitFromBuf(&sStr,SyBlobData(&pA->xKey.sKey),SyBlobLength(&pA->xKey.sKey));` |
|       ! 0 | 2080 | `			if( sStr.nByte < 1 ){` |
|       ! 0 | 2081 | `				iA = 0;` |
|       ! 0 | 2082 | `			}else{` |
|       ! 0 | 2083 | `				SyStrToInt64(sStr.zString,sStr.nByte,(void *)&iA,0);` |
|         - | 2084 | `			}` |
|       ! 0 | 2085 | `		}else{` |
|        11 | 2086 | `			iA = pA->xKey.iKey;` |
|         - | 2087 | `		}` |
|        11 | 2088 | `		if( pB->iType == HASHMAP_BLOB_NODE ){` |
|         - | 2089 | `			/* Cast to 64-bit integer */` |
|       ! 0 | 2090 | `			SyStringInitFromBuf(&sStr,SyBlobData(&pB->xKey.sKey),SyBlobLength(&pB->xKey.sKey));` |
|       ! 0 | 2091 | `			if( sStr.nByte < 1 ){` |
|       ! 0 | 2092 | `				iB = 0;` |
|       ! 0 | 2093 | `			}else{` |
|       ! 0 | 2094 | `				SyStrToInt64(sStr.zString,sStr.nByte,(void *)&iB,0);` |
|         - | 2095 | `			}` |
|       ! 0 | 2096 | `		}else{` |
|        11 | 2097 | `			iB = pB->xKey.iKey;` |
|         - | 2098 | `		}` |
|        11 | 2099 | `		rc = (sxi32)(iA-iB);` |
|         - | 2100 | `	}` |
|         - | 2101 | `	/* Comparison result */` |
|        15 | 2102 | `	return rc;` |
|         1 | 2103 | `}` |
|         - | 2104 | `/*` |
|         - | 2105 | ` * Node comparison callback.` |
|         - | 2106 | ` * Used by: [rsort(),arsort()];` |
|         - | 2107 | ` */` |
|        78 | 2108 | `static sxi32 HashmapCmpCallback3(ph7_hashmap_node *pA,ph7_hashmap_node *pB,void *pCmpData)` |
|         1 | 2109 | `{` |
|         - | 2110 | `	ph7_value sA,sB;` |
|         - | 2111 | `	sxi32 iFlags;` |
|         - | 2112 | `	int rc;` |
|        79 | 2113 | `	if( pCmpData == 0 ){` |
|         - | 2114 | `		/* Perform a standard comparison */` |
|        59 | 2115 | `		rc = HashmapNodeCmp(pA,pB,FALSE);` |
|        59 | 2116 | `		return -rc;` |
|         - | 2117 | `	}` |
|        21 | 2118 | `	iFlags = SX_PTR_TO_INT(pCmpData);` |
|         - | 2119 | `	/* Duplicate node values */` |
|        21 | 2120 | `	PH7_MemObjInit(pA->pMap->pVm,&sA);` |
|        21 | 2121 | `	PH7_MemObjInit(pA->pMap->pVm,&sB);` |
|        21 | 2122 | `	PH7_HashmapExtractNodeValue(pA,&sA,FALSE);` |
|        21 | 2123 | `	PH7_HashmapExtractNodeValue(pB,&sB,FALSE);` |
|        21 | 2124 | `	if( iFlags == 5 ){` |
|         - | 2125 | `		/* String cast */` |
|         - | 2126 | `		const char *zA,*zB;` |
|         - | 2127 | `		sxu32 nA,nB,nMin;` |
|        11 | 2128 | `		if( (sA.iFlags & MEMOBJ_STRING) == 0 ){` |
|       ! 0 | 2129 | `			PH7_MemObjToString(&sA);` |
|       ! 0 | 2130 | `		}` |
|        11 | 2131 | `		if( (sB.iFlags & MEMOBJ_STRING) == 0 ){` |
|       ! 0 | 2132 | `			PH7_MemObjToString(&sB);` |
|       ! 0 | 2133 | `		}` |
|         - | 2134 | `		/* Lexicographic string comparison to avoid numeric string coercion */` |
|        11 | 2135 | `		zA = (const char *)SyBlobData(&sA.sBlob);` |
|        11 | 2136 | `		zB = (const char *)SyBlobData(&sB.sBlob);` |
|        11 | 2137 | `		nA = SyBlobLength(&sA.sBlob);` |
|        11 | 2138 | `		nB = SyBlobLength(&sB.sBlob);` |
|        11 | 2139 | `		nMin = nA < nB ? nA : nB;` |
|        11 | 2140 | `		rc = SyMemcmp(zA,zB,nMin);` |
|        11 | 2141 | `		if( rc == 0 ){` |
|         3 | 2142 | `			if( nA < nB ) rc = -1;` |
|       ! 0 | 2143 | `			else if( nA > nB ) rc = 1;` |
|         1 | 2144 | `		}` |
|         6 | 2145 | `	}else{` |
|         - | 2146 | `		/* Numeric cast */` |
|        11 | 2147 | `		PH7_MemObjToNumeric(&sA);` |
|        11 | 2148 | `		PH7_MemObjToNumeric(&sB);` |
|        11 | 2149 | `		rc = PH7_MemObjCmp(&sA,&sB,FALSE,0);` |
|         - | 2150 | `	}` |
|        21 | 2151 | `	PH7_MemObjRelease(&sA);` |
|        21 | 2152 | `	PH7_MemObjRelease(&sB);` |
|        21 | 2153 | `	return -rc;` |
|        40 | 2154 | `}` |
|         - | 2155 | `/*` |
|         - | 2156 | ` * Node comparison callback: Invoke an user-defined callback for the purpose of node comparison.` |
|         - | 2157 | ` * used-by: [usort(),uasort()]` |
|         - | 2158 | ` */` |
|        88 | 2159 | `static sxi32 HashmapCmpCallback4(ph7_hashmap_node *pA,ph7_hashmap_node *pB,void *pCmpData)` |
|         3 | 2160 | `{` |
|         - | 2161 | `	ph7_value sResult,*pCallback;` |
|         - | 2162 | `	ph7_value *pV1,*pV2;` |
|         - | 2163 | `	ph7_value *apArg[2];  /* Callback arguments */` |
|         - | 2164 | `	sxi32 rc;` |
|         - | 2165 | `	/* Point to the desired callback */` |
|        91 | 2166 | `	pCallback = (ph7_value *)pCmpData;` |
|        91 | 2167 | `	if( pA->pMap->pVm->iCmpCallbackExc ){` |
|         - | 2168 | `		/* A previous comparison already raised: stop invoking the callback so` |
|         - | 2169 | `		 * the exception is not thrown again, and let the sort wind down. */` |
|         6 | 2170 | `		return 0;` |
|         - | 2171 | `	}` |
|         - | 2172 | `	/* initialize the result value */` |
|        87 | 2173 | `	PH7_MemObjInit(pA->pMap->pVm,&sResult);` |
|         - | 2174 | `	/* Extract nodes values */` |
|        87 | 2175 | `	pV1 = HashmapExtractNodeValue(pA);` |
|        87 | 2176 | `	pV2 = HashmapExtractNodeValue(pB);` |
|        87 | 2177 | `	apArg[0] = pV1;` |
|        87 | 2178 | `	apArg[1] = pV2;` |
|         - | 2179 | `	/* Invoke the callback */` |
|        87 | 2180 | `	rc = PH7_VmCallUserFunction(pA->pMap->pVm,pCallback,2,apArg,&sResult);` |
|        87 | 2181 | `	if( rc == PH7_EXCEPTION ){` |
|         - | 2182 | `		/* The comparator raised: flag it so the sort driver aborts and` |
|         - | 2183 | `		 * propagates, and order this pair arbitrarily for the rest of the run. */` |
|         6 | 2184 | `		pA->pMap->pVm->iCmpCallbackExc = 1;` |
|         6 | 2185 | `		rc = 0;` |
|        84 | 2186 | `	}else if( rc != SXRET_OK ){` |
|         - | 2187 | `		/* An error occured while calling user defined function [i.e: not defined] */` |
|       ! 0 | 2188 | `		rc = -1; /* Set a dummy result */` |
|       ! 0 | 2189 | `	}else{` |
|         - | 2190 | `		/* Extract callback result */` |
|        82 | 2191 | `		if((sResult.iFlags & MEMOBJ_INT) == 0 ){` |
|         - | 2192 | `			/* Perform an int cast */` |
|       ! 0 | 2193 | `			PH7_MemObjToInteger(&sResult);` |
|       ! 0 | 2194 | `		}` |
|        82 | 2195 | `		rc = (sxi32)sResult.x.iVal;` |
|         - | 2196 | `	}` |
|        87 | 2197 | `	PH7_MemObjRelease(&sResult);` |
|         - | 2198 | `	/* Callback result */` |
|        87 | 2199 | `	return rc;` |
|        47 | 2200 | `}` |
|         - | 2201 | `/*` |
|         - | 2202 | ` * Node comparison callback: Compare nodes by keys only.` |
|         - | 2203 | ` * used-by: [krsort()]` |
|         - | 2204 | ` */` |
|         4 | 2205 | `static sxi32 HashmapCmpCallback5(ph7_hashmap_node *pA,ph7_hashmap_node *pB,void *pCmpData)` |
|         1 | 2206 | `{` |
|         - | 2207 | `	sxi32 rc;` |
|         2 | 2208 | `	SXUNUSED(pCmpData); /* cc warning */` |
|         5 | 2209 | `	if( pA->iType == HASHMAP_BLOB_NODE && pB->iType == HASHMAP_BLOB_NODE ){` |
|         - | 2210 | `		/* Perform a string comparison */` |
|         5 | 2211 | `		rc = SyBlobCmp(&pA->xKey.sKey,&pB->xKey.sKey);` |
|         3 | 2212 | `	}else{` |
|         - | 2213 | `		SyString sStr;` |
|         - | 2214 | `		sxi64 iA,iB;` |
|         - | 2215 | `		/* Perform a numeric comparison */` |
|       ! 0 | 2216 | `		if( pA->iType == HASHMAP_BLOB_NODE ){` |
|         - | 2217 | `			/* Cast to 64-bit integer */` |
|       ! 0 | 2218 | `			SyStringInitFromBuf(&sStr,SyBlobData(&pA->xKey.sKey),SyBlobLength(&pA->xKey.sKey));` |
|       ! 0 | 2219 | `			if( sStr.nByte < 1 ){` |
|       ! 0 | 2220 | `				iA = 0;` |
|       ! 0 | 2221 | `			}else{` |
|       ! 0 | 2222 | `				SyStrToInt64(sStr.zString,sStr.nByte,(void *)&iA,0);` |
|         - | 2223 | `			}` |
|       ! 0 | 2224 | `		}else{` |
|       ! 0 | 2225 | `			iA = pA->xKey.iKey;` |
|         - | 2226 | `		}` |
|       ! 0 | 2227 | `		if( pB->iType == HASHMAP_BLOB_NODE ){` |
|         - | 2228 | `			/* Cast to 64-bit integer */` |
|       ! 0 | 2229 | `			SyStringInitFromBuf(&sStr,SyBlobData(&pB->xKey.sKey),SyBlobLength(&pB->xKey.sKey));` |
|       ! 0 | 2230 | `			if( sStr.nByte < 1 ){` |
|       ! 0 | 2231 | `				iB = 0;` |
|       ! 0 | 2232 | `			}else{` |
|       ! 0 | 2233 | `				SyStrToInt64(sStr.zString,sStr.nByte,(void *)&iB,0);` |
|         - | 2234 | `			}` |
|       ! 0 | 2235 | `		}else{` |
|       ! 0 | 2236 | `			iB = pB->xKey.iKey;` |
|         - | 2237 | `		}` |
|       ! 0 | 2238 | `		rc = (sxi32)(iA-iB);` |
|         - | 2239 | `	}` |
|         5 | 2240 | `	return -rc; /* Reverse result */` |
|         1 | 2241 | `}` |
|         - | 2242 | `/*` |
|         - | 2243 | ` * Node comparison callback: Invoke an user-defined callback for the purpose of node comparison.` |
|         - | 2244 | ` * used-by: [uksort()]` |
|         - | 2245 | ` */` |
|         6 | 2246 | `static sxi32 HashmapCmpCallback6(ph7_hashmap_node *pA,ph7_hashmap_node *pB,void *pCmpData)` |
|         1 | 2247 | `{` |
|         - | 2248 | `	ph7_value sResult,*pCallback;` |
|         - | 2249 | `	ph7_value *apArg[2];  /* Callback arguments */` |
|         - | 2250 | `	ph7_value sK1,sK2;` |
|         - | 2251 | `	sxi32 rc;` |
|         - | 2252 | `	/* Point to the desired callback */` |
|         7 | 2253 | `	pCallback = (ph7_value *)pCmpData;` |
|         7 | 2254 | `	if( pA->pMap->pVm->iCmpCallbackExc ){` |
|         - | 2255 | `		/* A previous comparison already raised: stop invoking the callback so` |
|         - | 2256 | `		 * the exception is not thrown again, and let the sort wind down. */` |
|       ! 0 | 2257 | `		return 0;` |
|         - | 2258 | `	}` |
|         - | 2259 | `	/* initialize the result value */` |
|         7 | 2260 | `	PH7_MemObjInit(pA->pMap->pVm,&sResult);` |
|         7 | 2261 | `	PH7_MemObjInit(pA->pMap->pVm,&sK1);` |
|         7 | 2262 | `	PH7_MemObjInit(pA->pMap->pVm,&sK2);` |
|         - | 2263 | `	/* Extract nodes keys */` |
|         7 | 2264 | `	PH7_HashmapExtractNodeKey(pA,&sK1);` |
|         7 | 2265 | `	PH7_HashmapExtractNodeKey(pB,&sK2);` |
|         7 | 2266 | `	apArg[0] = &sK1;` |
|         7 | 2267 | `	apArg[1] = &sK2;` |
|         - | 2268 | `	/* Mark keys as constants */` |
|         7 | 2269 | `	sK1.nIdx = SXU32_HIGH;` |
|         7 | 2270 | `	sK2.nIdx = SXU32_HIGH;` |
|         - | 2271 | `	/* Invoke the callback */` |
|         7 | 2272 | `	rc = PH7_VmCallUserFunction(pA->pMap->pVm,pCallback,2,apArg,&sResult);` |
|         7 | 2273 | `	if( rc == PH7_EXCEPTION ){` |
|         - | 2274 | `		/* The comparator raised: flag it so the sort driver aborts and` |
|         - | 2275 | `		 * propagates, and order this pair arbitrarily for the rest of the run. */` |
|       ! 0 | 2276 | `		pA->pMap->pVm->iCmpCallbackExc = 1;` |
|       ! 0 | 2277 | `		rc = 0;` |
|         7 | 2278 | `	}else if( rc != SXRET_OK ){` |
|         - | 2279 | `		/* An error occured while calling user defined function [i.e: not defined] */` |
|       ! 0 | 2280 | `		rc = -1; /* Set a dummy result */` |
|       ! 0 | 2281 | `	}else{` |
|         - | 2282 | `		/* Extract callback result */` |
|         7 | 2283 | `		if((sResult.iFlags & MEMOBJ_INT) == 0 ){` |
|         - | 2284 | `			/* Perform an int cast */` |
|       ! 0 | 2285 | `			PH7_MemObjToInteger(&sResult);` |
|       ! 0 | 2286 | `		}` |
|         7 | 2287 | `		rc = (sxi32)sResult.x.iVal;` |
|         - | 2288 | `	}` |
|         7 | 2289 | `	PH7_MemObjRelease(&sResult);` |
|         7 | 2290 | `	PH7_MemObjRelease(&sK1);` |
|         7 | 2291 | `	PH7_MemObjRelease(&sK2);` |
|         - | 2292 | `	/* Callback result */` |
|         7 | 2293 | `	return rc;` |
|         4 | 2294 | `}` |
|         - | 2295 | `/*` |
|         - | 2296 | ` * Node comparison callback: Random node comparison.` |
|         - | 2297 | ` * used-by: [shuffle()]` |
|         - | 2298 | ` */` |
|        20 | 2299 | `static sxi32 HashmapCmpCallback7(ph7_hashmap_node *pA,ph7_hashmap_node *pB,void *pCmpData)` |
|         1 | 2300 | `{` |
|         - | 2301 | `	sxu32 n;` |
|        11 | 2302 | `	SXUNUSED(pB); /* cc warning */` |
|        11 | 2303 | `	SXUNUSED(pCmpData);` |
|         - | 2304 | `	/* Grab a random number */` |
|        21 | 2305 | `	n = PH7_VmRandomNum(pA->pMap->pVm);` |
|         - | 2306 | `	/* if the random number is odd then the first node 'pA' is greater then` |
|         - | 2307 | `	 * the second node 'pB'. Otherwise the reverse is assumed.` |
|         - | 2308 | `	 */` |
|        21 | 2309 | `	return n&1 ? 1 : -1;` |
|         1 | 2310 | `}` |
|         - | 2311 | `/*` |
|         - | 2312 | ` * Rehash all nodes keys after a merge-sort have been applied.` |
|         - | 2313 | ` * Used by [sort(),usort() and rsort()].` |
|         - | 2314 | ` */` |
|       654 | 2315 | `static void HashmapSortRehash(ph7_hashmap *pMap)` |
|         5 | 2316 | `{` |
|         - | 2317 | `	ph7_hashmap_node *p,*pLast;` |
|         - | 2318 | `	sxu32 i;` |
|         - | 2319 | `	/* Rehash all entries */` |
|       659 | 2320 | `	pLast = p = pMap->pFirst;` |
|       659 | 2321 | `	pMap->iNextIdx = 0; /* Reset the automatic index */` |
|       659 | 2322 | `	i = 0;` |
|      6955 | 2323 | `	for( ;; ){` |
|     13915 | 2324 | `		if( i >= pMap->nEntry ){` |
|       659 | 2325 | `			pMap->pLast = pLast; /* Fix the last link broken by the merge-sort */` |
|       659 | 2326 | `			break;` |
|         - | 2327 | `		}` |
|     13261 | 2328 | `		if( p->iType == HASHMAP_BLOB_NODE ){` |
|         - | 2329 | `			/* Do not maintain index association as requested by the PHP specification */` |
|         5 | 2330 | `			SyBlobRelease(&p->xKey.sKey);` |
|         - | 2331 | `			/* Change key type */` |
|         5 | 2332 | `			p->iType = HASHMAP_INT_NODE;` |
|         2 | 2333 | `		}` |
|     13261 | 2334 | `		HashmapRehashIntNode(p);` |
|         - | 2335 | `		/* Point to the next entry */` |
|     13261 | 2336 | `		i++;` |
|     13261 | 2337 | `		pLast = p;` |
|     13261 | 2338 | `		p = p->pPrev; /* Reverse link */` |
|         5 | 2339 | `	}` |
|       659 | 2340 | `}` |
|         - | 2341 | `/*` |
|         - | 2342 | ` * Array functions implementation.` |
|         - | 2343 | ` * Status:` |
|         - | 2344 | ` *  Stable.` |
|         - | 2345 | ` */` |
|         - | 2346 | `/*` |
|         - | 2347 | ` * bool sort(array &$array[,int $sort_flags = SORT_REGULAR ] )` |
|         - | 2348 | ` * Sort an array.` |
|         - | 2349 | ` * Parameters` |
|         - | 2350 | ` *  $array` |
|         - | 2351 | ` *   The input array.` |
|         - | 2352 | ` * $sort_flags` |
|         - | 2353 | ` *  The optional second parameter sort_flags may be used to modify the sorting behavior using these values:` |
|         - | 2354 | ` *  Sorting type flags:` |
|         - | 2355 | ` *   SORT_REGULAR - compare items normally (don't change types)` |
|         - | 2356 | ` *   SORT_NUMERIC - compare items numerically` |
|         - | 2357 | ` *   SORT_STRING - compare items as strings` |
|         - | 2358 | ` * Return` |
|         - | 2359 | ` *  TRUE on success or FALSE on failure.` |
|         - | 2360 | ` *` |
|         - | 2361 | ` */` |
|       986 | 2362 | `static int ph7_hashmap_sort(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|         5 | 2363 | `{` |
|         - | 2364 | `	ph7_hashmap *pMap;` |
|         - | 2365 | `	/* Make sure we are dealing with a valid hashmap */` |
|       991 | 2366 | `	if( nArg < 1 \|\| !ph7_value_is_array(apArg[0]) ){` |
|         - | 2367 | `		/* Missing/Invalid arguments,return FALSE */` |
|       ! 0 | 2368 | `		ph7_result_bool(pCtx,0);` |
|       ! 0 | 2369 | `		return PH7_OK;` |
|         - | 2370 | `	}` |
|         - | 2371 | `	/* Point to the internal representation of the input hashmap */` |
|       991 | 2372 | `	PH7_HashmapCowSeparate(pCtx->pVm, apArg[0]);` |
|       991 | 2373 | `	pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|       991 | 2374 | `	if( pMap->nEntry > 1 ){` |
|       641 | 2375 | `		sxi32 iCmpFlags = 0;` |
|       641 | 2376 | `		if( nArg > 1 ){` |
|         - | 2377 | `			/* Extract comparison flags */` |
|         3 | 2378 | `			iCmpFlags = ph7_value_to_int(apArg[1]);` |
|         3 | 2379 | `			if( iCmpFlags == 3 /* SORT_REGULAR */ ){` |
|       ! 0 | 2380 | `				iCmpFlags = 0; /* Standard comparison */` |
|       ! 0 | 2381 | `			}` |
|         1 | 2382 | `		}` |
|         - | 2383 | `		/* Do the merge sort */` |
|       641 | 2384 | `		HashmapMergeSort(pMap,HashmapCmpCallback1,SX_INT_TO_PTR(iCmpFlags));` |
|         - | 2385 | `		/* Rehash [Do not maintain index association as requested by the PHP specification] */` |
|       641 | 2386 | `		HashmapSortRehash(pMap);` |
|       318 | 2387 | `	}` |
|         - | 2388 | `	/* All done,return TRUE */` |
|       991 | 2389 | `	ph7_result_bool(pCtx,1);` |
|       991 | 2390 | `	return PH7_OK;` |
|       498 | 2391 | `}` |
|         - | 2392 | `/*` |
|         - | 2393 | ` * bool asort(array &$array[,int $sort_flags = SORT_REGULAR ] )` |
|         - | 2394 | ` *  Sort an array and maintain index association.` |
|         - | 2395 | ` * Parameters` |
|         - | 2396 | ` *  $array` |
|         - | 2397 | ` *   The input array.` |
|         - | 2398 | ` * $sort_flags` |
|         - | 2399 | ` *  The optional second parameter sort_flags may be used to modify the sorting behavior using these values:` |
|         - | 2400 | ` *  Sorting type flags:` |
|         - | 2401 | ` *   SORT_REGULAR - compare items normally (don't change types)` |
|         - | 2402 | ` *   SORT_NUMERIC - compare items numerically` |
|         - | 2403 | ` *   SORT_STRING - compare items as strings` |
|         - | 2404 | ` * Return` |
|         - | 2405 | ` *  TRUE on success or FALSE on failure.` |
|         - | 2406 | ` */` |
|        32 | 2407 | `static int ph7_hashmap_asort(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|         5 | 2408 | `{` |
|         - | 2409 | `	ph7_hashmap *pMap;` |
|         - | 2410 | `	/* PHP 8: ArgumentCountError if no arguments */` |
|        37 | 2411 | `	if( nArg < 1 ){` |
|         3 | 2412 | `		return PH7_VmThrowException(pCtx,` |
|         - | 2413 | `			"ArgumentCountError",` |
|         - | 2414 | `			"asort() expects at least 1 argument, 0 given"` |
|         - | 2415 | `			);` |
|         - | 2416 | `	}` |
|         - | 2417 | `	/* PHP 8: TypeError if first argument is not an array */` |
|        35 | 2418 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|        16 | 2419 | `		return PH7_VmThrowException(pCtx,` |
|         - | 2420 | `			"TypeError",` |
|         - | 2421 | `			"asort(): Argument #1 ($array) must be of type array, %s given",` |
|         4 | 2422 | `			ph7_type_name(apArg[0])` |
|         - | 2423 | `			);` |
|         - | 2424 | `	}` |
|         - | 2425 | `	/* Point to the internal representation of the input hashmap */` |
|        23 | 2426 | `	PH7_HashmapCowSeparate(pCtx->pVm, apArg[0]);` |
|        23 | 2427 | `	pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|        23 | 2428 | `	if( pMap->nEntry > 1 ){` |
|        19 | 2429 | `		sxi32 iCmpFlags = 0;` |
|        19 | 2430 | `		if( nArg > 1 ){` |
|         - | 2431 | `			/* Extract comparison flags */` |
|         5 | 2432 | `			iCmpFlags = ph7_value_to_int(apArg[1]);` |
|         5 | 2433 | `			if( iCmpFlags == 3 /* SORT_REGULAR */ ){` |
|       ! 0 | 2434 | `				iCmpFlags = 0; /* Standard comparison */` |
|       ! 0 | 2435 | `			}` |
|         2 | 2436 | `		}` |
|         - | 2437 | `		/* Do the merge sort */` |
|        19 | 2438 | `		HashmapMergeSort(pMap,HashmapCmpCallback1,SX_INT_TO_PTR(iCmpFlags));` |
|         - | 2439 | `		/* Fix the last link broken by the merge */` |
|        45 | 2440 | `		while(pMap->pLast->pPrev){` |
|        27 | 2441 | `			pMap->pLast = pMap->pLast->pPrev;` |
|         1 | 2442 | `		}` |
|         9 | 2443 | `	}` |
|         - | 2444 | `	/* All done,return TRUE */` |
|        23 | 2445 | `	ph7_result_bool(pCtx,1);` |
|        23 | 2446 | `	return PH7_OK;` |
|        21 | 2447 | `}` |
|         - | 2448 | `/*` |
|         - | 2449 | ` * bool arsort(array &$array[,int $sort_flags = SORT_REGULAR ] )` |
|         - | 2450 | ` *  Sort an array in reverse order and maintain index association.` |
|         - | 2451 | ` * Parameters` |
|         - | 2452 | ` *  $array` |
|         - | 2453 | ` *   The input array.` |
|         - | 2454 | ` * $sort_flags` |
|         - | 2455 | ` *  The optional second parameter sort_flags may be used to modify the sorting behavior using these values:` |
|         - | 2456 | ` *  Sorting type flags:` |
|         - | 2457 | ` *   SORT_REGULAR - compare items normally (don't change types)` |
|         - | 2458 | ` *   SORT_NUMERIC - compare items numerically` |
|         - | 2459 | ` *   SORT_STRING - compare items as strings` |
|         - | 2460 | ` * Return` |
|         - | 2461 | ` *  TRUE on success or FALSE on failure.` |
|         - | 2462 | ` */` |
|        32 | 2463 | `static int ph7_hashmap_arsort(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|         5 | 2464 | `{` |
|         - | 2465 | `	ph7_hashmap *pMap;` |
|         - | 2466 | `	/* PHP 8: ArgumentCountError if no arguments */` |
|        37 | 2467 | `	if( nArg < 1 ){` |
|         3 | 2468 | `		return PH7_VmThrowException(pCtx,` |
|         - | 2469 | `			"ArgumentCountError",` |
|         - | 2470 | `			"arsort() expects at least 1 argument, 0 given"` |
|         - | 2471 | `			);` |
|         - | 2472 | `	}` |
|         - | 2473 | `	/* PHP 8: TypeError if first argument is not an array */` |
|        35 | 2474 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|        16 | 2475 | `		return PH7_VmThrowException(pCtx,` |
|         - | 2476 | `			"TypeError",` |
|         - | 2477 | `			"arsort(): Argument #1 ($array) must be of type array, %s given",` |
|         4 | 2478 | `			ph7_type_name(apArg[0])` |
|         - | 2479 | `			);` |
|         - | 2480 | `	}` |
|         - | 2481 | `	/* Point to the internal representation of the input hashmap */` |
|        23 | 2482 | `	PH7_HashmapCowSeparate(pCtx->pVm, apArg[0]);` |
|        23 | 2483 | `	pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|        23 | 2484 | `	if( pMap->nEntry > 1 ){` |
|        19 | 2485 | `		sxi32 iCmpFlags = 0;` |
|        19 | 2486 | `		if( nArg > 1 ){` |
|         - | 2487 | `			/* Extract comparison flags */` |
|         5 | 2488 | `			iCmpFlags = ph7_value_to_int(apArg[1]);` |
|         5 | 2489 | `			if( iCmpFlags == 3 /* SORT_REGULAR */ ){` |
|       ! 0 | 2490 | `				iCmpFlags = 0; /* Standard comparison */` |
|       ! 0 | 2491 | `			}` |
|         2 | 2492 | `		}` |
|         - | 2493 | `		/* Do the merge sort */` |
|        19 | 2494 | `		HashmapMergeSort(pMap,HashmapCmpCallback3,SX_INT_TO_PTR(iCmpFlags));` |
|         - | 2495 | `		/* Fix the last link broken by the merge */` |
|        35 | 2496 | `		while(pMap->pLast->pPrev){` |
|        17 | 2497 | `			pMap->pLast = pMap->pLast->pPrev;` |
|         1 | 2498 | `		}` |
|         9 | 2499 | `	}` |
|         - | 2500 | `	/* All done,return TRUE */` |
|        23 | 2501 | `	ph7_result_bool(pCtx,1);` |
|        23 | 2502 | `	return PH7_OK;` |
|        21 | 2503 | `}` |
|         - | 2504 | `/*` |
|         - | 2505 | ` * bool ksort(array &$array[,int $sort_flags = SORT_REGULAR ] )` |
|         - | 2506 | ` *  Sort an array by key.` |
|         - | 2507 | ` * Parameters` |
|         - | 2508 | ` *  $array` |
|         - | 2509 | ` *   The input array.` |
|         - | 2510 | ` * $sort_flags` |
|         - | 2511 | ` *  The optional second parameter sort_flags may be used to modify the sorting behavior using these values:` |
|         - | 2512 | ` *  Sorting type flags:` |
|         - | 2513 | ` *   SORT_REGULAR - compare items normally (don't change types)` |
|         - | 2514 | ` *   SORT_NUMERIC - compare items numerically` |
|         - | 2515 | ` *   SORT_STRING - compare items as strings` |
|         - | 2516 | ` * Return` |
|         - | 2517 | ` *  TRUE on success or FALSE on failure.` |
|         - | 2518 | ` */` |
|         4 | 2519 | `static int ph7_hashmap_ksort(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|         1 | 2520 | `{` |
|         - | 2521 | `	ph7_hashmap *pMap;` |
|         - | 2522 | `	/* Make sure we are dealing with a valid hashmap */` |
|         5 | 2523 | `	if( nArg < 1 \|\| !ph7_value_is_array(apArg[0]) ){` |
|         - | 2524 | `		/* Missing/Invalid arguments,return FALSE */` |
|       ! 0 | 2525 | `		ph7_result_bool(pCtx,0);` |
|       ! 0 | 2526 | `		return PH7_OK;` |
|         - | 2527 | `	}` |
|         - | 2528 | `	/* Point to the internal representation of the input hashmap */` |
|         5 | 2529 | `	PH7_HashmapCowSeparate(pCtx->pVm, apArg[0]);` |
|         5 | 2530 | `	pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|         5 | 2531 | `	if( pMap->nEntry > 1 ){` |
|         5 | 2532 | `		sxi32 iCmpFlags = 0;` |
|         5 | 2533 | `		if( nArg > 1 ){` |
|         - | 2534 | `			/* Extract comparison flags */` |
|       ! 0 | 2535 | `			iCmpFlags = ph7_value_to_int(apArg[1]);` |
|       ! 0 | 2536 | `			if( iCmpFlags == 3 /* SORT_REGULAR */ ){` |
|       ! 0 | 2537 | `				iCmpFlags = 0; /* Standard comparison */` |
|       ! 0 | 2538 | `			}` |
|       ! 0 | 2539 | `		}` |
|         - | 2540 | `		/* Do the merge sort */` |
|         5 | 2541 | `		HashmapMergeSort(pMap,HashmapCmpCallback2,SX_INT_TO_PTR(iCmpFlags));` |
|         - | 2542 | `		/* Fix the last link broken by the merge */` |
|        15 | 2543 | `		while(pMap->pLast->pPrev){` |
|        11 | 2544 | `			pMap->pLast = pMap->pLast->pPrev;` |
|         1 | 2545 | `		}` |
|         2 | 2546 | `	}` |
|         - | 2547 | `	/* All done,return TRUE */` |
|         5 | 2548 | `	ph7_result_bool(pCtx,1);` |
|         5 | 2549 | `	return PH7_OK;` |
|         3 | 2550 | `}` |
|         - | 2551 | `/*` |
|         - | 2552 | ` * bool krsort(array &$array[,int $sort_flags = SORT_REGULAR ] )` |
|         - | 2553 | ` *  Sort an array by key in reverse order.` |
|         - | 2554 | ` * Parameters` |
|         - | 2555 | ` *  $array` |
|         - | 2556 | ` *   The input array.` |
|         - | 2557 | ` * $sort_flags` |
|         - | 2558 | ` *  The optional second parameter sort_flags may be used to modify the sorting behavior using these values:` |
|         - | 2559 | ` *  Sorting type flags:` |
|         - | 2560 | ` *   SORT_REGULAR - compare items normally (don't change types)` |
|         - | 2561 | ` *   SORT_NUMERIC - compare items numerically` |
|         - | 2562 | ` *   SORT_STRING - compare items as strings` |
|         - | 2563 | ` * Return` |
|         - | 2564 | ` *  TRUE on success or FALSE on failure.` |
|         - | 2565 | ` */` |
|         2 | 2566 | `static int ph7_hashmap_krsort(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|         1 | 2567 | `{` |
|         - | 2568 | `	ph7_hashmap *pMap;` |
|         - | 2569 | `	/* Make sure we are dealing with a valid hashmap */` |
|         3 | 2570 | `	if( nArg < 1 \|\| !ph7_value_is_array(apArg[0]) ){` |
|         - | 2571 | `		/* Missing/Invalid arguments,return FALSE */` |
|       ! 0 | 2572 | `		ph7_result_bool(pCtx,0);` |
|       ! 0 | 2573 | `		return PH7_OK;` |
|         - | 2574 | `	}` |
|         - | 2575 | `	/* Point to the internal representation of the input hashmap */` |
|         3 | 2576 | `	PH7_HashmapCowSeparate(pCtx->pVm, apArg[0]);` |
|         3 | 2577 | `	pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|         3 | 2578 | `	if( pMap->nEntry > 1 ){` |
|         3 | 2579 | `		sxi32 iCmpFlags = 0;` |
|         3 | 2580 | `		if( nArg > 1 ){` |
|         - | 2581 | `			/* Extract comparison flags */` |
|       ! 0 | 2582 | `			iCmpFlags = ph7_value_to_int(apArg[1]);` |
|       ! 0 | 2583 | `			if( iCmpFlags == 3 /* SORT_REGULAR */ ){` |
|       ! 0 | 2584 | `				iCmpFlags = 0; /* Standard comparison */` |
|       ! 0 | 2585 | `			}` |
|       ! 0 | 2586 | `		}` |
|         - | 2587 | `		/* Do the merge sort */` |
|         3 | 2588 | `		HashmapMergeSort(pMap,HashmapCmpCallback5,SX_INT_TO_PTR(iCmpFlags));` |
|         - | 2589 | `		/* Fix the last link broken by the merge */` |
|         7 | 2590 | `		while(pMap->pLast->pPrev){` |
|         5 | 2591 | `			pMap->pLast = pMap->pLast->pPrev;` |
|         1 | 2592 | `		}` |
|         1 | 2593 | `	}` |
|         - | 2594 | `	/* All done,return TRUE */` |
|         3 | 2595 | `	ph7_result_bool(pCtx,1);` |
|         3 | 2596 | `	return PH7_OK;` |
|         2 | 2597 | `}` |
|         - | 2598 | `/*` |
|         - | 2599 | ` * bool rsort(array &$array[,int $sort_flags = SORT_REGULAR ] )` |
|         - | 2600 | ` * Sort an array in reverse order.` |
|         - | 2601 | ` * Parameters` |
|         - | 2602 | ` *  $array` |
|         - | 2603 | ` *   The input array.` |
|         - | 2604 | ` * $sort_flags` |
|         - | 2605 | ` *  The optional second parameter sort_flags may be used to modify the sorting behavior using these values:` |
|         - | 2606 | ` *  Sorting type flags:` |
|         - | 2607 | ` *   SORT_REGULAR - compare items normally (don't change types)` |
|         - | 2608 | ` *   SORT_NUMERIC - compare items numerically` |
|         - | 2609 | ` *   SORT_STRING - compare items as strings` |
|         - | 2610 | ` * Return` |
|         - | 2611 | ` *  TRUE on success or FALSE on failure.` |
|         - | 2612 | ` */` |
|         2 | 2613 | `static int ph7_hashmap_rsort(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|         1 | 2614 | `{` |
|         - | 2615 | `	ph7_hashmap *pMap;` |
|         - | 2616 | `	/* Make sure we are dealing with a valid hashmap */` |
|         3 | 2617 | `	if( nArg < 1 \|\| !ph7_value_is_array(apArg[0]) ){` |
|         - | 2618 | `		/* Missing/Invalid arguments,return FALSE */` |
|       ! 0 | 2619 | `		ph7_result_bool(pCtx,0);` |
|       ! 0 | 2620 | `		return PH7_OK;` |
|         - | 2621 | `	}` |
|         - | 2622 | `	/* Point to the internal representation of the input hashmap */` |
|         3 | 2623 | `	PH7_HashmapCowSeparate(pCtx->pVm, apArg[0]);` |
|         3 | 2624 | `	pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|         3 | 2625 | `	if( pMap->nEntry > 1 ){` |
|         3 | 2626 | `		sxi32 iCmpFlags = 0;` |
|         3 | 2627 | `		if( nArg > 1 ){` |
|         - | 2628 | `			/* Extract comparison flags */` |
|       ! 0 | 2629 | `			iCmpFlags = ph7_value_to_int(apArg[1]);` |
|       ! 0 | 2630 | `			if( iCmpFlags == 3 /* SORT_REGULAR */ ){` |
|       ! 0 | 2631 | `				iCmpFlags = 0; /* Standard comparison */` |
|       ! 0 | 2632 | `			}` |
|       ! 0 | 2633 | `		}` |
|         - | 2634 | `		/* Do the merge sort */` |
|         3 | 2635 | `		HashmapMergeSort(pMap,HashmapCmpCallback3,SX_INT_TO_PTR(iCmpFlags));` |
|         - | 2636 | `		/* Rehash [Do not maintain index association as requested by the PHP specification] */` |
|         3 | 2637 | `		HashmapSortRehash(pMap);` |
|         1 | 2638 | `	}` |
|         - | 2639 | `	/* All done,return TRUE */` |
|         3 | 2640 | `	ph7_result_bool(pCtx,1);` |
|         3 | 2641 | `	return PH7_OK;` |
|         2 | 2642 | `}` |
|         - | 2643 | `/*` |
|         - | 2644 | ` * bool usort(array &$array,callable $cmp_function)` |
|         - | 2645 | ` *  Sort an array by values using a user-defined comparison function.` |
|         - | 2646 | ` * Parameters` |
|         - | 2647 | ` *  $array` |
|         - | 2648 | ` *   The input array.` |
|         - | 2649 | ` * $cmp_function` |
|         - | 2650 | ` *  The comparison function must return an integer less than, equal to, or greater` |
|         - | 2651 | ` *  than zero if the first argument is considered to be respectively less than, equal` |
|         - | 2652 | ` *  to, or greater than the second.` |
|         - | 2653 | ` *    int callback ( mixed $a, mixed $b )` |
|         - | 2654 | ` * Return` |
|         - | 2655 | ` *  TRUE on success or FALSE on failure.` |
|         - | 2656 | ` */` |
|        12 | 2657 | `static int ph7_hashmap_usort(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|         3 | 2658 | `{` |
|         - | 2659 | `	ph7_hashmap *pMap;` |
|         - | 2660 | `	/* Make sure we are dealing with a valid hashmap */` |
|        15 | 2661 | `	if( nArg < 1 \|\| !ph7_value_is_array(apArg[0]) ){` |
|         - | 2662 | `		/* Missing/Invalid arguments,return FALSE */` |
|       ! 0 | 2663 | `		ph7_result_bool(pCtx,0);` |
|       ! 0 | 2664 | `		return PH7_OK;` |
|         - | 2665 | `	}` |
|         - | 2666 | `	/* Point to the internal representation of the input hashmap */` |
|        15 | 2667 | `	PH7_HashmapCowSeparate(pCtx->pVm, apArg[0]);` |
|        15 | 2668 | `	pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|        15 | 2669 | `	if( pMap->nEntry > 1 ){` |
|        15 | 2670 | `		ph7_value *pCallback = 0;` |
|         - | 2671 | `		ProcNodeCmp xCmp;` |
|        15 | 2672 | `		xCmp = HashmapCmpCallback4; /* User-defined function as the comparison callback */` |
|        15 | 2673 | `		if( nArg > 1 && ph7_value_is_callable(apArg[1]) ){` |
|         - | 2674 | `			/* Point to the desired callback */` |
|        15 | 2675 | `			pCallback = apArg[1];` |
|         9 | 2676 | `		}else{` |
|         - | 2677 | `			/* Use the default comparison function */` |
|       ! 0 | 2678 | `			xCmp = HashmapCmpCallback1;` |
|         - | 2679 | `		}` |
|         - | 2680 | `		/* Do the merge sort */` |
|        15 | 2681 | `		pCtx->pVm->iCmpCallbackExc = 0;` |
|        15 | 2682 | `		HashmapMergeSort(pMap,xCmp,pCallback);` |
|         - | 2683 | `		/* Rehash [Do not maintain index association as requested by the PHP specification] */` |
|        15 | 2684 | `		HashmapSortRehash(pMap);` |
|        15 | 2685 | `		if( pCtx->pVm->iCmpCallbackExc ){` |
|         - | 2686 | `			/* The comparison callback raised: propagate so the dispatcher unwinds. */` |
|         6 | 2687 | `			pCtx->pVm->iCmpCallbackExc = 0;` |
|         6 | 2688 | `			return PH7_EXCEPTION;` |
|         - | 2689 | `		}` |
|         4 | 2690 | `	}` |
|         - | 2691 | `	/* All done,return TRUE */` |
|        10 | 2692 | `	ph7_result_bool(pCtx,1);` |
|        10 | 2693 | `	return PH7_OK;` |
|         9 | 2694 | `}` |
|         - | 2695 | `/*` |
|         - | 2696 | ` * bool uasort(array &$array,callable $cmp_function)` |
|         - | 2697 | ` *  Sort an array by values using a user-defined comparison function` |
|         - | 2698 | ` *  and maintain index association.` |
|         - | 2699 | ` * Parameters` |
|         - | 2700 | ` *  $array` |
|         - | 2701 | ` *   The input array.` |
|         - | 2702 | ` * $cmp_function` |
|         - | 2703 | ` *  The comparison function must return an integer less than, equal to, or greater` |
|         - | 2704 | ` *  than zero if the first argument is considered to be respectively less than, equal` |
|         - | 2705 | ` *  to, or greater than the second.` |
|         - | 2706 | ` *    int callback ( mixed $a, mixed $b )` |
|         - | 2707 | ` * Return` |
|         - | 2708 | ` *  TRUE on success or FALSE on failure.` |
|         - | 2709 | ` */` |
|         2 | 2710 | `static int ph7_hashmap_uasort(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|         1 | 2711 | `{` |
|         - | 2712 | `	ph7_hashmap *pMap;` |
|         - | 2713 | `	/* Make sure we are dealing with a valid hashmap */` |
|         3 | 2714 | `	if( nArg < 1 \|\| !ph7_value_is_array(apArg[0]) ){` |
|         - | 2715 | `		/* Missing/Invalid arguments,return FALSE */` |
|       ! 0 | 2716 | `		ph7_result_bool(pCtx,0);` |
|       ! 0 | 2717 | `		return PH7_OK;` |
|         - | 2718 | `	}` |
|         - | 2719 | `	/* Point to the internal representation of the input hashmap */` |
|         3 | 2720 | `	PH7_HashmapCowSeparate(pCtx->pVm, apArg[0]);` |
|         3 | 2721 | `	pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|         3 | 2722 | `	if( pMap->nEntry > 1 ){` |
|         3 | 2723 | `		ph7_value *pCallback = 0;` |
|         - | 2724 | `		ProcNodeCmp xCmp;` |
|         3 | 2725 | `		xCmp = HashmapCmpCallback4; /* User-defined function as the comparison callback */` |
|         3 | 2726 | `		if( nArg > 1 && ph7_value_is_callable(apArg[1]) ){` |
|         - | 2727 | `			/* Point to the desired callback */` |
|         3 | 2728 | `			pCallback = apArg[1];` |
|         2 | 2729 | `		}else{` |
|         - | 2730 | `			/* Use the default comparison function */` |
|       ! 0 | 2731 | `			xCmp = HashmapCmpCallback1;` |
|         - | 2732 | `		}` |
|         - | 2733 | `		/* Do the merge sort */` |
|         3 | 2734 | `		pCtx->pVm->iCmpCallbackExc = 0;` |
|         3 | 2735 | `		HashmapMergeSort(pMap,xCmp,pCallback);` |
|         - | 2736 | `		/* Fix the last link broken by the merge */` |
|         5 | 2737 | `		while(pMap->pLast->pPrev){` |
|         3 | 2738 | `			pMap->pLast = pMap->pLast->pPrev;` |
|         1 | 2739 | `		}` |
|         3 | 2740 | `		if( pCtx->pVm->iCmpCallbackExc ){` |
|         - | 2741 | `			/* The comparison callback raised: propagate so the dispatcher unwinds. */` |
|       ! 0 | 2742 | `			pCtx->pVm->iCmpCallbackExc = 0;` |
|       ! 0 | 2743 | `			return PH7_EXCEPTION;` |
|         - | 2744 | `		}` |
|         1 | 2745 | `	}` |
|         - | 2746 | `	/* All done,return TRUE */` |
|         3 | 2747 | `	ph7_result_bool(pCtx,1);` |
|         3 | 2748 | `	return PH7_OK;` |
|         2 | 2749 | `}` |
|         - | 2750 | `/*` |
|         - | 2751 | ` * bool uksort(array &$array,callable $cmp_function)` |
|         - | 2752 | ` *  Sort an array by keys using a user-defined comparison` |
|         - | 2753 | ` *  function and maintain index association.` |
|         - | 2754 | ` * Parameters` |
|         - | 2755 | ` *  $array` |
|         - | 2756 | ` *   The input array.` |
|         - | 2757 | ` * $cmp_function` |
|         - | 2758 | ` *  The comparison function must return an integer less than, equal to, or greater` |
|         - | 2759 | ` *  than zero if the first argument is considered to be respectively less than, equal` |
|         - | 2760 | ` *  to, or greater than the second.` |
|         - | 2761 | ` *    int callback ( mixed $a, mixed $b )` |
|         - | 2762 | ` * Return` |
|         - | 2763 | ` *  TRUE on success or FALSE on failure.` |
|         - | 2764 | ` */` |
|         2 | 2765 | `static int ph7_hashmap_uksort(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|         1 | 2766 | `{` |
|         - | 2767 | `	ph7_hashmap *pMap;` |
|         - | 2768 | `	/* Make sure we are dealing with a valid hashmap */` |
|         3 | 2769 | `	if( nArg < 1 \|\| !ph7_value_is_array(apArg[0]) ){` |
|         - | 2770 | `		/* Missing/Invalid arguments,return FALSE */` |
|       ! 0 | 2771 | `		ph7_result_bool(pCtx,0);` |
|       ! 0 | 2772 | `		return PH7_OK;` |
|         - | 2773 | `	}` |
|         - | 2774 | `	/* Point to the internal representation of the input hashmap */` |
|         3 | 2775 | `	PH7_HashmapCowSeparate(pCtx->pVm, apArg[0]);` |
|         3 | 2776 | `	pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|         3 | 2777 | `	if( pMap->nEntry > 1 ){` |
|         3 | 2778 | `		ph7_value *pCallback = 0;` |
|         - | 2779 | `		ProcNodeCmp xCmp;` |
|         3 | 2780 | `		xCmp = HashmapCmpCallback6; /* User-defined function as the comparison callback */` |
|         3 | 2781 | `		if( nArg > 1 && ph7_value_is_callable(apArg[1]) ){` |
|         - | 2782 | `			/* Point to the desired callback */` |
|         3 | 2783 | `			pCallback = apArg[1];` |
|         2 | 2784 | `		}else{` |
|         - | 2785 | `			/* Use the default comparison function */` |
|       ! 0 | 2786 | `			xCmp = HashmapCmpCallback2;` |
|         - | 2787 | `		}` |
|         - | 2788 | `		/* Do the merge sort */` |
|         3 | 2789 | `		pCtx->pVm->iCmpCallbackExc = 0;` |
|         3 | 2790 | `		HashmapMergeSort(pMap,xCmp,pCallback);` |
|         - | 2791 | `		/* Fix the last link broken by the merge */` |
|         3 | 2792 | `		while(pMap->pLast->pPrev){` |
|       ! 0 | 2793 | `			pMap->pLast = pMap->pLast->pPrev;` |
|       ! 0 | 2794 | `		}` |
|         3 | 2795 | `		if( pCtx->pVm->iCmpCallbackExc ){` |
|         - | 2796 | `			/* The comparison callback raised: propagate so the dispatcher unwinds. */` |
|       ! 0 | 2797 | `			pCtx->pVm->iCmpCallbackExc = 0;` |
|       ! 0 | 2798 | `			return PH7_EXCEPTION;` |
|         - | 2799 | `		}` |
|         1 | 2800 | `	}` |
|         - | 2801 | `	/* All done,return TRUE */` |
|         3 | 2802 | `	ph7_result_bool(pCtx,1);` |
|         3 | 2803 | `	return PH7_OK;` |
|         2 | 2804 | `}` |
|         - | 2805 | `/*` |
|         - | 2806 | ` * bool shuffle(array &$array)` |
|         - | 2807 | ` *  shuffles (randomizes the order of the elements in) an array.` |
|         - | 2808 | ` * Parameters` |
|         - | 2809 | ` *  $array` |
|         - | 2810 | ` *   The input array.` |
|         - | 2811 | ` * Return` |
|         - | 2812 | ` *  TRUE on success or FALSE on failure.` |
|         - | 2813 | ` *` |
|         - | 2814 | ` */` |
|         2 | 2815 | `static int ph7_hashmap_shuffle(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|         1 | 2816 | `{` |
|         - | 2817 | `	ph7_hashmap *pMap;` |
|         - | 2818 | `	/* Make sure we are dealing with a valid hashmap */` |
|         3 | 2819 | `	if( nArg < 1 \|\| !ph7_value_is_array(apArg[0]) ){` |
|         - | 2820 | `		/* Missing/Invalid arguments,return FALSE */` |
|       ! 0 | 2821 | `		ph7_result_bool(pCtx,0);` |
|       ! 0 | 2822 | `		return PH7_OK;` |
|         - | 2823 | `	}` |
|         - | 2824 | `	/* Point to the internal representation of the input hashmap */` |
|         3 | 2825 | `	PH7_HashmapCowSeparate(pCtx->pVm, apArg[0]);` |
|         3 | 2826 | `	pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|         3 | 2827 | `	if( pMap->nEntry > 1 ){` |
|         - | 2828 | `		/* Do the merge sort */` |
|         3 | 2829 | `		HashmapMergeSort(pMap,HashmapCmpCallback7,0);` |
|         - | 2830 | `		/* Fix the last link broken by the merge */` |
|         7 | 2831 | `		while(pMap->pLast->pPrev){` |
|         5 | 2832 | `			pMap->pLast = pMap->pLast->pPrev;` |
|         1 | 2833 | `		}` |
|         1 | 2834 | `	}` |
|         - | 2835 | `	/* All done,return TRUE */` |
|         3 | 2836 | `	ph7_result_bool(pCtx,1);` |
|         3 | 2837 | `	return PH7_OK;` |
|         2 | 2838 | `}` |
|         - | 2839 | `/*` |
|         - | 2840 | ` * int count(array $var [, int $mode = COUNT_NORMAL ])` |
|         - | 2841 | ` *   Count all elements in an array, or something in an object.` |
|         - | 2842 | ` * Parameters` |
|         - | 2843 | ` *  $var` |
|         - | 2844 | ` *   The array or the object.` |
|         - | 2845 | ` * $mode` |
|         - | 2846 | ` *  If the optional mode parameter is set to COUNT_RECURSIVE (or 1), count()` |
|         - | 2847 | ` *  will recursively count the array. This is particularly useful for counting` |
|         - | 2848 | ` *  all the elements of a multidimensional array.` |
|         - | 2849 | ` * Return` |
|         - | 2850 | ` *  Returns the number of elements in the array.` |
|         - | 2851 | ` */` |
|       964 | 2852 | `static int ph7_hashmap_count(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|         5 | 2853 | `{` |
|       969 | 2854 | `	int bRecursive = FALSE;` |
|       969 | 2855 | `	int bCycleDetected = FALSE;` |
|         - | 2856 | `	sxi64 iCount;` |
|       969 | 2857 | `	if( nArg < 1 ){` |
|         3 | 2858 | `		return PH7_VmThrowException(pCtx,` |
|         - | 2859 | `			"ArgumentCountError",` |
|         - | 2860 | `			"count() expects at least 1 argument, 0 given"` |
|         - | 2861 | `			);` |
|         - | 2862 | `	}` |
|       967 | 2863 | `	if( nArg > 2 ){` |
|         4 | 2864 | `		return PH7_VmThrowException(pCtx,` |
|         - | 2865 | `			"ArgumentCountError",` |
|         - | 2866 | `			"count() expects at most 2 arguments, %d given",` |
|         1 | 2867 | `			nArg` |
|         - | 2868 | `			);` |
|         - | 2869 | `	}` |
|         - | 2870 | `	/* PHP validates $mode right after parsing, before the type dispatch, so` |
|         - | 2871 | `	 * an invalid mode raises ValueError whether $value is an array or a` |
|         - | 2872 | `	 * Countable object (the mode is then ignored for the Countable path). */` |
|       965 | 2873 | `	if( nArg > 1 ){` |
|        45 | 2874 | `		sxi32 iMode = ph7_value_to_int(apArg[1]);` |
|        45 | 2875 | `		if( iMode != 0 /* COUNT_NORMAL */ && iMode != 1 /* COUNT_RECURSIVE */ ){` |
|        12 | 2876 | `			return PH7_VmThrowException(pCtx,` |
|         - | 2877 | `				"ValueError",` |
|         - | 2878 | `				"count(): Argument #2 ($mode) must be either COUNT_NORMAL or COUNT_RECURSIVE"` |
|         - | 2879 | `				);` |
|         - | 2880 | `		}` |
|        34 | 2881 | `		bRecursive = iMode == 1;` |
|        16 | 2882 | `	}` |
|       957 | 2883 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|         - | 2884 | `		/* Countable object: dispatch to ->count() */` |
|        35 | 2885 | `		if( apArg[0]->iFlags & MEMOBJ_OBJ ){` |
|        24 | 2886 | `			ph7_class_instance *pInst = (ph7_class_instance *)apArg[0]->x.pOther;` |
|        24 | 2887 | `			ph7_class *pCountable = pCtx->pVm->pCountableClass;` |
|        24 | 2888 | `			if( pCountable && PH7_VmInstanceOf(pInst->pClass,pCountable) ){` |
|        21 | 2889 | `				ph7_class_method *pMeth = PH7_ClassExtractMethod(pInst->pClass,` |
|         - | 2890 | `					"count",sizeof("count")-1);` |
|        21 | 2891 | `				if( pMeth ){` |
|         - | 2892 | `					ph7_value sResult;` |
|        21 | 2893 | `					PH7_MemObjInit(pCtx->pVm,&sResult);` |
|        21 | 2894 | `					PH7_VmCallClassMethod(pCtx->pVm,pInst,pMeth,&sResult,0,0);` |
|        21 | 2895 | `					ph7_result_int64(pCtx,ph7_value_to_int64(&sResult));` |
|        21 | 2896 | `					PH7_MemObjRelease(&sResult);` |
|        21 | 2897 | `					return PH7_OK;` |
|         - | 2898 | `				}` |
|       ! 0 | 2899 | `			}` |
|         1 | 2900 | `		}` |
|        22 | 2901 | `		return PH7_VmThrowException(pCtx,` |
|         - | 2902 | `			"TypeError",` |
|         - | 2903 | `			"count(): Argument #1 ($value) must be of type Countable\|array, %s given",` |
|         6 | 2904 | `			ph7_type_name(apArg[0])` |
|         - | 2905 | `			);` |
|         - | 2906 | `	}` |
|         - | 2907 | `	/* Count */` |
|       927 | 2908 | `	iCount = HashmapCount((ph7_hashmap *)apArg[0]->x.pOther,bRecursive,&bCycleDetected);` |
|       927 | 2909 | `	if( bCycleDetected ){` |
|         3 | 2910 | `		PH7_VmThrowError(pCtx->pVm,0,PH7_CTX_WARNING,"count(): Recursion detected");` |
|         1 | 2911 | `	}` |
|       927 | 2912 | `	ph7_result_int64(pCtx,iCount);` |
|       927 | 2913 | `	return PH7_OK;` |
|       487 | 2914 | `}` |
|         - | 2915 | `/*` |
|         - | 2916 | ` * bool array_key_exists(value $key,array $search)` |
|         - | 2917 | ` *  Checks if the given key or index exists in the array.` |
|         - | 2918 | ` * Parameters` |
|         - | 2919 | ` * $key` |
|         - | 2920 | ` *   Value to check.` |
|         - | 2921 | ` * $search` |
|         - | 2922 | ` *  An array with keys to check.` |
|         - | 2923 | ` * Return` |
|         - | 2924 | ` *  TRUE on success or FALSE on failure.` |
|         - | 2925 | ` */` |
|        86 | 2926 | `static int ph7_hashmap_key_exists(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|         5 | 2927 | `{` |
|         - | 2928 | `	sxi32 rc;` |
|        91 | 2929 | `	if( nArg != 2 ){` |
|         - | 2930 | `		/* PHP requires exactly two arguments */` |
|        12 | 2931 | `		return PH7_VmThrowException(pCtx,` |
|         - | 2932 | `			"ArgumentCountError",` |
|         - | 2933 | `			"array_key_exists() expects exactly 2 arguments, %d given",` |
|         3 | 2934 | `			nArg` |
|         - | 2935 | `			);` |
|         - | 2936 | `	}` |
|         - | 2937 | `	/* Make sure we are dealing with a valid hashmap */` |
|        85 | 2938 | `	if( !ph7_value_is_array(apArg[1]) ){` |
|         - | 2939 | `		/* Type mismatch -> TypeError */` |
|         8 | 2940 | `		return PH7_VmThrowException(pCtx,` |
|         - | 2941 | `			"TypeError",` |
|         - | 2942 | `			"array_key_exists(): Argument #2 ($array) must be of type array, %s given",` |
|         4 | 2943 | `			ph7_type_name(apArg[1])` |
|         - | 2944 | `			);` |
|         - | 2945 | `	}` |
|         - | 2946 | `	/* Emit deprecation warnings matching PHP behaviour */` |
|        80 | 2947 | `	if( apArg[0]->iFlags & MEMOBJ_NULL ){` |
|         3 | 2948 | `		ph7_context_throw_error_format(pCtx,8192,` |
|         - | 2949 | `			"Using null as the key parameter for array_key_exists() is deprecated, "` |
|         - | 2950 | `			"use an empty string instead"` |
|         - | 2951 | `			);` |
|        79 | 2952 | `	}else if( apArg[0]->iFlags & MEMOBJ_REAL ){` |
|         3 | 2953 | `		ph7_real rVal = apArg[0]->rVal;` |
|         3 | 2954 | `		if( rVal != (ph7_real)(sxi64)rVal ){` |
|         4 | 2955 | `			ph7_context_throw_error_format(pCtx,8192,` |
|         - | 2956 | `				"Implicit conversion from float %g to int loses precision"` |
|         1 | 2957 | `				,rVal` |
|         - | 2958 | `				);` |
|         1 | 2959 | `		}` |
|         1 | 2960 | `	}` |
|         - | 2961 | `	/* Perform the lookup */` |
|        80 | 2962 | `	rc = PH7_HashmapLookup((ph7_hashmap *)apArg[1]->x.pOther,apArg[0],0);` |
|         - | 2963 | `	/* lookup result */` |
|        80 | 2964 | `	ph7_result_bool(pCtx,rc == SXRET_OK ? 1 : 0);` |
|        80 | 2965 | `	return PH7_OK;` |
|        48 | 2966 | `}` |
|         - | 2967 | `/*` |
|         - | 2968 | ` * value array_pop(array $array)` |
|         - | 2969 | ` *   POP the last inserted element from the array.` |
|         - | 2970 | ` * Parameter` |
|         - | 2971 | ` *  The array to get the value from.` |
|         - | 2972 | ` * Return` |
|         - | 2973 | ` *  Poped value or NULL on failure.` |
|         - | 2974 | ` */` |
|        18 | 2975 | `static int ph7_hashmap_pop(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|         5 | 2976 | `{` |
|         - | 2977 | `	ph7_hashmap *pMap;` |
|         - | 2978 | `	/* PHP requires exactly one argument and it must be passed by reference */` |
|        23 | 2979 | `	if( nArg != 1 ){` |
|         8 | 2980 | `		return PH7_VmThrowException(pCtx,` |
|         - | 2981 | `			"ArgumentCountError",` |
|         - | 2982 | `			"array_pop() expects exactly 1 argument, %d given",` |
|         2 | 2983 | `			nArg` |
|         - | 2984 | `			);` |
|         - | 2985 | `	}` |
|         - | 2986 | `	/* Passing a constant (including literals) or non-variable triggers the same` |
|         - | 2987 | `	 * error message as official PHP. Check the index to detect constants. */` |
|        18 | 2988 | `	if( apArg[0]->nIdx == SXU32_HIGH ){` |
|         6 | 2989 | `		return PH7_VmThrowException(pCtx,` |
|         - | 2990 | `			"Error",` |
|         - | 2991 | `			"array_pop(): Argument #1 ($array) could not be passed by reference"` |
|         - | 2992 | `			);` |
|         - | 2993 | `	}` |
|         - | 2994 | `	/* Make sure we are dealing with a valid hashmap */` |
|        12 | 2995 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|         4 | 2996 | `		return PH7_VmThrowException(pCtx,` |
|         - | 2997 | `			"TypeError",` |
|         - | 2998 | `			"array_pop(): Argument #1 ($array) must be of type array, %s given",` |
|         1 | 2999 | `			ph7_type_name(apArg[0])` |
|         - | 3000 | `			);` |
|         - | 3001 | `	}` |
|         9 | 3002 | `	PH7_HashmapCowSeparate(pCtx->pVm, apArg[0]);` |
|         9 | 3003 | `	pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|         9 | 3004 | `	if( pMap->nEntry < 1 ){` |
|         - | 3005 | `		/* Nothing to pop,return NULL */` |
|         3 | 3006 | `		ph7_result_null(pCtx);` |
|         2 | 3007 | `	}else{` |
|         7 | 3008 | `		ph7_hashmap_node *pLast = pMap->pLast;` |
|         - | 3009 | `		ph7_value *pObj;` |
|         7 | 3010 | `		pObj = HashmapExtractNodeValue(pLast);` |
|         7 | 3011 | `		if( pObj ){` |
|         - | 3012 | `			/* Node value */` |
|         7 | 3013 | `			ph7_result_value(pCtx,pObj);` |
|         - | 3014 | `			/* Unlink the node */` |
|         7 | 3015 | `			PH7_HashmapUnlinkNode(pLast,TRUE);` |
|         4 | 3016 | `		}else{` |
|       ! 0 | 3017 | `			ph7_result_null(pCtx);` |
|         - | 3018 | `		}` |
|         - | 3019 | `		/* Reset the cursor */` |
|         7 | 3020 | `		pMap->pCur = pMap->pFirst;` |
|         - | 3021 | `	}` |
|         9 | 3022 | `	return PH7_OK;` |
|        14 | 3023 | `}` |
|         - | 3024 | `/*` |
|         - | 3025 | ` * int array_push($array,$var,...)` |
|         - | 3026 | ` *   Push one or more elements onto the end of array. (Stack insertion)` |
|         - | 3027 | ` * Parameters` |
|         - | 3028 | ` *  array` |
|         - | 3029 | ` *    The input array.` |
|         - | 3030 | ` *  var` |
|         - | 3031 | ` *   On or more value to push.` |
|         - | 3032 | ` * Return` |
|         - | 3033 | ` *  New array count (including old items).` |
|         - | 3034 | ` */` |
|        24 | 3035 | `static int ph7_hashmap_push(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|         5 | 3036 | `{` |
|         - | 3037 | `	ph7_hashmap *pMap;` |
|         - | 3038 | `	sxi32 rc;` |
|         - | 3039 | `	int i;` |
|        29 | 3040 | `	if( nArg < 1 ){` |
|         4 | 3041 | `		return PH7_VmThrowException(pCtx,` |
|         - | 3042 | `			"ArgumentCountError",` |
|         - | 3043 | `			"array_push() expects at least 1 argument, %d given",` |
|         1 | 3044 | `			nArg` |
|         - | 3045 | `			);` |
|         - | 3046 | `	}` |
|         - | 3047 | `	/* Passing a constant (including literals) or non-variable triggers the same` |
|         - | 3048 | `	 * error message as official PHP. Check the index to detect constants. */` |
|        26 | 3049 | `	if( apArg[0]->nIdx == SXU32_HIGH ){` |
|         6 | 3050 | `		return PH7_VmThrowException(pCtx,` |
|         - | 3051 | `			"Error",` |
|         - | 3052 | `			"array_push(): Argument #1 ($array) could not be passed by reference"` |
|         - | 3053 | `			);` |
|         - | 3054 | `	}` |
|         - | 3055 | `	/* Make sure we are dealing with a valid hashmap */` |
|        20 | 3056 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|         4 | 3057 | `		return PH7_VmThrowException(pCtx,` |
|         - | 3058 | `			"TypeError",` |
|         - | 3059 | `			"array_push(): Argument #1 ($array) must be of type array, %s given",` |
|         1 | 3060 | `			ph7_type_name(apArg[0])` |
|         - | 3061 | `			);` |
|         - | 3062 | `	}` |
|         - | 3063 | `	/* Point to the internal representation of the input hashmap */` |
|        18 | 3064 | `	PH7_HashmapCowSeparate(pCtx->pVm, apArg[0]);` |
|        18 | 3065 | `	pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|         - | 3066 | `	/* Start pushing given values */` |
|        34 | 3067 | `	for( i = 1 ; i < nArg ; ++i ){` |
|        20 | 3068 | `		rc = PH7_HashmapInsert(pMap,0,apArg[i]);` |
|        20 | 3069 | `		if( rc != SXRET_OK ){` |
|         3 | 3070 | `			if( rc == PH7_EXCEPTION \|\| rc == PH7_ABORT ){` |
|         - | 3071 | `				/* Saturated-append Error (php: array_push throws, no result) */` |
|         3 | 3072 | `				return rc;` |
|         - | 3073 | `			}` |
|       ! 0 | 3074 | `			break;` |
|         - | 3075 | `		}` |
|         9 | 3076 | `	}` |
|         - | 3077 | `	/* Return the new count */` |
|        15 | 3078 | `	ph7_result_int64(pCtx,(sxi64)pMap->nEntry);` |
|        15 | 3079 | `	return PH7_OK;` |
|        17 | 3080 | `}` |
|         - | 3081 | `/*` |
|         - | 3082 | ` * value array_shift(array $array)` |
|         - | 3083 | ` *   Shift an element off the beginning of array.` |
|         - | 3084 | ` * Parameter` |
|         - | 3085 | ` *  The array to get the value from.` |
|         - | 3086 | ` * Return` |
|         - | 3087 | ` *  Shifted value or NULL on failure.` |
|         - | 3088 | ` */` |
|        38 | 3089 | `static int ph7_hashmap_shift(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|         5 | 3090 | `{` |
|         - | 3091 | `	ph7_hashmap *pMap;` |
|         - | 3092 | `	/* PHP requires exactly one argument and it must be passed by reference */` |
|        43 | 3093 | `	if( nArg != 1 ){` |
|         8 | 3094 | `		return PH7_VmThrowException(pCtx,` |
|         - | 3095 | `			"ArgumentCountError",` |
|         - | 3096 | `			"array_shift() expects exactly 1 argument, %d given",` |
|         2 | 3097 | `			nArg` |
|         - | 3098 | `			);` |
|         - | 3099 | `	}` |
|         - | 3100 | `	/* Detect constants or literals, which cannot be passed by reference. */` |
|        39 | 3101 | `	if( apArg[0]->nIdx == SXU32_HIGH ){` |
|         6 | 3102 | `		return PH7_VmThrowException(pCtx,` |
|         - | 3103 | `			"Error",` |
|         - | 3104 | `			"array_shift(): Argument #1 ($array) could not be passed by reference"` |
|         - | 3105 | `			);` |
|         - | 3106 | `	}` |
|         - | 3107 | `	/* Make sure we are dealing with a valid hashmap */` |
|        35 | 3108 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|         4 | 3109 | `		return PH7_VmThrowException(pCtx,` |
|         - | 3110 | `			"TypeError",` |
|         - | 3111 | `			"array_shift(): Argument #1 ($array) must be of type array, %s given",` |
|         1 | 3112 | `			ph7_type_name(apArg[0])` |
|         - | 3113 | `			);` |
|         - | 3114 | `	}` |
|         - | 3115 | `	/* Point to the internal representation of the hashmap */` |
|        33 | 3116 | `	PH7_HashmapCowSeparate(pCtx->pVm, apArg[0]);` |
|        33 | 3117 | `	pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|        33 | 3118 | `	if( pMap->nEntry < 1 ){` |
|         - | 3119 | `		/* Empty hashmap,return NULL */` |
|         3 | 3120 | `		ph7_result_null(pCtx);` |
|         2 | 3121 | `	}else{` |
|        31 | 3122 | `		ph7_hashmap_node *pEntry = pMap->pFirst;` |
|         - | 3123 | `		ph7_value *pObj;` |
|         - | 3124 | `		sxu32 n;` |
|        31 | 3125 | `		pObj = HashmapExtractNodeValue(pEntry);` |
|        31 | 3126 | `		if( pObj ){` |
|         - | 3127 | `			/* Node value */` |
|        31 | 3128 | `			ph7_result_value(pCtx,pObj);` |
|         - | 3129 | `			/* Unlink the first node */` |
|        31 | 3130 | `			PH7_HashmapUnlinkNode(pEntry,TRUE);` |
|        18 | 3131 | `		}else{` |
|       ! 0 | 3132 | `			ph7_result_null(pCtx);` |
|         - | 3133 | `		}` |
|         - | 3134 | `		/* Rehash all int keys */` |
|        31 | 3135 | `		n = pMap->nEntry;` |
|        31 | 3136 | `		pEntry = pMap->pFirst;` |
|        31 | 3137 | `		pMap->iNextIdx = 0; /* Reset the automatic index */` |
|        40 | 3138 | `		for(;;){` |
|        85 | 3139 | `			if( n < 1 ){` |
|        31 | 3140 | `				break;` |
|         - | 3141 | `			}` |
|        59 | 3142 | `			if( pEntry->iType == HASHMAP_INT_NODE ){` |
|        59 | 3143 | `				HashmapRehashIntNode(pEntry);` |
|        27 | 3144 | `			}` |
|         - | 3145 | `			/* Point to the next entry */` |
|        59 | 3146 | `			pEntry = pEntry->pPrev; /* Reverse link */` |
|        59 | 3147 | `			n--;` |
|         5 | 3148 | `		}` |
|         - | 3149 | `		/* Reset the cursor */` |
|        31 | 3150 | `		pMap->pCur = pMap->pFirst;` |
|         - | 3151 | `	}` |
|        33 | 3152 | `	return PH7_OK;` |
|        24 | 3153 | `}` |
|         - | 3154 | `/*` |
|         - | 3155 | ` * Extract the node cursor value.` |
|         - | 3156 | ` */` |
|        28 | 3157 | `static sxi32 HashmapCurrentValue(ph7_context *pCtx,ph7_hashmap *pMap,int iDirection)` |
|         1 | 3158 | `{` |
|        29 | 3159 | `	ph7_hashmap_node *pCur = pMap->pCur;` |
|         - | 3160 | `	ph7_value *pVal;` |
|        29 | 3161 | `	if( pCur == 0 ){` |
|         - | 3162 | `		/* Cursor does not point to anything,return FALSE */` |
|       ! 0 | 3163 | `		ph7_result_bool(pCtx,0);` |
|       ! 0 | 3164 | `		return PH7_OK;` |
|         - | 3165 | `	}` |
|        29 | 3166 | `	if( iDirection != 0 ){` |
|        11 | 3167 | `		if( iDirection > 0 ){` |
|         - | 3168 | `			/* Point to the next entry */` |
|         9 | 3169 | `			pMap->pCur = pCur->pPrev; /* Reverse link */` |
|         9 | 3170 | `			pCur = pMap->pCur;` |
|         5 | 3171 | `		}else{` |
|         - | 3172 | `			/* Point to the previous entry */` |
|         3 | 3173 | `			pMap->pCur = pCur->pNext; /* Reverse link */` |
|         3 | 3174 | `			pCur = pMap->pCur;` |
|         - | 3175 | `		}` |
|        11 | 3176 | `		if( pCur == 0 ){` |
|         - | 3177 | `			/* End of input reached,return FALSE */` |
|       ! 0 | 3178 | `			ph7_result_bool(pCtx,0);` |
|       ! 0 | 3179 | `			return PH7_OK;` |
|         - | 3180 | `		}` |
|         5 | 3181 | `	}` |
|         - | 3182 | `	/* Point to the desired element */` |
|        29 | 3183 | `	pVal = HashmapExtractNodeValue(pCur);` |
|        29 | 3184 | `	if( pVal ){` |
|        29 | 3185 | `		ph7_result_value(pCtx,pVal);` |
|        15 | 3186 | `	}else{` |
|       ! 0 | 3187 | `		ph7_result_bool(pCtx,0);` |
|         - | 3188 | `	}` |
|        29 | 3189 | `	return PH7_OK;` |
|        15 | 3190 | `}` |
|         - | 3191 | `/*` |
|         - | 3192 | ` * value current(array $array)` |
|         - | 3193 | ` *  Return the current element in an array.` |
|         - | 3194 | ` * Parameter` |
|         - | 3195 | ` *  $input: The input array.` |
|         - | 3196 | ` * Return` |
|         - | 3197 | ` *  The current() function simply returns the value of the array element that's currently` |
|         - | 3198 | ` *  being pointed to by the internal pointer. It does not move the pointer in any way.` |
|         - | 3199 | ` *  If the internal pointer points beyond the end of the elements list or the array` |
|         - | 3200 | ` *  is empty, current() returns FALSE.` |
|         - | 3201 | ` */` |
|        12 | 3202 | `static int ph7_hashmap_current(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|         1 | 3203 | `{` |
|        13 | 3204 | `	if( nArg < 1 ){` |
|         - | 3205 | `		/* Missing arguments,return FALSE */` |
|       ! 0 | 3206 | `		ph7_result_bool(pCtx,0);` |
|       ! 0 | 3207 | `		return PH7_OK;` |
|         - | 3208 | `	}` |
|         - | 3209 | `	/* Make sure we are dealing with a valid hashmap */` |
|        13 | 3210 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|         - | 3211 | `		/* Invalid argument,return FALSE */` |
|       ! 0 | 3212 | `		ph7_result_bool(pCtx,0);` |
|       ! 0 | 3213 | `		return PH7_OK;` |
|         - | 3214 | `	}` |
|        13 | 3215 | `	HashmapCurrentValue(&(*pCtx),(ph7_hashmap *)apArg[0]->x.pOther,0);` |
|        13 | 3216 | `	return PH7_OK;` |
|         7 | 3217 | `}` |
|         - | 3218 | `/*` |
|         - | 3219 | ` * value next(array $input)` |
|         - | 3220 | ` *  Advance the internal array pointer of an array.` |
|         - | 3221 | ` * Parameter` |
|         - | 3222 | ` *  $input: The input array.` |
|         - | 3223 | ` * Return` |
|         - | 3224 | ` *  next() behaves like current(), with one difference. It advances the internal array` |
|         - | 3225 | ` *  pointer one place forward before returning the element value. That means it returns` |
|         - | 3226 | ` *  the next array value and advances the internal array pointer by one.` |
|         - | 3227 | ` */` |
|         8 | 3228 | `static int ph7_hashmap_next(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|         1 | 3229 | `{` |
|         9 | 3230 | `	if( nArg < 1 ){` |
|         - | 3231 | `		/* Missing arguments,return FALSE */` |
|       ! 0 | 3232 | `		ph7_result_bool(pCtx,0);` |
|       ! 0 | 3233 | `		return PH7_OK;` |
|         - | 3234 | `	}` |
|         - | 3235 | `	/* Make sure we are dealing with a valid hashmap */` |
|         9 | 3236 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|         - | 3237 | `		/* Invalid argument,return FALSE */` |
|       ! 0 | 3238 | `		ph7_result_bool(pCtx,0);` |
|       ! 0 | 3239 | `		return PH7_OK;` |
|         - | 3240 | `	}` |
|         9 | 3241 | `	HashmapCurrentValue(&(*pCtx),(ph7_hashmap *)apArg[0]->x.pOther,1);` |
|         9 | 3242 | `	return PH7_OK;` |
|         5 | 3243 | `}` |
|         - | 3244 | `/*` |
|         - | 3245 | ` * value prev(array $input)` |
|         - | 3246 | ` *  Rewind the internal array pointer.` |
|         - | 3247 | ` * Parameter` |
|         - | 3248 | ` *  $input: The input array.` |
|         - | 3249 | ` * Return` |
|         - | 3250 | ` *  Returns the array value in the previous place that's pointed` |
|         - | 3251 | ` *  to by the internal array pointer, or FALSE if there are no more` |
|         - | 3252 | ` *  elements.` |
|         - | 3253 | ` */` |
|         2 | 3254 | `static int ph7_hashmap_prev(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|         1 | 3255 | `{` |
|         3 | 3256 | `	if( nArg < 1 ){` |
|         - | 3257 | `		/* Missing arguments,return FALSE */` |
|       ! 0 | 3258 | `		ph7_result_bool(pCtx,0);` |
|       ! 0 | 3259 | `		return PH7_OK;` |
|         - | 3260 | `	}` |
|         - | 3261 | `	/* Make sure we are dealing with a valid hashmap */` |
|         3 | 3262 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|         - | 3263 | `		/* Invalid argument,return FALSE */` |
|       ! 0 | 3264 | `		ph7_result_bool(pCtx,0);` |
|       ! 0 | 3265 | `		return PH7_OK;` |
|         - | 3266 | `	}` |
|         3 | 3267 | `	HashmapCurrentValue(&(*pCtx),(ph7_hashmap *)apArg[0]->x.pOther,-1);` |
|         3 | 3268 | `	return PH7_OK;` |
|         2 | 3269 | `}` |
|         - | 3270 | `/*` |
|         - | 3271 | ` * value end(array $input)` |
|         - | 3272 | ` *  Set the internal pointer of an array to its last element.` |
|         - | 3273 | ` * Parameter` |
|         - | 3274 | ` *  $input: The input array.` |
|         - | 3275 | ` * Return` |
|         - | 3276 | ` *  Returns the value of the last element or FALSE for empty array.` |
|         - | 3277 | ` */` |
|         2 | 3278 | `static int ph7_hashmap_end(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|         1 | 3279 | `{` |
|         - | 3280 | `	ph7_hashmap *pMap;` |
|         3 | 3281 | `	if( nArg < 1 ){` |
|         - | 3282 | `		/* Missing arguments,return FALSE */` |
|       ! 0 | 3283 | `		ph7_result_bool(pCtx,0);` |
|       ! 0 | 3284 | `		return PH7_OK;` |
|         - | 3285 | `	}` |
|         - | 3286 | `	/* Make sure we are dealing with a valid hashmap */` |
|         3 | 3287 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|         - | 3288 | `		/* Invalid argument,return FALSE */` |
|       ! 0 | 3289 | `		ph7_result_bool(pCtx,0);` |
|       ! 0 | 3290 | `		return PH7_OK;` |
|         - | 3291 | `	}` |
|         - | 3292 | `	/* Point to the internal representation of the input hashmap */` |
|         3 | 3293 | `	pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|         - | 3294 | `	/* Point to the last node */` |
|         3 | 3295 | `	pMap->pCur = pMap->pLast;` |
|         - | 3296 | `	/* Return the last node value */` |
|         3 | 3297 | `	HashmapCurrentValue(&(*pCtx),pMap,0);` |
|         3 | 3298 | `	return PH7_OK;` |
|         2 | 3299 | `}` |
|         - | 3300 | `/*` |
|         - | 3301 | ` * value reset(array $array )` |
|         - | 3302 | ` *  Set the internal pointer of an array to its first element.` |
|         - | 3303 | ` * Parameter` |
|         - | 3304 | ` *  $input: The input array.` |
|         - | 3305 | ` * Return` |
|         - | 3306 | ` *  Returns the value of the first array element,or FALSE if the array is empty.` |
|         - | 3307 | ` */` |
|         4 | 3308 | `static int ph7_hashmap_reset(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|         1 | 3309 | `{` |
|         - | 3310 | `	ph7_hashmap *pMap;` |
|         5 | 3311 | `	if( nArg < 1 ){` |
|         - | 3312 | `		/* Missing arguments,return FALSE */` |
|       ! 0 | 3313 | `		ph7_result_bool(pCtx,0);` |
|       ! 0 | 3314 | `		return PH7_OK;` |
|         - | 3315 | `	}` |
|         - | 3316 | `	/* Make sure we are dealing with a valid hashmap */` |
|         5 | 3317 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|         - | 3318 | `		/* Invalid argument,return FALSE */` |
|       ! 0 | 3319 | `		ph7_result_bool(pCtx,0);` |
|       ! 0 | 3320 | `		return PH7_OK;` |
|         - | 3321 | `	}` |
|         - | 3322 | `	/* Point to the internal representation of the input hashmap */` |
|         5 | 3323 | `	pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|         - | 3324 | `	/* Point to the first node */` |
|         5 | 3325 | `	pMap->pCur = pMap->pFirst;` |
|         - | 3326 | `	/* Return the last node value if available */` |
|         5 | 3327 | `	HashmapCurrentValue(&(*pCtx),pMap,0);` |
|         5 | 3328 | `	return PH7_OK;` |
|         3 | 3329 | `}` |
|         - | 3330 | `/*` |
|         - | 3331 | ` * value key(array $array)` |
|         - | 3332 | ` *   Fetch a key from an array` |
|         - | 3333 | ` * Parameter` |
|         - | 3334 | ` *  $input` |
|         - | 3335 | ` *   The input array.` |
|         - | 3336 | ` * Return` |
|         - | 3337 | ` *  The key() function simply returns the key of the array element that's currently` |
|         - | 3338 | ` *  being pointed to by the internal pointer. It does not move the pointer in any way.` |
|         - | 3339 | ` *  If the internal pointer points beyond the end of the elements list or the array` |
|         - | 3340 | ` *  is empty, key() returns NULL.` |
|         - | 3341 | ` */` |
|         4 | 3342 | `static int ph7_hashmap_simple_key(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|         1 | 3343 | `{` |
|         - | 3344 | `	ph7_hashmap_node *pCur;` |
|         - | 3345 | `	ph7_hashmap *pMap;` |
|         5 | 3346 | `	if( nArg < 1 ){` |
|         - | 3347 | `		/* Missing arguments,return NULL */` |
|       ! 0 | 3348 | `		ph7_result_null(pCtx);` |
|       ! 0 | 3349 | `		return PH7_OK;` |
|         - | 3350 | `	}` |
|         - | 3351 | `	/* Make sure we are dealing with a valid hashmap */` |
|         5 | 3352 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|         - | 3353 | `		/* Invalid argument,return NULL */` |
|       ! 0 | 3354 | `		ph7_result_null(pCtx);` |
|       ! 0 | 3355 | `		return PH7_OK;` |
|         - | 3356 | `	}` |
|         5 | 3357 | `	pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|         5 | 3358 | `	pCur = pMap->pCur;` |
|         5 | 3359 | `	if( pCur == 0 ){` |
|         - | 3360 | `		/* Cursor does not point to anything,return NULL */` |
|       ! 0 | 3361 | `		ph7_result_null(pCtx);` |
|       ! 0 | 3362 | `		return PH7_OK;` |
|         - | 3363 | `	}` |
|         5 | 3364 | `	if( pCur->iType == HASHMAP_INT_NODE){` |
|         - | 3365 | `		/* Key is integer */` |
|       ! 0 | 3366 | `		ph7_result_int64(pCtx,pCur->xKey.iKey);` |
|       ! 0 | 3367 | `	}else{` |
|         - | 3368 | `		/* Key is blob */` |
|         7 | 3369 | `		ph7_result_string(pCtx,` |
|         4 | 3370 | `			(const char *)SyBlobData(&pCur->xKey.sKey),(int)SyBlobLength(&pCur->xKey.sKey));` |
|         - | 3371 | `	}` |
|         5 | 3372 | `	return PH7_OK;` |
|         3 | 3373 | `}` |
|         - | 3374 | `/*` |
|         - | 3375 | ` * array each(array $input)` |
|         - | 3376 | ` *  Return the current key and value pair from an array and advance the array cursor.` |
|         - | 3377 | ` * Parameter` |
|         - | 3378 | ` *  $input` |
|         - | 3379 | ` *    The input array.` |
|         - | 3380 | ` * Return` |
|         - | 3381 | ` *  Returns the current key and value pair from the array array. This pair is returned` |
|         - | 3382 | ` *  in a four-element array, with the keys 0, 1, key, and value. Elements 0 and key` |
|         - | 3383 | ` *  contain the key name of the array element, and 1 and value contain the data.` |
|         - | 3384 | ` *  If the internal pointer for the array points past the end of the array contents` |
|         - | 3385 | ` *  each() returns FALSE.` |
|         - | 3386 | ` */` |
|        22 | 3387 | `static int ph7_hashmap_each(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|         1 | 3388 | `{` |
|         - | 3389 | `	ph7_hashmap_node *pCur;` |
|         - | 3390 | `	ph7_hashmap *pMap;` |
|         - | 3391 | `	ph7_value *pArray;` |
|         - | 3392 | `	ph7_value *pVal;` |
|         - | 3393 | `	ph7_value sKey;` |
|        23 | 3394 | `	if( nArg < 1 ){` |
|         - | 3395 | `		/* Missing arguments,return FALSE */` |
|       ! 0 | 3396 | `		ph7_result_bool(pCtx,0);` |
|       ! 0 | 3397 | `		return PH7_OK;` |
|         - | 3398 | `	}` |
|         - | 3399 | `	/* Make sure we are dealing with a valid hashmap */` |
|        23 | 3400 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|         - | 3401 | `		/* Invalid argument,return FALSE */` |
|       ! 0 | 3402 | `		ph7_result_bool(pCtx,0);` |
|       ! 0 | 3403 | `		return PH7_OK;` |
|         - | 3404 | `	}` |
|         - | 3405 | `	/* Point to the internal representation that describe the input hashmap */` |
|        23 | 3406 | `	pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|        23 | 3407 | `	if( pMap->pCur == 0 ){` |
|         - | 3408 | `		/* Cursor does not point to anything,return FALSE */` |
|         9 | 3409 | `		ph7_result_bool(pCtx,0);` |
|         9 | 3410 | `		return PH7_OK;` |
|         - | 3411 | `	}` |
|        15 | 3412 | `	pCur = pMap->pCur;` |
|         - | 3413 | `	/* Create a new array */` |
|        15 | 3414 | `	pArray = ph7_context_new_array(pCtx);` |
|        15 | 3415 | `	if( pArray == 0 ){` |
|       ! 0 | 3416 | `		ph7_result_bool(pCtx,0);` |
|       ! 0 | 3417 | `		return PH7_OK;` |
|         - | 3418 | `	}` |
|        15 | 3419 | `	pVal = HashmapExtractNodeValue(pCur);` |
|         - | 3420 | `	/* Insert the current value */` |
|        15 | 3421 | `	ph7_array_add_intkey_elem(pArray,1,pVal);` |
|        15 | 3422 | `	ph7_array_add_strkey_elem(pArray,"value",pVal);` |
|         - | 3423 | `	/* Make the key */` |
|        15 | 3424 | `	if( pCur->iType == HASHMAP_INT_NODE ){` |
|         7 | 3425 | `		PH7_MemObjInitFromInt(pMap->pVm,&sKey,pCur->xKey.iKey);` |
|         4 | 3426 | `	}else{` |
|         9 | 3427 | `		PH7_MemObjInitFromString(pMap->pVm,&sKey,0);` |
|         9 | 3428 | `		PH7_MemObjStringAppend(&sKey,(const char *)SyBlobData(&pCur->xKey.sKey),SyBlobLength(&pCur->xKey.sKey));` |
|         - | 3429 | `	}` |
|         - | 3430 | `	/* Insert the current key */` |
|        15 | 3431 | `	ph7_array_add_intkey_elem(pArray,0,&sKey);` |
|        15 | 3432 | `	ph7_array_add_strkey_elem(pArray,"key",&sKey);` |
|        15 | 3433 | `	PH7_MemObjRelease(&sKey);` |
|         - | 3434 | `	/* Advance the cursor */` |
|        15 | 3435 | `	pMap->pCur = pCur->pPrev; /* Reverse link */` |
|         - | 3436 | `	/* Return the current entry */` |
|        15 | 3437 | `	ph7_result_value(pCtx,pArray);` |
|        15 | 3438 | `	return PH7_OK;` |
|        12 | 3439 | `}` |
|         - | 3440 | `/*` |
|         - | 3441 | ` * range() — a faithful port of php 8.5's ext/standard/array.c implementation` |
|         - | 3442 | ` * (php_range_process_input + PHP_FUNCTION(range)), so the value semantics,` |
|         - | 3443 | ` * diagnostics, and their ordering are byte-exact: decreasing ranges, float` |
|         - | 3444 | ` * ranges, character ranges, the step/endpoint ValueErrors, the ZPP TypeErrors` |
|         - | 3445 | ` * and null deprecations, and the string-endpoint warnings.` |
|         - | 3446 | ` */` |
|         - | 3447 | `#define PH7_RANGE_HT_MAX_SIZE 1073741824 /* php's HT_MAX_SIZE (2^30 entries) */` |
|         - | 3448 | `/*` |
|         - | 3449 | ` * Endpoint classification, mirroring php_range_process_input's return` |
|         - | 3450 | ` * contract. php returns zval type tags whose ORDER encodes the logic` |
|         - | 3451 | ` * (IS_LONG < IS_DOUBLE < IS_STRING < IS_ARRAY); the >=/< comparisons in` |
|         - | 3452 | ` * ph7_hashmap_range depend on the same ordering here.` |
|         - | 3453 | ` *   RANGE_IN_LONG/DOUBLE : only interpretable as int / float` |
|         - | 3454 | ` *   RANGE_IN_STRING      : only interpretable as a (char-range) string` |
|         - | 3455 | ` *   RANGE_IN_DIGIT       : single-byte numeric string — valid as both a char` |
|         - | 3456 | ` *                          and a number (php returns IS_ARRAY for this)` |
|         - | 3457 | ` * The RANGE_IN_* codes and RangeStrToNumber are declared in ph7int.h so the` |
|         - | 3458 | ` * stage-2 ZPP domain-error sweep can reuse the classifier (PLAN §3.9(a)).` |
|         - | 3459 | ` */` |
|         - | 3460 | `/* IEEE special-value tests: the engine-wide bit-pattern macros from` |
|         - | 3461 | ` * sxtypes.h (via ph7int.h) — same ones the printf/serialize paths use. */` |
|         - | 3462 | `/*` |
|         - | 3463 | ` * The type name php's ZPP prints after "must be of type ..., X given":` |
|         - | 3464 | ` * the concrete class name for objects, the usual type name otherwise.` |
|         - | 3465 | ` */` |
|         8 | 3466 | `static const char * RangeArgTypeName(ph7_value *pVal,char *zBuf,sxu32 nBufLen)` |
|         1 | 3467 | `{` |
|         9 | 3468 | `	if( pVal->iFlags & MEMOBJ_OBJ ){` |
|         3 | 3469 | `		ph7_class_instance *pThis = (ph7_class_instance *)pVal->x.pOther;` |
|         3 | 3470 | `		sxu32 n = SXMIN(pThis->pClass->sName.nByte,nBufLen - 1);` |
|         3 | 3471 | `		SyMemcpy((const void *)pThis->pClass->sName.zString,zBuf,n);` |
|         3 | 3472 | `		zBuf[n] = 0;` |
|         3 | 3473 | `		return zBuf;` |
|         - | 3474 | `	}` |
|         7 | 3475 | `	return ph7_type_name(pVal);` |
|         5 | 3476 | `}` |
|         - | 3477 | `/*` |
|         - | 3478 | ` * Classify a string with php's is_numeric_string() grammar:` |
|         - | 3479 | ` *   [ws] [sign] ( D+ [ . D* ] \| . D+ ) [ (e\|E) [sign] D+ ] [ws]` |
|         - | 3480 | ` * — the whole string must be consumed; hex/binary/"INF"/"NAN" are NOT` |
|         - | 3481 | ` * numeric. Returns RANGE_IN_LONG with *pLong set, RANGE_IN_DOUBLE with` |
|         - | 3482 | ` * *pDouble set (a fractional/exponent form, or an integer too wide for an` |
|         - | 3483 | ` * sxi64 — php reclassifies those as float), or RANGE_IN_ERROR when the` |
|         - | 3484 | ` * string is not numeric. The float value comes from libc strtod, like` |
|         - | 3485 | ` * php's zend_strtod (byte-exact-floats rule). zIn must be NUL-terminated` |
|         - | 3486 | ` * at zIn[nLen] — ph7_value_to_string guarantees this (SyBlobNullAppend) —` |
|         - | 3487 | ` * so strtod can parse it in place once the grammar has validated it.` |
|         - | 3488 | ` */` |
|       156 | 3489 | `PH7_PRIVATE sxu8 RangeStrToNumber(const char *zIn,sxu32 nLen,sxi64 *pLong,double *pDouble)` |
|         1 | 3490 | `{` |
|       157 | 3491 | `	const char *z = zIn,*zEnd = &zIn[nLen];` |
|       157 | 3492 | `	sxu64 uVal = 0;` |
|       157 | 3493 | `	int bNeg = 0,bDigit = 0,bReal = 0,bOverflow = 0;` |
|       167 | 3494 | `	while( z < zEnd && (unsigned char)z[0] < 0xc0 && SyisSpace(z[0]) ){ z++; }` |
|       157 | 3495 | `	if( z < zEnd && (z[0] == '+' \|\| z[0] == '-') ){` |
|         3 | 3496 | `		bNeg = (z[0] == '-');` |
|         3 | 3497 | `		z++;` |
|         1 | 3498 | `	}` |
|       237 | 3499 | `	while( z < zEnd && (unsigned char)z[0] < 0xc0 && SyisDigit(z[0]) ){` |
|        81 | 3500 | `		int d = z[0] - '0';` |
|         - | 3501 | `		/* Track overflow past 2^63, the widest magnitude an sxi64 can carry` |
|         - | 3502 | `		 * (as LONG_MIN); overflowing integers become floats like in php. */` |
|        81 | 3503 | `		if( uVal > 922337203685477580ULL \|\| (uVal == 922337203685477580ULL && d > 8) ){` |
|       ! 0 | 3504 | `			bOverflow = 1;` |
|       ! 0 | 3505 | `		}else{` |
|        81 | 3506 | `			uVal = uVal * 10 + (sxu64)d;` |
|         - | 3507 | `		}` |
|        81 | 3508 | `		bDigit = 1;` |
|        81 | 3509 | `		z++;` |
|         1 | 3510 | `	}` |
|       157 | 3511 | `	if( z < zEnd && z[0] == '.' ){` |
|         3 | 3512 | `		bReal = 1;` |
|         3 | 3513 | `		z++;` |
|         5 | 3514 | `		while( z < zEnd && (unsigned char)z[0] < 0xc0 && SyisDigit(z[0]) ){` |
|         3 | 3515 | `			bDigit = 1;` |
|         3 | 3516 | `			z++;` |
|         1 | 3517 | `		}` |
|         1 | 3518 | `	}` |
|         - | 3519 | `	/* At least one mantissa digit required (rejects "", ".", "+", "e5"). */` |
|       157 | 3520 | `	if( !bDigit ){` |
|        61 | 3521 | `		return RANGE_IN_ERROR;` |
|         - | 3522 | `	}` |
|         - | 3523 | `	/* Optional exponent — needs at least one digit (rejects "1e", "1e+"). */` |
|        97 | 3524 | `	if( z < zEnd && (z[0] == 'e' \|\| z[0] == 'E') ){` |
|         9 | 3525 | `		z++;` |
|         9 | 3526 | `		if( z < zEnd && (z[0] == '+' \|\| z[0] == '-') ){ z++; }` |
|         9 | 3527 | `		if( z >= zEnd \|\| (unsigned char)z[0] >= 0xc0 \|\| !SyisDigit(z[0]) ){` |
|       ! 0 | 3528 | `			return RANGE_IN_ERROR;` |
|         - | 3529 | `		}` |
|         9 | 3530 | `		bReal = 1;` |
|        17 | 3531 | `		while( z < zEnd && (unsigned char)z[0] < 0xc0 && SyisDigit(z[0]) ){ z++; }` |
|         4 | 3532 | `	}` |
|         - | 3533 | `	/* Trailing whitespace allowed; anything else means not numeric. */` |
|       101 | 3534 | `	while( z < zEnd && (unsigned char)z[0] < 0xc0 && SyisSpace(z[0]) ){ z++; }` |
|        97 | 3535 | `	if( z != zEnd ){` |
|        13 | 3536 | `		return RANGE_IN_ERROR;` |
|         - | 3537 | `	}` |
|        84 | 3538 | `	if( bOverflow \|\| (!bNeg && uVal > (sxu64)LARGEST_INT64)` |
|        43 | 3539 | `	 \|\| (bNeg && uVal > (sxu64)LARGEST_INT64 + 1) ){` |
|        84 | 3540 | `		bReal = 1;` |
|        84 | 3541 | `	}` |
|        43 | 3542 | `	if( bReal ){` |
|        11 | 3543 | `		*pDouble = strtod(zIn,0);` |
|        11 | 3544 | `		return RANGE_IN_DOUBLE;` |
|         - | 3545 | `	}` |
|         - | 3546 | `	/* Negate in unsigned space so 2^63 lands on LONG_MIN without overflow. */` |
|        33 | 3547 | `	*pLong = bNeg ? (sxi64)((sxu64)0 - uVal) : (sxi64)uVal;` |
|        33 | 3548 | `	return RANGE_IN_LONG;` |
|        58 | 3549 | `}` |
|         - | 3550 | `/*` |
|         - | 3551 | ` * ZPP emulation for $start/$end (php's Z_PARAM_NUMBER_OR_STR, weak mode):` |
|         - | 3552 | ` * reject array/object/resource with php's TypeError, deprecate null (the` |
|         - | 3553 | ` * value then reads as int 0 — *pbNullCoerced). php runs this for all` |
|         - | 3554 | ` * arguments BEFORE any value/domain check, hence the split from` |
|         - | 3555 | ` * RangeProcessInput below. Returns FALSE after throwing (*pRc set).` |
|         - | 3556 | ` */` |
|       262 | 3557 | `static int RangeEndpointZpp(ph7_context *pCtx,ph7_value *pIn,int iArg,const char *zName,int *pbNullCoerced,sxi32 *pRc)` |
|         1 | 3558 | `{` |
|         - | 3559 | `	char zMsg[160];` |
|       263 | 3560 | `	*pRc = PH7_OK;` |
|       263 | 3561 | `	if( pIn->iFlags & (MEMOBJ_HASHMAP\|MEMOBJ_OBJ\|MEMOBJ_RES) ){` |
|         - | 3562 | `		char zType[80];` |
|        10 | 3563 | `		*pRc = PH7_VmThrowException(pCtx,"TypeError",` |
|         - | 3564 | `			"range(): Argument #%d ($%s) must be of type string\|int\|float, %s given",` |
|         3 | 3565 | `			iArg,zName,RangeArgTypeName(pIn,zType,sizeof(zType)));` |
|         7 | 3566 | `		return FALSE;` |
|         - | 3567 | `	}` |
|       257 | 3568 | `	if( pIn->iFlags & MEMOBJ_NULL ){` |
|         7 | 3569 | `		SyBufferFormat(zMsg,sizeof(zMsg),` |
|         - | 3570 | `			"range(): Passing null to parameter #%d ($%s) of type string\|int\|float is deprecated",` |
|         2 | 3571 | `			iArg,zName);` |
|         5 | 3572 | `		PH7_VmThrowError(pCtx->pVm,0,E_DEPRECATED,zMsg);` |
|         5 | 3573 | `		*pbNullCoerced = TRUE;` |
|         2 | 3574 | `	}` |
|       257 | 3575 | `	return TRUE;` |
|       132 | 3576 | `}` |
|         - | 3577 | `/*` |
|         - | 3578 | ` * ZPP emulation for $step (php's Z_PARAM_NUMBER, weak mode): int/float pass` |
|         - | 3579 | ` * through, bool coerces to int, null deprecates to int 0 (which then trips` |
|         - | 3580 | ` * the "cannot be 0" ValueError like php), a numeric string coerces to its` |
|         - | 3581 | ` * number, anything else is a TypeError. Returns RANGE_IN_LONG/DOUBLE, or` |
|         - | 3582 | ` * RANGE_IN_ERROR after throwing (*pRc set).` |
|         - | 3583 | ` */` |
|        62 | 3584 | `static sxu8 RangeStepInput(ph7_context *pCtx,ph7_value *pIn,sxi64 *pLong,double *pDouble,sxi32 *pRc)` |
|         1 | 3585 | `{` |
|        63 | 3586 | `	*pRc = PH7_OK;` |
|        63 | 3587 | `	if( pIn->iFlags & (MEMOBJ_HASHMAP\|MEMOBJ_OBJ\|MEMOBJ_RES) ){` |
|         - | 3588 | `		char zType[80];` |
|         4 | 3589 | `		*pRc = PH7_VmThrowException(pCtx,"TypeError",` |
|         - | 3590 | `			"range(): Argument #3 ($step) must be of type int\|float, %s given",` |
|         1 | 3591 | `			RangeArgTypeName(pIn,zType,sizeof(zType)));` |
|         3 | 3592 | `		return RANGE_IN_ERROR;` |
|         - | 3593 | `	}` |
|        61 | 3594 | `	if( pIn->iFlags & MEMOBJ_NULL ){` |
|         3 | 3595 | `		PH7_VmThrowError(pCtx->pVm,0,E_DEPRECATED,` |
|         - | 3596 | `			"range(): Passing null to parameter #3 ($step) of type int\|float is deprecated");` |
|         3 | 3597 | `		*pLong = 0;` |
|         3 | 3598 | `		return RANGE_IN_LONG;` |
|         - | 3599 | `	}` |
|        59 | 3600 | `	if( pIn->iFlags & MEMOBJ_REAL ){` |
|        25 | 3601 | `		*pDouble = ph7_value_to_double(pIn);` |
|        25 | 3602 | `		return RANGE_IN_DOUBLE;` |
|         - | 3603 | `	}` |
|        35 | 3604 | `	if( pIn->iFlags & MEMOBJ_STRING ){` |
|         - | 3605 | `		const char *zStr;` |
|         - | 3606 | `		int nLen;` |
|         - | 3607 | `		sxu8 iKind;` |
|         3 | 3608 | `		zStr = ph7_value_to_string(pIn,&nLen);` |
|         3 | 3609 | `		iKind = RangeStrToNumber(zStr,(sxu32)nLen,pLong,pDouble);` |
|         3 | 3610 | `		if( iKind == RANGE_IN_ERROR ){` |
|         3 | 3611 | `			*pRc = PH7_VmThrowException(pCtx,"TypeError",` |
|         - | 3612 | `				"range(): Argument #3 ($step) must be of type int\|float, string given");` |
|         1 | 3613 | `		}` |
|         3 | 3614 | `		return iKind;` |
|         - | 3615 | `	}` |
|         - | 3616 | `	/* int / bool */` |
|        33 | 3617 | `	*pLong = ph7_value_to_int64(pIn);` |
|        33 | 3618 | `	return RANGE_IN_LONG;` |
|        32 | 3619 | `}` |
|         - | 3620 | `/*` |
|         - | 3621 | ` * php_range_process_input port: resolve $start/$end into a number and/or a` |
|         - | 3622 | ` * char-range byte, emitting php's exact warnings (empty string, multi-byte` |
|         - | 3623 | ` * string) and ValueErrors (INF/NAN). Returns a RANGE_IN_* code, or` |
|         - | 3624 | ` * RANGE_IN_ERROR after throwing (*pRc set).` |
|         - | 3625 | ` */` |
|       220 | 3626 | `static sxu8 RangeProcessInput(ph7_context *pCtx,ph7_value *pIn,int iArg,const char *zName,` |
|         - | 3627 | `	int bNullCoerced,sxi64 *pLong,double *pDouble,unsigned char *pChar,sxi32 *pRc)` |
|         1 | 3628 | `{` |
|         - | 3629 | `	char zMsg[160];` |
|         - | 3630 | `	double r;` |
|       221 | 3631 | `	*pRc = PH7_OK;` |
|       221 | 3632 | `	if( bNullCoerced ){` |
|         - | 3633 | `		/* ZPP already deprecated the null; it reads as int 0. */` |
|         5 | 3634 | `		*pLong = 0;` |
|         5 | 3635 | `		*pDouble = 0.0;` |
|         5 | 3636 | `		return RANGE_IN_LONG;` |
|         - | 3637 | `	}` |
|       217 | 3638 | `	if( pIn->iFlags & MEMOBJ_REAL ){` |
|        21 | 3639 | `		r = ph7_value_to_double(pIn);` |
|        12 | 3640 | `check_dval:` |
|        25 | 3641 | `		if( PH7_IS_INF(r) ){` |
|         7 | 3642 | `			*pRc = PH7_VmThrowException(pCtx,"ValueError",` |
|         2 | 3643 | `				"range(): Argument #%d ($%s) must be a finite number, INF provided",iArg,zName);` |
|         5 | 3644 | `			return RANGE_IN_ERROR;` |
|         - | 3645 | `		}` |
|        21 | 3646 | `		if( PH7_IS_NAN(r) ){` |
|         7 | 3647 | `			*pRc = PH7_VmThrowException(pCtx,"ValueError",` |
|         2 | 3648 | `				"range(): Argument #%d ($%s) must be a finite number, NAN provided",iArg,zName);` |
|         5 | 3649 | `			return RANGE_IN_ERROR;` |
|         - | 3650 | `		}` |
|        17 | 3651 | `		*pDouble = r;` |
|        17 | 3652 | `		return RANGE_IN_DOUBLE;` |
|         - | 3653 | `	}` |
|       197 | 3654 | `	if( pIn->iFlags & MEMOBJ_STRING ){` |
|         - | 3655 | `		const char *zStr;` |
|         - | 3656 | `		int nLen;` |
|         - | 3657 | `		sxu8 iKind;` |
|        81 | 3658 | `		zStr = ph7_value_to_string(pIn,&nLen);` |
|        81 | 3659 | `		if( nLen == 0 ){` |
|         7 | 3660 | `			SyBufferFormat(zMsg,sizeof(zMsg),` |
|         2 | 3661 | `				"range(): Argument #%d ($%s) must not be empty, casted to 0",iArg,zName);` |
|         5 | 3662 | `			PH7_VmThrowError(pCtx->pVm,0,PH7_CTX_WARNING,zMsg);` |
|         5 | 3663 | `			*pLong = 0;` |
|         5 | 3664 | `			*pDouble = 0.0;` |
|        41 | 3665 | `			return RANGE_IN_LONG;` |
|         - | 3666 | `		}` |
|        77 | 3667 | `		iKind = RangeStrToNumber(zStr,(sxu32)nLen,pLong,pDouble);` |
|        77 | 3668 | `		if( iKind == RANGE_IN_DOUBLE ){` |
|         5 | 3669 | `			r = *pDouble;` |
|         5 | 3670 | `			goto check_dval;` |
|         - | 3671 | `		}` |
|        73 | 3672 | `		if( iKind == RANGE_IN_LONG ){` |
|        23 | 3673 | `			*pDouble = (double)*pLong;` |
|        23 | 3674 | `			if( nLen == 1 ){` |
|         - | 3675 | `				/* A single numeric digit works as both a char and a number. */` |
|         9 | 3676 | `				*pChar = (unsigned char)zStr[0];` |
|         9 | 3677 | `				return RANGE_IN_DIGIT;` |
|         - | 3678 | `			}` |
|        15 | 3679 | `			return RANGE_IN_LONG;` |
|         - | 3680 | `		}` |
|        51 | 3681 | `		if( nLen != 1 ){` |
|        10 | 3682 | `			SyBufferFormat(zMsg,sizeof(zMsg),` |
|         3 | 3683 | `				"range(): Argument #%d ($%s) must be a single byte, subsequent bytes are ignored",iArg,zName);` |
|         7 | 3684 | `			PH7_VmThrowError(pCtx->pVm,0,PH7_CTX_WARNING,zMsg);` |
|         3 | 3685 | `		}` |
|        51 | 3686 | `		*pChar = (unsigned char)zStr[0];` |
|         - | 3687 | `		/* Fall-back numeric value in case the other argument is not a string. */` |
|        51 | 3688 | `		*pLong = 0;` |
|        51 | 3689 | `		*pDouble = 0.0;` |
|        51 | 3690 | `		return RANGE_IN_STRING;` |
|         - | 3691 | `	}` |
|         - | 3692 | `	/* int / bool */` |
|       117 | 3693 | `	*pLong = ph7_value_to_int64(pIn);` |
|       117 | 3694 | `	*pDouble = (double)*pLong;` |
|       117 | 3695 | `	return RANGE_IN_LONG;` |
|       111 | 3696 | `}` |
|         - | 3697 | `/*` |
|         - | 3698 | ` * The two "supplied range exceeds the maximum array size" ValueErrors.` |
|         - | 3699 | ` * Both php messages print the macro's (start,end) parameters, which its` |
|         - | 3700 | ` * callers pass SWAPPED for a decreasing range — a php quirk kept for` |
|         - | 3701 | ` * byte-parity (callers below pass the values to *print*). The int and` |
|         - | 3702 | ` * float variants differ in wording ("Maximum size: N." vs "Max size: N")` |
|         - | 3703 | ` * exactly like php's two macros.` |
|         - | 3704 | ` */` |
|         6 | 3705 | `static sxi32 RangeLongSizeError(ph7_context *pCtx,sxu64 nCalc,sxi64 iStart,sxi64 iEnd,sxi64 iStep)` |
|         1 | 3706 | `{` |
|        10 | 3707 | `	return PH7_VmThrowException(pCtx,"ValueError",` |
|         - | 3708 | `		"The supplied range exceeds the maximum array size by %qu elements: "` |
|         - | 3709 | `		"start=%qd, end=%qd, step=%qd. Calculated size: %qu. Maximum size: %qu.",` |
|         3 | 3710 | `		nCalc - (sxu64)(PH7_RANGE_HT_MAX_SIZE - 1),iStart,iEnd,iStep,` |
|         3 | 3711 | `		nCalc,(sxu64)PH7_RANGE_HT_MAX_SIZE);` |
|         1 | 3712 | `}` |
|         6 | 3713 | `static sxi32 RangeDoubleSizeError(ph7_context *pCtx,double rCalc,double rStart,double rEnd,double rStep)` |
|         1 | 3714 | `{` |
|         - | 3715 | `	/* Four %.1f doubles can reach ~313 bytes each near DBL_MAX, so format on` |
|         - | 3716 | `	 * the VM heap (auto-released with the call context) rather than parking` |
|         - | 3717 | `	 * ~1.5 KB on the native stack of a small-stack embedded port. */` |
|         7 | 3718 | `	const unsigned int nBuf = 1500;` |
|         7 | 3719 | `	char *zMsg = (char *)ph7_context_alloc_chunk(pCtx,nBuf,FALSE,TRUE/* Auto-release */);` |
|         7 | 3720 | `	if( zMsg == 0 ){` |
|       ! 0 | 3721 | `		return PH7_ContextMemoryError(pCtx);` |
|         - | 3722 | `	}` |
|         7 | 3723 | `	snprintf(zMsg,nBuf,` |
|         - | 3724 | `		"The supplied range exceeds the maximum array size by %.1f elements: "` |
|         - | 3725 | `		"start=%.1f, end=%.1f, step=%.1f. Max size: 1073741824",` |
|         - | 3726 | `		rCalc - (double)PH7_RANGE_HT_MAX_SIZE,rStart,rEnd,rStep);` |
|         7 | 3727 | `	return PH7_VmThrowException(pCtx,"ValueError","%s",zMsg);` |
|         4 | 3728 | `}` |
|         - | 3729 | `/*` |
|         - | 3730 | ` * Set the element container to the next range element and append it to the` |
|         - | 3731 | ` * result array, surfacing allocation failure as the OOM fatal (never a` |
|         - | 3732 | ` * silently-truncated array). One helper per element type so the fill loops` |
|         - | 3733 | ` * below stay one line per iteration.` |
|         - | 3734 | ` */` |
|       334 | 3735 | `static sxi32 RangeAppendInt(ph7_context *pCtx,ph7_value *pArray,ph7_value *pValue,sxi64 iVal)` |
|         1 | 3736 | `{` |
|       335 | 3737 | `	ph7_value_int64(pValue,iVal);` |
|       335 | 3738 | `	if( ph7_array_add_elem(pArray,0/* Automatic index assign*/,pValue) != SXRET_OK ){` |
|       ! 0 | 3739 | `		return PH7_ContextMemoryError(pCtx);` |
|         - | 3740 | `	}` |
|       335 | 3741 | `	return PH7_OK;` |
|       168 | 3742 | `}` |
|        70 | 3743 | `static sxi32 RangeAppendDouble(ph7_context *pCtx,ph7_value *pArray,ph7_value *pValue,double rVal)` |
|         1 | 3744 | `{` |
|        71 | 3745 | `	ph7_value_double(pValue,rVal);` |
|        71 | 3746 | `	if( ph7_array_add_elem(pArray,0,pValue) != SXRET_OK ){` |
|       ! 0 | 3747 | `		return PH7_ContextMemoryError(pCtx);` |
|         - | 3748 | `	}` |
|        71 | 3749 | `	return PH7_OK;` |
|        36 | 3750 | `}` |
|       168 | 3751 | `static sxi32 RangeAppendChar(ph7_context *pCtx,ph7_value *pArray,ph7_value *pValue,char c)` |
|         1 | 3752 | `{` |
|       169 | 3753 | `	ph7_value_string(pValue,&c,1);` |
|       169 | 3754 | `	if( ph7_array_add_elem(pArray,0,pValue) != SXRET_OK ){` |
|       ! 0 | 3755 | `		return PH7_ContextMemoryError(pCtx);` |
|         - | 3756 | `	}` |
|       169 | 3757 | `	ph7_value_reset_string_cursor(pValue);` |
|       169 | 3758 | `	return PH7_OK;` |
|        85 | 3759 | `}` |
|         - | 3760 | `/*` |
|         - | 3761 | ` * array range(string\|int\|float $start,string\|int\|float $end,int\|float $step = 1)` |
|         - | 3762 | ` *  Create an array containing a range of elements.` |
|         - | 3763 | ` * Return` |
|         - | 3764 | ` *  An array of elements from start to end, inclusive; int, float, or` |
|         - | 3765 | ` *  single-character string elements depending on the inputs, like php 8.` |
|         - | 3766 | ` */` |
|       136 | 3767 | `static int ph7_hashmap_range(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|         1 | 3768 | `{` |
|         - | 3769 | `	ph7_value *pValue,*pArray;` |
|       137 | 3770 | `	sxi32 rc = PH7_OK;` |
|       137 | 3771 | `	int is_step_double = 0,is_step_negative = 0;` |
|       137 | 3772 | `	double step_double = 1.0;` |
|       137 | 3773 | `	sxi64 step = 1;` |
|         - | 3774 | `	sxu8 start_type,end_type;` |
|       137 | 3775 | `	sxi64 start_long = 0,end_long = 0;` |
|       137 | 3776 | `	double start_double = 0.0,end_double = 0.0;` |
|       137 | 3777 | `	unsigned char cStart = 0,cEnd = 0;` |
|       137 | 3778 | `	int bStartNull = FALSE,bEndNull = FALSE;` |
|         - | 3779 | `	sxu32 i,size;` |
|         - | 3780 |  |
|         - | 3781 | `	/* php ZPP arity: at least 2 (enforced centrally, aBuiltinArity), at most 3. */` |
|       137 | 3782 | `	if( nArg > 3 ){` |
|         4 | 3783 | `		return PH7_VmThrowException(pCtx,"ArgumentCountError",` |
|         1 | 3784 | `			"range() expects at most 3 arguments, %d given",nArg);` |
|         - | 3785 | `	}` |
|       135 | 3786 | `	if( nArg < 2 ){` |
|         - | 3787 | `		/* Defensive only: the central arity table throws before we run. */` |
|       ! 0 | 3788 | `		return PH7_VmThrowException(pCtx,"ArgumentCountError",` |
|       ! 0 | 3789 | `			"range() expects at least 2 arguments, %d given",nArg);` |
|         - | 3790 | `	}` |
|         - | 3791 | `	/* ZPP pass in argument order: type errors and null deprecations fire` |
|         - | 3792 | `	 * before any value/domain check, like php's zend_parse_parameters. */` |
|       135 | 3793 | `	if( !RangeEndpointZpp(pCtx,apArg[0],1,"start",&bStartNull,&rc) ){` |
|         7 | 3794 | `		return rc;` |
|         - | 3795 | `	}` |
|       129 | 3796 | `	if( !RangeEndpointZpp(pCtx,apArg[1],2,"end",&bEndNull,&rc) ){` |
|       ! 0 | 3797 | `		return rc;` |
|         - | 3798 | `	}` |
|       129 | 3799 | `	if( nArg > 2 ){` |
|        63 | 3800 | `		sxu8 iStepKind = RangeStepInput(pCtx,apArg[2],&step,&step_double,&rc);` |
|        63 | 3801 | `		if( iStepKind == RANGE_IN_ERROR ){` |
|         5 | 3802 | `			return rc;` |
|         - | 3803 | `		}` |
|        59 | 3804 | `		if( iStepKind == RANGE_IN_DOUBLE ){` |
|        25 | 3805 | `			if( PH7_IS_INF(step_double) ){` |
|         3 | 3806 | `				return PH7_VmThrowException(pCtx,"ValueError",` |
|         - | 3807 | `					"range(): Argument #3 ($step) must be a finite number, INF provided");` |
|         - | 3808 | `			}` |
|        23 | 3809 | `			if( PH7_IS_NAN(step_double) ){` |
|         3 | 3810 | `				return PH7_VmThrowException(pCtx,"ValueError",` |
|         - | 3811 | `					"range(): Argument #3 ($step) must be a finite number, NAN provided");` |
|         - | 3812 | `			}` |
|         - | 3813 | `			/* We only want positive step values. */` |
|        21 | 3814 | `			if( step_double < 0.0 ){` |
|       ! 0 | 3815 | `				is_step_negative = 1;` |
|       ! 0 | 3816 | `				step_double *= -1;` |
|       ! 0 | 3817 | `			}` |
|         - | 3818 | `			/* zend_dval_to_lval_silent + zend_is_long_compatible: an integral` |
|         - | 3819 | `			 * in-sxi64-range float step behaves as an int (char ranges accept` |
|         - | 3820 | `			 * it, int endpoints stay int); anything else is a float step. */` |
|        21 | 3821 | `			if( step_double < 9223372036854775808.0 ){` |
|        19 | 3822 | `				step = (sxi64)step_double;` |
|        19 | 3823 | `				if( (double)step != step_double ){` |
|        17 | 3824 | `					is_step_double = 1;` |
|         8 | 3825 | `				}` |
|        10 | 3826 | `			}else{` |
|         - | 3827 | ``				/* Casting out-of-range would be UB; `step` stays unread —`` |
|         - | 3828 | `				 * every reader is gated behind !is_step_double. */` |
|         3 | 3829 | `				is_step_double = 1;` |
|         - | 3830 | `			}` |
|        11 | 3831 | `		}else{` |
|         - | 3832 | `			/* We only want positive step values. */` |
|        35 | 3833 | `			if( step < 0 ){` |
|        11 | 3834 | `				if( step == SMALLEST_INT64 ){` |
|         - | 3835 | `					/* -step would overflow */` |
|         4 | 3836 | `					return PH7_VmThrowException(pCtx,"ValueError",` |
|         1 | 3837 | `						"range(): Argument #3 ($step) must be greater than %qd",step);` |
|         - | 3838 | `				}` |
|         9 | 3839 | `				is_step_negative = 1;` |
|         9 | 3840 | `				step = -step;` |
|         4 | 3841 | `			}` |
|        33 | 3842 | `			step_double = (double)step;` |
|         - | 3843 | `		}` |
|        53 | 3844 | `		if( step_double == 0.0 ){` |
|         7 | 3845 | `			return PH7_VmThrowException(pCtx,"ValueError",` |
|         - | 3846 | `				"range(): Argument #3 ($step) cannot be 0");` |
|         - | 3847 | `		}` |
|        23 | 3848 | `	}` |
|       113 | 3849 | `	start_type = RangeProcessInput(pCtx,apArg[0],1,"start",bStartNull,&start_long,&start_double,&cStart,&rc);` |
|       113 | 3850 | `	if( start_type == RANGE_IN_ERROR ){` |
|         5 | 3851 | `		return rc;` |
|         - | 3852 | `	}` |
|       109 | 3853 | `	end_type = RangeProcessInput(pCtx,apArg[1],2,"end",bEndNull,&end_long,&end_double,&cEnd,&rc);` |
|       109 | 3854 | `	if( end_type == RANGE_IN_ERROR ){` |
|         5 | 3855 | `		return rc;` |
|         - | 3856 | `	}` |
|         - | 3857 | `	/* Element container + result array */` |
|       105 | 3858 | `	pValue = ph7_context_new_scalar(pCtx);` |
|       105 | 3859 | `	pArray = ph7_context_new_array(pCtx);` |
|       105 | 3860 | `	if( pValue == 0 \|\| pArray == 0 ){` |
|       ! 0 | 3861 | `		return PH7_ContextMemoryError(pCtx);` |
|         - | 3862 | `	}` |
|         - | 3863 | `	/* If the range is given as strings, generate an array of characters. */` |
|       105 | 3864 | `	if( start_type >= RANGE_IN_STRING \|\| end_type >= RANGE_IN_STRING ){` |
|        37 | 3865 | `		if( start_type < RANGE_IN_STRING \|\| end_type < RANGE_IN_STRING ){` |
|         - | 3866 | `			/* Only one side is a string: the char side converts to 0 (with a` |
|         - | 3867 | `			 * warning unless the numeric side is an ambiguous single digit)` |
|         - | 3868 | `			 * and the range is numeric. */` |
|        15 | 3869 | `			if( start_type < RANGE_IN_STRING ){` |
|         7 | 3870 | `				if( end_type != RANGE_IN_DIGIT ){` |
|         7 | 3871 | `					PH7_VmThrowError(pCtx->pVm,0,PH7_CTX_WARNING,` |
|         - | 3872 | `						"range(): Argument #1 ($start) must be a single byte string if"` |
|         - | 3873 | `						" argument #2 ($end) is a single byte string, argument #2 ($end) converted to 0");` |
|         3 | 3874 | `				}` |
|         7 | 3875 | `				end_type = RANGE_IN_LONG;` |
|         4 | 3876 | `			}else{` |
|         9 | 3877 | `				if( start_type != RANGE_IN_DIGIT ){` |
|         9 | 3878 | `					PH7_VmThrowError(pCtx->pVm,0,PH7_CTX_WARNING,` |
|         - | 3879 | `						"range(): Argument #2 ($end) must be a single byte string if"` |
|         - | 3880 | `						" argument #1 ($start) is a single byte string, argument #1 ($start) converted to 0");` |
|         4 | 3881 | `				}` |
|         9 | 3882 | `				start_type = RANGE_IN_LONG;` |
|         - | 3883 | `			}` |
|        15 | 3884 | `			goto handle_numeric_inputs;` |
|         - | 3885 | `		}` |
|        23 | 3886 | `		if( is_step_double ){` |
|         - | 3887 | `			/* Only emit the warning if one of the inputs is not a numeric digit. */` |
|         5 | 3888 | `			if( start_type == RANGE_IN_STRING \|\| end_type == RANGE_IN_STRING ){` |
|         3 | 3889 | `				PH7_VmThrowError(pCtx->pVm,0,PH7_CTX_WARNING,` |
|         - | 3890 | `					"range(): Argument #3 ($step) must be of type int when generating an array"` |
|         - | 3891 | `					" of characters, inputs converted to 0");` |
|         1 | 3892 | `			}` |
|         5 | 3893 | `			start_type = RANGE_IN_LONG;` |
|         5 | 3894 | `			end_type = RANGE_IN_LONG;` |
|         5 | 3895 | `			goto handle_numeric_inputs;` |
|         - | 3896 | `		}` |
|         - | 3897 | `		/* Generate an array of characters */` |
|        19 | 3898 | `		if( cStart > cEnd ){` |
|         - | 3899 | `			/* Decreasing char range */` |
|         - | 3900 | `			int iCur;` |
|         3 | 3901 | `			if( (sxi64)(cStart - cEnd) < step ){` |
|       ! 0 | 3902 | `				goto boundary_error;` |
|         - | 3903 | `			}` |
|        17 | 3904 | `			for( iCur = (int)cStart ; iCur >= (int)cEnd ; iCur -= (int)step ){` |
|        15 | 3905 | `				if( (rc = RangeAppendChar(pCtx,pArray,pValue,(char)iCur)) != PH7_OK ){` |
|       ! 0 | 3906 | `					return rc;` |
|         - | 3907 | `				}` |
|         8 | 3908 | `			}` |
|        18 | 3909 | `		}else if( cEnd > cStart ){` |
|         - | 3910 | `			/* Increasing char range */` |
|         - | 3911 | `			int iCur;` |
|        15 | 3912 | `			if( is_step_negative ){` |
|         3 | 3913 | `				goto negative_step_error;` |
|         - | 3914 | `			}` |
|        13 | 3915 | `			if( (sxi64)(cEnd - cStart) < step ){` |
|         3 | 3916 | `				goto boundary_error;` |
|         - | 3917 | `			}` |
|       163 | 3918 | `			for( iCur = (int)cStart ; iCur <= (int)cEnd ; iCur += (int)step ){` |
|       153 | 3919 | `				if( (rc = RangeAppendChar(pCtx,pArray,pValue,(char)iCur)) != PH7_OK ){` |
|       ! 0 | 3920 | `					return rc;` |
|         - | 3921 | `				}` |
|        77 | 3922 | `			}` |
|         6 | 3923 | `		}else{` |
|         3 | 3924 | `			if( (rc = RangeAppendChar(pCtx,pArray,pValue,(char)cStart)) != PH7_OK ){` |
|       ! 0 | 3925 | `				return rc;` |
|         - | 3926 | `			}` |
|         - | 3927 | `		}` |
|        15 | 3928 | `		ph7_result_value(pCtx,pArray);` |
|        15 | 3929 | `		return PH7_OK;` |
|         - | 3930 | `	}` |
|        34 | 3931 | `handle_numeric_inputs:` |
|        95 | 3932 | `	if( start_type == RANGE_IN_DOUBLE \|\| end_type == RANGE_IN_DOUBLE \|\| is_step_double ){` |
|         - | 3933 | `		/* Float range */` |
|         - | 3934 | `		double elem,calc;` |
|        25 | 3935 | `		if( start_double > end_double ){` |
|         - | 3936 | `			/* Decreasing float range */` |
|         7 | 3937 | `			if( start_double - end_double < step_double ){` |
|       ! 0 | 3938 | `				goto boundary_error;` |
|         - | 3939 | `			}` |
|         7 | 3940 | `			calc = ((start_double - end_double) / step_double) + 1;` |
|         7 | 3941 | `			if( calc >= (double)PH7_RANGE_HT_MAX_SIZE ){` |
|         - | 3942 | `				/* php prints start/end swapped here (see RangeDoubleSizeError). */` |
|         3 | 3943 | `				return RangeDoubleSizeError(pCtx,calc,end_double,start_double,step_double);` |
|         - | 3944 | `			}` |
|         5 | 3945 | `			size = (sxu32)(calc + 0.5); /* _php_math_round(...,0,HALF_UP) */` |
|        19 | 3946 | `			for( i = 0,elem = start_double ; i < size && elem >= end_double ; ++i,elem = start_double - ((double)i * step_double) ){` |
|        15 | 3947 | `				if( (rc = RangeAppendDouble(pCtx,pArray,pValue,elem)) != PH7_OK ){` |
|       ! 0 | 3948 | `					return rc;` |
|         - | 3949 | `				}` |
|         8 | 3950 | `			}` |
|        21 | 3951 | `		}else if( end_double > start_double ){` |
|         - | 3952 | `			/* Increasing float range */` |
|        17 | 3953 | `			if( is_step_negative ){` |
|       ! 0 | 3954 | `				goto negative_step_error;` |
|         - | 3955 | `			}` |
|        17 | 3956 | `			if( end_double - start_double < step_double ){` |
|         3 | 3957 | `				goto boundary_error;` |
|         - | 3958 | `			}` |
|        15 | 3959 | `			calc = ((end_double - start_double) / step_double) + 1;` |
|        15 | 3960 | `			if( calc >= (double)PH7_RANGE_HT_MAX_SIZE ){` |
|         5 | 3961 | `				return RangeDoubleSizeError(pCtx,calc,start_double,end_double,step_double);` |
|         - | 3962 | `			}` |
|        11 | 3963 | `			size = (sxu32)(calc + 0.5);` |
|        65 | 3964 | `			for( i = 0,elem = start_double ; i < size && elem <= end_double ; ++i,elem = start_double + ((double)i * step_double) ){` |
|        55 | 3965 | `				if( (rc = RangeAppendDouble(pCtx,pArray,pValue,elem)) != PH7_OK ){` |
|       ! 0 | 3966 | `					return rc;` |
|         - | 3967 | `				}` |
|        28 | 3968 | `			}` |
|         6 | 3969 | `		}else{` |
|         3 | 3970 | `			if( (rc = RangeAppendDouble(pCtx,pArray,pValue,start_double)) != PH7_OK ){` |
|       ! 0 | 3971 | `				return rc;` |
|         - | 3972 | `			}` |
|         - | 3973 | `		}` |
|         9 | 3974 | `	}else{` |
|         - | 3975 | `		/* Int range. All arithmetic in unsigned space so a span wider than` |
|         - | 3976 | `		 * LARGEST_INT64 (e.g. -PHP_INT_MAX..PHP_INT_MAX) wraps correctly` |
|         - | 3977 | `		 * instead of overflowing, exactly like php's zend_ulong math. */` |
|        63 | 3978 | `		sxu64 ustep = (sxu64)step;` |
|         - | 3979 | `		sxu64 calc;` |
|        63 | 3980 | `		if( start_long > end_long ){` |
|         - | 3981 | `			/* Decreasing int range */` |
|        19 | 3982 | `			if( (sxu64)start_long - (sxu64)end_long < ustep ){` |
|         3 | 3983 | `				goto boundary_error;` |
|         - | 3984 | `			}` |
|        17 | 3985 | `			calc = ((sxu64)start_long - (sxu64)end_long) / ustep;` |
|        17 | 3986 | `			if( calc >= (sxu64)(PH7_RANGE_HT_MAX_SIZE - 1) ){` |
|         - | 3987 | `				/* php prints start/end swapped here (see RangeLongSizeError). */` |
|         3 | 3988 | `				return RangeLongSizeError(pCtx,calc,end_long,start_long,step);` |
|         - | 3989 | `			}` |
|        15 | 3990 | `			size = (sxu32)(calc + 1);` |
|       101 | 3991 | `			for( i = 0 ; i < size ; ++i ){` |
|        87 | 3992 | `				if( (rc = RangeAppendInt(pCtx,pArray,pValue,(sxi64)((sxu64)start_long - (sxu64)i * ustep))) != PH7_OK ){` |
|       ! 0 | 3993 | `					return rc;` |
|         - | 3994 | `				}` |
|        44 | 3995 | `			}` |
|        52 | 3996 | `		}else if( end_long > start_long ){` |
|         - | 3997 | `			/* Increasing int range */` |
|        39 | 3998 | `			if( is_step_negative ){` |
|         3 | 3999 | `				goto negative_step_error;` |
|         - | 4000 | `			}` |
|        37 | 4001 | `			if( (sxu64)end_long - (sxu64)start_long < ustep ){` |
|         3 | 4002 | `				goto boundary_error;` |
|         - | 4003 | `			}` |
|        35 | 4004 | `			calc = ((sxu64)end_long - (sxu64)start_long) / ustep;` |
|        35 | 4005 | `			if( calc >= (sxu64)(PH7_RANGE_HT_MAX_SIZE - 1) ){` |
|         5 | 4006 | `				return RangeLongSizeError(pCtx,calc,start_long,end_long,step);` |
|         - | 4007 | `			}` |
|        31 | 4008 | `			size = (sxu32)(calc + 1);` |
|       273 | 4009 | `			for( i = 0 ; i < size ; ++i ){` |
|       243 | 4010 | `				if( (rc = RangeAppendInt(pCtx,pArray,pValue,(sxi64)((sxu64)start_long + (sxu64)i * ustep))) != PH7_OK ){` |
|       ! 0 | 4011 | `					return rc;` |
|         - | 4012 | `				}` |
|       122 | 4013 | `			}` |
|        16 | 4014 | `		}else{` |
|         7 | 4015 | `			if( (rc = RangeAppendInt(pCtx,pArray,pValue,start_long)) != PH7_OK ){` |
|       ! 0 | 4016 | `				return rc;` |
|         - | 4017 | `			}` |
|         - | 4018 | `		}` |
|         - | 4019 | `	}` |
|         - | 4020 | `	/* Return the new array. 'pValue' is released automatically by the` |
|         - | 4021 | `	 * virtual machine as soon as we return from this foreign function. */` |
|        67 | 4022 | `	ph7_result_value(pCtx,pArray);` |
|        67 | 4023 | `	return PH7_OK;` |
|         2 | 4024 | `negative_step_error:` |
|         5 | 4025 | `	return PH7_VmThrowException(pCtx,"ValueError",` |
|         - | 4026 | `		"range(): Argument #3 ($step) must be greater than 0 for increasing ranges");` |
|         4 | 4027 | `boundary_error:` |
|         9 | 4028 | `	return PH7_VmThrowException(pCtx,"ValueError",` |
|         - | 4029 | `		"range(): Argument #3 ($step) must be less than the range spanned by argument #1 ($start) and argument #2 ($end)");` |
|        69 | 4030 | `}` |
|         - | 4031 | `/*` |
|         - | 4032 | ` * array array_values(array $array)` |
|         - | 4033 | ` *  Return all the values of an array, indexed numerically.` |
|         - | 4034 | ` * Parameters` |
|         - | 4035 | ` *  $array` |
|         - | 4036 | ` *   The input array.` |
|         - | 4037 | ` * Return` |
|         - | 4038 | ` *  An indexed array of values or NULL on allocation failure.` |
|         - | 4039 | ` */` |
|        36 | 4040 | `static int ph7_hashmap_values(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|         4 | 4041 | `{` |
|         - | 4042 | `	ph7_hashmap_node *pNode;` |
|         - | 4043 | `	ph7_hashmap *pMap;` |
|         - | 4044 | `	ph7_value *pArray;` |
|         - | 4045 | `	ph7_value *pObj;` |
|         - | 4046 | `	sxu32 n;` |
|        40 | 4047 | `	if( nArg != 1 ){` |
|         - | 4048 | `		/* Wrong argument count, throw ArgumentCountError */` |
|         8 | 4049 | `		return PH7_VmThrowException(pCtx,` |
|         - | 4050 | `			"ArgumentCountError",` |
|         - | 4051 | `			"array_values() expects exactly 1 argument, %d given",` |
|         2 | 4052 | `			nArg` |
|         - | 4053 | `			);` |
|         - | 4054 | `	}` |
|         - | 4055 | `	/* Make sure we are dealing with a valid hashmap */` |
|        35 | 4056 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|         - | 4057 | `		/* Type mismatch, throw TypeError */` |
|         4 | 4058 | `		return PH7_VmThrowException(pCtx,` |
|         - | 4059 | `			"TypeError",` |
|         - | 4060 | `			"array_values(): Argument #1 ($array) must be of type array, %s given",` |
|         1 | 4061 | `			ph7_type_name(apArg[0])` |
|         - | 4062 | `			);` |
|         - | 4063 | `	}` |
|         - | 4064 | `	/* Point to the internal representation that describe the input hashmap */` |
|        32 | 4065 | `	pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|         - | 4066 | `	/* Create a new array */` |
|        32 | 4067 | `	pArray = ph7_context_new_array(pCtx);` |
|        32 | 4068 | `	if( pArray == 0 ){` |
|       ! 0 | 4069 | `		ph7_result_null(pCtx);` |
|       ! 0 | 4070 | `		return PH7_OK;` |
|         - | 4071 | `	}` |
|         - | 4072 | `	/* Perform the requested operation */` |
|        32 | 4073 | `	pNode = pMap->pFirst;` |
|       104 | 4074 | `	for( n = 0 ; n < pMap->nEntry ; ++n ){` |
|        74 | 4075 | `		pObj = HashmapExtractNodeValue(pNode);` |
|        74 | 4076 | `		if( pObj ){` |
|         - | 4077 | `			/* perform the insertion */` |
|        74 | 4078 | `			ph7_array_add_elem(pArray,0/* Automatic index assign */,pObj);` |
|        36 | 4079 | `		}` |
|         - | 4080 | `		/* Point to the next entry */` |
|        74 | 4081 | `		pNode = pNode->pPrev; /* Reverse link */` |
|        38 | 4082 | `	}` |
|         - | 4083 | `	/* return the new array */` |
|        32 | 4084 | `	ph7_result_value(pCtx,pArray);` |
|        32 | 4085 | `	return PH7_OK;` |
|        22 | 4086 | `}` |
|         - | 4087 | `/*` |
|         - | 4088 | ` * array array_keys(array $input [, val $search_value [, bool $strict = false ]] )` |
|         - | 4089 | ` *  Return all the keys or a subset of the keys of an array.` |
|         - | 4090 | ` * Parameters` |
|         - | 4091 | ` *  $input` |
|         - | 4092 | ` *   An array containing keys to return.` |
|         - | 4093 | ` * $search_value` |
|         - | 4094 | ` *   If specified, then only keys containing these values are returned.` |
|         - | 4095 | ` * $strict` |
|         - | 4096 | ` *   Determines if strict comparison (===) should be used during the search.` |
|         - | 4097 | ` * Return` |
|         - | 4098 | ` *  An array of all the keys in input or NULL on failure.` |
|         - | 4099 | ` */` |
|       142 | 4100 | `static int ph7_hashmap_keys(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|         5 | 4101 | `{` |
|         - | 4102 | `	ph7_hashmap_node *pNode;` |
|         - | 4103 | `	ph7_hashmap *pMap;` |
|         - | 4104 | `	ph7_value *pArray;` |
|         - | 4105 | `	ph7_value sObj;` |
|         - | 4106 | `	ph7_value sVal;` |
|         - | 4107 | `	SyString sKey;` |
|         - | 4108 | `	int bStrict;` |
|         - | 4109 | `	sxi32 rc;` |
|         - | 4110 | `	sxu32 n;` |
|       147 | 4111 | `	if( nArg < 1 ){` |
|         - | 4112 | `		/* Missing argument,throw ArgumentCountError */` |
|         3 | 4113 | `		return PH7_VmThrowException(pCtx,` |
|         - | 4114 | `			"ArgumentCountError",` |
|         - | 4115 | `			"array_keys() expects at least 1 argument, 0 given"` |
|         - | 4116 | `			);` |
|         - | 4117 | `	}` |
|         - | 4118 | `	/* Make sure we are dealing with a valid hashmap */` |
|       144 | 4119 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|         - | 4120 | `		/* haystack must be an array,throw TypeError */` |
|         4 | 4121 | `		return PH7_VmThrowException(pCtx,` |
|         - | 4122 | `			"TypeError",` |
|         - | 4123 | `			"array_keys(): Argument #1 ($array) must be of type array, %s given",` |
|         1 | 4124 | `			ph7_type_name(apArg[0])` |
|         - | 4125 | `			);` |
|         - | 4126 | `	}` |
|         - | 4127 | `	/* Point to the internal representation of the input hashmap */` |
|       142 | 4128 | `	pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|         - | 4129 | `	/* Create a new array */` |
|       142 | 4130 | `	pArray = ph7_context_new_array(pCtx);` |
|       142 | 4131 | `	if( pArray == 0 ){` |
|       ! 0 | 4132 | `		ph7_result_null(pCtx);` |
|       ! 0 | 4133 | `		return PH7_OK;` |
|         - | 4134 | `	}` |
|       142 | 4135 | `	bStrict = FALSE;` |
|       142 | 4136 | `	if( nArg > 2 ){` |
|         - | 4137 | `		/* In PHP, non-scalar values for a bool-hinted parameter raise TypeError */` |
|         8 | 4138 | `		if( ph7_value_is_array(apArg[2]) \|\| ph7_value_is_object(apArg[2]) \|\| ph7_value_is_resource(apArg[2]) ){` |
|         4 | 4139 | `			return PH7_VmThrowException(pCtx,` |
|         - | 4140 | `				"TypeError",` |
|         - | 4141 | `				"array_keys(): Argument #3 ($strict) must be of type bool, %s given",` |
|         2 | 4142 | `				ph7_type_name(apArg[2])` |
|         - | 4143 | `				);` |
|         - | 4144 | `		}` |
|         5 | 4145 | `		bStrict = ph7_value_to_bool(apArg[2]);` |
|         2 | 4146 | `	}` |
|         - | 4147 | `	/* Perform the requested operation */` |
|       139 | 4148 | `	pNode = pMap->pFirst;` |
|       139 | 4149 | `	PH7_MemObjInit(pMap->pVm,&sVal);` |
|      1357 | 4150 | `	for( n = 0 ; n < pMap->nEntry ; ++n ){` |
|      1221 | 4151 | `		if( pNode->iType == HASHMAP_INT_NODE ){` |
|       129 | 4152 | `			PH7_MemObjInitFromInt(pMap->pVm,&sObj,pNode->xKey.iKey);` |
|        66 | 4153 | `		}else{` |
|      1094 | 4154 | `			SyStringInitFromBuf(&sKey,SyBlobData(&pNode->xKey.sKey),SyBlobLength(&pNode->xKey.sKey));` |
|      1094 | 4155 | `			PH7_MemObjInitFromString(pMap->pVm,&sObj,&sKey);` |
|         - | 4156 | `		}` |
|      1221 | 4157 | `		rc = 0;` |
|      1221 | 4158 | `		if( nArg > 1 ){` |
|        31 | 4159 | `			ph7_value *pValue = HashmapExtractNodeValue(pNode);` |
|        31 | 4160 | `			if( pValue ){` |
|        31 | 4161 | `				PH7_MemObjLoad(pValue,&sVal);` |
|         - | 4162 | `				/* Filter key */` |
|        31 | 4163 | `				rc = ph7_value_compare(&sVal,apArg[1],bStrict);` |
|        31 | 4164 | `				PH7_MemObjRelease(&sVal);` |
|        15 | 4165 | `			}` |
|        15 | 4166 | `		}` |
|      1221 | 4167 | `		if( rc == 0 ){` |
|         - | 4168 | `			/* Perform the insertion */` |
|      1203 | 4169 | `			ph7_array_add_elem(pArray,0,&sObj);` |
|       600 | 4170 | `		}` |
|      1221 | 4171 | `		PH7_MemObjRelease(&sObj);` |
|         - | 4172 | `		/* Point to the next entry */` |
|      1221 | 4173 | `		pNode = pNode->pPrev; /* Reverse link */` |
|       612 | 4174 | `	}` |
|         - | 4175 | `	/* return the new array */` |
|       139 | 4176 | `	ph7_result_value(pCtx,pArray);` |
|       139 | 4177 | `	return PH7_OK;` |
|        76 | 4178 | `}` |
|         - | 4179 | `/*` |
|         - | 4180 | ` * bool array_same(array $arr1,array $arr2)` |
|         - | 4181 | ` *  Return TRUE if the given arrays are the same instance.` |
|         - | 4182 | ` *  This function is useful under PH7 since arrays are passed` |
|         - | 4183 | ` *  by reference unlike the zend engine which use pass by values.` |
|         - | 4184 | ` * Parameters` |
|         - | 4185 | ` *  $arr1` |
|         - | 4186 | ` *   First array` |
|         - | 4187 | ` *  $arr2` |
|         - | 4188 | ` *   Second array` |
|         - | 4189 | ` * Return` |
|         - | 4190 | ` *  TRUE if the arrays are the same instance.FALSE otherwise.` |
|         - | 4191 | ` * Note` |
|         - | 4192 | ` *  This function is a symisc eXtension.` |
|         - | 4193 | ` */` |
|         4 | 4194 | `static int ph7_hashmap_same(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|         1 | 4195 | `{` |
|         - | 4196 | `	ph7_hashmap *p1,*p2;` |
|         - | 4197 | `	int rc;` |
|         5 | 4198 | `	if( nArg < 2 \|\| !ph7_value_is_array(apArg[0]) \|\| !ph7_value_is_array(apArg[1]) ){` |
|         - | 4199 | `		/* Missing or invalid arguments,return FALSE*/` |
|       ! 0 | 4200 | `		ph7_result_bool(pCtx,0);` |
|       ! 0 | 4201 | `		return PH7_OK;` |
|         - | 4202 | `	}` |
|         - | 4203 | `	/* Point to the hashmaps */` |
|         5 | 4204 | `	p1 = (ph7_hashmap *)apArg[0]->x.pOther;` |
|         5 | 4205 | `	p2 = (ph7_hashmap *)apArg[1]->x.pOther;` |
|         5 | 4206 | `	rc = (p1 == p2);` |
|         - | 4207 | `	/* Same instance? */` |
|         5 | 4208 | `	ph7_result_bool(pCtx,rc);` |
|         5 | 4209 | `	return PH7_OK;` |
|         3 | 4210 | `}` |
|         - | 4211 | `/*` |
|         - | 4212 | ` * array array_merge(array ...$arrays)` |
|         - | 4213 | ` *  Merge one or more arrays.` |
|         - | 4214 | ` * Parameters` |
|         - | 4215 | ` *  ...$arrays` |
|         - | 4216 | ` *   Variable list of arrays to merge. Each argument must be an array;` |
|         - | 4217 | ` *   passing a non-array argument throws a TypeError.` |
|         - | 4218 | ` * Return` |
|         - | 4219 | ` *  The resulting merged array. Returns an empty array when called` |
|         - | 4220 | ` *  with no arguments.` |
|         - | 4221 | ` */` |
|      1028 | 4222 | `static int ph7_hashmap_merge(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|         5 | 4223 | `{` |
|         - | 4224 | `	ph7_hashmap *pMap,*pSrc;` |
|         - | 4225 | `	ph7_value *pArray;` |
|         - | 4226 | `	int i;` |
|         - | 4227 | `	/* Create a new array */` |
|      1033 | 4228 | `	pArray = ph7_context_new_array(pCtx);` |
|      1033 | 4229 | `	if( pArray == 0 ){` |
|       ! 0 | 4230 | `		ph7_result_null(pCtx);` |
|       ! 0 | 4231 | `		return PH7_OK;` |
|         - | 4232 | `	}` |
|         - | 4233 | `	/* Point to the internal representation of the hashmap */` |
|      1033 | 4234 | `	pMap = (ph7_hashmap *)pArray->x.pOther;` |
|         - | 4235 | `	/* Start merging */` |
|      3079 | 4236 | `	for( i = 0 ; i < nArg ; i++ ){` |
|         - | 4237 | `		/* Make sure we are dealing with a valid hashmap */` |
|      2055 | 4238 | `		if( !ph7_value_is_array(apArg[i]) ){` |
|         - | 4239 | `			/* Type mismatch -> TypeError */` |
|         8 | 4240 | `			return PH7_VmThrowException(pCtx,` |
|         - | 4241 | `				"TypeError",` |
|         - | 4242 | `				"array_merge(): Argument #%d must be of type array, %s given",` |
|         2 | 4243 | `				i + 1,` |
|         4 | 4244 | `				ph7_type_name(apArg[i])` |
|         - | 4245 | `				);` |
|       ! 0 | 4246 | `		}else{` |
|      2051 | 4247 | `			pSrc = (ph7_hashmap *)apArg[i]->x.pOther;` |
|         - | 4248 | `			/* Merge the two hashmaps */` |
|      2051 | 4249 | `			HashmapMerge(pSrc,pMap);` |
|         - | 4250 | `		}` |
|      1028 | 4251 | `	}` |
|         - | 4252 | `	/* Return the freshly created array */` |
|      1029 | 4253 | `	ph7_result_value(pCtx,pArray);` |
|      1029 | 4254 | `	return PH7_OK;` |
|       519 | 4255 | `}` |
|         - | 4256 | `/*` |
|         - | 4257 | ` * array array_copy(array $source)` |
|         - | 4258 | ` *  Make a blind copy of the target array.` |
|         - | 4259 | ` * Parameters` |
|         - | 4260 | ` *  $source` |
|         - | 4261 | ` *   Target array` |
|         - | 4262 | ` * Return` |
|         - | 4263 | ` *  Copy of the target array on success.NULL otherwise.` |
|         - | 4264 | ` * Note` |
|         - | 4265 | ` *  This function is a symisc eXtension.` |
|         - | 4266 | ` */` |
|        16 | 4267 | `static int ph7_hashmap_copy(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|         1 | 4268 | `{` |
|         - | 4269 | `	ph7_hashmap *pMap;` |
|         - | 4270 | `	ph7_value *pArray;` |
|        17 | 4271 | `	if( nArg < 1 ){` |
|         - | 4272 | `		/* Missing arguments,return NULL */` |
|       ! 0 | 4273 | `		ph7_result_null(pCtx);` |
|       ! 0 | 4274 | `		return PH7_OK;` |
|         - | 4275 | `	}` |
|         - | 4276 | `	/* Create a new array */` |
|        17 | 4277 | `	pArray = ph7_context_new_array(pCtx);` |
|        17 | 4278 | `	if( pArray == 0 ){` |
|       ! 0 | 4279 | `		ph7_result_null(pCtx);` |
|       ! 0 | 4280 | `		return PH7_OK;` |
|         - | 4281 | `	}` |
|         - | 4282 | `	/* Point to the internal representation of the hashmap */` |
|        17 | 4283 | `	pMap = (ph7_hashmap *)pArray->x.pOther;` |
|        17 | 4284 | `	if( ph7_value_is_array(apArg[0])){` |
|         - | 4285 | `		/* Point to the internal representation of the source */` |
|        17 | 4286 | `		ph7_hashmap *pSrc = (ph7_hashmap *)apArg[0]->x.pOther;` |
|         - | 4287 | `		/* Perform the copy */` |
|        17 | 4288 | `		PH7_HashmapDup(pSrc,pMap);` |
|         9 | 4289 | `	}else{` |
|         - | 4290 | `		/* Simple insertion */` |
|       ! 0 | 4291 | `		PH7_HashmapInsert(pMap,0/* Automatic index assign*/,apArg[0]);` |
|         - | 4292 | `	}` |
|         - | 4293 | `	/* Return the duplicated array */` |
|        17 | 4294 | `	ph7_result_value(pCtx,pArray);` |
|        17 | 4295 | `	return PH7_OK;` |
|         9 | 4296 | `}` |
|         - | 4297 | `/*` |
|         - | 4298 | ` * bool array_erase(array $source)` |
|         - | 4299 | ` *  Remove all elements from a given array.` |
|         - | 4300 | ` * Parameters` |
|         - | 4301 | ` *  $source` |
|         - | 4302 | ` *   Target array` |
|         - | 4303 | ` * Return` |
|         - | 4304 | ` *  TRUE on success.FALSE otherwise.` |
|         - | 4305 | ` * Note` |
|         - | 4306 | ` *  This function is a symisc eXtension.` |
|         - | 4307 | ` */` |
|        16 | 4308 | `static int ph7_hashmap_erase(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|         1 | 4309 | `{` |
|         - | 4310 | `	ph7_hashmap *pMap;` |
|        17 | 4311 | `	if( nArg < 1 ){` |
|         - | 4312 | `		/* Missing arguments */` |
|       ! 0 | 4313 | `		ph7_result_bool(pCtx,0);` |
|       ! 0 | 4314 | `		return PH7_OK;` |
|         - | 4315 | `	}` |
|         - | 4316 | `	/* Point to the target hashmap */` |
|        17 | 4317 | `	PH7_HashmapCowSeparate(pCtx->pVm, apArg[0]);` |
|        17 | 4318 | `	pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|         - | 4319 | `	/* Erase */` |
|        17 | 4320 | `	PH7_HashmapRelease(pMap,FALSE);` |
|        17 | 4321 | `	return PH7_OK;` |
|         9 | 4322 | `}` |
|         - | 4323 | `/*` |
|         - | 4324 | ` * array array_slice(array $array, int $offset [, ?int $length = null [, bool $preserve_keys = false ]])` |
|         - | 4325 | ` *  Extract a slice of the array.` |
|         - | 4326 | ` * Parameters` |
|         - | 4327 | ` *  $array` |
|         - | 4328 | ` *    The input array.` |
|         - | 4329 | ` * $offset` |
|         - | 4330 | ` *    If offset is non-negative, the sequence will start at that offset in the array.` |
|         - | 4331 | ` *    If offset is negative, the sequence will start that far from the end of the array.` |
|         - | 4332 | ` * $length (optional, nullable)` |
|         - | 4333 | ` *    If length is given and is positive, then the sequence will have that many elements` |
|         - | 4334 | ` *    in it. If length is given and is negative then the sequence will stop that many` |
|         - | 4335 | ` *    elements from the end of the array. If it is omitted or NULL, then the sequence` |
|         - | 4336 | ` *    will have everything from offset up until the end of the array.` |
|         - | 4337 | ` * $preserve_keys (optional)` |
|         - | 4338 | ` *    Note that array_slice() will reorder and reset the array indices by default.` |
|         - | 4339 | ` *    You can change this behaviour by setting preserve_keys to TRUE.` |
|         - | 4340 | ` * Return` |
|         - | 4341 | ` *   The new slice.` |
|         - | 4342 | ` */` |
|        50 | 4343 | `static int ph7_hashmap_slice(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|         5 | 4344 | `{` |
|         - | 4345 | `	ph7_hashmap *pMap,*pSrc;` |
|         - | 4346 | `	ph7_hashmap_node *pCur;` |
|         - | 4347 | `	ph7_value *pArray;` |
|         - | 4348 | `	int iLength,iOfft;` |
|         - | 4349 | `	int bPreserve;` |
|         - | 4350 | `	sxi32 rc;` |
|        55 | 4351 | `	if( nArg < 2 ){` |
|         8 | 4352 | `		return PH7_VmThrowException(pCtx,` |
|         - | 4353 | `			"ArgumentCountError",` |
|         - | 4354 | `			"array_slice() expects at least 2 arguments, %d given",` |
|         2 | 4355 | `			nArg` |
|         - | 4356 | `			);` |
|         - | 4357 | `	}` |
|        51 | 4358 | `	if( nArg > 4 ){` |
|         4 | 4359 | `		return PH7_VmThrowException(pCtx,` |
|         - | 4360 | `			"ArgumentCountError",` |
|         - | 4361 | `			"array_slice() expects at most 4 arguments, %d given",` |
|         1 | 4362 | `			nArg` |
|         - | 4363 | `			);` |
|         - | 4364 | `	}` |
|        49 | 4365 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|         4 | 4366 | `		return PH7_VmThrowException(pCtx,` |
|         - | 4367 | `			"TypeError",` |
|         - | 4368 | `			"array_slice(): Argument #1 ($array) must be of type array, %s given",` |
|         1 | 4369 | `			ph7_type_name(apArg[0])` |
|         - | 4370 | `			);` |
|         - | 4371 | `	}` |
|         - | 4372 | `	/* Validate $offset type: reject string, array, object, resource */` |
|        62 | 4373 | `	if( ph7_value_is_string(apArg[1]) \|\| ph7_value_is_array(apArg[1]) \|\|` |
|        65 | 4374 | `		ph7_value_is_object(apArg[1]) \|\| ph7_value_is_resource(apArg[1]) ){` |
|         4 | 4375 | `		return PH7_VmThrowException(pCtx,` |
|         - | 4376 | `			"TypeError",` |
|         - | 4377 | `			"array_slice(): Argument #2 ($offset) must be of type int, %s given",` |
|         2 | 4378 | `			ph7_type_name(apArg[1])` |
|         - | 4379 | `			);` |
|         - | 4380 | `	}` |
|         - | 4381 | `	/* Validate $length type if provided: nullable int */` |
|        45 | 4382 | `	if( nArg > 2 && !ph7_value_is_null(apArg[2]) ){` |
|        26 | 4383 | `		if( ph7_value_is_string(apArg[2]) \|\| ph7_value_is_array(apArg[2]) \|\|` |
|        26 | 4384 | `			ph7_value_is_object(apArg[2]) \|\| ph7_value_is_resource(apArg[2]) ){` |
|         4 | 4385 | `			return PH7_VmThrowException(pCtx,` |
|         - | 4386 | `				"TypeError",` |
|         - | 4387 | `				"array_slice(): Argument #3 ($length) must be of type ?int, %s given",` |
|         2 | 4388 | `				ph7_type_name(apArg[2])` |
|         - | 4389 | `				);` |
|         - | 4390 | `		}` |
|         8 | 4391 | `	}` |
|         - | 4392 | `	/* Validate $preserve_keys type if provided: reject array, object, resource */` |
|        43 | 4393 | `	if( nArg > 3 ){` |
|        10 | 4394 | `		if( ph7_value_is_array(apArg[3]) \|\| ph7_value_is_object(apArg[3]) \|\|` |
|         4 | 4395 | `			ph7_value_is_resource(apArg[3]) ){` |
|         4 | 4396 | `			return PH7_VmThrowException(pCtx,` |
|         - | 4397 | `				"TypeError",` |
|         - | 4398 | `				"array_slice(): Argument #4 ($preserve_keys) must be of type bool, %s given",` |
|         2 | 4399 | `				ph7_type_name(apArg[3])` |
|         - | 4400 | `				);` |
|         - | 4401 | `		}` |
|         2 | 4402 | `	}` |
|         - | 4403 | `	/* Point the internal representation of the target array */` |
|        41 | 4404 | `	pSrc = (ph7_hashmap *)apArg[0]->x.pOther;` |
|        41 | 4405 | `	bPreserve = FALSE;` |
|         - | 4406 | `	/* Get the offset */` |
|        41 | 4407 | `	iOfft = ph7_value_to_int(apArg[1]);` |
|        41 | 4408 | `	if( iOfft < 0 ){` |
|         5 | 4409 | `		iOfft = (int)pSrc->nEntry + iOfft;` |
|         5 | 4410 | `		if( iOfft < 0 ){` |
|         3 | 4411 | `			iOfft = 0;` |
|         1 | 4412 | `		}` |
|         2 | 4413 | `	}` |
|        41 | 4414 | `	if( iOfft >= (int)pSrc->nEntry ){` |
|         - | 4415 | `		/* Offset past end of array, return empty array */` |
|         5 | 4416 | `		pArray = ph7_context_new_array(pCtx);` |
|         5 | 4417 | `		if( pArray == 0 ){` |
|       ! 0 | 4418 | `			ph7_result_null(pCtx);` |
|       ! 0 | 4419 | `			return PH7_OK;` |
|         - | 4420 | `		}` |
|         5 | 4421 | `		ph7_result_value(pCtx,pArray);` |
|         5 | 4422 | `		return PH7_OK;` |
|         - | 4423 | `	}` |
|         - | 4424 | `	/* Get the length: NULL means "all remaining" (same as omitting) */` |
|        37 | 4425 | `	iLength = (int)pSrc->nEntry - iOfft;` |
|        37 | 4426 | `	if( nArg > 2 && !ph7_value_is_null(apArg[2]) ){` |
|        15 | 4427 | `		iLength = ph7_value_to_int(apArg[2]);` |
|        15 | 4428 | `		if( iLength < 0 ){` |
|         5 | 4429 | `			iLength = ((int)pSrc->nEntry + iLength) - iOfft;` |
|         2 | 4430 | `		}` |
|        15 | 4431 | `		if( iLength < 0 ){` |
|         3 | 4432 | `			iLength = 0;` |
|         1 | 4433 | `		}` |
|        15 | 4434 | `		if( iOfft + iLength > (int)pSrc->nEntry ){` |
|         3 | 4435 | `			iLength = (int)pSrc->nEntry - iOfft;` |
|         1 | 4436 | `		}` |
|         7 | 4437 | `	}` |
|        37 | 4438 | `	if( nArg > 3 ){` |
|         5 | 4439 | `		bPreserve = ph7_value_to_bool(apArg[3]);` |
|         2 | 4440 | `	}` |
|         - | 4441 | `	/* Create a new array */` |
|        37 | 4442 | `	pArray = ph7_context_new_array(pCtx);` |
|        37 | 4443 | `	if( pArray == 0 ){` |
|       ! 0 | 4444 | `		ph7_result_null(pCtx);` |
|       ! 0 | 4445 | `		return PH7_OK;` |
|         - | 4446 | `	}` |
|        37 | 4447 | `	if( iLength < 1 ){` |
|         - | 4448 | `		/* Don't bother processing,return the empty array */` |
|         5 | 4449 | `		ph7_result_value(pCtx,pArray);` |
|         5 | 4450 | `		return PH7_OK;` |
|         - | 4451 | `	}` |
|         - | 4452 | `	/* Point to the desired entry */` |
|        33 | 4453 | `	pCur = pSrc->pFirst;` |
|        28 | 4454 | `	for(;;){` |
|        61 | 4455 | `		if( iOfft < 1 ){` |
|        33 | 4456 | `			break;` |
|         - | 4457 | `		}` |
|         - | 4458 | `		/* Point to the next entry */` |
|        33 | 4459 | `		pCur = pCur->pPrev; /* Reverse link */` |
|        33 | 4460 | `		iOfft--;` |
|         5 | 4461 | `	}` |
|         - | 4462 | `	/* Point to the internal representation of the hashmap */` |
|        33 | 4463 | `	pMap = (ph7_hashmap *)pArray->x.pOther;` |
|        51 | 4464 | `	for(;;){` |
|       107 | 4465 | `		if( iLength < 1 ){` |
|        33 | 4466 | `			break;` |
|         - | 4467 | `		}` |
|         - | 4468 | `		/* String keys are always preserved; preserve_keys only affects int keys */` |
|         - | 4469 | `		{` |
|        79 | 4470 | `			int bKeep = (pCur->iType == HASHMAP_INT_NODE) ? bPreserve : TRUE;` |
|        79 | 4471 | `			rc = HashmapInsertNode(pMap,pCur,bKeep);` |
|         - | 4472 | `		}` |
|        79 | 4473 | `		if( rc != SXRET_OK ){` |
|       ! 0 | 4474 | `			break;` |
|         - | 4475 | `		}` |
|         - | 4476 | `		/* Point to the next entry */` |
|        79 | 4477 | `		pCur = pCur->pPrev; /* Reverse link */` |
|        79 | 4478 | `		iLength--;` |
|         5 | 4479 | `	}` |
|         - | 4480 | `	/* Return the freshly created array */` |
|        33 | 4481 | `	ph7_result_value(pCtx,pArray);` |
|        33 | 4482 | `	return PH7_OK;` |
|        30 | 4483 | `}` |
|         - | 4484 | `/*` |
|         - | 4485 | ` * Move the last node in the hashmap linked list to immediately after pAfter` |
|         - | 4486 | ` * in iteration order.  If pAfter is NULL the node is moved to the very` |
|         - | 4487 | ` * beginning (becomes the new pFirst).` |
|         - | 4488 | ` */` |
|        30 | 4489 | `static void HashmapMoveLastAfter(ph7_hashmap *pMap,ph7_hashmap_node *pAfter)` |
|         1 | 4490 | `{` |
|         - | 4491 | `	ph7_hashmap_node *pNode;` |
|         - | 4492 | `	ph7_hashmap_node *pOldNext;` |
|        31 | 4493 | `	pNode = pMap->pLast;` |
|        31 | 4494 | `	if( pNode == 0 ){` |
|       ! 0 | 4495 | `		return;` |
|         - | 4496 | `	}` |
|        31 | 4497 | `	if( pNode->pNext == 0 ){` |
|         - | 4498 | `		/* Only node in the list, nothing to move */` |
|         5 | 4499 | `		return;` |
|         - | 4500 | `	}` |
|        27 | 4501 | `	if( pAfter != 0 && pAfter->pPrev == pNode ){` |
|         - | 4502 | `		/* Already in the correct position */` |
|         9 | 4503 | `		return;` |
|         - | 4504 | `	}` |
|         - | 4505 | `	/* Unlink pNode from the end of the list */` |
|        19 | 4506 | `	pMap->pLast = pNode->pNext;` |
|        19 | 4507 | `	pMap->pLast->pPrev = 0;` |
|         - | 4508 | `	/* Insert pNode after pAfter in iteration order */` |
|        19 | 4509 | `	if( pAfter == 0 ){` |
|         - | 4510 | `		/* Insert at the very beginning, before pFirst */` |
|         3 | 4511 | `		pNode->pNext = 0;` |
|         3 | 4512 | `		pNode->pPrev = pMap->pFirst;` |
|         3 | 4513 | `		if( pMap->pFirst ){` |
|         3 | 4514 | `			pMap->pFirst->pNext = pNode;` |
|         1 | 4515 | `		}` |
|         3 | 4516 | `		pMap->pFirst = pNode;` |
|         2 | 4517 | `	}else{` |
|        17 | 4518 | `		pOldNext = pAfter->pPrev;` |
|        17 | 4519 | `		pNode->pPrev = pOldNext;` |
|        17 | 4520 | `		pNode->pNext = pAfter;` |
|        17 | 4521 | `		pAfter->pPrev = pNode;` |
|        17 | 4522 | `		if( pOldNext ){` |
|        17 | 4523 | `			pOldNext->pNext = pNode;` |
|         9 | 4524 | `		}else{` |
|       ! 0 | 4525 | `			pMap->pLast = pNode;` |
|         - | 4526 | `		}` |
|         - | 4527 | `	}` |
|        16 | 4528 | `}` |
|         - | 4529 | `/*` |
|         - | 4530 | ` * array array_splice(array $array, int $offset [, int $length [, value $replacement]])` |
|         - | 4531 | ` *  Remove a portion of the array and replace it with something else.` |
|         - | 4532 | ` * Parameters` |
|         - | 4533 | ` *  $array` |
|         - | 4534 | ` *    The input array.` |
|         - | 4535 | ` *  $offset` |
|         - | 4536 | ` *    If offset is positive then the start of removed portion is at that offset` |
|         - | 4537 | ` *    from the beginning of the input array.  If offset is negative then it` |
|         - | 4538 | ` *    starts that far from the end of the input array.  If the absolute value of` |
|         - | 4539 | ` *    a negative offset exceeds the array length, offset is clamped to 0.  If a` |
|         - | 4540 | ` *    positive offset exceeds the array length, offset is clamped to the array` |
|         - | 4541 | ` *    length (i.e. nothing is removed, but replacement is appended).` |
|         - | 4542 | ` *  $length (optional)` |
|         - | 4543 | ` *    If length is omitted, removes everything from offset to the end of the` |
|         - | 4544 | ` *    array.  If length is specified and is positive, then that many elements` |
|         - | 4545 | ` *    will be removed.  If length is specified and is negative then the end of` |
|         - | 4546 | ` *    the removed portion will be that many elements from the end of the array.` |
|         - | 4547 | ` *    If the resulting length is negative it is clamped to 0.` |
|         - | 4548 | ` *  $replacement (optional)` |
|         - | 4549 | ` *    If replacement array is specified, then the removed elements are replaced` |
|         - | 4550 | ` *    with elements from this array.` |
|         - | 4551 | ` *    If offset and length are such that nothing is removed, then the elements` |
|         - | 4552 | ` *    from the replacement array are inserted in the place specified by the` |
|         - | 4553 | ` *    offset.` |
|         - | 4554 | ` *    Note that keys in replacement array are not preserved.` |
|         - | 4555 | ` *    If replacement is just one element it is not necessary to put array()` |
|         - | 4556 | ` *    around it, unless the element is an array itself, an object or NULL.` |
|         - | 4557 | ` * Return` |
|         - | 4558 | ` *   A new array consisting of the extracted elements.` |
|         - | 4559 | ` */` |
|        54 | 4560 | `static int ph7_hashmap_splice(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|         4 | 4561 | `{` |
|         - | 4562 | `	ph7_hashmap_node *pCur,*pPrev,*pRnode,*pInsertAfter,*pNewNode;` |
|         - | 4563 | `	ph7_value *pArray,*pRvalue;` |
|         - | 4564 | `	ph7_hashmap *pMap,*pSrc,*pRep;` |
|         - | 4565 | `	int iLength,iOfft,i;` |
|         - | 4566 | `	sxi32 rc;` |
|        58 | 4567 | `	if( nArg < 2 ){` |
|         8 | 4568 | `		return PH7_VmThrowException(pCtx,` |
|         - | 4569 | `			"ArgumentCountError",` |
|         - | 4570 | `			"array_splice() expects at least 2 arguments, %d given",` |
|         2 | 4571 | `			nArg` |
|         - | 4572 | `			);` |
|         - | 4573 | `	}` |
|        52 | 4574 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|         4 | 4575 | `		return PH7_VmThrowException(pCtx,` |
|         - | 4576 | `			"TypeError",` |
|         - | 4577 | `			"array_splice(): Argument #1 ($array) must be of type array, %s given",` |
|         1 | 4578 | `			ph7_type_name(apArg[0])` |
|         - | 4579 | `			);` |
|         - | 4580 | `	}` |
|         - | 4581 | `	/* Point to the internal representation of the target array */` |
|        49 | 4582 | `	PH7_HashmapCowSeparate(pCtx->pVm, apArg[0]);` |
|        49 | 4583 | `	pSrc = (ph7_hashmap *)apArg[0]->x.pOther;` |
|         - | 4584 | `	/* Get the offset and clamp to valid range */` |
|        49 | 4585 | `	iOfft = ph7_value_to_int(apArg[1]);` |
|        49 | 4586 | `	if( iOfft < 0 ){` |
|         7 | 4587 | `		iOfft = (int)pSrc->nEntry + iOfft;` |
|         7 | 4588 | `		if( iOfft < 0 ){` |
|         3 | 4589 | `			iOfft = 0;` |
|         2 | 4590 | `		}` |
|        46 | 4591 | `	}else if( iOfft > (int)pSrc->nEntry ){` |
|         3 | 4592 | `		iOfft = (int)pSrc->nEntry;` |
|         1 | 4593 | `	}` |
|         - | 4594 | `	/* Get the length and clamp to valid range.` |
|         - | 4595 | `	 * NULL means "all remaining" (same as omitting the argument). */` |
|        49 | 4596 | `	iLength = (int)pSrc->nEntry - iOfft;` |
|        49 | 4597 | `	if( nArg > 2 && !ph7_value_is_null(apArg[2]) ){` |
|        31 | 4598 | `		iLength = ph7_value_to_int(apArg[2]);` |
|        31 | 4599 | `		if( iLength < 0 ){` |
|         7 | 4600 | `			iLength = ((int)pSrc->nEntry + iLength) - iOfft;` |
|         7 | 4601 | `			if( iLength < 0 ){` |
|         3 | 4602 | `				iLength = 0;` |
|         1 | 4603 | `			}` |
|         3 | 4604 | `		}` |
|        31 | 4605 | `		if( iOfft + iLength > (int)pSrc->nEntry ){` |
|         3 | 4606 | `			iLength = (int)pSrc->nEntry - iOfft;` |
|         1 | 4607 | `		}` |
|        15 | 4608 | `	}` |
|         - | 4609 | `	/* Create the result array for removed elements */` |
|        49 | 4610 | `	pArray = ph7_context_new_array(pCtx);` |
|        49 | 4611 | `	if( pArray == 0 ){` |
|       ! 0 | 4612 | `		ph7_result_null(pCtx);` |
|       ! 0 | 4613 | `		return PH7_OK;` |
|         - | 4614 | `	}` |
|         - | 4615 | `	/* Get replacement array if provided */` |
|        49 | 4616 | `	pRep = 0;` |
|        49 | 4617 | `	if( nArg > 3 ){` |
|        21 | 4618 | `		if( !ph7_value_is_array(apArg[3]) ){` |
|         - | 4619 | `			/* Perform an array cast */` |
|         3 | 4620 | `			PH7_MemObjToHashmap(apArg[3]);` |
|         3 | 4621 | `			if( ph7_value_is_array(apArg[3]) ){` |
|         3 | 4622 | `				pRep = (ph7_hashmap *)apArg[3]->x.pOther;` |
|         1 | 4623 | `			}` |
|         2 | 4624 | `		}else{` |
|        19 | 4625 | `			pRep = (ph7_hashmap *)apArg[3]->x.pOther;` |
|         - | 4626 | `		}` |
|        21 | 4627 | `		if( pRep ){` |
|         - | 4628 | `			/* Reset the loop cursor */` |
|        21 | 4629 | `			pRep->pCur = pRep->pFirst;` |
|        10 | 4630 | `		}` |
|        10 | 4631 | `	}` |
|         - | 4632 | `	/* Early return if nothing to remove and no replacement */` |
|        49 | 4633 | `	if( iLength < 1 && pRep == 0 ){` |
|         9 | 4634 | `		ph7_result_value(pCtx,pArray);` |
|         9 | 4635 | `		return PH7_OK;` |
|         - | 4636 | `	}` |
|         - | 4637 | `	/* Navigate to the offset position */` |
|        41 | 4638 | `	pCur = pSrc->pFirst;` |
|        85 | 4639 | `	for( i = 0 ; i < iOfft && pCur ; i++ ){` |
|        45 | 4640 | `		pCur = pCur->pPrev; /* Reverse link */` |
|        23 | 4641 | `	}` |
|         - | 4642 | `	/* Save the node just before the splice range as the insertion anchor.` |
|         - | 4643 | `	 * pCur->pNext is the backward link (previous node in iteration order).` |
|         - | 4644 | `	 * If pCur is NULL (offset == nEntry), the anchor is the last node. */` |
|        41 | 4645 | `	pInsertAfter = (pCur != 0) ? pCur->pNext : pSrc->pLast;` |
|         - | 4646 | `	/* Remove nodes in the splice range and copy them to the result array */` |
|        41 | 4647 | `	pMap = (ph7_hashmap *)pArray->x.pOther;` |
|       111 | 4648 | `	for( i = 0 ; i < iLength && pCur ; i++ ){` |
|        71 | 4649 | `		pPrev = pCur->pPrev;` |
|        71 | 4650 | `		rc = HashmapInsertNode(pMap,pCur,FALSE);` |
|        71 | 4651 | `		PH7_HashmapUnlinkNode(pCur,TRUE);` |
|        71 | 4652 | `		if( rc != SXRET_OK ){` |
|       ! 0 | 4653 | `			break;` |
|         - | 4654 | `		}` |
|        71 | 4655 | `		pCur = pPrev; /* Reverse link */` |
|        36 | 4656 | `	}` |
|         - | 4657 | `	/* Insert replacement elements at the correct position */` |
|        41 | 4658 | `	if( pRep ){` |
|         - | 4659 | `		ph7_value sSafeVal;` |
|        61 | 4660 | `		while( (pRnode = PH7_HashmapGetNextEntry(pRep)) != 0 ){` |
|        31 | 4661 | `			pRvalue = HashmapExtractNodeValue(pRnode);` |
|        31 | 4662 | `			if( pRvalue ){` |
|         - | 4663 | `				/* Make a stack copy before inserting.  HashmapInsert() may` |
|         - | 4664 | `				 * grow the VM memobj pool, which would invalidate pRvalue` |
|         - | 4665 | `				 * since it points into that same pool. */` |
|        31 | 4666 | `				sSafeVal = *pRvalue;` |
|        31 | 4667 | `				rc = HashmapInsert(pSrc,0,&sSafeVal);` |
|        31 | 4668 | `				if( rc == SXRET_OK && pSrc->pLast != 0 ){` |
|        31 | 4669 | `					pNewNode = pSrc->pLast;` |
|        31 | 4670 | `					HashmapMoveLastAfter(pSrc,pInsertAfter);` |
|        31 | 4671 | `					pInsertAfter = pNewNode;` |
|        15 | 4672 | `				}` |
|        15 | 4673 | `			}` |
|         1 | 4674 | `		}` |
|        10 | 4675 | `	}` |
|         - | 4676 | `	/* Return the freshly created array */` |
|        41 | 4677 | `	ph7_result_value(pCtx,pArray);` |
|        41 | 4678 | `	return PH7_OK;` |
|        31 | 4679 | `}` |
|         - | 4680 | `/*` |
|         - | 4681 | ` * bool in_array(value $needle,array $haystack[,bool $strict = FALSE ])` |
|         - | 4682 | ` *  Checks if a value exists in an array.` |
|         - | 4683 | ` * Parameters` |
|         - | 4684 | ` *  $needle` |
|         - | 4685 | ` *   The searched value.` |
|         - | 4686 | ` *   Note:` |
|         - | 4687 | ` *    If needle is a string, the comparison is done in a case-sensitive manner.` |
|         - | 4688 | ` * $haystack` |
|         - | 4689 | ` *  The target array.` |
|         - | 4690 | ` * $strict` |
|         - | 4691 | ` *  If the third parameter strict is set to TRUE then the in_array() function` |
|         - | 4692 | ` *  will also check the types of the needle in the haystack.` |
|         - | 4693 | ` */` |
|     32228 | 4694 | `static int ph7_hashmap_in_array(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|         5 | 4695 | `{` |
|         - | 4696 | `	ph7_value *pNeedle;` |
|         - | 4697 | `	int bStrict;` |
|         - | 4698 | `	int rc;` |
|     32233 | 4699 | `	if( nArg < 2 ){` |
|         - | 4700 | `		/* Missing argument,return FALSE */` |
|       ! 0 | 4701 | `		ph7_result_bool(pCtx,0);` |
|       ! 0 | 4702 | `		return PH7_OK;` |
|         - | 4703 | `	}` |
|     32233 | 4704 | `	pNeedle = apArg[0];` |
|     32233 | 4705 | `	bStrict = 0;` |
|     32233 | 4706 | `	if( nArg > 2 ){` |
|        19 | 4707 | `		bStrict = ph7_value_to_bool(apArg[2]);` |
|         9 | 4708 | `	}` |
|     32233 | 4709 | `	if( !ph7_value_is_array(apArg[1]) ){` |
|         - | 4710 | `		/* haystack must be an array,perform a standard comparison */` |
|       ! 0 | 4711 | `		rc = ph7_value_compare(pNeedle,apArg[1],bStrict);` |
|         - | 4712 | `		/* Set the comparison result */` |
|       ! 0 | 4713 | `		ph7_result_bool(pCtx,rc == 0);` |
|       ! 0 | 4714 | `		return PH7_OK;` |
|         - | 4715 | `	}` |
|         - | 4716 | `	/* Perform the lookup */` |
|     32233 | 4717 | `	rc = HashmapFindValue((ph7_hashmap *)apArg[1]->x.pOther,pNeedle,0,bStrict);` |
|         - | 4718 | `	/* Lookup result */` |
|     32233 | 4719 | `	ph7_result_bool(pCtx,rc == SXRET_OK);` |
|     32233 | 4720 | `	return PH7_OK;` |
|     16119 | 4721 | `}` |
|         - | 4722 | `/*` |
|         - | 4723 | ` * value array_search(value $needle,array $haystack[,bool $strict = false ])` |
|         - | 4724 | ` *  Searches the array for a given value and returns the corresponding key if successful.` |
|         - | 4725 | ` * Parameters` |
|         - | 4726 | ` * $needle` |
|         - | 4727 | ` *   The searched value.` |
|         - | 4728 | ` * $haystack` |
|         - | 4729 | ` *   The array.` |
|         - | 4730 | ` * $strict` |
|         - | 4731 | ` *  If the third parameter strict is set to TRUE then the array_search() function` |
|         - | 4732 | ` *  will search for identical elements in the haystack. This means it will also check` |
|         - | 4733 | ` *  the types of the needle in the haystack, and objects must be the same instance.` |
|         - | 4734 | ` * Return` |
|         - | 4735 | ` *  Returns the key for needle if it is found in the array, FALSE otherwise.` |
|         - | 4736 | ` */` |
|        28 | 4737 | `static int ph7_hashmap_search(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|         5 | 4738 | `{` |
|         - | 4739 | `	ph7_hashmap_node *pEntry;` |
|         - | 4740 | `	ph7_value *pVal,sNeedle;` |
|         - | 4741 | `	ph7_hashmap *pMap;` |
|         - | 4742 | `	ph7_value sVal;` |
|         - | 4743 | `	int bStrict;` |
|         - | 4744 | `	sxu32 n;` |
|         - | 4745 | `	int rc;` |
|        33 | 4746 | `	if( nArg < 2 ){` |
|         - | 4747 | `		/* Missing argument,throw ArgumentCountError */` |
|         8 | 4748 | `		return PH7_VmThrowException(pCtx,` |
|         - | 4749 | `			"ArgumentCountError",` |
|         - | 4750 | `			"array_search() expects at least 2 arguments, %d given",` |
|         2 | 4751 | `			nArg` |
|         - | 4752 | `			);` |
|         - | 4753 | `	}` |
|        27 | 4754 | `	bStrict = FALSE;` |
|        27 | 4755 | `	if( !ph7_value_is_array(apArg[1]) ){` |
|         - | 4756 | `		/* haystack must be an array,throw TypeError */` |
|         4 | 4757 | `		return PH7_VmThrowException(pCtx,` |
|         - | 4758 | `			"TypeError",` |
|         - | 4759 | `			"array_search(): Argument #2 ($haystack) must be of type array, %s given",` |
|         2 | 4760 | `			ph7_type_name(apArg[1])` |
|         - | 4761 | `			);` |
|         - | 4762 | `	}` |
|        24 | 4763 | `	if( nArg > 2 ){` |
|         - | 4764 | `		/* In PHP, non-scalar values for a bool-hinted parameter raise TypeError */` |
|        12 | 4765 | `		if( ph7_value_is_array(apArg[2]) \|\| ph7_value_is_object(apArg[2]) \|\| ph7_value_is_resource(apArg[2]) ){` |
|         4 | 4766 | `			return PH7_VmThrowException(pCtx,` |
|         - | 4767 | `				"TypeError",` |
|         - | 4768 | `				"array_search(): Argument #3 ($strict) must be of type bool, %s given",` |
|         2 | 4769 | `				ph7_type_name(apArg[2])` |
|         - | 4770 | `				);` |
|         - | 4771 | `		}` |
|         9 | 4772 | `		bStrict = ph7_value_to_bool(apArg[2]);` |
|         4 | 4773 | `	}` |
|         - | 4774 | `	/* Point to the internal representation of the internal hashmap */` |
|        21 | 4775 | `	pMap = (ph7_hashmap *)apArg[1]->x.pOther;` |
|         - | 4776 | `	/* Perform a linear search since we cannot sort the hashmap based on values */` |
|        21 | 4777 | `	PH7_MemObjInit(pMap->pVm,&sVal);` |
|        21 | 4778 | `	PH7_MemObjInit(pMap->pVm,&sNeedle);` |
|        21 | 4779 | `	pEntry = pMap->pFirst;` |
|        21 | 4780 | `	n = pMap->nEntry;` |
|        23 | 4781 | `	for(;;){` |
|        47 | 4782 | `		if( !n ){` |
|         9 | 4783 | `			break;` |
|         - | 4784 | `		}` |
|         - | 4785 | `		/* Extract node value */` |
|        39 | 4786 | `		pVal = HashmapExtractNodeValue(pEntry);` |
|        39 | 4787 | `		if( pVal ){` |
|         - | 4788 | `			/* Make a copy of the vuurent values since the comparison routine` |
|         - | 4789 | `			 * can change their type.` |
|         - | 4790 | `			 */` |
|        39 | 4791 | `			PH7_MemObjLoad(pVal,&sVal);` |
|        39 | 4792 | `			PH7_MemObjLoad(apArg[0],&sNeedle);` |
|        39 | 4793 | `			rc = PH7_MemObjCmp(&sNeedle,&sVal,bStrict,0);` |
|        39 | 4794 | `			PH7_MemObjRelease(&sVal);` |
|        39 | 4795 | `			PH7_MemObjRelease(&sNeedle);` |
|        39 | 4796 | `			if( rc == 0 ){` |
|         - | 4797 | `				/* Match found,return key */` |
|        13 | 4798 | `				if( pEntry->iType == HASHMAP_INT_NODE){` |
|         - | 4799 | `					/* INT key */` |
|         7 | 4800 | `					ph7_result_int64(pCtx,pEntry->xKey.iKey);` |
|         4 | 4801 | `				}else{` |
|         7 | 4802 | `					SyBlob *pKey = &pEntry->xKey.sKey;` |
|         - | 4803 | `					/* Blob key */` |
|         7 | 4804 | `					ph7_result_string(pCtx,(const char *)SyBlobData(pKey),(int)SyBlobLength(pKey));` |
|         - | 4805 | `				}` |
|        13 | 4806 | `				return PH7_OK;` |
|         - | 4807 | `			}` |
|        13 | 4808 | `		}` |
|         - | 4809 | `		/* Point to the next entry */` |
|        27 | 4810 | `		pEntry = pEntry->pPrev; /* Reverse link */` |
|        27 | 4811 | `		n--;` |
|         1 | 4812 | `	}` |
|         - | 4813 | `	/* No such value,return FALSE */` |
|         9 | 4814 | `	ph7_result_bool(pCtx,0);` |
|         9 | 4815 | `	return PH7_OK;` |
|        19 | 4816 | `}` |
|         - | 4817 | `/*` |
|         - | 4818 | ` * array array_diff(array $array1,array $array2,...)` |
|         - | 4819 | ` *  Computes the difference of arrays.` |
|         - | 4820 | ` * Parameters` |
|         - | 4821 | ` *  $array1` |
|         - | 4822 | ` *    The array to compare from` |
|         - | 4823 | ` *  $array2` |
|         - | 4824 | ` *    An array to compare against` |
|         - | 4825 | ` *  $...` |
|         - | 4826 | ` *   More arrays to compare against` |
|         - | 4827 | ` * Return` |
|         - | 4828 | ` *  Returns an array containing all the entries from array1 that` |
|         - | 4829 | ` *  are not present in any of the other arrays.` |
|         - | 4830 | ` */` |
|        22 | 4831 | `static int ph7_hashmap_diff(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|         4 | 4832 | `{` |
|         - | 4833 | `	ph7_hashmap_node *pEntry;` |
|         - | 4834 | `	ph7_hashmap *pSrc,*pMap;` |
|         - | 4835 | `	ph7_value *pArray;` |
|         - | 4836 | `	ph7_value *pVal;` |
|         - | 4837 | `	sxi32 rc;` |
|         - | 4838 | `	sxu32 n;` |
|         - | 4839 | `	int i;` |
|         - | 4840 | `	/* Validate arguments to mimic PHP behaviour. Earlier versions simply` |
|         - | 4841 | `	 * returned NULL when the caller passed invalid parameters which made` |
|         - | 4842 | `	 * debugging difficult. */` |
|        26 | 4843 | `	if( nArg < 1 ){` |
|         4 | 4844 | `		return PH7_VmThrowException(pCtx,` |
|         - | 4845 | `			"ArgumentCountError",` |
|         - | 4846 | `			"array_diff() expects at least 1 argument, %d given",` |
|         1 | 4847 | `			nArg` |
|         - | 4848 | `			);` |
|         - | 4849 | `	}` |
|        23 | 4850 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|         4 | 4851 | `		return PH7_VmThrowException(pCtx,` |
|         - | 4852 | `			"TypeError",` |
|         - | 4853 | `			"array_diff(): Argument #1 ($array) must be of type array, %s given",` |
|         1 | 4854 | `			ph7_type_name(apArg[0])` |
|         - | 4855 | `			);` |
|         - | 4856 | `	}` |
|        36 | 4857 | `	for(i = 1 ; i < nArg ; i++){` |
|        20 | 4858 | `		if( !ph7_value_is_array(apArg[i]) ){` |
|         4 | 4859 | `			return PH7_VmThrowException(pCtx,` |
|         - | 4860 | `				"TypeError",` |
|         - | 4861 | `				"array_diff(): Argument #%d must be of type array, %s given",` |
|         1 | 4862 | `				i + 1,` |
|         2 | 4863 | `				ph7_type_name(apArg[i])` |
|         - | 4864 | `				);` |
|         - | 4865 | `		}` |
|         9 | 4866 | `	}` |
|        17 | 4867 | `	if( nArg == 1 ){` |
|         - | 4868 | `		/* Return the first array since we cannot perform a diff */` |
|         3 | 4869 | `		ph7_result_value(pCtx,apArg[0]);` |
|         3 | 4870 | `		return PH7_OK;` |
|         - | 4871 | `	}` |
|         - | 4872 | `	/* Create a new array */` |
|        15 | 4873 | `	pArray = ph7_context_new_array(pCtx);` |
|        15 | 4874 | `	if( pArray == 0 ){` |
|       ! 0 | 4875 | `		ph7_result_null(pCtx);` |
|       ! 0 | 4876 | `		return PH7_OK;` |
|         - | 4877 | `	}` |
|         - | 4878 | `	/* Point to the internal representation of the source hashmap */` |
|        15 | 4879 | `	pSrc = (ph7_hashmap *)apArg[0]->x.pOther;` |
|         - | 4880 | `	/* Perform the diff */` |
|        15 | 4881 | `	pEntry = pSrc->pFirst;` |
|        15 | 4882 | `	n = pSrc->nEntry;` |
|        27 | 4883 | `	for(;;){` |
|        55 | 4884 | `		if( n < 1 ){` |
|        15 | 4885 | `			break;` |
|         - | 4886 | `		}` |
|         - | 4887 | `		/* Extract the node value */` |
|        41 | 4888 | `		pVal = HashmapExtractNodeValue(pEntry);` |
|        41 | 4889 | `		if( pVal ){` |
|        69 | 4890 | `			for( i = 1 ; i < nArg ; i++ ){` |
|         - | 4891 | `				/* Point to the internal representation of the hashmap */` |
|        45 | 4892 | `				pMap = (ph7_hashmap *)apArg[i]->x.pOther;` |
|         - | 4893 | `				/* Perform the lookup */` |
|        45 | 4894 | `				rc = HashmapFindValue(pMap,pVal,0,TRUE);` |
|        45 | 4895 | `				if( rc == SXRET_OK ){` |
|         - | 4896 | `					/* Value exist */` |
|        17 | 4897 | `					break;` |
|         - | 4898 | `				}` |
|        15 | 4899 | `			}` |
|        41 | 4900 | `			if( i >= nArg ){` |
|         - | 4901 | `				/* Perform the insertion */` |
|        25 | 4902 | `				HashmapInsertNode((ph7_hashmap *)pArray->x.pOther,pEntry,TRUE);` |
|        12 | 4903 | `			}` |
|        20 | 4904 | `		}` |
|         - | 4905 | `		/* Point to the next entry */` |
|        41 | 4906 | `		pEntry = pEntry->pPrev; /* Reverse link */` |
|        41 | 4907 | `		n--;` |
|         1 | 4908 | `	}` |
|         - | 4909 | `	/* Return the freshly created array */` |
|        15 | 4910 | `	ph7_result_value(pCtx,pArray);` |
|        15 | 4911 | `	return PH7_OK;` |
|        15 | 4912 | `}` |
|         - | 4913 | `/*` |
|         - | 4914 | ` * array array_udiff(array $array1,array $array2,...,$callback)` |
|         - | 4915 | ` *  Computes the difference of arrays by using a callback function for data comparison.` |
|         - | 4916 | ` * Parameters` |
|         - | 4917 | ` *  $array1` |
|         - | 4918 | ` *    The array to compare from` |
|         - | 4919 | ` *  $array2` |
|         - | 4920 | ` *    An array to compare against` |
|         - | 4921 | ` *  $...` |
|         - | 4922 | ` *   More arrays to compare against.` |
|         - | 4923 | ` * $callback` |
|         - | 4924 | ` *  The callback comparison function.` |
|         - | 4925 | ` *  The comparison function must return an integer less than, equal to, or greater than zero` |
|         - | 4926 | ` *  if the first argument is considered to be respectively less than, equal to, or greater` |
|         - | 4927 | ` *  than the second.` |
|         - | 4928 | ` *     int callback ( mixed $a, mixed $b )` |
|         - | 4929 | ` * Return` |
|         - | 4930 | ` *  Returns an array containing all the entries from array1 that` |
|         - | 4931 | ` *  are not present in any of the other arrays.` |
|         - | 4932 | ` */` |
|        22 | 4933 | `static int ph7_hashmap_udiff(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|         5 | 4934 | `{` |
|         - | 4935 | `	ph7_hashmap_node *pEntry;` |
|         - | 4936 | `	ph7_hashmap *pSrc,*pMap;` |
|         - | 4937 | `	ph7_value *pCallback;` |
|         - | 4938 | `	ph7_value *pArray;` |
|         - | 4939 | `	ph7_value *pVal;` |
|         - | 4940 | `	sxi32 rc;` |
|         - | 4941 | `	sxu32 n;` |
|         - | 4942 | `	int i;` |
|         - | 4943 |  |
|         - | 4944 | `	/* Ensure the argument count matches PHP behaviour. */` |
|        27 | 4945 | `	if( nArg < 2 ){` |
|         4 | 4946 | `		return PH7_VmThrowException(pCtx,` |
|         - | 4947 | `			"ArgumentCountError",` |
|         - | 4948 | `			"array_udiff() expects at least 2 arguments, %d given",` |
|         1 | 4949 | `			nArg` |
|         - | 4950 | `			);` |
|         - | 4951 | `	}` |
|        25 | 4952 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|         4 | 4953 | `		return PH7_VmThrowException(pCtx,` |
|         - | 4954 | `			"TypeError",` |
|         - | 4955 | `			"array_udiff(): Argument #1 ($array) must be of type array, %s given",` |
|         1 | 4956 | `			ph7_type_name(apArg[0])` |
|         - | 4957 | `			);` |
|         - | 4958 | `	}` |
|         - | 4959 |  |
|        23 | 4960 | `	if( nArg == 2 ){` |
|         - | 4961 | `		/* Only the original array and the callback were provided. */` |
|         - | 4962 | `		/* Nevertheless, we still validate the callback after verifying any` |
|         - | 4963 | `		 * intermediate array arguments to match PHP's left-to-right parameter` |
|         - | 4964 | `		 * validation order.` |
|         - | 4965 | `		 */` |
|         4 | 4966 | `	} else {` |
|         - | 4967 | `		/* Ensure intermediary arguments are arrays (matches PHP strict typing). */` |
|        27 | 4968 | `		for( i = 1 ; i < nArg - 1; i++ ){` |
|        19 | 4969 | `			if( !ph7_value_is_array(apArg[i]) ){` |
|        11 | 4970 | `				return PH7_VmThrowException(pCtx,` |
|         - | 4971 | `					"TypeError",` |
|         - | 4972 | `					"array_udiff(): Argument #%d must be of type array, %s given",` |
|         3 | 4973 | `					i + 1,` |
|         6 | 4974 | `					ph7_type_name(apArg[i])` |
|         - | 4975 | `					);` |
|         - | 4976 | `			}` |
|         7 | 4977 | `		}` |
|         - | 4978 | `	}` |
|         - | 4979 |  |
|         - | 4980 | `	/* Identify the callback (always expected as the last argument). */` |
|        16 | 4981 | `	pCallback = apArg[nArg - 1];` |
|         - | 4982 | `	/* Validate the callback to match PHP's error messages. */` |
|        16 | 4983 | `	if( !ph7_value_is_callable(pCallback) ){` |
|         9 | 4984 | `		if( ph7_value_is_array(pCallback) ){` |
|         4 | 4985 | `			return PH7_VmThrowException(pCtx,` |
|         - | 4986 | `				"TypeError",` |
|         - | 4987 | `				"array_udiff(): Argument #%d must be a valid callback, array callback must have exactly two members",` |
|         1 | 4988 | `				nArg` |
|         - | 4989 | `				);` |
|         - | 4990 | `		}` |
|         6 | 4991 | `		if( ph7_value_is_string(pCallback) ){` |
|         - | 4992 | `			int len;` |
|         3 | 4993 | `			const char *zName = ph7_value_to_string(pCallback, &len);` |
|         4 | 4994 | `			return PH7_VmThrowException(pCtx,` |
|         - | 4995 | `				"TypeError",` |
|         - | 4996 | `				"array_udiff(): Argument #%d must be a valid callback, function \"%s\" not found or invalid function name",` |
|         1 | 4997 | `				nArg,` |
|         1 | 4998 | `				zName` |
|         - | 4999 | `				);` |
|         - | 5000 | `		}` |
|         4 | 5001 | `		return PH7_VmThrowException(pCtx,` |
|         - | 5002 | `			"TypeError",` |
|         - | 5003 | `			"array_udiff(): Argument #%d must be a valid callback, no array or string given",` |
|         1 | 5004 | `			nArg` |
|         - | 5005 | `			);` |
|         - | 5006 | `	}` |
|         - | 5007 |  |
|         7 | 5008 | `	if( nArg == 2 ){` |
|         - | 5009 | `		/* Only the original array and the callback were provided. */` |
|         3 | 5010 | `		ph7_result_value(pCtx,apArg[0]);` |
|         3 | 5011 | `		return PH7_OK;` |
|         - | 5012 | `	}` |
|         - | 5013 |  |
|         - | 5014 | `	/* Create a new array */` |
|         5 | 5015 | `	pArray = ph7_context_new_array(pCtx);` |
|         5 | 5016 | `	if( pArray == 0 ){` |
|       ! 0 | 5017 | `		ph7_result_null(pCtx);` |
|       ! 0 | 5018 | `		return PH7_OK;` |
|         - | 5019 | `	}` |
|         - | 5020 | `	/* Point to the internal representation of the source hashmap */` |
|         5 | 5021 | `	pSrc = (ph7_hashmap *)apArg[0]->x.pOther;` |
|         - | 5022 | `	/* Perform the diff */` |
|         5 | 5023 | `	pEntry = pSrc->pFirst;` |
|         5 | 5024 | `	n = pSrc->nEntry;` |
|         5 | 5025 | `	pCtx->pVm->iCmpCallbackExc = 0;` |
|         5 | 5026 | `	for(;;){` |
|        11 | 5027 | `		if( n < 1 ){` |
|         3 | 5028 | `			break;` |
|         - | 5029 | `		}` |
|         - | 5030 | `		/* Extract the node value */` |
|         9 | 5031 | `		pVal = HashmapExtractNodeValue(pEntry);` |
|         9 | 5032 | `		if( pVal ){` |
|        15 | 5033 | `			for( i = 1 ; i < nArg - 1; i++ ){` |
|         - | 5034 | `				/* Point to the internal representation of the hashmap */` |
|         9 | 5035 | `				pMap = (ph7_hashmap *)apArg[i]->x.pOther;` |
|         - | 5036 | `				/* Perform the lookup */` |
|         9 | 5037 | `				rc = HashmapFindValueByCallback(pMap,pVal,pCallback,0);` |
|         9 | 5038 | `				if( rc == SXRET_OK ){` |
|         - | 5039 | `					/* Value exist */` |
|         3 | 5040 | `					break;` |
|         - | 5041 | `				}` |
|         4 | 5042 | `			}` |
|         9 | 5043 | `			if( pCtx->pVm->iCmpCallbackExc ){` |
|         - | 5044 | `				/* The comparison callback raised: propagate so the dispatcher` |
|         - | 5045 | `				 * unwinds, before any spurious insertion into the result. */` |
|         3 | 5046 | `				pCtx->pVm->iCmpCallbackExc = 0;` |
|         3 | 5047 | `				return PH7_EXCEPTION;` |
|         - | 5048 | `			}` |
|         7 | 5049 | `			if( i >= (nArg - 1)){` |
|         - | 5050 | `				/* Perform the insertion */` |
|         5 | 5051 | `				HashmapInsertNode((ph7_hashmap *)pArray->x.pOther,pEntry,TRUE);` |
|         2 | 5052 | `			}` |
|         3 | 5053 | `		}` |
|         - | 5054 | `		/* Point to the next entry */` |
|         7 | 5055 | `		pEntry = pEntry->pPrev; /* Reverse link */` |
|         7 | 5056 | `		n--;` |
|         1 | 5057 | `	}` |
|         - | 5058 | `	/* Return the freshly created array */` |
|         3 | 5059 | `	ph7_result_value(pCtx,pArray);` |
|         3 | 5060 | `	return PH7_OK;` |
|        16 | 5061 | `}` |
|         - | 5062 | `/*` |
|         - | 5063 | ` * array array_diff_assoc(array $array1,array $array2,...)` |
|         - | 5064 | ` *  Computes the difference of arrays with additional index check.` |
|         - | 5065 | ` * Parameters` |
|         - | 5066 | ` *  $array1` |
|         - | 5067 | ` *    The array to compare from` |
|         - | 5068 | ` *  $array2` |
|         - | 5069 | ` *    An array to compare against` |
|         - | 5070 | ` *  $...` |
|         - | 5071 | ` *   More arrays to compare against` |
|         - | 5072 | ` * Return` |
|         - | 5073 | ` *  Returns an array containing all the entries from array1 that` |
|         - | 5074 | ` *  are not present in any of the other arrays.` |
|         - | 5075 | ` */` |
|        20 | 5076 | `static int ph7_hashmap_diff_assoc(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|         5 | 5077 | `{` |
|         - | 5078 | `	ph7_hashmap_node *pN1,*pN2,*pEntry;` |
|         - | 5079 | `	ph7_hashmap *pSrc,*pMap;` |
|         - | 5080 | `	ph7_value *pArray;` |
|         - | 5081 | `	ph7_value *pVal;` |
|         - | 5082 | `	sxi32 rc;` |
|         - | 5083 | `	sxu32 n;` |
|         - | 5084 | `	int i;` |
|         - | 5085 | `	/* Ensure the argument list is valid, emitting the same errors PHP` |
|         - | 5086 | `	 * would produce. This makes behaviour predictable and allows the` |
|         - | 5087 | `	 * accompanying integration tests to pass. */` |
|        25 | 5088 | `	if( nArg < 1 ){` |
|         4 | 5089 | `		return PH7_VmThrowException(pCtx,` |
|         - | 5090 | `			"ArgumentCountError",` |
|         - | 5091 | `			"array_diff_assoc() expects at least 1 argument, %d given",` |
|         1 | 5092 | `			nArg` |
|         - | 5093 | `			);` |
|         - | 5094 | `	}` |
|        22 | 5095 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|         4 | 5096 | `		return PH7_VmThrowException(pCtx,` |
|         - | 5097 | `			"TypeError",` |
|         - | 5098 | `			"array_diff_assoc(): Argument #1 ($array) must be of type array, %s given",` |
|         1 | 5099 | `			ph7_type_name(apArg[0])` |
|         - | 5100 | `			);` |
|         - | 5101 | `	}` |
|        33 | 5102 | `	for(i = 1 ; i < nArg ; i++){` |
|        21 | 5103 | `		if( !ph7_value_is_array(apArg[i]) ){` |
|         8 | 5104 | `			return PH7_VmThrowException(pCtx,` |
|         - | 5105 | `				"TypeError",` |
|         - | 5106 | `				"array_diff_assoc(): Argument #%d must be of type array, %s given",` |
|         2 | 5107 | `				i + 1,` |
|         4 | 5108 | `				ph7_type_name(apArg[i])` |
|         - | 5109 | `				);` |
|         - | 5110 | `		}` |
|         9 | 5111 | `	}` |
|        13 | 5112 | `	if( nArg == 1 ){` |
|         - | 5113 | `		/* Return the first array since we cannot perform a diff */` |
|         3 | 5114 | `		ph7_result_value(pCtx,apArg[0]);` |
|         3 | 5115 | `		return PH7_OK;` |
|         - | 5116 | `	}` |
|         - | 5117 | `	/* Create a new array */` |
|        11 | 5118 | `	pArray = ph7_context_new_array(pCtx);` |
|        11 | 5119 | `	if( pArray == 0 ){` |
|       ! 0 | 5120 | `		ph7_result_null(pCtx);` |
|       ! 0 | 5121 | `		return PH7_OK;` |
|         - | 5122 | `	}` |
|         - | 5123 | `	/* Point to the internal representation of the source hashmap */` |
|        11 | 5124 | `	pSrc = (ph7_hashmap *)apArg[0]->x.pOther;` |
|         - | 5125 | `	/* Perform the diff */` |
|        11 | 5126 | `	pEntry = pSrc->pFirst;` |
|        11 | 5127 | `	n = pSrc->nEntry;` |
|        11 | 5128 | `	pN1 = pN2 = 0;` |
|        29 | 5129 | `	for(;;){` |
|         - | 5130 | `		int keep;` |
|        35 | 5131 | `		if( n < 1 ){` |
|        11 | 5132 | `			break;` |
|         - | 5133 | `		}` |
|         - | 5134 | `		/* assume the element should be kept until we find a match */` |
|        25 | 5135 | `		keep = 1;` |
|        41 | 5136 | `		for( i = 1 ; i < nArg ; i++ ){` |
|         - | 5137 | `			/* all arguments have been validated already, so cast directly */` |
|        29 | 5138 | `			pMap = (ph7_hashmap *)apArg[i]->x.pOther;` |
|         - | 5139 | `			/* Perform a key lookup first */` |
|        29 | 5140 | `			if( pEntry->iType == HASHMAP_INT_NODE ){` |
|        13 | 5141 | `				rc = HashmapLookupIntKey(pMap,pEntry->xKey.iKey,&pN1);` |
|         7 | 5142 | `			}else{` |
|        17 | 5143 | `				rc = HashmapLookupBlobKey(pMap,SyBlobData(&pEntry->xKey.sKey),SyBlobLength(&pEntry->xKey.sKey),&pN1);` |
|         - | 5144 | `			}` |
|        29 | 5145 | `			if( rc != SXRET_OK ){` |
|         - | 5146 | `				/* this array does not contain the key, continue checking others */` |
|        15 | 5147 | `				continue;` |
|         - | 5148 | `			}` |
|         - | 5149 | `			/* key exists; check that value stored in the matching node is equal */` |
|        15 | 5150 | `			pVal = HashmapExtractNodeValue(pEntry);` |
|        15 | 5151 | `			if( pVal ){` |
|         - | 5152 | `				/* directly compare with value at pN1 rather than searching again */` |
|        15 | 5153 | `				ph7_value *pVal2 = HashmapExtractNodeValue(pN1);` |
|        15 | 5154 | `				if( pVal2 ){` |
|        15 | 5155 | `					sxi32 cmp = PH7_MemObjCmp(pVal, pVal2, TRUE, 0);` |
|        15 | 5156 | `					if( cmp == 0 ){` |
|         - | 5157 | `						/* identical key+value found in one of the arrays => drop it */` |
|        13 | 5158 | `						keep = 0;` |
|        13 | 5159 | `						break;` |
|         - | 5160 | `					}` |
|         1 | 5161 | `				}` |
|         1 | 5162 | `			}` |
|         2 | 5163 | `		}` |
|        25 | 5164 | `		if( keep ){` |
|         - | 5165 | `			/* Perform the insertion */` |
|        13 | 5166 | `			HashmapInsertNode((ph7_hashmap *)pArray->x.pOther,pEntry,TRUE);` |
|         6 | 5167 | `		}` |
|         - | 5168 | `		/* Point to the next entry */` |
|        25 | 5169 | `		pEntry = pEntry->pPrev; /* Reverse link */` |
|        25 | 5170 | `		n--;` |
|         1 | 5171 | `	}` |
|         - | 5172 | `	/* Return the freshly created array */` |
|        11 | 5173 | `	ph7_result_value(pCtx,pArray);` |
|        11 | 5174 | `	return PH7_OK;` |
|        15 | 5175 | `}` |
|         - | 5176 | `/*` |
|         - | 5177 | ` * array array_diff_uassoc(array $array1,array $array2,...,callback $key_compare_func)` |
|         - | 5178 | ` *  Computes the difference of arrays with additional index check which is performed` |
|         - | 5179 | ` *  by a user supplied callback function.` |
|         - | 5180 | ` * Parameters` |
|         - | 5181 | ` *  $array1` |
|         - | 5182 | ` *    The array to compare from` |
|         - | 5183 | ` *  $array2` |
|         - | 5184 | ` *    An array to compare against` |
|         - | 5185 | ` *  $...` |
|         - | 5186 | ` *   More arrays to compare against.` |
|         - | 5187 | ` *  $key_compare_func` |
|         - | 5188 | ` *   Callback function to use. The callback function must return an integer` |
|         - | 5189 | ` *   less than, equal to, or greater than zero if the first argument is considered` |
|         - | 5190 | ` *   to be respectively less than, equal to, or greater than the second.` |
|         - | 5191 | ` * Return` |
|         - | 5192 | ` *  Returns an array containing all the entries from array1 that` |
|         - | 5193 | ` *  are not present in any of the other arrays.` |
|         - | 5194 | ` */` |
|        24 | 5195 | `static int ph7_hashmap_diff_uassoc(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|         5 | 5196 | `{` |
|         - | 5197 | `	ph7_hashmap_node *pEntry;` |
|         - | 5198 | `	ph7_hashmap *pSrc,*pMap;` |
|         - | 5199 | `	ph7_value *pCallback;` |
|         - | 5200 | `	ph7_value *pArray;` |
|         - | 5201 | `	sxi32 rc;` |
|         - | 5202 | `	sxu32 n;` |
|         - | 5203 | `	int i;` |
|         - | 5204 |  |
|         - | 5205 | `	/* Argument validation mimicking PHP errors. */` |
|        29 | 5206 | `	if( nArg < 2 ){` |
|         4 | 5207 | `		return PH7_VmThrowException(pCtx,` |
|         - | 5208 | `			"ArgumentCountError",` |
|         - | 5209 | `			"array_diff_uassoc() expects at least 2 arguments, %d given",` |
|         1 | 5210 | `			nArg` |
|         - | 5211 | `			);` |
|         - | 5212 | `	}` |
|        26 | 5213 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|         4 | 5214 | `		return PH7_VmThrowException(pCtx,` |
|         - | 5215 | `			"TypeError",` |
|         - | 5216 | `			"array_diff_uassoc(): Argument #1 ($array) must be of type array, %s given",` |
|         1 | 5217 | `			ph7_type_name(apArg[0])` |
|         - | 5218 | `			);` |
|         - | 5219 | `	}` |
|         - | 5220 | `	/* Intermediate arguments (except last) must be arrays. Last argument is` |
|         - | 5221 | `	 * expected to be a callback. */` |
|        38 | 5222 | `	for(i = 1 ; i < nArg - 1; i++){` |
|        19 | 5223 | `		if( !ph7_value_is_array(apArg[i]) ){` |
|         4 | 5224 | `			return PH7_VmThrowException(pCtx,` |
|         - | 5225 | `				"TypeError",` |
|         - | 5226 | `				"array_diff_uassoc(): Argument #%d must be of type array, %s given",` |
|         1 | 5227 | `				i + 1,` |
|         2 | 5228 | `				ph7_type_name(apArg[i])` |
|         - | 5229 | `				);` |
|         - | 5230 | `		}` |
|         9 | 5231 | `	}` |
|         - | 5232 | `	/* Point to the callback value */` |
|        22 | 5233 | `	pCallback = apArg[nArg - 1];` |
|        22 | 5234 | `	if( !ph7_value_is_callable(pCallback) ){` |
|         - | 5235 | `		/* Compose an error message that closely matches PHP output. When the` |
|         - | 5236 | `		 * argument is an array of the wrong shape we include an extra clause.` |
|         - | 5237 | `		 * If the value is neither array nor string, PHP says "no array or` |
|         - | 5238 | `		 * string given" which we also reproduce. */` |
|         9 | 5239 | `		if( ph7_value_is_array(pCallback) ){` |
|         - | 5240 | `			/* ARRAY CALLBACK must have exactly two members */` |
|         4 | 5241 | `			return PH7_VmThrowException(pCtx,` |
|         - | 5242 | `				"TypeError",` |
|         - | 5243 | `				"array_diff_uassoc(): Argument #%d must be a valid callback, array callback must have exactly two members",` |
|         1 | 5244 | `				nArg` |
|         - | 5245 | `				);` |
|         - | 5246 | `		}` |
|         6 | 5247 | `		if( !ph7_value_is_string(pCallback) ){` |
|         - | 5248 | `			/* neither array nor string */` |
|         8 | 5249 | `			return PH7_VmThrowException(pCtx,` |
|         - | 5250 | `				"TypeError",` |
|         - | 5251 | `				"array_diff_uassoc(): Argument #%d must be a valid callback, no array or string given",` |
|         2 | 5252 | `				nArg` |
|         - | 5253 | `				);` |
|         - | 5254 | `		}` |
|         - | 5255 | `		/* Fallback for string (non-callable) or other leftover cases */` |
|       ! 0 | 5256 | `		return PH7_VmThrowException(pCtx,` |
|         - | 5257 | `			"TypeError",` |
|         - | 5258 | `			"array_diff_uassoc(): Argument #%d must be a valid callback, %s given",` |
|       ! 0 | 5259 | `			nArg,` |
|       ! 0 | 5260 | `			ph7_type_name(pCallback)` |
|         - | 5261 | `			);` |
|         - | 5262 | `	}` |
|        13 | 5263 | `	if( nArg == 2 ){` |
|         - | 5264 | `		/* If we only have the first array and the callback, just return the` |
|         - | 5265 | `		 * input array. */` |
|         3 | 5266 | `		ph7_result_value(pCtx,apArg[0]);` |
|         3 | 5267 | `		return PH7_OK;` |
|         - | 5268 | `	}` |
|         - | 5269 | `	/* Create a new array */` |
|        11 | 5270 | `	pArray = ph7_context_new_array(pCtx);` |
|        11 | 5271 | `	if( pArray == 0 ){` |
|       ! 0 | 5272 | `		ph7_result_null(pCtx);` |
|       ! 0 | 5273 | `		return PH7_OK;` |
|         - | 5274 | `	}` |
|         - | 5275 | `	/* Point to the internal representation of the source hashmap */` |
|        11 | 5276 | `	pSrc = (ph7_hashmap *)apArg[0]->x.pOther;` |
|         - | 5277 | `	/* Perform the diff */` |
|        11 | 5278 | `	pEntry = pSrc->pFirst;` |
|        11 | 5279 | `	n = pSrc->nEntry;` |
|        21 | 5280 | `	for(;;){` |
|         - | 5281 | `		int keep;` |
|        27 | 5282 | `		if( n < 1 ){` |
|         9 | 5283 | `			break;` |
|         - | 5284 | `		}` |
|        19 | 5285 | `		keep = 1;` |
|        31 | 5286 | `		for( i = 1 ; i < nArg - 1; i++ ){` |
|         - | 5287 | `			/* each of these must already be arrays thanks to earlier validation */` |
|        23 | 5288 | `			pMap = (ph7_hashmap *)apArg[i]->x.pOther;` |
|         - | 5289 | `			/* we must compare keys via callback, not by direct lookup */` |
|        23 | 5290 | `			ph7_hashmap_node *pIt = pMap->pFirst;` |
|        45 | 5291 | `			while( pIt ){` |
|         - | 5292 | `				/* build temporary key values for callback */` |
|         - | 5293 | `				ph7_value key1, key2, result;` |
|         - | 5294 | `				/* initialise only once using the appropriate helper */` |
|        33 | 5295 | `				if( pEntry->iType == HASHMAP_INT_NODE ){` |
|       ! 0 | 5296 | `					PH7_MemObjInitFromInt(pMap->pVm,&key1,pEntry->xKey.iKey);` |
|       ! 0 | 5297 | `				}else{` |
|         - | 5298 | `					SyString sStr;` |
|        33 | 5299 | `					SyStringInitFromBuf(&sStr,` |
|         - | 5300 | `						SyBlobData(&pEntry->xKey.sKey),` |
|         - | 5301 | `						SyBlobLength(&pEntry->xKey.sKey));` |
|        33 | 5302 | `					PH7_MemObjInitFromString(pMap->pVm,&key1,&sStr);` |
|         - | 5303 | `				}` |
|        33 | 5304 | `				if( pIt->iType == HASHMAP_INT_NODE ){` |
|       ! 0 | 5305 | `					PH7_MemObjInitFromInt(pMap->pVm,&key2,pIt->xKey.iKey);` |
|       ! 0 | 5306 | `				}else{` |
|         - | 5307 | `					SyString sStr;` |
|        33 | 5308 | `					SyStringInitFromBuf(&sStr,` |
|         - | 5309 | `						SyBlobData(&pIt->xKey.sKey),` |
|         - | 5310 | `						SyBlobLength(&pIt->xKey.sKey));` |
|        33 | 5311 | `					PH7_MemObjInitFromString(pMap->pVm,&key2,&sStr);` |
|         - | 5312 | `				}` |
|        33 | 5313 | `				PH7_MemObjInit(pMap->pVm,&result);` |
|         - | 5314 | `				/* call user callback with (key1, key2) */` |
|         - | 5315 | `				{` |
|         - | 5316 | `					ph7_value *apK[2];` |
|        33 | 5317 | `					apK[0] = &key1;` |
|        33 | 5318 | `					apK[1] = &key2;` |
|        33 | 5319 | `					rc = PH7_VmCallUserFunction(pMap->pVm,pCallback,2,apK,&result);` |
|         - | 5320 | `				}` |
|        33 | 5321 | `				if( rc == PH7_EXCEPTION ){` |
|         - | 5322 | `					/* The key comparison callback raised. Unlike array_udiff/` |
|         - | 5323 | `					 * array_uintersect (which signal back from` |
|         - | 5324 | `					 * HashmapFindValueByCallback via pVm->iCmpCallbackExc), this` |
|         - | 5325 | `					 * function invokes the callback inline, so it cleans up its own` |
|         - | 5326 | `					 * temporaries and propagates the exception directly. */` |
|         3 | 5327 | `					PH7_MemObjRelease(&result);` |
|         3 | 5328 | `					PH7_MemObjRelease(&key1);` |
|         3 | 5329 | `					PH7_MemObjRelease(&key2);` |
|         3 | 5330 | `					return PH7_EXCEPTION;` |
|         - | 5331 | `				}` |
|        31 | 5332 | `				if( rc == SXRET_OK ){` |
|        31 | 5333 | `					if( (result.iFlags & MEMOBJ_INT) == 0 ){` |
|       ! 0 | 5334 | `						PH7_MemObjToInteger(&result);` |
|       ! 0 | 5335 | `					}` |
|        31 | 5336 | `					if( result.x.iVal == 0 ){` |
|         - | 5337 | `						/* keys considered equal by callback; now compare values */` |
|        13 | 5338 | `						ph7_value *pVal1 = HashmapExtractNodeValue(pEntry);` |
|        13 | 5339 | `						ph7_value *pVal2 = HashmapExtractNodeValue(pIt);` |
|        13 | 5340 | `						if( pVal1 && pVal2 ){` |
|        13 | 5341 | `							if( PH7_MemObjCmp(pVal1,pVal2,TRUE,0) == 0 ){` |
|         9 | 5342 | `								keep = 0;` |
|         9 | 5343 | `								PH7_MemObjRelease(&result);` |
|         - | 5344 | `								/* release keys too before breaking */` |
|         9 | 5345 | `								PH7_MemObjRelease(&key1);` |
|         9 | 5346 | `								PH7_MemObjRelease(&key2);` |
|         9 | 5347 | `								break;` |
|         - | 5348 | `							}` |
|         2 | 5349 | `						}` |
|         2 | 5350 | `					}` |
|        11 | 5351 | `				}` |
|        23 | 5352 | `				PH7_MemObjRelease(&result);` |
|        23 | 5353 | `				PH7_MemObjRelease(&key1);` |
|        23 | 5354 | `				PH7_MemObjRelease(&key2);` |
|         - | 5355 | `				/* move to next node */` |
|        23 | 5356 | `				pIt = pIt->pPrev;` |
|        23 | 5357 | `				if( keep == 0 ) break;` |
|         1 | 5358 | `			}` |
|        21 | 5359 | `			if( keep == 0 ) break;` |
|         7 | 5360 | `		}` |
|        17 | 5361 | `		if( keep ){` |
|         - | 5362 | `			/* Perform the insertion */` |
|         9 | 5363 | `			HashmapInsertNode((ph7_hashmap *)pArray->x.pOther,pEntry,TRUE);` |
|         4 | 5364 | `		}` |
|         - | 5365 | `		/* Point to the next entry */` |
|        17 | 5366 | `		pEntry = pEntry->pPrev; /* Reverse link */` |
|        17 | 5367 | `		n--;` |
|         1 | 5368 | `	}` |
|         - | 5369 | `	/* Return the freshly created array */` |
|         9 | 5370 | `	ph7_result_value(pCtx,pArray);` |
|         9 | 5371 | `	return PH7_OK;` |
|        17 | 5372 | `}` |
|         - | 5373 | `/*` |
|         - | 5374 | ` * array array_diff_key(array $array1 ,array $array2,...)` |
|         - | 5375 | ` *  Computes the difference of arrays using keys for comparison.` |
|         - | 5376 | ` * Parameters` |
|         - | 5377 | ` *  $array1` |
|         - | 5378 | ` *    The array to compare from` |
|         - | 5379 | ` *  $array2` |
|         - | 5380 | ` *    An array to compare against` |
|         - | 5381 | ` *  $...` |
|         - | 5382 | ` *   More arrays to compare against` |
|         - | 5383 | ` * Return` |
|         - | 5384 | ` *  Returns an array containing all the entries from array1 whose keys are not present` |
|         - | 5385 | ` *  in any of the other arrays.` |
|         - | 5386 | ` * Note that NULL is returned on failure.` |
|         - | 5387 | ` */` |
|        14 | 5388 | `static int ph7_hashmap_diff_key(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|         4 | 5389 | `{` |
|         - | 5390 | `	ph7_hashmap_node *pEntry;` |
|         - | 5391 | `	ph7_hashmap *pSrc,*pMap;` |
|         - | 5392 | `	ph7_value *pArray;` |
|         - | 5393 | `	sxi32 rc;` |
|         - | 5394 | `	sxu32 n;` |
|         - | 5395 | `	int i;` |
|         - | 5396 | `	/* Validate arguments to mirror PHP behaviour. Previously invalid inputs` |
|         - | 5397 | `	 * would quietly return NULL which is inconsistent with other hashmap` |
|         - | 5398 | `	 * helpers. */` |
|        18 | 5399 | `	if( nArg < 1 ){` |
|         4 | 5400 | `		return PH7_VmThrowException(pCtx,` |
|         - | 5401 | `			"ArgumentCountError",` |
|         - | 5402 | `			"array_diff_key() expects at least 1 argument, %d given",` |
|         1 | 5403 | `			nArg` |
|         - | 5404 | `			);` |
|         - | 5405 | `	}` |
|        15 | 5406 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|         4 | 5407 | `		return PH7_VmThrowException(pCtx,` |
|         - | 5408 | `			"TypeError",` |
|         - | 5409 | `			"array_diff_key(): Argument #1 ($array) must be of type array, %s given",` |
|         1 | 5410 | `			ph7_type_name(apArg[0])` |
|         - | 5411 | `			);` |
|         - | 5412 | `	}` |
|        20 | 5413 | `	for(i = 1 ; i < nArg ; i++){` |
|        12 | 5414 | `		if( !ph7_value_is_array(apArg[i]) ){` |
|         4 | 5415 | `			return PH7_VmThrowException(pCtx,` |
|         - | 5416 | `				"TypeError",` |
|         - | 5417 | `				"array_diff_key(): Argument #%d must be of type array, %s given",` |
|         1 | 5418 | `				i + 1,` |
|         2 | 5419 | `				ph7_type_name(apArg[i])` |
|         - | 5420 | `				);` |
|         - | 5421 | `		}` |
|         5 | 5422 | `	}` |
|         9 | 5423 | `	if( nArg == 1 ){` |
|         - | 5424 | `		/* Return the first array since we cannot perform a diff */` |
|         3 | 5425 | `		ph7_result_value(pCtx,apArg[0]);` |
|         3 | 5426 | `		return PH7_OK;` |
|         - | 5427 | `	}` |
|         - | 5428 | `	/* Create a new array */` |
|         7 | 5429 | `	pArray = ph7_context_new_array(pCtx);` |
|         7 | 5430 | `	if( pArray == 0 ){` |
|       ! 0 | 5431 | `		ph7_result_null(pCtx);` |
|       ! 0 | 5432 | `		return PH7_OK;` |
|         - | 5433 | `	}` |
|         - | 5434 | `	/* Point to the internal representation of the main hashmap */` |
|         7 | 5435 | `	pSrc = (ph7_hashmap *)apArg[0]->x.pOther;` |
|         - | 5436 | `	/* Perfrom the diff */` |
|         7 | 5437 | `	pEntry = pSrc->pFirst;` |
|         7 | 5438 | `	n = pSrc->nEntry;` |
|        12 | 5439 | `	for(;;){` |
|        25 | 5440 | `		if( n < 1 ){` |
|         7 | 5441 | `			break;` |
|         - | 5442 | `		}` |
|        31 | 5443 | `		for( i = 1 ; i < nArg ; i++ ){` |
|        23 | 5444 | `			if( !ph7_value_is_array(apArg[i])) {` |
|         - | 5445 | `				/* ignore */` |
|       ! 0 | 5446 | `				continue;` |
|         - | 5447 | `			}` |
|        23 | 5448 | `			pMap = (ph7_hashmap *)apArg[i]->x.pOther;` |
|        23 | 5449 | `			if( pEntry->iType == HASHMAP_BLOB_NODE ){` |
|        17 | 5450 | `				SyBlob *pKey = &pEntry->xKey.sKey;` |
|         - | 5451 | `				/* Blob lookup */` |
|        17 | 5452 | `				rc = HashmapLookupBlobKey(pMap,SyBlobData(pKey),SyBlobLength(pKey),0);` |
|         9 | 5453 | `			}else{` |
|         - | 5454 | `				/* Int lookup */` |
|         7 | 5455 | `				rc = HashmapLookupIntKey(pMap,pEntry->xKey.iKey,0);` |
|         - | 5456 | `			}` |
|        23 | 5457 | `			if( rc == SXRET_OK ){` |
|         - | 5458 | `				/* Key exists,break immediately */` |
|        11 | 5459 | `				break;` |
|         - | 5460 | `			}` |
|         7 | 5461 | `		}` |
|        19 | 5462 | `		if( i >= nArg ){` |
|         - | 5463 | `			/* Perform the insertion */` |
|         9 | 5464 | `			HashmapInsertNode((ph7_hashmap *)pArray->x.pOther,pEntry,TRUE);` |
|         4 | 5465 | `		}` |
|         - | 5466 | `		/* Point to the next entry */` |
|        19 | 5467 | `		pEntry = pEntry->pPrev; /* Reverse link */` |
|        19 | 5468 | `		n--;` |
|         1 | 5469 | `	}` |
|         - | 5470 | `	/* Return the freshly created array */` |
|         7 | 5471 | `	ph7_result_value(pCtx,pArray);` |
|         7 | 5472 | `	return PH7_OK;` |
|        11 | 5473 | `}` |
|         - | 5474 | `/*` |
|         - | 5475 | ` * array array_intersect(array $array1 ,array $array2,...)` |
|         - | 5476 | ` *  Computes the intersection of arrays.` |
|         - | 5477 | ` * Parameters` |
|         - | 5478 | ` *  $array1` |
|         - | 5479 | ` *    The array to compare from` |
|         - | 5480 | ` *  $array2` |
|         - | 5481 | ` *    An array to compare against` |
|         - | 5482 | ` *  $...` |
|         - | 5483 | ` *   More arrays to compare against` |
|         - | 5484 | ` * Return` |
|         - | 5485 | ` *  Returns an array containing all of the values in array1 whose values exist` |
|         - | 5486 | ` *  in all of the parameters.` |
|         - | 5487 | ` * Throws ArgumentCountError if no arguments are given.` |
|         - | 5488 | ` * Throws TypeError if any argument is not an array.` |
|         - | 5489 | ` */` |
|        22 | 5490 | `static int ph7_hashmap_intersect(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|         4 | 5491 | `{` |
|         - | 5492 | `	ph7_hashmap_node *pEntry;` |
|         - | 5493 | `	ph7_hashmap *pSrc,*pMap;` |
|         - | 5494 | `	ph7_value *pArray;` |
|         - | 5495 | `	ph7_value *pVal;` |
|         - | 5496 | `	sxi32 rc;` |
|         - | 5497 | `	sxu32 n;` |
|         - | 5498 | `	int i;` |
|        26 | 5499 | `	if( nArg < 1 ){` |
|         4 | 5500 | `		return PH7_VmThrowException(pCtx,` |
|         - | 5501 | `			"ArgumentCountError",` |
|         - | 5502 | `			"array_intersect() expects at least 1 argument, %d given",` |
|         1 | 5503 | `			nArg` |
|         - | 5504 | `			);` |
|         - | 5505 | `	}` |
|        23 | 5506 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|         4 | 5507 | `		return PH7_VmThrowException(pCtx,` |
|         - | 5508 | `			"TypeError",` |
|         - | 5509 | `			"array_intersect(): Argument #1 ($array) must be of type array, %s given",` |
|         1 | 5510 | `			ph7_type_name(apArg[0])` |
|         - | 5511 | `			);` |
|         - | 5512 | `	}` |
|        36 | 5513 | `	for( i = 1 ; i < nArg ; i++ ){` |
|        20 | 5514 | `		if( !ph7_value_is_array(apArg[i]) ){` |
|         4 | 5515 | `			return PH7_VmThrowException(pCtx,` |
|         - | 5516 | `				"TypeError",` |
|         - | 5517 | `				"array_intersect(): Argument #%d must be of type array, %s given",` |
|         1 | 5518 | `				i + 1,` |
|         2 | 5519 | `				ph7_type_name(apArg[i])` |
|         - | 5520 | `				);` |
|         - | 5521 | `		}` |
|         9 | 5522 | `	}` |
|        17 | 5523 | `	if( nArg == 1 ){` |
|         - | 5524 | `		/* Return the first array since we cannot perform a diff */` |
|         3 | 5525 | `		ph7_result_value(pCtx,apArg[0]);` |
|         3 | 5526 | `		return PH7_OK;` |
|         - | 5527 | `	}` |
|         - | 5528 | `	/* Create a new array */` |
|        15 | 5529 | `	pArray = ph7_context_new_array(pCtx);` |
|        15 | 5530 | `	if( pArray == 0 ){` |
|       ! 0 | 5531 | `		ph7_result_null(pCtx);` |
|       ! 0 | 5532 | `		return PH7_OK;` |
|         - | 5533 | `	}` |
|         - | 5534 | `	/* Point to the internal representation of the source hashmap */` |
|        15 | 5535 | `	pSrc = (ph7_hashmap *)apArg[0]->x.pOther;` |
|         - | 5536 | `	/* Perform the intersection */` |
|        15 | 5537 | `	pEntry = pSrc->pFirst;` |
|        15 | 5538 | `	n = pSrc->nEntry;` |
|        31 | 5539 | `	for(;;){` |
|        63 | 5540 | `		if( n < 1 ){` |
|        15 | 5541 | `			break;` |
|         - | 5542 | `		}` |
|         - | 5543 | `		/* Extract the node value */` |
|        49 | 5544 | `		pVal = HashmapExtractNodeValue(pEntry);` |
|        49 | 5545 | `		if( pVal ){` |
|        79 | 5546 | `			for( i = 1 ; i < nArg ; i++ ){` |
|         - | 5547 | `				/* Point to the internal representation of the hashmap */` |
|        55 | 5548 | `				pMap = (ph7_hashmap *)apArg[i]->x.pOther;` |
|         - | 5549 | `				/* Perform the lookup */` |
|        55 | 5550 | `				rc = HashmapFindValue(pMap,pVal,0,TRUE);` |
|        55 | 5551 | `				if( rc != SXRET_OK ){` |
|         - | 5552 | `					/* Value does not exist */` |
|        25 | 5553 | `					break;` |
|         - | 5554 | `				}` |
|        16 | 5555 | `			}` |
|        49 | 5556 | `			if( i >= nArg ){` |
|         - | 5557 | `				/* Perform the insertion */` |
|        25 | 5558 | `				HashmapInsertNode((ph7_hashmap *)pArray->x.pOther,pEntry,TRUE);` |
|        12 | 5559 | `			}` |
|        24 | 5560 | `		}` |
|         - | 5561 | `		/* Point to the next entry */` |
|        49 | 5562 | `		pEntry = pEntry->pPrev; /* Reverse link */` |
|        49 | 5563 | `		n--;` |
|         1 | 5564 | `	}` |
|         - | 5565 | `	/* Return the freshly created array */` |
|        15 | 5566 | `	ph7_result_value(pCtx,pArray);` |
|        15 | 5567 | `	return PH7_OK;` |
|        15 | 5568 | `}` |
|         - | 5569 | `/*` |
|         - | 5570 | ` * array array_intersect_assoc(array $array1 ,array $array2,...)` |
|         - | 5571 | ` *  Computes the intersection of arrays with additional index check.` |
|         - | 5572 | ` * Parameters` |
|         - | 5573 | ` *  $array1` |
|         - | 5574 | ` *    The array to compare from` |
|         - | 5575 | ` *  $array2` |
|         - | 5576 | ` *    An array to compare against` |
|         - | 5577 | ` *  $...` |
|         - | 5578 | ` *   More arrays to compare against` |
|         - | 5579 | ` * Return` |
|         - | 5580 | ` *  Returns an array containing all the values of array1 that are present` |
|         - | 5581 | ` *  in all the arguments, with matching keys.` |
|         - | 5582 | ` */` |
|        22 | 5583 | `static int ph7_hashmap_intersect_assoc(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|         4 | 5584 | `{` |
|         - | 5585 | `	ph7_hashmap_node *pEntry,*pN1,*pN2;` |
|         - | 5586 | `	ph7_hashmap *pSrc,*pMap;` |
|         - | 5587 | `	ph7_value *pArray;` |
|         - | 5588 | `	ph7_value *pVal;` |
|         - | 5589 | `	sxi32 rc;` |
|         - | 5590 | `	sxu32 n;` |
|         - | 5591 | `	int i;` |
|        26 | 5592 | `	if( nArg < 1 ){` |
|         4 | 5593 | `		return PH7_VmThrowException(pCtx,` |
|         - | 5594 | `			"ArgumentCountError",` |
|         - | 5595 | `			"array_intersect_assoc() expects at least 1 argument, %d given",` |
|         1 | 5596 | `			nArg` |
|         - | 5597 | `			);` |
|         - | 5598 | `	}` |
|        23 | 5599 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|         4 | 5600 | `		return PH7_VmThrowException(pCtx,` |
|         - | 5601 | `			"TypeError",` |
|         - | 5602 | `			"array_intersect_assoc(): Argument #1 ($array) must be of type array, %s given",` |
|         1 | 5603 | `			ph7_type_name(apArg[0])` |
|         - | 5604 | `			);` |
|         - | 5605 | `	}` |
|        36 | 5606 | `	for( i = 1 ; i < nArg ; i++ ){` |
|        20 | 5607 | `		if( !ph7_value_is_array(apArg[i]) ){` |
|         4 | 5608 | `			return PH7_VmThrowException(pCtx,` |
|         - | 5609 | `				"TypeError",` |
|         - | 5610 | `				"array_intersect_assoc(): Argument #%d must be of type array, %s given",` |
|         1 | 5611 | `				i + 1,` |
|         2 | 5612 | `				ph7_type_name(apArg[i])` |
|         - | 5613 | `				);` |
|         - | 5614 | `		}` |
|         9 | 5615 | `	}` |
|        17 | 5616 | `	if( nArg == 1 ){` |
|         - | 5617 | `		/* Return the first array since we cannot perform an intersection */` |
|         3 | 5618 | `		ph7_result_value(pCtx,apArg[0]);` |
|         3 | 5619 | `		return PH7_OK;` |
|         - | 5620 | `	}` |
|         - | 5621 | `	/* Create a new array */` |
|        15 | 5622 | `	pArray = ph7_context_new_array(pCtx);` |
|        15 | 5623 | `	if( pArray == 0 ){` |
|       ! 0 | 5624 | `		ph7_result_null(pCtx);` |
|       ! 0 | 5625 | `		return PH7_OK;` |
|         - | 5626 | `	}` |
|         - | 5627 | `	/* Point to the internal representation of the source hashmap */` |
|        15 | 5628 | `	pSrc = (ph7_hashmap *)apArg[0]->x.pOther;` |
|         - | 5629 | `	/* Perform the intersection */` |
|        15 | 5630 | `	pEntry = pSrc->pFirst;` |
|        15 | 5631 | `	n = pSrc->nEntry;` |
|        15 | 5632 | `	pN1 = pN2 = 0; /* cc warning */` |
|        23 | 5633 | `	for(;;){` |
|        47 | 5634 | `		if( n < 1 ){` |
|        15 | 5635 | `			break;` |
|         - | 5636 | `		}` |
|         - | 5637 | `		/* Extract the node value */` |
|        33 | 5638 | `		pVal = HashmapExtractNodeValue(pEntry);` |
|        33 | 5639 | `		if( pVal ){` |
|        53 | 5640 | `			for( i = 1 ; i < nArg ; i++ ){` |
|         - | 5641 | `				/* Point to the internal representation of the hashmap */` |
|        37 | 5642 | `				pMap = (ph7_hashmap *)apArg[i]->x.pOther;` |
|         - | 5643 | `				/* Perform a key lookup first */` |
|        37 | 5644 | `				if( pEntry->iType == HASHMAP_INT_NODE ){` |
|        15 | 5645 | `					rc = HashmapLookupIntKey(pMap,pEntry->xKey.iKey,&pN1);` |
|         8 | 5646 | `				}else{` |
|        23 | 5647 | `					rc = HashmapLookupBlobKey(pMap,SyBlobData(&pEntry->xKey.sKey),SyBlobLength(&pEntry->xKey.sKey),&pN1);` |
|         - | 5648 | `				}` |
|        37 | 5649 | `				if( rc != SXRET_OK ){` |
|         - | 5650 | `					/* No such key,break immediately */` |
|         7 | 5651 | `					break;` |
|         - | 5652 | `				}` |
|         - | 5653 | `				/* Perform the lookup */` |
|        31 | 5654 | `				rc = HashmapFindValue(pMap,pVal,&pN2,TRUE);` |
|        31 | 5655 | `				if( rc != SXRET_OK \|\| pN1 != pN2 ){` |
|         - | 5656 | `					/* Value does not exist */` |
|         6 | 5657 | `					break;` |
|         - | 5658 | `				}` |
|        11 | 5659 | `			}` |
|        33 | 5660 | `			if( i >= nArg ){` |
|         - | 5661 | `				/* Perform the insertion */` |
|        17 | 5662 | `				HashmapInsertNode((ph7_hashmap *)pArray->x.pOther,pEntry,TRUE);` |
|         8 | 5663 | `			}` |
|        16 | 5664 | `		}` |
|         - | 5665 | `		/* Point to the next entry */` |
|        33 | 5666 | `		pEntry = pEntry->pPrev; /* Reverse link */` |
|        33 | 5667 | `		n--;` |
|         1 | 5668 | `	}` |
|         - | 5669 | `	/* Return the freshly created array */` |
|        15 | 5670 | `	ph7_result_value(pCtx,pArray);` |
|        15 | 5671 | `	return PH7_OK;` |
|        15 | 5672 | `}` |
|         - | 5673 | `/*` |
|         - | 5674 | ` * array array_intersect_key(array $array1 ,...)` |
|         - | 5675 | ` *  Computes the intersection of arrays using keys for comparison.` |
|         - | 5676 | ` * Parameters` |
|         - | 5677 | ` *  $array1` |
|         - | 5678 | ` *    The array to compare from` |
|         - | 5679 | ` *  $...` |
|         - | 5680 | ` *   More arrays to compare against` |
|         - | 5681 | ` * Return` |
|         - | 5682 | ` *  Returns an associative array containing all the entries of array1 which` |
|         - | 5683 | ` *  have keys that are present in all arguments.` |
|         - | 5684 | ` * Note that NULL is returned on failure.` |
|         - | 5685 | ` */` |
|        22 | 5686 | `static int ph7_hashmap_intersect_key(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|         4 | 5687 | `{` |
|         - | 5688 | `	ph7_hashmap_node *pEntry;` |
|         - | 5689 | `	ph7_hashmap *pSrc,*pMap;` |
|         - | 5690 | `	ph7_value *pArray;` |
|         - | 5691 | `	sxi32 rc;` |
|         - | 5692 | `	sxu32 n;` |
|         - | 5693 | `	int i;` |
|        26 | 5694 | `	if( nArg < 1 ){` |
|         4 | 5695 | `		return PH7_VmThrowException(pCtx,` |
|         - | 5696 | `			"ArgumentCountError",` |
|         - | 5697 | `			"array_intersect_key() expects at least 1 argument, %d given",` |
|         1 | 5698 | `			nArg` |
|         - | 5699 | `			);` |
|         - | 5700 | `	}` |
|        23 | 5701 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|         4 | 5702 | `		return PH7_VmThrowException(pCtx,` |
|         - | 5703 | `			"TypeError",` |
|         - | 5704 | `			"array_intersect_key(): Argument #1 ($array) must be of type array, %s given",` |
|         1 | 5705 | `			ph7_type_name(apArg[0])` |
|         - | 5706 | `			);` |
|         - | 5707 | `	}` |
|        36 | 5708 | `	for( i = 1 ; i < nArg ; i++ ){` |
|        20 | 5709 | `		if( !ph7_value_is_array(apArg[i]) ){` |
|         4 | 5710 | `			return PH7_VmThrowException(pCtx,` |
|         - | 5711 | `				"TypeError",` |
|         - | 5712 | `				"array_intersect_key(): Argument #%d must be of type array, %s given",` |
|         1 | 5713 | `				i + 1,` |
|         2 | 5714 | `				ph7_type_name(apArg[i])` |
|         - | 5715 | `				);` |
|         - | 5716 | `		}` |
|         9 | 5717 | `	}` |
|        17 | 5718 | `	if( nArg == 1 ){` |
|         - | 5719 | `		/* Return the first array since we cannot perform an intersection */` |
|         3 | 5720 | `		ph7_result_value(pCtx,apArg[0]);` |
|         3 | 5721 | `		return PH7_OK;` |
|         - | 5722 | `	}` |
|         - | 5723 | `	/* Create a new array */` |
|        15 | 5724 | `	pArray = ph7_context_new_array(pCtx);` |
|        15 | 5725 | `	if( pArray == 0 ){` |
|       ! 0 | 5726 | `		ph7_result_null(pCtx);` |
|       ! 0 | 5727 | `		return PH7_OK;` |
|         - | 5728 | `	}` |
|         - | 5729 | `	/* Point to the internal representation of the main hashmap */` |
|        15 | 5730 | `	pSrc = (ph7_hashmap *)apArg[0]->x.pOther;` |
|         - | 5731 | `	/* Perform the intersection */` |
|        15 | 5732 | `	pEntry = pSrc->pFirst;` |
|        15 | 5733 | `	n = pSrc->nEntry;` |
|        24 | 5734 | `	for(;;){` |
|        49 | 5735 | `		if( n < 1 ){` |
|        15 | 5736 | `			break;` |
|         - | 5737 | `		}` |
|        57 | 5738 | `		for( i = 1 ; i < nArg ; i++ ){` |
|        39 | 5739 | `			pMap = (ph7_hashmap *)apArg[i]->x.pOther;` |
|        39 | 5740 | `			if( pEntry->iType == HASHMAP_BLOB_NODE ){` |
|        27 | 5741 | `				SyBlob *pKey = &pEntry->xKey.sKey;` |
|         - | 5742 | `				/* Blob lookup */` |
|        27 | 5743 | `				rc = HashmapLookupBlobKey(pMap,SyBlobData(pKey),SyBlobLength(pKey),0);` |
|        14 | 5744 | `			}else{` |
|         - | 5745 | `				/* Int key */` |
|        13 | 5746 | `				rc = HashmapLookupIntKey(pMap,pEntry->xKey.iKey,0);` |
|         - | 5747 | `			}` |
|        39 | 5748 | `			if( rc != SXRET_OK ){` |
|         - | 5749 | `				/* Key does not exist, break immediately */` |
|        17 | 5750 | `				break;` |
|         - | 5751 | `			}` |
|        12 | 5752 | `		}` |
|        35 | 5753 | `		if( i >= nArg ){` |
|         - | 5754 | `			/* Perform the insertion */` |
|        19 | 5755 | `			HashmapInsertNode((ph7_hashmap *)pArray->x.pOther,pEntry,TRUE);` |
|         9 | 5756 | `		}` |
|         - | 5757 | `		/* Point to the next entry */` |
|        35 | 5758 | `		pEntry = pEntry->pPrev; /* Reverse link */` |
|        35 | 5759 | `		n--;` |
|         1 | 5760 | `	}` |
|         - | 5761 | `	/* Return the freshly created array */` |
|        15 | 5762 | `	ph7_result_value(pCtx,pArray);` |
|        15 | 5763 | `	return PH7_OK;` |
|        15 | 5764 | `}` |
|         - | 5765 | `/*` |
|         - | 5766 | ` * array array_uintersect(array $array1 ,array $array2,...,$callback)` |
|         - | 5767 | ` *  Computes the intersection of arrays.` |
|         - | 5768 | ` * Parameters` |
|         - | 5769 | ` *  $array1` |
|         - | 5770 | ` *    The array to compare from` |
|         - | 5771 | ` *  $array2` |
|         - | 5772 | ` *    An array to compare against` |
|         - | 5773 | ` *  $...` |
|         - | 5774 | ` *   More arrays to compare against` |
|         - | 5775 | ` * $callback` |
|         - | 5776 | ` *  The callback comparison function.` |
|         - | 5777 | ` *  The comparison function must return an integer less than, equal to, or greater than zero` |
|         - | 5778 | ` *  if the first argument is considered to be respectively less than, equal to, or greater` |
|         - | 5779 | ` *  than the second.` |
|         - | 5780 | ` *     int callback ( mixed $a, mixed $b )` |
|         - | 5781 | ` * Return` |
|         - | 5782 | ` *  Returns an array containing all of the values in array1 whose values exist` |
|         - | 5783 | ` *  in all of the parameters. .` |
|         - | 5784 | ` * Note that NULL is returned on failure.` |
|         - | 5785 | ` */` |
|        26 | 5786 | `static int ph7_hashmap_uintersect(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|         5 | 5787 | `{` |
|         - | 5788 | `	ph7_hashmap_node *pEntry;` |
|         - | 5789 | `	ph7_hashmap *pSrc,*pMap;` |
|         - | 5790 | `	ph7_value *pCallback;` |
|         - | 5791 | `	ph7_value *pArray;` |
|         - | 5792 | `	ph7_value *pVal;` |
|         - | 5793 | `	sxi32 rc;` |
|         - | 5794 | `	sxu32 n;` |
|         - | 5795 | `	int i;` |
|         - | 5796 |  |
|         - | 5797 | `	/* Ensure the argument count matches PHP behaviour. */` |
|        31 | 5798 | `	if( nArg < 2 ){` |
|         4 | 5799 | `		return PH7_VmThrowException(pCtx,` |
|         - | 5800 | `			"ArgumentCountError",` |
|         - | 5801 | `			"array_uintersect() expects at least 2 arguments, %d given",` |
|         1 | 5802 | `			nArg` |
|         - | 5803 | `			);` |
|         - | 5804 | `	}` |
|        29 | 5805 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|         4 | 5806 | `		return PH7_VmThrowException(pCtx,` |
|         - | 5807 | `			"TypeError",` |
|         - | 5808 | `			"array_uintersect(): Argument #1 ($array) must be of type array, %s given",` |
|         1 | 5809 | `			ph7_type_name(apArg[0])` |
|         - | 5810 | `			);` |
|         - | 5811 | `	}` |
|         - | 5812 |  |
|        27 | 5813 | `	if( nArg == 2 ){` |
|         - | 5814 | `		/* Only the original array and the callback were provided. */` |
|         - | 5815 | `		/* Validate the callback below in order to match PHP's parameter` |
|         - | 5816 | `		 * validation ordering. */` |
|         3 | 5817 | `	} else {` |
|         - | 5818 | `		/* Ensure intermediary arguments are arrays (matches PHP strict typing). */` |
|        39 | 5819 | `		for( i = 1 ; i < nArg - 1; i++ ){` |
|        23 | 5820 | `			if( !ph7_value_is_array(apArg[i]) ){` |
|         4 | 5821 | `				return PH7_VmThrowException(pCtx,` |
|         - | 5822 | `					"TypeError",` |
|         - | 5823 | `					"array_uintersect(): Argument #%d must be of type array, %s given",` |
|         1 | 5824 | `					i + 1,` |
|         2 | 5825 | `					ph7_type_name(apArg[i])` |
|         - | 5826 | `					);` |
|         - | 5827 | `			}` |
|        13 | 5828 | `		}` |
|         - | 5829 | `	}` |
|         - | 5830 |  |
|         - | 5831 | `	/* Identify the callback (always expected as the last argument). */` |
|        25 | 5832 | `	pCallback = apArg[nArg - 1];` |
|         - | 5833 | `	/* Validate the callback to match PHP's error messages. */` |
|        25 | 5834 | `	if( !ph7_value_is_callable(pCallback) ){` |
|        14 | 5835 | `		if( ph7_value_is_array(pCallback) ){` |
|         - | 5836 | `			/* PHP emits a special message when the array length is wrong.` |
|         - | 5837 | `			 * If the array has two elements but is still not callable (e.g. missing` |
|         - | 5838 | `			 * method / missing class), we must emit a more general error instead.` |
|         - | 5839 | `			 */` |
|         9 | 5840 | `			ph7_hashmap *pCb = (ph7_hashmap *)pCallback->x.pOther;` |
|         9 | 5841 | `			if( pCb->nEntry != 2 ){` |
|         4 | 5842 | `				return PH7_VmThrowException(pCtx,` |
|         - | 5843 | `					"TypeError",` |
|         - | 5844 | `					"array_uintersect(): Argument #%d must be a valid callback, array callback must have exactly two members",` |
|         1 | 5845 | `					nArg` |
|         - | 5846 | `					);` |
|         - | 5847 | `			}` |
|         - | 5848 | `			/* Try to provide a more precise error like PHP does for missing classes/methods. */` |
|         - | 5849 | `			{` |
|         6 | 5850 | `				ph7_value *pKey = (ph7_value *)SySetAt(&pCtx->pVm->aMemObj,pCb->pFirst->nValIdx);` |
|         6 | 5851 | `				ph7_value *pMethod = (ph7_value *)SySetAt(&pCtx->pVm->aMemObj,pCb->pFirst->pPrev->nValIdx);` |
|         6 | 5852 | `				if( pKey && pMethod && (pMethod->iFlags & MEMOBJ_STRING) ){` |
|         - | 5853 | `					int nMethodLen;` |
|         6 | 5854 | `					const char *zMethod = ph7_value_to_string(pMethod,&nMethodLen);` |
|         6 | 5855 | `					ph7_class *pClass = PH7_VmExtractClassFromValue(pCtx->pVm,pKey);` |
|         6 | 5856 | `					if( pClass ){` |
|         - | 5857 | `						/* Class exists but method is missing. */` |
|         4 | 5858 | `						return PH7_VmThrowException(pCtx,` |
|         - | 5859 | `							"TypeError",` |
|         - | 5860 | `							"array_uintersect(): Argument #%d must be a valid callback, class %s does not have a method \"%s\"",` |
|         1 | 5861 | `							nArg,` |
|         1 | 5862 | `							(const char *)SyStringData(&pClass->sName),` |
|         1 | 5863 | `							zMethod` |
|         - | 5864 | `							);` |
|         - | 5865 | `					}` |
|         - | 5866 | `					/* Class not found */` |
|         - | 5867 | `					{` |
|         - | 5868 | `						int nName;` |
|         3 | 5869 | `						const char *zName = ph7_value_to_string(pKey,&nName);` |
|         4 | 5870 | `						return PH7_VmThrowException(pCtx,` |
|         - | 5871 | `							"TypeError",` |
|         - | 5872 | `							"array_uintersect(): Argument #%d must be a valid callback, class \"%s\" not found",` |
|         1 | 5873 | `							nArg,` |
|         1 | 5874 | `							zName` |
|         - | 5875 | `							);` |
|         - | 5876 | `					}` |
|         - | 5877 | `				}` |
|         - | 5878 | `			}` |
|         - | 5879 | `			/* Fallback message */` |
|       ! 0 | 5880 | `			return PH7_VmThrowException(pCtx,` |
|         - | 5881 | `				"TypeError",` |
|         - | 5882 | `				"array_uintersect(): Argument #%d must be a valid callback, no array or string given",` |
|       ! 0 | 5883 | `				nArg` |
|         - | 5884 | `				);` |
|         - | 5885 | `		}` |
|         6 | 5886 | `		if( ph7_value_is_string(pCallback) ){` |
|         - | 5887 | `			int len;` |
|         3 | 5888 | `			const char *zName = ph7_value_to_string(pCallback, &len);` |
|         4 | 5889 | `			return PH7_VmThrowException(pCtx,` |
|         - | 5890 | `				"TypeError",` |
|         - | 5891 | `				"array_uintersect(): Argument #%d must be a valid callback, function \"%s\" not found or invalid function name",` |
|         1 | 5892 | `				nArg,` |
|         1 | 5893 | `				zName` |
|         - | 5894 | `				);` |
|         - | 5895 | `		}` |
|         4 | 5896 | `		return PH7_VmThrowException(pCtx,` |
|         - | 5897 | `			"TypeError",` |
|         - | 5898 | `			"array_uintersect(): Argument #%d must be a valid callback, no array or string given",` |
|         1 | 5899 | `			nArg` |
|         - | 5900 | `			);` |
|         - | 5901 | `	}` |
|         - | 5902 |  |
|        11 | 5903 | `	if( nArg == 2 ){` |
|         - | 5904 | `		/* Only the original array and the callback were provided. */` |
|         5 | 5905 | `		ph7_result_value(pCtx,apArg[0]);` |
|         5 | 5906 | `		return PH7_OK;` |
|         - | 5907 | `	}` |
|         - | 5908 |  |
|         - | 5909 | `	/* Create a new array */` |
|         7 | 5910 | `	pArray = ph7_context_new_array(pCtx);` |
|         7 | 5911 | `	if( pArray == 0 ){` |
|       ! 0 | 5912 | `		ph7_result_null(pCtx);` |
|       ! 0 | 5913 | `		return PH7_OK;` |
|         - | 5914 | `	}` |
|         - | 5915 | `	/* Point to the internal representation of the source hashmap */` |
|         7 | 5916 | `	pSrc = (ph7_hashmap *)apArg[0]->x.pOther;` |
|         - | 5917 | `	/* Perform the intersection */` |
|         7 | 5918 | `	pEntry = pSrc->pFirst;` |
|         7 | 5919 | `	n = pSrc->nEntry;` |
|         7 | 5920 | `	pCtx->pVm->iCmpCallbackExc = 0;` |
|         9 | 5921 | `	for(;;){` |
|        19 | 5922 | `		if( n < 1 ){` |
|         5 | 5923 | `			break;` |
|         - | 5924 | `		}` |
|         - | 5925 | `		/* Extract the node value */` |
|        15 | 5926 | `		pVal = HashmapExtractNodeValue(pEntry);` |
|        15 | 5927 | `		if( pVal ){` |
|        23 | 5928 | `			for( i = 1 ; i < nArg - 1; i++ ){` |
|        15 | 5929 | `				if( !ph7_value_is_array(apArg[i])) {` |
|         - | 5930 | `					/* ignore */` |
|       ! 0 | 5931 | `					continue;` |
|         - | 5932 | `				}` |
|         - | 5933 | `				/* Point to the internal representation of the hashmap */` |
|        15 | 5934 | `				pMap = (ph7_hashmap *)apArg[i]->x.pOther;` |
|         - | 5935 | `				/* Perform the lookup */` |
|        15 | 5936 | `				rc = HashmapFindValueByCallback(pMap,pVal,pCallback,0);` |
|        15 | 5937 | `				if( rc != SXRET_OK ){` |
|         - | 5938 | `					/* Value does not exist */` |
|         7 | 5939 | `					break;` |
|         - | 5940 | `				}` |
|         5 | 5941 | `			}` |
|        15 | 5942 | `			if( i >= (nArg-1) ){` |
|         - | 5943 | `				/* Perform the insertion */` |
|         9 | 5944 | `				HashmapInsertNode((ph7_hashmap *)pArray->x.pOther,pEntry,TRUE);` |
|         4 | 5945 | `			}` |
|         7 | 5946 | `		}` |
|        15 | 5947 | `		if( pCtx->pVm->iCmpCallbackExc ){` |
|         - | 5948 | `			/* The comparison callback raised: propagate so the dispatcher unwinds. */` |
|         3 | 5949 | `			pCtx->pVm->iCmpCallbackExc = 0;` |
|         3 | 5950 | `			return PH7_EXCEPTION;` |
|         - | 5951 | `		}` |
|         - | 5952 | `		/* Point to the next entry */` |
|        13 | 5953 | `		pEntry = pEntry->pPrev; /* Reverse link */` |
|        13 | 5954 | `		n--;` |
|         1 | 5955 | `	}` |
|         - | 5956 | `	/* Return the freshly created array */` |
|         5 | 5957 | `	ph7_result_value(pCtx,pArray);` |
|         5 | 5958 | `	return PH7_OK;` |
|        18 | 5959 | `}` |
|         - | 5960 | `/*` |
|         - | 5961 | ` * array array_fill(int $start_index,int $num,var $value)` |
|         - | 5962 | ` *  Fill an array with values.` |
|         - | 5963 | ` * Parameters` |
|         - | 5964 | ` *  $start_index` |
|         - | 5965 | ` *    The first index of the returned array.` |
|         - | 5966 | ` *  $num` |
|         - | 5967 | ` *   Number of elements to insert.` |
|         - | 5968 | ` *  $value` |
|         - | 5969 | ` *    Value to use for filling.` |
|         - | 5970 | ` * Return` |
|         - | 5971 | ` *  The filled array or null on failure.` |
|         - | 5972 | ` */` |
|       244 | 5973 | `static int ph7_hashmap_fill(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|         5 | 5974 | `{` |
|         - | 5975 | `	ph7_value *pArray;` |
|         - | 5976 | `	int i,nEntry;` |
|         - | 5977 |  |
|         - | 5978 | `	/* PHP enforces argument count and type checks. */` |
|       249 | 5979 | `	if( nArg != 3 ){` |
|         - | 5980 | `		/* wrong number of arguments -> ArgumentCountError */` |
|         8 | 5981 | `		return PH7_VmThrowException(pCtx,` |
|         - | 5982 | `			"ArgumentCountError",` |
|         - | 5983 | `			"array_fill() expects exactly 3 arguments, %d given",` |
|         2 | 5984 | `			nArg` |
|         - | 5985 | `			);` |
|         - | 5986 | `	}` |
|         - | 5987 |  |
|         - | 5988 | `	/* Argument #1: start index must be convertible to int.  Accept booleans,` |
|         - | 5989 | `	 * floats, and numeric strings (including those with decimal point) by` |
|         - | 5990 | `	 * allowing them through the conversion.  Only arrays, objects, resources` |
|         - | 5991 | `	 * and NULLs are rejected outright. */` |
|       359 | 5992 | `	if( ph7_value_is_array(apArg[0]) \|\| ph7_value_is_object(apArg[0]) \|\|` |
|       362 | 5993 | `		ph7_value_is_resource(apArg[0]) \|\| ph7_value_is_null(apArg[0]) ){` |
|         4 | 5994 | `		return PH7_VmThrowException(pCtx,` |
|         - | 5995 | `			"TypeError",` |
|         - | 5996 | `			"array_fill(): Argument #1 ($start_index) must be of type int, %s given",` |
|         1 | 5997 | `			ph7_type_name(apArg[0])` |
|         - | 5998 | `			);` |
|         - | 5999 | `	}` |
|       242 | 6000 | `	if( ph7_value_is_string(apArg[0]) ){` |
|         - | 6001 | `		int len;` |
|         8 | 6002 | `		sxu8 bReal = FALSE;` |
|         8 | 6003 | `		const char *zStr = ph7_value_to_string(apArg[0], &len);` |
|         8 | 6004 | `		if( SyStrIsNumeric(zStr, len, &bReal, 0) != SXRET_OK ){` |
|         - | 6005 | `			/* Non‑numeric string is an error. */` |
|         3 | 6006 | `			return PH7_VmThrowException(pCtx,` |
|         - | 6007 | `				"TypeError",` |
|         - | 6008 | `				"array_fill(): Argument #1 ($start_index) must be of type int, string given"` |
|         - | 6009 | `				);` |
|         - | 6010 | `		}` |
|         5 | 6011 | `		if( bReal ){` |
|         - | 6012 | `			/* float-string -> deprecation warning */` |
|         4 | 6013 | `			ph7_context_throw_error_format(pCtx, E_DEPRECATED,` |
|         - | 6014 | `				"Implicit conversion from float-string \"%s\" to int loses precision",` |
|         1 | 6015 | `				zStr` |
|         - | 6016 | `				);` |
|         1 | 6017 | `		}` |
|         2 | 6018 | `	}` |
|         - | 6019 |  |
|         - | 6020 | `	/* Argument #2: count must be convertible to non-negative int.  Allow booleans,` |
|         - | 6021 | `	 * floats and numeric strings; reject arrays, objects, resources and NULL. */` |
|       354 | 6022 | `	if( ph7_value_is_array(apArg[1]) \|\| ph7_value_is_object(apArg[1]) \|\|` |
|       357 | 6023 | `		ph7_value_is_resource(apArg[1]) \|\| ph7_value_is_null(apArg[1]) ){` |
|       ! 0 | 6024 | `		return PH7_VmThrowException(pCtx,` |
|         - | 6025 | `			"TypeError",` |
|         - | 6026 | `			"array_fill(): Argument #2 ($count) must be of type int, %s given",` |
|       ! 0 | 6027 | `			ph7_type_name(apArg[1])` |
|         - | 6028 | `			);` |
|         - | 6029 | `	}` |
|       239 | 6030 | `	if( ph7_value_is_string(apArg[1]) ){` |
|         - | 6031 | `		int len;` |
|         3 | 6032 | `		sxu8 bReal = FALSE;` |
|         3 | 6033 | `		const char *zStr = ph7_value_to_string(apArg[1], &len);` |
|         3 | 6034 | `		if( SyStrIsNumeric(zStr, len, &bReal, 0) != SXRET_OK ){` |
|         3 | 6035 | `			return PH7_VmThrowException(pCtx,` |
|         - | 6036 | `				"TypeError",` |
|         - | 6037 | `				"array_fill(): Argument #2 ($count) must be of type int, string given"` |
|         - | 6038 | `				);` |
|         - | 6039 | `		}` |
|       ! 0 | 6040 | `	}` |
|         - | 6041 | `	/* Note: booleans and floats (including fractional) are now accepted; they` |
|         - | 6042 | `	 * will be converted by ph7_value_to_int below. */` |
|       236 | 6043 | `	if( ph7_value_is_float(apArg[1]) ){` |
|         5 | 6044 | `		double d = ph7_value_to_double(apArg[1]);` |
|         - | 6045 | `		/* avoid hiding outer 'i' (loop index) */` |
|         5 | 6046 | `		sxi64 i64 = (sxi64)d;` |
|         5 | 6047 | `		if( d != (double)i64 ){` |
|         7 | 6048 | `			ph7_context_throw_error_format(pCtx, E_DEPRECATED,` |
|         - | 6049 | `				"Implicit conversion from float %g to int loses precision",` |
|         2 | 6050 | `				d` |
|         - | 6051 | `				);` |
|         2 | 6052 | `		}` |
|         2 | 6053 | `	}` |
|         - | 6054 |  |
|         - | 6055 | `	/* Total number of entries to insert */` |
|       236 | 6056 | `	nEntry = ph7_value_to_int(apArg[1]);` |
|         - | 6057 | `	/* Reject negative counts with a ValueError like PHP. */` |
|       236 | 6058 | `	if( nEntry < 0 ){` |
|         3 | 6059 | `		return PH7_VmThrowException(pCtx,` |
|         - | 6060 | `			"ValueError",` |
|         - | 6061 | `			"array_fill(): Argument #2 ($count) must be greater than or equal to 0"` |
|         - | 6062 | `			);` |
|         - | 6063 | `	}` |
|         - | 6064 |  |
|         - | 6065 | `	/* If zero elements were requested, return an empty array without allocating */` |
|       233 | 6066 | `	if( nEntry == 0 ){` |
|         7 | 6067 | `		ph7_result_value(pCtx, ph7_context_new_array(pCtx));` |
|         7 | 6068 | `		return PH7_OK;` |
|         - | 6069 | `	}` |
|         - | 6070 |  |
|         - | 6071 | `	/* Create a new array */` |
|       227 | 6072 | `	pArray = ph7_context_new_array(pCtx);` |
|       227 | 6073 | `	if( pArray == 0 ){` |
|       ! 0 | 6074 | `		return PH7_ContextMemoryError(pCtx);` |
|         - | 6075 | `	}` |
|         - | 6076 |  |
|         - | 6077 | `	/* PHP 8 fills consecutive integer keys start_index, start_index+1, … even` |
|         - | 6078 | `	 * when start_index is negative (PHP 7 restarted the remaining keys from 0,` |
|         - | 6079 | `	 * so array_fill(-5,3) gave -5,0,1 instead of -5,-4,-3). Assign each key` |
|         - | 6080 | `	 * explicitly rather than relying on automatic (append) indexing. */` |
|       227 | 6081 | `	int iStart = ph7_value_to_int(apArg[0]);` |
|   2117831 | 6082 | `	for( i = 0 ; i < nEntry ; i++ ){` |
|   2117605 | 6083 | `		if( ph7_array_add_intkey_elem(pArray, iStart + i, apArg[2]) != SXRET_OK ){` |
|         - | 6084 | `			/* Allocation failure: surface a fatal instead of a partial array */` |
|       ! 0 | 6085 | `			return PH7_ContextMemoryError(pCtx);` |
|         - | 6086 | `		}` |
|   1058803 | 6087 | `	}` |
|         - | 6088 | `	/* Return the filled array */` |
|       227 | 6089 | `	ph7_result_value(pCtx, pArray);` |
|       227 | 6090 | `	return PH7_OK;` |
|       127 | 6091 | `}` |
|         - | 6092 | `/*` |
|         - | 6093 | ` * array array_fill_keys(array $input,mixed $value)` |
|         - | 6094 | ` *  Fill an array with values, specifying keys.` |
|         - | 6095 | ` * Parameters` |
|         - | 6096 | ` *  $input` |
|         - | 6097 | ` *   Array of values that will be used as key.` |
|         - | 6098 | ` *  $value` |
|         - | 6099 | ` *    Value to use for filling.` |
|         - | 6100 | ` * Return` |
|         - | 6101 | ` *  The filled array.` |
|         - | 6102 | ` * Throws` |
|         - | 6103 | ` *  ValueError if $input is not an array.` |
|         - | 6104 | ` */` |
|        26 | 6105 | `static int ph7_hashmap_fill_keys(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|         5 | 6106 | `{` |
|         - | 6107 | `	ph7_hashmap_node *pEntry;` |
|         - | 6108 | `	ph7_hashmap *pSrc;` |
|         - | 6109 | `	ph7_value *pArray;` |
|         - | 6110 | `	sxu32 n;` |
|         - | 6111 | `	/* PHP enforces exactly 2 arguments. */` |
|        31 | 6112 | `	if( nArg != 2 ){` |
|        12 | 6113 | `		return PH7_VmThrowException(pCtx,` |
|         - | 6114 | `			"ArgumentCountError",` |
|         - | 6115 | `			"array_fill_keys() expects exactly 2 arguments, %d given",` |
|         3 | 6116 | `			nArg` |
|         - | 6117 | `			);` |
|         - | 6118 | `	}` |
|         - | 6119 | `	/* Make sure we are dealing with a valid hashmap */` |
|        23 | 6120 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|         8 | 6121 | `		return PH7_VmThrowException(pCtx,` |
|         - | 6122 | `			"TypeError",` |
|         - | 6123 | `			"array_fill_keys(): Argument #1 ($keys) must be of type array, %s given",` |
|         2 | 6124 | `			ph7_type_name(apArg[0])` |
|         - | 6125 | `			);` |
|         - | 6126 | `	}` |
|         - | 6127 | `	/* Point to the internal representation of the input hashmap */` |
|        17 | 6128 | `	pSrc = (ph7_hashmap *)apArg[0]->x.pOther;` |
|         - | 6129 | `	/* Create a new array */` |
|        17 | 6130 | `	pArray = ph7_context_new_array(pCtx);` |
|        17 | 6131 | `	if( pArray == 0 ){` |
|       ! 0 | 6132 | `		ph7_result_null(pCtx);` |
|       ! 0 | 6133 | `		return PH7_OK;` |
|         - | 6134 | `	}` |
|         - | 6135 | `	/* Perform the requested operation */` |
|        17 | 6136 | `	pEntry = pSrc->pFirst;` |
|        45 | 6137 | `	for( n = 0 ; n < pSrc->nEntry ; n++ ){` |
|        29 | 6138 | `		ph7_array_add_elem(pArray,HashmapExtractNodeValue(pEntry),apArg[1]);` |
|         - | 6139 | `		/* Point to the next entry */` |
|        29 | 6140 | `		pEntry = pEntry->pPrev; /* Reverse link */` |
|        15 | 6141 | `	}` |
|         - | 6142 | `	/* Return the filled array */` |
|        17 | 6143 | `	ph7_result_value(pCtx,pArray);` |
|        17 | 6144 | `	return PH7_OK;` |
|        18 | 6145 | `}` |
|         - | 6146 | `/*` |
|         - | 6147 | ` * array array_combine(array $keys,array $values)` |
|         - | 6148 | ` *  Creates an array by using one array for keys and another for its values.` |
|         - | 6149 | ` * Parameters` |
|         - | 6150 | ` *  $keys` |
|         - | 6151 | ` *    Array of keys to be used.` |
|         - | 6152 | ` * $values` |
|         - | 6153 | ` *   Array of values to be used.` |
|         - | 6154 | ` * Return` |
|         - | 6155 | ` *  Returns the combined array. Otherwise FALSE if the number of elements` |
|         - | 6156 | ` *  for each array isn't equal or if one of the given arguments is` |
|         - | 6157 | ` *  not an array.` |
|         - | 6158 | ` */` |
|        18 | 6159 | `static int ph7_hashmap_combine(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|         5 | 6160 | `{` |
|         - | 6161 | `	ph7_hashmap_node *pKe,*pVe;` |
|         - | 6162 | `	ph7_hashmap *pKey,*pValue;` |
|         - | 6163 | `	ph7_value *pArray;` |
|         - | 6164 | `	sxu32 n;` |
|         - | 6165 | `	/* PHP enforces argument count and type checks. */` |
|        23 | 6166 | `	if( nArg != 2 ){` |
|         - | 6167 | `		/* wrong number of arguments -> ArgumentCountError */` |
|         4 | 6168 | `		return PH7_VmThrowException(pCtx,` |
|         - | 6169 | `			"ArgumentCountError",` |
|         - | 6170 | `			"array_combine() expects exactly 2 arguments, %d given",` |
|         1 | 6171 | `			nArg` |
|         - | 6172 | `			);` |
|         - | 6173 | `	}` |
|         - | 6174 | `	/* Validate argument types individually so we can report the correct` |
|         - | 6175 | `	 * argument index in the error message. */` |
|        20 | 6176 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|         4 | 6177 | `		return PH7_VmThrowException(pCtx,` |
|         - | 6178 | `			"TypeError",` |
|         - | 6179 | `			"array_combine(): Argument #1 ($keys) must be of type array, %s given",` |
|         1 | 6180 | `			ph7_type_name(apArg[0])` |
|         - | 6181 | `			);` |
|         - | 6182 | `	}` |
|        17 | 6183 | `	if( !ph7_value_is_array(apArg[1]) ){` |
|         4 | 6184 | `		return PH7_VmThrowException(pCtx,` |
|         - | 6185 | `			"TypeError",` |
|         - | 6186 | `			"array_combine(): Argument #2 ($values) must be of type array, %s given",` |
|         2 | 6187 | `			ph7_type_name(apArg[1])` |
|         - | 6188 | `			);` |
|         - | 6189 | `	}` |
|         - | 6190 | `	/* Point to the internal representation of the input hashmaps */` |
|        14 | 6191 | `	pKey   = (ph7_hashmap *)apArg[0]->x.pOther;` |
|        14 | 6192 | `	pValue = (ph7_hashmap *)apArg[1]->x.pOther;` |
|        14 | 6193 | `	if( pKey->nEntry != pValue->nEntry ){` |
|         - | 6194 | `		/* Length mismatch -> ValueError */` |
|         3 | 6195 | `		return PH7_VmThrowException(pCtx,` |
|         - | 6196 | `			"ValueError",` |
|         - | 6197 | `			"array_combine(): Argument #1 ($keys) and argument #2 ($values) must have the same number of elements"` |
|         - | 6198 | `			);` |
|         - | 6199 | `	}` |
|         - | 6200 | `	/* Create a new array */` |
|        11 | 6201 | `	pArray = ph7_context_new_array(pCtx);` |
|        11 | 6202 | `	if( pArray == 0 ){` |
|       ! 0 | 6203 | `		ph7_result_bool(pCtx,0);` |
|       ! 0 | 6204 | `		return PH7_OK;` |
|         - | 6205 | `	}` |
|         - | 6206 | `	/* Perform the requested operation */` |
|        11 | 6207 | `	pKe = pKey->pFirst;` |
|        11 | 6208 | `	pVe = pValue->pFirst;` |
|        33 | 6209 | `	for( n = 0 ; n < pKey->nEntry ; n++ ){` |
|        23 | 6210 | `		ph7_value *pKeyVal = HashmapExtractNodeValue(pKe);` |
|        23 | 6211 | `		ph7_value *pValVal = HashmapExtractNodeValue(pVe);` |
|         - | 6212 | `		/* PHP treats floats used as keys in array_combine differently than` |
|         - | 6213 | `		 * ordinary offset access: the float is stringified rather than` |
|         - | 6214 | `		 * truncated.  To emulate this behavior we create a temporary copy of` |
|         - | 6215 | `		 * the value when it is a float and convert the copy to string.  The` |
|         - | 6216 | `		 * original array must not be mutated. */` |
|        23 | 6217 | `		ph7_value *pKeyCopy = pKeyVal;` |
|        23 | 6218 | `		if( ph7_value_is_float(pKeyVal) ){` |
|         5 | 6219 | `			ph7_value *pTmpKey = ph7_context_new_scalar(pCtx);` |
|         5 | 6220 | `			if( pTmpKey ){` |
|         5 | 6221 | `				PH7_MemObjStore(pKeyVal,pTmpKey);` |
|         - | 6222 | `				/* Convert copy to string so it becomes "1.5" or "2" etc. */` |
|         5 | 6223 | `				PH7_MemObjToString(pTmpKey);` |
|         5 | 6224 | `				pKeyCopy = pTmpKey;` |
|         2 | 6225 | `			}` |
|         2 | 6226 | `		}` |
|        23 | 6227 | `		ph7_array_add_elem(pArray,pKeyCopy,pValVal);` |
|         - | 6228 | `		/* Point to the next entry */` |
|        23 | 6229 | `		pKe = pKe->pPrev; /* Reverse link */` |
|        23 | 6230 | `		pVe = pVe->pPrev;` |
|        12 | 6231 | `	}` |
|         - | 6232 | `	/* Return the filled array */` |
|        11 | 6233 | `	ph7_result_value(pCtx,pArray);` |
|        11 | 6234 | `	return PH7_OK;` |
|        14 | 6235 | `}` |
|         - | 6236 | `/*` |
|         - | 6237 | ` * array array_reverse(array $array [,bool $preserve_keys = false ])` |
|         - | 6238 | ` *  Return an array with elements in reverse order.` |
|         - | 6239 | ` * Parameters` |
|         - | 6240 | ` *  $array` |
|         - | 6241 | ` *   The input array.` |
|         - | 6242 | ` *  $preserve_keys (optional)` |
|         - | 6243 | ` *   If set to TRUE keys are preserved.` |
|         - | 6244 | ` * Return` |
|         - | 6245 | ` *  The reversed array.` |
|         - | 6246 | ` */` |
|        20 | 6247 | `static int ph7_hashmap_reverse(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|         3 | 6248 | `{` |
|         - | 6249 | `	ph7_hashmap_node *pEntry;` |
|         - | 6250 | `	ph7_hashmap *pSrc;` |
|         - | 6251 | `	ph7_value *pArray;` |
|         - | 6252 | `	int bPreserve;` |
|         - | 6253 | `	sxu32 n;` |
|        23 | 6254 | `	if( nArg < 1 ){` |
|         4 | 6255 | `		return PH7_VmThrowException(pCtx,` |
|         - | 6256 | `			"ArgumentCountError",` |
|         - | 6257 | `			"array_reverse() expects at least 1 argument, %d given",` |
|         1 | 6258 | `			nArg` |
|         - | 6259 | `			);` |
|         - | 6260 | `	}` |
|         - | 6261 | `	/* Make sure we are dealing with a valid hashmap */` |
|        20 | 6262 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|         4 | 6263 | `		return PH7_VmThrowException(pCtx,` |
|         - | 6264 | `			"TypeError",` |
|         - | 6265 | `			"array_reverse(): Argument #1 ($array) must be of type array, %s given",` |
|         1 | 6266 | `			ph7_type_name(apArg[0])` |
|         - | 6267 | `			);` |
|         - | 6268 | `	}` |
|        17 | 6269 | `	bPreserve = FALSE;` |
|        17 | 6270 | `	if( nArg > 1 ){` |
|         7 | 6271 | `		bPreserve = ph7_value_to_bool(apArg[1]);` |
|         3 | 6272 | `	}` |
|         - | 6273 | `	/* Point to the internal representation of the input hashmap */` |
|        17 | 6274 | `	pSrc = (ph7_hashmap *)apArg[0]->x.pOther;` |
|         - | 6275 | `	/* Create a new array */` |
|        17 | 6276 | `	pArray = ph7_context_new_array(pCtx);` |
|        17 | 6277 | `	if( pArray == 0 ){` |
|       ! 0 | 6278 | `		ph7_result_null(pCtx);` |
|       ! 0 | 6279 | `		return PH7_OK;` |
|         - | 6280 | `	}` |
|         - | 6281 | `	/* Perform the requested operation */` |
|        17 | 6282 | `	pEntry = pSrc->pLast;` |
|        55 | 6283 | `	for( n = 0 ; n < pSrc->nEntry ; n++ ){` |
|         - | 6284 | `		/* String keys are always preserved; preserve_keys only affects int keys */` |
|        39 | 6285 | `		int bKeep = (pEntry->iType == HASHMAP_INT_NODE) ? bPreserve : TRUE;` |
|        39 | 6286 | `		HashmapInsertNode((ph7_hashmap *)pArray->x.pOther,pEntry,bKeep);` |
|         - | 6287 | `		/* Point to the previous entry */` |
|        39 | 6288 | `		pEntry = pEntry->pNext; /* Reverse link */` |
|        20 | 6289 | `	}` |
|        17 | 6290 | `	ph7_result_value(pCtx,pArray);` |
|        17 | 6291 | `	return PH7_OK;` |
|        13 | 6292 | `}` |
|         - | 6293 | `/*` |
|         - | 6294 | ` * array array_unique(array $array, int $flags = SORT_STRING)` |
|         - | 6295 | ` *  Removes duplicate values from an array.` |
|         - | 6296 | ` * Parameters` |
|         - | 6297 | ` *  $array` |
|         - | 6298 | ` *   The input array.` |
|         - | 6299 | ` *  $flags` |
|         - | 6300 | ` *   The optional second parameter may be used to modify the comparison` |
|         - | 6301 | ` *   behavior using these values:` |
|         - | 6302 | ` *     SORT_REGULAR - compare items normally (don't change types)` |
|         - | 6303 | ` *     SORT_NUMERIC - compare items numerically` |
|         - | 6304 | ` *     SORT_STRING  - compare items as strings` |
|         - | 6305 | ` * Return` |
|         - | 6306 | ` *  The filtered array.` |
|         - | 6307 | ` */` |
|        24 | 6308 | `static int ph7_hashmap_unique(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|         4 | 6309 | `{` |
|         - | 6310 | `	ph7_hashmap_node *pEntry;` |
|         - | 6311 | `	ph7_value *pNeedle;` |
|         - | 6312 | `	ph7_hashmap *pSrc;` |
|         - | 6313 | `	ph7_value *pArray;` |
|         - | 6314 | `	int bStrict;` |
|         - | 6315 | `	sxi32 rc;` |
|         - | 6316 | `	sxu32 n;` |
|        28 | 6317 | `	if( nArg < 1 ){` |
|         - | 6318 | `		/* Missing arguments, throw ArgumentCountError */` |
|         3 | 6319 | `		return PH7_VmThrowException(pCtx,` |
|         - | 6320 | `			"ArgumentCountError",` |
|         - | 6321 | `			"array_unique() expects at least 1 argument, 0 given"` |
|         - | 6322 | `			);` |
|         - | 6323 | `	}` |
|        25 | 6324 | `	if( nArg > 2 ){` |
|         - | 6325 | `		/* Too many arguments, throw ArgumentCountError */` |
|         4 | 6326 | `		return PH7_VmThrowException(pCtx,` |
|         - | 6327 | `			"ArgumentCountError",` |
|         - | 6328 | `			"array_unique() expects at most 2 arguments, %d given",` |
|         1 | 6329 | `			nArg` |
|         - | 6330 | `			);` |
|         - | 6331 | `	}` |
|         - | 6332 | `	/* Make sure we are dealing with a valid hashmap */` |
|        22 | 6333 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|         - | 6334 | `		/* Type mismatch, throw TypeError */` |
|         4 | 6335 | `		return PH7_VmThrowException(pCtx,` |
|         - | 6336 | `			"TypeError",` |
|         - | 6337 | `			"array_unique(): Argument #1 ($array) must be of type array, %s given",` |
|         1 | 6338 | `			ph7_type_name(apArg[0])` |
|         - | 6339 | `			);` |
|         - | 6340 | `	}` |
|        19 | 6341 | `	bStrict = FALSE;` |
|         - | 6342 | `	/* Point to the internal representation of the input hashmap */` |
|        19 | 6343 | `	pSrc = (ph7_hashmap *)apArg[0]->x.pOther;` |
|         - | 6344 | `	/* Create a new array */` |
|        19 | 6345 | `	pArray = ph7_context_new_array(pCtx);` |
|        19 | 6346 | `	if( pArray == 0 ){` |
|       ! 0 | 6347 | `		ph7_result_null(pCtx);` |
|       ! 0 | 6348 | `		return PH7_OK;` |
|         - | 6349 | `	}` |
|         - | 6350 | `	/* Perform the requested operation */` |
|        19 | 6351 | `	pEntry = pSrc->pFirst;` |
|        83 | 6352 | `	for( n = 0 ; n < pSrc->nEntry ; n++ ){` |
|        65 | 6353 | `		pNeedle = HashmapExtractNodeValue(pEntry);` |
|        65 | 6354 | `		rc = SXERR_NOTFOUND;` |
|        65 | 6355 | `		if( pNeedle ){` |
|        65 | 6356 | `			rc = HashmapFindValue((ph7_hashmap *)pArray->x.pOther,pNeedle,0,bStrict);` |
|        32 | 6357 | `		}` |
|        65 | 6358 | `		if( rc != SXRET_OK ){` |
|         - | 6359 | `			/* Perform the insertion */` |
|        37 | 6360 | `			HashmapInsertNode((ph7_hashmap *)pArray->x.pOther,pEntry,TRUE);` |
|        18 | 6361 | `		}` |
|         - | 6362 | `		/* Point to the next entry */` |
|        65 | 6363 | `		pEntry = pEntry->pPrev; /* Reverse link */` |
|        33 | 6364 | `	}` |
|         - | 6365 | `	/* Return the freshly created array */` |
|        19 | 6366 | `	ph7_result_value(pCtx,pArray);` |
|        19 | 6367 | `	return PH7_OK;` |
|        16 | 6368 | `}` |
|         - | 6369 | `/*` |
|         - | 6370 | ` * array array_flip(array $input)` |
|         - | 6371 | ` *  Exchanges all keys with their associated values in an array.` |
|         - | 6372 | ` * Parameter` |
|         - | 6373 | ` *  $input` |
|         - | 6374 | ` *   Input array.` |
|         - | 6375 | ` * Return` |
|         - | 6376 | ` *   The flipped array on success or NULL on failure.` |
|         - | 6377 | ` */` |
|        34 | 6378 | `static int ph7_hashmap_flip(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|         5 | 6379 | `{` |
|         - | 6380 | `	ph7_hashmap_node *pEntry;` |
|         - | 6381 | `	ph7_hashmap *pSrc;` |
|         - | 6382 | `	ph7_value *pArray;` |
|         - | 6383 | `	ph7_value *pKey;` |
|         - | 6384 | `	ph7_value sVal;` |
|         - | 6385 | `	sxu32 n;` |
|         - | 6386 |  |
|         - | 6387 | `	/* PHP requires exactly one argument */` |
|        39 | 6388 | `	if( nArg != 1 ){` |
|         - | 6389 | `		/* Use ArgumentCountError like other array helpers */` |
|         8 | 6390 | `		return PH7_VmThrowException(pCtx,` |
|         - | 6391 | `			"ArgumentCountError",` |
|         - | 6392 | `			"array_flip() expects exactly 1 argument, %d given",` |
|         2 | 6393 | `			nArg` |
|         - | 6394 | `			);` |
|         - | 6395 | `	}` |
|         - | 6396 | `	/* Make sure we are dealing with a valid hashmap */` |
|        33 | 6397 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|         - | 6398 | `		/* Type mismatch -> TypeError */` |
|         8 | 6399 | `		return PH7_VmThrowException(pCtx,` |
|         - | 6400 | `			"TypeError",` |
|         - | 6401 | `			"array_flip(): Argument #1 ($array) must be of type array, %s given",` |
|         2 | 6402 | `			ph7_type_name(apArg[0])` |
|         - | 6403 | `			);` |
|         - | 6404 | `	}` |
|         - | 6405 | `	/* Point to the internal representation of the input hashmap */` |
|        27 | 6406 | `	pSrc = (ph7_hashmap *)apArg[0]->x.pOther;` |
|         - | 6407 | `	/* Create a new array */` |
|        27 | 6408 | `	pArray = ph7_context_new_array(pCtx);` |
|        27 | 6409 | `	if( pArray == 0 ){` |
|       ! 0 | 6410 | `		ph7_result_null(pCtx);` |
|       ! 0 | 6411 | `		return PH7_OK;` |
|         - | 6412 | `	}` |
|         - | 6413 | `	/* Start processing */` |
|        27 | 6414 | `	pEntry = pSrc->pFirst;` |
|     22263 | 6415 | `	for( n = 0 ; n < pSrc->nEntry ; n++ ){` |
|         - | 6416 | `		/* Extract the node value (will become a key in the result) */` |
|     22237 | 6417 | `		pKey = HashmapExtractNodeValue(pEntry);` |
|     22237 | 6418 | `		if( pKey ){` |
|         - | 6419 | `			/* NULL values are not valid keys either, PHP emits a warning */` |
|     22237 | 6420 | `			if( pKey->iFlags & MEMOBJ_NULL ){` |
|         3 | 6421 | `				PH7_VmThrowError(pCtx->pVm,0,PH7_CTX_WARNING,` |
|         - | 6422 | `					"array_flip(): Can only flip string and integer values, entry skipped"` |
|         - | 6423 | `					);` |
|     22236 | 6424 | `			}else if( (pKey->iFlags & MEMOBJ_INT) \|\| (pKey->iFlags & MEMOBJ_STRING) ){` |
|         - | 6425 | `				/* Prepare the value for insertion (original key) */` |
|     22227 | 6426 | `				if( pEntry->iType == HASHMAP_INT_NODE ){` |
|     20001 | 6427 | `					PH7_MemObjInitFromInt(pSrc->pVm,&sVal,pEntry->xKey.iKey);` |
|     10001 | 6428 | `				}else{` |
|         - | 6429 | `					SyString sStr;` |
|      2227 | 6430 | `					SyStringInitFromBuf(&sStr,SyBlobData(&pEntry->xKey.sKey),SyBlobLength(&pEntry->xKey.sKey));` |
|      2227 | 6431 | `					PH7_MemObjInitFromString(pSrc->pVm,&sVal,&sStr);` |
|         - | 6432 | `				}` |
|         - | 6433 | `				/* Perform the insertion */` |
|     22227 | 6434 | `				ph7_array_add_elem(pArray,pKey,&sVal);` |
|         - | 6435 | `				/* Safely release the value because each inserted entry` |
|         - | 6436 | `				 * has its own private copy of the value.` |
|         - | 6437 | `				 */` |
|     22227 | 6438 | `				PH7_MemObjRelease(&sVal);` |
|     11114 | 6439 | `			}else{` |
|         - | 6440 | `				/* Unsupported value type -> emit warning and skip the entry */` |
|         9 | 6441 | `				PH7_VmThrowError(pCtx->pVm,0,PH7_CTX_WARNING,` |
|         - | 6442 | `					"array_flip(): Can only flip string and integer values, entry skipped"` |
|         - | 6443 | `					);` |
|         - | 6444 | `			}` |
|     11118 | 6445 | `		}` |
|         - | 6446 | `		/* Point to the next entry */` |
|     22237 | 6447 | `		pEntry = pEntry->pPrev; /* Reverse link */` |
|     11119 | 6448 | `	}` |
|         - | 6449 | `	/* Return the freshly created array */` |
|        27 | 6450 | `	ph7_result_value(pCtx,pArray);` |
|        27 | 6451 | `	return PH7_OK;` |
|        22 | 6452 | `}` |
|         - | 6453 | `/*` |
|         - | 6454 | ` * number array_sum(array $array )` |
|         - | 6455 | ` *  Calculate the sum of values in an array.` |
|         - | 6456 | ` * Parameters` |
|         - | 6457 | ` *  $array: The input array.` |
|         - | 6458 | ` * Return` |
|         - | 6459 | ` *  Returns the sum of values as an integer or float.` |
|         - | 6460 | ` */` |
|        24 | 6461 | `static void DoubleSum(ph7_context *pCtx,ph7_hashmap *pMap)` |
|         1 | 6462 | `{` |
|         - | 6463 | `	ph7_hashmap_node *pEntry;` |
|         - | 6464 | `	ph7_value *pObj;` |
|        25 | 6465 | `	double dSum = 0;` |
|         - | 6466 | `	sxu32 n;` |
|        25 | 6467 | `	pEntry = pMap->pFirst;` |
|        91 | 6468 | `	for( n = 0 ; n < pMap->nEntry ; n++ ){` |
|        67 | 6469 | `		pObj = HashmapExtractNodeValue(pEntry);` |
|        67 | 6470 | `		if( pObj ){` |
|        67 | 6471 | `			if( pObj->iFlags & MEMOBJ_REAL ){` |
|        29 | 6472 | `				dSum += pObj->rVal;` |
|        53 | 6473 | `			}else if( pObj->iFlags & (MEMOBJ_INT\|MEMOBJ_BOOL) ){` |
|        21 | 6474 | `				dSum += (double)pObj->x.iVal;` |
|        29 | 6475 | `			}else if( pObj->iFlags & MEMOBJ_STRING ){` |
|        15 | 6476 | `				if( SyBlobLength(&pObj->sBlob) > 0 ){` |
|        13 | 6477 | `					double dv = 0;` |
|        13 | 6478 | `					SyStrToReal((const char *)SyBlobData(&pObj->sBlob),SyBlobLength(&pObj->sBlob),(void *)&dv,0);` |
|        13 | 6479 | `					dSum += dv;` |
|         7 | 6480 | `				}` |
|        12 | 6481 | `			}else if( pObj->iFlags & MEMOBJ_HASHMAP ){` |
|         3 | 6482 | `				PH7_VmThrowError(pCtx->pVm,0,PH7_CTX_WARNING,` |
|         - | 6483 | `					"array_sum(): Addition is not supported on type array");` |
|         4 | 6484 | `			}else if( pObj->iFlags & MEMOBJ_OBJ ){` |
|       ! 0 | 6485 | `				PH7_VmThrowError(pCtx->pVm,0,PH7_CTX_WARNING,` |
|         - | 6486 | `					"array_sum(): Addition is not supported on type object");` |
|         3 | 6487 | `			}else if( pObj->iFlags & MEMOBJ_RES ){` |
|       ! 0 | 6488 | `				PH7_VmThrowError(pCtx->pVm,0,PH7_CTX_WARNING,` |
|         - | 6489 | `					"array_sum(): Addition is not supported on type resource");` |
|       ! 0 | 6490 | `			}` |
|         - | 6491 | `			/* NULL is silently treated as 0 (matches PHP) */` |
|        33 | 6492 | `		}` |
|         - | 6493 | `		/* Point to the next entry */` |
|        67 | 6494 | `		pEntry = pEntry->pPrev; /* Reverse link */` |
|        34 | 6495 | `	}` |
|         - | 6496 | `	/* Return sum */` |
|        25 | 6497 | `	ph7_result_double(pCtx,dSum);` |
|        25 | 6498 | `}` |
|        32 | 6499 | `static void Int64Sum(ph7_context *pCtx,ph7_hashmap *pMap)` |
|         2 | 6500 | `{` |
|         - | 6501 | `	ph7_hashmap_node *pEntry;` |
|         - | 6502 | `	ph7_value *pObj;` |
|        34 | 6503 | `	sxi64 nSum = 0;` |
|         - | 6504 | `	sxu32 n;` |
|        34 | 6505 | `	pEntry = pMap->pFirst;` |
|       136 | 6506 | `	for( n = 0 ; n < pMap->nEntry ; n++ ){` |
|       104 | 6507 | `		pObj = HashmapExtractNodeValue(pEntry);` |
|       104 | 6508 | `		if( pObj ){` |
|       104 | 6509 | `			if( pObj->iFlags & (MEMOBJ_INT\|MEMOBJ_BOOL) ){` |
|        94 | 6510 | `				nSum += pObj->x.iVal;` |
|        57 | 6511 | `			}else if( pObj->iFlags & MEMOBJ_STRING ){` |
|         7 | 6512 | `				if( SyBlobLength(&pObj->sBlob) > 0 ){` |
|         5 | 6513 | `					sxi64 nv = 0;` |
|         5 | 6514 | `					SyStrToInt64((const char *)SyBlobData(&pObj->sBlob),SyBlobLength(&pObj->sBlob),(void *)&nv,0);` |
|         5 | 6515 | `					nSum += nv;` |
|         3 | 6516 | `				}` |
|         8 | 6517 | `			}else if( pObj->iFlags & MEMOBJ_HASHMAP ){` |
|         3 | 6518 | `				PH7_VmThrowError(pCtx->pVm,0,PH7_CTX_WARNING,` |
|         - | 6519 | `					"array_sum(): Addition is not supported on type array");` |
|         4 | 6520 | `			}else if( pObj->iFlags & MEMOBJ_OBJ ){` |
|       ! 0 | 6521 | `				PH7_VmThrowError(pCtx->pVm,0,PH7_CTX_WARNING,` |
|         - | 6522 | `					"array_sum(): Addition is not supported on type object");` |
|         3 | 6523 | `			}else if( pObj->iFlags & MEMOBJ_RES ){` |
|       ! 0 | 6524 | `				PH7_VmThrowError(pCtx->pVm,0,PH7_CTX_WARNING,` |
|         - | 6525 | `					"array_sum(): Addition is not supported on type resource");` |
|       ! 0 | 6526 | `			}` |
|         - | 6527 | `			/* NULL is silently treated as 0 (matches PHP) */` |
|        51 | 6528 | `		}` |
|         - | 6529 | `		/* Point to the next entry */` |
|       104 | 6530 | `		pEntry = pEntry->pPrev; /* Reverse link */` |
|        53 | 6531 | `	}` |
|         - | 6532 | `	/* Return sum */` |
|        34 | 6533 | `	ph7_result_int64(pCtx,nSum);` |
|        34 | 6534 | `}` |
|         - | 6535 | `/* number array_sum(array $array )` |
|         - | 6536 | ` * (See block-coment above)` |
|         - | 6537 | ` */` |
|        70 | 6538 | `static int ph7_hashmap_sum(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|         5 | 6539 | `{` |
|         - | 6540 | `	ph7_hashmap_node *pEntry;` |
|         - | 6541 | `	ph7_hashmap *pMap;` |
|         - | 6542 | `	ph7_value *pObj;` |
|        75 | 6543 | `	int useDouble = 0;` |
|         - | 6544 | `	sxu32 n;` |
|         - | 6545 | `	/* PHP requires exactly one argument */` |
|        75 | 6546 | `	if( nArg != 1 ){` |
|         8 | 6547 | `		return PH7_VmThrowException(pCtx,` |
|         - | 6548 | `			"ArgumentCountError",` |
|         - | 6549 | `			"array_sum() expects exactly 1 argument, %d given",` |
|         2 | 6550 | `			nArg` |
|         - | 6551 | `			);` |
|         - | 6552 | `	}` |
|         - | 6553 | `	/* Make sure we are dealing with a valid hashmap */` |
|        70 | 6554 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|         - | 6555 | `		/* Type mismatch -> TypeError (php's true/false/class-name convention). */` |
|         - | 6556 | `		char zBuf[64];` |
|         8 | 6557 | `		return PH7_VmThrowException(pCtx,` |
|         - | 6558 | `			"TypeError",` |
|         - | 6559 | `			"array_sum(): Argument #1 ($array) must be of type array, %s given",` |
|         2 | 6560 | `			VmValueGivenName(apArg[0],zBuf,sizeof(zBuf))` |
|         - | 6561 | `			);` |
|         - | 6562 | `	}` |
|        64 | 6563 | `	pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|        64 | 6564 | `	if( pMap->nEntry < 1 ){` |
|         - | 6565 | `		/* Nothing to compute,return 0 */` |
|         7 | 6566 | `		ph7_result_int(pCtx,0);` |
|         7 | 6567 | `		return PH7_OK;` |
|         - | 6568 | `	}` |
|         - | 6569 | `	/* Scan all elements: if any value is a float, use floating-point` |
|         - | 6570 | `	 * arithmetic for the entire sum (matches PHP behaviour).` |
|         - | 6571 | `	 */` |
|        58 | 6572 | `	pEntry = pMap->pFirst;` |
|       168 | 6573 | `	for( n = 0 ; n < pMap->nEntry ; n++ ){` |
|       136 | 6574 | `		pObj = HashmapExtractNodeValue(pEntry);` |
|       136 | 6575 | `		if( pObj ){` |
|       136 | 6576 | `			if( pObj->iFlags & MEMOBJ_REAL ){` |
|        19 | 6577 | `				useDouble = 1;` |
|        19 | 6578 | `				break;` |
|         - | 6579 | `			}` |
|       118 | 6580 | `			if( pObj->iFlags & MEMOBJ_STRING ){` |
|        13 | 6581 | `				const char *zStr = (const char *)SyBlobData(&pObj->sBlob);` |
|        13 | 6582 | `				sxu32 nLen = SyBlobLength(&pObj->sBlob);` |
|         - | 6583 | `				sxu32 i;` |
|        23 | 6584 | `				for( i = 0 ; i < nLen ; i++ ){` |
|        17 | 6585 | `					if( zStr[i] == '.' \|\| zStr[i] == 'e' \|\| zStr[i] == 'E' ){` |
|         7 | 6586 | `						useDouble = 1;` |
|         7 | 6587 | `						break;` |
|         - | 6588 | `					}` |
|         6 | 6589 | `				}` |
|        13 | 6590 | `				if( useDouble ){` |
|         7 | 6591 | `					break;` |
|         - | 6592 | `				}` |
|         3 | 6593 | `			}` |
|        55 | 6594 | `		}` |
|       112 | 6595 | `		pEntry = pEntry->pPrev;` |
|        57 | 6596 | `	}` |
|        58 | 6597 | `	if( useDouble ){` |
|        25 | 6598 | `		DoubleSum(pCtx,pMap);` |
|        13 | 6599 | `	}else{` |
|        34 | 6600 | `		Int64Sum(pCtx,pMap);` |
|         - | 6601 | `	}` |
|        58 | 6602 | `	return PH7_OK;` |
|        40 | 6603 | `}` |
|         - | 6604 | `/*` |
|         - | 6605 | ` * number array_product(array $array )` |
|         - | 6606 | ` *  Calculate the product of values in an array.` |
|         - | 6607 | ` * Parameters` |
|         - | 6608 | ` *  $array: The input array.` |
|         - | 6609 | ` * Return` |
|         - | 6610 | ` *  Returns the product of values as an integer or float.` |
|         - | 6611 | ` */` |
|         2 | 6612 | `static void DoubleProd(ph7_context *pCtx,ph7_hashmap *pMap)` |
|         1 | 6613 | `{` |
|         - | 6614 | `	ph7_hashmap_node *pEntry;` |
|         - | 6615 | `	ph7_value *pObj;` |
|         - | 6616 | `	double dProd;` |
|         - | 6617 | `	sxu32 n;` |
|         3 | 6618 | `	pEntry = pMap->pFirst;` |
|         3 | 6619 | `	dProd = 1;` |
|         7 | 6620 | `	for( n = 0 ; n < pMap->nEntry ; n++ ){` |
|         5 | 6621 | `		pObj = HashmapExtractNodeValue(pEntry);` |
|         5 | 6622 | `		if( pObj && (pObj->iFlags & (MEMOBJ_NULL\|MEMOBJ_HASHMAP\|MEMOBJ_OBJ\|MEMOBJ_RES)) == 0){` |
|         5 | 6623 | `			if( pObj->iFlags & MEMOBJ_REAL ){` |
|         3 | 6624 | `				dProd *= pObj->rVal;` |
|         4 | 6625 | `			}else if( pObj->iFlags & (MEMOBJ_INT\|MEMOBJ_BOOL) ){` |
|         3 | 6626 | `				dProd *= (double)pObj->x.iVal;` |
|         1 | 6627 | `			}else if( pObj->iFlags & MEMOBJ_STRING ){` |
|       ! 0 | 6628 | `				if( SyBlobLength(&pObj->sBlob) > 0 ){` |
|       ! 0 | 6629 | `					double dv = 0;` |
|       ! 0 | 6630 | `					SyStrToReal((const char *)SyBlobData(&pObj->sBlob),SyBlobLength(&pObj->sBlob),(void *)&dv,0);` |
|       ! 0 | 6631 | `					dProd *= dv;` |
|       ! 0 | 6632 | `				}` |
|       ! 0 | 6633 | `			}` |
|         2 | 6634 | `		}` |
|         - | 6635 | `		/* Point to the next entry */` |
|         5 | 6636 | `		pEntry = pEntry->pPrev; /* Reverse link */` |
|         3 | 6637 | `	}` |
|         - | 6638 | `	/* Return product */` |
|         3 | 6639 | `	ph7_result_double(pCtx,dProd);` |
|         3 | 6640 | `}` |
|         2 | 6641 | `static void Int64Prod(ph7_context *pCtx,ph7_hashmap *pMap)` |
|         1 | 6642 | `{` |
|         - | 6643 | `	ph7_hashmap_node *pEntry;` |
|         - | 6644 | `	ph7_value *pObj;` |
|         - | 6645 | `	sxi64 nProd;` |
|         - | 6646 | `	sxu32 n;` |
|         3 | 6647 | `	pEntry = pMap->pFirst;` |
|         3 | 6648 | `	nProd = 1;` |
|         9 | 6649 | `	for( n = 0 ; n < pMap->nEntry ; n++ ){` |
|         7 | 6650 | `		pObj = HashmapExtractNodeValue(pEntry);` |
|         7 | 6651 | `		if( pObj && (pObj->iFlags & (MEMOBJ_NULL\|MEMOBJ_HASHMAP\|MEMOBJ_OBJ\|MEMOBJ_RES)) == 0){` |
|         7 | 6652 | `			if( pObj->iFlags & MEMOBJ_REAL ){` |
|       ! 0 | 6653 | `				nProd *= (sxi64)pObj->rVal;` |
|         7 | 6654 | `			}else if( pObj->iFlags & (MEMOBJ_INT\|MEMOBJ_BOOL) ){` |
|         7 | 6655 | `				nProd *= pObj->x.iVal;` |
|         3 | 6656 | `			}else if( pObj->iFlags & MEMOBJ_STRING ){` |
|       ! 0 | 6657 | `				if( SyBlobLength(&pObj->sBlob) > 0 ){` |
|       ! 0 | 6658 | `					sxi64 nv = 0;` |
|       ! 0 | 6659 | `					SyStrToInt64((const char *)SyBlobData(&pObj->sBlob),SyBlobLength(&pObj->sBlob),(void *)&nv,0);` |
|       ! 0 | 6660 | `					nProd *= nv;` |
|       ! 0 | 6661 | `				}` |
|       ! 0 | 6662 | `			}` |
|         3 | 6663 | `		}` |
|         - | 6664 | `		/* Point to the next entry */` |
|         7 | 6665 | `		pEntry = pEntry->pPrev; /* Reverse link */` |
|         4 | 6666 | `	}` |
|         - | 6667 | `	/* Return product */` |
|         3 | 6668 | `	ph7_result_int64(pCtx,nProd);` |
|         3 | 6669 | `}` |
|         - | 6670 | `/* number array_product(array $array )` |
|         - | 6671 | ` * (See block-block comment above)` |
|         - | 6672 | ` */` |
|        18 | 6673 | `static int ph7_hashmap_product(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|         1 | 6674 | `{` |
|         - | 6675 | `	ph7_hashmap *pMap;` |
|         - | 6676 | `	ph7_value *pObj;` |
|        19 | 6677 | `	if( nArg < 1 ){` |
|         - | 6678 | `		/* Missing arguments (arity is enforced upstream; defensive). */` |
|       ! 0 | 6679 | `		ph7_result_int(pCtx,1);` |
|       ! 0 | 6680 | `		return PH7_OK;` |
|         - | 6681 | `	}` |
|         - | 6682 | `	/* PHP 8: a non-array $array is a catchable TypeError, not a silent 0. */` |
|        19 | 6683 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|         - | 6684 | `		char zBuf[64];` |
|        19 | 6685 | `		return PH7_VmThrowException(pCtx,` |
|         - | 6686 | `			"TypeError",` |
|         - | 6687 | `			"array_product(): Argument #1 ($array) must be of type array, %s given",` |
|         6 | 6688 | `			VmValueGivenName(apArg[0],zBuf,sizeof(zBuf))` |
|         - | 6689 | `			);` |
|         - | 6690 | `	}` |
|         7 | 6691 | `	pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|         7 | 6692 | `	if( pMap->nEntry < 1 ){` |
|         - | 6693 | `		/* The product of an empty array is the multiplicative identity 1 (PHP). */` |
|         3 | 6694 | `		ph7_result_int(pCtx,1);` |
|         3 | 6695 | `		return PH7_OK;` |
|         - | 6696 | `	}` |
|         - | 6697 | `	/* If the first element is of type float,then perform floating` |
|         - | 6698 | `	 * point computaion.Otherwise switch to int64 computaion.` |
|         - | 6699 | `	 */` |
|         5 | 6700 | `	pObj = HashmapExtractNodeValue(pMap->pFirst);` |
|         5 | 6701 | `	if( pObj == 0 ){` |
|       ! 0 | 6702 | `		ph7_result_int(pCtx,0);` |
|       ! 0 | 6703 | `		return PH7_OK;` |
|         - | 6704 | `	}` |
|         5 | 6705 | `	if( pObj->iFlags & MEMOBJ_REAL ){` |
|         3 | 6706 | `		DoubleProd(pCtx,pMap);` |
|         2 | 6707 | `	}else{` |
|         3 | 6708 | `		Int64Prod(pCtx,pMap);` |
|         - | 6709 | `	}` |
|         5 | 6710 | `	return PH7_OK;` |
|        10 | 6711 | `}` |
|         - | 6712 | `/*` |
|         - | 6713 | ` * value array_rand(array $input[,int $num_req = 1 ])` |
|         - | 6714 | ` *  Pick one or more random entries out of an array.` |
|         - | 6715 | ` * Parameters` |
|         - | 6716 | ` * $input` |
|         - | 6717 | ` *  The input array.` |
|         - | 6718 | ` * $num_req` |
|         - | 6719 | ` *  Specifies how many entries you want to pick.` |
|         - | 6720 | ` * Return` |
|         - | 6721 | ` *  If you are picking only one entry, array_rand() returns the key for a random entry.` |
|         - | 6722 | ` *  Otherwise, it returns an array of keys for the random entries.` |
|         - | 6723 | ` *  NULL is returned on failure.` |
|         - | 6724 | ` */` |
|        42 | 6725 | `static int ph7_hashmap_rand(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|         1 | 6726 | `{` |
|         - | 6727 | `	ph7_hashmap_node *pNode;` |
|         - | 6728 | `	ph7_hashmap *pMap;` |
|        43 | 6729 | `	int nItem = 1;` |
|        43 | 6730 | `	if( nArg < 1 ){` |
|         - | 6731 | `		/* Missing argument (arity is enforced upstream; defensive) */` |
|       ! 0 | 6732 | `		ph7_result_null(pCtx);` |
|       ! 0 | 6733 | `		return PH7_OK;` |
|         - | 6734 | `	}` |
|         - | 6735 | `	/* php 8: $array must be an array (TypeError, not a silent NULL return) */` |
|        43 | 6736 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|         - | 6737 | `		char zBuf[64];` |
|        10 | 6738 | `		return PH7_VmThrowException(pCtx,` |
|         - | 6739 | `			"TypeError",` |
|         - | 6740 | `			"array_rand(): Argument #1 ($array) must be of type array, %s given",` |
|         3 | 6741 | `			VmValueGivenName(apArg[0],zBuf,sizeof(zBuf))` |
|         - | 6742 | `			);` |
|         - | 6743 | `	}` |
|         - | 6744 | `	/* php validates $num (and weak-coerces it) BEFORE the empty-array body` |
|         - | 6745 | `	 * check, matching its ZPP-before-body ordering. */` |
|        37 | 6746 | `	if( nArg > 1 ){` |
|        29 | 6747 | `		ph7_value *pNum = apArg[1];` |
|        28 | 6748 | `		if( ph7_value_is_array(pNum) \|\| ph7_value_is_object(pNum)` |
|        24 | 6749 | `			\|\| ph7_value_is_resource(pNum) ){` |
|         - | 6750 | `			char zBuf[64];` |
|        10 | 6751 | `			return PH7_VmThrowException(pCtx,` |
|         - | 6752 | `				"TypeError",` |
|         - | 6753 | `				"array_rand(): Argument #2 ($num) must be of type int, %s given",` |
|         3 | 6754 | `				VmValueGivenName(pNum,zBuf,sizeof(zBuf))` |
|         - | 6755 | `				);` |
|         - | 6756 | `		}` |
|        23 | 6757 | `		if( ph7_value_is_string(pNum) ){` |
|         - | 6758 | `			/* Weak int coercion of a string $num follows php's numeric-string` |
|         - | 6759 | `			 * grammar (whole string, int or float): a non-numeric string` |
|         - | 6760 | `			 * (incl. leading-numeric junk like "2abc" or "0x1A") is a TypeError,` |
|         - | 6761 | `			 * a well-formed float-string ("1e3") coerces like a float value.` |
|         - | 6762 | `			 * Reuses the range() ZPP number parser (§3.9 shared-helper note). */` |
|         - | 6763 | `			int len;` |
|         9 | 6764 | `			const char *zStr = ph7_value_to_string(pNum, &len);` |
|         - | 6765 | `			sxi64 iLong; double dReal;` |
|         9 | 6766 | `			sxu8 iKind = RangeStrToNumber(zStr, (sxu32)len, &iLong, &dReal);` |
|         9 | 6767 | `			if( iKind == RANGE_IN_ERROR ){` |
|         7 | 6768 | `				return PH7_VmThrowException(pCtx,` |
|         - | 6769 | `					"TypeError",` |
|         - | 6770 | `					"array_rand(): Argument #2 ($num) must be of type int, string given"` |
|         - | 6771 | `					);` |
|         - | 6772 | `			}` |
|         - | 6773 | `			/* Clamp into a signed-int band so an absurd magnitude still yields` |
|         - | 6774 | `			 * the out-of-range ValueError below without an out-of-int cast. */` |
|         3 | 6775 | `			if( iKind == RANGE_IN_DOUBLE ){` |
|         3 | 6776 | `				iLong = dReal <= 0.0 ? 0 : (dReal >= 2147483647.0 ? 2147483647 : (sxi64)dReal);` |
|         1 | 6777 | `			}` |
|         3 | 6778 | `			if( iLong > 2147483647 ){ iLong = 2147483647; }` |
|         3 | 6779 | `			else if( iLong < -2147483647 ){ iLong = -2147483647; }` |
|         3 | 6780 | `			nItem = (int)iLong;` |
|         2 | 6781 | `		}else{` |
|        15 | 6782 | `			nItem = ph7_value_to_int(pNum);` |
|         - | 6783 | `		}` |
|         8 | 6784 | `	}` |
|         - | 6785 | `	/* Point to the internal representation of the input hashmap */` |
|        25 | 6786 | `	pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|         - | 6787 | `	/* php 8: an empty array is a ValueError, not a NULL return */` |
|        25 | 6788 | `	if( pMap->nEntry < 1 ){` |
|         5 | 6789 | `		return PH7_VmThrowException(pCtx,` |
|         - | 6790 | `			"ValueError",` |
|         - | 6791 | `			"array_rand(): Argument #1 ($array) must not be empty"` |
|         - | 6792 | `			);` |
|         - | 6793 | `	}` |
|         - | 6794 | `	/* php 8: $num outside [1, count] is a ValueError, not a clamp/wrong value */` |
|        21 | 6795 | `	if( nItem < 1 \|\| nItem > (int)pMap->nEntry ){` |
|         9 | 6796 | `		return PH7_VmThrowException(pCtx,` |
|         - | 6797 | `			"ValueError",` |
|         - | 6798 | `			"array_rand(): Argument #2 ($num) must be between 1 and the number of elements in argument #1 ($array)"` |
|         - | 6799 | `			);` |
|         - | 6800 | `	}` |
|        13 | 6801 | `	if( nItem < 2 ){` |
|         - | 6802 | `		sxu32 nEntry;` |
|         - | 6803 | `		/* Select a random number */` |
|         9 | 6804 | `		nEntry = PH7_VmRandomNum(pMap->pVm) % pMap->nEntry;` |
|         - | 6805 | `		/* Extract the desired entry.` |
|         - | 6806 | `		 * Note that we perform a linear lookup here (later version must change this)` |
|         - | 6807 | `		 */` |
|         9 | 6808 | `		if( nEntry > pMap->nEntry / 2 ){` |
|         2 | 6809 | `			pNode = pMap->pLast;` |
|         2 | 6810 | `			nEntry = pMap->nEntry - nEntry;` |
|         2 | 6811 | `			if( nEntry > 1 ){` |
|       ! 0 | 6812 | `				for(;;){` |
|       ! 0 | 6813 | `					if( nEntry == 0 ){` |
|       ! 0 | 6814 | `						break;` |
|         - | 6815 | `					}` |
|         - | 6816 | `					/* Point to the previous entry */` |
|       ! 0 | 6817 | `					pNode = pNode->pNext; /* Reverse link */` |
|       ! 0 | 6818 | `					nEntry--;` |
|       ! 0 | 6819 | `				}` |
|       ! 0 | 6820 | `			}` |
|         1 | 6821 | `		}else{` |
|         8 | 6822 | `			pNode = pMap->pFirst;` |
|         6 | 6823 | `			for(;;){` |
|        12 | 6824 | `				if( nEntry == 0 ){` |
|         8 | 6825 | `					break;` |
|         - | 6826 | `				}` |
|         - | 6827 | `				/* Point to the next entry */` |
|         5 | 6828 | `				pNode = pNode->pPrev; /* Reverse link */` |
|         5 | 6829 | `				nEntry--;` |
|         1 | 6830 | `			}` |
|         - | 6831 | `		}` |
|         9 | 6832 | `		if( pNode->iType == HASHMAP_INT_NODE ){` |
|         - | 6833 | `			/* Int key */` |
|         7 | 6834 | `			ph7_result_int64(pCtx,pNode->xKey.iKey);` |
|         4 | 6835 | `		}else{` |
|         - | 6836 | `			/* Blob key */` |
|         3 | 6837 | `			ph7_result_string(pCtx,(const char *)SyBlobData(&pNode->xKey.sKey),(int)SyBlobLength(&pNode->xKey.sKey));` |
|         - | 6838 | `		}` |
|         5 | 6839 | `	}else{` |
|         - | 6840 | `		ph7_value sKey,*pArray;` |
|         - | 6841 | `		ph7_hashmap *pDest;` |
|         - | 6842 | `		/* Create a new array */` |
|         5 | 6843 | `		pArray = ph7_context_new_array(pCtx);` |
|         5 | 6844 | `		if( pArray == 0 ){` |
|       ! 0 | 6845 | `			ph7_result_null(pCtx);` |
|       ! 0 | 6846 | `			return PH7_OK;` |
|         - | 6847 | `		}` |
|         - | 6848 | `		/* Point to the internal representation of the hashmap */` |
|         5 | 6849 | `		pDest = (ph7_hashmap *)pArray->x.pOther;` |
|         5 | 6850 | `		PH7_MemObjInit(pDest->pVm,&sKey);` |
|         - | 6851 | `		/* Copy the first n items */` |
|         5 | 6852 | `		pNode = pMap->pFirst;` |
|         5 | 6853 | `		if( nItem > (int)pMap->nEntry ){` |
|       ! 0 | 6854 | `			nItem = (int)pMap->nEntry;` |
|       ! 0 | 6855 | `		}` |
|        15 | 6856 | `		while( nItem > 0){` |
|        11 | 6857 | `			PH7_HashmapExtractNodeKey(pNode,&sKey);` |
|        11 | 6858 | `			PH7_HashmapInsert(pDest,0/* Automatic index assign*/,&sKey);` |
|        11 | 6859 | `			PH7_MemObjRelease(&sKey);` |
|         - | 6860 | `			/* Point to the next entry */` |
|        11 | 6861 | `			pNode = pNode->pPrev; /* Reverse link */` |
|        11 | 6862 | `			nItem--;` |
|         1 | 6863 | `		}` |
|         - | 6864 | `		/* Shuffle the array */` |
|         5 | 6865 | `		HashmapMergeSort(pDest,HashmapCmpCallback7,0);` |
|         - | 6866 | `		/* Rehash node */` |
|         5 | 6867 | `		HashmapSortRehash(pDest);` |
|         - | 6868 | `		/* Return the random array */` |
|         5 | 6869 | `		ph7_result_value(pCtx,pArray);` |
|         - | 6870 | `	}` |
|        13 | 6871 | `	return PH7_OK;` |
|        22 | 6872 | `}` |
|         - | 6873 | `/*` |
|         - | 6874 | ` * array array_chunk (array $input,int $size [,bool $preserve_keys = false ])` |
|         - | 6875 | ` *  Split an array into chunks.` |
|         - | 6876 | ` * Parameters` |
|         - | 6877 | ` * $input` |
|         - | 6878 | ` *   The array to work on` |
|         - | 6879 | ` * $size` |
|         - | 6880 | ` *   The size of each chunk` |
|         - | 6881 | ` * $preserve_keys` |
|         - | 6882 | ` *   When set to TRUE keys will be preserved. Default is FALSE which will reindex` |
|         - | 6883 | ` *   the chunk numerically.` |
|         - | 6884 | ` * Return` |
|         - | 6885 | ` *  Returns a multidimensional numerically indexed array, starting with` |
|         - | 6886 | ` *  zero, with each dimension containing size elements.` |
|         - | 6887 | ` */` |
|        42 | 6888 | `static int ph7_hashmap_chunk(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|         5 | 6889 | `{` |
|         - | 6890 | `	ph7_value *pArray,*pChunk;` |
|         - | 6891 | `	ph7_hashmap_node *pEntry;` |
|         - | 6892 | `	ph7_hashmap *pMap;` |
|         - | 6893 | `	int bPreserve;` |
|         - | 6894 | `	sxu32 nChunk;` |
|         - | 6895 | `	sxu32 nSize;` |
|         - | 6896 | `	sxu32 n;` |
|         - | 6897 | `	/* Argument count and types follow PHP semantics. */` |
|        47 | 6898 | `	if( nArg < 2 ){` |
|         - | 6899 | `		/* fewer than required arguments -> ArgumentCountError */` |
|         4 | 6900 | `		return PH7_VmThrowException(pCtx,` |
|         - | 6901 | `			"ArgumentCountError",` |
|         - | 6902 | `			"array_chunk() expects at least 2 arguments, %d given",` |
|         1 | 6903 | `			nArg` |
|         - | 6904 | `			);` |
|         - | 6905 | `	}` |
|        45 | 6906 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|         4 | 6907 | `		return PH7_VmThrowException(pCtx,` |
|         - | 6908 | `			"TypeError",` |
|         - | 6909 | `			"array_chunk(): Argument #1 ($array) must be of type array, %s given",` |
|         1 | 6910 | `			ph7_type_name(apArg[0])` |
|         - | 6911 | `			);` |
|         - | 6912 | `	}` |
|         - | 6913 | `	/* Create a new array */` |
|        43 | 6914 | `	pArray = ph7_context_new_array(pCtx);` |
|        43 | 6915 | `	if( pArray == 0 ){` |
|       ! 0 | 6916 | `		ph7_result_null(pCtx);` |
|       ! 0 | 6917 | `		return PH7_OK;` |
|         - | 6918 | `	}` |
|         - | 6919 | `	/* Point to the internal representation of the input hashmap */` |
|        43 | 6920 | `	pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|         - | 6921 | `	/* Extract and validate the chunk size argument. */` |
|         - | 6922 | `	/* Reject types that cannot be sensibly converted to an integer. */` |
|        57 | 6923 | `	if( ph7_value_is_array(apArg[1]) \|\| ph7_value_is_object(apArg[1]) \|\|` |
|        81 | 6924 | `		ph7_value_is_resource(apArg[1]) \|\| ph7_value_is_null(apArg[1]) \|\|` |
|        38 | 6925 | `		ph7_value_is_bool(apArg[1]) ){` |
|       ! 0 | 6926 | `		return PH7_VmThrowException(pCtx,` |
|         - | 6927 | `			"TypeError",` |
|         - | 6928 | `			"array_chunk(): Argument #2 ($length) must be of type int, %s given",` |
|       ! 0 | 6929 | `			ph7_type_name(apArg[1])` |
|         - | 6930 | `			);` |
|         - | 6931 | `	}` |
|         - | 6932 | `	/* Strings that are non-numeric produce a TypeError.  Numeric` |
|         - | 6933 | `	 * strings are permitted; however those representing floats lose` |
|         - | 6934 | `	 * precision and PHP emits a deprecation warning. */` |
|        43 | 6935 | `	if( ph7_value_is_string(apArg[1]) ){` |
|         - | 6936 | `		int len;` |
|         3 | 6937 | `		sxu8 bReal = FALSE;` |
|         3 | 6938 | `		const char *zStr = ph7_value_to_string(apArg[1], &len);` |
|         3 | 6939 | `		if( SyStrIsNumeric(zStr, len, &bReal, 0) != SXRET_OK ){` |
|         3 | 6940 | `			return PH7_VmThrowException(pCtx,` |
|         - | 6941 | `				"TypeError",` |
|         - | 6942 | `				"array_chunk(): Argument #2 ($length) must be of type int, string given"` |
|         - | 6943 | `				);` |
|         - | 6944 | `		}` |
|       ! 0 | 6945 | `		if( bReal ){` |
|         - | 6946 | `			/* float-string -> warn but allow */` |
|       ! 0 | 6947 | `			ph7_context_throw_error_format(pCtx, E_DEPRECATED,` |
|         - | 6948 | `				"Implicit conversion from float-string \"%s\" to int loses precision",` |
|       ! 0 | 6949 | `				zStr` |
|         - | 6950 | `				);` |
|       ! 0 | 6951 | `		}` |
|       ! 0 | 6952 | `	}` |
|         - | 6953 | `	/* If the value is a float with a fractional component, emit a` |
|         - | 6954 | `	 * deprecation warning but continue.  The following conversion occurs` |
|         - | 6955 | `	 * later via ph7_value_to_int. */` |
|        40 | 6956 | `	if( ph7_value_is_float(apArg[1]) ){` |
|         3 | 6957 | `		double d = ph7_value_to_double(apArg[1]);` |
|         3 | 6958 | `		sxi64 i = (sxi64)d;` |
|         3 | 6959 | `		if( d != (double)i ){` |
|         4 | 6960 | `			ph7_context_throw_error_format(pCtx, E_DEPRECATED,` |
|         - | 6961 | `				"Implicit conversion from float %g to int loses precision",` |
|         1 | 6962 | `				d` |
|         - | 6963 | `				);` |
|         1 | 6964 | `		}` |
|         1 | 6965 | `	}` |
|         - | 6966 | `	/* Convert using ph7_value_to_int; now that float fractions are` |
|         - | 6967 | `	 * eliminated, this will not produce a warning. */` |
|         - | 6968 | `	{` |
|        40 | 6969 | `		sxi64 nSizeSigned = ph7_value_to_int(apArg[1]);` |
|        40 | 6970 | `		if( nSizeSigned < 1 ){` |
|         - | 6971 | `			/* size <= 0 -> ValueError */` |
|         6 | 6972 | `			return PH7_VmThrowException(pCtx,` |
|         - | 6973 | `				"ValueError",` |
|         - | 6974 | `				"array_chunk(): Argument #2 ($length) must be greater than 0"` |
|         - | 6975 | `				);` |
|         - | 6976 | `		}` |
|        35 | 6977 | `		nSize = (sxu32)nSizeSigned;` |
|         - | 6978 | `	}` |
|        35 | 6979 | `	if( nSize >= pMap->nEntry ){` |
|         - | 6980 | `		/* Return the whole array */` |
|         3 | 6981 | `		ph7_array_add_elem(pArray,0,apArg[0]);` |
|         3 | 6982 | `		ph7_result_value(pCtx,pArray);` |
|         3 | 6983 | `		return PH7_OK;` |
|         - | 6984 | `	}` |
|        33 | 6985 | `	bPreserve = 0;` |
|        33 | 6986 | `	if( nArg > 2 ){` |
|         - | 6987 | `		/* The third argument has a bool type hint in PHP.  Values that` |
|         - | 6988 | `		 * cannot be sensibly converted (arrays, objects, resources) are` |
|         - | 6989 | `		 * rejected with a TypeError.  Scalars and null coerce to bool` |
|         - | 6990 | `		 * normally, matching PHP behaviour. */` |
|        35 | 6991 | `		if( ph7_value_is_array(apArg[2]) \|\|` |
|        35 | 6992 | `			ph7_value_is_object(apArg[2]) \|\|` |
|        20 | 6993 | `			ph7_value_is_resource(apArg[2]) ){` |
|         8 | 6994 | `			return PH7_VmThrowException(pCtx,` |
|         - | 6995 | `				"TypeError",` |
|         - | 6996 | `				"array_chunk(): Argument #3 ($preserve_keys) must be of type bool, %s given",` |
|         4 | 6997 | `				ph7_type_name(apArg[2])` |
|         - | 6998 | `				);` |
|         - | 6999 | `		}` |
|        21 | 7000 | `		bPreserve = ph7_value_to_bool(apArg[2]);` |
|        10 | 7001 | `	}` |
|         - | 7002 | `	/* Start processing */` |
|        27 | 7003 | `	pEntry = pMap->pFirst;` |
|        27 | 7004 | `	nChunk = 0;` |
|        27 | 7005 | `	pChunk = 0;` |
|        27 | 7006 | `	n = pMap->nEntry;` |
|        56 | 7007 | `	for( ;; ){` |
|       113 | 7008 | `		if( n < 1 ){` |
|         - | 7009 | `			/* When the loop terminates we may still have a current chunk` |
|         - | 7010 | `			 * that hasn't been added to the result array.  The previous` |
|         - | 7011 | `			 * implementation only pushed it if nChunk>0 which dropped the` |
|         - | 7012 | `			 * final chunk when the input size was an exact multiple of` |
|         - | 7013 | `			 * the chunk length.  Always append the pending chunk if it` |
|         - | 7014 | `			 * exists. */` |
|        27 | 7015 | `			if( pChunk ){` |
|        27 | 7016 | `				ph7_array_add_elem(pArray,0,pChunk); /* Will have its own copy */` |
|        13 | 7017 | `			}` |
|        27 | 7018 | `			break;` |
|         - | 7019 | `		}` |
|        87 | 7020 | `		if( nChunk < 1 ){` |
|        71 | 7021 | `			if( pChunk ){` |
|         - | 7022 | `				/* Put the first chunk */` |
|        45 | 7023 | `				ph7_array_add_elem(pArray,0,pChunk); /* Will have it's own copy */` |
|        22 | 7024 | `			}` |
|         - | 7025 | `			/* Create a new dimension */` |
|        71 | 7026 | `			pChunk = ph7_context_new_array(pCtx); /* Don't worry about freeing memory here,everything` |
|         - | 7027 | `												   * will be automatically released as soon we return` |
|         - | 7028 | `												   * from this function */` |
|        71 | 7029 | `			if( pChunk == 0 ){` |
|       ! 0 | 7030 | `				break;` |
|         - | 7031 | `			}` |
|        71 | 7032 | `			nChunk = nSize;` |
|        35 | 7033 | `		}` |
|         - | 7034 | `		/* Insert the entry */` |
|        87 | 7035 | `		HashmapInsertNode((ph7_hashmap *)pChunk->x.pOther,pEntry,bPreserve);` |
|         - | 7036 | `		/* Point to the next entry */` |
|        87 | 7037 | `		pEntry = pEntry->pPrev; /* Reverse link */` |
|        87 | 7038 | `		nChunk--;` |
|        87 | 7039 | `		n--;` |
|         1 | 7040 | `	}` |
|         - | 7041 | `	/* Return the multidimensional array */` |
|        27 | 7042 | `	ph7_result_value(pCtx,pArray);` |
|        27 | 7043 | `	return PH7_OK;` |
|        26 | 7044 | `}` |
|         - | 7045 | `/*` |
|         - | 7046 | ` * array array_pad(array $input,int $pad_size,value $pad_value)` |
|         - | 7047 | ` *  Pad array to the specified length with a value.` |
|         - | 7048 | ` * $input` |
|         - | 7049 | ` *   Initial array of values to pad.` |
|         - | 7050 | ` * $pad_size` |
|         - | 7051 | ` *   New size of the array.` |
|         - | 7052 | ` * $pad_value` |
|         - | 7053 | ` *   Value to pad if input is less than pad_size.` |
|         - | 7054 | ` */` |
|        50 | 7055 | `static int ph7_hashmap_pad(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|         5 | 7056 | `{` |
|         - | 7057 | `	ph7_hashmap *pMap;` |
|         - | 7058 | `	ph7_value *pArray;` |
|         - | 7059 | `	int nEntry;` |
|        55 | 7060 | `	if( nArg != 3 ){` |
|        12 | 7061 | `		return PH7_VmThrowException(pCtx,` |
|         - | 7062 | `			"ArgumentCountError",` |
|         - | 7063 | `			"array_pad() expects exactly 3 arguments, %d given",` |
|         3 | 7064 | `			nArg` |
|         - | 7065 | `			);` |
|         - | 7066 | `	}` |
|        46 | 7067 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|         - | 7068 | `		char zBuf[64];` |
|        14 | 7069 | `		return PH7_VmThrowException(pCtx,` |
|         - | 7070 | `			"TypeError",` |
|         - | 7071 | `			"array_pad(): Argument #1 ($array) must be of type array, %s given",` |
|         4 | 7072 | `			VmValueGivenName(apArg[0],zBuf,sizeof(zBuf))` |
|         - | 7073 | `			);` |
|         - | 7074 | `	}` |
|         - | 7075 | `	/* php 8: $length must be int-coercible. An array/object/resource or a` |
|         - | 7076 | `	 * non-numeric string throws a TypeError instead of silently padding to 0;` |
|         - | 7077 | `	 * a numeric string is weak-coerced via php's is_numeric_string grammar` |
|         - | 7078 | `	 * (reusing the shared RangeStrToNumber, like array_rand's $num). */` |
|        36 | 7079 | `	if( ph7_value_is_array(apArg[1]) \|\| ph7_value_is_object(apArg[1])` |
|        34 | 7080 | `		\|\| ph7_value_is_resource(apArg[1]) ){` |
|         - | 7081 | `		char zBuf[64];` |
|         7 | 7082 | `		return PH7_VmThrowException(pCtx,` |
|         - | 7083 | `			"TypeError",` |
|         - | 7084 | `			"array_pad(): Argument #2 ($length) must be of type int, %s given",` |
|         4 | 7085 | `			VmValueGivenName(apArg[1],zBuf,sizeof(zBuf))` |
|         - | 7086 | `			);` |
|         - | 7087 | `	}` |
|        33 | 7088 | `	if( ph7_value_is_string(apArg[1]) ){` |
|         - | 7089 | `		int nStr;` |
|        11 | 7090 | `		const char *zStr = ph7_value_to_string(apArg[1],&nStr);` |
|         - | 7091 | `		sxi64 iLong; double dReal;` |
|        11 | 7092 | `		sxu8 iKind = RangeStrToNumber(zStr,(sxu32)nStr,&iLong,&dReal);` |
|        11 | 7093 | `		if( iKind == RANGE_IN_ERROR ){` |
|         5 | 7094 | `			return PH7_VmThrowException(pCtx,` |
|         - | 7095 | `				"TypeError",` |
|         - | 7096 | `				"array_pad(): Argument #2 ($length) must be of type int, string given"` |
|         - | 7097 | `				);` |
|         - | 7098 | `		}` |
|         7 | 7099 | `		nEntry = (int)(iKind == RANGE_IN_DOUBLE ? (sxi64)dReal : iLong);` |
|         4 | 7100 | `	}else{` |
|        23 | 7101 | `		nEntry = ph7_value_to_int(apArg[1]);` |
|         - | 7102 | `	}` |
|         - | 7103 | `	/* Create a new array */` |
|        29 | 7104 | `	pArray = ph7_context_new_array(pCtx);` |
|        29 | 7105 | `	if( pArray == 0 ){` |
|       ! 0 | 7106 | `		return PH7_ContextMemoryError(pCtx);` |
|         - | 7107 | `	}` |
|         - | 7108 | `	/* Point to the internal representation of the input hashmap */` |
|        29 | 7109 | `	pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|        29 | 7110 | `	if( nEntry < 0 ){` |
|         9 | 7111 | `		nEntry = -nEntry;` |
|         9 | 7112 | `		if( nEntry > (int)pMap->nEntry ){` |
|         5 | 7113 | `			nEntry -= (int)pMap->nEntry;` |
|         - | 7114 | `			/* Insert given items first */` |
|        17 | 7115 | `			while( nEntry > 0 ){` |
|        13 | 7116 | `				if( ph7_array_add_elem(pArray,0,apArg[2]) != SXRET_OK ){` |
|       ! 0 | 7117 | `					return PH7_ContextMemoryError(pCtx);` |
|         - | 7118 | `				}` |
|        13 | 7119 | `				nEntry--;` |
|         1 | 7120 | `			}` |
|         - | 7121 | `			/* Merge the two arrays */` |
|         5 | 7122 | `			HashmapMerge(pMap,(ph7_hashmap *)pArray->x.pOther);` |
|         3 | 7123 | `		}else{` |
|         5 | 7124 | `			PH7_HashmapDup(pMap,(ph7_hashmap *)pArray->x.pOther);` |
|         1 | 7125 | `		}` |
|        25 | 7126 | `	}else if( nEntry > 0 ){` |
|        19 | 7127 | `		if( nEntry > (int)pMap->nEntry ){` |
|        15 | 7128 | `			nEntry -= (int)pMap->nEntry;` |
|         - | 7129 | `			/* Merge the two arrays first */` |
|        15 | 7130 | `			HashmapMerge(pMap,(ph7_hashmap *)pArray->x.pOther);` |
|         - | 7131 | `			/* Insert given items */` |
|        65 | 7132 | `			while( nEntry > 0 ){` |
|        51 | 7133 | `				if( ph7_array_add_elem(pArray,0,apArg[2]) != SXRET_OK ){` |
|       ! 0 | 7134 | `					return PH7_ContextMemoryError(pCtx);` |
|         - | 7135 | `				}` |
|        51 | 7136 | `				nEntry--;` |
|         1 | 7137 | `			}` |
|         8 | 7138 | `		}else{` |
|         5 | 7139 | `			PH7_HashmapDup(pMap,(ph7_hashmap *)pArray->x.pOther);` |
|         - | 7140 | `		}` |
|        10 | 7141 | `	}else{` |
|         - | 7142 | `		/* nEntry == 0: return a copy of the input array */` |
|         3 | 7143 | `		PH7_HashmapDup(pMap,(ph7_hashmap *)pArray->x.pOther);` |
|         - | 7144 | `	}` |
|         - | 7145 | `	/* Return the new array */` |
|        29 | 7146 | `	ph7_result_value(pCtx,pArray);` |
|        29 | 7147 | `	return PH7_OK;` |
|        30 | 7148 | `}` |
|         - | 7149 | `/*` |
|         - | 7150 | ` * array array_replace(array &$array,array &$array1,...)` |
|         - | 7151 | ` *  Replaces elements from passed arrays into the first array.` |
|         - | 7152 | ` * Parameters` |
|         - | 7153 | ` * $array` |
|         - | 7154 | ` *   The array in which elements are replaced.` |
|         - | 7155 | ` * $array1` |
|         - | 7156 | ` *   The array from which elements will be extracted.` |
|         - | 7157 | ` * ....` |
|         - | 7158 | ` *  More arrays from which elements will be extracted.` |
|         - | 7159 | ` *  Values from later arrays overwrite the previous values.` |
|         - | 7160 | ` * Return` |
|         - | 7161 | ` *  Returns an array.` |
|         - | 7162 | ` *  Throws ArgumentCountError if no arguments are given.` |
|         - | 7163 | ` *  Throws TypeError if any argument is not an array.` |
|         - | 7164 | ` */` |
|        22 | 7165 | `static int ph7_hashmap_replace(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|         4 | 7166 | `{` |
|         - | 7167 | `	ph7_hashmap *pMap;` |
|         - | 7168 | `	ph7_value *pArray;` |
|         - | 7169 | `	int i;` |
|        26 | 7170 | `	if( nArg < 1 ){` |
|         3 | 7171 | `		return PH7_VmThrowException(pCtx,` |
|         - | 7172 | `			"ArgumentCountError",` |
|         - | 7173 | `			"array_replace() expects at least 1 argument, 0 given"` |
|         - | 7174 | `			);` |
|         - | 7175 | `	}` |
|        23 | 7176 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|         4 | 7177 | `		return PH7_VmThrowException(pCtx,` |
|         - | 7178 | `			"TypeError",` |
|         - | 7179 | `			"array_replace(): Argument #1 ($array) must be of type array, %s given",` |
|         1 | 7180 | `			ph7_type_name(apArg[0])` |
|         - | 7181 | `			);` |
|         - | 7182 | `	}` |
|         - | 7183 | `	/* Create a new array */` |
|        20 | 7184 | `	pArray = ph7_context_new_array(pCtx);` |
|        20 | 7185 | `	if( pArray == 0 ){` |
|       ! 0 | 7186 | `		ph7_result_null(pCtx);` |
|       ! 0 | 7187 | `		return PH7_OK;` |
|         - | 7188 | `	}` |
|         - | 7189 | `	/* Overwrite from the first array */` |
|        20 | 7190 | `	pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|        20 | 7191 | `	HashmapOverwrite(pMap,(ph7_hashmap *)pArray->x.pOther);` |
|         - | 7192 | `	/* Perform the requested operation for remaining arrays */` |
|        36 | 7193 | `	for( i = 1 ; i < nArg ; i++ ){` |
|        20 | 7194 | `		if( !ph7_value_is_array(apArg[i]) ){` |
|         - | 7195 | `			/* Type mismatch -> TypeError */` |
|         4 | 7196 | `			return PH7_VmThrowException(pCtx,` |
|         - | 7197 | `				"TypeError",` |
|         - | 7198 | `				"array_replace(): Argument #%d must be of type array, %s given",` |
|         1 | 7199 | `				i + 1,` |
|         2 | 7200 | `				ph7_type_name(apArg[i])` |
|         - | 7201 | `				);` |
|         - | 7202 | `		}` |
|         - | 7203 | `		/* Point to the internal representation of the input hashmap */` |
|        17 | 7204 | `		pMap = (ph7_hashmap *)apArg[i]->x.pOther;` |
|        17 | 7205 | `		HashmapOverwrite(pMap,(ph7_hashmap *)pArray->x.pOther);` |
|         9 | 7206 | `	}` |
|         - | 7207 | `	/* Return the new array */` |
|        17 | 7208 | `	ph7_result_value(pCtx,pArray);` |
|        17 | 7209 | `	return PH7_OK;` |
|        15 | 7210 | `}` |
|         - | 7211 | `/*` |
|         - | 7212 | ` * array array_filter(array $input [,callback $callback ])` |
|         - | 7213 | ` *  Filters elements of an array using a callback function.` |
|         - | 7214 | ` * Parameters` |
|         - | 7215 | ` *  $input` |
|         - | 7216 | ` *    The array to iterate over` |
|         - | 7217 | ` * $callback` |
|         - | 7218 | ` *    The callback function to use` |
|         - | 7219 | ` *    If no callback is supplied, all entries of input equal to FALSE (see converting to boolean)` |
|         - | 7220 | ` *    will be removed.` |
|         - | 7221 | ` * Return` |
|         - | 7222 | ` *  The filtered array.` |
|         - | 7223 | ` */` |
|        32 | 7224 | `static int ph7_hashmap_filter(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|         2 | 7225 | `{` |
|         - | 7226 | `	ph7_hashmap_node *pEntry;` |
|         - | 7227 | `	ph7_hashmap *pMap;` |
|         - | 7228 | `	ph7_value *pArray;` |
|         - | 7229 | `	ph7_value sResult;   /* Callback result */` |
|         - | 7230 | `	ph7_value *pValue;` |
|         - | 7231 | `	sxi32 rc;` |
|         - | 7232 | `	int keep;` |
|         - | 7233 | `	sxu32 n;` |
|        34 | 7234 | `	if( nArg < 1 ){` |
|         - | 7235 | `		/* Missing argument (arity is enforced upstream; defensive) */` |
|       ! 0 | 7236 | `		ph7_result_null(pCtx);` |
|       ! 0 | 7237 | `		return PH7_OK;` |
|         - | 7238 | `	}` |
|         - | 7239 | `	/* php 8: $array must be an array (TypeError, not a silent NULL return) */` |
|        34 | 7240 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|         - | 7241 | `		char zBuf[64];` |
|        22 | 7242 | `		return PH7_VmThrowException(pCtx,` |
|         - | 7243 | `			"TypeError",` |
|         - | 7244 | `			"array_filter(): Argument #1 ($array) must be of type array, %s given",` |
|         7 | 7245 | `			VmValueGivenName(apArg[0],zBuf,sizeof(zBuf))` |
|         - | 7246 | `			);` |
|         - | 7247 | `	}` |
|         - | 7248 | `	/* Create a new array */` |
|        20 | 7249 | `	pArray = ph7_context_new_array(pCtx);` |
|        20 | 7250 | `	if( pArray == 0 ){` |
|       ! 0 | 7251 | `		ph7_result_null(pCtx);` |
|       ! 0 | 7252 | `		return PH7_OK;` |
|         - | 7253 | `	}` |
|         - | 7254 | `	/* Point to the internal representation of the input hashmap */` |
|        20 | 7255 | `	pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|        20 | 7256 | `	pEntry = pMap->pFirst;` |
|        20 | 7257 | `	PH7_MemObjInit(pMap->pVm,&sResult);` |
|        20 | 7258 | `	sResult.nIdx = SXU32_HIGH; /* Mark as constant */` |
|         - | 7259 | `	/* Perform the requested operation */` |
|        78 | 7260 | `	for( n = 0 ; n < pMap->nEntry ; n++ ){` |
|         - | 7261 | `		/* Extract node value (may be NULL if allocation failed) */` |
|        64 | 7262 | `		pValue = HashmapExtractNodeValue(pEntry);` |
|        64 | 7263 | `		if( pValue == 0 ){` |
|         - | 7264 | `			/* Can happen if SySetAt() failed earlier; drop the entry. */` |
|       ! 0 | 7265 | `			keep = FALSE;` |
|        64 | 7266 | `		}else if( nArg > 1 && !ph7_value_is_null(apArg[1]) ){` |
|         - | 7267 | `			/* Callback was supplied (not NULL).  PHP 8 throws a` |
|         - | 7268 | `				* TypeError when the value is not callable or null; prior PH7` |
|         - | 7269 | `				* silently dropped the element.  Emit similar message. */` |
|        36 | 7270 | `			if( !ph7_value_is_callable(apArg[1]) ){` |
|         3 | 7271 | `				if( ph7_value_is_string(apArg[1]) ){` |
|         - | 7272 | `					int len;` |
|         3 | 7273 | `					const char *zName = ph7_value_to_string(apArg[1], &len);` |
|         4 | 7274 | `					return PH7_VmThrowException(pCtx,` |
|         - | 7275 | `						"TypeError",` |
|         - | 7276 | `						"array_filter(): Argument #2 ($callback) must be a valid callback or null, function \"%s\" not found or invalid function name",` |
|         1 | 7277 | `						zName` |
|         - | 7278 | `						);` |
|       ! 0 | 7279 | `				}else{` |
|       ! 0 | 7280 | `					return PH7_VmThrowException(pCtx,` |
|         - | 7281 | `						"TypeError",` |
|         - | 7282 | `						"array_filter(): Argument #2 ($callback) must be a valid callback or null, %s given",` |
|       ! 0 | 7283 | `						ph7_type_name(apArg[1])` |
|         - | 7284 | `						);` |
|         - | 7285 | `				}` |
|         - | 7286 | `			}` |
|        33 | 7287 | `			keep = FALSE;` |
|        33 | 7288 | `			rc = PH7_VmCallUserFunction(pMap->pVm,apArg[1],1,&pValue,&sResult);` |
|        33 | 7289 | `			if( rc == PH7_EXCEPTION ){` |
|         - | 7290 | `				/* The callback raised: propagate so the dispatcher unwinds. */` |
|         3 | 7291 | `				PH7_MemObjRelease(&sResult);` |
|         3 | 7292 | `				return PH7_EXCEPTION;` |
|         - | 7293 | `			}` |
|        31 | 7294 | `			if( rc == SXRET_OK ){` |
|         - | 7295 | `				/* Perform a boolean cast */` |
|        31 | 7296 | `				keep = ph7_value_to_bool(&sResult);` |
|        15 | 7297 | `			}` |
|        31 | 7298 | `			PH7_MemObjRelease(&sResult);` |
|        16 | 7299 | `		}else{` |
|         - | 7300 | `			/* No callback provided or callback explicitly NULL: use default` |
|         - | 7301 | `			 * behaviour where "empty" values are removed. This also covers` |
|         - | 7302 | `			 * the case where the callback argument is missing entirely.` |
|         - | 7303 | `			 */` |
|        29 | 7304 | `			keep = !PH7_MemObjIsEmpty(pValue);` |
|         - | 7305 | `		}` |
|        59 | 7306 | `		if( keep ){` |
|         - | 7307 | `			/* Perform the insertion,now the callback returned true */` |
|        21 | 7308 | `			HashmapInsertNode((ph7_hashmap *)pArray->x.pOther,pEntry,TRUE);` |
|        10 | 7309 | `		}` |
|         - | 7310 | `		/* Point to the next entry */` |
|        59 | 7311 | `		pEntry = pEntry->pPrev; /* Reverse link */` |
|        30 | 7312 | `	}` |
|        15 | 7313 | `	ph7_result_value(pCtx,pArray);` |
|        15 | 7314 | `	return PH7_OK;` |
|        18 | 7315 | `}` |
|         - | 7316 | `/*` |
|         - | 7317 | ` * array array_map(?callable $callback, array $array, array ...$arrays)` |
|         - | 7318 | ` *  Applies the callback to the elements of the given arrays.` |
|         - | 7319 | ` * Parameters` |
|         - | 7320 | ` *  $callback` |
|         - | 7321 | ` *   A callable to run for each element in each array, or NULL. With a single` |
|         - | 7322 | ` *   array and a NULL callback this is the identity function (the array is` |
|         - | 7323 | ` *   returned unchanged); with several arrays and a NULL callback the arrays` |
|         - | 7324 | ` *   are zipped together.` |
|         - | 7325 | ` *  $array` |
|         - | 7326 | ` *   The first array to run through the callback function.` |
|         - | 7327 | ` *  $arrays` |
|         - | 7328 | ` *   Zero or more additional arrays to process in parallel.` |
|         - | 7329 | ` * Return` |
|         - | 7330 | ` *  Returns an array containing the results of applying the callback function.` |
|         - | 7331 | ` *  With a single array the keys are preserved; with several arrays the result` |
|         - | 7332 | ` *  is re-indexed and the iteration runs to the length of the longest array,` |
|         - | 7333 | ` *  padding shorter arrays with NULL.` |
|         - | 7334 | ` */` |
|        54 | 7335 | `static int ph7_hashmap_map(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|         5 | 7336 | `{` |
|         - | 7337 | `	ph7_value *pArray,*pValue,sKey,sResult;` |
|         - | 7338 | `	ph7_hashmap_node *pEntry;` |
|         - | 7339 | `	ph7_hashmap *pMap;` |
|         - | 7340 | `	ph7_vm *pVm;` |
|         - | 7341 | `	int bNullCallback;` |
|         - | 7342 | `	sxi32 rc;` |
|         - | 7343 | `	int i;` |
|         - | 7344 | `	sxu32 n;` |
|        59 | 7345 | `	if( nArg < 2 ){` |
|         8 | 7346 | `		return PH7_VmThrowException(pCtx,` |
|         - | 7347 | `			"ArgumentCountError",` |
|         - | 7348 | `			"array_map() expects at least 2 arguments, %d given",` |
|         2 | 7349 | `			nArg` |
|         - | 7350 | `			);` |
|         - | 7351 | `	}` |
|        54 | 7352 | `	bNullCallback = ph7_value_is_null(apArg[0]);` |
|        54 | 7353 | `	if( !bNullCallback && !ph7_value_is_callable(apArg[0]) ){` |
|         5 | 7354 | `		if( ph7_value_is_string(apArg[0]) ){` |
|         3 | 7355 | `			const char *zFunc = ph7_value_to_string(apArg[0],0);` |
|         4 | 7356 | `			return PH7_VmThrowException(pCtx,` |
|         - | 7357 | `				"TypeError",` |
|         - | 7358 | `				"array_map(): Argument #1 ($callback) must be a valid callback or null, "` |
|         - | 7359 | `				"function \"%s\" not found or invalid function name",` |
|         1 | 7360 | `				zFunc` |
|         - | 7361 | `				);` |
|         - | 7362 | `		}` |
|         3 | 7363 | `		return PH7_VmThrowException(pCtx,` |
|         - | 7364 | `			"TypeError",` |
|         - | 7365 | `			"array_map(): Argument #1 ($callback) must be a valid callback or null, "` |
|         - | 7366 | `			"no array or string given"` |
|         - | 7367 | `			);` |
|         - | 7368 | `	}` |
|         - | 7369 | `	/* Every remaining argument must be an array */` |
|       105 | 7370 | `	for( i = 1 ; i < nArg ; i++ ){` |
|        61 | 7371 | `		if( !ph7_value_is_array(apArg[i]) ){` |
|         3 | 7372 | `			if( i == 1 ){` |
|         4 | 7373 | `				return PH7_VmThrowException(pCtx,` |
|         - | 7374 | `					"TypeError",` |
|         - | 7375 | `					"array_map(): Argument #2 ($array) must be of type array, %s given",` |
|         2 | 7376 | `					ph7_type_name(apArg[1])` |
|         - | 7377 | `					);` |
|         - | 7378 | `			}` |
|       ! 0 | 7379 | `			return PH7_VmThrowException(pCtx,` |
|         - | 7380 | `				"TypeError",` |
|         - | 7381 | `				"array_map(): Argument #%d must be of type array, %s given",` |
|       ! 0 | 7382 | `				i+1,ph7_type_name(apArg[i])` |
|         - | 7383 | `				);` |
|         - | 7384 | `		}` |
|        30 | 7385 | `	}` |
|        46 | 7386 | `	pVm = pCtx->pVm;` |
|         - | 7387 | `	/* Create a new array */` |
|        46 | 7388 | `	pArray = ph7_context_new_array(pCtx);` |
|        46 | 7389 | `	if( pArray == 0 ){` |
|       ! 0 | 7390 | `		ph7_result_null(pCtx);` |
|       ! 0 | 7391 | `		return PH7_OK;` |
|         - | 7392 | `	}` |
|        46 | 7393 | `	PH7_MemObjInit(pVm,&sResult);` |
|        46 | 7394 | `	PH7_MemObjInit(pVm,&sKey);` |
|        46 | 7395 | `	sResult.nIdx = SXU32_HIGH; /* Mark as constant */` |
|        46 | 7396 | `	sKey.nIdx    = SXU32_HIGH; /* Mark as constant */` |
|        46 | 7397 | `	if( nArg == 2 ){` |
|         - | 7398 | `		/* Single-array mode: keys are preserved (PHP semantics). */` |
|        36 | 7399 | `		pMap = (ph7_hashmap *)apArg[1]->x.pOther;` |
|        36 | 7400 | `		pEntry = pMap->pFirst;` |
|       110 | 7401 | `		for( n = 0 ; n < pMap->nEntry ; n++ ){` |
|         - | 7402 | `			/* Extract the node value */` |
|        78 | 7403 | `			pValue = HashmapExtractNodeValue(pEntry);` |
|        78 | 7404 | `			if( pValue ){` |
|         - | 7405 | `				/* Extract the node key */` |
|        78 | 7406 | `				PH7_HashmapExtractNodeKey(pEntry,&sKey);` |
|        78 | 7407 | `				if( bNullCallback ){` |
|         - | 7408 | `					/* NULL callback: identity function, keep original value */` |
|        11 | 7409 | `					ph7_array_add_elem(pArray,&sKey,pValue);` |
|         6 | 7410 | `				}else{` |
|         - | 7411 | `					/* Invoke the supplied callback */` |
|        68 | 7412 | `					rc = PH7_VmCallUserFunction(pVm,apArg[0],1,&pValue,&sResult);` |
|        68 | 7413 | `					if( rc == PH7_EXCEPTION ){` |
|         - | 7414 | `						/* Callback raised: abort and let the foreign-function` |
|         - | 7415 | `						 * dispatcher unwind through the nearest try/catch. */` |
|         3 | 7416 | `						PH7_MemObjRelease(&sKey);` |
|         3 | 7417 | `						PH7_MemObjRelease(&sResult);` |
|         3 | 7418 | `						return PH7_EXCEPTION;` |
|         - | 7419 | `					}` |
|         - | 7420 | `					/* Insert the callback return value */` |
|        66 | 7421 | `					ph7_array_add_elem(pArray,&sKey,&sResult);` |
|         - | 7422 | `				}` |
|        76 | 7423 | `				PH7_MemObjRelease(&sKey);` |
|        76 | 7424 | `				PH7_MemObjRelease(&sResult);` |
|        37 | 7425 | `			}` |
|         - | 7426 | `			/* Point to the next entry */` |
|        76 | 7427 | `			pEntry = pEntry->pPrev; /* Reverse link */` |
|        39 | 7428 | `		}` |
|        18 | 7429 | `	}else{` |
|         - | 7430 | `		/* Multi-array mode: walk every array in parallel to the length of the` |
|         - | 7431 | `		 * longest one, pad shorter arrays with NULL, and re-index the result. */` |
|        11 | 7432 | `		int nArrays = nArg - 1;` |
|         - | 7433 | `		ph7_hashmap_node **apCur;` |
|         - | 7434 | `		ph7_value **apCallArg;` |
|         - | 7435 | `		ph7_value sNull;` |
|        11 | 7436 | `		sxu32 nMax = 0;` |
|        11 | 7437 | `		apCur     = (ph7_hashmap_node **)SyMemBackendAlloc(&pVm->sAllocator,(sxu32)(nArrays*sizeof(ph7_hashmap_node *)));` |
|        11 | 7438 | `		apCallArg = (ph7_value **)SyMemBackendAlloc(&pVm->sAllocator,(sxu32)(nArrays*sizeof(ph7_value *)));` |
|        11 | 7439 | `		if( apCur == 0 \|\| apCallArg == 0 ){` |
|       ! 0 | 7440 | `			if( apCur ){ SyMemBackendFree(&pVm->sAllocator,apCur); }` |
|       ! 0 | 7441 | `			if( apCallArg ){ SyMemBackendFree(&pVm->sAllocator,apCallArg); }` |
|       ! 0 | 7442 | `			PH7_MemObjRelease(&sKey);` |
|       ! 0 | 7443 | `			PH7_MemObjRelease(&sResult);` |
|       ! 0 | 7444 | `			ph7_result_value(pCtx,pArray);` |
|       ! 0 | 7445 | `			return PH7_OK;` |
|         - | 7446 | `		}` |
|        11 | 7447 | `		PH7_MemObjInit(pVm,&sNull); /* shared NULL pad for short arrays */` |
|        11 | 7448 | `		sNull.nIdx = SXU32_HIGH;` |
|        33 | 7449 | `		for( i = 0 ; i < nArrays ; i++ ){` |
|        23 | 7450 | `			pMap = (ph7_hashmap *)apArg[i+1]->x.pOther;` |
|        23 | 7451 | `			apCur[i] = pMap->pFirst;` |
|        23 | 7452 | `			if( pMap->nEntry > nMax ){` |
|        13 | 7453 | `				nMax = pMap->nEntry;` |
|         6 | 7454 | `			}` |
|        12 | 7455 | `		}` |
|        35 | 7456 | `		for( n = 0 ; n < nMax ; n++ ){` |
|        25 | 7457 | `			ph7_value *pZip = 0;` |
|        25 | 7458 | `			if( bNullCallback ){` |
|         - | 7459 | `				/* zip: each result element is an array of the i-th values */` |
|         5 | 7460 | `				pZip = ph7_context_new_array(pCtx);` |
|         2 | 7461 | `			}` |
|        79 | 7462 | `			for( i = 0 ; i < nArrays ; i++ ){` |
|        55 | 7463 | `				ph7_value *pv = &sNull;` |
|        55 | 7464 | `				if( apCur[i] ){` |
|        53 | 7465 | `					ph7_value *pNodeVal = HashmapExtractNodeValue(apCur[i]);` |
|        53 | 7466 | `					if( pNodeVal ){` |
|        53 | 7467 | `						pv = pNodeVal;` |
|        26 | 7468 | `					}` |
|        53 | 7469 | `					apCur[i] = apCur[i]->pPrev; /* Reverse link */` |
|        26 | 7470 | `				}` |
|        55 | 7471 | `				if( bNullCallback ){` |
|         9 | 7472 | `					if( pZip ){` |
|         9 | 7473 | `						ph7_array_add_elem(pZip,0,pv);` |
|         4 | 7474 | `					}` |
|         5 | 7475 | `				}else{` |
|        47 | 7476 | `					apCallArg[i] = pv;` |
|         - | 7477 | `				}` |
|        28 | 7478 | `			}` |
|        25 | 7479 | `			if( bNullCallback ){` |
|         5 | 7480 | `				if( pZip ){` |
|         5 | 7481 | `					ph7_array_add_elem(pArray,0,pZip);` |
|         2 | 7482 | `				}` |
|         3 | 7483 | `			}else{` |
|        21 | 7484 | `				rc = PH7_VmCallUserFunction(pVm,apArg[0],nArrays,apCallArg,&sResult);` |
|        21 | 7485 | `				if( rc == PH7_EXCEPTION ){` |
|       ! 0 | 7486 | `					SyMemBackendFree(&pVm->sAllocator,apCur);` |
|       ! 0 | 7487 | `					SyMemBackendFree(&pVm->sAllocator,apCallArg);` |
|       ! 0 | 7488 | `					PH7_MemObjRelease(&sNull);` |
|       ! 0 | 7489 | `					PH7_MemObjRelease(&sKey);` |
|       ! 0 | 7490 | `					PH7_MemObjRelease(&sResult);` |
|       ! 0 | 7491 | `					return PH7_EXCEPTION;` |
|         - | 7492 | `				}` |
|        21 | 7493 | `				ph7_array_add_elem(pArray,0,&sResult);` |
|        21 | 7494 | `				PH7_MemObjRelease(&sResult);` |
|         - | 7495 | `			}` |
|        13 | 7496 | `		}` |
|        11 | 7497 | `		SyMemBackendFree(&pVm->sAllocator,apCur);` |
|        11 | 7498 | `		SyMemBackendFree(&pVm->sAllocator,apCallArg);` |
|        11 | 7499 | `		PH7_MemObjRelease(&sNull);` |
|         - | 7500 | `	}` |
|        44 | 7501 | `	PH7_MemObjRelease(&sKey);` |
|        44 | 7502 | `	PH7_MemObjRelease(&sResult);` |
|        44 | 7503 | `	ph7_result_value(pCtx,pArray);` |
|        44 | 7504 | `	return PH7_OK;` |
|        32 | 7505 | `}` |
|         - | 7506 | `/*` |
|         - | 7507 | ` * value array_reduce(array $array, callable $callback[, value $initial = NULL])` |
|         - | 7508 | ` *  Iteratively reduce the array to a single value using a callback function.` |
|         - | 7509 | ` * Parameters` |
|         - | 7510 | ` *  $array` |
|         - | 7511 | ` *   The input array.` |
|         - | 7512 | ` *  $callback` |
|         - | 7513 | ` *   The callback function. Signature: callback(mixed $carry, mixed $item): mixed` |
|         - | 7514 | ` *  $initial` |
|         - | 7515 | ` *   If the optional initial is available, it will be used at the beginning` |
|         - | 7516 | ` *   of the process, or as a final result in case the array is empty.` |
|         - | 7517 | ` * Return` |
|         - | 7518 | ` *  Returns the resulting value.` |
|         - | 7519 | ` *  If the array is empty and initial is not passed, array_reduce() returns NULL.` |
|         - | 7520 | ` */` |
|        34 | 7521 | `static int ph7_hashmap_reduce(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|         5 | 7522 | `{` |
|         - | 7523 | `	ph7_hashmap_node *pEntry;` |
|         - | 7524 | `	ph7_hashmap *pMap;` |
|         - | 7525 | `	ph7_value *pValue;` |
|         - | 7526 | `	ph7_value sResult;` |
|         - | 7527 | `	sxi32 rc;` |
|         - | 7528 | `	sxu32 n;` |
|        39 | 7529 | `	if( nArg < 2 ){` |
|         8 | 7530 | `		return PH7_VmThrowException(pCtx,` |
|         - | 7531 | `			"ArgumentCountError",` |
|         - | 7532 | `			"array_reduce() expects at least 2 arguments, %d given",` |
|         2 | 7533 | `			nArg` |
|         - | 7534 | `			);` |
|         - | 7535 | `	}` |
|        35 | 7536 | `	if( nArg > 3 ){` |
|         4 | 7537 | `		return PH7_VmThrowException(pCtx,` |
|         - | 7538 | `			"ArgumentCountError",` |
|         - | 7539 | `			"array_reduce() expects at most 3 arguments, %d given",` |
|         1 | 7540 | `			nArg` |
|         - | 7541 | `			);` |
|         - | 7542 | `	}` |
|        33 | 7543 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|         4 | 7544 | `		return PH7_VmThrowException(pCtx,` |
|         - | 7545 | `			"TypeError",` |
|         - | 7546 | `			"array_reduce(): Argument #1 ($array) must be of type array, %s given",` |
|         1 | 7547 | `			ph7_type_name(apArg[0])` |
|         - | 7548 | `			);` |
|         - | 7549 | `	}` |
|        31 | 7550 | `	if( !ph7_value_is_callable(apArg[1]) ){` |
|        12 | 7551 | `		if( ph7_value_is_string(apArg[1]) ){` |
|         3 | 7552 | `			const char *zFunc = ph7_value_to_string(apArg[1],0);` |
|         4 | 7553 | `			return PH7_VmThrowException(pCtx,` |
|         - | 7554 | `				"TypeError",` |
|         - | 7555 | `				"array_reduce(): Argument #2 ($callback) must be a valid callback, "` |
|         - | 7556 | `				"function \"%s\" not found or invalid function name",` |
|         1 | 7557 | `				zFunc` |
|         - | 7558 | `				);` |
|         - | 7559 | `		}` |
|         9 | 7560 | `		if( ph7_value_is_array(apArg[1]) ){` |
|         3 | 7561 | `			return PH7_VmThrowException(pCtx,` |
|         - | 7562 | `				"TypeError",` |
|         - | 7563 | `				"array_reduce(): Argument #2 ($callback) must be a valid callback, "` |
|         - | 7564 | `				"array callback must have exactly two members"` |
|         - | 7565 | `				);` |
|         - | 7566 | `		}` |
|         6 | 7567 | `		return PH7_VmThrowException(pCtx,` |
|         - | 7568 | `			"TypeError",` |
|         - | 7569 | `			"array_reduce(): Argument #2 ($callback) must be a valid callback, "` |
|         - | 7570 | `			"no array or string given"` |
|         - | 7571 | `			);` |
|         - | 7572 | `	}` |
|         - | 7573 | `	/* Point to the internal representation of the input hashmap */` |
|        19 | 7574 | `	pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|         - | 7575 | `	/* Assume a NULL initial value */` |
|        19 | 7576 | `	PH7_MemObjInit(pMap->pVm,&sResult);` |
|        19 | 7577 | `	sResult.nIdx = SXU32_HIGH; /* Mark as constant */` |
|        19 | 7578 | `	if( nArg > 2 ){` |
|         - | 7579 | `		/* Set the initial value */` |
|        13 | 7580 | `		PH7_MemObjLoad(apArg[2],&sResult);` |
|         6 | 7581 | `	}` |
|         - | 7582 | `	/* Perform the requested operation */` |
|        19 | 7583 | `	pEntry = pMap->pFirst;` |
|        55 | 7584 | `	for( n = 0 ; n < pMap->nEntry ; n++ ){` |
|         - | 7585 | `		/* Extract the node value */` |
|        39 | 7586 | `		pValue = HashmapExtractNodeValue(pEntry);` |
|         - | 7587 | `		/* Invoke the supplied callback */` |
|        39 | 7588 | `		rc = PH7_VmCallUserFunctionAp(pMap->pVm,apArg[1],&sResult,&sResult,pValue,0);` |
|        39 | 7589 | `		if( rc == PH7_EXCEPTION ){` |
|         - | 7590 | `			/* The callback raised: propagate so the dispatcher unwinds. */` |
|         3 | 7591 | `			PH7_MemObjRelease(&sResult);` |
|         3 | 7592 | `			return PH7_EXCEPTION;` |
|         - | 7593 | `		}` |
|         - | 7594 | `		/* Point to the next entry */` |
|        37 | 7595 | `		pEntry = pEntry->pPrev; /* Reverse link */` |
|        19 | 7596 | `	}` |
|        17 | 7597 | `	ph7_result_value(pCtx,&sResult); /* Will make it's own copy */` |
|        17 | 7598 | `	PH7_MemObjRelease(&sResult);` |
|        17 | 7599 | `	return PH7_OK;` |
|        22 | 7600 | `}` |
|         - | 7601 | `/*` |
|         - | 7602 | ` * bool array_walk(array &$array, callback $funcname [, mixed $userdata])` |
|         - | 7603 | ` *  Apply a user function to every member of an array.` |
|         - | 7604 | ` * Parameters` |
|         - | 7605 | ` *  $array` |
|         - | 7606 | ` *   The input array.` |
|         - | 7607 | ` *  $funcname` |
|         - | 7608 | ` *   Typically, funcname takes on two parameters. The array parameter's value being` |
|         - | 7609 | ` *   the first, and the key/index second.` |
|         - | 7610 | ` * Note:` |
|         - | 7611 | ` *  If funcname needs to be working with the actual values of the array, specify the first` |
|         - | 7612 | ` *  parameter of funcname as a reference. Then, any changes made to those elements will` |
|         - | 7613 | ` *  be made in the original array itself.` |
|         - | 7614 | ` *  $userdata` |
|         - | 7615 | ` *   If the optional userdata parameter is supplied, it will be passed as the third parameter` |
|         - | 7616 | ` *   to the callback funcname.` |
|         - | 7617 | ` * Return` |
|         - | 7618 | ` *  Returns TRUE on success or FALSE on failure.` |
|         - | 7619 | ` */` |
|        38 | 7620 | `static int ph7_hashmap_walk(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|         5 | 7621 | `{` |
|         - | 7622 | `	ph7_value *pValue,*pUserData,sKey;` |
|         - | 7623 | `	ph7_hashmap_node *pEntry;` |
|         - | 7624 | `	ph7_hashmap *pMap;` |
|         - | 7625 | `	sxu32 n;` |
|        43 | 7626 | `	if( nArg < 2 ){` |
|         8 | 7627 | `		return PH7_VmThrowException(pCtx,` |
|         - | 7628 | `			"ArgumentCountError",` |
|         - | 7629 | `			"array_walk() expects at least 2 arguments, %d given",` |
|         2 | 7630 | `			nArg` |
|         - | 7631 | `			);` |
|         - | 7632 | `	}` |
|        39 | 7633 | `	if( nArg > 3 ){` |
|         4 | 7634 | `		return PH7_VmThrowException(pCtx,` |
|         - | 7635 | `			"ArgumentCountError",` |
|         - | 7636 | `			"array_walk() expects at most 3 arguments, %d given",` |
|         1 | 7637 | `			nArg` |
|         - | 7638 | `			);` |
|         - | 7639 | `	}` |
|        37 | 7640 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|         4 | 7641 | `		return PH7_VmThrowException(pCtx,` |
|         - | 7642 | `			"TypeError",` |
|         - | 7643 | `			"array_walk(): Argument #1 ($array) must be of type array, %s given",` |
|         1 | 7644 | `			ph7_type_name(apArg[0])` |
|         - | 7645 | `			);` |
|         - | 7646 | `	}` |
|        35 | 7647 | `	if( !ph7_value_is_callable(apArg[1]) ){` |
|        14 | 7648 | `		if( ph7_value_is_string(apArg[1]) ){` |
|         3 | 7649 | `			const char *zFunc = ph7_value_to_string(apArg[1],0);` |
|         4 | 7650 | `			return PH7_VmThrowException(pCtx,` |
|         - | 7651 | `				"TypeError",` |
|         - | 7652 | `				"array_walk(): Argument #2 ($callback) must be a valid callback, "` |
|         - | 7653 | `				"function \"%s\" not found or invalid function name",` |
|         1 | 7654 | `				zFunc` |
|         - | 7655 | `				);` |
|         - | 7656 | `		}` |
|        12 | 7657 | `		if( ph7_value_is_array(apArg[1]) ){` |
|         6 | 7658 | `			return PH7_VmThrowException(pCtx,` |
|         - | 7659 | `				"TypeError",` |
|         - | 7660 | `				"array_walk(): Argument #2 ($callback) must be a valid callback, "` |
|         - | 7661 | `				"array callback must have exactly two members"` |
|         - | 7662 | `				);` |
|         - | 7663 | `		}` |
|         6 | 7664 | `		return PH7_VmThrowException(pCtx,` |
|         - | 7665 | `			"TypeError",` |
|         - | 7666 | `			"array_walk(): Argument #2 ($callback) must be a valid callback, "` |
|         - | 7667 | `			"no array or string given"` |
|         - | 7668 | `			);` |
|         - | 7669 | `	}` |
|        21 | 7670 | `	pUserData = nArg > 2 ? apArg[2] : 0;` |
|         - | 7671 | `	/* Point to the internal representation of the input hashmap */` |
|        21 | 7672 | `	PH7_HashmapCowSeparate(pCtx->pVm, apArg[0]);` |
|        21 | 7673 | `	pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|        21 | 7674 | `	PH7_MemObjInit(pMap->pVm,&sKey);` |
|        21 | 7675 | `	sKey.nIdx = SXU32_HIGH; /* Mark as constant */` |
|         - | 7676 | `	/* Perform the desired operation */` |
|        21 | 7677 | `	pEntry = pMap->pFirst;` |
|        61 | 7678 | `	for( n = 0 ; n < pMap->nEntry ; n++ ){` |
|         - | 7679 | `		/* Extract the node value */` |
|        43 | 7680 | `		pValue = HashmapExtractNodeValue(pEntry);` |
|        43 | 7681 | `		if( pValue ){` |
|         - | 7682 | `			sxi32 rcW;` |
|         - | 7683 | `			/* Extract the entry key */` |
|        43 | 7684 | `			PH7_HashmapExtractNodeKey(pEntry,&sKey);` |
|         - | 7685 | `			/* Invoke the supplied callback */` |
|        43 | 7686 | `			rcW = PH7_VmCallUserFunctionAp(pMap->pVm,apArg[1],0,pValue,&sKey,pUserData,0);` |
|        43 | 7687 | `			PH7_MemObjRelease(&sKey);` |
|        43 | 7688 | `			if( rcW == PH7_EXCEPTION ){` |
|         - | 7689 | `				/* The callback raised: propagate so the dispatcher unwinds. */` |
|         3 | 7690 | `				return PH7_EXCEPTION;` |
|         - | 7691 | `			}` |
|        20 | 7692 | `		}` |
|         - | 7693 | `		/* Point to the next entry */` |
|        41 | 7694 | `		pEntry = pEntry->pPrev; /* Reverse link */` |
|        21 | 7695 | `	}` |
|         - | 7696 | `	/* All done, return TRUE */` |
|        19 | 7697 | `	ph7_result_bool(pCtx,1);` |
|        19 | 7698 | `	return PH7_OK;` |
|        24 | 7699 | `}` |
|         - | 7700 | `/*` |
|         - | 7701 | ` * Apply a user function to every member of an array.(Recurse on array's).` |
|         - | 7702 | ` * Refer to the [array_walk_recursive()] implementation for more information.` |
|         - | 7703 | ` */` |
|        22 | 7704 | `static sxi32 HashmapWalkRecursive(` |
|         - | 7705 | `	ph7_hashmap *pMap,    /* Target hashmap */` |
|         - | 7706 | `	ph7_value *pCallback, /* User callback */` |
|         - | 7707 | `	ph7_value *pUserData, /* Callback private data */` |
|         - | 7708 | `	int iNest             /* Nesting level */` |
|         - | 7709 | `	)` |
|         1 | 7710 | `{` |
|         - | 7711 | `	ph7_hashmap_node *pEntry;` |
|         - | 7712 | `	ph7_value *pValue,sKey;` |
|         - | 7713 | `	sxi32 rc;` |
|         - | 7714 | `	sxu32 n;` |
|         - | 7715 | `	/* Iterate through hashmap entries */` |
|        23 | 7716 | `	PH7_MemObjInit(pMap->pVm,&sKey);` |
|        23 | 7717 | `	sKey.nIdx = SXU32_HIGH; /* Mark as constant */` |
|        23 | 7718 | `	pEntry = pMap->pFirst;` |
|        59 | 7719 | `	for( n = 0 ; n < pMap->nEntry ; n++ ){` |
|         - | 7720 | `		/* Extract the node value */` |
|        37 | 7721 | `		pValue = HashmapExtractNodeValue(pEntry);` |
|        37 | 7722 | `		if( pValue ){` |
|        37 | 7723 | `			if( pValue->iFlags & MEMOBJ_HASHMAP ){` |
|        11 | 7724 | `				if( iNest < 32 ){` |
|         - | 7725 | `					/* Recurse */` |
|        11 | 7726 | `					iNest++;` |
|        11 | 7727 | `					rc = HashmapWalkRecursive((ph7_hashmap *)pValue->x.pOther,pCallback,pUserData,iNest);` |
|        11 | 7728 | `					iNest--;` |
|        11 | 7729 | `					if( rc == PH7_EXCEPTION ){` |
|       ! 0 | 7730 | `						return PH7_EXCEPTION;` |
|         - | 7731 | `					}` |
|         5 | 7732 | `				}` |
|         6 | 7733 | `			}else{` |
|         - | 7734 | `				/* Extract the node key */` |
|        27 | 7735 | `				PH7_HashmapExtractNodeKey(pEntry,&sKey);` |
|         - | 7736 | `				/* Invoke the supplied callback */` |
|        27 | 7737 | `				rc = PH7_VmCallUserFunctionAp(pMap->pVm,pCallback,0,pValue,&sKey,pUserData,0);` |
|        27 | 7738 | `				PH7_MemObjRelease(&sKey);` |
|        27 | 7739 | `				if( rc == PH7_EXCEPTION ){` |
|         - | 7740 | `					/* The callback raised: propagate so the dispatcher unwinds. */` |
|       ! 0 | 7741 | `					return PH7_EXCEPTION;` |
|         - | 7742 | `				}` |
|         - | 7743 | `			}` |
|        18 | 7744 | `		}` |
|         - | 7745 | `		/* Point to the next entry */` |
|        37 | 7746 | `		pEntry = pEntry->pPrev; /* Reverse link */` |
|        19 | 7747 | `	}` |
|        23 | 7748 | `	return PH7_OK;` |
|        12 | 7749 | `}` |
|         - | 7750 | `/*` |
|         - | 7751 | ` * bool array_walk_recursive(array &$array, callback $funcname [, mixed $userdata])` |
|         - | 7752 | ` *  Apply a user function recursively to every member of an array.` |
|         - | 7753 | ` * Parameters` |
|         - | 7754 | ` *  $array` |
|         - | 7755 | ` *   The input array.` |
|         - | 7756 | ` *  $funcname` |
|         - | 7757 | ` *   Typically, funcname takes on two parameters. The array parameter's value being` |
|         - | 7758 | ` *   the first, and the key/index second.` |
|         - | 7759 | ` * Note:` |
|         - | 7760 | ` *  If funcname needs to be working with the actual values of the array, specify the first` |
|         - | 7761 | ` *  parameter of funcname as a reference. Then, any changes made to those elements will` |
|         - | 7762 | ` *  be made in the original array itself.` |
|         - | 7763 | ` *  $userdata` |
|         - | 7764 | ` *   If the optional userdata parameter is supplied, it will be passed as the third parameter` |
|         - | 7765 | ` *   to the callback funcname.` |
|         - | 7766 | ` * Return` |
|         - | 7767 | ` *  Returns TRUE on success or FALSE on failure.` |
|         - | 7768 | ` */` |
|        30 | 7769 | `static int ph7_hashmap_walk_recursive(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|         5 | 7770 | `{` |
|         - | 7771 | `	ph7_hashmap *pMap;` |
|        35 | 7772 | `	if( nArg < 2 ){` |
|         8 | 7773 | `		return PH7_VmThrowException(pCtx,` |
|         - | 7774 | `			"ArgumentCountError",` |
|         - | 7775 | `			"array_walk_recursive() expects at least 2 arguments, %d given",` |
|         2 | 7776 | `			nArg` |
|         - | 7777 | `			);` |
|         - | 7778 | `	}` |
|        31 | 7779 | `	if( nArg > 3 ){` |
|         4 | 7780 | `		return PH7_VmThrowException(pCtx,` |
|         - | 7781 | `			"ArgumentCountError",` |
|         - | 7782 | `			"array_walk_recursive() expects at most 3 arguments, %d given",` |
|         1 | 7783 | `			nArg` |
|         - | 7784 | `			);` |
|         - | 7785 | `	}` |
|        29 | 7786 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|         4 | 7787 | `		return PH7_VmThrowException(pCtx,` |
|         - | 7788 | `			"TypeError",` |
|         - | 7789 | `			"array_walk_recursive(): Argument #1 ($array) must be of type array, %s given",` |
|         1 | 7790 | `			ph7_type_name(apArg[0])` |
|         - | 7791 | `			);` |
|         - | 7792 | `	}` |
|        27 | 7793 | `	if( !ph7_value_is_callable(apArg[1]) ){` |
|        14 | 7794 | `		if( ph7_value_is_string(apArg[1]) ){` |
|         3 | 7795 | `			const char *zFunc = ph7_value_to_string(apArg[1],0);` |
|         4 | 7796 | `			return PH7_VmThrowException(pCtx,` |
|         - | 7797 | `				"TypeError",` |
|         - | 7798 | `				"array_walk_recursive(): Argument #2 ($callback) must be a valid callback, "` |
|         - | 7799 | `				"function \"%s\" not found or invalid function name",` |
|         1 | 7800 | `				zFunc` |
|         - | 7801 | `				);` |
|         - | 7802 | `		}` |
|        12 | 7803 | `		if( ph7_value_is_array(apArg[1]) ){` |
|         6 | 7804 | `			return PH7_VmThrowException(pCtx,` |
|         - | 7805 | `				"TypeError",` |
|         - | 7806 | `				"array_walk_recursive(): Argument #2 ($callback) must be a valid callback, "` |
|         - | 7807 | `				"array callback must have exactly two members"` |
|         - | 7808 | `				);` |
|         - | 7809 | `		}` |
|         6 | 7810 | `		return PH7_VmThrowException(pCtx,` |
|         - | 7811 | `			"TypeError",` |
|         - | 7812 | `			"array_walk_recursive(): Argument #2 ($callback) must be a valid callback, "` |
|         - | 7813 | `			"no array or string given"` |
|         - | 7814 | `			);` |
|         - | 7815 | `	}` |
|         - | 7816 | `	/* Point to the internal representation of the input hashmap */` |
|        13 | 7817 | `	PH7_HashmapCowSeparate(pCtx->pVm, apArg[0]);` |
|        13 | 7818 | `	pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|         - | 7819 | `	/* Perform the desired operation */` |
|        13 | 7820 | `	if( HashmapWalkRecursive(pMap,apArg[1],nArg > 2 ? apArg[2] : 0,0) == PH7_EXCEPTION ){` |
|         - | 7821 | `		/* A callback raised: propagate so the dispatcher unwinds. */` |
|       ! 0 | 7822 | `		return PH7_EXCEPTION;` |
|         - | 7823 | `	}` |
|         - | 7824 | `	/* All done, return TRUE */` |
|        13 | 7825 | `	ph7_result_bool(pCtx,1);` |
|        13 | 7826 | `	return PH7_OK;` |
|        20 | 7827 | `}` |
|         - | 7828 | `/*` |
|         - | 7829 | ` * bool array_is_list(array $array)` |
|         - | 7830 | ` *  Checks whether a given array is a list: its keys consist of consecutive` |
|         - | 7831 | ` *  integers starting at 0. An empty array is a list.` |
|         - | 7832 | ` * Return` |
|         - | 7833 | ` *  TRUE if the array is a list, FALSE otherwise.` |
|         - | 7834 | ` */` |
|         - | 7835 | `/*` |
|         - | 7836 | ` * Return TRUE if the given hashmap is a "list" [i.e: its keys are the` |
|         - | 7837 | ` * consecutive integers 0,1,2,... with no gaps]. An empty map is a list.` |
|         - | 7838 | ` * Shared by array_is_list() and the JSON encoder (vm_json.c).` |
|         - | 7839 | ` */` |
|       128 | 7840 | `PH7_PRIVATE int PH7_HashmapIsList(ph7_hashmap *pMap)` |
|         1 | 7841 | `{` |
|       129 | 7842 | `	ph7_hashmap_node *pNode = pMap->pFirst;` |
|       129 | 7843 | `	sxi64 iExpect = 0;` |
|         - | 7844 | `	sxu32 n;` |
|       265 | 7845 | `	for( n = 0 ; n < pMap->nEntry ; ++n ){` |
|       197 | 7846 | `		if( pNode->iType != HASHMAP_INT_NODE \|\| pNode->xKey.iKey != iExpect ){` |
|         - | 7847 | `			/* A non-integer key or a gap in the sequence: not a list */` |
|        61 | 7848 | `			return 0;` |
|         - | 7849 | `		}` |
|       137 | 7850 | `		++iExpect;` |
|       137 | 7851 | `		pNode = pNode->pPrev; /* Reverse link */` |
|        69 | 7852 | `	}` |
|        69 | 7853 | `	return 1;` |
|        65 | 7854 | `}` |
|        12 | 7855 | `static int ph7_hashmap_is_list(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|         1 | 7856 | `{` |
|        13 | 7857 | `	if( nArg < 1 ){` |
|       ! 0 | 7858 | `		return PH7_VmThrowException(pCtx,` |
|         - | 7859 | `			"ArgumentCountError",` |
|         - | 7860 | `			"array_is_list() expects exactly 1 argument, 0 given"` |
|         - | 7861 | `			);` |
|         - | 7862 | `	}` |
|        13 | 7863 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|       ! 0 | 7864 | `		return PH7_VmThrowException(pCtx,` |
|         - | 7865 | `			"TypeError",` |
|         - | 7866 | `			"array_is_list(): Argument #1 ($array) must be of type array, %s given",` |
|       ! 0 | 7867 | `			ph7_type_name(apArg[0])` |
|         - | 7868 | `			);` |
|         - | 7869 | `	}` |
|        13 | 7870 | `	ph7_result_bool(pCtx,PH7_HashmapIsList((ph7_hashmap *)apArg[0]->x.pOther));` |
|        13 | 7871 | `	return PH7_OK;` |
|         7 | 7872 | `}` |
|         - | 7873 | `/*` |
|         - | 7874 | ` * mixed array_first(array $array)` |
|         - | 7875 | ` * mixed array_last(array $array)` |
|         - | 7876 | ` *  Return the value of the first (respectively last) element of the array,` |
|         - | 7877 | ` *  or NULL when the array is empty. The internal array pointer is left` |
|         - | 7878 | ` *  untouched (unlike reset()/end()).` |
|         - | 7879 | ` */` |
|        20 | 7880 | `static int HashmapFirstLast(ph7_context *pCtx,int nArg,ph7_value **apArg,int bLast)` |
|         1 | 7881 | `{` |
|         - | 7882 | `	ph7_hashmap *pMap;` |
|         - | 7883 | `	ph7_hashmap_node *pNode;` |
|         - | 7884 | `	ph7_value *pVal;` |
|        21 | 7885 | `	const char *zName = bLast ? "array_last" : "array_first";` |
|        21 | 7886 | `	if( nArg < 1 ){` |
|         4 | 7887 | `		return PH7_VmThrowException(pCtx,` |
|         - | 7888 | `			"ArgumentCountError",` |
|         - | 7889 | `			"%s() expects exactly 1 argument, 0 given",` |
|         1 | 7890 | `			zName` |
|         - | 7891 | `			);` |
|         - | 7892 | `	}` |
|        19 | 7893 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|         4 | 7894 | `		return PH7_VmThrowException(pCtx,` |
|         - | 7895 | `			"TypeError",` |
|         - | 7896 | `			"%s(): Argument #1 ($array) must be of type array, %s given",` |
|         1 | 7897 | `			zName,` |
|         1 | 7898 | `			ph7_type_name(apArg[0])` |
|         - | 7899 | `			);` |
|         - | 7900 | `	}` |
|        17 | 7901 | `	pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|        17 | 7902 | `	pNode = bLast ? pMap->pLast : pMap->pFirst;` |
|        17 | 7903 | `	if( pNode == 0 ){` |
|         - | 7904 | `		/* Empty array: PHP returns NULL */` |
|         5 | 7905 | `		ph7_result_null(pCtx);` |
|         5 | 7906 | `		return PH7_OK;` |
|         - | 7907 | `	}` |
|        13 | 7908 | `	pVal = HashmapExtractNodeValue(pNode);` |
|        13 | 7909 | `	if( pVal ){` |
|        13 | 7910 | `		ph7_result_value(pCtx,pVal);` |
|         7 | 7911 | `	}else{` |
|       ! 0 | 7912 | `		ph7_result_null(pCtx);` |
|         - | 7913 | `	}` |
|        13 | 7914 | `	return PH7_OK;` |
|        11 | 7915 | `}` |
|        10 | 7916 | `static int ph7_hashmap_first(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|         1 | 7917 | `{` |
|        11 | 7918 | `	return HashmapFirstLast(pCtx,nArg,apArg,0);` |
|         1 | 7919 | `}` |
|        10 | 7920 | `static int ph7_hashmap_last(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|         1 | 7921 | `{` |
|        11 | 7922 | `	return HashmapFirstLast(pCtx,nArg,apArg,1);` |
|         1 | 7923 | `}` |
|         - | 7924 | `/*` |
|         - | 7925 | ` * Fetch the element identified by 'pKey' from 'pRow' which may be either an` |
|         - | 7926 | ` * array (hashmap lookup) or an object (public attribute lookup). Used by` |
|         - | 7927 | ` * array_column() for both the column value and the index key.` |
|         - | 7928 | ` * Returns a borrowed pointer to the value, or NULL when the row is not a` |
|         - | 7929 | ` * container or the key is absent.` |
|         - | 7930 | ` */` |
|        32 | 7931 | `static ph7_value * HashmapColumnFetch(ph7_vm *pVm,ph7_value *pRow,ph7_value *pKey)` |
|         1 | 7932 | `{` |
|        33 | 7933 | `	if( ph7_value_is_array(pRow) ){` |
|         - | 7934 | `		ph7_hashmap_node *pNode;` |
|        25 | 7935 | `		if( PH7_HashmapLookup((ph7_hashmap *)pRow->x.pOther,pKey,&pNode) == SXRET_OK ){` |
|        21 | 7936 | `			return HashmapExtractNodeValue(pNode);` |
|         1 | 7937 | `		}` |
|        11 | 7938 | `	}else if( ph7_value_is_object(pRow) ){` |
|         - | 7939 | `		ph7_value sName;` |
|         - | 7940 | `		const char *zName;` |
|         - | 7941 | `		ph7_value *pAttr;` |
|         - | 7942 | `		/* Stringify a *copy* of the key (objects address attributes by name);` |
|         - | 7943 | `		 * never mutate pKey itself or the array-lookup path would break. */` |
|         9 | 7944 | `		PH7_MemObjInit(pVm,&sName);` |
|         9 | 7945 | `		PH7_MemObjStore(pKey,&sName);` |
|         9 | 7946 | `		zName = ph7_value_to_string(&sName,0); /* NUL-terminated */` |
|         9 | 7947 | `		pAttr = ph7_object_fetch_attr(pRow,zName);` |
|         9 | 7948 | `		PH7_MemObjRelease(&sName);` |
|         9 | 7949 | `		return pAttr;` |
|         - | 7950 | `	}` |
|         5 | 7951 | `	return 0;` |
|        17 | 7952 | `}` |
|         - | 7953 | `/*` |
|         - | 7954 | ` * array array_column(array $array, int\|string\|null $column_key, int\|string\|null $index_key = null)` |
|         - | 7955 | ` *  Returns the values from a single column of the input, identified by` |
|         - | 7956 | ` *  $column_key. Optionally indexes the result by the $index_key column.` |
|         - | 7957 | ` *  A NULL $column_key collects the whole row. Rows missing the column are` |
|         - | 7958 | ` *  skipped; rows missing the index key are appended with a numeric key.` |
|         - | 7959 | ` *  Each row may be an array or an object.` |
|         - | 7960 | ` */` |
|        12 | 7961 | `static int ph7_hashmap_column(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|         1 | 7962 | `{` |
|         - | 7963 | `	ph7_hashmap_node *pNode;` |
|         - | 7964 | `	ph7_hashmap *pMap;` |
|         - | 7965 | `	ph7_value *pArray;` |
|         - | 7966 | `	ph7_value *pRow;` |
|         - | 7967 | `	ph7_value *pCol;` |
|         - | 7968 | `	ph7_value *pIdx;` |
|         - | 7969 | `	int bWantCol;` |
|         - | 7970 | `	int bWantIdx;` |
|         - | 7971 | `	sxu32 n;` |
|        13 | 7972 | `	if( nArg < 2 ){` |
|       ! 0 | 7973 | `		return PH7_VmThrowException(pCtx,` |
|         - | 7974 | `			"ArgumentCountError",` |
|         - | 7975 | `			"array_column() expects at least 2 arguments, %d given",` |
|       ! 0 | 7976 | `			nArg` |
|         - | 7977 | `			);` |
|         - | 7978 | `	}` |
|        13 | 7979 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|       ! 0 | 7980 | `		return PH7_VmThrowException(pCtx,` |
|         - | 7981 | `			"TypeError",` |
|         - | 7982 | `			"array_column(): Argument #1 ($array) must be of type array, %s given",` |
|       ! 0 | 7983 | `			ph7_type_name(apArg[0])` |
|         - | 7984 | `			);` |
|         - | 7985 | `	}` |
|        13 | 7986 | `	pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|        13 | 7987 | `	pArray = ph7_context_new_array(pCtx);` |
|        13 | 7988 | `	if( pArray == 0 ){` |
|       ! 0 | 7989 | `		ph7_result_null(pCtx);` |
|       ! 0 | 7990 | `		return PH7_OK;` |
|         - | 7991 | `	}` |
|         - | 7992 | `	/* A NULL column_key means "collect the entire row". */` |
|        13 | 7993 | `	bWantCol = !ph7_value_is_null(apArg[1]);` |
|        13 | 7994 | `	bWantIdx = (nArg > 2 && !ph7_value_is_null(apArg[2]));` |
|        13 | 7995 | `	pNode = pMap->pFirst;` |
|        33 | 7996 | `	for( n = 0 ; n < pMap->nEntry ; ++n ){` |
|        21 | 7997 | `		pRow = HashmapExtractNodeValue(pNode);` |
|        21 | 7998 | `		pNode = pNode->pPrev; /* Advance now so 'continue' is safe */` |
|        21 | 7999 | `		if( pRow == 0 ){` |
|       ! 0 | 8000 | `			continue;` |
|         - | 8001 | `		}` |
|        21 | 8002 | `		if( bWantCol ){` |
|        19 | 8003 | `			pCol = HashmapColumnFetch(pMap->pVm,pRow,apArg[1]);` |
|        19 | 8004 | `			if( pCol == 0 ){` |
|         - | 8005 | `				/* Row lacks the requested column: skip it (PHP semantics). */` |
|         3 | 8006 | `				continue;` |
|         - | 8007 | `			}` |
|         9 | 8008 | `		}else{` |
|         3 | 8009 | `			pCol = pRow;` |
|         - | 8010 | `		}` |
|        19 | 8011 | `		pIdx = bWantIdx ? HashmapColumnFetch(pMap->pVm,pRow,apArg[2]) : 0;` |
|        19 | 8012 | `		if( pIdx ){` |
|        13 | 8013 | `			ph7_array_add_elem(pArray,pIdx,pCol);` |
|         7 | 8014 | `		}else{` |
|         7 | 8015 | `			ph7_array_add_elem(pArray,0,pCol); /* Auto-index */` |
|         - | 8016 | `		}` |
|        10 | 8017 | `	}` |
|        13 | 8018 | `	ph7_result_value(pCtx,pArray);` |
|        13 | 8019 | `	return PH7_OK;` |
|         7 | 8020 | `}` |
|         - | 8021 | `/*` |
|         - | 8022 | ` * Shared core for array_find/array_find_key/array_any/array_all (PHP 8.4).` |
|         - | 8023 | ` * Invokes $callback($value, $key) over each entry and reports the first node` |
|         - | 8024 | ` * whose truthiness equals 'bWant'. Propagates a callback exception as` |
|         - | 8025 | ` * PH7_EXCEPTION; sets *ppMatch to the matching node (or NULL if none).` |
|         - | 8026 | ` */` |
|        28 | 8027 | `static sxi32 HashmapCallbackSearch(` |
|         - | 8028 | `	ph7_context *pCtx,int nArg,ph7_value **apArg,` |
|         - | 8029 | `	const char *zName,            /* Function name for diagnostics */` |
|         - | 8030 | `	int bWant,                    /* Truthiness being hunted for */` |
|         - | 8031 | `	ph7_hashmap_node **ppMatch    /* OUT: first matching node or NULL */` |
|         - | 8032 | `	)` |
|         1 | 8033 | `{` |
|         - | 8034 | `	ph7_hashmap_node *pEntry;` |
|         - | 8035 | `	ph7_hashmap *pMap;` |
|         - | 8036 | `	ph7_value *pValue;` |
|         - | 8037 | `	ph7_value *apCbArg[2];` |
|         - | 8038 | `	ph7_value sKey;` |
|         - | 8039 | `	ph7_value sResult;` |
|         - | 8040 | `	sxi32 rc;` |
|         - | 8041 | `	sxu32 n;` |
|        29 | 8042 | `	*ppMatch = 0;` |
|        29 | 8043 | `	if( nArg < 2 ){` |
|       ! 0 | 8044 | `		return PH7_VmThrowException(pCtx,` |
|         - | 8045 | `			"ArgumentCountError",` |
|         - | 8046 | `			"%s() expects exactly 2 arguments, %d given",` |
|       ! 0 | 8047 | `			zName,nArg` |
|         - | 8048 | `			);` |
|         - | 8049 | `	}` |
|        29 | 8050 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|       ! 0 | 8051 | `		return PH7_VmThrowException(pCtx,` |
|         - | 8052 | `			"TypeError",` |
|         - | 8053 | `			"%s(): Argument #1 ($array) must be of type array, %s given",` |
|       ! 0 | 8054 | `			zName,ph7_type_name(apArg[0])` |
|         - | 8055 | `			);` |
|         - | 8056 | `	}` |
|        29 | 8057 | `	if( !ph7_value_is_callable(apArg[1]) ){` |
|       ! 0 | 8058 | `		return PH7_VmThrowException(pCtx,` |
|         - | 8059 | `			"TypeError",` |
|         - | 8060 | `			"%s(): Argument #2 ($callback) must be a valid callback, %s given",` |
|       ! 0 | 8061 | `			zName,ph7_type_name(apArg[1])` |
|         - | 8062 | `			);` |
|         - | 8063 | `	}` |
|        29 | 8064 | `	pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|        29 | 8065 | `	pEntry = pMap->pFirst;` |
|        29 | 8066 | `	PH7_MemObjInit(pMap->pVm,&sKey);` |
|        29 | 8067 | `	sKey.nIdx = SXU32_HIGH;    /* Mark as constant */` |
|        29 | 8068 | `	PH7_MemObjInit(pMap->pVm,&sResult);` |
|        29 | 8069 | `	sResult.nIdx = SXU32_HIGH; /* Mark as constant */` |
|        73 | 8070 | `	for( n = 0 ; n < pMap->nEntry ; ++n ){` |
|        59 | 8071 | `		pValue = HashmapExtractNodeValue(pEntry);` |
|        59 | 8072 | `		if( pValue ){` |
|         - | 8073 | `			/* The callback receives ($value, $key). */` |
|        59 | 8074 | `			PH7_HashmapExtractNodeKey(pEntry,&sKey);` |
|        59 | 8075 | `			apCbArg[0] = pValue;` |
|        59 | 8076 | `			apCbArg[1] = &sKey;` |
|        59 | 8077 | `			rc = PH7_VmCallUserFunction(pMap->pVm,apArg[1],2,apCbArg,&sResult);` |
|        59 | 8078 | `			if( rc == PH7_EXCEPTION ){` |
|         - | 8079 | `				/* The callback raised: propagate so the dispatcher unwinds. */` |
|       ! 0 | 8080 | `				PH7_MemObjRelease(&sKey);` |
|       ! 0 | 8081 | `				PH7_MemObjRelease(&sResult);` |
|       ! 0 | 8082 | `				return PH7_EXCEPTION;` |
|         - | 8083 | `			}` |
|        59 | 8084 | `			if( rc == SXRET_OK && (ph7_value_to_bool(&sResult) ? 1 : 0) == bWant ){` |
|        15 | 8085 | `				*ppMatch = pEntry;` |
|        15 | 8086 | `				break;` |
|         - | 8087 | `			}` |
|        22 | 8088 | `		}` |
|        45 | 8089 | `		pEntry = pEntry->pPrev; /* Reverse link */` |
|        23 | 8090 | `	}` |
|        29 | 8091 | `	PH7_MemObjRelease(&sKey);` |
|        29 | 8092 | `	PH7_MemObjRelease(&sResult);` |
|        29 | 8093 | `	return PH7_OK;` |
|        15 | 8094 | `}` |
|         - | 8095 | `/*` |
|         - | 8096 | ` * mixed array_find(array $array, callable $callback)` |
|         - | 8097 | ` *  Returns the value of the first element for which $callback($value,$key)` |
|         - | 8098 | ` *  is truthy, or NULL if none match.` |
|         - | 8099 | ` */` |
|         6 | 8100 | `static int ph7_hashmap_find(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|         1 | 8101 | `{` |
|         - | 8102 | `	ph7_hashmap_node *pMatch;` |
|         - | 8103 | `	ph7_value *pVal;` |
|         - | 8104 | `	sxi32 rc;` |
|         7 | 8105 | `	rc = HashmapCallbackSearch(pCtx,nArg,apArg,"array_find",1,&pMatch);` |
|         7 | 8106 | `	if( rc != PH7_OK ){` |
|       ! 0 | 8107 | `		return rc;` |
|         - | 8108 | `	}` |
|         7 | 8109 | `	if( pMatch && (pVal = HashmapExtractNodeValue(pMatch)) != 0 ){` |
|         5 | 8110 | `		ph7_result_value(pCtx,pVal);` |
|         3 | 8111 | `	}else{` |
|         3 | 8112 | `		ph7_result_null(pCtx);` |
|         - | 8113 | `	}` |
|         7 | 8114 | `	return PH7_OK;` |
|         4 | 8115 | `}` |
|         - | 8116 | `/*` |
|         - | 8117 | ` * mixed array_find_key(array $array, callable $callback)` |
|         - | 8118 | ` *  Returns the key of the first element for which $callback($value,$key)` |
|         - | 8119 | ` *  is truthy, or NULL if none match.` |
|         - | 8120 | ` */` |
|         6 | 8121 | `static int ph7_hashmap_find_key(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|         1 | 8122 | `{` |
|         - | 8123 | `	ph7_hashmap_node *pMatch;` |
|         - | 8124 | `	sxi32 rc;` |
|         7 | 8125 | `	rc = HashmapCallbackSearch(pCtx,nArg,apArg,"array_find_key",1,&pMatch);` |
|         7 | 8126 | `	if( rc != PH7_OK ){` |
|       ! 0 | 8127 | `		return rc;` |
|         - | 8128 | `	}` |
|         7 | 8129 | `	if( pMatch == 0 ){` |
|         3 | 8130 | `		ph7_result_null(pCtx);` |
|         6 | 8131 | `	}else if( pMatch->iType == HASHMAP_INT_NODE ){` |
|         3 | 8132 | `		ph7_result_int64(pCtx,pMatch->xKey.iKey);` |
|         2 | 8133 | `	}else{` |
|         4 | 8134 | `		ph7_result_string(pCtx,` |
|         2 | 8135 | `			(const char *)SyBlobData(&pMatch->xKey.sKey),` |
|         2 | 8136 | `			(int)SyBlobLength(&pMatch->xKey.sKey));` |
|         - | 8137 | `	}` |
|         7 | 8138 | `	return PH7_OK;` |
|         4 | 8139 | `}` |
|         - | 8140 | `/*` |
|         - | 8141 | ` * bool array_any(array $array, callable $callback)` |
|         - | 8142 | ` *  Returns TRUE if $callback($value,$key) is truthy for at least one element.` |
|         - | 8143 | ` *  FALSE for an empty array.` |
|         - | 8144 | ` */` |
|         8 | 8145 | `static int ph7_hashmap_any(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|         1 | 8146 | `{` |
|         - | 8147 | `	ph7_hashmap_node *pMatch;` |
|         - | 8148 | `	sxi32 rc;` |
|         9 | 8149 | `	rc = HashmapCallbackSearch(pCtx,nArg,apArg,"array_any",1,&pMatch);` |
|         9 | 8150 | `	if( rc != PH7_OK ){` |
|       ! 0 | 8151 | `		return rc;` |
|         - | 8152 | `	}` |
|         9 | 8153 | `	ph7_result_bool(pCtx,pMatch != 0);` |
|         9 | 8154 | `	return PH7_OK;` |
|         5 | 8155 | `}` |
|         - | 8156 | `/*` |
|         - | 8157 | ` * bool array_all(array $array, callable $callback)` |
|         - | 8158 | ` *  Returns TRUE if $callback($value,$key) is truthy for every element (and for` |
|         - | 8159 | ` *  an empty array). Hunts for the first falsy element: its absence means "all".` |
|         - | 8160 | ` */` |
|         8 | 8161 | `static int ph7_hashmap_all(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|         1 | 8162 | `{` |
|         - | 8163 | `	ph7_hashmap_node *pMatch;` |
|         - | 8164 | `	sxi32 rc;` |
|         9 | 8165 | `	rc = HashmapCallbackSearch(pCtx,nArg,apArg,"array_all",0,&pMatch);` |
|         9 | 8166 | `	if( rc != PH7_OK ){` |
|       ! 0 | 8167 | `		return rc;` |
|         - | 8168 | `	}` |
|         9 | 8169 | `	ph7_result_bool(pCtx,pMatch == 0);` |
|         9 | 8170 | `	return PH7_OK;` |
|         5 | 8171 | `}` |
|         - | 8172 | `/*` |
|         - | 8173 | ` * The iterator_*() family — walk a Traversable via the shared PH7_VmIteratorWalk` |
|         - | 8174 | ` * helper (the reusable form of the foreach Iterator protocol).` |
|         - | 8175 | ` */` |
|         - | 8176 | `/* Step shared by iterator_to_array (pArray set) and iterator_count (pArray NULL). */` |
|         - | 8177 | `struct IterCollect { ph7_value *pArray; int bPreserve; sxi64 nCount; };` |
|        70 | 8178 | `static sxi32 IterCollectStep(ph7_vm *pVm, ph7_value *pKey, ph7_value *pValue, void *pUserData)` |
|         4 | 8179 | `{` |
|        74 | 8180 | `	struct IterCollect *p = (struct IterCollect *)pUserData;` |
|        35 | 8181 | `	(void)pVm;` |
|        74 | 8182 | `	p->nCount++;` |
|        74 | 8183 | `	if( p->pArray ){` |
|         - | 8184 | `		/* preserve_keys: insert with the iterator key (later wins on collision);` |
|         - | 8185 | `		 * otherwise append with an auto-assigned int index. */` |
|        66 | 8186 | `		ph7_array_add_elem(p->pArray, p->bPreserve ? pKey : 0, pValue);` |
|        31 | 8187 | `	}` |
|        74 | 8188 | `	return SXRET_OK;` |
|         4 | 8189 | `}` |
|         - | 8190 | `/*` |
|         - | 8191 | ` * array iterator_to_array(Traversable\|array $iterator, bool $preserve_keys = true)` |
|         - | 8192 | ` */` |
|        26 | 8193 | `static int ph7_iterator_to_array(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|         4 | 8194 | `{` |
|         - | 8195 | `	struct IterCollect sCol;` |
|         - | 8196 | `	ph7_value *pArray;` |
|         - | 8197 | `	sxi32 rc;` |
|        30 | 8198 | `	if( nArg < 1 ){ ph7_result_null(pCtx); return PH7_OK; }` |
|        30 | 8199 | `	pArray = ph7_context_new_array(pCtx);` |
|        30 | 8200 | `	if( pArray == 0 ){ ph7_result_null(pCtx); return PH7_OK; }` |
|        30 | 8201 | `	sCol.pArray = pArray;` |
|        30 | 8202 | `	sCol.bPreserve = (nArg > 1) ? ph7_value_to_bool(apArg[1]) : 1;` |
|        30 | 8203 | `	sCol.nCount = 0;` |
|        30 | 8204 | `	if( ph7_value_is_array(apArg[0]) ){` |
|         - | 8205 | `		/* PHP 8.2 accepts a plain array: copy it (preserving or renumbering keys). */` |
|         3 | 8206 | `		ph7_hashmap *pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|         3 | 8207 | `		ph7_hashmap_node *pEntry = pMap->pFirst;` |
|         - | 8208 | `		sxu32 n;` |
|         9 | 8209 | `		for( n = 0 ; n < pMap->nEntry ; n++ ){` |
|         - | 8210 | `			ph7_value sKey, *pVal;` |
|         7 | 8211 | `			PH7_MemObjInit(pCtx->pVm,&sKey);` |
|         7 | 8212 | `			PH7_HashmapExtractNodeKey(pEntry,&sKey);` |
|         7 | 8213 | `			pVal = (ph7_value *)SySetAt(&pCtx->pVm->aMemObj,pEntry->nValIdx);` |
|         7 | 8214 | `			if( pVal ){ ph7_array_add_elem(pArray, sCol.bPreserve ? &sKey : 0, pVal); }` |
|         7 | 8215 | `			PH7_MemObjRelease(&sKey);` |
|         7 | 8216 | `			pEntry = pEntry->pPrev;` |
|         4 | 8217 | `		}` |
|         3 | 8218 | `		ph7_result_value(pCtx,pArray);` |
|         3 | 8219 | `		return PH7_OK;` |
|         - | 8220 | `	}` |
|        28 | 8221 | `	rc = PH7_VmIteratorWalk(pCtx->pVm, apArg[0], IterCollectStep, &sCol);` |
|        28 | 8222 | `	if( rc == PH7_EXCEPTION \|\| rc == PH7_ABORT ){ return rc; }` |
|        26 | 8223 | `	if( rc == SXERR_NOTIMPLEMENTED ){` |
|       ! 0 | 8224 | `		return PH7_VmThrowException(pCtx,"TypeError",` |
|         - | 8225 | `			"iterator_to_array(): Argument #1 ($iterator) must be of type Traversable\|array, %s given",` |
|       ! 0 | 8226 | `			ph7_type_name(apArg[0]));` |
|         - | 8227 | `	}` |
|        26 | 8228 | `	ph7_result_value(pCtx,pArray);` |
|        26 | 8229 | `	return PH7_OK;` |
|        17 | 8230 | `}` |
|         - | 8231 | `/*` |
|         - | 8232 | ` * int iterator_count(Traversable\|array $iterator)` |
|         - | 8233 | ` */` |
|         6 | 8234 | `static int ph7_iterator_count(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|         1 | 8235 | `{` |
|         - | 8236 | `	struct IterCollect sCol;` |
|         - | 8237 | `	sxi32 rc;` |
|         7 | 8238 | `	if( nArg < 1 ){ ph7_result_int(pCtx,0); return PH7_OK; }` |
|         7 | 8239 | `	if( ph7_value_is_array(apArg[0]) ){` |
|         3 | 8240 | `		ph7_result_int64(pCtx, (ph7_int64)((ph7_hashmap *)apArg[0]->x.pOther)->nEntry);` |
|         3 | 8241 | `		return PH7_OK;` |
|         - | 8242 | `	}` |
|         5 | 8243 | `	sCol.pArray = 0; sCol.bPreserve = 0; sCol.nCount = 0;` |
|         5 | 8244 | `	rc = PH7_VmIteratorWalk(pCtx->pVm, apArg[0], IterCollectStep, &sCol);` |
|         5 | 8245 | `	if( rc == PH7_EXCEPTION \|\| rc == PH7_ABORT ){ return rc; }` |
|         5 | 8246 | `	if( rc == SXERR_NOTIMPLEMENTED ){` |
|       ! 0 | 8247 | `		return PH7_VmThrowException(pCtx,"TypeError",` |
|         - | 8248 | `			"iterator_count(): Argument #1 ($iterator) must be of type Traversable\|array, %s given",` |
|       ! 0 | 8249 | `			ph7_type_name(apArg[0]));` |
|         - | 8250 | `	}` |
|         5 | 8251 | `	ph7_result_int64(pCtx, sCol.nCount);` |
|         5 | 8252 | `	return PH7_OK;` |
|         4 | 8253 | `}` |
|         - | 8254 | `/* iterator_apply step: call the fixed callback with $args each iteration. The` |
|         - | 8255 | ` * arg pointers are resolved fresh per step because the iterator's own methods` |
|         - | 8256 | ` * run user code between iterations and may reallocate the aMemObj pool. */` |
|         - | 8257 | `struct IterApply { ph7_value *pCallback; ph7_value *pArgsArray; sxi64 nCount; };` |
|        24 | 8258 | `static sxi32 IterApplyStep(ph7_vm *pVm, ph7_value *pKey, ph7_value *pValue, void *pUserData)` |
|         1 | 8259 | `{` |
|        25 | 8260 | `	struct IterApply *p = (struct IterApply *)pUserData;` |
|         - | 8261 | `	ph7_value sResult;` |
|         - | 8262 | `	SySet aArg;` |
|         - | 8263 | `	sxi32 rc;` |
|         - | 8264 | `	int bContinue;` |
|        12 | 8265 | `	(void)pKey; (void)pValue; /* iterator_apply does NOT pass the element to the callback */` |
|        25 | 8266 | `	SySetInit(&aArg,&pVm->sAllocator,sizeof(ph7_value *));` |
|        25 | 8267 | `	if( p->pArgsArray && (p->pArgsArray->iFlags & MEMOBJ_HASHMAP) ){` |
|         9 | 8268 | `		ph7_hashmap *pMap = (ph7_hashmap *)p->pArgsArray->x.pOther;` |
|         9 | 8269 | `		ph7_hashmap_node *pEntry = pMap->pFirst;` |
|         - | 8270 | `		sxu32 n;` |
|        17 | 8271 | `		for( n = 0 ; n < pMap->nEntry ; n++ ){` |
|         9 | 8272 | `			ph7_value *pVal = (ph7_value *)SySetAt(&pVm->aMemObj,pEntry->nValIdx);` |
|         9 | 8273 | `			if( pVal ){ SySetPut(&aArg,(const void *)&pVal); }` |
|         9 | 8274 | `			pEntry = pEntry->pPrev;` |
|         5 | 8275 | `		}` |
|         4 | 8276 | `	}` |
|        25 | 8277 | `	PH7_MemObjInit(pVm,&sResult);` |
|        37 | 8278 | `	rc = PH7_VmCallUserFunction(pVm, p->pCallback, (int)SySetUsed(&aArg),` |
|        24 | 8279 | `		(ph7_value **)SySetBasePtr(&aArg), &sResult);` |
|        25 | 8280 | `	SySetRelease(&aArg);` |
|        25 | 8281 | `	if( rc == PH7_EXCEPTION \|\| rc == PH7_ABORT ){ PH7_MemObjRelease(&sResult); return rc; }` |
|        23 | 8282 | `	p->nCount++;` |
|        23 | 8283 | `	PH7_MemObjToBool(&sResult);` |
|        23 | 8284 | `	bContinue = (sResult.x.iVal != 0);` |
|        23 | 8285 | `	PH7_MemObjRelease(&sResult);` |
|        23 | 8286 | `	return bContinue ? SXRET_OK : SXERR_EOF; /* falsy return stops iteration */` |
|        13 | 8287 | `}` |
|         - | 8288 | `/*` |
|         - | 8289 | ` * int iterator_apply(Traversable $iterator, callable $callback, array $args = [])` |
|         - | 8290 | ` */` |
|         8 | 8291 | `static int ph7_iterator_apply(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|         1 | 8292 | `{` |
|         - | 8293 | `	struct IterApply sApp;` |
|         - | 8294 | `	sxi32 rc;` |
|         9 | 8295 | `	if( nArg < 2 ){ ph7_result_int(pCtx,0); return PH7_OK; }` |
|         9 | 8296 | `	if( !ph7_value_is_callable(apArg[1]) ){` |
|       ! 0 | 8297 | `		return PH7_VmThrowException(pCtx,"TypeError",` |
|         - | 8298 | `			"iterator_apply(): Argument #2 ($callback) must be a valid callback");` |
|         - | 8299 | `	}` |
|         9 | 8300 | `	sApp.pCallback = apArg[1];` |
|         9 | 8301 | `	sApp.pArgsArray = (nArg > 2 && ph7_value_is_array(apArg[2])) ? apArg[2] : 0;` |
|         9 | 8302 | `	sApp.nCount = 0;` |
|         9 | 8303 | `	rc = PH7_VmIteratorWalk(pCtx->pVm, apArg[0], IterApplyStep, &sApp);` |
|         9 | 8304 | `	if( rc == PH7_EXCEPTION \|\| rc == PH7_ABORT ){ return rc; }` |
|         7 | 8305 | `	if( rc == SXERR_NOTIMPLEMENTED ){` |
|       ! 0 | 8306 | `		return PH7_VmThrowException(pCtx,"TypeError",` |
|         - | 8307 | `			"iterator_apply(): Argument #1 ($iterator) must be of type Traversable, %s given",` |
|       ! 0 | 8308 | `			ph7_type_name(apArg[0]));` |
|         - | 8309 | `	}` |
|         7 | 8310 | `	ph7_result_int64(pCtx, sApp.nCount);` |
|         7 | 8311 | `	return PH7_OK;` |
|         5 | 8312 | `}` |
|         - | 8313 | `/*` |
|         - | 8314 | ` * Table of hashmap functions.` |
|         - | 8315 | ` */` |
|         - | 8316 | `static const ph7_builtin_func aHashmapFunc[] = {` |
|         - | 8317 | `	{"iterator_to_array",  ph7_iterator_to_array },` |
|         - | 8318 | `	{"iterator_count",     ph7_iterator_count },` |
|         - | 8319 | `	{"iterator_apply",     ph7_iterator_apply },` |
|         - | 8320 | `	{"count",             ph7_hashmap_count },` |
|         - | 8321 | `	{"sizeof",            ph7_hashmap_count },` |
|         - | 8322 | `	{"array_key_exists",  ph7_hashmap_key_exists },` |
|         - | 8323 | `	{"array_pop",         ph7_hashmap_pop     },` |
|         - | 8324 | `	{"array_push",        ph7_hashmap_push    },` |
|         - | 8325 | `	{"array_shift",       ph7_hashmap_shift   },` |
|         - | 8326 | `	{"array_product",     ph7_hashmap_product },` |
|         - | 8327 | `	{"array_sum",         ph7_hashmap_sum     },` |
|         - | 8328 | `	{"array_keys",        ph7_hashmap_keys    },` |
|         - | 8329 | `	{"array_values",      ph7_hashmap_values  },` |
|         - | 8330 | `	{"array_same",        ph7_hashmap_same    },  /* Symisc eXtension */` |
|         - | 8331 | `	{"array_merge",       ph7_hashmap_merge   },` |
|         - | 8332 | `	{"array_slice",       ph7_hashmap_slice   },` |
|         - | 8333 | `	{"array_splice",      ph7_hashmap_splice  },` |
|         - | 8334 | `	{"array_search",      ph7_hashmap_search  },` |
|         - | 8335 | `	{"array_diff",        ph7_hashmap_diff    },` |
|         - | 8336 | `	{"array_udiff",       ph7_hashmap_udiff   },` |
|         - | 8337 | `	{"array_diff_assoc",  ph7_hashmap_diff_assoc },` |
|         - | 8338 | `	{"array_diff_uassoc", ph7_hashmap_diff_uassoc },` |
|         - | 8339 | `	{"array_diff_key",    ph7_hashmap_diff_key },` |
|         - | 8340 | `	{"array_intersect",   ph7_hashmap_intersect},` |
|         - | 8341 | `	{"array_intersect_assoc", ph7_hashmap_intersect_assoc},` |
|         - | 8342 | `	{"array_uintersect",  ph7_hashmap_uintersect},` |
|         - | 8343 | `	{"array_intersect_key",   ph7_hashmap_intersect_key},` |
|         - | 8344 | `	{"array_copy",        ph7_hashmap_copy    },` |
|         - | 8345 | `	{"array_erase",       ph7_hashmap_erase   },` |
|         - | 8346 | `	{"array_fill",        ph7_hashmap_fill    },` |
|         - | 8347 | `	{"array_fill_keys",   ph7_hashmap_fill_keys},` |
|         - | 8348 | `	{"array_combine",     ph7_hashmap_combine },` |
|         - | 8349 | `	{"array_reverse",     ph7_hashmap_reverse },` |
|         - | 8350 | `	{"array_unique",      ph7_hashmap_unique  },` |
|         - | 8351 | `	{"array_flip",        ph7_hashmap_flip    },` |
|         - | 8352 | `	{"array_rand",        ph7_hashmap_rand    },` |
|         - | 8353 | `	{"array_chunk",       ph7_hashmap_chunk   },` |
|         - | 8354 | `	{"array_pad",         ph7_hashmap_pad     },` |
|         - | 8355 | `	{"array_replace",     ph7_hashmap_replace },` |
|         - | 8356 | `	{"array_filter",      ph7_hashmap_filter  },` |
|         - | 8357 | `	{"array_map",         ph7_hashmap_map     },` |
|         - | 8358 | `	{"array_column",      ph7_hashmap_column  },` |
|         - | 8359 | `	{"array_is_list",     ph7_hashmap_is_list },` |
|         - | 8360 | `	{"array_first",       ph7_hashmap_first   },` |
|         - | 8361 | `	{"array_last",        ph7_hashmap_last    },` |
|         - | 8362 | `	{"array_find",        ph7_hashmap_find    },` |
|         - | 8363 | `	{"array_find_key",    ph7_hashmap_find_key},` |
|         - | 8364 | `	{"array_any",         ph7_hashmap_any     },` |
|         - | 8365 | `	{"array_all",         ph7_hashmap_all     },` |
|         - | 8366 | `	{"array_reduce",      ph7_hashmap_reduce  },` |
|         - | 8367 | `	{"array_walk",        ph7_hashmap_walk    },` |
|         - | 8368 | `	{"array_walk_recursive", ph7_hashmap_walk_recursive },` |
|         - | 8369 | `	{"in_array",          ph7_hashmap_in_array},` |
|         - | 8370 | `	{"sort",              ph7_hashmap_sort    },` |
|         - | 8371 | `	{"asort",             ph7_hashmap_asort   },` |
|         - | 8372 | `	{"arsort",            ph7_hashmap_arsort  },` |
|         - | 8373 | `	{"ksort",             ph7_hashmap_ksort   },` |
|         - | 8374 | `	{"krsort",            ph7_hashmap_krsort  },` |
|         - | 8375 | `	{"rsort",             ph7_hashmap_rsort   },` |
|         - | 8376 | `	{"usort",             ph7_hashmap_usort   },` |
|         - | 8377 | `	{"uasort",            ph7_hashmap_uasort  },` |
|         - | 8378 | `	{"uksort",            ph7_hashmap_uksort  },` |
|         - | 8379 | `	{"shuffle",           ph7_hashmap_shuffle },` |
|         - | 8380 | `	{"range",             ph7_hashmap_range   },` |
|         - | 8381 | `	{"current",           ph7_hashmap_current },` |
|         - | 8382 | `	{"each",              ph7_hashmap_each    },` |
|         - | 8383 | `	{"pos",               ph7_hashmap_current },` |
|         - | 8384 | `	{"next",              ph7_hashmap_next    },` |
|         - | 8385 | `	{"prev",              ph7_hashmap_prev    },` |
|         - | 8386 | `	{"end",               ph7_hashmap_end     },` |
|         - | 8387 | `	{"reset",             ph7_hashmap_reset   },` |
|         - | 8388 | `	{"key",               ph7_hashmap_simple_key }` |
|         - | 8389 | `};` |
|         - | 8390 | `/*` |
|         - | 8391 | ` * Register the built-in hashmap functions defined above.` |
|         - | 8392 | ` */` |
|      3472 | 8393 | `PH7_PRIVATE void PH7_RegisterHashmapFunctions(ph7_vm *pVm)` |
|         5 | 8394 | `{` |
|         - | 8395 | `	sxu32 n;` |
|    253461 | 8396 | `	for( n = 0 ; n < SX_ARRAYSIZE(aHashmapFunc) ; n++ ){` |
|    249989 | 8397 | `		ph7_create_function(&(*pVm),aHashmapFunc[n].zName,aHashmapFunc[n].xFunc,0);` |
|    124997 | 8398 | `	}` |
|      3477 | 8399 | `}` |
|         - | 8400 | `/*` |
|         - | 8401 | ` * Dump a hashmap instance and it's entries and the store the dump in` |
|         - | 8402 | ` * the BLOB given as the first argument.` |
|         - | 8403 | ` * This function is typically invoked when the user issue a call to` |
|         - | 8404 | ` * [var_dump(),var_export(),print_r(),...]` |
|         - | 8405 | ` * This function SXRET_OK on success. Any other return value including` |
|         - | 8406 | ` * SXERR_LIMIT(infinite recursion) indicates failure.` |
|         - | 8407 | ` */` |
|         - | 8408 | `/*` |
|         - | 8409 | ` * Dump the entries of a hashmap [i.e: the key/value lines between the opening` |
|         - | 8410 | ` * '{' and the closing '}'] in the var_dump/print_r style. Factored out of` |
|         - | 8411 | ` * PH7_HashmapDump so the var_dump object renderer can reuse it for a` |
|         - | 8412 | ` * __debugInfo() array body (which carries an object header, not "array(N)").` |
|         - | 8413 | ` * Returns SXERR_LIMIT if a nested value hit the depth cap.` |
|         - | 8414 | ` */` |
|        26 | 8415 | `PH7_PRIVATE sxi32 PH7_HashmapDumpEntries(SyBlob *pOut,ph7_hashmap *pMap,int ShowType,int nTab,int nDepth)` |
|         3 | 8416 | `{` |
|        29 | 8417 | `	ph7_hashmap_node *pEntry = pMap->pFirst;` |
|         - | 8418 | `	ph7_value *pObj;` |
|        29 | 8419 | `	sxu32 n = 0;` |
|         - | 8420 | `	int isRef;` |
|        29 | 8421 | `	sxi32 rc = SXRET_OK;` |
|         - | 8422 | `	int i;` |
|        44 | 8423 | `	for(;;){` |
|        91 | 8424 | `		if( n >= pMap->nEntry ){` |
|        29 | 8425 | `			break;` |
|         - | 8426 | `		}` |
|       127 | 8427 | `		for( i = 0 ; i < nTab ; i++ ){` |
|        65 | 8428 | `			SyBlobAppend(&(*pOut)," ",sizeof(char));` |
|        34 | 8429 | `		}` |
|         - | 8430 | `		/* Dump key */` |
|        65 | 8431 | `		if( pEntry->iType == HASHMAP_INT_NODE){` |
|        33 | 8432 | `			SyBlobFormat(&(*pOut),"[%qd] =>",pEntry->xKey.iKey);` |
|        17 | 8433 | `		}else{` |
|        48 | 8434 | `			SyBlobFormat(&(*pOut),"[%.*s] =>",` |
|        15 | 8435 | `				SyBlobLength(&pEntry->xKey.sKey),SyBlobData(&pEntry->xKey.sKey));` |
|         - | 8436 | `		}` |
|         - | 8437 | `#ifdef __WINNT__` |
|         3 | 8438 | `		SyBlobAppend(&(*pOut),"\r\n",sizeof("\r\n")-1);` |
|         - | 8439 | `#else` |
|        62 | 8440 | `		SyBlobAppend(&(*pOut),"\n",sizeof(char));` |
|         - | 8441 | `#endif` |
|         - | 8442 | `		/* Dump node value */` |
|        65 | 8443 | `		pObj = HashmapExtractNodeValue(pEntry);` |
|        65 | 8444 | `		isRef = 0;` |
|        65 | 8445 | `		if( pObj ){` |
|        65 | 8446 | `			if( pEntry->iFlags & HASHMAP_NODE_FOREIGN_OBJ ){` |
|         - | 8447 | `				/* Referenced object */` |
|       ! 0 | 8448 | `				isRef = 1;` |
|       ! 0 | 8449 | `			}` |
|        65 | 8450 | `			rc = PH7_MemObjDump(&(*pOut),pObj,ShowType,nTab+1,nDepth,isRef);` |
|        65 | 8451 | `			if( rc == SXERR_LIMIT ){` |
|       ! 0 | 8452 | `				break;` |
|         - | 8453 | `			}` |
|        31 | 8454 | `		}` |
|         - | 8455 | `		/* Point to the next entry */` |
|        65 | 8456 | `		n++;` |
|        65 | 8457 | `		pEntry = pEntry->pPrev; /* Reverse link */` |
|         3 | 8458 | `	}` |
|        29 | 8459 | `	return rc;` |
|         3 | 8460 | `}` |
|        22 | 8461 | `PH7_PRIVATE sxi32 PH7_HashmapDump(SyBlob *pOut,ph7_hashmap *pMap,int ShowType,int nTab,int nDepth)` |
|         2 | 8462 | `{` |
|         - | 8463 | `	sxi32 rc;` |
|         - | 8464 | `	int i;` |
|        24 | 8465 | `	if( nDepth > 31 ){` |
|         - | 8466 | `		static const char zInfinite[] = "Nesting limit reached: Infinite recursion?";` |
|         - | 8467 | `		/* Nesting limit reached */` |
|       ! 0 | 8468 | `		SyBlobAppend(&(*pOut),zInfinite,sizeof(zInfinite)-1);` |
|       ! 0 | 8469 | `		if( ShowType ){` |
|       ! 0 | 8470 | `			SyBlobAppend(&(*pOut),")",sizeof(char));` |
|       ! 0 | 8471 | `		}` |
|       ! 0 | 8472 | `		return SXERR_LIMIT;` |
|         - | 8473 | `	}` |
|        24 | 8474 | `	if( !ShowType ){` |
|        11 | 8475 | `		SyBlobAppend(&(*pOut),"Array(",sizeof("Array(")-1);` |
|         5 | 8476 | `	}` |
|         - | 8477 | `	/* Total entries */` |
|        24 | 8478 | `	SyBlobFormat(&(*pOut),"%u) {",pMap->nEntry);` |
|         - | 8479 | `#ifdef __WINNT__` |
|         2 | 8480 | `	SyBlobAppend(&(*pOut),"\r\n",sizeof("\r\n")-1);` |
|         - | 8481 | `#else` |
|        22 | 8482 | `	SyBlobAppend(&(*pOut),"\n",sizeof(char));` |
|         - | 8483 | `#endif` |
|        24 | 8484 | `	rc = PH7_HashmapDumpEntries(&(*pOut),pMap,ShowType,nTab,nDepth);` |
|        46 | 8485 | `	for( i = 0 ; i < nTab ; i++ ){` |
|        24 | 8486 | `		SyBlobAppend(&(*pOut)," ",sizeof(char));` |
|        13 | 8487 | `	}` |
|        24 | 8488 | `	SyBlobAppend(&(*pOut),"}",sizeof(char));` |
|        24 | 8489 | `	return rc;` |
|        13 | 8490 | `}` |
|         - | 8491 | `/*` |
|         - | 8492 | ` * Iterate throw hashmap entries and invoke the given callback [i.e: xWalk()] for each` |
|         - | 8493 | ` * retrieved entry.` |
|         - | 8494 | ` * Note that argument are passed to the callback by copy. That is,any modification to` |
|         - | 8495 | ` * the entry value in the callback body will not alter the real value.` |
|         - | 8496 | ` * If the callback wishes to abort processing [i.e: it's invocation] it must return` |
|         - | 8497 | ` * a value different from PH7_OK.` |
|         - | 8498 | ` * Refer to [ph7_array_walk()] for more information.` |
|         - | 8499 | ` */` |
|     32906 | 8500 | `PH7_PRIVATE sxi32 PH7_HashmapWalk(` |
|         - | 8501 | `	ph7_hashmap *pMap, /* Target hashmap */` |
|         - | 8502 | `	int (*xWalk)(ph7_value *,ph7_value *,void *), /* Walker callback */` |
|         - | 8503 | `	void *pUserData /* Last argument to xWalk() */` |
|         - | 8504 | `	)` |
|         5 | 8505 | `{` |
|         - | 8506 | `	ph7_hashmap_node *pEntry;` |
|         - | 8507 | `	ph7_value sKey,sValue;` |
|         - | 8508 | `	sxi32 rc;` |
|         - | 8509 | `	sxu32 n;` |
|         - | 8510 | `	/* Initialize walker parameter */` |
|     32911 | 8511 | `	rc = SXRET_OK;` |
|     32911 | 8512 | `	PH7_MemObjInit(pMap->pVm,&sKey);` |
|     32911 | 8513 | `	PH7_MemObjInit(pMap->pVm,&sValue);` |
|     32911 | 8514 | `	n = pMap->nEntry;` |
|     32911 | 8515 | `	pEntry = pMap->pFirst;` |
|         - | 8516 | `	/* Start the iteration process */` |
|     84790 | 8517 | `	for(;;){` |
|    169585 | 8518 | `		if( n < 1 ){` |
|     32911 | 8519 | `			break;` |
|         - | 8520 | `		}` |
|         - | 8521 | `		/* Extract a copy of the key and a copy the current value */` |
|    136679 | 8522 | `		PH7_HashmapExtractNodeKey(pEntry,&sKey);` |
|    136679 | 8523 | `		PH7_HashmapExtractNodeValue(pEntry,&sValue,FALSE);` |
|         - | 8524 | `		/* Invoke the user callback */` |
|    136679 | 8525 | `		rc = xWalk(&sKey,&sValue,pUserData);` |
|         - | 8526 | `		/* Release the copy of the key and the value */` |
|    136679 | 8527 | `		PH7_MemObjRelease(&sKey);` |
|    136679 | 8528 | `		PH7_MemObjRelease(&sValue);` |
|    136679 | 8529 | `		if( rc != PH7_OK ){` |
|         - | 8530 | `			/* Callback request an operation abort */` |
|       ! 0 | 8531 | `			return SXERR_ABORT;` |
|         - | 8532 | `		}` |
|         - | 8533 | `		/* Point to the next entry */` |
|    136679 | 8534 | `		pEntry = pEntry->pPrev; /* Reverse link */` |
|    136679 | 8535 | `		n--;` |
|         5 | 8536 | `	}` |
|         - | 8537 | `	/* All done */` |
|     32911 | 8538 | `	return SXRET_OK;` |
|     16458 | 8539 | `}` |
|         - | 8540 |  |
