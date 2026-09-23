/**
 * SPDX-FileCopyrightText: 2011, 2012, 2013, 2014 Symisc Systems <licensing@symisc.net>
 * SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
 * SPDX-License-Identifier: BSD-3-Clause
 */
#include "ph7int.h"
/* range() formats the float variant of its max-array-size ValueError with libc
 * snprintf and parses numeric strings with libc strtod — the byte-exact-floats
 * rule (see builtin_math.c): SyBufferFormat/SyStrToReal are not correctly
 * rounded at extreme magnitudes. */
#include <stdio.h>  /* snprintf */
#include <stdlib.h> /* strtod */
/* This file implement generic hashmaps known as 'array' in the PHP world */
/* HASHMAP_INT_NODE / HASHMAP_BLOB_NODE (node key types) are declared in ph7int.h
 * alongside ph7_hashmap_node so name-forwarding builtins can classify keys. */
/* Node control flags */
#define HASHMAP_NODE_FOREIGN_OBJ 0x001 /* Node hold a reference to a foreign ph7_value
                                        * [i.e: array(&var)/$a[] =& $var ]
										*/
/*
 * Default hash function for int [i.e; 64-bit integer] keys.
 */
static sxu32 IntHash(sxi64 iKey)
{
	sxu64 uKey = (sxu64)iKey; /* unsigned mixing: shifting a negative key is UB */
	return (sxu32)(uKey ^ (uKey << 8) ^ (uKey >> 8));
}
/*
 * Default hash function for string/BLOB keys.
 */
static sxu32 BinHash(const void *pSrc,sxu32 nLen)
{
	register unsigned char *zIn = (unsigned char *)pSrc;
	unsigned char *zEnd;
	sxu32 nH = 5381;
	zEnd = &zIn[nLen];
	for(;;){
		if( zIn >= zEnd ){ break; } nH = nH * 33 + zIn[0] ; zIn++;
		if( zIn >= zEnd ){ break; } nH = nH * 33 + zIn[0] ; zIn++;
		if( zIn >= zEnd ){ break; } nH = nH * 33 + zIn[0] ; zIn++;
		if( zIn >= zEnd ){ break; } nH = nH * 33 + zIn[0] ; zIn++;
	}
	return nH;
}
/*
 * Return the total number of entries in a given hashmap.
 * If bRecursive is set to TRUE then recurse on hashmap entries.
 * Self-referential arrays are detected via the HASHMAP_COUNTING flag;
 * when a cycle is found the nested array is skipped and *pCycleDetected
 * is set to TRUE so the caller can emit a warning.
 */
PH7_PRIVATE sxi64 HashmapCount(ph7_hashmap *pMap,int bRecursive,int *pCycleDetected)
{
	sxi64 iCount = 0;
	if( !bRecursive ){
		iCount = pMap->nEntry;
	}else{
		/* Recursive hashmap walk */
		ph7_hashmap_node *pEntry = pMap->pLast;
		ph7_value *pElem;
		sxu32 n = 0;
		/* Mark this map as being counted */
		pMap->iFlags |= HASHMAP_COUNTING;
		for(;;){
			if( n >= pMap->nEntry ){
				break;
			}
			/* Point to the element value */
			pElem = (ph7_value *)SySetAt(&pMap->pVm->aMemObj,pEntry->nValIdx);
			if( pElem ){
				if( pElem->iFlags & MEMOBJ_HASHMAP ){
					ph7_hashmap *pSub = (ph7_hashmap *)pElem->x.pOther;
					if( pSub->iFlags & HASHMAP_COUNTING ){
						/* Cycle detected — skip this entry */
						if( pCycleDetected ){
							*pCycleDetected = TRUE;
						}
					}else{
						iCount += HashmapCount(pSub,TRUE,pCycleDetected);
					}
				}
			}
			/* Point to the next entry */
			pEntry = pEntry->pNext;
			++n;
		}
		/* Clear the counting flag */
		pMap->iFlags &= ~HASHMAP_COUNTING;
		/* Update count */
		iCount += pMap->nEntry;
	}
	return iCount;
}
/*
 * Allocate a new hashmap node with a 64-bit integer key.
 * If something goes wrong [i.e: out of memory],this function return NULL.
 * Otherwise a fresh [ph7_hashmap_node] instance is returned.
 */
static ph7_hashmap_node * HashmapNewIntNode(ph7_hashmap *pMap,sxi64 iKey,sxu32 nHash,sxu32 nValIdx)
{
	ph7_hashmap_node *pNode;
	/* Allocate a new node */
	pNode = (ph7_hashmap_node *)SyMemBackendPoolAlloc(&pMap->pVm->sAllocator,sizeof(ph7_hashmap_node));
	if( pNode == 0 ){
		return 0;
	}
	/* Zero the stucture */
	SyZero(pNode,sizeof(ph7_hashmap_node));
	/* Fill in the structure */
	pNode->pMap  = &(*pMap);
	pNode->iType = HASHMAP_INT_NODE;
	pNode->nHash = nHash;
	pNode->xKey.iKey = iKey;
	pNode->nValIdx  = nValIdx;
	return pNode;
}
/*
 * Allocate a new hashmap node with a BLOB key.
 * If something goes wrong [i.e: out of memory],this function return NULL.
 * Otherwise a fresh [ph7_hashmap_node] instance is returned.
 */
static ph7_hashmap_node * HashmapNewBlobNode(ph7_hashmap *pMap,const void *pKey,sxu32 nKeyLen,sxu32 nHash,sxu32 nValIdx)
{
	ph7_hashmap_node *pNode;
	/* Allocate a new node */
	pNode = (ph7_hashmap_node *)SyMemBackendPoolAlloc(&pMap->pVm->sAllocator,sizeof(ph7_hashmap_node));
	if( pNode == 0 ){
		return 0;
	}
	/* Zero the stucture */
	SyZero(pNode,sizeof(ph7_hashmap_node));
	/* Fill in the structure */
	pNode->pMap  = &(*pMap);
	pNode->iType = HASHMAP_BLOB_NODE;
	pNode->nHash = nHash;
	SyBlobInit(&pNode->xKey.sKey,&pMap->pVm->sAllocator);
	SyBlobAppend(&pNode->xKey.sKey,pKey,nKeyLen);
	pNode->nValIdx = nValIdx;
	return pNode;
}
/*
 * link a hashmap node to the given bucket index (last argument to this function).
 */
static void HashmapNodeLink(ph7_hashmap *pMap,ph7_hashmap_node *pNode,sxu32 nBucketIdx)
{
	/* Link */
	if( pMap->apBucket[nBucketIdx] != 0 ){
		pNode->pNextCollide = pMap->apBucket[nBucketIdx];
		pMap->apBucket[nBucketIdx]->pPrevCollide = pNode;
	}
	pMap->apBucket[nBucketIdx] = pNode;
	/* Link to the map list */
	if( pMap->pFirst == 0 ){
		pMap->pFirst = pMap->pLast = pNode;
		/* Point to the first inserted node */
		pMap->pCur = pNode;
	}else{
		MACRO_LD_PUSH(pMap->pLast,pNode);
	}
	if( pMap->pActiveSteps ){
		/* Re-arm any live foreach cursor parked past the end: php's by-ref
		 * foreach iterates the LIVE array, so an element appended while the
		 * loop stands on the last node (worklist idiom), or after the body
		 * emptied the map, is still visited. A registered step with a NULL
		 * cursor is always mid-loop — natural exhaustion unregisters before
		 * the loop ends. */
		ph7_foreach_step *pStep;
		for( pStep = pMap->pActiveSteps ; pStep ; pStep = pStep->pNextActive ){
			if( pStep->pCursor == 0 ){
				pStep->pCursor = pNode;
			}
		}
	}
	++pMap->nEntry;
}
/*
 * Unlink a node from the hashmap.
 * If the node count reaches zero then release the whole hash-bucket.
 */
PH7_PRIVATE void PH7_HashmapUnlinkNode(ph7_hashmap_node *pNode,int bRestore)
{
	ph7_hashmap *pMap = pNode->pMap;
	ph7_vm *pVm = pMap->pVm;
	/* Unlink from the corresponding bucket */
	if( pNode->pPrevCollide == 0 ){
		pMap->apBucket[pNode->nHash & (pMap->nSize - 1)] = pNode->pNextCollide;
	}else{
		pNode->pPrevCollide->pNextCollide = pNode->pNextCollide;
	}
	if( pNode->pNextCollide ){
		pNode->pNextCollide->pPrevCollide = pNode->pPrevCollide;
	}
	if( pMap->pFirst == pNode ){
		pMap->pFirst = pNode->pPrev;
	}
	if( pMap->pCur == pNode ){
		/* Advance the node cursor */
		pMap->pCur = pMap->pCur->pPrev; /* Reverse link */
	}
	if( pMap->pActiveSteps ){
		/* Advance any live foreach cursor parked on this node (delete during
		 * live-map iteration: by-ref foreach, $GLOBALS, snapshot fallbacks). */
		ph7_foreach_step *pStep;
		for( pStep = pMap->pActiveSteps ; pStep ; pStep = pStep->pNextActive ){
			if( pStep->pCursor == pNode ){
				pStep->pCursor = pNode->pPrev; /* Reverse link */
			}
		}
	}
	/* Unlink from the map list */
	MACRO_LD_REMOVE(pMap->pLast,pNode);
	if( bRestore ){
		/* Remove the ph7_value associated with this node from the reference table */
		PH7_VmRefObjRemove(pVm,pNode->nValIdx,0,pNode);
		/* Restore to the freelist */
		if( (pNode->iFlags & HASHMAP_NODE_FOREIGN_OBJ) == 0 ){
			PH7_VmUnsetMemObj(pVm,pNode->nValIdx,FALSE);
		}
	}
	if( pNode->iType == HASHMAP_BLOB_NODE ){
		SyBlobRelease(&pNode->xKey.sKey);
	}
	SyMemBackendPoolFree(&pVm->sAllocator,pNode);
	pMap->nEntry--;
	if( pMap->nEntry < 1 && pMap != pVm->pGlobal ){
		/* Free the hash-bucket */
		SyMemBackendFree(&pVm->sAllocator,pMap->apBucket);
		pMap->apBucket = 0;
		pMap->nSize = 0;
		pMap->pFirst = pMap->pLast = pMap->pCur = 0;
	}
}
#define HASHMAP_FILL_FACTOR 3
/*
 * Grow the hash-table and rehash all entries.
 */
static sxi32 HashmapGrowBucket(ph7_hashmap *pMap)
{
	if( pMap->nEntry >= pMap->nSize * HASHMAP_FILL_FACTOR ){
		ph7_hashmap_node **apOld = pMap->apBucket;
		ph7_hashmap_node *pEntry,**apNew;
		sxu32 nNew = pMap->nSize << 1;
		sxu32 nBucket;
		sxu32 n;
		if( nNew < 1 ){
			nNew = 16;
		}
		/* Allocate a new bucket */
		apNew = (ph7_hashmap_node **)SyMemBackendAlloc(&pMap->pVm->sAllocator,nNew * sizeof(ph7_hashmap_node *));
		if( apNew == 0 ){
			if( pMap->nSize < 1 ){
				return SXERR_MEM; /* Fatal */
			}
			/* Not so fatal here,simply a performance hit */
			return SXRET_OK;
		}
		/* Zero the table */
		SyZero((void *)apNew,nNew * sizeof(ph7_hashmap_node *));
		/* Reflect the change */
		pMap->apBucket = apNew;
		pMap->nSize = nNew;
		if( apOld == 0 ){
			/* First allocated table [i.e: no entry],return immediately */
			return SXRET_OK;
		}
		/* Rehash old entries */
		pEntry = pMap->pFirst;
		n = 0;
		for( ;; ){
			if( n >= pMap->nEntry ){
				break;
			}
			/* Clear the old collision link */
			pEntry->pNextCollide = pEntry->pPrevCollide = 0;
			/* Link to the new bucket */
			nBucket = pEntry->nHash & (nNew - 1);
			if( pMap->apBucket[nBucket] != 0 ){
				pEntry->pNextCollide = pMap->apBucket[nBucket];
				pMap->apBucket[nBucket]->pPrevCollide = pEntry;
			}
			pMap->apBucket[nBucket] = pEntry;
			/* Point to the next entry */
			pEntry = pEntry->pPrev; /* Reverse link */
			n++;
		}
		/* Free the old table */
		SyMemBackendFree(&pMap->pVm->sAllocator,(void *)apOld);
	}
	return SXRET_OK;
}
/*
 * Insert a 64-bit integer key and it's associated value (if any) in the given
 * hashmap.
 */
