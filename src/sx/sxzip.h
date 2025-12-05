/**
 * SPDX-FileCopyrightText: 2011, 2012, 2013, 2014 Symisc Systems <licensing@symisc.net>
 * SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
 * SPDX-License-Identifier: BSD-3-Clause
 */
#ifndef __SXZIP_H__
#define __SXZIP_H__

#include "sxtypes.h"
#include "sxmem.h"

#ifndef PH7_DISABLE_BUILTIN_FUNC
/*
 * --------------
 * Archive extractor:
 * --------------
 * Each open ZIP/TAR archive is identified by an instance of the following structure.
 * That is, a process can open one or more archives and manipulates them in thread safe
 * way by simply working with pointers to the following structure.
 * Each entry in the archive is remembered in a hashtable.
 * Lookup is very fast and entry with the same name are chained together.
 */
typedef struct SyArchiveEntry SyArchiveEntry;
typedef struct SyArchive SyArchive;

struct SyArchive
{
	SyMemBackend    *pAllocator; /* Memory backend */
	SyArchiveEntry *pCursor;     /* Cursor for linear traversal of archive entries */
	SyArchiveEntry *pList;       /* Pointer to the List of the loaded archive */
	SyArchiveEntry **apHash;     /* Hashtable for archive entry */
	ProcRawStrCmp xCmp;          /* Hash comparison function */
	ProcHash xHash;              /* Hash Function */
	sxu32 nSize;                 /* Hashtable size */
	sxu32 nEntry;                /* Total number of entries in the zip/tar archive */
	sxu32 nLoaded;               /* Total number of entries loaded in memory */
	sxu32 nCentralOfft;          /* Central directory offset (ZIP only. Otherwise Zero) */
	sxu32 nCentralSize;          /* Central directory size (ZIP only. Otherwise Zero) */
	void *pUserData;             /* Upper layer private data */
	sxu32 nMagic;                /* Sanity check */
};

#define SXARCH_MAGIC                    0xDEAD635A
#define SXARCH_INVALID(ARCH)            (ARCH == 0  || ARCH->nMagic != SXARCH_MAGIC)
#define SXARCH_ENTRY_INVALID(ENTRY)     (ENTRY == 0 || ENTRY->nMagic != SXARCH_MAGIC)
#define SyArchiveHashFunc(ARCH)         (ARCH)->xHash
#define SyArchiveCmpFunc(ARCH)          (ARCH)->xCmp
#define SyArchiveUserData(ARCH)         (ARCH)->pUserData
#define SyArchiveSetUserData(ARCH,DATA) (ARCH)->pUserData = DATA

/*
 * Each loaded archive record is identified by an instance
 * of the following structure.
 */
struct SyArchiveEntry
{
	sxu32 nByte;         /* Contents size before compression */
	sxu32 nByteCompr;    /* Contents size after compression */
	sxu32 nReadCount;    /* Read counter */
	sxu32 nCrc;          /* Contents CRC32 */
	Sytm  sFmt;          /* Last-modification time */
	sxu32 nOfft;         /* Data offset */
	sxu16 nComprMeth;    /* Compression method 0 == stored/8 == deflated and so on (see appnote.txt) */
	sxu16 nExtra;        /* Extra size if any */
	SyString sFileName;  /* Entry name & length */
	sxu32 nDup;          /* Total number of entries with the same name */
	SyArchiveEntry *pNextHash,*pPrevHash; /* Hash collision chains */
	SyArchiveEntry *pNextName;    /* Next entry with the same name */
	SyArchiveEntry *pNext,*pPrev; /* Next and previous entry in the list */
	sxu32 nHash;         /* Hash of the entry name */
	void *pUserData;     /* User data */
	sxu32 nMagic;        /* Sanity check */
};

/*
 * Extra flags for extending the file local header
 */
#define SXZIP_EXTRA_TIMESTAMP   0x001   /* Extended UNIX timestamp */

/* Archive function prototypes */
PH7_PRIVATE sxi32 SyArchiveInit(SyArchive *pArch,SyMemBackend *pAllocator,ProcHash xHash,ProcRawStrCmp xCmp);
PH7_PRIVATE sxi32 SyArchiveRelease(SyArchive *pArch);
PH7_PRIVATE sxi32 SyArchiveResetLoopCursor(SyArchive *pArch);
PH7_PRIVATE sxi32 SyArchiveGetNextEntry(SyArchive *pArch,SyArchiveEntry **ppEntry);
PH7_PRIVATE sxi32 SyZipExtractFromBuf(SyArchive *pArch,const char *zBuf,sxu32 nLen);

#endif /* PH7_DISABLE_BUILTIN_FUNC */

#endif /* __SXZIP_H__ */
