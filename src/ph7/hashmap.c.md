# src/ph7/hashmap.c

<style>code, pre { background: none !important; white-space: pre !important; width: 100% !important; display: inline-block !important; } td { border: none !important; margin-top: 0 !important; margin-bottom: 0 !important; padding-top: 0 !important; padding-bottom: 0 !important; }</style>

Coverage: 3817/4271 lines (89.37%)

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
|   7403020 |   23 | `static sxu32 IntHash(sxi64 iKey)` |
|         5 |   24 | `{` |
|   7403025 |   25 | `	sxu64 uKey = (sxu64)iKey; /* unsigned mixing: shifting a negative key is UB */` |
|   7403025 |   26 | `	return (sxu32)(uKey ^ (uKey << 8) ^ (uKey >> 8));` |
|         5 |   27 | `}` |
|         - |   28 | `/*` |
|         - |   29 | ` * Default hash function for string/BLOB keys.` |
|         - |   30 | ` */` |
|    575134 |   31 | `static sxu32 BinHash(const void *pSrc,sxu32 nLen)` |
|         5 |   32 | `{` |
|    575139 |   33 | `	register unsigned char *zIn = (unsigned char *)pSrc;` |
|         - |   34 | `	unsigned char *zEnd;` |
|    575139 |   35 | `	sxu32 nH = 5381;` |
|    575139 |   36 | `	zEnd = &zIn[nLen];` |
|    656409 |   37 | `	for(;;){` |
|   1312823 |   38 | `		if( zIn >= zEnd ){ break; } nH = nH * 33 + zIn[0] ; zIn++;` |
|   1115169 |   39 | `		if( zIn >= zEnd ){ break; } nH = nH * 33 + zIn[0] ; zIn++;` |
|    998231 |   40 | `		if( zIn >= zEnd ){ break; } nH = nH * 33 + zIn[0] ; zIn++;` |
|    861613 |   41 | `		if( zIn >= zEnd ){ break; } nH = nH * 33 + zIn[0] ; zIn++;` |
|         5 |   42 | `	}` |
|    575139 |   43 | `	return nH;` |
|         5 |   44 | `}` |
|         - |   45 | `/*` |
|         - |   46 | ` * Return the total number of entries in a given hashmap.` |
|         - |   47 | ` * If bRecursive is set to TRUE then recurse on hashmap entries.` |
|         - |   48 | ` * Self-referential arrays are detected via the HASHMAP_COUNTING flag;` |
|         - |   49 | ` * when a cycle is found the nested array is skipped and *pCycleDetected` |
|         - |   50 | ` * is set to TRUE so the caller can emit a warning.` |
|         - |   51 | ` */` |
|      1252 |   52 | `static sxi64 HashmapCount(ph7_hashmap *pMap,int bRecursive,int *pCycleDetected)` |
|         5 |   53 | `{` |
|      1257 |   54 | `	sxi64 iCount = 0;` |
|      1257 |   55 | `	if( !bRecursive ){` |
|      1083 |   56 | `		iCount = pMap->nEntry;` |
|       544 |   57 | `	}else{` |
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
|      1257 |   92 | `	return iCount;` |
|         5 |   93 | `}` |
|         - |   94 | `/*` |
|         - |   95 | ` * Allocate a new hashmap node with a 64-bit integer key.` |
|         - |   96 | ` * If something goes wrong [i.e: out of memory],this function return NULL.` |
|         - |   97 | ` * Otherwise a fresh [ph7_hashmap_node] instance is returned.` |
|         - |   98 | ` */` |
|   3105860 |   99 | `static ph7_hashmap_node * HashmapNewIntNode(ph7_hashmap *pMap,sxi64 iKey,sxu32 nHash,sxu32 nValIdx)` |
|         5 |  100 | `{` |
|         - |  101 | `	ph7_hashmap_node *pNode;` |
|         - |  102 | `	/* Allocate a new node */` |
|   3105865 |  103 | `	pNode = (ph7_hashmap_node *)SyMemBackendPoolAlloc(&pMap->pVm->sAllocator,sizeof(ph7_hashmap_node));` |
|   3105865 |  104 | `	if( pNode == 0 ){` |
|       ! 0 |  105 | `		return 0;` |
|         - |  106 | `	}` |
|         - |  107 | `	/* Zero the stucture */` |
|   3105865 |  108 | `	SyZero(pNode,sizeof(ph7_hashmap_node));` |
|         - |  109 | `	/* Fill in the structure */` |
|   3105865 |  110 | `	pNode->pMap  = &(*pMap);` |
|   3105865 |  111 | `	pNode->iType = HASHMAP_INT_NODE;` |
|   3105865 |  112 | `	pNode->nHash = nHash;` |
|   3105865 |  113 | `	pNode->xKey.iKey = iKey;` |
|   3105865 |  114 | `	pNode->nValIdx  = nValIdx;` |
|   3105865 |  115 | `	return pNode;` |
|   1552935 |  116 | `}` |
|         - |  117 | `/*` |
|         - |  118 | ` * Allocate a new hashmap node with a BLOB key.` |
|         - |  119 | ` * If something goes wrong [i.e: out of memory],this function return NULL.` |
|         - |  120 | ` * Otherwise a fresh [ph7_hashmap_node] instance is returned.` |
|         - |  121 | ` */` |
|    238322 |  122 | `static ph7_hashmap_node * HashmapNewBlobNode(ph7_hashmap *pMap,const void *pKey,sxu32 nKeyLen,sxu32 nHash,sxu32 nValIdx)` |
|         5 |  123 | `{` |
|         - |  124 | `	ph7_hashmap_node *pNode;` |
|         - |  125 | `	/* Allocate a new node */` |
|    238327 |  126 | `	pNode = (ph7_hashmap_node *)SyMemBackendPoolAlloc(&pMap->pVm->sAllocator,sizeof(ph7_hashmap_node));` |
|    238327 |  127 | `	if( pNode == 0 ){` |
|       ! 0 |  128 | `		return 0;` |
|         - |  129 | `	}` |
|         - |  130 | `	/* Zero the stucture */` |
|    238327 |  131 | `	SyZero(pNode,sizeof(ph7_hashmap_node));` |
|         - |  132 | `	/* Fill in the structure */` |
|    238327 |  133 | `	pNode->pMap  = &(*pMap);` |
|    238327 |  134 | `	pNode->iType = HASHMAP_BLOB_NODE;` |
|    238327 |  135 | `	pNode->nHash = nHash;` |
|    238327 |  136 | `	SyBlobInit(&pNode->xKey.sKey,&pMap->pVm->sAllocator);` |
|    238327 |  137 | `	SyBlobAppend(&pNode->xKey.sKey,pKey,nKeyLen);` |
|    238327 |  138 | `	pNode->nValIdx = nValIdx;` |
|    238327 |  139 | `	return pNode;` |
|    119166 |  140 | `}` |
|         - |  141 | `/*` |
|         - |  142 | ` * link a hashmap node to the given bucket index (last argument to this function).` |
|         - |  143 | ` */` |
|   3344182 |  144 | `static void HashmapNodeLink(ph7_hashmap *pMap,ph7_hashmap_node *pNode,sxu32 nBucketIdx)` |
|         5 |  145 | `{` |
|         - |  146 | `	/* Link */` |
|   3344187 |  147 | `	if( pMap->apBucket[nBucketIdx] != 0 ){` |
|   2897053 |  148 | `		pNode->pNextCollide = pMap->apBucket[nBucketIdx];` |
|   2897053 |  149 | `		pMap->apBucket[nBucketIdx]->pPrevCollide = pNode;` |
|   1448524 |  150 | `	}` |
|   3344187 |  151 | `	pMap->apBucket[nBucketIdx] = pNode;` |
|         - |  152 | `	/* Link to the map list */` |
|   3344187 |  153 | `	if( pMap->pFirst == 0 ){` |
|     86067 |  154 | `		pMap->pFirst = pMap->pLast = pNode;` |
|         - |  155 | `		/* Point to the first inserted node */` |
|     86067 |  156 | `		pMap->pCur = pNode;` |
|     43036 |  157 | `	}else{` |
|   3258125 |  158 | `		MACRO_LD_PUSH(pMap->pLast,pNode);` |
|         - |  159 | `	}` |
|   3344187 |  160 | `	++pMap->nEntry;` |
|   3344187 |  161 | `}` |
|         - |  162 | `/*` |
|         - |  163 | ` * Unlink a node from the hashmap.` |
|         - |  164 | ` * If the node count reaches zero then release the whole hash-bucket.` |
|         - |  165 | ` */` |
|      7420 |  166 | `PH7_PRIVATE void PH7_HashmapUnlinkNode(ph7_hashmap_node *pNode,int bRestore)` |
|         5 |  167 | `{` |
|      7425 |  168 | `	ph7_hashmap *pMap = pNode->pMap;` |
|      7425 |  169 | `	ph7_vm *pVm = pMap->pVm;` |
|         - |  170 | `	/* Unlink from the corresponding bucket */` |
|      7425 |  171 | `	if( pNode->pPrevCollide == 0 ){` |
|      6957 |  172 | `		pMap->apBucket[pNode->nHash & (pMap->nSize - 1)] = pNode->pNextCollide;` |
|      3481 |  173 | `	}else{` |
|       470 |  174 | `		pNode->pPrevCollide->pNextCollide = pNode->pNextCollide;` |
|         - |  175 | `	}` |
|      7425 |  176 | `	if( pNode->pNextCollide ){` |
|      4449 |  177 | `		pNode->pNextCollide->pPrevCollide = pNode->pPrevCollide;` |
|      2223 |  178 | `	}` |
|      7425 |  179 | `	if( pMap->pFirst == pNode ){` |
|       131 |  180 | `		pMap->pFirst = pNode->pPrev;` |
|        63 |  181 | `	}` |
|      7425 |  182 | `	if( pMap->pCur == pNode ){` |
|         - |  183 | `		/* Advance the node cursor */` |
|       133 |  184 | `		pMap->pCur = pMap->pCur->pPrev; /* Reverse link */` |
|        64 |  185 | `	}` |
|         - |  186 | `	/* Unlink from the map list */` |
|      7425 |  187 | `	MACRO_LD_REMOVE(pMap->pLast,pNode);` |
|      7425 |  188 | `	if( bRestore ){` |
|         - |  189 | `		/* Remove the ph7_value associated with this node from the reference table */` |
|       107 |  190 | `		PH7_VmRefObjRemove(pVm,pNode->nValIdx,0,pNode);` |
|         - |  191 | `		/* Restore to the freelist */` |
|       107 |  192 | `		if( (pNode->iFlags & HASHMAP_NODE_FOREIGN_OBJ) == 0 ){` |
|       107 |  193 | `			PH7_VmUnsetMemObj(pVm,pNode->nValIdx,FALSE);` |
|        51 |  194 | `		}` |
|        51 |  195 | `	}` |
|      7425 |  196 | `	if( pNode->iType == HASHMAP_BLOB_NODE ){` |
|      7288 |  197 | `		SyBlobRelease(&pNode->xKey.sKey);` |
|      3642 |  198 | `	}` |
|      7425 |  199 | `	SyMemBackendPoolFree(&pVm->sAllocator,pNode);` |
|      7425 |  200 | `	pMap->nEntry--;` |
|      7425 |  201 | `	if( pMap->nEntry < 1 && pMap != pVm->pGlobal ){` |
|         - |  202 | `		/* Free the hash-bucket */` |
|        75 |  203 | `		SyMemBackendFree(&pVm->sAllocator,pMap->apBucket);` |
|        75 |  204 | `		pMap->apBucket = 0;` |
|        75 |  205 | `		pMap->nSize = 0;` |
|        75 |  206 | `		pMap->pFirst = pMap->pLast = pMap->pCur = 0;` |
|        35 |  207 | `	}` |
|      7425 |  208 | `}` |
|         - |  209 | `#define HASHMAP_FILL_FACTOR 3` |
|         - |  210 | `/*` |
|         - |  211 | ` * Grow the hash-table and rehash all entries.` |
|         - |  212 | ` */` |
|   3344182 |  213 | `static sxi32 HashmapGrowBucket(ph7_hashmap *pMap)` |
|         5 |  214 | `{` |
|   3344187 |  215 | `	if( pMap->nEntry >= pMap->nSize * HASHMAP_FILL_FACTOR ){` |
|     90867 |  216 | `		ph7_hashmap_node **apOld = pMap->apBucket;` |
|         - |  217 | `		ph7_hashmap_node *pEntry,**apNew;` |
|     90867 |  218 | `		sxu32 nNew = pMap->nSize << 1;` |
|         - |  219 | `		sxu32 nBucket;` |
|         - |  220 | `		sxu32 n;` |
|     90867 |  221 | `		if( nNew < 1 ){` |
|     86067 |  222 | `			nNew = 16;` |
|     43031 |  223 | `		}` |
|         - |  224 | `		/* Allocate a new bucket */` |
|     90867 |  225 | `		apNew = (ph7_hashmap_node **)SyMemBackendAlloc(&pMap->pVm->sAllocator,nNew * sizeof(ph7_hashmap_node *));` |
|     90867 |  226 | `		if( apNew == 0 ){` |
|       ! 0 |  227 | `			if( pMap->nSize < 1 ){` |
|       ! 0 |  228 | `				return SXERR_MEM; /* Fatal */` |
|         - |  229 | `			}` |
|         - |  230 | `			/* Not so fatal here,simply a performance hit */` |
|       ! 0 |  231 | `			return SXRET_OK;` |
|         - |  232 | `		}` |
|         - |  233 | `		/* Zero the table */` |
|     90867 |  234 | `		SyZero((void *)apNew,nNew * sizeof(ph7_hashmap_node *));` |
|         - |  235 | `		/* Reflect the change */` |
|     90867 |  236 | `		pMap->apBucket = apNew;` |
|     90867 |  237 | `		pMap->nSize = nNew;` |
|     90867 |  238 | `		if( apOld == 0 ){` |
|         - |  239 | `			/* First allocated table [i.e: no entry],return immediately */` |
|     86067 |  240 | `			return SXRET_OK;` |
|         - |  241 | `		}` |
|         - |  242 | `		/* Rehash old entries */` |
|      4805 |  243 | `		pEntry = pMap->pFirst;` |
|      4805 |  244 | `		n = 0;` |
|   2090160 |  245 | `		for( ;; ){` |
|   4180325 |  246 | `			if( n >= pMap->nEntry ){` |
|      4805 |  247 | `				break;` |
|         - |  248 | `			}` |
|         - |  249 | `			/* Clear the old collision link */` |
|   4175525 |  250 | `			pEntry->pNextCollide = pEntry->pPrevCollide = 0;` |
|         - |  251 | `			/* Link to the new bucket */` |
|   4175525 |  252 | `			nBucket = pEntry->nHash & (nNew - 1);` |
|   4175525 |  253 | `			if( pMap->apBucket[nBucket] != 0 ){` |
|   3577221 |  254 | `				pEntry->pNextCollide = pMap->apBucket[nBucket];` |
|   3577221 |  255 | `				pMap->apBucket[nBucket]->pPrevCollide = pEntry;` |
|   1788608 |  256 | `			}` |
|   4175525 |  257 | `			pMap->apBucket[nBucket] = pEntry;` |
|         - |  258 | `			/* Point to the next entry */` |
|   4175525 |  259 | `			pEntry = pEntry->pPrev; /* Reverse link */` |
|   4175525 |  260 | `			n++;` |
|         5 |  261 | `		}` |
|         - |  262 | `		/* Free the old table */` |
|      4805 |  263 | `		SyMemBackendFree(&pMap->pVm->sAllocator,(void *)apOld);` |
|      2400 |  264 | `	}` |
|   3258125 |  265 | `	return SXRET_OK;` |
|   1672096 |  266 | `}` |
|         - |  267 | `/*` |
|         - |  268 | ` * Insert a 64-bit integer key and it's associated value (if any) in the given` |
|         - |  269 | ` * hashmap.` |
|         - |  270 | ` */` |
|   3105860 |  271 | `static sxi32 HashmapInsertIntKey(ph7_hashmap *pMap,sxi64 iKey,ph7_value *pValue,sxu32 nRefIdx,int isForeign)` |
|         5 |  272 | `{` |
|         - |  273 | `	ph7_hashmap_node *pNode;` |
|         - |  274 | `	sxu32 nIdx;` |
|         - |  275 | `	sxu32 nHash;` |
|         - |  276 | `	sxi32 rc;` |
|   3105865 |  277 | `	if( !isForeign ){` |
|         - |  278 | `		ph7_value *pObj;` |
|         - |  279 | `		ph7_value sSafeVal;` |
|         - |  280 | `		/* Snapshot the source BEFORE reserving: PH7_ReserveMemObj can grow (move)` |
|         - |  281 | `		 * pVm->aMemObj, which would dangle pValue when it points into the pool` |
|         - |  282 | `		 * (e.g. get_defined_vars/func_get_args/get_class_vars/get_object_vars pass` |
|         - |  283 | `		 * a pool slot). A shallow copy is a safe PH7_MemObjStore source — the` |
|         - |  284 | `		 * referent and the heap-resident blob data survive the move; only the` |
|         - |  285 | `		 * ph7_value struct relocates (same sSafeVal idiom used by PH7_HashmapDup). */` |
|   3105827 |  286 | `		if( pValue ){` |
|   3105825 |  287 | `			sSafeVal = *pValue;` |
|   3105825 |  288 | `			pValue = &sSafeVal;` |
|   1552910 |  289 | `		}` |
|         - |  290 | `		/* Reserve a ph7_value for the value */` |
|   3105827 |  291 | `		pObj = PH7_ReserveMemObj(pMap->pVm);` |
|   3105827 |  292 | `		if( pObj == 0 ){` |
|       ! 0 |  293 | `			return SXERR_MEM;` |
|         - |  294 | `		}` |
|   3105827 |  295 | `		if( pValue ){` |
|         - |  296 | `			/* Duplicate the value */` |
|   3105825 |  297 | `			PH7_MemObjStore(pValue,pObj);` |
|   1552910 |  298 | `		}` |
|   3105827 |  299 | `		nIdx = pObj->nIdx;` |
|   1552916 |  300 | `	}else{` |
|        39 |  301 | `		nIdx = nRefIdx;` |
|         - |  302 | `	}` |
|         - |  303 | `	/* Hash the key */` |
|   3105865 |  304 | `	nHash = pMap->xIntHash(iKey);` |
|         - |  305 | `	/* Allocate a new int node */` |
|   3105865 |  306 | `	pNode = HashmapNewIntNode(&(*pMap),iKey,nHash,nIdx);` |
|   3105865 |  307 | `	if( pNode == 0 ){` |
|       ! 0 |  308 | `		return SXERR_MEM;` |
|         - |  309 | `	}` |
|   3105865 |  310 | `	if( isForeign ){` |
|         - |  311 | `		/* Mark as a foregin entry */` |
|        39 |  312 | `		pNode->iFlags \|= HASHMAP_NODE_FOREIGN_OBJ;` |
|        19 |  313 | `	}` |
|         - |  314 | `	/* Make sure the bucket is big enough to hold the new entry */` |
|   3105865 |  315 | `	rc = HashmapGrowBucket(&(*pMap));` |
|   3105865 |  316 | `	if( rc != SXRET_OK ){` |
|       ! 0 |  317 | `		SyMemBackendPoolFree(&pMap->pVm->sAllocator,pNode);` |
|       ! 0 |  318 | `		return rc;` |
|         - |  319 | `	}` |
|         - |  320 | `	/* Perform the insertion */` |
|   3105865 |  321 | `	HashmapNodeLink(&(*pMap),pNode,nHash & (pMap->nSize - 1));` |
|         - |  322 | `	/* Install in the reference table */` |
|   3105865 |  323 | `	PH7_VmRefObjInstall(pMap->pVm,nIdx,0,pNode,0);` |
|         - |  324 | `	/* All done */` |
|   3105865 |  325 | `	return SXRET_OK;` |
|   1552935 |  326 | `}` |
|         - |  327 | `/*` |
|         - |  328 | ` * Insert a BLOB key and it's associated value (if any) in the given` |
|         - |  329 | ` * hashmap.` |
|         - |  330 | ` */` |
|    238322 |  331 | `static sxi32 HashmapInsertBlobKey(ph7_hashmap *pMap,const void *pKey,sxu32 nKeyLen,ph7_value *pValue,sxu32 nRefIdx,int isForeign)` |
|         5 |  332 | `{` |
|         - |  333 | `	ph7_hashmap_node *pNode;` |
|         - |  334 | `	sxu32 nHash;` |
|         - |  335 | `	sxu32 nIdx;` |
|         - |  336 | `	sxi32 rc;` |
|    238327 |  337 | `	if( !isForeign ){` |
|         - |  338 | `		ph7_value *pObj;` |
|         - |  339 | `		ph7_value sSafeVal;` |
|         - |  340 | `		/* Snapshot the source BEFORE reserving: PH7_ReserveMemObj can grow (move)` |
|         - |  341 | `		 * pVm->aMemObj, which would dangle pValue when it points into the pool` |
|         - |  342 | `		 * (e.g. get_defined_vars/func_get_args/get_class_vars/get_object_vars pass` |
|         - |  343 | `		 * a pool slot). A shallow copy is a safe PH7_MemObjStore source — the` |
|         - |  344 | `		 * referent and the heap-resident blob data survive the move; only the` |
|         - |  345 | `		 * ph7_value struct relocates (same sSafeVal idiom used by PH7_HashmapDup). */` |
|    191891 |  346 | `		if( pValue ){` |
|    191601 |  347 | `			sSafeVal = *pValue;` |
|    191601 |  348 | `			pValue = &sSafeVal;` |
|     95798 |  349 | `		}` |
|         - |  350 | `		/* Reserve a ph7_value for the value */` |
|    191891 |  351 | `		pObj = PH7_ReserveMemObj(pMap->pVm);` |
|    191891 |  352 | `		if( pObj == 0 ){` |
|       ! 0 |  353 | `			return SXERR_MEM;` |
|         - |  354 | `		}` |
|    191891 |  355 | `		if( pValue ){` |
|         - |  356 | `			/* Duplicate the value */` |
|    191601 |  357 | `			PH7_MemObjStore(pValue,pObj);` |
|     95798 |  358 | `		}` |
|    191891 |  359 | `		nIdx = pObj->nIdx;` |
|     95948 |  360 | `	}else{` |
|     46441 |  361 | `		nIdx = nRefIdx;` |
|         - |  362 | `	}` |
|         - |  363 | `	/* Hash the key */` |
|    238327 |  364 | `	nHash = pMap->xBlobHash(pKey,nKeyLen);` |
|         - |  365 | `	/* Allocate a new blob node */` |
|    238327 |  366 | `	pNode = HashmapNewBlobNode(&(*pMap),pKey,nKeyLen,nHash,nIdx);` |
|    238327 |  367 | `	if( pNode == 0 ){` |
|       ! 0 |  368 | `		return SXERR_MEM;` |
|         - |  369 | `	}` |
|    238327 |  370 | `	if( isForeign ){` |
|         - |  371 | `		/* Mark as a foregin entry */` |
|     46441 |  372 | `		pNode->iFlags \|= HASHMAP_NODE_FOREIGN_OBJ;` |
|     23218 |  373 | `	}` |
|         - |  374 | `	/* Make sure the bucket is big enough to hold the new entry */` |
|    238327 |  375 | `	rc = HashmapGrowBucket(&(*pMap));` |
|    238327 |  376 | `	if( rc != SXRET_OK ){` |
|       ! 0 |  377 | `		SyMemBackendPoolFree(&pMap->pVm->sAllocator,pNode);` |
|       ! 0 |  378 | `		return rc;` |
|         - |  379 | `	}` |
|         - |  380 | `	/* Perform the insertion */` |
|    238327 |  381 | `	HashmapNodeLink(&(*pMap),pNode,nHash & (pMap->nSize - 1));` |
|         - |  382 | `	/* Install in the reference table */` |
|    238327 |  383 | `	PH7_VmRefObjInstall(pMap->pVm,nIdx,0,pNode,0);` |
|         - |  384 | `	/* All done */` |
|    238327 |  385 | `	return SXRET_OK;` |
|    119166 |  386 | `}` |
|         - |  387 | `/*` |
|         - |  388 | ` * Check if a given 64-bit integer key exists in the given hashmap.` |
|         - |  389 | ` * Write a pointer to the target node on success. Otherwise` |
|         - |  390 | ` * SXERR_NOTFOUND is returned on failure.` |
|         - |  391 | ` */` |
|   4284246 |  392 | `static sxi32 HashmapLookupIntKey(` |
|         - |  393 | `	ph7_hashmap *pMap,         /* Target hashmap */` |
|         - |  394 | `	sxi64 iKey,                /* lookup key */` |
|         - |  395 | `	ph7_hashmap_node **ppNode  /* OUT: target node on success */` |
|         - |  396 | `	)` |
|         5 |  397 | `{` |
|         - |  398 | `	ph7_hashmap_node *pNode;` |
|         - |  399 | `	sxu32 nHash;` |
|   4284251 |  400 | `	if( pMap->nEntry < 1 ){` |
|         - |  401 | `		/* Don't bother hashing,there is no entry anyway */` |
|       569 |  402 | `		return SXERR_NOTFOUND;` |
|         - |  403 | `	}` |
|         - |  404 | `	/* Hash the key first */` |
|   4283687 |  405 | `	nHash = pMap->xIntHash(iKey);` |
|         - |  406 | `	/* Point to the appropriate bucket */` |
|   4283687 |  407 | `	pNode = pMap->apBucket[nHash & (pMap->nSize - 1)];` |
|         - |  408 | `	/* Perform the lookup */` |
| 110562370 |  409 | `	for(;;){` |
| 221124745 |  410 | `		if( pNode == 0 ){` |
|   4281099 |  411 | `			break;` |
|         - |  412 | `		}` |
| 216843646 |  413 | `		if( pNode->iType == HASHMAP_INT_NODE` |
| 216840636 |  414 | `			&& pNode->nHash == nHash` |
| 108420112 |  415 | `			&& pNode->xKey.iKey == iKey ){` |
|         - |  416 | `				/* Node found */` |
|      2593 |  417 | `				if( ppNode ){` |
|      2575 |  418 | `					*ppNode = pNode;` |
|      1285 |  419 | `				}` |
|      2593 |  420 | `				return SXRET_OK;` |
|         - |  421 | `		}` |
|         - |  422 | `		/* Follow the collision link */` |
| 216841059 |  423 | `		pNode = pNode->pNextCollide;` |
|         1 |  424 | `	}` |
|         - |  425 | `	/* No such entry */` |
|   4281099 |  426 | `	return SXERR_NOTFOUND;` |
|   2142128 |  427 | `}` |
|         - |  428 | `/*` |
|         - |  429 | ` * Check if a given BLOB key exists in the given hashmap.` |
|         - |  430 | ` * Write a pointer to the target node on success. Otherwise` |
|         - |  431 | ` * SXERR_NOTFOUND is returned on failure.` |
|         - |  432 | ` */` |
|    370398 |  433 | `static sxi32 HashmapLookupBlobKey(` |
|         - |  434 | `	ph7_hashmap *pMap,          /* Target hashmap */` |
|         - |  435 | `	const void *pKey,           /* Lookup key */` |
|         - |  436 | `	sxu32 nKeyLen,              /* Key length in bytes */` |
|         - |  437 | `	ph7_hashmap_node **ppNode   /* OUT: target node on success */` |
|         - |  438 | `	)` |
|         5 |  439 | `{` |
|         - |  440 | `	ph7_hashmap_node *pNode;` |
|         - |  441 | `	sxu32 nHash;` |
|    370403 |  442 | `	if( pMap->nEntry < 1 ){` |
|         - |  443 | `		/* Don't bother hashing,there is no entry anyway */` |
|     33591 |  444 | `		return SXERR_NOTFOUND;` |
|         - |  445 | `	}` |
|         - |  446 | `	/* Hash the key first */` |
|    336817 |  447 | `	nHash = pMap->xBlobHash(pKey,nKeyLen);` |
|         - |  448 | `	/* Point to the appropriate bucket */` |
|    336817 |  449 | `	pNode = pMap->apBucket[nHash & (pMap->nSize - 1)];` |
|         - |  450 | `	/* Perform the lookup */` |
|    276708 |  451 | `	for(;;){` |
|    553421 |  452 | `		if( pNode == 0 ){` |
|    279493 |  453 | `			break;` |
|         - |  454 | `		}` |
|    273928 |  455 | `		if( pNode->iType == HASHMAP_BLOB_NODE` |
|    272423 |  456 | `			&& pNode->nHash == nHash` |
|    164168 |  457 | `			&& SyBlobLength(&pNode->xKey.sKey) == nKeyLen` |
|     57423 |  458 | `			&& SyMemcmp(SyBlobData(&pNode->xKey.sKey),pKey,nKeyLen) == 0 ){` |
|         - |  459 | `				/* Node found */` |
|     57329 |  460 | `				if( ppNode ){` |
|     57301 |  461 | `					*ppNode = pNode;` |
|     28648 |  462 | `				}` |
|     57329 |  463 | `				return SXRET_OK;` |
|         - |  464 | `		}` |
|         - |  465 | `		/* Follow the collision link */` |
|    216609 |  466 | `		pNode = pNode->pNextCollide;` |
|         5 |  467 | `	}` |
|         - |  468 | `	/* No such entry */` |
|    279493 |  469 | `	return SXERR_NOTFOUND;` |
|    185204 |  470 | `}` |
|         - |  471 | `/*` |
|         - |  472 | ` * Check if the given BLOB key looks like a decimal number.` |
|         - |  473 | ` * Retrurn TRUE on success.FALSE otherwise.` |
|         - |  474 | ` */` |
|    370532 |  475 | `static int HashmapIsIntKey(SyBlob *pKey)` |
|         5 |  476 | `{` |
|    370537 |  477 | `	const char *zIn  = (const char *)SyBlobData(pKey);` |
|    370537 |  478 | `	const char *zEnd = &zIn[SyBlobLength(pKey)];` |
|         - |  479 | `	const char *zDigit;` |
|    370537 |  480 | `	int isNeg = FALSE, nDigit;` |
|    370537 |  481 | `	if( zIn >= zEnd ){` |
|       ! 0 |  482 | `		return FALSE;` |
|         - |  483 | `	}` |
|    370537 |  484 | `	if( (int)(zEnd-zIn) > 1 && zIn[0] == '0' ){` |
|         - |  485 | `		/* Octal not decimal number */` |
|         5 |  486 | `		return FALSE;` |
|         - |  487 | `	}` |
|    370533 |  488 | `	if( (zIn[0] == '-' \|\| zIn[0] == '+') && &zIn[1] < zEnd ){` |
|         5 |  489 | `		isNeg = (zIn[0] == '-');` |
|         5 |  490 | `		zIn++;` |
|         2 |  491 | `	}` |
|    370533 |  492 | `	zDigit = zIn;` |
|    185696 |  493 | `	for(;;){` |
|    371397 |  494 | `		if( zIn >= zEnd ){` |
|       249 |  495 | `			break;` |
|         - |  496 | `		}` |
|    371149 |  497 | `		if( (unsigned char)zIn[0] >= 0xc0 /* UTF-8 stream */  \|\| !SyisDigit(zIn[0]) ){` |
|         - |  498 | `			/* Key does not look like a decimal number */` |
|    370285 |  499 | `			return FALSE;` |
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
|    185271 |  517 | `}` |
|         - |  518 | `/*` |
|         - |  519 | ` * Check if a given key exists in the given hashmap.` |
|         - |  520 | ` * Write a pointer to the target node on success.` |
|         - |  521 | ` * Otherwise SXERR_NOTFOUND is returned on failure.` |
|         - |  522 | ` */` |
|    134656 |  523 | `static sxi32 HashmapLookup(` |
|         - |  524 | `	ph7_hashmap *pMap,          /* Target hashmap */` |
|         - |  525 | `	ph7_value *pKey,            /* Lookup key */` |
|         - |  526 | `	ph7_hashmap_node **ppNode   /* OUT: target node on success */` |
|         - |  527 | `	)` |
|         5 |  528 | `{` |
|    134661 |  529 | `	ph7_hashmap_node *pNode = 0; /* cc -O6 warning */` |
|         - |  530 | `	sxi32 rc;` |
|    134661 |  531 | `	if( pKey->iFlags & (MEMOBJ_STRING\|MEMOBJ_HASHMAP\|MEMOBJ_OBJ\|MEMOBJ_RES) ){` |
|    132299 |  532 | `		if( (pKey->iFlags & MEMOBJ_STRING) == 0 ){` |
|         - |  533 | `			/* Force a string cast */` |
|       ! 0 |  534 | `			PH7_MemObjToString(&(*pKey));` |
|       ! 0 |  535 | `		}` |
|    132299 |  536 | `		if( SyBlobLength(&pKey->sBlob) > 0 && !HashmapIsIntKey(&pKey->sBlob) ){` |
|         - |  537 | `			/* Perform a blob lookup */` |
|    132279 |  538 | `			rc = HashmapLookupBlobKey(&(*pMap),SyBlobData(&pKey->sBlob),SyBlobLength(&pKey->sBlob),&pNode);` |
|    132279 |  539 | `			goto result;` |
|         - |  540 | `		}` |
|        10 |  541 | `	}` |
|         - |  542 | `	/* Perform an int lookup */` |
|      2387 |  543 | `	if((pKey->iFlags & MEMOBJ_INT) == 0 ){` |
|         - |  544 | `		/* Force an integer cast */` |
|        35 |  545 | `		PH7_MemObjToInteger(pKey);` |
|        17 |  546 | `	}` |
|         - |  547 | `	/* Perform an int lookup */` |
|      2387 |  548 | `	rc = HashmapLookupIntKey(&(*pMap),pKey->x.iVal,&pNode);` |
|     67328 |  549 | `result:` |
|    134661 |  550 | `	if( rc == SXRET_OK ){` |
|         - |  551 | `		/* Node found */` |
|     59271 |  552 | `		if( ppNode ){` |
|     59225 |  553 | `			*ppNode = pNode;` |
|     29610 |  554 | `		}` |
|     59271 |  555 | `		return SXRET_OK;` |
|         - |  556 | `	}` |
|         - |  557 | `	/* No such entry */` |
|     75395 |  558 | `	return SXERR_NOTFOUND;` |
|     67333 |  559 | `}` |
|         - |  560 | `/*` |
|         - |  561 | ` * Advance the auto-index after a successful insertion of int key iKey.` |
|         - |  562 | ` * Mirrors Zend's nNextFreeElement: saturates at PHP_INT_MAX (incrementing` |
|         - |  563 | ` * past it is signed overflow); the occupied-slot case errors at append time` |
|         - |  564 | ` * via HashmapAppendIndexBusy.` |
|         - |  565 | ` */` |
|   2140944 |  566 | `static void HashmapAdvanceAutoIndex(ph7_hashmap *pMap,sxi64 iKey)` |
|         5 |  567 | `{` |
|   2140949 |  568 | `	if( iKey >= pMap->iNextIdx ){` |
|   2140689 |  569 | `		pMap->iNextIdx = iKey < SXI64_HIGH ? iKey + 1 : SXI64_HIGH;` |
|         - |  570 | `		/* Make sure the automatic index is not reserved */` |
|   2140689 |  571 | `		while( pMap->iNextIdx < SXI64_HIGH && SXRET_OK == HashmapLookupIntKey(&(*pMap),pMap->iNextIdx,0) ){` |
|       ! 0 |  572 | `			pMap->iNextIdx++;` |
|       ! 0 |  573 | `		}` |
|   1070342 |  574 | `	}` |
|   2140949 |  575 | `}` |
|         - |  576 | `/*` |
|         - |  577 | `` * TRUE when an append (`$a[] = v`) cannot proceed because the saturated`` |
|         - |  578 | ` * auto-index slot (PHP_INT_MAX) is already occupied. Throws php's catchable` |
|         - |  579 | ` * Error and stores the rc the insert function must return (PH7_EXCEPTION,` |
|         - |  580 | ` * or PH7_ABORT when the Error class itself cannot be built).` |
|         - |  581 | ` */` |
|    964576 |  582 | `static sxi32 HashmapAppendIndexBusy(ph7_hashmap *pMap,sxi32 *pRc)` |
|         5 |  583 | `{` |
|    964581 |  584 | `	if( pMap->iNextIdx == SXI64_HIGH && SXRET_OK == HashmapLookupIntKey(&(*pMap),pMap->iNextIdx,0) ){` |
|         7 |  585 | `		*pRc = PH7_VmThrowArrayNextIndexError(pMap->pVm);` |
|         7 |  586 | `		return TRUE;` |
|         - |  587 | `	}` |
|    964575 |  588 | `	return FALSE;` |
|    482293 |  589 | `}` |
|         - |  590 | `/*` |
|         - |  591 | ` * Insert a given key and it's associated value (if any) in the given` |
|         - |  592 | ` * hashmap.` |
|         - |  593 | ` * If a node with the given key already exists in the database` |
|         - |  594 | ` * then this function overwrite the old value.` |
|         - |  595 | ` */` |
|   3297146 |  596 | `static sxi32 HashmapInsert(` |
|         - |  597 | `	ph7_hashmap *pMap, /* Target hashmap */` |
|         - |  598 | `	ph7_value *pKey,   /* Lookup key  */` |
|         - |  599 | `	ph7_value *pVal    /* Node value */` |
|         - |  600 | `	)` |
|         5 |  601 | `{` |
|   3297151 |  602 | `	ph7_hashmap_node *pNode = 0;` |
|   3297151 |  603 | `	sxi32 rc = SXRET_OK;` |
|   3297151 |  604 | `	if( pKey && pKey->iFlags & (MEMOBJ_STRING\|MEMOBJ_HASHMAP\|MEMOBJ_OBJ\|MEMOBJ_RES) ){` |
|    195295 |  605 | `		if( (pKey->iFlags & MEMOBJ_STRING) == 0 ){` |
|         - |  606 | `			/* Force a string cast */` |
|         3 |  607 | `			PH7_MemObjToString(&(*pKey));` |
|         1 |  608 | `		}` |
|    195295 |  609 | `		if( SyBlobLength(&pKey->sBlob) < 1 \|\| HashmapIsIntKey(&pKey->sBlob) ){` |
|      3721 |  610 | `			if(SyBlobLength(&pKey->sBlob) < 1){` |
|         - |  611 | `				/* Automatic index assign */` |
|      3495 |  612 | `				pKey = 0;` |
|      1745 |  613 | `			}` |
|      3721 |  614 | `			goto IntKey;` |
|         - |  615 | `		}` |
|    287366 |  616 | `		if( SXRET_OK == HashmapLookupBlobKey(&(*pMap),SyBlobData(&pKey->sBlob),` |
|     95787 |  617 | `			SyBlobLength(&pKey->sBlob),&pNode) ){` |
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
|    191211 |  631 | `		if( pMap == pMap->pVm->pGlobal ){` |
|         - |  632 | `			/* php 8.1: writing a new key into $GLOBALS creates a real global` |
|         - |  633 | `			 * variable ($GLOBALS stays a live view of the symbol table). */` |
|       127 |  634 | `			if( SyBlobLength(&pKey->sBlob) < 1 ){` |
|         - |  635 | `				/* Pathological empty name: keep the legacy diagnostic */` |
|       ! 0 |  636 | `				PH7_VmThrowError(pMap->pVm,0,PH7_CTX_NOTICE,"$GLOBALS is a read-only array,insertion is forbidden");` |
|       ! 0 |  637 | `				return SXRET_OK;` |
|         - |  638 | `			}` |
|       190 |  639 | `			return PH7_VmInstallGlobalVar(pMap->pVm,` |
|       126 |  640 | `				(const char *)SyBlobData(&pKey->sBlob),SyBlobLength(&pKey->sBlob),` |
|        63 |  641 | `				pVal,SXU32_HIGH);` |
|         - |  642 | `		}` |
|         - |  643 | `		/* Perform a blob-key insertion */` |
|    191085 |  644 | `		rc = HashmapInsertBlobKey(&(*pMap),SyBlobData(&pKey->sBlob),SyBlobLength(&pKey->sBlob),&(*pVal),0,FALSE);` |
|    191085 |  645 | `		return rc;` |
|         - |  646 | `	}` |
|   1550928 |  647 | `IntKey:` |
|   3105577 |  648 | `	if( pKey ){` |
|   2141031 |  649 | `		if((pKey->iFlags & MEMOBJ_INT) == 0 ){` |
|         - |  650 | `			/* Force an integer cast */` |
|       257 |  651 | `			PH7_MemObjToInteger(pKey);` |
|       128 |  652 | `		}` |
|   2141031 |  653 | `		if( SXRET_OK == HashmapLookupIntKey(&(*pMap),pKey->x.iVal,&pNode) ){` |
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
|   2140945 |  667 | `		if( pMap == pMap->pVm->pGlobal ){` |
|         - |  668 | `			/* php 8.1: an int key creates the global named by its decimal` |
|         - |  669 | `			 * form ($GLOBALS[7] = ... behaves like $GLOBALS['7'] = ...). */` |
|         - |  670 | `			char zKey[24];` |
|         3 |  671 | `			sxu32 nKey = SyBufferFormat(zKey,sizeof(zKey),"%qd",pKey->x.iVal);` |
|         3 |  672 | `			return PH7_VmInstallGlobalVar(pMap->pVm,zKey,nKey,pVal,SXU32_HIGH);` |
|         - |  673 | `		}` |
|         - |  674 | `		/* Perform a 64-bit-int-key insertion */` |
|   2140943 |  675 | `		rc = HashmapInsertIntKey(&(*pMap),pKey->x.iVal,&(*pVal),0,FALSE);` |
|   2140943 |  676 | `		if( rc == SXRET_OK ){` |
|   2140943 |  677 | `			HashmapAdvanceAutoIndex(&(*pMap),pKey->x.iVal);` |
|   1070469 |  678 | `		}` |
|   1070474 |  679 | `	}else{` |
|    964551 |  680 | `		if( pMap == pMap->pVm->pGlobal ){` |
|         - |  681 | `			/* php's catchable Error: Cannot append to $GLOBALS */` |
|         3 |  682 | `			return PH7_VmThrowGlobalsAppendError(pMap->pVm);` |
|         - |  683 | `		}` |
|    964549 |  684 | `		if( HashmapAppendIndexBusy(&(*pMap),&rc) ){` |
|         7 |  685 | `			return rc; /* PH7_EXCEPTION/PH7_ABORT: php's catchable Error was thrown */` |
|         - |  686 | `		}` |
|         - |  687 | `		/* Assign an automatic index */` |
|    964543 |  688 | `		rc = HashmapInsertIntKey(&(*pMap),pMap->iNextIdx,&(*pVal),0,FALSE);` |
|    964543 |  689 | `		if( rc == SXRET_OK && pMap->iNextIdx < SXI64_HIGH ){` |
|    964541 |  690 | `			++pMap->iNextIdx;` |
|    482268 |  691 | `		}` |
|         - |  692 | `	}` |
|         - |  693 | `	/* Insertion result */` |
|   3105481 |  694 | `	return rc;` |
|   1648578 |  695 | `}` |
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
|     46480 |  723 | `static sxi32 HashmapInsertByRef(` |
|         - |  724 | `	ph7_hashmap *pMap,   /* Target hashmap */` |
|         - |  725 | `	ph7_value *pKey,     /* Lookup key */` |
|         - |  726 | `	sxu32 nRefIdx        /* Foreign ph7_value index */` |
|         - |  727 | `	)` |
|         5 |  728 | `{` |
|     46485 |  729 | `	ph7_hashmap_node *pNode = 0;` |
|     46485 |  730 | `	sxi32 rc = SXRET_OK;` |
|     46485 |  731 | `	if( pKey && pKey->iFlags & (MEMOBJ_STRING\|MEMOBJ_HASHMAP\|MEMOBJ_OBJ\|MEMOBJ_RES) ){` |
|     46449 |  732 | `		if( (pKey->iFlags & MEMOBJ_STRING) == 0 ){` |
|         - |  733 | `			/* Force a string cast */` |
|       ! 0 |  734 | `			PH7_MemObjToString(&(*pKey));` |
|       ! 0 |  735 | `		}` |
|     46449 |  736 | `		if( SyBlobLength(&pKey->sBlob) < 1 \|\| HashmapIsIntKey(&pKey->sBlob) ){` |
|         3 |  737 | `			if(SyBlobLength(&pKey->sBlob) < 1){` |
|         - |  738 | `				/* Automatic index assign */` |
|       ! 0 |  739 | `				pKey = 0;` |
|       ! 0 |  740 | `			}` |
|         3 |  741 | `			goto IntKey;` |
|         - |  742 | `		}` |
|     69668 |  743 | `		if( SXRET_OK == HashmapLookupBlobKey(&(*pMap),SyBlobData(&pKey->sBlob),` |
|     23221 |  744 | `			SyBlobLength(&pKey->sBlob),&pNode) ){` |
|         - |  745 | `				/* Overwrite */` |
|         7 |  746 | `				PH7_VmRefObjRemove(pMap->pVm,pNode->nValIdx,0,pNode);` |
|         7 |  747 | `				pNode->nValIdx = nRefIdx;` |
|         - |  748 | `				/* Install in the reference table */` |
|         7 |  749 | `				PH7_VmRefObjInstall(pMap->pVm,nRefIdx,0,pNode,0);` |
|         7 |  750 | `				return SXRET_OK;` |
|         - |  751 | `		}` |
|         - |  752 | `		/* Perform a blob-key insertion */` |
|     46441 |  753 | `		rc = HashmapInsertBlobKey(&(*pMap),SyBlobData(&pKey->sBlob),SyBlobLength(&pKey->sBlob),0,nRefIdx,TRUE);` |
|     46441 |  754 | `		return rc;` |
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
|     23245 |  787 | `}` |
|         - |  788 | `/*` |
|         - |  789 | ` * Extract node value.` |
|         - |  790 | ` */` |
|   1379670 |  791 | `static ph7_value * HashmapExtractNodeValue(ph7_hashmap_node *pNode)` |
|         5 |  792 | `{` |
|         - |  793 | `	/* Point to the desired object */` |
|         - |  794 | `	ph7_value *pObj;` |
|   1379675 |  795 | `	pObj = (ph7_value *)SySetAt(&pNode->pMap->pVm->aMemObj,pNode->nValIdx);` |
|   1379675 |  796 | `	return pObj;` |
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
|     70236 |  842 | `static sxi32 HashmapNodeCmp(ph7_hashmap_node *pLeft,ph7_hashmap_node *pRight,int bStrict)` |
|         5 |  843 | `{` |
|         - |  844 | `	ph7_value sObj1,sObj2;` |
|         - |  845 | `	sxi32 rc;` |
|     70241 |  846 | `	if( pLeft == pRight ){` |
|         - |  847 | `		/*` |
|         - |  848 | `		 * Same node.Refer to the sort() implementation defined` |
|         - |  849 | `		 * below for more information on this sceanario.` |
|         - |  850 | `		 */` |
|       ! 0 |  851 | `		return 0;` |
|         - |  852 | `	}` |
|         - |  853 | `	/* Do the comparison */` |
|     70241 |  854 | `	PH7_MemObjInit(pLeft->pMap->pVm,&sObj1);` |
|     70241 |  855 | `	PH7_MemObjInit(pLeft->pMap->pVm,&sObj2);` |
|     70241 |  856 | `	PH7_HashmapExtractNodeValue(pLeft,&sObj1,FALSE);` |
|     70241 |  857 | `	PH7_HashmapExtractNodeValue(pRight,&sObj2,FALSE);` |
|     70241 |  858 | `	rc = PH7_MemObjCmp(&sObj1,&sObj2,bStrict,0);` |
|     70241 |  859 | `	PH7_MemObjRelease(&sObj1);` |
|     70241 |  860 | `	PH7_MemObjRelease(&sObj2);` |
|     70241 |  861 | `	return rc;` |
|     35135 |  862 | `}` |
|         - |  863 | `/*` |
|         - |  864 | ` * Rehash a node with a 64-bit integer key.` |
|         - |  865 | ` * Refer to [merge_sort(),array_shift()] implementations for more information.` |
|         - |  866 | ` */` |
|     13478 |  867 | `static void HashmapRehashIntNode(ph7_hashmap_node *pEntry)` |
|         5 |  868 | `{` |
|     13483 |  869 | `	ph7_hashmap *pMap = pEntry->pMap;` |
|         - |  870 | `	sxu32 nBucket;` |
|         - |  871 | `	/* Remove old collision links */` |
|     13483 |  872 | `	if( pEntry->pPrevCollide ){` |
|     11025 |  873 | `		pEntry->pPrevCollide->pNextCollide = pEntry->pNextCollide;` |
|      5519 |  874 | `	}else{` |
|      2463 |  875 | `		pMap->apBucket[pEntry->nHash & (pMap->nSize - 1)] = pEntry->pNextCollide;` |
|         - |  876 | `	}` |
|     13483 |  877 | `	if( pEntry->pNextCollide ){` |
|      1109 |  878 | `		pEntry->pNextCollide->pPrevCollide = pEntry->pPrevCollide;` |
|       561 |  879 | `	}` |
|     13483 |  880 | `	pEntry->pNextCollide = pEntry->pPrevCollide = 0;` |
|         - |  881 | `	/* Compute the new hash */` |
|     13483 |  882 | `	pEntry->nHash = pMap->xIntHash(pMap->iNextIdx);` |
|     13483 |  883 | `	pEntry->xKey.iKey = pMap->iNextIdx;` |
|     13483 |  884 | `	nBucket = pEntry->nHash & (pMap->nSize - 1);` |
|         - |  885 | `	/* Link to the new bucket */` |
|     13483 |  886 | `	pEntry->pNextCollide = pMap->apBucket[nBucket];` |
|     13483 |  887 | `	if( pMap->apBucket[nBucket] ){` |
|     11353 |  888 | `		pMap->apBucket[nBucket]->pPrevCollide = pEntry;` |
|      5678 |  889 | `	}` |
|     13483 |  890 | `	pEntry->pNextCollide = pMap->apBucket[nBucket];` |
|     13483 |  891 | `	pMap->apBucket[nBucket] = pEntry;` |
|         - |  892 | `	/* Increment the automatic index (saturating, like every other advance —` |
|         - |  893 | `	 * unreachable in practice since renumbering assigns 0..nEntry-1, but keep` |
|         - |  894 | `	 * the no-overflow invariant uniform). */` |
|     13483 |  895 | `	if( pMap->iNextIdx < SXI64_HIGH ){` |
|     13483 |  896 | `		pMap->iNextIdx++;` |
|      6739 |  897 | `	}` |
|     13483 |  898 | `}` |
|         - |  899 | `/*` |
|         - |  900 | ` * Perform a linear search on a given hashmap.` |
|         - |  901 | ` * Write a pointer to the target node on success.` |
|         - |  902 | ` * Otherwise SXERR_NOTFOUND is returned on failure.` |
|         - |  903 | ` * Refer to [array_intersect(),array_diff(),in_array(),...] implementations` |
|         - |  904 | ` * for more information.` |
|         - |  905 | ` */` |
|     32740 |  906 | `static int HashmapFindValue(` |
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
|     32745 |  919 | `	pEntry = pMap->pFirst;` |
|     32745 |  920 | `	n = pMap->nEntry;` |
|     32745 |  921 | `	PH7_MemObjInit(pMap->pVm,&sVal);` |
|     32745 |  922 | `	PH7_MemObjInit(pMap->pVm,&sNeedle);` |
|     77730 |  923 | `	for(;;){` |
|    155465 |  924 | `		if( n < 1 ){` |
|       107 |  925 | `			break;` |
|         - |  926 | `		}` |
|         - |  927 | `		/* Extract node value */` |
|    155359 |  928 | `		pVal = HashmapExtractNodeValue(pEntry);` |
|    155359 |  929 | `		if( pVal ){` |
|    155359 |  930 | `			if( (pVal->iFlags\|pNeedle->iFlags) & MEMOBJ_NULL ){` |
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
|    155359 |  942 | `				PH7_MemObjLoad(pVal,&sVal);` |
|    155359 |  943 | `				PH7_MemObjLoad(pNeedle,&sNeedle);` |
|    155359 |  944 | `				rc = PH7_MemObjCmp(&sNeedle,&sVal,bStrict,0);` |
|    155359 |  945 | `				PH7_MemObjRelease(&sVal);` |
|    155359 |  946 | `				PH7_MemObjRelease(&sNeedle);` |
|    155359 |  947 | `				if( rc == 0 ){` |
|     32639 |  948 | `					if( ppNode ){` |
|        23 |  949 | `						*ppNode = pEntry;` |
|        11 |  950 | `					}` |
|         - |  951 | `					/* Match found*/` |
|     32639 |  952 | `					return SXRET_OK;` |
|         - |  953 | `				}` |
|         - |  954 | `			}` |
|     61360 |  955 | `		}` |
|         - |  956 | `		/* Point to the next entry */` |
|    122725 |  957 | `		pEntry = pEntry->pPrev; /* Reverse link */` |
|    122725 |  958 | `		n--;` |
|         5 |  959 | `	}` |
|         - |  960 | `	/* No such entry */` |
|       107 |  961 | `	return SXERR_NOTFOUND;` |
|     16375 |  962 | `}` |
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
|    636488 | 1148 | `static sxi32 HashmapDuplicateNode(` |
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
|    636493 | 1159 | `	if( pEntry->iFlags & HASHMAP_NODE_FOREIGN_OBJ ){` |
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
|    636487 | 1184 | `	sSafeVal = *pVal;` |
|         - | 1185 |  |
|    636487 | 1186 | `	if( pEntry->iType == HASHMAP_BLOB_NODE ){` |
|         - | 1187 | `		/* Blob key insertion */` |
|      4029 | 1188 | `		PH7_MemObjInitFromString(pDest->pVm,&sKey,0);` |
|      4029 | 1189 | `		PH7_MemObjStringAppend(&sKey,(const char *)SyBlobData(&pEntry->xKey.sKey),SyBlobLength(&pEntry->xKey.sKey));` |
|      4029 | 1190 | `		rc = PH7_HashmapInsert(pDest,&sKey,&sSafeVal);` |
|      4029 | 1191 | `		PH7_MemObjRelease(&sKey);` |
|      2017 | 1192 | `	}else{` |
|         - | 1193 | `		/* Int key */` |
|    632463 | 1194 | `		if( iAction == 0 ){ /* Merge */` |
|    632249 | 1195 | `			rc = HashmapInsert(pDest,0/* Automatic index assign */,&sSafeVal);` |
|    316339 | 1196 | `		}else if( iAction == 1 ){ /* Overwrite */` |
|        32 | 1197 | `			PH7_MemObjInitFromInt(pDest->pVm,&sKey,pEntry->xKey.iKey);` |
|        32 | 1198 | `			rc = PH7_HashmapInsert(pDest,&sKey,&sSafeVal);` |
|        32 | 1199 | `			PH7_MemObjRelease(&sKey);` |
|        17 | 1200 | `		}else{ /* Dup */` |
|       187 | 1201 | `			rc = HashmapInsertIntKey(pDest,pEntry->xKey.iKey,&sSafeVal,0,FALSE);` |
|         - | 1202 | `		}` |
|         - | 1203 | `	}` |
|    636487 | 1204 | `	return rc;` |
|    318249 | 1205 | `}` |
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
|      2120 | 1218 | `static sxi32 HashmapMerge(ph7_hashmap *pSrc,ph7_hashmap *pDest)` |
|         5 | 1219 | `{` |
|         - | 1220 | `	ph7_hashmap_node *pEntry;` |
|         - | 1221 | `	ph7_value *pVal;` |
|         - | 1222 | `	sxi32 rc;` |
|         - | 1223 | `	sxu32 n;` |
|      2125 | 1224 | `	if( pSrc == pDest ){` |
|         - | 1225 | `		/* Same map. This can easily happen since hashmaps are passed by reference.` |
|         - | 1226 | `		 * Unlike the zend engine.` |
|         - | 1227 | `		 */` |
|       ! 0 | 1228 | `		return SXRET_OK;` |
|         - | 1229 | `	}` |
|         - | 1230 | `	/* Point to the first inserted entry in the source */` |
|      2125 | 1231 | `	pEntry = pSrc->pFirst;` |
|         - | 1232 | `	/* Perform the merge */` |
|    634427 | 1233 | `	for( n = 0 ; n < pSrc->nEntry ; ++n ){` |
|         - | 1234 | `		/* Extract the node value */` |
|    632307 | 1235 | `		pVal = HashmapExtractNodeValue(pEntry);` |
|    632307 | 1236 | `		if( pVal ){` |
|         - | 1237 | `			/* Make a local copy of the value.` |
|         - | 1238 | `			 * The insertion call below may trigger a memory pool reallocation` |
|         - | 1239 | `			 * which will invalidate the 'pVal' pointer since it points` |
|         - | 1240 | `			 * to the old pool.` |
|         - | 1241 | `			 */` |
|    632307 | 1242 | `			rc = HashmapDuplicateNode(pDest,pEntry,pVal,0);` |
|    316156 | 1243 | `		}else{` |
|       ! 0 | 1244 | `			rc = SXRET_OK;` |
|         - | 1245 | `		}` |
|    632307 | 1246 | `		if( rc != SXRET_OK ){` |
|       ! 0 | 1247 | `			return rc;` |
|         - | 1248 | `		}` |
|         - | 1249 | `		/* Point to the next entry */` |
|    632307 | 1250 | `		pEntry = pEntry->pPrev; /* Reverse link */` |
|    316156 | 1251 | `	}` |
|      2125 | 1252 | `	return SXRET_OK;` |
|      1065 | 1253 | `}` |
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
|      8069 | 1318 | `	for( n = 0 ; n < pSrc->nEntry ; ++n ){` |
|         - | 1319 | `		/* Extract the node value */` |
|      4147 | 1320 | `		pVal = HashmapExtractNodeValue(pEntry);` |
|      4147 | 1321 | `		if( pVal ){` |
|      4147 | 1322 | `			rc = HashmapDuplicateNode(pDest,pEntry,pVal,2);` |
|      2076 | 1323 | `		}else{` |
|       ! 0 | 1324 | `			rc = SXRET_OK;` |
|         - | 1325 | `		}` |
|      4147 | 1326 | `		if( rc != SXRET_OK ){` |
|       ! 0 | 1327 | `			return rc;` |
|         - | 1328 | `		}` |
|         - | 1329 | `		/* Point to the next entry */` |
|      4147 | 1330 | `		pEntry = pEntry->pPrev; /* Reverse link */` |
|      2076 | 1331 | `	}` |
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
|       715 | 1352 | `	for( n = 0 ; n < pSrc->nEntry ; ++n ){` |
|         - | 1353 | `		/* Extract the node value (resolves foreign references) */` |
|       703 | 1354 | `		pVal = HashmapExtractNodeValue(pEntry);` |
|       702 | 1355 | `		if( pVal && (pVal->iFlags & MEMOBJ_HASHMAP)` |
|       461 | 1356 | `		 && (ph7_hashmap *)pVal->x.pOther == pSrc->pVm->pGlobal ){` |
|         - | 1357 | `			/* A global still holding the live $GLOBALS map is the snapshot's` |
|         - | 1358 | `			 * own destination mid-store ($snap = $GLOBALS registers $snap` |
|         - | 1359 | `			 * before the value lands). php's snapshot — taken when $GLOBALS` |
|         - | 1360 | `			 * is READ, before the assignment — has no such entry, so skip it` |
|         - | 1361 | `			 * (also breaks the would-be infinite recursion). */` |
|         5 | 1362 | `			pVal = 0;` |
|         2 | 1363 | `		}` |
|       703 | 1364 | `		if( pVal ){` |
|       699 | 1365 | `			if( pEntry->iType == HASHMAP_BLOB_NODE ){` |
|      1042 | 1366 | `				rc = HashmapInsertBlobKey(&(*pDest),SyBlobData(&pEntry->xKey.sKey),` |
|       347 | 1367 | `					SyBlobLength(&pEntry->xKey.sKey),pVal,0,FALSE);` |
|       348 | 1368 | `			}else{` |
|         5 | 1369 | `				rc = HashmapInsertIntKey(&(*pDest),pEntry->xKey.iKey,pVal,0,FALSE);` |
|         - | 1370 | `			}` |
|       699 | 1371 | `			if( rc != SXRET_OK ){` |
|       ! 0 | 1372 | `				return rc;` |
|         - | 1373 | `			}` |
|       349 | 1374 | `		}` |
|         - | 1375 | `		/* Point to the next entry */` |
|       703 | 1376 | `		pEntry = pEntry->pPrev; /* Reverse link */` |
|       352 | 1377 | `	}` |
|        13 | 1378 | `	return SXRET_OK;` |
|         7 | 1379 | `}` |
|         - | 1380 | `/*` |
|         - | 1381 | ` * Copy-on-write separation for arrays.` |
|         - | 1382 | ` * If the hashmap inside pValue has iRef > 1 (shared), duplicate it so that` |
|         - | 1383 | ` * pValue owns a private copy. The original map's refcount is decremented.` |
|         - | 1384 | ` * Returns the (possibly new) hashmap pointer.` |
|         - | 1385 | ` */` |
|    222512 | 1386 | `PH7_PRIVATE ph7_hashmap * PH7_HashmapCowSeparate(ph7_vm *pVm,ph7_value *pValue)` |
|         5 | 1387 | `{` |
|    222517 | 1388 | `	ph7_hashmap *pMap = (ph7_hashmap *)pValue->x.pOther;` |
|         - | 1389 | `	ph7_hashmap *pNew;` |
|         - | 1390 | `	ph7_value *pBacking;` |
|         - | 1391 | `	sxu32 nValIdx;` |
|         - | 1392 | `	int bValueInPool;` |
|    222517 | 1393 | `	if( pMap->iRef < 2 ){` |
|         - | 1394 | `		/* Sole owner, no separation needed */` |
|    220197 | 1395 | `		return pMap;` |
|         - | 1396 | `	}` |
|      2325 | 1397 | `	if( pMap == pVm->pGlobal ){` |
|         - | 1398 | `		/* Never separate $GLOBALS — it is a live view of the symbol table.` |
|         - | 1399 | `		 * (A COPY of $GLOBALS never shares this map: PH7_MemObjStore` |
|         - | 1400 | `		 * materializes a by-value snapshot at assignment, php 8.1.) */` |
|       123 | 1401 | `		return pMap;` |
|         - | 1402 | `	}` |
|         - | 1403 | `	/* If this value is a stack copy of a named variable, separate the` |
|         - | 1404 | `	 * backing variable instead so the change persists after the stack` |
|         - | 1405 | `	 * frame is popped. */` |
|      2203 | 1406 | `	if( pValue->nIdx != SXU32_HIGH ){` |
|      2203 | 1407 | `		pBacking = (ph7_value *)SySetAt(&pVm->aMemObj,pValue->nIdx);` |
|      2198 | 1408 | `		if( pBacking && pBacking != pValue` |
|      2178 | 1409 | `			&& (pBacking->iFlags & MEMOBJ_HASHMAP)` |
|      2163 | 1410 | `			&& (ph7_hashmap *)pBacking->x.pOther == pMap ){` |
|         - | 1411 | `			/* Undo the stack ref to reveal true sharing count */` |
|      2163 | 1412 | `			pMap->iRef--;` |
|      2163 | 1413 | `			if( pMap->iRef < 2 ){` |
|         - | 1414 | `				/* After undoing stack ref, sole owner — no separation */` |
|      2121 | 1415 | `				pMap->iRef++;` |
|      2121 | 1416 | `				return pMap;` |
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
|    111261 | 1479 | `}` |
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
|    135956 | 1571 | `PH7_PRIVATE ph7_hashmap * PH7_NewHashmap(` |
|         - | 1572 | `	ph7_vm *pVm,              /* VM that trigger the hashmap creation */` |
|         - | 1573 | `	sxu32 (*xIntHash)(sxi64), /* Hash function for int keys.NULL otherwise*/` |
|         - | 1574 | `	sxu32 (*xBlobHash)(const void *,sxu32) /* Hash function for BLOB keys.NULL otherwise */` |
|         - | 1575 | `	)` |
|         5 | 1576 | `{` |
|         - | 1577 | `	ph7_hashmap *pMap;` |
|         - | 1578 | `	/* Allocate a new instance */` |
|    135961 | 1579 | `	pMap = (ph7_hashmap *)SyMemBackendPoolAlloc(&pVm->sAllocator,sizeof(ph7_hashmap));` |
|    135961 | 1580 | `	if( pMap == 0 ){` |
|       ! 0 | 1581 | `		return 0;` |
|         - | 1582 | `	}` |
|         - | 1583 | `	/* Zero the structure */` |
|    135961 | 1584 | `	SyZero(pMap,sizeof(ph7_hashmap));` |
|         - | 1585 | `	/* Fill in the structure */` |
|    135961 | 1586 | `	pMap->pVm = &(*pVm);` |
|    135961 | 1587 | `	pMap->iRef = 1;` |
|         - | 1588 | `	/* Default hash functions */` |
|    135961 | 1589 | `	pMap->xIntHash  = xIntHash ? xIntHash : IntHash;` |
|    135961 | 1590 | `	pMap->xBlobHash = xBlobHash ? xBlobHash : BinHash;` |
|    135961 | 1591 | `	return pMap;` |
|     67983 | 1592 | `}` |
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
|     93014 | 1684 | `PH7_PRIVATE sxi32 PH7_HashmapRelease(ph7_hashmap *pMap,int FreeDS)` |
|         5 | 1685 | `{` |
|         - | 1686 | `	ph7_hashmap_node *pEntry,*pNext;` |
|     93019 | 1687 | `	ph7_vm *pVm = pMap->pVm;` |
|         - | 1688 | `	sxu32 n;` |
|     93019 | 1689 | `	if( pMap == pVm->pGlobal ){` |
|         - | 1690 | `		/* Cannot delete the $GLOBALS array */` |
|       ! 0 | 1691 | `		PH7_VmThrowError(pMap->pVm,0,PH7_CTX_NOTICE,"$GLOBALS is a read-only array,deletion is forbidden");` |
|       ! 0 | 1692 | `		return SXRET_OK;` |
|         - | 1693 | `	}` |
|         - | 1694 | `	/* Start the release process */` |
|     93019 | 1695 | `	n = 0;` |
|     93019 | 1696 | `	pEntry = pMap->pFirst;` |
|   1675754 | 1697 | `	for(;;){` |
|   3351513 | 1698 | `		if( n >= pMap->nEntry ){` |
|     93019 | 1699 | `			break;` |
|         - | 1700 | `		}` |
|   3258499 | 1701 | `		pNext = pEntry->pPrev; /* Reverse link */` |
|         - | 1702 | `		/* Remove the reference from the foreign table */` |
|   3258499 | 1703 | `		PH7_VmRefObjRemove(pVm,pEntry->nValIdx,0,pEntry);` |
|   3258499 | 1704 | `		if( (pEntry->iFlags & HASHMAP_NODE_FOREIGN_OBJ) == 0 ){` |
|         - | 1705 | `			/* Restore the ph7_value to the free list */` |
|   3258489 | 1706 | `			PH7_VmUnsetMemObj(pVm,pEntry->nValIdx,FALSE);` |
|   1629242 | 1707 | `		}` |
|         - | 1708 | `		/* Release the node */` |
|   3258499 | 1709 | `		if( pEntry->iType == HASHMAP_BLOB_NODE ){` |
|    164037 | 1710 | `			SyBlobRelease(&pEntry->xKey.sKey);` |
|     82016 | 1711 | `		}` |
|   3258499 | 1712 | `		SyMemBackendPoolFree(&pVm->sAllocator,pEntry);` |
|         - | 1713 | `		/* Point to the next entry */` |
|   3258499 | 1714 | `		pEntry = pNext;` |
|   3258499 | 1715 | `		n++;` |
|         5 | 1716 | `	}` |
|     93019 | 1717 | `	if( pMap->nEntry > 0 ){` |
|         - | 1718 | `		/* Release the hash bucket */` |
|     70871 | 1719 | `		SyMemBackendFree(&pVm->sAllocator,pMap->apBucket);` |
|     35433 | 1720 | `	}` |
|     93019 | 1721 | `	if( FreeDS ){` |
|         - | 1722 | `		/* Free the whole instance */` |
|     93003 | 1723 | `		SyMemBackendPoolFree(&pVm->sAllocator,pMap);` |
|     46504 | 1724 | `	}else{` |
|         - | 1725 | `		/* Keep the instance but reset it's fields */` |
|        17 | 1726 | `		pMap->apBucket = 0;` |
|        17 | 1727 | `		pMap->iNextIdx = 0;` |
|        17 | 1728 | `		pMap->nEntry = pMap->nSize = 0;` |
|        17 | 1729 | `		pMap->pFirst = pMap->pLast = pMap->pCur = 0;` |
|         - | 1730 | `	}` |
|     93019 | 1731 | `	return SXRET_OK;` |
|     46512 | 1732 | `}` |
|         - | 1733 | `/*` |
|         - | 1734 | ` * Decrement the reference count of a given hashmap.` |
|         - | 1735 | ` * If the count reaches zero which mean no more variables` |
|         - | 1736 | ` * are pointing to this hashmap,then release the whole instance.` |
|         - | 1737 | ` */` |
|    797926 | 1738 | `PH7_PRIVATE void  PH7_HashmapUnref(ph7_hashmap *pMap)` |
|         5 | 1739 | `{` |
|    797931 | 1740 | `	ph7_vm *pVm = pMap->pVm;` |
|         - | 1741 | `	/* TICKET 1432-49: $GLOBALS is not subject to garbage collection */` |
|    797931 | 1742 | `	pMap->iRef--;` |
|    797931 | 1743 | `	if( pMap->iRef < 1 && pMap != pVm->pGlobal){` |
|     92983 | 1744 | `		PH7_HashmapRelease(pMap,TRUE);` |
|     46489 | 1745 | `	}` |
|    797931 | 1746 | `}` |
|         - | 1747 | `/*` |
|         - | 1748 | ` * Check if a given key exists in the given hashmap.` |
|         - | 1749 | ` * Write a pointer to the target node on success.` |
|         - | 1750 | ` * Otherwise SXERR_NOTFOUND is returned on failure.` |
|         - | 1751 | ` */` |
|    134772 | 1752 | `PH7_PRIVATE sxi32 PH7_HashmapLookup(` |
|         - | 1753 | `	ph7_hashmap *pMap,        /* Target hashmap */` |
|         - | 1754 | `	ph7_value *pKey,          /* Lookup key */` |
|         - | 1755 | `	ph7_hashmap_node **ppNode /* OUT: Target node on success */` |
|         - | 1756 | `	)` |
|         5 | 1757 | `{` |
|         - | 1758 | `	sxi32 rc;` |
|    134777 | 1759 | `	if( pMap->nEntry < 1 ){` |
|         - | 1760 | `		/* TICKET 1433-25: Don't bother hashing,the hashmap is empty anyway.` |
|         - | 1761 | `		 */` |
|       120 | 1762 | `		return SXERR_NOTFOUND;` |
|         - | 1763 | `	}` |
|    134661 | 1764 | `	rc = HashmapLookup(&(*pMap),&(*pKey),ppNode);` |
|    134661 | 1765 | `	return rc;` |
|     67391 | 1766 | `}` |
|         - | 1767 | `/*` |
|         - | 1768 | ` * Insert a given key and it's associated value (if any) in the given` |
|         - | 1769 | ` * hashmap.` |
|         - | 1770 | ` * If a node with the given key already exists in the database` |
|         - | 1771 | ` * then this function overwrite the old value.` |
|         - | 1772 | ` */` |
|   2664670 | 1773 | `PH7_PRIVATE sxi32 PH7_HashmapInsert(` |
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
|   2664675 | 1784 | `	rc = HashmapInsert(&(*pMap),&(*pKey),&(*pVal));` |
|   2664675 | 1785 | `	return rc;` |
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
|     46474 | 1824 | `PH7_PRIVATE sxi32 PH7_HashmapInsertByRef(` |
|         - | 1825 | `	ph7_hashmap *pMap, /* Target hashmap */` |
|         - | 1826 | `	ph7_value *pKey,   /* Lookup key */` |
|         - | 1827 | `	sxu32 nRefIdx      /* Foreign ph7_value index */` |
|         - | 1828 | `	)` |
|         5 | 1829 | `{` |
|         - | 1830 | `	sxi32 rc;` |
|     46479 | 1831 | `	if( nRefIdx == pMap->pVm->nGlobalIdx ){` |
|         - | 1832 | `		/* php's non-catchable fatal: $a[] =& $GLOBALS is forbidden (8.1) */` |
|       ! 0 | 1833 | `		PH7_VmThrowError(pMap->pVm,0,PH7_CTX_ERR,"Cannot acquire reference to $GLOBALS");` |
|       ! 0 | 1834 | `		pMap->pVm->iExitStatus = 255;` |
|       ! 0 | 1835 | `		pMap->pVm->bHaltRequested = 1;` |
|       ! 0 | 1836 | `		return PH7_ABORT;` |
|         - | 1837 | `	}` |
|     46479 | 1838 | `	rc = HashmapInsertByRef(&(*pMap),&(*pKey),nRefIdx);` |
|     46479 | 1839 | `	return rc;` |
|     23242 | 1840 | `}` |
|         - | 1841 | `/*` |
|         - | 1842 | ` * Reset the node cursor of a given hashmap.` |
|         - | 1843 | ` */` |
|     36648 | 1844 | `PH7_PRIVATE void PH7_HashmapResetLoopCursor(ph7_hashmap *pMap)` |
|         5 | 1845 | `{` |
|         - | 1846 | `	/* Reset the loop cursor */` |
|     36653 | 1847 | `	pMap->pCur = pMap->pFirst;` |
|     36653 | 1848 | `}` |
|         - | 1849 | `/*` |
|         - | 1850 | ` * Return a pointer to the node currently pointed by the node cursor.` |
|         - | 1851 | ` * If the cursor reaches the end of the list,then this function` |
|         - | 1852 | ` * return NULL.` |
|         - | 1853 | ` * Note that the node cursor is automatically advanced by this function.` |
|         - | 1854 | ` */` |
|    240584 | 1855 | `PH7_PRIVATE ph7_hashmap_node * PH7_HashmapGetNextEntry(ph7_hashmap *pMap)` |
|         5 | 1856 | `{` |
|    240589 | 1857 | `	ph7_hashmap_node *pCur = pMap->pCur;` |
|    240589 | 1858 | `	if( pCur == 0 ){` |
|         - | 1859 | `		/* End of the list,return null */` |
|     18317 | 1860 | `		return 0;` |
|         - | 1861 | `	}` |
|         - | 1862 | `	/* Advance the node cursor */` |
|    222277 | 1863 | `	pMap->pCur = pCur->pPrev; /* Reverse link */` |
|    222277 | 1864 | `	return pCur;` |
|    120297 | 1865 | `}` |
|         - | 1866 | `/*` |
|         - | 1867 | ` * Extract a node value.` |
|         - | 1868 | ` */` |
|    562794 | 1869 | `PH7_PRIVATE void PH7_HashmapExtractNodeValue(ph7_hashmap_node *pNode,ph7_value *pValue,int bStore)` |
|         5 | 1870 | `{` |
|    562799 | 1871 | `	ph7_value *pEntry = HashmapExtractNodeValue(pNode);` |
|    562799 | 1872 | `	if( pEntry ){` |
|    562799 | 1873 | `		if( bStore ){` |
|    222687 | 1874 | `			PH7_MemObjStore(pEntry,pValue);` |
|    111346 | 1875 | `		}else{` |
|    340117 | 1876 | `			PH7_MemObjLoad(pEntry,pValue);` |
|         - | 1877 | `		}` |
|    281426 | 1878 | `	}else{` |
|       ! 0 | 1879 | `		PH7_MemObjRelease(pValue);` |
|         - | 1880 | `	}` |
|    562799 | 1881 | `}` |
|         - | 1882 | `/*` |
|         - | 1883 | ` * Extract a node key.` |
|         - | 1884 | ` */` |
|    145650 | 1885 | `PH7_PRIVATE void PH7_HashmapExtractNodeKey(ph7_hashmap_node *pNode,ph7_value *pKey)` |
|         5 | 1886 | `{` |
|         - | 1887 | `	/* Fill with the current key */` |
|    145655 | 1888 | `	if( pNode->iType == HASHMAP_INT_NODE ){` |
|    140821 | 1889 | `		if( SyBlobLength(&pKey->sBlob) > 0 ){` |
|        31 | 1890 | `			SyBlobRelease(&pKey->sBlob);` |
|        15 | 1891 | `		}` |
|    140821 | 1892 | `		pKey->x.iVal = pNode->xKey.iKey;` |
|    140821 | 1893 | `		MemObjSetType(pKey,MEMOBJ_INT);` |
|     70413 | 1894 | `	}else{` |
|      4839 | 1895 | `		SyBlobReset(&pKey->sBlob);` |
|      4839 | 1896 | `		SyBlobAppend(&pKey->sBlob,SyBlobData(&pNode->xKey.sKey),SyBlobLength(&pNode->xKey.sKey));` |
|      4839 | 1897 | `		MemObjSetType(pKey,MEMOBJ_STRING);` |
|         - | 1898 | `	}` |
|    145655 | 1899 | `}` |
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
|     34478 | 1950 | `static ph7_hashmap_node * HashmapNodeMerge(ph7_hashmap_node *pA,ph7_hashmap_node *pB,ProcNodeCmp xCmp,void *pCmpData)` |
|         5 | 1951 | `{` |
|         - | 1952 | `	ph7_hashmap_node result,*pTail;` |
|         - | 1953 | `    /* Prevent compiler warning */` |
|     34483 | 1954 | `	result.pNext = result.pPrev = 0;` |
|     34483 | 1955 | `	pTail = &result;` |
|    104805 | 1956 | `	while( pA && pB ){` |
|     70327 | 1957 | `		if( xCmp(pA,pB,pCmpData) <= 0 ){` |
|     46675 | 1958 | `			pTail->pPrev = pA;` |
|     46675 | 1959 | `			pA->pNext = pTail;` |
|     46675 | 1960 | `			pTail = pA;` |
|     46675 | 1961 | `			pA = pA->pPrev;` |
|     23337 | 1962 | `		}else{` |
|     23657 | 1963 | `			pTail->pPrev = pB;` |
|     23657 | 1964 | `			pB->pNext = pTail;` |
|     23657 | 1965 | `			pTail = pB;` |
|     23657 | 1966 | `			pB = pB->pPrev;` |
|         - | 1967 | `		}` |
|         5 | 1968 | `	}` |
|     34483 | 1969 | `	if( pA ){` |
|     24185 | 1970 | `		pTail->pPrev = pA;` |
|     24185 | 1971 | `		pA->pNext = pTail;` |
|     22405 | 1972 | `	}else if( pB ){` |
|     10071 | 1973 | `		pTail->pPrev = pB;` |
|     10071 | 1974 | `		pB->pNext = pTail;` |
|      5026 | 1975 | `	}else{` |
|       237 | 1976 | `		pTail->pPrev = pTail->pNext = 0;` |
|         - | 1977 | `	}` |
|     34483 | 1978 | `	return result.pPrev;` |
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
|       714 | 1992 | `static sxi32 HashmapMergeSort(ph7_hashmap *pMap,ProcNodeCmp xCmp,void *pCmpData)` |
|         5 | 1993 | `{` |
|         - | 1994 | `	ph7_hashmap_node *a[N_SORT_BUCKET], *p,*pIn;` |
|         - | 1995 | `	sxu32 i;` |
|       719 | 1996 | `	SyZero(a,sizeof(a));` |
|         - | 1997 | `	/* Point to the first inserted entry */` |
|       719 | 1998 | `	pIn = pMap->pFirst;` |
|     14317 | 1999 | `	while( pIn ){` |
|     13603 | 2000 | `		p = pIn;` |
|     13603 | 2001 | `		pIn = p->pPrev;` |
|     13603 | 2002 | `		p->pPrev = 0;` |
|     25947 | 2003 | `		for(i=0; i<N_SORT_BUCKET-1; i++){` |
|     25947 | 2004 | `			if( a[i]==0 ){` |
|     13603 | 2005 | `				a[i] = p;` |
|     13603 | 2006 | `				break;` |
|       ! 0 | 2007 | `			}else{` |
|     12349 | 2008 | `				p = HashmapNodeMerge(a[i],p,xCmp,pCmpData);` |
|     12349 | 2009 | `				a[i] = 0;` |
|         - | 2010 | `			}` |
|      6177 | 2011 | `		}` |
|     13603 | 2012 | `		if( i==N_SORT_BUCKET-1 ){` |
|         - | 2013 | `			/* To get here, there need to be 2^(N_SORT_BUCKET) elements in he input list.` |
|         - | 2014 | `			 * But that is impossible.` |
|         - | 2015 | `			 */` |
|       ! 0 | 2016 | `			a[i] = HashmapNodeMerge(a[i], p,xCmp,pCmpData);` |
|       ! 0 | 2017 | `		}` |
|         5 | 2018 | `	}` |
|       719 | 2019 | `	p = a[0];` |
|     22853 | 2020 | `	for(i=1; i<N_SORT_BUCKET; i++){` |
|     22139 | 2021 | `		p = HashmapNodeMerge(p,a[i],xCmp,pCmpData);` |
|     11072 | 2022 | `	}` |
|       719 | 2023 | `	p->pNext = 0;` |
|         - | 2024 | `	/* Reflect the change */` |
|       719 | 2025 | `	pMap->pFirst = p;` |
|         - | 2026 | `	/* Reset the loop cursor */` |
|       719 | 2027 | `	pMap->pCur = pMap->pFirst;` |
|       719 | 2028 | `	return SXRET_OK;` |
|         5 | 2029 | `}` |
|         - | 2030 | `/* SPDX-SnippetEnd */` |
|         - | 2031 | `/*` |
|         - | 2032 | ` * Node comparison callback.` |
|         - | 2033 | ` * used-by: [sort(),asort(),...]` |
|         - | 2034 | ` */` |
|     70106 | 2035 | `static sxi32 HashmapCmpCallback1(ph7_hashmap_node *pA,ph7_hashmap_node *pB,void *pCmpData)` |
|         5 | 2036 | `{` |
|         - | 2037 | `	ph7_value sA,sB;` |
|         - | 2038 | `	sxi32 iFlags;` |
|         - | 2039 | `	int rc;` |
|     70111 | 2040 | `	if( pCmpData == 0 ){` |
|         - | 2041 | `		/* Perform a standard comparison */` |
|     70087 | 2042 | `		rc = HashmapNodeCmp(pA,pB,FALSE);` |
|     70087 | 2043 | `		return rc;` |
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
|     35070 | 2081 | `}` |
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
|        18 | 2319 | `static sxi32 HashmapCmpCallback7(ph7_hashmap_node *pA,ph7_hashmap_node *pB,void *pCmpData)` |
|         1 | 2320 | `{` |
|         - | 2321 | `	sxu32 n;` |
|         8 | 2322 | `	SXUNUSED(pB); /* cc warning */` |
|         8 | 2323 | `	SXUNUSED(pCmpData);` |
|         - | 2324 | `	/* Grab a random number */` |
|        19 | 2325 | `	n = PH7_VmRandomNum(pA->pMap->pVm);` |
|         - | 2326 | `	/* if the random number is odd then the first node 'pA' is greater then` |
|         - | 2327 | `	 * the second node 'pB'. Otherwise the reverse is assumed.` |
|         - | 2328 | `	 */` |
|        19 | 2329 | `	return n&1 ? 1 : -1;` |
|         1 | 2330 | `}` |
|         - | 2331 | `/*` |
|         - | 2332 | ` * Rehash all nodes keys after a merge-sort have been applied.` |
|         - | 2333 | ` * Used by [sort(),usort() and rsort()].` |
|         - | 2334 | ` */` |
|       664 | 2335 | `static void HashmapSortRehash(ph7_hashmap *pMap)` |
|         5 | 2336 | `{` |
|         - | 2337 | `	ph7_hashmap_node *p,*pLast;` |
|         - | 2338 | `	sxu32 i;` |
|         - | 2339 | `	/* Rehash all entries */` |
|       669 | 2340 | `	pLast = p = pMap->pFirst;` |
|       669 | 2341 | `	pMap->iNextIdx = 0; /* Reset the automatic index */` |
|       669 | 2342 | `	i = 0;` |
|      7044 | 2343 | `	for( ;; ){` |
|     14093 | 2344 | `		if( i >= pMap->nEntry ){` |
|       669 | 2345 | `			pMap->pLast = pLast; /* Fix the last link broken by the merge-sort */` |
|       669 | 2346 | `			break;` |
|         - | 2347 | `		}` |
|     13429 | 2348 | `		if( p->iType == HASHMAP_BLOB_NODE ){` |
|         - | 2349 | `			/* Do not maintain index association as requested by the PHP specification */` |
|         5 | 2350 | `			SyBlobRelease(&p->xKey.sKey);` |
|         - | 2351 | `			/* Change key type */` |
|         5 | 2352 | `			p->iType = HASHMAP_INT_NODE;` |
|         2 | 2353 | `		}` |
|     13429 | 2354 | `		HashmapRehashIntNode(p);` |
|         - | 2355 | `		/* Point to the next entry */` |
|     13429 | 2356 | `		i++;` |
|     13429 | 2357 | `		pLast = p;` |
|     13429 | 2358 | `		p = p->pPrev; /* Reverse link */` |
|         5 | 2359 | `	}` |
|       669 | 2360 | `}` |
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
|       992 | 2382 | `static int ph7_hashmap_sort(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|         5 | 2383 | `{` |
|         - | 2384 | `	ph7_hashmap *pMap;` |
|         - | 2385 | `	/* Make sure we are dealing with a valid hashmap */` |
|       997 | 2386 | `	if( nArg < 1 \|\| !ph7_value_is_array(apArg[0]) ){` |
|         - | 2387 | `		/* Missing/Invalid arguments,return FALSE */` |
|       ! 0 | 2388 | `		ph7_result_bool(pCtx,0);` |
|       ! 0 | 2389 | `		return PH7_OK;` |
|         - | 2390 | `	}` |
|         - | 2391 | `	/* Point to the internal representation of the input hashmap */` |
|       997 | 2392 | `	PH7_HashmapCowSeparate(pCtx->pVm, apArg[0]);` |
|       997 | 2393 | `	pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|       997 | 2394 | `	if( pMap->nEntry > 1 ){` |
|       647 | 2395 | `		sxi32 iCmpFlags = 0;` |
|       647 | 2396 | `		if( nArg > 1 ){` |
|         - | 2397 | `			/* Extract comparison flags */` |
|         3 | 2398 | `			iCmpFlags = ph7_value_to_int(apArg[1]);` |
|         3 | 2399 | `			if( iCmpFlags == 3 /* SORT_REGULAR */ ){` |
|       ! 0 | 2400 | `				iCmpFlags = 0; /* Standard comparison */` |
|       ! 0 | 2401 | `			}` |
|         1 | 2402 | `		}` |
|         - | 2403 | `		/* Do the merge sort */` |
|       647 | 2404 | `		HashmapMergeSort(pMap,HashmapCmpCallback1,SX_INT_TO_PTR(iCmpFlags));` |
|         - | 2405 | `		/* Rehash [Do not maintain index association as requested by the PHP specification] */` |
|       647 | 2406 | `		HashmapSortRehash(pMap);` |
|       321 | 2407 | `	}` |
|         - | 2408 | `	/* All done,return TRUE */` |
|       997 | 2409 | `	ph7_result_bool(pCtx,1);` |
|       997 | 2410 | `	return PH7_OK;` |
|       501 | 2411 | `}` |
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
|      1148 | 2872 | `static int ph7_hashmap_count(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|         5 | 2873 | `{` |
|      1153 | 2874 | `	int bRecursive = FALSE;` |
|      1153 | 2875 | `	int bCycleDetected = FALSE;` |
|         - | 2876 | `	sxi64 iCount;` |
|      1153 | 2877 | `	if( nArg < 1 ){` |
|         3 | 2878 | `		return PH7_VmThrowException(pCtx,` |
|         - | 2879 | `			"ArgumentCountError",` |
|         - | 2880 | `			"count() expects at least 1 argument, 0 given"` |
|         - | 2881 | `			);` |
|         - | 2882 | `	}` |
|      1151 | 2883 | `	if( nArg > 2 ){` |
|         4 | 2884 | `		return PH7_VmThrowException(pCtx,` |
|         - | 2885 | `			"ArgumentCountError",` |
|         - | 2886 | `			"count() expects at most 2 arguments, %d given",` |
|         1 | 2887 | `			nArg` |
|         - | 2888 | `			);` |
|         - | 2889 | `	}` |
|         - | 2890 | `	/* PHP validates $mode right after parsing, before the type dispatch, so` |
|         - | 2891 | `	 * an invalid mode raises ValueError whether $value is an array or a` |
|         - | 2892 | `	 * Countable object (the mode is then ignored for the Countable path). */` |
|      1149 | 2893 | `	if( nArg > 1 ){` |
|        45 | 2894 | `		sxi32 iMode = ph7_value_to_int(apArg[1]);` |
|        45 | 2895 | `		if( iMode != 0 /* COUNT_NORMAL */ && iMode != 1 /* COUNT_RECURSIVE */ ){` |
|        12 | 2896 | `			return PH7_VmThrowException(pCtx,` |
|         - | 2897 | `				"ValueError",` |
|         - | 2898 | `				"count(): Argument #2 ($mode) must be either COUNT_NORMAL or COUNT_RECURSIVE"` |
|         - | 2899 | `				);` |
|         - | 2900 | `		}` |
|        34 | 2901 | `		bRecursive = iMode == 1;` |
|        16 | 2902 | `	}` |
|      1141 | 2903 | `	if( !ph7_value_is_array(apArg[0]) ){` |
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
|      1109 | 2928 | `	iCount = HashmapCount((ph7_hashmap *)apArg[0]->x.pOther,bRecursive,&bCycleDetected);` |
|      1109 | 2929 | `	if( bCycleDetected ){` |
|         3 | 2930 | `		PH7_VmThrowError(pCtx->pVm,0,PH7_CTX_WARNING,"count(): Recursion detected");` |
|         1 | 2931 | `	}` |
|      1109 | 2932 | `	ph7_result_int64(pCtx,iCount);` |
|      1109 | 2933 | `	return PH7_OK;` |
|       579 | 2934 | `}` |
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
|        28 | 3177 | `static sxi32 HashmapCurrentValue(ph7_context *pCtx,ph7_hashmap *pMap,int iDirection)` |
|         1 | 3178 | `{` |
|        29 | 3179 | `	ph7_hashmap_node *pCur = pMap->pCur;` |
|         - | 3180 | `	ph7_value *pVal;` |
|        29 | 3181 | `	if( pCur == 0 ){` |
|         - | 3182 | `		/* Cursor does not point to anything,return FALSE */` |
|       ! 0 | 3183 | `		ph7_result_bool(pCtx,0);` |
|       ! 0 | 3184 | `		return PH7_OK;` |
|         - | 3185 | `	}` |
|        29 | 3186 | `	if( iDirection != 0 ){` |
|        11 | 3187 | `		if( iDirection > 0 ){` |
|         - | 3188 | `			/* Point to the next entry */` |
|         9 | 3189 | `			pMap->pCur = pCur->pPrev; /* Reverse link */` |
|         9 | 3190 | `			pCur = pMap->pCur;` |
|         5 | 3191 | `		}else{` |
|         - | 3192 | `			/* Point to the previous entry */` |
|         3 | 3193 | `			pMap->pCur = pCur->pNext; /* Reverse link */` |
|         3 | 3194 | `			pCur = pMap->pCur;` |
|         - | 3195 | `		}` |
|        11 | 3196 | `		if( pCur == 0 ){` |
|         - | 3197 | `			/* End of input reached,return FALSE */` |
|       ! 0 | 3198 | `			ph7_result_bool(pCtx,0);` |
|       ! 0 | 3199 | `			return PH7_OK;` |
|         - | 3200 | `		}` |
|         5 | 3201 | `	}` |
|         - | 3202 | `	/* Point to the desired element */` |
|        29 | 3203 | `	pVal = HashmapExtractNodeValue(pCur);` |
|        29 | 3204 | `	if( pVal ){` |
|        29 | 3205 | `		ph7_result_value(pCtx,pVal);` |
|        15 | 3206 | `	}else{` |
|       ! 0 | 3207 | `		ph7_result_bool(pCtx,0);` |
|         - | 3208 | `	}` |
|        29 | 3209 | `	return PH7_OK;` |
|        15 | 3210 | `}` |
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
|        12 | 3222 | `static int ph7_hashmap_current(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|         1 | 3223 | `{` |
|        13 | 3224 | `	if( nArg < 1 ){` |
|         - | 3225 | `		/* Missing arguments,return FALSE */` |
|       ! 0 | 3226 | `		ph7_result_bool(pCtx,0);` |
|       ! 0 | 3227 | `		return PH7_OK;` |
|         - | 3228 | `	}` |
|         - | 3229 | `	/* Make sure we are dealing with a valid hashmap */` |
|        13 | 3230 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|         - | 3231 | `		/* Invalid argument,return FALSE */` |
|       ! 0 | 3232 | `		ph7_result_bool(pCtx,0);` |
|       ! 0 | 3233 | `		return PH7_OK;` |
|         - | 3234 | `	}` |
|        13 | 3235 | `	HashmapCurrentValue(&(*pCtx),(ph7_hashmap *)apArg[0]->x.pOther,0);` |
|        13 | 3236 | `	return PH7_OK;` |
|         7 | 3237 | `}` |
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
|         8 | 3248 | `static int ph7_hashmap_next(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|         1 | 3249 | `{` |
|         9 | 3250 | `	if( nArg < 1 ){` |
|         - | 3251 | `		/* Missing arguments,return FALSE */` |
|       ! 0 | 3252 | `		ph7_result_bool(pCtx,0);` |
|       ! 0 | 3253 | `		return PH7_OK;` |
|         - | 3254 | `	}` |
|         - | 3255 | `	/* Make sure we are dealing with a valid hashmap */` |
|         9 | 3256 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|         - | 3257 | `		/* Invalid argument,return FALSE */` |
|       ! 0 | 3258 | `		ph7_result_bool(pCtx,0);` |
|       ! 0 | 3259 | `		return PH7_OK;` |
|         - | 3260 | `	}` |
|         9 | 3261 | `	HashmapCurrentValue(&(*pCtx),(ph7_hashmap *)apArg[0]->x.pOther,1);` |
|         9 | 3262 | `	return PH7_OK;` |
|         5 | 3263 | `}` |
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
|         - | 3351 | ` * value key(array $array)` |
|         - | 3352 | ` *   Fetch a key from an array` |
|         - | 3353 | ` * Parameter` |
|         - | 3354 | ` *  $input` |
|         - | 3355 | ` *   The input array.` |
|         - | 3356 | ` * Return` |
|         - | 3357 | ` *  The key() function simply returns the key of the array element that's currently` |
|         - | 3358 | ` *  being pointed to by the internal pointer. It does not move the pointer in any way.` |
|         - | 3359 | ` *  If the internal pointer points beyond the end of the elements list or the array` |
|         - | 3360 | ` *  is empty, key() returns NULL.` |
|         - | 3361 | ` */` |
|         4 | 3362 | `static int ph7_hashmap_simple_key(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|         1 | 3363 | `{` |
|         - | 3364 | `	ph7_hashmap_node *pCur;` |
|         - | 3365 | `	ph7_hashmap *pMap;` |
|         5 | 3366 | `	if( nArg < 1 ){` |
|         - | 3367 | `		/* Missing arguments,return NULL */` |
|       ! 0 | 3368 | `		ph7_result_null(pCtx);` |
|       ! 0 | 3369 | `		return PH7_OK;` |
|         - | 3370 | `	}` |
|         - | 3371 | `	/* Make sure we are dealing with a valid hashmap */` |
|         5 | 3372 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|         - | 3373 | `		/* Invalid argument,return NULL */` |
|       ! 0 | 3374 | `		ph7_result_null(pCtx);` |
|       ! 0 | 3375 | `		return PH7_OK;` |
|         - | 3376 | `	}` |
|         5 | 3377 | `	pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|         5 | 3378 | `	pCur = pMap->pCur;` |
|         5 | 3379 | `	if( pCur == 0 ){` |
|         - | 3380 | `		/* Cursor does not point to anything,return NULL */` |
|       ! 0 | 3381 | `		ph7_result_null(pCtx);` |
|       ! 0 | 3382 | `		return PH7_OK;` |
|         - | 3383 | `	}` |
|         5 | 3384 | `	if( pCur->iType == HASHMAP_INT_NODE){` |
|         - | 3385 | `		/* Key is integer */` |
|       ! 0 | 3386 | `		ph7_result_int64(pCtx,pCur->xKey.iKey);` |
|       ! 0 | 3387 | `	}else{` |
|         - | 3388 | `		/* Key is blob */` |
|         7 | 3389 | `		ph7_result_string(pCtx,` |
|         4 | 3390 | `			(const char *)SyBlobData(&pCur->xKey.sKey),(int)SyBlobLength(&pCur->xKey.sKey));` |
|         - | 3391 | `	}` |
|         5 | 3392 | `	return PH7_OK;` |
|         3 | 3393 | `}` |
|         - | 3394 | `/*` |
|         - | 3395 | ` * array each(array $input)` |
|         - | 3396 | ` *  Return the current key and value pair from an array and advance the array cursor.` |
|         - | 3397 | ` * Parameter` |
|         - | 3398 | ` *  $input` |
|         - | 3399 | ` *    The input array.` |
|         - | 3400 | ` * Return` |
|         - | 3401 | ` *  Returns the current key and value pair from the array array. This pair is returned` |
|         - | 3402 | ` *  in a four-element array, with the keys 0, 1, key, and value. Elements 0 and key` |
|         - | 3403 | ` *  contain the key name of the array element, and 1 and value contain the data.` |
|         - | 3404 | ` *  If the internal pointer for the array points past the end of the array contents` |
|         - | 3405 | ` *  each() returns FALSE.` |
|         - | 3406 | ` */` |
|        22 | 3407 | `static int ph7_hashmap_each(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|         1 | 3408 | `{` |
|         - | 3409 | `	ph7_hashmap_node *pCur;` |
|         - | 3410 | `	ph7_hashmap *pMap;` |
|         - | 3411 | `	ph7_value *pArray;` |
|         - | 3412 | `	ph7_value *pVal;` |
|         - | 3413 | `	ph7_value sKey;` |
|        23 | 3414 | `	if( nArg < 1 ){` |
|         - | 3415 | `		/* Missing arguments,return FALSE */` |
|       ! 0 | 3416 | `		ph7_result_bool(pCtx,0);` |
|       ! 0 | 3417 | `		return PH7_OK;` |
|         - | 3418 | `	}` |
|         - | 3419 | `	/* Make sure we are dealing with a valid hashmap */` |
|        23 | 3420 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|         - | 3421 | `		/* Invalid argument,return FALSE */` |
|       ! 0 | 3422 | `		ph7_result_bool(pCtx,0);` |
|       ! 0 | 3423 | `		return PH7_OK;` |
|         - | 3424 | `	}` |
|         - | 3425 | `	/* Point to the internal representation that describe the input hashmap */` |
|        23 | 3426 | `	pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|        23 | 3427 | `	if( pMap->pCur == 0 ){` |
|         - | 3428 | `		/* Cursor does not point to anything,return FALSE */` |
|         9 | 3429 | `		ph7_result_bool(pCtx,0);` |
|         9 | 3430 | `		return PH7_OK;` |
|         - | 3431 | `	}` |
|        15 | 3432 | `	pCur = pMap->pCur;` |
|         - | 3433 | `	/* Create a new array */` |
|        15 | 3434 | `	pArray = ph7_context_new_array(pCtx);` |
|        15 | 3435 | `	if( pArray == 0 ){` |
|       ! 0 | 3436 | `		ph7_result_bool(pCtx,0);` |
|       ! 0 | 3437 | `		return PH7_OK;` |
|         - | 3438 | `	}` |
|        15 | 3439 | `	pVal = HashmapExtractNodeValue(pCur);` |
|         - | 3440 | `	/* Insert the current value */` |
|        15 | 3441 | `	ph7_array_add_intkey_elem(pArray,1,pVal);` |
|        15 | 3442 | `	ph7_array_add_strkey_elem(pArray,"value",pVal);` |
|         - | 3443 | `	/* Make the key */` |
|        15 | 3444 | `	if( pCur->iType == HASHMAP_INT_NODE ){` |
|         7 | 3445 | `		PH7_MemObjInitFromInt(pMap->pVm,&sKey,pCur->xKey.iKey);` |
|         4 | 3446 | `	}else{` |
|         9 | 3447 | `		PH7_MemObjInitFromString(pMap->pVm,&sKey,0);` |
|         9 | 3448 | `		PH7_MemObjStringAppend(&sKey,(const char *)SyBlobData(&pCur->xKey.sKey),SyBlobLength(&pCur->xKey.sKey));` |
|         - | 3449 | `	}` |
|         - | 3450 | `	/* Insert the current key */` |
|        15 | 3451 | `	ph7_array_add_intkey_elem(pArray,0,&sKey);` |
|        15 | 3452 | `	ph7_array_add_strkey_elem(pArray,"key",&sKey);` |
|        15 | 3453 | `	PH7_MemObjRelease(&sKey);` |
|         - | 3454 | `	/* Advance the cursor */` |
|        15 | 3455 | `	pMap->pCur = pCur->pPrev; /* Reverse link */` |
|         - | 3456 | `	/* Return the current entry */` |
|        15 | 3457 | `	ph7_result_value(pCtx,pArray);` |
|        15 | 3458 | `	return PH7_OK;` |
|        12 | 3459 | `}` |
|         - | 3460 | `/*` |
|         - | 3461 | ` * range() — a faithful port of php 8.5's ext/standard/array.c implementation` |
|         - | 3462 | ` * (php_range_process_input + PHP_FUNCTION(range)), so the value semantics,` |
|         - | 3463 | ` * diagnostics, and their ordering are byte-exact: decreasing ranges, float` |
|         - | 3464 | ` * ranges, character ranges, the step/endpoint ValueErrors, the ZPP TypeErrors` |
|         - | 3465 | ` * and null deprecations, and the string-endpoint warnings.` |
|         - | 3466 | ` */` |
|         - | 3467 | `#define PH7_RANGE_HT_MAX_SIZE 1073741824 /* php's HT_MAX_SIZE (2^30 entries) */` |
|         - | 3468 | `/*` |
|         - | 3469 | ` * Endpoint classification, mirroring php_range_process_input's return` |
|         - | 3470 | ` * contract. php returns zval type tags whose ORDER encodes the logic` |
|         - | 3471 | ` * (IS_LONG < IS_DOUBLE < IS_STRING < IS_ARRAY); the >=/< comparisons in` |
|         - | 3472 | ` * ph7_hashmap_range depend on the same ordering here.` |
|         - | 3473 | ` *   RANGE_IN_LONG/DOUBLE : only interpretable as int / float` |
|         - | 3474 | ` *   RANGE_IN_STRING      : only interpretable as a (char-range) string` |
|         - | 3475 | ` *   RANGE_IN_DIGIT       : single-byte numeric string — valid as both a char` |
|         - | 3476 | ` *                          and a number (php returns IS_ARRAY for this)` |
|         - | 3477 | ` * The RANGE_IN_* codes and RangeStrToNumber are declared in ph7int.h so the` |
|         - | 3478 | ` * stage-2 ZPP domain-error sweep can reuse the classifier (PLAN §3.9(a)).` |
|         - | 3479 | ` */` |
|         - | 3480 | `/* IEEE special-value tests: the engine-wide bit-pattern macros from` |
|         - | 3481 | ` * sxtypes.h (via ph7int.h) — same ones the printf/serialize paths use. */` |
|         - | 3482 | `/*` |
|         - | 3483 | ` * The type name php's ZPP prints after "must be of type ..., X given":` |
|         - | 3484 | ` * the concrete class name for objects, the usual type name otherwise.` |
|         - | 3485 | ` */` |
|         8 | 3486 | `static const char * RangeArgTypeName(ph7_value *pVal,char *zBuf,sxu32 nBufLen)` |
|         1 | 3487 | `{` |
|         9 | 3488 | `	if( pVal->iFlags & MEMOBJ_OBJ ){` |
|         3 | 3489 | `		ph7_class_instance *pThis = (ph7_class_instance *)pVal->x.pOther;` |
|         3 | 3490 | `		sxu32 n = SXMIN(pThis->pClass->sName.nByte,nBufLen - 1);` |
|         3 | 3491 | `		SyMemcpy((const void *)pThis->pClass->sName.zString,zBuf,n);` |
|         3 | 3492 | `		zBuf[n] = 0;` |
|         3 | 3493 | `		return zBuf;` |
|         - | 3494 | `	}` |
|         7 | 3495 | `	return ph7_type_name(pVal);` |
|         5 | 3496 | `}` |
|         - | 3497 | `/*` |
|         - | 3498 | ` * Classify a string with php's is_numeric_string() grammar:` |
|         - | 3499 | ` *   [ws] [sign] ( D+ [ . D* ] \| . D+ ) [ (e\|E) [sign] D+ ] [ws]` |
|         - | 3500 | ` * — the whole string must be consumed; hex/binary/"INF"/"NAN" are NOT` |
|         - | 3501 | ` * numeric. Returns RANGE_IN_LONG with *pLong set, RANGE_IN_DOUBLE with` |
|         - | 3502 | ` * *pDouble set (a fractional/exponent form, or an integer too wide for an` |
|         - | 3503 | ` * sxi64 — php reclassifies those as float), or RANGE_IN_ERROR when the` |
|         - | 3504 | ` * string is not numeric. The float value comes from libc strtod, like` |
|         - | 3505 | ` * php's zend_strtod (byte-exact-floats rule). zIn must be NUL-terminated` |
|         - | 3506 | ` * at zIn[nLen] — ph7_value_to_string guarantees this (SyBlobNullAppend) —` |
|         - | 3507 | ` * so strtod can parse it in place once the grammar has validated it.` |
|         - | 3508 | ` */` |
|       156 | 3509 | `PH7_PRIVATE sxu8 RangeStrToNumber(const char *zIn,sxu32 nLen,sxi64 *pLong,double *pDouble)` |
|         1 | 3510 | `{` |
|       157 | 3511 | `	const char *z = zIn,*zEnd = &zIn[nLen];` |
|       157 | 3512 | `	sxu64 uVal = 0;` |
|       157 | 3513 | `	int bNeg = 0,bDigit = 0,bReal = 0,bOverflow = 0;` |
|       167 | 3514 | `	while( z < zEnd && (unsigned char)z[0] < 0xc0 && SyisSpace(z[0]) ){ z++; }` |
|       157 | 3515 | `	if( z < zEnd && (z[0] == '+' \|\| z[0] == '-') ){` |
|         3 | 3516 | `		bNeg = (z[0] == '-');` |
|         3 | 3517 | `		z++;` |
|         1 | 3518 | `	}` |
|       237 | 3519 | `	while( z < zEnd && (unsigned char)z[0] < 0xc0 && SyisDigit(z[0]) ){` |
|        81 | 3520 | `		int d = z[0] - '0';` |
|         - | 3521 | `		/* Track overflow past 2^63, the widest magnitude an sxi64 can carry` |
|         - | 3522 | `		 * (as LONG_MIN); overflowing integers become floats like in php. */` |
|        81 | 3523 | `		if( uVal > 922337203685477580ULL \|\| (uVal == 922337203685477580ULL && d > 8) ){` |
|       ! 0 | 3524 | `			bOverflow = 1;` |
|       ! 0 | 3525 | `		}else{` |
|        81 | 3526 | `			uVal = uVal * 10 + (sxu64)d;` |
|         - | 3527 | `		}` |
|        81 | 3528 | `		bDigit = 1;` |
|        81 | 3529 | `		z++;` |
|         1 | 3530 | `	}` |
|       157 | 3531 | `	if( z < zEnd && z[0] == '.' ){` |
|         3 | 3532 | `		bReal = 1;` |
|         3 | 3533 | `		z++;` |
|         5 | 3534 | `		while( z < zEnd && (unsigned char)z[0] < 0xc0 && SyisDigit(z[0]) ){` |
|         3 | 3535 | `			bDigit = 1;` |
|         3 | 3536 | `			z++;` |
|         1 | 3537 | `		}` |
|         1 | 3538 | `	}` |
|         - | 3539 | `	/* At least one mantissa digit required (rejects "", ".", "+", "e5"). */` |
|       157 | 3540 | `	if( !bDigit ){` |
|        61 | 3541 | `		return RANGE_IN_ERROR;` |
|         - | 3542 | `	}` |
|         - | 3543 | `	/* Optional exponent — needs at least one digit (rejects "1e", "1e+"). */` |
|        97 | 3544 | `	if( z < zEnd && (z[0] == 'e' \|\| z[0] == 'E') ){` |
|         9 | 3545 | `		z++;` |
|         9 | 3546 | `		if( z < zEnd && (z[0] == '+' \|\| z[0] == '-') ){ z++; }` |
|         9 | 3547 | `		if( z >= zEnd \|\| (unsigned char)z[0] >= 0xc0 \|\| !SyisDigit(z[0]) ){` |
|       ! 0 | 3548 | `			return RANGE_IN_ERROR;` |
|         - | 3549 | `		}` |
|         9 | 3550 | `		bReal = 1;` |
|        17 | 3551 | `		while( z < zEnd && (unsigned char)z[0] < 0xc0 && SyisDigit(z[0]) ){ z++; }` |
|         4 | 3552 | `	}` |
|         - | 3553 | `	/* Trailing whitespace allowed; anything else means not numeric. */` |
|       101 | 3554 | `	while( z < zEnd && (unsigned char)z[0] < 0xc0 && SyisSpace(z[0]) ){ z++; }` |
|        97 | 3555 | `	if( z != zEnd ){` |
|        13 | 3556 | `		return RANGE_IN_ERROR;` |
|         - | 3557 | `	}` |
|        84 | 3558 | `	if( bOverflow \|\| (!bNeg && uVal > (sxu64)LARGEST_INT64)` |
|        43 | 3559 | `	 \|\| (bNeg && uVal > (sxu64)LARGEST_INT64 + 1) ){` |
|        84 | 3560 | `		bReal = 1;` |
|        84 | 3561 | `	}` |
|        43 | 3562 | `	if( bReal ){` |
|        11 | 3563 | `		*pDouble = strtod(zIn,0);` |
|        11 | 3564 | `		return RANGE_IN_DOUBLE;` |
|         - | 3565 | `	}` |
|         - | 3566 | `	/* Negate in unsigned space so 2^63 lands on LONG_MIN without overflow. */` |
|        33 | 3567 | `	*pLong = bNeg ? (sxi64)((sxu64)0 - uVal) : (sxi64)uVal;` |
|        33 | 3568 | `	return RANGE_IN_LONG;` |
|        58 | 3569 | `}` |
|         - | 3570 | `/*` |
|         - | 3571 | ` * ZPP emulation for $start/$end (php's Z_PARAM_NUMBER_OR_STR, weak mode):` |
|         - | 3572 | ` * reject array/object/resource with php's TypeError, deprecate null (the` |
|         - | 3573 | ` * value then reads as int 0 — *pbNullCoerced). php runs this for all` |
|         - | 3574 | ` * arguments BEFORE any value/domain check, hence the split from` |
|         - | 3575 | ` * RangeProcessInput below. Returns FALSE after throwing (*pRc set).` |
|         - | 3576 | ` */` |
|       262 | 3577 | `static int RangeEndpointZpp(ph7_context *pCtx,ph7_value *pIn,int iArg,const char *zName,int *pbNullCoerced,sxi32 *pRc)` |
|         1 | 3578 | `{` |
|         - | 3579 | `	char zMsg[160];` |
|       263 | 3580 | `	*pRc = PH7_OK;` |
|       263 | 3581 | `	if( pIn->iFlags & (MEMOBJ_HASHMAP\|MEMOBJ_OBJ\|MEMOBJ_RES) ){` |
|         - | 3582 | `		char zType[80];` |
|        10 | 3583 | `		*pRc = PH7_VmThrowException(pCtx,"TypeError",` |
|         - | 3584 | `			"range(): Argument #%d ($%s) must be of type string\|int\|float, %s given",` |
|         3 | 3585 | `			iArg,zName,RangeArgTypeName(pIn,zType,sizeof(zType)));` |
|         7 | 3586 | `		return FALSE;` |
|         - | 3587 | `	}` |
|       257 | 3588 | `	if( pIn->iFlags & MEMOBJ_NULL ){` |
|         7 | 3589 | `		SyBufferFormat(zMsg,sizeof(zMsg),` |
|         - | 3590 | `			"range(): Passing null to parameter #%d ($%s) of type string\|int\|float is deprecated",` |
|         2 | 3591 | `			iArg,zName);` |
|         5 | 3592 | `		PH7_VmThrowError(pCtx->pVm,0,E_DEPRECATED,zMsg);` |
|         5 | 3593 | `		*pbNullCoerced = TRUE;` |
|         2 | 3594 | `	}` |
|       257 | 3595 | `	return TRUE;` |
|       132 | 3596 | `}` |
|         - | 3597 | `/*` |
|         - | 3598 | ` * ZPP emulation for $step (php's Z_PARAM_NUMBER, weak mode): int/float pass` |
|         - | 3599 | ` * through, bool coerces to int, null deprecates to int 0 (which then trips` |
|         - | 3600 | ` * the "cannot be 0" ValueError like php), a numeric string coerces to its` |
|         - | 3601 | ` * number, anything else is a TypeError. Returns RANGE_IN_LONG/DOUBLE, or` |
|         - | 3602 | ` * RANGE_IN_ERROR after throwing (*pRc set).` |
|         - | 3603 | ` */` |
|        62 | 3604 | `static sxu8 RangeStepInput(ph7_context *pCtx,ph7_value *pIn,sxi64 *pLong,double *pDouble,sxi32 *pRc)` |
|         1 | 3605 | `{` |
|        63 | 3606 | `	*pRc = PH7_OK;` |
|        63 | 3607 | `	if( pIn->iFlags & (MEMOBJ_HASHMAP\|MEMOBJ_OBJ\|MEMOBJ_RES) ){` |
|         - | 3608 | `		char zType[80];` |
|         4 | 3609 | `		*pRc = PH7_VmThrowException(pCtx,"TypeError",` |
|         - | 3610 | `			"range(): Argument #3 ($step) must be of type int\|float, %s given",` |
|         1 | 3611 | `			RangeArgTypeName(pIn,zType,sizeof(zType)));` |
|         3 | 3612 | `		return RANGE_IN_ERROR;` |
|         - | 3613 | `	}` |
|        61 | 3614 | `	if( pIn->iFlags & MEMOBJ_NULL ){` |
|         3 | 3615 | `		PH7_VmThrowError(pCtx->pVm,0,E_DEPRECATED,` |
|         - | 3616 | `			"range(): Passing null to parameter #3 ($step) of type int\|float is deprecated");` |
|         3 | 3617 | `		*pLong = 0;` |
|         3 | 3618 | `		return RANGE_IN_LONG;` |
|         - | 3619 | `	}` |
|        59 | 3620 | `	if( pIn->iFlags & MEMOBJ_REAL ){` |
|        25 | 3621 | `		*pDouble = ph7_value_to_double(pIn);` |
|        25 | 3622 | `		return RANGE_IN_DOUBLE;` |
|         - | 3623 | `	}` |
|        35 | 3624 | `	if( pIn->iFlags & MEMOBJ_STRING ){` |
|         - | 3625 | `		const char *zStr;` |
|         - | 3626 | `		int nLen;` |
|         - | 3627 | `		sxu8 iKind;` |
|         3 | 3628 | `		zStr = ph7_value_to_string(pIn,&nLen);` |
|         3 | 3629 | `		iKind = RangeStrToNumber(zStr,(sxu32)nLen,pLong,pDouble);` |
|         3 | 3630 | `		if( iKind == RANGE_IN_ERROR ){` |
|         3 | 3631 | `			*pRc = PH7_VmThrowException(pCtx,"TypeError",` |
|         - | 3632 | `				"range(): Argument #3 ($step) must be of type int\|float, string given");` |
|         1 | 3633 | `		}` |
|         3 | 3634 | `		return iKind;` |
|         - | 3635 | `	}` |
|         - | 3636 | `	/* int / bool */` |
|        33 | 3637 | `	*pLong = ph7_value_to_int64(pIn);` |
|        33 | 3638 | `	return RANGE_IN_LONG;` |
|        32 | 3639 | `}` |
|         - | 3640 | `/*` |
|         - | 3641 | ` * php_range_process_input port: resolve $start/$end into a number and/or a` |
|         - | 3642 | ` * char-range byte, emitting php's exact warnings (empty string, multi-byte` |
|         - | 3643 | ` * string) and ValueErrors (INF/NAN). Returns a RANGE_IN_* code, or` |
|         - | 3644 | ` * RANGE_IN_ERROR after throwing (*pRc set).` |
|         - | 3645 | ` */` |
|       220 | 3646 | `static sxu8 RangeProcessInput(ph7_context *pCtx,ph7_value *pIn,int iArg,const char *zName,` |
|         - | 3647 | `	int bNullCoerced,sxi64 *pLong,double *pDouble,unsigned char *pChar,sxi32 *pRc)` |
|         1 | 3648 | `{` |
|         - | 3649 | `	char zMsg[160];` |
|         - | 3650 | `	double r;` |
|       221 | 3651 | `	*pRc = PH7_OK;` |
|       221 | 3652 | `	if( bNullCoerced ){` |
|         - | 3653 | `		/* ZPP already deprecated the null; it reads as int 0. */` |
|         5 | 3654 | `		*pLong = 0;` |
|         5 | 3655 | `		*pDouble = 0.0;` |
|         5 | 3656 | `		return RANGE_IN_LONG;` |
|         - | 3657 | `	}` |
|       217 | 3658 | `	if( pIn->iFlags & MEMOBJ_REAL ){` |
|        21 | 3659 | `		r = ph7_value_to_double(pIn);` |
|        12 | 3660 | `check_dval:` |
|        25 | 3661 | `		if( PH7_IS_INF(r) ){` |
|         7 | 3662 | `			*pRc = PH7_VmThrowException(pCtx,"ValueError",` |
|         2 | 3663 | `				"range(): Argument #%d ($%s) must be a finite number, INF provided",iArg,zName);` |
|         5 | 3664 | `			return RANGE_IN_ERROR;` |
|         - | 3665 | `		}` |
|        21 | 3666 | `		if( PH7_IS_NAN(r) ){` |
|         7 | 3667 | `			*pRc = PH7_VmThrowException(pCtx,"ValueError",` |
|         2 | 3668 | `				"range(): Argument #%d ($%s) must be a finite number, NAN provided",iArg,zName);` |
|         5 | 3669 | `			return RANGE_IN_ERROR;` |
|         - | 3670 | `		}` |
|        17 | 3671 | `		*pDouble = r;` |
|        17 | 3672 | `		return RANGE_IN_DOUBLE;` |
|         - | 3673 | `	}` |
|       197 | 3674 | `	if( pIn->iFlags & MEMOBJ_STRING ){` |
|         - | 3675 | `		const char *zStr;` |
|         - | 3676 | `		int nLen;` |
|         - | 3677 | `		sxu8 iKind;` |
|        81 | 3678 | `		zStr = ph7_value_to_string(pIn,&nLen);` |
|        81 | 3679 | `		if( nLen == 0 ){` |
|         7 | 3680 | `			SyBufferFormat(zMsg,sizeof(zMsg),` |
|         2 | 3681 | `				"range(): Argument #%d ($%s) must not be empty, casted to 0",iArg,zName);` |
|         5 | 3682 | `			PH7_VmThrowError(pCtx->pVm,0,PH7_CTX_WARNING,zMsg);` |
|         5 | 3683 | `			*pLong = 0;` |
|         5 | 3684 | `			*pDouble = 0.0;` |
|        41 | 3685 | `			return RANGE_IN_LONG;` |
|         - | 3686 | `		}` |
|        77 | 3687 | `		iKind = RangeStrToNumber(zStr,(sxu32)nLen,pLong,pDouble);` |
|        77 | 3688 | `		if( iKind == RANGE_IN_DOUBLE ){` |
|         5 | 3689 | `			r = *pDouble;` |
|         5 | 3690 | `			goto check_dval;` |
|         - | 3691 | `		}` |
|        73 | 3692 | `		if( iKind == RANGE_IN_LONG ){` |
|        23 | 3693 | `			*pDouble = (double)*pLong;` |
|        23 | 3694 | `			if( nLen == 1 ){` |
|         - | 3695 | `				/* A single numeric digit works as both a char and a number. */` |
|         9 | 3696 | `				*pChar = (unsigned char)zStr[0];` |
|         9 | 3697 | `				return RANGE_IN_DIGIT;` |
|         - | 3698 | `			}` |
|        15 | 3699 | `			return RANGE_IN_LONG;` |
|         - | 3700 | `		}` |
|        51 | 3701 | `		if( nLen != 1 ){` |
|        10 | 3702 | `			SyBufferFormat(zMsg,sizeof(zMsg),` |
|         3 | 3703 | `				"range(): Argument #%d ($%s) must be a single byte, subsequent bytes are ignored",iArg,zName);` |
|         7 | 3704 | `			PH7_VmThrowError(pCtx->pVm,0,PH7_CTX_WARNING,zMsg);` |
|         3 | 3705 | `		}` |
|        51 | 3706 | `		*pChar = (unsigned char)zStr[0];` |
|         - | 3707 | `		/* Fall-back numeric value in case the other argument is not a string. */` |
|        51 | 3708 | `		*pLong = 0;` |
|        51 | 3709 | `		*pDouble = 0.0;` |
|        51 | 3710 | `		return RANGE_IN_STRING;` |
|         - | 3711 | `	}` |
|         - | 3712 | `	/* int / bool */` |
|       117 | 3713 | `	*pLong = ph7_value_to_int64(pIn);` |
|       117 | 3714 | `	*pDouble = (double)*pLong;` |
|       117 | 3715 | `	return RANGE_IN_LONG;` |
|       111 | 3716 | `}` |
|         - | 3717 | `/*` |
|         - | 3718 | ` * The two "supplied range exceeds the maximum array size" ValueErrors.` |
|         - | 3719 | ` * Both php messages print the macro's (start,end) parameters, which its` |
|         - | 3720 | ` * callers pass SWAPPED for a decreasing range — a php quirk kept for` |
|         - | 3721 | ` * byte-parity (callers below pass the values to *print*). The int and` |
|         - | 3722 | ` * float variants differ in wording ("Maximum size: N." vs "Max size: N")` |
|         - | 3723 | ` * exactly like php's two macros.` |
|         - | 3724 | ` */` |
|         6 | 3725 | `static sxi32 RangeLongSizeError(ph7_context *pCtx,sxu64 nCalc,sxi64 iStart,sxi64 iEnd,sxi64 iStep)` |
|         1 | 3726 | `{` |
|        10 | 3727 | `	return PH7_VmThrowException(pCtx,"ValueError",` |
|         - | 3728 | `		"The supplied range exceeds the maximum array size by %qu elements: "` |
|         - | 3729 | `		"start=%qd, end=%qd, step=%qd. Calculated size: %qu. Maximum size: %qu.",` |
|         3 | 3730 | `		nCalc - (sxu64)(PH7_RANGE_HT_MAX_SIZE - 1),iStart,iEnd,iStep,` |
|         3 | 3731 | `		nCalc,(sxu64)PH7_RANGE_HT_MAX_SIZE);` |
|         1 | 3732 | `}` |
|         6 | 3733 | `static sxi32 RangeDoubleSizeError(ph7_context *pCtx,double rCalc,double rStart,double rEnd,double rStep)` |
|         1 | 3734 | `{` |
|         - | 3735 | `	/* Four %.1f doubles can reach ~313 bytes each near DBL_MAX, so format on` |
|         - | 3736 | `	 * the VM heap (auto-released with the call context) rather than parking` |
|         - | 3737 | `	 * ~1.5 KB on the native stack of a small-stack embedded port. */` |
|         7 | 3738 | `	const unsigned int nBuf = 1500;` |
|         7 | 3739 | `	char *zMsg = (char *)ph7_context_alloc_chunk(pCtx,nBuf,FALSE,TRUE/* Auto-release */);` |
|         7 | 3740 | `	if( zMsg == 0 ){` |
|       ! 0 | 3741 | `		return PH7_ContextMemoryError(pCtx);` |
|         - | 3742 | `	}` |
|         7 | 3743 | `	snprintf(zMsg,nBuf,` |
|         - | 3744 | `		"The supplied range exceeds the maximum array size by %.1f elements: "` |
|         - | 3745 | `		"start=%.1f, end=%.1f, step=%.1f. Max size: 1073741824",` |
|         - | 3746 | `		rCalc - (double)PH7_RANGE_HT_MAX_SIZE,rStart,rEnd,rStep);` |
|         7 | 3747 | `	return PH7_VmThrowException(pCtx,"ValueError","%s",zMsg);` |
|         4 | 3748 | `}` |
|         - | 3749 | `/*` |
|         - | 3750 | ` * Set the element container to the next range element and append it to the` |
|         - | 3751 | ` * result array, surfacing allocation failure as the OOM fatal (never a` |
|         - | 3752 | ` * silently-truncated array). One helper per element type so the fill loops` |
|         - | 3753 | ` * below stay one line per iteration.` |
|         - | 3754 | ` */` |
|       334 | 3755 | `static sxi32 RangeAppendInt(ph7_context *pCtx,ph7_value *pArray,ph7_value *pValue,sxi64 iVal)` |
|         1 | 3756 | `{` |
|       335 | 3757 | `	ph7_value_int64(pValue,iVal);` |
|       335 | 3758 | `	if( ph7_array_add_elem(pArray,0/* Automatic index assign*/,pValue) != SXRET_OK ){` |
|       ! 0 | 3759 | `		return PH7_ContextMemoryError(pCtx);` |
|         - | 3760 | `	}` |
|       335 | 3761 | `	return PH7_OK;` |
|       168 | 3762 | `}` |
|        70 | 3763 | `static sxi32 RangeAppendDouble(ph7_context *pCtx,ph7_value *pArray,ph7_value *pValue,double rVal)` |
|         1 | 3764 | `{` |
|        71 | 3765 | `	ph7_value_double(pValue,rVal);` |
|        71 | 3766 | `	if( ph7_array_add_elem(pArray,0,pValue) != SXRET_OK ){` |
|       ! 0 | 3767 | `		return PH7_ContextMemoryError(pCtx);` |
|         - | 3768 | `	}` |
|        71 | 3769 | `	return PH7_OK;` |
|        36 | 3770 | `}` |
|       168 | 3771 | `static sxi32 RangeAppendChar(ph7_context *pCtx,ph7_value *pArray,ph7_value *pValue,char c)` |
|         1 | 3772 | `{` |
|       169 | 3773 | `	ph7_value_string(pValue,&c,1);` |
|       169 | 3774 | `	if( ph7_array_add_elem(pArray,0,pValue) != SXRET_OK ){` |
|       ! 0 | 3775 | `		return PH7_ContextMemoryError(pCtx);` |
|         - | 3776 | `	}` |
|       169 | 3777 | `	ph7_value_reset_string_cursor(pValue);` |
|       169 | 3778 | `	return PH7_OK;` |
|        85 | 3779 | `}` |
|         - | 3780 | `/*` |
|         - | 3781 | ` * array range(string\|int\|float $start,string\|int\|float $end,int\|float $step = 1)` |
|         - | 3782 | ` *  Create an array containing a range of elements.` |
|         - | 3783 | ` * Return` |
|         - | 3784 | ` *  An array of elements from start to end, inclusive; int, float, or` |
|         - | 3785 | ` *  single-character string elements depending on the inputs, like php 8.` |
|         - | 3786 | ` */` |
|       136 | 3787 | `static int ph7_hashmap_range(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|         1 | 3788 | `{` |
|         - | 3789 | `	ph7_value *pValue,*pArray;` |
|       137 | 3790 | `	sxi32 rc = PH7_OK;` |
|       137 | 3791 | `	int is_step_double = 0,is_step_negative = 0;` |
|       137 | 3792 | `	double step_double = 1.0;` |
|       137 | 3793 | `	sxi64 step = 1;` |
|         - | 3794 | `	sxu8 start_type,end_type;` |
|       137 | 3795 | `	sxi64 start_long = 0,end_long = 0;` |
|       137 | 3796 | `	double start_double = 0.0,end_double = 0.0;` |
|       137 | 3797 | `	unsigned char cStart = 0,cEnd = 0;` |
|       137 | 3798 | `	int bStartNull = FALSE,bEndNull = FALSE;` |
|         - | 3799 | `	sxu32 i,size;` |
|         - | 3800 |  |
|         - | 3801 | `	/* php ZPP arity: at least 2 (enforced centrally, aBuiltinArity), at most 3. */` |
|       137 | 3802 | `	if( nArg > 3 ){` |
|         4 | 3803 | `		return PH7_VmThrowException(pCtx,"ArgumentCountError",` |
|         1 | 3804 | `			"range() expects at most 3 arguments, %d given",nArg);` |
|         - | 3805 | `	}` |
|       135 | 3806 | `	if( nArg < 2 ){` |
|         - | 3807 | `		/* Defensive only: the central arity table throws before we run. */` |
|       ! 0 | 3808 | `		return PH7_VmThrowException(pCtx,"ArgumentCountError",` |
|       ! 0 | 3809 | `			"range() expects at least 2 arguments, %d given",nArg);` |
|         - | 3810 | `	}` |
|         - | 3811 | `	/* ZPP pass in argument order: type errors and null deprecations fire` |
|         - | 3812 | `	 * before any value/domain check, like php's zend_parse_parameters. */` |
|       135 | 3813 | `	if( !RangeEndpointZpp(pCtx,apArg[0],1,"start",&bStartNull,&rc) ){` |
|         7 | 3814 | `		return rc;` |
|         - | 3815 | `	}` |
|       129 | 3816 | `	if( !RangeEndpointZpp(pCtx,apArg[1],2,"end",&bEndNull,&rc) ){` |
|       ! 0 | 3817 | `		return rc;` |
|         - | 3818 | `	}` |
|       129 | 3819 | `	if( nArg > 2 ){` |
|        63 | 3820 | `		sxu8 iStepKind = RangeStepInput(pCtx,apArg[2],&step,&step_double,&rc);` |
|        63 | 3821 | `		if( iStepKind == RANGE_IN_ERROR ){` |
|         5 | 3822 | `			return rc;` |
|         - | 3823 | `		}` |
|        59 | 3824 | `		if( iStepKind == RANGE_IN_DOUBLE ){` |
|        25 | 3825 | `			if( PH7_IS_INF(step_double) ){` |
|         3 | 3826 | `				return PH7_VmThrowException(pCtx,"ValueError",` |
|         - | 3827 | `					"range(): Argument #3 ($step) must be a finite number, INF provided");` |
|         - | 3828 | `			}` |
|        23 | 3829 | `			if( PH7_IS_NAN(step_double) ){` |
|         3 | 3830 | `				return PH7_VmThrowException(pCtx,"ValueError",` |
|         - | 3831 | `					"range(): Argument #3 ($step) must be a finite number, NAN provided");` |
|         - | 3832 | `			}` |
|         - | 3833 | `			/* We only want positive step values. */` |
|        21 | 3834 | `			if( step_double < 0.0 ){` |
|       ! 0 | 3835 | `				is_step_negative = 1;` |
|       ! 0 | 3836 | `				step_double *= -1;` |
|       ! 0 | 3837 | `			}` |
|         - | 3838 | `			/* zend_dval_to_lval_silent + zend_is_long_compatible: an integral` |
|         - | 3839 | `			 * in-sxi64-range float step behaves as an int (char ranges accept` |
|         - | 3840 | `			 * it, int endpoints stay int); anything else is a float step. */` |
|        21 | 3841 | `			if( step_double < 9223372036854775808.0 ){` |
|        19 | 3842 | `				step = (sxi64)step_double;` |
|        19 | 3843 | `				if( (double)step != step_double ){` |
|        17 | 3844 | `					is_step_double = 1;` |
|         8 | 3845 | `				}` |
|        10 | 3846 | `			}else{` |
|         - | 3847 | ``				/* Casting out-of-range would be UB; `step` stays unread —`` |
|         - | 3848 | `				 * every reader is gated behind !is_step_double. */` |
|         3 | 3849 | `				is_step_double = 1;` |
|         - | 3850 | `			}` |
|        11 | 3851 | `		}else{` |
|         - | 3852 | `			/* We only want positive step values. */` |
|        35 | 3853 | `			if( step < 0 ){` |
|        11 | 3854 | `				if( step == SMALLEST_INT64 ){` |
|         - | 3855 | `					/* -step would overflow */` |
|         4 | 3856 | `					return PH7_VmThrowException(pCtx,"ValueError",` |
|         1 | 3857 | `						"range(): Argument #3 ($step) must be greater than %qd",step);` |
|         - | 3858 | `				}` |
|         9 | 3859 | `				is_step_negative = 1;` |
|         9 | 3860 | `				step = -step;` |
|         4 | 3861 | `			}` |
|        33 | 3862 | `			step_double = (double)step;` |
|         - | 3863 | `		}` |
|        53 | 3864 | `		if( step_double == 0.0 ){` |
|         7 | 3865 | `			return PH7_VmThrowException(pCtx,"ValueError",` |
|         - | 3866 | `				"range(): Argument #3 ($step) cannot be 0");` |
|         - | 3867 | `		}` |
|        23 | 3868 | `	}` |
|       113 | 3869 | `	start_type = RangeProcessInput(pCtx,apArg[0],1,"start",bStartNull,&start_long,&start_double,&cStart,&rc);` |
|       113 | 3870 | `	if( start_type == RANGE_IN_ERROR ){` |
|         5 | 3871 | `		return rc;` |
|         - | 3872 | `	}` |
|       109 | 3873 | `	end_type = RangeProcessInput(pCtx,apArg[1],2,"end",bEndNull,&end_long,&end_double,&cEnd,&rc);` |
|       109 | 3874 | `	if( end_type == RANGE_IN_ERROR ){` |
|         5 | 3875 | `		return rc;` |
|         - | 3876 | `	}` |
|         - | 3877 | `	/* Element container + result array */` |
|       105 | 3878 | `	pValue = ph7_context_new_scalar(pCtx);` |
|       105 | 3879 | `	pArray = ph7_context_new_array(pCtx);` |
|       105 | 3880 | `	if( pValue == 0 \|\| pArray == 0 ){` |
|       ! 0 | 3881 | `		return PH7_ContextMemoryError(pCtx);` |
|         - | 3882 | `	}` |
|         - | 3883 | `	/* If the range is given as strings, generate an array of characters. */` |
|       105 | 3884 | `	if( start_type >= RANGE_IN_STRING \|\| end_type >= RANGE_IN_STRING ){` |
|        37 | 3885 | `		if( start_type < RANGE_IN_STRING \|\| end_type < RANGE_IN_STRING ){` |
|         - | 3886 | `			/* Only one side is a string: the char side converts to 0 (with a` |
|         - | 3887 | `			 * warning unless the numeric side is an ambiguous single digit)` |
|         - | 3888 | `			 * and the range is numeric. */` |
|        15 | 3889 | `			if( start_type < RANGE_IN_STRING ){` |
|         7 | 3890 | `				if( end_type != RANGE_IN_DIGIT ){` |
|         7 | 3891 | `					PH7_VmThrowError(pCtx->pVm,0,PH7_CTX_WARNING,` |
|         - | 3892 | `						"range(): Argument #1 ($start) must be a single byte string if"` |
|         - | 3893 | `						" argument #2 ($end) is a single byte string, argument #2 ($end) converted to 0");` |
|         3 | 3894 | `				}` |
|         7 | 3895 | `				end_type = RANGE_IN_LONG;` |
|         4 | 3896 | `			}else{` |
|         9 | 3897 | `				if( start_type != RANGE_IN_DIGIT ){` |
|         9 | 3898 | `					PH7_VmThrowError(pCtx->pVm,0,PH7_CTX_WARNING,` |
|         - | 3899 | `						"range(): Argument #2 ($end) must be a single byte string if"` |
|         - | 3900 | `						" argument #1 ($start) is a single byte string, argument #1 ($start) converted to 0");` |
|         4 | 3901 | `				}` |
|         9 | 3902 | `				start_type = RANGE_IN_LONG;` |
|         - | 3903 | `			}` |
|        15 | 3904 | `			goto handle_numeric_inputs;` |
|         - | 3905 | `		}` |
|        23 | 3906 | `		if( is_step_double ){` |
|         - | 3907 | `			/* Only emit the warning if one of the inputs is not a numeric digit. */` |
|         5 | 3908 | `			if( start_type == RANGE_IN_STRING \|\| end_type == RANGE_IN_STRING ){` |
|         3 | 3909 | `				PH7_VmThrowError(pCtx->pVm,0,PH7_CTX_WARNING,` |
|         - | 3910 | `					"range(): Argument #3 ($step) must be of type int when generating an array"` |
|         - | 3911 | `					" of characters, inputs converted to 0");` |
|         1 | 3912 | `			}` |
|         5 | 3913 | `			start_type = RANGE_IN_LONG;` |
|         5 | 3914 | `			end_type = RANGE_IN_LONG;` |
|         5 | 3915 | `			goto handle_numeric_inputs;` |
|         - | 3916 | `		}` |
|         - | 3917 | `		/* Generate an array of characters */` |
|        19 | 3918 | `		if( cStart > cEnd ){` |
|         - | 3919 | `			/* Decreasing char range */` |
|         - | 3920 | `			int iCur;` |
|         3 | 3921 | `			if( (sxi64)(cStart - cEnd) < step ){` |
|       ! 0 | 3922 | `				goto boundary_error;` |
|         - | 3923 | `			}` |
|        17 | 3924 | `			for( iCur = (int)cStart ; iCur >= (int)cEnd ; iCur -= (int)step ){` |
|        15 | 3925 | `				if( (rc = RangeAppendChar(pCtx,pArray,pValue,(char)iCur)) != PH7_OK ){` |
|       ! 0 | 3926 | `					return rc;` |
|         - | 3927 | `				}` |
|         8 | 3928 | `			}` |
|        18 | 3929 | `		}else if( cEnd > cStart ){` |
|         - | 3930 | `			/* Increasing char range */` |
|         - | 3931 | `			int iCur;` |
|        15 | 3932 | `			if( is_step_negative ){` |
|         3 | 3933 | `				goto negative_step_error;` |
|         - | 3934 | `			}` |
|        13 | 3935 | `			if( (sxi64)(cEnd - cStart) < step ){` |
|         3 | 3936 | `				goto boundary_error;` |
|         - | 3937 | `			}` |
|       163 | 3938 | `			for( iCur = (int)cStart ; iCur <= (int)cEnd ; iCur += (int)step ){` |
|       153 | 3939 | `				if( (rc = RangeAppendChar(pCtx,pArray,pValue,(char)iCur)) != PH7_OK ){` |
|       ! 0 | 3940 | `					return rc;` |
|         - | 3941 | `				}` |
|        77 | 3942 | `			}` |
|         6 | 3943 | `		}else{` |
|         3 | 3944 | `			if( (rc = RangeAppendChar(pCtx,pArray,pValue,(char)cStart)) != PH7_OK ){` |
|       ! 0 | 3945 | `				return rc;` |
|         - | 3946 | `			}` |
|         - | 3947 | `		}` |
|        15 | 3948 | `		ph7_result_value(pCtx,pArray);` |
|        15 | 3949 | `		return PH7_OK;` |
|         - | 3950 | `	}` |
|        34 | 3951 | `handle_numeric_inputs:` |
|        95 | 3952 | `	if( start_type == RANGE_IN_DOUBLE \|\| end_type == RANGE_IN_DOUBLE \|\| is_step_double ){` |
|         - | 3953 | `		/* Float range */` |
|         - | 3954 | `		double elem,calc;` |
|        25 | 3955 | `		if( start_double > end_double ){` |
|         - | 3956 | `			/* Decreasing float range */` |
|         7 | 3957 | `			if( start_double - end_double < step_double ){` |
|       ! 0 | 3958 | `				goto boundary_error;` |
|         - | 3959 | `			}` |
|         7 | 3960 | `			calc = ((start_double - end_double) / step_double) + 1;` |
|         7 | 3961 | `			if( calc >= (double)PH7_RANGE_HT_MAX_SIZE ){` |
|         - | 3962 | `				/* php prints start/end swapped here (see RangeDoubleSizeError). */` |
|         3 | 3963 | `				return RangeDoubleSizeError(pCtx,calc,end_double,start_double,step_double);` |
|         - | 3964 | `			}` |
|         5 | 3965 | `			size = (sxu32)(calc + 0.5); /* _php_math_round(...,0,HALF_UP) */` |
|        19 | 3966 | `			for( i = 0,elem = start_double ; i < size && elem >= end_double ; ++i,elem = start_double - ((double)i * step_double) ){` |
|        15 | 3967 | `				if( (rc = RangeAppendDouble(pCtx,pArray,pValue,elem)) != PH7_OK ){` |
|       ! 0 | 3968 | `					return rc;` |
|         - | 3969 | `				}` |
|         8 | 3970 | `			}` |
|        21 | 3971 | `		}else if( end_double > start_double ){` |
|         - | 3972 | `			/* Increasing float range */` |
|        17 | 3973 | `			if( is_step_negative ){` |
|       ! 0 | 3974 | `				goto negative_step_error;` |
|         - | 3975 | `			}` |
|        17 | 3976 | `			if( end_double - start_double < step_double ){` |
|         3 | 3977 | `				goto boundary_error;` |
|         - | 3978 | `			}` |
|        15 | 3979 | `			calc = ((end_double - start_double) / step_double) + 1;` |
|        15 | 3980 | `			if( calc >= (double)PH7_RANGE_HT_MAX_SIZE ){` |
|         5 | 3981 | `				return RangeDoubleSizeError(pCtx,calc,start_double,end_double,step_double);` |
|         - | 3982 | `			}` |
|        11 | 3983 | `			size = (sxu32)(calc + 0.5);` |
|        65 | 3984 | `			for( i = 0,elem = start_double ; i < size && elem <= end_double ; ++i,elem = start_double + ((double)i * step_double) ){` |
|        55 | 3985 | `				if( (rc = RangeAppendDouble(pCtx,pArray,pValue,elem)) != PH7_OK ){` |
|       ! 0 | 3986 | `					return rc;` |
|         - | 3987 | `				}` |
|        28 | 3988 | `			}` |
|         6 | 3989 | `		}else{` |
|         3 | 3990 | `			if( (rc = RangeAppendDouble(pCtx,pArray,pValue,start_double)) != PH7_OK ){` |
|       ! 0 | 3991 | `				return rc;` |
|         - | 3992 | `			}` |
|         - | 3993 | `		}` |
|         9 | 3994 | `	}else{` |
|         - | 3995 | `		/* Int range. All arithmetic in unsigned space so a span wider than` |
|         - | 3996 | `		 * LARGEST_INT64 (e.g. -PHP_INT_MAX..PHP_INT_MAX) wraps correctly` |
|         - | 3997 | `		 * instead of overflowing, exactly like php's zend_ulong math. */` |
|        63 | 3998 | `		sxu64 ustep = (sxu64)step;` |
|         - | 3999 | `		sxu64 calc;` |
|        63 | 4000 | `		if( start_long > end_long ){` |
|         - | 4001 | `			/* Decreasing int range */` |
|        19 | 4002 | `			if( (sxu64)start_long - (sxu64)end_long < ustep ){` |
|         3 | 4003 | `				goto boundary_error;` |
|         - | 4004 | `			}` |
|        17 | 4005 | `			calc = ((sxu64)start_long - (sxu64)end_long) / ustep;` |
|        17 | 4006 | `			if( calc >= (sxu64)(PH7_RANGE_HT_MAX_SIZE - 1) ){` |
|         - | 4007 | `				/* php prints start/end swapped here (see RangeLongSizeError). */` |
|         3 | 4008 | `				return RangeLongSizeError(pCtx,calc,end_long,start_long,step);` |
|         - | 4009 | `			}` |
|        15 | 4010 | `			size = (sxu32)(calc + 1);` |
|       101 | 4011 | `			for( i = 0 ; i < size ; ++i ){` |
|        87 | 4012 | `				if( (rc = RangeAppendInt(pCtx,pArray,pValue,(sxi64)((sxu64)start_long - (sxu64)i * ustep))) != PH7_OK ){` |
|       ! 0 | 4013 | `					return rc;` |
|         - | 4014 | `				}` |
|        44 | 4015 | `			}` |
|        52 | 4016 | `		}else if( end_long > start_long ){` |
|         - | 4017 | `			/* Increasing int range */` |
|        39 | 4018 | `			if( is_step_negative ){` |
|         3 | 4019 | `				goto negative_step_error;` |
|         - | 4020 | `			}` |
|        37 | 4021 | `			if( (sxu64)end_long - (sxu64)start_long < ustep ){` |
|         3 | 4022 | `				goto boundary_error;` |
|         - | 4023 | `			}` |
|        35 | 4024 | `			calc = ((sxu64)end_long - (sxu64)start_long) / ustep;` |
|        35 | 4025 | `			if( calc >= (sxu64)(PH7_RANGE_HT_MAX_SIZE - 1) ){` |
|         5 | 4026 | `				return RangeLongSizeError(pCtx,calc,start_long,end_long,step);` |
|         - | 4027 | `			}` |
|        31 | 4028 | `			size = (sxu32)(calc + 1);` |
|       273 | 4029 | `			for( i = 0 ; i < size ; ++i ){` |
|       243 | 4030 | `				if( (rc = RangeAppendInt(pCtx,pArray,pValue,(sxi64)((sxu64)start_long + (sxu64)i * ustep))) != PH7_OK ){` |
|       ! 0 | 4031 | `					return rc;` |
|         - | 4032 | `				}` |
|       122 | 4033 | `			}` |
|        16 | 4034 | `		}else{` |
|         7 | 4035 | `			if( (rc = RangeAppendInt(pCtx,pArray,pValue,start_long)) != PH7_OK ){` |
|       ! 0 | 4036 | `				return rc;` |
|         - | 4037 | `			}` |
|         - | 4038 | `		}` |
|         - | 4039 | `	}` |
|         - | 4040 | `	/* Return the new array. 'pValue' is released automatically by the` |
|         - | 4041 | `	 * virtual machine as soon as we return from this foreign function. */` |
|        67 | 4042 | `	ph7_result_value(pCtx,pArray);` |
|        67 | 4043 | `	return PH7_OK;` |
|         2 | 4044 | `negative_step_error:` |
|         5 | 4045 | `	return PH7_VmThrowException(pCtx,"ValueError",` |
|         - | 4046 | `		"range(): Argument #3 ($step) must be greater than 0 for increasing ranges");` |
|         4 | 4047 | `boundary_error:` |
|         9 | 4048 | `	return PH7_VmThrowException(pCtx,"ValueError",` |
|         - | 4049 | `		"range(): Argument #3 ($step) must be less than the range spanned by argument #1 ($start) and argument #2 ($end)");` |
|        69 | 4050 | `}` |
|         - | 4051 | `/*` |
|         - | 4052 | ` * array array_values(array $array)` |
|         - | 4053 | ` *  Return all the values of an array, indexed numerically.` |
|         - | 4054 | ` * Parameters` |
|         - | 4055 | ` *  $array` |
|         - | 4056 | ` *   The input array.` |
|         - | 4057 | ` * Return` |
|         - | 4058 | ` *  An indexed array of values or NULL on allocation failure.` |
|         - | 4059 | ` */` |
|        36 | 4060 | `static int ph7_hashmap_values(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|         4 | 4061 | `{` |
|         - | 4062 | `	ph7_hashmap_node *pNode;` |
|         - | 4063 | `	ph7_hashmap *pMap;` |
|         - | 4064 | `	ph7_value *pArray;` |
|         - | 4065 | `	ph7_value *pObj;` |
|         - | 4066 | `	sxu32 n;` |
|        40 | 4067 | `	if( nArg != 1 ){` |
|         - | 4068 | `		/* Wrong argument count, throw ArgumentCountError */` |
|         8 | 4069 | `		return PH7_VmThrowException(pCtx,` |
|         - | 4070 | `			"ArgumentCountError",` |
|         - | 4071 | `			"array_values() expects exactly 1 argument, %d given",` |
|         2 | 4072 | `			nArg` |
|         - | 4073 | `			);` |
|         - | 4074 | `	}` |
|         - | 4075 | `	/* Make sure we are dealing with a valid hashmap */` |
|        35 | 4076 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|         - | 4077 | `		/* Type mismatch, throw TypeError */` |
|         4 | 4078 | `		return PH7_VmThrowException(pCtx,` |
|         - | 4079 | `			"TypeError",` |
|         - | 4080 | `			"array_values(): Argument #1 ($array) must be of type array, %s given",` |
|         1 | 4081 | `			ph7_type_name(apArg[0])` |
|         - | 4082 | `			);` |
|         - | 4083 | `	}` |
|         - | 4084 | `	/* Point to the internal representation that describe the input hashmap */` |
|        32 | 4085 | `	pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|         - | 4086 | `	/* Create a new array */` |
|        32 | 4087 | `	pArray = ph7_context_new_array(pCtx);` |
|        32 | 4088 | `	if( pArray == 0 ){` |
|       ! 0 | 4089 | `		ph7_result_null(pCtx);` |
|       ! 0 | 4090 | `		return PH7_OK;` |
|         - | 4091 | `	}` |
|         - | 4092 | `	/* Perform the requested operation */` |
|        32 | 4093 | `	pNode = pMap->pFirst;` |
|       104 | 4094 | `	for( n = 0 ; n < pMap->nEntry ; ++n ){` |
|        74 | 4095 | `		pObj = HashmapExtractNodeValue(pNode);` |
|        74 | 4096 | `		if( pObj ){` |
|         - | 4097 | `			/* perform the insertion */` |
|        74 | 4098 | `			ph7_array_add_elem(pArray,0/* Automatic index assign */,pObj);` |
|        36 | 4099 | `		}` |
|         - | 4100 | `		/* Point to the next entry */` |
|        74 | 4101 | `		pNode = pNode->pPrev; /* Reverse link */` |
|        38 | 4102 | `	}` |
|         - | 4103 | `	/* return the new array */` |
|        32 | 4104 | `	ph7_result_value(pCtx,pArray);` |
|        32 | 4105 | `	return PH7_OK;` |
|        22 | 4106 | `}` |
|         - | 4107 | `/*` |
|         - | 4108 | ` * array array_keys(array $input [, val $search_value [, bool $strict = false ]] )` |
|         - | 4109 | ` *  Return all the keys or a subset of the keys of an array.` |
|         - | 4110 | ` * Parameters` |
|         - | 4111 | ` *  $input` |
|         - | 4112 | ` *   An array containing keys to return.` |
|         - | 4113 | ` * $search_value` |
|         - | 4114 | ` *   If specified, then only keys containing these values are returned.` |
|         - | 4115 | ` * $strict` |
|         - | 4116 | ` *   Determines if strict comparison (===) should be used during the search.` |
|         - | 4117 | ` * Return` |
|         - | 4118 | ` *  An array of all the keys in input or NULL on failure.` |
|         - | 4119 | ` */` |
|       142 | 4120 | `static int ph7_hashmap_keys(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|         5 | 4121 | `{` |
|         - | 4122 | `	ph7_hashmap_node *pNode;` |
|         - | 4123 | `	ph7_hashmap *pMap;` |
|         - | 4124 | `	ph7_value *pArray;` |
|         - | 4125 | `	ph7_value sObj;` |
|         - | 4126 | `	ph7_value sVal;` |
|         - | 4127 | `	SyString sKey;` |
|         - | 4128 | `	int bStrict;` |
|         - | 4129 | `	sxi32 rc;` |
|         - | 4130 | `	sxu32 n;` |
|       147 | 4131 | `	if( nArg < 1 ){` |
|         - | 4132 | `		/* Missing argument,throw ArgumentCountError */` |
|         3 | 4133 | `		return PH7_VmThrowException(pCtx,` |
|         - | 4134 | `			"ArgumentCountError",` |
|         - | 4135 | `			"array_keys() expects at least 1 argument, 0 given"` |
|         - | 4136 | `			);` |
|         - | 4137 | `	}` |
|         - | 4138 | `	/* Make sure we are dealing with a valid hashmap */` |
|       144 | 4139 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|         - | 4140 | `		/* haystack must be an array,throw TypeError */` |
|         4 | 4141 | `		return PH7_VmThrowException(pCtx,` |
|         - | 4142 | `			"TypeError",` |
|         - | 4143 | `			"array_keys(): Argument #1 ($array) must be of type array, %s given",` |
|         1 | 4144 | `			ph7_type_name(apArg[0])` |
|         - | 4145 | `			);` |
|         - | 4146 | `	}` |
|         - | 4147 | `	/* Point to the internal representation of the input hashmap */` |
|       142 | 4148 | `	pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|         - | 4149 | `	/* Create a new array */` |
|       142 | 4150 | `	pArray = ph7_context_new_array(pCtx);` |
|       142 | 4151 | `	if( pArray == 0 ){` |
|       ! 0 | 4152 | `		ph7_result_null(pCtx);` |
|       ! 0 | 4153 | `		return PH7_OK;` |
|         - | 4154 | `	}` |
|       142 | 4155 | `	bStrict = FALSE;` |
|       142 | 4156 | `	if( nArg > 2 ){` |
|         - | 4157 | `		/* In PHP, non-scalar values for a bool-hinted parameter raise TypeError */` |
|         8 | 4158 | `		if( ph7_value_is_array(apArg[2]) \|\| ph7_value_is_object(apArg[2]) \|\| ph7_value_is_resource(apArg[2]) ){` |
|         4 | 4159 | `			return PH7_VmThrowException(pCtx,` |
|         - | 4160 | `				"TypeError",` |
|         - | 4161 | `				"array_keys(): Argument #3 ($strict) must be of type bool, %s given",` |
|         2 | 4162 | `				ph7_type_name(apArg[2])` |
|         - | 4163 | `				);` |
|         - | 4164 | `		}` |
|         5 | 4165 | `		bStrict = ph7_value_to_bool(apArg[2]);` |
|         2 | 4166 | `	}` |
|         - | 4167 | `	/* Perform the requested operation */` |
|       139 | 4168 | `	pNode = pMap->pFirst;` |
|       139 | 4169 | `	PH7_MemObjInit(pMap->pVm,&sVal);` |
|      1361 | 4170 | `	for( n = 0 ; n < pMap->nEntry ; ++n ){` |
|      1225 | 4171 | `		if( pNode->iType == HASHMAP_INT_NODE ){` |
|       129 | 4172 | `			PH7_MemObjInitFromInt(pMap->pVm,&sObj,pNode->xKey.iKey);` |
|        66 | 4173 | `		}else{` |
|      1098 | 4174 | `			SyStringInitFromBuf(&sKey,SyBlobData(&pNode->xKey.sKey),SyBlobLength(&pNode->xKey.sKey));` |
|      1098 | 4175 | `			PH7_MemObjInitFromString(pMap->pVm,&sObj,&sKey);` |
|         - | 4176 | `		}` |
|      1225 | 4177 | `		rc = 0;` |
|      1225 | 4178 | `		if( nArg > 1 ){` |
|        31 | 4179 | `			ph7_value *pValue = HashmapExtractNodeValue(pNode);` |
|        31 | 4180 | `			if( pValue ){` |
|        31 | 4181 | `				PH7_MemObjLoad(pValue,&sVal);` |
|         - | 4182 | `				/* Filter key */` |
|        31 | 4183 | `				rc = ph7_value_compare(&sVal,apArg[1],bStrict);` |
|        31 | 4184 | `				PH7_MemObjRelease(&sVal);` |
|        15 | 4185 | `			}` |
|        15 | 4186 | `		}` |
|      1225 | 4187 | `		if( rc == 0 ){` |
|         - | 4188 | `			/* Perform the insertion */` |
|      1207 | 4189 | `			ph7_array_add_elem(pArray,0,&sObj);` |
|       602 | 4190 | `		}` |
|      1225 | 4191 | `		PH7_MemObjRelease(&sObj);` |
|         - | 4192 | `		/* Point to the next entry */` |
|      1225 | 4193 | `		pNode = pNode->pPrev; /* Reverse link */` |
|       614 | 4194 | `	}` |
|         - | 4195 | `	/* return the new array */` |
|       139 | 4196 | `	ph7_result_value(pCtx,pArray);` |
|       139 | 4197 | `	return PH7_OK;` |
|        76 | 4198 | `}` |
|         - | 4199 | `/*` |
|         - | 4200 | ` * bool array_same(array $arr1,array $arr2)` |
|         - | 4201 | ` *  Return TRUE if the given arrays are the same instance.` |
|         - | 4202 | ` *  This function is useful under PH7 since arrays are passed` |
|         - | 4203 | ` *  by reference unlike the zend engine which use pass by values.` |
|         - | 4204 | ` * Parameters` |
|         - | 4205 | ` *  $arr1` |
|         - | 4206 | ` *   First array` |
|         - | 4207 | ` *  $arr2` |
|         - | 4208 | ` *   Second array` |
|         - | 4209 | ` * Return` |
|         - | 4210 | ` *  TRUE if the arrays are the same instance.FALSE otherwise.` |
|         - | 4211 | ` * Note` |
|         - | 4212 | ` *  This function is a symisc eXtension.` |
|         - | 4213 | ` */` |
|         4 | 4214 | `static int ph7_hashmap_same(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|         1 | 4215 | `{` |
|         - | 4216 | `	ph7_hashmap *p1,*p2;` |
|         - | 4217 | `	int rc;` |
|         5 | 4218 | `	if( nArg < 2 \|\| !ph7_value_is_array(apArg[0]) \|\| !ph7_value_is_array(apArg[1]) ){` |
|         - | 4219 | `		/* Missing or invalid arguments,return FALSE*/` |
|       ! 0 | 4220 | `		ph7_result_bool(pCtx,0);` |
|       ! 0 | 4221 | `		return PH7_OK;` |
|         - | 4222 | `	}` |
|         - | 4223 | `	/* Point to the hashmaps */` |
|         5 | 4224 | `	p1 = (ph7_hashmap *)apArg[0]->x.pOther;` |
|         5 | 4225 | `	p2 = (ph7_hashmap *)apArg[1]->x.pOther;` |
|         5 | 4226 | `	rc = (p1 == p2);` |
|         - | 4227 | `	/* Same instance? */` |
|         5 | 4228 | `	ph7_result_bool(pCtx,rc);` |
|         5 | 4229 | `	return PH7_OK;` |
|         3 | 4230 | `}` |
|         - | 4231 | `/*` |
|         - | 4232 | ` * array array_merge(array ...$arrays)` |
|         - | 4233 | ` *  Merge one or more arrays.` |
|         - | 4234 | ` * Parameters` |
|         - | 4235 | ` *  ...$arrays` |
|         - | 4236 | ` *   Variable list of arrays to merge. Each argument must be an array;` |
|         - | 4237 | ` *   passing a non-array argument throws a TypeError.` |
|         - | 4238 | ` * Return` |
|         - | 4239 | ` *  The resulting merged array. Returns an empty array when called` |
|         - | 4240 | ` *  with no arguments.` |
|         - | 4241 | ` */` |
|      1030 | 4242 | `static int ph7_hashmap_merge(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|         5 | 4243 | `{` |
|         - | 4244 | `	ph7_hashmap *pMap,*pSrc;` |
|         - | 4245 | `	ph7_value *pArray;` |
|         - | 4246 | `	int i;` |
|         - | 4247 | `	/* Create a new array */` |
|      1035 | 4248 | `	pArray = ph7_context_new_array(pCtx);` |
|      1035 | 4249 | `	if( pArray == 0 ){` |
|       ! 0 | 4250 | `		ph7_result_null(pCtx);` |
|       ! 0 | 4251 | `		return PH7_OK;` |
|         - | 4252 | `	}` |
|         - | 4253 | `	/* Point to the internal representation of the hashmap */` |
|      1035 | 4254 | `	pMap = (ph7_hashmap *)pArray->x.pOther;` |
|         - | 4255 | `	/* Start merging */` |
|      3085 | 4256 | `	for( i = 0 ; i < nArg ; i++ ){` |
|         - | 4257 | `		/* Make sure we are dealing with a valid hashmap */` |
|      2059 | 4258 | `		if( !ph7_value_is_array(apArg[i]) ){` |
|         - | 4259 | `			/* Type mismatch -> TypeError */` |
|         8 | 4260 | `			return PH7_VmThrowException(pCtx,` |
|         - | 4261 | `				"TypeError",` |
|         - | 4262 | `				"array_merge(): Argument #%d must be of type array, %s given",` |
|         2 | 4263 | `				i + 1,` |
|         4 | 4264 | `				ph7_type_name(apArg[i])` |
|         - | 4265 | `				);` |
|       ! 0 | 4266 | `		}else{` |
|      2055 | 4267 | `			pSrc = (ph7_hashmap *)apArg[i]->x.pOther;` |
|         - | 4268 | `			/* Merge the two hashmaps */` |
|      2055 | 4269 | `			HashmapMerge(pSrc,pMap);` |
|         - | 4270 | `		}` |
|      1030 | 4271 | `	}` |
|         - | 4272 | `	/* Return the freshly created array */` |
|      1031 | 4273 | `	ph7_result_value(pCtx,pArray);` |
|      1031 | 4274 | `	return PH7_OK;` |
|       520 | 4275 | `}` |
|         - | 4276 | `/*` |
|         - | 4277 | ` * array array_copy(array $source)` |
|         - | 4278 | ` *  Make a blind copy of the target array.` |
|         - | 4279 | ` * Parameters` |
|         - | 4280 | ` *  $source` |
|         - | 4281 | ` *   Target array` |
|         - | 4282 | ` * Return` |
|         - | 4283 | ` *  Copy of the target array on success.NULL otherwise.` |
|         - | 4284 | ` * Note` |
|         - | 4285 | ` *  This function is a symisc eXtension.` |
|         - | 4286 | ` */` |
|        16 | 4287 | `static int ph7_hashmap_copy(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|         1 | 4288 | `{` |
|         - | 4289 | `	ph7_hashmap *pMap;` |
|         - | 4290 | `	ph7_value *pArray;` |
|        17 | 4291 | `	if( nArg < 1 ){` |
|         - | 4292 | `		/* Missing arguments,return NULL */` |
|       ! 0 | 4293 | `		ph7_result_null(pCtx);` |
|       ! 0 | 4294 | `		return PH7_OK;` |
|         - | 4295 | `	}` |
|         - | 4296 | `	/* Create a new array */` |
|        17 | 4297 | `	pArray = ph7_context_new_array(pCtx);` |
|        17 | 4298 | `	if( pArray == 0 ){` |
|       ! 0 | 4299 | `		ph7_result_null(pCtx);` |
|       ! 0 | 4300 | `		return PH7_OK;` |
|         - | 4301 | `	}` |
|         - | 4302 | `	/* Point to the internal representation of the hashmap */` |
|        17 | 4303 | `	pMap = (ph7_hashmap *)pArray->x.pOther;` |
|        17 | 4304 | `	if( ph7_value_is_array(apArg[0])){` |
|         - | 4305 | `		/* Point to the internal representation of the source */` |
|        17 | 4306 | `		ph7_hashmap *pSrc = (ph7_hashmap *)apArg[0]->x.pOther;` |
|         - | 4307 | `		/* Perform the copy */` |
|        17 | 4308 | `		PH7_HashmapDup(pSrc,pMap);` |
|         9 | 4309 | `	}else{` |
|         - | 4310 | `		/* Simple insertion */` |
|       ! 0 | 4311 | `		PH7_HashmapInsert(pMap,0/* Automatic index assign*/,apArg[0]);` |
|         - | 4312 | `	}` |
|         - | 4313 | `	/* Return the duplicated array */` |
|        17 | 4314 | `	ph7_result_value(pCtx,pArray);` |
|        17 | 4315 | `	return PH7_OK;` |
|         9 | 4316 | `}` |
|         - | 4317 | `/*` |
|         - | 4318 | ` * bool array_erase(array $source)` |
|         - | 4319 | ` *  Remove all elements from a given array.` |
|         - | 4320 | ` * Parameters` |
|         - | 4321 | ` *  $source` |
|         - | 4322 | ` *   Target array` |
|         - | 4323 | ` * Return` |
|         - | 4324 | ` *  TRUE on success.FALSE otherwise.` |
|         - | 4325 | ` * Note` |
|         - | 4326 | ` *  This function is a symisc eXtension.` |
|         - | 4327 | ` */` |
|        16 | 4328 | `static int ph7_hashmap_erase(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|         1 | 4329 | `{` |
|         - | 4330 | `	ph7_hashmap *pMap;` |
|        17 | 4331 | `	if( nArg < 1 ){` |
|         - | 4332 | `		/* Missing arguments */` |
|       ! 0 | 4333 | `		ph7_result_bool(pCtx,0);` |
|       ! 0 | 4334 | `		return PH7_OK;` |
|         - | 4335 | `	}` |
|         - | 4336 | `	/* Point to the target hashmap */` |
|        17 | 4337 | `	PH7_HashmapCowSeparate(pCtx->pVm, apArg[0]);` |
|        17 | 4338 | `	pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|         - | 4339 | `	/* Erase */` |
|        17 | 4340 | `	PH7_HashmapRelease(pMap,FALSE);` |
|        17 | 4341 | `	return PH7_OK;` |
|         9 | 4342 | `}` |
|         - | 4343 | `/*` |
|         - | 4344 | ` * array array_slice(array $array, int $offset [, ?int $length = null [, bool $preserve_keys = false ]])` |
|         - | 4345 | ` *  Extract a slice of the array.` |
|         - | 4346 | ` * Parameters` |
|         - | 4347 | ` *  $array` |
|         - | 4348 | ` *    The input array.` |
|         - | 4349 | ` * $offset` |
|         - | 4350 | ` *    If offset is non-negative, the sequence will start at that offset in the array.` |
|         - | 4351 | ` *    If offset is negative, the sequence will start that far from the end of the array.` |
|         - | 4352 | ` * $length (optional, nullable)` |
|         - | 4353 | ` *    If length is given and is positive, then the sequence will have that many elements` |
|         - | 4354 | ` *    in it. If length is given and is negative then the sequence will stop that many` |
|         - | 4355 | ` *    elements from the end of the array. If it is omitted or NULL, then the sequence` |
|         - | 4356 | ` *    will have everything from offset up until the end of the array.` |
|         - | 4357 | ` * $preserve_keys (optional)` |
|         - | 4358 | ` *    Note that array_slice() will reorder and reset the array indices by default.` |
|         - | 4359 | ` *    You can change this behaviour by setting preserve_keys to TRUE.` |
|         - | 4360 | ` * Return` |
|         - | 4361 | ` *   The new slice.` |
|         - | 4362 | ` */` |
|        50 | 4363 | `static int ph7_hashmap_slice(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|         5 | 4364 | `{` |
|         - | 4365 | `	ph7_hashmap *pMap,*pSrc;` |
|         - | 4366 | `	ph7_hashmap_node *pCur;` |
|         - | 4367 | `	ph7_value *pArray;` |
|         - | 4368 | `	int iLength,iOfft;` |
|         - | 4369 | `	int bPreserve;` |
|         - | 4370 | `	sxi32 rc;` |
|        55 | 4371 | `	if( nArg < 2 ){` |
|         8 | 4372 | `		return PH7_VmThrowException(pCtx,` |
|         - | 4373 | `			"ArgumentCountError",` |
|         - | 4374 | `			"array_slice() expects at least 2 arguments, %d given",` |
|         2 | 4375 | `			nArg` |
|         - | 4376 | `			);` |
|         - | 4377 | `	}` |
|        51 | 4378 | `	if( nArg > 4 ){` |
|         4 | 4379 | `		return PH7_VmThrowException(pCtx,` |
|         - | 4380 | `			"ArgumentCountError",` |
|         - | 4381 | `			"array_slice() expects at most 4 arguments, %d given",` |
|         1 | 4382 | `			nArg` |
|         - | 4383 | `			);` |
|         - | 4384 | `	}` |
|        49 | 4385 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|         4 | 4386 | `		return PH7_VmThrowException(pCtx,` |
|         - | 4387 | `			"TypeError",` |
|         - | 4388 | `			"array_slice(): Argument #1 ($array) must be of type array, %s given",` |
|         1 | 4389 | `			ph7_type_name(apArg[0])` |
|         - | 4390 | `			);` |
|         - | 4391 | `	}` |
|         - | 4392 | `	/* Validate $offset type: reject string, array, object, resource */` |
|        62 | 4393 | `	if( ph7_value_is_string(apArg[1]) \|\| ph7_value_is_array(apArg[1]) \|\|` |
|        65 | 4394 | `		ph7_value_is_object(apArg[1]) \|\| ph7_value_is_resource(apArg[1]) ){` |
|         4 | 4395 | `		return PH7_VmThrowException(pCtx,` |
|         - | 4396 | `			"TypeError",` |
|         - | 4397 | `			"array_slice(): Argument #2 ($offset) must be of type int, %s given",` |
|         2 | 4398 | `			ph7_type_name(apArg[1])` |
|         - | 4399 | `			);` |
|         - | 4400 | `	}` |
|         - | 4401 | `	/* Validate $length type if provided: nullable int */` |
|        45 | 4402 | `	if( nArg > 2 && !ph7_value_is_null(apArg[2]) ){` |
|        26 | 4403 | `		if( ph7_value_is_string(apArg[2]) \|\| ph7_value_is_array(apArg[2]) \|\|` |
|        26 | 4404 | `			ph7_value_is_object(apArg[2]) \|\| ph7_value_is_resource(apArg[2]) ){` |
|         4 | 4405 | `			return PH7_VmThrowException(pCtx,` |
|         - | 4406 | `				"TypeError",` |
|         - | 4407 | `				"array_slice(): Argument #3 ($length) must be of type ?int, %s given",` |
|         2 | 4408 | `				ph7_type_name(apArg[2])` |
|         - | 4409 | `				);` |
|         - | 4410 | `		}` |
|         8 | 4411 | `	}` |
|         - | 4412 | `	/* Validate $preserve_keys type if provided: reject array, object, resource */` |
|        43 | 4413 | `	if( nArg > 3 ){` |
|        10 | 4414 | `		if( ph7_value_is_array(apArg[3]) \|\| ph7_value_is_object(apArg[3]) \|\|` |
|         4 | 4415 | `			ph7_value_is_resource(apArg[3]) ){` |
|         4 | 4416 | `			return PH7_VmThrowException(pCtx,` |
|         - | 4417 | `				"TypeError",` |
|         - | 4418 | `				"array_slice(): Argument #4 ($preserve_keys) must be of type bool, %s given",` |
|         2 | 4419 | `				ph7_type_name(apArg[3])` |
|         - | 4420 | `				);` |
|         - | 4421 | `		}` |
|         2 | 4422 | `	}` |
|         - | 4423 | `	/* Point the internal representation of the target array */` |
|        41 | 4424 | `	pSrc = (ph7_hashmap *)apArg[0]->x.pOther;` |
|        41 | 4425 | `	bPreserve = FALSE;` |
|         - | 4426 | `	/* Get the offset */` |
|        41 | 4427 | `	iOfft = ph7_value_to_int(apArg[1]);` |
|        41 | 4428 | `	if( iOfft < 0 ){` |
|         5 | 4429 | `		iOfft = (int)pSrc->nEntry + iOfft;` |
|         5 | 4430 | `		if( iOfft < 0 ){` |
|         3 | 4431 | `			iOfft = 0;` |
|         1 | 4432 | `		}` |
|         2 | 4433 | `	}` |
|        41 | 4434 | `	if( iOfft >= (int)pSrc->nEntry ){` |
|         - | 4435 | `		/* Offset past end of array, return empty array */` |
|         5 | 4436 | `		pArray = ph7_context_new_array(pCtx);` |
|         5 | 4437 | `		if( pArray == 0 ){` |
|       ! 0 | 4438 | `			ph7_result_null(pCtx);` |
|       ! 0 | 4439 | `			return PH7_OK;` |
|         - | 4440 | `		}` |
|         5 | 4441 | `		ph7_result_value(pCtx,pArray);` |
|         5 | 4442 | `		return PH7_OK;` |
|         - | 4443 | `	}` |
|         - | 4444 | `	/* Get the length: NULL means "all remaining" (same as omitting) */` |
|        37 | 4445 | `	iLength = (int)pSrc->nEntry - iOfft;` |
|        37 | 4446 | `	if( nArg > 2 && !ph7_value_is_null(apArg[2]) ){` |
|        15 | 4447 | `		iLength = ph7_value_to_int(apArg[2]);` |
|        15 | 4448 | `		if( iLength < 0 ){` |
|         5 | 4449 | `			iLength = ((int)pSrc->nEntry + iLength) - iOfft;` |
|         2 | 4450 | `		}` |
|        15 | 4451 | `		if( iLength < 0 ){` |
|         3 | 4452 | `			iLength = 0;` |
|         1 | 4453 | `		}` |
|        15 | 4454 | `		if( iOfft + iLength > (int)pSrc->nEntry ){` |
|         3 | 4455 | `			iLength = (int)pSrc->nEntry - iOfft;` |
|         1 | 4456 | `		}` |
|         7 | 4457 | `	}` |
|        37 | 4458 | `	if( nArg > 3 ){` |
|         5 | 4459 | `		bPreserve = ph7_value_to_bool(apArg[3]);` |
|         2 | 4460 | `	}` |
|         - | 4461 | `	/* Create a new array */` |
|        37 | 4462 | `	pArray = ph7_context_new_array(pCtx);` |
|        37 | 4463 | `	if( pArray == 0 ){` |
|       ! 0 | 4464 | `		ph7_result_null(pCtx);` |
|       ! 0 | 4465 | `		return PH7_OK;` |
|         - | 4466 | `	}` |
|        37 | 4467 | `	if( iLength < 1 ){` |
|         - | 4468 | `		/* Don't bother processing,return the empty array */` |
|         5 | 4469 | `		ph7_result_value(pCtx,pArray);` |
|         5 | 4470 | `		return PH7_OK;` |
|         - | 4471 | `	}` |
|         - | 4472 | `	/* Point to the desired entry */` |
|        33 | 4473 | `	pCur = pSrc->pFirst;` |
|        28 | 4474 | `	for(;;){` |
|        61 | 4475 | `		if( iOfft < 1 ){` |
|        33 | 4476 | `			break;` |
|         - | 4477 | `		}` |
|         - | 4478 | `		/* Point to the next entry */` |
|        33 | 4479 | `		pCur = pCur->pPrev; /* Reverse link */` |
|        33 | 4480 | `		iOfft--;` |
|         5 | 4481 | `	}` |
|         - | 4482 | `	/* Point to the internal representation of the hashmap */` |
|        33 | 4483 | `	pMap = (ph7_hashmap *)pArray->x.pOther;` |
|        51 | 4484 | `	for(;;){` |
|       107 | 4485 | `		if( iLength < 1 ){` |
|        33 | 4486 | `			break;` |
|         - | 4487 | `		}` |
|         - | 4488 | `		/* String keys are always preserved; preserve_keys only affects int keys */` |
|         - | 4489 | `		{` |
|        79 | 4490 | `			int bKeep = (pCur->iType == HASHMAP_INT_NODE) ? bPreserve : TRUE;` |
|        79 | 4491 | `			rc = HashmapInsertNode(pMap,pCur,bKeep);` |
|         - | 4492 | `		}` |
|        79 | 4493 | `		if( rc != SXRET_OK ){` |
|       ! 0 | 4494 | `			break;` |
|         - | 4495 | `		}` |
|         - | 4496 | `		/* Point to the next entry */` |
|        79 | 4497 | `		pCur = pCur->pPrev; /* Reverse link */` |
|        79 | 4498 | `		iLength--;` |
|         5 | 4499 | `	}` |
|         - | 4500 | `	/* Return the freshly created array */` |
|        33 | 4501 | `	ph7_result_value(pCtx,pArray);` |
|        33 | 4502 | `	return PH7_OK;` |
|        30 | 4503 | `}` |
|         - | 4504 | `/*` |
|         - | 4505 | ` * Move the last node in the hashmap linked list to immediately after pAfter` |
|         - | 4506 | ` * in iteration order.  If pAfter is NULL the node is moved to the very` |
|         - | 4507 | ` * beginning (becomes the new pFirst).` |
|         - | 4508 | ` */` |
|        30 | 4509 | `static void HashmapMoveLastAfter(ph7_hashmap *pMap,ph7_hashmap_node *pAfter)` |
|         1 | 4510 | `{` |
|         - | 4511 | `	ph7_hashmap_node *pNode;` |
|         - | 4512 | `	ph7_hashmap_node *pOldNext;` |
|        31 | 4513 | `	pNode = pMap->pLast;` |
|        31 | 4514 | `	if( pNode == 0 ){` |
|       ! 0 | 4515 | `		return;` |
|         - | 4516 | `	}` |
|        31 | 4517 | `	if( pNode->pNext == 0 ){` |
|         - | 4518 | `		/* Only node in the list, nothing to move */` |
|         5 | 4519 | `		return;` |
|         - | 4520 | `	}` |
|        27 | 4521 | `	if( pAfter != 0 && pAfter->pPrev == pNode ){` |
|         - | 4522 | `		/* Already in the correct position */` |
|         9 | 4523 | `		return;` |
|         - | 4524 | `	}` |
|         - | 4525 | `	/* Unlink pNode from the end of the list */` |
|        19 | 4526 | `	pMap->pLast = pNode->pNext;` |
|        19 | 4527 | `	pMap->pLast->pPrev = 0;` |
|         - | 4528 | `	/* Insert pNode after pAfter in iteration order */` |
|        19 | 4529 | `	if( pAfter == 0 ){` |
|         - | 4530 | `		/* Insert at the very beginning, before pFirst */` |
|         3 | 4531 | `		pNode->pNext = 0;` |
|         3 | 4532 | `		pNode->pPrev = pMap->pFirst;` |
|         3 | 4533 | `		if( pMap->pFirst ){` |
|         3 | 4534 | `			pMap->pFirst->pNext = pNode;` |
|         1 | 4535 | `		}` |
|         3 | 4536 | `		pMap->pFirst = pNode;` |
|         2 | 4537 | `	}else{` |
|        17 | 4538 | `		pOldNext = pAfter->pPrev;` |
|        17 | 4539 | `		pNode->pPrev = pOldNext;` |
|        17 | 4540 | `		pNode->pNext = pAfter;` |
|        17 | 4541 | `		pAfter->pPrev = pNode;` |
|        17 | 4542 | `		if( pOldNext ){` |
|        17 | 4543 | `			pOldNext->pNext = pNode;` |
|         9 | 4544 | `		}else{` |
|       ! 0 | 4545 | `			pMap->pLast = pNode;` |
|         - | 4546 | `		}` |
|         - | 4547 | `	}` |
|        16 | 4548 | `}` |
|         - | 4549 | `/*` |
|         - | 4550 | ` * array array_splice(array $array, int $offset [, int $length [, value $replacement]])` |
|         - | 4551 | ` *  Remove a portion of the array and replace it with something else.` |
|         - | 4552 | ` * Parameters` |
|         - | 4553 | ` *  $array` |
|         - | 4554 | ` *    The input array.` |
|         - | 4555 | ` *  $offset` |
|         - | 4556 | ` *    If offset is positive then the start of removed portion is at that offset` |
|         - | 4557 | ` *    from the beginning of the input array.  If offset is negative then it` |
|         - | 4558 | ` *    starts that far from the end of the input array.  If the absolute value of` |
|         - | 4559 | ` *    a negative offset exceeds the array length, offset is clamped to 0.  If a` |
|         - | 4560 | ` *    positive offset exceeds the array length, offset is clamped to the array` |
|         - | 4561 | ` *    length (i.e. nothing is removed, but replacement is appended).` |
|         - | 4562 | ` *  $length (optional)` |
|         - | 4563 | ` *    If length is omitted, removes everything from offset to the end of the` |
|         - | 4564 | ` *    array.  If length is specified and is positive, then that many elements` |
|         - | 4565 | ` *    will be removed.  If length is specified and is negative then the end of` |
|         - | 4566 | ` *    the removed portion will be that many elements from the end of the array.` |
|         - | 4567 | ` *    If the resulting length is negative it is clamped to 0.` |
|         - | 4568 | ` *  $replacement (optional)` |
|         - | 4569 | ` *    If replacement array is specified, then the removed elements are replaced` |
|         - | 4570 | ` *    with elements from this array.` |
|         - | 4571 | ` *    If offset and length are such that nothing is removed, then the elements` |
|         - | 4572 | ` *    from the replacement array are inserted in the place specified by the` |
|         - | 4573 | ` *    offset.` |
|         - | 4574 | ` *    Note that keys in replacement array are not preserved.` |
|         - | 4575 | ` *    If replacement is just one element it is not necessary to put array()` |
|         - | 4576 | ` *    around it, unless the element is an array itself, an object or NULL.` |
|         - | 4577 | ` * Return` |
|         - | 4578 | ` *   A new array consisting of the extracted elements.` |
|         - | 4579 | ` */` |
|        54 | 4580 | `static int ph7_hashmap_splice(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|         4 | 4581 | `{` |
|         - | 4582 | `	ph7_hashmap_node *pCur,*pPrev,*pRnode,*pInsertAfter,*pNewNode;` |
|         - | 4583 | `	ph7_value *pArray,*pRvalue;` |
|         - | 4584 | `	ph7_hashmap *pMap,*pSrc,*pRep;` |
|         - | 4585 | `	int iLength,iOfft,i;` |
|         - | 4586 | `	sxi32 rc;` |
|        58 | 4587 | `	if( nArg < 2 ){` |
|         8 | 4588 | `		return PH7_VmThrowException(pCtx,` |
|         - | 4589 | `			"ArgumentCountError",` |
|         - | 4590 | `			"array_splice() expects at least 2 arguments, %d given",` |
|         2 | 4591 | `			nArg` |
|         - | 4592 | `			);` |
|         - | 4593 | `	}` |
|        52 | 4594 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|         4 | 4595 | `		return PH7_VmThrowException(pCtx,` |
|         - | 4596 | `			"TypeError",` |
|         - | 4597 | `			"array_splice(): Argument #1 ($array) must be of type array, %s given",` |
|         1 | 4598 | `			ph7_type_name(apArg[0])` |
|         - | 4599 | `			);` |
|         - | 4600 | `	}` |
|         - | 4601 | `	/* Point to the internal representation of the target array */` |
|        49 | 4602 | `	PH7_HashmapCowSeparate(pCtx->pVm, apArg[0]);` |
|        49 | 4603 | `	pSrc = (ph7_hashmap *)apArg[0]->x.pOther;` |
|         - | 4604 | `	/* Get the offset and clamp to valid range */` |
|        49 | 4605 | `	iOfft = ph7_value_to_int(apArg[1]);` |
|        49 | 4606 | `	if( iOfft < 0 ){` |
|         7 | 4607 | `		iOfft = (int)pSrc->nEntry + iOfft;` |
|         7 | 4608 | `		if( iOfft < 0 ){` |
|         3 | 4609 | `			iOfft = 0;` |
|         2 | 4610 | `		}` |
|        46 | 4611 | `	}else if( iOfft > (int)pSrc->nEntry ){` |
|         3 | 4612 | `		iOfft = (int)pSrc->nEntry;` |
|         1 | 4613 | `	}` |
|         - | 4614 | `	/* Get the length and clamp to valid range.` |
|         - | 4615 | `	 * NULL means "all remaining" (same as omitting the argument). */` |
|        49 | 4616 | `	iLength = (int)pSrc->nEntry - iOfft;` |
|        49 | 4617 | `	if( nArg > 2 && !ph7_value_is_null(apArg[2]) ){` |
|        31 | 4618 | `		iLength = ph7_value_to_int(apArg[2]);` |
|        31 | 4619 | `		if( iLength < 0 ){` |
|         7 | 4620 | `			iLength = ((int)pSrc->nEntry + iLength) - iOfft;` |
|         7 | 4621 | `			if( iLength < 0 ){` |
|         3 | 4622 | `				iLength = 0;` |
|         1 | 4623 | `			}` |
|         3 | 4624 | `		}` |
|        31 | 4625 | `		if( iOfft + iLength > (int)pSrc->nEntry ){` |
|         3 | 4626 | `			iLength = (int)pSrc->nEntry - iOfft;` |
|         1 | 4627 | `		}` |
|        15 | 4628 | `	}` |
|         - | 4629 | `	/* Create the result array for removed elements */` |
|        49 | 4630 | `	pArray = ph7_context_new_array(pCtx);` |
|        49 | 4631 | `	if( pArray == 0 ){` |
|       ! 0 | 4632 | `		ph7_result_null(pCtx);` |
|       ! 0 | 4633 | `		return PH7_OK;` |
|         - | 4634 | `	}` |
|         - | 4635 | `	/* Get replacement array if provided */` |
|        49 | 4636 | `	pRep = 0;` |
|        49 | 4637 | `	if( nArg > 3 ){` |
|        21 | 4638 | `		if( !ph7_value_is_array(apArg[3]) ){` |
|         - | 4639 | `			/* Perform an array cast */` |
|         3 | 4640 | `			PH7_MemObjToHashmap(apArg[3]);` |
|         3 | 4641 | `			if( ph7_value_is_array(apArg[3]) ){` |
|         3 | 4642 | `				pRep = (ph7_hashmap *)apArg[3]->x.pOther;` |
|         1 | 4643 | `			}` |
|         2 | 4644 | `		}else{` |
|        19 | 4645 | `			pRep = (ph7_hashmap *)apArg[3]->x.pOther;` |
|         - | 4646 | `		}` |
|        21 | 4647 | `		if( pRep ){` |
|         - | 4648 | `			/* Reset the loop cursor */` |
|        21 | 4649 | `			pRep->pCur = pRep->pFirst;` |
|        10 | 4650 | `		}` |
|        10 | 4651 | `	}` |
|         - | 4652 | `	/* Early return if nothing to remove and no replacement */` |
|        49 | 4653 | `	if( iLength < 1 && pRep == 0 ){` |
|         9 | 4654 | `		ph7_result_value(pCtx,pArray);` |
|         9 | 4655 | `		return PH7_OK;` |
|         - | 4656 | `	}` |
|         - | 4657 | `	/* Navigate to the offset position */` |
|        41 | 4658 | `	pCur = pSrc->pFirst;` |
|        85 | 4659 | `	for( i = 0 ; i < iOfft && pCur ; i++ ){` |
|        45 | 4660 | `		pCur = pCur->pPrev; /* Reverse link */` |
|        23 | 4661 | `	}` |
|         - | 4662 | `	/* Save the node just before the splice range as the insertion anchor.` |
|         - | 4663 | `	 * pCur->pNext is the backward link (previous node in iteration order).` |
|         - | 4664 | `	 * If pCur is NULL (offset == nEntry), the anchor is the last node. */` |
|        41 | 4665 | `	pInsertAfter = (pCur != 0) ? pCur->pNext : pSrc->pLast;` |
|         - | 4666 | `	/* Remove nodes in the splice range and copy them to the result array */` |
|        41 | 4667 | `	pMap = (ph7_hashmap *)pArray->x.pOther;` |
|       111 | 4668 | `	for( i = 0 ; i < iLength && pCur ; i++ ){` |
|        71 | 4669 | `		pPrev = pCur->pPrev;` |
|        71 | 4670 | `		rc = HashmapInsertNode(pMap,pCur,FALSE);` |
|        71 | 4671 | `		PH7_HashmapUnlinkNode(pCur,TRUE);` |
|        71 | 4672 | `		if( rc != SXRET_OK ){` |
|       ! 0 | 4673 | `			break;` |
|         - | 4674 | `		}` |
|        71 | 4675 | `		pCur = pPrev; /* Reverse link */` |
|        36 | 4676 | `	}` |
|         - | 4677 | `	/* Insert replacement elements at the correct position */` |
|        41 | 4678 | `	if( pRep ){` |
|         - | 4679 | `		ph7_value sSafeVal;` |
|        61 | 4680 | `		while( (pRnode = PH7_HashmapGetNextEntry(pRep)) != 0 ){` |
|        31 | 4681 | `			pRvalue = HashmapExtractNodeValue(pRnode);` |
|        31 | 4682 | `			if( pRvalue ){` |
|         - | 4683 | `				/* Make a stack copy before inserting.  HashmapInsert() may` |
|         - | 4684 | `				 * grow the VM memobj pool, which would invalidate pRvalue` |
|         - | 4685 | `				 * since it points into that same pool. */` |
|        31 | 4686 | `				sSafeVal = *pRvalue;` |
|        31 | 4687 | `				rc = HashmapInsert(pSrc,0,&sSafeVal);` |
|        31 | 4688 | `				if( rc == SXRET_OK && pSrc->pLast != 0 ){` |
|        31 | 4689 | `					pNewNode = pSrc->pLast;` |
|        31 | 4690 | `					HashmapMoveLastAfter(pSrc,pInsertAfter);` |
|        31 | 4691 | `					pInsertAfter = pNewNode;` |
|        15 | 4692 | `				}` |
|        15 | 4693 | `			}` |
|         1 | 4694 | `		}` |
|        10 | 4695 | `	}` |
|         - | 4696 | `	/* Return the freshly created array */` |
|        41 | 4697 | `	ph7_result_value(pCtx,pArray);` |
|        41 | 4698 | `	return PH7_OK;` |
|        31 | 4699 | `}` |
|         - | 4700 | `/*` |
|         - | 4701 | ` * bool in_array(value $needle,array $haystack[,bool $strict = FALSE ])` |
|         - | 4702 | ` *  Checks if a value exists in an array.` |
|         - | 4703 | ` * Parameters` |
|         - | 4704 | ` *  $needle` |
|         - | 4705 | ` *   The searched value.` |
|         - | 4706 | ` *   Note:` |
|         - | 4707 | ` *    If needle is a string, the comparison is done in a case-sensitive manner.` |
|         - | 4708 | ` * $haystack` |
|         - | 4709 | ` *  The target array.` |
|         - | 4710 | ` * $strict` |
|         - | 4711 | ` *  If the third parameter strict is set to TRUE then the in_array() function` |
|         - | 4712 | ` *  will also check the types of the needle in the haystack.` |
|         - | 4713 | ` */` |
|     32548 | 4714 | `static int ph7_hashmap_in_array(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|         5 | 4715 | `{` |
|         - | 4716 | `	ph7_value *pNeedle;` |
|         - | 4717 | `	int bStrict;` |
|         - | 4718 | `	int rc;` |
|     32553 | 4719 | `	if( nArg < 2 ){` |
|         - | 4720 | `		/* Missing argument,return FALSE */` |
|       ! 0 | 4721 | `		ph7_result_bool(pCtx,0);` |
|       ! 0 | 4722 | `		return PH7_OK;` |
|         - | 4723 | `	}` |
|     32553 | 4724 | `	pNeedle = apArg[0];` |
|     32553 | 4725 | `	bStrict = 0;` |
|     32553 | 4726 | `	if( nArg > 2 ){` |
|        41 | 4727 | `		bStrict = ph7_value_to_bool(apArg[2]);` |
|        20 | 4728 | `	}` |
|     32553 | 4729 | `	if( !ph7_value_is_array(apArg[1]) ){` |
|         - | 4730 | `		/* haystack must be an array,perform a standard comparison */` |
|       ! 0 | 4731 | `		rc = ph7_value_compare(pNeedle,apArg[1],bStrict);` |
|         - | 4732 | `		/* Set the comparison result */` |
|       ! 0 | 4733 | `		ph7_result_bool(pCtx,rc == 0);` |
|       ! 0 | 4734 | `		return PH7_OK;` |
|         - | 4735 | `	}` |
|         - | 4736 | `	/* Perform the lookup */` |
|     32553 | 4737 | `	rc = HashmapFindValue((ph7_hashmap *)apArg[1]->x.pOther,pNeedle,0,bStrict);` |
|         - | 4738 | `	/* Lookup result */` |
|     32553 | 4739 | `	ph7_result_bool(pCtx,rc == SXRET_OK);` |
|     32553 | 4740 | `	return PH7_OK;` |
|     16279 | 4741 | `}` |
|         - | 4742 | `/*` |
|         - | 4743 | ` * value array_search(value $needle,array $haystack[,bool $strict = false ])` |
|         - | 4744 | ` *  Searches the array for a given value and returns the corresponding key if successful.` |
|         - | 4745 | ` * Parameters` |
|         - | 4746 | ` * $needle` |
|         - | 4747 | ` *   The searched value.` |
|         - | 4748 | ` * $haystack` |
|         - | 4749 | ` *   The array.` |
|         - | 4750 | ` * $strict` |
|         - | 4751 | ` *  If the third parameter strict is set to TRUE then the array_search() function` |
|         - | 4752 | ` *  will search for identical elements in the haystack. This means it will also check` |
|         - | 4753 | ` *  the types of the needle in the haystack, and objects must be the same instance.` |
|         - | 4754 | ` * Return` |
|         - | 4755 | ` *  Returns the key for needle if it is found in the array, FALSE otherwise.` |
|         - | 4756 | ` */` |
|        28 | 4757 | `static int ph7_hashmap_search(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|         5 | 4758 | `{` |
|         - | 4759 | `	ph7_hashmap_node *pEntry;` |
|         - | 4760 | `	ph7_value *pVal,sNeedle;` |
|         - | 4761 | `	ph7_hashmap *pMap;` |
|         - | 4762 | `	ph7_value sVal;` |
|         - | 4763 | `	int bStrict;` |
|         - | 4764 | `	sxu32 n;` |
|         - | 4765 | `	int rc;` |
|        33 | 4766 | `	if( nArg < 2 ){` |
|         - | 4767 | `		/* Missing argument,throw ArgumentCountError */` |
|         8 | 4768 | `		return PH7_VmThrowException(pCtx,` |
|         - | 4769 | `			"ArgumentCountError",` |
|         - | 4770 | `			"array_search() expects at least 2 arguments, %d given",` |
|         2 | 4771 | `			nArg` |
|         - | 4772 | `			);` |
|         - | 4773 | `	}` |
|        27 | 4774 | `	bStrict = FALSE;` |
|        27 | 4775 | `	if( !ph7_value_is_array(apArg[1]) ){` |
|         - | 4776 | `		/* haystack must be an array,throw TypeError */` |
|         4 | 4777 | `		return PH7_VmThrowException(pCtx,` |
|         - | 4778 | `			"TypeError",` |
|         - | 4779 | `			"array_search(): Argument #2 ($haystack) must be of type array, %s given",` |
|         2 | 4780 | `			ph7_type_name(apArg[1])` |
|         - | 4781 | `			);` |
|         - | 4782 | `	}` |
|        24 | 4783 | `	if( nArg > 2 ){` |
|         - | 4784 | `		/* In PHP, non-scalar values for a bool-hinted parameter raise TypeError */` |
|        12 | 4785 | `		if( ph7_value_is_array(apArg[2]) \|\| ph7_value_is_object(apArg[2]) \|\| ph7_value_is_resource(apArg[2]) ){` |
|         4 | 4786 | `			return PH7_VmThrowException(pCtx,` |
|         - | 4787 | `				"TypeError",` |
|         - | 4788 | `				"array_search(): Argument #3 ($strict) must be of type bool, %s given",` |
|         2 | 4789 | `				ph7_type_name(apArg[2])` |
|         - | 4790 | `				);` |
|         - | 4791 | `		}` |
|         9 | 4792 | `		bStrict = ph7_value_to_bool(apArg[2]);` |
|         4 | 4793 | `	}` |
|         - | 4794 | `	/* Point to the internal representation of the internal hashmap */` |
|        21 | 4795 | `	pMap = (ph7_hashmap *)apArg[1]->x.pOther;` |
|         - | 4796 | `	/* Perform a linear search since we cannot sort the hashmap based on values */` |
|        21 | 4797 | `	PH7_MemObjInit(pMap->pVm,&sVal);` |
|        21 | 4798 | `	PH7_MemObjInit(pMap->pVm,&sNeedle);` |
|        21 | 4799 | `	pEntry = pMap->pFirst;` |
|        21 | 4800 | `	n = pMap->nEntry;` |
|        23 | 4801 | `	for(;;){` |
|        47 | 4802 | `		if( !n ){` |
|         9 | 4803 | `			break;` |
|         - | 4804 | `		}` |
|         - | 4805 | `		/* Extract node value */` |
|        39 | 4806 | `		pVal = HashmapExtractNodeValue(pEntry);` |
|        39 | 4807 | `		if( pVal ){` |
|         - | 4808 | `			/* Make a copy of the vuurent values since the comparison routine` |
|         - | 4809 | `			 * can change their type.` |
|         - | 4810 | `			 */` |
|        39 | 4811 | `			PH7_MemObjLoad(pVal,&sVal);` |
|        39 | 4812 | `			PH7_MemObjLoad(apArg[0],&sNeedle);` |
|        39 | 4813 | `			rc = PH7_MemObjCmp(&sNeedle,&sVal,bStrict,0);` |
|        39 | 4814 | `			PH7_MemObjRelease(&sVal);` |
|        39 | 4815 | `			PH7_MemObjRelease(&sNeedle);` |
|        39 | 4816 | `			if( rc == 0 ){` |
|         - | 4817 | `				/* Match found,return key */` |
|        13 | 4818 | `				if( pEntry->iType == HASHMAP_INT_NODE){` |
|         - | 4819 | `					/* INT key */` |
|         7 | 4820 | `					ph7_result_int64(pCtx,pEntry->xKey.iKey);` |
|         4 | 4821 | `				}else{` |
|         7 | 4822 | `					SyBlob *pKey = &pEntry->xKey.sKey;` |
|         - | 4823 | `					/* Blob key */` |
|         7 | 4824 | `					ph7_result_string(pCtx,(const char *)SyBlobData(pKey),(int)SyBlobLength(pKey));` |
|         - | 4825 | `				}` |
|        13 | 4826 | `				return PH7_OK;` |
|         - | 4827 | `			}` |
|        13 | 4828 | `		}` |
|         - | 4829 | `		/* Point to the next entry */` |
|        27 | 4830 | `		pEntry = pEntry->pPrev; /* Reverse link */` |
|        27 | 4831 | `		n--;` |
|         1 | 4832 | `	}` |
|         - | 4833 | `	/* No such value,return FALSE */` |
|         9 | 4834 | `	ph7_result_bool(pCtx,0);` |
|         9 | 4835 | `	return PH7_OK;` |
|        19 | 4836 | `}` |
|         - | 4837 | `/*` |
|         - | 4838 | ` * array array_diff(array $array1,array $array2,...)` |
|         - | 4839 | ` *  Computes the difference of arrays.` |
|         - | 4840 | ` * Parameters` |
|         - | 4841 | ` *  $array1` |
|         - | 4842 | ` *    The array to compare from` |
|         - | 4843 | ` *  $array2` |
|         - | 4844 | ` *    An array to compare against` |
|         - | 4845 | ` *  $...` |
|         - | 4846 | ` *   More arrays to compare against` |
|         - | 4847 | ` * Return` |
|         - | 4848 | ` *  Returns an array containing all the entries from array1 that` |
|         - | 4849 | ` *  are not present in any of the other arrays.` |
|         - | 4850 | ` */` |
|        22 | 4851 | `static int ph7_hashmap_diff(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|         4 | 4852 | `{` |
|         - | 4853 | `	ph7_hashmap_node *pEntry;` |
|         - | 4854 | `	ph7_hashmap *pSrc,*pMap;` |
|         - | 4855 | `	ph7_value *pArray;` |
|         - | 4856 | `	ph7_value *pVal;` |
|         - | 4857 | `	sxi32 rc;` |
|         - | 4858 | `	sxu32 n;` |
|         - | 4859 | `	int i;` |
|         - | 4860 | `	/* Validate arguments to mimic PHP behaviour. Earlier versions simply` |
|         - | 4861 | `	 * returned NULL when the caller passed invalid parameters which made` |
|         - | 4862 | `	 * debugging difficult. */` |
|        26 | 4863 | `	if( nArg < 1 ){` |
|         4 | 4864 | `		return PH7_VmThrowException(pCtx,` |
|         - | 4865 | `			"ArgumentCountError",` |
|         - | 4866 | `			"array_diff() expects at least 1 argument, %d given",` |
|         1 | 4867 | `			nArg` |
|         - | 4868 | `			);` |
|         - | 4869 | `	}` |
|        23 | 4870 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|         4 | 4871 | `		return PH7_VmThrowException(pCtx,` |
|         - | 4872 | `			"TypeError",` |
|         - | 4873 | `			"array_diff(): Argument #1 ($array) must be of type array, %s given",` |
|         1 | 4874 | `			ph7_type_name(apArg[0])` |
|         - | 4875 | `			);` |
|         - | 4876 | `	}` |
|        36 | 4877 | `	for(i = 1 ; i < nArg ; i++){` |
|        20 | 4878 | `		if( !ph7_value_is_array(apArg[i]) ){` |
|         4 | 4879 | `			return PH7_VmThrowException(pCtx,` |
|         - | 4880 | `				"TypeError",` |
|         - | 4881 | `				"array_diff(): Argument #%d must be of type array, %s given",` |
|         1 | 4882 | `				i + 1,` |
|         2 | 4883 | `				ph7_type_name(apArg[i])` |
|         - | 4884 | `				);` |
|         - | 4885 | `		}` |
|         9 | 4886 | `	}` |
|        17 | 4887 | `	if( nArg == 1 ){` |
|         - | 4888 | `		/* Return the first array since we cannot perform a diff */` |
|         3 | 4889 | `		ph7_result_value(pCtx,apArg[0]);` |
|         3 | 4890 | `		return PH7_OK;` |
|         - | 4891 | `	}` |
|         - | 4892 | `	/* Create a new array */` |
|        15 | 4893 | `	pArray = ph7_context_new_array(pCtx);` |
|        15 | 4894 | `	if( pArray == 0 ){` |
|       ! 0 | 4895 | `		ph7_result_null(pCtx);` |
|       ! 0 | 4896 | `		return PH7_OK;` |
|         - | 4897 | `	}` |
|         - | 4898 | `	/* Point to the internal representation of the source hashmap */` |
|        15 | 4899 | `	pSrc = (ph7_hashmap *)apArg[0]->x.pOther;` |
|         - | 4900 | `	/* Perform the diff */` |
|        15 | 4901 | `	pEntry = pSrc->pFirst;` |
|        15 | 4902 | `	n = pSrc->nEntry;` |
|        27 | 4903 | `	for(;;){` |
|        55 | 4904 | `		if( n < 1 ){` |
|        15 | 4905 | `			break;` |
|         - | 4906 | `		}` |
|         - | 4907 | `		/* Extract the node value */` |
|        41 | 4908 | `		pVal = HashmapExtractNodeValue(pEntry);` |
|        41 | 4909 | `		if( pVal ){` |
|        69 | 4910 | `			for( i = 1 ; i < nArg ; i++ ){` |
|         - | 4911 | `				/* Point to the internal representation of the hashmap */` |
|        45 | 4912 | `				pMap = (ph7_hashmap *)apArg[i]->x.pOther;` |
|         - | 4913 | `				/* Perform the lookup */` |
|        45 | 4914 | `				rc = HashmapFindValue(pMap,pVal,0,TRUE);` |
|        45 | 4915 | `				if( rc == SXRET_OK ){` |
|         - | 4916 | `					/* Value exist */` |
|        17 | 4917 | `					break;` |
|         - | 4918 | `				}` |
|        15 | 4919 | `			}` |
|        41 | 4920 | `			if( i >= nArg ){` |
|         - | 4921 | `				/* Perform the insertion */` |
|        25 | 4922 | `				HashmapInsertNode((ph7_hashmap *)pArray->x.pOther,pEntry,TRUE);` |
|        12 | 4923 | `			}` |
|        20 | 4924 | `		}` |
|         - | 4925 | `		/* Point to the next entry */` |
|        41 | 4926 | `		pEntry = pEntry->pPrev; /* Reverse link */` |
|        41 | 4927 | `		n--;` |
|         1 | 4928 | `	}` |
|         - | 4929 | `	/* Return the freshly created array */` |
|        15 | 4930 | `	ph7_result_value(pCtx,pArray);` |
|        15 | 4931 | `	return PH7_OK;` |
|        15 | 4932 | `}` |
|         - | 4933 | `/*` |
|         - | 4934 | ` * array array_udiff(array $array1,array $array2,...,$callback)` |
|         - | 4935 | ` *  Computes the difference of arrays by using a callback function for data comparison.` |
|         - | 4936 | ` * Parameters` |
|         - | 4937 | ` *  $array1` |
|         - | 4938 | ` *    The array to compare from` |
|         - | 4939 | ` *  $array2` |
|         - | 4940 | ` *    An array to compare against` |
|         - | 4941 | ` *  $...` |
|         - | 4942 | ` *   More arrays to compare against.` |
|         - | 4943 | ` * $callback` |
|         - | 4944 | ` *  The callback comparison function.` |
|         - | 4945 | ` *  The comparison function must return an integer less than, equal to, or greater than zero` |
|         - | 4946 | ` *  if the first argument is considered to be respectively less than, equal to, or greater` |
|         - | 4947 | ` *  than the second.` |
|         - | 4948 | ` *     int callback ( mixed $a, mixed $b )` |
|         - | 4949 | ` * Return` |
|         - | 4950 | ` *  Returns an array containing all the entries from array1 that` |
|         - | 4951 | ` *  are not present in any of the other arrays.` |
|         - | 4952 | ` */` |
|        22 | 4953 | `static int ph7_hashmap_udiff(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|         5 | 4954 | `{` |
|         - | 4955 | `	ph7_hashmap_node *pEntry;` |
|         - | 4956 | `	ph7_hashmap *pSrc,*pMap;` |
|         - | 4957 | `	ph7_value *pCallback;` |
|         - | 4958 | `	ph7_value *pArray;` |
|         - | 4959 | `	ph7_value *pVal;` |
|         - | 4960 | `	sxi32 rc;` |
|         - | 4961 | `	sxu32 n;` |
|         - | 4962 | `	int i;` |
|         - | 4963 |  |
|         - | 4964 | `	/* Ensure the argument count matches PHP behaviour. */` |
|        27 | 4965 | `	if( nArg < 2 ){` |
|         4 | 4966 | `		return PH7_VmThrowException(pCtx,` |
|         - | 4967 | `			"ArgumentCountError",` |
|         - | 4968 | `			"array_udiff() expects at least 2 arguments, %d given",` |
|         1 | 4969 | `			nArg` |
|         - | 4970 | `			);` |
|         - | 4971 | `	}` |
|        25 | 4972 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|         4 | 4973 | `		return PH7_VmThrowException(pCtx,` |
|         - | 4974 | `			"TypeError",` |
|         - | 4975 | `			"array_udiff(): Argument #1 ($array) must be of type array, %s given",` |
|         1 | 4976 | `			ph7_type_name(apArg[0])` |
|         - | 4977 | `			);` |
|         - | 4978 | `	}` |
|         - | 4979 |  |
|        23 | 4980 | `	if( nArg == 2 ){` |
|         - | 4981 | `		/* Only the original array and the callback were provided. */` |
|         - | 4982 | `		/* Nevertheless, we still validate the callback after verifying any` |
|         - | 4983 | `		 * intermediate array arguments to match PHP's left-to-right parameter` |
|         - | 4984 | `		 * validation order.` |
|         - | 4985 | `		 */` |
|         4 | 4986 | `	} else {` |
|         - | 4987 | `		/* Ensure intermediary arguments are arrays (matches PHP strict typing). */` |
|        27 | 4988 | `		for( i = 1 ; i < nArg - 1; i++ ){` |
|        19 | 4989 | `			if( !ph7_value_is_array(apArg[i]) ){` |
|        11 | 4990 | `				return PH7_VmThrowException(pCtx,` |
|         - | 4991 | `					"TypeError",` |
|         - | 4992 | `					"array_udiff(): Argument #%d must be of type array, %s given",` |
|         3 | 4993 | `					i + 1,` |
|         6 | 4994 | `					ph7_type_name(apArg[i])` |
|         - | 4995 | `					);` |
|         - | 4996 | `			}` |
|         7 | 4997 | `		}` |
|         - | 4998 | `	}` |
|         - | 4999 |  |
|         - | 5000 | `	/* Identify the callback (always expected as the last argument). */` |
|        16 | 5001 | `	pCallback = apArg[nArg - 1];` |
|         - | 5002 | `	/* Validate the callback to match PHP's error messages. */` |
|        16 | 5003 | `	if( !ph7_value_is_callable(pCallback) ){` |
|         9 | 5004 | `		if( ph7_value_is_array(pCallback) ){` |
|         4 | 5005 | `			return PH7_VmThrowException(pCtx,` |
|         - | 5006 | `				"TypeError",` |
|         - | 5007 | `				"array_udiff(): Argument #%d must be a valid callback, array callback must have exactly two members",` |
|         1 | 5008 | `				nArg` |
|         - | 5009 | `				);` |
|         - | 5010 | `		}` |
|         6 | 5011 | `		if( ph7_value_is_string(pCallback) ){` |
|         - | 5012 | `			int len;` |
|         3 | 5013 | `			const char *zName = ph7_value_to_string(pCallback, &len);` |
|         4 | 5014 | `			return PH7_VmThrowException(pCtx,` |
|         - | 5015 | `				"TypeError",` |
|         - | 5016 | `				"array_udiff(): Argument #%d must be a valid callback, function \"%s\" not found or invalid function name",` |
|         1 | 5017 | `				nArg,` |
|         1 | 5018 | `				zName` |
|         - | 5019 | `				);` |
|         - | 5020 | `		}` |
|         4 | 5021 | `		return PH7_VmThrowException(pCtx,` |
|         - | 5022 | `			"TypeError",` |
|         - | 5023 | `			"array_udiff(): Argument #%d must be a valid callback, no array or string given",` |
|         1 | 5024 | `			nArg` |
|         - | 5025 | `			);` |
|         - | 5026 | `	}` |
|         - | 5027 |  |
|         7 | 5028 | `	if( nArg == 2 ){` |
|         - | 5029 | `		/* Only the original array and the callback were provided. */` |
|         3 | 5030 | `		ph7_result_value(pCtx,apArg[0]);` |
|         3 | 5031 | `		return PH7_OK;` |
|         - | 5032 | `	}` |
|         - | 5033 |  |
|         - | 5034 | `	/* Create a new array */` |
|         5 | 5035 | `	pArray = ph7_context_new_array(pCtx);` |
|         5 | 5036 | `	if( pArray == 0 ){` |
|       ! 0 | 5037 | `		ph7_result_null(pCtx);` |
|       ! 0 | 5038 | `		return PH7_OK;` |
|         - | 5039 | `	}` |
|         - | 5040 | `	/* Point to the internal representation of the source hashmap */` |
|         5 | 5041 | `	pSrc = (ph7_hashmap *)apArg[0]->x.pOther;` |
|         - | 5042 | `	/* Perform the diff */` |
|         5 | 5043 | `	pEntry = pSrc->pFirst;` |
|         5 | 5044 | `	n = pSrc->nEntry;` |
|         5 | 5045 | `	pCtx->pVm->iCmpCallbackExc = 0;` |
|         5 | 5046 | `	for(;;){` |
|        11 | 5047 | `		if( n < 1 ){` |
|         3 | 5048 | `			break;` |
|         - | 5049 | `		}` |
|         - | 5050 | `		/* Extract the node value */` |
|         9 | 5051 | `		pVal = HashmapExtractNodeValue(pEntry);` |
|         9 | 5052 | `		if( pVal ){` |
|        15 | 5053 | `			for( i = 1 ; i < nArg - 1; i++ ){` |
|         - | 5054 | `				/* Point to the internal representation of the hashmap */` |
|         9 | 5055 | `				pMap = (ph7_hashmap *)apArg[i]->x.pOther;` |
|         - | 5056 | `				/* Perform the lookup */` |
|         9 | 5057 | `				rc = HashmapFindValueByCallback(pMap,pVal,pCallback,0);` |
|         9 | 5058 | `				if( rc == SXRET_OK ){` |
|         - | 5059 | `					/* Value exist */` |
|         3 | 5060 | `					break;` |
|         - | 5061 | `				}` |
|         4 | 5062 | `			}` |
|         9 | 5063 | `			if( pCtx->pVm->iCmpCallbackExc ){` |
|         - | 5064 | `				/* The comparison callback raised: propagate so the dispatcher` |
|         - | 5065 | `				 * unwinds, before any spurious insertion into the result. */` |
|         3 | 5066 | `				pCtx->pVm->iCmpCallbackExc = 0;` |
|         3 | 5067 | `				return PH7_EXCEPTION;` |
|         - | 5068 | `			}` |
|         7 | 5069 | `			if( i >= (nArg - 1)){` |
|         - | 5070 | `				/* Perform the insertion */` |
|         5 | 5071 | `				HashmapInsertNode((ph7_hashmap *)pArray->x.pOther,pEntry,TRUE);` |
|         2 | 5072 | `			}` |
|         3 | 5073 | `		}` |
|         - | 5074 | `		/* Point to the next entry */` |
|         7 | 5075 | `		pEntry = pEntry->pPrev; /* Reverse link */` |
|         7 | 5076 | `		n--;` |
|         1 | 5077 | `	}` |
|         - | 5078 | `	/* Return the freshly created array */` |
|         3 | 5079 | `	ph7_result_value(pCtx,pArray);` |
|         3 | 5080 | `	return PH7_OK;` |
|        16 | 5081 | `}` |
|         - | 5082 | `/*` |
|         - | 5083 | ` * array array_diff_assoc(array $array1,array $array2,...)` |
|         - | 5084 | ` *  Computes the difference of arrays with additional index check.` |
|         - | 5085 | ` * Parameters` |
|         - | 5086 | ` *  $array1` |
|         - | 5087 | ` *    The array to compare from` |
|         - | 5088 | ` *  $array2` |
|         - | 5089 | ` *    An array to compare against` |
|         - | 5090 | ` *  $...` |
|         - | 5091 | ` *   More arrays to compare against` |
|         - | 5092 | ` * Return` |
|         - | 5093 | ` *  Returns an array containing all the entries from array1 that` |
|         - | 5094 | ` *  are not present in any of the other arrays.` |
|         - | 5095 | ` */` |
|        20 | 5096 | `static int ph7_hashmap_diff_assoc(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|         5 | 5097 | `{` |
|         - | 5098 | `	ph7_hashmap_node *pN1,*pN2,*pEntry;` |
|         - | 5099 | `	ph7_hashmap *pSrc,*pMap;` |
|         - | 5100 | `	ph7_value *pArray;` |
|         - | 5101 | `	ph7_value *pVal;` |
|         - | 5102 | `	sxi32 rc;` |
|         - | 5103 | `	sxu32 n;` |
|         - | 5104 | `	int i;` |
|         - | 5105 | `	/* Ensure the argument list is valid, emitting the same errors PHP` |
|         - | 5106 | `	 * would produce. This makes behaviour predictable and allows the` |
|         - | 5107 | `	 * accompanying integration tests to pass. */` |
|        25 | 5108 | `	if( nArg < 1 ){` |
|         4 | 5109 | `		return PH7_VmThrowException(pCtx,` |
|         - | 5110 | `			"ArgumentCountError",` |
|         - | 5111 | `			"array_diff_assoc() expects at least 1 argument, %d given",` |
|         1 | 5112 | `			nArg` |
|         - | 5113 | `			);` |
|         - | 5114 | `	}` |
|        22 | 5115 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|         4 | 5116 | `		return PH7_VmThrowException(pCtx,` |
|         - | 5117 | `			"TypeError",` |
|         - | 5118 | `			"array_diff_assoc(): Argument #1 ($array) must be of type array, %s given",` |
|         1 | 5119 | `			ph7_type_name(apArg[0])` |
|         - | 5120 | `			);` |
|         - | 5121 | `	}` |
|        33 | 5122 | `	for(i = 1 ; i < nArg ; i++){` |
|        21 | 5123 | `		if( !ph7_value_is_array(apArg[i]) ){` |
|         8 | 5124 | `			return PH7_VmThrowException(pCtx,` |
|         - | 5125 | `				"TypeError",` |
|         - | 5126 | `				"array_diff_assoc(): Argument #%d must be of type array, %s given",` |
|         2 | 5127 | `				i + 1,` |
|         4 | 5128 | `				ph7_type_name(apArg[i])` |
|         - | 5129 | `				);` |
|         - | 5130 | `		}` |
|         9 | 5131 | `	}` |
|        13 | 5132 | `	if( nArg == 1 ){` |
|         - | 5133 | `		/* Return the first array since we cannot perform a diff */` |
|         3 | 5134 | `		ph7_result_value(pCtx,apArg[0]);` |
|         3 | 5135 | `		return PH7_OK;` |
|         - | 5136 | `	}` |
|         - | 5137 | `	/* Create a new array */` |
|        11 | 5138 | `	pArray = ph7_context_new_array(pCtx);` |
|        11 | 5139 | `	if( pArray == 0 ){` |
|       ! 0 | 5140 | `		ph7_result_null(pCtx);` |
|       ! 0 | 5141 | `		return PH7_OK;` |
|         - | 5142 | `	}` |
|         - | 5143 | `	/* Point to the internal representation of the source hashmap */` |
|        11 | 5144 | `	pSrc = (ph7_hashmap *)apArg[0]->x.pOther;` |
|         - | 5145 | `	/* Perform the diff */` |
|        11 | 5146 | `	pEntry = pSrc->pFirst;` |
|        11 | 5147 | `	n = pSrc->nEntry;` |
|        11 | 5148 | `	pN1 = pN2 = 0;` |
|        29 | 5149 | `	for(;;){` |
|         - | 5150 | `		int keep;` |
|        35 | 5151 | `		if( n < 1 ){` |
|        11 | 5152 | `			break;` |
|         - | 5153 | `		}` |
|         - | 5154 | `		/* assume the element should be kept until we find a match */` |
|        25 | 5155 | `		keep = 1;` |
|        41 | 5156 | `		for( i = 1 ; i < nArg ; i++ ){` |
|         - | 5157 | `			/* all arguments have been validated already, so cast directly */` |
|        29 | 5158 | `			pMap = (ph7_hashmap *)apArg[i]->x.pOther;` |
|         - | 5159 | `			/* Perform a key lookup first */` |
|        29 | 5160 | `			if( pEntry->iType == HASHMAP_INT_NODE ){` |
|        13 | 5161 | `				rc = HashmapLookupIntKey(pMap,pEntry->xKey.iKey,&pN1);` |
|         7 | 5162 | `			}else{` |
|        17 | 5163 | `				rc = HashmapLookupBlobKey(pMap,SyBlobData(&pEntry->xKey.sKey),SyBlobLength(&pEntry->xKey.sKey),&pN1);` |
|         - | 5164 | `			}` |
|        29 | 5165 | `			if( rc != SXRET_OK ){` |
|         - | 5166 | `				/* this array does not contain the key, continue checking others */` |
|        15 | 5167 | `				continue;` |
|         - | 5168 | `			}` |
|         - | 5169 | `			/* key exists; check that value stored in the matching node is equal */` |
|        15 | 5170 | `			pVal = HashmapExtractNodeValue(pEntry);` |
|        15 | 5171 | `			if( pVal ){` |
|         - | 5172 | `				/* directly compare with value at pN1 rather than searching again */` |
|        15 | 5173 | `				ph7_value *pVal2 = HashmapExtractNodeValue(pN1);` |
|        15 | 5174 | `				if( pVal2 ){` |
|        15 | 5175 | `					sxi32 cmp = PH7_MemObjCmp(pVal, pVal2, TRUE, 0);` |
|        15 | 5176 | `					if( cmp == 0 ){` |
|         - | 5177 | `						/* identical key+value found in one of the arrays => drop it */` |
|        13 | 5178 | `						keep = 0;` |
|        13 | 5179 | `						break;` |
|         - | 5180 | `					}` |
|         1 | 5181 | `				}` |
|         1 | 5182 | `			}` |
|         2 | 5183 | `		}` |
|        25 | 5184 | `		if( keep ){` |
|         - | 5185 | `			/* Perform the insertion */` |
|        13 | 5186 | `			HashmapInsertNode((ph7_hashmap *)pArray->x.pOther,pEntry,TRUE);` |
|         6 | 5187 | `		}` |
|         - | 5188 | `		/* Point to the next entry */` |
|        25 | 5189 | `		pEntry = pEntry->pPrev; /* Reverse link */` |
|        25 | 5190 | `		n--;` |
|         1 | 5191 | `	}` |
|         - | 5192 | `	/* Return the freshly created array */` |
|        11 | 5193 | `	ph7_result_value(pCtx,pArray);` |
|        11 | 5194 | `	return PH7_OK;` |
|        15 | 5195 | `}` |
|         - | 5196 | `/*` |
|         - | 5197 | ` * array array_diff_uassoc(array $array1,array $array2,...,callback $key_compare_func)` |
|         - | 5198 | ` *  Computes the difference of arrays with additional index check which is performed` |
|         - | 5199 | ` *  by a user supplied callback function.` |
|         - | 5200 | ` * Parameters` |
|         - | 5201 | ` *  $array1` |
|         - | 5202 | ` *    The array to compare from` |
|         - | 5203 | ` *  $array2` |
|         - | 5204 | ` *    An array to compare against` |
|         - | 5205 | ` *  $...` |
|         - | 5206 | ` *   More arrays to compare against.` |
|         - | 5207 | ` *  $key_compare_func` |
|         - | 5208 | ` *   Callback function to use. The callback function must return an integer` |
|         - | 5209 | ` *   less than, equal to, or greater than zero if the first argument is considered` |
|         - | 5210 | ` *   to be respectively less than, equal to, or greater than the second.` |
|         - | 5211 | ` * Return` |
|         - | 5212 | ` *  Returns an array containing all the entries from array1 that` |
|         - | 5213 | ` *  are not present in any of the other arrays.` |
|         - | 5214 | ` */` |
|        24 | 5215 | `static int ph7_hashmap_diff_uassoc(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|         5 | 5216 | `{` |
|         - | 5217 | `	ph7_hashmap_node *pEntry;` |
|         - | 5218 | `	ph7_hashmap *pSrc,*pMap;` |
|         - | 5219 | `	ph7_value *pCallback;` |
|         - | 5220 | `	ph7_value *pArray;` |
|         - | 5221 | `	sxi32 rc;` |
|         - | 5222 | `	sxu32 n;` |
|         - | 5223 | `	int i;` |
|         - | 5224 |  |
|         - | 5225 | `	/* Argument validation mimicking PHP errors. */` |
|        29 | 5226 | `	if( nArg < 2 ){` |
|         4 | 5227 | `		return PH7_VmThrowException(pCtx,` |
|         - | 5228 | `			"ArgumentCountError",` |
|         - | 5229 | `			"array_diff_uassoc() expects at least 2 arguments, %d given",` |
|         1 | 5230 | `			nArg` |
|         - | 5231 | `			);` |
|         - | 5232 | `	}` |
|        26 | 5233 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|         4 | 5234 | `		return PH7_VmThrowException(pCtx,` |
|         - | 5235 | `			"TypeError",` |
|         - | 5236 | `			"array_diff_uassoc(): Argument #1 ($array) must be of type array, %s given",` |
|         1 | 5237 | `			ph7_type_name(apArg[0])` |
|         - | 5238 | `			);` |
|         - | 5239 | `	}` |
|         - | 5240 | `	/* Intermediate arguments (except last) must be arrays. Last argument is` |
|         - | 5241 | `	 * expected to be a callback. */` |
|        38 | 5242 | `	for(i = 1 ; i < nArg - 1; i++){` |
|        19 | 5243 | `		if( !ph7_value_is_array(apArg[i]) ){` |
|         4 | 5244 | `			return PH7_VmThrowException(pCtx,` |
|         - | 5245 | `				"TypeError",` |
|         - | 5246 | `				"array_diff_uassoc(): Argument #%d must be of type array, %s given",` |
|         1 | 5247 | `				i + 1,` |
|         2 | 5248 | `				ph7_type_name(apArg[i])` |
|         - | 5249 | `				);` |
|         - | 5250 | `		}` |
|         9 | 5251 | `	}` |
|         - | 5252 | `	/* Point to the callback value */` |
|        22 | 5253 | `	pCallback = apArg[nArg - 1];` |
|        22 | 5254 | `	if( !ph7_value_is_callable(pCallback) ){` |
|         - | 5255 | `		/* Compose an error message that closely matches PHP output. When the` |
|         - | 5256 | `		 * argument is an array of the wrong shape we include an extra clause.` |
|         - | 5257 | `		 * If the value is neither array nor string, PHP says "no array or` |
|         - | 5258 | `		 * string given" which we also reproduce. */` |
|         9 | 5259 | `		if( ph7_value_is_array(pCallback) ){` |
|         - | 5260 | `			/* ARRAY CALLBACK must have exactly two members */` |
|         4 | 5261 | `			return PH7_VmThrowException(pCtx,` |
|         - | 5262 | `				"TypeError",` |
|         - | 5263 | `				"array_diff_uassoc(): Argument #%d must be a valid callback, array callback must have exactly two members",` |
|         1 | 5264 | `				nArg` |
|         - | 5265 | `				);` |
|         - | 5266 | `		}` |
|         6 | 5267 | `		if( !ph7_value_is_string(pCallback) ){` |
|         - | 5268 | `			/* neither array nor string */` |
|         8 | 5269 | `			return PH7_VmThrowException(pCtx,` |
|         - | 5270 | `				"TypeError",` |
|         - | 5271 | `				"array_diff_uassoc(): Argument #%d must be a valid callback, no array or string given",` |
|         2 | 5272 | `				nArg` |
|         - | 5273 | `				);` |
|         - | 5274 | `		}` |
|         - | 5275 | `		/* Fallback for string (non-callable) or other leftover cases */` |
|       ! 0 | 5276 | `		return PH7_VmThrowException(pCtx,` |
|         - | 5277 | `			"TypeError",` |
|         - | 5278 | `			"array_diff_uassoc(): Argument #%d must be a valid callback, %s given",` |
|       ! 0 | 5279 | `			nArg,` |
|       ! 0 | 5280 | `			ph7_type_name(pCallback)` |
|         - | 5281 | `			);` |
|         - | 5282 | `	}` |
|        13 | 5283 | `	if( nArg == 2 ){` |
|         - | 5284 | `		/* If we only have the first array and the callback, just return the` |
|         - | 5285 | `		 * input array. */` |
|         3 | 5286 | `		ph7_result_value(pCtx,apArg[0]);` |
|         3 | 5287 | `		return PH7_OK;` |
|         - | 5288 | `	}` |
|         - | 5289 | `	/* Create a new array */` |
|        11 | 5290 | `	pArray = ph7_context_new_array(pCtx);` |
|        11 | 5291 | `	if( pArray == 0 ){` |
|       ! 0 | 5292 | `		ph7_result_null(pCtx);` |
|       ! 0 | 5293 | `		return PH7_OK;` |
|         - | 5294 | `	}` |
|         - | 5295 | `	/* Point to the internal representation of the source hashmap */` |
|        11 | 5296 | `	pSrc = (ph7_hashmap *)apArg[0]->x.pOther;` |
|         - | 5297 | `	/* Perform the diff */` |
|        11 | 5298 | `	pEntry = pSrc->pFirst;` |
|        11 | 5299 | `	n = pSrc->nEntry;` |
|        21 | 5300 | `	for(;;){` |
|         - | 5301 | `		int keep;` |
|        27 | 5302 | `		if( n < 1 ){` |
|         9 | 5303 | `			break;` |
|         - | 5304 | `		}` |
|        19 | 5305 | `		keep = 1;` |
|        31 | 5306 | `		for( i = 1 ; i < nArg - 1; i++ ){` |
|         - | 5307 | `			/* each of these must already be arrays thanks to earlier validation */` |
|        23 | 5308 | `			pMap = (ph7_hashmap *)apArg[i]->x.pOther;` |
|         - | 5309 | `			/* we must compare keys via callback, not by direct lookup */` |
|        23 | 5310 | `			ph7_hashmap_node *pIt = pMap->pFirst;` |
|        45 | 5311 | `			while( pIt ){` |
|         - | 5312 | `				/* build temporary key values for callback */` |
|         - | 5313 | `				ph7_value key1, key2, result;` |
|         - | 5314 | `				/* initialise only once using the appropriate helper */` |
|        33 | 5315 | `				if( pEntry->iType == HASHMAP_INT_NODE ){` |
|       ! 0 | 5316 | `					PH7_MemObjInitFromInt(pMap->pVm,&key1,pEntry->xKey.iKey);` |
|       ! 0 | 5317 | `				}else{` |
|         - | 5318 | `					SyString sStr;` |
|        33 | 5319 | `					SyStringInitFromBuf(&sStr,` |
|         - | 5320 | `						SyBlobData(&pEntry->xKey.sKey),` |
|         - | 5321 | `						SyBlobLength(&pEntry->xKey.sKey));` |
|        33 | 5322 | `					PH7_MemObjInitFromString(pMap->pVm,&key1,&sStr);` |
|         - | 5323 | `				}` |
|        33 | 5324 | `				if( pIt->iType == HASHMAP_INT_NODE ){` |
|       ! 0 | 5325 | `					PH7_MemObjInitFromInt(pMap->pVm,&key2,pIt->xKey.iKey);` |
|       ! 0 | 5326 | `				}else{` |
|         - | 5327 | `					SyString sStr;` |
|        33 | 5328 | `					SyStringInitFromBuf(&sStr,` |
|         - | 5329 | `						SyBlobData(&pIt->xKey.sKey),` |
|         - | 5330 | `						SyBlobLength(&pIt->xKey.sKey));` |
|        33 | 5331 | `					PH7_MemObjInitFromString(pMap->pVm,&key2,&sStr);` |
|         - | 5332 | `				}` |
|        33 | 5333 | `				PH7_MemObjInit(pMap->pVm,&result);` |
|         - | 5334 | `				/* call user callback with (key1, key2) */` |
|         - | 5335 | `				{` |
|         - | 5336 | `					ph7_value *apK[2];` |
|        33 | 5337 | `					apK[0] = &key1;` |
|        33 | 5338 | `					apK[1] = &key2;` |
|        33 | 5339 | `					rc = PH7_VmCallUserFunction(pMap->pVm,pCallback,2,apK,&result);` |
|         - | 5340 | `				}` |
|        33 | 5341 | `				if( rc == PH7_EXCEPTION ){` |
|         - | 5342 | `					/* The key comparison callback raised. Unlike array_udiff/` |
|         - | 5343 | `					 * array_uintersect (which signal back from` |
|         - | 5344 | `					 * HashmapFindValueByCallback via pVm->iCmpCallbackExc), this` |
|         - | 5345 | `					 * function invokes the callback inline, so it cleans up its own` |
|         - | 5346 | `					 * temporaries and propagates the exception directly. */` |
|         3 | 5347 | `					PH7_MemObjRelease(&result);` |
|         3 | 5348 | `					PH7_MemObjRelease(&key1);` |
|         3 | 5349 | `					PH7_MemObjRelease(&key2);` |
|         3 | 5350 | `					return PH7_EXCEPTION;` |
|         - | 5351 | `				}` |
|        31 | 5352 | `				if( rc == SXRET_OK ){` |
|        31 | 5353 | `					if( (result.iFlags & MEMOBJ_INT) == 0 ){` |
|       ! 0 | 5354 | `						PH7_MemObjToInteger(&result);` |
|       ! 0 | 5355 | `					}` |
|        31 | 5356 | `					if( result.x.iVal == 0 ){` |
|         - | 5357 | `						/* keys considered equal by callback; now compare values */` |
|        13 | 5358 | `						ph7_value *pVal1 = HashmapExtractNodeValue(pEntry);` |
|        13 | 5359 | `						ph7_value *pVal2 = HashmapExtractNodeValue(pIt);` |
|        13 | 5360 | `						if( pVal1 && pVal2 ){` |
|        13 | 5361 | `							if( PH7_MemObjCmp(pVal1,pVal2,TRUE,0) == 0 ){` |
|         9 | 5362 | `								keep = 0;` |
|         9 | 5363 | `								PH7_MemObjRelease(&result);` |
|         - | 5364 | `								/* release keys too before breaking */` |
|         9 | 5365 | `								PH7_MemObjRelease(&key1);` |
|         9 | 5366 | `								PH7_MemObjRelease(&key2);` |
|         9 | 5367 | `								break;` |
|         - | 5368 | `							}` |
|         2 | 5369 | `						}` |
|         2 | 5370 | `					}` |
|        11 | 5371 | `				}` |
|        23 | 5372 | `				PH7_MemObjRelease(&result);` |
|        23 | 5373 | `				PH7_MemObjRelease(&key1);` |
|        23 | 5374 | `				PH7_MemObjRelease(&key2);` |
|         - | 5375 | `				/* move to next node */` |
|        23 | 5376 | `				pIt = pIt->pPrev;` |
|        23 | 5377 | `				if( keep == 0 ) break;` |
|         1 | 5378 | `			}` |
|        21 | 5379 | `			if( keep == 0 ) break;` |
|         7 | 5380 | `		}` |
|        17 | 5381 | `		if( keep ){` |
|         - | 5382 | `			/* Perform the insertion */` |
|         9 | 5383 | `			HashmapInsertNode((ph7_hashmap *)pArray->x.pOther,pEntry,TRUE);` |
|         4 | 5384 | `		}` |
|         - | 5385 | `		/* Point to the next entry */` |
|        17 | 5386 | `		pEntry = pEntry->pPrev; /* Reverse link */` |
|        17 | 5387 | `		n--;` |
|         1 | 5388 | `	}` |
|         - | 5389 | `	/* Return the freshly created array */` |
|         9 | 5390 | `	ph7_result_value(pCtx,pArray);` |
|         9 | 5391 | `	return PH7_OK;` |
|        17 | 5392 | `}` |
|         - | 5393 | `/*` |
|         - | 5394 | ` * array array_diff_key(array $array1 ,array $array2,...)` |
|         - | 5395 | ` *  Computes the difference of arrays using keys for comparison.` |
|         - | 5396 | ` * Parameters` |
|         - | 5397 | ` *  $array1` |
|         - | 5398 | ` *    The array to compare from` |
|         - | 5399 | ` *  $array2` |
|         - | 5400 | ` *    An array to compare against` |
|         - | 5401 | ` *  $...` |
|         - | 5402 | ` *   More arrays to compare against` |
|         - | 5403 | ` * Return` |
|         - | 5404 | ` *  Returns an array containing all the entries from array1 whose keys are not present` |
|         - | 5405 | ` *  in any of the other arrays.` |
|         - | 5406 | ` * Note that NULL is returned on failure.` |
|         - | 5407 | ` */` |
|        14 | 5408 | `static int ph7_hashmap_diff_key(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|         4 | 5409 | `{` |
|         - | 5410 | `	ph7_hashmap_node *pEntry;` |
|         - | 5411 | `	ph7_hashmap *pSrc,*pMap;` |
|         - | 5412 | `	ph7_value *pArray;` |
|         - | 5413 | `	sxi32 rc;` |
|         - | 5414 | `	sxu32 n;` |
|         - | 5415 | `	int i;` |
|         - | 5416 | `	/* Validate arguments to mirror PHP behaviour. Previously invalid inputs` |
|         - | 5417 | `	 * would quietly return NULL which is inconsistent with other hashmap` |
|         - | 5418 | `	 * helpers. */` |
|        18 | 5419 | `	if( nArg < 1 ){` |
|         4 | 5420 | `		return PH7_VmThrowException(pCtx,` |
|         - | 5421 | `			"ArgumentCountError",` |
|         - | 5422 | `			"array_diff_key() expects at least 1 argument, %d given",` |
|         1 | 5423 | `			nArg` |
|         - | 5424 | `			);` |
|         - | 5425 | `	}` |
|        15 | 5426 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|         4 | 5427 | `		return PH7_VmThrowException(pCtx,` |
|         - | 5428 | `			"TypeError",` |
|         - | 5429 | `			"array_diff_key(): Argument #1 ($array) must be of type array, %s given",` |
|         1 | 5430 | `			ph7_type_name(apArg[0])` |
|         - | 5431 | `			);` |
|         - | 5432 | `	}` |
|        20 | 5433 | `	for(i = 1 ; i < nArg ; i++){` |
|        12 | 5434 | `		if( !ph7_value_is_array(apArg[i]) ){` |
|         4 | 5435 | `			return PH7_VmThrowException(pCtx,` |
|         - | 5436 | `				"TypeError",` |
|         - | 5437 | `				"array_diff_key(): Argument #%d must be of type array, %s given",` |
|         1 | 5438 | `				i + 1,` |
|         2 | 5439 | `				ph7_type_name(apArg[i])` |
|         - | 5440 | `				);` |
|         - | 5441 | `		}` |
|         5 | 5442 | `	}` |
|         9 | 5443 | `	if( nArg == 1 ){` |
|         - | 5444 | `		/* Return the first array since we cannot perform a diff */` |
|         3 | 5445 | `		ph7_result_value(pCtx,apArg[0]);` |
|         3 | 5446 | `		return PH7_OK;` |
|         - | 5447 | `	}` |
|         - | 5448 | `	/* Create a new array */` |
|         7 | 5449 | `	pArray = ph7_context_new_array(pCtx);` |
|         7 | 5450 | `	if( pArray == 0 ){` |
|       ! 0 | 5451 | `		ph7_result_null(pCtx);` |
|       ! 0 | 5452 | `		return PH7_OK;` |
|         - | 5453 | `	}` |
|         - | 5454 | `	/* Point to the internal representation of the main hashmap */` |
|         7 | 5455 | `	pSrc = (ph7_hashmap *)apArg[0]->x.pOther;` |
|         - | 5456 | `	/* Perfrom the diff */` |
|         7 | 5457 | `	pEntry = pSrc->pFirst;` |
|         7 | 5458 | `	n = pSrc->nEntry;` |
|        12 | 5459 | `	for(;;){` |
|        25 | 5460 | `		if( n < 1 ){` |
|         7 | 5461 | `			break;` |
|         - | 5462 | `		}` |
|        31 | 5463 | `		for( i = 1 ; i < nArg ; i++ ){` |
|        23 | 5464 | `			if( !ph7_value_is_array(apArg[i])) {` |
|         - | 5465 | `				/* ignore */` |
|       ! 0 | 5466 | `				continue;` |
|         - | 5467 | `			}` |
|        23 | 5468 | `			pMap = (ph7_hashmap *)apArg[i]->x.pOther;` |
|        23 | 5469 | `			if( pEntry->iType == HASHMAP_BLOB_NODE ){` |
|        17 | 5470 | `				SyBlob *pKey = &pEntry->xKey.sKey;` |
|         - | 5471 | `				/* Blob lookup */` |
|        17 | 5472 | `				rc = HashmapLookupBlobKey(pMap,SyBlobData(pKey),SyBlobLength(pKey),0);` |
|         9 | 5473 | `			}else{` |
|         - | 5474 | `				/* Int lookup */` |
|         7 | 5475 | `				rc = HashmapLookupIntKey(pMap,pEntry->xKey.iKey,0);` |
|         - | 5476 | `			}` |
|        23 | 5477 | `			if( rc == SXRET_OK ){` |
|         - | 5478 | `				/* Key exists,break immediately */` |
|        11 | 5479 | `				break;` |
|         - | 5480 | `			}` |
|         7 | 5481 | `		}` |
|        19 | 5482 | `		if( i >= nArg ){` |
|         - | 5483 | `			/* Perform the insertion */` |
|         9 | 5484 | `			HashmapInsertNode((ph7_hashmap *)pArray->x.pOther,pEntry,TRUE);` |
|         4 | 5485 | `		}` |
|         - | 5486 | `		/* Point to the next entry */` |
|        19 | 5487 | `		pEntry = pEntry->pPrev; /* Reverse link */` |
|        19 | 5488 | `		n--;` |
|         1 | 5489 | `	}` |
|         - | 5490 | `	/* Return the freshly created array */` |
|         7 | 5491 | `	ph7_result_value(pCtx,pArray);` |
|         7 | 5492 | `	return PH7_OK;` |
|        11 | 5493 | `}` |
|         - | 5494 | `/*` |
|         - | 5495 | ` * array array_intersect(array $array1 ,array $array2,...)` |
|         - | 5496 | ` *  Computes the intersection of arrays.` |
|         - | 5497 | ` * Parameters` |
|         - | 5498 | ` *  $array1` |
|         - | 5499 | ` *    The array to compare from` |
|         - | 5500 | ` *  $array2` |
|         - | 5501 | ` *    An array to compare against` |
|         - | 5502 | ` *  $...` |
|         - | 5503 | ` *   More arrays to compare against` |
|         - | 5504 | ` * Return` |
|         - | 5505 | ` *  Returns an array containing all of the values in array1 whose values exist` |
|         - | 5506 | ` *  in all of the parameters.` |
|         - | 5507 | ` * Throws ArgumentCountError if no arguments are given.` |
|         - | 5508 | ` * Throws TypeError if any argument is not an array.` |
|         - | 5509 | ` */` |
|        22 | 5510 | `static int ph7_hashmap_intersect(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|         4 | 5511 | `{` |
|         - | 5512 | `	ph7_hashmap_node *pEntry;` |
|         - | 5513 | `	ph7_hashmap *pSrc,*pMap;` |
|         - | 5514 | `	ph7_value *pArray;` |
|         - | 5515 | `	ph7_value *pVal;` |
|         - | 5516 | `	sxi32 rc;` |
|         - | 5517 | `	sxu32 n;` |
|         - | 5518 | `	int i;` |
|        26 | 5519 | `	if( nArg < 1 ){` |
|         4 | 5520 | `		return PH7_VmThrowException(pCtx,` |
|         - | 5521 | `			"ArgumentCountError",` |
|         - | 5522 | `			"array_intersect() expects at least 1 argument, %d given",` |
|         1 | 5523 | `			nArg` |
|         - | 5524 | `			);` |
|         - | 5525 | `	}` |
|        23 | 5526 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|         4 | 5527 | `		return PH7_VmThrowException(pCtx,` |
|         - | 5528 | `			"TypeError",` |
|         - | 5529 | `			"array_intersect(): Argument #1 ($array) must be of type array, %s given",` |
|         1 | 5530 | `			ph7_type_name(apArg[0])` |
|         - | 5531 | `			);` |
|         - | 5532 | `	}` |
|        36 | 5533 | `	for( i = 1 ; i < nArg ; i++ ){` |
|        20 | 5534 | `		if( !ph7_value_is_array(apArg[i]) ){` |
|         4 | 5535 | `			return PH7_VmThrowException(pCtx,` |
|         - | 5536 | `				"TypeError",` |
|         - | 5537 | `				"array_intersect(): Argument #%d must be of type array, %s given",` |
|         1 | 5538 | `				i + 1,` |
|         2 | 5539 | `				ph7_type_name(apArg[i])` |
|         - | 5540 | `				);` |
|         - | 5541 | `		}` |
|         9 | 5542 | `	}` |
|        17 | 5543 | `	if( nArg == 1 ){` |
|         - | 5544 | `		/* Return the first array since we cannot perform a diff */` |
|         3 | 5545 | `		ph7_result_value(pCtx,apArg[0]);` |
|         3 | 5546 | `		return PH7_OK;` |
|         - | 5547 | `	}` |
|         - | 5548 | `	/* Create a new array */` |
|        15 | 5549 | `	pArray = ph7_context_new_array(pCtx);` |
|        15 | 5550 | `	if( pArray == 0 ){` |
|       ! 0 | 5551 | `		ph7_result_null(pCtx);` |
|       ! 0 | 5552 | `		return PH7_OK;` |
|         - | 5553 | `	}` |
|         - | 5554 | `	/* Point to the internal representation of the source hashmap */` |
|        15 | 5555 | `	pSrc = (ph7_hashmap *)apArg[0]->x.pOther;` |
|         - | 5556 | `	/* Perform the intersection */` |
|        15 | 5557 | `	pEntry = pSrc->pFirst;` |
|        15 | 5558 | `	n = pSrc->nEntry;` |
|        31 | 5559 | `	for(;;){` |
|        63 | 5560 | `		if( n < 1 ){` |
|        15 | 5561 | `			break;` |
|         - | 5562 | `		}` |
|         - | 5563 | `		/* Extract the node value */` |
|        49 | 5564 | `		pVal = HashmapExtractNodeValue(pEntry);` |
|        49 | 5565 | `		if( pVal ){` |
|        79 | 5566 | `			for( i = 1 ; i < nArg ; i++ ){` |
|         - | 5567 | `				/* Point to the internal representation of the hashmap */` |
|        55 | 5568 | `				pMap = (ph7_hashmap *)apArg[i]->x.pOther;` |
|         - | 5569 | `				/* Perform the lookup */` |
|        55 | 5570 | `				rc = HashmapFindValue(pMap,pVal,0,TRUE);` |
|        55 | 5571 | `				if( rc != SXRET_OK ){` |
|         - | 5572 | `					/* Value does not exist */` |
|        25 | 5573 | `					break;` |
|         - | 5574 | `				}` |
|        16 | 5575 | `			}` |
|        49 | 5576 | `			if( i >= nArg ){` |
|         - | 5577 | `				/* Perform the insertion */` |
|        25 | 5578 | `				HashmapInsertNode((ph7_hashmap *)pArray->x.pOther,pEntry,TRUE);` |
|        12 | 5579 | `			}` |
|        24 | 5580 | `		}` |
|         - | 5581 | `		/* Point to the next entry */` |
|        49 | 5582 | `		pEntry = pEntry->pPrev; /* Reverse link */` |
|        49 | 5583 | `		n--;` |
|         1 | 5584 | `	}` |
|         - | 5585 | `	/* Return the freshly created array */` |
|        15 | 5586 | `	ph7_result_value(pCtx,pArray);` |
|        15 | 5587 | `	return PH7_OK;` |
|        15 | 5588 | `}` |
|         - | 5589 | `/*` |
|         - | 5590 | ` * array array_intersect_assoc(array $array1 ,array $array2,...)` |
|         - | 5591 | ` *  Computes the intersection of arrays with additional index check.` |
|         - | 5592 | ` * Parameters` |
|         - | 5593 | ` *  $array1` |
|         - | 5594 | ` *    The array to compare from` |
|         - | 5595 | ` *  $array2` |
|         - | 5596 | ` *    An array to compare against` |
|         - | 5597 | ` *  $...` |
|         - | 5598 | ` *   More arrays to compare against` |
|         - | 5599 | ` * Return` |
|         - | 5600 | ` *  Returns an array containing all the values of array1 that are present` |
|         - | 5601 | ` *  in all the arguments, with matching keys.` |
|         - | 5602 | ` */` |
|        22 | 5603 | `static int ph7_hashmap_intersect_assoc(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|         4 | 5604 | `{` |
|         - | 5605 | `	ph7_hashmap_node *pEntry,*pN1,*pN2;` |
|         - | 5606 | `	ph7_hashmap *pSrc,*pMap;` |
|         - | 5607 | `	ph7_value *pArray;` |
|         - | 5608 | `	ph7_value *pVal;` |
|         - | 5609 | `	sxi32 rc;` |
|         - | 5610 | `	sxu32 n;` |
|         - | 5611 | `	int i;` |
|        26 | 5612 | `	if( nArg < 1 ){` |
|         4 | 5613 | `		return PH7_VmThrowException(pCtx,` |
|         - | 5614 | `			"ArgumentCountError",` |
|         - | 5615 | `			"array_intersect_assoc() expects at least 1 argument, %d given",` |
|         1 | 5616 | `			nArg` |
|         - | 5617 | `			);` |
|         - | 5618 | `	}` |
|        23 | 5619 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|         4 | 5620 | `		return PH7_VmThrowException(pCtx,` |
|         - | 5621 | `			"TypeError",` |
|         - | 5622 | `			"array_intersect_assoc(): Argument #1 ($array) must be of type array, %s given",` |
|         1 | 5623 | `			ph7_type_name(apArg[0])` |
|         - | 5624 | `			);` |
|         - | 5625 | `	}` |
|        36 | 5626 | `	for( i = 1 ; i < nArg ; i++ ){` |
|        20 | 5627 | `		if( !ph7_value_is_array(apArg[i]) ){` |
|         4 | 5628 | `			return PH7_VmThrowException(pCtx,` |
|         - | 5629 | `				"TypeError",` |
|         - | 5630 | `				"array_intersect_assoc(): Argument #%d must be of type array, %s given",` |
|         1 | 5631 | `				i + 1,` |
|         2 | 5632 | `				ph7_type_name(apArg[i])` |
|         - | 5633 | `				);` |
|         - | 5634 | `		}` |
|         9 | 5635 | `	}` |
|        17 | 5636 | `	if( nArg == 1 ){` |
|         - | 5637 | `		/* Return the first array since we cannot perform an intersection */` |
|         3 | 5638 | `		ph7_result_value(pCtx,apArg[0]);` |
|         3 | 5639 | `		return PH7_OK;` |
|         - | 5640 | `	}` |
|         - | 5641 | `	/* Create a new array */` |
|        15 | 5642 | `	pArray = ph7_context_new_array(pCtx);` |
|        15 | 5643 | `	if( pArray == 0 ){` |
|       ! 0 | 5644 | `		ph7_result_null(pCtx);` |
|       ! 0 | 5645 | `		return PH7_OK;` |
|         - | 5646 | `	}` |
|         - | 5647 | `	/* Point to the internal representation of the source hashmap */` |
|        15 | 5648 | `	pSrc = (ph7_hashmap *)apArg[0]->x.pOther;` |
|         - | 5649 | `	/* Perform the intersection */` |
|        15 | 5650 | `	pEntry = pSrc->pFirst;` |
|        15 | 5651 | `	n = pSrc->nEntry;` |
|        15 | 5652 | `	pN1 = pN2 = 0; /* cc warning */` |
|        23 | 5653 | `	for(;;){` |
|        47 | 5654 | `		if( n < 1 ){` |
|        15 | 5655 | `			break;` |
|         - | 5656 | `		}` |
|         - | 5657 | `		/* Extract the node value */` |
|        33 | 5658 | `		pVal = HashmapExtractNodeValue(pEntry);` |
|        33 | 5659 | `		if( pVal ){` |
|        53 | 5660 | `			for( i = 1 ; i < nArg ; i++ ){` |
|         - | 5661 | `				/* Point to the internal representation of the hashmap */` |
|        37 | 5662 | `				pMap = (ph7_hashmap *)apArg[i]->x.pOther;` |
|         - | 5663 | `				/* Perform a key lookup first */` |
|        37 | 5664 | `				if( pEntry->iType == HASHMAP_INT_NODE ){` |
|        15 | 5665 | `					rc = HashmapLookupIntKey(pMap,pEntry->xKey.iKey,&pN1);` |
|         8 | 5666 | `				}else{` |
|        23 | 5667 | `					rc = HashmapLookupBlobKey(pMap,SyBlobData(&pEntry->xKey.sKey),SyBlobLength(&pEntry->xKey.sKey),&pN1);` |
|         - | 5668 | `				}` |
|        37 | 5669 | `				if( rc != SXRET_OK ){` |
|         - | 5670 | `					/* No such key,break immediately */` |
|         7 | 5671 | `					break;` |
|         - | 5672 | `				}` |
|         - | 5673 | `				/* Perform the lookup */` |
|        31 | 5674 | `				rc = HashmapFindValue(pMap,pVal,&pN2,TRUE);` |
|        31 | 5675 | `				if( rc != SXRET_OK \|\| pN1 != pN2 ){` |
|         - | 5676 | `					/* Value does not exist */` |
|         6 | 5677 | `					break;` |
|         - | 5678 | `				}` |
|        11 | 5679 | `			}` |
|        33 | 5680 | `			if( i >= nArg ){` |
|         - | 5681 | `				/* Perform the insertion */` |
|        17 | 5682 | `				HashmapInsertNode((ph7_hashmap *)pArray->x.pOther,pEntry,TRUE);` |
|         8 | 5683 | `			}` |
|        16 | 5684 | `		}` |
|         - | 5685 | `		/* Point to the next entry */` |
|        33 | 5686 | `		pEntry = pEntry->pPrev; /* Reverse link */` |
|        33 | 5687 | `		n--;` |
|         1 | 5688 | `	}` |
|         - | 5689 | `	/* Return the freshly created array */` |
|        15 | 5690 | `	ph7_result_value(pCtx,pArray);` |
|        15 | 5691 | `	return PH7_OK;` |
|        15 | 5692 | `}` |
|         - | 5693 | `/*` |
|         - | 5694 | ` * array array_intersect_key(array $array1 ,...)` |
|         - | 5695 | ` *  Computes the intersection of arrays using keys for comparison.` |
|         - | 5696 | ` * Parameters` |
|         - | 5697 | ` *  $array1` |
|         - | 5698 | ` *    The array to compare from` |
|         - | 5699 | ` *  $...` |
|         - | 5700 | ` *   More arrays to compare against` |
|         - | 5701 | ` * Return` |
|         - | 5702 | ` *  Returns an associative array containing all the entries of array1 which` |
|         - | 5703 | ` *  have keys that are present in all arguments.` |
|         - | 5704 | ` * Note that NULL is returned on failure.` |
|         - | 5705 | ` */` |
|        22 | 5706 | `static int ph7_hashmap_intersect_key(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|         4 | 5707 | `{` |
|         - | 5708 | `	ph7_hashmap_node *pEntry;` |
|         - | 5709 | `	ph7_hashmap *pSrc,*pMap;` |
|         - | 5710 | `	ph7_value *pArray;` |
|         - | 5711 | `	sxi32 rc;` |
|         - | 5712 | `	sxu32 n;` |
|         - | 5713 | `	int i;` |
|        26 | 5714 | `	if( nArg < 1 ){` |
|         4 | 5715 | `		return PH7_VmThrowException(pCtx,` |
|         - | 5716 | `			"ArgumentCountError",` |
|         - | 5717 | `			"array_intersect_key() expects at least 1 argument, %d given",` |
|         1 | 5718 | `			nArg` |
|         - | 5719 | `			);` |
|         - | 5720 | `	}` |
|        23 | 5721 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|         4 | 5722 | `		return PH7_VmThrowException(pCtx,` |
|         - | 5723 | `			"TypeError",` |
|         - | 5724 | `			"array_intersect_key(): Argument #1 ($array) must be of type array, %s given",` |
|         1 | 5725 | `			ph7_type_name(apArg[0])` |
|         - | 5726 | `			);` |
|         - | 5727 | `	}` |
|        36 | 5728 | `	for( i = 1 ; i < nArg ; i++ ){` |
|        20 | 5729 | `		if( !ph7_value_is_array(apArg[i]) ){` |
|         4 | 5730 | `			return PH7_VmThrowException(pCtx,` |
|         - | 5731 | `				"TypeError",` |
|         - | 5732 | `				"array_intersect_key(): Argument #%d must be of type array, %s given",` |
|         1 | 5733 | `				i + 1,` |
|         2 | 5734 | `				ph7_type_name(apArg[i])` |
|         - | 5735 | `				);` |
|         - | 5736 | `		}` |
|         9 | 5737 | `	}` |
|        17 | 5738 | `	if( nArg == 1 ){` |
|         - | 5739 | `		/* Return the first array since we cannot perform an intersection */` |
|         3 | 5740 | `		ph7_result_value(pCtx,apArg[0]);` |
|         3 | 5741 | `		return PH7_OK;` |
|         - | 5742 | `	}` |
|         - | 5743 | `	/* Create a new array */` |
|        15 | 5744 | `	pArray = ph7_context_new_array(pCtx);` |
|        15 | 5745 | `	if( pArray == 0 ){` |
|       ! 0 | 5746 | `		ph7_result_null(pCtx);` |
|       ! 0 | 5747 | `		return PH7_OK;` |
|         - | 5748 | `	}` |
|         - | 5749 | `	/* Point to the internal representation of the main hashmap */` |
|        15 | 5750 | `	pSrc = (ph7_hashmap *)apArg[0]->x.pOther;` |
|         - | 5751 | `	/* Perform the intersection */` |
|        15 | 5752 | `	pEntry = pSrc->pFirst;` |
|        15 | 5753 | `	n = pSrc->nEntry;` |
|        24 | 5754 | `	for(;;){` |
|        49 | 5755 | `		if( n < 1 ){` |
|        15 | 5756 | `			break;` |
|         - | 5757 | `		}` |
|        57 | 5758 | `		for( i = 1 ; i < nArg ; i++ ){` |
|        39 | 5759 | `			pMap = (ph7_hashmap *)apArg[i]->x.pOther;` |
|        39 | 5760 | `			if( pEntry->iType == HASHMAP_BLOB_NODE ){` |
|        27 | 5761 | `				SyBlob *pKey = &pEntry->xKey.sKey;` |
|         - | 5762 | `				/* Blob lookup */` |
|        27 | 5763 | `				rc = HashmapLookupBlobKey(pMap,SyBlobData(pKey),SyBlobLength(pKey),0);` |
|        14 | 5764 | `			}else{` |
|         - | 5765 | `				/* Int key */` |
|        13 | 5766 | `				rc = HashmapLookupIntKey(pMap,pEntry->xKey.iKey,0);` |
|         - | 5767 | `			}` |
|        39 | 5768 | `			if( rc != SXRET_OK ){` |
|         - | 5769 | `				/* Key does not exist, break immediately */` |
|        17 | 5770 | `				break;` |
|         - | 5771 | `			}` |
|        12 | 5772 | `		}` |
|        35 | 5773 | `		if( i >= nArg ){` |
|         - | 5774 | `			/* Perform the insertion */` |
|        19 | 5775 | `			HashmapInsertNode((ph7_hashmap *)pArray->x.pOther,pEntry,TRUE);` |
|         9 | 5776 | `		}` |
|         - | 5777 | `		/* Point to the next entry */` |
|        35 | 5778 | `		pEntry = pEntry->pPrev; /* Reverse link */` |
|        35 | 5779 | `		n--;` |
|         1 | 5780 | `	}` |
|         - | 5781 | `	/* Return the freshly created array */` |
|        15 | 5782 | `	ph7_result_value(pCtx,pArray);` |
|        15 | 5783 | `	return PH7_OK;` |
|        15 | 5784 | `}` |
|         - | 5785 | `/*` |
|         - | 5786 | ` * array array_uintersect(array $array1 ,array $array2,...,$callback)` |
|         - | 5787 | ` *  Computes the intersection of arrays.` |
|         - | 5788 | ` * Parameters` |
|         - | 5789 | ` *  $array1` |
|         - | 5790 | ` *    The array to compare from` |
|         - | 5791 | ` *  $array2` |
|         - | 5792 | ` *    An array to compare against` |
|         - | 5793 | ` *  $...` |
|         - | 5794 | ` *   More arrays to compare against` |
|         - | 5795 | ` * $callback` |
|         - | 5796 | ` *  The callback comparison function.` |
|         - | 5797 | ` *  The comparison function must return an integer less than, equal to, or greater than zero` |
|         - | 5798 | ` *  if the first argument is considered to be respectively less than, equal to, or greater` |
|         - | 5799 | ` *  than the second.` |
|         - | 5800 | ` *     int callback ( mixed $a, mixed $b )` |
|         - | 5801 | ` * Return` |
|         - | 5802 | ` *  Returns an array containing all of the values in array1 whose values exist` |
|         - | 5803 | ` *  in all of the parameters. .` |
|         - | 5804 | ` * Note that NULL is returned on failure.` |
|         - | 5805 | ` */` |
|        26 | 5806 | `static int ph7_hashmap_uintersect(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|         5 | 5807 | `{` |
|         - | 5808 | `	ph7_hashmap_node *pEntry;` |
|         - | 5809 | `	ph7_hashmap *pSrc,*pMap;` |
|         - | 5810 | `	ph7_value *pCallback;` |
|         - | 5811 | `	ph7_value *pArray;` |
|         - | 5812 | `	ph7_value *pVal;` |
|         - | 5813 | `	sxi32 rc;` |
|         - | 5814 | `	sxu32 n;` |
|         - | 5815 | `	int i;` |
|         - | 5816 |  |
|         - | 5817 | `	/* Ensure the argument count matches PHP behaviour. */` |
|        31 | 5818 | `	if( nArg < 2 ){` |
|         4 | 5819 | `		return PH7_VmThrowException(pCtx,` |
|         - | 5820 | `			"ArgumentCountError",` |
|         - | 5821 | `			"array_uintersect() expects at least 2 arguments, %d given",` |
|         1 | 5822 | `			nArg` |
|         - | 5823 | `			);` |
|         - | 5824 | `	}` |
|        29 | 5825 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|         4 | 5826 | `		return PH7_VmThrowException(pCtx,` |
|         - | 5827 | `			"TypeError",` |
|         - | 5828 | `			"array_uintersect(): Argument #1 ($array) must be of type array, %s given",` |
|         1 | 5829 | `			ph7_type_name(apArg[0])` |
|         - | 5830 | `			);` |
|         - | 5831 | `	}` |
|         - | 5832 |  |
|        27 | 5833 | `	if( nArg == 2 ){` |
|         - | 5834 | `		/* Only the original array and the callback were provided. */` |
|         - | 5835 | `		/* Validate the callback below in order to match PHP's parameter` |
|         - | 5836 | `		 * validation ordering. */` |
|         3 | 5837 | `	} else {` |
|         - | 5838 | `		/* Ensure intermediary arguments are arrays (matches PHP strict typing). */` |
|        39 | 5839 | `		for( i = 1 ; i < nArg - 1; i++ ){` |
|        23 | 5840 | `			if( !ph7_value_is_array(apArg[i]) ){` |
|         4 | 5841 | `				return PH7_VmThrowException(pCtx,` |
|         - | 5842 | `					"TypeError",` |
|         - | 5843 | `					"array_uintersect(): Argument #%d must be of type array, %s given",` |
|         1 | 5844 | `					i + 1,` |
|         2 | 5845 | `					ph7_type_name(apArg[i])` |
|         - | 5846 | `					);` |
|         - | 5847 | `			}` |
|        13 | 5848 | `		}` |
|         - | 5849 | `	}` |
|         - | 5850 |  |
|         - | 5851 | `	/* Identify the callback (always expected as the last argument). */` |
|        25 | 5852 | `	pCallback = apArg[nArg - 1];` |
|         - | 5853 | `	/* Validate the callback to match PHP's error messages. */` |
|        25 | 5854 | `	if( !ph7_value_is_callable(pCallback) ){` |
|        14 | 5855 | `		if( ph7_value_is_array(pCallback) ){` |
|         - | 5856 | `			/* PHP emits a special message when the array length is wrong.` |
|         - | 5857 | `			 * If the array has two elements but is still not callable (e.g. missing` |
|         - | 5858 | `			 * method / missing class), we must emit a more general error instead.` |
|         - | 5859 | `			 */` |
|         9 | 5860 | `			ph7_hashmap *pCb = (ph7_hashmap *)pCallback->x.pOther;` |
|         9 | 5861 | `			if( pCb->nEntry != 2 ){` |
|         4 | 5862 | `				return PH7_VmThrowException(pCtx,` |
|         - | 5863 | `					"TypeError",` |
|         - | 5864 | `					"array_uintersect(): Argument #%d must be a valid callback, array callback must have exactly two members",` |
|         1 | 5865 | `					nArg` |
|         - | 5866 | `					);` |
|         - | 5867 | `			}` |
|         - | 5868 | `			/* Try to provide a more precise error like PHP does for missing classes/methods. */` |
|         - | 5869 | `			{` |
|         6 | 5870 | `				ph7_value *pKey = (ph7_value *)SySetAt(&pCtx->pVm->aMemObj,pCb->pFirst->nValIdx);` |
|         6 | 5871 | `				ph7_value *pMethod = (ph7_value *)SySetAt(&pCtx->pVm->aMemObj,pCb->pFirst->pPrev->nValIdx);` |
|         6 | 5872 | `				if( pKey && pMethod && (pMethod->iFlags & MEMOBJ_STRING) ){` |
|         - | 5873 | `					int nMethodLen;` |
|         6 | 5874 | `					const char *zMethod = ph7_value_to_string(pMethod,&nMethodLen);` |
|         6 | 5875 | `					ph7_class *pClass = PH7_VmExtractClassFromValue(pCtx->pVm,pKey);` |
|         6 | 5876 | `					if( pClass ){` |
|         - | 5877 | `						/* Class exists but method is missing. */` |
|         4 | 5878 | `						return PH7_VmThrowException(pCtx,` |
|         - | 5879 | `							"TypeError",` |
|         - | 5880 | `							"array_uintersect(): Argument #%d must be a valid callback, class %s does not have a method \"%s\"",` |
|         1 | 5881 | `							nArg,` |
|         1 | 5882 | `							(const char *)SyStringData(&pClass->sName),` |
|         1 | 5883 | `							zMethod` |
|         - | 5884 | `							);` |
|         - | 5885 | `					}` |
|         - | 5886 | `					/* Class not found */` |
|         - | 5887 | `					{` |
|         - | 5888 | `						int nName;` |
|         3 | 5889 | `						const char *zName = ph7_value_to_string(pKey,&nName);` |
|         4 | 5890 | `						return PH7_VmThrowException(pCtx,` |
|         - | 5891 | `							"TypeError",` |
|         - | 5892 | `							"array_uintersect(): Argument #%d must be a valid callback, class \"%s\" not found",` |
|         1 | 5893 | `							nArg,` |
|         1 | 5894 | `							zName` |
|         - | 5895 | `							);` |
|         - | 5896 | `					}` |
|         - | 5897 | `				}` |
|         - | 5898 | `			}` |
|         - | 5899 | `			/* Fallback message */` |
|       ! 0 | 5900 | `			return PH7_VmThrowException(pCtx,` |
|         - | 5901 | `				"TypeError",` |
|         - | 5902 | `				"array_uintersect(): Argument #%d must be a valid callback, no array or string given",` |
|       ! 0 | 5903 | `				nArg` |
|         - | 5904 | `				);` |
|         - | 5905 | `		}` |
|         6 | 5906 | `		if( ph7_value_is_string(pCallback) ){` |
|         - | 5907 | `			int len;` |
|         3 | 5908 | `			const char *zName = ph7_value_to_string(pCallback, &len);` |
|         4 | 5909 | `			return PH7_VmThrowException(pCtx,` |
|         - | 5910 | `				"TypeError",` |
|         - | 5911 | `				"array_uintersect(): Argument #%d must be a valid callback, function \"%s\" not found or invalid function name",` |
|         1 | 5912 | `				nArg,` |
|         1 | 5913 | `				zName` |
|         - | 5914 | `				);` |
|         - | 5915 | `		}` |
|         4 | 5916 | `		return PH7_VmThrowException(pCtx,` |
|         - | 5917 | `			"TypeError",` |
|         - | 5918 | `			"array_uintersect(): Argument #%d must be a valid callback, no array or string given",` |
|         1 | 5919 | `			nArg` |
|         - | 5920 | `			);` |
|         - | 5921 | `	}` |
|         - | 5922 |  |
|        11 | 5923 | `	if( nArg == 2 ){` |
|         - | 5924 | `		/* Only the original array and the callback were provided. */` |
|         5 | 5925 | `		ph7_result_value(pCtx,apArg[0]);` |
|         5 | 5926 | `		return PH7_OK;` |
|         - | 5927 | `	}` |
|         - | 5928 |  |
|         - | 5929 | `	/* Create a new array */` |
|         7 | 5930 | `	pArray = ph7_context_new_array(pCtx);` |
|         7 | 5931 | `	if( pArray == 0 ){` |
|       ! 0 | 5932 | `		ph7_result_null(pCtx);` |
|       ! 0 | 5933 | `		return PH7_OK;` |
|         - | 5934 | `	}` |
|         - | 5935 | `	/* Point to the internal representation of the source hashmap */` |
|         7 | 5936 | `	pSrc = (ph7_hashmap *)apArg[0]->x.pOther;` |
|         - | 5937 | `	/* Perform the intersection */` |
|         7 | 5938 | `	pEntry = pSrc->pFirst;` |
|         7 | 5939 | `	n = pSrc->nEntry;` |
|         7 | 5940 | `	pCtx->pVm->iCmpCallbackExc = 0;` |
|         9 | 5941 | `	for(;;){` |
|        19 | 5942 | `		if( n < 1 ){` |
|         5 | 5943 | `			break;` |
|         - | 5944 | `		}` |
|         - | 5945 | `		/* Extract the node value */` |
|        15 | 5946 | `		pVal = HashmapExtractNodeValue(pEntry);` |
|        15 | 5947 | `		if( pVal ){` |
|        23 | 5948 | `			for( i = 1 ; i < nArg - 1; i++ ){` |
|        15 | 5949 | `				if( !ph7_value_is_array(apArg[i])) {` |
|         - | 5950 | `					/* ignore */` |
|       ! 0 | 5951 | `					continue;` |
|         - | 5952 | `				}` |
|         - | 5953 | `				/* Point to the internal representation of the hashmap */` |
|        15 | 5954 | `				pMap = (ph7_hashmap *)apArg[i]->x.pOther;` |
|         - | 5955 | `				/* Perform the lookup */` |
|        15 | 5956 | `				rc = HashmapFindValueByCallback(pMap,pVal,pCallback,0);` |
|        15 | 5957 | `				if( rc != SXRET_OK ){` |
|         - | 5958 | `					/* Value does not exist */` |
|         7 | 5959 | `					break;` |
|         - | 5960 | `				}` |
|         5 | 5961 | `			}` |
|        15 | 5962 | `			if( i >= (nArg-1) ){` |
|         - | 5963 | `				/* Perform the insertion */` |
|         9 | 5964 | `				HashmapInsertNode((ph7_hashmap *)pArray->x.pOther,pEntry,TRUE);` |
|         4 | 5965 | `			}` |
|         7 | 5966 | `		}` |
|        15 | 5967 | `		if( pCtx->pVm->iCmpCallbackExc ){` |
|         - | 5968 | `			/* The comparison callback raised: propagate so the dispatcher unwinds. */` |
|         3 | 5969 | `			pCtx->pVm->iCmpCallbackExc = 0;` |
|         3 | 5970 | `			return PH7_EXCEPTION;` |
|         - | 5971 | `		}` |
|         - | 5972 | `		/* Point to the next entry */` |
|        13 | 5973 | `		pEntry = pEntry->pPrev; /* Reverse link */` |
|        13 | 5974 | `		n--;` |
|         1 | 5975 | `	}` |
|         - | 5976 | `	/* Return the freshly created array */` |
|         5 | 5977 | `	ph7_result_value(pCtx,pArray);` |
|         5 | 5978 | `	return PH7_OK;` |
|        18 | 5979 | `}` |
|         - | 5980 | `/*` |
|         - | 5981 | ` * array array_fill(int $start_index,int $num,var $value)` |
|         - | 5982 | ` *  Fill an array with values.` |
|         - | 5983 | ` * Parameters` |
|         - | 5984 | ` *  $start_index` |
|         - | 5985 | ` *    The first index of the returned array.` |
|         - | 5986 | ` *  $num` |
|         - | 5987 | ` *   Number of elements to insert.` |
|         - | 5988 | ` *  $value` |
|         - | 5989 | ` *    Value to use for filling.` |
|         - | 5990 | ` * Return` |
|         - | 5991 | ` *  The filled array or null on failure.` |
|         - | 5992 | ` */` |
|       244 | 5993 | `static int ph7_hashmap_fill(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|         5 | 5994 | `{` |
|         - | 5995 | `	ph7_value *pArray;` |
|         - | 5996 | `	int i,nEntry;` |
|         - | 5997 |  |
|         - | 5998 | `	/* PHP enforces argument count and type checks. */` |
|       249 | 5999 | `	if( nArg != 3 ){` |
|         - | 6000 | `		/* wrong number of arguments -> ArgumentCountError */` |
|         8 | 6001 | `		return PH7_VmThrowException(pCtx,` |
|         - | 6002 | `			"ArgumentCountError",` |
|         - | 6003 | `			"array_fill() expects exactly 3 arguments, %d given",` |
|         2 | 6004 | `			nArg` |
|         - | 6005 | `			);` |
|         - | 6006 | `	}` |
|         - | 6007 |  |
|         - | 6008 | `	/* Argument #1: start index must be convertible to int.  Accept booleans,` |
|         - | 6009 | `	 * floats, and numeric strings (including those with decimal point) by` |
|         - | 6010 | `	 * allowing them through the conversion.  Only arrays, objects, resources` |
|         - | 6011 | `	 * and NULLs are rejected outright. */` |
|       359 | 6012 | `	if( ph7_value_is_array(apArg[0]) \|\| ph7_value_is_object(apArg[0]) \|\|` |
|       362 | 6013 | `		ph7_value_is_resource(apArg[0]) \|\| ph7_value_is_null(apArg[0]) ){` |
|         4 | 6014 | `		return PH7_VmThrowException(pCtx,` |
|         - | 6015 | `			"TypeError",` |
|         - | 6016 | `			"array_fill(): Argument #1 ($start_index) must be of type int, %s given",` |
|         1 | 6017 | `			ph7_type_name(apArg[0])` |
|         - | 6018 | `			);` |
|         - | 6019 | `	}` |
|       242 | 6020 | `	if( ph7_value_is_string(apArg[0]) ){` |
|         - | 6021 | `		int len;` |
|         8 | 6022 | `		sxu8 bReal = FALSE;` |
|         8 | 6023 | `		const char *zStr = ph7_value_to_string(apArg[0], &len);` |
|         8 | 6024 | `		if( SyStrIsNumeric(zStr, len, &bReal, 0) != SXRET_OK ){` |
|         - | 6025 | `			/* Non‑numeric string is an error. */` |
|         3 | 6026 | `			return PH7_VmThrowException(pCtx,` |
|         - | 6027 | `				"TypeError",` |
|         - | 6028 | `				"array_fill(): Argument #1 ($start_index) must be of type int, string given"` |
|         - | 6029 | `				);` |
|         - | 6030 | `		}` |
|         5 | 6031 | `		if( bReal ){` |
|         - | 6032 | `			/* float-string -> deprecation warning */` |
|         4 | 6033 | `			ph7_context_throw_error_format(pCtx, E_DEPRECATED,` |
|         - | 6034 | `				"Implicit conversion from float-string \"%s\" to int loses precision",` |
|         1 | 6035 | `				zStr` |
|         - | 6036 | `				);` |
|         1 | 6037 | `		}` |
|         2 | 6038 | `	}` |
|         - | 6039 |  |
|         - | 6040 | `	/* Argument #2: count must be convertible to non-negative int.  Allow booleans,` |
|         - | 6041 | `	 * floats and numeric strings; reject arrays, objects, resources and NULL. */` |
|       354 | 6042 | `	if( ph7_value_is_array(apArg[1]) \|\| ph7_value_is_object(apArg[1]) \|\|` |
|       357 | 6043 | `		ph7_value_is_resource(apArg[1]) \|\| ph7_value_is_null(apArg[1]) ){` |
|       ! 0 | 6044 | `		return PH7_VmThrowException(pCtx,` |
|         - | 6045 | `			"TypeError",` |
|         - | 6046 | `			"array_fill(): Argument #2 ($count) must be of type int, %s given",` |
|       ! 0 | 6047 | `			ph7_type_name(apArg[1])` |
|         - | 6048 | `			);` |
|         - | 6049 | `	}` |
|       239 | 6050 | `	if( ph7_value_is_string(apArg[1]) ){` |
|         - | 6051 | `		int len;` |
|         3 | 6052 | `		sxu8 bReal = FALSE;` |
|         3 | 6053 | `		const char *zStr = ph7_value_to_string(apArg[1], &len);` |
|         3 | 6054 | `		if( SyStrIsNumeric(zStr, len, &bReal, 0) != SXRET_OK ){` |
|         3 | 6055 | `			return PH7_VmThrowException(pCtx,` |
|         - | 6056 | `				"TypeError",` |
|         - | 6057 | `				"array_fill(): Argument #2 ($count) must be of type int, string given"` |
|         - | 6058 | `				);` |
|         - | 6059 | `		}` |
|       ! 0 | 6060 | `	}` |
|         - | 6061 | `	/* Note: booleans and floats (including fractional) are now accepted; they` |
|         - | 6062 | `	 * will be converted by ph7_value_to_int below. */` |
|       236 | 6063 | `	if( ph7_value_is_float(apArg[1]) ){` |
|         5 | 6064 | `		double d = ph7_value_to_double(apArg[1]);` |
|         - | 6065 | `		/* avoid hiding outer 'i' (loop index) */` |
|         5 | 6066 | `		sxi64 i64 = (sxi64)d;` |
|         5 | 6067 | `		if( d != (double)i64 ){` |
|         7 | 6068 | `			ph7_context_throw_error_format(pCtx, E_DEPRECATED,` |
|         - | 6069 | `				"Implicit conversion from float %g to int loses precision",` |
|         2 | 6070 | `				d` |
|         - | 6071 | `				);` |
|         2 | 6072 | `		}` |
|         2 | 6073 | `	}` |
|         - | 6074 |  |
|         - | 6075 | `	/* Total number of entries to insert */` |
|       236 | 6076 | `	nEntry = ph7_value_to_int(apArg[1]);` |
|         - | 6077 | `	/* Reject negative counts with a ValueError like PHP. */` |
|       236 | 6078 | `	if( nEntry < 0 ){` |
|         3 | 6079 | `		return PH7_VmThrowException(pCtx,` |
|         - | 6080 | `			"ValueError",` |
|         - | 6081 | `			"array_fill(): Argument #2 ($count) must be greater than or equal to 0"` |
|         - | 6082 | `			);` |
|         - | 6083 | `	}` |
|         - | 6084 |  |
|         - | 6085 | `	/* If zero elements were requested, return an empty array without allocating */` |
|       233 | 6086 | `	if( nEntry == 0 ){` |
|         7 | 6087 | `		ph7_result_value(pCtx, ph7_context_new_array(pCtx));` |
|         7 | 6088 | `		return PH7_OK;` |
|         - | 6089 | `	}` |
|         - | 6090 |  |
|         - | 6091 | `	/* Create a new array */` |
|       227 | 6092 | `	pArray = ph7_context_new_array(pCtx);` |
|       227 | 6093 | `	if( pArray == 0 ){` |
|       ! 0 | 6094 | `		return PH7_ContextMemoryError(pCtx);` |
|         - | 6095 | `	}` |
|         - | 6096 |  |
|         - | 6097 | `	/* PHP 8 fills consecutive integer keys start_index, start_index+1, … even` |
|         - | 6098 | `	 * when start_index is negative (PHP 7 restarted the remaining keys from 0,` |
|         - | 6099 | `	 * so array_fill(-5,3) gave -5,0,1 instead of -5,-4,-3). Assign each key` |
|         - | 6100 | `	 * explicitly rather than relying on automatic (append) indexing. */` |
|       227 | 6101 | `	int iStart = ph7_value_to_int(apArg[0]);` |
|   2117831 | 6102 | `	for( i = 0 ; i < nEntry ; i++ ){` |
|   2117605 | 6103 | `		if( ph7_array_add_intkey_elem(pArray, iStart + i, apArg[2]) != SXRET_OK ){` |
|         - | 6104 | `			/* Allocation failure: surface a fatal instead of a partial array */` |
|       ! 0 | 6105 | `			return PH7_ContextMemoryError(pCtx);` |
|         - | 6106 | `		}` |
|   1058803 | 6107 | `	}` |
|         - | 6108 | `	/* Return the filled array */` |
|       227 | 6109 | `	ph7_result_value(pCtx, pArray);` |
|       227 | 6110 | `	return PH7_OK;` |
|       127 | 6111 | `}` |
|         - | 6112 | `/*` |
|         - | 6113 | ` * array array_fill_keys(array $input,mixed $value)` |
|         - | 6114 | ` *  Fill an array with values, specifying keys.` |
|         - | 6115 | ` * Parameters` |
|         - | 6116 | ` *  $input` |
|         - | 6117 | ` *   Array of values that will be used as key.` |
|         - | 6118 | ` *  $value` |
|         - | 6119 | ` *    Value to use for filling.` |
|         - | 6120 | ` * Return` |
|         - | 6121 | ` *  The filled array.` |
|         - | 6122 | ` * Throws` |
|         - | 6123 | ` *  ValueError if $input is not an array.` |
|         - | 6124 | ` */` |
|        26 | 6125 | `static int ph7_hashmap_fill_keys(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|         5 | 6126 | `{` |
|         - | 6127 | `	ph7_hashmap_node *pEntry;` |
|         - | 6128 | `	ph7_hashmap *pSrc;` |
|         - | 6129 | `	ph7_value *pArray;` |
|         - | 6130 | `	sxu32 n;` |
|         - | 6131 | `	/* PHP enforces exactly 2 arguments. */` |
|        31 | 6132 | `	if( nArg != 2 ){` |
|        12 | 6133 | `		return PH7_VmThrowException(pCtx,` |
|         - | 6134 | `			"ArgumentCountError",` |
|         - | 6135 | `			"array_fill_keys() expects exactly 2 arguments, %d given",` |
|         3 | 6136 | `			nArg` |
|         - | 6137 | `			);` |
|         - | 6138 | `	}` |
|         - | 6139 | `	/* Make sure we are dealing with a valid hashmap */` |
|        23 | 6140 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|         8 | 6141 | `		return PH7_VmThrowException(pCtx,` |
|         - | 6142 | `			"TypeError",` |
|         - | 6143 | `			"array_fill_keys(): Argument #1 ($keys) must be of type array, %s given",` |
|         2 | 6144 | `			ph7_type_name(apArg[0])` |
|         - | 6145 | `			);` |
|         - | 6146 | `	}` |
|         - | 6147 | `	/* Point to the internal representation of the input hashmap */` |
|        17 | 6148 | `	pSrc = (ph7_hashmap *)apArg[0]->x.pOther;` |
|         - | 6149 | `	/* Create a new array */` |
|        17 | 6150 | `	pArray = ph7_context_new_array(pCtx);` |
|        17 | 6151 | `	if( pArray == 0 ){` |
|       ! 0 | 6152 | `		ph7_result_null(pCtx);` |
|       ! 0 | 6153 | `		return PH7_OK;` |
|         - | 6154 | `	}` |
|         - | 6155 | `	/* Perform the requested operation */` |
|        17 | 6156 | `	pEntry = pSrc->pFirst;` |
|        45 | 6157 | `	for( n = 0 ; n < pSrc->nEntry ; n++ ){` |
|        29 | 6158 | `		ph7_array_add_elem(pArray,HashmapExtractNodeValue(pEntry),apArg[1]);` |
|         - | 6159 | `		/* Point to the next entry */` |
|        29 | 6160 | `		pEntry = pEntry->pPrev; /* Reverse link */` |
|        15 | 6161 | `	}` |
|         - | 6162 | `	/* Return the filled array */` |
|        17 | 6163 | `	ph7_result_value(pCtx,pArray);` |
|        17 | 6164 | `	return PH7_OK;` |
|        18 | 6165 | `}` |
|         - | 6166 | `/*` |
|         - | 6167 | ` * array array_combine(array $keys,array $values)` |
|         - | 6168 | ` *  Creates an array by using one array for keys and another for its values.` |
|         - | 6169 | ` * Parameters` |
|         - | 6170 | ` *  $keys` |
|         - | 6171 | ` *    Array of keys to be used.` |
|         - | 6172 | ` * $values` |
|         - | 6173 | ` *   Array of values to be used.` |
|         - | 6174 | ` * Return` |
|         - | 6175 | ` *  Returns the combined array. Otherwise FALSE if the number of elements` |
|         - | 6176 | ` *  for each array isn't equal or if one of the given arguments is` |
|         - | 6177 | ` *  not an array.` |
|         - | 6178 | ` */` |
|        18 | 6179 | `static int ph7_hashmap_combine(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|         5 | 6180 | `{` |
|         - | 6181 | `	ph7_hashmap_node *pKe,*pVe;` |
|         - | 6182 | `	ph7_hashmap *pKey,*pValue;` |
|         - | 6183 | `	ph7_value *pArray;` |
|         - | 6184 | `	sxu32 n;` |
|         - | 6185 | `	/* PHP enforces argument count and type checks. */` |
|        23 | 6186 | `	if( nArg != 2 ){` |
|         - | 6187 | `		/* wrong number of arguments -> ArgumentCountError */` |
|         4 | 6188 | `		return PH7_VmThrowException(pCtx,` |
|         - | 6189 | `			"ArgumentCountError",` |
|         - | 6190 | `			"array_combine() expects exactly 2 arguments, %d given",` |
|         1 | 6191 | `			nArg` |
|         - | 6192 | `			);` |
|         - | 6193 | `	}` |
|         - | 6194 | `	/* Validate argument types individually so we can report the correct` |
|         - | 6195 | `	 * argument index in the error message. */` |
|        20 | 6196 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|         4 | 6197 | `		return PH7_VmThrowException(pCtx,` |
|         - | 6198 | `			"TypeError",` |
|         - | 6199 | `			"array_combine(): Argument #1 ($keys) must be of type array, %s given",` |
|         1 | 6200 | `			ph7_type_name(apArg[0])` |
|         - | 6201 | `			);` |
|         - | 6202 | `	}` |
|        17 | 6203 | `	if( !ph7_value_is_array(apArg[1]) ){` |
|         4 | 6204 | `		return PH7_VmThrowException(pCtx,` |
|         - | 6205 | `			"TypeError",` |
|         - | 6206 | `			"array_combine(): Argument #2 ($values) must be of type array, %s given",` |
|         2 | 6207 | `			ph7_type_name(apArg[1])` |
|         - | 6208 | `			);` |
|         - | 6209 | `	}` |
|         - | 6210 | `	/* Point to the internal representation of the input hashmaps */` |
|        14 | 6211 | `	pKey   = (ph7_hashmap *)apArg[0]->x.pOther;` |
|        14 | 6212 | `	pValue = (ph7_hashmap *)apArg[1]->x.pOther;` |
|        14 | 6213 | `	if( pKey->nEntry != pValue->nEntry ){` |
|         - | 6214 | `		/* Length mismatch -> ValueError */` |
|         3 | 6215 | `		return PH7_VmThrowException(pCtx,` |
|         - | 6216 | `			"ValueError",` |
|         - | 6217 | `			"array_combine(): Argument #1 ($keys) and argument #2 ($values) must have the same number of elements"` |
|         - | 6218 | `			);` |
|         - | 6219 | `	}` |
|         - | 6220 | `	/* Create a new array */` |
|        11 | 6221 | `	pArray = ph7_context_new_array(pCtx);` |
|        11 | 6222 | `	if( pArray == 0 ){` |
|       ! 0 | 6223 | `		ph7_result_bool(pCtx,0);` |
|       ! 0 | 6224 | `		return PH7_OK;` |
|         - | 6225 | `	}` |
|         - | 6226 | `	/* Perform the requested operation */` |
|        11 | 6227 | `	pKe = pKey->pFirst;` |
|        11 | 6228 | `	pVe = pValue->pFirst;` |
|        33 | 6229 | `	for( n = 0 ; n < pKey->nEntry ; n++ ){` |
|        23 | 6230 | `		ph7_value *pKeyVal = HashmapExtractNodeValue(pKe);` |
|        23 | 6231 | `		ph7_value *pValVal = HashmapExtractNodeValue(pVe);` |
|         - | 6232 | `		/* PHP treats floats used as keys in array_combine differently than` |
|         - | 6233 | `		 * ordinary offset access: the float is stringified rather than` |
|         - | 6234 | `		 * truncated.  To emulate this behavior we create a temporary copy of` |
|         - | 6235 | `		 * the value when it is a float and convert the copy to string.  The` |
|         - | 6236 | `		 * original array must not be mutated. */` |
|        23 | 6237 | `		ph7_value *pKeyCopy = pKeyVal;` |
|        23 | 6238 | `		if( ph7_value_is_float(pKeyVal) ){` |
|         5 | 6239 | `			ph7_value *pTmpKey = ph7_context_new_scalar(pCtx);` |
|         5 | 6240 | `			if( pTmpKey ){` |
|         5 | 6241 | `				PH7_MemObjStore(pKeyVal,pTmpKey);` |
|         - | 6242 | `				/* Convert copy to string so it becomes "1.5" or "2" etc. */` |
|         5 | 6243 | `				PH7_MemObjToString(pTmpKey);` |
|         5 | 6244 | `				pKeyCopy = pTmpKey;` |
|         2 | 6245 | `			}` |
|         2 | 6246 | `		}` |
|        23 | 6247 | `		ph7_array_add_elem(pArray,pKeyCopy,pValVal);` |
|         - | 6248 | `		/* Point to the next entry */` |
|        23 | 6249 | `		pKe = pKe->pPrev; /* Reverse link */` |
|        23 | 6250 | `		pVe = pVe->pPrev;` |
|        12 | 6251 | `	}` |
|         - | 6252 | `	/* Return the filled array */` |
|        11 | 6253 | `	ph7_result_value(pCtx,pArray);` |
|        11 | 6254 | `	return PH7_OK;` |
|        14 | 6255 | `}` |
|         - | 6256 | `/*` |
|         - | 6257 | ` * array array_reverse(array $array [,bool $preserve_keys = false ])` |
|         - | 6258 | ` *  Return an array with elements in reverse order.` |
|         - | 6259 | ` * Parameters` |
|         - | 6260 | ` *  $array` |
|         - | 6261 | ` *   The input array.` |
|         - | 6262 | ` *  $preserve_keys (optional)` |
|         - | 6263 | ` *   If set to TRUE keys are preserved.` |
|         - | 6264 | ` * Return` |
|         - | 6265 | ` *  The reversed array.` |
|         - | 6266 | ` */` |
|        20 | 6267 | `static int ph7_hashmap_reverse(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|         3 | 6268 | `{` |
|         - | 6269 | `	ph7_hashmap_node *pEntry;` |
|         - | 6270 | `	ph7_hashmap *pSrc;` |
|         - | 6271 | `	ph7_value *pArray;` |
|         - | 6272 | `	int bPreserve;` |
|         - | 6273 | `	sxu32 n;` |
|        23 | 6274 | `	if( nArg < 1 ){` |
|         4 | 6275 | `		return PH7_VmThrowException(pCtx,` |
|         - | 6276 | `			"ArgumentCountError",` |
|         - | 6277 | `			"array_reverse() expects at least 1 argument, %d given",` |
|         1 | 6278 | `			nArg` |
|         - | 6279 | `			);` |
|         - | 6280 | `	}` |
|         - | 6281 | `	/* Make sure we are dealing with a valid hashmap */` |
|        20 | 6282 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|         4 | 6283 | `		return PH7_VmThrowException(pCtx,` |
|         - | 6284 | `			"TypeError",` |
|         - | 6285 | `			"array_reverse(): Argument #1 ($array) must be of type array, %s given",` |
|         1 | 6286 | `			ph7_type_name(apArg[0])` |
|         - | 6287 | `			);` |
|         - | 6288 | `	}` |
|        17 | 6289 | `	bPreserve = FALSE;` |
|        17 | 6290 | `	if( nArg > 1 ){` |
|         7 | 6291 | `		bPreserve = ph7_value_to_bool(apArg[1]);` |
|         3 | 6292 | `	}` |
|         - | 6293 | `	/* Point to the internal representation of the input hashmap */` |
|        17 | 6294 | `	pSrc = (ph7_hashmap *)apArg[0]->x.pOther;` |
|         - | 6295 | `	/* Create a new array */` |
|        17 | 6296 | `	pArray = ph7_context_new_array(pCtx);` |
|        17 | 6297 | `	if( pArray == 0 ){` |
|       ! 0 | 6298 | `		ph7_result_null(pCtx);` |
|       ! 0 | 6299 | `		return PH7_OK;` |
|         - | 6300 | `	}` |
|         - | 6301 | `	/* Perform the requested operation */` |
|        17 | 6302 | `	pEntry = pSrc->pLast;` |
|        55 | 6303 | `	for( n = 0 ; n < pSrc->nEntry ; n++ ){` |
|         - | 6304 | `		/* String keys are always preserved; preserve_keys only affects int keys */` |
|        39 | 6305 | `		int bKeep = (pEntry->iType == HASHMAP_INT_NODE) ? bPreserve : TRUE;` |
|        39 | 6306 | `		HashmapInsertNode((ph7_hashmap *)pArray->x.pOther,pEntry,bKeep);` |
|         - | 6307 | `		/* Point to the previous entry */` |
|        39 | 6308 | `		pEntry = pEntry->pNext; /* Reverse link */` |
|        20 | 6309 | `	}` |
|        17 | 6310 | `	ph7_result_value(pCtx,pArray);` |
|        17 | 6311 | `	return PH7_OK;` |
|        13 | 6312 | `}` |
|         - | 6313 | `/*` |
|         - | 6314 | ` * array array_unique(array $array, int $flags = SORT_STRING)` |
|         - | 6315 | ` *  Removes duplicate values from an array.` |
|         - | 6316 | ` * Parameters` |
|         - | 6317 | ` *  $array` |
|         - | 6318 | ` *   The input array.` |
|         - | 6319 | ` *  $flags` |
|         - | 6320 | ` *   The optional second parameter may be used to modify the comparison` |
|         - | 6321 | ` *   behavior using these values:` |
|         - | 6322 | ` *     SORT_REGULAR - compare items normally (don't change types)` |
|         - | 6323 | ` *     SORT_NUMERIC - compare items numerically` |
|         - | 6324 | ` *     SORT_STRING  - compare items as strings` |
|         - | 6325 | ` * Return` |
|         - | 6326 | ` *  The filtered array.` |
|         - | 6327 | ` */` |
|        24 | 6328 | `static int ph7_hashmap_unique(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|         4 | 6329 | `{` |
|         - | 6330 | `	ph7_hashmap_node *pEntry;` |
|         - | 6331 | `	ph7_value *pNeedle;` |
|         - | 6332 | `	ph7_hashmap *pSrc;` |
|         - | 6333 | `	ph7_value *pArray;` |
|         - | 6334 | `	int bStrict;` |
|         - | 6335 | `	sxi32 rc;` |
|         - | 6336 | `	sxu32 n;` |
|        28 | 6337 | `	if( nArg < 1 ){` |
|         - | 6338 | `		/* Missing arguments, throw ArgumentCountError */` |
|         3 | 6339 | `		return PH7_VmThrowException(pCtx,` |
|         - | 6340 | `			"ArgumentCountError",` |
|         - | 6341 | `			"array_unique() expects at least 1 argument, 0 given"` |
|         - | 6342 | `			);` |
|         - | 6343 | `	}` |
|        25 | 6344 | `	if( nArg > 2 ){` |
|         - | 6345 | `		/* Too many arguments, throw ArgumentCountError */` |
|         4 | 6346 | `		return PH7_VmThrowException(pCtx,` |
|         - | 6347 | `			"ArgumentCountError",` |
|         - | 6348 | `			"array_unique() expects at most 2 arguments, %d given",` |
|         1 | 6349 | `			nArg` |
|         - | 6350 | `			);` |
|         - | 6351 | `	}` |
|         - | 6352 | `	/* Make sure we are dealing with a valid hashmap */` |
|        22 | 6353 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|         - | 6354 | `		/* Type mismatch, throw TypeError */` |
|         4 | 6355 | `		return PH7_VmThrowException(pCtx,` |
|         - | 6356 | `			"TypeError",` |
|         - | 6357 | `			"array_unique(): Argument #1 ($array) must be of type array, %s given",` |
|         1 | 6358 | `			ph7_type_name(apArg[0])` |
|         - | 6359 | `			);` |
|         - | 6360 | `	}` |
|        19 | 6361 | `	bStrict = FALSE;` |
|         - | 6362 | `	/* Point to the internal representation of the input hashmap */` |
|        19 | 6363 | `	pSrc = (ph7_hashmap *)apArg[0]->x.pOther;` |
|         - | 6364 | `	/* Create a new array */` |
|        19 | 6365 | `	pArray = ph7_context_new_array(pCtx);` |
|        19 | 6366 | `	if( pArray == 0 ){` |
|       ! 0 | 6367 | `		ph7_result_null(pCtx);` |
|       ! 0 | 6368 | `		return PH7_OK;` |
|         - | 6369 | `	}` |
|         - | 6370 | `	/* Perform the requested operation */` |
|        19 | 6371 | `	pEntry = pSrc->pFirst;` |
|        83 | 6372 | `	for( n = 0 ; n < pSrc->nEntry ; n++ ){` |
|        65 | 6373 | `		pNeedle = HashmapExtractNodeValue(pEntry);` |
|        65 | 6374 | `		rc = SXERR_NOTFOUND;` |
|        65 | 6375 | `		if( pNeedle ){` |
|        65 | 6376 | `			rc = HashmapFindValue((ph7_hashmap *)pArray->x.pOther,pNeedle,0,bStrict);` |
|        32 | 6377 | `		}` |
|        65 | 6378 | `		if( rc != SXRET_OK ){` |
|         - | 6379 | `			/* Perform the insertion */` |
|        37 | 6380 | `			HashmapInsertNode((ph7_hashmap *)pArray->x.pOther,pEntry,TRUE);` |
|        18 | 6381 | `		}` |
|         - | 6382 | `		/* Point to the next entry */` |
|        65 | 6383 | `		pEntry = pEntry->pPrev; /* Reverse link */` |
|        33 | 6384 | `	}` |
|         - | 6385 | `	/* Return the freshly created array */` |
|        19 | 6386 | `	ph7_result_value(pCtx,pArray);` |
|        19 | 6387 | `	return PH7_OK;` |
|        16 | 6388 | `}` |
|         - | 6389 | `/*` |
|         - | 6390 | ` * array array_flip(array $input)` |
|         - | 6391 | ` *  Exchanges all keys with their associated values in an array.` |
|         - | 6392 | ` * Parameter` |
|         - | 6393 | ` *  $input` |
|         - | 6394 | ` *   Input array.` |
|         - | 6395 | ` * Return` |
|         - | 6396 | ` *   The flipped array on success or NULL on failure.` |
|         - | 6397 | ` */` |
|        34 | 6398 | `static int ph7_hashmap_flip(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|         5 | 6399 | `{` |
|         - | 6400 | `	ph7_hashmap_node *pEntry;` |
|         - | 6401 | `	ph7_hashmap *pSrc;` |
|         - | 6402 | `	ph7_value *pArray;` |
|         - | 6403 | `	ph7_value *pKey;` |
|         - | 6404 | `	ph7_value sVal;` |
|         - | 6405 | `	sxu32 n;` |
|         - | 6406 |  |
|         - | 6407 | `	/* PHP requires exactly one argument */` |
|        39 | 6408 | `	if( nArg != 1 ){` |
|         - | 6409 | `		/* Use ArgumentCountError like other array helpers */` |
|         8 | 6410 | `		return PH7_VmThrowException(pCtx,` |
|         - | 6411 | `			"ArgumentCountError",` |
|         - | 6412 | `			"array_flip() expects exactly 1 argument, %d given",` |
|         2 | 6413 | `			nArg` |
|         - | 6414 | `			);` |
|         - | 6415 | `	}` |
|         - | 6416 | `	/* Make sure we are dealing with a valid hashmap */` |
|        33 | 6417 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|         - | 6418 | `		/* Type mismatch -> TypeError */` |
|         8 | 6419 | `		return PH7_VmThrowException(pCtx,` |
|         - | 6420 | `			"TypeError",` |
|         - | 6421 | `			"array_flip(): Argument #1 ($array) must be of type array, %s given",` |
|         2 | 6422 | `			ph7_type_name(apArg[0])` |
|         - | 6423 | `			);` |
|         - | 6424 | `	}` |
|         - | 6425 | `	/* Point to the internal representation of the input hashmap */` |
|        27 | 6426 | `	pSrc = (ph7_hashmap *)apArg[0]->x.pOther;` |
|         - | 6427 | `	/* Create a new array */` |
|        27 | 6428 | `	pArray = ph7_context_new_array(pCtx);` |
|        27 | 6429 | `	if( pArray == 0 ){` |
|       ! 0 | 6430 | `		ph7_result_null(pCtx);` |
|       ! 0 | 6431 | `		return PH7_OK;` |
|         - | 6432 | `	}` |
|         - | 6433 | `	/* Start processing */` |
|        27 | 6434 | `	pEntry = pSrc->pFirst;` |
|     22263 | 6435 | `	for( n = 0 ; n < pSrc->nEntry ; n++ ){` |
|         - | 6436 | `		/* Extract the node value (will become a key in the result) */` |
|     22237 | 6437 | `		pKey = HashmapExtractNodeValue(pEntry);` |
|     22237 | 6438 | `		if( pKey ){` |
|         - | 6439 | `			/* NULL values are not valid keys either, PHP emits a warning */` |
|     22237 | 6440 | `			if( pKey->iFlags & MEMOBJ_NULL ){` |
|         3 | 6441 | `				PH7_VmThrowError(pCtx->pVm,0,PH7_CTX_WARNING,` |
|         - | 6442 | `					"array_flip(): Can only flip string and integer values, entry skipped"` |
|         - | 6443 | `					);` |
|     22236 | 6444 | `			}else if( (pKey->iFlags & MEMOBJ_INT) \|\| (pKey->iFlags & MEMOBJ_STRING) ){` |
|         - | 6445 | `				/* Prepare the value for insertion (original key) */` |
|     22227 | 6446 | `				if( pEntry->iType == HASHMAP_INT_NODE ){` |
|     20001 | 6447 | `					PH7_MemObjInitFromInt(pSrc->pVm,&sVal,pEntry->xKey.iKey);` |
|     10001 | 6448 | `				}else{` |
|         - | 6449 | `					SyString sStr;` |
|      2227 | 6450 | `					SyStringInitFromBuf(&sStr,SyBlobData(&pEntry->xKey.sKey),SyBlobLength(&pEntry->xKey.sKey));` |
|      2227 | 6451 | `					PH7_MemObjInitFromString(pSrc->pVm,&sVal,&sStr);` |
|         - | 6452 | `				}` |
|         - | 6453 | `				/* Perform the insertion */` |
|     22227 | 6454 | `				ph7_array_add_elem(pArray,pKey,&sVal);` |
|         - | 6455 | `				/* Safely release the value because each inserted entry` |
|         - | 6456 | `				 * has its own private copy of the value.` |
|         - | 6457 | `				 */` |
|     22227 | 6458 | `				PH7_MemObjRelease(&sVal);` |
|     11114 | 6459 | `			}else{` |
|         - | 6460 | `				/* Unsupported value type -> emit warning and skip the entry */` |
|         9 | 6461 | `				PH7_VmThrowError(pCtx->pVm,0,PH7_CTX_WARNING,` |
|         - | 6462 | `					"array_flip(): Can only flip string and integer values, entry skipped"` |
|         - | 6463 | `					);` |
|         - | 6464 | `			}` |
|     11118 | 6465 | `		}` |
|         - | 6466 | `		/* Point to the next entry */` |
|     22237 | 6467 | `		pEntry = pEntry->pPrev; /* Reverse link */` |
|     11119 | 6468 | `	}` |
|         - | 6469 | `	/* Return the freshly created array */` |
|        27 | 6470 | `	ph7_result_value(pCtx,pArray);` |
|        27 | 6471 | `	return PH7_OK;` |
|        22 | 6472 | `}` |
|         - | 6473 | `/*` |
|         - | 6474 | ` * number array_sum(array $array )` |
|         - | 6475 | ` *  Calculate the sum of values in an array.` |
|         - | 6476 | ` * Parameters` |
|         - | 6477 | ` *  $array: The input array.` |
|         - | 6478 | ` * Return` |
|         - | 6479 | ` *  Returns the sum of values as an integer or float.` |
|         - | 6480 | ` */` |
|        24 | 6481 | `static void DoubleSum(ph7_context *pCtx,ph7_hashmap *pMap)` |
|         1 | 6482 | `{` |
|         - | 6483 | `	ph7_hashmap_node *pEntry;` |
|         - | 6484 | `	ph7_value *pObj;` |
|        25 | 6485 | `	double dSum = 0;` |
|         - | 6486 | `	sxu32 n;` |
|        25 | 6487 | `	pEntry = pMap->pFirst;` |
|        91 | 6488 | `	for( n = 0 ; n < pMap->nEntry ; n++ ){` |
|        67 | 6489 | `		pObj = HashmapExtractNodeValue(pEntry);` |
|        67 | 6490 | `		if( pObj ){` |
|        67 | 6491 | `			if( pObj->iFlags & MEMOBJ_REAL ){` |
|        29 | 6492 | `				dSum += pObj->rVal;` |
|        53 | 6493 | `			}else if( pObj->iFlags & (MEMOBJ_INT\|MEMOBJ_BOOL) ){` |
|        21 | 6494 | `				dSum += (double)pObj->x.iVal;` |
|        29 | 6495 | `			}else if( pObj->iFlags & MEMOBJ_STRING ){` |
|        15 | 6496 | `				if( SyBlobLength(&pObj->sBlob) > 0 ){` |
|        13 | 6497 | `					double dv = 0;` |
|        13 | 6498 | `					SyStrToReal((const char *)SyBlobData(&pObj->sBlob),SyBlobLength(&pObj->sBlob),(void *)&dv,0);` |
|        13 | 6499 | `					dSum += dv;` |
|         7 | 6500 | `				}` |
|        12 | 6501 | `			}else if( pObj->iFlags & MEMOBJ_HASHMAP ){` |
|         3 | 6502 | `				PH7_VmThrowError(pCtx->pVm,0,PH7_CTX_WARNING,` |
|         - | 6503 | `					"array_sum(): Addition is not supported on type array");` |
|         4 | 6504 | `			}else if( pObj->iFlags & MEMOBJ_OBJ ){` |
|       ! 0 | 6505 | `				PH7_VmThrowError(pCtx->pVm,0,PH7_CTX_WARNING,` |
|         - | 6506 | `					"array_sum(): Addition is not supported on type object");` |
|         3 | 6507 | `			}else if( pObj->iFlags & MEMOBJ_RES ){` |
|       ! 0 | 6508 | `				PH7_VmThrowError(pCtx->pVm,0,PH7_CTX_WARNING,` |
|         - | 6509 | `					"array_sum(): Addition is not supported on type resource");` |
|       ! 0 | 6510 | `			}` |
|         - | 6511 | `			/* NULL is silently treated as 0 (matches PHP) */` |
|        33 | 6512 | `		}` |
|         - | 6513 | `		/* Point to the next entry */` |
|        67 | 6514 | `		pEntry = pEntry->pPrev; /* Reverse link */` |
|        34 | 6515 | `	}` |
|         - | 6516 | `	/* Return sum */` |
|        25 | 6517 | `	ph7_result_double(pCtx,dSum);` |
|        25 | 6518 | `}` |
|        34 | 6519 | `static void Int64Sum(ph7_context *pCtx,ph7_hashmap *pMap)` |
|         2 | 6520 | `{` |
|         - | 6521 | `	ph7_hashmap_node *pEntry;` |
|         - | 6522 | `	ph7_value *pObj;` |
|        36 | 6523 | `	sxi64 nSum = 0;` |
|         - | 6524 | `	sxu32 n;` |
|        36 | 6525 | `	pEntry = pMap->pFirst;` |
|       144 | 6526 | `	for( n = 0 ; n < pMap->nEntry ; n++ ){` |
|       110 | 6527 | `		pObj = HashmapExtractNodeValue(pEntry);` |
|       110 | 6528 | `		if( pObj ){` |
|       110 | 6529 | `			if( pObj->iFlags & (MEMOBJ_INT\|MEMOBJ_BOOL) ){` |
|       100 | 6530 | `				nSum += pObj->x.iVal;` |
|        60 | 6531 | `			}else if( pObj->iFlags & MEMOBJ_STRING ){` |
|         7 | 6532 | `				if( SyBlobLength(&pObj->sBlob) > 0 ){` |
|         5 | 6533 | `					sxi64 nv = 0;` |
|         5 | 6534 | `					SyStrToInt64((const char *)SyBlobData(&pObj->sBlob),SyBlobLength(&pObj->sBlob),(void *)&nv,0);` |
|         5 | 6535 | `					nSum += nv;` |
|         3 | 6536 | `				}` |
|         8 | 6537 | `			}else if( pObj->iFlags & MEMOBJ_HASHMAP ){` |
|         3 | 6538 | `				PH7_VmThrowError(pCtx->pVm,0,PH7_CTX_WARNING,` |
|         - | 6539 | `					"array_sum(): Addition is not supported on type array");` |
|         4 | 6540 | `			}else if( pObj->iFlags & MEMOBJ_OBJ ){` |
|       ! 0 | 6541 | `				PH7_VmThrowError(pCtx->pVm,0,PH7_CTX_WARNING,` |
|         - | 6542 | `					"array_sum(): Addition is not supported on type object");` |
|         3 | 6543 | `			}else if( pObj->iFlags & MEMOBJ_RES ){` |
|       ! 0 | 6544 | `				PH7_VmThrowError(pCtx->pVm,0,PH7_CTX_WARNING,` |
|         - | 6545 | `					"array_sum(): Addition is not supported on type resource");` |
|       ! 0 | 6546 | `			}` |
|         - | 6547 | `			/* NULL is silently treated as 0 (matches PHP) */` |
|        54 | 6548 | `		}` |
|         - | 6549 | `		/* Point to the next entry */` |
|       110 | 6550 | `		pEntry = pEntry->pPrev; /* Reverse link */` |
|        56 | 6551 | `	}` |
|         - | 6552 | `	/* Return sum */` |
|        36 | 6553 | `	ph7_result_int64(pCtx,nSum);` |
|        36 | 6554 | `}` |
|         - | 6555 | `/* number array_sum(array $array )` |
|         - | 6556 | ` * (See block-coment above)` |
|         - | 6557 | ` */` |
|        72 | 6558 | `static int ph7_hashmap_sum(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|         5 | 6559 | `{` |
|         - | 6560 | `	ph7_hashmap_node *pEntry;` |
|         - | 6561 | `	ph7_hashmap *pMap;` |
|         - | 6562 | `	ph7_value *pObj;` |
|        77 | 6563 | `	int useDouble = 0;` |
|         - | 6564 | `	sxu32 n;` |
|         - | 6565 | `	/* PHP requires exactly one argument */` |
|        77 | 6566 | `	if( nArg != 1 ){` |
|         8 | 6567 | `		return PH7_VmThrowException(pCtx,` |
|         - | 6568 | `			"ArgumentCountError",` |
|         - | 6569 | `			"array_sum() expects exactly 1 argument, %d given",` |
|         2 | 6570 | `			nArg` |
|         - | 6571 | `			);` |
|         - | 6572 | `	}` |
|         - | 6573 | `	/* Make sure we are dealing with a valid hashmap */` |
|        71 | 6574 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|         - | 6575 | `		/* Type mismatch -> TypeError (php's true/false/class-name convention). */` |
|         - | 6576 | `		char zBuf[64];` |
|         8 | 6577 | `		return PH7_VmThrowException(pCtx,` |
|         - | 6578 | `			"TypeError",` |
|         - | 6579 | `			"array_sum(): Argument #1 ($array) must be of type array, %s given",` |
|         2 | 6580 | `			VmValueGivenName(apArg[0],zBuf,sizeof(zBuf))` |
|         - | 6581 | `			);` |
|         - | 6582 | `	}` |
|        66 | 6583 | `	pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|        66 | 6584 | `	if( pMap->nEntry < 1 ){` |
|         - | 6585 | `		/* Nothing to compute,return 0 */` |
|         7 | 6586 | `		ph7_result_int(pCtx,0);` |
|         7 | 6587 | `		return PH7_OK;` |
|         - | 6588 | `	}` |
|         - | 6589 | `	/* Scan all elements: if any value is a float, use floating-point` |
|         - | 6590 | `	 * arithmetic for the entire sum (matches PHP behaviour).` |
|         - | 6591 | `	 */` |
|        60 | 6592 | `	pEntry = pMap->pFirst;` |
|       176 | 6593 | `	for( n = 0 ; n < pMap->nEntry ; n++ ){` |
|       142 | 6594 | `		pObj = HashmapExtractNodeValue(pEntry);` |
|       142 | 6595 | `		if( pObj ){` |
|       142 | 6596 | `			if( pObj->iFlags & MEMOBJ_REAL ){` |
|        19 | 6597 | `				useDouble = 1;` |
|        19 | 6598 | `				break;` |
|         - | 6599 | `			}` |
|       124 | 6600 | `			if( pObj->iFlags & MEMOBJ_STRING ){` |
|        13 | 6601 | `				const char *zStr = (const char *)SyBlobData(&pObj->sBlob);` |
|        13 | 6602 | `				sxu32 nLen = SyBlobLength(&pObj->sBlob);` |
|         - | 6603 | `				sxu32 i;` |
|        23 | 6604 | `				for( i = 0 ; i < nLen ; i++ ){` |
|        17 | 6605 | `					if( zStr[i] == '.' \|\| zStr[i] == 'e' \|\| zStr[i] == 'E' ){` |
|         7 | 6606 | `						useDouble = 1;` |
|         7 | 6607 | `						break;` |
|         - | 6608 | `					}` |
|         6 | 6609 | `				}` |
|        13 | 6610 | `				if( useDouble ){` |
|         7 | 6611 | `					break;` |
|         - | 6612 | `				}` |
|         3 | 6613 | `			}` |
|        58 | 6614 | `		}` |
|       118 | 6615 | `		pEntry = pEntry->pPrev;` |
|        60 | 6616 | `	}` |
|        60 | 6617 | `	if( useDouble ){` |
|        25 | 6618 | `		DoubleSum(pCtx,pMap);` |
|        13 | 6619 | `	}else{` |
|        36 | 6620 | `		Int64Sum(pCtx,pMap);` |
|         - | 6621 | `	}` |
|        60 | 6622 | `	return PH7_OK;` |
|        41 | 6623 | `}` |
|         - | 6624 | `/*` |
|         - | 6625 | ` * number array_product(array $array )` |
|         - | 6626 | ` *  Calculate the product of values in an array.` |
|         - | 6627 | ` * Parameters` |
|         - | 6628 | ` *  $array: The input array.` |
|         - | 6629 | ` * Return` |
|         - | 6630 | ` *  Returns the product of values as an integer or float.` |
|         - | 6631 | ` */` |
|         2 | 6632 | `static void DoubleProd(ph7_context *pCtx,ph7_hashmap *pMap)` |
|         1 | 6633 | `{` |
|         - | 6634 | `	ph7_hashmap_node *pEntry;` |
|         - | 6635 | `	ph7_value *pObj;` |
|         - | 6636 | `	double dProd;` |
|         - | 6637 | `	sxu32 n;` |
|         3 | 6638 | `	pEntry = pMap->pFirst;` |
|         3 | 6639 | `	dProd = 1;` |
|         7 | 6640 | `	for( n = 0 ; n < pMap->nEntry ; n++ ){` |
|         5 | 6641 | `		pObj = HashmapExtractNodeValue(pEntry);` |
|         5 | 6642 | `		if( pObj && (pObj->iFlags & (MEMOBJ_NULL\|MEMOBJ_HASHMAP\|MEMOBJ_OBJ\|MEMOBJ_RES)) == 0){` |
|         5 | 6643 | `			if( pObj->iFlags & MEMOBJ_REAL ){` |
|         3 | 6644 | `				dProd *= pObj->rVal;` |
|         4 | 6645 | `			}else if( pObj->iFlags & (MEMOBJ_INT\|MEMOBJ_BOOL) ){` |
|         3 | 6646 | `				dProd *= (double)pObj->x.iVal;` |
|         1 | 6647 | `			}else if( pObj->iFlags & MEMOBJ_STRING ){` |
|       ! 0 | 6648 | `				if( SyBlobLength(&pObj->sBlob) > 0 ){` |
|       ! 0 | 6649 | `					double dv = 0;` |
|       ! 0 | 6650 | `					SyStrToReal((const char *)SyBlobData(&pObj->sBlob),SyBlobLength(&pObj->sBlob),(void *)&dv,0);` |
|       ! 0 | 6651 | `					dProd *= dv;` |
|       ! 0 | 6652 | `				}` |
|       ! 0 | 6653 | `			}` |
|         2 | 6654 | `		}` |
|         - | 6655 | `		/* Point to the next entry */` |
|         5 | 6656 | `		pEntry = pEntry->pPrev; /* Reverse link */` |
|         3 | 6657 | `	}` |
|         - | 6658 | `	/* Return product */` |
|         3 | 6659 | `	ph7_result_double(pCtx,dProd);` |
|         3 | 6660 | `}` |
|         2 | 6661 | `static void Int64Prod(ph7_context *pCtx,ph7_hashmap *pMap)` |
|         1 | 6662 | `{` |
|         - | 6663 | `	ph7_hashmap_node *pEntry;` |
|         - | 6664 | `	ph7_value *pObj;` |
|         - | 6665 | `	sxi64 nProd;` |
|         - | 6666 | `	sxu32 n;` |
|         3 | 6667 | `	pEntry = pMap->pFirst;` |
|         3 | 6668 | `	nProd = 1;` |
|         9 | 6669 | `	for( n = 0 ; n < pMap->nEntry ; n++ ){` |
|         7 | 6670 | `		pObj = HashmapExtractNodeValue(pEntry);` |
|         7 | 6671 | `		if( pObj && (pObj->iFlags & (MEMOBJ_NULL\|MEMOBJ_HASHMAP\|MEMOBJ_OBJ\|MEMOBJ_RES)) == 0){` |
|         7 | 6672 | `			if( pObj->iFlags & MEMOBJ_REAL ){` |
|       ! 0 | 6673 | `				nProd *= (sxi64)pObj->rVal;` |
|         7 | 6674 | `			}else if( pObj->iFlags & (MEMOBJ_INT\|MEMOBJ_BOOL) ){` |
|         7 | 6675 | `				nProd *= pObj->x.iVal;` |
|         3 | 6676 | `			}else if( pObj->iFlags & MEMOBJ_STRING ){` |
|       ! 0 | 6677 | `				if( SyBlobLength(&pObj->sBlob) > 0 ){` |
|       ! 0 | 6678 | `					sxi64 nv = 0;` |
|       ! 0 | 6679 | `					SyStrToInt64((const char *)SyBlobData(&pObj->sBlob),SyBlobLength(&pObj->sBlob),(void *)&nv,0);` |
|       ! 0 | 6680 | `					nProd *= nv;` |
|       ! 0 | 6681 | `				}` |
|       ! 0 | 6682 | `			}` |
|         3 | 6683 | `		}` |
|         - | 6684 | `		/* Point to the next entry */` |
|         7 | 6685 | `		pEntry = pEntry->pPrev; /* Reverse link */` |
|         4 | 6686 | `	}` |
|         - | 6687 | `	/* Return product */` |
|         3 | 6688 | `	ph7_result_int64(pCtx,nProd);` |
|         3 | 6689 | `}` |
|         - | 6690 | `/* number array_product(array $array )` |
|         - | 6691 | ` * (See block-block comment above)` |
|         - | 6692 | ` */` |
|        18 | 6693 | `static int ph7_hashmap_product(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|         1 | 6694 | `{` |
|         - | 6695 | `	ph7_hashmap *pMap;` |
|         - | 6696 | `	ph7_value *pObj;` |
|        19 | 6697 | `	if( nArg < 1 ){` |
|         - | 6698 | `		/* Missing arguments (arity is enforced upstream; defensive). */` |
|       ! 0 | 6699 | `		ph7_result_int(pCtx,1);` |
|       ! 0 | 6700 | `		return PH7_OK;` |
|         - | 6701 | `	}` |
|         - | 6702 | `	/* PHP 8: a non-array $array is a catchable TypeError, not a silent 0. */` |
|        19 | 6703 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|         - | 6704 | `		char zBuf[64];` |
|        19 | 6705 | `		return PH7_VmThrowException(pCtx,` |
|         - | 6706 | `			"TypeError",` |
|         - | 6707 | `			"array_product(): Argument #1 ($array) must be of type array, %s given",` |
|         6 | 6708 | `			VmValueGivenName(apArg[0],zBuf,sizeof(zBuf))` |
|         - | 6709 | `			);` |
|         - | 6710 | `	}` |
|         7 | 6711 | `	pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|         7 | 6712 | `	if( pMap->nEntry < 1 ){` |
|         - | 6713 | `		/* The product of an empty array is the multiplicative identity 1 (PHP). */` |
|         3 | 6714 | `		ph7_result_int(pCtx,1);` |
|         3 | 6715 | `		return PH7_OK;` |
|         - | 6716 | `	}` |
|         - | 6717 | `	/* If the first element is of type float,then perform floating` |
|         - | 6718 | `	 * point computaion.Otherwise switch to int64 computaion.` |
|         - | 6719 | `	 */` |
|         5 | 6720 | `	pObj = HashmapExtractNodeValue(pMap->pFirst);` |
|         5 | 6721 | `	if( pObj == 0 ){` |
|       ! 0 | 6722 | `		ph7_result_int(pCtx,0);` |
|       ! 0 | 6723 | `		return PH7_OK;` |
|         - | 6724 | `	}` |
|         5 | 6725 | `	if( pObj->iFlags & MEMOBJ_REAL ){` |
|         3 | 6726 | `		DoubleProd(pCtx,pMap);` |
|         2 | 6727 | `	}else{` |
|         3 | 6728 | `		Int64Prod(pCtx,pMap);` |
|         - | 6729 | `	}` |
|         5 | 6730 | `	return PH7_OK;` |
|        10 | 6731 | `}` |
|         - | 6732 | `/*` |
|         - | 6733 | ` * value array_rand(array $input[,int $num_req = 1 ])` |
|         - | 6734 | ` *  Pick one or more random entries out of an array.` |
|         - | 6735 | ` * Parameters` |
|         - | 6736 | ` * $input` |
|         - | 6737 | ` *  The input array.` |
|         - | 6738 | ` * $num_req` |
|         - | 6739 | ` *  Specifies how many entries you want to pick.` |
|         - | 6740 | ` * Return` |
|         - | 6741 | ` *  If you are picking only one entry, array_rand() returns the key for a random entry.` |
|         - | 6742 | ` *  Otherwise, it returns an array of keys for the random entries.` |
|         - | 6743 | ` *  NULL is returned on failure.` |
|         - | 6744 | ` */` |
|        42 | 6745 | `static int ph7_hashmap_rand(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|         1 | 6746 | `{` |
|         - | 6747 | `	ph7_hashmap_node *pNode;` |
|         - | 6748 | `	ph7_hashmap *pMap;` |
|        43 | 6749 | `	int nItem = 1;` |
|        43 | 6750 | `	if( nArg < 1 ){` |
|         - | 6751 | `		/* Missing argument (arity is enforced upstream; defensive) */` |
|       ! 0 | 6752 | `		ph7_result_null(pCtx);` |
|       ! 0 | 6753 | `		return PH7_OK;` |
|         - | 6754 | `	}` |
|         - | 6755 | `	/* php 8: $array must be an array (TypeError, not a silent NULL return) */` |
|        43 | 6756 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|         - | 6757 | `		char zBuf[64];` |
|        10 | 6758 | `		return PH7_VmThrowException(pCtx,` |
|         - | 6759 | `			"TypeError",` |
|         - | 6760 | `			"array_rand(): Argument #1 ($array) must be of type array, %s given",` |
|         3 | 6761 | `			VmValueGivenName(apArg[0],zBuf,sizeof(zBuf))` |
|         - | 6762 | `			);` |
|         - | 6763 | `	}` |
|         - | 6764 | `	/* php validates $num (and weak-coerces it) BEFORE the empty-array body` |
|         - | 6765 | `	 * check, matching its ZPP-before-body ordering. */` |
|        37 | 6766 | `	if( nArg > 1 ){` |
|        29 | 6767 | `		ph7_value *pNum = apArg[1];` |
|        28 | 6768 | `		if( ph7_value_is_array(pNum) \|\| ph7_value_is_object(pNum)` |
|        24 | 6769 | `			\|\| ph7_value_is_resource(pNum) ){` |
|         - | 6770 | `			char zBuf[64];` |
|        10 | 6771 | `			return PH7_VmThrowException(pCtx,` |
|         - | 6772 | `				"TypeError",` |
|         - | 6773 | `				"array_rand(): Argument #2 ($num) must be of type int, %s given",` |
|         3 | 6774 | `				VmValueGivenName(pNum,zBuf,sizeof(zBuf))` |
|         - | 6775 | `				);` |
|         - | 6776 | `		}` |
|        23 | 6777 | `		if( ph7_value_is_string(pNum) ){` |
|         - | 6778 | `			/* Weak int coercion of a string $num follows php's numeric-string` |
|         - | 6779 | `			 * grammar (whole string, int or float): a non-numeric string` |
|         - | 6780 | `			 * (incl. leading-numeric junk like "2abc" or "0x1A") is a TypeError,` |
|         - | 6781 | `			 * a well-formed float-string ("1e3") coerces like a float value.` |
|         - | 6782 | `			 * Reuses the range() ZPP number parser (§3.9 shared-helper note). */` |
|         - | 6783 | `			int len;` |
|         9 | 6784 | `			const char *zStr = ph7_value_to_string(pNum, &len);` |
|         - | 6785 | `			sxi64 iLong; double dReal;` |
|         9 | 6786 | `			sxu8 iKind = RangeStrToNumber(zStr, (sxu32)len, &iLong, &dReal);` |
|         9 | 6787 | `			if( iKind == RANGE_IN_ERROR ){` |
|         7 | 6788 | `				return PH7_VmThrowException(pCtx,` |
|         - | 6789 | `					"TypeError",` |
|         - | 6790 | `					"array_rand(): Argument #2 ($num) must be of type int, string given"` |
|         - | 6791 | `					);` |
|         - | 6792 | `			}` |
|         - | 6793 | `			/* Clamp into a signed-int band so an absurd magnitude still yields` |
|         - | 6794 | `			 * the out-of-range ValueError below without an out-of-int cast. */` |
|         3 | 6795 | `			if( iKind == RANGE_IN_DOUBLE ){` |
|         3 | 6796 | `				iLong = dReal <= 0.0 ? 0 : (dReal >= 2147483647.0 ? 2147483647 : (sxi64)dReal);` |
|         1 | 6797 | `			}` |
|         3 | 6798 | `			if( iLong > 2147483647 ){ iLong = 2147483647; }` |
|         3 | 6799 | `			else if( iLong < -2147483647 ){ iLong = -2147483647; }` |
|         3 | 6800 | `			nItem = (int)iLong;` |
|         2 | 6801 | `		}else{` |
|        15 | 6802 | `			nItem = ph7_value_to_int(pNum);` |
|         - | 6803 | `		}` |
|         8 | 6804 | `	}` |
|         - | 6805 | `	/* Point to the internal representation of the input hashmap */` |
|        25 | 6806 | `	pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|         - | 6807 | `	/* php 8: an empty array is a ValueError, not a NULL return */` |
|        25 | 6808 | `	if( pMap->nEntry < 1 ){` |
|         5 | 6809 | `		return PH7_VmThrowException(pCtx,` |
|         - | 6810 | `			"ValueError",` |
|         - | 6811 | `			"array_rand(): Argument #1 ($array) must not be empty"` |
|         - | 6812 | `			);` |
|         - | 6813 | `	}` |
|         - | 6814 | `	/* php 8: $num outside [1, count] is a ValueError, not a clamp/wrong value */` |
|        21 | 6815 | `	if( nItem < 1 \|\| nItem > (int)pMap->nEntry ){` |
|         9 | 6816 | `		return PH7_VmThrowException(pCtx,` |
|         - | 6817 | `			"ValueError",` |
|         - | 6818 | `			"array_rand(): Argument #2 ($num) must be between 1 and the number of elements in argument #1 ($array)"` |
|         - | 6819 | `			);` |
|         - | 6820 | `	}` |
|        13 | 6821 | `	if( nItem < 2 ){` |
|         - | 6822 | `		sxu32 nEntry;` |
|         - | 6823 | `		/* Select a random number */` |
|         9 | 6824 | `		nEntry = PH7_VmRandomNum(pMap->pVm) % pMap->nEntry;` |
|         - | 6825 | `		/* Extract the desired entry.` |
|         - | 6826 | `		 * Note that we perform a linear lookup here (later version must change this)` |
|         - | 6827 | `		 */` |
|         9 | 6828 | `		if( nEntry > pMap->nEntry / 2 ){` |
|         5 | 6829 | `			pNode = pMap->pLast;` |
|         5 | 6830 | `			nEntry = pMap->nEntry - nEntry;` |
|         5 | 6831 | `			if( nEntry > 1 ){` |
|       ! 0 | 6832 | `				for(;;){` |
|       ! 0 | 6833 | `					if( nEntry == 0 ){` |
|       ! 0 | 6834 | `						break;` |
|         - | 6835 | `					}` |
|         - | 6836 | `					/* Point to the previous entry */` |
|       ! 0 | 6837 | `					pNode = pNode->pNext; /* Reverse link */` |
|       ! 0 | 6838 | `					nEntry--;` |
|       ! 0 | 6839 | `				}` |
|       ! 0 | 6840 | `			}` |
|         3 | 6841 | `		}else{` |
|         5 | 6842 | `			pNode = pMap->pFirst;` |
|         2 | 6843 | `			for(;;){` |
|         6 | 6844 | `				if( nEntry == 0 ){` |
|         5 | 6845 | `					break;` |
|         - | 6846 | `				}` |
|         - | 6847 | `				/* Point to the next entry */` |
|         2 | 6848 | `				pNode = pNode->pPrev; /* Reverse link */` |
|         2 | 6849 | `				nEntry--;` |
|         1 | 6850 | `			}` |
|         - | 6851 | `		}` |
|         9 | 6852 | `		if( pNode->iType == HASHMAP_INT_NODE ){` |
|         - | 6853 | `			/* Int key */` |
|         7 | 6854 | `			ph7_result_int64(pCtx,pNode->xKey.iKey);` |
|         4 | 6855 | `		}else{` |
|         - | 6856 | `			/* Blob key */` |
|         3 | 6857 | `			ph7_result_string(pCtx,(const char *)SyBlobData(&pNode->xKey.sKey),(int)SyBlobLength(&pNode->xKey.sKey));` |
|         - | 6858 | `		}` |
|         5 | 6859 | `	}else{` |
|         - | 6860 | `		ph7_value sKey,*pArray;` |
|         - | 6861 | `		ph7_hashmap *pDest;` |
|         - | 6862 | `		/* Create a new array */` |
|         5 | 6863 | `		pArray = ph7_context_new_array(pCtx);` |
|         5 | 6864 | `		if( pArray == 0 ){` |
|       ! 0 | 6865 | `			ph7_result_null(pCtx);` |
|       ! 0 | 6866 | `			return PH7_OK;` |
|         - | 6867 | `		}` |
|         - | 6868 | `		/* Point to the internal representation of the hashmap */` |
|         5 | 6869 | `		pDest = (ph7_hashmap *)pArray->x.pOther;` |
|         5 | 6870 | `		PH7_MemObjInit(pDest->pVm,&sKey);` |
|         - | 6871 | `		/* Copy the first n items */` |
|         5 | 6872 | `		pNode = pMap->pFirst;` |
|         5 | 6873 | `		if( nItem > (int)pMap->nEntry ){` |
|       ! 0 | 6874 | `			nItem = (int)pMap->nEntry;` |
|       ! 0 | 6875 | `		}` |
|        15 | 6876 | `		while( nItem > 0){` |
|        11 | 6877 | `			PH7_HashmapExtractNodeKey(pNode,&sKey);` |
|        11 | 6878 | `			PH7_HashmapInsert(pDest,0/* Automatic index assign*/,&sKey);` |
|        11 | 6879 | `			PH7_MemObjRelease(&sKey);` |
|         - | 6880 | `			/* Point to the next entry */` |
|        11 | 6881 | `			pNode = pNode->pPrev; /* Reverse link */` |
|        11 | 6882 | `			nItem--;` |
|         1 | 6883 | `		}` |
|         - | 6884 | `		/* Shuffle the array */` |
|         5 | 6885 | `		HashmapMergeSort(pDest,HashmapCmpCallback7,0);` |
|         - | 6886 | `		/* Rehash node */` |
|         5 | 6887 | `		HashmapSortRehash(pDest);` |
|         - | 6888 | `		/* Return the random array */` |
|         5 | 6889 | `		ph7_result_value(pCtx,pArray);` |
|         - | 6890 | `	}` |
|        13 | 6891 | `	return PH7_OK;` |
|        22 | 6892 | `}` |
|         - | 6893 | `/*` |
|         - | 6894 | ` * array array_chunk (array $input,int $size [,bool $preserve_keys = false ])` |
|         - | 6895 | ` *  Split an array into chunks.` |
|         - | 6896 | ` * Parameters` |
|         - | 6897 | ` * $input` |
|         - | 6898 | ` *   The array to work on` |
|         - | 6899 | ` * $size` |
|         - | 6900 | ` *   The size of each chunk` |
|         - | 6901 | ` * $preserve_keys` |
|         - | 6902 | ` *   When set to TRUE keys will be preserved. Default is FALSE which will reindex` |
|         - | 6903 | ` *   the chunk numerically.` |
|         - | 6904 | ` * Return` |
|         - | 6905 | ` *  Returns a multidimensional numerically indexed array, starting with` |
|         - | 6906 | ` *  zero, with each dimension containing size elements.` |
|         - | 6907 | ` */` |
|        42 | 6908 | `static int ph7_hashmap_chunk(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|         5 | 6909 | `{` |
|         - | 6910 | `	ph7_value *pArray,*pChunk;` |
|         - | 6911 | `	ph7_hashmap_node *pEntry;` |
|         - | 6912 | `	ph7_hashmap *pMap;` |
|         - | 6913 | `	int bPreserve;` |
|         - | 6914 | `	sxu32 nChunk;` |
|         - | 6915 | `	sxu32 nSize;` |
|         - | 6916 | `	sxu32 n;` |
|         - | 6917 | `	/* Argument count and types follow PHP semantics. */` |
|        47 | 6918 | `	if( nArg < 2 ){` |
|         - | 6919 | `		/* fewer than required arguments -> ArgumentCountError */` |
|         4 | 6920 | `		return PH7_VmThrowException(pCtx,` |
|         - | 6921 | `			"ArgumentCountError",` |
|         - | 6922 | `			"array_chunk() expects at least 2 arguments, %d given",` |
|         1 | 6923 | `			nArg` |
|         - | 6924 | `			);` |
|         - | 6925 | `	}` |
|        45 | 6926 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|         4 | 6927 | `		return PH7_VmThrowException(pCtx,` |
|         - | 6928 | `			"TypeError",` |
|         - | 6929 | `			"array_chunk(): Argument #1 ($array) must be of type array, %s given",` |
|         1 | 6930 | `			ph7_type_name(apArg[0])` |
|         - | 6931 | `			);` |
|         - | 6932 | `	}` |
|         - | 6933 | `	/* Create a new array */` |
|        43 | 6934 | `	pArray = ph7_context_new_array(pCtx);` |
|        43 | 6935 | `	if( pArray == 0 ){` |
|       ! 0 | 6936 | `		ph7_result_null(pCtx);` |
|       ! 0 | 6937 | `		return PH7_OK;` |
|         - | 6938 | `	}` |
|         - | 6939 | `	/* Point to the internal representation of the input hashmap */` |
|        43 | 6940 | `	pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|         - | 6941 | `	/* Extract and validate the chunk size argument. */` |
|         - | 6942 | `	/* Reject types that cannot be sensibly converted to an integer. */` |
|        57 | 6943 | `	if( ph7_value_is_array(apArg[1]) \|\| ph7_value_is_object(apArg[1]) \|\|` |
|        81 | 6944 | `		ph7_value_is_resource(apArg[1]) \|\| ph7_value_is_null(apArg[1]) \|\|` |
|        38 | 6945 | `		ph7_value_is_bool(apArg[1]) ){` |
|       ! 0 | 6946 | `		return PH7_VmThrowException(pCtx,` |
|         - | 6947 | `			"TypeError",` |
|         - | 6948 | `			"array_chunk(): Argument #2 ($length) must be of type int, %s given",` |
|       ! 0 | 6949 | `			ph7_type_name(apArg[1])` |
|         - | 6950 | `			);` |
|         - | 6951 | `	}` |
|         - | 6952 | `	/* Strings that are non-numeric produce a TypeError.  Numeric` |
|         - | 6953 | `	 * strings are permitted; however those representing floats lose` |
|         - | 6954 | `	 * precision and PHP emits a deprecation warning. */` |
|        43 | 6955 | `	if( ph7_value_is_string(apArg[1]) ){` |
|         - | 6956 | `		int len;` |
|         3 | 6957 | `		sxu8 bReal = FALSE;` |
|         3 | 6958 | `		const char *zStr = ph7_value_to_string(apArg[1], &len);` |
|         3 | 6959 | `		if( SyStrIsNumeric(zStr, len, &bReal, 0) != SXRET_OK ){` |
|         3 | 6960 | `			return PH7_VmThrowException(pCtx,` |
|         - | 6961 | `				"TypeError",` |
|         - | 6962 | `				"array_chunk(): Argument #2 ($length) must be of type int, string given"` |
|         - | 6963 | `				);` |
|         - | 6964 | `		}` |
|       ! 0 | 6965 | `		if( bReal ){` |
|         - | 6966 | `			/* float-string -> warn but allow */` |
|       ! 0 | 6967 | `			ph7_context_throw_error_format(pCtx, E_DEPRECATED,` |
|         - | 6968 | `				"Implicit conversion from float-string \"%s\" to int loses precision",` |
|       ! 0 | 6969 | `				zStr` |
|         - | 6970 | `				);` |
|       ! 0 | 6971 | `		}` |
|       ! 0 | 6972 | `	}` |
|         - | 6973 | `	/* If the value is a float with a fractional component, emit a` |
|         - | 6974 | `	 * deprecation warning but continue.  The following conversion occurs` |
|         - | 6975 | `	 * later via ph7_value_to_int. */` |
|        40 | 6976 | `	if( ph7_value_is_float(apArg[1]) ){` |
|         3 | 6977 | `		double d = ph7_value_to_double(apArg[1]);` |
|         3 | 6978 | `		sxi64 i = (sxi64)d;` |
|         3 | 6979 | `		if( d != (double)i ){` |
|         4 | 6980 | `			ph7_context_throw_error_format(pCtx, E_DEPRECATED,` |
|         - | 6981 | `				"Implicit conversion from float %g to int loses precision",` |
|         1 | 6982 | `				d` |
|         - | 6983 | `				);` |
|         1 | 6984 | `		}` |
|         1 | 6985 | `	}` |
|         - | 6986 | `	/* Convert using ph7_value_to_int; now that float fractions are` |
|         - | 6987 | `	 * eliminated, this will not produce a warning. */` |
|         - | 6988 | `	{` |
|        40 | 6989 | `		sxi64 nSizeSigned = ph7_value_to_int(apArg[1]);` |
|        40 | 6990 | `		if( nSizeSigned < 1 ){` |
|         - | 6991 | `			/* size <= 0 -> ValueError */` |
|         6 | 6992 | `			return PH7_VmThrowException(pCtx,` |
|         - | 6993 | `				"ValueError",` |
|         - | 6994 | `				"array_chunk(): Argument #2 ($length) must be greater than 0"` |
|         - | 6995 | `				);` |
|         - | 6996 | `		}` |
|        35 | 6997 | `		nSize = (sxu32)nSizeSigned;` |
|         - | 6998 | `	}` |
|        35 | 6999 | `	if( nSize >= pMap->nEntry ){` |
|         - | 7000 | `		/* Return the whole array */` |
|         3 | 7001 | `		ph7_array_add_elem(pArray,0,apArg[0]);` |
|         3 | 7002 | `		ph7_result_value(pCtx,pArray);` |
|         3 | 7003 | `		return PH7_OK;` |
|         - | 7004 | `	}` |
|        33 | 7005 | `	bPreserve = 0;` |
|        33 | 7006 | `	if( nArg > 2 ){` |
|         - | 7007 | `		/* The third argument has a bool type hint in PHP.  Values that` |
|         - | 7008 | `		 * cannot be sensibly converted (arrays, objects, resources) are` |
|         - | 7009 | `		 * rejected with a TypeError.  Scalars and null coerce to bool` |
|         - | 7010 | `		 * normally, matching PHP behaviour. */` |
|        35 | 7011 | `		if( ph7_value_is_array(apArg[2]) \|\|` |
|        35 | 7012 | `			ph7_value_is_object(apArg[2]) \|\|` |
|        20 | 7013 | `			ph7_value_is_resource(apArg[2]) ){` |
|         8 | 7014 | `			return PH7_VmThrowException(pCtx,` |
|         - | 7015 | `				"TypeError",` |
|         - | 7016 | `				"array_chunk(): Argument #3 ($preserve_keys) must be of type bool, %s given",` |
|         4 | 7017 | `				ph7_type_name(apArg[2])` |
|         - | 7018 | `				);` |
|         - | 7019 | `		}` |
|        21 | 7020 | `		bPreserve = ph7_value_to_bool(apArg[2]);` |
|        10 | 7021 | `	}` |
|         - | 7022 | `	/* Start processing */` |
|        27 | 7023 | `	pEntry = pMap->pFirst;` |
|        27 | 7024 | `	nChunk = 0;` |
|        27 | 7025 | `	pChunk = 0;` |
|        27 | 7026 | `	n = pMap->nEntry;` |
|        56 | 7027 | `	for( ;; ){` |
|       113 | 7028 | `		if( n < 1 ){` |
|         - | 7029 | `			/* When the loop terminates we may still have a current chunk` |
|         - | 7030 | `			 * that hasn't been added to the result array.  The previous` |
|         - | 7031 | `			 * implementation only pushed it if nChunk>0 which dropped the` |
|         - | 7032 | `			 * final chunk when the input size was an exact multiple of` |
|         - | 7033 | `			 * the chunk length.  Always append the pending chunk if it` |
|         - | 7034 | `			 * exists. */` |
|        27 | 7035 | `			if( pChunk ){` |
|        27 | 7036 | `				ph7_array_add_elem(pArray,0,pChunk); /* Will have its own copy */` |
|        13 | 7037 | `			}` |
|        27 | 7038 | `			break;` |
|         - | 7039 | `		}` |
|        87 | 7040 | `		if( nChunk < 1 ){` |
|        71 | 7041 | `			if( pChunk ){` |
|         - | 7042 | `				/* Put the first chunk */` |
|        45 | 7043 | `				ph7_array_add_elem(pArray,0,pChunk); /* Will have it's own copy */` |
|        22 | 7044 | `			}` |
|         - | 7045 | `			/* Create a new dimension */` |
|        71 | 7046 | `			pChunk = ph7_context_new_array(pCtx); /* Don't worry about freeing memory here,everything` |
|         - | 7047 | `												   * will be automatically released as soon we return` |
|         - | 7048 | `												   * from this function */` |
|        71 | 7049 | `			if( pChunk == 0 ){` |
|       ! 0 | 7050 | `				break;` |
|         - | 7051 | `			}` |
|        71 | 7052 | `			nChunk = nSize;` |
|        35 | 7053 | `		}` |
|         - | 7054 | `		/* Insert the entry */` |
|        87 | 7055 | `		HashmapInsertNode((ph7_hashmap *)pChunk->x.pOther,pEntry,bPreserve);` |
|         - | 7056 | `		/* Point to the next entry */` |
|        87 | 7057 | `		pEntry = pEntry->pPrev; /* Reverse link */` |
|        87 | 7058 | `		nChunk--;` |
|        87 | 7059 | `		n--;` |
|         1 | 7060 | `	}` |
|         - | 7061 | `	/* Return the multidimensional array */` |
|        27 | 7062 | `	ph7_result_value(pCtx,pArray);` |
|        27 | 7063 | `	return PH7_OK;` |
|        26 | 7064 | `}` |
|         - | 7065 | `/*` |
|         - | 7066 | ` * array array_pad(array $input,int $pad_size,value $pad_value)` |
|         - | 7067 | ` *  Pad array to the specified length with a value.` |
|         - | 7068 | ` * $input` |
|         - | 7069 | ` *   Initial array of values to pad.` |
|         - | 7070 | ` * $pad_size` |
|         - | 7071 | ` *   New size of the array.` |
|         - | 7072 | ` * $pad_value` |
|         - | 7073 | ` *   Value to pad if input is less than pad_size.` |
|         - | 7074 | ` */` |
|        50 | 7075 | `static int ph7_hashmap_pad(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|         5 | 7076 | `{` |
|         - | 7077 | `	ph7_hashmap *pMap;` |
|         - | 7078 | `	ph7_value *pArray;` |
|         - | 7079 | `	int nEntry;` |
|        55 | 7080 | `	if( nArg != 3 ){` |
|        12 | 7081 | `		return PH7_VmThrowException(pCtx,` |
|         - | 7082 | `			"ArgumentCountError",` |
|         - | 7083 | `			"array_pad() expects exactly 3 arguments, %d given",` |
|         3 | 7084 | `			nArg` |
|         - | 7085 | `			);` |
|         - | 7086 | `	}` |
|        46 | 7087 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|         - | 7088 | `		char zBuf[64];` |
|        14 | 7089 | `		return PH7_VmThrowException(pCtx,` |
|         - | 7090 | `			"TypeError",` |
|         - | 7091 | `			"array_pad(): Argument #1 ($array) must be of type array, %s given",` |
|         4 | 7092 | `			VmValueGivenName(apArg[0],zBuf,sizeof(zBuf))` |
|         - | 7093 | `			);` |
|         - | 7094 | `	}` |
|         - | 7095 | `	/* php 8: $length must be int-coercible. An array/object/resource or a` |
|         - | 7096 | `	 * non-numeric string throws a TypeError instead of silently padding to 0;` |
|         - | 7097 | `	 * a numeric string is weak-coerced via php's is_numeric_string grammar` |
|         - | 7098 | `	 * (reusing the shared RangeStrToNumber, like array_rand's $num). */` |
|        36 | 7099 | `	if( ph7_value_is_array(apArg[1]) \|\| ph7_value_is_object(apArg[1])` |
|        34 | 7100 | `		\|\| ph7_value_is_resource(apArg[1]) ){` |
|         - | 7101 | `		char zBuf[64];` |
|         7 | 7102 | `		return PH7_VmThrowException(pCtx,` |
|         - | 7103 | `			"TypeError",` |
|         - | 7104 | `			"array_pad(): Argument #2 ($length) must be of type int, %s given",` |
|         4 | 7105 | `			VmValueGivenName(apArg[1],zBuf,sizeof(zBuf))` |
|         - | 7106 | `			);` |
|         - | 7107 | `	}` |
|        33 | 7108 | `	if( ph7_value_is_string(apArg[1]) ){` |
|         - | 7109 | `		int nStr;` |
|        11 | 7110 | `		const char *zStr = ph7_value_to_string(apArg[1],&nStr);` |
|         - | 7111 | `		sxi64 iLong; double dReal;` |
|        11 | 7112 | `		sxu8 iKind = RangeStrToNumber(zStr,(sxu32)nStr,&iLong,&dReal);` |
|        11 | 7113 | `		if( iKind == RANGE_IN_ERROR ){` |
|         5 | 7114 | `			return PH7_VmThrowException(pCtx,` |
|         - | 7115 | `				"TypeError",` |
|         - | 7116 | `				"array_pad(): Argument #2 ($length) must be of type int, string given"` |
|         - | 7117 | `				);` |
|         - | 7118 | `		}` |
|         7 | 7119 | `		nEntry = (int)(iKind == RANGE_IN_DOUBLE ? (sxi64)dReal : iLong);` |
|         4 | 7120 | `	}else{` |
|        23 | 7121 | `		nEntry = ph7_value_to_int(apArg[1]);` |
|         - | 7122 | `	}` |
|         - | 7123 | `	/* Create a new array */` |
|        29 | 7124 | `	pArray = ph7_context_new_array(pCtx);` |
|        29 | 7125 | `	if( pArray == 0 ){` |
|       ! 0 | 7126 | `		return PH7_ContextMemoryError(pCtx);` |
|         - | 7127 | `	}` |
|         - | 7128 | `	/* Point to the internal representation of the input hashmap */` |
|        29 | 7129 | `	pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|        29 | 7130 | `	if( nEntry < 0 ){` |
|         9 | 7131 | `		nEntry = -nEntry;` |
|         9 | 7132 | `		if( nEntry > (int)pMap->nEntry ){` |
|         5 | 7133 | `			nEntry -= (int)pMap->nEntry;` |
|         - | 7134 | `			/* Insert given items first */` |
|        17 | 7135 | `			while( nEntry > 0 ){` |
|        13 | 7136 | `				if( ph7_array_add_elem(pArray,0,apArg[2]) != SXRET_OK ){` |
|       ! 0 | 7137 | `					return PH7_ContextMemoryError(pCtx);` |
|         - | 7138 | `				}` |
|        13 | 7139 | `				nEntry--;` |
|         1 | 7140 | `			}` |
|         - | 7141 | `			/* Merge the two arrays */` |
|         5 | 7142 | `			HashmapMerge(pMap,(ph7_hashmap *)pArray->x.pOther);` |
|         3 | 7143 | `		}else{` |
|         5 | 7144 | `			PH7_HashmapDup(pMap,(ph7_hashmap *)pArray->x.pOther);` |
|         1 | 7145 | `		}` |
|        25 | 7146 | `	}else if( nEntry > 0 ){` |
|        19 | 7147 | `		if( nEntry > (int)pMap->nEntry ){` |
|        15 | 7148 | `			nEntry -= (int)pMap->nEntry;` |
|         - | 7149 | `			/* Merge the two arrays first */` |
|        15 | 7150 | `			HashmapMerge(pMap,(ph7_hashmap *)pArray->x.pOther);` |
|         - | 7151 | `			/* Insert given items */` |
|        65 | 7152 | `			while( nEntry > 0 ){` |
|        51 | 7153 | `				if( ph7_array_add_elem(pArray,0,apArg[2]) != SXRET_OK ){` |
|       ! 0 | 7154 | `					return PH7_ContextMemoryError(pCtx);` |
|         - | 7155 | `				}` |
|        51 | 7156 | `				nEntry--;` |
|         1 | 7157 | `			}` |
|         8 | 7158 | `		}else{` |
|         5 | 7159 | `			PH7_HashmapDup(pMap,(ph7_hashmap *)pArray->x.pOther);` |
|         - | 7160 | `		}` |
|        10 | 7161 | `	}else{` |
|         - | 7162 | `		/* nEntry == 0: return a copy of the input array */` |
|         3 | 7163 | `		PH7_HashmapDup(pMap,(ph7_hashmap *)pArray->x.pOther);` |
|         - | 7164 | `	}` |
|         - | 7165 | `	/* Return the new array */` |
|        29 | 7166 | `	ph7_result_value(pCtx,pArray);` |
|        29 | 7167 | `	return PH7_OK;` |
|        30 | 7168 | `}` |
|         - | 7169 | `/*` |
|         - | 7170 | ` * array array_replace(array &$array,array &$array1,...)` |
|         - | 7171 | ` *  Replaces elements from passed arrays into the first array.` |
|         - | 7172 | ` * Parameters` |
|         - | 7173 | ` * $array` |
|         - | 7174 | ` *   The array in which elements are replaced.` |
|         - | 7175 | ` * $array1` |
|         - | 7176 | ` *   The array from which elements will be extracted.` |
|         - | 7177 | ` * ....` |
|         - | 7178 | ` *  More arrays from which elements will be extracted.` |
|         - | 7179 | ` *  Values from later arrays overwrite the previous values.` |
|         - | 7180 | ` * Return` |
|         - | 7181 | ` *  Returns an array.` |
|         - | 7182 | ` *  Throws ArgumentCountError if no arguments are given.` |
|         - | 7183 | ` *  Throws TypeError if any argument is not an array.` |
|         - | 7184 | ` */` |
|        22 | 7185 | `static int ph7_hashmap_replace(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|         4 | 7186 | `{` |
|         - | 7187 | `	ph7_hashmap *pMap;` |
|         - | 7188 | `	ph7_value *pArray;` |
|         - | 7189 | `	int i;` |
|        26 | 7190 | `	if( nArg < 1 ){` |
|         3 | 7191 | `		return PH7_VmThrowException(pCtx,` |
|         - | 7192 | `			"ArgumentCountError",` |
|         - | 7193 | `			"array_replace() expects at least 1 argument, 0 given"` |
|         - | 7194 | `			);` |
|         - | 7195 | `	}` |
|        23 | 7196 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|         4 | 7197 | `		return PH7_VmThrowException(pCtx,` |
|         - | 7198 | `			"TypeError",` |
|         - | 7199 | `			"array_replace(): Argument #1 ($array) must be of type array, %s given",` |
|         1 | 7200 | `			ph7_type_name(apArg[0])` |
|         - | 7201 | `			);` |
|         - | 7202 | `	}` |
|         - | 7203 | `	/* Create a new array */` |
|        20 | 7204 | `	pArray = ph7_context_new_array(pCtx);` |
|        20 | 7205 | `	if( pArray == 0 ){` |
|       ! 0 | 7206 | `		ph7_result_null(pCtx);` |
|       ! 0 | 7207 | `		return PH7_OK;` |
|         - | 7208 | `	}` |
|         - | 7209 | `	/* Overwrite from the first array */` |
|        20 | 7210 | `	pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|        20 | 7211 | `	HashmapOverwrite(pMap,(ph7_hashmap *)pArray->x.pOther);` |
|         - | 7212 | `	/* Perform the requested operation for remaining arrays */` |
|        36 | 7213 | `	for( i = 1 ; i < nArg ; i++ ){` |
|        20 | 7214 | `		if( !ph7_value_is_array(apArg[i]) ){` |
|         - | 7215 | `			/* Type mismatch -> TypeError */` |
|         4 | 7216 | `			return PH7_VmThrowException(pCtx,` |
|         - | 7217 | `				"TypeError",` |
|         - | 7218 | `				"array_replace(): Argument #%d must be of type array, %s given",` |
|         1 | 7219 | `				i + 1,` |
|         2 | 7220 | `				ph7_type_name(apArg[i])` |
|         - | 7221 | `				);` |
|         - | 7222 | `		}` |
|         - | 7223 | `		/* Point to the internal representation of the input hashmap */` |
|        17 | 7224 | `		pMap = (ph7_hashmap *)apArg[i]->x.pOther;` |
|        17 | 7225 | `		HashmapOverwrite(pMap,(ph7_hashmap *)pArray->x.pOther);` |
|         9 | 7226 | `	}` |
|         - | 7227 | `	/* Return the new array */` |
|        17 | 7228 | `	ph7_result_value(pCtx,pArray);` |
|        17 | 7229 | `	return PH7_OK;` |
|        15 | 7230 | `}` |
|         - | 7231 | `/*` |
|         - | 7232 | ` * array array_filter(array $input [,callback $callback ])` |
|         - | 7233 | ` *  Filters elements of an array using a callback function.` |
|         - | 7234 | ` * Parameters` |
|         - | 7235 | ` *  $input` |
|         - | 7236 | ` *    The array to iterate over` |
|         - | 7237 | ` * $callback` |
|         - | 7238 | ` *    The callback function to use` |
|         - | 7239 | ` *    If no callback is supplied, all entries of input equal to FALSE (see converting to boolean)` |
|         - | 7240 | ` *    will be removed.` |
|         - | 7241 | ` * Return` |
|         - | 7242 | ` *  The filtered array.` |
|         - | 7243 | ` */` |
|        32 | 7244 | `static int ph7_hashmap_filter(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|         2 | 7245 | `{` |
|         - | 7246 | `	ph7_hashmap_node *pEntry;` |
|         - | 7247 | `	ph7_hashmap *pMap;` |
|         - | 7248 | `	ph7_value *pArray;` |
|         - | 7249 | `	ph7_value sResult;   /* Callback result */` |
|         - | 7250 | `	ph7_value *pValue;` |
|         - | 7251 | `	sxi32 rc;` |
|         - | 7252 | `	int keep;` |
|         - | 7253 | `	sxu32 n;` |
|        34 | 7254 | `	if( nArg < 1 ){` |
|         - | 7255 | `		/* Missing argument (arity is enforced upstream; defensive) */` |
|       ! 0 | 7256 | `		ph7_result_null(pCtx);` |
|       ! 0 | 7257 | `		return PH7_OK;` |
|         - | 7258 | `	}` |
|         - | 7259 | `	/* php 8: $array must be an array (TypeError, not a silent NULL return) */` |
|        34 | 7260 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|         - | 7261 | `		char zBuf[64];` |
|        22 | 7262 | `		return PH7_VmThrowException(pCtx,` |
|         - | 7263 | `			"TypeError",` |
|         - | 7264 | `			"array_filter(): Argument #1 ($array) must be of type array, %s given",` |
|         7 | 7265 | `			VmValueGivenName(apArg[0],zBuf,sizeof(zBuf))` |
|         - | 7266 | `			);` |
|         - | 7267 | `	}` |
|         - | 7268 | `	/* Create a new array */` |
|        20 | 7269 | `	pArray = ph7_context_new_array(pCtx);` |
|        20 | 7270 | `	if( pArray == 0 ){` |
|       ! 0 | 7271 | `		ph7_result_null(pCtx);` |
|       ! 0 | 7272 | `		return PH7_OK;` |
|         - | 7273 | `	}` |
|         - | 7274 | `	/* Point to the internal representation of the input hashmap */` |
|        20 | 7275 | `	pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|        20 | 7276 | `	pEntry = pMap->pFirst;` |
|        20 | 7277 | `	PH7_MemObjInit(pMap->pVm,&sResult);` |
|        20 | 7278 | `	sResult.nIdx = SXU32_HIGH; /* Mark as constant */` |
|         - | 7279 | `	/* Perform the requested operation */` |
|        78 | 7280 | `	for( n = 0 ; n < pMap->nEntry ; n++ ){` |
|         - | 7281 | `		/* Extract node value (may be NULL if allocation failed) */` |
|        64 | 7282 | `		pValue = HashmapExtractNodeValue(pEntry);` |
|        64 | 7283 | `		if( pValue == 0 ){` |
|         - | 7284 | `			/* Can happen if SySetAt() failed earlier; drop the entry. */` |
|       ! 0 | 7285 | `			keep = FALSE;` |
|        64 | 7286 | `		}else if( nArg > 1 && !ph7_value_is_null(apArg[1]) ){` |
|         - | 7287 | `			/* Callback was supplied (not NULL).  PHP 8 throws a` |
|         - | 7288 | `				* TypeError when the value is not callable or null; prior PH7` |
|         - | 7289 | `				* silently dropped the element.  Emit similar message. */` |
|        36 | 7290 | `			if( !ph7_value_is_callable(apArg[1]) ){` |
|         3 | 7291 | `				if( ph7_value_is_string(apArg[1]) ){` |
|         - | 7292 | `					int len;` |
|         3 | 7293 | `					const char *zName = ph7_value_to_string(apArg[1], &len);` |
|         4 | 7294 | `					return PH7_VmThrowException(pCtx,` |
|         - | 7295 | `						"TypeError",` |
|         - | 7296 | `						"array_filter(): Argument #2 ($callback) must be a valid callback or null, function \"%s\" not found or invalid function name",` |
|         1 | 7297 | `						zName` |
|         - | 7298 | `						);` |
|       ! 0 | 7299 | `				}else{` |
|       ! 0 | 7300 | `					return PH7_VmThrowException(pCtx,` |
|         - | 7301 | `						"TypeError",` |
|         - | 7302 | `						"array_filter(): Argument #2 ($callback) must be a valid callback or null, %s given",` |
|       ! 0 | 7303 | `						ph7_type_name(apArg[1])` |
|         - | 7304 | `						);` |
|         - | 7305 | `				}` |
|         - | 7306 | `			}` |
|        33 | 7307 | `			keep = FALSE;` |
|        33 | 7308 | `			rc = PH7_VmCallUserFunction(pMap->pVm,apArg[1],1,&pValue,&sResult);` |
|        33 | 7309 | `			if( rc == PH7_EXCEPTION ){` |
|         - | 7310 | `				/* The callback raised: propagate so the dispatcher unwinds. */` |
|         3 | 7311 | `				PH7_MemObjRelease(&sResult);` |
|         3 | 7312 | `				return PH7_EXCEPTION;` |
|         - | 7313 | `			}` |
|        31 | 7314 | `			if( rc == SXRET_OK ){` |
|         - | 7315 | `				/* Perform a boolean cast */` |
|        31 | 7316 | `				keep = ph7_value_to_bool(&sResult);` |
|        15 | 7317 | `			}` |
|        31 | 7318 | `			PH7_MemObjRelease(&sResult);` |
|        16 | 7319 | `		}else{` |
|         - | 7320 | `			/* No callback provided or callback explicitly NULL: use default` |
|         - | 7321 | `			 * behaviour where "empty" values are removed. This also covers` |
|         - | 7322 | `			 * the case where the callback argument is missing entirely.` |
|         - | 7323 | `			 */` |
|        29 | 7324 | `			keep = !PH7_MemObjIsEmpty(pValue);` |
|         - | 7325 | `		}` |
|        59 | 7326 | `		if( keep ){` |
|         - | 7327 | `			/* Perform the insertion,now the callback returned true */` |
|        21 | 7328 | `			HashmapInsertNode((ph7_hashmap *)pArray->x.pOther,pEntry,TRUE);` |
|        10 | 7329 | `		}` |
|         - | 7330 | `		/* Point to the next entry */` |
|        59 | 7331 | `		pEntry = pEntry->pPrev; /* Reverse link */` |
|        30 | 7332 | `	}` |
|        15 | 7333 | `	ph7_result_value(pCtx,pArray);` |
|        15 | 7334 | `	return PH7_OK;` |
|        18 | 7335 | `}` |
|         - | 7336 | `/*` |
|         - | 7337 | ` * array array_map(?callable $callback, array $array, array ...$arrays)` |
|         - | 7338 | ` *  Applies the callback to the elements of the given arrays.` |
|         - | 7339 | ` * Parameters` |
|         - | 7340 | ` *  $callback` |
|         - | 7341 | ` *   A callable to run for each element in each array, or NULL. With a single` |
|         - | 7342 | ` *   array and a NULL callback this is the identity function (the array is` |
|         - | 7343 | ` *   returned unchanged); with several arrays and a NULL callback the arrays` |
|         - | 7344 | ` *   are zipped together.` |
|         - | 7345 | ` *  $array` |
|         - | 7346 | ` *   The first array to run through the callback function.` |
|         - | 7347 | ` *  $arrays` |
|         - | 7348 | ` *   Zero or more additional arrays to process in parallel.` |
|         - | 7349 | ` * Return` |
|         - | 7350 | ` *  Returns an array containing the results of applying the callback function.` |
|         - | 7351 | ` *  With a single array the keys are preserved; with several arrays the result` |
|         - | 7352 | ` *  is re-indexed and the iteration runs to the length of the longest array,` |
|         - | 7353 | ` *  padding shorter arrays with NULL.` |
|         - | 7354 | ` */` |
|        56 | 7355 | `static int ph7_hashmap_map(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|         5 | 7356 | `{` |
|         - | 7357 | `	ph7_value *pArray,*pValue,sKey,sResult;` |
|         - | 7358 | `	ph7_hashmap_node *pEntry;` |
|         - | 7359 | `	ph7_hashmap *pMap;` |
|         - | 7360 | `	ph7_vm *pVm;` |
|         - | 7361 | `	int bNullCallback;` |
|         - | 7362 | `	sxi32 rc;` |
|         - | 7363 | `	int i;` |
|         - | 7364 | `	sxu32 n;` |
|        61 | 7365 | `	if( nArg < 2 ){` |
|         8 | 7366 | `		return PH7_VmThrowException(pCtx,` |
|         - | 7367 | `			"ArgumentCountError",` |
|         - | 7368 | `			"array_map() expects at least 2 arguments, %d given",` |
|         2 | 7369 | `			nArg` |
|         - | 7370 | `			);` |
|         - | 7371 | `	}` |
|        56 | 7372 | `	bNullCallback = ph7_value_is_null(apArg[0]);` |
|        56 | 7373 | `	if( !bNullCallback && !ph7_value_is_callable(apArg[0]) ){` |
|         5 | 7374 | `		if( ph7_value_is_string(apArg[0]) ){` |
|         3 | 7375 | `			const char *zFunc = ph7_value_to_string(apArg[0],0);` |
|         4 | 7376 | `			return PH7_VmThrowException(pCtx,` |
|         - | 7377 | `				"TypeError",` |
|         - | 7378 | `				"array_map(): Argument #1 ($callback) must be a valid callback or null, "` |
|         - | 7379 | `				"function \"%s\" not found or invalid function name",` |
|         1 | 7380 | `				zFunc` |
|         - | 7381 | `				);` |
|         - | 7382 | `		}` |
|         3 | 7383 | `		return PH7_VmThrowException(pCtx,` |
|         - | 7384 | `			"TypeError",` |
|         - | 7385 | `			"array_map(): Argument #1 ($callback) must be a valid callback or null, "` |
|         - | 7386 | `			"no array or string given"` |
|         - | 7387 | `			);` |
|         - | 7388 | `	}` |
|         - | 7389 | `	/* Every remaining argument must be an array */` |
|       109 | 7390 | `	for( i = 1 ; i < nArg ; i++ ){` |
|        63 | 7391 | `		if( !ph7_value_is_array(apArg[i]) ){` |
|         3 | 7392 | `			if( i == 1 ){` |
|         4 | 7393 | `				return PH7_VmThrowException(pCtx,` |
|         - | 7394 | `					"TypeError",` |
|         - | 7395 | `					"array_map(): Argument #2 ($array) must be of type array, %s given",` |
|         2 | 7396 | `					ph7_type_name(apArg[1])` |
|         - | 7397 | `					);` |
|         - | 7398 | `			}` |
|       ! 0 | 7399 | `			return PH7_VmThrowException(pCtx,` |
|         - | 7400 | `				"TypeError",` |
|         - | 7401 | `				"array_map(): Argument #%d must be of type array, %s given",` |
|       ! 0 | 7402 | `				i+1,ph7_type_name(apArg[i])` |
|         - | 7403 | `				);` |
|         - | 7404 | `		}` |
|        31 | 7405 | `	}` |
|        48 | 7406 | `	pVm = pCtx->pVm;` |
|         - | 7407 | `	/* Create a new array */` |
|        48 | 7408 | `	pArray = ph7_context_new_array(pCtx);` |
|        48 | 7409 | `	if( pArray == 0 ){` |
|       ! 0 | 7410 | `		ph7_result_null(pCtx);` |
|       ! 0 | 7411 | `		return PH7_OK;` |
|         - | 7412 | `	}` |
|        48 | 7413 | `	PH7_MemObjInit(pVm,&sResult);` |
|        48 | 7414 | `	PH7_MemObjInit(pVm,&sKey);` |
|        48 | 7415 | `	sResult.nIdx = SXU32_HIGH; /* Mark as constant */` |
|        48 | 7416 | `	sKey.nIdx    = SXU32_HIGH; /* Mark as constant */` |
|        48 | 7417 | `	if( nArg == 2 ){` |
|         - | 7418 | `		/* Single-array mode: keys are preserved (PHP semantics). */` |
|        38 | 7419 | `		pMap = (ph7_hashmap *)apArg[1]->x.pOther;` |
|        38 | 7420 | `		pEntry = pMap->pFirst;` |
|       112 | 7421 | `		for( n = 0 ; n < pMap->nEntry ; n++ ){` |
|         - | 7422 | `			/* Extract the node value */` |
|        80 | 7423 | `			pValue = HashmapExtractNodeValue(pEntry);` |
|        80 | 7424 | `			if( pValue ){` |
|         - | 7425 | `				/* Extract the node key */` |
|        80 | 7426 | `				PH7_HashmapExtractNodeKey(pEntry,&sKey);` |
|        80 | 7427 | `				if( bNullCallback ){` |
|         - | 7428 | `					/* NULL callback: identity function, keep original value */` |
|        11 | 7429 | `					ph7_array_add_elem(pArray,&sKey,pValue);` |
|         6 | 7430 | `				}else{` |
|         - | 7431 | `					/* Invoke the supplied callback */` |
|        70 | 7432 | `					rc = PH7_VmCallUserFunction(pVm,apArg[0],1,&pValue,&sResult);` |
|        70 | 7433 | `					if( rc == PH7_EXCEPTION ){` |
|         - | 7434 | `						/* Callback raised: abort and let the foreign-function` |
|         - | 7435 | `						 * dispatcher unwind through the nearest try/catch. */` |
|         5 | 7436 | `						PH7_MemObjRelease(&sKey);` |
|         5 | 7437 | `						PH7_MemObjRelease(&sResult);` |
|         5 | 7438 | `						return PH7_EXCEPTION;` |
|         - | 7439 | `					}` |
|         - | 7440 | `					/* Insert the callback return value */` |
|        66 | 7441 | `					ph7_array_add_elem(pArray,&sKey,&sResult);` |
|         - | 7442 | `				}` |
|        76 | 7443 | `				PH7_MemObjRelease(&sKey);` |
|        76 | 7444 | `				PH7_MemObjRelease(&sResult);` |
|        37 | 7445 | `			}` |
|         - | 7446 | `			/* Point to the next entry */` |
|        76 | 7447 | `			pEntry = pEntry->pPrev; /* Reverse link */` |
|        39 | 7448 | `		}` |
|        18 | 7449 | `	}else{` |
|         - | 7450 | `		/* Multi-array mode: walk every array in parallel to the length of the` |
|         - | 7451 | `		 * longest one, pad shorter arrays with NULL, and re-index the result. */` |
|        11 | 7452 | `		int nArrays = nArg - 1;` |
|         - | 7453 | `		ph7_hashmap_node **apCur;` |
|         - | 7454 | `		ph7_value **apCallArg;` |
|         - | 7455 | `		ph7_value sNull;` |
|        11 | 7456 | `		sxu32 nMax = 0;` |
|        11 | 7457 | `		apCur     = (ph7_hashmap_node **)SyMemBackendAlloc(&pVm->sAllocator,(sxu32)(nArrays*sizeof(ph7_hashmap_node *)));` |
|        11 | 7458 | `		apCallArg = (ph7_value **)SyMemBackendAlloc(&pVm->sAllocator,(sxu32)(nArrays*sizeof(ph7_value *)));` |
|        11 | 7459 | `		if( apCur == 0 \|\| apCallArg == 0 ){` |
|       ! 0 | 7460 | `			if( apCur ){ SyMemBackendFree(&pVm->sAllocator,apCur); }` |
|       ! 0 | 7461 | `			if( apCallArg ){ SyMemBackendFree(&pVm->sAllocator,apCallArg); }` |
|       ! 0 | 7462 | `			PH7_MemObjRelease(&sKey);` |
|       ! 0 | 7463 | `			PH7_MemObjRelease(&sResult);` |
|       ! 0 | 7464 | `			ph7_result_value(pCtx,pArray);` |
|       ! 0 | 7465 | `			return PH7_OK;` |
|         - | 7466 | `		}` |
|        11 | 7467 | `		PH7_MemObjInit(pVm,&sNull); /* shared NULL pad for short arrays */` |
|        11 | 7468 | `		sNull.nIdx = SXU32_HIGH;` |
|        33 | 7469 | `		for( i = 0 ; i < nArrays ; i++ ){` |
|        23 | 7470 | `			pMap = (ph7_hashmap *)apArg[i+1]->x.pOther;` |
|        23 | 7471 | `			apCur[i] = pMap->pFirst;` |
|        23 | 7472 | `			if( pMap->nEntry > nMax ){` |
|        13 | 7473 | `				nMax = pMap->nEntry;` |
|         6 | 7474 | `			}` |
|        12 | 7475 | `		}` |
|        35 | 7476 | `		for( n = 0 ; n < nMax ; n++ ){` |
|        25 | 7477 | `			ph7_value *pZip = 0;` |
|        25 | 7478 | `			if( bNullCallback ){` |
|         - | 7479 | `				/* zip: each result element is an array of the i-th values */` |
|         5 | 7480 | `				pZip = ph7_context_new_array(pCtx);` |
|         2 | 7481 | `			}` |
|        79 | 7482 | `			for( i = 0 ; i < nArrays ; i++ ){` |
|        55 | 7483 | `				ph7_value *pv = &sNull;` |
|        55 | 7484 | `				if( apCur[i] ){` |
|        53 | 7485 | `					ph7_value *pNodeVal = HashmapExtractNodeValue(apCur[i]);` |
|        53 | 7486 | `					if( pNodeVal ){` |
|        53 | 7487 | `						pv = pNodeVal;` |
|        26 | 7488 | `					}` |
|        53 | 7489 | `					apCur[i] = apCur[i]->pPrev; /* Reverse link */` |
|        26 | 7490 | `				}` |
|        55 | 7491 | `				if( bNullCallback ){` |
|         9 | 7492 | `					if( pZip ){` |
|         9 | 7493 | `						ph7_array_add_elem(pZip,0,pv);` |
|         4 | 7494 | `					}` |
|         5 | 7495 | `				}else{` |
|        47 | 7496 | `					apCallArg[i] = pv;` |
|         - | 7497 | `				}` |
|        28 | 7498 | `			}` |
|        25 | 7499 | `			if( bNullCallback ){` |
|         5 | 7500 | `				if( pZip ){` |
|         5 | 7501 | `					ph7_array_add_elem(pArray,0,pZip);` |
|         2 | 7502 | `				}` |
|         3 | 7503 | `			}else{` |
|        21 | 7504 | `				rc = PH7_VmCallUserFunction(pVm,apArg[0],nArrays,apCallArg,&sResult);` |
|        21 | 7505 | `				if( rc == PH7_EXCEPTION ){` |
|       ! 0 | 7506 | `					SyMemBackendFree(&pVm->sAllocator,apCur);` |
|       ! 0 | 7507 | `					SyMemBackendFree(&pVm->sAllocator,apCallArg);` |
|       ! 0 | 7508 | `					PH7_MemObjRelease(&sNull);` |
|       ! 0 | 7509 | `					PH7_MemObjRelease(&sKey);` |
|       ! 0 | 7510 | `					PH7_MemObjRelease(&sResult);` |
|       ! 0 | 7511 | `					return PH7_EXCEPTION;` |
|         - | 7512 | `				}` |
|        21 | 7513 | `				ph7_array_add_elem(pArray,0,&sResult);` |
|        21 | 7514 | `				PH7_MemObjRelease(&sResult);` |
|         - | 7515 | `			}` |
|        13 | 7516 | `		}` |
|        11 | 7517 | `		SyMemBackendFree(&pVm->sAllocator,apCur);` |
|        11 | 7518 | `		SyMemBackendFree(&pVm->sAllocator,apCallArg);` |
|        11 | 7519 | `		PH7_MemObjRelease(&sNull);` |
|         - | 7520 | `	}` |
|        44 | 7521 | `	PH7_MemObjRelease(&sKey);` |
|        44 | 7522 | `	PH7_MemObjRelease(&sResult);` |
|        44 | 7523 | `	ph7_result_value(pCtx,pArray);` |
|        44 | 7524 | `	return PH7_OK;` |
|        33 | 7525 | `}` |
|         - | 7526 | `/*` |
|         - | 7527 | ` * value array_reduce(array $array, callable $callback[, value $initial = NULL])` |
|         - | 7528 | ` *  Iteratively reduce the array to a single value using a callback function.` |
|         - | 7529 | ` * Parameters` |
|         - | 7530 | ` *  $array` |
|         - | 7531 | ` *   The input array.` |
|         - | 7532 | ` *  $callback` |
|         - | 7533 | ` *   The callback function. Signature: callback(mixed $carry, mixed $item): mixed` |
|         - | 7534 | ` *  $initial` |
|         - | 7535 | ` *   If the optional initial is available, it will be used at the beginning` |
|         - | 7536 | ` *   of the process, or as a final result in case the array is empty.` |
|         - | 7537 | ` * Return` |
|         - | 7538 | ` *  Returns the resulting value.` |
|         - | 7539 | ` *  If the array is empty and initial is not passed, array_reduce() returns NULL.` |
|         - | 7540 | ` */` |
|        34 | 7541 | `static int ph7_hashmap_reduce(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|         5 | 7542 | `{` |
|         - | 7543 | `	ph7_hashmap_node *pEntry;` |
|         - | 7544 | `	ph7_hashmap *pMap;` |
|         - | 7545 | `	ph7_value *pValue;` |
|         - | 7546 | `	ph7_value sResult;` |
|         - | 7547 | `	sxi32 rc;` |
|         - | 7548 | `	sxu32 n;` |
|        39 | 7549 | `	if( nArg < 2 ){` |
|         8 | 7550 | `		return PH7_VmThrowException(pCtx,` |
|         - | 7551 | `			"ArgumentCountError",` |
|         - | 7552 | `			"array_reduce() expects at least 2 arguments, %d given",` |
|         2 | 7553 | `			nArg` |
|         - | 7554 | `			);` |
|         - | 7555 | `	}` |
|        35 | 7556 | `	if( nArg > 3 ){` |
|         4 | 7557 | `		return PH7_VmThrowException(pCtx,` |
|         - | 7558 | `			"ArgumentCountError",` |
|         - | 7559 | `			"array_reduce() expects at most 3 arguments, %d given",` |
|         1 | 7560 | `			nArg` |
|         - | 7561 | `			);` |
|         - | 7562 | `	}` |
|        33 | 7563 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|         4 | 7564 | `		return PH7_VmThrowException(pCtx,` |
|         - | 7565 | `			"TypeError",` |
|         - | 7566 | `			"array_reduce(): Argument #1 ($array) must be of type array, %s given",` |
|         1 | 7567 | `			ph7_type_name(apArg[0])` |
|         - | 7568 | `			);` |
|         - | 7569 | `	}` |
|        31 | 7570 | `	if( !ph7_value_is_callable(apArg[1]) ){` |
|        12 | 7571 | `		if( ph7_value_is_string(apArg[1]) ){` |
|         3 | 7572 | `			const char *zFunc = ph7_value_to_string(apArg[1],0);` |
|         4 | 7573 | `			return PH7_VmThrowException(pCtx,` |
|         - | 7574 | `				"TypeError",` |
|         - | 7575 | `				"array_reduce(): Argument #2 ($callback) must be a valid callback, "` |
|         - | 7576 | `				"function \"%s\" not found or invalid function name",` |
|         1 | 7577 | `				zFunc` |
|         - | 7578 | `				);` |
|         - | 7579 | `		}` |
|         9 | 7580 | `		if( ph7_value_is_array(apArg[1]) ){` |
|         3 | 7581 | `			return PH7_VmThrowException(pCtx,` |
|         - | 7582 | `				"TypeError",` |
|         - | 7583 | `				"array_reduce(): Argument #2 ($callback) must be a valid callback, "` |
|         - | 7584 | `				"array callback must have exactly two members"` |
|         - | 7585 | `				);` |
|         - | 7586 | `		}` |
|         6 | 7587 | `		return PH7_VmThrowException(pCtx,` |
|         - | 7588 | `			"TypeError",` |
|         - | 7589 | `			"array_reduce(): Argument #2 ($callback) must be a valid callback, "` |
|         - | 7590 | `			"no array or string given"` |
|         - | 7591 | `			);` |
|         - | 7592 | `	}` |
|         - | 7593 | `	/* Point to the internal representation of the input hashmap */` |
|        19 | 7594 | `	pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|         - | 7595 | `	/* Assume a NULL initial value */` |
|        19 | 7596 | `	PH7_MemObjInit(pMap->pVm,&sResult);` |
|        19 | 7597 | `	sResult.nIdx = SXU32_HIGH; /* Mark as constant */` |
|        19 | 7598 | `	if( nArg > 2 ){` |
|         - | 7599 | `		/* Set the initial value */` |
|        13 | 7600 | `		PH7_MemObjLoad(apArg[2],&sResult);` |
|         6 | 7601 | `	}` |
|         - | 7602 | `	/* Perform the requested operation */` |
|        19 | 7603 | `	pEntry = pMap->pFirst;` |
|        55 | 7604 | `	for( n = 0 ; n < pMap->nEntry ; n++ ){` |
|         - | 7605 | `		/* Extract the node value */` |
|        39 | 7606 | `		pValue = HashmapExtractNodeValue(pEntry);` |
|         - | 7607 | `		/* Invoke the supplied callback */` |
|        39 | 7608 | `		rc = PH7_VmCallUserFunctionAp(pMap->pVm,apArg[1],&sResult,&sResult,pValue,0);` |
|        39 | 7609 | `		if( rc == PH7_EXCEPTION ){` |
|         - | 7610 | `			/* The callback raised: propagate so the dispatcher unwinds. */` |
|         3 | 7611 | `			PH7_MemObjRelease(&sResult);` |
|         3 | 7612 | `			return PH7_EXCEPTION;` |
|         - | 7613 | `		}` |
|         - | 7614 | `		/* Point to the next entry */` |
|        37 | 7615 | `		pEntry = pEntry->pPrev; /* Reverse link */` |
|        19 | 7616 | `	}` |
|        17 | 7617 | `	ph7_result_value(pCtx,&sResult); /* Will make it's own copy */` |
|        17 | 7618 | `	PH7_MemObjRelease(&sResult);` |
|        17 | 7619 | `	return PH7_OK;` |
|        22 | 7620 | `}` |
|         - | 7621 | `/*` |
|         - | 7622 | ` * bool array_walk(array &$array, callback $funcname [, mixed $userdata])` |
|         - | 7623 | ` *  Apply a user function to every member of an array.` |
|         - | 7624 | ` * Parameters` |
|         - | 7625 | ` *  $array` |
|         - | 7626 | ` *   The input array.` |
|         - | 7627 | ` *  $funcname` |
|         - | 7628 | ` *   Typically, funcname takes on two parameters. The array parameter's value being` |
|         - | 7629 | ` *   the first, and the key/index second.` |
|         - | 7630 | ` * Note:` |
|         - | 7631 | ` *  If funcname needs to be working with the actual values of the array, specify the first` |
|         - | 7632 | ` *  parameter of funcname as a reference. Then, any changes made to those elements will` |
|         - | 7633 | ` *  be made in the original array itself.` |
|         - | 7634 | ` *  $userdata` |
|         - | 7635 | ` *   If the optional userdata parameter is supplied, it will be passed as the third parameter` |
|         - | 7636 | ` *   to the callback funcname.` |
|         - | 7637 | ` * Return` |
|         - | 7638 | ` *  Returns TRUE on success or FALSE on failure.` |
|         - | 7639 | ` */` |
|        38 | 7640 | `static int ph7_hashmap_walk(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|         5 | 7641 | `{` |
|         - | 7642 | `	ph7_value *pValue,*pUserData,sKey;` |
|         - | 7643 | `	ph7_hashmap_node *pEntry;` |
|         - | 7644 | `	ph7_hashmap *pMap;` |
|         - | 7645 | `	sxu32 n;` |
|        43 | 7646 | `	if( nArg < 2 ){` |
|         8 | 7647 | `		return PH7_VmThrowException(pCtx,` |
|         - | 7648 | `			"ArgumentCountError",` |
|         - | 7649 | `			"array_walk() expects at least 2 arguments, %d given",` |
|         2 | 7650 | `			nArg` |
|         - | 7651 | `			);` |
|         - | 7652 | `	}` |
|        39 | 7653 | `	if( nArg > 3 ){` |
|         4 | 7654 | `		return PH7_VmThrowException(pCtx,` |
|         - | 7655 | `			"ArgumentCountError",` |
|         - | 7656 | `			"array_walk() expects at most 3 arguments, %d given",` |
|         1 | 7657 | `			nArg` |
|         - | 7658 | `			);` |
|         - | 7659 | `	}` |
|        37 | 7660 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|         4 | 7661 | `		return PH7_VmThrowException(pCtx,` |
|         - | 7662 | `			"TypeError",` |
|         - | 7663 | `			"array_walk(): Argument #1 ($array) must be of type array, %s given",` |
|         1 | 7664 | `			ph7_type_name(apArg[0])` |
|         - | 7665 | `			);` |
|         - | 7666 | `	}` |
|        35 | 7667 | `	if( !ph7_value_is_callable(apArg[1]) ){` |
|        14 | 7668 | `		if( ph7_value_is_string(apArg[1]) ){` |
|         3 | 7669 | `			const char *zFunc = ph7_value_to_string(apArg[1],0);` |
|         4 | 7670 | `			return PH7_VmThrowException(pCtx,` |
|         - | 7671 | `				"TypeError",` |
|         - | 7672 | `				"array_walk(): Argument #2 ($callback) must be a valid callback, "` |
|         - | 7673 | `				"function \"%s\" not found or invalid function name",` |
|         1 | 7674 | `				zFunc` |
|         - | 7675 | `				);` |
|         - | 7676 | `		}` |
|        12 | 7677 | `		if( ph7_value_is_array(apArg[1]) ){` |
|         6 | 7678 | `			return PH7_VmThrowException(pCtx,` |
|         - | 7679 | `				"TypeError",` |
|         - | 7680 | `				"array_walk(): Argument #2 ($callback) must be a valid callback, "` |
|         - | 7681 | `				"array callback must have exactly two members"` |
|         - | 7682 | `				);` |
|         - | 7683 | `		}` |
|         6 | 7684 | `		return PH7_VmThrowException(pCtx,` |
|         - | 7685 | `			"TypeError",` |
|         - | 7686 | `			"array_walk(): Argument #2 ($callback) must be a valid callback, "` |
|         - | 7687 | `			"no array or string given"` |
|         - | 7688 | `			);` |
|         - | 7689 | `	}` |
|        21 | 7690 | `	pUserData = nArg > 2 ? apArg[2] : 0;` |
|         - | 7691 | `	/* Point to the internal representation of the input hashmap */` |
|        21 | 7692 | `	PH7_HashmapCowSeparate(pCtx->pVm, apArg[0]);` |
|        21 | 7693 | `	pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|        21 | 7694 | `	PH7_MemObjInit(pMap->pVm,&sKey);` |
|        21 | 7695 | `	sKey.nIdx = SXU32_HIGH; /* Mark as constant */` |
|         - | 7696 | `	/* Perform the desired operation */` |
|        21 | 7697 | `	pEntry = pMap->pFirst;` |
|        61 | 7698 | `	for( n = 0 ; n < pMap->nEntry ; n++ ){` |
|         - | 7699 | `		/* Extract the node value */` |
|        43 | 7700 | `		pValue = HashmapExtractNodeValue(pEntry);` |
|        43 | 7701 | `		if( pValue ){` |
|         - | 7702 | `			sxi32 rcW;` |
|         - | 7703 | `			/* Extract the entry key */` |
|        43 | 7704 | `			PH7_HashmapExtractNodeKey(pEntry,&sKey);` |
|         - | 7705 | `			/* Invoke the supplied callback */` |
|        43 | 7706 | `			rcW = PH7_VmCallUserFunctionAp(pMap->pVm,apArg[1],0,pValue,&sKey,pUserData,0);` |
|        43 | 7707 | `			PH7_MemObjRelease(&sKey);` |
|        43 | 7708 | `			if( rcW == PH7_EXCEPTION ){` |
|         - | 7709 | `				/* The callback raised: propagate so the dispatcher unwinds. */` |
|         3 | 7710 | `				return PH7_EXCEPTION;` |
|         - | 7711 | `			}` |
|        20 | 7712 | `		}` |
|         - | 7713 | `		/* Point to the next entry */` |
|        41 | 7714 | `		pEntry = pEntry->pPrev; /* Reverse link */` |
|        21 | 7715 | `	}` |
|         - | 7716 | `	/* All done, return TRUE */` |
|        19 | 7717 | `	ph7_result_bool(pCtx,1);` |
|        19 | 7718 | `	return PH7_OK;` |
|        24 | 7719 | `}` |
|         - | 7720 | `/*` |
|         - | 7721 | ` * Apply a user function to every member of an array.(Recurse on array's).` |
|         - | 7722 | ` * Refer to the [array_walk_recursive()] implementation for more information.` |
|         - | 7723 | ` */` |
|        22 | 7724 | `static sxi32 HashmapWalkRecursive(` |
|         - | 7725 | `	ph7_hashmap *pMap,    /* Target hashmap */` |
|         - | 7726 | `	ph7_value *pCallback, /* User callback */` |
|         - | 7727 | `	ph7_value *pUserData, /* Callback private data */` |
|         - | 7728 | `	int iNest             /* Nesting level */` |
|         - | 7729 | `	)` |
|         1 | 7730 | `{` |
|         - | 7731 | `	ph7_hashmap_node *pEntry;` |
|         - | 7732 | `	ph7_value *pValue,sKey;` |
|         - | 7733 | `	sxi32 rc;` |
|         - | 7734 | `	sxu32 n;` |
|         - | 7735 | `	/* Iterate through hashmap entries */` |
|        23 | 7736 | `	PH7_MemObjInit(pMap->pVm,&sKey);` |
|        23 | 7737 | `	sKey.nIdx = SXU32_HIGH; /* Mark as constant */` |
|        23 | 7738 | `	pEntry = pMap->pFirst;` |
|        59 | 7739 | `	for( n = 0 ; n < pMap->nEntry ; n++ ){` |
|         - | 7740 | `		/* Extract the node value */` |
|        37 | 7741 | `		pValue = HashmapExtractNodeValue(pEntry);` |
|        37 | 7742 | `		if( pValue ){` |
|        37 | 7743 | `			if( pValue->iFlags & MEMOBJ_HASHMAP ){` |
|        11 | 7744 | `				if( iNest < 32 ){` |
|         - | 7745 | `					/* Recurse */` |
|        11 | 7746 | `					iNest++;` |
|        11 | 7747 | `					rc = HashmapWalkRecursive((ph7_hashmap *)pValue->x.pOther,pCallback,pUserData,iNest);` |
|        11 | 7748 | `					iNest--;` |
|        11 | 7749 | `					if( rc == PH7_EXCEPTION ){` |
|       ! 0 | 7750 | `						return PH7_EXCEPTION;` |
|         - | 7751 | `					}` |
|         5 | 7752 | `				}` |
|         6 | 7753 | `			}else{` |
|         - | 7754 | `				/* Extract the node key */` |
|        27 | 7755 | `				PH7_HashmapExtractNodeKey(pEntry,&sKey);` |
|         - | 7756 | `				/* Invoke the supplied callback */` |
|        27 | 7757 | `				rc = PH7_VmCallUserFunctionAp(pMap->pVm,pCallback,0,pValue,&sKey,pUserData,0);` |
|        27 | 7758 | `				PH7_MemObjRelease(&sKey);` |
|        27 | 7759 | `				if( rc == PH7_EXCEPTION ){` |
|         - | 7760 | `					/* The callback raised: propagate so the dispatcher unwinds. */` |
|       ! 0 | 7761 | `					return PH7_EXCEPTION;` |
|         - | 7762 | `				}` |
|         - | 7763 | `			}` |
|        18 | 7764 | `		}` |
|         - | 7765 | `		/* Point to the next entry */` |
|        37 | 7766 | `		pEntry = pEntry->pPrev; /* Reverse link */` |
|        19 | 7767 | `	}` |
|        23 | 7768 | `	return PH7_OK;` |
|        12 | 7769 | `}` |
|         - | 7770 | `/*` |
|         - | 7771 | ` * bool array_walk_recursive(array &$array, callback $funcname [, mixed $userdata])` |
|         - | 7772 | ` *  Apply a user function recursively to every member of an array.` |
|         - | 7773 | ` * Parameters` |
|         - | 7774 | ` *  $array` |
|         - | 7775 | ` *   The input array.` |
|         - | 7776 | ` *  $funcname` |
|         - | 7777 | ` *   Typically, funcname takes on two parameters. The array parameter's value being` |
|         - | 7778 | ` *   the first, and the key/index second.` |
|         - | 7779 | ` * Note:` |
|         - | 7780 | ` *  If funcname needs to be working with the actual values of the array, specify the first` |
|         - | 7781 | ` *  parameter of funcname as a reference. Then, any changes made to those elements will` |
|         - | 7782 | ` *  be made in the original array itself.` |
|         - | 7783 | ` *  $userdata` |
|         - | 7784 | ` *   If the optional userdata parameter is supplied, it will be passed as the third parameter` |
|         - | 7785 | ` *   to the callback funcname.` |
|         - | 7786 | ` * Return` |
|         - | 7787 | ` *  Returns TRUE on success or FALSE on failure.` |
|         - | 7788 | ` */` |
|        30 | 7789 | `static int ph7_hashmap_walk_recursive(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|         5 | 7790 | `{` |
|         - | 7791 | `	ph7_hashmap *pMap;` |
|        35 | 7792 | `	if( nArg < 2 ){` |
|         8 | 7793 | `		return PH7_VmThrowException(pCtx,` |
|         - | 7794 | `			"ArgumentCountError",` |
|         - | 7795 | `			"array_walk_recursive() expects at least 2 arguments, %d given",` |
|         2 | 7796 | `			nArg` |
|         - | 7797 | `			);` |
|         - | 7798 | `	}` |
|        31 | 7799 | `	if( nArg > 3 ){` |
|         4 | 7800 | `		return PH7_VmThrowException(pCtx,` |
|         - | 7801 | `			"ArgumentCountError",` |
|         - | 7802 | `			"array_walk_recursive() expects at most 3 arguments, %d given",` |
|         1 | 7803 | `			nArg` |
|         - | 7804 | `			);` |
|         - | 7805 | `	}` |
|        29 | 7806 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|         4 | 7807 | `		return PH7_VmThrowException(pCtx,` |
|         - | 7808 | `			"TypeError",` |
|         - | 7809 | `			"array_walk_recursive(): Argument #1 ($array) must be of type array, %s given",` |
|         1 | 7810 | `			ph7_type_name(apArg[0])` |
|         - | 7811 | `			);` |
|         - | 7812 | `	}` |
|        27 | 7813 | `	if( !ph7_value_is_callable(apArg[1]) ){` |
|        14 | 7814 | `		if( ph7_value_is_string(apArg[1]) ){` |
|         3 | 7815 | `			const char *zFunc = ph7_value_to_string(apArg[1],0);` |
|         4 | 7816 | `			return PH7_VmThrowException(pCtx,` |
|         - | 7817 | `				"TypeError",` |
|         - | 7818 | `				"array_walk_recursive(): Argument #2 ($callback) must be a valid callback, "` |
|         - | 7819 | `				"function \"%s\" not found or invalid function name",` |
|         1 | 7820 | `				zFunc` |
|         - | 7821 | `				);` |
|         - | 7822 | `		}` |
|        12 | 7823 | `		if( ph7_value_is_array(apArg[1]) ){` |
|         6 | 7824 | `			return PH7_VmThrowException(pCtx,` |
|         - | 7825 | `				"TypeError",` |
|         - | 7826 | `				"array_walk_recursive(): Argument #2 ($callback) must be a valid callback, "` |
|         - | 7827 | `				"array callback must have exactly two members"` |
|         - | 7828 | `				);` |
|         - | 7829 | `		}` |
|         6 | 7830 | `		return PH7_VmThrowException(pCtx,` |
|         - | 7831 | `			"TypeError",` |
|         - | 7832 | `			"array_walk_recursive(): Argument #2 ($callback) must be a valid callback, "` |
|         - | 7833 | `			"no array or string given"` |
|         - | 7834 | `			);` |
|         - | 7835 | `	}` |
|         - | 7836 | `	/* Point to the internal representation of the input hashmap */` |
|        13 | 7837 | `	PH7_HashmapCowSeparate(pCtx->pVm, apArg[0]);` |
|        13 | 7838 | `	pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|         - | 7839 | `	/* Perform the desired operation */` |
|        13 | 7840 | `	if( HashmapWalkRecursive(pMap,apArg[1],nArg > 2 ? apArg[2] : 0,0) == PH7_EXCEPTION ){` |
|         - | 7841 | `		/* A callback raised: propagate so the dispatcher unwinds. */` |
|       ! 0 | 7842 | `		return PH7_EXCEPTION;` |
|         - | 7843 | `	}` |
|         - | 7844 | `	/* All done, return TRUE */` |
|        13 | 7845 | `	ph7_result_bool(pCtx,1);` |
|        13 | 7846 | `	return PH7_OK;` |
|        20 | 7847 | `}` |
|         - | 7848 | `/*` |
|         - | 7849 | ` * bool array_is_list(array $array)` |
|         - | 7850 | ` *  Checks whether a given array is a list: its keys consist of consecutive` |
|         - | 7851 | ` *  integers starting at 0. An empty array is a list.` |
|         - | 7852 | ` * Return` |
|         - | 7853 | ` *  TRUE if the array is a list, FALSE otherwise.` |
|         - | 7854 | ` */` |
|         - | 7855 | `/*` |
|         - | 7856 | ` * Return TRUE if the given hashmap is a "list" [i.e: its keys are the` |
|         - | 7857 | ` * consecutive integers 0,1,2,... with no gaps]. An empty map is a list.` |
|         - | 7858 | ` * Shared by array_is_list() and the JSON encoder (vm_json.c).` |
|         - | 7859 | ` */` |
|       170 | 7860 | `PH7_PRIVATE int PH7_HashmapIsList(ph7_hashmap *pMap)` |
|         1 | 7861 | `{` |
|       171 | 7862 | `	ph7_hashmap_node *pNode = pMap->pFirst;` |
|       171 | 7863 | `	sxi64 iExpect = 0;` |
|         - | 7864 | `	sxu32 n;` |
|       327 | 7865 | `	for( n = 0 ; n < pMap->nEntry ; ++n ){` |
|       235 | 7866 | `		if( pNode->iType != HASHMAP_INT_NODE \|\| pNode->xKey.iKey != iExpect ){` |
|         - | 7867 | `			/* A non-integer key or a gap in the sequence: not a list */` |
|        79 | 7868 | `			return 0;` |
|         - | 7869 | `		}` |
|       157 | 7870 | `		++iExpect;` |
|       157 | 7871 | `		pNode = pNode->pPrev; /* Reverse link */` |
|        79 | 7872 | `	}` |
|        93 | 7873 | `	return 1;` |
|        86 | 7874 | `}` |
|        12 | 7875 | `static int ph7_hashmap_is_list(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|         1 | 7876 | `{` |
|        13 | 7877 | `	if( nArg < 1 ){` |
|       ! 0 | 7878 | `		return PH7_VmThrowException(pCtx,` |
|         - | 7879 | `			"ArgumentCountError",` |
|         - | 7880 | `			"array_is_list() expects exactly 1 argument, 0 given"` |
|         - | 7881 | `			);` |
|         - | 7882 | `	}` |
|        13 | 7883 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|       ! 0 | 7884 | `		return PH7_VmThrowException(pCtx,` |
|         - | 7885 | `			"TypeError",` |
|         - | 7886 | `			"array_is_list(): Argument #1 ($array) must be of type array, %s given",` |
|       ! 0 | 7887 | `			ph7_type_name(apArg[0])` |
|         - | 7888 | `			);` |
|         - | 7889 | `	}` |
|        13 | 7890 | `	ph7_result_bool(pCtx,PH7_HashmapIsList((ph7_hashmap *)apArg[0]->x.pOther));` |
|        13 | 7891 | `	return PH7_OK;` |
|         7 | 7892 | `}` |
|         - | 7893 | `/*` |
|         - | 7894 | ` * mixed array_first(array $array)` |
|         - | 7895 | ` * mixed array_last(array $array)` |
|         - | 7896 | ` *  Return the value of the first (respectively last) element of the array,` |
|         - | 7897 | ` *  or NULL when the array is empty. The internal array pointer is left` |
|         - | 7898 | ` *  untouched (unlike reset()/end()).` |
|         - | 7899 | ` */` |
|        20 | 7900 | `static int HashmapFirstLast(ph7_context *pCtx,int nArg,ph7_value **apArg,int bLast)` |
|         1 | 7901 | `{` |
|         - | 7902 | `	ph7_hashmap *pMap;` |
|         - | 7903 | `	ph7_hashmap_node *pNode;` |
|         - | 7904 | `	ph7_value *pVal;` |
|        21 | 7905 | `	const char *zName = bLast ? "array_last" : "array_first";` |
|        21 | 7906 | `	if( nArg < 1 ){` |
|         4 | 7907 | `		return PH7_VmThrowException(pCtx,` |
|         - | 7908 | `			"ArgumentCountError",` |
|         - | 7909 | `			"%s() expects exactly 1 argument, 0 given",` |
|         1 | 7910 | `			zName` |
|         - | 7911 | `			);` |
|         - | 7912 | `	}` |
|        19 | 7913 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|         4 | 7914 | `		return PH7_VmThrowException(pCtx,` |
|         - | 7915 | `			"TypeError",` |
|         - | 7916 | `			"%s(): Argument #1 ($array) must be of type array, %s given",` |
|         1 | 7917 | `			zName,` |
|         1 | 7918 | `			ph7_type_name(apArg[0])` |
|         - | 7919 | `			);` |
|         - | 7920 | `	}` |
|        17 | 7921 | `	pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|        17 | 7922 | `	pNode = bLast ? pMap->pLast : pMap->pFirst;` |
|        17 | 7923 | `	if( pNode == 0 ){` |
|         - | 7924 | `		/* Empty array: PHP returns NULL */` |
|         5 | 7925 | `		ph7_result_null(pCtx);` |
|         5 | 7926 | `		return PH7_OK;` |
|         - | 7927 | `	}` |
|        13 | 7928 | `	pVal = HashmapExtractNodeValue(pNode);` |
|        13 | 7929 | `	if( pVal ){` |
|        13 | 7930 | `		ph7_result_value(pCtx,pVal);` |
|         7 | 7931 | `	}else{` |
|       ! 0 | 7932 | `		ph7_result_null(pCtx);` |
|         - | 7933 | `	}` |
|        13 | 7934 | `	return PH7_OK;` |
|        11 | 7935 | `}` |
|        10 | 7936 | `static int ph7_hashmap_first(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|         1 | 7937 | `{` |
|        11 | 7938 | `	return HashmapFirstLast(pCtx,nArg,apArg,0);` |
|         1 | 7939 | `}` |
|        10 | 7940 | `static int ph7_hashmap_last(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|         1 | 7941 | `{` |
|        11 | 7942 | `	return HashmapFirstLast(pCtx,nArg,apArg,1);` |
|         1 | 7943 | `}` |
|         - | 7944 | `/*` |
|         - | 7945 | ` * Fetch the element identified by 'pKey' from 'pRow' which may be either an` |
|         - | 7946 | ` * array (hashmap lookup) or an object (public attribute lookup). Used by` |
|         - | 7947 | ` * array_column() for both the column value and the index key.` |
|         - | 7948 | ` * Returns a borrowed pointer to the value, or NULL when the row is not a` |
|         - | 7949 | ` * container or the key is absent.` |
|         - | 7950 | ` */` |
|        32 | 7951 | `static ph7_value * HashmapColumnFetch(ph7_vm *pVm,ph7_value *pRow,ph7_value *pKey)` |
|         1 | 7952 | `{` |
|        33 | 7953 | `	if( ph7_value_is_array(pRow) ){` |
|         - | 7954 | `		ph7_hashmap_node *pNode;` |
|        25 | 7955 | `		if( PH7_HashmapLookup((ph7_hashmap *)pRow->x.pOther,pKey,&pNode) == SXRET_OK ){` |
|        21 | 7956 | `			return HashmapExtractNodeValue(pNode);` |
|         1 | 7957 | `		}` |
|        11 | 7958 | `	}else if( ph7_value_is_object(pRow) ){` |
|         - | 7959 | `		ph7_value sName;` |
|         - | 7960 | `		const char *zName;` |
|         - | 7961 | `		ph7_value *pAttr;` |
|         - | 7962 | `		/* Stringify a *copy* of the key (objects address attributes by name);` |
|         - | 7963 | `		 * never mutate pKey itself or the array-lookup path would break. */` |
|         9 | 7964 | `		PH7_MemObjInit(pVm,&sName);` |
|         9 | 7965 | `		PH7_MemObjStore(pKey,&sName);` |
|         9 | 7966 | `		zName = ph7_value_to_string(&sName,0); /* NUL-terminated */` |
|         9 | 7967 | `		pAttr = ph7_object_fetch_attr(pRow,zName);` |
|         9 | 7968 | `		PH7_MemObjRelease(&sName);` |
|         9 | 7969 | `		return pAttr;` |
|         - | 7970 | `	}` |
|         5 | 7971 | `	return 0;` |
|        17 | 7972 | `}` |
|         - | 7973 | `/*` |
|         - | 7974 | ` * array array_column(array $array, int\|string\|null $column_key, int\|string\|null $index_key = null)` |
|         - | 7975 | ` *  Returns the values from a single column of the input, identified by` |
|         - | 7976 | ` *  $column_key. Optionally indexes the result by the $index_key column.` |
|         - | 7977 | ` *  A NULL $column_key collects the whole row. Rows missing the column are` |
|         - | 7978 | ` *  skipped; rows missing the index key are appended with a numeric key.` |
|         - | 7979 | ` *  Each row may be an array or an object.` |
|         - | 7980 | ` */` |
|        12 | 7981 | `static int ph7_hashmap_column(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|         1 | 7982 | `{` |
|         - | 7983 | `	ph7_hashmap_node *pNode;` |
|         - | 7984 | `	ph7_hashmap *pMap;` |
|         - | 7985 | `	ph7_value *pArray;` |
|         - | 7986 | `	ph7_value *pRow;` |
|         - | 7987 | `	ph7_value *pCol;` |
|         - | 7988 | `	ph7_value *pIdx;` |
|         - | 7989 | `	int bWantCol;` |
|         - | 7990 | `	int bWantIdx;` |
|         - | 7991 | `	sxu32 n;` |
|        13 | 7992 | `	if( nArg < 2 ){` |
|       ! 0 | 7993 | `		return PH7_VmThrowException(pCtx,` |
|         - | 7994 | `			"ArgumentCountError",` |
|         - | 7995 | `			"array_column() expects at least 2 arguments, %d given",` |
|       ! 0 | 7996 | `			nArg` |
|         - | 7997 | `			);` |
|         - | 7998 | `	}` |
|        13 | 7999 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|       ! 0 | 8000 | `		return PH7_VmThrowException(pCtx,` |
|         - | 8001 | `			"TypeError",` |
|         - | 8002 | `			"array_column(): Argument #1 ($array) must be of type array, %s given",` |
|       ! 0 | 8003 | `			ph7_type_name(apArg[0])` |
|         - | 8004 | `			);` |
|         - | 8005 | `	}` |
|        13 | 8006 | `	pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|        13 | 8007 | `	pArray = ph7_context_new_array(pCtx);` |
|        13 | 8008 | `	if( pArray == 0 ){` |
|       ! 0 | 8009 | `		ph7_result_null(pCtx);` |
|       ! 0 | 8010 | `		return PH7_OK;` |
|         - | 8011 | `	}` |
|         - | 8012 | `	/* A NULL column_key means "collect the entire row". */` |
|        13 | 8013 | `	bWantCol = !ph7_value_is_null(apArg[1]);` |
|        13 | 8014 | `	bWantIdx = (nArg > 2 && !ph7_value_is_null(apArg[2]));` |
|        13 | 8015 | `	pNode = pMap->pFirst;` |
|        33 | 8016 | `	for( n = 0 ; n < pMap->nEntry ; ++n ){` |
|        21 | 8017 | `		pRow = HashmapExtractNodeValue(pNode);` |
|        21 | 8018 | `		pNode = pNode->pPrev; /* Advance now so 'continue' is safe */` |
|        21 | 8019 | `		if( pRow == 0 ){` |
|       ! 0 | 8020 | `			continue;` |
|         - | 8021 | `		}` |
|        21 | 8022 | `		if( bWantCol ){` |
|        19 | 8023 | `			pCol = HashmapColumnFetch(pMap->pVm,pRow,apArg[1]);` |
|        19 | 8024 | `			if( pCol == 0 ){` |
|         - | 8025 | `				/* Row lacks the requested column: skip it (PHP semantics). */` |
|         3 | 8026 | `				continue;` |
|         - | 8027 | `			}` |
|         9 | 8028 | `		}else{` |
|         3 | 8029 | `			pCol = pRow;` |
|         - | 8030 | `		}` |
|        19 | 8031 | `		pIdx = bWantIdx ? HashmapColumnFetch(pMap->pVm,pRow,apArg[2]) : 0;` |
|        19 | 8032 | `		if( pIdx ){` |
|        13 | 8033 | `			ph7_array_add_elem(pArray,pIdx,pCol);` |
|         7 | 8034 | `		}else{` |
|         7 | 8035 | `			ph7_array_add_elem(pArray,0,pCol); /* Auto-index */` |
|         - | 8036 | `		}` |
|        10 | 8037 | `	}` |
|        13 | 8038 | `	ph7_result_value(pCtx,pArray);` |
|        13 | 8039 | `	return PH7_OK;` |
|         7 | 8040 | `}` |
|         - | 8041 | `/*` |
|         - | 8042 | ` * Shared core for array_find/array_find_key/array_any/array_all (PHP 8.4).` |
|         - | 8043 | ` * Invokes $callback($value, $key) over each entry and reports the first node` |
|         - | 8044 | ` * whose truthiness equals 'bWant'. Propagates a callback exception as` |
|         - | 8045 | ` * PH7_EXCEPTION; sets *ppMatch to the matching node (or NULL if none).` |
|         - | 8046 | ` */` |
|        28 | 8047 | `static sxi32 HashmapCallbackSearch(` |
|         - | 8048 | `	ph7_context *pCtx,int nArg,ph7_value **apArg,` |
|         - | 8049 | `	const char *zName,            /* Function name for diagnostics */` |
|         - | 8050 | `	int bWant,                    /* Truthiness being hunted for */` |
|         - | 8051 | `	ph7_hashmap_node **ppMatch    /* OUT: first matching node or NULL */` |
|         - | 8052 | `	)` |
|         1 | 8053 | `{` |
|         - | 8054 | `	ph7_hashmap_node *pEntry;` |
|         - | 8055 | `	ph7_hashmap *pMap;` |
|         - | 8056 | `	ph7_value *pValue;` |
|         - | 8057 | `	ph7_value *apCbArg[2];` |
|         - | 8058 | `	ph7_value sKey;` |
|         - | 8059 | `	ph7_value sResult;` |
|         - | 8060 | `	sxi32 rc;` |
|         - | 8061 | `	sxu32 n;` |
|        29 | 8062 | `	*ppMatch = 0;` |
|        29 | 8063 | `	if( nArg < 2 ){` |
|       ! 0 | 8064 | `		return PH7_VmThrowException(pCtx,` |
|         - | 8065 | `			"ArgumentCountError",` |
|         - | 8066 | `			"%s() expects exactly 2 arguments, %d given",` |
|       ! 0 | 8067 | `			zName,nArg` |
|         - | 8068 | `			);` |
|         - | 8069 | `	}` |
|        29 | 8070 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|       ! 0 | 8071 | `		return PH7_VmThrowException(pCtx,` |
|         - | 8072 | `			"TypeError",` |
|         - | 8073 | `			"%s(): Argument #1 ($array) must be of type array, %s given",` |
|       ! 0 | 8074 | `			zName,ph7_type_name(apArg[0])` |
|         - | 8075 | `			);` |
|         - | 8076 | `	}` |
|        29 | 8077 | `	if( !ph7_value_is_callable(apArg[1]) ){` |
|       ! 0 | 8078 | `		return PH7_VmThrowException(pCtx,` |
|         - | 8079 | `			"TypeError",` |
|         - | 8080 | `			"%s(): Argument #2 ($callback) must be a valid callback, %s given",` |
|       ! 0 | 8081 | `			zName,ph7_type_name(apArg[1])` |
|         - | 8082 | `			);` |
|         - | 8083 | `	}` |
|        29 | 8084 | `	pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|        29 | 8085 | `	pEntry = pMap->pFirst;` |
|        29 | 8086 | `	PH7_MemObjInit(pMap->pVm,&sKey);` |
|        29 | 8087 | `	sKey.nIdx = SXU32_HIGH;    /* Mark as constant */` |
|        29 | 8088 | `	PH7_MemObjInit(pMap->pVm,&sResult);` |
|        29 | 8089 | `	sResult.nIdx = SXU32_HIGH; /* Mark as constant */` |
|        73 | 8090 | `	for( n = 0 ; n < pMap->nEntry ; ++n ){` |
|        59 | 8091 | `		pValue = HashmapExtractNodeValue(pEntry);` |
|        59 | 8092 | `		if( pValue ){` |
|         - | 8093 | `			/* The callback receives ($value, $key). */` |
|        59 | 8094 | `			PH7_HashmapExtractNodeKey(pEntry,&sKey);` |
|        59 | 8095 | `			apCbArg[0] = pValue;` |
|        59 | 8096 | `			apCbArg[1] = &sKey;` |
|        59 | 8097 | `			rc = PH7_VmCallUserFunction(pMap->pVm,apArg[1],2,apCbArg,&sResult);` |
|        59 | 8098 | `			if( rc == PH7_EXCEPTION ){` |
|         - | 8099 | `				/* The callback raised: propagate so the dispatcher unwinds. */` |
|       ! 0 | 8100 | `				PH7_MemObjRelease(&sKey);` |
|       ! 0 | 8101 | `				PH7_MemObjRelease(&sResult);` |
|       ! 0 | 8102 | `				return PH7_EXCEPTION;` |
|         - | 8103 | `			}` |
|        59 | 8104 | `			if( rc == SXRET_OK && (ph7_value_to_bool(&sResult) ? 1 : 0) == bWant ){` |
|        15 | 8105 | `				*ppMatch = pEntry;` |
|        15 | 8106 | `				break;` |
|         - | 8107 | `			}` |
|        22 | 8108 | `		}` |
|        45 | 8109 | `		pEntry = pEntry->pPrev; /* Reverse link */` |
|        23 | 8110 | `	}` |
|        29 | 8111 | `	PH7_MemObjRelease(&sKey);` |
|        29 | 8112 | `	PH7_MemObjRelease(&sResult);` |
|        29 | 8113 | `	return PH7_OK;` |
|        15 | 8114 | `}` |
|         - | 8115 | `/*` |
|         - | 8116 | ` * mixed array_find(array $array, callable $callback)` |
|         - | 8117 | ` *  Returns the value of the first element for which $callback($value,$key)` |
|         - | 8118 | ` *  is truthy, or NULL if none match.` |
|         - | 8119 | ` */` |
|         6 | 8120 | `static int ph7_hashmap_find(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|         1 | 8121 | `{` |
|         - | 8122 | `	ph7_hashmap_node *pMatch;` |
|         - | 8123 | `	ph7_value *pVal;` |
|         - | 8124 | `	sxi32 rc;` |
|         7 | 8125 | `	rc = HashmapCallbackSearch(pCtx,nArg,apArg,"array_find",1,&pMatch);` |
|         7 | 8126 | `	if( rc != PH7_OK ){` |
|       ! 0 | 8127 | `		return rc;` |
|         - | 8128 | `	}` |
|         7 | 8129 | `	if( pMatch && (pVal = HashmapExtractNodeValue(pMatch)) != 0 ){` |
|         5 | 8130 | `		ph7_result_value(pCtx,pVal);` |
|         3 | 8131 | `	}else{` |
|         3 | 8132 | `		ph7_result_null(pCtx);` |
|         - | 8133 | `	}` |
|         7 | 8134 | `	return PH7_OK;` |
|         4 | 8135 | `}` |
|         - | 8136 | `/*` |
|         - | 8137 | ` * mixed array_find_key(array $array, callable $callback)` |
|         - | 8138 | ` *  Returns the key of the first element for which $callback($value,$key)` |
|         - | 8139 | ` *  is truthy, or NULL if none match.` |
|         - | 8140 | ` */` |
|         6 | 8141 | `static int ph7_hashmap_find_key(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|         1 | 8142 | `{` |
|         - | 8143 | `	ph7_hashmap_node *pMatch;` |
|         - | 8144 | `	sxi32 rc;` |
|         7 | 8145 | `	rc = HashmapCallbackSearch(pCtx,nArg,apArg,"array_find_key",1,&pMatch);` |
|         7 | 8146 | `	if( rc != PH7_OK ){` |
|       ! 0 | 8147 | `		return rc;` |
|         - | 8148 | `	}` |
|         7 | 8149 | `	if( pMatch == 0 ){` |
|         3 | 8150 | `		ph7_result_null(pCtx);` |
|         6 | 8151 | `	}else if( pMatch->iType == HASHMAP_INT_NODE ){` |
|         3 | 8152 | `		ph7_result_int64(pCtx,pMatch->xKey.iKey);` |
|         2 | 8153 | `	}else{` |
|         4 | 8154 | `		ph7_result_string(pCtx,` |
|         2 | 8155 | `			(const char *)SyBlobData(&pMatch->xKey.sKey),` |
|         2 | 8156 | `			(int)SyBlobLength(&pMatch->xKey.sKey));` |
|         - | 8157 | `	}` |
|         7 | 8158 | `	return PH7_OK;` |
|         4 | 8159 | `}` |
|         - | 8160 | `/*` |
|         - | 8161 | ` * bool array_any(array $array, callable $callback)` |
|         - | 8162 | ` *  Returns TRUE if $callback($value,$key) is truthy for at least one element.` |
|         - | 8163 | ` *  FALSE for an empty array.` |
|         - | 8164 | ` */` |
|         8 | 8165 | `static int ph7_hashmap_any(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|         1 | 8166 | `{` |
|         - | 8167 | `	ph7_hashmap_node *pMatch;` |
|         - | 8168 | `	sxi32 rc;` |
|         9 | 8169 | `	rc = HashmapCallbackSearch(pCtx,nArg,apArg,"array_any",1,&pMatch);` |
|         9 | 8170 | `	if( rc != PH7_OK ){` |
|       ! 0 | 8171 | `		return rc;` |
|         - | 8172 | `	}` |
|         9 | 8173 | `	ph7_result_bool(pCtx,pMatch != 0);` |
|         9 | 8174 | `	return PH7_OK;` |
|         5 | 8175 | `}` |
|         - | 8176 | `/*` |
|         - | 8177 | ` * bool array_all(array $array, callable $callback)` |
|         - | 8178 | ` *  Returns TRUE if $callback($value,$key) is truthy for every element (and for` |
|         - | 8179 | ` *  an empty array). Hunts for the first falsy element: its absence means "all".` |
|         - | 8180 | ` */` |
|         8 | 8181 | `static int ph7_hashmap_all(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|         1 | 8182 | `{` |
|         - | 8183 | `	ph7_hashmap_node *pMatch;` |
|         - | 8184 | `	sxi32 rc;` |
|         9 | 8185 | `	rc = HashmapCallbackSearch(pCtx,nArg,apArg,"array_all",0,&pMatch);` |
|         9 | 8186 | `	if( rc != PH7_OK ){` |
|       ! 0 | 8187 | `		return rc;` |
|         - | 8188 | `	}` |
|         9 | 8189 | `	ph7_result_bool(pCtx,pMatch == 0);` |
|         9 | 8190 | `	return PH7_OK;` |
|         5 | 8191 | `}` |
|         - | 8192 | `/*` |
|         - | 8193 | ` * The iterator_*() family — walk a Traversable via the shared PH7_VmIteratorWalk` |
|         - | 8194 | ` * helper (the reusable form of the foreach Iterator protocol).` |
|         - | 8195 | ` */` |
|         - | 8196 | `/* Step shared by iterator_to_array (pArray set) and iterator_count (pArray NULL). */` |
|         - | 8197 | `struct IterCollect { ph7_value *pArray; int bPreserve; sxi64 nCount; };` |
|        70 | 8198 | `static sxi32 IterCollectStep(ph7_vm *pVm, ph7_value *pKey, ph7_value *pValue, void *pUserData)` |
|         4 | 8199 | `{` |
|        74 | 8200 | `	struct IterCollect *p = (struct IterCollect *)pUserData;` |
|        35 | 8201 | `	(void)pVm;` |
|        74 | 8202 | `	p->nCount++;` |
|        74 | 8203 | `	if( p->pArray ){` |
|         - | 8204 | `		/* preserve_keys: insert with the iterator key (later wins on collision);` |
|         - | 8205 | `		 * otherwise append with an auto-assigned int index. */` |
|        66 | 8206 | `		ph7_array_add_elem(p->pArray, p->bPreserve ? pKey : 0, pValue);` |
|        31 | 8207 | `	}` |
|        74 | 8208 | `	return SXRET_OK;` |
|         4 | 8209 | `}` |
|         - | 8210 | `/*` |
|         - | 8211 | ` * array iterator_to_array(Traversable\|array $iterator, bool $preserve_keys = true)` |
|         - | 8212 | ` */` |
|        26 | 8213 | `static int ph7_iterator_to_array(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|         4 | 8214 | `{` |
|         - | 8215 | `	struct IterCollect sCol;` |
|         - | 8216 | `	ph7_value *pArray;` |
|         - | 8217 | `	sxi32 rc;` |
|        30 | 8218 | `	if( nArg < 1 ){ ph7_result_null(pCtx); return PH7_OK; }` |
|        30 | 8219 | `	pArray = ph7_context_new_array(pCtx);` |
|        30 | 8220 | `	if( pArray == 0 ){ ph7_result_null(pCtx); return PH7_OK; }` |
|        30 | 8221 | `	sCol.pArray = pArray;` |
|        30 | 8222 | `	sCol.bPreserve = (nArg > 1) ? ph7_value_to_bool(apArg[1]) : 1;` |
|        30 | 8223 | `	sCol.nCount = 0;` |
|        30 | 8224 | `	if( ph7_value_is_array(apArg[0]) ){` |
|         - | 8225 | `		/* PHP 8.2 accepts a plain array: copy it (preserving or renumbering keys). */` |
|         3 | 8226 | `		ph7_hashmap *pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|         3 | 8227 | `		ph7_hashmap_node *pEntry = pMap->pFirst;` |
|         - | 8228 | `		sxu32 n;` |
|         9 | 8229 | `		for( n = 0 ; n < pMap->nEntry ; n++ ){` |
|         - | 8230 | `			ph7_value sKey, *pVal;` |
|         7 | 8231 | `			PH7_MemObjInit(pCtx->pVm,&sKey);` |
|         7 | 8232 | `			PH7_HashmapExtractNodeKey(pEntry,&sKey);` |
|         7 | 8233 | `			pVal = (ph7_value *)SySetAt(&pCtx->pVm->aMemObj,pEntry->nValIdx);` |
|         7 | 8234 | `			if( pVal ){ ph7_array_add_elem(pArray, sCol.bPreserve ? &sKey : 0, pVal); }` |
|         7 | 8235 | `			PH7_MemObjRelease(&sKey);` |
|         7 | 8236 | `			pEntry = pEntry->pPrev;` |
|         4 | 8237 | `		}` |
|         3 | 8238 | `		ph7_result_value(pCtx,pArray);` |
|         3 | 8239 | `		return PH7_OK;` |
|         - | 8240 | `	}` |
|        28 | 8241 | `	rc = PH7_VmIteratorWalk(pCtx->pVm, apArg[0], IterCollectStep, &sCol);` |
|        28 | 8242 | `	if( rc == PH7_EXCEPTION \|\| rc == PH7_ABORT ){ return rc; }` |
|        26 | 8243 | `	if( rc == SXERR_NOTIMPLEMENTED ){` |
|       ! 0 | 8244 | `		return PH7_VmThrowException(pCtx,"TypeError",` |
|         - | 8245 | `			"iterator_to_array(): Argument #1 ($iterator) must be of type Traversable\|array, %s given",` |
|       ! 0 | 8246 | `			ph7_type_name(apArg[0]));` |
|         - | 8247 | `	}` |
|        26 | 8248 | `	ph7_result_value(pCtx,pArray);` |
|        26 | 8249 | `	return PH7_OK;` |
|        17 | 8250 | `}` |
|         - | 8251 | `/*` |
|         - | 8252 | ` * int iterator_count(Traversable\|array $iterator)` |
|         - | 8253 | ` */` |
|         6 | 8254 | `static int ph7_iterator_count(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|         1 | 8255 | `{` |
|         - | 8256 | `	struct IterCollect sCol;` |
|         - | 8257 | `	sxi32 rc;` |
|         7 | 8258 | `	if( nArg < 1 ){ ph7_result_int(pCtx,0); return PH7_OK; }` |
|         7 | 8259 | `	if( ph7_value_is_array(apArg[0]) ){` |
|         3 | 8260 | `		ph7_result_int64(pCtx, (ph7_int64)((ph7_hashmap *)apArg[0]->x.pOther)->nEntry);` |
|         3 | 8261 | `		return PH7_OK;` |
|         - | 8262 | `	}` |
|         5 | 8263 | `	sCol.pArray = 0; sCol.bPreserve = 0; sCol.nCount = 0;` |
|         5 | 8264 | `	rc = PH7_VmIteratorWalk(pCtx->pVm, apArg[0], IterCollectStep, &sCol);` |
|         5 | 8265 | `	if( rc == PH7_EXCEPTION \|\| rc == PH7_ABORT ){ return rc; }` |
|         5 | 8266 | `	if( rc == SXERR_NOTIMPLEMENTED ){` |
|       ! 0 | 8267 | `		return PH7_VmThrowException(pCtx,"TypeError",` |
|         - | 8268 | `			"iterator_count(): Argument #1 ($iterator) must be of type Traversable\|array, %s given",` |
|       ! 0 | 8269 | `			ph7_type_name(apArg[0]));` |
|         - | 8270 | `	}` |
|         5 | 8271 | `	ph7_result_int64(pCtx, sCol.nCount);` |
|         5 | 8272 | `	return PH7_OK;` |
|         4 | 8273 | `}` |
|         - | 8274 | `/* iterator_apply step: call the fixed callback with $args each iteration. The` |
|         - | 8275 | ` * arg pointers are resolved fresh per step because the iterator's own methods` |
|         - | 8276 | ` * run user code between iterations and may reallocate the aMemObj pool. */` |
|         - | 8277 | `struct IterApply { ph7_value *pCallback; ph7_value *pArgsArray; sxi64 nCount; };` |
|        24 | 8278 | `static sxi32 IterApplyStep(ph7_vm *pVm, ph7_value *pKey, ph7_value *pValue, void *pUserData)` |
|         1 | 8279 | `{` |
|        25 | 8280 | `	struct IterApply *p = (struct IterApply *)pUserData;` |
|         - | 8281 | `	ph7_value sResult;` |
|         - | 8282 | `	SySet aArg;` |
|         - | 8283 | `	sxi32 rc;` |
|         - | 8284 | `	int bContinue;` |
|        12 | 8285 | `	(void)pKey; (void)pValue; /* iterator_apply does NOT pass the element to the callback */` |
|        25 | 8286 | `	SySetInit(&aArg,&pVm->sAllocator,sizeof(ph7_value *));` |
|        25 | 8287 | `	if( p->pArgsArray && (p->pArgsArray->iFlags & MEMOBJ_HASHMAP) ){` |
|         9 | 8288 | `		ph7_hashmap *pMap = (ph7_hashmap *)p->pArgsArray->x.pOther;` |
|         9 | 8289 | `		ph7_hashmap_node *pEntry = pMap->pFirst;` |
|         - | 8290 | `		sxu32 n;` |
|        17 | 8291 | `		for( n = 0 ; n < pMap->nEntry ; n++ ){` |
|         9 | 8292 | `			ph7_value *pVal = (ph7_value *)SySetAt(&pVm->aMemObj,pEntry->nValIdx);` |
|         9 | 8293 | `			if( pVal ){ SySetPut(&aArg,(const void *)&pVal); }` |
|         9 | 8294 | `			pEntry = pEntry->pPrev;` |
|         5 | 8295 | `		}` |
|         4 | 8296 | `	}` |
|        25 | 8297 | `	PH7_MemObjInit(pVm,&sResult);` |
|        37 | 8298 | `	rc = PH7_VmCallUserFunction(pVm, p->pCallback, (int)SySetUsed(&aArg),` |
|        24 | 8299 | `		(ph7_value **)SySetBasePtr(&aArg), &sResult);` |
|        25 | 8300 | `	SySetRelease(&aArg);` |
|        25 | 8301 | `	if( rc == PH7_EXCEPTION \|\| rc == PH7_ABORT ){ PH7_MemObjRelease(&sResult); return rc; }` |
|        23 | 8302 | `	p->nCount++;` |
|        23 | 8303 | `	PH7_MemObjToBool(&sResult);` |
|        23 | 8304 | `	bContinue = (sResult.x.iVal != 0);` |
|        23 | 8305 | `	PH7_MemObjRelease(&sResult);` |
|        23 | 8306 | `	return bContinue ? SXRET_OK : SXERR_EOF; /* falsy return stops iteration */` |
|        13 | 8307 | `}` |
|         - | 8308 | `/*` |
|         - | 8309 | ` * int iterator_apply(Traversable $iterator, callable $callback, array $args = [])` |
|         - | 8310 | ` */` |
|         8 | 8311 | `static int ph7_iterator_apply(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|         1 | 8312 | `{` |
|         - | 8313 | `	struct IterApply sApp;` |
|         - | 8314 | `	sxi32 rc;` |
|         9 | 8315 | `	if( nArg < 2 ){ ph7_result_int(pCtx,0); return PH7_OK; }` |
|         9 | 8316 | `	if( !ph7_value_is_callable(apArg[1]) ){` |
|       ! 0 | 8317 | `		return PH7_VmThrowException(pCtx,"TypeError",` |
|         - | 8318 | `			"iterator_apply(): Argument #2 ($callback) must be a valid callback");` |
|         - | 8319 | `	}` |
|         9 | 8320 | `	sApp.pCallback = apArg[1];` |
|         9 | 8321 | `	sApp.pArgsArray = (nArg > 2 && ph7_value_is_array(apArg[2])) ? apArg[2] : 0;` |
|         9 | 8322 | `	sApp.nCount = 0;` |
|         9 | 8323 | `	rc = PH7_VmIteratorWalk(pCtx->pVm, apArg[0], IterApplyStep, &sApp);` |
|         9 | 8324 | `	if( rc == PH7_EXCEPTION \|\| rc == PH7_ABORT ){ return rc; }` |
|         7 | 8325 | `	if( rc == SXERR_NOTIMPLEMENTED ){` |
|       ! 0 | 8326 | `		return PH7_VmThrowException(pCtx,"TypeError",` |
|         - | 8327 | `			"iterator_apply(): Argument #1 ($iterator) must be of type Traversable, %s given",` |
|       ! 0 | 8328 | `			ph7_type_name(apArg[0]));` |
|         - | 8329 | `	}` |
|         7 | 8330 | `	ph7_result_int64(pCtx, sApp.nCount);` |
|         7 | 8331 | `	return PH7_OK;` |
|         5 | 8332 | `}` |
|         - | 8333 | `/*` |
|         - | 8334 | ` * Table of hashmap functions.` |
|         - | 8335 | ` */` |
|         - | 8336 | `static const ph7_builtin_func aHashmapFunc[] = {` |
|         - | 8337 | `	{"iterator_to_array",  ph7_iterator_to_array },` |
|         - | 8338 | `	{"iterator_count",     ph7_iterator_count },` |
|         - | 8339 | `	{"iterator_apply",     ph7_iterator_apply },` |
|         - | 8340 | `	{"count",             ph7_hashmap_count },` |
|         - | 8341 | `	{"sizeof",            ph7_hashmap_count },` |
|         - | 8342 | `	{"array_key_exists",  ph7_hashmap_key_exists },` |
|         - | 8343 | `	{"array_pop",         ph7_hashmap_pop     },` |
|         - | 8344 | `	{"array_push",        ph7_hashmap_push    },` |
|         - | 8345 | `	{"array_shift",       ph7_hashmap_shift   },` |
|         - | 8346 | `	{"array_product",     ph7_hashmap_product },` |
|         - | 8347 | `	{"array_sum",         ph7_hashmap_sum     },` |
|         - | 8348 | `	{"array_keys",        ph7_hashmap_keys    },` |
|         - | 8349 | `	{"array_values",      ph7_hashmap_values  },` |
|         - | 8350 | `	{"array_same",        ph7_hashmap_same    },  /* Symisc eXtension */` |
|         - | 8351 | `	{"array_merge",       ph7_hashmap_merge   },` |
|         - | 8352 | `	{"array_slice",       ph7_hashmap_slice   },` |
|         - | 8353 | `	{"array_splice",      ph7_hashmap_splice  },` |
|         - | 8354 | `	{"array_search",      ph7_hashmap_search  },` |
|         - | 8355 | `	{"array_diff",        ph7_hashmap_diff    },` |
|         - | 8356 | `	{"array_udiff",       ph7_hashmap_udiff   },` |
|         - | 8357 | `	{"array_diff_assoc",  ph7_hashmap_diff_assoc },` |
|         - | 8358 | `	{"array_diff_uassoc", ph7_hashmap_diff_uassoc },` |
|         - | 8359 | `	{"array_diff_key",    ph7_hashmap_diff_key },` |
|         - | 8360 | `	{"array_intersect",   ph7_hashmap_intersect},` |
|         - | 8361 | `	{"array_intersect_assoc", ph7_hashmap_intersect_assoc},` |
|         - | 8362 | `	{"array_uintersect",  ph7_hashmap_uintersect},` |
|         - | 8363 | `	{"array_intersect_key",   ph7_hashmap_intersect_key},` |
|         - | 8364 | `	{"array_copy",        ph7_hashmap_copy    },` |
|         - | 8365 | `	{"array_erase",       ph7_hashmap_erase   },` |
|         - | 8366 | `	{"array_fill",        ph7_hashmap_fill    },` |
|         - | 8367 | `	{"array_fill_keys",   ph7_hashmap_fill_keys},` |
|         - | 8368 | `	{"array_combine",     ph7_hashmap_combine },` |
|         - | 8369 | `	{"array_reverse",     ph7_hashmap_reverse },` |
|         - | 8370 | `	{"array_unique",      ph7_hashmap_unique  },` |
|         - | 8371 | `	{"array_flip",        ph7_hashmap_flip    },` |
|         - | 8372 | `	{"array_rand",        ph7_hashmap_rand    },` |
|         - | 8373 | `	{"array_chunk",       ph7_hashmap_chunk   },` |
|         - | 8374 | `	{"array_pad",         ph7_hashmap_pad     },` |
|         - | 8375 | `	{"array_replace",     ph7_hashmap_replace },` |
|         - | 8376 | `	{"array_filter",      ph7_hashmap_filter  },` |
|         - | 8377 | `	{"array_map",         ph7_hashmap_map     },` |
|         - | 8378 | `	{"array_column",      ph7_hashmap_column  },` |
|         - | 8379 | `	{"array_is_list",     ph7_hashmap_is_list },` |
|         - | 8380 | `	{"array_first",       ph7_hashmap_first   },` |
|         - | 8381 | `	{"array_last",        ph7_hashmap_last    },` |
|         - | 8382 | `	{"array_find",        ph7_hashmap_find    },` |
|         - | 8383 | `	{"array_find_key",    ph7_hashmap_find_key},` |
|         - | 8384 | `	{"array_any",         ph7_hashmap_any     },` |
|         - | 8385 | `	{"array_all",         ph7_hashmap_all     },` |
|         - | 8386 | `	{"array_reduce",      ph7_hashmap_reduce  },` |
|         - | 8387 | `	{"array_walk",        ph7_hashmap_walk    },` |
|         - | 8388 | `	{"array_walk_recursive", ph7_hashmap_walk_recursive },` |
|         - | 8389 | `	{"in_array",          ph7_hashmap_in_array},` |
|         - | 8390 | `	{"sort",              ph7_hashmap_sort    },` |
|         - | 8391 | `	{"asort",             ph7_hashmap_asort   },` |
|         - | 8392 | `	{"arsort",            ph7_hashmap_arsort  },` |
|         - | 8393 | `	{"ksort",             ph7_hashmap_ksort   },` |
|         - | 8394 | `	{"krsort",            ph7_hashmap_krsort  },` |
|         - | 8395 | `	{"rsort",             ph7_hashmap_rsort   },` |
|         - | 8396 | `	{"usort",             ph7_hashmap_usort   },` |
|         - | 8397 | `	{"uasort",            ph7_hashmap_uasort  },` |
|         - | 8398 | `	{"uksort",            ph7_hashmap_uksort  },` |
|         - | 8399 | `	{"shuffle",           ph7_hashmap_shuffle },` |
|         - | 8400 | `	{"range",             ph7_hashmap_range   },` |
|         - | 8401 | `	{"current",           ph7_hashmap_current },` |
|         - | 8402 | `	{"each",              ph7_hashmap_each    },` |
|         - | 8403 | `	{"pos",               ph7_hashmap_current },` |
|         - | 8404 | `	{"next",              ph7_hashmap_next    },` |
|         - | 8405 | `	{"prev",              ph7_hashmap_prev    },` |
|         - | 8406 | `	{"end",               ph7_hashmap_end     },` |
|         - | 8407 | `	{"reset",             ph7_hashmap_reset   },` |
|         - | 8408 | `	{"key",               ph7_hashmap_simple_key }` |
|         - | 8409 | `};` |
|         - | 8410 | `/*` |
|         - | 8411 | ` * Register the built-in hashmap functions defined above.` |
|         - | 8412 | ` */` |
|      3474 | 8413 | `PH7_PRIVATE void PH7_RegisterHashmapFunctions(ph7_vm *pVm)` |
|         5 | 8414 | `{` |
|         - | 8415 | `	sxu32 n;` |
|    253607 | 8416 | `	for( n = 0 ; n < SX_ARRAYSIZE(aHashmapFunc) ; n++ ){` |
|    250133 | 8417 | `		ph7_create_function(&(*pVm),aHashmapFunc[n].zName,aHashmapFunc[n].xFunc,0);` |
|    125069 | 8418 | `	}` |
|      3479 | 8419 | `}` |
|         - | 8420 | `/*` |
|         - | 8421 | ` * Dump a hashmap instance and it's entries and the store the dump in` |
|         - | 8422 | ` * the BLOB given as the first argument.` |
|         - | 8423 | ` * This function is typically invoked when the user issue a call to` |
|         - | 8424 | ` * [var_dump(),var_export(),print_r(),...]` |
|         - | 8425 | ` * This function SXRET_OK on success. Any other return value including` |
|         - | 8426 | ` * SXERR_LIMIT(infinite recursion) indicates failure.` |
|         - | 8427 | ` */` |
|         - | 8428 | `/*` |
|         - | 8429 | ` * Dump the entries of a hashmap [i.e: the key/value lines between the opening` |
|         - | 8430 | ` * '{' and the closing '}'] in the var_dump/print_r style. Factored out of` |
|         - | 8431 | ` * PH7_HashmapDump so the var_dump object renderer can reuse it for a` |
|         - | 8432 | ` * __debugInfo() array body (which carries an object header, not "array(N)").` |
|         - | 8433 | ` * Returns SXERR_LIMIT if a nested value hit the depth cap.` |
|         - | 8434 | ` */` |
|        26 | 8435 | `PH7_PRIVATE sxi32 PH7_HashmapDumpEntries(SyBlob *pOut,ph7_hashmap *pMap,int ShowType,int nTab,int nDepth)` |
|         2 | 8436 | `{` |
|        28 | 8437 | `	ph7_hashmap_node *pEntry = pMap->pFirst;` |
|         - | 8438 | `	ph7_value *pObj;` |
|        28 | 8439 | `	sxu32 n = 0;` |
|         - | 8440 | `	int isRef;` |
|        28 | 8441 | `	sxi32 rc = SXRET_OK;` |
|         - | 8442 | `	int i;` |
|        44 | 8443 | `	for(;;){` |
|        90 | 8444 | `		if( n >= pMap->nEntry ){` |
|        28 | 8445 | `			break;` |
|         - | 8446 | `		}` |
|       126 | 8447 | `		for( i = 0 ; i < nTab ; i++ ){` |
|        64 | 8448 | `			SyBlobAppend(&(*pOut)," ",sizeof(char));` |
|        33 | 8449 | `		}` |
|         - | 8450 | `		/* Dump key */` |
|        64 | 8451 | `		if( pEntry->iType == HASHMAP_INT_NODE){` |
|        33 | 8452 | `			SyBlobFormat(&(*pOut),"[%qd] =>",pEntry->xKey.iKey);` |
|        17 | 8453 | `		}else{` |
|        47 | 8454 | `			SyBlobFormat(&(*pOut),"[%.*s] =>",` |
|        15 | 8455 | `				SyBlobLength(&pEntry->xKey.sKey),SyBlobData(&pEntry->xKey.sKey));` |
|         - | 8456 | `		}` |
|         - | 8457 | `#ifdef __WINNT__` |
|         2 | 8458 | `		SyBlobAppend(&(*pOut),"\r\n",sizeof("\r\n")-1);` |
|         - | 8459 | `#else` |
|        62 | 8460 | `		SyBlobAppend(&(*pOut),"\n",sizeof(char));` |
|         - | 8461 | `#endif` |
|         - | 8462 | `		/* Dump node value */` |
|        64 | 8463 | `		pObj = HashmapExtractNodeValue(pEntry);` |
|        64 | 8464 | `		isRef = 0;` |
|        64 | 8465 | `		if( pObj ){` |
|        64 | 8466 | `			if( pEntry->iFlags & HASHMAP_NODE_FOREIGN_OBJ ){` |
|         - | 8467 | `				/* Referenced object */` |
|       ! 0 | 8468 | `				isRef = 1;` |
|       ! 0 | 8469 | `			}` |
|        64 | 8470 | `			rc = PH7_MemObjDump(&(*pOut),pObj,ShowType,nTab+1,nDepth,isRef);` |
|        64 | 8471 | `			if( rc == SXERR_LIMIT ){` |
|       ! 0 | 8472 | `				break;` |
|         - | 8473 | `			}` |
|        31 | 8474 | `		}` |
|         - | 8475 | `		/* Point to the next entry */` |
|        64 | 8476 | `		n++;` |
|        64 | 8477 | `		pEntry = pEntry->pPrev; /* Reverse link */` |
|         2 | 8478 | `	}` |
|        28 | 8479 | `	return rc;` |
|         2 | 8480 | `}` |
|        22 | 8481 | `PH7_PRIVATE sxi32 PH7_HashmapDump(SyBlob *pOut,ph7_hashmap *pMap,int ShowType,int nTab,int nDepth)` |
|         2 | 8482 | `{` |
|         - | 8483 | `	sxi32 rc;` |
|         - | 8484 | `	int i;` |
|        24 | 8485 | `	if( nDepth > 31 ){` |
|         - | 8486 | `		static const char zInfinite[] = "Nesting limit reached: Infinite recursion?";` |
|         - | 8487 | `		/* Nesting limit reached */` |
|       ! 0 | 8488 | `		SyBlobAppend(&(*pOut),zInfinite,sizeof(zInfinite)-1);` |
|       ! 0 | 8489 | `		if( ShowType ){` |
|       ! 0 | 8490 | `			SyBlobAppend(&(*pOut),")",sizeof(char));` |
|       ! 0 | 8491 | `		}` |
|       ! 0 | 8492 | `		return SXERR_LIMIT;` |
|         - | 8493 | `	}` |
|        24 | 8494 | `	if( !ShowType ){` |
|        11 | 8495 | `		SyBlobAppend(&(*pOut),"Array(",sizeof("Array(")-1);` |
|         5 | 8496 | `	}` |
|         - | 8497 | `	/* Total entries */` |
|        24 | 8498 | `	SyBlobFormat(&(*pOut),"%u) {",pMap->nEntry);` |
|         - | 8499 | `#ifdef __WINNT__` |
|         2 | 8500 | `	SyBlobAppend(&(*pOut),"\r\n",sizeof("\r\n")-1);` |
|         - | 8501 | `#else` |
|        22 | 8502 | `	SyBlobAppend(&(*pOut),"\n",sizeof(char));` |
|         - | 8503 | `#endif` |
|        24 | 8504 | `	rc = PH7_HashmapDumpEntries(&(*pOut),pMap,ShowType,nTab,nDepth);` |
|        46 | 8505 | `	for( i = 0 ; i < nTab ; i++ ){` |
|        24 | 8506 | `		SyBlobAppend(&(*pOut)," ",sizeof(char));` |
|        13 | 8507 | `	}` |
|        24 | 8508 | `	SyBlobAppend(&(*pOut),"}",sizeof(char));` |
|        24 | 8509 | `	return rc;` |
|        13 | 8510 | `}` |
|         - | 8511 | `/*` |
|         - | 8512 | ` * Iterate throw hashmap entries and invoke the given callback [i.e: xWalk()] for each` |
|         - | 8513 | ` * retrieved entry.` |
|         - | 8514 | ` * Note that argument are passed to the callback by copy. That is,any modification to` |
|         - | 8515 | ` * the entry value in the callback body will not alter the real value.` |
|         - | 8516 | ` * If the callback wishes to abort processing [i.e: it's invocation] it must return` |
|         - | 8517 | ` * a value different from PH7_OK.` |
|         - | 8518 | ` * Refer to [ph7_array_walk()] for more information.` |
|         - | 8519 | ` */` |
|     33274 | 8520 | `PH7_PRIVATE sxi32 PH7_HashmapWalk(` |
|         - | 8521 | `	ph7_hashmap *pMap, /* Target hashmap */` |
|         - | 8522 | `	int (*xWalk)(ph7_value *,ph7_value *,void *), /* Walker callback */` |
|         - | 8523 | `	void *pUserData /* Last argument to xWalk() */` |
|         - | 8524 | `	)` |
|         5 | 8525 | `{` |
|         - | 8526 | `	ph7_hashmap_node *pEntry;` |
|         - | 8527 | `	ph7_value sKey,sValue;` |
|         - | 8528 | `	sxi32 rc;` |
|         - | 8529 | `	sxu32 n;` |
|         - | 8530 | `	/* Initialize walker parameter */` |
|     33279 | 8531 | `	rc = SXRET_OK;` |
|     33279 | 8532 | `	PH7_MemObjInit(pMap->pVm,&sKey);` |
|     33279 | 8533 | `	PH7_MemObjInit(pMap->pVm,&sValue);` |
|     33279 | 8534 | `	n = pMap->nEntry;` |
|     33279 | 8535 | `	pEntry = pMap->pFirst;` |
|         - | 8536 | `	/* Start the iteration process */` |
|     86897 | 8537 | `	for(;;){` |
|    173799 | 8538 | `		if( n < 1 ){` |
|     33279 | 8539 | `			break;` |
|         - | 8540 | `		}` |
|         - | 8541 | `		/* Extract a copy of the key and a copy the current value */` |
|    140525 | 8542 | `		PH7_HashmapExtractNodeKey(pEntry,&sKey);` |
|    140525 | 8543 | `		PH7_HashmapExtractNodeValue(pEntry,&sValue,FALSE);` |
|         - | 8544 | `		/* Invoke the user callback */` |
|    140525 | 8545 | `		rc = xWalk(&sKey,&sValue,pUserData);` |
|         - | 8546 | `		/* Release the copy of the key and the value */` |
|    140525 | 8547 | `		PH7_MemObjRelease(&sKey);` |
|    140525 | 8548 | `		PH7_MemObjRelease(&sValue);` |
|    140525 | 8549 | `		if( rc != PH7_OK ){` |
|         - | 8550 | `			/* Callback request an operation abort */` |
|       ! 0 | 8551 | `			return SXERR_ABORT;` |
|         - | 8552 | `		}` |
|         - | 8553 | `		/* Point to the next entry */` |
|    140525 | 8554 | `		pEntry = pEntry->pPrev; /* Reverse link */` |
|    140525 | 8555 | `		n--;` |
|         5 | 8556 | `	}` |
|         - | 8557 | `	/* All done */` |
|     33279 | 8558 | `	return SXRET_OK;` |
|     16642 | 8559 | `}` |
|         - | 8560 |  |
