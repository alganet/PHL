/**
 * SPDX-FileCopyrightText: 2011, 2012, 2013, 2014 Symisc Systems <licensing@symisc.net>
 * SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
 * SPDX-License-Identifier: BSD-3-Clause
 */
#include "sxtypes.h"
#include "sxmacros.h"
#include "sxset.h"
#include "sxmem.h"
#include "sxhash.h"
#include "sxzip.h"
#include "sxstr.h"

#ifndef PH7_DISABLE_BUILTIN_FUNC
/*
 * Zip File Format:
 *
 * Byte order: Little-endian
 *
 * [Local file header + Compressed data [+ Extended local header]?]*
 * [Central directory]*
 * [End of central directory record]
 *
 * Local file header:*
 * Offset   Length   Contents
 *  0      4 bytes  Local file header signature (0x04034b50)
 *  4      2 bytes  Version needed to extract
 *  6      2 bytes  General purpose bit flag
 *  8      2 bytes  Compression method
 * 10      2 bytes  Last mod file time
 * 12      2 bytes  Last mod file date
 * 14      4 bytes  CRC-32
 * 18      4 bytes  Compressed size (n)
 * 22      4 bytes  Uncompressed size
 * 26      2 bytes  Filename length (f)
 * 28      2 bytes  Extra field length (e)
 * 30     (f)bytes  Filename
 *        (e)bytes  Extra field
 *        (n)bytes  Compressed data
 *
 * Extended local header:*
 * Offset   Length   Contents
 *  0      4 bytes  Extended Local file header signature (0x08074b50)
 *  4      4 bytes  CRC-32
 *  8      4 bytes  Compressed size
 * 12      4 bytes  Uncompressed size
 *
 * Extra field:?(if any)
 * Offset 	Length		Contents
 * 0	  	2 bytes		Header ID (0x001 until 0xfb4a) see extended appnote from Info-zip
 * 2	  	2 bytes		Data size (g)
 * 		  	(g) bytes	(g) bytes of extra field
 *
 * Central directory:*
 * Offset   Length   Contents
 *  0      4 bytes  Central file header signature (0x02014b50)
 *  4      2 bytes  Version made by
 *  6      2 bytes  Version needed to extract
 *  8      2 bytes  General purpose bit flag
 * 10      2 bytes  Compression method
 * 12      2 bytes  Last mod file time
 * 14      2 bytes  Last mod file date
 * 16      4 bytes  CRC-32
 * 20      4 bytes  Compressed size
 * 24      4 bytes  Uncompressed size
 * 28      2 bytes  Filename length (f)
 * 30      2 bytes  Extra field length (e)
 * 32      2 bytes  File comment length (c)
 * 34      2 bytes  Disk number start
 * 36      2 bytes  Internal file attributes
 * 38      4 bytes  External file attributes
 * 42      4 bytes  Relative offset of local header
 * 46     (f)bytes  Filename
 *        (e)bytes  Extra field
 *        (c)bytes  File comment
 *
 * End of central directory record:
 * Offset   Length   Contents
 *  0      4 bytes  End of central dir signature (0x06054b50)
 *  4      2 bytes  Number of this disk
 *  6      2 bytes  Number of the disk with the start of the central directory
 *  8      2 bytes  Total number of entries in the central dir on this disk
 * 10      2 bytes  Total number of entries in the central dir
 * 12      4 bytes  Size of the central directory
 * 16      4 bytes  Offset of start of central directory with respect to the starting disk number
 * 20      2 bytes  zipfile comment length (c)
 * 22     (c)bytes  zipfile comment
 *
 * compression method: (2 bytes)
 *          0 - The file is stored (no compression)
 *          1 - The file is Shrunk
 *          2 - The file is Reduced with compression factor 1
 *          3 - The file is Reduced with compression factor 2
 *          4 - The file is Reduced with compression factor 3
 *          5 - The file is Reduced with compression factor 4
 *          6 - The file is Imploded
 *          7 - Reserved for Tokenizing compression algorithm
 *          8 - The file is Deflated
 */

#define SXMAKE_ZIP_WORKBUF	(SXU16_HIGH/2)	/* 32KB Initial working buffer size */
#define SXMAKE_ZIP_EXTRACT_VER	0x000a	/* Version needed to extract */
#define SXMAKE_ZIP_VER	0x003	/* Version made by */