static sxi32 HashmapInsertIntKey(ph7_hashmap *pMap,sxi64 iKey,ph7_value *pValue,sxu32 nRefIdx,int isForeign)
{
	ph7_hashmap_node *pNode;
	sxu32 nIdx;
	sxu32 nHash;
	sxi32 rc;
	if( !isForeign ){
		ph7_value *pObj;
		ph7_value sSafeVal;
		/* Snapshot the source BEFORE reserving: PH7_ReserveMemObj can grow (move)
		 * pVm->aMemObj, which would dangle pValue when it points into the pool
		 * (e.g. get_defined_vars/func_get_args/get_class_vars/get_object_vars pass
		 * a pool slot). A shallow copy is a safe PH7_MemObjStore source — the
		 * referent and the heap-resident blob data survive the move; only the
		 * ph7_value struct relocates (same sSafeVal idiom used by PH7_HashmapDup). */
		if( pValue ){
			sSafeVal = *pValue;
			pValue = &sSafeVal;
		}
		/* Reserve a ph7_value for the value */
		pObj = PH7_ReserveMemObj(pMap->pVm);
		if( pObj == 0 ){
			return SXERR_MEM;
		}
		if( pValue ){
			/* Duplicate the value */
			PH7_MemObjStore(pValue,pObj);
		}
		nIdx = pObj->nIdx;
	}else{
		nIdx = nRefIdx;
	}
	/* Hash the key */
	nHash = pMap->xIntHash(iKey);
	/* Allocate a new int node */
	pNode = HashmapNewIntNode(&(*pMap),iKey,nHash,nIdx);
	if( pNode == 0 ){
		return SXERR_MEM;
	}
	if( isForeign ){
		/* Mark as a foregin entry */
		pNode->iFlags |= HASHMAP_NODE_FOREIGN_OBJ;
	}
	/* Make sure the bucket is big enough to hold the new entry */
	rc = HashmapGrowBucket(&(*pMap));
	if( rc != SXRET_OK ){
		SyMemBackendPoolFree(&pMap->pVm->sAllocator,pNode);
		return rc;
	}
	/* Perform the insertion */
	HashmapNodeLink(&(*pMap),pNode,nHash & (pMap->nSize - 1));
	/* Install in the reference table */
	PH7_VmRefObjInstall(pMap->pVm,nIdx,0,pNode,0);
	/* All done */
	return SXRET_OK;
}
/*
 * Insert a BLOB key and it's associated value (if any) in the given
 * hashmap.
 */
static sxi32 HashmapInsertBlobKey(ph7_hashmap *pMap,const void *pKey,sxu32 nKeyLen,ph7_value *pValue,sxu32 nRefIdx,int isForeign)
{
	ph7_hashmap_node *pNode;
	sxu32 nHash;
	sxu32 nIdx;
	sxi32 rc;
	if( !isForeign ){
		ph7_value *pObj;
		ph7_value sSafeVal;
		/* Snapshot the source BEFORE reserving: PH7_ReserveMemObj can grow (move)
		 * pVm->aMemObj, which would dangle pValue when it points into the pool
		 * (e.g. get_defined_vars/func_get_args/get_class_vars/get_object_vars pass
		 * a pool slot). A shallow copy is a safe PH7_MemObjStore source — the
		 * referent and the heap-resident blob data survive the move; only the
		 * ph7_value struct relocates (same sSafeVal idiom used by PH7_HashmapDup). */
		if( pValue ){
			sSafeVal = *pValue;
			pValue = &sSafeVal;
		}
		/* Reserve a ph7_value for the value */
		pObj = PH7_ReserveMemObj(pMap->pVm);
		if( pObj == 0 ){
			return SXERR_MEM;
		}
		if( pValue ){
			/* Duplicate the value */
			PH7_MemObjStore(pValue,pObj);
		}
		nIdx = pObj->nIdx;
	}else{
		nIdx = nRefIdx;
	}
	/* Hash the key */
	nHash = pMap->xBlobHash(pKey,nKeyLen);
	/* Allocate a new blob node */
	pNode = HashmapNewBlobNode(&(*pMap),pKey,nKeyLen,nHash,nIdx);
	if( pNode == 0 ){
		return SXERR_MEM;
	}
	if( isForeign ){
		/* Mark as a foregin entry */
		pNode->iFlags |= HASHMAP_NODE_FOREIGN_OBJ;
	}
	/* Make sure the bucket is big enough to hold the new entry */
	rc = HashmapGrowBucket(&(*pMap));
	if( rc != SXRET_OK ){
		SyMemBackendPoolFree(&pMap->pVm->sAllocator,pNode);
		return rc;
	}
	/* Perform the insertion */
	HashmapNodeLink(&(*pMap),pNode,nHash & (pMap->nSize - 1));
	/* Install in the reference table */
	PH7_VmRefObjInstall(pMap->pVm,nIdx,0,pNode,0);
	/* All done */
	return SXRET_OK;
}
/*
 * Check if a given 64-bit integer key exists in the given hashmap.
 * Write a pointer to the target node on success. Otherwise
 * SXERR_NOTFOUND is returned on failure.
 */
PH7_PRIVATE sxi32 HashmapLookupIntKey(
	ph7_hashmap *pMap,         /* Target hashmap */
	sxi64 iKey,                /* lookup key */
	ph7_hashmap_node **ppNode  /* OUT: target node on success */
	)
{
	ph7_hashmap_node *pNode;
	sxu32 nHash;
	if( pMap->nEntry < 1 ){
		/* Don't bother hashing,there is no entry anyway */
		return SXERR_NOTFOUND;
	}
	/* Hash the key first */
	nHash = pMap->xIntHash(iKey);
	/* Point to the appropriate bucket */
	pNode = pMap->apBucket[nHash & (pMap->nSize - 1)];
	/* Perform the lookup */
	for(;;){
		if( pNode == 0 ){
			break;
		}
		if( pNode->iType == HASHMAP_INT_NODE
			&& pNode->nHash == nHash
			&& pNode->xKey.iKey == iKey ){
				/* Node found */
				if( ppNode ){
					*ppNode = pNode;
				}
				return SXRET_OK;
		}
		/* Follow the collision link */
		pNode = pNode->pNextCollide;
	}
	/* No such entry */
	return SXERR_NOTFOUND;
}
/*
 * Check if a given BLOB key exists in the given hashmap.
 * Write a pointer to the target node on success. Otherwise
 * SXERR_NOTFOUND is returned on failure.
 */
PH7_PRIVATE sxi32 HashmapLookupBlobKey(
	ph7_hashmap *pMap,          /* Target hashmap */
	const void *pKey,           /* Lookup key */
	sxu32 nKeyLen,              /* Key length in bytes */
	ph7_hashmap_node **ppNode   /* OUT: target node on success */
	)
{
	ph7_hashmap_node *pNode;
	sxu32 nHash;
	if( pMap->nEntry < 1 ){
		/* Don't bother hashing,there is no entry anyway */
		return SXERR_NOTFOUND;
	}
	/* Hash the key first */
	nHash = pMap->xBlobHash(pKey,nKeyLen);
	/* Point to the appropriate bucket */
	pNode = pMap->apBucket[nHash & (pMap->nSize - 1)];
	/* Perform the lookup */
	for(;;){
		if( pNode == 0 ){
			break;
		}
		if( pNode->iType == HASHMAP_BLOB_NODE
			&& pNode->nHash == nHash
			&& SyBlobLength(&pNode->xKey.sKey) == nKeyLen
			&& SyMemcmp(SyBlobData(&pNode->xKey.sKey),pKey,nKeyLen) == 0 ){
				/* Node found */
				if( ppNode ){
					*ppNode = pNode;
				}
				return SXRET_OK;
		}
		/* Follow the collision link */
		pNode = pNode->pNextCollide;
	}
	/* No such entry */
	return SXERR_NOTFOUND;
}
/*
 * Check if the given BLOB key looks like a decimal number.
 * Retrurn TRUE on success.FALSE otherwise.
 */
static int HashmapIsIntKey(SyBlob *pKey)
{
	const char *zIn  = (const char *)SyBlobData(pKey);
	const char *zEnd = &zIn[SyBlobLength(pKey)];
	const char *zDigit;
	int isNeg = FALSE, nDigit;
	if( zIn >= zEnd ){
		return FALSE;
	}
	/* php's rule (_zend_handle_numeric_str_ex), byte for byte:
	 *   - a leading '-' is allowed, a leading '+' is NOT ('+1' stays a
	 *     string key)
	 *   - after the sign, a leading '0' disqualifies the key unless the
	 *     WHOLE key is the single digit "0" -- so "00", "01", "-0" and
	 *     "-01" all stay string keys. The length is measured over the key,
	 *     sign included, which is what makes "-0" fail.
	 * PH7 tested the leading zero BEFORE skipping the sign and accepted '+',
	 * so $a['-0'], $a['+0'], $a['+1'] and $a['-01'] canonicalised onto the
	 * integer keys 0/0/1/-1 -- silently COLLIDING with a genuine 0/1/-1 entry
	 * ($a = ['-0'=>'a','0'=>'d'] kept one element where php keeps two) and
	 * carrying the wrong key through array_keys/array_flip/json_decode/
	 * serialize/array_count_values alike. */
	if( zIn[0] == '-' && &zIn[1] < zEnd ){
		isNeg = TRUE;
		zIn++;
	}
	if( zIn < zEnd && zIn[0] == '0' && SyBlobLength(pKey) > 1 ){
		/* Leading zero: octal-looking, signed zero, or just padded */
		return FALSE;
	}
	zDigit = zIn;
	for(;;){
		if( zIn >= zEnd ){
			break;
		}
		if( (unsigned char)zIn[0] >= 0xc0 /* UTF-8 stream */  || !SyisDigit(zIn[0]) ){
			/* Key does not look like a decimal number */
			return FALSE;
		}
		zIn++;
	}
	/* An all-digit key that overflows the signed 64-bit range is NOT an integer
	 * key: php keeps it a string key (its (string)(int)$k === $k round-trip
	 * fails). Treating it as an int would let PH7_MemObjToInteger saturate it to
	 * PHP_INT_MAX/MIN and collide with the genuine boundary key. */
	nDigit = (int)(zEnd - zDigit);
	if( nDigit < 1 ){
		/* A lone "-" (the digit loop rejects it first; kept defensive) */
		return FALSE;
	}
	if( nDigit > 19 ||
		(nDigit == 19 && SyMemcmp(zDigit, isNeg ? "9223372036854775808" : "9223372036854775807", 19) > 0) ){
		return FALSE;
	}
	return TRUE;
}
/*
 * TRUE when this key value lands on an INTEGER key — the same fold HashmapLookup
 * and HashmapInsert perform below, exposed so a DIAGNOSTIC can name the key the
 * way the lookup saw it rather than the way it was written ($a["10"] misses the
 * integer key 10, so php's warning says `Undefined array key 10`, unquoted).
 * A non-integer key is left as a STRING with its blob ready to print — including
 * the NULL key, which folds to "" exactly as the lookup folds it.
 */
PH7_PRIVATE int PH7_HashmapKeyIsInt(ph7_value *pKey)
{
	if( pKey->iFlags & (MEMOBJ_STRING|MEMOBJ_HASHMAP|MEMOBJ_OBJ|MEMOBJ_RES|MEMOBJ_NULL) ){
		if( (pKey->iFlags & MEMOBJ_STRING) == 0 ){
			/* Force a string cast (NULL becomes "", php's empty-string key) */
			PH7_MemObjToString(&(*pKey));
		}
		return HashmapIsIntKey(&pKey->sBlob) ? TRUE : FALSE;
	}
	/* int / float / BOOL all reach an integer key ($a[false] is $a[0]) */
	return TRUE;
}
/*
 * Check if a given key exists in the given hashmap.
 * Write a pointer to the target node on success.
 * Otherwise SXERR_NOTFOUND is returned on failure.
 */
static sxi32 HashmapLookup(
	ph7_hashmap *pMap,          /* Target hashmap */
	ph7_value *pKey,            /* Lookup key */
	ph7_hashmap_node **ppNode   /* OUT: target node on success */
	)
{
	ph7_hashmap_node *pNode = 0; /* cc -O6 warning */
	sxi32 rc;
	if( pKey->iFlags & (MEMOBJ_STRING|MEMOBJ_HASHMAP|MEMOBJ_OBJ|MEMOBJ_RES|MEMOBJ_NULL) ){
		if( (pKey->iFlags & MEMOBJ_STRING) == 0 ){
			/* Force a string cast (NULL becomes "", php's empty-string key) */
			PH7_MemObjToString(&(*pKey));
		}
		if( !HashmapIsIntKey(&pKey->sBlob) ){
			/* Blob lookup. The EMPTY string is a real key here, symmetric with the insert
			 * path: reading $a[""] must find what writing $a[""] stored, not fall through
			 * to an integer lookup for key 0. */
			rc = HashmapLookupBlobKey(&(*pMap),SyBlobData(&pKey->sBlob),SyBlobLength(&pKey->sBlob),&pNode);
			goto result;
		}
	}
	/* Perform an int lookup */
	if((pKey->iFlags & MEMOBJ_INT) == 0 ){
		/* Force an integer cast */
		PH7_MemObjToInteger(pKey);
	}
	/* Perform an int lookup */
	rc = HashmapLookupIntKey(&(*pMap),pKey->x.iVal,&pNode);
result:
	if( rc == SXRET_OK ){
		/* Node found */
		if( ppNode ){
			*ppNode = pNode;
		}
		return SXRET_OK;
	}
	/* No such entry */
	return SXERR_NOTFOUND;
}
/*
 * Advance the auto-index after a successful insertion of int key iKey.
 * Mirrors Zend's nNextFreeElement: saturates at PHP_INT_MAX (incrementing
 * past it is signed overflow); the occupied-slot case errors at append time
 * via HashmapAppendIndexBusy.
 */
static void HashmapAdvanceAutoIndex(ph7_hashmap *pMap,sxi64 iKey)
{
	if( !pMap->bIntKeySeen ){
		/* php 8.3: the first integer key sets the auto-index even if it is negative */
		pMap->bIntKeySeen = 1;
		pMap->iNextIdx = iKey < SXI64_HIGH ? iKey + 1 : SXI64_HIGH;
		while( pMap->iNextIdx < SXI64_HIGH && SXRET_OK == HashmapLookupIntKey(&(*pMap),pMap->iNextIdx,0) ){
			pMap->iNextIdx++;
		}
		return;
	}
	if( iKey >= pMap->iNextIdx ){
		pMap->iNextIdx = iKey < SXI64_HIGH ? iKey + 1 : SXI64_HIGH;
		/* Make sure the automatic index is not reserved */
		while( pMap->iNextIdx < SXI64_HIGH && SXRET_OK == HashmapLookupIntKey(&(*pMap),pMap->iNextIdx,0) ){
			pMap->iNextIdx++;
		}
	}
}
/*
 * TRUE when an append (`$a[] = v`) cannot proceed because the saturated
 * auto-index slot (PHP_INT_MAX) is already occupied. Throws php's catchable
 * Error and stores the rc the insert function must return (PH7_EXCEPTION,
 * or PH7_ABORT when the Error class itself cannot be built).
 */
