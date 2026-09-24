/**
 * SPDX-FileCopyrightText: 2011, 2012, 2013, 2014 Symisc Systems <licensing@symisc.net>
 * SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
 * SPDX-License-Identifier: BSD-3-Clause
 */
#include "ph7int.h"
#include <stdio.h>
#include <errno.h>
#include <string.h>

#ifdef __UNIXES__
#include <unistd.h>
#include <sys/wait.h>
#include <fcntl.h>
#include <signal.h>
#endif
#ifndef PH7_DISABLE_DISK_IO
/*
 * Section:
 *    IO stream implementation.
 * Status:
 *    Stable.
 */
/* Forward declaration */
static void ResetIOPrivate(io_private *pDev);
/*
 * Return the PHP resource-type name for a raw resource handle.
 * Every IO handle this VFS hands out (fopen/tmpfile/popen/opendir and the
 * STDIN/STDOUT/STDERR constants) is an io_private, which PHP reports as
 * "stream"; anything else is "Unknown". The magic probe mirrors the
 * IO_PRIVATE_INVALID check the rest of this file uses to validate handles.
 */
PH7_PRIVATE const char * PH7_VfsResourceType(void *pResource)
{
	io_private *pDev = (io_private *)pResource;
	if( !IO_PRIVATE_INVALID(pDev) ){
		return "stream";
	}
	return "Unknown";
}
/*
 * Return TRUE if the given resource handle is an io_private that has been closed
 * (fclose/closedir/pclose) yet kept alive so shared copies still observe it. php
 * reports such a value as gettype()=='resource (closed)' and is_resource()==false.
 * The magic probe mirrors IO_PRIVATE_INVALID and is safe on any resource handle:
 * every resource this engine hands out is a struct larger than an io_private, so
 * the iMagic slot is always in bounds and never equals the closed magic by chance.
 */
PH7_PRIVATE int PH7_VfsResourceIsClosed(void *pResource)
{
	io_private *pDev = (io_private *)pResource;
	return pDev != 0 && pDev->iMagic == IO_PRIVATE_CLOSED_MAGIC;
}
/*
 * bool ftruncate(resource $handle,int64 $size)
 *  Truncates a file to a given length.
 * Parameters
 *  $handle
 *   The file pointer.
 *   Note:
 *    The handle must be open for writing.
 * $size
 *   The size to truncate to.
 * Return
 *  TRUE on success or FALSE on failure.
 */
PH7_PRIVATE int PH7_builtin_ftruncate(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	const ph7_io_stream *pStream;
	io_private *pDev;
	ph7_int64 nSize;
	int rc;
	if( nArg < 2 || !ph7_value_is_resource(apArg[0]) ){
		/* Missing/Invalid arguments,return FALSE */
		ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"Expecting an IO handle");
		ph7_result_bool(pCtx,0);
		return PH7_OK;
	}
	/* Extract our private data */
	pDev = (io_private *)ph7_value_to_resource(apArg[0]);
	/* Make sure we are dealing with a valid io_private instance */
	if( IO_PRIVATE_INVALID(pDev) ){
		/*Expecting an IO handle */
		ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"Expecting an IO handle");
		ph7_result_bool(pCtx,0);
		return PH7_OK;
	}
	nSize = ph7_value_to_int64(apArg[1]);
	if( nSize < 0 ){
		/* php 8: catchable ValueError, raised BEFORE the unsupported-stream
		 * check (php-src orders the size check first). PHL used to truncate. */
		return PH7_VmThrowException(pCtx,"ValueError",
			"ftruncate(): Argument #2 ($size) must be greater than or equal to 0");
	}
	/* Point to the target IO stream device */
	pStream = pDev->pStream;
	if( pStream == 0  || pStream->xTrunc == 0){
		ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,
			"IO routine(%s) not implemented in the underlying stream(%s) device,PH7 is returning FALSE",
			ph7_function_name(pCtx),pStream ? pStream->zName : "null_stream"
			);
		ph7_result_bool(pCtx,0);
		return PH7_OK;
	}
	/* Perform the requested operation */
	rc = pStream->xTrunc(pDev->pHandle,nSize);
	if( rc == PH7_OK ){
		/* Discard buffered data */
		ResetIOPrivate(pDev);
	}
	/* IO result */
	ph7_result_bool(pCtx,rc == PH7_OK);
	return PH7_OK;
}
/*
 * int fseek(resource $handle,int $offset[,int $whence = SEEK_SET ])
 *  Seeks on a file pointer.
 * Parameters
 *  $handle
 *   A file system pointer resource that is typically created using fopen().
 * $offset
 *   The offset.
 *   To move to a position before the end-of-file, you need to pass a negative
 *   value in offset and set whence to SEEK_END.
 *   whence
 *   whence values are:
 *    SEEK_SET - Set position equal to offset bytes.
 *    SEEK_CUR - Set position to current location plus offset.
 *    SEEK_END - Set position to end-of-file plus offset.
 * Return
 *  0 on success,-1 on failure
 */
PH7_PRIVATE int PH7_builtin_fseek(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	const ph7_io_stream *pStream;
	io_private *pDev;
	ph7_int64 iOfft;
	int whence;
	int rc;
	if( nArg < 2 || !ph7_value_is_resource(apArg[0]) ){
		/* Missing/Invalid arguments,return FALSE */
		ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"Expecting an IO handle");
		ph7_result_int(pCtx,-1);
		return PH7_OK;
	}
	/* Extract our private data */
	pDev = (io_private *)ph7_value_to_resource(apArg[0]);
	/* Make sure we are dealing with a valid io_private instance */
	if( IO_PRIVATE_INVALID(pDev) ){
		/*Expecting an IO handle */
		ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"Expecting an IO handle");
		ph7_result_int(pCtx,-1);
		return PH7_OK;
	}
	/* Point to the target IO stream device */
	pStream = pDev->pStream;
	if( pStream == 0  || pStream->xSeek == 0){
		ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,
			"IO routine(%s) not implemented in the underlying stream(%s) device",
			ph7_function_name(pCtx),pStream ? pStream->zName : "null_stream"
			);
		ph7_result_int(pCtx,-1);
		return PH7_OK;
	}
	/* Extract the offset */
	iOfft = ph7_value_to_int64(apArg[1]);
	whence = 0;/* SEEK_SET */
	if( nArg > 2 ){
		/* Read whatever was passed, coercing like php's ZPP: gating this on
		 * ph7_value_is_int() left `fseek($f, 4, "99")` and `fseek($f, 4, 99.9)`
		 * falling back to whence 0, i.e. the very silent-SEEK_SET the check below
		 * exists to stop (and it disagreed with `99.0`, which is_int accepts). */
		whence = (int)ph7_value_to_int64(apArg[2]);
	}
	if( whence != 0 /* SEEK_SET */ && whence != 1 /* SEEK_CUR */ && whence != 2 /* SEEK_END */ ){
		/* php's php_stream_seek rejects an unknown whence and answers -1 WITHOUT
		 * moving the cursor. PHL passed the raw value through to the driver, where
		 * every non-CUR/END value fell into the SEEK_SET arm — so fseek($f, 0, 99)
		 * reported success (0) AND silently seeked to the start of the stream, two
		 * wrong answers from one unchecked argument. php raises no error here. */
		ph7_result_int(pCtx,-1);
		return PH7_OK;
	}
	if( whence == 1 /* SEEK_CUR */ ){
		/* The CURRENT position is the LOGICAL one: the device sits past the
		 * read-ahead the line readers buffer, so seek relative to where the
		 * SCRIPT is, not where the device is (StreamLogicalAdjust). Without
		 * this, fseek($f,2,SEEK_CUR) after an fgets() that buffered ahead
		 * skipped everything still sitting in the buffer. */
		iOfft -= (ph7_int64)(SyBlobLength(&pDev->sBuffer) - pDev->nOfft);
	}
	/* Perform the requested operation */
	rc = pStream->xSeek(pDev->pHandle,iOfft,whence);
	if( rc == PH7_OK ){
		/* Ignore buffered data */
		ResetIOPrivate(pDev);
	}
	/* IO result */
	ph7_result_int(pCtx,rc == PH7_OK ? 0 : - 1);
	return PH7_OK;
}
/*
 * int64 ftell(resource $handle)
 *  Returns the current position of the file read/write pointer.
 * Parameters
 *  $handle
 *   The file pointer.
 * Return
 *  Returns the position of the file pointer referenced by handle
 *  as an integer; i.e., its offset into the file stream.
 *  FALSE is returned on failure.
 */
PH7_PRIVATE int PH7_builtin_ftell(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	const ph7_io_stream *pStream;
	io_private *pDev;
	ph7_int64 iOfft;
	if( nArg < 1 || !ph7_value_is_resource(apArg[0]) ){
		/* Missing/Invalid arguments,return FALSE */
		ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"Expecting an IO handle");
		ph7_result_bool(pCtx,0);
		return PH7_OK;
	}
	/* Extract our private data */
	pDev = (io_private *)ph7_value_to_resource(apArg[0]);
	/* Make sure we are dealing with a valid io_private instance */
	if( IO_PRIVATE_INVALID(pDev) ){
		/*Expecting an IO handle */
		ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"Expecting an IO handle");
		ph7_result_bool(pCtx,0);
		return PH7_OK;
	}
	/* Point to the target IO stream device */
	pStream = pDev->pStream;
	if( pStream == 0  || pStream->xTell == 0){
		ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,
			"IO routine(%s) not implemented in the underlying stream(%s) device,PH7 is returning FALSE",
			ph7_function_name(pCtx),pStream ? pStream->zName : "null_stream"
			);
		ph7_result_bool(pCtx,0);
		return PH7_OK;
	}
	/* Perform the requested operation. The device sits past whatever the line
	 * readers buffered ahead, so the SCRIPT's position is the device position
	 * less the unconsumed remainder — ftell() after fgets("abcdefghij\nrest")
	 * is php's 11, not the 15 the device already read. */
	iOfft = pStream->xTell(pDev->pHandle);
	iOfft -= (ph7_int64)(SyBlobLength(&pDev->sBuffer) - pDev->nOfft);
	/* IO result */
	ph7_result_int64(pCtx,iOfft);
	return PH7_OK;
}
/*
 * bool rewind(resource $handle)
 *  Rewind the position of a file pointer.
 * Parameters
 *  $handle
 *   The file pointer.
 * Return
 *  TRUE on success or FALSE on failure.
 */
PH7_PRIVATE int PH7_builtin_rewind(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	const ph7_io_stream *pStream;
	io_private *pDev;
	int rc;
	if( nArg < 1 || !ph7_value_is_resource(apArg[0]) ){
		/* Missing/Invalid arguments,return FALSE */
		ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"Expecting an IO handle");
		ph7_result_bool(pCtx,0);
		return PH7_OK;
	}
	/* Extract our private data */
	pDev = (io_private *)ph7_value_to_resource(apArg[0]);
	/* Make sure we are dealing with a valid io_private instance */
	if( IO_PRIVATE_INVALID(pDev) ){
		/*Expecting an IO handle */
		ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"Expecting an IO handle");
		ph7_result_bool(pCtx,0);
		return PH7_OK;
	}
	/* Point to the target IO stream device */
	pStream = pDev->pStream;
	if( pStream == 0  || pStream->xSeek == 0){
		ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,
			"IO routine(%s) not implemented in the underlying stream(%s) device,PH7 is returning FALSE",
			ph7_function_name(pCtx),pStream ? pStream->zName : "null_stream"
			);
		ph7_result_bool(pCtx,0);
		return PH7_OK;
	}
	/* Perform the requested operation */
	rc = pStream->xSeek(pDev->pHandle,0,0/*SEEK_SET*/);
	if( rc == PH7_OK ){
		/* Ignore buffered data */
		ResetIOPrivate(pDev);
	}
	/* IO result */
	ph7_result_bool(pCtx,rc == PH7_OK);
	return PH7_OK;
}
/*
 * bool fflush(resource $handle)
 *  Flushes the output to a file.
 * Parameters
 *  $handle
 *   The file pointer.
 * Return
 *  TRUE on success or FALSE on failure.
 */
PH7_PRIVATE int PH7_builtin_fflush(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	const ph7_io_stream *pStream;
	io_private *pDev;
	int rc;
	if( nArg < 1 || !ph7_value_is_resource(apArg[0]) ){
		/* Missing/Invalid arguments,return FALSE */
		ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"Expecting an IO handle");
		ph7_result_bool(pCtx,0);
		return PH7_OK;
	}
	/* Extract our private data */
	pDev = (io_private *)ph7_value_to_resource(apArg[0]);
	/* Make sure we are dealing with a valid io_private instance */
	if( IO_PRIVATE_INVALID(pDev) ){
		/*Expecting an IO handle */
		ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"Expecting an IO handle");
		ph7_result_bool(pCtx,0);
		return PH7_OK;
	}
	/* Point to the target IO stream device */
	pStream = pDev->pStream;
	if( pStream == 0 || pStream->xSync == 0){
		ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,
			"IO routine(%s) not implemented in the underlying stream(%s) device,PH7 is returning FALSE",
			ph7_function_name(pCtx),pStream ? pStream->zName : "null_stream"
			);
		ph7_result_bool(pCtx,0);
		return PH7_OK;
	}
	/* Perform the requested operation */
	rc = pStream->xSync(pDev->pHandle);
	/* IO result */
	ph7_result_bool(pCtx,rc == PH7_OK);
	return PH7_OK;
}
/*
 * bool feof(resource $handle)
 *  Tests for end-of-file on a file pointer.
 * Parameters
 *  $handle
 *   The file pointer.
 * Return
 *  Returns TRUE if the file pointer is at EOF.FALSE otherwise
 */
PH7_PRIVATE int PH7_builtin_feof(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	const ph7_io_stream *pStream;
	io_private *pDev;
	int rc;
	if( nArg < 1 || !ph7_value_is_resource(apArg[0]) ){
		/* Missing/Invalid arguments */
		ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"Expecting an IO handle");
		ph7_result_bool(pCtx,1);
		return PH7_OK;
	}
	/* Extract our private data */
	pDev = (io_private *)ph7_value_to_resource(apArg[0]);
	/* Make sure we are dealing with a valid io_private instance */
	if( IO_PRIVATE_INVALID(pDev) ){
		/*Expecting an IO handle */
		ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"Expecting an IO handle");
		ph7_result_bool(pCtx,1);
		return PH7_OK;
	}
	/* Point to the target IO stream device */
	pStream = pDev->pStream;
	if( pStream == 0 ){
		ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,
			"IO routine(%s) not implemented in the underlying stream(%s) device,PH7 is returning FALSE",
			ph7_function_name(pCtx),pStream ? pStream->zName : "null_stream"
			);
		ph7_result_bool(pCtx,1);
		return PH7_OK;
	}
	rc = SXERR_EOF;
	/* Perform the requested operation */
	if( SyBlobLength(&pDev->sBuffer) > pDev->nOfft ){
		/* Data is available */
		rc = PH7_OK;
	}else{
		char zBuf[4096];
		ph7_int64 n;
		/* Perform a buffered read */
		n = pStream->xRead(pDev->pHandle,zBuf,sizeof(zBuf));
		if( n > 0 ){
			/* Copy buffered data */
			SyBlobAppend(&pDev->sBuffer,zBuf,(sxu32)n);
			rc = PH7_OK;
		}
	}
	/* EOF or not */
	ph7_result_bool(pCtx,rc == SXERR_EOF);
	return PH7_OK;
}
/*
 * Read n bytes from the underlying IO stream device.
 * Return total numbers of bytes readen on success. A number < 1 on failure
 * [i.e: IO error ] or EOF.
 *
 * This is the read every SCRIPT-level reader goes through, because it drains
 * the line readers' read-ahead buffer first: a stream that fgets() has already
 * pulled a block out of is positioned where the SCRIPT thinks it is, not where
 * the device is. Anything reading from a caller's handle has to use this and
 * not the device's own xRead.
 */
PH7_PRIVATE ph7_int64 PH7_StreamRead(io_private *pDev,void *pBuf,ph7_int64 nLen)
{
	const ph7_io_stream *pStream = pDev->pStream;
	char *zBuf = (char *)pBuf;
	ph7_int64 n,nRead;
	n = SyBlobLength(&pDev->sBuffer) - pDev->nOfft;
	if( n > 0 ){
		if( n > nLen ){
			n = nLen;
		}
		/* Copy the buffered data */
		SyMemcpy(SyBlobDataAt(&pDev->sBuffer,pDev->nOfft),pBuf,(sxu32)n);
		/* Update the read offset */
		pDev->nOfft += (sxu32)n;
		if( pDev->nOfft >= SyBlobLength(&pDev->sBuffer) ){
			/* Reset the working buffer so that we avoid excessive memory allocation */
			SyBlobReset(&pDev->sBuffer);
			pDev->nOfft = 0;
		}
		nLen -= n;
		if( nLen < 1 ){
			/* All done */
			return n;
		}
		/* Advance the cursor */
		zBuf += n;
	}
	/* Read without buffering */
	nRead = pStream->xRead(pDev->pHandle,zBuf,nLen);
	if( nRead > 0 ){
		n += nRead;
	}else if( n < 1 ){
		/* EOF or IO error */
		return nRead;
	}
	return n;
}
/*
 * Extract a single line from the buffered input.
 */
static sxi32 GetLine(io_private *pDev,ph7_int64 *pLen,const char **pzLine)
{
	const char *zIn,*zEnd,*zPtr;
	zIn = (const char *)SyBlobDataAt(&pDev->sBuffer,pDev->nOfft);
	zEnd = &zIn[SyBlobLength(&pDev->sBuffer)-pDev->nOfft];
	zPtr = zIn;
	while( zIn < zEnd ){
		if( zIn[0] == '\n' ){
			/* Line found */
			zIn++; /* Include the line ending as requested by the PHP specification */
			*pLen = (ph7_int64)(zIn-zPtr);
			*pzLine = zPtr;
			return SXRET_OK;
		}
		zIn++;
	}
	/* No line were found */
	return SXERR_NOTFOUND;
}
/*
 * Read a single line from the underlying IO stream device.
 */
static ph7_int64 StreamReadLine(io_private *pDev,const char **pzData,ph7_int64 nMaxLen)
{
	const ph7_io_stream *pStream = pDev->pStream;
	char zBuf[8192];
	ph7_int64 n;
	sxi32 rc;
	n = 0;
	if( pDev->nOfft >= SyBlobLength(&pDev->sBuffer) ){
		/* Reset the working buffer so that we avoid excessive memory allocation */
		SyBlobReset(&pDev->sBuffer);
		pDev->nOfft = 0;
	}
	if( SyBlobLength(&pDev->sBuffer) > pDev->nOfft ){
		/* Check if there is a line */
		rc = GetLine(pDev,&n,pzData);
		if( rc == SXRET_OK ){
			/* Got line. php caps it at nMaxLen bytes even when the newline
			 * falls later; the remainder (incl. the newline) stays buffered. */
			if( nMaxLen > 0 && n > nMaxLen ){
				n = nMaxLen;
			}
			pDev->nOfft += (sxu32)n;
			return n;
		}
	}
	/* Perform the read operation until a new line is extracted or length
	 * limit is reached.
	 */
	for(;;){
		n = pStream->xRead(pDev->pHandle,zBuf, (nMaxLen > 0 && nMaxLen < (ph7_int64)sizeof(zBuf)) ? nMaxLen : (ph7_int64)sizeof(zBuf));
		if( n < 1 ){
			/* EOF or IO error */
			break;
		}
		/* Append the data just read */
		SyBlobAppend(&pDev->sBuffer,zBuf,(sxu32)n);
		/* Try to extract a line */
		rc = GetLine(pDev,&n,pzData);
		if( rc == SXRET_OK ){
			/* Got one. Cap at nMaxLen (php's length limit); anything past the
			 * cap, newline included, is left buffered for the next read. */
			if( nMaxLen > 0 && n > nMaxLen ){
				n = nMaxLen;
			}
			pDev->nOfft += (sxu32)n;
			return n;
		}
		if( nMaxLen > 0 && (SyBlobLength(&pDev->sBuffer) - pDev->nOfft >= nMaxLen) ){
			/* Cap reached before a newline: return EXACTLY nMaxLen bytes (a prior
			 * sub-cap leftover plus this read can hold more) and keep the
			 * remainder buffered via nOfft for the next read, so we never hand
			 * back more than nMaxLen. The top-of-function check reclaims the
			 * buffer once it is fully consumed. */
			*pzData = (const char *)SyBlobDataAt(&pDev->sBuffer,pDev->nOfft);
			n = nMaxLen;
			pDev->nOfft += (sxu32)nMaxLen;
			return n;
		}
	}
	if( SyBlobLength(&pDev->sBuffer) > pDev->nOfft ){
		/* Read limit reached,return the available data */
		*pzData = (const char *)SyBlobDataAt(&pDev->sBuffer,pDev->nOfft);
		n = SyBlobLength(&pDev->sBuffer) - pDev->nOfft;
		/* Reset the working buffer */
		SyBlobReset(&pDev->sBuffer);
		pDev->nOfft = 0;
	}
	return n;
}
/*
 * Open an IO stream handle.
 * Notes on stream:
 * According to the PHP reference manual.
 * In its simplest definition, a stream is a resource object which exhibits streamable behavior.
 * That is, it can be read from or written to in a linear fashion, and may be able to fseek()
 * to an arbitrary locations within the stream.
 * A wrapper is additional code which tells the stream how to handle specific protocols/encodings.
 * For example, the http wrapper knows how to translate a URL into an HTTP/1.0 request for a file
 * on a remote server.
 * A stream is referenced as: scheme://target
 *   scheme(string) - The name of the wrapper to be used. Examples include: file, http...
 *   If no wrapper is specified, the function default is used (typically file://).
 *   target - Depends on the wrapper used. For filesystem related streams this is typically a path
 *  and filename of the desired file. For network related streams this is typically a hostname, often
 *  with a path appended.
 *
 * Note that PH7 IO streams looks like PHP streams but their implementation differ greately.
 * Please refer to the official documentation for a full discussion.
 * This function return a handle on success. Otherwise null.
 */