#define SXZIP_CENTRAL_MAGIC			0x02014b50
#define SXZIP_END_CENTRAL_MAGIC		0x06054b50
#define SXZIP_LOCAL_MAGIC			0x04034b50
/*#define SXZIP_CRC32_START			0xdebb20e3*/

#define SXZIP_LOCAL_HDRSZ		30	/* Local header size */
#define SXZIP_LOCAL_EXT_HDRZ	16	/* Extended local header(footer) size */
#define SXZIP_CENTRAL_HDRSZ		46	/* Central directory header size */
#define SXZIP_END_CENTRAL_HDRSZ	22	/* End of central directory header size */

#define SXARCHIVE_HASH_SIZE	64 /* Starting hash table size(MUST BE POWER OF 2)*/
static sxi32 SyLittleEndianUnpack32(sxu32 *uNB,const unsigned char *buf,sxu32 Len)
{
	if( Len < sizeof(sxu32) ){
		return SXERR_SHORT;
	}
	*uNB =  buf[0] + (buf[1] << 8) + (buf[2] << 16) + (buf[3] << 24);
	return SXRET_OK;
}
static sxi32 SyLittleEndianUnpack16(sxu16 *pOut,const unsigned char *zBuf,sxu32 nLen)
{
	if( nLen < sizeof(sxu16) ){
		return SXERR_SHORT;
	}
	*pOut = zBuf[0] + (zBuf[1] <<8);

	return SXRET_OK;
}
static sxi32 SyDosTimeFormat(sxu32 nDosDate,Sytm *pOut)
{
	sxu16 nDate;
	sxu16 nTime;
	nDate = nDosDate >> 16;
	nTime = nDosDate & 0xFFFF;
	pOut->tm_isdst  = 0;
	pOut->tm_year 	= 1980 + (nDate >> 9);
	pOut->tm_mon	= (nDate % (1<<9))>>5;
	pOut->tm_mday	= (nDate % (1<<9))&0x1F;
	pOut->tm_hour	= nTime >> 11;
	pOut->tm_min	= (nTime % (1<<11)) >> 5;
	pOut->tm_sec	= ((nTime % (1<<11))& 0x1F )<<1;
	return SXRET_OK;
}
/*
 * Archive hashtable manager
 */