static sxi32 HashmapAppendIndexBusy(ph7_hashmap *pMap,sxi32 *pRc)
{
	if( pMap->iNextIdx == SXI64_HIGH && SXRET_OK == HashmapLookupIntKey(&(*pMap),pMap->iNextIdx,0) ){
		*pRc = PH7_VmThrowArrayNextIndexError(pMap->pVm);
		return TRUE;
	}
	return FALSE;
}
/*
 * Insert a given key and it's associated value (if any) in the given
 * hashmap.
 * If a node with the given key already exists in the database
 * then this function overwrite the old value.
 */
PH7_PRIVATE sxi32 HashmapInsert(
	ph7_hashmap *pMap, /* Target hashmap */
	ph7_value *pKey,   /* Lookup key  */
	ph7_value *pVal    /* Node value */
	)
{
	ph7_hashmap_node *pNode = 0;
	sxi32 rc = SXRET_OK;
	if( pKey && pKey->iFlags & (MEMOBJ_STRING|MEMOBJ_HASHMAP|MEMOBJ_OBJ|MEMOBJ_RES|MEMOBJ_NULL) ){
		if( (pKey->iFlags & MEMOBJ_STRING) == 0 ){
			/* Force a string cast. NULL casts to "": php stores $a[null] under the EMPTY
			 * STRING key, it does not treat it as an integer (PH7 fell through to the int
			 * path and filed it under 0). */
			PH7_MemObjToString(&(*pKey));
		}
		if( HashmapIsIntKey(&pKey->sBlob) ){
			goto IntKey;
		}
		/* An empty key is a real key: $a[""] = v stores under "", it does NOT
		 * auto-index (PH7 turned it into the next integer slot, silently
		 * overwriting nothing and bumping the auto-index). */
		if( SXRET_OK == HashmapLookupBlobKey(&(*pMap),SyBlobData(&pKey->sBlob),
			SyBlobLength(&pKey->sBlob),&pNode) ){
				/* Overwrite the old value */
				ph7_value *pElem;
				pElem = (ph7_value *)SySetAt(&pMap->pVm->aMemObj,pNode->nValIdx);
				if( pElem ){
					if( pVal ){
						PH7_MemObjStore(pVal,pElem);
					}else{
						/* Nullify the entry */
						PH7_MemObjToNull(pElem);
					}
				}
				return SXRET_OK;
		}
		if( pMap == pMap->pVm->pGlobal ){
			/* php 8.1: writing a new key into $GLOBALS creates a real global
			 * variable ($GLOBALS stays a live view of the symbol table). */
			if( SyBlobLength(&pKey->sBlob) < 1 ){
				/* Pathological empty name: keep the legacy diagnostic */
				PH7_VmThrowError(pMap->pVm,0,PH7_CTX_NOTICE,"$GLOBALS is a read-only array,insertion is forbidden");
				return SXRET_OK;
			}
			return PH7_VmInstallGlobalVar(pMap->pVm,
				(const char *)SyBlobData(&pKey->sBlob),SyBlobLength(&pKey->sBlob),
				pVal,SXU32_HIGH);
		}
		/* Perform a blob-key insertion */
		rc = HashmapInsertBlobKey(&(*pMap),SyBlobData(&pKey->sBlob),SyBlobLength(&pKey->sBlob),&(*pVal),0,FALSE);
		return rc;
	}
IntKey:
	if( pKey ){
		if((pKey->iFlags & MEMOBJ_INT) == 0 ){
			/* Force an integer cast */
			PH7_MemObjToInteger(pKey);
		}
		if( SXRET_OK == HashmapLookupIntKey(&(*pMap),pKey->x.iVal,&pNode) ){
			/* Overwrite the old value */
			ph7_value *pElem;
			pElem = (ph7_value *)SySetAt(&pMap->pVm->aMemObj,pNode->nValIdx);
			if( pElem ){
				if( pVal ){
					PH7_MemObjStore(pVal,pElem);
				}else{
					/* Nullify the entry */
					PH7_MemObjToNull(pElem);
				}
			}
			return SXRET_OK;
		}
		if( pMap == pMap->pVm->pGlobal ){
			/* php 8.1: an int key creates the global named by its decimal
			 * form ($GLOBALS[7] = ... behaves like $GLOBALS['7'] = ...). */
			char zKey[24];
			sxu32 nKey = SyBufferFormat(zKey,sizeof(zKey),"%qd",pKey->x.iVal);
			return PH7_VmInstallGlobalVar(pMap->pVm,zKey,nKey,pVal,SXU32_HIGH);
		}
		/* Perform a 64-bit-int-key insertion */
		rc = HashmapInsertIntKey(&(*pMap),pKey->x.iVal,&(*pVal),0,FALSE);
		if( rc == SXRET_OK ){
			HashmapAdvanceAutoIndex(&(*pMap),pKey->x.iVal);
		}
	}else{
		if( pMap == pMap->pVm->pGlobal ){
			/* php's catchable Error: Cannot append to $GLOBALS */
			return PH7_VmThrowGlobalsAppendError(pMap->pVm);
		}
		if( HashmapAppendIndexBusy(&(*pMap),&rc) ){
			return rc; /* PH7_EXCEPTION/PH7_ABORT: php's catchable Error was thrown */
		}
		/* Assign an automatic index */
		rc = HashmapInsertIntKey(&(*pMap),pMap->iNextIdx,&(*pVal),0,FALSE);
		if( rc == SXRET_OK && pMap->iNextIdx < SXI64_HIGH ){
			++pMap->iNextIdx;
		}
	}
	/* Insertion result */
	return rc;
}
/*
 * Insert a given key and it's associated value (foreign index) in the given
 * hashmap.
 * This is insertion by reference so be careful to mark the node
 * with the HASHMAP_NODE_FOREIGN_OBJ flag being set.
 * The insertion by reference is triggered when the following
 * expression is encountered.
 * $var = 10;
 *  $a = array(&var);
 * OR
 *  $a[] =& $var;
 * That is,$var is a foreign ph7_value and the $a array have no control
 * over it's contents.
 * Note that the node that hold the foreign ph7_value is automatically
 * removed when the foreign ph7_value is unset.
 * Example:
 *  $var = 10;
 *  $a[] =& $var;
 *  echo count($a).PHP_EOL; //1
 *  //Unset the foreign ph7_value now
 *  unset($var);
 *  echo count($a); //0
 * Note that this is a PH7 eXtension.
 * Refer to the official documentation for more information.
 * If a node with the given key already exists in the database
 * then this function overwrite the old value.
 */
static sxi32 HashmapInsertByRef(
	ph7_hashmap *pMap,   /* Target hashmap */
	ph7_value *pKey,     /* Lookup key */
	sxu32 nRefIdx        /* Foreign ph7_value index */
	)
{
	ph7_hashmap_node *pNode = 0;
	sxi32 rc = SXRET_OK;
	if( pKey && pKey->iFlags & (MEMOBJ_STRING|MEMOBJ_HASHMAP|MEMOBJ_OBJ|MEMOBJ_RES|MEMOBJ_NULL) ){
		if( (pKey->iFlags & MEMOBJ_STRING) == 0 ){
			/* Force a string cast. NULL casts to "": `$a[null] =& $x` binds under the
			 * EMPTY STRING key, symmetric with HashmapInsert (the by-value path). */
			PH7_MemObjToString(&(*pKey));
		}
		if( HashmapIsIntKey(&pKey->sBlob) ){
			goto IntKey;
		}
		/* An empty key is a REAL key: `$a[""] =& $x` binds (and OVERWRITES an existing
		 * "" element) under "", it does NOT auto-index. The legacy path turned "" into
		 * the next integer slot — `$a[""] =& $x` filed under 0 and a second write added
		 * a duplicate rather than rebinding. A genuine auto-index caller passes
		 * pKey == 0 (a literal null pointer), handled at IntKey below, never a "" blob. */
		if( SXRET_OK == HashmapLookupBlobKey(&(*pMap),SyBlobData(&pKey->sBlob),
			SyBlobLength(&pKey->sBlob),&pNode) ){
				/* Overwrite */
				PH7_VmRefObjRemove(pMap->pVm,pNode->nValIdx,0,pNode);
				pNode->nValIdx = nRefIdx;
				/* Install in the reference table */
				PH7_VmRefObjInstall(pMap->pVm,nRefIdx,0,pNode,0);
				return SXRET_OK;
		}
		/* Perform a blob-key insertion */
		rc = HashmapInsertBlobKey(&(*pMap),SyBlobData(&pKey->sBlob),SyBlobLength(&pKey->sBlob),0,nRefIdx,TRUE);
		return rc;
	}
IntKey:
	if( pKey ){
		if((pKey->iFlags & MEMOBJ_INT) == 0 ){
			/* Force an integer cast */
			PH7_MemObjToInteger(pKey);
		}
		if( SXRET_OK == HashmapLookupIntKey(&(*pMap),pKey->x.iVal,&pNode) ){
			/* Overwrite */
			PH7_VmRefObjRemove(pMap->pVm,pNode->nValIdx,0,pNode);
			pNode->nValIdx = nRefIdx;
			/* Install in the reference table */
			PH7_VmRefObjInstall(pMap->pVm,nRefIdx,0,pNode,0);
			return SXRET_OK;
		}
		/* Perform a 64-bit-int-key insertion */
		rc = HashmapInsertIntKey(&(*pMap),pKey->x.iVal,0,nRefIdx,TRUE);
		if( rc == SXRET_OK ){
			HashmapAdvanceAutoIndex(&(*pMap),pKey->x.iVal);
		}
	}else{
		if( HashmapAppendIndexBusy(&(*pMap),&rc) ){
			return rc; /* PH7_EXCEPTION/PH7_ABORT: php's catchable Error was thrown */
		}
		/* Assign an automatic index */
		rc = HashmapInsertIntKey(&(*pMap),pMap->iNextIdx,0,nRefIdx,TRUE);
		if( rc == SXRET_OK && pMap->iNextIdx < SXI64_HIGH ){
			++pMap->iNextIdx;
		}
	}
	/* Insertion result */
	return rc;
}
/*
 * Extract node value.
 */
PH7_PRIVATE ph7_value * HashmapExtractNodeValue(ph7_hashmap_node *pNode)
{
	/* Point to the desired object */
	ph7_value *pObj;
	pObj = (ph7_value *)SySetAt(&pNode->pMap->pVm->aMemObj,pNode->nValIdx);
	return pObj;
}
/*
 * Insert a node in the given hashmap.
 * If a node with the given key already exists in the database
 * then this function overwrite the old value.
 */
PH7_PRIVATE sxi32 HashmapInsertNode(ph7_hashmap *pMap,ph7_hashmap_node *pNode,int bPreserve)
{
	ph7_value *pObj;
	sxi32 rc;
	/* Extract the node value */
	pObj = HashmapExtractNodeValue(&(*pNode));
	if( pObj == 0 ){
		return SXERR_EMPTY;
	}
	if( (pNode->iFlags & HASHMAP_NODE_FOREIGN_OBJ)
	 || PH7_VmSlotIsReferenced(pMap->pVm,pNode->nValIdx) ){
		/* A referenced element keeps its reference through the copy (php: array_slice()
		 * of an array holding `$r = &$a[1]` still var_dumps that element as &int(2)).
		 * Same rule HashmapDuplicateNode applies for array_merge()/spread. */
		sxu32 nRefIdx = pNode->nValIdx;
		ph7_value sKey;
		if( pNode->iType == HASHMAP_INT_NODE ){
			if( !bPreserve ){
				return HashmapInsertByRef(&(*pMap),0 /* auto index */,nRefIdx);
			}
			PH7_MemObjInitFromInt(pMap->pVm,&sKey,pNode->xKey.iKey);
		}else{
			if( !bPreserve ){
				return HashmapInsertByRef(&(*pMap),0 /* auto index */,nRefIdx);
			}
			PH7_MemObjInitFromString(pMap->pVm,&sKey,0);
			PH7_MemObjStringAppend(&sKey,(const char *)SyBlobData(&pNode->xKey.sKey),
				SyBlobLength(&pNode->xKey.sKey));
		}
		rc = HashmapInsertByRef(&(*pMap),&sKey,nRefIdx);
		PH7_MemObjRelease(&sKey);
		return rc;
	}
	/* Preserve key */
	if( pNode->iType == HASHMAP_INT_NODE){
		/* Int64 key */
		if( !bPreserve ){
			/* Assign an automatic index */
			rc = HashmapInsert(&(*pMap),0,pObj);
		}else{
			rc = HashmapInsertIntKey(&(*pMap),pNode->xKey.iKey,pObj,0,FALSE);
		}
	}else{
		/* Blob key */
		if( !bPreserve ){
			/* treat it like an automatically-indexed element, drop the
			 * original string key entirely */
			rc = HashmapInsert(&(*pMap),0,pObj);
		}else{
			rc = HashmapInsertBlobKey(&(*pMap),SyBlobData(&pNode->xKey.sKey),
				SyBlobLength(&pNode->xKey.sKey),pObj,0,FALSE);
		}
	}
	return rc;
}
/*
 * Compare two node values.
 * Return 0 if the node values are equals, > 0 if pLeft is greater than pRight
 * or < 0 if pRight is greater than pLeft.
 * For a full description on ph7_values comparison,refer to the implementation
 * of the [PH7_MemObjCmp()] function defined in memobj.c or the official
 * documenation.
 */
