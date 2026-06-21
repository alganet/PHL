# src/sx/sxzip.c

<style>code, pre { background: none !important; white-space: pre !important; width: 100% !important; display: inline-block !important; } td { border: none !important; margin-top: 0 !important; margin-bottom: 0 !important; padding-top: 0 !important; padding-bottom: 0 !important; }</style>

Coverage: 219/293 lines (74.74%)

[Root index](../../index.md) | [Directory index](index.md)

| Hits | Line | Source |
| ---: | ---: | :--- |
|    - |    1 | `/**` |
|    - |    2 | ` * SPDX-FileCopyrightText: 2011, 2012, 2013, 2014 Symisc Systems <licensing@symisc.net>` |
|    - |    3 | ` * SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>` |
|    - |    4 | ` * SPDX-License-Identifier: BSD-3-Clause` |
|    - |    5 | ` */` |
|    - |    6 | `#include "sxtypes.h"` |
|    - |    7 | `#include "sxmacros.h"` |
|    - |    8 | `#include "sxset.h"` |
|    - |    9 | `#include "sxmem.h"` |
|    - |   10 | `#include "sxhash.h"` |
|    - |   11 | `#include "sxzip.h"` |
|    - |   12 | `#include "sxstr.h"` |
|    - |   13 |  |
|    - |   14 | `#ifndef PH7_DISABLE_BUILTIN_FUNC` |
|    - |   15 | `/*` |
|    - |   16 | ` * Zip File Format:` |
|    - |   17 | ` *` |
|    - |   18 | ` * Byte order: Little-endian` |
|    - |   19 | ` *` |
|    - |   20 | ` * [Local file header + Compressed data [+ Extended local header]?]*` |
|    - |   21 | ` * [Central directory]*` |
|    - |   22 | ` * [End of central directory record]` |
|    - |   23 | ` *` |
|    - |   24 | ` * Local file header:*` |
|    - |   25 | ` * Offset   Length   Contents` |
|    - |   26 | ` *  0      4 bytes  Local file header signature (0x04034b50)` |
|    - |   27 | ` *  4      2 bytes  Version needed to extract` |
|    - |   28 | ` *  6      2 bytes  General purpose bit flag` |
|    - |   29 | ` *  8      2 bytes  Compression method` |
|    - |   30 | ` * 10      2 bytes  Last mod file time` |
|    - |   31 | ` * 12      2 bytes  Last mod file date` |
|    - |   32 | ` * 14      4 bytes  CRC-32` |
|    - |   33 | ` * 18      4 bytes  Compressed size (n)` |
|    - |   34 | ` * 22      4 bytes  Uncompressed size` |
|    - |   35 | ` * 26      2 bytes  Filename length (f)` |
|    - |   36 | ` * 28      2 bytes  Extra field length (e)` |
|    - |   37 | ` * 30     (f)bytes  Filename` |
|    - |   38 | ` *        (e)bytes  Extra field` |
|    - |   39 | ` *        (n)bytes  Compressed data` |
|    - |   40 | ` *` |
|    - |   41 | ` * Extended local header:*` |
|    - |   42 | ` * Offset   Length   Contents` |
|    - |   43 | ` *  0      4 bytes  Extended Local file header signature (0x08074b50)` |
|    - |   44 | ` *  4      4 bytes  CRC-32` |
|    - |   45 | ` *  8      4 bytes  Compressed size` |
|    - |   46 | ` * 12      4 bytes  Uncompressed size` |
|    - |   47 | ` *` |
|    - |   48 | ` * Extra field:?(if any)` |
|    - |   49 | ` * Offset 	Length		Contents` |
|    - |   50 | ` * 0	  	2 bytes		Header ID (0x001 until 0xfb4a) see extended appnote from Info-zip` |
|    - |   51 | ` * 2	  	2 bytes		Data size (g)` |
|    - |   52 | ` * 		  	(g) bytes	(g) bytes of extra field` |
|    - |   53 | ` *` |
|    - |   54 | ` * Central directory:*` |
|    - |   55 | ` * Offset   Length   Contents` |
|    - |   56 | ` *  0      4 bytes  Central file header signature (0x02014b50)` |
|    - |   57 | ` *  4      2 bytes  Version made by` |
|    - |   58 | ` *  6      2 bytes  Version needed to extract` |
|    - |   59 | ` *  8      2 bytes  General purpose bit flag` |
|    - |   60 | ` * 10      2 bytes  Compression method` |
|    - |   61 | ` * 12      2 bytes  Last mod file time` |
|    - |   62 | ` * 14      2 bytes  Last mod file date` |
|    - |   63 | ` * 16      4 bytes  CRC-32` |
|    - |   64 | ` * 20      4 bytes  Compressed size` |
|    - |   65 | ` * 24      4 bytes  Uncompressed size` |
|    - |   66 | ` * 28      2 bytes  Filename length (f)` |
|    - |   67 | ` * 30      2 bytes  Extra field length (e)` |
|    - |   68 | ` * 32      2 bytes  File comment length (c)` |
|    - |   69 | ` * 34      2 bytes  Disk number start` |
|    - |   70 | ` * 36      2 bytes  Internal file attributes` |
|    - |   71 | ` * 38      4 bytes  External file attributes` |
|    - |   72 | ` * 42      4 bytes  Relative offset of local header` |
|    - |   73 | ` * 46     (f)bytes  Filename` |
|    - |   74 | ` *        (e)bytes  Extra field` |
|    - |   75 | ` *        (c)bytes  File comment` |
|    - |   76 | ` *` |
|    - |   77 | ` * End of central directory record:` |
|    - |   78 | ` * Offset   Length   Contents` |
|    - |   79 | ` *  0      4 bytes  End of central dir signature (0x06054b50)` |
|    - |   80 | ` *  4      2 bytes  Number of this disk` |
|    - |   81 | ` *  6      2 bytes  Number of the disk with the start of the central directory` |
|    - |   82 | ` *  8      2 bytes  Total number of entries in the central dir on this disk` |
|    - |   83 | ` * 10      2 bytes  Total number of entries in the central dir` |
|    - |   84 | ` * 12      4 bytes  Size of the central directory` |
|    - |   85 | ` * 16      4 bytes  Offset of start of central directory with respect to the starting disk number` |
|    - |   86 | ` * 20      2 bytes  zipfile comment length (c)` |
|    - |   87 | ` * 22     (c)bytes  zipfile comment` |
|    - |   88 | ` *` |
|    - |   89 | ` * compression method: (2 bytes)` |
|    - |   90 | ` *          0 - The file is stored (no compression)` |
|    - |   91 | ` *          1 - The file is Shrunk` |
|    - |   92 | ` *          2 - The file is Reduced with compression factor 1` |
|    - |   93 | ` *          3 - The file is Reduced with compression factor 2` |
|    - |   94 | ` *          4 - The file is Reduced with compression factor 3` |
|    - |   95 | ` *          5 - The file is Reduced with compression factor 4` |
|    - |   96 | ` *          6 - The file is Imploded` |
|    - |   97 | ` *          7 - Reserved for Tokenizing compression algorithm` |
|    - |   98 | ` *          8 - The file is Deflated` |
|    - |   99 | ` */` |
|    - |  100 |  |
|    - |  101 | `#define SXMAKE_ZIP_WORKBUF	(SXU16_HIGH/2)	/* 32KB Initial working buffer size */` |
|    - |  102 | `#define SXMAKE_ZIP_EXTRACT_VER	0x000a	/* Version needed to extract */` |
|    - |  103 | `#define SXMAKE_ZIP_VER	0x003	/* Version made by */` |
|    - |  104 |  |
|    - |  105 | `#define SXZIP_CENTRAL_MAGIC			0x02014b50` |
|    - |  106 | `#define SXZIP_END_CENTRAL_MAGIC		0x06054b50` |
|    - |  107 | `#define SXZIP_LOCAL_MAGIC			0x04034b50` |
|    - |  108 | `/*#define SXZIP_CRC32_START			0xdebb20e3*/` |
|    - |  109 |  |
|    - |  110 | `#define SXZIP_LOCAL_HDRSZ		30	/* Local header size */` |
|    - |  111 | `#define SXZIP_LOCAL_EXT_HDRZ	16	/* Extended local header(footer) size */` |
|    - |  112 | `#define SXZIP_CENTRAL_HDRSZ		46	/* Central directory header size */` |
|    - |  113 | `#define SXZIP_END_CENTRAL_HDRSZ	22	/* End of central directory header size */` |
|    - |  114 |  |
|    - |  115 | `#define SXARCHIVE_HASH_SIZE	64 /* Starting hash table size(MUST BE POWER OF 2)*/` |
|  154 |  116 | `static sxi32 SyLittleEndianUnpack32(sxu32 *uNB,const unsigned char *buf,sxu32 Len)` |
|    2 |  117 |  |
|  156 |  118 | `	if( Len < sizeof(sxu32) ){` |
|  ! 0 |  119 | `		return SXERR_SHORT;` |
|    - |  120 | `	}` |
|  156 |  121 | `	*uNB =  buf[0] + (buf[1] << 8) + (buf[2] << 16) + (buf[3] << 24);` |
|  156 |  122 | `	return SXRET_OK;` |
|   79 |  123 |  |
|  156 |  124 | `static sxi32 SyLittleEndianUnpack16(sxu16 *pOut,const unsigned char *zBuf,sxu32 nLen)` |
|    2 |  125 |  |
|  158 |  126 | `	if( nLen < sizeof(sxu16) ){` |
|  ! 0 |  127 | `		return SXERR_SHORT;` |
|    - |  128 | `	}` |
|  158 |  129 | `	*pOut = zBuf[0] + (zBuf[1] <<8);` |
|    - |  130 |  |
|  158 |  131 | `	return SXRET_OK;` |
|   80 |  132 |  |
|   18 |  133 | `static sxi32 SyDosTimeFormat(sxu32 nDosDate,Sytm *pOut)` |
|    2 |  134 |  |
|    - |  135 | `	sxu16 nDate;` |
|    - |  136 | `	sxu16 nTime;` |
|   20 |  137 | `	nDate = nDosDate >> 16;` |
|   20 |  138 | `	nTime = nDosDate & 0xFFFF;` |
|   20 |  139 | `	pOut->tm_isdst  = 0;` |
|   20 |  140 | `	pOut->tm_year 	= 1980 + (nDate >> 9);` |
|   20 |  141 | `	pOut->tm_mon	= (nDate % (1<<9))>>5;` |
|   20 |  142 | `	pOut->tm_mday	= (nDate % (1<<9))&0x1F;` |
|   20 |  143 | `	pOut->tm_hour	= nTime >> 11;` |
|   20 |  144 | `	pOut->tm_min	= (nTime % (1<<11)) >> 5;` |
|   20 |  145 | `	pOut->tm_sec	= ((nTime % (1<<11))& 0x1F )<<1;` |
|   20 |  146 | `	return SXRET_OK;` |
|    2 |  147 |  |
|    - |  148 | `/*` |
|    - |  149 | ` * Archive hashtable manager` |
|    - |  150 | ` */` |
|   14 |  151 | `static sxi32 ArchiveHashGetEntry(SyArchive *pArch,const char *zName,sxu32 nLen,SyArchiveEntry **ppEntry)` |
|    2 |  152 |  |
|    - |  153 | `	SyArchiveEntry *pBucketEntry;` |
|    - |  154 | `	SyString sEntry;` |
|    - |  155 | `	sxu32 nHash;` |
|    - |  156 |  |
|   16 |  157 | `	nHash = pArch->xHash(zName,nLen);` |
|   16 |  158 | `	pBucketEntry = pArch->apHash[nHash & (pArch->nSize - 1)];` |
|    - |  159 |  |
|   16 |  160 | `	SyStringInitFromBuf(&sEntry,zName,nLen);` |
|    - |  161 |  |
|    7 |  162 | `	for(;;){` |
|   16 |  163 | `		if( pBucketEntry == 0 ){` |
|   16 |  164 | `			break;` |
|    - |  165 | `		}` |
|  ! 0 |  166 | `		if( nHash == pBucketEntry->nHash && pArch->xCmp(&sEntry,&pBucketEntry->sFileName) == 0 ){` |
|  ! 0 |  167 | `			if( ppEntry ){` |
|  ! 0 |  168 | `				*ppEntry = pBucketEntry;` |
|  ! 0 |  169 | `			}` |
|  ! 0 |  170 | `			return SXRET_OK;` |
|    - |  171 | `		}` |
|  ! 0 |  172 | `		pBucketEntry = pBucketEntry->pNextHash;` |
|  ! 0 |  173 | `	}` |
|   16 |  174 | `	return SXERR_NOTFOUND;` |
|    9 |  175 |  |
|   14 |  176 | `static void ArchiveHashBucketInstall(SyArchiveEntry **apTable,sxu32 nBucket,SyArchiveEntry *pEntry)` |
|    2 |  177 |  |
|   16 |  178 | `	pEntry->pNextHash = apTable[nBucket];` |
|   16 |  179 | `	if( apTable[nBucket] != 0 ){` |
|  ! 0 |  180 | `		apTable[nBucket]->pPrevHash = pEntry;` |
|  ! 0 |  181 | `	}` |
|   16 |  182 | `	apTable[nBucket] = pEntry;` |
|   16 |  183 |  |
|  ! 0 |  184 | `static sxi32 ArchiveHashGrowTable(SyArchive *pArch)` |
|  ! 0 |  185 |  |
|  ! 0 |  186 | `	sxu32 nNewSize = pArch->nSize * 2;` |
|    - |  187 | `	SyArchiveEntry **apNew;` |
|    - |  188 | `	SyArchiveEntry *pEntry;` |
|    - |  189 | `	sxu32 n;` |
|    - |  190 |  |
|    - |  191 | `	/* Allocate a new table */` |
|  ! 0 |  192 | `	apNew = (SyArchiveEntry **)SyMemBackendAlloc(pArch->pAllocator,nNewSize * sizeof(SyArchiveEntry *));` |
|  ! 0 |  193 | `	if( apNew == 0 ){` |
|  ! 0 |  194 | `		return SXRET_OK; /* Not so fatal,simply a performance hit */` |
|    - |  195 | `	}` |
|  ! 0 |  196 | `	SyZero(apNew,nNewSize * sizeof(SyArchiveEntry *));` |
|    - |  197 | `	/* Rehash old entries */` |
|  ! 0 |  198 | `	for( n = 0 , pEntry = pArch->pList ; n < pArch->nLoaded ; n++ , pEntry = pEntry->pNext ){` |
|  ! 0 |  199 | `		pEntry->pNextHash = pEntry->pPrevHash = 0;` |
|  ! 0 |  200 | `		ArchiveHashBucketInstall(apNew,pEntry->nHash & (nNewSize - 1),pEntry);` |
|  ! 0 |  201 | `	}` |
|    - |  202 | `	/* Release the old table */` |
|  ! 0 |  203 | `	SyMemBackendFree(pArch->pAllocator,pArch->apHash);` |
|  ! 0 |  204 | `	pArch->apHash = apNew;` |
|  ! 0 |  205 | `	pArch->nSize = nNewSize;` |
|    - |  206 |  |
|  ! 0 |  207 | `	return SXRET_OK;` |
|  ! 0 |  208 |  |
|   14 |  209 | `static sxi32 ArchiveHashInstallEntry(SyArchive *pArch,SyArchiveEntry *pEntry)` |
|    2 |  210 |  |
|   16 |  211 | `	if( pArch->nLoaded > pArch->nSize * 3 ){` |
|  ! 0 |  212 | `		ArchiveHashGrowTable(&(*pArch));` |
|  ! 0 |  213 | `	}` |
|   16 |  214 | `	pEntry->nHash = pArch->xHash(SyStringData(&pEntry->sFileName),SyStringLength(&pEntry->sFileName));` |
|    - |  215 | `	/* Install the entry in its bucket */` |
|   16 |  216 | `	ArchiveHashBucketInstall(pArch->apHash,pEntry->nHash & (pArch->nSize - 1),pEntry);` |
|   16 |  217 | `	MACRO_LD_PUSH(pArch->pList,pEntry);` |
|   16 |  218 | `	pArch->nLoaded++;` |
|    - |  219 |  |
|   16 |  220 | `	return SXRET_OK;` |
|    2 |  221 |  |
|    - |  222 | ` /*` |
|    - |  223 | `  * Parse the End of central directory and report status` |
|    - |  224 | `  */` |
|   24 |  225 | ` static sxi32 ParseEndOfCentralDirectory(SyArchive *pArch,const unsigned char *zBuf)` |
|    2 |  226 | ` {` |
|   26 |  227 | `	sxu32 nMagic = 0; /* cc -O6 warning */` |
|    - |  228 | ` 	sxi32 rc;` |
|    - |  229 |  |
|    - |  230 | `	 /* Sanity check */` |
|   26 |  231 | `	 rc = SyLittleEndianUnpack32(&nMagic,zBuf,sizeof(sxu32));` |
|   26 |  232 | `	 if( rc != SXRET_OK ){` |
|  ! 0 |  233 | `		 return SXERR_CORRUPT;` |
|    - |  234 | `	 }` |
|   26 |  235 | `	 if( nMagic != SXZIP_END_CENTRAL_MAGIC ){` |
|    5 |  236 | `		 return SXERR_CORRUPT;` |
|    - |  237 | `	 }` |
|    - |  238 | `	 /* # of entries */` |
|   22 |  239 | `	 rc = SyLittleEndianUnpack16((sxu16 *)&pArch->nEntry,&zBuf[8],sizeof(sxu16));` |
|   22 |  240 | `	 if( rc != SXRET_OK ){` |
|  ! 0 |  241 | `		 return SXERR_CORRUPT;` |
|    - |  242 | `	 }` |
|   22 |  243 | ` 	if( /* rc != SXRET_OK \|\| */ pArch->nEntry > SXI16_HIGH /* SXU16_HIGH */ ){` |
|  ! 0 |  244 | ` 		return SXERR_CORRUPT;` |
|    - |  245 | ` 	}` |
|    - |  246 | ` 	/* Size of central directory */` |
|   22 |  247 | ` 	rc = SyLittleEndianUnpack32(&pArch->nCentralSize,&zBuf[12],sizeof(sxu32));` |
|   22 |  248 | ` 	if( /*rc != SXRET_OK \|\|*/ pArch->nCentralSize > SXI32_HIGH ){` |
|  ! 0 |  249 | ` 		return SXERR_CORRUPT;` |
|    - |  250 | ` 	}` |
|    - |  251 | ` 	/* Starting offset of central directory */` |
|   22 |  252 | ` 	rc = SyLittleEndianUnpack32(&pArch->nCentralOfft,&zBuf[16],sizeof(sxu32));` |
|   22 |  253 | ` 	if( /*rc != SXRET_OK \|\|*/ pArch->nCentralSize > SXI32_HIGH ){` |
|  ! 0 |  254 | ` 		return SXERR_CORRUPT;` |
|    - |  255 | ` 	}` |
|    - |  256 |  |
|   22 |  257 | ` 	return SXRET_OK;` |
|   14 |  258 | ` }` |
|    - |  259 | ` /*` |
|    - |  260 | `  * Fill the zip entry with the appropriate information from the central directory` |
|    - |  261 | `  */` |
|   18 |  262 | `static sxi32 GetCentralDirectoryEntry(SyArchive *pArch,SyArchiveEntry *pEntry,const unsigned char *zCentral,sxu32 *pNextOffset)` |
|    2 |  263 | ` {` |
|   20 |  264 | ` 	SyString *pName = &pEntry->sFileName; /* File name */` |
|    - |  265 | ` 	sxu16 nDosDate,nDosTime;` |
|   20 |  266 | `	sxu16 nComment = 0 ;` |
|   20 |  267 | `	sxu32 nMagic = 0; /* cc -O6 warning */` |
|    - |  268 | `	sxi32 rc;` |
|   20 |  269 | `	nDosDate = nDosTime = 0; /* cc -O6 warning */` |
|    9 |  270 | `	SXUNUSED(pArch);` |
|    - |  271 | ` 	/* Sanity check */` |
|   20 |  272 | ` 	rc = SyLittleEndianUnpack32(&nMagic,zCentral,sizeof(sxu32));` |
|   20 |  273 | ` 	if( /* rc != SXRET_OK \|\| */ nMagic != SXZIP_CENTRAL_MAGIC ){` |
|  ! 0 |  274 | ` 		rc = SXERR_CORRUPT;` |
|    - |  275 | ` 		/*` |
|    - |  276 | ` 		 * Try to recover by examining the next central directory record.` |
|    - |  277 | ` 		 * Dont worry here, there is no risk of an infinite loop since` |
|    - |  278 | `		 * the buffer size is delimited.` |
|    - |  279 | ` 		 */` |
|    - |  280 |  |
|    - |  281 | ` 		/* pName->nByte = 0; nComment = 0; pName->nExtra = 0 */` |
|  ! 0 |  282 | ` 		goto update;` |
|    - |  283 | ` 	}` |
|    - |  284 | ` 	/*` |
|    - |  285 | ` 	 * entry name length` |
|    - |  286 | ` 	 */` |
|   20 |  287 | ` 	SyLittleEndianUnpack16((sxu16 *)&pName->nByte,&zCentral[28],sizeof(sxu16));` |
|   20 |  288 | ` 	if( pName->nByte > SXI16_HIGH /* SXU16_HIGH */){` |
|  ! 0 |  289 | ` 		 rc = SXERR_BIG;` |
|  ! 0 |  290 | ` 		 goto update;` |
|    - |  291 | ` 	}` |
|    - |  292 | ` 	/* Extra information */` |
|   20 |  293 | ` 	SyLittleEndianUnpack16(&pEntry->nExtra,&zCentral[30],sizeof(sxu16));` |
|    - |  294 | ` 	/* Comment length  */` |
|   20 |  295 | ` 	SyLittleEndianUnpack16(&nComment,&zCentral[32],sizeof(sxu16));` |
|    - |  296 | ` 	/* Compression method 0 == stored / 8 == deflated */` |
|   20 |  297 | ` 	rc = SyLittleEndianUnpack16(&pEntry->nComprMeth,&zCentral[10],sizeof(sxu16));` |
|    - |  298 | ` 	/* DOS Timestamp */` |
|   20 |  299 | ` 	SyLittleEndianUnpack16(&nDosTime,&zCentral[12],sizeof(sxu16));` |
|   20 |  300 | ` 	SyLittleEndianUnpack16(&nDosDate,&zCentral[14],sizeof(sxu16));` |
|   20 |  301 | ` 	SyDosTimeFormat((nDosDate << 16 \| nDosTime),&pEntry->sFmt);` |
|    - |  302 | `	/* Little hack to fix month index  */` |
|   20 |  303 | `	pEntry->sFmt.tm_mon--;` |
|    - |  304 | ` 	/* CRC32 */` |
|   20 |  305 | ` 	rc = SyLittleEndianUnpack32(&pEntry->nCrc,&zCentral[16],sizeof(sxu32));` |
|    - |  306 | ` 	/* Content size before compression */` |
|   20 |  307 | ` 	rc = SyLittleEndianUnpack32(&pEntry->nByte,&zCentral[24],sizeof(sxu32));` |
|   20 |  308 | ` 	if(  pEntry->nByte > SXI32_HIGH ){` |
|  ! 0 |  309 | ` 		rc = SXERR_BIG;` |
|  ! 0 |  310 | ` 		goto update;` |
|    - |  311 | ` 	}` |
|    - |  312 | ` 	/*` |
|    - |  313 | ` 	 * Content size after compression.` |
|    - |  314 | ` 	 * Note that if the file is stored pEntry->nByte should be equal to pEntry->nByteCompr` |
|    - |  315 | ` 	 */` |
|   20 |  316 | ` 	rc = SyLittleEndianUnpack32(&pEntry->nByteCompr,&zCentral[20],sizeof(sxu32));` |
|   20 |  317 | ` 	if( pEntry->nByteCompr > SXI32_HIGH ){` |
|  ! 0 |  318 | ` 		rc = SXERR_BIG;` |
|  ! 0 |  319 | ` 		goto update;` |
|    - |  320 | ` 	}` |
|    - |  321 | ` 	/* Finally grab the contents offset */` |
|   20 |  322 | ` 	SyLittleEndianUnpack32(&pEntry->nOfft,&zCentral[42],sizeof(sxu32));` |
|   20 |  323 | ` 	if( pEntry->nOfft > SXI32_HIGH ){` |
|  ! 0 |  324 | ` 		rc = SXERR_BIG;` |
|  ! 0 |  325 | ` 		goto update;` |
|    - |  326 | ` 	}` |
|   20 |  327 | `  	 rc = SXRET_OK;` |
|    9 |  328 | `update:` |
|    - |  329 | ` 	/* Update the offset to point to the next central directory record */` |
|   20 |  330 | ` 	*pNextOffset =  SXZIP_CENTRAL_HDRSZ + pName->nByte + pEntry->nExtra + nComment;` |
|   20 |  331 | ` 	return rc; /* Report failure or success */` |
|    2 |  332 |  |
|   18 |  333 | `static sxi32 ZipFixOffset(SyArchiveEntry *pEntry,void *pSrc)` |
|    2 |  334 |  |
|    - |  335 | `	sxu16 nExtra,nNameLen;` |
|    - |  336 | `	unsigned char *zHdr;` |
|   20 |  337 | `	nExtra = nNameLen = 0;` |
|   20 |  338 | `	zHdr = (unsigned char *)pSrc;` |
|   20 |  339 | `	zHdr = &zHdr[pEntry->nOfft];` |
|   20 |  340 | `	if( SyMemcmp(zHdr,"PK\003\004",sizeof(sxu32)) != 0 ){` |
|    5 |  341 | `		return SXERR_CORRUPT;` |
|    - |  342 | `	}` |
|   16 |  343 | `	SyLittleEndianUnpack16(&nNameLen,&zHdr[26],sizeof(sxu16));` |
|   16 |  344 | `	SyLittleEndianUnpack16(&nExtra,&zHdr[28],sizeof(sxu16));` |
|    - |  345 | `	/* Fix contents offset */` |
|   16 |  346 | `	pEntry->nOfft += SXZIP_LOCAL_HDRSZ + nExtra + nNameLen;` |
|   16 |  347 | `	return SXRET_OK;` |
|   11 |  348 |  |
|    - |  349 | `/*` |
|    - |  350 | ` * Extract all valid entries from the central directory` |
|    - |  351 | ` */` |
|   18 |  352 | `static sxi32 ZipExtract(SyArchive *pArch,const unsigned char *zCentral,sxu32 nLen,void *pSrc)` |
|    2 |  353 |  |
|    - |  354 | `	SyArchiveEntry *pEntry,*pDup;` |
|    - |  355 | `	const unsigned char *zEnd ; /* End of central directory */` |
|    - |  356 | `	sxu32 nIncr,nOfft;          /* Central Offset */` |
|    - |  357 | `	SyString *pName;	        /* Entry name */` |
|    - |  358 | `	char *zName;` |
|    - |  359 | `	sxi32 rc;` |
|    - |  360 |  |
|   20 |  361 | `	nOfft = nIncr = 0;` |
|   20 |  362 | `	zEnd = &zCentral[nLen];` |
|    - |  363 |  |
|   16 |  364 | `	for(;;){` |
|   34 |  365 | `		if( &zCentral[nOfft] >= zEnd ){` |
|   16 |  366 | `			break;` |
|    - |  367 | `		}` |
|    - |  368 | `		/* Add a new entry */` |
|   20 |  369 | `		pEntry = (SyArchiveEntry *)SyMemBackendPoolAlloc(pArch->pAllocator,sizeof(SyArchiveEntry));` |
|   20 |  370 | `		if( pEntry == 0 ){` |
|  ! 0 |  371 | `			break;` |
|    - |  372 | `		}` |
|   20 |  373 | `		SyZero(pEntry,sizeof(SyArchiveEntry));` |
|   20 |  374 | `		pEntry->nMagic = SXARCH_MAGIC;` |
|   20 |  375 | `		nIncr = 0;` |
|   20 |  376 | `		rc = GetCentralDirectoryEntry(&(*pArch),pEntry,&zCentral[nOfft],&nIncr);` |
|   20 |  377 | `		if( rc == SXRET_OK ){` |
|    - |  378 | `			/* Fix the starting record offset so we can access entry contents correctly */` |
|   20 |  379 | `			rc = ZipFixOffset(pEntry,pSrc);` |
|    9 |  380 | `		}` |
|   20 |  381 | `		if(rc != SXRET_OK ){` |
|    5 |  382 | `			sxu32 nJmp = 0;` |
|    5 |  383 | `			SyMemBackendPoolFree(pArch->pAllocator,pEntry);` |
|    - |  384 | `			/* Try to recover by brute-forcing for a valid central directory record */` |
|    5 |  385 | `			if( SXRET_OK == SyBlobSearch((const void *)&zCentral[nOfft + nIncr],(sxu32)(zEnd - &zCentral[nOfft + nIncr]),` |
|    - |  386 | `				(const void *)"PK\001\002",sizeof(sxu32),&nJmp)){` |
|  ! 0 |  387 | `					nOfft += nIncr + nJmp; /* Check next entry */` |
|  ! 0 |  388 | `					continue;` |
|    - |  389 | `			}` |
|    5 |  390 | `			break; /* Giving up,archive is hopelessly corrupted */` |
|    - |  391 | `		}` |
|   16 |  392 | `		pName = &pEntry->sFileName;` |
|   16 |  393 | `		pName->zString = (const char *)&zCentral[nOfft + SXZIP_CENTRAL_HDRSZ];` |
|   16 |  394 | `		if( pName->nByte <= 0 \|\| ( pEntry->nByte <= 0 && pName->zString[pName->nByte - 1] != '/') ){` |
|    - |  395 | `			/* Ignore zero length records (except folders) and records without names */` |
|  ! 0 |  396 | `			SyMemBackendPoolFree(pArch->pAllocator,pEntry);` |
|  ! 0 |  397 | `		 	nOfft += nIncr; /* Check next entry */` |
|  ! 0 |  398 | `			continue;` |
|    - |  399 | `		}` |
|   16 |  400 | `		zName = SyMemBackendStrDup(pArch->pAllocator,pName->zString,pName->nByte);` |
|   16 |  401 | ` 	 	if( zName == 0 ){` |
|  ! 0 |  402 | ` 	 		 SyMemBackendPoolFree(pArch->pAllocator,pEntry);` |
|  ! 0 |  403 | `		 	 nOfft += nIncr; /* Check next entry */` |
|  ! 0 |  404 | `			continue;` |
|    - |  405 | ` 	 	}` |
|   16 |  406 | `		pName->zString = (const char *)zName;` |
|    - |  407 | `		/* Check for duplicates */` |
|   16 |  408 | `		rc = ArchiveHashGetEntry(&(*pArch),pName->zString,pName->nByte,&pDup);` |
|   16 |  409 | `		if( rc == SXRET_OK ){` |
|    - |  410 | `			/* Another entry with the same name exists ; link them together */` |
|  ! 0 |  411 | `			pEntry->pNextName = pDup->pNextName;` |
|  ! 0 |  412 | `			pDup->pNextName = pEntry;` |
|  ! 0 |  413 | `			pDup->nDup++;` |
|  ! 0 |  414 | `		}else{` |
|    - |  415 | `			/* Insert in hashtable */` |
|   16 |  416 | `			ArchiveHashInstallEntry(pArch,pEntry);` |
|    - |  417 | `		}` |
|   16 |  418 | `		nOfft += nIncr;	/* Check next record */` |
|    2 |  419 | `	}` |
|   20 |  420 | `	pArch->pCursor = pArch->pList;` |
|    - |  421 |  |
|   20 |  422 | `	return pArch->nLoaded > 0 ? SXRET_OK : SXERR_EMPTY;` |
|    2 |  423 |  |
|   28 |  424 | `PH7_PRIVATE sxi32 SyZipExtractFromBuf(SyArchive *pArch,const char *zBuf,sxu32 nLen)` |
|    2 |  425 | ` {` |
|    - |  426 | ` 	const unsigned char *zCentral,*zEnd;` |
|    - |  427 | ` 	sxi32 rc;` |
|    - |  428 | `#if defined(UNTRUST)` |
|    - |  429 | ` 	if( SXARCH_INVALID(pArch) \|\| zBuf == 0 ){` |
|    - |  430 | ` 		return SXERR_INVALID;` |
|    - |  431 | ` 	}` |
|    - |  432 | `#endif` |
|    - |  433 | ` 	/* The miminal size of a zip archive:` |
|    - |  434 | ` 	 * LOCAL_HDR_SZ + CENTRAL_HDR_SZ + END_OF_CENTRAL_HDR_SZ` |
|    - |  435 | ` 	 * 		30				46				22` |
|    - |  436 | ` 	 */` |
|   30 |  437 | ` 	 if( nLen < SXZIP_LOCAL_HDRSZ + SXZIP_CENTRAL_HDRSZ + SXZIP_END_CENTRAL_HDRSZ ){` |
|    5 |  438 | ` 	 	return SXERR_CORRUPT; /* Don't bother processing return immediately */` |
|    - |  439 | ` 	 }` |
|    - |  440 |  |
|   26 |  441 | ` 	zEnd = (unsigned char *)&zBuf[nLen - SXZIP_END_CENTRAL_HDRSZ];` |
|    - |  442 | ` 	/* Find the end of central directory */` |
|  414 |  443 | ` 	while( ((sxu32)((unsigned char *)&zBuf[nLen] - zEnd) < (SXZIP_END_CENTRAL_HDRSZ + SXI16_HIGH)) &&` |
|  585 |  444 | `		zEnd > (unsigned char *)zBuf && SyMemcmp(zEnd,"PK\005\006",sizeof(sxu32)) != 0 ){` |
|  367 |  445 | ` 		zEnd--;` |
|    1 |  446 | ` 	}` |
|    - |  447 | ` 	/* Parse the end of central directory */` |
|   26 |  448 | ` 	rc = ParseEndOfCentralDirectory(&(*pArch),zEnd);` |
|   26 |  449 | ` 	if( rc != SXRET_OK ){` |
|    5 |  450 | ` 		return rc;` |
|    - |  451 | ` 	}` |
|    - |  452 |  |
|    - |  453 | ` 	/* Find the starting offset of the central directory */` |
|   22 |  454 | ` 	zCentral = &zEnd[-(sxi32)pArch->nCentralSize];` |
|   22 |  455 | ` 	if( zCentral <= (unsigned char *)zBuf \|\| SyMemcmp(zCentral,"PK\001\002",sizeof(sxu32)) != 0 ){` |
|    3 |  456 | ` 		if( pArch->nCentralOfft >= nLen ){` |
|    - |  457 | `			/* Corrupted central directory offset */` |
|  ! 0 |  458 | ` 			return SXERR_CORRUPT;` |
|    - |  459 | ` 		}` |
|    3 |  460 | ` 		zCentral = (unsigned char *)&zBuf[pArch->nCentralOfft];` |
|    3 |  461 | ` 		if( SyMemcmp(zCentral,"PK\001\002",sizeof(sxu32)) != 0 ){` |
|    - |  462 | ` 			/* Corrupted zip archive */` |
|    3 |  463 | ` 			return SXERR_CORRUPT;` |
|    - |  464 | ` 		}` |
|    - |  465 | ` 		/* Fall thru and extract all valid entries from the central directory */` |
|  ! 0 |  466 | ` 	}` |
|   20 |  467 | ` 	rc = ZipExtract(&(*pArch),zCentral,(sxu32)(zEnd - zCentral),(void *)zBuf);` |
|   20 |  468 | ` 	return rc;` |
|   16 |  469 | ` }` |
|    - |  470 | `/*` |
|    - |  471 | `  * Default comparison function.` |
|    - |  472 | `  */` |
|  ! 0 |  473 | ` static sxi32 ArchiveHashCmp(const SyString *pStr1,const SyString *pStr2)` |
|  ! 0 |  474 | ` {` |
|    - |  475 | `	 sxi32 rc;` |
|  ! 0 |  476 | `	 rc = SyStringCmp(pStr1,pStr2,SyMemcmp);` |
|  ! 0 |  477 | `	 return rc;` |
|  ! 0 |  478 | ` }` |
|   30 |  479 | `PH7_PRIVATE sxi32 SyArchiveInit(SyArchive *pArch,SyMemBackend *pAllocator,ProcHash xHash,ProcRawStrCmp xCmp)` |
|    3 |  480 | ` {` |
|    - |  481 | `	SyArchiveEntry **apHash;` |
|    - |  482 | `#if defined(UNTRUST)` |
|    - |  483 | ` 	if( pArch == 0 ){` |
|    - |  484 | ` 		return SXERR_EMPTY;` |
|    - |  485 | ` 	}` |
|    - |  486 | `#endif` |
|   33 |  487 | ` 	SyZero(pArch,sizeof(SyArchive));` |
|    - |  488 | ` 	/* Allocate a new hashtable */` |
|   33 |  489 | `	apHash = (SyArchiveEntry **)SyMemBackendAlloc(&(*pAllocator),SXARCHIVE_HASH_SIZE * sizeof(SyArchiveEntry *));` |
|   33 |  490 | `	if( apHash == 0){` |
|  ! 0 |  491 | `		return SXERR_MEM;` |
|    - |  492 | `	}` |
|   33 |  493 | `	SyZero(apHash,SXARCHIVE_HASH_SIZE * sizeof(SyArchiveEntry *));` |
|   33 |  494 | `	pArch->apHash = apHash;` |
|   33 |  495 | `	pArch->xHash  = xHash ? xHash : SyBinHash;` |
|   33 |  496 | `	pArch->xCmp   = xCmp ? xCmp : ArchiveHashCmp;` |
|   33 |  497 | `	pArch->nSize  = SXARCHIVE_HASH_SIZE;` |
|   33 |  498 | ` 	pArch->pAllocator = &(*pAllocator);` |
|   33 |  499 | ` 	pArch->nMagic = SXARCH_MAGIC;` |
|   33 |  500 | ` 	return SXRET_OK;` |
|   18 |  501 | ` }` |
|   14 |  502 | ` static sxi32 ArchiveReleaseEntry(SyMemBackend *pAllocator,SyArchiveEntry *pEntry)` |
|    2 |  503 | ` {` |
|   16 |  504 | ` 	SyArchiveEntry *pDup = pEntry->pNextName;` |
|    - |  505 | ` 	SyArchiveEntry *pNextDup;` |
|    - |  506 |  |
|    - |  507 | ` 	/* Release duplicates first since there are not stored in the hashtable */` |
|    7 |  508 | ` 	for(;;){` |
|   16 |  509 | ` 		if( pEntry->nDup == 0 ){` |
|   16 |  510 | ` 			break;` |
|    - |  511 | ` 		}` |
|  ! 0 |  512 | ` 		pNextDup = pDup->pNextName;` |
|  ! 0 |  513 | `		pDup->nMagic = 0x2661;` |
|  ! 0 |  514 | ` 		SyMemBackendFree(pAllocator,(void *)SyStringData(&pDup->sFileName));` |
|  ! 0 |  515 | ` 		SyMemBackendPoolFree(pAllocator,pDup);` |
|  ! 0 |  516 | ` 		pDup = pNextDup;` |
|  ! 0 |  517 | ` 		pEntry->nDup--;` |
|  ! 0 |  518 | ` 	}` |
|   16 |  519 | `	pEntry->nMagic = 0x2661;` |
|   16 |  520 | `  	SyMemBackendFree(pAllocator,(void *)SyStringData(&pEntry->sFileName));` |
|   16 |  521 | ` 	SyMemBackendPoolFree(pAllocator,pEntry);` |
|   16 |  522 | ` 	return SXRET_OK;` |
|    2 |  523 | ` }` |
|   14 |  524 | `PH7_PRIVATE sxi32 SyArchiveRelease(SyArchive *pArch)` |
|    2 |  525 | ` {` |
|    - |  526 | `	SyArchiveEntry *pEntry,*pNext;` |
|   16 |  527 | ` 	pEntry = pArch->pList;` |
|   14 |  528 | `	for(;;){` |
|   30 |  529 | `		if( pArch->nLoaded < 1 ){` |
|   16 |  530 | `			break;` |
|    - |  531 | `		}` |
|   16 |  532 | `		pNext = pEntry->pNext;` |
|   16 |  533 | `		MACRO_LD_REMOVE(pArch->pList,pEntry);` |
|   16 |  534 | `		ArchiveReleaseEntry(pArch->pAllocator,pEntry);` |
|   16 |  535 | `		pEntry = pNext;` |
|   16 |  536 | `		pArch->nLoaded--;` |
|    2 |  537 | `	}` |
|   16 |  538 | `	SyMemBackendFree(pArch->pAllocator,pArch->apHash);` |
|   16 |  539 | `	pArch->pCursor = 0;` |
|   16 |  540 | `	pArch->nMagic = 0x2626;` |
|   16 |  541 | `	return SXRET_OK;` |
|    2 |  542 | ` }` |
|   14 |  543 | ` PH7_PRIVATE sxi32 SyArchiveResetLoopCursor(SyArchive *pArch)` |
|    2 |  544 | ` {` |
|   16 |  545 | `	pArch->pCursor = pArch->pList;` |
|   16 |  546 | `	return SXRET_OK;` |
|    2 |  547 | ` }` |
|    8 |  548 | ` PH7_PRIVATE sxi32 SyArchiveGetNextEntry(SyArchive *pArch,SyArchiveEntry **ppEntry)` |
|    2 |  549 | ` {` |
|    - |  550 | `	SyArchiveEntry *pNext;` |
|   10 |  551 | `	if( pArch->pCursor == 0 ){` |
|    - |  552 | `		/* Rewind the cursor */` |
|  ! 0 |  553 | `		pArch->pCursor = pArch->pList;` |
|  ! 0 |  554 | `		return SXERR_EOF;` |
|    - |  555 | `	}` |
|   10 |  556 | `	*ppEntry = pArch->pCursor;` |
|   10 |  557 | `	 pNext = pArch->pCursor->pNext;` |
|    - |  558 | `	 /* Advance the cursor to the next entry */` |
|   10 |  559 | `	 pArch->pCursor = pNext;` |
|   10 |  560 | `	 return SXRET_OK;` |
|    6 |  561 | `  }` |
|    - |  562 | `#endif /* PH7_DISABLE_BUILTIN_FUNC */` |
|    - |  563 |  |