static sxi32 ArchiveHashGetEntry(SyArchive *pArch,const char *zName,sxu32 nLen,SyArchiveEntry **ppEntry)
{
	SyArchiveEntry *pBucketEntry;
	SyString sEntry;
	sxu32 nHash;

	nHash = pArch->xHash(zName,nLen);
	pBucketEntry = pArch->apHash[nHash & (pArch->nSize - 1)];

	SyStringInitFromBuf(&sEntry,zName,nLen);

	for(;;){
		if( pBucketEntry == 0 ){
			break;
		}
		if( nHash == pBucketEntry->nHash && pArch->xCmp(&sEntry,&pBucketEntry->sFileName) == 0 ){
			if( ppEntry ){
				*ppEntry = pBucketEntry;
			}
			return SXRET_OK;
		}
		pBucketEntry = pBucketEntry->pNextHash;
	}
	return SXERR_NOTFOUND;
}
static void ArchiveHashBucketInstall(SyArchiveEntry **apTable,sxu32 nBucket,SyArchiveEntry *pEntry)
{
	pEntry->pNextHash = apTable[nBucket];
	if( apTable[nBucket] != 0 ){
		apTable[nBucket]->pPrevHash = pEntry;
	}
	apTable[nBucket] = pEntry;
}
static sxi32 ArchiveHashGrowTable(SyArchive *pArch)
{
	sxu32 nNewSize = pArch->nSize * 2;
	SyArchiveEntry **apNew;
	SyArchiveEntry *pEntry;
	sxu32 n;

	/* Allocate a new table */
	apNew = (SyArchiveEntry **)SyMemBackendAlloc(pArch->pAllocator,nNewSize * sizeof(SyArchiveEntry *));
	if( apNew == 0 ){
		return SXRET_OK; /* Not so fatal,simply a performance hit */
	}
	SyZero(apNew,nNewSize * sizeof(SyArchiveEntry *));
	/* Rehash old entries */
	for( n = 0 , pEntry = pArch->pList ; n < pArch->nLoaded ; n++ , pEntry = pEntry->pNext ){
		pEntry->pNextHash = pEntry->pPrevHash = 0;
		ArchiveHashBucketInstall(apNew,pEntry->nHash & (nNewSize - 1),pEntry);
	}
	/* Release the old table */
	SyMemBackendFree(pArch->pAllocator,pArch->apHash);
	pArch->apHash = apNew;
	pArch->nSize = nNewSize;

	return SXRET_OK;
}
static sxi32 ArchiveHashInstallEntry(SyArchive *pArch,SyArchiveEntry *pEntry)
{
	if( pArch->nLoaded > pArch->nSize * 3 ){
		ArchiveHashGrowTable(&(*pArch));
	}
	pEntry->nHash = pArch->xHash(SyStringData(&pEntry->sFileName),SyStringLength(&pEntry->sFileName));
	/* Install the entry in its bucket */
	ArchiveHashBucketInstall(pArch->apHash,pEntry->nHash & (pArch->nSize - 1),pEntry);
	MACRO_LD_PUSH(pArch->pList,pEntry);
	pArch->nLoaded++;

	return SXRET_OK;
}
 /*
  * Parse the End of central directory and report status
  */
 static sxi32 ParseEndOfCentralDirectory(SyArchive *pArch,const unsigned char *zBuf)
 {
	sxu32 nMagic = 0; /* cc -O6 warning */
 	sxi32 rc;

	 /* Sanity check */
	 rc = SyLittleEndianUnpack32(&nMagic,zBuf,sizeof(sxu32));
	 if( rc != SXRET_OK ){
		 return SXERR_CORRUPT;
	 }
	 if( nMagic != SXZIP_END_CENTRAL_MAGIC ){
		 return SXERR_CORRUPT;
	 }
	 /* # of entries */
	 rc = SyLittleEndianUnpack16((sxu16 *)&pArch->nEntry,&zBuf[8],sizeof(sxu16));
	 if( rc != SXRET_OK ){
		 return SXERR_CORRUPT;
	 }
 	if( /* rc != SXRET_OK || */ pArch->nEntry > SXI16_HIGH /* SXU16_HIGH */ ){
 		return SXERR_CORRUPT;
 	}
 	/* Size of central directory */
 	rc = SyLittleEndianUnpack32(&pArch->nCentralSize,&zBuf[12],sizeof(sxu32));
 	if( /*rc != SXRET_OK ||*/ pArch->nCentralSize > SXI32_HIGH ){
 		return SXERR_CORRUPT;
 	}
 	/* Starting offset of central directory */
 	rc = SyLittleEndianUnpack32(&pArch->nCentralOfft,&zBuf[16],sizeof(sxu32));
 	if( /*rc != SXRET_OK ||*/ pArch->nCentralSize > SXI32_HIGH ){
 		return SXERR_CORRUPT;
 	}

 	return SXRET_OK;
 }
 /*
  * Fill the zip entry with the appropriate information from the central directory
  */