PH7_PRIVATE sxi32 HashmapNodeCmp(ph7_hashmap_node *pLeft,ph7_hashmap_node *pRight,int bStrict)
{
	ph7_value sObj1,sObj2;
	sxi32 rc;
	if( pLeft == pRight ){
		/*
		 * Same node.Refer to the sort() implementation defined
		 * below for more information on this sceanario.
		 */
		return 0;
	}
	/* Do the comparison */
	PH7_MemObjInit(pLeft->pMap->pVm,&sObj1);
	PH7_MemObjInit(pLeft->pMap->pVm,&sObj2);
	PH7_HashmapExtractNodeValue(pLeft,&sObj1,FALSE);
	PH7_HashmapExtractNodeValue(pRight,&sObj2,FALSE);
	rc = PH7_MemObjCmp(&sObj1,&sObj2,bStrict,0);
	PH7_MemObjRelease(&sObj1);
	PH7_MemObjRelease(&sObj2);
	return rc;
}
/*
 * Rehash a node with a 64-bit integer key.
 * Refer to [merge_sort(),array_shift()] implementations for more information.
 */
PH7_PRIVATE void HashmapRehashIntNode(ph7_hashmap_node *pEntry)
{
	ph7_hashmap *pMap = pEntry->pMap;
	sxu32 nBucket;
	/* Remove old collision links */
	if( pEntry->pPrevCollide ){
		pEntry->pPrevCollide->pNextCollide = pEntry->pNextCollide;
	}else{
		pMap->apBucket[pEntry->nHash & (pMap->nSize - 1)] = pEntry->pNextCollide;
	}
	if( pEntry->pNextCollide ){
		pEntry->pNextCollide->pPrevCollide = pEntry->pPrevCollide;
	}
	pEntry->pNextCollide = pEntry->pPrevCollide = 0;
	/* Compute the new hash */
	pEntry->nHash = pMap->xIntHash(pMap->iNextIdx);
	pEntry->xKey.iKey = pMap->iNextIdx;
	nBucket = pEntry->nHash & (pMap->nSize - 1);
	/* Link to the new bucket */
	pEntry->pNextCollide = pMap->apBucket[nBucket];
	if( pMap->apBucket[nBucket] ){
		pMap->apBucket[nBucket]->pPrevCollide = pEntry;
	}
	pEntry->pNextCollide = pMap->apBucket[nBucket];
	pMap->apBucket[nBucket] = pEntry;
	/* Increment the automatic index (saturating, like every other advance —
	 * unreachable in practice since renumbering assigns 0..nEntry-1, but keep
	 * the no-overflow invariant uniform). */
	if( pMap->iNextIdx < SXI64_HIGH ){
		pMap->iNextIdx++;
	}
}
/*
 * Perform a linear search on a given hashmap.
 * Write a pointer to the target node on success.
 * Otherwise SXERR_NOTFOUND is returned on failure.
 * Refer to [array_intersect(),array_diff(),in_array(),...] implementations
 * for more information.
 */
PH7_PRIVATE int HashmapFindValue(
	ph7_hashmap *pMap,   /* Target hashmap */
	ph7_value *pNeedle,  /* Lookup key */
	ph7_hashmap_node **ppNode, /* OUT: target node on success  */
	int bStrict      /* TRUE for strict comparison */
	)
{
	ph7_hashmap_node *pEntry;
	ph7_value sVal,*pVal;
	ph7_value sNeedle;
	sxi32 rc;
	sxu32 n;
	/* Perform a linear search since we cannot sort the hashmap based on values */
	pEntry = pMap->pFirst;
	n = pMap->nEntry;
	PH7_MemObjInit(pMap->pVm,&sVal);
	PH7_MemObjInit(pMap->pVm,&sNeedle);
	for(;;){
		if( n < 1 ){
			break;
		}
		/* Extract node value */
		pVal = HashmapExtractNodeValue(pEntry);
		if( pVal ){
			/* Compare on duplicates (PH7_MemObjCmp converts its operands in
			 * place). PH7_MemObjCmp implements php's full comparison table for
			 * null too — loose null == ""/0/false, strict null === null only —
			 * so null needles/values take the same path as everything else
			 * (the historical null-to-null shortcut here made
			 * in_array(null, [""]) false where php says true). */
			PH7_MemObjLoad(pVal,&sVal);
			PH7_MemObjLoad(pNeedle,&sNeedle);
			rc = PH7_MemObjCmp(&sNeedle,&sVal,bStrict,0);
			PH7_MemObjRelease(&sVal);
			PH7_MemObjRelease(&sNeedle);
			if( rc == 0 ){
				if( ppNode ){
					*ppNode = pEntry;
				}
				/* Match found*/
				return SXRET_OK;
			}
		}
		/* Point to the next entry */
		pEntry = pEntry->pPrev; /* Reverse link */
		n--;
	}
	/* No such entry */
	return SXERR_NOTFOUND;
}
/*
 * The element comparison array_diff()/array_intersect() and their _assoc pair
 * use, which is NOT the engine's value comparison: php's manual defines all four
 * as
 *     (string)$elem1 === (string)$elem2
 * a PURE string comparison — not numeric-string aware, so array_diff(["10"],
 * ["1e1"]) keeps "10" — where PHL used to call PH7_MemObjCmp with bStrict. That
 * made no int ever match its own decimal string, so array_diff([1,2,3],
 * ["1","2"]) answered the whole first array instead of [2=>3].
 *
 * bUserVisible picks the coercion: TRUE emits php's user-visible diagnostics (an
 * ARRAY element warns "Array to string conversion", an object with no
 * __toString() throws the catchable "could not be converted to string" Error,
 * reported through *pRc so the builtin answers the throw instead of a result),
 * FALSE renders silently. The two diff families need different answers there:
 * the _assoc pair converts LAZILY, only when a key matched, so it coerces
 * user-visibly right here; array_diff/array_intersect convert every element of
 * every input array up front (php sorts them), so those pre-pass with
 * HashmapStringifyElems and compare silently afterwards — which is what makes
 * the warning COUNT and the "throws even though an earlier element matched"
 * behaviour come out php-exact.
 *
 * Both operands are coerced on COPIES: these are live array elements, and a
 * diff must not rewrite the caller's array.
 */
PH7_PRIVATE int HashmapValueStrEq(ph7_value *pA,ph7_value *pB,int bUserVisible,sxi32 *pRc)
{
	ph7_value sA,sB;
	int bEq = FALSE;
	sxi32 rc;
	*pRc = SXRET_OK;
	/* Two fast paths that need no rendering at all, because each type's string
	 * form is canonical and injective: two STRINGS already ARE their string form,
	 * and two INTS are string-equal exactly when they are equal. Without them
	 * array_diff() over a pair of integer ranges formatted both operands of every
	 * one of its O(n*m) comparisons (~4x slower than the strict compare it
	 * replaced). A value carrying MEMOBJ_INT alongside MEMOBJ_REAL is an integral
	 * FLOAT, whose "1" can equal an int's — the mask sends it down the slow path
	 * rather than comparing rVal-derived iVal, and bools/null/resources likewise. */
	if( (pA->iFlags & MEMOBJ_STRING) && (pB->iFlags & MEMOBJ_STRING) ){
		return SyBlobLength(&pA->sBlob) == SyBlobLength(&pB->sBlob)
		    && ( SyBlobLength(&pA->sBlob) == 0
		      || SyMemcmp(SyBlobData(&pA->sBlob),SyBlobData(&pB->sBlob),
		                  SyBlobLength(&pA->sBlob)) == 0 );
	}
	if( (pA->iFlags & (MEMOBJ_INT|MEMOBJ_REAL|MEMOBJ_STRING)) == MEMOBJ_INT
	 && (pB->iFlags & (MEMOBJ_INT|MEMOBJ_REAL|MEMOBJ_STRING)) == MEMOBJ_INT ){
		return pA->x.iVal == pB->x.iVal;
	}
	PH7_MemObjInit(pA->pVm,&sA);
	PH7_MemObjInit(pA->pVm,&sB);
	PH7_MemObjLoad(pA,&sA);
	PH7_MemObjLoad(pB,&sB);
	rc = bUserVisible ? PH7_MemObjToStringUV(&sA) : PH7_MemObjToString(&sA);
	if( rc == SXRET_OK ){
		rc = bUserVisible ? PH7_MemObjToStringUV(&sB) : PH7_MemObjToString(&sB);
	}
	if( rc != SXRET_OK ){
		*pRc = rc;
	}else if( SyBlobLength(&sA.sBlob) == SyBlobLength(&sB.sBlob) ){
		bEq = SyBlobLength(&sA.sBlob) == 0
		   || SyMemcmp(SyBlobData(&sA.sBlob),SyBlobData(&sB.sBlob),SyBlobLength(&sA.sBlob)) == 0;
	}
	PH7_MemObjRelease(&sA);
	PH7_MemObjRelease(&sB);
	return bEq;
}
/*
 * Run the USER-VISIBLE string coercion over every element of pMap once, in
 * insertion order, discarding the result: php's array_diff/array_intersect sort
 * each input array, which converts every element exactly once, so this is where
 * their "Array to string conversion" warnings and their not-stringable-object
 * Error come from. Doing it as a pre-pass is what lets
 * array_diff([1,2],[1,new P()]) throw the way php's does even though the first
 * element already matched. Returns the throw status, SXRET_OK otherwise.
 */
PH7_PRIVATE sxi32 HashmapStringifyElems(ph7_hashmap *pMap)
{
	ph7_hashmap_node *pEntry = pMap->pFirst;
	sxu32 n = pMap->nEntry;
	while( n > 0 && pEntry ){
		ph7_value *pVal = HashmapExtractNodeValue(pEntry);
		if( pVal && (pVal->iFlags & MEMOBJ_STRING) == 0 ){
			ph7_value sTmp;
			sxi32 rc;
			PH7_MemObjInit(pMap->pVm,&sTmp);
			PH7_MemObjLoad(pVal,&sTmp);
			rc = PH7_MemObjToStringUV(&sTmp);
			PH7_MemObjRelease(&sTmp);
			if( rc != SXRET_OK ){
				return rc;
			}
		}
		pEntry = pEntry->pPrev; /* Reverse link — insertion order */
		n--;
	}
	return SXRET_OK;
}
/*
 * Perform a linear search on a given hashmap, comparing values the way
 * array_diff()/array_intersect() do (see HashmapValueStrEq). Writes a pointer to
 * the target node on success; SXERR_NOTFOUND otherwise, with *pRc carrying the
 * status of a coercion that threw.
 */
PH7_PRIVATE int HashmapFindStringValue(
	ph7_hashmap *pMap,   /* Target hashmap */
	ph7_value *pNeedle,  /* Lookup value */
	ph7_hashmap_node **ppNode, /* OUT: target node on success */
	sxi32 *pRc           /* OUT: coercion status */
	)
{
	ph7_hashmap_node *pEntry = pMap->pFirst;
	sxu32 n = pMap->nEntry;
	*pRc = SXRET_OK;
	while( n > 0 && pEntry ){
		ph7_value *pVal = HashmapExtractNodeValue(pEntry);
		if( pVal ){
			if( HashmapValueStrEq(pNeedle,pVal,/*bUserVisible*/0,pRc) ){
				if( ppNode ){
					*ppNode = pEntry;
				}
				return SXRET_OK;
			}
			if( *pRc != SXRET_OK ){
				return SXERR_NOTFOUND;
			}
		}
		pEntry = pEntry->pPrev; /* Reverse link */
		n--;
	}
	return SXERR_NOTFOUND;
}
/*
 * Perform a linear search on a given hashmap but use an user-defined callback
 * for values comparison.
 * Write a pointer to the target node on success.
 * Otherwise SXERR_NOTFOUND is returned on failure.
 * Refer to [array_uintersect(),array_udiff()...] implementations
 * for more information.
 */
