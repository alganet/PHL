/*
 * Symisc PH7: An embeddable bytecode compiler and a virtual machine for the PHP(5) programming language.
 * Copyright (C) 2011, 2012, 2013, 2014 Symisc Systems http://ph7.symisc.net/
 * Version 2.1.4
 * For information on licensing,redistribution of this file,and for a DISCLAIMER OF ALL WARRANTIES
 * please contact Symisc Systems via:
 *       legal@symisc.net
 *       licensing@symisc.net
 *       contact@symisc.net
 * or visit:
 *      http://ph7.symisc.net/
 */
/*
 * Copyright (C) 2011, 2012, 2013, 2014 Symisc Systems. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. Redistributions in any form must be accompanied by information on
 *    how to obtain complete source code for the PH7 engine and any 
 *    accompanying software that uses the PH7 engine software.
 *    The source code must either be included in the distribution
 *    or be available for no more than the cost of distribution plus
 *    a nominal fee, and must be freely redistributable under reasonable
 *    conditions. For an executable file, complete source code means
 *    the source code for all modules it contains.It does not include
 *    source code for modules or files that typically accompany the major
 *    components of the operating system on which the executable file runs.
 *
 * THIS SOFTWARE IS PROVIDED BY SYMISC SYSTEMS ``AS IS'' AND ANY EXPRESS
 * OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE, OR
 * NON-INFRINGEMENT, ARE DISCLAIMED.  IN NO EVENT SHALL SYMISC SYSTEMS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR
 * BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
 * WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE
 * OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN
 * IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */
/*
 * $SymiscID: ph7.c v2.1 UNIX|WIN32/64 2012-09-15 09:00 stable <chm@symisc.net> $ 
 */
/* This file is an amalgamation of many separate C source files from PH7 version 2.1.
 * By combining all the individual C code files into this single large file, the entire code
 * can be compiled as a single translation unit.This allows many compilers to do optimization's
 * that would not be possible if the files were compiled separately.Performance improvements
 * are commonly seen when PH7 is compiled as a single translation unit.
 *
 * This file is all you need to compile PH7.To use PH7 in other programs, you need
 * this file and the "ph7.h" header file that defines the programming interface to the 
 * PH7 engine.(If you do not have the "ph7.h" header file at hand, you will find
 * a copy embedded within the text of this file.Search for "Header file: <ph7.h>" to find
 * the start of the embedded ph7.h header file.) Additional code files may be needed if
 * you want a wrapper to interface PH7 with your choice of programming language.
 * To get the official documentation,please visit http://ph7.symisc.net/
 */
 /*
  * Make the sure the following is defined in the amalgamation build
  */
 #ifndef PH7_AMALGAMATION
 #define PH7_AMALGAMATION
 #endif /* PH7_AMALGAMATION */




/*
 * ----------------------------------------------------------
 * File: hashmap.c
 * MD5: cf4287c2602a9c97df208364cb9be084
 * ----------------------------------------------------------
 */
/*
 * Symisc PH7: An embeddable bytecode compiler and a virtual machine for the PHP(5) programming language.
 * Copyright (C) 2011-2012, Symisc Systems http://ph7.symisc.net/
 * Version 2.1.4
 * For information on licensing,redistribution of this file,and for a DISCLAIMER OF ALL WARRANTIES
 * please contact Symisc Systems via:
 *       legal@symisc.net
 *       licensing@symisc.net
 *       contact@symisc.net
 * or visit:
 *      http://ph7.symisc.net/
 */
 /* $SymiscID: hashmap.c v3.5 FreeBSD 2012-08-07 08:29 stable <chm@symisc.net> $ */
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
 * Authors:
 *  Symisc Systems,devel@symisc.net.
 *  Copyright (C) Symisc Systems,http://ph7.symisc.net
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
/*
 * ----------------------------------------------------------
 * File: constant.c
 * MD5: 9cf62714d3cc5de3825c4eebc8378bb7
 * ----------------------------------------------------------
 */
/*
 * Symisc PH7: An embeddable bytecode compiler and a virtual machine for the PHP(5) programming language.
 * Copyright (C) 2011-2012, Symisc Systems http://ph7.symisc.net/
 * Version 2.1.4
 * For information on licensing,redistribution of this file,and for a DISCLAIMER OF ALL WARRANTIES
 * please contact Symisc Systems via:
 *       legal@symisc.net
 *       licensing@symisc.net
 *       contact@symisc.net
 * or visit:
 *      http://ph7.symisc.net/
 */
 /* $SymiscID: constant.c v1.1 Win7 2012-08-07 08:22 devel <chm@symisc.net> $ */
#ifndef PH7_AMALGAMATION
#include "ph7int.h"
#endif
/* This file implement built-in constants for the PH7 engine. */
/*
 * PH7_VERSION
 * __PH7__
 *   Expand the current version of the PH7 engine.
 */
static void PH7_VER_Const(ph7_value *pVal,void *pUnused)
{
	SXUNUSED(pUnused);
	ph7_value_string(pVal,ph7_lib_signature(),-1/*Compute length automatically*/);
}
#ifdef __WINNT__
#include <Windows.h>
#elif defined(__UNIXES__)
#include <sys/utsname.h>
#endif
/*
 * PHP_OS
 *  Expand the name of the host Operating System.
 */
static void PH7_OS_Const(ph7_value *pVal,void *pUnused)
{
#if defined(__WINNT__)
	ph7_value_string(pVal,"WINNT",(int)sizeof("WINNT")-1);
#elif defined(__UNIXES__)
	struct utsname sInfo;
	if( uname(&sInfo) != 0 ){
		ph7_value_string(pVal,"Unix",(int)sizeof("Unix")-1);
	}else{
		ph7_value_string(pVal,sInfo.sysname,-1);
	}
#else
	ph7_value_string(pVal,"Host OS",(int)sizeof("Host OS")-1);
#endif
	SXUNUSED(pUnused);
}
/*
 * PHP_EOL
 *  Expand the correct 'End Of Line' symbol for this platform.
 */
static void PH7_EOL_Const(ph7_value *pVal,void *pUnused)
{
	SXUNUSED(pUnused);
#ifdef __WINNT__
	ph7_value_string(pVal,"\r\n",(int)sizeof("\r\n")-1);
#else
	ph7_value_string(pVal,"\n",(int)sizeof(char));
#endif
}
/*
 * PHP_INT_MAX
 * Expand the largest integer supported.
 * Note that PH7 deals with 64-bit integer for all platforms.
 */
static void PH7_INTMAX_Const(ph7_value *pVal,void *pUnused)
{
	SXUNUSED(pUnused);
	ph7_value_int64(pVal,SXI64_HIGH);
}
/*
 * PHP_INT_SIZE
 * Expand the size in bytes of a 64-bit integer.
 */
static void PH7_INTSIZE_Const(ph7_value *pVal,void *pUnused)
{
	SXUNUSED(pUnused);
	ph7_value_int64(pVal,sizeof(sxi64));
}
/*
 * DIRECTORY_SEPARATOR.
 * Expand the directory separator character.
 */
static void PH7_DIRSEP_Const(ph7_value *pVal,void *pUnused)
{
	SXUNUSED(pUnused);
#ifdef __WINNT__
	ph7_value_string(pVal,"\\",(int)sizeof(char));
#else
	ph7_value_string(pVal,"/",(int)sizeof(char));
#endif
}
/*
 * PATH_SEPARATOR.
 * Expand the path separator character.
 */
static void PH7_PATHSEP_Const(ph7_value *pVal,void *pUnused)
{
	SXUNUSED(pUnused);
#ifdef __WINNT__
	ph7_value_string(pVal,";",(int)sizeof(char));
#else
	ph7_value_string(pVal,":",(int)sizeof(char));
#endif
}
#ifndef __WINNT__
#include <time.h>
#endif
/*
 * __TIME__
 *  Expand the current time (GMT).
 */
static void PH7_TIME_Const(ph7_value *pVal,void *pUnused)
{
	Sytm sTm;
#ifdef __WINNT__
	SYSTEMTIME sOS;
	GetSystemTime(&sOS);
	SYSTEMTIME_TO_SYTM(&sOS,&sTm);
#else
	struct tm *pTm;
	time_t t;
	time(&t);
	pTm = gmtime(&t);
	STRUCT_TM_TO_SYTM(pTm,&sTm);
#endif
	SXUNUSED(pUnused); /* cc warning */
	/* Expand */
	ph7_value_string_format(pVal,"%02d:%02d:%02d",sTm.tm_hour,sTm.tm_min,sTm.tm_sec);
}
/*
 * __DATE__
 *  Expand the current date in the ISO-8601 format.
 */
static void PH7_DATE_Const(ph7_value *pVal,void *pUnused)
{
	Sytm sTm;
#ifdef __WINNT__
	SYSTEMTIME sOS;
	GetSystemTime(&sOS);
	SYSTEMTIME_TO_SYTM(&sOS,&sTm);
#else
	struct tm *pTm;
	time_t t;
	time(&t);
	pTm = gmtime(&t);
	STRUCT_TM_TO_SYTM(pTm,&sTm);
#endif
	SXUNUSED(pUnused); /* cc warning */
	/* Expand */
	ph7_value_string_format(pVal,"%04d-%02d-%02d",sTm.tm_year,sTm.tm_mon+1,sTm.tm_mday);
}
/*
 * __FILE__
 *  Path of the processed script.
 */
static void PH7_FILE_Const(ph7_value *pVal,void *pUserData)
{
	ph7_vm *pVm = (ph7_vm *)pUserData;
	SyString *pFile;
	/* Peek the top entry */
	pFile = (SyString *)SySetPeek(&pVm->aFiles);
	if( pFile == 0 ){
		/* Expand the magic word: ":MEMORY:" */
		ph7_value_string(pVal,":MEMORY:",(int)sizeof(":MEMORY:")-1);
	}else{
		ph7_value_string(pVal,pFile->zString,pFile->nByte);
	}
}
/*
 * __DIR__
 *  Directory holding the processed script.
 */
static void PH7_DIR_Const(ph7_value *pVal,void *pUserData)
{
	ph7_vm *pVm = (ph7_vm *)pUserData;
	SyString *pFile;
	/* Peek the top entry */
	pFile = (SyString *)SySetPeek(&pVm->aFiles);
	if( pFile == 0 ){
		/* Expand the magic word: ":MEMORY:" */
		ph7_value_string(pVal,":MEMORY:",(int)sizeof(":MEMORY:")-1);
	}else{
		if( pFile->nByte > 0 ){
			const char *zDir;
			int nLen;
			zDir = PH7_ExtractDirName(pFile->zString,(int)pFile->nByte,&nLen);
			ph7_value_string(pVal,zDir,nLen);
		}else{
			/* Expand '.' as the current directory*/
			ph7_value_string(pVal,".",(int)sizeof(char));
		}
	}
}
/*
 * PHP_SHLIB_SUFFIX
 *  Expand shared library suffix.
 */
static void PH7_PHP_SHLIB_SUFFIX_Const(ph7_value *pVal,void *pUserData)
{
#ifdef __WINNT__
	ph7_value_string(pVal,"dll",(int)sizeof("dll")-1);
#else
	ph7_value_string(pVal,"so",(int)sizeof("so")-1);
#endif
	SXUNUSED(pUserData); /* cc warning */
}
/*
 * E_ERROR
 *  Expands 1
 */
static void PH7_E_ERROR_Const(ph7_value *pVal,void *pUserData)
{
	ph7_value_int(pVal,1);
	SXUNUSED(pUserData);
}
/*
 * E_WARNING
 *  Expands 2
 */
static void PH7_E_WARNING_Const(ph7_value *pVal,void *pUserData)
{
	ph7_value_int(pVal,2);
	SXUNUSED(pUserData);
}
/*
 * E_PARSE
 *  Expands 4
 */
static void PH7_E_PARSE_Const(ph7_value *pVal,void *pUserData)
{
	ph7_value_int(pVal,4);
	SXUNUSED(pUserData);
}
/*
 * E_NOTICE
 * Expands 8
 */
static void PH7_E_NOTICE_Const(ph7_value *pVal,void *pUserData)
{
	ph7_value_int(pVal,8);
	SXUNUSED(pUserData);
}
/*
 * E_CORE_ERROR
 * Expands 16
 */
static void PH7_E_CORE_ERROR_Const(ph7_value *pVal,void *pUserData)
{
	ph7_value_int(pVal,16);
	SXUNUSED(pUserData);
}
/*
 * E_CORE_WARNING
 * Expands 32
 */
static void PH7_E_CORE_WARNING_Const(ph7_value *pVal,void *pUserData)
{
	ph7_value_int(pVal,32);
	SXUNUSED(pUserData);
}
/*
 * E_COMPILE_ERROR
 * Expands 64
 */
static void PH7_E_COMPILE_ERROR_Const(ph7_value *pVal,void *pUserData)
{
	ph7_value_int(pVal,64);
	SXUNUSED(pUserData);
}
/*
 * E_COMPILE_WARNING 
 * Expands 128
 */
static void PH7_E_COMPILE_WARNING_Const(ph7_value *pVal,void *pUserData)
{
	ph7_value_int(pVal,128);
	SXUNUSED(pUserData);
}
/*
 * E_USER_ERROR
 * Expands 256
 */
static void PH7_E_USER_ERROR_Const(ph7_value *pVal,void *pUserData)
{
	ph7_value_int(pVal,256);
	SXUNUSED(pUserData);
}
/*
 * E_USER_WARNING
 * Expands 512
 */
static void PH7_E_USER_WARNING_Const(ph7_value *pVal,void *pUserData)
{
	ph7_value_int(pVal,512);
	SXUNUSED(pUserData);
}
/*
 * E_USER_NOTICE
 * Expands 1024
 */
static void PH7_E_USER_NOTICE_Const(ph7_value *pVal,void *pUserData)
{
	ph7_value_int(pVal,1024);
	SXUNUSED(pUserData);
}
/*
 * E_STRICT
 * Expands 2048
 */
static void PH7_E_STRICT_Const(ph7_value *pVal,void *pUserData)
{
	ph7_value_int(pVal,2048);
	SXUNUSED(pUserData);
}
/*
 * E_RECOVERABLE_ERROR 
 * Expands 4096
 */
static void PH7_E_RECOVERABLE_ERROR_Const(ph7_value *pVal,void *pUserData)
{
	ph7_value_int(pVal,4096);
	SXUNUSED(pUserData);
}
/*
 * E_DEPRECATED
 * Expands 8192
 */
static void PH7_E_DEPRECATED_Const(ph7_value *pVal,void *pUserData)
{
	ph7_value_int(pVal,8192);
	SXUNUSED(pUserData);
}
/*
 * E_USER_DEPRECATED 
 *   Expands 16384.
 */
static void PH7_E_USER_DEPRECATED_Const(ph7_value *pVal,void *pUserData)
{
	ph7_value_int(pVal,16384);
	SXUNUSED(pUserData);
}
/*
 * E_ALL
 *  Expands 32767
 */
static void PH7_E_ALL_Const(ph7_value *pVal,void *pUserData)
{
	ph7_value_int(pVal,32767);
	SXUNUSED(pUserData);
}
/*
 * CASE_LOWER
 *  Expands 0.
 */
static void PH7_CASE_LOWER_Const(ph7_value *pVal,void *pUserData)
{
	ph7_value_int(pVal,0);
	SXUNUSED(pUserData);
}
/*
 * CASE_UPPER
 *  Expands 1.
 */
static void PH7_CASE_UPPER_Const(ph7_value *pVal,void *pUserData)
{
	ph7_value_int(pVal,1);
	SXUNUSED(pUserData);
}
/*
 * STR_PAD_LEFT
 *  Expands 0.
 */
static void PH7_STR_PAD_LEFT_Const(ph7_value *pVal,void *pUserData)
{
	ph7_value_int(pVal,0);
	SXUNUSED(pUserData);
}
/*
 * STR_PAD_RIGHT
 *  Expands 1.
 */
static void PH7_STR_PAD_RIGHT_Const(ph7_value *pVal,void *pUserData)
{
	ph7_value_int(pVal,1);
	SXUNUSED(pUserData);
}
/*
 * STR_PAD_BOTH
 *  Expands 2.
 */
static void PH7_STR_PAD_BOTH_Const(ph7_value *pVal,void *pUserData)
{
	ph7_value_int(pVal,2);
	SXUNUSED(pUserData);
}
/*
 * COUNT_NORMAL
 *  Expands 0
 */
static void PH7_COUNT_NORMAL_Const(ph7_value *pVal,void *pUserData)
{
	ph7_value_int(pVal,0);
	SXUNUSED(pUserData);
}
/*
 * COUNT_RECURSIVE
 *  Expands 1.
 */
static void PH7_COUNT_RECURSIVE_Const(ph7_value *pVal,void *pUserData)
{
	ph7_value_int(pVal,1);
	SXUNUSED(pUserData);
}
/*
 * SORT_ASC
 *  Expands 1.
 */
static void PH7_SORT_ASC_Const(ph7_value *pVal,void *pUserData)
{
	ph7_value_int(pVal,1);
	SXUNUSED(pUserData);
}
/*
 * SORT_DESC
 *  Expands 2.
 */
static void PH7_SORT_DESC_Const(ph7_value *pVal,void *pUserData)
{
	ph7_value_int(pVal,2);
	SXUNUSED(pUserData);
}
/*
 * SORT_REGULAR
 *  Expands 3.
 */
static void PH7_SORT_REG_Const(ph7_value *pVal,void *pUserData)
{
	ph7_value_int(pVal,3);
	SXUNUSED(pUserData);
}
/*
 * SORT_NUMERIC
 *  Expands 4.
 */
static void PH7_SORT_NUMERIC_Const(ph7_value *pVal,void *pUserData)
{
	ph7_value_int(pVal,4);
	SXUNUSED(pUserData);
}
/*
 * SORT_STRING
 *  Expands 5.
 */
static void PH7_SORT_STRING_Const(ph7_value *pVal,void *pUserData)
{
	ph7_value_int(pVal,5);
	SXUNUSED(pUserData);
}
/*
 * PHP_ROUND_HALF_UP
 *  Expands 1.
 */
static void PH7_PHP_ROUND_HALF_UP_Const(ph7_value *pVal,void *pUserData)
{
	ph7_value_int(pVal,1);
	SXUNUSED(pUserData);
}
/*
 * SPHP_ROUND_HALF_DOWN
 *  Expands 2.
 */
static void PH7_PHP_ROUND_HALF_DOWN_Const(ph7_value *pVal,void *pUserData)
{
	ph7_value_int(pVal,2);
	SXUNUSED(pUserData);
}
/*
 * PHP_ROUND_HALF_EVEN
 *  Expands 3.
 */
static void PH7_PHP_ROUND_HALF_EVEN_Const(ph7_value *pVal,void *pUserData)
{
	ph7_value_int(pVal,3);
	SXUNUSED(pUserData);
}
/*
 * PHP_ROUND_HALF_ODD
 *  Expands 4.
 */
static void PH7_PHP_ROUND_HALF_ODD_Const(ph7_value *pVal,void *pUserData)
{
	ph7_value_int(pVal,4);
	SXUNUSED(pUserData);
}
/*
 * DEBUG_BACKTRACE_PROVIDE_OBJECT
 *  Expand 0x01 
 * NOTE:
 *  The expanded value must be a power of two.
 */
static void PH7_DBPO_Const(ph7_value *pVal,void *pUserData)
{
	ph7_value_int(pVal,0x01); /* MUST BE A POWER OF TWO */
	SXUNUSED(pUserData);
}
/*
 * DEBUG_BACKTRACE_IGNORE_ARGS
 *  Expand 0x02 
 * NOTE:
 *  The expanded value must be a power of two.
 */
static void PH7_DBIA_Const(ph7_value *pVal,void *pUserData)
{
	ph7_value_int(pVal,0x02); /* MUST BE A POWER OF TWO */
	SXUNUSED(pUserData);
}
#ifdef PH7_ENABLE_MATH_FUNC
/*
 * M_PI
 *  Expand the value of pi.
 */
static void PH7_M_PI_Const(ph7_value *pVal,void *pUserData)
{
	SXUNUSED(pUserData); /* cc warning */
	ph7_value_double(pVal,PH7_PI);
}
/*
 * M_E
 *  Expand 2.7182818284590452354
 */
static void PH7_M_E_Const(ph7_value *pVal,void *pUserData)
{
	SXUNUSED(pUserData); /* cc warning */
	ph7_value_double(pVal,2.7182818284590452354);
}
/*
 * M_LOG2E
 *  Expand 2.7182818284590452354
 */
static void PH7_M_LOG2E_Const(ph7_value *pVal,void *pUserData)
{
	SXUNUSED(pUserData); /* cc warning */
	ph7_value_double(pVal,1.4426950408889634074);
}
/*
 * M_LOG10E
 *  Expand 0.4342944819032518276
 */
static void PH7_M_LOG10E_Const(ph7_value *pVal,void *pUserData)
{
	SXUNUSED(pUserData); /* cc warning */
	ph7_value_double(pVal,0.4342944819032518276);
}
/*
 * M_LN2
 *  Expand 	0.69314718055994530942
 */
static void PH7_M_LN2_Const(ph7_value *pVal,void *pUserData)
{
	SXUNUSED(pUserData); /* cc warning */
	ph7_value_double(pVal,0.69314718055994530942);
}
/*
 * M_LN10
 *  Expand 	2.30258509299404568402
 */
static void PH7_M_LN10_Const(ph7_value *pVal,void *pUserData)
{
	SXUNUSED(pUserData); /* cc warning */
	ph7_value_double(pVal,2.30258509299404568402);
}
/*
 * M_PI_2
 *  Expand 	1.57079632679489661923
 */
static void PH7_M_PI_2_Const(ph7_value *pVal,void *pUserData)
{
	SXUNUSED(pUserData); /* cc warning */
	ph7_value_double(pVal,1.57079632679489661923);
}
/*
 * M_PI_4
 *  Expand 	0.78539816339744830962
 */
static void PH7_M_PI_4_Const(ph7_value *pVal,void *pUserData)
{
	SXUNUSED(pUserData); /* cc warning */
	ph7_value_double(pVal,0.78539816339744830962);
}
/*
 * M_1_PI
 *  Expand 	0.31830988618379067154
 */
static void PH7_M_1_PI_Const(ph7_value *pVal,void *pUserData)
{
	SXUNUSED(pUserData); /* cc warning */
	ph7_value_double(pVal,0.31830988618379067154);
}
/*
 * M_2_PI
 *  Expand 0.63661977236758134308
 */
static void PH7_M_2_PI_Const(ph7_value *pVal,void *pUserData)
{
	SXUNUSED(pUserData); /* cc warning */
	ph7_value_double(pVal,0.63661977236758134308);
}
/*
 * M_SQRTPI
 *  Expand 1.77245385090551602729
 */
static void PH7_M_SQRTPI_Const(ph7_value *pVal,void *pUserData)
{
	SXUNUSED(pUserData); /* cc warning */
	ph7_value_double(pVal,1.77245385090551602729);
}
/*
 * M_2_SQRTPI
 *  Expand 	1.12837916709551257390
 */
static void PH7_M_2_SQRTPI_Const(ph7_value *pVal,void *pUserData)
{
	SXUNUSED(pUserData); /* cc warning */
	ph7_value_double(pVal,1.12837916709551257390);
}
/*
 * M_SQRT2
 *  Expand 	1.41421356237309504880
 */
static void PH7_M_SQRT2_Const(ph7_value *pVal,void *pUserData)
{
	SXUNUSED(pUserData); /* cc warning */
	ph7_value_double(pVal,1.41421356237309504880);
}
/*
 * M_SQRT3
 *  Expand 	1.73205080756887729352
 */
static void PH7_M_SQRT3_Const(ph7_value *pVal,void *pUserData)
{
	SXUNUSED(pUserData); /* cc warning */
	ph7_value_double(pVal,1.73205080756887729352);
}
/*
 * M_SQRT1_2
 *  Expand 	0.70710678118654752440
 */
static void PH7_M_SQRT1_2_Const(ph7_value *pVal,void *pUserData)
{
	SXUNUSED(pUserData); /* cc warning */
	ph7_value_double(pVal,0.70710678118654752440);
}
/*
 * M_LNPI
 *  Expand 	1.14472988584940017414
 */
static void PH7_M_LNPI_Const(ph7_value *pVal,void *pUserData)
{
	SXUNUSED(pUserData); /* cc warning */
	ph7_value_double(pVal,1.14472988584940017414);
}
/*
 * M_EULER
 *  Expand  0.57721566490153286061
 */
static void PH7_M_EULER_Const(ph7_value *pVal,void *pUserData)
{
	SXUNUSED(pUserData); /* cc warning */
	ph7_value_double(pVal,0.57721566490153286061);
}
#endif /* PH7_DISABLE_BUILTIN_MATH */
/*
 * DATE_ATOM
 *  Expand Atom (example: 2005-08-15T15:52:01+00:00) 
 */
static void PH7_DATE_ATOM_Const(ph7_value *pVal,void *pUserData)
{
	SXUNUSED(pUserData); /* cc warning */
	ph7_value_string(pVal,"Y-m-d\\TH:i:sP",-1/*Compute length automatically*/);
}
/*
 * DATE_COOKIE
 *  HTTP Cookies (example: Monday, 15-Aug-05 15:52:01 UTC)  
 */
static void PH7_DATE_COOKIE_Const(ph7_value *pVal,void *pUserData)
{
	SXUNUSED(pUserData); /* cc warning */
	ph7_value_string(pVal,"l, d-M-y H:i:s T",-1/*Compute length automatically*/);
}
/*
 * DATE_ISO8601
 *  ISO-8601 (example: 2005-08-15T15:52:01+0000) 
 */
static void PH7_DATE_ISO8601_Const(ph7_value *pVal,void *pUserData)
{
	SXUNUSED(pUserData); /* cc warning */
	ph7_value_string(pVal,"Y-m-d\\TH:i:sO",-1/*Compute length automatically*/);
}
/*
 * DATE_RFC822
 *  RFC 822 (example: Mon, 15 Aug 05 15:52:01 +0000) 
 */
static void PH7_DATE_RFC822_Const(ph7_value *pVal,void *pUserData)
{
	SXUNUSED(pUserData); /* cc warning */
	ph7_value_string(pVal,"D, d M y H:i:s O",-1/*Compute length automatically*/);
}
/*
 * DATE_RFC850
 *  RFC 850 (example: Monday, 15-Aug-05 15:52:01 UTC) 
 */
static void PH7_DATE_RFC850_Const(ph7_value *pVal,void *pUserData)
{
	SXUNUSED(pUserData); /* cc warning */
	ph7_value_string(pVal,"l, d-M-y H:i:s T",-1/*Compute length automatically*/);
}
/*
 * DATE_RFC1036
 *  RFC 1123 (example: Mon, 15 Aug 2005 15:52:01 +0000) 
 */
static void PH7_DATE_RFC1036_Const(ph7_value *pVal,void *pUserData)
{
	SXUNUSED(pUserData); /* cc warning */
	ph7_value_string(pVal,"D, d M y H:i:s O",-1/*Compute length automatically*/);
}
/*
 * DATE_RFC1123
 *  RFC 1123 (example: Mon, 15 Aug 2005 15:52:01 +0000)  
 */
static void PH7_DATE_RFC1123_Const(ph7_value *pVal,void *pUserData)
{
	SXUNUSED(pUserData); /* cc warning */
	ph7_value_string(pVal,"D, d M Y H:i:s O",-1/*Compute length automatically*/);
}
/*
 * DATE_RFC2822
 *  RFC 2822 (Mon, 15 Aug 2005 15:52:01 +0000)  
 */
static void PH7_DATE_RFC2822_Const(ph7_value *pVal,void *pUserData)
{
	SXUNUSED(pUserData); /* cc warning */
	ph7_value_string(pVal,"D, d M Y H:i:s O",-1/*Compute length automatically*/);
}
/*
 * DATE_RSS
 *  RSS (Mon, 15 Aug 2005 15:52:01 +0000) 
 */
static void PH7_DATE_RSS_Const(ph7_value *pVal,void *pUserData)
{
	SXUNUSED(pUserData); /* cc warning */
	ph7_value_string(pVal,"D, d M Y H:i:s O",-1/*Compute length automatically*/);
}
/*
 * DATE_W3C
 *  World Wide Web Consortium (example: 2005-08-15T15:52:01+00:00) 
 */
static void PH7_DATE_W3C_Const(ph7_value *pVal,void *pUserData)
{
	SXUNUSED(pUserData); /* cc warning */
	ph7_value_string(pVal,"Y-m-d\\TH:i:sP",-1/*Compute length automatically*/);
}
/*
 * ENT_COMPAT
 *  Expand 0x01 (Must be a power of two)
 */
static void PH7_ENT_COMPAT_Const(ph7_value *pVal,void *pUserData)
{
	SXUNUSED(pUserData); /* cc warning */
	ph7_value_int(pVal,0x01);
}
/*
 * ENT_QUOTES
 *  Expand 0x02 (Must be a power of two)
 */
static void PH7_ENT_QUOTES_Const(ph7_value *pVal,void *pUserData)
{
	SXUNUSED(pUserData); /* cc warning */
	ph7_value_int(pVal,0x02);
}
/*
 * ENT_NOQUOTES
 *  Expand 0x04 (Must be a power of two)
 */
static void PH7_ENT_NOQUOTES_Const(ph7_value *pVal,void *pUserData)
{
	SXUNUSED(pUserData); /* cc warning */
	ph7_value_int(pVal,0x04);
}
/*
 * ENT_IGNORE
 *  Expand 0x08 (Must be a power of two)
 */
static void PH7_ENT_IGNORE_Const(ph7_value *pVal,void *pUserData)
{
	SXUNUSED(pUserData); /* cc warning */
	ph7_value_int(pVal,0x08);
}
/*
 * ENT_SUBSTITUTE
 *  Expand 0x10 (Must be a power of two)
 */
static void PH7_ENT_SUBSTITUTE_Const(ph7_value *pVal,void *pUserData)
{
	SXUNUSED(pUserData); /* cc warning */
	ph7_value_int(pVal,0x10);
}
/*
 * ENT_DISALLOWED
 *  Expand 0x20 (Must be a power of two)
 */
static void PH7_ENT_DISALLOWED_Const(ph7_value *pVal,void *pUserData)
{
	SXUNUSED(pUserData); /* cc warning */
	ph7_value_int(pVal,0x20);
}
/*
 * ENT_HTML401
 *  Expand 0x40 (Must be a power of two)
 */
static void PH7_ENT_HTML401_Const(ph7_value *pVal,void *pUserData)
{
	SXUNUSED(pUserData); /* cc warning */
	ph7_value_int(pVal,0x40);
}
/*
 * ENT_XML1
 *  Expand 0x80 (Must be a power of two)
 */
static void PH7_ENT_XML1_Const(ph7_value *pVal,void *pUserData)
{
	SXUNUSED(pUserData); /* cc warning */
	ph7_value_int(pVal,0x80);
}
/*
 * ENT_XHTML
 *  Expand 0x100 (Must be a power of two)
 */
static void PH7_ENT_XHTML_Const(ph7_value *pVal,void *pUserData)
{
	SXUNUSED(pUserData); /* cc warning */
	ph7_value_int(pVal,0x100);
}
/*
 * ENT_HTML5
 *  Expand 0x200 (Must be a power of two)
 */
static void PH7_ENT_HTML5_Const(ph7_value *pVal,void *pUserData)
{
	SXUNUSED(pUserData); /* cc warning */
	ph7_value_int(pVal,0x200);
}
/*
 * ISO-8859-1
 * ISO_8859_1
 *   Expand 1
 */
static void PH7_ISO88591_Const(ph7_value *pVal,void *pUserData)
{
	SXUNUSED(pUserData); /* cc warning */
	ph7_value_int(pVal,1);
}
/*
 * UTF-8
 * UTF8
 *  Expand 2
 */
static void PH7_UTF8_Const(ph7_value *pVal,void *pUserData)
{
	SXUNUSED(pUserData); /* cc warning */
	ph7_value_int(pVal,1);
}
/*
 * HTML_ENTITIES
 *  Expand 1
 */
static void PH7_HTML_ENTITIES_Const(ph7_value *pVal,void *pUserData)
{
	SXUNUSED(pUserData); /* cc warning */
	ph7_value_int(pVal,1);
}
/*
 * HTML_SPECIALCHARS
 *  Expand 2
 */
static void PH7_HTML_SPECIALCHARS_Const(ph7_value *pVal,void *pUserData)
{
	SXUNUSED(pUserData); /* cc warning */
	ph7_value_int(pVal,2);
}
/*
 * PHP_URL_SCHEME.
 * Expand 1
 */
static void PH7_PHP_URL_SCHEME_Const(ph7_value *pVal,void *pUserData)
{
	SXUNUSED(pUserData); /* cc warning */
	ph7_value_int(pVal,1);
}
/*
 * PHP_URL_HOST.
 * Expand 2
 */
static void PH7_PHP_URL_HOST_Const(ph7_value *pVal,void *pUserData)
{
	SXUNUSED(pUserData); /* cc warning */
	ph7_value_int(pVal,2);
}
/*
 * PHP_URL_PORT.
 * Expand 3
 */
static void PH7_PHP_URL_PORT_Const(ph7_value *pVal,void *pUserData)
{
	SXUNUSED(pUserData); /* cc warning */
	ph7_value_int(pVal,3);
}
/*
 * PHP_URL_USER.
 * Expand 4
 */
static void PH7_PHP_URL_USER_Const(ph7_value *pVal,void *pUserData)
{
	SXUNUSED(pUserData); /* cc warning */
	ph7_value_int(pVal,4);
}
/*
 * PHP_URL_PASS.
 * Expand 5
 */
static void PH7_PHP_URL_PASS_Const(ph7_value *pVal,void *pUserData)
{
	SXUNUSED(pUserData); /* cc warning */
	ph7_value_int(pVal,5);
}
/*
 * PHP_URL_PATH.
 * Expand 6
 */
static void PH7_PHP_URL_PATH_Const(ph7_value *pVal,void *pUserData)
{
	SXUNUSED(pUserData); /* cc warning */
	ph7_value_int(pVal,6);
}
/*
 * PHP_URL_QUERY.
 * Expand 7
 */
static void PH7_PHP_URL_QUERY_Const(ph7_value *pVal,void *pUserData)
{
	SXUNUSED(pUserData); /* cc warning */
	ph7_value_int(pVal,7);
}
/*
 * PHP_URL_FRAGMENT.
 * Expand 8
 */
static void PH7_PHP_URL_FRAGMENT_Const(ph7_value *pVal,void *pUserData)
{
	SXUNUSED(pUserData); /* cc warning */
	ph7_value_int(pVal,8);
}
/*
 * PHP_QUERY_RFC1738
 * Expand 1
 */
static void PH7_PHP_QUERY_RFC1738_Const(ph7_value *pVal,void *pUserData)
{
	SXUNUSED(pUserData); /* cc warning */
	ph7_value_int(pVal,1);
}
/*
 * PHP_QUERY_RFC3986
 * Expand 1
 */
static void PH7_PHP_QUERY_RFC3986_Const(ph7_value *pVal,void *pUserData)
{
	SXUNUSED(pUserData); /* cc warning */
	ph7_value_int(pVal,2);
}
/*
 * FNM_NOESCAPE
 *  Expand 0x01 (Must be a power of two)
 */
static void PH7_FNM_NOESCAPE_Const(ph7_value *pVal,void *pUserData)
{
	SXUNUSED(pUserData); /* cc warning */
	ph7_value_int(pVal,0x01);
}
/*
 * FNM_PATHNAME
 *  Expand 0x02 (Must be a power of two)
 */
static void PH7_FNM_PATHNAME_Const(ph7_value *pVal,void *pUserData)
{
	SXUNUSED(pUserData); /* cc warning */
	ph7_value_int(pVal,0x02);
}
/*
 * FNM_PERIOD
 *  Expand 0x04 (Must be a power of two)
 */
static void PH7_FNM_PERIOD_Const(ph7_value *pVal,void *pUserData)
{
	SXUNUSED(pUserData); /* cc warning */
	ph7_value_int(pVal,0x04);
}
/*
 * FNM_CASEFOLD
 *  Expand 0x08 (Must be a power of two)
 */
static void PH7_FNM_CASEFOLD_Const(ph7_value *pVal,void *pUserData)
{
	SXUNUSED(pUserData); /* cc warning */
	ph7_value_int(pVal,0x08);
}
/*
 * PATHINFO_DIRNAME
 *  Expand 1.
 */
static void PH7_PATHINFO_DIRNAME_Const(ph7_value *pVal,void *pUserData)
{
	SXUNUSED(pUserData); /* cc warning */
	ph7_value_int(pVal,1);
}
/*
 * PATHINFO_BASENAME
 *  Expand 2.
 */
static void PH7_PATHINFO_BASENAME_Const(ph7_value *pVal,void *pUserData)
{
	SXUNUSED(pUserData); /* cc warning */
	ph7_value_int(pVal,2);
}
/*
 * PATHINFO_EXTENSION
 *  Expand 3.
 */
static void PH7_PATHINFO_EXTENSION_Const(ph7_value *pVal,void *pUserData)
{
	SXUNUSED(pUserData); /* cc warning */
	ph7_value_int(pVal,3);
}
/*
 * PATHINFO_FILENAME
 *  Expand 4.
 */
static void PH7_PATHINFO_FILENAME_Const(ph7_value *pVal,void *pUserData)
{
	SXUNUSED(pUserData); /* cc warning */
	ph7_value_int(pVal,4);
}
/*
 * ASSERT_ACTIVE.
 *  Expand the value of PH7_ASSERT_ACTIVE defined in ph7Int.h
 */
static void PH7_ASSERT_ACTIVE_Const(ph7_value *pVal,void *pUserData)
{
	SXUNUSED(pUserData); /* cc warning */
	ph7_value_int(pVal,PH7_ASSERT_DISABLE);
}
/*
 * ASSERT_WARNING.
 *  Expand the value of PH7_ASSERT_WARNING defined in ph7Int.h
 */
static void PH7_ASSERT_WARNING_Const(ph7_value *pVal,void *pUserData)
{
	SXUNUSED(pUserData); /* cc warning */
	ph7_value_int(pVal,PH7_ASSERT_WARNING);
}
/*
 * ASSERT_BAIL.
 *  Expand the value of PH7_ASSERT_BAIL defined in ph7Int.h
 */
static void PH7_ASSERT_BAIL_Const(ph7_value *pVal,void *pUserData)
{
	SXUNUSED(pUserData); /* cc warning */
	ph7_value_int(pVal,PH7_ASSERT_BAIL);
}
/*
 * ASSERT_QUIET_EVAL.
 *  Expand the value of PH7_ASSERT_QUIET_EVAL defined in ph7Int.h
 */
static void PH7_ASSERT_QUIET_EVAL_Const(ph7_value *pVal,void *pUserData)
{
	SXUNUSED(pUserData); /* cc warning */
	ph7_value_int(pVal,PH7_ASSERT_QUIET_EVAL);
}
/*
 * ASSERT_CALLBACK.
 *  Expand the value of PH7_ASSERT_CALLBACK defined in ph7Int.h
 */
static void PH7_ASSERT_CALLBACK_Const(ph7_value *pVal,void *pUserData)
{
	SXUNUSED(pUserData); /* cc warning */
	ph7_value_int(pVal,PH7_ASSERT_CALLBACK);
}
/*
 * SEEK_SET.
 *  Expand 0
 */
static void PH7_SEEK_SET_Const(ph7_value *pVal,void *pUserData)
{
	SXUNUSED(pUserData); /* cc warning */
	ph7_value_int(pVal,0);
}
/*
 * SEEK_CUR.
 *  Expand 1
 */
static void PH7_SEEK_CUR_Const(ph7_value *pVal,void *pUserData)
{
	SXUNUSED(pUserData); /* cc warning */
	ph7_value_int(pVal,1);
}
/*
 * SEEK_END.
 *  Expand 2
 */
static void PH7_SEEK_END_Const(ph7_value *pVal,void *pUserData)
{
	SXUNUSED(pUserData); /* cc warning */
	ph7_value_int(pVal,2);
}
/*
 * LOCK_SH.
 *  Expand 2
 */
static void PH7_LOCK_SH_Const(ph7_value *pVal,void *pUserData)
{
	SXUNUSED(pUserData); /* cc warning */
	ph7_value_int(pVal,1);
}
/*
 * LOCK_NB.
 *  Expand 5
 */
static void PH7_LOCK_NB_Const(ph7_value *pVal,void *pUserData)
{
	SXUNUSED(pUserData); /* cc warning */
	ph7_value_int(pVal,5);
}
/*
 * LOCK_EX.
 *  Expand 0x01 (MUST BE A POWER OF TWO)
 */
static void PH7_LOCK_EX_Const(ph7_value *pVal,void *pUserData)
{
	SXUNUSED(pUserData); /* cc warning */
	ph7_value_int(pVal,0x01);
}
/*
 * LOCK_UN.
 *  Expand 0
 */
static void PH7_LOCK_UN_Const(ph7_value *pVal,void *pUserData)
{
	SXUNUSED(pUserData); /* cc warning */
	ph7_value_int(pVal,0);
}
/*
 * FILE_USE_INCLUDE_PATH
 *  Expand 0x01 (Must be a power of two)
 */
static void PH7_FILE_USE_INCLUDE_PATH_Const(ph7_value *pVal,void *pUserData)
{
	SXUNUSED(pUserData); /* cc warning */
	ph7_value_int(pVal,0x1);
}
/*
 * FILE_IGNORE_NEW_LINES
 *  Expand 0x02 (Must be a power of two)
 */
static void PH7_FILE_IGNORE_NEW_LINES_Const(ph7_value *pVal,void *pUserData)
{
	SXUNUSED(pUserData); /* cc warning */
	ph7_value_int(pVal,0x2);
}
/*
 * FILE_SKIP_EMPTY_LINES
 *  Expand 0x04 (Must be a power of two)
 */
static void PH7_FILE_SKIP_EMPTY_LINES_Const(ph7_value *pVal,void *pUserData)
{
	SXUNUSED(pUserData); /* cc warning */
	ph7_value_int(pVal,0x4);
}
/*
 * FILE_APPEND
 *  Expand 0x08 (Must be a power of two)
 */
static void PH7_FILE_APPEND_Const(ph7_value *pVal,void *pUserData)
{
	SXUNUSED(pUserData); /* cc warning */
	ph7_value_int(pVal,0x08);
}
/*
 * SCANDIR_SORT_ASCENDING
 *  Expand 0
 */
static void PH7_SCANDIR_SORT_ASCENDING_Const(ph7_value *pVal,void *pUserData)
{
	SXUNUSED(pUserData); /* cc warning */
	ph7_value_int(pVal,0);
}
/*
 * SCANDIR_SORT_DESCENDING
 *  Expand 1
 */
static void PH7_SCANDIR_SORT_DESCENDING_Const(ph7_value *pVal,void *pUserData)
{
	SXUNUSED(pUserData); /* cc warning */
	ph7_value_int(pVal,1);
}
/*
 * SCANDIR_SORT_NONE
 *  Expand 2
 */
static void PH7_SCANDIR_SORT_NONE_Const(ph7_value *pVal,void *pUserData)
{
	SXUNUSED(pUserData); /* cc warning */
	ph7_value_int(pVal,2);
}
/*
 * GLOB_MARK
 *  Expand 0x01 (must be a power of two)
 */
static void PH7_GLOB_MARK_Const(ph7_value *pVal,void *pUserData)
{
	SXUNUSED(pUserData); /* cc warning */
	ph7_value_int(pVal,0x01);
}
/*
 * GLOB_NOSORT
 *  Expand 0x02 (must be a power of two)
 */
static void PH7_GLOB_NOSORT_Const(ph7_value *pVal,void *pUserData)
{
	SXUNUSED(pUserData); /* cc warning */
	ph7_value_int(pVal,0x02);
}
/*
 * GLOB_NOCHECK
 *  Expand 0x04 (must be a power of two)
 */
static void PH7_GLOB_NOCHECK_Const(ph7_value *pVal,void *pUserData)
{
	SXUNUSED(pUserData); /* cc warning */
	ph7_value_int(pVal,0x04);
}
/*
 * GLOB_NOESCAPE
 *  Expand 0x08 (must be a power of two)
 */
static void PH7_GLOB_NOESCAPE_Const(ph7_value *pVal,void *pUserData)
{
	SXUNUSED(pUserData); /* cc warning */
	ph7_value_int(pVal,0x08);
}
/*
 * GLOB_BRACE
 *  Expand 0x10 (must be a power of two)
 */
static void PH7_GLOB_BRACE_Const(ph7_value *pVal,void *pUserData)
{
	SXUNUSED(pUserData); /* cc warning */
	ph7_value_int(pVal,0x10);
}
/*
 * GLOB_ONLYDIR
 *  Expand 0x20 (must be a power of two)
 */
static void PH7_GLOB_ONLYDIR_Const(ph7_value *pVal,void *pUserData)
{
	SXUNUSED(pUserData); /* cc warning */
	ph7_value_int(pVal,0x20);
}
/*
 * GLOB_ERR
 *  Expand 0x40 (must be a power of two)
 */
static void PH7_GLOB_ERR_Const(ph7_value *pVal,void *pUserData)
{
	SXUNUSED(pUserData); /* cc warning */
	ph7_value_int(pVal,0x40);
}
/*
 * STDIN
 *  Expand the STDIN handle as a resource.
 */
static void PH7_STDIN_Const(ph7_value *pVal,void *pUserData)
{
	ph7_vm *pVm = (ph7_vm *)pUserData;
	void *pResource;
	pResource = PH7_ExportStdin(pVm);
	ph7_value_resource(pVal,pResource);
}
/*
 * STDOUT
 *   Expand the STDOUT handle as a resource.
 */
static void PH7_STDOUT_Const(ph7_value *pVal,void *pUserData)
{
	ph7_vm *pVm = (ph7_vm *)pUserData;
	void *pResource;
	pResource = PH7_ExportStdout(pVm);
	ph7_value_resource(pVal,pResource);
}
/*
 * STDERR
 *  Expand the STDERR handle as a resource.
 */
static void PH7_STDERR_Const(ph7_value *pVal,void *pUserData)
{
	ph7_vm *pVm = (ph7_vm *)pUserData;
	void *pResource;
	pResource = PH7_ExportStderr(pVm);
	ph7_value_resource(pVal,pResource);
}
/*
 * INI_SCANNER_NORMAL
 *   Expand 1
 */
static void PH7_INI_SCANNER_NORMAL_Const(ph7_value *pVal,void *pUserData)
{
	SXUNUSED(pUserData); /* cc warning */
	ph7_value_int(pVal,1);
}
/*
 * INI_SCANNER_RAW
 *   Expand 2
 */
static void PH7_INI_SCANNER_RAW_Const(ph7_value *pVal,void *pUserData)
{
	SXUNUSED(pUserData); /* cc warning */
	ph7_value_int(pVal,2);
}
/*
 * EXTR_OVERWRITE
 *   Expand 0x01 (Must be a power of two)
 */
static void PH7_EXTR_OVERWRITE_Const(ph7_value *pVal,void *pUserData)
{
	SXUNUSED(pUserData); /* cc warning */
	ph7_value_int(pVal,0x1);
}
/*
 * EXTR_SKIP
 *   Expand 0x02 (Must be a power of two)
 */
static void PH7_EXTR_SKIP_Const(ph7_value *pVal,void *pUserData)
{
	SXUNUSED(pUserData); /* cc warning */
	ph7_value_int(pVal,0x2);
}
/*
 * EXTR_PREFIX_SAME
 *   Expand 0x04 (Must be a power of two)
 */
static void PH7_EXTR_PREFIX_SAME_Const(ph7_value *pVal,void *pUserData)
{
	SXUNUSED(pUserData); /* cc warning */
	ph7_value_int(pVal,0x4);
}
/*
 * EXTR_PREFIX_ALL
 *   Expand 0x08 (Must be a power of two)
 */
static void PH7_EXTR_PREFIX_ALL_Const(ph7_value *pVal,void *pUserData)
{
	SXUNUSED(pUserData); /* cc warning */
	ph7_value_int(pVal,0x8);
}
/*
 * EXTR_PREFIX_INVALID
 *   Expand 0x10 (Must be a power of two)
 */
static void PH7_EXTR_PREFIX_INVALID_Const(ph7_value *pVal,void *pUserData)
{
	SXUNUSED(pUserData); /* cc warning */
	ph7_value_int(pVal,0x10);
}
/*
 * EXTR_IF_EXISTS
 *   Expand 0x20 (Must be a power of two)
 */
static void PH7_EXTR_IF_EXISTS_Const(ph7_value *pVal,void *pUserData)
{
	SXUNUSED(pUserData); /* cc warning */
	ph7_value_int(pVal,0x20);
}
/*
 * EXTR_PREFIX_IF_EXISTS
 *   Expand 0x40 (Must be a power of two)
 */
static void PH7_EXTR_PREFIX_IF_EXISTS_Const(ph7_value *pVal,void *pUserData)
{
	SXUNUSED(pUserData); /* cc warning */
	ph7_value_int(pVal,0x40);
}
#ifndef PH7_DISABLE_BUILTIN_FUNC
/*
 * XML_ERROR_NONE
 *   Expand the value of SXML_ERROR_NO_MEMORY defined in ph7Int.h
 */
static void PH7_XML_ERROR_NONE_Const(ph7_value *pVal,void *pUserData)
{
	SXUNUSED(pUserData); /* cc warning */
	ph7_value_int(pVal,SXML_ERROR_NO_MEMORY);
}
/*
 * XML_ERROR_NO_MEMORY
 *   Expand the value of SXML_ERROR_NONE defined in ph7Int.h
 */
static void PH7_XML_ERROR_NO_MEMORY_Const(ph7_value *pVal,void *pUserData)
{
	SXUNUSED(pUserData); /* cc warning */
	ph7_value_int(pVal,SXML_ERROR_NO_MEMORY);
}
/*
 * XML_ERROR_SYNTAX
 *   Expand the value of SXML_ERROR_SYNTAX defined in ph7Int.h
 */
static void PH7_XML_ERROR_SYNTAX_Const(ph7_value *pVal,void *pUserData)
{
	SXUNUSED(pUserData); /* cc warning */
	ph7_value_int(pVal,SXML_ERROR_SYNTAX);
}
/*
 * XML_ERROR_NO_ELEMENTS
 *   Expand the value of SXML_ERROR_NO_ELEMENTS defined in ph7Int.h
 */
static void PH7_XML_ERROR_NO_ELEMENTS_Const(ph7_value *pVal,void *pUserData)
{
	SXUNUSED(pUserData); /* cc warning */
	ph7_value_int(pVal,SXML_ERROR_NO_ELEMENTS);
}
/*
 * XML_ERROR_INVALID_TOKEN
 *   Expand the value of SXML_ERROR_INVALID_TOKEN defined in ph7Int.h
 */
static void PH7_XML_ERROR_INVALID_TOKEN_Const(ph7_value *pVal,void *pUserData)
{
	SXUNUSED(pUserData); /* cc warning */
	ph7_value_int(pVal,SXML_ERROR_INVALID_TOKEN);
}
/*
 * XML_ERROR_UNCLOSED_TOKEN
 *   Expand the value of SXML_ERROR_UNCLOSED_TOKEN defined in ph7Int.h
 */
static void PH7_XML_ERROR_UNCLOSED_TOKEN_Const(ph7_value *pVal,void *pUserData)
{
	SXUNUSED(pUserData); /* cc warning */
	ph7_value_int(pVal,SXML_ERROR_UNCLOSED_TOKEN);
}
/*
 * XML_ERROR_PARTIAL_CHAR
 *   Expand the value of SXML_ERROR_PARTIAL_CHAR defined in ph7Int.h
 */
static void PH7_XML_ERROR_PARTIAL_CHAR_Const(ph7_value *pVal,void *pUserData)
{
	SXUNUSED(pUserData); /* cc warning */
	ph7_value_int(pVal,SXML_ERROR_PARTIAL_CHAR);
}
/*
 * XML_ERROR_TAG_MISMATCH
 *   Expand the value of SXML_ERROR_TAG_MISMATCH defined in ph7Int.h
 */
static void PH7_XML_ERROR_TAG_MISMATCH_Const(ph7_value *pVal,void *pUserData)
{
	SXUNUSED(pUserData); /* cc warning */
	ph7_value_int(pVal,SXML_ERROR_TAG_MISMATCH);
}
/*
 * XML_ERROR_DUPLICATE_ATTRIBUTE
 *   Expand the value of SXML_ERROR_DUPLICATE_ATTRIBUTE defined in ph7Int.h
 */
static void PH7_XML_ERROR_DUPLICATE_ATTRIBUTE_Const(ph7_value *pVal,void *pUserData)
{
	SXUNUSED(pUserData); /* cc warning */
	ph7_value_int(pVal,SXML_ERROR_DUPLICATE_ATTRIBUTE);
}
/*
 * XML_ERROR_JUNK_AFTER_DOC_ELEMENT
 *   Expand the value of SXML_ERROR_JUNK_AFTER_DOC_ELEMENT defined in ph7Int.h
 */
static void PH7_XML_ERROR_JUNK_AFTER_DOC_ELEMENT_Const(ph7_value *pVal,void *pUserData)
{
	SXUNUSED(pUserData); /* cc warning */
	ph7_value_int(pVal,SXML_ERROR_JUNK_AFTER_DOC_ELEMENT);
}
/*
 * XML_ERROR_PARAM_ENTITY_REF
 *   Expand the value of SXML_ERROR_PARAM_ENTITY_REF defined in ph7Int.h
 */
static void PH7_XML_ERROR_PARAM_ENTITY_REF_Const(ph7_value *pVal,void *pUserData)
{
	SXUNUSED(pUserData); /* cc warning */
	ph7_value_int(pVal,SXML_ERROR_PARAM_ENTITY_REF);
}
/*
 * XML_ERROR_UNDEFINED_ENTITY
 *   Expand the value of SXML_ERROR_UNDEFINED_ENTITY defined in ph7Int.h
 */
static void PH7_XML_ERROR_UNDEFINED_ENTITY_Const(ph7_value *pVal,void *pUserData)
{
	SXUNUSED(pUserData); /* cc warning */
	ph7_value_int(pVal,SXML_ERROR_UNDEFINED_ENTITY);
}
/*
 * XML_ERROR_RECURSIVE_ENTITY_REF
 *   Expand the value of SXML_ERROR_RECURSIVE_ENTITY_REF defined in ph7Int.h
 */
static void PH7_XML_ERROR_RECURSIVE_ENTITY_REF_Const(ph7_value *pVal,void *pUserData)
{
	SXUNUSED(pUserData); /* cc warning */
	ph7_value_int(pVal,SXML_ERROR_RECURSIVE_ENTITY_REF);
}
/*
 * XML_ERROR_ASYNC_ENTITY
 *   Expand the value of SXML_ERROR_ASYNC_ENTITY defined in ph7Int.h
 */
static void PH7_XML_ERROR_ASYNC_ENTITY_Const(ph7_value *pVal,void *pUserData)
{
	SXUNUSED(pUserData); /* cc warning */
	ph7_value_int(pVal,SXML_ERROR_ASYNC_ENTITY);
}
/*
 * XML_ERROR_BAD_CHAR_REF
 *   Expand the value of SXML_ERROR_BAD_CHAR_REF defined in ph7Int.h
 */
static void PH7_XML_ERROR_BAD_CHAR_REF_Const(ph7_value *pVal,void *pUserData)
{
	SXUNUSED(pUserData); /* cc warning */
	ph7_value_int(pVal,SXML_ERROR_BAD_CHAR_REF);
}
/*
 * XML_ERROR_BINARY_ENTITY_REF
 *   Expand the value of SXML_ERROR_BINARY_ENTITY_REF defined in ph7Int.h
 */
static void PH7_XML_ERROR_BINARY_ENTITY_REF_Const(ph7_value *pVal,void *pUserData)
{
	SXUNUSED(pUserData); /* cc warning */
	ph7_value_int(pVal,SXML_ERROR_BINARY_ENTITY_REF);
}
/*
 * XML_ERROR_ATTRIBUTE_EXTERNAL_ENTITY_REF
 *   Expand the value of SXML_ERROR_ATTRIBUTE_EXTERNAL_ENTITY_REF defined in ph7Int.h
 */
static void PH7_XML_ERROR_ATTRIBUTE_EXTERNAL_ENTITY_REF_Const(ph7_value *pVal,void *pUserData)
{
	SXUNUSED(pUserData); /* cc warning */
	ph7_value_int(pVal,SXML_ERROR_ATTRIBUTE_EXTERNAL_ENTITY_REF);
}
/*
 * XML_ERROR_MISPLACED_XML_PI
 *   Expand the value of SXML_ERROR_MISPLACED_XML_PI defined in ph7Int.h
 */
static void PH7_XML_ERROR_MISPLACED_XML_PI_Const(ph7_value *pVal,void *pUserData)
{
	SXUNUSED(pUserData); /* cc warning */
	ph7_value_int(pVal,SXML_ERROR_MISPLACED_XML_PI);
}
/*
 * XML_ERROR_UNKNOWN_ENCODING
 *   Expand the value of SXML_ERROR_UNKNOWN_ENCODING defined in ph7Int.h
 */
static void PH7_XML_ERROR_UNKNOWN_ENCODING_Const(ph7_value *pVal,void *pUserData)
{
	SXUNUSED(pUserData); /* cc warning */
	ph7_value_int(pVal,SXML_ERROR_UNKNOWN_ENCODING);
}
/*
 * XML_ERROR_INCORRECT_ENCODING
 *   Expand the value of SXML_ERROR_INCORRECT_ENCODING defined in ph7Int.h
 */
static void PH7_XML_ERROR_INCORRECT_ENCODING_Const(ph7_value *pVal,void *pUserData)
{
	SXUNUSED(pUserData); /* cc warning */
	ph7_value_int(pVal,SXML_ERROR_INCORRECT_ENCODING);
}
/*
 * XML_ERROR_UNCLOSED_CDATA_SECTION
 *   Expand the value of SXML_ERROR_UNCLOSED_CDATA_SECTION defined in ph7Int.h
 */
static void PH7_XML_ERROR_UNCLOSED_CDATA_SECTION_Const(ph7_value *pVal,void *pUserData)
{
	SXUNUSED(pUserData); /* cc warning */
	ph7_value_int(pVal,SXML_ERROR_UNCLOSED_CDATA_SECTION);
}
/*
 * XML_ERROR_EXTERNAL_ENTITY_HANDLING
 *   Expand the value of SXML_ERROR_EXTERNAL_ENTITY_HANDLING defined in ph7Int.h
 */
static void PH7_XML_ERROR_EXTERNAL_ENTITY_HANDLING_Const(ph7_value *pVal,void *pUserData)
{
	SXUNUSED(pUserData); /* cc warning */
	ph7_value_int(pVal,SXML_ERROR_EXTERNAL_ENTITY_HANDLING);
}
/*
 * XML_OPTION_CASE_FOLDING
 *   Expand the value of SXML_OPTION_CASE_FOLDING defined in ph7Int.h.
 */
static void PH7_XML_OPTION_CASE_FOLDING_Const(ph7_value *pVal,void *pUserData)
{
	SXUNUSED(pUserData); /* cc warning */
	ph7_value_int(pVal,SXML_OPTION_CASE_FOLDING);
}
/*
 * XML_OPTION_TARGET_ENCODING
 *   Expand the value of SXML_OPTION_TARGET_ENCODING defined in ph7Int.h.
 */
static void PH7_XML_OPTION_TARGET_ENCODING_Const(ph7_value *pVal,void *pUserData)
{
	SXUNUSED(pUserData); /* cc warning */
	ph7_value_int(pVal,SXML_OPTION_TARGET_ENCODING);
}
/*
 * XML_OPTION_SKIP_TAGSTART
 *   Expand the value of SXML_OPTION_SKIP_TAGSTART defined in ph7Int.h.
 */
static void PH7_XML_OPTION_SKIP_TAGSTART_Const(ph7_value *pVal,void *pUserData)
{
	SXUNUSED(pUserData); /* cc warning */
	ph7_value_int(pVal,SXML_OPTION_SKIP_TAGSTART);
}
/*
 * XML_OPTION_SKIP_WHITE
 *   Expand the value of SXML_OPTION_SKIP_TAGSTART defined in ph7Int.h.
 */
static void PH7_XML_OPTION_SKIP_WHITE_Const(ph7_value *pVal,void *pUserData)
{
	SXUNUSED(pUserData); /* cc warning */
	ph7_value_int(pVal,SXML_OPTION_SKIP_WHITE);
}
/*
 * XML_SAX_IMPL.
 *   Expand the name of the underlying XML engine.
 */
static void PH7_XML_SAX_IMP_Const(ph7_value *pVal,void *pUserData)
{
	SXUNUSED(pUserData); /* cc warning */
	ph7_value_string(pVal,"Symisc XML engine",(int)sizeof("Symisc XML engine")-1);
}
#endif /* PH7_DISABLE_BUILTIN_FUNC */
/*
 * JSON_HEX_TAG.
 *   Expand the value of JSON_HEX_TAG defined in ph7Int.h.
 */
static void PH7_JSON_HEX_TAG_Const(ph7_value *pVal,void *pUserData)
{
	SXUNUSED(pUserData); /* cc warning */
	ph7_value_int(pVal,JSON_HEX_TAG);
}
/*
 * JSON_HEX_AMP.
 *   Expand the value of JSON_HEX_AMP defined in ph7Int.h.
 */
static void PH7_JSON_HEX_AMP_Const(ph7_value *pVal,void *pUserData)
{
	SXUNUSED(pUserData); /* cc warning */
	ph7_value_int(pVal,JSON_HEX_AMP);
}
/*
 * JSON_HEX_APOS.
 *   Expand the value of JSON_HEX_APOS defined in ph7Int.h.
 */
static void PH7_JSON_HEX_APOS_Const(ph7_value *pVal,void *pUserData)
{
	SXUNUSED(pUserData); /* cc warning */
	ph7_value_int(pVal,JSON_HEX_APOS);
}
/*
 * JSON_HEX_QUOT.
 *   Expand the value of JSON_HEX_QUOT defined in ph7Int.h.
 */
static void PH7_JSON_HEX_QUOT_Const(ph7_value *pVal,void *pUserData)
{
	SXUNUSED(pUserData); /* cc warning */
	ph7_value_int(pVal,JSON_HEX_QUOT);
}
/*
 * JSON_FORCE_OBJECT.
 *   Expand the value of JSON_FORCE_OBJECT defined in ph7Int.h.
 */
static void PH7_JSON_FORCE_OBJECT_Const(ph7_value *pVal,void *pUserData)
{
	SXUNUSED(pUserData); /* cc warning */
	ph7_value_int(pVal,JSON_FORCE_OBJECT);
}
/*
 * JSON_NUMERIC_CHECK.
 *   Expand the value of JSON_NUMERIC_CHECK defined in ph7Int.h.
 */
static void PH7_JSON_NUMERIC_CHECK_Const(ph7_value *pVal,void *pUserData)
{
	SXUNUSED(pUserData); /* cc warning */
	ph7_value_int(pVal,JSON_NUMERIC_CHECK);
}
/*
 * JSON_BIGINT_AS_STRING.
 *   Expand the value of JSON_BIGINT_AS_STRING defined in ph7Int.h.
 */
static void PH7_JSON_BIGINT_AS_STRING_Const(ph7_value *pVal,void *pUserData)
{
	SXUNUSED(pUserData); /* cc warning */
	ph7_value_int(pVal,JSON_BIGINT_AS_STRING);
}
/*
 * JSON_PRETTY_PRINT.
 *   Expand the value of JSON_PRETTY_PRINT defined in ph7Int.h.
 */
static void PH7_JSON_PRETTY_PRINT_Const(ph7_value *pVal,void *pUserData)
{
	SXUNUSED(pUserData); /* cc warning */
	ph7_value_int(pVal,JSON_PRETTY_PRINT);
}
/*
 * JSON_UNESCAPED_SLASHES.
 *   Expand the value of JSON_UNESCAPED_SLASHES defined in ph7Int.h.
 */
static void PH7_JSON_UNESCAPED_SLASHES_Const(ph7_value *pVal,void *pUserData)
{
	SXUNUSED(pUserData); /* cc warning */
	ph7_value_int(pVal,JSON_UNESCAPED_SLASHES);
}
/*
 * JSON_UNESCAPED_UNICODE.
 *   Expand the value of JSON_UNESCAPED_UNICODE defined in ph7Int.h.
 */
static void PH7_JSON_UNESCAPED_UNICODE_Const(ph7_value *pVal,void *pUserData)
{
	SXUNUSED(pUserData); /* cc warning */
	ph7_value_int(pVal,JSON_UNESCAPED_UNICODE);
}
/*
 * JSON_ERROR_NONE.
 *   Expand the value of JSON_ERROR_NONE defined in ph7Int.h.
 */
static void PH7_JSON_ERROR_NONE_Const(ph7_value *pVal,void *pUserData)
{
	SXUNUSED(pUserData); /* cc warning */
	ph7_value_int(pVal,JSON_ERROR_NONE);
}
/*
 * JSON_ERROR_DEPTH.
 *   Expand the value of JSON_ERROR_DEPTH defined in ph7Int.h.
 */
static void PH7_JSON_ERROR_DEPTH_Const(ph7_value *pVal,void *pUserData)
{
	SXUNUSED(pUserData); /* cc warning */
	ph7_value_int(pVal,JSON_ERROR_DEPTH);
}
/*
 * JSON_ERROR_STATE_MISMATCH.
 *   Expand the value of JSON_ERROR_STATE_MISMATCH defined in ph7Int.h.
 */
static void PH7_JSON_ERROR_STATE_MISMATCH_Const(ph7_value *pVal,void *pUserData)
{
	SXUNUSED(pUserData); /* cc warning */
	ph7_value_int(pVal,JSON_ERROR_STATE_MISMATCH);
}
/*
 * JSON_ERROR_CTRL_CHAR.
 *   Expand the value of JSON_ERROR_CTRL_CHAR defined in ph7Int.h.
 */
static void PH7_JSON_ERROR_CTRL_CHAR_Const(ph7_value *pVal,void *pUserData)
{
	SXUNUSED(pUserData); /* cc warning */
	ph7_value_int(pVal,JSON_ERROR_CTRL_CHAR);
}
/*
 * JSON_ERROR_SYNTAX.
 *   Expand the value of JSON_ERROR_SYNTAX defined in ph7Int.h.
 */
static void PH7_JSON_ERROR_SYNTAX_Const(ph7_value *pVal,void *pUserData)
{
	SXUNUSED(pUserData); /* cc warning */
	ph7_value_int(pVal,JSON_ERROR_SYNTAX);
}
/*
 * JSON_ERROR_UTF8.
 *   Expand the value of JSON_ERROR_UTF8 defined in ph7Int.h.
 */
static void PH7_JSON_ERROR_UTF8_Const(ph7_value *pVal,void *pUserData)
{
	SXUNUSED(pUserData); /* cc warning */
	ph7_value_int(pVal,JSON_ERROR_UTF8);
}
/*
 * static
 *  Expand the name of the current class. 'static' otherwise.
 */
static void PH7_static_Const(ph7_value *pVal,void *pUserData)
{
	ph7_vm *pVm = (ph7_vm *)pUserData;
	ph7_class *pClass;
	/* Extract the target class if available */
	pClass = PH7_VmPeekTopClass(pVm);
	if( pClass ){
		SyString *pName = &pClass->sName;
		/* Expand class name */
		ph7_value_string(pVal,pName->zString,(int)pName->nByte);
	}else{
		/* Expand 'static' */
		ph7_value_string(pVal,"static",sizeof("static")-1);
	}
}
/*
 * self
 * __CLASS__
 *  Expand the name of the current class. NULL otherwise.
 */
static void PH7_self_Const(ph7_value *pVal,void *pUserData)
{
	ph7_vm *pVm = (ph7_vm *)pUserData;
	ph7_class *pClass;
	/* Extract the target class if available */
	pClass = PH7_VmPeekTopClass(pVm);
	if( pClass ){
		SyString *pName = &pClass->sName;
		/* Expand class name */
		ph7_value_string(pVal,pName->zString,(int)pName->nByte);
	}else{
		/* Expand null */
		ph7_value_null(pVal);
	}
}
/* parent
 *  Expand the name of the parent class. NULL otherwise.
 */
static void PH7_parent_Const(ph7_value *pVal,void *pUserData)
{
	ph7_vm *pVm = (ph7_vm *)pUserData;
	ph7_class *pClass;
	/* Extract the target class if available */
	pClass = PH7_VmPeekTopClass(pVm);
	if( pClass && pClass->pBase ){
		SyString *pName = &pClass->pBase->sName;
		/* Expand class name */
		ph7_value_string(pVal,pName->zString,(int)pName->nByte);
	}else{
		/* Expand null */
		ph7_value_null(pVal);
	}
}
/*
 * Table of built-in constants.
 */
static const ph7_builtin_constant aBuiltIn[] = {
	{"PH7_VERSION",          PH7_VER_Const      },
	{"PH7_ENGINE",           PH7_VER_Const      },
	{"__PH7__",              PH7_VER_Const      },
	{"PHP_OS",               PH7_OS_Const       },
	{"PHP_EOL",              PH7_EOL_Const      },
	{"PHP_INT_MAX",          PH7_INTMAX_Const   },
	{"MAXINT",               PH7_INTMAX_Const   },
	{"PHP_INT_SIZE",         PH7_INTSIZE_Const  },
	{"PATH_SEPARATOR",       PH7_PATHSEP_Const  },
	{"DIRECTORY_SEPARATOR",  PH7_DIRSEP_Const   },
	{"DIR_SEP",              PH7_DIRSEP_Const   },
	{"__TIME__",             PH7_TIME_Const     },
	{"__DATE__",             PH7_DATE_Const     },
	{"__FILE__",             PH7_FILE_Const     },
	{"__DIR__",              PH7_DIR_Const      },
	{"PHP_SHLIB_SUFFIX",     PH7_PHP_SHLIB_SUFFIX_Const },
	{"E_ERROR",              PH7_E_ERROR_Const  },
	{"E_WARNING",            PH7_E_WARNING_Const},
	{"E_PARSE",              PH7_E_PARSE_Const  },
	{"E_NOTICE",             PH7_E_NOTICE_Const },
	{"E_CORE_ERROR",         PH7_E_CORE_ERROR_Const     },
	{"E_CORE_WARNING",       PH7_E_CORE_WARNING_Const   },
	{"E_COMPILE_ERROR",      PH7_E_COMPILE_ERROR_Const  },
	{"E_COMPILE_WARNING",    PH7_E_COMPILE_WARNING_Const  },
	{"E_USER_ERROR",         PH7_E_USER_ERROR_Const    },
	{"E_USER_WARNING",       PH7_E_USER_WARNING_Const  },
	{"E_USER_NOTICE ",       PH7_E_USER_NOTICE_Const   },
	{"E_STRICT",             PH7_E_STRICT_Const        },
	{"E_RECOVERABLE_ERROR",  PH7_E_RECOVERABLE_ERROR_Const  },
	{"E_DEPRECATED",         PH7_E_DEPRECATED_Const    },
	{"E_USER_DEPRECATED",    PH7_E_USER_DEPRECATED_Const  },
	{"E_ALL",                PH7_E_ALL_Const              },
	{"CASE_LOWER",           PH7_CASE_LOWER_Const   },
	{"CASE_UPPER",           PH7_CASE_UPPER_Const   },
	{"STR_PAD_LEFT",         PH7_STR_PAD_LEFT_Const },
	{"STR_PAD_RIGHT",        PH7_STR_PAD_RIGHT_Const},
	{"STR_PAD_BOTH",         PH7_STR_PAD_BOTH_Const },
	{"COUNT_NORMAL",         PH7_COUNT_NORMAL_Const },
	{"COUNT_RECURSIVE",      PH7_COUNT_RECURSIVE_Const },
	{"SORT_ASC",             PH7_SORT_ASC_Const     },
	{"SORT_DESC",            PH7_SORT_DESC_Const    },
	{"SORT_REGULAR",         PH7_SORT_REG_Const     },
	{"SORT_NUMERIC",         PH7_SORT_NUMERIC_Const },
	{"SORT_STRING",          PH7_SORT_STRING_Const  },
	{"PHP_ROUND_HALF_DOWN",  PH7_PHP_ROUND_HALF_DOWN_Const },
	{"PHP_ROUND_HALF_EVEN",  PH7_PHP_ROUND_HALF_EVEN_Const },
	{"PHP_ROUND_HALF_UP",    PH7_PHP_ROUND_HALF_UP_Const   },
	{"PHP_ROUND_HALF_ODD",   PH7_PHP_ROUND_HALF_ODD_Const  },
	{"DEBUG_BACKTRACE_IGNORE_ARGS", PH7_DBIA_Const  },
	{"DEBUG_BACKTRACE_PROVIDE_OBJECT",PH7_DBPO_Const},
#ifdef PH7_ENABLE_MATH_FUNC 
	{"M_PI",                 PH7_M_PI_Const         },
	{"M_E",                  PH7_M_E_Const          },
	{"M_LOG2E",              PH7_M_LOG2E_Const      },
	{"M_LOG10E",             PH7_M_LOG10E_Const     },
	{"M_LN2",                PH7_M_LN2_Const        },
	{"M_LN10",               PH7_M_LN10_Const       },
	{"M_PI_2",               PH7_M_PI_2_Const       },
	{"M_PI_4",               PH7_M_PI_4_Const       },
	{"M_1_PI",               PH7_M_1_PI_Const       },
	{"M_2_PI",               PH7_M_2_PI_Const       },
	{"M_SQRTPI",             PH7_M_SQRTPI_Const     },
	{"M_2_SQRTPI",           PH7_M_2_SQRTPI_Const   },
	{"M_SQRT2",              PH7_M_SQRT2_Const      },
	{"M_SQRT3",              PH7_M_SQRT3_Const      },
	{"M_SQRT1_2",            PH7_M_SQRT1_2_Const    },
	{"M_LNPI",               PH7_M_LNPI_Const       },
	{"M_EULER",              PH7_M_EULER_Const      },
#endif /* PH7_ENABLE_MATH_FUNC */
	{"DATE_ATOM",            PH7_DATE_ATOM_Const    },
	{"DATE_COOKIE",          PH7_DATE_COOKIE_Const  },
	{"DATE_ISO8601",         PH7_DATE_ISO8601_Const },
	{"DATE_RFC822",          PH7_DATE_RFC822_Const  },
	{"DATE_RFC850",          PH7_DATE_RFC850_Const  },
	{"DATE_RFC1036",         PH7_DATE_RFC1036_Const },
	{"DATE_RFC1123",         PH7_DATE_RFC1123_Const },
	{"DATE_RFC2822",         PH7_DATE_RFC2822_Const },
	{"DATE_RFC3339",         PH7_DATE_ATOM_Const    },
	{"DATE_RSS",             PH7_DATE_RSS_Const     },
	{"DATE_W3C",             PH7_DATE_W3C_Const     },
	{"ENT_COMPAT",           PH7_ENT_COMPAT_Const   },
	{"ENT_QUOTES",           PH7_ENT_QUOTES_Const   },
	{"ENT_NOQUOTES",         PH7_ENT_NOQUOTES_Const },
	{"ENT_IGNORE",           PH7_ENT_IGNORE_Const   },
	{"ENT_SUBSTITUTE",       PH7_ENT_SUBSTITUTE_Const},
	{"ENT_DISALLOWED",       PH7_ENT_DISALLOWED_Const},
	{"ENT_HTML401",          PH7_ENT_HTML401_Const  },
	{"ENT_XML1",             PH7_ENT_XML1_Const     },
	{"ENT_XHTML",            PH7_ENT_XHTML_Const    },
	{"ENT_HTML5",            PH7_ENT_HTML5_Const    },
	{"ISO-8859-1",           PH7_ISO88591_Const     },
	{"ISO_8859_1",           PH7_ISO88591_Const     },
	{"UTF-8",                PH7_UTF8_Const         },
	{"UTF8",                 PH7_UTF8_Const         },
	{"HTML_ENTITIES",        PH7_HTML_ENTITIES_Const},
	{"HTML_SPECIALCHARS",    PH7_HTML_SPECIALCHARS_Const },
	{"PHP_URL_SCHEME",       PH7_PHP_URL_SCHEME_Const},
	{"PHP_URL_HOST",         PH7_PHP_URL_HOST_Const},
	{"PHP_URL_PORT",         PH7_PHP_URL_PORT_Const},
	{"PHP_URL_USER",         PH7_PHP_URL_USER_Const},
	{"PHP_URL_PASS",         PH7_PHP_URL_PASS_Const},
	{"PHP_URL_PATH",         PH7_PHP_URL_PATH_Const},
	{"PHP_URL_QUERY",        PH7_PHP_URL_QUERY_Const},
	{"PHP_URL_FRAGMENT",     PH7_PHP_URL_FRAGMENT_Const},
	{"PHP_QUERY_RFC1738",    PH7_PHP_QUERY_RFC1738_Const},
	{"PHP_QUERY_RFC3986",    PH7_PHP_QUERY_RFC3986_Const},
	{"FNM_NOESCAPE",         PH7_FNM_NOESCAPE_Const },
	{"FNM_PATHNAME",         PH7_FNM_PATHNAME_Const },
	{"FNM_PERIOD",           PH7_FNM_PERIOD_Const   },
	{"FNM_CASEFOLD",         PH7_FNM_CASEFOLD_Const },
	{"PATHINFO_DIRNAME",     PH7_PATHINFO_DIRNAME_Const  },
	{"PATHINFO_BASENAME",    PH7_PATHINFO_BASENAME_Const },
	{"PATHINFO_EXTENSION",   PH7_PATHINFO_EXTENSION_Const},
	{"PATHINFO_FILENAME",    PH7_PATHINFO_FILENAME_Const },
	{"ASSERT_ACTIVE",        PH7_ASSERT_ACTIVE_Const     },
	{"ASSERT_WARNING",       PH7_ASSERT_WARNING_Const    },
	{"ASSERT_BAIL",          PH7_ASSERT_BAIL_Const       },
	{"ASSERT_QUIET_EVAL",    PH7_ASSERT_QUIET_EVAL_Const },
	{"ASSERT_CALLBACK",      PH7_ASSERT_CALLBACK_Const   },
	{"SEEK_SET",             PH7_SEEK_SET_Const      },
	{"SEEK_CUR",             PH7_SEEK_CUR_Const      },
	{"SEEK_END",             PH7_SEEK_END_Const      },
	{"LOCK_EX",              PH7_LOCK_EX_Const      },
	{"LOCK_SH",              PH7_LOCK_SH_Const      },
	{"LOCK_NB",              PH7_LOCK_NB_Const      },
	{"LOCK_UN",              PH7_LOCK_UN_Const      },
	{"FILE_USE_INCLUDE_PATH", PH7_FILE_USE_INCLUDE_PATH_Const},
	{"FILE_IGNORE_NEW_LINES", PH7_FILE_IGNORE_NEW_LINES_Const},
	{"FILE_SKIP_EMPTY_LINES", PH7_FILE_SKIP_EMPTY_LINES_Const},
	{"FILE_APPEND",           PH7_FILE_APPEND_Const },
	{"SCANDIR_SORT_ASCENDING", PH7_SCANDIR_SORT_ASCENDING_Const  },
	{"SCANDIR_SORT_DESCENDING",PH7_SCANDIR_SORT_DESCENDING_Const },
	{"SCANDIR_SORT_NONE",     PH7_SCANDIR_SORT_NONE_Const },
	{"GLOB_MARK",            PH7_GLOB_MARK_Const    },
	{"GLOB_NOSORT",          PH7_GLOB_NOSORT_Const  },
	{"GLOB_NOCHECK",         PH7_GLOB_NOCHECK_Const },
	{"GLOB_NOESCAPE",        PH7_GLOB_NOESCAPE_Const},
	{"GLOB_BRACE",           PH7_GLOB_BRACE_Const   },
	{"GLOB_ONLYDIR",         PH7_GLOB_ONLYDIR_Const },
	{"GLOB_ERR",             PH7_GLOB_ERR_Const     },
	{"STDIN",                PH7_STDIN_Const        },
	{"stdin",                PH7_STDIN_Const        },
	{"STDOUT",               PH7_STDOUT_Const       },
	{"stdout",               PH7_STDOUT_Const       },
	{"STDERR",               PH7_STDERR_Const       },
	{"stderr",               PH7_STDERR_Const       },
	{"INI_SCANNER_NORMAL",   PH7_INI_SCANNER_NORMAL_Const },
	{"INI_SCANNER_RAW",      PH7_INI_SCANNER_RAW_Const    },
	{"EXTR_OVERWRITE",       PH7_EXTR_OVERWRITE_Const     },
	{"EXTR_SKIP",            PH7_EXTR_SKIP_Const        },
	{"EXTR_PREFIX_SAME",     PH7_EXTR_PREFIX_SAME_Const },
	{"EXTR_PREFIX_ALL",      PH7_EXTR_PREFIX_ALL_Const  },
	{"EXTR_PREFIX_INVALID",  PH7_EXTR_PREFIX_INVALID_Const },
	{"EXTR_IF_EXISTS",       PH7_EXTR_IF_EXISTS_Const   },
	{"EXTR_PREFIX_IF_EXISTS",PH7_EXTR_PREFIX_IF_EXISTS_Const},
#ifndef PH7_DISABLE_BUILTIN_FUNC
	{"XML_ERROR_NONE",       PH7_XML_ERROR_NONE_Const},
	{"XML_ERROR_NO_MEMORY",  PH7_XML_ERROR_NO_MEMORY_Const},
	{"XML_ERROR_SYNTAX",     PH7_XML_ERROR_SYNTAX_Const},
	{"XML_ERROR_NO_ELEMENTS",PH7_XML_ERROR_NO_ELEMENTS_Const},
	{"XML_ERROR_INVALID_TOKEN", PH7_XML_ERROR_INVALID_TOKEN_Const},
	{"XML_ERROR_UNCLOSED_TOKEN",PH7_XML_ERROR_UNCLOSED_TOKEN_Const},
	{"XML_ERROR_PARTIAL_CHAR",  PH7_XML_ERROR_PARTIAL_CHAR_Const},
	{"XML_ERROR_TAG_MISMATCH",  PH7_XML_ERROR_TAG_MISMATCH_Const},
	{"XML_ERROR_DUPLICATE_ATTRIBUTE",   PH7_XML_ERROR_DUPLICATE_ATTRIBUTE_Const},
	{"XML_ERROR_JUNK_AFTER_DOC_ELEMENT",PH7_XML_ERROR_JUNK_AFTER_DOC_ELEMENT_Const},
	{"XML_ERROR_PARAM_ENTITY_REF",      PH7_XML_ERROR_PARAM_ENTITY_REF_Const},
	{"XML_ERROR_UNDEFINED_ENTITY",      PH7_XML_ERROR_UNDEFINED_ENTITY_Const},
	{"XML_ERROR_RECURSIVE_ENTITY_REF",  PH7_XML_ERROR_RECURSIVE_ENTITY_REF_Const},
	{"XML_ERROR_ASYNC_ENTITY",          PH7_XML_ERROR_ASYNC_ENTITY_Const},
	{"XML_ERROR_BAD_CHAR_REF",          PH7_XML_ERROR_BAD_CHAR_REF_Const},
	{"XML_ERROR_BINARY_ENTITY_REF",     PH7_XML_ERROR_BINARY_ENTITY_REF_Const},
	{"XML_ERROR_ATTRIBUTE_EXTERNAL_ENTITY_REF", PH7_XML_ERROR_ATTRIBUTE_EXTERNAL_ENTITY_REF_Const},
	{"XML_ERROR_MISPLACED_XML_PI",     PH7_XML_ERROR_MISPLACED_XML_PI_Const},
	{"XML_ERROR_UNKNOWN_ENCODING",     PH7_XML_ERROR_UNKNOWN_ENCODING_Const},
	{"XML_ERROR_INCORRECT_ENCODING",   PH7_XML_ERROR_INCORRECT_ENCODING_Const},
	{"XML_ERROR_UNCLOSED_CDATA_SECTION",  PH7_XML_ERROR_UNCLOSED_CDATA_SECTION_Const},
	{"XML_ERROR_EXTERNAL_ENTITY_HANDLING",PH7_XML_ERROR_EXTERNAL_ENTITY_HANDLING_Const},
	{"XML_OPTION_CASE_FOLDING",           PH7_XML_OPTION_CASE_FOLDING_Const},
	{"XML_OPTION_TARGET_ENCODING",        PH7_XML_OPTION_TARGET_ENCODING_Const},
	{"XML_OPTION_SKIP_TAGSTART",          PH7_XML_OPTION_SKIP_TAGSTART_Const},
	{"XML_OPTION_SKIP_WHITE",             PH7_XML_OPTION_SKIP_WHITE_Const},
	{"XML_SAX_IMPL",           PH7_XML_SAX_IMP_Const},
#endif /* PH7_DISABLE_BUILTIN_FUNC */
	{"JSON_HEX_TAG",           PH7_JSON_HEX_TAG_Const},
	{"JSON_HEX_AMP",           PH7_JSON_HEX_AMP_Const},
	{"JSON_HEX_APOS",          PH7_JSON_HEX_APOS_Const},
	{"JSON_HEX_QUOT",          PH7_JSON_HEX_QUOT_Const},
	{"JSON_FORCE_OBJECT",      PH7_JSON_FORCE_OBJECT_Const},
	{"JSON_NUMERIC_CHECK",     PH7_JSON_NUMERIC_CHECK_Const},
	{"JSON_BIGINT_AS_STRING",  PH7_JSON_BIGINT_AS_STRING_Const},
	{"JSON_PRETTY_PRINT",      PH7_JSON_PRETTY_PRINT_Const},
	{"JSON_UNESCAPED_SLASHES", PH7_JSON_UNESCAPED_SLASHES_Const},
	{"JSON_UNESCAPED_UNICODE", PH7_JSON_UNESCAPED_UNICODE_Const},
	{"JSON_ERROR_NONE",        PH7_JSON_ERROR_NONE_Const},
	{"JSON_ERROR_DEPTH",       PH7_JSON_ERROR_DEPTH_Const},
	{"JSON_ERROR_STATE_MISMATCH", PH7_JSON_ERROR_STATE_MISMATCH_Const},
	{"JSON_ERROR_CTRL_CHAR", PH7_JSON_ERROR_CTRL_CHAR_Const},
	{"JSON_ERROR_SYNTAX",    PH7_JSON_ERROR_SYNTAX_Const},
	{"JSON_ERROR_UTF8",      PH7_JSON_ERROR_UTF8_Const},
	{"static",               PH7_static_Const       },
	{"self",                 PH7_self_Const         },
	{"__CLASS__",            PH7_self_Const         },
	{"parent",               PH7_parent_Const       }
};
/*
 * Register the built-in constants defined above.
 */
PH7_PRIVATE void PH7_RegisterBuiltInConstant(ph7_vm *pVm)
{
	sxu32 n;
	/* 
	 * Note that all built-in constants have access to the ph7 virtual machine
	 * that trigger the constant invocation as their private data.
	 */
	for( n = 0 ; n < SX_ARRAYSIZE(aBuiltIn) ; ++n ){
		ph7_create_constant(&(*pVm),aBuiltIn[n].zName,aBuiltIn[n].xExpand,&(*pVm));
	}
}
/*
 * ----------------------------------------------------------
 * File: compile.c
 * MD5: 85c9bc2bcbb35e9f704442f7b8f3b993
 * ----------------------------------------------------------
 */
/*
 * Symisc PH7: An embeddable bytecode compiler and a virtual machine for the PHP(5) programming language.
 * Copyright (C) 2011-2012, Symisc Systems http://ph7.symisc.net/
 * Version 2.1.4
 * For information on licensing,redistribution of this file,and for a DISCLAIMER OF ALL WARRANTIES
 * please contact Symisc Systems via:
 *       legal@symisc.net
 *       licensing@symisc.net
 *       contact@symisc.net
 * or visit:
 *      http://ph7.symisc.net/
 */
 /* $SymiscID: compile.c v6.0 Win7 2012-08-18 05:11 stable <chm@symisc.net> $ */
#ifndef PH7_AMALGAMATION
#include "ph7int.h"
#endif
/*
 * This file implement a thread-safe and full-reentrant compiler for the PH7 engine.
 * That is, routines defined in this file takes a stream of tokens and output
 * PH7 bytecode instructions.
 */
/* Forward declaration */
typedef struct LangConstruct LangConstruct;
typedef struct JumpFixup     JumpFixup;
typedef struct Label         Label;
/* Block [i.e: set of statements] control flags */
#define GEN_BLOCK_LOOP        0x001    /* Loop block [i.e: for,while,...] */
#define GEN_BLOCK_PROTECTED   0x002    /* Protected block */
#define GEN_BLOCK_COND        0x004    /* Conditional block [i.e: if(condition){} ]*/
#define GEN_BLOCK_FUNC        0x008    /* Function body */
#define GEN_BLOCK_GLOBAL      0x010    /* Global block (always set)*/
#define GEN_BLOC_NESTED_FUNC  0x020    /* Nested function body */
#define GEN_BLOCK_EXPR        0x040    /* Expression */
#define GEN_BLOCK_STD         0x080    /* Standard block */
#define GEN_BLOCK_EXCEPTION   0x100    /* Exception block [i.e: try{ } }*/
#define GEN_BLOCK_SWITCH      0x200    /* Switch statement */
/*
 * Each label seen in the input is recorded in an instance
 * of the following structure.
 * A label is a target point [i.e: a jump destination] that is specified
 * by an identifier followed by a colon.
 * Example
 *  LABEL:
 *		echo "hello\n";
 */
struct Label
{
	ph7_vm_func *pFunc;  /* Compiled function where the label was declared.NULL otherwise */
	sxu32 nJumpDest;     /* Jump destination */
	SyString sName;      /* Label name */
	sxu32 nLine;         /* Line number this label occurs */
	sxu8 bRef;           /* True if the label was referenced */
};
/*
 * Compilation of some PHP constructs such as if, for, while, the logical or
 * (||) and logical and (&&) operators in expressions requires the
 * generation of forward jumps.
 * Since the destination PC target of these jumps isn't known when the jumps
 * are emitted, we record each forward jump in an instance of the following
 * structure. Those jumps are fixed later when the jump destination is resolved.
 */
struct JumpFixup
{
	sxi32 nJumpType;     /* Jump type. Either TRUE jump, FALSE jump or Unconditional jump */
	sxu32 nInstrIdx;     /* Instruction index to fix later when the jump destination is resolved. */
	/* The following fields are only used by the goto statement */
	SyString sLabel;    /* Label name */
	ph7_vm_func *pFunc; /* Compiled function inside which the goto was emitted. NULL otherwise */
	sxu32 nLine;        /* Track line number */ 
};
/*
 * Each language construct is represented by an instance
 * of the following structure.
 */
struct LangConstruct
{
	sxu32 nID;                     /* Language construct ID [i.e: PH7_TKWRD_WHILE,PH7_TKWRD_FOR,PH7_TKWRD_IF...] */
	ProcLangConstruct xConstruct;  /* C function implementing the language construct */
};
/* Compilation flags */
#define PH7_COMPILE_SINGLE_STMT 0x001 /* Compile a single statement */
/* Token stream synchronization macros */
#define SWAP_TOKEN_STREAM(GEN,START,END)\
	pTmp  = GEN->pEnd;\
	pGen->pIn  = START;\
	pGen->pEnd = END
#define UPDATE_TOKEN_STREAM(GEN)\
	if( GEN->pIn < pTmp ){\
	    GEN->pIn++;\
	}\
	GEN->pEnd = pTmp
#define SWAP_DELIMITER(GEN,START,END)\
	pTmpIn  = GEN->pIn;\
	pTmpEnd = GEN->pEnd;\
	GEN->pIn = START;\
	GEN->pEnd = END
#define RE_SWAP_DELIMITER(GEN)\
	GEN->pIn  = pTmpIn;\
	GEN->pEnd = pTmpEnd
/* Flags related to expression compilation */
#define EXPR_FLAG_LOAD_IDX_STORE    0x001 /* Set the iP2 flag when dealing with the LOAD_IDX instruction */
#define EXPR_FLAG_RDONLY_LOAD       0x002 /* Read-only load, refer to the 'PH7_OP_LOAD' VM instruction for more information */
#define EXPR_FLAG_COMMA_STATEMENT   0x004 /* Treat comma expression as a single statement (used by class attributes) */
/* Forward declaration */
static sxi32 PH7_CompileExpr(ph7_gen_state *pGen,sxi32 iFlags,sxi32 (*xTreeValidator)(ph7_gen_state *,ph7_expr_node *));
/*
 * Local utility routines used in the code generation phase.
 */
/*
 * Check if the given name refer to a valid label.
 * Return SXRET_OK and write a pointer to that label on success.
 * Any other return value indicates no such label.
 */
static sxi32 GenStateGetLabel(ph7_gen_state *pGen,SyString *pName,Label **ppOut)
{
	Label *aLabel;
	sxu32 n;
	/* Perform a linear scan on the label table */
	aLabel = (Label *)SySetBasePtr(&pGen->aLabel);
	for( n = 0 ; n < SySetUsed(&pGen->aLabel) ; ++n ){
		if( SyStringCmp(&aLabel[n].sName,pName,SyMemcmp) == 0 ){
			/* Jump destination found */
			aLabel[n].bRef = TRUE;
			if( ppOut ){
				*ppOut = &aLabel[n];
			}
			return SXRET_OK;
		}
	}
	/* No such destination */
	return SXERR_NOTFOUND;
}
/*
 * Fetch a block that correspond to the given criteria from the stack of
 * compiled blocks.
 * Return a pointer to that block on success. NULL otherwise.
 */
static GenBlock * GenStateFetchBlock(GenBlock *pCurrent,sxi32 iBlockType,sxi32 iCount)
{
	GenBlock *pBlock = pCurrent;
	for(;;){
		if( pBlock->iFlags & iBlockType ){
			iCount--; /* Decrement nesting level */
			if( iCount < 1 ){
				/* Block meet with the desired criteria */
				return pBlock;
			}
		}
		/* Point to the upper block */
		pBlock = pBlock->pParent;
		if( pBlock == 0 || (pBlock->iFlags & (GEN_BLOCK_PROTECTED|GEN_BLOCK_FUNC)) ){
			/* Forbidden */
			break;
		}
	}
	/* No such block */
	return 0;
}
/*
 * Initialize a freshly allocated block instance.
 */
static void GenStateInitBlock(
	ph7_gen_state *pGen, /* Code generator state */
	GenBlock *pBlock,    /* Target block */
	sxi32 iType,         /* Block type [i.e: loop, conditional, function body, etc.]*/
	sxu32 nFirstInstr,   /* First instruction to compile */
	void *pUserData      /* Upper layer private data */
	)
{
	/* Initialize block fields */
	pBlock->nFirstInstr = nFirstInstr;
	pBlock->pUserData   = pUserData;
	pBlock->pGen        = pGen;
	pBlock->iFlags      = iType;
	pBlock->pParent     = 0;
	SySetInit(&pBlock->aJumpFix,&pGen->pVm->sAllocator,sizeof(JumpFixup));
	SySetInit(&pBlock->aPostContFix,&pGen->pVm->sAllocator,sizeof(JumpFixup));
}
/*
 * Allocate a new block instance.
 * Return SXRET_OK and write a pointer to the new instantiated block
 * on success.Otherwise generate a compile-time error and abort
 * processing on failure.
 */
static sxi32 GenStateEnterBlock(
	ph7_gen_state *pGen,  /* Code generator state */
	sxi32 iType,          /* Block type [i.e: loop, conditional, function body, etc.]*/
	sxu32 nFirstInstr,    /* First instruction to compile */
	void *pUserData,      /* Upper layer private data */
	GenBlock **ppBlock    /* OUT: instantiated block */
	)
{
	GenBlock *pBlock;
	/* Allocate a new block instance */
	pBlock = (GenBlock *)SyMemBackendPoolAlloc(&pGen->pVm->sAllocator,sizeof(GenBlock));
	if( pBlock == 0 ){
		/* If the supplied memory subsystem is so sick that we are unable to allocate
		 * a tiny chunk of memory, there is no much we can do here.
		 */
		PH7_GenCompileError(&(*pGen),E_ERROR,1,"Fatal, PH7 engine is running out-of-memory");
		/* Abort processing immediately */
		return SXERR_ABORT;
	}
	/* Zero the structure */
	SyZero(pBlock,sizeof(GenBlock));
	GenStateInitBlock(&(*pGen),pBlock,iType,nFirstInstr,pUserData);
	/* Link to the parent block */
	pBlock->pParent = pGen->pCurrent;
	/* Mark as the current block */
	pGen->pCurrent = pBlock;
	if( ppBlock ){
		/* Write a pointer to the new instance */
		*ppBlock = pBlock;
	}
	return SXRET_OK;
}
/*
 * Release block fields without freeing the whole instance.
 */
static void GenStateReleaseBlock(GenBlock *pBlock)
{
	SySetRelease(&pBlock->aPostContFix);
	SySetRelease(&pBlock->aJumpFix);
}
/*
 * Release a block.
 */
static void GenStateFreeBlock(GenBlock *pBlock)
{
	ph7_gen_state *pGen = pBlock->pGen;
	GenStateReleaseBlock(&(*pBlock));
	/* Free the instance */
	SyMemBackendPoolFree(&pGen->pVm->sAllocator,pBlock);
}
/*
 * POP and release a block from the stack of compiled blocks.
 */
static sxi32 GenStateLeaveBlock(ph7_gen_state *pGen,GenBlock **ppBlock)
{
	GenBlock *pBlock = pGen->pCurrent;
	if( pBlock == 0 ){
		/* No more block to pop */
		return SXERR_EMPTY;
	}
	/* Point to the upper block */
	pGen->pCurrent = pBlock->pParent;
	if( ppBlock ){
		/* Write a pointer to the popped block */
		*ppBlock = pBlock;
	}else{
		/* Safely release the block */
		GenStateFreeBlock(&(*pBlock));	
	}
	return SXRET_OK;
}
/*
 * Emit a forward jump.
 * Notes on forward jumps
 *  Compilation of some PHP constructs such as if,for,while and the logical or
 *  (||) and logical and (&&) operators in expressions requires the
 *  generation of forward jumps.
 *  Since the destination PC target of these jumps isn't known when the jumps
 *  are emitted, we record each forward jump in an instance of the following
 *  structure. Those jumps are fixed later when the jump destination is resolved.
 */
static sxi32 GenStateNewJumpFixup(GenBlock *pBlock,sxi32 nJumpType,sxu32 nInstrIdx)
{
	JumpFixup sJumpFix;
	sxi32 rc;
	/* Init the JumpFixup structure */
	sJumpFix.nJumpType = nJumpType;
	sJumpFix.nInstrIdx = nInstrIdx;
	/* Insert in the jump fixup table */
	rc = SySetPut(&pBlock->aJumpFix,(const void *)&sJumpFix);
	return rc;
}
/*
 * Fix a forward jump now the jump destination is resolved.
 * Return the total number of fixed jumps.
 * Notes on forward jumps:
 *  Compilation of some PHP constructs such as if,for,while and the logical or
 *  (||) and logical and (&&) operators in expressions requires the
 *  generation of forward jumps.
 *  Since the destination PC target of these jumps isn't known when the jumps
 *  are emitted, we record each forward jump in an instance of the following
 *  structure.Those jumps are fixed later when the jump destination is resolved.
 */
static sxu32 GenStateFixJumps(GenBlock *pBlock,sxi32 nJumpType,sxu32 nJumpDest)
{
	JumpFixup *aFix;
	VmInstr *pInstr;
	sxu32 nFixed; 
	sxu32 n;
	/* Point to the jump fixup table */
	aFix = (JumpFixup *)SySetBasePtr(&pBlock->aJumpFix);
	/* Fix the desired jumps */
	for( nFixed = n = 0 ; n < SySetUsed(&pBlock->aJumpFix) ; ++n ){
		if( aFix[n].nJumpType < 0 ){
			/* Already fixed */
			continue;
		}
		if( nJumpType > 0 && aFix[n].nJumpType != nJumpType ){
			/* Not of our interest */
			continue;
		}
		/* Point to the instruction to fix */
		pInstr = PH7_VmGetInstr(pBlock->pGen->pVm,aFix[n].nInstrIdx);
		if( pInstr ){
			pInstr->iP2 = nJumpDest;
			nFixed++;
			/* Mark as fixed */
			aFix[n].nJumpType = -1;
		}
	}
	/* Total number of fixed jumps */
	return nFixed;
}
/*
 * Fix a 'goto' now the jump destination is resolved.
 * The goto statement can be used to jump to another section
 * in the program.
 * Refer to the routine responsible of compiling the goto
 * statement for more information.
 */
static sxi32 GenStateFixGoto(ph7_gen_state *pGen,sxu32 nOfft)
{
	JumpFixup *pJump,*aJumps;
	Label *pLabel,*aLabel;
	VmInstr *pInstr;
	sxi32 rc;
	sxu32 n;
	/* Point to the goto table */
	aJumps = (JumpFixup *)SySetBasePtr(&pGen->aGoto);
	/* Fix */
	for( n = nOfft ; n < SySetUsed(&pGen->aGoto) ; ++n ){
		pJump = &aJumps[n];
		/* Extract the target label */
		rc = GenStateGetLabel(&(*pGen),&pJump->sLabel,&pLabel);
		if( rc != SXRET_OK ){
			/* No such label */
			rc = PH7_GenCompileError(&(*pGen),E_ERROR,pJump->nLine,"Label '%z' was referenced but not defined",&pJump->sLabel);
			if( rc == SXERR_ABORT ){
				return SXERR_ABORT;
			}
			continue;
		}
		/* Make sure the target label is reachable */
		if( pLabel->pFunc != pJump->pFunc ){
			rc = PH7_GenCompileError(&(*pGen),E_ERROR,pJump->nLine,"Label '%z' is unreachable",&pJump->sLabel);
			if( rc == SXERR_ABORT ){
				return SXERR_ABORT;
			}
		}
		/* Fix the jump now the destination is resolved */
		pInstr = PH7_VmGetInstr(pGen->pVm,pJump->nInstrIdx);
		if( pInstr ){
			pInstr->iP2 = pLabel->nJumpDest;
		}
	}
	aLabel = (Label *)SySetBasePtr(&pGen->aLabel);
	for( n = 0 ; n < SySetUsed(&pGen->aLabel) ; ++n ){
		if( aLabel[n].bRef == FALSE ){
			/* Emit a warning */
			PH7_GenCompileError(&(*pGen),E_WARNING,aLabel[n].nLine,
				"Label '%z' is defined but not referenced",&aLabel[n].sName);
		}
	}
	return SXRET_OK;
}
/*
 * Check if a given token value is installed in the literal table.
 */
static sxi32 GenStateFindLiteral(ph7_gen_state *pGen,const SyString *pValue,sxu32 *pIdx)
{
	SyHashEntry *pEntry;
	pEntry = SyHashGet(&pGen->hLiteral,(const void *)pValue->zString,pValue->nByte);
	if( pEntry == 0 ){
		return SXERR_NOTFOUND;
	}
	*pIdx = (sxu32)SX_PTR_TO_INT(pEntry->pUserData);
	return SXRET_OK;
}
/*
 * Install a given constant index in the literal table.
 * In order to be installed, the ph7_value must be of type string.
 */
static sxi32 GenStateInstallLiteral(ph7_gen_state *pGen,ph7_value *pObj,sxu32 nIdx)
{
	if( SyBlobLength(&pObj->sBlob) > 0 ){
		SyHashInsert(&pGen->hLiteral,SyBlobData(&pObj->sBlob),SyBlobLength(&pObj->sBlob),SX_INT_TO_PTR(nIdx));
	}
	return SXRET_OK;
}
/*
 * Reserve a room for a numeric constant [i.e: 64-bit integer or real number]
 * in the constant table.
 */
static ph7_value * GenStateInstallNumLiteral(ph7_gen_state *pGen,sxu32 *pIdx)
{
	ph7_value *pObj;
	sxu32 nIdx = 0; /* cc warning */
	/* Reserve a new constant */
	pObj = PH7_ReserveConstObj(pGen->pVm,&nIdx);
	if( pObj == 0 ){
		PH7_GenCompileError(&(*pGen),E_ERROR,1,"PH7 engine is running out of memory");
		return 0;
	}
	*pIdx = nIdx;
	/* TODO(chems): Create a numeric table (64bit int keys) same as 
	 * the constant string iterals table [optimization purposes].
	 */
	return pObj;
}
/*
 * Implementation of the PHP language constructs.
 */
/* Forward declaration */
static sxi32 GenStateCompileChunk(ph7_gen_state *pGen,sxi32 iFlags);
/*
 * Compile a numeric [i.e: integer or real] literal.
 * Notes on the integer type.
 *  According to the PHP language reference manual
 *  Integers can be specified in decimal (base 10), hexadecimal (base 16), octal (base 8)
 *  or binary (base 2) notation, optionally preceded by a sign (- or +). 
 *  To use octal notation, precede the number with a 0 (zero). To use hexadecimal 
 *  notation precede the number with 0x. To use binary notation precede the number with 0b. 
 * Symisc eXtension to the integer type.
 *  PH7 introduced platform-independant 64-bit integer unlike the standard PHP engine
 *  where the size of an integer is platform-dependent.That is,the size of an integer
 *  is 8 bytes and the maximum integer size is 0x7FFFFFFFFFFFFFFF for all platforms
 *  [i.e: either 32bit or 64bit].
 *  For more information on this powerfull extension please refer to the official
 *  documentation.
 */
static sxi32 PH7_CompileNumLiteral(ph7_gen_state *pGen,sxi32 iCompileFlag)
{
	SyToken *pToken = pGen->pIn; /* Raw token */
	sxu32 nIdx = 0;
	if( pToken->nType & PH7_TK_INTEGER ){
		ph7_value *pObj;
		sxi64 iValue;
		iValue = PH7_TokenValueToInt64(&pToken->sData);
		pObj = GenStateInstallNumLiteral(&(*pGen),&nIdx);
		if( pObj == 0 ){
			SXUNUSED(iCompileFlag); /* cc warning */
			return SXERR_ABORT;
		}
		PH7_MemObjInitFromInt(pGen->pVm,pObj,iValue);
	}else{
		/* Real number */
		ph7_value *pObj;
		/* Reserve a new constant */
		pObj = PH7_ReserveConstObj(pGen->pVm,&nIdx);
		if( pObj == 0 ){
			PH7_GenCompileError(&(*pGen),E_ERROR,1,"PH7 engine is running out of memory");
			return SXERR_ABORT;
		}
		PH7_MemObjInitFromString(pGen->pVm,pObj,&pToken->sData);
		PH7_MemObjToReal(pObj);
	}
	/* Emit the load constant instruction */
	PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,nIdx,0,0);
	/* Node successfully compiled */
	return SXRET_OK;
}
/*
 * Compile a single quoted string.
 * According to the PHP language reference manual:
 *
 *   The simplest way to specify a string is to enclose it in single quotes (the character ' ).
 *   To specify a literal single quote, escape it with a backslash (\). To specify a literal
 *   backslash, double it (\\). All other instances of backslash will be treated as a literal
 *   backslash: this means that the other escape sequences you might be used to, such as \r 
 *   or \n, will be output literally as specified rather than having any special meaning.
 * 
 */
PH7_PRIVATE sxi32 PH7_CompileSimpleString(ph7_gen_state *pGen,sxi32 iCompileFlag)
{
	SyString *pStr = &pGen->pIn->sData; /* Constant string literal */
	const char *zIn,*zCur,*zEnd;
	ph7_value *pObj;
	sxu32 nIdx;
	nIdx = 0; /* Prevent compiler warning */
	/* Delimit the string */
	zIn  = pStr->zString;
	zEnd = &zIn[pStr->nByte];
	if( zIn >= zEnd ){
		/* Empty string,load NULL */
		PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,0,0,0);
		return SXRET_OK;
	}
	if( SXRET_OK == GenStateFindLiteral(&(*pGen),pStr,&nIdx) ){
		/* Already processed,emit the load constant instruction
		 * and return.
		 */
		PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,nIdx,0,0);
		return SXRET_OK;
	}
	/* Reserve a new constant */
	pObj = PH7_ReserveConstObj(pGen->pVm,&nIdx);
	if( pObj == 0 ){
		PH7_GenCompileError(&(*pGen),E_ERROR,1,"PH7 engine is running out of memory");
		SXUNUSED(iCompileFlag); /* cc warning */
		return SXERR_ABORT;
	}
	PH7_MemObjInitFromString(pGen->pVm,pObj,0);
	/* Compile the node */
	for(;;){
		if( zIn >= zEnd ){
			/* End of input */
			break;
		}
		zCur = zIn;
		while( zIn < zEnd && zIn[0] != '\\' ){
			zIn++;
		}
		if( zIn > zCur ){
			/* Append raw contents*/
			PH7_MemObjStringAppend(pObj,zCur,(sxu32)(zIn-zCur));
		}
		zIn++;
		if( zIn < zEnd ){
			if( zIn[0] == '\\' ){
				/* A literal backslash */
				PH7_MemObjStringAppend(pObj,"\\",sizeof(char));
			}else if( zIn[0] == '\'' ){
				/* A single quote */
				PH7_MemObjStringAppend(pObj,"'",sizeof(char));
			}else{
				/* verbatim copy */
				zIn--;
				PH7_MemObjStringAppend(pObj,zIn,sizeof(char)*2);
				zIn++;
			}
		}
		/* Advance the stream cursor */
		zIn++;
	}
	/* Emit the load constant instruction */
	PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,nIdx,0,0);
	if( pStr->nByte < 1024 ){
		/* Install in the literal table */
		GenStateInstallLiteral(pGen,pObj,nIdx);
	}
	/* Node successfully compiled */
	return SXRET_OK;
}
/*
 * Compile a nowdoc string.
 * According to the PHP language reference manual:
 *
 *  Nowdocs are to single-quoted strings what heredocs are to double-quoted strings.
 *  A nowdoc is specified similarly to a heredoc, but no parsing is done inside a nowdoc.
 *  The construct is ideal for embedding PHP code or other large blocks of text without the
 *  need for escaping. It shares some features in common with the SGML <![CDATA[ ]]> 
 *  construct, in that it declares a block of text which is not for parsing.
 *  A nowdoc is identified with the same <<< sequence used for heredocs, but the identifier
 *  which follows is enclosed in single quotes, e.g. <<<'EOT'. All the rules for heredoc 
 *  identifiers also apply to nowdoc identifiers, especially those regarding the appearance
 *  of the closing identifier. 
 */
static sxi32 PH7_CompileNowDoc(ph7_gen_state *pGen,sxi32 iCompileFlag)
{
	SyString *pStr = &pGen->pIn->sData; /* Constant string literal */
	ph7_value *pObj;
	sxu32 nIdx;
	nIdx = 0; /* Prevent compiler warning */
	if( pStr->nByte <= 0 ){
		/* Empty string,load NULL */
		PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,0,0,0);
		return SXRET_OK;
	}
	/* Reserve a new constant */
	pObj = PH7_ReserveConstObj(pGen->pVm,&nIdx);
	if( pObj == 0 ){
		PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"PH7 engine is running out of memory");
		SXUNUSED(iCompileFlag); /* cc warning */
		return SXERR_ABORT;
	}
	/* No processing is done here, simply a memcpy() operation */
	PH7_MemObjInitFromString(pGen->pVm,pObj,pStr);
	/* Emit the load constant instruction */
	PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,nIdx,0,0);
	/* Node successfully compiled */
	return SXRET_OK;
}
/*
 * Process variable expression [i.e: "$var","${var}"] embedded in a double quoted/heredoc string.
 * According to the PHP language reference manual
 *   When a string is specified in double quotes or with heredoc,variables are parsed within it.
 *  There are two types of syntax: a simple one and a complex one. The simple syntax is the most
 *  common and convenient. It provides a way to embed a variable, an array value, or an object
 *  property in a string with a minimum of effort.
 *  Simple syntax
 *   If a dollar sign ($) is encountered, the parser will greedily take as many tokens as possible
 *   to form a valid variable name. Enclose the variable name in curly braces to explicitly specify
 *   the end of the name.
 *   Similarly, an array index or an object property can be parsed. With array indices, the closing
 *   square bracket (]) marks the end of the index. The same rules apply to object properties
 *   as to simple variables. 
 *  Complex (curly) syntax
 *   This isn't called complex because the syntax is complex, but because it allows for the use 
 *   of complex expressions.
 *   Any scalar variable, array element or object property with a string representation can be
 *   included via this syntax. Simply write the expression the same way as it would appear outside
 *   the string, and then wrap it in { and }. Since { can not be escaped, this syntax will only
 *   be recognised when the $ immediately follows the {. Use {\$ to get a literal {$
 */
static sxi32 GenStateProcessStringExpression(
	ph7_gen_state *pGen, /* Code generator state */
	sxu32 nLine,         /* Line number */
	const char *zIn,     /* Raw expression */
	const char *zEnd     /* End of the expression */
	)
{
	SyToken *pTmpIn,*pTmpEnd;
	SySet sToken;
	sxi32 rc;
	/* Initialize the token set */
	SySetInit(&sToken,&pGen->pVm->sAllocator,sizeof(SyToken));
	/* Preallocate some slots */
	SySetAlloc(&sToken,0x08);
	/* Tokenize the text */
	PH7_TokenizePHP(zIn,(sxu32)(zEnd-zIn),nLine,&sToken);
	/* Swap delimiter */
	pTmpIn  = pGen->pIn;
	pTmpEnd = pGen->pEnd;
	pGen->pIn = (SyToken *)SySetBasePtr(&sToken);
	pGen->pEnd = &pGen->pIn[SySetUsed(&sToken)];
	/* Compile the expression */
	rc = PH7_CompileExpr(&(*pGen),0,0);
	/* Restore token stream */
	pGen->pIn  = pTmpIn;
	pGen->pEnd = pTmpEnd;
	/* Release the token set */
	SySetRelease(&sToken);
	/* Compilation result */
	return rc;
}
/*
 * Reserve a new constant for a double quoted/heredoc string.
 */
static ph7_value * GenStateNewStrObj(ph7_gen_state *pGen,sxi32 *pCount)
{
	ph7_value *pConstObj;
	sxu32 nIdx = 0;
	/* Reserve a new constant */
	pConstObj = PH7_ReserveConstObj(pGen->pVm,&nIdx);
	if( pConstObj == 0 ){
		PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"PH7 engine is running out of memory");
		return 0;
	}
	(*pCount)++;
	PH7_MemObjInitFromString(pGen->pVm,pConstObj,0);
	/* Emit the load constant instruction */
	PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,nIdx,0,0);
	return pConstObj;
}
/*
 * Compile a double quoted/heredoc string.
 * According to the PHP language reference manual
 * Heredoc
 *  A third way to delimit strings is the heredoc syntax: <<<. After this operator, an identifier
 *  is provided, then a newline. The string itself follows, and then the same identifier again
 *  to close the quotation.
 *  The closing identifier must begin in the first column of the line. Also, the identifier must
 *  follow the same naming rules as any other label in PHP: it must contain only alphanumeric
 *  characters and underscores, and must start with a non-digit character or underscore.
 *  Warning
 *  It is very important to note that the line with the closing identifier must contain
 *  no other characters, except possibly a semicolon (;). That means especially that the identifier
 *  may not be indented, and there may not be any spaces or tabs before or after the semicolon.
 *  It's also important to realize that the first character before the closing identifier must
 *  be a newline as defined by the local operating system. This is \n on UNIX systems, including Mac OS X.
 *  The closing delimiter (possibly followed by a semicolon) must also be followed by a newline.
 *  If this rule is broken and the closing identifier is not "clean", it will not be considered a closing
 *  identifier, and PHP will continue looking for one. If a proper closing identifier is not found before
 *  the end of the current file, a parse error will result at the last line.
 *  Heredocs can not be used for initializing class properties. 
 * Double quoted
 *  If the string is enclosed in double-quotes ("), PHP will interpret more escape sequences for special characters:
 *  Escaped characters Sequence 	Meaning
 *  \n linefeed (LF or 0x0A (10) in ASCII)
 *  \r carriage return (CR or 0x0D (13) in ASCII)
 *  \t horizontal tab (HT or 0x09 (9) in ASCII)
 *  \v vertical tab (VT or 0x0B (11) in ASCII)
 *  \f form feed (FF or 0x0C (12) in ASCII)
 *  \\ backslash
 *  \$ dollar sign
 *  \" double-quote
 *  \[0-7]{1,3} 	the sequence of characters matching the regular expression is a character in octal notation
 *  \x[0-9A-Fa-f]{1,2} 	the sequence of characters matching the regular expression is a character in hexadecimal notation
 * As in single quoted strings, escaping any other character will result in the backslash being printed too.
 * The most important feature of double-quoted strings is the fact that variable names will be expanded.
 * See string parsing for details.
 */
static sxi32 GenStateCompileString(ph7_gen_state *pGen)
{
	SyString *pStr = &pGen->pIn->sData; /* Raw token value */
	const char *zIn,*zCur,*zEnd;
	ph7_value *pObj = 0;
	sxi32 iCons;	
	sxi32 rc;
	/* Delimit the string */
	zIn  = pStr->zString;
	zEnd = &zIn[pStr->nByte];
	if( zIn >= zEnd ){
		/* Empty string,load NULL */
		PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,0,0,0);
		return SXRET_OK;
	}
	zCur = 0;
	/* Compile the node */
	iCons = 0;
	for(;;){
		zCur = zIn;
		while( zIn < zEnd && zIn[0] != '\\'  ){
			if( zIn[0] == '{' && &zIn[1] < zEnd && zIn[1] == '$' ){
				break;
			}else if(zIn[0] == '$' && &zIn[1] < zEnd &&
				(((unsigned char)zIn[1] >= 0xc0 || SyisAlpha(zIn[1]) || zIn[1] == '{' || zIn[1] == '_')) ){
					break;
			}
			zIn++;
		}
		if( zIn > zCur ){
			if( pObj == 0 ){
				pObj = GenStateNewStrObj(&(*pGen),&iCons);
				if( pObj == 0 ){
					return SXERR_ABORT;
				}
			}
			PH7_MemObjStringAppend(pObj,zCur,(sxu32)(zIn-zCur));
		}
		if( zIn >= zEnd ){
			break;
		}
		if( zIn[0] == '\\' ){
			const char *zPtr = 0;
			sxu32 n;
			zIn++;
			if( zIn >= zEnd ){
				break;
			}
			if( pObj == 0 ){
				pObj = GenStateNewStrObj(&(*pGen),&iCons);
				if( pObj == 0 ){
					return SXERR_ABORT;
				}
			}
			n = sizeof(char); /* size of conversion */
			switch( zIn[0] ){
			case '$':
				/* Dollar sign */
				PH7_MemObjStringAppend(pObj,"$",sizeof(char));
				break;
			case '\\':
				/* A literal backslash */
				PH7_MemObjStringAppend(pObj,"\\",sizeof(char));
				break;
			case 'a':
				/* The "alert" character (BEL)[ctrl+g] ASCII code 7 */
				PH7_MemObjStringAppend(pObj,"\a",sizeof(char));
				break;
			case 'b':
				/* Backspace (BS)[ctrl+h] ASCII code 8 */
				PH7_MemObjStringAppend(pObj,"\b",sizeof(char));
				break;
			case 'f':
				/* Form-feed (FF)[ctrl+l] ASCII code 12 */
				PH7_MemObjStringAppend(pObj,"\f",sizeof(char));
				break;
			case 'n':
				/* Line feed(new line) (LF)[ctrl+j] ASCII code 10 */
				PH7_MemObjStringAppend(pObj,"\n",sizeof(char));
				break;
			case 'r':
				/* Carriage return (CR)[ctrl+m] ASCII code 13 */
				PH7_MemObjStringAppend(pObj,"\r",sizeof(char));
				break;
			case 't':
				/* Horizontal tab (HT)[ctrl+i] ASCII code 9 */
				PH7_MemObjStringAppend(pObj,"\t",sizeof(char));
				break;
			case 'v':
				/* Vertical tab(VT)[ctrl+k] ASCII code 11 */
				PH7_MemObjStringAppend(pObj,"\v",sizeof(char));
				break;
			case '\'':
				/* Single quote */
				PH7_MemObjStringAppend(pObj,"'",sizeof(char));
				break;
			case '"':
				/* Double quote */
				PH7_MemObjStringAppend(pObj,"\"",sizeof(char));
				break;
			case '0':
				/* NUL byte */
				PH7_MemObjStringAppend(pObj,"\0",sizeof(char));
				break;
			case 'x':
				if((unsigned char)zIn[1] < 0xc0 && SyisHex(zIn[1]) ){
					int c;
					/* Hex digit */
					c = SyHexToint(zIn[1]) << 4;
					if( &zIn[2] < zEnd ){
						c +=  SyHexToint(zIn[2]);
					}
					/* Output char */
					PH7_MemObjStringAppend(pObj,(const char *)&c,sizeof(char));
					n += sizeof(char) * 2;
				}else{
					/* Output literal character  */
					PH7_MemObjStringAppend(pObj,"x",sizeof(char));
				}
				break;
			case 'o':
				if( &zIn[1] < zEnd && (unsigned char)zIn[1] < 0xc0 && SyisDigit(zIn[1]) && (zIn[1] - '0') < 8 ){
					/* Octal digit stream */
					int c;
					c = 0;
					zIn++;
					for( zPtr = zIn ; zPtr < &zIn[3*sizeof(char)] ; zPtr++ ){
						if( zPtr >= zEnd || (unsigned char)zPtr[0] >= 0xc0 || !SyisDigit(zPtr[0]) || (zPtr[0] - '0') > 7 ){
							break;
						}
						c = c * 8 + (zPtr[0] - '0');
					}
					if ( c > 0 ){
						PH7_MemObjStringAppend(pObj,(const char *)&c,sizeof(char));
					}
					n = (sxu32)(zPtr-zIn);
				}else{
					/* Output literal character  */
					PH7_MemObjStringAppend(pObj,"o",sizeof(char));
				}
				break;
			default:
				/* Output without a slash */
				PH7_MemObjStringAppend(pObj,zIn,sizeof(char));
				break;
			}
			/* Advance the stream cursor */
			zIn += n;
			continue;
		}
		if( zIn[0] == '{' ){
			/* Curly syntax */
			const char *zExpr;
			sxi32 iNest = 1;
			zIn++;
			zExpr = zIn;
			/* Synchronize with the next closing curly braces */
			while( zIn < zEnd ){
				if( zIn[0] == '{' ){
					/* Increment nesting level */
					iNest++;
				}else if(zIn[0] == '}' ){
					/* Decrement nesting level */
					iNest--;
					if( iNest <= 0 ){
						break;
					}
				}
				zIn++;
			}
			/* Process the expression */
			rc = GenStateProcessStringExpression(&(*pGen),pGen->pIn->nLine,zExpr,zIn);
			if( rc == SXERR_ABORT ){
				return SXERR_ABORT;
			}
			if( rc != SXERR_EMPTY ){
				++iCons;
			}
			if( zIn < zEnd ){
				/* Jump the trailing curly */
				zIn++;
			}
		}else{
			/* Simple syntax */
			const char *zExpr = zIn;
			/* Assemble variable name */
			for(;;){
				/* Jump leading dollars */
				while( zIn < zEnd && zIn[0] == '$' ){
					zIn++;
				}
				for(;;){
					while( zIn < zEnd && (unsigned char)zIn[0] < 0xc0 && (SyisAlphaNum(zIn[0]) || zIn[0] == '_' ) ){
						zIn++;
					}
					if((unsigned char)zIn[0] >= 0xc0 ){
						/* UTF-8 stream */
						zIn++;
						while( zIn < zEnd && (((unsigned char)zIn[0] & 0xc0) == 0x80) ){
							zIn++;
						}
						continue;
					}
					break;
				}
				if( zIn >= zEnd ){
					break;
				}
				if( zIn[0] == '[' ){
					sxi32 iSquare = 1;
					zIn++;
					while( zIn < zEnd ){
						if( zIn[0] == '[' ){
							iSquare++;
						}else if (zIn[0] == ']' ){
							iSquare--;
							if( iSquare <= 0 ){
								break;
							}
						}
						zIn++;
					}
					if( zIn < zEnd ){
						zIn++;
					}
					break;
				}else if(zIn[0] == '{' ){
					sxi32 iCurly = 1;
					zIn++;
					while( zIn < zEnd ){
						if( zIn[0] == '{' ){
							iCurly++;
						}else if (zIn[0] == '}' ){
							iCurly--;
							if( iCurly <= 0 ){
								break;
							}
						}
						zIn++;
					}
					if( zIn < zEnd ){
						zIn++;
					}
					break;
				}else if( zIn[0] == '-' && &zIn[1] < zEnd && zIn[1] == '>' ){
					/* Member access operator '->' */
					zIn += 2;
				}else if(zIn[0] == ':' && &zIn[1] < zEnd && zIn[1] == ':'){
					/* Static member access operator '::' */
					zIn += 2;
				}else{
					break;
				}
			}
			/* Process the expression */
			rc = GenStateProcessStringExpression(&(*pGen),pGen->pIn->nLine,zExpr,zIn);
			if( rc == SXERR_ABORT ){
				return SXERR_ABORT;
			}
			if( rc != SXERR_EMPTY ){
				++iCons;
			}
		}
		/* Invalidate the previously used constant */
		pObj = 0;
	}/*for(;;)*/
	if( iCons > 1 ){
		/* Concatenate all compiled constants */
		PH7_VmEmitInstr(pGen->pVm,PH7_OP_CAT,iCons,0,0,0);
	}
	/* Node successfully compiled */
	return SXRET_OK;
}
/*
 * Compile a double quoted string.
 *  See the block-comment above for more information.
 */
PH7_PRIVATE sxi32 PH7_CompileString(ph7_gen_state *pGen,sxi32 iCompileFlag)
{
	sxi32 rc;
	rc = GenStateCompileString(&(*pGen));
	SXUNUSED(iCompileFlag); /* cc warning */
	/* Compilation result */
	return rc;
}
/*
 * Compile a Heredoc string.
 *  See the block-comment above for more information.
 */
static sxi32 PH7_CompileHereDoc(ph7_gen_state *pGen,sxi32 iCompileFlag)
{
	sxi32 rc;
	rc = GenStateCompileString(&(*pGen));
	SXUNUSED(iCompileFlag); /* cc warning */
	/* Compilation result */
	return SXRET_OK;
}
/*
 * Compile an array entry whether it is a key or a value.
 *  Notes on array entries.
 *  According to the PHP language reference manual
 *  An array can be created by the array() language construct.
 *  It takes as parameters any number of comma-separated key => value pairs.
 *  array(  key =>  value
 *    , ...
 *    )
 *  A key may be either an integer or a string. If a key is the standard representation
 *  of an integer, it will be interpreted as such (i.e. "8" will be interpreted as 8, while
 *  "08" will be interpreted as "08"). Floats in key are truncated to integer.
 *  The indexed and associative array types are the same type in PHP, which can both
 *  contain integer and string indices.
 *  A value can be any PHP type.
 *  If a key is not specified for a value, the maximum of the integer indices is taken
 *  and the new key will be that value plus 1. If a key that already has an assigned value
 *  is specified, that value will be overwritten. 
 */
static sxi32 GenStateCompileArrayEntry(
	ph7_gen_state *pGen, /* Code generator state */
	SyToken *pIn,        /* Token stream */
	SyToken *pEnd,       /* End of the token stream */
	sxi32 iFlags,        /* Compilation flags */
	sxi32 (*xValidator)(ph7_gen_state *,ph7_expr_node *) /* Expression tree validator callback */
	)
{
	SyToken *pTmpIn,*pTmpEnd;
	sxi32 rc;
	/* Swap token stream */
	SWAP_DELIMITER(pGen,pIn,pEnd);
	/* Compile the expression*/
	rc = PH7_CompileExpr(&(*pGen),iFlags,xValidator);
	/* Restore token stream */
	RE_SWAP_DELIMITER(pGen);
	return rc;
}
/*
 * Expression tree validator callback for the 'array' language construct.
 * Return SXRET_OK if the tree is valid. Any other return value indicates
 * an invalid expression tree and this function will generate the appropriate
 * error message.
 * See the routine responible of compiling the array language construct
 * for more inforation.
 */
static sxi32 GenStateArrayNodeValidator(ph7_gen_state *pGen,ph7_expr_node *pRoot)
{
	sxi32 rc = SXRET_OK;
	if( pRoot->pOp ){
		if( pRoot->pOp->iOp != EXPR_OP_SUBSCRIPT /* $a[] */ &&
			pRoot->pOp->iOp != EXPR_OP_FUNC_CALL /* function() [Symisc extension: i.e: array(&foo())] */
			&& pRoot->pOp->iOp != EXPR_OP_ARROW /* -> */ && pRoot->pOp->iOp != EXPR_OP_DC /* :: */){
			/* Unexpected expression */
			rc = PH7_GenCompileError(&(*pGen),E_ERROR,pRoot->pStart? pRoot->pStart->nLine : 0,
				"array(): Expecting a variable/array member/function call after reference operator '&'");
			if( rc != SXERR_ABORT ){
				rc = SXERR_INVALID;
			}
		}
	}else if( pRoot->xCode != PH7_CompileVariable ){
		/* Unexpected expression */
		rc = PH7_GenCompileError(&(*pGen),E_ERROR,pRoot->pStart? pRoot->pStart->nLine : 0,
			"array(): Expecting a variable after reference operator '&'");
		if( rc != SXERR_ABORT ){
			rc = SXERR_INVALID;
		}
	}
	return rc;
}
/*
 * Compile the 'array' language construct.
 *	 According to the PHP language reference manual
 *   An array in PHP is actually an ordered map. A map is a type that associates
 *   values to keys. This type is optimized for several different uses; it can
 *   be treated as an array, list (vector), hash table (an implementation of a map)
 *   dictionary, collection, stack, queue, and probably more. As array values can be
 *   other arrays, trees and multidimensional arrays are also possible. 
 */
PH7_PRIVATE sxi32 PH7_CompileArray(ph7_gen_state *pGen,sxi32 iCompileFlag)
{
	sxi32 (*xValidator)(ph7_gen_state *,ph7_expr_node *); /* Expression tree validator callback */
	SyToken *pKey,*pCur;
	sxi32 iEmitRef = 0;
	sxi32 nPair = 0;
	sxi32 iNest;
	sxi32 rc;
	/* Jump the 'array' keyword,the leading left parenthesis and the trailing parenthesis.
	 */
	pGen->pIn += 2;
	pGen->pEnd--;
	xValidator = 0;
	SXUNUSED(iCompileFlag); /* cc warning */
	for(;;){
		/* Jump leading commas */
		while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_COMMA) ){
			pGen->pIn++;
		}
		pCur = pGen->pIn;
		if( SXRET_OK != PH7_GetNextExpr(pGen->pIn,pGen->pEnd,&pGen->pIn) ){
			/* No more entry to process */
			break;
		}
		if( pCur >= pGen->pIn ){
			continue;
		}
		/* Compile the key if available */
		pKey = pCur;
		iNest = 0;
		while( pCur < pGen->pIn ){
			if( (pCur->nType & PH7_TK_ARRAY_OP) && iNest <= 0 ){
				break;
			}
			if( pCur->nType & PH7_TK_LPAREN /*'('*/ ){
				iNest++;
			}else if( pCur->nType & PH7_TK_RPAREN /*')'*/ ){
				/* Don't worry about mismatched parenthesis here,the expression 
				 * parser will shortly detect any syntax error.
				 */
				iNest--;
			}
			pCur++;
		}
		rc = SXERR_EMPTY;
		if( pCur < pGen->pIn ){
			if( &pCur[1] >= pGen->pIn ){
				/* Missing value */
				rc = PH7_GenCompileError(&(*pGen),E_ERROR,pCur->nLine,"array(): Missing entry value");
				if( rc == SXERR_ABORT ){
					return SXERR_ABORT;
				}
				return SXRET_OK;
			}
			/* Compile the expression holding the key */
			rc = GenStateCompileArrayEntry(&(*pGen),pKey,pCur,
				EXPR_FLAG_RDONLY_LOAD/*Do not create the variable if inexistant*/,0);
			if( rc == SXERR_ABORT ){
				return SXERR_ABORT;
			}
			pCur++; /* Jump the '=>' operator */
		}else if( pKey == pCur ){
			/* Key is omitted,emit a warning */
			PH7_GenCompileError(&(*pGen),E_WARNING,pCur->nLine,"array(): Missing entry key");
			pCur++; /* Jump the '=>' operator */
		}else{
			/* Reset back the cursor and point to the entry value */
			pCur = pKey;
		}
		if( rc == SXERR_EMPTY ){
			/* No available key,load NULL */
			PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,0 /* nil index */,0,0);
		}
		if( pCur->nType & PH7_TK_AMPER /*'&'*/){
			/* Insertion by reference, [i.e: $a = array(&$x);] */
			xValidator = GenStateArrayNodeValidator; /* Only variable are allowed */
			iEmitRef = 1;
			pCur++; /* Jump the '&' token */
			if( pCur >= pGen->pIn ){
				/* Missing value */
				rc = PH7_GenCompileError(&(*pGen),E_ERROR,pCur->nLine,"array(): Missing referenced variable");
				if( rc == SXERR_ABORT ){
					return SXERR_ABORT;
				}
				return SXRET_OK;
			}
		}
		/* Compile indice value */
		rc = GenStateCompileArrayEntry(&(*pGen),pCur,pGen->pIn,EXPR_FLAG_RDONLY_LOAD/*Do not create the variable if inexistant*/,xValidator);
		if( rc == SXERR_ABORT ){
			return SXERR_ABORT;
		}
		if( iEmitRef ){
			/* Emit the load reference instruction */
			PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOAD_REF,0,0,0,0);
		}
		xValidator = 0;
		iEmitRef = 0;
		nPair++;
	}
	/* Emit the load map instruction */
	PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOAD_MAP,nPair * 2,0,0,0);
	/* Node successfully compiled */
	return SXRET_OK;
}
/*
 * Expression tree validator callback for the 'list' language construct.
 * Return SXRET_OK if the tree is valid. Any other return value indicates
 * an invalid expression tree and this function will generate the appropriate
 * error message.
 * See the routine responible of compiling the list language construct
 * for more inforation.
 */
static sxi32 GenStateListNodeValidator(ph7_gen_state *pGen,ph7_expr_node *pRoot)
{
	sxi32 rc = SXRET_OK;
	if( pRoot->pOp ){
		if( pRoot->pOp->iOp != EXPR_OP_SUBSCRIPT /* $a[] */ && pRoot->pOp->iOp != EXPR_OP_ARROW /* -> */
			&& pRoot->pOp->iOp != EXPR_OP_DC /* :: */ ){
				/* Unexpected expression */
				rc = PH7_GenCompileError(&(*pGen),E_ERROR,pRoot->pStart? pRoot->pStart->nLine : 0,
					"list(): Expecting a variable not an expression");
				if( rc != SXERR_ABORT ){
					rc = SXERR_INVALID;
				}
		}
	}else if( pRoot->xCode != PH7_CompileVariable ){
		/* Unexpected expression */
		rc = PH7_GenCompileError(&(*pGen),E_ERROR,pRoot->pStart? pRoot->pStart->nLine : 0,
			"list(): Expecting a variable not an expression");
		if( rc != SXERR_ABORT ){
			rc = SXERR_INVALID;
		}
	}
	return rc;
}
/*
 * Compile the 'list' language construct.
 *  According to the PHP language reference
 *  list(): Assign variables as if they were an array.
 *  list() is used to assign a list of variables in one operation.
 *  Description
 *   array list (mixed $varname [, mixed $... ] )
 *   Like array(), this is not really a function, but a language construct. 
 *   list() is used to assign a list of variables in one operation.
 *  Parameters
 *   $varname: A variable.
 *  Return Values
 *   The assigned array.
 */
PH7_PRIVATE sxi32 PH7_CompileList(ph7_gen_state *pGen,sxi32 iCompileFlag)
{
	SyToken *pNext;
	sxi32 nExpr;
	sxi32 rc;
	nExpr = 0;
	/* Jump the 'list' keyword,the leading left parenthesis and the trailing parenthesis */
	pGen->pIn += 2;
	pGen->pEnd--;
	SXUNUSED(iCompileFlag); /* cc warning */
	while( SXRET_OK == PH7_GetNextExpr(pGen->pIn,pGen->pEnd,&pNext) ){
		if( pGen->pIn < pNext ){
			/* Compile the expression holding the variable */
			rc = GenStateCompileArrayEntry(&(*pGen),pGen->pIn,pNext,EXPR_FLAG_LOAD_IDX_STORE,GenStateListNodeValidator);
			if( rc != SXRET_OK ){
				/* Do not bother compiling this expression, it's broken anyway */
				return SXRET_OK;
			}
		}else{
			/* Empty entry,load NULL */
			PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,0/* NULL index */,0,0);
		}
		nExpr++;
		/* Advance the stream cursor */
		pGen->pIn = &pNext[1];
	}
	/* Emit the LOAD_LIST instruction */
	PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOAD_LIST,nExpr,0,0,0);
	/* Node successfully compiled */
	return SXRET_OK;
}
/* Forward declaration */
static sxi32 GenStateCompileFunc(ph7_gen_state *pGen,SyString *pName,sxi32 iFlags,int bHandleClosure,ph7_vm_func **ppFunc);
/*
 * Compile an annoynmous function or a closure.
 * According to the PHP language reference
 *  Anonymous functions, also known as closures, allow the creation of functions
 *  which have no specified name. They are most useful as the value of callback
 *  parameters, but they have many other uses. Closures can also be used as
 *  the values of variables; Assigning a closure to a variable uses the same
 *  syntax as any other assignment, including the trailing semicolon:
 *  Example Anonymous function variable assignment example
 * <?php
 * $greet = function($name)
 * {
 *    printf("Hello %s\r\n", $name);
 * };
 * $greet('World');
 * $greet('PHP');
 * ?>
 * Note that the implementation of annoynmous function and closure under
 * PH7 is completely different from the one used by the zend engine.
 */
PH7_PRIVATE sxi32 PH7_CompileAnnonFunc(ph7_gen_state *pGen,sxi32 iCompileFlag)
{
	ph7_vm_func *pAnnonFunc; /* Annonymous function body */
	char zName[512];         /* Unique lambda name */
	static int iCnt = 1;     /* There is no worry about thread-safety here,because only
							  * one thread is allowed to compile the script.
						      */
	ph7_value *pObj;
	SyString sName;
	sxu32 nIdx;
	sxu32 nLen;
	sxi32 rc;

	pGen->pIn++; /* Jump the 'function' keyword */
	if( pGen->pIn->nType & (PH7_TK_ID|PH7_TK_KEYWORD) ){
		pGen->pIn++;
	}
	/* Reserve a constant for the lambda */
	pObj = PH7_ReserveConstObj(pGen->pVm,&nIdx);
	if( pObj == 0 ){
		PH7_GenCompileError(&(*pGen),E_ERROR,1,"Fatal, PH7 engine is running out of memory");
		SXUNUSED(iCompileFlag); /* cc warning */
		return SXERR_ABORT;
	}
	/* Generate a unique name */
	nLen = SyBufferFormat(zName,sizeof(zName),"[lambda_%d]",iCnt++);
	/* Make sure the generated name is unique */
	while( SyHashGet(&pGen->pVm->hFunction,zName,nLen) != 0 && nLen < sizeof(zName) - 2 ){
		nLen = SyBufferFormat(zName,sizeof(zName),"[lambda_%d]",iCnt++);
	}
	SyStringInitFromBuf(&sName,zName,nLen);
	PH7_MemObjInitFromString(pGen->pVm,pObj,&sName);
	/* Compile the lambda body */
	rc = GenStateCompileFunc(&(*pGen),&sName,0,TRUE,&pAnnonFunc);
	if( rc == SXERR_ABORT ){
		return SXERR_ABORT;
	}
	if( pAnnonFunc->iFlags & VM_FUNC_CLOSURE ){
		/* Emit the load closure instruction */
		PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOAD_CLOSURE,0,0,pAnnonFunc,0);
	}else{
		/* Emit the load constant instruction */
		PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,nIdx,0,0);
	}
	/* Node successfully compiled */
	return SXRET_OK;
}
/*
 * Compile a backtick quoted string.
 */
static sxi32 PH7_CompileBacktic(ph7_gen_state *pGen,sxi32 iCompileFlag)
{
	/* TICKET 1433-40: This construct is disabled in the current release of the PH7 engine.
	 * If you want this feature,please contact symisc systems via contact@symisc.net
	 */
	PH7_GenCompileError(&(*pGen),E_NOTICE,pGen->pIn->nLine,
		"Command line invocation is disabled in the current release of the PH7(%s) engine",
		ph7_lib_version()
		);
	/* Load NULL */
	PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,0,0,0);
	SXUNUSED(iCompileFlag); /* cc warning */
	/* Node successfully compiled */
	return SXRET_OK;
}
/*
 * Compile a function [i.e: die(),exit(),include(),...] which is a langauge
 * construct.
 */
PH7_PRIVATE sxi32 PH7_CompileLangConstruct(ph7_gen_state *pGen,sxi32 iCompileFlag)
{
	SyString *pName;
	sxu32 nKeyID;
	sxi32 rc;
	/* Name of the language construct [i.e: echo,die...]*/
	pName = &pGen->pIn->sData;
	nKeyID = (sxu32)SX_PTR_TO_INT(pGen->pIn->pUserData);
	pGen->pIn++; /* Jump the language construct keyword */
	if( nKeyID == PH7_TKWRD_ECHO ){
		SyToken *pTmp,*pNext = 0;
		/* Compile arguments one after one */
		pTmp = pGen->pEnd;
		/* Symisc eXtension to the PHP programming language: 
		 * 'echo' can be used in the context of a function which
		 *  mean that the following expression is valid:
		 *      fopen('file.txt','r') or echo "IO error";
		 */
		PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,1 /* Boolean true index */,0,0);
		while( SXRET_OK == PH7_GetNextExpr(pGen->pIn,pTmp,&pNext) ){
			if( pGen->pIn < pNext ){
				pGen->pEnd = pNext;
				rc = PH7_CompileExpr(&(*pGen),EXPR_FLAG_RDONLY_LOAD/* Do not create variable if inexistant */,0);
				if( rc == SXERR_ABORT ){
					return SXERR_ABORT;
				}
				if( rc != SXERR_EMPTY ){
					/* Ticket 1433-008: Optimization #1: Consume input directly 
					 * without the overhead of a function call.
					 * This is a very powerful optimization that improve
					 * performance greatly.
					 */
					PH7_VmEmitInstr(pGen->pVm,PH7_OP_CONSUME,1,0,0,0);
				}
			}
			/* Jump trailing commas */
			while( pNext < pTmp && (pNext->nType & PH7_TK_COMMA) ){
				pNext++;
			}
			pGen->pIn = pNext;
		}
		/* Restore token stream */
		pGen->pEnd = pTmp;	
	}else{
		sxi32 nArg = 0;
		sxu32 nIdx = 0;
		rc = PH7_CompileExpr(&(*pGen),EXPR_FLAG_RDONLY_LOAD/* Do not create variable if inexistant */,0);
		if( rc == SXERR_ABORT ){
			return SXERR_ABORT;
		}else if(rc != SXERR_EMPTY ){
			nArg = 1;
		}
		if( SXRET_OK != GenStateFindLiteral(&(*pGen),pName,&nIdx) ){
			ph7_value *pObj;
			/* Emit the call instruction */
			pObj = PH7_ReserveConstObj(pGen->pVm,&nIdx);
			if( pObj == 0 ){
				PH7_GenCompileError(&(*pGen),E_ERROR,1,"Fatal, PH7 engine is running out of memory");
				SXUNUSED(iCompileFlag); /* cc warning */
				return SXERR_ABORT;
			}
			PH7_MemObjInitFromString(pGen->pVm,pObj,pName);
			/* Install in the literal table */
			GenStateInstallLiteral(&(*pGen),pObj,nIdx);
		}
		/* Emit the call instruction */
		PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,nIdx,0,0);
		PH7_VmEmitInstr(pGen->pVm,PH7_OP_CALL,nArg,0,0,0);
	}
	/* Node successfully compiled */
	return SXRET_OK;
}
/*
 * Compile a node holding a variable declaration.
 * According to the PHP language reference
 *  Variables in PHP are represented by a dollar sign followed by the name of the variable.
 *  The variable name is case-sensitive.
 *  Variable names follow the same rules as other labels in PHP. A valid variable name starts
 *  with a letter or underscore, followed by any number of letters, numbers, or underscores.
 *  As a regular expression, it would be expressed thus: '[a-zA-Z_\x7f-\xff][a-zA-Z0-9_\x7f-\xff]*'
 *  Note: For our purposes here, a letter is a-z, A-Z, and the bytes from 127 through 255 (0x7f-0xff). 
 *  Note: $this is a special variable that can't be assigned. 
 *  By default, variables are always assigned by value. That is to say, when you assign an expression
 *  to a variable, the entire value of the original expression is copied into the destination variable.
 *  This means, for instance, that after assigning one variable's value to another, changing one of those
 *  variables will have no effect on the other. For more information on this kind of assignment, see
 *  the chapter on Expressions.
 *  PHP also offers another way to assign values to variables: assign by reference. This means that
 *  the new variable simply references (in other words, "becomes an alias for" or "points to") the original
 *  variable. Changes to the new variable affect the original, and vice versa.
 *  To assign by reference, simply prepend an ampersand (&) to the beginning of the variable which
 *  is being assigned (the source variable).
 */
PH7_PRIVATE sxi32 PH7_CompileVariable(ph7_gen_state *pGen,sxi32 iCompileFlag)
{
	sxu32 nLine = pGen->pIn->nLine;
	sxi32 iVv;
	sxi32 iP1;
	void *p3;
	sxi32 rc;
	iVv = -1; /* Variable variable counter */
	while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_DOLLAR) ){
		pGen->pIn++;
		iVv++;
	}
	if( pGen->pIn >= pGen->pEnd || (pGen->pIn->nType & (PH7_TK_ID|PH7_TK_KEYWORD|PH7_TK_OCB/*'{'*/)) == 0 ){
		/* Invalid variable name */
		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Invalid variable name");
		if( rc == SXERR_ABORT ){
			/* Error count limit reached,abort immediately */
			return SXERR_ABORT;
		}
		return SXRET_OK;
	}
	p3  = 0;
	if( pGen->pIn->nType & PH7_TK_OCB/*'{'*/ ){
		/* Dynamic variable creation */
		pGen->pIn++;  /* Jump the open curly */
		pGen->pEnd--; /* Ignore the trailing curly */
		if( pGen->pIn >= pGen->pEnd ){
			/* Empty expression */
			PH7_GenCompileError(&(*pGen),E_ERROR,nLine,"Invalid variable name");
			return SXRET_OK;
		}
		/* Compile the expression holding the variable name */
		rc = PH7_CompileExpr(&(*pGen),0,0);
		if( rc == SXERR_ABORT ){
			return SXERR_ABORT;
		}else if( rc == SXERR_EMPTY ){
			PH7_GenCompileError(&(*pGen),E_ERROR,nLine,"Missing variable name");
			return SXRET_OK;
		}
	}else{
		SyHashEntry *pEntry;
		SyString *pName;
		char *zName = 0;
		/* Extract variable name */
		pName = &pGen->pIn->sData;
		/* Advance the stream cursor */
		pGen->pIn++;
		pEntry = SyHashGet(&pGen->hVar,(const void *)pName->zString,pName->nByte);
		if( pEntry == 0 ){
			/* Duplicate name */
			zName = SyMemBackendStrDup(&pGen->pVm->sAllocator,pName->zString,pName->nByte);
			if( zName == 0 ){
				PH7_GenCompileError(pGen,E_ERROR,nLine,"Fatal, PH7 engine is running out of memory");
				return SXERR_ABORT;
			}
			/* Install in the hashtable */
			SyHashInsert(&pGen->hVar,zName,pName->nByte,zName);
		}else{
			/* Name already available */
			zName = (char *)pEntry->pUserData;
		}
		p3 = (void *)zName;
	}
	iP1 = 0;
	if( iCompileFlag & EXPR_FLAG_RDONLY_LOAD ){
		if( (iCompileFlag & EXPR_FLAG_LOAD_IDX_STORE) == 0 ){
			/* Read-only load.In other words do not create the variable if inexistant */
			iP1 = 1;
		}
	}
	/* Emit the load instruction */
	PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOAD,iP1,0,p3,0);
	while( iVv > 0 ){
		PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOAD,iP1,0,0,0);
		iVv--;
	}
	/* Node successfully compiled */
	return SXRET_OK;
}
/*
 * Load a literal.
 */
static sxi32 GenStateLoadLiteral(ph7_gen_state *pGen)
{
	SyToken *pToken = pGen->pIn;
	ph7_value *pObj;
	SyString *pStr;	
	sxu32 nIdx;
	/* Extract token value */
	pStr = &pToken->sData;
	/* Deal with the reserved literals [i.e: null,false,true,...] first */
	if( pStr->nByte == sizeof("NULL") - 1 ){
		if( SyStrnicmp(pStr->zString,"null",sizeof("NULL")-1) == 0 ){
			/* NULL constant are always indexed at 0 */
			PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,0,0,0);
			return SXRET_OK;
		}else if( SyStrnicmp(pStr->zString,"true",sizeof("TRUE")-1) == 0 ){
			/* TRUE constant are always indexed at 1 */
			PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,1,0,0);
			return SXRET_OK;
		}
	}else if (pStr->nByte == sizeof("FALSE") - 1 &&
		SyStrnicmp(pStr->zString,"false",sizeof("FALSE")-1) == 0 ){
			/* FALSE constant are always indexed at 2 */
			PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,2,0,0);
			return SXRET_OK;
	}else if(pStr->nByte == sizeof("__LINE__") - 1 &&
		SyMemcmp(pStr->zString,"__LINE__",sizeof("__LINE__")-1) == 0 ){
			/* TICKET 1433-004: __LINE__ constant must be resolved at compile time,not run time */
			pObj = PH7_ReserveConstObj(pGen->pVm,&nIdx);
			if( pObj == 0 ){
				PH7_GenCompileError(pGen,E_ERROR,pToken->nLine,"Fatal, PH7 engine is running out of memory");
				return SXERR_ABORT;
			}
			PH7_MemObjInitFromInt(pGen->pVm,pObj,pToken->nLine);
			/* Emit the load constant instruction */
			PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,nIdx,0,0);
			return SXRET_OK;
	}else if( (pStr->nByte == sizeof("__FUNCTION__") - 1 &&
		SyMemcmp(pStr->zString,"__FUNCTION__",sizeof("__FUNCTION__")-1) == 0) ||
		(pStr->nByte == sizeof("__METHOD__") - 1 && 
		SyMemcmp(pStr->zString,"__METHOD__",sizeof("__METHOD__")-1) == 0) ){
			GenBlock *pBlock = pGen->pCurrent;
			/* TICKET 1433-004: __FUNCTION__/__METHOD__ constants must be resolved at compile time,not run time */
			while( pBlock && (pBlock->iFlags & GEN_BLOCK_FUNC) == 0 ){
				/* Point to the upper block */
				pBlock = pBlock->pParent;
			}
			if( pBlock == 0 ){
				/* Called in the global scope,load NULL */
				PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,0,0,0);
			}else{
				/* Extract the target function/method */
				ph7_vm_func *pFunc = (ph7_vm_func *)pBlock->pUserData;
				if( pStr->zString[2] == 'M' /* METHOD */ && (pFunc->iFlags & VM_FUNC_CLASS_METHOD) == 0 ){
					/* Not a class method,Load null */
					PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,0,0,0);
				}else{
					pObj = PH7_ReserveConstObj(pGen->pVm,&nIdx);
					if( pObj == 0 ){
						PH7_GenCompileError(pGen,E_ERROR,pToken->nLine,"Fatal, PH7 engine is running out of memory");
						return SXERR_ABORT;
					}
					PH7_MemObjInitFromString(pGen->pVm,pObj,&pFunc->sName);
					/* Emit the load constant instruction */
					PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,nIdx,0,0);
				}
			}
			return SXRET_OK;
	}
	/* Query literal table */
	if( SXRET_OK != GenStateFindLiteral(&(*pGen),&pToken->sData,&nIdx) ){
		ph7_value *pObj;
		/* Unknown literal,install it in the literal table */
		pObj = PH7_ReserveConstObj(pGen->pVm,&nIdx);
		if( pObj == 0 ){
			PH7_GenCompileError(&(*pGen),E_ERROR,1,"PH7 engine is running out of memory");
			return SXERR_ABORT;
		}
		PH7_MemObjInitFromString(pGen->pVm,pObj,&pToken->sData);
		GenStateInstallLiteral(&(*pGen),pObj,nIdx);
	}
	/* Emit the load constant instruction */
	PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,1,nIdx,0,0);
	return SXRET_OK;
}
/*
 * Resolve a namespace path or simply load a literal:
 * As of this version namespace support is disabled. If you need
 * a working version that implement namespace,please contact
 * symisc systems via contact@symisc.net
 */
static sxi32 GenStateResolveNamespaceLiteral(ph7_gen_state *pGen)
{
	int emit = 0;
	sxi32 rc;
	while( pGen->pIn < &pGen->pEnd[-1] ){
		/* Emit a warning */
		if( !emit ){
			PH7_GenCompileError(&(*pGen),E_WARNING,pGen->pIn->nLine,
				"Namespace support is disabled in the current release of the PH7(%s) engine",
				ph7_lib_version()
				);
			emit = 1;
		}
		pGen->pIn++; /* Ignore the token */
	}
	/* Load literal */
	rc = GenStateLoadLiteral(&(*pGen));	
	return rc;
}
/*
 * Compile a literal which is an identifier(name) for a simple value.
 */
PH7_PRIVATE sxi32 PH7_CompileLiteral(ph7_gen_state *pGen,sxi32 iCompileFlag)
{
	sxi32 rc;
	rc = GenStateResolveNamespaceLiteral(&(*pGen));
	if( rc != SXRET_OK ){
		SXUNUSED(iCompileFlag); /* cc warning */
		return rc;
	}
	/* Node successfully compiled */
	return SXRET_OK;
}
/*
 * Recover from a compile-time error. In other words synchronize
 * the token stream cursor with the first semi-colon seen.
 */
static sxi32 PH7_ErrorRecover(ph7_gen_state *pGen)
{
	/* Synchronize with the next-semi-colon and avoid compiling this erroneous statement */
	while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_SEMI /*';'*/) == 0){
		pGen->pIn++;
	}
	return SXRET_OK;
}
/*
 * Check if the given identifier name is reserved or not.
 * Return TRUE if reserved.FALSE otherwise.
 */
static int GenStateIsReservedConstant(SyString *pName)
{
	if( pName->nByte == sizeof("null") - 1 ){
		if( SyStrnicmp(pName->zString,"null",sizeof("null")-1) == 0 ){
			return TRUE;
		}else if( SyStrnicmp(pName->zString,"true",sizeof("true")-1) == 0 ){
			return TRUE;
		}
	}else if( pName->nByte == sizeof("false") - 1 ){
		if( SyStrnicmp(pName->zString,"false",sizeof("false")-1) == 0 ){
			return TRUE;
		}
	}
	/* Not a reserved constant */
	return FALSE;
}
/*
 * Compile the 'const' statement.
 * According to the PHP language reference
 *  A constant is an identifier (name) for a simple value. As the name suggests, that value
 *  cannot change during the execution of the script (except for magic constants, which aren't actually constants).
 *  A constant is case-sensitive by default. By convention, constant identifiers are always uppercase.
 *  The name of a constant follows the same rules as any label in PHP. A valid constant name starts
 *  with a letter or underscore, followed by any number of letters, numbers, or underscores.
 *  As a regular expression it would be expressed thusly: [a-zA-Z_\x7f-\xff][a-zA-Z0-9_\x7f-\xff]* 
 *  Syntax
 *  You can define a constant by using the define()-function or by using the const keyword outside
 *  a class definition. Once a constant is defined, it can never be changed or undefined.
 *  You can get the value of a constant by simply specifying its name. Unlike with variables
 *  you should not prepend a constant with a $. You can also use the function constant() to read
 *  a constant's value if you wish to obtain the constant's name dynamically. Use get_defined_constants()
 *  to get a list of all defined constants.
 * 
 * Symisc eXtension.
 *  PH7 allow any complex expression to be associated with the constant while the zend engine
 *  would allow only simple scalar value.
 *  Example
 *    const HELLO = "Welcome "." guest ".rand_str(3); //Valid under PH7/Generate error using the zend engine
 *    Refer to the official documentation for more information on this feature.
 */
static sxi32 PH7_CompileConstant(ph7_gen_state *pGen)
{
	SySet *pConsCode,*pInstrContainer;
	sxu32 nLine = pGen->pIn->nLine;
	SyString *pName;
	sxi32 rc;
	pGen->pIn++; /* Jump the 'const' keyword */
	if( pGen->pIn >= pGen->pEnd || (pGen->pIn->nType & (PH7_TK_SSTR|PH7_TK_DSTR|PH7_TK_ID|PH7_TK_KEYWORD)) == 0 ){
		/* Invalid constant name */
		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"const: Invalid constant name");
		if( rc == SXERR_ABORT ){
			/* Error count limit reached,abort immediately */
			return SXERR_ABORT;
		}
		goto Synchronize;
	}
	/* Peek constant name */
	pName = &pGen->pIn->sData;
	/* Make sure the constant name isn't reserved */
	if( GenStateIsReservedConstant(pName) ){
		/* Reserved constant */
		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"const: Cannot redeclare a reserved constant '%z'",pName);
		if( rc == SXERR_ABORT ){
			/* Error count limit reached,abort immediately */
			return SXERR_ABORT;
		}
		goto Synchronize;
	}
	pGen->pIn++;
	if(pGen->pIn >= pGen->pEnd || (pGen->pIn->nType & PH7_TK_EQUAL /* '=' */) == 0 ){
		/* Invalid statement*/
		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"const: Expected '=' after constant name");
		if( rc == SXERR_ABORT ){
			/* Error count limit reached,abort immediately */
			return SXERR_ABORT;
		}
		goto Synchronize;
	}
	pGen->pIn++; /*Jump the equal sign */
	/* Allocate a new constant value container */
	pConsCode = (SySet *)SyMemBackendPoolAlloc(&pGen->pVm->sAllocator,sizeof(SySet));
	if( pConsCode == 0 ){
		PH7_GenCompileError(pGen,E_ERROR,nLine,"Fatal, PH7 engine is running out of memory");
		return SXERR_ABORT;
	}
	SySetInit(pConsCode,&pGen->pVm->sAllocator,sizeof(VmInstr));
	/* Swap bytecode container */
	pInstrContainer = PH7_VmGetByteCodeContainer(pGen->pVm);
	PH7_VmSetByteCodeContainer(pGen->pVm,pConsCode);
	/* Compile constant value */
	rc = PH7_CompileExpr(&(*pGen),0,0);
	/* Emit the done instruction */
	PH7_VmEmitInstr(pGen->pVm,PH7_OP_DONE,(rc != SXERR_EMPTY ? 1 : 0),0,0,0);
	PH7_VmSetByteCodeContainer(pGen->pVm,pInstrContainer); 
	if( rc == SXERR_ABORT ){
		/* Don't worry about freeing memory, everything will be released shortly */
		return SXERR_ABORT;
	}
	SySetSetUserData(pConsCode,pGen->pVm);
	/* Register the constant */
	rc = PH7_VmRegisterConstant(pGen->pVm,pName,PH7_VmExpandConstantValue,pConsCode);
	if( rc != SXRET_OK ){
		SySetRelease(pConsCode);
		SyMemBackendPoolFree(&pGen->pVm->sAllocator,pConsCode);
	}
	return SXRET_OK;
Synchronize:
	/* Synchronize with the next-semi-colon and avoid compiling this erroneous statement */
	while(pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_SEMI) == 0 ){
		pGen->pIn++;
	}
	return SXRET_OK;
}
/*
 * Compile the 'continue' statement.
 * According to the PHP language reference
 *  continue is used within looping structures to skip the rest of the current loop iteration
 *  and continue execution at the condition evaluation and then the beginning of the next
 *  iteration.
 *  Note: Note that in PHP the switch statement is considered a looping structure for
 *  the purposes of continue. 
 *  continue accepts an optional numeric argument which tells it how many levels
 *  of enclosing loops it should skip to the end of.
 *  Note:
 *   continue 0; and continue 1; is the same as running continue;. 
 */
static sxi32 PH7_CompileContinue(ph7_gen_state *pGen)
{
	GenBlock *pLoop; /* Target loop */
	sxi32 iLevel;    /* How many nesting loop to skip */
	sxu32 nLine;
	sxi32 rc;
	nLine = pGen->pIn->nLine;
	iLevel = 0;
	/* Jump the 'continue' keyword */
	pGen->pIn++;
	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_NUM) ){
		/* optional numeric argument which tells us how many levels
		 * of enclosing loops we should skip to the end of. 
		 */
		iLevel = (sxi32)PH7_TokenValueToInt64(&pGen->pIn->sData);
		if( iLevel < 2 ){
			iLevel = 0;
		}
		pGen->pIn++; /* Jump the optional numeric argument */
	}
	/* Point to the target loop */
	pLoop = GenStateFetchBlock(pGen->pCurrent,GEN_BLOCK_LOOP,iLevel);
	if( pLoop == 0 ){
		/* Illegal continue */
		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"A 'continue' statement may only be used within a loop or switch");
		if( rc == SXERR_ABORT ){
			/* Error count limit reached,abort immediately */
			return SXERR_ABORT;
		}
	}else{
		sxu32 nInstrIdx = 0;
		if( pLoop->iFlags & GEN_BLOCK_SWITCH ){
			/* According to the PHP language reference manual
			 *  Note that unlike some other languages, the continue statement applies to switch 
			 *  and acts similar to break. If you have a switch inside a loop and wish to continue
			 *  to the next iteration of the outer loop, use continue 2. 
			 */
			rc = PH7_VmEmitInstr(pGen->pVm,PH7_OP_JMP,0,0,0,&nInstrIdx);
			if( rc == SXRET_OK ){
				GenStateNewJumpFixup(pLoop,PH7_OP_JMP,nInstrIdx);
			}
		}else{
			/* Emit the unconditional jump to the beginning of the target loop */
			PH7_VmEmitInstr(pGen->pVm,PH7_OP_JMP,0,pLoop->nFirstInstr,0,&nInstrIdx);
			if( pLoop->bPostContinue == TRUE ){
				JumpFixup sJumpFix;
				/* Post-continue */
				sJumpFix.nJumpType = PH7_OP_JMP;
				sJumpFix.nInstrIdx = nInstrIdx;
				SySetPut(&pLoop->aPostContFix,(const void *)&sJumpFix);
			}
		}
	}
	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_SEMI) == 0 ){
		/* Not so fatal,emit a warning only */
		PH7_GenCompileError(&(*pGen),E_WARNING,pGen->pIn->nLine,"Expected semi-colon ';' after 'continue' statement");
	}
	/* Statement successfully compiled */
	return SXRET_OK;
}
/*
 * Compile the 'break' statement.
 * According to the PHP language reference
 *  break ends execution of the current for, foreach, while, do-while or switch
 *  structure.
 *  break accepts an optional numeric argument which tells it how many nested
 *  enclosing structures are to be broken out of. 
 */
static sxi32 PH7_CompileBreak(ph7_gen_state *pGen)
{
	GenBlock *pLoop; /* Target loop */
	sxi32 iLevel;    /* How many nesting loop to skip */
	sxu32 nLine;
	sxi32 rc;
	nLine = pGen->pIn->nLine;
	iLevel = 0;
	/* Jump the 'break' keyword */
	pGen->pIn++;
	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_NUM) ){
		/* optional numeric argument which tells us how many levels
		 * of enclosing loops we should skip to the end of. 
		 */
		iLevel = (sxi32)PH7_TokenValueToInt64(&pGen->pIn->sData);
		if( iLevel < 2 ){
			iLevel = 0;
		}
		pGen->pIn++; /* Jump the optional numeric argument */
	}
	/* Extract the target loop */
	pLoop = GenStateFetchBlock(pGen->pCurrent,GEN_BLOCK_LOOP,iLevel);
	if( pLoop == 0 ){
		/* Illegal break */
		rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,"A 'break' statement may only be used within a loop or switch");
		if( rc == SXERR_ABORT ){
			/* Error count limit reached,abort immediately */
			return SXERR_ABORT;
		}
	}else{
		sxu32 nInstrIdx; 
		rc = PH7_VmEmitInstr(pGen->pVm,PH7_OP_JMP,0,0,0,&nInstrIdx);
		if( rc == SXRET_OK ){
			/* Fix the jump later when the jump destination is resolved */
			GenStateNewJumpFixup(pLoop,PH7_OP_JMP,nInstrIdx);
		}
	}
	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_SEMI) == 0 ){
		/* Not so fatal,emit a warning only */
		PH7_GenCompileError(&(*pGen),E_WARNING,pGen->pIn->nLine,"Expected semi-colon ';' after 'break' statement");
	}
	/* Statement successfully compiled */
	return SXRET_OK;
}
/*
 * Compile or record a label.
 *  A label is a target point that is specified by an identifier followed by a colon.
 * Example
 *  goto LABEL;
 *   echo 'Foo';
 *  LABEL:
 *   echo 'Bar';
 */
static sxi32 PH7_CompileLabel(ph7_gen_state *pGen)
{
	GenBlock *pBlock;
	Label sLabel;
	/* Make sure the label does not occur inside a loop or a try{}catch(); block */
	pBlock = GenStateFetchBlock(pGen->pCurrent,GEN_BLOCK_LOOP|GEN_BLOCK_EXCEPTION,0);
	if( pBlock ){
		sxi32 rc;
		rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,
			"Label '%z' inside loop or try/catch block is disallowed",&pGen->pIn->sData);
		if( rc == SXERR_ABORT ){
			return SXERR_ABORT;
		}
	}else{
		SyString *pTarget = &pGen->pIn->sData;
		char *zDup;
		/* Initialize label fields */
		sLabel.nJumpDest = PH7_VmInstrLength(pGen->pVm);
		/* Duplicate label name */
		zDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,pTarget->zString,pTarget->nByte);
		if( zDup == 0 ){
			PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"Fatal, PH7 is running out of memory");
			return SXERR_ABORT;
		}
		SyStringInitFromBuf(&sLabel.sName,zDup,pTarget->nByte);
		sLabel.bRef  = FALSE;
		sLabel.nLine = pGen->pIn->nLine;
		pBlock = pGen->pCurrent;
		while( pBlock ){
			if( pBlock->iFlags & (GEN_BLOCK_FUNC|GEN_BLOCK_EXCEPTION) ){
				break;
			}
			/* Point to the upper block */
			pBlock = pBlock->pParent;
		}
		if( pBlock ){
			sLabel.pFunc = (ph7_vm_func *)pBlock->pUserData;
		}else{
			sLabel.pFunc = 0;
		}
		/* Insert in label set */
		SySetPut(&pGen->aLabel,(const void *)&sLabel);
	}
	pGen->pIn += 2; /* Jump the label name and the semi-colon*/
	return SXRET_OK;
}
/*
 * Compile the so hated 'goto' statement.
 * You've probably been taught that gotos are bad, but this sort
 * of rewriting  happens all the time, in fact every time you run
 * a compiler it has to do this.
 * According to the PHP language reference manual
 *   The goto operator can be used to jump to another section in the program.
 *   The target point is specified by a label followed by a colon, and the instruction
 *   is given as goto followed by the desired target label. This is not a full unrestricted goto.
 *   The target label must be within the same file and context, meaning that you cannot jump out
 *   of a function or method, nor can you jump into one. You also cannot jump into any sort of loop
 *   or switch structure. You may jump out of these, and a common use is to use a goto in place 
 *   of a multi-level break
 */
static sxi32 PH7_CompileGoto(ph7_gen_state *pGen)
{
	JumpFixup sJump;
	sxi32 rc;
	pGen->pIn++; /* Jump the 'goto' keyword */
	if( pGen->pIn >= pGen->pEnd ){
		/* Missing label */
		rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,"goto: expecting a 'label_name'");
		if( rc == SXERR_ABORT ){
			/* Error count limit reached,abort immediately */
			return SXERR_ABORT;
		}
		return SXRET_OK;
	}
	if( (pGen->pIn->nType & (PH7_TK_KEYWORD|PH7_TK_ID)) == 0 ){
		rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,"goto: Invalid label name: '%z'",&pGen->pIn->sData);
		if( rc == SXERR_ABORT ){
			/* Error count limit reached,abort immediately */
			return SXERR_ABORT;
		}
	}else{
		SyString *pTarget = &pGen->pIn->sData;
		GenBlock *pBlock;
		char *zDup;
		/* Prepare the jump destination */
		sJump.nJumpType = PH7_OP_JMP;
		sJump.nLine = pGen->pIn->nLine;
		/* Duplicate label name */
		zDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,pTarget->zString,pTarget->nByte);
		if( zDup == 0 ){
			PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"Fatal, PH7 is running out of memory");
			return SXERR_ABORT;
		}
		SyStringInitFromBuf(&sJump.sLabel,zDup,pTarget->nByte);
		pBlock = pGen->pCurrent;
		while( pBlock ){
			if( pBlock->iFlags & (GEN_BLOCK_FUNC|GEN_BLOCK_EXCEPTION) ){
				break;
			}
			/* Point to the upper block */
			pBlock = pBlock->pParent;
		}
		if( pBlock && pBlock->iFlags & GEN_BLOCK_EXCEPTION ){
			rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,"goto inside try/catch block is disallowed");
			if( rc == SXERR_ABORT ){
				/* Error count limit reached,abort immediately */
				return SXERR_ABORT;
			}
		}
		if( pBlock && (pBlock->iFlags & GEN_BLOCK_FUNC)){
			sJump.pFunc = (ph7_vm_func *)pBlock->pUserData;
		}else{
			sJump.pFunc = 0;
		}
		/* Emit the unconditional jump */
		if( SXRET_OK == PH7_VmEmitInstr(pGen->pVm,PH7_OP_JMP,0,0,0,&sJump.nInstrIdx) ){
			SySetPut(&pGen->aGoto,(const void *)&sJump);
		}
	}
	pGen->pIn++; /* Jump the label name */
	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_SEMI) == 0 ){
		PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"Expected semi-colon ';' after 'goto' statement");
	}
	/* Statement successfully compiled */
	return SXRET_OK;
}
/*
 * Point to the next PHP chunk that will be processed shortly.
 * Return SXRET_OK on success. Any other return value indicates
 * failure.
 */
static sxi32 GenStateNextChunk(ph7_gen_state *pGen)
{
	ph7_value *pRawObj; /* Raw chunk [i.e: HTML,XML...] */
	sxu32 nRawObj;   
	sxu32 nObjIdx;
	/* Consume raw chunks verbatim without any processing until we get
	 * a PHP block.
	 */
Consume:
	nRawObj = nObjIdx = 0;
	while( pGen->pRawIn < pGen->pRawEnd && pGen->pRawIn->nType != PH7_TOKEN_PHP ){
		pRawObj = PH7_ReserveConstObj(pGen->pVm,&nObjIdx);
		if( pRawObj == 0 ){
			PH7_GenCompileError(pGen,E_ERROR,1,"Fatal, PH7 engine is running out of memory");
			return SXERR_ABORT;
		}
		/* Mark as constant and emit the load constant instruction */
		PH7_MemObjInitFromString(pGen->pVm,pRawObj,&pGen->pRawIn->sData);
		PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,nObjIdx,0,0);
		++nRawObj;
		pGen->pRawIn++; /* Next chunk */
	}
	if( nRawObj > 0 ){
		/* Emit the consume instruction */
		PH7_VmEmitInstr(pGen->pVm,PH7_OP_CONSUME,nRawObj,0,0,0);
	}
	if( pGen->pRawIn < pGen->pRawEnd ){
		SySet *pTokenSet = pGen->pTokenSet;
		/* Reset the token set */
		SySetReset(pTokenSet);
		/* Tokenize input */
		PH7_TokenizePHP(SyStringData(&pGen->pRawIn->sData),SyStringLength(&pGen->pRawIn->sData),
			pGen->pRawIn->nLine,pTokenSet);
		/* Point to the fresh token stream */
		pGen->pIn  = (SyToken *)SySetBasePtr(pTokenSet);
		pGen->pEnd = &pGen->pIn[SySetUsed(pTokenSet)];
		/* Advance the stream cursor */
		pGen->pRawIn++;
		/* TICKET 1433-011 */
		if( pGen->pIn < pGen->pEnd && ( pGen->pIn->nType & PH7_TK_EQUAL ) ){
			static const sxu32 nKeyID = PH7_TKWRD_ECHO;
			sxi32 rc;
			/* Refer to TICKET 1433-009  */
			pGen->pIn->nType = PH7_TK_KEYWORD;
			pGen->pIn->pUserData = SX_INT_TO_PTR(nKeyID);
			SyStringInitFromBuf(&pGen->pIn->sData,"echo",sizeof("echo")-1);
			rc = PH7_CompileExpr(pGen,0,0);
			if( rc == SXERR_ABORT ){
				return SXERR_ABORT;
			}else if( rc != SXERR_EMPTY ){
				PH7_VmEmitInstr(pGen->pVm,PH7_OP_POP,1,0,0,0);
			}
			goto Consume;
		}
	}else{
		/* No more chunks to process */
		pGen->pIn = pGen->pEnd;
		return SXERR_EOF;
	}
	return SXRET_OK;
}
/*
 * Compile a PHP block.
 * A block is simply one or more PHP statements and expressions to compile
 * optionally delimited by braces {}.
 * Return SXRET_OK on success. Any other return value indicates failure
 * and this function takes care of generating the appropriate error
 * message.
 */
static sxi32 PH7_CompileBlock(
	ph7_gen_state *pGen, /* Code generator state */
	sxi32 nKeywordEnd    /* EOF-keyword [i.e: endif;endfor;...]. 0 (zero) otherwise */
	)
{
	sxi32 rc;
	if( pGen->pIn->nType & PH7_TK_OCB /* '{' */ ){
		sxu32 nLine = pGen->pIn->nLine;
		rc = GenStateEnterBlock(&(*pGen),GEN_BLOCK_STD,PH7_VmInstrLength(pGen->pVm),0,0);
		if( rc != SXRET_OK ){
			return SXERR_ABORT;
		}
		pGen->pIn++;
		/* Compile until we hit the closing braces '}' */
		for(;;){
			if( pGen->pIn >= pGen->pEnd ){
				rc = GenStateNextChunk(&(*pGen));
				if (rc == SXERR_ABORT ){
			 	   return SXERR_ABORT;
				}
				if( rc == SXERR_EOF ){
					/* No more token to process. Missing closing braces */
					PH7_GenCompileError(&(*pGen),E_ERROR,nLine,"Missing closing braces '}'");
					break;
				}
			}
			if( pGen->pIn->nType & PH7_TK_CCB/*'}'*/ ){
				/* Closing braces found,break immediately*/
				pGen->pIn++;
				break;
			}
			/* Compile a single statement */
			rc = GenStateCompileChunk(&(*pGen),PH7_COMPILE_SINGLE_STMT);
			if( rc == SXERR_ABORT ){
				return SXERR_ABORT;
			}
		}
		GenStateLeaveBlock(&(*pGen),0);		
	}else if( (pGen->pIn->nType & PH7_TK_COLON /* ':' */) && nKeywordEnd > 0 ){
		pGen->pIn++;
		rc = GenStateEnterBlock(&(*pGen),GEN_BLOCK_STD,PH7_VmInstrLength(pGen->pVm),0,0);
		if( rc != SXRET_OK ){
			return SXERR_ABORT;
		}
		/* Compile until we hit the EOF-keyword [i.e: endif;endfor;...] */
		for(;;){
			if( pGen->pIn >= pGen->pEnd ){
				rc = GenStateNextChunk(&(*pGen));
				if (rc == SXERR_ABORT ){
			 	   return SXERR_ABORT;
				}
				if( rc == SXERR_EOF || pGen->pIn >= pGen->pEnd ){
					/* No more token to process */
					if( rc == SXERR_EOF ){
						PH7_GenCompileError(&(*pGen),E_WARNING,pGen->pEnd[-1].nLine,
							"Missing 'endfor;','endwhile;','endswitch;' or 'endforeach;' keyword");
					}
					break;
				}
			}
			if( pGen->pIn->nType & PH7_TK_KEYWORD ){
				sxi32 nKwrd; 
				/* Keyword found */
				nKwrd = SX_PTR_TO_INT(pGen->pIn->pUserData);
				if( nKwrd == nKeywordEnd ||
					(nKeywordEnd == PH7_TKWRD_ENDIF && (nKwrd == PH7_TKWRD_ELSE || nKwrd == PH7_TKWRD_ELIF)) ){
						/* Delimiter keyword found,break */
						if( nKwrd != PH7_TKWRD_ELSE && nKwrd != PH7_TKWRD_ELIF ){
							pGen->pIn++; /*  endif;endswitch... */
						}
						break;
				}
			}
			/* Compile a single statement */
			rc = GenStateCompileChunk(&(*pGen),PH7_COMPILE_SINGLE_STMT);
			if( rc == SXERR_ABORT ){
				return SXERR_ABORT;
			}
		}
		GenStateLeaveBlock(&(*pGen),0);	
	}else{
		/* Compile a single statement */
		rc = GenStateCompileChunk(&(*pGen),PH7_COMPILE_SINGLE_STMT);
		if( rc == SXERR_ABORT ){
			return SXERR_ABORT;
		}
	}
	/* Jump trailing semi-colons ';' */
	while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_SEMI) ){
		pGen->pIn++;
	}
	return SXRET_OK;
}
/*
 * Compile the gentle 'while' statement.
 * According to the PHP language reference
 *  while loops are the simplest type of loop in PHP.They behave just like their C counterparts.
 *  The basic form of a while statement is:
 *  while (expr)
 *   statement
 *  The meaning of a while statement is simple. It tells PHP to execute the nested statement(s)
 *  repeatedly, as long as the while expression evaluates to TRUE. The value of the expression
 *  is checked each time at the beginning of the loop, so even if this value changes during
 *  the execution of the nested statement(s), execution will not stop until the end of the iteration
 *  (each time PHP runs the statements in the loop is one iteration). Sometimes, if the while
 *  expression evaluates to FALSE from the very beginning, the nested statement(s) won't even be run once.
 *  Like with the if statement, you can group multiple statements within the same while loop by surrounding
 *  a group of statements with curly braces, or by using the alternate syntax:
 *  while (expr):
 *    statement
 *   endwhile;
 */
static sxi32 PH7_CompileWhile(ph7_gen_state *pGen)
{ 
	GenBlock *pWhileBlock = 0;
	SyToken *pTmp,*pEnd = 0;
	sxu32 nFalseJump;
	sxu32 nLine;
	sxi32 rc;
	nLine = pGen->pIn->nLine;
	/* Jump the 'while' keyword */
	pGen->pIn++;    
	if( pGen->pIn >= pGen->pEnd || (pGen->pIn->nType & PH7_TK_LPAREN) == 0 ){
		/* Syntax error */
		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Expected '(' after 'while' keyword");
		if( rc == SXERR_ABORT ){
			/* Error count limit reached,abort immediately */
			return SXERR_ABORT;
		}
		goto Synchronize;
	}
	/* Jump the left parenthesis '(' */
	pGen->pIn++; 
	/* Create the loop block */
	rc = GenStateEnterBlock(&(*pGen),GEN_BLOCK_LOOP,PH7_VmInstrLength(pGen->pVm),0,&pWhileBlock);
	if( rc != SXRET_OK ){
		return SXERR_ABORT;
	}
	/* Delimit the condition */
	PH7_DelimitNestedTokens(pGen->pIn,pGen->pEnd,PH7_TK_LPAREN /* '(' */,PH7_TK_RPAREN /* ')' */,&pEnd);
	if( pGen->pIn == pEnd || pEnd >= pGen->pEnd ){
		/* Empty expression */
		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Expected expression after 'while' keyword");
		if( rc == SXERR_ABORT ){
			/* Error count limit reached,abort immediately */
			return SXERR_ABORT;
		}
	}
	/* Swap token streams */
	pTmp = pGen->pEnd;
	pGen->pEnd = pEnd;
	/* Compile the expression */
	rc = PH7_CompileExpr(&(*pGen),0,0);
	if( rc == SXERR_ABORT ){
		/* Expression handler request an operation abort [i.e: Out-of-memory] */
		return SXERR_ABORT;
	}
	/* Update token stream */
	while(pGen->pIn < pEnd ){
		rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"Unexpected token '%z'",&pGen->pIn->sData);
		if( rc == SXERR_ABORT ){
			return SXERR_ABORT;
		}
		pGen->pIn++;
	}
	/* Synchronize pointers */
	pGen->pIn  = &pEnd[1];
	pGen->pEnd = pTmp;
	/* Emit the false jump */
	PH7_VmEmitInstr(pGen->pVm,PH7_OP_JZ,0,0,0,&nFalseJump);
	/* Save the instruction index so we can fix it later when the jump destination is resolved */
	GenStateNewJumpFixup(pWhileBlock,PH7_OP_JZ,nFalseJump);
	/* Compile the loop body */
	rc = PH7_CompileBlock(&(*pGen),PH7_TKWRD_ENDWHILE);
	if( rc == SXERR_ABORT ){
		return SXERR_ABORT;
	}
	/* Emit the unconditional jump to the start of the loop */
	PH7_VmEmitInstr(pGen->pVm,PH7_OP_JMP,0,pWhileBlock->nFirstInstr,0,0);
	/* Fix all jumps now the destination is resolved */
	GenStateFixJumps(pWhileBlock,-1,PH7_VmInstrLength(pGen->pVm));
	/* Release the loop block */
	GenStateLeaveBlock(pGen,0);
	/* Statement successfully compiled */
	return SXRET_OK;
Synchronize:
	/* Synchronize with the first semi-colon ';' so we can avoid 
	 * compiling this erroneous block.
	 */
	while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & (PH7_TK_SEMI|PH7_TK_OCB)) == 0 ){
		pGen->pIn++;
	}
	return SXRET_OK;
}
/*
 * Compile the ugly do..while() statement.
 * According to the PHP language reference
 *  do-while loops are very similar to while loops, except the truth expression is checked
 *  at the end of each iteration instead of in the beginning. The main difference from regular
 *  while loops is that the first iteration of a do-while loop is guaranteed to run
 *  (the truth expression is only checked at the end of the iteration), whereas it may not 
 *  necessarily run with a regular while loop (the truth expression is checked at the beginning
 *  of each iteration, if it evaluates to FALSE right from the beginning, the loop execution
 *  would end immediately).
 *  There is just one syntax for do-while loops:
 *  <?php
 *  $i = 0;
 *  do {
 *   echo $i;
 *  } while ($i > 0);
 * ?>
 */
static sxi32 PH7_CompileDoWhile(ph7_gen_state *pGen)
{
	SyToken *pTmp,*pEnd = 0;
	GenBlock *pDoBlock = 0;
	sxu32 nLine;
	sxi32 rc;
	nLine = pGen->pIn->nLine;
	/* Jump the 'do' keyword */
	pGen->pIn++;   
	/* Create the loop block */
	rc = GenStateEnterBlock(&(*pGen),GEN_BLOCK_LOOP,PH7_VmInstrLength(pGen->pVm),0,&pDoBlock);
	if( rc != SXRET_OK ){
		return SXERR_ABORT;
	}
	/* Deffer 'continue;' jumps until we compile the block */
	pDoBlock->bPostContinue = TRUE;
	rc = PH7_CompileBlock(&(*pGen),0);
	if( rc == SXERR_ABORT ){
		return SXERR_ABORT;
	}
	if( pGen->pIn < pGen->pEnd ){
		nLine = pGen->pIn->nLine;
	}
	if( pGen->pIn >= pGen->pEnd || pGen->pIn->nType != PH7_TK_KEYWORD ||
		SX_PTR_TO_INT(pGen->pIn->pUserData) != PH7_TKWRD_WHILE ){
			/* Missing 'while' statement */
			rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Missing 'while' statement after 'do' block");
			if( rc == SXERR_ABORT ){
				/* Error count limit reached,abort immediately */
				return SXERR_ABORT;
			}
			goto Synchronize;
	}
	/* Jump the 'while' keyword */
	pGen->pIn++;    
	if( pGen->pIn >= pGen->pEnd || (pGen->pIn->nType & PH7_TK_LPAREN) == 0 ){
		/* Syntax error */
		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Expected '(' after 'while' keyword");
		if( rc == SXERR_ABORT ){
			/* Error count limit reached,abort immediately */
			return SXERR_ABORT;
		}
		goto Synchronize;
	}
	/* Jump the left parenthesis '(' */
	pGen->pIn++; 
	/* Delimit the condition */
	PH7_DelimitNestedTokens(pGen->pIn,pGen->pEnd,PH7_TK_LPAREN /* '(' */,PH7_TK_RPAREN /* ')' */,&pEnd);
	if( pGen->pIn == pEnd || pEnd >= pGen->pEnd ){
		/* Empty expression */
		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Expected expression after 'while' keyword");
		if( rc == SXERR_ABORT ){
			/* Error count limit reached,abort immediately */
			return SXERR_ABORT;
		}
		goto Synchronize;
	}
	/* Fix post-continue jumps now the jump destination is resolved */
	if( SySetUsed(&pDoBlock->aPostContFix) > 0 ){
		JumpFixup *aPost;
		VmInstr *pInstr;
		sxu32 nJumpDest;
		sxu32 n;
		aPost = (JumpFixup *)SySetBasePtr(&pDoBlock->aPostContFix);
		nJumpDest = PH7_VmInstrLength(pGen->pVm);
		for( n = 0 ; n < SySetUsed(&pDoBlock->aPostContFix) ; ++n ){
			pInstr = PH7_VmGetInstr(pGen->pVm,aPost[n].nInstrIdx);
			if( pInstr ){
				/* Fix */
				pInstr->iP2 = nJumpDest;
			}
		}
	}
	/* Swap token streams */
	pTmp = pGen->pEnd;
	pGen->pEnd = pEnd;
	/* Compile the expression */
	rc = PH7_CompileExpr(&(*pGen),0,0);
	if( rc == SXERR_ABORT ){
		/* Expression handler request an operation abort [i.e: Out-of-memory] */
		return SXERR_ABORT;
	}
	/* Update token stream */
	while(pGen->pIn < pEnd ){
		rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"Unexpected token '%z'",&pGen->pIn->sData);
		if( rc == SXERR_ABORT ){
			return SXERR_ABORT;
		}
		pGen->pIn++;
	}
	pGen->pIn  = &pEnd[1];
	pGen->pEnd = pTmp;
	/* Emit the true jump to the beginning of the loop */
	PH7_VmEmitInstr(pGen->pVm,PH7_OP_JNZ,0,pDoBlock->nFirstInstr,0,0);
	/* Fix all jumps now the destination is resolved */
	GenStateFixJumps(pDoBlock,-1,PH7_VmInstrLength(pGen->pVm));
	/* Release the loop block */
	GenStateLeaveBlock(pGen,0);
	/* Statement successfully compiled */
	return SXRET_OK;
Synchronize:
	/* Synchronize with the first semi-colon ';' so we can avoid 
	 * compiling this erroneous block.
	 */
	while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & (PH7_TK_SEMI|PH7_TK_OCB)) == 0 ){
		pGen->pIn++;
	}
	return SXRET_OK;
}
/*
 * Compile the complex and powerful 'for' statement.
 * According to the PHP language reference
 *  for loops are the most complex loops in PHP. They behave like their C counterparts.
 *  The syntax of a for loop is:
 *  for (expr1; expr2; expr3)
 *   statement
 *  The first expression (expr1) is evaluated (executed) once unconditionally at
 *  the beginning of the loop.
 *  In the beginning of each iteration, expr2 is evaluated. If it evaluates to
 *  TRUE, the loop continues and the nested statement(s) are executed. If it evaluates
 *  to FALSE, the execution of the loop ends.
 *  At the end of each iteration, expr3 is evaluated (executed).
 *  Each of the expressions can be empty or contain multiple expressions separated by commas.
 *  In expr2, all expressions separated by a comma are evaluated but the result is taken
 *  from the last part. expr2 being empty means the loop should be run indefinitely
 *  (PHP implicitly considers it as TRUE, like C). This may not be as useless as you might
 *  think, since often you'd want to end the loop using a conditional break statement instead
 *  of using the for truth expression.
 */
static sxi32 PH7_CompileFor(ph7_gen_state *pGen)
{
	SyToken *pTmp,*pPostStart,*pEnd = 0;
	GenBlock *pForBlock = 0;
	sxu32 nFalseJump;
	sxu32 nLine;
	sxi32 rc;
	nLine = pGen->pIn->nLine;
	/* Jump the 'for' keyword */
	pGen->pIn++;    
	if( pGen->pIn >= pGen->pEnd || (pGen->pIn->nType & PH7_TK_LPAREN) == 0 ){
		/* Syntax error */
		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Expected '(' after 'for' keyword");
		if( rc == SXERR_ABORT ){
			/* Error count limit reached,abort immediately */
			return SXERR_ABORT;
		}
		return SXRET_OK;
	}
	/* Jump the left parenthesis '(' */
	pGen->pIn++; 
	/* Delimit the init-expr;condition;post-expr */
	PH7_DelimitNestedTokens(pGen->pIn,pGen->pEnd,PH7_TK_LPAREN /* '(' */,PH7_TK_RPAREN /* ')' */,&pEnd);
	if( pGen->pIn == pEnd || pEnd >= pGen->pEnd ){
		/* Empty expression */
		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"for: Invalid expression");
		if( rc == SXERR_ABORT ){
			/* Error count limit reached,abort immediately */
			return SXERR_ABORT;
		}
		/* Synchronize */
		pGen->pIn = pEnd;
		if( pGen->pIn < pGen->pEnd ){
			pGen->pIn++;
		}
		return SXRET_OK;
	}
	/* Swap token streams */
	pTmp = pGen->pEnd;
	pGen->pEnd = pEnd;
	/* Compile initialization expressions if available */
	rc = PH7_CompileExpr(&(*pGen),0,0);
	/* Pop operand lvalues */
	if( rc == SXERR_ABORT ){
		/* Expression handler request an operation abort [i.e: Out-of-memory] */
		return SXERR_ABORT;
	}else if( rc != SXERR_EMPTY ){
		PH7_VmEmitInstr(pGen->pVm,PH7_OP_POP,1,0,0,0);
	}
	if( (pGen->pIn->nType & PH7_TK_SEMI) == 0 ){
		/* Syntax error */
		rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,
			"for: Expected ';' after initialization expressions");
		if( rc == SXERR_ABORT ){
			/* Error count limit reached,abort immediately */
			return SXERR_ABORT;
		}
		return SXRET_OK;
	}
	/* Jump the trailing ';' */
	pGen->pIn++;
	/* Create the loop block */
	rc = GenStateEnterBlock(&(*pGen),GEN_BLOCK_LOOP,PH7_VmInstrLength(pGen->pVm),0,&pForBlock);
	if( rc != SXRET_OK ){
		return SXERR_ABORT;
	}
	/* Deffer continue jumps */
	pForBlock->bPostContinue = TRUE;
	/* Compile the condition */
	rc = PH7_CompileExpr(&(*pGen),0,0);
	if( rc == SXERR_ABORT ){
		/* Expression handler request an operation abort [i.e: Out-of-memory] */
		return SXERR_ABORT;
	}else if( rc != SXERR_EMPTY ){
		/* Emit the false jump */
		PH7_VmEmitInstr(pGen->pVm,PH7_OP_JZ,0,0,0,&nFalseJump);
		/* Save the instruction index so we can fix it later when the jump destination is resolved */
		GenStateNewJumpFixup(pForBlock,PH7_OP_JZ,nFalseJump);
	}
	if( (pGen->pIn->nType & PH7_TK_SEMI) == 0 ){
		/* Syntax error */
		rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,
			"for: Expected ';' after conditionals expressions");
		if( rc == SXERR_ABORT ){
			/* Error count limit reached,abort immediately */
			return SXERR_ABORT;
		}
		return SXRET_OK;
	}
	/* Jump the trailing ';' */
	pGen->pIn++;
	/* Save the post condition stream */
	pPostStart = pGen->pIn;
	/* Compile the loop body */
	pGen->pIn  = &pEnd[1]; /* Jump the trailing parenthesis ')' */
	pGen->pEnd = pTmp;
	rc = PH7_CompileBlock(&(*pGen),PH7_TKWRD_ENDFOR);
	if( rc == SXERR_ABORT ){
		return SXERR_ABORT;
	}
	/* Fix post-continue jumps */
	if( SySetUsed(&pForBlock->aPostContFix) > 0 ){
		JumpFixup *aPost;
		VmInstr *pInstr;
		sxu32 nJumpDest;
		sxu32 n;
		aPost = (JumpFixup *)SySetBasePtr(&pForBlock->aPostContFix);
		nJumpDest = PH7_VmInstrLength(pGen->pVm);
		for( n = 0 ; n < SySetUsed(&pForBlock->aPostContFix) ; ++n ){
			pInstr = PH7_VmGetInstr(pGen->pVm,aPost[n].nInstrIdx);
			if( pInstr ){
				/* Fix jump */
				pInstr->iP2 = nJumpDest;
			}
		}
	}
	/* compile the post-expressions if available */
	while( pPostStart < pEnd && (pPostStart->nType & PH7_TK_SEMI) ){
		pPostStart++;
	}
	if( pPostStart < pEnd ){
		SyToken *pTmpIn,*pTmpEnd;
		SWAP_DELIMITER(pGen,pPostStart,pEnd);
		rc = PH7_CompileExpr(&(*pGen),0,0);
		if( pGen->pIn < pGen->pEnd ){
			/* Syntax error */
			rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,"for: Expected ')' after post-expressions");
			if( rc == SXERR_ABORT ){
				/* Error count limit reached,abort immediately */
				return SXERR_ABORT;
			}
			return SXRET_OK;
		}
		RE_SWAP_DELIMITER(pGen);
		if( rc == SXERR_ABORT ){
			/* Expression handler request an operation abort [i.e: Out-of-memory] */
			return SXERR_ABORT;
		}else if( rc != SXERR_EMPTY){
			/* Pop operand lvalue */
			PH7_VmEmitInstr(pGen->pVm,PH7_OP_POP,1,0,0,0);
		}
	}
	/* Emit the unconditional jump to the start of the loop */
	PH7_VmEmitInstr(pGen->pVm,PH7_OP_JMP,0,pForBlock->nFirstInstr,0,0);
	/* Fix all jumps now the destination is resolved */
	GenStateFixJumps(pForBlock,-1,PH7_VmInstrLength(pGen->pVm));
	/* Release the loop block */
	GenStateLeaveBlock(pGen,0);
	/* Statement successfully compiled */
	return SXRET_OK;
}
/* Expression tree validator callback used by the 'foreach' statement.
 * Note that only variable expression [i.e: $x; ${'My'.'Var'}; ${$a['key]};...]
 * are allowed.
 */
static sxi32 GenStateForEachNodeValidator(ph7_gen_state *pGen,ph7_expr_node *pRoot)
{
	sxi32 rc = SXRET_OK; /* Assume a valid expression tree */
	if( pRoot->xCode != PH7_CompileVariable ){
		/* Unexpected expression */
		rc = PH7_GenCompileError(&(*pGen),E_ERROR,pRoot->pStart? pRoot->pStart->nLine : 0,
			"foreach: Expecting a variable name");
		if( rc != SXERR_ABORT ){
			rc = SXERR_INVALID;
		}
	}
	return rc;
}
/*
 * Compile the 'foreach' statement.
 * According to the PHP language reference
 *  The foreach construct simply gives an easy way to iterate over arrays. foreach works
 *  only on arrays (and objects), and will issue an error when you try to use it on a variable
 *  with a different data type or an uninitialized variable. There are two syntaxes; the second
 *  is a minor but useful extension of the first:
 *  foreach (array_expression as $value)
 *    statement
 *  foreach (array_expression as $key => $value)
 *   statement
 *  The first form loops over the array given by array_expression. On each loop, the value 
 *  of the current element is assigned to $value and the internal array pointer is advanced
 *  by one (so on the next loop, you'll be looking at the next element).
 *  The second form does the same thing, except that the current element's key will be assigned
 *  to the variable $key on each loop.
 *  Note:
 *  When foreach first starts executing, the internal array pointer is automatically reset to the
 *  first element of the array. This means that you do not need to call reset() before a foreach loop.
 *  Note:
 *  Unless the array is referenced, foreach operates on a copy of the specified array and not the array
 *  itself. foreach has some side effects on the array pointer. Don't rely on the array pointer during
 *  or after the foreach without resetting it.
 *  You can easily modify array's elements by preceding $value with &. This will assign reference instead
 *  of copying the value. 
 */
static sxi32 PH7_CompileForeach(ph7_gen_state *pGen)
{ 
	SyToken *pCur,*pTmp,*pEnd = 0;
	GenBlock *pForeachBlock = 0;
	ph7_foreach_info *pInfo;
	sxu32 nFalseJump;
	VmInstr *pInstr;
	sxu32 nLine;
	sxi32 rc;
	nLine = pGen->pIn->nLine;
	/* Jump the 'foreach' keyword */
	pGen->pIn++;    
	if( pGen->pIn >= pGen->pEnd || (pGen->pIn->nType & PH7_TK_LPAREN) == 0 ){
		/* Syntax error */
		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"foreach: Expected '('");
		if( rc == SXERR_ABORT ){
			/* Error count limit reached,abort immediately */
			return SXERR_ABORT;
		}
		goto Synchronize;
	}
	/* Jump the left parenthesis '(' */
	pGen->pIn++; 
	/* Create the loop block */
	rc = GenStateEnterBlock(&(*pGen),GEN_BLOCK_LOOP,PH7_VmInstrLength(pGen->pVm),0,&pForeachBlock);
	if( rc != SXRET_OK ){
		return SXERR_ABORT;
	}
	/* Delimit the expression */
	PH7_DelimitNestedTokens(pGen->pIn,pGen->pEnd,PH7_TK_LPAREN /* '(' */,PH7_TK_RPAREN /* ')' */,&pEnd);
	if( pGen->pIn == pEnd || pEnd >= pGen->pEnd ){
		/* Empty expression */
		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"foreach: Missing expression");
		if( rc == SXERR_ABORT ){
			/* Error count limit reached,abort immediately */
			return SXERR_ABORT;
		}
		/* Synchronize */
		pGen->pIn = pEnd;
		if( pGen->pIn < pGen->pEnd ){
			pGen->pIn++;
		}
		return SXRET_OK;
	}
	/* Compile the array expression */
	pCur = pGen->pIn;
	while( pCur < pEnd ){
		if( pCur->nType & PH7_TK_KEYWORD ){
			sxi32 nKeywrd = SX_PTR_TO_INT(pCur->pUserData);
			if( nKeywrd == PH7_TKWRD_AS ){
				/* Break with the first 'as' found */
				break;
			}
		}
		/* Advance the stream cursor */
		pCur++;
	}
	if( pCur <= pGen->pIn ){
		rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,
			"foreach: Missing array/object expression");
		if( rc == SXERR_ABORT ){
			/* Don't worry about freeing memory, everything will be released shortly */
			return SXERR_ABORT;
		}
		goto Synchronize;
	}
	/* Swap token streams */
	pTmp = pGen->pEnd;
	pGen->pEnd = pCur;
	rc = PH7_CompileExpr(&(*pGen),0,0);
	if( rc == SXERR_ABORT ){
		/* Expression handler request an operation abort [i.e: Out-of-memory] */
		return SXERR_ABORT;
	}
	/* Update token stream */
	while(pGen->pIn < pCur ){
		rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"foreach: Unexpected token '%z'",&pGen->pIn->sData);
		if( rc == SXERR_ABORT ){
			/* Don't worry about freeing memory, everything will be released shortly */
			return SXERR_ABORT;
		}
		pGen->pIn++;
	}
	pCur++; /* Jump the 'as' keyword */
	pGen->pIn = pCur; 
	if( pGen->pIn >= pEnd ){
		rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"foreach: Missing $key => $value pair");
		if( rc == SXERR_ABORT ){
			return SXERR_ABORT;
		}
	}
	/* Create the foreach context */
	pInfo = (ph7_foreach_info *)SyMemBackendAlloc(&pGen->pVm->sAllocator,sizeof(ph7_foreach_info));
	if( pInfo == 0 ){
		PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"Fatal, PH7 engine is running out-of-memory");
		return SXERR_ABORT;
	}
	/* Zero the structure */
	SyZero(pInfo,sizeof(ph7_foreach_info));
	/* Initialize structure fields */
	SySetInit(&pInfo->aStep,&pGen->pVm->sAllocator,sizeof(ph7_foreach_step *));
	/* Check if we have a key field */
	while( pCur < pEnd && (pCur->nType & PH7_TK_ARRAY_OP) == 0 ){
		pCur++;
	}
	if( pCur < pEnd ){
		/* Compile the expression holding the key name */
		if( pGen->pIn >= pCur ){
			rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"foreach: Missing $key");
			if( rc == SXERR_ABORT ){
				/* Don't worry about freeing memory, everything will be released shortly */
				return SXERR_ABORT;
			}
		}else{
			pGen->pEnd = pCur;
			rc = PH7_CompileExpr(&(*pGen),0,GenStateForEachNodeValidator);
			if( rc == SXERR_ABORT ){
				/* Don't worry about freeing memory, everything will be released shortly */
				return SXERR_ABORT;
			}
			pInstr = PH7_VmPopInstr(pGen->pVm);
			if( pInstr->p3 ){
				/* Record key name */
				SyStringInitFromBuf(&pInfo->sKey,pInstr->p3,SyStrlen((const char *)pInstr->p3));
			}
			pInfo->iFlags |= PH7_4EACH_STEP_KEY;
		}
		pGen->pIn = &pCur[1]; /* Jump the arrow */
	}
	pGen->pEnd = pEnd;
	if( pGen->pIn >= pEnd ){
		rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"foreach: Missing $value");
		if( rc == SXERR_ABORT ){
			/* Don't worry about freeing memory, everything will be released shortly */
			return SXERR_ABORT;
		}
		goto Synchronize;
	}
	if( pGen->pIn->nType & PH7_TK_AMPER /*'&'*/){
		pGen->pIn++;
		/* Pass by reference  */
		pInfo->iFlags |= PH7_4EACH_STEP_REF;
	}
	/* Compile the expression holding the value name */
	rc = PH7_CompileExpr(&(*pGen),0,GenStateForEachNodeValidator);
	if( rc == SXERR_ABORT ){
		/* Don't worry about freeing memory, everything will be released shortly */
		return SXERR_ABORT;
	}
	pInstr = PH7_VmPopInstr(pGen->pVm);
	if( pInstr->p3 ){
		/* Record value name */
		SyStringInitFromBuf(&pInfo->sValue,pInstr->p3,SyStrlen((const char *)pInstr->p3));
	}
	/* Emit the 'FOREACH_INIT' instruction */
	PH7_VmEmitInstr(pGen->pVm,PH7_OP_FOREACH_INIT,0,0,pInfo,&nFalseJump);
	/* Save the instruction index so we can fix it later when the jump destination is resolved */
	GenStateNewJumpFixup(pForeachBlock,PH7_OP_FOREACH_INIT,nFalseJump);
	/* Record the first instruction to execute */
	pForeachBlock->nFirstInstr = PH7_VmInstrLength(pGen->pVm);
	/* Emit the FOREACH_STEP instruction */
	PH7_VmEmitInstr(pGen->pVm,PH7_OP_FOREACH_STEP,0,0,pInfo,&nFalseJump);
	/* Save the instruction index so we can fix it later when the jump destination is resolved */
	GenStateNewJumpFixup(pForeachBlock,PH7_OP_FOREACH_STEP,nFalseJump);
	/* Compile the loop body */
	pGen->pIn = &pEnd[1];
	pGen->pEnd = pTmp;
	rc = PH7_CompileBlock(&(*pGen),PH7_TKWRD_END4EACH);
	if( rc == SXERR_ABORT ){
		/* Don't worry about freeing memory, everything will be released shortly */
		return SXERR_ABORT;
	}
	/* Emit the unconditional jump to the start of the loop */
	PH7_VmEmitInstr(pGen->pVm,PH7_OP_JMP,0,pForeachBlock->nFirstInstr,0,0);
	/* Fix all jumps now the destination is resolved */
	GenStateFixJumps(pForeachBlock,-1,PH7_VmInstrLength(pGen->pVm));
	/* Release the loop block */
	GenStateLeaveBlock(pGen,0);
	/* Statement successfully compiled */
	return SXRET_OK;
Synchronize:
	/* Synchronize with the first semi-colon ';' so we can avoid 
	 * compiling this erroneous block.
	 */
	while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & (PH7_TK_SEMI|PH7_TK_OCB)) == 0 ){
		pGen->pIn++;
	}
	return SXRET_OK;
}
/*
 * Compile the infamous if/elseif/else if/else statements.
 * According to the PHP language reference
 *  The if construct is one of the most important features of many languages PHP included.
 *  It allows for conditional execution of code fragments. PHP features an if structure 
 *  that is similar to that of C:
 *  if (expr)
 *   statement
 *  else construct:
 *   Often you'd want to execute a statement if a certain condition is met, and a different
 *   statement if the condition is not met. This is what else is for. else extends an if statement
 *   to execute a statement in case the expression in the if statement evaluates to FALSE.
 *   For example, the following code would display a is greater than b if $a is greater than
 *   $b, and a is NOT greater than b otherwise.
 *   The else statement is only executed if the if expression evaluated to FALSE, and if there
 *   were any elseif expressions - only if they evaluated to FALSE as well
 *  elseif
 *   elseif, as its name suggests, is a combination of if and else. Like else, it extends
 *   an if statement to execute a different statement in case the original if expression evaluates
 *   to FALSE. However, unlike else, it will execute that alternative expression only if the elseif
 *   conditional expression evaluates to TRUE. For example, the following code would display a is bigger
 *   than b, a equal to b or a is smaller than b:
 *   <?php
 *    if ($a > $b) {
 *     echo "a is bigger than b";
 *    } elseif ($a == $b) {
 *     echo "a is equal to b";
 *    } else {
 *     echo "a is smaller than b";
 *    }
 *    ?>
 */
static sxi32 PH7_CompileIf(ph7_gen_state *pGen)
{
	SyToken *pToken,*pTmp,*pEnd = 0;
	GenBlock *pCondBlock = 0;
	sxu32 nJumpIdx;
	sxu32 nKeyID;
	sxi32 rc;
	/* Jump the 'if' keyword */
	pGen->pIn++;
	pToken = pGen->pIn; 
	/* Create the conditional block */
	rc = GenStateEnterBlock(&(*pGen),GEN_BLOCK_COND,PH7_VmInstrLength(pGen->pVm),0,&pCondBlock);
	if( rc != SXRET_OK ){
		return SXERR_ABORT;
	}
	/* Process as many [if/else if/elseif/else] blocks as we can */
	for(;;){
		if( pToken >= pGen->pEnd || (pToken->nType & PH7_TK_LPAREN) == 0 ){
			/* Syntax error */
			if( pToken >= pGen->pEnd ){
				pToken--;
			}
			rc = PH7_GenCompileError(pGen,E_ERROR,pToken->nLine,"if/else/elseif: Missing '('");
			if( rc == SXERR_ABORT ){
				/* Error count limit reached,abort immediately */
				return SXERR_ABORT;
			}
			goto Synchronize;
		}
		/* Jump the left parenthesis '(' */
		pToken++; 
		/* Delimit the condition */
		PH7_DelimitNestedTokens(pToken,pGen->pEnd,PH7_TK_LPAREN /* '(' */,PH7_TK_RPAREN /* ')' */,&pEnd);
		if( pToken >= pEnd || (pEnd->nType & PH7_TK_RPAREN) == 0 ){
			/* Syntax error */
			if( pToken >= pGen->pEnd ){
				pToken--;
			}
			rc = PH7_GenCompileError(pGen,E_ERROR,pToken->nLine,"if/else/elseif: Missing ')'");
			if( rc == SXERR_ABORT ){
				/* Error count limit reached,abort immediately */
				return SXERR_ABORT;
			}
			goto Synchronize;
		}
		/* Swap token streams */
		SWAP_TOKEN_STREAM(pGen,pToken,pEnd);
		/* Compile the condition */
		rc = PH7_CompileExpr(&(*pGen),0,0);
		/* Update token stream */
		while(pGen->pIn < pEnd ){
			PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"Unexpected token '%z'",&pGen->pIn->sData);
			pGen->pIn++;
		}
		pGen->pIn  = &pEnd[1];
		pGen->pEnd = pTmp;
		if( rc == SXERR_ABORT ){
			/* Expression handler request an operation abort [i.e: Out-of-memory] */
			return SXERR_ABORT;
		}
		/* Emit the false jump */
		PH7_VmEmitInstr(pGen->pVm,PH7_OP_JZ,0,0,0,&nJumpIdx);
		/* Save the instruction index so we can fix it later when the jump destination is resolved */
		GenStateNewJumpFixup(pCondBlock,PH7_OP_JZ,nJumpIdx);
		/* Compile the body */
		rc = PH7_CompileBlock(&(*pGen),PH7_TKWRD_ENDIF);
		if( rc == SXERR_ABORT ){
			return SXERR_ABORT;
		}
		if( pGen->pIn >= pGen->pEnd || (pGen->pIn->nType & PH7_TK_KEYWORD) == 0 ){
			break;
		}
		/* Ensure that the keyword ID is 'else if' or 'else' */
		nKeyID = (sxu32)SX_PTR_TO_INT(pGen->pIn->pUserData);
		if( (nKeyID & (PH7_TKWRD_ELSE|PH7_TKWRD_ELIF)) == 0 ){
			break;
		}
		/* Emit the unconditional jump */
		PH7_VmEmitInstr(pGen->pVm,PH7_OP_JMP,0,0,0,&nJumpIdx);
		/* Save the instruction index so we can fix it later when the jump destination is resolved */
		GenStateNewJumpFixup(pCondBlock,PH7_OP_JMP,nJumpIdx);
		if( nKeyID & PH7_TKWRD_ELSE ){
			pToken = &pGen->pIn[1];
			if( pToken >= pGen->pEnd || (pToken->nType & PH7_TK_KEYWORD) == 0 ||
				SX_PTR_TO_INT(pToken->pUserData) != PH7_TKWRD_IF ){
					break;
			}
			pGen->pIn++; /* Jump the 'else' keyword */
		}
		pGen->pIn++; /* Jump the 'elseif/if' keyword */
		/* Synchronize cursors */
		pToken = pGen->pIn;
		/* Fix the false jump */
		GenStateFixJumps(pCondBlock,PH7_OP_JZ,PH7_VmInstrLength(pGen->pVm));
	} /* For(;;) */
	/* Fix the false jump */
	GenStateFixJumps(pCondBlock,PH7_OP_JZ,PH7_VmInstrLength(pGen->pVm));
	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_KEYWORD) &&
		(SX_PTR_TO_INT(pGen->pIn->pUserData) & PH7_TKWRD_ELSE) ){
			/* Compile the else block */
			pGen->pIn++;
			rc = PH7_CompileBlock(&(*pGen),PH7_TKWRD_ENDIF);
			if( rc == SXERR_ABORT ){
				
				return SXERR_ABORT;
			}
	}
	nJumpIdx = PH7_VmInstrLength(pGen->pVm);
	/* Fix all unconditional jumps now the destination is resolved */
	GenStateFixJumps(pCondBlock,PH7_OP_JMP,nJumpIdx);
	/* Release the conditional block */
	GenStateLeaveBlock(pGen,0);
	/* Statement successfully compiled */
	return SXRET_OK;
Synchronize:
	/* Synchronize with the first semi-colon ';' so we can avoid compiling this erroneous block.
	 */
	while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & (PH7_TK_SEMI|PH7_TK_OCB)) == 0 ){
		pGen->pIn++;
	}
	return SXRET_OK;
}
/*
 * Compile the global construct.
 * According to the PHP language reference
 *  In PHP global variables must be declared global inside a function if they are going
 *  to be used in that function.
 *  Example #1 Using global
 *  <?php
 *   $a = 1;
 *   $b = 2;
 *   function Sum()
 *   {
 *    global $a, $b;
 *    $b = $a + $b;
 *   } 
 *   Sum();
 *   echo $b;
 *  ?>
 *  The above script will output 3. By declaring $a and $b global within the function
 *  all references to either variable will refer to the global version. There is no limit
 *  to the number of global variables that can be manipulated by a function.
 */
static sxi32 PH7_CompileGlobal(ph7_gen_state *pGen)
{
	SyToken *pTmp,*pNext = 0;
	sxi32 nExpr;
	sxi32 rc;
	/* Jump the 'global' keyword */
	pGen->pIn++;
	if( pGen->pIn >= pGen->pEnd || (pGen->pIn->nType & PH7_TK_SEMI) ){
		/* Nothing to process */
		return SXRET_OK;
	}
	pTmp = pGen->pEnd;
	nExpr = 0;
	while( SXRET_OK == PH7_GetNextExpr(pGen->pIn,pTmp,&pNext) ){
		if( pGen->pIn < pNext ){
			pGen->pEnd = pNext;
			if( (pGen->pIn->nType & PH7_TK_DOLLAR) == 0 ){
				rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"global: Expected variable name");
				if( rc == SXERR_ABORT ){
					return SXERR_ABORT;
				}
			}else{
				pGen->pIn++;
				if( pGen->pIn >= pGen->pEnd ){
					/* Emit a warning */
					PH7_GenCompileError(&(*pGen),E_WARNING,pGen->pIn[-1].nLine,"global: Empty variable name");
				}else{
					rc = PH7_CompileExpr(&(*pGen),0,0);
					if( rc == SXERR_ABORT ){
						return SXERR_ABORT;
					}else if(rc != SXERR_EMPTY ){
						nExpr++;
					}
				}
			}
		}
		/* Next expression in the stream */
		pGen->pIn = pNext;
		/* Jump trailing commas */
		while( pGen->pIn < pTmp && (pGen->pIn->nType & PH7_TK_COMMA) ){
			pGen->pIn++;
		}
	}
	/* Restore token stream */
	pGen->pEnd = pTmp;
	if( nExpr > 0 ){
		/* Emit the uplink instruction */
		PH7_VmEmitInstr(pGen->pVm,PH7_OP_UPLINK,nExpr,0,0,0);
	}
	return SXRET_OK;
}
/*
 * Compile the return statement.
 * According to the PHP language reference
 *  If called from within a function, the return() statement immediately ends execution
 *  of the current function, and returns its argument as the value of the function call.
 *  return() will also end the execution of an eval() statement or script file.
 *  If called from the global scope, then execution of the current script file is ended.
 *  If the current script file was include()ed or require()ed, then control is passed back
 *  to the calling file. Furthermore, if the current script file was include()ed, then the value
 *  given to return() will be returned as the value of the include() call. If return() is called
 *  from within the main script file, then script execution end.
 *  Note that since return() is a language construct and not a function, the parentheses
 *  surrounding its arguments are not required. It is common to leave them out, and you actually
 *  should do so as PHP has less work to do in this case. 
 *  Note: If no parameter is supplied, then the parentheses must be omitted and NULL will be returned.
 */
static sxi32 PH7_CompileReturn(ph7_gen_state *pGen)
{
	sxi32 nRet = 0; /* TRUE if there is a return value */
	sxi32 rc;
	/* Jump the 'return' keyword */
	pGen->pIn++;
	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_SEMI) == 0 ){
		/* Compile the expression */
		rc = PH7_CompileExpr(&(*pGen),0,0);
		if( rc == SXERR_ABORT ){
			return SXERR_ABORT;
		}else if(rc != SXERR_EMPTY ){
			nRet = 1;
		}
	}
	/* Emit the done instruction */
	PH7_VmEmitInstr(pGen->pVm,PH7_OP_DONE,nRet,0,0,0);
	return SXRET_OK;
}
/*
 * Compile the die/exit language construct.
 * The role of these constructs is to terminate execution of the script.
 * Shutdown functions will always be executed even if exit() is called. 
 */
static sxi32 PH7_CompileHalt(ph7_gen_state *pGen)
{
	sxi32 nExpr = 0;
	sxi32 rc;
	/* Jump the die/exit keyword */
	pGen->pIn++;
	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_SEMI) == 0 ){
		/* Compile the expression */
		rc = PH7_CompileExpr(&(*pGen),0,0);
		if( rc == SXERR_ABORT ){
			return SXERR_ABORT;
		}else if(rc != SXERR_EMPTY ){
			nExpr = 1;
		}
	}
	/* Emit the HALT instruction */
	PH7_VmEmitInstr(pGen->pVm,PH7_OP_HALT,nExpr,0,0,0);
	return SXRET_OK;
}
/*
 * Compile the 'echo' language construct.
 */
static sxi32 PH7_CompileEcho(ph7_gen_state *pGen)
{
	SyToken *pTmp,*pNext = 0;
	sxi32 rc;
	/* Jump the 'echo' keyword */
	pGen->pIn++;
	/* Compile arguments one after one */
	pTmp = pGen->pEnd;
	while( SXRET_OK == PH7_GetNextExpr(pGen->pIn,pTmp,&pNext) ){
		if( pGen->pIn < pNext ){
			pGen->pEnd = pNext;
			rc = PH7_CompileExpr(&(*pGen),EXPR_FLAG_RDONLY_LOAD/* Do not create variable if inexistant */,0);
			if( rc == SXERR_ABORT ){
				return SXERR_ABORT;
			}else if( rc != SXERR_EMPTY ){
				/* Emit the consume instruction */
				PH7_VmEmitInstr(pGen->pVm,PH7_OP_CONSUME,1,0,0,0);
			}
		}
		/* Jump trailing commas */
		while( pNext < pTmp && (pNext->nType & PH7_TK_COMMA) ){
			pNext++;
		}
		pGen->pIn = pNext;
	}
	/* Restore token stream */
	pGen->pEnd = pTmp;	
	return SXRET_OK;
}
/*
 * Compile the static statement.
 * According to the PHP language reference
 *  Another important feature of variable scoping is the static variable.
 *  A static variable exists only in a local function scope, but it does not lose its value
 *  when program execution leaves this scope.
 *  Static variables also provide one way to deal with recursive functions.
 * Symisc eXtension.
 *  PH7 allow any complex expression to be associated with the static variable while
 *  the zend engine would allow only simple scalar value.
 *  Example
 *    static $myVar = "Welcome "." guest ".rand_str(3); //Valid under PH7/Generate error using the zend engine
 *    Refer to the official documentation for more information on this feature.
 */
static sxi32 PH7_CompileStatic(ph7_gen_state *pGen)
{
	ph7_vm_func_static_var sStatic; /* Structure describing the static variable */
	ph7_vm_func *pFunc;             /* Enclosing function */
	GenBlock *pBlock;
	SyString *pName;
	char *zDup;
	sxu32 nLine;
	sxi32 rc;
	/* Jump the static keyword */
	nLine = pGen->pIn->nLine;
	pGen->pIn++;
	/* Extract the enclosing function if any */
	pBlock = pGen->pCurrent;
	while( pBlock ){
		if( pBlock->iFlags & GEN_BLOCK_FUNC){
			break;
		}
		/* Point to the upper block */
		pBlock = pBlock->pParent;
	}
	if( pBlock == 0 ){
		/* Static statement,called outside of a function body,treat it as a simple variable. */
		if( pGen->pIn >= pGen->pEnd || (pGen->pIn->nType & PH7_TK_DOLLAR) == 0 ){
			rc = PH7_GenCompileError(&(*pGen),E_ERROR,nLine,"Expected variable after 'static' keyword");
			if( rc == SXERR_ABORT ){
				return SXERR_ABORT;
			}
			goto Synchronize;
		}
		/* Compile the expression holding the variable */
		rc = PH7_CompileExpr(&(*pGen),0,0);
		if( rc == SXERR_ABORT ){
			return SXERR_ABORT;
		}else if( rc != SXERR_EMPTY ){
			/* Emit the POP instruction */
			PH7_VmEmitInstr(pGen->pVm,PH7_OP_POP,1,0,0,0);
		}
		return SXRET_OK;
	}
	pFunc = (ph7_vm_func *)pBlock->pUserData;
	/* Make sure we are dealing with a valid statement */
	if( pGen->pIn >= pGen->pEnd || (pGen->pIn->nType & PH7_TK_DOLLAR) == 0 || &pGen->pIn[1] >= pGen->pEnd ||
		(pGen->pIn[1].nType & (PH7_TK_ID|PH7_TK_KEYWORD)) == 0 ){
			rc = PH7_GenCompileError(&(*pGen),E_ERROR,nLine,"Expected variable after 'static' keyword");
			if( rc == SXERR_ABORT ){
				return SXERR_ABORT;
			}
			goto Synchronize;
	}
	pGen->pIn++;
	/* Extract variable name */
	pName = &pGen->pIn->sData;
	pGen->pIn++; /* Jump the var name */
	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & (PH7_TK_SEMI/*';'*/|PH7_TK_EQUAL/*'='*/)) == 0 ){
		rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"static: Unexpected token '%z'",&pGen->pIn->sData);
		goto Synchronize;
	}
	/* Initialize the structure describing the static variable */
	SySetInit(&sStatic.aByteCode,&pGen->pVm->sAllocator,sizeof(VmInstr));
	sStatic.nIdx = SXU32_HIGH; /* Not yet created */
	/* Duplicate variable name */
	zDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,pName->zString,pName->nByte);
	if( zDup == 0 ){
		PH7_GenCompileError(&(*pGen),E_ERROR,nLine,"Fatal, PH7 engine is running out of memory");
		return SXERR_ABORT;
	}
	SyStringInitFromBuf(&sStatic.sName,zDup,pName->nByte);
	/* Check if we have an expression to compile */
	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_EQUAL) ){
		SySet *pInstrContainer;
		/* TICKET 1433-014: Symisc extension to the PHP programming language
		 * Static variable can take any complex expression including function
		 * call as their initialization value.
		 * Example:
		 *		static $var = foo(1,4+5,bar());
		 */
		pGen->pIn++; /* Jump the equal '=' sign */
		/* Swap bytecode container */
		pInstrContainer = PH7_VmGetByteCodeContainer(pGen->pVm);
		PH7_VmSetByteCodeContainer(pGen->pVm,&sStatic.aByteCode);
		/* Compile the expression */
		rc = PH7_CompileExpr(&(*pGen),0,0);
		/* Emit the done instruction */
		PH7_VmEmitInstr(pGen->pVm,PH7_OP_DONE,(rc != SXERR_EMPTY ? 1 : 0),0,0,0);
		/* Restore default bytecode container */
		PH7_VmSetByteCodeContainer(pGen->pVm,pInstrContainer);
	}
	/* Finally save the compiled static variable in the appropriate container */
	SySetPut(&pFunc->aStatic,(const void *)&sStatic);
	return SXRET_OK;
Synchronize:
	/* Synchronize with the first semi-colon ';',so we can avoid compiling this erroneous
	 * statement. 
	 */
	while(pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_SEMI) ==  0 ){
		pGen->pIn++;
	}
	return SXRET_OK;
}
/*
 * Compile the var statement.
 * Symisc Extension:
 *      var statement can be used outside of a class definition.
 */
static sxi32 PH7_CompileVar(ph7_gen_state *pGen)
{
	sxu32 nLine = pGen->pIn->nLine;
	sxi32 rc;
	/* Jump the 'var' keyword */
	pGen->pIn++;
	if( pGen->pIn >= pGen->pEnd || (pGen->pIn->nType & PH7_TK_DOLLAR/*'$'*/) == 0 ){
		rc = PH7_GenCompileError(&(*pGen),E_ERROR,nLine,"var: Expecting variable name");
		/* Synchronize with the first semi-colon */
		while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_SEMI/*';'*/) == 0 ){
			pGen->pIn++;
		}
		if( rc == SXERR_ABORT ){
			return SXERR_ABORT;
		}
	}else{
		/* Compile the expression */
		rc = PH7_CompileExpr(&(*pGen),0,0);
		if( rc == SXERR_ABORT ){
			return SXERR_ABORT;
		}else if( rc != SXERR_EMPTY ){
			PH7_VmEmitInstr(pGen->pVm,PH7_OP_POP,1,0,0,0);
		}
	}
	return SXRET_OK;
}
/*
 * Compile a namespace statement
 * According to the PHP language reference manual
 *  What are namespaces? In the broadest definition namespaces are a way of encapsulating items.
 *  This can be seen as an abstract concept in many places. For example, in any operating system
 *  directories serve to group related files, and act as a namespace for the files within them.
 *  As a concrete example, the file foo.txt can exist in both directory /home/greg and in /home/other
 *  but two copies of foo.txt cannot co-exist in the same directory. In addition, to access the foo.txt
 *  file outside of the /home/greg directory, we must prepend the directory name to the file name using
 *  the directory separator to get /home/greg/foo.txt. This same principle extends to namespaces in the
 *  programming world.
 *  In the PHP world, namespaces are designed to solve two problems that authors of libraries and applications
 *  encounter when creating re-usable code elements such as classes or functions:
 *  Name collisions between code you create, and internal PHP classes/functions/constants or third-party 
 *  classes/functions/constants.
 *  Ability to alias (or shorten) Extra_Long_Names designed to alleviate the first problem, improving 
 *  readability of source code.
 *  PHP Namespaces provide a way in which to group related classes, interfaces, functions and constants.
 *  Here is an example of namespace syntax in PHP: 
 *       namespace my\name; // see "Defining Namespaces" section
 *       class MyClass {}
 *       function myfunction() {}
 *       const MYCONST = 1;
 *       $a = new MyClass;
 *       $c = new \my\name\MyClass;
 *       $a = strlen('hi');
 *       $d = namespace\MYCONST;
 *       $d = __NAMESPACE__ . '\MYCONST';
 *       echo constant($d);
 * NOTE
 *  AS OF THIS VERSION NAMESPACE SUPPORT IS DISABLED. IF YOU NEED A WORKING VERSION THAT IMPLEMENT 
 *  NAMESPACE,PLEASE CONTACT SYMISC SYSTEMS VIA contact@symisc.net.
 */
static sxi32 PH7_CompileNamespace(ph7_gen_state *pGen)
{
	sxu32 nLine = pGen->pIn->nLine;
	sxi32 rc;
	pGen->pIn++; /* Jump the 'namespace' keyword */
	if( pGen->pIn >= pGen->pEnd ||
		(pGen->pIn->nType & (PH7_TK_NSSEP|PH7_TK_ID|PH7_TK_KEYWORD|PH7_TK_SEMI/*';'*/|PH7_TK_OCB/*'{'*/)) == 0 ){
			SyToken *pTok = pGen->pIn;
			if( pTok >= pGen->pEnd ){
				pTok--;
			}
			/* Unexpected token */
			rc = PH7_GenCompileError(&(*pGen),E_ERROR,nLine,"Namespace: Unexpected token '%z'",&pTok->sData);
			if( rc == SXERR_ABORT ){
				return SXERR_ABORT;
			}
	}
	/* Ignore the path */
	while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & (PH7_TK_NSSEP/*'\'*/|PH7_TK_ID|PH7_TK_KEYWORD)) ){
		pGen->pIn++;
	}
	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & (PH7_TK_SEMI/*';'*/|PH7_TK_OCB/*'{'*/)) == 0 ){
		/* Unexpected token */
		rc = PH7_GenCompileError(&(*pGen),E_ERROR,nLine,
			"Namespace: Unexpected token '%z',expecting ';' or '{'",&pGen->pIn->sData);
		if( rc == SXERR_ABORT ){
			return SXERR_ABORT;
		}
	}
	/* Emit a warning */
	PH7_GenCompileError(&(*pGen),E_WARNING,nLine,
		"Namespace support is disabled in the current release of the PH7(%s) engine",ph7_lib_version());
	return SXRET_OK;
}
/*
 * Compile the 'use' statement
 * According to the PHP language reference manual
 *  The ability to refer to an external fully qualified name with an alias or importing
 *  is an important feature of namespaces. This is similar to the ability of unix-based
 *  filesystems to create symbolic links to a file or to a directory.
 *  PHP namespaces support three kinds of aliasing or importing: aliasing a class name
 *  aliasing an interface name, and aliasing a namespace name. Note that importing
 *  a function or constant is not supported.
 *  In PHP, aliasing is accomplished with the 'use' operator.
 * NOTE
 *  AS OF THIS VERSION NAMESPACE SUPPORT IS DISABLED. IF YOU NEED A WORKING VERSION THAT IMPLEMENT 
 *  NAMESPACE,PLEASE CONTACT SYMISC SYSTEMS VIA contact@symisc.net.
 */
static sxi32 PH7_CompileUse(ph7_gen_state *pGen)
{
	sxu32 nLine = pGen->pIn->nLine;
	sxi32 rc;
	pGen->pIn++; /* Jump the 'use' keyword */
	/* Assemeble one or more real namespace path */
	for(;;){
		if( pGen->pIn >= pGen->pEnd ){
			break;
		}
		/* Ignore the path */
		while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & (PH7_TK_NSSEP|PH7_TK_ID))  ){
			pGen->pIn++;
		}
		if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_COMMA/*','*/) ){
			pGen->pIn++; /* Jump the comma and process the next path */
		}else{
			break;
		}
	}
	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_KEYWORD) && PH7_TKWRD_AS == SX_PTR_TO_INT(pGen->pIn->pUserData) ){
		pGen->pIn++; /* Jump the 'as' keyword */
		/* Compile one or more aliasses */
		for(;;){
			if( pGen->pIn >= pGen->pEnd ){
				break;
			}
			while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & (PH7_TK_NSSEP|PH7_TK_ID)) ){
				pGen->pIn++;
			}
			if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_COMMA/*','*/) ){
				pGen->pIn++; /* Jump the comma and process the next alias */
			}else{
				break;
			}
		}
	}
	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_SEMI/*';'*/) == 0 ){
		/* Unexpected token */
		rc = PH7_GenCompileError(&(*pGen),E_ERROR,nLine,"use statement: Unexpected token '%z',expecting ';'",
			&pGen->pIn->sData);
		if( rc == SXERR_ABORT ){
			return SXERR_ABORT;
		}
	}
	/* Emit a notice */
	PH7_GenCompileError(&(*pGen),E_NOTICE,nLine,
		"Namespace support is disabled in the current release of the PH7(%s) engine",
		ph7_lib_version()
		);
	return SXRET_OK;
}
/*
 * Compile the stupid 'declare' language construct.
 *
 * According to the PHP language reference manual.
 *  The declare construct is used to set execution directives for a block of code.
 *  The syntax of declare is similar to the syntax of other flow control constructs:
 *  declare (directive)
 *   statement
 * The directive section allows the behavior of the declare block to be set.
 *  Currently only two directives are recognized: the ticks directive and the encoding directive.
 * The statement part of the declare block will be executed - how it is executed and what side
 * effects occur during execution may depend on the directive set in the directive block.
 * The declare construct can also be used in the global scope, affecting all code following
 * it (however if the file with declare was included then it does not affect the parent file).
 * <?php
 * // these are the same:
 * // you can use this:
 * declare(ticks=1) {
 *   // entire script here
 * }
 * // or you can use this:
 * declare(ticks=1);
 * // entire script here
 * ?>
 *
 * Well,actually this language construct is a NO-OP in the current release of the PH7 engine.
 */
static sxi32 PH7_CompileDeclare(ph7_gen_state *pGen)
{
	sxu32 nLine = pGen->pIn->nLine;
	SyToken *pEnd = 0; /* cc warning */
	sxi32 rc;
	pGen->pIn++; /* Jump the 'declare' keyword */
	if( pGen->pIn >= pGen->pEnd || (pGen->pIn->nType & PH7_TK_LPAREN) == 0 /*'('*/ ){
		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"declare: Expecting opening parenthesis '('");
		if( rc == SXERR_ABORT ){
			return SXERR_ABORT;
		}
		goto Synchro;
	}
	pGen->pIn++; /* Jump the left parenthesis */
	/* Delimit the directive */
	PH7_DelimitNestedTokens(pGen->pIn,pGen->pEnd,PH7_TK_LPAREN/*'('*/,PH7_TK_RPAREN/*')'*/,&pEnd);
	if( pEnd >= pGen->pEnd ){
		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"declare: Missing closing parenthesis ')'");
		if( rc == SXERR_ABORT ){
			return SXERR_ABORT;
		}
		return SXRET_OK;
	}
	/* Update the cursor */
	pGen->pIn = &pEnd[1];
	if( pGen->pIn >= pGen->pEnd || (pGen->pIn->nType & (PH7_TK_SEMI/*';'*/|PH7_TK_OCB/*'{'*/)) == 0  ){
		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"declare: Expecting ';' or '{' after directive");
		if( rc == SXERR_ABORT ){
			return SXERR_ABORT;
		}
	}
	/* TICKET 1433-81: This construct is disabled in the current release of the PH7 engine. */
	PH7_GenCompileError(&(*pGen),E_NOTICE,nLine, /* Emit a notice */
		"the declare construct is a no-op in the current release of the PH7(%s) engine",
		ph7_lib_version()
		);
	/*All done */
	return SXRET_OK;
Synchro:
	/* Sycnhronize with the first semi-colon ';' or curly braces '{' */
	while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & (PH7_TK_SEMI/*';'*/|PH7_TK_OCB/*'{'*/)) == 0 ){
		pGen->pIn++;
	}
	return SXRET_OK;
}
/*
 * Process default argument values. That is,a function may define C++-style default value
 * as follows:
 * function makecoffee($type = "cappuccino")
 * {
 *   return "Making a cup of $type.\n";
 * }
 * Symisc eXtension.
 *  1 -) Default arguments value can be any complex expression [i.e: function call,annynoymous
 *      functions,array member,..] unlike the zend which would allow only single scalar value.
 *      Example: Work only with PH7,generate error under zend
 *      function test($a = 'Hello'.'World: '.rand_str(3))
 *      {
 *       var_dump($a);
 *      }
 *     //call test without args
 *      test(); 
 * 2 -) Full type hinting: (Arguments are automatically casted to the desired type)
 *      Example:
 *           function a(string $a){} function b(int $a,string $c,float $d){}
 * 3 -) Function overloading!!
 *      Example:
 *      function foo($a) {
 *   	  return $a.PHP_EOL;
 *	    }
 *	    function foo($a, $b) {
 *   	  return $a + $b;
 *	    }
 *	    echo foo(5); // Prints "5"
 *	    echo foo(5, 2); // Prints "7"
 *      // Same arg
 *	   function foo(string $a)
 *	   {
 *	     echo "a is a string\n";
 *	     var_dump($a);
 *	   }
 *	  function foo(int $a)
 *	  {
 *	    echo "a is integer\n";
 *	    var_dump($a);
 *	  }
 *	  function foo(array $a)
 *	  {
 * 	    echo "a is an array\n";
 * 	    var_dump($a);
 *	  }
 *	  foo('This is a great feature'); // a is a string [first foo]
 *	  foo(52); // a is integer [second foo] 
 *    foo(array(14,__TIME__,__DATE__)); // a is an array [third foo]
 * Please refer to the official documentation for more information on the powerful extension
 * introduced by the PH7 engine.
 */
static sxi32 GenStateProcessArgValue(ph7_gen_state *pGen,ph7_vm_func_arg *pArg,SyToken *pIn,SyToken *pEnd)
{
	SyToken *pTmpIn,*pTmpEnd;
	SySet *pInstrContainer;
	sxi32 rc;
	/* Swap token stream */
	SWAP_DELIMITER(pGen,pIn,pEnd);
	pInstrContainer = PH7_VmGetByteCodeContainer(pGen->pVm);
	PH7_VmSetByteCodeContainer(pGen->pVm,&pArg->aByteCode);
	/* Compile the expression holding the argument value */
	rc = PH7_CompileExpr(&(*pGen),0,0);
	/* Emit the done instruction */
	PH7_VmEmitInstr(pGen->pVm,PH7_OP_DONE,(rc != SXERR_EMPTY ? 1 : 0),0,0,0);
	PH7_VmSetByteCodeContainer(pGen->pVm,pInstrContainer); 
	RE_SWAP_DELIMITER(pGen);
	if( rc == SXERR_ABORT ){
		return SXERR_ABORT;
	}
	return SXRET_OK;
}
/*
 * Collect function arguments one after one.
 * According to the PHP language reference manual.
 * Information may be passed to functions via the argument list, which is a comma-delimited
 * list of expressions.
 * PHP supports passing arguments by value (the default), passing by reference
 * and default argument values. Variable-length argument lists are also supported,
 * see also the function references for func_num_args(), func_get_arg(), and func_get_args()
 * for more information.
 * Example #1 Passing arrays to functions
 * <?php
 * function takes_array($input)
 * {
 *    echo "$input[0] + $input[1] = ", $input[0]+$input[1];
 * }
 * ?>
 * Making arguments be passed by reference
 * By default, function arguments are passed by value (so that if the value of the argument
 * within the function is changed, it does not get changed outside of the function).
 * To allow a function to modify its arguments, they must be passed by reference.
 * To have an argument to a function always passed by reference, prepend an ampersand (&)
 * to the argument name in the function definition:
 * Example #2 Passing function parameters by reference
 * <?php
 * function add_some_extra(&$string)
 * {
 *   $string .= 'and something extra.';
 * }
 * $str = 'This is a string, ';
 * add_some_extra($str);
 * echo $str;    // outputs 'This is a string, and something extra.'
 * ?>
 *
 * PH7 have introduced powerful extension including full type hinting,function overloading
 * complex agrument values.Please refer to the official documentation for more information
 * on these extension.
 */
static sxi32 GenStateCollectFuncArgs(ph7_vm_func *pFunc,ph7_gen_state *pGen,SyToken *pEnd)
{
	ph7_vm_func_arg sArg; /* Current processed argument */
	SyToken *pCur,*pIn;  /* Token stream */
	SyBlob sSig;         /* Function signature */
	char *zDup;          /* Copy of argument name */
	sxi32 rc;

	pIn = pGen->pIn;
	pCur = 0;
	SyBlobInit(&sSig,&pGen->pVm->sAllocator);
	/* Process arguments one after one */
	for(;;){
		if( pIn >= pEnd ){
			/* No more arguments to process */
			break;
		}
		SyZero(&sArg,sizeof(ph7_vm_func_arg));
		SySetInit(&sArg.aByteCode,&pGen->pVm->sAllocator,sizeof(VmInstr));
		if( pIn->nType & (PH7_TK_ID|PH7_TK_KEYWORD) ){
			if( pIn->nType & PH7_TK_KEYWORD ){
				sxu32 nKey = (sxu32)(SX_PTR_TO_INT(pIn->pUserData));
				if( nKey & PH7_TKWRD_ARRAY ){
					sArg.nType = MEMOBJ_HASHMAP;
				}else if( nKey & PH7_TKWRD_BOOL ){
					sArg.nType = MEMOBJ_BOOL;
				}else if( nKey & PH7_TKWRD_INT ){
					sArg.nType = MEMOBJ_INT;
				}else if( nKey & PH7_TKWRD_STRING ){
					sArg.nType = MEMOBJ_STRING;
				}else if( nKey & PH7_TKWRD_FLOAT ){
					sArg.nType = MEMOBJ_REAL;
				}else{
					PH7_GenCompileError(&(*pGen),E_WARNING,pGen->pIn->nLine,
						"Invalid argument type '%z',Automatic cast will not be performed",
						&pIn->sData);
				}
			}else{
				SyString *pName = &pIn->sData; /* Class name */
				char *zDup;
				/* Argument must be a class instance,record that*/
				zDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,pName->zString,pName->nByte);
				if( zDup ){
					sArg.nType = SXU32_HIGH; /* 0xFFFFFFFF as sentinel */
					SyStringInitFromBuf(&sArg.sClass,zDup,pName->nByte);
				}
			}
			pIn++;
		}
		if( pIn >= pEnd ){
			rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"Missing argument name");
			return rc;
		}
		if( pIn->nType & PH7_TK_AMPER ){
			/* Pass by reference,record that */
			sArg.iFlags = VM_FUNC_ARG_BY_REF;
			pIn++;
		}
		if( pIn >= pEnd || (pIn->nType & PH7_TK_DOLLAR) == 0 || &pIn[1] >= pEnd || (pIn[1].nType & (PH7_TK_ID|PH7_TK_KEYWORD)) == 0 ){
			/* Invalid argument */ 
			rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"Invalid argument name");
			return rc;
		}
		pIn++; /* Jump the dollar sign */
		/* Copy argument name */
		zDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,SyStringData(&pIn->sData),SyStringLength(&pIn->sData));
		if( zDup == 0 ){
			PH7_GenCompileError(&(*pGen),E_ERROR,pIn->nLine,"PH7 engine is running out of memory");
			return SXERR_ABORT;
		}
		SyStringInitFromBuf(&sArg.sName,zDup,SyStringLength(&pIn->sData));
		pIn++;
		if( pIn < pEnd ){
			if( pIn->nType & PH7_TK_EQUAL ){
				SyToken *pDefend;
				sxi32 iNest = 0;
				pIn++; /* Jump the equal sign */
				pDefend = pIn;
				/* Process the default value associated with this argument */
				while( pDefend < pEnd ){
					if( (pDefend->nType & PH7_TK_COMMA) && iNest <= 0 ){
						break;
					}
					if( pDefend->nType & (PH7_TK_LPAREN/*'('*/|PH7_TK_OCB/*'{'*/|PH7_TK_OSB/*[*/) ){
						/* Increment nesting level */
						iNest++;
					}else if( pDefend->nType & (PH7_TK_RPAREN/*')'*/|PH7_TK_CCB/*'}'*/|PH7_TK_CSB/*]*/) ){
						/* Decrement nesting level */
						iNest--;
					}
					pDefend++;
				}
				if( pIn >= pDefend ){
					rc = PH7_GenCompileError(&(*pGen),E_ERROR,pIn->nLine,"Missing argument default value");
					return rc;
				}
				/* Process default value */
				rc = GenStateProcessArgValue(&(*pGen),&sArg,pIn,pDefend);
				if( rc != SXRET_OK ){
					return rc;
				}
				/* Point beyond the default value */
				pIn = pDefend;
			}
			if( pIn < pEnd && (pIn->nType & PH7_TK_COMMA) == 0 ){
				rc = PH7_GenCompileError(&(*pGen),E_ERROR,pIn->nLine,"Unexpected token '%z'",&pIn->sData);
				return rc;
			}
			pIn++; /* Jump the trailing comma */
		}
		/* Append argument signature */
		if( sArg.nType > 0 ){
			if( SyStringLength(&sArg.sClass) > 0 ){
				/* Class name */
				SyBlobAppend(&sSig,SyStringData(&sArg.sClass),SyStringLength(&sArg.sClass));
			}else{
				int c;
				c = 'n'; /* cc warning */
				/* Type leading character */
				switch(sArg.nType){
				case MEMOBJ_HASHMAP:
					/* Hashmap aka 'array' */
					c = 'h';
					break;
				case MEMOBJ_INT:
					/* Integer */
					c = 'i';
					break;
				case MEMOBJ_BOOL:
					/* Bool */
					c = 'b';
					break;
				case MEMOBJ_REAL:
					/* Float */
					c = 'f';
					break;
				case MEMOBJ_STRING:
					/* String */
					c = 's';
					break;
				default:
					break;
				}
				SyBlobAppend(&sSig,(const void *)&c,sizeof(char));
			}
		}else{
			/* No type is associated with this parameter which mean
			 * that this function is not condidate for overloading. 
			 */
			SyBlobRelease(&sSig);
		}
		/* Save in the argument set */
		SySetPut(&pFunc->aArgs,(const void *)&sArg);
	}
	if( SyBlobLength(&sSig) > 0 ){
		/* Save function signature */
		SyStringInitFromBuf(&pFunc->sSignature,SyBlobData(&sSig),SyBlobLength(&sSig));
	}
	return SXRET_OK;
}
/*
 * Compile function [i.e: standard function, annonymous function or closure ] body.
 * Return SXRET_OK on success. Any other return value indicates failure
 * and this routine takes care of generating the appropriate error message.
 */
static sxi32 GenStateCompileFuncBody(
	ph7_gen_state *pGen,  /* Code generator state */
	ph7_vm_func *pFunc    /* Function state */
	)
{
	SySet *pInstrContainer; /* Instruction container */
	GenBlock *pBlock;
	sxu32 nGotoOfft;
	sxi32 rc;
	/* Attach the new function */
	rc = GenStateEnterBlock(&(*pGen),GEN_BLOCK_PROTECTED|GEN_BLOCK_FUNC,PH7_VmInstrLength(pGen->pVm),pFunc,&pBlock);
	if( rc != SXRET_OK ){
		PH7_GenCompileError(&(*pGen),E_ERROR,1,"PH7 engine is running out-of-memory");
		/* Don't worry about freeing memory, everything will be released shortly */
		return SXERR_ABORT;
	}
	nGotoOfft = SySetUsed(&pGen->aGoto);
	/* Swap bytecode containers */
	pInstrContainer = PH7_VmGetByteCodeContainer(pGen->pVm);
	PH7_VmSetByteCodeContainer(pGen->pVm,&pFunc->aByteCode);
	/* Compile the body */
	PH7_CompileBlock(&(*pGen),0);
	/* Fix exception jumps now the destination is resolved */
	GenStateFixJumps(pGen->pCurrent,PH7_OP_THROW,PH7_VmInstrLength(pGen->pVm));
	/* Emit the final return if not yet done */
	PH7_VmEmitInstr(pGen->pVm,PH7_OP_DONE,0,0,0,0);
	/* Fix gotos jumps now the destination is resolved */
	if( SXERR_ABORT == GenStateFixGoto(&(*pGen),nGotoOfft) ){
		rc = SXERR_ABORT;
	}
	SySetTruncate(&pGen->aGoto,nGotoOfft);
	/* Restore the default container */
	PH7_VmSetByteCodeContainer(pGen->pVm,pInstrContainer);
	/* Leave function block */
	GenStateLeaveBlock(&(*pGen),0);
	if( rc == SXERR_ABORT ){
		/* Don't worry about freeing memory, everything will be released shortly */
		return SXERR_ABORT;
	}
	/* All done, function body compiled */
	return SXRET_OK;
}
/*
 * Compile a PHP function whether is a Standard or Annonymous function.
 * According to the PHP language reference manual.
 *  Function names follow the same rules as other labels in PHP. A valid function name
 *  starts with a letter or underscore, followed by any number of letters, numbers, or
 *  underscores. As a regular expression, it would be expressed thus:
 *     [a-zA-Z_\x7f-\xff][a-zA-Z0-9_\x7f-\xff]*. 
 *  Functions need not be defined before they are referenced.
 *  All functions and classes in PHP have the global scope - they can be called outside
 *  a function even if they were defined inside and vice versa.
 *  It is possible to call recursive functions in PHP. However avoid recursive function/method
 *  calls with over 32-64 recursion levels. 
 * 
 * PH7 have introduced powerful extension including full type hinting, function overloading, 
 * complex agrument values and more. Please refer to the official documentation for more information
 * on these extension.
 */
static sxi32 GenStateCompileFunc(
	ph7_gen_state *pGen, /* Code generator state */
	SyString *pName,     /* Function name. NULL otherwise */
	sxi32 iFlags,        /* Control flags */
	int bHandleClosure,  /* TRUE if we are dealing with a closure */
	ph7_vm_func **ppFunc /* OUT: function state */
	)
{
	ph7_vm_func *pFunc;
	SyToken *pEnd;
	sxu32 nLine;
	char *zName;
	sxi32 rc;
	/* Extract line number */
	nLine = pGen->pIn->nLine;
	/* Jump the left parenthesis '(' */
	pGen->pIn++;
	/* Delimit the function signature */
	PH7_DelimitNestedTokens(pGen->pIn,pGen->pEnd,PH7_TK_LPAREN /* '(' */,PH7_TK_RPAREN /* ')' */,&pEnd);
	if( pEnd >= pGen->pEnd ){
		/* Syntax error */
		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Missing ')' after function '%z' signature",pName);
		if( rc == SXERR_ABORT ){
			/* Error count limit reached,abort immediately */
			return SXERR_ABORT;
		}
		pGen->pIn = pGen->pEnd;
		return SXRET_OK;
	}
	/* Create the function state */
	pFunc = (ph7_vm_func *)SyMemBackendPoolAlloc(&pGen->pVm->sAllocator,sizeof(ph7_vm_func));
	if( pFunc == 0 ){
		goto OutOfMem;
	}
	/* function ID */
	zName = SyMemBackendStrDup(&pGen->pVm->sAllocator,pName->zString,pName->nByte);
	if( zName == 0 ){
		/* Don't worry about freeing memory, everything will be released shortly */
		goto OutOfMem;
	}
	/* Initialize the function state */
	PH7_VmInitFuncState(pGen->pVm,pFunc,zName,pName->nByte,iFlags,0);
	if( pGen->pIn < pEnd ){
		/* Collect function arguments */
		rc = GenStateCollectFuncArgs(pFunc,&(*pGen),pEnd);
		if( rc == SXERR_ABORT ){
			/* Don't worry about freeing memory, everything will be released shortly */
			return SXERR_ABORT;
		}
	}
	/* Compile function body */
	pGen->pIn = &pEnd[1];
	if( bHandleClosure ){
		ph7_vm_func_closure_env sEnv;
		int got_this = 0; /* TRUE if $this have been seen */
		if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_KEYWORD) 
			&& SX_PTR_TO_INT(pGen->pIn->pUserData) == PH7_TKWRD_USE ){
				sxu32 nLine = pGen->pIn->nLine;
				/* Closure,record environment variable */
				pGen->pIn++;
				if( pGen->pIn >= pGen->pEnd || (pGen->pIn->nType & PH7_TK_LPAREN) == 0 ){
					rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Closure: Unexpected token. Expecting a left parenthesis '('");
					if( rc == SXERR_ABORT ){
						return SXERR_ABORT;
					}
				}
				pGen->pIn++; /* Jump the left parenthesis or any other unexpected token */
				/* Compile until we hit the first closing parenthesis */
				while( pGen->pIn < pGen->pEnd ){
					int iFlags = 0;
					if( pGen->pIn->nType & PH7_TK_RPAREN ){
						pGen->pIn++; /* Jump the closing parenthesis */
						break;
					}
					nLine = pGen->pIn->nLine;
					if( pGen->pIn->nType & PH7_TK_AMPER ){
						/* Pass by reference,record that */
						PH7_GenCompileError(pGen,E_WARNING,nLine,
							"Closure: Pass by reference is disabled in the current release of the PH7 engine,PH7 is switching to pass by value"
							);
						iFlags = VM_FUNC_ARG_BY_REF;
						pGen->pIn++;
					}
					if( pGen->pIn >= pGen->pEnd || (pGen->pIn->nType & PH7_TK_DOLLAR) == 0 || &pGen->pIn[1] >= pGen->pEnd
						|| (pGen->pIn[1].nType & (PH7_TK_ID|PH7_TK_KEYWORD)) == 0 ){
							rc = PH7_GenCompileError(pGen,E_ERROR,nLine,
								"Closure: Unexpected token. Expecting a variable name");
							if( rc == SXERR_ABORT ){
								return SXERR_ABORT;
							}
							/* Find the closing parenthesis */
							while( (pGen->pIn < pGen->pEnd) && (pGen->pIn->nType & PH7_TK_RPAREN) == 0 ){
								pGen->pIn++;
							}
							if(pGen->pIn < pGen->pEnd){
								pGen->pIn++;
							}
							break;
							/* TICKET 1433-95: No need for the else block below.*/
					}else{
						SyString *pName; 
						char *zDup;
						/* Duplicate variable name */
						pName = &pGen->pIn[1].sData;
						zDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,pName->zString,pName->nByte);
						if( zDup ){
							/* Zero the structure */
							SyZero(&sEnv,sizeof(ph7_vm_func_closure_env));
							sEnv.iFlags = iFlags;
							PH7_MemObjInit(pGen->pVm,&sEnv.sValue);
							SyStringInitFromBuf(&sEnv.sName,zDup,pName->nByte);
							if( !got_this && pName->nByte == sizeof("this")-1 && 
								SyMemcmp((const void *)zDup,(const void *)"this",sizeof("this")-1) == 0 ){
									got_this = 1;
							}
							/* Save imported variable */
							SySetPut(&pFunc->aClosureEnv,(const void *)&sEnv);
						}else{
							 PH7_GenCompileError(pGen,E_ERROR,nLine,"Fatal, PH7 is running out of memory");
							 return SXERR_ABORT;
						}
					}
					pGen->pIn += 2; /* $ + variable name or any other unexpected token */
					while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_COMMA /*','*/) ){
						/* Ignore trailing commas */
						pGen->pIn++;
					}
				}
				if( !got_this ){
					/* Make the $this variable [Current processed Object (class instance)]  
					 * available to the closure environment.
					 */
					SyZero(&sEnv,sizeof(ph7_vm_func_closure_env));
					sEnv.iFlags = VM_FUNC_ARG_IGNORE; /* Do not install if NULL */
					PH7_MemObjInit(pGen->pVm,&sEnv.sValue);
					SyStringInitFromBuf(&sEnv.sName,"this",sizeof("this")-1);
					SySetPut(&pFunc->aClosureEnv,(const void *)&sEnv);
				}
				if( SySetUsed(&pFunc->aClosureEnv) > 0 ){
					/* Mark as closure */
					pFunc->iFlags |= VM_FUNC_CLOSURE;
				}
		}
	}
	/* Compile the body */
	rc = GenStateCompileFuncBody(&(*pGen),pFunc);
	if( rc == SXERR_ABORT ){
		return SXERR_ABORT;
	}
	if( ppFunc ){
		*ppFunc = pFunc;
	}
	rc = SXRET_OK;
	if( (pFunc->iFlags & VM_FUNC_CLOSURE) == 0 ){
		/* Finally register the function */
		rc = PH7_VmInstallUserFunction(pGen->pVm,pFunc,0);
	}
	if( rc == SXRET_OK ){
		return SXRET_OK;
	}
	/* Fall through if something goes wrong */
OutOfMem:
	/* If the supplied memory subsystem is so sick that we are unable to allocate
	 * a tiny chunk of memory, there is no much we can do here.
	 */
	PH7_GenCompileError(&(*pGen),E_ERROR,1,"Fatal, PH7 engine is running out-of-memory");
	return SXERR_ABORT;
}
/*
 * Compile a standard PHP function.
 *  Refer to the block-comment above for more information.
 */
static sxi32 PH7_CompileFunction(ph7_gen_state *pGen)
{
	SyString *pName;
	sxi32 iFlags;
	sxu32 nLine;
	sxi32 rc;

	nLine = pGen->pIn->nLine;
	pGen->pIn++; /* Jump the 'function' keyword */
	iFlags = 0;
	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_AMPER) ){
		/* Return by reference,remember that */
		iFlags |= VM_FUNC_REF_RETURN;
		/* Jump the '&' token */
		pGen->pIn++;
	}
	if( pGen->pIn >= pGen->pEnd || (pGen->pIn->nType & (PH7_TK_ID|PH7_TK_KEYWORD)) == 0 ){
		/* Invalid function name */
		rc = PH7_GenCompileError(&(*pGen),E_ERROR,nLine,"Invalid function name");
		if( rc == SXERR_ABORT ){
			return SXERR_ABORT;
		}
		/* Sychronize with the next semi-colon or braces*/
		while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & (PH7_TK_SEMI|PH7_TK_OCB)) == 0 ){
			pGen->pIn++;
		}
		return SXRET_OK;
	}
	pName = &pGen->pIn->sData;
	nLine = pGen->pIn->nLine;
	/* Jump the function name */
	pGen->pIn++;
	if( pGen->pIn >= pGen->pEnd || (pGen->pIn->nType & PH7_TK_LPAREN) == 0 ){
		/* Syntax error */
		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Expected '(' after function name '%z'",pName);
		if( rc == SXERR_ABORT ){
			/* Error count limit reached,abort immediately */
			return SXERR_ABORT;
		}
		/* Sychronize with the next semi-colon or '{' */
		while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & (PH7_TK_SEMI|PH7_TK_OCB)) == 0 ){
			pGen->pIn++;
		}
		return SXRET_OK;
	}
	/* Compile function body */
	rc = GenStateCompileFunc(&(*pGen),pName,iFlags,FALSE,0);
	return rc;
}
/*
 * Extract the visibility level associated with a given keyword.
 * According to the PHP language reference manual
 *  Visibility:
 *  The visibility of a property or method can be defined by prefixing
 *  the declaration with the keywords public, protected or private.
 *  Class members declared public can be accessed everywhere.
 *  Members declared protected can be accessed only within the class
 *  itself and by inherited and parent classes. Members declared as private
 *  may only be accessed by the class that defines the member. 
 */
static sxi32 GetProtectionLevel(sxi32 nKeyword)
{
	if( nKeyword == PH7_TKWRD_PRIVATE ){
		return PH7_CLASS_PROT_PRIVATE;
	}else if( nKeyword == PH7_TKWRD_PROTECTED ){
		return PH7_CLASS_PROT_PROTECTED;
	}
	/* Assume public by default */
	return PH7_CLASS_PROT_PUBLIC;
}
/*
 * Compile a class constant.
 * According to the PHP language reference manual
 *  Class Constants
 *   It is possible to define constant values on a per-class basis remaining 
 *   the same and unchangeable. Constants differ from normal variables in that
 *   you don't use the $ symbol to declare or use them.
 *   The value must be a constant expression, not (for example) a variable,
 *   a property, a result of a mathematical operation, or a function call.
 *   It's also possible for interfaces to have constants. 
 * Symisc eXtension.
 *  PH7 allow any complex expression to be associated with the constant while
 *  the zend engine would allow only simple scalar value.
 *  Example: 
 *   class Test{
 *        const MyConst = "Hello"."world: ".rand_str(3); //concatenation operation + Function call
 *   };
 *   var_dump(TEST::MyConst);
 *   Refer to the official documentation for more information on the powerful extension
 *   introduced by the PH7 engine to the OO subsystem.
 */
static sxi32 GenStateCompileClassConstant(ph7_gen_state *pGen,sxi32 iProtection,sxi32 iFlags,ph7_class *pClass)
{
	sxu32 nLine = pGen->pIn->nLine;
	SySet *pInstrContainer;
	ph7_class_attr *pCons;
	SyString *pName;
	sxi32 rc;
	/* Extract visibility level */
	iProtection = GetProtectionLevel(iProtection);
	pGen->pIn++; /* Jump the 'const' keyword */
loop:
	/* Mark as constant */
	iFlags |= PH7_CLASS_ATTR_CONSTANT;
	if( pGen->pIn >= pGen->pEnd || (pGen->pIn->nType & PH7_TK_ID) == 0 ){
		/* Invalid constant name */
		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Invalid constant name");
		if( rc == SXERR_ABORT ){
			/* Error count limit reached,abort immediately */
			return SXERR_ABORT;
		}
		goto Synchronize;
	}
	/* Peek constant name */
	pName = &pGen->pIn->sData;
	/* Make sure the constant name isn't reserved */
	if( GenStateIsReservedConstant(pName) ){
		/* Reserved constant name */
		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Cannot redeclare a reserved constant '%z'",pName);
		if( rc == SXERR_ABORT ){
			/* Error count limit reached,abort immediately */
			return SXERR_ABORT;
		}
		goto Synchronize;
	}
	/* Advance the stream cursor */
	pGen->pIn++;
	if(pGen->pIn >= pGen->pEnd || (pGen->pIn->nType & PH7_TK_EQUAL /* '=' */) == 0 ){
		/* Invalid declaration */
		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Expected '=' after class constant %z'",pName);
		if( rc == SXERR_ABORT ){
			/* Error count limit reached,abort immediately */
			return SXERR_ABORT;
		}
		goto Synchronize;
	}
	pGen->pIn++; /* Jump the equal sign */
	/* Allocate a new class attribute */
	pCons = PH7_NewClassAttr(pGen->pVm,pName,nLine,iProtection,iFlags);
	if( pCons == 0 ){
		PH7_GenCompileError(pGen,E_ERROR,nLine,"Fatal, PH7 is running out of memory");
		return SXERR_ABORT;
	}
	/* Swap bytecode container */
	pInstrContainer = PH7_VmGetByteCodeContainer(pGen->pVm);
	PH7_VmSetByteCodeContainer(pGen->pVm,&pCons->aByteCode);
	/* Compile constant value.
	 */
	rc = PH7_CompileExpr(&(*pGen),EXPR_FLAG_COMMA_STATEMENT,0);
	if( rc == SXERR_EMPTY ){
		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Empty constant '%z' value",pName);
		if( rc == SXERR_ABORT ){
			return SXERR_ABORT;
		}
	}
	/* Emit the done instruction */
	PH7_VmEmitInstr(pGen->pVm,PH7_OP_DONE,1,0,0,0);
	PH7_VmSetByteCodeContainer(pGen->pVm,pInstrContainer); 
	if( rc == SXERR_ABORT ){
		/* Don't worry about freeing memory, everything will be released shortly */
		return SXERR_ABORT;
	}
	/* All done,install the constant */
	rc = PH7_ClassInstallAttr(pClass,pCons);
	if( rc != SXRET_OK ){
		PH7_GenCompileError(pGen,E_ERROR,nLine,"Fatal, PH7 is running out of memory");
		return SXERR_ABORT;
	}
	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_COMMA /*','*/) ){
		/* Multiple constants declarations [i.e: const min=-1,max = 10] */
		pGen->pIn++; /* Jump the comma */
		if( pGen->pIn >= pGen->pEnd || (pGen->pIn->nType & PH7_TK_ID) == 0 ){
			SyToken *pTok = pGen->pIn;
			if( pTok >= pGen->pEnd ){
				pTok--;
			}
			rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,
				"Unexpected token '%z',expecting constant declaration inside class '%z'",
				&pTok->sData,&pClass->sName);
			if( rc == SXERR_ABORT ){
				return SXERR_ABORT;
			}
		}else{
			if( pGen->pIn->nType & PH7_TK_ID ){
				goto loop;
			}
		}
	}
	return SXRET_OK;
Synchronize:
	/* Synchronize with the first semi-colon */
	while(pGen->pIn < pGen->pEnd && ((pGen->pIn->nType & PH7_TK_SEMI/*';'*/) == 0) ){
		pGen->pIn++;
	}
	return SXERR_CORRUPT;
}
/*
 * complie a class attribute or Properties in the PHP jargon.
 * According to the PHP language reference manual
 *  Properties
 *  Class member variables are called "properties". You may also see them referred
 *  to using other terms such as "attributes" or "fields", but for the purposes
 *  of this reference we will use "properties". They are defined by using one
 *  of the keywords public, protected, or private, followed by a normal variable
 *  declaration. This declaration may include an initialization, but this initialization
 *  must be a constant value--that is, it must be able to be evaluated at compile time
 *  and must not depend on run-time information in order to be evaluated. 
 * Symisc eXtension.
 *  PH7 allow any complex expression to be associated with the attribute while
 *  the zend engine would allow only simple scalar value.
 *  Example: 
 *   class Test{
 *        public static $myVar = "Hello"."world: ".rand_str(3); //concatenation operation + Function call
 *   };
 *   var_dump(TEST::myVar);
 *   Refer to the official documentation for more information on the powerful extension
 *   introduced by the PH7 engine to the OO subsystem.
 */
static sxi32 GenStateCompileClassAttr(ph7_gen_state *pGen,sxi32 iProtection,sxi32 iFlags,ph7_class *pClass)
{
	sxu32 nLine = pGen->pIn->nLine;
	ph7_class_attr *pAttr;
	SyString *pName;
	sxi32 rc;
	/* Extract visibility level */
	iProtection = GetProtectionLevel(iProtection);
loop:
	pGen->pIn++; /* Jump the dollar sign */
	if( pGen->pIn >= pGen->pEnd || (pGen->pIn->nType & (PH7_TK_KEYWORD|PH7_TK_ID)) == 0 ){
		/* Invalid attribute name */
		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Invalid attribute name");
		if( rc == SXERR_ABORT ){
			/* Error count limit reached,abort immediately */
			return SXERR_ABORT;
		}
		goto Synchronize;
	}
	/* Peek attribute name */
	pName = &pGen->pIn->sData;
	/* Advance the stream cursor */
	pGen->pIn++;
	if(pGen->pIn >= pGen->pEnd || (pGen->pIn->nType & (PH7_TK_EQUAL/*'='*/|PH7_TK_SEMI/*';'*/|PH7_TK_COMMA/*','*/)) == 0 ){
		/* Invalid declaration */
		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Expected '=' or ';' after attribute name '%z'",pName);
		if( rc == SXERR_ABORT ){
			/* Error count limit reached,abort immediately */
			return SXERR_ABORT;
		}
		goto Synchronize;
	}
	/* Allocate a new class attribute */
	pAttr = PH7_NewClassAttr(pGen->pVm,pName,nLine,iProtection,iFlags);
	if( pAttr == 0 ){
		PH7_GenCompileError(pGen,E_ERROR,nLine,"Fatal, PH7 engine is running out of memory");
		return SXERR_ABORT;
	}
	if( pGen->pIn->nType & PH7_TK_EQUAL /*'='*/ ){
		SySet *pInstrContainer;
		pGen->pIn++; /*Jump the equal sign */
		/* Swap bytecode container */
		pInstrContainer = PH7_VmGetByteCodeContainer(pGen->pVm);
		PH7_VmSetByteCodeContainer(pGen->pVm,&pAttr->aByteCode);
		/* Compile attribute value.
		 */
		rc = PH7_CompileExpr(&(*pGen),EXPR_FLAG_COMMA_STATEMENT,0);
		if( rc == SXERR_EMPTY ){
			rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Attribute '%z': Missing default value",pName);
			if( rc == SXERR_ABORT ){
				return SXERR_ABORT;
			}
		}
		/* Emit the done instruction */
		PH7_VmEmitInstr(pGen->pVm,PH7_OP_DONE,1,0,0,0);
		PH7_VmSetByteCodeContainer(pGen->pVm,pInstrContainer);
	}
	/* All done,install the attribute */
	rc = PH7_ClassInstallAttr(pClass,pAttr);
	if( rc != SXRET_OK ){
		PH7_GenCompileError(pGen,E_ERROR,nLine,"Fatal, PH7 is running out of memory");
		return SXERR_ABORT;
	}
	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_COMMA /*','*/) ){
		/* Multiple attribute declarations [i.e: public $var1,$var2=5<<1,$var3] */
		pGen->pIn++; /* Jump the comma */
		if( pGen->pIn >= pGen->pEnd || (pGen->pIn->nType & PH7_TK_DOLLAR/*'$'*/) == 0 ){
			SyToken *pTok = pGen->pIn;
			if( pTok >= pGen->pEnd ){
				pTok--;
			}
			rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,
				"Unexpected token '%z',expecting attribute declaration inside class '%z'",
				&pTok->sData,&pClass->sName);
			if( rc == SXERR_ABORT ){
				return SXERR_ABORT;
			}
		}else{
			if( pGen->pIn->nType & PH7_TK_DOLLAR ){
				goto loop;
			}
		}
	}
	return SXRET_OK;
Synchronize:
	/* Synchronize with the first semi-colon */
	while(pGen->pIn < pGen->pEnd && ((pGen->pIn->nType & PH7_TK_SEMI/*';'*/) == 0) ){
		pGen->pIn++;
	}
	return SXERR_CORRUPT;
}
/*
 * Compile a class method.
 *
 * Refer to the official documentation for more information
 * on the powerful extension introduced by the PH7 engine
 * to the OO subsystem such as full type hinting,method 
 * overloading and many more.
 */
static sxi32 GenStateCompileClassMethod(
	ph7_gen_state *pGen, /* Code generator state */
	sxi32 iProtection,   /* Visibility level */
	sxi32 iFlags,        /* Configuration flags */
	int doBody,          /* TRUE to process method body */
	ph7_class *pClass    /* Class this method belongs */
	)
{
	sxu32 nLine = pGen->pIn->nLine;
	ph7_class_method *pMeth;
	sxi32 iFuncFlags;
	SyString *pName;
	SyToken *pEnd;
	sxi32 rc;
	/* Extract visibility level */
	iProtection = GetProtectionLevel(iProtection);
	pGen->pIn++; /* Jump the 'function' keyword */
	iFuncFlags = 0;
	if( pGen->pIn >= pGen->pEnd ){
		/* Invalid method name */
		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Invalid method name");
		if( rc == SXERR_ABORT ){
			/* Error count limit reached,abort immediately */
			return SXERR_ABORT;
		}
		goto Synchronize;
	}
	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_AMPER) ){
		/* Return by reference,remember that */
		iFuncFlags |= VM_FUNC_REF_RETURN;
		/* Jump the '&' token */
		pGen->pIn++;
	}
	if( pGen->pIn >= pGen->pEnd || (pGen->pIn->nType & (PH7_TK_ID)) == 0 ){
		/* Invalid method name */
		rc = PH7_GenCompileError(&(*pGen),E_ERROR,nLine,"Invalid method name");
		if( rc == SXERR_ABORT ){
			return SXERR_ABORT;
		}
		goto Synchronize;
	}
	/* Peek method name */
	pName = &pGen->pIn->sData;
	nLine = pGen->pIn->nLine;
	/* Jump the method name */
	pGen->pIn++;
	if( iFlags & PH7_CLASS_ATTR_ABSTRACT ){
		/* Abstract method */
		if( iProtection == PH7_CLASS_PROT_PRIVATE ){
			rc = PH7_GenCompileError(pGen,E_ERROR,nLine,
				"Access type for abstract method '%z::%z' cannot be 'private'",
				&pClass->sName,pName);
			if( rc == SXERR_ABORT ){
				return SXERR_ABORT;
			}
		}
		/* Assemble method signature only */
		doBody = FALSE;
	}
	if( pGen->pIn >= pGen->pEnd || (pGen->pIn->nType & PH7_TK_LPAREN) == 0 ){
		/* Syntax error */
		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Expected '(' after method name '%z'",pName);
		if( rc == SXERR_ABORT ){
			/* Error count limit reached,abort immediately */
			return SXERR_ABORT;
		}
		goto Synchronize;
	}
	/* Allocate a new class_method instance */
	pMeth = PH7_NewClassMethod(pGen->pVm,pClass,pName,nLine,iProtection,iFlags,iFuncFlags);
	if( pMeth == 0 ){
		PH7_GenCompileError(&(*pGen),E_ERROR,nLine,"Fatal, PH7 is running out of memory");
		return SXERR_ABORT;
	}
	/* Jump the left parenthesis '(' */
	pGen->pIn++;
	pEnd = 0; /* cc warning */
	/* Delimit the method signature */
	PH7_DelimitNestedTokens(pGen->pIn,pGen->pEnd,PH7_TK_LPAREN /* '(' */,PH7_TK_RPAREN /* ')' */,&pEnd);
	if( pEnd >= pGen->pEnd ){
		/* Syntax error */
		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Missing ')' after method '%z' declaration",pName);
		if( rc == SXERR_ABORT ){
			/* Error count limit reached,abort immediately */
			return SXERR_ABORT;
		}
		goto Synchronize;
	}
	if( pGen->pIn < pEnd ){
		/* Collect method arguments */
		rc = GenStateCollectFuncArgs(&pMeth->sFunc,&(*pGen),pEnd);
		if( rc == SXERR_ABORT ){
			return SXERR_ABORT;
		}
	}
	/* Point beyond method signature */
	pGen->pIn = &pEnd[1];
	if( doBody ){
		/* Compile method body */
		rc = GenStateCompileFuncBody(&(*pGen),&pMeth->sFunc);
		if( rc == SXERR_ABORT ){
			return SXERR_ABORT;
		}
	}else{
		/* Only method signature is allowed */
		if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_SEMI /* ';'*/) == 0 ){
			rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,
				"Expected ';' after method signature '%z'",pName);
				if( rc == SXERR_ABORT ){
					/* Error count limit reached,abort immediately */
					return SXERR_ABORT;
				}
				return SXERR_CORRUPT;
			}
	}
	/* All done,install the method */
	rc = PH7_ClassInstallMethod(pClass,pMeth);
	if( rc != SXRET_OK ){
		PH7_GenCompileError(pGen,E_ERROR,nLine,"Fatal, PH7 is running out of memory");
		return SXERR_ABORT;
	}
	return SXRET_OK;
Synchronize:
	/* Synchronize with the first semi-colon */
	while(pGen->pIn < pGen->pEnd && ((pGen->pIn->nType & PH7_TK_SEMI/*';'*/) == 0) ){
		pGen->pIn++;
	}
	return SXERR_CORRUPT;
}
/*
 * Compile an object interface.
 *  According to the PHP language reference manual
 *   Object Interfaces:
 *   Object interfaces allow you to create code which specifies which methods
 *   a class must implement, without having to define how these methods are handled.
 *   Interfaces are defined using the interface keyword, in the same way as a standard
 *   class, but without any of the methods having their contents defined.
 *   All methods declared in an interface must be public, this is the nature of an interface. 
 */
static sxi32 PH7_CompileClassInterface(ph7_gen_state *pGen)
{
	sxu32 nLine = pGen->pIn->nLine;
	ph7_class *pClass,*pBase;
	SyToken *pEnd,*pTmp;
	SyString *pName;
	sxi32 nKwrd;
	sxi32 rc;
	/* Jump the 'interface' keyword */
	pGen->pIn++;
	/* Extract interface name */
	pName = &pGen->pIn->sData;
	/* Advance the stream cursor */
	pGen->pIn++;
	/* Obtain a raw class */
	pClass = PH7_NewRawClass(pGen->pVm,pName,nLine);
	if( pClass == 0 ){
		PH7_GenCompileError(pGen,E_ERROR,nLine,"Fatal, PH7 is running out of memory");
		return SXERR_ABORT;
	}
	/* Mark as an interface */
	pClass->iFlags = PH7_CLASS_INTERFACE;
	/* Assume no base class is given */
	pBase = 0;
	if( pGen->pIn < pGen->pEnd  && (pGen->pIn->nType & PH7_TK_KEYWORD) ){
		nKwrd = SX_PTR_TO_INT(pGen->pIn->pUserData);
		if( nKwrd == PH7_TKWRD_EXTENDS /* interface b extends a */ ){
			SyString *pBaseName;
			/* Extract base interface */
			pGen->pIn++;
			if( pGen->pIn >= pGen->pEnd || (pGen->pIn->nType & PH7_TK_ID) == 0 ){
				/* Syntax error */
				rc = PH7_GenCompileError(pGen,E_ERROR,nLine,
					"Expected 'interface_name' after 'extends' keyword inside interface '%z'",
					pName);
				SyMemBackendPoolFree(&pGen->pVm->sAllocator,pClass);
				if( rc == SXERR_ABORT ){
					/* Error count limit reached,abort immediately */
					return SXERR_ABORT;
				}
				return SXRET_OK;
			}
			pBaseName = &pGen->pIn->sData;
			pBase = PH7_VmExtractClass(pGen->pVm,pBaseName->zString,pBaseName->nByte,FALSE,0);
			/* Only interfaces is allowed */
			while( pBase && (pBase->iFlags & PH7_CLASS_INTERFACE) == 0 ){
				pBase = pBase->pNextName;
			}
			if( pBase == 0 ){
				/* Inexistant interface */
				rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,"Inexistant base interface '%z'",pBaseName);
				if( rc == SXERR_ABORT ){
					/* Error count limit reached,abort immediately */
					return SXERR_ABORT;
				}
			}
			/* Advance the stream cursor */
			pGen->pIn++;
		}
	}
	if( pGen->pIn >= pGen->pEnd  || (pGen->pIn->nType & PH7_TK_OCB /*'{'*/) == 0 ){
		/* Syntax error */
		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Expected '{' after interface '%z' definition",pName);
		SyMemBackendPoolFree(&pGen->pVm->sAllocator,pClass);
		if( rc == SXERR_ABORT ){
			/* Error count limit reached,abort immediately */
			return SXERR_ABORT;
		}
		return SXRET_OK;
	}
	pGen->pIn++; /* Jump the leading curly brace */
	pEnd = 0; /* cc warning */
	/* Delimit the interface body */
	PH7_DelimitNestedTokens(pGen->pIn,pGen->pEnd,PH7_TK_OCB/*'{'*/,PH7_TK_CCB/*'}'*/,&pEnd);
	if( pEnd >= pGen->pEnd ){
		/* Syntax error */
		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Missing '}' after interface '%z' definition",pName);
		SyMemBackendPoolFree(&pGen->pVm->sAllocator,pClass);
		if( rc == SXERR_ABORT ){
			/* Error count limit reached,abort immediately */
			return SXERR_ABORT;
		}
		return SXRET_OK;
	}
	/* Swap token stream */
	pTmp = pGen->pEnd;
	pGen->pEnd = pEnd;
	/* Start the parse process 
	 * Note (According to the PHP reference manual):
	 *  Only constants and function signatures(without body) are allowed.
	 *  Only 'public' visibility is allowed.
	 */
	for(;;){
		/* Jump leading/trailing semi-colons */
		while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_SEMI/*';'*/) ){
			pGen->pIn++;
		}
		if( pGen->pIn >= pGen->pEnd ){
			/* End of interface body */
			break;
		}
		if( (pGen->pIn->nType & PH7_TK_KEYWORD) == 0 ){
			rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,
				"Unexpected token '%z'.Expecting method signature or constant declaration inside interface '%z'",
				&pGen->pIn->sData,pName);
			if( rc == SXERR_ABORT ){
				/* Error count limit reached,abort immediately */
				return SXERR_ABORT;
			}
			goto done;
		}
		/* Extract the current keyword */
		nKwrd = SX_PTR_TO_INT(pGen->pIn->pUserData);
		if( nKwrd == PH7_TKWRD_PRIVATE || nKwrd == PH7_TKWRD_PROTECTED ){
			/* Emit a warning and switch to public visibility */
			PH7_GenCompileError(&(*pGen),E_WARNING,pGen->pIn->nLine,"interface: Access type must be public");
			nKwrd = PH7_TKWRD_PUBLIC;
		}
		if( nKwrd != PH7_TKWRD_PUBLIC && nKwrd != PH7_TKWRD_FUNCTION && nKwrd != PH7_TKWRD_CONST && nKwrd != PH7_TKWRD_STATIC ){
			rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,
				"Expecting method signature or constant declaration inside interface '%z'",pName);
			if( rc == SXERR_ABORT ){
				/* Error count limit reached,abort immediately */
				return SXERR_ABORT;
			}
			goto done;
		}
		if( nKwrd == PH7_TKWRD_PUBLIC ){
			/* Advance the stream cursor */
			pGen->pIn++;
			if( pGen->pIn >= pGen->pEnd || (pGen->pIn->nType & PH7_TK_KEYWORD) == 0 ){
				rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,
					"Expecting method signature inside interface '%z'",pName);
				if( rc == SXERR_ABORT ){
					/* Error count limit reached,abort immediately */
					return SXERR_ABORT;
				}
				goto done;
			}
			nKwrd = SX_PTR_TO_INT(pGen->pIn->pUserData);
			if( nKwrd != PH7_TKWRD_FUNCTION && nKwrd != PH7_TKWRD_CONST && nKwrd != PH7_TKWRD_STATIC ){
				rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,
					"Expecting method signature or constant declaration inside interface '%z'",pName);
				if( rc == SXERR_ABORT ){
					/* Error count limit reached,abort immediately */
					return SXERR_ABORT;
				}
				goto done;
			}
		}
		if( nKwrd == PH7_TKWRD_CONST ){
			/* Parse constant */
			rc = GenStateCompileClassConstant(&(*pGen),0,0,pClass);
			if( rc != SXRET_OK ){
				if( rc == SXERR_ABORT ){
					return SXERR_ABORT;
				}
				goto done;
			}
		}else{
			sxi32 iFlags = 0;
			if( nKwrd == PH7_TKWRD_STATIC ){
				/* Static method,record that */
				iFlags |= PH7_CLASS_ATTR_STATIC;
				/* Advance the stream cursor */
				pGen->pIn++;
				if( pGen->pIn >= pGen->pEnd || (pGen->pIn->nType & PH7_TK_KEYWORD) == 0 
					|| SX_PTR_TO_INT(pGen->pIn->pUserData) != PH7_TKWRD_FUNCTION ){
						rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,
							"Expecting method signature inside interface '%z'",pName);
						if( rc == SXERR_ABORT ){
							/* Error count limit reached,abort immediately */
							return SXERR_ABORT;
						}
						goto done;
				}
			}
			/* Process method signature */
			rc = GenStateCompileClassMethod(&(*pGen),0,FALSE/* Only method signature*/,iFlags,pClass);
			if( rc != SXRET_OK ){
				if( rc == SXERR_ABORT ){
					return SXERR_ABORT;
				}
				goto done;
			}
		}
	}
	/* Install the interface */
	rc = PH7_VmInstallClass(pGen->pVm,pClass);
	if( rc == SXRET_OK && pBase ){
		/* Inherit from the base interface */
		rc = PH7_ClassInterfaceInherit(pClass,pBase);
	}
	if( rc != SXRET_OK ){
		PH7_GenCompileError(pGen,E_ERROR,nLine,"Fatal, PH7 is running out of memory");
		return SXERR_ABORT;
	}
done:
	/* Point beyond the interface body */
	pGen->pIn  = &pEnd[1];
	pGen->pEnd = pTmp;
	return PH7_OK;
}
/*
 * Compile a user-defined class.
 * According to the PHP language reference manual
 *  class
 *  Basic class definitions begin with the keyword class, followed by a class
 *  name, followed by a pair of curly braces which enclose the definitions
 *  of the properties and methods belonging to the class.
 *  The class name can be any valid label which is a not a PHP reserved word.
 *  A valid class name starts with a letter or underscore, followed by any number
 *  of letters, numbers, or underscores. As a regular expression, it would be expressed
 *  thus: [a-zA-Z_\x7f-\xff][a-zA-Z0-9_\x7f-\xff]*.
 *  A class may contain its own constants, variables (called "properties"), and functions
 *  (called "methods"). 
 */
static sxi32 GenStateCompileClass(ph7_gen_state *pGen,sxi32 iFlags)
{
	sxu32 nLine = pGen->pIn->nLine;
	ph7_class *pClass,*pBase;
	SyToken *pEnd,*pTmp;
	sxi32 iProtection;
	SySet aInterfaces;
	sxi32 iAttrflags;
	SyString *pName;
	sxi32 nKwrd;
	sxi32 rc;
	/* Jump the 'class' keyword */
	pGen->pIn++;
	if( pGen->pIn >= pGen->pEnd || (pGen->pIn->nType & PH7_TK_ID) == 0 ){
		/* Syntax error */
		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Invalid class name");
		if( rc == SXERR_ABORT ){
			/* Error count limit reached,abort immediately */
			return SXERR_ABORT;
		}
		/* Synchronize with the first semi-colon or curly braces */
		while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & (PH7_TK_OCB/*'{'*/|PH7_TK_SEMI/*';'*/)) == 0 ){
			pGen->pIn++;
		}
		return SXRET_OK;
	}
	/* Extract class name */
	pName = &pGen->pIn->sData;
	/* Advance the stream cursor */
	pGen->pIn++;
	/* Obtain a raw class */
	pClass = PH7_NewRawClass(pGen->pVm,pName,nLine);
	if( pClass == 0 ){
		PH7_GenCompileError(pGen,E_ERROR,nLine,"Fatal, PH7 is running out of memory");
		return SXERR_ABORT;
	}
	/* implemented interfaces container */
	SySetInit(&aInterfaces,&pGen->pVm->sAllocator,sizeof(ph7_class *));
	/* Assume a standalone class */
	pBase = 0;
	if( pGen->pIn < pGen->pEnd  && (pGen->pIn->nType & PH7_TK_KEYWORD) ){
		SyString *pBaseName;
		nKwrd = SX_PTR_TO_INT(pGen->pIn->pUserData);
		if( nKwrd == PH7_TKWRD_EXTENDS /* class b extends a */ ){
			pGen->pIn++; /* Advance the stream cursor */
			if( pGen->pIn >= pGen->pEnd || (pGen->pIn->nType & PH7_TK_ID) == 0 ){
				/* Syntax error */
				rc = PH7_GenCompileError(pGen,E_ERROR,nLine,
					"Expected 'class_name' after 'extends' keyword inside class '%z'",
					pName);
				SyMemBackendPoolFree(&pGen->pVm->sAllocator,pClass);
				if( rc == SXERR_ABORT ){
					/* Error count limit reached,abort immediately */
					return SXERR_ABORT;
				}
				return SXRET_OK;
			}
			/* Extract base class name */
			pBaseName = &pGen->pIn->sData;
			/* Perform the query */
			pBase = PH7_VmExtractClass(pGen->pVm,pBaseName->zString,pBaseName->nByte,FALSE,0);
			/* Interfaces are not allowed */
			while( pBase && (pBase->iFlags & PH7_CLASS_INTERFACE) ){
				pBase = pBase->pNextName;
			}
			if( pBase == 0 ){
				/* Inexistant base class */
				rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,"Inexistant base class '%z'",pBaseName);
				if( rc == SXERR_ABORT ){
					/* Error count limit reached,abort immediately */
					return SXERR_ABORT;
				}
			}else{
				if( pBase->iFlags & PH7_CLASS_FINAL ){
					rc = PH7_GenCompileError(pGen,E_ERROR,nLine,
						"Class '%z' may not inherit from final class '%z'",pName,&pBase->sName);
					if( rc == SXERR_ABORT ){
						/* Error count limit reached,abort immediately */
						return SXERR_ABORT;
					}
				}
			}
			/* Advance the stream cursor */
			pGen->pIn++;
		}
		if (pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_KEYWORD) && SX_PTR_TO_INT(pGen->pIn->pUserData) == PH7_TKWRD_IMPLEMENTS ){
			ph7_class *pInterface;
			SyString *pIntName;
			/* Interface implementation */
			pGen->pIn++; /* Advance the stream cursor */
			for(;;){
				if( pGen->pIn >= pGen->pEnd || (pGen->pIn->nType & PH7_TK_ID) == 0 ){
					/* Syntax error */
					rc = PH7_GenCompileError(pGen,E_ERROR,nLine,
						"Expected 'interface_name' after 'implements' keyword inside class '%z' declaration",
						pName);
					if( rc == SXERR_ABORT ){
						/* Error count limit reached,abort immediately */
						return SXERR_ABORT;
					}
					break;
				}
				/* Extract interface name */
				pIntName = &pGen->pIn->sData;
				/* Make sure the interface is already defined */
				pInterface = PH7_VmExtractClass(pGen->pVm,pIntName->zString,pIntName->nByte,FALSE,0);
				/* Only interfaces are allowed */
				while( pInterface && (pInterface->iFlags & PH7_CLASS_INTERFACE) == 0 ){
					pInterface = pInterface->pNextName;
				}
				if( pInterface == 0 ){
					/* Inexistant interface */
					rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,"Inexistant base interface '%z'",pIntName);
					if( rc == SXERR_ABORT ){
						/* Error count limit reached,abort immediately */
						return SXERR_ABORT;
					}
				}else{
					/* Register interface */
					SySetPut(&aInterfaces,(const void *)&pInterface);
				}
				/* Advance the stream cursor */
				pGen->pIn++;
				if( pGen->pIn >= pGen->pEnd || (pGen->pIn->nType & PH7_TK_COMMA) == 0 ){
					break;
				}
				pGen->pIn++;/* Jump the comma */
			}
		}
	}
	if( pGen->pIn >= pGen->pEnd  || (pGen->pIn->nType & PH7_TK_OCB /*'{'*/) == 0 ){
		/* Syntax error */
		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Expected '{' after class '%z' declaration",pName);
		SyMemBackendPoolFree(&pGen->pVm->sAllocator,pClass);
		if( rc == SXERR_ABORT ){
			/* Error count limit reached,abort immediately */
			return SXERR_ABORT;
		}
		return SXRET_OK;
	}
	pGen->pIn++; /* Jump the leading curly brace */
	pEnd = 0; /* cc warning */
	/* Delimit the class body */
	PH7_DelimitNestedTokens(pGen->pIn,pGen->pEnd,PH7_TK_OCB/*'{'*/,PH7_TK_CCB/*'}'*/,&pEnd);
	if( pEnd >= pGen->pEnd ){
		/* Syntax error */
		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Missing closing braces'}' after class '%z' definition",pName);
		SyMemBackendPoolFree(&pGen->pVm->sAllocator,pClass);
		if( rc == SXERR_ABORT ){
			/* Error count limit reached,abort immediately */
			return SXERR_ABORT;
		}
		return SXRET_OK;
	}
	/* Swap token stream */
	pTmp = pGen->pEnd;
	pGen->pEnd = pEnd;
	/* Set the inherited flags */
	pClass->iFlags = iFlags;
	/* Start the parse process */
	for(;;){
		/* Jump leading/trailing semi-colons */
		while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_SEMI/*';'*/) ){
			pGen->pIn++;
		}
		if( pGen->pIn >= pGen->pEnd ){
			/* End of class body */
			break;
		}
		if( (pGen->pIn->nType & (PH7_TK_KEYWORD|PH7_TK_DOLLAR)) == 0 ){
			rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,
				"Unexpected token '%z'. Expecting attribute declaration inside class '%z'",
				&pGen->pIn->sData,pName);
			if( rc == SXERR_ABORT ){
				/* Error count limit reached,abort immediately */
				return SXERR_ABORT;
			}
			goto done;
		}
		/* Assume public visibility */
		iProtection = PH7_TKWRD_PUBLIC;
		iAttrflags = 0;
		if( pGen->pIn->nType & PH7_TK_KEYWORD ){
			/* Extract the current keyword */
			nKwrd = SX_PTR_TO_INT(pGen->pIn->pUserData);
			if( nKwrd == PH7_TKWRD_PUBLIC || nKwrd == PH7_TKWRD_PRIVATE || nKwrd == PH7_TKWRD_PROTECTED ){
				iProtection = nKwrd;
				pGen->pIn++; /* Jump the visibility token */
				if( pGen->pIn >= pGen->pEnd || (pGen->pIn->nType & (PH7_TK_KEYWORD|PH7_TK_DOLLAR)) == 0 ){
					rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,
						"Unexpected token '%z'. Expecting attribute declaration inside class '%z'",
						&pGen->pIn->sData,pName);
					if( rc == SXERR_ABORT ){
						/* Error count limit reached,abort immediately */
						return SXERR_ABORT;
					}
					goto done;
				}
				if( pGen->pIn->nType & PH7_TK_DOLLAR ){
					/* Attribute declaration */
					rc = GenStateCompileClassAttr(&(*pGen),iProtection,iAttrflags,pClass);
					if( rc != SXRET_OK ){
						if( rc == SXERR_ABORT ){
							return SXERR_ABORT;
						}
						goto done;
					}
					continue;
				}
				/* Extract the keyword */
				nKwrd = SX_PTR_TO_INT(pGen->pIn->pUserData);
			}
			if( nKwrd == PH7_TKWRD_CONST ){
				/* Process constant declaration */
				rc = GenStateCompileClassConstant(&(*pGen),iProtection,iAttrflags,pClass);
				if( rc != SXRET_OK ){
					if( rc == SXERR_ABORT ){
						return SXERR_ABORT;
					}
					goto done;
				}
			}else{
				if( nKwrd == PH7_TKWRD_STATIC ){
					/* Static method or attribute,record that */
					iAttrflags |= PH7_CLASS_ATTR_STATIC;
					pGen->pIn++; /* Jump the static keyword */
					if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_KEYWORD) ){
						/* Extract the keyword */
						nKwrd = SX_PTR_TO_INT(pGen->pIn->pUserData);
						if( nKwrd == PH7_TKWRD_PUBLIC || nKwrd == PH7_TKWRD_PRIVATE || nKwrd == PH7_TKWRD_PROTECTED ){
							iProtection = nKwrd;
							pGen->pIn++; /* Jump the visibility token */
						}
					}
					if( pGen->pIn >= pGen->pEnd || (pGen->pIn->nType & (PH7_TK_KEYWORD|PH7_TK_DOLLAR)) == 0 ){
						rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,
							"Unexpected token '%z',Expecting method,attribute or constant declaration inside class '%z'",
							&pGen->pIn->sData,pName);
						if( rc == SXERR_ABORT ){
							/* Error count limit reached,abort immediately */
							return SXERR_ABORT;
						}
						goto done;
					}
					if( pGen->pIn->nType & PH7_TK_DOLLAR ){
						/* Attribute declaration */
						rc = GenStateCompileClassAttr(&(*pGen),iProtection,iAttrflags,pClass);
						if( rc != SXRET_OK ){
							if( rc == SXERR_ABORT ){
								return SXERR_ABORT;
							}
							goto done;
						}
						continue;
					}
					/* Extract the keyword */
					nKwrd = SX_PTR_TO_INT(pGen->pIn->pUserData);
				}else if( nKwrd == PH7_TKWRD_ABSTRACT ){
					/* Abstract method,record that */
					iAttrflags |= PH7_CLASS_ATTR_ABSTRACT;
					/* Mark the whole class as abstract */
					pClass->iFlags |= PH7_CLASS_ABSTRACT;
					/* Advance the stream cursor */
					pGen->pIn++; 
					if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_KEYWORD) ){
						nKwrd = SX_PTR_TO_INT(pGen->pIn->pUserData);
						if( nKwrd == PH7_TKWRD_PUBLIC || nKwrd == PH7_TKWRD_PRIVATE || nKwrd == PH7_TKWRD_PROTECTED ){
							iProtection = nKwrd;
							pGen->pIn++; /* Jump the visibility token */
						}
					}
					if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_KEYWORD) && 
						SX_PTR_TO_INT(pGen->pIn->pUserData) == PH7_TKWRD_STATIC ){
							/* Static method */
							iAttrflags |= PH7_CLASS_ATTR_STATIC;
							pGen->pIn++; /* Jump the static keyword */
					}					
					if( pGen->pIn >= pGen->pEnd || (pGen->pIn->nType & PH7_TK_KEYWORD) == 0 ||
						SX_PTR_TO_INT(pGen->pIn->pUserData) != PH7_TKWRD_FUNCTION ){
							rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,
								"Unexpected token '%z',Expecting method declaration after 'abstract' keyword inside class '%z'",
								&pGen->pIn->sData,pName);
							if( rc == SXERR_ABORT ){
								/* Error count limit reached,abort immediately */
								return SXERR_ABORT;
							}
							goto done;
					}
					nKwrd = PH7_TKWRD_FUNCTION;
				}else if( nKwrd == PH7_TKWRD_FINAL ){
					/* final method ,record that */
					iAttrflags |= PH7_CLASS_ATTR_FINAL;
					pGen->pIn++; /* Jump the final keyword */
					if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_KEYWORD) ){
						/* Extract the keyword */
						nKwrd = SX_PTR_TO_INT(pGen->pIn->pUserData);
						if( nKwrd == PH7_TKWRD_PUBLIC || nKwrd == PH7_TKWRD_PRIVATE || nKwrd == PH7_TKWRD_PROTECTED ){
							iProtection = nKwrd;
							pGen->pIn++; /* Jump the visibility token */
						}
					}
					if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_KEYWORD) &&
						SX_PTR_TO_INT(pGen->pIn->pUserData) == PH7_TKWRD_STATIC ){
							/* Static method */
							iAttrflags |= PH7_CLASS_ATTR_STATIC;
							pGen->pIn++; /* Jump the static keyword */
					}
					if( pGen->pIn >= pGen->pEnd || (pGen->pIn->nType & PH7_TK_KEYWORD) == 0 || 
						SX_PTR_TO_INT(pGen->pIn->pUserData) != PH7_TKWRD_FUNCTION ){
							rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,
								"Unexpected token '%z',Expecting method declaration after 'final' keyword inside class '%z'",
								&pGen->pIn->sData,pName);
							if( rc == SXERR_ABORT ){
								/* Error count limit reached,abort immediately */
								return SXERR_ABORT;
							}
							goto done;
					}
					nKwrd = PH7_TKWRD_FUNCTION;
				}
				if( nKwrd != PH7_TKWRD_FUNCTION && nKwrd != PH7_TKWRD_VAR ){
					rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,
						"Unexpected token '%z',Expecting method declaration inside class '%z'",
							&pGen->pIn->sData,pName);
						if( rc == SXERR_ABORT ){
							/* Error count limit reached,abort immediately */
							return SXERR_ABORT;
						}
						goto done;
				}
				if( nKwrd == PH7_TKWRD_VAR ){
					pGen->pIn++; /* Jump the 'var' keyword */
					if( pGen->pIn >= pGen->pEnd || (pGen->pIn->nType & PH7_TK_DOLLAR/*'$'*/) == 0){
						rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,
							"Expecting attribute declaration after 'var' keyword");
						if( rc == SXERR_ABORT ){
							/* Error count limit reached,abort immediately */
							return SXERR_ABORT;
						}
						goto done;
					}
					/* Attribute declaration */
					rc = GenStateCompileClassAttr(&(*pGen),iProtection,iAttrflags,pClass);
				}else{
					/* Process method declaration */
					rc = GenStateCompileClassMethod(&(*pGen),iProtection,iAttrflags,TRUE,pClass);
				}
				if( rc != SXRET_OK ){
					if( rc == SXERR_ABORT ){
						return SXERR_ABORT;
					}
					goto done;
				}
			}
		}else{
			/* Attribute declaration */
			rc = GenStateCompileClassAttr(&(*pGen),iProtection,iAttrflags,pClass);
			if( rc != SXRET_OK ){
				if( rc == SXERR_ABORT ){
					return SXERR_ABORT;
				}
				goto done;
			}
		}
	}
	/* Install the class */
	rc = PH7_VmInstallClass(pGen->pVm,pClass);
	if( rc == SXRET_OK ){
		ph7_class **apInterface;
		sxu32 n;
		if( pBase ){
			/* Inherit from base class and mark as a subclass */
			rc = PH7_ClassInherit(&(*pGen),pClass,pBase);
		}
		apInterface = (ph7_class **)SySetBasePtr(&aInterfaces);
		for( n = 0 ; n < SySetUsed(&aInterfaces) ; n++ ){
			/* Implements one or more interface */
			rc = PH7_ClassImplement(pClass,apInterface[n]);
			if( rc != SXRET_OK ){
				break;
			}
		}
	}
	SySetRelease(&aInterfaces);
	if( rc != SXRET_OK ){
		PH7_GenCompileError(pGen,E_ERROR,nLine,"Fatal, PH7 is running out of memory");
		return SXERR_ABORT;
	}
done:
	/* Point beyond the class body */
	pGen->pIn = &pEnd[1];
	pGen->pEnd = pTmp;
	return PH7_OK;
}
/*
 * Compile a user-defined abstract class.
 *  According to the PHP language reference manual
 *   PHP 5 introduces abstract classes and methods. Classes defined as abstract 
 *   may not be instantiated, and any class that contains at least one abstract
 *   method must also be abstract. Methods defined as abstract simply declare 
 *   the method's signature - they cannot define the implementation.
 *   When inheriting from an abstract class, all methods marked abstract in the parent's
 *   class declaration must be defined by the child; additionally, these methods must be
 *   defined with the same (or a less restricted) visibility. For example, if the abstract
 *   method is defined as protected, the function implementation must be defined as either
 *   protected or public, but not private. Furthermore the signatures of the methods must
 *   match, i.e. the type hints and the number of required arguments must be the same. 
 *   This also applies to constructors as of PHP 5.4. Before 5.4 constructor signatures
 *   could differ.
 */
static sxi32 PH7_CompileAbstractClass(ph7_gen_state *pGen)
{
	sxi32 rc;
	pGen->pIn++; /* Jump the 'abstract' keyword */
	rc = GenStateCompileClass(&(*pGen),PH7_CLASS_ABSTRACT);
	return rc;
}
/*
 * Compile a user-defined final class.
 *  According to the PHP language reference manual
 *    PHP 5 introduces the final keyword, which prevents child classes from overriding
 *    a method by prefixing the definition with final. If the class itself is being defined
 *    final then it cannot be extended. 
 */
static sxi32 PH7_CompileFinalClass(ph7_gen_state *pGen)
{
	sxi32 rc;
	pGen->pIn++; /* Jump the 'final' keyword */
	rc = GenStateCompileClass(&(*pGen),PH7_CLASS_FINAL);
	return rc;
}
/*
 * Compile a user-defined class.
 *  According to the PHP language reference manual
 *   Basic class definitions begin with the keyword class, followed
 *   by a class name, followed by a pair of curly braces which enclose
 *   the definitions of the properties and methods belonging to the class.
 *   A class may contain its own constants, variables (called "properties")
 *   and functions (called "methods"). 
 */
static sxi32 PH7_CompileClass(ph7_gen_state *pGen)
{
	sxi32 rc;
	rc = GenStateCompileClass(&(*pGen),0);
	return rc;
}
/*
 * Exception handling.
 *  According to the PHP language reference manual
 *    An exception can be thrown, and caught ("catched") within PHP. Code may be surrounded
 *    in a try block, to facilitate the catching of potential exceptions. Each try must have
 *    at least one corresponding catch block. Multiple catch blocks can be used to catch 
 *    different classes of exceptions. Normal execution (when no exception is thrown within
 *    the try block, or when a catch matching the thrown exception's class is not present) 
 *    will continue after that last catch block defined in sequence. Exceptions can be thrown
 *    (or re-thrown) within a catch block.
 *    When an exception is thrown, code following the statement will not be executed, and PHP
 *    will attempt to find the first matching catch block. If an exception is not caught, a PHP
 *    Fatal Error will be issued with an "Uncaught Exception ..." message, unless a handler has
 *    been defined with set_exception_handler().
 *    The thrown object must be an instance of the Exception class or a subclass of Exception.
 *    Trying to throw an object that is not will result in a PHP Fatal Error. 
 */
/*
 * Expression tree validator callback associated with the 'throw' statement.
 * Return SXRET_OK if the tree form a valid expression.Any other error
 * indicates failure.
 */
static sxi32 GenStateThrowNodeValidator(ph7_gen_state *pGen,ph7_expr_node *pRoot)
{
	sxi32 rc = SXRET_OK;
	if( pRoot->pOp ){
		if( pRoot->pOp->iOp != EXPR_OP_SUBSCRIPT /* $a[] */ && pRoot->pOp->iOp != EXPR_OP_NEW /* new Exception() */
			&& pRoot->pOp->iOp != EXPR_OP_ARROW /* -> */ && pRoot->pOp->iOp != EXPR_OP_DC /* :: */){
			/* Unexpected expression */
			rc = PH7_GenCompileError(&(*pGen),E_ERROR,pRoot->pStart? pRoot->pStart->nLine : 0,
				"throw: Expecting an exception class instance");
			if( rc != SXERR_ABORT ){
				rc = SXERR_INVALID;
			}
		}
	}else if( pRoot->xCode != PH7_CompileVariable ){
		/* Unexpected expression */
		rc = PH7_GenCompileError(&(*pGen),E_ERROR,pRoot->pStart? pRoot->pStart->nLine : 0,
			"throw: Expecting an exception class instance");
		if( rc != SXERR_ABORT ){
			rc = SXERR_INVALID;
		}
	}
	return rc;
}
/*
 * Compile a 'throw' statement.
 * throw: This is how you trigger an exception.
 * Each "throw" block must have at least one "catch" block associated with it.
 */
static sxi32 PH7_CompileThrow(ph7_gen_state *pGen)
{
	sxu32 nLine = pGen->pIn->nLine;
	GenBlock *pBlock;
	sxu32 nIdx;
	sxi32 rc;
	pGen->pIn++; /* Jump the 'throw' keyword */
	/* Compile the expression */
	rc = PH7_CompileExpr(&(*pGen),0,GenStateThrowNodeValidator);
	if( rc == SXERR_EMPTY ){
		rc = PH7_GenCompileError(&(*pGen),E_ERROR,nLine,"throw: Expecting an exception class instance");
		if( rc == SXERR_ABORT ){
			return SXERR_ABORT;
		}
		return SXRET_OK;
	}
	pBlock = pGen->pCurrent;
	/* Point to the top most function or try block and emit the forward jump */
	while(pBlock->pParent){
		if( pBlock->iFlags & (GEN_BLOCK_EXCEPTION|GEN_BLOCK_FUNC) ){
			break;
		}
		/* Point to the parent block */
		pBlock = pBlock->pParent;
	}
	/* Emit the throw instruction */
	PH7_VmEmitInstr(pGen->pVm,PH7_OP_THROW,0,0,0,&nIdx);
	/* Emit the jump */
	GenStateNewJumpFixup(pBlock,PH7_OP_THROW,nIdx);
	return SXRET_OK;
}
/*
 * Compile a 'catch' block.
 * Catch: A "catch" block retrieves an exception and creates
 * an object containing the exception information.
 */
static sxi32 PH7_CompileCatch(ph7_gen_state *pGen,ph7_exception *pException)
{
	sxu32 nLine = pGen->pIn->nLine;
	ph7_exception_block sCatch;
	SySet *pInstrContainer;
	GenBlock *pCatch;
	SyToken *pToken;
	SyString *pName;
	char *zDup;
	sxi32 rc;
	pGen->pIn++; /* Jump the 'catch' keyword */
	/* Zero the structure */
	SyZero(&sCatch,sizeof(ph7_exception_block));
	/* Initialize fields */
	SySetInit(&sCatch.sByteCode,&pException->pVm->sAllocator,sizeof(VmInstr));
	if( pGen->pIn >= pGen->pEnd || (pGen->pIn->nType & PH7_TK_LPAREN) == 0 /*(*/ ||
		&pGen->pIn[1] >= pGen->pEnd || (pGen->pIn[1].nType & (PH7_TK_ID|PH7_TK_KEYWORD)) == 0 ){
			/* Unexpected token,break immediately */
			pToken = pGen->pIn;
			if( pToken >= pGen->pEnd ){
				pToken--;
			}
			rc = PH7_GenCompileError(pGen,E_ERROR,pToken->nLine,
				"Catch: Unexpected token '%z',excpecting class name",&pToken->sData);
			if( rc == SXERR_ABORT ){
				return SXERR_ABORT;
			}
			return SXERR_INVALID;
	}
	/* Extract the exception class */
	pGen->pIn++; /* Jump the left parenthesis '(' */
	/* Duplicate class name */
	pName = &pGen->pIn->sData;
	zDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,pName->zString,pName->nByte);
	if( zDup == 0 ){
		goto Mem;
	}
	SyStringInitFromBuf(&sCatch.sClass,zDup,pName->nByte);
	pGen->pIn++;
	if( pGen->pIn >= pGen->pEnd || (pGen->pIn->nType & PH7_TK_DOLLAR) == 0 /*$*/ ||
		&pGen->pIn[1] >= pGen->pEnd || (pGen->pIn[1].nType & (PH7_TK_ID|PH7_TK_KEYWORD)) == 0 ){
			/* Unexpected token,break immediately */
			pToken = pGen->pIn;
			if( pToken >= pGen->pEnd ){
				pToken--;
			}
			rc = PH7_GenCompileError(pGen,E_ERROR,pToken->nLine,
				"Catch: Unexpected token '%z',expecting variable name",&pToken->sData);
			if( rc == SXERR_ABORT ){
				return SXERR_ABORT;
			}
			return SXERR_INVALID;
	}
	pGen->pIn++; /* Jump the dollar sign */
	/* Duplicate instance name */
	pName = &pGen->pIn->sData;
	zDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,pName->zString,pName->nByte);
	if( zDup == 0 ){
		goto Mem;
	}
	SyStringInitFromBuf(&sCatch.sThis,zDup,pName->nByte);
	pGen->pIn++;
	if( pGen->pIn >= pGen->pEnd || (pGen->pIn->nType & PH7_TK_RPAREN) == 0 /*)*/ ){
		/* Unexpected token,break immediately */
		pToken = pGen->pIn;
		if( pToken >= pGen->pEnd ){
			pToken--;
		}
		rc = PH7_GenCompileError(pGen,E_ERROR,pToken->nLine,
			"Catch: Unexpected token '%z',expecting right parenthesis ')'",&pToken->sData);
		if( rc == SXERR_ABORT ){
			return SXERR_ABORT;
		}
		return SXERR_INVALID;
	}
	/* Compile the block */
	pGen->pIn++; /* Jump the right parenthesis */
	/* Create the catch block */
	rc = GenStateEnterBlock(&(*pGen),GEN_BLOCK_EXCEPTION,PH7_VmInstrLength(pGen->pVm),0,&pCatch);
	if( rc != SXRET_OK ){
		return SXERR_ABORT;
	}
	/* Swap bytecode container */
	pInstrContainer = PH7_VmGetByteCodeContainer(pGen->pVm);
	PH7_VmSetByteCodeContainer(pGen->pVm,&sCatch.sByteCode);
	/* Compile the block */
	PH7_CompileBlock(&(*pGen),0);
	/* Fix forward jumps now the destination is resolved  */
	GenStateFixJumps(pCatch,-1,PH7_VmInstrLength(pGen->pVm));
	/* Emit the DONE instruction */
	PH7_VmEmitInstr(pGen->pVm,PH7_OP_DONE,0,0,0,0);
	/* Leave the block */
	GenStateLeaveBlock(&(*pGen),0);
	/* Restore the default container */
	PH7_VmSetByteCodeContainer(pGen->pVm,pInstrContainer);
	/* Install the catch block */
	rc = SySetPut(&pException->sEntry,(const void *)&sCatch);
	if( rc != SXRET_OK ){
		goto Mem;
	}
	return SXRET_OK;
Mem:
	PH7_GenCompileError(&(*pGen),E_ERROR,nLine,"Fatal, PH7 engine is running out of memory");
	return SXERR_ABORT;
}
/*
 * Compile a 'try' block.
 * A function using an exception should be in a "try" block.
 * If the exception does not trigger, the code will continue
 * as normal. However if the exception triggers, an exception
 * is "thrown".
 */
static sxi32 PH7_CompileTry(ph7_gen_state *pGen)
{
	ph7_exception *pException;
	GenBlock *pTry;
	sxu32 nJmpIdx;
	sxi32 rc;
	/* Create the exception container */
	pException = (ph7_exception *)SyMemBackendAlloc(&pGen->pVm->sAllocator,sizeof(ph7_exception));
	if( pException == 0 ){
		PH7_GenCompileError(&(*pGen),E_ERROR,
			pGen->pIn->nLine,"Fatal, PH7 engine is running out of memory");
		return SXERR_ABORT;
	}
	/* Zero the structure */
	SyZero(pException,sizeof(ph7_exception));
	/* Initialize fields */
	SySetInit(&pException->sEntry,&pGen->pVm->sAllocator,sizeof(ph7_exception_block));
	pException->pVm = pGen->pVm;
	/* Create the try block */
	rc = GenStateEnterBlock(&(*pGen),GEN_BLOCK_EXCEPTION,PH7_VmInstrLength(pGen->pVm),0,&pTry);
	if( rc != SXRET_OK ){
		return SXERR_ABORT;
	}
	/* Emit the 'LOAD_EXCEPTION' instruction */
	PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOAD_EXCEPTION,0,0,pException,&nJmpIdx);
	/* Fix the jump later when the destination is resolved */
	GenStateNewJumpFixup(pTry,PH7_OP_LOAD_EXCEPTION,nJmpIdx);
	pGen->pIn++; /* Jump the 'try' keyword */
	/* Compile the block */
	rc = PH7_CompileBlock(&(*pGen),0);
	if( rc == SXERR_ABORT ){
		return SXERR_ABORT;
	}
	/* Fix forward jumps now the destination is resolved */
	GenStateFixJumps(pTry,-1,PH7_VmInstrLength(pGen->pVm));
	/* Emit the 'POP_EXCEPTION' instruction */
	PH7_VmEmitInstr(pGen->pVm,PH7_OP_POP_EXCEPTION,0,0,pException,0);
	/* Leave the block */
	GenStateLeaveBlock(&(*pGen),0);
	/* Compile the catch block */
	if( pGen->pIn >= pGen->pEnd || (pGen->pIn->nType & PH7_TK_KEYWORD) == 0 || 
		SX_PTR_TO_INT(pGen->pIn->pUserData) != PH7_TKWRD_CATCH ){
			SyToken *pTok = pGen->pIn;
			if( pTok >= pGen->pEnd ){
				pTok--; /* Point back */
			}
			/* Unexpected token */
			rc = PH7_GenCompileError(&(*pGen),E_ERROR,pTok->nLine,
				"Try: Unexpected token '%z',expecting 'catch' block",&pTok->sData);
			if( rc == SXERR_ABORT ){
				return SXERR_ABORT;
			}
			return SXRET_OK;
	}
	/* Compile one or more catch blocks */
	for(;;){
		if( pGen->pIn >= pGen->pEnd || (pGen->pIn->nType & PH7_TK_KEYWORD) == 0
			|| SX_PTR_TO_INT(pGen->pIn->pUserData) != PH7_TKWRD_CATCH ){
				/* No more blocks */
				break;
		}
		/* Compile the catch block */
		rc = PH7_CompileCatch(&(*pGen),pException);
		if( rc == SXERR_ABORT ){
			return SXERR_ABORT;
		}
 	}
	return SXRET_OK;
}
/*
 * Compile a switch block.
 *  (See block-comment below for more information)
 */
static sxi32 GenStateCompileSwitchBlock(ph7_gen_state *pGen,sxu32 iTokenDelim,sxu32 *pBlockStart)
{
	sxi32 rc = SXRET_OK;
	while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & (PH7_TK_SEMI/*';'*/|PH7_TK_COLON/*':'*/)) == 0 ){
		/* Unexpected token */
		rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"Unexpected token '%z'",&pGen->pIn->sData);
		if( rc == SXERR_ABORT ){
			return SXERR_ABORT;
		}
		pGen->pIn++;
	}
	pGen->pIn++;
	/* First instruction to execute in this block. */
	*pBlockStart = PH7_VmInstrLength(pGen->pVm);
	/* Compile the block until we hit a case/default/endswitch keyword
	 * or the '}' token */
	for(;;){
		if( pGen->pIn >= pGen->pEnd ){
			/* No more input to process */
			break;
		}
		rc = SXRET_OK;
		if( (pGen->pIn->nType & PH7_TK_KEYWORD) == 0 ){
			if( pGen->pIn->nType & PH7_TK_CCB /*'}' */ ){
				if( iTokenDelim != PH7_TK_CCB ){
					/* Unexpected token */
					rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"Unexpected token '%z'",
						&pGen->pIn->sData);
					if( rc == SXERR_ABORT ){
						return SXERR_ABORT;
					}
					/* FALL THROUGH */
				}
				rc = SXERR_EOF;
				break;
			}
		}else{
			sxi32 nKwrd;
			/* Extract the keyword */
			nKwrd = SX_PTR_TO_INT(pGen->pIn->pUserData);
			if( nKwrd == PH7_TKWRD_CASE || nKwrd == PH7_TKWRD_DEFAULT ){
				break;
			}
			if( nKwrd == PH7_TKWRD_ENDSWITCH /* endswitch; */){
				if( iTokenDelim != PH7_TK_KEYWORD ){
					/* Unexpected token */
					rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"Unexpected token '%z'",
						&pGen->pIn->sData);
					if( rc == SXERR_ABORT ){
						return SXERR_ABORT;
					}
					/* FALL THROUGH */
				}
				/* Block compiled */
				break;
			}
		}
		/* Compile block */
		rc = PH7_CompileBlock(&(*pGen),0);
		if( rc == SXERR_ABORT ){
			return SXERR_ABORT;
		}
	}
	return rc;
}
/*
 * Compile a case eXpression.
 *  (See block-comment below for more information)
 */
static sxi32 GenStateCompileCaseExpr(ph7_gen_state *pGen,ph7_case_expr *pExpr)
{
	SySet *pInstrContainer;
	SyToken *pEnd,*pTmp;
	sxi32 iNest = 0;
	sxi32 rc;
	/* Delimit the expression */
	pEnd = pGen->pIn;
	while( pEnd < pGen->pEnd ){
		if( pEnd->nType & PH7_TK_LPAREN /*(*/ ){
			/* Increment nesting level */
			iNest++;
		}else if( pEnd->nType & PH7_TK_RPAREN /*)*/ ){
			/* Decrement nesting level */
			iNest--;
		}else if( pEnd->nType & (PH7_TK_SEMI/*';'*/|PH7_TK_COLON/*;'*/) && iNest < 1 ){
			break;
		}
		pEnd++;
	}
	if( pGen->pIn >= pEnd ){
		rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,"Empty case expression");
		if( rc == SXERR_ABORT ){
			/* Error count limit reached,abort immediately */
			return SXERR_ABORT;
		}
	}
	/* Swap token stream */
	pTmp = pGen->pEnd;
	pGen->pEnd = pEnd;
	pInstrContainer = PH7_VmGetByteCodeContainer(pGen->pVm);
	PH7_VmSetByteCodeContainer(pGen->pVm,&pExpr->aByteCode);
	rc = PH7_CompileExpr(&(*pGen),0,0);
	/* Emit the done instruction */
	PH7_VmEmitInstr(pGen->pVm,PH7_OP_DONE,(rc != SXERR_EMPTY ? 1 : 0),0,0,0);
	PH7_VmSetByteCodeContainer(pGen->pVm,pInstrContainer); 
	/* Update token stream */
	pGen->pIn  = pEnd;
	pGen->pEnd = pTmp;
	if( rc == SXERR_ABORT ){
		return SXERR_ABORT;
	}
	return SXRET_OK;
}
/*
 * Compile the smart switch statement.
 * According to the PHP language reference manual
 *  The switch statement is similar to a series of IF statements on the same expression.
 *  In many occasions, you may want to compare the same variable (or expression) with many
 *  different values, and execute a different piece of code depending on which value it equals to.
 *  This is exactly what the switch statement is for.
 *  Note: Note that unlike some other languages, the continue statement applies to switch and acts
 *  similar to break. If you have a switch inside a loop and wish to continue to the next iteration
 *  of the outer loop, use continue 2. 
 *  Note that switch/case does loose comparision. 
 *  It is important to understand how the switch statement is executed in order to avoid mistakes.
 *  The switch statement executes line by line (actually, statement by statement).
 *  In the beginning, no code is executed. Only when a case statement is found with a value that
 *  matches the value of the switch expression does PHP begin to execute the statements.
 *  PHP continues to execute the statements until the end of the switch block, or the first time
 *  it sees a break statement. If you don't write a break statement at the end of a case's statement list.
 *  In a switch statement, the condition is evaluated only once and the result is compared to each
 *  case statement. In an elseif statement, the condition is evaluated again. If your condition
 *  is more complicated than a simple compare and/or is in a tight loop, a switch may be faster.
 *  The statement list for a case can also be empty, which simply passes control into the statement
 *  list for the next case. 
 *  The case expression may be any expression that evaluates to a simple type, that is, integer
 *  or floating-point numbers and strings.
 */
static sxi32 PH7_CompileSwitch(ph7_gen_state *pGen)
{
	GenBlock *pSwitchBlock;
	SyToken *pTmp,*pEnd;
	ph7_switch *pSwitch;
	sxu32 nToken;
	sxu32 nLine;
	sxi32 rc;
	nLine = pGen->pIn->nLine;
	/* Jump the 'switch' keyword */
	pGen->pIn++;    
	if( pGen->pIn >= pGen->pEnd || (pGen->pIn->nType & PH7_TK_LPAREN) == 0 ){
		/* Syntax error */
		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Expected '(' after 'switch' keyword");
		if( rc == SXERR_ABORT ){
			/* Error count limit reached,abort immediately */
			return SXERR_ABORT;
		}
		goto Synchronize;
	}
	/* Jump the left parenthesis '(' */
	pGen->pIn++; 
	pEnd = 0; /* cc warning */
	/* Create the loop block */
	rc = GenStateEnterBlock(&(*pGen),GEN_BLOCK_LOOP|GEN_BLOCK_SWITCH,
		PH7_VmInstrLength(pGen->pVm),0,&pSwitchBlock);
	if( rc != SXRET_OK ){
		return SXERR_ABORT;
	}
	/* Delimit the condition */
	PH7_DelimitNestedTokens(pGen->pIn,pGen->pEnd,PH7_TK_LPAREN /* '(' */,PH7_TK_RPAREN /* ')' */,&pEnd);
	if( pGen->pIn == pEnd || pEnd >= pGen->pEnd ){
		/* Empty expression */
		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Expected expression after 'switch' keyword");
		if( rc == SXERR_ABORT ){
			/* Error count limit reached,abort immediately */
			return SXERR_ABORT;
		}
	}
	/* Swap token streams */
	pTmp = pGen->pEnd;
	pGen->pEnd = pEnd;
	/* Compile the expression */
	rc = PH7_CompileExpr(&(*pGen),0,0);
	if( rc == SXERR_ABORT ){
		/* Expression handler request an operation abort [i.e: Out-of-memory] */
		return SXERR_ABORT;
	}
	/* Update token stream */
	while(pGen->pIn < pEnd ){
		rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,
			"Switch: Unexpected token '%z'",&pGen->pIn->sData);
		if( rc == SXERR_ABORT ){
			return SXERR_ABORT;
		}
		pGen->pIn++;
	}
	pGen->pIn  = &pEnd[1];
	pGen->pEnd = pTmp;
	if( pGen->pIn >= pGen->pEnd || &pGen->pIn[1] >= pGen->pEnd ||
		(pGen->pIn->nType & (PH7_TK_OCB/*'{'*/|PH7_TK_COLON/*:*/)) == 0 ){
			pTmp = pGen->pIn;
			if( pTmp >= pGen->pEnd ){
				pTmp--;
			}
			/* Unexpected token */
			rc = PH7_GenCompileError(&(*pGen),E_ERROR,pTmp->nLine,"Switch: Unexpected token '%z'",&pTmp->sData);
			if( rc == SXERR_ABORT ){
				return SXERR_ABORT;
			}
			goto Synchronize;
	}
	/* Set the delimiter token */
	if( pGen->pIn->nType & PH7_TK_COLON ){
		nToken = PH7_TK_KEYWORD;
		/* Stop compilation when the 'endswitch;' keyword is seen */
	}else{
		nToken = PH7_TK_CCB; /* '}' */
	}
	pGen->pIn++; /* Jump the leading curly braces/colons */
	/* Create the switch blocks container */
	pSwitch = (ph7_switch *)SyMemBackendAlloc(&pGen->pVm->sAllocator,sizeof(ph7_switch));
	if( pSwitch == 0 ){
		/* Abort compilation */
		PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"Fatal, PH7 is running out of memory");
		return SXERR_ABORT; 
	}
	/* Zero the structure */
	SyZero(pSwitch,sizeof(ph7_switch));
	/* Initialize fields */
	SySetInit(&pSwitch->aCaseExpr,&pGen->pVm->sAllocator,sizeof(ph7_case_expr));
	/* Emit the switch instruction */
	PH7_VmEmitInstr(pGen->pVm,PH7_OP_SWITCH,0,0,pSwitch,0);
	/* Compile case blocks */
	for(;;){
		sxu32 nKwrd;
		if( pGen->pIn >= pGen->pEnd ){
			/* No more input to process */
			break;
		}
		if( (pGen->pIn->nType & PH7_TK_KEYWORD) == 0 ){
			if( nToken != PH7_TK_CCB || (pGen->pIn->nType & PH7_TK_CCB /*}*/) == 0 ){
				/* Unexpected token */
				rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"Switch: Unexpected token '%z'",
					&pGen->pIn->sData);
				if( rc == SXERR_ABORT ){
					return SXERR_ABORT;
				}
				/* FALL THROUGH */
			}
			/* Block compiled */
			break;
		}
		/* Extract the keyword */
		nKwrd = SX_PTR_TO_INT(pGen->pIn->pUserData);
		if( nKwrd == PH7_TKWRD_ENDSWITCH /* endswitch; */){
			if( nToken != PH7_TK_KEYWORD ){
				/* Unexpected token */
				rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"Switch: Unexpected token '%z'",
					&pGen->pIn->sData);
				if( rc == SXERR_ABORT ){
					return SXERR_ABORT;
				}
				/* FALL THROUGH */
			}
			/* Block compiled */
			break;
		}
		if( nKwrd == PH7_TKWRD_DEFAULT ){
			/*
			 * Accroding to the PHP language reference manual
			 *  A special case is the default case. This case matches anything
			 *  that wasn't matched by the other cases.
			 */
			if( pSwitch->nDefault > 0 ){
				/* Default case already compiled */ 
				rc = PH7_GenCompileError(&(*pGen),E_WARNING,pGen->pIn->nLine,"Switch: 'default' case already compiled");
				if( rc == SXERR_ABORT ){
					return SXERR_ABORT;
				}
			}
			pGen->pIn++; /* Jump the 'default' keyword */
			/* Compile the default block */
			rc = GenStateCompileSwitchBlock(pGen,nToken,&pSwitch->nDefault);
			if( rc == SXERR_ABORT){
				return SXERR_ABORT;
			}else if( rc == SXERR_EOF ){
				break;
			}
		}else if( nKwrd == PH7_TKWRD_CASE ){
			ph7_case_expr sCase;
			/* Standard case block */
			pGen->pIn++; /* Jump the 'case' keyword */
			/* initialize the structure */
			SySetInit(&sCase.aByteCode,&pGen->pVm->sAllocator,sizeof(VmInstr));
			/* Compile the case expression */
			rc = GenStateCompileCaseExpr(pGen,&sCase);
			if( rc == SXERR_ABORT ){
				return SXERR_ABORT;
			}
			/* Compile the case block */
			rc = GenStateCompileSwitchBlock(pGen,nToken,&sCase.nStart);
			/* Insert in the switch container */
			SySetPut(&pSwitch->aCaseExpr,(const void *)&sCase);
			if( rc == SXERR_ABORT){
				return SXERR_ABORT;
			}else if( rc == SXERR_EOF ){
				break;
			}
		}else{
			/* Unexpected token */
			rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"Switch: Unexpected token '%z'",
				&pGen->pIn->sData);
			if( rc == SXERR_ABORT ){
				return SXERR_ABORT;
			}
			break;
		}
	}
	/* Fix all jumps now the destination is resolved */
	pSwitch->nOut = PH7_VmInstrLength(pGen->pVm);
	GenStateFixJumps(pSwitchBlock,-1,PH7_VmInstrLength(pGen->pVm));
	/* Release the loop block */
	GenStateLeaveBlock(pGen,0);
	if( pGen->pIn < pGen->pEnd ){
		/* Jump the trailing curly braces or the endswitch keyword*/
		pGen->pIn++;
	}
	/* Statement successfully compiled */
	return SXRET_OK;
Synchronize:
	/* Synchronize with the first semi-colon */
	while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_SEMI) == 0 ){
		pGen->pIn++;
	}
	return SXRET_OK;
}
/*
 * Generate bytecode for a given expression tree.
 * If something goes wrong while generating bytecode
 * for the expression tree (A very unlikely scenario)
 * this function takes care of generating the appropriate
 * error message.
 */
static sxi32 GenStateEmitExprCode(
	ph7_gen_state *pGen,  /* Code generator state */
	ph7_expr_node *pNode, /* Root of the expression tree */
	sxi32 iFlags /* Control flags */
	)
{
	VmInstr *pInstr;
	sxu32 nJmpIdx;
	sxi32 iP1 = 0;
	sxu32 iP2 = 0;
	void *p3  = 0;
	sxi32 iVmOp;
	sxi32 rc;
	if( pNode->xCode ){
		SyToken *pTmpIn,*pTmpEnd;
		/* Compile node */
		SWAP_DELIMITER(pGen,pNode->pStart,pNode->pEnd);
		rc = pNode->xCode(&(*pGen),iFlags);
		RE_SWAP_DELIMITER(pGen);
		return rc;
	}
	if( pNode->pOp == 0 ){
		PH7_GenCompileError(&(*pGen),E_ERROR,pNode->pStart->nLine,
			"Invalid expression node,PH7 is aborting compilation");
		return SXERR_ABORT;
	}
	iVmOp = pNode->pOp->iVmOp;
	if( pNode->pOp->iOp == EXPR_OP_QUESTY ){
		sxu32 nJz,nJmp;
		/* Ternary operator require special handling */
		/* Phase#1: Compile the condition */
		rc = GenStateEmitExprCode(&(*pGen),pNode->pCond,iFlags);
		if( rc != SXRET_OK ){
			return rc;
		}
		nJz = nJmp = 0; /* cc -O6 warning */
		/* Phase#2: Emit the false jump */
		PH7_VmEmitInstr(pGen->pVm,PH7_OP_JZ,0,0,0,&nJz);
		if( pNode->pLeft ){
			/* Phase#3: Compile the 'then' expression  */
			rc = GenStateEmitExprCode(&(*pGen),pNode->pLeft,iFlags);
			if( rc != SXRET_OK ){
				return rc;
			}
		}
		/* Phase#4: Emit the unconditional jump */
		PH7_VmEmitInstr(pGen->pVm,PH7_OP_JMP,0,0,0,&nJmp);
		/* Phase#5: Fix the false jump now the jump destination is resolved. */
		pInstr = PH7_VmGetInstr(pGen->pVm,nJz);
		if( pInstr ){
			pInstr->iP2 = PH7_VmInstrLength(pGen->pVm);
		}
		/* Phase#6: Compile the 'else' expression */
		if( pNode->pRight ){
			rc = GenStateEmitExprCode(&(*pGen),pNode->pRight,iFlags);
			if( rc != SXRET_OK ){
				return rc;
			}
		}
		if( nJmp > 0 ){
			/* Phase#7: Fix the unconditional jump */
			pInstr = PH7_VmGetInstr(pGen->pVm,nJmp);
			if( pInstr ){
				pInstr->iP2 = PH7_VmInstrLength(pGen->pVm);
			}
		}
		/* All done */
		return SXRET_OK;
	}
	/* Generate code for the left tree */
	if( pNode->pLeft ){
		if( iVmOp == PH7_OP_CALL ){
			ph7_expr_node **apNode;
			sxi32 n;
			/* Recurse and generate bytecodes for function arguments */
			apNode = (ph7_expr_node **)SySetBasePtr(&pNode->aNodeArgs);
			/* Read-only load */
			iFlags |= EXPR_FLAG_RDONLY_LOAD;
			for( n = 0 ; n < (sxi32)SySetUsed(&pNode->aNodeArgs) ; ++n ){
				rc = GenStateEmitExprCode(&(*pGen),apNode[n],iFlags&~EXPR_FLAG_LOAD_IDX_STORE);
				if( rc != SXRET_OK ){
					return rc;
				}
			}
			/* Total number of given arguments */
			iP1 = (sxi32)SySetUsed(&pNode->aNodeArgs);
			/* Remove stale flags now */
			iFlags &= ~EXPR_FLAG_RDONLY_LOAD;
		}
		rc = GenStateEmitExprCode(&(*pGen),pNode->pLeft,iFlags);
		if( rc != SXRET_OK ){
			return rc;
		}
		if( iVmOp == PH7_OP_CALL ){
			pInstr = PH7_VmPeekInstr(pGen->pVm);
			if( pInstr ){
				if ( pInstr->iOp == PH7_OP_LOADC ){
					/* Prevent constant expansion */
					pInstr->iP1 = 0;
				}else if( pInstr->iOp == PH7_OP_MEMBER /* $a->b(1,2,3) */ || pInstr->iOp == PH7_OP_NEW ){
					/* Method call,flag that */
					pInstr->iP2 = 1;
				}
			}
		}else if( iVmOp == PH7_OP_LOAD_IDX ){
			ph7_expr_node **apNode;
			sxi32 n;
			/* Recurse and generate bytecodes for array index */
			apNode = (ph7_expr_node **)SySetBasePtr(&pNode->aNodeArgs);
			for( n = 0 ; n < (sxi32)SySetUsed(&pNode->aNodeArgs) ; ++n ){
				rc = GenStateEmitExprCode(&(*pGen),apNode[n],iFlags&~EXPR_FLAG_LOAD_IDX_STORE);
				if( rc != SXRET_OK ){
					return rc;
				}
			}
			if( SySetUsed(&pNode->aNodeArgs) > 0 ){
				iP1 = 1; /* Node have an index associated with it */
			}
			if( iFlags & EXPR_FLAG_LOAD_IDX_STORE ){
				/* Create an empty entry when the desired index is not found */
				iP2 = 1;
			}
		}else if( pNode->pOp->iOp == EXPR_OP_COMMA ){
			/* POP the left node */
			PH7_VmEmitInstr(pGen->pVm,PH7_OP_POP,1,0,0,0);
		}
	}
	rc = SXRET_OK;
	nJmpIdx = 0;
	/* Generate code for the right tree */
	if( pNode->pRight ){
		if( iVmOp == PH7_OP_LAND ){
			/* Emit the false jump so we can short-circuit the logical and */
			PH7_VmEmitInstr(pGen->pVm,PH7_OP_JZ,1/* Keep the value on the stack */,0,0,&nJmpIdx);
		}else if (iVmOp == PH7_OP_LOR ){
			/* Emit the true jump so we can short-circuit the logical or*/
			PH7_VmEmitInstr(pGen->pVm,PH7_OP_JNZ,1/* Keep the value on the stack */,0,0,&nJmpIdx);
		}else if( pNode->pOp->iPrec == 18 /* Combined binary operators [i.e: =,'.=','+=',*=' ...] precedence */ ){
			iFlags |= EXPR_FLAG_LOAD_IDX_STORE;
		}
		rc = GenStateEmitExprCode(&(*pGen),pNode->pRight,iFlags);
		if( iVmOp == PH7_OP_STORE ){
			pInstr = PH7_VmPeekInstr(pGen->pVm);
			if( pInstr ){
				if( pInstr->iOp == PH7_OP_LOAD_LIST ){
					/* Hide the STORE instruction */
					iVmOp = 0;
				}else if(pInstr->iOp == PH7_OP_MEMBER ){
					/* Perform a member store operation [i.e: $this->x = 50] */
					iP2 = 1;
				}else{
					if( pInstr->iOp == PH7_OP_LOAD_IDX ){
						/* Transform the STORE instruction to STORE_IDX instruction */
						iVmOp = PH7_OP_STORE_IDX;
						iP1 = pInstr->iP1;
					}else{
						p3 = pInstr->p3;
					}
					/* POP the last dynamic load instruction */
					(void)PH7_VmPopInstr(pGen->pVm);
				}
			}
		}else if( iVmOp == PH7_OP_STORE_REF ){
			pInstr = PH7_VmPopInstr(pGen->pVm);
			if( pInstr ){
				if( pInstr->iOp == PH7_OP_LOAD_IDX ){
					/* Array insertion by reference [i.e: $pArray[] =& $some_var; ]
					 * We have to convert the STORE_REF instruction into STORE_IDX_REF 
					 */
					iVmOp = PH7_OP_STORE_IDX_REF;
					iP1 = pInstr->iP1;
					iP2 = pInstr->iP2;
					p3  = pInstr->p3;
				}else{
					p3 = pInstr->p3;
				}
			}
		}
	}
	if( iVmOp > 0 ){
		if( iVmOp == PH7_OP_INCR || iVmOp == PH7_OP_DECR ){
			if( pNode->iFlags & EXPR_NODE_PRE_INCR ){
				/* Pre-increment/decrement operator [i.e: ++$i,--$j ] */
				iP1 = 1;
			}
		}else if( iVmOp == PH7_OP_NEW ){
			pInstr = PH7_VmPeekInstr(pGen->pVm);
			if( pInstr && pInstr->iOp == PH7_OP_CALL ){
				VmInstr *pPrev;
				pPrev = PH7_VmPeekNextInstr(pGen->pVm);
				if( pPrev == 0 || pPrev->iOp != PH7_OP_MEMBER ){
					/* Pop the call instruction */
					iP1 = pInstr->iP1;
					(void)PH7_VmPopInstr(pGen->pVm);
				}
			}
		}else if( iVmOp == PH7_OP_MEMBER){
			if( pNode->pOp->iOp == EXPR_OP_DC /* '::' */){
				/* Static member access,remember that */
				iP1 = 1;
				pInstr = PH7_VmPeekInstr(pGen->pVm);
				if( pInstr && pInstr->iOp == PH7_OP_LOAD ){
					p3 = pInstr->p3;
					(void)PH7_VmPopInstr(pGen->pVm);
				}
			}
		}
		/* Finally,emit the VM instruction associated with this operator */
		PH7_VmEmitInstr(pGen->pVm,iVmOp,iP1,iP2,p3,0);
		if( nJmpIdx > 0 ){
			/* Fix short-circuited jumps now the destination is resolved */
			pInstr = PH7_VmGetInstr(pGen->pVm,nJmpIdx);
			if( pInstr ){
				pInstr->iP2 = PH7_VmInstrLength(pGen->pVm);
			}
		}
	}
	return rc;
}
/*
 * Compile a PHP expression.
 * According to the PHP language reference manual:
 *  Expressions are the most important building stones of PHP.
 *  In PHP, almost anything you write is an expression.
 *  The simplest yet most accurate way to define an expression
 *  is "anything that has a value". 
 * If something goes wrong while compiling the expression,this
 * function takes care of generating the appropriate error
 * message.
 */
static sxi32 PH7_CompileExpr(
	ph7_gen_state *pGen, /* Code generator state */
	sxi32 iFlags,        /* Control flags */
	sxi32 (*xTreeValidator)(ph7_gen_state *,ph7_expr_node *) /* Node validator callback.NULL otherwise */
	)
{
	ph7_expr_node *pRoot;
	SySet sExprNode;
	SyToken *pEnd;
	sxi32 nExpr;
	sxi32 iNest;
	sxi32 rc;
	/* Initialize worker variables */
	nExpr = 0;
	pRoot = 0;
	SySetInit(&sExprNode,&pGen->pVm->sAllocator,sizeof(ph7_expr_node *));
	SySetAlloc(&sExprNode,0x10);
	rc = SXRET_OK;
	/* Delimit the expression */
	pEnd = pGen->pIn;
	iNest = 0;
	while( pEnd < pGen->pEnd ){
		if( pEnd->nType & PH7_TK_OCB /* '{' */ ){
			/* Ticket 1433-30: Annonymous/Closure functions body */
			iNest++;
		}else if(pEnd->nType & PH7_TK_CCB /* '}' */ ){
			iNest--;
		}else if( pEnd->nType & PH7_TK_SEMI /* ';' */ ){
			if( iNest <= 0 ){
				break;
			}
		}
		pEnd++;
	}
	if( iFlags & EXPR_FLAG_COMMA_STATEMENT ){
		SyToken *pEnd2 = pGen->pIn;
		iNest = 0;
		/* Stop at the first comma */
		while( pEnd2 < pEnd ){
			if( pEnd2->nType & (PH7_TK_OCB/*'{'*/|PH7_TK_OSB/*'['*/|PH7_TK_LPAREN/*'('*/) ){
				iNest++;
			}else if(pEnd2->nType & (PH7_TK_CCB/*'}'*/|PH7_TK_CSB/*']'*/|PH7_TK_RPAREN/*')'*/)){
				iNest--;
			}else if( pEnd2->nType & PH7_TK_COMMA /*','*/ ){
				if( iNest <= 0 ){
					break;
				}
			}
			pEnd2++;
		}
		if( pEnd2 <pEnd ){
			pEnd = pEnd2;
		}
	}
	if( pEnd > pGen->pIn ){
		SyToken *pTmp = pGen->pEnd;
		/* Swap delimiter */
		pGen->pEnd = pEnd;
		/* Try to get an expression tree */
		rc = PH7_ExprMakeTree(&(*pGen),&sExprNode,&pRoot);
		if( rc == SXRET_OK && pRoot ){
			rc = SXRET_OK;
			if( xTreeValidator ){
				/* Call the upper layer validator callback */
				rc = xTreeValidator(&(*pGen),pRoot);
			}
			if( rc != SXERR_ABORT ){
				/* Generate code for the given tree */
				rc = GenStateEmitExprCode(&(*pGen),pRoot,iFlags);
			}
			nExpr = 1;
		}
		/* Release the whole tree */
		PH7_ExprFreeTree(&(*pGen),&sExprNode);
		/* Synchronize token stream */
		pGen->pEnd = pTmp;
		pGen->pIn  = pEnd;
		if( rc == SXERR_ABORT ){
			SySetRelease(&sExprNode);
			return SXERR_ABORT;
		}
	}
	SySetRelease(&sExprNode);
	return nExpr > 0 ? SXRET_OK : SXERR_EMPTY;
}
/*
 * Return a pointer to the node construct handler associated
 * with a given node type [i.e: string,integer,float,...].
 */
PH7_PRIVATE ProcNodeConstruct PH7_GetNodeHandler(sxu32 nNodeType)
{
	if( nNodeType & PH7_TK_NUM ){
		/* Numeric literal: Either real or integer */
		return PH7_CompileNumLiteral;
	}else if( nNodeType & PH7_TK_DSTR ){
		/* Double quoted string */
		return PH7_CompileString;
	}else if( nNodeType & PH7_TK_SSTR ){
		/* Single quoted string */
		return PH7_CompileSimpleString;
	}else if( nNodeType & PH7_TK_HEREDOC ){
		/* Heredoc */
		return PH7_CompileHereDoc;
	}else if( nNodeType & PH7_TK_NOWDOC ){
		/* Nowdoc */
		return PH7_CompileNowDoc;
	}else if( nNodeType & PH7_TK_BSTR ){
		/* Backtick quoted string */
		return PH7_CompileBacktic;
	}
	return 0;
}
/*
 * PHP Language construct table.
 */
static const LangConstruct aLangConstruct[] = {
	{ PH7_TKWRD_ECHO,     PH7_CompileEcho     }, /* echo language construct */
	{ PH7_TKWRD_IF,       PH7_CompileIf       }, /* if statement */
	{ PH7_TKWRD_FOR,      PH7_CompileFor      }, /* for statement */
	{ PH7_TKWRD_WHILE,    PH7_CompileWhile    }, /* while statement */
	{ PH7_TKWRD_FOREACH,  PH7_CompileForeach  }, /* foreach statement */
	{ PH7_TKWRD_FUNCTION, PH7_CompileFunction }, /* function statement */
	{ PH7_TKWRD_CONTINUE, PH7_CompileContinue }, /* continue statement */
	{ PH7_TKWRD_BREAK,    PH7_CompileBreak    }, /* break statement */
	{ PH7_TKWRD_RETURN,   PH7_CompileReturn   }, /* return statement */
	{ PH7_TKWRD_SWITCH,   PH7_CompileSwitch   }, /* Switch statement */
	{ PH7_TKWRD_DO,       PH7_CompileDoWhile  }, /* do{ }while(); statement */
	{ PH7_TKWRD_GLOBAL,   PH7_CompileGlobal   }, /* global statement */
	{ PH7_TKWRD_STATIC,   PH7_CompileStatic   }, /* static statement */
	{ PH7_TKWRD_DIE,      PH7_CompileHalt     }, /* die language construct */
	{ PH7_TKWRD_EXIT,     PH7_CompileHalt     }, /* exit language construct */
	{ PH7_TKWRD_TRY,      PH7_CompileTry      }, /* try statement */
	{ PH7_TKWRD_THROW,    PH7_CompileThrow    }, /* throw statement */
	{ PH7_TKWRD_GOTO,     PH7_CompileGoto     }, /* goto statement */
	{ PH7_TKWRD_CONST,    PH7_CompileConstant }, /* const statement */
	{ PH7_TKWRD_VAR,      PH7_CompileVar      }, /* var statement */
	{ PH7_TKWRD_NAMESPACE, PH7_CompileNamespace }, /* namespace statement */
	{ PH7_TKWRD_USE,      PH7_CompileUse      },  /* use statement */
	{ PH7_TKWRD_DECLARE,  PH7_CompileDeclare  }   /* declare statement */
};
/*
 * Return a pointer to the statement handler routine associated
 * with a given PHP keyword [i.e: if,for,while,...].
 */
static ProcLangConstruct GenStateGetStatementHandler(
	sxu32 nKeywordID,   /* Keyword  ID*/
	SyToken *pLookahed  /* Look-ahead token */
	)
{
	sxu32 n = 0;
	for(;;){
		if( n >= SX_ARRAYSIZE(aLangConstruct) ){
			break;
		}
		if( aLangConstruct[n].nID == nKeywordID ){
			if( nKeywordID == PH7_TKWRD_STATIC && pLookahed && (pLookahed->nType & PH7_TK_OP)){
				const ph7_expr_op *pOp = (const ph7_expr_op *)pLookahed->pUserData;
				if( pOp && pOp->iOp == EXPR_OP_DC /*::*/){
					/* 'static' (class context),return null */
					return 0;
				}
			}
			/* Return a pointer to the handler.
			*/
			return aLangConstruct[n].xConstruct;
		}
		n++;
	}
	if( pLookahed ){
		if(nKeywordID == PH7_TKWRD_INTERFACE && (pLookahed->nType & PH7_TK_ID) ){
			return PH7_CompileClassInterface;
		}else if(nKeywordID == PH7_TKWRD_CLASS && (pLookahed->nType & PH7_TK_ID) ){
			return PH7_CompileClass;
		}else if( nKeywordID == PH7_TKWRD_ABSTRACT && (pLookahed->nType & PH7_TK_KEYWORD)
			&& SX_PTR_TO_INT(pLookahed->pUserData) == PH7_TKWRD_CLASS ){
				return PH7_CompileAbstractClass;
		}else if( nKeywordID == PH7_TKWRD_FINAL && (pLookahed->nType & PH7_TK_KEYWORD)
			&& SX_PTR_TO_INT(pLookahed->pUserData) == PH7_TKWRD_CLASS ){
				return PH7_CompileFinalClass;
		}
	}
	/* Not a language construct */
	return 0;
}
/*
 * Check if the given keyword is in fact a PHP language construct.
 * Return TRUE on success. FALSE otheriwse.
 */
static int GenStateisLangConstruct(sxu32 nKeyword)
{
	int rc;
	rc = PH7_IsLangConstruct(nKeyword,TRUE);
	if( rc == FALSE ){
		if( nKeyword == PH7_TKWRD_SELF || nKeyword == PH7_TKWRD_PARENT || nKeyword == PH7_TKWRD_STATIC
			/*|| nKeyword == PH7_TKWRD_CLASS || nKeyword == PH7_TKWRD_FINAL || nKeyword == PH7_TKWRD_EXTENDS 
			  || nKeyword == PH7_TKWRD_ABSTRACT || nKeyword == PH7_TKWRD_INTERFACE 
			  || nKeyword == PH7_TKWRD_PUBLIC || nKeyword == PH7_TKWRD_PROTECTED 
			  || nKeyword == PH7_TKWRD_PRIVATE || nKeyword == PH7_TKWRD_IMPLEMENTS
			*/
			){
				rc = TRUE;
		}
	}
	return rc;
}
/*
 * Compile a PHP chunk.
 * If something goes wrong while compiling the PHP chunk,this function
 * takes care of generating the appropriate error message.
 */
static sxi32 GenStateCompileChunk(
	ph7_gen_state *pGen, /* Code generator state */
	sxi32 iFlags         /* Compile flags */
	)
{
	ProcLangConstruct xCons;
	sxi32 rc;
	rc = SXRET_OK; /* Prevent compiler warning */
	for(;;){
		if( pGen->pIn >= pGen->pEnd ){
			/* No more input to process */
			break;
		}
		if( pGen->pIn->nType & PH7_TK_OCB /* '{' */ ){
			/* Compile block */
			rc = PH7_CompileBlock(&(*pGen),0);
			if( rc == SXERR_ABORT ){
				break;
			}
		}else{
			xCons = 0;
			if( pGen->pIn->nType & PH7_TK_KEYWORD ){
				sxu32 nKeyword = (sxu32)SX_PTR_TO_INT(pGen->pIn->pUserData);
				/* Try to extract a language construct handler */
				xCons = GenStateGetStatementHandler(nKeyword,(&pGen->pIn[1] < pGen->pEnd) ? &pGen->pIn[1] : 0);
				if( xCons == 0 && GenStateisLangConstruct(nKeyword) == FALSE ){
					rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,
						"Syntax error: Unexpected keyword '%z'",
						&pGen->pIn->sData);
					if( rc == SXERR_ABORT ){
						break;
					}
					/* Synchronize with the first semi-colon and avoid compiling
					 * this erroneous statement.
					 */
					xCons = PH7_ErrorRecover;
				}
			}else if( (pGen->pIn->nType & PH7_TK_ID) && (&pGen->pIn[1] < pGen->pEnd)
				&& (pGen->pIn[1].nType & PH7_TK_COLON /*':'*/) ){
				/* Label found [i.e: Out: ],point to the routine responsible of compiling it */
				xCons = PH7_CompileLabel;
			}
			if( xCons == 0 ){
				/* Assume an expression an try to compile it */
				rc = PH7_CompileExpr(&(*pGen),0,0);
				if(  rc != SXERR_EMPTY ){
					/* Pop l-value */
					PH7_VmEmitInstr(pGen->pVm,PH7_OP_POP,1,0,0,0);
				}
			}else{
				/* Go compile the sucker */
				rc = xCons(&(*pGen));
			}
			if( rc == SXERR_ABORT ){
				/* Request to abort compilation */
				break;
			}
		}
		/* Ignore trailing semi-colons ';' */
		while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_SEMI) ){
			pGen->pIn++;
		}
		if( iFlags & PH7_COMPILE_SINGLE_STMT ){
			/* Compile a single statement and return */
			break;
		}
		/* LOOP ONE */
		/* LOOP TWO */
		/* LOOP THREE */
		/* LOOP FOUR */
	}
	/* Return compilation status */
	return rc;
}
/*
 * Compile a Raw PHP chunk.
 * If something goes wrong while compiling the PHP chunk,this function
 * takes care of generating the appropriate error message.
 */
static sxi32 PH7_CompilePHP(
	ph7_gen_state *pGen,  /* Code generator state */
	SySet *pTokenSet,     /* Token set */
	int is_expr           /* TRUE if we are dealing with a simple expression */
	)
{
	SyToken *pScript = pGen->pRawIn; /* Script to compile */
	sxi32 rc;
	/* Reset the token set */
	SySetReset(&(*pTokenSet));
	/* Mark as the default token set */
	pGen->pTokenSet = &(*pTokenSet);
	/* Advance the stream cursor */
	pGen->pRawIn++;
	/* Tokenize the PHP chunk first */
	PH7_TokenizePHP(SyStringData(&pScript->sData),SyStringLength(&pScript->sData),pScript->nLine,&(*pTokenSet));
	/* Point to the head and tail of the token stream. */
	pGen->pIn  = (SyToken *)SySetBasePtr(pTokenSet);
	pGen->pEnd = &pGen->pIn[SySetUsed(pTokenSet)];
	if( is_expr ){
		rc = SXERR_EMPTY;
		if( pGen->pIn < pGen->pEnd ){
			/* A simple expression,compile it */
			rc = PH7_CompileExpr(pGen,0,0);
		}
		/* Emit the DONE instruction */
		PH7_VmEmitInstr(pGen->pVm,PH7_OP_DONE,(rc != SXERR_EMPTY ? 1 : 0),0,0,0);
		return SXRET_OK;
	}
	if( pGen->pIn < pGen->pEnd && ( pGen->pIn->nType & PH7_TK_EQUAL ) ){
		static const sxu32 nKeyID = PH7_TKWRD_ECHO;
		/*
		 * Shortcut syntax for the 'echo' language construct.
		 * According to the PHP reference manual:
		 *  echo() also has a shortcut syntax, where you can
		 *  immediately follow
		 *  the opening tag with an equals sign as follows:
		 *  <?= 4+5?> is the same as <?echo 4+5?>
		 * Symisc extension:
		 *   This short syntax works with all PHP opening
		 *   tags unlike the default PHP engine that handle
		 *   only short tag.
		 */
		/* Ticket 1433-009: Emulate the 'echo' call */
		pGen->pIn->nType = PH7_TK_KEYWORD;
		pGen->pIn->pUserData = SX_INT_TO_PTR(nKeyID);
		SyStringInitFromBuf(&pGen->pIn->sData,"echo",sizeof("echo")-1);
		rc = PH7_CompileExpr(pGen,0,0);
		if( rc != SXERR_EMPTY ){
			PH7_VmEmitInstr(pGen->pVm,PH7_OP_POP,1,0,0,0);
		}
		return SXRET_OK;
	}
	/* Compile the PHP chunk */
	rc = GenStateCompileChunk(pGen,0);
	/* Fix exceptions jumps */
	GenStateFixJumps(pGen->pCurrent,PH7_OP_THROW,PH7_VmInstrLength(pGen->pVm));
	/* Fix gotos now, the jump destination is resolved */
	if( SXERR_ABORT == GenStateFixGoto(&(*pGen),0) ){
		rc = SXERR_ABORT;
	}
	/* Reset container */
	SySetReset(&pGen->aGoto);
	SySetReset(&pGen->aLabel);
	/* Compilation result */
	return rc;
}
/*
 * Compile a raw chunk. The raw chunk can contain PHP code embedded
 * in HTML, XML and so on. This function handle all the stuff.
 * This is the only compile interface exported from this file.
 */
PH7_PRIVATE sxi32 PH7_CompileScript(
	ph7_vm *pVm,        /* Generate PH7 byte-codes for this Virtual Machine */
	SyString *pScript,  /* Script to compile */
	sxi32 iFlags        /* Compile flags */
	)
{
	SySet aPhpToken,aRawToken;
	ph7_gen_state *pCodeGen;
	ph7_value *pRawObj;
	sxu32 nObjIdx;
	sxi32 nRawObj;
	int is_expr;
	sxi32 rc;
	if( pScript->nByte < 1 ){
		/* Nothing to compile */
		return PH7_OK;
	}
	/* Initialize the tokens containers */
	SySetInit(&aRawToken,&pVm->sAllocator,sizeof(SyToken));
	SySetInit(&aPhpToken,&pVm->sAllocator,sizeof(SyToken));
	SySetAlloc(&aPhpToken,0xc0);
	is_expr = 0;
	if( iFlags & PH7_PHP_ONLY ){
		SyToken sTmp;
		/* PHP only: -*/
		sTmp.nLine = 1;
		sTmp.nType = PH7_TOKEN_PHP;
		sTmp.pUserData = 0;
		SyStringDupPtr(&sTmp.sData,pScript);
		SySetPut(&aRawToken,(const void *)&sTmp);
		if( iFlags & PH7_PHP_EXPR ){
			/* A simple PHP expression */
			is_expr = 1;
		}
	}else{
		/* Tokenize raw text */
		SySetAlloc(&aRawToken,32);
		PH7_TokenizeRawText(pScript->zString,pScript->nByte,&aRawToken);
	}
	pCodeGen = &pVm->sCodeGen;
	/* Process high-level tokens */
	pCodeGen->pRawIn = (SyToken *)SySetBasePtr(&aRawToken);
	pCodeGen->pRawEnd = &pCodeGen->pRawIn[SySetUsed(&aRawToken)];
	rc = PH7_OK;
	if( is_expr ){
		/* Compile the expression */
		rc = PH7_CompilePHP(pCodeGen,&aPhpToken,TRUE);
		goto cleanup;
	}
	nObjIdx = 0;
	/* Start the compilation process */
	for(;;){
		if( pCodeGen->pRawIn >= pCodeGen->pRawEnd ){
			break; /* No more tokens to process */
		}
		if( pCodeGen->pRawIn->nType & PH7_TOKEN_PHP ){
			/* Compile the PHP chunk */
			rc = PH7_CompilePHP(pCodeGen,&aPhpToken,FALSE);
			if( rc == SXERR_ABORT ){
				break;
			}
			continue;
		}
		/* Raw chunk: [i.e: HTML, XML, etc.] */
		nRawObj = 0;
		while( (pCodeGen->pRawIn < pCodeGen->pRawEnd) && (pCodeGen->pRawIn->nType != PH7_TOKEN_PHP) ){
			/* Consume the raw chunk without any processing */
			pRawObj = PH7_ReserveConstObj(&(*pVm),&nObjIdx);
			if( pRawObj == 0 ){
				rc = SXERR_MEM;
				break;
			}
			/* Mark as constant and emit the load constant instruction */
			PH7_MemObjInitFromString(pVm,pRawObj,&pCodeGen->pRawIn->sData);
			PH7_VmEmitInstr(&(*pVm),PH7_OP_LOADC,0,nObjIdx,0,0);
			++nRawObj;
			pCodeGen->pRawIn++; /* Next chunk */
		}
		if( nRawObj > 0 ){
			/* Emit the consume instruction */
			PH7_VmEmitInstr(&(*pVm),PH7_OP_CONSUME,nRawObj,0,0,0);
		}
	}
cleanup:
	SySetRelease(&aRawToken);
	SySetRelease(&aPhpToken);
	return rc;
}
/*
 * Utility routines.Initialize the code generator.
 */
PH7_PRIVATE sxi32 PH7_InitCodeGenerator(
	ph7_vm *pVm,       /* Target VM */
	ProcConsumer xErr, /* Error log consumer callabck  */
	void *pErrData     /* Last argument to xErr() */
	)
{
	ph7_gen_state *pGen = &pVm->sCodeGen;
	/* Zero the structure */
	SyZero(pGen,sizeof(ph7_gen_state));
	/* Initial state */
	pGen->pVm  = &(*pVm);
	pGen->xErr = xErr;
	pGen->pErrData = pErrData;
	SySetInit(&pGen->aLabel,&pVm->sAllocator,sizeof(Label));
	SySetInit(&pGen->aGoto,&pVm->sAllocator,sizeof(JumpFixup));
	SyHashInit(&pGen->hLiteral,&pVm->sAllocator,0,0);
	SyHashInit(&pGen->hVar,&pVm->sAllocator,0,0);
	/* Error log buffer */
	SyBlobInit(&pGen->sErrBuf,&pVm->sAllocator);
	/* General purpose working buffer */
	SyBlobInit(&pGen->sWorker,&pVm->sAllocator);
	/* Create the global scope */
	GenStateInitBlock(pGen,&pGen->sGlobal,GEN_BLOCK_GLOBAL,PH7_VmInstrLength(&(*pVm)),0);
	/* Point to the global scope */
	pGen->pCurrent = &pGen->sGlobal;
	return SXRET_OK;
}
/*
 * Utility routines. Reset the code generator to it's initial state.
 */
PH7_PRIVATE sxi32 PH7_ResetCodeGenerator(
	ph7_vm *pVm,       /* Target VM */
	ProcConsumer xErr, /* Error log consumer callabck  */
	void *pErrData     /* Last argument to xErr() */
	)
{
	ph7_gen_state *pGen = &pVm->sCodeGen;
	GenBlock *pBlock,*pParent;
	/* Reset state */
	SySetReset(&pGen->aLabel);
	SySetReset(&pGen->aGoto);
	SyBlobRelease(&pGen->sErrBuf);
	SyBlobRelease(&pGen->sWorker);
	/* Point to the global scope */
	pBlock = pGen->pCurrent;
	while( pBlock->pParent != 0 ){
		pParent = pBlock->pParent;
		GenStateFreeBlock(pBlock);
		pBlock = pParent;
	}
	pGen->xErr = xErr;
	pGen->pErrData = pErrData;
	pGen->pCurrent = &pGen->sGlobal;
	pGen->pRawIn = pGen->pRawEnd = 0;
	pGen->pIn = pGen->pEnd = 0;
	pGen->nErr = 0;
	return SXRET_OK;
}
/*
 * Generate a compile-time error message.
 * If the error count limit is reached (usually 15 error message)
 * this function return SXERR_ABORT.In that case upper-layers must
 * abort compilation immediately.
 */
PH7_PRIVATE sxi32 PH7_GenCompileError(ph7_gen_state *pGen,sxi32 nErrType,sxu32 nLine,const char *zFormat,...)
{
	SyBlob *pWorker = &pGen->sErrBuf;
	const char *zErr = "Error";
	SyString *pFile;
	va_list ap;
	sxi32 rc;
	/* Reset the working buffer */
	SyBlobReset(pWorker);
	/* Peek the processed file path if available */
	pFile = (SyString *)SySetPeek(&pGen->pVm->aFiles);
	if( pFile && pGen->xErr ){
		/* Append file name */
		SyBlobAppend(pWorker,pFile->zString,pFile->nByte);
		SyBlobAppend(pWorker,(const void *)": ",sizeof(": ")-1);
	}
	if( nErrType == E_ERROR ){
		/* Increment the error counter */
		pGen->nErr++;
		if( pGen->nErr > 15 ){
			/* Error count limit reached */
			if( pGen->xErr ){
				SyBlobFormat(pWorker,"%u Error count limit reached,PH7 is aborting compilation\n",nLine);
				if( SyBlobLength(pWorker) > 0 ){
					/* Consume the generated error message */
					pGen->xErr(SyBlobData(pWorker),SyBlobLength(pWorker),pGen->pErrData);
				}
			}
			/* Abort immediately */
			return SXERR_ABORT;
		}
	}
	if( pGen->xErr == 0 ){
		/* No available error consumer,return immediately */
		return SXRET_OK;
	}
	switch(nErrType){
	case E_WARNING: zErr = "Warning";     break;
	case E_PARSE:   zErr = "Parse error"; break;
	case E_NOTICE:  zErr = "Notice";      break;
	case E_USER_ERROR:   zErr = "User error";   break;
	case E_USER_WARNING: zErr = "User warning"; break;
	case E_USER_NOTICE:  zErr = "User notice";  break;
	default:
		break;
	}
	rc = SXRET_OK;
	/* Format the error message */
	SyBlobFormat(pWorker,"%u %s: ",nLine,zErr);
	va_start(ap,zFormat);
	SyBlobFormatAp(pWorker,zFormat,ap);
	va_end(ap);
	/* Append a new line */
	SyBlobAppend(pWorker,(const void *)"\n",sizeof(char));
	if( SyBlobLength(pWorker) > 0 ){
		/* Consume the generated error message */
		pGen->xErr(SyBlobData(pWorker),SyBlobLength(pWorker),pGen->pErrData);
	}
	return rc;
}
/*
 * ----------------------------------------------------------
 * File: builtin.c
 * MD5: 243e3ae4de6382dfa13bd461b136240b
 * ----------------------------------------------------------
 */
/*
 * Symisc PH7: An embeddable bytecode compiler and a virtual machine for the PHP(5) programming language.
 * Copyright (C) 2011-2012, Symisc Systems http://ph7.symisc.net/
 * Version 2.1.4
 * For information on licensing,redistribution of this file,and for a DISCLAIMER OF ALL WARRANTIES
 * please contact Symisc Systems via:
 *       legal@symisc.net
 *       licensing@symisc.net
 *       contact@symisc.net
 * or visit:
 *      http://ph7.symisc.net/
 */
 /* $SymiscID: builtin.c v1.0 FreeBSD 2012-08-06 08:39 devel <chm@symisc.net> $ */
#ifndef PH7_AMALGAMATION
#include "ph7int.h"
#endif
/* This file implement built-in 'foreign' functions for the PH7 engine */
/*
 * Section:
 *    Variable handling Functions.
 * Authors:
 *    Symisc Systems,devel@symisc.net.
 *    Copyright (C) Symisc Systems,http://ph7.symisc.net
 * Status:
 *    Stable.
 */
/*
 * bool is_bool($var)
 *  Finds out whether a variable is a boolean.
 * Parameters
 *   $var: The variable being evaluated.
 * Return
 *  TRUE if var is a boolean. False otherwise.
 */
static int PH7_builtin_is_bool(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	int res = 0; /* Assume false by default */
	if( nArg > 0 ){
		res = ph7_value_is_bool(apArg[0]);
	}
	/* Query result */
	ph7_result_bool(pCtx,res);
	return PH7_OK;
}
/*
 * bool is_float($var)
 * bool is_real($var)
 * bool is_double($var)
 *  Finds out whether a variable is a float.
 * Parameters
 *   $var: The variable being evaluated.
 * Return
 *  TRUE if var is a float. False otherwise.
 */
static int PH7_builtin_is_float(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	int res = 0; /* Assume false by default */
	if( nArg > 0 ){
		res = ph7_value_is_float(apArg[0]);
	}
	/* Query result */
	ph7_result_bool(pCtx,res);
	return PH7_OK;
}
/*
 * bool is_int($var)
 * bool is_integer($var)
 * bool is_long($var)
 *  Finds out whether a variable is an integer.
 * Parameters
 *   $var: The variable being evaluated.
 * Return
 *  TRUE if var is an integer. False otherwise.
 */
static int PH7_builtin_is_int(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	int res = 0; /* Assume false by default */
	if( nArg > 0 ){
		res = ph7_value_is_int(apArg[0]);
	}
	/* Query result */
	ph7_result_bool(pCtx,res);
	return PH7_OK;
}
/*
 * bool is_string($var)
 *  Finds out whether a variable is a string.
 * Parameters
 *   $var: The variable being evaluated.
 * Return
 *  TRUE if var is string. False otherwise.
 */
static int PH7_builtin_is_string(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	int res = 0; /* Assume false by default */
	if( nArg > 0 ){
		res = ph7_value_is_string(apArg[0]);
	}
	/* Query result */
	ph7_result_bool(pCtx,res);
	return PH7_OK;
}
/*
 * bool is_null($var)
 *  Finds out whether a variable is NULL.
 * Parameters
 *   $var: The variable being evaluated.
 * Return
 *  TRUE if var is NULL. False otherwise.
 */
static int PH7_builtin_is_null(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	int res = 0; /* Assume false by default */
	if( nArg > 0 ){
		res = ph7_value_is_null(apArg[0]);
	}
	/* Query result */
	ph7_result_bool(pCtx,res);
	return PH7_OK;
}
/*
 * bool is_numeric($var)
 *  Find out whether a variable is NULL.
 * Parameters
 *  $var: The variable being evaluated.
 * Return
 *  True if var is numeric. False otherwise.
 */
static int PH7_builtin_is_numeric(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	int res = 0; /* Assume false by default */
	if( nArg > 0 ){
		res = ph7_value_is_numeric(apArg[0]);
	}
	/* Query result */
	ph7_result_bool(pCtx,res);
	return PH7_OK;
}
/*
 * bool is_scalar($var)
 *  Find out whether a variable is a scalar.
 * Parameters
 *  $var: The variable being evaluated.
 * Return
 *  True if var is scalar. False otherwise.
 */
static int PH7_builtin_is_scalar(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	int res = 0; /* Assume false by default */
	if( nArg > 0 ){
		res = ph7_value_is_scalar(apArg[0]);
	}
	/* Query result */
	ph7_result_bool(pCtx,res);
	return PH7_OK;
}
/*
 * bool is_array($var)
 *  Find out whether a variable is an array.
 * Parameters
 *  $var: The variable being evaluated.
 * Return
 *  True if var is an array. False otherwise.
 */
static int PH7_builtin_is_array(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	int res = 0; /* Assume false by default */
	if( nArg > 0 ){
		res = ph7_value_is_array(apArg[0]);
	}
	/* Query result */
	ph7_result_bool(pCtx,res);
	return PH7_OK;
}
/*
 * bool is_object($var)
 *  Find out whether a variable is an object.
 * Parameters
 *  $var: The variable being evaluated.
 * Return
 *  True if var is an object. False otherwise.
 */
static int PH7_builtin_is_object(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	int res = 0; /* Assume false by default */
	if( nArg > 0 ){
		res = ph7_value_is_object(apArg[0]);
	}
	/* Query result */
	ph7_result_bool(pCtx,res);
	return PH7_OK;
}
/*
 * bool is_resource($var)
 *  Find out whether a variable is a resource.
 * Parameters
 *  $var: The variable being evaluated.
 * Return
 *  True if a resource. False otherwise.
 */
static int PH7_builtin_is_resource(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	int res = 0; /* Assume false by default */
	if( nArg > 0 ){
		res = ph7_value_is_resource(apArg[0]);
	}
	ph7_result_bool(pCtx,res);
	return PH7_OK;
}
/*
 * float floatval($var)
 *  Get float value of a variable.
 * Parameter
 *  $var: The variable being processed.
 * Return
 *  the float value of a variable.
 */
static int PH7_builtin_floatval(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	if( nArg < 1 ){
		/* return 0.0 */
		ph7_result_double(pCtx,0);
	}else{
		double dval;
		/* Perform the cast */
		dval = ph7_value_to_double(apArg[0]);
		ph7_result_double(pCtx,dval);
	}
	return PH7_OK;
}
/*
 * int intval($var)
 *  Get integer value of a variable.
 * Parameter
 *  $var: The variable being processed.
 * Return
 *  the int value of a variable.
 */
static int PH7_builtin_intval(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	if( nArg < 1 ){
		/* return 0 */
		ph7_result_int(pCtx,0);
	}else{
		sxi64 iVal;
		/* Perform the cast */
		iVal = ph7_value_to_int64(apArg[0]);
		ph7_result_int64(pCtx,iVal);
	}
	return PH7_OK;
}
/*
 * string strval($var)
 *  Get the string representation of a variable.
 * Parameter
 *  $var: The variable being processed.
 * Return
 *  the string value of a variable.
 */
static int PH7_builtin_strval(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	if( nArg < 1 ){
		/* return NULL */
		ph7_result_null(pCtx);
	}else{
		const char *zVal;
		int iLen = 0; /* cc -O6 warning */
		/* Perform the cast */
		zVal = ph7_value_to_string(apArg[0],&iLen);
		ph7_result_string(pCtx,zVal,iLen);
	}
	return PH7_OK;
}
/*
 * bool empty($var)
 *  Determine whether a variable is empty.
 * Parameters
 *   $var: The variable being checked.
 * Return
 *  0 if var has a non-empty and non-zero value.1 otherwise.
 */
static int PH7_builtin_empty(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	int res = 1; /* Assume empty by default */
	if( nArg > 0 ){
		res = ph7_value_is_empty(apArg[0]);
	}
	ph7_result_bool(pCtx,res);
	return PH7_OK;
	
}
#ifndef PH7_DISABLE_BUILTIN_FUNC
#ifdef PH7_ENABLE_MATH_FUNC
/*
 * Section:
 *    Math Functions.
 * Authors:
 *    Symisc Systems,devel@symisc.net.
 *    Copyright (C) Symisc Systems,http://ph7.symisc.net
 * Status:
 *    Stable.
 */
#include <stdlib.h> /* abs */
#include <math.h>
/*
 * float sqrt(float $arg )
 *  Square root of the given number.
 * Parameter
 *  The number to process.
 * Return
 *  The square root of arg or the special value Nan of failure.
 */
static int PH7_builtin_sqrt(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	double r,x;
	if( nArg < 1 ){
		/* Missing argument,return 0 */
		ph7_result_int(pCtx,0);
		return PH7_OK;
	}
	x = ph7_value_to_double(apArg[0]);
	/* Perform the requested operation */
	r = sqrt(x);
	/* store the result back */
	ph7_result_double(pCtx,r);
	return PH7_OK;
}
/*
 * float exp(float $arg )
 *  Calculates the exponent of e.
 * Parameter
 *  The number to process.
 * Return
 *  'e' raised to the power of arg.
 */
static int PH7_builtin_exp(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	double r,x;
	if( nArg < 1 ){
		/* Missing argument,return 0 */
		ph7_result_int(pCtx,0);
		return PH7_OK;
	}
	x = ph7_value_to_double(apArg[0]);
	/* Perform the requested operation */
	r = exp(x);
	/* store the result back */
	ph7_result_double(pCtx,r);
	return PH7_OK;
}
/*
 * float floor(float $arg )
 *  Round fractions down.
 * Parameter
 *  The number to process.
 * Return
 *  Returns the next lowest integer value by rounding down value if necessary.
 */
static int PH7_builtin_floor(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	double r,x;
	if( nArg < 1 ){
		/* Missing argument,return 0 */
		ph7_result_int(pCtx,0);
		return PH7_OK;
	}
	x = ph7_value_to_double(apArg[0]);
	/* Perform the requested operation */
	r = floor(x);
	/* store the result back */
	ph7_result_double(pCtx,r);
	return PH7_OK;
}
/*
 * float cos(float $arg )
 *  Cosine.
 * Parameter
 *  The number to process.
 * Return
 *  The cosine of arg.
 */
static int PH7_builtin_cos(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	double r,x;
	if( nArg < 1 ){
		/* Missing argument,return 0 */
		ph7_result_int(pCtx,0);
		return PH7_OK;
	}
	x = ph7_value_to_double(apArg[0]);
	/* Perform the requested operation */
	r = cos(x);
	/* store the result back */
	ph7_result_double(pCtx,r);
	return PH7_OK;
}
/*
 * float acos(float $arg )
 *  Arc cosine.
 * Parameter
 *  The number to process.
 * Return
 *  The arc cosine of arg.
 */
static int PH7_builtin_acos(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	double r,x;
	if( nArg < 1 ){
		/* Missing argument,return 0 */
		ph7_result_int(pCtx,0);
		return PH7_OK;
	}
	x = ph7_value_to_double(apArg[0]);
	/* Perform the requested operation */
	r = acos(x);
	/* store the result back */
	ph7_result_double(pCtx,r);
	return PH7_OK;
}
/*
 * float cosh(float $arg )
 *  Hyperbolic cosine.
 * Parameter
 *  The number to process.
 * Return
 *  The hyperbolic cosine of arg.
 */
static int PH7_builtin_cosh(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	double r,x;
	if( nArg < 1 ){
		/* Missing argument,return 0 */
		ph7_result_int(pCtx,0);
		return PH7_OK;
	}
	x = ph7_value_to_double(apArg[0]);
	/* Perform the requested operation */
	r = cosh(x);
	/* store the result back */
	ph7_result_double(pCtx,r);
	return PH7_OK;
}
/*
 * float sin(float $arg )
 *  Sine.
 * Parameter
 *  The number to process.
 * Return
 *  The sine of arg.
 */
static int PH7_builtin_sin(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	double r,x;
	if( nArg < 1 ){
		/* Missing argument,return 0 */
		ph7_result_int(pCtx,0);
		return PH7_OK;
	}
	x = ph7_value_to_double(apArg[0]);
	/* Perform the requested operation */
	r = sin(x);
	/* store the result back */
	ph7_result_double(pCtx,r);
	return PH7_OK;
}
/*
 * float asin(float $arg )
 *  Arc sine.
 * Parameter
 *  The number to process.
 * Return
 *  The arc sine of arg.
 */
static int PH7_builtin_asin(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	double r,x;
	if( nArg < 1 ){
		/* Missing argument,return 0 */
		ph7_result_int(pCtx,0);
		return PH7_OK;
	}
	x = ph7_value_to_double(apArg[0]);
	/* Perform the requested operation */
	r = asin(x);
	/* store the result back */
	ph7_result_double(pCtx,r);
	return PH7_OK;
}
/*
 * float sinh(float $arg )
 *  Hyperbolic sine.
 * Parameter
 *  The number to process.
 * Return
 *  The hyperbolic sine of arg.
 */
static int PH7_builtin_sinh(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	double r,x;
	if( nArg < 1 ){
		/* Missing argument,return 0 */
		ph7_result_int(pCtx,0);
		return PH7_OK;
	}
	x = ph7_value_to_double(apArg[0]);
	/* Perform the requested operation */
	r = sinh(x);
	/* store the result back */
	ph7_result_double(pCtx,r);
	return PH7_OK;
}
/*
 * float ceil(float $arg )
 *  Round fractions up.
 * Parameter
 *  The number to process.
 * Return
 *  The next highest integer value by rounding up value if necessary.
 */
static int PH7_builtin_ceil(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	double r,x;
	if( nArg < 1 ){
		/* Missing argument,return 0 */
		ph7_result_int(pCtx,0);
		return PH7_OK;
	}
	x = ph7_value_to_double(apArg[0]);
	/* Perform the requested operation */
	r = ceil(x);
	/* store the result back */
	ph7_result_double(pCtx,r);
	return PH7_OK;
}
/*
 * float tan(float $arg )
 *  Tangent.
 * Parameter
 *  The number to process.
 * Return
 *  The tangent of arg.
 */
static int PH7_builtin_tan(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	double r,x;
	if( nArg < 1 ){
		/* Missing argument,return 0 */
		ph7_result_int(pCtx,0);
		return PH7_OK;
	}
	x = ph7_value_to_double(apArg[0]);
	/* Perform the requested operation */
	r = tan(x);
	/* store the result back */
	ph7_result_double(pCtx,r);
	return PH7_OK;
}
/*
 * float atan(float $arg )
 *  Arc tangent.
 * Parameter
 *  The number to process.
 * Return
 *  The arc tangent of arg.
 */
static int PH7_builtin_atan(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	double r,x;
	if( nArg < 1 ){
		/* Missing argument,return 0 */
		ph7_result_int(pCtx,0);
		return PH7_OK;
	}
	x = ph7_value_to_double(apArg[0]);
	/* Perform the requested operation */
	r = atan(x);
	/* store the result back */
	ph7_result_double(pCtx,r);
	return PH7_OK;
}
/*
 * float tanh(float $arg )
 *  Hyperbolic tangent.
 * Parameter
 *  The number to process.
 * Return
 *  The Hyperbolic tangent of arg.
 */
static int PH7_builtin_tanh(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	double r,x;
	if( nArg < 1 ){
		/* Missing argument,return 0 */
		ph7_result_int(pCtx,0);
		return PH7_OK;
	}
	x = ph7_value_to_double(apArg[0]);
	/* Perform the requested operation */
	r = tanh(x);
	/* store the result back */
	ph7_result_double(pCtx,r);
	return PH7_OK;
}
/*
 * float atan2(float $y,float $x)
 *  Arc tangent of two variable.
 * Parameter
 *  $y = Dividend parameter.
 *  $x = Divisor parameter.
 * Return
 *  The arc tangent of y/x in radian.
 */
static int PH7_builtin_atan2(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	double r,x,y;
	if( nArg < 2 ){
		/* Missing arguments,return 0 */
		ph7_result_int(pCtx,0);
		return PH7_OK;
	}
	y = ph7_value_to_double(apArg[0]);
	x = ph7_value_to_double(apArg[1]);
	/* Perform the requested operation */
	r = atan2(y,x);
	/* store the result back */
	ph7_result_double(pCtx,r);
	return PH7_OK;
}
/*
 * float/int64 abs(float/int64 $arg )
 *  Absolute value.
 * Parameter
 *  The number to process.
 * Return
 *  The absolute value of number.
 */
static int PH7_builtin_abs(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	int is_float;	
	if( nArg < 1 ){
		/* Missing argument,return 0 */
		ph7_result_int(pCtx,0);
		return PH7_OK;
	}
	is_float = ph7_value_is_float(apArg[0]);
	if( is_float ){
		double r,x;
		x = ph7_value_to_double(apArg[0]);
		/* Perform the requested operation */
		r = fabs(x);
		ph7_result_double(pCtx,r);
	}else{
		int r,x;
		x = ph7_value_to_int(apArg[0]);
		/* Perform the requested operation */
		r = abs(x);
		ph7_result_int(pCtx,r);
	}
	return PH7_OK;
}
/*
 * float log(float $arg,[int/float $base])
 *  Natural logarithm.
 * Parameter
 *  $arg: The number to process.
 *  $base: The optional logarithmic base to use. (only base-10 is supported)
 * Return
 *  The logarithm of arg to base, if given, or the natural logarithm.
 * Note: 
 *  only Natural log and base-10 log are supported. 
 */
static int PH7_builtin_log(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	double r,x;
	if( nArg < 1 ){
		/* Missing argument,return 0 */
		ph7_result_int(pCtx,0);
		return PH7_OK;
	}
	x = ph7_value_to_double(apArg[0]);
	/* Perform the requested operation */
	if( nArg == 2 && ph7_value_is_numeric(apArg[1]) && ph7_value_to_int(apArg[1]) == 10 ){
		/* Base-10 log */
		r = log10(x);
	}else{
		r = log(x);
	}
	/* store the result back */
	ph7_result_double(pCtx,r);
	return PH7_OK;
}
/*
 * float log10(float $arg )
 *  Base-10 logarithm.
 * Parameter
 *  The number to process.
 * Return
 *  The Base-10 logarithm of the given number.
 */
static int PH7_builtin_log10(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	double r,x;
	if( nArg < 1 ){
		/* Missing argument,return 0 */
		ph7_result_int(pCtx,0);
		return PH7_OK;
	}
	x = ph7_value_to_double(apArg[0]);
	/* Perform the requested operation */
	r = log10(x);
	/* store the result back */
	ph7_result_double(pCtx,r);
	return PH7_OK;
}
/*
 * number pow(number $base,number $exp)
 *  Exponential expression.
 * Parameter
 *  base
 *  The base to use.
 * exp
 *  The exponent.
 * Return
 *  base raised to the power of exp.
 *  If the result can be represented as integer it will be returned
 *  as type integer, else it will be returned as type float. 
 */
static int PH7_builtin_pow(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	double r,x,y;
	if( nArg < 1 ){
		/* Missing argument,return 0 */
		ph7_result_int(pCtx,0);
		return PH7_OK;
	}
	x = ph7_value_to_double(apArg[0]);
	y = ph7_value_to_double(apArg[1]);
	/* Perform the requested operation */
	r = pow(x,y);
	ph7_result_double(pCtx,r);
	return PH7_OK;
}
/*
 * float pi(void)
 *  Returns an approximation of pi. 
 * Note
 *  you can use the M_PI constant which yields identical results to pi(). 
 * Return
 *  The value of pi as float.
 */
static int PH7_builtin_pi(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	SXUNUSED(nArg); /* cc warning */
	SXUNUSED(apArg);
	ph7_result_double(pCtx,PH7_PI);
	return PH7_OK;
}
/*
 * float fmod(float $x,float $y)
 *  Returns the floating point remainder (modulo) of the division of the arguments. 
 * Parameters
 * $x
 *  The dividend
 * $y
 *  The divisor
 * Return
 *  The floating point remainder of x/y.
 */
static int PH7_builtin_fmod(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	double x,y,r; 
	if( nArg < 2 ){
		/* Missing arguments */
		ph7_result_double(pCtx,0);
		return PH7_OK;
	}
	/* Extract given arguments */
	x = ph7_value_to_double(apArg[0]);
	y = ph7_value_to_double(apArg[1]);
	/* Perform the requested operation */
	r = fmod(x,y);
	/* Processing result */
	ph7_result_double(pCtx,r);
	return PH7_OK;
}
/*
 * float hypot(float $x,float $y)
 *  Calculate the length of the hypotenuse of a right-angle triangle . 
 * Parameters
 * $x
 *  Length of first side
 * $y
 *  Length of first side
 * Return
 *  Calculated length of the hypotenuse.
 */
static int PH7_builtin_hypot(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	double x,y,r; 
	if( nArg < 2 ){
		/* Missing arguments */
		ph7_result_double(pCtx,0);
		return PH7_OK;
	}
	/* Extract given arguments */
	x = ph7_value_to_double(apArg[0]);
	y = ph7_value_to_double(apArg[1]);
	/* Perform the requested operation */
	r = hypot(x,y);
	/* Processing result */
	ph7_result_double(pCtx,r);
	return PH7_OK;
}
#endif /* PH7_ENABLE_MATH_FUNC */
/*
 * float round ( float $val [, int $precision = 0 [, int $mode = PHP_ROUND_HALF_UP ]] )
 *  Exponential expression.
 * Parameter
 *  $val
 *   The value to round.
 * $precision
 *   The optional number of decimal digits to round to.
 * $mode
 *   One of PHP_ROUND_HALF_UP, PHP_ROUND_HALF_DOWN, PHP_ROUND_HALF_EVEN, or PHP_ROUND_HALF_ODD.
 *   (not supported).
 * Return
 *  The rounded value.
 */
static int PH7_builtin_round(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	int n = 0;
	double r;
	if( nArg < 1 ){
		/* Missing argument,return 0 */
		ph7_result_int(pCtx,0);
		return PH7_OK;
	}
	/* Extract the precision if available */
	if( nArg > 1 ){
		n = ph7_value_to_int(apArg[1]);
		if( n>30 ){
			n = 30;
		}
		if( n<0 ){
			n = 0;
		}
	}
	r = ph7_value_to_double(apArg[0]);
	/* If Y==0 and X will fit in a 64-bit int,
     * handle the rounding directly.Otherwise 
	 * use our own cutsom printf [i.e:SyBufferFormat()].
     */
  if( n==0 && r>=0 && r<LARGEST_INT64-1 ){
    r = (double)((ph7_int64)(r+0.5));
  }else if( n==0 && r<0 && (-r)<LARGEST_INT64-1 ){
    r = -(double)((ph7_int64)((-r)+0.5));
  }else{
	  char zBuf[256];
	  sxu32 nLen;
	  nLen = SyBufferFormat(zBuf,sizeof(zBuf),"%.*f",n,r);
	  /* Convert the string to real number */
	  SyStrToReal(zBuf,nLen,(void *)&r,0);
  }
  /* Return thr rounded value */
  ph7_result_double(pCtx,r);
  return PH7_OK;
}
/*
 * string dechex(int $number)
 *  Decimal to hexadecimal.
 * Parameters
 *  $number
 *   Decimal value to convert
 * Return
 *  Hexadecimal string representation of number
 */
static int PH7_builtin_dechex(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	int iVal;
	if( nArg < 1 ){
		/* Missing arguments,return null */
		ph7_result_null(pCtx);
		return PH7_OK;
	}
	/* Extract the given number */
	iVal = ph7_value_to_int(apArg[0]);
	/* Format */
	ph7_result_string_format(pCtx,"%x",iVal);
	return PH7_OK;
}
/*
 * string decoct(int $number)
 *  Decimal to Octal.
 * Parameters
 *  $number
 *   Decimal value to convert
 * Return
 *  Octal string representation of number
 */
static int PH7_builtin_decoct(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	int iVal;
	if( nArg < 1 ){
		/* Missing arguments,return null */
		ph7_result_null(pCtx);
		return PH7_OK;
	}
	/* Extract the given number */
	iVal = ph7_value_to_int(apArg[0]);
	/* Format */
	ph7_result_string_format(pCtx,"%o",iVal);
	return PH7_OK;
}
/*
 * string decbin(int $number)
 *  Decimal to binary.
 * Parameters
 *  $number
 *   Decimal value to convert
 * Return
 *  Binary string representation of number
 */
static int PH7_builtin_decbin(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	int iVal;
	if( nArg < 1 ){
		/* Missing arguments,return null */
		ph7_result_null(pCtx);
		return PH7_OK;
	}
	/* Extract the given number */
	iVal = ph7_value_to_int(apArg[0]);
	/* Format */
	ph7_result_string_format(pCtx,"%B",iVal);
	return PH7_OK;
}
/*
 * int64 hexdec(string $hex_string)
 *  Hexadecimal to decimal.
 * Parameters
 *  $hex_string
 *   The hexadecimal string to convert
 * Return
 *  The decimal representation of hex_string 
 */
static int PH7_builtin_hexdec(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	const char *zString,*zEnd;
	ph7_int64 iVal;
	int nLen;
	if( nArg < 1 ){
		/* Missing arguments,return -1 */
		ph7_result_int(pCtx,-1);
		return PH7_OK;
	}
	iVal = 0;
	if( ph7_value_is_string(apArg[0]) ){
		/* Extract the given string */
		zString = ph7_value_to_string(apArg[0],&nLen);
		/* Delimit the string */
		zEnd = &zString[nLen];
		/* Ignore non hex-stream */
		while( zString < zEnd ){
			if( (unsigned char)zString[0] >= 0xc0 ){
				/* UTF-8 stream */
				zString++;
				while( zString < zEnd && (((unsigned char)zString[0] & 0xc0) == 0x80) ){
					zString++;
				}
			}else{
				if( SyisHex(zString[0]) ){
					break;
				}
				/* Ignore */
				zString++;
			}
		}
		if( zString < zEnd ){
			/* Cast */
			SyHexStrToInt64(zString,(sxu32)(zEnd-zString),(void *)&iVal,0);
		}
	}else{
		/* Extract as a 64-bit integer */
		iVal = ph7_value_to_int64(apArg[0]);
	}
	/* Return the number */
	ph7_result_int64(pCtx,iVal);
	return PH7_OK;
}
/*
 * int64 bindec(string $bin_string)
 *  Binary to decimal.
 * Parameters
 *  $bin_string
 *   The binary string to convert
 * Return
 *  Returns the decimal equivalent of the binary number represented by the binary_string argument.  
 */
static int PH7_builtin_bindec(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	const char *zString;
	ph7_int64 iVal;
	int nLen;
	if( nArg < 1 ){
		/* Missing arguments,return -1 */
		ph7_result_int(pCtx,-1);
		return PH7_OK;
	}
	iVal = 0;
	if( ph7_value_is_string(apArg[0]) ){
		/* Extract the given string */
		zString = ph7_value_to_string(apArg[0],&nLen);
		if( nLen > 0 ){
			/* Perform a binary cast */
			SyBinaryStrToInt64(zString,(sxu32)nLen,(void *)&iVal,0);
		}
	}else{
		/* Extract as a 64-bit integer */
		iVal = ph7_value_to_int64(apArg[0]);
	}
	/* Return the number */
	ph7_result_int64(pCtx,iVal);
	return PH7_OK;
}
/*
 * int64 octdec(string $oct_string)
 *  Octal to decimal.
 * Parameters
 *  $oct_string
 *   The octal string to convert
 * Return
 *  Returns the decimal equivalent of the octal number represented by the octal_string argument.  
 */
static int PH7_builtin_octdec(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	const char *zString;
	ph7_int64 iVal;
	int nLen;
	if( nArg < 1 ){
		/* Missing arguments,return -1 */
		ph7_result_int(pCtx,-1);
		return PH7_OK;
	}
	iVal = 0;
	if( ph7_value_is_string(apArg[0]) ){
		/* Extract the given string */
		zString = ph7_value_to_string(apArg[0],&nLen);
		if( nLen > 0 ){
			/* Perform the cast */
			SyOctalStrToInt64(zString,(sxu32)nLen,(void *)&iVal,0);
		}
	}else{
		/* Extract as a 64-bit integer */
		iVal = ph7_value_to_int64(apArg[0]);
	}
	/* Return the number */
	ph7_result_int64(pCtx,iVal);
	return PH7_OK;
}
/*
 * srand([int $seed])
 * mt_srand([int $seed])
 *  Seed the random number generator.
 * Parameters
 * $seed
 *  Optional seed value
 * Return
 *  null.
 * Note:
 *  THIS FUNCTION IS A NO-OP.
 *  THE PH7 PRNG IS AUTOMATICALLY SEEDED WHEN THE VM IS CREATED.
 */
static int PH7_builtin_srand(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	SXUNUSED(nArg);
	SXUNUSED(apArg);
	ph7_result_null(pCtx);
	return PH7_OK;
}
/*
 * string base_convert(string $number,int $frombase,int $tobase)
 *  Convert a number between arbitrary bases.
 * Parameters
 * $number
 *  The number to convert
 * $frombase
 *  The base number is in
 * $tobase
 *  The base to convert number to
 * Return
 *  Number converted to base tobase 
 */
static int PH7_builtin_base_convert(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	int nLen,iFbase,iTobase;
	const char *zNum;
	ph7_int64 iNum;
	if( nArg < 3 ){
		/* Return the empty string*/
		ph7_result_string(pCtx,"",0);
		return PH7_OK;
	}
	/* Base numbers */
	iFbase  = ph7_value_to_int(apArg[1]);
	iTobase = ph7_value_to_int(apArg[2]);
	if( ph7_value_is_string(apArg[0]) ){
		/* Extract the target number */
		zNum = ph7_value_to_string(apArg[0],&nLen);
		if( nLen < 1 ){
			/* Return the empty string*/
			ph7_result_string(pCtx,"",0);
			return PH7_OK;
		}
		/* Base conversion */
		switch(iFbase){
		case 16:
			/* Hex */
			SyHexStrToInt64(zNum,(sxu32)nLen,(void *)&iNum,0);
			break;
		case 8:
			/* Octal */
			SyOctalStrToInt64(zNum,(sxu32)nLen,(void *)&iNum,0);
			break;
		case 2:
			/* Binary */
			SyBinaryStrToInt64(zNum,(sxu32)nLen,(void *)&iNum,0);
			break;
		default:
			/* Decimal */
			SyStrToInt64(zNum,(sxu32)nLen,(void *)&iNum,0);
			break;
		}
	}else{
		iNum = ph7_value_to_int64(apArg[0]);
	}
	switch(iTobase){
	case 16:
		/* Hex */
		ph7_result_string_format(pCtx,"%qx",iNum); /* Quad hex */
		break;
	case 8:
		/* Octal */
		ph7_result_string_format(pCtx,"%qo",iNum); /* Quad octal */
		break;
	case 2:
		/* Binary */
		ph7_result_string_format(pCtx,"%qB",iNum); /* Quad binary */
		break;
	default:
		/* Decimal */
		ph7_result_string_format(pCtx,"%qd",iNum); /* Quad decimal */
		break;
	}
	return PH7_OK;
}
/*
 * Section:
 *    String handling Functions.
 * Authors:
 *    Symisc Systems,devel@symisc.net.
 *    Copyright (C) Symisc Systems,http://ph7.symisc.net
 * Status:
 *    Stable.
 */
/*
 * string substr(string $string,int $start[, int $length ])
 *  Return part of a string.
 * Parameters
 *  $string
 *   The input string. Must be one character or longer.
 * $start
 *   If start is non-negative, the returned string will start at the start'th position
 *   in string, counting from zero. For instance, in the string 'abcdef', the character
 *   at position 0 is 'a', the character at position 2 is 'c', and so forth.
 *   If start is negative, the returned string will start at the start'th character
 *   from the end of string.
 *   If string is less than or equal to start characters long, FALSE will be returned.
 * $length
 *   If length is given and is positive, the string returned will contain at most length
 *   characters beginning from start (depending on the length of string).
 *   If length is given and is negative, then that many characters will be omitted from
 *   the end of string (after the start position has been calculated when a start is negative).
 *   If start denotes the position of this truncation or beyond, false will be returned.
 *   If length is given and is 0, FALSE or NULL an empty string will be returned.
 *   If length is omitted, the substring starting from start until the end of the string
 *   will be returned. 
 * Return
 *  Returns the extracted part of string, or FALSE on failure or an empty string.
 */
static int PH7_builtin_substr(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	const char *zSource,*zOfft;
	int nOfft,nLen,nSrcLen;	
	if( nArg < 2 ){
		/* return FALSE */
		ph7_result_bool(pCtx,0);
		return PH7_OK;
	}
	/* Extract the target string */
	zSource = ph7_value_to_string(apArg[0],&nSrcLen);
	if( nSrcLen < 1 ){
		/* Empty string,return FALSE */
		ph7_result_bool(pCtx,0);
		return PH7_OK;
	}
	nLen = nSrcLen; /* cc warning */
	/* Extract the offset */
	nOfft = ph7_value_to_int(apArg[1]);
	if( nOfft < 0 ){
		zOfft = &zSource[nSrcLen+nOfft]; 
		if( zOfft < zSource ){
			/* Invalid offset */
			ph7_result_bool(pCtx,0);
			return PH7_OK;
		}
		nLen = (int)(&zSource[nSrcLen]-zOfft);
		nOfft = (int)(zOfft-zSource);
	}else if( nOfft >= nSrcLen ){
		/* Invalid offset */
		ph7_result_bool(pCtx,0);
		return PH7_OK;
	}else{
		zOfft = &zSource[nOfft];
		nLen = nSrcLen - nOfft;
	}
	if( nArg > 2 ){
		/* Extract the length */
		nLen = ph7_value_to_int(apArg[2]);
		if( nLen == 0 ){
			/* Invalid length,return an empty string */
			ph7_result_string(pCtx,"",0);
			return PH7_OK;
		}else if( nLen < 0 ){
			nLen = nSrcLen + nLen - nOfft;
			if( nLen < 1 ){
				/* Invalid  length */
				nLen = nSrcLen - nOfft;
			}
		}
		if( nLen + nOfft > nSrcLen ){
			/* Invalid length */
			nLen = nSrcLen - nOfft;
		}
	}
	/* Return the substring */
	ph7_result_string(pCtx,zOfft,nLen);
	return PH7_OK;
}
/*
 * int substr_compare(string $main_str,string $str ,int $offset[,int $length[,bool $case_insensitivity = false ]])
 *  Binary safe comparison of two strings from an offset, up to length characters.
 * Parameters
 *  $main_str
 *  The main string being compared.
 *  $str
 *   The secondary string being compared.
 * $offset
 *  The start position for the comparison. If negative, it starts counting from
 *  the end of the string.
 * $length
 *  The length of the comparison. The default value is the largest of the length 
 *  of the str compared to the length of main_str less the offset.
 * $case_insensitivity
 *  If case_insensitivity is TRUE, comparison is case insensitive.
 * Return
 *  Returns < 0 if main_str from position offset is less than str, > 0 if it is greater than
 *  str, and 0 if they are equal. If offset is equal to or greater than the length of main_str
 *  or length is set and is less than 1, substr_compare() prints a warning and returns FALSE. 
 */
static int PH7_builtin_substr_compare(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	const char *zSource,*zOfft,*zSub;
	int nOfft,nLen,nSrcLen,nSublen;
	int iCase = 0;
	int rc;
	if( nArg < 3 ){
		/* Missing arguments,return FALSE */
		ph7_result_bool(pCtx,0);
		return PH7_OK;
	}
	/* Extract the target string */
	zSource = ph7_value_to_string(apArg[0],&nSrcLen);
	if( nSrcLen < 1 ){
		/* Empty string,return FALSE */
		ph7_result_bool(pCtx,0);
		return PH7_OK;
	}
	nLen = nSrcLen; /* cc warning */
	/* Extract the substring */
	zSub = ph7_value_to_string(apArg[1],&nSublen);
	if( nSublen < 1 || nSublen > nSrcLen){
		/* Empty string,return FALSE */
		ph7_result_bool(pCtx,0);
		return PH7_OK;
	}
	/* Extract the offset */
	nOfft = ph7_value_to_int(apArg[2]);
	if( nOfft < 0 ){
		zOfft = &zSource[nSrcLen+nOfft]; 
		if( zOfft < zSource ){
			/* Invalid offset */
			ph7_result_bool(pCtx,0);
			return PH7_OK;
		}
		nLen = (int)(&zSource[nSrcLen]-zOfft);
		nOfft = (int)(zOfft-zSource);
	}else if( nOfft >= nSrcLen ){
		/* Invalid offset */
		ph7_result_bool(pCtx,0);
		return PH7_OK;
	}else{
		zOfft = &zSource[nOfft];
		nLen = nSrcLen - nOfft;
	}
	if( nArg > 3 ){
		/* Extract the length */
		nLen = ph7_value_to_int(apArg[3]);
		if( nLen < 1 ){
			/* Invalid  length */
			ph7_result_int(pCtx,1);
			return PH7_OK;
		}else if( nLen + nOfft > nSrcLen ){
			/* Invalid length */
			nLen = nSrcLen - nOfft;
		}
		if( nArg > 4 ){
			/* Case-sensitive or not */
			iCase = ph7_value_to_bool(apArg[4]);
		}
	}
	/* Perform the comparison */
	if( iCase ){
		rc = SyStrnicmp(zOfft,zSub,(sxu32)nLen);
	}else{
		rc = SyStrncmp(zOfft,zSub,(sxu32)nLen);
	}
	/* Comparison result */
	ph7_result_int(pCtx,rc);
	return PH7_OK;
}
/*
 * int substr_count(string $haystack,string $needle[,int $offset = 0 [,int $length ]])
 *  Count the number of substring occurrences.
 * Parameters
 * $haystack
 *   The string to search in
 * $needle
 *   The substring to search for
 * $offset
 *  The offset where to start counting
 * $length (NOT USED)
 *  The maximum length after the specified offset to search for the substring.
 *  It outputs a warning if the offset plus the length is greater than the haystack length.
 * Return
 *  Toral number of substring occurrences.
 */
static int PH7_builtin_substr_count(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	const char *zText,*zPattern,*zEnd;
	int nTextlen,nPatlen;
	int iCount = 0;
	sxu32 nOfft;
	sxi32 rc;
	if( nArg < 2 ){
		/* Missing arguments */
		ph7_result_int(pCtx,0);
		return PH7_OK;
	}
	/* Point to the haystack */
	zText = ph7_value_to_string(apArg[0],&nTextlen);
	/* Point to the neddle */
	zPattern = ph7_value_to_string(apArg[1],&nPatlen);
	if( nTextlen < 1 || nPatlen < 1 || nPatlen > nTextlen ){
		/* NOOP,return zero */
		ph7_result_int(pCtx,0);
		return PH7_OK;
	}
	if( nArg > 2 ){
		int nOfft;
		/* Extract the offset */
		nOfft = ph7_value_to_int(apArg[2]);
		if( nOfft < 0 || nOfft > nTextlen ){
			/* Invalid offset,return zero */
			ph7_result_int(pCtx,0);
			return PH7_OK;
		}
		/* Point to the desired offset */
		zText = &zText[nOfft];
		/* Adjust length */
		nTextlen -= nOfft;
	}
	/* Point to the end of the string */
	zEnd = &zText[nTextlen];
	if( nArg > 3 ){
		int nLen;
		/* Extract the length */
		nLen = ph7_value_to_int(apArg[3]);
		if( nLen < 0 || nLen > nTextlen ){
			/* Invalid length,return 0 */
			ph7_result_int(pCtx,0);
			return PH7_OK;
		}
		/* Adjust pointer */
		nTextlen = nLen;
		zEnd = &zText[nTextlen];
	}
	/* Perform the search */
	for(;;){
		rc = SyBlobSearch((const void *)zText,(sxu32)(zEnd-zText),(const void *)zPattern,nPatlen,&nOfft);
		if( rc != SXRET_OK ){
			/* Pattern not found,break immediately */
			break;
		}
		/* Increment counter and update the offset */
		iCount++;
		zText += nOfft + nPatlen;
		if( zText >= zEnd ){
			break;
		}
	}
	/* Pattern count */
	ph7_result_int(pCtx,iCount);
	return PH7_OK;
}
/*
 * string chunk_split(string $body[,int $chunklen = 76 [, string $end = "\r\n" ]])
 *   Split a string into smaller chunks.
 * Parameters
 *  $body
 *   The string to be chunked.
 * $chunklen
 *   The chunk length.
 * $end
 *   The line ending sequence.
 * Return
 *  The chunked string or NULL on failure.
 */
static int PH7_builtin_chunk_split(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	const char *zIn,*zEnd,*zSep = "\r\n";
	int nSepLen,nChunkLen,nLen;
	if( nArg < 1 || !ph7_value_is_string(apArg[0]) ){
		/* Nothing to split,return null */
		ph7_result_null(pCtx);
		return PH7_OK;
	}
	/* initialize/Extract arguments */
	nSepLen = (int)sizeof("\r\n") - 1;
	nChunkLen = 76;
	zIn = ph7_value_to_string(apArg[0],&nLen);
	zEnd = &zIn[nLen];
	if( nArg > 1 ){
		/* Chunk length */
		nChunkLen = ph7_value_to_int(apArg[1]);
		if( nChunkLen < 1 ){
			/* Switch back to the default length */
			nChunkLen = 76;
		}
		if( nArg > 2 ){
			/* Separator */
			zSep = ph7_value_to_string(apArg[2],&nSepLen);
			if( nSepLen < 1 ){
				/* Switch back to the default separator */
				zSep = "\r\n";
				nSepLen = (int)sizeof("\r\n") - 1;
			}
		}
	}
	/* Perform the requested operation */
	if( nChunkLen > nLen ){
		/* Nothing to split,return the string and the separator */
		ph7_result_string_format(pCtx,"%.*s%.*s",nLen,zIn,nSepLen,zSep);
		return PH7_OK;
	}
	while( zIn < zEnd ){
		if( nChunkLen > (int)(zEnd-zIn) ){
			nChunkLen = (int)(zEnd - zIn);
		}
		/* Append the chunk and the separator */
		ph7_result_string_format(pCtx,"%.*s%.*s",nChunkLen,zIn,nSepLen,zSep);
		/* Point beyond the chunk */
		zIn += nChunkLen;
	}
	return PH7_OK;
}
/*
 * string addslashes(string $str)
 *  Quote string with slashes.
 *  Returns a string with backslashes before characters that need
 *  to be quoted in database queries etc. These characters are single
 *  quote ('), double quote ("), backslash (\) and NUL (the NULL byte). 
 * Parameter
 *  str: The string to be escaped.
 * Return
 *  Returns the escaped string
 */
static int PH7_builtin_addslashes(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	const char *zCur,*zIn,*zEnd;
	int nLen;
	if( nArg < 1 ){
		/* Nothing to process,retun NULL */
		ph7_result_null(pCtx);
		return PH7_OK;
	}
	/* Extract the string to process */
	zIn  = ph7_value_to_string(apArg[0],&nLen);
	if( nLen < 1 ){
		/* Return the empty string */
		ph7_result_string(pCtx,"",0);
		return PH7_OK;
	}
	zEnd = &zIn[nLen];
	zCur = 0; /* cc warning */
	for(;;){
		if( zIn >= zEnd ){
			/* No more input */
			break;
		}
		zCur = zIn;
		while( zIn < zEnd && zIn[0] != '\'' && zIn[0] != '"' && zIn[0] != '\\' ){
			zIn++;
		}
		if( zIn > zCur ){
			/* Append raw contents */
			ph7_result_string(pCtx,zCur,(int)(zIn-zCur));
		}
		if( zIn < zEnd ){
			int c = zIn[0];
			ph7_result_string_format(pCtx,"\\%c",c);
		}
		zIn++;
	}
	return PH7_OK;
}
/*
 * Check if the given character is present in the given mask.
 * Return TRUE if present. FALSE otherwise.
 */
static int cSlashCheckMask(int c,const char *zMask,int nLen)
{
	const char *zEnd = &zMask[nLen];
	while( zMask < zEnd ){
		if( zMask[0] == c ){
			/* Character present,return TRUE */
			return 1;
		}
		/* Advance the pointer */
		zMask++;
	}
	/* Not present */
	return 0;
}
/*
 * string addcslashes(string $str,string $charlist)
 *  Quote string with slashes in a C style.
 * Parameter
 *  $str:
 *    The string to be escaped.
 *  $charlist:
 *    A list of characters to be escaped. If charlist contains characters \n, \r etc.
 *    they are converted in C-like style, while other non-alphanumeric characters 
 *    with ASCII codes lower than 32 and higher than 126 converted to octal representation. 
 * Return
 *  Returns the escaped string.
 * Note:
 *  Range characters [i.e: 'A..Z'] is not implemented in the current release.
 */
static int PH7_builtin_addcslashes(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	const char *zCur,*zIn,*zEnd,*zMask;
	int nLen,nMask;
	if( nArg < 1 ){
		/* Nothing to process,retun NULL */
		ph7_result_null(pCtx);
		return PH7_OK;
	}
	/* Extract the string to process */
	zIn  = ph7_value_to_string(apArg[0],&nLen);
	if( nLen < 1 || nArg < 2 ){
		/* Return the string untouched */
		ph7_result_string(pCtx,zIn,nLen);
		return PH7_OK;
	}
	/* Extract the desired mask */
	zMask = ph7_value_to_string(apArg[1],&nMask);
	zEnd = &zIn[nLen];
	zCur = 0; /* cc warning */
	for(;;){
		if( zIn >= zEnd ){
			/* No more input */
			break;
		}
		zCur = zIn;
		while( zIn < zEnd && !cSlashCheckMask(zIn[0],zMask,nMask) ){
			zIn++;
		}
		if( zIn > zCur ){
			/* Append raw contents */
			ph7_result_string(pCtx,zCur,(int)(zIn-zCur));
		}
		if( zIn < zEnd ){
			int c = zIn[0];
			if( c > 126 || (c < 32 && (!SyisAlphaNum(c)/*EBCDIC*/ && !SyisSpace(c))) ){
				/* Convert to octal */
				ph7_result_string_format(pCtx,"\\%o",c);
			}else{
				ph7_result_string_format(pCtx,"\\%c",c);
			}
		}
		zIn++;
	}
	return PH7_OK;
}
/*
 * string quotemeta(string $str)
 *  Quote meta characters.
 * Parameter
 *  $str:
 *    The string to be escaped.
 * Return
 *  Returns the escaped string.
*/
static int PH7_builtin_quotemeta(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	const char *zCur,*zIn,*zEnd;
	int nLen;
	if( nArg < 1 ){
		/* Nothing to process,retun NULL */
		ph7_result_null(pCtx);
		return PH7_OK;
	}
	/* Extract the string to process */
	zIn  = ph7_value_to_string(apArg[0],&nLen);
	if( nLen < 1 ){
		/* Return the empty string */
		ph7_result_string(pCtx,"",0);
		return PH7_OK;
	}
	zEnd = &zIn[nLen];
	zCur = 0; /* cc warning */
	for(;;){
		if( zIn >= zEnd ){
			/* No more input */
			break;
		}
		zCur = zIn;
		while( zIn < zEnd && !cSlashCheckMask(zIn[0],".\\+*?[^]($)",(int)sizeof(".\\+*?[^]($)")-1) ){
			zIn++;
		}
		if( zIn > zCur ){
			/* Append raw contents */
			ph7_result_string(pCtx,zCur,(int)(zIn-zCur));
		}
		if( zIn < zEnd ){
			int c = zIn[0];
			ph7_result_string_format(pCtx,"\\%c",c);
		}
		zIn++;
	}
	return PH7_OK;
}
/*
 * string stripslashes(string $str)
 *  Un-quotes a quoted string.
 *  Returns a string with backslashes before characters that need
 *  to be quoted in database queries etc. These characters are single
 *  quote ('), double quote ("), backslash (\) and NUL (the NULL byte). 
 * Parameter
 *  $str
 *   The input string.
 * Return
 *  Returns a string with backslashes stripped off.
 */
static int PH7_builtin_stripslashes(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	const char *zCur,*zIn,*zEnd;
	int nLen;
	if( nArg < 1 ){
		/* Nothing to process,retun NULL */
		ph7_result_null(pCtx);
		return PH7_OK;
	}
	/* Extract the string to process */
	zIn  = ph7_value_to_string(apArg[0],&nLen);
	if( zIn == 0 ){
		ph7_result_null(pCtx);
		return PH7_OK;
	}
	zEnd = &zIn[nLen];
	zCur = 0; /* cc warning */
	/* Encode the string */
	for(;;){
		if( zIn >= zEnd ){
			/* No more input */
			break;
		}
		zCur = zIn;
		while( zIn < zEnd && zIn[0] != '\\' ){
			zIn++;
		}
		if( zIn > zCur ){
			/* Append raw contents */
			ph7_result_string(pCtx,zCur,(int)(zIn-zCur));
		}
		if( &zIn[1] < zEnd ){
			int c = zIn[1];
			if( c == '\'' || c == '"' || c == '\\' ){
				/* Ignore the backslash */
				zIn++;
			}
		}else{
			break;
		}
	}
	return PH7_OK;
}
/*
 * string htmlspecialchars(string $string [, int $flags = ENT_COMPAT | ENT_HTML401 [, string $charset]])
 *  HTML escaping of special characters.
 *  The translations performed are:
 *   '&' (ampersand) ==> '&amp;'
 *   '"' (double quote) ==> '&quot;' when ENT_NOQUOTES is not set.
 *   "'" (single quote) ==> '&#039;' only when ENT_QUOTES is set.
 *   '<' (less than) ==> '&lt;'
 *   '>' (greater than) ==> '&gt;'
 * Parameters
 *  $string
 *   The string being converted.
 * $flags
 *   A bitmask of one or more of the following flags, which specify how to handle quotes.
 *   The default is ENT_COMPAT | ENT_HTML401.
 *   ENT_COMPAT 	Will convert double-quotes and leave single-quotes alone.
 *   ENT_QUOTES 	Will convert both double and single quotes.
 *   ENT_NOQUOTES 	Will leave both double and single quotes unconverted.
 *   ENT_IGNORE 	Silently discard invalid code unit sequences instead of returning an empty string.
 * $charset
 *  Defines character set used in conversion. The default character set is ISO-8859-1. (Not used)
 * Return
 *  The escaped string or NULL on failure.
 */
static int PH7_builtin_htmlspecialchars(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	const char *zCur,*zIn,*zEnd;
	int iFlags = 0x01|0x40; /* ENT_COMPAT | ENT_HTML401 */
	int nLen,c;
	if( nArg < 1 || !ph7_value_is_string(apArg[0]) ){
		/* Missing/Invalid arguments,return NULL */
		ph7_result_null(pCtx);
		return PH7_OK;
	}
	/* Extract the target string */
	zIn = ph7_value_to_string(apArg[0],&nLen);
	zEnd = &zIn[nLen];
	/* Extract the flags if available */
	if( nArg > 1 ){
		iFlags = ph7_value_to_int(apArg[1]);
		if( iFlags < 0 ){
			iFlags = 0x01|0x40;
		}
	}
	/* Perform the requested operation */
	for(;;){
		if( zIn >= zEnd ){
			break;
		}
		zCur = zIn;
		while( zIn < zEnd && zIn[0] != '&' && zIn[0] != '\'' && zIn[0] != '"' && zIn[0] != '<' && zIn[0] != '>' ){
			zIn++;
		}
		if( zCur < zIn ){
			/* Append the raw string verbatim */
			ph7_result_string(pCtx,zCur,(int)(zIn-zCur));
		}
		if( zIn >= zEnd ){
			break;
		}
		c = zIn[0];
		if( c == '&' ){
			/* Expand '&amp;' */
			ph7_result_string(pCtx,"&amp;",(int)sizeof("&amp;")-1);
		}else if( c == '<' ){
			/* Expand '&lt;' */
			ph7_result_string(pCtx,"&lt;",(int)sizeof("&lt;")-1);
		}else if( c == '>' ){
			/* Expand '&gt;' */
			ph7_result_string(pCtx,"&gt;",(int)sizeof("&gt;")-1);
		}else if( c == '\'' ){
			if( iFlags & 0x02 /*ENT_QUOTES*/ ){
				/* Expand '&#039;' */
				ph7_result_string(pCtx,"&#039;",(int)sizeof("&#039;")-1);
			}else{
				/* Leave the single quote untouched */
				ph7_result_string(pCtx,"'",(int)sizeof(char));
			}
		}else if( c == '"' ){
			if( (iFlags & 0x04) == 0 /*ENT_NOQUOTES*/ ){
				/* Expand '&quot;' */
				ph7_result_string(pCtx,"&quot;",(int)sizeof("&quot;")-1);
			}else{
				/* Leave the double quote untouched */
				ph7_result_string(pCtx,"\"",(int)sizeof(char));
			}
		}
		/* Ignore the unsafe HTML character */
		zIn++;
	}
	return PH7_OK;
}
/*
 * string htmlspecialchars_decode(string $string[,int $quote_style = ENT_COMPAT ])
 *  Unescape HTML entities.
 * Parameters
 *  $string
 *   The string to decode
 *  $quote_style
 *    The quote style. One of the following constants:
 *   ENT_COMPAT 	Will convert double-quotes and leave single-quotes alone (default)
 *   ENT_QUOTES 	Will convert both double and single quotes
 *   ENT_NOQUOTES 	Will leave both double and single quotes unconverted
 * Return
 *  The unescaped string or NULL on failure.
 */
static int PH7_builtin_htmlspecialchars_decode(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	const char *zCur,*zIn,*zEnd;
	int iFlags = 0x01; /* ENT_COMPAT */
	int nLen,nJump;
	if( nArg < 1 || !ph7_value_is_string(apArg[0]) ){
		/* Missing/Invalid arguments,return NULL */
		ph7_result_null(pCtx);
		return PH7_OK;
	}
	/* Extract the target string */
	zIn = ph7_value_to_string(apArg[0],&nLen);
	zEnd = &zIn[nLen];
	/* Extract the flags if available */
	if( nArg > 1 ){
		iFlags = ph7_value_to_int(apArg[1]);
		if( iFlags < 0 ){
			iFlags = 0x01;
		}
	}
	/* Perform the requested operation */
	for(;;){
		if( zIn >= zEnd ){
			break;
		}
		zCur = zIn;
		while( zIn < zEnd && zIn[0] != '&' ){
			zIn++;
		}
		if( zCur < zIn ){
			/* Append the raw string verbatim */
			ph7_result_string(pCtx,zCur,(int)(zIn-zCur));
		}
		nLen = (int)(zEnd-zIn);
		nJump = (int)sizeof(char);
		if( nLen >= (int)sizeof("&amp;")-1 && SyStrnicmp(zIn,"&amp;",sizeof("&amp;")-1) == 0 ){
			/* &amp; ==> '&' */
			ph7_result_string(pCtx,"&",(int)sizeof(char));
			nJump = (int)sizeof("&amp;")-1;
		}else if( nLen >= (int)sizeof("&lt;")-1 && SyStrnicmp(zIn,"&lt;",sizeof("&lt;")-1) == 0 ){
			/* &lt; ==> < */
			ph7_result_string(pCtx,"<",(int)sizeof(char));
			nJump = (int)sizeof("&lt;")-1; 
		}else if( nLen >= (int)sizeof("&gt;")-1 && SyStrnicmp(zIn,"&gt;",sizeof("&gt;")-1) == 0 ){
			/* &gt; ==> '>' */
			ph7_result_string(pCtx,">",(int)sizeof(char));
			nJump = (int)sizeof("&gt;")-1; 
		}else if( nLen >= (int)sizeof("&quot;")-1 && SyStrnicmp(zIn,"&quot;",sizeof("&quot;")-1) == 0 ){
			/* &quot; ==> '"' */
			if( (iFlags & 0x04) == 0 /*ENT_NOQUOTES*/ ){
				ph7_result_string(pCtx,"\"",(int)sizeof(char));
			}else{
				/* Leave untouched */
				ph7_result_string(pCtx,"&quot;",(int)sizeof("&quot;")-1);
			}
			nJump = (int)sizeof("&quot;")-1;
		}else if( nLen >= (int)sizeof("&#039;")-1 && SyStrnicmp(zIn,"&#039;",sizeof("&#039;")-1) == 0 ){
			/* &#039; ==> ''' */
			if( iFlags & 0x02 /*ENT_QUOTES*/ ){
				/* Expand ''' */
				ph7_result_string(pCtx,"'",(int)sizeof(char));
			}else{
				/* Leave untouched */
				ph7_result_string(pCtx,"&#039;",(int)sizeof("&#039;")-1);
			}
			nJump = (int)sizeof("&#039;")-1;
		}else if( nLen >= (int)sizeof(char) ){
			/* expand '&' */
			ph7_result_string(pCtx,"&",(int)sizeof(char));
		}else{
			/* No more input to process */
			break;
		}
		zIn += nJump;
	}
	return PH7_OK;
}
/* HTML encoding/Decoding table 
 * Source: Symisc RunTime API.[chm@symisc.net]
 */
static const char *azHtmlEscape[] = {
 	"&lt;","<","&gt;",">","&amp;","&","&quot;","\"","&#39;","'",
	"&#33;","!","&#36;","$","&#35;","#","&#37;","%","&#40;","(",
	"&#41;",")","&#123;","{","&#125;","}","&#61;","=","&#43;","+",
	"&#63;","?","&#91;","[","&#93;","]","&#64;","@","&#44;","," 
 };
/*
 * array get_html_translation_table(void)
 *  Returns the translation table used by htmlspecialchars() and htmlentities().
 * Parameters
 *  None
 * Return
 *  The translation table as an array or NULL on failure.
 */
static int PH7_builtin_get_html_translation_table(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	ph7_value *pArray,*pValue;
	sxu32 n;
	/* Element value */
	pValue = ph7_context_new_scalar(pCtx);
	if( pValue == 0 ){
		SXUNUSED(nArg); /* cc warning */
		SXUNUSED(apArg);
		/* Return NULL */
		ph7_result_null(pCtx);
		return PH7_OK;
	}
	/* Create a new array */
	pArray = ph7_context_new_array(pCtx);
	if( pArray == 0 ){
		/* Return NULL */
		ph7_result_null(pCtx);
		return PH7_OK;
	}
	/* Make the table */
	for( n = 0 ; n < SX_ARRAYSIZE(azHtmlEscape) ; n += 2 ){
		/* Prepare the value */
		ph7_value_string(pValue,azHtmlEscape[n],-1 /* Compute length automatically */);
		/* Insert the value */
		ph7_array_add_strkey_elem(pArray,azHtmlEscape[n+1],pValue);
		/* Reset the string cursor */
		ph7_value_reset_string_cursor(pValue);
	}
	/* 
	 * Return the array.
	 * Don't worry about freeing memory, everything will be automatically
	 * released upon we return from this function.
	 */
	ph7_result_value(pCtx,pArray);
	return PH7_OK;
}
/*
 * string htmlentities( string $string [, int $flags = ENT_COMPAT | ENT_HTML401]);
 *   Convert all applicable characters to HTML entities
 * Parameters
 * $string
 *   The input string.
 * $flags
 *  A bitmask of one or more of the flags (see block-comment on PH7_builtin_htmlspecialchars())
 * Return
 * The encoded string.
 */
static int PH7_builtin_htmlentities(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	int iFlags = 0x01; /* ENT_COMPAT */
	const char *zIn,*zEnd;
	int nLen,c;
	sxu32 n;
	if( nArg < 1 || !ph7_value_is_string(apArg[0]) ){
		/* Missing/Invalid arguments,return NULL */
		ph7_result_null(pCtx);
		return PH7_OK;
	}
	/* Extract the target string */
	zIn = ph7_value_to_string(apArg[0],&nLen);
	zEnd = &zIn[nLen];
	/* Extract the flags if available */
	if( nArg > 1 ){
		iFlags = ph7_value_to_int(apArg[1]);
		if( iFlags < 0 ){
			iFlags = 0x01;
		}
	}
	/* Perform the requested operation */
	for(;;){
		if( zIn >= zEnd ){
			/* No more input to process */
			break;
		}
		c = zIn[0];
		/* Perform a linear lookup on the decoding table */
		for( n = 0 ; n < SX_ARRAYSIZE(azHtmlEscape) ; n += 2 ){
			if( azHtmlEscape[n+1][0] == c ){
				/* Got one */
				break;
			}
		}
		if( n < SX_ARRAYSIZE(azHtmlEscape) ){
			/* Output the safe sequence [i.e: '<' ==> '&lt;"] */
			if( c == '"' && (iFlags & 0x04) /*ENT_NOQUOTES*/ ){
				/* Expand the double quote verbatim */
				ph7_result_string(pCtx,(const char *)&c,(int)sizeof(char));
			}else if(c == '\'' && ((iFlags & 0x02 /*ENT_QUOTES*/) == 0 || (iFlags & 0x04) /*ENT_NOQUOTES*/) ){
				/* expand single quote verbatim */
				ph7_result_string(pCtx,(const char *)&c,(int)sizeof(char));
			}else{
				ph7_result_string(pCtx,azHtmlEscape[n],-1/*Compute length automatically */);
			}
		}else{
			/* Output character verbatim */
			ph7_result_string(pCtx,(const char *)&c,(int)sizeof(char));
		}
		zIn++;
	}
	return PH7_OK;
}
/*
 * string html_entity_decode(string $string [, int $quote_style = ENT_COMPAT [, string $charset = 'UTF-8' ]])
 *   Perform the reverse operation of html_entity_decode().
 * Parameters
 * $string
 *   The input string.
 * $flags
 *  A bitmask of one or more of the flags (see comment on PH7_builtin_htmlspecialchars())
 * Return
 * The decoded string.
 */
static int PH7_builtin_html_entity_decode(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	const char *zCur,*zIn,*zEnd;
	int iFlags = 0x01; /* ENT_COMPAT  */
	int nLen;
	sxu32 n;
	if( nArg < 1 || !ph7_value_is_string(apArg[0]) ){
		/* Missing/Invalid arguments,return NULL */
		ph7_result_null(pCtx);
		return PH7_OK;
	}
	/* Extract the target string */
	zIn = ph7_value_to_string(apArg[0],&nLen);
	zEnd = &zIn[nLen];
	/* Extract the flags if available */
	if( nArg > 1 ){
		iFlags = ph7_value_to_int(apArg[1]);
		if( iFlags < 0 ){
			iFlags = 0x01;
		}
	}
	/* Perform the requested operation */
	for(;;){
		if( zIn >= zEnd ){
			/* No more input to process */
			break;
		}
		zCur = zIn;
		while( zIn < zEnd && zIn[0] != '&' ){
			zIn++;
		}
		if( zCur < zIn ){
			/* Append raw string verbatim */
			ph7_result_string(pCtx,zCur,(int)(zIn-zCur));
		}
		if( zIn >= zEnd ){
			break;
		}
		nLen = (int)(zEnd-zIn);
		/* Find an encoded sequence */
		for(n = 0 ; n < SX_ARRAYSIZE(azHtmlEscape) ; n += 2 ){
			int iLen = (int)SyStrlen(azHtmlEscape[n]);
			if( nLen >= iLen && SyStrnicmp(zIn,azHtmlEscape[n],(sxu32)iLen) == 0 ){
				/* Got one */
				zIn += iLen;
				break;
			}
		}
		if( n < SX_ARRAYSIZE(azHtmlEscape) ){
			int c = azHtmlEscape[n+1][0];
			/* Output the decoded character */
			if( c == '\'' && ((iFlags & 0x02) == 0 /*ENT_QUOTES*/|| (iFlags & 0x04) /*ENT_NOQUOTES*/)  ){
				/* Do not process single quotes */
				ph7_result_string(pCtx,azHtmlEscape[n],-1);
			}else if( c == '"' && (iFlags & 0x04) /*ENT_NOQUOTES*/ ){
				/* Do not process double quotes */
				ph7_result_string(pCtx,azHtmlEscape[n],-1);
			}else{
				ph7_result_string(pCtx,azHtmlEscape[n+1],-1); /* Compute length automatically */
			}
		}else{
			/* Append '&' */
			ph7_result_string(pCtx,"&",(int)sizeof(char));
			zIn++;
		}
	}
	return PH7_OK;
}
/*
 * int strlen($string)
 *  return the length of the given string.
 * Parameter
 *  string: The string being measured for length.
 * Return
 *  length of the given string.
 */
static int PH7_builtin_strlen(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	int iLen = 0;
	if( nArg > 0 ){
		ph7_value_to_string(apArg[0],&iLen);
	}
	/* String length */
	ph7_result_int(pCtx,iLen);
	return PH7_OK;
}
/*
 * int strcmp(string $str1,string $str2)
 *  Perform a binary safe string comparison.
 * Parameter
 *  str1: The first string
 *  str2: The second string
 * Return
 *  Returns < 0 if str1 is less than str2; > 0 if str1 is greater 
 *  than str2, and 0 if they are equal.
 */
static int PH7_builtin_strcmp(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	const char *z1,*z2;
	int n1,n2;
	int res;
	if( nArg < 2 ){
		res = nArg == 0 ? 0 : 1;
		ph7_result_int(pCtx,res);
		return PH7_OK;
	}
	/* Perform the comparison */
	z1 = ph7_value_to_string(apArg[0],&n1);
	z2 = ph7_value_to_string(apArg[1],&n2);
	res = SyStrncmp(z1,z2,(sxu32)(SXMAX(n1,n2)));
	/* Comparison result */
	ph7_result_int(pCtx,res);
	return PH7_OK;
}
/*
 * int strncmp(string $str1,string $str2,int n)
 *  Perform a binary safe string comparison of the first n characters.
 * Parameter
 *  str1: The first string
 *  str2: The second string
 * Return
 *  Returns < 0 if str1 is less than str2; > 0 if str1 is greater 
 *  than str2, and 0 if they are equal.
 */
static int PH7_builtin_strncmp(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	const char *z1,*z2;
	int res;
	int n;
	if( nArg < 3 ){
		/* Perform a standard comparison */
		return PH7_builtin_strcmp(pCtx,nArg,apArg);
	}
	/* Desired comparison length */
	n  = ph7_value_to_int(apArg[2]);
	if( n < 0 ){
		/* Invalid length */
		ph7_result_int(pCtx,-1);
		return PH7_OK;
	}
	/* Perform the comparison */
	z1 = ph7_value_to_string(apArg[0],0);
	z2 = ph7_value_to_string(apArg[1],0);
	res = SyStrncmp(z1,z2,(sxu32)n);
	/* Comparison result */
	ph7_result_int(pCtx,res);
	return PH7_OK;
}
/*
 * int strcasecmp(string $str1,string $str2,int n)
 *  Perform a binary safe case-insensitive string comparison.
 * Parameter
 *  str1: The first string
 *  str2: The second string
 * Return
 *  Returns < 0 if str1 is less than str2; > 0 if str1 is greater 
 *  than str2, and 0 if they are equal.
 */
static int PH7_builtin_strcasecmp(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	const char *z1,*z2;
	int n1,n2;
	int res;
	if( nArg < 2 ){
		res = nArg == 0 ? 0 : 1;
		ph7_result_int(pCtx,res);
		return PH7_OK;
	}
	/* Perform the comparison */
	z1 = ph7_value_to_string(apArg[0],&n1);
	z2 = ph7_value_to_string(apArg[1],&n2);
	res = SyStrnicmp(z1,z2,(sxu32)(SXMAX(n1,n2)));
	/* Comparison result */
	ph7_result_int(pCtx,res);
	return PH7_OK;
}
/*
 * int strncasecmp(string $str1,string $str2,int n)
 *  Perform a binary safe case-insensitive string comparison of the first n characters.
 * Parameter
 *  $str1: The first string
 *  $str2: The second string
 *  $len:  The length of strings to be used in the comparison.
 * Return
 *  Returns < 0 if str1 is less than str2; > 0 if str1 is greater 
 *  than str2, and 0 if they are equal.
 */
static int PH7_builtin_strncasecmp(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	const char *z1,*z2;
	int res;
	int n;
	if( nArg < 3 ){
		/* Perform a standard comparison */
		return PH7_builtin_strcasecmp(pCtx,nArg,apArg);
	}
	/* Desired comparison length */
	n  = ph7_value_to_int(apArg[2]);
	if( n < 0 ){
		/* Invalid length */
		ph7_result_int(pCtx,-1);
		return PH7_OK;
	}
	/* Perform the comparison */
	z1 = ph7_value_to_string(apArg[0],0);
	z2 = ph7_value_to_string(apArg[1],0);
	res = SyStrnicmp(z1,z2,(sxu32)n);
	/* Comparison result */
	ph7_result_int(pCtx,res);
	return PH7_OK;
}
/*
 * Implode context [i.e: it's private data].
 * A pointer to the following structure is forwarded
 * verbatim to the array walker callback defined below.
 */
struct implode_data {
	ph7_context *pCtx;    /* Call context */
	int bRecursive;       /* TRUE if recursive implode [this is a symisc eXtension] */
	const char *zSep;     /* Arguments separator if any */
	int nSeplen;          /* Separator length */
	int bFirst;           /* TRUE if first call */
	int nRecCount;        /* Recursion count to avoid infinite loop */
};
/*
 * Implode walker callback for the [ph7_array_walk()] interface.
 * The following routine is invoked for each array entry passed
 * to the implode() function.
 */
static int implode_callback(ph7_value *pKey,ph7_value *pValue,void *pUserData)
{
	struct implode_data *pData = (struct implode_data *)pUserData;
	const char *zData;
	int nLen;
	if( pData->bRecursive && ph7_value_is_array(pValue) && pData->nRecCount < 32 ){
		if( pData->nSeplen > 0 ){
			if( !pData->bFirst ){
				/* append the separator first */
				ph7_result_string(pData->pCtx,pData->zSep,pData->nSeplen);
			}else{
				pData->bFirst = 0;
			}
		}
		/* Recurse */
		pData->bFirst = 1;
		pData->nRecCount++;
		PH7_HashmapWalk((ph7_hashmap *)pValue->x.pOther,implode_callback,pData);
		pData->nRecCount--;
		return PH7_OK;
	}
	/* Extract the string representation of the entry value */
	zData = ph7_value_to_string(pValue,&nLen);
	if( nLen > 0 ){
		if( pData->nSeplen > 0 ){
			if( !pData->bFirst ){
				/* append the separator first */
				ph7_result_string(pData->pCtx,pData->zSep,pData->nSeplen);
			}else{
				pData->bFirst = 0;
			}
		}
		ph7_result_string(pData->pCtx,zData,nLen);
	}else{
		SXUNUSED(pKey); /* cc warning */
	}
	return PH7_OK;
}
/*
 * string implode(string $glue,array $pieces,...)
 * string implode(array $pieces,...)
 *  Join array elements with a string.
 * $glue
 *   Defaults to an empty string. This is not the preferred usage of implode() as glue
 *   would be the second parameter and thus, the bad prototype would be used.
 * $pieces
 *   The array of strings to implode.
 * Return
 *  Returns a string containing a string representation of all the array elements in the same
 *  order, with the glue string between each element. 
 */
static int PH7_builtin_implode(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	struct implode_data imp_data;
	int i = 1;
	if( nArg < 1 ){
		/* Missing argument,return NULL */
		ph7_result_null(pCtx);
		return PH7_OK;
	}
	/* Prepare the implode context */
	imp_data.pCtx = pCtx;
	imp_data.bRecursive = 0;
	imp_data.bFirst = 1;
	imp_data.nRecCount = 0;
	if( !ph7_value_is_array(apArg[0]) ){
		imp_data.zSep = ph7_value_to_string(apArg[0],&imp_data.nSeplen);
	}else{
		imp_data.zSep = 0;
		imp_data.nSeplen = 0;
		i = 0;
	}
	ph7_result_string(pCtx,"",0); /* Set an empty stirng */
	/* Start the 'join' process */
	while( i < nArg ){
		if( ph7_value_is_array(apArg[i]) ){
			/* Iterate throw array entries */
			ph7_array_walk(apArg[i],implode_callback,&imp_data);
		}else{
			const char *zData;
			int nLen;
			/* Extract the string representation of the ph7 value */
			zData = ph7_value_to_string(apArg[i],&nLen);
			if( nLen > 0 ){
				if( imp_data.nSeplen > 0 ){
					if( !imp_data.bFirst ){
						/* append the separator first */
						ph7_result_string(pCtx,imp_data.zSep,imp_data.nSeplen);
					}else{
						imp_data.bFirst = 0;
					}
				}
				ph7_result_string(pCtx,zData,nLen);
			}
		}
		i++;
	}
	return PH7_OK;
}
/*
 * Symisc eXtension:
 * string implode_recursive(string $glue,array $pieces,...)
 * Purpose
 *  Same as implode() but recurse on arrays.
 * Example:
 *   $a = array('usr',array('home','dean'));
 *   echo implode_recursive("/",$a);
 *   Will output
 *     usr/home/dean.
 *   While the standard implode would produce.
 *    usr/Array.
 * Parameter
 *  Refer to implode().
 * Return
 *  Refer to implode().
 */
static int PH7_builtin_implode_recursive(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	struct implode_data imp_data;
	int i = 1;
	if( nArg < 1 ){
		/* Missing argument,return NULL */
		ph7_result_null(pCtx);
		return PH7_OK;
	}
	/* Prepare the implode context */
	imp_data.pCtx = pCtx;
	imp_data.bRecursive = 1;
	imp_data.bFirst = 1;
	imp_data.nRecCount = 0;
	if( !ph7_value_is_array(apArg[0]) ){
		imp_data.zSep = ph7_value_to_string(apArg[0],&imp_data.nSeplen);
	}else{
		imp_data.zSep = 0;
		imp_data.nSeplen = 0;
		i = 0;
	}
	ph7_result_string(pCtx,"",0); /* Set an empty stirng */
	/* Start the 'join' process */
	while( i < nArg ){
		if( ph7_value_is_array(apArg[i]) ){
			/* Iterate throw array entries */
			ph7_array_walk(apArg[i],implode_callback,&imp_data);
		}else{
			const char *zData;
			int nLen;
			/* Extract the string representation of the ph7 value */
			zData = ph7_value_to_string(apArg[i],&nLen);
			if( nLen > 0 ){
				if( imp_data.nSeplen > 0 ){
					if( !imp_data.bFirst ){
						/* append the separator first */
						ph7_result_string(pCtx,imp_data.zSep,imp_data.nSeplen);
					}else{
						imp_data.bFirst = 0;
					}
				}
				ph7_result_string(pCtx,zData,nLen);
			}
		}
		i++;
	}
	return PH7_OK;
}
/*
 * array explode(string $delimiter,string $string[,int $limit ])
 *  Returns an array of strings, each of which is a substring of string 
 *  formed by splitting it on boundaries formed by the string delimiter. 
 * Parameters
 *  $delimiter
 *   The boundary string.
 * $string
 *   The input string.
 * $limit
 *   If limit is set and positive, the returned array will contain a maximum
 *   of limit elements with the last element containing the rest of string.
 *   If the limit parameter is negative, all fields except the last -limit are returned.
 *   If the limit parameter is zero, then this is treated as 1.
 * Returns
 *  Returns an array of strings created by splitting the string parameter
 *  on boundaries formed by the delimiter.
 *  If delimiter is an empty string (""), explode() will return FALSE. 
 *  If delimiter contains a value that is not contained in string and a negative
 *  limit is used, then an empty array will be returned, otherwise an array containing string
 *  will be returned. 
 * NOTE:
 *  Negative limit is not supported.
 */
static int PH7_builtin_explode(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	const char *zDelim,*zString,*zCur,*zEnd;
	int nDelim,nStrlen,iLimit;
	ph7_value *pArray;
	ph7_value *pValue;
	sxu32 nOfft;
	sxi32 rc;
	if( nArg < 2 ){
		/* Missing arguments,return FALSE */
		ph7_result_bool(pCtx,0);
		return PH7_OK;
	}
	/* Extract the delimiter */
	zDelim = ph7_value_to_string(apArg[0],&nDelim);
	if( nDelim < 1 ){
		/* Empty delimiter,return FALSE */
		ph7_result_bool(pCtx,0);
		return PH7_OK;
	}
	/* Extract the string */
	zString = ph7_value_to_string(apArg[1],&nStrlen);
	if( nStrlen < 1 ){
		/* Empty delimiter,return FALSE */
		ph7_result_bool(pCtx,0);
		return PH7_OK;
	}
	/* Point to the end of the string */
	zEnd = &zString[nStrlen];
	/* Create the array */
	pArray =  ph7_context_new_array(pCtx);
	pValue = ph7_context_new_scalar(pCtx);
	if( pArray == 0 || pValue == 0 ){
		/* Out of memory,return FALSE */
		ph7_result_bool(pCtx,0);
		return PH7_OK;
	}
	/* Set a defualt limit */
	iLimit = SXI32_HIGH;
	if( nArg > 2 ){
		iLimit = ph7_value_to_int(apArg[2]);
		 if( iLimit < 0 ){
			iLimit = -iLimit;
		}
		if( iLimit == 0 ){
			iLimit = 1;
		}
		iLimit--;
	}
	/* Start exploding */
	for(;;){
		if( zString >= zEnd ){
			/* No more entry to process */
			break;
		}
		rc = SyBlobSearch(zString,(sxu32)(zEnd-zString),zDelim,nDelim,&nOfft);
		if( rc != SXRET_OK || iLimit <= (int)ph7_array_count(pArray) ){
			/* Limit reached,insert the rest of the string and break */
			if( zEnd > zString ){
				ph7_value_string(pValue,zString,(int)(zEnd-zString));
				ph7_array_add_elem(pArray,0/* Automatic index assign*/,pValue);
			}
			break;
		}
		/* Point to the desired offset */
		zCur = &zString[nOfft];
		if( zCur > zString ){
			/* Perform the store operation */
			ph7_value_string(pValue,zString,(int)(zCur-zString));
			ph7_array_add_elem(pArray,0/* Automatic index assign*/,pValue);
		}
		/* Point beyond the delimiter */
		zString = &zCur[nDelim];
		/* Reset the cursor */
		ph7_value_reset_string_cursor(pValue);
	}
	/* Return the freshly created array */
	ph7_result_value(pCtx,pArray);
	/* NOTE that every allocated ph7_value will be automatically 
	 * released as soon we return from this foregin function.
	 */
	return PH7_OK;
}
/*
 * string trim(string $str[,string $charlist ])
 *  Strip whitespace (or other characters) from the beginning and end of a string.
 * Parameters
 *  $str
 *   The string that will be trimmed.
 * $charlist
 *   Optionally, the stripped characters can also be specified using the charlist parameter.
 *   Simply list all characters that you want to be stripped.
 *   With .. you can specify a range of characters.
 * Returns.
 *  Thr processed string.
 * NOTE:
 *   RANGE CHARACTERS [I.E: 'a'..'z'] are not supported.
 */
static int PH7_builtin_trim(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	const char *zString;
	int nLen;
	if( nArg < 1 ){
		/* Missing arguments,return null */
		ph7_result_null(pCtx);
		return PH7_OK;
	}
	/* Extract the target string */
	zString = ph7_value_to_string(apArg[0],&nLen);
	if( nLen < 1 ){
		/* Empty string,return */
		ph7_result_string(pCtx,"",0);
		return PH7_OK;
	}
	/* Start the trim process */
	if( nArg < 2 ){
		SyString sStr;
		/* Remove white spaces and NUL bytes */
		SyStringInitFromBuf(&sStr,zString,nLen);
		SyStringFullTrimSafe(&sStr);
		ph7_result_string(pCtx,sStr.zString,(int)sStr.nByte);
	}else{
		/* Char list */
		const char *zList;
		int nListlen;
		zList = ph7_value_to_string(apArg[1],&nListlen);
		if( nListlen < 1 ){
			/* Return the string unchanged */
			ph7_result_string(pCtx,zString,nLen);
		}else{
			const char *zEnd = &zString[nLen];
			const char *zCur = zString;
			const char *zPtr;
			int i;
			/* Left trim */
			for(;;){
				if( zCur >= zEnd ){
					break;
				}
				zPtr = zCur;
				for( i = 0 ; i < nListlen ; i++ ){
					if( zCur < zEnd && zCur[0] == zList[i] ){
						zCur++;
					}
				}
				if( zCur == zPtr ){
					/* No match,break immediately */
					break;
				}
			}
			/* Right trim */
			zEnd--;
			for(;;){
				if( zEnd <= zCur ){
					break;
				}
				zPtr = zEnd;
				for( i = 0 ; i < nListlen ; i++ ){
					if( zEnd > zCur && zEnd[0] == zList[i] ){
						zEnd--;
					}
				}
				if( zEnd == zPtr ){
					break;
				}
			}
			if( zCur >= zEnd ){
				/* Return the empty string */
				ph7_result_string(pCtx,"",0);
			}else{
				zEnd++;
				ph7_result_string(pCtx,zCur,(int)(zEnd-zCur));
			}
		}
	}
	return PH7_OK;
}
/*
 * string rtrim(string $str[,string $charlist ])
 *  Strip whitespace (or other characters) from the end of a string.
 * Parameters
 *  $str
 *   The string that will be trimmed.
 * $charlist
 *   Optionally, the stripped characters can also be specified using the charlist parameter.
 *   Simply list all characters that you want to be stripped.
 *   With .. you can specify a range of characters.
 * Returns.
 *  Thr processed string.
 * NOTE:
 *   RANGE CHARACTERS [I.E: 'a'..'z'] are not supported.
 */
static int PH7_builtin_rtrim(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	const char *zString;
	int nLen;
	if( nArg < 1 ){
		/* Missing arguments,return null */
		ph7_result_null(pCtx);
		return PH7_OK;
	}
	/* Extract the target string */
	zString = ph7_value_to_string(apArg[0],&nLen);
	if( nLen < 1 ){
		/* Empty string,return */
		ph7_result_string(pCtx,"",0);
		return PH7_OK;
	}
	/* Start the trim process */
	if( nArg < 2 ){
		SyString sStr;
		/* Remove white spaces and NUL bytes*/
		SyStringInitFromBuf(&sStr,zString,nLen);
		SyStringRightTrimSafe(&sStr);
		ph7_result_string(pCtx,sStr.zString,(int)sStr.nByte);
	}else{
		/* Char list */
		const char *zList;
		int nListlen;
		zList = ph7_value_to_string(apArg[1],&nListlen);
		if( nListlen < 1 ){
			/* Return the string unchanged */
			ph7_result_string(pCtx,zString,nLen);
		}else{
			const char *zEnd = &zString[nLen - 1];
			const char *zCur = zString;
			const char *zPtr;
			int i;
			/* Right trim */
			for(;;){
				if( zEnd <= zCur ){
					break;
				}
				zPtr = zEnd;
				for( i = 0 ; i < nListlen ; i++ ){
					if( zEnd > zCur && zEnd[0] == zList[i] ){
						zEnd--;
					}
				}
				if( zEnd == zPtr ){
					break;
				}
			}
			if( zEnd <= zCur ){
				/* Return the empty string */
				ph7_result_string(pCtx,"",0);
			}else{
				zEnd++;
				ph7_result_string(pCtx,zCur,(int)(zEnd-zCur));
			}
		}
	}
	return PH7_OK;
}
/*
 * string ltrim(string $str[,string $charlist ])
 *  Strip whitespace (or other characters) from the beginning and end of a string.
 * Parameters
 *  $str
 *   The string that will be trimmed.
 * $charlist
 *   Optionally, the stripped characters can also be specified using the charlist parameter.
 *   Simply list all characters that you want to be stripped.
 *   With .. you can specify a range of characters.
 * Returns.
 *  Thr processed string.
 * NOTE:
 *   RANGE CHARACTERS [I.E: 'a'..'z'] are not supported.
 */
static int PH7_builtin_ltrim(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	const char *zString;
	int nLen;
	if( nArg < 1 ){
		/* Missing arguments,return null */
		ph7_result_null(pCtx);
		return PH7_OK;
	}
	/* Extract the target string */
	zString = ph7_value_to_string(apArg[0],&nLen);
	if( nLen < 1 ){
		/* Empty string,return */
		ph7_result_string(pCtx,"",0);
		return PH7_OK;
	}
	/* Start the trim process */
	if( nArg < 2 ){
		SyString sStr;
		/* Remove white spaces and NUL byte */
		SyStringInitFromBuf(&sStr,zString,nLen);
		SyStringLeftTrimSafe(&sStr);
		ph7_result_string(pCtx,sStr.zString,(int)sStr.nByte);
	}else{
		/* Char list */
		const char *zList;
		int nListlen;
		zList = ph7_value_to_string(apArg[1],&nListlen);
		if( nListlen < 1 ){
			/* Return the string unchanged */
			ph7_result_string(pCtx,zString,nLen);
		}else{
			const char *zEnd = &zString[nLen];
			const char *zCur = zString;
			const char *zPtr;
			int i;
			/* Left trim */
			for(;;){
				if( zCur >= zEnd ){
					break;
				}
				zPtr = zCur;
				for( i = 0 ; i < nListlen ; i++ ){
					if( zCur < zEnd && zCur[0] == zList[i] ){
						zCur++;
					}
				}
				if( zCur == zPtr ){
					/* No match,break immediately */
					break;
				}
			}
			if( zCur >= zEnd ){
				/* Return the empty string */
				ph7_result_string(pCtx,"",0);
			}else{
				ph7_result_string(pCtx,zCur,(int)(zEnd-zCur));
			}
		}
	}
	return PH7_OK;
}
/*
 * string strtolower(string $str)
 *  Make a string lowercase.
 * Parameters
 *  $str
 *   The input string.
 * Returns.
 *  The lowercased string.
 */
static int PH7_builtin_strtolower(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	const char *zString,*zCur,*zEnd;
	int nLen;
	if( nArg < 1 ){
		/* Missing arguments,return null */
		ph7_result_null(pCtx);
		return PH7_OK;
	}
	/* Extract the target string */
	zString = ph7_value_to_string(apArg[0],&nLen);
	if( nLen < 1 ){
		/* Empty string,return */
		ph7_result_string(pCtx,"",0);
		return PH7_OK;
	}
	/* Perform the requested operation */
	zEnd = &zString[nLen];
	for(;;){
		if( zString >= zEnd ){
			/* No more input,break immediately */
			break;
		}
		if( (unsigned char)zString[0] >= 0xc0 ){
			/* UTF-8 stream,output verbatim */
			zCur = zString;
			zString++;
			while( zString < zEnd && ((unsigned char)zString[0] & 0xc0) == 0x80){
				zString++;
			}
			/* Append UTF-8 stream */
			ph7_result_string(pCtx,zCur,(int)(zString-zCur));
		}else{
			int c = zString[0];
			if( SyisUpper(c) ){
				c = SyToLower(zString[0]);
			}
			/* Append character */
			ph7_result_string(pCtx,(const char *)&c,(int)sizeof(char));
			/* Advance the cursor */
			zString++;
		}
	}
	return PH7_OK;
}
/*
 * string strtolower(string $str)
 *  Make a string uppercase.
 * Parameters
 *  $str
 *   The input string.
 * Returns.
 *  The uppercased string.
 */
static int PH7_builtin_strtoupper(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	const char *zString,*zCur,*zEnd;
	int nLen;
	if( nArg < 1 ){
		/* Missing arguments,return null */
		ph7_result_null(pCtx);
		return PH7_OK;
	}
	/* Extract the target string */
	zString = ph7_value_to_string(apArg[0],&nLen);
	if( nLen < 1 ){
		/* Empty string,return */
		ph7_result_string(pCtx,"",0);
		return PH7_OK;
	}
	/* Perform the requested operation */
	zEnd = &zString[nLen];
	for(;;){
		if( zString >= zEnd ){
			/* No more input,break immediately */
			break;
		}
		if( (unsigned char)zString[0] >= 0xc0 ){
			/* UTF-8 stream,output verbatim */
			zCur = zString;
			zString++;
			while( zString < zEnd && ((unsigned char)zString[0] & 0xc0) == 0x80){
				zString++;
			}
			/* Append UTF-8 stream */
			ph7_result_string(pCtx,zCur,(int)(zString-zCur));
		}else{
			int c = zString[0];
			if( SyisLower(c) ){
				c = SyToUpper(zString[0]);
			}
			/* Append character */
			ph7_result_string(pCtx,(const char *)&c,(int)sizeof(char));
			/* Advance the cursor */
			zString++;
		}
	}
	return PH7_OK;
}
/*
 * string ucfirst(string $str)
 *  Returns a string with the first character of str capitalized, if that
 *  character is alphabetic. 
 * Parameters
 *  $str
 *   The input string.
 * Returns.
 *  The processed string.
 */
static int PH7_builtin_ucfirst(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	const char *zString,*zEnd;
	int nLen,c;
	if( nArg < 1 ){
		/* Missing arguments,return null */
		ph7_result_null(pCtx);
		return PH7_OK;
	}
	/* Extract the target string */
	zString = ph7_value_to_string(apArg[0],&nLen);
	if( nLen < 1 ){
		/* Empty string,return */
		ph7_result_string(pCtx,"",0);
		return PH7_OK;
	}
	/* Perform the requested operation */
	zEnd = &zString[nLen];
	c = zString[0];
	if( SyisLower(c) ){
		c = SyToUpper(c);
	}
	/* Append the first character */
	ph7_result_string(pCtx,(const char *)&c,(int)sizeof(char));
	zString++;
	if( zString < zEnd ){
		/* Append the rest of the input verbatim */
		ph7_result_string(pCtx,zString,(int)(zEnd-zString));
	}
	return PH7_OK;
}
/*
 * string lcfirst(string $str)
 *  Make a string's first character lowercase.
 * Parameters
 *  $str
 *   The input string.
 * Returns.
 *  The processed string.
 */
static int PH7_builtin_lcfirst(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	const char *zString,*zEnd;
	int nLen,c;
	if( nArg < 1 ){
		/* Missing arguments,return null */
		ph7_result_null(pCtx);
		return PH7_OK;
	}
	/* Extract the target string */
	zString = ph7_value_to_string(apArg[0],&nLen);
	if( nLen < 1 ){
		/* Empty string,return */
		ph7_result_string(pCtx,"",0);
		return PH7_OK;
	}
	/* Perform the requested operation */
	zEnd = &zString[nLen];
	c = zString[0];
	if( SyisUpper(c) ){
		c = SyToLower(c);
	}
	/* Append the first character */
	ph7_result_string(pCtx,(const char *)&c,(int)sizeof(char));
	zString++;
	if( zString < zEnd ){
		/* Append the rest of the input verbatim */
		ph7_result_string(pCtx,zString,(int)(zEnd-zString));
	}
	return PH7_OK;
}
/*
 * int ord(string $string)
 *  Returns the ASCII value of the first character of string.
 * Parameters
 *  $str
 *   The input string.
 * Returns.
 *  The ASCII value as an integer.
 */
static int PH7_builtin_ord(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	const char *zString;
	int nLen,c;
	if( nArg < 1 ){
		/* Missing arguments,return -1 */
		ph7_result_int(pCtx,-1);
		return PH7_OK;
	}
	/* Extract the target string */
	zString = ph7_value_to_string(apArg[0],&nLen);
	if( nLen < 1 ){
		/* Empty string,return -1 */
		ph7_result_int(pCtx,-1);
		return PH7_OK;
	}
	/* Extract the ASCII value of the first character */
	c = zString[0];
	/* Return that value */
	ph7_result_int(pCtx,c);
	return PH7_OK;
}
/*
 * string chr(int $ascii)
 *  Returns a one-character string containing the character specified by ascii.
 * Parameters
 *  $ascii
 *   The ascii code.
 * Returns.
 *  The specified character.
 */
static int PH7_builtin_chr(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	int c;
	if( nArg < 1 ){
		/* Missing arguments,return null */
		ph7_result_null(pCtx);
		return PH7_OK;
	}
	/* Extract the ASCII value */
	c = ph7_value_to_int(apArg[0]);
	/* Return the specified character */
	ph7_result_string(pCtx,(const char *)&c,(int)sizeof(char));
	return PH7_OK;
}
/*
 * Binary to hex consumer callback.
 * This callback is the default consumer used by the hash functions
 * [i.e: bin2hex(),md5(),sha1(),md5_file() ... ] defined below.
 */
static int HashConsumer(const void *pData,unsigned int nLen,void *pUserData)
{
	/* Append hex chunk verbatim */
	ph7_result_string((ph7_context *)pUserData,(const char *)pData,(int)nLen);
	return SXRET_OK;
}
/*
 * string bin2hex(string $str)
 *  Convert binary data into hexadecimal representation.
 * Parameters
 *  $str
 *   The input string.
 * Returns.
 *  Returns the hexadecimal representation of the given string.
 */
static int PH7_builtin_bin2hex(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	const char *zString;
	int nLen;
	if( nArg < 1 ){
		/* Missing arguments,return null */
		ph7_result_null(pCtx);
		return PH7_OK;
	}
	/* Extract the target string */
	zString = ph7_value_to_string(apArg[0],&nLen);
	if( nLen < 1 ){
		/* Empty string,return */
		ph7_result_string(pCtx,"",0);
		return PH7_OK;
	}
	/* Perform the requested operation */
	SyBinToHexConsumer((const void *)zString,(sxu32)nLen,HashConsumer,pCtx);
	return PH7_OK;
}
/* Search callback signature */
typedef sxi32 (*ProcStringMatch)(const void *,sxu32,const void *,sxu32,sxu32 *);
/*
 * Case-insensitive pattern match.
 * Brute force is the default search method used here.
 * This is due to the fact that brute-forcing works quite
 * well for short/medium texts on modern hardware.
 */
static sxi32 iPatternMatch(const void *pText,sxu32 nLen,const void *pPattern,sxu32 iPatLen,sxu32 *pOfft)
{
	const char *zpIn = (const char *)pPattern;
	const char *zIn = (const char *)pText;
	const char *zpEnd = &zpIn[iPatLen];
	const char *zEnd = &zIn[nLen];
	const char *zPtr,*zPtr2;
	int c,d;
	if( iPatLen > nLen ){
		/* Don't bother processing */
		return SXERR_NOTFOUND;
	}
	for(;;){
		if( zIn >= zEnd ){
			break;
		}
		c = SyToLower(zIn[0]);
		d = SyToLower(zpIn[0]);
		if( c == d ){
			zPtr   = &zIn[1];
			zPtr2  = &zpIn[1];
			for(;;){
				if( zPtr2 >= zpEnd ){
					/* Pattern found */
					if( pOfft ){ *pOfft = (sxu32)(zIn-(const char *)pText); }
					return SXRET_OK;
				}
				if( zPtr >= zEnd ){
					break;
				}
				c = SyToLower(zPtr[0]);
				d = SyToLower(zPtr2[0]);
				if( c != d ){
					break;
				}
				zPtr++; zPtr2++;
			}
		}
		zIn++;
	}
	/* Pattern not found */
	return SXERR_NOTFOUND;
}
/*
 * string strstr(string $haystack,string $needle[,bool $before_needle = false ])
 *  Find the first occurrence of a string.
 * Parameters
 *  $haystack
 *   The input string.
 * $needle
 *   Search pattern (must be a string).
 * $before_needle
 *   If TRUE, strstr() returns the part of the haystack before the first occurrence 
 *   of the needle (excluding the needle).
 * Return
 *  Returns the portion of string, or FALSE if needle is not found.
 */
static int PH7_builtin_strstr(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	ProcStringMatch xPatternMatch = SyBlobSearch; /* Case-sensitive pattern match */
	const char *zBlob,*zPattern;
	int nLen,nPatLen;
	sxu32 nOfft;
	sxi32 rc;
	if( nArg < 2 ){
		/* Missing arguments,return FALSE */
		ph7_result_bool(pCtx,0);
		return PH7_OK;
	}
	/* Extract the needle and the haystack */
	zBlob = ph7_value_to_string(apArg[0],&nLen);
	zPattern = ph7_value_to_string(apArg[1],&nPatLen);
	nOfft = 0; /* cc warning */
	if( nLen > 0 && nPatLen > 0 ){
		int before = 0;
		/* Perform the lookup */
		rc = xPatternMatch(zBlob,(sxu32)nLen,zPattern,(sxu32)nPatLen,&nOfft);
		if( rc != SXRET_OK ){
			/* Pattern not found,return FALSE */
			ph7_result_bool(pCtx,0);
			return PH7_OK;
		}
		/* Return the portion of the string */
		if( nArg > 2 ){
			before = ph7_value_to_int(apArg[2]);
		}
		if( before ){
			ph7_result_string(pCtx,zBlob,(int)(&zBlob[nOfft]-zBlob));
		}else{
			ph7_result_string(pCtx,&zBlob[nOfft],(int)(&zBlob[nLen]-&zBlob[nOfft]));
		}
	}else{
		ph7_result_bool(pCtx,0);
	}
	return PH7_OK;
}
/*
 * string stristr(string $haystack,string $needle[,bool $before_needle = false ])
 *  Case-insensitive strstr().
 * Parameters
 *  $haystack
 *   The input string.
 * $needle
 *   Search pattern (must be a string).
 * $before_needle
 *   If TRUE, strstr() returns the part of the haystack before the first occurrence 
 *   of the needle (excluding the needle).
 * Return
 *  Returns the portion of string, or FALSE if needle is not found.
 */
static int PH7_builtin_stristr(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	ProcStringMatch xPatternMatch = iPatternMatch; /* Case-insensitive pattern match */
	const char *zBlob,*zPattern;
	int nLen,nPatLen;
	sxu32 nOfft;
	sxi32 rc;
	if( nArg < 2 ){
		/* Missing arguments,return FALSE */
		ph7_result_bool(pCtx,0);
		return PH7_OK;
	}
	/* Extract the needle and the haystack */
	zBlob = ph7_value_to_string(apArg[0],&nLen);
	zPattern = ph7_value_to_string(apArg[1],&nPatLen);
	nOfft = 0; /* cc warning */
	if( nLen > 0 && nPatLen > 0 ){
		int before = 0;
		/* Perform the lookup */
		rc = xPatternMatch(zBlob,(sxu32)nLen,zPattern,(sxu32)nPatLen,&nOfft);
		if( rc != SXRET_OK ){
			/* Pattern not found,return FALSE */
			ph7_result_bool(pCtx,0);
			return PH7_OK;
		}
		/* Return the portion of the string */
		if( nArg > 2 ){
			before = ph7_value_to_int(apArg[2]);
		}
		if( before ){
			ph7_result_string(pCtx,zBlob,(int)(&zBlob[nOfft]-zBlob));
		}else{
			ph7_result_string(pCtx,&zBlob[nOfft],(int)(&zBlob[nLen]-&zBlob[nOfft]));
		}
	}else{
		ph7_result_bool(pCtx,0);
	}
	return PH7_OK;
}
/*
 * int strpos(string $haystack,string $needle [,int $offset = 0 ] )
 *  Returns the numeric position of the first occurrence of needle in the haystack string.
 * Parameters
 *  $haystack
 *   The input string.
 * $needle
 *   Search pattern (must be a string).
 * $offset
 *   This optional offset parameter allows you to specify which character in haystack
 *   to start searching. The position returned is still relative to the beginning
 *   of haystack.
 * Return
 *  Returns the position as an integer.If needle is not found, strpos() will return FALSE.
 */
static int PH7_builtin_strpos(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	ProcStringMatch xPatternMatch = SyBlobSearch; /* Case-sensitive pattern match */
	const char *zBlob,*zPattern;
	int nLen,nPatLen,nStart;
	sxu32 nOfft;
	sxi32 rc;
	if( nArg < 2 ){
		/* Missing arguments,return FALSE */
		ph7_result_bool(pCtx,0);
		return PH7_OK;
	}
	/* Extract the needle and the haystack */
	zBlob = ph7_value_to_string(apArg[0],&nLen);
	zPattern = ph7_value_to_string(apArg[1],&nPatLen);
	nOfft = 0; /* cc warning */
	nStart = 0;
	/* Peek the starting offset if available */
	if( nArg > 2 ){
		nStart = ph7_value_to_int(apArg[2]);
		if( nStart < 0 ){
			nStart = -nStart;
		}
		if( nStart >= nLen ){
			/* Invalid offset */
			nStart = 0;
		}else{
			zBlob += nStart;
			nLen -= nStart;
		}
	}
	if( nLen > 0 && nPatLen > 0 ){
		/* Perform the lookup */
		rc = xPatternMatch(zBlob,(sxu32)nLen,zPattern,(sxu32)nPatLen,&nOfft);
		if( rc != SXRET_OK ){
			/* Pattern not found,return FALSE */
			ph7_result_bool(pCtx,0);
			return PH7_OK;
		}
		/* Return the pattern position */
		ph7_result_int64(pCtx,(ph7_int64)(nOfft+nStart));
	}else{
		ph7_result_bool(pCtx,0);
	}
	return PH7_OK;
}
/*
 * int stripos(string $haystack,string $needle [,int $offset = 0 ] )
 *  Case-insensitive strpos.
 * Parameters
 *  $haystack
 *   The input string.
 * $needle
 *   Search pattern (must be a string).
 * $offset
 *   This optional offset parameter allows you to specify which character in haystack
 *   to start searching. The position returned is still relative to the beginning
 *   of haystack.
 * Return
 *  Returns the position as an integer.If needle is not found, strpos() will return FALSE.
 */
static int PH7_builtin_stripos(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	ProcStringMatch xPatternMatch = iPatternMatch; /* Case-insensitive pattern match */
	const char *zBlob,*zPattern;
	int nLen,nPatLen,nStart;
	sxu32 nOfft;
	sxi32 rc;
	if( nArg < 2 ){
		/* Missing arguments,return FALSE */
		ph7_result_bool(pCtx,0);
		return PH7_OK;
	}
	/* Extract the needle and the haystack */
	zBlob = ph7_value_to_string(apArg[0],&nLen);
	zPattern = ph7_value_to_string(apArg[1],&nPatLen);
	nOfft = 0; /* cc warning */
	nStart = 0;
	/* Peek the starting offset if available */
	if( nArg > 2 ){
		nStart = ph7_value_to_int(apArg[2]);
		if( nStart < 0 ){
			nStart = -nStart;
		}
		if( nStart >= nLen ){
			/* Invalid offset */
			nStart = 0;
		}else{
			zBlob += nStart;
			nLen -= nStart;
		}
	}
	if( nLen > 0 && nPatLen > 0 ){
		/* Perform the lookup */
		rc = xPatternMatch(zBlob,(sxu32)nLen,zPattern,(sxu32)nPatLen,&nOfft);
		if( rc != SXRET_OK ){
			/* Pattern not found,return FALSE */
			ph7_result_bool(pCtx,0);
			return PH7_OK;
		}
		/* Return the pattern position */
		ph7_result_int64(pCtx,(ph7_int64)(nOfft+nStart));
	}else{
		ph7_result_bool(pCtx,0);
	}
	return PH7_OK;
}
/*
 * int strrpos(string $haystack,string $needle [,int $offset = 0 ] )
 *  Find the numeric position of the last occurrence of needle in the haystack string.
 * Parameters
 *  $haystack
 *   The input string.
 * $needle
 *   Search pattern (must be a string).
 * $offset
 *   If specified, search will start this number of characters counted from the beginning
 *   of the string. If the value is negative, search will instead start from that many 
 *   characters from the end of the string, searching backwards.
 * Return
 *  Returns the position as an integer.If needle is not found, strrpos() will return FALSE.
 */
static int PH7_builtin_strrpos(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	const char *zStart,*zBlob,*zPattern,*zPtr,*zEnd;
	ProcStringMatch xPatternMatch = SyBlobSearch; /* Case-sensitive pattern match */
	int nLen,nPatLen;
	sxu32 nOfft;
	sxi32 rc;
	if( nArg < 2 ){
		/* Missing arguments,return FALSE */
		ph7_result_bool(pCtx,0);
		return PH7_OK;
	}
	/* Extract the needle and the haystack */
	zBlob = ph7_value_to_string(apArg[0],&nLen);
	zPattern = ph7_value_to_string(apArg[1],&nPatLen);
	/* Point to the end of the pattern */
	zPtr = &zBlob[nLen - 1];
	zEnd = &zBlob[nLen];
	/* Save the starting posistion */
	zStart = zBlob;
	nOfft = 0; /* cc warning */
	/* Peek the starting offset if available */
	if( nArg > 2 ){
		int nStart;
		nStart = ph7_value_to_int(apArg[2]);
		if( nStart < 0 ){
			nStart = -nStart;
			if( nStart >= nLen ){
				/* Invalid offset */
				ph7_result_bool(pCtx,0);
				return PH7_OK;
			}else{
				nLen -= nStart;
				zPtr = &zBlob[nLen - 1];
				zEnd = &zBlob[nLen];
			}
		}else{
			if( nStart >= nLen ){
				/* Invalid offset */
				ph7_result_bool(pCtx,0);
				return PH7_OK;
			}else{
				zBlob += nStart;
				nLen -= nStart;
			}
		}
	}
	if( nLen > 0 && nPatLen > 0 ){
		/* Perform the lookup */
		for(;;){
			if( zBlob >= zPtr ){
				break;
			}
			rc = xPatternMatch((const void *)zPtr,(sxu32)(zEnd-zPtr),(const void *)zPattern,(sxu32)nPatLen,&nOfft);
			if( rc == SXRET_OK ){
				/* Pattern found,return it's position */
				ph7_result_int64(pCtx,(ph7_int64)(&zPtr[nOfft] - zStart));
				return PH7_OK;
			}
			zPtr--;
		}
		/* Pattern not found,return FALSE */
		ph7_result_bool(pCtx,0);
	}else{
		ph7_result_bool(pCtx,0);
	}
	return PH7_OK;
}
/*
 * int strripos(string $haystack,string $needle [,int $offset = 0 ] )
 *  Case-insensitive strrpos.
 * Parameters
 *  $haystack
 *   The input string.
 * $needle
 *   Search pattern (must be a string).
 * $offset
 *   If specified, search will start this number of characters counted from the beginning
 *   of the string. If the value is negative, search will instead start from that many 
 *   characters from the end of the string, searching backwards.
 * Return
 *  Returns the position as an integer.If needle is not found, strrpos() will return FALSE.
 */
static int PH7_builtin_strripos(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	const char *zStart,*zBlob,*zPattern,*zPtr,*zEnd;
	ProcStringMatch xPatternMatch = iPatternMatch; /* Case-insensitive pattern match */
	int nLen,nPatLen;
	sxu32 nOfft;
	sxi32 rc;
	if( nArg < 2 ){
		/* Missing arguments,return FALSE */
		ph7_result_bool(pCtx,0);
		return PH7_OK;
	}
	/* Extract the needle and the haystack */
	zBlob = ph7_value_to_string(apArg[0],&nLen);
	zPattern = ph7_value_to_string(apArg[1],&nPatLen);
	/* Point to the end of the pattern */
	zPtr = &zBlob[nLen - 1];
	zEnd = &zBlob[nLen];
	/* Save the starting posistion */
	zStart = zBlob;
	nOfft = 0; /* cc warning */
	/* Peek the starting offset if available */
	if( nArg > 2 ){
		int nStart;
		nStart = ph7_value_to_int(apArg[2]);
		if( nStart < 0 ){
			nStart = -nStart;
			if( nStart >= nLen ){
				/* Invalid offset */
				ph7_result_bool(pCtx,0);
				return PH7_OK;
			}else{
				nLen -= nStart;
				zPtr = &zBlob[nLen - 1];
				zEnd = &zBlob[nLen];
			}
		}else{
			if( nStart >= nLen ){
				/* Invalid offset */
				ph7_result_bool(pCtx,0);
				return PH7_OK;
			}else{
				zBlob += nStart;
				nLen -= nStart;
			}
		}
	}
	if( nLen > 0 && nPatLen > 0 ){
		/* Perform the lookup */
		for(;;){
			if( zBlob >= zPtr ){
				break;
			}
			rc = xPatternMatch((const void *)zPtr,(sxu32)(zEnd-zPtr),(const void *)zPattern,(sxu32)nPatLen,&nOfft);
			if( rc == SXRET_OK ){
				/* Pattern found,return it's position */
				ph7_result_int64(pCtx,(ph7_int64)(&zPtr[nOfft] - zStart));
				return PH7_OK;
			}
			zPtr--;
		}
		/* Pattern not found,return FALSE */
		ph7_result_bool(pCtx,0);
	}else{
		ph7_result_bool(pCtx,0);
	}
	return PH7_OK;
}
/*
 * int strrchr(string $haystack,mixed $needle)
 *  Find the last occurrence of a character in a string.
 * Parameters
 *  $haystack
 *   The input string.
 * $needle
 *  If needle contains more than one character, only the first is used.
 *  This behavior is different from that of strstr().
 *  If needle is not a string, it is converted to an integer and applied
 *  as the ordinal value of a character.
 * Return
 *  This function returns the portion of string, or FALSE if needle is not found.
 */
static int PH7_builtin_strrchr(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	const char *zBlob;
	int nLen,c;
	if( nArg < 2 ){
		/* Missing arguments,return FALSE */
		ph7_result_bool(pCtx,0);
		return PH7_OK;
	}
	/* Extract the haystack */
	zBlob = ph7_value_to_string(apArg[0],&nLen);
	c = 0; /* cc warning */
	if( nLen > 0 ){
		sxu32 nOfft;
		sxi32 rc;
		if( ph7_value_is_string(apArg[1]) ){
			const char *zPattern;
			zPattern = ph7_value_to_string(apArg[1],0); /* Never fail,so there is no need to check
														 * for NULL pointer.
														 */
			c = zPattern[0];
		}else{
			/* Int cast */
			c = ph7_value_to_int(apArg[1]);
		}
		/* Perform the lookup */
		rc = SyByteFind2(zBlob,(sxu32)nLen,c,&nOfft);
		if( rc != SXRET_OK ){
			/* No such entry,return FALSE */
			ph7_result_bool(pCtx,0);
			return PH7_OK;
		}
		/* Return the string portion */
		ph7_result_string(pCtx,&zBlob[nOfft],(int)(&zBlob[nLen]-&zBlob[nOfft]));
	}else{
		ph7_result_bool(pCtx,0);
	}
	return PH7_OK;
}
/*
 * string strrev(string $string)
 *  Reverse a string.
 * Parameters
 *  $string
 *   String to be reversed.
 * Return
 *  The reversed string.
 */
static int PH7_builtin_strrev(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	const char *zIn,*zEnd;
	int nLen,c;
	if( nArg < 1 ){
		/* Missing arguments,return NULL */
		ph7_result_null(pCtx);
		return PH7_OK;
	}
	/* Extract the target string */
	zIn = ph7_value_to_string(apArg[0],&nLen);
	if( nLen < 1 ){
		/* Empty string Return null */
		ph7_result_null(pCtx);
		return PH7_OK;
	}
	/* Perform the requested operation */
	zEnd = &zIn[nLen - 1];
	for(;;){
		if( zEnd < zIn ){
			/* No more input to process */
			break;
		}
		/* Append current character */
		c = zEnd[0];
		ph7_result_string(pCtx,(const char *)&c,(int)sizeof(char));
		zEnd--;
	}
	return PH7_OK;
}
/*
 * string ucwords(string $string)
 *  Uppercase the first character of each word in a string.
 *  The definition of a word is any string of characters that is immediately after
 *  a whitespace (These are: space, form-feed, newline, carriage return, horizontal tab, and vertical tab). 
 * Parameters
 *  $string
 *   The input string.
 * Return
 *  The modified string..
 */
static int PH7_builtin_ucwords(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	const char *zIn,*zCur,*zEnd;
	int nLen,c;
	if( nArg < 1 ){
		/* Missing arguments,return NULL */
		ph7_result_null(pCtx);
		return PH7_OK;
	}
	/* Extract the target string */
	zIn = ph7_value_to_string(apArg[0],&nLen);
	if( nLen < 1 ){
		/* Empty string Return null */
		ph7_result_null(pCtx);
		return PH7_OK;
	}
	/* Perform the requested operation */
	zEnd = &zIn[nLen];
	for(;;){
		/* Jump leading white spaces */
		zCur = zIn;
		while( zIn < zEnd && (unsigned char)zIn[0] < 0x80 && SyisSpace(zIn[0]) ){
			zIn++;
		}
		if( zCur < zIn ){
			/* Append white space stream */
			ph7_result_string(pCtx,zCur,(int)(zIn-zCur));
		}
		if( zIn >= zEnd ){
			/* No more input to process */
			break;
		}
		c = zIn[0];
		if( c < 0x80 && SyisLower(c) ){
			c = SyToUpper(c);
		}
		/* Append the upper-cased character */
		ph7_result_string(pCtx,(const char *)&c,(int)sizeof(char));
		zIn++;
		zCur = zIn;
		/* Append the word varbatim */
		while( zIn < zEnd ){
			if( (unsigned char)zIn[0] >= 0xc0 ){
				/* UTF-8 stream */
				zIn++;
				SX_JMP_UTF8(zIn,zEnd);
			}else if( !SyisSpace(zIn[0]) ){
				zIn++;
			}else{
				break;
			}
		}
		if( zCur < zIn ){
			ph7_result_string(pCtx,zCur,(int)(zIn-zCur));
		}
	}
	return PH7_OK;
}
/*
 * string str_repeat(string $input,int $multiplier)
 *  Returns input repeated multiplier times.
 * Parameters
 *  $string
 *   String to be repeated.
 * $multiplier
 *  Number of time the input string should be repeated.
 *  multiplier has to be greater than or equal to 0. If the multiplier is set
 *  to 0, the function will return an empty string.
 * Return
 *  The repeated string.
 */
static int PH7_builtin_str_repeat(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	const char *zIn;
	int nLen,nMul;
	int rc;
	if( nArg < 2 ){
		/* Missing arguments,return NULL */
		ph7_result_null(pCtx);
		return PH7_OK;
	}
	/* Extract the target string */
	zIn = ph7_value_to_string(apArg[0],&nLen);
	if( nLen < 1 ){
		/* Empty string.Return null */
		ph7_result_null(pCtx);
		return PH7_OK;
	}
	/* Extract the multiplier */
	nMul = ph7_value_to_int(apArg[1]);
	if( nMul < 1 ){
		/* Return the empty string */
		ph7_result_string(pCtx,"",0);
		return PH7_OK;
	}
	/* Perform the requested operation */
	for(;;){
		if( !nMul ){
			break;
		}
		/* Append the copy */
		rc = ph7_result_string(pCtx,zIn,nLen);
		if( rc != PH7_OK ){
			/* Out of memory,break immediately */
			break;
		}
		nMul--;
	}
	return PH7_OK;
}
/*
 * string nl2br(string $string[,bool $is_xhtml = true ])
 *  Inserts HTML line breaks before all newlines in a string.
 * Parameters
 *  $string
 *   The input string.
 * $is_xhtml
 *   Whenever to use XHTML compatible line breaks or not.
 * Return
 *  The processed string.
 */
static int PH7_builtin_nl2br(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	const char *zIn,*zCur,*zEnd;
	int is_xhtml = 0;
	int nLen;
	if( nArg < 1 ){
		/* Missing arguments,return the empty string */
		ph7_result_string(pCtx,"",0);
		return PH7_OK;
	}
	/* Extract the target string */
	zIn = ph7_value_to_string(apArg[0],&nLen);
	if( nLen < 1 ){
		/* Empty string,return null */
		ph7_result_null(pCtx);
		return PH7_OK;
	}
	if( nArg > 1 ){
		is_xhtml = ph7_value_to_bool(apArg[1]);
	}
	zEnd = &zIn[nLen];
	/* Perform the requested operation */
	for(;;){
		zCur = zIn;
		/* Delimit the string */
		while( zIn < zEnd && (zIn[0] != '\n'&& zIn[0] != '\r') ){
			zIn++;
		}
		if( zCur < zIn ){
			/* Output chunk verbatim */
			ph7_result_string(pCtx,zCur,(int)(zIn-zCur));
		}
		if( zIn >= zEnd ){
			/* No more input to process */
			break;
		}
		/* Output the HTML line break */
		if( is_xhtml ){
			ph7_result_string(pCtx,"<br>",(int)sizeof("<br>")-1);
		}else{
			ph7_result_string(pCtx,"<br/>",(int)sizeof("<br/>")-1);
		}
		zCur = zIn;
		/* Append trailing line */
		while( zIn < zEnd && (zIn[0] == '\n'  || zIn[0] == '\r') ){
			zIn++;
		}
		if( zCur < zIn ){
			/* Output chunk verbatim */
			ph7_result_string(pCtx,zCur,(int)(zIn-zCur));
		}
	}
	return PH7_OK;
}
/*
 * Format a given string and invoke the given callback on each processed chunk.
 *  According to the PHP reference manual.
 * The format string is composed of zero or more directives: ordinary characters
 * (excluding %) that are copied directly to the result, and conversion 
 * specifications, each of which results in fetching its own parameter.
 * This applies to both sprintf() and printf().
 * Each conversion specification consists of a percent sign (%), followed by one
 * or more of these elements, in order:
 *   An optional sign specifier that forces a sign (- or +) to be used on a number.
 *   By default, only the - sign is used on a number if it's negative. This specifier forces
 *   positive numbers to have the + sign attached as well.
 *   An optional padding specifier that says what character will be used for padding
 *   the results to the right string size. This may be a space character or a 0 (zero character).
 *   The default is to pad with spaces. An alternate padding character can be specified by prefixing
 *   it with a single quote ('). See the examples below.
 *   An optional alignment specifier that says if the result should be left-justified or right-justified.
 *   The default is right-justified; a - character here will make it left-justified.
 *   An optional number, a width specifier that says how many characters (minimum) this conversion
 *   should result in.
 *   An optional precision specifier in the form of a period (`.') followed by an optional decimal
 *   digit string that says how many decimal digits should be displayed for floating-point numbers.
 *   When using this specifier on a string, it acts as a cutoff point, setting a maximum character
 *   limit to the string.
 *  A type specifier that says what type the argument data should be treated as. Possible types:
 *       % - a literal percent character. No argument is required.
 *       b - the argument is treated as an integer, and presented as a binary number.
 *       c - the argument is treated as an integer, and presented as the character with that ASCII value.
 *       d - the argument is treated as an integer, and presented as a (signed) decimal number.
 *       e - the argument is treated as scientific notation (e.g. 1.2e+2). The precision specifier stands 
 * 	     for the number of digits after the decimal point.
 *       E - like %e but uses uppercase letter (e.g. 1.2E+2).
 *       u - the argument is treated as an integer, and presented as an unsigned decimal number.
 *       f - the argument is treated as a float, and presented as a floating-point number (locale aware).
 *       F - the argument is treated as a float, and presented as a floating-point number (non-locale aware).
 *       g - shorter of %e and %f.
 *       G - shorter of %E and %f.
 *       o - the argument is treated as an integer, and presented as an octal number.
 *       s - the argument is treated as and presented as a string.
 *       x - the argument is treated as an integer and presented as a hexadecimal number (with lowercase letters).
 *       X - the argument is treated as an integer and presented as a hexadecimal number (with uppercase letters).
 */
/*
 * This implementation is based on the one found in the SQLite3 source tree.
 */
#define PH7_FMT_BUFSIZ 1024 /* Conversion buffer size */
/*
** Conversion types fall into various categories as defined by the
** following enumeration.
*/
#define PH7_FMT_RADIX       1 /* Integer types.%d, %x, %o, and so forth */
#define PH7_FMT_FLOAT       2 /* Floating point.%f */
#define PH7_FMT_EXP         3 /* Exponentional notation.%e and %E */
#define PH7_FMT_GENERIC     4 /* Floating or exponential, depending on exponent.%g */
#define PH7_FMT_SIZE        5 /* Total number of characters processed so far.%n */
#define PH7_FMT_STRING      6 /* Strings.%s */
#define PH7_FMT_PERCENT     7 /* Percent symbol.%% */
#define PH7_FMT_CHARX       8 /* Characters.%c */
#define PH7_FMT_ERROR       9 /* Used to indicate no such conversion type */
/*
** Allowed values for ph7_fmt_info.flags
*/
#define PH7_FMT_FLAG_SIGNED	  0x01
#define PH7_FMT_FLAG_UNSIGNED 0x02
/*
** Each builtin conversion character (ex: the 'd' in "%d") is described
** by an instance of the following structure
*/
typedef struct ph7_fmt_info ph7_fmt_info;
struct ph7_fmt_info
{
  char fmttype;  /* The format field code letter [i.e: 'd','s','x'] */
  sxu8 base;     /* The base for radix conversion */
  int flags;    /* One or more of PH7_FMT_FLAG_ constants below */
  sxu8 type;     /* Conversion paradigm */
  char *charset; /* The character set for conversion */
  char *prefix;  /* Prefix on non-zero values in alt format */
};
#ifndef PH7_OMIT_FLOATING_POINT
/*
** "*val" is a double such that 0.1 <= *val < 10.0
** Return the ascii code for the leading digit of *val, then
** multiply "*val" by 10.0 to renormalize.
**
** Example:
**     input:     *val = 3.14159
**     output:    *val = 1.4159    function return = '3'
**
** The counter *cnt is incremented each time.  After counter exceeds
** 16 (the number of significant digits in a 64-bit float) '0' is
** always returned.
*/
static int vxGetdigit(sxlongreal *val,int *cnt)
{
  sxlongreal d;
  int digit;

  if( (*cnt)++ >= 16 ){
	  return '0';
  }
  digit = (int)*val;
  d = digit;
   *val = (*val - d)*10.0;
  return digit + '0' ;
}
#endif /* PH7_OMIT_FLOATING_POINT */
/*
 * The following table is searched linearly, so it is good to put the most frequently
 * used conversion types first.
 */
static const ph7_fmt_info aFmt[] = {
  {  'd', 10, PH7_FMT_FLAG_SIGNED, PH7_FMT_RADIX, "0123456789",0    },
  {  's',  0, 0, PH7_FMT_STRING,     0,                  0    },
  {  'c',  0, 0, PH7_FMT_CHARX,      0,                  0    },
  {  'x', 16, 0, PH7_FMT_RADIX,      "0123456789abcdef", "x0" },
  {  'X', 16, 0, PH7_FMT_RADIX,      "0123456789ABCDEF", "X0" },
  {  'b',  2, 0, PH7_FMT_RADIX,      "01",                "b0"},
  {  'o',  8, 0, PH7_FMT_RADIX,      "01234567",         "0"  },
  {  'u', 10, 0, PH7_FMT_RADIX,      "0123456789",       0    },
  {  'f',  0, PH7_FMT_FLAG_SIGNED, PH7_FMT_FLOAT,        0,    0    },
  {  'F',  0, PH7_FMT_FLAG_SIGNED, PH7_FMT_FLOAT,        0,    0    },
  {  'e',  0, PH7_FMT_FLAG_SIGNED, PH7_FMT_EXP,        "e",    0    },
  {  'E',  0, PH7_FMT_FLAG_SIGNED, PH7_FMT_EXP,        "E",    0    },
  {  'g',  0, PH7_FMT_FLAG_SIGNED, PH7_FMT_GENERIC,    "e",    0    },
  {  'G',  0, PH7_FMT_FLAG_SIGNED, PH7_FMT_GENERIC,    "E",    0    },
  {  '%',  0, 0, PH7_FMT_PERCENT,    0,                  0    }
};
/*
 * Format a given string.
 * The root program.  All variations call this core.
 * INPUTS:
 *   xConsumer   This is a pointer to a function taking four arguments
 *            1. A pointer to the call context.
 *            2. A pointer to the list of characters to be output
 *               (Note, this list is NOT null terminated.)
 *            3. An integer number of characters to be output.
 *               (Note: This number might be zero.)
 *            4. Upper layer private data.
 *   zIn       This is the format string, as in the usual print.
 *   apArg     This is a pointer to a list of arguments.
 */
PH7_PRIVATE sxi32 PH7_InputFormat(
	int (*xConsumer)(ph7_context *,const char *,int,void *), /* Format consumer */
	ph7_context *pCtx,  /* call context */
	const char *zIn,    /* Format string */
	int nByte,          /* Format string length */
	int nArg,           /* Total argument of the given arguments */
	ph7_value **apArg,  /* User arguments */
	void *pUserData,    /* Last argument to xConsumer() */
	int vf              /* TRUE if called from vfprintf,vsprintf context */ 
	)
{
	char spaces[] = "                                                  ";
#define etSPACESIZE ((int)sizeof(spaces)-1)
	const char *zCur,*zEnd = &zIn[nByte];
	char *zBuf,zWorker[PH7_FMT_BUFSIZ];       /* Working buffer */
	const ph7_fmt_info *pInfo;  /* Pointer to the appropriate info structure */
	int flag_alternateform; /* True if "#" flag is present */
	int flag_leftjustify;   /* True if "-" flag is present */
	int flag_blanksign;     /* True if " " flag is present */
	int flag_plussign;      /* True if "+" flag is present */
	int flag_zeropad;       /* True if field width constant starts with zero */
	ph7_value *pArg;         /* Current processed argument */
	ph7_int64 iVal;
	int precision;           /* Precision of the current field */
	char *zExtra;  
	int c,rc,n;
	int length;              /* Length of the field */
	int prefix;
	sxu8 xtype;              /* Conversion paradigm */
	int width;               /* Width of the current field */
	int idx;
	n = (vf == TRUE) ? 0 : 1;
#define NEXT_ARG	( n < nArg ? apArg[n++] : 0 )
	/* Start the format process */
	for(;;){
		zCur = zIn;
		while( zIn < zEnd && zIn[0] != '%' ){
			zIn++;
		}
		if( zCur < zIn ){
			/* Consume chunk verbatim */
			rc = xConsumer(pCtx,zCur,(int)(zIn-zCur),pUserData);
			if( rc == SXERR_ABORT ){
				/* Callback request an operation abort */
				break;
			}
		}
		if( zIn >= zEnd ){
			/* No more input to process,break immediately */
			break;
		}
		/* Find out what flags are present */
		flag_leftjustify = flag_plussign = flag_blanksign = 
			flag_alternateform = flag_zeropad = 0;
		zIn++; /* Jump the precent sign */
		do{
			c = zIn[0];
			switch( c ){
			case '-':   flag_leftjustify = 1;     c = 0;   break;
			case '+':   flag_plussign = 1;        c = 0;   break;
			case ' ':   flag_blanksign = 1;       c = 0;   break;
			case '#':   flag_alternateform = 1;   c = 0;   break;
			case '0':   flag_zeropad = 1;         c = 0;   break;
			case '\'':
				zIn++;
				if( zIn < zEnd ){
					/* An alternate padding character can be specified by prefixing it with a single quote (') */
					c = zIn[0];
					for(idx = 0 ; idx < etSPACESIZE ; ++idx ){
						spaces[idx] = (char)c;
					}
					c = 0;
				}
				break;
			default:                                       break;
			}
		}while( c==0 && (zIn++ < zEnd) );
		/* Get the field width */
		width = 0;
		while( zIn < zEnd && ( zIn[0] >='0' && zIn[0] <='9') ){
			width = width*10 + (zIn[0] - '0');
			zIn++;
		}
		if( zIn < zEnd && zIn[0] == '$' ){
			/* Position specifer */
			if( width > 0 ){
				n = width;
				if( vf && n > 0 ){ 
					n--;
				}
			}
			zIn++;
			width = 0;
			if( zIn < zEnd && zIn[0] == '0' ){
				flag_zeropad = 1;
				zIn++;
			}
			while( zIn < zEnd && ( zIn[0] >='0' && zIn[0] <='9') ){
				width = width*10 + (zIn[0] - '0');
				zIn++;
			}
		}
		if( width > PH7_FMT_BUFSIZ-10 ){
			width = PH7_FMT_BUFSIZ-10;
		}
		/* Get the precision */
		precision = -1;
		if( zIn < zEnd && zIn[0] == '.' ){
			precision = 0;
			zIn++;
			while( zIn < zEnd && ( zIn[0] >='0' && zIn[0] <='9') ){
				precision = precision*10 + (zIn[0] - '0');
				zIn++;
			}
		}
		if( zIn >= zEnd ){
			/* No more input */
			break;
		}
		/* Fetch the info entry for the field */
		pInfo = 0;
		xtype = PH7_FMT_ERROR;
		c = zIn[0];
		zIn++; /* Jump the format specifer */
		for(idx=0; idx< (int)SX_ARRAYSIZE(aFmt); idx++){
			if( c==aFmt[idx].fmttype ){
				pInfo = &aFmt[idx];
				xtype = pInfo->type;
				break;
			}
		}
		zBuf = zWorker; /* Point to the working buffer */
		length = 0;
		zExtra = 0;
		 /*
		  ** At this point, variables are initialized as follows:
		  **
		  **   flag_alternateform          TRUE if a '#' is present.
		  **   flag_plussign               TRUE if a '+' is present.
		  **   flag_leftjustify            TRUE if a '-' is present or if the
		  **                               field width was negative.
		  **   flag_zeropad                TRUE if the width began with 0.
		  **                               the conversion character.
		  **   flag_blanksign              TRUE if a ' ' is present.
		  **   width                       The specified field width.  This is
		  **                               always non-negative.  Zero is the default.
		  **   precision                   The specified precision.  The default
		  **                               is -1.
		  */
		switch(xtype){
		case PH7_FMT_PERCENT:
			/* A literal percent character */
			zWorker[0] = '%';
			length = (int)sizeof(char);
			break;
		case PH7_FMT_CHARX:
			/* The argument is treated as an integer, and presented as the character
			 * with that ASCII value
			 */
			pArg = NEXT_ARG;
			if( pArg == 0 ){
				c = 0;
			}else{
				c = ph7_value_to_int(pArg);
			}
			/* NUL byte is an acceptable value */
			zWorker[0] = (char)c;
			length = (int)sizeof(char);
			break;
		case PH7_FMT_STRING:
			/* the argument is treated as and presented as a string */
			pArg = NEXT_ARG;
			if( pArg == 0 ){
				length = 0;
			}else{
				zBuf = (char *)ph7_value_to_string(pArg,&length);
			}
			if( length < 1 ){
				zBuf = " ";
				length = (int)sizeof(char);
			}
			if( precision>=0 && precision<length ){
				length = precision;
			}
			if( flag_zeropad ){
				/* zero-padding works on strings too */
				for(idx = 0 ; idx < etSPACESIZE ; ++idx ){
					spaces[idx] = '0';
				}
			}
			break;
		case PH7_FMT_RADIX:
			pArg = NEXT_ARG;
			if( pArg == 0 ){
				iVal = 0;
			}else{
				iVal = ph7_value_to_int64(pArg);
			}
			/* Limit the precision to prevent overflowing buf[] during conversion */
			if( precision>PH7_FMT_BUFSIZ-40 ){
				precision = PH7_FMT_BUFSIZ-40;
			}
#if 1
        /* For the format %#x, the value zero is printed "0" not "0x0".
        ** I think this is stupid.*/
        if( iVal==0 ) flag_alternateform = 0;
#else
        /* More sensible: turn off the prefix for octal (to prevent "00"),
        ** but leave the prefix for hex.*/
        if( iVal==0 && pInfo->base==8 ) flag_alternateform = 0;
#endif
        if( pInfo->flags & PH7_FMT_FLAG_SIGNED ){
          if( iVal<0 ){ 
            iVal = -iVal;
			/* Ticket 1433-003 */
			if( iVal < 0 ){
				/* Overflow */
				iVal= 0x7FFFFFFFFFFFFFFF;
			}
            prefix = '-';
          }else if( flag_plussign )  prefix = '+';
          else if( flag_blanksign )  prefix = ' ';
          else                       prefix = 0;
        }else{
			if( iVal<0 ){
				iVal = -iVal;
				/* Ticket 1433-003 */
				if( iVal < 0 ){
					/* Overflow */
					iVal= 0x7FFFFFFFFFFFFFFF;
				}
			}
			prefix = 0;
		}
        if( flag_zeropad && precision<width-(prefix!=0) ){
          precision = width-(prefix!=0);
        }
        zBuf = &zWorker[PH7_FMT_BUFSIZ-1];
        {
          register char *cset;      /* Use registers for speed */
          register int base;
          cset = pInfo->charset;
          base = pInfo->base;
          do{                                           /* Convert to ascii */
            *(--zBuf) = cset[iVal%base];
            iVal = iVal/base;
          }while( iVal>0 );
        }
        length = &zWorker[PH7_FMT_BUFSIZ-1]-zBuf;
        for(idx=precision-length; idx>0; idx--){
          *(--zBuf) = '0';                             /* Zero pad */
        }
        if( prefix ) *(--zBuf) = (char)prefix;               /* Add sign */
        if( flag_alternateform && pInfo->prefix ){      /* Add "0" or "0x" */
          char *pre, x;
          pre = pInfo->prefix;
          if( *zBuf!=pre[0] ){
            for(pre=pInfo->prefix; (x=(*pre))!=0; pre++) *(--zBuf) = x;
          }
        }
        length = &zWorker[PH7_FMT_BUFSIZ-1]-zBuf;
		break;
		case PH7_FMT_FLOAT:
		case PH7_FMT_EXP:
		case PH7_FMT_GENERIC:{
#ifndef PH7_OMIT_FLOATING_POINT
		long double realvalue;
		int  exp;                /* exponent of real numbers */
		double rounder;          /* Used for rounding floating point values */
		int flag_dp;            /* True if decimal point should be shown */
		int flag_rtz;           /* True if trailing zeros should be removed */
		int flag_exp;           /* True to force display of the exponent */
		int nsd;                 /* Number of significant digits returned */
		pArg = NEXT_ARG;
		if( pArg == 0 ){
			realvalue = 0;
		}else{
			realvalue = ph7_value_to_double(pArg);
		}
        if( precision<0 ) precision = 6;         /* Set default precision */
        if( precision>PH7_FMT_BUFSIZ-40) precision = PH7_FMT_BUFSIZ-40;
        if( realvalue<0.0 ){
          realvalue = -realvalue;
          prefix = '-';
        }else{
          if( flag_plussign )          prefix = '+';
          else if( flag_blanksign )    prefix = ' ';
          else                         prefix = 0;
        }
        if( pInfo->type==PH7_FMT_GENERIC && precision>0 ) precision--;
        rounder = 0.0;
#if 0
        /* Rounding works like BSD when the constant 0.4999 is used.Wierd! */
        for(idx=precision, rounder=0.4999; idx>0; idx--, rounder*=0.1);
#else
        /* It makes more sense to use 0.5 */
        for(idx=precision, rounder=0.5; idx>0; idx--, rounder*=0.1);
#endif
        if( pInfo->type==PH7_FMT_FLOAT ) realvalue += rounder;
        /* Normalize realvalue to within 10.0 > realvalue >= 1.0 */
        exp = 0;
        if( realvalue>0.0 ){
          while( realvalue>=1e8 && exp<=350 ){ realvalue *= 1e-8; exp+=8; }
          while( realvalue>=10.0 && exp<=350 ){ realvalue *= 0.1; exp++; }
          while( realvalue<1e-8 && exp>=-350 ){ realvalue *= 1e8; exp-=8; }
          while( realvalue<1.0 && exp>=-350 ){ realvalue *= 10.0; exp--; }
          if( exp>350 || exp<-350 ){
            zBuf = "NaN";
            length = 3;
            break;
          }
        }
        zBuf = zWorker;
        /*
        ** If the field type is etGENERIC, then convert to either etEXP
        ** or etFLOAT, as appropriate.
        */
        flag_exp = xtype==PH7_FMT_EXP;
        if( xtype!=PH7_FMT_FLOAT ){
          realvalue += rounder;
          if( realvalue>=10.0 ){ realvalue *= 0.1; exp++; }
        }
        if( xtype==PH7_FMT_GENERIC ){
          flag_rtz = !flag_alternateform;
          if( exp<-4 || exp>precision ){
            xtype = PH7_FMT_EXP;
          }else{
            precision = precision - exp;
            xtype = PH7_FMT_FLOAT;
          }
        }else{
          flag_rtz = 0;
        }
        /*
        ** The "exp+precision" test causes output to be of type etEXP if
        ** the precision is too large to fit in buf[].
        */
        nsd = 0;
        if( xtype==PH7_FMT_FLOAT && exp+precision<PH7_FMT_BUFSIZ-30 ){
          flag_dp = (precision>0 || flag_alternateform);
          if( prefix ) *(zBuf++) = (char)prefix;         /* Sign */
          if( exp<0 )  *(zBuf++) = '0';            /* Digits before "." */
          else for(; exp>=0; exp--) *(zBuf++) = (char)vxGetdigit(&realvalue,&nsd);
          if( flag_dp ) *(zBuf++) = '.';           /* The decimal point */
          for(exp++; exp<0 && precision>0; precision--, exp++){
            *(zBuf++) = '0';
          }
          while( (precision--)>0 ) *(zBuf++) = (char)vxGetdigit(&realvalue,&nsd);
          *(zBuf--) = 0;                           /* Null terminate */
          if( flag_rtz && flag_dp ){     /* Remove trailing zeros and "." */
            while( zBuf>=zWorker && *zBuf=='0' ) *(zBuf--) = 0;
            if( zBuf>=zWorker && *zBuf=='.' ) *(zBuf--) = 0;
          }
          zBuf++;                            /* point to next free slot */
        }else{    /* etEXP or etGENERIC */
          flag_dp = (precision>0 || flag_alternateform);
          if( prefix ) *(zBuf++) = (char)prefix;   /* Sign */
          *(zBuf++) = (char)vxGetdigit(&realvalue,&nsd);  /* First digit */
          if( flag_dp ) *(zBuf++) = '.';     /* Decimal point */
          while( (precision--)>0 ) *(zBuf++) = (char)vxGetdigit(&realvalue,&nsd);
          zBuf--;                            /* point to last digit */
          if( flag_rtz && flag_dp ){          /* Remove tail zeros */
            while( zBuf>=zWorker && *zBuf=='0' ) *(zBuf--) = 0;
            if( zBuf>=zWorker && *zBuf=='.' ) *(zBuf--) = 0;
          }
          zBuf++;                            /* point to next free slot */
          if( exp || flag_exp ){
            *(zBuf++) = pInfo->charset[0];
            if( exp<0 ){ *(zBuf++) = '-'; exp = -exp; } /* sign of exp */
            else       { *(zBuf++) = '+'; }
            if( exp>=100 ){
              *(zBuf++) = (char)((exp/100)+'0');                /* 100's digit */
              exp %= 100;
            }
            *(zBuf++) = (char)(exp/10+'0');                     /* 10's digit */
            *(zBuf++) = (char)(exp%10+'0');                     /* 1's digit */
          }
        }
        /* The converted number is in buf[] and zero terminated.Output it.
        ** Note that the number is in the usual order, not reversed as with
        ** integer conversions.*/
        length = (int)(zBuf-zWorker);
        zBuf = zWorker;
        /* Special case:  Add leading zeros if the flag_zeropad flag is
        ** set and we are not left justified */
        if( flag_zeropad && !flag_leftjustify && length < width){
          int i;
          int nPad = width - length;
          for(i=width; i>=nPad; i--){
            zBuf[i] = zBuf[i-nPad];
          }
          i = prefix!=0;
          while( nPad-- ) zBuf[i++] = '0';
          length = width;
        }
#else
         zBuf = " ";
		 length = (int)sizeof(char);
#endif /* PH7_OMIT_FLOATING_POINT */
		 break;
							 }
		default:
			/* Invalid format specifer */
			zWorker[0] = '?';
			length = (int)sizeof(char);
			break;
		}
		 /*
		 ** The text of the conversion is pointed to by "zBuf" and is
		 ** "length" characters long.The field width is "width".Do
		 ** the output.
		 */
    if( !flag_leftjustify ){
      register int nspace;
      nspace = width-length;
      if( nspace>0 ){
        while( nspace>=etSPACESIZE ){
			rc = xConsumer(pCtx,spaces,etSPACESIZE,pUserData);
			if( rc != SXRET_OK ){
				return SXERR_ABORT; /* Consumer routine request an operation abort */
			}
			nspace -= etSPACESIZE;
        }
        if( nspace>0 ){
			rc = xConsumer(pCtx,spaces,(unsigned int)nspace,pUserData);
			if( rc != SXRET_OK ){
				return SXERR_ABORT; /* Consumer routine request an operation abort */
			}
		}
      }
    }
    if( length>0 ){
		rc = xConsumer(pCtx,zBuf,(unsigned int)length,pUserData);
		if( rc != SXRET_OK ){
		  return SXERR_ABORT; /* Consumer routine request an operation abort */
		}
    }
    if( flag_leftjustify ){
      register int nspace;
      nspace = width-length;
      if( nspace>0 ){
        while( nspace>=etSPACESIZE ){
			rc = xConsumer(pCtx,spaces,etSPACESIZE,pUserData);
			if( rc != SXRET_OK ){
				return SXERR_ABORT; /* Consumer routine request an operation abort */
			}
			nspace -= etSPACESIZE;
        }
        if( nspace>0 ){
			rc = xConsumer(pCtx,spaces,(unsigned int)nspace,pUserData);
			if( rc != SXRET_OK ){
				return SXERR_ABORT; /* Consumer routine request an operation abort */
			}
		}
      }
    }
 }/* for(;;) */
	return SXRET_OK;
}
/*
 * Callback [i.e: Formatted input consumer] of the sprintf function.
 */
static int sprintfConsumer(ph7_context *pCtx,const char *zInput,int nLen,void *pUserData)
{
	/* Consume directly */
	ph7_result_string(pCtx,zInput,nLen);
	SXUNUSED(pUserData); /* cc warning */
	return PH7_OK;
}
/*
 * string sprintf(string $format[,mixed $args [, mixed $... ]])
 *  Return a formatted string.
 * Parameters
 *  $format 
 *    The format string (see block comment above)
 * Return
 *  A string produced according to the formatting string format. 
 */
static int PH7_builtin_sprintf(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	const char *zFormat;
	int nLen;
	if( nArg < 1 || !ph7_value_is_string(apArg[0]) ){
		/* Missing/Invalid arguments,return the empty string */
		ph7_result_string(pCtx,"",0);
		return PH7_OK;
	}
	/* Extract the string format */
	zFormat = ph7_value_to_string(apArg[0],&nLen);
	if( nLen < 1 ){
		/* Empty string */
		ph7_result_string(pCtx,"",0);
		return PH7_OK;
	}
	/* Format the string */
	PH7_InputFormat(sprintfConsumer,pCtx,zFormat,nLen,nArg,apArg,0,FALSE);
	return PH7_OK;
}
/*
 * Callback [i.e: Formatted input consumer] of the printf function.
 */
static int printfConsumer(ph7_context *pCtx,const char *zInput,int nLen,void *pUserData)
{
	ph7_int64 *pCounter = (ph7_int64 *)pUserData;
	/* Call the VM output consumer directly */
	ph7_context_output(pCtx,zInput,nLen);
	/* Increment counter */
	*pCounter += nLen;
	return PH7_OK;
}
/*
 * int64 printf(string $format[,mixed $args[,mixed $... ]])
 *  Output a formatted string.
 * Parameters
 *  $format
 *   See sprintf() for a description of format.
 * Return
 *  The length of the outputted string.
 */
static int PH7_builtin_printf(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	ph7_int64 nCounter = 0;
	const char *zFormat;
	int nLen;
	if( nArg < 1 || !ph7_value_is_string(apArg[0]) ){
		/* Missing/Invalid arguments,return 0 */
		ph7_result_int(pCtx,0);
		return PH7_OK;
	}
	/* Extract the string format */
	zFormat = ph7_value_to_string(apArg[0],&nLen);
	if( nLen < 1 ){
		/* Empty string */
		ph7_result_int(pCtx,0);
		return PH7_OK;
	}
	/* Format the string */
	PH7_InputFormat(printfConsumer,pCtx,zFormat,nLen,nArg,apArg,(void *)&nCounter,FALSE);
	/* Return the length of the outputted string */
	ph7_result_int64(pCtx,nCounter);
	return PH7_OK;
}
/*
 * int vprintf(string $format,array $args)
 *  Output a formatted string.
 * Parameters
 *  $format
 *   See sprintf() for a description of format.
 * Return
 *  The length of the outputted string.
 */
static int PH7_builtin_vprintf(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	ph7_int64 nCounter = 0;
	const char *zFormat;
	ph7_hashmap *pMap;
	SySet sArg;
	int nLen,n;
	if( nArg < 2 || !ph7_value_is_string(apArg[0]) || !ph7_value_is_array(apArg[1]) ){
		/* Missing/Invalid arguments,return 0 */
		ph7_result_int(pCtx,0);
		return PH7_OK;
	}
	/* Extract the string format */
	zFormat = ph7_value_to_string(apArg[0],&nLen);
	if( nLen < 1 ){
		/* Empty string */
		ph7_result_int(pCtx,0);
		return PH7_OK;
	}
	/* Point to the hashmap */
	pMap = (ph7_hashmap *)apArg[1]->x.pOther;
	/* Extract arguments from the hashmap */
	n = PH7_HashmapValuesToSet(pMap,&sArg);
	/* Format the string */
	PH7_InputFormat(printfConsumer,pCtx,zFormat,nLen,n,(ph7_value **)SySetBasePtr(&sArg),(void *)&nCounter,TRUE);
	/* Return the length of the outputted string */
	ph7_result_int64(pCtx,nCounter);
	/* Release the container */
	SySetRelease(&sArg);
	return PH7_OK;
}
/*
 * int vsprintf(string $format,array $args)
 *  Output a formatted string.
 * Parameters
 *  $format
 *   See sprintf() for a description of format.
 * Return
 *  A string produced according to the formatting string format.
 */
static int PH7_builtin_vsprintf(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	const char *zFormat;
	ph7_hashmap *pMap;
	SySet sArg;
	int nLen,n;
	if( nArg < 2 || !ph7_value_is_string(apArg[0]) || !ph7_value_is_array(apArg[1]) ){
		/* Missing/Invalid arguments,return the empty string */
		ph7_result_string(pCtx,"",0);
		return PH7_OK;
	}
	/* Extract the string format */
	zFormat = ph7_value_to_string(apArg[0],&nLen);
	if( nLen < 1 ){
		/* Empty string */
		ph7_result_string(pCtx,"",0);
		return PH7_OK;
	}
	/* Point to hashmap */
	pMap = (ph7_hashmap *)apArg[1]->x.pOther;
	/* Extract arguments from the hashmap */
	n = PH7_HashmapValuesToSet(pMap,&sArg);
	/* Format the string */
	PH7_InputFormat(sprintfConsumer,pCtx,zFormat,nLen,n,(ph7_value **)SySetBasePtr(&sArg),0,TRUE);
	/* Release the container */
	SySetRelease(&sArg);
	return PH7_OK;
}
/*
 * Symisc eXtension.
 * string size_format(int64 $size)
 *  Return a smart string represenation of the given size [i.e: 64-bit integer]
 *  Example:
 *    echo size_format(1*1024*1024*1024);// 1GB
 *    echo size_format(512*1024*1024); // 512 MB
 *    echo size_format(file_size(/path/to/my/file_8192)); //8KB
 * Parameter
 *  $size
 *    Entity size in bytes.
 * Return
 *   Formatted string representation of the given size.
 */
static int PH7_builtin_size_format(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	/*Kilo*/ /*Mega*/ /*Giga*/ /*Tera*/ /*Peta*/ /*Exa*/ /*Zeta*/
	static const char zUnit[] = {"KMGTPEZ"};
	sxi32 nRest,i_32;
	ph7_int64 iSize;
	int c = -1; /* index in zUnit[] */

	if( nArg < 1 ){
		/* Missing argument,return the empty string */
		ph7_result_string(pCtx,"",0);
		return PH7_OK;
	}
	/* Extract the given size */
	iSize = ph7_value_to_int64(apArg[0]);
	if( iSize < 100 /* Bytes */ ){
		/* Don't bother formatting,return immediately */
		ph7_result_string(pCtx,"0.1 KB",(int)sizeof("0.1 KB")-1);
		return PH7_OK;
	}
	for(;;){
		nRest = (sxi32)(iSize & 0x3FF); 
		iSize >>= 10;
		c++;
		if( (iSize & (~0 ^ 1023)) == 0 ){
			break;
		}
	}
	nRest /= 100;
	if( nRest > 9 ){
		nRest = 9;
	}
	if( iSize > 999 ){
		c++;
		nRest = 9;
		iSize = 0;
	}
	i_32 = (sxi32)iSize;
	/* Format */
	ph7_result_string_format(pCtx,"%d.%d %cB",i_32,nRest,zUnit[c]);
	return PH7_OK;
}
#if !defined(PH7_DISABLE_HASH_FUNC)
/*
 * string md5(string $str[,bool $raw_output = false])
 *   Calculate the md5 hash of a string.
 * Parameter
 *  $str
 *   Input string
 * $raw_output
 *   If the optional raw_output is set to TRUE, then the md5 digest
 *   is instead returned in raw binary format with a length of 16.
 * Return
 *  MD5 Hash as a 32-character hexadecimal string.
 */
static int PH7_builtin_md5(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	unsigned char zDigest[16];
	int raw_output = FALSE;
	const void *pIn;
	int nLen;
	if( nArg < 1 ){
		/* Missing arguments,return the empty string */
		ph7_result_string(pCtx,"",0);
		return PH7_OK;
	}
	/* Extract the input string */
	pIn = (const void *)ph7_value_to_string(apArg[0],&nLen);
	if( nLen < 1 ){
		/* Empty string */
		ph7_result_string(pCtx,"",0);
		return PH7_OK;
	}
	if( nArg > 1 && ph7_value_is_bool(apArg[1])){
		raw_output = ph7_value_to_bool(apArg[1]);
	}
	/* Compute the MD5 digest */
	SyMD5Compute(pIn,(sxu32)nLen,zDigest);
	if( raw_output ){
		/* Output raw digest */
		ph7_result_string(pCtx,(const char *)zDigest,(int)sizeof(zDigest));
	}else{
		/* Perform a binary to hex conversion */
		SyBinToHexConsumer((const void *)zDigest,sizeof(zDigest),HashConsumer,pCtx);
	}
	return PH7_OK;
}
/*
 * string sha1(string $str[,bool $raw_output = false])
 *   Calculate the sha1 hash of a string.
 * Parameter
 *  $str
 *   Input string
 * $raw_output
 *   If the optional raw_output is set to TRUE, then the md5 digest
 *   is instead returned in raw binary format with a length of 16.
 * Return
 *  SHA1 Hash as a 40-character hexadecimal string.
 */
static int PH7_builtin_sha1(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	unsigned char zDigest[20];
	int raw_output = FALSE;
	const void *pIn;
	int nLen;
	if( nArg < 1 ){
		/* Missing arguments,return the empty string */
		ph7_result_string(pCtx,"",0);
		return PH7_OK;
	}
	/* Extract the input string */
	pIn = (const void *)ph7_value_to_string(apArg[0],&nLen);
	if( nLen < 1 ){
		/* Empty string */
		ph7_result_string(pCtx,"",0);
		return PH7_OK;
	}
	if( nArg > 1 && ph7_value_is_bool(apArg[1])){
		raw_output = ph7_value_to_bool(apArg[1]);
	}
	/* Compute the SHA1 digest */
	SySha1Compute(pIn,(sxu32)nLen,zDigest);
	if( raw_output ){
		/* Output raw digest */
		ph7_result_string(pCtx,(const char *)zDigest,(int)sizeof(zDigest));
	}else{
		/* Perform a binary to hex conversion */
		SyBinToHexConsumer((const void *)zDigest,sizeof(zDigest),HashConsumer,pCtx);
	}
	return PH7_OK;
}
/*
 * int64 crc32(string $str)
 *   Calculates the crc32 polynomial of a strin.
 * Parameter
 *  $str
 *   Input string
 * Return
 *  CRC32 checksum of the given input (64-bit integer).
 */
static int PH7_builtin_crc32(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	const void *pIn;
	sxu32 nCRC;
	int nLen;
	if( nArg < 1 ){
		/* Missing arguments,return 0 */
		ph7_result_int(pCtx,0);
		return PH7_OK;
	}
	/* Extract the input string */
	pIn = (const void *)ph7_value_to_string(apArg[0],&nLen);
	if( nLen < 1 ){
		/* Empty string */
		ph7_result_int(pCtx,0);
		return PH7_OK;
	}
	/* Calculate the sum */
	nCRC = SyCrc32(pIn,(sxu32)nLen);
	/* Return the CRC32 as 64-bit integer */
	ph7_result_int64(pCtx,(ph7_int64)nCRC^ 0xFFFFFFFF);
	return PH7_OK;
}
#endif /* PH7_DISABLE_HASH_FUNC */
/*
 * Parse a CSV string and invoke the supplied callback for each processed xhunk.
 */
PH7_PRIVATE sxi32 PH7_ProcessCsv(
	const char *zInput, /* Raw input */
	int nByte,  /* Input length */
	int delim,  /* Delimiter */
	int encl,   /* Enclosure */
	int escape,  /* Escape character */
	sxi32 (*xConsumer)(const char *,int,void *), /* User callback */
	void *pUserData /* Last argument to xConsumer() */
	)
{
	const char *zEnd = &zInput[nByte];
	const char *zIn = zInput;
	const char *zPtr;
	int isEnc;
	/* Start processing */
	for(;;){
		if( zIn >= zEnd ){
			/* No more input to process */
			break;
		}
		isEnc = 0;
		zPtr = zIn;
		/* Find the first delimiter */
		while( zIn < zEnd ){
			if( zIn[0] == delim && !isEnc){
				/* Delimiter found,break imediately */
				break;
			}else if( zIn[0] == encl ){
				/* Inside enclosure? */
				isEnc = !isEnc;
			}else if( zIn[0] == escape ){
				/* Escape sequence */
				zIn++;
			}
			/* Advance the cursor */
			zIn++;
		}
		if( zIn > zPtr ){
			int nByte = (int)(zIn-zPtr);
			sxi32 rc;
			/* Invoke the supllied callback */
			if( zPtr[0] == encl ){
				zPtr++;
				nByte-=2;
			}
			if( nByte > 0 ){
				rc = xConsumer(zPtr,nByte,pUserData);
				if( rc == SXERR_ABORT ){
					/* User callback request an operation abort */
					break;
				}
			}
		}
		/* Ignore trailing delimiter */
		while( zIn < zEnd && zIn[0] == delim ){
			zIn++;
		}
	}
	return SXRET_OK;
}
/*
 * Default consumer callback for the CSV parsing routine defined above.
 * All the processed input is insereted into an array passed as the last
 * argument to this callback.
 */
PH7_PRIVATE sxi32 PH7_CsvConsumer(const char *zToken,int nTokenLen,void *pUserData)
{
	ph7_value *pArray = (ph7_value *)pUserData;
	ph7_value sEntry;
	SyString sToken;
	/* Insert the token in the given array */
	SyStringInitFromBuf(&sToken,zToken,nTokenLen);
	/* Remove trailing and leading white spcaces and null bytes */
	SyStringFullTrimSafe(&sToken);
	if( sToken.nByte < 1){
		return SXRET_OK;
	}
	PH7_MemObjInitFromString(pArray->pVm,&sEntry,&sToken);
	ph7_array_add_elem(pArray,0,&sEntry);
	PH7_MemObjRelease(&sEntry);
	return SXRET_OK;
}
/*
 * array str_getcsv(string $input[,string $delimiter = ','[,string $enclosure = '"' [,string $escape='\\']]])
 *  Parse a CSV string into an array.
 * Parameters
 *  $input
 *   The string to parse.
 *  $delimiter
 *   Set the field delimiter (one character only).
 *  $enclosure
 *   Set the field enclosure character (one character only).
 *  $escape
 *   Set the escape character (one character only). Defaults as a backslash (\)
 * Return
 *  An indexed array containing the CSV fields or NULL on failure.
 */
static int PH7_builtin_str_getcsv(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	const char *zInput,*zPtr;
	ph7_value *pArray;
	int delim  = ',';   /* Delimiter */
	int encl   = '"' ;  /* Enclosure */
	int escape = '\\';  /* Escape character */
	int nLen;
	if( nArg < 1 || !ph7_value_is_string(apArg[0]) ){
		/* Missing/Invalid arguments,return NULL */
		ph7_result_null(pCtx);
		return PH7_OK;
	}
	/* Extract the raw input */
	zInput = ph7_value_to_string(apArg[0],&nLen);
	if( nArg > 1 ){
		int i;
		if( ph7_value_is_string(apArg[1]) ){
			/* Extract the delimiter */
			zPtr = ph7_value_to_string(apArg[1],&i);
			if( i > 0 ){
				delim = zPtr[0];
			}
		}
		if( nArg > 2 ){
			if( ph7_value_is_string(apArg[2]) ){
				/* Extract the enclosure */
				zPtr = ph7_value_to_string(apArg[2],&i);
				if( i > 0 ){
					encl = zPtr[0];
				}
			}
			if( nArg > 3 ){
				if( ph7_value_is_string(apArg[3]) ){
					/* Extract the escape character */
					zPtr = ph7_value_to_string(apArg[3],&i);
					if( i > 0 ){
						escape = zPtr[0];
					}
				}
			}
		}
	}
	/* Create our array */
	pArray = ph7_context_new_array(pCtx);
	if( pArray == 0 ){
		ph7_context_throw_error(pCtx,PH7_CTX_ERR,"PH7 is running out of memory");
		ph7_result_null(pCtx);
		return PH7_OK;
	}
	/* Parse the raw input */
	PH7_ProcessCsv(zInput,nLen,delim,encl,escape,PH7_CsvConsumer,pArray);
	/* Return the freshly created array */
	ph7_result_value(pCtx,pArray);
	return PH7_OK;
}
/*
 * Extract a tag name from a raw HTML input and insert it in the given
 * container.
 * Refer to [strip_tags()].
 */
static sxi32 AddTag(SySet *pSet,const char *zTag,int nByte)
{
	const char *zEnd = &zTag[nByte];
	const char *zPtr;
	SyString sEntry;
	/* Strip tags */
	for(;;){
		while( zTag < zEnd && (zTag[0] == '<' || zTag[0] == '/' || zTag[0] == '?'
			|| zTag[0] == '!' || zTag[0] == '-' || ((unsigned char)zTag[0] < 0xc0 && SyisSpace(zTag[0]))) ){
				zTag++;
		}
		if( zTag >= zEnd ){
			break;
		}
		zPtr = zTag;
		/* Delimit the tag */
		while(zTag < zEnd ){
			if( (unsigned char)zTag[0] >= 0xc0 ){
				/* UTF-8 stream */
				zTag++;
				SX_JMP_UTF8(zTag,zEnd);
			}else if( !SyisAlphaNum(zTag[0]) ){
				break;
			}else{
				zTag++;
			}
		}
		if( zTag > zPtr ){
			/* Perform the insertion */
			SyStringInitFromBuf(&sEntry,zPtr,(int)(zTag-zPtr));
			SyStringFullTrim(&sEntry);
			SySetPut(pSet,(const void *)&sEntry);
		}
		/* Jump the trailing '>' */
		zTag++;
	}
	return SXRET_OK;
}
/*
 * Check if the given HTML tag name is present in the given container.
 * Return SXRET_OK if present.SXERR_NOTFOUND otherwise.
 * Refer to [strip_tags()].
 */
static sxi32 FindTag(SySet *pSet,const char *zTag,int nByte)
{
	if( SySetUsed(pSet) > 0 ){
		const char *zCur,*zEnd = &zTag[nByte];
		SyString sTag;
		while( zTag < zEnd &&  (zTag[0] == '<' || zTag[0] == '/' || zTag[0] == '?' ||
			((unsigned char)zTag[0] < 0xc0 && SyisSpace(zTag[0]))) ){
			zTag++;
		}
		/* Delimit the tag */
		zCur = zTag;
		while(zTag < zEnd ){
			if( (unsigned char)zTag[0] >= 0xc0 ){
				/* UTF-8 stream */
				zTag++;
				SX_JMP_UTF8(zTag,zEnd);
			}else if( !SyisAlphaNum(zTag[0]) ){
				break;
			}else{
				zTag++;
			}
		}
		SyStringInitFromBuf(&sTag,zCur,zTag-zCur);
		/* Trim leading white spaces and null bytes */
		SyStringLeftTrimSafe(&sTag);
		if( sTag.nByte > 0 ){
			SyString *aEntry,*pEntry;
			sxi32 rc;
			sxu32 n;
			/* Perform the lookup */
			aEntry = (SyString *)SySetBasePtr(pSet);
			for( n = 0 ; n < SySetUsed(pSet) ; ++n ){
				pEntry = &aEntry[n];
				/* Do the comparison */
				rc = SyStringCmp(pEntry,&sTag,SyStrnicmp);
				if( !rc ){
					return SXRET_OK;
				}
			}
		}
	}
	/* No such tag */
	return SXERR_NOTFOUND;
}
/*
 * This function tries to return a string [i.e: in the call context result buffer]
 * with all NUL bytes,HTML and PHP tags stripped from a given string.
 * Refer to [strip_tags()].
 */
PH7_PRIVATE sxi32 PH7_StripTagsFromString(ph7_context *pCtx,const char *zIn,int nByte,const char *zTaglist,int nTaglen)
{
	const char *zEnd = &zIn[nByte];
	const char *zPtr,*zTag;
	SySet sSet;
	/* initialize the set of allowed tags */
	SySetInit(&sSet,&pCtx->pVm->sAllocator,sizeof(SyString));
	if( nTaglen > 0 ){
		/* Set of allowed tags */
		AddTag(&sSet,zTaglist,nTaglen);
	}
	/* Set the empty string */
	ph7_result_string(pCtx,"",0);
	/* Start processing */
	for(;;){
		if(zIn >= zEnd){
			/* No more input to process */
			break;
		}
		zPtr = zIn;
		/* Find a tag */
		while( zIn < zEnd && zIn[0] != '<' && zIn[0] != 0 /* NUL byte */ ){
			zIn++;
		}
		if( zIn > zPtr ){
			/* Consume raw input */
			ph7_result_string(pCtx,zPtr,(int)(zIn-zPtr));
		}
		/* Ignore trailing null bytes */
		while( zIn < zEnd && zIn[0] == 0 ){
			zIn++;
		}
		if(zIn >= zEnd){
			/* No more input to process */
			break;
		}
		if( zIn[0] == '<' ){
			sxi32 rc;
			zTag = zIn++;
			/* Delimit the tag */
			while( zIn < zEnd && zIn[0] != '>' ){
				zIn++;
			}
			if( zIn < zEnd ){
				zIn++; /* Ignore the trailing closing tag */
			}
			/* Query the set */
			rc = FindTag(&sSet,zTag,(int)(zIn-zTag));
			if( rc == SXRET_OK ){
				/* Keep the tag */
				ph7_result_string(pCtx,zTag,(int)(zIn-zTag));
			}
		}
	}
	/* Cleanup */
	SySetRelease(&sSet);
	return SXRET_OK;
}
/*
 * string strip_tags(string $str[,string $allowable_tags])
 *   Strip HTML and PHP tags from a string.
 * Parameters
 *  $str
 *  The input string.
 * $allowable_tags
 *  You can use the optional second parameter to specify tags which should not be stripped. 
 * Return
 *  Returns the stripped string.
 */
static int PH7_builtin_strip_tags(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	const char *zTaglist = 0;
	const char *zString;
	int nTaglen = 0;
	int nLen;
	if( nArg < 1 || !ph7_value_is_string(apArg[0]) ){
		/* Missing/Invalid arguments,return the empty string */
		ph7_result_string(pCtx,"",0);
		return PH7_OK;
	}
	/* Point to the raw string */
	zString = ph7_value_to_string(apArg[0],&nLen);
	if( nArg > 1 && ph7_value_is_string(apArg[1]) ){
		/* Allowed tag */
		zTaglist = ph7_value_to_string(apArg[1],&nTaglen);		
	}
	/* Process input */
	PH7_StripTagsFromString(pCtx,zString,nLen,zTaglist,nTaglen);
	return PH7_OK;
}
/*
 * string str_shuffle(string $str)
 *  Randomly shuffles a string.
 * Parameters
 *  $str
 *   The input string.
 * Return
 *  Returns the shuffled string.
 */
static int PH7_builtin_str_shuffle(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	const char *zString;
	int nLen,i,c;
	sxu32 iR;
	if( nArg < 1 ){
		/* Missing arguments,return the empty string */
		ph7_result_string(pCtx,"",0);
		return PH7_OK;
	}
	/* Extract the target string */
	zString = ph7_value_to_string(apArg[0],&nLen);
	if( nLen < 1 ){
		/* Nothing to shuffle */
		ph7_result_string(pCtx,"",0);
		return PH7_OK;
	}
	/* Shuffle the string */
	for( i = 0 ; i < nLen ; ++i ){
		/* Generate a random number first */
		iR = ph7_context_random_num(pCtx);
		/* Extract a random offset */
		c = zString[iR % nLen];
		/* Append it */
		ph7_result_string(pCtx,(const char *)&c,(int)sizeof(char));
	}
	return PH7_OK;
}
/*
 * array str_split(string $string[,int $split_length = 1 ])
 *  Convert a string to an array.
 * Parameters
 * $str
 *  The input string.
 * $split_length
 *  Maximum length of the chunk.
 * Return
 *  If the optional split_length parameter is specified, the returned array
 *  will be broken down into chunks with each being split_length in length, otherwise
 *  each chunk will be one character in length. FALSE is returned if split_length is less than 1.
 *  If the split_length length exceeds the length of string, the entire string is returned 
 *  as the first (and only) array element.
 */
static int PH7_builtin_str_split(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	const char *zString,*zEnd;
	ph7_value *pArray,*pValue;
	int split_len;
	int nLen;
	if( nArg < 1 ){
		/* Missing arguments,return FALSE */
		ph7_result_bool(pCtx,0);
		return PH7_OK;
	}
	/* Point to the target string */
	zString = ph7_value_to_string(apArg[0],&nLen);
	if( nLen < 1 ){
		/* Nothing to process,return FALSE */
		ph7_result_bool(pCtx,0);
		return PH7_OK;
	}
	split_len = (int)sizeof(char);
	if( nArg > 1 ){
		/* Split length */
		split_len = ph7_value_to_int(apArg[1]);
		if( split_len < 1 ){
			/* Invalid length,return FALSE */
			ph7_result_bool(pCtx,0);
			return PH7_OK;
		}
		if( split_len > nLen ){
			split_len = nLen;
		}
	}
	/* Create the array and the scalar value */
	pArray = ph7_context_new_array(pCtx);
	/*Chunk value */
	pValue = ph7_context_new_scalar(pCtx);
	if( pValue == 0 || pArray == 0 ){
		/* Return FALSE */
		ph7_result_bool(pCtx,0);
		return PH7_OK;
	}
	/* Point to the end of the string */
	zEnd = &zString[nLen];
	/* Perform the requested operation */
	for(;;){
		int nMax;
		if( zString >= zEnd ){
			/* No more input to process */
			break;
		}
		nMax = (int)(zEnd-zString);
		if( nMax < split_len ){
			split_len = nMax;
		}
		/* Copy the current chunk */
		ph7_value_string(pValue,zString,split_len);
		/* Insert it */
		ph7_array_add_elem(pArray,0,pValue); /* Will make it's own copy */
		/* reset the string cursor */
		ph7_value_reset_string_cursor(pValue);
		/* Update position */
		zString += split_len;
	}
	/* 
	 * Return the array.
	 * Don't worry about freeing memory, everything will be automatically released
	 * upon we return from this function.
	 */
	ph7_result_value(pCtx,pArray);
	return PH7_OK;
}
/*
 * Tokenize a raw string and extract the first non-space token.
 * Refer to [strspn()].
 */
static sxi32 ExtractNonSpaceToken(const char **pzIn,const char *zEnd,SyString *pOut)
{
	const char *zIn = *pzIn;
	const char *zPtr;
	/* Ignore leading white spaces */
	while( zIn < zEnd && (unsigned char)zIn[0] < 0xc0 && SyisSpace(zIn[0]) ){
		zIn++;
	}
	if( zIn >= zEnd ){
		/* End of input */
		return SXERR_EOF;
	}
	zPtr = zIn;
	/* Extract the token */
	while( zIn < zEnd && (unsigned char)zIn[0] < 0xc0 && !SyisSpace(zIn[0]) ){
		zIn++;
	}
	SyStringInitFromBuf(pOut,zPtr,zIn-zPtr);
	/* Synchronize pointers */
	*pzIn = zIn;
	/* Return to the caller */
	return SXRET_OK;
}
/*
 * Check if the given string contains only characters from the given mask.
 * return the longest match.
 * Refer to [strspn()].
 */
static int LongestStringMask(const char *zString,int nLen,const char *zMask,int nMaskLen)
{
	const char *zEnd = &zString[nLen];
	const char *zIn = zString;
	int i,c;
	for(;;){
		if( zString >= zEnd ){
			break;
		}
		/* Extract current character */
		c = zString[0];
		/* Perform the lookup */
		for( i = 0 ; i < nMaskLen ; i++ ){
			if( c == zMask[i] ){
				/* Character found */
				break;
			}
		}
		if( i >= nMaskLen ){
			/* Character not in the current mask,break immediately */
			break;
		}
		/* Advance cursor */
		zString++;
	}
	/* Longest match */
	return (int)(zString-zIn);
}
/*
 * Do the reverse operation of the previous function [i.e: LongestStringMask()].
 * Refer to [strcspn()].
 */
static int LongestStringMask2(const char *zString,int nLen,const char *zMask,int nMaskLen)
{
	const char *zEnd = &zString[nLen];
	const char *zIn = zString;
	int i,c;
	for(;;){
		if( zString >= zEnd ){
			break;
		}
		/* Extract current character */
		c = zString[0];
		/* Perform the lookup */
		for( i = 0 ; i < nMaskLen ; i++ ){
			if( c == zMask[i] ){
				break;
			}
		}
		if( i < nMaskLen ){
			/* Character in the current mask,break immediately */
			break;
		}
		/* Advance cursor */
		zString++;
	}
	/* Longest match */
	return (int)(zString-zIn);
}
/*
 * int strspn(string $str,string $mask[,int $start[,int $length]])
 *  Finds the length of the initial segment of a string consisting entirely
 *  of characters contained within a given mask.
 * Parameters
 * $str
 *  The input string.
 * $mask
 *  The list of allowable characters.
 * $start
 *  The position in subject to start searching.
 *  If start is given and is non-negative, then strspn() will begin examining 
 *  subject at the start'th position. For instance, in the string 'abcdef', the character
 *  at position 0 is 'a', the character at position 2 is 'c', and so forth.
 *  If start is given and is negative, then strspn() will begin examining subject at the
 *  start'th position from the end of subject.
 * $length
 *  The length of the segment from subject to examine.
 *  If length is given and is non-negative, then subject will be examined for length
 *  characters after the starting position.
 *  If lengthis given and is negative, then subject will be examined from the starting
 *  position up to length characters from the end of subject.
 * Return
 * Returns the length of the initial segment of subject which consists entirely of characters
 * in mask.
 */
static int PH7_builtin_strspn(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	const char *zString,*zMask,*zEnd;
	int iMasklen,iLen;
	SyString sToken;
	int iCount = 0;
	int rc;
	if( nArg < 2 ){
		/* Missing agruments,return zero */
		ph7_result_int(pCtx,0);
		return PH7_OK;
	}
	/* Extract the target string */
	zString = ph7_value_to_string(apArg[0],&iLen);
	/* Extract the mask */
	zMask = ph7_value_to_string(apArg[1],&iMasklen);
	if( iLen < 1 || iMasklen < 1 ){
		/* Nothing to process,return zero */
		ph7_result_int(pCtx,0);
		return PH7_OK;
	}
	if( nArg > 2 ){
		int nOfft;
		/* Extract the offset */
		nOfft = ph7_value_to_int(apArg[2]);
		if( nOfft < 0 ){
			const char *zBase = &zString[iLen + nOfft];
			if( zBase > zString ){
				iLen = (int)(&zString[iLen]-zBase);
				zString = zBase;	
			}else{
				/* Invalid offset */
				ph7_result_int(pCtx,0);
				return PH7_OK;
			}
		}else{
			if( nOfft >= iLen ){
				/* Invalid offset */
				ph7_result_int(pCtx,0);
				return PH7_OK;
			}else{
				/* Update offset */
				zString += nOfft;
				iLen -= nOfft;
			}
		}
		if( nArg > 3 ){
			int iUserlen;
			/* Extract the desired length */
			iUserlen = ph7_value_to_int(apArg[3]);
			if( iUserlen > 0 && iUserlen < iLen ){
				iLen = iUserlen;
			}
		}
	}
	/* Point to the end of the string */
	zEnd = &zString[iLen];
	/* Extract the first non-space token */
	rc = ExtractNonSpaceToken(&zString,zEnd,&sToken);
	if( rc == SXRET_OK && sToken.nByte > 0 ){
		/* Compare against the current mask */
		iCount = LongestStringMask(sToken.zString,(int)sToken.nByte,zMask,iMasklen);
	}
	/* Longest match */
	ph7_result_int(pCtx,iCount);
	return PH7_OK;
}
/*
 * int strcspn(string $str,string $mask[,int $start[,int $length]])
 *  Find length of initial segment not matching mask.
 * Parameters
 * $str
 *  The input string.
 * $mask
 *  The list of not allowed characters.
 * $start
 *  The position in subject to start searching.
 *  If start is given and is non-negative, then strspn() will begin examining 
 *  subject at the start'th position. For instance, in the string 'abcdef', the character
 *  at position 0 is 'a', the character at position 2 is 'c', and so forth.
 *  If start is given and is negative, then strspn() will begin examining subject at the
 *  start'th position from the end of subject.
 * $length
 *  The length of the segment from subject to examine.
 *  If length is given and is non-negative, then subject will be examined for length
 *  characters after the starting position.
 *  If lengthis given and is negative, then subject will be examined from the starting
 *  position up to length characters from the end of subject.
 * Return
 *  Returns the length of the segment as an integer.
 */
static int PH7_builtin_strcspn(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	const char *zString,*zMask,*zEnd;
	int iMasklen,iLen;
	SyString sToken;
	int iCount = 0;
	int rc;
	if( nArg < 2 ){
		/* Missing agruments,return zero */
		ph7_result_int(pCtx,0);
		return PH7_OK;
	}
	/* Extract the target string */
	zString = ph7_value_to_string(apArg[0],&iLen);
	/* Extract the mask */
	zMask = ph7_value_to_string(apArg[1],&iMasklen);
	if( iLen < 1 ){
		/* Nothing to process,return zero */
		ph7_result_int(pCtx,0);
		return PH7_OK;
	}
	if( iMasklen < 1 ){
		/* No given mask,return the string length */
		ph7_result_int(pCtx,iLen);
		return PH7_OK;
	}
	if( nArg > 2 ){
		int nOfft;
		/* Extract the offset */
		nOfft = ph7_value_to_int(apArg[2]);
		if( nOfft < 0 ){
			const char *zBase = &zString[iLen + nOfft];
			if( zBase > zString ){
				iLen = (int)(&zString[iLen]-zBase);
				zString = zBase;	
			}else{
				/* Invalid offset */
				ph7_result_int(pCtx,0);
				return PH7_OK;
			}
		}else{
			if( nOfft >= iLen ){
				/* Invalid offset */
				ph7_result_int(pCtx,0);
				return PH7_OK;
			}else{
				/* Update offset */
				zString += nOfft;
				iLen -= nOfft;
			}
		}
		if( nArg > 3 ){
			int iUserlen;
			/* Extract the desired length */
			iUserlen = ph7_value_to_int(apArg[3]);
			if( iUserlen > 0 && iUserlen < iLen ){
				iLen = iUserlen;
			}
		}
	}
	/* Point to the end of the string */
	zEnd = &zString[iLen];
	/* Extract the first non-space token */
	rc = ExtractNonSpaceToken(&zString,zEnd,&sToken);
	if( rc == SXRET_OK && sToken.nByte > 0 ){
		/* Compare against the current mask */
		iCount = LongestStringMask2(sToken.zString,(int)sToken.nByte,zMask,iMasklen);
	}
	/* Longest match */
	ph7_result_int(pCtx,iCount);
	return PH7_OK;
}
/*
 * string strpbrk(string $haystack,string $char_list)
 *  Search a string for any of a set of characters.
 * Parameters
 *  $haystack
 *   The string where char_list is looked for.
 *  $char_list
 *   This parameter is case sensitive.
 * Return
 *  Returns a string starting from the character found, or FALSE if it is not found.
 */
static int PH7_builtin_strpbrk(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	const char *zString,*zList,*zEnd;
	int iLen,iListLen,i,c;
	sxu32 nOfft,nMax;
	sxi32 rc;
	if( nArg < 2 ){
		/* Missing arguments,return FALSE */
		ph7_result_bool(pCtx,0);
		return PH7_OK;
	}
	/* Extract the haystack and the char list */
	zString = ph7_value_to_string(apArg[0],&iLen);
	zList = ph7_value_to_string(apArg[1],&iListLen);
	if( iLen < 1 ){
		/* Nothing to process,return FALSE */
		ph7_result_bool(pCtx,0);
		return PH7_OK;
	}
	/* Point to the end of the string */
	zEnd = &zString[iLen];
	nOfft = nMax = SXU32_HIGH;
	/* perform the requested operation */
	for( i = 0 ; i < iListLen ; i++ ){
		c = zList[i];
		rc = SyByteFind(zString,(sxu32)iLen,c,&nMax);
		if( rc == SXRET_OK ){
			if( nMax < nOfft ){
				nOfft = nMax;
			}
		}
	}
	if( nOfft == SXU32_HIGH ){
		/* No such substring,return FALSE */
		ph7_result_bool(pCtx,0);
	}else{
		/* Return the substring */
		ph7_result_string(pCtx,&zString[nOfft],(int)(zEnd-&zString[nOfft]));
	}
	return PH7_OK;
}
/*
 * string soundex(string $str)
 *  Calculate the soundex key of a string.
 * Parameters
 *  $str
 *   The input string.
 * Return
 *  Returns the soundex key as a string.
 * Note:
 *  This implementation is based on the one found in the SQLite3
 * source tree.
 */
static int PH7_builtin_soundex(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	const unsigned char *zIn;
	char zResult[8];
	int i, j;
	static const unsigned char iCode[] = {
		0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
		0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
		0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
		0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
		0, 0, 1, 2, 3, 0, 1, 2, 0, 0, 2, 2, 4, 5, 5, 0,
		1, 2, 6, 2, 3, 0, 1, 0, 2, 0, 2, 0, 0, 0, 0, 0,
		0, 0, 1, 2, 3, 0, 1, 2, 0, 0, 2, 2, 4, 5, 5, 0,
		1, 2, 6, 2, 3, 0, 1, 0, 2, 0, 2, 0, 0, 0, 0, 0,
	};
	if( nArg < 1 ){
		/* Missing arguments,return the empty string */
		ph7_result_string(pCtx,"",0);
		return PH7_OK;
	}
	zIn = (unsigned char *)ph7_value_to_string(apArg[0],0);
	for(i=0; zIn[i] && zIn[i] < 0xc0 && !SyisAlpha(zIn[i]); i++){}
	if( zIn[i] ){
		unsigned char prevcode = iCode[zIn[i]&0x7f];
		zResult[0] = (char)SyToUpper(zIn[i]);
		for(j=1; j<4 && zIn[i]; i++){
			int code = iCode[zIn[i]&0x7f];
			if( code>0 ){
				if( code!=prevcode ){
					prevcode = (unsigned char)code;
					zResult[j++] = (char)code + '0';
				}
			}else{
				prevcode = 0;
			}
		}
		while( j<4 ){
			zResult[j++] = '0';
		}
		ph7_result_string(pCtx,zResult,4);
	}else{
	  ph7_result_string(pCtx,"?000",4);
	}
	return PH7_OK;
}
/*
 * string wordwrap(string $str[,int $width = 75[,string $break = "\n"]])
 *  Wraps a string to a given number of characters.
 * Parameters
 *  $str
 *   The input string.
 * $width
 *  The column width.
 * $break
 *  The line is broken using the optional break parameter.
 * Return
 *  Returns the given string wrapped at the specified column. 
 */
static int PH7_builtin_wordwrap(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	const char *zIn,*zEnd,*zBreak;
	int iLen,iBreaklen,iChunk;
	if( nArg < 1 ){
		/* Missing arguments,return the empty string */
		ph7_result_string(pCtx,"",0);
		return PH7_OK;
	}
	/* Extract the input string */
	zIn = ph7_value_to_string(apArg[0],&iLen);
	if( iLen < 1 ){
		/* Nothing to process,return the empty string */
		ph7_result_string(pCtx,"",0);
		return PH7_OK;
	}
	/* Chunk length */
	iChunk = 75;
	iBreaklen = 0;
	zBreak = ""; /* cc warning */
	if( nArg > 1 ){
		iChunk = ph7_value_to_int(apArg[1]);
		if( iChunk < 1 ){
			iChunk = 75;
		}
		if( nArg > 2 ){
			zBreak = ph7_value_to_string(apArg[2],&iBreaklen);
		}
	}
	if( iBreaklen < 1 ){
		/* Set a default column break */
#ifdef __WINNT__
		zBreak = "\r\n";
		iBreaklen = (int)sizeof("\r\n")-1;
#else
		zBreak = "\n";
		iBreaklen = (int)sizeof(char);
#endif
	}
	/* Perform the requested operation */
	zEnd = &zIn[iLen];
	for(;;){
		int nMax;
		if( zIn >= zEnd ){
			/* No more input to process */
			break;
		}
		nMax = (int)(zEnd-zIn);
		if( iChunk > nMax ){
			iChunk = nMax;
		}
		/* Append the column first */
		ph7_result_string(pCtx,zIn,iChunk); /* Will make it's own copy */
		/* Advance the cursor */
		zIn += iChunk;
		if( zIn < zEnd ){
			/* Append the line break */
			ph7_result_string(pCtx,zBreak,iBreaklen);
		}
	}
	return PH7_OK;
}
/*
 * Check if the given character is a member of the given mask.
 * Return TRUE on success. FALSE otherwise.
 * Refer to [strtok()].
 */
static int CheckMask(int c,const char *zMask,int nMasklen,int *pOfft)
{
	int i;
	for( i = 0 ; i < nMasklen ; ++i ){
		if( c == zMask[i] ){
			if( pOfft ){
				*pOfft = i;
			}
			return TRUE;
		}
	}
	return FALSE;
}
/*
 * Extract a single token from the input stream.
 * Refer to [strtok()].
 */
static sxi32 ExtractToken(const char **pzIn,const char *zEnd,const char *zMask,int nMasklen,SyString *pOut)
{
	const char *zIn = *pzIn;
	const char *zPtr;
	/* Ignore leading delimiter */
	while( zIn < zEnd && (unsigned char)zIn[0] < 0xc0 && CheckMask(zIn[0],zMask,nMasklen,0) ){
		zIn++;
	}
	if( zIn >= zEnd ){
		/* End of input */
		return SXERR_EOF;
	}
	zPtr = zIn;
	/* Extract the token */
	while( zIn < zEnd ){
		if( (unsigned char)zIn[0] >= 0xc0 ){
			/* UTF-8 stream */
			zIn++;
			SX_JMP_UTF8(zIn,zEnd);
		}else{
			if( CheckMask(zIn[0],zMask,nMasklen,0) ){
				break;
			}
			zIn++;
		}
	}
	SyStringInitFromBuf(pOut,zPtr,zIn-zPtr);
	/* Update the cursor */
	*pzIn = zIn;
	/* Return to the caller */
	return SXRET_OK;
}
/* strtok auxiliary private data */
typedef struct strtok_aux_data strtok_aux_data;
struct strtok_aux_data
{
	const char *zDup;  /* Complete duplicate of the input */
	const char *zIn;   /* Current input stream */
	const char *zEnd;  /* End of input */
};
/*
 * string strtok(string $str,string $token)
 * string strtok(string $token)
 *  strtok() splits a string (str) into smaller strings (tokens), with each token
 *  being delimited by any character from token. That is, if you have a string like
 *  "This is an example string" you could tokenize this string into its individual
 *  words by using the space character as the token.
 *  Note that only the first call to strtok uses the string argument. Every subsequent
 *  call to strtok only needs the token to use, as it keeps track of where it is in 
 *  the current string. To start over, or to tokenize a new string you simply call strtok
 *  with the string argument again to initialize it. Note that you may put multiple tokens
 *  in the token parameter. The string will be tokenized when any one of the characters in 
 *  the argument are found. 
 * Parameters
 *  $str
 *  The string being split up into smaller strings (tokens).
 * $token
 *  The delimiter used when splitting up str.
 * Return
 *   Current token or FALSE on EOF.
 */
static int PH7_builtin_strtok(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	strtok_aux_data *pAux;
	const char *zMask;
	SyString sToken; 
	int nMasklen;
	sxi32 rc;
	if( nArg < 2 ){
		/* Extract top aux data */
		pAux = (strtok_aux_data *)ph7_context_peek_aux_data(pCtx);
		if( pAux == 0 ){
			/* No aux data,return FALSE */
			ph7_result_bool(pCtx,0);
			return PH7_OK;
		}
		nMasklen = 0;
		zMask = ""; /* cc warning */
		if( nArg > 0 ){
			/* Extract the mask */
			zMask = ph7_value_to_string(apArg[0],&nMasklen);
		}
		if( nMasklen < 1 ){
			/* Invalid mask,return FALSE */
			ph7_context_free_chunk(pCtx,(void *)pAux->zDup);
			ph7_context_free_chunk(pCtx,pAux);
			(void)ph7_context_pop_aux_data(pCtx);
			ph7_result_bool(pCtx,0);
			return PH7_OK;
		}
		/* Extract the token */
		rc = ExtractToken(&pAux->zIn,pAux->zEnd,zMask,nMasklen,&sToken);
		if( rc != SXRET_OK ){
			/* EOF ,discard the aux data */
			ph7_context_free_chunk(pCtx,(void *)pAux->zDup);
			ph7_context_free_chunk(pCtx,pAux);
			(void)ph7_context_pop_aux_data(pCtx);
			ph7_result_bool(pCtx,0);
		}else{
			/* Return the extracted token */
			ph7_result_string(pCtx,sToken.zString,(int)sToken.nByte);
		}
	}else{
		const char *zInput,*zCur;
		char *zDup;
		int nLen;
		/* Extract the raw input */
		zCur = zInput = ph7_value_to_string(apArg[0],&nLen);
		if( nLen < 1 ){
			/* Empty input,return FALSE */
			ph7_result_bool(pCtx,0);
			return PH7_OK;
		}
		/* Extract the mask */
		zMask = ph7_value_to_string(apArg[1],&nMasklen);
		if( nMasklen < 1 ){
			/* Set a default mask */
#define TOK_MASK " \n\t\r\f" 
			zMask = TOK_MASK;
			nMasklen = (int)sizeof(TOK_MASK) - 1;
#undef TOK_MASK
		}
		/* Extract a single token */
		rc = ExtractToken(&zInput,&zInput[nLen],zMask,nMasklen,&sToken);
		if( rc != SXRET_OK ){
			/* Empty input */
			ph7_result_bool(pCtx,0);
			return PH7_OK;
		}else{
			/* Return the extracted token */
			ph7_result_string(pCtx,sToken.zString,(int)sToken.nByte);
		}
		/* Create our auxilliary data and copy the input */
		pAux = (strtok_aux_data *)ph7_context_alloc_chunk(pCtx,sizeof(strtok_aux_data),TRUE,FALSE);
		if( pAux ){
			nLen -= (int)(zInput-zCur);
			if( nLen < 1 ){
				ph7_context_free_chunk(pCtx,pAux);
				return PH7_OK;
			}
			/* Duplicate input */
			zDup = (char *)ph7_context_alloc_chunk(pCtx,(unsigned int)(nLen+1),TRUE,FALSE);
			if( zDup  ){
				SyMemcpy(zInput,zDup,(sxu32)nLen);
				/* Register the aux data */
				pAux->zDup = pAux->zIn = zDup;
				pAux->zEnd = &zDup[nLen];
				ph7_context_push_aux_data(pCtx,pAux);
			}
		}
	}
	return PH7_OK;
}
/*
 * string str_pad(string $input,int $pad_length[,string $pad_string = " " [,int $pad_type = STR_PAD_RIGHT]])
 *  Pad a string to a certain length with another string
 * Parameters
 *  $input
 *   The input string.
 * $pad_length
 *   If the value of pad_length is negative, less than, or equal to the length of the input 
 *   string, no padding takes place.
 * $pad_string
 *   Note:
 *    The pad_string WIIL NOT BE truncated if the required number of padding characters can't be evenly
 *    divided by the pad_string's length.
 * $pad_type
 *    Optional argument pad_type can be STR_PAD_RIGHT, STR_PAD_LEFT, or STR_PAD_BOTH. If pad_type
 *    is not specified it is assumed to be STR_PAD_RIGHT.
 * Return
 *  The padded string.
 */
static int PH7_builtin_str_pad(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	int iLen,iPadlen,iType,i,iDiv,iStrpad,iRealPad,jPad;
	const char *zIn,*zPad;
	if( nArg < 2 ){
		/* Missing arguments,return the empty string */
		ph7_result_string(pCtx,"",0);
		return PH7_OK;
	}
	/* Extract the target string */
	zIn = ph7_value_to_string(apArg[0],&iLen);
	/* Padding length */
	iRealPad = iPadlen = ph7_value_to_int(apArg[1]);
	if( iPadlen > 0 ){
		iPadlen -= iLen;
	}
	if( iPadlen < 1  ){
		/* Return the string verbatim */
		ph7_result_string(pCtx,zIn,iLen);
		return PH7_OK;
	}
	zPad = " "; /* Whitespace padding */
	iStrpad = (int)sizeof(char);
	iType = 1 ; /* STR_PAD_RIGHT */
	if( nArg > 2 ){
		/* Padding string */
		zPad = ph7_value_to_string(apArg[2],&iStrpad);
		if( iStrpad < 1 ){
			/* Empty string */
			zPad = " "; /* Whitespace padding */
			iStrpad = (int)sizeof(char);
		}
		if( nArg > 3 ){
			/* Padd type */
			iType = ph7_value_to_int(apArg[3]);
			if( iType != 0 /* STR_PAD_LEFT */ && iType != 2 /* STR_PAD_BOTH */ ){
				iType = 1 ; /* STR_PAD_RIGHT */
			}
		}
	}
	iDiv = 1;
	if( iType == 2 ){
		iDiv = 2; /* STR_PAD_BOTH */
	}
	/* Perform the requested operation */
	if( iType == 0 /* STR_PAD_LEFT */ || iType == 2 /* STR_PAD_BOTH */ ){
		jPad = iStrpad;
		for( i = 0 ; i < iPadlen/iDiv ; i += jPad ){
			/* Padding */
			if( (int)ph7_context_result_buf_length(pCtx) + iLen + jPad >= iRealPad ){
				break;
			}
			ph7_result_string(pCtx,zPad,jPad);
		}
		if( iType == 0 /* STR_PAD_LEFT */ ){
			while( (int)ph7_context_result_buf_length(pCtx) + iLen < iRealPad ){
				jPad = iRealPad - (iLen + (int)ph7_context_result_buf_length(pCtx) );
				if( jPad > iStrpad ){
					jPad = iStrpad;
				}
				if( jPad < 1){
					break;
				}
				ph7_result_string(pCtx,zPad,jPad);
			}
		}
	}
	if( iLen > 0 ){
		/* Append the input string */
		ph7_result_string(pCtx,zIn,iLen);
	}
	if( iType == 1 /* STR_PAD_RIGHT */ || iType == 2 /* STR_PAD_BOTH */ ){
		for( i = 0 ; i < iPadlen/iDiv ; i += iStrpad ){
			/* Padding */
			if( (int)ph7_context_result_buf_length(pCtx) + iStrpad >= iRealPad ){
				break;
			}
			ph7_result_string(pCtx,zPad,iStrpad);
		}
		while( (int)ph7_context_result_buf_length(pCtx) < iRealPad ){
			jPad = iRealPad - (int)ph7_context_result_buf_length(pCtx);
			if( jPad > iStrpad ){
				jPad = iStrpad;
			}
			if( jPad < 1){
				break;
			}
			ph7_result_string(pCtx,zPad,jPad);
		}
	}
	return PH7_OK;
}
/*
 * String replacement private data.
 */
typedef struct str_replace_data str_replace_data;
struct str_replace_data
{
	/* The following two fields are only used by the strtr function */
	SyBlob *pWorker;         /* Working buffer */
	ProcStringMatch xMatch;  /* Pattern match routine */
	/* The following two fields are only used by the str_replace function */
	SySet *pCollector;  /* Argument collector*/
	ph7_context *pCtx;  /* Call context */
};
/*
 * Remove a substring.
 */
#define STRDEL(SRC,SLEN,OFFT,ILEN){\
	for(;;){\
		if( OFFT + ILEN >= SLEN ) break; SRC[OFFT] = SRC[OFFT+ILEN]; ++OFFT;\
	}\
}
/*
 * Shift right and insert algorithm.
 */
#define SHIFTRANDINSERT(SRC,LEN,OFFT,ENTRY,ELEN){\
	sxu32 INLEN = LEN - OFFT;\
	for(;;){\
	  if( LEN > 0 ){ LEN--; } if(INLEN < 1 ) break; SRC[LEN + ELEN] = SRC[LEN] ; --INLEN; \
	}\
	for(;;){\
		if(ELEN < 1)break; SRC[OFFT] = ENTRY[0]; OFFT++; ENTRY++; --ELEN;\
	}\
} 
/*
 * Replace all occurrences of the search string at offset (nOfft) with the given 
 * replacement string [i.e: zReplace].
 */
static int StringReplace(SyBlob *pWorker,sxu32 nOfft,int nLen,const char *zReplace,int nReplen)
{
	char *zInput = (char *)SyBlobData(pWorker);
	sxu32 n,m;
	n = SyBlobLength(pWorker);
	m = nOfft;
	/* Delete the old entry */
	STRDEL(zInput,n,m,nLen);
	SyBlobLength(pWorker) -= nLen;
	if( nReplen > 0 ){
		sxi32 iRep = nReplen;
		sxi32 rc;
		/*
		 * Make sure the working buffer is big enough to hold the replacement
		 * string.
		 */
		rc = SyBlobAppend(pWorker,0/* Grow without an append operation*/,(sxu32)nReplen);
		if( rc != SXRET_OK ){
			/* Simply ignore any memory failure problem */
			return SXRET_OK;
		}
		/* Perform the insertion now */
		zInput = (char *)SyBlobData(pWorker);
		n = SyBlobLength(pWorker);
		SHIFTRANDINSERT(zInput,n,nOfft,zReplace,iRep);
		SyBlobLength(pWorker) += nReplen;
	}	
	return SXRET_OK;
}
/*
 * String replacement walker callback.
 * The following callback is invoked for each array entry that hold
 * the replace string.
 * Refer to the strtr() implementation for more information.
 */
static int StringReplaceWalker(ph7_value *pKey,ph7_value *pData,void *pUserData)
{
	str_replace_data *pRepData = (str_replace_data *)pUserData;
	const char *zTarget,*zReplace;
	SyBlob *pWorker;
	int tLen,nLen;
	sxu32 nOfft;
	sxi32 rc;
	/* Point to the working buffer */
	pWorker = pRepData->pWorker;
	if( !ph7_value_is_string(pKey) ){
		/* Target and replace must be a string */
		return PH7_OK;
	}
	/* Extract the target and the replace */
	zTarget = ph7_value_to_string(pKey,&tLen);
	if( tLen < 1 ){
		/* Empty target,return immediately */
		return PH7_OK;
	}
	/* Perform a pattern search */
	rc = pRepData->xMatch(SyBlobData(pWorker),SyBlobLength(pWorker),(const void *)zTarget,(sxu32)tLen,&nOfft);
	if( rc != SXRET_OK ){
		/* Pattern not found */
		return PH7_OK;
	}
	/* Extract the replace string */
	zReplace = ph7_value_to_string(pData,&nLen);
	/* Perform the replace process */
	StringReplace(pWorker,nOfft,tLen,zReplace,nLen);
	/* All done */
	return PH7_OK;
}
/*
 * The following walker callback is invoked by the str_rplace() function inorder
 * to collect search/replace string.
 * This callback is invoked only if the given argument is of type array.
 */
static int StrReplaceWalker(ph7_value *pKey,ph7_value *pData,void *pUserData)
{
	str_replace_data *pRep = (str_replace_data *)pUserData;
	SyString sWorker;
	const char *zIn;
	int nByte;
	/* Extract a string representation of the given argument */
	zIn = ph7_value_to_string(pData,&nByte);
	SyStringInitFromBuf(&sWorker,0,0);
	if( nByte > 0 ){
		char *zDup;
		/* Duplicate the chunk */
		zDup = (char *)ph7_context_alloc_chunk(pRep->pCtx,(unsigned int)nByte,FALSE,
			TRUE /* Release the chunk automatically,upon this context is destroyd */
			);
		if( zDup == 0 ){
			/* Ignore any memory failure problem */
			ph7_context_throw_error(pRep->pCtx,PH7_CTX_ERR,"PH7 is running out of memory");
			return PH7_OK;
		}
		SyMemcpy(zIn,zDup,(sxu32)nByte);
		/* Save the chunk */
		SyStringInitFromBuf(&sWorker,zDup,nByte);
	}
	/* Save for later processing */
	SySetPut(pRep->pCollector,(const void *)&sWorker);
	/* All done */
	SXUNUSED(pKey); /* cc warning */
	return PH7_OK;
}
/*
 * mixed str_replace(mixed $search,mixed $replace,mixed $subject[,int &$count ])
 * mixed str_ireplace(mixed $search,mixed $replace,mixed $subject[,int &$count ])
 *  Replace all occurrences of the search string with the replacement string.
 * Parameters
 *  If search and replace are arrays, then str_replace() takes a value from each
 *  array and uses them to search and replace on subject. If replace has fewer values
 *  than search, then an empty string is used for the rest of replacement values.
 *  If search is an array and replace is a string, then this replacement string is used
 *  for every value of search. The converse would not make sense, though.
 *  If search or replace are arrays, their elements are processed first to last.
 * $search
 *  The value being searched for, otherwise known as the needle. An array may be used
 *  to designate multiple needles.
 * $replace
 *  The replacement value that replaces found search values. An array may be used
 *  to designate multiple replacements.
 * $subject
 *  The string or array being searched and replaced on, otherwise known as the haystack.
 *  If subject is an array, then the search and replace is performed with every entry 
 *  of subject, and the return value is an array as well.
 * $count (Not used)
 *  If passed, this will be set to the number of replacements performed.
 * Return
 * This function returns a string or an array with the replaced values.
 */
static int PH7_builtin_str_replace(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	SyString sTemp,*pSearch,*pReplace;
	ProcStringMatch xMatch;
	const char *zIn,*zFunc;
	str_replace_data sRep;
	SyBlob sWorker;
	SySet sReplace;
	SySet sSearch;
	int rep_str;
	int nByte;
	sxi32 rc;
	if( nArg < 3 ){
		/* Missing/Invalid arguments,return null */
		ph7_result_null(pCtx);
		return PH7_OK;
	}
	/* Initialize fields */
	SySetInit(&sSearch,&pCtx->pVm->sAllocator,sizeof(SyString));
	SySetInit(&sReplace,&pCtx->pVm->sAllocator,sizeof(SyString));
	SyBlobInit(&sWorker,&pCtx->pVm->sAllocator);
	SyZero(&sRep,sizeof(str_replace_data));
	sRep.pCtx = pCtx;
	sRep.pCollector = &sSearch;
	rep_str = 0;
	/* Extract the subject */
	zIn = ph7_value_to_string(apArg[2],&nByte);
	if( nByte < 1 ){
		/* Nothing to replace,return the empty string */
		ph7_result_string(pCtx,"",0);
		return PH7_OK;
	}
	/* Copy the subject */
	SyBlobAppend(&sWorker,(const void *)zIn,(sxu32)nByte);
	/* Search string */
	if( ph7_value_is_array(apArg[0]) ){
		/* Collect search string */
		ph7_array_walk(apArg[0],StrReplaceWalker,&sRep);
	}else{
		/* Single pattern */
		zIn = ph7_value_to_string(apArg[0],&nByte);
		if( nByte < 1 ){
			/* Return the subject untouched since no search string is available */
			ph7_result_value(pCtx,apArg[2]/* Subject as thrird argument*/);
			return PH7_OK;
		}
		SyStringInitFromBuf(&sTemp,zIn,nByte);
		/* Save for later processing */
		SySetPut(&sSearch,(const void *)&sTemp);
	}
	/* Replace string */
	if( ph7_value_is_array(apArg[1]) ){
		/* Collect replace string */
		sRep.pCollector = &sReplace;
		ph7_array_walk(apArg[1],StrReplaceWalker,&sRep);
	}else{
		/* Single needle */
		zIn = ph7_value_to_string(apArg[1],&nByte);
		rep_str = 1;
		SyStringInitFromBuf(&sTemp,zIn,nByte);
		/* Save for later processing */
		SySetPut(&sReplace,(const void *)&sTemp);
	}
	/* Reset loop cursors */
	SySetResetCursor(&sSearch);
	SySetResetCursor(&sReplace);
	pReplace = pSearch = 0; /* cc warning */
	SyStringInitFromBuf(&sTemp,"",0);
	/* Extract function name */
	zFunc = ph7_function_name(pCtx);
	/* Set the default pattern match routine */
	xMatch = SyBlobSearch;
	if( SyStrncmp(zFunc,"str_ireplace",sizeof("str_ireplace") - 1) ==  0 ){
		/* Case insensitive pattern match */
		xMatch = iPatternMatch;
	}
	/* Start the replace process */
	while( SXRET_OK == SySetGetNextEntry(&sSearch,(void **)&pSearch) ){
		sxu32 nCount,nOfft;
		if( pSearch->nByte <  1 ){
			/* Empty string,ignore */
			continue;
		}
		/* Extract the replace string */
		if( rep_str ){
			pReplace = (SyString *)SySetPeek(&sReplace);
		}else{
			if( SXRET_OK != SySetGetNextEntry(&sReplace,(void **)&pReplace) ){
				/* Sepecial case when 'replace set' has fewer values than the search set.
				 * An empty string is used for the rest of replacement values
				 */
				pReplace = 0;
			}
		}
		if( pReplace == 0 ){
			/* Use an empty string instead */
			pReplace = &sTemp;
		}
		nOfft = nCount = 0;
		for(;;){
			if( nCount >= SyBlobLength(&sWorker) ){
				break;
			}
			/* Perform a pattern lookup */
			rc = xMatch(SyBlobDataAt(&sWorker,nCount),SyBlobLength(&sWorker) - nCount,(const void *)pSearch->zString,
				pSearch->nByte,&nOfft);
			if( rc != SXRET_OK ){
				/* Pattern not found */
				break;
			}
			/* Perform the replace operation */
			StringReplace(&sWorker,nCount+nOfft,(int)pSearch->nByte,pReplace->zString,(int)pReplace->nByte);
			/* Increment offset counter */
			nCount += nOfft + pReplace->nByte;
		}
	}
	/* All done,clean-up the mess left behind */
	ph7_result_string(pCtx,(const char *)SyBlobData(&sWorker),(int)SyBlobLength(&sWorker));
	SySetRelease(&sSearch);
	SySetRelease(&sReplace);
	SyBlobRelease(&sWorker);
	return PH7_OK;
}
/*
 * string strtr(string $str,string $from,string $to)
 * string strtr(string $str,array $replace_pairs)
 *  Translate characters or replace substrings.
 * Parameters
 *  $str
 *  The string being translated.
 * $from
 *  The string being translated to to.
 * $to
 *  The string replacing from.
 * $replace_pairs
 *  The replace_pairs parameter may be used instead of to and 
 *  from, in which case it's an array in the form array('from' => 'to', ...).
 * Return
 *  The translated string.
 *  If replace_pairs contains a key which is an empty string (""), FALSE will be returned.
 */
static int PH7_builtin_strtr(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	const char *zIn;
	int nLen;
	if( nArg < 1 ){
		/* Nothing to replace,return FALSE */
		ph7_result_bool(pCtx,0);
		return PH7_OK;
	}
	zIn = ph7_value_to_string(apArg[0],&nLen);
	if( nLen < 1 || nArg < 2 ){
		/* Invalid arguments */
		ph7_result_string(pCtx,zIn,nLen);
		return PH7_OK;
	}
	if( nArg == 2 && ph7_value_is_array(apArg[1]) ){
		str_replace_data sRepData;
		SyBlob sWorker;
		/* Initilaize the working buffer */
		SyBlobInit(&sWorker,&pCtx->pVm->sAllocator);
		/* Copy raw string */
		SyBlobAppend(&sWorker,(const void *)zIn,(sxu32)nLen);
		/* Init our replace data instance */
		sRepData.pWorker = &sWorker;
		sRepData.xMatch = SyBlobSearch;
		/* Iterate throw array entries and perform the replace operation.*/
		ph7_array_walk(apArg[1],StringReplaceWalker,&sRepData);
		/* All done, return the result string */
		ph7_result_string(pCtx,(const char *)SyBlobData(&sWorker),
			(int)SyBlobLength(&sWorker)); /* Will make it's own copy */
		/* Clean-up */
		SyBlobRelease(&sWorker);
	}else{
		int i,flen,tlen,c,iOfft;
		const char *zFrom,*zTo;
		if( nArg < 3 ){
			/* Nothing to replace */
			ph7_result_string(pCtx,zIn,nLen);
			return PH7_OK;
		}
		/* Extract given arguments */
		zFrom = ph7_value_to_string(apArg[1],&flen);
		zTo = ph7_value_to_string(apArg[2],&tlen);
		if( flen < 1 || tlen < 1 ){
			/* Nothing to replace */
			ph7_result_string(pCtx,zIn,nLen);
			return PH7_OK;
		}
		/* Start the replace process */
		for( i = 0 ; i < nLen ; ++i ){
			c = zIn[i];
			if( CheckMask(c,zFrom,flen,&iOfft) ){
				if ( iOfft < tlen ){
					c = zTo[iOfft];
				}
			}
			ph7_result_string(pCtx,(const char *)&c,(int)sizeof(char));
			
		}
	}
	return PH7_OK;
}
/*
 * Parse an INI string.
 * According to wikipedia
 *  The INI file format is an informal standard for configuration files for some platforms or software.
 *  INI files are simple text files with a basic structure composed of "sections" and "properties".
 *  Format
*    Properties
*     The basic element contained in an INI file is the property. Every property has a name and a value
*     delimited by an equals sign (=). The name appears to the left of the equals sign.
*     Example:
*      name=value
*    Sections
*     Properties may be grouped into arbitrarily named sections. The section name appears on a line by itself
*     in square brackets ([ and ]). All properties after the section declaration are associated with that section.
*     There is no explicit "end of section" delimiter; sections end at the next section declaration
*     or the end of the file. Sections may not be nested.
*     Example:
*      [section]
*   Comments
*    Semicolons (;) at the beginning of the line indicate a comment. Comment lines are ignored.
* This function return an array holding parsed values on success.FALSE otherwise.
*/
PH7_PRIVATE sxi32 PH7_ParseIniString(ph7_context *pCtx,const char *zIn,sxu32 nByte,int bProcessSection)
{
	ph7_value *pCur,*pArray,*pSection,*pWorker,*pValue;
	const char *zCur,*zEnd = &zIn[nByte];
	SyHashEntry *pEntry;
	SyString sEntry;
	SyHash sHash;
	int c;
	/* Create an empty array and worker variables */
	pArray = ph7_context_new_array(pCtx);
	pWorker = ph7_context_new_scalar(pCtx);
	pValue = ph7_context_new_scalar(pCtx);
	if( pArray == 0 || pWorker == 0 || pValue == 0){
		/* Out of memory */
		ph7_context_throw_error(pCtx,PH7_CTX_ERR,"PH7 is running out of memory");
		/* Return FALSE */
		ph7_result_bool(pCtx,0);
		return PH7_OK;
	}
	SyHashInit(&sHash,&pCtx->pVm->sAllocator,0,0);
	pCur = pArray;
	/* Start the parse process */
	for(;;){
		/* Ignore leading white spaces */
		while( zIn < zEnd && (unsigned char)zIn[0] < 0xc0 && SyisSpace(zIn[0])){
			zIn++;
		}
		if( zIn >= zEnd ){
			/* No more input to process */
			break;
		}
		if( zIn[0] == ';' || zIn[0] == '#' ){
			/* Comment til the end of line */
			zIn++;
			while(zIn < zEnd && zIn[0] != '\n' ){
				zIn++;
			}
			continue;
		}
		/* Reset the string cursor of the working variable */
		ph7_value_reset_string_cursor(pWorker);
		if( zIn[0] == '[' ){
			/* Section: Extract the section name */
			zIn++;
			zCur = zIn;
			while( zIn < zEnd && zIn[0] != ']' ){
				zIn++;
			}
			if( zIn > zCur && bProcessSection ){
				/* Save the section name */
				SyStringInitFromBuf(&sEntry,zCur,(int)(zIn-zCur));
				SyStringFullTrim(&sEntry);
				ph7_value_string(pWorker,sEntry.zString,(int)sEntry.nByte);
				if( sEntry.nByte > 0 ){
					/* Associate an array with the section */
					pSection = ph7_context_new_array(pCtx);
					if( pSection ){
						ph7_array_add_elem(pArray,pWorker/*Section name*/,pSection);
						pCur = pSection;
					}
				}
			}
			zIn++; /* Trailing square brackets ']' */
		}else{
			ph7_value *pOldCur;
			int is_array;
			int iLen;
			/* Properties */
			is_array = 0;
			zCur = zIn;
			iLen = 0; /* cc warning */
			pOldCur = pCur;
			while( zIn < zEnd && zIn[0] != '=' ){
				if( zIn[0] == '[' && !is_array ){
					/* Array */
					iLen = (int)(zIn-zCur);
					is_array = 1;
					if( iLen > 0 ){
						ph7_value *pvArr = 0; /* cc warning */
						/* Query the hashtable */
						SyStringInitFromBuf(&sEntry,zCur,iLen);
						SyStringFullTrim(&sEntry);
						pEntry = SyHashGet(&sHash,(const void *)sEntry.zString,sEntry.nByte);
						if( pEntry ){
							pvArr = (ph7_value *)SyHashEntryGetUserData(pEntry);
						}else{
							/* Create an empty array */
							pvArr = ph7_context_new_array(pCtx);
							if( pvArr ){
								/* Save the entry */
								SyHashInsert(&sHash,(const void *)sEntry.zString,sEntry.nByte,pvArr);
								/* Insert the entry */
								ph7_value_reset_string_cursor(pWorker);
								ph7_value_string(pWorker,sEntry.zString,(int)sEntry.nByte);
								ph7_array_add_elem(pCur,pWorker,pvArr);
								ph7_value_reset_string_cursor(pWorker);
							}
						}
						if( pvArr ){
							pCur = pvArr;
						}
					}
					while ( zIn < zEnd && zIn[0] != ']' ){
						zIn++;
					}
				}
				zIn++;
			}
			if( !is_array ){
				iLen = (int)(zIn-zCur);
			}
			/* Trim the key */
			SyStringInitFromBuf(&sEntry,zCur,iLen);
			SyStringFullTrim(&sEntry);
			if( sEntry.nByte > 0 ){
				if( !is_array ){
					/* Save the key name */
					ph7_value_string(pWorker,sEntry.zString,(int)sEntry.nByte);
				}
				/* extract key value */
				ph7_value_reset_string_cursor(pValue);
				zIn++; /* '=' */
				while( zIn < zEnd && (unsigned char)zIn[0] < 0xc0 && SyisSpace(zIn[0]) ){
					zIn++;
				}
				if( zIn < zEnd ){
					zCur = zIn;
					c = zIn[0];
					if( c == '"' || c == '\'' ){
						zIn++;
						/* Delimit the value */
						while( zIn < zEnd ){
							if ( zIn[0] == c && zIn[-1] != '\\' ){
								break;
							}
							zIn++;
						}
						if( zIn < zEnd ){
							zIn++;
						}
					}else{
						while( zIn < zEnd ){
							if( zIn[0] == '\n' ){
								if( zIn[-1] != '\\' ){
									break;
								}
							}else if( zIn[0] == ';' || zIn[0] == '#' ){
								/* Inline comments */
								break;
							}
							zIn++;
						}
					}
					/* Trim the value */
					SyStringInitFromBuf(&sEntry,zCur,(int)(zIn-zCur));
					SyStringFullTrim(&sEntry);
					if( c == '"' || c == '\'' ){
						SyStringTrimLeadingChar(&sEntry,c);
						SyStringTrimTrailingChar(&sEntry,c);
					}
					if( sEntry.nByte > 0 ){
						ph7_value_string(pValue,sEntry.zString,(int)sEntry.nByte);
					}
					/* Insert the key and it's value */
					ph7_array_add_elem(pCur,is_array ? 0 /*Automatic index assign */: pWorker,pValue);
				}
			}else{
				while( zIn < zEnd && (unsigned char)zIn[0] < 0xc0 && ( SyisSpace(zIn[0]) || zIn[0] == '=' ) ){
					zIn++;
				}
			}
			pCur = pOldCur;
		}
	}
	SyHashRelease(&sHash);
	/* Return the parse of the INI string */
	ph7_result_value(pCtx,pArray);
	return SXRET_OK;
}
/*
 * array parse_ini_string(string $ini[,bool $process_sections = false[,int $scanner_mode = INI_SCANNER_NORMAL ]])
 *  Parse a configuration string.
 * Parameters
 *  $ini
 *   The contents of the ini file being parsed.
 *  $process_sections
 *   By setting the process_sections parameter to TRUE, you get a multidimensional array, with the section names
 *   and settings included. The default for process_sections is FALSE.
 *  $scanner_mode (Not used)
 *   Can either be INI_SCANNER_NORMAL (default) or INI_SCANNER_RAW. If INI_SCANNER_RAW is supplied
 *   then option values will not be parsed.
 * Return
 *  The settings are returned as an associative array on success, and FALSE on failure.
 */
static int PH7_builtin_parse_ini_string(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	const char *zIni;
	int nByte;
	if( nArg < 1 || !ph7_value_is_string(apArg[0]) ){
		/* Missing/Invalid arguments,return FALSE*/
		ph7_result_bool(pCtx,0);
		return PH7_OK;
	}
	/* Extract the raw INI buffer */
	zIni = ph7_value_to_string(apArg[0],&nByte);
	/* Process the INI buffer*/
	PH7_ParseIniString(pCtx,zIni,(sxu32)nByte,(nArg > 1) ? ph7_value_to_bool(apArg[1]) : 0);
	return PH7_OK;
}
/*
 * Ctype Functions.
 * Authors:
 *    Symisc Systems,devel@symisc.net.
 *    Copyright (C) Symisc Systems,http://ph7.symisc.net
 * Status:
 *    Stable.
 */
/*
 * bool ctype_alnum(string $text)
 *  Checks if all of the characters in the provided string, text, are alphanumeric.
 * Parameters
 *  $text
 *   The tested string.
 * Return
 *   TRUE if every character in text is either a letter or a digit, FALSE otherwise.
 */
static int PH7_builtin_ctype_alnum(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	const unsigned char *zIn,*zEnd;
	int nLen;
	if( nArg < 1 ){
		/* Missing arguments,return FALSE */
		ph7_result_bool(pCtx,0);
		return PH7_OK;
	}
	/* Extract the target string */
	zIn  = (const unsigned char *)ph7_value_to_string(apArg[0],&nLen);
	zEnd = &zIn[nLen];
	if( nLen < 1 ){
		/* Empty string,return FALSE */
		ph7_result_bool(pCtx,0);
		return PH7_OK;
	}
	/* Perform the requested operation */
	for(;;){
		if( zIn >= zEnd ){
			/* If we reach the end of the string,then the test succeeded. */
			ph7_result_bool(pCtx,1);
			return PH7_OK;
		}
		if( !SyisAlphaNum(zIn[0]) ){
			break;
		}
		/* Point to the next character */
		zIn++;
	}
	/* The test failed,return FALSE */
	ph7_result_bool(pCtx,0);
	return PH7_OK;
}
/*
 * bool ctype_alpha(string $text)
 *  Checks if all of the characters in the provided string, text, are alphabetic.
 * Parameters
 *  $text
 *   The tested string.
 * Return
 *  TRUE if every character in text is a letter from the current locale, FALSE otherwise.
 */
static int PH7_builtin_ctype_alpha(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	const unsigned char *zIn,*zEnd;
	int nLen;
	if( nArg < 1 ){
		/* Missing arguments,return FALSE */
		ph7_result_bool(pCtx,0);
		return PH7_OK;
	}
	/* Extract the target string */
	zIn  = (const unsigned char *)ph7_value_to_string(apArg[0],&nLen);
	zEnd = &zIn[nLen];
	if( nLen < 1 ){
		/* Empty string,return FALSE */
		ph7_result_bool(pCtx,0);
		return PH7_OK;
	}
	/* Perform the requested operation */
	for(;;){
		if( zIn >= zEnd ){
			/* If we reach the end of the string,then the test succeeded. */
			ph7_result_bool(pCtx,1);
			return PH7_OK;
		}
		if( !SyisAlpha(zIn[0]) ){
			break;
		}
		/* Point to the next character */
		zIn++;
	}
	/* The test failed,return FALSE */
	ph7_result_bool(pCtx,0);
	return PH7_OK;
}
/*
 * bool ctype_cntrl(string $text)
 *  Checks if all of the characters in the provided string, text, are control characters.
 * Parameters
 *  $text
 *   The tested string.
 * Return
 *  TRUE if every character in text is a control characters,FALSE otherwise.
 */
static int PH7_builtin_ctype_cntrl(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	const unsigned char *zIn,*zEnd;
	int nLen;
	if( nArg < 1 ){
		/* Missing arguments,return FALSE */
		ph7_result_bool(pCtx,0);
		return PH7_OK;
	}
	/* Extract the target string */
	zIn  = (const unsigned char *)ph7_value_to_string(apArg[0],&nLen);
	zEnd = &zIn[nLen];
	if( nLen < 1 ){
		/* Empty string,return FALSE */
		ph7_result_bool(pCtx,0);
		return PH7_OK;
	}
	/* Perform the requested operation */
	for(;;){
		if( zIn >= zEnd ){
			/* If we reach the end of the string,then the test succeeded. */
			ph7_result_bool(pCtx,1);
			return PH7_OK;
		}
		if( zIn[0] >= 0xc0 ){
			/* UTF-8 stream  */
			break;
		}
		if( !SyisCtrl(zIn[0]) ){
			break;
		}
		/* Point to the next character */
		zIn++;
	}
	/* The test failed,return FALSE */
	ph7_result_bool(pCtx,0);
	return PH7_OK;
}
/*
 * bool ctype_digit(string $text)
 *  Checks if all of the characters in the provided string, text, are numerical.
 * Parameters
 *  $text
 *   The tested string.
 * Return
 *  TRUE if every character in the string text is a decimal digit, FALSE otherwise.
 */
static int PH7_builtin_ctype_digit(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	const unsigned char *zIn,*zEnd;
	int nLen;
	if( nArg < 1 ){
		/* Missing arguments,return FALSE */
		ph7_result_bool(pCtx,0);
		return PH7_OK;
	}
	/* Extract the target string */
	zIn  = (const unsigned char *)ph7_value_to_string(apArg[0],&nLen);
	zEnd = &zIn[nLen];
	if( nLen < 1 ){
		/* Empty string,return FALSE */
		ph7_result_bool(pCtx,0);
		return PH7_OK;
	}
	/* Perform the requested operation */
	for(;;){
		if( zIn >= zEnd ){
			/* If we reach the end of the string,then the test succeeded. */
			ph7_result_bool(pCtx,1);
			return PH7_OK;
		}
		if( zIn[0] >= 0xc0 ){
			/* UTF-8 stream  */
			break;
		}
		if( !SyisDigit(zIn[0]) ){
			break;
		}
		/* Point to the next character */
		zIn++;
	}
	/* The test failed,return FALSE */
	ph7_result_bool(pCtx,0);
	return PH7_OK;
}
/*
 * bool ctype_xdigit(string $text)
 *  Check for character(s) representing a hexadecimal digit.
 * Parameters
 *  $text
 *   The tested string.
 * Return
 *  Returns TRUE if every character in text is a hexadecimal 'digit', that is
 * a decimal digit or a character from [A-Fa-f] , FALSE otherwise. 
 */
static int PH7_builtin_ctype_xdigit(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	const unsigned char *zIn,*zEnd;
	int nLen;
	if( nArg < 1 ){
		/* Missing arguments,return FALSE */
		ph7_result_bool(pCtx,0);
		return PH7_OK;
	}
	/* Extract the target string */
	zIn  = (const unsigned char *)ph7_value_to_string(apArg[0],&nLen);
	zEnd = &zIn[nLen];
	if( nLen < 1 ){
		/* Empty string,return FALSE */
		ph7_result_bool(pCtx,0);
		return PH7_OK;
	}
	/* Perform the requested operation */
	for(;;){
		if( zIn >= zEnd ){
			/* If we reach the end of the string,then the test succeeded. */
			ph7_result_bool(pCtx,1);
			return PH7_OK;
		}
		if( zIn[0] >= 0xc0 ){
			/* UTF-8 stream  */
			break;
		}
		if( !SyisHex(zIn[0]) ){
			break;
		}
		/* Point to the next character */
		zIn++;
	}
	/* The test failed,return FALSE */
	ph7_result_bool(pCtx,0);
	return PH7_OK;
}
/*
 * bool ctype_graph(string $text)
 *  Checks if all of the characters in the provided string, text, creates visible output.
 * Parameters
 *  $text
 *   The tested string.
 * Return
 *  Returns TRUE if every character in text is printable and actually creates visible output
 * (no white space), FALSE otherwise. 
 */
static int PH7_builtin_ctype_graph(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	const unsigned char *zIn,*zEnd;
	int nLen;
	if( nArg < 1 ){
		/* Missing arguments,return FALSE */
		ph7_result_bool(pCtx,0);
		return PH7_OK;
	}
	/* Extract the target string */
	zIn  = (const unsigned char *)ph7_value_to_string(apArg[0],&nLen);
	zEnd = &zIn[nLen];
	if( nLen < 1 ){
		/* Empty string,return FALSE */
		ph7_result_bool(pCtx,0);
		return PH7_OK;
	}
	/* Perform the requested operation */
	for(;;){
		if( zIn >= zEnd ){
			/* If we reach the end of the string,then the test succeeded. */
			ph7_result_bool(pCtx,1);
			return PH7_OK;
		}
		if( zIn[0] >= 0xc0 ){
			/* UTF-8 stream  */
			break;
		}
		if( !SyisGraph(zIn[0]) ){
			break;
		}
		/* Point to the next character */
		zIn++;
	}
	/* The test failed,return FALSE */
	ph7_result_bool(pCtx,0);
	return PH7_OK;
}
/*
 * bool ctype_print(string $text)
 *  Checks if all of the characters in the provided string, text, are printable.
 * Parameters
 *  $text
 *   The tested string.
 * Return
 *  Returns TRUE if every character in text will actually create output (including blanks).
 *  Returns FALSE if text contains control characters or characters that do not have any output
 *  or control function at all. 
 */
static int PH7_builtin_ctype_print(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	const unsigned char *zIn,*zEnd;
	int nLen;
	if( nArg < 1 ){
		/* Missing arguments,return FALSE */
		ph7_result_bool(pCtx,0);
		return PH7_OK;
	}
	/* Extract the target string */
	zIn  = (const unsigned char *)ph7_value_to_string(apArg[0],&nLen);
	zEnd = &zIn[nLen];
	if( nLen < 1 ){
		/* Empty string,return FALSE */
		ph7_result_bool(pCtx,0);
		return PH7_OK;
	}
	/* Perform the requested operation */
	for(;;){
		if( zIn >= zEnd ){
			/* If we reach the end of the string,then the test succeeded. */
			ph7_result_bool(pCtx,1);
			return PH7_OK;
		}
		if( zIn[0] >= 0xc0 ){
			/* UTF-8 stream  */
			break;
		}
		if( !SyisPrint(zIn[0]) ){
			break;
		}
		/* Point to the next character */
		zIn++;
	}
	/* The test failed,return FALSE */
	ph7_result_bool(pCtx,0);
	return PH7_OK;
}
/*
 * bool ctype_punct(string $text)
 *  Checks if all of the characters in the provided string, text, are punctuation character.
 * Parameters
 *  $text
 *   The tested string.
 * Return
 *  Returns TRUE if every character in text is printable, but neither letter
 *  digit or blank, FALSE otherwise.
 */
static int PH7_builtin_ctype_punct(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	const unsigned char *zIn,*zEnd;
	int nLen;
	if( nArg < 1 ){
		/* Missing arguments,return FALSE */
		ph7_result_bool(pCtx,0);
		return PH7_OK;
	}
	/* Extract the target string */
	zIn  = (const unsigned char *)ph7_value_to_string(apArg[0],&nLen);
	zEnd = &zIn[nLen];
	if( nLen < 1 ){
		/* Empty string,return FALSE */
		ph7_result_bool(pCtx,0);
		return PH7_OK;
	}
	/* Perform the requested operation */
	for(;;){
		if( zIn >= zEnd ){
			/* If we reach the end of the string,then the test succeeded. */
			ph7_result_bool(pCtx,1);
			return PH7_OK;
		}
		if( zIn[0] >= 0xc0 ){
			/* UTF-8 stream  */
			break;
		}
		if( !SyisPunct(zIn[0]) ){
			break;
		}
		/* Point to the next character */
		zIn++;
	}
	/* The test failed,return FALSE */
	ph7_result_bool(pCtx,0);
	return PH7_OK;
}
/*
 * bool ctype_space(string $text)
 *  Checks if all of the characters in the provided string, text, creates whitespace.
 * Parameters
 *  $text
 *   The tested string.
 * Return
 *  Returns TRUE if every character in text creates some sort of white space, FALSE otherwise.
 *  Besides the blank character this also includes tab, vertical tab, line feed, carriage return
 *  and form feed characters. 
 */
static int PH7_builtin_ctype_space(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	const unsigned char *zIn,*zEnd;
	int nLen;
	if( nArg < 1 ){
		/* Missing arguments,return FALSE */
		ph7_result_bool(pCtx,0);
		return PH7_OK;
	}
	/* Extract the target string */
	zIn  = (const unsigned char *)ph7_value_to_string(apArg[0],&nLen);
	zEnd = &zIn[nLen];
	if( nLen < 1 ){
		/* Empty string,return FALSE */
		ph7_result_bool(pCtx,0);
		return PH7_OK;
	}
	/* Perform the requested operation */
	for(;;){
		if( zIn >= zEnd ){
			/* If we reach the end of the string,then the test succeeded. */
			ph7_result_bool(pCtx,1);
			return PH7_OK;
		}
		if( zIn[0] >= 0xc0 ){
			/* UTF-8 stream  */
			break;
		}
		if( !SyisSpace(zIn[0]) ){
			break;
		}
		/* Point to the next character */
		zIn++;
	}
	/* The test failed,return FALSE */
	ph7_result_bool(pCtx,0);
	return PH7_OK;
}
/*
 * bool ctype_lower(string $text)
 *  Checks if all of the characters in the provided string, text, are lowercase letters.
 * Parameters
 *  $text
 *   The tested string.
 * Return
 *  Returns TRUE if every character in text is a lowercase letter in the current locale. 
 */
static int PH7_builtin_ctype_lower(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	const unsigned char *zIn,*zEnd;
	int nLen;
	if( nArg < 1 ){
		/* Missing arguments,return FALSE */
		ph7_result_bool(pCtx,0);
		return PH7_OK;
	}
	/* Extract the target string */
	zIn  = (const unsigned char *)ph7_value_to_string(apArg[0],&nLen);
	zEnd = &zIn[nLen];
	if( nLen < 1 ){
		/* Empty string,return FALSE */
		ph7_result_bool(pCtx,0);
		return PH7_OK;
	}
	/* Perform the requested operation */
	for(;;){
		if( zIn >= zEnd ){
			/* If we reach the end of the string,then the test succeeded. */
			ph7_result_bool(pCtx,1);
			return PH7_OK;
		}
		if( !SyisLower(zIn[0]) ){
			break;
		}
		/* Point to the next character */
		zIn++;
	}
	/* The test failed,return FALSE */
	ph7_result_bool(pCtx,0);
	return PH7_OK;
}
/*
 * bool ctype_upper(string $text)
 *  Checks if all of the characters in the provided string, text, are uppercase letters.
 * Parameters
 *  $text
 *   The tested string.
 * Return
 *  Returns TRUE if every character in text is a uppercase letter in the current locale. 
 */
static int PH7_builtin_ctype_upper(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	const unsigned char *zIn,*zEnd;
	int nLen;
	if( nArg < 1 ){
		/* Missing arguments,return FALSE */
		ph7_result_bool(pCtx,0);
		return PH7_OK;
	}
	/* Extract the target string */
	zIn  = (const unsigned char *)ph7_value_to_string(apArg[0],&nLen);
	zEnd = &zIn[nLen];
	if( nLen < 1 ){
		/* Empty string,return FALSE */
		ph7_result_bool(pCtx,0);
		return PH7_OK;
	}
	/* Perform the requested operation */
	for(;;){
		if( zIn >= zEnd ){
			/* If we reach the end of the string,then the test succeeded. */
			ph7_result_bool(pCtx,1);
			return PH7_OK;
		}
		if( !SyisUpper(zIn[0]) ){
			break;
		}
		/* Point to the next character */
		zIn++;
	}
	/* The test failed,return FALSE */
	ph7_result_bool(pCtx,0);
	return PH7_OK;
}
/*
 * Date/Time functions
 * Authors:
 *    Symisc Systems,devel@symisc.net.
 *    Copyright (C) Symisc Systems,http://ph7.symisc.net
 * Status:
 *    Devel.
 */
#include <time.h>
#ifdef __WINNT__
/* GetSystemTime() */
#include <Windows.h> 
#ifdef _WIN32_WCE
/*
** WindowsCE does not have a localtime() function.  So create a
** substitute.
** Taken from the SQLite3 source tree.
** Status: Public domain
*/
struct tm *__cdecl localtime(const time_t *t)
{
  static struct tm y;
  FILETIME uTm, lTm;
  SYSTEMTIME pTm;
  ph7_int64 t64;
  t64 = *t;
  t64 = (t64 + 11644473600)*10000000;
  uTm.dwLowDateTime = (DWORD)(t64 & 0xFFFFFFFF);
  uTm.dwHighDateTime= (DWORD)(t64 >> 32);
  FileTimeToLocalFileTime(&uTm,&lTm);
  FileTimeToSystemTime(&lTm,&pTm);
  y.tm_year = pTm.wYear - 1900;
  y.tm_mon = pTm.wMonth - 1;
  y.tm_wday = pTm.wDayOfWeek;
  y.tm_mday = pTm.wDay;
  y.tm_hour = pTm.wHour;
  y.tm_min = pTm.wMinute;
  y.tm_sec = pTm.wSecond;
  return &y;
}
#endif /*_WIN32_WCE */
#elif defined(__UNIXES__)
#include <sys/time.h>
#endif /* __WINNT__*/
 /*
  * int64 time(void)
  *  Current Unix timestamp
  * Parameters
  *  None.
  * Return
  *  Returns the current time measured in the number of seconds
  *  since the Unix Epoch (January 1 1970 00:00:00 GMT).
  */
static int PH7_builtin_time(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	time_t tt;
	SXUNUSED(nArg); /* cc warning */
	SXUNUSED(apArg);
	/* Extract the current time */
	time(&tt);
	/* Return as 64-bit integer */
	ph7_result_int64(pCtx,(ph7_int64)tt);
	return  PH7_OK;
}
/*
  * string/float microtime([ bool $get_as_float = false ])
  *  microtime() returns the current Unix timestamp with microseconds.
  * Parameters
  *  $get_as_float
  *   If used and set to TRUE, microtime() will return a float instead of a string
  *   as described in the return values section below.
  * Return
  *  By default, microtime() returns a string in the form "msec sec", where sec 
  *  is the current time measured in the number of seconds since the Unix 
  *  epoch (0:00:00 January 1, 1970 GMT), and msec is the number of microseconds
  *  that have elapsed since sec expressed in seconds.
  *  If get_as_float is set to TRUE, then microtime() returns a float, which represents
  *  the current time in seconds since the Unix epoch accurate to the nearest microsecond. 
  */
static int PH7_builtin_microtime(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	int bFloat = 0;
	sytime sTime;	
#if defined(__UNIXES__)
	struct timeval tv;
	gettimeofday(&tv,0);
	sTime.tm_sec  = (long)tv.tv_sec;
	sTime.tm_usec = (long)tv.tv_usec;
#else
	time_t tt;
	time(&tt);
	sTime.tm_sec  = (long)tt;
	sTime.tm_usec = (long)(tt%SX_USEC_PER_SEC);
#endif /* __UNIXES__ */
	if( nArg > 0 ){
		bFloat = ph7_value_to_bool(apArg[0]);
	}
	if( bFloat ){
		/* Return as float */
		ph7_result_double(pCtx,(double)sTime.tm_sec);
	}else{
		/* Return as string */
		ph7_result_string_format(pCtx,"%ld %ld",sTime.tm_usec,sTime.tm_sec);
	}
	return PH7_OK;
}
/*
 * array getdate ([ int $timestamp = time() ])
 *  Get date/time information.
 * Parameter
 *  $timestamp: The optional timestamp parameter is an integer Unix timestamp
 *     that defaults to the current local time if a timestamp is not given.
 *     In other words, it defaults to the value of time().
 * Returns
 *  Returns an associative array of information related to the timestamp.
 *  Elements from the returned associative array are as follows: 
 *   KEY                                                         VALUE
 * ---------                                                    -------
 * "seconds" 	Numeric representation of seconds 	            0 to 59
 * "minutes" 	Numeric representation of minutes 	            0 to 59
 * "hours" 	    Numeric representation of hours 	            0 to 23
 * "mday" 	    Numeric representation of the day of the month 	1 to 31
 * "wday" 	    Numeric representation of the day of the week 	0 (for Sunday) through 6 (for Saturday)
 * "mon" 	    Numeric representation of a month 	            1 through 12
 * "year" 	    A full numeric representation of a year,        4 digits 	Examples: 1999 or 2003
 * "yday" 	    Numeric representation of the day of the year   0 through 365
 * "weekday" 	A full textual representation of the day of the week 	Sunday through Saturday
 * "month" 	    A full textual representation of a month, such as January or March 	January through December
 * 0 	        Seconds since the Unix Epoch, similar to the values returned by time() and used by date(). 
 * NOTE:
 *   NULL is returned on failure.
 */
static int PH7_builtin_getdate(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	ph7_value *pValue,*pArray;
	Sytm sTm;
	if( nArg < 1 ){
#ifdef __WINNT__
		SYSTEMTIME sOS;
		GetSystemTime(&sOS);
		SYSTEMTIME_TO_SYTM(&sOS,&sTm);
#else
		struct tm *pTm;
		time_t t;
		time(&t);
		pTm = localtime(&t);
		STRUCT_TM_TO_SYTM(pTm,&sTm);
#endif
	}else{
		/* Use the given timestamp */
		time_t t;
		struct tm *pTm;
#ifdef __WINNT__
#ifdef _MSC_VER
#if _MSC_VER >= 1400 /* Visual Studio 2005 and up */
#pragma warning(disable:4996) /* _CRT_SECURE...*/
#endif
#endif
#endif
		if( ph7_value_is_int(apArg[0]) ){
			t = (time_t)ph7_value_to_int64(apArg[0]);
			pTm = localtime(&t);
			if( pTm == 0 ){
				time(&t);
			}
		}else{
			time(&t);
		}
		pTm = localtime(&t);
		STRUCT_TM_TO_SYTM(pTm,&sTm);
	}
	/* Element value */
	pValue = ph7_context_new_scalar(pCtx);
	if( pValue == 0 ){
		/* Return NULL */
		ph7_result_null(pCtx);
		return PH7_OK;
	}
	/* Create a new array */
	pArray = ph7_context_new_array(pCtx);
	if( pArray == 0 ){
		/* Return NULL */
		ph7_result_null(pCtx);
		return PH7_OK;
	}
	/* Fill the array */
	/* Seconds */
	ph7_value_int(pValue,sTm.tm_sec);
	ph7_array_add_strkey_elem(pArray,"seconds",pValue);
	/* Minutes */
	ph7_value_int(pValue,sTm.tm_min);
	ph7_array_add_strkey_elem(pArray,"minutes",pValue);
	/* Hours */
	ph7_value_int(pValue,sTm.tm_hour);
	ph7_array_add_strkey_elem(pArray,"hours",pValue);
	/* mday */
	ph7_value_int(pValue,sTm.tm_mday);
	ph7_array_add_strkey_elem(pArray,"mday",pValue);
	/* wday */
	ph7_value_int(pValue,sTm.tm_wday);
	ph7_array_add_strkey_elem(pArray,"wday",pValue);
	/* mon */
	ph7_value_int(pValue,sTm.tm_mon+1);
	ph7_array_add_strkey_elem(pArray,"mon",pValue);
	/* year */
	ph7_value_int(pValue,sTm.tm_year);
	ph7_array_add_strkey_elem(pArray,"year",pValue);
	/* yday */
	ph7_value_int(pValue,sTm.tm_yday);
	ph7_array_add_strkey_elem(pArray,"yday",pValue);
	/* Weekday */
	ph7_value_string(pValue,SyTimeGetDay(sTm.tm_wday),-1);
	ph7_array_add_strkey_elem(pArray,"weekday",pValue);
	/* Month */
	ph7_value_reset_string_cursor(pValue);
	ph7_value_string(pValue,SyTimeGetMonth(sTm.tm_mon),-1);
	ph7_array_add_strkey_elem(pArray,"month",pValue);
	/* Seconds since the epoch */
	ph7_value_int64(pValue,(ph7_int64)time(0));
	ph7_array_add_intkey_elem(pArray,0 /* Index zero */,pValue);
	/* Return the freshly created array */
	ph7_result_value(pCtx,pArray);
	return PH7_OK;
}
/*
 * mixed gettimeofday([ bool $return_float = false ] )
 *   Returns an associative array containing the data returned from the system call.
 * Parameters
 *  $return_float
 *   When set to TRUE, a float instead of an array is returned.
 * Return
 *   By default an array is returned. If return_float is set, then
 *   a float is returned. 
 */
static int PH7_builtin_gettimeofday(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	int bFloat = 0;
	sytime sTime;
#if defined(__UNIXES__)
	struct timeval tv;
	gettimeofday(&tv,0);
	sTime.tm_sec  = (long)tv.tv_sec;
	sTime.tm_usec = (long)tv.tv_usec;
#else
	time_t tt;
	time(&tt);
	sTime.tm_sec  = (long)tt;
	sTime.tm_usec = (long)(tt%SX_USEC_PER_SEC);
#endif /* __UNIXES__ */
	if( nArg > 0 ){
		bFloat = ph7_value_to_bool(apArg[0]);
	}
	if( bFloat ){
		/* Return as float */
		ph7_result_double(pCtx,(double)sTime.tm_sec);
	}else{
		/* Return an associative array */
		ph7_value *pValue,*pArray;
		/* Create a new array */
		pArray = ph7_context_new_array(pCtx);
		/* Element value */
		pValue = ph7_context_new_scalar(pCtx);
		if( pValue == 0 || pArray == 0 ){
			/* Return NULL */
			ph7_result_null(pCtx);
			return PH7_OK;
		}
		/* Fill the array */
		/* sec */
		ph7_value_int64(pValue,sTime.tm_sec);
		ph7_array_add_strkey_elem(pArray,"sec",pValue);
		/* usec */
		ph7_value_int64(pValue,sTime.tm_usec);
		ph7_array_add_strkey_elem(pArray,"usec",pValue);
		/* Return the array */
		ph7_result_value(pCtx,pArray);
	}
	return PH7_OK;
}
/* Check if the given year is leap or not */
#define IS_LEAP_YEAR(YEAR)	(YEAR % 400 ? ( YEAR % 100 ? ( YEAR % 4 ? 0 : 1 ) : 0 ) : 1)
/* ISO-8601 numeric representation of the day of the week */
static const int aISO8601[] = { 7 /* Sunday */,1 /* Monday */,2,3,4,5,6 };
/*
 * Format a given date string.
 * Supported format: (Taken from PHP online docs)
 * character 	Description
 * d          Day of the month 
 * D          A textual representation of a days
 * j          Day of the month without leading zeros
 * l          A full textual representation of the day of the week 	
 * N          ISO-8601 numeric representation of the day of the week 
 * w          Numeric representation of the day of the week
 * z          The day of the year (starting from 0) 	
 * F          A full textual representation of a month, such as January or March
 * m          Numeric representation of a month, with leading zeros 	01 through 12
 * M          A short textual representation of a month, three letters 	Jan through Dec
 * n          Numeric representation of a month, without leading zeros 	1 through 12
 * t          Number of days in the given month 	28 through 31
 * L          Whether it's a leap year 	1 if it is a leap year, 0 otherwise.
 * o          ISO-8601 year number. This has the same value as Y, except that if the ISO week number
 *            (W) belongs to the previous or next year, that year is used instead. (added in PHP 5.1.0) Examples: 1999 or 2003
 * Y          A full numeric representation of a year, 4 digits 	Examples: 1999 or 2003
 * y          A two digit representation of a year 	Examples: 99 or 03
 * a          Lowercase Ante meridiem and Post meridiem 	am or pm
 * A          Uppercase Ante meridiem and Post meridiem 	AM or PM
 * g          12-hour format of an hour without leading zeros 	1 through 12
 * G          24-hour format of an hour without leading zeros 	0 through 23
 * h          12-hour format of an hour with leading zeros 	01 through 12
 * H          24-hour format of an hour with leading zeros 	00 through 23
 * i          Minutes with leading zeros 	00 to 59
 * s          Seconds, with leading zeros 	00 through 59
 * u          Microseconds Example: 654321
 * e          Timezone identifier 	Examples: UTC, GMT, Atlantic/Azores
 * I          (capital i) Whether or not the date is in daylight saving time 	1 if Daylight Saving Time, 0 otherwise.
 * r          RFC 2822 formatted date 	Example: Thu, 21 Dec 2000 16:01:07 +0200
 * U          Seconds since the Unix Epoch (January 1 1970 00:00:00 GMT)
 * S          English ordinal suffix for the day of the month, 2 characters
 * O          Difference to Greenwich time (GMT) in hours
 * Z          Timezone offset in seconds. The offset for timezones west of UTC is always negative, and for those
 *            east of UTC is always positive.
 * c         ISO 8601 date
 */
static sxi32 DateFormat(ph7_context *pCtx,const char *zIn,int nLen,Sytm *pTm)
{
	const char *zEnd = &zIn[nLen];
	const char *zCur;
	/* Start the format process */
	for(;;){
		if( zIn >= zEnd ){
			/* No more input to process */
			break;
		}
		switch(zIn[0]){
		case 'd':
			/* Day of the month, 2 digits with leading zeros */
			ph7_result_string_format(pCtx,"%02d",pTm->tm_mday);
			break;
		case 'D':
			/*A textual representation of a day, three letters*/
			zCur = SyTimeGetDay(pTm->tm_wday);
			ph7_result_string(pCtx,zCur,3);
			break;
		case 'j':
			/*	Day of the month without leading zeros */
			ph7_result_string_format(pCtx,"%d",pTm->tm_mday);
			break;
		case 'l':
			/* A full textual representation of the day of the week */
			zCur = SyTimeGetDay(pTm->tm_wday);
			ph7_result_string(pCtx,zCur,-1/*Compute length automatically*/);
			break;
		case 'N':{
			/* ISO-8601 numeric representation of the day of the week */
			ph7_result_string_format(pCtx,"%d",aISO8601[pTm->tm_wday % 7 ]);
			break;
				 }
		case 'w':
			/*Numeric representation of the day of the week*/
			ph7_result_string_format(pCtx,"%d",pTm->tm_wday);
			break;
		case 'z':
			/*The day of the year*/
			ph7_result_string_format(pCtx,"%d",pTm->tm_yday);
			break;
		case 'F':
			/*A full textual representation of a month, such as January or March*/
			zCur = SyTimeGetMonth(pTm->tm_mon);
			ph7_result_string(pCtx,zCur,-1/*Compute length automatically*/);
			break;
		case 'm':
			/*Numeric representation of a month, with leading zeros*/
			ph7_result_string_format(pCtx,"%02d",pTm->tm_mon + 1);
			break;
		case 'M':
			/*A short textual representation of a month, three letters*/
			zCur = SyTimeGetMonth(pTm->tm_mon);
			ph7_result_string(pCtx,zCur,3);
			break;
		case 'n':
			/*Numeric representation of a month, without leading zeros*/
			ph7_result_string_format(pCtx,"%d",pTm->tm_mon + 1);
			break;
		case 't':{
			static const int aMonDays[] = {31,29,31,30,31,30,31,31,30,31,30,31 };
			int nDays = aMonDays[pTm->tm_mon % 12 ];
			if( pTm->tm_mon == 1 /* 'February' */ && !IS_LEAP_YEAR(pTm->tm_year) ){
				nDays = 28;
			}
			/*Number of days in the given month*/
			ph7_result_string_format(pCtx,"%d",nDays);
			break;
				 }
		case 'L':{
			int isLeap = IS_LEAP_YEAR(pTm->tm_year);
			/* Whether it's a leap year */
			ph7_result_string_format(pCtx,"%d",isLeap);
			break;
				 }
		case 'o':
			/* ISO-8601 year number.*/
			ph7_result_string_format(pCtx,"%4d",pTm->tm_year);
			break;
		case 'Y':
			/*	A full numeric representation of a year, 4 digits */
			ph7_result_string_format(pCtx,"%4d",pTm->tm_year);
			break;
		case 'y':
			/*A two digit representation of a year*/
			ph7_result_string_format(pCtx,"%02d",pTm->tm_year%100);
			break;
		case 'a':
			/*	Lowercase Ante meridiem and Post meridiem */
			ph7_result_string(pCtx,pTm->tm_hour > 12 ? "pm" : "am",2);
			break;
		case 'A':
			/*	Uppercase Ante meridiem and Post meridiem */
			ph7_result_string(pCtx,pTm->tm_hour > 12 ? "PM" : "AM",2);
			break;
		case 'g':
			/*	12-hour format of an hour without leading zeros*/
			ph7_result_string_format(pCtx,"%d",1+(pTm->tm_hour%12));
			break;
		case 'G':
			/* 24-hour format of an hour without leading zeros */
			ph7_result_string_format(pCtx,"%d",pTm->tm_hour);
			break;
		case 'h':
			/* 12-hour format of an hour with leading zeros */
			ph7_result_string_format(pCtx,"%02d",1+(pTm->tm_hour%12));
			break;
		case 'H':
			/*	24-hour format of an hour with leading zeros */
			ph7_result_string_format(pCtx,"%02d",pTm->tm_hour);
			break;
		case 'i':
			/* 	Minutes with leading zeros */
			ph7_result_string_format(pCtx,"%02d",pTm->tm_min);
			break;
		case 's':
			/* 	second with leading zeros */
			ph7_result_string_format(pCtx,"%02d",pTm->tm_sec);
			break;
		case 'u':
			/* 	Microseconds */
			ph7_result_string_format(pCtx,"%u",pTm->tm_sec * SX_USEC_PER_SEC);
			break;
		case 'S':{
			/* English ordinal suffix for the day of the month, 2 characters */
			static const char zSuffix[] = "thstndrdthththththth";
			int v = pTm->tm_mday;
			ph7_result_string(pCtx,&zSuffix[2 * (int)(v / 10 % 10 != 1 ? v % 10 : 0)],(int)sizeof(char) * 2);
			break;
				 }
		case 'e':
			/* 	Timezone identifier */
			zCur = pTm->tm_zone;
			if( zCur == 0 ){
				/* Assume GMT */
				zCur = "GMT";
			}
			ph7_result_string(pCtx,zCur,-1);
			break;
		case 'I':
			/* Whether or not the date is in daylight saving time */
#ifdef __WINNT__
#ifdef _MSC_VER
#ifndef _WIN32_WCE
			_get_daylight(&pTm->tm_isdst);
#endif
#endif
#endif
			ph7_result_string_format(pCtx,"%d",pTm->tm_isdst == 1);
			break;
		case 'r':
			/* RFC 2822 formatted date 	Example: Thu, 21 Dec 2000 16:01:07 */
			ph7_result_string_format(pCtx,"%.3s, %02d %.3s %4d %02d:%02d:%02d",
				SyTimeGetDay(pTm->tm_wday),
				pTm->tm_mday,
				SyTimeGetMonth(pTm->tm_mon),
				pTm->tm_year,
				pTm->tm_hour,
				pTm->tm_min,
				pTm->tm_sec
				);
			break;
		case 'U':{
			time_t tt;
			/* Seconds since the Unix Epoch */
			time(&tt);
			ph7_result_string_format(pCtx,"%u",(unsigned int)tt);
			break;
				 }
		case 'O':
		case 'P':
			/* Difference to Greenwich time (GMT) in hours */
			ph7_result_string_format(pCtx,"%+05d",pTm->tm_gmtoff);
			break;
		case 'Z':
			/* Timezone offset in seconds. The offset for timezones west of UTC
			 * is always negative, and for those east of UTC is always positive.
			 */
			ph7_result_string_format(pCtx,"%+05d",pTm->tm_gmtoff);
			break;
		case 'c':
			/* 	ISO 8601 date */
			ph7_result_string_format(pCtx,"%4d-%02d-%02dT%02d:%02d:%02d%+05d",
				pTm->tm_year,
				pTm->tm_mon+1,
				pTm->tm_mday,
				pTm->tm_hour,
				pTm->tm_min,
				pTm->tm_sec,
				pTm->tm_gmtoff
				);
			break;
		case '\\':
			zIn++;
			/* Expand verbatim */
			if( zIn < zEnd ){
				ph7_result_string(pCtx,zIn,(int)sizeof(char));
			}
			break;
		default:
			/* Unknown format specifer,expand verbatim */
			ph7_result_string(pCtx,zIn,(int)sizeof(char));
			break;
		}
		/* Point to the next character */
		zIn++;
	}
	return SXRET_OK;
}
/*
 * PH7 implementation of the strftime() function.
 * The following formats are supported:
 * %a 	An abbreviated textual representation of the day
 * %A 	A full textual representation of the day
 * %d 	Two-digit day of the month (with leading zeros)
 * %e 	Day of the month, with a space preceding single digits.
 * %j 	Day of the year, 3 digits with leading zeros 
 * %u 	ISO-8601 numeric representation of the day of the week 	1 (for Monday) though 7 (for Sunday)
 * %w 	Numeric representation of the day of the week 0 (for Sunday) through 6 (for Saturday)
 * %U 	Week number of the given year, starting with the first Sunday as the first week
 * %V 	ISO-8601:1988 week number of the given year, starting with the first week of the year with at least
 *   4 weekdays, with Monday being the start of the week.
 * %W 	A numeric representation of the week of the year
 * %b 	Abbreviated month name, based on the locale
 * %B 	Full month name, based on the locale
 * %h 	Abbreviated month name, based on the locale (an alias of %b)
 * %m 	Two digit representation of the month
 * %C 	Two digit representation of the century (year divided by 100, truncated to an integer)
 * %g 	Two digit representation of the year going by ISO-8601:1988 standards (see %V)
 * %G 	The full four-digit version of %g
 * %y 	Two digit representation of the year
 * %Y 	Four digit representation for the year
 * %H 	Two digit representation of the hour in 24-hour format
 * %I 	Two digit representation of the hour in 12-hour format
 * %l (lower-case 'L') 	Hour in 12-hour format, with a space preceeding single digits
 * %M 	Two digit representation of the minute
 * %p 	UPPER-CASE 'AM' or 'PM' based on the given time
 * %P 	lower-case 'am' or 'pm' based on the given time
 * %r 	Same as "%I:%M:%S %p"
 * %R 	Same as "%H:%M"
 * %S 	Two digit representation of the second
 * %T 	Same as "%H:%M:%S"
 * %X 	Preferred time representation based on locale, without the date
 * %z 	Either the time zone offset from UTC or the abbreviation
 * %Z 	The time zone offset/abbreviation option NOT given by %z
 * %c 	Preferred date and time stamp based on local
 * %D 	Same as "%m/%d/%y"
 * %F 	Same as "%Y-%m-%d"
 * %s 	Unix Epoch Time timestamp (same as the time() function)
 * %x 	Preferred date representation based on locale, without the time
 * %n 	A newline character ("\n")
 * %t 	A Tab character ("\t")
 * %% 	A literal percentage character ("%")
 */
static int PH7_Strftime(
	ph7_context *pCtx,  /* Call context */
	const char *zIn,    /* Input string */
	int nLen,           /* Input length */
	Sytm *pTm           /* Parse of the given time */
	)
{
	const char *zCur,*zEnd = &zIn[nLen];
	int c;
	/* Start the format process */
	for(;;){
		zCur = zIn;
		while(zIn < zEnd && zIn[0] != '%' ){
			zIn++;
		}
		if( zIn > zCur ){
			/* Consume input verbatim */
			ph7_result_string(pCtx,zCur,(int)(zIn-zCur));
		}
		zIn++; /* Jump the percent sign */
		if( zIn >= zEnd ){
			/* No more input to process */
			break;
		}
		c = zIn[0];
		/* Act according to the current specifer */
		switch(c){
		case '%':
			/* A literal percentage character ("%") */
			ph7_result_string(pCtx,"%",(int)sizeof(char));
			break;
		case 't':
			/* A Tab character */
			ph7_result_string(pCtx,"\t",(int)sizeof(char));
			break;
		case 'n':
			/* A newline character */
			ph7_result_string(pCtx,"\n",(int)sizeof(char));
			break;
		case 'a':
			/* An abbreviated textual representation of the day */
			ph7_result_string(pCtx,SyTimeGetDay(pTm->tm_wday),(int)sizeof(char)*3);
			break;
		case 'A':
			/* A full textual representation of the day */
			ph7_result_string(pCtx,SyTimeGetDay(pTm->tm_wday),-1/*Compute length automatically*/);
			break;
		case 'e':
			/* Day of the month, 2 digits with leading space for single digit*/
			ph7_result_string_format(pCtx,"%2d",pTm->tm_mday);
			break;
		case 'd':
			/* Two-digit day of the month (with leading zeros) */
			ph7_result_string_format(pCtx,"%02d",pTm->tm_mon+1);
			break;
		case 'j':
			/*The day of the year,3 digits with leading zeros*/
			ph7_result_string_format(pCtx,"%03d",pTm->tm_yday);
			break;
		case 'u':
			/* ISO-8601 numeric representation of the day of the week */
			ph7_result_string_format(pCtx,"%d",aISO8601[pTm->tm_wday % 7 ]);
			break;
		case 'w':
			/* Numeric representation of the day of the week */
			ph7_result_string_format(pCtx,"%d",pTm->tm_wday);
			break;
		case 'b':
		case 'h':
			/*A short textual representation of a month, three letters (Not based on locale)*/
			ph7_result_string(pCtx,SyTimeGetMonth(pTm->tm_mon),(int)sizeof(char)*3);
			break;
		case 'B':
			/* Full month name (Not based on locale) */
			ph7_result_string(pCtx,SyTimeGetMonth(pTm->tm_mon),-1/*Compute length automatically*/);
			break;
		case 'm':
			/*Numeric representation of a month, with leading zeros*/
			ph7_result_string_format(pCtx,"%02d",pTm->tm_mon + 1);
			break;
		case 'C':
			/* Two digit representation of the century */
			ph7_result_string_format(pCtx,"%2d",pTm->tm_year/100);
			break;
		case 'y':
		case 'g':
			/* Two digit representation of the year */
			ph7_result_string_format(pCtx,"%2d",pTm->tm_year%100);
			break;
		case 'Y':
		case 'G':
			/* Four digit representation of the year */
			ph7_result_string_format(pCtx,"%4d",pTm->tm_year);
			break;
		case 'I':
			/* 12-hour format of an hour with leading zeros */
			ph7_result_string_format(pCtx,"%02d",1+(pTm->tm_hour%12));
			break;
		case 'l':
			/* 12-hour format of an hour with leading space */
			ph7_result_string_format(pCtx,"%2d",1+(pTm->tm_hour%12));
			break;
		case 'H':
			/* 24-hour format of an hour with leading zeros */
			ph7_result_string_format(pCtx,"%02d",pTm->tm_hour);
			break;
		case 'M':
			/* Minutes with leading zeros */
			ph7_result_string_format(pCtx,"%02d",pTm->tm_min);
			break;
		case 'S':
			/* Seconds with leading zeros */
			ph7_result_string_format(pCtx,"%02d",pTm->tm_sec);
			break;
		case 'z':
		case 'Z':
			/* 	Timezone identifier */
			zCur = pTm->tm_zone;
			if( zCur == 0 ){
				/* Assume GMT */
				zCur = "GMT";
			}
			ph7_result_string(pCtx,zCur,-1);
			break;
		case 'T':
		case 'X':
			/* Same as "%H:%M:%S" */
			ph7_result_string_format(pCtx,"%02d:%02d:%02d",pTm->tm_hour,pTm->tm_min,pTm->tm_sec);
			break;
		case 'R':
			/* Same as "%H:%M" */
			ph7_result_string_format(pCtx,"%02d:%02d",pTm->tm_hour,pTm->tm_min);
			break;
		case 'P':
			/*	Lowercase Ante meridiem and Post meridiem */
			ph7_result_string(pCtx,pTm->tm_hour > 12 ? "pm" : "am",(int)sizeof(char)*2);
			break;
		case 'p':
			/*	Uppercase Ante meridiem and Post meridiem */
			ph7_result_string(pCtx,pTm->tm_hour > 12 ? "PM" : "AM",(int)sizeof(char)*2);
			break;
		case 'r':
			/* Same as "%I:%M:%S %p" */
			ph7_result_string_format(pCtx,"%02d:%02d:%02d %s",
				1+(pTm->tm_hour%12),
				pTm->tm_min,
				pTm->tm_sec,
				pTm->tm_hour > 12 ? "PM" : "AM"
				);
			break;
		case 'D':
		case 'x':
			/* Same as "%m/%d/%y" */
			ph7_result_string_format(pCtx,"%02d/%02d/%02d",
				pTm->tm_mon+1,
				pTm->tm_mday,
				pTm->tm_year%100
				);
			break;
		case 'F':
			/* Same as "%Y-%m-%d" */
			ph7_result_string_format(pCtx,"%d-%02d-%02d",
				pTm->tm_year,
				pTm->tm_mon+1,
				pTm->tm_mday
				);
			break;
		case 'c':
			ph7_result_string_format(pCtx,"%d-%02d-%02d %02d:%02d:%02d",
				pTm->tm_year,
				pTm->tm_mon+1,
				pTm->tm_mday,
				pTm->tm_hour,
				pTm->tm_min,
				pTm->tm_sec
				);
			break;
		case 's':{
			time_t tt;
			/* Seconds since the Unix Epoch */
			time(&tt);
			ph7_result_string_format(pCtx,"%u",(unsigned int)tt);
			break;
				 }
		default:
			/* unknown specifer,simply ignore*/
			break;
		}
		/* Advance the cursor */
		zIn++;
	}
	return SXRET_OK;
}
/*
 * string date(string $format [, int $timestamp = time() ] )
 *  Returns a string formatted according to the given format string using
 *  the given integer timestamp or the current time if no timestamp is given.
 *  In other words, timestamp is optional and defaults to the value of time(). 
 * Parameters
 *  $format
 *   The format of the outputted date string (See code above)
 * $timestamp
 *   The optional timestamp parameter is an integer Unix timestamp
 *   that defaults to the current local time if a timestamp is not given.
 *   In other words, it defaults to the value of time(). 
 * Return
 *  A formatted date string. If a non-numeric value is used for timestamp, FALSE is returned.
 */
static int PH7_builtin_date(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	const char *zFormat;
	int nLen;
	Sytm sTm;
	if( nArg < 1 || !ph7_value_is_string(apArg[0]) ){
		/* Missing/Invalid argument,return FALSE */
		ph7_result_bool(pCtx,0);
		return PH7_OK;
	}
	zFormat = ph7_value_to_string(apArg[0],&nLen);
	if( nLen < 1 ){
		/* Don't bother processing return the empty string */
		ph7_result_string(pCtx,"",0);
	}
	if( nArg < 2 ){
#ifdef __WINNT__
		SYSTEMTIME sOS;
		GetSystemTime(&sOS);
		SYSTEMTIME_TO_SYTM(&sOS,&sTm);
#else
		struct tm *pTm;
		time_t t;
		time(&t);
		pTm = localtime(&t);
		STRUCT_TM_TO_SYTM(pTm,&sTm);
#endif
	}else{
		/* Use the given timestamp */
		time_t t;
		struct tm *pTm;
		if( ph7_value_is_int(apArg[1]) ){
			t = (time_t)ph7_value_to_int64(apArg[1]);
			pTm = localtime(&t);
			if( pTm == 0 ){
				time(&t);
			}
		}else{
			time(&t);
		}
		pTm = localtime(&t);
		STRUCT_TM_TO_SYTM(pTm,&sTm);
	}
	/* Format the given string */
	DateFormat(pCtx,zFormat,nLen,&sTm);
	return PH7_OK;
}
/*
 * string strftime(string $format [, int $timestamp = time() ] )
 *  Format a local time/date (PLATFORM INDEPENDANT IMPLEENTATION NOT BASED ON LOCALE)
 * Parameters
 *  $format
 *   The format of the outputted date string (See code above)
 * $timestamp
 *   The optional timestamp parameter is an integer Unix timestamp
 *   that defaults to the current local time if a timestamp is not given.
 *   In other words, it defaults to the value of time(). 
 * Return
 * Returns a string formatted according format using the given timestamp
 * or the current local time if no timestamp is given.
 */
static int PH7_builtin_strftime(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	const char *zFormat;
	int nLen;
	Sytm sTm;
	if( nArg < 1 || !ph7_value_is_string(apArg[0]) ){
		/* Missing/Invalid argument,return FALSE */
		ph7_result_bool(pCtx,0);
		return PH7_OK;
	}
	zFormat = ph7_value_to_string(apArg[0],&nLen);
	if( nLen < 1 ){
		/* Don't bother processing return FALSE */
		ph7_result_bool(pCtx,0);
	}
	if( nArg < 2 ){
#ifdef __WINNT__
		SYSTEMTIME sOS;
		GetSystemTime(&sOS);
		SYSTEMTIME_TO_SYTM(&sOS,&sTm);
#else
		struct tm *pTm;
		time_t t;
		time(&t);
		pTm = localtime(&t);
		STRUCT_TM_TO_SYTM(pTm,&sTm);
#endif
	}else{
		/* Use the given timestamp */
		time_t t;
		struct tm *pTm;
		if( ph7_value_is_int(apArg[1]) ){
			t = (time_t)ph7_value_to_int64(apArg[1]);
			pTm = localtime(&t);
			if( pTm == 0 ){
				time(&t);
			}
		}else{
			time(&t);
		}
		pTm = localtime(&t);
		STRUCT_TM_TO_SYTM(pTm,&sTm);
	}
	/* Format the given string */
	PH7_Strftime(pCtx,zFormat,nLen,&sTm);
	if( ph7_context_result_buf_length(pCtx) < 1 ){
		/* Nothing was formatted,return FALSE */
		ph7_result_bool(pCtx,0);
	}
	return PH7_OK;
}
/*
 * string gmdate(string $format [, int $timestamp = time() ] )
 *  Identical to the date() function except that the time returned
 *  is Greenwich Mean Time (GMT).
 * Parameters
 *  $format
 *  The format of the outputted date string (See code above)
 *  $timestamp
 *   The optional timestamp parameter is an integer Unix timestamp
 *   that defaults to the current local time if a timestamp is not given.
 *   In other words, it defaults to the value of time(). 
 * Return
 *  A formatted date string. If a non-numeric value is used for timestamp, FALSE is returned.
 */
static int PH7_builtin_gmdate(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	const char *zFormat;
	int nLen;
	Sytm sTm;
	if( nArg < 1 || !ph7_value_is_string(apArg[0]) ){
		/* Missing/Invalid argument,return FALSE */
		ph7_result_bool(pCtx,0);
		return PH7_OK;
	}
	zFormat = ph7_value_to_string(apArg[0],&nLen);
	if( nLen < 1 ){
		/* Don't bother processing return the empty string */
		ph7_result_string(pCtx,"",0);
	}
	if( nArg < 2 ){
#ifdef __WINNT__
		SYSTEMTIME sOS;
		GetSystemTime(&sOS);
		SYSTEMTIME_TO_SYTM(&sOS,&sTm);
#else
		struct tm *pTm;
		time_t t;
		time(&t);
		pTm = gmtime(&t);
		STRUCT_TM_TO_SYTM(pTm,&sTm);
#endif
	}else{
		/* Use the given timestamp */
		time_t t;
		struct tm *pTm;
		if( ph7_value_is_int(apArg[1]) ){
			t = (time_t)ph7_value_to_int64(apArg[1]);
			pTm = gmtime(&t);
			if( pTm == 0 ){
				time(&t);
			}
		}else{
			time(&t);
		}
		pTm = gmtime(&t);
		STRUCT_TM_TO_SYTM(pTm,&sTm);
	}
	/* Format the given string */
	DateFormat(pCtx,zFormat,nLen,&sTm);
	return PH7_OK;
}
/*
 * array localtime([ int $timestamp = time() [, bool $is_associative = false ]])
 *  Return the local time.
 * Parameter
 *  $timestamp: The optional timestamp parameter is an integer Unix timestamp
 *     that defaults to the current local time if a timestamp is not given.
 *     In other words, it defaults to the value of time().
 * $is_associative
 *   If set to FALSE or not supplied then the array is returned as a regular, numerically
 *   indexed array. If the argument is set to TRUE then localtime() returns an associative
 *   array containing all the different elements of the structure returned by the C function
 *   call to localtime. The names of the different keys of the associative array are as follows:
 *      "tm_sec" - seconds, 0 to 59
 *      "tm_min" - minutes, 0 to 59
 *      "tm_hour" - hours, 0 to 23
 *      "tm_mday" - day of the month, 1 to 31
 *      "tm_mon" - month of the year, 0 (Jan) to 11 (Dec)
 *      "tm_year" - years since 1900
 *      "tm_wday" - day of the week, 0 (Sun) to 6 (Sat)
 *      "tm_yday" - day of the year, 0 to 365
 *      "tm_isdst" - is daylight savings time in effect? Positive if yes, 0 if not, negative if unknown.
 * Returns
 *  An associative array of information related to the timestamp.
 */
static int PH7_builtin_localtime(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	ph7_value *pValue,*pArray;
	int isAssoc = 0;
	Sytm sTm;
	if( nArg < 1 ){
#ifdef __WINNT__
		SYSTEMTIME sOS;
		GetSystemTime(&sOS); /* TODO(chems): GMT not local */
		SYSTEMTIME_TO_SYTM(&sOS,&sTm);
#else
		struct tm *pTm;
		time_t t;
		time(&t);
		pTm = localtime(&t);
		STRUCT_TM_TO_SYTM(pTm,&sTm);
#endif
	}else{
		/* Use the given timestamp */
		time_t t;
		struct tm *pTm;
		if( ph7_value_is_int(apArg[0]) ){
			t = (time_t)ph7_value_to_int64(apArg[0]);
			pTm = localtime(&t);
			if( pTm == 0 ){
				time(&t);
			}
		}else{
			time(&t);
		}
		pTm = localtime(&t);
		STRUCT_TM_TO_SYTM(pTm,&sTm);
	}
	/* Element value */
	pValue = ph7_context_new_scalar(pCtx);
	if( pValue == 0 ){
		/* Return NULL */
		ph7_result_null(pCtx);
		return PH7_OK;
	}
	/* Create a new array */
	pArray = ph7_context_new_array(pCtx);
	if( pArray == 0 ){
		/* Return NULL */
		ph7_result_null(pCtx);
		return PH7_OK;
	}
	if( nArg > 1 ){
		isAssoc = ph7_value_to_bool(apArg[1]); 
	}
	/* Fill the array */
	/* Seconds */
	ph7_value_int(pValue,sTm.tm_sec);
	if( isAssoc ){
		ph7_array_add_strkey_elem(pArray,"tm_sec",pValue);
	}else{
		ph7_array_add_elem(pArray,0/* Automatic index */,pValue);
	}
	/* Minutes */
	ph7_value_int(pValue,sTm.tm_min);
	if( isAssoc ){
		ph7_array_add_strkey_elem(pArray,"tm_min",pValue);
	}else{
		ph7_array_add_elem(pArray,0/* Automatic index */,pValue);
	}
	/* Hours */
	ph7_value_int(pValue,sTm.tm_hour);
	if( isAssoc ){
		ph7_array_add_strkey_elem(pArray,"tm_hour",pValue);
	}else{
		ph7_array_add_elem(pArray,0/* Automatic index */,pValue);
	}
	/* mday */
	ph7_value_int(pValue,sTm.tm_mday);
	if( isAssoc ){
		ph7_array_add_strkey_elem(pArray,"tm_mday",pValue);
	}else{
		ph7_array_add_elem(pArray,0/* Automatic index */,pValue);
	}
	/* mon */
	ph7_value_int(pValue,sTm.tm_mon);
	if( isAssoc ){
		ph7_array_add_strkey_elem(pArray,"tm_mon",pValue);
	}else{
		ph7_array_add_elem(pArray,0/* Automatic index */,pValue);
	}
	/* year since 1900 */
	ph7_value_int(pValue,sTm.tm_year-1900);
	if( isAssoc ){
		ph7_array_add_strkey_elem(pArray,"tm_year",pValue);
	}else{
		ph7_array_add_elem(pArray,0/* Automatic index */,pValue);
	}
	/* wday */
	ph7_value_int(pValue,sTm.tm_wday);
	if( isAssoc ){
		ph7_array_add_strkey_elem(pArray,"tm_wday",pValue);
	}else{
		ph7_array_add_elem(pArray,0/* Automatic index */,pValue);
	}
	/* yday */
	ph7_value_int(pValue,sTm.tm_yday);
	if( isAssoc ){
		ph7_array_add_strkey_elem(pArray,"tm_yday",pValue);
	}else{
		ph7_array_add_elem(pArray,0/* Automatic index */,pValue);
	}
	/* isdst */
#ifdef __WINNT__
#ifdef _MSC_VER
#ifndef _WIN32_WCE
			_get_daylight(&sTm.tm_isdst);
#endif
#endif
#endif
	ph7_value_int(pValue,sTm.tm_isdst);
	if( isAssoc ){
		ph7_array_add_strkey_elem(pArray,"tm_isdst",pValue);
	}else{
		ph7_array_add_elem(pArray,0/* Automatic index */,pValue);
	}
	/* Return the array */
	ph7_result_value(pCtx,pArray);
	return PH7_OK;
}
/*
 * int idate(string $format [, int $timestamp = time() ])
 *  Returns a number formatted according to the given format string
 *  using the given integer timestamp or the current local time if 
 *  no timestamp is given. In other words, timestamp is optional and defaults
 *  to the value of time().
 *  Unlike the function date(), idate() accepts just one char in the format
 *  parameter.
 * $Parameters
 *  Supported format
 *   d 	Day of the month
 *   h 	Hour (12 hour format)
 *   H 	Hour (24 hour format)
 *   i 	Minutes
 *   I (uppercase i)1 if DST is activated, 0 otherwise
 *   L (uppercase l) returns 1 for leap year, 0 otherwise
 *   m 	Month number
 *   s 	Seconds
 *   t 	Days in current month
 *   U 	Seconds since the Unix Epoch - January 1 1970 00:00:00 UTC - this is the same as time()
 *   w 	Day of the week (0 on Sunday)
 *   W 	ISO-8601 week number of year, weeks starting on Monday
 *   y 	Year (1 or 2 digits - check note below)
 *   Y 	Year (4 digits)
 *   z 	Day of the year
 *   Z 	Timezone offset in seconds
 * $timestamp
 *  The optional timestamp parameter is an integer Unix timestamp that defaults
 *  to the current local time if a timestamp is not given. In other words, it defaults
 *  to the value of time(). 
 * Return
 *  An integer. 
 */
static int PH7_builtin_idate(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	const char *zFormat;
	ph7_int64 iVal = 0;
	int nLen;
	Sytm sTm;
	if( nArg < 1 || !ph7_value_is_string(apArg[0]) ){
		/* Missing/Invalid argument,return -1 */
		ph7_result_int(pCtx,-1);
		return PH7_OK;
	}
	zFormat = ph7_value_to_string(apArg[0],&nLen);
	if( nLen < 1 ){
		/* Don't bother processing return -1*/
		ph7_result_int(pCtx,-1);
	}
	if( nArg < 2 ){
#ifdef __WINNT__
		SYSTEMTIME sOS;
		GetSystemTime(&sOS);
		SYSTEMTIME_TO_SYTM(&sOS,&sTm);
#else
		struct tm *pTm;
		time_t t;
		time(&t);
		pTm = localtime(&t);
		STRUCT_TM_TO_SYTM(pTm,&sTm);
#endif
	}else{
		/* Use the given timestamp */
		time_t t;
		struct tm *pTm;
		if( ph7_value_is_int(apArg[1]) ){
			t = (time_t)ph7_value_to_int64(apArg[1]);
			pTm = localtime(&t);
			if( pTm == 0 ){
				time(&t);
			}
		}else{
			time(&t);
		}
		pTm = localtime(&t);
		STRUCT_TM_TO_SYTM(pTm,&sTm);
	}
	/* Perform the requested operation */
	switch(zFormat[0]){
	case 'd':
		/* Day of the month */
		iVal = sTm.tm_mday;
		break;
	case 'h':
		/*	Hour (12 hour format)*/
		iVal = 1 + (sTm.tm_hour % 12);
		break;
	case 'H':
		/* Hour (24 hour format)*/
		iVal = sTm.tm_hour;
		break;
	case 'i':
		/*Minutes*/
		iVal = sTm.tm_min;
		break;
	case 'I':
		/*	returns 1 if DST is activated, 0 otherwise */
#ifdef __WINNT__
#ifdef _MSC_VER
#ifndef _WIN32_WCE
			_get_daylight(&sTm.tm_isdst);
#endif
#endif
#endif
		iVal = sTm.tm_isdst;
		break;
	case 'L':
		/* 	returns 1 for leap year, 0 otherwise */
		iVal = IS_LEAP_YEAR(sTm.tm_year);
		break;
	case 'm':
		/* Month number*/
		iVal = sTm.tm_mon;
		break;
	case 's':
		/*Seconds*/
		iVal = sTm.tm_sec;
		break;
	case 't':{
		/*Days in current month*/
		static const int aMonDays[] = {31,29,31,30,31,30,31,31,30,31,30,31 };
		int nDays = aMonDays[sTm.tm_mon % 12 ];
		if( sTm.tm_mon == 1 /* 'February' */ && !IS_LEAP_YEAR(sTm.tm_year) ){
			nDays = 28;
		}
		iVal = nDays;
		break;
			 }
	case 'U':
		/*Seconds since the Unix Epoch*/
		iVal = (ph7_int64)time(0);
		break;
	case 'w':
		/*	Day of the week (0 on Sunday) */
		iVal = sTm.tm_wday;
		break;
	case 'W': {
		/* ISO-8601 week number of year, weeks starting on Monday */
		static const int aISO8601[] = { 7 /* Sunday */,1 /* Monday */,2,3,4,5,6 };
		iVal = aISO8601[sTm.tm_wday % 7 ];
		break;
			  }
	case 'y':
		/* Year (2 digits) */
		iVal = sTm.tm_year % 100;
		break;
	case 'Y':
		/* Year (4 digits) */
		iVal = sTm.tm_year;
		break;
	case 'z':
		/* Day of the year */
		iVal = sTm.tm_yday;
		break;
	case 'Z':
		/*Timezone offset in seconds*/
		iVal = sTm.tm_gmtoff;
		break;
	default:
		/* unknown format,throw a warning */
		ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"Unknown date format token");
		break;
	}
	/* Return the time value */
	ph7_result_int64(pCtx,iVal);
	return PH7_OK;
}
/*
 * int mktime/gmmktime([ int $hour = date("H") [, int $minute = date("i") [, int $second = date("s") 
 *  [, int $month = date("n") [, int $day = date("j") [, int $year = date("Y") [, int $is_dst = -1 ]]]]]]] )
 *  Returns the Unix timestamp corresponding to the arguments given. This timestamp is a 64bit integer 
 *  containing the number of seconds between the Unix Epoch (January 1 1970 00:00:00 GMT) and the time
 *  specified.
 *  Arguments may be left out in order from right to left; any arguments thus omitted will be set to
 *  the current value according to the local date and time.
 * Parameters
 * $hour
 *  The number of the hour relevant to the start of the day determined by month, day and year.
 *  Negative values reference the hour before midnight of the day in question. Values greater
 *  than 23 reference the appropriate hour in the following day(s).
 * $minute
 *  The number of the minute relevant to the start of the hour. Negative values reference
 *  the minute in the previous hour. Values greater than 59 reference the appropriate minute
 *  in the following hour(s).
 * $second
 *  The number of seconds relevant to the start of the minute. Negative values reference 
 *  the second in the previous minute. Values greater than 59 reference the appropriate 
 * second in the following minute(s).
 * $month
 *  The number of the month relevant to the end of the previous year. Values 1 to 12 reference
 *  the normal calendar months of the year in question. Values less than 1 (including negative values)
 *  reference the months in the previous year in reverse order, so 0 is December, -1 is November)...
 * $day
 *  The number of the day relevant to the end of the previous month. Values 1 to 28, 29, 30 or 31 
 *  (depending upon the month) reference the normal days in the relevant month. Values less than 1
 *  (including negative values) reference the days in the previous month, so 0 is the last day 
 *  of the previous month, -1 is the day before that, etc. Values greater than the number of days
 *  in the relevant month reference the appropriate day in the following month(s).
 * $year
 *  The number of the year, may be a two or four digit value, with values between 0-69 mapping
 *  to 2000-2069 and 70-100 to 1970-2000. On systems where time_t is a 32bit signed integer, as 
 *  most common today, the valid range for year is somewhere between 1901 and 2038.
 * $is_dst
 *  This parameter can be set to 1 if the time is during daylight savings time (DST), 0 if it is not,
 *  or -1 (the default) if it is unknown whether the time is within daylight savings time or not. 
 * Return
 *   mktime() returns the Unix timestamp of the arguments given. 
 *   If the arguments are invalid, the function returns FALSE
 */
static int PH7_builtin_mktime(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	const char *zFunction;
	ph7_int64 iVal = 0;
	struct tm *pTm;
	time_t t;
	/* Extract function name */
	zFunction = ph7_function_name(pCtx);
	/* Get the current time */
	time(&t);
	if( zFunction[0] == 'g' /* gmmktime */ ){
		pTm = gmtime(&t);
	}else{
		/* localtime */
		pTm = localtime(&t);
	}
	if( nArg > 0 ){
		int iVal;
		/* Hour */
		iVal = ph7_value_to_int(apArg[0]);
		pTm->tm_hour = iVal;
		if( nArg > 1 ){
			/* Minutes */
			iVal = ph7_value_to_int(apArg[1]);
			pTm->tm_min = iVal;
			if( nArg > 2 ){
				/* Seconds */
				iVal = ph7_value_to_int(apArg[2]);
				pTm->tm_sec = iVal;
				if( nArg > 3 ){
					/* Month */
					iVal = ph7_value_to_int(apArg[3]);
					pTm->tm_mon = iVal - 1;
					if( nArg > 4 ){
						/* mday */
						iVal = ph7_value_to_int(apArg[4]);
						pTm->tm_mday = iVal;
						if( nArg > 5 ){
							/* Year */
							iVal = ph7_value_to_int(apArg[5]);
							if( iVal > 1900 ){
								iVal -= 1900;
							}
							pTm->tm_year = iVal;
							if( nArg > 6 ){
								/* is_dst */
								iVal = ph7_value_to_bool(apArg[6]);
								pTm->tm_isdst = iVal;
							}
						}
					}
				}
			}
		}
	}
	/* Make the time */
	iVal = (ph7_int64)mktime(pTm);
	/* Return the timesatmp as a 64bit integer */
	ph7_result_int64(pCtx,iVal);
	return PH7_OK;
}
/*
 * Section:
 *    URL handling Functions.
 * Authors:
 *    Symisc Systems,devel@symisc.net.
 *    Copyright (C) Symisc Systems,http://ph7.symisc.net
 * Status:
 *    Stable.
 */
/*
 * Output consumer callback for the standard Symisc routines.
 * [i.e: SyBase64Encode(),SyBase64Decode(),SyUriEncode(),...].
 */
static int Consumer(const void *pData,unsigned int nLen,void *pUserData)
{
	/* Store in the call context result buffer */
	ph7_result_string((ph7_context *)pUserData,(const char *)pData,(int)nLen);
	return SXRET_OK;
}
/*
 * string base64_encode(string $data)
 * string convert_uuencode(string $data)  
 *  Encodes data with MIME base64
 * Parameter
 *  $data
 *    Data to encode
 * Return
 *  Encoded data or FALSE on failure.
 */
static int PH7_builtin_base64_encode(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	const char *zIn;
	int nLen;
	if( nArg < 1 ){
		/* Missing arguments,return FALSE */
		ph7_result_bool(pCtx,0);
		return PH7_OK;
	}
	/* Extract the input string */
	zIn = ph7_value_to_string(apArg[0],&nLen);
	if( nLen < 1 ){
		/* Nothing to process,return FALSE */
		ph7_result_bool(pCtx,0);
		return PH7_OK;
	}
	/* Perform the BASE64 encoding */
	SyBase64Encode(zIn,(sxu32)nLen,Consumer,pCtx);
	return PH7_OK;
}
/*
 * string base64_decode(string $data)
 * string convert_uudecode(string $data)
 *  Decodes data encoded with MIME base64
 * Parameter
 *  $data
 *    Encoded data.
 * Return
 *  Returns the original data or FALSE on failure.
 */
static int PH7_builtin_base64_decode(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	const char *zIn;
	int nLen;
	if( nArg < 1 ){
		/* Missing arguments,return FALSE */
		ph7_result_bool(pCtx,0);
		return PH7_OK;
	}
	/* Extract the input string */
	zIn = ph7_value_to_string(apArg[0],&nLen);
	if( nLen < 1 ){
		/* Nothing to process,return FALSE */
		ph7_result_bool(pCtx,0);
		return PH7_OK;
	}
	/* Perform the BASE64 decoding */
	SyBase64Decode(zIn,(sxu32)nLen,Consumer,pCtx);
	return PH7_OK;
}
/*
 * string urlencode(string $str)
 *  URL encoding
 * Parameter
 *  $data
 *   Input string.
 * Return
 *  Returns a string in which all non-alphanumeric characters except -_. have
 *  been replaced with a percent (%) sign followed by two hex digits and spaces
 *  encoded as plus (+) signs.
 */
static int PH7_builtin_urlencode(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	const char *zIn;
	int nLen;
	if( nArg < 1 ){
		/* Missing arguments,return FALSE */
		ph7_result_bool(pCtx,0);
		return PH7_OK;
	}
	/* Extract the input string */
	zIn = ph7_value_to_string(apArg[0],&nLen);
	if( nLen < 1 ){
		/* Nothing to process,return FALSE */
		ph7_result_bool(pCtx,0);
		return PH7_OK;
	}
	/* Perform the URL encoding */
	SyUriEncode(zIn,(sxu32)nLen,Consumer,pCtx);
	return PH7_OK;
}
/*
 * string urldecode(string $str)
 *  Decodes any %## encoding in the given string.
 *  Plus symbols ('+') are decoded to a space character. 
 * Parameter
 *  $data
 *    Input string.
 * Return
 *  Decoded URL or FALSE on failure.
 */
static int PH7_builtin_urldecode(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	const char *zIn;
	int nLen;
	if( nArg < 1 ){
		/* Missing arguments,return FALSE */
		ph7_result_bool(pCtx,0);
		return PH7_OK;
	}
	/* Extract the input string */
	zIn = ph7_value_to_string(apArg[0],&nLen);
	if( nLen < 1 ){
		/* Nothing to process,return FALSE */
		ph7_result_bool(pCtx,0);
		return PH7_OK;
	}
	/* Perform the URL decoding */
	SyUriDecode(zIn,(sxu32)nLen,Consumer,pCtx,TRUE);
	return PH7_OK;
}
#endif /* PH7_DISABLE_BUILTIN_FUNC */
/* Table of the built-in functions */
static const ph7_builtin_func aBuiltInFunc[] = {
	   /* Variable handling functions */
	{ "is_bool"    , PH7_builtin_is_bool     },
	{ "is_float"   , PH7_builtin_is_float    },
	{ "is_real"    , PH7_builtin_is_float    },
	{ "is_double"  , PH7_builtin_is_float    },
	{ "is_int"     , PH7_builtin_is_int      },
	{ "is_integer" , PH7_builtin_is_int      },
	{ "is_long"    , PH7_builtin_is_int      },
	{ "is_string"  , PH7_builtin_is_string   },
	{ "is_null"    , PH7_builtin_is_null     },
	{ "is_numeric" , PH7_builtin_is_numeric  },
	{ "is_scalar"  , PH7_builtin_is_scalar   },
	{ "is_array"   , PH7_builtin_is_array    },
	{ "is_object"  , PH7_builtin_is_object   },
	{ "is_resource", PH7_builtin_is_resource },
	{ "douleval"   , PH7_builtin_floatval    },
	{ "floatval"   , PH7_builtin_floatval    },
	{ "intval"     , PH7_builtin_intval      },
	{ "strval"     , PH7_builtin_strval      },
	{ "empty"      , PH7_builtin_empty       },
#ifndef PH7_DISABLE_BUILTIN_FUNC
#ifdef PH7_ENABLE_MATH_FUNC
	   /* Math functions */
	{ "abs"  ,    PH7_builtin_abs          },
	{ "sqrt" ,    PH7_builtin_sqrt         },
	{ "exp"  ,    PH7_builtin_exp          },
	{ "floor",    PH7_builtin_floor        },
	{ "cos"  ,    PH7_builtin_cos          },
	{ "sin"  ,    PH7_builtin_sin          },
	{ "acos" ,    PH7_builtin_acos         },
	{ "asin" ,    PH7_builtin_asin         },
	{ "cosh" ,    PH7_builtin_cosh         },
	{ "sinh" ,    PH7_builtin_sinh         },
	{ "ceil" ,    PH7_builtin_ceil         },
	{ "tan"  ,    PH7_builtin_tan          },
	{ "tanh" ,    PH7_builtin_tanh         },
	{ "atan" ,    PH7_builtin_atan         },
	{ "atan2",    PH7_builtin_atan2        },
	{ "log"  ,    PH7_builtin_log          },
	{ "log10" ,   PH7_builtin_log10        },
	{ "pow"  ,    PH7_builtin_pow          },
	{ "pi",       PH7_builtin_pi           },
	{ "fmod",     PH7_builtin_fmod         },
	{ "hypot",    PH7_builtin_hypot        },
#endif /* PH7_ENABLE_MATH_FUNC */
	{ "round",    PH7_builtin_round        },
	{ "dechex", PH7_builtin_dechex         },
	{ "decoct", PH7_builtin_decoct         },
	{ "decbin", PH7_builtin_decbin         },
	{ "hexdec", PH7_builtin_hexdec         },
	{ "bindec", PH7_builtin_bindec         },
	{ "octdec", PH7_builtin_octdec         },
	{ "srand",  PH7_builtin_srand          },
	{ "mt_srand",PH7_builtin_srand         },
	{ "base_convert", PH7_builtin_base_convert },
	   /* String handling functions */
	{ "substr",          PH7_builtin_substr     },
	{ "substr_compare",  PH7_builtin_substr_compare },
	{ "substr_count",    PH7_builtin_substr_count },
	{ "chunk_split",     PH7_builtin_chunk_split},
	{ "addslashes" ,     PH7_builtin_addslashes },
	{ "addcslashes",     PH7_builtin_addcslashes},
	{ "quotemeta",       PH7_builtin_quotemeta  },
	{ "stripslashes",    PH7_builtin_stripslashes },
	{ "htmlspecialchars",PH7_builtin_htmlspecialchars },
	{ "htmlspecialchars_decode", PH7_builtin_htmlspecialchars_decode },
	{ "get_html_translation_table",PH7_builtin_get_html_translation_table },
	{ "htmlentities",PH7_builtin_htmlentities},
	{ "html_entity_decode", PH7_builtin_html_entity_decode},
	{ "strlen"     , PH7_builtin_strlen     },
	{ "strcmp"     , PH7_builtin_strcmp     },
	{ "strcoll"    , PH7_builtin_strcmp     },
	{ "strncmp"    , PH7_builtin_strncmp    },
	{ "strcasecmp" , PH7_builtin_strcasecmp },
	{ "strncasecmp", PH7_builtin_strncasecmp},
	{ "implode"    , PH7_builtin_implode    },
	{ "join"       , PH7_builtin_implode    },
	{ "implode_recursive" , PH7_builtin_implode_recursive },
	{ "join_recursive"    , PH7_builtin_implode_recursive },
	{ "explode"     , PH7_builtin_explode    },
	{ "trim"        , PH7_builtin_trim       },
	{ "rtrim"       , PH7_builtin_rtrim      },
	{ "chop"        , PH7_builtin_rtrim      },
	{ "ltrim"       , PH7_builtin_ltrim      },
	{ "strtolower",   PH7_builtin_strtolower },
	{ "mb_strtolower",PH7_builtin_strtolower }, /* Only UTF-8 encoding is supported */
	{ "strtoupper",   PH7_builtin_strtoupper },
	{ "mb_strtoupper",PH7_builtin_strtoupper }, /* Only UTF-8 encoding is supported */
	{ "ucfirst",      PH7_builtin_ucfirst    },
	{ "lcfirst",      PH7_builtin_lcfirst    },
	{ "ord",          PH7_builtin_ord        },
	{ "chr",          PH7_builtin_chr        },
	{ "bin2hex",      PH7_builtin_bin2hex    },
	{ "strstr",       PH7_builtin_strstr     },
	{ "stristr",      PH7_builtin_stristr    },
	{ "strchr",       PH7_builtin_strstr     },
	{ "strpos",       PH7_builtin_strpos     },
	{ "stripos",      PH7_builtin_stripos    },
	{ "strrpos",      PH7_builtin_strrpos    },
	{ "strripos",     PH7_builtin_strripos   },
	{ "strrchr",      PH7_builtin_strrchr    },
	{ "strrev",       PH7_builtin_strrev     },
	{ "ucwords",      PH7_builtin_ucwords    },
	{ "str_repeat",   PH7_builtin_str_repeat },
	{ "nl2br",        PH7_builtin_nl2br      },
	{ "sprintf",      PH7_builtin_sprintf    },
	{ "printf",       PH7_builtin_printf     },
	{ "vprintf",      PH7_builtin_vprintf    },
	{ "vsprintf",     PH7_builtin_vsprintf   },
	{ "size_format",  PH7_builtin_size_format},
#if !defined(PH7_DISABLE_HASH_FUNC)
	{ "md5",          PH7_builtin_md5       },
	{ "sha1",         PH7_builtin_sha1      },
	{ "crc32",        PH7_builtin_crc32     },
#endif /* PH7_DISABLE_HASH_FUNC */
	{ "str_getcsv",   PH7_builtin_str_getcsv },
	{ "strip_tags",   PH7_builtin_strip_tags },
	{ "str_shuffle",  PH7_builtin_str_shuffle},
	{ "str_split",    PH7_builtin_str_split  },
	{ "strspn",       PH7_builtin_strspn     },
	{ "strcspn",      PH7_builtin_strcspn    },
	{ "strpbrk",      PH7_builtin_strpbrk    },
	{ "soundex",      PH7_builtin_soundex    },
	{ "wordwrap",     PH7_builtin_wordwrap   },
	{ "strtok",       PH7_builtin_strtok     },
	{ "str_pad",      PH7_builtin_str_pad    },
	{ "str_replace",  PH7_builtin_str_replace},
	{ "str_ireplace", PH7_builtin_str_replace},
	{ "strtr",        PH7_builtin_strtr      },
	{ "parse_ini_string", PH7_builtin_parse_ini_string},
	         /* Ctype functions */
	{ "ctype_alnum", PH7_builtin_ctype_alnum },
	{ "ctype_alpha", PH7_builtin_ctype_alpha },
	{ "ctype_cntrl", PH7_builtin_ctype_cntrl },
	{ "ctype_digit", PH7_builtin_ctype_digit },
	{ "ctype_xdigit",PH7_builtin_ctype_xdigit}, 
	{ "ctype_graph", PH7_builtin_ctype_graph },
	{ "ctype_print", PH7_builtin_ctype_print },
	{ "ctype_punct", PH7_builtin_ctype_punct },
	{ "ctype_space", PH7_builtin_ctype_space },
	{ "ctype_lower", PH7_builtin_ctype_lower },
	{ "ctype_upper", PH7_builtin_ctype_upper },
	         /* Time functions */
	{ "time"    ,    PH7_builtin_time         },
	{ "microtime",   PH7_builtin_microtime    },
	{ "getdate" ,    PH7_builtin_getdate      },
	{ "gettimeofday",PH7_builtin_gettimeofday },
	{ "date",        PH7_builtin_date         },
	{ "strftime",    PH7_builtin_strftime     },
	{ "idate",       PH7_builtin_idate        },
	{ "gmdate",      PH7_builtin_gmdate       },
	{ "localtime",   PH7_builtin_localtime    },
	{ "mktime",      PH7_builtin_mktime       },
	{ "gmmktime",    PH7_builtin_mktime       },
	        /* URL functions */
	{ "base64_encode",PH7_builtin_base64_encode },
	{ "base64_decode",PH7_builtin_base64_decode },
	{ "convert_uuencode",PH7_builtin_base64_encode },
	{ "convert_uudecode",PH7_builtin_base64_decode },
	{ "urlencode",    PH7_builtin_urlencode },
	{ "urldecode",    PH7_builtin_urldecode },
	{ "rawurlencode", PH7_builtin_urlencode },
	{ "rawurldecode", PH7_builtin_urldecode },
#endif /* PH7_DISABLE_BUILTIN_FUNC */
};
/*
 * Register the built-in functions defined above,the array functions 
 * defined in hashmap.c and the IO functions defined in vfs.c.
 */
PH7_PRIVATE void PH7_RegisterBuiltInFunction(ph7_vm *pVm)
{
	sxu32 n;
	for( n = 0 ; n < SX_ARRAYSIZE(aBuiltInFunc) ; ++n ){
		ph7_create_function(&(*pVm),aBuiltInFunc[n].zName,aBuiltInFunc[n].xFunc,0);
	}
	/* Register hashmap functions [i.e: array_merge(),sort(),count(),array_diff(),...] */
	PH7_RegisterHashmapFunctions(&(*pVm));
	/* Register IO functions [i.e: fread(),fwrite(),chdir(),mkdir(),file(),...] */
	PH7_RegisterIORoutine(&(*pVm));
}

/*
 * ----------------------------------------------------------
 * File: api.c
 * MD5: ec37aefad456de49a24c8f73f45f8c84
 * ----------------------------------------------------------
 */
/*
 * Symisc PH7: An embeddable bytecode compiler and a virtual machine for the PHP(5) programming language.
 * Copyright (C) 2011-2012, Symisc Systems http://ph7.symisc.net/
 * Version 2.1.4
 * For information on licensing,redistribution of this file,and for a DISCLAIMER OF ALL WARRANTIES
 * please contact Symisc Systems via:
 *       legal@symisc.net
 *       licensing@symisc.net
 *       contact@symisc.net
 * or visit:
 *      http://ph7.symisc.net/
 */
 /* $SymiscID: api.c v2.0 FreeBSD 2012-08-18 06:54 stable <chm@symisc.net> $ */
#ifndef PH7_AMALGAMATION
#include "ph7int.h"
#endif
/* This file implement the public interfaces presented to host-applications.
 * Routines in other files are for internal use by PH7 and should not be
 * accessed by users of the library.
 */
#define PH7_ENGINE_MAGIC 0xF874BCD7
#define PH7_ENGINE_MISUSE(ENGINE) (ENGINE == 0 || ENGINE->nMagic != PH7_ENGINE_MAGIC)
#define PH7_VM_MISUSE(VM) (VM == 0 || VM->nMagic == PH7_VM_STALE)
/* If another thread have released a working instance,the following macros
 * evaluates to true. These macros are only used when the library
 * is built with threading support enabled which is not the case in
 * the default built.
 */
#define PH7_THRD_ENGINE_RELEASE(ENGINE) (ENGINE->nMagic != PH7_ENGINE_MAGIC)
#define PH7_THRD_VM_RELEASE(VM) (VM->nMagic == PH7_VM_STALE)
/* IMPLEMENTATION: ph7@embedded@symisc 311-12-32 */
/*
 * All global variables are collected in the structure named "sMPGlobal".
 * That way it is clear in the code when we are using static variable because
 * its name start with sMPGlobal.
 */
static struct Global_Data
{
	SyMemBackend sAllocator;                /* Global low level memory allocator */
#if defined(PH7_ENABLE_THREADS)
	const SyMutexMethods *pMutexMethods;   /* Mutex methods */
	SyMutex *pMutex;                       /* Global mutex */
	sxu32 nThreadingLevel;                 /* Threading level: 0 == Single threaded/1 == Multi-Threaded 
										    * The threading level can be set using the [ph7_lib_config()]
											* interface with a configuration verb set to
											* PH7_LIB_CONFIG_THREAD_LEVEL_SINGLE or 
											* PH7_LIB_CONFIG_THREAD_LEVEL_MULTI
											*/
#endif
	const ph7_vfs *pVfs;                    /* Underlying virtual file system */
	sxi32 nEngine;                          /* Total number of active engines */
	ph7 *pEngines;                          /* List of active engine */
	sxu32 nMagic;                           /* Sanity check against library misuse */
}sMPGlobal = {
	{0,0,0,0,0,0,0,0,{0}},
#if defined(PH7_ENABLE_THREADS)
	0,
	0,
	0,
#endif
	0,
	0,
	0,
	0
};
#define PH7_LIB_MAGIC  0xEA1495BA
#define PH7_LIB_MISUSE (sMPGlobal.nMagic != PH7_LIB_MAGIC)
/*
 * Supported threading level.
 * These options have meaning only when the library is compiled with multi-threading
 * support.That is,the PH7_ENABLE_THREADS compile time directive must be defined
 * when PH7 is built.
 * PH7_THREAD_LEVEL_SINGLE:
 * In this mode,mutexing is disabled and the library can only be used by a single thread.
 * PH7_THREAD_LEVEL_MULTI
 * In this mode, all mutexes including the recursive mutexes on [ph7] objects
 * are enabled so that the application is free to share the same engine
 * between different threads at the same time.
 */
#define PH7_THREAD_LEVEL_SINGLE 1 
#define PH7_THREAD_LEVEL_MULTI  2
/*
 * Configure a running PH7 engine instance.
 * return PH7_OK on success.Any other return
 * value indicates failure.
 * Refer to [ph7_config()].
 */
static sxi32 EngineConfig(ph7 *pEngine,sxi32 nOp,va_list ap)
{
	ph7_conf *pConf = &pEngine->xConf;
	int rc = PH7_OK;
	/* Perform the requested operation */
	switch(nOp){									 
	case PH7_CONFIG_ERR_OUTPUT: {
		ProcConsumer xConsumer = va_arg(ap,ProcConsumer);
		void *pUserData = va_arg(ap,void *);
		/* Compile time error consumer routine */
		if( xConsumer == 0 ){
			rc = PH7_CORRUPT;
			break;
		}
		/* Install the error consumer */
		pConf->xErr     = xConsumer;
		pConf->pErrData = pUserData;
		break;
									 }
	case PH7_CONFIG_ERR_LOG:{
		/* Extract compile-time error log if any */
		const char **pzPtr = va_arg(ap,const char **);
		int *pLen = va_arg(ap,int *);
		if( pzPtr == 0 ){
			rc = PH7_CORRUPT;
			break;
		}
		/* NULL terminate the error-log buffer */
		SyBlobNullAppend(&pConf->sErrConsumer);
		/* Point to the error-log buffer */
		*pzPtr = (const char *)SyBlobData(&pConf->sErrConsumer);
		if( pLen ){
			if( SyBlobLength(&pConf->sErrConsumer) > 1 /* NULL '\0' terminator */ ){
				*pLen = (int)SyBlobLength(&pConf->sErrConsumer);
			}else{
				*pLen = 0;
			}
		}
		break;
							}
	case PH7_CONFIG_ERR_ABORT:
		/* Reserved for future use */
		break;
	default:
		/* Unknown configuration verb */
		rc = PH7_CORRUPT;
		break;
	} /* Switch() */
	return rc;
}
/*
 * Configure the PH7 library.
 * return PH7_OK on success.Any other return value
 * indicates failure.
 * Refer to [ph7_lib_config()].
 */
static sxi32 PH7CoreConfigure(sxi32 nOp,va_list ap)
{
	int rc = PH7_OK;
	switch(nOp){
	    case PH7_LIB_CONFIG_VFS:{
			/* Install a virtual file system */
			const ph7_vfs *pVfs = va_arg(ap,const ph7_vfs *);
			sMPGlobal.pVfs = pVfs;
			break;
								}
		case PH7_LIB_CONFIG_USER_MALLOC: {
			/* Use an alternative low-level memory allocation routines */
			const SyMemMethods *pMethods = va_arg(ap,const SyMemMethods *);
			/* Save the memory failure callback (if available) */
			ProcMemError xMemErr = sMPGlobal.sAllocator.xMemError;
			void *pMemErr = sMPGlobal.sAllocator.pUserData;
			if( pMethods == 0 ){
				/* Use the built-in memory allocation subsystem */
				rc = SyMemBackendInit(&sMPGlobal.sAllocator,xMemErr,pMemErr);
			}else{
				rc = SyMemBackendInitFromOthers(&sMPGlobal.sAllocator,pMethods,xMemErr,pMemErr);
			}
			break;
										  }
		case PH7_LIB_CONFIG_MEM_ERR_CALLBACK: {
			/* Memory failure callback */
			ProcMemError xMemErr = va_arg(ap,ProcMemError);
			void *pUserData = va_arg(ap,void *);
			sMPGlobal.sAllocator.xMemError = xMemErr;
			sMPGlobal.sAllocator.pUserData = pUserData;
			break;
												 }	  
		case PH7_LIB_CONFIG_USER_MUTEX: {
#if defined(PH7_ENABLE_THREADS)
			/* Use an alternative low-level mutex subsystem */
			const SyMutexMethods *pMethods = va_arg(ap,const SyMutexMethods *);
#if defined (UNTRUST)
			if( pMethods == 0 ){
				rc = PH7_CORRUPT;
			}
#endif
			/* Sanity check */
			if( pMethods->xEnter == 0 || pMethods->xLeave == 0 || pMethods->xNew == 0){
				/* At least three criticial callbacks xEnter(),xLeave() and xNew() must be supplied */
				rc = PH7_CORRUPT;
				break;
			}
			if( sMPGlobal.pMutexMethods ){
				/* Overwrite the previous mutex subsystem */
				SyMutexRelease(sMPGlobal.pMutexMethods,sMPGlobal.pMutex);
				if( sMPGlobal.pMutexMethods->xGlobalRelease ){
					sMPGlobal.pMutexMethods->xGlobalRelease();
				}
				sMPGlobal.pMutex = 0;
			}
			/* Initialize and install the new mutex subsystem */
			if( pMethods->xGlobalInit ){
				rc = pMethods->xGlobalInit();
				if ( rc != PH7_OK ){
					break;
				}
			}
			/* Create the global mutex */
			sMPGlobal.pMutex = pMethods->xNew(SXMUTEX_TYPE_FAST);
			if( sMPGlobal.pMutex == 0 ){
				/*
				 * If the supplied mutex subsystem is so sick that we are unable to
				 * create a single mutex,there is no much we can do here.
				 */
				if( pMethods->xGlobalRelease ){
					pMethods->xGlobalRelease();
				}
				rc = PH7_CORRUPT;
				break;
			}
			sMPGlobal.pMutexMethods = pMethods;			
			if( sMPGlobal.nThreadingLevel == 0 ){
				/* Set a default threading level */
				sMPGlobal.nThreadingLevel = PH7_THREAD_LEVEL_MULTI; 
			}
#endif
			break;
										   }
		case PH7_LIB_CONFIG_THREAD_LEVEL_SINGLE:
#if defined(PH7_ENABLE_THREADS)
			/* Single thread mode(Only one thread is allowed to play with the library) */
			sMPGlobal.nThreadingLevel = PH7_THREAD_LEVEL_SINGLE;
#endif
			break;
		case PH7_LIB_CONFIG_THREAD_LEVEL_MULTI:
#if defined(PH7_ENABLE_THREADS)
			/* Multi-threading mode (library is thread safe and PH7 engines and virtual machines
			 * may be shared between multiple threads).
			 */
			sMPGlobal.nThreadingLevel = PH7_THREAD_LEVEL_MULTI;
#endif
			break;
		default:
			/* Unknown configuration option */
			rc = PH7_CORRUPT;
			break;
	}
	return rc;
}
/*
 * [CAPIREF: ph7_lib_config()]
 * Please refer to the official documentation for function purpose and expected parameters.
 */
int ph7_lib_config(int nConfigOp,...)
{
	va_list ap;
	int rc;

	if( sMPGlobal.nMagic == PH7_LIB_MAGIC ){
		/* Library is already initialized,this operation is forbidden */
		return PH7_LOOKED;
	}
	va_start(ap,nConfigOp);
	rc = PH7CoreConfigure(nConfigOp,ap);
	va_end(ap);
	return rc;
}
/*
 * Global library initialization
 * Refer to [ph7_lib_init()]
 * This routine must be called to initialize the memory allocation subsystem,the mutex 
 * subsystem prior to doing any serious work with the library.The first thread to call
 * this routine does the initialization process and set the magic number so no body later
 * can re-initialize the library.If subsequent threads call this  routine before the first
 * thread have finished the initialization process, then the subsequent threads must block 
 * until the initialization process is done.
 */
static sxi32 PH7CoreInitialize(void)
{
	const ph7_vfs *pVfs; /* Built-in vfs */
#if defined(PH7_ENABLE_THREADS)
	const SyMutexMethods *pMutexMethods = 0;
	SyMutex *pMaster = 0;
#endif
	int rc;
	/*
	 * If the library is already initialized,then a call to this routine
	 * is a no-op.
	 */
	if( sMPGlobal.nMagic == PH7_LIB_MAGIC ){
		return PH7_OK; /* Already initialized */
	}
	/* Point to the built-in vfs */
	pVfs = PH7_ExportBuiltinVfs();
	/* Install it */
	ph7_lib_config(PH7_LIB_CONFIG_VFS,pVfs);
#if defined(PH7_ENABLE_THREADS)
	if( sMPGlobal.nThreadingLevel != PH7_THREAD_LEVEL_SINGLE ){
		pMutexMethods = sMPGlobal.pMutexMethods;
		if( pMutexMethods == 0 ){
			/* Use the built-in mutex subsystem */
			pMutexMethods = SyMutexExportMethods();
			if( pMutexMethods == 0 ){
				return PH7_CORRUPT; /* Can't happen */
			}
			/* Install the mutex subsystem */
			rc = ph7_lib_config(PH7_LIB_CONFIG_USER_MUTEX,pMutexMethods);
			if( rc != PH7_OK ){
				return rc;
			}
		}
		/* Obtain a static mutex so we can initialize the library without calling malloc() */
		pMaster = SyMutexNew(pMutexMethods,SXMUTEX_TYPE_STATIC_1);
		if( pMaster == 0 ){
			return PH7_CORRUPT; /* Can't happen */
		}
	}
	/* Lock the master mutex */
	rc = PH7_OK;
	SyMutexEnter(pMutexMethods,pMaster); /* NO-OP if sMPGlobal.nThreadingLevel == PH7_THREAD_LEVEL_SINGLE */
	if( sMPGlobal.nMagic != PH7_LIB_MAGIC ){
#endif
		if( sMPGlobal.sAllocator.pMethods == 0 ){
			/* Install a memory subsystem */
			rc = ph7_lib_config(PH7_LIB_CONFIG_USER_MALLOC,0); /* zero mean use the built-in memory backend */
			if( rc != PH7_OK ){
				/* If we are unable to initialize the memory backend,there is no much we can do here.*/
				goto End;
			}
		}
#if defined(PH7_ENABLE_THREADS)
		if( sMPGlobal.nThreadingLevel > PH7_THREAD_LEVEL_SINGLE ){
			/* Protect the memory allocation subsystem */
			rc = SyMemBackendMakeThreadSafe(&sMPGlobal.sAllocator,sMPGlobal.pMutexMethods);
			if( rc != PH7_OK ){
				goto End;
			}
		}
#endif
		/* Our library is initialized,set the magic number */
		sMPGlobal.nMagic = PH7_LIB_MAGIC;
		rc = PH7_OK;
#if defined(PH7_ENABLE_THREADS)
	} /* sMPGlobal.nMagic != PH7_LIB_MAGIC */
#endif
End:
#if defined(PH7_ENABLE_THREADS)
	/* Unlock the master mutex */
	SyMutexLeave(pMutexMethods,pMaster); /* NO-OP if sMPGlobal.nThreadingLevel == PH7_THREAD_LEVEL_SINGLE */
#endif
	return rc;
}
/*
 * [CAPIREF: ph7_lib_init()]
 * Please refer to the official documentation for function purpose and expected parameters.
 */
int ph7_lib_init(void)
{
	int rc;
	rc = PH7CoreInitialize();
	return rc;
}
/*
 * Release an active PH7 engine and it's associated active virtual machines.
 */
static sxi32 EngineRelease(ph7 *pEngine)
{
	ph7_vm *pVm,*pNext;
	/* Release all active VM */
	pVm = pEngine->pVms;
	for(;;){
		if( pEngine->iVm <= 0 ){
			break;
		}
		pNext = pVm->pNext;
		PH7_VmRelease(pVm);
		pVm = pNext;
		pEngine->iVm--;
	}
	/* Set a dummy magic number */
	pEngine->nMagic = 0x7635;
	/* Release the private memory subsystem */
	SyMemBackendRelease(&pEngine->sAllocator); 
	return PH7_OK;
}
/*
 * Release all resources consumed by the library.
 * If PH7 is already shut down when this routine
 * is invoked then this routine is a harmless no-op.
 * Note: This call is not thread safe.
 * Refer to [ph7_lib_shutdown()].
 */
static void PH7CoreShutdown(void)
{
	ph7 *pEngine,*pNext;
	/* Release all active engines first */
	pEngine = sMPGlobal.pEngines;
	for(;;){
		if( sMPGlobal.nEngine < 1 ){
			break;
		}
		pNext = pEngine->pNext;
		EngineRelease(pEngine); 
		pEngine = pNext;
		sMPGlobal.nEngine--;
	}
#if defined(PH7_ENABLE_THREADS)
	/* Release the mutex subsystem */
	if( sMPGlobal.pMutexMethods ){
		if( sMPGlobal.pMutex ){
			SyMutexRelease(sMPGlobal.pMutexMethods,sMPGlobal.pMutex);
			sMPGlobal.pMutex = 0;
		}
		if( sMPGlobal.pMutexMethods->xGlobalRelease ){
			sMPGlobal.pMutexMethods->xGlobalRelease();
		}
		sMPGlobal.pMutexMethods = 0;
	}
	sMPGlobal.nThreadingLevel = 0;
#endif
	if( sMPGlobal.sAllocator.pMethods ){
		/* Release the memory backend */
		SyMemBackendRelease(&sMPGlobal.sAllocator);
	}
	sMPGlobal.nMagic = 0x1928;	
}
/*
 * [CAPIREF: ph7_lib_shutdown()]
 * Please refer to the official documentation for function purpose and expected parameters.
 */
int ph7_lib_shutdown(void)
{
	if( sMPGlobal.nMagic != PH7_LIB_MAGIC ){
		/* Already shut */
		return PH7_OK;
	}
	PH7CoreShutdown();
	return PH7_OK;
}
/*
 * [CAPIREF: ph7_lib_is_threadsafe()]
 * Please refer to the official documentation for function purpose and expected parameters.
 */
int ph7_lib_is_threadsafe(void)
{
	if( sMPGlobal.nMagic != PH7_LIB_MAGIC ){
		return 0;
	}
#if defined(PH7_ENABLE_THREADS)
		if( sMPGlobal.nThreadingLevel > PH7_THREAD_LEVEL_SINGLE ){
			/* Muli-threading support is enabled */
			return 1;
		}else{
			/* Single-threading */
			return 0;
		}
#else
	return 0;
#endif
}
/*
 * [CAPIREF: ph7_lib_version()]
 * Please refer to the official documentation for function purpose and expected parameters.
 */
const char * ph7_lib_version(void)
{
	return PH7_VERSION;
}
/*
 * [CAPIREF: ph7_lib_signature()]
 * Please refer to the official documentation for function purpose and expected parameters.
 */
const char * ph7_lib_signature(void)
{
	return PH7_SIG;
}
/*
 * [CAPIREF: ph7_lib_ident()]
 * Please refer to the official documentation for function purpose and expected parameters.
 */
const char * ph7_lib_ident(void)
{
	return PH7_IDENT;
}
/*
 * [CAPIREF: ph7_lib_copyright()]
 * Please refer to the official documentation for function purpose and expected parameters.
 */
const char * ph7_lib_copyright(void)
{
	return PH7_COPYRIGHT;
}
/*
 * [CAPIREF: ph7_config()]
 * Please refer to the official documentation for function purpose and expected parameters.
 */
int ph7_config(ph7 *pEngine,int nConfigOp,...)
{
	va_list ap;
	int rc;
	if( PH7_ENGINE_MISUSE(pEngine) ){
		return PH7_CORRUPT;
	}
#if defined(PH7_ENABLE_THREADS)
	 /* Acquire engine mutex */
	 SyMutexEnter(sMPGlobal.pMutexMethods,pEngine->pMutex); /* NO-OP if sMPGlobal.nThreadingLevel != PH7_THREAD_LEVEL_MULTI */
	 if( sMPGlobal.nThreadingLevel > PH7_THREAD_LEVEL_SINGLE && 
		 PH7_THRD_ENGINE_RELEASE(pEngine) ){
			 return PH7_ABORT; /* Another thread have released this instance */
	 }
#endif
	 va_start(ap,nConfigOp);
	 rc = EngineConfig(&(*pEngine),nConfigOp,ap);
	 va_end(ap);
#if defined(PH7_ENABLE_THREADS)
	 /* Leave engine mutex */
	 SyMutexLeave(sMPGlobal.pMutexMethods,pEngine->pMutex); /* NO-OP if sMPGlobal.nThreadingLevel != PH7_THREAD_LEVEL_MULTI */
#endif
	return rc;
}
/*
 * [CAPIREF: ph7_init()]
 * Please refer to the official documentation for function purpose and expected parameters.
 */
int ph7_init(ph7 **ppEngine)
{
	ph7 *pEngine;
	int rc;
#if defined(UNTRUST)
	if( ppEngine == 0 ){
		return PH7_CORRUPT;
	}
#endif
	*ppEngine = 0;
	/* One-time automatic library initialization */
	rc = PH7CoreInitialize();
	if( rc != PH7_OK ){
		return rc;
	}
	/* Allocate a new engine */
	pEngine = (ph7 *)SyMemBackendPoolAlloc(&sMPGlobal.sAllocator,sizeof(ph7));
	if( pEngine == 0 ){
		return PH7_NOMEM;
	}
	/* Zero the structure */
	SyZero(pEngine,sizeof(ph7));
	/* Initialize engine fields */
	pEngine->nMagic = PH7_ENGINE_MAGIC;
	rc = SyMemBackendInitFromParent(&pEngine->sAllocator,&sMPGlobal.sAllocator);
	if( rc != PH7_OK ){
		goto Release;
	}
#if defined(PH7_ENABLE_THREADS)
	SyMemBackendDisbaleMutexing(&pEngine->sAllocator);
#endif
	/* Default configuration */
	SyBlobInit(&pEngine->xConf.sErrConsumer,&pEngine->sAllocator);
	/* Install a default compile-time error consumer routine */
	ph7_config(pEngine,PH7_CONFIG_ERR_OUTPUT,PH7_VmBlobConsumer,&pEngine->xConf.sErrConsumer);
	/* Built-in vfs */
	pEngine->pVfs = sMPGlobal.pVfs;
#if defined(PH7_ENABLE_THREADS)
	if( sMPGlobal.nThreadingLevel > PH7_THREAD_LEVEL_SINGLE ){
		 /* Associate a recursive mutex with this instance */
		 pEngine->pMutex = SyMutexNew(sMPGlobal.pMutexMethods,SXMUTEX_TYPE_RECURSIVE);
		 if( pEngine->pMutex == 0 ){
			 rc = PH7_NOMEM;
			 goto Release;
		 }
	 }
#endif
	/* Link to the list of active engines */
#if defined(PH7_ENABLE_THREADS)
	/* Enter the global mutex */
	 SyMutexEnter(sMPGlobal.pMutexMethods,sMPGlobal.pMutex); /* NO-OP if sMPGlobal.nThreadingLevel == PH7_THREAD_LEVEL_SINGLE */
#endif
	MACRO_LD_PUSH(sMPGlobal.pEngines,pEngine);
	sMPGlobal.nEngine++;
#if defined(PH7_ENABLE_THREADS)
	/* Leave the global mutex */
	 SyMutexLeave(sMPGlobal.pMutexMethods,sMPGlobal.pMutex); /* NO-OP if sMPGlobal.nThreadingLevel == PH7_THREAD_LEVEL_SINGLE */
#endif
	/* Write a pointer to the new instance */
	*ppEngine = pEngine;
	return PH7_OK;
Release:
	SyMemBackendRelease(&pEngine->sAllocator);
	SyMemBackendPoolFree(&sMPGlobal.sAllocator,pEngine);
	return rc;
}
/*
 * [CAPIREF: ph7_release()]
 * Please refer to the official documentation for function purpose and expected parameters.
 */
int ph7_release(ph7 *pEngine)
{
	int rc;
	if( PH7_ENGINE_MISUSE(pEngine) ){
		return PH7_CORRUPT;
	}
#if defined(PH7_ENABLE_THREADS)
	 /* Acquire engine mutex */
	 SyMutexEnter(sMPGlobal.pMutexMethods,pEngine->pMutex); /* NO-OP if sMPGlobal.nThreadingLevel != PH7_THREAD_LEVEL_MULTI */
	 if( sMPGlobal.nThreadingLevel > PH7_THREAD_LEVEL_SINGLE && 
		 PH7_THRD_ENGINE_RELEASE(pEngine) ){
			 return PH7_ABORT; /* Another thread have released this instance */
	 }
#endif
	/* Release the engine */
	rc = EngineRelease(&(*pEngine));
#if defined(PH7_ENABLE_THREADS)
	 /* Leave engine mutex */
	 SyMutexLeave(sMPGlobal.pMutexMethods,pEngine->pMutex); /* NO-OP if sMPGlobal.nThreadingLevel != PH7_THREAD_LEVEL_MULTI */
	 /* Release engine mutex */
	 SyMutexRelease(sMPGlobal.pMutexMethods,pEngine->pMutex) /* NO-OP if sMPGlobal.nThreadingLevel != PH7_THREAD_LEVEL_MULTI */
#endif
#if defined(PH7_ENABLE_THREADS)
	/* Enter the global mutex */
	 SyMutexEnter(sMPGlobal.pMutexMethods,sMPGlobal.pMutex); /* NO-OP if sMPGlobal.nThreadingLevel == PH7_THREAD_LEVEL_SINGLE */
#endif
	/* Unlink from the list of active engines */
	MACRO_LD_REMOVE(sMPGlobal.pEngines,pEngine);
	sMPGlobal.nEngine--;
#if defined(PH7_ENABLE_THREADS)
	/* Leave the global mutex */
	 SyMutexLeave(sMPGlobal.pMutexMethods,sMPGlobal.pMutex); /* NO-OP if sMPGlobal.nThreadingLevel == PH7_THREAD_LEVEL_SINGLE */
#endif
	/* Release the memory chunk allocated to this engine */
	SyMemBackendPoolFree(&sMPGlobal.sAllocator,pEngine);
	return rc;
}
/*
 * Compile a raw PHP script.
 * To execute a PHP code, it must first be compiled into a byte-code program using this routine.
 * If something goes wrong [i.e: compile-time error], your error log [i.e: error consumer callback]
 * should  display the appropriate error message and this function set ppVm to null and return
 * an error code that is different from PH7_OK. Otherwise when the script is successfully compiled
 * ppVm should hold the PH7 byte-code and it's safe to call [ph7_vm_exec(), ph7_vm_reset(), etc.].
 * This API does not actually evaluate the PHP code. It merely compile and prepares the PHP script
 * for evaluation.
 */
static sxi32 ProcessScript(
	ph7 *pEngine,          /* Running PH7 engine */
	ph7_vm **ppVm,         /* OUT: A pointer to the virtual machine */
	SyString *pScript,     /* Raw PHP script to compile */
	sxi32 iFlags,          /* Compile-time flags */
	const char *zFilePath  /* File path if script come from a file. NULL otherwise */
	)
{
	ph7_vm *pVm;
	int rc;
	/* Allocate a new virtual machine */
	pVm = (ph7_vm *)SyMemBackendPoolAlloc(&pEngine->sAllocator,sizeof(ph7_vm));
	if( pVm == 0 ){
		/* If the supplied memory subsystem is so sick that we are unable to allocate
		 * a tiny chunk of memory, there is no much we can do here. */
		if( ppVm ){
			*ppVm = 0;
		}
		return PH7_NOMEM;
	}
	if( iFlags < 0 ){
		/* Default compile-time flags */
		iFlags = 0;
	}
	/* Initialize the Virtual Machine */
	rc = PH7_VmInit(pVm,&(*pEngine));
	if( rc != PH7_OK ){
		SyMemBackendPoolFree(&pEngine->sAllocator,pVm);
		if( ppVm ){
			*ppVm = 0;
		}
		return PH7_VM_ERR;
	}
	if( zFilePath ){
		/* Push processed file path */
		PH7_VmPushFilePath(pVm,zFilePath,-1,TRUE,0);
	}
	/* Reset the error message consumer */
	SyBlobReset(&pEngine->xConf.sErrConsumer);
	/* Compile the script */
	PH7_CompileScript(pVm,&(*pScript),iFlags);
	if( pVm->sCodeGen.nErr > 0 || pVm == 0){
		sxu32 nErr = pVm->sCodeGen.nErr;
		/* Compilation error or null ppVm pointer,release this VM */
		SyMemBackendRelease(&pVm->sAllocator);
		SyMemBackendPoolFree(&pEngine->sAllocator,pVm);
		if( ppVm ){
			*ppVm = 0;
		}
		return nErr > 0 ? PH7_COMPILE_ERR : PH7_OK;
	}
	/* Prepare the virtual machine for bytecode execution */
	rc = PH7_VmMakeReady(pVm);
	if( rc != PH7_OK ){
		goto Release;
	}
	/* Install local import path which is the current directory */
	ph7_vm_config(pVm,PH7_VM_CONFIG_IMPORT_PATH,"./");
#if defined(PH7_ENABLE_THREADS)
	if( sMPGlobal.nThreadingLevel > PH7_THREAD_LEVEL_SINGLE ){
		 /* Associate a recursive mutex with this instance */
		 pVm->pMutex = SyMutexNew(sMPGlobal.pMutexMethods,SXMUTEX_TYPE_RECURSIVE);
		 if( pVm->pMutex == 0 ){
			 goto Release;
		 }
	 }
#endif
	/* Script successfully compiled,link to the list of active virtual machines */
	MACRO_LD_PUSH(pEngine->pVms,pVm);
	pEngine->iVm++;
	/* Point to the freshly created VM */
	*ppVm = pVm;
	/* Ready to execute PH7 bytecode */
	return PH7_OK;
Release:
	SyMemBackendRelease(&pVm->sAllocator);
	SyMemBackendPoolFree(&pEngine->sAllocator,pVm);
	*ppVm = 0;
	return PH7_VM_ERR;
}
/*
 * [CAPIREF: ph7_compile()]
 * Please refer to the official documentation for function purpose and expected parameters.
 */
int ph7_compile(ph7 *pEngine,const char *zSource,int nLen,ph7_vm **ppOutVm)
{
	SyString sScript;
	int rc;
	if( PH7_ENGINE_MISUSE(pEngine) || zSource == 0){
		return PH7_CORRUPT;
	}
	if( nLen < 0 ){
		/* Compute input length automatically */
		nLen = (int)SyStrlen(zSource);
	}
	SyStringInitFromBuf(&sScript,zSource,nLen);
#if defined(PH7_ENABLE_THREADS)
	 /* Acquire engine mutex */
	 SyMutexEnter(sMPGlobal.pMutexMethods,pEngine->pMutex); /* NO-OP if sMPGlobal.nThreadingLevel != PH7_THREAD_LEVEL_MULTI */
	 if( sMPGlobal.nThreadingLevel > PH7_THREAD_LEVEL_SINGLE && 
		 PH7_THRD_ENGINE_RELEASE(pEngine) ){
			 return PH7_ABORT; /* Another thread have released this instance */
	 }
#endif
	/* Compile the script */
	rc = ProcessScript(&(*pEngine),ppOutVm,&sScript,0,0);
#if defined(PH7_ENABLE_THREADS)
	 /* Leave engine mutex */
	 SyMutexLeave(sMPGlobal.pMutexMethods,pEngine->pMutex); /* NO-OP if sMPGlobal.nThreadingLevel != PH7_THREAD_LEVEL_MULTI */
#endif
	/* Compilation result */
	return rc;
}
/*
 * [CAPIREF: ph7_compile_v2()]
 * Please refer to the official documentation for function purpose and expected parameters.
 */
int ph7_compile_v2(ph7 *pEngine,const char *zSource,int nLen,ph7_vm **ppOutVm,int iFlags)
{
	SyString sScript;
	int rc;
	if( PH7_ENGINE_MISUSE(pEngine) || zSource == 0){
		return PH7_CORRUPT;
	}
	if( nLen < 0 ){
		/* Compute input length automatically */
		nLen = (int)SyStrlen(zSource);
	}
	SyStringInitFromBuf(&sScript,zSource,nLen);
#if defined(PH7_ENABLE_THREADS)
	 /* Acquire engine mutex */
	 SyMutexEnter(sMPGlobal.pMutexMethods,pEngine->pMutex); /* NO-OP if sMPGlobal.nThreadingLevel != PH7_THREAD_LEVEL_MULTI */
	 if( sMPGlobal.nThreadingLevel > PH7_THREAD_LEVEL_SINGLE && 
		 PH7_THRD_ENGINE_RELEASE(pEngine) ){
			 return PH7_ABORT; /* Another thread have released this instance */
	 }
#endif
	/* Compile the script */
	rc = ProcessScript(&(*pEngine),ppOutVm,&sScript,iFlags,0);
#if defined(PH7_ENABLE_THREADS)
	 /* Leave engine mutex */
	 SyMutexLeave(sMPGlobal.pMutexMethods,pEngine->pMutex); /* NO-OP if sMPGlobal.nThreadingLevel != PH7_THREAD_LEVEL_MULTI */
#endif
	/* Compilation result */
	return rc;
}
/*
 * [CAPIREF: ph7_compile_file()]
 * Please refer to the official documentation for function purpose and expected parameters.
 */
int ph7_compile_file(ph7 *pEngine,const char *zFilePath,ph7_vm **ppOutVm,int iFlags)
{
	const ph7_vfs *pVfs;
	int rc;
	if( ppOutVm ){
		*ppOutVm = 0;
	}
	rc = PH7_OK; /* cc warning */
	if( PH7_ENGINE_MISUSE(pEngine) || SX_EMPTY_STR(zFilePath) ){
		return PH7_CORRUPT;
	}
#if defined(PH7_ENABLE_THREADS)
	 /* Acquire engine mutex */
	 SyMutexEnter(sMPGlobal.pMutexMethods,pEngine->pMutex); /* NO-OP if sMPGlobal.nThreadingLevel != PH7_THREAD_LEVEL_MULTI */
	 if( sMPGlobal.nThreadingLevel > PH7_THREAD_LEVEL_SINGLE && 
		 PH7_THRD_ENGINE_RELEASE(pEngine) ){
			 return PH7_ABORT; /* Another thread have released this instance */
	 }
#endif
	 /*
	  * Check if the underlying vfs implement the memory map
	  * [i.e: mmap() under UNIX/MapViewOfFile() under windows] function.
	  */
	 pVfs = pEngine->pVfs;
	 if( pVfs == 0 || pVfs->xMmap == 0 ){
		 /* Memory map routine not implemented */
		 rc = PH7_IO_ERR;
	 }else{
		 void *pMapView = 0; /* cc warning */
		 ph7_int64 nSize = 0; /* cc warning */
		 SyString sScript;
		 /* Try to get a memory view of the whole file */
		 rc = pVfs->xMmap(zFilePath,&pMapView,&nSize);
		 if( rc != PH7_OK ){
			 /* Assume an IO error */
			 rc = PH7_IO_ERR;
		 }else{
			 /* Compile the file */
			 SyStringInitFromBuf(&sScript,pMapView,nSize);
			 rc = ProcessScript(&(*pEngine),ppOutVm,&sScript,iFlags,zFilePath);
			 /* Release the memory view of the whole file */
			 if( pVfs->xUnmap ){
				 pVfs->xUnmap(pMapView,nSize);
			 }
		 }
	 }
#if defined(PH7_ENABLE_THREADS)
	 /* Leave engine mutex */
	 SyMutexLeave(sMPGlobal.pMutexMethods,pEngine->pMutex); /* NO-OP if sMPGlobal.nThreadingLevel != PH7_THREAD_LEVEL_MULTI */
#endif
	/* Compilation result */
	return rc;
}
/*
 * [CAPIREF: ph7_vm_dump_v2()]
 * Please refer to the official documentation for function purpose and expected parameters.
 */
int ph7_vm_dump_v2(ph7_vm *pVm,int (*xConsumer)(const void *,unsigned int,void *),void *pUserData)
{
	int rc;
	/* Ticket 1433-002: NULL VM is harmless operation */
	if ( PH7_VM_MISUSE(pVm) ){
		return PH7_CORRUPT;
	}
#ifdef UNTRUST
	if( xConsumer == 0 ){
		return PH7_CORRUPT;
	}
#endif
	/* Dump VM instructions */
	rc = PH7_VmDump(&(*pVm),xConsumer,pUserData);
	return rc;
}
/*
 * [CAPIREF: ph7_vm_config()]
 * Please refer to the official documentation for function purpose and expected parameters.
 */
int ph7_vm_config(ph7_vm *pVm,int iConfigOp,...)
{
	va_list ap;
	int rc;
	/* Ticket 1433-002: NULL VM is harmless operation */
	if ( PH7_VM_MISUSE(pVm) ){
		return PH7_CORRUPT;
	}
#if defined(PH7_ENABLE_THREADS)
	 /* Acquire VM mutex */
	 SyMutexEnter(sMPGlobal.pMutexMethods,pVm->pMutex); /* NO-OP if sMPGlobal.nThreadingLevel != PH7_THREAD_LEVEL_MULTI */
	 if( sMPGlobal.nThreadingLevel > PH7_THREAD_LEVEL_SINGLE && 
		 PH7_THRD_VM_RELEASE(pVm) ){
			 return PH7_ABORT; /* Another thread have released this instance */
	 }
#endif
	/* Confiugure the virtual machine */
	va_start(ap,iConfigOp);
	rc = PH7_VmConfigure(&(*pVm),iConfigOp,ap);
	va_end(ap);
#if defined(PH7_ENABLE_THREADS)
	 /* Leave VM mutex */
	 SyMutexLeave(sMPGlobal.pMutexMethods,pVm->pMutex); /* NO-OP if sMPGlobal.nThreadingLevel != PH7_THREAD_LEVEL_MULTI */
#endif
	return rc;
}
/*
 * [CAPIREF: ph7_vm_exec()]
 * Please refer to the official documentation for function purpose and expected parameters.
 */
int ph7_vm_exec(ph7_vm *pVm,int *pExitStatus)
{
	int rc;
	/* Ticket 1433-002: NULL VM is harmless operation */
	if ( PH7_VM_MISUSE(pVm) ){
		return PH7_CORRUPT;
	}
#if defined(PH7_ENABLE_THREADS)
	 /* Acquire VM mutex */
	 SyMutexEnter(sMPGlobal.pMutexMethods,pVm->pMutex); /* NO-OP if sMPGlobal.nThreadingLevel != PH7_THREAD_LEVEL_MULTI */
	 if( sMPGlobal.nThreadingLevel > PH7_THREAD_LEVEL_SINGLE && 
		 PH7_THRD_VM_RELEASE(pVm) ){
			 return PH7_ABORT; /* Another thread have released this instance */
	 }
#endif
	/* Execute PH7 byte-code */
	rc = PH7_VmByteCodeExec(&(*pVm));
	if( pExitStatus ){
		/* Exit status */
		*pExitStatus = pVm->iExitStatus;
	}
#if defined(PH7_ENABLE_THREADS)
	 /* Leave VM mutex */
	 SyMutexLeave(sMPGlobal.pMutexMethods,pVm->pMutex); /* NO-OP if sMPGlobal.nThreadingLevel != PH7_THREAD_LEVEL_MULTI */
#endif
	/* Execution result */
	return rc;
}
/*
 * [CAPIREF: ph7_vm_reset()]
 * Please refer to the official documentation for function purpose and expected parameters.
 */
int ph7_vm_reset(ph7_vm *pVm)
{
	int rc;
	/* Ticket 1433-002: NULL VM is harmless operation */
	if ( PH7_VM_MISUSE(pVm) ){
		return PH7_CORRUPT;
	}
#if defined(PH7_ENABLE_THREADS)
	 /* Acquire VM mutex */
	 SyMutexEnter(sMPGlobal.pMutexMethods,pVm->pMutex); /* NO-OP if sMPGlobal.nThreadingLevel != PH7_THREAD_LEVEL_MULTI */
	 if( sMPGlobal.nThreadingLevel > PH7_THREAD_LEVEL_SINGLE && 
		 PH7_THRD_VM_RELEASE(pVm) ){
			 return PH7_ABORT; /* Another thread have released this instance */
	 }
#endif
	rc = PH7_VmReset(&(*pVm));
#if defined(PH7_ENABLE_THREADS)
	 /* Leave VM mutex */
	 SyMutexLeave(sMPGlobal.pMutexMethods,pVm->pMutex); /* NO-OP if sMPGlobal.nThreadingLevel != PH7_THREAD_LEVEL_MULTI */
#endif
	return rc;
}
/*
 * [CAPIREF: ph7_vm_release()]
 * Please refer to the official documentation for function purpose and expected parameters.
 */
int ph7_vm_release(ph7_vm *pVm)
{
	ph7 *pEngine;
	int rc;
	/* Ticket 1433-002: NULL VM is harmless operation */
	if ( PH7_VM_MISUSE(pVm) ){
		return PH7_CORRUPT;
	}
#if defined(PH7_ENABLE_THREADS)
	 /* Acquire VM mutex */
	 SyMutexEnter(sMPGlobal.pMutexMethods,pVm->pMutex); /* NO-OP if sMPGlobal.nThreadingLevel != PH7_THREAD_LEVEL_MULTI */
	 if( sMPGlobal.nThreadingLevel > PH7_THREAD_LEVEL_SINGLE && 
		 PH7_THRD_VM_RELEASE(pVm) ){
			 return PH7_ABORT; /* Another thread have released this instance */
	 }
#endif
	pEngine = pVm->pEngine;
	rc = PH7_VmRelease(&(*pVm));
#if defined(PH7_ENABLE_THREADS)
	 /* Leave VM mutex */
	 SyMutexLeave(sMPGlobal.pMutexMethods,pVm->pMutex); /* NO-OP if sMPGlobal.nThreadingLevel != PH7_THREAD_LEVEL_MULTI */
#endif
	if( rc == PH7_OK ){
		/* Unlink from the list of active VM */
#if defined(PH7_ENABLE_THREADS)
			/* Acquire engine mutex */
			SyMutexEnter(sMPGlobal.pMutexMethods,pEngine->pMutex); /* NO-OP if sMPGlobal.nThreadingLevel != PH7_THREAD_LEVEL_MULTI */
			if( sMPGlobal.nThreadingLevel > PH7_THREAD_LEVEL_SINGLE && 
				PH7_THRD_ENGINE_RELEASE(pEngine) ){
					return PH7_ABORT; /* Another thread have released this instance */
			}
#endif
		MACRO_LD_REMOVE(pEngine->pVms,pVm);
		pEngine->iVm--;
		/* Release the memory chunk allocated to this VM */
		SyMemBackendPoolFree(&pEngine->sAllocator,pVm);
#if defined(PH7_ENABLE_THREADS)
			/* Leave engine mutex */
			SyMutexLeave(sMPGlobal.pMutexMethods,pEngine->pMutex); /* NO-OP if sMPGlobal.nThreadingLevel != PH7_THREAD_LEVEL_MULTI */
#endif	
	}
	return rc;
}
/*
 * [CAPIREF: ph7_create_function()]
 * Please refer to the official documentation for function purpose and expected parameters.
 */
int ph7_create_function(ph7_vm *pVm,const char *zName,int (*xFunc)(ph7_context *,int,ph7_value **),void *pUserData)
{
	SyString sName;
	int rc;
	/* Ticket 1433-002: NULL VM is harmless operation */
	if ( PH7_VM_MISUSE(pVm) ){
		return PH7_CORRUPT;
	}
	SyStringInitFromBuf(&sName,zName,SyStrlen(zName));
	/* Remove leading and trailing white spaces */
	SyStringFullTrim(&sName);
	/* Ticket 1433-003: NULL values are not allowed */
	if( sName.nByte < 1 || xFunc == 0 ){
		return PH7_CORRUPT;
	}
#if defined(PH7_ENABLE_THREADS)
	 /* Acquire VM mutex */
	 SyMutexEnter(sMPGlobal.pMutexMethods,pVm->pMutex); /* NO-OP if sMPGlobal.nThreadingLevel != PH7_THREAD_LEVEL_MULTI */
	 if( sMPGlobal.nThreadingLevel > PH7_THREAD_LEVEL_SINGLE && 
		 PH7_THRD_VM_RELEASE(pVm) ){
			 return PH7_ABORT; /* Another thread have released this instance */
	 }
#endif
	/* Install the foreign function */
	rc = PH7_VmInstallForeignFunction(&(*pVm),&sName,xFunc,pUserData); 
#if defined(PH7_ENABLE_THREADS)
	 /* Leave VM mutex */
	 SyMutexLeave(sMPGlobal.pMutexMethods,pVm->pMutex); /* NO-OP if sMPGlobal.nThreadingLevel != PH7_THREAD_LEVEL_MULTI */
#endif
	return rc;
}
/*
 * [CAPIREF: ph7_delete_function()]
 * Please refer to the official documentation for function purpose and expected parameters.
 */
int ph7_delete_function(ph7_vm *pVm,const char *zName)
{
	ph7_user_func *pFunc = 0;
	int rc;
	/* Ticket 1433-002: NULL VM is harmless operation */
	if ( PH7_VM_MISUSE(pVm) ){
		return PH7_CORRUPT;
	}
#if defined(PH7_ENABLE_THREADS)
	 /* Acquire VM mutex */
	 SyMutexEnter(sMPGlobal.pMutexMethods,pVm->pMutex); /* NO-OP if sMPGlobal.nThreadingLevel != PH7_THREAD_LEVEL_MULTI */
	 if( sMPGlobal.nThreadingLevel > PH7_THREAD_LEVEL_SINGLE && 
		 PH7_THRD_VM_RELEASE(pVm) ){
			 return PH7_ABORT; /* Another thread have released this instance */
	 }
#endif
	/* Perform the deletion */
	rc = SyHashDeleteEntry(&pVm->hHostFunction,(const void *)zName,SyStrlen(zName),(void **)&pFunc);
	if( rc == PH7_OK ){
		/* Release internal fields */
		SySetRelease(&pFunc->aAux);
		SyMemBackendFree(&pVm->sAllocator,(void *)SyStringData(&pFunc->sName));
		SyMemBackendPoolFree(&pVm->sAllocator,pFunc);
	}
#if defined(PH7_ENABLE_THREADS)
	 /* Leave VM mutex */
	 SyMutexLeave(sMPGlobal.pMutexMethods,pVm->pMutex); /* NO-OP if sMPGlobal.nThreadingLevel != PH7_THREAD_LEVEL_MULTI */
#endif
	return rc;
}
/*
 * [CAPIREF: ph7_create_constant()]
 * Please refer to the official documentation for function purpose and expected parameters.
 */
int ph7_create_constant(ph7_vm *pVm,const char *zName,void (*xExpand)(ph7_value *,void *),void *pUserData)
{
	SyString sName;
	int rc;
	/* Ticket 1433-002: NULL VM is harmless operation */
	if ( PH7_VM_MISUSE(pVm) ){
		return PH7_CORRUPT;
	}
	SyStringInitFromBuf(&sName,zName,SyStrlen(zName));
	/* Remove leading and trailing white spaces */
	SyStringFullTrim(&sName);
	if( sName.nByte < 1 ){
		/* Empty constant name */
		return PH7_CORRUPT;
	}
	/* TICKET 1433-003: NULL pointer harmless operation */
	if( xExpand == 0 ){
		return PH7_CORRUPT;
	}
#if defined(PH7_ENABLE_THREADS)
	 /* Acquire VM mutex */
	 SyMutexEnter(sMPGlobal.pMutexMethods,pVm->pMutex); /* NO-OP if sMPGlobal.nThreadingLevel != PH7_THREAD_LEVEL_MULTI */
	 if( sMPGlobal.nThreadingLevel > PH7_THREAD_LEVEL_SINGLE && 
		 PH7_THRD_VM_RELEASE(pVm) ){
			 return PH7_ABORT; /* Another thread have released this instance */
	 }
#endif
	/* Perform the registration */
	rc = PH7_VmRegisterConstant(&(*pVm),&sName,xExpand,pUserData);
#if defined(PH7_ENABLE_THREADS)
	 /* Leave VM mutex */
	 SyMutexLeave(sMPGlobal.pMutexMethods,pVm->pMutex); /* NO-OP if sMPGlobal.nThreadingLevel != PH7_THREAD_LEVEL_MULTI */
#endif
	 return rc;
}
/*
 * [CAPIREF: ph7_delete_constant()]
 * Please refer to the official documentation for function purpose and expected parameters.
 */
int ph7_delete_constant(ph7_vm *pVm,const char *zName)
{
	ph7_constant *pCons;
	int rc;
	/* Ticket 1433-002: NULL VM is harmless operation */
	if ( PH7_VM_MISUSE(pVm) ){
		return PH7_CORRUPT;
	}
#if defined(PH7_ENABLE_THREADS)
	 /* Acquire VM mutex */
	 SyMutexEnter(sMPGlobal.pMutexMethods,pVm->pMutex); /* NO-OP if sMPGlobal.nThreadingLevel != PH7_THREAD_LEVEL_MULTI */
	 if( sMPGlobal.nThreadingLevel > PH7_THREAD_LEVEL_SINGLE && 
		 PH7_THRD_VM_RELEASE(pVm) ){
			 return PH7_ABORT; /* Another thread have released this instance */
	 }
#endif
	 /* Query the constant hashtable */
	 rc = SyHashDeleteEntry(&pVm->hConstant,(const void *)zName,SyStrlen(zName),(void **)&pCons);
	 if( rc == PH7_OK ){
		 /* Perform the deletion */
		 SyMemBackendFree(&pVm->sAllocator,(void *)SyStringData(&pCons->sName));
		 SyMemBackendPoolFree(&pVm->sAllocator,pCons);
	 }
#if defined(PH7_ENABLE_THREADS)
	 /* Leave VM mutex */
	 SyMutexLeave(sMPGlobal.pMutexMethods,pVm->pMutex); /* NO-OP if sMPGlobal.nThreadingLevel != PH7_THREAD_LEVEL_MULTI */
#endif
	return rc;
}
/*
 * [CAPIREF: ph7_new_scalar()]
 * Please refer to the official documentation for function purpose and expected parameters.
 */
ph7_value * ph7_new_scalar(ph7_vm *pVm)
{
	ph7_value *pObj;
	/* Ticket 1433-002: NULL VM is harmless operation */
	if ( PH7_VM_MISUSE(pVm) ){
		return 0;
	}
	/* Allocate a new scalar variable */
	pObj = (ph7_value *)SyMemBackendPoolAlloc(&pVm->sAllocator,sizeof(ph7_value));
	if( pObj == 0 ){
		return 0;
	}
	/* Nullify the new scalar */
	PH7_MemObjInit(pVm,pObj);
	return pObj;
}
/*
 * [CAPIREF: ph7_new_array()] 
 * Please refer to the official documentation for function purpose and expected parameters.
 */
ph7_value * ph7_new_array(ph7_vm *pVm)
{
	ph7_hashmap *pMap;
	ph7_value *pObj;
	/* Ticket 1433-002: NULL VM is harmless operation */
	if ( PH7_VM_MISUSE(pVm) ){
		return 0;
	}
	/* Create a new hashmap first */
	pMap = PH7_NewHashmap(&(*pVm),0,0);
	if( pMap == 0 ){
		return 0;
	}
	/* Associate a new ph7_value with this hashmap */
	pObj = (ph7_value *)SyMemBackendPoolAlloc(&pVm->sAllocator,sizeof(ph7_value));
	if( pObj == 0 ){
		PH7_HashmapRelease(pMap,TRUE);
		return 0;
	}
	PH7_MemObjInitFromArray(pVm,pObj,pMap);
	return pObj;
}
/*
 * [CAPIREF: ph7_release_value()]
 * Please refer to the official documentation for function purpose and expected parameters.
 */
int ph7_release_value(ph7_vm *pVm,ph7_value *pValue)
{
	/* Ticket 1433-002: NULL VM is harmless operation */
	if ( PH7_VM_MISUSE(pVm) ){
		return PH7_CORRUPT;
	}
	if( pValue ){
		/* Release the value */
		PH7_MemObjRelease(pValue);
		SyMemBackendPoolFree(&pVm->sAllocator,pValue);
	}
	return PH7_OK;
}
/*
 * [CAPIREF: ph7_value_to_int()]
 * Please refer to the official documentation for function purpose and expected parameters.
 */
int ph7_value_to_int(ph7_value *pValue)
{
	int rc;
	rc = PH7_MemObjToInteger(pValue);
	if( rc != PH7_OK ){
		return 0;
	}
	return (int)pValue->x.iVal;
}
/*
 * [CAPIREF: ph7_value_to_bool()]
 * Please refer to the official documentation for function purpose and expected parameters.
 */
int ph7_value_to_bool(ph7_value *pValue)
{
	int rc;
	rc = PH7_MemObjToBool(pValue);
	if( rc != PH7_OK ){
		return 0;
	}
	return (int)pValue->x.iVal;
}
/*
 * [CAPIREF: ph7_value_to_int64()]
 * Please refer to the official documentation for function purpose and expected parameters.
 */
ph7_int64 ph7_value_to_int64(ph7_value *pValue)
{
	int rc;
	rc = PH7_MemObjToInteger(pValue);
	if( rc != PH7_OK ){
		return 0;
	}
	return pValue->x.iVal;
}
/*
 * [CAPIREF: ph7_value_to_double()]
 * Please refer to the official documentation for function purpose and expected parameters.
 */
double ph7_value_to_double(ph7_value *pValue)
{
	int rc;
	rc = PH7_MemObjToReal(pValue);
	if( rc != PH7_OK ){
		return (double)0;
	}
	return (double)pValue->rVal;
}
/*
 * [CAPIREF: ph7_value_to_string()]
 * Please refer to the official documentation for function purpose and expected parameters.
 */
const char * ph7_value_to_string(ph7_value *pValue,int *pLen)
{
	PH7_MemObjToString(pValue);
	if( SyBlobLength(&pValue->sBlob) > 0 ){
		SyBlobNullAppend(&pValue->sBlob);
		if( pLen ){
			*pLen = (int)SyBlobLength(&pValue->sBlob);
		}
		return (const char *)SyBlobData(&pValue->sBlob);
	}else{
		/* Return the empty string */
		if( pLen ){
			*pLen = 0;
		}
		return "";
	}
}
/*
 * [CAPIREF: ph7_value_to_resource()]
 * Please refer to the official documentation for function purpose and expected parameters.
 */
void * ph7_value_to_resource(ph7_value *pValue)
{
	if( (pValue->iFlags & MEMOBJ_RES) == 0 ){
		/* Not a resource,return NULL */
		return 0;
	}
	return pValue->x.pOther;
}
/*
 * [CAPIREF: ph7_value_compare()]
 * Please refer to the official documentation for function purpose and expected parameters.
 */
int ph7_value_compare(ph7_value *pLeft,ph7_value *pRight,int bStrict)
{
	int rc;
	if( pLeft == 0 || pRight == 0 ){
		/* TICKET 1433-24: NULL values is harmless operation */
		return 1;
	}
	/* Perform the comparison */
	rc = PH7_MemObjCmp(&(*pLeft),&(*pRight),bStrict,0);
	/* Comparison result */
	return rc;
}
/*
 * [CAPIREF: ph7_result_int()]
 * Please refer to the official documentation for function purpose and expected parameters.
 */
int ph7_result_int(ph7_context *pCtx,int iValue)
{
	return ph7_value_int(pCtx->pRet,iValue);
}
/*
 * [CAPIREF: ph7_result_int64()]
 * Please refer to the official documentation for function purpose and expected parameters.
 */
int ph7_result_int64(ph7_context *pCtx,ph7_int64 iValue)
{
	return ph7_value_int64(pCtx->pRet,iValue);
}
/*
 * [CAPIREF: ph7_result_bool()]
 * Please refer to the official documentation for function purpose and expected parameters.
 */
int ph7_result_bool(ph7_context *pCtx,int iBool)
{
	return ph7_value_bool(pCtx->pRet,iBool);
}
/*
 * [CAPIREF: ph7_result_double()]
 * Please refer to the official documentation for function purpose and expected parameters.
 */
int ph7_result_double(ph7_context *pCtx,double Value)
{
	return ph7_value_double(pCtx->pRet,Value);
}
/*
 * [CAPIREF: ph7_result_null()]
 * Please refer to the official documentation for function purpose and expected parameters.
 */
int ph7_result_null(ph7_context *pCtx)
{
	/* Invalidate any prior representation and set the NULL flag */
	PH7_MemObjRelease(pCtx->pRet);
	return PH7_OK;
}
/*
 * [CAPIREF: ph7_result_string()]
 * Please refer to the official documentation for function purpose and expected parameters.
 */
int ph7_result_string(ph7_context *pCtx,const char *zString,int nLen)
{
	return ph7_value_string(pCtx->pRet,zString,nLen);
}
/*
 * [CAPIREF: ph7_result_string_format()]
 * Please refer to the official documentation for function purpose and expected parameters.
 */
int ph7_result_string_format(ph7_context *pCtx,const char *zFormat,...)
{
	ph7_value *p;
	va_list ap;
	int rc;
	p = pCtx->pRet;
	if( (p->iFlags & MEMOBJ_STRING) == 0 ){
		/* Invalidate any prior representation */
		PH7_MemObjRelease(p);
		MemObjSetType(p,MEMOBJ_STRING);
	}
	/* Format the given string */
	va_start(ap,zFormat);
	rc = SyBlobFormatAp(&p->sBlob,zFormat,ap);
	va_end(ap);
	return rc;
}
/*
 * [CAPIREF: ph7_result_value()]
 * Please refer to the official documentation for function purpose and expected parameters.
 */
int ph7_result_value(ph7_context *pCtx,ph7_value *pValue)
{
	int rc = PH7_OK;
	if( pValue == 0 ){
		PH7_MemObjRelease(pCtx->pRet);
	}else{
		rc = PH7_MemObjStore(pValue,pCtx->pRet);
	}
	return rc;
}
/*
 * [CAPIREF: ph7_result_resource()]
 * Please refer to the official documentation for function purpose and expected parameters.
 */
int ph7_result_resource(ph7_context *pCtx,void *pUserData)
{
	return ph7_value_resource(pCtx->pRet,pUserData);
}
/*
 * [CAPIREF: ph7_context_new_scalar()]
 * Please refer to the official documentation for function purpose and expected parameters.
 */
ph7_value * ph7_context_new_scalar(ph7_context *pCtx)
{
	ph7_value *pVal;
	pVal = ph7_new_scalar(pCtx->pVm);
	if( pVal ){
		/* Record value address so it can be freed automatically
		 * when the calling function returns. 
		 */
		SySetPut(&pCtx->sVar,(const void *)&pVal);
	}
	return pVal;
}
/*
 * [CAPIREF: ph7_context_new_array()]
 * Please refer to the official documentation for function purpose and expected parameters.
 */
ph7_value * ph7_context_new_array(ph7_context *pCtx)
{
	ph7_value *pVal;
	pVal = ph7_new_array(pCtx->pVm);
	if( pVal ){
		/* Record value address so it can be freed automatically
		 * when the calling function returns. 
		 */
		SySetPut(&pCtx->sVar,(const void *)&pVal);
	}
	return pVal;
}
/*
 * [CAPIREF: ph7_context_release_value()]
 * Please refer to the official documentation for function purpose and expected parameters.
 */
void ph7_context_release_value(ph7_context *pCtx,ph7_value *pValue)
{
	PH7_VmReleaseContextValue(&(*pCtx),pValue);
}
/*
 * [CAPIREF: ph7_context_alloc_chunk()]
 * Please refer to the official documentation for function purpose and expected parameters.
 */
void * ph7_context_alloc_chunk(ph7_context *pCtx,unsigned int nByte,int ZeroChunk,int AutoRelease)
{
	void *pChunk;
	pChunk = SyMemBackendAlloc(&pCtx->pVm->sAllocator,nByte);
	if( pChunk ){
		if( ZeroChunk ){
			/* Zero the memory chunk */
			SyZero(pChunk,nByte);
		}
		if( AutoRelease ){
			ph7_aux_data sAux;
			/* Track the chunk so that it can be released automatically 
			 * upon this context is destroyed.
			 */
			sAux.pAuxData = pChunk;
			SySetPut(&pCtx->sChunk,(const void *)&sAux);
		}
	}
	return pChunk;
}
/*
 * Check if the given chunk address is registered in the call context
 * chunk container.
 * Return TRUE if registered.FALSE otherwise.
 * Refer to [ph7_context_realloc_chunk(),ph7_context_free_chunk()].
 */
static ph7_aux_data * ContextFindChunk(ph7_context *pCtx,void *pChunk)
{
	ph7_aux_data *aAux,*pAux;
	sxu32 n;
	if( SySetUsed(&pCtx->sChunk) < 1 ){
		/* Don't bother processing,the container is empty */
		return 0;
	}
	/* Perform the lookup */
	aAux = (ph7_aux_data *)SySetBasePtr(&pCtx->sChunk);
	for( n = 0; n < SySetUsed(&pCtx->sChunk) ; ++n ){
		pAux = &aAux[n];
		if( pAux->pAuxData == pChunk ){
			/* Chunk found */
			return pAux;
		}
	}
	/* No such allocated chunk */
	return 0;
}
/*
 * [CAPIREF: ph7_context_realloc_chunk()]
 * Please refer to the official documentation for function purpose and expected parameters.
 */
void * ph7_context_realloc_chunk(ph7_context *pCtx,void *pChunk,unsigned int nByte)
{
	ph7_aux_data *pAux;
	void *pNew;
	pNew = SyMemBackendRealloc(&pCtx->pVm->sAllocator,pChunk,nByte);
	if( pNew ){
		pAux = ContextFindChunk(pCtx,pChunk);
		if( pAux ){
			pAux->pAuxData = pNew;
		}
	}
	return pNew;
}
/*
 * [CAPIREF: ph7_context_free_chunk()]
 * Please refer to the official documentation for function purpose and expected parameters.
 */
void ph7_context_free_chunk(ph7_context *pCtx,void *pChunk)
{
	ph7_aux_data *pAux;
	if( pChunk == 0 ){
		/* TICKET-1433-93: NULL chunk is a harmless operation */
		return;
	}
	pAux = ContextFindChunk(pCtx,pChunk);
	if( pAux ){
		/* Mark as destroyed */
		pAux->pAuxData = 0;
	}
	SyMemBackendFree(&pCtx->pVm->sAllocator,pChunk);
}
/*
 * [CAPIREF: ph7_array_fetch()]
 * Please refer to the official documentation for function purpose and expected parameters.
 */
ph7_value * ph7_array_fetch(ph7_value *pArray,const char *zKey,int nByte)
{
	ph7_hashmap_node *pNode;
	ph7_value *pValue;
	ph7_value skey;
	int rc;
	/* Make sure we are dealing with a valid hashmap */
	if( (pArray->iFlags & MEMOBJ_HASHMAP) == 0 ){
		return 0;
	}
	if( nByte < 0 ){
		nByte = (int)SyStrlen(zKey);
	}
	/* Convert the key to a ph7_value  */
	PH7_MemObjInit(pArray->pVm,&skey);
	PH7_MemObjStringAppend(&skey,zKey,(sxu32)nByte);
	/* Perform the lookup */
	rc = PH7_HashmapLookup((ph7_hashmap *)pArray->x.pOther,&skey,&pNode);
	PH7_MemObjRelease(&skey);
	if( rc != PH7_OK ){
		/* No such entry */
		return 0;
	}
	/* Extract the target value */
	pValue = (ph7_value *)SySetAt(&pArray->pVm->aMemObj,pNode->nValIdx);
	return pValue;
}
/*
 * [CAPIREF: ph7_array_walk()]
 * Please refer to the official documentation for function purpose and expected parameters.
 */
int ph7_array_walk(ph7_value *pArray,int (*xWalk)(ph7_value *pValue,ph7_value *,void *),void *pUserData)
{
	int rc;
	if( xWalk == 0 ){
		return PH7_CORRUPT;
	}
	/* Make sure we are dealing with a valid hashmap */
	if( (pArray->iFlags & MEMOBJ_HASHMAP) == 0 ){
		return PH7_CORRUPT;
	}
	/* Start the walk process */
	rc = PH7_HashmapWalk((ph7_hashmap *)pArray->x.pOther,xWalk,pUserData);
	return rc != PH7_OK ? PH7_ABORT /* User callback request an operation abort*/ : PH7_OK;
}
/*
 * [CAPIREF: ph7_array_add_elem()]
 * Please refer to the official documentation for function purpose and expected parameters.
 */
int ph7_array_add_elem(ph7_value *pArray,ph7_value *pKey,ph7_value *pValue)
{
	int rc;
	/* Make sure we are dealing with a valid hashmap */
	if( (pArray->iFlags & MEMOBJ_HASHMAP) == 0 ){
		return PH7_CORRUPT;
	}
	/* Perform the insertion */
	rc = PH7_HashmapInsert((ph7_hashmap *)pArray->x.pOther,&(*pKey),&(*pValue));
	return rc;
}
/*
 * [CAPIREF: ph7_array_add_strkey_elem()]
 * Please refer to the official documentation for function purpose and expected parameters.
 */
int ph7_array_add_strkey_elem(ph7_value *pArray,const char *zKey,ph7_value *pValue)
{
	int rc;
	/* Make sure we are dealing with a valid hashmap */
	if( (pArray->iFlags & MEMOBJ_HASHMAP) == 0 ){
		return PH7_CORRUPT;
	}
	/* Perform the insertion */
	if( SX_EMPTY_STR(zKey) ){
		/* Empty key,assign an automatic index */
		rc = PH7_HashmapInsert((ph7_hashmap *)pArray->x.pOther,0,&(*pValue));
	}else{
		ph7_value sKey;
		PH7_MemObjInitFromString(pArray->pVm,&sKey,0);
		PH7_MemObjStringAppend(&sKey,zKey,(sxu32)SyStrlen(zKey));
		rc = PH7_HashmapInsert((ph7_hashmap *)pArray->x.pOther,&sKey,&(*pValue));
		PH7_MemObjRelease(&sKey);
	}
	return rc;
}
/*
 * [CAPIREF: ph7_array_add_intkey_elem()]
 * Please refer to the official documentation for function purpose and expected parameters.
 */
int ph7_array_add_intkey_elem(ph7_value *pArray,int iKey,ph7_value *pValue)
{
	ph7_value sKey;
	int rc;
	/* Make sure we are dealing with a valid hashmap */
	if( (pArray->iFlags & MEMOBJ_HASHMAP) == 0 ){
		return PH7_CORRUPT;
	}
	PH7_MemObjInitFromInt(pArray->pVm,&sKey,iKey);
	/* Perform the insertion */
	rc = PH7_HashmapInsert((ph7_hashmap *)pArray->x.pOther,&sKey,&(*pValue));
	PH7_MemObjRelease(&sKey);
	return rc;
}
/*
 * [CAPIREF: ph7_array_count()]
 * Please refer to the official documentation for function purpose and expected parameters.
 */
unsigned int ph7_array_count(ph7_value *pArray)
{
	ph7_hashmap *pMap;
	/* Make sure we are dealing with a valid hashmap */
	if( (pArray->iFlags & MEMOBJ_HASHMAP) == 0 ){
		return 0;
	}
	/* Point to the internal representation of the hashmap */
	pMap = (ph7_hashmap *)pArray->x.pOther;
	return pMap->nEntry;
}
/*
 * [CAPIREF: ph7_object_walk()]
 * Please refer to the official documentation for function purpose and expected parameters.
 */
int ph7_object_walk(ph7_value *pObject,int (*xWalk)(const char *,ph7_value *,void *),void *pUserData)
{
	int rc;
	if( xWalk == 0 ){
		return PH7_CORRUPT;
	}
	/* Make sure we are dealing with a valid class instance */
	if( (pObject->iFlags & MEMOBJ_OBJ) == 0 ){
		return PH7_CORRUPT;
	}
	/* Start the walk process */
	rc = PH7_ClassInstanceWalk((ph7_class_instance *)pObject->x.pOther,xWalk,pUserData);
	return rc != PH7_OK ? PH7_ABORT /* User callback request an operation abort*/ : PH7_OK;
}
/*
 * [CAPIREF: ph7_object_fetch_attr()]
 * Please refer to the official documentation for function purpose and expected parameters.
 */
ph7_value * ph7_object_fetch_attr(ph7_value *pObject,const char *zAttr)
{
	ph7_value *pValue;
	SyString sAttr;
	/* Make sure we are dealing with a valid class instance */
	if( (pObject->iFlags & MEMOBJ_OBJ) == 0 || zAttr == 0 ){
		return 0; 
	}
	SyStringInitFromBuf(&sAttr,zAttr,SyStrlen(zAttr));
	/* Extract the attribute value if available.
	 */
	pValue = PH7_ClassInstanceFetchAttr((ph7_class_instance *)pObject->x.pOther,&sAttr);
	return pValue;
}
/*
 * [CAPIREF: ph7_object_get_class_name()]
 * Please refer to the official documentation for function purpose and expected parameters.
 */
const char * ph7_object_get_class_name(ph7_value *pObject,int *pLength)
{
	ph7_class *pClass;
	if( pLength ){
		*pLength = 0;
	}
	/* Make sure we are dealing with a valid class instance */
	if( (pObject->iFlags & MEMOBJ_OBJ) == 0  ){
		return 0; 
	}
	/* Point to the class */
	pClass = ((ph7_class_instance *)pObject->x.pOther)->pClass;
	/* Return the class name */
	if( pLength ){
		*pLength = (int)SyStringLength(&pClass->sName);
	}
	return SyStringData(&pClass->sName);
}
/*
 * [CAPIREF: ph7_context_output()]
 * Please refer to the official documentation for function purpose and expected parameters.
 */
int ph7_context_output(ph7_context *pCtx,const char *zString,int nLen)
{
	SyString sData;
	int rc;
	if( nLen < 0 ){
		nLen = (int)SyStrlen(zString);
	}
	SyStringInitFromBuf(&sData,zString,nLen);
	rc = PH7_VmOutputConsume(pCtx->pVm,&sData);
	return rc;
}
/*
 * [CAPIREF: ph7_context_output_format()]
 * Please refer to the official documentation for function purpose and expected parameters.
 */
int ph7_context_output_format(ph7_context *pCtx,const char *zFormat,...)
{
	va_list ap;
	int rc;
	va_start(ap,zFormat);
	rc = PH7_VmOutputConsumeAp(pCtx->pVm,zFormat,ap);
	va_end(ap);
	return rc;
}
/*
 * [CAPIREF: ph7_context_throw_error()]
 * Please refer to the official documentation for function purpose and expected parameters.
 */
int ph7_context_throw_error(ph7_context *pCtx,int iErr,const char *zErr)
{
	int rc = PH7_OK;
	if( zErr ){
		rc = PH7_VmThrowError(pCtx->pVm,&pCtx->pFunc->sName,iErr,zErr);
	}
	return rc;
}
/*
 * [CAPIREF: ph7_context_throw_error_format()]
 * Please refer to the official documentation for function purpose and expected parameters.
 */
int ph7_context_throw_error_format(ph7_context *pCtx,int iErr,const char *zFormat,...)
{
	va_list ap;
	int rc;
	if( zFormat == 0){
		return PH7_OK;
	}
	va_start(ap,zFormat);
	rc = PH7_VmThrowErrorAp(pCtx->pVm,&pCtx->pFunc->sName,iErr,zFormat,ap);
	va_end(ap);
	return rc;
}
/*
 * [CAPIREF: ph7_context_random_num()]
 * Please refer to the official documentation for function purpose and expected parameters.
 */
unsigned int ph7_context_random_num(ph7_context *pCtx)
{
	sxu32 n;
	n = PH7_VmRandomNum(pCtx->pVm);
	return n;
}
/*
 * [CAPIREF: ph7_context_random_string()]
 * Please refer to the official documentation for function purpose and expected parameters.
 */
int ph7_context_random_string(ph7_context *pCtx,char *zBuf,int nBuflen)
{
	if( nBuflen < 3 ){
		return PH7_CORRUPT;
	}
	PH7_VmRandomString(pCtx->pVm,zBuf,nBuflen);
	return PH7_OK;
}
/*
 * IMP-12-07-2012 02:10 Experimantal public API.
 *
 * ph7_vm * ph7_context_get_vm(ph7_context *pCtx)
 * {
 *	return pCtx->pVm;
 * }
 */
/*
 * [CAPIREF: ph7_context_user_data()]
 * Please refer to the official documentation for function purpose and expected parameters.
 */
void * ph7_context_user_data(ph7_context *pCtx)
{
	return pCtx->pFunc->pUserData;
}
/*
 * [CAPIREF: ph7_context_push_aux_data()]
 * Please refer to the official documentation for function purpose and expected parameters.
 */
int ph7_context_push_aux_data(ph7_context *pCtx,void *pUserData)
{
	ph7_aux_data sAux;
	int rc;
	sAux.pAuxData = pUserData;
	rc = SySetPut(&pCtx->pFunc->aAux,(const void *)&sAux);
	return rc;
}
/*
 * [CAPIREF: ph7_context_peek_aux_data()]
 * Please refer to the official documentation for function purpose and expected parameters.
 */
void * ph7_context_peek_aux_data(ph7_context *pCtx)
{
	ph7_aux_data *pAux;
	pAux = (ph7_aux_data *)SySetPeek(&pCtx->pFunc->aAux);
	return pAux ? pAux->pAuxData : 0;
}
/*
 * [CAPIREF: ph7_context_pop_aux_data()]
 * Please refer to the official documentation for function purpose and expected parameters.
 */
void * ph7_context_pop_aux_data(ph7_context *pCtx)
{
	ph7_aux_data *pAux;
	pAux = (ph7_aux_data *)SySetPop(&pCtx->pFunc->aAux);
	return pAux ? pAux->pAuxData : 0;
}
/*
 * [CAPIREF: ph7_context_result_buf_length()]
 * Please refer to the official documentation for function purpose and expected parameters.
 */
unsigned int ph7_context_result_buf_length(ph7_context *pCtx)
{
	return SyBlobLength(&pCtx->pRet->sBlob);
}
/*
 * [CAPIREF: ph7_function_name()]
 * Please refer to the official documentation for function purpose and expected parameters.
 */
const char * ph7_function_name(ph7_context *pCtx)
{
	SyString *pName;
	pName = &pCtx->pFunc->sName;
	return pName->zString;
}
/*
 * [CAPIREF: ph7_value_int()]
 * Please refer to the official documentation for function purpose and expected parameters.
 */
int ph7_value_int(ph7_value *pVal,int iValue)
{
	/* Invalidate any prior representation */
	PH7_MemObjRelease(pVal);
	pVal->x.iVal = (ph7_int64)iValue;
	MemObjSetType(pVal,MEMOBJ_INT);
	return PH7_OK;
}
/*
 * [CAPIREF: ph7_value_int64()]
 * Please refer to the official documentation for function purpose and expected parameters.
 */
int ph7_value_int64(ph7_value *pVal,ph7_int64 iValue)
{
	/* Invalidate any prior representation */
	PH7_MemObjRelease(pVal);
	pVal->x.iVal = iValue;
	MemObjSetType(pVal,MEMOBJ_INT);
	return PH7_OK;
}
/*
 * [CAPIREF: ph7_value_bool()]
 * Please refer to the official documentation for function purpose and expected parameters.
 */
int ph7_value_bool(ph7_value *pVal,int iBool)
{
	/* Invalidate any prior representation */
	PH7_MemObjRelease(pVal);
	pVal->x.iVal = iBool ? 1 : 0;
	MemObjSetType(pVal,MEMOBJ_BOOL);
	return PH7_OK;
}
/*
 * [CAPIREF: ph7_value_null()]
 * Please refer to the official documentation for function purpose and expected parameters.
 */
int ph7_value_null(ph7_value *pVal)
{
	/* Invalidate any prior representation and set the NULL flag */
	PH7_MemObjRelease(pVal);
	return PH7_OK;
}
/*
 * [CAPIREF: ph7_value_double()]
 * Please refer to the official documentation for function purpose and expected parameters.
 */
int ph7_value_double(ph7_value *pVal,double Value)
{
	/* Invalidate any prior representation */
	PH7_MemObjRelease(pVal);
	pVal->rVal = (ph7_real)Value;
	MemObjSetType(pVal,MEMOBJ_REAL);
	/* Try to get an integer representation also */
	PH7_MemObjTryInteger(pVal);
	return PH7_OK;
}
/*
 * [CAPIREF: ph7_value_string()]
 * Please refer to the official documentation for function purpose and expected parameters.
 */
int ph7_value_string(ph7_value *pVal,const char *zString,int nLen)
{
	if((pVal->iFlags & MEMOBJ_STRING) == 0 ){
		/* Invalidate any prior representation */
		PH7_MemObjRelease(pVal);
		MemObjSetType(pVal,MEMOBJ_STRING);
	}
	if( zString ){
		if( nLen < 0 ){
			/* Compute length automatically */
			nLen = (int)SyStrlen(zString);
		}
		SyBlobAppend(&pVal->sBlob,(const void *)zString,(sxu32)nLen);
	}
	return PH7_OK;
}
/*
 * [CAPIREF: ph7_value_string_format()]
 * Please refer to the official documentation for function purpose and expected parameters.
 */
int ph7_value_string_format(ph7_value *pVal,const char *zFormat,...)
{
	va_list ap;
	int rc;
	if((pVal->iFlags & MEMOBJ_STRING) == 0 ){
		/* Invalidate any prior representation */
		PH7_MemObjRelease(pVal);
		MemObjSetType(pVal,MEMOBJ_STRING);
	}
	va_start(ap,zFormat);
	rc = SyBlobFormatAp(&pVal->sBlob,zFormat,ap);
	va_end(ap);
	return PH7_OK;
}
/*
 * [CAPIREF: ph7_value_reset_string_cursor()]
 * Please refer to the official documentation for function purpose and expected parameters.
 */
int ph7_value_reset_string_cursor(ph7_value *pVal)
{
	/* Reset the string cursor */
	SyBlobReset(&pVal->sBlob);
	return PH7_OK;
}
/*
 * [CAPIREF: ph7_value_resource()]
 * Please refer to the official documentation for function purpose and expected parameters.
 */
int ph7_value_resource(ph7_value *pVal,void *pUserData)
{
	/* Invalidate any prior representation */
	PH7_MemObjRelease(pVal);
	/* Reflect the new type */
	pVal->x.pOther = pUserData;
	MemObjSetType(pVal,MEMOBJ_RES);
	return PH7_OK;
}
/*
 * [CAPIREF: ph7_value_release()]
 * Please refer to the official documentation for function purpose and expected parameters.
 */
int ph7_value_release(ph7_value *pVal)
{
	PH7_MemObjRelease(pVal);
	return PH7_OK;
}
/*
 * [CAPIREF: ph7_value_is_int()]
 * Please refer to the official documentation for function purpose and expected parameters.
 */
int ph7_value_is_int(ph7_value *pVal)
{
	return (pVal->iFlags & MEMOBJ_INT) ? TRUE : FALSE;
}
/*
 * [CAPIREF: ph7_value_is_float()]
 * Please refer to the official documentation for function purpose and expected parameters.
 */
int ph7_value_is_float(ph7_value *pVal)
{
	return (pVal->iFlags & MEMOBJ_REAL) ? TRUE : FALSE;
}
/*
 * [CAPIREF: ph7_value_is_bool()]
 * Please refer to the official documentation for function purpose and expected parameters.
 */
int ph7_value_is_bool(ph7_value *pVal)
{
	return (pVal->iFlags & MEMOBJ_BOOL) ? TRUE : FALSE;
}
/*
 * [CAPIREF: ph7_value_is_string()]
 * Please refer to the official documentation for function purpose and expected parameters.
 */
int ph7_value_is_string(ph7_value *pVal)
{
	return (pVal->iFlags & MEMOBJ_STRING) ? TRUE : FALSE;
}
/*
 * [CAPIREF: ph7_value_is_null()]
 * Please refer to the official documentation for function purpose and expected parameters.
 */
int ph7_value_is_null(ph7_value *pVal)
{
	return (pVal->iFlags & MEMOBJ_NULL) ? TRUE : FALSE;
}
/*
 * [CAPIREF: ph7_value_is_numeric()]
 * Please refer to the official documentation for function purpose and expected parameters.
 */
int ph7_value_is_numeric(ph7_value *pVal)
{
	int rc;
	rc = PH7_MemObjIsNumeric(pVal);
	return rc;
}
/*
 * [CAPIREF: ph7_value_is_callable()]
 * Please refer to the official documentation for function purpose and expected parameters.
 */
int ph7_value_is_callable(ph7_value *pVal)
{
	int rc;
	rc = PH7_VmIsCallable(pVal->pVm,pVal,FALSE);
	return rc;
}
/*
 * [CAPIREF: ph7_value_is_scalar()]
 * Please refer to the official documentation for function purpose and expected parameters.
 */
int ph7_value_is_scalar(ph7_value *pVal)
{
	return (pVal->iFlags & MEMOBJ_SCALAR) ? TRUE : FALSE;
}
/*
 * [CAPIREF: ph7_value_is_array()]
 * Please refer to the official documentation for function purpose and expected parameters.
 */
int ph7_value_is_array(ph7_value *pVal)
{
	return (pVal->iFlags & MEMOBJ_HASHMAP) ? TRUE : FALSE;
}
/*
 * [CAPIREF: ph7_value_is_object()]
 * Please refer to the official documentation for function purpose and expected parameters.
 */
int ph7_value_is_object(ph7_value *pVal)
{
	return (pVal->iFlags & MEMOBJ_OBJ) ? TRUE : FALSE;
}
/*
 * [CAPIREF: ph7_value_is_resource()]
 * Please refer to the official documentation for function purpose and expected parameters.
 */
int ph7_value_is_resource(ph7_value *pVal)
{
	return (pVal->iFlags & MEMOBJ_RES) ? TRUE : FALSE;
}
/*
 * [CAPIREF: ph7_value_is_empty()]
 * Please refer to the official documentation for function purpose and expected parameters.
 */
int ph7_value_is_empty(ph7_value *pVal)
{
	int rc;
	rc = PH7_MemObjIsEmpty(pVal);
	return rc;
}
/* END-OF-IMPLEMENTATION: ph7@embedded@symisc 345-09-46 */
/*
 * Symisc PH7: An embeddable bytecode compiler and a virtual machine for the PHP(5) programming language.
 * Copyright (C) 2011-2012,Symisc Systems http://ph7.symisc.net/
 * Version 2.1.4
 * For information on licensing,redistribution of this file,and for a DISCLAIMER OF ALL WARRANTIES
 * please contact Symisc Systems via:
 *       legal@symisc.net
 *       licensing@symisc.net
 *       contact@symisc.net
 * or visit:
 *      http://ph7.symisc.net/
 */
/*
 * Copyright (C) 2011, 2012 Symisc Systems. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. Redistributions in any form must be accompanied by information on
 *    how to obtain complete source code for the PH7 engine and any 
 *    accompanying software that uses the PH7 engine software.
 *    The source code must either be included in the distribution
 *    or be available for no more than the cost of distribution plus
 *    a nominal fee, and must be freely redistributable under reasonable
 *    conditions. For an executable file, complete source code means
 *    the source code for all modules it contains.It does not include
 *    source code for modules or files that typically accompany the major
 *    components of the operating system on which the executable file runs.
 *
 * THIS SOFTWARE IS PROVIDED BY SYMISC SYSTEMS ``AS IS'' AND ANY EXPRESS
 * OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE, OR
 * NON-INFRINGEMENT, ARE DISCLAIMED.  IN NO EVENT SHALL SYMISC SYSTEMS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR
 * BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
 * WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE
 * OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN
 * IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */
