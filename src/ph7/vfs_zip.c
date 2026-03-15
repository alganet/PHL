/**
 * SPDX-FileCopyrightText: 2011, 2012, 2013, 2014 Symisc Systems <licensing@symisc.net>
 * SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
 * SPDX-License-Identifier: BSD-3-Clause
 */
#include "ph7int.h"
/*
 * Section:
 *    ZIP archive processing.
 * Status:
 *    Stable.
 */
typedef struct zip_raw_data zip_raw_data;
struct zip_raw_data
{
	int iType;         /* Where the raw data is stored */
	union raw_data{
		struct mmap_data{
			void *pMap;          /* Memory mapped data */
			ph7_int64 nSize;     /* Map size */
			const ph7_vfs *pVfs; /* Underlying vfs */
		}mmap;
		SyBlob sBlob;  /* Memory buffer */
	}raw;
};
#define ZIP_RAW_DATA_MMAPED 1 /* Memory mapped ZIP raw data */
#define ZIP_RAW_DATA_MEMBUF 2 /* ZIP raw data stored in a dynamically
                               * allocated memory chunk.
							   */
#ifndef PH7_DISABLE_BUILTIN_FUNC
 /*
  * mixed zip_open(string $filename)

  *  Opens a new zip archive for reading.
  * Parameters
  *  $filename
  *   The file name of the ZIP archive to open.
  * Return
  *  A resource handle for later use with zip_read() and zip_close() or FALSE on failure.
  */
PH7_PRIVATE int PH7_builtin_zip_open(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	const ph7_io_stream *pStream;
	SyArchive *pArchive;
	zip_raw_data *pRaw;
	const char *zFile;
	SyBlob *pContents;
	void *pHandle;
	int nLen;
	sxi32 rc;
	if( nArg < 1 || !ph7_value_is_string(apArg[0]) ){
		/* Missing/Invalid arguments,return FALSE */
		ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"Expecting a file path");
		ph7_result_bool(pCtx,0);
		return PH7_OK;
	}
	/* Extract the file path */
	zFile = ph7_value_to_string(apArg[0],&nLen);
	/* Point to the target IO stream device */
	pStream = PH7_VmGetStreamDevice(pCtx->pVm,&zFile,nLen);
	if( pStream == 0 ){
		ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"No such stream device,PH7 is returning FALSE");
		ph7_result_bool(pCtx,0);
		return PH7_OK;
	}
	/* Create an in-memory archive */
	pArchive = (SyArchive *)ph7_context_alloc_chunk(pCtx,sizeof(SyArchive)+sizeof(zip_raw_data),TRUE,FALSE);
	if( pArchive == 0 ){
		ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"PH7 is running out of memory");
		ph7_result_bool(pCtx,0);
		return PH7_OK;
	}
	pRaw = (zip_raw_data *)&pArchive[1];
	/* Initialize the archive */
	SyArchiveInit(pArchive,&pCtx->pVm->sAllocator,0,0);
	/* Extract the default stream */
	if( pStream == pCtx->pVm->pDefStream /* file:// stream*/){
		const ph7_vfs *pVfs;
		/* Try to get a memory view of the whole file since ZIP files
		 * tends to be very big this days,this is a huge performance win.
		 */
		pVfs = PH7_ExportBuiltinVfs();
		if( pVfs && pVfs->xMmap ){
			rc = pVfs->xMmap(zFile,&pRaw->raw.mmap.pMap,&pRaw->raw.mmap.nSize);
			if( rc == PH7_OK ){
				/* Nice,Extract the whole archive */
				rc = SyZipExtractFromBuf(pArchive,(const char *)pRaw->raw.mmap.pMap,(sxu32)pRaw->raw.mmap.nSize);
				if( rc != SXRET_OK ){
					if( pVfs->xUnmap ){
						pVfs->xUnmap(pRaw->raw.mmap.pMap,pRaw->raw.mmap.nSize);
					}
					/* Release the allocated chunk */
					ph7_context_free_chunk(pCtx,pArchive);
					/* Something goes wrong with this ZIP archive,return FALSE */
					ph7_result_bool(pCtx,0);
					return PH7_OK;
				}
				/* Archive successfully opened */
				pRaw->iType = ZIP_RAW_DATA_MMAPED;
				pRaw->raw.mmap.pVfs = pVfs;
				goto success;
			}
		}
		/* FALL THROUGH */
	}
	/* Try to open the file in read-only mode */
	pHandle = PH7_StreamOpenHandle(pCtx->pVm,pStream,zFile,PH7_IO_OPEN_RDONLY,FALSE,0,FALSE,0);
	if( pHandle == 0 ){
		ph7_context_throw_error_format(pCtx,PH7_CTX_ERR,"IO error while opening '%s'",zFile);
		ph7_result_bool(pCtx,0);
		return PH7_OK;
	}
	pContents = &pRaw->raw.sBlob;
	SyBlobInit(pContents,&pCtx->pVm->sAllocator);
	/* Read the whole file */
	PH7_StreamReadWholeFile(pHandle,pStream,pContents);
	/* Assume an invalid ZIP file */
	rc = SXERR_INVALID;
	if( SyBlobLength(pContents) > 0 ){
		/* Extract archive entries */
		rc = SyZipExtractFromBuf(pArchive,(const char *)SyBlobData(pContents),SyBlobLength(pContents));
	}
	pRaw->iType = ZIP_RAW_DATA_MEMBUF;
	/* Close the stream */
	PH7_StreamCloseHandle(pStream,pHandle);
	if( rc != SXRET_OK ){
		/* Release the working buffer */
		SyBlobRelease(pContents);
		/* Release the allocated chunk */
		ph7_context_free_chunk(pCtx,pArchive);
		/* Something goes wrong with this ZIP archive,return FALSE */
		ph7_result_bool(pCtx,0);
		return PH7_OK;
	}