PH7_PRIVATE int HashmapFindValueByCallback(
	ph7_hashmap *pMap,     /* Target hashmap */
	ph7_value *pNeedle,    /* Lookup key */
	ph7_value *pCallback,  /* User defined callback */
	ph7_hashmap_node **ppNode /* OUT: target node on success */
	)
{
	ph7_hashmap_node *pEntry;
	ph7_value sResult,*pVal;
	ph7_value *apArg[2];    /* Callback arguments */
	sxi32 rc;
	sxu32 n;
	if( pMap->pVm->iCmpCallbackExc ){
		/* A previous comparison already raised: stop invoking the callback so the
		 * exception is not thrown again, and let the caller wind down. */
		return SXERR_NOTFOUND;
	}
	/* Perform a linear search since we cannot sort the array based on values */
	pEntry = pMap->pFirst;
	n = pMap->nEntry;
	/* Store callback result here */
	PH7_MemObjInit(pMap->pVm,&sResult);
	/* First argument to the callback */
	apArg[0] = pNeedle;
	for(;;){
		if( n < 1 ){
			break;
		}
		/* Extract node value */
		pVal = HashmapExtractNodeValue(pEntry);
		if( pVal ){
			/* Invoke the user callback */
			apArg[1] = pVal; /* Second argument to the callback */
			rc = PH7_VmCallUserFunction(pMap->pVm,pCallback,2,apArg,&sResult);
			if( rc == PH7_EXCEPTION ){
				/* The callback raised: flag it so the caller aborts and propagates,
				 * and report no match for the rest of the run. */
				pMap->pVm->iCmpCallbackExc = 1;
				PH7_MemObjRelease(&sResult);
				return SXERR_NOTFOUND;
			}
			if( rc == SXRET_OK ){
				/* Extract callback result */
				if( (sResult.iFlags & MEMOBJ_INT) == 0 ){
					/* Perform an int cast */
					PH7_MemObjToInteger(&sResult);
				}
				rc = (sxi32)sResult.x.iVal;
				PH7_MemObjRelease(&sResult);
				if( rc == 0 ){
					/* Match found*/
					if( ppNode ){
						*ppNode = pEntry;
					}
					return SXRET_OK;
				}
			}
		}
		/* Point to the next entry */
		pEntry = pEntry->pPrev; /* Reverse link */
		n--;
	}
	/* No such entry */
	return SXERR_NOTFOUND;
}
/*
 * Compare two hashmaps.
 * Return 0 if the hashmaps are equals.Any other value indicates inequality.
 * Note on array comparison operators.
 *  According to the PHP language reference manual.
 *  Array Operators Example 	Name 	Result
 *  $a + $b 	Union 	Union of $a and $b.
 *  $a == $b 	Equality 	TRUE if $a and $b have the same key/value pairs.
 *  $a === $b 	Identity 	TRUE if $a and $b have the same key/value pairs in the same
 *                          order and of the same types.
 *  $a != $b 	Inequality 	TRUE if $a is not equal to $b.
 *  $a <> $b 	Inequality 	TRUE if $a is not equal to $b.
 *  $a !== $b 	Non-identity 	TRUE if $a is not identical to $b.
 * The + operator returns the right-hand array appended to the left-hand array;
 * For keys that exist in both arrays, the elements from the left-hand array will be used
 * and the matching elements from the right-hand array will be ignored.
 * <?php
 * $a = array("a" => "apple", "b" => "banana");
 * $b = array("a" => "pear", "b" => "strawberry", "c" => "cherry");
 * $c = $a + $b; // Union of $a and $b
 * echo "Union of \$a and \$b: \n";
 * var_dump($c);
 * $c = $b + $a; // Union of $b and $a
 * echo "Union of \$b and \$a: \n";
 * var_dump($c);
 * ?>
 * When executed, this script will print the following:
 * Union of $a and $b:
 * array(3) {
 *  ["a"]=>
 *  string(5) "apple"
 *  ["b"]=>
 * string(6) "banana"
 *  ["c"]=>
 * string(6) "cherry"
 * }
 * Union of $b and $a:
 * array(3) {
 * ["a"]=>
 * string(4) "pear"
 * ["b"]=>
 * string(10) "strawberry"
 * ["c"]=>
 * string(6) "cherry"
 * }
 * Elements of arrays are equal for the comparison if they have the same key and value.
 */
PH7_PRIVATE sxi32 PH7_HashmapCmp(
	ph7_hashmap *pLeft,  /* Left hashmap */
	ph7_hashmap *pRight, /* Right hashmap */
	int bStrict          /* TRUE for strict comparison */
	)
{
	ph7_hashmap_node *pLe,*pRe;
	sxi32 rc;
	sxu32 n;
	if( pLeft == pRight ){
		/* Same hashmap instance. This can easily happen since hashmaps are passed by reference.
		 * Unlike the zend engine.
		 */
		return 0;
	}
	if( pLeft->nEntry != pRight->nEntry ){
		/* Must have the same number of entries */
		return pLeft->nEntry > pRight->nEntry ? 1 : -1;
	}
	if( bStrict ){
		/* PHP's '===' on arrays is ORDER-SENSITIVE: the two maps must hold the
		 * same key/value pairs, with identical key types, in the same insertion
		 * order. Walk both in insertion order (pFirst, then the pPrev chain, per
		 * this file's forward-iteration convention) in lockstep and compare each
		 * position's key then value. (Loose '==' below stays order-insensitive,
		 * matching each left key by lookup into the right map.) */
		ph7_hashmap_node *pLs = pLeft->pFirst;
		ph7_hashmap_node *pRs = pRight->pFirst;
		for( n = pLeft->nEntry ; n > 0 ; n-- ){
			/* Keys must match in type and value at this position */
			if( pLs->iType != pRs->iType ){
				return 1;
			}
			if( pLs->iType == HASHMAP_INT_NODE ){
				if( pLs->xKey.iKey != pRs->xKey.iKey ){
					return 1;
				}
			}else{
				SyBlob *pLk = &pLs->xKey.sKey;
				SyBlob *pRk = &pRs->xKey.sKey;
				if( SyBlobLength(pLk) != SyBlobLength(pRk)
				 || (SyBlobLength(pLk) > 0
				  && SyMemcmp(SyBlobData(pLk),SyBlobData(pRk),SyBlobLength(pLk)) != 0) ){
					return 1;
				}
			}
			/* Values must be strictly identical */
			if( HashmapNodeCmp(pLs,pRs,TRUE) != 0 ){
				return 1;
			}
			pLs = pLs->pPrev; /* Reverse link = insertion order */
			pRs = pRs->pPrev;
		}
		return 0; /* Same pairs, same order */
	}
	/* Point to the first inserted entry of the left hashmap */
	pLe = pLeft->pFirst;
	pRe = 0; /* cc warning */
	/* Perform the comparison */
	n = pLeft->nEntry;
	for(;;){
		if( n < 1 ){
			break;
		}
		if( pLe->iType == HASHMAP_INT_NODE){
			/* Int key */
			rc = HashmapLookupIntKey(&(*pRight),pLe->xKey.iKey,&pRe);
		}else{
			SyBlob *pKey = &pLe->xKey.sKey;
			/* Blob key */
			rc = HashmapLookupBlobKey(&(*pRight),SyBlobData(pKey),SyBlobLength(pKey),&pRe);
		}
		if( rc != SXRET_OK ){
			/* No such entry in the right side */
			return 1;
		}
		rc = 0;
		if( bStrict ){
			/* Make sure,the keys are of the same type */
			if( pLe->iType != pRe->iType ){
				rc = 1;
			}
		}
		if( !rc ){
			/* Compare nodes */
			rc = HashmapNodeCmp(pLe,pRe,bStrict);
		}
		if( rc != 0 ){
			/* Nodes key/value differ */
			return rc;
		}
		/* Point to the next entry */
		pLe = pLe->pPrev; /* Reverse link */
		n--;
	}
	return 0; /* Hashmaps are equals */
}
/*
 * Duplicate a hashmap node.
 * This function is used by HashmapMerge, HashmapOverwrite and PH7_HashmapDup.
 */
static sxi32 HashmapDuplicateNode(
	ph7_hashmap *pDest,
	ph7_hashmap_node *pEntry,
	ph7_value *pVal,
	int iAction /* 0: Merge, 1: Overwrite, 2: Dup */
	)
{
	ph7_value sSafeVal;
	ph7_value sKey;
	sxi32 rc;

	if( (pEntry->iFlags & HASHMAP_NODE_FOREIGN_OBJ)
	 || PH7_VmSlotIsReferenced(pDest->pVm,pEntry->nValIdx) ){
		/* The source node is a reference — either a FOREIGN one (`[&$x]`, the node points
		 * at an outside slot) or, the case PH7 missed, an element somebody took a
		 * reference TO (`$r = &$a[1]`). php carries an element's reference bit through
		 * array COPIES, so array_merge()/array_slice()/array_replace()/spread all keep
		 * var_dump'ing it as `&int(2)`; flattening it to a value copy lost that. */
		sxu32 nRefIdx = pEntry->nValIdx;
		if( pEntry->iType == HASHMAP_BLOB_NODE ){
			PH7_MemObjInitFromString(pDest->pVm,&sKey,0);
			PH7_MemObjStringAppend(&sKey,(const char *)SyBlobData(&pEntry->xKey.sKey),SyBlobLength(&pEntry->xKey.sKey));
			rc = HashmapInsertByRef(pDest,&sKey,nRefIdx);
			PH7_MemObjRelease(&sKey);
		}else{
			if( iAction == 0 ){ /* Merge: automatic index assign */
				rc = HashmapInsertByRef(pDest,0,nRefIdx);
			}else if( iAction == 1 ){ /* Overwrite: keep the int key */
				PH7_MemObjInitFromInt(pDest->pVm,&sKey,pEntry->xKey.iKey);
				rc = HashmapInsertByRef(pDest,&sKey,nRefIdx);
				PH7_MemObjRelease(&sKey);
			}else{ /* Dup: preserve the int key */
				rc = HashmapInsertIntKey(pDest,pEntry->xKey.iKey,0,nRefIdx,TRUE);
			}
		}
		return rc;
	}
	sSafeVal = *pVal;

	if( pEntry->iType == HASHMAP_BLOB_NODE ){
		/* Blob key insertion */
		PH7_MemObjInitFromString(pDest->pVm,&sKey,0);
		PH7_MemObjStringAppend(&sKey,(const char *)SyBlobData(&pEntry->xKey.sKey),SyBlobLength(&pEntry->xKey.sKey));
		rc = PH7_HashmapInsert(pDest,&sKey,&sSafeVal);
		PH7_MemObjRelease(&sKey);
	}else{
		/* Int key */
		if( iAction == 0 ){ /* Merge */
			rc = HashmapInsert(pDest,0/* Automatic index assign */,&sSafeVal);
		}else if( iAction == 1 ){ /* Overwrite */
			PH7_MemObjInitFromInt(pDest->pVm,&sKey,pEntry->xKey.iKey);
			rc = PH7_HashmapInsert(pDest,&sKey,&sSafeVal);
			PH7_MemObjRelease(&sKey);
		}else{ /* Dup */
			rc = HashmapInsertIntKey(pDest,pEntry->xKey.iKey,&sSafeVal,0,FALSE);
		}
	}
	return rc;
}
/*
 * Merge two hashmaps.
 * Note on the merge process
 * According to the PHP language reference manual.
 *  Merges the elements of two arrays together so that the values of one are appended
 *  to the end of the previous one. It returns the resulting array (pDest).
 *  If the input arrays have the same string keys, then the later value for that key
 *  will overwrite the previous one. If, however, the arrays contain numeric keys
 *  the later value will not overwrite the original value, but will be appended.
 *  Values in the input array with numeric keys will be renumbered with incrementing
 *  keys starting from zero in the result array.
 */
PH7_PRIVATE sxi32 HashmapMerge(ph7_hashmap *pSrc,ph7_hashmap *pDest)
{
	ph7_hashmap_node *pEntry;
	ph7_value *pVal;
	sxi32 rc;
	sxu32 n;
	if( pSrc == pDest ){
		/* Same map. This can easily happen since hashmaps are passed by reference.
		 * Unlike the zend engine.
		 */
		return SXRET_OK;
	}
	/* Point to the first inserted entry in the source */
	pEntry = pSrc->pFirst;
	/* Perform the merge */
	for( n = 0 ; n < pSrc->nEntry ; ++n ){
		/* Extract the node value */
		pVal = HashmapExtractNodeValue(pEntry);
		if( pVal ){
			/* Make a local copy of the value.
			 * The insertion call below may trigger a memory pool reallocation
			 * which will invalidate the 'pVal' pointer since it points
			 * to the old pool.
			 */
			rc = HashmapDuplicateNode(pDest,pEntry,pVal,0);
		}else{
			rc = SXRET_OK;
		}
		if( rc != SXRET_OK ){
			return rc;
		}
		/* Point to the next entry */
		pEntry = pEntry->pPrev; /* Reverse link */
	}
	return SXRET_OK;
}
/*
 * Overwrite entries with the same key.
 * Refer to the [array_replace()] implementation for more information.
 *  According to the PHP language reference manual.
 *  array_replace() replaces the values of the first array with the same values
 *  from all the following arrays. If a key from the first array exists in the second
 *  array, its value will be replaced by the value from the second array. If the key
 *  exists in the second array, and not the first, it will be created in the first array.
 *  If a key only exists in the first array, it will be left as is. If several arrays
 *  are passed for replacement, they will be processed in order, the later arrays
 *  overwriting the previous values.
 *  array_replace() is not recursive : it will replace values in the first array
 *  by whatever type is in the second array.
 */
PH7_PRIVATE sxi32 HashmapOverwrite(ph7_hashmap *pSrc,ph7_hashmap *pDest)
{
	ph7_hashmap_node *pEntry;
	ph7_value *pVal;
	sxi32 rc;
	sxu32 n;
	if( pSrc == pDest ){
		/* Same map. This can easily happen since hashmaps are passed by reference.
		 * Unlike the zend engine.
		 */
		return SXRET_OK;
	}
	/* Point to the first inserted entry in the source */
	pEntry = pSrc->pFirst;
	/* Perform the merge */
	for( n = 0 ; n < pSrc->nEntry ; ++n ){
		/* Extract the node value */
		pVal = HashmapExtractNodeValue(pEntry);
		if( pVal ){
			rc = HashmapDuplicateNode(pDest,pEntry,pVal,1);
		}else{
			rc = SXRET_OK;
		}
		if( rc != SXRET_OK ){
			return rc;
		}
		/* Point to the next entry */
		pEntry = pEntry->pPrev; /* Reverse link */
	}
	return SXRET_OK;
}
/*
 * Duplicate the contents of a hashmap. Store the copy in pDest.
 * Refer to the [array_pad(),array_copy(),...] implementation for more information.
 */