PH7_PRIVATE void * PH7_StreamOpenHandle(ph7_vm *pVm,const ph7_io_stream *pStream,const char *zFile,
	int iFlags,int use_include,ph7_value *pResource,int bPushInclude,int *pNew)
{
	void *pHandle = 0; /* cc warning */
	SyString sFile;
	ph7_value sDummy;
	int rc;
	if( pStream == 0 ){
		/* No such stream device */
		return 0;
	}
	if( pResource == 0 ){
		/* VM-dependent devices (php://, data://, tcp://, userland wrappers)
		 * reach the VM only through pResource->pVm — their xOpen has no vm
		 * parameter. Callers like file_get_contents pass no resource, so hand
		 * every device a synthesized stack value carrying the VM; xOpen only
		 * reads it during the call, and file:// ignores it. */
		PH7_MemObjInit(pVm,&sDummy);
		pResource = &sDummy;
	}
	SyStringInitFromBuf(&sFile,zFile,SyStrlen(zFile));
	if( use_include ){
		if(	sFile.zString[0] == '/' ||
#ifdef __WINNT__
			(sFile.nByte > 2 && sFile.zString[1] == ':' && (sFile.zString[2] == '\\' || sFile.zString[2] == '/') ) ||
#endif
			(sFile.nByte > 1 && sFile.zString[0] == '.' && sFile.zString[1] == '/') ||
			(sFile.nByte > 2 && sFile.zString[0] == '.' && sFile.zString[1] == '.' && sFile.zString[2] == '/') ){
				/*  Open the file directly */
				rc = pStream->xOpen(zFile,iFlags,pResource,&pHandle);
				if( rc == PH7_OK && bPushInclude ){
					/* Mark as included */
					PH7_VmPushFilePath(pVm,sFile.zString,sFile.nByte,FALSE,pNew);
				}
		}else{
			SyString *pPath;
			SyBlob sWorker;
#ifdef __WINNT__
			static const int c = '\\';
#else
			static const int c = '/';
#endif
			/* Init the path builder working buffer */
			SyBlobInit(&sWorker,&pVm->sAllocator);
			/* Build a path from the set of include path */
			SySetResetCursor(&pVm->aPaths);
			rc = SXERR_IO;
			while( SXRET_OK == SySetGetNextEntry(&pVm->aPaths,(void **)&pPath) ){
				/* Build full path */
				SyBlobFormat(&sWorker,"%z%c%z",pPath,c,&sFile);
				/* Append null terminator */
				if( SXRET_OK != SyBlobNullAppend(&sWorker) ){
					continue;
				}
				/* Try to open the file */
				rc = pStream->xOpen((const char *)SyBlobData(&sWorker),iFlags,pResource,&pHandle);
				if( rc == PH7_OK ){
					if( bPushInclude ){
						/* Mark as included */
						PH7_VmPushFilePath(pVm,(const char *)SyBlobData(&sWorker),SyBlobLength(&sWorker),FALSE,pNew);
					}
					break;
				}
				/* Reset the working buffer */
				SyBlobReset(&sWorker);
				/* Check the next path */
			}
			SyBlobRelease(&sWorker);
		}
	}else{
		/* Open the URI direcly */
		rc = pStream->xOpen(zFile,iFlags,pResource,&pHandle);
	}
	if( rc != PH7_OK ){
		/* IO error */
		return 0;
	}
	/* Return the file handle */
	return pHandle;
}
/*
 * Read the whole contents of an open IO stream handle [i.e local file/URL..]
 * Store the read data in the given BLOB (last argument).
 * The read operation is stopped when he hit the EOF or an IO error occurs.
 */
PH7_PRIVATE sxi32 PH7_StreamReadWholeFile(void *pHandle,const ph7_io_stream *pStream,SyBlob *pOut)
{
	ph7_int64 nRead;
	char zBuf[8192]; /* 8K */
	int rc;
	/* Perform the requested operation */
	for(;;){
		nRead = pStream->xRead(pHandle,zBuf,sizeof(zBuf));
		if( nRead < 1 ){
			/* EOF or IO error */
			break;
		}
		/* Append contents */
		rc = SyBlobAppend(pOut,zBuf,(sxu32)nRead);
		if( rc != SXRET_OK ){
			break;
		}
	}
	return SyBlobLength(pOut) > 0 ? SXRET_OK : -1;
}
/*
 * Close an open IO stream handle [i.e local file/URI..].
 */
PH7_PRIVATE void PH7_StreamCloseHandle(const ph7_io_stream *pStream,void *pHandle)
{
	if( pStream->xClose ){
		pStream->xClose(pHandle);
	}
}
/*
 * string fgetc(resource $handle)
 *  Gets a character from the given file pointer.
 * Parameters
 *  $handle
 *   The file pointer.
 * Return
 *  Returns a string containing a single character read from the file
 *  pointed to by handle. Returns FALSE on EOF.
 * WARNING
 *  This operation is extremely slow.Avoid using it.
 */
PH7_PRIVATE int PH7_builtin_fgetc(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	const ph7_io_stream *pStream;
	io_private *pDev;
	int c,n;
	if( nArg < 1 || !ph7_value_is_resource(apArg[0]) ){
		/* Missing/Invalid arguments,return FALSE */
		ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"Expecting an IO handle");
		ph7_result_bool(pCtx,0);
		return PH7_OK;
	}
	/* Extract our private data */
	pDev = (io_private *)ph7_value_to_resource(apArg[0]);
	/* Make sure we are dealing with a valid io_private instance */
	if( IO_PRIVATE_INVALID(pDev) ){
		/*Expecting an IO handle */
		ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"Expecting an IO handle");
		ph7_result_bool(pCtx,0);
		return PH7_OK;
	}
	/* Point to the target IO stream device */
	pStream = pDev->pStream;
	if( pStream == 0  ){
		ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,
			"IO routine(%s) not implemented in the underlying stream(%s) device,PH7 is returning FALSE",
			ph7_function_name(pCtx),pStream ? pStream->zName : "null_stream"
			);
		ph7_result_bool(pCtx,0);
		return PH7_OK;
	}
	/* Perform the requested operation */
	n = (int)PH7_StreamRead(pDev,(void *)&c,sizeof(char));
	/* IO result */
	if( n < 1 ){
		/* EOF or error,return FALSE */
		ph7_result_bool(pCtx,0);
	}else{
		/* Return the string holding the character */
		ph7_result_string(pCtx,(const char *)&c,sizeof(char));
	}
	return PH7_OK;
}
/*
 * string fgets(resource $handle[,int64 $length ])
 *  Gets line from file pointer.
 * Parameters
 *  $handle
 *   The file pointer.
 * $length
 *  Reading ends when length - 1 bytes have been read, on a newline
 *  (which is included in the return value), or on EOF (whichever comes first).
 *  If no length is specified, it will keep reading from the stream until it reaches
 *  the end of the line.
 * Return
 *  Returns a string of up to length - 1 bytes read from the file pointed to by handle.
 *  If there is no more data to read in the file pointer, then FALSE is returned.
 *  If an error occurs, FALSE is returned.
 */
PH7_PRIVATE int PH7_builtin_fgets(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	const ph7_io_stream *pStream;
	const char *zLine;
	io_private *pDev;
	ph7_int64 n,nLen;
	if( nArg < 1 || !ph7_value_is_resource(apArg[0]) ){
		/* Missing/Invalid arguments,return FALSE */
		ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"Expecting an IO handle");
		ph7_result_bool(pCtx,0);
		return PH7_OK;
	}
	/* Extract our private data */
	pDev = (io_private *)ph7_value_to_resource(apArg[0]);
	/* Make sure we are dealing with a valid io_private instance */
	if( IO_PRIVATE_INVALID(pDev) ){
		/*Expecting an IO handle */
		ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"Expecting an IO handle");
		ph7_result_bool(pCtx,0);
		return PH7_OK;
	}
	/* Point to the target IO stream device */
	pStream = pDev->pStream;
	if( pStream == 0  ){
		ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,
			"IO routine(%s) not implemented in the underlying stream(%s) device,PH7 is returning FALSE",
			ph7_function_name(pCtx),pStream ? pStream->zName : "null_stream"
			);
		ph7_result_bool(pCtx,0);
		return PH7_OK;
	}
	nLen = -1;
	if( nArg > 1 && !ph7_value_is_null(apArg[1]) ){
		/* Maximum data to read. PHP 8 raises a catchable ValueError for a
		 * non-positive length; a NULL (the ?int default) reads the whole line. */
		nLen = ph7_value_to_int64(apArg[1]);
		if( nLen < 1 ){
			return PH7_VmThrowException(pCtx,"ValueError",
				"fgets(): Argument #2 ($length) must be greater than 0");
		}
		/* php reads at most length-1 bytes -- one byte is reserved for the
		 * string terminator -- so a length of 1 reads nothing and returns
		 * false at any position, exactly like EOF. */
		nLen -= 1;
		if( nLen == 0 ){
			ph7_result_bool(pCtx,0);
			return PH7_OK;
		}
	}
	/* Perform the requested operation */
	n = StreamReadLine(pDev,&zLine,nLen);
	if( n < 1 ){
		/* EOF or IO error,return FALSE */
		ph7_result_bool(pCtx,0);
	}else{
		/* Return the freshly extracted line */
		ph7_result_string(pCtx,zLine,(int)n);
	}
	return PH7_OK;
}
/*
 * string|false stream_get_line(resource $stream, int $length, string $ending = "")
 *  Read a line from a stream, up to $length bytes or the FIRST occurrence of
 *  $ending, whichever comes first. Unlike fgets(), the ending is CONSUMED but
 *  never returned, and it may be any string.
 *  php's window rule (php_stream_get_record), pinned by probe: the ending
 *  counts only when it fits ENTIRELY inside the first $length bytes —
 *  stream_get_line($h,4,"--") over "abc--def" answers "abc-", the raw window,
 *  because the ending straddles its edge — and a capped read consumes no
 *  ending that starts at the boundary. $length 0 means php's 8192 default; at
 *  EOF the remainder is returned as-is, and false only when nothing is left.
 */
PH7_PRIVATE int PH7_builtin_stream_get_line(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	const ph7_io_stream *pStream;
	const char *zEnding = "";
	io_private *pDev;
	ph7_int64 nMaxLen;
	int nEndLen = 0;
	sxu32 iScanFrom = 0;
	int bEof = 0;
	if( nArg < 2 ){
		/* The central arity screen reports this; keep a refusal for a direct call. */
		ph7_result_bool(pCtx,0);
		return PH7_OK;
	}
	if( !ph7_value_is_resource(apArg[0]) ){
		return PH7_VmThrowException(pCtx,"TypeError",
			"stream_get_line(): Argument #1 ($stream) must be of type resource, %s given",
			ph7_type_name(apArg[0]));
	}
	pDev = (io_private *)ph7_value_to_resource(apArg[0]);
	if( IO_PRIVATE_INVALID(pDev) ){
		/* A closed or foreign resource is php's own TypeError, not a warning. */
		return PH7_VmThrowException(pCtx,"TypeError",
			"stream_get_line(): Argument #1 ($stream) must be an open stream resource");
	}
	pStream = pDev->pStream;
	if( pStream == 0 ){
		ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,
			"IO routine(%s) not implemented in the underlying stream(%s) device,PH7 is returning FALSE",
			ph7_function_name(pCtx),"null_stream"
			);
		ph7_result_bool(pCtx,0);
		return PH7_OK;
	}
	nMaxLen = ph7_value_to_int64(apArg[1]);
	if( nMaxLen < 0 ){
		return PH7_VmThrowException(pCtx,"ValueError",
			"stream_get_line(): Argument #2 ($length) must be greater than or equal to 0");
	}
	if( nMaxLen == 0 ){
		/* php's documented default window */
		nMaxLen = 8192;
	}
	if( nArg > 2 ){
		zEnding = ph7_value_to_string(apArg[2],&nEndLen);
	}
	if( pDev->nOfft >= SyBlobLength(&pDev->sBuffer) ){
		/* Reset the working buffer so that we avoid excessive memory allocation */
		SyBlobReset(&pDev->sBuffer);
		pDev->nOfft = 0;
	}
	/* Fill-and-scan: buffer chunks until the ending fits inside the window,
	 * the window itself fills, or the stream dries up. */
	for(;;){
		const char *zData = (const char *)SyBlobDataAt(&pDev->sBuffer,pDev->nOfft);
		sxu32 nAvail = SyBlobLength(&pDev->sBuffer) - pDev->nOfft;
		sxu32 nWindow = (nMaxLen < (ph7_int64)nAvail) ? (sxu32)nMaxLen : nAvail;
		ph7_int64 n;
		char zBuf[8192];
		if( nEndLen > 0 && (sxu32)nEndLen <= nWindow ){
			/* The ending must END inside the window to count. Resume the scan
			 * where the previous fill left off — a candidate can straddle two
			 * fills, so back up by the ending's length less one. */
			sxu32 i;
			for( i = iScanFrom ; i + (sxu32)nEndLen <= nWindow ; i++ ){
				if( zData[i] == zEnding[0] && SyMemcmp(&zData[i],zEnding,(sxu32)nEndLen) == 0 ){
					pDev->nOfft += i + (sxu32)nEndLen;
					ph7_result_string(pCtx,zData,(int)i);
					return PH7_OK;
				}
			}
			iScanFrom = i;
		}
		if( (ph7_int64)nAvail >= nMaxLen ){
			/* Window full with no ending inside it: hand the window back raw,
			 * anything past it (an ending included) stays buffered. */
			pDev->nOfft += (sxu32)nMaxLen;
			ph7_result_string(pCtx,zData,(int)nMaxLen);
			return PH7_OK;
		}
		if( bEof ){
			/* EOF: the remainder as-is, false when nothing is left. */
			if( nAvail > 0 ){
				pDev->nOfft += nAvail;
				ph7_result_string(pCtx,zData,(int)nAvail);
			}else{
				ph7_result_bool(pCtx,0);
			}
			return PH7_OK;
		}
		n = pStream->xRead(pDev->pHandle,zBuf,(ph7_int64)sizeof(zBuf));
		if( n < 1 ){
			bEof = 1;
			continue;
		}
		if( SXRET_OK != SyBlobAppend(&pDev->sBuffer,zBuf,(sxu32)n) ){
			return PH7_ContextMemoryError(pCtx);
		}
	}
}
/*
 * string fread(resource $handle,int64 $length)
 *  Binary-safe file read.
 * Parameters
 *  $handle
 *   The file pointer.
 * $length
 *  Up to length number of bytes read.
 * Return
 *  The data readen on success or FALSE on failure.
 */
PH7_PRIVATE int PH7_builtin_fread(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	const ph7_io_stream *pStream;
	io_private *pDev;
	ph7_int64 nRead;
	void *pBuf;
	int nLen;
	if( nArg < 1 || !ph7_value_is_resource(apArg[0]) ){
		/* Missing/Invalid arguments,return FALSE */
		ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"Expecting an IO handle");
		ph7_result_bool(pCtx,0);
		return PH7_OK;
	}
	/* Extract our private data */
	pDev = (io_private *)ph7_value_to_resource(apArg[0]);
	/* Make sure we are dealing with a valid io_private instance */
	if( IO_PRIVATE_INVALID(pDev) ){
		/*Expecting an IO handle */
		ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"Expecting an IO handle");
		ph7_result_bool(pCtx,0);
		return PH7_OK;
	}
	/* Point to the target IO stream device */
	pStream = pDev->pStream;
	if( pStream == 0  ){
		ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,
			"IO routine(%s) not implemented in the underlying stream(%s) device,PH7 is returning FALSE",
			ph7_function_name(pCtx),pStream ? pStream->zName : "null_stream"
			);
		ph7_result_bool(pCtx,0);
		return PH7_OK;
	}
        nLen = 4096;
	if( nArg > 1 ){
	  /* PHP 8 raises a catchable ValueError for a non-positive length; the
	   * $length parameter is non-nullable, so a NULL is rejected upstream by
	   * the central type screen (the recorded null-policy divergence). */
	  ph7_int64 nWant = ph7_value_to_int64(apArg[1]);
	  if( nWant < 1 ){
		return PH7_VmThrowException(pCtx,"ValueError",
			"fread(): Argument #2 ($length) must be greater than 0");
	  }
	  nLen = (int)nWant;
	  if( nLen < 1 ){
		/* A > INT_MAX length overflowed the int cast; read a single chunk
		 * (StreamRead returns only what the stream holds) instead of
		 * over-allocating -- matching the pre-existing behavior for lengths
		 * that do not fit an int. */
		nLen = 4096;
	  }
        }
	/* Allocate enough buffer */
	pBuf = ph7_context_alloc_chunk(pCtx,(unsigned int)nLen,FALSE,FALSE);
	if( pBuf == 0 ){
		ph7_context_throw_error(pCtx,PH7_CTX_ERR,"PH7 is running out of memory");
		ph7_result_bool(pCtx,0);
		return PH7_OK;
	}
	/* Perform the requested operation */
	nRead = PH7_StreamRead(pDev,pBuf,(ph7_int64)nLen);
	if( nRead < 0 ){
		/* An IO ERROR, which is php's only false here */
		ph7_result_bool(pCtx,0);
	}else{
		/* Make a copy of the data just read. Zero bytes is EOF, not a failure:
		 * php answers "" for it (php_stream_read returns 0 and the empty string
		 * rides through), where PHL answered FALSE — so the ordinary
		 * `while (!feof($f)) $buf .= fread($f, 8192);` loop ended on a value
		 * that means "the read failed" and a `=== false` guard fired at EOF. */
		ph7_result_string(pCtx,(const char *)pBuf,(int)nRead);
	}
	/* Release the buffer */
	ph7_context_free_chunk(pCtx,pBuf);
	return PH7_OK;
}
/*
 * array fgetcsv(resource $handle [, int $length = 0
 *         [,string $delimiter = ','[,string $enclosure = '"'[,string $escape='\\']]]])
 * Gets line from file pointer and parse for CSV fields.
 * Parameters
 * $handle
 *   The file pointer.
 * $length
 *  Reading ends when length - 1 bytes have been read, on a newline
 *  (which is included in the return value), or on EOF (whichever comes first).
 *  If no length is specified, it will keep reading from the stream until it reaches
 *  the end of the line.
 * $delimiter
 *   Set the field delimiter (one character only).
 * $enclosure
 *   Set the field enclosure character (one character only).
 * $escape
 *   Set the escape character (one character only). Defaults as a backslash (\)
 * Return
 *  Returns a string of up to length - 1 bytes read from the file pointed to by handle.
 *  If there is no more data to read in the file pointer, then FALSE is returned.
 *  If an error occurs, FALSE is returned.
 */