success:
	/* Reset the loop cursor */
	SyArchiveResetLoopCursor(pArchive);
	/* Return the in-memory archive as a resource handle */
	ph7_result_resource(pCtx,pArchive);
	return PH7_OK;
}
/*
  * void zip_close(resource $zip)
  *  Close an in-memory ZIP archive.
  * Parameters
  *  $zip
  *   A ZIP file previously opened with zip_open().
  * Return
  *  null.
  */
PH7_PRIVATE int PH7_builtin_zip_close(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	SyArchive *pArchive;
	zip_raw_data *pRaw;
	if( nArg < 1 || !ph7_value_is_resource(apArg[0]) ){
		/* Missing/Invalid arguments */
		ph7_context_throw_error(pCtx,PH7_CTX_ERR,"Expecting a ZIP archive");
		return PH7_OK;
	}
	/* Point to the in-memory archive */
	pArchive = (SyArchive *)ph7_value_to_resource(apArg[0]);
	/* Make sure we are dealing with a valid ZIP archive */
	if( SXARCH_INVALID(pArchive) ){
		ph7_context_throw_error(pCtx,PH7_CTX_ERR,"Expecting a ZIP archive");
		return PH7_OK;
	}
	/* Release the archive */
	SyArchiveRelease(pArchive);
	pRaw = (zip_raw_data *)&pArchive[1];
	if( pRaw->iType == ZIP_RAW_DATA_MEMBUF ){
		SyBlobRelease(&pRaw->raw.sBlob);
	}else{
		const ph7_vfs *pVfs = pRaw->raw.mmap.pVfs;
		if( pVfs->xUnmap ){
			/* Unmap the memory view */
			pVfs->xUnmap(pRaw->raw.mmap.pMap,pRaw->raw.mmap.nSize);
		}
	}
	/* Release the memory chunk */
	ph7_context_free_chunk(pCtx,pArchive);
	return PH7_OK;
}
/*
  * mixed zip_read(resource $zip)
  *  Reads the next entry from an in-memory ZIP archive.
  * Parameters
  *  $zip
  *   A ZIP file previously opened with zip_open().
  * Return
  *  A directory entry resource for later use with the zip_entry_... functions
  *  or FALSE if there are no more entries to read, or an error code if an error occurred.
  */
