/**
 * SPDX-FileCopyrightText: 2011, 2012, 2013, 2014 Symisc Systems <licensing@symisc.net>
 * SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
 * SPDX-License-Identifier: BSD-3-Clause
 */
#ifndef __SXHASHTABLE_H__
#define __SXHASHTABLE_H__

#include "sxtypes.h"
#include "sxmem.h"

/* Forward declarations */
typedef struct SyHashEntry_Pr SyHashEntry_Pr;
typedef struct SyHashEntry SyHashEntry;
typedef struct SyHash SyHash;

/*
 * Each public hashtable entry is represented by an instance
 * of the following structure.
 */
struct SyHashEntry
{
	const void *pKey; /* Hash key */
	sxu32 nKeyLen;    /* Key length */
	void *pUserData;  /* User private data */
};

#define SyHashEntryGetUserData(ENTRY) ((ENTRY)->pUserData)
#define SyHashEntryGetKey(ENTRY)      ((ENTRY)->pKey)

/*
 * Each active hashtable is identified by an instance of the following structure.
 */
struct SyHash
{
	SyMemBackend *pAllocator;         /* Memory backend */
	ProcHash xHash;                   /* Hash function */
	ProcCmp xCmp;                     /* Comparison function */
	SyHashEntry_Pr *pList,*pCurrent;  /* Linked list of hash entries for linear traversal */
	SyHashEntry_Pr *pLast;            /* Tail of pList — O(1) append for the tail-insert path */
	sxu32 nEntry;                     /* Total number of entries */
	SyHashEntry_Pr **apBucket;        /* Hash buckets */
	sxu32 nBucketSize;                /* Current bucket size */
};

/* Hashtable constants */
#define SXHASH_BUCKET_SIZE 16 /* Initial bucket size: must be a power of two */
#define SXHASH_FILL_FACTOR 3

/* Hash access macros */
#define SyHashFunc(HASH)       ((HASH)->xHash)
#define SyHashCmpFunc(HASH)    ((HASH)->xCmp)
#define SyHashTotalEntry(HASH) ((HASH)->nEntry)
#define SyHashGetPool(HASH)    ((HASH)->pAllocator)

/* Hashtable function prototypes */
PH7_PRIVATE sxi32 SyHashInit(SyHash *pHash,SyMemBackend *pAllocator,ProcHash xHash,ProcCmp xCmp);
PH7_PRIVATE sxi32 SyHashRelease(SyHash *pHash);
PH7_PRIVATE SyHashEntry *SyHashGet(SyHash *pHash,const void *pKey,sxu32 nKeyLen);
PH7_PRIVATE sxi32 SyHashDeleteEntry(SyHash *pHash,const void *pKey,sxu32 nKeyLen,void **ppUserData);
PH7_PRIVATE sxi32 SyHashDeleteEntry2(SyHashEntry *pEntry);
PH7_PRIVATE sxi32 SyHashResetLoopCursor(SyHash *pHash);
PH7_PRIVATE SyHashEntry *SyHashGetNextEntry(SyHash *pHash);
PH7_PRIVATE sxi32 SyHashForEach(SyHash *pHash,sxi32(*xStep)(SyHashEntry *,void *),void *pUserData);
PH7_PRIVATE sxi32 SyHashInsert(SyHash *pHash,const void *pKey,sxu32 nKeyLen,void *pUserData);
PH7_PRIVATE SyHashEntry *SyHashLastEntry(SyHash *pHash);

#endif /* __SXHASHTABLE_H__ */