PH7_PRIVATE int PH7_builtin_fgetcsv(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	const ph7_io_stream *pStream;
	const char *zLine;
	io_private *pDev;
	ph7_int64 n,nLen;
	int delim  = ',';   /* Delimiter */
	int encl   = '"' ;  /* Enclosure */
	int escape = '\\';  /* Escape character */
	if( nArg < 1 || !ph7_value_is_resource(apArg[0]) ){
		/* Missing/Invalid arguments,return FALSE */
		ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"Expecting an IO handle");
		ph7_result_bool(pCtx,0);
		return PH7_OK;
	}
	/* Extract our private data */
	pDev = (io_private *)ph7_value_to_resource(apArg[0]);
	/* Make sure we are dealing with a valid io_private instance */
	if( IO_PRIVATE_INVALID(pDev) ){
		/*Expecting an IO handle */
		ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"Expecting an IO handle");
		ph7_result_bool(pCtx,0);
		return PH7_OK;
	}
	/* Point to the target IO stream device */
	pStream = pDev->pStream;
	if( pStream == 0  ){
		ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,
			"IO routine(%s) not implemented in the underlying stream(%s) device,PH7 is returning FALSE",
			ph7_function_name(pCtx),pStream ? pStream->zName : "null_stream"
			);
		ph7_result_bool(pCtx,0);
		return PH7_OK;
	}
	if( nArg > 2 ){
		/* php validates $separator/$enclosure/$escape BEFORE $length (probed
		 * ordering) and even when the stream is already at EOF. */
		sxi32 rc = PH7_CsvCharArg(pCtx,apArg[2],3,"separator",0,&delim);
		if( rc != PH7_OK ){
			return rc;
		}
		if( nArg > 3 ){
			rc = PH7_CsvCharArg(pCtx,apArg[3],4,"enclosure",0,&encl);
			if( rc != PH7_OK ){
				return rc;
			}
			if( nArg > 4 ){
				rc = PH7_CsvCharArg(pCtx,apArg[4],5,"escape",1,&escape);
				if( rc != PH7_OK ){
					return rc;
				}
			}
		}
	}
	nLen = -1;
	if( nArg > 1 && !ph7_value_is_null(apArg[1]) ){
		/* Maximum data to read. PHP 8 raises a catchable ValueError when the
		 * length is negative or hits PHP_INT_MAX (the valid range is
		 * 0..PHP_INT_MAX-1, where 0/NULL both mean "no limit"). */
		nLen = ph7_value_to_int64(apArg[1]);
		if( nLen < 0 || nLen >= SXI64_HIGH ){
			return PH7_VmThrowException(pCtx,"ValueError",
				"fgetcsv(): Argument #2 ($length) must be between 0 and 9223372036854775806");
		}
		/* 0 means "no limit", which StreamReadLine already treats as unlimited. */
	}
	/* Perform the requested operation */
	n = StreamReadLine(pDev,&zLine,nLen);
	if( n < 1 ){
		/* EOF or IO error,return FALSE */
		ph7_result_bool(pCtx,0);
	}else{
		ph7_value *pArray;
		SyBlob sRec;
		PH7_CsvScan sScan;
		/* Create our array */
		pArray = ph7_context_new_array(pCtx);
		if( pArray == 0 ){
			ph7_context_throw_error(pCtx,PH7_CTX_ERR,"PH7 is running out of memory");
			ph7_result_null(pCtx);
			return PH7_OK;
		}
		/* A RECORD is not a line: an enclosure that is still open when the line
		 * ends means the value contains the newline and the record continues on
		 * the next one. Parsing a single line and stopping split such a value
		 * across two rows, with the halves quoted wrong. The whole record is
		 * gathered FIRST and parsed once -- the scan below carries its position
		 * across the appends, so a stray quote costs one pass over the file
		 * rather than one per line. */
		SyBlobInit(&sRec,&pCtx->pVm->sAllocator);
		SyBlobAppend(&sRec,(const void *)zLine,(sxu32)n);
		PH7_CsvScanInit(&sScan);
		while( PH7_CsvScanOpen(&sScan,(const char *)SyBlobData(&sRec),
				SyBlobLength(&sRec),delim,encl,escape) ){
			if( SyBlobLength(&sRec) >= (sxu32)SXI32_HIGH ){
				/* The parser measures in int; stop rather than wrap negative. */
				break;
			}
			/* Continuation reads are NOT capped by $length: php's limit applies
			 * to the first read of the record, and reusing it here ended the
			 * record on a chunk boundary in the middle of a quoted value. */
			n = StreamReadLine(pDev,&zLine,0);
			if( n < 1 ){
				/* EOF inside the enclosure: php answers what it has. */
				break;
			}
			SyBlobAppend(&sRec,(const void *)zLine,(sxu32)n);
		}
		PH7_ProcessCsv(pArray,(const char *)SyBlobData(&sRec),
			(int)SyBlobLength(&sRec),delim,encl,escape,0);
		SyBlobRelease(&sRec);
		/* Return the freshly created array  */
		ph7_result_value(pCtx,pArray);
	}
	return PH7_OK;
}
/*
 * string fgetss(resource $handle [,int $length [,string $allowable_tags ]])
 *  Gets line from file pointer and strip HTML tags.
 * Parameters
 * $handle
 *   The file pointer.
 * $length
 *  Reading ends when length - 1 bytes have been read, on a newline
 *  (which is included in the return value), or on EOF (whichever comes first).
 *  If no length is specified, it will keep reading from the stream until it reaches
 *  the end of the line.
 * $allowable_tags
 *  You can use the optional second parameter to specify tags which should not be stripped.
 * Return
 *  Returns a string of up to length - 1 bytes read from the file pointed to by
 *  handle, with all HTML and PHP code stripped. If an error occurs, returns FALSE.
 */
PH7_PRIVATE int PH7_builtin_fgetss(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	const ph7_io_stream *pStream;
	const char *zLine;
	io_private *pDev;
	ph7_int64 n,nLen;
	if( nArg < 1 || !ph7_value_is_resource(apArg[0]) ){
		/* Missing/Invalid arguments,return FALSE */
		ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"Expecting an IO handle");
		ph7_result_bool(pCtx,0);
		return PH7_OK;
	}
	/* Extract our private data */
	pDev = (io_private *)ph7_value_to_resource(apArg[0]);
	/* Make sure we are dealing with a valid io_private instance */
	if( IO_PRIVATE_INVALID(pDev) ){
		/*Expecting an IO handle */
		ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"Expecting an IO handle");
		ph7_result_bool(pCtx,0);
		return PH7_OK;
	}
	/* Point to the target IO stream device */
	pStream = pDev->pStream;
	if( pStream == 0  ){
		ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,
			"IO routine(%s) not implemented in the underlying stream(%s) device,PH7 is returning FALSE",
			ph7_function_name(pCtx),pStream ? pStream->zName : "null_stream"
			);
		ph7_result_bool(pCtx,0);
		return PH7_OK;
	}
	nLen = -1;
	if( nArg > 1 ){
		/* Maximum data to read */
		nLen = ph7_value_to_int64(apArg[1]);
	}
	/* Perform the requested operation */
	n = StreamReadLine(pDev,&zLine,nLen);
	if( n < 1 ){
		/* EOF or IO error,return FALSE */
		ph7_result_bool(pCtx,0);
	}else{
		const char *zTaglist = 0;
		int nTaglen = 0;
		if( nArg > 2 && ph7_value_is_string(apArg[2]) ){
			/* Allowed tag */
			zTaglist = ph7_value_to_string(apArg[2],&nTaglen);
		}
		/* Process data just read */
		PH7_StripTagsFromString(pCtx,zLine,(int)n,zTaglist,nTaglen);
	}
	return PH7_OK;
}
/*
 * string readdir(resource $dir_handle)
 *   Read entry from directory handle.
 * Parameter
 *  $dir_handle
 *   The directory handle resource previously opened with opendir().
 * Return
 *  Returns the filename on success or FALSE on failure.
 */
PH7_PRIVATE int PH7_builtin_readdir(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	const ph7_io_stream *pStream;
	io_private *pDev;
	int rc;
	if( nArg < 1 || !ph7_value_is_resource(apArg[0]) ){
		/* Missing/Invalid arguments,return FALSE */
		ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"Expecting an IO handle");
		ph7_result_bool(pCtx,0);
		return PH7_OK;
	}
	/* Extract our private data */
	pDev = (io_private *)ph7_value_to_resource(apArg[0]);
	/* Make sure we are dealing with a valid io_private instance */
	if( IO_PRIVATE_INVALID(pDev) ){
		/*Expecting an IO handle */
		ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"Expecting an IO handle");
		ph7_result_bool(pCtx,0);
		return PH7_OK;
	}
	/* Point to the target IO stream device */
	pStream = pDev->pStream;
	if( pStream == 0  || pStream->xReadDir == 0 ){
		ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,
			"IO routine(%s) not implemented in the underlying stream(%s) device,PH7 is returning FALSE",
			ph7_function_name(pCtx),pStream ? pStream->zName : "null_stream"
			);
		ph7_result_bool(pCtx,0);
		return PH7_OK;
	}
	ph7_result_bool(pCtx,0);
	/* Perform the requested operation */
	rc = pStream->xReadDir(pDev->pHandle,pCtx);
	if( rc != PH7_OK ){
		/* Return FALSE */
		ph7_result_bool(pCtx,0);
	}
	return PH7_OK;
}
/*
 * void rewinddir(resource $dir_handle)
 *   Rewind directory handle.
 * Parameter
 *  $dir_handle
 *   The directory handle resource previously opened with opendir().
 * Return
 *  FALSE on failure.
 */
PH7_PRIVATE int PH7_builtin_rewinddir(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	const ph7_io_stream *pStream;
	io_private *pDev;
	if( nArg < 1 || !ph7_value_is_resource(apArg[0]) ){
		/* Missing/Invalid arguments,return FALSE */
		ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"Expecting an IO handle");
		ph7_result_bool(pCtx,0);
		return PH7_OK;
	}
	/* Extract our private data */
	pDev = (io_private *)ph7_value_to_resource(apArg[0]);
	/* Make sure we are dealing with a valid io_private instance */
	if( IO_PRIVATE_INVALID(pDev) ){
		/*Expecting an IO handle */
		ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"Expecting an IO handle");
		ph7_result_bool(pCtx,0);
		return PH7_OK;
	}
	/* Point to the target IO stream device */
	pStream = pDev->pStream;
	if( pStream == 0  || pStream->xRewindDir == 0 ){
		ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,
			"IO routine(%s) not implemented in the underlying stream(%s) device,PH7 is returning FALSE",
			ph7_function_name(pCtx),pStream ? pStream->zName : "null_stream"
			);
		ph7_result_bool(pCtx,0);
		return PH7_OK;
	}
	/* Perform the requested operation */
	pStream->xRewindDir(pDev->pHandle);
	return PH7_OK;
 }
/* Forward declaration */
static void ReleaseIOPrivate(ph7_context *pCtx,io_private *pDev);
/*
 * void closedir(resource $dir_handle)
 *   Close directory handle.
 * Parameter
 *  $dir_handle
 *   The directory handle resource previously opened with opendir().
 * Return
 *  FALSE on failure.
 */
PH7_PRIVATE int PH7_builtin_closedir(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	const ph7_io_stream *pStream;
	io_private *pDev;
	if( nArg < 1 || !ph7_value_is_resource(apArg[0]) ){
		/* Missing/Invalid arguments,return FALSE */
		ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"Expecting an IO handle");
		ph7_result_bool(pCtx,0);
		return PH7_OK;
	}
	/* Extract our private data */
	pDev = (io_private *)ph7_value_to_resource(apArg[0]);
	/* Make sure we are dealing with a valid io_private instance */
	if( IO_PRIVATE_INVALID(pDev) ){
		/*Expecting an IO handle */
		ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"Expecting an IO handle");
		ph7_result_bool(pCtx,0);
		return PH7_OK;
	}
	/* Point to the target IO stream device */
	pStream = pDev->pStream;
	if( pStream == 0  || pStream->xCloseDir == 0 ){
		ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,
			"IO routine(%s) not implemented in the underlying stream(%s) device,PH7 is returning FALSE",
			ph7_function_name(pCtx),pStream ? pStream->zName : "null_stream"
			);
		ph7_result_bool(pCtx,0);
		return PH7_OK;
	}
	/* Perform the requested operation */
	pStream->xCloseDir(pDev->pHandle);
	/* Keep the handle alive but flag it closed (php: gettype()=='resource (closed)') */
	MarkIOPrivateClosed(pDev);
	return PH7_OK;
 }
/*
 * resource opendir(string $path[,resource $context])
 *  Open directory handle.
 * Parameters
 * $path
 *   The directory path that is to be opened.
 * $context
 *   A context stream resource.
 * Return
 *  A directory handle resource on success,or FALSE on failure.
 */
PH7_PRIVATE int PH7_builtin_opendir(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	const ph7_io_stream *pStream;
	const char *zPath;
	io_private *pDev;
	int iLen,rc;
	if( nArg < 1 || !ph7_value_is_string(apArg[0]) ){
		/* Missing/Invalid arguments,return FALSE */
		ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"Expecting a directory path");
		ph7_result_bool(pCtx,0);
		return PH7_OK;
	}
	/* Extract the target path */
	zPath  = ph7_value_to_string(apArg[0],&iLen);
	/* Try to extract a stream */
	pStream = PH7_VmGetStreamDevice(pCtx->pVm,&zPath,iLen);
	if( pStream == 0 ){
		ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,
			"No stream device is associated with the given path(%s)",zPath);
		ph7_result_bool(pCtx,0);
		return PH7_OK;
	}
	if( pStream->xOpenDir == 0 ){
		ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,
			"IO routine(%s) not implemented in the underlying stream(%s) device",
			ph7_function_name(pCtx),pStream->zName
			);
		ph7_result_bool(pCtx,0);
		return PH7_OK;
	}
	/* Allocate a new IO private instance */
	pDev = (io_private *)ph7_context_alloc_chunk(pCtx,sizeof(io_private),TRUE,FALSE);
	if( pDev == 0 ){
		ph7_context_throw_error(pCtx,PH7_CTX_ERR,"PH7 is running out of memory");
		ph7_result_bool(pCtx,0);
		return PH7_OK;
	}
	/* Initialize the structure */
	InitIOPrivate(pCtx->pVm,pStream,pDev);
	/* Open the target directory */
	rc = pStream->xOpenDir(zPath,nArg > 1 ? apArg[1] : 0,&pDev->pHandle);
	if( rc != PH7_OK ){
		/* IO error: php WARNS here — `opendir(/nope): Failed to open directory: No
		 * such file or directory` — and PHL returned FALSE in silence. The message
		 * names the ACTIVE function, which is how dir() gets php's `dir(...)`
		 * wording out of the same call. */
		PH7_VmThrowWarningFmt(pCtx->pVm,"%s(%s): Failed to open directory: %s",
			ph7_function_name(pCtx),zPath,VfsStrerror(errno));
		ReleaseIOPrivate(pCtx,pDev);
		ph7_result_bool(pCtx,0);
	}else{
		/* Return the handle as a resource */
		ph7_result_resource(pCtx,pDev);
	}
	return PH7_OK;
}
/*
 * `dir(string $directory, $context = null): Directory|false`
 *
 * php's own dir() opens the stream and fills the object itself, which is why its
 * class needs no constructor. The open goes through the engine's opendir builtin
 * with THIS context, so the failure warning names `dir(...)` exactly as php's
 * does; a failed open is FALSE, where the chunk's version handed back a Directory
 * whose handle was `false`.
 */
PH7_PRIVATE int PH7_builtin_dir(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	ph7_class_instance *pObj;
	ph7_class *pClass;
	ph7_value *pRet;
	int rc;
	rc = PH7_builtin_opendir(pCtx,nArg,apArg);
	if( rc != PH7_OK ){
		return rc;
	}
	pRet = pCtx->pRet;
	if( pRet == 0 || (pRet->iFlags & MEMOBJ_RES) == 0 ){
		ph7_result_bool(pCtx,0);
		return PH7_OK;
	}
	pClass = PH7_VmExtractClass(pCtx->pVm,"Directory",sizeof("Directory")-1,FALSE,0);
	pObj = pClass ? PH7_NewClassInstance(pCtx->pVm,pClass) : 0;
	if( pObj == 0 ){
		ph7_result_bool(pCtx,0);
		return PH7_OK;
	}
	/* php's order: the path first, then the handle (var_dump shows both). */
	{
		int nPath = 0;
		const char *zPath = nArg > 0 ? ph7_value_to_string(apArg[0],&nPath) : "";
		PH7_NativeSetAttrStr(pCtx->pVm,pObj,"path",zPath,nPath);
	}
	PH7_NativeSetProp(pCtx->pVm,pObj,"handle",sizeof("handle")-1,pRet);
	PH7_NativeResultObject(pCtx,pObj);
	return PH7_OK;
}
/*
 * int readfile(string $filename[,bool $use_include_path = false [,resource $context ]])
 *  Reads a file and writes it to the output buffer.
 * Parameters
 *  $filename
 *   The filename being read.
 *  $use_include_path
 *   You can use the optional second parameter and set it to
 *   TRUE, if you want to search for the file in the include_path, too.
 *  $context
 *   A context stream resource.
 * Return
 *  The number of bytes read from the file on success or FALSE on failure.
 */
/*
 * php's IO failures are E_WARNINGs naming the function, the path and the system reason:
 *   file_get_contents(/nope): Failed to open stream: No such file or directory
 * PH7 raised an E_ERROR reading "func(): IO error while opening '/nope'" for every one of
 * them -- wrong severity, wrong text, and no reason. errno still holds the failing
 * syscall's code at this point (a successful call never clears it), which is where the
 * trailing reason comes from.
 */
PH7_PRIVATE int PH7_builtin_readfile(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	int use_include  = FALSE;
	const ph7_io_stream *pStream;
	ph7_int64 n,nRead;
	const char *zFile;
	char zBuf[8192];
	void *pHandle;
	int rc,nLen;
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
	if( nArg > 1 ){
		use_include = ph7_value_to_bool(apArg[1]);
	}
	/* Try to open the file in read-only mode */
	pHandle = PH7_StreamOpenHandle(pCtx->pVm,pStream,zFile,PH7_IO_OPEN_RDONLY,
		use_include,nArg > 2 ? apArg[2] : 0,FALSE,0);
	if( pHandle == 0 ){
		VfsThrowOpenWarning(pCtx,zFile);
		ph7_result_bool(pCtx,0);
		return PH7_OK;
	}
	/* Perform the requested operation */
	nRead = 0;
	for(;;){
		n = pStream->xRead(pHandle,zBuf,sizeof(zBuf));
		if( n < 1 ){
			/* EOF or IO error,break immediately */
			break;
		}
		/* Output data */
		rc = ph7_context_output(pCtx,zBuf,(int)n);
		if( rc == PH7_ABORT ){
			break;
		}
		/* Increment counter */
		nRead += n;
	}
	/* Close the stream */
	PH7_StreamCloseHandle(pStream,pHandle);
	/* Total number of bytes readen */
	ph7_result_int64(pCtx,nRead);
	return PH7_OK;
}
/*
 * string file_get_contents(string $filename[,bool $use_include_path = false
 *         [, resource $context [, int $offset = -1 [, int $maxlen ]]]])
 *  Reads entire file into a string.
 * Parameters
 *  $filename
 *   The filename being read.
 *  $use_include_path
 *   You can use the optional second parameter and set it to
 *   TRUE, if you want to search for the file in the include_path, too.
 *  $context
 *   A context stream resource.
 *  $offset
 *   The offset where the reading starts on the original stream.
 *  $maxlen
 *    Maximum length of data read. The default is to read until end of file
 *    is reached. Note that this parameter is applied to the stream processed by the filters.
 * Return
 *   The function returns the read data or FALSE on failure.
 */
