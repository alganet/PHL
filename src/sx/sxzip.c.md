# src/sx/sxzip.c

<style>code, pre { background: none !important; white-space: pre !important; width: 100% !important; display: inline-block !important; } td { border: none !important; margin-top: 0 !important; margin-bottom: 0 !important; padding-top: 0 !important; padding-bottom: 0 !important; }</style>

Coverage: 222/301 lines (73.75%)

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
|    2 |  117 | `{` |
|  156 |  118 | `	if( Len < sizeof(sxu32) ){` |
|  ! 0 |  119 | `		return SXERR_SHORT;` |
|    - |  120 | `	}` |
|  156 |  121 | `	*uNB =  buf[0] + (buf[1] << 8) + (buf[2] << 16) + (buf[3] << 24);` |
|  156 |  122 | `	return SXRET_OK;` |
|   79 |  123 | `}` |
|  156 |  124 | `static sxi32 SyLittleEndianUnpack16(sxu16 *pOut,const unsigned char *zBuf,sxu32 nLen)` |
|    2 |  125 | `{` |
|  158 |  126 | `	if( nLen < sizeof(sxu16) ){` |
|  ! 0 |  127 | `		return SXERR_SHORT;` |
|    - |  128 | `	}` |
|  158 |  129 | `	*pOut = zBuf[0] + (zBuf[1] <<8);` |
|    - |  130 |  |
|  158 |  131 | `	return SXRET_OK;` |
|   80 |  132 | `}` |
|   18 |  133 | `static sxi32 SyDosTimeFormat(sxu32 nDosDate,Sytm *pOut)` |
|    2 |  134 | `{` |
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
|    2 |  147 | `}` |
|    - |  148 | `/*` |
|    - |  149 | ` * Archive hashtable manager` |
|    - |  150 | ` */` |
|   14 |  151 | `static sxi32 ArchiveHashGetEntry(SyArchive *pArch,const char *zName,sxu32 nLen,SyArchiveEntry **ppEntry)` |
|    2 |  152 | `{` |
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
|    9 |  175 | `}` |
|   14 |  176 | `static void ArchiveHashBucketInstall(SyArchiveEntry **apTable,sxu32 nBucket,SyArchiveEntry *pEntry)` |
|    2 |  177 | `{` |
|   16 |  178 | `	pEntry->pNextHash = apTable[nBucket];` |
|   16 |  179 | `	if( apTable[nBucket] != 0 ){` |
|  ! 0 |  180 | `		apTable[nBucket]->pPrevHash = pEntry;` |
|  ! 0 |  181 | `	}` |
|   16 |  182 | `	apTable[nBucket] = pEntry;` |
|   16 |  183 | `}` |
|  ! 0 |  184 | `static sxi32 ArchiveHashGrowTable(SyArchive *pArch)` |
|  ! 0 |  185 | `{` |
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
|  ! 0 |  208 | `}` |
|   14 |  209 | `static sxi32 ArchiveHashInstallEntry(SyArchive *pArch,SyArchiveEntry *pEntry)` |
|    2 |  210 | `{` |
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
|    2 |  221 | `}` |
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
|    2 |  332 | `}` |
|   18 |  333 | `static sxi32 ZipFixOffset(SyArchiveEntry *pEntry,void *pSrc,sxu32 nSrcLen)` |
|    2 |  334 | `{` |
|    - |  335 | `	sxu16 nExtra,nNameLen;` |
|    - |  336 | `	unsigned char *zHdr;` |
|   20 |  337 | `	nExtra = nNameLen = 0;` |
|    - |  338 | `	/* Bound the local-header read: nOfft comes from the (attacker-controlled)` |
|    - |  339 | `	 * central record and can point far past the mapped source buffer. */` |
|   20 |  340 | `	if( pEntry->nOfft > nSrcLen \|\| nSrcLen - pEntry->nOfft < SXZIP_LOCAL_HDRSZ ){` |
|  ! 0 |  341 | `		return SXERR_CORRUPT;` |
|    - |  342 | `	}` |
|   20 |  343 | `	zHdr = (unsigned char *)pSrc;` |
|   20 |  344 | `	zHdr = &zHdr[pEntry->nOfft];` |
|   20 |  345 | `	if( SyMemcmp(zHdr,"PK\003\004",sizeof(sxu32)) != 0 ){` |
|    5 |  346 | `		return SXERR_CORRUPT;` |
|    - |  347 | `	}` |
|   16 |  348 | `	SyLittleEndianUnpack16(&nNameLen,&zHdr[26],sizeof(sxu16));` |
|   16 |  349 | `	SyLittleEndianUnpack16(&nExtra,&zHdr[28],sizeof(sxu16));` |
|    - |  350 | `	/* Fix contents offset */` |
|   16 |  351 | `	pEntry->nOfft += SXZIP_LOCAL_HDRSZ + nExtra + nNameLen;` |
|   16 |  352 | `	if( pEntry->nOfft > nSrcLen ){` |
|    - |  353 | `		/* Contents would start past the source buffer */` |
|  ! 0 |  354 | `		return SXERR_CORRUPT;` |
|    - |  355 | `	}` |
|   16 |  356 | `	return SXRET_OK;` |
|   11 |  357 | `}` |
|    - |  358 | `/*` |
|    - |  359 | ` * Extract all valid entries from the central directory` |
|    - |  360 | ` */` |
|   18 |  361 | `static sxi32 ZipExtract(SyArchive *pArch,const unsigned char *zCentral,sxu32 nLen,void *pSrc,sxu32 nSrcLen)` |
|    2 |  362 | `{` |
|    - |  363 | `	SyArchiveEntry *pEntry,*pDup;` |
|    - |  364 | `	const unsigned char *zEnd ; /* End of central directory */` |
|    - |  365 | `	sxu32 nIncr,nOfft;          /* Central Offset */` |
|    - |  366 | `	SyString *pName;	        /* Entry name */` |
|    - |  367 | `	char *zName;` |
|    - |  368 | `	sxi32 rc;` |
|    - |  369 |  |
|   20 |  370 | `	nOfft = nIncr = 0;` |
|   20 |  371 | `	zEnd = &zCentral[nLen];` |
|    - |  372 |  |
|   16 |  373 | `	for(;;){` |
|   34 |  374 | `		if( &zCentral[nOfft] >= zEnd \|\| (sxu32)(zEnd - &zCentral[nOfft]) < SXZIP_CENTRAL_HDRSZ ){` |
|    - |  375 | `			/* No room left for a full central directory record */` |
|    9 |  376 | `			break;` |
|    - |  377 | `		}` |
|    - |  378 | `		/* Add a new entry */` |
|   20 |  379 | `		pEntry = (SyArchiveEntry *)SyMemBackendPoolAlloc(pArch->pAllocator,sizeof(SyArchiveEntry));` |
|   20 |  380 | `		if( pEntry == 0 ){` |
|  ! 0 |  381 | `			break;` |
|    - |  382 | `		}` |
|   20 |  383 | `		SyZero(pEntry,sizeof(SyArchiveEntry));` |
|   20 |  384 | `		pEntry->nMagic = SXARCH_MAGIC;` |
|   20 |  385 | `		nIncr = 0;` |
|   20 |  386 | `		rc = GetCentralDirectoryEntry(&(*pArch),pEntry,&zCentral[nOfft],&nIncr);` |
|   20 |  387 | `		if( rc == SXRET_OK && nIncr > (sxu32)(zEnd - &zCentral[nOfft]) ){` |
|    - |  388 | `			/* Name/extra/comment lengths run past the central directory end */` |
|  ! 0 |  389 | `			rc = SXERR_CORRUPT;` |
|  ! 0 |  390 | `		}` |
|   20 |  391 | `		if( rc == SXRET_OK ){` |
|    - |  392 | `			/* Fix the starting record offset so we can access entry contents correctly */` |
|   20 |  393 | `			rc = ZipFixOffset(pEntry,pSrc,nSrcLen);` |
|    9 |  394 | `		}` |
|   20 |  395 | `		if(rc != SXRET_OK ){` |
|    5 |  396 | `			sxu32 nJmp = 0;` |
|    5 |  397 | `			SyMemBackendPoolFree(pArch->pAllocator,pEntry);` |
|    - |  398 | `			/* Try to recover by brute-forcing for a valid central directory record.` |
|    - |  399 | `			 * Guard the window: a corrupted record can claim lengths that put` |
|    - |  400 | `			 * nOfft + nIncr past zEnd, and the unsigned size would underflow. */` |
|    5 |  401 | `			if( nOfft + nIncr < (sxu32)(zEnd - zCentral) &&` |
|  ! 0 |  402 | `				SXRET_OK == SyBlobSearch((const void *)&zCentral[nOfft + nIncr],(sxu32)(zEnd - &zCentral[nOfft + nIncr]),` |
|    - |  403 | `				(const void *)"PK\001\002",sizeof(sxu32),&nJmp)){` |
|  ! 0 |  404 | `					nOfft += nIncr + nJmp; /* Check next entry */` |
|  ! 0 |  405 | `					continue;` |
|    - |  406 | `			}` |
|    5 |  407 | `			break; /* Giving up,archive is hopelessly corrupted */` |
|    - |  408 | `		}` |
|   16 |  409 | `		pName = &pEntry->sFileName;` |
|   16 |  410 | `		pName->zString = (const char *)&zCentral[nOfft + SXZIP_CENTRAL_HDRSZ];` |
|   16 |  411 | `		if( pName->nByte <= 0 \|\| ( pEntry->nByte <= 0 && pName->zString[pName->nByte - 1] != '/') ){` |
|    - |  412 | `			/* Ignore zero length records (except folders) and records without names */` |
|  ! 0 |  413 | `			SyMemBackendPoolFree(pArch->pAllocator,pEntry);` |
|  ! 0 |  414 | `		 	nOfft += nIncr; /* Check next entry */` |
|  ! 0 |  415 | `			continue;` |
|    - |  416 | `		}` |
|   16 |  417 | `		zName = SyMemBackendStrDup(pArch->pAllocator,pName->zString,pName->nByte);` |
|   16 |  418 | ` 	 	if( zName == 0 ){` |
|  ! 0 |  419 | ` 	 		 SyMemBackendPoolFree(pArch->pAllocator,pEntry);` |
|  ! 0 |  420 | `		 	 nOfft += nIncr; /* Check next entry */` |
|  ! 0 |  421 | `			continue;` |
|    - |  422 | ` 	 	}` |
|   16 |  423 | `		pName->zString = (const char *)zName;` |
|    - |  424 | `		/* Check for duplicates */` |
|   16 |  425 | `		rc = ArchiveHashGetEntry(&(*pArch),pName->zString,pName->nByte,&pDup);` |
|   16 |  426 | `		if( rc == SXRET_OK ){` |
|    - |  427 | `			/* Another entry with the same name exists ; link them together */` |
|  ! 0 |  428 | `			pEntry->pNextName = pDup->pNextName;` |
|  ! 0 |  429 | `			pDup->pNextName = pEntry;` |
|  ! 0 |  430 | `			pDup->nDup++;` |
|  ! 0 |  431 | `		}else{` |
|    - |  432 | `			/* Insert in hashtable */` |
|   16 |  433 | `			ArchiveHashInstallEntry(pArch,pEntry);` |
|    - |  434 | `		}` |
|   16 |  435 | `		nOfft += nIncr;	/* Check next record */` |
|    2 |  436 | `	}` |
|   20 |  437 | `	pArch->pCursor = pArch->pList;` |
|    - |  438 |  |
|   20 |  439 | `	return pArch->nLoaded > 0 ? SXRET_OK : SXERR_EMPTY;` |
|    2 |  440 | `}` |
|   28 |  441 | `PH7_PRIVATE sxi32 SyZipExtractFromBuf(SyArchive *pArch,const char *zBuf,sxu32 nLen)` |
|    2 |  442 | ` {` |
|    - |  443 | ` 	const unsigned char *zCentral,*zEnd;` |
|    - |  444 | ` 	sxi32 rc;` |
|    - |  445 | `#if defined(UNTRUST)` |
|    - |  446 | ` 	if( SXARCH_INVALID(pArch) \|\| zBuf == 0 ){` |
|    - |  447 | ` 		return SXERR_INVALID;` |
|    - |  448 | ` 	}` |
|    - |  449 | `#endif` |
|    - |  450 | ` 	/* The miminal size of a zip archive:` |
|    - |  451 | ` 	 * LOCAL_HDR_SZ + CENTRAL_HDR_SZ + END_OF_CENTRAL_HDR_SZ` |
|    - |  452 | ` 	 * 		30				46				22` |
|    - |  453 | ` 	 */` |
|   30 |  454 | ` 	 if( nLen < SXZIP_LOCAL_HDRSZ + SXZIP_CENTRAL_HDRSZ + SXZIP_END_CENTRAL_HDRSZ ){` |
|    5 |  455 | ` 	 	return SXERR_CORRUPT; /* Don't bother processing return immediately */` |
|    - |  456 | ` 	 }` |
|    - |  457 |  |
|   26 |  458 | ` 	zEnd = (unsigned char *)&zBuf[nLen - SXZIP_END_CENTRAL_HDRSZ];` |
|    - |  459 | ` 	/* Find the end of central directory */` |
|  414 |  460 | ` 	while( ((sxu32)((unsigned char *)&zBuf[nLen] - zEnd) < (SXZIP_END_CENTRAL_HDRSZ + SXI16_HIGH)) &&` |
|  585 |  461 | `		zEnd > (unsigned char *)zBuf && SyMemcmp(zEnd,"PK\005\006",sizeof(sxu32)) != 0 ){` |
|  367 |  462 | ` 		zEnd--;` |
|    1 |  463 | ` 	}` |
|    - |  464 | ` 	/* Parse the end of central directory */` |
|   26 |  465 | ` 	rc = ParseEndOfCentralDirectory(&(*pArch),zEnd);` |
|   26 |  466 | ` 	if( rc != SXRET_OK ){` |
|    5 |  467 | ` 		return rc;` |
|    - |  468 | ` 	}` |
|    - |  469 |  |
|    - |  470 | ` 	/* Find the starting offset of the central directory */` |
|   22 |  471 | ` 	zCentral = &zEnd[-(sxi32)pArch->nCentralSize];` |
|   22 |  472 | ` 	if( zCentral <= (unsigned char *)zBuf \|\| SyMemcmp(zCentral,"PK\001\002",sizeof(sxu32)) != 0 ){` |
|    3 |  473 | ` 		if( pArch->nCentralOfft >= nLen ){` |
|    - |  474 | `			/* Corrupted central directory offset */` |
|  ! 0 |  475 | ` 			return SXERR_CORRUPT;` |
|    - |  476 | ` 		}` |
|    3 |  477 | ` 		zCentral = (unsigned char *)&zBuf[pArch->nCentralOfft];` |
|    3 |  478 | ` 		if( SyMemcmp(zCentral,"PK\001\002",sizeof(sxu32)) != 0 ){` |
|    - |  479 | ` 			/* Corrupted zip archive */` |
|    3 |  480 | ` 			return SXERR_CORRUPT;` |
|    - |  481 | ` 		}` |
|    - |  482 | ` 		/* Fall thru and extract all valid entries from the central directory */` |
|  ! 0 |  483 | ` 	}` |
|   20 |  484 | ` 	rc = ZipExtract(&(*pArch),zCentral,(sxu32)(zEnd - zCentral),(void *)zBuf,nLen);` |
|   20 |  485 | ` 	return rc;` |
|   16 |  486 | ` }` |
|    - |  487 | `/*` |
|    - |  488 | `  * Default comparison function.` |
|    - |  489 | `  */` |
|  ! 0 |  490 | ` static sxi32 ArchiveHashCmp(const SyString *pStr1,const SyString *pStr2)` |
|  ! 0 |  491 | ` {` |
|    - |  492 | `	 sxi32 rc;` |
|  ! 0 |  493 | `	 rc = SyStringCmp(pStr1,pStr2,SyMemcmp);` |
|  ! 0 |  494 | `	 return rc;` |
|  ! 0 |  495 | ` }` |
|   30 |  496 | `PH7_PRIVATE sxi32 SyArchiveInit(SyArchive *pArch,SyMemBackend *pAllocator,ProcHash xHash,ProcRawStrCmp xCmp)` |
|    3 |  497 | ` {` |
|    - |  498 | `	SyArchiveEntry **apHash;` |
|    - |  499 | `#if defined(UNTRUST)` |
|    - |  500 | ` 	if( pArch == 0 ){` |
|    - |  501 | ` 		return SXERR_EMPTY;` |
|    - |  502 | ` 	}` |
|    - |  503 | `#endif` |
|   33 |  504 | ` 	SyZero(pArch,sizeof(SyArchive));` |
|    - |  505 | ` 	/* Allocate a new hashtable */` |
|   33 |  506 | `	apHash = (SyArchiveEntry **)SyMemBackendAlloc(&(*pAllocator),SXARCHIVE_HASH_SIZE * sizeof(SyArchiveEntry *));` |
|   33 |  507 | `	if( apHash == 0){` |
|  ! 0 |  508 | `		return SXERR_MEM;` |
|    - |  509 | `	}` |
|   33 |  510 | `	SyZero(apHash,SXARCHIVE_HASH_SIZE * sizeof(SyArchiveEntry *));` |
|   33 |  511 | `	pArch->apHash = apHash;` |
|   33 |  512 | `	pArch->xHash  = xHash ? xHash : SyBinHash;` |
|   33 |  513 | `	pArch->xCmp   = xCmp ? xCmp : ArchiveHashCmp;` |
|   33 |  514 | `	pArch->nSize  = SXARCHIVE_HASH_SIZE;` |
|   33 |  515 | ` 	pArch->pAllocator = &(*pAllocator);` |
|   33 |  516 | ` 	pArch->nMagic = SXARCH_MAGIC;` |
|   33 |  517 | ` 	return SXRET_OK;` |
|   18 |  518 | ` }` |
|   14 |  519 | ` static sxi32 ArchiveReleaseEntry(SyMemBackend *pAllocator,SyArchiveEntry *pEntry)` |
|    2 |  520 | ` {` |
|   16 |  521 | ` 	SyArchiveEntry *pDup = pEntry->pNextName;` |
|    - |  522 | ` 	SyArchiveEntry *pNextDup;` |
|    - |  523 |  |
|    - |  524 | ` 	/* Release duplicates first since there are not stored in the hashtable */` |
|    7 |  525 | ` 	for(;;){` |
|   16 |  526 | ` 		if( pEntry->nDup == 0 ){` |
|   16 |  527 | ` 			break;` |
|    - |  528 | ` 		}` |
|  ! 0 |  529 | ` 		pNextDup = pDup->pNextName;` |
|  ! 0 |  530 | `		pDup->nMagic = 0x2661;` |
|  ! 0 |  531 | ` 		SyMemBackendFree(pAllocator,(void *)SyStringData(&pDup->sFileName));` |
|  ! 0 |  532 | ` 		SyMemBackendPoolFree(pAllocator,pDup);` |
|  ! 0 |  533 | ` 		pDup = pNextDup;` |
|  ! 0 |  534 | ` 		pEntry->nDup--;` |
|  ! 0 |  535 | ` 	}` |
|   16 |  536 | `	pEntry->nMagic = 0x2661;` |
|   16 |  537 | `  	SyMemBackendFree(pAllocator,(void *)SyStringData(&pEntry->sFileName));` |
|   16 |  538 | ` 	SyMemBackendPoolFree(pAllocator,pEntry);` |
|   16 |  539 | ` 	return SXRET_OK;` |
|    2 |  540 | ` }` |
|   14 |  541 | `PH7_PRIVATE sxi32 SyArchiveRelease(SyArchive *pArch)` |
|    2 |  542 | ` {` |
|    - |  543 | `	SyArchiveEntry *pEntry,*pNext;` |
|   16 |  544 | ` 	pEntry = pArch->pList;` |
|   14 |  545 | `	for(;;){` |
|   30 |  546 | `		if( pArch->nLoaded < 1 ){` |
|   16 |  547 | `			break;` |
|    - |  548 | `		}` |
|   16 |  549 | `		pNext = pEntry->pNext;` |
|   16 |  550 | `		MACRO_LD_REMOVE(pArch->pList,pEntry);` |
|   16 |  551 | `		ArchiveReleaseEntry(pArch->pAllocator,pEntry);` |
|   16 |  552 | `		pEntry = pNext;` |
|   16 |  553 | `		pArch->nLoaded--;` |
|    2 |  554 | `	}` |
|   16 |  555 | `	SyMemBackendFree(pArch->pAllocator,pArch->apHash);` |
|   16 |  556 | `	pArch->pCursor = 0;` |
|   16 |  557 | `	pArch->nMagic = 0x2626;` |
|   16 |  558 | `	return SXRET_OK;` |
|    2 |  559 | ` }` |
|   14 |  560 | ` PH7_PRIVATE sxi32 SyArchiveResetLoopCursor(SyArchive *pArch)` |
|    2 |  561 | ` {` |
|   16 |  562 | `	pArch->pCursor = pArch->pList;` |
|   16 |  563 | `	return SXRET_OK;` |
|    2 |  564 | ` }` |
|    8 |  565 | ` PH7_PRIVATE sxi32 SyArchiveGetNextEntry(SyArchive *pArch,SyArchiveEntry **ppEntry)` |
|    2 |  566 | ` {` |
|    - |  567 | `	SyArchiveEntry *pNext;` |
|   10 |  568 | `	if( pArch->pCursor == 0 ){` |
|    - |  569 | `		/* Rewind the cursor */` |
|  ! 0 |  570 | `		pArch->pCursor = pArch->pList;` |
|  ! 0 |  571 | `		return SXERR_EOF;` |
|    - |  572 | `	}` |
|   10 |  573 | `	*ppEntry = pArch->pCursor;` |
|   10 |  574 | `	 pNext = pArch->pCursor->pNext;` |
|    - |  575 | `	 /* Advance the cursor to the next entry */` |
|   10 |  576 | `	 pArch->pCursor = pNext;` |
|   10 |  577 | `	 return SXRET_OK;` |
|    6 |  578 | `  }` |
|    - |  579 | `#endif /* PH7_DISABLE_BUILTIN_FUNC */` |
|    - |  580 |  |