PH7_PRIVATE int PH7_builtin_zip_read(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	SyArchiveEntry *pNext = 0; /* cc warning */
	SyArchive *pArchive;
	sxi32 rc;
	if( nArg < 1 || !ph7_value_is_resource(apArg[0]) ){
		/* Missing/Invalid arguments */
		ph7_context_throw_error(pCtx,PH7_CTX_ERR,"Expecting a ZIP archive");
		/* return FALSE */
		ph7_result_bool(pCtx,0);
		return PH7_OK;
	}
	/* Point to the in-memory archive */
	pArchive = (SyArchive *)ph7_value_to_resource(apArg[0]);
	/* Make sure we are dealing with a valid ZIP archive */
	if( SXARCH_INVALID(pArchive) ){
		ph7_context_throw_error(pCtx,PH7_CTX_ERR,"Expecting a ZIP archive");
		/* return FALSE */
		ph7_result_bool(pCtx,0);
		return PH7_OK;
	}
	/* Extract the next entry */
	rc = SyArchiveGetNextEntry(pArchive,&pNext);
	if( rc != SXRET_OK ){
		/* No more entries in the central directory,return FALSE */
		ph7_result_bool(pCtx,0);
	}else{
		/* Return as a resource handle */
		ph7_result_resource(pCtx,pNext);
		/* Point to the ZIP raw data */
		pNext->pUserData = (void *)&pArchive[1];
	}
	return PH7_OK;
}
/*
  * bool zip_entry_open(resource $zip,resource $zip_entry[,string $mode ])
  *  Open a directory entry for reading
  * Parameters
  *  $zip
  *   A ZIP file previously opened with zip_open().
  *  $zip_entry
  *   A directory entry returned by zip_read().
  * $mode
  *   Not used
  * Return
  *  A directory entry resource for later use with the zip_entry_... functions
  *  or FALSE if there are no more entries to read, or an error code if an error occurred.
  */
PH7_PRIVATE int PH7_builtin_zip_entry_open(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	SyArchiveEntry *pEntry;
	SyArchive *pArchive;
	if( nArg < 2 || !ph7_value_is_resource(apArg[0]) || !ph7_value_is_resource(apArg[1]) ){
		/* Missing/Invalid arguments */
		ph7_context_throw_error(pCtx,PH7_CTX_ERR,"Expecting a ZIP archive");
		/* return FALSE */
		ph7_result_bool(pCtx,0);
		return PH7_OK;
	}
	/* Point to the in-memory archive */
	pArchive = (SyArchive *)ph7_value_to_resource(apArg[0]);
	/* Make sure we are dealing with a valid ZIP archive */
	if( SXARCH_INVALID(pArchive) ){
		ph7_context_throw_error(pCtx,PH7_CTX_ERR,"Expecting a ZIP archive");
		/* return FALSE */
		ph7_result_bool(pCtx,0);
		return PH7_OK;
	}
	/* Make sure we are dealing with a valid ZIP archive entry */
	pEntry = (SyArchiveEntry *)ph7_value_to_resource(apArg[1]);
	if( SXARCH_ENTRY_INVALID(pEntry) ){
		ph7_context_throw_error(pCtx,PH7_CTX_ERR,"Expecting a ZIP archive entry");
		/* return FALSE */
		ph7_result_bool(pCtx,0);
		return PH7_OK;
	}
	/* All done. Actually this function is a no-op,return TRUE */
	ph7_result_bool(pCtx,1);
	return PH7_OK;
}
/*
  * bool zip_entry_close(resource $zip_entry)
  *  Close a directory entry.
  * Parameters
  *  $zip_entry
  *   A directory entry returned by zip_read().
  * Return
  *  Returns TRUE on success or FALSE on failure.
  */