PH7_PRIVATE int PH7_builtin_file_get_contents(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	const ph7_io_stream *pStream;
	ph7_int64 n,nRead,nMaxlen;
	int use_include  = FALSE;
	const char *zFile;
	char zBuf[8192];
	void *pHandle;
	int nLen;

	if( nArg < 1 || !ph7_value_is_string(apArg[0]) ){
		/* Missing/Invalid arguments,return FALSE */
		ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"Expecting a file path");
		ph7_result_bool(pCtx,0);
		return PH7_OK;
	}
	/* Extract the file path */
	zFile = ph7_value_to_string(apArg[0],&nLen);
	/* PHP 8 validates $length (arg #5) up front, before the wrapper is resolved
	 * or the file opened, so a negative length raises its catchable ValueError
	 * even for a bad wrapper or a missing file; a NULL (the ?int default) reads
	 * the whole file. */
	if( nArg > 4 && !ph7_value_is_null(apArg[4]) ){
		if( ph7_value_to_int64(apArg[4]) < 0 ){
			return PH7_VmThrowException(pCtx,"ValueError",
				"file_get_contents(): Argument #5 ($length) must be greater than or equal to 0");
		}
	}
	/* Point to the target IO stream device */
	pStream = PH7_VmGetStreamDevice(pCtx->pVm,&zFile,nLen);
	if( pStream == 0 ){
		ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"No such stream device,PH7 is returning FALSE");
		ph7_result_bool(pCtx,0);
		return PH7_OK;
	}
	nMaxlen = -1;
	if( nArg > 1 ){
		use_include = ph7_value_to_bool(apArg[1]);
	}
	/* Try to open the file in read-only mode */
	pHandle = PH7_StreamOpenHandle(pCtx->pVm,pStream,zFile,PH7_IO_OPEN_RDONLY,use_include,nArg > 2 ? apArg[2] : 0,FALSE,0);
	if( pHandle == 0 ){
		VfsThrowOpenWarning(pCtx,zFile);
		ph7_result_bool(pCtx,0);
		return PH7_OK;
	}
	if( nArg > 3 ){
		/* Extract the offset */
		n = ph7_value_to_int64(apArg[3]);
		if( n > 0 ){
			if( pStream->xSeek ){
				/* Seek to the desired offset */
				pStream->xSeek(pHandle,n,0/*SEEK_SET*/);
			}
		}
		if( nArg > 4 && !ph7_value_is_null(apArg[4]) ){
			/* Maximum data to read. An explicit 0 reads nothing (php returns
			 * ""); only an omitted or NULL length keeps the -1 "whole file"
			 * sentinel, so NULL must not collapse to 0 here. */
			nMaxlen = ph7_value_to_int64(apArg[4]);
		}
	}
	/* Perform the requested operation. nMaxlen: -1 = whole file, 0 = read
	 * nothing, >0 = at most that many bytes. The nMaxlen==0 case falls straight
	 * through to the empty-string result below. */
	nRead = 0;
	while( nMaxlen != 0 ){
		/* Cap the chunk to the bytes STILL wanted (nMaxlen - nRead), not to the
		 * whole nMaxlen: with a limit above the buffer size the final chunk would
		 * otherwise overshoot and append past $length. */
		ph7_int64 nAsk = (ph7_int64)sizeof(zBuf);
		if( nMaxlen > 0 && (nMaxlen - nRead) < nAsk ){
			nAsk = nMaxlen - nRead;
		}
		n = pStream->xRead(pHandle,zBuf,nAsk);
		if( n < 1 ){
			/* EOF or IO error,break immediately */
			break;
		}
		/* Append data */
		ph7_result_string(pCtx,zBuf,(int)n);
		/* Increment read counter */
		nRead += n;
		if( nMaxlen > 0 && nRead >= nMaxlen ){
			/* Read limit reached */
			break;
		}
	}
	/* Close the stream */
	PH7_StreamCloseHandle(pStream,pHandle);
	/* A successfully opened but empty file yields "" in php (FALSE is only for an
	 * open failure, handled above); the read loop never set a string result, so
	 * force an empty string rather than leaving a null/FALSE result. */
	if( ph7_context_result_buf_length(pCtx) < 1 ){
		ph7_result_string(pCtx,"",0);
	}
	return PH7_OK;
}
/*
 * int file_put_contents(string $filename,mixed $data[,int $flags = 0[,resource $context]])
 *  Write a string to a file.
 * Parameters
 *  $filename
 *  Path to the file where to write the data.
 * $data
 *  The data to write(Must be a string).
 * $flags
 *  The value of flags can be any combination of the following
 * flags, joined with the binary OR (|) operator.
 *   FILE_USE_INCLUDE_PATH 	Search for filename in the include directory. See include_path for more information.
 *   FILE_APPEND 	        If file filename already exists, append the data to the file instead of overwriting it.
 *   LOCK_EX 	            Acquire an exclusive lock on the file while proceeding to the writing.
 * context
 *  A context stream resource.
 * Return
 *  The function returns the number of bytes that were written to the file, or FALSE on failure.
 */
/*
 * Append a buffer to a file, creating it when absent, and raise php's open
 * warning (`f(/nope): Failed to open stream: …`) under the CALLING builtin's
 * name when it cannot be opened. Returns PH7_OK or -1.
 *
 * This is error_log()'s message_type 3, factored here because that is where the
 * stream device, the open flags and the warning shape already live.
 */
PH7_PRIVATE int PH7_VfsAppendFile(ph7_context *pCtx,const char *zFile,const void *pData,int nLen)
{
	const ph7_io_stream *pStream;
	void *pHandle;
	int nPath;
	if( zFile == 0 || zFile[0] == 0 ){
		VfsThrowOpenWarning(pCtx,zFile);
		return -1;
	}
	nPath = (int)SyStrlen(zFile);
	pStream = PH7_VmGetStreamDevice(pCtx->pVm,&zFile,nPath);
	if( pStream == 0 || pStream->xWrite == 0 ){
		VfsThrowOpenWarning(pCtx,zFile);
		return -1;
	}
	pHandle = PH7_StreamOpenHandle(pCtx->pVm,pStream,zFile,
		PH7_IO_OPEN_CREATE|PH7_IO_OPEN_RDWR|PH7_IO_OPEN_APPEND,FALSE,0,FALSE,0);
	if( pHandle == 0 ){
		VfsThrowOpenWarning(pCtx,zFile);
		return -1;
	}
	if( nLen > 0 && pStream->xWrite(pHandle,pData,nLen) < 0 ){
		PH7_StreamCloseHandle(pStream,pHandle);
		return -1;
	}
	PH7_StreamCloseHandle(pStream,pHandle);
	return PH7_OK;
}
PH7_PRIVATE int PH7_builtin_file_put_contents(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	int use_include  = FALSE;
	const ph7_io_stream *pStream;
	const char *zFile;
	const char *zData;
	int iOpenFlags;
	void *pHandle;
	int iFlags;
	int nLen;

	if( nArg < 2 || !ph7_value_is_string(apArg[0]) ){
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
	/* Data to write */
	zData = ph7_value_to_string(apArg[1],&nLen);
	/* Try to open the file in read-write mode */
	iOpenFlags = PH7_IO_OPEN_CREATE|PH7_IO_OPEN_RDWR|PH7_IO_OPEN_TRUNC;
	/* Extract the flags */
	iFlags = 0;
	if( nArg > 2 ){
		iFlags = ph7_value_to_int(apArg[2]);
		if( iFlags & 0x01 /*FILE_USE_INCLUDE_PATH*/){
			use_include = TRUE;
		}
		if( iFlags & 0x08 /* FILE_APPEND */){
			/* If the file already exists, append the data to the file
			 * instead of overwriting it.
			 */
			iOpenFlags &= ~PH7_IO_OPEN_TRUNC;
			/* Append mode */
			iOpenFlags |= PH7_IO_OPEN_APPEND;
		}
	}
	pHandle = PH7_StreamOpenHandle(pCtx->pVm,pStream,zFile,iOpenFlags,use_include,
		nArg > 3 ? apArg[3] : 0,FALSE,FALSE);
	if( pHandle == 0 ){
		VfsThrowOpenWarning(pCtx,zFile);
		ph7_result_bool(pCtx,0);
		return PH7_OK;
	}
	if( nLen < 1 ){
		/* Empty data, file is created/truncated */
		ph7_result_int64(pCtx,0);
		PH7_StreamCloseHandle(pStream,pHandle);
		return PH7_OK;
	}
	if( pStream->xWrite ){
		ph7_int64 n;
		if( (iFlags & 2/* LOCK_EX, php's value */) && pStream->xLock ){
			/* Try to acquire an exclusive lock */
			pStream->xLock(pHandle,1/* LOCK_EX */);
		}
		/* Perform the write operation */
		n = pStream->xWrite(pHandle,(const void *)zData,nLen);
		if( n < 0 ){
			/* IO error,return FALSE — with php's write-failure diagnostic. */
			ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,
				"Write of %d bytes failed with errno=%d %s",
				(int)nLen,errno,VfsStrerror(errno));
			ph7_result_bool(pCtx,0);
		}else{
			/* Total number of bytes written */
			ph7_result_int64(pCtx,n);
		}
	}else{
		/* Read-only stream */
		ph7_context_throw_error_format(pCtx,PH7_CTX_ERR,
			"Read-only stream(%s): Cannot perform write operation",
			pStream ? pStream->zName : "null_stream"
			);
		ph7_result_bool(pCtx,0);
	}
	/* Close the handle */
	PH7_StreamCloseHandle(pStream,pHandle);
	return PH7_OK;
}
/*
 * array file(string $filename[,int $flags = 0[,resource $context]])
 *  Reads entire file into an array.
 * Parameters
 *  $filename
 *   The filename being read.
 *  $flags
 *   The optional parameter flags can be one, or more, of the following constants:
 *   FILE_USE_INCLUDE_PATH
 *       Search for the file in the include_path.
 *   FILE_IGNORE_NEW_LINES
 *       Do not add newline at the end of each array element
 *   FILE_SKIP_EMPTY_LINES
 *       Skip empty lines
 *  $context
 *   A context stream resource.
 * Return
 *   The function returns the read data or FALSE on failure.
 */
PH7_PRIVATE int PH7_builtin_file(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	const char *zFile,*zPtr,*zEnd,*zBuf;
	ph7_value *pArray,*pLine;
	const ph7_io_stream *pStream;
	int use_include = 0;
	io_private *pDev;
	ph7_int64 n;
	int iFlags;
	int nLen;

	if( nArg < 1 || !ph7_value_is_string(apArg[0]) ){
		/* Missing/Invalid arguments,return FALSE */
		ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"Expecting a file path");
		ph7_result_bool(pCtx,0);
		return PH7_OK;
	}
	iFlags = 0;
	if( nArg > 1 ){
		/* php validates the mask FIRST — before the wrapper is resolved and before
		 * anything is allocated, so file("bogus://x", 8) is the ValueError and not a
		 * stream warning, and the throw cannot strand the io_private below (its chunk
		 * is not auto-released). file() accepts only USE_INCLUDE_PATH|IGNORE_NEW_LINES|
		 * SKIP_EMPTY_LINES|NO_DEFAULT_CONTEXT (1|2|4|16) — FILE_APPEND belongs to
		 * file_put_contents and is rejected here like any other stray bit. PHL masked
		 * the bits it knew and silently ignored the rest, so file($p, 8) and
		 * file($p, -1) read the file with a flag combination the caller never asked
		 * for. Read at 64-bit width so a high bit cannot be truncated into a valid
		 * mask. */
		ph7_int64 nFlags = ph7_value_to_int64(apArg[1]);
		if( nFlags & ~(ph7_int64)(0x01|0x02|0x04|0x10) ){
			return PH7_VmThrowException(pCtx,"ValueError",
				"file(): Argument #2 ($flags) must be a valid flag value");
		}
		iFlags = (int)nFlags;
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
	/* Allocate a new IO private instance */
	pDev = (io_private *)ph7_context_alloc_chunk(pCtx,sizeof(io_private),TRUE,FALSE);
	if( pDev == 0 ){
		ph7_context_throw_error(pCtx,PH7_CTX_ERR,"PH7 is running out of memory");
		ph7_result_bool(pCtx,0);
		return PH7_OK;
	}
	/* Initialize the structure */
	InitIOPrivate(pCtx->pVm,pStream,pDev);
	if( iFlags & 0x01 /*FILE_USE_INCLUDE_PATH*/ ){
		use_include = TRUE;
	}
	/* Create the array and the working value */
	pArray = ph7_context_new_array(pCtx);
	pLine = ph7_context_new_scalar(pCtx);
	if( pArray == 0 || pLine == 0 ){
		ph7_context_throw_error(pCtx,PH7_CTX_ERR,"PH7 is running out of memory");
		ph7_result_bool(pCtx,0);
		return PH7_OK;
	}
	/* Try to open the file in read-only mode */
	pDev->pHandle = PH7_StreamOpenHandle(pCtx->pVm,pStream,zFile,PH7_IO_OPEN_RDONLY,use_include,nArg > 2 ? apArg[2] : 0,FALSE,0);
	if( pDev->pHandle == 0 ){
		VfsThrowOpenWarning(pCtx,zFile);
		ph7_result_bool(pCtx,0);
		/* Don't worry about freeing memory, everything will be released automatically
		 * as soon we return from this function.
		 */
		return PH7_OK;
	}
	/* Perform the requested operation */
	for(;;){
		/* Try to extract a line */
		n = StreamReadLine(pDev,&zBuf,-1);
		if( n < 1 ){
			/* EOF or IO error */
			break;
		}
		/* Reset the cursor */
		ph7_value_reset_string_cursor(pLine);
		/* Remove line ending if requested by the caller */
		zPtr = zBuf;
		zEnd = &zBuf[n];
		if( iFlags & 0x02 /* FILE_IGNORE_NEW_LINES */ ){
			/* php strips ONE line ending: the LF, plus the CR that immediately
			 * precedes it. Not a run — "a\r\r\n" keeps its first CR and a
			 * CR-TERMINATED last line ("a\r", no LF) keeps it entirely. The
			 * platform-gated version this replaces left the CR on every CRLF line
			 * read on POSIX; a strip-all loop would instead eat data php keeps. */
			if( zEnd > zPtr && zEnd[-1] == '\n' ){
				n--;
				zEnd--;
				if( zEnd > zPtr && zEnd[-1] == '\r' ){
					n--;
					zEnd--;
				}
			}
		}
		if( iFlags & 0x04 /* FILE_SKIP_EMPTY_LINES */ ){
			/* php's "empty" is ZERO LENGTH after the optional newline strip — not
			 * "blank". PHL skipped any all-whitespace line, so a line of spaces was
			 * dropped where php keeps it, and without IGNORE_NEW_LINES a bare "\n"
			 * line (never zero-length, since the newline is still attached) was
			 * dropped too. Both are silent data loss from a read. */
			if( zEnd <= zPtr ){
				continue;
			}
		}
		ph7_value_string(pLine,zBuf,(int)(zEnd-zBuf));
		/* Insert line */
		ph7_array_add_elem(pArray,0/* Automatic index assign*/,pLine);
	}
	/* Close the stream */
	PH7_StreamCloseHandle(pStream,pDev->pHandle);
	/* Release the io_private instance */
	ReleaseIOPrivate(pCtx,pDev);
	/* Return the created array */
	ph7_result_value(pCtx,pArray);
	return PH7_OK;
}
/*
 * bool copy(string $source,string $dest[,resource $context ] )
 *  Makes a copy of the file source to dest.
 * Parameters
 *  $source
 *   Path to the source file.
 *  $dest
 *   The destination path. If dest is a URL, the copy operation
 *   may fail if the wrapper does not support overwriting of existing files.
 *  $context
 *   A context stream resource.
 * Return
 *  TRUE on success or FALSE on failure.
 */
PH7_PRIVATE int PH7_builtin_copy(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	const ph7_io_stream *pSin,*pSout;
	const char *zFile;
	char zBuf[8192];
	void *pIn,*pOut;
	ph7_int64 n;
	int nLen;
	if( nArg < 2 || !ph7_value_is_string(apArg[0]) || !ph7_value_is_string(apArg[1])){
		/* Missing/Invalid arguments,return FALSE */
		ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"Expecting a source and a destination path");
		ph7_result_bool(pCtx,0);
		return PH7_OK;
	}
	/* Extract the source name */
	zFile = ph7_value_to_string(apArg[0],&nLen);
	/* Point to the target IO stream device */
	pSin = PH7_VmGetStreamDevice(pCtx->pVm,&zFile,nLen);
	if( pSin == 0 ){
		ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"No such stream device,PH7 is returning FALSE");
		ph7_result_bool(pCtx,0);
		return PH7_OK;
	}
	/* Try to open the source file in a read-only mode */
	pIn = PH7_StreamOpenHandle(pCtx->pVm,pSin,zFile,PH7_IO_OPEN_RDONLY,FALSE,nArg > 2 ? apArg[2] : 0,FALSE,0);
	if( pIn == 0 ){
		VfsThrowOpenWarning(pCtx,zFile);
		ph7_result_bool(pCtx,0);
		return PH7_OK;
	}
	/* Extract the destination name */
	zFile = ph7_value_to_string(apArg[1],&nLen);
	/* Point to the target IO stream device */
	pSout = PH7_VmGetStreamDevice(pCtx->pVm,&zFile,nLen);
	if( pSout == 0 ){
		ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"No such stream device,PH7 is returning FALSE");
		ph7_result_bool(pCtx,0);
		PH7_StreamCloseHandle(pSin,pIn);
		return PH7_OK;
	}
	if( pSout->xWrite == 0 ){
		ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,
			"IO routine(%s) not implemented in the underlying stream(%s) device,PH7 is returning FALSE",
			ph7_function_name(pCtx),pSin->zName
			);
		ph7_result_bool(pCtx,0);
		PH7_StreamCloseHandle(pSin,pIn);
		return PH7_OK;
	}
	/* Try to open the destination file in a read-write mode */
	pOut = PH7_StreamOpenHandle(pCtx->pVm,pSout,zFile,
		PH7_IO_OPEN_CREATE|PH7_IO_OPEN_TRUNC|PH7_IO_OPEN_RDWR,FALSE,nArg > 2 ? apArg[2] : 0,FALSE,0);
	if( pOut == 0 ){
		VfsThrowOpenWarning(pCtx,zFile);
		ph7_result_bool(pCtx,0);
		PH7_StreamCloseHandle(pSin,pIn);
		return PH7_OK;
	}
	/* Perform the requested operation */
	for(;;){
		/* Read from source */
		n = pSin->xRead(pIn,zBuf,sizeof(zBuf));
		if( n < 1 ){
			/* EOF or IO error,break immediately */
			break;
		}
		/* Write to dest */
		n = pSout->xWrite(pOut,zBuf,n);
		if( n < 1 ){
			/* IO error,break immediately */
			break;
		}
	}
	/* Close the streams */
	PH7_StreamCloseHandle(pSin,pIn);
	PH7_StreamCloseHandle(pSout,pOut);
	/* Return TRUE */
	ph7_result_bool(pCtx,1);
	return PH7_OK;
}
/*
 * array fstat(resource $handle)
 *  Gets information about a file using an open file pointer.
 * Parameters
 *  $handle
 *   The file pointer.
 * Return
 *  Returns an array with the statistics of the file or FALSE on failure.
 */
