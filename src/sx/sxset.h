/**
 * SPDX-FileCopyrightText: 2011, 2012, 2013, 2014 Symisc Systems <licensing@symisc.net>
 * SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
 * SPDX-License-Identifier: BSD-3-Clause
 */
#ifndef __SXSET_H__
#define __SXSET_H__

#include "sxtypes.h"

/* Forward declaration */
typedef struct SyMemBackend SyMemBackend;

/*
 * A generic dynamic set.
 */
struct SySet
{
	SyMemBackend *pAllocator; /* Memory backend */
	void *pBase;              /* Base pointer */
	sxu32 nUsed;              /* Total number of used slots  */
	sxu32 nSize;              /* Total number of available slots */
	sxu32 eSize;              /* Size of a single slot */
	sxu32 nCursor;            /* Loop cursor */
	void *pUserData;          /* User private data associated with this container */
};

/* SySet access macros */
#define SySetBasePtr(S)           ((S)->pBase)
#define SySetBasePtrJump(S,OFFT)  (&((char *)(S)->pBase)[OFFT*(S)->eSize])
#define SySetUsed(S)              ((S)->nUsed)
#define SySetSize(S)              ((S)->nSize)
#define SySetElemSize(S)          ((S)->eSize)
#define SySetCursor(S)            ((S)->nCursor)
#define SySetGetAllocator(S)      ((S)->pAllocator)
#define SySetSetUserData(S,DATA)  ((S)->pUserData = DATA)
#define SySetGetUserData(S)       ((S)->pUserData)

/*
 * A variable length container for generic data.
 */
struct SyBlob
{
	SyMemBackend *pAllocator; /* Memory backend */
	void   *pBlob;            /* Base pointer */
	sxu32  nByte;             /* Total number of used bytes */
	sxu32  mByte;             /* Total number of available bytes */
	sxu32  nFlags;            /* Blob internal flags, see below */
};

/* Blob flags */
#define SXBLOB_LOCKED  0x01  /* Blob is locked [i.e: Cannot auto grow] */
#define SXBLOB_STATIC  0x02  /* Not allocated from heap */
#define SXBLOB_RDONLY  0x04  /* Read-Only data */

/* SyBlob access macros */
#define SyBlobFreeSpace(BLOB)    ((BLOB)->mByte - (BLOB)->nByte)
#define SyBlobLength(BLOB)       ((BLOB)->nByte)
#define SyBlobData(BLOB)         ((BLOB)->pBlob)
#define SyBlobCurData(BLOB)      ((void*)(&((char*)(BLOB)->pBlob)[(BLOB)->nByte]))
#define SyBlobDataAt(BLOB,OFFT)  ((void *)(&((char *)(BLOB)->pBlob)[OFFT]))
#define SyBlobGetAllocator(BLOB) ((BLOB)->pAllocator)

/* SySet function prototypes */
PH7_PRIVATE sxi32 SySetInit(SySet *pSet,SyMemBackend *pAllocator,sxu32 ElemSize);
PH7_PRIVATE sxi32 SySetPut(SySet *pSet,const void *pItem);
PH7_PRIVATE sxi32 SySetAlloc(SySet *pSet,sxi32 nItem);
PH7_PRIVATE sxi32 SySetReset(SySet *pSet);
PH7_PRIVATE sxi32 SySetResetCursor(SySet *pSet);
PH7_PRIVATE sxi32 SySetGetNextEntry(SySet *pSet,void **ppEntry);
#ifndef PH7_DISABLE_BUILTIN_FUNC
PH7_PRIVATE void * SySetPeekCurrentEntry(SySet *pSet);
#endif /* PH7_DISABLE_BUILTIN_FUNC */
PH7_PRIVATE sxi32 SySetTruncate(SySet *pSet,sxu32 nNewSize);
PH7_PRIVATE sxi32 SySetRelease(SySet *pSet);
PH7_PRIVATE void * SySetPeek(SySet *pSet);
PH7_PRIVATE void * SySetPop(SySet *pSet);
PH7_PRIVATE void * SySetAt(SySet *pSet,sxu32 nIdx);

/* SyBlob function prototypes */
PH7_PRIVATE sxi32 SyBlobInit(SyBlob *pBlob,SyMemBackend *pAllocator);
PH7_PRIVATE sxi32 SyBlobInitFromBuf(SyBlob *pBlob,void *pBuffer,sxu32 nSize);
PH7_PRIVATE sxi32 SyBlobReadOnly(SyBlob *pBlob,const void *pData,sxu32 nByte);
PH7_PRIVATE sxi32 SyBlobAppend(SyBlob *pBlob,const void *pData,sxu32 nSize);
PH7_PRIVATE sxi32 SyBlobNullAppend(SyBlob *pBlob);
PH7_PRIVATE sxi32 SyBlobDup(SyBlob *pSrc,SyBlob *pDest);
PH7_PRIVATE sxi32 SyBlobCmp(SyBlob *pLeft,SyBlob *pRight);
PH7_PRIVATE sxi32 SyBlobReset(SyBlob *pBlob);
PH7_PRIVATE sxi32 SyBlobRelease(SyBlob *pBlob);
#if !defined(PH7_DISABLE_BUILTIN_FUNC) || !defined(PH7_DISABLE_DISK_IO)
PH7_PRIVATE sxi32 SyBlobSearch(const void *pBlob,sxu32 nLen,const void *pPattern,sxu32 pLen,sxu32 *pOfft);
#endif /* PH7_DISABLE_BUILTIN_FUNC || PH7_DISABLE_DISK_IO */

#endif /* __SXSET_H__ */