PH7_PRIVATE int PH7_builtin_zip_entry_close(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	SyArchiveEntry *pEntry;
	if( nArg < 1 || !ph7_value_is_resource(apArg[0]) ){
		/* Missing/Invalid arguments */
		ph7_context_throw_error(pCtx,PH7_CTX_ERR,"Expecting a ZIP archive entry");
		/* return FALSE */
		ph7_result_bool(pCtx,0);
		return PH7_OK;
	}
	/* Make sure we are dealing with a valid ZIP archive entry */
	pEntry = (SyArchiveEntry *)ph7_value_to_resource(apArg[0]);
	if( SXARCH_ENTRY_INVALID(pEntry) ){
		ph7_context_throw_error(pCtx,PH7_CTX_ERR,"Expecting a ZIP archive entry");
		/* return FALSE */
		ph7_result_bool(pCtx,0);
		return PH7_OK;
	}
	/* Reset the read cursor */
	pEntry->nReadCount = 0;
	/*All done. Actually this function is a no-op,return TRUE */
	ph7_result_bool(pCtx,1);
	return PH7_OK;
}
/*
  * string zip_entry_name(resource $zip_entry)
  *  Retrieve the name of a directory entry.
  * Parameters
  *  $zip_entry
  *   A directory entry returned by zip_read().
  * Return
  *  The name of the directory entry.
  */
PH7_PRIVATE int PH7_builtin_zip_entry_name(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	SyArchiveEntry *pEntry;
	SyString *pName;
	if( nArg < 1 || !ph7_value_is_resource(apArg[0]) ){
		/* Missing/Invalid arguments */
		ph7_context_throw_error(pCtx,PH7_CTX_ERR,"Expecting a ZIP archive entry");
		/* return FALSE */
		ph7_result_bool(pCtx,0);
		return PH7_OK;
	}
	/* Make sure we are dealing with a valid ZIP archive entry */
	pEntry = (SyArchiveEntry *)ph7_value_to_resource(apArg[0]);
	if( SXARCH_ENTRY_INVALID(pEntry) ){
		ph7_context_throw_error(pCtx,PH7_CTX_ERR,"Expecting a ZIP archive entry");
		/* return FALSE */
		ph7_result_bool(pCtx,0);
		return PH7_OK;
	}
	/* Return entry name */
	pName = &pEntry->sFileName;
	ph7_result_string(pCtx,pName->zString,(int)pName->nByte);
	return PH7_OK;
}
/*
  * int64 zip_entry_filesize(resource $zip_entry)
  *  Retrieve the actual file size of a directory entry.
  * Parameters
  *  $zip_entry
  *   A directory entry returned by zip_read().
  * Return
  *  The size of the directory entry.
  */
PH7_PRIVATE int PH7_builtin_zip_entry_filesize(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	SyArchiveEntry *pEntry;
	if( nArg < 1 || !ph7_value_is_resource(apArg[0]) ){
		/* Missing/Invalid arguments */
		ph7_context_throw_error(pCtx,PH7_CTX_ERR,"Expecting a ZIP archive entry");
		/* return FALSE */
		ph7_result_bool(pCtx,0);
		return PH7_OK;
	}
	/* Make sure we are dealing with a valid ZIP archive entry */
	pEntry = (SyArchiveEntry *)ph7_value_to_resource(apArg[0]);
	if( SXARCH_ENTRY_INVALID(pEntry) ){
		ph7_context_throw_error(pCtx,PH7_CTX_ERR,"Expecting a ZIP archive entry");
		/* return FALSE */
		ph7_result_bool(pCtx,0);
		return PH7_OK;
	}
	/* Return entry size */
	ph7_result_int64(pCtx,(ph7_int64)pEntry->nByte);
	return PH7_OK;
}
/*
  * int64 zip_entry_compressedsize(resource $zip_entry)
  *  Retrieve the compressed size of a directory entry.
  * Parameters
  *  $zip_entry
  *   A directory entry returned by zip_read().
  * Return
  *  The compressed size.
  */