PH7_PRIVATE int PH7_builtin_fstat(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	ph7_value *pArray,*pValue;
	const ph7_io_stream *pStream;
	io_private *pDev;
	if( nArg < 1 || !ph7_value_is_resource(apArg[0]) ){
		/* Missing/Invalid arguments,return FALSE */
		ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"Expecting an IO handle");
		ph7_result_bool(pCtx,0);
		return PH7_OK;
	}
	/* Extract our private data */
	pDev = (io_private *)ph7_value_to_resource(apArg[0]);
	/* Make sure we are dealing with a valid io_private instance */
	if( IO_PRIVATE_INVALID(pDev) ){
		/* Expecting an IO handle */
		ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"Expecting an IO handle");
		ph7_result_bool(pCtx,0);
		return PH7_OK;
	}
	/* Point to the target IO stream device */
	pStream = pDev->pStream;
	if( pStream == 0  || pStream->xStat == 0){
		ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,
			"IO routine(%s) not implemented in the underlying stream(%s) device,PH7 is returning FALSE",
			ph7_function_name(pCtx),pStream ? pStream->zName : "null_stream"
			);
		ph7_result_bool(pCtx,0);
		return PH7_OK;
	}
	/* Create the array and the working value */
	pArray = ph7_context_new_array(pCtx);
	pValue = ph7_context_new_scalar(pCtx);
	if( pArray == 0 || pValue == 0 ){
		ph7_context_throw_error(pCtx,PH7_CTX_ERR,"PH7 is running out of memory");
		ph7_result_bool(pCtx,0);
		return PH7_OK;
	}
	/* Perform the requested operation */
	pStream->xStat(pDev->pHandle,pArray,pValue);
	/* php answers the same thirteen fields twice -- numeric 0..12, then named
	 * (PH7_VfsStatDoubleUp); fstat() is stat()'s answer for an open handle and
	 * had the same missing half. */
	{
		ph7_value *pFull = ph7_context_new_array(pCtx);
		if( pFull && PH7_VfsStatDoubleUp(pArray,pFull) == PH7_OK ){
			ph7_result_value(pCtx,pFull);
			return PH7_OK;
		}
	}
	/* Return the freshly created array */
	ph7_result_value(pCtx,pArray);
	/* Don't worry about freeing memory here,everything will be
	 * released automatically as soon we return from this function.
	 */
	return PH7_OK;
}
/*
 * int fwrite(resource $handle,string $string[,int $length])
 *  Writes the contents of string to the file stream pointed to by handle.
 * Parameters
 *  $handle
 *   The file pointer.
 *  $string
 *   The string that is to be written.
 *  $length
 *   If the length argument is given, writing will stop after length bytes have been written
 *   or the end of string is reached, whichever comes first.
 * Return
 *  Returns the number of bytes written, or FALSE on error.
 */
PH7_PRIVATE int PH7_builtin_fwrite(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	const ph7_io_stream *pStream;
	const char *zString;
	io_private *pDev;
	int nLen,n;
	if( nArg < 2 || !ph7_value_is_resource(apArg[0]) ){
		/* Missing/Invalid arguments,return FALSE */
		ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"Expecting an IO handle");
		ph7_result_bool(pCtx,0);
		return PH7_OK;
	}
	/* Extract our private data */
	pDev = (io_private *)ph7_value_to_resource(apArg[0]);
	/* Make sure we are dealing with a valid io_private instance */
	if( IO_PRIVATE_INVALID(pDev) ){
		/* Expecting an IO handle */
		ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"Expecting an IO handle");
		ph7_result_bool(pCtx,0);
		return PH7_OK;
	}
	/* Point to the target IO stream device */
	pStream = pDev->pStream;
	if( pStream == 0  || pStream->xWrite == 0){
		ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,
			"IO routine(%s) not implemented in the underlying stream(%s) device,PH7 is returning FALSE",
			ph7_function_name(pCtx),pStream ? pStream->zName : "null_stream"
			);
		ph7_result_bool(pCtx,0);
		return PH7_OK;
	}
	/* Extract the data to write */
	zString = ph7_value_to_string(apArg[1],&nLen);
	if( nArg > 2 && !ph7_value_is_null(apArg[2]) ){
		/* Maximum data length to write, read at 64-bit width so a large limit
		 * (PHP_INT_MAX as "no limit" is a common idiom) does not truncate to a
		 * negative int. php 8: NULL means "no limit" and a NEGATIVE $length
		 * writes NOTHING and returns 0 (probed; PHL used to ignore a negative
		 * and write the whole string). */
		sxi64 nMax = ph7_value_to_int64(apArg[2]);
		if( nMax < 0 ){
			nLen = 0;
		}else if( nMax < (sxi64)nLen ){
			nLen = (int)nMax;
		}
	}
	if( nLen < 1 ){
		/* Nothing to write */
		ph7_result_int(pCtx,0);
		return PH7_OK;
	}
	if( pDev->nOfft < SyBlobLength(&pDev->sBuffer) && pStream->xSeek ){
		/* The device sits PAST the line readers' read-ahead: php writes at the
		 * LOGICAL position (fgets() then fwrite() overwrites what fgets left
		 * unread), so step the device back by the unconsumed remainder and
		 * drop the buffer — the ftell()/SEEK_CUR rule, applied to the write. */
		pStream->xSeek(pDev->pHandle,
			-(ph7_int64)(SyBlobLength(&pDev->sBuffer) - pDev->nOfft),1/*SEEK_CUR*/);
		ResetIOPrivate(pDev);
	}
	/* Perform the requested operation */
	n = (int)pStream->xWrite(pDev->pHandle,(const void *)zString,nLen);
	if( n <  0 ){
		/* IO error,return FALSE */
		ph7_result_bool(pCtx,0);
	}else{
		/* #Bytes written */
		ph7_result_int(pCtx,n);
	}
	return PH7_OK;
}
/*
 * Write flock()'s optional by-reference &$would_block out-param. php writes it on
 * every call that does not throw — including the ones that answer FALSE — so a
 * script can tell contention (1) from a plain failure (0).
 */
static void FlockStoreWouldBlock(ph7_context *pCtx,int nArg,ph7_value **apArg,int bWouldBlock)
{
	ph7_value sVal;
	if( nArg < 3 ){
		return;
	}
	PH7_MemObjInitFromInt(pCtx->pVm,&sVal,bWouldBlock ? 1 : 0);
	PH7_VmStoreArgByRef(pCtx->pVm,apArg[2],&sVal);
	PH7_MemObjRelease(&sVal);
}
/*
 * bool flock(resource $handle,int $operation[,int &$would_block])
 *  Portable advisory file locking.
 * Parameters
 *  $handle
 *   The file pointer.
 *  $operation
 *   operation is one of the following:
 *      LOCK_SH to acquire a shared lock (reader).
 *      LOCK_EX to acquire an exclusive lock (writer).
 *      LOCK_UN to release a lock (shared or exclusive).
 *   optionally OR'd with LOCK_NB to fail immediately instead of waiting.
 *  &$would_block
 *   Set to 1 when a LOCK_NB request was refused because another holder has the
 *   file, 0 otherwise. php writes it on every call that does not throw.
 * Return
 *  Returns TRUE on success or FALSE on failure.
 */
PH7_PRIVATE int PH7_builtin_flock(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	const ph7_io_stream *pStream;
	io_private *pDev;
	int nLock;
	int rc;
	if( nArg < 2 || !ph7_value_is_resource(apArg[0]) ){
		/* Missing/Invalid arguments,return FALSE */
		ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"Expecting an IO handle");
		ph7_result_bool(pCtx,0);
		return PH7_OK;
	}
	/* Extract our private data */
	pDev = (io_private *)ph7_value_to_resource(apArg[0]);
	/* Make sure we are dealing with a valid io_private instance */
	if( IO_PRIVATE_INVALID(pDev) ){
		/*Expecting an IO handle */
		ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"Expecting an IO handle");
		ph7_result_bool(pCtx,0);
		return PH7_OK;
	}
	/* Requested lock operation. php 8 validates it BEFORE the stream's lock
	 * support is considered: the low two bits select the action (its bison
	 * table is act = operation & 3), 0 is invalid, and every higher bit except
	 * LOCK_NB is ignored (flock($f,99) is LOCK_UN in php). */
	nLock = ph7_value_to_int(apArg[1]);
	if( (nLock & 3) == 0 ){
		return PH7_VmThrowException(pCtx,"ValueError",
			"flock(): Argument #2 ($operation) must be one of LOCK_SH, LOCK_EX, or LOCK_UN");
	}
	/* Point to the target IO stream device */
	pStream = pDev->pStream;
	if( pStream == 0  || pStream->xLock == 0){
		/* php returns FALSE silently when the stream does not support locking
		 * (php://memory & co) — no warning. It still writes $would_block. */
		FlockStoreWouldBlock(pCtx,nArg,apArg,0);
		ph7_result_bool(pCtx,0);
		return PH7_OK;
	}
	/*
	 * Translate php's operation (LOCK_SH=1, LOCK_EX=2, LOCK_UN=3, optionally
	 * |LOCK_NB=4) into the xLock() vtable value space documented in ph7.h.
	 */
	{
		int iOp = nLock & 3;
		int bNoBlock = (nLock & 4 /* LOCK_NB */) != 0;
		if( iOp == 3 /* LOCK_UN */ ){
			nLock = -1;
		}else if( iOp == 2 /* LOCK_EX */ ){
			nLock = bNoBlock ? PH7_IO_LOCK_EX_NB : PH7_IO_LOCK_EX;
		}else{
			nLock = bNoBlock ? PH7_IO_LOCK_SH_NB : PH7_IO_LOCK_SH;
		}
	}
	/* Lock operation */
	rc = pStream->xLock(pDev->pHandle,nLock);
	/* A refused non-blocking request is php's $would_block: FALSE, and the
	 * out-param tells the script it was contention rather than an IO error. */
	FlockStoreWouldBlock(pCtx,nArg,apArg,rc == SXERR_BUSY);
	/* IO result */
	ph7_result_bool(pCtx,rc == PH7_OK);
	return PH7_OK;
}
/*
 * int fpassthru(resource $handle)
 *  Output all remaining data on a file pointer.
 * Parameters
 *  $handle
 *   The file pointer.
 * Return
 *  Total number of characters read from handle and passed through
 *  to the output on success or FALSE on failure.
 */
PH7_PRIVATE int PH7_builtin_fpassthru(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	const ph7_io_stream *pStream;
	io_private *pDev;
	ph7_int64 n,nRead;
	char zBuf[8192];
	int rc;
	if( nArg < 1 || !ph7_value_is_resource(apArg[0]) ){
		/* Missing/Invalid arguments,return FALSE */
		ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"Expecting an IO handle");
		ph7_result_bool(pCtx,0);
		return PH7_OK;
	}
	/* Extract our private data */
	pDev = (io_private *)ph7_value_to_resource(apArg[0]);
	/* Make sure we are dealing with a valid io_private instance */
	if( IO_PRIVATE_INVALID(pDev) ){
		/*Expecting an IO handle */
		ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"Expecting an IO handle");
		ph7_result_bool(pCtx,0);
		return PH7_OK;
	}
	/* Point to the target IO stream device */
	pStream = pDev->pStream;
	if( pStream == 0  ){
		ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,
			"IO routine(%s) not implemented in the underlying stream(%s) device,PH7 is returning FALSE",
			ph7_function_name(pCtx),pStream ? pStream->zName : "null_stream"
			);
		ph7_result_bool(pCtx,0);
		return PH7_OK;
	}
	/* Perform the requested operation */
	nRead = 0;
	for(;;){
		n = PH7_StreamRead(pDev,zBuf,sizeof(zBuf));
		if( n < 1 ){
			/* Error or EOF */
			break;
		}
		/* Increment the read counter */
		nRead += n;
		/* Output the bytes THIS read produced. Handing the running total to
		 * ph7_context_output() instead read past the end of zBuf from the second
		 * chunk on (an out-of-bounds read) and wrote the overrun to the output:
		 * a 12000-byte file passed through as 20189 bytes of file-plus-garbage. */
		rc = ph7_context_output(pCtx,zBuf,(int)n);
		if( rc == PH7_ABORT ){
			/* Consumer callback request an operation abort */
			break;
		}
	}
	/* Total number of bytes readen */
	ph7_result_int64(pCtx,nRead);
	return PH7_OK;
}
/* CSV writer private data */
struct csv_data
{
	int delimiter;     /* Delimiter. Default ',' */
	int enclosure;     /* Enclosure. Default '"' */
	int escape;        /* Escape, or PH7_CSV_NO_ESCAPE when "" disabled it */
	SyBlob *pLine;     /* The line being built */
	sxu32 nCount;      /* Fields still to write after this one */
};
/*
 * The following callback is used by fputcsv() to walk the $fields array and
 * append each entry to the line under construction. It is a port of php's own
 * php_fputcsv (ext/standard/file.c), and the parts a re-derivation gets wrong
 * are all here:
 *  - WHICH fields are enclosed. php quotes a field containing the delimiter,
 *    the enclosure, the escape (when one is enabled) or any of \n, \r, \t and
 *    SPACE. PH7 tested the first two only, so a field with an embedded newline
 *    was written raw and became two CSV ROWS on the way back in.
 *  - HOW an embedded enclosure is written: doubled, unless the escape character
 *    came immediately before it (then the pair is passed through as-is and the
 *    escape does NOT arm again for the byte after).
 *  - that an EMPTY field is still a field. PH7 returned early for a zero-length
 *    value and skipped its delimiter with it, so `['', 'a']` wrote "a" -- one
 *    column where the caller wrote two, silently shifting every later column.
 * The delimiter goes BETWEEN fields, so it is written from the remaining count
 * rather than from a "not the first" flag: php appends it after every field but
 * the last, and an empty first field must still be followed by one.
 */
static int csv_write_callback(ph7_value *pKey,ph7_value *pValue,void *pUserData)
{
	struct csv_data *pData = (struct csv_data *)pUserData;
	const char *zData;
	int nLen,i;
	int bEnclose = 0;
	SXUNUSED(pKey); /* cc warning */
	zData = ph7_value_to_string(pValue,&nLen);
	for( i = 0 ; i < nLen ; ++i ){
		int c = (unsigned char)zData[i];
		if( c == pData->delimiter || c == pData->enclosure
		 || (pData->escape != PH7_CSV_NO_ESCAPE && c == pData->escape)
		 || c == '\n' || c == '\r' || c == '\t' || c == ' ' ){
			bEnclose = 1;
			break;
		}
	}
	if( bEnclose ){
		char cEnc = (char)pData->enclosure;
		int bEscaped = 0;
		SyBlobAppend(pData->pLine,(const void *)&cEnc,sizeof(char));
		for( i = 0 ; i < nLen ; ++i ){
			char c = zData[i];
			if( pData->escape != PH7_CSV_NO_ESCAPE && (unsigned char)c == pData->escape ){
				bEscaped = 1;
			}else if( !bEscaped && (unsigned char)c == pData->enclosure ){
				SyBlobAppend(pData->pLine,(const void *)&cEnc,sizeof(char));
			}else{
				bEscaped = 0;
			}
			SyBlobAppend(pData->pLine,(const void *)&c,sizeof(char));
		}
		SyBlobAppend(pData->pLine,(const void *)&cEnc,sizeof(char));
	}else if( nLen > 0 ){
		SyBlobAppend(pData->pLine,(const void *)zData,(sxu32)nLen);
	}
	if( pData->nCount > 0 ){
		pData->nCount--;
	}
	if( pData->nCount > 0 ){
		char cDel = (char)pData->delimiter;
		SyBlobAppend(pData->pLine,(const void *)&cDel,sizeof(char));
	}
	return PH7_OK;
}
/*
 * int|false fputcsv(resource $stream, array $fields, string $separator = ',',
 *                   string $enclosure = '"', string $escape = '\\',
 *                   string $eol = "\n")
 *  Format line as CSV and write to file pointer.
 * Parameters
 *  $stream
 *   Open file handle.
 *  $fields
 *   An array of values.
 *  $separator
 *   The optional separator parameter sets the field delimiter (one character only).
 *  $enclosure
 *   The optional enclosure parameter sets the field enclosure (one character only).
 *  $escape
 *   The escape character (one character), or "" to disable escaping entirely.
 *  $eol
 *   php 8.1's line ending. It is "\n" on EVERY platform -- php does not follow
 *   the host's convention here, and PHL used to write CRLF on Windows, so the
 *   same program produced a different FILE depending on where it ran.
 * Return
 *  The number of bytes written, or FALSE when the write fails. The count was
 *  missing entirely (the call answered NULL), so the documented
 *  `if (fputcsv(...) === false)` check never fired and a caller totalling the
 *  bytes it wrote added nothing.
 */
PH7_PRIVATE int PH7_builtin_fputcsv(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	const ph7_io_stream *pStream;
	struct csv_data sCsv;
	io_private *pDev;
	SyBlob sLine;
	const char *zEol = "\n";
	int nEol = 1;
	ph7_int64 nWr;
	if( nArg < 2 || !ph7_value_is_resource(apArg[0]) || !ph7_value_is_array(apArg[1]) ){
		/* Missing/Invalid arguments,return FALSE */
		ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"Missing/Invalid arguments");
		ph7_result_bool(pCtx,0);
		return PH7_OK;
	}
	/* Extract our private data */
	pDev = (io_private *)ph7_value_to_resource(apArg[0]);
	/* Make sure we are dealing with a valid io_private instance */
	if( IO_PRIVATE_INVALID(pDev) ){
		/*Expecting an IO handle */
		ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"Expecting an IO handle");
		ph7_result_bool(pCtx,0);
		return PH7_OK;
	}
	/* Point to the target IO stream device */
	pStream = pDev->pStream;
	if( pStream == 0  || pStream->xWrite == 0){
		ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,
			"IO routine(%s) not implemented in the underlying stream(%s) device,PH7 is returning FALSE",
			ph7_function_name(pCtx),pStream ? pStream->zName : "null_stream"
			);
		ph7_result_bool(pCtx,0);
		return PH7_OK;
	}
	/* Set default csv separator */
	sCsv.delimiter = ',';
	sCsv.enclosure = '"';
	sCsv.escape = '\\';
	if( nArg > 2 ){
		sxi32 rc = PH7_CsvCharArg(pCtx,apArg[2],3,"separator",0,&sCsv.delimiter);
		if( rc != PH7_OK ){
			return rc;
		}
		if( nArg > 3 ){
			rc = PH7_CsvCharArg(pCtx,apArg[3],4,"enclosure",0,&sCsv.enclosure);
			if( rc != PH7_OK ){
				return rc;
			}
			if( nArg > 4 ){
				rc = PH7_CsvCharArg(pCtx,apArg[4],5,"escape",1,&sCsv.escape);
				if( rc != PH7_OK ){
					return rc;
				}
				if( nArg > 5 ){
					/* $eol takes ANY string, the empty one included -- it is not
					 * a single-character argument like the three above. */
					zEol = ph7_value_to_string(apArg[5],&nEol);
				}
			}
		}
	}
	/* php builds the whole line first and writes it ONCE, which is what makes the
	 * byte count meaningful and keeps a partly-written row off the stream. */
	SyBlobInit(&sLine,&pCtx->pVm->sAllocator);
	sCsv.pLine = &sLine;
	sCsv.nCount = (sxu32)ph7_array_count(apArg[1]);
	ph7_array_walk(apArg[1],csv_write_callback,&sCsv);
	if( nEol > 0 ){
		SyBlobAppend(&sLine,(const void *)zEol,(sxu32)nEol);
	}
	if( pDev->nOfft < SyBlobLength(&pDev->sBuffer) && pStream->xSeek ){
		/* Write at the LOGICAL position, not the device one -- the same rule
		 * PH7_builtin_fwrite applies after a buffered read (fgets() then
		 * fputcsv() overwrites what fgets left unread). */
		pStream->xSeek(pDev->pHandle,
			-(ph7_int64)(SyBlobLength(&pDev->sBuffer) - pDev->nOfft),1/*SEEK_CUR*/);
		ResetIOPrivate(pDev);
	}
	nWr = pStream->xWrite(pDev->pHandle,(const void *)SyBlobData(&sLine),
		(ph7_int64)SyBlobLength(&sLine));
	SyBlobRelease(&sLine);
	if( nWr < 0 ){
		ph7_result_bool(pCtx,0);
	}else{
		ph7_result_int64(pCtx,nWr);
	}
	return PH7_OK;
}
/*
 * fprintf,vfprintf private data.
 * An instance of the following structure is passed to the formatted
 * input consumer callback defined below.
 */
typedef struct fprintf_data fprintf_data;
struct fprintf_data
{
	io_private *pIO;        /* IO stream */
	ph7_int64 nCount;       /* Total number of bytes written */
};
/*
 * Callback [i.e: Formatted input consumer] for the fprintf function.
 */