static sxi32 GetCentralDirectoryEntry(SyArchive *pArch,SyArchiveEntry *pEntry,const unsigned char *zCentral,sxu32 *pNextOffset)
 {
 	SyString *pName = &pEntry->sFileName; /* File name */
 	sxu16 nDosDate,nDosTime;
	sxu16 nComment = 0 ;
	sxu32 nMagic = 0; /* cc -O6 warning */
	sxi32 rc;
	nDosDate = nDosTime = 0; /* cc -O6 warning */
	SXUNUSED(pArch);
 	/* Sanity check */
 	rc = SyLittleEndianUnpack32(&nMagic,zCentral,sizeof(sxu32));
 	if( /* rc != SXRET_OK || */ nMagic != SXZIP_CENTRAL_MAGIC ){
 		rc = SXERR_CORRUPT;
 		/*
 		 * Try to recover by examining the next central directory record.
 		 * Dont worry here, there is no risk of an infinite loop since
		 * the buffer size is delimited.
 		 */

 		/* pName->nByte = 0; nComment = 0; pName->nExtra = 0 */
 		goto update;
 	}
 	/*
 	 * entry name length
 	 */
 	SyLittleEndianUnpack16((sxu16 *)&pName->nByte,&zCentral[28],sizeof(sxu16));
 	if( pName->nByte > SXI16_HIGH /* SXU16_HIGH */){
 		 rc = SXERR_BIG;
 		 goto update;
 	}
 	/* Extra information */
 	SyLittleEndianUnpack16(&pEntry->nExtra,&zCentral[30],sizeof(sxu16));
 	/* Comment length  */
 	SyLittleEndianUnpack16(&nComment,&zCentral[32],sizeof(sxu16));
 	/* Compression method 0 == stored / 8 == deflated */
 	rc = SyLittleEndianUnpack16(&pEntry->nComprMeth,&zCentral[10],sizeof(sxu16));
 	/* DOS Timestamp */
 	SyLittleEndianUnpack16(&nDosTime,&zCentral[12],sizeof(sxu16));
 	SyLittleEndianUnpack16(&nDosDate,&zCentral[14],sizeof(sxu16));
 	SyDosTimeFormat((nDosDate << 16 | nDosTime),&pEntry->sFmt);
	/* Little hack to fix month index  */
	pEntry->sFmt.tm_mon--;
 	/* CRC32 */
 	rc = SyLittleEndianUnpack32(&pEntry->nCrc,&zCentral[16],sizeof(sxu32));
 	/* Content size before compression */
 	rc = SyLittleEndianUnpack32(&pEntry->nByte,&zCentral[24],sizeof(sxu32));
 	if(  pEntry->nByte > SXI32_HIGH ){
 		rc = SXERR_BIG;
 		goto update;
 	}
 	/*
 	 * Content size after compression.
 	 * Note that if the file is stored pEntry->nByte should be equal to pEntry->nByteCompr
 	 */
 	rc = SyLittleEndianUnpack32(&pEntry->nByteCompr,&zCentral[20],sizeof(sxu32));
 	if( pEntry->nByteCompr > SXI32_HIGH ){
 		rc = SXERR_BIG;
 		goto update;
 	}
 	/* Finally grab the contents offset */
 	SyLittleEndianUnpack32(&pEntry->nOfft,&zCentral[42],sizeof(sxu32));
 	if( pEntry->nOfft > SXI32_HIGH ){
 		rc = SXERR_BIG;
 		goto update;
 	}
  	 rc = SXRET_OK;
update:
 	/* Update the offset to point to the next central directory record */
 	*pNextOffset =  SXZIP_CENTRAL_HDRSZ + pName->nByte + pEntry->nExtra + nComment;
 	return rc; /* Report failure or success */
}
static sxi32 ZipFixOffset(SyArchiveEntry *pEntry,void *pSrc)
{
	sxu16 nExtra,nNameLen;
	unsigned char *zHdr;
	nExtra = nNameLen = 0;
	zHdr = (unsigned char *)pSrc;
	zHdr = &zHdr[pEntry->nOfft];
	if( SyMemcmp(zHdr,"PK\003\004",sizeof(sxu32)) != 0 ){
		return SXERR_CORRUPT;
	}
	SyLittleEndianUnpack16(&nNameLen,&zHdr[26],sizeof(sxu16));
	SyLittleEndianUnpack16(&nExtra,&zHdr[28],sizeof(sxu16));
	/* Fix contents offset */
	pEntry->nOfft += SXZIP_LOCAL_HDRSZ + nExtra + nNameLen;
	return SXRET_OK;
}
/*
 * Extract all valid entries from the central directory
 */