PH7_PRIVATE sxi32 PH7_HashmapDup(ph7_hashmap *pSrc,ph7_hashmap *pDest)
{
	ph7_hashmap_node *pEntry;
	ph7_value *pVal;
	sxi32 rc;
	sxu32 n;
	if( pSrc == pDest ){
		/* Same map. This can easily happen since hashmaps are passed by reference.
		 * Unlike the zend engine.
		 */
		return SXRET_OK;
	}
	/* Point to the first inserted entry in the source */
	pEntry = pSrc->pFirst;
	/* Perform the duplication */
	for( n = 0 ; n < pSrc->nEntry ; ++n ){
		/* Extract the node value */
		pVal = HashmapExtractNodeValue(pEntry);
		if( pVal ){
			rc = HashmapDuplicateNode(pDest,pEntry,pVal,2);
		}else{
			rc = SXRET_OK;
		}
		if( rc != SXRET_OK ){
			return rc;
		}
		/* Point to the next entry */
		pEntry = pEntry->pPrev; /* Reverse link */
	}
	return SXRET_OK;
}
/*
 * Duplicate a hashmap, flattening every foreign (by-reference) node into a
 * plain value copy. php 8.1 gives a COPY of $GLOBALS pure value semantics
 * ($snap = $GLOBALS snapshots the symbol table: later writes on either side
 * never affect the other) — unlike ordinary array copies, where reference
 * elements stay live — so the $GLOBALS store path (PH7_MemObjStore) uses
 * this instead of PH7_HashmapDup.
 */
PH7_PRIVATE sxi32 PH7_HashmapDupMaterialized(ph7_hashmap *pSrc,ph7_hashmap *pDest)
{
	ph7_hashmap_node *pEntry;
	ph7_value *pVal;
	sxi32 rc;
	sxu32 n;
	if( pSrc == pDest ){
		return SXRET_OK;
	}
	pEntry = pSrc->pFirst;
	for( n = 0 ; n < pSrc->nEntry ; ++n ){
		/* Extract the node value (resolves foreign references) */
		pVal = HashmapExtractNodeValue(pEntry);
		if( pVal && (pVal->iFlags & MEMOBJ_HASHMAP)
		 && (ph7_hashmap *)pVal->x.pOther == pSrc->pVm->pGlobal ){
			/* A global still holding the live $GLOBALS map is the snapshot's
			 * own destination mid-store ($snap = $GLOBALS registers $snap
			 * before the value lands). php's snapshot — taken when $GLOBALS
			 * is READ, before the assignment — has no such entry, so skip it
			 * (also breaks the would-be infinite recursion). */
			pVal = 0;
		}
		if( pVal ){
			if( pEntry->iType == HASHMAP_BLOB_NODE ){
				rc = HashmapInsertBlobKey(&(*pDest),SyBlobData(&pEntry->xKey.sKey),
					SyBlobLength(&pEntry->xKey.sKey),pVal,0,FALSE);
			}else{
				rc = HashmapInsertIntKey(&(*pDest),pEntry->xKey.iKey,pVal,0,FALSE);
			}
			if( rc != SXRET_OK ){
				return rc;
			}
		}
		/* Point to the next entry */
		pEntry = pEntry->pPrev; /* Reverse link */
	}
	return SXRET_OK;
}
/*
 * Count the map references held by BY-REFERENCE foreach steps iterating the
 * given hashmap. php's `foreach ($a as &$v)` iterates the LIVE array —
 * appends/deletes inside the body are visited — so a by-ref step's retain
 * must not make writes through the source variable COW-separate away from
 * the loop's map. By-VALUE steps are deliberately NOT discounted: their
 * retain is exactly what makes an in-loop write separate, which is php's
 * iterate-a-snapshot semantic.
 */
static sxi32 HashmapByRefStepRefs(ph7_hashmap *pMap)
{
	ph7_foreach_step *pStep;
	sxi32 nRef = 0;
	for( pStep = pMap->pActiveSteps ; pStep ; pStep = pStep->pNextActive ){
		if( pStep->iFlags & PH7_4EACH_STEP_REF ){
			nRef++;
		}
	}
	return nRef;
}
/*
 * Copy-on-write separation for arrays.
 * If the hashmap inside pValue has iRef > 1 (shared), duplicate it so that
 * pValue owns a private copy. The original map's refcount is decremented.
 * Returns the (possibly new) hashmap pointer.
 * References held by active by-ref foreach steps do not count as sharers
 * (see HashmapByRefStepRefs): writes during `foreach ($a as &$v)` must land
 * on the live map the loop is walking, like php.
 */
PH7_PRIVATE ph7_hashmap * PH7_HashmapCowSeparate(ph7_vm *pVm,ph7_value *pValue)
{
	ph7_hashmap *pMap = (ph7_hashmap *)pValue->x.pOther;
	ph7_hashmap *pNew;
	ph7_value *pBacking;
	sxu32 nValIdx;
	int bValueInPool;
	sxi32 nByRefSteps = pMap->pActiveSteps ? HashmapByRefStepRefs(pMap) : 0;
	if( pMap->iRef - nByRefSteps < 2 ){
		/* Sole owner, no separation needed */
		return pMap;
	}
	if( pMap == pVm->pGlobal ){
		/* Never separate $GLOBALS — it is a live view of the symbol table.
		 * (A COPY of $GLOBALS never shares this map: PH7_MemObjStore
		 * materializes a by-value snapshot at assignment, php 8.1.) */
		return pMap;
	}
	/* If this value is a stack copy of a named variable, separate the
	 * backing variable instead so the change persists after the stack
	 * frame is popped. */
	if( pValue->nIdx != SXU32_HIGH ){
		pBacking = (ph7_value *)SySetAt(&pVm->aMemObj,pValue->nIdx);
		if( pBacking && pBacking != pValue
			&& (pBacking->iFlags & MEMOBJ_HASHMAP)
			&& (ph7_hashmap *)pBacking->x.pOther == pMap ){
			/* Undo the stack ref to reveal true sharing count */
			pMap->iRef--;
			if( pMap->iRef - nByRefSteps < 2 ){
				/* After undoing stack ref, sole owner — no separation */
				pMap->iRef++;
				return pMap;
			}
			pNew = PH7_NewHashmap(pVm,0,0);
			if( pNew == 0 ){
				pMap->iRef++;
				return pMap;
			}
			if( PH7_HashmapDup(pMap,pNew) != SXRET_OK ){
				/* Dup failed (OOM) — discard partial copy, restore state */
				PH7_HashmapRelease(pNew,TRUE);
				pMap->iRef++;
				return pMap;
			}
			pNew->iNextIdx = pMap->iNextIdx;
			pMap->iRef--;  /* Backing variable no longer references old map */
			/* PH7_HashmapDup reserves a memory object per duplicated entry, which
			 * can grow — and therefore reallocate (move) — pVm->aMemObj. That
			 * invalidates the pBacking pointer captured above, so re-resolve it
			 * from the (stable) slot index before writing. Using the stale pointer
			 * dereferences the freed old buffer, which is a hard SIGSEGV on
			 * glibc/x86_64 once aMemObj is large enough to be mmap-backed (the old
			 * mapping is munmap'd on move) and a silent use-after-free elsewhere. */
			pBacking = (ph7_value *)SySetAt(&pVm->aMemObj,pValue->nIdx);
			if( pBacking ){
				pBacking->x.pOther = pNew;
			}
			/* Update the stack value to match */
			pValue->x.pOther = pNew;
			pNew->iRef++;  /* +1 for stack (pValue); iRef=1 from NewHashmap covers pBacking */
			return pNew;
		}
	}
	/* Some callers (e.g. OP_STORE_IDX, by-ref foreach) pass a pValue that points
	 * directly into pVm->aMemObj. PH7_HashmapDup below reserves a memory object
	 * per duplicated entry, which can grow — and therefore reallocate (move) —
	 * pVm->aMemObj, leaving such a pValue dangling. Capture its slot identity now,
	 * before the dup, so the write-back can re-resolve from the (stable) index
	 * rather than dereference the captured pointer (the same hazard handled for
	 * pBacking in the backing-variable branch above). */
	nValIdx = pValue->nIdx;
	bValueInPool = ( nValIdx != SXU32_HIGH
		&& (ph7_value *)SySetAt(&pVm->aMemObj,nValIdx) == pValue );
	pNew = PH7_NewHashmap(pVm,0,0);
	if( pNew == 0 ){
		/* Allocation failure — fall through with shared map */
		return pMap;
	}
	if( PH7_HashmapDup(pMap,pNew) != SXRET_OK ){
		/* Dup failed (OOM) — discard partial copy, keep original */
		PH7_HashmapRelease(pNew,TRUE);
		return pMap;
	}
	pNew->iNextIdx = pMap->iNextIdx;
	pMap->iRef--;
	if( bValueInPool ){
		/* aMemObj may have moved during the dup — re-resolve pValue's slot. */
		pValue = (ph7_value *)SySetAt(&pVm->aMemObj,nValIdx);
		if( pValue == 0 ){
			return pNew;
		}
	}
	pValue->x.pOther = pNew;
	return pNew;
}
/*
 * Perform the union of two hashmaps.
 * This operation is performed only if the user uses the '+' operator
 * with a variable holding an array as follows:
 * <?php
 * $a = array("a" => "apple", "b" => "banana");
 * $b = array("a" => "pear", "b" => "strawberry", "c" => "cherry");
 * $c = $a + $b; // Union of $a and $b
 * echo "Union of \$a and \$b: \n";
 * var_dump($c);
 * $c = $b + $a; // Union of $b and $a
 * echo "Union of \$b and \$a: \n";
 * var_dump($c);
 * ?>
 * When executed, this script will print the following:
 * Union of $a and $b:
 * array(3) {
 *  ["a"]=>
 *  string(5) "apple"
 *  ["b"]=>
 * string(6) "banana"
 *  ["c"]=>
 * string(6) "cherry"
 * }
 * Union of $b and $a:
 * array(3) {
 * ["a"]=>
 * string(4) "pear"
 * ["b"]=>
 * string(10) "strawberry"
 * ["c"]=>
 * string(6) "cherry"
 * }
 * The + operator returns the right-hand array appended to the left-hand array;
 * For keys that exist in both arrays, the elements from the left-hand array will be used
 * and the matching elements from the right-hand array will be ignored.
 */
PH7_PRIVATE sxi32 PH7_HashmapUnion(ph7_hashmap *pLeft,ph7_hashmap *pRight)
{
	ph7_hashmap_node *pEntry;
	sxi32 rc = SXRET_OK;
	ph7_value *pObj;
	sxu32 n;
	if( pLeft == pRight ){
		/* Same map. This can easily happen since hashmaps are passed by reference.
		 * Unlike the zend engine.
		 */
		return SXRET_OK;
	}
	/* Perform the union */
	pEntry = pRight->pFirst;
	for(n = 0 ; n < pRight->nEntry ; ++n ){
		/* Make sure the given key does not exists in the left array */
		if( pEntry->iType == HASHMAP_BLOB_NODE ){
			/* BLOB key */
			if( SXRET_OK !=
				HashmapLookupBlobKey(&(*pLeft),SyBlobData(&pEntry->xKey.sKey),SyBlobLength(&pEntry->xKey.sKey),0) ){
					pObj = HashmapExtractNodeValue(pEntry);
					if( pObj ){
						ph7_value sSafeVal = *pObj;
						/* Perform the insertion */
						rc = HashmapInsertBlobKey(&(*pLeft),SyBlobData(&pEntry->xKey.sKey),SyBlobLength(&pEntry->xKey.sKey),
							&sSafeVal,0,FALSE);
						if( rc != SXRET_OK ){
							return rc;
						}
					}
			}
		}else{
			/* INT key */
			if( SXRET_OK != HashmapLookupIntKey(&(*pLeft),pEntry->xKey.iKey,0) ){
				pObj = HashmapExtractNodeValue(pEntry);
				if( pObj ){
					ph7_value sSafeVal = *pObj;
					/* Perform the insertion */
					rc = HashmapInsertIntKey(&(*pLeft),pEntry->xKey.iKey,&sSafeVal,0,FALSE);
					if( rc != SXRET_OK ){
						return rc;
					}
				}
			}
		}
		/* Point to the next entry */
		pEntry = pEntry->pPrev; /* Reverse link */
	}
	return SXRET_OK;
}
/*
 * Allocate a new hashmap.
 * Return a pointer to the freshly allocated hashmap on success.NULL otherwise.
 */
