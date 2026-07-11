# src/ph7/hashmap.c

<style>code, pre { background: none !important; white-space: pre !important; width: 100% !important; display: inline-block !important; } td { border: none !important; margin-top: 0 !important; margin-bottom: 0 !important; padding-top: 0 !important; padding-bottom: 0 !important; }</style>

Coverage: 3689/4203 lines (87.77%)

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
| 3134722 |   23 | `static sxu32 IntHash(sxi64 iKey)` |
|       5 |   24 | `{` |
| 3134727 |   25 | `	sxu64 uKey = (sxu64)iKey; /* unsigned mixing: shifting a negative key is UB */` |
| 3134727 |   26 | `	return (sxu32)(uKey ^ (uKey << 8) ^ (uKey >> 8));` |
|       5 |   27 | `}` |
|       - |   28 | `/*` |
|       - |   29 | ` * Default hash function for string/BLOB keys.` |
|       - |   30 | ` */` |
|  405494 |   31 | `static sxu32 BinHash(const void *pSrc,sxu32 nLen)` |
|       5 |   32 | `{` |
|  405499 |   33 | `	register unsigned char *zIn = (unsigned char *)pSrc;` |
|       - |   34 | `	unsigned char *zEnd;` |
|  405499 |   35 | `	sxu32 nH = 5381;` |
|  405499 |   36 | `	zEnd = &zIn[nLen];` |
|  474064 |   37 | `	for(;;){` |
|  948133 |   38 | `		if( zIn >= zEnd ){ break; } nH = nH * 33 + zIn[0] ; zIn++;` |
|  820765 |   39 | `		if( zIn >= zEnd ){ break; } nH = nH * 33 + zIn[0] ; zIn++;` |
|  742951 |   40 | `		if( zIn >= zEnd ){ break; } nH = nH * 33 + zIn[0] ; zIn++;` |
|  634847 |   41 | `		if( zIn >= zEnd ){ break; } nH = nH * 33 + zIn[0] ; zIn++;` |
|       5 |   42 | `	}` |
|  405499 |   43 | `	return nH;` |
|       5 |   44 | `}` |
|       - |   45 | `/*` |
|       - |   46 | ` * Return the total number of entries in a given hashmap.` |
|       - |   47 | ` * If bRecursive is set to TRUE then recurse on hashmap entries.` |
|       - |   48 | ` * Self-referential arrays are detected via the HASHMAP_COUNTING flag;` |
|       - |   49 | ` * when a cycle is found the nested array is skipped and *pCycleDetected` |
|       - |   50 | ` * is set to TRUE so the caller can emit a warning.` |
|       - |   51 | ` */` |
|     946 |   52 | `static sxi64 HashmapCount(ph7_hashmap *pMap,int bRecursive,int *pCycleDetected)` |
|       5 |   53 | `{` |
|     951 |   54 | `	sxi64 iCount = 0;` |
|     951 |   55 | `	if( !bRecursive ){` |
|     777 |   56 | `		iCount = pMap->nEntry;` |
|     391 |   57 | `	}else{` |
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
|     951 |   92 | `	return iCount;` |
|       5 |   93 | `}` |
|       - |   94 | `/*` |
|       - |   95 | ` * Allocate a new hashmap node with a 64-bit integer key.` |
|       - |   96 | ` * If something goes wrong [i.e: out of memory],this function return NULL.` |
|       - |   97 | ` * Otherwise a fresh [ph7_hashmap_node] instance is returned.` |
|       - |   98 | ` */` |
| 3073452 |   99 | `static ph7_hashmap_node * HashmapNewIntNode(ph7_hashmap *pMap,sxi64 iKey,sxu32 nHash,sxu32 nValIdx)` |
|       5 |  100 | `{` |
|       - |  101 | `	ph7_hashmap_node *pNode;` |
|       - |  102 | `	/* Allocate a new node */` |
| 3073457 |  103 | `	pNode = (ph7_hashmap_node *)SyMemBackendPoolAlloc(&pMap->pVm->sAllocator,sizeof(ph7_hashmap_node));` |
| 3073457 |  104 | `	if( pNode == 0 ){` |
|     ! 0 |  105 | `		return 0;` |
|       - |  106 | `	}` |
|       - |  107 | `	/* Zero the stucture */` |
| 3073457 |  108 | `	SyZero(pNode,sizeof(ph7_hashmap_node));` |
|       - |  109 | `	/* Fill in the structure */` |
| 3073457 |  110 | `	pNode->pMap  = &(*pMap);` |
| 3073457 |  111 | `	pNode->iType = HASHMAP_INT_NODE;` |
| 3073457 |  112 | `	pNode->nHash = nHash;` |
| 3073457 |  113 | `	pNode->xKey.iKey = iKey;` |
| 3073457 |  114 | `	pNode->nValIdx  = nValIdx;` |
| 3073457 |  115 | `	return pNode;` |
| 1536731 |  116 | `}` |
|       - |  117 | `/*` |
|       - |  118 | ` * Allocate a new hashmap node with a BLOB key.` |
|       - |  119 | ` * If something goes wrong [i.e: out of memory],this function return NULL.` |
|       - |  120 | ` * Otherwise a fresh [ph7_hashmap_node] instance is returned.` |
|       - |  121 | ` */` |
|  152566 |  122 | `static ph7_hashmap_node * HashmapNewBlobNode(ph7_hashmap *pMap,const void *pKey,sxu32 nKeyLen,sxu32 nHash,sxu32 nValIdx)` |
|       5 |  123 | `{` |
|       - |  124 | `	ph7_hashmap_node *pNode;` |
|       - |  125 | `	/* Allocate a new node */` |
|  152571 |  126 | `	pNode = (ph7_hashmap_node *)SyMemBackendPoolAlloc(&pMap->pVm->sAllocator,sizeof(ph7_hashmap_node));` |
|  152571 |  127 | `	if( pNode == 0 ){` |
|     ! 0 |  128 | `		return 0;` |
|       - |  129 | `	}` |
|       - |  130 | `	/* Zero the stucture */` |
|  152571 |  131 | `	SyZero(pNode,sizeof(ph7_hashmap_node));` |
|       - |  132 | `	/* Fill in the structure */` |
|  152571 |  133 | `	pNode->pMap  = &(*pMap);` |
|  152571 |  134 | `	pNode->iType = HASHMAP_BLOB_NODE;` |
|  152571 |  135 | `	pNode->nHash = nHash;` |
|  152571 |  136 | `	SyBlobInit(&pNode->xKey.sKey,&pMap->pVm->sAllocator);` |
|  152571 |  137 | `	SyBlobAppend(&pNode->xKey.sKey,pKey,nKeyLen);` |
|  152571 |  138 | `	pNode->nValIdx = nValIdx;` |
|  152571 |  139 | `	return pNode;` |
|   76288 |  140 | `}` |
|       - |  141 | `/*` |
|       - |  142 | ` * link a hashmap node to the given bucket index (last argument to this function).` |
|       - |  143 | ` */` |
| 3226018 |  144 | `static void HashmapNodeLink(ph7_hashmap *pMap,ph7_hashmap_node *pNode,sxu32 nBucketIdx)` |
|       5 |  145 | `{` |
|       - |  146 | `	/* Link */` |
| 3226023 |  147 | `	if( pMap->apBucket[nBucketIdx] != 0 ){` |
| 2850645 |  148 | `		pNode->pNextCollide = pMap->apBucket[nBucketIdx];` |
| 2850645 |  149 | `		pMap->apBucket[nBucketIdx]->pPrevCollide = pNode;` |
| 1425320 |  150 | `	}` |
| 3226023 |  151 | `	pMap->apBucket[nBucketIdx] = pNode;` |
|       - |  152 | `	/* Link to the map list */` |
| 3226023 |  153 | `	if( pMap->pFirst == 0 ){` |
|   74121 |  154 | `		pMap->pFirst = pMap->pLast = pNode;` |
|       - |  155 | `		/* Point to the first inserted node */` |
|   74121 |  156 | `		pMap->pCur = pNode;` |
|   37063 |  157 | `	}else{` |
| 3151907 |  158 | `		MACRO_LD_PUSH(pMap->pLast,pNode);` |
|       - |  159 | `	}` |
| 3226023 |  160 | `	++pMap->nEntry;` |
| 3226023 |  161 | `}` |
|       - |  162 | `/*` |
|       - |  163 | ` * Unlink a node from the hashmap.` |
|       - |  164 | ` * If the node count reaches zero then release the whole hash-bucket.` |
|       - |  165 | ` */` |
|    7284 |  166 | `PH7_PRIVATE void PH7_HashmapUnlinkNode(ph7_hashmap_node *pNode,int bRestore)` |
|       5 |  167 | `{` |
|    7289 |  168 | `	ph7_hashmap *pMap = pNode->pMap;` |
|    7289 |  169 | `	ph7_vm *pVm = pMap->pVm;` |
|       - |  170 | `	/* Unlink from the corresponding bucket */` |
|    7289 |  171 | `	if( pNode->pPrevCollide == 0 ){` |
|    6847 |  172 | `		pMap->apBucket[pNode->nHash & (pMap->nSize - 1)] = pNode->pNextCollide;` |
|    3426 |  173 | `	}else{` |
|     444 |  174 | `		pNode->pPrevCollide->pNextCollide = pNode->pNextCollide;` |
|       - |  175 | `	}` |
|    7289 |  176 | `	if( pNode->pNextCollide ){` |
|    5515 |  177 | `		pNode->pNextCollide->pPrevCollide = pNode->pPrevCollide;` |
|    2756 |  178 | `	}` |
|    7289 |  179 | `	if( pMap->pFirst == pNode ){` |
|     131 |  180 | `		pMap->pFirst = pNode->pPrev;` |
|      63 |  181 | `	}` |
|    7289 |  182 | `	if( pMap->pCur == pNode ){` |
|       - |  183 | `		/* Advance the node cursor */` |
|     133 |  184 | `		pMap->pCur = pMap->pCur->pPrev; /* Reverse link */` |
|      64 |  185 | `	}` |
|       - |  186 | `	/* Unlink from the map list */` |
|    7289 |  187 | `	MACRO_LD_REMOVE(pMap->pLast,pNode);` |
|    7289 |  188 | `	if( bRestore ){` |
|       - |  189 | `		/* Remove the ph7_value associated with this node from the reference table */` |
|     107 |  190 | `		PH7_VmRefObjRemove(pVm,pNode->nValIdx,0,pNode);` |
|       - |  191 | `		/* Restore to the freelist */` |
|     107 |  192 | `		if( (pNode->iFlags & HASHMAP_NODE_FOREIGN_OBJ) == 0 ){` |
|     107 |  193 | `			PH7_VmUnsetMemObj(pVm,pNode->nValIdx,FALSE);` |
|      51 |  194 | `		}` |
|      51 |  195 | `	}` |
|    7289 |  196 | `	if( pNode->iType == HASHMAP_BLOB_NODE ){` |
|    7154 |  197 | `		SyBlobRelease(&pNode->xKey.sKey);` |
|    3575 |  198 | `	}` |
|    7289 |  199 | `	SyMemBackendPoolFree(&pVm->sAllocator,pNode);` |
|    7289 |  200 | `	pMap->nEntry--;` |
|    7289 |  201 | `	if( pMap->nEntry < 1 && pMap != pVm->pGlobal ){` |
|       - |  202 | `		/* Free the hash-bucket */` |
|      75 |  203 | `		SyMemBackendFree(&pVm->sAllocator,pMap->apBucket);` |
|      75 |  204 | `		pMap->apBucket = 0;` |
|      75 |  205 | `		pMap->nSize = 0;` |
|      75 |  206 | `		pMap->pFirst = pMap->pLast = pMap->pCur = 0;` |
|      35 |  207 | `	}` |
|    7289 |  208 | `}` |
|       - |  209 | `#define HASHMAP_FILL_FACTOR 3` |
|       - |  210 | `/*` |
|       - |  211 | ` * Grow the hash-table and rehash all entries.` |
|       - |  212 | ` */` |
| 3226018 |  213 | `static sxi32 HashmapGrowBucket(ph7_hashmap *pMap)` |
|       5 |  214 | `{` |
| 3226023 |  215 | `	if( pMap->nEntry >= pMap->nSize * HASHMAP_FILL_FACTOR ){` |
|   78797 |  216 | `		ph7_hashmap_node **apOld = pMap->apBucket;` |
|       - |  217 | `		ph7_hashmap_node *pEntry,**apNew;` |
|   78797 |  218 | `		sxu32 nNew = pMap->nSize << 1;` |
|       - |  219 | `		sxu32 nBucket;` |
|       - |  220 | `		sxu32 n;` |
|   78797 |  221 | `		if( nNew < 1 ){` |
|   74121 |  222 | `			nNew = 16;` |
|   37058 |  223 | `		}` |
|       - |  224 | `		/* Allocate a new bucket */` |
|   78797 |  225 | `		apNew = (ph7_hashmap_node **)SyMemBackendAlloc(&pMap->pVm->sAllocator,nNew * sizeof(ph7_hashmap_node *));` |
|   78797 |  226 | `		if( apNew == 0 ){` |
|     ! 0 |  227 | `			if( pMap->nSize < 1 ){` |
|     ! 0 |  228 | `				return SXERR_MEM; /* Fatal */` |
|       - |  229 | `			}` |
|       - |  230 | `			/* Not so fatal here,simply a performance hit */` |
|     ! 0 |  231 | `			return SXRET_OK;` |
|       - |  232 | `		}` |
|       - |  233 | `		/* Zero the table */` |
|   78797 |  234 | `		SyZero((void *)apNew,nNew * sizeof(ph7_hashmap_node *));` |
|       - |  235 | `		/* Reflect the change */` |
|   78797 |  236 | `		pMap->apBucket = apNew;` |
|   78797 |  237 | `		pMap->nSize = nNew;` |
|   78797 |  238 | `		if( apOld == 0 ){` |
|       - |  239 | `			/* First allocated table [i.e: no entry],return immediately */` |
|   74121 |  240 | `			return SXRET_OK;` |
|       - |  241 | `		}` |
|       - |  242 | `		/* Rehash old entries */` |
|    4681 |  243 | `		pEntry = pMap->pFirst;` |
|    4681 |  244 | `		n = 0;` |
| 2077378 |  245 | `		for( ;; ){` |
| 4154761 |  246 | `			if( n >= pMap->nEntry ){` |
|    4681 |  247 | `				break;` |
|       - |  248 | `			}` |
|       - |  249 | `			/* Clear the old collision link */` |
| 4150085 |  250 | `			pEntry->pNextCollide = pEntry->pPrevCollide = 0;` |
|       - |  251 | `			/* Link to the new bucket */` |
| 4150085 |  252 | `			nBucket = pEntry->nHash & (nNew - 1);` |
| 4150085 |  253 | `			if( pMap->apBucket[nBucket] != 0 ){` |
| 3561983 |  254 | `				pEntry->pNextCollide = pMap->apBucket[nBucket];` |
| 3561983 |  255 | `				pMap->apBucket[nBucket]->pPrevCollide = pEntry;` |
| 1780989 |  256 | `			}` |
| 4150085 |  257 | `			pMap->apBucket[nBucket] = pEntry;` |
|       - |  258 | `			/* Point to the next entry */` |
| 4150085 |  259 | `			pEntry = pEntry->pPrev; /* Reverse link */` |
| 4150085 |  260 | `			n++;` |
|       5 |  261 | `		}` |
|       - |  262 | `		/* Free the old table */` |
|    4681 |  263 | `		SyMemBackendFree(&pMap->pVm->sAllocator,(void *)apOld);` |
|    2338 |  264 | `	}` |
| 3151907 |  265 | `	return SXRET_OK;` |
| 1613014 |  266 | `}` |
|       - |  267 | `/*` |
|       - |  268 | ` * Insert a 64-bit integer key and it's associated value (if any) in the given` |
|       - |  269 | ` * hashmap.` |
|       - |  270 | ` */` |
| 3073452 |  271 | `static sxi32 HashmapInsertIntKey(ph7_hashmap *pMap,sxi64 iKey,ph7_value *pValue,sxu32 nRefIdx,int isForeign)` |
|       5 |  272 | `{` |
|       - |  273 | `	ph7_hashmap_node *pNode;` |
|       - |  274 | `	sxu32 nIdx;` |
|       - |  275 | `	sxu32 nHash;` |
|       - |  276 | `	sxi32 rc;` |
| 3073457 |  277 | `	if( !isForeign ){` |
|       - |  278 | `		ph7_value *pObj;` |
|       - |  279 | `		ph7_value sSafeVal;` |
|       - |  280 | `		/* Snapshot the source BEFORE reserving: PH7_ReserveMemObj can grow (move)` |
|       - |  281 | `		 * pVm->aMemObj, which would dangle pValue when it points into the pool` |
|       - |  282 | `		 * (e.g. get_defined_vars/func_get_args/get_class_vars/get_object_vars pass` |
|       - |  283 | `		 * a pool slot). A shallow copy is a safe PH7_MemObjStore source — the` |
|       - |  284 | `		 * referent and the heap-resident blob data survive the move; only the` |
|       - |  285 | `		 * ph7_value struct relocates (same sSafeVal idiom used by PH7_HashmapDup). */` |
| 3073421 |  286 | `		if( pValue ){` |
| 3073419 |  287 | `			sSafeVal = *pValue;` |
| 3073419 |  288 | `			pValue = &sSafeVal;` |
| 1536707 |  289 | `		}` |
|       - |  290 | `		/* Reserve a ph7_value for the value */` |
| 3073421 |  291 | `		pObj = PH7_ReserveMemObj(pMap->pVm);` |
| 3073421 |  292 | `		if( pObj == 0 ){` |
|     ! 0 |  293 | `			return SXERR_MEM;` |
|       - |  294 | `		}` |
| 3073421 |  295 | `		if( pValue ){` |
|       - |  296 | `			/* Duplicate the value */` |
| 3073419 |  297 | `			PH7_MemObjStore(pValue,pObj);` |
| 1536707 |  298 | `		}` |
| 3073421 |  299 | `		nIdx = pObj->nIdx;` |
| 1536713 |  300 | `	}else{` |
|      37 |  301 | `		nIdx = nRefIdx;` |
|       - |  302 | `	}` |
|       - |  303 | `	/* Hash the key */` |
| 3073457 |  304 | `	nHash = pMap->xIntHash(iKey);` |
|       - |  305 | `	/* Allocate a new int node */` |
| 3073457 |  306 | `	pNode = HashmapNewIntNode(&(*pMap),iKey,nHash,nIdx);` |
| 3073457 |  307 | `	if( pNode == 0 ){` |
|     ! 0 |  308 | `		return SXERR_MEM;` |
|       - |  309 | `	}` |
| 3073457 |  310 | `	if( isForeign ){` |
|       - |  311 | `		/* Mark as a foregin entry */` |
|      37 |  312 | `		pNode->iFlags \|= HASHMAP_NODE_FOREIGN_OBJ;` |
|      18 |  313 | `	}` |
|       - |  314 | `	/* Make sure the bucket is big enough to hold the new entry */` |
| 3073457 |  315 | `	rc = HashmapGrowBucket(&(*pMap));` |
| 3073457 |  316 | `	if( rc != SXRET_OK ){` |
|     ! 0 |  317 | `		SyMemBackendPoolFree(&pMap->pVm->sAllocator,pNode);` |
|     ! 0 |  318 | `		return rc;` |
|       - |  319 | `	}` |
|       - |  320 | `	/* Perform the insertion */` |
| 3073457 |  321 | `	HashmapNodeLink(&(*pMap),pNode,nHash & (pMap->nSize - 1));` |
|       - |  322 | `	/* Install in the reference table */` |
| 3073457 |  323 | `	PH7_VmRefObjInstall(pMap->pVm,nIdx,0,pNode,0);` |
|       - |  324 | `	/* All done */` |
| 3073457 |  325 | `	return SXRET_OK;` |
| 1536731 |  326 | `}` |
|       - |  327 | `/*` |
|       - |  328 | ` * Insert a BLOB key and it's associated value (if any) in the given` |
|       - |  329 | ` * hashmap.` |
|       - |  330 | ` */` |
|  152566 |  331 | `static sxi32 HashmapInsertBlobKey(ph7_hashmap *pMap,const void *pKey,sxu32 nKeyLen,ph7_value *pValue,sxu32 nRefIdx,int isForeign)` |
|       5 |  332 | `{` |
|       - |  333 | `	ph7_hashmap_node *pNode;` |
|       - |  334 | `	sxu32 nHash;` |
|       - |  335 | `	sxu32 nIdx;` |
|       - |  336 | `	sxi32 rc;` |
|  152571 |  337 | `	if( !isForeign ){` |
|       - |  338 | `		ph7_value *pObj;` |
|       - |  339 | `		ph7_value sSafeVal;` |
|       - |  340 | `		/* Snapshot the source BEFORE reserving: PH7_ReserveMemObj can grow (move)` |
|       - |  341 | `		 * pVm->aMemObj, which would dangle pValue when it points into the pool` |
|       - |  342 | `		 * (e.g. get_defined_vars/func_get_args/get_class_vars/get_object_vars pass` |
|       - |  343 | `		 * a pool slot). A shallow copy is a safe PH7_MemObjStore source — the` |
|       - |  344 | `		 * referent and the heap-resident blob data survive the move; only the` |
|       - |  345 | `		 * ph7_value struct relocates (same sSafeVal idiom used by PH7_HashmapDup). */` |
|  106637 |  346 | `		if( pValue ){` |
|  106347 |  347 | `			sSafeVal = *pValue;` |
|  106347 |  348 | `			pValue = &sSafeVal;` |
|   53171 |  349 | `		}` |
|       - |  350 | `		/* Reserve a ph7_value for the value */` |
|  106637 |  351 | `		pObj = PH7_ReserveMemObj(pMap->pVm);` |
|  106637 |  352 | `		if( pObj == 0 ){` |
|     ! 0 |  353 | `			return SXERR_MEM;` |
|       - |  354 | `		}` |
|  106637 |  355 | `		if( pValue ){` |
|       - |  356 | `			/* Duplicate the value */` |
|  106347 |  357 | `			PH7_MemObjStore(pValue,pObj);` |
|   53171 |  358 | `		}` |
|  106637 |  359 | `		nIdx = pObj->nIdx;` |
|   53321 |  360 | `	}else{` |
|   45939 |  361 | `		nIdx = nRefIdx;` |
|       - |  362 | `	}` |
|       - |  363 | `	/* Hash the key */` |
|  152571 |  364 | `	nHash = pMap->xBlobHash(pKey,nKeyLen);` |
|       - |  365 | `	/* Allocate a new blob node */` |
|  152571 |  366 | `	pNode = HashmapNewBlobNode(&(*pMap),pKey,nKeyLen,nHash,nIdx);` |
|  152571 |  367 | `	if( pNode == 0 ){` |
|     ! 0 |  368 | `		return SXERR_MEM;` |
|       - |  369 | `	}` |
|  152571 |  370 | `	if( isForeign ){` |
|       - |  371 | `		/* Mark as a foregin entry */` |
|   45939 |  372 | `		pNode->iFlags \|= HASHMAP_NODE_FOREIGN_OBJ;` |
|   22967 |  373 | `	}` |
|       - |  374 | `	/* Make sure the bucket is big enough to hold the new entry */` |
|  152571 |  375 | `	rc = HashmapGrowBucket(&(*pMap));` |
|  152571 |  376 | `	if( rc != SXRET_OK ){` |
|     ! 0 |  377 | `		SyMemBackendPoolFree(&pMap->pVm->sAllocator,pNode);` |
|     ! 0 |  378 | `		return rc;` |
|       - |  379 | `	}` |
|       - |  380 | `	/* Perform the insertion */` |
|  152571 |  381 | `	HashmapNodeLink(&(*pMap),pNode,nHash & (pMap->nSize - 1));` |
|       - |  382 | `	/* Install in the reference table */` |
|  152571 |  383 | `	PH7_VmRefObjInstall(pMap->pVm,nIdx,0,pNode,0);` |
|       - |  384 | `	/* All done */` |
|  152571 |  385 | `	return SXRET_OK;` |
|   76288 |  386 | `}` |
|       - |  387 | `/*` |
|       - |  388 | ` * Check if a given 64-bit integer key exists in the given hashmap.` |
|       - |  389 | ` * Write a pointer to the target node on success. Otherwise` |
|       - |  390 | ` * SXERR_NOTFOUND is returned on failure.` |
|       - |  391 | ` */` |
|   48656 |  392 | `static sxi32 HashmapLookupIntKey(` |
|       - |  393 | `	ph7_hashmap *pMap,         /* Target hashmap */` |
|       - |  394 | `	sxi64 iKey,                /* lookup key */` |
|       - |  395 | `	ph7_hashmap_node **ppNode  /* OUT: target node on success */` |
|       - |  396 | `	)` |
|       5 |  397 | `{` |
|       - |  398 | `	ph7_hashmap_node *pNode;` |
|       - |  399 | `	sxu32 nHash;` |
|   48661 |  400 | `	if( pMap->nEntry < 1 ){` |
|       - |  401 | `		/* Don't bother hashing,there is no entry anyway */` |
|     559 |  402 | `		return SXERR_NOTFOUND;` |
|       - |  403 | `	}` |
|       - |  404 | `	/* Hash the key first */` |
|   48107 |  405 | `	nHash = pMap->xIntHash(iKey);` |
|       - |  406 | `	/* Point to the appropriate bucket */` |
|   48107 |  407 | `	pNode = pMap->apBucket[nHash & (pMap->nSize - 1)];` |
|       - |  408 | `	/* Perform the lookup */` |
|  412423 |  409 | `	for(;;){` |
|  824851 |  410 | `		if( pNode == 0 ){` |
|   46317 |  411 | `			break;` |
|       - |  412 | `		}` |
|  778534 |  413 | `		if( pNode->iType == HASHMAP_INT_NODE` |
|  775518 |  414 | `			&& pNode->nHash == nHash` |
|  387151 |  415 | `			&& pNode->xKey.iKey == iKey ){` |
|       - |  416 | `				/* Node found */` |
|    1795 |  417 | `				if( ppNode ){` |
|    1777 |  418 | `					*ppNode = pNode;` |
|     886 |  419 | `				}` |
|    1795 |  420 | `				return SXRET_OK;` |
|       - |  421 | `		}` |
|       - |  422 | `		/* Follow the collision link */` |
|  776745 |  423 | `		pNode = pNode->pNextCollide;` |
|       1 |  424 | `	}` |
|       - |  425 | `	/* No such entry */` |
|   46317 |  426 | `	return SXERR_NOTFOUND;` |
|   24333 |  427 | `}` |
|       - |  428 | `/*` |
|       - |  429 | ` * Check if a given BLOB key exists in the given hashmap.` |
|       - |  430 | ` * Write a pointer to the target node on success. Otherwise` |
|       - |  431 | ` * SXERR_NOTFOUND is returned on failure.` |
|       - |  432 | ` */` |
|  277144 |  433 | `static sxi32 HashmapLookupBlobKey(` |
|       - |  434 | `	ph7_hashmap *pMap,          /* Target hashmap */` |
|       - |  435 | `	const void *pKey,           /* Lookup key */` |
|       - |  436 | `	sxu32 nKeyLen,              /* Key length in bytes */` |
|       - |  437 | `	ph7_hashmap_node **ppNode   /* OUT: target node on success */` |
|       - |  438 | `	)` |
|       5 |  439 | `{` |
|       - |  440 | `	ph7_hashmap_node *pNode;` |
|       - |  441 | `	sxu32 nHash;` |
|  277149 |  442 | `	if( pMap->nEntry < 1 ){` |
|       - |  443 | `		/* Don't bother hashing,there is no entry anyway */` |
|   24221 |  444 | `		return SXERR_NOTFOUND;` |
|       - |  445 | `	}` |
|       - |  446 | `	/* Hash the key first */` |
|  252933 |  447 | `	nHash = pMap->xBlobHash(pKey,nKeyLen);` |
|       - |  448 | `	/* Point to the appropriate bucket */` |
|  252933 |  449 | `	pNode = pMap->apBucket[nHash & (pMap->nSize - 1)];` |
|       - |  450 | `	/* Perform the lookup */` |
|  216018 |  451 | `	for(;;){` |
|  432041 |  452 | `		if( pNode == 0 ){` |
|  201197 |  453 | `			break;` |
|       - |  454 | `		}` |
|  230844 |  455 | `		if( pNode->iType == HASHMAP_BLOB_NODE` |
|  229341 |  456 | `			&& pNode->nHash == nHash` |
|  139833 |  457 | `			&& SyBlobLength(&pNode->xKey.sKey) == nKeyLen` |
|   51833 |  458 | `			&& SyMemcmp(SyBlobData(&pNode->xKey.sKey),pKey,nKeyLen) == 0 ){` |
|       - |  459 | `				/* Node found */` |
|   51741 |  460 | `				if( ppNode ){` |
|   51713 |  461 | `					*ppNode = pNode;` |
|   25854 |  462 | `				}` |
|   51741 |  463 | `				return SXRET_OK;` |
|       - |  464 | `		}` |
|       - |  465 | `		/* Follow the collision link */` |
|  179113 |  466 | `		pNode = pNode->pNextCollide;` |
|       5 |  467 | `	}` |
|       - |  468 | `	/* No such entry */` |
|  201197 |  469 | `	return SXERR_NOTFOUND;` |
|  138577 |  470 | `}` |
|       - |  471 | `/*` |
|       - |  472 | ` * Check if the given BLOB key looks like a decimal number.` |
|       - |  473 | ` * Retrurn TRUE on success.FALSE otherwise.` |
|       - |  474 | ` */` |
|  277268 |  475 | `static int HashmapIsIntKey(SyBlob *pKey)` |
|       5 |  476 | `{` |
|  277273 |  477 | `	const char *zIn  = (const char *)SyBlobData(pKey);` |
|  277273 |  478 | `	const char *zEnd = &zIn[SyBlobLength(pKey)];` |
|  277273 |  479 | `	if( (int)(zEnd-zIn) > 1 && zIn[0] == '0' ){` |
|       - |  480 | `		/* Octal not decimal number */` |
|       5 |  481 | `		return FALSE;` |
|       - |  482 | `	}` |
|  277269 |  483 | `	if( (zIn[0] == '-' \|\| zIn[0] == '+') && &zIn[1] < zEnd ){` |
|     ! 0 |  484 | `		zIn++;` |
|     ! 0 |  485 | `	}` |
|  138965 |  486 | `	for(;;){` |
|  277935 |  487 | `		if( zIn >= zEnd ){` |
|     233 |  488 | `			return TRUE;` |
|       - |  489 | `		}` |
|  277703 |  490 | `		if( (unsigned char)zIn[0] >= 0xc0 /* UTF-8 stream */  \|\| !SyisDigit(zIn[0]) ){` |
|  138521 |  491 | `			break;` |
|       - |  492 | `		}` |
|     667 |  493 | `		zIn++;` |
|       1 |  494 | `	}` |
|       - |  495 | `	/* Key does not look like a decimal number */` |
|  277037 |  496 | `	return FALSE;` |
|  138639 |  497 | `}` |
|       - |  498 | `/*` |
|       - |  499 | ` * Check if a given key exists in the given hashmap.` |
|       - |  500 | ` * Write a pointer to the target node on success.` |
|       - |  501 | ` * Otherwise SXERR_NOTFOUND is returned on failure.` |
|       - |  502 | ` */` |
|  126082 |  503 | `static sxi32 HashmapLookup(` |
|       - |  504 | `	ph7_hashmap *pMap,          /* Target hashmap */` |
|       - |  505 | `	ph7_value *pKey,            /* Lookup key */` |
|       - |  506 | `	ph7_hashmap_node **ppNode   /* OUT: target node on success */` |
|       - |  507 | `	)` |
|       5 |  508 | `{` |
|  126087 |  509 | `	ph7_hashmap_node *pNode = 0; /* cc -O6 warning */` |
|       - |  510 | `	sxi32 rc;` |
|  126087 |  511 | `	if( pKey->iFlags & (MEMOBJ_STRING\|MEMOBJ_HASHMAP\|MEMOBJ_OBJ\|MEMOBJ_RES) ){` |
|  124517 |  512 | `		if( (pKey->iFlags & MEMOBJ_STRING) == 0 ){` |
|       - |  513 | `			/* Force a string cast */` |
|     ! 0 |  514 | `			PH7_MemObjToString(&(*pKey));` |
|     ! 0 |  515 | `		}` |
|  124517 |  516 | `		if( SyBlobLength(&pKey->sBlob) > 0 && !HashmapIsIntKey(&pKey->sBlob) ){` |
|       - |  517 | `			/* Perform a blob lookup */` |
|  124501 |  518 | `			rc = HashmapLookupBlobKey(&(*pMap),SyBlobData(&pKey->sBlob),SyBlobLength(&pKey->sBlob),&pNode);` |
|  124501 |  519 | `			goto result;` |
|       - |  520 | `		}` |
|       8 |  521 | `	}` |
|       - |  522 | `	/* Perform an int lookup */` |
|    1591 |  523 | `	if((pKey->iFlags & MEMOBJ_INT) == 0 ){` |
|       - |  524 | `		/* Force an integer cast */` |
|      27 |  525 | `		PH7_MemObjToInteger(pKey);` |
|      13 |  526 | `	}` |
|       - |  527 | `	/* Perform an int lookup */` |
|    1591 |  528 | `	rc = HashmapLookupIntKey(&(*pMap),pKey->x.iVal,&pNode);` |
|   63041 |  529 | `result:` |
|  126087 |  530 | `	if( rc == SXRET_OK ){` |
|       - |  531 | `		/* Node found */` |
|   53175 |  532 | `		if( ppNode ){` |
|   53131 |  533 | `			*ppNode = pNode;` |
|   26563 |  534 | `		}` |
|   53175 |  535 | `		return SXRET_OK;` |
|       - |  536 | `	}` |
|       - |  537 | `	/* No such entry */` |
|   72917 |  538 | `	return SXERR_NOTFOUND;` |
|   63046 |  539 | `}` |
|       - |  540 | `/*` |
|       - |  541 | ` * Advance the auto-index after a successful insertion of int key iKey.` |
|       - |  542 | ` * Mirrors Zend's nNextFreeElement: saturates at PHP_INT_MAX (incrementing` |
|       - |  543 | ` * past it is signed overflow); the occupied-slot case errors at append time` |
|       - |  544 | ` * via HashmapAppendIndexBusy.` |
|       - |  545 | ` */` |
|   23538 |  546 | `static void HashmapAdvanceAutoIndex(ph7_hashmap *pMap,sxi64 iKey)` |
|       5 |  547 | `{` |
|   23543 |  548 | `	if( iKey >= pMap->iNextIdx ){` |
|   23299 |  549 | `		pMap->iNextIdx = iKey < SXI64_HIGH ? iKey + 1 : SXI64_HIGH;` |
|       - |  550 | `		/* Make sure the automatic index is not reserved */` |
|   23299 |  551 | `		while( pMap->iNextIdx < SXI64_HIGH && SXRET_OK == HashmapLookupIntKey(&(*pMap),pMap->iNextIdx,0) ){` |
|     ! 0 |  552 | `			pMap->iNextIdx++;` |
|     ! 0 |  553 | `		}` |
|   11647 |  554 | `	}` |
|   23543 |  555 | `}` |
|       - |  556 | `/*` |
|       - |  557 | `` * TRUE when an append (`$a[] = v`) cannot proceed because the saturated`` |
|       - |  558 | ` * auto-index slot (PHP_INT_MAX) is already occupied. Throws php's catchable` |
|       - |  559 | ` * Error and stores the rc the insert function must return (PH7_EXCEPTION,` |
|       - |  560 | ` * or PH7_ABORT when the Error class itself cannot be built).` |
|       - |  561 | ` */` |
| 3049582 |  562 | `static sxi32 HashmapAppendIndexBusy(ph7_hashmap *pMap,sxi32 *pRc)` |
|       5 |  563 | `{` |
| 3049587 |  564 | `	if( pMap->iNextIdx == SXI64_HIGH && SXRET_OK == HashmapLookupIntKey(&(*pMap),pMap->iNextIdx,0) ){` |
|       7 |  565 | `		*pRc = PH7_VmThrowArrayNextIndexError(pMap->pVm);` |
|       7 |  566 | `		return TRUE;` |
|       - |  567 | `	}` |
| 3049581 |  568 | `	return FALSE;` |
| 1524796 |  569 | `}` |
|       - |  570 | `/*` |
|       - |  571 | ` * Insert a given key and it's associated value (if any) in the given` |
|       - |  572 | ` * hashmap.` |
|       - |  573 | ` * If a node with the given key already exists in the database` |
|       - |  574 | ` * then this function overwrite the old value.` |
|       - |  575 | ` */` |
| 3179774 |  576 | `static sxi32 HashmapInsert(` |
|       - |  577 | `	ph7_hashmap *pMap, /* Target hashmap */` |
|       - |  578 | `	ph7_value *pKey,   /* Lookup key  */` |
|       - |  579 | `	ph7_value *pVal    /* Node value */` |
|       - |  580 | `	)` |
|       5 |  581 | `{` |
| 3179779 |  582 | `	ph7_hashmap_node *pNode = 0;` |
| 3179779 |  583 | `	sxi32 rc = SXRET_OK;` |
| 3179779 |  584 | `	if( pKey && pKey->iFlags & (MEMOBJ_STRING\|MEMOBJ_HASHMAP\|MEMOBJ_OBJ\|MEMOBJ_RES) ){` |
|  110299 |  585 | `		if( (pKey->iFlags & MEMOBJ_STRING) == 0 ){` |
|       - |  586 | `			/* Force a string cast */` |
|       3 |  587 | `			PH7_MemObjToString(&(*pKey));` |
|       1 |  588 | `		}` |
|  110299 |  589 | `		if( SyBlobLength(&pKey->sBlob) < 1 \|\| HashmapIsIntKey(&pKey->sBlob) ){` |
|    3699 |  590 | `			if(SyBlobLength(&pKey->sBlob) < 1){` |
|       - |  591 | `				/* Automatic index assign */` |
|    3477 |  592 | `				pKey = 0;` |
|    1736 |  593 | `			}` |
|    3699 |  594 | `			goto IntKey;` |
|       - |  595 | `		}` |
|  159905 |  596 | `		if( SXRET_OK == HashmapLookupBlobKey(&(*pMap),SyBlobData(&pKey->sBlob),` |
|   53300 |  597 | `			SyBlobLength(&pKey->sBlob),&pNode) ){` |
|       - |  598 | `				/* Overwrite the old value */` |
|       - |  599 | `				ph7_value *pElem;` |
|      81 |  600 | `				pElem = (ph7_value *)SySetAt(&pMap->pVm->aMemObj,pNode->nValIdx);` |
|      81 |  601 | `				if( pElem ){` |
|      81 |  602 | `					if( pVal ){` |
|      81 |  603 | `						PH7_MemObjStore(pVal,pElem);` |
|      42 |  604 | `					}else{` |
|       - |  605 | `						/* Nullify the entry */` |
|     ! 0 |  606 | `						PH7_MemObjToNull(pElem);` |
|       - |  607 | `					}` |
|      39 |  608 | `				}` |
|      81 |  609 | `				return SXRET_OK;` |
|       - |  610 | `		}` |
|  106527 |  611 | `		if( pMap == pMap->pVm->pGlobal ){` |
|       - |  612 | `			/* Forbidden */` |
|       3 |  613 | `			PH7_VmThrowError(pMap->pVm,0,PH7_CTX_NOTICE,"$GLOBALS is a read-only array,insertion is forbidden");` |
|       3 |  614 | `			return SXRET_OK;` |
|       - |  615 | `		}` |
|       - |  616 | `		/* Perform a blob-key insertion */` |
|  106525 |  617 | `		rc = HashmapInsertBlobKey(&(*pMap),SyBlobData(&pKey->sBlob),SyBlobLength(&pKey->sBlob),&(*pVal),0,FALSE);` |
|  106525 |  618 | `		return rc;` |
|       - |  619 | `	}` |
| 1534740 |  620 | `IntKey:` |
| 3073179 |  621 | `	if( pKey ){` |
|   23627 |  622 | `		if((pKey->iFlags & MEMOBJ_INT) == 0 ){` |
|       - |  623 | `			/* Force an integer cast */` |
|     251 |  624 | `			PH7_MemObjToInteger(pKey);` |
|     125 |  625 | `		}` |
|   23627 |  626 | `		if( SXRET_OK == HashmapLookupIntKey(&(*pMap),pKey->x.iVal,&pNode) ){` |
|       - |  627 | `			/* Overwrite the old value */` |
|       - |  628 | `			ph7_value *pElem;` |
|      87 |  629 | `			pElem = (ph7_value *)SySetAt(&pMap->pVm->aMemObj,pNode->nValIdx);` |
|      87 |  630 | `			if( pElem ){` |
|      87 |  631 | `				if( pVal ){` |
|      87 |  632 | `					PH7_MemObjStore(pVal,pElem);` |
|      44 |  633 | `				}else{` |
|       - |  634 | `					/* Nullify the entry */` |
|     ! 0 |  635 | `					PH7_MemObjToNull(pElem);` |
|       - |  636 | `				}` |
|      43 |  637 | `			}` |
|      87 |  638 | `			return SXRET_OK;` |
|       - |  639 | `		}` |
|   23541 |  640 | `		if( pMap == pMap->pVm->pGlobal ){` |
|       - |  641 | `			/* Forbidden */` |
|       3 |  642 | `			PH7_VmThrowError(pMap->pVm,0,PH7_CTX_NOTICE,"$GLOBALS is a read-only array,insertion is forbidden");` |
|       3 |  643 | `			return SXRET_OK;` |
|       - |  644 | `		}` |
|       - |  645 | `		/* Perform a 64-bit-int-key insertion */` |
|   23539 |  646 | `		rc = HashmapInsertIntKey(&(*pMap),pKey->x.iVal,&(*pVal),0,FALSE);` |
|   23539 |  647 | `		if( rc == SXRET_OK ){` |
|   23539 |  648 | `			HashmapAdvanceAutoIndex(&(*pMap),pKey->x.iVal);` |
|   11767 |  649 | `		}` |
|   11772 |  650 | `	}else{` |
| 3049557 |  651 | `		if( pMap == pMap->pVm->pGlobal ){` |
|       - |  652 | `			/* Forbidden */` |
|       3 |  653 | `			PH7_VmThrowError(pMap->pVm,0,PH7_CTX_NOTICE,"$GLOBALS is a read-only array,insertion is forbidden");` |
|       3 |  654 | `			return SXRET_OK;` |
|       - |  655 | `		}` |
| 3049555 |  656 | `		if( HashmapAppendIndexBusy(&(*pMap),&rc) ){` |
|       7 |  657 | `			return rc; /* PH7_EXCEPTION/PH7_ABORT: php's catchable Error was thrown */` |
|       - |  658 | `		}` |
|       - |  659 | `		/* Assign an automatic index */` |
| 3049549 |  660 | `		rc = HashmapInsertIntKey(&(*pMap),pMap->iNextIdx,&(*pVal),0,FALSE);` |
| 3049549 |  661 | `		if( rc == SXRET_OK && pMap->iNextIdx < SXI64_HIGH ){` |
| 3049547 |  662 | `			++pMap->iNextIdx;` |
| 1524771 |  663 | `		}` |
|       - |  664 | `	}` |
|       - |  665 | `	/* Insertion result */` |
| 3073083 |  666 | `	return rc;` |
| 1589892 |  667 | `}` |
|       - |  668 | `/*` |
|       - |  669 | ` * Insert a given key and it's associated value (foreign index) in the given` |
|       - |  670 | ` * hashmap.` |
|       - |  671 | ` * This is insertion by reference so be careful to mark the node` |
|       - |  672 | ` * with the HASHMAP_NODE_FOREIGN_OBJ flag being set.` |
|       - |  673 | ` * The insertion by reference is triggered when the following` |
|       - |  674 | ` * expression is encountered.` |
|       - |  675 | ` * $var = 10;` |
|       - |  676 | ` *  $a = array(&var);` |
|       - |  677 | ` * OR` |
|       - |  678 | ` *  $a[] =& $var;` |
|       - |  679 | ` * That is,$var is a foreign ph7_value and the $a array have no control` |
|       - |  680 | ` * over it's contents.` |
|       - |  681 | ` * Note that the node that hold the foreign ph7_value is automatically` |
|       - |  682 | ` * removed when the foreign ph7_value is unset.` |
|       - |  683 | ` * Example:` |
|       - |  684 | ` *  $var = 10;` |
|       - |  685 | ` *  $a[] =& $var;` |
|       - |  686 | ` *  echo count($a).PHP_EOL; //1` |
|       - |  687 | ` *  //Unset the foreign ph7_value now` |
|       - |  688 | ` *  unset($var);` |
|       - |  689 | ` *  echo count($a); //0` |
|       - |  690 | ` * Note that this is a PH7 eXtension.` |
|       - |  691 | ` * Refer to the official documentation for more information.` |
|       - |  692 | ` * If a node with the given key already exists in the database` |
|       - |  693 | ` * then this function overwrite the old value.` |
|       - |  694 | ` */` |
|   45976 |  695 | `static sxi32 HashmapInsertByRef(` |
|       - |  696 | `	ph7_hashmap *pMap,   /* Target hashmap */` |
|       - |  697 | `	ph7_value *pKey,     /* Lookup key */` |
|       - |  698 | `	sxu32 nRefIdx        /* Foreign ph7_value index */` |
|       - |  699 | `	)` |
|       5 |  700 | `{` |
|   45981 |  701 | `	ph7_hashmap_node *pNode = 0;` |
|   45981 |  702 | `	sxi32 rc = SXRET_OK;` |
|   45981 |  703 | `	if( pKey && pKey->iFlags & (MEMOBJ_STRING\|MEMOBJ_HASHMAP\|MEMOBJ_OBJ\|MEMOBJ_RES) ){` |
|   45945 |  704 | `		if( (pKey->iFlags & MEMOBJ_STRING) == 0 ){` |
|       - |  705 | `			/* Force a string cast */` |
|     ! 0 |  706 | `			PH7_MemObjToString(&(*pKey));` |
|     ! 0 |  707 | `		}` |
|   45945 |  708 | `		if( SyBlobLength(&pKey->sBlob) < 1 \|\| HashmapIsIntKey(&pKey->sBlob) ){` |
|     ! 0 |  709 | `			if(SyBlobLength(&pKey->sBlob) < 1){` |
|       - |  710 | `				/* Automatic index assign */` |
|     ! 0 |  711 | `				pKey = 0;` |
|     ! 0 |  712 | `			}` |
|     ! 0 |  713 | `			goto IntKey;` |
|       - |  714 | `		}` |
|   68915 |  715 | `		if( SXRET_OK == HashmapLookupBlobKey(&(*pMap),SyBlobData(&pKey->sBlob),` |
|   22970 |  716 | `			SyBlobLength(&pKey->sBlob),&pNode) ){` |
|       - |  717 | `				/* Overwrite */` |
|       7 |  718 | `				PH7_VmRefObjRemove(pMap->pVm,pNode->nValIdx,0,pNode);` |
|       7 |  719 | `				pNode->nValIdx = nRefIdx;` |
|       - |  720 | `				/* Install in the reference table */` |
|       7 |  721 | `				PH7_VmRefObjInstall(pMap->pVm,nRefIdx,0,pNode,0);` |
|       7 |  722 | `				return SXRET_OK;` |
|       - |  723 | `		}` |
|       - |  724 | `		/* Perform a blob-key insertion */` |
|   45939 |  725 | `		rc = HashmapInsertBlobKey(&(*pMap),SyBlobData(&pKey->sBlob),SyBlobLength(&pKey->sBlob),0,nRefIdx,TRUE);` |
|   45939 |  726 | `		return rc;` |
|       - |  727 | `	}` |
|      18 |  728 | `IntKey:` |
|      37 |  729 | `	if( pKey ){` |
|       5 |  730 | `		if((pKey->iFlags & MEMOBJ_INT) == 0 ){` |
|       - |  731 | `			/* Force an integer cast */` |
|     ! 0 |  732 | `			PH7_MemObjToInteger(pKey);` |
|     ! 0 |  733 | `		}` |
|       5 |  734 | `		if( SXRET_OK == HashmapLookupIntKey(&(*pMap),pKey->x.iVal,&pNode) ){` |
|       - |  735 | `			/* Overwrite */` |
|     ! 0 |  736 | `			PH7_VmRefObjRemove(pMap->pVm,pNode->nValIdx,0,pNode);` |
|     ! 0 |  737 | `			pNode->nValIdx = nRefIdx;` |
|       - |  738 | `			/* Install in the reference table */` |
|     ! 0 |  739 | `			PH7_VmRefObjInstall(pMap->pVm,nRefIdx,0,pNode,0);` |
|     ! 0 |  740 | `			return SXRET_OK;` |
|       - |  741 | `		}` |
|       - |  742 | `		/* Perform a 64-bit-int-key insertion */` |
|       5 |  743 | `		rc = HashmapInsertIntKey(&(*pMap),pKey->x.iVal,0,nRefIdx,TRUE);` |
|       5 |  744 | `		if( rc == SXRET_OK ){` |
|       5 |  745 | `			HashmapAdvanceAutoIndex(&(*pMap),pKey->x.iVal);` |
|       2 |  746 | `		}` |
|       3 |  747 | `	}else{` |
|      33 |  748 | `		if( HashmapAppendIndexBusy(&(*pMap),&rc) ){` |
|     ! 0 |  749 | `			return rc; /* PH7_EXCEPTION/PH7_ABORT: php's catchable Error was thrown */` |
|       - |  750 | `		}` |
|       - |  751 | `		/* Assign an automatic index */` |
|      33 |  752 | `		rc = HashmapInsertIntKey(&(*pMap),pMap->iNextIdx,0,nRefIdx,TRUE);` |
|      33 |  753 | `		if( rc == SXRET_OK && pMap->iNextIdx < SXI64_HIGH ){` |
|      33 |  754 | `			++pMap->iNextIdx;` |
|      16 |  755 | `		}` |
|       - |  756 | `	}` |
|       - |  757 | `	/* Insertion result */` |
|      37 |  758 | `	return rc;` |
|   22993 |  759 | `}` |
|       - |  760 | `/*` |
|       - |  761 | ` * Extract node value.` |
|       - |  762 | ` */` |
| 1337559 |  763 | `static ph7_value * HashmapExtractNodeValue(ph7_hashmap_node *pNode)` |
|       5 |  764 | `{` |
|       - |  765 | `	/* Point to the desired object */` |
|       - |  766 | `	ph7_value *pObj;` |
| 1337564 |  767 | `	pObj = (ph7_value *)SySetAt(&pNode->pMap->pVm->aMemObj,pNode->nValIdx);` |
| 1337564 |  768 | `	return pObj;` |
|       5 |  769 | `}` |
|       - |  770 | `/*` |
|       - |  771 | ` * Insert a node in the given hashmap.` |
|       - |  772 | ` * If a node with the given key already exists in the database` |
|       - |  773 | ` * then this function overwrite the old value.` |
|       - |  774 | ` */` |
|     446 |  775 | `static sxi32 HashmapInsertNode(ph7_hashmap *pMap,ph7_hashmap_node *pNode,int bPreserve)` |
|       5 |  776 | `{` |
|       - |  777 | `	ph7_value *pObj;` |
|       - |  778 | `	sxi32 rc;` |
|       - |  779 | `	/* Extract the node value */` |
|     451 |  780 | `	pObj = HashmapExtractNodeValue(&(*pNode));` |
|     451 |  781 | `	if( pObj == 0 ){` |
|     ! 0 |  782 | `		return SXERR_EMPTY;` |
|       - |  783 | `	}` |
|       - |  784 | `	/* Preserve key */` |
|     451 |  785 | `	if( pNode->iType == HASHMAP_INT_NODE){` |
|       - |  786 | `		/* Int64 key */` |
|     321 |  787 | `		if( !bPreserve ){` |
|       - |  788 | `			/* Assign an automatic index */` |
|     173 |  789 | `			rc = HashmapInsert(&(*pMap),0,pObj);` |
|      89 |  790 | `		}else{` |
|     149 |  791 | `			rc = HashmapInsertIntKey(&(*pMap),pNode->xKey.iKey,pObj,0,FALSE);` |
|       - |  792 | `		}` |
|     163 |  793 | `	}else{` |
|       - |  794 | `		/* Blob key */` |
|     131 |  795 | `		if( !bPreserve ){` |
|       - |  796 | `			/* treat it like an automatically-indexed element, drop the` |
|       - |  797 | `			 * original string key entirely */` |
|      35 |  798 | `			rc = HashmapInsert(&(*pMap),0,pObj);` |
|      18 |  799 | `		}else{` |
|     145 |  800 | `			rc = HashmapInsertBlobKey(&(*pMap),SyBlobData(&pNode->xKey.sKey),` |
|      48 |  801 | `				SyBlobLength(&pNode->xKey.sKey),pObj,0,FALSE);` |
|       - |  802 | `		}` |
|       - |  803 | `	}` |
|     451 |  804 | `	return rc;` |
|     228 |  805 | `}` |
|       - |  806 | `/*` |
|       - |  807 | ` * Compare two node values.` |
|       - |  808 | ` * Return 0 if the node values are equals, > 0 if pLeft is greater than pRight` |
|       - |  809 | ` * or < 0 if pRight is greater than pLeft.` |
|       - |  810 | ` * For a full description on ph7_values comparison,refer to the implementation` |
|       - |  811 | ` * of the [PH7_MemObjCmp()] function defined in memobj.c or the official` |
|       - |  812 | ` * documenation.` |
|       - |  813 | ` */` |
|   68822 |  814 | `static sxi32 HashmapNodeCmp(ph7_hashmap_node *pLeft,ph7_hashmap_node *pRight,int bStrict)` |
|       5 |  815 | `{` |
|       - |  816 | `	ph7_value sObj1,sObj2;` |
|       - |  817 | `	sxi32 rc;` |
|   68827 |  818 | `	if( pLeft == pRight ){` |
|       - |  819 | `		/*` |
|       - |  820 | `		 * Same node.Refer to the sort() implementation defined` |
|       - |  821 | `		 * below for more information on this sceanario.` |
|       - |  822 | `		 */` |
|     ! 0 |  823 | `		return 0;` |
|       - |  824 | `	}` |
|       - |  825 | `	/* Do the comparison */` |
|   68827 |  826 | `	PH7_MemObjInit(pLeft->pMap->pVm,&sObj1);` |
|   68827 |  827 | `	PH7_MemObjInit(pLeft->pMap->pVm,&sObj2);` |
|   68827 |  828 | `	PH7_HashmapExtractNodeValue(pLeft,&sObj1,FALSE);` |
|   68827 |  829 | `	PH7_HashmapExtractNodeValue(pRight,&sObj2,FALSE);` |
|   68827 |  830 | `	rc = PH7_MemObjCmp(&sObj1,&sObj2,bStrict,0);` |
|   68827 |  831 | `	PH7_MemObjRelease(&sObj1);` |
|   68827 |  832 | `	PH7_MemObjRelease(&sObj2);` |
|   68827 |  833 | `	return rc;` |
|   34430 |  834 | `}` |
|       - |  835 | `/*` |
|       - |  836 | ` * Rehash a node with a 64-bit integer key.` |
|       - |  837 | ` * Refer to [merge_sort(),array_shift()] implementations for more information.` |
|       - |  838 | ` */` |
|   13168 |  839 | `static void HashmapRehashIntNode(ph7_hashmap_node *pEntry)` |
|       5 |  840 | `{` |
|   13173 |  841 | `	ph7_hashmap *pMap = pEntry->pMap;` |
|       - |  842 | `	sxu32 nBucket;` |
|       - |  843 | `	/* Remove old collision links */` |
|   13173 |  844 | `	if( pEntry->pPrevCollide ){` |
|   10786 |  845 | `		pEntry->pPrevCollide->pNextCollide = pEntry->pNextCollide;` |
|    5402 |  846 | `	}else{` |
|    2392 |  847 | `		pMap->apBucket[pEntry->nHash & (pMap->nSize - 1)] = pEntry->pNextCollide;` |
|       - |  848 | `	}` |
|   13173 |  849 | `	if( pEntry->pNextCollide ){` |
|    1074 |  850 | `		pEntry->pNextCollide->pPrevCollide = pEntry->pPrevCollide;` |
|     547 |  851 | `	}` |
|   13173 |  852 | `	pEntry->pNextCollide = pEntry->pPrevCollide = 0;` |
|       - |  853 | `	/* Compute the new hash */` |
|   13173 |  854 | `	pEntry->nHash = pMap->xIntHash(pMap->iNextIdx);` |
|   13173 |  855 | `	pEntry->xKey.iKey = pMap->iNextIdx;` |
|   13173 |  856 | `	nBucket = pEntry->nHash & (pMap->nSize - 1);` |
|       - |  857 | `	/* Link to the new bucket */` |
|   13173 |  858 | `	pEntry->pNextCollide = pMap->apBucket[nBucket];` |
|   13173 |  859 | `	if( pMap->apBucket[nBucket] ){` |
|   11098 |  860 | `		pMap->apBucket[nBucket]->pPrevCollide = pEntry;` |
|    5550 |  861 | `	}` |
|   13173 |  862 | `	pEntry->pNextCollide = pMap->apBucket[nBucket];` |
|   13173 |  863 | `	pMap->apBucket[nBucket] = pEntry;` |
|       - |  864 | `	/* Increment the automatic index (saturating, like every other advance —` |
|       - |  865 | `	 * unreachable in practice since renumbering assigns 0..nEntry-1, but keep` |
|       - |  866 | `	 * the no-overflow invariant uniform). */` |
|   13173 |  867 | `	if( pMap->iNextIdx < SXI64_HIGH ){` |
|   13173 |  868 | `		pMap->iNextIdx++;` |
|    6584 |  869 | `	}` |
|   13173 |  870 | `}` |
|       - |  871 | `/*` |
|       - |  872 | ` * Perform a linear search on a given hashmap.` |
|       - |  873 | ` * Write a pointer to the target node on success.` |
|       - |  874 | ` * Otherwise SXERR_NOTFOUND is returned on failure.` |
|       - |  875 | ` * Refer to [array_intersect(),array_diff(),in_array(),...] implementations` |
|       - |  876 | ` * for more information.` |
|       - |  877 | ` */` |
|   32146 |  878 | `static int HashmapFindValue(` |
|       - |  879 | `	ph7_hashmap *pMap,   /* Target hashmap */` |
|       - |  880 | `	ph7_value *pNeedle,  /* Lookup key */` |
|       - |  881 | `	ph7_hashmap_node **ppNode, /* OUT: target node on success  */` |
|       - |  882 | `	int bStrict      /* TRUE for strict comparison */` |
|       - |  883 | `	)` |
|       5 |  884 | `{` |
|       - |  885 | `	ph7_hashmap_node *pEntry;` |
|       - |  886 | `	ph7_value sVal,*pVal;` |
|       - |  887 | `	ph7_value sNeedle;` |
|       - |  888 | `	sxi32 rc;` |
|       - |  889 | `	sxu32 n;` |
|       - |  890 | `	/* Perform a linear search since we cannot sort the hashmap based on values */` |
|   32151 |  891 | `	pEntry = pMap->pFirst;` |
|   32151 |  892 | `	n = pMap->nEntry;` |
|   32151 |  893 | `	PH7_MemObjInit(pMap->pVm,&sVal);` |
|   32151 |  894 | `	PH7_MemObjInit(pMap->pVm,&sNeedle);` |
|   76383 |  895 | `	for(;;){` |
|  152772 |  896 | `		if( n < 1 ){` |
|      99 |  897 | `			break;` |
|       - |  898 | `		}` |
|       - |  899 | `		/* Extract node value */` |
|  152674 |  900 | `		pVal = HashmapExtractNodeValue(pEntry);` |
|  152674 |  901 | `		if( pVal ){` |
|  152674 |  902 | `			if( (pVal->iFlags\|pNeedle->iFlags) & MEMOBJ_NULL ){` |
|     ! 0 |  903 | `				sxi32 iF1 = pVal->iFlags&~MEMOBJ_AUX;` |
|     ! 0 |  904 | `				sxi32 iF2 = pNeedle->iFlags&~MEMOBJ_AUX;` |
|     ! 0 |  905 | `				if( iF1 == iF2 ){` |
|       - |  906 | `					/* NULL values are equals */` |
|     ! 0 |  907 | `					if( ppNode ){` |
|     ! 0 |  908 | `						*ppNode = pEntry;` |
|     ! 0 |  909 | `					}` |
|     ! 0 |  910 | `					return SXRET_OK;` |
|       - |  911 | `				}` |
|     ! 0 |  912 | `			}else{` |
|       - |  913 | `				/* Duplicate value */` |
|  152674 |  914 | `				PH7_MemObjLoad(pVal,&sVal);` |
|  152674 |  915 | `				PH7_MemObjLoad(pNeedle,&sNeedle);` |
|  152674 |  916 | `				rc = PH7_MemObjCmp(&sNeedle,&sVal,bStrict,0);` |
|  152674 |  917 | `				PH7_MemObjRelease(&sVal);` |
|  152674 |  918 | `				PH7_MemObjRelease(&sNeedle);` |
|  152674 |  919 | `				if( rc == 0 ){` |
|   32053 |  920 | `					if( ppNode ){` |
|      23 |  921 | `						*ppNode = pEntry;` |
|      11 |  922 | `					}` |
|       - |  923 | `					/* Match found*/` |
|   32053 |  924 | `					return SXRET_OK;` |
|       - |  925 | `				}` |
|       - |  926 | `			}` |
|   60310 |  927 | `		}` |
|       - |  928 | `		/* Point to the next entry */` |
|  120626 |  929 | `		pEntry = pEntry->pPrev; /* Reverse link */` |
|  120626 |  930 | `		n--;` |
|       5 |  931 | `	}` |
|       - |  932 | `	/* No such entry */` |
|      99 |  933 | `	return SXERR_NOTFOUND;` |
|   16078 |  934 | `}` |
|       - |  935 | `/*` |
|       - |  936 | ` * Perform a linear search on a given hashmap but use an user-defined callback` |
|       - |  937 | ` * for values comparison.` |
|       - |  938 | ` * Write a pointer to the target node on success.` |
|       - |  939 | ` * Otherwise SXERR_NOTFOUND is returned on failure.` |
|       - |  940 | ` * Refer to [array_uintersect(),array_udiff()...] implementations` |
|       - |  941 | ` * for more information.` |
|       - |  942 | ` */` |
|      22 |  943 | `static int HashmapFindValueByCallback(` |
|       - |  944 | `	ph7_hashmap *pMap,     /* Target hashmap */` |
|       - |  945 | `	ph7_value *pNeedle,    /* Lookup key */` |
|       - |  946 | `	ph7_value *pCallback,  /* User defined callback */` |
|       - |  947 | `	ph7_hashmap_node **ppNode /* OUT: target node on success */` |
|       - |  948 | `	)` |
|       1 |  949 | `{` |
|       - |  950 | `	ph7_hashmap_node *pEntry;` |
|       - |  951 | `	ph7_value sResult,*pVal;` |
|       - |  952 | `	ph7_value *apArg[2];    /* Callback arguments */` |
|       - |  953 | `	sxi32 rc;` |
|       - |  954 | `	sxu32 n;` |
|      23 |  955 | `	if( pMap->pVm->iCmpCallbackExc ){` |
|       - |  956 | `		/* A previous comparison already raised: stop invoking the callback so the` |
|       - |  957 | `		 * exception is not thrown again, and let the caller wind down. */` |
|     ! 0 |  958 | `		return SXERR_NOTFOUND;` |
|       - |  959 | `	}` |
|       - |  960 | `	/* Perform a linear search since we cannot sort the array based on values */` |
|      23 |  961 | `	pEntry = pMap->pFirst;` |
|      23 |  962 | `	n = pMap->nEntry;` |
|       - |  963 | `	/* Store callback result here */` |
|      23 |  964 | `	PH7_MemObjInit(pMap->pVm,&sResult);` |
|       - |  965 | `	/* First argument to the callback */` |
|      23 |  966 | `	apArg[0] = pNeedle;` |
|      25 |  967 | `	for(;;){` |
|      51 |  968 | `		if( n < 1 ){` |
|       9 |  969 | `			break;` |
|       - |  970 | `		}` |
|       - |  971 | `		/* Extract node value */` |
|      43 |  972 | `		pVal = HashmapExtractNodeValue(pEntry);` |
|      43 |  973 | `		if( pVal ){` |
|       - |  974 | `			/* Invoke the user callback */` |
|      43 |  975 | `			apArg[1] = pVal; /* Second argument to the callback */` |
|      43 |  976 | `			rc = PH7_VmCallUserFunction(pMap->pVm,pCallback,2,apArg,&sResult);` |
|      43 |  977 | `			if( rc == PH7_EXCEPTION ){` |
|       - |  978 | `				/* The callback raised: flag it so the caller aborts and propagates,` |
|       - |  979 | `				 * and report no match for the rest of the run. */` |
|       5 |  980 | `				pMap->pVm->iCmpCallbackExc = 1;` |
|       5 |  981 | `				PH7_MemObjRelease(&sResult);` |
|       5 |  982 | `				return SXERR_NOTFOUND;` |
|       - |  983 | `			}` |
|      39 |  984 | `			if( rc == SXRET_OK ){` |
|       - |  985 | `				/* Extract callback result */` |
|      39 |  986 | `				if( (sResult.iFlags & MEMOBJ_INT) == 0 ){` |
|       - |  987 | `					/* Perform an int cast */` |
|     ! 0 |  988 | `					PH7_MemObjToInteger(&sResult);` |
|     ! 0 |  989 | `				}` |
|      39 |  990 | `				rc = (sxi32)sResult.x.iVal;` |
|      39 |  991 | `				PH7_MemObjRelease(&sResult);` |
|      39 |  992 | `				if( rc == 0 ){` |
|       - |  993 | `					/* Match found*/` |
|      11 |  994 | `					if( ppNode ){` |
|     ! 0 |  995 | `						*ppNode = pEntry;` |
|     ! 0 |  996 | `					}` |
|      11 |  997 | `					return SXRET_OK;` |
|       - |  998 | `				}` |
|      14 |  999 | `			}` |
|      14 | 1000 | `		}` |
|       - | 1001 | `		/* Point to the next entry */` |
|      29 | 1002 | `		pEntry = pEntry->pPrev; /* Reverse link */` |
|      29 | 1003 | `		n--;` |
|       1 | 1004 | `	}` |
|       - | 1005 | `	/* No such entry */` |
|       9 | 1006 | `	return SXERR_NOTFOUND;` |
|      12 | 1007 | `}` |
|       - | 1008 | `/*` |
|       - | 1009 | ` * Compare two hashmaps.` |
|       - | 1010 | ` * Return 0 if the hashmaps are equals.Any other value indicates inequality.` |
|       - | 1011 | ` * Note on array comparison operators.` |
|       - | 1012 | ` *  According to the PHP language reference manual.` |
|       - | 1013 | ` *  Array Operators Example 	Name 	Result` |
|       - | 1014 | ` *  $a + $b 	Union 	Union of $a and $b.` |
|       - | 1015 | ` *  $a == $b 	Equality 	TRUE if $a and $b have the same key/value pairs.` |
|       - | 1016 | ` *  $a === $b 	Identity 	TRUE if $a and $b have the same key/value pairs in the same` |
|       - | 1017 | ` *                          order and of the same types.` |
|       - | 1018 | ` *  $a != $b 	Inequality 	TRUE if $a is not equal to $b.` |
|       - | 1019 | ` *  $a <> $b 	Inequality 	TRUE if $a is not equal to $b.` |
|       - | 1020 | ` *  $a !== $b 	Non-identity 	TRUE if $a is not identical to $b.` |
|       - | 1021 | ` * The + operator returns the right-hand array appended to the left-hand array;` |
|       - | 1022 | ` * For keys that exist in both arrays, the elements from the left-hand array will be used` |
|       - | 1023 | ` * and the matching elements from the right-hand array will be ignored.` |
|       - | 1024 | ` * <?php` |
|       - | 1025 | ` * $a = array("a" => "apple", "b" => "banana");` |
|       - | 1026 | ` * $b = array("a" => "pear", "b" => "strawberry", "c" => "cherry");` |
|       - | 1027 | ` * $c = $a + $b; // Union of $a and $b` |
|       - | 1028 | ` * echo "Union of \$a and \$b: \n";` |
|       - | 1029 | ` * var_dump($c);` |
|       - | 1030 | ` * $c = $b + $a; // Union of $b and $a` |
|       - | 1031 | ` * echo "Union of \$b and \$a: \n";` |
|       - | 1032 | ` * var_dump($c);` |
|       - | 1033 | ` * ?>` |
|       - | 1034 | ` * When executed, this script will print the following:` |
|       - | 1035 | ` * Union of $a and $b:` |
|       - | 1036 | ` * array(3) {` |
|       - | 1037 | ` *  ["a"]=>` |
|       - | 1038 | ` *  string(5) "apple"` |
|       - | 1039 | ` *  ["b"]=>` |
|       - | 1040 | ` * string(6) "banana"` |
|       - | 1041 | ` *  ["c"]=>` |
|       - | 1042 | ` * string(6) "cherry"` |
|       - | 1043 | ` * }` |
|       - | 1044 | ` * Union of $b and $a:` |
|       - | 1045 | ` * array(3) {` |
|       - | 1046 | ` * ["a"]=>` |
|       - | 1047 | ` * string(4) "pear"` |
|       - | 1048 | ` * ["b"]=>` |
|       - | 1049 | ` * string(10) "strawberry"` |
|       - | 1050 | ` * ["c"]=>` |
|       - | 1051 | ` * string(6) "cherry"` |
|       - | 1052 | ` * }` |
|       - | 1053 | ` * Elements of arrays are equal for the comparison if they have the same key and value.` |
|       - | 1054 | ` */` |
|      28 | 1055 | `PH7_PRIVATE sxi32 PH7_HashmapCmp(` |
|       - | 1056 | `	ph7_hashmap *pLeft,  /* Left hashmap */` |
|       - | 1057 | `	ph7_hashmap *pRight, /* Right hashmap */` |
|       - | 1058 | `	int bStrict          /* TRUE for strict comparison */` |
|       - | 1059 | `	)` |
|       1 | 1060 | `{` |
|       - | 1061 | `	ph7_hashmap_node *pLe,*pRe;` |
|       - | 1062 | `	sxi32 rc;` |
|       - | 1063 | `	sxu32 n;` |
|      29 | 1064 | `	if( pLeft == pRight ){` |
|       - | 1065 | `		/* Same hashmap instance. This can easily happen since hashmaps are passed by reference.` |
|       - | 1066 | `		 * Unlike the zend engine.` |
|       - | 1067 | `		 */` |
|     ! 0 | 1068 | `		return 0;` |
|       - | 1069 | `	}` |
|      29 | 1070 | `	if( pLeft->nEntry != pRight->nEntry ){` |
|       - | 1071 | `		/* Must have the same number of entries */` |
|       5 | 1072 | `		return pLeft->nEntry > pRight->nEntry ? 1 : -1;` |
|       - | 1073 | `	}` |
|       - | 1074 | `	/* Point to the first inserted entry of the left hashmap */` |
|      25 | 1075 | `	pLe = pLeft->pFirst;` |
|      25 | 1076 | `	pRe = 0; /* cc warning */` |
|       - | 1077 | `	/* Perform the comparison */` |
|      25 | 1078 | `	n = pLeft->nEntry;` |
|      59 | 1079 | `	for(;;){` |
|     119 | 1080 | `		if( n < 1 ){` |
|      23 | 1081 | `			break;` |
|       - | 1082 | `		}` |
|      97 | 1083 | `		if( pLe->iType == HASHMAP_INT_NODE){` |
|       - | 1084 | `			/* Int key */` |
|      89 | 1085 | `			rc = HashmapLookupIntKey(&(*pRight),pLe->xKey.iKey,&pRe);` |
|      45 | 1086 | `		}else{` |
|       9 | 1087 | `			SyBlob *pKey = &pLe->xKey.sKey;` |
|       - | 1088 | `			/* Blob key */` |
|       9 | 1089 | `			rc = HashmapLookupBlobKey(&(*pRight),SyBlobData(pKey),SyBlobLength(pKey),&pRe);` |
|       - | 1090 | `		}` |
|      97 | 1091 | `		if( rc != SXRET_OK ){` |
|       - | 1092 | `			/* No such entry in the right side */` |
|     ! 0 | 1093 | `			return 1;` |
|       - | 1094 | `		}` |
|      97 | 1095 | `		rc = 0;` |
|      97 | 1096 | `		if( bStrict ){` |
|       - | 1097 | `			/* Make sure,the keys are of the same type */` |
|      81 | 1098 | `			if( pLe->iType != pRe->iType ){` |
|     ! 0 | 1099 | `				rc = 1;` |
|     ! 0 | 1100 | `			}` |
|      40 | 1101 | `		}` |
|      97 | 1102 | `		if( !rc ){` |
|       - | 1103 | `			/* Compare nodes */` |
|      97 | 1104 | `			rc = HashmapNodeCmp(pLe,pRe,bStrict);` |
|      48 | 1105 | `		}` |
|      97 | 1106 | `		if( rc != 0 ){` |
|       - | 1107 | `			/* Nodes key/value differ */` |
|       3 | 1108 | `			return rc;` |
|       - | 1109 | `		}` |
|       - | 1110 | `		/* Point to the next entry */` |
|      95 | 1111 | `		pLe = pLe->pPrev; /* Reverse link */` |
|      95 | 1112 | `		n--;` |
|       1 | 1113 | `	}` |
|      23 | 1114 | `	return 0; /* Hashmaps are equals */` |
|      15 | 1115 | `}` |
|       - | 1116 | `/*` |
|       - | 1117 | ` * Duplicate a hashmap node.` |
|       - | 1118 | ` * This function is used by HashmapMerge, HashmapOverwrite and PH7_HashmapDup.` |
|       - | 1119 | ` */` |
|  621020 | 1120 | `static sxi32 HashmapDuplicateNode(` |
|       - | 1121 | `	ph7_hashmap *pDest,` |
|       - | 1122 | `	ph7_hashmap_node *pEntry,` |
|       - | 1123 | `	ph7_value *pVal,` |
|       - | 1124 | `	int iAction /* 0: Merge, 1: Overwrite, 2: Dup */` |
|       - | 1125 | `	)` |
|       5 | 1126 | `{` |
|       - | 1127 | `	ph7_value sSafeVal;` |
|       - | 1128 | `	ph7_value sKey;` |
|       - | 1129 | `	sxi32 rc;` |
|       - | 1130 |  |
|  621025 | 1131 | `	if( pEntry->iFlags & HASHMAP_NODE_FOREIGN_OBJ ){` |
|       - | 1132 | `		/* The source node holds a reference to a foreign ph7_value (e.g: [&$x]).` |
|       - | 1133 | `		 * Re-insert it by reference so the reference survives the duplication` |
|       - | 1134 | `		 * instead of being flattened to a value copy. This keeps spread` |
|       - | 1135 | `		 * ([...$a]), array_merge(), array_replace() and array copies in sync` |
|       - | 1136 | `		 * with PHP semantics. */` |
|       7 | 1137 | `		sxu32 nRefIdx = pEntry->nValIdx;` |
|       7 | 1138 | `		if( pEntry->iType == HASHMAP_BLOB_NODE ){` |
|       3 | 1139 | `			PH7_MemObjInitFromString(pDest->pVm,&sKey,0);` |
|       3 | 1140 | `			PH7_MemObjStringAppend(&sKey,(const char *)SyBlobData(&pEntry->xKey.sKey),SyBlobLength(&pEntry->xKey.sKey));` |
|       3 | 1141 | `			rc = HashmapInsertByRef(pDest,&sKey,nRefIdx);` |
|       3 | 1142 | `			PH7_MemObjRelease(&sKey);` |
|       2 | 1143 | `		}else{` |
|       5 | 1144 | `			if( iAction == 0 ){ /* Merge: automatic index assign */` |
|       5 | 1145 | `				rc = HashmapInsertByRef(pDest,0,nRefIdx);` |
|       2 | 1146 | `			}else if( iAction == 1 ){ /* Overwrite: keep the int key */` |
|     ! 0 | 1147 | `				PH7_MemObjInitFromInt(pDest->pVm,&sKey,pEntry->xKey.iKey);` |
|     ! 0 | 1148 | `				rc = HashmapInsertByRef(pDest,&sKey,nRefIdx);` |
|     ! 0 | 1149 | `				PH7_MemObjRelease(&sKey);` |
|     ! 0 | 1150 | `			}else{ /* Dup: preserve the int key */` |
|     ! 0 | 1151 | `				rc = HashmapInsertIntKey(pDest,pEntry->xKey.iKey,0,nRefIdx,TRUE);` |
|       - | 1152 | `			}` |
|       - | 1153 | `		}` |
|       7 | 1154 | `		return rc;` |
|       - | 1155 | `	}` |
|  621019 | 1156 | `	sSafeVal = *pVal;` |
|       - | 1157 |  |
|  621019 | 1158 | `	if( pEntry->iType == HASHMAP_BLOB_NODE ){` |
|       - | 1159 | `		/* Blob key insertion */` |
|    3893 | 1160 | `		PH7_MemObjInitFromString(pDest->pVm,&sKey,0);` |
|    3893 | 1161 | `		PH7_MemObjStringAppend(&sKey,(const char *)SyBlobData(&pEntry->xKey.sKey),SyBlobLength(&pEntry->xKey.sKey));` |
|    3893 | 1162 | `		rc = PH7_HashmapInsert(pDest,&sKey,&sSafeVal);` |
|    3893 | 1163 | `		PH7_MemObjRelease(&sKey);` |
|    1949 | 1164 | `	}else{` |
|       - | 1165 | `		/* Int key */` |
|  617131 | 1166 | `		if( iAction == 0 ){ /* Merge */` |
|  616921 | 1167 | `			rc = HashmapInsert(pDest,0/* Automatic index assign */,&sSafeVal);` |
|  308671 | 1168 | `		}else if( iAction == 1 ){ /* Overwrite */` |
|      32 | 1169 | `			PH7_MemObjInitFromInt(pDest->pVm,&sKey,pEntry->xKey.iKey);` |
|      32 | 1170 | `			rc = PH7_HashmapInsert(pDest,&sKey,&sSafeVal);` |
|      32 | 1171 | `			PH7_MemObjRelease(&sKey);` |
|      17 | 1172 | `		}else{ /* Dup */` |
|     182 | 1173 | `			rc = HashmapInsertIntKey(pDest,pEntry->xKey.iKey,&sSafeVal,0,FALSE);` |
|       - | 1174 | `		}` |
|       - | 1175 | `	}` |
|  621019 | 1176 | `	return rc;` |
|  310515 | 1177 | `}` |
|       - | 1178 | `/*` |
|       - | 1179 | ` * Merge two hashmaps.` |
|       - | 1180 | ` * Note on the merge process` |
|       - | 1181 | ` * According to the PHP language reference manual.` |
|       - | 1182 | ` *  Merges the elements of two arrays together so that the values of one are appended` |
|       - | 1183 | ` *  to the end of the previous one. It returns the resulting array (pDest).` |
|       - | 1184 | ` *  If the input arrays have the same string keys, then the later value for that key` |
|       - | 1185 | ` *  will overwrite the previous one. If, however, the arrays contain numeric keys` |
|       - | 1186 | ` *  the later value will not overwrite the original value, but will be appended.` |
|       - | 1187 | ` *  Values in the input array with numeric keys will be renumbered with incrementing` |
|       - | 1188 | ` *  keys starting from zero in the result array.` |
|       - | 1189 | ` */` |
|    2104 | 1190 | `static sxi32 HashmapMerge(ph7_hashmap *pSrc,ph7_hashmap *pDest)` |
|       5 | 1191 | `{` |
|       - | 1192 | `	ph7_hashmap_node *pEntry;` |
|       - | 1193 | `	ph7_value *pVal;` |
|       - | 1194 | `	sxi32 rc;` |
|       - | 1195 | `	sxu32 n;` |
|    2109 | 1196 | `	if( pSrc == pDest ){` |
|       - | 1197 | `		/* Same map. This can easily happen since hashmaps are passed by reference.` |
|       - | 1198 | `		 * Unlike the zend engine.` |
|       - | 1199 | `		 */` |
|     ! 0 | 1200 | `		return SXRET_OK;` |
|       - | 1201 | `	}` |
|       - | 1202 | `	/* Point to the first inserted entry in the source */` |
|    2109 | 1203 | `	pEntry = pSrc->pFirst;` |
|       - | 1204 | `	/* Perform the merge */` |
|  619083 | 1205 | `	for( n = 0 ; n < pSrc->nEntry ; ++n ){` |
|       - | 1206 | `		/* Extract the node value */` |
|  616979 | 1207 | `		pVal = HashmapExtractNodeValue(pEntry);` |
|  616979 | 1208 | `		if( pVal ){` |
|       - | 1209 | `			/* Make a local copy of the value.` |
|       - | 1210 | `			 * The insertion call below may trigger a memory pool reallocation` |
|       - | 1211 | `			 * which will invalidate the 'pVal' pointer since it points` |
|       - | 1212 | `			 * to the old pool.` |
|       - | 1213 | `			 */` |
|  616979 | 1214 | `			rc = HashmapDuplicateNode(pDest,pEntry,pVal,0);` |
|  308492 | 1215 | `		}else{` |
|     ! 0 | 1216 | `			rc = SXRET_OK;` |
|       - | 1217 | `		}` |
|  616979 | 1218 | `		if( rc != SXRET_OK ){` |
|     ! 0 | 1219 | `			return rc;` |
|       - | 1220 | `		}` |
|       - | 1221 | `		/* Point to the next entry */` |
|  616979 | 1222 | `		pEntry = pEntry->pPrev; /* Reverse link */` |
|  308492 | 1223 | `	}` |
|    2109 | 1224 | `	return SXRET_OK;` |
|    1057 | 1225 | `}` |
|       - | 1226 | `/*` |
|       - | 1227 | ` * Overwrite entries with the same key.` |
|       - | 1228 | ` * Refer to the [array_replace()] implementation for more information.` |
|       - | 1229 | ` *  According to the PHP language reference manual.` |
|       - | 1230 | ` *  array_replace() replaces the values of the first array with the same values` |
|       - | 1231 | ` *  from all the following arrays. If a key from the first array exists in the second` |
|       - | 1232 | ` *  array, its value will be replaced by the value from the second array. If the key` |
|       - | 1233 | ` *  exists in the second array, and not the first, it will be created in the first array.` |
|       - | 1234 | ` *  If a key only exists in the first array, it will be left as is. If several arrays` |
|       - | 1235 | ` *  are passed for replacement, they will be processed in order, the later arrays` |
|       - | 1236 | ` *  overwriting the previous values.` |
|       - | 1237 | ` *  array_replace() is not recursive : it will replace values in the first array` |
|       - | 1238 | ` *  by whatever type is in the second array.` |
|       - | 1239 | ` */` |
|      34 | 1240 | `static sxi32 HashmapOverwrite(ph7_hashmap *pSrc,ph7_hashmap *pDest)` |
|       2 | 1241 | `{` |
|       - | 1242 | `	ph7_hashmap_node *pEntry;` |
|       - | 1243 | `	ph7_value *pVal;` |
|       - | 1244 | `	sxi32 rc;` |
|       - | 1245 | `	sxu32 n;` |
|      36 | 1246 | `	if( pSrc == pDest ){` |
|       - | 1247 | `		/* Same map. This can easily happen since hashmaps are passed by reference.` |
|       - | 1248 | `		 * Unlike the zend engine.` |
|       - | 1249 | `		 */` |
|     ! 0 | 1250 | `		return SXRET_OK;` |
|       - | 1251 | `	}` |
|       - | 1252 | `	/* Point to the first inserted entry in the source */` |
|      36 | 1253 | `	pEntry = pSrc->pFirst;` |
|       - | 1254 | `	/* Perform the merge */` |
|      80 | 1255 | `	for( n = 0 ; n < pSrc->nEntry ; ++n ){` |
|       - | 1256 | `		/* Extract the node value */` |
|      46 | 1257 | `		pVal = HashmapExtractNodeValue(pEntry);` |
|      46 | 1258 | `		if( pVal ){` |
|      46 | 1259 | `			rc = HashmapDuplicateNode(pDest,pEntry,pVal,1);` |
|      24 | 1260 | `		}else{` |
|     ! 0 | 1261 | `			rc = SXRET_OK;` |
|       - | 1262 | `		}` |
|      46 | 1263 | `		if( rc != SXRET_OK ){` |
|     ! 0 | 1264 | `			return rc;` |
|       - | 1265 | `		}` |
|       - | 1266 | `		/* Point to the next entry */` |
|      46 | 1267 | `		pEntry = pEntry->pPrev; /* Reverse link */` |
|      24 | 1268 | `	}` |
|      36 | 1269 | `	return SXRET_OK;` |
|      19 | 1270 | `}` |
|       - | 1271 | `/*` |
|       - | 1272 | ` * Duplicate the contents of a hashmap. Store the copy in pDest.` |
|       - | 1273 | ` * Refer to the [array_pad(),array_copy(),...] implementation for more information.` |
|       - | 1274 | ` */` |
|    3898 | 1275 | `PH7_PRIVATE sxi32 PH7_HashmapDup(ph7_hashmap *pSrc,ph7_hashmap *pDest)` |
|       5 | 1276 | `{` |
|       - | 1277 | `	ph7_hashmap_node *pEntry;` |
|       - | 1278 | `	ph7_value *pVal;` |
|       - | 1279 | `	sxi32 rc;` |
|       - | 1280 | `	sxu32 n;` |
|    3903 | 1281 | `	if( pSrc == pDest ){` |
|       - | 1282 | `		/* Same map. This can easily happen since hashmaps are passed by reference.` |
|       - | 1283 | `		 * Unlike the zend engine.` |
|       - | 1284 | `		 */` |
|     ! 0 | 1285 | `		return SXRET_OK;` |
|       - | 1286 | `	}` |
|       - | 1287 | `	/* Point to the first inserted entry in the source */` |
|    3903 | 1288 | `	pEntry = pSrc->pFirst;` |
|       - | 1289 | `	/* Perform the duplication */` |
|    7905 | 1290 | `	for( n = 0 ; n < pSrc->nEntry ; ++n ){` |
|       - | 1291 | `		/* Extract the node value */` |
|    4007 | 1292 | `		pVal = HashmapExtractNodeValue(pEntry);` |
|    4007 | 1293 | `		if( pVal ){` |
|    4007 | 1294 | `			rc = HashmapDuplicateNode(pDest,pEntry,pVal,2);` |
|    2006 | 1295 | `		}else{` |
|     ! 0 | 1296 | `			rc = SXRET_OK;` |
|       - | 1297 | `		}` |
|    4007 | 1298 | `		if( rc != SXRET_OK ){` |
|     ! 0 | 1299 | `			return rc;` |
|       - | 1300 | `		}` |
|       - | 1301 | `		/* Point to the next entry */` |
|    4007 | 1302 | `		pEntry = pEntry->pPrev; /* Reverse link */` |
|    2006 | 1303 | `	}` |
|    3903 | 1304 | `	return SXRET_OK;` |
|    1954 | 1305 | `}` |
|       - | 1306 | `/*` |
|       - | 1307 | ` * Copy-on-write separation for arrays.` |
|       - | 1308 | ` * If the hashmap inside pValue has iRef > 1 (shared), duplicate it so that` |
|       - | 1309 | ` * pValue owns a private copy. The original map's refcount is decremented.` |
|       - | 1310 | ` * Returns the (possibly new) hashmap pointer.` |
|       - | 1311 | ` */` |
|  215058 | 1312 | `PH7_PRIVATE ph7_hashmap * PH7_HashmapCowSeparate(ph7_vm *pVm,ph7_value *pValue)` |
|       5 | 1313 | `{` |
|  215063 | 1314 | `	ph7_hashmap *pMap = (ph7_hashmap *)pValue->x.pOther;` |
|       - | 1315 | `	ph7_hashmap *pNew;` |
|       - | 1316 | `	ph7_value *pBacking;` |
|       - | 1317 | `	sxu32 nValIdx;` |
|       - | 1318 | `	int bValueInPool;` |
|  215063 | 1319 | `	if( pMap->iRef < 2 ){` |
|       - | 1320 | `		/* Sole owner, no separation needed */` |
|  212889 | 1321 | `		return pMap;` |
|       - | 1322 | `	}` |
|    2179 | 1323 | `	if( pMap == pVm->pGlobal ){` |
|       - | 1324 | `		/* Never separate $GLOBALS */` |
|     ! 0 | 1325 | `		return pMap;` |
|       - | 1326 | `	}` |
|       - | 1327 | `	/* If this value is a stack copy of a named variable, separate the` |
|       - | 1328 | `	 * backing variable instead so the change persists after the stack` |
|       - | 1329 | `	 * frame is popped. */` |
|    2179 | 1330 | `	if( pValue->nIdx != SXU32_HIGH ){` |
|    2179 | 1331 | `		pBacking = (ph7_value *)SySetAt(&pVm->aMemObj,pValue->nIdx);` |
|    2174 | 1332 | `		if( pBacking && pBacking != pValue` |
|    2156 | 1333 | `			&& (pBacking->iFlags & MEMOBJ_HASHMAP)` |
|    2143 | 1334 | `			&& (ph7_hashmap *)pBacking->x.pOther == pMap ){` |
|       - | 1335 | `			/* Undo the stack ref to reveal true sharing count */` |
|    2143 | 1336 | `			pMap->iRef--;` |
|    2143 | 1337 | `			if( pMap->iRef < 2 ){` |
|       - | 1338 | `				/* After undoing stack ref, sole owner — no separation */` |
|    2103 | 1339 | `				pMap->iRef++;` |
|    2103 | 1340 | `				return pMap;` |
|       - | 1341 | `			}` |
|      42 | 1342 | `			pNew = PH7_NewHashmap(pVm,0,0);` |
|      42 | 1343 | `			if( pNew == 0 ){` |
|     ! 0 | 1344 | `				pMap->iRef++;` |
|     ! 0 | 1345 | `				return pMap;` |
|       - | 1346 | `			}` |
|      42 | 1347 | `			if( PH7_HashmapDup(pMap,pNew) != SXRET_OK ){` |
|       - | 1348 | `				/* Dup failed (OOM) — discard partial copy, restore state */` |
|     ! 0 | 1349 | `				PH7_HashmapRelease(pNew,TRUE);` |
|     ! 0 | 1350 | `				pMap->iRef++;` |
|     ! 0 | 1351 | `				return pMap;` |
|       - | 1352 | `			}` |
|      42 | 1353 | `			pNew->iNextIdx = pMap->iNextIdx;` |
|      42 | 1354 | `			pMap->iRef--;  /* Backing variable no longer references old map */` |
|       - | 1355 | `			/* PH7_HashmapDup reserves a memory object per duplicated entry, which` |
|       - | 1356 | `			 * can grow — and therefore reallocate (move) — pVm->aMemObj. That` |
|       - | 1357 | `			 * invalidates the pBacking pointer captured above, so re-resolve it` |
|       - | 1358 | `			 * from the (stable) slot index before writing. Using the stale pointer` |
|       - | 1359 | `			 * dereferences the freed old buffer, which is a hard SIGSEGV on` |
|       - | 1360 | `			 * glibc/x86_64 once aMemObj is large enough to be mmap-backed (the old` |
|       - | 1361 | `			 * mapping is munmap'd on move) and a silent use-after-free elsewhere. */` |
|      42 | 1362 | `			pBacking = (ph7_value *)SySetAt(&pVm->aMemObj,pValue->nIdx);` |
|      42 | 1363 | `			if( pBacking ){` |
|      42 | 1364 | `				pBacking->x.pOther = pNew;` |
|      20 | 1365 | `			}` |
|       - | 1366 | `			/* Update the stack value to match */` |
|      42 | 1367 | `			pValue->x.pOther = pNew;` |
|      42 | 1368 | `			pNew->iRef++;  /* +1 for stack (pValue); iRef=1 from NewHashmap covers pBacking */` |
|      42 | 1369 | `			return pNew;` |
|       - | 1370 | `		}` |
|      18 | 1371 | `	}` |
|       - | 1372 | `	/* Some callers (e.g. OP_STORE_IDX, by-ref foreach) pass a pValue that points` |
|       - | 1373 | `	 * directly into pVm->aMemObj. PH7_HashmapDup below reserves a memory object` |
|       - | 1374 | `	 * per duplicated entry, which can grow — and therefore reallocate (move) —` |
|       - | 1375 | `	 * pVm->aMemObj, leaving such a pValue dangling. Capture its slot identity now,` |
|       - | 1376 | `	 * before the dup, so the write-back can re-resolve from the (stable) index` |
|       - | 1377 | `	 * rather than dereference the captured pointer (the same hazard handled for` |
|       - | 1378 | `	 * pBacking in the backing-variable branch above). */` |
|      37 | 1379 | `	nValIdx = pValue->nIdx;` |
|      55 | 1380 | `	bValueInPool = ( nValIdx != SXU32_HIGH` |
|      36 | 1381 | `		&& (ph7_value *)SySetAt(&pVm->aMemObj,nValIdx) == pValue );` |
|      37 | 1382 | `	pNew = PH7_NewHashmap(pVm,0,0);` |
|      37 | 1383 | `	if( pNew == 0 ){` |
|       - | 1384 | `		/* Allocation failure — fall through with shared map */` |
|     ! 0 | 1385 | `		return pMap;` |
|       - | 1386 | `	}` |
|      37 | 1387 | `	if( PH7_HashmapDup(pMap,pNew) != SXRET_OK ){` |
|       - | 1388 | `		/* Dup failed (OOM) — discard partial copy, keep original */` |
|     ! 0 | 1389 | `		PH7_HashmapRelease(pNew,TRUE);` |
|     ! 0 | 1390 | `		return pMap;` |
|       - | 1391 | `	}` |
|      37 | 1392 | `	pNew->iNextIdx = pMap->iNextIdx;` |
|      37 | 1393 | `	pMap->iRef--;` |
|      37 | 1394 | `	if( bValueInPool ){` |
|       - | 1395 | `		/* aMemObj may have moved during the dup — re-resolve pValue's slot. */` |
|      37 | 1396 | `		pValue = (ph7_value *)SySetAt(&pVm->aMemObj,nValIdx);` |
|      37 | 1397 | `		if( pValue == 0 ){` |
|     ! 0 | 1398 | `			return pNew;` |
|       - | 1399 | `		}` |
|      18 | 1400 | `	}` |
|      37 | 1401 | `	pValue->x.pOther = pNew;` |
|      37 | 1402 | `	return pNew;` |
|  107534 | 1403 | `}` |
|       - | 1404 | `/*` |
|       - | 1405 | ` * Perform the union of two hashmaps.` |
|       - | 1406 | ` * This operation is performed only if the user uses the '+' operator` |
|       - | 1407 | ` * with a variable holding an array as follows:` |
|       - | 1408 | ` * <?php` |
|       - | 1409 | ` * $a = array("a" => "apple", "b" => "banana");` |
|       - | 1410 | ` * $b = array("a" => "pear", "b" => "strawberry", "c" => "cherry");` |
|       - | 1411 | ` * $c = $a + $b; // Union of $a and $b` |
|       - | 1412 | ` * echo "Union of \$a and \$b: \n";` |
|       - | 1413 | ` * var_dump($c);` |
|       - | 1414 | ` * $c = $b + $a; // Union of $b and $a` |
|       - | 1415 | ` * echo "Union of \$b and \$a: \n";` |
|       - | 1416 | ` * var_dump($c);` |
|       - | 1417 | ` * ?>` |
|       - | 1418 | ` * When executed, this script will print the following:` |
|       - | 1419 | ` * Union of $a and $b:` |
|       - | 1420 | ` * array(3) {` |
|       - | 1421 | ` *  ["a"]=>` |
|       - | 1422 | ` *  string(5) "apple"` |
|       - | 1423 | ` *  ["b"]=>` |
|       - | 1424 | ` * string(6) "banana"` |
|       - | 1425 | ` *  ["c"]=>` |
|       - | 1426 | ` * string(6) "cherry"` |
|       - | 1427 | ` * }` |
|       - | 1428 | ` * Union of $b and $a:` |
|       - | 1429 | ` * array(3) {` |
|       - | 1430 | ` * ["a"]=>` |
|       - | 1431 | ` * string(4) "pear"` |
|       - | 1432 | ` * ["b"]=>` |
|       - | 1433 | ` * string(10) "strawberry"` |
|       - | 1434 | ` * ["c"]=>` |
|       - | 1435 | ` * string(6) "cherry"` |
|       - | 1436 | ` * }` |
|       - | 1437 | ` * The + operator returns the right-hand array appended to the left-hand array;` |
|       - | 1438 | ` * For keys that exist in both arrays, the elements from the left-hand array will be used` |
|       - | 1439 | ` * and the matching elements from the right-hand array will be ignored.` |
|       - | 1440 | ` */` |
|    3798 | 1441 | `PH7_PRIVATE sxi32 PH7_HashmapUnion(ph7_hashmap *pLeft,ph7_hashmap *pRight)` |
|       5 | 1442 | `{` |
|       - | 1443 | `	ph7_hashmap_node *pEntry;` |
|    3803 | 1444 | `	sxi32 rc = SXRET_OK;` |
|       - | 1445 | `	ph7_value *pObj;` |
|       - | 1446 | `	sxu32 n;` |
|    3803 | 1447 | `	if( pLeft == pRight ){` |
|       - | 1448 | `		/* Same map. This can easily happen since hashmaps are passed by reference.` |
|       - | 1449 | `		 * Unlike the zend engine.` |
|       - | 1450 | `		 */` |
|     ! 0 | 1451 | `		return SXRET_OK;` |
|       - | 1452 | `	}` |
|       - | 1453 | `	/* Perform the union */` |
|    3803 | 1454 | `	pEntry = pRight->pFirst;` |
|    3837 | 1455 | `	for(n = 0 ; n < pRight->nEntry ; ++n ){` |
|       - | 1456 | `		/* Make sure the given key does not exists in the left array */` |
|      39 | 1457 | `		if( pEntry->iType == HASHMAP_BLOB_NODE ){` |
|       - | 1458 | `			/* BLOB key */` |
|      24 | 1459 | `			if( SXRET_OK !=` |
|      20 | 1460 | `				HashmapLookupBlobKey(&(*pLeft),SyBlobData(&pEntry->xKey.sKey),SyBlobLength(&pEntry->xKey.sKey),0) ){` |
|      20 | 1461 | `					pObj = HashmapExtractNodeValue(pEntry);` |
|      20 | 1462 | `					if( pObj ){` |
|      20 | 1463 | `						ph7_value sSafeVal = *pObj;` |
|       - | 1464 | `						/* Perform the insertion */` |
|      20 | 1465 | `						rc = HashmapInsertBlobKey(&(*pLeft),SyBlobData(&pEntry->xKey.sKey),SyBlobLength(&pEntry->xKey.sKey),` |
|       - | 1466 | `							&sSafeVal,0,FALSE);` |
|      20 | 1467 | `						if( rc != SXRET_OK ){` |
|     ! 0 | 1468 | `							return rc;` |
|       - | 1469 | `						}` |
|       8 | 1470 | `					}` |
|       8 | 1471 | `			}` |
|      14 | 1472 | `		}else{` |
|       - | 1473 | `			/* INT key */` |
|      16 | 1474 | `			if( SXRET_OK != HashmapLookupIntKey(&(*pLeft),pEntry->xKey.iKey,0) ){` |
|      11 | 1475 | `				pObj = HashmapExtractNodeValue(pEntry);` |
|      11 | 1476 | `				if( pObj ){` |
|      11 | 1477 | `					ph7_value sSafeVal = *pObj;` |
|       - | 1478 | `					/* Perform the insertion */` |
|      11 | 1479 | `					rc = HashmapInsertIntKey(&(*pLeft),pEntry->xKey.iKey,&sSafeVal,0,FALSE);` |
|      11 | 1480 | `					if( rc != SXRET_OK ){` |
|     ! 0 | 1481 | `						return rc;` |
|       - | 1482 | `					}` |
|       5 | 1483 | `				}` |
|       5 | 1484 | `			}` |
|       - | 1485 | `		}` |
|       - | 1486 | `		/* Point to the next entry */` |
|      39 | 1487 | `		pEntry = pEntry->pPrev; /* Reverse link */` |
|      22 | 1488 | `	}` |
|    3803 | 1489 | `	return SXRET_OK;` |
|    1904 | 1490 | `}` |
|       - | 1491 | `/*` |
|       - | 1492 | ` * Allocate a new hashmap.` |
|       - | 1493 | ` * Return a pointer to the freshly allocated hashmap on success.NULL otherwise.` |
|       - | 1494 | ` */` |
|  115032 | 1495 | `PH7_PRIVATE ph7_hashmap * PH7_NewHashmap(` |
|       - | 1496 | `	ph7_vm *pVm,              /* VM that trigger the hashmap creation */` |
|       - | 1497 | `	sxu32 (*xIntHash)(sxi64), /* Hash function for int keys.NULL otherwise*/` |
|       - | 1498 | `	sxu32 (*xBlobHash)(const void *,sxu32) /* Hash function for BLOB keys.NULL otherwise */` |
|       - | 1499 | `	)` |
|       5 | 1500 | `{` |
|       - | 1501 | `	ph7_hashmap *pMap;` |
|       - | 1502 | `	/* Allocate a new instance */` |
|  115037 | 1503 | `	pMap = (ph7_hashmap *)SyMemBackendPoolAlloc(&pVm->sAllocator,sizeof(ph7_hashmap));` |
|  115037 | 1504 | `	if( pMap == 0 ){` |
|     ! 0 | 1505 | `		return 0;` |
|       - | 1506 | `	}` |
|       - | 1507 | `	/* Zero the structure */` |
|  115037 | 1508 | `	SyZero(pMap,sizeof(ph7_hashmap));` |
|       - | 1509 | `	/* Fill in the structure */` |
|  115037 | 1510 | `	pMap->pVm = &(*pVm);` |
|  115037 | 1511 | `	pMap->iRef = 1;` |
|       - | 1512 | `	/* Default hash functions */` |
|  115037 | 1513 | `	pMap->xIntHash  = xIntHash ? xIntHash : IntHash;` |
|  115037 | 1514 | `	pMap->xBlobHash = xBlobHash ? xBlobHash : BinHash;` |
|  115037 | 1515 | `	return pMap;` |
|   57521 | 1516 | `}` |
|       - | 1517 | `/*` |
|       - | 1518 | ` * Install superglobals in the given virtual machine.` |
|       - | 1519 | ` * Note on superglobals.` |
|       - | 1520 | ` *  According to the PHP language reference manual.` |
|       - | 1521 | ` *  Superglobals are built-in variables that are always available in all scopes.` |
|       - | 1522 | `*   Description` |
|       - | 1523 | `*   Several predefined variables in PHP are "superglobals", which means they` |
|       - | 1524 | `*   are available in all scopes throughout a script. There is no need to do` |
|       - | 1525 | `*   global $variable; to access them within functions or methods.` |
|       - | 1526 | `*   These superglobal variables are:` |
|       - | 1527 | `*    $GLOBALS` |
|       - | 1528 | `*    $_SERVER` |
|       - | 1529 | `*    $_GET` |
|       - | 1530 | `*    $_POST` |
|       - | 1531 | `*    $_FILES` |
|       - | 1532 | `*    $_COOKIE` |
|       - | 1533 | `*    $_SESSION` |
|       - | 1534 | `*    $_REQUEST` |
|       - | 1535 | `*    $_ENV` |
|       - | 1536 | `*/` |
|    3462 | 1537 | `PH7_PRIVATE sxi32 PH7_HashmapCreateSuper(ph7_vm *pVm)` |
|       5 | 1538 | `{` |
|       - | 1539 | `	static const char * azSuper[] = {` |
|       - | 1540 | `		"_SERVER",   /* $_SERVER */` |
|       - | 1541 | `		"_GET",      /* $_GET */` |
|       - | 1542 | `		"_POST",     /* $_POST */` |
|       - | 1543 | `		"_FILES",    /* $_FILES */` |
|       - | 1544 | `		"_COOKIE",   /* $_COOKIE */` |
|       - | 1545 | `		"_SESSION",  /* $_SESSION */` |
|       - | 1546 | `		"_REQUEST",  /* $_REQUEST */` |
|       - | 1547 | `		"_ENV",      /* $_ENV */` |
|       - | 1548 | `		"_HEADER",   /* $_HEADER */` |
|       - | 1549 | `		"argv"       /* $argv */` |
|       - | 1550 | `	};` |
|       - | 1551 | `	ph7_hashmap *pMap;` |
|       - | 1552 | `	ph7_value *pObj;` |
|       - | 1553 | `	SyString *pFile;` |
|       - | 1554 | `	sxi32 rc;` |
|       - | 1555 | `	sxu32 n;` |
|       - | 1556 | `	/* Allocate a new hashmap for the $GLOBALS array */` |
|    3467 | 1557 | `	pMap = PH7_NewHashmap(&(*pVm),0,0);` |
|    3467 | 1558 | `	if( pMap == 0 ){` |
|     ! 0 | 1559 | `		return SXERR_MEM;` |
|       - | 1560 | `	}` |
|    3467 | 1561 | `	pVm->pGlobal = pMap;` |
|       - | 1562 | `	/* Reserve a ph7_value for the $GLOBALS array*/` |
|    3467 | 1563 | `	pObj = PH7_ReserveMemObj(&(*pVm));` |
|    3467 | 1564 | `	if( pObj == 0 ){` |
|     ! 0 | 1565 | `		return SXERR_MEM;` |
|       - | 1566 | `	}` |
|    3467 | 1567 | `	PH7_MemObjInitFromArray(&(*pVm),pObj,pMap);` |
|       - | 1568 | `	/* Record object index */` |
|    3467 | 1569 | `	pVm->nGlobalIdx = pObj->nIdx;` |
|       - | 1570 | `	/* Install the special $GLOBALS array */` |
|    3467 | 1571 | `	rc = SyHashInsert(&pVm->hSuper,(const void *)"GLOBALS",sizeof("GLOBALS")-1,SX_INT_TO_PTR(pVm->nGlobalIdx));` |
|    3467 | 1572 | `	if( rc != SXRET_OK ){` |
|     ! 0 | 1573 | `		return rc;` |
|       - | 1574 | `	}` |
|       - | 1575 | `	/* Install superglobals now */` |
|   38087 | 1576 | `	for( n =  0 ; n < SX_ARRAYSIZE(azSuper)  ; n++ ){` |
|       - | 1577 | `		ph7_value *pSuper;` |
|       - | 1578 | `		/* Request an empty array */` |
|   34625 | 1579 | `		pSuper = ph7_new_array(&(*pVm));` |
|   34625 | 1580 | `		if( pSuper == 0 ){` |
|     ! 0 | 1581 | `			return SXERR_MEM;` |
|       - | 1582 | `		}` |
|       - | 1583 | `		/* Install */` |
|   34625 | 1584 | `		rc = ph7_vm_config(&(*pVm),PH7_VM_CONFIG_CREATE_SUPER,azSuper[n]/* Super-global name*/,pSuper/* Super-global value */);` |
|   34625 | 1585 | `		if( rc != SXRET_OK ){` |
|     ! 0 | 1586 | `			return rc;` |
|       - | 1587 | `		}` |
|       - | 1588 | `		/* Release the value now it have been installed */` |
|   34625 | 1589 | `		ph7_release_value(&(*pVm),pSuper);` |
|   17315 | 1590 | `	}` |
|       - | 1591 | `	/* Set some $_SERVER entries */` |
|    3467 | 1592 | `	pFile = (SyString *)SySetPeek(&pVm->aFiles);` |
|       - | 1593 | `	/*` |
|       - | 1594 | `	 * 'SCRIPT_FILENAME'` |
|       - | 1595 | `	 * The absolute pathname of the currently executing script.` |
|       - | 1596 | `	 */` |
|    6925 | 1597 | `	ph7_vm_config(pVm,PH7_VM_CONFIG_SERVER_ATTR,` |
|       - | 1598 | `		"SCRIPT_FILENAME",` |
|    1731 | 1599 | `		pFile ? pFile->zString : ":Memory:",` |
|    3458 | 1600 | `		pFile ? pFile->nByte : sizeof(":Memory:") - 1` |
|       - | 1601 | `		);` |
|       - | 1602 | `	/* All done,all super-global are installed now */` |
|    3467 | 1603 | `	return SXRET_OK;` |
|    1736 | 1604 | `}` |
|       - | 1605 | `/*` |
|       - | 1606 | ` * Release a hashmap.` |
|       - | 1607 | ` */` |
|   72522 | 1608 | `PH7_PRIVATE sxi32 PH7_HashmapRelease(ph7_hashmap *pMap,int FreeDS)` |
|       5 | 1609 | `{` |
|       - | 1610 | `	ph7_hashmap_node *pEntry,*pNext;` |
|   72527 | 1611 | `	ph7_vm *pVm = pMap->pVm;` |
|       - | 1612 | `	sxu32 n;` |
|   72527 | 1613 | `	if( pMap == pVm->pGlobal ){` |
|       - | 1614 | `		/* Cannot delete the $GLOBALS array */` |
|     ! 0 | 1615 | `		PH7_VmThrowError(pMap->pVm,0,PH7_CTX_NOTICE,"$GLOBALS is a read-only array,deletion is forbidden");` |
|     ! 0 | 1616 | `		return SXRET_OK;` |
|       - | 1617 | `	}` |
|       - | 1618 | `	/* Start the release process */` |
|   72527 | 1619 | `	n = 0;` |
|   72527 | 1620 | `	pEntry = pMap->pFirst;` |
| 1607310 | 1621 | `	for(;;){` |
| 3214625 | 1622 | `		if( n >= pMap->nEntry ){` |
|   72527 | 1623 | `			break;` |
|       - | 1624 | `		}` |
| 3142103 | 1625 | `		pNext = pEntry->pPrev; /* Reverse link */` |
|       - | 1626 | `		/* Remove the reference from the foreign table */` |
| 3142103 | 1627 | `		PH7_VmRefObjRemove(pVm,pEntry->nValIdx,0,pEntry);` |
| 3142103 | 1628 | `		if( (pEntry->iFlags & HASHMAP_NODE_FOREIGN_OBJ) == 0 ){` |
|       - | 1629 | `			/* Restore the ph7_value to the free list */` |
| 3142093 | 1630 | `			PH7_VmUnsetMemObj(pVm,pEntry->nValIdx,FALSE);` |
| 1571044 | 1631 | `		}` |
|       - | 1632 | `		/* Release the node */` |
| 3142103 | 1633 | `		if( pEntry->iType == HASHMAP_BLOB_NODE ){` |
|   79731 | 1634 | `			SyBlobRelease(&pEntry->xKey.sKey);` |
|   39863 | 1635 | `		}` |
| 3142103 | 1636 | `		SyMemBackendPoolFree(&pVm->sAllocator,pEntry);` |
|       - | 1637 | `		/* Point to the next entry */` |
| 3142103 | 1638 | `		pEntry = pNext;` |
| 3142103 | 1639 | `		n++;` |
|       5 | 1640 | `	}` |
|   72527 | 1641 | `	if( pMap->nEntry > 0 ){` |
|       - | 1642 | `		/* Release the hash bucket */` |
|   59193 | 1643 | `		SyMemBackendFree(&pVm->sAllocator,pMap->apBucket);` |
|   29594 | 1644 | `	}` |
|   72527 | 1645 | `	if( FreeDS ){` |
|       - | 1646 | `		/* Free the whole instance */` |
|   72511 | 1647 | `		SyMemBackendPoolFree(&pVm->sAllocator,pMap);` |
|   36258 | 1648 | `	}else{` |
|       - | 1649 | `		/* Keep the instance but reset it's fields */` |
|      17 | 1650 | `		pMap->apBucket = 0;` |
|      17 | 1651 | `		pMap->iNextIdx = 0;` |
|      17 | 1652 | `		pMap->nEntry = pMap->nSize = 0;` |
|      17 | 1653 | `		pMap->pFirst = pMap->pLast = pMap->pCur = 0;` |
|       - | 1654 | `	}` |
|   72527 | 1655 | `	return SXRET_OK;` |
|   36266 | 1656 | `}` |
|       - | 1657 | `/*` |
|       - | 1658 | ` * Decrement the reference count of a given hashmap.` |
|       - | 1659 | ` * If the count reaches zero which mean no more variables` |
|       - | 1660 | ` * are pointing to this hashmap,then release the whole instance.` |
|       - | 1661 | ` */` |
|  725922 | 1662 | `PH7_PRIVATE void  PH7_HashmapUnref(ph7_hashmap *pMap)` |
|       5 | 1663 | `{` |
|  725927 | 1664 | `	ph7_vm *pVm = pMap->pVm;` |
|       - | 1665 | `	/* TICKET 1432-49: $GLOBALS is not subject to garbage collection */` |
|  725927 | 1666 | `	pMap->iRef--;` |
|  725927 | 1667 | `	if( pMap->iRef < 1 && pMap != pVm->pGlobal){` |
|   72491 | 1668 | `		PH7_HashmapRelease(pMap,TRUE);` |
|   36243 | 1669 | `	}` |
|  725927 | 1670 | `}` |
|       - | 1671 | `/*` |
|       - | 1672 | ` * Check if a given key exists in the given hashmap.` |
|       - | 1673 | ` * Write a pointer to the target node on success.` |
|       - | 1674 | ` * Otherwise SXERR_NOTFOUND is returned on failure.` |
|       - | 1675 | ` */` |
|  126142 | 1676 | `PH7_PRIVATE sxi32 PH7_HashmapLookup(` |
|       - | 1677 | `	ph7_hashmap *pMap,        /* Target hashmap */` |
|       - | 1678 | `	ph7_value *pKey,          /* Lookup key */` |
|       - | 1679 | `	ph7_hashmap_node **ppNode /* OUT: Target node on success */` |
|       - | 1680 | `	)` |
|       5 | 1681 | `{` |
|       - | 1682 | `	sxi32 rc;` |
|  126147 | 1683 | `	if( pMap->nEntry < 1 ){` |
|       - | 1684 | `		/* TICKET 1433-25: Don't bother hashing,the hashmap is empty anyway.` |
|       - | 1685 | `		 */` |
|      64 | 1686 | `		return SXERR_NOTFOUND;` |
|       - | 1687 | `	}` |
|  126087 | 1688 | `	rc = HashmapLookup(&(*pMap),&(*pKey),ppNode);` |
|  126087 | 1689 | `	return rc;` |
|   63076 | 1690 | `}` |
|       - | 1691 | `/*` |
|       - | 1692 | ` * Insert a given key and it's associated value (if any) in the given` |
|       - | 1693 | ` * hashmap.` |
|       - | 1694 | ` * If a node with the given key already exists in the database` |
|       - | 1695 | ` * then this function overwrite the old value.` |
|       - | 1696 | ` */` |
| 2562626 | 1697 | `PH7_PRIVATE sxi32 PH7_HashmapInsert(` |
|       - | 1698 | `	ph7_hashmap *pMap, /* Target hashmap */` |
|       - | 1699 | `	ph7_value *pKey,   /* Lookup key */` |
|       - | 1700 | `	ph7_value *pVal    /* Node value.NULL otherwise */` |
|       - | 1701 | `	)` |
|       5 | 1702 | `{` |
|       - | 1703 | `	sxi32 rc;` |
| 2562631 | 1704 | `	if( pVal && (pVal->iFlags & MEMOBJ_HASHMAP) && (ph7_hashmap *)pVal->x.pOther == pMap->pVm->pGlobal ){` |
|       - | 1705 | `		/*` |
|       - | 1706 | `		 * TICKET 1433-35: Insertion in the $GLOBALS array is forbidden.` |
|       - | 1707 | `		 */` |
|     ! 0 | 1708 | `		PH7_VmThrowError(pMap->pVm,0,PH7_CTX_ERR,"$GLOBALS is a read-only array,insertion is forbidden");` |
|     ! 0 | 1709 | `		return SXRET_OK;` |
|       - | 1710 | `	}` |
| 2562631 | 1711 | `	rc = HashmapInsert(&(*pMap),&(*pKey),&(*pVal));` |
| 2562631 | 1712 | `	return rc;` |
| 1281318 | 1713 | `}` |
|       - | 1714 | `/*` |
|       - | 1715 | ` * Merge entries of pSrc into pDest using PHP merge semantics:` |
|       - | 1716 | ` *   - String keys overwrite same-key entries in pDest.` |
|       - | 1717 | ` *   - Integer keys are renumbered with the destination's auto-index.` |
|       - | 1718 | ` * This is the same routine that backs array_merge().` |
|       - | 1719 | ` */` |
|      52 | 1720 | `PH7_PRIVATE sxi32 PH7_HashmapMerge(ph7_hashmap *pSrc,ph7_hashmap *pDest)` |
|       1 | 1721 | `{` |
|      53 | 1722 | `	return HashmapMerge(&(*pSrc),&(*pDest));` |
|       1 | 1723 | `}` |
|       - | 1724 | `/*` |
|       - | 1725 | ` * Insert a given key and it's associated value (foreign index) in the given` |
|       - | 1726 | ` * hashmap.` |
|       - | 1727 | ` * This is insertion by reference so be careful to mark the node` |
|       - | 1728 | ` * with the HASHMAP_NODE_FOREIGN_OBJ flag being set.` |
|       - | 1729 | ` * The insertion by reference is triggered when the following` |
|       - | 1730 | ` * expression is encountered.` |
|       - | 1731 | ` * $var = 10;` |
|       - | 1732 | ` *  $a = array(&var);` |
|       - | 1733 | ` * OR` |
|       - | 1734 | ` *  $a[] =& $var;` |
|       - | 1735 | ` * That is,$var is a foreign ph7_value and the $a array have no control` |
|       - | 1736 | ` * over it's contents.` |
|       - | 1737 | ` * Note that the node that hold the foreign ph7_value is automatically` |
|       - | 1738 | ` * removed when the foreign ph7_value is unset.` |
|       - | 1739 | ` * Example:` |
|       - | 1740 | ` *  $var = 10;` |
|       - | 1741 | ` *  $a[] =& $var;` |
|       - | 1742 | ` *  echo count($a).PHP_EOL; //1` |
|       - | 1743 | ` *  //Unset the foreign ph7_value now` |
|       - | 1744 | ` *  unset($var);` |
|       - | 1745 | ` *  echo count($a); //0` |
|       - | 1746 | ` * Note that this is a PH7 eXtension.` |
|       - | 1747 | ` * Refer to the official documentation for more information.` |
|       - | 1748 | ` * If a node with the given key already exists in the database` |
|       - | 1749 | ` * then this function overwrite the old value.` |
|       - | 1750 | ` */` |
|   45970 | 1751 | `PH7_PRIVATE sxi32 PH7_HashmapInsertByRef(` |
|       - | 1752 | `	ph7_hashmap *pMap, /* Target hashmap */` |
|       - | 1753 | `	ph7_value *pKey,   /* Lookup key */` |
|       - | 1754 | `	sxu32 nRefIdx      /* Foreign ph7_value index */` |
|       - | 1755 | `	)` |
|       5 | 1756 | `{` |
|       - | 1757 | `	sxi32 rc;` |
|   45975 | 1758 | `	if( nRefIdx == pMap->pVm->nGlobalIdx ){` |
|       - | 1759 | `		/*` |
|       - | 1760 | `		 * TICKET 1433-35: Insertion in the $GLOBALS array is forbidden.` |
|       - | 1761 | `		 */` |
|     ! 0 | 1762 | `		PH7_VmThrowError(pMap->pVm,0,PH7_CTX_ERR,"$GLOBALS is a read-only array,insertion is forbidden");` |
|     ! 0 | 1763 | `		return SXRET_OK;` |
|       - | 1764 | `	}` |
|   45975 | 1765 | `	rc = HashmapInsertByRef(&(*pMap),&(*pKey),nRefIdx);` |
|   45975 | 1766 | `	return rc;` |
|   22990 | 1767 | `}` |
|       - | 1768 | `/*` |
|       - | 1769 | ` * Reset the node cursor of a given hashmap.` |
|       - | 1770 | ` */` |
|   35038 | 1771 | `PH7_PRIVATE void PH7_HashmapResetLoopCursor(ph7_hashmap *pMap)` |
|       5 | 1772 | `{` |
|       - | 1773 | `	/* Reset the loop cursor */` |
|   35043 | 1774 | `	pMap->pCur = pMap->pFirst;` |
|   35043 | 1775 | `}` |
|       - | 1776 | `/*` |
|       - | 1777 | ` * Return a pointer to the node currently pointed by the node cursor.` |
|       - | 1778 | ` * If the cursor reaches the end of the list,then this function` |
|       - | 1779 | ` * return NULL.` |
|       - | 1780 | ` * Note that the node cursor is automatically advanced by this function.` |
|       - | 1781 | ` */` |
|  230904 | 1782 | `PH7_PRIVATE ph7_hashmap_node * PH7_HashmapGetNextEntry(ph7_hashmap *pMap)` |
|       5 | 1783 | `{` |
|  230909 | 1784 | `	ph7_hashmap_node *pCur = pMap->pCur;` |
|  230909 | 1785 | `	if( pCur == 0 ){` |
|       - | 1786 | `		/* End of the list,return null */` |
|   17543 | 1787 | `		return 0;` |
|       - | 1788 | `	}` |
|       - | 1789 | `	/* Advance the node cursor */` |
|  213371 | 1790 | `	pMap->pCur = pCur->pPrev; /* Reverse link */` |
|  213371 | 1791 | `	return pCur;` |
|  115457 | 1792 | `}` |
|       - | 1793 | `/*` |
|       - | 1794 | ` * Extract a node value.` |
|       - | 1795 | ` */` |
|  539578 | 1796 | `PH7_PRIVATE void PH7_HashmapExtractNodeValue(ph7_hashmap_node *pNode,ph7_value *pValue,int bStore)` |
|       5 | 1797 | `{` |
|  539583 | 1798 | `	ph7_value *pEntry = HashmapExtractNodeValue(pNode);` |
|  539583 | 1799 | `	if( pEntry ){` |
|  539583 | 1800 | `		if( bStore ){` |
|  213751 | 1801 | `			PH7_MemObjStore(pEntry,pValue);` |
|  106878 | 1802 | `		}else{` |
|  325837 | 1803 | `			PH7_MemObjLoad(pEntry,pValue);` |
|       - | 1804 | `		}` |
|  269822 | 1805 | `	}else{` |
|     ! 0 | 1806 | `		PH7_MemObjRelease(pValue);` |
|       - | 1807 | `	}` |
|  539583 | 1808 | `}` |
|       - | 1809 | `/*` |
|       - | 1810 | ` * Extract a node key.` |
|       - | 1811 | ` */` |
|  139750 | 1812 | `PH7_PRIVATE void PH7_HashmapExtractNodeKey(ph7_hashmap_node *pNode,ph7_value *pKey)` |
|       5 | 1813 | `{` |
|       - | 1814 | `	/* Fill with the current key */` |
|  139755 | 1815 | `	if( pNode->iType == HASHMAP_INT_NODE ){` |
|  135455 | 1816 | `		if( SyBlobLength(&pKey->sBlob) > 0 ){` |
|      27 | 1817 | `			SyBlobRelease(&pKey->sBlob);` |
|      13 | 1818 | `		}` |
|  135455 | 1819 | `		pKey->x.iVal = pNode->xKey.iKey;` |
|  135455 | 1820 | `		MemObjSetType(pKey,MEMOBJ_INT);` |
|   67730 | 1821 | `	}else{` |
|    4305 | 1822 | `		SyBlobReset(&pKey->sBlob);` |
|    4305 | 1823 | `		SyBlobAppend(&pKey->sBlob,SyBlobData(&pNode->xKey.sKey),SyBlobLength(&pNode->xKey.sKey));` |
|    4305 | 1824 | `		MemObjSetType(pKey,MEMOBJ_STRING);` |
|       - | 1825 | `	}` |
|  139755 | 1826 | `}` |
|       - | 1827 | `#ifndef PH7_DISABLE_DISK_IO` |
|       - | 1828 | `/*` |
|       - | 1829 | ` * Store the address of nodes value in the given container.` |
|       - | 1830 | ` * Refer to the [vfprintf(),vprintf(),vsprintf()] implementations` |
|       - | 1831 | ` * defined in 'builtin.c' for more information.` |
|       - | 1832 | ` */` |
|      10 | 1833 | `PH7_PRIVATE int PH7_HashmapValuesToSet(ph7_hashmap *pMap,SySet *pOut)` |
|       1 | 1834 | `{` |
|      11 | 1835 | `	ph7_hashmap_node *pEntry = pMap->pFirst;` |
|       - | 1836 | `	ph7_value *pValue;` |
|       - | 1837 | `	sxu32 n;` |
|       - | 1838 | `	/* Initialize the container */` |
|      11 | 1839 | `	SySetInit(pOut,&pMap->pVm->sAllocator,sizeof(ph7_value *));` |
|      27 | 1840 | `	for(n = 0 ; n < pMap->nEntry ; n++ ){` |
|       - | 1841 | `		/* Extract node value */` |
|      17 | 1842 | `		pValue = HashmapExtractNodeValue(pEntry);` |
|      17 | 1843 | `		if( pValue ){` |
|      17 | 1844 | `			SySetPut(pOut,(const void *)&pValue);` |
|       8 | 1845 | `		}` |
|       - | 1846 | `		/* Point to the next entry */` |
|      17 | 1847 | `		pEntry = pEntry->pPrev; /* Reverse link */` |
|       9 | 1848 | `	}` |
|       - | 1849 | `	/* Total inserted entries */` |
|      11 | 1850 | `	return (int)SySetUsed(pOut);` |
|       1 | 1851 | `}` |
|       - | 1852 | `#endif /* PH7_DISABLE_BUILTIN_FUNC \|\| PH7_DISABLE_DISK_IO */` |
|       - | 1853 | `/* SPDX-SnippetBegin */` |
|       - | 1854 | `/* SPDX-SnippetCopyrightText: D. Richard Hipp and the SQLite authors <https://sqlite.org/> */` |
|       - | 1855 | `/* SPDX-License-Identifier: blessing */` |
|       - | 1856 | `/*` |
|       - | 1857 | ` * Merge sort.` |
|       - | 1858 | ` * The merge sort implementation is based on the one found in the SQLite3 source tree.` |
|       - | 1859 | ` * Status: Public domain` |
|       - | 1860 | ` */` |
|       - | 1861 | `/* Node comparison callback signature */` |
|       - | 1862 | `typedef sxi32 (*ProcNodeCmp)(ph7_hashmap_node *,ph7_hashmap_node *,void *);` |
|       - | 1863 | `/*` |
|       - | 1864 | `** Inputs:` |
|       - | 1865 | `**   a:       A sorted, null-terminated linked list.  (May be null).` |
|       - | 1866 | `**   b:       A sorted, null-terminated linked list.  (May be null).` |
|       - | 1867 | `**   cmp:     A pointer to the comparison function.` |
|       - | 1868 | `**` |
|       - | 1869 | `** Return Value:` |
|       - | 1870 | `**   A pointer to the head of a sorted list containing the elements` |
|       - | 1871 | `**   of both a and b.` |
|       - | 1872 | `**` |
|       - | 1873 | `** Side effects:` |
|       - | 1874 | `**   The "next","prev" pointers for elements in the lists a and b are` |
|       - | 1875 | `**   changed.` |
|       - | 1876 | `*/` |
|   33392 | 1877 | `static ph7_hashmap_node * HashmapNodeMerge(ph7_hashmap_node *pA,ph7_hashmap_node *pB,ProcNodeCmp xCmp,void *pCmpData)` |
|       5 | 1878 | `{` |
|       - | 1879 | `	ph7_hashmap_node result,*pTail;` |
|       - | 1880 | `    /* Prevent compiler warning */` |
|   33397 | 1881 | `	result.pNext = result.pPrev = 0;` |
|   33397 | 1882 | `	pTail = &result;` |
|  102294 | 1883 | `	while( pA && pB ){` |
|   68902 | 1884 | `		if( xCmp(pA,pB,pCmpData) <= 0 ){` |
|   45544 | 1885 | `			pTail->pPrev = pA;` |
|   45544 | 1886 | `			pA->pNext = pTail;` |
|   45544 | 1887 | `			pTail = pA;` |
|   45544 | 1888 | `			pA = pA->pPrev;` |
|   22763 | 1889 | `		}else{` |
|   23363 | 1890 | `			pTail->pPrev = pB;` |
|   23363 | 1891 | `			pB->pNext = pTail;` |
|   23363 | 1892 | `			pTail = pB;` |
|   23363 | 1893 | `			pB = pB->pPrev;` |
|       - | 1894 | `		}` |
|       5 | 1895 | `	}` |
|   33397 | 1896 | `	if( pA ){` |
|   23412 | 1897 | `		pTail->pPrev = pA;` |
|   23412 | 1898 | `		pA->pNext = pTail;` |
|   21706 | 1899 | `	}else if( pB ){` |
|    9774 | 1900 | `		pTail->pPrev = pB;` |
|    9774 | 1901 | `		pB->pNext = pTail;` |
|    4877 | 1902 | `	}else{` |
|     221 | 1903 | `		pTail->pPrev = pTail->pNext = 0;` |
|       - | 1904 | `	}` |
|   33397 | 1905 | `	return result.pPrev;` |
|       5 | 1906 | `}` |
|       - | 1907 | `/*` |
|       - | 1908 | `** Inputs:` |
|       - | 1909 | `**   Map:       Input hashmap` |
|       - | 1910 | `**   cmp:       A comparison function.` |
|       - | 1911 | `**` |
|       - | 1912 | `** Return Value:` |
|       - | 1913 | `**   Sorted hashmap.` |
|       - | 1914 | `**` |
|       - | 1915 | `** Side effects:` |
|       - | 1916 | `**   The "next" pointers for elements in list are changed.` |
|       - | 1917 | `*/` |
|       - | 1918 | `#define N_SORT_BUCKET  32` |
|     688 | 1919 | `static sxi32 HashmapMergeSort(ph7_hashmap *pMap,ProcNodeCmp xCmp,void *pCmpData)` |
|       5 | 1920 | `{` |
|       - | 1921 | `	ph7_hashmap_node *a[N_SORT_BUCKET], *p,*pIn;` |
|       - | 1922 | `	sxu32 i;` |
|     693 | 1923 | `	SyZero(a,sizeof(a));` |
|       - | 1924 | `	/* Point to the first inserted entry */` |
|     693 | 1925 | `	pIn = pMap->pFirst;` |
|   13977 | 1926 | `	while( pIn ){` |
|   13289 | 1927 | `		p = pIn;` |
|   13289 | 1928 | `		pIn = p->pPrev;` |
|   13289 | 1929 | `		p->pPrev = 0;` |
|   25353 | 1930 | `		for(i=0; i<N_SORT_BUCKET-1; i++){` |
|   25353 | 1931 | `			if( a[i]==0 ){` |
|   13289 | 1932 | `				a[i] = p;` |
|   13289 | 1933 | `				break;` |
|     ! 0 | 1934 | `			}else{` |
|   12069 | 1935 | `				p = HashmapNodeMerge(a[i],p,xCmp,pCmpData);` |
|   12069 | 1936 | `				a[i] = 0;` |
|       - | 1937 | `			}` |
|    6037 | 1938 | `		}` |
|   13289 | 1939 | `		if( i==N_SORT_BUCKET-1 ){` |
|       - | 1940 | `			/* To get here, there need to be 2^(N_SORT_BUCKET) elements in he input list.` |
|       - | 1941 | `			 * But that is impossible.` |
|       - | 1942 | `			 */` |
|     ! 0 | 1943 | `			a[i] = HashmapNodeMerge(a[i], p,xCmp,pCmpData);` |
|     ! 0 | 1944 | `		}` |
|       5 | 1945 | `	}` |
|     693 | 1946 | `	p = a[0];` |
|   22021 | 1947 | `	for(i=1; i<N_SORT_BUCKET; i++){` |
|   21333 | 1948 | `		p = HashmapNodeMerge(p,a[i],xCmp,pCmpData);` |
|   10669 | 1949 | `	}` |
|     693 | 1950 | `	p->pNext = 0;` |
|       - | 1951 | `	/* Reflect the change */` |
|     693 | 1952 | `	pMap->pFirst = p;` |
|       - | 1953 | `	/* Reset the loop cursor */` |
|     693 | 1954 | `	pMap->pCur = pMap->pFirst;` |
|     693 | 1955 | `	return SXRET_OK;` |
|       5 | 1956 | `}` |
|       - | 1957 | `/* SPDX-SnippetEnd */` |
|       - | 1958 | `/*` |
|       - | 1959 | ` * Node comparison callback.` |
|       - | 1960 | ` * used-by: [sort(),asort(),...]` |
|       - | 1961 | ` */` |
|   68692 | 1962 | `static sxi32 HashmapCmpCallback1(ph7_hashmap_node *pA,ph7_hashmap_node *pB,void *pCmpData)` |
|       5 | 1963 | `{` |
|       - | 1964 | `	ph7_value sA,sB;` |
|       - | 1965 | `	sxi32 iFlags;` |
|       - | 1966 | `	int rc;` |
|   68697 | 1967 | `	if( pCmpData == 0 ){` |
|       - | 1968 | `		/* Perform a standard comparison */` |
|   68673 | 1969 | `		rc = HashmapNodeCmp(pA,pB,FALSE);` |
|   68673 | 1970 | `		return rc;` |
|       - | 1971 | `	}` |
|      25 | 1972 | `	iFlags = SX_PTR_TO_INT(pCmpData);` |
|       - | 1973 | `	/* Duplicate node values */` |
|      25 | 1974 | `	PH7_MemObjInit(pA->pMap->pVm,&sA);` |
|      25 | 1975 | `	PH7_MemObjInit(pA->pMap->pVm,&sB);` |
|      25 | 1976 | `	PH7_HashmapExtractNodeValue(pA,&sA,FALSE);` |
|      25 | 1977 | `	PH7_HashmapExtractNodeValue(pB,&sB,FALSE);` |
|      25 | 1978 | `	if( iFlags == 5 ){` |
|       - | 1979 | `		/* String cast */` |
|       - | 1980 | `		const char *zA,*zB;` |
|       - | 1981 | `		sxu32 nA,nB,nMin;` |
|      15 | 1982 | `		if( (sA.iFlags & MEMOBJ_STRING) == 0 ){` |
|     ! 0 | 1983 | `			PH7_MemObjToString(&sA);` |
|     ! 0 | 1984 | `		}` |
|      15 | 1985 | `		if( (sB.iFlags & MEMOBJ_STRING) == 0 ){` |
|     ! 0 | 1986 | `			PH7_MemObjToString(&sB);` |
|     ! 0 | 1987 | `		}` |
|       - | 1988 | `		/* Lexicographic string comparison to avoid numeric string coercion */` |
|      15 | 1989 | `		zA = (const char *)SyBlobData(&sA.sBlob);` |
|      15 | 1990 | `		zB = (const char *)SyBlobData(&sB.sBlob);` |
|      15 | 1991 | `		nA = SyBlobLength(&sA.sBlob);` |
|      15 | 1992 | `		nB = SyBlobLength(&sB.sBlob);` |
|      15 | 1993 | `		nMin = nA < nB ? nA : nB;` |
|      15 | 1994 | `		rc = SyMemcmp(zA,zB,nMin);` |
|      15 | 1995 | `		if( rc == 0 ){` |
|       5 | 1996 | `			if( nA < nB ) rc = -1;` |
|     ! 0 | 1997 | `			else if( nA > nB ) rc = 1;` |
|       2 | 1998 | `		}` |
|       8 | 1999 | `	}else{` |
|       - | 2000 | `		/* Numeric cast */` |
|      11 | 2001 | `		PH7_MemObjToNumeric(&sA);` |
|      11 | 2002 | `		PH7_MemObjToNumeric(&sB);` |
|      11 | 2003 | `		rc = PH7_MemObjCmp(&sA,&sB,FALSE,0);` |
|       - | 2004 | `	}` |
|      25 | 2005 | `	PH7_MemObjRelease(&sA);` |
|      25 | 2006 | `	PH7_MemObjRelease(&sB);` |
|      25 | 2007 | `	return rc;` |
|   34365 | 2008 | `}` |
|       - | 2009 | `/*` |
|       - | 2010 | ` * Node comparison callback: Compare nodes by keys only.` |
|       - | 2011 | ` * used-by: [ksort()]` |
|       - | 2012 | ` */` |
|      14 | 2013 | `static sxi32 HashmapCmpCallback2(ph7_hashmap_node *pA,ph7_hashmap_node *pB,void *pCmpData)` |
|       1 | 2014 | `{` |
|       - | 2015 | `	sxi32 rc;` |
|       7 | 2016 | `	SXUNUSED(pCmpData); /* cc warning */` |
|      15 | 2017 | `	if( pA->iType == HASHMAP_BLOB_NODE && pB->iType == HASHMAP_BLOB_NODE ){` |
|       - | 2018 | `		/* Perform a string comparison */` |
|       5 | 2019 | `		rc = SyBlobCmp(&pA->xKey.sKey,&pB->xKey.sKey);` |
|       3 | 2020 | `	}else{` |
|       - | 2021 | `		SyString sStr;` |
|       - | 2022 | `		sxi64 iA,iB;` |
|       - | 2023 | `		/* Perform a numeric comparison */` |
|      11 | 2024 | `		if( pA->iType == HASHMAP_BLOB_NODE ){` |
|       - | 2025 | `			/* Cast to 64-bit integer */` |
|     ! 0 | 2026 | `			SyStringInitFromBuf(&sStr,SyBlobData(&pA->xKey.sKey),SyBlobLength(&pA->xKey.sKey));` |
|     ! 0 | 2027 | `			if( sStr.nByte < 1 ){` |
|     ! 0 | 2028 | `				iA = 0;` |
|     ! 0 | 2029 | `			}else{` |
|     ! 0 | 2030 | `				SyStrToInt64(sStr.zString,sStr.nByte,(void *)&iA,0);` |
|       - | 2031 | `			}` |
|     ! 0 | 2032 | `		}else{` |
|      11 | 2033 | `			iA = pA->xKey.iKey;` |
|       - | 2034 | `		}` |
|      11 | 2035 | `		if( pB->iType == HASHMAP_BLOB_NODE ){` |
|       - | 2036 | `			/* Cast to 64-bit integer */` |
|     ! 0 | 2037 | `			SyStringInitFromBuf(&sStr,SyBlobData(&pB->xKey.sKey),SyBlobLength(&pB->xKey.sKey));` |
|     ! 0 | 2038 | `			if( sStr.nByte < 1 ){` |
|     ! 0 | 2039 | `				iB = 0;` |
|     ! 0 | 2040 | `			}else{` |
|     ! 0 | 2041 | `				SyStrToInt64(sStr.zString,sStr.nByte,(void *)&iB,0);` |
|       - | 2042 | `			}` |
|     ! 0 | 2043 | `		}else{` |
|      11 | 2044 | `			iB = pB->xKey.iKey;` |
|       - | 2045 | `		}` |
|      11 | 2046 | `		rc = (sxi32)(iA-iB);` |
|       - | 2047 | `	}` |
|       - | 2048 | `	/* Comparison result */` |
|      15 | 2049 | `	return rc;` |
|       1 | 2050 | `}` |
|       - | 2051 | `/*` |
|       - | 2052 | ` * Node comparison callback.` |
|       - | 2053 | ` * Used by: [rsort(),arsort()];` |
|       - | 2054 | ` */` |
|      78 | 2055 | `static sxi32 HashmapCmpCallback3(ph7_hashmap_node *pA,ph7_hashmap_node *pB,void *pCmpData)` |
|       1 | 2056 | `{` |
|       - | 2057 | `	ph7_value sA,sB;` |
|       - | 2058 | `	sxi32 iFlags;` |
|       - | 2059 | `	int rc;` |
|      79 | 2060 | `	if( pCmpData == 0 ){` |
|       - | 2061 | `		/* Perform a standard comparison */` |
|      59 | 2062 | `		rc = HashmapNodeCmp(pA,pB,FALSE);` |
|      59 | 2063 | `		return -rc;` |
|       - | 2064 | `	}` |
|      21 | 2065 | `	iFlags = SX_PTR_TO_INT(pCmpData);` |
|       - | 2066 | `	/* Duplicate node values */` |
|      21 | 2067 | `	PH7_MemObjInit(pA->pMap->pVm,&sA);` |
|      21 | 2068 | `	PH7_MemObjInit(pA->pMap->pVm,&sB);` |
|      21 | 2069 | `	PH7_HashmapExtractNodeValue(pA,&sA,FALSE);` |
|      21 | 2070 | `	PH7_HashmapExtractNodeValue(pB,&sB,FALSE);` |
|      21 | 2071 | `	if( iFlags == 5 ){` |
|       - | 2072 | `		/* String cast */` |
|       - | 2073 | `		const char *zA,*zB;` |
|       - | 2074 | `		sxu32 nA,nB,nMin;` |
|      11 | 2075 | `		if( (sA.iFlags & MEMOBJ_STRING) == 0 ){` |
|     ! 0 | 2076 | `			PH7_MemObjToString(&sA);` |
|     ! 0 | 2077 | `		}` |
|      11 | 2078 | `		if( (sB.iFlags & MEMOBJ_STRING) == 0 ){` |
|     ! 0 | 2079 | `			PH7_MemObjToString(&sB);` |
|     ! 0 | 2080 | `		}` |
|       - | 2081 | `		/* Lexicographic string comparison to avoid numeric string coercion */` |
|      11 | 2082 | `		zA = (const char *)SyBlobData(&sA.sBlob);` |
|      11 | 2083 | `		zB = (const char *)SyBlobData(&sB.sBlob);` |
|      11 | 2084 | `		nA = SyBlobLength(&sA.sBlob);` |
|      11 | 2085 | `		nB = SyBlobLength(&sB.sBlob);` |
|      11 | 2086 | `		nMin = nA < nB ? nA : nB;` |
|      11 | 2087 | `		rc = SyMemcmp(zA,zB,nMin);` |
|      11 | 2088 | `		if( rc == 0 ){` |
|       3 | 2089 | `			if( nA < nB ) rc = -1;` |
|     ! 0 | 2090 | `			else if( nA > nB ) rc = 1;` |
|       1 | 2091 | `		}` |
|       6 | 2092 | `	}else{` |
|       - | 2093 | `		/* Numeric cast */` |
|      11 | 2094 | `		PH7_MemObjToNumeric(&sA);` |
|      11 | 2095 | `		PH7_MemObjToNumeric(&sB);` |
|      11 | 2096 | `		rc = PH7_MemObjCmp(&sA,&sB,FALSE,0);` |
|       - | 2097 | `	}` |
|      21 | 2098 | `	PH7_MemObjRelease(&sA);` |
|      21 | 2099 | `	PH7_MemObjRelease(&sB);` |
|      21 | 2100 | `	return -rc;` |
|      40 | 2101 | `}` |
|       - | 2102 | `/*` |
|       - | 2103 | ` * Node comparison callback: Invoke an user-defined callback for the purpose of node comparison.` |
|       - | 2104 | ` * used-by: [usort(),uasort()]` |
|       - | 2105 | ` */` |
|      88 | 2106 | `static sxi32 HashmapCmpCallback4(ph7_hashmap_node *pA,ph7_hashmap_node *pB,void *pCmpData)` |
|       3 | 2107 | `{` |
|       - | 2108 | `	ph7_value sResult,*pCallback;` |
|       - | 2109 | `	ph7_value *pV1,*pV2;` |
|       - | 2110 | `	ph7_value *apArg[2];  /* Callback arguments */` |
|       - | 2111 | `	sxi32 rc;` |
|       - | 2112 | `	/* Point to the desired callback */` |
|      91 | 2113 | `	pCallback = (ph7_value *)pCmpData;` |
|      91 | 2114 | `	if( pA->pMap->pVm->iCmpCallbackExc ){` |
|       - | 2115 | `		/* A previous comparison already raised: stop invoking the callback so` |
|       - | 2116 | `		 * the exception is not thrown again, and let the sort wind down. */` |
|       6 | 2117 | `		return 0;` |
|       - | 2118 | `	}` |
|       - | 2119 | `	/* initialize the result value */` |
|      87 | 2120 | `	PH7_MemObjInit(pA->pMap->pVm,&sResult);` |
|       - | 2121 | `	/* Extract nodes values */` |
|      87 | 2122 | `	pV1 = HashmapExtractNodeValue(pA);` |
|      87 | 2123 | `	pV2 = HashmapExtractNodeValue(pB);` |
|      87 | 2124 | `	apArg[0] = pV1;` |
|      87 | 2125 | `	apArg[1] = pV2;` |
|       - | 2126 | `	/* Invoke the callback */` |
|      87 | 2127 | `	rc = PH7_VmCallUserFunction(pA->pMap->pVm,pCallback,2,apArg,&sResult);` |
|      87 | 2128 | `	if( rc == PH7_EXCEPTION ){` |
|       - | 2129 | `		/* The comparator raised: flag it so the sort driver aborts and` |
|       - | 2130 | `		 * propagates, and order this pair arbitrarily for the rest of the run. */` |
|       6 | 2131 | `		pA->pMap->pVm->iCmpCallbackExc = 1;` |
|       6 | 2132 | `		rc = 0;` |
|      84 | 2133 | `	}else if( rc != SXRET_OK ){` |
|       - | 2134 | `		/* An error occured while calling user defined function [i.e: not defined] */` |
|     ! 0 | 2135 | `		rc = -1; /* Set a dummy result */` |
|     ! 0 | 2136 | `	}else{` |
|       - | 2137 | `		/* Extract callback result */` |
|      82 | 2138 | `		if((sResult.iFlags & MEMOBJ_INT) == 0 ){` |
|       - | 2139 | `			/* Perform an int cast */` |
|     ! 0 | 2140 | `			PH7_MemObjToInteger(&sResult);` |
|     ! 0 | 2141 | `		}` |
|      82 | 2142 | `		rc = (sxi32)sResult.x.iVal;` |
|       - | 2143 | `	}` |
|      87 | 2144 | `	PH7_MemObjRelease(&sResult);` |
|       - | 2145 | `	/* Callback result */` |
|      87 | 2146 | `	return rc;` |
|      47 | 2147 | `}` |
|       - | 2148 | `/*` |
|       - | 2149 | ` * Node comparison callback: Compare nodes by keys only.` |
|       - | 2150 | ` * used-by: [krsort()]` |
|       - | 2151 | ` */` |
|       4 | 2152 | `static sxi32 HashmapCmpCallback5(ph7_hashmap_node *pA,ph7_hashmap_node *pB,void *pCmpData)` |
|       1 | 2153 | `{` |
|       - | 2154 | `	sxi32 rc;` |
|       2 | 2155 | `	SXUNUSED(pCmpData); /* cc warning */` |
|       5 | 2156 | `	if( pA->iType == HASHMAP_BLOB_NODE && pB->iType == HASHMAP_BLOB_NODE ){` |
|       - | 2157 | `		/* Perform a string comparison */` |
|       5 | 2158 | `		rc = SyBlobCmp(&pA->xKey.sKey,&pB->xKey.sKey);` |
|       3 | 2159 | `	}else{` |
|       - | 2160 | `		SyString sStr;` |
|       - | 2161 | `		sxi64 iA,iB;` |
|       - | 2162 | `		/* Perform a numeric comparison */` |
|     ! 0 | 2163 | `		if( pA->iType == HASHMAP_BLOB_NODE ){` |
|       - | 2164 | `			/* Cast to 64-bit integer */` |
|     ! 0 | 2165 | `			SyStringInitFromBuf(&sStr,SyBlobData(&pA->xKey.sKey),SyBlobLength(&pA->xKey.sKey));` |
|     ! 0 | 2166 | `			if( sStr.nByte < 1 ){` |
|     ! 0 | 2167 | `				iA = 0;` |
|     ! 0 | 2168 | `			}else{` |
|     ! 0 | 2169 | `				SyStrToInt64(sStr.zString,sStr.nByte,(void *)&iA,0);` |
|       - | 2170 | `			}` |
|     ! 0 | 2171 | `		}else{` |
|     ! 0 | 2172 | `			iA = pA->xKey.iKey;` |
|       - | 2173 | `		}` |
|     ! 0 | 2174 | `		if( pB->iType == HASHMAP_BLOB_NODE ){` |
|       - | 2175 | `			/* Cast to 64-bit integer */` |
|     ! 0 | 2176 | `			SyStringInitFromBuf(&sStr,SyBlobData(&pB->xKey.sKey),SyBlobLength(&pB->xKey.sKey));` |
|     ! 0 | 2177 | `			if( sStr.nByte < 1 ){` |
|     ! 0 | 2178 | `				iB = 0;` |
|     ! 0 | 2179 | `			}else{` |
|     ! 0 | 2180 | `				SyStrToInt64(sStr.zString,sStr.nByte,(void *)&iB,0);` |
|       - | 2181 | `			}` |
|     ! 0 | 2182 | `		}else{` |
|     ! 0 | 2183 | `			iB = pB->xKey.iKey;` |
|       - | 2184 | `		}` |
|     ! 0 | 2185 | `		rc = (sxi32)(iA-iB);` |
|       - | 2186 | `	}` |
|       5 | 2187 | `	return -rc; /* Reverse result */` |
|       1 | 2188 | `}` |
|       - | 2189 | `/*` |
|       - | 2190 | ` * Node comparison callback: Invoke an user-defined callback for the purpose of node comparison.` |
|       - | 2191 | ` * used-by: [uksort()]` |
|       - | 2192 | ` */` |
|       6 | 2193 | `static sxi32 HashmapCmpCallback6(ph7_hashmap_node *pA,ph7_hashmap_node *pB,void *pCmpData)` |
|       1 | 2194 | `{` |
|       - | 2195 | `	ph7_value sResult,*pCallback;` |
|       - | 2196 | `	ph7_value *apArg[2];  /* Callback arguments */` |
|       - | 2197 | `	ph7_value sK1,sK2;` |
|       - | 2198 | `	sxi32 rc;` |
|       - | 2199 | `	/* Point to the desired callback */` |
|       7 | 2200 | `	pCallback = (ph7_value *)pCmpData;` |
|       7 | 2201 | `	if( pA->pMap->pVm->iCmpCallbackExc ){` |
|       - | 2202 | `		/* A previous comparison already raised: stop invoking the callback so` |
|       - | 2203 | `		 * the exception is not thrown again, and let the sort wind down. */` |
|     ! 0 | 2204 | `		return 0;` |
|       - | 2205 | `	}` |
|       - | 2206 | `	/* initialize the result value */` |
|       7 | 2207 | `	PH7_MemObjInit(pA->pMap->pVm,&sResult);` |
|       7 | 2208 | `	PH7_MemObjInit(pA->pMap->pVm,&sK1);` |
|       7 | 2209 | `	PH7_MemObjInit(pA->pMap->pVm,&sK2);` |
|       - | 2210 | `	/* Extract nodes keys */` |
|       7 | 2211 | `	PH7_HashmapExtractNodeKey(pA,&sK1);` |
|       7 | 2212 | `	PH7_HashmapExtractNodeKey(pB,&sK2);` |
|       7 | 2213 | `	apArg[0] = &sK1;` |
|       7 | 2214 | `	apArg[1] = &sK2;` |
|       - | 2215 | `	/* Mark keys as constants */` |
|       7 | 2216 | `	sK1.nIdx = SXU32_HIGH;` |
|       7 | 2217 | `	sK2.nIdx = SXU32_HIGH;` |
|       - | 2218 | `	/* Invoke the callback */` |
|       7 | 2219 | `	rc = PH7_VmCallUserFunction(pA->pMap->pVm,pCallback,2,apArg,&sResult);` |
|       7 | 2220 | `	if( rc == PH7_EXCEPTION ){` |
|       - | 2221 | `		/* The comparator raised: flag it so the sort driver aborts and` |
|       - | 2222 | `		 * propagates, and order this pair arbitrarily for the rest of the run. */` |
|     ! 0 | 2223 | `		pA->pMap->pVm->iCmpCallbackExc = 1;` |
|     ! 0 | 2224 | `		rc = 0;` |
|       7 | 2225 | `	}else if( rc != SXRET_OK ){` |
|       - | 2226 | `		/* An error occured while calling user defined function [i.e: not defined] */` |
|     ! 0 | 2227 | `		rc = -1; /* Set a dummy result */` |
|     ! 0 | 2228 | `	}else{` |
|       - | 2229 | `		/* Extract callback result */` |
|       7 | 2230 | `		if((sResult.iFlags & MEMOBJ_INT) == 0 ){` |
|       - | 2231 | `			/* Perform an int cast */` |
|     ! 0 | 2232 | `			PH7_MemObjToInteger(&sResult);` |
|     ! 0 | 2233 | `		}` |
|       7 | 2234 | `		rc = (sxi32)sResult.x.iVal;` |
|       - | 2235 | `	}` |
|       7 | 2236 | `	PH7_MemObjRelease(&sResult);` |
|       7 | 2237 | `	PH7_MemObjRelease(&sK1);` |
|       7 | 2238 | `	PH7_MemObjRelease(&sK2);` |
|       - | 2239 | `	/* Callback result */` |
|       7 | 2240 | `	return rc;` |
|       4 | 2241 | `}` |
|       - | 2242 | `/*` |
|       - | 2243 | ` * Node comparison callback: Random node comparison.` |
|       - | 2244 | ` * used-by: [shuffle()]` |
|       - | 2245 | ` */` |
|      15 | 2246 | `static sxi32 HashmapCmpCallback7(ph7_hashmap_node *pA,ph7_hashmap_node *pB,void *pCmpData)` |
|       1 | 2247 | `{` |
|       - | 2248 | `	sxu32 n;` |
|       7 | 2249 | `	SXUNUSED(pB); /* cc warning */` |
|       7 | 2250 | `	SXUNUSED(pCmpData);` |
|       - | 2251 | `	/* Grab a random number */` |
|      16 | 2252 | `	n = PH7_VmRandomNum(pA->pMap->pVm);` |
|       - | 2253 | `	/* if the random number is odd then the first node 'pA' is greater then` |
|       - | 2254 | `	 * the second node 'pB'. Otherwise the reverse is assumed.` |
|       - | 2255 | `	 */` |
|      16 | 2256 | `	return n&1 ? 1 : -1;` |
|       1 | 2257 | `}` |
|       - | 2258 | `/*` |
|       - | 2259 | ` * Rehash all nodes keys after a merge-sort have been applied.` |
|       - | 2260 | ` * Used by [sort(),usort() and rsort()].` |
|       - | 2261 | ` */` |
|     640 | 2262 | `static void HashmapSortRehash(ph7_hashmap *pMap)` |
|       5 | 2263 | `{` |
|       - | 2264 | `	ph7_hashmap_node *p,*pLast;` |
|       - | 2265 | `	sxu32 i;` |
|       - | 2266 | `	/* Rehash all entries */` |
|     645 | 2267 | `	pLast = p = pMap->pFirst;` |
|     645 | 2268 | `	pMap->iNextIdx = 0; /* Reset the automatic index */` |
|     645 | 2269 | `	i = 0;` |
|    6877 | 2270 | `	for( ;; ){` |
|   13759 | 2271 | `		if( i >= pMap->nEntry ){` |
|     645 | 2272 | `			pMap->pLast = pLast; /* Fix the last link broken by the merge-sort */` |
|     645 | 2273 | `			break;` |
|       - | 2274 | `		}` |
|   13119 | 2275 | `		if( p->iType == HASHMAP_BLOB_NODE ){` |
|       - | 2276 | `			/* Do not maintain index association as requested by the PHP specification */` |
|       5 | 2277 | `			SyBlobRelease(&p->xKey.sKey);` |
|       - | 2278 | `			/* Change key type */` |
|       5 | 2279 | `			p->iType = HASHMAP_INT_NODE;` |
|       2 | 2280 | `		}` |
|   13119 | 2281 | `		HashmapRehashIntNode(p);` |
|       - | 2282 | `		/* Point to the next entry */` |
|   13119 | 2283 | `		i++;` |
|   13119 | 2284 | `		pLast = p;` |
|   13119 | 2285 | `		p = p->pPrev; /* Reverse link */` |
|       5 | 2286 | `	}` |
|     645 | 2287 | `}` |
|       - | 2288 | `/*` |
|       - | 2289 | ` * Array functions implementation.` |
|       - | 2290 | ` * Status:` |
|       - | 2291 | ` *  Stable.` |
|       - | 2292 | ` */` |
|       - | 2293 | `/*` |
|       - | 2294 | ` * bool sort(array &$array[,int $sort_flags = SORT_REGULAR ] )` |
|       - | 2295 | ` * Sort an array.` |
|       - | 2296 | ` * Parameters` |
|       - | 2297 | ` *  $array` |
|       - | 2298 | ` *   The input array.` |
|       - | 2299 | ` * $sort_flags` |
|       - | 2300 | ` *  The optional second parameter sort_flags may be used to modify the sorting behavior using these values:` |
|       - | 2301 | ` *  Sorting type flags:` |
|       - | 2302 | ` *   SORT_REGULAR - compare items normally (don't change types)` |
|       - | 2303 | ` *   SORT_NUMERIC - compare items numerically` |
|       - | 2304 | ` *   SORT_STRING - compare items as strings` |
|       - | 2305 | ` * Return` |
|       - | 2306 | ` *  TRUE on success or FALSE on failure.` |
|       - | 2307 | ` *` |
|       - | 2308 | ` */` |
|     982 | 2309 | `static int ph7_hashmap_sort(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       5 | 2310 | `{` |
|       - | 2311 | `	ph7_hashmap *pMap;` |
|       - | 2312 | `	/* Make sure we are dealing with a valid hashmap */` |
|     987 | 2313 | `	if( nArg < 1 \|\| !ph7_value_is_array(apArg[0]) ){` |
|       - | 2314 | `		/* Missing/Invalid arguments,return FALSE */` |
|     ! 0 | 2315 | `		ph7_result_bool(pCtx,0);` |
|     ! 0 | 2316 | `		return PH7_OK;` |
|       - | 2317 | `	}` |
|       - | 2318 | `	/* Point to the internal representation of the input hashmap */` |
|     987 | 2319 | `	PH7_HashmapCowSeparate(pCtx->pVm, apArg[0]);` |
|     987 | 2320 | `	pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|     987 | 2321 | `	if( pMap->nEntry > 1 ){` |
|     629 | 2322 | `		sxi32 iCmpFlags = 0;` |
|     629 | 2323 | `		if( nArg > 1 ){` |
|       - | 2324 | `			/* Extract comparison flags */` |
|       3 | 2325 | `			iCmpFlags = ph7_value_to_int(apArg[1]);` |
|       3 | 2326 | `			if( iCmpFlags == 3 /* SORT_REGULAR */ ){` |
|     ! 0 | 2327 | `				iCmpFlags = 0; /* Standard comparison */` |
|     ! 0 | 2328 | `			}` |
|       1 | 2329 | `		}` |
|       - | 2330 | `		/* Do the merge sort */` |
|     629 | 2331 | `		HashmapMergeSort(pMap,HashmapCmpCallback1,SX_INT_TO_PTR(iCmpFlags));` |
|       - | 2332 | `		/* Rehash [Do not maintain index association as requested by the PHP specification] */` |
|     629 | 2333 | `		HashmapSortRehash(pMap);` |
|     312 | 2334 | `	}` |
|       - | 2335 | `	/* All done,return TRUE */` |
|     987 | 2336 | `	ph7_result_bool(pCtx,1);` |
|     987 | 2337 | `	return PH7_OK;` |
|     496 | 2338 | `}` |
|       - | 2339 | `/*` |
|       - | 2340 | ` * bool asort(array &$array[,int $sort_flags = SORT_REGULAR ] )` |
|       - | 2341 | ` *  Sort an array and maintain index association.` |
|       - | 2342 | ` * Parameters` |
|       - | 2343 | ` *  $array` |
|       - | 2344 | ` *   The input array.` |
|       - | 2345 | ` * $sort_flags` |
|       - | 2346 | ` *  The optional second parameter sort_flags may be used to modify the sorting behavior using these values:` |
|       - | 2347 | ` *  Sorting type flags:` |
|       - | 2348 | ` *   SORT_REGULAR - compare items normally (don't change types)` |
|       - | 2349 | ` *   SORT_NUMERIC - compare items numerically` |
|       - | 2350 | ` *   SORT_STRING - compare items as strings` |
|       - | 2351 | ` * Return` |
|       - | 2352 | ` *  TRUE on success or FALSE on failure.` |
|       - | 2353 | ` */` |
|      32 | 2354 | `static int ph7_hashmap_asort(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       5 | 2355 | `{` |
|       - | 2356 | `	ph7_hashmap *pMap;` |
|       - | 2357 | `	/* PHP 8: ArgumentCountError if no arguments */` |
|      37 | 2358 | `	if( nArg < 1 ){` |
|       3 | 2359 | `		return PH7_VmThrowException(pCtx,` |
|       - | 2360 | `			"ArgumentCountError",` |
|       - | 2361 | `			"asort() expects at least 1 argument, 0 given"` |
|       - | 2362 | `			);` |
|       - | 2363 | `	}` |
|       - | 2364 | `	/* PHP 8: TypeError if first argument is not an array */` |
|      35 | 2365 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|      16 | 2366 | `		return PH7_VmThrowException(pCtx,` |
|       - | 2367 | `			"TypeError",` |
|       - | 2368 | `			"asort(): Argument #1 ($array) must be of type array, %s given",` |
|       4 | 2369 | `			ph7_type_name(apArg[0])` |
|       - | 2370 | `			);` |
|       - | 2371 | `	}` |
|       - | 2372 | `	/* Point to the internal representation of the input hashmap */` |
|      23 | 2373 | `	PH7_HashmapCowSeparate(pCtx->pVm, apArg[0]);` |
|      23 | 2374 | `	pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|      23 | 2375 | `	if( pMap->nEntry > 1 ){` |
|      19 | 2376 | `		sxi32 iCmpFlags = 0;` |
|      19 | 2377 | `		if( nArg > 1 ){` |
|       - | 2378 | `			/* Extract comparison flags */` |
|       5 | 2379 | `			iCmpFlags = ph7_value_to_int(apArg[1]);` |
|       5 | 2380 | `			if( iCmpFlags == 3 /* SORT_REGULAR */ ){` |
|     ! 0 | 2381 | `				iCmpFlags = 0; /* Standard comparison */` |
|     ! 0 | 2382 | `			}` |
|       2 | 2383 | `		}` |
|       - | 2384 | `		/* Do the merge sort */` |
|      19 | 2385 | `		HashmapMergeSort(pMap,HashmapCmpCallback1,SX_INT_TO_PTR(iCmpFlags));` |
|       - | 2386 | `		/* Fix the last link broken by the merge */` |
|      45 | 2387 | `		while(pMap->pLast->pPrev){` |
|      27 | 2388 | `			pMap->pLast = pMap->pLast->pPrev;` |
|       1 | 2389 | `		}` |
|       9 | 2390 | `	}` |
|       - | 2391 | `	/* All done,return TRUE */` |
|      23 | 2392 | `	ph7_result_bool(pCtx,1);` |
|      23 | 2393 | `	return PH7_OK;` |
|      21 | 2394 | `}` |
|       - | 2395 | `/*` |
|       - | 2396 | ` * bool arsort(array &$array[,int $sort_flags = SORT_REGULAR ] )` |
|       - | 2397 | ` *  Sort an array in reverse order and maintain index association.` |
|       - | 2398 | ` * Parameters` |
|       - | 2399 | ` *  $array` |
|       - | 2400 | ` *   The input array.` |
|       - | 2401 | ` * $sort_flags` |
|       - | 2402 | ` *  The optional second parameter sort_flags may be used to modify the sorting behavior using these values:` |
|       - | 2403 | ` *  Sorting type flags:` |
|       - | 2404 | ` *   SORT_REGULAR - compare items normally (don't change types)` |
|       - | 2405 | ` *   SORT_NUMERIC - compare items numerically` |
|       - | 2406 | ` *   SORT_STRING - compare items as strings` |
|       - | 2407 | ` * Return` |
|       - | 2408 | ` *  TRUE on success or FALSE on failure.` |
|       - | 2409 | ` */` |
|      32 | 2410 | `static int ph7_hashmap_arsort(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       5 | 2411 | `{` |
|       - | 2412 | `	ph7_hashmap *pMap;` |
|       - | 2413 | `	/* PHP 8: ArgumentCountError if no arguments */` |
|      37 | 2414 | `	if( nArg < 1 ){` |
|       3 | 2415 | `		return PH7_VmThrowException(pCtx,` |
|       - | 2416 | `			"ArgumentCountError",` |
|       - | 2417 | `			"arsort() expects at least 1 argument, 0 given"` |
|       - | 2418 | `			);` |
|       - | 2419 | `	}` |
|       - | 2420 | `	/* PHP 8: TypeError if first argument is not an array */` |
|      35 | 2421 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|      16 | 2422 | `		return PH7_VmThrowException(pCtx,` |
|       - | 2423 | `			"TypeError",` |
|       - | 2424 | `			"arsort(): Argument #1 ($array) must be of type array, %s given",` |
|       4 | 2425 | `			ph7_type_name(apArg[0])` |
|       - | 2426 | `			);` |
|       - | 2427 | `	}` |
|       - | 2428 | `	/* Point to the internal representation of the input hashmap */` |
|      23 | 2429 | `	PH7_HashmapCowSeparate(pCtx->pVm, apArg[0]);` |
|      23 | 2430 | `	pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|      23 | 2431 | `	if( pMap->nEntry > 1 ){` |
|      19 | 2432 | `		sxi32 iCmpFlags = 0;` |
|      19 | 2433 | `		if( nArg > 1 ){` |
|       - | 2434 | `			/* Extract comparison flags */` |
|       5 | 2435 | `			iCmpFlags = ph7_value_to_int(apArg[1]);` |
|       5 | 2436 | `			if( iCmpFlags == 3 /* SORT_REGULAR */ ){` |
|     ! 0 | 2437 | `				iCmpFlags = 0; /* Standard comparison */` |
|     ! 0 | 2438 | `			}` |
|       2 | 2439 | `		}` |
|       - | 2440 | `		/* Do the merge sort */` |
|      19 | 2441 | `		HashmapMergeSort(pMap,HashmapCmpCallback3,SX_INT_TO_PTR(iCmpFlags));` |
|       - | 2442 | `		/* Fix the last link broken by the merge */` |
|      35 | 2443 | `		while(pMap->pLast->pPrev){` |
|      17 | 2444 | `			pMap->pLast = pMap->pLast->pPrev;` |
|       1 | 2445 | `		}` |
|       9 | 2446 | `	}` |
|       - | 2447 | `	/* All done,return TRUE */` |
|      23 | 2448 | `	ph7_result_bool(pCtx,1);` |
|      23 | 2449 | `	return PH7_OK;` |
|      21 | 2450 | `}` |
|       - | 2451 | `/*` |
|       - | 2452 | ` * bool ksort(array &$array[,int $sort_flags = SORT_REGULAR ] )` |
|       - | 2453 | ` *  Sort an array by key.` |
|       - | 2454 | ` * Parameters` |
|       - | 2455 | ` *  $array` |
|       - | 2456 | ` *   The input array.` |
|       - | 2457 | ` * $sort_flags` |
|       - | 2458 | ` *  The optional second parameter sort_flags may be used to modify the sorting behavior using these values:` |
|       - | 2459 | ` *  Sorting type flags:` |
|       - | 2460 | ` *   SORT_REGULAR - compare items normally (don't change types)` |
|       - | 2461 | ` *   SORT_NUMERIC - compare items numerically` |
|       - | 2462 | ` *   SORT_STRING - compare items as strings` |
|       - | 2463 | ` * Return` |
|       - | 2464 | ` *  TRUE on success or FALSE on failure.` |
|       - | 2465 | ` */` |
|       4 | 2466 | `static int ph7_hashmap_ksort(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       1 | 2467 | `{` |
|       - | 2468 | `	ph7_hashmap *pMap;` |
|       - | 2469 | `	/* Make sure we are dealing with a valid hashmap */` |
|       5 | 2470 | `	if( nArg < 1 \|\| !ph7_value_is_array(apArg[0]) ){` |
|       - | 2471 | `		/* Missing/Invalid arguments,return FALSE */` |
|     ! 0 | 2472 | `		ph7_result_bool(pCtx,0);` |
|     ! 0 | 2473 | `		return PH7_OK;` |
|       - | 2474 | `	}` |
|       - | 2475 | `	/* Point to the internal representation of the input hashmap */` |
|       5 | 2476 | `	PH7_HashmapCowSeparate(pCtx->pVm, apArg[0]);` |
|       5 | 2477 | `	pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|       5 | 2478 | `	if( pMap->nEntry > 1 ){` |
|       5 | 2479 | `		sxi32 iCmpFlags = 0;` |
|       5 | 2480 | `		if( nArg > 1 ){` |
|       - | 2481 | `			/* Extract comparison flags */` |
|     ! 0 | 2482 | `			iCmpFlags = ph7_value_to_int(apArg[1]);` |
|     ! 0 | 2483 | `			if( iCmpFlags == 3 /* SORT_REGULAR */ ){` |
|     ! 0 | 2484 | `				iCmpFlags = 0; /* Standard comparison */` |
|     ! 0 | 2485 | `			}` |
|     ! 0 | 2486 | `		}` |
|       - | 2487 | `		/* Do the merge sort */` |
|       5 | 2488 | `		HashmapMergeSort(pMap,HashmapCmpCallback2,SX_INT_TO_PTR(iCmpFlags));` |
|       - | 2489 | `		/* Fix the last link broken by the merge */` |
|      15 | 2490 | `		while(pMap->pLast->pPrev){` |
|      11 | 2491 | `			pMap->pLast = pMap->pLast->pPrev;` |
|       1 | 2492 | `		}` |
|       2 | 2493 | `	}` |
|       - | 2494 | `	/* All done,return TRUE */` |
|       5 | 2495 | `	ph7_result_bool(pCtx,1);` |
|       5 | 2496 | `	return PH7_OK;` |
|       3 | 2497 | `}` |
|       - | 2498 | `/*` |
|       - | 2499 | ` * bool krsort(array &$array[,int $sort_flags = SORT_REGULAR ] )` |
|       - | 2500 | ` *  Sort an array by key in reverse order.` |
|       - | 2501 | ` * Parameters` |
|       - | 2502 | ` *  $array` |
|       - | 2503 | ` *   The input array.` |
|       - | 2504 | ` * $sort_flags` |
|       - | 2505 | ` *  The optional second parameter sort_flags may be used to modify the sorting behavior using these values:` |
|       - | 2506 | ` *  Sorting type flags:` |
|       - | 2507 | ` *   SORT_REGULAR - compare items normally (don't change types)` |
|       - | 2508 | ` *   SORT_NUMERIC - compare items numerically` |
|       - | 2509 | ` *   SORT_STRING - compare items as strings` |
|       - | 2510 | ` * Return` |
|       - | 2511 | ` *  TRUE on success or FALSE on failure.` |
|       - | 2512 | ` */` |
|       2 | 2513 | `static int ph7_hashmap_krsort(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       1 | 2514 | `{` |
|       - | 2515 | `	ph7_hashmap *pMap;` |
|       - | 2516 | `	/* Make sure we are dealing with a valid hashmap */` |
|       3 | 2517 | `	if( nArg < 1 \|\| !ph7_value_is_array(apArg[0]) ){` |
|       - | 2518 | `		/* Missing/Invalid arguments,return FALSE */` |
|     ! 0 | 2519 | `		ph7_result_bool(pCtx,0);` |
|     ! 0 | 2520 | `		return PH7_OK;` |
|       - | 2521 | `	}` |
|       - | 2522 | `	/* Point to the internal representation of the input hashmap */` |
|       3 | 2523 | `	PH7_HashmapCowSeparate(pCtx->pVm, apArg[0]);` |
|       3 | 2524 | `	pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|       3 | 2525 | `	if( pMap->nEntry > 1 ){` |
|       3 | 2526 | `		sxi32 iCmpFlags = 0;` |
|       3 | 2527 | `		if( nArg > 1 ){` |
|       - | 2528 | `			/* Extract comparison flags */` |
|     ! 0 | 2529 | `			iCmpFlags = ph7_value_to_int(apArg[1]);` |
|     ! 0 | 2530 | `			if( iCmpFlags == 3 /* SORT_REGULAR */ ){` |
|     ! 0 | 2531 | `				iCmpFlags = 0; /* Standard comparison */` |
|     ! 0 | 2532 | `			}` |
|     ! 0 | 2533 | `		}` |
|       - | 2534 | `		/* Do the merge sort */` |
|       3 | 2535 | `		HashmapMergeSort(pMap,HashmapCmpCallback5,SX_INT_TO_PTR(iCmpFlags));` |
|       - | 2536 | `		/* Fix the last link broken by the merge */` |
|       7 | 2537 | `		while(pMap->pLast->pPrev){` |
|       5 | 2538 | `			pMap->pLast = pMap->pLast->pPrev;` |
|       1 | 2539 | `		}` |
|       1 | 2540 | `	}` |
|       - | 2541 | `	/* All done,return TRUE */` |
|       3 | 2542 | `	ph7_result_bool(pCtx,1);` |
|       3 | 2543 | `	return PH7_OK;` |
|       2 | 2544 | `}` |
|       - | 2545 | `/*` |
|       - | 2546 | ` * bool rsort(array &$array[,int $sort_flags = SORT_REGULAR ] )` |
|       - | 2547 | ` * Sort an array in reverse order.` |
|       - | 2548 | ` * Parameters` |
|       - | 2549 | ` *  $array` |
|       - | 2550 | ` *   The input array.` |
|       - | 2551 | ` * $sort_flags` |
|       - | 2552 | ` *  The optional second parameter sort_flags may be used to modify the sorting behavior using these values:` |
|       - | 2553 | ` *  Sorting type flags:` |
|       - | 2554 | ` *   SORT_REGULAR - compare items normally (don't change types)` |
|       - | 2555 | ` *   SORT_NUMERIC - compare items numerically` |
|       - | 2556 | ` *   SORT_STRING - compare items as strings` |
|       - | 2557 | ` * Return` |
|       - | 2558 | ` *  TRUE on success or FALSE on failure.` |
|       - | 2559 | ` */` |
|       2 | 2560 | `static int ph7_hashmap_rsort(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       1 | 2561 | `{` |
|       - | 2562 | `	ph7_hashmap *pMap;` |
|       - | 2563 | `	/* Make sure we are dealing with a valid hashmap */` |
|       3 | 2564 | `	if( nArg < 1 \|\| !ph7_value_is_array(apArg[0]) ){` |
|       - | 2565 | `		/* Missing/Invalid arguments,return FALSE */` |
|     ! 0 | 2566 | `		ph7_result_bool(pCtx,0);` |
|     ! 0 | 2567 | `		return PH7_OK;` |
|       - | 2568 | `	}` |
|       - | 2569 | `	/* Point to the internal representation of the input hashmap */` |
|       3 | 2570 | `	PH7_HashmapCowSeparate(pCtx->pVm, apArg[0]);` |
|       3 | 2571 | `	pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|       3 | 2572 | `	if( pMap->nEntry > 1 ){` |
|       3 | 2573 | `		sxi32 iCmpFlags = 0;` |
|       3 | 2574 | `		if( nArg > 1 ){` |
|       - | 2575 | `			/* Extract comparison flags */` |
|     ! 0 | 2576 | `			iCmpFlags = ph7_value_to_int(apArg[1]);` |
|     ! 0 | 2577 | `			if( iCmpFlags == 3 /* SORT_REGULAR */ ){` |
|     ! 0 | 2578 | `				iCmpFlags = 0; /* Standard comparison */` |
|     ! 0 | 2579 | `			}` |
|     ! 0 | 2580 | `		}` |
|       - | 2581 | `		/* Do the merge sort */` |
|       3 | 2582 | `		HashmapMergeSort(pMap,HashmapCmpCallback3,SX_INT_TO_PTR(iCmpFlags));` |
|       - | 2583 | `		/* Rehash [Do not maintain index association as requested by the PHP specification] */` |
|       3 | 2584 | `		HashmapSortRehash(pMap);` |
|       1 | 2585 | `	}` |
|       - | 2586 | `	/* All done,return TRUE */` |
|       3 | 2587 | `	ph7_result_bool(pCtx,1);` |
|       3 | 2588 | `	return PH7_OK;` |
|       2 | 2589 | `}` |
|       - | 2590 | `/*` |
|       - | 2591 | ` * bool usort(array &$array,callable $cmp_function)` |
|       - | 2592 | ` *  Sort an array by values using a user-defined comparison function.` |
|       - | 2593 | ` * Parameters` |
|       - | 2594 | ` *  $array` |
|       - | 2595 | ` *   The input array.` |
|       - | 2596 | ` * $cmp_function` |
|       - | 2597 | ` *  The comparison function must return an integer less than, equal to, or greater` |
|       - | 2598 | ` *  than zero if the first argument is considered to be respectively less than, equal` |
|       - | 2599 | ` *  to, or greater than the second.` |
|       - | 2600 | ` *    int callback ( mixed $a, mixed $b )` |
|       - | 2601 | ` * Return` |
|       - | 2602 | ` *  TRUE on success or FALSE on failure.` |
|       - | 2603 | ` */` |
|      12 | 2604 | `static int ph7_hashmap_usort(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       3 | 2605 | `{` |
|       - | 2606 | `	ph7_hashmap *pMap;` |
|       - | 2607 | `	/* Make sure we are dealing with a valid hashmap */` |
|      15 | 2608 | `	if( nArg < 1 \|\| !ph7_value_is_array(apArg[0]) ){` |
|       - | 2609 | `		/* Missing/Invalid arguments,return FALSE */` |
|     ! 0 | 2610 | `		ph7_result_bool(pCtx,0);` |
|     ! 0 | 2611 | `		return PH7_OK;` |
|       - | 2612 | `	}` |
|       - | 2613 | `	/* Point to the internal representation of the input hashmap */` |
|      15 | 2614 | `	PH7_HashmapCowSeparate(pCtx->pVm, apArg[0]);` |
|      15 | 2615 | `	pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|      15 | 2616 | `	if( pMap->nEntry > 1 ){` |
|      15 | 2617 | `		ph7_value *pCallback = 0;` |
|       - | 2618 | `		ProcNodeCmp xCmp;` |
|      15 | 2619 | `		xCmp = HashmapCmpCallback4; /* User-defined function as the comparison callback */` |
|      15 | 2620 | `		if( nArg > 1 && ph7_value_is_callable(apArg[1]) ){` |
|       - | 2621 | `			/* Point to the desired callback */` |
|      15 | 2622 | `			pCallback = apArg[1];` |
|       9 | 2623 | `		}else{` |
|       - | 2624 | `			/* Use the default comparison function */` |
|     ! 0 | 2625 | `			xCmp = HashmapCmpCallback1;` |
|       - | 2626 | `		}` |
|       - | 2627 | `		/* Do the merge sort */` |
|      15 | 2628 | `		pCtx->pVm->iCmpCallbackExc = 0;` |
|      15 | 2629 | `		HashmapMergeSort(pMap,xCmp,pCallback);` |
|       - | 2630 | `		/* Rehash [Do not maintain index association as requested by the PHP specification] */` |
|      15 | 2631 | `		HashmapSortRehash(pMap);` |
|      15 | 2632 | `		if( pCtx->pVm->iCmpCallbackExc ){` |
|       - | 2633 | `			/* The comparison callback raised: propagate so the dispatcher unwinds. */` |
|       6 | 2634 | `			pCtx->pVm->iCmpCallbackExc = 0;` |
|       6 | 2635 | `			return PH7_EXCEPTION;` |
|       - | 2636 | `		}` |
|       4 | 2637 | `	}` |
|       - | 2638 | `	/* All done,return TRUE */` |
|      10 | 2639 | `	ph7_result_bool(pCtx,1);` |
|      10 | 2640 | `	return PH7_OK;` |
|       9 | 2641 | `}` |
|       - | 2642 | `/*` |
|       - | 2643 | ` * bool uasort(array &$array,callable $cmp_function)` |
|       - | 2644 | ` *  Sort an array by values using a user-defined comparison function` |
|       - | 2645 | ` *  and maintain index association.` |
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
|       2 | 2657 | `static int ph7_hashmap_uasort(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       1 | 2658 | `{` |
|       - | 2659 | `	ph7_hashmap *pMap;` |
|       - | 2660 | `	/* Make sure we are dealing with a valid hashmap */` |
|       3 | 2661 | `	if( nArg < 1 \|\| !ph7_value_is_array(apArg[0]) ){` |
|       - | 2662 | `		/* Missing/Invalid arguments,return FALSE */` |
|     ! 0 | 2663 | `		ph7_result_bool(pCtx,0);` |
|     ! 0 | 2664 | `		return PH7_OK;` |
|       - | 2665 | `	}` |
|       - | 2666 | `	/* Point to the internal representation of the input hashmap */` |
|       3 | 2667 | `	PH7_HashmapCowSeparate(pCtx->pVm, apArg[0]);` |
|       3 | 2668 | `	pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|       3 | 2669 | `	if( pMap->nEntry > 1 ){` |
|       3 | 2670 | `		ph7_value *pCallback = 0;` |
|       - | 2671 | `		ProcNodeCmp xCmp;` |
|       3 | 2672 | `		xCmp = HashmapCmpCallback4; /* User-defined function as the comparison callback */` |
|       3 | 2673 | `		if( nArg > 1 && ph7_value_is_callable(apArg[1]) ){` |
|       - | 2674 | `			/* Point to the desired callback */` |
|       3 | 2675 | `			pCallback = apArg[1];` |
|       2 | 2676 | `		}else{` |
|       - | 2677 | `			/* Use the default comparison function */` |
|     ! 0 | 2678 | `			xCmp = HashmapCmpCallback1;` |
|       - | 2679 | `		}` |
|       - | 2680 | `		/* Do the merge sort */` |
|       3 | 2681 | `		pCtx->pVm->iCmpCallbackExc = 0;` |
|       3 | 2682 | `		HashmapMergeSort(pMap,xCmp,pCallback);` |
|       - | 2683 | `		/* Fix the last link broken by the merge */` |
|       5 | 2684 | `		while(pMap->pLast->pPrev){` |
|       3 | 2685 | `			pMap->pLast = pMap->pLast->pPrev;` |
|       1 | 2686 | `		}` |
|       3 | 2687 | `		if( pCtx->pVm->iCmpCallbackExc ){` |
|       - | 2688 | `			/* The comparison callback raised: propagate so the dispatcher unwinds. */` |
|     ! 0 | 2689 | `			pCtx->pVm->iCmpCallbackExc = 0;` |
|     ! 0 | 2690 | `			return PH7_EXCEPTION;` |
|       - | 2691 | `		}` |
|       1 | 2692 | `	}` |
|       - | 2693 | `	/* All done,return TRUE */` |
|       3 | 2694 | `	ph7_result_bool(pCtx,1);` |
|       3 | 2695 | `	return PH7_OK;` |
|       2 | 2696 | `}` |
|       - | 2697 | `/*` |
|       - | 2698 | ` * bool uksort(array &$array,callable $cmp_function)` |
|       - | 2699 | ` *  Sort an array by keys using a user-defined comparison` |
|       - | 2700 | ` *  function and maintain index association.` |
|       - | 2701 | ` * Parameters` |
|       - | 2702 | ` *  $array` |
|       - | 2703 | ` *   The input array.` |
|       - | 2704 | ` * $cmp_function` |
|       - | 2705 | ` *  The comparison function must return an integer less than, equal to, or greater` |
|       - | 2706 | ` *  than zero if the first argument is considered to be respectively less than, equal` |
|       - | 2707 | ` *  to, or greater than the second.` |
|       - | 2708 | ` *    int callback ( mixed $a, mixed $b )` |
|       - | 2709 | ` * Return` |
|       - | 2710 | ` *  TRUE on success or FALSE on failure.` |
|       - | 2711 | ` */` |
|       2 | 2712 | `static int ph7_hashmap_uksort(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       1 | 2713 | `{` |
|       - | 2714 | `	ph7_hashmap *pMap;` |
|       - | 2715 | `	/* Make sure we are dealing with a valid hashmap */` |
|       3 | 2716 | `	if( nArg < 1 \|\| !ph7_value_is_array(apArg[0]) ){` |
|       - | 2717 | `		/* Missing/Invalid arguments,return FALSE */` |
|     ! 0 | 2718 | `		ph7_result_bool(pCtx,0);` |
|     ! 0 | 2719 | `		return PH7_OK;` |
|       - | 2720 | `	}` |
|       - | 2721 | `	/* Point to the internal representation of the input hashmap */` |
|       3 | 2722 | `	PH7_HashmapCowSeparate(pCtx->pVm, apArg[0]);` |
|       3 | 2723 | `	pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|       3 | 2724 | `	if( pMap->nEntry > 1 ){` |
|       3 | 2725 | `		ph7_value *pCallback = 0;` |
|       - | 2726 | `		ProcNodeCmp xCmp;` |
|       3 | 2727 | `		xCmp = HashmapCmpCallback6; /* User-defined function as the comparison callback */` |
|       3 | 2728 | `		if( nArg > 1 && ph7_value_is_callable(apArg[1]) ){` |
|       - | 2729 | `			/* Point to the desired callback */` |
|       3 | 2730 | `			pCallback = apArg[1];` |
|       2 | 2731 | `		}else{` |
|       - | 2732 | `			/* Use the default comparison function */` |
|     ! 0 | 2733 | `			xCmp = HashmapCmpCallback2;` |
|       - | 2734 | `		}` |
|       - | 2735 | `		/* Do the merge sort */` |
|       3 | 2736 | `		pCtx->pVm->iCmpCallbackExc = 0;` |
|       3 | 2737 | `		HashmapMergeSort(pMap,xCmp,pCallback);` |
|       - | 2738 | `		/* Fix the last link broken by the merge */` |
|       3 | 2739 | `		while(pMap->pLast->pPrev){` |
|     ! 0 | 2740 | `			pMap->pLast = pMap->pLast->pPrev;` |
|     ! 0 | 2741 | `		}` |
|       3 | 2742 | `		if( pCtx->pVm->iCmpCallbackExc ){` |
|       - | 2743 | `			/* The comparison callback raised: propagate so the dispatcher unwinds. */` |
|     ! 0 | 2744 | `			pCtx->pVm->iCmpCallbackExc = 0;` |
|     ! 0 | 2745 | `			return PH7_EXCEPTION;` |
|       - | 2746 | `		}` |
|       1 | 2747 | `	}` |
|       - | 2748 | `	/* All done,return TRUE */` |
|       3 | 2749 | `	ph7_result_bool(pCtx,1);` |
|       3 | 2750 | `	return PH7_OK;` |
|       2 | 2751 | `}` |
|       - | 2752 | `/*` |
|       - | 2753 | ` * bool shuffle(array &$array)` |
|       - | 2754 | ` *  shuffles (randomizes the order of the elements in) an array.` |
|       - | 2755 | ` * Parameters` |
|       - | 2756 | ` *  $array` |
|       - | 2757 | ` *   The input array.` |
|       - | 2758 | ` * Return` |
|       - | 2759 | ` *  TRUE on success or FALSE on failure.` |
|       - | 2760 | ` *` |
|       - | 2761 | ` */` |
|       2 | 2762 | `static int ph7_hashmap_shuffle(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       1 | 2763 | `{` |
|       - | 2764 | `	ph7_hashmap *pMap;` |
|       - | 2765 | `	/* Make sure we are dealing with a valid hashmap */` |
|       3 | 2766 | `	if( nArg < 1 \|\| !ph7_value_is_array(apArg[0]) ){` |
|       - | 2767 | `		/* Missing/Invalid arguments,return FALSE */` |
|     ! 0 | 2768 | `		ph7_result_bool(pCtx,0);` |
|     ! 0 | 2769 | `		return PH7_OK;` |
|       - | 2770 | `	}` |
|       - | 2771 | `	/* Point to the internal representation of the input hashmap */` |
|       3 | 2772 | `	PH7_HashmapCowSeparate(pCtx->pVm, apArg[0]);` |
|       3 | 2773 | `	pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|       3 | 2774 | `	if( pMap->nEntry > 1 ){` |
|       - | 2775 | `		/* Do the merge sort */` |
|       3 | 2776 | `		HashmapMergeSort(pMap,HashmapCmpCallback7,0);` |
|       - | 2777 | `		/* Fix the last link broken by the merge */` |
|       9 | 2778 | `		while(pMap->pLast->pPrev){` |
|       6 | 2779 | `			pMap->pLast = pMap->pLast->pPrev;` |
|     ! 0 | 2780 | `		}` |
|       1 | 2781 | `	}` |
|       - | 2782 | `	/* All done,return TRUE */` |
|       3 | 2783 | `	ph7_result_bool(pCtx,1);` |
|       3 | 2784 | `	return PH7_OK;` |
|       2 | 2785 | `}` |
|       - | 2786 | `/*` |
|       - | 2787 | ` * int count(array $var [, int $mode = COUNT_NORMAL ])` |
|       - | 2788 | ` *   Count all elements in an array, or something in an object.` |
|       - | 2789 | ` * Parameters` |
|       - | 2790 | ` *  $var` |
|       - | 2791 | ` *   The array or the object.` |
|       - | 2792 | ` * $mode` |
|       - | 2793 | ` *  If the optional mode parameter is set to COUNT_RECURSIVE (or 1), count()` |
|       - | 2794 | ` *  will recursively count the array. This is particularly useful for counting` |
|       - | 2795 | ` *  all the elements of a multidimensional array.` |
|       - | 2796 | ` * Return` |
|       - | 2797 | ` *  Returns the number of elements in the array.` |
|       - | 2798 | ` */` |
|     840 | 2799 | `static int ph7_hashmap_count(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       5 | 2800 | `{` |
|     845 | 2801 | `	int bRecursive = FALSE;` |
|     845 | 2802 | `	int bCycleDetected = FALSE;` |
|       - | 2803 | `	sxi64 iCount;` |
|     845 | 2804 | `	if( nArg < 1 ){` |
|       3 | 2805 | `		return PH7_VmThrowException(pCtx,` |
|       - | 2806 | `			"ArgumentCountError",` |
|       - | 2807 | `			"count() expects at least 1 argument, 0 given"` |
|       - | 2808 | `			);` |
|       - | 2809 | `	}` |
|     843 | 2810 | `	if( nArg > 2 ){` |
|       4 | 2811 | `		return PH7_VmThrowException(pCtx,` |
|       - | 2812 | `			"ArgumentCountError",` |
|       - | 2813 | `			"count() expects at most 2 arguments, %d given",` |
|       1 | 2814 | `			nArg` |
|       - | 2815 | `			);` |
|       - | 2816 | `	}` |
|       - | 2817 | `	/* PHP validates $mode right after parsing, before the type dispatch, so` |
|       - | 2818 | `	 * an invalid mode raises ValueError whether $value is an array or a` |
|       - | 2819 | `	 * Countable object (the mode is then ignored for the Countable path). */` |
|     841 | 2820 | `	if( nArg > 1 ){` |
|      44 | 2821 | `		sxi32 iMode = ph7_value_to_int(apArg[1]);` |
|      44 | 2822 | `		if( iMode != 0 /* COUNT_NORMAL */ && iMode != 1 /* COUNT_RECURSIVE */ ){` |
|      11 | 2823 | `			return PH7_VmThrowException(pCtx,` |
|       - | 2824 | `				"ValueError",` |
|       - | 2825 | `				"count(): Argument #2 ($mode) must be either COUNT_NORMAL or COUNT_RECURSIVE"` |
|       - | 2826 | `				);` |
|       - | 2827 | `		}` |
|      34 | 2828 | `		bRecursive = iMode == 1;` |
|      16 | 2829 | `	}` |
|     833 | 2830 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|       - | 2831 | `		/* Countable object: dispatch to ->count() */` |
|      35 | 2832 | `		if( apArg[0]->iFlags & MEMOBJ_OBJ ){` |
|      24 | 2833 | `			ph7_class_instance *pInst = (ph7_class_instance *)apArg[0]->x.pOther;` |
|      24 | 2834 | `			ph7_class *pCountable = pCtx->pVm->pCountableClass;` |
|      24 | 2835 | `			if( pCountable && PH7_VmInstanceOf(pInst->pClass,pCountable) ){` |
|      21 | 2836 | `				ph7_class_method *pMeth = PH7_ClassExtractMethod(pInst->pClass,` |
|       - | 2837 | `					"count",sizeof("count")-1);` |
|      21 | 2838 | `				if( pMeth ){` |
|       - | 2839 | `					ph7_value sResult;` |
|      21 | 2840 | `					PH7_MemObjInit(pCtx->pVm,&sResult);` |
|      21 | 2841 | `					PH7_VmCallClassMethod(pCtx->pVm,pInst,pMeth,&sResult,0,0);` |
|      21 | 2842 | `					ph7_result_int64(pCtx,ph7_value_to_int64(&sResult));` |
|      21 | 2843 | `					PH7_MemObjRelease(&sResult);` |
|      21 | 2844 | `					return PH7_OK;` |
|       - | 2845 | `				}` |
|     ! 0 | 2846 | `			}` |
|       1 | 2847 | `		}` |
|      22 | 2848 | `		return PH7_VmThrowException(pCtx,` |
|       - | 2849 | `			"TypeError",` |
|       - | 2850 | `			"count(): Argument #1 ($value) must be of type Countable\|array, %s given",` |
|       6 | 2851 | `			ph7_type_name(apArg[0])` |
|       - | 2852 | `			);` |
|       - | 2853 | `	}` |
|       - | 2854 | `	/* Count */` |
|     803 | 2855 | `	iCount = HashmapCount((ph7_hashmap *)apArg[0]->x.pOther,bRecursive,&bCycleDetected);` |
|     803 | 2856 | `	if( bCycleDetected ){` |
|       3 | 2857 | `		PH7_VmThrowError(pCtx->pVm,0,PH7_CTX_WARNING,"count(): Recursion detected");` |
|       1 | 2858 | `	}` |
|     803 | 2859 | `	ph7_result_int64(pCtx,iCount);` |
|     803 | 2860 | `	return PH7_OK;` |
|     425 | 2861 | `}` |
|       - | 2862 | `/*` |
|       - | 2863 | ` * bool array_key_exists(value $key,array $search)` |
|       - | 2864 | ` *  Checks if the given key or index exists in the array.` |
|       - | 2865 | ` * Parameters` |
|       - | 2866 | ` * $key` |
|       - | 2867 | ` *   Value to check.` |
|       - | 2868 | ` * $search` |
|       - | 2869 | ` *  An array with keys to check.` |
|       - | 2870 | ` * Return` |
|       - | 2871 | ` *  TRUE on success or FALSE on failure.` |
|       - | 2872 | ` */` |
|      84 | 2873 | `static int ph7_hashmap_key_exists(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       5 | 2874 | `{` |
|       - | 2875 | `	sxi32 rc;` |
|      89 | 2876 | `	if( nArg != 2 ){` |
|       - | 2877 | `		/* PHP requires exactly two arguments */` |
|      12 | 2878 | `		return PH7_VmThrowException(pCtx,` |
|       - | 2879 | `			"ArgumentCountError",` |
|       - | 2880 | `			"array_key_exists() expects exactly 2 arguments, %d given",` |
|       3 | 2881 | `			nArg` |
|       - | 2882 | `			);` |
|       - | 2883 | `	}` |
|       - | 2884 | `	/* Make sure we are dealing with a valid hashmap */` |
|      83 | 2885 | `	if( !ph7_value_is_array(apArg[1]) ){` |
|       - | 2886 | `		/* Type mismatch -> TypeError */` |
|       8 | 2887 | `		return PH7_VmThrowException(pCtx,` |
|       - | 2888 | `			"TypeError",` |
|       - | 2889 | `			"array_key_exists(): Argument #2 ($array) must be of type array, %s given",` |
|       4 | 2890 | `			ph7_type_name(apArg[1])` |
|       - | 2891 | `			);` |
|       - | 2892 | `	}` |
|       - | 2893 | `	/* Emit deprecation warnings matching PHP behaviour */` |
|      78 | 2894 | `	if( apArg[0]->iFlags & MEMOBJ_NULL ){` |
|       3 | 2895 | `		ph7_context_throw_error_format(pCtx,8192,` |
|       - | 2896 | `			"Using null as the key parameter for array_key_exists() is deprecated, "` |
|       - | 2897 | `			"use an empty string instead"` |
|       - | 2898 | `			);` |
|      77 | 2899 | `	}else if( apArg[0]->iFlags & MEMOBJ_REAL ){` |
|       3 | 2900 | `		ph7_real rVal = apArg[0]->rVal;` |
|       3 | 2901 | `		if( rVal != (ph7_real)(sxi64)rVal ){` |
|       4 | 2902 | `			ph7_context_throw_error_format(pCtx,8192,` |
|       - | 2903 | `				"Implicit conversion from float %g to int loses precision"` |
|       1 | 2904 | `				,rVal` |
|       - | 2905 | `				);` |
|       1 | 2906 | `		}` |
|       1 | 2907 | `	}` |
|       - | 2908 | `	/* Perform the lookup */` |
|      78 | 2909 | `	rc = PH7_HashmapLookup((ph7_hashmap *)apArg[1]->x.pOther,apArg[0],0);` |
|       - | 2910 | `	/* lookup result */` |
|      78 | 2911 | `	ph7_result_bool(pCtx,rc == SXRET_OK ? 1 : 0);` |
|      78 | 2912 | `	return PH7_OK;` |
|      47 | 2913 | `}` |
|       - | 2914 | `/*` |
|       - | 2915 | ` * value array_pop(array $array)` |
|       - | 2916 | ` *   POP the last inserted element from the array.` |
|       - | 2917 | ` * Parameter` |
|       - | 2918 | ` *  The array to get the value from.` |
|       - | 2919 | ` * Return` |
|       - | 2920 | ` *  Poped value or NULL on failure.` |
|       - | 2921 | ` */` |
|      18 | 2922 | `static int ph7_hashmap_pop(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       5 | 2923 | `{` |
|       - | 2924 | `	ph7_hashmap *pMap;` |
|       - | 2925 | `	/* PHP requires exactly one argument and it must be passed by reference */` |
|      23 | 2926 | `	if( nArg != 1 ){` |
|       8 | 2927 | `		return PH7_VmThrowException(pCtx,` |
|       - | 2928 | `			"ArgumentCountError",` |
|       - | 2929 | `			"array_pop() expects exactly 1 argument, %d given",` |
|       2 | 2930 | `			nArg` |
|       - | 2931 | `			);` |
|       - | 2932 | `	}` |
|       - | 2933 | `	/* Passing a constant (including literals) or non-variable triggers the same` |
|       - | 2934 | `	 * error message as official PHP. Check the index to detect constants. */` |
|      18 | 2935 | `	if( apArg[0]->nIdx == SXU32_HIGH ){` |
|       6 | 2936 | `		return PH7_VmThrowException(pCtx,` |
|       - | 2937 | `			"Error",` |
|       - | 2938 | `			"array_pop(): Argument #1 ($array) could not be passed by reference"` |
|       - | 2939 | `			);` |
|       - | 2940 | `	}` |
|       - | 2941 | `	/* Make sure we are dealing with a valid hashmap */` |
|      12 | 2942 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|       4 | 2943 | `		return PH7_VmThrowException(pCtx,` |
|       - | 2944 | `			"TypeError",` |
|       - | 2945 | `			"array_pop(): Argument #1 ($array) must be of type array, %s given",` |
|       1 | 2946 | `			ph7_type_name(apArg[0])` |
|       - | 2947 | `			);` |
|       - | 2948 | `	}` |
|       9 | 2949 | `	PH7_HashmapCowSeparate(pCtx->pVm, apArg[0]);` |
|       9 | 2950 | `	pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|       9 | 2951 | `	if( pMap->nEntry < 1 ){` |
|       - | 2952 | `		/* Nothing to pop,return NULL */` |
|       3 | 2953 | `		ph7_result_null(pCtx);` |
|       2 | 2954 | `	}else{` |
|       7 | 2955 | `		ph7_hashmap_node *pLast = pMap->pLast;` |
|       - | 2956 | `		ph7_value *pObj;` |
|       7 | 2957 | `		pObj = HashmapExtractNodeValue(pLast);` |
|       7 | 2958 | `		if( pObj ){` |
|       - | 2959 | `			/* Node value */` |
|       7 | 2960 | `			ph7_result_value(pCtx,pObj);` |
|       - | 2961 | `			/* Unlink the node */` |
|       7 | 2962 | `			PH7_HashmapUnlinkNode(pLast,TRUE);` |
|       4 | 2963 | `		}else{` |
|     ! 0 | 2964 | `			ph7_result_null(pCtx);` |
|       - | 2965 | `		}` |
|       - | 2966 | `		/* Reset the cursor */` |
|       7 | 2967 | `		pMap->pCur = pMap->pFirst;` |
|       - | 2968 | `	}` |
|       9 | 2969 | `	return PH7_OK;` |
|      14 | 2970 | `}` |
|       - | 2971 | `/*` |
|       - | 2972 | ` * int array_push($array,$var,...)` |
|       - | 2973 | ` *   Push one or more elements onto the end of array. (Stack insertion)` |
|       - | 2974 | ` * Parameters` |
|       - | 2975 | ` *  array` |
|       - | 2976 | ` *    The input array.` |
|       - | 2977 | ` *  var` |
|       - | 2978 | ` *   On or more value to push.` |
|       - | 2979 | ` * Return` |
|       - | 2980 | ` *  New array count (including old items).` |
|       - | 2981 | ` */` |
|      24 | 2982 | `static int ph7_hashmap_push(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       5 | 2983 | `{` |
|       - | 2984 | `	ph7_hashmap *pMap;` |
|       - | 2985 | `	sxi32 rc;` |
|       - | 2986 | `	int i;` |
|      29 | 2987 | `	if( nArg < 1 ){` |
|       4 | 2988 | `		return PH7_VmThrowException(pCtx,` |
|       - | 2989 | `			"ArgumentCountError",` |
|       - | 2990 | `			"array_push() expects at least 1 argument, %d given",` |
|       1 | 2991 | `			nArg` |
|       - | 2992 | `			);` |
|       - | 2993 | `	}` |
|       - | 2994 | `	/* Passing a constant (including literals) or non-variable triggers the same` |
|       - | 2995 | `	 * error message as official PHP. Check the index to detect constants. */` |
|      26 | 2996 | `	if( apArg[0]->nIdx == SXU32_HIGH ){` |
|       6 | 2997 | `		return PH7_VmThrowException(pCtx,` |
|       - | 2998 | `			"Error",` |
|       - | 2999 | `			"array_push(): Argument #1 ($array) could not be passed by reference"` |
|       - | 3000 | `			);` |
|       - | 3001 | `	}` |
|       - | 3002 | `	/* Make sure we are dealing with a valid hashmap */` |
|      20 | 3003 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|       4 | 3004 | `		return PH7_VmThrowException(pCtx,` |
|       - | 3005 | `			"TypeError",` |
|       - | 3006 | `			"array_push(): Argument #1 ($array) must be of type array, %s given",` |
|       1 | 3007 | `			ph7_type_name(apArg[0])` |
|       - | 3008 | `			);` |
|       - | 3009 | `	}` |
|       - | 3010 | `	/* Point to the internal representation of the input hashmap */` |
|      18 | 3011 | `	PH7_HashmapCowSeparate(pCtx->pVm, apArg[0]);` |
|      18 | 3012 | `	pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|       - | 3013 | `	/* Start pushing given values */` |
|      34 | 3014 | `	for( i = 1 ; i < nArg ; ++i ){` |
|      20 | 3015 | `		rc = PH7_HashmapInsert(pMap,0,apArg[i]);` |
|      20 | 3016 | `		if( rc != SXRET_OK ){` |
|       3 | 3017 | `			if( rc == PH7_EXCEPTION \|\| rc == PH7_ABORT ){` |
|       - | 3018 | `				/* Saturated-append Error (php: array_push throws, no result) */` |
|       3 | 3019 | `				return rc;` |
|       - | 3020 | `			}` |
|     ! 0 | 3021 | `			break;` |
|       - | 3022 | `		}` |
|       9 | 3023 | `	}` |
|       - | 3024 | `	/* Return the new count */` |
|      15 | 3025 | `	ph7_result_int64(pCtx,(sxi64)pMap->nEntry);` |
|      15 | 3026 | `	return PH7_OK;` |
|      17 | 3027 | `}` |
|       - | 3028 | `/*` |
|       - | 3029 | ` * value array_shift(array $array)` |
|       - | 3030 | ` *   Shift an element off the beginning of array.` |
|       - | 3031 | ` * Parameter` |
|       - | 3032 | ` *  The array to get the value from.` |
|       - | 3033 | ` * Return` |
|       - | 3034 | ` *  Shifted value or NULL on failure.` |
|       - | 3035 | ` */` |
|      38 | 3036 | `static int ph7_hashmap_shift(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       5 | 3037 | `{` |
|       - | 3038 | `	ph7_hashmap *pMap;` |
|       - | 3039 | `	/* PHP requires exactly one argument and it must be passed by reference */` |
|      43 | 3040 | `	if( nArg != 1 ){` |
|       8 | 3041 | `		return PH7_VmThrowException(pCtx,` |
|       - | 3042 | `			"ArgumentCountError",` |
|       - | 3043 | `			"array_shift() expects exactly 1 argument, %d given",` |
|       2 | 3044 | `			nArg` |
|       - | 3045 | `			);` |
|       - | 3046 | `	}` |
|       - | 3047 | `	/* Detect constants or literals, which cannot be passed by reference. */` |
|      39 | 3048 | `	if( apArg[0]->nIdx == SXU32_HIGH ){` |
|       6 | 3049 | `		return PH7_VmThrowException(pCtx,` |
|       - | 3050 | `			"Error",` |
|       - | 3051 | `			"array_shift(): Argument #1 ($array) could not be passed by reference"` |
|       - | 3052 | `			);` |
|       - | 3053 | `	}` |
|       - | 3054 | `	/* Make sure we are dealing with a valid hashmap */` |
|      35 | 3055 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|       4 | 3056 | `		return PH7_VmThrowException(pCtx,` |
|       - | 3057 | `			"TypeError",` |
|       - | 3058 | `			"array_shift(): Argument #1 ($array) must be of type array, %s given",` |
|       1 | 3059 | `			ph7_type_name(apArg[0])` |
|       - | 3060 | `			);` |
|       - | 3061 | `	}` |
|       - | 3062 | `	/* Point to the internal representation of the hashmap */` |
|      33 | 3063 | `	PH7_HashmapCowSeparate(pCtx->pVm, apArg[0]);` |
|      33 | 3064 | `	pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|      33 | 3065 | `	if( pMap->nEntry < 1 ){` |
|       - | 3066 | `		/* Empty hashmap,return NULL */` |
|       3 | 3067 | `		ph7_result_null(pCtx);` |
|       2 | 3068 | `	}else{` |
|      31 | 3069 | `		ph7_hashmap_node *pEntry = pMap->pFirst;` |
|       - | 3070 | `		ph7_value *pObj;` |
|       - | 3071 | `		sxu32 n;` |
|      31 | 3072 | `		pObj = HashmapExtractNodeValue(pEntry);` |
|      31 | 3073 | `		if( pObj ){` |
|       - | 3074 | `			/* Node value */` |
|      31 | 3075 | `			ph7_result_value(pCtx,pObj);` |
|       - | 3076 | `			/* Unlink the first node */` |
|      31 | 3077 | `			PH7_HashmapUnlinkNode(pEntry,TRUE);` |
|      18 | 3078 | `		}else{` |
|     ! 0 | 3079 | `			ph7_result_null(pCtx);` |
|       - | 3080 | `		}` |
|       - | 3081 | `		/* Rehash all int keys */` |
|      31 | 3082 | `		n = pMap->nEntry;` |
|      31 | 3083 | `		pEntry = pMap->pFirst;` |
|      31 | 3084 | `		pMap->iNextIdx = 0; /* Reset the automatic index */` |
|      40 | 3085 | `		for(;;){` |
|      85 | 3086 | `			if( n < 1 ){` |
|      31 | 3087 | `				break;` |
|       - | 3088 | `			}` |
|      59 | 3089 | `			if( pEntry->iType == HASHMAP_INT_NODE ){` |
|      59 | 3090 | `				HashmapRehashIntNode(pEntry);` |
|      27 | 3091 | `			}` |
|       - | 3092 | `			/* Point to the next entry */` |
|      59 | 3093 | `			pEntry = pEntry->pPrev; /* Reverse link */` |
|      59 | 3094 | `			n--;` |
|       5 | 3095 | `		}` |
|       - | 3096 | `		/* Reset the cursor */` |
|      31 | 3097 | `		pMap->pCur = pMap->pFirst;` |
|       - | 3098 | `	}` |
|      33 | 3099 | `	return PH7_OK;` |
|      24 | 3100 | `}` |
|       - | 3101 | `/*` |
|       - | 3102 | ` * Extract the node cursor value.` |
|       - | 3103 | ` */` |
|      28 | 3104 | `static sxi32 HashmapCurrentValue(ph7_context *pCtx,ph7_hashmap *pMap,int iDirection)` |
|       1 | 3105 | `{` |
|      29 | 3106 | `	ph7_hashmap_node *pCur = pMap->pCur;` |
|       - | 3107 | `	ph7_value *pVal;` |
|      29 | 3108 | `	if( pCur == 0 ){` |
|       - | 3109 | `		/* Cursor does not point to anything,return FALSE */` |
|     ! 0 | 3110 | `		ph7_result_bool(pCtx,0);` |
|     ! 0 | 3111 | `		return PH7_OK;` |
|       - | 3112 | `	}` |
|      29 | 3113 | `	if( iDirection != 0 ){` |
|      11 | 3114 | `		if( iDirection > 0 ){` |
|       - | 3115 | `			/* Point to the next entry */` |
|       9 | 3116 | `			pMap->pCur = pCur->pPrev; /* Reverse link */` |
|       9 | 3117 | `			pCur = pMap->pCur;` |
|       5 | 3118 | `		}else{` |
|       - | 3119 | `			/* Point to the previous entry */` |
|       3 | 3120 | `			pMap->pCur = pCur->pNext; /* Reverse link */` |
|       3 | 3121 | `			pCur = pMap->pCur;` |
|       - | 3122 | `		}` |
|      11 | 3123 | `		if( pCur == 0 ){` |
|       - | 3124 | `			/* End of input reached,return FALSE */` |
|     ! 0 | 3125 | `			ph7_result_bool(pCtx,0);` |
|     ! 0 | 3126 | `			return PH7_OK;` |
|       - | 3127 | `		}` |
|       5 | 3128 | `	}` |
|       - | 3129 | `	/* Point to the desired element */` |
|      29 | 3130 | `	pVal = HashmapExtractNodeValue(pCur);` |
|      29 | 3131 | `	if( pVal ){` |
|      29 | 3132 | `		ph7_result_value(pCtx,pVal);` |
|      15 | 3133 | `	}else{` |
|     ! 0 | 3134 | `		ph7_result_bool(pCtx,0);` |
|       - | 3135 | `	}` |
|      29 | 3136 | `	return PH7_OK;` |
|      15 | 3137 | `}` |
|       - | 3138 | `/*` |
|       - | 3139 | ` * value current(array $array)` |
|       - | 3140 | ` *  Return the current element in an array.` |
|       - | 3141 | ` * Parameter` |
|       - | 3142 | ` *  $input: The input array.` |
|       - | 3143 | ` * Return` |
|       - | 3144 | ` *  The current() function simply returns the value of the array element that's currently` |
|       - | 3145 | ` *  being pointed to by the internal pointer. It does not move the pointer in any way.` |
|       - | 3146 | ` *  If the internal pointer points beyond the end of the elements list or the array` |
|       - | 3147 | ` *  is empty, current() returns FALSE.` |
|       - | 3148 | ` */` |
|      12 | 3149 | `static int ph7_hashmap_current(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       1 | 3150 | `{` |
|      13 | 3151 | `	if( nArg < 1 ){` |
|       - | 3152 | `		/* Missing arguments,return FALSE */` |
|     ! 0 | 3153 | `		ph7_result_bool(pCtx,0);` |
|     ! 0 | 3154 | `		return PH7_OK;` |
|       - | 3155 | `	}` |
|       - | 3156 | `	/* Make sure we are dealing with a valid hashmap */` |
|      13 | 3157 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|       - | 3158 | `		/* Invalid argument,return FALSE */` |
|     ! 0 | 3159 | `		ph7_result_bool(pCtx,0);` |
|     ! 0 | 3160 | `		return PH7_OK;` |
|       - | 3161 | `	}` |
|      13 | 3162 | `	HashmapCurrentValue(&(*pCtx),(ph7_hashmap *)apArg[0]->x.pOther,0);` |
|      13 | 3163 | `	return PH7_OK;` |
|       7 | 3164 | `}` |
|       - | 3165 | `/*` |
|       - | 3166 | ` * value next(array $input)` |
|       - | 3167 | ` *  Advance the internal array pointer of an array.` |
|       - | 3168 | ` * Parameter` |
|       - | 3169 | ` *  $input: The input array.` |
|       - | 3170 | ` * Return` |
|       - | 3171 | ` *  next() behaves like current(), with one difference. It advances the internal array` |
|       - | 3172 | ` *  pointer one place forward before returning the element value. That means it returns` |
|       - | 3173 | ` *  the next array value and advances the internal array pointer by one.` |
|       - | 3174 | ` */` |
|       8 | 3175 | `static int ph7_hashmap_next(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       1 | 3176 | `{` |
|       9 | 3177 | `	if( nArg < 1 ){` |
|       - | 3178 | `		/* Missing arguments,return FALSE */` |
|     ! 0 | 3179 | `		ph7_result_bool(pCtx,0);` |
|     ! 0 | 3180 | `		return PH7_OK;` |
|       - | 3181 | `	}` |
|       - | 3182 | `	/* Make sure we are dealing with a valid hashmap */` |
|       9 | 3183 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|       - | 3184 | `		/* Invalid argument,return FALSE */` |
|     ! 0 | 3185 | `		ph7_result_bool(pCtx,0);` |
|     ! 0 | 3186 | `		return PH7_OK;` |
|       - | 3187 | `	}` |
|       9 | 3188 | `	HashmapCurrentValue(&(*pCtx),(ph7_hashmap *)apArg[0]->x.pOther,1);` |
|       9 | 3189 | `	return PH7_OK;` |
|       5 | 3190 | `}` |
|       - | 3191 | `/*` |
|       - | 3192 | ` * value prev(array $input)` |
|       - | 3193 | ` *  Rewind the internal array pointer.` |
|       - | 3194 | ` * Parameter` |
|       - | 3195 | ` *  $input: The input array.` |
|       - | 3196 | ` * Return` |
|       - | 3197 | ` *  Returns the array value in the previous place that's pointed` |
|       - | 3198 | ` *  to by the internal array pointer, or FALSE if there are no more` |
|       - | 3199 | ` *  elements.` |
|       - | 3200 | ` */` |
|       2 | 3201 | `static int ph7_hashmap_prev(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       1 | 3202 | `{` |
|       3 | 3203 | `	if( nArg < 1 ){` |
|       - | 3204 | `		/* Missing arguments,return FALSE */` |
|     ! 0 | 3205 | `		ph7_result_bool(pCtx,0);` |
|     ! 0 | 3206 | `		return PH7_OK;` |
|       - | 3207 | `	}` |
|       - | 3208 | `	/* Make sure we are dealing with a valid hashmap */` |
|       3 | 3209 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|       - | 3210 | `		/* Invalid argument,return FALSE */` |
|     ! 0 | 3211 | `		ph7_result_bool(pCtx,0);` |
|     ! 0 | 3212 | `		return PH7_OK;` |
|       - | 3213 | `	}` |
|       3 | 3214 | `	HashmapCurrentValue(&(*pCtx),(ph7_hashmap *)apArg[0]->x.pOther,-1);` |
|       3 | 3215 | `	return PH7_OK;` |
|       2 | 3216 | `}` |
|       - | 3217 | `/*` |
|       - | 3218 | ` * value end(array $input)` |
|       - | 3219 | ` *  Set the internal pointer of an array to its last element.` |
|       - | 3220 | ` * Parameter` |
|       - | 3221 | ` *  $input: The input array.` |
|       - | 3222 | ` * Return` |
|       - | 3223 | ` *  Returns the value of the last element or FALSE for empty array.` |
|       - | 3224 | ` */` |
|       2 | 3225 | `static int ph7_hashmap_end(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       1 | 3226 | `{` |
|       - | 3227 | `	ph7_hashmap *pMap;` |
|       3 | 3228 | `	if( nArg < 1 ){` |
|       - | 3229 | `		/* Missing arguments,return FALSE */` |
|     ! 0 | 3230 | `		ph7_result_bool(pCtx,0);` |
|     ! 0 | 3231 | `		return PH7_OK;` |
|       - | 3232 | `	}` |
|       - | 3233 | `	/* Make sure we are dealing with a valid hashmap */` |
|       3 | 3234 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|       - | 3235 | `		/* Invalid argument,return FALSE */` |
|     ! 0 | 3236 | `		ph7_result_bool(pCtx,0);` |
|     ! 0 | 3237 | `		return PH7_OK;` |
|       - | 3238 | `	}` |
|       - | 3239 | `	/* Point to the internal representation of the input hashmap */` |
|       3 | 3240 | `	pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|       - | 3241 | `	/* Point to the last node */` |
|       3 | 3242 | `	pMap->pCur = pMap->pLast;` |
|       - | 3243 | `	/* Return the last node value */` |
|       3 | 3244 | `	HashmapCurrentValue(&(*pCtx),pMap,0);` |
|       3 | 3245 | `	return PH7_OK;` |
|       2 | 3246 | `}` |
|       - | 3247 | `/*` |
|       - | 3248 | ` * value reset(array $array )` |
|       - | 3249 | ` *  Set the internal pointer of an array to its first element.` |
|       - | 3250 | ` * Parameter` |
|       - | 3251 | ` *  $input: The input array.` |
|       - | 3252 | ` * Return` |
|       - | 3253 | ` *  Returns the value of the first array element,or FALSE if the array is empty.` |
|       - | 3254 | ` */` |
|       4 | 3255 | `static int ph7_hashmap_reset(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       1 | 3256 | `{` |
|       - | 3257 | `	ph7_hashmap *pMap;` |
|       5 | 3258 | `	if( nArg < 1 ){` |
|       - | 3259 | `		/* Missing arguments,return FALSE */` |
|     ! 0 | 3260 | `		ph7_result_bool(pCtx,0);` |
|     ! 0 | 3261 | `		return PH7_OK;` |
|       - | 3262 | `	}` |
|       - | 3263 | `	/* Make sure we are dealing with a valid hashmap */` |
|       5 | 3264 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|       - | 3265 | `		/* Invalid argument,return FALSE */` |
|     ! 0 | 3266 | `		ph7_result_bool(pCtx,0);` |
|     ! 0 | 3267 | `		return PH7_OK;` |
|       - | 3268 | `	}` |
|       - | 3269 | `	/* Point to the internal representation of the input hashmap */` |
|       5 | 3270 | `	pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|       - | 3271 | `	/* Point to the first node */` |
|       5 | 3272 | `	pMap->pCur = pMap->pFirst;` |
|       - | 3273 | `	/* Return the last node value if available */` |
|       5 | 3274 | `	HashmapCurrentValue(&(*pCtx),pMap,0);` |
|       5 | 3275 | `	return PH7_OK;` |
|       3 | 3276 | `}` |
|       - | 3277 | `/*` |
|       - | 3278 | ` * value key(array $array)` |
|       - | 3279 | ` *   Fetch a key from an array` |
|       - | 3280 | ` * Parameter` |
|       - | 3281 | ` *  $input` |
|       - | 3282 | ` *   The input array.` |
|       - | 3283 | ` * Return` |
|       - | 3284 | ` *  The key() function simply returns the key of the array element that's currently` |
|       - | 3285 | ` *  being pointed to by the internal pointer. It does not move the pointer in any way.` |
|       - | 3286 | ` *  If the internal pointer points beyond the end of the elements list or the array` |
|       - | 3287 | ` *  is empty, key() returns NULL.` |
|       - | 3288 | ` */` |
|       4 | 3289 | `static int ph7_hashmap_simple_key(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       1 | 3290 | `{` |
|       - | 3291 | `	ph7_hashmap_node *pCur;` |
|       - | 3292 | `	ph7_hashmap *pMap;` |
|       5 | 3293 | `	if( nArg < 1 ){` |
|       - | 3294 | `		/* Missing arguments,return NULL */` |
|     ! 0 | 3295 | `		ph7_result_null(pCtx);` |
|     ! 0 | 3296 | `		return PH7_OK;` |
|       - | 3297 | `	}` |
|       - | 3298 | `	/* Make sure we are dealing with a valid hashmap */` |
|       5 | 3299 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|       - | 3300 | `		/* Invalid argument,return NULL */` |
|     ! 0 | 3301 | `		ph7_result_null(pCtx);` |
|     ! 0 | 3302 | `		return PH7_OK;` |
|       - | 3303 | `	}` |
|       5 | 3304 | `	pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|       5 | 3305 | `	pCur = pMap->pCur;` |
|       5 | 3306 | `	if( pCur == 0 ){` |
|       - | 3307 | `		/* Cursor does not point to anything,return NULL */` |
|     ! 0 | 3308 | `		ph7_result_null(pCtx);` |
|     ! 0 | 3309 | `		return PH7_OK;` |
|       - | 3310 | `	}` |
|       5 | 3311 | `	if( pCur->iType == HASHMAP_INT_NODE){` |
|       - | 3312 | `		/* Key is integer */` |
|     ! 0 | 3313 | `		ph7_result_int64(pCtx,pCur->xKey.iKey);` |
|     ! 0 | 3314 | `	}else{` |
|       - | 3315 | `		/* Key is blob */` |
|       7 | 3316 | `		ph7_result_string(pCtx,` |
|       4 | 3317 | `			(const char *)SyBlobData(&pCur->xKey.sKey),(int)SyBlobLength(&pCur->xKey.sKey));` |
|       - | 3318 | `	}` |
|       5 | 3319 | `	return PH7_OK;` |
|       3 | 3320 | `}` |
|       - | 3321 | `/*` |
|       - | 3322 | ` * array each(array $input)` |
|       - | 3323 | ` *  Return the current key and value pair from an array and advance the array cursor.` |
|       - | 3324 | ` * Parameter` |
|       - | 3325 | ` *  $input` |
|       - | 3326 | ` *    The input array.` |
|       - | 3327 | ` * Return` |
|       - | 3328 | ` *  Returns the current key and value pair from the array array. This pair is returned` |
|       - | 3329 | ` *  in a four-element array, with the keys 0, 1, key, and value. Elements 0 and key` |
|       - | 3330 | ` *  contain the key name of the array element, and 1 and value contain the data.` |
|       - | 3331 | ` *  If the internal pointer for the array points past the end of the array contents` |
|       - | 3332 | ` *  each() returns FALSE.` |
|       - | 3333 | ` */` |
|      22 | 3334 | `static int ph7_hashmap_each(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       1 | 3335 | `{` |
|       - | 3336 | `	ph7_hashmap_node *pCur;` |
|       - | 3337 | `	ph7_hashmap *pMap;` |
|       - | 3338 | `	ph7_value *pArray;` |
|       - | 3339 | `	ph7_value *pVal;` |
|       - | 3340 | `	ph7_value sKey;` |
|      23 | 3341 | `	if( nArg < 1 ){` |
|       - | 3342 | `		/* Missing arguments,return FALSE */` |
|     ! 0 | 3343 | `		ph7_result_bool(pCtx,0);` |
|     ! 0 | 3344 | `		return PH7_OK;` |
|       - | 3345 | `	}` |
|       - | 3346 | `	/* Make sure we are dealing with a valid hashmap */` |
|      23 | 3347 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|       - | 3348 | `		/* Invalid argument,return FALSE */` |
|     ! 0 | 3349 | `		ph7_result_bool(pCtx,0);` |
|     ! 0 | 3350 | `		return PH7_OK;` |
|       - | 3351 | `	}` |
|       - | 3352 | `	/* Point to the internal representation that describe the input hashmap */` |
|      23 | 3353 | `	pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|      23 | 3354 | `	if( pMap->pCur == 0 ){` |
|       - | 3355 | `		/* Cursor does not point to anything,return FALSE */` |
|       9 | 3356 | `		ph7_result_bool(pCtx,0);` |
|       9 | 3357 | `		return PH7_OK;` |
|       - | 3358 | `	}` |
|      15 | 3359 | `	pCur = pMap->pCur;` |
|       - | 3360 | `	/* Create a new array */` |
|      15 | 3361 | `	pArray = ph7_context_new_array(pCtx);` |
|      15 | 3362 | `	if( pArray == 0 ){` |
|     ! 0 | 3363 | `		ph7_result_bool(pCtx,0);` |
|     ! 0 | 3364 | `		return PH7_OK;` |
|       - | 3365 | `	}` |
|      15 | 3366 | `	pVal = HashmapExtractNodeValue(pCur);` |
|       - | 3367 | `	/* Insert the current value */` |
|      15 | 3368 | `	ph7_array_add_intkey_elem(pArray,1,pVal);` |
|      15 | 3369 | `	ph7_array_add_strkey_elem(pArray,"value",pVal);` |
|       - | 3370 | `	/* Make the key */` |
|      15 | 3371 | `	if( pCur->iType == HASHMAP_INT_NODE ){` |
|       7 | 3372 | `		PH7_MemObjInitFromInt(pMap->pVm,&sKey,pCur->xKey.iKey);` |
|       4 | 3373 | `	}else{` |
|       9 | 3374 | `		PH7_MemObjInitFromString(pMap->pVm,&sKey,0);` |
|       9 | 3375 | `		PH7_MemObjStringAppend(&sKey,(const char *)SyBlobData(&pCur->xKey.sKey),SyBlobLength(&pCur->xKey.sKey));` |
|       - | 3376 | `	}` |
|       - | 3377 | `	/* Insert the current key */` |
|      15 | 3378 | `	ph7_array_add_intkey_elem(pArray,0,&sKey);` |
|      15 | 3379 | `	ph7_array_add_strkey_elem(pArray,"key",&sKey);` |
|      15 | 3380 | `	PH7_MemObjRelease(&sKey);` |
|       - | 3381 | `	/* Advance the cursor */` |
|      15 | 3382 | `	pMap->pCur = pCur->pPrev; /* Reverse link */` |
|       - | 3383 | `	/* Return the current entry */` |
|      15 | 3384 | `	ph7_result_value(pCtx,pArray);` |
|      15 | 3385 | `	return PH7_OK;` |
|      12 | 3386 | `}` |
|       - | 3387 | `/*` |
|       - | 3388 | ` * range() — a faithful port of php 8.5's ext/standard/array.c implementation` |
|       - | 3389 | ` * (php_range_process_input + PHP_FUNCTION(range)), so the value semantics,` |
|       - | 3390 | ` * diagnostics, and their ordering are byte-exact: decreasing ranges, float` |
|       - | 3391 | ` * ranges, character ranges, the step/endpoint ValueErrors, the ZPP TypeErrors` |
|       - | 3392 | ` * and null deprecations, and the string-endpoint warnings.` |
|       - | 3393 | ` */` |
|       - | 3394 | `#define PH7_RANGE_HT_MAX_SIZE 1073741824 /* php's HT_MAX_SIZE (2^30 entries) */` |
|       - | 3395 | `/*` |
|       - | 3396 | ` * Endpoint classification, mirroring php_range_process_input's return` |
|       - | 3397 | ` * contract. php returns zval type tags whose ORDER encodes the logic` |
|       - | 3398 | ` * (IS_LONG < IS_DOUBLE < IS_STRING < IS_ARRAY); the >=/< comparisons in` |
|       - | 3399 | ` * ph7_hashmap_range depend on the same ordering here.` |
|       - | 3400 | ` *   RANGE_IN_LONG/DOUBLE : only interpretable as int / float` |
|       - | 3401 | ` *   RANGE_IN_STRING      : only interpretable as a (char-range) string` |
|       - | 3402 | ` *   RANGE_IN_DIGIT       : single-byte numeric string — valid as both a char` |
|       - | 3403 | ` *                          and a number (php returns IS_ARRAY for this)` |
|       - | 3404 | ` */` |
|       - | 3405 | `#define RANGE_IN_ERROR   0` |
|       - | 3406 | `#define RANGE_IN_LONG    1` |
|       - | 3407 | `#define RANGE_IN_DOUBLE  2` |
|       - | 3408 | `#define RANGE_IN_STRING  3` |
|       - | 3409 | `#define RANGE_IN_DIGIT   4` |
|       - | 3410 | `/* IEEE special-value tests: the engine-wide bit-pattern macros from` |
|       - | 3411 | ` * sxtypes.h (via ph7int.h) — same ones the printf/serialize paths use. */` |
|       - | 3412 | `/*` |
|       - | 3413 | ` * The type name php's ZPP prints after "must be of type ..., X given":` |
|       - | 3414 | ` * the concrete class name for objects, the usual type name otherwise.` |
|       - | 3415 | ` */` |
|       8 | 3416 | `static const char * RangeArgTypeName(ph7_value *pVal,char *zBuf,sxu32 nBufLen)` |
|       1 | 3417 | `{` |
|       9 | 3418 | `	if( pVal->iFlags & MEMOBJ_OBJ ){` |
|       3 | 3419 | `		ph7_class_instance *pThis = (ph7_class_instance *)pVal->x.pOther;` |
|       3 | 3420 | `		sxu32 n = SXMIN(pThis->pClass->sName.nByte,nBufLen - 1);` |
|       3 | 3421 | `		SyMemcpy((const void *)pThis->pClass->sName.zString,zBuf,n);` |
|       3 | 3422 | `		zBuf[n] = 0;` |
|       3 | 3423 | `		return zBuf;` |
|       - | 3424 | `	}` |
|       7 | 3425 | `	return ph7_type_name(pVal);` |
|       5 | 3426 | `}` |
|       - | 3427 | `/*` |
|       - | 3428 | ` * Classify a string with php's is_numeric_string() grammar:` |
|       - | 3429 | ` *   [ws] [sign] ( D+ [ . D* ] \| . D+ ) [ (e\|E) [sign] D+ ] [ws]` |
|       - | 3430 | ` * — the whole string must be consumed; hex/binary/"INF"/"NAN" are NOT` |
|       - | 3431 | ` * numeric. Returns RANGE_IN_LONG with *pLong set, RANGE_IN_DOUBLE with` |
|       - | 3432 | ` * *pDouble set (a fractional/exponent form, or an integer too wide for an` |
|       - | 3433 | ` * sxi64 — php reclassifies those as float), or RANGE_IN_ERROR when the` |
|       - | 3434 | ` * string is not numeric. The float value comes from libc strtod, like` |
|       - | 3435 | ` * php's zend_strtod (byte-exact-floats rule). zIn must be NUL-terminated` |
|       - | 3436 | ` * at zIn[nLen] — ph7_value_to_string guarantees this (SyBlobNullAppend) —` |
|       - | 3437 | ` * so strtod can parse it in place once the grammar has validated it.` |
|       - | 3438 | ` */` |
|     104 | 3439 | `static sxu8 RangeStrToNumber(const char *zIn,sxu32 nLen,sxi64 *pLong,double *pDouble)` |
|       1 | 3440 | `{` |
|     105 | 3441 | `	const char *z = zIn,*zEnd = &zIn[nLen];` |
|     105 | 3442 | `	sxu64 uVal = 0;` |
|     105 | 3443 | `	int bNeg = 0,bDigit = 0,bReal = 0,bOverflow = 0;` |
|     107 | 3444 | `	while( z < zEnd && (unsigned char)z[0] < 0xc0 && SyisSpace(z[0]) ){ z++; }` |
|     105 | 3445 | `	if( z < zEnd && (z[0] == '+' \|\| z[0] == '-') ){` |
|     ! 0 | 3446 | `		bNeg = (z[0] == '-');` |
|     ! 0 | 3447 | `		z++;` |
|     ! 0 | 3448 | `	}` |
|     147 | 3449 | `	while( z < zEnd && (unsigned char)z[0] < 0xc0 && SyisDigit(z[0]) ){` |
|      43 | 3450 | `		int d = z[0] - '0';` |
|       - | 3451 | `		/* Track overflow past 2^63, the widest magnitude an sxi64 can carry` |
|       - | 3452 | `		 * (as LONG_MIN); overflowing integers become floats like in php. */` |
|      43 | 3453 | `		if( uVal > 922337203685477580ULL \|\| (uVal == 922337203685477580ULL && d > 8) ){` |
|     ! 0 | 3454 | `			bOverflow = 1;` |
|     ! 0 | 3455 | `		}else{` |
|      43 | 3456 | `			uVal = uVal * 10 + (sxu64)d;` |
|       - | 3457 | `		}` |
|      43 | 3458 | `		bDigit = 1;` |
|      43 | 3459 | `		z++;` |
|       1 | 3460 | `	}` |
|     105 | 3461 | `	if( z < zEnd && z[0] == '.' ){` |
|       3 | 3462 | `		bReal = 1;` |
|       3 | 3463 | `		z++;` |
|       5 | 3464 | `		while( z < zEnd && (unsigned char)z[0] < 0xc0 && SyisDigit(z[0]) ){` |
|       3 | 3465 | `			bDigit = 1;` |
|       3 | 3466 | `			z++;` |
|       1 | 3467 | `		}` |
|       1 | 3468 | `	}` |
|       - | 3469 | `	/* At least one mantissa digit required (rejects "", ".", "+", "e5"). */` |
|     105 | 3470 | `	if( !bDigit ){` |
|      51 | 3471 | `		return RANGE_IN_ERROR;` |
|       - | 3472 | `	}` |
|       - | 3473 | `	/* Optional exponent — needs at least one digit (rejects "1e", "1e+"). */` |
|      55 | 3474 | `	if( z < zEnd && (z[0] == 'e' \|\| z[0] == 'E') ){` |
|       3 | 3475 | `		z++;` |
|       3 | 3476 | `		if( z < zEnd && (z[0] == '+' \|\| z[0] == '-') ){ z++; }` |
|       3 | 3477 | `		if( z >= zEnd \|\| (unsigned char)z[0] >= 0xc0 \|\| !SyisDigit(z[0]) ){` |
|     ! 0 | 3478 | `			return RANGE_IN_ERROR;` |
|       - | 3479 | `		}` |
|       3 | 3480 | `		bReal = 1;` |
|       5 | 3481 | `		while( z < zEnd && (unsigned char)z[0] < 0xc0 && SyisDigit(z[0]) ){ z++; }` |
|       1 | 3482 | `	}` |
|       - | 3483 | `	/* Trailing whitespace allowed; anything else means not numeric. */` |
|      55 | 3484 | `	while( z < zEnd && (unsigned char)z[0] < 0xc0 && SyisSpace(z[0]) ){ z++; }` |
|      55 | 3485 | `	if( z != zEnd ){` |
|       3 | 3486 | `		return RANGE_IN_ERROR;` |
|       - | 3487 | `	}` |
|      52 | 3488 | `	if( bOverflow \|\| (!bNeg && uVal > (sxu64)LARGEST_INT64)` |
|      27 | 3489 | `	 \|\| (bNeg && uVal > (sxu64)LARGEST_INT64 + 1) ){` |
|      52 | 3490 | `		bReal = 1;` |
|      52 | 3491 | `	}` |
|      27 | 3492 | `	if( bReal ){` |
|       5 | 3493 | `		*pDouble = strtod(zIn,0);` |
|       5 | 3494 | `		return RANGE_IN_DOUBLE;` |
|       - | 3495 | `	}` |
|       - | 3496 | `	/* Negate in unsigned space so 2^63 lands on LONG_MIN without overflow. */` |
|      23 | 3497 | `	*pLong = bNeg ? (sxi64)((sxu64)0 - uVal) : (sxi64)uVal;` |
|      23 | 3498 | `	return RANGE_IN_LONG;` |
|      40 | 3499 | `}` |
|       - | 3500 | `/*` |
|       - | 3501 | ` * ZPP emulation for $start/$end (php's Z_PARAM_NUMBER_OR_STR, weak mode):` |
|       - | 3502 | ` * reject array/object/resource with php's TypeError, deprecate null (the` |
|       - | 3503 | ` * value then reads as int 0 — *pbNullCoerced). php runs this for all` |
|       - | 3504 | ` * arguments BEFORE any value/domain check, hence the split from` |
|       - | 3505 | ` * RangeProcessInput below. Returns FALSE after throwing (*pRc set).` |
|       - | 3506 | ` */` |
|     262 | 3507 | `static int RangeEndpointZpp(ph7_context *pCtx,ph7_value *pIn,int iArg,const char *zName,int *pbNullCoerced,sxi32 *pRc)` |
|       1 | 3508 | `{` |
|       - | 3509 | `	char zMsg[160];` |
|     263 | 3510 | `	*pRc = PH7_OK;` |
|     263 | 3511 | `	if( pIn->iFlags & (MEMOBJ_HASHMAP\|MEMOBJ_OBJ\|MEMOBJ_RES) ){` |
|       - | 3512 | `		char zType[80];` |
|      10 | 3513 | `		*pRc = PH7_VmThrowException(pCtx,"TypeError",` |
|       - | 3514 | `			"range(): Argument #%d ($%s) must be of type string\|int\|float, %s given",` |
|       3 | 3515 | `			iArg,zName,RangeArgTypeName(pIn,zType,sizeof(zType)));` |
|       7 | 3516 | `		return FALSE;` |
|       - | 3517 | `	}` |
|     257 | 3518 | `	if( pIn->iFlags & MEMOBJ_NULL ){` |
|       7 | 3519 | `		SyBufferFormat(zMsg,sizeof(zMsg),` |
|       - | 3520 | `			"range(): Passing null to parameter #%d ($%s) of type string\|int\|float is deprecated",` |
|       2 | 3521 | `			iArg,zName);` |
|       5 | 3522 | `		PH7_VmThrowError(pCtx->pVm,0,E_DEPRECATED,zMsg);` |
|       5 | 3523 | `		*pbNullCoerced = TRUE;` |
|       2 | 3524 | `	}` |
|     257 | 3525 | `	return TRUE;` |
|     132 | 3526 | `}` |
|       - | 3527 | `/*` |
|       - | 3528 | ` * ZPP emulation for $step (php's Z_PARAM_NUMBER, weak mode): int/float pass` |
|       - | 3529 | ` * through, bool coerces to int, null deprecates to int 0 (which then trips` |
|       - | 3530 | ` * the "cannot be 0" ValueError like php), a numeric string coerces to its` |
|       - | 3531 | ` * number, anything else is a TypeError. Returns RANGE_IN_LONG/DOUBLE, or` |
|       - | 3532 | ` * RANGE_IN_ERROR after throwing (*pRc set).` |
|       - | 3533 | ` */` |
|      62 | 3534 | `static sxu8 RangeStepInput(ph7_context *pCtx,ph7_value *pIn,sxi64 *pLong,double *pDouble,sxi32 *pRc)` |
|       1 | 3535 | `{` |
|      63 | 3536 | `	*pRc = PH7_OK;` |
|      63 | 3537 | `	if( pIn->iFlags & (MEMOBJ_HASHMAP\|MEMOBJ_OBJ\|MEMOBJ_RES) ){` |
|       - | 3538 | `		char zType[80];` |
|       4 | 3539 | `		*pRc = PH7_VmThrowException(pCtx,"TypeError",` |
|       - | 3540 | `			"range(): Argument #3 ($step) must be of type int\|float, %s given",` |
|       1 | 3541 | `			RangeArgTypeName(pIn,zType,sizeof(zType)));` |
|       3 | 3542 | `		return RANGE_IN_ERROR;` |
|       - | 3543 | `	}` |
|      61 | 3544 | `	if( pIn->iFlags & MEMOBJ_NULL ){` |
|       3 | 3545 | `		PH7_VmThrowError(pCtx->pVm,0,E_DEPRECATED,` |
|       - | 3546 | `			"range(): Passing null to parameter #3 ($step) of type int\|float is deprecated");` |
|       3 | 3547 | `		*pLong = 0;` |
|       3 | 3548 | `		return RANGE_IN_LONG;` |
|       - | 3549 | `	}` |
|      59 | 3550 | `	if( pIn->iFlags & MEMOBJ_REAL ){` |
|      25 | 3551 | `		*pDouble = ph7_value_to_double(pIn);` |
|      25 | 3552 | `		return RANGE_IN_DOUBLE;` |
|       - | 3553 | `	}` |
|      35 | 3554 | `	if( pIn->iFlags & MEMOBJ_STRING ){` |
|       - | 3555 | `		const char *zStr;` |
|       - | 3556 | `		int nLen;` |
|       - | 3557 | `		sxu8 iKind;` |
|       3 | 3558 | `		zStr = ph7_value_to_string(pIn,&nLen);` |
|       3 | 3559 | `		iKind = RangeStrToNumber(zStr,(sxu32)nLen,pLong,pDouble);` |
|       3 | 3560 | `		if( iKind == RANGE_IN_ERROR ){` |
|       3 | 3561 | `			*pRc = PH7_VmThrowException(pCtx,"TypeError",` |
|       - | 3562 | `				"range(): Argument #3 ($step) must be of type int\|float, string given");` |
|       1 | 3563 | `		}` |
|       3 | 3564 | `		return iKind;` |
|       - | 3565 | `	}` |
|       - | 3566 | `	/* int / bool */` |
|      33 | 3567 | `	*pLong = ph7_value_to_int64(pIn);` |
|      33 | 3568 | `	return RANGE_IN_LONG;` |
|      32 | 3569 | `}` |
|       - | 3570 | `/*` |
|       - | 3571 | ` * php_range_process_input port: resolve $start/$end into a number and/or a` |
|       - | 3572 | ` * char-range byte, emitting php's exact warnings (empty string, multi-byte` |
|       - | 3573 | ` * string) and ValueErrors (INF/NAN). Returns a RANGE_IN_* code, or` |
|       - | 3574 | ` * RANGE_IN_ERROR after throwing (*pRc set).` |
|       - | 3575 | ` */` |
|     220 | 3576 | `static sxu8 RangeProcessInput(ph7_context *pCtx,ph7_value *pIn,int iArg,const char *zName,` |
|       - | 3577 | `	int bNullCoerced,sxi64 *pLong,double *pDouble,unsigned char *pChar,sxi32 *pRc)` |
|       1 | 3578 | `{` |
|       - | 3579 | `	char zMsg[160];` |
|       - | 3580 | `	double r;` |
|     221 | 3581 | `	*pRc = PH7_OK;` |
|     221 | 3582 | `	if( bNullCoerced ){` |
|       - | 3583 | `		/* ZPP already deprecated the null; it reads as int 0. */` |
|       5 | 3584 | `		*pLong = 0;` |
|       5 | 3585 | `		*pDouble = 0.0;` |
|       5 | 3586 | `		return RANGE_IN_LONG;` |
|       - | 3587 | `	}` |
|     217 | 3588 | `	if( pIn->iFlags & MEMOBJ_REAL ){` |
|      21 | 3589 | `		r = ph7_value_to_double(pIn);` |
|      12 | 3590 | `check_dval:` |
|      25 | 3591 | `		if( PH7_IS_INF(r) ){` |
|       7 | 3592 | `			*pRc = PH7_VmThrowException(pCtx,"ValueError",` |
|       2 | 3593 | `				"range(): Argument #%d ($%s) must be a finite number, INF provided",iArg,zName);` |
|       5 | 3594 | `			return RANGE_IN_ERROR;` |
|       - | 3595 | `		}` |
|      21 | 3596 | `		if( PH7_IS_NAN(r) ){` |
|       7 | 3597 | `			*pRc = PH7_VmThrowException(pCtx,"ValueError",` |
|       2 | 3598 | `				"range(): Argument #%d ($%s) must be a finite number, NAN provided",iArg,zName);` |
|       5 | 3599 | `			return RANGE_IN_ERROR;` |
|       - | 3600 | `		}` |
|      17 | 3601 | `		*pDouble = r;` |
|      17 | 3602 | `		return RANGE_IN_DOUBLE;` |
|       - | 3603 | `	}` |
|     197 | 3604 | `	if( pIn->iFlags & MEMOBJ_STRING ){` |
|       - | 3605 | `		const char *zStr;` |
|       - | 3606 | `		int nLen;` |
|       - | 3607 | `		sxu8 iKind;` |
|      81 | 3608 | `		zStr = ph7_value_to_string(pIn,&nLen);` |
|      81 | 3609 | `		if( nLen == 0 ){` |
|       7 | 3610 | `			SyBufferFormat(zMsg,sizeof(zMsg),` |
|       2 | 3611 | `				"range(): Argument #%d ($%s) must not be empty, casted to 0",iArg,zName);` |
|       5 | 3612 | `			PH7_VmThrowError(pCtx->pVm,0,PH7_CTX_WARNING,zMsg);` |
|       5 | 3613 | `			*pLong = 0;` |
|       5 | 3614 | `			*pDouble = 0.0;` |
|      41 | 3615 | `			return RANGE_IN_LONG;` |
|       - | 3616 | `		}` |
|      77 | 3617 | `		iKind = RangeStrToNumber(zStr,(sxu32)nLen,pLong,pDouble);` |
|      77 | 3618 | `		if( iKind == RANGE_IN_DOUBLE ){` |
|       5 | 3619 | `			r = *pDouble;` |
|       5 | 3620 | `			goto check_dval;` |
|       - | 3621 | `		}` |
|      73 | 3622 | `		if( iKind == RANGE_IN_LONG ){` |
|      23 | 3623 | `			*pDouble = (double)*pLong;` |
|      23 | 3624 | `			if( nLen == 1 ){` |
|       - | 3625 | `				/* A single numeric digit works as both a char and a number. */` |
|       9 | 3626 | `				*pChar = (unsigned char)zStr[0];` |
|       9 | 3627 | `				return RANGE_IN_DIGIT;` |
|       - | 3628 | `			}` |
|      15 | 3629 | `			return RANGE_IN_LONG;` |
|       - | 3630 | `		}` |
|      51 | 3631 | `		if( nLen != 1 ){` |
|      10 | 3632 | `			SyBufferFormat(zMsg,sizeof(zMsg),` |
|       3 | 3633 | `				"range(): Argument #%d ($%s) must be a single byte, subsequent bytes are ignored",iArg,zName);` |
|       7 | 3634 | `			PH7_VmThrowError(pCtx->pVm,0,PH7_CTX_WARNING,zMsg);` |
|       3 | 3635 | `		}` |
|      51 | 3636 | `		*pChar = (unsigned char)zStr[0];` |
|       - | 3637 | `		/* Fall-back numeric value in case the other argument is not a string. */` |
|      51 | 3638 | `		*pLong = 0;` |
|      51 | 3639 | `		*pDouble = 0.0;` |
|      51 | 3640 | `		return RANGE_IN_STRING;` |
|       - | 3641 | `	}` |
|       - | 3642 | `	/* int / bool */` |
|     117 | 3643 | `	*pLong = ph7_value_to_int64(pIn);` |
|     117 | 3644 | `	*pDouble = (double)*pLong;` |
|     117 | 3645 | `	return RANGE_IN_LONG;` |
|     111 | 3646 | `}` |
|       - | 3647 | `/*` |
|       - | 3648 | ` * The two "supplied range exceeds the maximum array size" ValueErrors.` |
|       - | 3649 | ` * Both php messages print the macro's (start,end) parameters, which its` |
|       - | 3650 | ` * callers pass SWAPPED for a decreasing range — a php quirk kept for` |
|       - | 3651 | ` * byte-parity (callers below pass the values to *print*). The int and` |
|       - | 3652 | ` * float variants differ in wording ("Maximum size: N." vs "Max size: N")` |
|       - | 3653 | ` * exactly like php's two macros.` |
|       - | 3654 | ` */` |
|       6 | 3655 | `static sxi32 RangeLongSizeError(ph7_context *pCtx,sxu64 nCalc,sxi64 iStart,sxi64 iEnd,sxi64 iStep)` |
|       1 | 3656 | `{` |
|      10 | 3657 | `	return PH7_VmThrowException(pCtx,"ValueError",` |
|       - | 3658 | `		"The supplied range exceeds the maximum array size by %qu elements: "` |
|       - | 3659 | `		"start=%qd, end=%qd, step=%qd. Calculated size: %qu. Maximum size: %qu.",` |
|       3 | 3660 | `		nCalc - (sxu64)(PH7_RANGE_HT_MAX_SIZE - 1),iStart,iEnd,iStep,` |
|       3 | 3661 | `		nCalc,(sxu64)PH7_RANGE_HT_MAX_SIZE);` |
|       1 | 3662 | `}` |
|       6 | 3663 | `static sxi32 RangeDoubleSizeError(ph7_context *pCtx,double rCalc,double rStart,double rEnd,double rStep)` |
|       1 | 3664 | `{` |
|       - | 3665 | `	/* Four %.1f doubles can reach ~313 bytes each near DBL_MAX, so format on` |
|       - | 3666 | `	 * the VM heap (auto-released with the call context) rather than parking` |
|       - | 3667 | `	 * ~1.5 KB on the native stack of a small-stack embedded port. */` |
|       7 | 3668 | `	const unsigned int nBuf = 1500;` |
|       7 | 3669 | `	char *zMsg = (char *)ph7_context_alloc_chunk(pCtx,nBuf,FALSE,TRUE/* Auto-release */);` |
|       7 | 3670 | `	if( zMsg == 0 ){` |
|     ! 0 | 3671 | `		return PH7_ContextMemoryError(pCtx);` |
|       - | 3672 | `	}` |
|       7 | 3673 | `	snprintf(zMsg,nBuf,` |
|       - | 3674 | `		"The supplied range exceeds the maximum array size by %.1f elements: "` |
|       - | 3675 | `		"start=%.1f, end=%.1f, step=%.1f. Max size: 1073741824",` |
|       - | 3676 | `		rCalc - (double)PH7_RANGE_HT_MAX_SIZE,rStart,rEnd,rStep);` |
|       7 | 3677 | `	return PH7_VmThrowException(pCtx,"ValueError","%s",zMsg);` |
|       4 | 3678 | `}` |
|       - | 3679 | `/*` |
|       - | 3680 | ` * Set the element container to the next range element and append it to the` |
|       - | 3681 | ` * result array, surfacing allocation failure as the OOM fatal (never a` |
|       - | 3682 | ` * silently-truncated array). One helper per element type so the fill loops` |
|       - | 3683 | ` * below stay one line per iteration.` |
|       - | 3684 | ` */` |
|     334 | 3685 | `static sxi32 RangeAppendInt(ph7_context *pCtx,ph7_value *pArray,ph7_value *pValue,sxi64 iVal)` |
|       1 | 3686 | `{` |
|     335 | 3687 | `	ph7_value_int64(pValue,iVal);` |
|     335 | 3688 | `	if( ph7_array_add_elem(pArray,0/* Automatic index assign*/,pValue) != SXRET_OK ){` |
|     ! 0 | 3689 | `		return PH7_ContextMemoryError(pCtx);` |
|       - | 3690 | `	}` |
|     335 | 3691 | `	return PH7_OK;` |
|     168 | 3692 | `}` |
|      70 | 3693 | `static sxi32 RangeAppendDouble(ph7_context *pCtx,ph7_value *pArray,ph7_value *pValue,double rVal)` |
|       1 | 3694 | `{` |
|      71 | 3695 | `	ph7_value_double(pValue,rVal);` |
|      71 | 3696 | `	if( ph7_array_add_elem(pArray,0,pValue) != SXRET_OK ){` |
|     ! 0 | 3697 | `		return PH7_ContextMemoryError(pCtx);` |
|       - | 3698 | `	}` |
|      71 | 3699 | `	return PH7_OK;` |
|      36 | 3700 | `}` |
|     168 | 3701 | `static sxi32 RangeAppendChar(ph7_context *pCtx,ph7_value *pArray,ph7_value *pValue,char c)` |
|       1 | 3702 | `{` |
|     169 | 3703 | `	ph7_value_string(pValue,&c,1);` |
|     169 | 3704 | `	if( ph7_array_add_elem(pArray,0,pValue) != SXRET_OK ){` |
|     ! 0 | 3705 | `		return PH7_ContextMemoryError(pCtx);` |
|       - | 3706 | `	}` |
|     169 | 3707 | `	ph7_value_reset_string_cursor(pValue);` |
|     169 | 3708 | `	return PH7_OK;` |
|      85 | 3709 | `}` |
|       - | 3710 | `/*` |
|       - | 3711 | ` * array range(string\|int\|float $start,string\|int\|float $end,int\|float $step = 1)` |
|       - | 3712 | ` *  Create an array containing a range of elements.` |
|       - | 3713 | ` * Return` |
|       - | 3714 | ` *  An array of elements from start to end, inclusive; int, float, or` |
|       - | 3715 | ` *  single-character string elements depending on the inputs, like php 8.` |
|       - | 3716 | ` */` |
|     136 | 3717 | `static int ph7_hashmap_range(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       1 | 3718 | `{` |
|       - | 3719 | `	ph7_value *pValue,*pArray;` |
|     137 | 3720 | `	sxi32 rc = PH7_OK;` |
|     137 | 3721 | `	int is_step_double = 0,is_step_negative = 0;` |
|     137 | 3722 | `	double step_double = 1.0;` |
|     137 | 3723 | `	sxi64 step = 1;` |
|       - | 3724 | `	sxu8 start_type,end_type;` |
|     137 | 3725 | `	sxi64 start_long = 0,end_long = 0;` |
|     137 | 3726 | `	double start_double = 0.0,end_double = 0.0;` |
|     137 | 3727 | `	unsigned char cStart = 0,cEnd = 0;` |
|     137 | 3728 | `	int bStartNull = FALSE,bEndNull = FALSE;` |
|       - | 3729 | `	sxu32 i,size;` |
|       - | 3730 |  |
|       - | 3731 | `	/* php ZPP arity: at least 2 (enforced centrally, aBuiltinArity), at most 3. */` |
|     137 | 3732 | `	if( nArg > 3 ){` |
|       4 | 3733 | `		return PH7_VmThrowException(pCtx,"ArgumentCountError",` |
|       1 | 3734 | `			"range() expects at most 3 arguments, %d given",nArg);` |
|       - | 3735 | `	}` |
|     135 | 3736 | `	if( nArg < 2 ){` |
|       - | 3737 | `		/* Defensive only: the central arity table throws before we run. */` |
|     ! 0 | 3738 | `		return PH7_VmThrowException(pCtx,"ArgumentCountError",` |
|     ! 0 | 3739 | `			"range() expects at least 2 arguments, %d given",nArg);` |
|       - | 3740 | `	}` |
|       - | 3741 | `	/* ZPP pass in argument order: type errors and null deprecations fire` |
|       - | 3742 | `	 * before any value/domain check, like php's zend_parse_parameters. */` |
|     135 | 3743 | `	if( !RangeEndpointZpp(pCtx,apArg[0],1,"start",&bStartNull,&rc) ){` |
|       7 | 3744 | `		return rc;` |
|       - | 3745 | `	}` |
|     129 | 3746 | `	if( !RangeEndpointZpp(pCtx,apArg[1],2,"end",&bEndNull,&rc) ){` |
|     ! 0 | 3747 | `		return rc;` |
|       - | 3748 | `	}` |
|     129 | 3749 | `	if( nArg > 2 ){` |
|      63 | 3750 | `		sxu8 iStepKind = RangeStepInput(pCtx,apArg[2],&step,&step_double,&rc);` |
|      63 | 3751 | `		if( iStepKind == RANGE_IN_ERROR ){` |
|       5 | 3752 | `			return rc;` |
|       - | 3753 | `		}` |
|      59 | 3754 | `		if( iStepKind == RANGE_IN_DOUBLE ){` |
|      25 | 3755 | `			if( PH7_IS_INF(step_double) ){` |
|       3 | 3756 | `				return PH7_VmThrowException(pCtx,"ValueError",` |
|       - | 3757 | `					"range(): Argument #3 ($step) must be a finite number, INF provided");` |
|       - | 3758 | `			}` |
|      23 | 3759 | `			if( PH7_IS_NAN(step_double) ){` |
|       3 | 3760 | `				return PH7_VmThrowException(pCtx,"ValueError",` |
|       - | 3761 | `					"range(): Argument #3 ($step) must be a finite number, NAN provided");` |
|       - | 3762 | `			}` |
|       - | 3763 | `			/* We only want positive step values. */` |
|      21 | 3764 | `			if( step_double < 0.0 ){` |
|     ! 0 | 3765 | `				is_step_negative = 1;` |
|     ! 0 | 3766 | `				step_double *= -1;` |
|     ! 0 | 3767 | `			}` |
|       - | 3768 | `			/* zend_dval_to_lval_silent + zend_is_long_compatible: an integral` |
|       - | 3769 | `			 * in-sxi64-range float step behaves as an int (char ranges accept` |
|       - | 3770 | `			 * it, int endpoints stay int); anything else is a float step. */` |
|      21 | 3771 | `			if( step_double < 9223372036854775808.0 ){` |
|      19 | 3772 | `				step = (sxi64)step_double;` |
|      19 | 3773 | `				if( (double)step != step_double ){` |
|      17 | 3774 | `					is_step_double = 1;` |
|       8 | 3775 | `				}` |
|      10 | 3776 | `			}else{` |
|       - | 3777 | ``				/* Casting out-of-range would be UB; `step` stays unread —`` |
|       - | 3778 | `				 * every reader is gated behind !is_step_double. */` |
|       3 | 3779 | `				is_step_double = 1;` |
|       - | 3780 | `			}` |
|      11 | 3781 | `		}else{` |
|       - | 3782 | `			/* We only want positive step values. */` |
|      35 | 3783 | `			if( step < 0 ){` |
|      11 | 3784 | `				if( step == SMALLEST_INT64 ){` |
|       - | 3785 | `					/* -step would overflow */` |
|       4 | 3786 | `					return PH7_VmThrowException(pCtx,"ValueError",` |
|       1 | 3787 | `						"range(): Argument #3 ($step) must be greater than %qd",step);` |
|       - | 3788 | `				}` |
|       9 | 3789 | `				is_step_negative = 1;` |
|       9 | 3790 | `				step = -step;` |
|       4 | 3791 | `			}` |
|      33 | 3792 | `			step_double = (double)step;` |
|       - | 3793 | `		}` |
|      53 | 3794 | `		if( step_double == 0.0 ){` |
|       7 | 3795 | `			return PH7_VmThrowException(pCtx,"ValueError",` |
|       - | 3796 | `				"range(): Argument #3 ($step) cannot be 0");` |
|       - | 3797 | `		}` |
|      23 | 3798 | `	}` |
|     113 | 3799 | `	start_type = RangeProcessInput(pCtx,apArg[0],1,"start",bStartNull,&start_long,&start_double,&cStart,&rc);` |
|     113 | 3800 | `	if( start_type == RANGE_IN_ERROR ){` |
|       5 | 3801 | `		return rc;` |
|       - | 3802 | `	}` |
|     109 | 3803 | `	end_type = RangeProcessInput(pCtx,apArg[1],2,"end",bEndNull,&end_long,&end_double,&cEnd,&rc);` |
|     109 | 3804 | `	if( end_type == RANGE_IN_ERROR ){` |
|       5 | 3805 | `		return rc;` |
|       - | 3806 | `	}` |
|       - | 3807 | `	/* Element container + result array */` |
|     105 | 3808 | `	pValue = ph7_context_new_scalar(pCtx);` |
|     105 | 3809 | `	pArray = ph7_context_new_array(pCtx);` |
|     105 | 3810 | `	if( pValue == 0 \|\| pArray == 0 ){` |
|     ! 0 | 3811 | `		return PH7_ContextMemoryError(pCtx);` |
|       - | 3812 | `	}` |
|       - | 3813 | `	/* If the range is given as strings, generate an array of characters. */` |
|     105 | 3814 | `	if( start_type >= RANGE_IN_STRING \|\| end_type >= RANGE_IN_STRING ){` |
|      37 | 3815 | `		if( start_type < RANGE_IN_STRING \|\| end_type < RANGE_IN_STRING ){` |
|       - | 3816 | `			/* Only one side is a string: the char side converts to 0 (with a` |
|       - | 3817 | `			 * warning unless the numeric side is an ambiguous single digit)` |
|       - | 3818 | `			 * and the range is numeric. */` |
|      15 | 3819 | `			if( start_type < RANGE_IN_STRING ){` |
|       7 | 3820 | `				if( end_type != RANGE_IN_DIGIT ){` |
|       7 | 3821 | `					PH7_VmThrowError(pCtx->pVm,0,PH7_CTX_WARNING,` |
|       - | 3822 | `						"range(): Argument #1 ($start) must be a single byte string if"` |
|       - | 3823 | `						" argument #2 ($end) is a single byte string, argument #2 ($end) converted to 0");` |
|       3 | 3824 | `				}` |
|       7 | 3825 | `				end_type = RANGE_IN_LONG;` |
|       4 | 3826 | `			}else{` |
|       9 | 3827 | `				if( start_type != RANGE_IN_DIGIT ){` |
|       9 | 3828 | `					PH7_VmThrowError(pCtx->pVm,0,PH7_CTX_WARNING,` |
|       - | 3829 | `						"range(): Argument #2 ($end) must be a single byte string if"` |
|       - | 3830 | `						" argument #1 ($start) is a single byte string, argument #1 ($start) converted to 0");` |
|       4 | 3831 | `				}` |
|       9 | 3832 | `				start_type = RANGE_IN_LONG;` |
|       - | 3833 | `			}` |
|      15 | 3834 | `			goto handle_numeric_inputs;` |
|       - | 3835 | `		}` |
|      23 | 3836 | `		if( is_step_double ){` |
|       - | 3837 | `			/* Only emit the warning if one of the inputs is not a numeric digit. */` |
|       5 | 3838 | `			if( start_type == RANGE_IN_STRING \|\| end_type == RANGE_IN_STRING ){` |
|       3 | 3839 | `				PH7_VmThrowError(pCtx->pVm,0,PH7_CTX_WARNING,` |
|       - | 3840 | `					"range(): Argument #3 ($step) must be of type int when generating an array"` |
|       - | 3841 | `					" of characters, inputs converted to 0");` |
|       1 | 3842 | `			}` |
|       5 | 3843 | `			start_type = RANGE_IN_LONG;` |
|       5 | 3844 | `			end_type = RANGE_IN_LONG;` |
|       5 | 3845 | `			goto handle_numeric_inputs;` |
|       - | 3846 | `		}` |
|       - | 3847 | `		/* Generate an array of characters */` |
|      19 | 3848 | `		if( cStart > cEnd ){` |
|       - | 3849 | `			/* Decreasing char range */` |
|       - | 3850 | `			int iCur;` |
|       3 | 3851 | `			if( (sxi64)(cStart - cEnd) < step ){` |
|     ! 0 | 3852 | `				goto boundary_error;` |
|       - | 3853 | `			}` |
|      17 | 3854 | `			for( iCur = (int)cStart ; iCur >= (int)cEnd ; iCur -= (int)step ){` |
|      15 | 3855 | `				if( (rc = RangeAppendChar(pCtx,pArray,pValue,(char)iCur)) != PH7_OK ){` |
|     ! 0 | 3856 | `					return rc;` |
|       - | 3857 | `				}` |
|       8 | 3858 | `			}` |
|      18 | 3859 | `		}else if( cEnd > cStart ){` |
|       - | 3860 | `			/* Increasing char range */` |
|       - | 3861 | `			int iCur;` |
|      15 | 3862 | `			if( is_step_negative ){` |
|       3 | 3863 | `				goto negative_step_error;` |
|       - | 3864 | `			}` |
|      13 | 3865 | `			if( (sxi64)(cEnd - cStart) < step ){` |
|       3 | 3866 | `				goto boundary_error;` |
|       - | 3867 | `			}` |
|     163 | 3868 | `			for( iCur = (int)cStart ; iCur <= (int)cEnd ; iCur += (int)step ){` |
|     153 | 3869 | `				if( (rc = RangeAppendChar(pCtx,pArray,pValue,(char)iCur)) != PH7_OK ){` |
|     ! 0 | 3870 | `					return rc;` |
|       - | 3871 | `				}` |
|      77 | 3872 | `			}` |
|       6 | 3873 | `		}else{` |
|       3 | 3874 | `			if( (rc = RangeAppendChar(pCtx,pArray,pValue,(char)cStart)) != PH7_OK ){` |
|     ! 0 | 3875 | `				return rc;` |
|       - | 3876 | `			}` |
|       - | 3877 | `		}` |
|      15 | 3878 | `		ph7_result_value(pCtx,pArray);` |
|      15 | 3879 | `		return PH7_OK;` |
|       - | 3880 | `	}` |
|      34 | 3881 | `handle_numeric_inputs:` |
|      95 | 3882 | `	if( start_type == RANGE_IN_DOUBLE \|\| end_type == RANGE_IN_DOUBLE \|\| is_step_double ){` |
|       - | 3883 | `		/* Float range */` |
|       - | 3884 | `		double elem,calc;` |
|      25 | 3885 | `		if( start_double > end_double ){` |
|       - | 3886 | `			/* Decreasing float range */` |
|       7 | 3887 | `			if( start_double - end_double < step_double ){` |
|     ! 0 | 3888 | `				goto boundary_error;` |
|       - | 3889 | `			}` |
|       7 | 3890 | `			calc = ((start_double - end_double) / step_double) + 1;` |
|       7 | 3891 | `			if( calc >= (double)PH7_RANGE_HT_MAX_SIZE ){` |
|       - | 3892 | `				/* php prints start/end swapped here (see RangeDoubleSizeError). */` |
|       3 | 3893 | `				return RangeDoubleSizeError(pCtx,calc,end_double,start_double,step_double);` |
|       - | 3894 | `			}` |
|       5 | 3895 | `			size = (sxu32)(calc + 0.5); /* _php_math_round(...,0,HALF_UP) */` |
|      19 | 3896 | `			for( i = 0,elem = start_double ; i < size && elem >= end_double ; ++i,elem = start_double - ((double)i * step_double) ){` |
|      15 | 3897 | `				if( (rc = RangeAppendDouble(pCtx,pArray,pValue,elem)) != PH7_OK ){` |
|     ! 0 | 3898 | `					return rc;` |
|       - | 3899 | `				}` |
|       8 | 3900 | `			}` |
|      21 | 3901 | `		}else if( end_double > start_double ){` |
|       - | 3902 | `			/* Increasing float range */` |
|      17 | 3903 | `			if( is_step_negative ){` |
|     ! 0 | 3904 | `				goto negative_step_error;` |
|       - | 3905 | `			}` |
|      17 | 3906 | `			if( end_double - start_double < step_double ){` |
|       3 | 3907 | `				goto boundary_error;` |
|       - | 3908 | `			}` |
|      15 | 3909 | `			calc = ((end_double - start_double) / step_double) + 1;` |
|      15 | 3910 | `			if( calc >= (double)PH7_RANGE_HT_MAX_SIZE ){` |
|       5 | 3911 | `				return RangeDoubleSizeError(pCtx,calc,start_double,end_double,step_double);` |
|       - | 3912 | `			}` |
|      11 | 3913 | `			size = (sxu32)(calc + 0.5);` |
|      65 | 3914 | `			for( i = 0,elem = start_double ; i < size && elem <= end_double ; ++i,elem = start_double + ((double)i * step_double) ){` |
|      55 | 3915 | `				if( (rc = RangeAppendDouble(pCtx,pArray,pValue,elem)) != PH7_OK ){` |
|     ! 0 | 3916 | `					return rc;` |
|       - | 3917 | `				}` |
|      28 | 3918 | `			}` |
|       6 | 3919 | `		}else{` |
|       3 | 3920 | `			if( (rc = RangeAppendDouble(pCtx,pArray,pValue,start_double)) != PH7_OK ){` |
|     ! 0 | 3921 | `				return rc;` |
|       - | 3922 | `			}` |
|       - | 3923 | `		}` |
|       9 | 3924 | `	}else{` |
|       - | 3925 | `		/* Int range. All arithmetic in unsigned space so a span wider than` |
|       - | 3926 | `		 * LARGEST_INT64 (e.g. -PHP_INT_MAX..PHP_INT_MAX) wraps correctly` |
|       - | 3927 | `		 * instead of overflowing, exactly like php's zend_ulong math. */` |
|      63 | 3928 | `		sxu64 ustep = (sxu64)step;` |
|       - | 3929 | `		sxu64 calc;` |
|      63 | 3930 | `		if( start_long > end_long ){` |
|       - | 3931 | `			/* Decreasing int range */` |
|      19 | 3932 | `			if( (sxu64)start_long - (sxu64)end_long < ustep ){` |
|       3 | 3933 | `				goto boundary_error;` |
|       - | 3934 | `			}` |
|      17 | 3935 | `			calc = ((sxu64)start_long - (sxu64)end_long) / ustep;` |
|      17 | 3936 | `			if( calc >= (sxu64)(PH7_RANGE_HT_MAX_SIZE - 1) ){` |
|       - | 3937 | `				/* php prints start/end swapped here (see RangeLongSizeError). */` |
|       3 | 3938 | `				return RangeLongSizeError(pCtx,calc,end_long,start_long,step);` |
|       - | 3939 | `			}` |
|      15 | 3940 | `			size = (sxu32)(calc + 1);` |
|     101 | 3941 | `			for( i = 0 ; i < size ; ++i ){` |
|      87 | 3942 | `				if( (rc = RangeAppendInt(pCtx,pArray,pValue,(sxi64)((sxu64)start_long - (sxu64)i * ustep))) != PH7_OK ){` |
|     ! 0 | 3943 | `					return rc;` |
|       - | 3944 | `				}` |
|      44 | 3945 | `			}` |
|      52 | 3946 | `		}else if( end_long > start_long ){` |
|       - | 3947 | `			/* Increasing int range */` |
|      39 | 3948 | `			if( is_step_negative ){` |
|       3 | 3949 | `				goto negative_step_error;` |
|       - | 3950 | `			}` |
|      37 | 3951 | `			if( (sxu64)end_long - (sxu64)start_long < ustep ){` |
|       3 | 3952 | `				goto boundary_error;` |
|       - | 3953 | `			}` |
|      35 | 3954 | `			calc = ((sxu64)end_long - (sxu64)start_long) / ustep;` |
|      35 | 3955 | `			if( calc >= (sxu64)(PH7_RANGE_HT_MAX_SIZE - 1) ){` |
|       5 | 3956 | `				return RangeLongSizeError(pCtx,calc,start_long,end_long,step);` |
|       - | 3957 | `			}` |
|      31 | 3958 | `			size = (sxu32)(calc + 1);` |
|     273 | 3959 | `			for( i = 0 ; i < size ; ++i ){` |
|     243 | 3960 | `				if( (rc = RangeAppendInt(pCtx,pArray,pValue,(sxi64)((sxu64)start_long + (sxu64)i * ustep))) != PH7_OK ){` |
|     ! 0 | 3961 | `					return rc;` |
|       - | 3962 | `				}` |
|     122 | 3963 | `			}` |
|      16 | 3964 | `		}else{` |
|       7 | 3965 | `			if( (rc = RangeAppendInt(pCtx,pArray,pValue,start_long)) != PH7_OK ){` |
|     ! 0 | 3966 | `				return rc;` |
|       - | 3967 | `			}` |
|       - | 3968 | `		}` |
|       - | 3969 | `	}` |
|       - | 3970 | `	/* Return the new array. 'pValue' is released automatically by the` |
|       - | 3971 | `	 * virtual machine as soon as we return from this foreign function. */` |
|      67 | 3972 | `	ph7_result_value(pCtx,pArray);` |
|      67 | 3973 | `	return PH7_OK;` |
|       2 | 3974 | `negative_step_error:` |
|       5 | 3975 | `	return PH7_VmThrowException(pCtx,"ValueError",` |
|       - | 3976 | `		"range(): Argument #3 ($step) must be greater than 0 for increasing ranges");` |
|       4 | 3977 | `boundary_error:` |
|       9 | 3978 | `	return PH7_VmThrowException(pCtx,"ValueError",` |
|       - | 3979 | `		"range(): Argument #3 ($step) must be less than the range spanned by argument #1 ($start) and argument #2 ($end)");` |
|      69 | 3980 | `}` |
|       - | 3981 | `/*` |
|       - | 3982 | ` * array array_values(array $array)` |
|       - | 3983 | ` *  Return all the values of an array, indexed numerically.` |
|       - | 3984 | ` * Parameters` |
|       - | 3985 | ` *  $array` |
|       - | 3986 | ` *   The input array.` |
|       - | 3987 | ` * Return` |
|       - | 3988 | ` *  An indexed array of values or NULL on allocation failure.` |
|       - | 3989 | ` */` |
|      36 | 3990 | `static int ph7_hashmap_values(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       4 | 3991 | `{` |
|       - | 3992 | `	ph7_hashmap_node *pNode;` |
|       - | 3993 | `	ph7_hashmap *pMap;` |
|       - | 3994 | `	ph7_value *pArray;` |
|       - | 3995 | `	ph7_value *pObj;` |
|       - | 3996 | `	sxu32 n;` |
|      40 | 3997 | `	if( nArg != 1 ){` |
|       - | 3998 | `		/* Wrong argument count, throw ArgumentCountError */` |
|       8 | 3999 | `		return PH7_VmThrowException(pCtx,` |
|       - | 4000 | `			"ArgumentCountError",` |
|       - | 4001 | `			"array_values() expects exactly 1 argument, %d given",` |
|       2 | 4002 | `			nArg` |
|       - | 4003 | `			);` |
|       - | 4004 | `	}` |
|       - | 4005 | `	/* Make sure we are dealing with a valid hashmap */` |
|      35 | 4006 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|       - | 4007 | `		/* Type mismatch, throw TypeError */` |
|       4 | 4008 | `		return PH7_VmThrowException(pCtx,` |
|       - | 4009 | `			"TypeError",` |
|       - | 4010 | `			"array_values(): Argument #1 ($array) must be of type array, %s given",` |
|       1 | 4011 | `			ph7_type_name(apArg[0])` |
|       - | 4012 | `			);` |
|       - | 4013 | `	}` |
|       - | 4014 | `	/* Point to the internal representation that describe the input hashmap */` |
|      32 | 4015 | `	pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|       - | 4016 | `	/* Create a new array */` |
|      32 | 4017 | `	pArray = ph7_context_new_array(pCtx);` |
|      32 | 4018 | `	if( pArray == 0 ){` |
|     ! 0 | 4019 | `		ph7_result_null(pCtx);` |
|     ! 0 | 4020 | `		return PH7_OK;` |
|       - | 4021 | `	}` |
|       - | 4022 | `	/* Perform the requested operation */` |
|      32 | 4023 | `	pNode = pMap->pFirst;` |
|     104 | 4024 | `	for( n = 0 ; n < pMap->nEntry ; ++n ){` |
|      74 | 4025 | `		pObj = HashmapExtractNodeValue(pNode);` |
|      74 | 4026 | `		if( pObj ){` |
|       - | 4027 | `			/* perform the insertion */` |
|      74 | 4028 | `			ph7_array_add_elem(pArray,0/* Automatic index assign */,pObj);` |
|      36 | 4029 | `		}` |
|       - | 4030 | `		/* Point to the next entry */` |
|      74 | 4031 | `		pNode = pNode->pPrev; /* Reverse link */` |
|      38 | 4032 | `	}` |
|       - | 4033 | `	/* return the new array */` |
|      32 | 4034 | `	ph7_result_value(pCtx,pArray);` |
|      32 | 4035 | `	return PH7_OK;` |
|      22 | 4036 | `}` |
|       - | 4037 | `/*` |
|       - | 4038 | ` * array array_keys(array $input [, val $search_value [, bool $strict = false ]] )` |
|       - | 4039 | ` *  Return all the keys or a subset of the keys of an array.` |
|       - | 4040 | ` * Parameters` |
|       - | 4041 | ` *  $input` |
|       - | 4042 | ` *   An array containing keys to return.` |
|       - | 4043 | ` * $search_value` |
|       - | 4044 | ` *   If specified, then only keys containing these values are returned.` |
|       - | 4045 | ` * $strict` |
|       - | 4046 | ` *   Determines if strict comparison (===) should be used during the search.` |
|       - | 4047 | ` * Return` |
|       - | 4048 | ` *  An array of all the keys in input or NULL on failure.` |
|       - | 4049 | ` */` |
|     142 | 4050 | `static int ph7_hashmap_keys(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       5 | 4051 | `{` |
|       - | 4052 | `	ph7_hashmap_node *pNode;` |
|       - | 4053 | `	ph7_hashmap *pMap;` |
|       - | 4054 | `	ph7_value *pArray;` |
|       - | 4055 | `	ph7_value sObj;` |
|       - | 4056 | `	ph7_value sVal;` |
|       - | 4057 | `	SyString sKey;` |
|       - | 4058 | `	int bStrict;` |
|       - | 4059 | `	sxi32 rc;` |
|       - | 4060 | `	sxu32 n;` |
|     147 | 4061 | `	if( nArg < 1 ){` |
|       - | 4062 | `		/* Missing argument,throw ArgumentCountError */` |
|       3 | 4063 | `		return PH7_VmThrowException(pCtx,` |
|       - | 4064 | `			"ArgumentCountError",` |
|       - | 4065 | `			"array_keys() expects at least 1 argument, 0 given"` |
|       - | 4066 | `			);` |
|       - | 4067 | `	}` |
|       - | 4068 | `	/* Make sure we are dealing with a valid hashmap */` |
|     144 | 4069 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|       - | 4070 | `		/* haystack must be an array,throw TypeError */` |
|       4 | 4071 | `		return PH7_VmThrowException(pCtx,` |
|       - | 4072 | `			"TypeError",` |
|       - | 4073 | `			"array_keys(): Argument #1 ($array) must be of type array, %s given",` |
|       1 | 4074 | `			ph7_type_name(apArg[0])` |
|       - | 4075 | `			);` |
|       - | 4076 | `	}` |
|       - | 4077 | `	/* Point to the internal representation of the input hashmap */` |
|     142 | 4078 | `	pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|       - | 4079 | `	/* Create a new array */` |
|     142 | 4080 | `	pArray = ph7_context_new_array(pCtx);` |
|     142 | 4081 | `	if( pArray == 0 ){` |
|     ! 0 | 4082 | `		ph7_result_null(pCtx);` |
|     ! 0 | 4083 | `		return PH7_OK;` |
|       - | 4084 | `	}` |
|     142 | 4085 | `	bStrict = FALSE;` |
|     142 | 4086 | `	if( nArg > 2 ){` |
|       - | 4087 | `		/* In PHP, non-scalar values for a bool-hinted parameter raise TypeError */` |
|       8 | 4088 | `		if( ph7_value_is_array(apArg[2]) \|\| ph7_value_is_object(apArg[2]) \|\| ph7_value_is_resource(apArg[2]) ){` |
|       4 | 4089 | `			return PH7_VmThrowException(pCtx,` |
|       - | 4090 | `				"TypeError",` |
|       - | 4091 | `				"array_keys(): Argument #3 ($strict) must be of type bool, %s given",` |
|       2 | 4092 | `				ph7_type_name(apArg[2])` |
|       - | 4093 | `				);` |
|       - | 4094 | `		}` |
|       5 | 4095 | `		bStrict = ph7_value_to_bool(apArg[2]);` |
|       2 | 4096 | `	}` |
|       - | 4097 | `	/* Perform the requested operation */` |
|     140 | 4098 | `	pNode = pMap->pFirst;` |
|     140 | 4099 | `	PH7_MemObjInit(pMap->pVm,&sVal);` |
|    1130 | 4100 | `	for( n = 0 ; n < pMap->nEntry ; ++n ){` |
|     994 | 4101 | `		if( pNode->iType == HASHMAP_INT_NODE ){` |
|     133 | 4102 | `			PH7_MemObjInitFromInt(pMap->pVm,&sObj,pNode->xKey.iKey);` |
|      68 | 4103 | `		}else{` |
|     862 | 4104 | `			SyStringInitFromBuf(&sKey,SyBlobData(&pNode->xKey.sKey),SyBlobLength(&pNode->xKey.sKey));` |
|     862 | 4105 | `			PH7_MemObjInitFromString(pMap->pVm,&sObj,&sKey);` |
|       - | 4106 | `		}` |
|     994 | 4107 | `		rc = 0;` |
|     994 | 4108 | `		if( nArg > 1 ){` |
|      31 | 4109 | `			ph7_value *pValue = HashmapExtractNodeValue(pNode);` |
|      31 | 4110 | `			if( pValue ){` |
|      31 | 4111 | `				PH7_MemObjLoad(pValue,&sVal);` |
|       - | 4112 | `				/* Filter key */` |
|      31 | 4113 | `				rc = ph7_value_compare(&sVal,apArg[1],bStrict);` |
|      31 | 4114 | `				PH7_MemObjRelease(&sVal);` |
|      15 | 4115 | `			}` |
|      15 | 4116 | `		}` |
|     994 | 4117 | `		if( rc == 0 ){` |
|       - | 4118 | `			/* Perform the insertion */` |
|     976 | 4119 | `			ph7_array_add_elem(pArray,0,&sObj);` |
|     486 | 4120 | `		}` |
|     994 | 4121 | `		PH7_MemObjRelease(&sObj);` |
|       - | 4122 | `		/* Point to the next entry */` |
|     994 | 4123 | `		pNode = pNode->pPrev; /* Reverse link */` |
|     499 | 4124 | `	}` |
|       - | 4125 | `	/* return the new array */` |
|     140 | 4126 | `	ph7_result_value(pCtx,pArray);` |
|     140 | 4127 | `	return PH7_OK;` |
|      76 | 4128 | `}` |
|       - | 4129 | `/*` |
|       - | 4130 | ` * bool array_same(array $arr1,array $arr2)` |
|       - | 4131 | ` *  Return TRUE if the given arrays are the same instance.` |
|       - | 4132 | ` *  This function is useful under PH7 since arrays are passed` |
|       - | 4133 | ` *  by reference unlike the zend engine which use pass by values.` |
|       - | 4134 | ` * Parameters` |
|       - | 4135 | ` *  $arr1` |
|       - | 4136 | ` *   First array` |
|       - | 4137 | ` *  $arr2` |
|       - | 4138 | ` *   Second array` |
|       - | 4139 | ` * Return` |
|       - | 4140 | ` *  TRUE if the arrays are the same instance.FALSE otherwise.` |
|       - | 4141 | ` * Note` |
|       - | 4142 | ` *  This function is a symisc eXtension.` |
|       - | 4143 | ` */` |
|       4 | 4144 | `static int ph7_hashmap_same(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       1 | 4145 | `{` |
|       - | 4146 | `	ph7_hashmap *p1,*p2;` |
|       - | 4147 | `	int rc;` |
|       5 | 4148 | `	if( nArg < 2 \|\| !ph7_value_is_array(apArg[0]) \|\| !ph7_value_is_array(apArg[1]) ){` |
|       - | 4149 | `		/* Missing or invalid arguments,return FALSE*/` |
|     ! 0 | 4150 | `		ph7_result_bool(pCtx,0);` |
|     ! 0 | 4151 | `		return PH7_OK;` |
|       - | 4152 | `	}` |
|       - | 4153 | `	/* Point to the hashmaps */` |
|       5 | 4154 | `	p1 = (ph7_hashmap *)apArg[0]->x.pOther;` |
|       5 | 4155 | `	p2 = (ph7_hashmap *)apArg[1]->x.pOther;` |
|       5 | 4156 | `	rc = (p1 == p2);` |
|       - | 4157 | `	/* Same instance? */` |
|       5 | 4158 | `	ph7_result_bool(pCtx,rc);` |
|       5 | 4159 | `	return PH7_OK;` |
|       3 | 4160 | `}` |
|       - | 4161 | `/*` |
|       - | 4162 | ` * array array_merge(array ...$arrays)` |
|       - | 4163 | ` *  Merge one or more arrays.` |
|       - | 4164 | ` * Parameters` |
|       - | 4165 | ` *  ...$arrays` |
|       - | 4166 | ` *   Variable list of arrays to merge. Each argument must be an array;` |
|       - | 4167 | ` *   passing a non-array argument throws a TypeError.` |
|       - | 4168 | ` * Return` |
|       - | 4169 | ` *  The resulting merged array. Returns an empty array when called` |
|       - | 4170 | ` *  with no arguments.` |
|       - | 4171 | ` */` |
|    1026 | 4172 | `static int ph7_hashmap_merge(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       5 | 4173 | `{` |
|       - | 4174 | `	ph7_hashmap *pMap,*pSrc;` |
|       - | 4175 | `	ph7_value *pArray;` |
|       - | 4176 | `	int i;` |
|       - | 4177 | `	/* Create a new array */` |
|    1031 | 4178 | `	pArray = ph7_context_new_array(pCtx);` |
|    1031 | 4179 | `	if( pArray == 0 ){` |
|     ! 0 | 4180 | `		ph7_result_null(pCtx);` |
|     ! 0 | 4181 | `		return PH7_OK;` |
|       - | 4182 | `	}` |
|       - | 4183 | `	/* Point to the internal representation of the hashmap */` |
|    1031 | 4184 | `	pMap = (ph7_hashmap *)pArray->x.pOther;` |
|       - | 4185 | `	/* Start merging */` |
|    3073 | 4186 | `	for( i = 0 ; i < nArg ; i++ ){` |
|       - | 4187 | `		/* Make sure we are dealing with a valid hashmap */` |
|    2051 | 4188 | `		if( !ph7_value_is_array(apArg[i]) ){` |
|       - | 4189 | `			/* Type mismatch -> TypeError */` |
|       8 | 4190 | `			return PH7_VmThrowException(pCtx,` |
|       - | 4191 | `				"TypeError",` |
|       - | 4192 | `				"array_merge(): Argument #%d must be of type array, %s given",` |
|       2 | 4193 | `				i + 1,` |
|       4 | 4194 | `				ph7_type_name(apArg[i])` |
|       - | 4195 | `				);` |
|     ! 0 | 4196 | `		}else{` |
|    2047 | 4197 | `			pSrc = (ph7_hashmap *)apArg[i]->x.pOther;` |
|       - | 4198 | `			/* Merge the two hashmaps */` |
|    2047 | 4199 | `			HashmapMerge(pSrc,pMap);` |
|       - | 4200 | `		}` |
|    1026 | 4201 | `	}` |
|       - | 4202 | `	/* Return the freshly created array */` |
|    1027 | 4203 | `	ph7_result_value(pCtx,pArray);` |
|    1027 | 4204 | `	return PH7_OK;` |
|     518 | 4205 | `}` |
|       - | 4206 | `/*` |
|       - | 4207 | ` * array array_copy(array $source)` |
|       - | 4208 | ` *  Make a blind copy of the target array.` |
|       - | 4209 | ` * Parameters` |
|       - | 4210 | ` *  $source` |
|       - | 4211 | ` *   Target array` |
|       - | 4212 | ` * Return` |
|       - | 4213 | ` *  Copy of the target array on success.NULL otherwise.` |
|       - | 4214 | ` * Note` |
|       - | 4215 | ` *  This function is a symisc eXtension.` |
|       - | 4216 | ` */` |
|      16 | 4217 | `static int ph7_hashmap_copy(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       1 | 4218 | `{` |
|       - | 4219 | `	ph7_hashmap *pMap;` |
|       - | 4220 | `	ph7_value *pArray;` |
|      17 | 4221 | `	if( nArg < 1 ){` |
|       - | 4222 | `		/* Missing arguments,return NULL */` |
|     ! 0 | 4223 | `		ph7_result_null(pCtx);` |
|     ! 0 | 4224 | `		return PH7_OK;` |
|       - | 4225 | `	}` |
|       - | 4226 | `	/* Create a new array */` |
|      17 | 4227 | `	pArray = ph7_context_new_array(pCtx);` |
|      17 | 4228 | `	if( pArray == 0 ){` |
|     ! 0 | 4229 | `		ph7_result_null(pCtx);` |
|     ! 0 | 4230 | `		return PH7_OK;` |
|       - | 4231 | `	}` |
|       - | 4232 | `	/* Point to the internal representation of the hashmap */` |
|      17 | 4233 | `	pMap = (ph7_hashmap *)pArray->x.pOther;` |
|      17 | 4234 | `	if( ph7_value_is_array(apArg[0])){` |
|       - | 4235 | `		/* Point to the internal representation of the source */` |
|      17 | 4236 | `		ph7_hashmap *pSrc = (ph7_hashmap *)apArg[0]->x.pOther;` |
|       - | 4237 | `		/* Perform the copy */` |
|      17 | 4238 | `		PH7_HashmapDup(pSrc,pMap);` |
|       9 | 4239 | `	}else{` |
|       - | 4240 | `		/* Simple insertion */` |
|     ! 0 | 4241 | `		PH7_HashmapInsert(pMap,0/* Automatic index assign*/,apArg[0]);` |
|       - | 4242 | `	}` |
|       - | 4243 | `	/* Return the duplicated array */` |
|      17 | 4244 | `	ph7_result_value(pCtx,pArray);` |
|      17 | 4245 | `	return PH7_OK;` |
|       9 | 4246 | `}` |
|       - | 4247 | `/*` |
|       - | 4248 | ` * bool array_erase(array $source)` |
|       - | 4249 | ` *  Remove all elements from a given array.` |
|       - | 4250 | ` * Parameters` |
|       - | 4251 | ` *  $source` |
|       - | 4252 | ` *   Target array` |
|       - | 4253 | ` * Return` |
|       - | 4254 | ` *  TRUE on success.FALSE otherwise.` |
|       - | 4255 | ` * Note` |
|       - | 4256 | ` *  This function is a symisc eXtension.` |
|       - | 4257 | ` */` |
|      16 | 4258 | `static int ph7_hashmap_erase(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       1 | 4259 | `{` |
|       - | 4260 | `	ph7_hashmap *pMap;` |
|      17 | 4261 | `	if( nArg < 1 ){` |
|       - | 4262 | `		/* Missing arguments */` |
|     ! 0 | 4263 | `		ph7_result_bool(pCtx,0);` |
|     ! 0 | 4264 | `		return PH7_OK;` |
|       - | 4265 | `	}` |
|       - | 4266 | `	/* Point to the target hashmap */` |
|      17 | 4267 | `	PH7_HashmapCowSeparate(pCtx->pVm, apArg[0]);` |
|      17 | 4268 | `	pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|       - | 4269 | `	/* Erase */` |
|      17 | 4270 | `	PH7_HashmapRelease(pMap,FALSE);` |
|      17 | 4271 | `	return PH7_OK;` |
|       9 | 4272 | `}` |
|       - | 4273 | `/*` |
|       - | 4274 | ` * array array_slice(array $array, int $offset [, ?int $length = null [, bool $preserve_keys = false ]])` |
|       - | 4275 | ` *  Extract a slice of the array.` |
|       - | 4276 | ` * Parameters` |
|       - | 4277 | ` *  $array` |
|       - | 4278 | ` *    The input array.` |
|       - | 4279 | ` * $offset` |
|       - | 4280 | ` *    If offset is non-negative, the sequence will start at that offset in the array.` |
|       - | 4281 | ` *    If offset is negative, the sequence will start that far from the end of the array.` |
|       - | 4282 | ` * $length (optional, nullable)` |
|       - | 4283 | ` *    If length is given and is positive, then the sequence will have that many elements` |
|       - | 4284 | ` *    in it. If length is given and is negative then the sequence will stop that many` |
|       - | 4285 | ` *    elements from the end of the array. If it is omitted or NULL, then the sequence` |
|       - | 4286 | ` *    will have everything from offset up until the end of the array.` |
|       - | 4287 | ` * $preserve_keys (optional)` |
|       - | 4288 | ` *    Note that array_slice() will reorder and reset the array indices by default.` |
|       - | 4289 | ` *    You can change this behaviour by setting preserve_keys to TRUE.` |
|       - | 4290 | ` * Return` |
|       - | 4291 | ` *   The new slice.` |
|       - | 4292 | ` */` |
|      50 | 4293 | `static int ph7_hashmap_slice(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       5 | 4294 | `{` |
|       - | 4295 | `	ph7_hashmap *pMap,*pSrc;` |
|       - | 4296 | `	ph7_hashmap_node *pCur;` |
|       - | 4297 | `	ph7_value *pArray;` |
|       - | 4298 | `	int iLength,iOfft;` |
|       - | 4299 | `	int bPreserve;` |
|       - | 4300 | `	sxi32 rc;` |
|      55 | 4301 | `	if( nArg < 2 ){` |
|       8 | 4302 | `		return PH7_VmThrowException(pCtx,` |
|       - | 4303 | `			"ArgumentCountError",` |
|       - | 4304 | `			"array_slice() expects at least 2 arguments, %d given",` |
|       2 | 4305 | `			nArg` |
|       - | 4306 | `			);` |
|       - | 4307 | `	}` |
|      51 | 4308 | `	if( nArg > 4 ){` |
|       4 | 4309 | `		return PH7_VmThrowException(pCtx,` |
|       - | 4310 | `			"ArgumentCountError",` |
|       - | 4311 | `			"array_slice() expects at most 4 arguments, %d given",` |
|       1 | 4312 | `			nArg` |
|       - | 4313 | `			);` |
|       - | 4314 | `	}` |
|      49 | 4315 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|       4 | 4316 | `		return PH7_VmThrowException(pCtx,` |
|       - | 4317 | `			"TypeError",` |
|       - | 4318 | `			"array_slice(): Argument #1 ($array) must be of type array, %s given",` |
|       1 | 4319 | `			ph7_type_name(apArg[0])` |
|       - | 4320 | `			);` |
|       - | 4321 | `	}` |
|       - | 4322 | `	/* Validate $offset type: reject string, array, object, resource */` |
|      62 | 4323 | `	if( ph7_value_is_string(apArg[1]) \|\| ph7_value_is_array(apArg[1]) \|\|` |
|      65 | 4324 | `		ph7_value_is_object(apArg[1]) \|\| ph7_value_is_resource(apArg[1]) ){` |
|       4 | 4325 | `		return PH7_VmThrowException(pCtx,` |
|       - | 4326 | `			"TypeError",` |
|       - | 4327 | `			"array_slice(): Argument #2 ($offset) must be of type int, %s given",` |
|       2 | 4328 | `			ph7_type_name(apArg[1])` |
|       - | 4329 | `			);` |
|       - | 4330 | `	}` |
|       - | 4331 | `	/* Validate $length type if provided: nullable int */` |
|      45 | 4332 | `	if( nArg > 2 && !ph7_value_is_null(apArg[2]) ){` |
|      26 | 4333 | `		if( ph7_value_is_string(apArg[2]) \|\| ph7_value_is_array(apArg[2]) \|\|` |
|      26 | 4334 | `			ph7_value_is_object(apArg[2]) \|\| ph7_value_is_resource(apArg[2]) ){` |
|       4 | 4335 | `			return PH7_VmThrowException(pCtx,` |
|       - | 4336 | `				"TypeError",` |
|       - | 4337 | `				"array_slice(): Argument #3 ($length) must be of type ?int, %s given",` |
|       2 | 4338 | `				ph7_type_name(apArg[2])` |
|       - | 4339 | `				);` |
|       - | 4340 | `		}` |
|       8 | 4341 | `	}` |
|       - | 4342 | `	/* Validate $preserve_keys type if provided: reject array, object, resource */` |
|      43 | 4343 | `	if( nArg > 3 ){` |
|      10 | 4344 | `		if( ph7_value_is_array(apArg[3]) \|\| ph7_value_is_object(apArg[3]) \|\|` |
|       4 | 4345 | `			ph7_value_is_resource(apArg[3]) ){` |
|       4 | 4346 | `			return PH7_VmThrowException(pCtx,` |
|       - | 4347 | `				"TypeError",` |
|       - | 4348 | `				"array_slice(): Argument #4 ($preserve_keys) must be of type bool, %s given",` |
|       2 | 4349 | `				ph7_type_name(apArg[3])` |
|       - | 4350 | `				);` |
|       - | 4351 | `		}` |
|       2 | 4352 | `	}` |
|       - | 4353 | `	/* Point the internal representation of the target array */` |
|      41 | 4354 | `	pSrc = (ph7_hashmap *)apArg[0]->x.pOther;` |
|      41 | 4355 | `	bPreserve = FALSE;` |
|       - | 4356 | `	/* Get the offset */` |
|      41 | 4357 | `	iOfft = ph7_value_to_int(apArg[1]);` |
|      41 | 4358 | `	if( iOfft < 0 ){` |
|       5 | 4359 | `		iOfft = (int)pSrc->nEntry + iOfft;` |
|       5 | 4360 | `		if( iOfft < 0 ){` |
|       3 | 4361 | `			iOfft = 0;` |
|       1 | 4362 | `		}` |
|       2 | 4363 | `	}` |
|      41 | 4364 | `	if( iOfft >= (int)pSrc->nEntry ){` |
|       - | 4365 | `		/* Offset past end of array, return empty array */` |
|       5 | 4366 | `		pArray = ph7_context_new_array(pCtx);` |
|       5 | 4367 | `		if( pArray == 0 ){` |
|     ! 0 | 4368 | `			ph7_result_null(pCtx);` |
|     ! 0 | 4369 | `			return PH7_OK;` |
|       - | 4370 | `		}` |
|       5 | 4371 | `		ph7_result_value(pCtx,pArray);` |
|       5 | 4372 | `		return PH7_OK;` |
|       - | 4373 | `	}` |
|       - | 4374 | `	/* Get the length: NULL means "all remaining" (same as omitting) */` |
|      37 | 4375 | `	iLength = (int)pSrc->nEntry - iOfft;` |
|      37 | 4376 | `	if( nArg > 2 && !ph7_value_is_null(apArg[2]) ){` |
|      15 | 4377 | `		iLength = ph7_value_to_int(apArg[2]);` |
|      15 | 4378 | `		if( iLength < 0 ){` |
|       5 | 4379 | `			iLength = ((int)pSrc->nEntry + iLength) - iOfft;` |
|       2 | 4380 | `		}` |
|      15 | 4381 | `		if( iLength < 0 ){` |
|       3 | 4382 | `			iLength = 0;` |
|       1 | 4383 | `		}` |
|      15 | 4384 | `		if( iOfft + iLength > (int)pSrc->nEntry ){` |
|       3 | 4385 | `			iLength = (int)pSrc->nEntry - iOfft;` |
|       1 | 4386 | `		}` |
|       7 | 4387 | `	}` |
|      37 | 4388 | `	if( nArg > 3 ){` |
|       5 | 4389 | `		bPreserve = ph7_value_to_bool(apArg[3]);` |
|       2 | 4390 | `	}` |
|       - | 4391 | `	/* Create a new array */` |
|      37 | 4392 | `	pArray = ph7_context_new_array(pCtx);` |
|      37 | 4393 | `	if( pArray == 0 ){` |
|     ! 0 | 4394 | `		ph7_result_null(pCtx);` |
|     ! 0 | 4395 | `		return PH7_OK;` |
|       - | 4396 | `	}` |
|      37 | 4397 | `	if( iLength < 1 ){` |
|       - | 4398 | `		/* Don't bother processing,return the empty array */` |
|       5 | 4399 | `		ph7_result_value(pCtx,pArray);` |
|       5 | 4400 | `		return PH7_OK;` |
|       - | 4401 | `	}` |
|       - | 4402 | `	/* Point to the desired entry */` |
|      33 | 4403 | `	pCur = pSrc->pFirst;` |
|      28 | 4404 | `	for(;;){` |
|      61 | 4405 | `		if( iOfft < 1 ){` |
|      33 | 4406 | `			break;` |
|       - | 4407 | `		}` |
|       - | 4408 | `		/* Point to the next entry */` |
|      33 | 4409 | `		pCur = pCur->pPrev; /* Reverse link */` |
|      33 | 4410 | `		iOfft--;` |
|       5 | 4411 | `	}` |
|       - | 4412 | `	/* Point to the internal representation of the hashmap */` |
|      33 | 4413 | `	pMap = (ph7_hashmap *)pArray->x.pOther;` |
|      51 | 4414 | `	for(;;){` |
|     107 | 4415 | `		if( iLength < 1 ){` |
|      33 | 4416 | `			break;` |
|       - | 4417 | `		}` |
|       - | 4418 | `		/* String keys are always preserved; preserve_keys only affects int keys */` |
|       - | 4419 | `		{` |
|      79 | 4420 | `			int bKeep = (pCur->iType == HASHMAP_INT_NODE) ? bPreserve : TRUE;` |
|      79 | 4421 | `			rc = HashmapInsertNode(pMap,pCur,bKeep);` |
|       - | 4422 | `		}` |
|      79 | 4423 | `		if( rc != SXRET_OK ){` |
|     ! 0 | 4424 | `			break;` |
|       - | 4425 | `		}` |
|       - | 4426 | `		/* Point to the next entry */` |
|      79 | 4427 | `		pCur = pCur->pPrev; /* Reverse link */` |
|      79 | 4428 | `		iLength--;` |
|       5 | 4429 | `	}` |
|       - | 4430 | `	/* Return the freshly created array */` |
|      33 | 4431 | `	ph7_result_value(pCtx,pArray);` |
|      33 | 4432 | `	return PH7_OK;` |
|      30 | 4433 | `}` |
|       - | 4434 | `/*` |
|       - | 4435 | ` * Move the last node in the hashmap linked list to immediately after pAfter` |
|       - | 4436 | ` * in iteration order.  If pAfter is NULL the node is moved to the very` |
|       - | 4437 | ` * beginning (becomes the new pFirst).` |
|       - | 4438 | ` */` |
|      30 | 4439 | `static void HashmapMoveLastAfter(ph7_hashmap *pMap,ph7_hashmap_node *pAfter)` |
|       1 | 4440 | `{` |
|       - | 4441 | `	ph7_hashmap_node *pNode;` |
|       - | 4442 | `	ph7_hashmap_node *pOldNext;` |
|      31 | 4443 | `	pNode = pMap->pLast;` |
|      31 | 4444 | `	if( pNode == 0 ){` |
|     ! 0 | 4445 | `		return;` |
|       - | 4446 | `	}` |
|      31 | 4447 | `	if( pNode->pNext == 0 ){` |
|       - | 4448 | `		/* Only node in the list, nothing to move */` |
|       5 | 4449 | `		return;` |
|       - | 4450 | `	}` |
|      27 | 4451 | `	if( pAfter != 0 && pAfter->pPrev == pNode ){` |
|       - | 4452 | `		/* Already in the correct position */` |
|       9 | 4453 | `		return;` |
|       - | 4454 | `	}` |
|       - | 4455 | `	/* Unlink pNode from the end of the list */` |
|      19 | 4456 | `	pMap->pLast = pNode->pNext;` |
|      19 | 4457 | `	pMap->pLast->pPrev = 0;` |
|       - | 4458 | `	/* Insert pNode after pAfter in iteration order */` |
|      19 | 4459 | `	if( pAfter == 0 ){` |
|       - | 4460 | `		/* Insert at the very beginning, before pFirst */` |
|       3 | 4461 | `		pNode->pNext = 0;` |
|       3 | 4462 | `		pNode->pPrev = pMap->pFirst;` |
|       3 | 4463 | `		if( pMap->pFirst ){` |
|       3 | 4464 | `			pMap->pFirst->pNext = pNode;` |
|       1 | 4465 | `		}` |
|       3 | 4466 | `		pMap->pFirst = pNode;` |
|       2 | 4467 | `	}else{` |
|      17 | 4468 | `		pOldNext = pAfter->pPrev;` |
|      17 | 4469 | `		pNode->pPrev = pOldNext;` |
|      17 | 4470 | `		pNode->pNext = pAfter;` |
|      17 | 4471 | `		pAfter->pPrev = pNode;` |
|      17 | 4472 | `		if( pOldNext ){` |
|      17 | 4473 | `			pOldNext->pNext = pNode;` |
|       9 | 4474 | `		}else{` |
|     ! 0 | 4475 | `			pMap->pLast = pNode;` |
|       - | 4476 | `		}` |
|       - | 4477 | `	}` |
|      16 | 4478 | `}` |
|       - | 4479 | `/*` |
|       - | 4480 | ` * array array_splice(array $array, int $offset [, int $length [, value $replacement]])` |
|       - | 4481 | ` *  Remove a portion of the array and replace it with something else.` |
|       - | 4482 | ` * Parameters` |
|       - | 4483 | ` *  $array` |
|       - | 4484 | ` *    The input array.` |
|       - | 4485 | ` *  $offset` |
|       - | 4486 | ` *    If offset is positive then the start of removed portion is at that offset` |
|       - | 4487 | ` *    from the beginning of the input array.  If offset is negative then it` |
|       - | 4488 | ` *    starts that far from the end of the input array.  If the absolute value of` |
|       - | 4489 | ` *    a negative offset exceeds the array length, offset is clamped to 0.  If a` |
|       - | 4490 | ` *    positive offset exceeds the array length, offset is clamped to the array` |
|       - | 4491 | ` *    length (i.e. nothing is removed, but replacement is appended).` |
|       - | 4492 | ` *  $length (optional)` |
|       - | 4493 | ` *    If length is omitted, removes everything from offset to the end of the` |
|       - | 4494 | ` *    array.  If length is specified and is positive, then that many elements` |
|       - | 4495 | ` *    will be removed.  If length is specified and is negative then the end of` |
|       - | 4496 | ` *    the removed portion will be that many elements from the end of the array.` |
|       - | 4497 | ` *    If the resulting length is negative it is clamped to 0.` |
|       - | 4498 | ` *  $replacement (optional)` |
|       - | 4499 | ` *    If replacement array is specified, then the removed elements are replaced` |
|       - | 4500 | ` *    with elements from this array.` |
|       - | 4501 | ` *    If offset and length are such that nothing is removed, then the elements` |
|       - | 4502 | ` *    from the replacement array are inserted in the place specified by the` |
|       - | 4503 | ` *    offset.` |
|       - | 4504 | ` *    Note that keys in replacement array are not preserved.` |
|       - | 4505 | ` *    If replacement is just one element it is not necessary to put array()` |
|       - | 4506 | ` *    around it, unless the element is an array itself, an object or NULL.` |
|       - | 4507 | ` * Return` |
|       - | 4508 | ` *   A new array consisting of the extracted elements.` |
|       - | 4509 | ` */` |
|      54 | 4510 | `static int ph7_hashmap_splice(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       4 | 4511 | `{` |
|       - | 4512 | `	ph7_hashmap_node *pCur,*pPrev,*pRnode,*pInsertAfter,*pNewNode;` |
|       - | 4513 | `	ph7_value *pArray,*pRvalue;` |
|       - | 4514 | `	ph7_hashmap *pMap,*pSrc,*pRep;` |
|       - | 4515 | `	int iLength,iOfft,i;` |
|       - | 4516 | `	sxi32 rc;` |
|      58 | 4517 | `	if( nArg < 2 ){` |
|       8 | 4518 | `		return PH7_VmThrowException(pCtx,` |
|       - | 4519 | `			"ArgumentCountError",` |
|       - | 4520 | `			"array_splice() expects at least 2 arguments, %d given",` |
|       2 | 4521 | `			nArg` |
|       - | 4522 | `			);` |
|       - | 4523 | `	}` |
|      52 | 4524 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|       4 | 4525 | `		return PH7_VmThrowException(pCtx,` |
|       - | 4526 | `			"TypeError",` |
|       - | 4527 | `			"array_splice(): Argument #1 ($array) must be of type array, %s given",` |
|       1 | 4528 | `			ph7_type_name(apArg[0])` |
|       - | 4529 | `			);` |
|       - | 4530 | `	}` |
|       - | 4531 | `	/* Point to the internal representation of the target array */` |
|      49 | 4532 | `	PH7_HashmapCowSeparate(pCtx->pVm, apArg[0]);` |
|      49 | 4533 | `	pSrc = (ph7_hashmap *)apArg[0]->x.pOther;` |
|       - | 4534 | `	/* Get the offset and clamp to valid range */` |
|      49 | 4535 | `	iOfft = ph7_value_to_int(apArg[1]);` |
|      49 | 4536 | `	if( iOfft < 0 ){` |
|       7 | 4537 | `		iOfft = (int)pSrc->nEntry + iOfft;` |
|       7 | 4538 | `		if( iOfft < 0 ){` |
|       3 | 4539 | `			iOfft = 0;` |
|       2 | 4540 | `		}` |
|      46 | 4541 | `	}else if( iOfft > (int)pSrc->nEntry ){` |
|       3 | 4542 | `		iOfft = (int)pSrc->nEntry;` |
|       1 | 4543 | `	}` |
|       - | 4544 | `	/* Get the length and clamp to valid range.` |
|       - | 4545 | `	 * NULL means "all remaining" (same as omitting the argument). */` |
|      49 | 4546 | `	iLength = (int)pSrc->nEntry - iOfft;` |
|      49 | 4547 | `	if( nArg > 2 && !ph7_value_is_null(apArg[2]) ){` |
|      31 | 4548 | `		iLength = ph7_value_to_int(apArg[2]);` |
|      31 | 4549 | `		if( iLength < 0 ){` |
|       7 | 4550 | `			iLength = ((int)pSrc->nEntry + iLength) - iOfft;` |
|       7 | 4551 | `			if( iLength < 0 ){` |
|       3 | 4552 | `				iLength = 0;` |
|       1 | 4553 | `			}` |
|       3 | 4554 | `		}` |
|      31 | 4555 | `		if( iOfft + iLength > (int)pSrc->nEntry ){` |
|       3 | 4556 | `			iLength = (int)pSrc->nEntry - iOfft;` |
|       1 | 4557 | `		}` |
|      15 | 4558 | `	}` |
|       - | 4559 | `	/* Create the result array for removed elements */` |
|      49 | 4560 | `	pArray = ph7_context_new_array(pCtx);` |
|      49 | 4561 | `	if( pArray == 0 ){` |
|     ! 0 | 4562 | `		ph7_result_null(pCtx);` |
|     ! 0 | 4563 | `		return PH7_OK;` |
|       - | 4564 | `	}` |
|       - | 4565 | `	/* Get replacement array if provided */` |
|      49 | 4566 | `	pRep = 0;` |
|      49 | 4567 | `	if( nArg > 3 ){` |
|      21 | 4568 | `		if( !ph7_value_is_array(apArg[3]) ){` |
|       - | 4569 | `			/* Perform an array cast */` |
|       3 | 4570 | `			PH7_MemObjToHashmap(apArg[3]);` |
|       3 | 4571 | `			if( ph7_value_is_array(apArg[3]) ){` |
|       3 | 4572 | `				pRep = (ph7_hashmap *)apArg[3]->x.pOther;` |
|       1 | 4573 | `			}` |
|       2 | 4574 | `		}else{` |
|      19 | 4575 | `			pRep = (ph7_hashmap *)apArg[3]->x.pOther;` |
|       - | 4576 | `		}` |
|      21 | 4577 | `		if( pRep ){` |
|       - | 4578 | `			/* Reset the loop cursor */` |
|      21 | 4579 | `			pRep->pCur = pRep->pFirst;` |
|      10 | 4580 | `		}` |
|      10 | 4581 | `	}` |
|       - | 4582 | `	/* Early return if nothing to remove and no replacement */` |
|      49 | 4583 | `	if( iLength < 1 && pRep == 0 ){` |
|       9 | 4584 | `		ph7_result_value(pCtx,pArray);` |
|       9 | 4585 | `		return PH7_OK;` |
|       - | 4586 | `	}` |
|       - | 4587 | `	/* Navigate to the offset position */` |
|      41 | 4588 | `	pCur = pSrc->pFirst;` |
|      85 | 4589 | `	for( i = 0 ; i < iOfft && pCur ; i++ ){` |
|      45 | 4590 | `		pCur = pCur->pPrev; /* Reverse link */` |
|      23 | 4591 | `	}` |
|       - | 4592 | `	/* Save the node just before the splice range as the insertion anchor.` |
|       - | 4593 | `	 * pCur->pNext is the backward link (previous node in iteration order).` |
|       - | 4594 | `	 * If pCur is NULL (offset == nEntry), the anchor is the last node. */` |
|      41 | 4595 | `	pInsertAfter = (pCur != 0) ? pCur->pNext : pSrc->pLast;` |
|       - | 4596 | `	/* Remove nodes in the splice range and copy them to the result array */` |
|      41 | 4597 | `	pMap = (ph7_hashmap *)pArray->x.pOther;` |
|     111 | 4598 | `	for( i = 0 ; i < iLength && pCur ; i++ ){` |
|      71 | 4599 | `		pPrev = pCur->pPrev;` |
|      71 | 4600 | `		rc = HashmapInsertNode(pMap,pCur,FALSE);` |
|      71 | 4601 | `		PH7_HashmapUnlinkNode(pCur,TRUE);` |
|      71 | 4602 | `		if( rc != SXRET_OK ){` |
|     ! 0 | 4603 | `			break;` |
|       - | 4604 | `		}` |
|      71 | 4605 | `		pCur = pPrev; /* Reverse link */` |
|      36 | 4606 | `	}` |
|       - | 4607 | `	/* Insert replacement elements at the correct position */` |
|      41 | 4608 | `	if( pRep ){` |
|       - | 4609 | `		ph7_value sSafeVal;` |
|      61 | 4610 | `		while( (pRnode = PH7_HashmapGetNextEntry(pRep)) != 0 ){` |
|      31 | 4611 | `			pRvalue = HashmapExtractNodeValue(pRnode);` |
|      31 | 4612 | `			if( pRvalue ){` |
|       - | 4613 | `				/* Make a stack copy before inserting.  HashmapInsert() may` |
|       - | 4614 | `				 * grow the VM memobj pool, which would invalidate pRvalue` |
|       - | 4615 | `				 * since it points into that same pool. */` |
|      31 | 4616 | `				sSafeVal = *pRvalue;` |
|      31 | 4617 | `				rc = HashmapInsert(pSrc,0,&sSafeVal);` |
|      31 | 4618 | `				if( rc == SXRET_OK && pSrc->pLast != 0 ){` |
|      31 | 4619 | `					pNewNode = pSrc->pLast;` |
|      31 | 4620 | `					HashmapMoveLastAfter(pSrc,pInsertAfter);` |
|      31 | 4621 | `					pInsertAfter = pNewNode;` |
|      15 | 4622 | `				}` |
|      15 | 4623 | `			}` |
|       1 | 4624 | `		}` |
|      10 | 4625 | `	}` |
|       - | 4626 | `	/* Return the freshly created array */` |
|      41 | 4627 | `	ph7_result_value(pCtx,pArray);` |
|      41 | 4628 | `	return PH7_OK;` |
|      31 | 4629 | `}` |
|       - | 4630 | `/*` |
|       - | 4631 | ` * bool in_array(value $needle,array $haystack[,bool $strict = FALSE ])` |
|       - | 4632 | ` *  Checks if a value exists in an array.` |
|       - | 4633 | ` * Parameters` |
|       - | 4634 | ` *  $needle` |
|       - | 4635 | ` *   The searched value.` |
|       - | 4636 | ` *   Note:` |
|       - | 4637 | ` *    If needle is a string, the comparison is done in a case-sensitive manner.` |
|       - | 4638 | ` * $haystack` |
|       - | 4639 | ` *  The target array.` |
|       - | 4640 | ` * $strict` |
|       - | 4641 | ` *  If the third parameter strict is set to TRUE then the in_array() function` |
|       - | 4642 | ` *  will also check the types of the needle in the haystack.` |
|       - | 4643 | ` */` |
|   31954 | 4644 | `static int ph7_hashmap_in_array(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       5 | 4645 | `{` |
|       - | 4646 | `	ph7_value *pNeedle;` |
|       - | 4647 | `	int bStrict;` |
|       - | 4648 | `	int rc;` |
|   31959 | 4649 | `	if( nArg < 2 ){` |
|       - | 4650 | `		/* Missing argument,return FALSE */` |
|     ! 0 | 4651 | `		ph7_result_bool(pCtx,0);` |
|     ! 0 | 4652 | `		return PH7_OK;` |
|       - | 4653 | `	}` |
|   31959 | 4654 | `	pNeedle = apArg[0];` |
|   31959 | 4655 | `	bStrict = 0;` |
|   31959 | 4656 | `	if( nArg > 2 ){` |
|      17 | 4657 | `		bStrict = ph7_value_to_bool(apArg[2]);` |
|       8 | 4658 | `	}` |
|   31959 | 4659 | `	if( !ph7_value_is_array(apArg[1]) ){` |
|       - | 4660 | `		/* haystack must be an array,perform a standard comparison */` |
|     ! 0 | 4661 | `		rc = ph7_value_compare(pNeedle,apArg[1],bStrict);` |
|       - | 4662 | `		/* Set the comparison result */` |
|     ! 0 | 4663 | `		ph7_result_bool(pCtx,rc == 0);` |
|     ! 0 | 4664 | `		return PH7_OK;` |
|       - | 4665 | `	}` |
|       - | 4666 | `	/* Perform the lookup */` |
|   31959 | 4667 | `	rc = HashmapFindValue((ph7_hashmap *)apArg[1]->x.pOther,pNeedle,0,bStrict);` |
|       - | 4668 | `	/* Lookup result */` |
|   31959 | 4669 | `	ph7_result_bool(pCtx,rc == SXRET_OK);` |
|   31959 | 4670 | `	return PH7_OK;` |
|   15982 | 4671 | `}` |
|       - | 4672 | `/*` |
|       - | 4673 | ` * value array_search(value $needle,array $haystack[,bool $strict = false ])` |
|       - | 4674 | ` *  Searches the array for a given value and returns the corresponding key if successful.` |
|       - | 4675 | ` * Parameters` |
|       - | 4676 | ` * $needle` |
|       - | 4677 | ` *   The searched value.` |
|       - | 4678 | ` * $haystack` |
|       - | 4679 | ` *   The array.` |
|       - | 4680 | ` * $strict` |
|       - | 4681 | ` *  If the third parameter strict is set to TRUE then the array_search() function` |
|       - | 4682 | ` *  will search for identical elements in the haystack. This means it will also check` |
|       - | 4683 | ` *  the types of the needle in the haystack, and objects must be the same instance.` |
|       - | 4684 | ` * Return` |
|       - | 4685 | ` *  Returns the key for needle if it is found in the array, FALSE otherwise.` |
|       - | 4686 | ` */` |
|      28 | 4687 | `static int ph7_hashmap_search(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       5 | 4688 | `{` |
|       - | 4689 | `	ph7_hashmap_node *pEntry;` |
|       - | 4690 | `	ph7_value *pVal,sNeedle;` |
|       - | 4691 | `	ph7_hashmap *pMap;` |
|       - | 4692 | `	ph7_value sVal;` |
|       - | 4693 | `	int bStrict;` |
|       - | 4694 | `	sxu32 n;` |
|       - | 4695 | `	int rc;` |
|      33 | 4696 | `	if( nArg < 2 ){` |
|       - | 4697 | `		/* Missing argument,throw ArgumentCountError */` |
|       8 | 4698 | `		return PH7_VmThrowException(pCtx,` |
|       - | 4699 | `			"ArgumentCountError",` |
|       - | 4700 | `			"array_search() expects at least 2 arguments, %d given",` |
|       2 | 4701 | `			nArg` |
|       - | 4702 | `			);` |
|       - | 4703 | `	}` |
|      27 | 4704 | `	bStrict = FALSE;` |
|      27 | 4705 | `	if( !ph7_value_is_array(apArg[1]) ){` |
|       - | 4706 | `		/* haystack must be an array,throw TypeError */` |
|       4 | 4707 | `		return PH7_VmThrowException(pCtx,` |
|       - | 4708 | `			"TypeError",` |
|       - | 4709 | `			"array_search(): Argument #2 ($haystack) must be of type array, %s given",` |
|       2 | 4710 | `			ph7_type_name(apArg[1])` |
|       - | 4711 | `			);` |
|       - | 4712 | `	}` |
|      24 | 4713 | `	if( nArg > 2 ){` |
|       - | 4714 | `		/* In PHP, non-scalar values for a bool-hinted parameter raise TypeError */` |
|      12 | 4715 | `		if( ph7_value_is_array(apArg[2]) \|\| ph7_value_is_object(apArg[2]) \|\| ph7_value_is_resource(apArg[2]) ){` |
|       4 | 4716 | `			return PH7_VmThrowException(pCtx,` |
|       - | 4717 | `				"TypeError",` |
|       - | 4718 | `				"array_search(): Argument #3 ($strict) must be of type bool, %s given",` |
|       2 | 4719 | `				ph7_type_name(apArg[2])` |
|       - | 4720 | `				);` |
|       - | 4721 | `		}` |
|       9 | 4722 | `		bStrict = ph7_value_to_bool(apArg[2]);` |
|       4 | 4723 | `	}` |
|       - | 4724 | `	/* Point to the internal representation of the internal hashmap */` |
|      21 | 4725 | `	pMap = (ph7_hashmap *)apArg[1]->x.pOther;` |
|       - | 4726 | `	/* Perform a linear search since we cannot sort the hashmap based on values */` |
|      21 | 4727 | `	PH7_MemObjInit(pMap->pVm,&sVal);` |
|      21 | 4728 | `	PH7_MemObjInit(pMap->pVm,&sNeedle);` |
|      21 | 4729 | `	pEntry = pMap->pFirst;` |
|      21 | 4730 | `	n = pMap->nEntry;` |
|      23 | 4731 | `	for(;;){` |
|      47 | 4732 | `		if( !n ){` |
|       9 | 4733 | `			break;` |
|       - | 4734 | `		}` |
|       - | 4735 | `		/* Extract node value */` |
|      39 | 4736 | `		pVal = HashmapExtractNodeValue(pEntry);` |
|      39 | 4737 | `		if( pVal ){` |
|       - | 4738 | `			/* Make a copy of the vuurent values since the comparison routine` |
|       - | 4739 | `			 * can change their type.` |
|       - | 4740 | `			 */` |
|      39 | 4741 | `			PH7_MemObjLoad(pVal,&sVal);` |
|      39 | 4742 | `			PH7_MemObjLoad(apArg[0],&sNeedle);` |
|      39 | 4743 | `			rc = PH7_MemObjCmp(&sNeedle,&sVal,bStrict,0);` |
|      39 | 4744 | `			PH7_MemObjRelease(&sVal);` |
|      39 | 4745 | `			PH7_MemObjRelease(&sNeedle);` |
|      39 | 4746 | `			if( rc == 0 ){` |
|       - | 4747 | `				/* Match found,return key */` |
|      13 | 4748 | `				if( pEntry->iType == HASHMAP_INT_NODE){` |
|       - | 4749 | `					/* INT key */` |
|       7 | 4750 | `					ph7_result_int64(pCtx,pEntry->xKey.iKey);` |
|       4 | 4751 | `				}else{` |
|       7 | 4752 | `					SyBlob *pKey = &pEntry->xKey.sKey;` |
|       - | 4753 | `					/* Blob key */` |
|       7 | 4754 | `					ph7_result_string(pCtx,(const char *)SyBlobData(pKey),(int)SyBlobLength(pKey));` |
|       - | 4755 | `				}` |
|      13 | 4756 | `				return PH7_OK;` |
|       - | 4757 | `			}` |
|      13 | 4758 | `		}` |
|       - | 4759 | `		/* Point to the next entry */` |
|      27 | 4760 | `		pEntry = pEntry->pPrev; /* Reverse link */` |
|      27 | 4761 | `		n--;` |
|       1 | 4762 | `	}` |
|       - | 4763 | `	/* No such value,return FALSE */` |
|       9 | 4764 | `	ph7_result_bool(pCtx,0);` |
|       9 | 4765 | `	return PH7_OK;` |
|      19 | 4766 | `}` |
|       - | 4767 | `/*` |
|       - | 4768 | ` * array array_diff(array $array1,array $array2,...)` |
|       - | 4769 | ` *  Computes the difference of arrays.` |
|       - | 4770 | ` * Parameters` |
|       - | 4771 | ` *  $array1` |
|       - | 4772 | ` *    The array to compare from` |
|       - | 4773 | ` *  $array2` |
|       - | 4774 | ` *    An array to compare against` |
|       - | 4775 | ` *  $...` |
|       - | 4776 | ` *   More arrays to compare against` |
|       - | 4777 | ` * Return` |
|       - | 4778 | ` *  Returns an array containing all the entries from array1 that` |
|       - | 4779 | ` *  are not present in any of the other arrays.` |
|       - | 4780 | ` */` |
|      22 | 4781 | `static int ph7_hashmap_diff(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       4 | 4782 | `{` |
|       - | 4783 | `	ph7_hashmap_node *pEntry;` |
|       - | 4784 | `	ph7_hashmap *pSrc,*pMap;` |
|       - | 4785 | `	ph7_value *pArray;` |
|       - | 4786 | `	ph7_value *pVal;` |
|       - | 4787 | `	sxi32 rc;` |
|       - | 4788 | `	sxu32 n;` |
|       - | 4789 | `	int i;` |
|       - | 4790 | `	/* Validate arguments to mimic PHP behaviour. Earlier versions simply` |
|       - | 4791 | `	 * returned NULL when the caller passed invalid parameters which made` |
|       - | 4792 | `	 * debugging difficult. */` |
|      26 | 4793 | `	if( nArg < 1 ){` |
|       4 | 4794 | `		return PH7_VmThrowException(pCtx,` |
|       - | 4795 | `			"ArgumentCountError",` |
|       - | 4796 | `			"array_diff() expects at least 1 argument, %d given",` |
|       1 | 4797 | `			nArg` |
|       - | 4798 | `			);` |
|       - | 4799 | `	}` |
|      23 | 4800 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|       4 | 4801 | `		return PH7_VmThrowException(pCtx,` |
|       - | 4802 | `			"TypeError",` |
|       - | 4803 | `			"array_diff(): Argument #1 ($array) must be of type array, %s given",` |
|       1 | 4804 | `			ph7_type_name(apArg[0])` |
|       - | 4805 | `			);` |
|       - | 4806 | `	}` |
|      36 | 4807 | `	for(i = 1 ; i < nArg ; i++){` |
|      20 | 4808 | `		if( !ph7_value_is_array(apArg[i]) ){` |
|       4 | 4809 | `			return PH7_VmThrowException(pCtx,` |
|       - | 4810 | `				"TypeError",` |
|       - | 4811 | `				"array_diff(): Argument #%d must be of type array, %s given",` |
|       1 | 4812 | `				i + 1,` |
|       2 | 4813 | `				ph7_type_name(apArg[i])` |
|       - | 4814 | `				);` |
|       - | 4815 | `		}` |
|       9 | 4816 | `	}` |
|      17 | 4817 | `	if( nArg == 1 ){` |
|       - | 4818 | `		/* Return the first array since we cannot perform a diff */` |
|       3 | 4819 | `		ph7_result_value(pCtx,apArg[0]);` |
|       3 | 4820 | `		return PH7_OK;` |
|       - | 4821 | `	}` |
|       - | 4822 | `	/* Create a new array */` |
|      15 | 4823 | `	pArray = ph7_context_new_array(pCtx);` |
|      15 | 4824 | `	if( pArray == 0 ){` |
|     ! 0 | 4825 | `		ph7_result_null(pCtx);` |
|     ! 0 | 4826 | `		return PH7_OK;` |
|       - | 4827 | `	}` |
|       - | 4828 | `	/* Point to the internal representation of the source hashmap */` |
|      15 | 4829 | `	pSrc = (ph7_hashmap *)apArg[0]->x.pOther;` |
|       - | 4830 | `	/* Perform the diff */` |
|      15 | 4831 | `	pEntry = pSrc->pFirst;` |
|      15 | 4832 | `	n = pSrc->nEntry;` |
|      27 | 4833 | `	for(;;){` |
|      55 | 4834 | `		if( n < 1 ){` |
|      15 | 4835 | `			break;` |
|       - | 4836 | `		}` |
|       - | 4837 | `		/* Extract the node value */` |
|      41 | 4838 | `		pVal = HashmapExtractNodeValue(pEntry);` |
|      41 | 4839 | `		if( pVal ){` |
|      69 | 4840 | `			for( i = 1 ; i < nArg ; i++ ){` |
|       - | 4841 | `				/* Point to the internal representation of the hashmap */` |
|      45 | 4842 | `				pMap = (ph7_hashmap *)apArg[i]->x.pOther;` |
|       - | 4843 | `				/* Perform the lookup */` |
|      45 | 4844 | `				rc = HashmapFindValue(pMap,pVal,0,TRUE);` |
|      45 | 4845 | `				if( rc == SXRET_OK ){` |
|       - | 4846 | `					/* Value exist */` |
|      17 | 4847 | `					break;` |
|       - | 4848 | `				}` |
|      15 | 4849 | `			}` |
|      41 | 4850 | `			if( i >= nArg ){` |
|       - | 4851 | `				/* Perform the insertion */` |
|      25 | 4852 | `				HashmapInsertNode((ph7_hashmap *)pArray->x.pOther,pEntry,TRUE);` |
|      12 | 4853 | `			}` |
|      20 | 4854 | `		}` |
|       - | 4855 | `		/* Point to the next entry */` |
|      41 | 4856 | `		pEntry = pEntry->pPrev; /* Reverse link */` |
|      41 | 4857 | `		n--;` |
|       1 | 4858 | `	}` |
|       - | 4859 | `	/* Return the freshly created array */` |
|      15 | 4860 | `	ph7_result_value(pCtx,pArray);` |
|      15 | 4861 | `	return PH7_OK;` |
|      15 | 4862 | `}` |
|       - | 4863 | `/*` |
|       - | 4864 | ` * array array_udiff(array $array1,array $array2,...,$callback)` |
|       - | 4865 | ` *  Computes the difference of arrays by using a callback function for data comparison.` |
|       - | 4866 | ` * Parameters` |
|       - | 4867 | ` *  $array1` |
|       - | 4868 | ` *    The array to compare from` |
|       - | 4869 | ` *  $array2` |
|       - | 4870 | ` *    An array to compare against` |
|       - | 4871 | ` *  $...` |
|       - | 4872 | ` *   More arrays to compare against.` |
|       - | 4873 | ` * $callback` |
|       - | 4874 | ` *  The callback comparison function.` |
|       - | 4875 | ` *  The comparison function must return an integer less than, equal to, or greater than zero` |
|       - | 4876 | ` *  if the first argument is considered to be respectively less than, equal to, or greater` |
|       - | 4877 | ` *  than the second.` |
|       - | 4878 | ` *     int callback ( mixed $a, mixed $b )` |
|       - | 4879 | ` * Return` |
|       - | 4880 | ` *  Returns an array containing all the entries from array1 that` |
|       - | 4881 | ` *  are not present in any of the other arrays.` |
|       - | 4882 | ` */` |
|      22 | 4883 | `static int ph7_hashmap_udiff(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       5 | 4884 | `{` |
|       - | 4885 | `	ph7_hashmap_node *pEntry;` |
|       - | 4886 | `	ph7_hashmap *pSrc,*pMap;` |
|       - | 4887 | `	ph7_value *pCallback;` |
|       - | 4888 | `	ph7_value *pArray;` |
|       - | 4889 | `	ph7_value *pVal;` |
|       - | 4890 | `	sxi32 rc;` |
|       - | 4891 | `	sxu32 n;` |
|       - | 4892 | `	int i;` |
|       - | 4893 |  |
|       - | 4894 | `	/* Ensure the argument count matches PHP behaviour. */` |
|      27 | 4895 | `	if( nArg < 2 ){` |
|       4 | 4896 | `		return PH7_VmThrowException(pCtx,` |
|       - | 4897 | `			"ArgumentCountError",` |
|       - | 4898 | `			"array_udiff() expects at least 2 arguments, %d given",` |
|       1 | 4899 | `			nArg` |
|       - | 4900 | `			);` |
|       - | 4901 | `	}` |
|      25 | 4902 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|       4 | 4903 | `		return PH7_VmThrowException(pCtx,` |
|       - | 4904 | `			"TypeError",` |
|       - | 4905 | `			"array_udiff(): Argument #1 ($array) must be of type array, %s given",` |
|       1 | 4906 | `			ph7_type_name(apArg[0])` |
|       - | 4907 | `			);` |
|       - | 4908 | `	}` |
|       - | 4909 |  |
|      23 | 4910 | `	if( nArg == 2 ){` |
|       - | 4911 | `		/* Only the original array and the callback were provided. */` |
|       - | 4912 | `		/* Nevertheless, we still validate the callback after verifying any` |
|       - | 4913 | `		 * intermediate array arguments to match PHP's left-to-right parameter` |
|       - | 4914 | `		 * validation order.` |
|       - | 4915 | `		 */` |
|       4 | 4916 | `	} else {` |
|       - | 4917 | `		/* Ensure intermediary arguments are arrays (matches PHP strict typing). */` |
|      27 | 4918 | `		for( i = 1 ; i < nArg - 1; i++ ){` |
|      19 | 4919 | `			if( !ph7_value_is_array(apArg[i]) ){` |
|      11 | 4920 | `				return PH7_VmThrowException(pCtx,` |
|       - | 4921 | `					"TypeError",` |
|       - | 4922 | `					"array_udiff(): Argument #%d must be of type array, %s given",` |
|       3 | 4923 | `					i + 1,` |
|       6 | 4924 | `					ph7_type_name(apArg[i])` |
|       - | 4925 | `					);` |
|       - | 4926 | `			}` |
|       7 | 4927 | `		}` |
|       - | 4928 | `	}` |
|       - | 4929 |  |
|       - | 4930 | `	/* Identify the callback (always expected as the last argument). */` |
|      16 | 4931 | `	pCallback = apArg[nArg - 1];` |
|       - | 4932 | `	/* Validate the callback to match PHP's error messages. */` |
|      16 | 4933 | `	if( !ph7_value_is_callable(pCallback) ){` |
|       9 | 4934 | `		if( ph7_value_is_array(pCallback) ){` |
|       4 | 4935 | `			return PH7_VmThrowException(pCtx,` |
|       - | 4936 | `				"TypeError",` |
|       - | 4937 | `				"array_udiff(): Argument #%d must be a valid callback, array callback must have exactly two members",` |
|       1 | 4938 | `				nArg` |
|       - | 4939 | `				);` |
|       - | 4940 | `		}` |
|       6 | 4941 | `		if( ph7_value_is_string(pCallback) ){` |
|       - | 4942 | `			int len;` |
|       3 | 4943 | `			const char *zName = ph7_value_to_string(pCallback, &len);` |
|       4 | 4944 | `			return PH7_VmThrowException(pCtx,` |
|       - | 4945 | `				"TypeError",` |
|       - | 4946 | `				"array_udiff(): Argument #%d must be a valid callback, function \"%s\" not found or invalid function name",` |
|       1 | 4947 | `				nArg,` |
|       1 | 4948 | `				zName` |
|       - | 4949 | `				);` |
|       - | 4950 | `		}` |
|       4 | 4951 | `		return PH7_VmThrowException(pCtx,` |
|       - | 4952 | `			"TypeError",` |
|       - | 4953 | `			"array_udiff(): Argument #%d must be a valid callback, no array or string given",` |
|       1 | 4954 | `			nArg` |
|       - | 4955 | `			);` |
|       - | 4956 | `	}` |
|       - | 4957 |  |
|       7 | 4958 | `	if( nArg == 2 ){` |
|       - | 4959 | `		/* Only the original array and the callback were provided. */` |
|       3 | 4960 | `		ph7_result_value(pCtx,apArg[0]);` |
|       3 | 4961 | `		return PH7_OK;` |
|       - | 4962 | `	}` |
|       - | 4963 |  |
|       - | 4964 | `	/* Create a new array */` |
|       5 | 4965 | `	pArray = ph7_context_new_array(pCtx);` |
|       5 | 4966 | `	if( pArray == 0 ){` |
|     ! 0 | 4967 | `		ph7_result_null(pCtx);` |
|     ! 0 | 4968 | `		return PH7_OK;` |
|       - | 4969 | `	}` |
|       - | 4970 | `	/* Point to the internal representation of the source hashmap */` |
|       5 | 4971 | `	pSrc = (ph7_hashmap *)apArg[0]->x.pOther;` |
|       - | 4972 | `	/* Perform the diff */` |
|       5 | 4973 | `	pEntry = pSrc->pFirst;` |
|       5 | 4974 | `	n = pSrc->nEntry;` |
|       5 | 4975 | `	pCtx->pVm->iCmpCallbackExc = 0;` |
|       5 | 4976 | `	for(;;){` |
|      11 | 4977 | `		if( n < 1 ){` |
|       3 | 4978 | `			break;` |
|       - | 4979 | `		}` |
|       - | 4980 | `		/* Extract the node value */` |
|       9 | 4981 | `		pVal = HashmapExtractNodeValue(pEntry);` |
|       9 | 4982 | `		if( pVal ){` |
|      15 | 4983 | `			for( i = 1 ; i < nArg - 1; i++ ){` |
|       - | 4984 | `				/* Point to the internal representation of the hashmap */` |
|       9 | 4985 | `				pMap = (ph7_hashmap *)apArg[i]->x.pOther;` |
|       - | 4986 | `				/* Perform the lookup */` |
|       9 | 4987 | `				rc = HashmapFindValueByCallback(pMap,pVal,pCallback,0);` |
|       9 | 4988 | `				if( rc == SXRET_OK ){` |
|       - | 4989 | `					/* Value exist */` |
|       3 | 4990 | `					break;` |
|       - | 4991 | `				}` |
|       4 | 4992 | `			}` |
|       9 | 4993 | `			if( pCtx->pVm->iCmpCallbackExc ){` |
|       - | 4994 | `				/* The comparison callback raised: propagate so the dispatcher` |
|       - | 4995 | `				 * unwinds, before any spurious insertion into the result. */` |
|       3 | 4996 | `				pCtx->pVm->iCmpCallbackExc = 0;` |
|       3 | 4997 | `				return PH7_EXCEPTION;` |
|       - | 4998 | `			}` |
|       7 | 4999 | `			if( i >= (nArg - 1)){` |
|       - | 5000 | `				/* Perform the insertion */` |
|       5 | 5001 | `				HashmapInsertNode((ph7_hashmap *)pArray->x.pOther,pEntry,TRUE);` |
|       2 | 5002 | `			}` |
|       3 | 5003 | `		}` |
|       - | 5004 | `		/* Point to the next entry */` |
|       7 | 5005 | `		pEntry = pEntry->pPrev; /* Reverse link */` |
|       7 | 5006 | `		n--;` |
|       1 | 5007 | `	}` |
|       - | 5008 | `	/* Return the freshly created array */` |
|       3 | 5009 | `	ph7_result_value(pCtx,pArray);` |
|       3 | 5010 | `	return PH7_OK;` |
|      16 | 5011 | `}` |
|       - | 5012 | `/*` |
|       - | 5013 | ` * array array_diff_assoc(array $array1,array $array2,...)` |
|       - | 5014 | ` *  Computes the difference of arrays with additional index check.` |
|       - | 5015 | ` * Parameters` |
|       - | 5016 | ` *  $array1` |
|       - | 5017 | ` *    The array to compare from` |
|       - | 5018 | ` *  $array2` |
|       - | 5019 | ` *    An array to compare against` |
|       - | 5020 | ` *  $...` |
|       - | 5021 | ` *   More arrays to compare against` |
|       - | 5022 | ` * Return` |
|       - | 5023 | ` *  Returns an array containing all the entries from array1 that` |
|       - | 5024 | ` *  are not present in any of the other arrays.` |
|       - | 5025 | ` */` |
|      20 | 5026 | `static int ph7_hashmap_diff_assoc(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       5 | 5027 | `{` |
|       - | 5028 | `	ph7_hashmap_node *pN1,*pN2,*pEntry;` |
|       - | 5029 | `	ph7_hashmap *pSrc,*pMap;` |
|       - | 5030 | `	ph7_value *pArray;` |
|       - | 5031 | `	ph7_value *pVal;` |
|       - | 5032 | `	sxi32 rc;` |
|       - | 5033 | `	sxu32 n;` |
|       - | 5034 | `	int i;` |
|       - | 5035 | `	/* Ensure the argument list is valid, emitting the same errors PHP` |
|       - | 5036 | `	 * would produce. This makes behaviour predictable and allows the` |
|       - | 5037 | `	 * accompanying integration tests to pass. */` |
|      25 | 5038 | `	if( nArg < 1 ){` |
|       4 | 5039 | `		return PH7_VmThrowException(pCtx,` |
|       - | 5040 | `			"ArgumentCountError",` |
|       - | 5041 | `			"array_diff_assoc() expects at least 1 argument, %d given",` |
|       1 | 5042 | `			nArg` |
|       - | 5043 | `			);` |
|       - | 5044 | `	}` |
|      22 | 5045 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|       4 | 5046 | `		return PH7_VmThrowException(pCtx,` |
|       - | 5047 | `			"TypeError",` |
|       - | 5048 | `			"array_diff_assoc(): Argument #1 ($array) must be of type array, %s given",` |
|       1 | 5049 | `			ph7_type_name(apArg[0])` |
|       - | 5050 | `			);` |
|       - | 5051 | `	}` |
|      33 | 5052 | `	for(i = 1 ; i < nArg ; i++){` |
|      21 | 5053 | `		if( !ph7_value_is_array(apArg[i]) ){` |
|       8 | 5054 | `			return PH7_VmThrowException(pCtx,` |
|       - | 5055 | `				"TypeError",` |
|       - | 5056 | `				"array_diff_assoc(): Argument #%d must be of type array, %s given",` |
|       2 | 5057 | `				i + 1,` |
|       4 | 5058 | `				ph7_type_name(apArg[i])` |
|       - | 5059 | `				);` |
|       - | 5060 | `		}` |
|       9 | 5061 | `	}` |
|      13 | 5062 | `	if( nArg == 1 ){` |
|       - | 5063 | `		/* Return the first array since we cannot perform a diff */` |
|       3 | 5064 | `		ph7_result_value(pCtx,apArg[0]);` |
|       3 | 5065 | `		return PH7_OK;` |
|       - | 5066 | `	}` |
|       - | 5067 | `	/* Create a new array */` |
|      11 | 5068 | `	pArray = ph7_context_new_array(pCtx);` |
|      11 | 5069 | `	if( pArray == 0 ){` |
|     ! 0 | 5070 | `		ph7_result_null(pCtx);` |
|     ! 0 | 5071 | `		return PH7_OK;` |
|       - | 5072 | `	}` |
|       - | 5073 | `	/* Point to the internal representation of the source hashmap */` |
|      11 | 5074 | `	pSrc = (ph7_hashmap *)apArg[0]->x.pOther;` |
|       - | 5075 | `	/* Perform the diff */` |
|      11 | 5076 | `	pEntry = pSrc->pFirst;` |
|      11 | 5077 | `	n = pSrc->nEntry;` |
|      11 | 5078 | `	pN1 = pN2 = 0;` |
|      29 | 5079 | `	for(;;){` |
|       - | 5080 | `		int keep;` |
|      35 | 5081 | `		if( n < 1 ){` |
|      11 | 5082 | `			break;` |
|       - | 5083 | `		}` |
|       - | 5084 | `		/* assume the element should be kept until we find a match */` |
|      25 | 5085 | `		keep = 1;` |
|      41 | 5086 | `		for( i = 1 ; i < nArg ; i++ ){` |
|       - | 5087 | `			/* all arguments have been validated already, so cast directly */` |
|      29 | 5088 | `			pMap = (ph7_hashmap *)apArg[i]->x.pOther;` |
|       - | 5089 | `			/* Perform a key lookup first */` |
|      29 | 5090 | `			if( pEntry->iType == HASHMAP_INT_NODE ){` |
|      13 | 5091 | `				rc = HashmapLookupIntKey(pMap,pEntry->xKey.iKey,&pN1);` |
|       7 | 5092 | `			}else{` |
|      17 | 5093 | `				rc = HashmapLookupBlobKey(pMap,SyBlobData(&pEntry->xKey.sKey),SyBlobLength(&pEntry->xKey.sKey),&pN1);` |
|       - | 5094 | `			}` |
|      29 | 5095 | `			if( rc != SXRET_OK ){` |
|       - | 5096 | `				/* this array does not contain the key, continue checking others */` |
|      15 | 5097 | `				continue;` |
|       - | 5098 | `			}` |
|       - | 5099 | `			/* key exists; check that value stored in the matching node is equal */` |
|      15 | 5100 | `			pVal = HashmapExtractNodeValue(pEntry);` |
|      15 | 5101 | `			if( pVal ){` |
|       - | 5102 | `				/* directly compare with value at pN1 rather than searching again */` |
|      15 | 5103 | `				ph7_value *pVal2 = HashmapExtractNodeValue(pN1);` |
|      15 | 5104 | `				if( pVal2 ){` |
|      15 | 5105 | `					sxi32 cmp = PH7_MemObjCmp(pVal, pVal2, TRUE, 0);` |
|      15 | 5106 | `					if( cmp == 0 ){` |
|       - | 5107 | `						/* identical key+value found in one of the arrays => drop it */` |
|      13 | 5108 | `						keep = 0;` |
|      13 | 5109 | `						break;` |
|       - | 5110 | `					}` |
|       1 | 5111 | `				}` |
|       1 | 5112 | `			}` |
|       2 | 5113 | `		}` |
|      25 | 5114 | `		if( keep ){` |
|       - | 5115 | `			/* Perform the insertion */` |
|      13 | 5116 | `			HashmapInsertNode((ph7_hashmap *)pArray->x.pOther,pEntry,TRUE);` |
|       6 | 5117 | `		}` |
|       - | 5118 | `		/* Point to the next entry */` |
|      25 | 5119 | `		pEntry = pEntry->pPrev; /* Reverse link */` |
|      25 | 5120 | `		n--;` |
|       1 | 5121 | `	}` |
|       - | 5122 | `	/* Return the freshly created array */` |
|      11 | 5123 | `	ph7_result_value(pCtx,pArray);` |
|      11 | 5124 | `	return PH7_OK;` |
|      15 | 5125 | `}` |
|       - | 5126 | `/*` |
|       - | 5127 | ` * array array_diff_uassoc(array $array1,array $array2,...,callback $key_compare_func)` |
|       - | 5128 | ` *  Computes the difference of arrays with additional index check which is performed` |
|       - | 5129 | ` *  by a user supplied callback function.` |
|       - | 5130 | ` * Parameters` |
|       - | 5131 | ` *  $array1` |
|       - | 5132 | ` *    The array to compare from` |
|       - | 5133 | ` *  $array2` |
|       - | 5134 | ` *    An array to compare against` |
|       - | 5135 | ` *  $...` |
|       - | 5136 | ` *   More arrays to compare against.` |
|       - | 5137 | ` *  $key_compare_func` |
|       - | 5138 | ` *   Callback function to use. The callback function must return an integer` |
|       - | 5139 | ` *   less than, equal to, or greater than zero if the first argument is considered` |
|       - | 5140 | ` *   to be respectively less than, equal to, or greater than the second.` |
|       - | 5141 | ` * Return` |
|       - | 5142 | ` *  Returns an array containing all the entries from array1 that` |
|       - | 5143 | ` *  are not present in any of the other arrays.` |
|       - | 5144 | ` */` |
|      24 | 5145 | `static int ph7_hashmap_diff_uassoc(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       5 | 5146 | `{` |
|       - | 5147 | `	ph7_hashmap_node *pEntry;` |
|       - | 5148 | `	ph7_hashmap *pSrc,*pMap;` |
|       - | 5149 | `	ph7_value *pCallback;` |
|       - | 5150 | `	ph7_value *pArray;` |
|       - | 5151 | `	sxi32 rc;` |
|       - | 5152 | `	sxu32 n;` |
|       - | 5153 | `	int i;` |
|       - | 5154 |  |
|       - | 5155 | `	/* Argument validation mimicking PHP errors. */` |
|      29 | 5156 | `	if( nArg < 2 ){` |
|       4 | 5157 | `		return PH7_VmThrowException(pCtx,` |
|       - | 5158 | `			"ArgumentCountError",` |
|       - | 5159 | `			"array_diff_uassoc() expects at least 2 arguments, %d given",` |
|       1 | 5160 | `			nArg` |
|       - | 5161 | `			);` |
|       - | 5162 | `	}` |
|      26 | 5163 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|       4 | 5164 | `		return PH7_VmThrowException(pCtx,` |
|       - | 5165 | `			"TypeError",` |
|       - | 5166 | `			"array_diff_uassoc(): Argument #1 ($array) must be of type array, %s given",` |
|       1 | 5167 | `			ph7_type_name(apArg[0])` |
|       - | 5168 | `			);` |
|       - | 5169 | `	}` |
|       - | 5170 | `	/* Intermediate arguments (except last) must be arrays. Last argument is` |
|       - | 5171 | `	 * expected to be a callback. */` |
|      38 | 5172 | `	for(i = 1 ; i < nArg - 1; i++){` |
|      19 | 5173 | `		if( !ph7_value_is_array(apArg[i]) ){` |
|       4 | 5174 | `			return PH7_VmThrowException(pCtx,` |
|       - | 5175 | `				"TypeError",` |
|       - | 5176 | `				"array_diff_uassoc(): Argument #%d must be of type array, %s given",` |
|       1 | 5177 | `				i + 1,` |
|       2 | 5178 | `				ph7_type_name(apArg[i])` |
|       - | 5179 | `				);` |
|       - | 5180 | `		}` |
|       9 | 5181 | `	}` |
|       - | 5182 | `	/* Point to the callback value */` |
|      22 | 5183 | `	pCallback = apArg[nArg - 1];` |
|      22 | 5184 | `	if( !ph7_value_is_callable(pCallback) ){` |
|       - | 5185 | `		/* Compose an error message that closely matches PHP output. When the` |
|       - | 5186 | `		 * argument is an array of the wrong shape we include an extra clause.` |
|       - | 5187 | `		 * If the value is neither array nor string, PHP says "no array or` |
|       - | 5188 | `		 * string given" which we also reproduce. */` |
|       9 | 5189 | `		if( ph7_value_is_array(pCallback) ){` |
|       - | 5190 | `			/* ARRAY CALLBACK must have exactly two members */` |
|       4 | 5191 | `			return PH7_VmThrowException(pCtx,` |
|       - | 5192 | `				"TypeError",` |
|       - | 5193 | `				"array_diff_uassoc(): Argument #%d must be a valid callback, array callback must have exactly two members",` |
|       1 | 5194 | `				nArg` |
|       - | 5195 | `				);` |
|       - | 5196 | `		}` |
|       6 | 5197 | `		if( !ph7_value_is_string(pCallback) ){` |
|       - | 5198 | `			/* neither array nor string */` |
|       8 | 5199 | `			return PH7_VmThrowException(pCtx,` |
|       - | 5200 | `				"TypeError",` |
|       - | 5201 | `				"array_diff_uassoc(): Argument #%d must be a valid callback, no array or string given",` |
|       2 | 5202 | `				nArg` |
|       - | 5203 | `				);` |
|       - | 5204 | `		}` |
|       - | 5205 | `		/* Fallback for string (non-callable) or other leftover cases */` |
|     ! 0 | 5206 | `		return PH7_VmThrowException(pCtx,` |
|       - | 5207 | `			"TypeError",` |
|       - | 5208 | `			"array_diff_uassoc(): Argument #%d must be a valid callback, %s given",` |
|     ! 0 | 5209 | `			nArg,` |
|     ! 0 | 5210 | `			ph7_type_name(pCallback)` |
|       - | 5211 | `			);` |
|       - | 5212 | `	}` |
|      13 | 5213 | `	if( nArg == 2 ){` |
|       - | 5214 | `		/* If we only have the first array and the callback, just return the` |
|       - | 5215 | `		 * input array. */` |
|       3 | 5216 | `		ph7_result_value(pCtx,apArg[0]);` |
|       3 | 5217 | `		return PH7_OK;` |
|       - | 5218 | `	}` |
|       - | 5219 | `	/* Create a new array */` |
|      11 | 5220 | `	pArray = ph7_context_new_array(pCtx);` |
|      11 | 5221 | `	if( pArray == 0 ){` |
|     ! 0 | 5222 | `		ph7_result_null(pCtx);` |
|     ! 0 | 5223 | `		return PH7_OK;` |
|       - | 5224 | `	}` |
|       - | 5225 | `	/* Point to the internal representation of the source hashmap */` |
|      11 | 5226 | `	pSrc = (ph7_hashmap *)apArg[0]->x.pOther;` |
|       - | 5227 | `	/* Perform the diff */` |
|      11 | 5228 | `	pEntry = pSrc->pFirst;` |
|      11 | 5229 | `	n = pSrc->nEntry;` |
|      21 | 5230 | `	for(;;){` |
|       - | 5231 | `		int keep;` |
|      27 | 5232 | `		if( n < 1 ){` |
|       9 | 5233 | `			break;` |
|       - | 5234 | `		}` |
|      19 | 5235 | `		keep = 1;` |
|      31 | 5236 | `		for( i = 1 ; i < nArg - 1; i++ ){` |
|       - | 5237 | `			/* each of these must already be arrays thanks to earlier validation */` |
|      23 | 5238 | `			pMap = (ph7_hashmap *)apArg[i]->x.pOther;` |
|       - | 5239 | `			/* we must compare keys via callback, not by direct lookup */` |
|      23 | 5240 | `			ph7_hashmap_node *pIt = pMap->pFirst;` |
|      45 | 5241 | `			while( pIt ){` |
|       - | 5242 | `				/* build temporary key values for callback */` |
|       - | 5243 | `				ph7_value key1, key2, result;` |
|       - | 5244 | `				/* initialise only once using the appropriate helper */` |
|      33 | 5245 | `				if( pEntry->iType == HASHMAP_INT_NODE ){` |
|     ! 0 | 5246 | `					PH7_MemObjInitFromInt(pMap->pVm,&key1,pEntry->xKey.iKey);` |
|     ! 0 | 5247 | `				}else{` |
|       - | 5248 | `					SyString sStr;` |
|      33 | 5249 | `					SyStringInitFromBuf(&sStr,` |
|       - | 5250 | `						SyBlobData(&pEntry->xKey.sKey),` |
|       - | 5251 | `						SyBlobLength(&pEntry->xKey.sKey));` |
|      33 | 5252 | `					PH7_MemObjInitFromString(pMap->pVm,&key1,&sStr);` |
|       - | 5253 | `				}` |
|      33 | 5254 | `				if( pIt->iType == HASHMAP_INT_NODE ){` |
|     ! 0 | 5255 | `					PH7_MemObjInitFromInt(pMap->pVm,&key2,pIt->xKey.iKey);` |
|     ! 0 | 5256 | `				}else{` |
|       - | 5257 | `					SyString sStr;` |
|      33 | 5258 | `					SyStringInitFromBuf(&sStr,` |
|       - | 5259 | `						SyBlobData(&pIt->xKey.sKey),` |
|       - | 5260 | `						SyBlobLength(&pIt->xKey.sKey));` |
|      33 | 5261 | `					PH7_MemObjInitFromString(pMap->pVm,&key2,&sStr);` |
|       - | 5262 | `				}` |
|      33 | 5263 | `				PH7_MemObjInit(pMap->pVm,&result);` |
|       - | 5264 | `				/* call user callback with (key1, key2) */` |
|       - | 5265 | `				{` |
|       - | 5266 | `					ph7_value *apK[2];` |
|      33 | 5267 | `					apK[0] = &key1;` |
|      33 | 5268 | `					apK[1] = &key2;` |
|      33 | 5269 | `					rc = PH7_VmCallUserFunction(pMap->pVm,pCallback,2,apK,&result);` |
|       - | 5270 | `				}` |
|      33 | 5271 | `				if( rc == PH7_EXCEPTION ){` |
|       - | 5272 | `					/* The key comparison callback raised. Unlike array_udiff/` |
|       - | 5273 | `					 * array_uintersect (which signal back from` |
|       - | 5274 | `					 * HashmapFindValueByCallback via pVm->iCmpCallbackExc), this` |
|       - | 5275 | `					 * function invokes the callback inline, so it cleans up its own` |
|       - | 5276 | `					 * temporaries and propagates the exception directly. */` |
|       3 | 5277 | `					PH7_MemObjRelease(&result);` |
|       3 | 5278 | `					PH7_MemObjRelease(&key1);` |
|       3 | 5279 | `					PH7_MemObjRelease(&key2);` |
|       3 | 5280 | `					return PH7_EXCEPTION;` |
|       - | 5281 | `				}` |
|      31 | 5282 | `				if( rc == SXRET_OK ){` |
|      31 | 5283 | `					if( (result.iFlags & MEMOBJ_INT) == 0 ){` |
|     ! 0 | 5284 | `						PH7_MemObjToInteger(&result);` |
|     ! 0 | 5285 | `					}` |
|      31 | 5286 | `					if( result.x.iVal == 0 ){` |
|       - | 5287 | `						/* keys considered equal by callback; now compare values */` |
|      13 | 5288 | `						ph7_value *pVal1 = HashmapExtractNodeValue(pEntry);` |
|      13 | 5289 | `						ph7_value *pVal2 = HashmapExtractNodeValue(pIt);` |
|      13 | 5290 | `						if( pVal1 && pVal2 ){` |
|      13 | 5291 | `							if( PH7_MemObjCmp(pVal1,pVal2,TRUE,0) == 0 ){` |
|       9 | 5292 | `								keep = 0;` |
|       9 | 5293 | `								PH7_MemObjRelease(&result);` |
|       - | 5294 | `								/* release keys too before breaking */` |
|       9 | 5295 | `								PH7_MemObjRelease(&key1);` |
|       9 | 5296 | `								PH7_MemObjRelease(&key2);` |
|       9 | 5297 | `								break;` |
|       - | 5298 | `							}` |
|       2 | 5299 | `						}` |
|       2 | 5300 | `					}` |
|      11 | 5301 | `				}` |
|      23 | 5302 | `				PH7_MemObjRelease(&result);` |
|      23 | 5303 | `				PH7_MemObjRelease(&key1);` |
|      23 | 5304 | `				PH7_MemObjRelease(&key2);` |
|       - | 5305 | `				/* move to next node */` |
|      23 | 5306 | `				pIt = pIt->pPrev;` |
|      23 | 5307 | `				if( keep == 0 ) break;` |
|       1 | 5308 | `			}` |
|      21 | 5309 | `			if( keep == 0 ) break;` |
|       7 | 5310 | `		}` |
|      17 | 5311 | `		if( keep ){` |
|       - | 5312 | `			/* Perform the insertion */` |
|       9 | 5313 | `			HashmapInsertNode((ph7_hashmap *)pArray->x.pOther,pEntry,TRUE);` |
|       4 | 5314 | `		}` |
|       - | 5315 | `		/* Point to the next entry */` |
|      17 | 5316 | `		pEntry = pEntry->pPrev; /* Reverse link */` |
|      17 | 5317 | `		n--;` |
|       1 | 5318 | `	}` |
|       - | 5319 | `	/* Return the freshly created array */` |
|       9 | 5320 | `	ph7_result_value(pCtx,pArray);` |
|       9 | 5321 | `	return PH7_OK;` |
|      17 | 5322 | `}` |
|       - | 5323 | `/*` |
|       - | 5324 | ` * array array_diff_key(array $array1 ,array $array2,...)` |
|       - | 5325 | ` *  Computes the difference of arrays using keys for comparison.` |
|       - | 5326 | ` * Parameters` |
|       - | 5327 | ` *  $array1` |
|       - | 5328 | ` *    The array to compare from` |
|       - | 5329 | ` *  $array2` |
|       - | 5330 | ` *    An array to compare against` |
|       - | 5331 | ` *  $...` |
|       - | 5332 | ` *   More arrays to compare against` |
|       - | 5333 | ` * Return` |
|       - | 5334 | ` *  Returns an array containing all the entries from array1 whose keys are not present` |
|       - | 5335 | ` *  in any of the other arrays.` |
|       - | 5336 | ` * Note that NULL is returned on failure.` |
|       - | 5337 | ` */` |
|      14 | 5338 | `static int ph7_hashmap_diff_key(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       4 | 5339 | `{` |
|       - | 5340 | `	ph7_hashmap_node *pEntry;` |
|       - | 5341 | `	ph7_hashmap *pSrc,*pMap;` |
|       - | 5342 | `	ph7_value *pArray;` |
|       - | 5343 | `	sxi32 rc;` |
|       - | 5344 | `	sxu32 n;` |
|       - | 5345 | `	int i;` |
|       - | 5346 | `	/* Validate arguments to mirror PHP behaviour. Previously invalid inputs` |
|       - | 5347 | `	 * would quietly return NULL which is inconsistent with other hashmap` |
|       - | 5348 | `	 * helpers. */` |
|      18 | 5349 | `	if( nArg < 1 ){` |
|       4 | 5350 | `		return PH7_VmThrowException(pCtx,` |
|       - | 5351 | `			"ArgumentCountError",` |
|       - | 5352 | `			"array_diff_key() expects at least 1 argument, %d given",` |
|       1 | 5353 | `			nArg` |
|       - | 5354 | `			);` |
|       - | 5355 | `	}` |
|      15 | 5356 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|       4 | 5357 | `		return PH7_VmThrowException(pCtx,` |
|       - | 5358 | `			"TypeError",` |
|       - | 5359 | `			"array_diff_key(): Argument #1 ($array) must be of type array, %s given",` |
|       1 | 5360 | `			ph7_type_name(apArg[0])` |
|       - | 5361 | `			);` |
|       - | 5362 | `	}` |
|      20 | 5363 | `	for(i = 1 ; i < nArg ; i++){` |
|      12 | 5364 | `		if( !ph7_value_is_array(apArg[i]) ){` |
|       4 | 5365 | `			return PH7_VmThrowException(pCtx,` |
|       - | 5366 | `				"TypeError",` |
|       - | 5367 | `				"array_diff_key(): Argument #%d must be of type array, %s given",` |
|       1 | 5368 | `				i + 1,` |
|       2 | 5369 | `				ph7_type_name(apArg[i])` |
|       - | 5370 | `				);` |
|       - | 5371 | `		}` |
|       5 | 5372 | `	}` |
|       9 | 5373 | `	if( nArg == 1 ){` |
|       - | 5374 | `		/* Return the first array since we cannot perform a diff */` |
|       3 | 5375 | `		ph7_result_value(pCtx,apArg[0]);` |
|       3 | 5376 | `		return PH7_OK;` |
|       - | 5377 | `	}` |
|       - | 5378 | `	/* Create a new array */` |
|       7 | 5379 | `	pArray = ph7_context_new_array(pCtx);` |
|       7 | 5380 | `	if( pArray == 0 ){` |
|     ! 0 | 5381 | `		ph7_result_null(pCtx);` |
|     ! 0 | 5382 | `		return PH7_OK;` |
|       - | 5383 | `	}` |
|       - | 5384 | `	/* Point to the internal representation of the main hashmap */` |
|       7 | 5385 | `	pSrc = (ph7_hashmap *)apArg[0]->x.pOther;` |
|       - | 5386 | `	/* Perfrom the diff */` |
|       7 | 5387 | `	pEntry = pSrc->pFirst;` |
|       7 | 5388 | `	n = pSrc->nEntry;` |
|      12 | 5389 | `	for(;;){` |
|      25 | 5390 | `		if( n < 1 ){` |
|       7 | 5391 | `			break;` |
|       - | 5392 | `		}` |
|      31 | 5393 | `		for( i = 1 ; i < nArg ; i++ ){` |
|      23 | 5394 | `			if( !ph7_value_is_array(apArg[i])) {` |
|       - | 5395 | `				/* ignore */` |
|     ! 0 | 5396 | `				continue;` |
|       - | 5397 | `			}` |
|      23 | 5398 | `			pMap = (ph7_hashmap *)apArg[i]->x.pOther;` |
|      23 | 5399 | `			if( pEntry->iType == HASHMAP_BLOB_NODE ){` |
|      17 | 5400 | `				SyBlob *pKey = &pEntry->xKey.sKey;` |
|       - | 5401 | `				/* Blob lookup */` |
|      17 | 5402 | `				rc = HashmapLookupBlobKey(pMap,SyBlobData(pKey),SyBlobLength(pKey),0);` |
|       9 | 5403 | `			}else{` |
|       - | 5404 | `				/* Int lookup */` |
|       7 | 5405 | `				rc = HashmapLookupIntKey(pMap,pEntry->xKey.iKey,0);` |
|       - | 5406 | `			}` |
|      23 | 5407 | `			if( rc == SXRET_OK ){` |
|       - | 5408 | `				/* Key exists,break immediately */` |
|      11 | 5409 | `				break;` |
|       - | 5410 | `			}` |
|       7 | 5411 | `		}` |
|      19 | 5412 | `		if( i >= nArg ){` |
|       - | 5413 | `			/* Perform the insertion */` |
|       9 | 5414 | `			HashmapInsertNode((ph7_hashmap *)pArray->x.pOther,pEntry,TRUE);` |
|       4 | 5415 | `		}` |
|       - | 5416 | `		/* Point to the next entry */` |
|      19 | 5417 | `		pEntry = pEntry->pPrev; /* Reverse link */` |
|      19 | 5418 | `		n--;` |
|       1 | 5419 | `	}` |
|       - | 5420 | `	/* Return the freshly created array */` |
|       7 | 5421 | `	ph7_result_value(pCtx,pArray);` |
|       7 | 5422 | `	return PH7_OK;` |
|      11 | 5423 | `}` |
|       - | 5424 | `/*` |
|       - | 5425 | ` * array array_intersect(array $array1 ,array $array2,...)` |
|       - | 5426 | ` *  Computes the intersection of arrays.` |
|       - | 5427 | ` * Parameters` |
|       - | 5428 | ` *  $array1` |
|       - | 5429 | ` *    The array to compare from` |
|       - | 5430 | ` *  $array2` |
|       - | 5431 | ` *    An array to compare against` |
|       - | 5432 | ` *  $...` |
|       - | 5433 | ` *   More arrays to compare against` |
|       - | 5434 | ` * Return` |
|       - | 5435 | ` *  Returns an array containing all of the values in array1 whose values exist` |
|       - | 5436 | ` *  in all of the parameters.` |
|       - | 5437 | ` * Throws ArgumentCountError if no arguments are given.` |
|       - | 5438 | ` * Throws TypeError if any argument is not an array.` |
|       - | 5439 | ` */` |
|      22 | 5440 | `static int ph7_hashmap_intersect(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       4 | 5441 | `{` |
|       - | 5442 | `	ph7_hashmap_node *pEntry;` |
|       - | 5443 | `	ph7_hashmap *pSrc,*pMap;` |
|       - | 5444 | `	ph7_value *pArray;` |
|       - | 5445 | `	ph7_value *pVal;` |
|       - | 5446 | `	sxi32 rc;` |
|       - | 5447 | `	sxu32 n;` |
|       - | 5448 | `	int i;` |
|      26 | 5449 | `	if( nArg < 1 ){` |
|       4 | 5450 | `		return PH7_VmThrowException(pCtx,` |
|       - | 5451 | `			"ArgumentCountError",` |
|       - | 5452 | `			"array_intersect() expects at least 1 argument, %d given",` |
|       1 | 5453 | `			nArg` |
|       - | 5454 | `			);` |
|       - | 5455 | `	}` |
|      23 | 5456 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|       4 | 5457 | `		return PH7_VmThrowException(pCtx,` |
|       - | 5458 | `			"TypeError",` |
|       - | 5459 | `			"array_intersect(): Argument #1 ($array) must be of type array, %s given",` |
|       1 | 5460 | `			ph7_type_name(apArg[0])` |
|       - | 5461 | `			);` |
|       - | 5462 | `	}` |
|      36 | 5463 | `	for( i = 1 ; i < nArg ; i++ ){` |
|      20 | 5464 | `		if( !ph7_value_is_array(apArg[i]) ){` |
|       4 | 5465 | `			return PH7_VmThrowException(pCtx,` |
|       - | 5466 | `				"TypeError",` |
|       - | 5467 | `				"array_intersect(): Argument #%d must be of type array, %s given",` |
|       1 | 5468 | `				i + 1,` |
|       2 | 5469 | `				ph7_type_name(apArg[i])` |
|       - | 5470 | `				);` |
|       - | 5471 | `		}` |
|       9 | 5472 | `	}` |
|      17 | 5473 | `	if( nArg == 1 ){` |
|       - | 5474 | `		/* Return the first array since we cannot perform a diff */` |
|       3 | 5475 | `		ph7_result_value(pCtx,apArg[0]);` |
|       3 | 5476 | `		return PH7_OK;` |
|       - | 5477 | `	}` |
|       - | 5478 | `	/* Create a new array */` |
|      15 | 5479 | `	pArray = ph7_context_new_array(pCtx);` |
|      15 | 5480 | `	if( pArray == 0 ){` |
|     ! 0 | 5481 | `		ph7_result_null(pCtx);` |
|     ! 0 | 5482 | `		return PH7_OK;` |
|       - | 5483 | `	}` |
|       - | 5484 | `	/* Point to the internal representation of the source hashmap */` |
|      15 | 5485 | `	pSrc = (ph7_hashmap *)apArg[0]->x.pOther;` |
|       - | 5486 | `	/* Perform the intersection */` |
|      15 | 5487 | `	pEntry = pSrc->pFirst;` |
|      15 | 5488 | `	n = pSrc->nEntry;` |
|      31 | 5489 | `	for(;;){` |
|      63 | 5490 | `		if( n < 1 ){` |
|      15 | 5491 | `			break;` |
|       - | 5492 | `		}` |
|       - | 5493 | `		/* Extract the node value */` |
|      49 | 5494 | `		pVal = HashmapExtractNodeValue(pEntry);` |
|      49 | 5495 | `		if( pVal ){` |
|      79 | 5496 | `			for( i = 1 ; i < nArg ; i++ ){` |
|       - | 5497 | `				/* Point to the internal representation of the hashmap */` |
|      55 | 5498 | `				pMap = (ph7_hashmap *)apArg[i]->x.pOther;` |
|       - | 5499 | `				/* Perform the lookup */` |
|      55 | 5500 | `				rc = HashmapFindValue(pMap,pVal,0,TRUE);` |
|      55 | 5501 | `				if( rc != SXRET_OK ){` |
|       - | 5502 | `					/* Value does not exist */` |
|      25 | 5503 | `					break;` |
|       - | 5504 | `				}` |
|      16 | 5505 | `			}` |
|      49 | 5506 | `			if( i >= nArg ){` |
|       - | 5507 | `				/* Perform the insertion */` |
|      25 | 5508 | `				HashmapInsertNode((ph7_hashmap *)pArray->x.pOther,pEntry,TRUE);` |
|      12 | 5509 | `			}` |
|      24 | 5510 | `		}` |
|       - | 5511 | `		/* Point to the next entry */` |
|      49 | 5512 | `		pEntry = pEntry->pPrev; /* Reverse link */` |
|      49 | 5513 | `		n--;` |
|       1 | 5514 | `	}` |
|       - | 5515 | `	/* Return the freshly created array */` |
|      15 | 5516 | `	ph7_result_value(pCtx,pArray);` |
|      15 | 5517 | `	return PH7_OK;` |
|      15 | 5518 | `}` |
|       - | 5519 | `/*` |
|       - | 5520 | ` * array array_intersect_assoc(array $array1 ,array $array2,...)` |
|       - | 5521 | ` *  Computes the intersection of arrays with additional index check.` |
|       - | 5522 | ` * Parameters` |
|       - | 5523 | ` *  $array1` |
|       - | 5524 | ` *    The array to compare from` |
|       - | 5525 | ` *  $array2` |
|       - | 5526 | ` *    An array to compare against` |
|       - | 5527 | ` *  $...` |
|       - | 5528 | ` *   More arrays to compare against` |
|       - | 5529 | ` * Return` |
|       - | 5530 | ` *  Returns an array containing all the values of array1 that are present` |
|       - | 5531 | ` *  in all the arguments, with matching keys.` |
|       - | 5532 | ` */` |
|      22 | 5533 | `static int ph7_hashmap_intersect_assoc(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       4 | 5534 | `{` |
|       - | 5535 | `	ph7_hashmap_node *pEntry,*pN1,*pN2;` |
|       - | 5536 | `	ph7_hashmap *pSrc,*pMap;` |
|       - | 5537 | `	ph7_value *pArray;` |
|       - | 5538 | `	ph7_value *pVal;` |
|       - | 5539 | `	sxi32 rc;` |
|       - | 5540 | `	sxu32 n;` |
|       - | 5541 | `	int i;` |
|      26 | 5542 | `	if( nArg < 1 ){` |
|       4 | 5543 | `		return PH7_VmThrowException(pCtx,` |
|       - | 5544 | `			"ArgumentCountError",` |
|       - | 5545 | `			"array_intersect_assoc() expects at least 1 argument, %d given",` |
|       1 | 5546 | `			nArg` |
|       - | 5547 | `			);` |
|       - | 5548 | `	}` |
|      23 | 5549 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|       4 | 5550 | `		return PH7_VmThrowException(pCtx,` |
|       - | 5551 | `			"TypeError",` |
|       - | 5552 | `			"array_intersect_assoc(): Argument #1 ($array) must be of type array, %s given",` |
|       1 | 5553 | `			ph7_type_name(apArg[0])` |
|       - | 5554 | `			);` |
|       - | 5555 | `	}` |
|      36 | 5556 | `	for( i = 1 ; i < nArg ; i++ ){` |
|      20 | 5557 | `		if( !ph7_value_is_array(apArg[i]) ){` |
|       4 | 5558 | `			return PH7_VmThrowException(pCtx,` |
|       - | 5559 | `				"TypeError",` |
|       - | 5560 | `				"array_intersect_assoc(): Argument #%d must be of type array, %s given",` |
|       1 | 5561 | `				i + 1,` |
|       2 | 5562 | `				ph7_type_name(apArg[i])` |
|       - | 5563 | `				);` |
|       - | 5564 | `		}` |
|       9 | 5565 | `	}` |
|      17 | 5566 | `	if( nArg == 1 ){` |
|       - | 5567 | `		/* Return the first array since we cannot perform an intersection */` |
|       3 | 5568 | `		ph7_result_value(pCtx,apArg[0]);` |
|       3 | 5569 | `		return PH7_OK;` |
|       - | 5570 | `	}` |
|       - | 5571 | `	/* Create a new array */` |
|      15 | 5572 | `	pArray = ph7_context_new_array(pCtx);` |
|      15 | 5573 | `	if( pArray == 0 ){` |
|     ! 0 | 5574 | `		ph7_result_null(pCtx);` |
|     ! 0 | 5575 | `		return PH7_OK;` |
|       - | 5576 | `	}` |
|       - | 5577 | `	/* Point to the internal representation of the source hashmap */` |
|      15 | 5578 | `	pSrc = (ph7_hashmap *)apArg[0]->x.pOther;` |
|       - | 5579 | `	/* Perform the intersection */` |
|      15 | 5580 | `	pEntry = pSrc->pFirst;` |
|      15 | 5581 | `	n = pSrc->nEntry;` |
|      15 | 5582 | `	pN1 = pN2 = 0; /* cc warning */` |
|      23 | 5583 | `	for(;;){` |
|      47 | 5584 | `		if( n < 1 ){` |
|      15 | 5585 | `			break;` |
|       - | 5586 | `		}` |
|       - | 5587 | `		/* Extract the node value */` |
|      33 | 5588 | `		pVal = HashmapExtractNodeValue(pEntry);` |
|      33 | 5589 | `		if( pVal ){` |
|      53 | 5590 | `			for( i = 1 ; i < nArg ; i++ ){` |
|       - | 5591 | `				/* Point to the internal representation of the hashmap */` |
|      37 | 5592 | `				pMap = (ph7_hashmap *)apArg[i]->x.pOther;` |
|       - | 5593 | `				/* Perform a key lookup first */` |
|      37 | 5594 | `				if( pEntry->iType == HASHMAP_INT_NODE ){` |
|      15 | 5595 | `					rc = HashmapLookupIntKey(pMap,pEntry->xKey.iKey,&pN1);` |
|       8 | 5596 | `				}else{` |
|      23 | 5597 | `					rc = HashmapLookupBlobKey(pMap,SyBlobData(&pEntry->xKey.sKey),SyBlobLength(&pEntry->xKey.sKey),&pN1);` |
|       - | 5598 | `				}` |
|      37 | 5599 | `				if( rc != SXRET_OK ){` |
|       - | 5600 | `					/* No such key,break immediately */` |
|       7 | 5601 | `					break;` |
|       - | 5602 | `				}` |
|       - | 5603 | `				/* Perform the lookup */` |
|      31 | 5604 | `				rc = HashmapFindValue(pMap,pVal,&pN2,TRUE);` |
|      31 | 5605 | `				if( rc != SXRET_OK \|\| pN1 != pN2 ){` |
|       - | 5606 | `					/* Value does not exist */` |
|       6 | 5607 | `					break;` |
|       - | 5608 | `				}` |
|      11 | 5609 | `			}` |
|      33 | 5610 | `			if( i >= nArg ){` |
|       - | 5611 | `				/* Perform the insertion */` |
|      17 | 5612 | `				HashmapInsertNode((ph7_hashmap *)pArray->x.pOther,pEntry,TRUE);` |
|       8 | 5613 | `			}` |
|      16 | 5614 | `		}` |
|       - | 5615 | `		/* Point to the next entry */` |
|      33 | 5616 | `		pEntry = pEntry->pPrev; /* Reverse link */` |
|      33 | 5617 | `		n--;` |
|       1 | 5618 | `	}` |
|       - | 5619 | `	/* Return the freshly created array */` |
|      15 | 5620 | `	ph7_result_value(pCtx,pArray);` |
|      15 | 5621 | `	return PH7_OK;` |
|      15 | 5622 | `}` |
|       - | 5623 | `/*` |
|       - | 5624 | ` * array array_intersect_key(array $array1 ,...)` |
|       - | 5625 | ` *  Computes the intersection of arrays using keys for comparison.` |
|       - | 5626 | ` * Parameters` |
|       - | 5627 | ` *  $array1` |
|       - | 5628 | ` *    The array to compare from` |
|       - | 5629 | ` *  $...` |
|       - | 5630 | ` *   More arrays to compare against` |
|       - | 5631 | ` * Return` |
|       - | 5632 | ` *  Returns an associative array containing all the entries of array1 which` |
|       - | 5633 | ` *  have keys that are present in all arguments.` |
|       - | 5634 | ` * Note that NULL is returned on failure.` |
|       - | 5635 | ` */` |
|      22 | 5636 | `static int ph7_hashmap_intersect_key(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       4 | 5637 | `{` |
|       - | 5638 | `	ph7_hashmap_node *pEntry;` |
|       - | 5639 | `	ph7_hashmap *pSrc,*pMap;` |
|       - | 5640 | `	ph7_value *pArray;` |
|       - | 5641 | `	sxi32 rc;` |
|       - | 5642 | `	sxu32 n;` |
|       - | 5643 | `	int i;` |
|      26 | 5644 | `	if( nArg < 1 ){` |
|       4 | 5645 | `		return PH7_VmThrowException(pCtx,` |
|       - | 5646 | `			"ArgumentCountError",` |
|       - | 5647 | `			"array_intersect_key() expects at least 1 argument, %d given",` |
|       1 | 5648 | `			nArg` |
|       - | 5649 | `			);` |
|       - | 5650 | `	}` |
|      23 | 5651 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|       4 | 5652 | `		return PH7_VmThrowException(pCtx,` |
|       - | 5653 | `			"TypeError",` |
|       - | 5654 | `			"array_intersect_key(): Argument #1 ($array) must be of type array, %s given",` |
|       1 | 5655 | `			ph7_type_name(apArg[0])` |
|       - | 5656 | `			);` |
|       - | 5657 | `	}` |
|      36 | 5658 | `	for( i = 1 ; i < nArg ; i++ ){` |
|      20 | 5659 | `		if( !ph7_value_is_array(apArg[i]) ){` |
|       4 | 5660 | `			return PH7_VmThrowException(pCtx,` |
|       - | 5661 | `				"TypeError",` |
|       - | 5662 | `				"array_intersect_key(): Argument #%d must be of type array, %s given",` |
|       1 | 5663 | `				i + 1,` |
|       2 | 5664 | `				ph7_type_name(apArg[i])` |
|       - | 5665 | `				);` |
|       - | 5666 | `		}` |
|       9 | 5667 | `	}` |
|      17 | 5668 | `	if( nArg == 1 ){` |
|       - | 5669 | `		/* Return the first array since we cannot perform an intersection */` |
|       3 | 5670 | `		ph7_result_value(pCtx,apArg[0]);` |
|       3 | 5671 | `		return PH7_OK;` |
|       - | 5672 | `	}` |
|       - | 5673 | `	/* Create a new array */` |
|      15 | 5674 | `	pArray = ph7_context_new_array(pCtx);` |
|      15 | 5675 | `	if( pArray == 0 ){` |
|     ! 0 | 5676 | `		ph7_result_null(pCtx);` |
|     ! 0 | 5677 | `		return PH7_OK;` |
|       - | 5678 | `	}` |
|       - | 5679 | `	/* Point to the internal representation of the main hashmap */` |
|      15 | 5680 | `	pSrc = (ph7_hashmap *)apArg[0]->x.pOther;` |
|       - | 5681 | `	/* Perform the intersection */` |
|      15 | 5682 | `	pEntry = pSrc->pFirst;` |
|      15 | 5683 | `	n = pSrc->nEntry;` |
|      24 | 5684 | `	for(;;){` |
|      49 | 5685 | `		if( n < 1 ){` |
|      15 | 5686 | `			break;` |
|       - | 5687 | `		}` |
|      57 | 5688 | `		for( i = 1 ; i < nArg ; i++ ){` |
|      39 | 5689 | `			pMap = (ph7_hashmap *)apArg[i]->x.pOther;` |
|      39 | 5690 | `			if( pEntry->iType == HASHMAP_BLOB_NODE ){` |
|      27 | 5691 | `				SyBlob *pKey = &pEntry->xKey.sKey;` |
|       - | 5692 | `				/* Blob lookup */` |
|      27 | 5693 | `				rc = HashmapLookupBlobKey(pMap,SyBlobData(pKey),SyBlobLength(pKey),0);` |
|      14 | 5694 | `			}else{` |
|       - | 5695 | `				/* Int key */` |
|      13 | 5696 | `				rc = HashmapLookupIntKey(pMap,pEntry->xKey.iKey,0);` |
|       - | 5697 | `			}` |
|      39 | 5698 | `			if( rc != SXRET_OK ){` |
|       - | 5699 | `				/* Key does not exist, break immediately */` |
|      17 | 5700 | `				break;` |
|       - | 5701 | `			}` |
|      12 | 5702 | `		}` |
|      35 | 5703 | `		if( i >= nArg ){` |
|       - | 5704 | `			/* Perform the insertion */` |
|      19 | 5705 | `			HashmapInsertNode((ph7_hashmap *)pArray->x.pOther,pEntry,TRUE);` |
|       9 | 5706 | `		}` |
|       - | 5707 | `		/* Point to the next entry */` |
|      35 | 5708 | `		pEntry = pEntry->pPrev; /* Reverse link */` |
|      35 | 5709 | `		n--;` |
|       1 | 5710 | `	}` |
|       - | 5711 | `	/* Return the freshly created array */` |
|      15 | 5712 | `	ph7_result_value(pCtx,pArray);` |
|      15 | 5713 | `	return PH7_OK;` |
|      15 | 5714 | `}` |
|       - | 5715 | `/*` |
|       - | 5716 | ` * array array_uintersect(array $array1 ,array $array2,...,$callback)` |
|       - | 5717 | ` *  Computes the intersection of arrays.` |
|       - | 5718 | ` * Parameters` |
|       - | 5719 | ` *  $array1` |
|       - | 5720 | ` *    The array to compare from` |
|       - | 5721 | ` *  $array2` |
|       - | 5722 | ` *    An array to compare against` |
|       - | 5723 | ` *  $...` |
|       - | 5724 | ` *   More arrays to compare against` |
|       - | 5725 | ` * $callback` |
|       - | 5726 | ` *  The callback comparison function.` |
|       - | 5727 | ` *  The comparison function must return an integer less than, equal to, or greater than zero` |
|       - | 5728 | ` *  if the first argument is considered to be respectively less than, equal to, or greater` |
|       - | 5729 | ` *  than the second.` |
|       - | 5730 | ` *     int callback ( mixed $a, mixed $b )` |
|       - | 5731 | ` * Return` |
|       - | 5732 | ` *  Returns an array containing all of the values in array1 whose values exist` |
|       - | 5733 | ` *  in all of the parameters. .` |
|       - | 5734 | ` * Note that NULL is returned on failure.` |
|       - | 5735 | ` */` |
|      26 | 5736 | `static int ph7_hashmap_uintersect(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       5 | 5737 | `{` |
|       - | 5738 | `	ph7_hashmap_node *pEntry;` |
|       - | 5739 | `	ph7_hashmap *pSrc,*pMap;` |
|       - | 5740 | `	ph7_value *pCallback;` |
|       - | 5741 | `	ph7_value *pArray;` |
|       - | 5742 | `	ph7_value *pVal;` |
|       - | 5743 | `	sxi32 rc;` |
|       - | 5744 | `	sxu32 n;` |
|       - | 5745 | `	int i;` |
|       - | 5746 |  |
|       - | 5747 | `	/* Ensure the argument count matches PHP behaviour. */` |
|      31 | 5748 | `	if( nArg < 2 ){` |
|       4 | 5749 | `		return PH7_VmThrowException(pCtx,` |
|       - | 5750 | `			"ArgumentCountError",` |
|       - | 5751 | `			"array_uintersect() expects at least 2 arguments, %d given",` |
|       1 | 5752 | `			nArg` |
|       - | 5753 | `			);` |
|       - | 5754 | `	}` |
|      29 | 5755 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|       4 | 5756 | `		return PH7_VmThrowException(pCtx,` |
|       - | 5757 | `			"TypeError",` |
|       - | 5758 | `			"array_uintersect(): Argument #1 ($array) must be of type array, %s given",` |
|       1 | 5759 | `			ph7_type_name(apArg[0])` |
|       - | 5760 | `			);` |
|       - | 5761 | `	}` |
|       - | 5762 |  |
|      27 | 5763 | `	if( nArg == 2 ){` |
|       - | 5764 | `		/* Only the original array and the callback were provided. */` |
|       - | 5765 | `		/* Validate the callback below in order to match PHP's parameter` |
|       - | 5766 | `		 * validation ordering. */` |
|       3 | 5767 | `	} else {` |
|       - | 5768 | `		/* Ensure intermediary arguments are arrays (matches PHP strict typing). */` |
|      39 | 5769 | `		for( i = 1 ; i < nArg - 1; i++ ){` |
|      23 | 5770 | `			if( !ph7_value_is_array(apArg[i]) ){` |
|       4 | 5771 | `				return PH7_VmThrowException(pCtx,` |
|       - | 5772 | `					"TypeError",` |
|       - | 5773 | `					"array_uintersect(): Argument #%d must be of type array, %s given",` |
|       1 | 5774 | `					i + 1,` |
|       2 | 5775 | `					ph7_type_name(apArg[i])` |
|       - | 5776 | `					);` |
|       - | 5777 | `			}` |
|      13 | 5778 | `		}` |
|       - | 5779 | `	}` |
|       - | 5780 |  |
|       - | 5781 | `	/* Identify the callback (always expected as the last argument). */` |
|      25 | 5782 | `	pCallback = apArg[nArg - 1];` |
|       - | 5783 | `	/* Validate the callback to match PHP's error messages. */` |
|      25 | 5784 | `	if( !ph7_value_is_callable(pCallback) ){` |
|      14 | 5785 | `		if( ph7_value_is_array(pCallback) ){` |
|       - | 5786 | `			/* PHP emits a special message when the array length is wrong.` |
|       - | 5787 | `			 * If the array has two elements but is still not callable (e.g. missing` |
|       - | 5788 | `			 * method / missing class), we must emit a more general error instead.` |
|       - | 5789 | `			 */` |
|       9 | 5790 | `			ph7_hashmap *pCb = (ph7_hashmap *)pCallback->x.pOther;` |
|       9 | 5791 | `			if( pCb->nEntry != 2 ){` |
|       4 | 5792 | `				return PH7_VmThrowException(pCtx,` |
|       - | 5793 | `					"TypeError",` |
|       - | 5794 | `					"array_uintersect(): Argument #%d must be a valid callback, array callback must have exactly two members",` |
|       1 | 5795 | `					nArg` |
|       - | 5796 | `					);` |
|       - | 5797 | `			}` |
|       - | 5798 | `			/* Try to provide a more precise error like PHP does for missing classes/methods. */` |
|       - | 5799 | `			{` |
|       6 | 5800 | `				ph7_value *pKey = (ph7_value *)SySetAt(&pCtx->pVm->aMemObj,pCb->pFirst->nValIdx);` |
|       6 | 5801 | `				ph7_value *pMethod = (ph7_value *)SySetAt(&pCtx->pVm->aMemObj,pCb->pFirst->pPrev->nValIdx);` |
|       6 | 5802 | `				if( pKey && pMethod && (pMethod->iFlags & MEMOBJ_STRING) ){` |
|       - | 5803 | `					int nMethodLen;` |
|       6 | 5804 | `					const char *zMethod = ph7_value_to_string(pMethod,&nMethodLen);` |
|       6 | 5805 | `					ph7_class *pClass = PH7_VmExtractClassFromValue(pCtx->pVm,pKey);` |
|       6 | 5806 | `					if( pClass ){` |
|       - | 5807 | `						/* Class exists but method is missing. */` |
|       4 | 5808 | `						return PH7_VmThrowException(pCtx,` |
|       - | 5809 | `							"TypeError",` |
|       - | 5810 | `							"array_uintersect(): Argument #%d must be a valid callback, class %s does not have a method \"%s\"",` |
|       1 | 5811 | `							nArg,` |
|       1 | 5812 | `							(const char *)SyStringData(&pClass->sName),` |
|       1 | 5813 | `							zMethod` |
|       - | 5814 | `							);` |
|       - | 5815 | `					}` |
|       - | 5816 | `					/* Class not found */` |
|       - | 5817 | `					{` |
|       - | 5818 | `						int nName;` |
|       3 | 5819 | `						const char *zName = ph7_value_to_string(pKey,&nName);` |
|       4 | 5820 | `						return PH7_VmThrowException(pCtx,` |
|       - | 5821 | `							"TypeError",` |
|       - | 5822 | `							"array_uintersect(): Argument #%d must be a valid callback, class \"%s\" not found",` |
|       1 | 5823 | `							nArg,` |
|       1 | 5824 | `							zName` |
|       - | 5825 | `							);` |
|       - | 5826 | `					}` |
|       - | 5827 | `				}` |
|       - | 5828 | `			}` |
|       - | 5829 | `			/* Fallback message */` |
|     ! 0 | 5830 | `			return PH7_VmThrowException(pCtx,` |
|       - | 5831 | `				"TypeError",` |
|       - | 5832 | `				"array_uintersect(): Argument #%d must be a valid callback, no array or string given",` |
|     ! 0 | 5833 | `				nArg` |
|       - | 5834 | `				);` |
|       - | 5835 | `		}` |
|       6 | 5836 | `		if( ph7_value_is_string(pCallback) ){` |
|       - | 5837 | `			int len;` |
|       3 | 5838 | `			const char *zName = ph7_value_to_string(pCallback, &len);` |
|       4 | 5839 | `			return PH7_VmThrowException(pCtx,` |
|       - | 5840 | `				"TypeError",` |
|       - | 5841 | `				"array_uintersect(): Argument #%d must be a valid callback, function \"%s\" not found or invalid function name",` |
|       1 | 5842 | `				nArg,` |
|       1 | 5843 | `				zName` |
|       - | 5844 | `				);` |
|       - | 5845 | `		}` |
|       4 | 5846 | `		return PH7_VmThrowException(pCtx,` |
|       - | 5847 | `			"TypeError",` |
|       - | 5848 | `			"array_uintersect(): Argument #%d must be a valid callback, no array or string given",` |
|       1 | 5849 | `			nArg` |
|       - | 5850 | `			);` |
|       - | 5851 | `	}` |
|       - | 5852 |  |
|      11 | 5853 | `	if( nArg == 2 ){` |
|       - | 5854 | `		/* Only the original array and the callback were provided. */` |
|       5 | 5855 | `		ph7_result_value(pCtx,apArg[0]);` |
|       5 | 5856 | `		return PH7_OK;` |
|       - | 5857 | `	}` |
|       - | 5858 |  |
|       - | 5859 | `	/* Create a new array */` |
|       7 | 5860 | `	pArray = ph7_context_new_array(pCtx);` |
|       7 | 5861 | `	if( pArray == 0 ){` |
|     ! 0 | 5862 | `		ph7_result_null(pCtx);` |
|     ! 0 | 5863 | `		return PH7_OK;` |
|       - | 5864 | `	}` |
|       - | 5865 | `	/* Point to the internal representation of the source hashmap */` |
|       7 | 5866 | `	pSrc = (ph7_hashmap *)apArg[0]->x.pOther;` |
|       - | 5867 | `	/* Perform the intersection */` |
|       7 | 5868 | `	pEntry = pSrc->pFirst;` |
|       7 | 5869 | `	n = pSrc->nEntry;` |
|       7 | 5870 | `	pCtx->pVm->iCmpCallbackExc = 0;` |
|       9 | 5871 | `	for(;;){` |
|      19 | 5872 | `		if( n < 1 ){` |
|       5 | 5873 | `			break;` |
|       - | 5874 | `		}` |
|       - | 5875 | `		/* Extract the node value */` |
|      15 | 5876 | `		pVal = HashmapExtractNodeValue(pEntry);` |
|      15 | 5877 | `		if( pVal ){` |
|      23 | 5878 | `			for( i = 1 ; i < nArg - 1; i++ ){` |
|      15 | 5879 | `				if( !ph7_value_is_array(apArg[i])) {` |
|       - | 5880 | `					/* ignore */` |
|     ! 0 | 5881 | `					continue;` |
|       - | 5882 | `				}` |
|       - | 5883 | `				/* Point to the internal representation of the hashmap */` |
|      15 | 5884 | `				pMap = (ph7_hashmap *)apArg[i]->x.pOther;` |
|       - | 5885 | `				/* Perform the lookup */` |
|      15 | 5886 | `				rc = HashmapFindValueByCallback(pMap,pVal,pCallback,0);` |
|      15 | 5887 | `				if( rc != SXRET_OK ){` |
|       - | 5888 | `					/* Value does not exist */` |
|       7 | 5889 | `					break;` |
|       - | 5890 | `				}` |
|       5 | 5891 | `			}` |
|      15 | 5892 | `			if( i >= (nArg-1) ){` |
|       - | 5893 | `				/* Perform the insertion */` |
|       9 | 5894 | `				HashmapInsertNode((ph7_hashmap *)pArray->x.pOther,pEntry,TRUE);` |
|       4 | 5895 | `			}` |
|       7 | 5896 | `		}` |
|      15 | 5897 | `		if( pCtx->pVm->iCmpCallbackExc ){` |
|       - | 5898 | `			/* The comparison callback raised: propagate so the dispatcher unwinds. */` |
|       3 | 5899 | `			pCtx->pVm->iCmpCallbackExc = 0;` |
|       3 | 5900 | `			return PH7_EXCEPTION;` |
|       - | 5901 | `		}` |
|       - | 5902 | `		/* Point to the next entry */` |
|      13 | 5903 | `		pEntry = pEntry->pPrev; /* Reverse link */` |
|      13 | 5904 | `		n--;` |
|       1 | 5905 | `	}` |
|       - | 5906 | `	/* Return the freshly created array */` |
|       5 | 5907 | `	ph7_result_value(pCtx,pArray);` |
|       5 | 5908 | `	return PH7_OK;` |
|      18 | 5909 | `}` |
|       - | 5910 | `/*` |
|       - | 5911 | ` * array array_fill(int $start_index,int $num,var $value)` |
|       - | 5912 | ` *  Fill an array with values.` |
|       - | 5913 | ` * Parameters` |
|       - | 5914 | ` *  $start_index` |
|       - | 5915 | ` *    The first index of the returned array.` |
|       - | 5916 | ` *  $num` |
|       - | 5917 | ` *   Number of elements to insert.` |
|       - | 5918 | ` *  $value` |
|       - | 5919 | ` *    Value to use for filling.` |
|       - | 5920 | ` * Return` |
|       - | 5921 | ` *  The filled array or null on failure.` |
|       - | 5922 | ` */` |
|     238 | 5923 | `static int ph7_hashmap_fill(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       5 | 5924 | `{` |
|       - | 5925 | `	ph7_value *pArray;` |
|       - | 5926 | `	int i,nEntry;` |
|       - | 5927 |  |
|       - | 5928 | `	/* PHP enforces argument count and type checks. */` |
|     243 | 5929 | `	if( nArg != 3 ){` |
|       - | 5930 | `		/* wrong number of arguments -> ArgumentCountError */` |
|       8 | 5931 | `		return PH7_VmThrowException(pCtx,` |
|       - | 5932 | `			"ArgumentCountError",` |
|       - | 5933 | `			"array_fill() expects exactly 3 arguments, %d given",` |
|       2 | 5934 | `			nArg` |
|       - | 5935 | `			);` |
|       - | 5936 | `	}` |
|       - | 5937 |  |
|       - | 5938 | `	/* Argument #1: start index must be convertible to int.  Accept booleans,` |
|       - | 5939 | `	 * floats, and numeric strings (including those with decimal point) by` |
|       - | 5940 | `	 * allowing them through the conversion.  Only arrays, objects, resources` |
|       - | 5941 | `	 * and NULLs are rejected outright. */` |
|     350 | 5942 | `	if( ph7_value_is_array(apArg[0]) \|\| ph7_value_is_object(apArg[0]) \|\|` |
|     353 | 5943 | `		ph7_value_is_resource(apArg[0]) \|\| ph7_value_is_null(apArg[0]) ){` |
|       4 | 5944 | `		return PH7_VmThrowException(pCtx,` |
|       - | 5945 | `			"TypeError",` |
|       - | 5946 | `			"array_fill(): Argument #1 ($start_index) must be of type int, %s given",` |
|       1 | 5947 | `			ph7_type_name(apArg[0])` |
|       - | 5948 | `			);` |
|       - | 5949 | `	}` |
|     236 | 5950 | `	if( ph7_value_is_string(apArg[0]) ){` |
|       - | 5951 | `		int len;` |
|       8 | 5952 | `		sxu8 bReal = FALSE;` |
|       8 | 5953 | `		const char *zStr = ph7_value_to_string(apArg[0], &len);` |
|       8 | 5954 | `		if( SyStrIsNumeric(zStr, len, &bReal, 0) != SXRET_OK ){` |
|       - | 5955 | `			/* Non‑numeric string is an error. */` |
|       3 | 5956 | `			return PH7_VmThrowException(pCtx,` |
|       - | 5957 | `				"TypeError",` |
|       - | 5958 | `				"array_fill(): Argument #1 ($start_index) must be of type int, string given"` |
|       - | 5959 | `				);` |
|       - | 5960 | `		}` |
|       5 | 5961 | `		if( bReal ){` |
|       - | 5962 | `			/* float-string -> deprecation warning */` |
|       4 | 5963 | `			ph7_context_throw_error_format(pCtx, E_DEPRECATED,` |
|       - | 5964 | `				"Implicit conversion from float-string \"%s\" to int loses precision",` |
|       1 | 5965 | `				zStr` |
|       - | 5966 | `				);` |
|       1 | 5967 | `		}` |
|       2 | 5968 | `	}` |
|       - | 5969 |  |
|       - | 5970 | `	/* Argument #2: count must be convertible to non-negative int.  Allow booleans,` |
|       - | 5971 | `	 * floats and numeric strings; reject arrays, objects, resources and NULL. */` |
|     345 | 5972 | `	if( ph7_value_is_array(apArg[1]) \|\| ph7_value_is_object(apArg[1]) \|\|` |
|     348 | 5973 | `		ph7_value_is_resource(apArg[1]) \|\| ph7_value_is_null(apArg[1]) ){` |
|     ! 0 | 5974 | `		return PH7_VmThrowException(pCtx,` |
|       - | 5975 | `			"TypeError",` |
|       - | 5976 | `			"array_fill(): Argument #2 ($count) must be of type int, %s given",` |
|     ! 0 | 5977 | `			ph7_type_name(apArg[1])` |
|       - | 5978 | `			);` |
|       - | 5979 | `	}` |
|     233 | 5980 | `	if( ph7_value_is_string(apArg[1]) ){` |
|       - | 5981 | `		int len;` |
|       3 | 5982 | `		sxu8 bReal = FALSE;` |
|       3 | 5983 | `		const char *zStr = ph7_value_to_string(apArg[1], &len);` |
|       3 | 5984 | `		if( SyStrIsNumeric(zStr, len, &bReal, 0) != SXRET_OK ){` |
|       3 | 5985 | `			return PH7_VmThrowException(pCtx,` |
|       - | 5986 | `				"TypeError",` |
|       - | 5987 | `				"array_fill(): Argument #2 ($count) must be of type int, string given"` |
|       - | 5988 | `				);` |
|       - | 5989 | `		}` |
|     ! 0 | 5990 | `	}` |
|       - | 5991 | `	/* Note: booleans and floats (including fractional) are now accepted; they` |
|       - | 5992 | `	 * will be converted by ph7_value_to_int below. */` |
|     230 | 5993 | `	if( ph7_value_is_float(apArg[1]) ){` |
|       5 | 5994 | `		double d = ph7_value_to_double(apArg[1]);` |
|       - | 5995 | `		/* avoid hiding outer 'i' (loop index) */` |
|       5 | 5996 | `		sxi64 i64 = (sxi64)d;` |
|       5 | 5997 | `		if( d != (double)i64 ){` |
|       7 | 5998 | `			ph7_context_throw_error_format(pCtx, E_DEPRECATED,` |
|       - | 5999 | `				"Implicit conversion from float %g to int loses precision",` |
|       2 | 6000 | `				d` |
|       - | 6001 | `				);` |
|       2 | 6002 | `		}` |
|       2 | 6003 | `	}` |
|       - | 6004 |  |
|       - | 6005 | `	/* Total number of entries to insert */` |
|     230 | 6006 | `	nEntry = ph7_value_to_int(apArg[1]);` |
|       - | 6007 | `	/* Reject negative counts with a ValueError like PHP. */` |
|     230 | 6008 | `	if( nEntry < 0 ){` |
|       3 | 6009 | `		return PH7_VmThrowException(pCtx,` |
|       - | 6010 | `			"ValueError",` |
|       - | 6011 | `			"array_fill(): Argument #2 ($count) must be greater than or equal to 0"` |
|       - | 6012 | `			);` |
|       - | 6013 | `	}` |
|       - | 6014 |  |
|       - | 6015 | `	/* If zero elements were requested, return an empty array without allocating */` |
|     227 | 6016 | `	if( nEntry == 0 ){` |
|       7 | 6017 | `		ph7_result_value(pCtx, ph7_context_new_array(pCtx));` |
|       7 | 6018 | `		return PH7_OK;` |
|       - | 6019 | `	}` |
|       - | 6020 |  |
|       - | 6021 | `	/* Create a new array */` |
|     221 | 6022 | `	pArray = ph7_context_new_array(pCtx);` |
|     221 | 6023 | `	if( pArray == 0 ){` |
|     ! 0 | 6024 | `		return PH7_ContextMemoryError(pCtx);` |
|       - | 6025 | `	}` |
|       - | 6026 |  |
|       - | 6027 | `	/* Insert the first entry alone because it has its own key */` |
|     221 | 6028 | `	if( ph7_array_add_intkey_elem(pArray, ph7_value_to_int(apArg[0]), apArg[2]) != SXRET_OK ){` |
|     ! 0 | 6029 | `		return PH7_ContextMemoryError(pCtx);` |
|       - | 6030 | `	}` |
|       - | 6031 | `	/* Repeat insertion of the desired value */` |
| 2117583 | 6032 | `	for( i = 1 ; i < nEntry ; i++ ){` |
| 2117363 | 6033 | `		if( ph7_array_add_elem(pArray, 0/*Automatic index assign */, apArg[2]) != SXRET_OK ){` |
|       - | 6034 | `			/* Allocation failure: surface a fatal instead of a partial array */` |
|     ! 0 | 6035 | `			return PH7_ContextMemoryError(pCtx);` |
|       - | 6036 | `		}` |
| 1058682 | 6037 | `	}` |
|       - | 6038 | `	/* Return the filled array */` |
|     221 | 6039 | `	ph7_result_value(pCtx, pArray);` |
|     221 | 6040 | `	return PH7_OK;` |
|     124 | 6041 | `}` |
|       - | 6042 | `/*` |
|       - | 6043 | ` * array array_fill_keys(array $input,mixed $value)` |
|       - | 6044 | ` *  Fill an array with values, specifying keys.` |
|       - | 6045 | ` * Parameters` |
|       - | 6046 | ` *  $input` |
|       - | 6047 | ` *   Array of values that will be used as key.` |
|       - | 6048 | ` *  $value` |
|       - | 6049 | ` *    Value to use for filling.` |
|       - | 6050 | ` * Return` |
|       - | 6051 | ` *  The filled array.` |
|       - | 6052 | ` * Throws` |
|       - | 6053 | ` *  ValueError if $input is not an array.` |
|       - | 6054 | ` */` |
|      26 | 6055 | `static int ph7_hashmap_fill_keys(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       5 | 6056 | `{` |
|       - | 6057 | `	ph7_hashmap_node *pEntry;` |
|       - | 6058 | `	ph7_hashmap *pSrc;` |
|       - | 6059 | `	ph7_value *pArray;` |
|       - | 6060 | `	sxu32 n;` |
|       - | 6061 | `	/* PHP enforces exactly 2 arguments. */` |
|      31 | 6062 | `	if( nArg != 2 ){` |
|      12 | 6063 | `		return PH7_VmThrowException(pCtx,` |
|       - | 6064 | `			"ArgumentCountError",` |
|       - | 6065 | `			"array_fill_keys() expects exactly 2 arguments, %d given",` |
|       3 | 6066 | `			nArg` |
|       - | 6067 | `			);` |
|       - | 6068 | `	}` |
|       - | 6069 | `	/* Make sure we are dealing with a valid hashmap */` |
|      23 | 6070 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|       8 | 6071 | `		return PH7_VmThrowException(pCtx,` |
|       - | 6072 | `			"TypeError",` |
|       - | 6073 | `			"array_fill_keys(): Argument #1 ($keys) must be of type array, %s given",` |
|       2 | 6074 | `			ph7_type_name(apArg[0])` |
|       - | 6075 | `			);` |
|       - | 6076 | `	}` |
|       - | 6077 | `	/* Point to the internal representation of the input hashmap */` |
|      17 | 6078 | `	pSrc = (ph7_hashmap *)apArg[0]->x.pOther;` |
|       - | 6079 | `	/* Create a new array */` |
|      17 | 6080 | `	pArray = ph7_context_new_array(pCtx);` |
|      17 | 6081 | `	if( pArray == 0 ){` |
|     ! 0 | 6082 | `		ph7_result_null(pCtx);` |
|     ! 0 | 6083 | `		return PH7_OK;` |
|       - | 6084 | `	}` |
|       - | 6085 | `	/* Perform the requested operation */` |
|      17 | 6086 | `	pEntry = pSrc->pFirst;` |
|      45 | 6087 | `	for( n = 0 ; n < pSrc->nEntry ; n++ ){` |
|      29 | 6088 | `		ph7_array_add_elem(pArray,HashmapExtractNodeValue(pEntry),apArg[1]);` |
|       - | 6089 | `		/* Point to the next entry */` |
|      29 | 6090 | `		pEntry = pEntry->pPrev; /* Reverse link */` |
|      15 | 6091 | `	}` |
|       - | 6092 | `	/* Return the filled array */` |
|      17 | 6093 | `	ph7_result_value(pCtx,pArray);` |
|      17 | 6094 | `	return PH7_OK;` |
|      18 | 6095 | `}` |
|       - | 6096 | `/*` |
|       - | 6097 | ` * array array_combine(array $keys,array $values)` |
|       - | 6098 | ` *  Creates an array by using one array for keys and another for its values.` |
|       - | 6099 | ` * Parameters` |
|       - | 6100 | ` *  $keys` |
|       - | 6101 | ` *    Array of keys to be used.` |
|       - | 6102 | ` * $values` |
|       - | 6103 | ` *   Array of values to be used.` |
|       - | 6104 | ` * Return` |
|       - | 6105 | ` *  Returns the combined array. Otherwise FALSE if the number of elements` |
|       - | 6106 | ` *  for each array isn't equal or if one of the given arguments is` |
|       - | 6107 | ` *  not an array.` |
|       - | 6108 | ` */` |
|      18 | 6109 | `static int ph7_hashmap_combine(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       5 | 6110 | `{` |
|       - | 6111 | `	ph7_hashmap_node *pKe,*pVe;` |
|       - | 6112 | `	ph7_hashmap *pKey,*pValue;` |
|       - | 6113 | `	ph7_value *pArray;` |
|       - | 6114 | `	sxu32 n;` |
|       - | 6115 | `	/* PHP enforces argument count and type checks. */` |
|      23 | 6116 | `	if( nArg != 2 ){` |
|       - | 6117 | `		/* wrong number of arguments -> ArgumentCountError */` |
|       4 | 6118 | `		return PH7_VmThrowException(pCtx,` |
|       - | 6119 | `			"ArgumentCountError",` |
|       - | 6120 | `			"array_combine() expects exactly 2 arguments, %d given",` |
|       1 | 6121 | `			nArg` |
|       - | 6122 | `			);` |
|       - | 6123 | `	}` |
|       - | 6124 | `	/* Validate argument types individually so we can report the correct` |
|       - | 6125 | `	 * argument index in the error message. */` |
|      20 | 6126 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|       4 | 6127 | `		return PH7_VmThrowException(pCtx,` |
|       - | 6128 | `			"TypeError",` |
|       - | 6129 | `			"array_combine(): Argument #1 ($keys) must be of type array, %s given",` |
|       1 | 6130 | `			ph7_type_name(apArg[0])` |
|       - | 6131 | `			);` |
|       - | 6132 | `	}` |
|      17 | 6133 | `	if( !ph7_value_is_array(apArg[1]) ){` |
|       4 | 6134 | `		return PH7_VmThrowException(pCtx,` |
|       - | 6135 | `			"TypeError",` |
|       - | 6136 | `			"array_combine(): Argument #2 ($values) must be of type array, %s given",` |
|       2 | 6137 | `			ph7_type_name(apArg[1])` |
|       - | 6138 | `			);` |
|       - | 6139 | `	}` |
|       - | 6140 | `	/* Point to the internal representation of the input hashmaps */` |
|      14 | 6141 | `	pKey   = (ph7_hashmap *)apArg[0]->x.pOther;` |
|      14 | 6142 | `	pValue = (ph7_hashmap *)apArg[1]->x.pOther;` |
|      14 | 6143 | `	if( pKey->nEntry != pValue->nEntry ){` |
|       - | 6144 | `		/* Length mismatch -> ValueError */` |
|       3 | 6145 | `		return PH7_VmThrowException(pCtx,` |
|       - | 6146 | `			"ValueError",` |
|       - | 6147 | `			"array_combine(): Argument #1 ($keys) and argument #2 ($values) must have the same number of elements"` |
|       - | 6148 | `			);` |
|       - | 6149 | `	}` |
|       - | 6150 | `	/* Create a new array */` |
|      11 | 6151 | `	pArray = ph7_context_new_array(pCtx);` |
|      11 | 6152 | `	if( pArray == 0 ){` |
|     ! 0 | 6153 | `		ph7_result_bool(pCtx,0);` |
|     ! 0 | 6154 | `		return PH7_OK;` |
|       - | 6155 | `	}` |
|       - | 6156 | `	/* Perform the requested operation */` |
|      11 | 6157 | `	pKe = pKey->pFirst;` |
|      11 | 6158 | `	pVe = pValue->pFirst;` |
|      33 | 6159 | `	for( n = 0 ; n < pKey->nEntry ; n++ ){` |
|      23 | 6160 | `		ph7_value *pKeyVal = HashmapExtractNodeValue(pKe);` |
|      23 | 6161 | `		ph7_value *pValVal = HashmapExtractNodeValue(pVe);` |
|       - | 6162 | `		/* PHP treats floats used as keys in array_combine differently than` |
|       - | 6163 | `		 * ordinary offset access: the float is stringified rather than` |
|       - | 6164 | `		 * truncated.  To emulate this behavior we create a temporary copy of` |
|       - | 6165 | `		 * the value when it is a float and convert the copy to string.  The` |
|       - | 6166 | `		 * original array must not be mutated. */` |
|      23 | 6167 | `		ph7_value *pKeyCopy = pKeyVal;` |
|      23 | 6168 | `		if( ph7_value_is_float(pKeyVal) ){` |
|       5 | 6169 | `			ph7_value *pTmpKey = ph7_context_new_scalar(pCtx);` |
|       5 | 6170 | `			if( pTmpKey ){` |
|       5 | 6171 | `				PH7_MemObjStore(pKeyVal,pTmpKey);` |
|       - | 6172 | `				/* Convert copy to string so it becomes "1.5" or "2" etc. */` |
|       5 | 6173 | `				PH7_MemObjToString(pTmpKey);` |
|       5 | 6174 | `				pKeyCopy = pTmpKey;` |
|       2 | 6175 | `			}` |
|       2 | 6176 | `		}` |
|      23 | 6177 | `		ph7_array_add_elem(pArray,pKeyCopy,pValVal);` |
|       - | 6178 | `		/* Point to the next entry */` |
|      23 | 6179 | `		pKe = pKe->pPrev; /* Reverse link */` |
|      23 | 6180 | `		pVe = pVe->pPrev;` |
|      12 | 6181 | `	}` |
|       - | 6182 | `	/* Return the filled array */` |
|      11 | 6183 | `	ph7_result_value(pCtx,pArray);` |
|      11 | 6184 | `	return PH7_OK;` |
|      14 | 6185 | `}` |
|       - | 6186 | `/*` |
|       - | 6187 | ` * array array_reverse(array $array [,bool $preserve_keys = false ])` |
|       - | 6188 | ` *  Return an array with elements in reverse order.` |
|       - | 6189 | ` * Parameters` |
|       - | 6190 | ` *  $array` |
|       - | 6191 | ` *   The input array.` |
|       - | 6192 | ` *  $preserve_keys (optional)` |
|       - | 6193 | ` *   If set to TRUE keys are preserved.` |
|       - | 6194 | ` * Return` |
|       - | 6195 | ` *  The reversed array.` |
|       - | 6196 | ` */` |
|      20 | 6197 | `static int ph7_hashmap_reverse(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       3 | 6198 | `{` |
|       - | 6199 | `	ph7_hashmap_node *pEntry;` |
|       - | 6200 | `	ph7_hashmap *pSrc;` |
|       - | 6201 | `	ph7_value *pArray;` |
|       - | 6202 | `	int bPreserve;` |
|       - | 6203 | `	sxu32 n;` |
|      23 | 6204 | `	if( nArg < 1 ){` |
|       4 | 6205 | `		return PH7_VmThrowException(pCtx,` |
|       - | 6206 | `			"ArgumentCountError",` |
|       - | 6207 | `			"array_reverse() expects at least 1 argument, %d given",` |
|       1 | 6208 | `			nArg` |
|       - | 6209 | `			);` |
|       - | 6210 | `	}` |
|       - | 6211 | `	/* Make sure we are dealing with a valid hashmap */` |
|      20 | 6212 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|       4 | 6213 | `		return PH7_VmThrowException(pCtx,` |
|       - | 6214 | `			"TypeError",` |
|       - | 6215 | `			"array_reverse(): Argument #1 ($array) must be of type array, %s given",` |
|       1 | 6216 | `			ph7_type_name(apArg[0])` |
|       - | 6217 | `			);` |
|       - | 6218 | `	}` |
|      17 | 6219 | `	bPreserve = FALSE;` |
|      17 | 6220 | `	if( nArg > 1 ){` |
|       7 | 6221 | `		bPreserve = ph7_value_to_bool(apArg[1]);` |
|       3 | 6222 | `	}` |
|       - | 6223 | `	/* Point to the internal representation of the input hashmap */` |
|      17 | 6224 | `	pSrc = (ph7_hashmap *)apArg[0]->x.pOther;` |
|       - | 6225 | `	/* Create a new array */` |
|      17 | 6226 | `	pArray = ph7_context_new_array(pCtx);` |
|      17 | 6227 | `	if( pArray == 0 ){` |
|     ! 0 | 6228 | `		ph7_result_null(pCtx);` |
|     ! 0 | 6229 | `		return PH7_OK;` |
|       - | 6230 | `	}` |
|       - | 6231 | `	/* Perform the requested operation */` |
|      17 | 6232 | `	pEntry = pSrc->pLast;` |
|      55 | 6233 | `	for( n = 0 ; n < pSrc->nEntry ; n++ ){` |
|       - | 6234 | `		/* String keys are always preserved; preserve_keys only affects int keys */` |
|      39 | 6235 | `		int bKeep = (pEntry->iType == HASHMAP_INT_NODE) ? bPreserve : TRUE;` |
|      39 | 6236 | `		HashmapInsertNode((ph7_hashmap *)pArray->x.pOther,pEntry,bKeep);` |
|       - | 6237 | `		/* Point to the previous entry */` |
|      39 | 6238 | `		pEntry = pEntry->pNext; /* Reverse link */` |
|      20 | 6239 | `	}` |
|      17 | 6240 | `	ph7_result_value(pCtx,pArray);` |
|      17 | 6241 | `	return PH7_OK;` |
|      13 | 6242 | `}` |
|       - | 6243 | `/*` |
|       - | 6244 | ` * array array_unique(array $array, int $flags = SORT_STRING)` |
|       - | 6245 | ` *  Removes duplicate values from an array.` |
|       - | 6246 | ` * Parameters` |
|       - | 6247 | ` *  $array` |
|       - | 6248 | ` *   The input array.` |
|       - | 6249 | ` *  $flags` |
|       - | 6250 | ` *   The optional second parameter may be used to modify the comparison` |
|       - | 6251 | ` *   behavior using these values:` |
|       - | 6252 | ` *     SORT_REGULAR - compare items normally (don't change types)` |
|       - | 6253 | ` *     SORT_NUMERIC - compare items numerically` |
|       - | 6254 | ` *     SORT_STRING  - compare items as strings` |
|       - | 6255 | ` * Return` |
|       - | 6256 | ` *  The filtered array.` |
|       - | 6257 | ` */` |
|      24 | 6258 | `static int ph7_hashmap_unique(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       4 | 6259 | `{` |
|       - | 6260 | `	ph7_hashmap_node *pEntry;` |
|       - | 6261 | `	ph7_value *pNeedle;` |
|       - | 6262 | `	ph7_hashmap *pSrc;` |
|       - | 6263 | `	ph7_value *pArray;` |
|       - | 6264 | `	int bStrict;` |
|       - | 6265 | `	sxi32 rc;` |
|       - | 6266 | `	sxu32 n;` |
|      28 | 6267 | `	if( nArg < 1 ){` |
|       - | 6268 | `		/* Missing arguments, throw ArgumentCountError */` |
|       3 | 6269 | `		return PH7_VmThrowException(pCtx,` |
|       - | 6270 | `			"ArgumentCountError",` |
|       - | 6271 | `			"array_unique() expects at least 1 argument, 0 given"` |
|       - | 6272 | `			);` |
|       - | 6273 | `	}` |
|      25 | 6274 | `	if( nArg > 2 ){` |
|       - | 6275 | `		/* Too many arguments, throw ArgumentCountError */` |
|       4 | 6276 | `		return PH7_VmThrowException(pCtx,` |
|       - | 6277 | `			"ArgumentCountError",` |
|       - | 6278 | `			"array_unique() expects at most 2 arguments, %d given",` |
|       1 | 6279 | `			nArg` |
|       - | 6280 | `			);` |
|       - | 6281 | `	}` |
|       - | 6282 | `	/* Make sure we are dealing with a valid hashmap */` |
|      22 | 6283 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|       - | 6284 | `		/* Type mismatch, throw TypeError */` |
|       4 | 6285 | `		return PH7_VmThrowException(pCtx,` |
|       - | 6286 | `			"TypeError",` |
|       - | 6287 | `			"array_unique(): Argument #1 ($array) must be of type array, %s given",` |
|       1 | 6288 | `			ph7_type_name(apArg[0])` |
|       - | 6289 | `			);` |
|       - | 6290 | `	}` |
|      19 | 6291 | `	bStrict = FALSE;` |
|       - | 6292 | `	/* Point to the internal representation of the input hashmap */` |
|      19 | 6293 | `	pSrc = (ph7_hashmap *)apArg[0]->x.pOther;` |
|       - | 6294 | `	/* Create a new array */` |
|      19 | 6295 | `	pArray = ph7_context_new_array(pCtx);` |
|      19 | 6296 | `	if( pArray == 0 ){` |
|     ! 0 | 6297 | `		ph7_result_null(pCtx);` |
|     ! 0 | 6298 | `		return PH7_OK;` |
|       - | 6299 | `	}` |
|       - | 6300 | `	/* Perform the requested operation */` |
|      19 | 6301 | `	pEntry = pSrc->pFirst;` |
|      83 | 6302 | `	for( n = 0 ; n < pSrc->nEntry ; n++ ){` |
|      65 | 6303 | `		pNeedle = HashmapExtractNodeValue(pEntry);` |
|      65 | 6304 | `		rc = SXERR_NOTFOUND;` |
|      65 | 6305 | `		if( pNeedle ){` |
|      65 | 6306 | `			rc = HashmapFindValue((ph7_hashmap *)pArray->x.pOther,pNeedle,0,bStrict);` |
|      32 | 6307 | `		}` |
|      65 | 6308 | `		if( rc != SXRET_OK ){` |
|       - | 6309 | `			/* Perform the insertion */` |
|      37 | 6310 | `			HashmapInsertNode((ph7_hashmap *)pArray->x.pOther,pEntry,TRUE);` |
|      18 | 6311 | `		}` |
|       - | 6312 | `		/* Point to the next entry */` |
|      65 | 6313 | `		pEntry = pEntry->pPrev; /* Reverse link */` |
|      33 | 6314 | `	}` |
|       - | 6315 | `	/* Return the freshly created array */` |
|      19 | 6316 | `	ph7_result_value(pCtx,pArray);` |
|      19 | 6317 | `	return PH7_OK;` |
|      16 | 6318 | `}` |
|       - | 6319 | `/*` |
|       - | 6320 | ` * array array_flip(array $input)` |
|       - | 6321 | ` *  Exchanges all keys with their associated values in an array.` |
|       - | 6322 | ` * Parameter` |
|       - | 6323 | ` *  $input` |
|       - | 6324 | ` *   Input array.` |
|       - | 6325 | ` * Return` |
|       - | 6326 | ` *   The flipped array on success or NULL on failure.` |
|       - | 6327 | ` */` |
|      34 | 6328 | `static int ph7_hashmap_flip(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       5 | 6329 | `{` |
|       - | 6330 | `	ph7_hashmap_node *pEntry;` |
|       - | 6331 | `	ph7_hashmap *pSrc;` |
|       - | 6332 | `	ph7_value *pArray;` |
|       - | 6333 | `	ph7_value *pKey;` |
|       - | 6334 | `	ph7_value sVal;` |
|       - | 6335 | `	sxu32 n;` |
|       - | 6336 |  |
|       - | 6337 | `	/* PHP requires exactly one argument */` |
|      39 | 6338 | `	if( nArg != 1 ){` |
|       - | 6339 | `		/* Use ArgumentCountError like other array helpers */` |
|       8 | 6340 | `		return PH7_VmThrowException(pCtx,` |
|       - | 6341 | `			"ArgumentCountError",` |
|       - | 6342 | `			"array_flip() expects exactly 1 argument, %d given",` |
|       2 | 6343 | `			nArg` |
|       - | 6344 | `			);` |
|       - | 6345 | `	}` |
|       - | 6346 | `	/* Make sure we are dealing with a valid hashmap */` |
|      33 | 6347 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|       - | 6348 | `		/* Type mismatch -> TypeError */` |
|       8 | 6349 | `		return PH7_VmThrowException(pCtx,` |
|       - | 6350 | `			"TypeError",` |
|       - | 6351 | `			"array_flip(): Argument #1 ($array) must be of type array, %s given",` |
|       2 | 6352 | `			ph7_type_name(apArg[0])` |
|       - | 6353 | `			);` |
|       - | 6354 | `	}` |
|       - | 6355 | `	/* Point to the internal representation of the input hashmap */` |
|      27 | 6356 | `	pSrc = (ph7_hashmap *)apArg[0]->x.pOther;` |
|       - | 6357 | `	/* Create a new array */` |
|      27 | 6358 | `	pArray = ph7_context_new_array(pCtx);` |
|      27 | 6359 | `	if( pArray == 0 ){` |
|     ! 0 | 6360 | `		ph7_result_null(pCtx);` |
|     ! 0 | 6361 | `		return PH7_OK;` |
|       - | 6362 | `	}` |
|       - | 6363 | `	/* Start processing */` |
|      27 | 6364 | `	pEntry = pSrc->pFirst;` |
|   22263 | 6365 | `	for( n = 0 ; n < pSrc->nEntry ; n++ ){` |
|       - | 6366 | `		/* Extract the node value (will become a key in the result) */` |
|   22237 | 6367 | `		pKey = HashmapExtractNodeValue(pEntry);` |
|   22237 | 6368 | `		if( pKey ){` |
|       - | 6369 | `			/* NULL values are not valid keys either, PHP emits a warning */` |
|   22237 | 6370 | `			if( pKey->iFlags & MEMOBJ_NULL ){` |
|       3 | 6371 | `				PH7_VmThrowError(pCtx->pVm,0,PH7_CTX_WARNING,` |
|       - | 6372 | `					"array_flip(): Can only flip string and integer values, entry skipped"` |
|       - | 6373 | `					);` |
|   22236 | 6374 | `			}else if( (pKey->iFlags & MEMOBJ_INT) \|\| (pKey->iFlags & MEMOBJ_STRING) ){` |
|       - | 6375 | `				/* Prepare the value for insertion (original key) */` |
|   22227 | 6376 | `				if( pEntry->iType == HASHMAP_INT_NODE ){` |
|   20001 | 6377 | `					PH7_MemObjInitFromInt(pSrc->pVm,&sVal,pEntry->xKey.iKey);` |
|   10001 | 6378 | `				}else{` |
|       - | 6379 | `					SyString sStr;` |
|    2227 | 6380 | `					SyStringInitFromBuf(&sStr,SyBlobData(&pEntry->xKey.sKey),SyBlobLength(&pEntry->xKey.sKey));` |
|    2227 | 6381 | `					PH7_MemObjInitFromString(pSrc->pVm,&sVal,&sStr);` |
|       - | 6382 | `				}` |
|       - | 6383 | `				/* Perform the insertion */` |
|   22227 | 6384 | `				ph7_array_add_elem(pArray,pKey,&sVal);` |
|       - | 6385 | `				/* Safely release the value because each inserted entry` |
|       - | 6386 | `				 * has its own private copy of the value.` |
|       - | 6387 | `				 */` |
|   22227 | 6388 | `				PH7_MemObjRelease(&sVal);` |
|   11114 | 6389 | `			}else{` |
|       - | 6390 | `				/* Unsupported value type -> emit warning and skip the entry */` |
|       9 | 6391 | `				PH7_VmThrowError(pCtx->pVm,0,PH7_CTX_WARNING,` |
|       - | 6392 | `					"array_flip(): Can only flip string and integer values, entry skipped"` |
|       - | 6393 | `					);` |
|       - | 6394 | `			}` |
|   11118 | 6395 | `		}` |
|       - | 6396 | `		/* Point to the next entry */` |
|   22237 | 6397 | `		pEntry = pEntry->pPrev; /* Reverse link */` |
|   11119 | 6398 | `	}` |
|       - | 6399 | `	/* Return the freshly created array */` |
|      27 | 6400 | `	ph7_result_value(pCtx,pArray);` |
|      27 | 6401 | `	return PH7_OK;` |
|      22 | 6402 | `}` |
|       - | 6403 | `/*` |
|       - | 6404 | ` * number array_sum(array $array )` |
|       - | 6405 | ` *  Calculate the sum of values in an array.` |
|       - | 6406 | ` * Parameters` |
|       - | 6407 | ` *  $array: The input array.` |
|       - | 6408 | ` * Return` |
|       - | 6409 | ` *  Returns the sum of values as an integer or float.` |
|       - | 6410 | ` */` |
|      24 | 6411 | `static void DoubleSum(ph7_context *pCtx,ph7_hashmap *pMap)` |
|       1 | 6412 | `{` |
|       - | 6413 | `	ph7_hashmap_node *pEntry;` |
|       - | 6414 | `	ph7_value *pObj;` |
|      25 | 6415 | `	double dSum = 0;` |
|       - | 6416 | `	sxu32 n;` |
|      25 | 6417 | `	pEntry = pMap->pFirst;` |
|      91 | 6418 | `	for( n = 0 ; n < pMap->nEntry ; n++ ){` |
|      67 | 6419 | `		pObj = HashmapExtractNodeValue(pEntry);` |
|      67 | 6420 | `		if( pObj ){` |
|      67 | 6421 | `			if( pObj->iFlags & MEMOBJ_REAL ){` |
|      29 | 6422 | `				dSum += pObj->rVal;` |
|      53 | 6423 | `			}else if( pObj->iFlags & (MEMOBJ_INT\|MEMOBJ_BOOL) ){` |
|      21 | 6424 | `				dSum += (double)pObj->x.iVal;` |
|      29 | 6425 | `			}else if( pObj->iFlags & MEMOBJ_STRING ){` |
|      15 | 6426 | `				if( SyBlobLength(&pObj->sBlob) > 0 ){` |
|      13 | 6427 | `					double dv = 0;` |
|      13 | 6428 | `					SyStrToReal((const char *)SyBlobData(&pObj->sBlob),SyBlobLength(&pObj->sBlob),(void *)&dv,0);` |
|      13 | 6429 | `					dSum += dv;` |
|       7 | 6430 | `				}` |
|      12 | 6431 | `			}else if( pObj->iFlags & MEMOBJ_HASHMAP ){` |
|       3 | 6432 | `				PH7_VmThrowError(pCtx->pVm,0,PH7_CTX_WARNING,` |
|       - | 6433 | `					"array_sum(): Addition is not supported on type array");` |
|       4 | 6434 | `			}else if( pObj->iFlags & MEMOBJ_OBJ ){` |
|     ! 0 | 6435 | `				PH7_VmThrowError(pCtx->pVm,0,PH7_CTX_WARNING,` |
|       - | 6436 | `					"array_sum(): Addition is not supported on type object");` |
|       3 | 6437 | `			}else if( pObj->iFlags & MEMOBJ_RES ){` |
|     ! 0 | 6438 | `				PH7_VmThrowError(pCtx->pVm,0,PH7_CTX_WARNING,` |
|       - | 6439 | `					"array_sum(): Addition is not supported on type resource");` |
|     ! 0 | 6440 | `			}` |
|       - | 6441 | `			/* NULL is silently treated as 0 (matches PHP) */` |
|      33 | 6442 | `		}` |
|       - | 6443 | `		/* Point to the next entry */` |
|      67 | 6444 | `		pEntry = pEntry->pPrev; /* Reverse link */` |
|      34 | 6445 | `	}` |
|       - | 6446 | `	/* Return sum */` |
|      25 | 6447 | `	ph7_result_double(pCtx,dSum);` |
|      25 | 6448 | `}` |
|      32 | 6449 | `static void Int64Sum(ph7_context *pCtx,ph7_hashmap *pMap)` |
|       2 | 6450 | `{` |
|       - | 6451 | `	ph7_hashmap_node *pEntry;` |
|       - | 6452 | `	ph7_value *pObj;` |
|      34 | 6453 | `	sxi64 nSum = 0;` |
|       - | 6454 | `	sxu32 n;` |
|      34 | 6455 | `	pEntry = pMap->pFirst;` |
|     136 | 6456 | `	for( n = 0 ; n < pMap->nEntry ; n++ ){` |
|     104 | 6457 | `		pObj = HashmapExtractNodeValue(pEntry);` |
|     104 | 6458 | `		if( pObj ){` |
|     104 | 6459 | `			if( pObj->iFlags & (MEMOBJ_INT\|MEMOBJ_BOOL) ){` |
|      94 | 6460 | `				nSum += pObj->x.iVal;` |
|      57 | 6461 | `			}else if( pObj->iFlags & MEMOBJ_STRING ){` |
|       7 | 6462 | `				if( SyBlobLength(&pObj->sBlob) > 0 ){` |
|       5 | 6463 | `					sxi64 nv = 0;` |
|       5 | 6464 | `					SyStrToInt64((const char *)SyBlobData(&pObj->sBlob),SyBlobLength(&pObj->sBlob),(void *)&nv,0);` |
|       5 | 6465 | `					nSum += nv;` |
|       3 | 6466 | `				}` |
|       8 | 6467 | `			}else if( pObj->iFlags & MEMOBJ_HASHMAP ){` |
|       3 | 6468 | `				PH7_VmThrowError(pCtx->pVm,0,PH7_CTX_WARNING,` |
|       - | 6469 | `					"array_sum(): Addition is not supported on type array");` |
|       4 | 6470 | `			}else if( pObj->iFlags & MEMOBJ_OBJ ){` |
|     ! 0 | 6471 | `				PH7_VmThrowError(pCtx->pVm,0,PH7_CTX_WARNING,` |
|       - | 6472 | `					"array_sum(): Addition is not supported on type object");` |
|       3 | 6473 | `			}else if( pObj->iFlags & MEMOBJ_RES ){` |
|     ! 0 | 6474 | `				PH7_VmThrowError(pCtx->pVm,0,PH7_CTX_WARNING,` |
|       - | 6475 | `					"array_sum(): Addition is not supported on type resource");` |
|     ! 0 | 6476 | `			}` |
|       - | 6477 | `			/* NULL is silently treated as 0 (matches PHP) */` |
|      51 | 6478 | `		}` |
|       - | 6479 | `		/* Point to the next entry */` |
|     104 | 6480 | `		pEntry = pEntry->pPrev; /* Reverse link */` |
|      53 | 6481 | `	}` |
|       - | 6482 | `	/* Return sum */` |
|      34 | 6483 | `	ph7_result_int64(pCtx,nSum);` |
|      34 | 6484 | `}` |
|       - | 6485 | `/* number array_sum(array $array )` |
|       - | 6486 | ` * (See block-coment above)` |
|       - | 6487 | ` */` |
|      70 | 6488 | `static int ph7_hashmap_sum(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       5 | 6489 | `{` |
|       - | 6490 | `	ph7_hashmap_node *pEntry;` |
|       - | 6491 | `	ph7_hashmap *pMap;` |
|       - | 6492 | `	ph7_value *pObj;` |
|      75 | 6493 | `	int useDouble = 0;` |
|       - | 6494 | `	sxu32 n;` |
|       - | 6495 | `	/* PHP requires exactly one argument */` |
|      75 | 6496 | `	if( nArg != 1 ){` |
|       8 | 6497 | `		return PH7_VmThrowException(pCtx,` |
|       - | 6498 | `			"ArgumentCountError",` |
|       - | 6499 | `			"array_sum() expects exactly 1 argument, %d given",` |
|       2 | 6500 | `			nArg` |
|       - | 6501 | `			);` |
|       - | 6502 | `	}` |
|       - | 6503 | `	/* Make sure we are dealing with a valid hashmap */` |
|      70 | 6504 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|       - | 6505 | `		/* Type mismatch -> TypeError */` |
|       8 | 6506 | `		return PH7_VmThrowException(pCtx,` |
|       - | 6507 | `			"TypeError",` |
|       - | 6508 | `			"array_sum(): Argument #1 ($array) must be of type array, %s given",` |
|       2 | 6509 | `			ph7_type_name(apArg[0])` |
|       - | 6510 | `			);` |
|       - | 6511 | `	}` |
|      64 | 6512 | `	pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|      64 | 6513 | `	if( pMap->nEntry < 1 ){` |
|       - | 6514 | `		/* Nothing to compute,return 0 */` |
|       7 | 6515 | `		ph7_result_int(pCtx,0);` |
|       7 | 6516 | `		return PH7_OK;` |
|       - | 6517 | `	}` |
|       - | 6518 | `	/* Scan all elements: if any value is a float, use floating-point` |
|       - | 6519 | `	 * arithmetic for the entire sum (matches PHP behaviour).` |
|       - | 6520 | `	 */` |
|      58 | 6521 | `	pEntry = pMap->pFirst;` |
|     168 | 6522 | `	for( n = 0 ; n < pMap->nEntry ; n++ ){` |
|     136 | 6523 | `		pObj = HashmapExtractNodeValue(pEntry);` |
|     136 | 6524 | `		if( pObj ){` |
|     136 | 6525 | `			if( pObj->iFlags & MEMOBJ_REAL ){` |
|      19 | 6526 | `				useDouble = 1;` |
|      19 | 6527 | `				break;` |
|       - | 6528 | `			}` |
|     118 | 6529 | `			if( pObj->iFlags & MEMOBJ_STRING ){` |
|      13 | 6530 | `				const char *zStr = (const char *)SyBlobData(&pObj->sBlob);` |
|      13 | 6531 | `				sxu32 nLen = SyBlobLength(&pObj->sBlob);` |
|       - | 6532 | `				sxu32 i;` |
|      23 | 6533 | `				for( i = 0 ; i < nLen ; i++ ){` |
|      17 | 6534 | `					if( zStr[i] == '.' \|\| zStr[i] == 'e' \|\| zStr[i] == 'E' ){` |
|       7 | 6535 | `						useDouble = 1;` |
|       7 | 6536 | `						break;` |
|       - | 6537 | `					}` |
|       6 | 6538 | `				}` |
|      13 | 6539 | `				if( useDouble ){` |
|       7 | 6540 | `					break;` |
|       - | 6541 | `				}` |
|       3 | 6542 | `			}` |
|      55 | 6543 | `		}` |
|     112 | 6544 | `		pEntry = pEntry->pPrev;` |
|      57 | 6545 | `	}` |
|      58 | 6546 | `	if( useDouble ){` |
|      25 | 6547 | `		DoubleSum(pCtx,pMap);` |
|      13 | 6548 | `	}else{` |
|      34 | 6549 | `		Int64Sum(pCtx,pMap);` |
|       - | 6550 | `	}` |
|      58 | 6551 | `	return PH7_OK;` |
|      40 | 6552 | `}` |
|       - | 6553 | `/*` |
|       - | 6554 | ` * number array_product(array $array )` |
|       - | 6555 | ` *  Calculate the product of values in an array.` |
|       - | 6556 | ` * Parameters` |
|       - | 6557 | ` *  $array: The input array.` |
|       - | 6558 | ` * Return` |
|       - | 6559 | ` *  Returns the product of values as an integer or float.` |
|       - | 6560 | ` */` |
|     ! 0 | 6561 | `static void DoubleProd(ph7_context *pCtx,ph7_hashmap *pMap)` |
|     ! 0 | 6562 | `{` |
|       - | 6563 | `	ph7_hashmap_node *pEntry;` |
|       - | 6564 | `	ph7_value *pObj;` |
|       - | 6565 | `	double dProd;` |
|       - | 6566 | `	sxu32 n;` |
|     ! 0 | 6567 | `	pEntry = pMap->pFirst;` |
|     ! 0 | 6568 | `	dProd = 1;` |
|     ! 0 | 6569 | `	for( n = 0 ; n < pMap->nEntry ; n++ ){` |
|     ! 0 | 6570 | `		pObj = HashmapExtractNodeValue(pEntry);` |
|     ! 0 | 6571 | `		if( pObj && (pObj->iFlags & (MEMOBJ_NULL\|MEMOBJ_HASHMAP\|MEMOBJ_OBJ\|MEMOBJ_RES)) == 0){` |
|     ! 0 | 6572 | `			if( pObj->iFlags & MEMOBJ_REAL ){` |
|     ! 0 | 6573 | `				dProd *= pObj->rVal;` |
|     ! 0 | 6574 | `			}else if( pObj->iFlags & (MEMOBJ_INT\|MEMOBJ_BOOL) ){` |
|     ! 0 | 6575 | `				dProd *= (double)pObj->x.iVal;` |
|     ! 0 | 6576 | `			}else if( pObj->iFlags & MEMOBJ_STRING ){` |
|     ! 0 | 6577 | `				if( SyBlobLength(&pObj->sBlob) > 0 ){` |
|     ! 0 | 6578 | `					double dv = 0;` |
|     ! 0 | 6579 | `					SyStrToReal((const char *)SyBlobData(&pObj->sBlob),SyBlobLength(&pObj->sBlob),(void *)&dv,0);` |
|     ! 0 | 6580 | `					dProd *= dv;` |
|     ! 0 | 6581 | `				}` |
|     ! 0 | 6582 | `			}` |
|     ! 0 | 6583 | `		}` |
|       - | 6584 | `		/* Point to the next entry */` |
|     ! 0 | 6585 | `		pEntry = pEntry->pPrev; /* Reverse link */` |
|     ! 0 | 6586 | `	}` |
|       - | 6587 | `	/* Return product */` |
|     ! 0 | 6588 | `	ph7_result_double(pCtx,dProd);` |
|     ! 0 | 6589 | `}` |
|     ! 0 | 6590 | `static void Int64Prod(ph7_context *pCtx,ph7_hashmap *pMap)` |
|     ! 0 | 6591 | `{` |
|       - | 6592 | `	ph7_hashmap_node *pEntry;` |
|       - | 6593 | `	ph7_value *pObj;` |
|       - | 6594 | `	sxi64 nProd;` |
|       - | 6595 | `	sxu32 n;` |
|     ! 0 | 6596 | `	pEntry = pMap->pFirst;` |
|     ! 0 | 6597 | `	nProd = 1;` |
|     ! 0 | 6598 | `	for( n = 0 ; n < pMap->nEntry ; n++ ){` |
|     ! 0 | 6599 | `		pObj = HashmapExtractNodeValue(pEntry);` |
|     ! 0 | 6600 | `		if( pObj && (pObj->iFlags & (MEMOBJ_NULL\|MEMOBJ_HASHMAP\|MEMOBJ_OBJ\|MEMOBJ_RES)) == 0){` |
|     ! 0 | 6601 | `			if( pObj->iFlags & MEMOBJ_REAL ){` |
|     ! 0 | 6602 | `				nProd *= (sxi64)pObj->rVal;` |
|     ! 0 | 6603 | `			}else if( pObj->iFlags & (MEMOBJ_INT\|MEMOBJ_BOOL) ){` |
|     ! 0 | 6604 | `				nProd *= pObj->x.iVal;` |
|     ! 0 | 6605 | `			}else if( pObj->iFlags & MEMOBJ_STRING ){` |
|     ! 0 | 6606 | `				if( SyBlobLength(&pObj->sBlob) > 0 ){` |
|     ! 0 | 6607 | `					sxi64 nv = 0;` |
|     ! 0 | 6608 | `					SyStrToInt64((const char *)SyBlobData(&pObj->sBlob),SyBlobLength(&pObj->sBlob),(void *)&nv,0);` |
|     ! 0 | 6609 | `					nProd *= nv;` |
|     ! 0 | 6610 | `				}` |
|     ! 0 | 6611 | `			}` |
|     ! 0 | 6612 | `		}` |
|       - | 6613 | `		/* Point to the next entry */` |
|     ! 0 | 6614 | `		pEntry = pEntry->pPrev; /* Reverse link */` |
|     ! 0 | 6615 | `	}` |
|       - | 6616 | `	/* Return product */` |
|     ! 0 | 6617 | `	ph7_result_int64(pCtx,nProd);` |
|     ! 0 | 6618 | `}` |
|       - | 6619 | `/* number array_product(array $array )` |
|       - | 6620 | ` * (See block-block comment above)` |
|       - | 6621 | ` */` |
|     ! 0 | 6622 | `static int ph7_hashmap_product(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     ! 0 | 6623 | `{` |
|       - | 6624 | `	ph7_hashmap *pMap;` |
|       - | 6625 | `	ph7_value *pObj;` |
|     ! 0 | 6626 | `	if( nArg < 1 ){` |
|       - | 6627 | `		/* Missing arguments,return 0 */` |
|     ! 0 | 6628 | `		ph7_result_int(pCtx,0);` |
|     ! 0 | 6629 | `		return PH7_OK;` |
|       - | 6630 | `	}` |
|       - | 6631 | `	/* Make sure we are dealing with a valid hashmap */` |
|     ! 0 | 6632 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|       - | 6633 | `		/* Invalid argument,return 0 */` |
|     ! 0 | 6634 | `		ph7_result_int(pCtx,0);` |
|     ! 0 | 6635 | `		return PH7_OK;` |
|       - | 6636 | `	}` |
|     ! 0 | 6637 | `	pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|     ! 0 | 6638 | `	if( pMap->nEntry < 1 ){` |
|       - | 6639 | `		/* Nothing to compute,return 0 */` |
|     ! 0 | 6640 | `		ph7_result_int(pCtx,0);` |
|     ! 0 | 6641 | `		return PH7_OK;` |
|       - | 6642 | `	}` |
|       - | 6643 | `	/* If the first element is of type float,then perform floating` |
|       - | 6644 | `	 * point computaion.Otherwise switch to int64 computaion.` |
|       - | 6645 | `	 */` |
|     ! 0 | 6646 | `	pObj = HashmapExtractNodeValue(pMap->pFirst);` |
|     ! 0 | 6647 | `	if( pObj == 0 ){` |
|     ! 0 | 6648 | `		ph7_result_int(pCtx,0);` |
|     ! 0 | 6649 | `		return PH7_OK;` |
|       - | 6650 | `	}` |
|     ! 0 | 6651 | `	if( pObj->iFlags & MEMOBJ_REAL ){` |
|     ! 0 | 6652 | `		DoubleProd(pCtx,pMap);` |
|     ! 0 | 6653 | `	}else{` |
|     ! 0 | 6654 | `		Int64Prod(pCtx,pMap);` |
|       - | 6655 | `	}` |
|     ! 0 | 6656 | `	return PH7_OK;` |
|     ! 0 | 6657 | `}` |
|       - | 6658 | `/*` |
|       - | 6659 | ` * value array_rand(array $input[,int $num_req = 1 ])` |
|       - | 6660 | ` *  Pick one or more random entries out of an array.` |
|       - | 6661 | ` * Parameters` |
|       - | 6662 | ` * $input` |
|       - | 6663 | ` *  The input array.` |
|       - | 6664 | ` * $num_req` |
|       - | 6665 | ` *  Specifies how many entries you want to pick.` |
|       - | 6666 | ` * Return` |
|       - | 6667 | ` *  If you are picking only one entry, array_rand() returns the key for a random entry.` |
|       - | 6668 | ` *  Otherwise, it returns an array of keys for the random entries.` |
|       - | 6669 | ` *  NULL is returned on failure.` |
|       - | 6670 | ` */` |
|       6 | 6671 | `static int ph7_hashmap_rand(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       1 | 6672 | `{` |
|       - | 6673 | `	ph7_hashmap_node *pNode;` |
|       - | 6674 | `	ph7_hashmap *pMap;` |
|       7 | 6675 | `	int nItem = 1;` |
|       7 | 6676 | `	if( nArg < 1 ){` |
|       - | 6677 | `		/* Missing argument,return NULL */` |
|     ! 0 | 6678 | `		ph7_result_null(pCtx);` |
|     ! 0 | 6679 | `		return PH7_OK;` |
|       - | 6680 | `	}` |
|       - | 6681 | `	/* Make sure we are dealing with an array */` |
|       7 | 6682 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|     ! 0 | 6683 | `		ph7_result_null(pCtx);` |
|     ! 0 | 6684 | `		return PH7_OK;` |
|       - | 6685 | `	}` |
|       - | 6686 | `	/* Point to the internal representation of the input hashmap */` |
|       7 | 6687 | `	pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|       7 | 6688 | `	if(pMap->nEntry < 1 ){` |
|       - | 6689 | `		/* Empty hashmap,return NULL */` |
|     ! 0 | 6690 | `		ph7_result_null(pCtx);` |
|     ! 0 | 6691 | `		return PH7_OK;` |
|       - | 6692 | `	}` |
|       7 | 6693 | `	if( nArg > 1 ){` |
|       3 | 6694 | `		nItem = ph7_value_to_int(apArg[1]);` |
|       1 | 6695 | `	}` |
|       7 | 6696 | `	if( nItem < 2 ){` |
|       - | 6697 | `		sxu32 nEntry;` |
|       - | 6698 | `		/* Select a random number */` |
|       5 | 6699 | `		nEntry = PH7_VmRandomNum(pMap->pVm) % pMap->nEntry;` |
|       - | 6700 | `		/* Extract the desired entry.` |
|       - | 6701 | `		 * Note that we perform a linear lookup here (later version must change this)` |
|       - | 6702 | `		 */` |
|       5 | 6703 | `		if( nEntry > pMap->nEntry / 2 ){` |
|       3 | 6704 | `			pNode = pMap->pLast;` |
|       3 | 6705 | `			nEntry = pMap->nEntry - nEntry;` |
|       3 | 6706 | `			if( nEntry > 1 ){` |
|     ! 0 | 6707 | `				for(;;){` |
|     ! 0 | 6708 | `					if( nEntry == 0 ){` |
|     ! 0 | 6709 | `						break;` |
|       - | 6710 | `					}` |
|       - | 6711 | `					/* Point to the previous entry */` |
|     ! 0 | 6712 | `					pNode = pNode->pNext; /* Reverse link */` |
|     ! 0 | 6713 | `					nEntry--;` |
|     ! 0 | 6714 | `				}` |
|     ! 0 | 6715 | `			}` |
|       1 | 6716 | `		}else{` |
|       3 | 6717 | `			pNode = pMap->pFirst;` |
|       4 | 6718 | `			for(;;){` |
|       5 | 6719 | `				if( nEntry == 0 ){` |
|       3 | 6720 | `					break;` |
|       - | 6721 | `				}` |
|       - | 6722 | `				/* Point to the next entry */` |
|       3 | 6723 | `				pNode = pNode->pPrev; /* Reverse link */` |
|       3 | 6724 | `				nEntry--;` |
|       1 | 6725 | `			}` |
|       - | 6726 | `		}` |
|       5 | 6727 | `		if( pNode->iType == HASHMAP_INT_NODE ){` |
|       - | 6728 | `			/* Int key */` |
|       3 | 6729 | `			ph7_result_int64(pCtx,pNode->xKey.iKey);` |
|       2 | 6730 | `		}else{` |
|       - | 6731 | `			/* Blob key */` |
|       3 | 6732 | `			ph7_result_string(pCtx,(const char *)SyBlobData(&pNode->xKey.sKey),(int)SyBlobLength(&pNode->xKey.sKey));` |
|       - | 6733 | `		}` |
|       3 | 6734 | `	}else{` |
|       - | 6735 | `		ph7_value sKey,*pArray;` |
|       - | 6736 | `		ph7_hashmap *pDest;` |
|       - | 6737 | `		/* Create a new array */` |
|       3 | 6738 | `		pArray = ph7_context_new_array(pCtx);` |
|       3 | 6739 | `		if( pArray == 0 ){` |
|     ! 0 | 6740 | `			ph7_result_null(pCtx);` |
|     ! 0 | 6741 | `			return PH7_OK;` |
|       - | 6742 | `		}` |
|       - | 6743 | `		/* Point to the internal representation of the hashmap */` |
|       3 | 6744 | `		pDest = (ph7_hashmap *)pArray->x.pOther;` |
|       3 | 6745 | `		PH7_MemObjInit(pDest->pVm,&sKey);` |
|       - | 6746 | `		/* Copy the first n items */` |
|       3 | 6747 | `		pNode = pMap->pFirst;` |
|       3 | 6748 | `		if( nItem > (int)pMap->nEntry ){` |
|     ! 0 | 6749 | `			nItem = (int)pMap->nEntry;` |
|     ! 0 | 6750 | `		}` |
|       7 | 6751 | `		while( nItem > 0){` |
|       5 | 6752 | `			PH7_HashmapExtractNodeKey(pNode,&sKey);` |
|       5 | 6753 | `			PH7_HashmapInsert(pDest,0/* Automatic index assign*/,&sKey);` |
|       5 | 6754 | `			PH7_MemObjRelease(&sKey);` |
|       - | 6755 | `			/* Point to the next entry */` |
|       5 | 6756 | `			pNode = pNode->pPrev; /* Reverse link */` |
|       5 | 6757 | `			nItem--;` |
|       1 | 6758 | `		}` |
|       - | 6759 | `		/* Shuffle the array */` |
|       3 | 6760 | `		HashmapMergeSort(pDest,HashmapCmpCallback7,0);` |
|       - | 6761 | `		/* Rehash node */` |
|       3 | 6762 | `		HashmapSortRehash(pDest);` |
|       - | 6763 | `		/* Return the random array */` |
|       3 | 6764 | `		ph7_result_value(pCtx,pArray);` |
|       - | 6765 | `	}` |
|       7 | 6766 | `	return PH7_OK;` |
|       4 | 6767 | `}` |
|       - | 6768 | `/*` |
|       - | 6769 | ` * array array_chunk (array $input,int $size [,bool $preserve_keys = false ])` |
|       - | 6770 | ` *  Split an array into chunks.` |
|       - | 6771 | ` * Parameters` |
|       - | 6772 | ` * $input` |
|       - | 6773 | ` *   The array to work on` |
|       - | 6774 | ` * $size` |
|       - | 6775 | ` *   The size of each chunk` |
|       - | 6776 | ` * $preserve_keys` |
|       - | 6777 | ` *   When set to TRUE keys will be preserved. Default is FALSE which will reindex` |
|       - | 6778 | ` *   the chunk numerically.` |
|       - | 6779 | ` * Return` |
|       - | 6780 | ` *  Returns a multidimensional numerically indexed array, starting with` |
|       - | 6781 | ` *  zero, with each dimension containing size elements.` |
|       - | 6782 | ` */` |
|      42 | 6783 | `static int ph7_hashmap_chunk(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       5 | 6784 | `{` |
|       - | 6785 | `	ph7_value *pArray,*pChunk;` |
|       - | 6786 | `	ph7_hashmap_node *pEntry;` |
|       - | 6787 | `	ph7_hashmap *pMap;` |
|       - | 6788 | `	int bPreserve;` |
|       - | 6789 | `	sxu32 nChunk;` |
|       - | 6790 | `	sxu32 nSize;` |
|       - | 6791 | `	sxu32 n;` |
|       - | 6792 | `	/* Argument count and types follow PHP semantics. */` |
|      47 | 6793 | `	if( nArg < 2 ){` |
|       - | 6794 | `		/* fewer than required arguments -> ArgumentCountError */` |
|       4 | 6795 | `		return PH7_VmThrowException(pCtx,` |
|       - | 6796 | `			"ArgumentCountError",` |
|       - | 6797 | `			"array_chunk() expects at least 2 arguments, %d given",` |
|       1 | 6798 | `			nArg` |
|       - | 6799 | `			);` |
|       - | 6800 | `	}` |
|      45 | 6801 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|       4 | 6802 | `		return PH7_VmThrowException(pCtx,` |
|       - | 6803 | `			"TypeError",` |
|       - | 6804 | `			"array_chunk(): Argument #1 ($array) must be of type array, %s given",` |
|       1 | 6805 | `			ph7_type_name(apArg[0])` |
|       - | 6806 | `			);` |
|       - | 6807 | `	}` |
|       - | 6808 | `	/* Create a new array */` |
|      43 | 6809 | `	pArray = ph7_context_new_array(pCtx);` |
|      43 | 6810 | `	if( pArray == 0 ){` |
|     ! 0 | 6811 | `		ph7_result_null(pCtx);` |
|     ! 0 | 6812 | `		return PH7_OK;` |
|       - | 6813 | `	}` |
|       - | 6814 | `	/* Point to the internal representation of the input hashmap */` |
|      43 | 6815 | `	pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|       - | 6816 | `	/* Extract and validate the chunk size argument. */` |
|       - | 6817 | `	/* Reject types that cannot be sensibly converted to an integer. */` |
|      57 | 6818 | `	if( ph7_value_is_array(apArg[1]) \|\| ph7_value_is_object(apArg[1]) \|\|` |
|      81 | 6819 | `		ph7_value_is_resource(apArg[1]) \|\| ph7_value_is_null(apArg[1]) \|\|` |
|      38 | 6820 | `		ph7_value_is_bool(apArg[1]) ){` |
|     ! 0 | 6821 | `		return PH7_VmThrowException(pCtx,` |
|       - | 6822 | `			"TypeError",` |
|       - | 6823 | `			"array_chunk(): Argument #2 ($length) must be of type int, %s given",` |
|     ! 0 | 6824 | `			ph7_type_name(apArg[1])` |
|       - | 6825 | `			);` |
|       - | 6826 | `	}` |
|       - | 6827 | `	/* Strings that are non-numeric produce a TypeError.  Numeric` |
|       - | 6828 | `	 * strings are permitted; however those representing floats lose` |
|       - | 6829 | `	 * precision and PHP emits a deprecation warning. */` |
|      43 | 6830 | `	if( ph7_value_is_string(apArg[1]) ){` |
|       - | 6831 | `		int len;` |
|       3 | 6832 | `		sxu8 bReal = FALSE;` |
|       3 | 6833 | `		const char *zStr = ph7_value_to_string(apArg[1], &len);` |
|       3 | 6834 | `		if( SyStrIsNumeric(zStr, len, &bReal, 0) != SXRET_OK ){` |
|       3 | 6835 | `			return PH7_VmThrowException(pCtx,` |
|       - | 6836 | `				"TypeError",` |
|       - | 6837 | `				"array_chunk(): Argument #2 ($length) must be of type int, string given"` |
|       - | 6838 | `				);` |
|       - | 6839 | `		}` |
|     ! 0 | 6840 | `		if( bReal ){` |
|       - | 6841 | `			/* float-string -> warn but allow */` |
|     ! 0 | 6842 | `			ph7_context_throw_error_format(pCtx, E_DEPRECATED,` |
|       - | 6843 | `				"Implicit conversion from float-string \"%s\" to int loses precision",` |
|     ! 0 | 6844 | `				zStr` |
|       - | 6845 | `				);` |
|     ! 0 | 6846 | `		}` |
|     ! 0 | 6847 | `	}` |
|       - | 6848 | `	/* If the value is a float with a fractional component, emit a` |
|       - | 6849 | `	 * deprecation warning but continue.  The following conversion occurs` |
|       - | 6850 | `	 * later via ph7_value_to_int. */` |
|      40 | 6851 | `	if( ph7_value_is_float(apArg[1]) ){` |
|       3 | 6852 | `		double d = ph7_value_to_double(apArg[1]);` |
|       3 | 6853 | `		sxi64 i = (sxi64)d;` |
|       3 | 6854 | `		if( d != (double)i ){` |
|       4 | 6855 | `			ph7_context_throw_error_format(pCtx, E_DEPRECATED,` |
|       - | 6856 | `				"Implicit conversion from float %g to int loses precision",` |
|       1 | 6857 | `				d` |
|       - | 6858 | `				);` |
|       1 | 6859 | `		}` |
|       1 | 6860 | `	}` |
|       - | 6861 | `	/* Convert using ph7_value_to_int; now that float fractions are` |
|       - | 6862 | `	 * eliminated, this will not produce a warning. */` |
|       - | 6863 | `	{` |
|      40 | 6864 | `		sxi64 nSizeSigned = ph7_value_to_int(apArg[1]);` |
|      40 | 6865 | `		if( nSizeSigned < 1 ){` |
|       - | 6866 | `			/* size <= 0 -> ValueError */` |
|       6 | 6867 | `			return PH7_VmThrowException(pCtx,` |
|       - | 6868 | `				"ValueError",` |
|       - | 6869 | `				"array_chunk(): Argument #2 ($length) must be greater than 0"` |
|       - | 6870 | `				);` |
|       - | 6871 | `		}` |
|      35 | 6872 | `		nSize = (sxu32)nSizeSigned;` |
|       - | 6873 | `	}` |
|      35 | 6874 | `	if( nSize >= pMap->nEntry ){` |
|       - | 6875 | `		/* Return the whole array */` |
|       3 | 6876 | `		ph7_array_add_elem(pArray,0,apArg[0]);` |
|       3 | 6877 | `		ph7_result_value(pCtx,pArray);` |
|       3 | 6878 | `		return PH7_OK;` |
|       - | 6879 | `	}` |
|      33 | 6880 | `	bPreserve = 0;` |
|      33 | 6881 | `	if( nArg > 2 ){` |
|       - | 6882 | `		/* The third argument has a bool type hint in PHP.  Values that` |
|       - | 6883 | `		 * cannot be sensibly converted (arrays, objects, resources) are` |
|       - | 6884 | `		 * rejected with a TypeError.  Scalars and null coerce to bool` |
|       - | 6885 | `		 * normally, matching PHP behaviour. */` |
|      35 | 6886 | `		if( ph7_value_is_array(apArg[2]) \|\|` |
|      35 | 6887 | `			ph7_value_is_object(apArg[2]) \|\|` |
|      20 | 6888 | `			ph7_value_is_resource(apArg[2]) ){` |
|       8 | 6889 | `			return PH7_VmThrowException(pCtx,` |
|       - | 6890 | `				"TypeError",` |
|       - | 6891 | `				"array_chunk(): Argument #3 ($preserve_keys) must be of type bool, %s given",` |
|       4 | 6892 | `				ph7_type_name(apArg[2])` |
|       - | 6893 | `				);` |
|       - | 6894 | `		}` |
|      21 | 6895 | `		bPreserve = ph7_value_to_bool(apArg[2]);` |
|      10 | 6896 | `	}` |
|       - | 6897 | `	/* Start processing */` |
|      27 | 6898 | `	pEntry = pMap->pFirst;` |
|      27 | 6899 | `	nChunk = 0;` |
|      27 | 6900 | `	pChunk = 0;` |
|      27 | 6901 | `	n = pMap->nEntry;` |
|      56 | 6902 | `	for( ;; ){` |
|     113 | 6903 | `		if( n < 1 ){` |
|       - | 6904 | `			/* When the loop terminates we may still have a current chunk` |
|       - | 6905 | `			 * that hasn't been added to the result array.  The previous` |
|       - | 6906 | `			 * implementation only pushed it if nChunk>0 which dropped the` |
|       - | 6907 | `			 * final chunk when the input size was an exact multiple of` |
|       - | 6908 | `			 * the chunk length.  Always append the pending chunk if it` |
|       - | 6909 | `			 * exists. */` |
|      27 | 6910 | `			if( pChunk ){` |
|      27 | 6911 | `				ph7_array_add_elem(pArray,0,pChunk); /* Will have its own copy */` |
|      13 | 6912 | `			}` |
|      27 | 6913 | `			break;` |
|       - | 6914 | `		}` |
|      87 | 6915 | `		if( nChunk < 1 ){` |
|      71 | 6916 | `			if( pChunk ){` |
|       - | 6917 | `				/* Put the first chunk */` |
|      45 | 6918 | `				ph7_array_add_elem(pArray,0,pChunk); /* Will have it's own copy */` |
|      22 | 6919 | `			}` |
|       - | 6920 | `			/* Create a new dimension */` |
|      71 | 6921 | `			pChunk = ph7_context_new_array(pCtx); /* Don't worry about freeing memory here,everything` |
|       - | 6922 | `												   * will be automatically released as soon we return` |
|       - | 6923 | `												   * from this function */` |
|      71 | 6924 | `			if( pChunk == 0 ){` |
|     ! 0 | 6925 | `				break;` |
|       - | 6926 | `			}` |
|      71 | 6927 | `			nChunk = nSize;` |
|      35 | 6928 | `		}` |
|       - | 6929 | `		/* Insert the entry */` |
|      87 | 6930 | `		HashmapInsertNode((ph7_hashmap *)pChunk->x.pOther,pEntry,bPreserve);` |
|       - | 6931 | `		/* Point to the next entry */` |
|      87 | 6932 | `		pEntry = pEntry->pPrev; /* Reverse link */` |
|      87 | 6933 | `		nChunk--;` |
|      87 | 6934 | `		n--;` |
|       1 | 6935 | `	}` |
|       - | 6936 | `	/* Return the multidimensional array */` |
|      27 | 6937 | `	ph7_result_value(pCtx,pArray);` |
|      27 | 6938 | `	return PH7_OK;` |
|      26 | 6939 | `}` |
|       - | 6940 | `/*` |
|       - | 6941 | ` * array array_pad(array $input,int $pad_size,value $pad_value)` |
|       - | 6942 | ` *  Pad array to the specified length with a value.` |
|       - | 6943 | ` * $input` |
|       - | 6944 | ` *   Initial array of values to pad.` |
|       - | 6945 | ` * $pad_size` |
|       - | 6946 | ` *   New size of the array.` |
|       - | 6947 | ` * $pad_value` |
|       - | 6948 | ` *   Value to pad if input is less than pad_size.` |
|       - | 6949 | ` */` |
|      28 | 6950 | `static int ph7_hashmap_pad(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       5 | 6951 | `{` |
|       - | 6952 | `	ph7_hashmap *pMap;` |
|       - | 6953 | `	ph7_value *pArray;` |
|       - | 6954 | `	int nEntry;` |
|      33 | 6955 | `	if( nArg != 3 ){` |
|      12 | 6956 | `		return PH7_VmThrowException(pCtx,` |
|       - | 6957 | `			"ArgumentCountError",` |
|       - | 6958 | `			"array_pad() expects exactly 3 arguments, %d given",` |
|       3 | 6959 | `			nArg` |
|       - | 6960 | `			);` |
|       - | 6961 | `	}` |
|      24 | 6962 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|       4 | 6963 | `		return PH7_VmThrowException(pCtx,` |
|       - | 6964 | `			"TypeError",` |
|       - | 6965 | `			"array_pad(): Argument #1 ($array) must be of type array, %s given",` |
|       1 | 6966 | `			ph7_type_name(apArg[0])` |
|       - | 6967 | `			);` |
|       - | 6968 | `	}` |
|       - | 6969 | `	/* Create a new array */` |
|      21 | 6970 | `	pArray = ph7_context_new_array(pCtx);` |
|      21 | 6971 | `	if( pArray == 0 ){` |
|     ! 0 | 6972 | `		return PH7_ContextMemoryError(pCtx);` |
|       - | 6973 | `	}` |
|       - | 6974 | `	/* Point to the internal representation of the input hashmap */` |
|      21 | 6975 | `	pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|       - | 6976 | `	/* Extract the total number of desired entry to insert */` |
|      21 | 6977 | `	nEntry = ph7_value_to_int(apArg[1]);` |
|      21 | 6978 | `	if( nEntry < 0 ){` |
|       9 | 6979 | `		nEntry = -nEntry;` |
|       9 | 6980 | `		if( nEntry > (int)pMap->nEntry ){` |
|       5 | 6981 | `			nEntry -= (int)pMap->nEntry;` |
|       - | 6982 | `			/* Insert given items first */` |
|      17 | 6983 | `			while( nEntry > 0 ){` |
|      13 | 6984 | `				if( ph7_array_add_elem(pArray,0,apArg[2]) != SXRET_OK ){` |
|     ! 0 | 6985 | `					return PH7_ContextMemoryError(pCtx);` |
|       - | 6986 | `				}` |
|      13 | 6987 | `				nEntry--;` |
|       1 | 6988 | `			}` |
|       - | 6989 | `			/* Merge the two arrays */` |
|       5 | 6990 | `			HashmapMerge(pMap,(ph7_hashmap *)pArray->x.pOther);` |
|       3 | 6991 | `		}else{` |
|       5 | 6992 | `			PH7_HashmapDup(pMap,(ph7_hashmap *)pArray->x.pOther);` |
|       1 | 6993 | `		}` |
|      17 | 6994 | `	}else if( nEntry > 0 ){` |
|      11 | 6995 | `		if( nEntry > (int)pMap->nEntry ){` |
|       7 | 6996 | `			nEntry -= (int)pMap->nEntry;` |
|       - | 6997 | `			/* Merge the two arrays first */` |
|       7 | 6998 | `			HashmapMerge(pMap,(ph7_hashmap *)pArray->x.pOther);` |
|       - | 6999 | `			/* Insert given items */` |
|      25 | 7000 | `			while( nEntry > 0 ){` |
|      19 | 7001 | `				if( ph7_array_add_elem(pArray,0,apArg[2]) != SXRET_OK ){` |
|     ! 0 | 7002 | `					return PH7_ContextMemoryError(pCtx);` |
|       - | 7003 | `				}` |
|      19 | 7004 | `				nEntry--;` |
|       1 | 7005 | `			}` |
|       4 | 7006 | `		}else{` |
|       5 | 7007 | `			PH7_HashmapDup(pMap,(ph7_hashmap *)pArray->x.pOther);` |
|       - | 7008 | `		}` |
|       6 | 7009 | `	}else{` |
|       - | 7010 | `		/* nEntry == 0: return a copy of the input array */` |
|       3 | 7011 | `		PH7_HashmapDup(pMap,(ph7_hashmap *)pArray->x.pOther);` |
|       - | 7012 | `	}` |
|       - | 7013 | `	/* Return the new array */` |
|      21 | 7014 | `	ph7_result_value(pCtx,pArray);` |
|      21 | 7015 | `	return PH7_OK;` |
|      19 | 7016 | `}` |
|       - | 7017 | `/*` |
|       - | 7018 | ` * array array_replace(array &$array,array &$array1,...)` |
|       - | 7019 | ` *  Replaces elements from passed arrays into the first array.` |
|       - | 7020 | ` * Parameters` |
|       - | 7021 | ` * $array` |
|       - | 7022 | ` *   The array in which elements are replaced.` |
|       - | 7023 | ` * $array1` |
|       - | 7024 | ` *   The array from which elements will be extracted.` |
|       - | 7025 | ` * ....` |
|       - | 7026 | ` *  More arrays from which elements will be extracted.` |
|       - | 7027 | ` *  Values from later arrays overwrite the previous values.` |
|       - | 7028 | ` * Return` |
|       - | 7029 | ` *  Returns an array.` |
|       - | 7030 | ` *  Throws ArgumentCountError if no arguments are given.` |
|       - | 7031 | ` *  Throws TypeError if any argument is not an array.` |
|       - | 7032 | ` */` |
|      22 | 7033 | `static int ph7_hashmap_replace(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       4 | 7034 | `{` |
|       - | 7035 | `	ph7_hashmap *pMap;` |
|       - | 7036 | `	ph7_value *pArray;` |
|       - | 7037 | `	int i;` |
|      26 | 7038 | `	if( nArg < 1 ){` |
|       3 | 7039 | `		return PH7_VmThrowException(pCtx,` |
|       - | 7040 | `			"ArgumentCountError",` |
|       - | 7041 | `			"array_replace() expects at least 1 argument, 0 given"` |
|       - | 7042 | `			);` |
|       - | 7043 | `	}` |
|      23 | 7044 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|       4 | 7045 | `		return PH7_VmThrowException(pCtx,` |
|       - | 7046 | `			"TypeError",` |
|       - | 7047 | `			"array_replace(): Argument #1 ($array) must be of type array, %s given",` |
|       1 | 7048 | `			ph7_type_name(apArg[0])` |
|       - | 7049 | `			);` |
|       - | 7050 | `	}` |
|       - | 7051 | `	/* Create a new array */` |
|      20 | 7052 | `	pArray = ph7_context_new_array(pCtx);` |
|      20 | 7053 | `	if( pArray == 0 ){` |
|     ! 0 | 7054 | `		ph7_result_null(pCtx);` |
|     ! 0 | 7055 | `		return PH7_OK;` |
|       - | 7056 | `	}` |
|       - | 7057 | `	/* Overwrite from the first array */` |
|      20 | 7058 | `	pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|      20 | 7059 | `	HashmapOverwrite(pMap,(ph7_hashmap *)pArray->x.pOther);` |
|       - | 7060 | `	/* Perform the requested operation for remaining arrays */` |
|      36 | 7061 | `	for( i = 1 ; i < nArg ; i++ ){` |
|      20 | 7062 | `		if( !ph7_value_is_array(apArg[i]) ){` |
|       - | 7063 | `			/* Type mismatch -> TypeError */` |
|       4 | 7064 | `			return PH7_VmThrowException(pCtx,` |
|       - | 7065 | `				"TypeError",` |
|       - | 7066 | `				"array_replace(): Argument #%d must be of type array, %s given",` |
|       1 | 7067 | `				i + 1,` |
|       2 | 7068 | `				ph7_type_name(apArg[i])` |
|       - | 7069 | `				);` |
|       - | 7070 | `		}` |
|       - | 7071 | `		/* Point to the internal representation of the input hashmap */` |
|      17 | 7072 | `		pMap = (ph7_hashmap *)apArg[i]->x.pOther;` |
|      17 | 7073 | `		HashmapOverwrite(pMap,(ph7_hashmap *)pArray->x.pOther);` |
|       9 | 7074 | `	}` |
|       - | 7075 | `	/* Return the new array */` |
|      17 | 7076 | `	ph7_result_value(pCtx,pArray);` |
|      17 | 7077 | `	return PH7_OK;` |
|      15 | 7078 | `}` |
|       - | 7079 | `/*` |
|       - | 7080 | ` * array array_filter(array $input [,callback $callback ])` |
|       - | 7081 | ` *  Filters elements of an array using a callback function.` |
|       - | 7082 | ` * Parameters` |
|       - | 7083 | ` *  $input` |
|       - | 7084 | ` *    The array to iterate over` |
|       - | 7085 | ` * $callback` |
|       - | 7086 | ` *    The callback function to use` |
|       - | 7087 | ` *    If no callback is supplied, all entries of input equal to FALSE (see converting to boolean)` |
|       - | 7088 | ` *    will be removed.` |
|       - | 7089 | ` * Return` |
|       - | 7090 | ` *  The filtered array.` |
|       - | 7091 | ` */` |
|      20 | 7092 | `static int ph7_hashmap_filter(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       2 | 7093 | `{` |
|       - | 7094 | `	ph7_hashmap_node *pEntry;` |
|       - | 7095 | `	ph7_hashmap *pMap;` |
|       - | 7096 | `	ph7_value *pArray;` |
|       - | 7097 | `	ph7_value sResult;   /* Callback result */` |
|       - | 7098 | `	ph7_value *pValue;` |
|       - | 7099 | `	sxi32 rc;` |
|       - | 7100 | `	int keep;` |
|       - | 7101 | `	sxu32 n;` |
|      22 | 7102 | `	if( nArg < 1 \|\| !ph7_value_is_array(apArg[0]) ){` |
|       - | 7103 | `		/* Invalid arguments,return NULL */` |
|       3 | 7104 | `		ph7_result_null(pCtx);` |
|       3 | 7105 | `		return PH7_OK;` |
|       - | 7106 | `	}` |
|       - | 7107 | `	/* Create a new array */` |
|      20 | 7108 | `	pArray = ph7_context_new_array(pCtx);` |
|      20 | 7109 | `	if( pArray == 0 ){` |
|     ! 0 | 7110 | `		ph7_result_null(pCtx);` |
|     ! 0 | 7111 | `		return PH7_OK;` |
|       - | 7112 | `	}` |
|       - | 7113 | `	/* Point to the internal representation of the input hashmap */` |
|      20 | 7114 | `	pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|      20 | 7115 | `	pEntry = pMap->pFirst;` |
|      20 | 7116 | `	PH7_MemObjInit(pMap->pVm,&sResult);` |
|      20 | 7117 | `	sResult.nIdx = SXU32_HIGH; /* Mark as constant */` |
|       - | 7118 | `	/* Perform the requested operation */` |
|      78 | 7119 | `	for( n = 0 ; n < pMap->nEntry ; n++ ){` |
|       - | 7120 | `		/* Extract node value (may be NULL if allocation failed) */` |
|      64 | 7121 | `		pValue = HashmapExtractNodeValue(pEntry);` |
|      64 | 7122 | `		if( pValue == 0 ){` |
|       - | 7123 | `			/* Can happen if SySetAt() failed earlier; drop the entry. */` |
|     ! 0 | 7124 | `			keep = FALSE;` |
|      64 | 7125 | `		}else if( nArg > 1 && !ph7_value_is_null(apArg[1]) ){` |
|       - | 7126 | `			/* Callback was supplied (not NULL).  PHP 8 throws a` |
|       - | 7127 | `				* TypeError when the value is not callable or null; prior PH7` |
|       - | 7128 | `				* silently dropped the element.  Emit similar message. */` |
|      36 | 7129 | `			if( !ph7_value_is_callable(apArg[1]) ){` |
|       3 | 7130 | `				if( ph7_value_is_string(apArg[1]) ){` |
|       - | 7131 | `					int len;` |
|       3 | 7132 | `					const char *zName = ph7_value_to_string(apArg[1], &len);` |
|       4 | 7133 | `					return PH7_VmThrowException(pCtx,` |
|       - | 7134 | `						"TypeError",` |
|       - | 7135 | `						"array_filter(): Argument #2 ($callback) must be a valid callback or null, function \"%s\" not found or invalid function name",` |
|       1 | 7136 | `						zName` |
|       - | 7137 | `						);` |
|     ! 0 | 7138 | `				}else{` |
|     ! 0 | 7139 | `					return PH7_VmThrowException(pCtx,` |
|       - | 7140 | `						"TypeError",` |
|       - | 7141 | `						"array_filter(): Argument #2 ($callback) must be a valid callback or null, %s given",` |
|     ! 0 | 7142 | `						ph7_type_name(apArg[1])` |
|       - | 7143 | `						);` |
|       - | 7144 | `				}` |
|       - | 7145 | `			}` |
|      33 | 7146 | `			keep = FALSE;` |
|      33 | 7147 | `			rc = PH7_VmCallUserFunction(pMap->pVm,apArg[1],1,&pValue,&sResult);` |
|      33 | 7148 | `			if( rc == PH7_EXCEPTION ){` |
|       - | 7149 | `				/* The callback raised: propagate so the dispatcher unwinds. */` |
|       3 | 7150 | `				PH7_MemObjRelease(&sResult);` |
|       3 | 7151 | `				return PH7_EXCEPTION;` |
|       - | 7152 | `			}` |
|      31 | 7153 | `			if( rc == SXRET_OK ){` |
|       - | 7154 | `				/* Perform a boolean cast */` |
|      31 | 7155 | `				keep = ph7_value_to_bool(&sResult);` |
|      15 | 7156 | `			}` |
|      31 | 7157 | `			PH7_MemObjRelease(&sResult);` |
|      16 | 7158 | `		}else{` |
|       - | 7159 | `			/* No callback provided or callback explicitly NULL: use default` |
|       - | 7160 | `			 * behaviour where "empty" values are removed. This also covers` |
|       - | 7161 | `			 * the case where the callback argument is missing entirely.` |
|       - | 7162 | `			 */` |
|      29 | 7163 | `			keep = !PH7_MemObjIsEmpty(pValue);` |
|       - | 7164 | `		}` |
|      59 | 7165 | `		if( keep ){` |
|       - | 7166 | `			/* Perform the insertion,now the callback returned true */` |
|      21 | 7167 | `			HashmapInsertNode((ph7_hashmap *)pArray->x.pOther,pEntry,TRUE);` |
|      10 | 7168 | `		}` |
|       - | 7169 | `		/* Point to the next entry */` |
|      59 | 7170 | `		pEntry = pEntry->pPrev; /* Reverse link */` |
|      30 | 7171 | `	}` |
|      15 | 7172 | `	ph7_result_value(pCtx,pArray);` |
|      15 | 7173 | `	return PH7_OK;` |
|      12 | 7174 | `}` |
|       - | 7175 | `/*` |
|       - | 7176 | ` * array array_map(?callable $callback, array $array, array ...$arrays)` |
|       - | 7177 | ` *  Applies the callback to the elements of the given arrays.` |
|       - | 7178 | ` * Parameters` |
|       - | 7179 | ` *  $callback` |
|       - | 7180 | ` *   A callable to run for each element in each array, or NULL. With a single` |
|       - | 7181 | ` *   array and a NULL callback this is the identity function (the array is` |
|       - | 7182 | ` *   returned unchanged); with several arrays and a NULL callback the arrays` |
|       - | 7183 | ` *   are zipped together.` |
|       - | 7184 | ` *  $array` |
|       - | 7185 | ` *   The first array to run through the callback function.` |
|       - | 7186 | ` *  $arrays` |
|       - | 7187 | ` *   Zero or more additional arrays to process in parallel.` |
|       - | 7188 | ` * Return` |
|       - | 7189 | ` *  Returns an array containing the results of applying the callback function.` |
|       - | 7190 | ` *  With a single array the keys are preserved; with several arrays the result` |
|       - | 7191 | ` *  is re-indexed and the iteration runs to the length of the longest array,` |
|       - | 7192 | ` *  padding shorter arrays with NULL.` |
|       - | 7193 | ` */` |
|      54 | 7194 | `static int ph7_hashmap_map(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       5 | 7195 | `{` |
|       - | 7196 | `	ph7_value *pArray,*pValue,sKey,sResult;` |
|       - | 7197 | `	ph7_hashmap_node *pEntry;` |
|       - | 7198 | `	ph7_hashmap *pMap;` |
|       - | 7199 | `	ph7_vm *pVm;` |
|       - | 7200 | `	int bNullCallback;` |
|       - | 7201 | `	sxi32 rc;` |
|       - | 7202 | `	int i;` |
|       - | 7203 | `	sxu32 n;` |
|      59 | 7204 | `	if( nArg < 2 ){` |
|       8 | 7205 | `		return PH7_VmThrowException(pCtx,` |
|       - | 7206 | `			"ArgumentCountError",` |
|       - | 7207 | `			"array_map() expects at least 2 arguments, %d given",` |
|       2 | 7208 | `			nArg` |
|       - | 7209 | `			);` |
|       - | 7210 | `	}` |
|      53 | 7211 | `	bNullCallback = ph7_value_is_null(apArg[0]);` |
|      53 | 7212 | `	if( !bNullCallback && !ph7_value_is_callable(apArg[0]) ){` |
|       5 | 7213 | `		if( ph7_value_is_string(apArg[0]) ){` |
|       3 | 7214 | `			const char *zFunc = ph7_value_to_string(apArg[0],0);` |
|       4 | 7215 | `			return PH7_VmThrowException(pCtx,` |
|       - | 7216 | `				"TypeError",` |
|       - | 7217 | `				"array_map(): Argument #1 ($callback) must be a valid callback or null, "` |
|       - | 7218 | `				"function \"%s\" not found or invalid function name",` |
|       1 | 7219 | `				zFunc` |
|       - | 7220 | `				);` |
|       - | 7221 | `		}` |
|       3 | 7222 | `		return PH7_VmThrowException(pCtx,` |
|       - | 7223 | `			"TypeError",` |
|       - | 7224 | `			"array_map(): Argument #1 ($callback) must be a valid callback or null, "` |
|       - | 7225 | `			"no array or string given"` |
|       - | 7226 | `			);` |
|       - | 7227 | `	}` |
|       - | 7228 | `	/* Every remaining argument must be an array */` |
|     104 | 7229 | `	for( i = 1 ; i < nArg ; i++ ){` |
|      60 | 7230 | `		if( !ph7_value_is_array(apArg[i]) ){` |
|       3 | 7231 | `			if( i == 1 ){` |
|       4 | 7232 | `				return PH7_VmThrowException(pCtx,` |
|       - | 7233 | `					"TypeError",` |
|       - | 7234 | `					"array_map(): Argument #2 ($array) must be of type array, %s given",` |
|       2 | 7235 | `					ph7_type_name(apArg[1])` |
|       - | 7236 | `					);` |
|       - | 7237 | `			}` |
|     ! 0 | 7238 | `			return PH7_VmThrowException(pCtx,` |
|       - | 7239 | `				"TypeError",` |
|       - | 7240 | `				"array_map(): Argument #%d must be of type array, %s given",` |
|     ! 0 | 7241 | `				i+1,ph7_type_name(apArg[i])` |
|       - | 7242 | `				);` |
|       - | 7243 | `		}` |
|      30 | 7244 | `	}` |
|      46 | 7245 | `	pVm = pCtx->pVm;` |
|       - | 7246 | `	/* Create a new array */` |
|      46 | 7247 | `	pArray = ph7_context_new_array(pCtx);` |
|      46 | 7248 | `	if( pArray == 0 ){` |
|     ! 0 | 7249 | `		ph7_result_null(pCtx);` |
|     ! 0 | 7250 | `		return PH7_OK;` |
|       - | 7251 | `	}` |
|      46 | 7252 | `	PH7_MemObjInit(pVm,&sResult);` |
|      46 | 7253 | `	PH7_MemObjInit(pVm,&sKey);` |
|      46 | 7254 | `	sResult.nIdx = SXU32_HIGH; /* Mark as constant */` |
|      46 | 7255 | `	sKey.nIdx    = SXU32_HIGH; /* Mark as constant */` |
|      46 | 7256 | `	if( nArg == 2 ){` |
|       - | 7257 | `		/* Single-array mode: keys are preserved (PHP semantics). */` |
|      36 | 7258 | `		pMap = (ph7_hashmap *)apArg[1]->x.pOther;` |
|      36 | 7259 | `		pEntry = pMap->pFirst;` |
|     110 | 7260 | `		for( n = 0 ; n < pMap->nEntry ; n++ ){` |
|       - | 7261 | `			/* Extract the node value */` |
|      78 | 7262 | `			pValue = HashmapExtractNodeValue(pEntry);` |
|      78 | 7263 | `			if( pValue ){` |
|       - | 7264 | `				/* Extract the node key */` |
|      78 | 7265 | `				PH7_HashmapExtractNodeKey(pEntry,&sKey);` |
|      78 | 7266 | `				if( bNullCallback ){` |
|       - | 7267 | `					/* NULL callback: identity function, keep original value */` |
|      11 | 7268 | `					ph7_array_add_elem(pArray,&sKey,pValue);` |
|       6 | 7269 | `				}else{` |
|       - | 7270 | `					/* Invoke the supplied callback */` |
|      68 | 7271 | `					rc = PH7_VmCallUserFunction(pVm,apArg[0],1,&pValue,&sResult);` |
|      68 | 7272 | `					if( rc == PH7_EXCEPTION ){` |
|       - | 7273 | `						/* Callback raised: abort and let the foreign-function` |
|       - | 7274 | `						 * dispatcher unwind through the nearest try/catch. */` |
|       3 | 7275 | `						PH7_MemObjRelease(&sKey);` |
|       3 | 7276 | `						PH7_MemObjRelease(&sResult);` |
|       3 | 7277 | `						return PH7_EXCEPTION;` |
|       - | 7278 | `					}` |
|       - | 7279 | `					/* Insert the callback return value */` |
|      66 | 7280 | `					ph7_array_add_elem(pArray,&sKey,&sResult);` |
|       - | 7281 | `				}` |
|      76 | 7282 | `				PH7_MemObjRelease(&sKey);` |
|      76 | 7283 | `				PH7_MemObjRelease(&sResult);` |
|      37 | 7284 | `			}` |
|       - | 7285 | `			/* Point to the next entry */` |
|      76 | 7286 | `			pEntry = pEntry->pPrev; /* Reverse link */` |
|      39 | 7287 | `		}` |
|      18 | 7288 | `	}else{` |
|       - | 7289 | `		/* Multi-array mode: walk every array in parallel to the length of the` |
|       - | 7290 | `		 * longest one, pad shorter arrays with NULL, and re-index the result. */` |
|      11 | 7291 | `		int nArrays = nArg - 1;` |
|       - | 7292 | `		ph7_hashmap_node **apCur;` |
|       - | 7293 | `		ph7_value **apCallArg;` |
|       - | 7294 | `		ph7_value sNull;` |
|      11 | 7295 | `		sxu32 nMax = 0;` |
|      11 | 7296 | `		apCur     = (ph7_hashmap_node **)SyMemBackendAlloc(&pVm->sAllocator,(sxu32)(nArrays*sizeof(ph7_hashmap_node *)));` |
|      11 | 7297 | `		apCallArg = (ph7_value **)SyMemBackendAlloc(&pVm->sAllocator,(sxu32)(nArrays*sizeof(ph7_value *)));` |
|      11 | 7298 | `		if( apCur == 0 \|\| apCallArg == 0 ){` |
|     ! 0 | 7299 | `			if( apCur ){ SyMemBackendFree(&pVm->sAllocator,apCur); }` |
|     ! 0 | 7300 | `			if( apCallArg ){ SyMemBackendFree(&pVm->sAllocator,apCallArg); }` |
|     ! 0 | 7301 | `			PH7_MemObjRelease(&sKey);` |
|     ! 0 | 7302 | `			PH7_MemObjRelease(&sResult);` |
|     ! 0 | 7303 | `			ph7_result_value(pCtx,pArray);` |
|     ! 0 | 7304 | `			return PH7_OK;` |
|       - | 7305 | `		}` |
|      11 | 7306 | `		PH7_MemObjInit(pVm,&sNull); /* shared NULL pad for short arrays */` |
|      11 | 7307 | `		sNull.nIdx = SXU32_HIGH;` |
|      33 | 7308 | `		for( i = 0 ; i < nArrays ; i++ ){` |
|      23 | 7309 | `			pMap = (ph7_hashmap *)apArg[i+1]->x.pOther;` |
|      23 | 7310 | `			apCur[i] = pMap->pFirst;` |
|      23 | 7311 | `			if( pMap->nEntry > nMax ){` |
|      13 | 7312 | `				nMax = pMap->nEntry;` |
|       6 | 7313 | `			}` |
|      12 | 7314 | `		}` |
|      35 | 7315 | `		for( n = 0 ; n < nMax ; n++ ){` |
|      25 | 7316 | `			ph7_value *pZip = 0;` |
|      25 | 7317 | `			if( bNullCallback ){` |
|       - | 7318 | `				/* zip: each result element is an array of the i-th values */` |
|       5 | 7319 | `				pZip = ph7_context_new_array(pCtx);` |
|       2 | 7320 | `			}` |
|      79 | 7321 | `			for( i = 0 ; i < nArrays ; i++ ){` |
|      55 | 7322 | `				ph7_value *pv = &sNull;` |
|      55 | 7323 | `				if( apCur[i] ){` |
|      53 | 7324 | `					ph7_value *pNodeVal = HashmapExtractNodeValue(apCur[i]);` |
|      53 | 7325 | `					if( pNodeVal ){` |
|      53 | 7326 | `						pv = pNodeVal;` |
|      26 | 7327 | `					}` |
|      53 | 7328 | `					apCur[i] = apCur[i]->pPrev; /* Reverse link */` |
|      26 | 7329 | `				}` |
|      55 | 7330 | `				if( bNullCallback ){` |
|       9 | 7331 | `					if( pZip ){` |
|       9 | 7332 | `						ph7_array_add_elem(pZip,0,pv);` |
|       4 | 7333 | `					}` |
|       5 | 7334 | `				}else{` |
|      47 | 7335 | `					apCallArg[i] = pv;` |
|       - | 7336 | `				}` |
|      28 | 7337 | `			}` |
|      25 | 7338 | `			if( bNullCallback ){` |
|       5 | 7339 | `				if( pZip ){` |
|       5 | 7340 | `					ph7_array_add_elem(pArray,0,pZip);` |
|       2 | 7341 | `				}` |
|       3 | 7342 | `			}else{` |
|      21 | 7343 | `				rc = PH7_VmCallUserFunction(pVm,apArg[0],nArrays,apCallArg,&sResult);` |
|      21 | 7344 | `				if( rc == PH7_EXCEPTION ){` |
|     ! 0 | 7345 | `					SyMemBackendFree(&pVm->sAllocator,apCur);` |
|     ! 0 | 7346 | `					SyMemBackendFree(&pVm->sAllocator,apCallArg);` |
|     ! 0 | 7347 | `					PH7_MemObjRelease(&sNull);` |
|     ! 0 | 7348 | `					PH7_MemObjRelease(&sKey);` |
|     ! 0 | 7349 | `					PH7_MemObjRelease(&sResult);` |
|     ! 0 | 7350 | `					return PH7_EXCEPTION;` |
|       - | 7351 | `				}` |
|      21 | 7352 | `				ph7_array_add_elem(pArray,0,&sResult);` |
|      21 | 7353 | `				PH7_MemObjRelease(&sResult);` |
|       - | 7354 | `			}` |
|      13 | 7355 | `		}` |
|      11 | 7356 | `		SyMemBackendFree(&pVm->sAllocator,apCur);` |
|      11 | 7357 | `		SyMemBackendFree(&pVm->sAllocator,apCallArg);` |
|      11 | 7358 | `		PH7_MemObjRelease(&sNull);` |
|       - | 7359 | `	}` |
|      44 | 7360 | `	PH7_MemObjRelease(&sKey);` |
|      44 | 7361 | `	PH7_MemObjRelease(&sResult);` |
|      44 | 7362 | `	ph7_result_value(pCtx,pArray);` |
|      44 | 7363 | `	return PH7_OK;` |
|      32 | 7364 | `}` |
|       - | 7365 | `/*` |
|       - | 7366 | ` * value array_reduce(array $array, callable $callback[, value $initial = NULL])` |
|       - | 7367 | ` *  Iteratively reduce the array to a single value using a callback function.` |
|       - | 7368 | ` * Parameters` |
|       - | 7369 | ` *  $array` |
|       - | 7370 | ` *   The input array.` |
|       - | 7371 | ` *  $callback` |
|       - | 7372 | ` *   The callback function. Signature: callback(mixed $carry, mixed $item): mixed` |
|       - | 7373 | ` *  $initial` |
|       - | 7374 | ` *   If the optional initial is available, it will be used at the beginning` |
|       - | 7375 | ` *   of the process, or as a final result in case the array is empty.` |
|       - | 7376 | ` * Return` |
|       - | 7377 | ` *  Returns the resulting value.` |
|       - | 7378 | ` *  If the array is empty and initial is not passed, array_reduce() returns NULL.` |
|       - | 7379 | ` */` |
|      34 | 7380 | `static int ph7_hashmap_reduce(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       5 | 7381 | `{` |
|       - | 7382 | `	ph7_hashmap_node *pEntry;` |
|       - | 7383 | `	ph7_hashmap *pMap;` |
|       - | 7384 | `	ph7_value *pValue;` |
|       - | 7385 | `	ph7_value sResult;` |
|       - | 7386 | `	sxi32 rc;` |
|       - | 7387 | `	sxu32 n;` |
|      39 | 7388 | `	if( nArg < 2 ){` |
|       8 | 7389 | `		return PH7_VmThrowException(pCtx,` |
|       - | 7390 | `			"ArgumentCountError",` |
|       - | 7391 | `			"array_reduce() expects at least 2 arguments, %d given",` |
|       2 | 7392 | `			nArg` |
|       - | 7393 | `			);` |
|       - | 7394 | `	}` |
|      35 | 7395 | `	if( nArg > 3 ){` |
|       4 | 7396 | `		return PH7_VmThrowException(pCtx,` |
|       - | 7397 | `			"ArgumentCountError",` |
|       - | 7398 | `			"array_reduce() expects at most 3 arguments, %d given",` |
|       1 | 7399 | `			nArg` |
|       - | 7400 | `			);` |
|       - | 7401 | `	}` |
|      33 | 7402 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|       4 | 7403 | `		return PH7_VmThrowException(pCtx,` |
|       - | 7404 | `			"TypeError",` |
|       - | 7405 | `			"array_reduce(): Argument #1 ($array) must be of type array, %s given",` |
|       1 | 7406 | `			ph7_type_name(apArg[0])` |
|       - | 7407 | `			);` |
|       - | 7408 | `	}` |
|      31 | 7409 | `	if( !ph7_value_is_callable(apArg[1]) ){` |
|      12 | 7410 | `		if( ph7_value_is_string(apArg[1]) ){` |
|       3 | 7411 | `			const char *zFunc = ph7_value_to_string(apArg[1],0);` |
|       4 | 7412 | `			return PH7_VmThrowException(pCtx,` |
|       - | 7413 | `				"TypeError",` |
|       - | 7414 | `				"array_reduce(): Argument #2 ($callback) must be a valid callback, "` |
|       - | 7415 | `				"function \"%s\" not found or invalid function name",` |
|       1 | 7416 | `				zFunc` |
|       - | 7417 | `				);` |
|       - | 7418 | `		}` |
|       9 | 7419 | `		if( ph7_value_is_array(apArg[1]) ){` |
|       3 | 7420 | `			return PH7_VmThrowException(pCtx,` |
|       - | 7421 | `				"TypeError",` |
|       - | 7422 | `				"array_reduce(): Argument #2 ($callback) must be a valid callback, "` |
|       - | 7423 | `				"array callback must have exactly two members"` |
|       - | 7424 | `				);` |
|       - | 7425 | `		}` |
|       6 | 7426 | `		return PH7_VmThrowException(pCtx,` |
|       - | 7427 | `			"TypeError",` |
|       - | 7428 | `			"array_reduce(): Argument #2 ($callback) must be a valid callback, "` |
|       - | 7429 | `			"no array or string given"` |
|       - | 7430 | `			);` |
|       - | 7431 | `	}` |
|       - | 7432 | `	/* Point to the internal representation of the input hashmap */` |
|      19 | 7433 | `	pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|       - | 7434 | `	/* Assume a NULL initial value */` |
|      19 | 7435 | `	PH7_MemObjInit(pMap->pVm,&sResult);` |
|      19 | 7436 | `	sResult.nIdx = SXU32_HIGH; /* Mark as constant */` |
|      19 | 7437 | `	if( nArg > 2 ){` |
|       - | 7438 | `		/* Set the initial value */` |
|      13 | 7439 | `		PH7_MemObjLoad(apArg[2],&sResult);` |
|       6 | 7440 | `	}` |
|       - | 7441 | `	/* Perform the requested operation */` |
|      19 | 7442 | `	pEntry = pMap->pFirst;` |
|      55 | 7443 | `	for( n = 0 ; n < pMap->nEntry ; n++ ){` |
|       - | 7444 | `		/* Extract the node value */` |
|      39 | 7445 | `		pValue = HashmapExtractNodeValue(pEntry);` |
|       - | 7446 | `		/* Invoke the supplied callback */` |
|      39 | 7447 | `		rc = PH7_VmCallUserFunctionAp(pMap->pVm,apArg[1],&sResult,&sResult,pValue,0);` |
|      39 | 7448 | `		if( rc == PH7_EXCEPTION ){` |
|       - | 7449 | `			/* The callback raised: propagate so the dispatcher unwinds. */` |
|       3 | 7450 | `			PH7_MemObjRelease(&sResult);` |
|       3 | 7451 | `			return PH7_EXCEPTION;` |
|       - | 7452 | `		}` |
|       - | 7453 | `		/* Point to the next entry */` |
|      37 | 7454 | `		pEntry = pEntry->pPrev; /* Reverse link */` |
|      19 | 7455 | `	}` |
|      17 | 7456 | `	ph7_result_value(pCtx,&sResult); /* Will make it's own copy */` |
|      17 | 7457 | `	PH7_MemObjRelease(&sResult);` |
|      17 | 7458 | `	return PH7_OK;` |
|      22 | 7459 | `}` |
|       - | 7460 | `/*` |
|       - | 7461 | ` * bool array_walk(array &$array, callback $funcname [, mixed $userdata])` |
|       - | 7462 | ` *  Apply a user function to every member of an array.` |
|       - | 7463 | ` * Parameters` |
|       - | 7464 | ` *  $array` |
|       - | 7465 | ` *   The input array.` |
|       - | 7466 | ` *  $funcname` |
|       - | 7467 | ` *   Typically, funcname takes on two parameters. The array parameter's value being` |
|       - | 7468 | ` *   the first, and the key/index second.` |
|       - | 7469 | ` * Note:` |
|       - | 7470 | ` *  If funcname needs to be working with the actual values of the array, specify the first` |
|       - | 7471 | ` *  parameter of funcname as a reference. Then, any changes made to those elements will` |
|       - | 7472 | ` *  be made in the original array itself.` |
|       - | 7473 | ` *  $userdata` |
|       - | 7474 | ` *   If the optional userdata parameter is supplied, it will be passed as the third parameter` |
|       - | 7475 | ` *   to the callback funcname.` |
|       - | 7476 | ` * Return` |
|       - | 7477 | ` *  Returns TRUE on success or FALSE on failure.` |
|       - | 7478 | ` */` |
|      38 | 7479 | `static int ph7_hashmap_walk(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       5 | 7480 | `{` |
|       - | 7481 | `	ph7_value *pValue,*pUserData,sKey;` |
|       - | 7482 | `	ph7_hashmap_node *pEntry;` |
|       - | 7483 | `	ph7_hashmap *pMap;` |
|       - | 7484 | `	sxu32 n;` |
|      43 | 7485 | `	if( nArg < 2 ){` |
|       8 | 7486 | `		return PH7_VmThrowException(pCtx,` |
|       - | 7487 | `			"ArgumentCountError",` |
|       - | 7488 | `			"array_walk() expects at least 2 arguments, %d given",` |
|       2 | 7489 | `			nArg` |
|       - | 7490 | `			);` |
|       - | 7491 | `	}` |
|      39 | 7492 | `	if( nArg > 3 ){` |
|       4 | 7493 | `		return PH7_VmThrowException(pCtx,` |
|       - | 7494 | `			"ArgumentCountError",` |
|       - | 7495 | `			"array_walk() expects at most 3 arguments, %d given",` |
|       1 | 7496 | `			nArg` |
|       - | 7497 | `			);` |
|       - | 7498 | `	}` |
|      37 | 7499 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|       4 | 7500 | `		return PH7_VmThrowException(pCtx,` |
|       - | 7501 | `			"TypeError",` |
|       - | 7502 | `			"array_walk(): Argument #1 ($array) must be of type array, %s given",` |
|       1 | 7503 | `			ph7_type_name(apArg[0])` |
|       - | 7504 | `			);` |
|       - | 7505 | `	}` |
|      35 | 7506 | `	if( !ph7_value_is_callable(apArg[1]) ){` |
|      14 | 7507 | `		if( ph7_value_is_string(apArg[1]) ){` |
|       3 | 7508 | `			const char *zFunc = ph7_value_to_string(apArg[1],0);` |
|       4 | 7509 | `			return PH7_VmThrowException(pCtx,` |
|       - | 7510 | `				"TypeError",` |
|       - | 7511 | `				"array_walk(): Argument #2 ($callback) must be a valid callback, "` |
|       - | 7512 | `				"function \"%s\" not found or invalid function name",` |
|       1 | 7513 | `				zFunc` |
|       - | 7514 | `				);` |
|       - | 7515 | `		}` |
|      12 | 7516 | `		if( ph7_value_is_array(apArg[1]) ){` |
|       6 | 7517 | `			return PH7_VmThrowException(pCtx,` |
|       - | 7518 | `				"TypeError",` |
|       - | 7519 | `				"array_walk(): Argument #2 ($callback) must be a valid callback, "` |
|       - | 7520 | `				"array callback must have exactly two members"` |
|       - | 7521 | `				);` |
|       - | 7522 | `		}` |
|       6 | 7523 | `		return PH7_VmThrowException(pCtx,` |
|       - | 7524 | `			"TypeError",` |
|       - | 7525 | `			"array_walk(): Argument #2 ($callback) must be a valid callback, "` |
|       - | 7526 | `			"no array or string given"` |
|       - | 7527 | `			);` |
|       - | 7528 | `	}` |
|      21 | 7529 | `	pUserData = nArg > 2 ? apArg[2] : 0;` |
|       - | 7530 | `	/* Point to the internal representation of the input hashmap */` |
|      21 | 7531 | `	PH7_HashmapCowSeparate(pCtx->pVm, apArg[0]);` |
|      21 | 7532 | `	pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|      21 | 7533 | `	PH7_MemObjInit(pMap->pVm,&sKey);` |
|      21 | 7534 | `	sKey.nIdx = SXU32_HIGH; /* Mark as constant */` |
|       - | 7535 | `	/* Perform the desired operation */` |
|      21 | 7536 | `	pEntry = pMap->pFirst;` |
|      61 | 7537 | `	for( n = 0 ; n < pMap->nEntry ; n++ ){` |
|       - | 7538 | `		/* Extract the node value */` |
|      43 | 7539 | `		pValue = HashmapExtractNodeValue(pEntry);` |
|      43 | 7540 | `		if( pValue ){` |
|       - | 7541 | `			sxi32 rcW;` |
|       - | 7542 | `			/* Extract the entry key */` |
|      43 | 7543 | `			PH7_HashmapExtractNodeKey(pEntry,&sKey);` |
|       - | 7544 | `			/* Invoke the supplied callback */` |
|      43 | 7545 | `			rcW = PH7_VmCallUserFunctionAp(pMap->pVm,apArg[1],0,pValue,&sKey,pUserData,0);` |
|      43 | 7546 | `			PH7_MemObjRelease(&sKey);` |
|      43 | 7547 | `			if( rcW == PH7_EXCEPTION ){` |
|       - | 7548 | `				/* The callback raised: propagate so the dispatcher unwinds. */` |
|       3 | 7549 | `				return PH7_EXCEPTION;` |
|       - | 7550 | `			}` |
|      20 | 7551 | `		}` |
|       - | 7552 | `		/* Point to the next entry */` |
|      41 | 7553 | `		pEntry = pEntry->pPrev; /* Reverse link */` |
|      21 | 7554 | `	}` |
|       - | 7555 | `	/* All done, return TRUE */` |
|      19 | 7556 | `	ph7_result_bool(pCtx,1);` |
|      19 | 7557 | `	return PH7_OK;` |
|      24 | 7558 | `}` |
|       - | 7559 | `/*` |
|       - | 7560 | ` * Apply a user function to every member of an array.(Recurse on array's).` |
|       - | 7561 | ` * Refer to the [array_walk_recursive()] implementation for more information.` |
|       - | 7562 | ` */` |
|      22 | 7563 | `static sxi32 HashmapWalkRecursive(` |
|       - | 7564 | `	ph7_hashmap *pMap,    /* Target hashmap */` |
|       - | 7565 | `	ph7_value *pCallback, /* User callback */` |
|       - | 7566 | `	ph7_value *pUserData, /* Callback private data */` |
|       - | 7567 | `	int iNest             /* Nesting level */` |
|       - | 7568 | `	)` |
|       1 | 7569 | `{` |
|       - | 7570 | `	ph7_hashmap_node *pEntry;` |
|       - | 7571 | `	ph7_value *pValue,sKey;` |
|       - | 7572 | `	sxi32 rc;` |
|       - | 7573 | `	sxu32 n;` |
|       - | 7574 | `	/* Iterate through hashmap entries */` |
|      23 | 7575 | `	PH7_MemObjInit(pMap->pVm,&sKey);` |
|      23 | 7576 | `	sKey.nIdx = SXU32_HIGH; /* Mark as constant */` |
|      23 | 7577 | `	pEntry = pMap->pFirst;` |
|      59 | 7578 | `	for( n = 0 ; n < pMap->nEntry ; n++ ){` |
|       - | 7579 | `		/* Extract the node value */` |
|      37 | 7580 | `		pValue = HashmapExtractNodeValue(pEntry);` |
|      37 | 7581 | `		if( pValue ){` |
|      37 | 7582 | `			if( pValue->iFlags & MEMOBJ_HASHMAP ){` |
|      11 | 7583 | `				if( iNest < 32 ){` |
|       - | 7584 | `					/* Recurse */` |
|      11 | 7585 | `					iNest++;` |
|      11 | 7586 | `					rc = HashmapWalkRecursive((ph7_hashmap *)pValue->x.pOther,pCallback,pUserData,iNest);` |
|      11 | 7587 | `					iNest--;` |
|      11 | 7588 | `					if( rc == PH7_EXCEPTION ){` |
|     ! 0 | 7589 | `						return PH7_EXCEPTION;` |
|       - | 7590 | `					}` |
|       5 | 7591 | `				}` |
|       6 | 7592 | `			}else{` |
|       - | 7593 | `				/* Extract the node key */` |
|      27 | 7594 | `				PH7_HashmapExtractNodeKey(pEntry,&sKey);` |
|       - | 7595 | `				/* Invoke the supplied callback */` |
|      27 | 7596 | `				rc = PH7_VmCallUserFunctionAp(pMap->pVm,pCallback,0,pValue,&sKey,pUserData,0);` |
|      27 | 7597 | `				PH7_MemObjRelease(&sKey);` |
|      27 | 7598 | `				if( rc == PH7_EXCEPTION ){` |
|       - | 7599 | `					/* The callback raised: propagate so the dispatcher unwinds. */` |
|     ! 0 | 7600 | `					return PH7_EXCEPTION;` |
|       - | 7601 | `				}` |
|       - | 7602 | `			}` |
|      18 | 7603 | `		}` |
|       - | 7604 | `		/* Point to the next entry */` |
|      37 | 7605 | `		pEntry = pEntry->pPrev; /* Reverse link */` |
|      19 | 7606 | `	}` |
|      23 | 7607 | `	return PH7_OK;` |
|      12 | 7608 | `}` |
|       - | 7609 | `/*` |
|       - | 7610 | ` * bool array_walk_recursive(array &$array, callback $funcname [, mixed $userdata])` |
|       - | 7611 | ` *  Apply a user function recursively to every member of an array.` |
|       - | 7612 | ` * Parameters` |
|       - | 7613 | ` *  $array` |
|       - | 7614 | ` *   The input array.` |
|       - | 7615 | ` *  $funcname` |
|       - | 7616 | ` *   Typically, funcname takes on two parameters. The array parameter's value being` |
|       - | 7617 | ` *   the first, and the key/index second.` |
|       - | 7618 | ` * Note:` |
|       - | 7619 | ` *  If funcname needs to be working with the actual values of the array, specify the first` |
|       - | 7620 | ` *  parameter of funcname as a reference. Then, any changes made to those elements will` |
|       - | 7621 | ` *  be made in the original array itself.` |
|       - | 7622 | ` *  $userdata` |
|       - | 7623 | ` *   If the optional userdata parameter is supplied, it will be passed as the third parameter` |
|       - | 7624 | ` *   to the callback funcname.` |
|       - | 7625 | ` * Return` |
|       - | 7626 | ` *  Returns TRUE on success or FALSE on failure.` |
|       - | 7627 | ` */` |
|      30 | 7628 | `static int ph7_hashmap_walk_recursive(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       5 | 7629 | `{` |
|       - | 7630 | `	ph7_hashmap *pMap;` |
|      35 | 7631 | `	if( nArg < 2 ){` |
|       8 | 7632 | `		return PH7_VmThrowException(pCtx,` |
|       - | 7633 | `			"ArgumentCountError",` |
|       - | 7634 | `			"array_walk_recursive() expects at least 2 arguments, %d given",` |
|       2 | 7635 | `			nArg` |
|       - | 7636 | `			);` |
|       - | 7637 | `	}` |
|      31 | 7638 | `	if( nArg > 3 ){` |
|       4 | 7639 | `		return PH7_VmThrowException(pCtx,` |
|       - | 7640 | `			"ArgumentCountError",` |
|       - | 7641 | `			"array_walk_recursive() expects at most 3 arguments, %d given",` |
|       1 | 7642 | `			nArg` |
|       - | 7643 | `			);` |
|       - | 7644 | `	}` |
|      29 | 7645 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|       4 | 7646 | `		return PH7_VmThrowException(pCtx,` |
|       - | 7647 | `			"TypeError",` |
|       - | 7648 | `			"array_walk_recursive(): Argument #1 ($array) must be of type array, %s given",` |
|       1 | 7649 | `			ph7_type_name(apArg[0])` |
|       - | 7650 | `			);` |
|       - | 7651 | `	}` |
|      27 | 7652 | `	if( !ph7_value_is_callable(apArg[1]) ){` |
|      14 | 7653 | `		if( ph7_value_is_string(apArg[1]) ){` |
|       3 | 7654 | `			const char *zFunc = ph7_value_to_string(apArg[1],0);` |
|       4 | 7655 | `			return PH7_VmThrowException(pCtx,` |
|       - | 7656 | `				"TypeError",` |
|       - | 7657 | `				"array_walk_recursive(): Argument #2 ($callback) must be a valid callback, "` |
|       - | 7658 | `				"function \"%s\" not found or invalid function name",` |
|       1 | 7659 | `				zFunc` |
|       - | 7660 | `				);` |
|       - | 7661 | `		}` |
|      12 | 7662 | `		if( ph7_value_is_array(apArg[1]) ){` |
|       6 | 7663 | `			return PH7_VmThrowException(pCtx,` |
|       - | 7664 | `				"TypeError",` |
|       - | 7665 | `				"array_walk_recursive(): Argument #2 ($callback) must be a valid callback, "` |
|       - | 7666 | `				"array callback must have exactly two members"` |
|       - | 7667 | `				);` |
|       - | 7668 | `		}` |
|       6 | 7669 | `		return PH7_VmThrowException(pCtx,` |
|       - | 7670 | `			"TypeError",` |
|       - | 7671 | `			"array_walk_recursive(): Argument #2 ($callback) must be a valid callback, "` |
|       - | 7672 | `			"no array or string given"` |
|       - | 7673 | `			);` |
|       - | 7674 | `	}` |
|       - | 7675 | `	/* Point to the internal representation of the input hashmap */` |
|      13 | 7676 | `	PH7_HashmapCowSeparate(pCtx->pVm, apArg[0]);` |
|      13 | 7677 | `	pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|       - | 7678 | `	/* Perform the desired operation */` |
|      13 | 7679 | `	if( HashmapWalkRecursive(pMap,apArg[1],nArg > 2 ? apArg[2] : 0,0) == PH7_EXCEPTION ){` |
|       - | 7680 | `		/* A callback raised: propagate so the dispatcher unwinds. */` |
|     ! 0 | 7681 | `		return PH7_EXCEPTION;` |
|       - | 7682 | `	}` |
|       - | 7683 | `	/* All done, return TRUE */` |
|      13 | 7684 | `	ph7_result_bool(pCtx,1);` |
|      13 | 7685 | `	return PH7_OK;` |
|      20 | 7686 | `}` |
|       - | 7687 | `/*` |
|       - | 7688 | ` * bool array_is_list(array $array)` |
|       - | 7689 | ` *  Checks whether a given array is a list: its keys consist of consecutive` |
|       - | 7690 | ` *  integers starting at 0. An empty array is a list.` |
|       - | 7691 | ` * Return` |
|       - | 7692 | ` *  TRUE if the array is a list, FALSE otherwise.` |
|       - | 7693 | ` */` |
|       - | 7694 | `/*` |
|       - | 7695 | ` * Return TRUE if the given hashmap is a "list" [i.e: its keys are the` |
|       - | 7696 | ` * consecutive integers 0,1,2,... with no gaps]. An empty map is a list.` |
|       - | 7697 | ` * Shared by array_is_list() and the JSON encoder (vm_json.c).` |
|       - | 7698 | ` */` |
|     118 | 7699 | `PH7_PRIVATE int PH7_HashmapIsList(ph7_hashmap *pMap)` |
|       1 | 7700 | `{` |
|     119 | 7701 | `	ph7_hashmap_node *pNode = pMap->pFirst;` |
|     119 | 7702 | `	sxi64 iExpect = 0;` |
|       - | 7703 | `	sxu32 n;` |
|     253 | 7704 | `	for( n = 0 ; n < pMap->nEntry ; ++n ){` |
|     187 | 7705 | `		if( pNode->iType != HASHMAP_INT_NODE \|\| pNode->xKey.iKey != iExpect ){` |
|       - | 7706 | `			/* A non-integer key or a gap in the sequence: not a list */` |
|      53 | 7707 | `			return 0;` |
|       - | 7708 | `		}` |
|     135 | 7709 | `		++iExpect;` |
|     135 | 7710 | `		pNode = pNode->pPrev; /* Reverse link */` |
|      68 | 7711 | `	}` |
|      67 | 7712 | `	return 1;` |
|      60 | 7713 | `}` |
|      12 | 7714 | `static int ph7_hashmap_is_list(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       1 | 7715 | `{` |
|      13 | 7716 | `	if( nArg < 1 ){` |
|     ! 0 | 7717 | `		return PH7_VmThrowException(pCtx,` |
|       - | 7718 | `			"ArgumentCountError",` |
|       - | 7719 | `			"array_is_list() expects exactly 1 argument, 0 given"` |
|       - | 7720 | `			);` |
|       - | 7721 | `	}` |
|      13 | 7722 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|     ! 0 | 7723 | `		return PH7_VmThrowException(pCtx,` |
|       - | 7724 | `			"TypeError",` |
|       - | 7725 | `			"array_is_list(): Argument #1 ($array) must be of type array, %s given",` |
|     ! 0 | 7726 | `			ph7_type_name(apArg[0])` |
|       - | 7727 | `			);` |
|       - | 7728 | `	}` |
|      13 | 7729 | `	ph7_result_bool(pCtx,PH7_HashmapIsList((ph7_hashmap *)apArg[0]->x.pOther));` |
|      13 | 7730 | `	return PH7_OK;` |
|       7 | 7731 | `}` |
|       - | 7732 | `/*` |
|       - | 7733 | ` * mixed array_first(array $array)` |
|       - | 7734 | ` * mixed array_last(array $array)` |
|       - | 7735 | ` *  Return the value of the first (respectively last) element of the array,` |
|       - | 7736 | ` *  or NULL when the array is empty. The internal array pointer is left` |
|       - | 7737 | ` *  untouched (unlike reset()/end()).` |
|       - | 7738 | ` */` |
|      20 | 7739 | `static int HashmapFirstLast(ph7_context *pCtx,int nArg,ph7_value **apArg,int bLast)` |
|       1 | 7740 | `{` |
|       - | 7741 | `	ph7_hashmap *pMap;` |
|       - | 7742 | `	ph7_hashmap_node *pNode;` |
|       - | 7743 | `	ph7_value *pVal;` |
|      21 | 7744 | `	const char *zName = bLast ? "array_last" : "array_first";` |
|      21 | 7745 | `	if( nArg < 1 ){` |
|       4 | 7746 | `		return PH7_VmThrowException(pCtx,` |
|       - | 7747 | `			"ArgumentCountError",` |
|       - | 7748 | `			"%s() expects exactly 1 argument, 0 given",` |
|       1 | 7749 | `			zName` |
|       - | 7750 | `			);` |
|       - | 7751 | `	}` |
|      19 | 7752 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|       4 | 7753 | `		return PH7_VmThrowException(pCtx,` |
|       - | 7754 | `			"TypeError",` |
|       - | 7755 | `			"%s(): Argument #1 ($array) must be of type array, %s given",` |
|       1 | 7756 | `			zName,` |
|       1 | 7757 | `			ph7_type_name(apArg[0])` |
|       - | 7758 | `			);` |
|       - | 7759 | `	}` |
|      17 | 7760 | `	pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|      17 | 7761 | `	pNode = bLast ? pMap->pLast : pMap->pFirst;` |
|      17 | 7762 | `	if( pNode == 0 ){` |
|       - | 7763 | `		/* Empty array: PHP returns NULL */` |
|       5 | 7764 | `		ph7_result_null(pCtx);` |
|       5 | 7765 | `		return PH7_OK;` |
|       - | 7766 | `	}` |
|      13 | 7767 | `	pVal = HashmapExtractNodeValue(pNode);` |
|      13 | 7768 | `	if( pVal ){` |
|      13 | 7769 | `		ph7_result_value(pCtx,pVal);` |
|       7 | 7770 | `	}else{` |
|     ! 0 | 7771 | `		ph7_result_null(pCtx);` |
|       - | 7772 | `	}` |
|      13 | 7773 | `	return PH7_OK;` |
|      11 | 7774 | `}` |
|      10 | 7775 | `static int ph7_hashmap_first(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       1 | 7776 | `{` |
|      11 | 7777 | `	return HashmapFirstLast(pCtx,nArg,apArg,0);` |
|       1 | 7778 | `}` |
|      10 | 7779 | `static int ph7_hashmap_last(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       1 | 7780 | `{` |
|      11 | 7781 | `	return HashmapFirstLast(pCtx,nArg,apArg,1);` |
|       1 | 7782 | `}` |
|       - | 7783 | `/*` |
|       - | 7784 | ` * Fetch the element identified by 'pKey' from 'pRow' which may be either an` |
|       - | 7785 | ` * array (hashmap lookup) or an object (public attribute lookup). Used by` |
|       - | 7786 | ` * array_column() for both the column value and the index key.` |
|       - | 7787 | ` * Returns a borrowed pointer to the value, or NULL when the row is not a` |
|       - | 7788 | ` * container or the key is absent.` |
|       - | 7789 | ` */` |
|      32 | 7790 | `static ph7_value * HashmapColumnFetch(ph7_vm *pVm,ph7_value *pRow,ph7_value *pKey)` |
|       1 | 7791 | `{` |
|      33 | 7792 | `	if( ph7_value_is_array(pRow) ){` |
|       - | 7793 | `		ph7_hashmap_node *pNode;` |
|      25 | 7794 | `		if( PH7_HashmapLookup((ph7_hashmap *)pRow->x.pOther,pKey,&pNode) == SXRET_OK ){` |
|      21 | 7795 | `			return HashmapExtractNodeValue(pNode);` |
|       1 | 7796 | `		}` |
|      11 | 7797 | `	}else if( ph7_value_is_object(pRow) ){` |
|       - | 7798 | `		ph7_value sName;` |
|       - | 7799 | `		const char *zName;` |
|       - | 7800 | `		ph7_value *pAttr;` |
|       - | 7801 | `		/* Stringify a *copy* of the key (objects address attributes by name);` |
|       - | 7802 | `		 * never mutate pKey itself or the array-lookup path would break. */` |
|       9 | 7803 | `		PH7_MemObjInit(pVm,&sName);` |
|       9 | 7804 | `		PH7_MemObjStore(pKey,&sName);` |
|       9 | 7805 | `		zName = ph7_value_to_string(&sName,0); /* NUL-terminated */` |
|       9 | 7806 | `		pAttr = ph7_object_fetch_attr(pRow,zName);` |
|       9 | 7807 | `		PH7_MemObjRelease(&sName);` |
|       9 | 7808 | `		return pAttr;` |
|       - | 7809 | `	}` |
|       5 | 7810 | `	return 0;` |
|      17 | 7811 | `}` |
|       - | 7812 | `/*` |
|       - | 7813 | ` * array array_column(array $array, int\|string\|null $column_key, int\|string\|null $index_key = null)` |
|       - | 7814 | ` *  Returns the values from a single column of the input, identified by` |
|       - | 7815 | ` *  $column_key. Optionally indexes the result by the $index_key column.` |
|       - | 7816 | ` *  A NULL $column_key collects the whole row. Rows missing the column are` |
|       - | 7817 | ` *  skipped; rows missing the index key are appended with a numeric key.` |
|       - | 7818 | ` *  Each row may be an array or an object.` |
|       - | 7819 | ` */` |
|      12 | 7820 | `static int ph7_hashmap_column(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       1 | 7821 | `{` |
|       - | 7822 | `	ph7_hashmap_node *pNode;` |
|       - | 7823 | `	ph7_hashmap *pMap;` |
|       - | 7824 | `	ph7_value *pArray;` |
|       - | 7825 | `	ph7_value *pRow;` |
|       - | 7826 | `	ph7_value *pCol;` |
|       - | 7827 | `	ph7_value *pIdx;` |
|       - | 7828 | `	int bWantCol;` |
|       - | 7829 | `	int bWantIdx;` |
|       - | 7830 | `	sxu32 n;` |
|      13 | 7831 | `	if( nArg < 2 ){` |
|     ! 0 | 7832 | `		return PH7_VmThrowException(pCtx,` |
|       - | 7833 | `			"ArgumentCountError",` |
|       - | 7834 | `			"array_column() expects at least 2 arguments, %d given",` |
|     ! 0 | 7835 | `			nArg` |
|       - | 7836 | `			);` |
|       - | 7837 | `	}` |
|      13 | 7838 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|     ! 0 | 7839 | `		return PH7_VmThrowException(pCtx,` |
|       - | 7840 | `			"TypeError",` |
|       - | 7841 | `			"array_column(): Argument #1 ($array) must be of type array, %s given",` |
|     ! 0 | 7842 | `			ph7_type_name(apArg[0])` |
|       - | 7843 | `			);` |
|       - | 7844 | `	}` |
|      13 | 7845 | `	pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|      13 | 7846 | `	pArray = ph7_context_new_array(pCtx);` |
|      13 | 7847 | `	if( pArray == 0 ){` |
|     ! 0 | 7848 | `		ph7_result_null(pCtx);` |
|     ! 0 | 7849 | `		return PH7_OK;` |
|       - | 7850 | `	}` |
|       - | 7851 | `	/* A NULL column_key means "collect the entire row". */` |
|      13 | 7852 | `	bWantCol = !ph7_value_is_null(apArg[1]);` |
|      13 | 7853 | `	bWantIdx = (nArg > 2 && !ph7_value_is_null(apArg[2]));` |
|      13 | 7854 | `	pNode = pMap->pFirst;` |
|      33 | 7855 | `	for( n = 0 ; n < pMap->nEntry ; ++n ){` |
|      21 | 7856 | `		pRow = HashmapExtractNodeValue(pNode);` |
|      21 | 7857 | `		pNode = pNode->pPrev; /* Advance now so 'continue' is safe */` |
|      21 | 7858 | `		if( pRow == 0 ){` |
|     ! 0 | 7859 | `			continue;` |
|       - | 7860 | `		}` |
|      21 | 7861 | `		if( bWantCol ){` |
|      19 | 7862 | `			pCol = HashmapColumnFetch(pMap->pVm,pRow,apArg[1]);` |
|      19 | 7863 | `			if( pCol == 0 ){` |
|       - | 7864 | `				/* Row lacks the requested column: skip it (PHP semantics). */` |
|       3 | 7865 | `				continue;` |
|       - | 7866 | `			}` |
|       9 | 7867 | `		}else{` |
|       3 | 7868 | `			pCol = pRow;` |
|       - | 7869 | `		}` |
|      19 | 7870 | `		pIdx = bWantIdx ? HashmapColumnFetch(pMap->pVm,pRow,apArg[2]) : 0;` |
|      19 | 7871 | `		if( pIdx ){` |
|      13 | 7872 | `			ph7_array_add_elem(pArray,pIdx,pCol);` |
|       7 | 7873 | `		}else{` |
|       7 | 7874 | `			ph7_array_add_elem(pArray,0,pCol); /* Auto-index */` |
|       - | 7875 | `		}` |
|      10 | 7876 | `	}` |
|      13 | 7877 | `	ph7_result_value(pCtx,pArray);` |
|      13 | 7878 | `	return PH7_OK;` |
|       7 | 7879 | `}` |
|       - | 7880 | `/*` |
|       - | 7881 | ` * Shared core for array_find/array_find_key/array_any/array_all (PHP 8.4).` |
|       - | 7882 | ` * Invokes $callback($value, $key) over each entry and reports the first node` |
|       - | 7883 | ` * whose truthiness equals 'bWant'. Propagates a callback exception as` |
|       - | 7884 | ` * PH7_EXCEPTION; sets *ppMatch to the matching node (or NULL if none).` |
|       - | 7885 | ` */` |
|      28 | 7886 | `static sxi32 HashmapCallbackSearch(` |
|       - | 7887 | `	ph7_context *pCtx,int nArg,ph7_value **apArg,` |
|       - | 7888 | `	const char *zName,            /* Function name for diagnostics */` |
|       - | 7889 | `	int bWant,                    /* Truthiness being hunted for */` |
|       - | 7890 | `	ph7_hashmap_node **ppMatch    /* OUT: first matching node or NULL */` |
|       - | 7891 | `	)` |
|       1 | 7892 | `{` |
|       - | 7893 | `	ph7_hashmap_node *pEntry;` |
|       - | 7894 | `	ph7_hashmap *pMap;` |
|       - | 7895 | `	ph7_value *pValue;` |
|       - | 7896 | `	ph7_value *apCbArg[2];` |
|       - | 7897 | `	ph7_value sKey;` |
|       - | 7898 | `	ph7_value sResult;` |
|       - | 7899 | `	sxi32 rc;` |
|       - | 7900 | `	sxu32 n;` |
|      29 | 7901 | `	*ppMatch = 0;` |
|      29 | 7902 | `	if( nArg < 2 ){` |
|     ! 0 | 7903 | `		return PH7_VmThrowException(pCtx,` |
|       - | 7904 | `			"ArgumentCountError",` |
|       - | 7905 | `			"%s() expects exactly 2 arguments, %d given",` |
|     ! 0 | 7906 | `			zName,nArg` |
|       - | 7907 | `			);` |
|       - | 7908 | `	}` |
|      29 | 7909 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|     ! 0 | 7910 | `		return PH7_VmThrowException(pCtx,` |
|       - | 7911 | `			"TypeError",` |
|       - | 7912 | `			"%s(): Argument #1 ($array) must be of type array, %s given",` |
|     ! 0 | 7913 | `			zName,ph7_type_name(apArg[0])` |
|       - | 7914 | `			);` |
|       - | 7915 | `	}` |
|      29 | 7916 | `	if( !ph7_value_is_callable(apArg[1]) ){` |
|     ! 0 | 7917 | `		return PH7_VmThrowException(pCtx,` |
|       - | 7918 | `			"TypeError",` |
|       - | 7919 | `			"%s(): Argument #2 ($callback) must be a valid callback, %s given",` |
|     ! 0 | 7920 | `			zName,ph7_type_name(apArg[1])` |
|       - | 7921 | `			);` |
|       - | 7922 | `	}` |
|      29 | 7923 | `	pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|      29 | 7924 | `	pEntry = pMap->pFirst;` |
|      29 | 7925 | `	PH7_MemObjInit(pMap->pVm,&sKey);` |
|      29 | 7926 | `	sKey.nIdx = SXU32_HIGH;    /* Mark as constant */` |
|      29 | 7927 | `	PH7_MemObjInit(pMap->pVm,&sResult);` |
|      29 | 7928 | `	sResult.nIdx = SXU32_HIGH; /* Mark as constant */` |
|      73 | 7929 | `	for( n = 0 ; n < pMap->nEntry ; ++n ){` |
|      59 | 7930 | `		pValue = HashmapExtractNodeValue(pEntry);` |
|      59 | 7931 | `		if( pValue ){` |
|       - | 7932 | `			/* The callback receives ($value, $key). */` |
|      59 | 7933 | `			PH7_HashmapExtractNodeKey(pEntry,&sKey);` |
|      59 | 7934 | `			apCbArg[0] = pValue;` |
|      59 | 7935 | `			apCbArg[1] = &sKey;` |
|      59 | 7936 | `			rc = PH7_VmCallUserFunction(pMap->pVm,apArg[1],2,apCbArg,&sResult);` |
|      59 | 7937 | `			if( rc == PH7_EXCEPTION ){` |
|       - | 7938 | `				/* The callback raised: propagate so the dispatcher unwinds. */` |
|     ! 0 | 7939 | `				PH7_MemObjRelease(&sKey);` |
|     ! 0 | 7940 | `				PH7_MemObjRelease(&sResult);` |
|     ! 0 | 7941 | `				return PH7_EXCEPTION;` |
|       - | 7942 | `			}` |
|      59 | 7943 | `			if( rc == SXRET_OK && (ph7_value_to_bool(&sResult) ? 1 : 0) == bWant ){` |
|      15 | 7944 | `				*ppMatch = pEntry;` |
|      15 | 7945 | `				break;` |
|       - | 7946 | `			}` |
|      22 | 7947 | `		}` |
|      45 | 7948 | `		pEntry = pEntry->pPrev; /* Reverse link */` |
|      23 | 7949 | `	}` |
|      29 | 7950 | `	PH7_MemObjRelease(&sKey);` |
|      29 | 7951 | `	PH7_MemObjRelease(&sResult);` |
|      29 | 7952 | `	return PH7_OK;` |
|      15 | 7953 | `}` |
|       - | 7954 | `/*` |
|       - | 7955 | ` * mixed array_find(array $array, callable $callback)` |
|       - | 7956 | ` *  Returns the value of the first element for which $callback($value,$key)` |
|       - | 7957 | ` *  is truthy, or NULL if none match.` |
|       - | 7958 | ` */` |
|       6 | 7959 | `static int ph7_hashmap_find(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       1 | 7960 | `{` |
|       - | 7961 | `	ph7_hashmap_node *pMatch;` |
|       - | 7962 | `	ph7_value *pVal;` |
|       - | 7963 | `	sxi32 rc;` |
|       7 | 7964 | `	rc = HashmapCallbackSearch(pCtx,nArg,apArg,"array_find",1,&pMatch);` |
|       7 | 7965 | `	if( rc != PH7_OK ){` |
|     ! 0 | 7966 | `		return rc;` |
|       - | 7967 | `	}` |
|       7 | 7968 | `	if( pMatch && (pVal = HashmapExtractNodeValue(pMatch)) != 0 ){` |
|       5 | 7969 | `		ph7_result_value(pCtx,pVal);` |
|       3 | 7970 | `	}else{` |
|       3 | 7971 | `		ph7_result_null(pCtx);` |
|       - | 7972 | `	}` |
|       7 | 7973 | `	return PH7_OK;` |
|       4 | 7974 | `}` |
|       - | 7975 | `/*` |
|       - | 7976 | ` * mixed array_find_key(array $array, callable $callback)` |
|       - | 7977 | ` *  Returns the key of the first element for which $callback($value,$key)` |
|       - | 7978 | ` *  is truthy, or NULL if none match.` |
|       - | 7979 | ` */` |
|       6 | 7980 | `static int ph7_hashmap_find_key(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       1 | 7981 | `{` |
|       - | 7982 | `	ph7_hashmap_node *pMatch;` |
|       - | 7983 | `	sxi32 rc;` |
|       7 | 7984 | `	rc = HashmapCallbackSearch(pCtx,nArg,apArg,"array_find_key",1,&pMatch);` |
|       7 | 7985 | `	if( rc != PH7_OK ){` |
|     ! 0 | 7986 | `		return rc;` |
|       - | 7987 | `	}` |
|       7 | 7988 | `	if( pMatch == 0 ){` |
|       3 | 7989 | `		ph7_result_null(pCtx);` |
|       6 | 7990 | `	}else if( pMatch->iType == HASHMAP_INT_NODE ){` |
|       3 | 7991 | `		ph7_result_int64(pCtx,pMatch->xKey.iKey);` |
|       2 | 7992 | `	}else{` |
|       4 | 7993 | `		ph7_result_string(pCtx,` |
|       2 | 7994 | `			(const char *)SyBlobData(&pMatch->xKey.sKey),` |
|       2 | 7995 | `			(int)SyBlobLength(&pMatch->xKey.sKey));` |
|       - | 7996 | `	}` |
|       7 | 7997 | `	return PH7_OK;` |
|       4 | 7998 | `}` |
|       - | 7999 | `/*` |
|       - | 8000 | ` * bool array_any(array $array, callable $callback)` |
|       - | 8001 | ` *  Returns TRUE if $callback($value,$key) is truthy for at least one element.` |
|       - | 8002 | ` *  FALSE for an empty array.` |
|       - | 8003 | ` */` |
|       8 | 8004 | `static int ph7_hashmap_any(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       1 | 8005 | `{` |
|       - | 8006 | `	ph7_hashmap_node *pMatch;` |
|       - | 8007 | `	sxi32 rc;` |
|       9 | 8008 | `	rc = HashmapCallbackSearch(pCtx,nArg,apArg,"array_any",1,&pMatch);` |
|       9 | 8009 | `	if( rc != PH7_OK ){` |
|     ! 0 | 8010 | `		return rc;` |
|       - | 8011 | `	}` |
|       9 | 8012 | `	ph7_result_bool(pCtx,pMatch != 0);` |
|       9 | 8013 | `	return PH7_OK;` |
|       5 | 8014 | `}` |
|       - | 8015 | `/*` |
|       - | 8016 | ` * bool array_all(array $array, callable $callback)` |
|       - | 8017 | ` *  Returns TRUE if $callback($value,$key) is truthy for every element (and for` |
|       - | 8018 | ` *  an empty array). Hunts for the first falsy element: its absence means "all".` |
|       - | 8019 | ` */` |
|       8 | 8020 | `static int ph7_hashmap_all(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       1 | 8021 | `{` |
|       - | 8022 | `	ph7_hashmap_node *pMatch;` |
|       - | 8023 | `	sxi32 rc;` |
|       9 | 8024 | `	rc = HashmapCallbackSearch(pCtx,nArg,apArg,"array_all",0,&pMatch);` |
|       9 | 8025 | `	if( rc != PH7_OK ){` |
|     ! 0 | 8026 | `		return rc;` |
|       - | 8027 | `	}` |
|       9 | 8028 | `	ph7_result_bool(pCtx,pMatch == 0);` |
|       9 | 8029 | `	return PH7_OK;` |
|       5 | 8030 | `}` |
|       - | 8031 | `/*` |
|       - | 8032 | ` * The iterator_*() family — walk a Traversable via the shared PH7_VmIteratorWalk` |
|       - | 8033 | ` * helper (the reusable form of the foreach Iterator protocol).` |
|       - | 8034 | ` */` |
|       - | 8035 | `/* Step shared by iterator_to_array (pArray set) and iterator_count (pArray NULL). */` |
|       - | 8036 | `struct IterCollect { ph7_value *pArray; int bPreserve; sxi64 nCount; };` |
|      70 | 8037 | `static sxi32 IterCollectStep(ph7_vm *pVm, ph7_value *pKey, ph7_value *pValue, void *pUserData)` |
|       4 | 8038 | `{` |
|      74 | 8039 | `	struct IterCollect *p = (struct IterCollect *)pUserData;` |
|      35 | 8040 | `	(void)pVm;` |
|      74 | 8041 | `	p->nCount++;` |
|      74 | 8042 | `	if( p->pArray ){` |
|       - | 8043 | `		/* preserve_keys: insert with the iterator key (later wins on collision);` |
|       - | 8044 | `		 * otherwise append with an auto-assigned int index. */` |
|      66 | 8045 | `		ph7_array_add_elem(p->pArray, p->bPreserve ? pKey : 0, pValue);` |
|      31 | 8046 | `	}` |
|      74 | 8047 | `	return SXRET_OK;` |
|       4 | 8048 | `}` |
|       - | 8049 | `/*` |
|       - | 8050 | ` * array iterator_to_array(Traversable\|array $iterator, bool $preserve_keys = true)` |
|       - | 8051 | ` */` |
|      26 | 8052 | `static int ph7_iterator_to_array(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|       4 | 8053 | `{` |
|       - | 8054 | `	struct IterCollect sCol;` |
|       - | 8055 | `	ph7_value *pArray;` |
|       - | 8056 | `	sxi32 rc;` |
|      30 | 8057 | `	if( nArg < 1 ){ ph7_result_null(pCtx); return PH7_OK; }` |
|      30 | 8058 | `	pArray = ph7_context_new_array(pCtx);` |
|      30 | 8059 | `	if( pArray == 0 ){ ph7_result_null(pCtx); return PH7_OK; }` |
|      30 | 8060 | `	sCol.pArray = pArray;` |
|      30 | 8061 | `	sCol.bPreserve = (nArg > 1) ? ph7_value_to_bool(apArg[1]) : 1;` |
|      30 | 8062 | `	sCol.nCount = 0;` |
|      30 | 8063 | `	if( ph7_value_is_array(apArg[0]) ){` |
|       - | 8064 | `		/* PHP 8.2 accepts a plain array: copy it (preserving or renumbering keys). */` |
|       3 | 8065 | `		ph7_hashmap *pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|       3 | 8066 | `		ph7_hashmap_node *pEntry = pMap->pFirst;` |
|       - | 8067 | `		sxu32 n;` |
|       9 | 8068 | `		for( n = 0 ; n < pMap->nEntry ; n++ ){` |
|       - | 8069 | `			ph7_value sKey, *pVal;` |
|       7 | 8070 | `			PH7_MemObjInit(pCtx->pVm,&sKey);` |
|       7 | 8071 | `			PH7_HashmapExtractNodeKey(pEntry,&sKey);` |
|       7 | 8072 | `			pVal = (ph7_value *)SySetAt(&pCtx->pVm->aMemObj,pEntry->nValIdx);` |
|       7 | 8073 | `			if( pVal ){ ph7_array_add_elem(pArray, sCol.bPreserve ? &sKey : 0, pVal); }` |
|       7 | 8074 | `			PH7_MemObjRelease(&sKey);` |
|       7 | 8075 | `			pEntry = pEntry->pPrev;` |
|       4 | 8076 | `		}` |
|       3 | 8077 | `		ph7_result_value(pCtx,pArray);` |
|       3 | 8078 | `		return PH7_OK;` |
|       - | 8079 | `	}` |
|      28 | 8080 | `	rc = PH7_VmIteratorWalk(pCtx->pVm, apArg[0], IterCollectStep, &sCol);` |
|      28 | 8081 | `	if( rc == PH7_EXCEPTION \|\| rc == PH7_ABORT ){ return rc; }` |
|      26 | 8082 | `	if( rc == SXERR_NOTIMPLEMENTED ){` |
|     ! 0 | 8083 | `		return PH7_VmThrowException(pCtx,"TypeError",` |
|       - | 8084 | `			"iterator_to_array(): Argument #1 ($iterator) must be of type Traversable\|array, %s given",` |
|     ! 0 | 8085 | `			ph7_type_name(apArg[0]));` |
|       - | 8086 | `	}` |
|      26 | 8087 | `	ph7_result_value(pCtx,pArray);` |
|      26 | 8088 | `	return PH7_OK;` |
|      17 | 8089 | `}` |
|       - | 8090 | `/*` |
|       - | 8091 | ` * int iterator_count(Traversable\|array $iterator)` |
|       - | 8092 | ` */` |
|       6 | 8093 | `static int ph7_iterator_count(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|       1 | 8094 | `{` |
|       - | 8095 | `	struct IterCollect sCol;` |
|       - | 8096 | `	sxi32 rc;` |
|       7 | 8097 | `	if( nArg < 1 ){ ph7_result_int(pCtx,0); return PH7_OK; }` |
|       7 | 8098 | `	if( ph7_value_is_array(apArg[0]) ){` |
|       3 | 8099 | `		ph7_result_int64(pCtx, (ph7_int64)((ph7_hashmap *)apArg[0]->x.pOther)->nEntry);` |
|       3 | 8100 | `		return PH7_OK;` |
|       - | 8101 | `	}` |
|       5 | 8102 | `	sCol.pArray = 0; sCol.bPreserve = 0; sCol.nCount = 0;` |
|       5 | 8103 | `	rc = PH7_VmIteratorWalk(pCtx->pVm, apArg[0], IterCollectStep, &sCol);` |
|       5 | 8104 | `	if( rc == PH7_EXCEPTION \|\| rc == PH7_ABORT ){ return rc; }` |
|       5 | 8105 | `	if( rc == SXERR_NOTIMPLEMENTED ){` |
|     ! 0 | 8106 | `		return PH7_VmThrowException(pCtx,"TypeError",` |
|       - | 8107 | `			"iterator_count(): Argument #1 ($iterator) must be of type Traversable\|array, %s given",` |
|     ! 0 | 8108 | `			ph7_type_name(apArg[0]));` |
|       - | 8109 | `	}` |
|       5 | 8110 | `	ph7_result_int64(pCtx, sCol.nCount);` |
|       5 | 8111 | `	return PH7_OK;` |
|       4 | 8112 | `}` |
|       - | 8113 | `/* iterator_apply step: call the fixed callback with $args each iteration. The` |
|       - | 8114 | ` * arg pointers are resolved fresh per step because the iterator's own methods` |
|       - | 8115 | ` * run user code between iterations and may reallocate the aMemObj pool. */` |
|       - | 8116 | `struct IterApply { ph7_value *pCallback; ph7_value *pArgsArray; sxi64 nCount; };` |
|      24 | 8117 | `static sxi32 IterApplyStep(ph7_vm *pVm, ph7_value *pKey, ph7_value *pValue, void *pUserData)` |
|       1 | 8118 | `{` |
|      25 | 8119 | `	struct IterApply *p = (struct IterApply *)pUserData;` |
|       - | 8120 | `	ph7_value sResult;` |
|       - | 8121 | `	SySet aArg;` |
|       - | 8122 | `	sxi32 rc;` |
|       - | 8123 | `	int bContinue;` |
|      12 | 8124 | `	(void)pKey; (void)pValue; /* iterator_apply does NOT pass the element to the callback */` |
|      25 | 8125 | `	SySetInit(&aArg,&pVm->sAllocator,sizeof(ph7_value *));` |
|      25 | 8126 | `	if( p->pArgsArray && (p->pArgsArray->iFlags & MEMOBJ_HASHMAP) ){` |
|       9 | 8127 | `		ph7_hashmap *pMap = (ph7_hashmap *)p->pArgsArray->x.pOther;` |
|       9 | 8128 | `		ph7_hashmap_node *pEntry = pMap->pFirst;` |
|       - | 8129 | `		sxu32 n;` |
|      17 | 8130 | `		for( n = 0 ; n < pMap->nEntry ; n++ ){` |
|       9 | 8131 | `			ph7_value *pVal = (ph7_value *)SySetAt(&pVm->aMemObj,pEntry->nValIdx);` |
|       9 | 8132 | `			if( pVal ){ SySetPut(&aArg,(const void *)&pVal); }` |
|       9 | 8133 | `			pEntry = pEntry->pPrev;` |
|       5 | 8134 | `		}` |
|       4 | 8135 | `	}` |
|      25 | 8136 | `	PH7_MemObjInit(pVm,&sResult);` |
|      37 | 8137 | `	rc = PH7_VmCallUserFunction(pVm, p->pCallback, (int)SySetUsed(&aArg),` |
|      24 | 8138 | `		(ph7_value **)SySetBasePtr(&aArg), &sResult);` |
|      25 | 8139 | `	SySetRelease(&aArg);` |
|      25 | 8140 | `	if( rc == PH7_EXCEPTION \|\| rc == PH7_ABORT ){ PH7_MemObjRelease(&sResult); return rc; }` |
|      23 | 8141 | `	p->nCount++;` |
|      23 | 8142 | `	PH7_MemObjToBool(&sResult);` |
|      23 | 8143 | `	bContinue = (sResult.x.iVal != 0);` |
|      23 | 8144 | `	PH7_MemObjRelease(&sResult);` |
|      23 | 8145 | `	return bContinue ? SXRET_OK : SXERR_EOF; /* falsy return stops iteration */` |
|      13 | 8146 | `}` |
|       - | 8147 | `/*` |
|       - | 8148 | ` * int iterator_apply(Traversable $iterator, callable $callback, array $args = [])` |
|       - | 8149 | ` */` |
|       8 | 8150 | `static int ph7_iterator_apply(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|       1 | 8151 | `{` |
|       - | 8152 | `	struct IterApply sApp;` |
|       - | 8153 | `	sxi32 rc;` |
|       9 | 8154 | `	if( nArg < 2 ){ ph7_result_int(pCtx,0); return PH7_OK; }` |
|       9 | 8155 | `	if( !ph7_value_is_callable(apArg[1]) ){` |
|     ! 0 | 8156 | `		return PH7_VmThrowException(pCtx,"TypeError",` |
|       - | 8157 | `			"iterator_apply(): Argument #2 ($callback) must be a valid callback");` |
|       - | 8158 | `	}` |
|       9 | 8159 | `	sApp.pCallback = apArg[1];` |
|       9 | 8160 | `	sApp.pArgsArray = (nArg > 2 && ph7_value_is_array(apArg[2])) ? apArg[2] : 0;` |
|       9 | 8161 | `	sApp.nCount = 0;` |
|       9 | 8162 | `	rc = PH7_VmIteratorWalk(pCtx->pVm, apArg[0], IterApplyStep, &sApp);` |
|       9 | 8163 | `	if( rc == PH7_EXCEPTION \|\| rc == PH7_ABORT ){ return rc; }` |
|       7 | 8164 | `	if( rc == SXERR_NOTIMPLEMENTED ){` |
|     ! 0 | 8165 | `		return PH7_VmThrowException(pCtx,"TypeError",` |
|       - | 8166 | `			"iterator_apply(): Argument #1 ($iterator) must be of type Traversable, %s given",` |
|     ! 0 | 8167 | `			ph7_type_name(apArg[0]));` |
|       - | 8168 | `	}` |
|       7 | 8169 | `	ph7_result_int64(pCtx, sApp.nCount);` |
|       7 | 8170 | `	return PH7_OK;` |
|       5 | 8171 | `}` |
|       - | 8172 | `/*` |
|       - | 8173 | ` * Table of hashmap functions.` |
|       - | 8174 | ` */` |
|       - | 8175 | `static const ph7_builtin_func aHashmapFunc[] = {` |
|       - | 8176 | `	{"iterator_to_array",  ph7_iterator_to_array },` |
|       - | 8177 | `	{"iterator_count",     ph7_iterator_count },` |
|       - | 8178 | `	{"iterator_apply",     ph7_iterator_apply },` |
|       - | 8179 | `	{"count",             ph7_hashmap_count },` |
|       - | 8180 | `	{"sizeof",            ph7_hashmap_count },` |
|       - | 8181 | `	{"array_key_exists",  ph7_hashmap_key_exists },` |
|       - | 8182 | `	{"array_pop",         ph7_hashmap_pop     },` |
|       - | 8183 | `	{"array_push",        ph7_hashmap_push    },` |
|       - | 8184 | `	{"array_shift",       ph7_hashmap_shift   },` |
|       - | 8185 | `	{"array_product",     ph7_hashmap_product },` |
|       - | 8186 | `	{"array_sum",         ph7_hashmap_sum     },` |
|       - | 8187 | `	{"array_keys",        ph7_hashmap_keys    },` |
|       - | 8188 | `	{"array_values",      ph7_hashmap_values  },` |
|       - | 8189 | `	{"array_same",        ph7_hashmap_same    },  /* Symisc eXtension */` |
|       - | 8190 | `	{"array_merge",       ph7_hashmap_merge   },` |
|       - | 8191 | `	{"array_slice",       ph7_hashmap_slice   },` |
|       - | 8192 | `	{"array_splice",      ph7_hashmap_splice  },` |
|       - | 8193 | `	{"array_search",      ph7_hashmap_search  },` |
|       - | 8194 | `	{"array_diff",        ph7_hashmap_diff    },` |
|       - | 8195 | `	{"array_udiff",       ph7_hashmap_udiff   },` |
|       - | 8196 | `	{"array_diff_assoc",  ph7_hashmap_diff_assoc },` |
|       - | 8197 | `	{"array_diff_uassoc", ph7_hashmap_diff_uassoc },` |
|       - | 8198 | `	{"array_diff_key",    ph7_hashmap_diff_key },` |
|       - | 8199 | `	{"array_intersect",   ph7_hashmap_intersect},` |
|       - | 8200 | `	{"array_intersect_assoc", ph7_hashmap_intersect_assoc},` |
|       - | 8201 | `	{"array_uintersect",  ph7_hashmap_uintersect},` |
|       - | 8202 | `	{"array_intersect_key",   ph7_hashmap_intersect_key},` |
|       - | 8203 | `	{"array_copy",        ph7_hashmap_copy    },` |
|       - | 8204 | `	{"array_erase",       ph7_hashmap_erase   },` |
|       - | 8205 | `	{"array_fill",        ph7_hashmap_fill    },` |
|       - | 8206 | `	{"array_fill_keys",   ph7_hashmap_fill_keys},` |
|       - | 8207 | `	{"array_combine",     ph7_hashmap_combine },` |
|       - | 8208 | `	{"array_reverse",     ph7_hashmap_reverse },` |
|       - | 8209 | `	{"array_unique",      ph7_hashmap_unique  },` |
|       - | 8210 | `	{"array_flip",        ph7_hashmap_flip    },` |
|       - | 8211 | `	{"array_rand",        ph7_hashmap_rand    },` |
|       - | 8212 | `	{"array_chunk",       ph7_hashmap_chunk   },` |
|       - | 8213 | `	{"array_pad",         ph7_hashmap_pad     },` |
|       - | 8214 | `	{"array_replace",     ph7_hashmap_replace },` |
|       - | 8215 | `	{"array_filter",      ph7_hashmap_filter  },` |
|       - | 8216 | `	{"array_map",         ph7_hashmap_map     },` |
|       - | 8217 | `	{"array_column",      ph7_hashmap_column  },` |
|       - | 8218 | `	{"array_is_list",     ph7_hashmap_is_list },` |
|       - | 8219 | `	{"array_first",       ph7_hashmap_first   },` |
|       - | 8220 | `	{"array_last",        ph7_hashmap_last    },` |
|       - | 8221 | `	{"array_find",        ph7_hashmap_find    },` |
|       - | 8222 | `	{"array_find_key",    ph7_hashmap_find_key},` |
|       - | 8223 | `	{"array_any",         ph7_hashmap_any     },` |
|       - | 8224 | `	{"array_all",         ph7_hashmap_all     },` |
|       - | 8225 | `	{"array_reduce",      ph7_hashmap_reduce  },` |
|       - | 8226 | `	{"array_walk",        ph7_hashmap_walk    },` |
|       - | 8227 | `	{"array_walk_recursive", ph7_hashmap_walk_recursive },` |
|       - | 8228 | `	{"in_array",          ph7_hashmap_in_array},` |
|       - | 8229 | `	{"sort",              ph7_hashmap_sort    },` |
|       - | 8230 | `	{"asort",             ph7_hashmap_asort   },` |
|       - | 8231 | `	{"arsort",            ph7_hashmap_arsort  },` |
|       - | 8232 | `	{"ksort",             ph7_hashmap_ksort   },` |
|       - | 8233 | `	{"krsort",            ph7_hashmap_krsort  },` |
|       - | 8234 | `	{"rsort",             ph7_hashmap_rsort   },` |
|       - | 8235 | `	{"usort",             ph7_hashmap_usort   },` |
|       - | 8236 | `	{"uasort",            ph7_hashmap_uasort  },` |
|       - | 8237 | `	{"uksort",            ph7_hashmap_uksort  },` |
|       - | 8238 | `	{"shuffle",           ph7_hashmap_shuffle },` |
|       - | 8239 | `	{"range",             ph7_hashmap_range   },` |
|       - | 8240 | `	{"current",           ph7_hashmap_current },` |
|       - | 8241 | `	{"each",              ph7_hashmap_each    },` |
|       - | 8242 | `	{"pos",               ph7_hashmap_current },` |
|       - | 8243 | `	{"next",              ph7_hashmap_next    },` |
|       - | 8244 | `	{"prev",              ph7_hashmap_prev    },` |
|       - | 8245 | `	{"end",               ph7_hashmap_end     },` |
|       - | 8246 | `	{"reset",             ph7_hashmap_reset   },` |
|       - | 8247 | `	{"key",               ph7_hashmap_simple_key }` |
|       - | 8248 | `};` |
|       - | 8249 | `/*` |
|       - | 8250 | ` * Register the built-in hashmap functions defined above.` |
|       - | 8251 | ` */` |
|    3456 | 8252 | `PH7_PRIVATE void PH7_RegisterHashmapFunctions(ph7_vm *pVm)` |
|       5 | 8253 | `{` |
|       - | 8254 | `	sxu32 n;` |
|  252293 | 8255 | `	for( n = 0 ; n < SX_ARRAYSIZE(aHashmapFunc) ; n++ ){` |
|  248837 | 8256 | `		ph7_create_function(&(*pVm),aHashmapFunc[n].zName,aHashmapFunc[n].xFunc,0);` |
|  124421 | 8257 | `	}` |
|    3461 | 8258 | `}` |
|       - | 8259 | `/*` |
|       - | 8260 | ` * Dump a hashmap instance and it's entries and the store the dump in` |
|       - | 8261 | ` * the BLOB given as the first argument.` |
|       - | 8262 | ` * This function is typically invoked when the user issue a call to` |
|       - | 8263 | ` * [var_dump(),var_export(),print_r(),...]` |
|       - | 8264 | ` * This function SXRET_OK on success. Any other return value including` |
|       - | 8265 | ` * SXERR_LIMIT(infinite recursion) indicates failure.` |
|       - | 8266 | ` */` |
|       - | 8267 | `/*` |
|       - | 8268 | ` * Dump the entries of a hashmap [i.e: the key/value lines between the opening` |
|       - | 8269 | ` * '{' and the closing '}'] in the var_dump/print_r style. Factored out of` |
|       - | 8270 | ` * PH7_HashmapDump so the var_dump object renderer can reuse it for a` |
|       - | 8271 | ` * __debugInfo() array body (which carries an object header, not "array(N)").` |
|       - | 8272 | ` * Returns SXERR_LIMIT if a nested value hit the depth cap.` |
|       - | 8273 | ` */` |
|      26 | 8274 | `PH7_PRIVATE sxi32 PH7_HashmapDumpEntries(SyBlob *pOut,ph7_hashmap *pMap,int ShowType,int nTab,int nDepth)` |
|       3 | 8275 | `{` |
|      29 | 8276 | `	ph7_hashmap_node *pEntry = pMap->pFirst;` |
|       - | 8277 | `	ph7_value *pObj;` |
|      29 | 8278 | `	sxu32 n = 0;` |
|       - | 8279 | `	int isRef;` |
|      29 | 8280 | `	sxi32 rc = SXRET_OK;` |
|       - | 8281 | `	int i;` |
|      44 | 8282 | `	for(;;){` |
|      91 | 8283 | `		if( n >= pMap->nEntry ){` |
|      29 | 8284 | `			break;` |
|       - | 8285 | `		}` |
|     127 | 8286 | `		for( i = 0 ; i < nTab ; i++ ){` |
|      65 | 8287 | `			SyBlobAppend(&(*pOut)," ",sizeof(char));` |
|      34 | 8288 | `		}` |
|       - | 8289 | `		/* Dump key */` |
|      65 | 8290 | `		if( pEntry->iType == HASHMAP_INT_NODE){` |
|      33 | 8291 | `			SyBlobFormat(&(*pOut),"[%qd] =>",pEntry->xKey.iKey);` |
|      17 | 8292 | `		}else{` |
|      48 | 8293 | `			SyBlobFormat(&(*pOut),"[%.*s] =>",` |
|      15 | 8294 | `				SyBlobLength(&pEntry->xKey.sKey),SyBlobData(&pEntry->xKey.sKey));` |
|       - | 8295 | `		}` |
|       - | 8296 | `#ifdef __WINNT__` |
|       3 | 8297 | `		SyBlobAppend(&(*pOut),"\r\n",sizeof("\r\n")-1);` |
|       - | 8298 | `#else` |
|      62 | 8299 | `		SyBlobAppend(&(*pOut),"\n",sizeof(char));` |
|       - | 8300 | `#endif` |
|       - | 8301 | `		/* Dump node value */` |
|      65 | 8302 | `		pObj = HashmapExtractNodeValue(pEntry);` |
|      65 | 8303 | `		isRef = 0;` |
|      65 | 8304 | `		if( pObj ){` |
|      65 | 8305 | `			if( pEntry->iFlags & HASHMAP_NODE_FOREIGN_OBJ ){` |
|       - | 8306 | `				/* Referenced object */` |
|     ! 0 | 8307 | `				isRef = 1;` |
|     ! 0 | 8308 | `			}` |
|      65 | 8309 | `			rc = PH7_MemObjDump(&(*pOut),pObj,ShowType,nTab+1,nDepth,isRef);` |
|      65 | 8310 | `			if( rc == SXERR_LIMIT ){` |
|     ! 0 | 8311 | `				break;` |
|       - | 8312 | `			}` |
|      31 | 8313 | `		}` |
|       - | 8314 | `		/* Point to the next entry */` |
|      65 | 8315 | `		n++;` |
|      65 | 8316 | `		pEntry = pEntry->pPrev; /* Reverse link */` |
|       3 | 8317 | `	}` |
|      29 | 8318 | `	return rc;` |
|       3 | 8319 | `}` |
|      22 | 8320 | `PH7_PRIVATE sxi32 PH7_HashmapDump(SyBlob *pOut,ph7_hashmap *pMap,int ShowType,int nTab,int nDepth)` |
|       2 | 8321 | `{` |
|       - | 8322 | `	sxi32 rc;` |
|       - | 8323 | `	int i;` |
|      24 | 8324 | `	if( nDepth > 31 ){` |
|       - | 8325 | `		static const char zInfinite[] = "Nesting limit reached: Infinite recursion?";` |
|       - | 8326 | `		/* Nesting limit reached */` |
|     ! 0 | 8327 | `		SyBlobAppend(&(*pOut),zInfinite,sizeof(zInfinite)-1);` |
|     ! 0 | 8328 | `		if( ShowType ){` |
|     ! 0 | 8329 | `			SyBlobAppend(&(*pOut),")",sizeof(char));` |
|     ! 0 | 8330 | `		}` |
|     ! 0 | 8331 | `		return SXERR_LIMIT;` |
|       - | 8332 | `	}` |
|      24 | 8333 | `	if( !ShowType ){` |
|      11 | 8334 | `		SyBlobAppend(&(*pOut),"Array(",sizeof("Array(")-1);` |
|       5 | 8335 | `	}` |
|       - | 8336 | `	/* Total entries */` |
|      24 | 8337 | `	SyBlobFormat(&(*pOut),"%u) {",pMap->nEntry);` |
|       - | 8338 | `#ifdef __WINNT__` |
|       2 | 8339 | `	SyBlobAppend(&(*pOut),"\r\n",sizeof("\r\n")-1);` |
|       - | 8340 | `#else` |
|      22 | 8341 | `	SyBlobAppend(&(*pOut),"\n",sizeof(char));` |
|       - | 8342 | `#endif` |
|      24 | 8343 | `	rc = PH7_HashmapDumpEntries(&(*pOut),pMap,ShowType,nTab,nDepth);` |
|      46 | 8344 | `	for( i = 0 ; i < nTab ; i++ ){` |
|      24 | 8345 | `		SyBlobAppend(&(*pOut)," ",sizeof(char));` |
|      13 | 8346 | `	}` |
|      24 | 8347 | `	SyBlobAppend(&(*pOut),"}",sizeof(char));` |
|      24 | 8348 | `	return rc;` |
|      13 | 8349 | `}` |
|       - | 8350 | `/*` |
|       - | 8351 | ` * Iterate throw hashmap entries and invoke the given callback [i.e: xWalk()] for each` |
|       - | 8352 | ` * retrieved entry.` |
|       - | 8353 | ` * Note that argument are passed to the callback by copy. That is,any modification to` |
|       - | 8354 | ` * the entry value in the callback body will not alter the real value.` |
|       - | 8355 | ` * If the callback wishes to abort processing [i.e: it's invocation] it must return` |
|       - | 8356 | ` * a value different from PH7_OK.` |
|       - | 8357 | ` * Refer to [ph7_array_walk()] for more information.` |
|       - | 8358 | ` */` |
|   32622 | 8359 | `PH7_PRIVATE sxi32 PH7_HashmapWalk(` |
|       - | 8360 | `	ph7_hashmap *pMap, /* Target hashmap */` |
|       - | 8361 | `	int (*xWalk)(ph7_value *,ph7_value *,void *), /* Walker callback */` |
|       - | 8362 | `	void *pUserData /* Last argument to xWalk() */` |
|       - | 8363 | `	)` |
|       5 | 8364 | `{` |
|       - | 8365 | `	ph7_hashmap_node *pEntry;` |
|       - | 8366 | `	ph7_value sKey,sValue;` |
|       - | 8367 | `	sxi32 rc;` |
|       - | 8368 | `	sxu32 n;` |
|       - | 8369 | `	/* Initialize walker parameter */` |
|   32627 | 8370 | `	rc = SXRET_OK;` |
|   32627 | 8371 | `	PH7_MemObjInit(pMap->pVm,&sKey);` |
|   32627 | 8372 | `	PH7_MemObjInit(pMap->pVm,&sValue);` |
|   32627 | 8373 | `	n = pMap->nEntry;` |
|   32627 | 8374 | `	pEntry = pMap->pFirst;` |
|       - | 8375 | `	/* Start the iteration process */` |
|   83874 | 8376 | `	for(;;){` |
|  167753 | 8377 | `		if( n < 1 ){` |
|   32627 | 8378 | `			break;` |
|       - | 8379 | `		}` |
|       - | 8380 | `		/* Extract a copy of the key and a copy the current value */` |
|  135131 | 8381 | `		PH7_HashmapExtractNodeKey(pEntry,&sKey);` |
|  135131 | 8382 | `		PH7_HashmapExtractNodeValue(pEntry,&sValue,FALSE);` |
|       - | 8383 | `		/* Invoke the user callback */` |
|  135131 | 8384 | `		rc = xWalk(&sKey,&sValue,pUserData);` |
|       - | 8385 | `		/* Release the copy of the key and the value */` |
|  135131 | 8386 | `		PH7_MemObjRelease(&sKey);` |
|  135131 | 8387 | `		PH7_MemObjRelease(&sValue);` |
|  135131 | 8388 | `		if( rc != PH7_OK ){` |
|       - | 8389 | `			/* Callback request an operation abort */` |
|     ! 0 | 8390 | `			return SXERR_ABORT;` |
|       - | 8391 | `		}` |
|       - | 8392 | `		/* Point to the next entry */` |
|  135131 | 8393 | `		pEntry = pEntry->pPrev; /* Reverse link */` |
|  135131 | 8394 | `		n--;` |
|       5 | 8395 | `	}` |
|       - | 8396 | `	/* All done */` |
|   32627 | 8397 | `	return SXRET_OK;` |
|   16316 | 8398 | `}` |
|       - | 8399 |  |