PH7_PRIVATE int PH7_builtin_zip_entry_compressedsize(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	SyArchiveEntry *pEntry;
	if( nArg < 1 || !ph7_value_is_resource(apArg[0]) ){
		/* Missing/Invalid arguments */
		ph7_context_throw_error(pCtx,PH7_CTX_ERR,"Expecting a ZIP archive entry");
		/* return FALSE */
		ph7_result_bool(pCtx,0);
		return PH7_OK;
	}
	/* Make sure we are dealing with a valid ZIP archive entry */
	pEntry = (SyArchiveEntry *)ph7_value_to_resource(apArg[0]);
	if( SXARCH_ENTRY_INVALID(pEntry) ){
		ph7_context_throw_error(pCtx,PH7_CTX_ERR,"Expecting a ZIP archive entry");
		/* return FALSE */
		ph7_result_bool(pCtx,0);
		return PH7_OK;
	}
	/* Return entry compressed size */
	ph7_result_int64(pCtx,(ph7_int64)pEntry->nByteCompr);
	return PH7_OK;
}
/*
  * string zip_entry_read(resource $zip_entry[,int $length])
  *  Reads from an open directory entry.
  * Parameters
  *  $zip_entry
  *   A directory entry returned by zip_read().
  *  $length
  *   The number of bytes to return. If not specified, this function
  *   will attempt to read 1024 bytes.
  * Return
  *  Returns the data read, or FALSE if the end of the file is reached.
  */
PH7_PRIVATE int PH7_builtin_zip_entry_read(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	SyArchiveEntry *pEntry;
	zip_raw_data *pRaw;
	const char *zData;
	int iLength;
	if( nArg < 1 || !ph7_value_is_resource(apArg[0]) ){
		/* Missing/Invalid arguments */
		ph7_context_throw_error(pCtx,PH7_CTX_ERR,"Expecting a ZIP archive entry");
		/* return FALSE */
		ph7_result_bool(pCtx,0);
		return PH7_OK;
	}
	/* Make sure we are dealing with a valid ZIP archive entry */
	pEntry = (SyArchiveEntry *)ph7_value_to_resource(apArg[0]);
	if( SXARCH_ENTRY_INVALID(pEntry) ){
		ph7_context_throw_error(pCtx,PH7_CTX_ERR,"Expecting a ZIP archive entry");
		/* return FALSE */
		ph7_result_bool(pCtx,0);
		return PH7_OK;
	}
	zData = 0;
	if( pEntry->nReadCount >= pEntry->nByteCompr ){
		/* No more data to read,return FALSE */
		ph7_result_bool(pCtx,0);
		return PH7_OK;
	}
	/* Set a default read length */
	iLength = 1024;
	if( nArg > 1 ){
		iLength = ph7_value_to_int(apArg[1]);
		if( iLength < 1 ){
			iLength = 1024;
		}
	}
	if( (sxu32)iLength > pEntry->nByteCompr - pEntry->nReadCount ){
		iLength = (int)(pEntry->nByteCompr - pEntry->nReadCount);
	}
	/* Return the entry contents */
	pRaw = (zip_raw_data *)pEntry->pUserData;
	if( pRaw->iType == ZIP_RAW_DATA_MEMBUF ){
		zData = (const char *)SyBlobDataAt(&pRaw->raw.sBlob,(pEntry->nOfft+pEntry->nReadCount));
	}else{
		const char *zMap = (const char *)pRaw->raw.mmap.pMap;
		/* Memory mmaped chunk */
		zData = &zMap[pEntry->nOfft+pEntry->nReadCount];
	}
	/* Increment the read counter */
	pEntry->nReadCount += iLength;
	/* Return the raw data */
	ph7_result_string(pCtx,zData,iLength);
	return PH7_OK;
}
/*
  * bool zip_entry_reset_read_cursor(resource $zip_entry)
  *  Reset the read cursor of an open directory entry.
  * Parameters
  *  $zip_entry
  *   A directory entry returned by zip_read().
  * Return
  *  TRUE on success,FALSE on failure.
  * Note that this is a symisc eXtension.
  */