static int fprintfConsumer(ph7_context *pCtx,const char *zInput,int nLen,void *pUserData)
{
	fprintf_data *pFdata = (fprintf_data *)pUserData;
	ph7_int64 n;
	/* Write the formatted data */
	n = pFdata->pIO->pStream->xWrite(pFdata->pIO->pHandle,(const void *)zInput,nLen);
	if( n < 1 ){
		SXUNUSED(pCtx); /* cc warning */
		/* IO error,abort immediately */
		return SXERR_ABORT;
	}
	/* Increment counter */
	pFdata->nCount += n;
	return PH7_OK;
}
/*
 * int fprintf(resource $handle,string $format[,mixed $args [, mixed $... ]])
 *  Write a formatted string to a stream.
 * Parameters
 *  $handle
 *   The file pointer.
 *  $format
 *   String format (see sprintf()).
 * Return
 *  The length of the written string.
 */
PH7_PRIVATE int PH7_builtin_fprintf(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	fprintf_data sFdata;
	const char *zFormat;
	io_private *pDev;
	int nLen;
	if( nArg < 2 ){
		ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"Invalid arguments");
		ph7_result_int(pCtx,0);
		return PH7_OK;
	}
	{
		/* php: a non-resource $stream is a TypeError (#1), not a warn-and-return-0. */
		sxi32 rcs = PH7_CheckStreamArg(pCtx,apArg[0],1,"stream");
		if( rcs != PH7_OK ){
			return rcs;
		}
	}
	/* Extract our private data */
	pDev = (io_private *)ph7_value_to_resource(apArg[0]);
	/* Make sure we are dealing with a valid io_private instance */
	if( IO_PRIVATE_INVALID(pDev) ){
		/*Expecting an IO handle */
		ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"Expecting an IO handle");
		ph7_result_int(pCtx,0);
		return PH7_OK;
	}
	/* Point to the target IO stream device */
	if( pDev->pStream == 0  || pDev->pStream->xWrite == 0 ){
		ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,
			"IO routine(%s) not implemented in the underlying stream(%s) device",
			ph7_function_name(pCtx),pDev->pStream ? pDev->pStream->zName : "null_stream"
			);
		ph7_result_int(pCtx,0);
		return PH7_OK;
	}
	/* PHP 8: a non-string-coercible $format (array/object/resource) is a TypeError (#2). */
	{
		sxi32 rcf = PH7_FormatCheckFormatArg(pCtx,apArg[1],2);
		if( rcf != PH7_OK ){
			return rcf;
		}
	}
	/* Extract the string format (scalars/null coerce). */
	zFormat = ph7_value_to_string(apArg[1],&nLen);
	if( nLen < 1 ){
		/* Empty string,return zero */
		ph7_result_int(pCtx,0);
		return PH7_OK;
	}
	{
		/* PHP 8: too few value arguments is a catchable ArgumentCountError before output,
		 * and php runs this check BEFORE validating the specifiers. fprintf's values start
		 * at apArg[2] (apArg[0]=stream, apArg[1]=format). */
		sxi32 rcv = PH7_FormatCheckArgCount(pCtx,zFormat,nLen,nArg - 2,2,FALSE);
		if( rcv != PH7_OK ){
			return rcv;
		}
		/* PHP 8: an unknown or missing format specifier throws a catchable ValueError
		 * before any output; propagate the throw status verbatim. */
		rcv = PH7_FormatValidate(pCtx,zFormat,nLen);
		if( rcv != PH7_OK ){
			return rcv;
		}
	}
	/* Prepare our private data */
	sFdata.nCount = 0;
	sFdata.pIO = pDev;
	/* Format the string */
	{
	sxi32 rcv = PH7_InputFormat(fprintfConsumer,pCtx,zFormat,nLen,nArg - 1,&apArg[1],(void *)&sFdata,FALSE);
	/* Return total number of bytes written */
	ph7_result_int64(pCtx,sFdata.nCount);
	/* A %s argument that could not be coerced raised php's Error mid-format; the
	 * bytes still went to the stream, as php's do, so report the throw last. */
	if( rcv != SXRET_OK ){
		pCtx->nThrowRc = rcv;
		return rcv;
	}
	}
	return PH7_OK;
}
/*
 * int vfprintf(resource $handle,string $format,array $args)
 *  Write a formatted string to a stream.
 * Parameters
 *  $handle
 *   The file pointer.
 *  $format
 *   String format (see sprintf()).
 * $args
 *   User arguments.
 * Return
 *  The length of the written string.
 */
PH7_PRIVATE int PH7_builtin_vfprintf(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	fprintf_data sFdata;
	const char *zFormat;
	ph7_hashmap *pMap;
	io_private *pDev;
	SySet sArg;
	int n,nLen;
	if( nArg < 3 ){
		ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"Invalid arguments");
		ph7_result_int(pCtx,0);
		return PH7_OK;
	}
	{
		/* php: a non-resource $stream is a TypeError (#1), not a warn-and-return-0. */
		sxi32 rcs = PH7_CheckStreamArg(pCtx,apArg[0],1,"stream");
		if( rcs != PH7_OK ){
			return rcs;
		}
	}
	/* PHP 8 checks argument types left-to-right: $format (#2) then $values (#3). */
	{
		sxi32 rcf = PH7_FormatCheckFormatArg(pCtx,apArg[1],2);
		if( rcf != PH7_OK ){
			return rcf;
		}
	}
	if( !ph7_value_is_array(apArg[2]) ){
		/* PHP 8: a non-array $values is a catchable TypeError. */
		char zBuf[64];
		return PH7_VmThrowException(pCtx,"TypeError",
			"vfprintf(): Argument #3 ($values) must be of type array, %s given",
			VmValueGivenName(apArg[2],zBuf,sizeof(zBuf)));
	}
	/* Extract our private data */
	pDev = (io_private *)ph7_value_to_resource(apArg[0]);
	/* Make sure we are dealing with a valid io_private instance */
	if( IO_PRIVATE_INVALID(pDev) ){
		/*Expecting an IO handle */
		ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"Expecting an IO handle");
		ph7_result_int(pCtx,0);
		return PH7_OK;
	}
	/* Point to the target IO stream device */
	if( pDev->pStream == 0  || pDev->pStream->xWrite == 0 ){
		ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,
			"IO routine(%s) not implemented in the underlying stream(%s) device",
			ph7_function_name(pCtx),pDev->pStream ? pDev->pStream->zName : "null_stream"
			);
		ph7_result_int(pCtx,0);
		return PH7_OK;
	}
	/* Extract the string format */
	zFormat = ph7_value_to_string(apArg[1],&nLen);
	if( nLen < 1 ){
		/* Empty string,return zero */
		ph7_result_int(pCtx,0);
		return PH7_OK;
	}
	/* Point to hashmap */
	pMap = (ph7_hashmap *)apArg[2]->x.pOther;
	/* PHP 8: too few items in the $values array is a catchable ValueError before output.
	 * php runs this BEFORE validating the specifiers. */
	{
		sxi32 rcc = PH7_FormatCheckArgCount(pCtx,zFormat,nLen,(int)pMap->nEntry,1,TRUE);
		if( rcc != PH7_OK ){
			return rcc;
		}
	}
	/* PHP 8: an unknown or missing format specifier throws a catchable ValueError before
	 * any output; propagate the throw status verbatim. */
	{
		sxi32 rcv = PH7_FormatValidate(pCtx,zFormat,nLen);
		if( rcv != PH7_OK ){
			return rcv;
		}
	}
	/* Extract arguments from the hashmap */
	n = PH7_HashmapValuesToSet(pMap,&sArg);
	/* Prepare our private data */
	sFdata.nCount = 0;
	sFdata.pIO = pDev;
	/* Format the string */
	{
	sxi32 rcv = PH7_InputFormat(fprintfConsumer,pCtx,zFormat,nLen,n,(ph7_value **)SySetBasePtr(&sArg),(void *)&sFdata,TRUE);
	/* Return total number of bytes written*/
	ph7_result_int64(pCtx,sFdata.nCount);
	SySetRelease(&sArg);
	/* A %s argument that could not be coerced raised php's Error mid-format; the
	 * bytes still went to the stream, as php's do, so report the throw last. */
	if( rcv != SXRET_OK ){
		pCtx->nThrowRc = rcv;
		return rcv;
	}
	}
	return PH7_OK;
}
/*
 * Convert open modes (string passed to the fopen() function) [i.e: 'r','w+','a',...] into PH7 flags.
 * According to the PHP reference manual:
 *  The mode parameter specifies the type of access you require to the stream. It may be any of the following
 *   'r' 	Open for reading only; place the file pointer at the beginning of the file.
 *   'r+' 	Open for reading and writing; place the file pointer at the beginning of the file.
 *   'w' 	Open for writing only; place the file pointer at the beginning of the file and truncate the file
 *          to zero length. If the file does not exist, attempt to create it.
 *   'w+' 	Open for reading and writing; place the file pointer at the beginning of the file and truncate
 *              the file to zero length. If the file does not exist, attempt to create it.
 *   'a' 	Open for writing only; place the file pointer at the end of the file. If the file does not
 *         exist, attempt to create it.
 *   'a+' 	Open for reading and writing; place the file pointer at the end of the file. If the file does
 *          not exist, attempt to create it.
 *   'x' 	Create and open for writing only; place the file pointer at the beginning of the file. If the file
 *         already exists,
 *         the fopen() call will fail by returning FALSE and generating an error of level E_WARNING. If the file
 *         does not exist attempt to create it. This is equivalent to specifying O_EXCL|O_CREAT flags for
 *         the underlying open(2) system call.
 *   'x+' 	Create and open for reading and writing; otherwise it has the same behavior as 'x'.
 *   'c' 	Open the file for writing only. If the file does not exist, it is created. If it exists, it is neither truncated
 *          (as opposed to 'w'), nor the call to this function fails (as is the case with 'x'). The file pointer
 *          is positioned on the beginning of the file.
 *          This may be useful if it's desired to get an advisory lock (see flock()) before attempting to modify the file
 *          as using 'w' could truncate the file before the lock was obtained (if truncation is desired, ftruncate() can
 *          be used after the lock is requested).
 *   'c+' 	Open the file for reading and writing; otherwise it has the same behavior as 'c'.
 */
static int StrModeToFlags(ph7_context *pCtx,const char *zMode,int nLen)
{
	const char *zEnd = &zMode[nLen];
	int iFlag = 0;
	int c;
	if( nLen < 1 ){
		/* Open in a read-only mode */
		return PH7_IO_OPEN_RDONLY;
	}
	c = zMode[0];
	if( c == 'r' || c == 'R' ){
		/* Read-only access */
		iFlag = PH7_IO_OPEN_RDONLY;
		zMode++; /* Advance */
		if( zMode < zEnd ){
			c = zMode[0];
			if( c == '+' || c == 'w' || c == 'W' ){
				/* Read+Write access */
				iFlag = PH7_IO_OPEN_RDWR;
			}
		}
	}else if( c == 'w' || c == 'W' ){
		/* Overwrite mode.
		 * If the file does not exists,try to create it
		 */
		iFlag = PH7_IO_OPEN_WRONLY|PH7_IO_OPEN_TRUNC|PH7_IO_OPEN_CREATE;
		zMode++; /* Advance */
		if( zMode < zEnd ){
			c = zMode[0];
			if( c == '+' || c == 'r' || c == 'R' ){
				/* Read+Write access */
				iFlag &= ~PH7_IO_OPEN_WRONLY;
				iFlag |= PH7_IO_OPEN_RDWR;
			}
		}
	}else if( c == 'a' || c == 'A' ){
		/* Append mode (place the file pointer at the end of the file).
		 * Create the file if it does not exists.
		 */
		iFlag = PH7_IO_OPEN_WRONLY|PH7_IO_OPEN_APPEND|PH7_IO_OPEN_CREATE;
		zMode++; /* Advance */
		if( zMode < zEnd ){
			c = zMode[0];
			if( c == '+' ){
				/* Read-Write access */
				iFlag &= ~PH7_IO_OPEN_WRONLY;
				iFlag |= PH7_IO_OPEN_RDWR;
			}
		}
	}else if( c == 'x' || c == 'X' ){
		/* Exclusive access.
		 * If the file already exists,return immediately with a failure code.
		 * Otherwise create a new file.
		 */
		iFlag = PH7_IO_OPEN_WRONLY|PH7_IO_OPEN_EXCL;
		zMode++; /* Advance */
		if( zMode < zEnd ){
			c = zMode[0];
			if( c == '+' || c == 'r' || c == 'R' ){
				/* Read-Write access */
				iFlag &= ~PH7_IO_OPEN_WRONLY;
				iFlag |= PH7_IO_OPEN_RDWR;
			}
		}
	}else if( c == 'c' || c == 'C' ){
		/* Overwrite mode.Create the file if it does not exists.*/
		iFlag = PH7_IO_OPEN_WRONLY|PH7_IO_OPEN_CREATE;
		zMode++; /* Advance */
		if( zMode < zEnd ){
			c = zMode[0];
			if( c == '+' ){
				/* Read-Write access */
				iFlag &= ~PH7_IO_OPEN_WRONLY;
				iFlag |= PH7_IO_OPEN_RDWR;
			}
		}
	}else{
		/* Invalid mode. Assume a read only open */
		ph7_context_throw_error(pCtx,PH7_CTX_NOTICE,"Invalid open mode,PH7 is assuming a Read-Only open");
		iFlag = PH7_IO_OPEN_RDONLY;
	}
	while( zMode < zEnd ){
		c = zMode[0];
		if( c == 'b' || c == 'B' ){
			iFlag &= ~PH7_IO_OPEN_TEXT;
			iFlag |= PH7_IO_OPEN_BINARY;
		}else if( c == 't' || c == 'T' ){
			iFlag &= ~PH7_IO_OPEN_BINARY;
			iFlag |= PH7_IO_OPEN_TEXT;
		}
		zMode++;
	}
	return iFlag;
}
/*
 * Initialize the IO private structure.
 */
PH7_PRIVATE void InitIOPrivate(ph7_vm *pVm,const ph7_io_stream *pStream,io_private *pOut)
{
	pOut->pStream = pStream;
	SyBlobInit(&pOut->sBuffer,&pVm->sAllocator);
	pOut->nOfft = 0;
	/* Set the magic number */
	pOut->iMagic = IO_PRIVATE_MAGIC;
}
/*
 * Release the IO private structure.
 */
static void ReleaseIOPrivate(ph7_context *pCtx,io_private *pDev)
{
	SyBlobRelease(&pDev->sBuffer);
	pDev->iMagic = 0x2126; /* Invalid magic number so we can detetct misuse */
	/* Release the whole structure */
	ph7_context_free_chunk(pCtx,pDev);
}
/*
 * Mark a user-facing IO handle as closed while keeping the io_private alive.
 * The caller has already closed the underlying OS handle; we drop the work
 * buffer and stamp the closed magic so every ph7_value that still references
 * this handle sees a "resource (closed)" (php semantics). The struct is
 * reclaimed in bulk when the VM allocator is torn down. Freeing it here — as
 * fclose/closedir/pclose used to — would dangle the caller's copy (a latent,
 * pool-masked UAF) and keep reporting the handle open.
 */
PH7_PRIVATE void MarkIOPrivateClosed(io_private *pDev)
{
	SyBlobRelease(&pDev->sBuffer);
	pDev->pHandle = 0;
	pDev->iMagic = IO_PRIVATE_CLOSED_MAGIC;
}
/*
 * Reset the IO private structure.
 */
static void ResetIOPrivate(io_private *pDev)
{
	SyBlobReset(&pDev->sBuffer);
	pDev->nOfft = 0;
}
/* Forward declaration */

/*
 * resource fopen(string $filename,string $mode [,bool $use_include_path = false[,resource $context ]])
 *  Open a file,a URL or any other IO stream.
 * Parameters
 *  $filename
 *   If filename is of the form "scheme://...", it is assumed to be a URL and PHP will search
 *   for a protocol handler (also known as a wrapper) for that scheme. If no scheme is given
 *   then a regular file is assumed.
 *  $mode
 *   The mode parameter specifies the type of access you require to the stream
 *   See the block comment associated with the StrModeToFlags() for the supported
 *   modes.
 *  $use_include_path
 *   You can use the optional second parameter and set it to
 *   TRUE, if you want to search for the file in the include_path, too.
 *  $context
 *   A context stream resource.
 * Return
 *  File handle on success or FALSE on failure.
 */
/*
 * string|false stream_get_contents(resource $stream, int $maxLength = -1,
 *                                  int $offset = -1)
 *  Read the remaining contents of a stream into a string.
 */
PH7_PRIVATE int PH7_builtin_stream_get_contents(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	const ph7_io_stream *pStream;
	io_private *pDev;
	ph7_int64 nMax = -1;
	char zBuf[4096];
	ph7_int64 nRead;
	if( nArg < 1 || !ph7_value_is_resource(apArg[0]) ){
		ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"Expecting an IO handle");
		ph7_result_bool(pCtx,0);
		return PH7_OK;
	}
	pDev = (io_private *)ph7_value_to_resource(apArg[0]);
	if( IO_PRIVATE_INVALID(pDev) ){
		ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"Expecting an IO handle");
		ph7_result_bool(pCtx,0);
		return PH7_OK;
	}
	pStream = pDev->pStream;
	if( pStream == 0 || pStream->xRead == 0 ){
		ph7_result_bool(pCtx,0);
		return PH7_OK;
	}
	if( nArg > 1 && !ph7_value_is_null(apArg[1]) ){
		/* PHP 8 raises a catchable ValueError below -1; -1 (or the NULL
		 * default) means "read until EOF". */
		nMax = ph7_value_to_int64(apArg[1]);
		if( nMax < -1 ){
			return PH7_VmThrowException(pCtx,"ValueError",
				"stream_get_contents(): Argument #2 ($length) must be greater than or equal to -1");
		}
	}
	if( nArg > 2 ){
		ph7_int64 iOfft = ph7_value_to_int64(apArg[2]);
		if( iOfft >= 0 && pStream->xSeek ){
			pStream->xSeek(pDev->pHandle,iOfft,0/*SEEK_SET*/);
		}
	}
	ph7_result_string(pCtx,"",0); /* seed an empty string result */
	while( nMax != 0 ){
		ph7_int64 nAsk = (ph7_int64)sizeof(zBuf);
		if( nMax > 0 && nMax < nAsk ){
			nAsk = nMax;
		}
		nRead = pStream->xRead(pDev->pHandle,zBuf,nAsk);
		if( nRead < 1 ){
			break;
		}
		ph7_result_string(pCtx,zBuf,(int)nRead); /* appends */
		if( nMax > 0 ){
			nMax -= nRead;
		}
	}
	return PH7_OK;
}
/*
 * array stream_get_wrappers(void) — names of the registered stream devices.
 */
PH7_PRIVATE int PH7_builtin_stream_get_wrappers(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	ph7_value *pArr,*pV;
	ph7_io_stream **apDev;
	sxu32 n;
	SXUNUSED(nArg);
	SXUNUSED(apArg);
	pArr = ph7_context_new_array(pCtx);
	pV = ph7_context_new_scalar(pCtx);
	if( pArr == 0 || pV == 0 ){
		ph7_result_null(pCtx);
		return PH7_OK;
	}
	apDev = (ph7_io_stream **)SySetBasePtr(&pCtx->pVm->aIOstream);
	for( n = 0 ; n < SySetUsed(&pCtx->pVm->aIOstream) ; n++ ){
		ph7_value_string(pV,apDev[n]->zName,-1);
		ph7_array_add_elem(pArr,0,pV);
		ph7_value_reset_string_cursor(pV);
	}
	ph7_result_value(pCtx,pArr);
	return PH7_OK;
}
/*
 * array stream_get_meta_data(resource $stream) — best-effort php shape over
 * the io_private state (uri/wrapper_type/seekable/eof; recorded approximation).
 */
