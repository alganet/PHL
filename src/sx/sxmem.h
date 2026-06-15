/**
 * SPDX-FileCopyrightText: 2011, 2012, 2013, 2014 Symisc Systems <licensing@symisc.net>
 * SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
 * SPDX-License-Identifier: BSD-3-Clause
 */
#ifndef __SXMEM_H__
#define __SXMEM_H__

#include "sxtypes.h"

/* Memory pool constants */
#define SXMEM_POOL_INCR       3
#define SXMEM_POOL_NBUCKETS   12
#define SXMEM_BACKEND_MAGIC   0xBAC3E67D
#define SXMEM_BACKEND_CORRUPT(BACKEND) (BACKEND == 0 || BACKEND->nMagic != SXMEM_BACKEND_MAGIC)
#define SXMEM_BACKEND_RETRY   3

/* A memory backend subsystem is defined by an instance of the following structures */
typedef union SyMemHeader SyMemHeader;
typedef struct SyMemBlock SyMemBlock;

struct SyMemBlock
{
	SyMemBlock *pNext,*pPrev; /* Chain of allocated memory blocks */
#ifdef UNTRUST
	sxu32 nGuard;             /* magic number associated with each valid block,so we
	                           * can detect misuse.
	                           */
#endif
};

/*
 * Header associated with each valid memory pool block.
 */
union SyMemHeader
{
	SyMemHeader *pNext; /* Next chunk of size 1 << (nBucket + SXMEM_POOL_INCR) in the list */
	sxu32 nBucket;      /* Bucket index in aPool[] */
};

struct SyMemBackend
{
	const SyMutexMethods *pMutexMethods; /* Mutex methods */
	const SyMemMethods *pMethods;  /* Memory allocation methods */
	SyMemBlock *pBlocks;           /* List of valid memory blocks */
	sxu32 nBlock;                  /* Total number of memory blocks allocated so far */
	ProcMemError xMemError;        /* Out-of memory callback */
	void *pUserData;               /* First arg to xMemError() */
	SyMutex *pMutex;               /* Per instance mutex */
	sxu32 nMagic;                  /* Sanity check against misuse */
	sxu32 nMaxRequest;             /* Per-allocation cap in bytes (0 = unlimited); fails
	                                * any single alloc/realloc larger than this. Used to
	                                * exercise out-of-memory paths deterministically. */
	SyMemHeader *apPool[SXMEM_POOL_NBUCKETS+SXMEM_POOL_INCR]; /* Pool of memory chunks */
};

/* Memory backend function prototypes */
PH7_PRIVATE sxi32 SyMemBackendInit(SyMemBackend *pBackend,ProcMemError xMemErr,void *pUserData);
PH7_PRIVATE sxi32 SyMemBackendInitFromOthers(SyMemBackend *pBackend,const SyMemMethods *pMethods,ProcMemError xMemErr,void *pUserData);
PH7_PRIVATE sxi32 SyMemBackendInitFromParent(SyMemBackend *pBackend,SyMemBackend *pParent);
PH7_PRIVATE sxi32 SyMemBackendRelease(SyMemBackend *pBackend);
PH7_PRIVATE void *SyMemBackendAlloc(SyMemBackend *pBackend,sxu32 nByte);
PH7_PRIVATE void *SyMemBackendRealloc(SyMemBackend *pBackend,void *pOld,sxu32 nByte);
PH7_PRIVATE sxi32 SyMemBackendFree(SyMemBackend *pBackend,void *pChunk);
PH7_PRIVATE void *SyMemBackendPoolAlloc(SyMemBackend *pBackend,sxu32 nByte);
PH7_PRIVATE sxi32 SyMemBackendPoolFree(SyMemBackend *pBackend,void *pChunk);
PH7_PRIVATE void *SyMemBackendDup(SyMemBackend *pBackend,const void *pSrc,sxu32 nSize);
PH7_PRIVATE char *SyMemBackendStrDup(SyMemBackend *pBackend,const char *zSrc,sxu32 nSize);
#if defined(PH7_ENABLE_THREADS)
PH7_PRIVATE sxi32 SyMemBackendMakeThreadSafe(SyMemBackend *pBackend,const SyMutexMethods *pMethods);
PH7_PRIVATE sxi32 SyMemBackendDisbaleMutexing(SyMemBackend *pBackend);
#endif

#endif /* __SXMEM_H__ */