static sxi32 ZipExtract(SyArchive *pArch,const unsigned char *zCentral,sxu32 nLen,void *pSrc)
{
	SyArchiveEntry *pEntry,*pDup;
	const unsigned char *zEnd ; /* End of central directory */
	sxu32 nIncr,nOfft;          /* Central Offset */
	SyString *pName;	        /* Entry name */
	char *zName;
	sxi32 rc;

	nOfft = nIncr = 0;
	zEnd = &zCentral[nLen];

	for(;;){
		if( &zCentral[nOfft] >= zEnd ){
			break;
		}
		/* Add a new entry */
		pEntry = (SyArchiveEntry *)SyMemBackendPoolAlloc(pArch->pAllocator,sizeof(SyArchiveEntry));
		if( pEntry == 0 ){
			break;
		}
		SyZero(pEntry,sizeof(SyArchiveEntry));
		pEntry->nMagic = SXARCH_MAGIC;
		nIncr = 0;
		rc = GetCentralDirectoryEntry(&(*pArch),pEntry,&zCentral[nOfft],&nIncr);
		if( rc == SXRET_OK ){
			/* Fix the starting record offset so we can access entry contents correctly */
			rc = ZipFixOffset(pEntry,pSrc);
		}
		if(rc != SXRET_OK ){
			sxu32 nJmp = 0;
			SyMemBackendPoolFree(pArch->pAllocator,pEntry);
			/* Try to recover by brute-forcing for a valid central directory record */
			if( SXRET_OK == SyBlobSearch((const void *)&zCentral[nOfft + nIncr],(sxu32)(zEnd - &zCentral[nOfft + nIncr]),
				(const void *)"PK\001\002",sizeof(sxu32),&nJmp)){
					nOfft += nIncr + nJmp; /* Check next entry */
					continue;
			}
			break; /* Giving up,archive is hopelessly corrupted */
		}
		pName = &pEntry->sFileName;
		pName->zString = (const char *)&zCentral[nOfft + SXZIP_CENTRAL_HDRSZ];
		if( pName->nByte <= 0 || ( pEntry->nByte <= 0 && pName->zString[pName->nByte - 1] != '/') ){
			/* Ignore zero length records (except folders) and records without names */
			SyMemBackendPoolFree(pArch->pAllocator,pEntry);
		 	nOfft += nIncr; /* Check next entry */
			continue;
		}
		zName = SyMemBackendStrDup(pArch->pAllocator,pName->zString,pName->nByte);
 	 	if( zName == 0 ){
 	 		 SyMemBackendPoolFree(pArch->pAllocator,pEntry);
		 	 nOfft += nIncr; /* Check next entry */
			continue;
 	 	}
		pName->zString = (const char *)zName;
		/* Check for duplicates */
		rc = ArchiveHashGetEntry(&(*pArch),pName->zString,pName->nByte,&pDup);
		if( rc == SXRET_OK ){
			/* Another entry with the same name exists ; link them together */
			pEntry->pNextName = pDup->pNextName;
			pDup->pNextName = pEntry;
			pDup->nDup++;
		}else{
			/* Insert in hashtable */
			ArchiveHashInstallEntry(pArch,pEntry);
		}
		nOfft += nIncr;	/* Check next record */
	}
	pArch->pCursor = pArch->pList;

	return pArch->nLoaded > 0 ? SXRET_OK : SXERR_EMPTY;
}
PH7_PRIVATE sxi32 SyZipExtractFromBuf(SyArchive *pArch,const char *zBuf,sxu32 nLen)
 {
 	const unsigned char *zCentral,*zEnd;
 	sxi32 rc;
#if defined(UNTRUST)
 	if( SXARCH_INVALID(pArch) || zBuf == 0 ){
 		return SXERR_INVALID;
 	}
#endif
 	/* The miminal size of a zip archive:
 	 * LOCAL_HDR_SZ + CENTRAL_HDR_SZ + END_OF_CENTRAL_HDR_SZ
 	 * 		30				46				22
 	 */
 	 if( nLen < SXZIP_LOCAL_HDRSZ + SXZIP_CENTRAL_HDRSZ + SXZIP_END_CENTRAL_HDRSZ ){
 	 	return SXERR_CORRUPT; /* Don't bother processing return immediately */
 	 }

 	zEnd = (unsigned char *)&zBuf[nLen - SXZIP_END_CENTRAL_HDRSZ];
 	/* Find the end of central directory */
 	while( ((sxu32)((unsigned char *)&zBuf[nLen] - zEnd) < (SXZIP_END_CENTRAL_HDRSZ + SXI16_HIGH)) &&
		zEnd > (unsigned char *)zBuf && SyMemcmp(zEnd,"PK\005\006",sizeof(sxu32)) != 0 ){
 		zEnd--;
 	}
 	/* Parse the end of central directory */
 	rc = ParseEndOfCentralDirectory(&(*pArch),zEnd);
 	if( rc != SXRET_OK ){
 		return rc;
 	}

 	/* Find the starting offset of the central directory */
 	zCentral = &zEnd[-(sxi32)pArch->nCentralSize];
 	if( zCentral <= (unsigned char *)zBuf || SyMemcmp(zCentral,"PK\001\002",sizeof(sxu32)) != 0 ){
 		if( pArch->nCentralOfft >= nLen ){
			/* Corrupted central directory offset */
 			return SXERR_CORRUPT;
 		}
 		zCentral = (unsigned char *)&zBuf[pArch->nCentralOfft];
 		if( SyMemcmp(zCentral,"PK\001\002",sizeof(sxu32)) != 0 ){
 			/* Corrupted zip archive */
 			return SXERR_CORRUPT;
 		}
 		/* Fall thru and extract all valid entries from the central directory */
 	}
 	rc = ZipExtract(&(*pArch),zCentral,(sxu32)(zEnd - zCentral),(void *)zBuf);
 	return rc;
 }