PH7_PRIVATE int PH7_builtin_stream_get_meta_data(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	io_private *pDev;
	ph7_value *pArr,*pV;
	if( nArg < 1 || !ph7_value_is_resource(apArg[0]) ){
		ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"Expecting an IO handle");
		ph7_result_bool(pCtx,0);
		return PH7_OK;
	}
	pDev = (io_private *)ph7_value_to_resource(apArg[0]);
	if( IO_PRIVATE_INVALID(pDev) ){
		ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"Expecting an IO handle");
		ph7_result_bool(pCtx,0);
		return PH7_OK;
	}
	pArr = ph7_context_new_array(pCtx);
	pV = ph7_context_new_scalar(pCtx);
	if( pArr == 0 || pV == 0 ){
		ph7_result_null(pCtx);
		return PH7_OK;
	}
	ph7_value_bool(pV,0);
	ph7_array_add_strkey_elem(pArr,"timed_out",pV);
	ph7_value_bool(pV,1);
	ph7_array_add_strkey_elem(pArr,"blocked",pV);
	/* eof is best-effort: a read probe would consume state on unseekable
	 * devices, so report FALSE and let feof() answer properly */
	ph7_value_bool(pV,0);
	ph7_array_add_strkey_elem(pArr,"eof",pV);
	ph7_value_int(pV,0);
	ph7_array_add_strkey_elem(pArr,"unread_bytes",pV);
	ph7_value_string(pV,pDev->pStream ? pDev->pStream->zName : "",-1);
	ph7_array_add_strkey_elem(pArr,"wrapper_type",pV);
	ph7_value_reset_string_cursor(pV);
	ph7_value_string(pV,pDev->pStream ? pDev->pStream->zName : "",-1);
	ph7_array_add_strkey_elem(pArr,"stream_type",pV);
	ph7_value_reset_string_cursor(pV);
	ph7_value_bool(pV,pDev->pStream && pDev->pStream->xSeek != 0);
	ph7_array_add_strkey_elem(pArr,"seekable",pV);
	ph7_result_value(pCtx,pArr);
	return PH7_OK;
}
/*
 * stream_context_create([array $options[, array $params]]) — INERT: PHL has
 * no context plumbing yet; the options array itself is returned so code that
 * creates and passes contexts keeps working (recorded divergence: not a
 * resource, options unconsumed).
 */
PH7_PRIVATE int PH7_builtin_stream_context_create(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	if( nArg > 0 && ph7_value_is_array(apArg[0]) ){
		ph7_result_value(pCtx,apArg[0]);
	}else{
		ph7_value *pArr = ph7_context_new_array(pCtx);
		if( pArr == 0 ){
			ph7_result_null(pCtx);
			return PH7_OK;
		}
		ph7_result_value(pCtx,pArr);
	}
	return PH7_OK;
}
/*
 * tcp:// socket stream (fsockopen / stream_socket_client). The handle is a
 * small struct carrying the OS socket plus an EOF latch, so feof() works.
 */
#ifdef PH7_ENABLE_NET
typedef struct sock_private sock_private;
struct sock_private
{
	ph7_vm *pVm;
	ph7_socket sock;
	int bEof;
};
static ph7_int64 SockStreamData_Read(void *pHandle,void *pBuffer,ph7_int64 nRead)
{
	sock_private *pSock = (sock_private *)pHandle;
	int n;
	if( pSock == 0 || pSock->bEof ){
		return 0;
	}
	n = PH7_NetRecv(pSock->sock,pBuffer,(int)nRead,0);
	if( n <= 0 ){
		pSock->bEof = 1;
		return 0;
	}
	return (ph7_int64)n;
}
static ph7_int64 SockStreamData_Write(void *pHandle,const void *pBuf,ph7_int64 nWrite)
{
	sock_private *pSock = (sock_private *)pHandle;
	int n;
	if( pSock == 0 ){
		return -1;
	}
	n = PH7_NetSendAll(pSock->sock,pBuf,(int)nWrite);
	return n < 0 ? -1 : (ph7_int64)n;
}
static void SockStreamData_Close(void *pHandle)
{
	sock_private *pSock = (sock_private *)pHandle;
	if( pSock == 0 ){
		return;
	}
	PH7_NetClose(pSock->sock);
	SyMemBackendFree(&pSock->pVm->sAllocator,pSock);
}
/* xOpen for "host:port" (the scheme is already stripped by the device lookup) */
static int SockStreamData_Open(const char *zName,int iMode,ph7_value *pResource,void ** ppHandle)
{
	sock_private *pSock;
	ph7_socket sock;
	char zHost[256];
	const char *zColon;
	int iPort = 0,iErrno = 0;
	const char *zErr = "";
	ph7_vm *pVm = pResource ? pResource->pVm : 0;
	SXUNUSED(iMode);
	if( pVm == 0 ){
		return -1;
	}
	zColon = SyStrlen(zName) ? &zName[SyStrlen(zName)-1] : zName;
	while( zColon > zName && zColon[0] != ':' ){
		zColon--;
	}
	if( zColon <= zName || zColon[0] != ':' ){
		return -1;
	}
	{
		sxu32 n = (sxu32)(zColon - zName);
		if( n >= sizeof(zHost) ){
			n = sizeof(zHost) - 1;
		}
		SyMemcpy(zName,zHost,n);
		zHost[n] = 0;
	}
	{
		sxi32 iTmp = 0;
		SyStrToInt32(&zColon[1],(sxu32)SyStrlen(&zColon[1]),(void *)&iTmp,0);
		iPort = (int)iTmp;
	}
	sock = PH7_NetConnect(zHost,iPort,0,&iErrno,&zErr);
	if( sock == PH7_NET_INVALID_SOCKET ){
		return -1;
	}
	pSock = (sock_private *)SyMemBackendAlloc(&pVm->sAllocator,sizeof(sock_private));
	if( pSock == 0 ){
		PH7_NetClose(sock);
		return -1;
	}
	pSock->pVm = pVm;
	pSock->sock = sock;
	pSock->bEof = 0;
	*ppHandle = (void *)pSock;
	return PH7_OK;
}
PH7_PRIVATE const ph7_io_stream sTCP_Stream = {
	"tcp",
	PH7_IO_STREAM_VERSION,
	SockStreamData_Open, /* xOpen */
	0,   /* xOpenDir */
	SockStreamData_Close,/* xClose */
	0,  /* xCloseDir */
	SockStreamData_Read, /* xRead */
	0,  /* xReadDir */
	SockStreamData_Write,/* xWrite */
	0,  /* xSeek (sockets are not seekable) */
	0,  /* xLock */
	0,  /* xRewindDir */
	0,  /* xTell */
	0,  /* xTrunc */
	0,  /* xSync */
	0   /* xStat */
};
#endif /* PH7_ENABLE_NET */
/*
 * Userland stream wrappers (stream_wrapper_register). The engine's device
 * callbacks receive no device pointer, so each registered wrapper needs its
 * OWN xOpen thunk: PHL keeps a bounded pool of PHL_UWRAP_MAX slots, each with
 * a static thunk that knows its index (recorded limit; php has no cap).
 * The handle carries the userland object, and every stream op dispatches the
 * php streamWrapper protocol method on it.
 */
#define PHL_UWRAP_MAX 8
typedef struct uwrap_slot uwrap_slot;
struct uwrap_slot
{
	ph7_vm *pVm;              /* owning VM (0 = free slot) */
	char zScheme[32];         /* protocol name */
	char zClass[128];         /* userland wrapper class */
	ph7_io_stream sStream;    /* the device handed to the VM */
};
typedef struct uwrap_handle uwrap_handle;
struct uwrap_handle
{
	ph7_vm *pVm;
	ph7_class_instance *pObj; /* the wrapper instance (one per open stream) */
	int iSlot;
	int bEof;
};
static uwrap_slot g_aUwrap[PHL_UWRAP_MAX];
/* Call $obj->$zMethod(...) and copy the result into pResult (may be 0) */
static int UwrapCall(uwrap_handle *pH,const char *zMethod,int nArg,ph7_value **apArg,
	ph7_value *pResult)
{
	ph7_class_method *pMeth;
	if( pH == 0 || pH->pObj == 0 ){
		return -1;
	}
	pMeth = PH7_ClassExtractMethod(pH->pObj->pClass,zMethod,(sxu32)SyStrlen(zMethod));
	if( pMeth == 0 ){
		return -1;
	}
	if( PH7_VmCallClassMethod(pH->pVm,pH->pObj,pMeth,pResult,nArg,apArg) != SXRET_OK ){
		return -1;
	}
	return 0;
}
static ph7_int64 UwrapRead(void *pHandle,void *pBuffer,ph7_int64 nRead)
{
	uwrap_handle *pH = (uwrap_handle *)pHandle;
	ph7_value sArg,sRet;
	const char *zData;
	int nData = 0;
	ph7_int64 n = 0;
	if( pH == 0 || pH->bEof ){
		return 0;
	}
	PH7_MemObjInit(pH->pVm,&sArg);
	PH7_MemObjInit(pH->pVm,&sRet);
	ph7_value_int64(&sArg,nRead);
	{
		ph7_value *apArg[1];
		apArg[0] = &sArg;
		if( UwrapCall(pH,"stream_read",1,apArg,&sRet) != 0 ){
			PH7_MemObjRelease(&sArg);
			PH7_MemObjRelease(&sRet);
			return -1;
		}
	}
	zData = ph7_value_to_string(&sRet,&nData);
	if( nData > 0 ){
		if( (ph7_int64)nData > nRead ){
			nData = (int)nRead;
		}
		SyMemcpy(zData,pBuffer,(sxu32)nData);
		n = nData;
	}else{
		pH->bEof = 1;
	}
	PH7_MemObjRelease(&sArg);
	PH7_MemObjRelease(&sRet);
	return n;
}
static ph7_int64 UwrapWrite(void *pHandle,const void *pBuf,ph7_int64 nWrite)
{
	uwrap_handle *pH = (uwrap_handle *)pHandle;
	ph7_value sArg,sRet;
	ph7_int64 n;
	if( pH == 0 ){
		return -1;
	}
	PH7_MemObjInit(pH->pVm,&sArg);
	PH7_MemObjInit(pH->pVm,&sRet);
	ph7_value_string(&sArg,(const char *)pBuf,(int)nWrite);
	{
		ph7_value *apArg[1];
		apArg[0] = &sArg;
		if( UwrapCall(pH,"stream_write",1,apArg,&sRet) != 0 ){
			PH7_MemObjRelease(&sArg);
			PH7_MemObjRelease(&sRet);
			return -1;
		}
	}
	n = ph7_value_to_int64(&sRet);
	PH7_MemObjRelease(&sArg);
	PH7_MemObjRelease(&sRet);
	return n;
}
static int UwrapSeek(void *pHandle,ph7_int64 iOfft,int whence)
{
	uwrap_handle *pH = (uwrap_handle *)pHandle;
	ph7_value sOfft,sWhence,sRet;
	ph7_value *apArg[2];
	int rc;
	if( pH == 0 ){
		return -1;
	}
	PH7_MemObjInit(pH->pVm,&sOfft);
	PH7_MemObjInit(pH->pVm,&sWhence);
	PH7_MemObjInit(pH->pVm,&sRet);
	ph7_value_int64(&sOfft,iOfft);
	ph7_value_int(&sWhence,whence);
	apArg[0] = &sOfft;
	apArg[1] = &sWhence;
	rc = UwrapCall(pH,"stream_seek",2,apArg,&sRet);
	if( rc == 0 ){
		pH->bEof = 0;
		rc = ph7_value_to_bool(&sRet) ? PH7_OK : -1;
	}
	PH7_MemObjRelease(&sOfft);
	PH7_MemObjRelease(&sWhence);
	PH7_MemObjRelease(&sRet);
	return rc;
}
static ph7_int64 UwrapTell(void *pHandle)
{
	uwrap_handle *pH = (uwrap_handle *)pHandle;
	ph7_value sRet;
	ph7_int64 n;
	if( pH == 0 ){
		return -1;
	}
	PH7_MemObjInit(pH->pVm,&sRet);
	if( UwrapCall(pH,"stream_tell",0,0,&sRet) != 0 ){
		PH7_MemObjRelease(&sRet);
		return -1;
	}
	n = ph7_value_to_int64(&sRet);
	PH7_MemObjRelease(&sRet);
	return n;
}
static void UwrapClose(void *pHandle)
{
	uwrap_handle *pH = (uwrap_handle *)pHandle;
	if( pH == 0 ){
		return;
	}
	UwrapCall(pH,"stream_close",0,0,0);
	if( pH->pObj ){
		PH7_ClassInstanceUnref(pH->pObj);
	}
	SyMemBackendFree(&pH->pVm->sAllocator,pH);
}
/* Shared open: instantiate the wrapper class and call stream_open() */
static int UwrapOpenSlot(int iSlot,const char *zName,int iMode,ph7_value *pResource,void **ppHandle)
{
	uwrap_slot *pSlot = &g_aUwrap[iSlot];
	ph7_vm *pVm = pResource ? pResource->pVm : 0;
	ph7_class *pClass;
	uwrap_handle *pH;
	ph7_value sPath,sMode,sOpts,sOpened,sRet;
	ph7_value *apArg[4];
	int rc;
	if( pVm == 0 || pSlot->pVm == 0 ){
		return -1;
	}
	pClass = PH7_VmExtractClass(pVm,pSlot->zClass,(sxu32)SyStrlen(pSlot->zClass),TRUE,0);
	if( pClass == 0 ){
		return -1;
	}
	pH = (uwrap_handle *)SyMemBackendAlloc(&pVm->sAllocator,sizeof(uwrap_handle));
	if( pH == 0 ){
		return -1;
	}
	pH->pVm = pVm;
	pH->iSlot = iSlot;
	pH->bEof = 0;
	pH->pObj = PH7_NewClassInstance(pVm,pClass);
	if( pH->pObj == 0 ){
		SyMemBackendFree(&pVm->sAllocator,pH);
		return -1;
	}
	/* php hands stream_open the FULL url, scheme included */
	PH7_MemObjInit(pVm,&sPath);
	PH7_MemObjInit(pVm,&sMode);
	PH7_MemObjInit(pVm,&sOpts);
	PH7_MemObjInit(pVm,&sRet);
	/* $opened_path is BY REFERENCE: the callee's binding needs a real memobj
	 * slot (a stack ph7_value has nIdx == SXU32_HIGH and the engine rejects
	 * it as "could not be passed by reference"). */
	{
		ph7_value *pRefSlot = PH7_ReserveMemObj(pVm);
		if( pRefSlot == 0 ){
			PH7_ClassInstanceUnref(pH->pObj);
			SyMemBackendFree(&pVm->sAllocator,pH);
			return -1;
		}
		PH7_MemObjInit(pVm,&sOpened);
		sOpened.nIdx = pRefSlot->nIdx;
	}
	{
		SyBlob sUrl;
		SyBlobInit(&sUrl,&pVm->sAllocator);
		SyBlobFormat(&sUrl,"%s://%s",pSlot->zScheme,zName);
		ph7_value_string(&sPath,(const char *)SyBlobData(&sUrl),(int)SyBlobLength(&sUrl));
		SyBlobRelease(&sUrl);
	}
	ph7_value_string(&sMode,(iMode & PH7_IO_OPEN_WRONLY) ? "w"
		: ((iMode & PH7_IO_OPEN_APPEND) ? "a" : "r"),-1);
	ph7_value_int(&sOpts,0);
	apArg[0] = &sPath;
	apArg[1] = &sMode;
	apArg[2] = &sOpts;
	apArg[3] = &sOpened;
	rc = UwrapCall(pH,"stream_open",4,apArg,&sRet);
	if( rc == 0 && !ph7_value_to_bool(&sRet) ){
		rc = -1;
	}
	PH7_MemObjRelease(&sPath);
	PH7_MemObjRelease(&sMode);
	PH7_MemObjRelease(&sOpts);
	PH7_MemObjRelease(&sOpened);
	PH7_MemObjRelease(&sRet);
	if( rc != 0 ){
		PH7_ClassInstanceUnref(pH->pObj);
		SyMemBackendFree(&pVm->sAllocator,pH);
		return -1;
	}
	*ppHandle = (void *)pH;
	return PH7_OK;
}
/* One xOpen thunk per slot (the device callbacks get no device pointer) */
#define PHL_UWRAP_THUNK(N) \
	static int UwrapOpen##N(const char *zName,int iMode,ph7_value *pResource,void **ppHandle) \
	{ return UwrapOpenSlot(N,zName,iMode,pResource,ppHandle); }
PHL_UWRAP_THUNK(0)
PHL_UWRAP_THUNK(1)
PHL_UWRAP_THUNK(2)
PHL_UWRAP_THUNK(3)
PHL_UWRAP_THUNK(4)
PHL_UWRAP_THUNK(5)
PHL_UWRAP_THUNK(6)
PHL_UWRAP_THUNK(7)
static int (* const g_aUwrapOpen[PHL_UWRAP_MAX])(const char *,int,ph7_value *,void **) = {
	UwrapOpen0,UwrapOpen1,UwrapOpen2,UwrapOpen3,
	UwrapOpen4,UwrapOpen5,UwrapOpen6,UwrapOpen7
};
/*
 * bool stream_wrapper_register(string $protocol, string $class, int $flags = 0)
 * bool stream_wrapper_unregister(string $protocol)
 */
PH7_PRIVATE int PH7_builtin_stream_wrapper_register(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	const char *zScheme,*zClass;
	int nScheme,nClass,i,iFree = -1;
	if( nArg < 2 ){
		ph7_result_bool(pCtx,0);
		return PH7_OK;
	}
	zScheme = ph7_value_to_string(apArg[0],&nScheme);
	zClass  = ph7_value_to_string(apArg[1],&nClass);
	if( nScheme < 1 || nScheme >= (int)sizeof(g_aUwrap[0].zScheme)
	 || nClass < 1 || nClass >= (int)sizeof(g_aUwrap[0].zClass) ){
		ph7_result_bool(pCtx,0);
		return PH7_OK;
	}
	/* php: registering an already-taken protocol warns and returns false.
	 * (Scan the device list directly — PH7_VmGetStreamDevice falls back to
	 * the DEFAULT device for a scheme-less name, so it can't answer this.) */
	{
		ph7_io_stream **apDev = (ph7_io_stream **)SySetBasePtr(&pCtx->pVm->aIOstream);
		sxu32 n;
		for( n = 0 ; n < SySetUsed(&pCtx->pVm->aIOstream) ; n++ ){
			if( (int)SyStrlen(apDev[n]->zName) == nScheme
			 && SyStrnicmp(apDev[n]->zName,zScheme,(sxu32)nScheme) == 0 ){
				ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,
					"Protocol %.*s:// is already defined.",nScheme,zScheme);
				ph7_result_bool(pCtx,0);
				return PH7_OK;
			}
		}
	}
	for( i = 0 ; i < PHL_UWRAP_MAX ; i++ ){
		if( g_aUwrap[i].pVm == 0 ){
			iFree = i;
			break;
		}
	}
	if( iFree < 0 ){
		ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,
			"Too many registered stream wrappers (PHL limit: %d)",PHL_UWRAP_MAX);
		ph7_result_bool(pCtx,0);
		return PH7_OK;
	}
	{
		uwrap_slot *pSlot = &g_aUwrap[iFree];
		SyMemcpy(zScheme,pSlot->zScheme,(sxu32)nScheme);
		pSlot->zScheme[nScheme] = 0;
		SyMemcpy(zClass,pSlot->zClass,(sxu32)nClass);
		pSlot->zClass[nClass] = 0;
		pSlot->pVm = pCtx->pVm;
		SyZero(&pSlot->sStream,sizeof(ph7_io_stream));
		pSlot->sStream.zName = pSlot->zScheme;
		pSlot->sStream.iVersion = PH7_IO_STREAM_VERSION;
		pSlot->sStream.xOpen = g_aUwrapOpen[iFree];
		pSlot->sStream.xClose = UwrapClose;
		pSlot->sStream.xRead = UwrapRead;
		pSlot->sStream.xWrite = UwrapWrite;
		pSlot->sStream.xSeek = UwrapSeek;
		pSlot->sStream.xTell = UwrapTell;
		ph7_vm_config(pCtx->pVm,PH7_VM_CONFIG_IO_STREAM,&pSlot->sStream);
	}
	ph7_result_bool(pCtx,1);
	return PH7_OK;
}
PH7_PRIVATE int PH7_builtin_stream_wrapper_unregister(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	const char *zScheme;
	int nScheme,i;
	if( nArg < 1 ){
		ph7_result_bool(pCtx,0);
		return PH7_OK;
	}
	zScheme = ph7_value_to_string(apArg[0],&nScheme);
	for( i = 0 ; i < PHL_UWRAP_MAX ; i++ ){
		if( g_aUwrap[i].pVm == pCtx->pVm
		 && (int)SyStrlen(g_aUwrap[i].zScheme) == nScheme
		 && SyMemcmp(g_aUwrap[i].zScheme,zScheme,(sxu32)nScheme) == 0 ){
			/* The device stays in the VM's list (the engine has no removal
			 * API); neutering the slot makes every later open fail, which is
			 * what unregister means to a script — recorded. */
			g_aUwrap[i].pVm = 0;
			ph7_result_bool(pCtx,1);
			return PH7_OK;
		}
	}
	ph7_result_bool(pCtx,0);
	return PH7_OK;
}
#ifdef PH7_ENABLE_NET
/*
 * resource|false fsockopen(string $hostname, int $port = -1, int &$error_code,
 *                          string &$error_message, ?float $timeout = null)
 * resource|false stream_socket_client(string $address, int &$error_code,
 *                          string &$error_message, ?float $timeout = null, ...)
 * TCP only (the recorded scope: no ssl://, udp:// or unix:// yet).
 */