PH7_PRIVATE ph7_hashmap * PH7_NewHashmap(
	ph7_vm *pVm,              /* VM that trigger the hashmap creation */
	sxu32 (*xIntHash)(sxi64), /* Hash function for int keys.NULL otherwise*/
	sxu32 (*xBlobHash)(const void *,sxu32) /* Hash function for BLOB keys.NULL otherwise */
	)
{
	ph7_hashmap *pMap;
	/* Allocate a new instance */
	pMap = (ph7_hashmap *)SyMemBackendPoolAlloc(&pVm->sAllocator,sizeof(ph7_hashmap));
	if( pMap == 0 ){
		return 0;
	}
	/* Zero the structure */
	SyZero(pMap,sizeof(ph7_hashmap));
	/* Fill in the structure */
	pMap->pVm = &(*pVm);
	pMap->iRef = 1;
	/* Default hash functions */
	pMap->xIntHash  = xIntHash ? xIntHash : IntHash;
	pMap->xBlobHash = xBlobHash ? xBlobHash : BinHash;
	return pMap;
}
/*
 * Install superglobals in the given virtual machine.
 * Note on superglobals.
 *  According to the PHP language reference manual.
 *  Superglobals are built-in variables that are always available in all scopes.
*   Description
*   Several predefined variables in PHP are "superglobals", which means they
*   are available in all scopes throughout a script. There is no need to do
*   global $variable; to access them within functions or methods.
*   These superglobal variables are:
*    $GLOBALS
*    $_SERVER
*    $_GET
*    $_POST
*    $_FILES
*    $_COOKIE
*    $_SESSION
*    $_REQUEST
*    $_ENV
*/
PH7_PRIVATE sxi32 PH7_HashmapCreateSuper(ph7_vm *pVm)
{
	static const char * azSuper[] = {
		"_SERVER",   /* $_SERVER */
		"_GET",      /* $_GET */
		"_POST",     /* $_POST */
		"_FILES",    /* $_FILES */
		"_COOKIE",   /* $_COOKIE */
		"_SESSION",  /* $_SESSION */
		"_REQUEST",  /* $_REQUEST */
		"_ENV",      /* $_ENV */
		"_HEADER",   /* $_HEADER */
		"argv"       /* $argv */
	};
	ph7_hashmap *pMap;
	ph7_value *pObj;
	SyString *pFile;
	sxi32 rc;
	sxu32 n;
	/* Allocate a new hashmap for the $GLOBALS array */
	pMap = PH7_NewHashmap(&(*pVm),0,0);
	if( pMap == 0 ){
		return SXERR_MEM;
	}
	pVm->pGlobal = pMap;
	/* Reserve a ph7_value for the $GLOBALS array*/
	pObj = PH7_ReserveMemObj(&(*pVm));
	if( pObj == 0 ){
		return SXERR_MEM;
	}
	PH7_MemObjInitFromArray(&(*pVm),pObj,pMap);
	/* Record object index */
	pVm->nGlobalIdx = pObj->nIdx;
	/* Install the special $GLOBALS array */
	rc = SyHashInsert(&pVm->hSuper,(const void *)"GLOBALS",sizeof("GLOBALS")-1,SX_INT_TO_PTR(pVm->nGlobalIdx));
	if( rc != SXRET_OK ){
		return rc;
	}
	/* Install superglobals now */
	for( n =  0 ; n < SX_ARRAYSIZE(azSuper)  ; n++ ){
		ph7_value *pSuper;
		/* Request an empty array */
		pSuper = ph7_new_array(&(*pVm));
		if( pSuper == 0 ){
			return SXERR_MEM;
		}
		/* Install */
		rc = ph7_vm_config(&(*pVm),PH7_VM_CONFIG_CREATE_SUPER,azSuper[n]/* Super-global name*/,pSuper/* Super-global value */);
		if( rc != SXRET_OK ){
			return rc;
		}
		/* Release the value now it have been installed */
		ph7_release_value(&(*pVm),pSuper);
	}
	/* Set some $_SERVER entries */
	pFile = (SyString *)SySetPeek(&pVm->aFiles);
	/*
	 * 'SCRIPT_FILENAME'
	 * The absolute pathname of the currently executing script.
	 */
	ph7_vm_config(pVm,PH7_VM_CONFIG_SERVER_ATTR,
		"SCRIPT_FILENAME",
		pFile ? pFile->zString : ":Memory:",
		pFile ? pFile->nByte : sizeof(":Memory:") - 1
		);
	/* All done,all super-global are installed now */
	return SXRET_OK;
}
/*
 * Release a hashmap.
 */
PH7_PRIVATE sxi32 PH7_HashmapRelease(ph7_hashmap *pMap,int FreeDS)
{
	ph7_hashmap_node *pEntry,*pNext;
	ph7_vm *pVm = pMap->pVm;
	sxu32 n;
	if( pMap == pVm->pGlobal ){
		/* Cannot delete the $GLOBALS array */
		PH7_VmThrowError(pMap->pVm,0,PH7_CTX_NOTICE,"$GLOBALS is a read-only array,deletion is forbidden");
		return SXRET_OK;
	}
	if( pMap->pActiveSteps ){
		/* Every node is about to be freed WITHOUT going through
		 * PH7_HashmapUnlinkNode, so its cursor fixup never runs. Park any
		 * live foreach cursor on this map (reachable: array_erase() on the
		 * live map of a by-ref foreach — the CowSeparate discount keeps the
		 * loop's map writable). A NULL cursor ends the loop cleanly at the
		 * next step, or resumes on a fresh insert via the link-time re-arm. */
		ph7_foreach_step *pStep;
		for( pStep = pMap->pActiveSteps ; pStep ; pStep = pStep->pNextActive ){
			pStep->pCursor = 0;
		}
	}
	/* Start the release process */
	n = 0;
	pEntry = pMap->pFirst;
	for(;;){
		if( n >= pMap->nEntry ){
			break;
		}
		pNext = pEntry->pPrev; /* Reverse link */
		/* Remove the reference from the foreign table */
		PH7_VmRefObjRemove(pVm,pEntry->nValIdx,0,pEntry);
		if( (pEntry->iFlags & HASHMAP_NODE_FOREIGN_OBJ) == 0 ){
			/* Restore the ph7_value to the free list */
			PH7_VmUnsetMemObj(pVm,pEntry->nValIdx,FALSE);
		}
		/* Release the node */
		if( pEntry->iType == HASHMAP_BLOB_NODE ){
			SyBlobRelease(&pEntry->xKey.sKey);
		}
		SyMemBackendPoolFree(&pVm->sAllocator,pEntry);
		/* Point to the next entry */
		pEntry = pNext;
		n++;
	}
	if( pMap->nEntry > 0 ){
		/* Release the hash bucket */
		SyMemBackendFree(&pVm->sAllocator,pMap->apBucket);
	}
	if( FreeDS ){
		/* Free the whole instance */
		SyMemBackendPoolFree(&pVm->sAllocator,pMap);
	}else{
		/* Keep the instance but reset it's fields */
		pMap->apBucket = 0;
		pMap->iNextIdx = 0;
	pMap->bIntKeySeen = 0;
		pMap->nEntry = pMap->nSize = 0;
		pMap->pFirst = pMap->pLast = pMap->pCur = 0;
	}
	return SXRET_OK;
}
/*
 * Decrement the reference count of a given hashmap.
 * If the count reaches zero which mean no more variables
 * are pointing to this hashmap,then release the whole instance.
 */
PH7_PRIVATE void  PH7_HashmapUnref(ph7_hashmap *pMap)
{
	ph7_vm *pVm = pMap->pVm;
	/* TICKET 1432-49: $GLOBALS is not subject to garbage collection */
	pMap->iRef--;
	if( pMap->iRef < 1 && pMap != pVm->pGlobal){
		PH7_HashmapRelease(pMap,TRUE);
	}
}
/*
 * Check if a given key exists in the given hashmap.
 * Write a pointer to the target node on success.
 * Otherwise SXERR_NOTFOUND is returned on failure.
 */
PH7_PRIVATE sxi32 PH7_HashmapLookup(
	ph7_hashmap *pMap,        /* Target hashmap */
	ph7_value *pKey,          /* Lookup key */
	ph7_hashmap_node **ppNode /* OUT: Target node on success */
	)
{
	sxi32 rc;
	if( pMap->nEntry < 1 ){
		/* TICKET 1433-25: Don't bother hashing,the hashmap is empty anyway.
		 */
		return SXERR_NOTFOUND;
	}
	rc = HashmapLookup(&(*pMap),&(*pKey),ppNode);
	return rc;
}
/*
 * Insert a given key and it's associated value (if any) in the given
 * hashmap.
 * If a node with the given key already exists in the database
 * then this function overwrite the old value.
 */
PH7_PRIVATE sxi32 PH7_HashmapInsert(
	ph7_hashmap *pMap, /* Target hashmap */
	ph7_value *pKey,   /* Lookup key */
	ph7_value *pVal    /* Node value.NULL otherwise */
	)
{
	sxi32 rc;
	/* Storing the $GLOBALS array itself as a VALUE is fine in php ($a[] =
	 * $GLOBALS copies the symbol table); the old TICKET 1433-35 guard that
	 * forbade it was a PH7-ism. Writes INTO $GLOBALS are handled inside
	 * HashmapInsert (they create real global variables, php 8.1). */
	rc = HashmapInsert(&(*pMap),&(*pKey),&(*pVal));
	return rc;
}
/*
 * Merge entries of pSrc into pDest using PHP merge semantics:
 *   - String keys overwrite same-key entries in pDest.
 *   - Integer keys are renumbered with the destination's auto-index.
 * This is the same routine that backs array_merge().
 */
PH7_PRIVATE sxi32 PH7_HashmapMerge(ph7_hashmap *pSrc,ph7_hashmap *pDest)
{
	return HashmapMerge(&(*pSrc),&(*pDest));
}
/*
 * Insert a given key and it's associated value (foreign index) in the given
 * hashmap.
 * This is insertion by reference so be careful to mark the node
 * with the HASHMAP_NODE_FOREIGN_OBJ flag being set.
 * The insertion by reference is triggered when the following
 * expression is encountered.
 * $var = 10;
 *  $a = array(&var);
 * OR
 *  $a[] =& $var;
 * That is,$var is a foreign ph7_value and the $a array have no control
 * over it's contents.
 * Note that the node that hold the foreign ph7_value is automatically
 * removed when the foreign ph7_value is unset.
 * Example:
 *  $var = 10;
 *  $a[] =& $var;
 *  echo count($a).PHP_EOL; //1
 *  //Unset the foreign ph7_value now
 *  unset($var);
 *  echo count($a); //0
 * Note that this is a PH7 eXtension.
 * Refer to the official documentation for more information.
 * If a node with the given key already exists in the database
 * then this function overwrite the old value.
 */
PH7_PRIVATE sxi32 PH7_HashmapInsertByRef(
	ph7_hashmap *pMap, /* Target hashmap */
	ph7_value *pKey,   /* Lookup key */
	sxu32 nRefIdx      /* Foreign ph7_value index */
	)
{
	sxi32 rc;
	if( nRefIdx == pMap->pVm->nGlobalIdx ){
		/* php's non-catchable fatal: $a[] =& $GLOBALS is forbidden (8.1) */
		PH7_VmThrowError(pMap->pVm,0,PH7_CTX_ERR,"Cannot acquire reference to $GLOBALS");
		pMap->pVm->iExitStatus = 255;
		pMap->pVm->bHaltRequested = 1;
		return PH7_ABORT;
	}
	rc = HashmapInsertByRef(&(*pMap),&(*pKey),nRefIdx);
	return rc;
}
/*
 * Register a foreach step as an active iterator of the given hashmap.
 * Each foreach owns a PRIVATE cursor (pStep->pCursor) — php semantics:
 * nested loops over the same array never disturb each other. The map keeps
 * the list of active steps so PH7_HashmapUnlinkNode can advance any cursor
 * parked on a node being deleted (live-map iteration: by-ref foreach,
 * $GLOBALS, OOM snapshot fallbacks).
 */
PH7_PRIVATE void PH7_HashmapRegisterForeachStep(ph7_hashmap *pMap,ph7_foreach_step *pStep)
{
	pStep->pCursor = pMap->pFirst;
	pStep->pNextActive = pMap->pActiveSteps;
	pMap->pActiveSteps = pStep;
}
/*
 * Unregister a foreach step from the map's active-iterator list. Must run
 * before the step is freed AND before the step's map reference is dropped —
 * a step left on the list after its pool slot is recycled is a use-after-free
 * on the next unlink fixup (the SyHash-layout incident class).
 */
PH7_PRIVATE void PH7_HashmapUnregisterForeachStep(ph7_hashmap *pMap,ph7_foreach_step *pStep)
{
	ph7_foreach_step **ppLink = &pMap->pActiveSteps;
	while( *ppLink ){
		if( *ppLink == pStep ){
			*ppLink = pStep->pNextActive;
			pStep->pNextActive = 0;
			return;
		}
		ppLink = &(*ppLink)->pNextActive;
	}
}
/*
 * Return a pointer to the node currently pointed by the node cursor.
 * If the cursor reaches the end of the list,then this function
 * return NULL.
 * Note that the node cursor is automatically advanced by this function.
 */
PH7_PRIVATE ph7_hashmap_node * PH7_HashmapGetNextEntry(ph7_hashmap *pMap)
{
	ph7_hashmap_node *pCur = pMap->pCur;
	if( pCur == 0 ){
		/* End of the list,return null */
		return 0;
	}
	/* Advance the node cursor */
	pMap->pCur = pCur->pPrev; /* Reverse link */
	return pCur;
}
/*
 * Extract a node value.
 */
PH7_PRIVATE void PH7_HashmapExtractNodeValue(ph7_hashmap_node *pNode,ph7_value *pValue,int bStore)
{
	ph7_value *pEntry = HashmapExtractNodeValue(pNode);
	if( pEntry ){
		if( bStore ){
			PH7_MemObjStore(pEntry,pValue);
		}else{
			PH7_MemObjLoad(pEntry,pValue);
		}
	}else{
		PH7_MemObjRelease(pValue);
	}
}
/*
 * Extract a node key.
 */
PH7_PRIVATE void PH7_HashmapExtractNodeKey(ph7_hashmap_node *pNode,ph7_value *pKey)
{
	/* Fill with the current key */
	if( pNode->iType == HASHMAP_INT_NODE ){
		if( SyBlobLength(&pKey->sBlob) > 0 ){
			SyBlobRelease(&pKey->sBlob);
		}
		pKey->x.iVal = pNode->xKey.iKey;
		MemObjSetType(pKey,MEMOBJ_INT);
	}else{
		SyBlobReset(&pKey->sBlob);
		SyBlobAppend(&pKey->sBlob,SyBlobData(&pNode->xKey.sKey),SyBlobLength(&pNode->xKey.sKey));
		MemObjSetType(pKey,MEMOBJ_STRING);
	}
}
#ifndef PH7_DISABLE_DISK_IO
/*
 * Store the address of nodes value in the given container.
 * Refer to the [vfprintf(),vprintf(),vsprintf()] implementations
 * defined in 'builtin.c' for more information.
 */