PH7_PRIVATE int PH7_builtin_zip_entry_reset_read_cursor(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	SyArchiveEntry *pEntry;
	if( nArg < 1 || !ph7_value_is_resource(apArg[0]) ){
		/* Missing/Invalid arguments */
		ph7_context_throw_error(pCtx,PH7_CTX_ERR,"Expecting a ZIP archive entry");
		/* return FALSE */
		ph7_result_bool(pCtx,0);
		return PH7_OK;
	}
	/* Make sure we are dealing with a valid ZIP archive entry */
	pEntry = (SyArchiveEntry *)ph7_value_to_resource(apArg[0]);
	if( SXARCH_ENTRY_INVALID(pEntry) ){
		ph7_context_throw_error(pCtx,PH7_CTX_ERR,"Expecting a ZIP archive entry");
		/* return FALSE */
		ph7_result_bool(pCtx,0);
		return PH7_OK;
	}
	/* Reset the cursor */
	pEntry->nReadCount = 0;
	/* Return TRUE */
	ph7_result_bool(pCtx,1);
	return PH7_OK;
}
/*
  * string zip_entry_compressionmethod(resource $zip_entry)
  *  Retrieve the compression method of a directory entry.
  * Parameters
  *  $zip_entry
  *   A directory entry returned by zip_read().
  * Return
  *  The compression method on success or FALSE on failure.
  */
PH7_PRIVATE int PH7_builtin_zip_entry_compressionmethod(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	SyArchiveEntry *pEntry;
	if( nArg < 1 || !ph7_value_is_resource(apArg[0]) ){
		/* Missing/Invalid arguments */
		ph7_context_throw_error(pCtx,PH7_CTX_ERR,"Expecting a ZIP archive entry");
		/* return FALSE */
		ph7_result_bool(pCtx,0);
		return PH7_OK;
	}
	/* Make sure we are dealing with a valid ZIP archive entry */
	pEntry = (SyArchiveEntry *)ph7_value_to_resource(apArg[0]);
	if( SXARCH_ENTRY_INVALID(pEntry) ){
		ph7_context_throw_error(pCtx,PH7_CTX_ERR,"Expecting a ZIP archive entry");
		/* return FALSE */
		ph7_result_bool(pCtx,0);
		return PH7_OK;
	}
	switch(pEntry->nComprMeth){
	case 0:
		/* No compression;entry is stored */
		ph7_result_string(pCtx,"stored",(int)sizeof("stored")-1);
		break;
	case 8:
		/* Entry is deflated (Default compression algorithm)  */
		ph7_result_string(pCtx,"deflate",(int)sizeof("deflate")-1);
		break;
		/* Exotic compression algorithms */
	case 1:
		ph7_result_string(pCtx,"shrunk",(int)sizeof("shrunk")-1);
		break;
	case 2:
	case 3:
	case 4:
	case 5:
		/* Entry is reduced */
		ph7_result_string(pCtx,"reduced",(int)sizeof("reduced")-1);
		break;
	case 6:
		/* Entry is imploded */
		ph7_result_string(pCtx,"implode",(int)sizeof("implode")-1);
		break;
	default:
		ph7_result_string(pCtx,"unknown",(int)sizeof("unknown")-1);
		break;
	}
	return PH7_OK;
}
#endif /* PH7_DISABLE_BUILTIN_FUNC */