/*
  * Default comparison function.
  */
 static sxi32 ArchiveHashCmp(const SyString *pStr1,const SyString *pStr2)
 {
	 sxi32 rc;
	 rc = SyStringCmp(pStr1,pStr2,SyMemcmp);
	 return rc;
 }
PH7_PRIVATE sxi32 SyArchiveInit(SyArchive *pArch,SyMemBackend *pAllocator,ProcHash xHash,ProcRawStrCmp xCmp)
 {
	SyArchiveEntry **apHash;
#if defined(UNTRUST)
 	if( pArch == 0 ){
 		return SXERR_EMPTY;
 	}
#endif
 	SyZero(pArch,sizeof(SyArchive));
 	/* Allocate a new hashtable */
	apHash = (SyArchiveEntry **)SyMemBackendAlloc(&(*pAllocator),SXARCHIVE_HASH_SIZE * sizeof(SyArchiveEntry *));
	if( apHash == 0){
		return SXERR_MEM;
	}
	SyZero(apHash,SXARCHIVE_HASH_SIZE * sizeof(SyArchiveEntry *));
	pArch->apHash = apHash;
	pArch->xHash  = xHash ? xHash : SyBinHash;
	pArch->xCmp   = xCmp ? xCmp : ArchiveHashCmp;
	pArch->nSize  = SXARCHIVE_HASH_SIZE;
 	pArch->pAllocator = &(*pAllocator);
 	pArch->nMagic = SXARCH_MAGIC;
 	return SXRET_OK;
 }
 static sxi32 ArchiveReleaseEntry(SyMemBackend *pAllocator,SyArchiveEntry *pEntry)
 {
 	SyArchiveEntry *pDup = pEntry->pNextName;
 	SyArchiveEntry *pNextDup;

 	/* Release duplicates first since there are not stored in the hashtable */
 	for(;;){
 		if( pEntry->nDup == 0 ){
 			break;
 		}
 		pNextDup = pDup->pNextName;
		pDup->nMagic = 0x2661;
 		SyMemBackendFree(pAllocator,(void *)SyStringData(&pDup->sFileName));
 		SyMemBackendPoolFree(pAllocator,pDup);
 		pDup = pNextDup;
 		pEntry->nDup--;
 	}
	pEntry->nMagic = 0x2661;
  	SyMemBackendFree(pAllocator,(void *)SyStringData(&pEntry->sFileName));
 	SyMemBackendPoolFree(pAllocator,pEntry);
 	return SXRET_OK;
 }
PH7_PRIVATE sxi32 SyArchiveRelease(SyArchive *pArch)
 {
	SyArchiveEntry *pEntry,*pNext;
 	pEntry = pArch->pList;
	for(;;){
		if( pArch->nLoaded < 1 ){
			break;
		}
		pNext = pEntry->pNext;
		MACRO_LD_REMOVE(pArch->pList,pEntry);
		ArchiveReleaseEntry(pArch->pAllocator,pEntry);
		pEntry = pNext;
		pArch->nLoaded--;
	}
	SyMemBackendFree(pArch->pAllocator,pArch->apHash);
	pArch->pCursor = 0;
	pArch->nMagic = 0x2626;
	return SXRET_OK;
 }
 PH7_PRIVATE sxi32 SyArchiveResetLoopCursor(SyArchive *pArch)
 {
	pArch->pCursor = pArch->pList;
	return SXRET_OK;
 }
 PH7_PRIVATE sxi32 SyArchiveGetNextEntry(SyArchive *pArch,SyArchiveEntry **ppEntry)
 {
	SyArchiveEntry *pNext;
	if( pArch->pCursor == 0 ){
		/* Rewind the cursor */
		pArch->pCursor = pArch->pList;
		return SXERR_EOF;
	}
	*ppEntry = pArch->pCursor;
	 pNext = pArch->pCursor->pNext;
	 /* Advance the cursor to the next entry */
	 pArch->pCursor = pNext;
	 return SXRET_OK;
  }
#endif /* PH7_DISABLE_BUILTIN_FUNC */
