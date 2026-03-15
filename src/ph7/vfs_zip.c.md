# src/ph7/vfs_zip.c

<style>code, pre { background: none !important; white-space: pre !important; width: 100% !important; display: inline-block !important; } td { border: none !important; margin-top: 0 !important; margin-bottom: 0 !important; padding-top: 0 !important; padding-bottom: 0 !important; }</style>

Coverage: 152/261 lines (58.24%)

[Root index](../../index.md) | [Directory index](index.md)

| Hits | Line | Source |
| ---: | ---: | :--- |
|    - |    1 | `/**` |
|    - |    2 | ` * SPDX-FileCopyrightText: 2011, 2012, 2013, 2014 Symisc Systems <licensing@symisc.net>` |
|    - |    3 | ` * SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>` |
|    - |    4 | ` * SPDX-License-Identifier: BSD-3-Clause` |
|    - |    5 | ` */` |
|    - |    6 | `#include "ph7int.h"` |
|    - |    7 | `/*` |
|    - |    8 | ` * Section:` |
|    - |    9 | ` *    ZIP archive processing.` |
|    - |   10 | ` * Status:` |
|    - |   11 | ` *    Stable.` |
|    - |   12 | ` */` |
|    - |   13 | `typedef struct zip_raw_data zip_raw_data;` |
|    - |   14 | `struct zip_raw_data` |
|    - |   15 |  |
|    - |   16 | `	int iType;         /* Where the raw data is stored */` |
|    - |   17 | `	union raw_data{` |
|    - |   18 | `		struct mmap_data{` |
|    - |   19 | `			void *pMap;          /* Memory mapped data */` |
|    - |   20 | `			ph7_int64 nSize;     /* Map size */` |
|    - |   21 | `			const ph7_vfs *pVfs; /* Underlying vfs */` |
|    - |   22 | `		}mmap;` |
|    - |   23 | `		SyBlob sBlob;  /* Memory buffer */` |
|    - |   24 | `	}raw;` |
|    - |   25 | `};` |
|    - |   26 | `#define ZIP_RAW_DATA_MMAPED 1 /* Memory mapped ZIP raw data */` |
|    - |   27 | `#define ZIP_RAW_DATA_MEMBUF 2 /* ZIP raw data stored in a dynamically` |
|    - |   28 | `                               * allocated memory chunk.` |
|    - |   29 | `							   */` |
|    - |   30 | `#ifndef PH7_DISABLE_BUILTIN_FUNC` |
|    - |   31 | ` /*` |
|    - |   32 | `  * mixed zip_open(string $filename)` |
|    - |   33 |  |
|    - |   34 | `  *  Opens a new zip archive for reading.` |
|    - |   35 | `  * Parameters` |
|    - |   36 | `  *  $filename` |
|    - |   37 | `  *   The file name of the ZIP archive to open.` |
|    - |   38 | `  * Return` |
|    - |   39 | `  *  A resource handle for later use with zip_read() and zip_close() or FALSE on failure.` |
|    - |   40 | `  */` |
|   30 |   41 | `PH7_PRIVATE int PH7_builtin_zip_open(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|    2 |   42 |  |
|    - |   43 | `	const ph7_io_stream *pStream;` |
|    - |   44 | `	SyArchive *pArchive;` |
|    - |   45 | `	zip_raw_data *pRaw;` |
|    - |   46 | `	const char *zFile;` |
|    - |   47 | `	SyBlob *pContents;` |
|    - |   48 | `	void *pHandle;` |
|    - |   49 | `	int nLen;` |
|    - |   50 | `	sxi32 rc;` |
|   32 |   51 | `	if( nArg < 1 \|\| !ph7_value_is_string(apArg[0]) ){` |
|    - |   52 | `		/* Missing/Invalid arguments,return FALSE */` |
|  ! 0 |   53 | `		ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"Expecting a file path");` |
|  ! 0 |   54 | `		ph7_result_bool(pCtx,0);` |
|  ! 0 |   55 | `		return PH7_OK;` |
|    - |   56 | `	}` |
|    - |   57 | `	/* Extract the file path */` |
|   32 |   58 | `	zFile = ph7_value_to_string(apArg[0],&nLen);` |
|    - |   59 | `	/* Point to the target IO stream device */` |
|   32 |   60 | `	pStream = PH7_VmGetStreamDevice(pCtx->pVm,&zFile,nLen);` |
|   32 |   61 | `	if( pStream == 0 ){` |
|  ! 0 |   62 | `		ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"No such stream device,PH7 is returning FALSE");` |
|  ! 0 |   63 | `		ph7_result_bool(pCtx,0);` |
|  ! 0 |   64 | `		return PH7_OK;` |
|    - |   65 | `	}` |
|    - |   66 | `	/* Create an in-memory archive */` |
|   32 |   67 | `	pArchive = (SyArchive *)ph7_context_alloc_chunk(pCtx,sizeof(SyArchive)+sizeof(zip_raw_data),TRUE,FALSE);` |
|   32 |   68 | `	if( pArchive == 0 ){` |
|  ! 0 |   69 | `		ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"PH7 is running out of memory");` |
|  ! 0 |   70 | `		ph7_result_bool(pCtx,0);` |
|  ! 0 |   71 | `		return PH7_OK;` |
|    - |   72 | `	}` |
|   32 |   73 | `	pRaw = (zip_raw_data *)&pArchive[1];` |
|    - |   74 | `	/* Initialize the archive */` |
|   32 |   75 | `	SyArchiveInit(pArchive,&pCtx->pVm->sAllocator,0,0);` |
|    - |   76 | `	/* Extract the default stream */` |
|   32 |   77 | `	if( pStream == pCtx->pVm->pDefStream /* file:// stream*/){` |
|    - |   78 | `		const ph7_vfs *pVfs;` |
|    - |   79 | `		/* Try to get a memory view of the whole file since ZIP files` |
|    - |   80 | `		 * tends to be very big this days,this is a huge performance win.` |
|    - |   81 | `		 */` |
|   32 |   82 | `		pVfs = PH7_ExportBuiltinVfs();` |
|   32 |   83 | `		if( pVfs && pVfs->xMmap ){` |
|   32 |   84 | `			rc = pVfs->xMmap(zFile,&pRaw->raw.mmap.pMap,&pRaw->raw.mmap.nSize);` |
|   32 |   85 | `			if( rc == PH7_OK ){` |
|    - |   86 | `				/* Nice,Extract the whole archive */` |
|   30 |   87 | `				rc = SyZipExtractFromBuf(pArchive,(const char *)pRaw->raw.mmap.pMap,(sxu32)pRaw->raw.mmap.nSize);` |
|   30 |   88 | `				if( rc != SXRET_OK ){` |
|   15 |   89 | `					if( pVfs->xUnmap ){` |
|   15 |   90 | `						pVfs->xUnmap(pRaw->raw.mmap.pMap,pRaw->raw.mmap.nSize);` |
|    7 |   91 | `					}` |
|    - |   92 | `					/* Release the allocated chunk */` |
|   15 |   93 | `					ph7_context_free_chunk(pCtx,pArchive);` |
|    - |   94 | `					/* Something goes wrong with this ZIP archive,return FALSE */` |
|   15 |   95 | `					ph7_result_bool(pCtx,0);` |
|   15 |   96 | `					return PH7_OK;` |
|    - |   97 | `				}` |
|    - |   98 | `				/* Archive successfully opened */` |
|   16 |   99 | `				pRaw->iType = ZIP_RAW_DATA_MMAPED;` |
|   16 |  100 | `				pRaw->raw.mmap.pVfs = pVfs;` |
|   16 |  101 | `				goto success;` |
|    - |  102 | `			}` |
|    1 |  103 | `		}` |
|    - |  104 | `		/* FALL THROUGH */` |
|    1 |  105 | `	}` |
|    - |  106 | `	/* Try to open the file in read-only mode */` |
|    3 |  107 | `	pHandle = PH7_StreamOpenHandle(pCtx->pVm,pStream,zFile,PH7_IO_OPEN_RDONLY,FALSE,0,FALSE,0);` |
|    3 |  108 | `	if( pHandle == 0 ){` |
|    3 |  109 | `		ph7_context_throw_error_format(pCtx,PH7_CTX_ERR,"IO error while opening '%s'",zFile);` |
|    3 |  110 | `		ph7_result_bool(pCtx,0);` |
|    3 |  111 | `		return PH7_OK;` |
|    - |  112 | `	}` |
|  ! 0 |  113 | `	pContents = &pRaw->raw.sBlob;` |
|  ! 0 |  114 | `	SyBlobInit(pContents,&pCtx->pVm->sAllocator);` |
|    - |  115 | `	/* Read the whole file */` |
|  ! 0 |  116 | `	PH7_StreamReadWholeFile(pHandle,pStream,pContents);` |
|    - |  117 | `	/* Assume an invalid ZIP file */` |
|  ! 0 |  118 | `	rc = SXERR_INVALID;` |
|  ! 0 |  119 | `	if( SyBlobLength(pContents) > 0 ){` |
|    - |  120 | `		/* Extract archive entries */` |
|  ! 0 |  121 | `		rc = SyZipExtractFromBuf(pArchive,(const char *)SyBlobData(pContents),SyBlobLength(pContents));` |
|  ! 0 |  122 | `	}` |
|  ! 0 |  123 | `	pRaw->iType = ZIP_RAW_DATA_MEMBUF;` |
|    - |  124 | `	/* Close the stream */` |
|  ! 0 |  125 | `	PH7_StreamCloseHandle(pStream,pHandle);` |
|  ! 0 |  126 | `	if( rc != SXRET_OK ){` |
|    - |  127 | `		/* Release the working buffer */` |
|  ! 0 |  128 | `		SyBlobRelease(pContents);` |
|    - |  129 | `		/* Release the allocated chunk */` |
|  ! 0 |  130 | `		ph7_context_free_chunk(pCtx,pArchive);` |
|    - |  131 | `		/* Something goes wrong with this ZIP archive,return FALSE */` |
|  ! 0 |  132 | `		ph7_result_bool(pCtx,0);` |
|  ! 0 |  133 | `		return PH7_OK;` |
|    - |  134 | `	}` |
|  ! 0 |  135 | `success:` |
|    - |  136 | `	/* Reset the loop cursor */` |
|   16 |  137 | `	SyArchiveResetLoopCursor(pArchive);` |
|    - |  138 | `	/* Return the in-memory archive as a resource handle */` |
|   16 |  139 | `	ph7_result_resource(pCtx,pArchive);` |
|   16 |  140 | `	return PH7_OK;` |
|   17 |  141 |  |
|    - |  142 | `/*` |
|    - |  143 | `  * void zip_close(resource $zip)` |
|    - |  144 | `  *  Close an in-memory ZIP archive.` |
|    - |  145 | `  * Parameters` |
|    - |  146 | `  *  $zip` |
|    - |  147 | `  *   A ZIP file previously opened with zip_open().` |
|    - |  148 | `  * Return` |
|    - |  149 | `  *  null.` |
|    - |  150 | `  */` |
|   14 |  151 | `PH7_PRIVATE int PH7_builtin_zip_close(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|    2 |  152 |  |
|    - |  153 | `	SyArchive *pArchive;` |
|    - |  154 | `	zip_raw_data *pRaw;` |
|   16 |  155 | `	if( nArg < 1 \|\| !ph7_value_is_resource(apArg[0]) ){` |
|    - |  156 | `		/* Missing/Invalid arguments */` |
|  ! 0 |  157 | `		ph7_context_throw_error(pCtx,PH7_CTX_ERR,"Expecting a ZIP archive");` |
|  ! 0 |  158 | `		return PH7_OK;` |
|    - |  159 | `	}` |
|    - |  160 | `	/* Point to the in-memory archive */` |
|   16 |  161 | `	pArchive = (SyArchive *)ph7_value_to_resource(apArg[0]);` |
|    - |  162 | `	/* Make sure we are dealing with a valid ZIP archive */` |
|   16 |  163 | `	if( SXARCH_INVALID(pArchive) ){` |
|  ! 0 |  164 | `		ph7_context_throw_error(pCtx,PH7_CTX_ERR,"Expecting a ZIP archive");` |
|  ! 0 |  165 | `		return PH7_OK;` |
|    - |  166 | `	}` |
|    - |  167 | `	/* Release the archive */` |
|   16 |  168 | `	SyArchiveRelease(pArchive);` |
|   16 |  169 | `	pRaw = (zip_raw_data *)&pArchive[1];` |
|   16 |  170 | `	if( pRaw->iType == ZIP_RAW_DATA_MEMBUF ){` |
|  ! 0 |  171 | `		SyBlobRelease(&pRaw->raw.sBlob);` |
|  ! 0 |  172 | `	}else{` |
|   16 |  173 | `		const ph7_vfs *pVfs = pRaw->raw.mmap.pVfs;` |
|   16 |  174 | `		if( pVfs->xUnmap ){` |
|    - |  175 | `			/* Unmap the memory view */` |
|   16 |  176 | `			pVfs->xUnmap(pRaw->raw.mmap.pMap,pRaw->raw.mmap.nSize);` |
|    7 |  177 | `		}` |
|    - |  178 | `	}` |
|    - |  179 | `	/* Release the memory chunk */` |
|   16 |  180 | `	ph7_context_free_chunk(pCtx,pArchive);` |
|   16 |  181 | `	return PH7_OK;` |
|    9 |  182 |  |
|    - |  183 | `/*` |
|    - |  184 | `  * mixed zip_read(resource $zip)` |
|    - |  185 | `  *  Reads the next entry from an in-memory ZIP archive.` |
|    - |  186 | `  * Parameters` |
|    - |  187 | `  *  $zip` |
|    - |  188 | `  *   A ZIP file previously opened with zip_open().` |
|    - |  189 | `  * Return` |
|    - |  190 | `  *  A directory entry resource for later use with the zip_entry_... functions` |
|    - |  191 | `  *  or FALSE if there are no more entries to read, or an error code if an error occurred.` |
|    - |  192 | `  */` |
|    8 |  193 | `PH7_PRIVATE int PH7_builtin_zip_read(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|    2 |  194 |  |
|   10 |  195 | `	SyArchiveEntry *pNext = 0; /* cc warning */` |
|    - |  196 | `	SyArchive *pArchive;` |
|    - |  197 | `	sxi32 rc;` |
|   10 |  198 | `	if( nArg < 1 \|\| !ph7_value_is_resource(apArg[0]) ){` |
|    - |  199 | `		/* Missing/Invalid arguments */` |
|  ! 0 |  200 | `		ph7_context_throw_error(pCtx,PH7_CTX_ERR,"Expecting a ZIP archive");` |
|    - |  201 | `		/* return FALSE */` |
|  ! 0 |  202 | `		ph7_result_bool(pCtx,0);` |
|  ! 0 |  203 | `		return PH7_OK;` |
|    - |  204 | `	}` |
|    - |  205 | `	/* Point to the in-memory archive */` |
|   10 |  206 | `	pArchive = (SyArchive *)ph7_value_to_resource(apArg[0]);` |
|    - |  207 | `	/* Make sure we are dealing with a valid ZIP archive */` |
|   10 |  208 | `	if( SXARCH_INVALID(pArchive) ){` |
|  ! 0 |  209 | `		ph7_context_throw_error(pCtx,PH7_CTX_ERR,"Expecting a ZIP archive");` |
|    - |  210 | `		/* return FALSE */` |
|  ! 0 |  211 | `		ph7_result_bool(pCtx,0);` |
|  ! 0 |  212 | `		return PH7_OK;` |
|    - |  213 | `	}` |
|    - |  214 | `	/* Extract the next entry */` |
|   10 |  215 | `	rc = SyArchiveGetNextEntry(pArchive,&pNext);` |
|   10 |  216 | `	if( rc != SXRET_OK ){` |
|    - |  217 | `		/* No more entries in the central directory,return FALSE */` |
|  ! 0 |  218 | `		ph7_result_bool(pCtx,0);` |
|  ! 0 |  219 | `	}else{` |
|    - |  220 | `		/* Return as a resource handle */` |
|   10 |  221 | `		ph7_result_resource(pCtx,pNext);` |
|    - |  222 | `		/* Point to the ZIP raw data */` |
|   10 |  223 | `		pNext->pUserData = (void *)&pArchive[1];` |
|    - |  224 | `	}` |
|   10 |  225 | `	return PH7_OK;` |
|    6 |  226 |  |
|    - |  227 | `/*` |
|    - |  228 | `  * bool zip_entry_open(resource $zip,resource $zip_entry[,string $mode ])` |
|    - |  229 | `  *  Open a directory entry for reading` |
|    - |  230 | `  * Parameters` |
|    - |  231 | `  *  $zip` |
|    - |  232 | `  *   A ZIP file previously opened with zip_open().` |
|    - |  233 | `  *  $zip_entry` |
|    - |  234 | `  *   A directory entry returned by zip_read().` |
|    - |  235 | `  * $mode` |
|    - |  236 | `  *   Not used` |
|    - |  237 | `  * Return` |
|    - |  238 | `  *  A directory entry resource for later use with the zip_entry_... functions` |
|    - |  239 | `  *  or FALSE if there are no more entries to read, or an error code if an error occurred.` |
|    - |  240 | `  */` |
|    2 |  241 | `PH7_PRIVATE int PH7_builtin_zip_entry_open(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|    1 |  242 |  |
|    - |  243 | `	SyArchiveEntry *pEntry;` |
|    - |  244 | `	SyArchive *pArchive;` |
|    3 |  245 | `	if( nArg < 2 \|\| !ph7_value_is_resource(apArg[0]) \|\| !ph7_value_is_resource(apArg[1]) ){` |
|    - |  246 | `		/* Missing/Invalid arguments */` |
|  ! 0 |  247 | `		ph7_context_throw_error(pCtx,PH7_CTX_ERR,"Expecting a ZIP archive");` |
|    - |  248 | `		/* return FALSE */` |
|  ! 0 |  249 | `		ph7_result_bool(pCtx,0);` |
|  ! 0 |  250 | `		return PH7_OK;` |
|    - |  251 | `	}` |
|    - |  252 | `	/* Point to the in-memory archive */` |
|    3 |  253 | `	pArchive = (SyArchive *)ph7_value_to_resource(apArg[0]);` |
|    - |  254 | `	/* Make sure we are dealing with a valid ZIP archive */` |
|    3 |  255 | `	if( SXARCH_INVALID(pArchive) ){` |
|  ! 0 |  256 | `		ph7_context_throw_error(pCtx,PH7_CTX_ERR,"Expecting a ZIP archive");` |
|    - |  257 | `		/* return FALSE */` |
|  ! 0 |  258 | `		ph7_result_bool(pCtx,0);` |
|  ! 0 |  259 | `		return PH7_OK;` |
|    - |  260 | `	}` |
|    - |  261 | `	/* Make sure we are dealing with a valid ZIP archive entry */` |
|    3 |  262 | `	pEntry = (SyArchiveEntry *)ph7_value_to_resource(apArg[1]);` |
|    3 |  263 | `	if( SXARCH_ENTRY_INVALID(pEntry) ){` |
|  ! 0 |  264 | `		ph7_context_throw_error(pCtx,PH7_CTX_ERR,"Expecting a ZIP archive entry");` |
|    - |  265 | `		/* return FALSE */` |
|  ! 0 |  266 | `		ph7_result_bool(pCtx,0);` |
|  ! 0 |  267 | `		return PH7_OK;` |
|    - |  268 | `	}` |
|    - |  269 | `	/* All done. Actually this function is a no-op,return TRUE */` |
|    3 |  270 | `	ph7_result_bool(pCtx,1);` |
|    3 |  271 | `	return PH7_OK;` |
|    2 |  272 |  |
|    - |  273 | `/*` |
|    - |  274 | `  * bool zip_entry_close(resource $zip_entry)` |
|    - |  275 | `  *  Close a directory entry.` |
|    - |  276 | `  * Parameters` |
|    - |  277 | `  *  $zip_entry` |
|    - |  278 | `  *   A directory entry returned by zip_read().` |
|    - |  279 | `  * Return` |
|    - |  280 | `  *  Returns TRUE on success or FALSE on failure.` |
|    - |  281 | `  */` |
|    6 |  282 | `PH7_PRIVATE int PH7_builtin_zip_entry_close(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|    1 |  283 |  |
|    - |  284 | `	SyArchiveEntry *pEntry;` |
|    7 |  285 | `	if( nArg < 1 \|\| !ph7_value_is_resource(apArg[0]) ){` |
|    - |  286 | `		/* Missing/Invalid arguments */` |
|  ! 0 |  287 | `		ph7_context_throw_error(pCtx,PH7_CTX_ERR,"Expecting a ZIP archive entry");` |
|    - |  288 | `		/* return FALSE */` |
|  ! 0 |  289 | `		ph7_result_bool(pCtx,0);` |
|  ! 0 |  290 | `		return PH7_OK;` |
|    - |  291 | `	}` |
|    - |  292 | `	/* Make sure we are dealing with a valid ZIP archive entry */` |
|    7 |  293 | `	pEntry = (SyArchiveEntry *)ph7_value_to_resource(apArg[0]);` |
|    7 |  294 | `	if( SXARCH_ENTRY_INVALID(pEntry) ){` |
|  ! 0 |  295 | `		ph7_context_throw_error(pCtx,PH7_CTX_ERR,"Expecting a ZIP archive entry");` |
|    - |  296 | `		/* return FALSE */` |
|  ! 0 |  297 | `		ph7_result_bool(pCtx,0);` |
|  ! 0 |  298 | `		return PH7_OK;` |
|    - |  299 | `	}` |
|    - |  300 | `	/* Reset the read cursor */` |
|    7 |  301 | `	pEntry->nReadCount = 0;` |
|    - |  302 | `	/*All done. Actually this function is a no-op,return TRUE */` |
|    7 |  303 | `	ph7_result_bool(pCtx,1);` |
|    7 |  304 | `	return PH7_OK;` |
|    4 |  305 |  |
|    - |  306 | `/*` |
|    - |  307 | `  * string zip_entry_name(resource $zip_entry)` |
|    - |  308 | `  *  Retrieve the name of a directory entry.` |
|    - |  309 | `  * Parameters` |
|    - |  310 | `  *  $zip_entry` |
|    - |  311 | `  *   A directory entry returned by zip_read().` |
|    - |  312 | `  * Return` |
|    - |  313 | `  *  The name of the directory entry.` |
|    - |  314 | `  */` |
|    2 |  315 | `PH7_PRIVATE int PH7_builtin_zip_entry_name(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|    1 |  316 |  |
|    - |  317 | `	SyArchiveEntry *pEntry;` |
|    - |  318 | `	SyString *pName;` |
|    3 |  319 | `	if( nArg < 1 \|\| !ph7_value_is_resource(apArg[0]) ){` |
|    - |  320 | `		/* Missing/Invalid arguments */` |
|  ! 0 |  321 | `		ph7_context_throw_error(pCtx,PH7_CTX_ERR,"Expecting a ZIP archive entry");` |
|    - |  322 | `		/* return FALSE */` |
|  ! 0 |  323 | `		ph7_result_bool(pCtx,0);` |
|  ! 0 |  324 | `		return PH7_OK;` |
|    - |  325 | `	}` |
|    - |  326 | `	/* Make sure we are dealing with a valid ZIP archive entry */` |
|    3 |  327 | `	pEntry = (SyArchiveEntry *)ph7_value_to_resource(apArg[0]);` |
|    3 |  328 | `	if( SXARCH_ENTRY_INVALID(pEntry) ){` |
|  ! 0 |  329 | `		ph7_context_throw_error(pCtx,PH7_CTX_ERR,"Expecting a ZIP archive entry");` |
|    - |  330 | `		/* return FALSE */` |
|  ! 0 |  331 | `		ph7_result_bool(pCtx,0);` |
|  ! 0 |  332 | `		return PH7_OK;` |
|    - |  333 | `	}` |
|    - |  334 | `	/* Return entry name */` |
|    3 |  335 | `	pName = &pEntry->sFileName;` |
|    3 |  336 | `	ph7_result_string(pCtx,pName->zString,(int)pName->nByte);` |
|    3 |  337 | `	return PH7_OK;` |
|    2 |  338 |  |
|    - |  339 | `/*` |
|    - |  340 | `  * int64 zip_entry_filesize(resource $zip_entry)` |
|    - |  341 | `  *  Retrieve the actual file size of a directory entry.` |
|    - |  342 | `  * Parameters` |
|    - |  343 | `  *  $zip_entry` |
|    - |  344 | `  *   A directory entry returned by zip_read().` |
|    - |  345 | `  * Return` |
|    - |  346 | `  *  The size of the directory entry.` |
|    - |  347 | `  */` |
|    4 |  348 | `PH7_PRIVATE int PH7_builtin_zip_entry_filesize(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|    1 |  349 |  |
|    - |  350 | `	SyArchiveEntry *pEntry;` |
|    5 |  351 | `	if( nArg < 1 \|\| !ph7_value_is_resource(apArg[0]) ){` |
|    - |  352 | `		/* Missing/Invalid arguments */` |
|  ! 0 |  353 | `		ph7_context_throw_error(pCtx,PH7_CTX_ERR,"Expecting a ZIP archive entry");` |
|    - |  354 | `		/* return FALSE */` |
|  ! 0 |  355 | `		ph7_result_bool(pCtx,0);` |
|  ! 0 |  356 | `		return PH7_OK;` |
|    - |  357 | `	}` |
|    - |  358 | `	/* Make sure we are dealing with a valid ZIP archive entry */` |
|    5 |  359 | `	pEntry = (SyArchiveEntry *)ph7_value_to_resource(apArg[0]);` |
|    5 |  360 | `	if( SXARCH_ENTRY_INVALID(pEntry) ){` |
|  ! 0 |  361 | `		ph7_context_throw_error(pCtx,PH7_CTX_ERR,"Expecting a ZIP archive entry");` |
|    - |  362 | `		/* return FALSE */` |
|  ! 0 |  363 | `		ph7_result_bool(pCtx,0);` |
|  ! 0 |  364 | `		return PH7_OK;` |
|    - |  365 | `	}` |
|    - |  366 | `	/* Return entry size */` |
|    5 |  367 | `	ph7_result_int64(pCtx,(ph7_int64)pEntry->nByte);` |
|    5 |  368 | `	return PH7_OK;` |
|    3 |  369 |  |
|    - |  370 | `/*` |
|    - |  371 | `  * int64 zip_entry_compressedsize(resource $zip_entry)` |
|    - |  372 | `  *  Retrieve the compressed size of a directory entry.` |
|    - |  373 | `  * Parameters` |
|    - |  374 | `  *  $zip_entry` |
|    - |  375 | `  *   A directory entry returned by zip_read().` |
|    - |  376 | `  * Return` |
|    - |  377 | `  *  The compressed size.` |
|    - |  378 | `  */` |
|    2 |  379 | `PH7_PRIVATE int PH7_builtin_zip_entry_compressedsize(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|    1 |  380 |  |
|    - |  381 | `	SyArchiveEntry *pEntry;` |
|    3 |  382 | `	if( nArg < 1 \|\| !ph7_value_is_resource(apArg[0]) ){` |
|    - |  383 | `		/* Missing/Invalid arguments */` |
|  ! 0 |  384 | `		ph7_context_throw_error(pCtx,PH7_CTX_ERR,"Expecting a ZIP archive entry");` |
|    - |  385 | `		/* return FALSE */` |
|  ! 0 |  386 | `		ph7_result_bool(pCtx,0);` |
|  ! 0 |  387 | `		return PH7_OK;` |
|    - |  388 | `	}` |
|    - |  389 | `	/* Make sure we are dealing with a valid ZIP archive entry */` |
|    3 |  390 | `	pEntry = (SyArchiveEntry *)ph7_value_to_resource(apArg[0]);` |
|    3 |  391 | `	if( SXARCH_ENTRY_INVALID(pEntry) ){` |
|  ! 0 |  392 | `		ph7_context_throw_error(pCtx,PH7_CTX_ERR,"Expecting a ZIP archive entry");` |
|    - |  393 | `		/* return FALSE */` |
|  ! 0 |  394 | `		ph7_result_bool(pCtx,0);` |
|  ! 0 |  395 | `		return PH7_OK;` |
|    - |  396 | `	}` |
|    - |  397 | `	/* Return entry compressed size */` |
|    3 |  398 | `	ph7_result_int64(pCtx,(ph7_int64)pEntry->nByteCompr);` |
|    3 |  399 | `	return PH7_OK;` |
|    2 |  400 |  |
|    - |  401 | `/*` |
|    - |  402 | `  * string zip_entry_read(resource $zip_entry[,int $length])` |
|    - |  403 | `  *  Reads from an open directory entry.` |
|    - |  404 | `  * Parameters` |
|    - |  405 | `  *  $zip_entry` |
|    - |  406 | `  *   A directory entry returned by zip_read().` |
|    - |  407 | `  *  $length` |
|    - |  408 | `  *   The number of bytes to return. If not specified, this function` |
|    - |  409 | `  *   will attempt to read 1024 bytes.` |
|    - |  410 | `  * Return` |
|    - |  411 | `  *  Returns the data read, or FALSE if the end of the file is reached.` |
|    - |  412 | `  */` |
|    2 |  413 | `PH7_PRIVATE int PH7_builtin_zip_entry_read(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|    1 |  414 |  |
|    - |  415 | `	SyArchiveEntry *pEntry;` |
|    - |  416 | `	zip_raw_data *pRaw;` |
|    - |  417 | `	const char *zData;` |
|    - |  418 | `	int iLength;` |
|    3 |  419 | `	if( nArg < 1 \|\| !ph7_value_is_resource(apArg[0]) ){` |
|    - |  420 | `		/* Missing/Invalid arguments */` |
|  ! 0 |  421 | `		ph7_context_throw_error(pCtx,PH7_CTX_ERR,"Expecting a ZIP archive entry");` |
|    - |  422 | `		/* return FALSE */` |
|  ! 0 |  423 | `		ph7_result_bool(pCtx,0);` |
|  ! 0 |  424 | `		return PH7_OK;` |
|    - |  425 | `	}` |
|    - |  426 | `	/* Make sure we are dealing with a valid ZIP archive entry */` |
|    3 |  427 | `	pEntry = (SyArchiveEntry *)ph7_value_to_resource(apArg[0]);` |
|    3 |  428 | `	if( SXARCH_ENTRY_INVALID(pEntry) ){` |
|  ! 0 |  429 | `		ph7_context_throw_error(pCtx,PH7_CTX_ERR,"Expecting a ZIP archive entry");` |
|    - |  430 | `		/* return FALSE */` |
|  ! 0 |  431 | `		ph7_result_bool(pCtx,0);` |
|  ! 0 |  432 | `		return PH7_OK;` |
|    - |  433 | `	}` |
|    3 |  434 | `	zData = 0;` |
|    3 |  435 | `	if( pEntry->nReadCount >= pEntry->nByteCompr ){` |
|    - |  436 | `		/* No more data to read,return FALSE */` |
|  ! 0 |  437 | `		ph7_result_bool(pCtx,0);` |
|  ! 0 |  438 | `		return PH7_OK;` |
|    - |  439 | `	}` |
|    - |  440 | `	/* Set a default read length */` |
|    3 |  441 | `	iLength = 1024;` |
|    3 |  442 | `	if( nArg > 1 ){` |
|    3 |  443 | `		iLength = ph7_value_to_int(apArg[1]);` |
|    3 |  444 | `		if( iLength < 1 ){` |
|  ! 0 |  445 | `			iLength = 1024;` |
|  ! 0 |  446 | `		}` |
|    1 |  447 | `	}` |
|    3 |  448 | `	if( (sxu32)iLength > pEntry->nByteCompr - pEntry->nReadCount ){` |
|  ! 0 |  449 | `		iLength = (int)(pEntry->nByteCompr - pEntry->nReadCount);` |
|  ! 0 |  450 | `	}` |
|    - |  451 | `	/* Return the entry contents */` |
|    3 |  452 | `	pRaw = (zip_raw_data *)pEntry->pUserData;` |
|    3 |  453 | `	if( pRaw->iType == ZIP_RAW_DATA_MEMBUF ){` |
|  ! 0 |  454 | `		zData = (const char *)SyBlobDataAt(&pRaw->raw.sBlob,(pEntry->nOfft+pEntry->nReadCount));` |
|  ! 0 |  455 | `	}else{` |
|    3 |  456 | `		const char *zMap = (const char *)pRaw->raw.mmap.pMap;` |
|    - |  457 | `		/* Memory mmaped chunk */` |
|    3 |  458 | `		zData = &zMap[pEntry->nOfft+pEntry->nReadCount];` |
|    - |  459 | `	}` |
|    - |  460 | `	/* Increment the read counter */` |
|    3 |  461 | `	pEntry->nReadCount += iLength;` |
|    - |  462 | `	/* Return the raw data */` |
|    3 |  463 | `	ph7_result_string(pCtx,zData,iLength);` |
|    3 |  464 | `	return PH7_OK;` |
|    2 |  465 |  |
|    - |  466 | `/*` |
|    - |  467 | `  * bool zip_entry_reset_read_cursor(resource $zip_entry)` |
|    - |  468 | `  *  Reset the read cursor of an open directory entry.` |
|    - |  469 | `  * Parameters` |
|    - |  470 | `  *  $zip_entry` |
|    - |  471 | `  *   A directory entry returned by zip_read().` |
|    - |  472 | `  * Return` |
|    - |  473 | `  *  TRUE on success,FALSE on failure.` |
|    - |  474 | `  * Note that this is a symisc eXtension.` |
|    - |  475 | `  */` |
|    6 |  476 | `PH7_PRIVATE int PH7_builtin_zip_entry_reset_read_cursor(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|    1 |  477 |  |
|    - |  478 | `	SyArchiveEntry *pEntry;` |
|    7 |  479 | `	if( nArg < 1 \|\| !ph7_value_is_resource(apArg[0]) ){` |
|    - |  480 | `		/* Missing/Invalid arguments */` |
|    5 |  481 | `		ph7_context_throw_error(pCtx,PH7_CTX_ERR,"Expecting a ZIP archive entry");` |
|    - |  482 | `		/* return FALSE */` |
|    5 |  483 | `		ph7_result_bool(pCtx,0);` |
|    5 |  484 | `		return PH7_OK;` |
|    - |  485 | `	}` |
|    - |  486 | `	/* Make sure we are dealing with a valid ZIP archive entry */` |
|    3 |  487 | `	pEntry = (SyArchiveEntry *)ph7_value_to_resource(apArg[0]);` |
|    3 |  488 | `	if( SXARCH_ENTRY_INVALID(pEntry) ){` |
|  ! 0 |  489 | `		ph7_context_throw_error(pCtx,PH7_CTX_ERR,"Expecting a ZIP archive entry");` |
|    - |  490 | `		/* return FALSE */` |
|  ! 0 |  491 | `		ph7_result_bool(pCtx,0);` |
|  ! 0 |  492 | `		return PH7_OK;` |
|    - |  493 | `	}` |
|    - |  494 | `	/* Reset the cursor */` |
|    3 |  495 | `	pEntry->nReadCount = 0;` |
|    - |  496 | `	/* Return TRUE */` |
|    3 |  497 | `	ph7_result_bool(pCtx,1);` |
|    3 |  498 | `	return PH7_OK;` |
|    4 |  499 |  |
|    - |  500 | `/*` |
|    - |  501 | `  * string zip_entry_compressionmethod(resource $zip_entry)` |
|    - |  502 | `  *  Retrieve the compression method of a directory entry.` |
|    - |  503 | `  * Parameters` |
|    - |  504 | `  *  $zip_entry` |
|    - |  505 | `  *   A directory entry returned by zip_read().` |
|    - |  506 | `  * Return` |
|    - |  507 | `  *  The compression method on success or FALSE on failure.` |
|    - |  508 | `  */` |
|    2 |  509 | `PH7_PRIVATE int PH7_builtin_zip_entry_compressionmethod(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|    1 |  510 |  |
|    - |  511 | `	SyArchiveEntry *pEntry;` |
|    3 |  512 | `	if( nArg < 1 \|\| !ph7_value_is_resource(apArg[0]) ){` |
|    - |  513 | `		/* Missing/Invalid arguments */` |
|  ! 0 |  514 | `		ph7_context_throw_error(pCtx,PH7_CTX_ERR,"Expecting a ZIP archive entry");` |
|    - |  515 | `		/* return FALSE */` |
|  ! 0 |  516 | `		ph7_result_bool(pCtx,0);` |
|  ! 0 |  517 | `		return PH7_OK;` |
|    - |  518 | `	}` |
|    - |  519 | `	/* Make sure we are dealing with a valid ZIP archive entry */` |
|    3 |  520 | `	pEntry = (SyArchiveEntry *)ph7_value_to_resource(apArg[0]);` |
|    3 |  521 | `	if( SXARCH_ENTRY_INVALID(pEntry) ){` |
|  ! 0 |  522 | `		ph7_context_throw_error(pCtx,PH7_CTX_ERR,"Expecting a ZIP archive entry");` |
|    - |  523 | `		/* return FALSE */` |
|  ! 0 |  524 | `		ph7_result_bool(pCtx,0);` |
|  ! 0 |  525 | `		return PH7_OK;` |
|    - |  526 | `	}` |
|    3 |  527 | `	switch(pEntry->nComprMeth){` |
|    1 |  528 | `	case 0:` |
|    - |  529 | `		/* No compression;entry is stored */` |
|    3 |  530 | `		ph7_result_string(pCtx,"stored",(int)sizeof("stored")-1);` |
|    3 |  531 | `		break;` |
|  ! 0 |  532 | `	case 8:` |
|    - |  533 | `		/* Entry is deflated (Default compression algorithm)  */` |
|  ! 0 |  534 | `		ph7_result_string(pCtx,"deflate",(int)sizeof("deflate")-1);` |
|  ! 0 |  535 | `		break;` |
|    - |  536 | `		/* Exotic compression algorithms */` |
|  ! 0 |  537 | `	case 1:` |
|  ! 0 |  538 | `		ph7_result_string(pCtx,"shrunk",(int)sizeof("shrunk")-1);` |
|  ! 0 |  539 | `		break;` |
|  ! 0 |  540 | `	case 2:` |
|    - |  541 | `	case 3:` |
|    - |  542 | `	case 4:` |
|    - |  543 | `	case 5:` |
|    - |  544 | `		/* Entry is reduced */` |
|  ! 0 |  545 | `		ph7_result_string(pCtx,"reduced",(int)sizeof("reduced")-1);` |
|  ! 0 |  546 | `		break;` |
|  ! 0 |  547 | `	case 6:` |
|    - |  548 | `		/* Entry is imploded */` |
|  ! 0 |  549 | `		ph7_result_string(pCtx,"implode",(int)sizeof("implode")-1);` |
|  ! 0 |  550 | `		break;` |
|  ! 0 |  551 | `	default:` |
|  ! 0 |  552 | `		ph7_result_string(pCtx,"unknown",(int)sizeof("unknown")-1);` |
|  ! 0 |  553 | `		break;` |
|    - |  554 | `	}` |
|    3 |  555 | `	return PH7_OK;` |
|    2 |  556 |  |
|    - |  557 | `#endif /* PH7_DISABLE_BUILTIN_FUNC */` |
|    - |  558 |  |