PH7_PRIVATE int PH7_HashmapValuesToSet(ph7_hashmap *pMap,SySet *pOut)
{
	ph7_hashmap_node *pEntry = pMap->pFirst;
	ph7_value *pValue;
	sxu32 n;
	/* Initialize the container */
	SySetInit(pOut,&pMap->pVm->sAllocator,sizeof(ph7_value *));
	for(n = 0 ; n < pMap->nEntry ; n++ ){
		/* Extract node value */
		pValue = HashmapExtractNodeValue(pEntry);
		if( pValue ){
			SySetPut(pOut,(const void *)&pValue);
		}
		/* Point to the next entry */
		pEntry = pEntry->pPrev; /* Reverse link */
	}
	/* Total inserted entries */
	return (int)SySetUsed(pOut);
}
#endif /* PH7_DISABLE_BUILTIN_FUNC || PH7_DISABLE_DISK_IO */
/*
 * Table of hashmap functions.
 */
static const ph7_builtin_func aHashmapFunc[] = {
	{"iterator_to_array",  ph7_iterator_to_array },
	{"iterator_count",     ph7_iterator_count },
	{"iterator_apply",     ph7_iterator_apply },
	{"count",             ph7_hashmap_count },
	{"sizeof",            ph7_hashmap_count },
	{"array_key_exists",  ph7_hashmap_key_exists },
	{"key_exists",        ph7_hashmap_key_exists },
	{"array_pop",         ph7_hashmap_pop     },
	{"array_push",        ph7_hashmap_push    },
	{"array_shift",       ph7_hashmap_shift   },
	{"array_product",     ph7_hashmap_product },
	{"array_sum",         ph7_hashmap_sum     },
	{"max",               ph7_hashmap_max     },
	{"min",               ph7_hashmap_min     },
	{"array_keys",        ph7_hashmap_keys    },
	{"array_values",      ph7_hashmap_values  },
	{"array_same",        ph7_hashmap_same    },  /* Symisc eXtension */
	{"array_merge",       ph7_hashmap_merge   },
	{"array_slice",       ph7_hashmap_slice   },
	{"array_splice",      ph7_hashmap_splice  },
	{"array_search",      ph7_hashmap_search  },
	{"array_diff",        ph7_hashmap_diff    },
	{"array_udiff",       ph7_hashmap_udiff   },
	{"array_diff_assoc",  ph7_hashmap_diff_assoc },
	{"array_diff_uassoc", ph7_hashmap_diff_uassoc },
	{"array_diff_key",    ph7_hashmap_diff_key },
	{"array_intersect",   ph7_hashmap_intersect},
	{"array_intersect_assoc", ph7_hashmap_intersect_assoc},
	{"array_uintersect",  ph7_hashmap_uintersect},
	{"array_intersect_key",   ph7_hashmap_intersect_key},
	{"array_copy",        ph7_hashmap_copy    },
	{"array_erase",       ph7_hashmap_erase   },
	{"array_fill",        ph7_hashmap_fill    },
	{"array_fill_keys",   ph7_hashmap_fill_keys},
	{"array_combine",     ph7_hashmap_combine },
	{"array_reverse",     ph7_hashmap_reverse },
	{"array_unique",      ph7_hashmap_unique  },
	{"array_flip",        ph7_hashmap_flip    },
	{"array_rand",        ph7_hashmap_rand    },
	{"array_chunk",       ph7_hashmap_chunk   },
	{"array_pad",         ph7_hashmap_pad     },
	{"array_replace",     ph7_hashmap_replace },
	{"array_filter",      ph7_hashmap_filter  },
	{"array_map",         ph7_hashmap_map     },
	{"array_column",      ph7_hashmap_column  },
	{"array_is_list",     ph7_hashmap_is_list },
	{"array_first",       ph7_hashmap_first   },
	{"array_last",        ph7_hashmap_last    },
	{"array_key_first",   ph7_hashmap_key_first },
	{"array_key_last",    ph7_hashmap_key_last  },
	{"array_find",        ph7_hashmap_find    },
	{"array_find_key",    ph7_hashmap_find_key},
	{"array_any",         ph7_hashmap_any     },
	{"array_all",         ph7_hashmap_all     },
	{"array_reduce",      ph7_hashmap_reduce  },
	{"array_walk",        ph7_hashmap_walk    },
	{"array_walk_recursive", ph7_hashmap_walk_recursive },
	{"in_array",          ph7_hashmap_in_array},
	{"sort",              ph7_hashmap_sort    },
	{"asort",             ph7_hashmap_asort   },
	{"natsort",           ph7_hashmap_natsort },
	{"natcasesort",       ph7_hashmap_natsort },
	{"arsort",            ph7_hashmap_arsort  },
	{"ksort",             ph7_hashmap_ksort   },
	{"krsort",            ph7_hashmap_krsort  },
	{"rsort",             ph7_hashmap_rsort   },
	{"usort",             ph7_hashmap_usort   },
	{"uasort",            ph7_hashmap_uasort  },
	{"uksort",            ph7_hashmap_uksort  },
	{"shuffle",           ph7_hashmap_shuffle },
	{"range",             ph7_hashmap_range   },
	{"current",           ph7_hashmap_current },
	{"each",              ph7_hashmap_each    },
	{"pos",               ph7_hashmap_current },
	{"next",              ph7_hashmap_next    },
	{"prev",              ph7_hashmap_prev    },
	{"end",               ph7_hashmap_end     },
	{"reset",             ph7_hashmap_reset   },
	{"key",               ph7_hashmap_simple_key }
};
/*
 * Register the built-in hashmap functions defined above.
 */
PH7_PRIVATE void PH7_RegisterHashmapFunctions(ph7_vm *pVm)
{
	sxu32 n;
	for( n = 0 ; n < SX_ARRAYSIZE(aHashmapFunc) ; n++ ){
		ph7_create_function(&(*pVm),aHashmapFunc[n].zName,aHashmapFunc[n].xFunc,0);
	}
}
/*
 * Dump a hashmap instance and it's entries and the store the dump in
 * the BLOB given as the first argument.
 * This function is typically invoked when the user issue a call to
 * [var_dump(),var_export(),print_r(),...]
 * This function SXRET_OK on success. Any other return value including
 * SXERR_LIMIT(infinite recursion) indicates failure.
 */
/*
 * Dump the entries of a hashmap [i.e: the key/value lines between the opening
 * '{' and the closing '}'] in the var_dump/print_r style. Factored out of
 * PH7_HashmapDump so the var_dump object renderer can reuse it for a
 * __debugInfo() array body (which carries an object header, not "array(N)").
 * Returns SXERR_LIMIT if a nested value hit the depth cap.
 */
PH7_PRIVATE sxi32 PH7_HashmapDumpEntries(SyBlob *pOut,ph7_hashmap *pMap,int ShowType,int nTab,int nDepth)
{
	ph7_hashmap_node *pEntry = pMap->pFirst;
	ph7_value *pObj;
	sxu32 n = 0;
	int isRef;
	sxi32 rc = SXRET_OK;
	int i;
	for(;;){
		if( n >= pMap->nEntry ){
			break;
		}
		pObj = HashmapExtractNodeValue(pEntry);
		/* '&' marks an element ANY other holder refers to: a foreign node (array(&$x))
		 * or, the case PH7 missed, an element someone took a reference to ($r = &$a[1]). */
		isRef = ((pEntry->iFlags & HASHMAP_NODE_FOREIGN_OBJ) != 0)
			|| PH7_VmSlotIsReferenced(pMap->pVm,pEntry->nValIdx);
		if( ShowType ){
			/* var_dump entry: `[key]=>` on its own line at nTab+2, the value
			 * on the next line at the same indent (php). */
			for( i = 0 ; i < nTab + 2 ; i++ ){
				SyBlobAppend(&(*pOut)," ",sizeof(char));
			}
			if( pEntry->iType == HASHMAP_INT_NODE){
				SyBlobFormat(&(*pOut),"[%qd]=>",pEntry->xKey.iKey);
			}else{
				SyBlobFormat(&(*pOut),"[\"%.*s\"]=>",
					SyBlobLength(&pEntry->xKey.sKey),SyBlobData(&pEntry->xKey.sKey));
			}
			SyBlobAppend(&(*pOut),"\n",sizeof(char));
			if( pObj ){
				rc = PH7_MemObjDump(&(*pOut),pObj,TRUE,nTab+2,nDepth,isRef);
				if( rc == SXERR_LIMIT ){
					break;
				}
			}
		}else{
			/* print_r entry: `[key] => value` at nTab+4; a container value
			 * renders its block inline (its parens at nTab+8) followed by
			 * php's extra blank line. References carry no marker. */
			for( i = 0 ; i < nTab + 4 ; i++ ){
				SyBlobAppend(&(*pOut)," ",sizeof(char));
			}
			if( pEntry->iType == HASHMAP_INT_NODE){
				SyBlobFormat(&(*pOut),"[%qd] => ",pEntry->xKey.iKey);
			}else{
				SyBlobFormat(&(*pOut),"[%.*s] => ",
					SyBlobLength(&pEntry->xKey.sKey),SyBlobData(&pEntry->xKey.sKey));
			}
			if( pObj && (pObj->iFlags & (MEMOBJ_HASHMAP|MEMOBJ_OBJ))
			 && (pObj->iFlags & MEMOBJ_NULL) == 0 ){
				rc = PH7_MemObjDump(&(*pOut),pObj,FALSE,nTab+8,nDepth,0);
				SyBlobAppend(&(*pOut),"\n",sizeof(char));
				if( rc == SXERR_LIMIT ){
					break;
				}
			}else{
				if( pObj ){
					PH7_MemObjPrintRInline(&(*pOut),pObj);
				}
				SyBlobAppend(&(*pOut),"\n",sizeof(char));
			}
		}
		/* Point to the next entry */
		n++;
		pEntry = pEntry->pPrev; /* Reverse link */
	}
	return rc;
}
PH7_PRIVATE sxi32 PH7_HashmapDump(SyBlob *pOut,ph7_hashmap *pMap,int ShowType,int nTab,int nDepth)
{
	sxi32 rc;
	int i;
	if( nDepth > 31 ){
		static const char zInfinite[] = "Nesting limit reached: Infinite recursion?";
		/* Nesting limit reached */
		SyBlobAppend(&(*pOut),zInfinite,sizeof(zInfinite)-1);
		return SXERR_LIMIT;
	}
	if( ShowType ){
		/* var_dump: `array(N) {\n … \n<nTab>}` — the caller adds the final
		 * newline (a nested array is itself an entry value line). */
		SyBlobFormat(&(*pOut),"array(%u) {",pMap->nEntry);
		SyBlobAppend(&(*pOut),"\n",sizeof(char));
		rc = PH7_HashmapDumpEntries(&(*pOut),pMap,TRUE,nTab,nDepth);
		for( i = 0 ; i < nTab ; i++ ){
			SyBlobAppend(&(*pOut)," ",sizeof(char));
		}
		SyBlobAppend(&(*pOut),"}",sizeof(char));
		return rc;
	}
	/* print_r: `Array\n<nTab>(\n … <nTab>)\n` */
	SyBlobAppend(&(*pOut),"Array\n",sizeof("Array\n")-1);
	for( i = 0 ; i < nTab ; i++ ){
		SyBlobAppend(&(*pOut)," ",sizeof(char));
	}
	SyBlobAppend(&(*pOut),"(\n",sizeof("(\n")-1);
	rc = PH7_HashmapDumpEntries(&(*pOut),pMap,FALSE,nTab,nDepth);
	for( i = 0 ; i < nTab ; i++ ){
		SyBlobAppend(&(*pOut)," ",sizeof(char));
	}
	SyBlobAppend(&(*pOut),")\n",sizeof(")\n")-1);
	return rc;
}
/*
 * Iterate throw hashmap entries and invoke the given callback [i.e: xWalk()] for each
 * retrieved entry.
 * Note that argument are passed to the callback by copy. That is,any modification to
 * the entry value in the callback body will not alter the real value.
 * If the callback wishes to abort processing [i.e: it's invocation] it must return
 * a value different from PH7_OK.
 * Refer to [ph7_array_walk()] for more information.
 */
PH7_PRIVATE sxi32 PH7_HashmapWalk(
	ph7_hashmap *pMap, /* Target hashmap */
	int (*xWalk)(ph7_value *,ph7_value *,void *), /* Walker callback */
	void *pUserData /* Last argument to xWalk() */
	)
{
	ph7_hashmap_node *pEntry;
	ph7_value sKey,sValue;
	sxi32 rc;
	sxu32 n;
	/* Initialize walker parameter */
	rc = SXRET_OK;
	PH7_MemObjInit(pMap->pVm,&sKey);
	PH7_MemObjInit(pMap->pVm,&sValue);
	n = pMap->nEntry;
	pEntry = pMap->pFirst;
	/* Start the iteration process */
	for(;;){
		if( n < 1 ){
			break;
		}
		/* Extract a copy of the key and a copy the current value */
		PH7_HashmapExtractNodeKey(pEntry,&sKey);
		PH7_HashmapExtractNodeValue(pEntry,&sValue,FALSE);
		/* Invoke the user callback */
		rc = xWalk(&sKey,&sValue,pUserData);
		/* Release the copy of the key and the value */
		PH7_MemObjRelease(&sKey);
		PH7_MemObjRelease(&sValue);
		if( rc != PH7_OK ){
			/* Callback request an operation abort */
			return SXERR_ABORT;
		}
		/* Point to the next entry */
		pEntry = pEntry->pPrev; /* Reverse link */
		n--;
	}
	/* All done */
	return SXRET_OK;
}