PH7_PRIVATE int PH7_builtin_fsockopen(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	const char *zFunc = ph7_function_name(pCtx);
	int bClientForm = (zFunc[0] == 's'); /* stream_socket_client */
	const char *zTarget,*zErr = "";
	char zHost[256];
	int nTarget,iPort = -1,iErrno = 0,iTimeoutMs = 0;
	ph7_socket sock;
	io_private *pDev;
	sock_private *pSock;
	int iArgErrno = bClientForm ? 1 : 2;
	int iArgErrstr = bClientForm ? 2 : 3;
	int iArgTimeout = bClientForm ? 3 : 4;
	if( nArg < 1 || !ph7_value_is_string(apArg[0]) ){
		ph7_result_bool(pCtx,0);
		return PH7_OK;
	}
	zTarget = ph7_value_to_string(apArg[0],&nTarget);
	/* Strip a scheme; only tcp:// (and the bare form) are supported */
	{
		const char *z = zTarget,*zEnd = &zTarget[nTarget];
		const char *zSep = 0;
		while( z < zEnd - 2 ){
			if( z[0] == ':' && z[1] == '/' && z[2] == '/' ){
				zSep = z;
				break;
			}
			z++;
		}
		if( zSep ){
			if( !((zSep - zTarget) == 3 && SyStrnicmp(zTarget,"tcp",3) == 0) ){
				ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,
					"Unable to connect to %.*s (unsupported transport; PHL supports tcp:// only)",
					nTarget,zTarget);
				ph7_result_bool(pCtx,0);
				return PH7_OK;
			}
			nTarget -= (int)(zSep + 3 - zTarget);
			zTarget = zSep + 3;
		}
	}
	/* host[:port] */
	{
		int i = nTarget - 1;
		int nHost = nTarget;
		while( i > 0 && zTarget[i] != ':' ){
			i--;
		}
		if( i > 0 && zTarget[i] == ':' ){
			sxi32 iTmp = 0;
			SyStrToInt32(&zTarget[i+1],(sxu32)(nTarget - i - 1),(void *)&iTmp,0);
			iPort = (int)iTmp;
			nHost = i;
		}
		if( nHost >= (int)sizeof(zHost) ){
			nHost = (int)sizeof(zHost) - 1;
		}
		SyMemcpy(zTarget,zHost,(sxu32)nHost);
		zHost[nHost] = 0;
	}
	if( !bClientForm && nArg > 1 && !ph7_value_is_null(apArg[1]) ){
		iPort = ph7_value_to_int(apArg[1]);
	}
	if( nArg > iArgTimeout && !ph7_value_is_null(apArg[iArgTimeout]) ){
		double rTimeout = ph7_value_to_double(apArg[iArgTimeout]);
		if( rTimeout > 0 ){
			iTimeoutMs = (int)(rTimeout * 1000);
		}
	}
	if( iPort < 0 ){
		ph7_result_bool(pCtx,0);
		return PH7_OK;
	}
	sock = PH7_NetConnect(zHost,iPort,iTimeoutMs,&iErrno,&zErr);
	if( sock == PH7_NET_INVALID_SOCKET ){
		/* php reports the failure through the by-ref out-params + a warning */
		{
			ph7_value *pTmp = ph7_context_new_scalar(pCtx);
			if( pTmp ){
				if( nArg > iArgErrno ){
					ph7_value_int(pTmp,iErrno);
					PH7_VmStoreArgByRef(pCtx->pVm,apArg[iArgErrno],pTmp);
				}
				if( nArg > iArgErrstr ){
					ph7_value_string(pTmp,zErr,-1);
					PH7_VmStoreArgByRef(pCtx->pVm,apArg[iArgErrstr],pTmp);
				}
			}
		}
		/* NOTE: ph7_context_throw_error_format already prepends "fname(): " */
		ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,
			"Unable to connect to %s:%d (%s)",zHost,iPort,zErr);
		ph7_result_bool(pCtx,0);
		return PH7_OK;
	}
	{
		ph7_value *pTmp = ph7_context_new_scalar(pCtx);
		if( pTmp ){
			if( nArg > iArgErrno ){
				ph7_value_int(pTmp,0);
				PH7_VmStoreArgByRef(pCtx->pVm,apArg[iArgErrno],pTmp);
			}
			if( nArg > iArgErrstr ){
				ph7_value_string(pTmp,"",0);
				PH7_VmStoreArgByRef(pCtx->pVm,apArg[iArgErrstr],pTmp);
			}
		}
	}
	/* Wrap the socket in an io_private so the whole f* family works on it */
	pDev = (io_private *)ph7_context_alloc_chunk(pCtx,sizeof(io_private),TRUE,FALSE);
	pSock = (sock_private *)SyMemBackendAlloc(&pCtx->pVm->sAllocator,sizeof(sock_private));
	if( pDev == 0 || pSock == 0 ){
		PH7_NetClose(sock);
		ph7_result_bool(pCtx,0);
		return PH7_OK;
	}
	pSock->pVm = pCtx->pVm;
	pSock->sock = sock;
	pSock->bEof = 0;
	InitIOPrivate(pCtx->pVm,&sTCP_Stream,pDev);
	pDev->pHandle = (void *)pSock;
	ph7_result_resource(pCtx,pDev);
	return PH7_OK;
}
#endif /* PH7_ENABLE_NET */
PH7_PRIVATE int PH7_builtin_fopen(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	const ph7_io_stream *pStream;
	const char *zUri,*zMode;
	ph7_value *pResource;
	io_private *pDev;
	int iLen,imLen;
	int iOpenFlags;
	if( nArg < 1 || !ph7_value_is_string(apArg[0]) ){
		/* Missing/Invalid arguments,return FALSE */
		ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"Expecting a file path or URL");
		ph7_result_bool(pCtx,0);
		return PH7_OK;
	}
	/* Extract the URI and the desired access mode */
	zUri  = ph7_value_to_string(apArg[0],&iLen);
	if( nArg > 1 ){
		zMode = ph7_value_to_string(apArg[1],&imLen);
	}else{
		/* Set a default read-only mode */
		zMode = "r";
		imLen = (int)sizeof(char);
	}
	/* Try to extract a stream */
	pStream = PH7_VmGetStreamDevice(pCtx->pVm,&zUri,iLen);
	if( pStream == 0 ){
		ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,
			"No stream device is associated with the given URI(%s)",zUri);
		ph7_result_bool(pCtx,0);
		return PH7_OK;
	}
	/* Allocate a new IO private instance */
	pDev = (io_private *)ph7_context_alloc_chunk(pCtx,sizeof(io_private),TRUE,FALSE);
	if( pDev == 0 ){
		ph7_context_throw_error(pCtx,PH7_CTX_ERR,"PH7 is running out of memory");
		ph7_result_bool(pCtx,0);
		return PH7_OK;
	}
	pResource = 0;
	if( nArg > 3 ){
		pResource = apArg[3];
	}else if( is_php_stream(pStream) || is_data_stream(pStream) ){
		/* TICKET 1433-80: The php:// and data:// streams need a ph7_value to
		 * access the underlying virtual machine.
		 */
		pResource = apArg[0];
	}
	/* Initialize the structure */
	InitIOPrivate(pCtx->pVm,pStream,pDev);
	/* Convert open mode to PH7 flags */
	iOpenFlags = StrModeToFlags(pCtx,zMode,imLen);
	/* Try to get a handle */
	pDev->pHandle = PH7_StreamOpenHandle(pCtx->pVm,pStream,zUri,iOpenFlags,
		nArg > 2 ? ph7_value_to_bool(apArg[2]) : FALSE,pResource,FALSE,0);
	if( pDev->pHandle == 0 ){
		VfsThrowOpenWarning(pCtx,zUri);
		ph7_result_bool(pCtx,0);
		ph7_context_free_chunk(pCtx,pDev);
		return PH7_OK;
	}
	/* All done,return the io_private instance as a resource */
	ph7_result_resource(pCtx,pDev);
	return PH7_OK;
}
/*
 * bool fclose(resource $handle)
 *  Closes an open file pointer
 * Parameters
 *  $handle
 *   The file pointer.
 * Return
 *  TRUE on success or FALSE on failure.
 */
PH7_PRIVATE int PH7_builtin_fclose(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	const ph7_io_stream *pStream;
	io_private *pDev;
	ph7_vm *pVm;
	if( nArg < 1 || !ph7_value_is_resource(apArg[0]) ){
		/* Missing/Invalid arguments,return FALSE */
		ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"Expecting an IO handle");
		ph7_result_bool(pCtx,0);
		return PH7_OK;
	}
	/* Extract our private data */
	pDev = (io_private *)ph7_value_to_resource(apArg[0]);
	/* php: fclose() on an already-closed stream raises a catchable TypeError */
	if( pDev != 0 && pDev->iMagic == IO_PRIVATE_CLOSED_MAGIC ){
		return PH7_VmThrowException(pCtx,"TypeError",
			"fclose(): Argument #1 ($stream) must be an open stream resource");
	}
	/* Make sure we are dealing with a valid io_private instance */
	if( IO_PRIVATE_INVALID(pDev) ){
		/*Expecting an IO handle */
		ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"Expecting an IO handle");
		ph7_result_bool(pCtx,0);
		return PH7_OK;
	}
	/* Point to the target IO stream device */
	pStream = pDev->pStream;
	if( pStream == 0 ){
		ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,
			"IO routine(%s) not implemented in the underlying stream(%s) device,PH7 is returning FALSE",
			ph7_function_name(pCtx),pStream ? pStream->zName : "null_stream"
			);
		ph7_result_bool(pCtx,0);
		return PH7_OK;
	}
	/* Point to the VM that own this context */
	pVm = pCtx->pVm;
	/* TICKET 1433-62: Keep the STDIN/STDOUT/STDERR handles open */
	if( pDev != pVm->pStdin && pDev != pVm->pStdout && pDev != pVm->pStderr ){
		/* Perform the requested operation */
		PH7_StreamCloseHandle(pStream,pDev->pHandle);
		/* Keep the handle alive but flag it closed so shared copies see it */
		MarkIOPrivateClosed(pDev);
	}
	/* Return TRUE */
	ph7_result_bool(pCtx,1);
	return PH7_OK;
}
#if !defined(PH7_DISABLE_HASH_FUNC)
/*
 * MD5/SHA1 digest consumer.
 */
static int vfsHashConsumer(const void *pData,unsigned int nLen,void *pUserData)
{
	/* Append hex chunk verbatim */
	ph7_result_string((ph7_context *)pUserData,(const char *)pData,(int)nLen);
	return SXRET_OK;
}
/*
 * string md5_file(string $uri[,bool $raw_output = false ])
 *  Calculates the md5 hash of a given file.
 * Parameters
 *  $uri
 *   Target URI (file(/path/to/something) or URL(http://www.symisc.net/))
 *  $raw_output
 *   When TRUE, returns the digest in raw binary format with a length of 16.
 * Return
 *  Return the MD5 digest on success or FALSE on failure.
 */
PH7_PRIVATE int PH7_builtin_md5_file(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	const ph7_io_stream *pStream;
	unsigned char zDigest[16];
	int raw_output  = FALSE;
	const char *zFile;
	MD5Context sCtx;
	char zBuf[8192];
	void *pHandle;
	ph7_int64 n;
	int nLen;
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
	if( nArg > 1 ){
		raw_output = ph7_value_to_bool(apArg[1]);
	}
	/* Try to open the file in read-only mode */
	pHandle = PH7_StreamOpenHandle(pCtx->pVm,pStream,zFile,PH7_IO_OPEN_RDONLY,FALSE,0,FALSE,0);
	if( pHandle == 0 ){
		VfsThrowOpenWarning(pCtx,zFile);
		ph7_result_bool(pCtx,0);
		return PH7_OK;
	}
	/* Init the MD5 context */
	MD5Init(&sCtx);
	/* Perform the requested operation */
	for(;;){
		n = pStream->xRead(pHandle,zBuf,sizeof(zBuf));
		if( n < 1 ){
			/* EOF or IO error,break immediately */
			break;
		}
		MD5Update(&sCtx,(const unsigned char *)zBuf,(unsigned int)n);
	}
	/* Close the stream */
	PH7_StreamCloseHandle(pStream,pHandle);
	/* Extract the digest */
	MD5Final(zDigest,&sCtx);
	if( raw_output ){
		/* Output raw digest */
		ph7_result_string(pCtx,(const char *)zDigest,sizeof(zDigest));
	}else{
		/* Perform a binary to hex conversion */
		SyBinToHexConsumer((const void *)zDigest,sizeof(zDigest),vfsHashConsumer,pCtx);
	}
	return PH7_OK;
}
/*
 * string sha1_file(string $uri[,bool $raw_output = false ])
 *  Calculates the SHA1 hash of a given file.
 * Parameters
 *  $uri
 *   Target URI (file(/path/to/something) or URL(http://www.symisc.net/))
 *  $raw_output
 *   When TRUE, returns the digest in raw binary format with a length of 20.
 * Return
 *  Return the SHA1 digest on success or FALSE on failure.
 */
PH7_PRIVATE int PH7_builtin_sha1_file(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	const ph7_io_stream *pStream;
	unsigned char zDigest[20];
	int raw_output  = FALSE;
	const char *zFile;
	SHA1Context sCtx;
	char zBuf[8192];
	void *pHandle;
	ph7_int64 n;
	int nLen;
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
	if( nArg > 1 ){
		raw_output = ph7_value_to_bool(apArg[1]);
	}
	/* Try to open the file in read-only mode */
	pHandle = PH7_StreamOpenHandle(pCtx->pVm,pStream,zFile,PH7_IO_OPEN_RDONLY,FALSE,0,FALSE,0);
	if( pHandle == 0 ){
		VfsThrowOpenWarning(pCtx,zFile);
		ph7_result_bool(pCtx,0);
		return PH7_OK;
	}
	/* Init the SHA1 context */
	SHA1Init(&sCtx);
	/* Perform the requested operation */
	for(;;){
		n = pStream->xRead(pHandle,zBuf,sizeof(zBuf));
		if( n < 1 ){
			/* EOF or IO error,break immediately */
			break;
		}
		SHA1Update(&sCtx,(const unsigned char *)zBuf,(unsigned int)n);
	}
	/* Close the stream */
	PH7_StreamCloseHandle(pStream,pHandle);
	/* Extract the digest */
	SHA1Final(&sCtx,zDigest);
	if( raw_output ){
		/* Output raw digest */
		ph7_result_string(pCtx,(const char *)zDigest,sizeof(zDigest));
	}else{
		/* Perform a binary to hex conversion */
		SyBinToHexConsumer((const void *)zDigest,sizeof(zDigest),vfsHashConsumer,pCtx);
	}
	return PH7_OK;
}
#endif /* PH7_DISABLE_HASH_FUNC */
/*
 * array parse_ini_file(string $filename[, bool $process_sections = false [, int $scanner_mode = INI_SCANNER_NORMAL ]] )
 *  Parse a configuration file.
 * Parameters
 * $filename
 *  The filename of the ini file being parsed.
 * $process_sections
 *  By setting the process_sections parameter to TRUE, you get a multidimensional array
 *  with the section names and settings included.
 *  The default for process_sections is FALSE.
 * $scanner_mode
 *  Can either be INI_SCANNER_NORMAL (default) or INI_SCANNER_RAW.
 *  If INI_SCANNER_RAW is supplied, then option values will not be parsed.
 * Return
 *  The settings are returned as an associative array on success.
 *  Otherwise is returned.
 */
PH7_PRIVATE int PH7_builtin_parse_ini_file(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	const ph7_io_stream *pStream;
	const char *zFile;
	SyBlob sContents;
	void *pHandle;
	int nLen;
	int iMode = PH7_INI_SCANNER_NORMAL;
	sxi32 rc = PH7_OK;
	if( nArg < 1 || !ph7_value_is_string(apArg[0]) ){
		/* Missing/Invalid arguments,return FALSE */
		ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"Expecting a file path");
		ph7_result_bool(pCtx,0);
		return PH7_OK;
	}
	if( nArg > 2 && ph7_value_is_int(apArg[2]) ){
		iMode = ph7_value_to_int(apArg[2]);
		if( iMode != PH7_INI_SCANNER_NORMAL && iMode != PH7_INI_SCANNER_RAW
		 && iMode != PH7_INI_SCANNER_TYPED ){
			/* php screens the mode BEFORE touching the file */
			/* php's bare message: no `func(): ` qualifier on this one */
			PH7_VmThrowError(pCtx->pVm,0,PH7_CTX_WARNING,"Invalid scanner mode");
			ph7_result_bool(pCtx,0);
			return PH7_OK;
		}
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
	/* Try to open the file in read-only mode */
	pHandle = PH7_StreamOpenHandle(pCtx->pVm,pStream,zFile,PH7_IO_OPEN_RDONLY,FALSE,0,FALSE,0);
	if( pHandle == 0 ){
		VfsThrowOpenWarning(pCtx,zFile);
		ph7_result_bool(pCtx,0);
		return PH7_OK;
	}
	SyBlobInit(&sContents,&pCtx->pVm->sAllocator);
	/* Read the whole file */
	PH7_StreamReadWholeFile(pHandle,pStream,&sContents);
	if( SyBlobLength(&sContents) < 1 ){
		/* Empty buffer,return FALSE */
		ph7_result_bool(pCtx,0);
	}else{
		/* Process the raw INI buffer; capture an OOM abort to propagate below */
		rc = PH7_ParseIniString(pCtx,(const char *)SyBlobData(&sContents),SyBlobLength(&sContents),
			nArg > 1 ? ph7_value_to_bool(apArg[1]) : 0,iMode);
	}
	/* Close the stream */
	PH7_StreamCloseHandle(pStream,pHandle);
	/* Release the working buffer */
	SyBlobRelease(&sContents);
	/* Propagate an OOM abort so the fatal actually halts the VM */
	return rc;
}
/* ZIP archive processing moved to vfs_zip.c */
#else /* PH7_DISABLE_DISK_IO */
/*
 * Disk I/O is compiled out: this VFS hands out no resource handles, so
 * get_resource_type() has nothing that could be a "stream" and every
 * resource reports as "Unknown" (the same fallback the full build gives
 * to any non-VFS resource).
 */
PH7_PRIVATE const char * PH7_VfsResourceType(void *pResource)
{
	SXUNUSED(pResource);
	return "Unknown";
}
/* No disk I/O means no closeable handles: nothing is ever a closed resource. */
PH7_PRIVATE int PH7_VfsResourceIsClosed(void *pResource)
{
	SXUNUSED(pResource);
	return 0;
}
#endif /* PH7_DISABLE_DISK_IO */
