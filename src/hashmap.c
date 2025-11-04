/**
 * SPDX-FileCopyrightText: 2011, 2012, 2013, 2014 Symisc Systems <licensing@symisc.net>
 * SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
 * SPDX-License-Identifier: BSD-3-Clause
 */
#ifndef PH7_AMALGAMATION
#include "ph7int.h"
#endif
/* This file implement generic hashmaps known as 'array' in the PHP world */
/* Allowed node types */
#define HASHMAP_INT_NODE   1  /* Node with an int [i.e: 64-bit integer] key */
#define HASHMAP_BLOB_NODE  2  /* Node with a string/BLOB key */
/* Node control flags */
#define HASHMAP_NODE_FOREIGN_OBJ 0x001 /* Node hold a reference to a foreign ph7_value
                                        * [i.e: array(&var)/$a[] =& $var ]
										*/
/*
 * Default hash function for int [i.e; 64-bit integer] keys.
 */
static sxu32 IntHash(sxi64 iKey)
{
	return (sxu32)(iKey ^ (iKey << 8) ^ (iKey >> 8));
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
 * If bRecurisve is set to TRUE then recurse on hashmap entries.
 * If the nesting limit is reached,this function abort immediately. 
 */
static sxi64 HashmapCount(ph7_hashmap *pMap,int bRecursive,int iRecCount)
{
	sxi64 iCount = 0;
	if( !bRecursive ){
		iCount = pMap->nEntry;
	}else{
		/* Recursive hashmap walk */
		ph7_hashmap_node *pEntry = pMap->pLast;
		ph7_value *pElem;
		sxu32 n = 0;
		for(;;){
			if( n >= pMap->nEntry ){
				break;
			}
			/* Point to the element value */
			pElem = (ph7_value *)SySetAt(&pMap->pVm->aMemObj,pEntry->nValIdx);
			if( pElem ){
				if( pElem->iFlags & MEMOBJ_HASHMAP ){
					if( iRecCount > 31 ){
						/* Nesting limit reached */
						return iCount;
					}
					/* Recurse */
					iRecCount++;
					iCount += HashmapCount((ph7_hashmap *)pElem->x.pOther,TRUE,iRecCount);
					iRecCount--;
				}
			}
			/* Point to the next entry */
			pEntry = pEntry->pNext;
			++n;
		}
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
static sxi32 HashmapLookupIntKey(
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
static sxi32 HashmapLookupBlobKey(
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
	if( (int)(zEnd-zIn) > 1 && zIn[0] == '0' ){
		/* Octal not decimal number */
		return FALSE;
	}
	if( (zIn[0] == '-' || zIn[0] == '+') && &zIn[1] < zEnd ){
		zIn++;
	}
	for(;;){
		if( zIn >= zEnd ){
			return TRUE;
		}
		if( (unsigned char)zIn[0] >= 0xc0 /* UTF-8 stream */  || !SyisDigit(zIn[0]) ){
			break;
		}
		zIn++;
	}
	/* Key does not look like a decimal number */
	return FALSE;
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
	if( pKey->iFlags & (MEMOBJ_STRING|MEMOBJ_HASHMAP|MEMOBJ_OBJ|MEMOBJ_RES) ){
		if( (pKey->iFlags & MEMOBJ_STRING) == 0 ){
			/* Force a string cast */
			PH7_MemObjToString(&(*pKey));
		}
		if( SyBlobLength(&pKey->sBlob) > 0 && !HashmapIsIntKey(&pKey->sBlob) ){
			/* Perform a blob lookup */
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
 * Insert a given key and it's associated value (if any) in the given
 * hashmap.
 * If a node with the given key already exists in the database
 * then this function overwrite the old value.
 */
static sxi32 HashmapInsert(
	ph7_hashmap *pMap, /* Target hashmap */
	ph7_value *pKey,   /* Lookup key  */
	ph7_value *pVal    /* Node value */
	)
{
	ph7_hashmap_node *pNode = 0;
	sxi32 rc = SXRET_OK;
	if( pKey && pKey->iFlags & (MEMOBJ_STRING|MEMOBJ_HASHMAP|MEMOBJ_OBJ|MEMOBJ_RES) ){
		if( (pKey->iFlags & MEMOBJ_STRING) == 0 ){
			/* Force a string cast */
			PH7_MemObjToString(&(*pKey));
		}
		if( SyBlobLength(&pKey->sBlob) < 1 || HashmapIsIntKey(&pKey->sBlob) ){
			if(SyBlobLength(&pKey->sBlob) < 1){
				/* Automatic index assign */
				pKey = 0;
			}
			goto IntKey;
		}
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
			/* Forbidden */
			PH7_VmThrowError(pMap->pVm,0,PH7_CTX_NOTICE,"$GLOBALS is a read-only array,insertion is forbidden");
			return SXRET_OK;
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
			/* Forbidden */
			PH7_VmThrowError(pMap->pVm,0,PH7_CTX_NOTICE,"$GLOBALS is a read-only array,insertion is forbidden");
			return SXRET_OK;
		}
		/* Perform a 64-bit-int-key insertion */
		rc = HashmapInsertIntKey(&(*pMap),pKey->x.iVal,&(*pVal),0,FALSE);
		if( rc == SXRET_OK ){
			if( pKey->x.iVal >= pMap->iNextIdx ){
				/* Increment the automatic index */ 
				pMap->iNextIdx = pKey->x.iVal + 1;
				/* Make sure the automatic index is not reserved */
				while( SXRET_OK == HashmapLookupIntKey(&(*pMap),pMap->iNextIdx,0) ){
					pMap->iNextIdx++;
				}
			}
		}
	}else{
		if( pMap == pMap->pVm->pGlobal ){
			/* Forbidden */
			PH7_VmThrowError(pMap->pVm,0,PH7_CTX_NOTICE,"$GLOBALS is a read-only array,insertion is forbidden");
			return SXRET_OK;
		}
		/* Assign an automatic index */
		rc = HashmapInsertIntKey(&(*pMap),pMap->iNextIdx,&(*pVal),0,FALSE);
		if( rc == SXRET_OK ){
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
	if( pKey && pKey->iFlags & (MEMOBJ_STRING|MEMOBJ_HASHMAP|MEMOBJ_OBJ|MEMOBJ_RES) ){
		if( (pKey->iFlags & MEMOBJ_STRING) == 0 ){
			/* Force a string cast */
			PH7_MemObjToString(&(*pKey));
		}
		if( SyBlobLength(&pKey->sBlob) < 1 || HashmapIsIntKey(&pKey->sBlob) ){
			if(SyBlobLength(&pKey->sBlob) < 1){
				/* Automatic index assign */
				pKey = 0;
			}
			goto IntKey;
		}
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
			if( pKey->x.iVal >= pMap->iNextIdx ){
				/* Increment the automatic index */ 
				pMap->iNextIdx = pKey->x.iVal + 1;
				/* Make sure the automatic index is not reserved */
				while( SXRET_OK == HashmapLookupIntKey(&(*pMap),pMap->iNextIdx,0) ){
					pMap->iNextIdx++;
				}
			}
		}
	}else{
		/* Assign an automatic index */
		rc = HashmapInsertIntKey(&(*pMap),pMap->iNextIdx,0,nRefIdx,TRUE);
		if( rc == SXRET_OK ){
			++pMap->iNextIdx;
		}
	}
	/* Insertion result */
	return rc;
}
/*
 * Extract node value.
 */
static ph7_value * HashmapExtractNodeValue(ph7_hashmap_node *pNode)
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
static sxi32 HashmapInsertNode(ph7_hashmap *pMap,ph7_hashmap_node *pNode,int bPreserve)
{
	ph7_value *pObj;
	sxi32 rc;
	/* Extract the node value */
	pObj = HashmapExtractNodeValue(&(*pNode));
	if( pObj == 0 ){
		return SXERR_EMPTY;
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
		rc = HashmapInsertBlobKey(&(*pMap),SyBlobData(&pNode->xKey.sKey),
			SyBlobLength(&pNode->xKey.sKey),pObj,0,FALSE);
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
static sxi32 HashmapNodeCmp(ph7_hashmap_node *pLeft,ph7_hashmap_node *pRight,int bStrict)
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
static void HashmapRehashIntNode(ph7_hashmap_node *pEntry)
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
	/* Increment the automatic index */
	pMap->iNextIdx++;
}
/*
 * Perform a linear search on a given hashmap.
 * Write a pointer to the target node on success.
 * Otherwise SXERR_NOTFOUND is returned on failure.
 * Refer to [array_intersect(),array_diff(),in_array(),...] implementations 
 * for more information.
 */
static int HashmapFindValue(
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
			if( (pVal->iFlags|pNeedle->iFlags) & MEMOBJ_NULL ){
				sxi32 iF1 = pVal->iFlags&~MEMOBJ_AUX;
				sxi32 iF2 = pNeedle->iFlags&~MEMOBJ_AUX;
				if( iF1 == iF2 ){
					/* NULL values are equals */
					if( ppNode ){
						*ppNode = pEntry;
					}
					return SXRET_OK;
				}
			}else{
				/* Duplicate value */
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
		}
		/* Point to the next entry */
		pEntry = pEntry->pPrev; /* Reverse link */
		n--;
	}
	/* No such entry */
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
static int HashmapFindValueByCallback(
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
static sxi32 HashmapMerge(ph7_hashmap *pSrc,ph7_hashmap *pDest)
{
	ph7_hashmap_node *pEntry;
	ph7_value sKey,*pVal;
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
		if( pEntry->iType == HASHMAP_BLOB_NODE ){
			/* Blob key insertion */
			PH7_MemObjInitFromString(pDest->pVm,&sKey,0);
			PH7_MemObjStringAppend(&sKey,(const char *)SyBlobData(&pEntry->xKey.sKey),SyBlobLength(&pEntry->xKey.sKey));
			rc = PH7_HashmapInsert(&(*pDest),&sKey,pVal);
			PH7_MemObjRelease(&sKey);
		}else{
			rc = HashmapInsert(&(*pDest),0/* Automatic index assign */,pVal);
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
static sxi32 HashmapOverwrite(ph7_hashmap *pSrc,ph7_hashmap *pDest)
{
	ph7_hashmap_node *pEntry;
	ph7_value sKey,*pVal;
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
		if( pEntry->iType == HASHMAP_BLOB_NODE ){
			/* Blob key insertion */
			PH7_MemObjInitFromString(pDest->pVm,&sKey,0);
			PH7_MemObjStringAppend(&sKey,(const char *)SyBlobData(&pEntry->xKey.sKey),SyBlobLength(&pEntry->xKey.sKey));
		}else{
			/* Int key insertion */
			PH7_MemObjInitFromInt(pDest->pVm,&sKey,pEntry->xKey.iKey);
		}
		rc = PH7_HashmapInsert(&(*pDest),&sKey,pVal);
		PH7_MemObjRelease(&sKey);
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
	ph7_value sKey,*pVal;
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
		if( pEntry->iType == HASHMAP_BLOB_NODE ){
			/* Blob key insertion */
			PH7_MemObjInitFromString(pDest->pVm,&sKey,0);
			PH7_MemObjStringAppend(&sKey,(const char *)SyBlobData(&pEntry->xKey.sKey),SyBlobLength(&pEntry->xKey.sKey));
			rc = PH7_HashmapInsert(&(*pDest),&sKey,pVal);
			PH7_MemObjRelease(&sKey);
		}else{
			/* Int key insertion */
			rc = HashmapInsertIntKey(&(*pDest),pEntry->xKey.iKey,pVal,0,FALSE);
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
						/* Perform the insertion */
						rc = HashmapInsertBlobKey(&(*pLeft),SyBlobData(&pEntry->xKey.sKey),SyBlobLength(&pEntry->xKey.sKey),
							pObj,0,FALSE);
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
					/* Perform the insertion */
					rc = HashmapInsertIntKey(&(*pLeft),pEntry->xKey.iKey,pObj,0,FALSE);
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
	if( pVal && (pVal->iFlags & MEMOBJ_HASHMAP) && (ph7_hashmap *)pVal->x.pOther == pMap->pVm->pGlobal ){
		/*
		 * TICKET 1433-35: Insertion in the $GLOBALS array is forbidden.
		 */ 
		PH7_VmThrowError(pMap->pVm,0,PH7_CTX_ERR,"$GLOBALS is a read-only array,insertion is forbidden");
		return SXRET_OK;
	}
	rc = HashmapInsert(&(*pMap),&(*pKey),&(*pVal));
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
PH7_PRIVATE sxi32 PH7_HashmapInsertByRef(
	ph7_hashmap *pMap, /* Target hashmap */
	ph7_value *pKey,   /* Lookup key */
	sxu32 nRefIdx      /* Foreign ph7_value index */
	)
{
	sxi32 rc;
	if( nRefIdx == pMap->pVm->nGlobalIdx ){
		/*
		 * TICKET 1433-35: Insertion in the $GLOBALS array is forbidden.
		 */ 
		PH7_VmThrowError(pMap->pVm,0,PH7_CTX_ERR,"$GLOBALS is a read-only array,insertion is forbidden");
		return SXRET_OK;
	}
	rc = HashmapInsertByRef(&(*pMap),&(*pKey),nRefIdx);
	return rc;
}
/*
 * Reset the node cursor of a given hashmap.
 */
PH7_PRIVATE void PH7_HashmapResetLoopCursor(ph7_hashmap *pMap)
{
	/* Reset the loop cursor */
	pMap->pCur = pMap->pFirst;
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
#ifndef PH7_DISABLE_BUILTIN_FUNC
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
#endif /* PH7_DISABLE_BUILTIN_FUNC */
/*
 * Merge sort.
 * The merge sort implementation is based on the one found in the SQLite3 source tree.
 * Status: Public domain
 */
/* Node comparison callback signature */
typedef sxi32 (*ProcNodeCmp)(ph7_hashmap_node *,ph7_hashmap_node *,void *);
/*
** Inputs:
**   a:       A sorted, null-terminated linked list.  (May be null).
**   b:       A sorted, null-terminated linked list.  (May be null).
**   cmp:     A pointer to the comparison function.
**
** Return Value:
**   A pointer to the head of a sorted list containing the elements
**   of both a and b.
**
** Side effects:
**   The "next","prev" pointers for elements in the lists a and b are
**   changed.
*/
static ph7_hashmap_node * HashmapNodeMerge(ph7_hashmap_node *pA,ph7_hashmap_node *pB,ProcNodeCmp xCmp,void *pCmpData)
{
	ph7_hashmap_node result,*pTail;
    /* Prevent compiler warning */
	result.pNext = result.pPrev = 0;
	pTail = &result;
	while( pA && pB ){
		if( xCmp(pA,pB,pCmpData) < 0 ){
			pTail->pPrev = pA;
			pA->pNext = pTail;
			pTail = pA;
			pA = pA->pPrev;
		}else{
			pTail->pPrev = pB;
			pB->pNext = pTail;
			pTail = pB;
			pB = pB->pPrev;
		}
	}
	if( pA ){
		pTail->pPrev = pA;
		pA->pNext = pTail;
	}else if( pB ){
		pTail->pPrev = pB;
		pB->pNext = pTail;
	}else{
		pTail->pPrev = pTail->pNext = 0;
	}
	return result.pPrev;
}
/*
** Inputs:
**   Map:       Input hashmap
**   cmp:       A comparison function.
**
** Return Value:
**   Sorted hashmap.
**
** Side effects:
**   The "next" pointers for elements in list are changed.
*/
#define N_SORT_BUCKET  32
static sxi32 HashmapMergeSort(ph7_hashmap *pMap,ProcNodeCmp xCmp,void *pCmpData)
{
	ph7_hashmap_node *a[N_SORT_BUCKET], *p,*pIn;
	sxu32 i;
	SyZero(a,sizeof(a));
	/* Point to the first inserted entry */
	pIn = pMap->pFirst;
	while( pIn ){
		p = pIn;
		pIn = p->pPrev;
		p->pPrev = 0;
		for(i=0; i<N_SORT_BUCKET-1; i++){
			if( a[i]==0 ){
				a[i] = p;
				break;
			}else{
				p = HashmapNodeMerge(a[i],p,xCmp,pCmpData);
				a[i] = 0;
			}
		}
		if( i==N_SORT_BUCKET-1 ){
			/* To get here, there need to be 2^(N_SORT_BUCKET) elements in he input list.
			 * But that is impossible.
			 */
			a[i] = HashmapNodeMerge(a[i], p,xCmp,pCmpData);
		}
	}
	p = a[0];
	for(i=1; i<N_SORT_BUCKET; i++){
		p = HashmapNodeMerge(p,a[i],xCmp,pCmpData);
	}
	p->pNext = 0;
	/* Reflect the change */
	pMap->pFirst = p;
	/* Reset the loop cursor */
	pMap->pCur = pMap->pFirst;
	return SXRET_OK;
}
/* 
 * Node comparison callback.
 * used-by: [sort(),asort(),...]
 */
static sxi32 HashmapCmpCallback1(ph7_hashmap_node *pA,ph7_hashmap_node *pB,void *pCmpData)
{
	ph7_value sA,sB;
	sxi32 iFlags;
	int rc;
	if( pCmpData == 0 ){
		/* Perform a standard comparison */
		rc = HashmapNodeCmp(pA,pB,FALSE);
		return rc;
	}
	iFlags = SX_PTR_TO_INT(pCmpData);
	/* Duplicate node values */
	PH7_MemObjInit(pA->pMap->pVm,&sA);
	PH7_MemObjInit(pA->pMap->pVm,&sB);
	PH7_HashmapExtractNodeValue(pA,&sA,FALSE);
	PH7_HashmapExtractNodeValue(pB,&sB,FALSE);
	if( iFlags == 5 ){
		/* String cast */
		if( (sA.iFlags & MEMOBJ_STRING) == 0 ){
			PH7_MemObjToString(&sA);
		}
		if( (sB.iFlags & MEMOBJ_STRING) == 0 ){
			PH7_MemObjToString(&sB);
		}
	}else{
		/* Numeric cast */
		PH7_MemObjToNumeric(&sA);
		PH7_MemObjToNumeric(&sB);
	}
	/* Perform the comparison */
	rc = PH7_MemObjCmp(&sA,&sB,FALSE,0);
	PH7_MemObjRelease(&sA);
	PH7_MemObjRelease(&sB);
	return rc;
}
/*
 * Node comparison callback: Compare nodes by keys only.
 * used-by: [ksort()]
 */
static sxi32 HashmapCmpCallback2(ph7_hashmap_node *pA,ph7_hashmap_node *pB,void *pCmpData)
{
	sxi32 rc;
	SXUNUSED(pCmpData); /* cc warning */
	if( pA->iType == HASHMAP_BLOB_NODE && pB->iType == HASHMAP_BLOB_NODE ){
		/* Perform a string comparison */
		rc = SyBlobCmp(&pA->xKey.sKey,&pB->xKey.sKey);
	}else{
		SyString sStr;
		sxi64 iA,iB;
		/* Perform a numeric comparison */
		if( pA->iType == HASHMAP_BLOB_NODE ){
			/* Cast to 64-bit integer */
			SyStringInitFromBuf(&sStr,SyBlobData(&pA->xKey.sKey),SyBlobLength(&pA->xKey.sKey));
			if( sStr.nByte < 1 ){
				iA = 0;
			}else{
				SyStrToInt64(sStr.zString,sStr.nByte,(void *)&iA,0);
			}
		}else{
			iA = pA->xKey.iKey;
		}
		if( pB->iType == HASHMAP_BLOB_NODE ){
			/* Cast to 64-bit integer */
			SyStringInitFromBuf(&sStr,SyBlobData(&pB->xKey.sKey),SyBlobLength(&pB->xKey.sKey));
			if( sStr.nByte < 1 ){
				iB = 0;
			}else{
				SyStrToInt64(sStr.zString,sStr.nByte,(void *)&iB,0);
			}
		}else{
			iB = pB->xKey.iKey;
		}
		rc = (sxi32)(iA-iB);
	}
	/* Comparison result */
	return rc;
}
/*
 * Node comparison callback.
 * Used by: [rsort(),arsort()];
 */
static sxi32 HashmapCmpCallback3(ph7_hashmap_node *pA,ph7_hashmap_node *pB,void *pCmpData)
{
	ph7_value sA,sB;
	sxi32 iFlags;
	int rc;
	if( pCmpData == 0 ){
		/* Perform a standard comparison */
		rc = HashmapNodeCmp(pA,pB,FALSE);
		return -rc;
	}
	iFlags = SX_PTR_TO_INT(pCmpData);
	/* Duplicate node values */
	PH7_MemObjInit(pA->pMap->pVm,&sA);
	PH7_MemObjInit(pA->pMap->pVm,&sB);
	PH7_HashmapExtractNodeValue(pA,&sA,FALSE);
	PH7_HashmapExtractNodeValue(pB,&sB,FALSE);
	if( iFlags == 5 ){
		/* String cast */
		if( (sA.iFlags & MEMOBJ_STRING) == 0 ){
			PH7_MemObjToString(&sA);
		}
		if( (sB.iFlags & MEMOBJ_STRING) == 0 ){
			PH7_MemObjToString(&sB);
		}
	}else{
		/* Numeric cast */
		PH7_MemObjToNumeric(&sA);
		PH7_MemObjToNumeric(&sB);
	}
	/* Perform the comparison */
	rc = PH7_MemObjCmp(&sA,&sB,FALSE,0);
	PH7_MemObjRelease(&sA);
	PH7_MemObjRelease(&sB);
	return -rc;
}
/*
 * Node comparison callback: Invoke an user-defined callback for the purpose of node comparison.
 * used-by: [usort(),uasort()]
 */
static sxi32 HashmapCmpCallback4(ph7_hashmap_node *pA,ph7_hashmap_node *pB,void *pCmpData)
{
	ph7_value sResult,*pCallback;
	ph7_value *pV1,*pV2;
	ph7_value *apArg[2];  /* Callback arguments */
	sxi32 rc;
	/* Point to the desired callback */
	pCallback = (ph7_value *)pCmpData;
	/* initialize the result value */
	PH7_MemObjInit(pA->pMap->pVm,&sResult);
	/* Extract nodes values */
	pV1 = HashmapExtractNodeValue(pA);
	pV2 = HashmapExtractNodeValue(pB);
	apArg[0] = pV1;
	apArg[1] = pV2;
	/* Invoke the callback */
	rc = PH7_VmCallUserFunction(pA->pMap->pVm,pCallback,2,apArg,&sResult);
	if( rc != SXRET_OK ){
		/* An error occured while calling user defined function [i.e: not defined] */
		rc = -1; /* Set a dummy result */
	}else{
		/* Extract callback result */
		if((sResult.iFlags & MEMOBJ_INT) == 0 ){
			/* Perform an int cast */
			PH7_MemObjToInteger(&sResult);
		}
		rc = (sxi32)sResult.x.iVal;
	}
	PH7_MemObjRelease(&sResult);
	/* Callback result */
	return rc;
}
/*
 * Node comparison callback: Compare nodes by keys only.
 * used-by: [krsort()]
 */
static sxi32 HashmapCmpCallback5(ph7_hashmap_node *pA,ph7_hashmap_node *pB,void *pCmpData)
{
	sxi32 rc;
	SXUNUSED(pCmpData); /* cc warning */
	if( pA->iType == HASHMAP_BLOB_NODE && pB->iType == HASHMAP_BLOB_NODE ){
		/* Perform a string comparison */
		rc = SyBlobCmp(&pA->xKey.sKey,&pB->xKey.sKey);
	}else{
		SyString sStr;
		sxi64 iA,iB;
		/* Perform a numeric comparison */
		if( pA->iType == HASHMAP_BLOB_NODE ){
			/* Cast to 64-bit integer */
			SyStringInitFromBuf(&sStr,SyBlobData(&pA->xKey.sKey),SyBlobLength(&pA->xKey.sKey));
			if( sStr.nByte < 1 ){
				iA = 0;
			}else{
				SyStrToInt64(sStr.zString,sStr.nByte,(void *)&iA,0);
			}
		}else{
			iA = pA->xKey.iKey;
		}
		if( pB->iType == HASHMAP_BLOB_NODE ){
			/* Cast to 64-bit integer */
			SyStringInitFromBuf(&sStr,SyBlobData(&pB->xKey.sKey),SyBlobLength(&pB->xKey.sKey));
			if( sStr.nByte < 1 ){
				iB = 0;
			}else{
				SyStrToInt64(sStr.zString,sStr.nByte,(void *)&iB,0);
			}
		}else{
			iB = pB->xKey.iKey;
		}
		rc = (sxi32)(iA-iB);
	}
	return -rc; /* Reverse result */
}
/*
 * Node comparison callback: Invoke an user-defined callback for the purpose of node comparison.
 * used-by: [uksort()]
 */
static sxi32 HashmapCmpCallback6(ph7_hashmap_node *pA,ph7_hashmap_node *pB,void *pCmpData)
{
	ph7_value sResult,*pCallback;
	ph7_value *apArg[2];  /* Callback arguments */
	ph7_value sK1,sK2;
	sxi32 rc;
	/* Point to the desired callback */
	pCallback = (ph7_value *)pCmpData;
	/* initialize the result value */
	PH7_MemObjInit(pA->pMap->pVm,&sResult);
	PH7_MemObjInit(pA->pMap->pVm,&sK1);
	PH7_MemObjInit(pA->pMap->pVm,&sK2);
	/* Extract nodes keys */
	PH7_HashmapExtractNodeKey(pA,&sK1);
	PH7_HashmapExtractNodeKey(pB,&sK2);
	apArg[0] = &sK1;
	apArg[1] = &sK2;
	/* Mark keys as constants */
	sK1.nIdx = SXU32_HIGH;
	sK2.nIdx = SXU32_HIGH;
	/* Invoke the callback */
	rc = PH7_VmCallUserFunction(pA->pMap->pVm,pCallback,2,apArg,&sResult);
	if( rc != SXRET_OK ){
		/* An error occured while calling user defined function [i.e: not defined] */
		rc = -1; /* Set a dummy result */
	}else{
		/* Extract callback result */
		if((sResult.iFlags & MEMOBJ_INT) == 0 ){
			/* Perform an int cast */
			PH7_MemObjToInteger(&sResult);
		}
		rc = (sxi32)sResult.x.iVal;
	}
	PH7_MemObjRelease(&sResult);
	PH7_MemObjRelease(&sK1);
	PH7_MemObjRelease(&sK2);
	/* Callback result */
	return rc;
}
/*
 * Node comparison callback: Random node comparison.
 * used-by: [shuffle()]
 */
static sxi32 HashmapCmpCallback7(ph7_hashmap_node *pA,ph7_hashmap_node *pB,void *pCmpData)
{
	sxu32 n;
	SXUNUSED(pB); /* cc warning */
	SXUNUSED(pCmpData);
	/* Grab a random number */
	n = PH7_VmRandomNum(pA->pMap->pVm);
	/* if the random number is odd then the first node 'pA' is greater then
	 * the second node 'pB'. Otherwise the reverse is assumed.
	 */
	return n&1 ? 1 : -1;
}
/*
 * Rehash all nodes keys after a merge-sort have been applied.
 * Used by [sort(),usort() and rsort()].
 */
static void HashmapSortRehash(ph7_hashmap *pMap)
{
	ph7_hashmap_node *p,*pLast;
	sxu32 i;
	/* Rehash all entries */
	pLast = p = pMap->pFirst;
	pMap->iNextIdx = 0; /* Reset the automatic index */
	i = 0;
	for( ;; ){
		if( i >= pMap->nEntry ){
			pMap->pLast = pLast; /* Fix the last link broken by the merge-sort */
			break;
		}
		if( p->iType == HASHMAP_BLOB_NODE ){
			/* Do not maintain index association as requested by the PHP specification */
			SyBlobRelease(&p->xKey.sKey);
			/* Change key type */
			p->iType = HASHMAP_INT_NODE;
		}
		HashmapRehashIntNode(p);
		/* Point to the next entry */
		i++;
		pLast = p;
		p = p->pPrev; /* Reverse link */
	}
}
/*
 * Array functions implementation.
 * Status:
 *  Stable.
 */
/*
 * bool sort(array &$array[,int $sort_flags = SORT_REGULAR ] )
 * Sort an array.
 * Parameters
 *  $array
 *   The input array.
 * $sort_flags
 *  The optional second parameter sort_flags may be used to modify the sorting behavior using these values:
 *  Sorting type flags:
 *   SORT_REGULAR - compare items normally (don't change types)
 *   SORT_NUMERIC - compare items numerically
 *   SORT_STRING - compare items as strings
 * Return
 *  TRUE on success or FALSE on failure.
 * 
 */
static int ph7_hashmap_sort(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	ph7_hashmap *pMap;
	/* Make sure we are dealing with a valid hashmap */
	if( nArg < 1 || !ph7_value_is_array(apArg[0]) ){
		/* Missing/Invalid arguments,return FALSE */
		ph7_result_bool(pCtx,0);
		return PH7_OK;
	}
	/* Point to the internal representation of the input hashmap */
	pMap = (ph7_hashmap *)apArg[0]->x.pOther;
	if( pMap->nEntry > 1 ){
		sxi32 iCmpFlags = 0;
		if( nArg > 1 ){
			/* Extract comparison flags */
			iCmpFlags = ph7_value_to_int(apArg[1]);
			if( iCmpFlags == 3 /* SORT_REGULAR */ ){
				iCmpFlags = 0; /* Standard comparison */
			}
		}
		/* Do the merge sort */
		HashmapMergeSort(pMap,HashmapCmpCallback1,SX_INT_TO_PTR(iCmpFlags));
		/* Rehash [Do not maintain index association as requested by the PHP specification] */
		HashmapSortRehash(pMap);
	}
	/* All done,return TRUE */
	ph7_result_bool(pCtx,1);
	return PH7_OK;
}
/*
 * bool asort(array &$array[,int $sort_flags = SORT_REGULAR ] )
 *  Sort an array and maintain index association.
 * Parameters
 *  $array
 *   The input array.
 * $sort_flags
 *  The optional second parameter sort_flags may be used to modify the sorting behavior using these values:
 *  Sorting type flags:
 *   SORT_REGULAR - compare items normally (don't change types)
 *   SORT_NUMERIC - compare items numerically
 *   SORT_STRING - compare items as strings
 * Return
 *  TRUE on success or FALSE on failure.
 */
static int ph7_hashmap_asort(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	ph7_hashmap *pMap;
	/* Make sure we are dealing with a valid hashmap */
	if( nArg < 1 || !ph7_value_is_array(apArg[0]) ){
		/* Missing/Invalid arguments,return FALSE */
		ph7_result_bool(pCtx,0);
		return PH7_OK;
	}
	/* Point to the internal representation of the input hashmap */
	pMap = (ph7_hashmap *)apArg[0]->x.pOther;
	if( pMap->nEntry > 1 ){
		sxi32 iCmpFlags = 0;
		if( nArg > 1 ){
			/* Extract comparison flags */
			iCmpFlags = ph7_value_to_int(apArg[1]);
			if( iCmpFlags == 3 /* SORT_REGULAR */ ){
				iCmpFlags = 0; /* Standard comparison */
			}
		}
		/* Do the merge sort */
		HashmapMergeSort(pMap,HashmapCmpCallback1,SX_INT_TO_PTR(iCmpFlags));
		/* Fix the last link broken by the merge */
		while(pMap->pLast->pPrev){
			pMap->pLast = pMap->pLast->pPrev;
		}
	}
	/* All done,return TRUE */
	ph7_result_bool(pCtx,1);
	return PH7_OK;
}
/*
 * bool arsort(array &$array[,int $sort_flags = SORT_REGULAR ] )
 *  Sort an array in reverse order and maintain index association.
 * Parameters
 *  $array
 *   The input array.
 * $sort_flags
 *  The optional second parameter sort_flags may be used to modify the sorting behavior using these values:
 *  Sorting type flags:
 *   SORT_REGULAR - compare items normally (don't change types)
 *   SORT_NUMERIC - compare items numerically
 *   SORT_STRING - compare items as strings
 * Return
 *  TRUE on success or FALSE on failure. 
 */
static int ph7_hashmap_arsort(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	ph7_hashmap *pMap;
	/* Make sure we are dealing with a valid hashmap */
	if( nArg < 1 || !ph7_value_is_array(apArg[0]) ){
		/* Missing/Invalid arguments,return FALSE */
		ph7_result_bool(pCtx,0);
		return PH7_OK;
	}
	/* Point to the internal representation of the input hashmap */
	pMap = (ph7_hashmap *)apArg[0]->x.pOther;
	if( pMap->nEntry > 1 ){
		sxi32 iCmpFlags = 0;
		if( nArg > 1 ){
			/* Extract comparison flags */
			iCmpFlags = ph7_value_to_int(apArg[1]);
			if( iCmpFlags == 3 /* SORT_REGULAR */ ){
				iCmpFlags = 0; /* Standard comparison */
			}
		}
		/* Do the merge sort */
		HashmapMergeSort(pMap,HashmapCmpCallback3,SX_INT_TO_PTR(iCmpFlags));
		/* Fix the last link broken by the merge */
		while(pMap->pLast->pPrev){
			pMap->pLast = pMap->pLast->pPrev;
		}
	}
	/* All done,return TRUE */
	ph7_result_bool(pCtx,1);
	return PH7_OK;
}
/*
 * bool ksort(array &$array[,int $sort_flags = SORT_REGULAR ] )
 *  Sort an array by key.
 * Parameters
 *  $array
 *   The input array.
 * $sort_flags
 *  The optional second parameter sort_flags may be used to modify the sorting behavior using these values:
 *  Sorting type flags:
 *   SORT_REGULAR - compare items normally (don't change types)
 *   SORT_NUMERIC - compare items numerically
 *   SORT_STRING - compare items as strings
 * Return
 *  TRUE on success or FALSE on failure.
 */
static int ph7_hashmap_ksort(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	ph7_hashmap *pMap;
	/* Make sure we are dealing with a valid hashmap */
	if( nArg < 1 || !ph7_value_is_array(apArg[0]) ){
		/* Missing/Invalid arguments,return FALSE */
		ph7_result_bool(pCtx,0);
		return PH7_OK;
	}
	/* Point to the internal representation of the input hashmap */
	pMap = (ph7_hashmap *)apArg[0]->x.pOther;
	if( pMap->nEntry > 1 ){
		sxi32 iCmpFlags = 0;
		if( nArg > 1 ){
			/* Extract comparison flags */
			iCmpFlags = ph7_value_to_int(apArg[1]);
			if( iCmpFlags == 3 /* SORT_REGULAR */ ){
				iCmpFlags = 0; /* Standard comparison */
			}
		}
		/* Do the merge sort */
		HashmapMergeSort(pMap,HashmapCmpCallback2,SX_INT_TO_PTR(iCmpFlags));
		/* Fix the last link broken by the merge */
		while(pMap->pLast->pPrev){
			pMap->pLast = pMap->pLast->pPrev;
		}
	}
	/* All done,return TRUE */
	ph7_result_bool(pCtx,1);
	return PH7_OK;
}
/*
 * bool krsort(array &$array[,int $sort_flags = SORT_REGULAR ] )
 *  Sort an array by key in reverse order.
 * Parameters
 *  $array
 *   The input array.
 * $sort_flags
 *  The optional second parameter sort_flags may be used to modify the sorting behavior using these values:
 *  Sorting type flags:
 *   SORT_REGULAR - compare items normally (don't change types)
 *   SORT_NUMERIC - compare items numerically
 *   SORT_STRING - compare items as strings
 * Return
 *  TRUE on success or FALSE on failure.
 */
static int ph7_hashmap_krsort(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	ph7_hashmap *pMap;
	/* Make sure we are dealing with a valid hashmap */
	if( nArg < 1 || !ph7_value_is_array(apArg[0]) ){
		/* Missing/Invalid arguments,return FALSE */
		ph7_result_bool(pCtx,0);
		return PH7_OK;
	}
	/* Point to the internal representation of the input hashmap */
	pMap = (ph7_hashmap *)apArg[0]->x.pOther;
	if( pMap->nEntry > 1 ){
		sxi32 iCmpFlags = 0;
		if( nArg > 1 ){
			/* Extract comparison flags */
			iCmpFlags = ph7_value_to_int(apArg[1]);
			if( iCmpFlags == 3 /* SORT_REGULAR */ ){
				iCmpFlags = 0; /* Standard comparison */
			}
		}
		/* Do the merge sort */
		HashmapMergeSort(pMap,HashmapCmpCallback5,SX_INT_TO_PTR(iCmpFlags));
		/* Fix the last link broken by the merge */
		while(pMap->pLast->pPrev){
			pMap->pLast = pMap->pLast->pPrev;
		}
	}
	/* All done,return TRUE */
	ph7_result_bool(pCtx,1);
	return PH7_OK;
}
/*
 * bool rsort(array &$array[,int $sort_flags = SORT_REGULAR ] )
 * Sort an array in reverse order.
 * Parameters
 *  $array
 *   The input array.
 * $sort_flags
 *  The optional second parameter sort_flags may be used to modify the sorting behavior using these values:
 *  Sorting type flags:
 *   SORT_REGULAR - compare items normally (don't change types)
 *   SORT_NUMERIC - compare items numerically
 *   SORT_STRING - compare items as strings
 * Return
 *  TRUE on success or FALSE on failure.
 */
static int ph7_hashmap_rsort(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	ph7_hashmap *pMap;
	/* Make sure we are dealing with a valid hashmap */
	if( nArg < 1 || !ph7_value_is_array(apArg[0]) ){
		/* Missing/Invalid arguments,return FALSE */
		ph7_result_bool(pCtx,0);
		return PH7_OK;
	}
	/* Point to the internal representation of the input hashmap */
	pMap = (ph7_hashmap *)apArg[0]->x.pOther;
	if( pMap->nEntry > 1 ){
		sxi32 iCmpFlags = 0;
		if( nArg > 1 ){
			/* Extract comparison flags */
			iCmpFlags = ph7_value_to_int(apArg[1]);
			if( iCmpFlags == 3 /* SORT_REGULAR */ ){
				iCmpFlags = 0; /* Standard comparison */
			}
		}
		/* Do the merge sort */
		HashmapMergeSort(pMap,HashmapCmpCallback3,SX_INT_TO_PTR(iCmpFlags));
		/* Rehash [Do not maintain index association as requested by the PHP specification] */
		HashmapSortRehash(pMap);
	}
	/* All done,return TRUE */
	ph7_result_bool(pCtx,1);
	return PH7_OK;
}
/*
 * bool usort(array &$array,callable $cmp_function)
 *  Sort an array by values using a user-defined comparison function.
 * Parameters
 *  $array
 *   The input array.
 * $cmp_function
 *  The comparison function must return an integer less than, equal to, or greater
 *  than zero if the first argument is considered to be respectively less than, equal
 *  to, or greater than the second.
 *    int callback ( mixed $a, mixed $b )
 * Return
 *  TRUE on success or FALSE on failure.
 */
static int ph7_hashmap_usort(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	ph7_hashmap *pMap;
	/* Make sure we are dealing with a valid hashmap */
	if( nArg < 1 || !ph7_value_is_array(apArg[0]) ){
		/* Missing/Invalid arguments,return FALSE */
		ph7_result_bool(pCtx,0);
		return PH7_OK;
	}
	/* Point to the internal representation of the input hashmap */
	pMap = (ph7_hashmap *)apArg[0]->x.pOther;
	if( pMap->nEntry > 1 ){
		ph7_value *pCallback = 0;
		ProcNodeCmp xCmp;
		xCmp = HashmapCmpCallback4; /* User-defined function as the comparison callback */
		if( nArg > 1 && ph7_value_is_callable(apArg[1]) ){
			/* Point to the desired callback */
			pCallback = apArg[1];
		}else{
			/* Use the default comparison function */
			xCmp = HashmapCmpCallback1;
		}
		/* Do the merge sort */
		HashmapMergeSort(pMap,xCmp,pCallback);
		/* Rehash [Do not maintain index association as requested by the PHP specification] */
		HashmapSortRehash(pMap);
	}
	/* All done,return TRUE */
	ph7_result_bool(pCtx,1);
	return PH7_OK;
}
/*
 * bool uasort(array &$array,callable $cmp_function)
 *  Sort an array by values using a user-defined comparison function
 *  and maintain index association.
 * Parameters
 *  $array
 *   The input array.
 * $cmp_function
 *  The comparison function must return an integer less than, equal to, or greater
 *  than zero if the first argument is considered to be respectively less than, equal
 *  to, or greater than the second.
 *    int callback ( mixed $a, mixed $b )
 * Return
 *  TRUE on success or FALSE on failure.
 */
static int ph7_hashmap_uasort(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	ph7_hashmap *pMap;
	/* Make sure we are dealing with a valid hashmap */
	if( nArg < 1 || !ph7_value_is_array(apArg[0]) ){
		/* Missing/Invalid arguments,return FALSE */
		ph7_result_bool(pCtx,0);
		return PH7_OK;
	}
	/* Point to the internal representation of the input hashmap */
	pMap = (ph7_hashmap *)apArg[0]->x.pOther;
	if( pMap->nEntry > 1 ){
		ph7_value *pCallback = 0;
		ProcNodeCmp xCmp;
		xCmp = HashmapCmpCallback4; /* User-defined function as the comparison callback */
		if( nArg > 1 && ph7_value_is_callable(apArg[1]) ){
			/* Point to the desired callback */
			pCallback = apArg[1];
		}else{
			/* Use the default comparison function */
			xCmp = HashmapCmpCallback1;
		}
		/* Do the merge sort */
		HashmapMergeSort(pMap,xCmp,pCallback);
		/* Fix the last link broken by the merge */
		while(pMap->pLast->pPrev){
			pMap->pLast = pMap->pLast->pPrev;
		}
	}
	/* All done,return TRUE */
	ph7_result_bool(pCtx,1);
	return PH7_OK;
}
/*
 * bool uksort(array &$array,callable $cmp_function)
 *  Sort an array by keys using a user-defined comparison
 *  function and maintain index association.
 * Parameters
 *  $array
 *   The input array.
 * $cmp_function
 *  The comparison function must return an integer less than, equal to, or greater
 *  than zero if the first argument is considered to be respectively less than, equal
 *  to, or greater than the second.
 *    int callback ( mixed $a, mixed $b )
 * Return
 *  TRUE on success or FALSE on failure.
 */
static int ph7_hashmap_uksort(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	ph7_hashmap *pMap;
	/* Make sure we are dealing with a valid hashmap */
	if( nArg < 1 || !ph7_value_is_array(apArg[0]) ){
		/* Missing/Invalid arguments,return FALSE */
		ph7_result_bool(pCtx,0);
		return PH7_OK;
	}
	/* Point to the internal representation of the input hashmap */
	pMap = (ph7_hashmap *)apArg[0]->x.pOther;
	if( pMap->nEntry > 1 ){
		ph7_value *pCallback = 0;
		ProcNodeCmp xCmp;
		xCmp = HashmapCmpCallback6; /* User-defined function as the comparison callback */
		if( nArg > 1 && ph7_value_is_callable(apArg[1]) ){
			/* Point to the desired callback */
			pCallback = apArg[1];
		}else{
			/* Use the default comparison function */
			xCmp = HashmapCmpCallback2;
		}
		/* Do the merge sort */
		HashmapMergeSort(pMap,xCmp,pCallback);
		/* Fix the last link broken by the merge */
		while(pMap->pLast->pPrev){
			pMap->pLast = pMap->pLast->pPrev;
		}
	}
	/* All done,return TRUE */
	ph7_result_bool(pCtx,1);
	return PH7_OK;
}
/*
 * bool shuffle(array &$array)
 *  shuffles (randomizes the order of the elements in) an array. 
 * Parameters
 *  $array
 *   The input array.
 * Return
 *  TRUE on success or FALSE on failure.
 * 
 */
static int ph7_hashmap_shuffle(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	ph7_hashmap *pMap;
	/* Make sure we are dealing with a valid hashmap */
	if( nArg < 1 || !ph7_value_is_array(apArg[0]) ){
		/* Missing/Invalid arguments,return FALSE */
		ph7_result_bool(pCtx,0);
		return PH7_OK;
	}
	/* Point to the internal representation of the input hashmap */
	pMap = (ph7_hashmap *)apArg[0]->x.pOther;
	if( pMap->nEntry > 1 ){
		/* Do the merge sort */
		HashmapMergeSort(pMap,HashmapCmpCallback7,0);
		/* Fix the last link broken by the merge */
		while(pMap->pLast->pPrev){
			pMap->pLast = pMap->pLast->pPrev;
		}
	}
	/* All done,return TRUE */
	ph7_result_bool(pCtx,1);
	return PH7_OK;
}
/*
 * int count(array $var [, int $mode = COUNT_NORMAL ])
 *   Count all elements in an array, or something in an object.
 * Parameters
 *  $var
 *   The array or the object.
 * $mode
 *  If the optional mode parameter is set to COUNT_RECURSIVE (or 1), count()
 *  will recursively count the array. This is particularly useful for counting 
 *  all the elements of a multidimensional array. count() does not detect infinite
 *  recursion.
 * Return
 *  Returns the number of elements in the array.
 */
static int ph7_hashmap_count(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	int bRecursive = FALSE;
	sxi64 iCount;
	if( nArg < 1 ){
		/* Missing arguments,return 0 */
		ph7_result_int(pCtx,0);
		return PH7_OK;
	}
	if( !ph7_value_is_array(apArg[0]) ){
		/* TICKET 1433-19: Handle objects */
		int res = !ph7_value_is_null(apArg[0]);
		ph7_result_int(pCtx,res);
		return PH7_OK;
	}
	if( nArg > 1 ){
		/* Recursive count? */
		bRecursive = ph7_value_to_int(apArg[1]) == 1 /* COUNT_RECURSIVE */;
	}
	/* Count */
	iCount = HashmapCount((ph7_hashmap *)apArg[0]->x.pOther,bRecursive,0);
	ph7_result_int64(pCtx,iCount);
	return PH7_OK;
}
/*
 * bool array_key_exists(value $key,array $search)
 *  Checks if the given key or index exists in the array.
 * Parameters
 * $key
 *   Value to check.
 * $search
 *  An array with keys to check.
 * Return
 *  TRUE on success or FALSE on failure.
 */
static int ph7_hashmap_key_exists(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	sxi32 rc;
	if( nArg < 2 ){
		/* Missing arguments,return FALSE */
		ph7_result_bool(pCtx,0);
		return PH7_OK;
	}
	/* Make sure we are dealing with a valid hashmap */
	if( !ph7_value_is_array(apArg[1]) ){
		/* Invalid argument,return FALSE */
		ph7_result_bool(pCtx,0);
		return PH7_OK;
	}
	/* Perform the lookup */
	rc = PH7_HashmapLookup((ph7_hashmap *)apArg[1]->x.pOther,apArg[0],0);
	/* lookup result */
	ph7_result_bool(pCtx,rc == SXRET_OK ? 1 : 0);
	return PH7_OK;
}
/*
 * value array_pop(array $array)
 *   POP the last inserted element from the array.
 * Parameter
 *  The array to get the value from.
 * Return
 *  Poped value or NULL on failure.
 */
static int ph7_hashmap_pop(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	ph7_hashmap *pMap;
	if( nArg < 1 ){
		/* Missing arguments,return null */
		ph7_result_null(pCtx);
		return PH7_OK;
	}
	/* Make sure we are dealing with a valid hashmap */
	if( !ph7_value_is_array(apArg[0]) ){
		/* Invalid argument,return null */
		ph7_result_null(pCtx);
		return PH7_OK;
	}
	pMap = (ph7_hashmap *)apArg[0]->x.pOther;
	if( pMap->nEntry < 1 ){
		/* Noting to pop,return NULL */
		ph7_result_null(pCtx);
	}else{
		ph7_hashmap_node *pLast = pMap->pLast;
		ph7_value *pObj;
		pObj = HashmapExtractNodeValue(pLast);
		if( pObj ){
			/* Node value */
			ph7_result_value(pCtx,pObj);
			/* Unlink the node */
			PH7_HashmapUnlinkNode(pLast,TRUE);
		}else{
			ph7_result_null(pCtx);
		}
		/* Reset the cursor */
		pMap->pCur = pMap->pFirst;
	}
	return PH7_OK;
}
/*
 * int array_push($array,$var,...)
 *   Push one or more elements onto the end of array. (Stack insertion)
 * Parameters
 *  array
 *    The input array.
 *  var
 *   On or more value to push.
 * Return
 *  New array count (including old items).
 */
static int ph7_hashmap_push(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	ph7_hashmap *pMap;
	sxi32 rc;
	int i;
	if( nArg < 1 ){
		/* Missing arguments,return 0 */
		ph7_result_int(pCtx,0);
		return PH7_OK;
	}
	/* Make sure we are dealing with a valid hashmap */
	if( !ph7_value_is_array(apArg[0]) ){
		/* Invalid argument,return 0 */
		ph7_result_int(pCtx,0);
		return PH7_OK;
	}
	/* Point to the internal representation of the input hashmap */
	pMap = (ph7_hashmap *)apArg[0]->x.pOther;
	/* Start pushing given values */
	for( i = 1 ; i < nArg ; ++i ){
		rc = PH7_HashmapInsert(pMap,0,apArg[i]);
		if( rc != SXRET_OK ){
			break;
		}
	}
	/* Return the new count */
	ph7_result_int64(pCtx,(sxi64)pMap->nEntry);
	return PH7_OK;
}
/*
 * value array_shift(array $array)
 *   Shift an element off the beginning of array.
 * Parameter
 *  The array to get the value from.
 * Return
 *  Shifted value or NULL on failure.
 */
static int ph7_hashmap_shift(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	ph7_hashmap *pMap;
	if( nArg < 1 ){
		/* Missing arguments,return null */
		ph7_result_null(pCtx);
		return PH7_OK;
	}
	/* Make sure we are dealing with a valid hashmap */
	if( !ph7_value_is_array(apArg[0]) ){
		/* Invalid argument,return null */
		ph7_result_null(pCtx);
		return PH7_OK;
	}
	/* Point to the internal representation of the hashmap */
	pMap = (ph7_hashmap *)apArg[0]->x.pOther;
	if( pMap->nEntry < 1 ){
		/* Empty hashmap,return NULL */
		ph7_result_null(pCtx);
	}else{
		ph7_hashmap_node *pEntry = pMap->pFirst;
		ph7_value *pObj;
		sxu32 n;
		pObj = HashmapExtractNodeValue(pEntry);
		if( pObj ){
			/* Node value */
			ph7_result_value(pCtx,pObj);
			/* Unlink the first node */
			PH7_HashmapUnlinkNode(pEntry,TRUE);
		}else{
			ph7_result_null(pCtx);
		}
		/* Rehash all int keys */
		n = pMap->nEntry;
		pEntry = pMap->pFirst;
		pMap->iNextIdx = 0; /* Reset the automatic index */
		for(;;){
			if( n < 1 ){
				break;
			}
			if( pEntry->iType == HASHMAP_INT_NODE ){
				HashmapRehashIntNode(pEntry);
			}
			/* Point to the next entry */
			pEntry = pEntry->pPrev; /* Reverse link */
			n--;
		}
		/* Reset the cursor */
		pMap->pCur = pMap->pFirst;
	}
	return PH7_OK;
}
/*
 * Extract the node cursor value.
 */
static sxi32 HashmapCurrentValue(ph7_context *pCtx,ph7_hashmap *pMap,int iDirection)
{
	ph7_hashmap_node *pCur = pMap->pCur;
	ph7_value *pVal;
	if( pCur == 0 ){
		/* Cursor does not point to anything,return FALSE */
		ph7_result_bool(pCtx,0);
		return PH7_OK;
	}
	if( iDirection != 0 ){
		if( iDirection > 0 ){
			/* Point to the next entry */
			pMap->pCur = pCur->pPrev; /* Reverse link */
			pCur = pMap->pCur;
		}else{
			/* Point to the previous entry */
			pMap->pCur = pCur->pNext; /* Reverse link */
			pCur = pMap->pCur;
		}
		if( pCur == 0 ){
			/* End of input reached,return FALSE */
			ph7_result_bool(pCtx,0);
			return PH7_OK;
		}
	}		
	/* Point to the desired element */
	pVal = HashmapExtractNodeValue(pCur);
	if( pVal ){
		ph7_result_value(pCtx,pVal);
	}else{
		ph7_result_bool(pCtx,0);
	}
	return PH7_OK;
}
/*
 * value current(array $array)
 *  Return the current element in an array.
 * Parameter
 *  $input: The input array.
 * Return
 *  The current() function simply returns the value of the array element that's currently
 *  being pointed to by the internal pointer. It does not move the pointer in any way.
 *  If the internal pointer points beyond the end of the elements list or the array 
 *  is empty, current() returns FALSE. 
 */
static int ph7_hashmap_current(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	if( nArg < 1 ){
		/* Missing arguments,return FALSE */
		ph7_result_bool(pCtx,0);
		return PH7_OK;
	}
	/* Make sure we are dealing with a valid hashmap */
	if( !ph7_value_is_array(apArg[0]) ){
		/* Invalid argument,return FALSE */
		ph7_result_bool(pCtx,0);
		return PH7_OK;
	}
	HashmapCurrentValue(&(*pCtx),(ph7_hashmap *)apArg[0]->x.pOther,0);
	return PH7_OK;
}
/*
 * value next(array $input)
 *  Advance the internal array pointer of an array.
 * Parameter
 *  $input: The input array.
 * Return
 *  next() behaves like current(), with one difference. It advances the internal array 
 *  pointer one place forward before returning the element value. That means it returns 
 *  the next array value and advances the internal array pointer by one. 
 */
static int ph7_hashmap_next(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	if( nArg < 1 ){
		/* Missing arguments,return FALSE */
		ph7_result_bool(pCtx,0);
		return PH7_OK;
	}
	/* Make sure we are dealing with a valid hashmap */
	if( !ph7_value_is_array(apArg[0]) ){
		/* Invalid argument,return FALSE */
		ph7_result_bool(pCtx,0);
		return PH7_OK;
	}
	HashmapCurrentValue(&(*pCtx),(ph7_hashmap *)apArg[0]->x.pOther,1);
	return PH7_OK;
}
/* 
 * value prev(array $input)
 *  Rewind the internal array pointer.
 * Parameter
 *  $input: The input array.
 * Return
 *  Returns the array value in the previous place that's pointed
 *  to by the internal array pointer, or FALSE if there are no more
 *  elements. 
 */
static int ph7_hashmap_prev(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	if( nArg < 1 ){
		/* Missing arguments,return FALSE */
		ph7_result_bool(pCtx,0);
		return PH7_OK;
	}
	/* Make sure we are dealing with a valid hashmap */
	if( !ph7_value_is_array(apArg[0]) ){
		/* Invalid argument,return FALSE */
		ph7_result_bool(pCtx,0);
		return PH7_OK;
	}
	HashmapCurrentValue(&(*pCtx),(ph7_hashmap *)apArg[0]->x.pOther,-1);
	return PH7_OK;
}
/* 
 * value end(array $input)
 *  Set the internal pointer of an array to its last element.
 * Parameter
 *  $input: The input array.
 * Return
 *  Returns the value of the last element or FALSE for empty array. 
 */
static int ph7_hashmap_end(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	ph7_hashmap *pMap;
	if( nArg < 1 ){
		/* Missing arguments,return FALSE */
		ph7_result_bool(pCtx,0);
		return PH7_OK;
	}
	/* Make sure we are dealing with a valid hashmap */
	if( !ph7_value_is_array(apArg[0]) ){
		/* Invalid argument,return FALSE */
		ph7_result_bool(pCtx,0);
		return PH7_OK;
	}
	/* Point to the internal representation of the input hashmap */
	pMap = (ph7_hashmap *)apArg[0]->x.pOther;
	/* Point to the last node */
	pMap->pCur = pMap->pLast;
	/* Return the last node value */
	HashmapCurrentValue(&(*pCtx),pMap,0);
	return PH7_OK;
}
/* 
 * value reset(array $array )
 *  Set the internal pointer of an array to its first element.
 * Parameter
 *  $input: The input array.
 * Return
 *  Returns the value of the first array element,or FALSE if the array is empty. 
 */
static int ph7_hashmap_reset(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	ph7_hashmap *pMap;
	if( nArg < 1 ){
		/* Missing arguments,return FALSE */
		ph7_result_bool(pCtx,0);
		return PH7_OK;
	}
	/* Make sure we are dealing with a valid hashmap */
	if( !ph7_value_is_array(apArg[0]) ){
		/* Invalid argument,return FALSE */
		ph7_result_bool(pCtx,0);
		return PH7_OK;
	}
	/* Point to the internal representation of the input hashmap */
	pMap = (ph7_hashmap *)apArg[0]->x.pOther;
	/* Point to the first node */
	pMap->pCur = pMap->pFirst;
	/* Return the last node value if available */
	HashmapCurrentValue(&(*pCtx),pMap,0);
	return PH7_OK;
}
/*
 * value key(array $array)
 *   Fetch a key from an array
 * Parameter
 *  $input
 *   The input array.
 * Return
 *  The key() function simply returns the key of the array element that's currently
 *  being pointed to by the internal pointer. It does not move the pointer in any way.
 *  If the internal pointer points beyond the end of the elements list or the array 
 *  is empty, key() returns NULL. 
 */
static int ph7_hashmap_simple_key(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	ph7_hashmap_node *pCur;
	ph7_hashmap *pMap;
	if( nArg < 1 ){
		/* Missing arguments,return NULL */
		ph7_result_null(pCtx);
		return PH7_OK;
	}
	/* Make sure we are dealing with a valid hashmap */
	if( !ph7_value_is_array(apArg[0]) ){
		/* Invalid argument,return NULL */
		ph7_result_null(pCtx);
		return PH7_OK;
	}
	pMap = (ph7_hashmap *)apArg[0]->x.pOther;
	pCur = pMap->pCur;
	if( pCur == 0 ){
		/* Cursor does not point to anything,return NULL */
		ph7_result_null(pCtx);
		return PH7_OK;
	}
	if( pCur->iType == HASHMAP_INT_NODE){
		/* Key is integer */
		ph7_result_int64(pCtx,pCur->xKey.iKey);
	}else{
		/* Key is blob */
		ph7_result_string(pCtx,
			(const char *)SyBlobData(&pCur->xKey.sKey),(int)SyBlobLength(&pCur->xKey.sKey));
	}
	return PH7_OK;
}
/*
 * array each(array $input)
 *  Return the current key and value pair from an array and advance the array cursor.
 * Parameter
 *  $input
 *    The input array.
 * Return
 *  Returns the current key and value pair from the array array. This pair is returned 
 *  in a four-element array, with the keys 0, 1, key, and value. Elements 0 and key 
 *  contain the key name of the array element, and 1 and value contain the data.
 *  If the internal pointer for the array points past the end of the array contents
 *  each() returns FALSE. 
 */
static int ph7_hashmap_each(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	ph7_hashmap_node *pCur;
	ph7_hashmap *pMap;
	ph7_value *pArray;
	ph7_value *pVal;
	ph7_value sKey;
	if( nArg < 1 ){
		/* Missing arguments,return FALSE */
		ph7_result_bool(pCtx,0);
		return PH7_OK;
	}
	/* Make sure we are dealing with a valid hashmap */
	if( !ph7_value_is_array(apArg[0]) ){
		/* Invalid argument,return FALSE */
		ph7_result_bool(pCtx,0);
		return PH7_OK;
	}
	/* Point to the internal representation that describe the input hashmap */
	pMap = (ph7_hashmap *)apArg[0]->x.pOther;
	if( pMap->pCur == 0 ){
		/* Cursor does not point to anything,return FALSE */
		ph7_result_bool(pCtx,0);
		return PH7_OK;
	}
	pCur = pMap->pCur;
	/* Create a new array */
	pArray = ph7_context_new_array(pCtx);
	if( pArray == 0 ){
		ph7_result_bool(pCtx,0);
		return PH7_OK;
	}
	pVal = HashmapExtractNodeValue(pCur);
	/* Insert the current value */
	ph7_array_add_intkey_elem(pArray,1,pVal);
	ph7_array_add_strkey_elem(pArray,"value",pVal);
	/* Make the key */
	if( pCur->iType == HASHMAP_INT_NODE ){
		PH7_MemObjInitFromInt(pMap->pVm,&sKey,pCur->xKey.iKey);
	}else{
		PH7_MemObjInitFromString(pMap->pVm,&sKey,0);
		PH7_MemObjStringAppend(&sKey,(const char *)SyBlobData(&pCur->xKey.sKey),SyBlobLength(&pCur->xKey.sKey));
	}
	/* Insert the current key */
	ph7_array_add_intkey_elem(pArray,0,&sKey);
	ph7_array_add_strkey_elem(pArray,"key",&sKey);
	PH7_MemObjRelease(&sKey);
	/* Advance the cursor */
	pMap->pCur = pCur->pPrev; /* Reverse link */
	/* Return the current entry */
	ph7_result_value(pCtx,pArray);
	return PH7_OK;
}
/*
 * array range(int $start,int $limit,int $step)
 *  Create an array containing a range of elements
 * Parameter
 *  start
 *   First value of the sequence.
 *  limit
 *   The sequence is ended upon reaching the limit value.
 *  step
 *  If a step value is given, it will be used as the increment between elements in the sequence.
 *  step should be given as a positive number. If not specified, step will default to 1.
 * Return
 *  An array of elements from start to limit, inclusive.
 * NOTE:
 *  Only 32/64 bit integer key is supported.
 */
static int ph7_hashmap_range(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	ph7_value *pValue,*pArray;
	sxi64 iOfft,iLimit;
	int iStep = 1;

	iOfft = iLimit = 0; /* cc -O6 */
	if( nArg > 0 ){
		/* Extract the offset */
		iOfft = ph7_value_to_int64(apArg[0]);
		if( nArg > 1 ){
			/* Extract the limit */
			iLimit = ph7_value_to_int64(apArg[1]);
			if( nArg > 2 ){
				/* Extract the increment */
				iStep = ph7_value_to_int(apArg[2]);
				if( iStep < 1 ){
					/* Only positive number are allowed */
					iStep = 1;
				}
			}
		}
	}
	/* Element container */
	pValue = ph7_context_new_scalar(pCtx);
	/* Create the new array */
	pArray = ph7_context_new_array(pCtx);
	if( pArray == 0 ){
		ph7_result_null(pCtx);
		return PH7_OK;
	}
	/* Start filling */
	while( iOfft <= iLimit ){
		ph7_value_int64(pValue,iOfft);
		/* Perform the insertion */
		ph7_array_add_elem(pArray,0/* Automatic index assign*/,pValue);
		/* Increment */
		iOfft += iStep;
	}
	/* Return the new array */
	ph7_result_value(pCtx,pArray);
	/* Dont'worry about freeing 'pValue',it will be released automatically
	 * by the virtual machine as soon we return from this foreign function.
	 */
	return PH7_OK;
}
/*
 * array array_values(array $input)
 *   Returns all the values from the input array and indexes numerically the array.
 * Parameters
 *   input: The input array.
 * Return
 *  An indexed array of values or NULL on failure.
 */
static int ph7_hashmap_values(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	ph7_hashmap_node *pNode;
	ph7_hashmap *pMap;
	ph7_value *pArray;
	ph7_value *pObj;
	sxu32 n;
	if( nArg < 1 ){
		/* Missing arguments,return NULL */
		ph7_result_null(pCtx);
		return PH7_OK;
	}
	/* Make sure we are dealing with a valid hashmap */
	if( !ph7_value_is_array(apArg[0]) ){
		/* Invalid argument,return NULL */
		ph7_result_null(pCtx);
		return PH7_OK;
	}
	/* Point to the internal representation that describe the input hashmap */
	pMap = (ph7_hashmap *)apArg[0]->x.pOther;
	/* Create a new array */
	pArray = ph7_context_new_array(pCtx);
	if( pArray == 0 ){
		ph7_result_null(pCtx);
		return PH7_OK;
	}
	/* Perform the requested operation */
	pNode = pMap->pFirst;
	for( n = 0 ; n < pMap->nEntry ; ++n ){
		pObj = HashmapExtractNodeValue(pNode);
		if( pObj ){
			/* perform the insertion */
			ph7_array_add_elem(pArray,0/* Automatic index assign */,pObj);
		}
		/* Point to the next entry */
		pNode = pNode->pPrev; /* Reverse link */
	}
	/* return the new array */
	ph7_result_value(pCtx,pArray);
	return PH7_OK;
}
/*
 * array array_keys(array $input [, val $search_value [, bool $strict = false ]] )
 *  Return all the keys or a subset of the keys of an array.
 * Parameters
 *  $input
 *   An array containing keys to return.
 * $search_value
 *   If specified, then only keys containing these values are returned.
 * $strict
 *   Determines if strict comparison (===) should be used during the search. 
 * Return
 *  An array of all the keys in input or NULL on failure.
 */
static int ph7_hashmap_keys(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	ph7_hashmap_node *pNode;
	ph7_hashmap *pMap;
	ph7_value *pArray;
	ph7_value sObj;
	ph7_value sVal;
	SyString sKey;
	int bStrict;
	sxi32 rc;
	sxu32 n;
	if( nArg < 1 ){
		/* Missing arguments,return NULL */
		ph7_result_null(pCtx);
		return PH7_OK;
	}
	/* Make sure we are dealing with a valid hashmap */
	if( !ph7_value_is_array(apArg[0]) ){
		/* Invalid argument,return NULL */
		ph7_result_null(pCtx);
		return PH7_OK;
	}
	/* Point to the internal representation of the input hashmap */
	pMap = (ph7_hashmap *)apArg[0]->x.pOther;
	/* Create a new array */
	pArray = ph7_context_new_array(pCtx);
	if( pArray == 0 ){
		ph7_result_null(pCtx);
		return PH7_OK;
	}
	bStrict = FALSE;
	if( nArg > 2 && ph7_value_is_bool(apArg[2]) ){
		bStrict = ph7_value_to_bool(apArg[2]);
	}
	/* Perform the requested operation */
	pNode = pMap->pFirst;
	PH7_MemObjInit(pMap->pVm,&sVal);
	for( n = 0 ; n < pMap->nEntry ; ++n ){
		if( pNode->iType == HASHMAP_INT_NODE ){
			PH7_MemObjInitFromInt(pMap->pVm,&sObj,pNode->xKey.iKey);
		}else{
			SyStringInitFromBuf(&sKey,SyBlobData(&pNode->xKey.sKey),SyBlobLength(&pNode->xKey.sKey));
			PH7_MemObjInitFromString(pMap->pVm,&sObj,&sKey);
		}
		rc = 0;
		if( nArg > 1 ){
			ph7_value *pValue = HashmapExtractNodeValue(pNode);
			if( pValue ){
				PH7_MemObjLoad(pValue,&sVal);
				/* Filter key */
				rc = ph7_value_compare(&sVal,apArg[1],bStrict);
				PH7_MemObjRelease(pValue);
			}
		}
		if( rc == 0 ){
			/* Perform the insertion */
			ph7_array_add_elem(pArray,0,&sObj);
		}
		PH7_MemObjRelease(&sObj);
		/* Point to the next entry */
		pNode = pNode->pPrev; /* Reverse link */
	}
	/* return the new array */
	ph7_result_value(pCtx,pArray);
	return PH7_OK;
}
/*
 * bool array_same(array $arr1,array $arr2)
 *  Return TRUE if the given arrays are the same instance.
 *  This function is useful under PH7 since arrays are passed
 *  by reference unlike the zend engine which use pass by values.
 * Parameters
 *  $arr1
 *   First array
 *  $arr2
 *   Second array
 * Return
 *  TRUE if the arrays are the same instance.FALSE otherwise.
 * Note
 *  This function is a symisc eXtension.
 */
static int ph7_hashmap_same(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	ph7_hashmap *p1,*p2;
	int rc;
	if( nArg < 2 || !ph7_value_is_array(apArg[0]) || !ph7_value_is_array(apArg[1]) ){
		/* Missing or invalid arguments,return FALSE*/
		ph7_result_bool(pCtx,0);
		return PH7_OK;
	}
	/* Point to the hashmaps */
	p1 = (ph7_hashmap *)apArg[0]->x.pOther;
	p2 = (ph7_hashmap *)apArg[1]->x.pOther;
	rc = (p1 == p2);
	/* Same instance? */
	ph7_result_bool(pCtx,rc);
	return PH7_OK;
}
/*
 * array array_merge(array $array1,...)
 *  Merge one or more arrays.
 * Parameters
 *  $array1
 *    Initial array to merge.
 *  ...
 *   More array to merge.
 * Return
 *  The resulting array.
 */
static int ph7_hashmap_merge(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	ph7_hashmap *pMap,*pSrc;
	ph7_value *pArray;
	int i;
	if( nArg < 1 ){
		/* Missing arguments,return NULL */
		ph7_result_null(pCtx);
		return PH7_OK;
	}
	/* Create a new array */
	pArray = ph7_context_new_array(pCtx);
	if( pArray == 0 ){
		ph7_result_null(pCtx);
		return PH7_OK;
	}
	/* Point to the internal representation of the hashmap */
	pMap = (ph7_hashmap *)pArray->x.pOther;
	/* Start merging */
	for( i = 0 ; i < nArg ; i++ ){
		/* Make sure we are dealing with a valid hashmap */
		if( !ph7_value_is_array(apArg[i]) ){
			/* Insert scalar value */
			ph7_array_add_elem(pArray,0,apArg[i]);
		}else{
			pSrc = (ph7_hashmap *)apArg[i]->x.pOther;
			/* Merge the two hashmaps */
			HashmapMerge(pSrc,pMap);
		}
	}
	/* Return the freshly created array */
	ph7_result_value(pCtx,pArray);
	return PH7_OK;
}
/*
 * array array_copy(array $source)
 *  Make a blind copy of the target array.
 * Parameters
 *  $source
 *   Target array
 * Return
 *  Copy of the target array on success.NULL otherwise.
 * Note
 *  This function is a symisc eXtension.
 */
static int ph7_hashmap_copy(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	ph7_hashmap *pMap;
	ph7_value *pArray;
	if( nArg < 1 ){
		/* Missing arguments,return NULL */
		ph7_result_null(pCtx);
		return PH7_OK;
	}
	/* Create a new array */
	pArray = ph7_context_new_array(pCtx);
	if( pArray == 0 ){
		ph7_result_null(pCtx);
		return PH7_OK;
	}
	/* Point to the internal representation of the hashmap */
	pMap = (ph7_hashmap *)pArray->x.pOther;
	if( ph7_value_is_array(apArg[0])){
		/* Point to the internal representation of the source */
		ph7_hashmap *pSrc = (ph7_hashmap *)apArg[0]->x.pOther;
		/* Perform the copy */
		PH7_HashmapDup(pSrc,pMap);
	}else{
		/* Simple insertion */
		PH7_HashmapInsert(pMap,0/* Automatic index assign*/,apArg[0]); 
	}
	/* Return the duplicated array */
	ph7_result_value(pCtx,pArray);
	return PH7_OK;
}
/*
 * bool array_erase(array $source)
 *  Remove all elements from a given array.
 * Parameters
 *  $source
 *   Target array
 * Return
 *  TRUE on success.FALSE otherwise.
 * Note
 *  This function is a symisc eXtension.
 */
static int ph7_hashmap_erase(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	ph7_hashmap *pMap;
	if( nArg < 1 ){
		/* Missing arguments */
		ph7_result_bool(pCtx,0);
		return PH7_OK;
	}
	/* Point to the target hashmap */
	pMap = (ph7_hashmap *)apArg[0]->x.pOther;
	/* Erase */
	PH7_HashmapRelease(pMap,FALSE);
	return PH7_OK;
}
/*
 * array array_slice(array $array,int $offset [,int $length [, bool $preserve_keys = false ]])
 *  Extract a slice of the array.
 * Parameters
 *  $array
 *    The input array.
 * $offset
 *    If offset is non-negative, the sequence will start at that offset in the array.
 *    If offset is negative, the sequence will start that far from the end of the array.
 * $length (optional)
 *    If length is given and is positive, then the sequence will have that many elements 
 *    in it. If length is given and is negative then the sequence will stop that many 
 *   elements from the end of the array. If it is omitted, then the sequence will have
 *   everything from offset up until the end of the array.
 * $preserve_keys (optional)
 *    Note that array_slice() will reorder and reset the array indices by default. 
 *    You can change this behaviour by setting preserve_keys to TRUE.
 * Return
 *   The new slice.
 */
static int ph7_hashmap_slice(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	ph7_hashmap *pMap,*pSrc;
	ph7_hashmap_node *pCur;
	ph7_value *pArray;
	int iLength,iOfft;
	int bPreserve;
	sxi32 rc;
	if( nArg < 2 || !ph7_value_is_array(apArg[0]) ){
		/* Missing/Invalid arguments,return NULL */
		ph7_result_null(pCtx);
		return PH7_OK;
	}
	/* Point the internal representation of the target array */
	pSrc = (ph7_hashmap *)apArg[0]->x.pOther;
	bPreserve = FALSE;
	/* Get the offset */
	iOfft = ph7_value_to_int(apArg[1]);
	if( iOfft < 0 ){
		iOfft = (int)pSrc->nEntry + iOfft;
	}
	if( iOfft < 0 || iOfft > (int)pSrc->nEntry ){
		/* Invalid offset,return the last entry */
		iOfft = (int)pSrc->nEntry - 1;
	}
	/* Get the length */
	iLength = (int)pSrc->nEntry - iOfft;
	if( nArg > 2 ){
		iLength = ph7_value_to_int(apArg[2]);
		if( iLength < 0 ){
			iLength = ((int)pSrc->nEntry + iLength) - iOfft;
		}
		if( iLength < 0 || iOfft + iLength >= (int)pSrc->nEntry ){
			iLength = (int)pSrc->nEntry - iOfft;
		}
		if( nArg > 3 && ph7_value_is_bool(apArg[3]) ){
			bPreserve = ph7_value_to_bool(apArg[3]);
		}
	}
	/* Create a new array */
	pArray = ph7_context_new_array(pCtx);
	if( pArray == 0 ){
		ph7_result_null(pCtx);
		return PH7_OK;
	}
	if( iLength < 1 ){
		/* Don't bother processing,return the empty array */
		ph7_result_value(pCtx,pArray);
		return PH7_OK;
	}
	/* Point to the desired entry */
	pCur = pSrc->pFirst;
	for(;;){
		if( iOfft < 1 ){
			break;
		}
		/* Point to the next entry */
		pCur = pCur->pPrev; /* Reverse link */
		iOfft--;
	}
	/* Point to the internal representation of the hashmap */
	pMap = (ph7_hashmap *)pArray->x.pOther;
	for(;;){
		if( iLength < 1 ){
			break;
		}
		rc = HashmapInsertNode(pMap,pCur,bPreserve);
		if( rc != SXRET_OK ){
			break;
		}
		/* Point to the next entry */
		pCur = pCur->pPrev; /* Reverse link */
		iLength--;
	}
	/* Return the freshly created array */
	ph7_result_value(pCtx,pArray);
	return PH7_OK;
}
/*
 * array array_splice(array $array,int $offset [,int $length [,value $replacement ]])
 *  Remove a portion of the array and replace it with something else.
 * Parameters
 *  $array
 *    The input array.
 * $offset
 *    If offset is positive then the start of removed portion is at that offset from 
 *    the beginning of the input array. If offset is negative then it starts that far
 *    from the end of the input array. 
 * $length (optional)
 *    If length is omitted, removes everything from offset to the end of the array.
 *    If length is specified and is positive, then that many elements will be removed.
 *    If length is specified and is negative then the end of the removed portion will 
 *    be that many elements from the end of the array.
 * $replacement (optional)
 *  If replacement array is specified, then the removed elements are replaced 
 *  with elements from this array.
 *  If offset and length are such that nothing is removed, then the elements 
 *  from the replacement array are inserted in the place specified by the offset.
 *  Note that keys in replacement array are not preserved.
 *  If replacement is just one element it is not necessary to put array() around
 *  it, unless the element is an array itself, an object or NULL.
 * Return
 *   A new array consisting of the extracted elements. 
 */
static int ph7_hashmap_splice(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	ph7_hashmap_node *pCur,*pPrev,*pRnode;
	ph7_value *pArray,*pRvalue,*pOld;
	ph7_hashmap *pMap,*pSrc,*pRep;
	int iLength,iOfft;
	sxi32 rc;
	if( nArg < 2 || !ph7_value_is_array(apArg[0]) ){
		/* Missing/Invalid arguments,return NULL */
		ph7_result_null(pCtx);
		return PH7_OK;
	}
	/* Point the internal representation of the target array */
	pSrc = (ph7_hashmap *)apArg[0]->x.pOther;
	/* Get the offset */
	iOfft = ph7_value_to_int(apArg[1]);
	if( iOfft < 0 ){
		iOfft = (int)pSrc->nEntry + iOfft;
	}
	if( iOfft < 0 || iOfft > (int)pSrc->nEntry ){
		/* Invalid offset,remove the last entry */
		iOfft = (int)pSrc->nEntry - 1;
	}
	/* Get the length */
	iLength = (int)pSrc->nEntry - iOfft;
	if( nArg > 2 ){
		iLength = ph7_value_to_int(apArg[2]);
		if( iLength < 0 ){
			iLength = ((int)pSrc->nEntry + iLength) - iOfft;
		}
		if( iLength < 0 || iOfft + iLength >= (int)pSrc->nEntry ){
			iLength = (int)pSrc->nEntry - iOfft;
		}
	}
	/* Create a new array */
	pArray = ph7_context_new_array(pCtx);
	if( pArray == 0 ){
		ph7_result_null(pCtx);
		return PH7_OK;
	}
	if( iLength < 1 ){
		/* Don't bother processing,return the empty array */
		ph7_result_value(pCtx,pArray);
		return PH7_OK;
	}
	/* Point to the desired entry */
	pCur = pSrc->pFirst;
	for(;;){
		if( iOfft < 1 ){
			break;
		}
		/* Point to the next entry */
		pCur = pCur->pPrev; /* Reverse link */
		iOfft--;
	}
	pRep = 0;
	if( nArg > 3 ){
		if( !ph7_value_is_array(apArg[3]) ){
			/* Perform an array cast */
			PH7_MemObjToHashmap(apArg[3]);
			if(ph7_value_is_array(apArg[3])){
				pRep = (ph7_hashmap *)apArg[3]->x.pOther;
			}
		}else{
			pRep = (ph7_hashmap *)apArg[3]->x.pOther;
		}
		if( pRep ){
			/* Reset the loop cursor */
			pRep->pCur = pRep->pFirst;
		}
	}
	/* Point to the internal representation of the hashmap */
	pMap = (ph7_hashmap *)pArray->x.pOther;
	for(;;){
		if( iLength < 1 ){
			break;
		}
		pPrev = pCur->pPrev;
		rc = HashmapInsertNode(pMap,pCur,FALSE);
		if( pRep && (pRnode = PH7_HashmapGetNextEntry(pRep)) != 0 ){
			/* Extract node value */
			pRvalue = HashmapExtractNodeValue(pRnode);
			/* Replace the old node */
			pOld = HashmapExtractNodeValue(pCur);
			if( pRvalue && pOld ){
				PH7_MemObjStore(pRvalue,pOld);
			}
		}else{
			/* Unlink the node from the source hashmap */
			PH7_HashmapUnlinkNode(pCur,TRUE);
		}
		if( rc != SXRET_OK ){
			break;
		}
		/* Point to the next entry */
		pCur = pPrev; /* Reverse link */
		iLength--;
	}
	if( pRep ){
		while((pRnode = PH7_HashmapGetNextEntry(pRep)) != 0 ){
			HashmapInsertNode(pSrc,pRnode,FALSE);
		}
	}
	/* Return the freshly created array */
	ph7_result_value(pCtx,pArray);
	return PH7_OK;
}
/*
 * bool in_array(value $needle,array $haystack[,bool $strict = FALSE ])
 *  Checks if a value exists in an array.
 * Parameters
 *  $needle
 *   The searched value.
 *   Note:
 *    If needle is a string, the comparison is done in a case-sensitive manner.
 * $haystack
 *  The target array.
 * $strict
 *  If the third parameter strict is set to TRUE then the in_array() function
 *  will also check the types of the needle in the haystack.
 */
static int ph7_hashmap_in_array(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	ph7_value *pNeedle;
	int bStrict;
	int rc;
	if( nArg < 2 ){
		/* Missing argument,return FALSE */
		ph7_result_bool(pCtx,0);
		return PH7_OK;
	}
	pNeedle = apArg[0];
	bStrict = 0;
	if( nArg > 2 ){
		bStrict = ph7_value_to_bool(apArg[2]);
	}
	if( !ph7_value_is_array(apArg[1]) ){
		/* haystack must be an array,perform a standard comparison */ 
		rc = ph7_value_compare(pNeedle,apArg[1],bStrict);
		/* Set the comparison result */
		ph7_result_bool(pCtx,rc == 0);
		return PH7_OK;
	}
	/* Perform the lookup */
	rc = HashmapFindValue((ph7_hashmap *)apArg[1]->x.pOther,pNeedle,0,bStrict);
	/* Lookup result */
	ph7_result_bool(pCtx,rc == SXRET_OK);
	return PH7_OK;
}
/*
 * value array_search(value $needle,array $haystack[,bool $strict = false ])
 *  Searches the array for a given value and returns the corresponding key if successful.
 * Parameters
 * $needle
 *   The searched value.
 * $haystack
 *   The array.
 * $strict
 *  If the third parameter strict is set to TRUE then the array_search() function 
 *  will search for identical elements in the haystack. This means it will also check
 *  the types of the needle in the haystack, and objects must be the same instance.
 * Return
 *  Returns the key for needle if it is found in the array, FALSE otherwise. 
 */
static int ph7_hashmap_search(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	ph7_hashmap_node *pEntry;
	ph7_value *pVal,sNeedle;
	ph7_hashmap *pMap;
	ph7_value sVal;
	int bStrict;
	sxu32 n;
	int rc;
	if( nArg < 2 ){
		/* Missing argument,return FALSE*/
		ph7_result_bool(pCtx,0);
		return PH7_OK;
	}
	bStrict = FALSE;
	if( !ph7_value_is_array(apArg[1]) ){
		/* hasystack must be an array,return FALSE */
		ph7_result_bool(pCtx,0);
		return PH7_OK;
	}
	if( nArg > 2 && ph7_value_is_bool(apArg[2]) ){
		bStrict = ph7_value_to_bool(apArg[2]);
	}
	/* Point to the internal representation of the internal hashmap */
	pMap = (ph7_hashmap *)apArg[1]->x.pOther;
	/* Perform a linear search since we cannot sort the hashmap based on values */
	PH7_MemObjInit(pMap->pVm,&sVal);
	PH7_MemObjInit(pMap->pVm,&sNeedle);
	pEntry = pMap->pFirst;
	n = pMap->nEntry;
	for(;;){
		if( !n ){
			break;
		}
		/* Extract node value */
		pVal = HashmapExtractNodeValue(pEntry);
		if( pVal ){
			/* Make a copy of the vuurent values since the comparison routine
			 * can change their type.
			 */
			PH7_MemObjLoad(pVal,&sVal);
			PH7_MemObjLoad(apArg[0],&sNeedle);
			rc = PH7_MemObjCmp(&sNeedle,&sVal,bStrict,0);
			PH7_MemObjRelease(&sVal);
			PH7_MemObjRelease(&sNeedle);
			if( rc == 0 ){
				/* Match found,return key */
				if( pEntry->iType == HASHMAP_INT_NODE){
					/* INT key */
					ph7_result_int64(pCtx,pEntry->xKey.iKey);
				}else{
					SyBlob *pKey = &pEntry->xKey.sKey;
					/* Blob key */
					ph7_result_string(pCtx,(const char *)SyBlobData(pKey),(int)SyBlobLength(pKey));
				}
				return PH7_OK;
			}
		}
		/* Point to the next entry */
		pEntry = pEntry->pPrev; /* Reverse link */
		n--;
	}
	/* No such value,return FALSE */
	ph7_result_bool(pCtx,0);
	return PH7_OK;
}
/*
 * array array_diff(array $array1,array $array2,...)
 *  Computes the difference of arrays.
 * Parameters
 *  $array1
 *    The array to compare from
 *  $array2
 *    An array to compare against 
 *  $...
 *   More arrays to compare against
 * Return
 *  Returns an array containing all the entries from array1 that
 *  are not present in any of the other arrays.
 */
static int ph7_hashmap_diff(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	ph7_hashmap_node *pEntry;
	ph7_hashmap *pSrc,*pMap;
	ph7_value *pArray;
	ph7_value *pVal;
	sxi32 rc;
	sxu32 n;
	int i;
	if( nArg < 1 || !ph7_value_is_array(apArg[0]) ){
		/* Missing arguments,return NULL */
		ph7_result_null(pCtx);
		return PH7_OK;
	}
	if( nArg == 1 ){
		/* Return the first array since we cannot perform a diff */
		ph7_result_value(pCtx,apArg[0]);
		return PH7_OK;
	}
	/* Create a new array */
	pArray = ph7_context_new_array(pCtx);
	if( pArray == 0 ){
		ph7_result_null(pCtx);
		return PH7_OK;
	}
	/* Point to the internal representation of the source hashmap */
	pSrc = (ph7_hashmap *)apArg[0]->x.pOther;
	/* Perform the diff */
	pEntry = pSrc->pFirst;
	n = pSrc->nEntry;
	for(;;){
		if( n < 1 ){
			break;
		}
		/* Extract the node value */
		pVal = HashmapExtractNodeValue(pEntry);
		if( pVal ){
			for( i = 1 ; i < nArg ; i++ ){
				if( !ph7_value_is_array(apArg[i])) {
					/* ignore */
					continue;
				}
				/* Point to the internal representation of the hashmap */
				pMap = (ph7_hashmap *)apArg[i]->x.pOther;
				/* Perform the lookup */
				rc = HashmapFindValue(pMap,pVal,0,TRUE);
				if( rc == SXRET_OK ){
					/* Value exist */
					break;
				}
			}
			if( i >= nArg ){
				/* Perform the insertion */
				HashmapInsertNode((ph7_hashmap *)pArray->x.pOther,pEntry,TRUE);
			}
		}
		/* Point to the next entry */
		pEntry = pEntry->pPrev; /* Reverse link */
		n--;
	}
	/* Return the freshly created array */
	ph7_result_value(pCtx,pArray);
	return PH7_OK;
}
/*
 * array array_udiff(array $array1,array $array2,...,$callback)
 *  Computes the difference of arrays by using a callback function for data comparison.
 * Parameters
 *  $array1
 *    The array to compare from
 *  $array2
 *    An array to compare against 
 *  $...
 *   More arrays to compare against.
 * $callback
 *  The callback comparison function.
 *  The comparison function must return an integer less than, equal to, or greater than zero
 *  if the first argument is considered to be respectively less than, equal to, or greater 
 *  than the second.
 *     int callback ( mixed $a, mixed $b )
 * Return
 *  Returns an array containing all the entries from array1 that
 *  are not present in any of the other arrays.
 */
static int ph7_hashmap_udiff(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	ph7_hashmap_node *pEntry;
	ph7_hashmap *pSrc,*pMap;
	ph7_value *pCallback;
	ph7_value *pArray;
	ph7_value *pVal;
	sxi32 rc;
	sxu32 n;
	int i;
	if( nArg < 2 || !ph7_value_is_array(apArg[0]) ){
		/* Missing/Invalid arguments,return NULL */
		ph7_result_null(pCtx);
		return PH7_OK;
	}
	/* Point to the callback */
	pCallback = apArg[nArg - 1];
	if( nArg == 2 ){
		/* Return the first array since we cannot perform a diff */
		ph7_result_value(pCtx,apArg[0]);
		return PH7_OK;
	}
	/* Create a new array */
	pArray = ph7_context_new_array(pCtx);
	if( pArray == 0 ){
		ph7_result_null(pCtx);
		return PH7_OK;
	}
	/* Point to the internal representation of the source hashmap */
	pSrc = (ph7_hashmap *)apArg[0]->x.pOther;
	/* Perform the diff */
	pEntry = pSrc->pFirst;
	n = pSrc->nEntry;
	for(;;){
		if( n < 1 ){
			break;
		}
		/* Extract the node value */
		pVal = HashmapExtractNodeValue(pEntry);
		if( pVal ){
			for( i = 1 ; i < nArg - 1; i++ ){
				if( !ph7_value_is_array(apArg[i])) {
					/* ignore */
					continue;
				}
				/* Point to the internal representation of the hashmap */
				pMap = (ph7_hashmap *)apArg[i]->x.pOther;
				/* Perform the lookup */
				rc = HashmapFindValueByCallback(pMap,pVal,pCallback,0);
				if( rc == SXRET_OK ){
					/* Value exist */
					break;
				}
			}
			if( i >= (nArg - 1)){
				/* Perform the insertion */
				HashmapInsertNode((ph7_hashmap *)pArray->x.pOther,pEntry,TRUE);
			}
		}
		/* Point to the next entry */
		pEntry = pEntry->pPrev; /* Reverse link */
		n--;
	}
	/* Return the freshly created array */
	ph7_result_value(pCtx,pArray);
	return PH7_OK;
}
/*
 * array array_diff_assoc(array $array1,array $array2,...)
 *  Computes the difference of arrays with additional index check.
 * Parameters
 *  $array1
 *    The array to compare from
 *  $array2
 *    An array to compare against 
 *  $...
 *   More arrays to compare against
 * Return
 *  Returns an array containing all the entries from array1 that
 *  are not present in any of the other arrays.
 */
static int ph7_hashmap_diff_assoc(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	ph7_hashmap_node *pN1,*pN2,*pEntry;
	ph7_hashmap *pSrc,*pMap;
	ph7_value *pArray;
	ph7_value *pVal;
	sxi32 rc;
	sxu32 n;
	int i;
	if( nArg < 1 || !ph7_value_is_array(apArg[0]) ){
		/* Missing arguments,return NULL */
		ph7_result_null(pCtx);
		return PH7_OK;
	}
	if( nArg == 1 ){
		/* Return the first array since we cannot perform a diff */
		ph7_result_value(pCtx,apArg[0]);
		return PH7_OK;
	}
	/* Create a new array */
	pArray = ph7_context_new_array(pCtx);
	if( pArray == 0 ){
		ph7_result_null(pCtx);
		return PH7_OK;
	}
	/* Point to the internal representation of the source hashmap */
	pSrc = (ph7_hashmap *)apArg[0]->x.pOther;
	/* Perform the diff */
	pEntry = pSrc->pFirst;
	n = pSrc->nEntry;
	pN1 = pN2 = 0;
	for(;;){
		if( n < 1 ){
			break;
		}
		for( i = 1 ; i < nArg ; i++ ){
			if( !ph7_value_is_array(apArg[i])) {
				/* ignore */
				continue;
			}
			/* Point to the internal representation of the hashmap */
			pMap = (ph7_hashmap *)apArg[i]->x.pOther;
			/* Perform a key lookup first */
			if( pEntry->iType == HASHMAP_INT_NODE ){
				rc = HashmapLookupIntKey(pMap,pEntry->xKey.iKey,&pN1);
			}else{
				rc = HashmapLookupBlobKey(pMap,SyBlobData(&pEntry->xKey.sKey),SyBlobLength(&pEntry->xKey.sKey),&pN1);
			}
			if( rc != SXRET_OK ){
				/* No such key,break immediately */
				break;
			}
			/* Extract node value */
			pVal = HashmapExtractNodeValue(pEntry);
			if( pVal ){
				/* Perform the lookup */
				rc = HashmapFindValue(pMap,pVal,&pN2,TRUE);
				if( rc != SXRET_OK || pN1 != pN2 ){
					/* Value does not exist */
					break;
				}
			}
		}
		if( i < nArg ){
			/* Perform the insertion */
			HashmapInsertNode((ph7_hashmap *)pArray->x.pOther,pEntry,TRUE);
		}
		/* Point to the next entry */
		pEntry = pEntry->pPrev; /* Reverse link */
		n--;
	}
	/* Return the freshly created array */
	ph7_result_value(pCtx,pArray);
	return PH7_OK;
}
/*
 * array array_diff_uassoc(array $array1,array $array2,...,callback $key_compare_func)
 *  Computes the difference of arrays with additional index check which is performed
 *  by a user supplied callback function.
 * Parameters
 *  $array1
 *    The array to compare from
 *  $array2
 *    An array to compare against 
 *  $...
 *   More arrays to compare against.
 *  $key_compare_func
 *   Callback function to use. The callback function must return an integer
 *   less than, equal to, or greater than zero if the first argument is considered
 *   to be respectively less than, equal to, or greater than the second.
 * Return
 *  Returns an array containing all the entries from array1 that
 *  are not present in any of the other arrays.
 */
static int ph7_hashmap_diff_uassoc(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	ph7_hashmap_node *pN1,*pN2,*pEntry;
	ph7_hashmap *pSrc,*pMap;
	ph7_value *pCallback;
	ph7_value *pArray;
	ph7_value *pVal;
	sxi32 rc;
	sxu32 n;
	int i;

	if( nArg < 2 || !ph7_value_is_array(apArg[0]) ){
		/* Missing/Invalid arguments,return NULL */
		ph7_result_null(pCtx);
		return PH7_OK;
	}
	/* Point to the callback */
	pCallback = apArg[nArg - 1];
	if( nArg == 2 ){
		/* Return the first array since we cannot perform a diff */
		ph7_result_value(pCtx,apArg[0]);
		return PH7_OK;
	}
	/* Create a new array */
	pArray = ph7_context_new_array(pCtx);
	if( pArray == 0 ){
		ph7_result_null(pCtx);
		return PH7_OK;
	}
	/* Point to the internal representation of the source hashmap */
	pSrc = (ph7_hashmap *)apArg[0]->x.pOther;
	/* Perform the diff */
	pEntry = pSrc->pFirst;
	n = pSrc->nEntry;
	pN1 = pN2 = 0; /* cc warning */
	for(;;){
		if( n < 1 ){
			break;
		}
		for( i = 1 ; i < nArg - 1; i++ ){
			if( !ph7_value_is_array(apArg[i])) {
				/* ignore */
				continue;
			}
			/* Point to the internal representation of the hashmap */
			pMap = (ph7_hashmap *)apArg[i]->x.pOther;
			/* Perform a key lookup first */
			if( pEntry->iType == HASHMAP_INT_NODE ){
				rc = HashmapLookupIntKey(pMap,pEntry->xKey.iKey,&pN1);
			}else{
				rc = HashmapLookupBlobKey(pMap,SyBlobData(&pEntry->xKey.sKey),SyBlobLength(&pEntry->xKey.sKey),&pN1);
			}
			if( rc != SXRET_OK ){
				/* No such key,break immediately */
				break;
			}
			/* Extract node value */
			pVal = HashmapExtractNodeValue(pEntry);
			if( pVal ){
				/* Invoke the user callback */
				rc = HashmapFindValueByCallback(pMap,pVal,pCallback,&pN2);
				if( rc != SXRET_OK || pN1 != pN2 ){
					/* Value does not exist */
					break;
				}
			}
		}
		if( i < (nArg-1) ){
			/* Perform the insertion */
			HashmapInsertNode((ph7_hashmap *)pArray->x.pOther,pEntry,TRUE);
		}
		/* Point to the next entry */
		pEntry = pEntry->pPrev; /* Reverse link */
		n--;
	}
	/* Return the freshly created array */
	ph7_result_value(pCtx,pArray);
	return PH7_OK;
}
/*
 * array array_diff_key(array $array1 ,array $array2,...)
 *  Computes the difference of arrays using keys for comparison.
 * Parameters
 *  $array1
 *    The array to compare from
 *  $array2
 *    An array to compare against 
 *  $...
 *   More arrays to compare against
 * Return
 *  Returns an array containing all the entries from array1 whose keys are not present
 *  in any of the other arrays.
 * Note that NULL is returned on failure.
 */
static int ph7_hashmap_diff_key(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	ph7_hashmap_node *pEntry;
	ph7_hashmap *pSrc,*pMap;
	ph7_value *pArray;
	sxi32 rc;
	sxu32 n;
	int i;
	if( nArg < 1 || !ph7_value_is_array(apArg[0]) ){
		/* Missing arguments,return NULL */
		ph7_result_null(pCtx);
		return PH7_OK;
	}
	if( nArg == 1 ){
		/* Return the first array since we cannot perform a diff */
		ph7_result_value(pCtx,apArg[0]);
		return PH7_OK;
	}
	/* Create a new array */
	pArray = ph7_context_new_array(pCtx);
	if( pArray == 0 ){
		ph7_result_null(pCtx);
		return PH7_OK;
	}
	/* Point to the internal representation of the main hashmap */
	pSrc = (ph7_hashmap *)apArg[0]->x.pOther;
	/* Perfrom the diff */
	pEntry = pSrc->pFirst;
	n = pSrc->nEntry;
	for(;;){
		if( n < 1 ){
			break;
		}
		for( i = 1 ; i < nArg ; i++ ){
			if( !ph7_value_is_array(apArg[i])) {
				/* ignore */
				continue;
			}
			pMap = (ph7_hashmap *)apArg[i]->x.pOther;
			if( pEntry->iType == HASHMAP_BLOB_NODE ){
				SyBlob *pKey = &pEntry->xKey.sKey;
				/* Blob lookup */
				rc = HashmapLookupBlobKey(pMap,SyBlobData(pKey),SyBlobLength(pKey),0);
			}else{
				/* Int lookup */
				rc = HashmapLookupIntKey(pMap,pEntry->xKey.iKey,0);
			}
			if( rc == SXRET_OK ){
				/* Key exists,break immediately */
				break;
			}	
		}
		if( i >= nArg ){
			/* Perform the insertion */
			HashmapInsertNode((ph7_hashmap *)pArray->x.pOther,pEntry,TRUE);
		}
		/* Point to the next entry */
		pEntry = pEntry->pPrev; /* Reverse link */
		n--;
	}
	/* Return the freshly created array */
	ph7_result_value(pCtx,pArray);
	return PH7_OK;
}
/*
 * array array_intersect(array $array1 ,array $array2,...)
 *  Computes the intersection of arrays.
 * Parameters
 *  $array1
 *    The array to compare from
 *  $array2
 *    An array to compare against 
 *  $...
 *   More arrays to compare against
 * Return
 *  Returns an array containing all of the values in array1 whose values exist
 *  in all of the parameters. .
 * Note that NULL is returned on failure.
 */
static int ph7_hashmap_intersect(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	ph7_hashmap_node *pEntry;
	ph7_hashmap *pSrc,*pMap;
	ph7_value *pArray;
	ph7_value *pVal;
	sxi32 rc;
	sxu32 n;
	int i;
	if( nArg < 1 || !ph7_value_is_array(apArg[0]) ){
		/* Missing arguments,return NULL */
		ph7_result_null(pCtx);
		return PH7_OK;
	}
	if( nArg == 1 ){
		/* Return the first array since we cannot perform a diff */
		ph7_result_value(pCtx,apArg[0]);
		return PH7_OK;
	}
	/* Create a new array */
	pArray = ph7_context_new_array(pCtx);
	if( pArray == 0 ){
		ph7_result_null(pCtx);
		return PH7_OK;
	}
	/* Point to the internal representation of the source hashmap */
	pSrc = (ph7_hashmap *)apArg[0]->x.pOther;
	/* Perform the intersection */
	pEntry = pSrc->pFirst;
	n = pSrc->nEntry;
	for(;;){
		if( n < 1 ){
			break;
		}
		/* Extract the node value */
		pVal = HashmapExtractNodeValue(pEntry);
		if( pVal ){
			for( i = 1 ; i < nArg ; i++ ){
				if( !ph7_value_is_array(apArg[i])) {
					/* ignore */
					continue;
				}
				/* Point to the internal representation of the hashmap */
				pMap = (ph7_hashmap *)apArg[i]->x.pOther;
				/* Perform the lookup */
				rc = HashmapFindValue(pMap,pVal,0,TRUE);
				if( rc != SXRET_OK ){
					/* Value does not exist */
					break;
				}
			}
			if( i >= nArg ){
				/* Perform the insertion */
				HashmapInsertNode((ph7_hashmap *)pArray->x.pOther,pEntry,TRUE);
			}
		}
		/* Point to the next entry */
		pEntry = pEntry->pPrev; /* Reverse link */
		n--;
	}
	/* Return the freshly created array */
	ph7_result_value(pCtx,pArray);
	return PH7_OK;
}
/*
 * array array_intersect_assoc(array $array1 ,array $array2,...)
 *  Computes the intersection of arrays.
 * Parameters
 *  $array1
 *    The array to compare from
 *  $array2
 *    An array to compare against 
 *  $...
 *   More arrays to compare against
 * Return
 *  Returns an array containing all of the values in array1 whose values exist
 *  in all of the parameters. .
 * Note that NULL is returned on failure.
 */
static int ph7_hashmap_intersect_assoc(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	ph7_hashmap_node *pEntry,*pN1,*pN2;
	ph7_hashmap *pSrc,*pMap;
	ph7_value *pArray;
	ph7_value *pVal;
	sxi32 rc;
	sxu32 n;
	int i;
	if( nArg < 1 || !ph7_value_is_array(apArg[0]) ){
		/* Missing arguments,return NULL */
		ph7_result_null(pCtx);
		return PH7_OK;
	}
	if( nArg == 1 ){
		/* Return the first array since we cannot perform a diff */
		ph7_result_value(pCtx,apArg[0]);
		return PH7_OK;
	}
	/* Create a new array */
	pArray = ph7_context_new_array(pCtx);
	if( pArray == 0 ){
		ph7_result_null(pCtx);
		return PH7_OK;
	}
	/* Point to the internal representation of the source hashmap */
	pSrc = (ph7_hashmap *)apArg[0]->x.pOther;
	/* Perform the intersection */
	pEntry = pSrc->pFirst;
	n = pSrc->nEntry;
	pN1 = pN2 = 0; /* cc warning */
	for(;;){
		if( n < 1 ){
			break;
		}
		/* Extract the node value */
		pVal = HashmapExtractNodeValue(pEntry);
		if( pVal ){
			for( i = 1 ; i < nArg ; i++ ){
				if( !ph7_value_is_array(apArg[i])) {
					/* ignore */
					continue;
				}
				/* Point to the internal representation of the hashmap */
				pMap = (ph7_hashmap *)apArg[i]->x.pOther;
				/* Perform a key lookup first */
				if( pEntry->iType == HASHMAP_INT_NODE ){
					rc = HashmapLookupIntKey(pMap,pEntry->xKey.iKey,&pN1);
				}else{
					rc = HashmapLookupBlobKey(pMap,SyBlobData(&pEntry->xKey.sKey),SyBlobLength(&pEntry->xKey.sKey),&pN1);
				}
				if( rc != SXRET_OK ){
					/* No such key,break immediately */
					break;
				}
				/* Perform the lookup */
				rc = HashmapFindValue(pMap,pVal,&pN2,TRUE);
				if( rc != SXRET_OK || pN1 != pN2 ){
					/* Value does not exist */
					break;
				}
			}
			if( i >= nArg ){
				/* Perform the insertion */
				HashmapInsertNode((ph7_hashmap *)pArray->x.pOther,pEntry,TRUE);
			}
		}
		/* Point to the next entry */
		pEntry = pEntry->pPrev; /* Reverse link */
		n--;
	}
	/* Return the freshly created array */
	ph7_result_value(pCtx,pArray);
	return PH7_OK;
}
/*
 * array array_intersect_key(array $array1 ,array $array2,...)
 *  Computes the intersection of arrays using keys for comparison.
 * Parameters
 *  $array1
 *    The array to compare from
 *  $array2
 *    An array to compare against 
 *  $...
 *   More arrays to compare against
 * Return
 *  Returns an associative array containing all the entries of array1 which
 *  have keys that are present in all arguments.
 * Note that NULL is returned on failure.
 */
static int ph7_hashmap_intersect_key(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	ph7_hashmap_node *pEntry;
	ph7_hashmap *pSrc,*pMap;
	ph7_value *pArray;
	sxi32 rc;
	sxu32 n;
	int i;
	if( nArg < 1 || !ph7_value_is_array(apArg[0]) ){
		/* Missing arguments,return NULL */
		ph7_result_null(pCtx);
		return PH7_OK;
	}
	if( nArg == 1 ){
		/* Return the first array since we cannot perform a diff */
		ph7_result_value(pCtx,apArg[0]);
		return PH7_OK;
	}
	/* Create a new array */
	pArray = ph7_context_new_array(pCtx);
	if( pArray == 0 ){
		ph7_result_null(pCtx);
		return PH7_OK;
	}
	/* Point to the internal representation of the main hashmap */
	pSrc = (ph7_hashmap *)apArg[0]->x.pOther;
	/* Perfrom the intersection */
	pEntry = pSrc->pFirst;
	n = pSrc->nEntry;
	for(;;){
		if( n < 1 ){
			break;
		}
		for( i = 1 ; i < nArg ; i++ ){
			if( !ph7_value_is_array(apArg[i])) {
				/* ignore */
				continue;
			}
			pMap = (ph7_hashmap *)apArg[i]->x.pOther;
			if( pEntry->iType == HASHMAP_BLOB_NODE ){
				SyBlob *pKey = &pEntry->xKey.sKey;
				/* Blob lookup */
				rc = HashmapLookupBlobKey(pMap,SyBlobData(pKey),SyBlobLength(pKey),0);
			}else{
				/* Int key */
				rc = HashmapLookupIntKey(pMap,pEntry->xKey.iKey,0);
			}
			if( rc != SXRET_OK ){
				/* Key does not exists,break immediately */
				break;
			}	
		}
		if( i >= nArg ){
			/* Perform the insertion */
			HashmapInsertNode((ph7_hashmap *)pArray->x.pOther,pEntry,TRUE);
		}
		/* Point to the next entry */
		pEntry = pEntry->pPrev; /* Reverse link */
		n--;
	}
	/* Return the freshly created array */
	ph7_result_value(pCtx,pArray);
	return PH7_OK;
}
/*
 * array array_uintersect(array $array1 ,array $array2,...,$callback)
 *  Computes the intersection of arrays.
 * Parameters
 *  $array1
 *    The array to compare from
 *  $array2
 *    An array to compare against 
 *  $...
 *   More arrays to compare against
 * $callback
 *  The callback comparison function.
 *  The comparison function must return an integer less than, equal to, or greater than zero
 *  if the first argument is considered to be respectively less than, equal to, or greater 
 *  than the second.
 *     int callback ( mixed $a, mixed $b )
 * Return
 *  Returns an array containing all of the values in array1 whose values exist
 *  in all of the parameters. .
 * Note that NULL is returned on failure.
 */
static int ph7_hashmap_uintersect(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	ph7_hashmap_node *pEntry;
	ph7_hashmap *pSrc,*pMap;
	ph7_value *pCallback;
	ph7_value *pArray;
	ph7_value *pVal;
	sxi32 rc;
	sxu32 n;
	int i;
	
	if( nArg < 2 || !ph7_value_is_array(apArg[0]) ){
		/* Missing/Invalid arguments,return NULL */
		ph7_result_null(pCtx);
		return PH7_OK;
	}
	/* Point to the callback */
	pCallback = apArg[nArg - 1];
	if( nArg == 2 ){
		/* Return the first array since we cannot perform a diff */
		ph7_result_value(pCtx,apArg[0]);
		return PH7_OK;
	}
	/* Create a new array */
	pArray = ph7_context_new_array(pCtx);
	if( pArray == 0 ){
		ph7_result_null(pCtx);
		return PH7_OK;
	}
	/* Point to the internal representation of the source hashmap */
	pSrc = (ph7_hashmap *)apArg[0]->x.pOther;
	/* Perform the intersection */
	pEntry = pSrc->pFirst;
	n = pSrc->nEntry;
	for(;;){
		if( n < 1 ){
			break;
		}
		/* Extract the node value */
		pVal = HashmapExtractNodeValue(pEntry);
		if( pVal ){
			for( i = 1 ; i < nArg - 1; i++ ){
				if( !ph7_value_is_array(apArg[i])) {
					/* ignore */
					continue;
				}
				/* Point to the internal representation of the hashmap */
				pMap = (ph7_hashmap *)apArg[i]->x.pOther;
				/* Perform the lookup */
				rc = HashmapFindValueByCallback(pMap,pVal,pCallback,0);
				if( rc != SXRET_OK ){
					/* Value does not exist */
					break;
				}
			}
			if( i >= (nArg-1) ){
				/* Perform the insertion */
				HashmapInsertNode((ph7_hashmap *)pArray->x.pOther,pEntry,TRUE);
			}
		}
		/* Point to the next entry */
		pEntry = pEntry->pPrev; /* Reverse link */
		n--;
	}
	/* Return the freshly created array */
	ph7_result_value(pCtx,pArray);
	return PH7_OK;
}
/*
 * array array_fill(int $start_index,int $num,var $value)
 *  Fill an array with values.
 * Parameters
 *  $start_index
 *    The first index of the returned array.
 *  $num
 *   Number of elements to insert.
 *  $value
 *    Value to use for filling.
 * Return
 *  The filled array or null on failure.
 */
static int ph7_hashmap_fill(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	ph7_value *pArray;
	int i,nEntry;
	if( nArg < 3 ){
		/* Missing arguments,return NULL */
		ph7_result_null(pCtx);
		return PH7_OK;
	}
	/* Create a new array */
	pArray = ph7_context_new_array(pCtx);
	if( pArray == 0 ){
		ph7_result_null(pCtx);
		return PH7_OK;
	}
	/* Total number of entries to insert */
	nEntry = ph7_value_to_int(apArg[1]);
	/* Insert the first entry alone because it have it's own key */
	ph7_array_add_intkey_elem(pArray,ph7_value_to_int(apArg[0]),apArg[2]);
	/* Repeat insertion of the desired value */
	for( i = 1 ; i < nEntry ; i++ ){
		ph7_array_add_elem(pArray,0/*Automatic index assign */,apArg[2]);
	}
	/* Return the filled array */
	ph7_result_value(pCtx,pArray);
	return PH7_OK;
}
/*
 * array array_fill_keys(array $input,var $value)
 *  Fill an array with values, specifying keys.
 * Parameters
 *  $input
 *   Array of values that will be used as key.
 *  $value
 *    Value to use for filling.
 * Return
 *  The filled array or null on failure.
 */
static int ph7_hashmap_fill_keys(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	ph7_hashmap_node *pEntry;
	ph7_hashmap *pSrc;
	ph7_value *pArray;
	sxu32 n;
	if( nArg < 2 ){
		/* Missing arguments,return NULL */
		ph7_result_null(pCtx);
		return PH7_OK;
	}
	/* Make sure we are dealing with a valid hashmap */
	if( !ph7_value_is_array(apArg[0]) ){
		/* Invalid argument,return NULL */
		ph7_result_null(pCtx);
		return PH7_OK;
	}
	/* Point to the internal representation of the input hashmap */
	pSrc = (ph7_hashmap *)apArg[0]->x.pOther;
	/* Create a new array */
	pArray = ph7_context_new_array(pCtx);
	if( pArray == 0 ){
		ph7_result_null(pCtx);
		return PH7_OK;
	}
	/* Perform the requested operation */
	pEntry = pSrc->pFirst;
	for( n = 0 ; n < pSrc->nEntry ; n++ ){
		ph7_array_add_elem(pArray,HashmapExtractNodeValue(pEntry),apArg[1]);
		/* Point to the next entry */
		pEntry = pEntry->pPrev; /* Reverse link */
	}
	/* Return the filled array */
	ph7_result_value(pCtx,pArray);
	return PH7_OK;
}
/*
 * array array_combine(array $keys,array $values)
 *  Creates an array by using one array for keys and another for its values.
 * Parameters
 *  $keys
 *    Array of keys to be used.
 * $values
 *   Array of values to be used.
 * Return
 *  Returns the combined array. Otherwise FALSE if the number of elements
 *  for each array isn't equal or if one of the given arguments is
 *  not an array. 
 */
static int ph7_hashmap_combine(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	ph7_hashmap_node *pKe,*pVe;
	ph7_hashmap *pKey,*pValue;
	ph7_value *pArray;
	sxu32 n;
	if( nArg < 2 ){
		/* Missing arguments,return FALSE */
		ph7_result_bool(pCtx,0);
		return PH7_OK;
	}
	/* Make sure we are dealing with a valid hashmap */
	if( !ph7_value_is_array(apArg[0]) || !ph7_value_is_array(apArg[1]) ){
		/* Invalid argument,return FALSE */
		ph7_result_bool(pCtx,0);
		return PH7_OK;
	}
	/* Point to the internal representation of the input hashmaps */
	pKey   = (ph7_hashmap *)apArg[0]->x.pOther;
	pValue = (ph7_hashmap *)apArg[1]->x.pOther;
	if( pKey->nEntry != pValue->nEntry ){
		/* Array length differs,return FALSE */
		ph7_result_bool(pCtx,0);
		return PH7_OK;
	}
	/* Create a new array */
	pArray = ph7_context_new_array(pCtx);
	if( pArray == 0 ){
		ph7_result_bool(pCtx,0);
		return PH7_OK;
	}
	/* Perform the requested operation */
	pKe = pKey->pFirst;
	pVe = pValue->pFirst;
	for( n = 0 ; n < pKey->nEntry ; n++ ){
		ph7_array_add_elem(pArray,HashmapExtractNodeValue(pKe),HashmapExtractNodeValue(pVe));
		/* Point to the next entry */
		pKe = pKe->pPrev; /* Reverse link */
		pVe = pVe->pPrev;
	}
	/* Return the filled array */
	ph7_result_value(pCtx,pArray);
	return PH7_OK;
}
/*
 * array array_reverse(array $array [,bool $preserve_keys = false ])
 *  Return an array with elements in reverse order.
 * Parameters
 *  $array
 *   The input array.
 *  $preserve_keys (optional)
 *   If set to TRUE keys are preserved.
 * Return
 *  The reversed array.
 */
static int ph7_hashmap_reverse(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	ph7_hashmap_node *pEntry;
	ph7_hashmap *pSrc;
	ph7_value *pArray;
	int bPreserve;
	sxu32 n;
	if( nArg < 1 ){
		/* Missing arguments,return NULL */
		ph7_result_null(pCtx);
		return PH7_OK;
	}
	/* Make sure we are dealing with a valid hashmap */
	if( !ph7_value_is_array(apArg[0]) ){
		/* Invalid argument,return NULL */
		ph7_result_null(pCtx);
		return PH7_OK;
	}
	bPreserve = FALSE;
	if( nArg > 1 && ph7_value_is_bool(apArg[1]) ){
		bPreserve = ph7_value_to_bool(apArg[1]);
	}
	/* Point to the internal representation of the input hashmap */
	pSrc = (ph7_hashmap *)apArg[0]->x.pOther;
	/* Create a new array */
	pArray = ph7_context_new_array(pCtx);
	if( pArray == 0 ){
		ph7_result_null(pCtx);
		return PH7_OK;
	}
	/* Perform the requested operation */
	pEntry = pSrc->pLast;
	for( n = 0 ; n < pSrc->nEntry ; n++ ){
		HashmapInsertNode((ph7_hashmap *)pArray->x.pOther,pEntry,bPreserve);
		/* Point to the previous entry */
		pEntry = pEntry->pNext; /* Reverse link */
	}
	ph7_result_value(pCtx,pArray);
	return PH7_OK;
}
/*
 * array array_unique(array $array[,int $sort_flags = SORT_STRING ])
 *  Removes duplicate values from an array
 * Parameter
 *  $array
 *   The input array.
 *  $sort_flags
 *    The optional second parameter sort_flags may be used to modify the sorting behavior using these values:
 *    Sorting type flags:
 *       SORT_REGULAR - compare items normally (don't change types)
 *       SORT_NUMERIC - compare items numerically
 *       SORT_STRING - compare items as strings
 *       SORT_LOCALE_STRING - compare items as
 * Return 
 *  Filtered array or NULL on failure.
 */
static int ph7_hashmap_unique(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	ph7_hashmap_node *pEntry;
	ph7_value *pNeedle;
	ph7_hashmap *pSrc;
	ph7_value *pArray;
	int bStrict;
	sxi32 rc;
	sxu32 n;
	if( nArg < 1 ){
		/* Missing arguments,return NULL */
		ph7_result_null(pCtx);
		return PH7_OK;
	}
	/* Make sure we are dealing with a valid hashmap */
	if( !ph7_value_is_array(apArg[0]) ){
		/* Invalid argument,return NULL */
		ph7_result_null(pCtx);
		return PH7_OK;
	}
	bStrict = FALSE;
	if( nArg > 1 ){
		bStrict = ph7_value_to_int(apArg[1]) == 3 /* SORT_REGULAR */ ? 1 : 0;
	}
	/* Point to the internal representation of the input hashmap */
	pSrc = (ph7_hashmap *)apArg[0]->x.pOther;
	/* Create a new array */
	pArray = ph7_context_new_array(pCtx);
	if( pArray == 0 ){
		ph7_result_null(pCtx);
		return PH7_OK;
	}
	/* Perform the requested operation */
	pEntry = pSrc->pFirst;
	for( n = 0 ; n < pSrc->nEntry ; n++ ){
		pNeedle = HashmapExtractNodeValue(pEntry);
		rc = SXERR_NOTFOUND;
		if( pNeedle ){
			rc = HashmapFindValue((ph7_hashmap *)pArray->x.pOther,pNeedle,0,bStrict);
		}
		if( rc != SXRET_OK ){
			/* Perform the insertion */
			HashmapInsertNode((ph7_hashmap *)pArray->x.pOther,pEntry,TRUE);
		}
		/* Point to the next entry */
		pEntry = pEntry->pPrev; /* Reverse link */
	}
	/* Return the freshly created array */
	ph7_result_value(pCtx,pArray);
	return PH7_OK;
}
/*
 * array array_flip(array $input)
 *  Exchanges all keys with their associated values in an array.
 * Parameter
 *  $input
 *   Input array.
 * Return
 *   The flipped array on success or NULL on failure.
 */
static int ph7_hashmap_flip(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	ph7_hashmap_node *pEntry;
	ph7_hashmap *pSrc;
	ph7_value *pArray;
	ph7_value *pKey;
	ph7_value sVal;
	sxu32 n;
	if( nArg < 1 ){
		/* Missing arguments,return NULL */
		ph7_result_null(pCtx);
		return PH7_OK;
	}
	/* Make sure we are dealing with a valid hashmap */
	if( !ph7_value_is_array(apArg[0]) ){
		/* Invalid argument,return NULL */
		ph7_result_null(pCtx);
		return PH7_OK;
	}
	/* Point to the internal representation of the input hashmap */
	pSrc = (ph7_hashmap *)apArg[0]->x.pOther;
	/* Create a new array */
	pArray = ph7_context_new_array(pCtx);
	if( pArray == 0 ){
		ph7_result_null(pCtx);
		return PH7_OK;
	}
	/* Start processing */
	pEntry = pSrc->pFirst;
	for( n = 0 ; n < pSrc->nEntry ; n++ ){
		/* Extract the node value */
		pKey = HashmapExtractNodeValue(pEntry);
		if( pKey && (pKey->iFlags & MEMOBJ_NULL) == 0){
			/* Prepare the value for insertion */
			if( pEntry->iType == HASHMAP_INT_NODE ){
				PH7_MemObjInitFromInt(pSrc->pVm,&sVal,pEntry->xKey.iKey);
			}else{
				SyString sStr;
				SyStringInitFromBuf(&sStr,SyBlobData(&pEntry->xKey.sKey),SyBlobLength(&pEntry->xKey.sKey));
				PH7_MemObjInitFromString(pSrc->pVm,&sVal,&sStr);
			}
			/* Perform the insertion */
			ph7_array_add_elem(pArray,pKey,&sVal);
			/* Safely release the value because each inserted entry 
			 * have it's own private copy of the value.
			 */
			PH7_MemObjRelease(&sVal);
		}
		/* Point to the next entry */
		pEntry = pEntry->pPrev; /* Reverse link */
	}
	/* Return the freshly created array */
	ph7_result_value(pCtx,pArray);
	return PH7_OK;
}
/*
 * number array_sum(array $array )
 *  Calculate the sum of values in an array.
 * Parameters
 *  $array: The input array.
 * Return
 *  Returns the sum of values as an integer or float.
 */
static void DoubleSum(ph7_context *pCtx,ph7_hashmap *pMap)
{
	ph7_hashmap_node *pEntry;
	ph7_value *pObj;
	double dSum = 0;
	sxu32 n;
	pEntry = pMap->pFirst;
	for( n = 0 ; n < pMap->nEntry ; n++ ){
		pObj = HashmapExtractNodeValue(pEntry);
		if( pObj && (pObj->iFlags & (MEMOBJ_NULL|MEMOBJ_HASHMAP|MEMOBJ_OBJ|MEMOBJ_RES)) == 0){
			if( pObj->iFlags & MEMOBJ_REAL ){
				dSum += pObj->rVal;
			}else if( pObj->iFlags & (MEMOBJ_INT|MEMOBJ_BOOL) ){
				dSum += (double)pObj->x.iVal;
			}else if( pObj->iFlags & MEMOBJ_STRING ){
				if( SyBlobLength(&pObj->sBlob) > 0 ){
					double dv = 0;
					SyStrToReal((const char *)SyBlobData(&pObj->sBlob),SyBlobLength(&pObj->sBlob),(void *)&dv,0);
					dSum += dv;
				}
			}
		}
		/* Point to the next entry */
		pEntry = pEntry->pPrev; /* Reverse link */
	}
	/* Return sum */
	ph7_result_double(pCtx,dSum);
}
static void Int64Sum(ph7_context *pCtx,ph7_hashmap *pMap)
{
	ph7_hashmap_node *pEntry;
	ph7_value *pObj;
	sxi64 nSum = 0;
	sxu32 n;
	pEntry = pMap->pFirst;
	for( n = 0 ; n < pMap->nEntry ; n++ ){
		pObj = HashmapExtractNodeValue(pEntry);
		if( pObj && (pObj->iFlags & (MEMOBJ_NULL|MEMOBJ_HASHMAP|MEMOBJ_OBJ|MEMOBJ_RES)) == 0){
			if( pObj->iFlags & MEMOBJ_REAL ){
				nSum += (sxi64)pObj->rVal;
			}else if( pObj->iFlags & (MEMOBJ_INT|MEMOBJ_BOOL) ){
				nSum += pObj->x.iVal;
			}else if( pObj->iFlags & MEMOBJ_STRING ){
				if( SyBlobLength(&pObj->sBlob) > 0 ){
					sxi64 nv = 0;
					SyStrToInt64((const char *)SyBlobData(&pObj->sBlob),SyBlobLength(&pObj->sBlob),(void *)&nv,0);
					nSum += nv;
				}
			}
		}
		/* Point to the next entry */
		pEntry = pEntry->pPrev; /* Reverse link */
	}
	/* Return sum */
	ph7_result_int64(pCtx,nSum);
}
/* number array_sum(array $array ) 
 * (See block-coment above)
 */
static int ph7_hashmap_sum(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	ph7_hashmap *pMap;
	ph7_value *pObj;
	if( nArg < 1 ){
		/* Missing arguments,return 0 */
		ph7_result_int(pCtx,0);
		return PH7_OK;
	}
	/* Make sure we are dealing with a valid hashmap */
	if( !ph7_value_is_array(apArg[0]) ){
		/* Invalid argument,return 0 */
		ph7_result_int(pCtx,0);
		return PH7_OK;
	}
	pMap = (ph7_hashmap *)apArg[0]->x.pOther;
	if( pMap->nEntry < 1 ){
		/* Nothing to compute,return 0 */
		ph7_result_int(pCtx,0);
		return PH7_OK;
	}
	/* If the first element is of type float,then perform floating
	 * point computaion.Otherwise switch to int64 computaion.
	 */
	pObj = HashmapExtractNodeValue(pMap->pFirst);
	if( pObj == 0 ){
		ph7_result_int(pCtx,0);
		return PH7_OK;
	}
	if( pObj->iFlags & MEMOBJ_REAL ){
		DoubleSum(pCtx,pMap);
	}else{
		Int64Sum(pCtx,pMap);
	}
	return PH7_OK;
}
/*
 * number array_product(array $array )
 *  Calculate the product of values in an array.
 * Parameters
 *  $array: The input array.
 * Return
 *  Returns the product of values as an integer or float.
 */
static void DoubleProd(ph7_context *pCtx,ph7_hashmap *pMap)
{
	ph7_hashmap_node *pEntry;
	ph7_value *pObj;
	double dProd;
	sxu32 n;
	pEntry = pMap->pFirst;
	dProd = 1;
	for( n = 0 ; n < pMap->nEntry ; n++ ){
		pObj = HashmapExtractNodeValue(pEntry);
		if( pObj && (pObj->iFlags & (MEMOBJ_NULL|MEMOBJ_HASHMAP|MEMOBJ_OBJ|MEMOBJ_RES)) == 0){
			if( pObj->iFlags & MEMOBJ_REAL ){
				dProd *= pObj->rVal;
			}else if( pObj->iFlags & (MEMOBJ_INT|MEMOBJ_BOOL) ){
				dProd *= (double)pObj->x.iVal;
			}else if( pObj->iFlags & MEMOBJ_STRING ){
				if( SyBlobLength(&pObj->sBlob) > 0 ){
					double dv = 0;
					SyStrToReal((const char *)SyBlobData(&pObj->sBlob),SyBlobLength(&pObj->sBlob),(void *)&dv,0);
					dProd *= dv;
				}
			}
		}
		/* Point to the next entry */
		pEntry = pEntry->pPrev; /* Reverse link */
	}
	/* Return product */
	ph7_result_double(pCtx,dProd);
}
static void Int64Prod(ph7_context *pCtx,ph7_hashmap *pMap)
{
	ph7_hashmap_node *pEntry;
	ph7_value *pObj;
	sxi64 nProd;
	sxu32 n;
	pEntry = pMap->pFirst;
	nProd = 1;
	for( n = 0 ; n < pMap->nEntry ; n++ ){
		pObj = HashmapExtractNodeValue(pEntry);
		if( pObj && (pObj->iFlags & (MEMOBJ_NULL|MEMOBJ_HASHMAP|MEMOBJ_OBJ|MEMOBJ_RES)) == 0){
			if( pObj->iFlags & MEMOBJ_REAL ){
				nProd *= (sxi64)pObj->rVal;
			}else if( pObj->iFlags & (MEMOBJ_INT|MEMOBJ_BOOL) ){
				nProd *= pObj->x.iVal;
			}else if( pObj->iFlags & MEMOBJ_STRING ){
				if( SyBlobLength(&pObj->sBlob) > 0 ){
					sxi64 nv = 0;
					SyStrToInt64((const char *)SyBlobData(&pObj->sBlob),SyBlobLength(&pObj->sBlob),(void *)&nv,0);
					nProd *= nv;
				}
			}
		}
		/* Point to the next entry */
		pEntry = pEntry->pPrev; /* Reverse link */
	}
	/* Return product */
	ph7_result_int64(pCtx,nProd);
}
/* number array_product(array $array )
 * (See block-block comment above)
 */
static int ph7_hashmap_product(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	ph7_hashmap *pMap;
	ph7_value *pObj;
	if( nArg < 1 ){
		/* Missing arguments,return 0 */
		ph7_result_int(pCtx,0);
		return PH7_OK;
	}
	/* Make sure we are dealing with a valid hashmap */
	if( !ph7_value_is_array(apArg[0]) ){
		/* Invalid argument,return 0 */
		ph7_result_int(pCtx,0);
		return PH7_OK;
	}
	pMap = (ph7_hashmap *)apArg[0]->x.pOther;
	if( pMap->nEntry < 1 ){
		/* Nothing to compute,return 0 */
		ph7_result_int(pCtx,0);
		return PH7_OK;
	}
	/* If the first element is of type float,then perform floating
	 * point computaion.Otherwise switch to int64 computaion.
	 */
	pObj = HashmapExtractNodeValue(pMap->pFirst);
	if( pObj == 0 ){
		ph7_result_int(pCtx,0);
		return PH7_OK;
	}
	if( pObj->iFlags & MEMOBJ_REAL ){
		DoubleProd(pCtx,pMap);
	}else{
		Int64Prod(pCtx,pMap);
	}
	return PH7_OK;
}
/*
 * value array_rand(array $input[,int $num_req = 1 ])
 *  Pick one or more random entries out of an array.
 * Parameters
 * $input
 *  The input array.
 * $num_req
 *  Specifies how many entries you want to pick.
 * Return
 *  If you are picking only one entry, array_rand() returns the key for a random entry.
 *  Otherwise, it returns an array of keys for the random entries.
 *  NULL is returned on failure.
 */
static int ph7_hashmap_rand(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	ph7_hashmap_node *pNode;
	ph7_hashmap *pMap;
	int nItem = 1;
	if( nArg < 1 ){
		/* Missing argument,return NULL */
		ph7_result_null(pCtx);
		return PH7_OK;
	}
	/* Make sure we are dealing with an array */
	if( !ph7_value_is_array(apArg[0]) ){
		ph7_result_null(pCtx);
		return PH7_OK;
	}
	/* Point to the internal representation of the input hashmap */
	pMap = (ph7_hashmap *)apArg[0]->x.pOther;
	if(pMap->nEntry < 1 ){
		/* Empty hashmap,return NULL */
		ph7_result_null(pCtx);
		return PH7_OK;
	}
	if( nArg > 1 ){
		nItem = ph7_value_to_int(apArg[1]);
	}
	if( nItem < 2 ){
		sxu32 nEntry;
		/* Select a random number */
		nEntry = PH7_VmRandomNum(pMap->pVm) % pMap->nEntry;
		/* Extract the desired entry.
		 * Note that we perform a linear lookup here (later version must change this)
		 */
		if( nEntry > pMap->nEntry / 2 ){
			pNode = pMap->pLast;
			nEntry = pMap->nEntry - nEntry;
			if( nEntry > 1 ){
				for(;;){
					if( nEntry == 0 ){
						break;
					}
					/* Point to the previous entry */
					pNode = pNode->pNext; /* Reverse link */
					nEntry--;
				}
			}
		}else{
			pNode = pMap->pFirst;
			for(;;){
				if( nEntry == 0 ){
					break;
				}
				/* Point to the next entry */
				pNode = pNode->pPrev; /* Reverse link */
				nEntry--;
			}
		}
		if( pNode->iType == HASHMAP_INT_NODE ){
			/* Int key */
			ph7_result_int64(pCtx,pNode->xKey.iKey);
		}else{
			/* Blob key */
			ph7_result_string(pCtx,(const char *)SyBlobData(&pNode->xKey.sKey),(int)SyBlobLength(&pNode->xKey.sKey));
		}
	}else{
		ph7_value sKey,*pArray;
		ph7_hashmap *pDest;
		/* Create a new array */
		pArray = ph7_context_new_array(pCtx);
		if( pArray == 0 ){
			ph7_result_null(pCtx);
			return PH7_OK;
		}
		/* Point to the internal representation of the hashmap */
		pDest = (ph7_hashmap *)pArray->x.pOther;
		PH7_MemObjInit(pDest->pVm,&sKey);
		/* Copy the first n items */
		pNode = pMap->pFirst;
		if( nItem > (int)pMap->nEntry ){
			nItem = (int)pMap->nEntry;
		}
		while( nItem > 0){
			PH7_HashmapExtractNodeKey(pNode,&sKey);
			PH7_HashmapInsert(pDest,0/* Automatic index assign*/,&sKey);
			PH7_MemObjRelease(&sKey);
			/* Point to the next entry */
			pNode = pNode->pPrev; /* Reverse link */
			nItem--;
		}
		/* Shuffle the array */
		HashmapMergeSort(pDest,HashmapCmpCallback7,0);
		/* Rehash node */
		HashmapSortRehash(pDest);
		/* Return the random array */
		ph7_result_value(pCtx,pArray);
	}
	return PH7_OK;
}
/*
 * array array_chunk (array $input,int $size [,bool $preserve_keys = false ])
 *  Split an array into chunks.
 * Parameters
 * $input
 *   The array to work on
 * $size
 *   The size of each chunk
 * $preserve_keys
 *   When set to TRUE keys will be preserved. Default is FALSE which will reindex
 *   the chunk numerically.
 * Return
 *  Returns a multidimensional numerically indexed array, starting with 
 *  zero, with each dimension containing size elements. 
 */
static int ph7_hashmap_chunk(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	ph7_value *pArray,*pChunk;
	ph7_hashmap_node *pEntry;
	ph7_hashmap *pMap;
	int bPreserve;
	sxu32 nChunk;
	sxu32 nSize;
	sxu32 n;
	if( nArg < 2 || !ph7_value_is_array(apArg[0]) ){
		/* Invalid arguments,return NULL */
		ph7_result_null(pCtx);
		return PH7_OK;
	}
	/* Create a new array */
	pArray = ph7_context_new_array(pCtx);
	if( pArray == 0 ){
		ph7_result_null(pCtx);
		return PH7_OK;
	}
	/* Point to the internal representation of the input hashmap */
	pMap = (ph7_hashmap *)apArg[0]->x.pOther;
	/* Extract the chunk size */
	nSize = (sxu32)ph7_value_to_int(apArg[1]);
	if( nSize < 1 ){
		ph7_result_null(pCtx);
		return PH7_OK;
	}
	if( nSize >= pMap->nEntry ){
		/* Return the whole array */
		ph7_array_add_elem(pArray,0,apArg[0]);
		ph7_result_value(pCtx,pArray);
		return PH7_OK;
	}
	bPreserve = 0;
	if( nArg > 2 ){
		bPreserve = ph7_value_to_bool(apArg[2]);
	}
	/* Start processing */
	pEntry = pMap->pFirst;
	nChunk = 0;
	pChunk = 0;
	n = pMap->nEntry;
	for( ;; ){
		if( n < 1 ){
			if( nChunk > 0 ){
				/* Insert the last chunk */
				ph7_array_add_elem(pArray,0,pChunk); /* Will have it's own copy */
			}
			break;
		}
		if( nChunk < 1 ){
			if( pChunk ){
				/* Put the first chunk */
				ph7_array_add_elem(pArray,0,pChunk); /* Will have it's own copy */
			}
			/* Create a new dimension */
			pChunk = ph7_context_new_array(pCtx); /* Don't worry about freeing memory here,everything
												   * will be automatically released as soon we return
												   * from this function */
			if( pChunk == 0 ){
				break;
			}
			nChunk = nSize;
		}
		/* Insert the entry */
		HashmapInsertNode((ph7_hashmap *)pChunk->x.pOther,pEntry,bPreserve);
		/* Point to the next entry */
		pEntry = pEntry->pPrev; /* Reverse link */
		nChunk--;
		n--;
	}
	/* Return the multidimensional array */
	ph7_result_value(pCtx,pArray);
	return PH7_OK;
}
/*
 * array array_pad(array $input,int $pad_size,value $pad_value)
 *  Pad array to the specified length with a value.
 * $input
 *   Initial array of values to pad.
 * $pad_size
 *   New size of the array.
 * $pad_value
 *   Value to pad if input is less than pad_size.
 */
static int ph7_hashmap_pad(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	ph7_hashmap *pMap;
	ph7_value *pArray;
	int nEntry;
	if( nArg < 3 || !ph7_value_is_array(apArg[0]) ){
		/* Invalid arguments,return NULL */
		ph7_result_null(pCtx);
		return PH7_OK;
	}
	/* Create a new array */
	pArray = ph7_context_new_array(pCtx);
	if( pArray == 0 ){
		ph7_result_null(pCtx);
		return PH7_OK;
	}
	/* Point to the internal representation of the input hashmap */
	pMap = (ph7_hashmap *)apArg[0]->x.pOther;
	/* Extract the total number of desired entry to insert */
	nEntry = ph7_value_to_int(apArg[1]);
	if( nEntry < 0 ){
		nEntry = -nEntry;
		if( nEntry > 1048576 ){
			nEntry = 1048576; /* Limit imposed by PHP */
		}
		if( nEntry > (int)pMap->nEntry ){
			nEntry -= (int)pMap->nEntry;
			/* Insert given items first */
			while( nEntry > 0 ){
				ph7_array_add_elem(pArray,0,apArg[2]);
				nEntry--;
			}
			/* Merge the two arrays */
			HashmapMerge(pMap,(ph7_hashmap *)pArray->x.pOther);
		}else{
			PH7_HashmapDup(pMap,(ph7_hashmap *)pArray->x.pOther);
		}
	}else if( nEntry > 0 ){
		if( nEntry > 1048576 ){
			nEntry = 1048576; /* Limit imposed by PHP */
		}
		if( nEntry > (int)pMap->nEntry ){
			nEntry -= (int)pMap->nEntry;
			/* Merge the two arrays first */
			HashmapMerge(pMap,(ph7_hashmap *)pArray->x.pOther);
			/* Insert given items */
			while( nEntry > 0 ){
				ph7_array_add_elem(pArray,0,apArg[2]);
				nEntry--;
			}
		}else{
			PH7_HashmapDup(pMap,(ph7_hashmap *)pArray->x.pOther);
		}
	}
	/* Return the new array */
	ph7_result_value(pCtx,pArray);
	return PH7_OK;
}
/*
 * array array_replace(array &$array,array &$array1,...)
 *  Replaces elements from passed arrays into the first array.
 * Parameters
 * $array
 *   The array in which elements are replaced.
 * $array1
 *   The array from which elements will be extracted.
 * ....
 *  More arrays from which elements will be extracted.
 *  Values from later arrays overwrite the previous values.
 * Return
 *  Returns an array, or NULL if an error occurs.
 */
static int ph7_hashmap_replace(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	ph7_hashmap *pMap;
	ph7_value *pArray;
	int i;
	if( nArg < 1 ){
		/* Invalid arguments,return NULL */
		ph7_result_null(pCtx);
		return PH7_OK;
	}
	/* Create a new array */
	pArray = ph7_context_new_array(pCtx);
	if( pArray == 0 ){
		ph7_result_null(pCtx);
		return PH7_OK;
	}
	/* Perform the requested operation */
	for( i = 0 ; i < nArg ; i++ ){
		if( !ph7_value_is_array(apArg[i]) ){
			continue;
		}
		/* Point to the internal representation of the input hashmap */
		pMap = (ph7_hashmap *)apArg[i]->x.pOther;
		HashmapOverwrite(pMap,(ph7_hashmap *)pArray->x.pOther);
	}
	/* Return the new array */
	ph7_result_value(pCtx,pArray);
	return PH7_OK;
}
/*
 * array array_filter(array $input [,callback $callback ])
 *  Filters elements of an array using a callback function.
 * Parameters
 *  $input
 *    The array to iterate over
 * $callback
 *    The callback function to use
 *    If no callback is supplied, all entries of input equal to FALSE (see converting to boolean)
 *    will be removed.
 * Return
 *  The filtered array.
 */
static int ph7_hashmap_filter(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	ph7_hashmap_node *pEntry;
	ph7_hashmap *pMap;
	ph7_value *pArray;
	ph7_value sResult;   /* Callback result */
	ph7_value *pValue;
	sxi32 rc;
	int keep;
	sxu32 n;
	if( nArg < 1 || !ph7_value_is_array(apArg[0]) ){
		/* Invalid arguments,return NULL */
		ph7_result_null(pCtx);
		return PH7_OK;
	}
	/* Create a new array */
	pArray = ph7_context_new_array(pCtx);
	if( pArray == 0 ){
		ph7_result_null(pCtx);
		return PH7_OK;
	}
	/* Point to the internal representation of the input hashmap */
	pMap = (ph7_hashmap *)apArg[0]->x.pOther;
	pEntry = pMap->pFirst;
	PH7_MemObjInit(pMap->pVm,&sResult);
	sResult.nIdx = SXU32_HIGH; /* Mark as constant */
	/* Perform the requested operation */
	for( n = 0 ; n < pMap->nEntry ; n++ ){
		/* Extract node value */
		pValue = HashmapExtractNodeValue(pEntry);
		if( nArg > 1 && pValue ){
			/* Invoke the given callback */
			keep = FALSE;
			rc = PH7_VmCallUserFunction(pMap->pVm,apArg[1],1,&pValue,&sResult);
			if( rc == SXRET_OK ){
				/* Perform a boolean cast */
				keep = ph7_value_to_bool(&sResult);
			}
			PH7_MemObjRelease(&sResult);
		}else{
			/* No available callback,check for empty item */
			keep = !PH7_MemObjIsEmpty(pValue);
		}
		if( keep ){
			/* Perform the insertion,now the callback returned true */
			HashmapInsertNode((ph7_hashmap *)pArray->x.pOther,pEntry,TRUE);
		}
		/* Point to the next entry */
		pEntry = pEntry->pPrev; /* Reverse link */
	}
	ph7_result_value(pCtx,pArray);
	return PH7_OK;
}
/*
 * array array_map(callback $callback,array $arr1)
 *  Applies the callback to the elements of the given arrays.
 * Parameters
 *  $callback
 *   Callback function to run for each element in each array.
 * $arr1
 *   An array to run through the callback function.
 * Return
 *  Returns an array containing all the elements of arr1 after applying
 *  the callback function to each one. 
 * NOTE:
 *  array_map() passes only a single value to the callback. 
 */
static int ph7_hashmap_map(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	ph7_value *pArray,*pValue,sKey,sResult;
	ph7_hashmap_node *pEntry;
	ph7_hashmap *pMap;
	sxu32 n;
	if( nArg < 2 || !ph7_value_is_array(apArg[1]) ){
		/* Invalid arguments,return NULL */
		ph7_result_null(pCtx);
		return PH7_OK;
	}
	/* Create a new array */
	pArray = ph7_context_new_array(pCtx);
	if( pArray == 0 ){
		ph7_result_null(pCtx);
		return PH7_OK;
	}
	/* Point to the internal representation of the input hashmap */
	pMap = (ph7_hashmap *)apArg[1]->x.pOther;
	PH7_MemObjInit(pMap->pVm,&sResult);
	PH7_MemObjInit(pMap->pVm,&sKey);
	sResult.nIdx = SXU32_HIGH; /* Mark as constant */
	sKey.nIdx    = SXU32_HIGH; /* Mark as constant */
	/* Perform the requested operation */
	pEntry = pMap->pFirst;
	for( n = 0 ; n < pMap->nEntry ; n++ ){
		/* Extrcat the node value */
		pValue = HashmapExtractNodeValue(pEntry);
		if( pValue ){
			sxi32 rc;
			/* Invoke the supplied callback */
			rc = PH7_VmCallUserFunction(pMap->pVm,apArg[0],1,&pValue,&sResult);
			/* Extract the node key */
			PH7_HashmapExtractNodeKey(pEntry,&sKey);
			if( rc != SXRET_OK ){
				/* An error occured while invoking the supplied callback [i.e: not defined] */
				ph7_array_add_elem(pArray,&sKey,pValue); /* Keep the same value */
			}else{
				/* Insert the callback return value */
				ph7_array_add_elem(pArray,&sKey,&sResult);
			}
			PH7_MemObjRelease(&sKey);
			PH7_MemObjRelease(&sResult);
		}
		/* Point to the next entry */
		pEntry = pEntry->pPrev; /* Reverse link */
	}
	ph7_result_value(pCtx,pArray);
	return PH7_OK;
}
/*
 * value array_reduce(array $input,callback $function[, value $initial = NULL ])
 *  Iteratively reduce the array to a single value using a callback function.
 * Parameters
 *  $input
 *   The input array.
 *  $function
 *  The callback function.
 * $initial
 *  If the optional initial is available, it will be used at the beginning 
 *  of the process, or as a final result in case the array is empty.
 * Return
 *  Returns the resulting value. 
 *  If the array is empty and initial is not passed, array_reduce() returns NULL. 
 */
static int ph7_hashmap_reduce(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	ph7_hashmap_node *pEntry;
	ph7_hashmap *pMap;
	ph7_value *pValue;
	ph7_value sResult;
	sxu32 n;
	if( nArg < 2 || !ph7_value_is_array(apArg[0]) ){
		/* Invalid/Missing arguments,return NULL */
		ph7_result_null(pCtx);
		return PH7_OK;
	}
	/* Point to the internal representation of the input hashmap */
	pMap = (ph7_hashmap *)apArg[0]->x.pOther;
	/* Assume a NULL initial value */
	PH7_MemObjInit(pMap->pVm,&sResult);
	sResult.nIdx = SXU32_HIGH; /* Mark as constant */
	if( nArg > 2 ){
		/* Set the initial value */
		PH7_MemObjLoad(apArg[2],&sResult);
	}
	/* Perform the requested operation */
	pEntry = pMap->pFirst;
	for( n = 0 ; n < pMap->nEntry ; n++ ){
		/* Extract the node value */
		pValue = HashmapExtractNodeValue(pEntry);
		/* Invoke the supplied callback */
		PH7_VmCallUserFunctionAp(pMap->pVm,apArg[1],&sResult,&sResult,pValue,0);
		/* Point to the next entry */
		pEntry = pEntry->pPrev; /* Reverse link */
	}
	ph7_result_value(pCtx,&sResult); /* Will make it's own copy */
	PH7_MemObjRelease(&sResult);
	return PH7_OK;
}
/*
 * bool array_walk(array &$array,callback $funcname [, value $userdata ] )
 *  Apply a user function to every member of an array.
 * Parameters
 *  $array
 *   The input array.
 * $funcname
 *  Typically, funcname takes on two parameters.The array parameter's value being
 *  the first, and the key/index second.
 * Note:
 *  If funcname needs to be working with the actual values of the array,specify the first
 *  parameter of funcname as a reference. Then, any changes made to those elements will 
 *  be made in the original array itself.
 * $userdata
 *  If the optional userdata parameter is supplied, it will be passed as the third parameter
 *  to the callback funcname.
 * Return
 *  Returns TRUE on success or FALSE on failure.
 */
static int ph7_hashmap_walk(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	ph7_value *pValue,*pUserData,sKey;
	ph7_hashmap_node *pEntry;
	ph7_hashmap *pMap;
	sxi32 rc;
	sxu32 n;
	if( nArg < 2 || !ph7_value_is_array(apArg[0]) ){
		/* Invalid/Missing arguments,return FALSE */
		ph7_result_bool(pCtx,0);
		return PH7_OK;
	}
	pUserData = nArg > 2 ? apArg[2] : 0;
	/* Point to the internal representation of the input hashmap */
	pMap = (ph7_hashmap *)apArg[0]->x.pOther;
	PH7_MemObjInit(pMap->pVm,&sKey);
	sKey.nIdx = SXU32_HIGH; /* Mark as constant */
	/* Perform the desired operation */
	pEntry = pMap->pFirst;
	for( n = 0 ; n < pMap->nEntry ; n++ ){
		/* Extract the node value */
		pValue = HashmapExtractNodeValue(pEntry);
		if( pValue ){
			/* Extract the entry key */
			PH7_HashmapExtractNodeKey(pEntry,&sKey);
			/* Invoke the supplied callback */
			rc = PH7_VmCallUserFunctionAp(pMap->pVm,apArg[1],0,pValue,&sKey,pUserData,0);
			PH7_MemObjRelease(&sKey);
			if( rc != SXRET_OK ){
				/* An error occured while invoking the supplied callback [i.e: not defined] */
				ph7_result_bool(pCtx,0); /* return FALSE */
				return PH7_OK;
			}
		}
		/* Point to the next entry */
		pEntry = pEntry->pPrev; /* Reverse link */
	}
	/* All done,return TRUE */
	ph7_result_bool(pCtx,1);
	return PH7_OK;
}
/*
 * Apply a user function to every member of an array.(Recurse on array's). 
 * Refer to the [array_walk_recursive()] implementation for more information.
 */
static int HashmapWalkRecursive(
	ph7_hashmap *pMap,    /* Target hashmap */
	ph7_value *pCallback, /* User callback */
	ph7_value *pUserData, /* Callback private data */
	int iNest             /* Nesting level */
	)
{
	ph7_hashmap_node *pEntry;
	ph7_value *pValue,sKey;
	sxi32 rc;
	sxu32 n;
	/* Iterate throw hashmap entries */
	PH7_MemObjInit(pMap->pVm,&sKey);
	sKey.nIdx = SXU32_HIGH; /* Mark as constant */
	pEntry = pMap->pFirst;
	for( n = 0 ; n < pMap->nEntry ; n++ ){
		/* Extract the node value */
		pValue = HashmapExtractNodeValue(pEntry);
		if( pValue ){
			if( pValue->iFlags & MEMOBJ_HASHMAP ){
				if( iNest < 32 ){
					/* Recurse */
					iNest++;
					HashmapWalkRecursive((ph7_hashmap *)pValue->x.pOther,pCallback,pUserData,iNest);
					iNest--;
				}
			}else{
				/* Extract the node key */
				PH7_HashmapExtractNodeKey(pEntry,&sKey);
				/* Invoke the supplied callback */
				rc = PH7_VmCallUserFunctionAp(pMap->pVm,pCallback,0,pValue,&sKey,pUserData,0);
				PH7_MemObjRelease(&sKey);
				if( rc != SXRET_OK ){
					return rc;
				}
			}
		}
		/* Point to the next entry */
		pEntry = pEntry->pPrev; /* Reverse link */
	}
	return SXRET_OK;
}
/*
 * bool array_walk_recursive(array &$array,callback $funcname [, value $userdata ] )
 *  Apply a user function recursively to every member of an array.
 * Parameters
 *  $array
 *   The input array.
 * $funcname
 *  Typically, funcname takes on two parameters.The array parameter's value being
 *  the first, and the key/index second.
 * Note:
 *  If funcname needs to be working with the actual values of the array,specify the first
 *  parameter of funcname as a reference. Then, any changes made to those elements will 
 *  be made in the original array itself.
 * $userdata
 *  If the optional userdata parameter is supplied, it will be passed as the third parameter
 *  to the callback funcname.
 * Return
 *  Returns TRUE on success or FALSE on failure.
 */
static int ph7_hashmap_walk_recursive(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	ph7_hashmap *pMap;
	sxi32 rc;
	if( nArg < 2 || !ph7_value_is_array(apArg[0]) ){
		/* Invalid/Missing arguments,return FALSE */
		ph7_result_bool(pCtx,0);
		return PH7_OK;
	}
	/* Point to the internal representation of the input hashmap */
	pMap = (ph7_hashmap *)apArg[0]->x.pOther;
	/* Perform the desired operation */
	rc = HashmapWalkRecursive(pMap,apArg[1],nArg > 2 ? apArg[2] : 0,0);
	/* All done */
	ph7_result_bool(pCtx,rc == SXRET_OK);
	return PH7_OK;
}
/*
 * Table of hashmap functions.
 */
static const ph7_builtin_func aHashmapFunc[] = {
	{"count",             ph7_hashmap_count },
	{"sizeof",            ph7_hashmap_count },
	{"array_key_exists",  ph7_hashmap_key_exists },
	{"array_pop",         ph7_hashmap_pop     },
	{"array_push",        ph7_hashmap_push    },
	{"array_shift",       ph7_hashmap_shift   },
	{"array_product",     ph7_hashmap_product },
	{"array_sum",         ph7_hashmap_sum     },
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
	{"array_reduce",      ph7_hashmap_reduce  },
	{"array_walk",        ph7_hashmap_walk    },
	{"array_walk_recursive", ph7_hashmap_walk_recursive },
	{"in_array",          ph7_hashmap_in_array},
	{"sort",              ph7_hashmap_sort    },
	{"asort",             ph7_hashmap_asort   },
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
PH7_PRIVATE sxi32 PH7_HashmapDump(SyBlob *pOut,ph7_hashmap *pMap,int ShowType,int nTab,int nDepth)
{
	ph7_hashmap_node *pEntry;
	ph7_value *pObj;
	sxu32 n = 0;
	int isRef;
	sxi32 rc;
	int i;
	if( nDepth > 31 ){
		static const char zInfinite[] = "Nesting limit reached: Infinite recursion?";
		/* Nesting limit reached */
		SyBlobAppend(&(*pOut),zInfinite,sizeof(zInfinite)-1);
		if( ShowType ){
			SyBlobAppend(&(*pOut),")",sizeof(char));
		}
		return SXERR_LIMIT;
	}
	/* Point to the first inserted entry */
	pEntry = pMap->pFirst;
	rc = SXRET_OK;
	if( !ShowType ){
		SyBlobAppend(&(*pOut),"Array(",sizeof("Array(")-1);
	}
	/* Total entries */
	SyBlobFormat(&(*pOut),"%u) {",pMap->nEntry);
#ifdef __WINNT__
	SyBlobAppend(&(*pOut),"\r\n",sizeof("\r\n")-1);
#else
	SyBlobAppend(&(*pOut),"\n",sizeof(char));
#endif
	for(;;){
		if( n >= pMap->nEntry ){
			break;
		}
		for( i = 0 ; i < nTab ; i++ ){
			SyBlobAppend(&(*pOut)," ",sizeof(char));
		}
		/* Dump key */
		if( pEntry->iType == HASHMAP_INT_NODE){
			SyBlobFormat(&(*pOut),"[%qd] =>",pEntry->xKey.iKey);
		}else{
			SyBlobFormat(&(*pOut),"[%.*s] =>",
				SyBlobLength(&pEntry->xKey.sKey),SyBlobData(&pEntry->xKey.sKey));
		}
#ifdef __WINNT__
		SyBlobAppend(&(*pOut),"\r\n",sizeof("\r\n")-1);
#else
		SyBlobAppend(&(*pOut),"\n",sizeof(char));
#endif		
		/* Dump node value */
		pObj = HashmapExtractNodeValue(pEntry);
		isRef = 0;
		if( pObj ){
			if( pEntry->iFlags & HASHMAP_NODE_FOREIGN_OBJ ){
				/* Referenced object */
				isRef = 1;
			}
			rc = PH7_MemObjDump(&(*pOut),pObj,ShowType,nTab+1,nDepth,isRef);
			if( rc == SXERR_LIMIT ){
				break;
			}
		}
		/* Point to the next entry */
		n++;
		pEntry = pEntry->pPrev; /* Reverse link */
	}
	for( i = 0 ; i < nTab ; i++ ){
		SyBlobAppend(&(*pOut)," ",sizeof(char));
	}
	SyBlobAppend(&(*pOut),"}",sizeof(char));
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
