# src/sx/sxset.h

<style>code, pre { background: none !important; white-space: pre !important; width: 100% !important; display: inline-block !important; } td { border: none !important; margin-top: 0 !important; margin-bottom: 0 !important; padding-top: 0 !important; padding-bottom: 0 !important; }</style>

Coverage: 6/6 lines (100.00%)

[Root index](../../index.md) | [Directory index](index.md)

|     Hits | Line | Source |
| -------: | ---: | :--- |
|        - |    1 | `/**` |
|        - |    2 | ` * SPDX-FileCopyrightText: 2011, 2012, 2013, 2014 Symisc Systems <licensing@symisc.net>` |
|        - |    3 | ` * SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>` |
|        - |    4 | ` * SPDX-License-Identifier: BSD-3-Clause` |
|        - |    5 | ` */` |
|        - |    6 | `#ifndef __SXSET_H__` |
|        - |    7 | `#define __SXSET_H__` |
|        - |    8 |  |
|        - |    9 | `#include "sxtypes.h"` |
|        - |   10 |  |
|        - |   11 | `/* Forward declaration */` |
|        - |   12 | `typedef struct SyMemBackend SyMemBackend;` |
|        - |   13 |  |
|        - |   14 | `/*` |
|        - |   15 | ` * A generic dynamic set.` |
|        - |   16 | ` */` |
|        - |   17 | `struct SySet` |
|        - |   18 | `{` |
|        - |   19 | `	SyMemBackend *pAllocator; /* Memory backend */` |
|        - |   20 | `	void *pBase;              /* Base pointer */` |
|        - |   21 | `	sxu32 nUsed;              /* Total number of used slots  */` |
|        - |   22 | `	sxu32 nSize;              /* Total number of available slots */` |
|        - |   23 | `	sxu32 eSize;              /* Size of a single slot */` |
|        - |   24 | `	sxu32 nCursor;            /* Loop cursor */` |
|        - |   25 | `	void *pUserData;          /* User private data associated with this container */` |
|        - |   26 | `};` |
|        - |   27 |  |
|        - |   28 | `/* SySet access macros */` |
|        - |   29 | `#define SySetBasePtr(S)           ((S)->pBase)` |
|        - |   30 | `#define SySetBasePtrJump(S,OFFT)  (&((char *)(S)->pBase)[OFFT*(S)->eSize])` |
|        - |   31 | `#define SySetUsed(S)              ((S)->nUsed)` |
|        - |   32 | `#define SySetSize(S)              ((S)->nSize)` |
|        - |   33 | `#define SySetElemSize(S)          ((S)->eSize)` |
|        - |   34 | `#define SySetCursor(S)            ((S)->nCursor)` |
|        - |   35 | `#define SySetGetAllocator(S)      ((S)->pAllocator)` |
|        - |   36 | `#define SySetSetUserData(S,DATA)  ((S)->pUserData = DATA)` |
|        - |   37 | `#define SySetGetUserData(S)       ((S)->pUserData)` |
|        - |   38 |  |
|        - |   39 | `/*` |
|        - |   40 | ` * A variable length container for generic data.` |
|        - |   41 | ` */` |
|        - |   42 | `struct SyBlob` |
|        - |   43 | `{` |
|        - |   44 | `	SyMemBackend *pAllocator; /* Memory backend */` |
|        - |   45 | `	void   *pBlob;            /* Base pointer */` |
|        - |   46 | `	sxu32  nByte;             /* Total number of used bytes */` |
|        - |   47 | `	sxu32  mByte;             /* Total number of available bytes */` |
|        - |   48 | `	sxu32  nFlags;            /* Blob internal flags, see below */` |
|        - |   49 | `};` |
|        - |   50 |  |
|        - |   51 | `/* Blob flags */` |
|        - |   52 | `#define SXBLOB_LOCKED  0x01  /* Blob is locked [i.e: Cannot auto grow] */` |
|        - |   53 | `#define SXBLOB_STATIC  0x02  /* Not allocated from heap */` |
|        - |   54 | `#define SXBLOB_RDONLY  0x04  /* Read-Only data */` |
|        - |   55 | `#define SXBLOB_POOLED  0x08  /* pBlob came from SyMemBackendPoolAlloc, not from the tracked` |
|        - |   56 | `                              * backend, so SyBlobRelease must give it back the same way.` |
|        - |   57 | `                              * The flag always describes the pointer CURRENTLY in pBlob:` |
|        - |   58 | `                              * every path that replaces the buffer sets it or clears it,` |
|        - |   59 | `                              * and none leaves it to mean something about a previous one.` |
|        - |   60 | `                              * (sxmem.c's BlobSetCapacity is the one place` |
|        - |   61 | `                              * that chooses.) */` |
|        - |   62 |  |
|        - |   63 | `/* SyBlob access macros */` |
|        - |   64 | `#define SyBlobFreeSpace(BLOB)    ((BLOB)->mByte - (BLOB)->nByte)` |
|        - |   65 | `#define SyBlobLength(BLOB)       ((BLOB)->nByte)` |
|        - |   66 | `#define SyBlobData(BLOB)         ((BLOB)->pBlob)` |
|        - |   67 | `#define SyBlobCurData(BLOB)      ((void*)(&((char*)(BLOB)->pBlob)[(BLOB)->nByte]))` |
|        - |   68 | `#define SyBlobDataAt(BLOB,OFFT)  ((void *)(&((char *)(BLOB)->pBlob)[OFFT]))` |
|        - |   69 | `#define SyBlobGetAllocator(BLOB) ((BLOB)->pAllocator)` |
|        - |   70 |  |
|        - |   71 | `/* SySet function prototypes */` |
|        - |   72 | `PH7_PRIVATE sxi32 SySetInit(SySet *pSet,SyMemBackend *pAllocator,sxu32 ElemSize);` |
|        - |   73 | `PH7_PRIVATE sxi32 SySetPut(SySet *pSet,const void *pItem);` |
|        - |   74 | `PH7_PRIVATE sxi32 SySetAlloc(SySet *pSet,sxi32 nItem);` |
|        - |   75 | `PH7_PRIVATE sxi32 SySetReset(SySet *pSet);` |
|        - |   76 | `PH7_PRIVATE sxi32 SySetResetCursor(SySet *pSet);` |
|        - |   77 | `PH7_PRIVATE sxi32 SySetGetNextEntry(SySet *pSet,void **ppEntry);` |
|        - |   78 | `#ifndef PH7_DISABLE_BUILTIN_FUNC` |
|        - |   79 | `PH7_PRIVATE void * SySetPeekCurrentEntry(SySet *pSet);` |
|        - |   80 | `#endif /* PH7_DISABLE_BUILTIN_FUNC */` |
|        - |   81 | `PH7_PRIVATE sxi32 SySetTruncate(SySet *pSet,sxu32 nNewSize);` |
|        - |   82 | `PH7_PRIVATE void SySetShrinkToFit(SySet *pSet);` |
|        - |   83 | `PH7_PRIVATE sxi32 SySetRelease(SySet *pSet);` |
|        - |   84 | `PH7_PRIVATE void * SySetPeek(SySet *pSet);` |
|        - |   85 | `PH7_PRIVATE void * SySetPop(SySet *pSet);` |
|        - |   86 | `/*` |
|        - |   87 | ` * The nIdx'th slot of a set, or NULL when the index is past the end.` |
|        - |   88 | ` *` |
|        - |   89 | ` * INLINE for a caller it no longer has: every array element the engine read went` |
|        - |   90 | ` * through it to reach the value table, and a bounds test plus a multiply was 1.5%` |
|        - |   91 | ` * of an ecosystem-gate phpcs run spent almost entirely on the call and return` |
|        - |   92 | ` * around them. That caller is PH7_MemObjAt now, and it inlines for the` |
|        - |   93 | ` * same reason; the remaining callers here are the bytecode and literal sets, which` |
|        - |   94 | ` * are read far less often but pay nothing for this.` |
|        - |   95 | ` */` |
| 99845243 |   96 | `SX_STATIC_INLINE void * SySetAt(SySet *pSet,sxu32 nIdx)` |
|        5 |   97 | `{` |
| 99845248 |   98 | `	if( nIdx >= pSet->nUsed ){` |
|       15 |   99 | `		return 0;   /* Out of range */` |
|        - |  100 | `	}` |
| 99845234 |  101 | `	return (void *)&((char *)pSet->pBase)[nIdx * pSet->eSize];` |
| 49923869 |  102 | `}` |
|        - |  103 |  |
|        - |  104 | `/* SyBlob function prototypes */` |
|        - |  105 | `PH7_PRIVATE sxi32 SyBlobInit(SyBlob *pBlob,SyMemBackend *pAllocator);` |
|        - |  106 | `PH7_PRIVATE sxi32 SyBlobInitFromBuf(SyBlob *pBlob,void *pBuffer,sxu32 nSize);` |
|        - |  107 | `PH7_PRIVATE sxi32 SyBlobReadOnly(SyBlob *pBlob,const void *pData,sxu32 nByte);` |
|        - |  108 | `PH7_PRIVATE sxi32 SyBlobAppend(SyBlob *pBlob,const void *pData,sxu32 nSize);` |
|        - |  109 | `PH7_PRIVATE sxi32 SyBlobNullAppend(SyBlob *pBlob);` |
|        - |  110 | `PH7_PRIVATE sxi32 SyBlobDup(SyBlob *pSrc,SyBlob *pDest);` |
|        - |  111 | `PH7_PRIVATE sxi32 SyBlobMakePrivate(SyBlob *pBlob);` |
|        - |  112 | `PH7_PRIVATE sxi32 SyBlobCmp(SyBlob *pLeft,SyBlob *pRight);` |
|        - |  113 | `PH7_PRIVATE sxi32 SyBlobReset(SyBlob *pBlob);` |
|        - |  114 | `PH7_PRIVATE sxi32 SyBlobRelease(SyBlob *pBlob);` |
|        - |  115 | `#if !defined(PH7_DISABLE_BUILTIN_FUNC) \|\| !defined(PH7_DISABLE_DISK_IO)` |
|        - |  116 | `PH7_PRIVATE sxi32 SyBlobSearch(const void *pBlob,sxu32 nLen,const void *pPattern,sxu32 pLen,sxu32 *pOfft);` |
|        - |  117 | `#endif /* PH7_DISABLE_BUILTIN_FUNC \|\| PH7_DISABLE_DISK_IO */` |
|        - |  118 |  |
|        - |  119 | `#endif /* __SXSET_H__ */` |
|        - |  120 |  |
