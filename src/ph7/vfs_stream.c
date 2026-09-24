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
 * How many bytes sit AHEAD of where the script is: the line readers' read-ahead
 * plus whatever the read filter chain has already produced and nobody has taken
 * yet. Both are past the position a script observes, so ftell(), a SEEK_CUR
 * seek and stream_get_meta_data()'s `unread_bytes` all have to discount them.
 */
static sxu32 StreamAheadBytes(io_private *pDev)
{
	sxu32 n = 0;
	if( SyBlobLength(&pDev->sBuffer) > pDev->nOfft ){
		n += SyBlobLength(&pDev->sBuffer) - pDev->nOfft;
	}
	if( SyBlobLength(&pDev->sFilt) > pDev->nFiltOfft ){
		n += SyBlobLength(&pDev->sFilt) - pDev->nFiltOfft;
	}
	return n;
}
#ifdef PH7_ENABLE_NET
/* The socket handle, declared here because stream_get_meta_data()'s labels ask
 * whether a socket has a transport under it. */
typedef struct sock_private sock_private;
struct sock_private
{
	ph7_vm *pVm;
	ph7_socket sock;
	int bEof;
	int iLastErr; /* the OS code a failed send left, for php's own notice */
	int bGeneric; /* a socketpair: no transport, and php labels it apart */
};
#endif
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
		/* php names a persistent stream apart, and that name is the only way a
		 * script can see that its handle is one. */
		return pDev->bPersist ? "persistent stream" : "stream";
	}
	if( pDev && pDev->iMagic == PROC_PRIVATE_MAGIC ){
		/* proc_open()'s handle is not a stream and php does not call it one: it
		 * answered "Unknown" here, so `get_resource_type($proc) === 'process'`
		 * — the documented way to tell a process handle from a pipe — was
		 * false. Its header IS an io_private, magic field included, which is
		 * what one probe can tell them apart by. */
		return "process";
	}
	if( pDev && pDev->iMagic == STREAM_CTX_MAGIC ){
		/* stream_context_create()'s handle, and the name php gives it. */
		return "stream-context";
	}
	if( pDev && pDev->iMagic == STREAM_FILTER_MAGIC ){
		/* stream_filter_append()'s handle. Note the SPACE: php names the context
		 * `stream-context` and the filter `stream filter`. */
		return "stream filter";
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
		iOfft -= (ph7_int64)StreamAheadBytes(pDev);
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
	iOfft -= (ph7_int64)StreamAheadBytes(pDev);
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
 * php's end-of-file flag is set AFTER THE FACT: a stream is at EOF once one of
 * its OWN reads has come back empty, and asking the question never reads. PHL
 * used to probe the device instead — a read-ahead of up to 4 KB from inside
 * feof() — which answered TRUE on a handle nothing had read yet (an empty file,
 * a fresh php://memory), answered TRUE on a WRITE-only handle because the
 * refused read looked like an end, and BLOCKED on `feof(STDIN)` with no input
 * waiting: a question about a stream is not a read of it. bEof is that flag,
 * set wherever a read here comes back with nothing and cleared by every seek.
 */
static int IoPrivateUwrapEof(io_private *pDev,int *pAnswer);
static int IoPrivateAtEof(io_private *pDev)
{
	int bEof;
	if( SyBlobLength(&pDev->sBuffer) > pDev->nOfft ){
		/* Buffered bytes are not an end. */
		return 0;
	}
	if( IoPrivateUwrapEof(pDev,&bEof) ){
		/* A userland wrapper answers the question itself — php calls its
		 * streamWrapper::stream_eof() rather than inferring anything. */
		return bEof;
	}
	return pDev->bEof != 0;
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
	rc = IoPrivateAtEof(pDev);
	/* EOF or not */
	ph7_result_bool(pCtx,rc != 0);
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
/*
 * One read from the device, with the timeout bookkeeping php does for EVERY
 * reader: `timed_out` describes the last read, so it is cleared on the way in
 * and set only by a wait that expired. Without the clear, one quiet period marks
 * a handle timed out for the rest of its life — and now that every socket
 * carries default_socket_timeout, that is every socket that ever waited. And
 * without the set being here, only fread() would ever report one: fgets(),
 * fgetc(), stream_get_line(), stream_get_contents() and fpassthru() all read
 * through their own loops.
 */
static ph7_int64 IoPrivateRawRead(io_private *pDev,void *pBuf,ph7_int64 nLen)
{
	ph7_int64 n;
	pDev->bTimedOut = 0;
	errno = 0;
	n = pDev->pStream->xRead(pDev->pHandle,pBuf,nLen);
	if( n < 0 && (errno == EAGAIN || errno == EWOULDBLOCK)
	 && pDev->bHasTimeout && !pDev->bNonBlock ){
		pDev->bTimedOut = 1;
	}
	return n;
}
/*
 * Serve a read from the FILTERED side of a handle. A filter changes the byte
 * count — base64 makes four out of three, dechunk throws whole runs away — so
 * what the chain produced cannot go straight into the caller's buffer: it waits
 * in sFilt and is handed out from there.
 *
 * The fill loop runs until sFilt holds what was asked for or the device is
 * spent, which is what keeps the caller's invariant intact: a SHORT answer here
 * still means end of file, exactly as it does for an unfiltered read.
 */
static ph7_int64 IoPrivateFilteredRead(io_private *pDev,void *pBuf,ph7_int64 nLen)
{
	phl_stream_filter *pChain = (phl_stream_filter *)pDev->pReadFilters;
	sxu32 nAvail;
	ph7_int64 n;
	while( pChain != 0 && !pDev->bFiltDone
	    && (ph7_int64)(SyBlobLength(&pDev->sFilt) - pDev->nFiltOfft) < nLen ){
		char zRaw[8192];
		ph7_int64 nAsk = (ph7_int64)sizeof(zRaw);
		ph7_int64 nRaw;
		int iStatus;
		if( pDev->nChunk > 0 && (ph7_int64)pDev->nChunk < nAsk ){
			nAsk = (ph7_int64)pDev->nChunk;
		}
		nRaw = IoPrivateRawRead(pDev,zRaw,nAsk);
		if( nRaw < 0 ){
			if( SyBlobLength(&pDev->sFilt) <= pDev->nFiltOfft ){
				/* Nothing was ever produced: the IO error is the answer. */
				return nRaw;
			}
			break;
		}
		iStatus = PH7_FilterChainProcess(pChain,zRaw,(sxu32)nRaw,
			nRaw > 0 ? PHL_PSFS_FLAG_NORMAL : PHL_PSFS_FLAG_FLUSH_CLOSE,&pDev->sFilt);
		if( nRaw == 0 ){
			/* The device is spent, and the call above was the chain's CLOSING
			 * one: running it again would make a buffering filter emit its tail
			 * twice, so the chain is finished for good. */
			pDev->bFiltDone = 1;
			break;
		}
		if( iStatus == PHL_PSFS_ERR_FATAL ){
			/* php answers the read itself as a failure once a filter says the
			 * stream is finished. */
			return -1;
		}
	}
	nAvail = SyBlobLength(&pDev->sFilt) - pDev->nFiltOfft;
	if( nAvail < 1 ){
		SyBlobReset(&pDev->sFilt);
		pDev->nFiltOfft = 0;
		return pChain != 0 ? 0 : IoPrivateRawRead(pDev,pBuf,nLen);
	}
	n = (ph7_int64)nAvail;
	if( n > nLen ){
		n = nLen;
	}
	SyMemcpy(SyBlobDataAt(&pDev->sFilt,pDev->nFiltOfft),pBuf,(sxu32)n);
	pDev->nFiltOfft += (sxu32)n;
	if( pDev->nFiltOfft >= SyBlobLength(&pDev->sFilt) ){
		SyBlobReset(&pDev->sFilt);
		pDev->nFiltOfft = 0;
	}
	return n;
}
static ph7_int64 IoPrivateDeviceRead(io_private *pDev,void *pBuf,ph7_int64 nLen)
{
	if( pDev->pReadFilters != 0 || SyBlobLength(&pDev->sFilt) > pDev->nFiltOfft ){
		/* Bytes can still be waiting after the last read filter was REMOVED:
		 * php flushes a filter on its way out and what it emitted belongs to
		 * the reader that comes next. */
		return IoPrivateFilteredRead(pDev,pBuf,nLen);
	}
	return IoPrivateRawRead(pDev,pBuf,nLen);
}
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
	nRead = IoPrivateDeviceRead(pDev,zBuf,nLen);
	if( nRead == 0
	 || (nRead > 0 && nRead < nLen && pStream->xSeek != 0 && pStream->xTell != 0
	     && pStream->xTell(pDev->pHandle) >= 0) ){
		/* A read that came back with nothing IS php's end-of-file event, and
		 * so is a SHORT one on a device that can say where it IS: php fills
		 * its buffer in a loop, so `fread($f, 100)` on a 12-byte file performs
		 * the second read that finds the end. The position query is what tells
		 * a regular file from a FIFO — both arrive here through the same file
		 * device, and a short read from a fifo, a pipe or a socket means only
		 * that less had arrived, so latching there would end
		 * `while (!feof($p)) $s .= fread($p, 8192);` with data still coming. A
		 * NEGATIVE answer is an IO error and never latches. */
		pDev->bEof = 1;
	}
	if( nRead > 0 ){
		n += nRead;
	}else if( n < 1 ){
		/* EOF or IO error */
		return nRead;
	}
	return n;
}
/*
 * Every SCRIPT-level write goes through here, because a handle can carry a
 * WRITE chain: php runs what the script wrote through the filters before the
 * device sees any of it, and a filter changes the byte count — so what reaches
 * the device is not what was handed in, while what fwrite() ANSWERS still is
 * (php reports the bytes it CONSUMED, not the bytes it emitted).
 */
PH7_PRIVATE ph7_int64 PH7_StreamWrite(io_private *pDev,const void *pData,ph7_int64 nLen)
{
	phl_stream_filter *pChain = (phl_stream_filter *)pDev->pWriteFilters;
	SyBlob sOut;
	ph7_int64 nWr;
	int iStatus;
	if( pDev->pStream == 0 || pDev->pStream->xWrite == 0 ){
		return -1;
	}
	if( pChain == 0 ){
		return pDev->pStream->xWrite(pDev->pHandle,pData,nLen);
	}
	SyBlobInit(&sOut,pDev->sBuffer.pAllocator);
	iStatus = PH7_FilterChainProcess(pChain,pData,(sxu32)nLen,PHL_PSFS_FLAG_NORMAL,&sOut);
	if( iStatus == PHL_PSFS_ERR_FATAL ){
		SyBlobRelease(&sOut);
		return -1;
	}
	nWr = 0;
	if( SyBlobLength(&sOut) > 0 ){
		nWr = pDev->pStream->xWrite(pDev->pHandle,SyBlobData(&sOut),
			(ph7_int64)SyBlobLength(&sOut));
	}
	SyBlobRelease(&sOut);
	if( nWr < 0 ){
		return -1;
	}
	/* A filter that held its input back (FEED_ME) still consumed it: php's
	 * fwrite() answers the length it was given. */
	return nLen;
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
		{
			/* php fills its read buffer one CHUNK at a time, and
			 * stream_set_chunk_size() is how a script asks for a smaller one. */
			ph7_int64 nAsk = (ph7_int64)sizeof(zBuf);
			if( pDev->nChunk > 0 && (ph7_int64)pDev->nChunk < nAsk ){
				nAsk = (ph7_int64)pDev->nChunk;
			}
			if( nMaxLen > 0 && nMaxLen < nAsk ){
				nAsk = nMaxLen;
			}
			n = IoPrivateDeviceRead(pDev,zBuf,nAsk);
		}
		if( n == 0 ){
			pDev->bEof = 1;
		}
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
	int iFlags,int use_include,ph7_value *pResource,int bPushInclude,int *pNew,const char *zCaller)
{
	void *pHandle = 0; /* cc warning */
	SyString sFile;
	ph7_value sDummy;
	int rc;
	if( pStream == 0 ){
		/* No such stream device. The armed context describes THIS open and
		 * nothing else, so it is dropped on every exit — a caller that armed one
		 * and returned early must not leave it for the next open to pick up. */
		pVm->pOpenCtx = 0;
		return 0;
	}
	/* A wrapper registered with STREAM_IS_URL speaks to the network, and php lets
	 * the configuration turn that off: allow_url_fopen for an ordinary open,
	 * allow_url_include for the one that EXECUTES what comes back — which is off
	 * by default, because including a remote file is the classic RFI. */
	if( PH7_StreamIsUrlWrapper(pStream) ){
		/* php tests BOTH, in this order: a URL wrapper is unusable at all without
		 * allow_url_fopen, and an INCLUDE needs allow_url_include on top of it. */
		const char *zIni = 0;
		if( !PH7_VmIniGetBool(pVm,"allow_url_fopen",1) ){
			zIni = "allow_url_fopen";
		}else if( bPushInclude && !PH7_VmIniGetBool(pVm,"allow_url_include",0) ){
			zIni = "allow_url_include";
		}
		if( zIni ){
			SyString sCaller;
			char zMsg[160];
			pVm->pOpenCtx = 0;
			SyStringInitFromBuf(&sCaller,zCaller ? zCaller : "",zCaller ? SyStrlen(zCaller) : 0);
			SyBufferFormat(zMsg,sizeof(zMsg),
				"%s:// wrapper is disabled in the server configuration by %s=0",
				pStream->zName,zIni);
			PH7_VmThrowError(pVm,zCaller ? &sCaller : 0,PH7_CTX_WARNING,zMsg);
			return 0;
		}
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
	/* The armed context describes exactly ONE open — every attempt of the
	 * include-path walk above included — so it is dropped here whether the open
	 * worked or not. A device that wanted it (a userland wrapper) read it while
	 * its xOpen was running. */
	pVm->pOpenCtx = 0;
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
		n = IoPrivateDeviceRead(pDev,zBuf,(ph7_int64)sizeof(zBuf));
		if( n < 1 ){
			bEof = 1;
			if( n == 0 ){
				pDev->bEof = 1;
			}
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
	errno = 0;
	nRead = PH7_StreamRead(pDev,pBuf,(ph7_int64)nLen);
	if( nRead < 0 && (errno == EAGAIN || errno == EWOULDBLOCK) ){
		/* Nothing had ARRIVED yet, which is not a failure: php answers "" for a
		 * read that could not proceed and reserves `false` for one that broke.
		 * The question is answered by errno rather than by a per-handle flag —
		 * two handles can share one descriptor (every php://stdin is fd 0), so
		 * a flag on the handle that set the mode answers wrongly for its
		 * siblings, and a genuine EBADF on a non-blocking write-only handle
		 * would come back as "" rather than false. When a TIMEOUT is what
		 * expired, php reports false and sets the metadata's `timed_out`. */
		if( pDev->bHasTimeout && !pDev->bNonBlock ){
			/* A handle in NON-BLOCKING mode is the other case: it answers "" for
			 * a read that found nothing whether or not a timeout is armed, and
			 * every socket now carries `default_socket_timeout`. */
			pDev->bTimedOut = 1;
			ph7_result_bool(pCtx,0);
		}else{
			ph7_result_string(pCtx,"",0);
		}
	}else if( nRead < 0 ){
		/* A real IO error, which is php's other false here. */
		ph7_result_bool(pCtx,0);
	}else{
		/* Make a copy of the data just read. Zero bytes is EOF, not a failure:
		 * php answers "" for it (php_stream_read returns 0 and the empty string
		 * rides through), where PHL answered FALSE — so the ordinary
		 * `while (!feof($f)) $buf .= fread($f, 8192);` loop ended on a value
		 * that means "the read failed" and a `=== false` guard fired at EOF. */
		ph7_result_string(pCtx,(const char *)pBuf,nRead > 0 ? (int)nRead : 0);
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
	PH7_StreamFilterReleaseChains(pDev);
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
	int iLen,rc,bThrew = 0;
	if( nArg < 1 || !ph7_value_is_string(apArg[0]) ){
		/* Missing/Invalid arguments,return FALSE */
		ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"Expecting a directory path");
		ph7_result_bool(pCtx,0);
		return PH7_OK;
	}
	/* php refuses a resource that is not a stream-context. Nothing CONSUMES it
	 * here — dir_opendir() over a userland wrapper is not dispatched (§7.4
	 * slice-2 (e)) — but the refusal is the argument's contract. */
	PH7_StreamCtxFromArg(pCtx,nArg,apArg,1,"$context",0,&bThrew);
	if( bThrew ){
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
		/* php's directory handles carry a mode and NO uri, and name their own
		 * ops `dir` rather than the byte-stream STDIO. */
		SetIOPrivateOpenedAs(pDev,0,0,"r",1);
		pDev->bDir = 1;
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
	phl_stream_ctx *pCtxRes;
	int rc,nLen,bThrew = 0;
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
	/* php's `?resource $context`: a resource that is not a stream-context is
	 * refused, and NULL means the DEFAULT context — never "no context at all".
	 * The armed one describes exactly this open. */
	pCtxRes = PH7_StreamCtxFromArg(pCtx,nArg,apArg,2,"$context",0,&bThrew);
	if( bThrew ){
		return PH7_OK;
	}
	PH7_StreamCtxArm(pCtx->pVm,pCtxRes);
	/* Try to open the file in read-only mode */
	pHandle = PH7_StreamOpenHandle(pCtx->pVm,pStream,zFile,PH7_IO_OPEN_RDONLY,
		use_include,nArg > 2 ? apArg[2] : 0,FALSE,0,ph7_function_name(pCtx));
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
	phl_stream_ctx *pCtxRes;
	int nLen,bThrew = 0;

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
	/* php's `?resource $context`: a resource that is not a stream-context is
	 * refused, and NULL means the DEFAULT context — never "no context at all".
	 * The armed one describes exactly this open. */
	pCtxRes = PH7_StreamCtxFromArg(pCtx,nArg,apArg,2,"$context",0,&bThrew);
	if( bThrew ){
		return PH7_OK;
	}
	PH7_StreamCtxArm(pCtx->pVm,pCtxRes);
	/* Try to open the file in read-only mode */
	pHandle = PH7_StreamOpenHandle(pCtx->pVm,pStream,zFile,PH7_IO_OPEN_RDONLY,use_include,nArg > 2 ? apArg[2] : 0,FALSE,0,ph7_function_name(pCtx));
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
		PH7_IO_OPEN_CREATE|PH7_IO_OPEN_RDWR|PH7_IO_OPEN_APPEND,FALSE,0,FALSE,0,ph7_function_name(pCtx));
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
	phl_stream_ctx *pCtxRes;
	int iFlags;
	int nLen,bThrew = 0;

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
	/* FILE_NO_DEFAULT_CONTEXT is the flag that means exactly "and do NOT fall
	 * back to the default context" — which is why it needed one to exist. */
	pCtxRes = PH7_StreamCtxFromArg(pCtx,nArg,apArg,3,"$context",
		(iFlags & 0x10) != 0,&bThrew);
	if( bThrew ){
		return PH7_OK;
	}
	PH7_StreamCtxArm(pCtx->pVm,pCtxRes);
	pHandle = PH7_StreamOpenHandle(pCtx->pVm,pStream,zFile,iOpenFlags,use_include,
		nArg > 3 ? apArg[3] : 0,FALSE,FALSE,ph7_function_name(pCtx));
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
	phl_stream_ctx *pCtxRes;
	ph7_int64 n;
	int iFlags;
	int nLen,bThrew = 0;

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
	/* Resolved here for the same reason the flag mask is: a refused $context
	 * must not strand the io_private chunk allocated below.
	 * FILE_NO_DEFAULT_CONTEXT is the flag that means exactly "and do NOT fall
	 * back to the default context" — which is why it needed one to exist. */
	pCtxRes = PH7_StreamCtxFromArg(pCtx,nArg,apArg,2,"$context",
		(iFlags & 0x10) != 0,&bThrew);
	if( bThrew ){
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
	PH7_StreamCtxArm(pCtx->pVm,pCtxRes);
	/* Try to open the file in read-only mode */
	pDev->pHandle = PH7_StreamOpenHandle(pCtx->pVm,pStream,zFile,PH7_IO_OPEN_RDONLY,use_include,nArg > 2 ? apArg[2] : 0,FALSE,0,ph7_function_name(pCtx));
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
	phl_stream_ctx *pCtxRes;
	ph7_int64 n;
	int nLen,bThrew = 0;
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
	/* php's `?resource $context`: a resource that is not a stream-context is
	 * refused, and NULL means the DEFAULT context — never "no context at all".
	 * The armed one describes exactly this open. */
	pCtxRes = PH7_StreamCtxFromArg(pCtx,nArg,apArg,2,"$context",0,&bThrew);
	if( bThrew ){
		return PH7_OK;
	}
	PH7_StreamCtxArm(pCtx->pVm,pCtxRes);
	/* Try to open the source file in a read-only mode */
	pIn = PH7_StreamOpenHandle(pCtx->pVm,pSin,zFile,PH7_IO_OPEN_RDONLY,FALSE,nArg > 2 ? apArg[2] : 0,FALSE,0,ph7_function_name(pCtx));
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
	/* php hands the ONE context to both halves of the copy. */
	PH7_StreamCtxArm(pCtx->pVm,pCtxRes);
	/* Try to open the destination file in a read-write mode */
	pOut = PH7_StreamOpenHandle(pCtx->pVm,pSout,zFile,
		PH7_IO_OPEN_CREATE|PH7_IO_OPEN_TRUNC|PH7_IO_OPEN_RDWR,FALSE,nArg > 2 ? apArg[2] : 0,FALSE,0,ph7_function_name(pCtx));
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
 * php's socket ops report a failed send THEMSELVES, as an E_NOTICE naming the
 * count, the errno and its text, before the caller ever sees the false — so a
 * write to a peer that has gone is diagnosed rather than silent. Defined with
 * the socket device further down; the write paths that can reach a socket call
 * it where php's own do.
 */
static void SockReportWriteFailure(ph7_context *pCtx,io_private *pDev,int nLen);
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
	n = (int)PH7_StreamWrite(pDev,(const void *)zString,nLen);
	if( n <  0 ){
		/* IO error,return FALSE */
		SockReportWriteFailure(pCtx,pDev,nLen);
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
	nWr = PH7_StreamWrite(pDev,(const void *)SyBlobData(&sLine),
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
	n = PH7_StreamWrite(pFdata->pIO,(const void *)zInput,nLen);
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
	SyBlobInit(&pOut->sUri,&pVm->sAllocator);
	pOut->zMode[0] = 0;
	pOut->bEof = 0;
	pOut->bDir = 0;
	pOut->bPersist = 0;
	pOut->nChunk = 8192; /* php's own default, and what stream_set_chunk_size() reports first */
	pOut->bNonBlock = 0;
	pOut->bHasTimeout = 0;
	pOut->bTimedOut = 0;
	pOut->pReadFilters = 0;
	pOut->pWriteFilters = 0;
	pOut->bFiltDone = 0;
	SyBlobInit(&pOut->sFilt,&pVm->sAllocator);
	pOut->nFiltOfft = 0;
	/* Set the magic number */
	pOut->iMagic = IO_PRIVATE_MAGIC;
}
/*
 * Record what the opener was asked for, for stream_get_meta_data()'s `uri` and
 * `mode` keys. php keeps the URI exactly as written (a relative path stays
 * relative) and the mode in a 16-byte field; passing a NULL/empty zUri leaves
 * the key out, which is how a popen() pipe reports no wrapper and no uri.
 */
PH7_PRIVATE void SetIOPrivateOpenedAs(io_private *pDev,const char *zUri,int nUriLen,const char *zMode,int nModeLen)
{
	if( pDev == 0 ){
		return;
	}
	SyBlobReset(&pDev->sUri);
	if( zUri && nUriLen > 0 ){
		SyBlobAppend(&pDev->sUri,zUri,(sxu32)nUriLen);
	}
	if( zMode && nModeLen > 0 ){
		if( nModeLen > (int)sizeof(pDev->zMode) - 1 ){
			nModeLen = (int)sizeof(pDev->zMode) - 1;
		}
		SyMemcpy(zMode,pDev->zMode,(sxu32)nModeLen);
		pDev->zMode[nModeLen] = 0;
	}else{
		pDev->zMode[0] = 0;
	}
}
/*
 * Release the IO private structure.
 */
static void ReleaseIOPrivate(ph7_context *pCtx,io_private *pDev)
{
	PH7_StreamFilterReleaseChains(pDev);
	SyBlobRelease(&pDev->sBuffer);
	SyBlobRelease(&pDev->sFilt);
	SyBlobRelease(&pDev->sUri);
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
	/* A filter outliving its handle would keep answering is_resource() and hold
	 * a pointer to a closed device; every close path releases the chains before
	 * the device goes, and this is the backstop for one that forgets. */
	PH7_StreamFilterReleaseChains(pDev);
	SyBlobRelease(&pDev->sBuffer);
	SyBlobRelease(&pDev->sFilt);
	SyBlobRelease(&pDev->sUri);
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
	/* A seek moves the DEVICE, so whatever the read chain had already produced
	 * from the old position is not what the new one answers. */
	SyBlobReset(&pDev->sFilt);
	pDev->nFiltOfft = 0;
	pDev->bFiltDone = 0;
	/* Every caller of this has just MOVED the device (a seek, a rewind, a
	 * truncate), and php clears the end-of-file flag on exactly those. */
	pDev->bEof = 0;
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
		nRead = IoPrivateDeviceRead(pDev,zBuf,nAsk);
		if( nRead < 1 ){
			if( nRead == 0 ){
				pDev->bEof = 1;
			}
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
/* The userland-wrapper pool is declared further down this file. */
static int IoPrivateIsUwrap(const ph7_io_stream *pStream);
static ph7_class_instance * IoPrivateUwrapObject(io_private *pDev);
/*
 * php names TWO things in a stream's metadata: the WRAPPER that opened it
 * (`wrapper_type`) and the ops that drive it (`stream_type`). They differ for
 * nearly every device — an ordinary file is opened by `plainfile` and driven by
 * `STDIO` — and PHL answered its own single device name for both, so neither
 * key ever matched php. A stream php opens with NO wrapper (a popen()/proc_open()
 * pipe, a socket) reports no `wrapper_type` at all; *pzWrapper stays 0 for those.
 */
static void IoPrivateStreamLabels(io_private *pDev,const char **pzWrapper,const char **pzStream)
{
	const ph7_io_stream *pS = pDev->pStream;
	*pzWrapper = 0;
	*pzStream  = "STDIO";
	if( pS == 0 ){
		return;
	}
	if( pDev->bDir ){
		*pzWrapper = "plainfile";
		*pzStream  = "dir";
		return;
	}
	if( is_php_stream(pS) ){
		*pzWrapper = "PHP";
		if( PH7_PhpStreamKind(pDev->pHandle) == PH7_IO_STREAM_OUTPUT ){
			/* php://output is the VM's output consumer, not a descriptor. */
			*pzStream = "Output";
			return;
		}
		if( PH7_PhpStreamKind(pDev->pHandle) == PH7_IO_STREAM_MEMORY ){
			/* php://memory and php://temp are ONE device here and two in php,
			 * which labels them apart; the URI is what separates them. */
			const char *zUri = (const char *)SyBlobData(&pDev->sUri);
			sxu32 nUri = SyBlobLength(&pDev->sUri);
			*pzStream = ( nUri >= sizeof("php://temp")-1
			           && SyStrnicmp(zUri,"php://temp",sizeof("php://temp")-1) == 0 )
				? "TEMP" : "MEMORY";
		}
		return;
	}
	if( is_data_stream(pS) ){
		*pzWrapper = *pzStream = "RFC2397";
		return;
	}
	if( IoPrivateIsUwrap(pS) ){
		*pzWrapper = *pzStream = "user-space";
		return;
	}
	if( pS->zName && SyStrncmp(pS->zName,"tcp",sizeof("tcp")) == 0 ){
		/* php names the socket ops and reports no wrapper for them — and names
		 * a socket with no transport under it (a pair) differently again. */
#ifdef PH7_ENABLE_NET
		*pzStream = (pDev->pHandle && ((sock_private *)pDev->pHandle)->bGeneric)
			? "generic_socket" : "tcp_socket/ssl";
#else
		*pzStream = "tcp_socket/ssl";
#endif
		return;
	}
	if( SyBlobLength(&pDev->sUri) < 1 ){
		/* Opened from a DESCRIPTOR rather than through a wrapper — a popen()
		 * or proc_open() pipe end — which is precisely when php reports
		 * neither a `wrapper_type` nor a `uri`. */
		return;
	}
	*pzWrapper = "plainfile";
}
/*
 * data:// carries its own metadata in php, and all of it comes back out of the
 * URI the wrapper parsed: `data://<mediatype>[;name=value]*[;base64],<payload>`
 * answers the media type, ONE KEY PER PARAMETER, and the base64 flag last. A
 * URI naming no media type has no `mediatype` key at all — php does not
 * substitute the RFC's default — and a repeated parameter keeps its last value.
 */
static void IoPrivateDataMeta(ph7_context *pCtx,io_private *pDev,ph7_value *pArr,ph7_value *pV)
{
	const char *zUri = (const char *)SyBlobData(&pDev->sUri);
	sxu32 nUri = SyBlobLength(&pDev->sUri);
	sxu32 nStart = 0,nComma,nSeg,i;
	int bBase64 = 0,bFirst = 1;
	if( nUri >= sizeof("data://")-1 && SyStrnicmp(zUri,"data://",sizeof("data://")-1) == 0 ){
		nStart = sizeof("data://")-1;
	}else if( nUri >= sizeof("data:")-1 && SyStrnicmp(zUri,"data:",sizeof("data:")-1) == 0 ){
		nStart = sizeof("data:")-1;
	}
	nComma = nStart;
	while( nComma < nUri && zUri[nComma] != ',' ){
		nComma++;
	}
	/* Walk the ';'-separated segments in front of the payload. */
	for( nSeg = nStart ; nSeg <= nComma ; ){
		sxu32 nEnd = nSeg;
		while( nEnd < nComma && zUri[nEnd] != ';' ){
			nEnd++;
		}
		if( bFirst ){
			if( nEnd > nSeg ){
				ph7_value_string(pV,&zUri[nSeg],(int)(nEnd - nSeg));
				ph7_array_add_strkey_elem(pArr,"mediatype",pV);
				ph7_value_reset_string_cursor(pV);
			}
			bFirst = 0;
		}else if( nEnd - nSeg == sizeof("base64")-1
		       && SyStrnicmp(&zUri[nSeg],"base64",sizeof("base64")-1) == 0 ){
			bBase64 = 1;
		}else{
			/* `name=value`; php keys the array by the name, so a repeat wins. */
			for( i = nSeg ; i < nEnd && zUri[i] != '=' ; i++ ){}
			if( i < nEnd && i > nSeg ){
				/* The name is keyed WHOLE — it has no length limit in the URI,
				 * and a clamped one files the value under a key no script can
				 * look up. */
				ph7_value *pKey = ph7_context_new_scalar(pCtx);
				if( pKey ){
					ph7_value_string(pKey,&zUri[nSeg],(int)(i - nSeg));
					ph7_value_string(pV,&zUri[i+1],(int)(nEnd - i - 1));
					ph7_array_add_elem(pArr,pKey,pV);
					ph7_value_reset_string_cursor(pV);
					ph7_context_release_value(pCtx,pKey);
				}
			}
		}
		if( nEnd >= nComma ){
			break;
		}
		nSeg = nEnd + 1;
	}
	ph7_value_bool(pV,bBase64);
	ph7_array_add_strkey_elem(pArr,"base64",pV);
}
/*
 * array stream_get_meta_data(resource $stream)
 *
 * php's own key set, in php's own order. What used to be here answered a
 * best-effort shape: `mode` and `uri` did not exist at all (so the documented
 * way to ask a handle what FILE it is on was an `Undefined array key` and
 * NULL), `eof` was hardcoded FALSE (a `while (!$m['eof'])` loop never ended),
 * `unread_bytes` was hardcoded 0, and `wrapper_type`/`stream_type` were both
 * PHL's internal device name rather than php's two different labels.
 */
PH7_PRIVATE int PH7_builtin_stream_get_meta_data(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	const char *zWrapper,*zStream;
	io_private *pDev;
	ph7_value *pArr,*pV;
	sxu32 nUnread;
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
	IoPrivateStreamLabels(pDev,&zWrapper,&zStream);
	/* Sample this BEFORE the eof probe below: php answers eof from state it
	 * already has and never reads ahead for it, so its `unread_bytes` counts
	 * only what the SCRIPT's own reads left buffered. */
	nUnread = StreamAheadBytes(pDev);
	if( is_data_stream(pDev->pStream) ){
		/* A device that answers metadata of its OWN replaces php's three
		 * defaults rather than adding to them: data:// (and php://temp, which
		 * simply has none) report no timed_out/blocked/eof at all. */
		IoPrivateDataMeta(pCtx,pDev,pArr,pV);
		ph7_value_reset_string_cursor(pV);
	}else if( is_php_stream(pDev->pStream)
	       && PH7_PhpStreamKind(pDev->pHandle) == PH7_IO_STREAM_MEMORY
	       && SyStrncmp(zStream,"TEMP",sizeof("TEMP")) == 0 ){
		/* php://temp: same rule, no keys of its own. */
	}else{
		ph7_value_bool(pV,pDev->bTimedOut != 0);
		ph7_array_add_strkey_elem(pArr,"timed_out",pV);
		/* A stream php cannot put in non-blocking mode always reports blocked;
		 * bNonBlock is only ever set for one that CAN. */
		ph7_value_bool(pV,pDev->bNonBlock == 0);
		ph7_array_add_strkey_elem(pArr,"blocked",pV);
		/* The read-ahead this performs is feof()'s own, so a script that asks
		 * for the metadata and then reads sees every byte. */
		ph7_value_bool(pV,IoPrivateAtEof(pDev) != 0);
		ph7_array_add_strkey_elem(pArr,"eof",pV);
	}
	{
		ph7_class_instance *pObj = IoPrivateUwrapObject(pDev);
		if( pObj ){
			/* php hands the wrapper INSTANCE back, which is the only way a
			 * script can reach the object serving an open userland stream. */
			ph7_value sObj;
			PH7_MemObjInit(pCtx->pVm,&sObj);
			sObj.x.pOther = pObj;
			sObj.iFlags = MEMOBJ_OBJ;
			ph7_array_add_strkey_elem(pArr,"wrapper_data",&sObj);
		}
	}
	if( zWrapper ){
		ph7_value_string(pV,zWrapper,-1);
		ph7_array_add_strkey_elem(pArr,"wrapper_type",pV);
		ph7_value_reset_string_cursor(pV);
	}
	ph7_value_string(pV,zStream,-1);
	ph7_array_add_strkey_elem(pArr,"stream_type",pV);
	ph7_value_reset_string_cursor(pV);
	ph7_value_string(pV,pDev->zMode,-1);
	ph7_array_add_strkey_elem(pArr,"mode",pV);
	ph7_value_reset_string_cursor(pV);
	/* Bytes already pulled off the device and not yet handed to the script —
	 * php's own writepos-minus-readpos, which was hardcoded 0. */
	ph7_value_int64(pV,(ph7_int64)nUnread);
	ph7_array_add_strkey_elem(pArr,"unread_bytes",pV);
	{
		/* php answers this from what the handle actually SITS ON, not from what
		 * the device could do: php://stdout is seekable into a file and not
		 * down a pipe, php://output never is, and a pipe is not. Ask the
		 * descriptor first and the device second; a USERLAND wrapper is php's
		 * one exception — its ops always carry a seek, so php always says yes. */
		int bSeekable = pDev->pStream != 0 && pDev->pStream->xSeek != 0;
		if( pDev->bDir ){
			/* php's directory ops carry a rewind, so a dir handle is seekable —
			 * and asking the FILE device where it is would hand lseek() the
			 * DIR* this handle stores where a file stores its descriptor. */
			bSeekable = 1;
		}else if( bSeekable && !IoPrivateIsUwrap(pDev->pStream) ){
			int rcSeek = PH7_StreamHandleCanSeek(pDev);
			if( rcSeek >= 0 ){
				bSeekable = rcSeek;
			}else if( pDev->pStream->xTell != 0 ){
				bSeekable = pDev->pStream->xTell(pDev->pHandle) >= 0;
			}
		}
		ph7_value_bool(pV,bSeekable);
	}
	ph7_array_add_strkey_elem(pArr,"seekable",pV);
	if( SyBlobLength(&pDev->sUri) > 0 ){
		/* php keeps the path exactly as the opener received it — a relative
		 * one stays relative — and omits the key for a stream that has none. */
		ph7_value_string(pV,(const char *)SyBlobData(&pDev->sUri),(int)SyBlobLength(&pDev->sUri));
		ph7_array_add_strkey_elem(pArr,"uri",pV);
		ph7_value_reset_string_cursor(pV);
	}
	ph7_result_value(pCtx,pArr);
	return PH7_OK;
}
/*
 * ---------------------------------------------------------------------------
 * Stream contexts (stream_context_create and the accessor family).
 *
 * php's context is a `stream-context` RESOURCE holding two things: a
 * wrapper => option => value map, and the `notification` parameter. Both
 * levels keep INSERTION order, which is the order stream_context_get_options()
 * answers in, so the store is a real nested array rather than a flat table.
 *
 * A PHL resource is a bare void*, so the struct opens with an io_private
 * header carrying its own magic (the shape proc_open()'s handle already uses)
 * and the VM owns every one it hands out.
 * ---------------------------------------------------------------------------
 */
/* Allocate one context, chained on the VM registry. */
static phl_stream_ctx * StreamCtxNew(ph7_vm *pVm)
{
	phl_stream_ctx *pRes;
	if( pVm == 0 ){
		return 0;
	}
	pRes = (phl_stream_ctx *)SyMemBackendAlloc(&pVm->sAllocator,sizeof(phl_stream_ctx));
	if( pRes == 0 ){
		return 0;
	}
	SyZero(pRes,sizeof(phl_stream_ctx));
	pRes->base.iMagic = STREAM_CTX_MAGIC;
	pRes->pVm = pVm;
	pRes->pOptions = ph7_new_array(pVm);
	if( pRes->pOptions == 0 ){
		SyMemBackendFree(&pVm->sAllocator,pRes);
		return 0;
	}
	pRes->pNext = (phl_stream_ctx *)pVm->pStreamCtx;
	pVm->pStreamCtx = (void *)pRes;
	return pRes;
}
/*
 * The context behind a ph7_value, or 0 when the value is not one. The magic
 * probe is the same in-bounds one every resource here answers to.
 */
PH7_PRIVATE phl_stream_ctx * PH7_StreamCtxFromValue(ph7_value *pVal)
{
	phl_stream_ctx *pRes;
	if( pVal == 0 || !ph7_value_is_resource(pVal) ){
		return 0;
	}
	pRes = (phl_stream_ctx *)pVal->x.pOther;
	if( pRes == 0 || pRes->base.iMagic != STREAM_CTX_MAGIC ){
		return 0;
	}
	return pRes;
}
/*
 * The per-VM DEFAULT context. php creates it on demand — the first
 * stream_context_get_default()/set_default() call — and every opener that was
 * handed no context of its own falls back to it.
 */
PH7_PRIVATE phl_stream_ctx * PH7_StreamCtxDefault(ph7_vm *pVm)
{
	if( pVm == 0 ){
		return 0;
	}
	if( pVm->pDefaultCtx == 0 ){
		pVm->pDefaultCtx = (void *)StreamCtxNew(pVm);
	}
	return (phl_stream_ctx *)pVm->pDefaultCtx;
}
/*
 * Drop every context this VM created. Called from PH7_VmReset, so a reused VM
 * (the -S server's) does not carry one request's default context into the next.
 */
PH7_PRIVATE void PH7_StreamCtxVmReset(ph7_vm *pVm)
{
	phl_stream_ctx *pRes;
	if( pVm == 0 ){
		return;
	}
	pRes = (phl_stream_ctx *)pVm->pStreamCtx;
	while( pRes ){
		phl_stream_ctx *pNext = pRes->pNext;
		if( pRes->pOptions ){
			ph7_release_value(pVm,pRes->pOptions);
		}
		if( pRes->pNotify ){
			ph7_release_value(pVm,pRes->pNotify);
		}
		/* Any ph7_value still naming this pointer must stop reporting a live
		 * context, so clear the magic before the memory goes back. */
		pRes->base.iMagic = 0;
		SyMemBackendFree(&pVm->sAllocator,pRes);
		pRes = pNext;
	}
	pVm->pStreamCtx = 0;
	pVm->pDefaultCtx = 0;
	/* Whatever an interrupted open left armed named one of those. */
	pVm->pOpenCtx = 0;
}
/* The live element of pArray under pKey, or 0 when there is none. */
static ph7_value * StreamCtxFetch(ph7_value *pArray,ph7_value *pKey)
{
	ph7_hashmap_node *pNode;
	if( pArray == 0 || (pArray->iFlags & MEMOBJ_HASHMAP) == 0 ){
		return 0;
	}
	if( PH7_HashmapLookup((ph7_hashmap *)pArray->x.pOther,pKey,&pNode) != SXRET_OK ){
		return 0;
	}
	return (ph7_value *)SySetAt(&pArray->pVm->aMemObj,pNode->nValIdx);
}
/*
 * Store one option. The wrapper's sub-array is created on first use; an
 * existing one may be SHARED with the script array it was stored from, so it
 * is separated first — otherwise setting an option would write through into
 * the caller's own array.
 */
static int StreamCtxSetOption(phl_stream_ctx *pRes,ph7_value *pWrapper,ph7_value *pName,ph7_value *pValue)
{
	ph7_value sKey,sName,sVal;
	ph7_value *pSub;
	ph7_hashmap *pMap;
	if( pRes == 0 || pRes->pOptions == 0 || pWrapper == 0 || pName == 0 || pValue == 0 ){
		return -1;
	}
	/* Every insertion below can reserve a memory object, which GROWS (and
	 * therefore moves) pVm->aMemObj — and all three arguments may point into
	 * it. Snapshot the structs first: a shallow copy is a safe insertion
	 * source, since the referent and the heap-resident blob survive the move. */
	sKey = *pWrapper; pWrapper = &sKey;
	sName = *pName;   pName = &sName;
	sVal = *pValue;   pValue = &sVal;
	pSub = StreamCtxFetch(pRes->pOptions,pWrapper);
	if( pSub == 0 || (pSub->iFlags & MEMOBJ_HASHMAP) == 0 ){
		ph7_value *pFresh = ph7_new_array(pRes->pVm);
		if( pFresh == 0 ){
			return -1;
		}
		if( PH7_HashmapInsert((ph7_hashmap *)pRes->pOptions->x.pOther,pWrapper,pFresh) != SXRET_OK ){
			ph7_release_value(pRes->pVm,pFresh);
			return -1;
		}
		ph7_release_value(pRes->pVm,pFresh);
		pSub = StreamCtxFetch(pRes->pOptions,pWrapper);
		if( pSub == 0 || (pSub->iFlags & MEMOBJ_HASHMAP) == 0 ){
			return -1;
		}
	}
	pMap = PH7_HashmapCowSeparate(pRes->pVm,pSub);
	if( pMap == 0 ){
		return -1;
	}
	return PH7_HashmapInsert(pMap,pName,pValue) == SXRET_OK ? 0 : -1;
}
/*
 * One wrapper option by name, or 0 when the context does not carry it. This is
 * the read side every consumer (the socket transports) asks through.
 */
PH7_PRIVATE ph7_value * PH7_StreamCtxOption(phl_stream_ctx *pRes,const char *zWrapper,const char *zOption)
{
	ph7_value *pSub;
	if( pRes == 0 || pRes->pOptions == 0 ){
		return 0;
	}
	pSub = ph7_array_fetch(pRes->pOptions,zWrapper,-1);
	if( pSub == 0 || (pSub->iFlags & MEMOBJ_HASHMAP) == 0 ){
		return 0;
	}
	return ph7_array_fetch(pSub,zOption,-1);
}
/*
 * php's parse_context_options: every entry must be wrappername => array, and a
 * non-array value — or an INTEGER key, which has no wrapper name at all — is
 * the ValueError below. An integer key one level DOWN has no option name, and
 * php drops that entry in silence rather than refusing the call.
 * Returns 0, or -1 once the exception has been raised.
 */
static int StreamCtxParseOptions(ph7_context *pCtx,phl_stream_ctx *pRes,ph7_value *pOptions)
{
	ph7_hashmap *pMap;
	ph7_hashmap_node *pEntry;
	if( pOptions == 0 || (pOptions->iFlags & MEMOBJ_HASHMAP) == 0 ){
		return 0;
	}
	pMap = (ph7_hashmap *)pOptions->x.pOther;
	pMap->pCur = pMap->pFirst;
	while( (pEntry = PH7_HashmapGetNextEntry(pMap)) != 0 ){
		ph7_value sKey;
		ph7_value *pVal;
		int bBad;
		PH7_MemObjInit(pRes->pVm,&sKey);
		PH7_HashmapExtractNodeKey(pEntry,&sKey);
		pVal = HashmapExtractNodeValue(pEntry);
		bBad = ( (sKey.iFlags & MEMOBJ_STRING) == 0 || pVal == 0
		      || (pVal->iFlags & MEMOBJ_HASHMAP) == 0 );
		if( bBad ){
			PH7_MemObjRelease(&sKey);
			PH7_VmThrowException(pCtx,"ValueError",
				"Options should have the form [\"wrappername\"][\"optionname\"] = $value");
			return -1;
		}
		{
			ph7_hashmap *pSub = (ph7_hashmap *)pVal->x.pOther;
			ph7_hashmap_node *pOpt;
			pSub->pCur = pSub->pFirst;
			while( (pOpt = PH7_HashmapGetNextEntry(pSub)) != 0 ){
				ph7_value sName;
				ph7_value *pOptVal;
				PH7_MemObjInit(pRes->pVm,&sName);
				PH7_HashmapExtractNodeKey(pOpt,&sName);
				pOptVal = HashmapExtractNodeValue(pOpt);
				if( (sName.iFlags & MEMOBJ_STRING) && pOptVal ){
					StreamCtxSetOption(pRes,&sKey,&sName,pOptVal);
				}
				PH7_MemObjRelease(&sName);
			}
		}
		PH7_MemObjRelease(&sKey);
	}
	return 0;
}
/*
 * php's parse_context_params: only `notification` and `options` are read, and
 * anything else in the array is ignored rather than refused. The notification
 * must be callable — php reports the same "must be an array with valid
 * callbacks as values" TypeError the callback taxonomy produces, naming
 * argument #1 whichever function was called.
 * Returns 0, or -1 once a diagnostic has been raised.
 */
static int StreamCtxParseParams(ph7_context *pCtx,phl_stream_ctx *pRes,ph7_value *pParams,const char *zArgName)
{
	ph7_value *pVal;
	if( pParams == 0 || (pParams->iFlags & MEMOBJ_HASHMAP) == 0 ){
		return 0;
	}
	pVal = ph7_array_fetch(pParams,"notification",-1);
	if( pVal ){
		char zBuf[128];
		const char *zReason = PH7_VmCallableReason(pCtx->pVm,pVal,zBuf,(int)sizeof(zBuf));
		if( zReason ){
			return PH7_VmThrowException(pCtx,"TypeError",
				"%s(): Argument #1 (%s) must be an array with valid callbacks as values, %s",
				ph7_function_name(pCtx),zArgName,zReason) == PH7_OK ? -1 : -1;
		}
		if( pRes->pNotify == 0 ){
			pRes->pNotify = ph7_new_scalar(pRes->pVm);
		}
		if( pRes->pNotify ){
			PH7_MemObjStore(pVal,pRes->pNotify);
		}
	}
	pVal = ph7_array_fetch(pParams,"options",-1);
	if( pVal ){
		if( (pVal->iFlags & MEMOBJ_HASHMAP) == 0 ){
			/* php's own wording for a params entry it cannot use. */
			return PH7_VmThrowException(pCtx,"TypeError",
				"Invalid stream/context parameter") == PH7_OK ? -1 : -1;
		}
		if( StreamCtxParseOptions(pCtx,pRes,pVal) != 0 ){
			return -1;
		}
	}
	return 0;
}
/*
 * Resolve the `$stream_or_context` first argument every accessor takes: a
 * context resource answers itself, and a STREAM answers the context it
 * carries — created on demand for the setters, the way php's does, since a
 * stream opened without one still accepts stream_context_set_option().
 * Raises php's TypeError and returns 0 for anything else.
 */
static phl_stream_ctx * StreamCtxArg(ph7_context *pCtx,ph7_value *pVal,int bCreate,
	const char *zArgName,int *pbThrew)
{
	phl_stream_ctx *pRes;
	io_private *pDev;
	*pbThrew = 1;
	if( !ph7_value_is_resource(pVal) ){
		PH7_VmThrowException(pCtx,"TypeError",
			"%s(): Argument #1 (%s) must be of type resource, %s given",
			ph7_function_name(pCtx),zArgName,ph7_type_name(pVal));
		return 0;
	}
	pRes = PH7_StreamCtxFromValue(pVal);
	if( pRes ){
		*pbThrew = 0;
		return pRes;
	}
	pDev = (io_private *)ph7_value_to_resource(pVal);
	if( IO_PRIVATE_INVALID(pDev) ){
		/* A closed handle, a process handle, anything that is neither: php
		 * refuses the call rather than answering an empty option set. */
		PH7_VmThrowException(pCtx,"TypeError",
			"%s(): Argument #1 (%s) must be a valid stream/context",
			ph7_function_name(pCtx),zArgName);
		return 0;
	}
	*pbThrew = 0;
	if( pDev->pCtxRes == 0 && bCreate ){
		pDev->pCtxRes = (void *)StreamCtxNew(pCtx->pVm);
	}
	return (phl_stream_ctx *)pDev->pCtxRes;
}
/*
 * The `$context` argument sixteen rows of aBuiltinSig[] declare and no C body
 * used to read. php's parameter is `?resource $context = null` and its rules
 * are: a resource that is NOT a stream-context is refused outright, anything
 * else non-null is the ordinary type refusal, and NULL means the DEFAULT
 * context — which php creates on demand, so an opener never runs without one.
 *
 * bNoDefault is FILE_NO_DEFAULT_CONTEXT, the flag file()/file_get_contents()/
 * file_put_contents() carry to mean exactly "and do not fall back to it".
 * Returns 0 with *pbThrew set once a diagnostic has been raised.
 */
PH7_PRIVATE phl_stream_ctx * PH7_StreamCtxFromArg(ph7_context *pCtx,int nArg,ph7_value **apArg,
	int iArg,const char *zArgName,int bNoDefault,int *pbThrew)
{
	phl_stream_ctx *pRes;
	*pbThrew = 0;
	if( iArg < nArg && apArg[iArg] && !ph7_value_is_null(apArg[iArg]) ){
		if( !ph7_value_is_resource(apArg[iArg]) ){
			*pbThrew = 1;
			PH7_VmThrowException(pCtx,"TypeError",
				"%s(): Argument #%d (%s) must be of type resource or null, %s given",
				ph7_function_name(pCtx),iArg + 1,zArgName,ph7_type_name(apArg[iArg]));
			return 0;
		}
		pRes = PH7_StreamCtxFromValue(apArg[iArg]);
		if( pRes == 0 ){
			/* php names the RESOURCE it wanted rather than the argument here. */
			*pbThrew = 1;
			PH7_VmThrowException(pCtx,"TypeError",
				"%s(): supplied resource is not a valid Stream-Context resource",
				ph7_function_name(pCtx));
			return 0;
		}
		return pRes;
	}
	return bNoDefault ? 0 : PH7_StreamCtxDefault(pCtx->pVm);
}
/*
 * Arm the context the NEXT open is to run under. PH7_StreamOpenHandle consumes
 * and clears it, so the slot describes exactly one open and a caller that never
 * set it finds nothing armed.
 */
PH7_PRIVATE void PH7_StreamCtxArm(ph7_vm *pVm,phl_stream_ctx *pRes)
{
	if( pVm ){
		pVm->pOpenCtx = (void *)pRes;
	}
}
/*
 * resource stream_context_create(?array $options = null, ?array $params = null)
 */
PH7_PRIVATE int PH7_builtin_stream_context_create(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	phl_stream_ctx *pRes = StreamCtxNew(pCtx->pVm);
	if( pRes == 0 ){
		ph7_result_bool(pCtx,0);
		return PH7_OK;
	}
	if( nArg > 0 && ph7_value_is_array(apArg[0]) ){
		if( StreamCtxParseOptions(pCtx,pRes,apArg[0]) != 0 ){
			return PH7_OK;
		}
	}
	if( nArg > 1 && ph7_value_is_array(apArg[1]) ){
		/* php names argument #1 ($options) even for a bad `notification` that
		 * arrived through $params — the error is raised against a hardcoded
		 * position, and a test that asserts the message would see it. */
		if( StreamCtxParseParams(pCtx,pRes,apArg[1],"$options") != 0 ){
			return PH7_OK;
		}
	}
	ph7_result_resource(pCtx,pRes);
	return PH7_OK;
}
/*
 * array stream_context_get_options(resource $stream_or_context)
 */
PH7_PRIVATE int PH7_builtin_stream_context_get_options(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	phl_stream_ctx *pRes;
	int bThrew;
	if( nArg < 1 ){
		ph7_result_bool(pCtx,0);
		return PH7_OK;
	}
	/* A live stream that was never given a context answers the EMPTY option set
	 * rather than refusing the call, so nothing is created here. */
	pRes = StreamCtxArg(pCtx,apArg[0],FALSE,"$stream_or_context",&bThrew);
	if( bThrew ){
		return PH7_OK;
	}
	if( pRes == 0 ){
		ph7_value *pArr = ph7_context_new_array(pCtx);
		if( pArr == 0 ){
			ph7_result_bool(pCtx,0);
			return PH7_OK;
		}
		ph7_result_value(pCtx,pArr);
		return PH7_OK;
	}
	ph7_result_value(pCtx,pRes->pOptions);
	return PH7_OK;
}
/*
 * bool stream_context_set_option(resource $context, string $wrapper, string $option_name, mixed $value)
 *
 * php also accepts the two-argument (context, options-array) spelling and
 * DEPRECATES it in 8.3 — §10 refuses what php deprecates, so an array in
 * argument #2 is the ordinary string TypeError here and the whole-array form
 * is spelled stream_context_set_options().
 */
PH7_PRIVATE int PH7_builtin_stream_context_set_option(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	phl_stream_ctx *pRes;
	int bThrew;
	if( nArg < 4 ){
		ph7_result_bool(pCtx,0);
		return PH7_OK;
	}
	pRes = StreamCtxArg(pCtx,apArg[0],TRUE,"$context",&bThrew);
	if( pRes == 0 ){
		return PH7_OK;
	}
	ph7_result_bool(pCtx,StreamCtxSetOption(pRes,apArg[1],apArg[2],apArg[3]) == 0);
	return PH7_OK;
}
/*
 * bool stream_context_set_options(resource $context, array $options)
 */
PH7_PRIVATE int PH7_builtin_stream_context_set_options(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	phl_stream_ctx *pRes;
	int bThrew;
	if( nArg < 2 ){
		ph7_result_bool(pCtx,0);
		return PH7_OK;
	}
	pRes = StreamCtxArg(pCtx,apArg[0],TRUE,"$context",&bThrew);
	if( pRes == 0 ){
		return PH7_OK;
	}
	if( StreamCtxParseOptions(pCtx,pRes,apArg[1]) != 0 ){
		return PH7_OK;
	}
	ph7_result_bool(pCtx,1);
	return PH7_OK;
}
/*
 * array stream_context_get_params(resource $stream_or_context)
 *  php answers `notification` (only when one is set) and `options`, in that
 *  order.
 */
PH7_PRIVATE int PH7_builtin_stream_context_get_params(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	phl_stream_ctx *pRes;
	ph7_value *pArr;
	int bThrew;
	if( nArg < 1 ){
		ph7_result_bool(pCtx,0);
		return PH7_OK;
	}
	pRes = StreamCtxArg(pCtx,apArg[0],TRUE,"$stream_or_context",&bThrew);
	if( pRes == 0 ){
		return PH7_OK;
	}
	pArr = ph7_context_new_array(pCtx);
	if( pArr == 0 ){
		ph7_result_bool(pCtx,0);
		return PH7_OK;
	}
	if( pRes->pNotify ){
		ph7_array_add_strkey_elem(pArr,"notification",pRes->pNotify);
	}
	ph7_array_add_strkey_elem(pArr,"options",pRes->pOptions);
	ph7_result_value(pCtx,pArr);
	return PH7_OK;
}
/*
 * bool stream_context_set_params(resource $context, array $params)
 */
PH7_PRIVATE int PH7_builtin_stream_context_set_params(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	phl_stream_ctx *pRes;
	int bThrew;
	if( nArg < 2 ){
		ph7_result_bool(pCtx,0);
		return PH7_OK;
	}
	pRes = StreamCtxArg(pCtx,apArg[0],TRUE,"$context",&bThrew);
	if( pRes == 0 ){
		return PH7_OK;
	}
	if( StreamCtxParseParams(pCtx,pRes,apArg[1],"$context") != 0 ){
		return PH7_OK;
	}
	ph7_result_bool(pCtx,1);
	return PH7_OK;
}
/*
 * resource stream_context_get_default(?array $options = null)
 * resource stream_context_set_default(array $options)
 *  Both answer the ONE default context and both MERGE their options into it —
 *  set_default is not a replacement, which is why a second call adds to what
 *  the first left.
 */
static int StreamCtxDefaultCommon(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	phl_stream_ctx *pRes = PH7_StreamCtxDefault(pCtx->pVm);
	if( pRes == 0 ){
		ph7_result_bool(pCtx,0);
		return PH7_OK;
	}
	if( nArg > 0 && ph7_value_is_array(apArg[0]) ){
		if( StreamCtxParseOptions(pCtx,pRes,apArg[0]) != 0 ){
			return PH7_OK;
		}
	}
	ph7_result_resource(pCtx,pRes);
	return PH7_OK;
}
PH7_PRIVATE int PH7_builtin_stream_context_get_default(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	return StreamCtxDefaultCommon(pCtx,nArg,apArg);
}
PH7_PRIVATE int PH7_builtin_stream_context_set_default(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	return StreamCtxDefaultCommon(pCtx,nArg,apArg);
}
/*
 * tcp:// socket stream (fsockopen / stream_socket_client). The handle is a
 * small struct carrying the OS socket plus an EOF latch, so feof() works.
 */
#ifdef PH7_ENABLE_NET
static ph7_int64 SockStreamData_Read(void *pHandle,void *pBuffer,ph7_int64 nRead)
{
	sock_private *pSock = (sock_private *)pHandle;
	int n;
	if( pSock == 0 || pSock->sock == PH7_NET_INVALID_SOCKET ){
		/* A server asked for neither BIND nor LISTEN has no socket at all, and
		 * php answers false for a read on it — the shape an ERROR takes. */
		return -1;
	}
	if( pSock->bEof ){
		return 0;
	}
	n = PH7_NetRecv(pSock->sock,pBuffer,(int)nRead,0);
	if( n == 0 ){
		/* The peer closed: THIS is the end of the stream. */
		pSock->bEof = 1;
		return 0;
	}
	if( n < 0 ){
		/* An error, and since stream_set_blocking()/stream_set_timeout() exist
		 * the ordinary one is EAGAIN — nothing had arrived YET. Latching EOF
		 * here (as this did for every n <= 0, safe only while every socket was
		 * blocking and untimed) made the first empty read close the connection
		 * for good and threw away everything the peer sent afterwards.
		 *
		 * The reader above tells "nothing yet" from "broken" by ERRNO, which a
		 * Winsock call never touches: without this the `""` a non-blocking read
		 * answers and the `timed_out` an expired one reports were both lost on
		 * Windows, and every such read came back as a plain failure. */
		errno = PH7_NetWouldBlock() ? EAGAIN : (errno != 0 ? errno : EIO);
		return -1;
	}
	return (ph7_int64)n;
}
static ph7_int64 SockStreamData_Write(void *pHandle,const void *pBuf,ph7_int64 nWrite)
{
	sock_private *pSock = (sock_private *)pHandle;
	const char *zBuf = (const char *)pBuf;
	ph7_int64 nSent = 0;
	if( pSock == 0 ){
		return -1;
	}
	if( pSock->sock == PH7_NET_INVALID_SOCKET ){
		/* Nothing to send on, and php answers 0 rather than false for it. */
		return 0;
	}
	/* php answers the number of bytes it MOVED. This used to hand back
	 * PH7_NetSendAll()'s STATUS — which is 0 on success — so every successful
	 * `fwrite($sock,$s)` answered 0 bytes written: the `=== strlen($s)` check
	 * failed on a write that worked, a partial-write retry loop never advanced,
	 * and stream_copy_to_stream() stopped after its first chunk. */
	while( nSent < nWrite ){
		int n = PH7_NetSend(pSock->sock,&zBuf[nSent],(int)(nWrite - nSent),0);
		if( n > 0 ){
			nSent += n;
			continue;
		}
		/* Nothing more can go right now. On a non-blocking or timed-out handle
		 * that is php's 0 (or the partial count), and only a write that moved
		 * NO bytes at all for a real error is php's false — which is why the
		 * count is answered here rather than the status. */
		if( PH7_NetWouldBlock() ){
			return nSent;
		}
		pSock->iLastErr = PH7_NetLastError();
		return nSent > 0 ? nSent : -1;
	}
	return nSent;
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
	sock = PH7_NetConnect(zHost,iPort,0,0,&iErrno,&zErr);
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
	pSock->iLastErr = 0;
	pSock->bGeneric = 0;
	*ppHandle = (void *)pSock;
	return PH7_OK;
}
/* php's own listen backlog for a stream server. */
#define SOCK_LISTEN_BACKLOG 128
/*
 * php's `fwrite(): Send of 4 bytes failed with errno=32 Broken pipe` — the
 * NOTICE its socket ops raise for a send that failed, which is the only
 * diagnostic a write to a departed peer produces (the return value is the same
 * false a closed handle answers). Silent for every other device: nothing else
 * here has an OS error of its own to report.
 */
static void SockReportWriteFailure(ph7_context *pCtx,io_private *pDev,int nLen)
{
#ifdef PH7_ENABLE_NET
	if( pDev && pDev->pStream == &sTCP_Stream && pDev->pHandle ){
		sock_private *pSock = (sock_private *)pDev->pHandle;
		if( pSock->iLastErr != 0 ){
			ph7_context_throw_error_format(pCtx,PH7_CTX_NOTICE,
				"Send of %d bytes failed with errno=%d %s",
				nLen,pSock->iLastErr,PH7_NetStrError(pSock->iLastErr));
			pSock->iLastErr = 0;
		}
	}
#else
	SXUNUSED(pCtx);
	SXUNUSED(pDev);
	SXUNUSED(nLen);
#endif
}
/* The settings family below owns both of these; the socket openers here are
 * declared ahead of it so one handle-wrapping routine can serve both halves. */
static io_private * StreamSettingArgNamed(ph7_context *pCtx,ph7_value *pArg,int iPos,
	const char *zName,int *pRc);
static ph7_socket * IoPrivateSocket(io_private *pDev);
/*
 * Wrap an open socket in the io_private every f* builtin drives, so a socket a
 * server accepted reads and writes exactly like one a client connected. A NULL
 * zUri is php's "opened by no name at all" — an accepted connection, which
 * reports no `uri` at all from stream_get_meta_data().
 * Answers 0 (and closes the socket) when there is no memory for the handle.
 */
static io_private * SockWrapSocket(ph7_context *pCtx,ph7_socket sock,const char *zUri,int nUri)
{
	io_private *pDev;
	sock_private *pSock;
	pDev = (io_private *)ph7_context_alloc_chunk(pCtx,sizeof(io_private),TRUE,FALSE);
	pSock = (sock_private *)SyMemBackendAlloc(&pCtx->pVm->sAllocator,sizeof(sock_private));
	if( pDev == 0 || pSock == 0 ){
		if( pSock ){
			SyMemBackendFree(&pCtx->pVm->sAllocator,pSock);
		}
		if( pDev ){
			/* Allocated with AutoRelease = FALSE, so nothing else will reclaim
			 * this chunk — it is not an io_private yet and has no buffers. */
			ph7_context_free_chunk(pCtx,pDev);
		}
		PH7_NetClose(sock);
		return 0;
	}
	pSock->pVm = pCtx->pVm;
	pSock->sock = sock;
	pSock->bEof = 0;
	pSock->iLastErr = 0;
	pSock->bGeneric = 0;
	InitIOPrivate(pCtx->pVm,&sTCP_Stream,pDev);
	/* php's feof() answers TRUE for a stream whose socket was never created. */
	pDev->bEof = (sxu8)(sock == PH7_NET_INVALID_SOCKET ? 1 : 0);
	SetIOPrivateOpenedAs(pDev,zUri,nUri,"r+",2);
	pDev->pHandle = (void *)pSock;
	return pDev;
}
/*
 * Undo a SockWrapSocket() whose partner could not be wrapped: the device's own
 * close hook frees the socket handle, and the io_private chunk goes with it.
 * Nothing has handed this out as a resource yet, so there is no ph7_value that
 * could observe it afterwards.
 */
static void SockCloseWrapped(ph7_context *pCtx,io_private *pDev)
{
	if( pDev == 0 ){
		return;
	}
	if( pDev->pStream && pDev->pStream->xClose && pDev->pHandle ){
		pDev->pStream->xClose(pDev->pHandle);
		pDev->pHandle = 0;
	}
	ReleaseIOPrivate(pCtx,pDev);
}
/* Forward: php's port rule, defined with the address parser further down. */
static int SockParsePort(const char *z,int n);
/*
 * php's `socket` context options, read into the shape net.c applies. Only the
 * ones a tcp-only, IPv4-only transport can honour are read: `bindto`, which is
 * the LOCAL address a client connects out from, `backlog`, `so_reuseport` and
 * `tcp_nodelay`. `so_broadcast` describes a datagram socket and `ipv6_v6only`
 * an address family this build has not got, so they stay on the context
 * unapplied (§7.4 slice-2 (a)).
 *
 * `bindto` is "host:port", split at the FIRST colon with an atoi() port — the
 * same address rule the server half already uses — and a spelling with no colon
 * at all is not an address, so php performs no bind and says nothing. A value
 * that is not a STRING is php's one hard failure here; everything else is a
 * warning and a connection made from wherever routing would have sent it.
 * Returns 0, or -1 with *pzErr set to php's refusal.
 */
static int SockCtxOptions(phl_stream_ctx *pCtxRes,ph7_sockopts *pOut,char *zHostBuf,int nHostBuf,
	const char **pzErr)
{
	ph7_value *pVal;
	SyZero(pOut,sizeof(*pOut));
	if( pCtxRes == 0 ){
		return 0;
	}
	pVal = PH7_StreamCtxOption(pCtxRes,"socket","backlog");
	if( pVal ){
		pOut->iBacklog = (int)ph7_value_to_int64(pVal);
	}
	pVal = PH7_StreamCtxOption(pCtxRes,"socket","so_reuseport");
	pOut->bReusePort = pVal != 0 && ph7_value_to_bool(pVal);
	pVal = PH7_StreamCtxOption(pCtxRes,"socket","tcp_nodelay");
	pOut->bNoDelay = pVal != 0 && ph7_value_to_bool(pVal);
	pVal = PH7_StreamCtxOption(pCtxRes,"socket","bindto");
	if( pVal ){
		const char *zSpec;
		int nSpec = 0,i,nHost = -1;
		if( (pVal->iFlags & MEMOBJ_STRING) == 0 ){
			*pzErr = "local_addr context option is not a string.";
			return -1;
		}
		zSpec = (const char *)SyBlobData(&pVal->sBlob);
		nSpec = (int)SyBlobLength(&pVal->sBlob);
		for( i = 0 ; i + 1 < nSpec ; i++ ){
			if( zSpec[i] == ':' ){
				nHost = i;
				pOut->iBindPort = SockParsePort(&zSpec[i+1],nSpec - i - 1);
				break;
			}
		}
		if( nHost >= 0 ){
			if( nHost >= nHostBuf ){
				nHost = nHostBuf - 1;
			}
			if( nHost > 0 ){
				SyMemcpy(zSpec,zHostBuf,(sxu32)nHost);
			}
			zHostBuf[nHost] = 0;
			pOut->zBindHost = zHostBuf;
		}
	}
	return 0;
}
/*
 * php's PERSISTENT sockets, which pfsockopen() and STREAM_CLIENT_PERSISTENT ask
 * for: a second open of the SAME address hands back the very same resource
 * rather than a second connection — `$a === $b` — and fclose() is what ends it,
 * after which the next open dials again. The key is the address as the opener
 * spelled it, so "localhost:80" and "127.0.0.1:80" are two of them.
 */
static void SockPersistKey(char *zBuf,int nBuf,int bClientForm,const char *zAddr,int nAddr)
{
	/* php prefixes the key with the FUNCTION that asked, so a pfsockopen() and a
	 * persistent stream_socket_client() of one address are two connections. */
	SyBufferFormat(zBuf,(sxu32)nBuf,"%s__%.*s",
		bClientForm ? "stream_socket_client" : "pfsockopen",nAddr,zAddr);
}
static io_private * SockPersistFind(ph7_vm *pVm,const char *zKey)
{
	VmPersistSock *aSlot = (VmPersistSock *)SySetBasePtr(&pVm->aPersistSock);
	sxu32 i;
	for( i = 0 ; i < SySetUsed(&pVm->aPersistSock) ; i++ ){
		if( aSlot[i].zKey[0] && SyStrncmp(aSlot[i].zKey,zKey,(sxu32)SyStrlen(zKey) + 1) == 0 ){
			if( !IO_PRIVATE_INVALID(aSlot[i].pDev) ){
				return aSlot[i].pDev;
			}
			/* fclose()'d since: the slot is free for the next connection. */
			aSlot[i].zKey[0] = 0;
			aSlot[i].pDev = 0;
		}
	}
	return 0;
}
static void SockPersistKeep(ph7_vm *pVm,const char *zKey,io_private *pDev)
{
	VmPersistSock *aSlot = (VmPersistSock *)SySetBasePtr(&pVm->aPersistSock);
	VmPersistSock sSlot;
	sxu32 i;
	for( i = 0 ; i < SySetUsed(&pVm->aPersistSock) ; i++ ){
		if( aSlot[i].zKey[0] == 0 || IO_PRIVATE_INVALID(aSlot[i].pDev) ){
			SyZero(&aSlot[i],sizeof(VmPersistSock));
			Systrcpy(aSlot[i].zKey,(sxu32)sizeof(aSlot[i].zKey),zKey,0);
			aSlot[i].pDev = pDev;
			return;
		}
	}
	SyZero(&sSlot,sizeof(sSlot));
	Systrcpy(sSlot.zKey,(sxu32)sizeof(sSlot.zKey),zKey,0);
	sSlot.pDev = pDev;
	SySetPut(&pVm->aPersistSock,(const void *)&sSlot);
}
/*
 * php bounds every CONNECTED socket's reads by `default_socket_timeout` from the
 * moment it is opened — a read from a peer that has gone quiet answers FALSE
 * after it, with `timed_out` set — where this engine armed nothing and waited
 * forever. That is the difference between a program that reports a dead peer and
 * one that hangs.
 *
 * A LISTENING socket is deliberately left alone: php's accept timeout is its own
 * argument and its own select(), so arming the OS receive timeout here would
 * bound `stream_socket_accept($srv, -1)` — the wait a server asks to be
 * unbounded — at sixty seconds.
 */
static void SockArmDefaultTimeout(ph7_context *pCtx,io_private *pDev)
{
	ph7_int64 iSec;
	ph7_socket *pSock = pDev ? IoPrivateSocket(pDev) : 0;
	if( pSock == 0 || *pSock == PH7_NET_INVALID_SOCKET ){
		return;
	}
	iSec = PH7_VmIniGetInt(pCtx->pVm,"default_socket_timeout",60);
	if( iSec > 0 ){
		PH7_NetSetRwTimeout(*pSock,iSec,0);
		pDev->bHasTimeout = 1;
	}
}
/*
 * The out-params every address-taking opener carries, on the path that WORKED:
 * php writes 0 and "" into them rather than leaving whatever the caller's
 * variables already held.
 */
static void SockAddressSuccess(ph7_context *pCtx,ph7_value **apArg,int nArg,int iArgErrno,int iArgErrstr)
{
	ph7_value *pTmp = ph7_context_new_scalar(pCtx);
	if( pTmp == 0 ){
		return;
	}
	if( iArgErrno >= 0 && nArg > iArgErrno ){
		ph7_value_int(pTmp,0);
		PH7_VmStoreArgByRef(pCtx->pVm,apArg[iArgErrno],pTmp);
	}
	if( iArgErrstr >= 0 && nArg > iArgErrstr ){
		ph7_value_string(pTmp,"",0);
		PH7_VmStoreArgByRef(pCtx->pVm,apArg[iArgErrstr],pTmp);
	}
}
/*
 * The failure shape the whole address-taking family shares: php words the
 * reason into BOTH the by-ref out-params and a warning naming the address as
 * the script wrote it. The `$errno` out-param stays 0 for everything the
 * ADDRESS itself is refused for — php only ever reports an OS code for a
 * connect() that reached the network.
 */
static void SockAddressFailure(ph7_context *pCtx,ph7_value **apArg,int nArg,int iArgErrno,int iArgErrstr,
	const char *zAddr,int nAddr,const char *zErr,int iErrno)
{
	ph7_value *pTmp;
	if( zErr == 0 ){
		zErr = "";
	}
	pTmp = ph7_context_new_scalar(pCtx);
	if( pTmp ){
		if( iArgErrno >= 0 && nArg > iArgErrno ){
			ph7_value_int(pTmp,iErrno);
			PH7_VmStoreArgByRef(pCtx->pVm,apArg[iArgErrno],pTmp);
		}
		if( iArgErrstr >= 0 && nArg > iArgErrstr ){
			ph7_value_string(pTmp,zErr,-1);
			PH7_VmStoreArgByRef(pCtx->pVm,apArg[iArgErrstr],pTmp);
		}
	}
	/* NOTE: ph7_context_throw_error_format already prepends "fname(): " */
	ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,"Unable to connect to %.*s (%s)",
		nAddr,zAddr,zErr);
}
/*
 * The one failure whose message names the HOST, and the one php reports TWICE:
 * its transport raises the text on its own before the opener that asked repeats
 * it inside "Unable to connect to". Composed here because net.c hands back a
 * static string and only the caller has the name to word in.
 */
static const char * SockResolveFailure(ph7_context *pCtx,const char *zHost,char *zBuf,int nBuf)
{
	SyBufferFormat(zBuf,(sxu32)nBuf,
		"php_network_getaddresses: getaddrinfo for %s failed: Name or service not known",zHost);
	ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,"%s",zBuf);
	return zBuf;
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
	int bIsUrl;               /* registered with STREAM_IS_URL: opening it is gated
	                           * by allow_url_fopen, INCLUDING it by
	                           * allow_url_include */
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
/*
 * Was this device registered with STREAM_IS_URL? Only a userland wrapper can
 * carry the flag, so the answer is a scan of the registration slots.
 */
PH7_PRIVATE int PH7_StreamIsUrlWrapper(const ph7_io_stream *pStream)
{
	int i;
	/* php marks its own data:// wrapper a URL, and that is the one that matters
	 * here: `include 'data://text/plain;base64,…'` executes bytes from the URI
	 * itself, which is why php refuses it unless allow_url_include says
	 * otherwise. php:// is NOT a URL wrapper in php and stays open. */
	if( pStream && pStream->zName
	 && SyStrlen(pStream->zName) == 4 && SyStrnicmp(pStream->zName,"data",4) == 0 ){
		return 1;
	}
	for( i = 0 ; i < PHL_UWRAP_MAX ; i++ ){
		if( g_aUwrap[i].pVm && &g_aUwrap[i].sStream == pStream ){
			return g_aUwrap[i].bIsUrl;
		}
	}
	return 0;
}
/*
 * Is this device one of the userland wrapper slots? php labels every such
 * stream `user-space` rather than by its protocol.
 */
static int IoPrivateIsUwrap(const ph7_io_stream *pStream)
{
	int i;
	for( i = 0 ; i < PHL_UWRAP_MAX ; i++ ){
		if( g_aUwrap[i].pVm && &g_aUwrap[i].sStream == pStream ){
			return 1;
		}
	}
	return 0;
}
/* Forward: the protocol dispatcher is defined just below. */
static int UwrapCall(uwrap_handle *pH,const char *zMethod,int nArg,ph7_value **apArg,
	ph7_value *pResult);
/*
 * Ask a userland wrapper whether it is at end of file — php's own
 * streamWrapper::stream_eof(), which PHL used to leave undispatched, inferring
 * the answer from a zero-length read instead. Returns 0 when the handle is not
 * a userland stream (nothing written to *pAnswer).
 */
static int IoPrivateUwrapEof(io_private *pDev,int *pAnswer)
{
	uwrap_handle *pH;
	ph7_value sRet;
	if( pDev == 0 || pDev->pHandle == 0 || !IoPrivateIsUwrap(pDev->pStream) ){
		return 0;
	}
	pH = (uwrap_handle *)pDev->pHandle;
	PH7_MemObjInit(pH->pVm,&sRet);
	if( UwrapCall(pH,"stream_eof",0,0,&sRet) != 0 ){
		/* php's streamWrapper requires the method; a class without one keeps
		 * the read-derived answer rather than being called into. */
		PH7_MemObjRelease(&sRet);
		*pAnswer = pH->bEof;
		return 1;
	}
	*pAnswer = ph7_value_to_bool(&sRet) ? 1 : 0;
	PH7_MemObjRelease(&sRet);
	return 1;
}
/*
 * The wrapper INSTANCE serving an open userland stream (php's `wrapper_data`),
 * or 0 for any other device.
 */
static ph7_class_instance * IoPrivateUwrapObject(io_private *pDev)
{
	if( pDev == 0 || pDev->pHandle == 0 || !IoPrivateIsUwrap(pDev->pStream) ){
		return 0;
	}
	return ((uwrap_handle *)pDev->pHandle)->pObj;
}
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
	{
		/* php's streamWrapper::$context, set on the serving instance BEFORE
		 * stream_open() runs — which is the whole reason a userland wrapper can
		 * be configured per open. It is exactly what the OPENER resolved: the
		 * default context substitutes for a NULL `$context` argument, so an
		 * ordinary fopen() hands a resource over; but an opener with no such
		 * argument at all (md5_file(), include) and one that carried
		 * FILE_NO_DEFAULT_CONTEXT hand over php's NULL. Substituting the default
		 * here would make that flag mean nothing.
		 * The class need not declare the slot; php adds it either way. */
		phl_stream_ctx *pOpenCtx = (phl_stream_ctx *)pVm->pOpenCtx;
		ph7_value *pCtxSlot = PH7_NativeAttr(pH->pObj,"context");
		if( pCtxSlot == 0 ){
			pCtxSlot = PH7_VmCreateDynamicAttr(pVm,pH->pObj,"context",sizeof("context")-1,0);
		}
		if( pCtxSlot ){
			if( pOpenCtx ){
				ph7_value_resource(pCtxSlot,(void *)pOpenCtx);
			}else{
				ph7_value_null(pCtxSlot);
			}
		}
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
		/* $flags: php defines exactly one bit for it, STREAM_IS_URL, and it is the
		 * whole reason the argument exists — a wrapper that says it speaks to the
		 * NETWORK is the one allow_url_fopen and allow_url_include turn off. It was
		 * declared in the signature and read by nothing, so a wrapper registered as
		 * a URL was opened and INCLUDED like a local file whatever the
		 * configuration said. */
		pSlot->bIsUrl = (nArg > 2 && (ph7_value_to_int64(apArg[2]) & PH7_STREAM_IS_URL) != 0);
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
 * php's socket address: `[transport://]host:port`. What a re-derivation gets
 * wrong here is that BOTH halves have a diagnostic of their own, and neither is
 * the other: a transport this build does not carry is not a malformed address,
 * and an address with no port is not an unknown transport.
 */
#define SOCK_ADDR_OK        0
#define SOCK_ADDR_TRANSPORT 1 /* named a transport this build has not got */
#define SOCK_ADDR_PARSE     2 /* no port separator at all */
/*
 * php's port half is `atoi()` of whatever follows the FIRST colon, and the
 * colon is looked for in every position but the LAST — which is the whole
 * difference between `127.0.0.1:` (php's "Failed to parse address") and
 * `127.0.0.1:abc` (a port of 0, i.e. one the OS picks). A re-derivation that
 * reads digits strictly refuses three addresses php accepts, and one that takes
 * the last colon reads `a:b:c` differently than php does.
 */
static int SockParsePort(const char *z,int n)
{
	int i = 0,iSign = 1,iVal = 0;
	while( i < n && (z[i] == ' ' || z[i] == '\t' || z[i] == '\n' || z[i] == '\r'
	              || z[i] == '\v' || z[i] == '\f') ){
		i++;
	}
	if( i < n && (z[i] == '+' || z[i] == '-') ){
		iSign = z[i] == '-' ? -1 : 1;
		i++;
	}
	for( ; i < n && z[i] >= '0' && z[i] <= '9' ; i++ ){
		if( iVal < 1000000000 ){
			iVal = iVal * 10 + (z[i] - '0');
		}
	}
	return iSign * iVal;
}
static int SockParseAddress(const char *zAddr,int nAddr,char *zHost,int nHostBuf,int *pPort,
	const char **pzTransport,int *pnTransport,const char **pzRest,int *pnRest)
{
	const char *zRest = zAddr;
	int nRest = nAddr,i,nHost = -1;
	*pPort = 0;
	*pzTransport = "tcp";
	*pnTransport = 3;
	for( i = 0 ; i + 2 < nAddr ; i++ ){
		if( zAddr[i] == ':' && zAddr[i+1] == '/' && zAddr[i+2] == '/' ){
			*pzTransport = zAddr;
			*pnTransport = i;
			zRest = &zAddr[i+3];
			nRest = nAddr - i - 3;
			break;
		}
	}
	*pzRest = zRest;
	*pnRest = nRest;
	if( *pnTransport != 3 || SyStrnicmp(*pzTransport,"tcp",3) != 0 ){
		return SOCK_ADDR_TRANSPORT;
	}
	for( i = 0 ; i + 1 < nRest ; i++ ){
		if( zRest[i] == ':' ){
			*pPort = SockParsePort(&zRest[i+1],nRest - i - 1);
			nHost = i;
			break;
		}
	}
	if( nHost < 0 ){
		return SOCK_ADDR_PARSE;
	}
	if( nHost >= nHostBuf ){
		nHost = nHostBuf - 1;
	}
	if( nHost > 0 ){
		SyMemcpy(zRest,zHost,(sxu32)nHost);
	}
	zHost[nHost] = 0;
	return SOCK_ADDR_OK;
}
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
	const char *zRaw,*zAddr,*zTransport,*zRest,*zErr = "";
	char zHost[256],zAddrBuf[352],zShowBuf[384],zMsg[512];
	const char *zShow;
	int nRaw,nAddr,nShow,nTransport,nRest,iPortArg = -1,iPort = 0,iErrno = 0,iTimeoutMs = 0,rc;
	int iFlags = PH7_STREAM_CLIENT_CONNECT,bPersist,bConnect;
	ph7_socket sock;
	io_private *pDev;
	int iArgErrno = bClientForm ? 1 : 2;
	int iArgErrstr = bClientForm ? 2 : 3;
	int iArgTimeout = bClientForm ? 3 : 4;
	phl_stream_ctx *pCtxRes = 0;
	ph7_sockopts sOpt;
	char zBindHost[256];
	int bThrew = 0;
	if( nArg < 1 ){
		ph7_result_bool(pCtx,0);
		return PH7_OK;
	}
	if( bClientForm ){
		/* php's `?resource $context` — fsockopen()/pfsockopen() have no such
		 * argument, so only the stream_socket_client() spelling takes one. */
		pCtxRes = PH7_StreamCtxFromArg(pCtx,nArg,apArg,5,"$context",0,&bThrew);
		if( bThrew ){
			return PH7_OK;
		}
	}
	zRaw = ph7_value_to_string(apArg[0],&nRaw);
	if( !bClientForm && nArg > 1 && !ph7_value_is_null(apArg[1]) ){
		iPortArg = ph7_value_to_int(apArg[1]);
	}
	if( bClientForm && nArg > 4 ){
		/* Declared in the signature and read by nothing until now, so the
		 * documented spellings did nothing and their constants were undefined
		 * fatals. */
		iFlags = (int)ph7_value_to_int64(apArg[4]);
	}
	/* pfsockopen() IS fsockopen() with this flag; php has no other difference
	 * between them. ASYNC_CONNECT is accepted and changes nothing here, because
	 * the connect() is blocking either way (§7.4 slice-2 (b)) — php reverts a
	 * socket it connected asynchronously to blocking mode too. */
	bPersist = bClientForm ? (iFlags & PH7_STREAM_CLIENT_PERSISTENT) != 0
		: (zFunc[0] == 'p');
	bConnect = bClientForm ? (iFlags & PH7_STREAM_CLIENT_CONNECT) != 0 : 1;
	/* php builds ONE address out of fsockopen()'s two arguments — and only when
	 * the port is a usable one, which is why `fsockopen($h)` reports the address
	 * it could not parse rather than connecting to port 0. The address it SHOWS
	 * keeps the port either way. */
	if( bClientForm || iPortArg <= 0 ){
		zAddr = zRaw;
		nAddr = nRaw;
	}else{
		nAddr = (int)SyBufferFormat(zAddrBuf,sizeof(zAddrBuf),"%.*s:%d",nRaw,zRaw,iPortArg);
		zAddr = zAddrBuf;
	}
	if( bClientForm ){
		zShow = zRaw;
		nShow = nRaw;
	}else{
		nShow = (int)SyBufferFormat(zShowBuf,sizeof(zShowBuf),"%.*s:%d",nRaw,zRaw,iPortArg);
		zShow = zShowBuf;
	}
	rc = SockParseAddress(zAddr,nAddr,zHost,(int)sizeof(zHost),&iPort,&zTransport,&nTransport,
		&zRest,&nRest);
	if( rc != SOCK_ADDR_OK ){
		if( rc == SOCK_ADDR_TRANSPORT ){
			/* php's own wording for a transport its build does not carry —
			 * which is what this engine's missing ones ARE (§7.4), and what a
			 * script reading $errstr is written against. This used to spell a
			 * message of PHL's own that no php ever answers. */
			SyBufferFormat(zMsg,sizeof(zMsg),
				"Unable to find the socket transport \"%.*s\" - did you forget to enable it when you configured PHP?",
				nTransport,zTransport);
		}else{
			SyBufferFormat(zMsg,sizeof(zMsg),"Failed to parse address \"%.*s\"",nRest,zRest);
		}
		SockAddressFailure(pCtx,apArg,nArg,iArgErrno,iArgErrstr,zShow,nShow,zMsg,0);
		ph7_result_bool(pCtx,0);
		return PH7_OK;
	}
	if( nArg > iArgTimeout && !ph7_value_is_null(apArg[iArgTimeout]) ){
		double rTimeout = ph7_value_to_double(apArg[iArgTimeout]);
		if( rTimeout > 0 ){
			iTimeoutMs = (int)(rTimeout * 1000);
		}
	}
	if( bPersist ){
		/* A live one for this address IS the answer: php hands the same resource
		 * back rather than opening a second connection to the same peer. */
		char zKey[320];
		io_private *pKept;
		SockPersistKey(zKey,sizeof(zKey),bClientForm,zAddr,nAddr);
		pKept = SockPersistFind(pCtx->pVm,zKey);
		if( pKept ){
			SockAddressSuccess(pCtx,apArg,nArg,iArgErrno,iArgErrstr);
			ph7_result_resource(pCtx,pKept);
			return PH7_OK;
		}
	}
	if( !bConnect ){
		/* php creates the socket while CONNECTING it, so a $flags without
		 * STREAM_CLIENT_CONNECT answers a stream with no socket behind it: no
		 * name at either end, reads false, writes 0, already at end of file. */
		SockAddressSuccess(pCtx,apArg,nArg,iArgErrno,iArgErrstr);
		pDev = SockWrapSocket(pCtx,PH7_NET_INVALID_SOCKET,zAddr,nAddr);
		if( pDev == 0 ){
			ph7_result_bool(pCtx,0);
			return PH7_OK;
		}
		pDev->pCtxRes = (void *)pCtxRes;
		ph7_result_resource(pCtx,pDev);
		return PH7_OK;
	}
	{
		/* php reads the `socket` options at the moment it creates the socket:
		 * so_reuseport and tcp_nodelay are a setsockopt on the fresh one, and
		 * bindto is the LOCAL address it takes before connecting. */
		const char *zOptErr = 0;
		if( SockCtxOptions(pCtxRes,&sOpt,zBindHost,(int)sizeof(zBindHost),&zOptErr) != 0 ){
			/* The one option failure php treats as a failed CONNECT rather than
			 * as a warning it can carry on past. */
			SockAddressFailure(pCtx,apArg,nArg,iArgErrno,iArgErrstr,zShow,nShow,zOptErr,0);
			ph7_result_bool(pCtx,0);
			return PH7_OK;
		}
	}
	sock = PH7_NetConnect(zHost,iPort,iTimeoutMs,&sOpt,&iErrno,&zErr);
	if( sOpt.iBindErr ){
		/* php's own wording, and NEITHER shape stops the connection: the socket
		 * goes out from wherever the routing table would have sent it. It tells
		 * the two apart — a local address that is not a numeric literal at all
		 * names the host, one the OS refused to BIND names the address it tried
		 * and the reason. */
		if( sOpt.iBindErr == PH7_SOCKOPT_BIND_RESOLVE ){
			ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,"Invalid IP Address: %s",
				sOpt.zBindHost ? sOpt.zBindHost : "");
		}else{
			/* php RE-COMPOSES the address it tried from the parts it parsed, so
			 * the quoted spelling is canonical: a `bindto` of "192.0.2.1:007"
			 * is reported as '192.0.2.1:7'. */
			ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,
				"Failed to bind to '%s:%d', system said: %s",
				sOpt.zBindHost ? sOpt.zBindHost : "",sOpt.iBindPort,
				PH7_NetStrError(sOpt.iBindErrno));
		}
	}
	if( sock == PH7_NET_INVALID_SOCKET ){
		if( iErrno == PH7_NET_ERR_RESOLVE ){
			zErr = SockResolveFailure(pCtx,zHost,zMsg,(int)sizeof(zMsg));
			iErrno = 0;
		}
		SockAddressFailure(pCtx,apArg,nArg,iArgErrno,iArgErrstr,zShow,nShow,zErr,iErrno);
		ph7_result_bool(pCtx,0);
		return PH7_OK;
	}
	SockAddressSuccess(pCtx,apArg,nArg,iArgErrno,iArgErrstr);
	/* Wrap the socket in an io_private so the whole f* family works on it. php
	 * reports the ADDRESS it opened as the handle's `uri`, which is the same
	 * one-address-out-of-two-arguments composition it connected through — so an
	 * argument naming only a host still records the port beside it. */
	pDev = SockWrapSocket(pCtx,sock,zAddr,nAddr);
	if( pDev == 0 ){
		ph7_result_bool(pCtx,0);
		return PH7_OK;
	}
	/* php attaches the opener's context to a TRANSPORT stream and to nothing
	 * else — which is why stream_context_get_options() answers for a socket and
	 * answers the empty set for a file opened through the very same call. */
	pDev->pCtxRes = (void *)pCtxRes;
	SockArmDefaultTimeout(pCtx,pDev);
	if( bPersist ){
		char zKey[320];
		SockPersistKey(zKey,sizeof(zKey),bClientForm,zAddr,nAddr);
		SockPersistKeep(pCtx->pVm,zKey,pDev);
		/* get_resource_type() names it apart, which is how a script can tell it
		 * asked for one at all. */
		pDev->bPersist = 1;
	}
	ph7_result_resource(pCtx,pDev);
	return PH7_OK;
}
/*
 * resource|false stream_socket_server(string $address, int &$error_code,
 *                    string &$error_message, int $flags = STREAM_SERVER_BIND|STREAM_SERVER_LISTEN,
 *                    ?resource $context = null)
 *
 * The name a php program becomes a SERVER through, and a loud
 * `Call to undefined function` until now — so a script that listens on a port
 * (a test double, a job runner, a line protocol) could not be spelled at all,
 * even though net.c had bind() and listen() all along.
 *
 * php's two flags are separate for a reason: BIND alone is what a datagram
 * socket wants (there is nothing to listen for), so LISTEN is what makes the
 * socket a stream server. Dropping LISTEN from a tcp:// address is therefore
 * a bound socket nothing can connect to, which is exactly what php answers.
 */
PH7_PRIVATE int PH7_builtin_stream_socket_server(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	const char *zAddr,*zTransport,*zRest,*zErr = "";
	char zHost[256];
	int nAddr,nTransport,nRest,iPort = -1,iErrno = 0,iFlags,rc;
	ph7_socket sock;
	io_private *pDev;
	phl_stream_ctx *pCtxRes;
	ph7_sockopts sOpt;
	char zBindHost[256];
	int bThrew = 0;
	if( nArg < 1 ){
		ph7_result_bool(pCtx,0);
		return PH7_OK;
	}
	pCtxRes = PH7_StreamCtxFromArg(pCtx,nArg,apArg,4,"$context",0,&bThrew);
	if( bThrew ){
		return PH7_OK;
	}
	/* The signature row declares `string $address`, so whatever arrives has
	 * already been screened; php's own ZPP then CASTS it, and refusing an int
	 * here would answer false in silence for `stream_socket_server(8080)`
	 * where php reports the address it could not parse. */
	zAddr = ph7_value_to_string(apArg[0],&nAddr);
	iFlags = nArg > 3 ? (int)ph7_value_to_int64(apArg[3])
		: (PH7_STREAM_SERVER_BIND|PH7_STREAM_SERVER_LISTEN);
	rc = SockParseAddress(zAddr,nAddr,zHost,(int)sizeof(zHost),&iPort,&zTransport,&nTransport,
		&zRest,&nRest);
	if( rc != SOCK_ADDR_OK ){
		char zMsg[512];
		if( rc == SOCK_ADDR_TRANSPORT ){
			/* php's own wording for a transport its build has not got, which is
			 * what udp://, unix:// and ssl:// are here (§7.4). */
			SyBufferFormat(zMsg,sizeof(zMsg),
				"Unable to find the socket transport \"%.*s\" - did you forget to enable it when you configured PHP?",
				nTransport,zTransport);
		}else{
			SyBufferFormat(zMsg,sizeof(zMsg),"Failed to parse address \"%.*s\"",nRest,zRest);
		}
		SockAddressFailure(pCtx,apArg,nArg,1,2,zAddr,nAddr,zMsg,0);
		ph7_result_bool(pCtx,0);
		return PH7_OK;
	}
	if( (iFlags & PH7_STREAM_SERVER_BIND) == 0 ){
		/* php creates the socket while BINDING it, so a $flags without
		 * STREAM_SERVER_BIND answers a socket stream with no socket behind it:
		 * it has no name, reads false, writes 0 and is already at end of file.
		 * It does not even resolve the host — `stream_socket_server(':1', $e,
		 * $es, 0)` is a resource in php. */
		pDev = SockWrapSocket(pCtx,PH7_NET_INVALID_SOCKET,zAddr,nAddr);
		if( pDev == 0 ){
			ph7_result_bool(pCtx,0);
			return PH7_OK;
		}
		pDev->pCtxRes = (void *)pCtxRes;
		SockAddressSuccess(pCtx,apArg,nArg,1,2);
		ph7_result_resource(pCtx,pDev);
		return PH7_OK;
	}
	if( zHost[0] == 0 ){
		/* An address with no host at all (`:8080`) is a name php asks the
		 * resolver about and is refused for — NOT a wildcard bind. Answering
		 * 0.0.0.0 for it would put a listener on every interface of the
		 * machine, which is the unsafe direction. */
		char zMsg[512];
		SockAddressFailure(pCtx,apArg,nArg,1,2,zAddr,nAddr,
			SockResolveFailure(pCtx,zHost,zMsg,(int)sizeof(zMsg)),0);
		ph7_result_bool(pCtx,0);
		return PH7_OK;
	}
	{
		/* The server half reads `backlog`, `so_reuseport` and `tcp_nodelay`;
		 * `bindto` is not one of its options, because the address argument IS
		 * where a server binds (php ignores it here too). */
		const char *zOptErr = 0;
		if( SockCtxOptions(pCtxRes,&sOpt,zBindHost,(int)sizeof(zBindHost),&zOptErr) != 0 ){
			SockAddressFailure(pCtx,apArg,nArg,1,2,zAddr,nAddr,zOptErr,0);
			ph7_result_bool(pCtx,0);
			return PH7_OK;
		}
		sOpt.zBindHost = 0;
	}
	sock = PH7_NetBind(zHost,iPort,0,(iFlags & PH7_STREAM_SERVER_LISTEN) != 0,
		SOCK_LISTEN_BACKLOG,&sOpt,&iErrno,&zErr);
	if( sock == PH7_NET_INVALID_SOCKET ){
		char zMsg[512];
		if( iErrno == PH7_NET_ERR_RESOLVE ){
			zErr = SockResolveFailure(pCtx,zHost,zMsg,(int)sizeof(zMsg));
		}
		/* php reports no OS code for a refused ADDRESS — only a connect() that
		 * reached the network carries one — so this stays 0 for every arm. */
		SockAddressFailure(pCtx,apArg,nArg,1,2,zAddr,nAddr,zErr,0);
		ph7_result_bool(pCtx,0);
		return PH7_OK;
	}
	SockAddressSuccess(pCtx,apArg,nArg,1,2);
	pDev = SockWrapSocket(pCtx,sock,zAddr,nAddr);
	if( pDev == 0 ){
		ph7_result_bool(pCtx,0);
		return PH7_OK;
	}
	pDev->pCtxRes = (void *)pCtxRes;
	ph7_result_resource(pCtx,pDev);
	return PH7_OK;
}
/*
 * resource|false stream_socket_accept(resource $socket, ?float $timeout = null,
 *                                    string &$peer_name = null)
 *
 * The other half of a server, and the one with the timing in it. php waits at
 * most `default_socket_timeout` seconds by default — NOT forever — and reports
 * an expired wait as a warning plus false, which is what lets a single-threaded
 * server do something else between connections. A negative timeout blocks.
 */
PH7_PRIVATE int PH7_builtin_stream_socket_accept(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	io_private *pDev,*pOut;
	ph7_socket *pSock,sock;
	char zPeer[128];
	int rc,bTimedOut = 0,iTimeoutMs;
	if( nArg < 1 ){
		ph7_result_bool(pCtx,0);
		return PH7_OK;
	}
	pDev = StreamSettingArgNamed(pCtx,apArg[0],1,"socket",&rc);
	if( pDev == 0 ){
		return rc;
	}
	pSock = IoPrivateSocket(pDev);
	if( pSock == 0 ){
		/* Not a socket at all. php's own answer for it reads oddly and is what
		 * a script sees: the accept never reaches the network, so there is no
		 * OS error to report and php asks its error table for code 0. */
		ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"Accept failed: Unknown error");
		ph7_result_bool(pCtx,0);
		return PH7_OK;
	}
	if( nArg > 1 && !ph7_value_is_null(apArg[1]) ){
		double rTimeout = ph7_value_to_double(apArg[1]);
		iTimeoutMs = rTimeout < 0 ? -1 : (int)(rTimeout * 1000);
	}else{
		iTimeoutMs = (int)(PH7_VmIniGetInt(pCtx->pVm,"default_socket_timeout",60) * 1000);
		if( iTimeoutMs < 0 ){
			iTimeoutMs = -1;
		}
	}
	sock = PH7_NetAcceptTimed(*pSock,iTimeoutMs,&bTimedOut,zPeer,(int)sizeof(zPeer));
	if( *pSock == PH7_NET_INVALID_SOCKET ){
		/* php waits and then reports the expiry; there is nothing to wait on. */
		bTimedOut = 1;
	}
	if( sock == PH7_NET_INVALID_SOCKET ){
		ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,"Accept failed: %s",
			bTimedOut ? "Connection timed out" : PH7_NetStrError(PH7_NetLastError()));
		ph7_result_bool(pCtx,0);
		return PH7_OK;
	}
	if( nArg > 2 ){
		ph7_value *pTmp = ph7_context_new_scalar(pCtx);
		if( pTmp ){
			ph7_value_string(pTmp,zPeer,-1);
			PH7_VmStoreArgByRef(pCtx->pVm,apArg[2],pTmp);
		}
	}
	/* php reports no `uri` for an ACCEPTED connection: nothing opened it by
	 * name, so stream_get_meta_data() has no address to answer with. */
	pOut = SockWrapSocket(pCtx,sock,0,0);
	if( pOut == 0 ){
		ph7_result_bool(pCtx,0);
		return PH7_OK;
	}
	SockArmDefaultTimeout(pCtx,pOut);
	ph7_result_resource(pCtx,pOut);
	return PH7_OK;
}
/*
 * recvfrom()'s `&$address`: the sender for a datagram, empty for a connected
 * stream that has none, and NULL for a read that did not happen — php writes it
 * on every call rather than leaving the caller's previous value in place.
 */
static void SockStoreAddress(ph7_context *pCtx,ph7_value **apArg,int nArg,int iArg,const char *zAddr)
{
	ph7_value *pTmp;
	if( iArg >= nArg ){
		return;
	}
	pTmp = ph7_context_new_scalar(pCtx);
	if( pTmp == 0 ){
		return;
	}
	if( zAddr ){
		ph7_value_string(pTmp,zAddr,-1);
	}else{
		ph7_value_null(pTmp);
	}
	PH7_VmStoreArgByRef(pCtx->pVm,apArg[iArg],pTmp);
}
/*
 * bool stream_socket_shutdown(resource $stream, int $mode)
 *
 * The half-close: "I am done SENDING" without closing a handle the program
 * still wants to read from, which is how every request/response protocol tells
 * its peer the request is over. Nothing else can say it — fclose() takes the
 * read side with it.
 */
PH7_PRIVATE int PH7_builtin_stream_socket_shutdown(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	io_private *pDev;
	ph7_socket *pSock;
	ph7_int64 iHow;
	int rc;
	if( nArg < 2 ){
		ph7_result_bool(pCtx,0);
		return PH7_OK;
	}
	pDev = StreamSettingArgNamed(pCtx,apArg[0],1,"stream",&rc);
	if( pDev == 0 ){
		return rc;
	}
	iHow = ph7_value_to_int64(apArg[1]);
	if( iHow != PH7_STREAM_SHUT_RD && iHow != PH7_STREAM_SHUT_WR && iHow != PH7_STREAM_SHUT_RDWR ){
		/* php names the three constants rather than the numbers behind them. */
		return PH7_VmThrowException(pCtx,"ValueError",
			"%s(): Argument #2 ($mode) must be one of STREAM_SHUT_RD, STREAM_SHUT_WR, or STREAM_SHUT_RDWR",
			ph7_function_name(pCtx));
	}
	pSock = IoPrivateSocket(pDev);
	if( pSock == 0 || *pSock == PH7_NET_INVALID_SOCKET ){
		/* Not a socket: php answers false in silence, since there is no
		 * direction to shut down. */
		ph7_result_bool(pCtx,0);
		return PH7_OK;
	}
	rc = PH7_NetShutdown(*pSock,(int)iHow) == PH7_OK;
	if( rc && iHow != PH7_STREAM_SHUT_WR && PH7_NetAtEnd(*pSock) ){
		/* The read side is gone AND nothing is queued behind it, so this handle
		 * is at its end: php answers feof() for a socket by probing it, and a
		 * `while (!feof($s))` drain loop after a half-close would otherwise spin
		 * on a stream that can never answer again. Bytes that HAD arrived are
		 * still handed over — which is why the answer is probed rather than
		 * assumed, and why the device's own latch stays clear. */
		pDev->bEof = 1;
	}
	ph7_result_bool(pCtx,rc);
	return PH7_OK;
}
/*
 * string|false stream_socket_recvfrom(resource $socket, int $length, int $flags = 0,
 *                                    string &$address = null)
 * int|false stream_socket_sendto(resource $socket, string $data, int $flags = 0,
 *                               string $address = "")
 *
 * The pair that reaches the socket UNDERNEATH the stream: php's own asks the
 * socket rather than the handle's read buffer, which is why `STREAM_PEEK` can
 * look at bytes without consuming them (nothing else in the family can) and why
 * a recvfrom() on a handle a line read has already buffered WAITS for more.
 * The `$address` is what a datagram carries and a connected stream does not.
 */
PH7_PRIVATE int PH7_builtin_stream_socket_recvfrom(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	io_private *pDev;
	ph7_socket *pSock;
	ph7_int64 nLen;
	char zAddr[128],*zBuf;
	int rc,iFlags = 0,n;
	if( nArg < 2 ){
		ph7_result_bool(pCtx,0);
		return PH7_OK;
	}
	pDev = StreamSettingArgNamed(pCtx,apArg[0],1,"socket",&rc);
	if( pDev == 0 ){
		return rc;
	}
	nLen = ph7_value_to_int64(apArg[1]);
	if( nLen < 1 ){
		return PH7_VmThrowException(pCtx,"ValueError",
			"%s(): Argument #2 ($length) must be greater than 0",ph7_function_name(pCtx));
	}
	if( nArg > 2 ){
		iFlags = (int)ph7_value_to_int64(apArg[2]);
	}
	pSock = IoPrivateSocket(pDev);
	if( pSock == 0 || *pSock == PH7_NET_INVALID_SOCKET ){
		SockStoreAddress(pCtx,apArg,nArg,3,0);
		ph7_result_bool(pCtx,0);
		return PH7_OK;
	}
	if( nLen > 0x7FFFFFF0 ){
		nLen = 0x7FFFFFF0;
	}
	zBuf = (char *)ph7_context_alloc_chunk(pCtx,(unsigned int)nLen,FALSE,TRUE);
	if( zBuf == 0 ){
		return PH7_ContextMemoryError(pCtx);
	}
	zAddr[0] = 0;
	n = PH7_NetRecvFrom(*pSock,zBuf,(int)nLen,iFlags,zAddr,(int)sizeof(zAddr));
	/* php writes the out-param on every call: the sender's address for a read
	 * that happened (empty for a connected stream, which has none to report) and
	 * NULL for one that did not — never the caller's previous value. */
	SockStoreAddress(pCtx,apArg,nArg,3,n < 0 ? 0 : zAddr);
	if( n < 0 ){
		ph7_result_bool(pCtx,0);
	}else{
		ph7_result_string(pCtx,zBuf,n);
	}
	ph7_context_free_chunk(pCtx,zBuf);
	return PH7_OK;
}
PH7_PRIVATE int PH7_builtin_stream_socket_sendto(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	io_private *pDev;
	ph7_socket *pSock;
	const char *zData,*zSentTo = "";
	char zHost[256];
	int rc,iFlags = 0,nData,n,iPort = 0,nSentTo = 0,iErr = 0;
	if( nArg < 2 ){
		ph7_result_bool(pCtx,0);
		return PH7_OK;
	}
	pDev = StreamSettingArgNamed(pCtx,apArg[0],1,"socket",&rc);
	if( pDev == 0 ){
		return rc;
	}
	zData = ph7_value_to_string(apArg[1],&nData);
	if( nArg > 2 ){
		iFlags = (int)ph7_value_to_int64(apArg[2]);
	}
	zHost[0] = 0;
	if( nArg > 3 && ph7_value_is_string(apArg[3]) ){
		int nAddr,i,nHost = -1;
		const char *zAddr = ph7_value_to_string(apArg[3],&nAddr);
		if( nAddr > 0 ){
			/* php parses THIS address without looking for a transport at all —
			 * the first colon is the separator, so `udp://1.2.3.4:53` names the
			 * host "udp" — and an address it cannot turn into a sockaddr is a
			 * refusal rather than a send to the connected peer, which is where
			 * the bytes would otherwise silently go. */
			for( i = 0 ; i + 1 < nAddr ; i++ ){
				if( zAddr[i] == ':' ){
					iPort = SockParsePort(&zAddr[i+1],nAddr - i - 1);
					nHost = i;
					break;
				}
			}
			if( nHost < 0 ){
				ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,
					"Failed to parse `%.*s' into a valid network address",nAddr,zAddr);
				ph7_result_bool(pCtx,0);
				return PH7_OK;
			}
			if( nHost >= (int)sizeof(zHost) ){
				nHost = (int)sizeof(zHost) - 1;
			}
			if( nHost > 0 ){
				SyMemcpy(zAddr,zHost,(sxu32)nHost);
			}
			zHost[nHost] = 0;
			zSentTo = zAddr;
			nSentTo = nAddr;
		}
	}
	pSock = IoPrivateSocket(pDev);
	if( pSock == 0 || *pSock == PH7_NET_INVALID_SOCKET ){
		/* php answers -1 here rather than false: this one reports the send()
		 * result, and it never made a call. */
		ph7_result_int(pCtx,-1);
		return PH7_OK;
	}
	n = PH7_NetSendTo(*pSock,(const void *)zData,nData,iFlags,zHost,iPort,&iErr);
	if( iErr == PH7_NET_ERR_RESOLVE ){
		/* php says it three times for one failure — the resolver's own text, the
		 * name it could not resolve, and the address it therefore could not
		 * parse — and answers FALSE rather than the -1 a failed send gives. */
		char zMsg[512];
		SockResolveFailure(pCtx,zHost,zMsg,(int)sizeof(zMsg));
		ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,"Failed to resolve `%s': %s",zHost,zMsg);
		ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,
			"Failed to parse `%.*s' into a valid network address",nSentTo,zSentTo);
		ph7_result_bool(pCtx,0);
		return PH7_OK;
	}
	if( n < 0 ){
		/* php reports the OS text and hands back the -1 send() answered — this
		 * one never answers false, which is why a caller compares it against 0
		 * rather than testing it for truth. The trailing newline is php's own. */
		ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,"%s\n",
			PH7_NetStrError(PH7_NetLastError()));
	}
	ph7_result_int(pCtx,n);
	return PH7_OK;
}
/*
 * array|false stream_socket_pair(int $domain, int $type, int $protocol)
 *
 * Two connected sockets with no address between them — the two-way pipe a
 * program hands a child, or a test double hands the code under test. Which
 * $domain works is the OS's answer and not php's: POSIX has AF_UNIX and refuses
 * AF_INET, and Windows is the other way round (php emulates the pair over the
 * loopback there, and so does this).
 */
PH7_PRIVATE int PH7_builtin_stream_socket_pair(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	ph7_socket aSock[2];
	io_private *apDev[2];
	ph7_value *pArr,*pVal;
	int iErrno = 0,i;
	if( nArg < 3 ){
		ph7_result_bool(pCtx,0);
		return PH7_OK;
	}
	if( PH7_NetSocketPair((int)ph7_value_to_int64(apArg[0]),(int)ph7_value_to_int64(apArg[1]),
		(int)ph7_value_to_int64(apArg[2]),aSock,&iErrno) != PH7_OK ){
		/* php reports the OS code and its text, in that order and in brackets. */
		ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,"Failed to create sockets: [%d]: %s",
			iErrno,PH7_NetStrError(iErrno));
		ph7_result_bool(pCtx,0);
		return PH7_OK;
	}
	pArr = ph7_context_new_array(pCtx);
	pVal = ph7_context_new_scalar(pCtx);
	apDev[0] = apDev[1] = 0;
	if( pArr == 0 || pVal == 0 ){
		PH7_NetClose(aSock[0]);
		PH7_NetClose(aSock[1]);
		return PH7_ContextMemoryError(pCtx);
	}
	for( i = 0 ; i < 2 ; i++ ){
		/* No uri: nothing opened these by name, which is what php reports. */
		apDev[i] = SockWrapSocket(pCtx,aSock[i],0,0);
		if( apDev[i] == 0 ){
			/* SockWrapSocket closed the one it could not wrap; the OTHER end is
			 * still ours to close, wrapped or not. */
			if( i == 0 ){
				PH7_NetClose(aSock[1]);
			}else{
				SockCloseWrapped(pCtx,apDev[0]);
			}
			return PH7_ContextMemoryError(pCtx);
		}
		/* A pair has no transport of its own, and php labels it apart from a
		 * tcp:// stream for exactly that reason. */
		((sock_private *)apDev[i]->pHandle)->bGeneric = 1;
		SockArmDefaultTimeout(pCtx,apDev[i]);
	}
	for( i = 0 ; i < 2 ; i++ ){
		ph7_value_resource(pVal,apDev[i]);
		ph7_array_add_elem(pArr,0,pVal);
	}
	ph7_result_value(pCtx,pArr);
	return PH7_OK;
}
/*
 * string|false stream_socket_get_name(resource $socket, bool $remote)
 *
 * Which address this socket sits on (`$remote` false) or is talking to (true).
 * It is the only way to learn the port a server bound with `:0` actually got,
 * so a test that needs a free port had to guess one without it.
 */
PH7_PRIVATE int PH7_builtin_stream_socket_get_name(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	io_private *pDev;
	ph7_socket *pSock;
	char zName[128];
	int rc;
	if( nArg < 2 ){
		ph7_result_bool(pCtx,0);
		return PH7_OK;
	}
	pDev = StreamSettingArgNamed(pCtx,apArg[0],1,"socket",&rc);
	if( pDev == 0 ){
		return rc;
	}
	pSock = IoPrivateSocket(pDev);
	if( pSock == 0 || PH7_NetSockName(*pSock,ph7_value_to_bool(apArg[1]),zName,(int)sizeof(zName)) != PH7_OK ){
		/* php answers false for a stream that is not a socket, and for the peer
		 * of a socket that is not connected — an unaccepted server. */
		ph7_result_bool(pCtx,0);
		return PH7_OK;
	}
	ph7_result_string(pCtx,zName,-1);
	return PH7_OK;
}
#endif /*
 * The stream SETTINGS family. Every one of these was a loud
 * `Call to undefined function` — so a program that puts a socket in
 * non-blocking mode, bounds a read with a timeout, or asks whether a stream
 * can be locked before calling flock() did not run at all.
 *
 * The shared preamble: php refuses a non-resource with a TypeError naming the
 * parameter, and an already-closed handle the same way.
 */
static io_private * StreamSettingArgNamed(ph7_context *pCtx,ph7_value *pArg,int iPos,
	const char *zName,int *pRc)
{
	io_private *pDev;
	*pRc = PH7_OK;
	if( !ph7_value_is_resource(pArg) ){
		*pRc = PH7_VmThrowException(pCtx,"TypeError",
			"%s(): Argument #%d ($%s) must be of type resource, %s given",
			ph7_function_name(pCtx),iPos,zName,ph7_type_name(pArg));
		return 0;
	}
	pDev = (io_private *)ph7_value_to_resource(pArg);
	if( IO_PRIVATE_INVALID(pDev) ){
		*pRc = PH7_VmThrowException(pCtx,"TypeError",
			"%s(): Argument #%d ($%s) must be an open stream resource",
			ph7_function_name(pCtx),iPos,zName);
		return 0;
	}
	return pDev;
}
/* The whole settings family names its one handle `$stream`; the copy names two. */
static io_private * StreamSettingArg(ph7_context *pCtx,ph7_value *pArg,int *pRc)
{
	return StreamSettingArgNamed(pCtx,pArg,1,"stream",pRc);
}
/* The same screen, for the filter family in vfs_filter.c. */
PH7_PRIVATE io_private * PH7_StreamHandleArg(ph7_context *pCtx,ph7_value *pArg,int iPos,
	const char *zName,int *pRc)
{
	return StreamSettingArgNamed(pCtx,pArg,iPos,zName,pRc);
}
/* The tcp:// socket behind a handle, or 0 for any other device. */
static ph7_socket * IoPrivateSocket(io_private *pDev)
{
#ifdef PH7_ENABLE_NET
	if( pDev->pStream == &sTCP_Stream && pDev->pHandle ){
		return &((sock_private *)pDev->pHandle)->sock;
	}
#endif
	SXUNUSED(pDev); /* cc warning when NET is off */
	return 0;
}
/*
 * bool stream_set_blocking(resource $stream, bool $enable)
 *
 * php sets the mode AT the descriptor and answers TRUE either way; a stream
 * with no descriptor — a memory buffer, a data:// payload — keeps reporting
 * itself blocked, which is why the flag is only recorded when it took. On
 * Windows php's plain-files device has no O_NONBLOCK to set, so every such
 * stream (a file, a pipe, php://stdin) answers FALSE there.
 */
PH7_PRIVATE int PH7_builtin_stream_set_blocking(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	io_private *pDev;
	int rc,bEnable,fd;
	ph7_socket *pSock;
	if( nArg < 2 ){
		ph7_result_bool(pCtx,0);
		return PH7_OK;
	}
	pDev = StreamSettingArg(pCtx,apArg[0],&rc);
	if( pDev == 0 ){
		return rc;
	}
	bEnable = ph7_value_to_bool(apArg[1]);
	pSock = IoPrivateSocket(pDev);
	if( pSock ){
#ifdef PH7_ENABLE_NET
		if( *pSock == PH7_NET_INVALID_SOCKET ){
			/* No socket to set the mode on: php's own answer is FALSE, which is
			 * the one place this family reports a setting that did not take. */
			ph7_result_bool(pCtx,0);
			return PH7_OK;
		}
		PH7_NetSetBlocking(*pSock,bEnable);
#endif
		pDev->bNonBlock = (sxu8)(bEnable ? 0 : 1);
	}else{
#ifdef __WINNT__
		if( PH7_StreamIsPlainDevice(pDev) ){
			ph7_result_bool(pCtx,0);
			return PH7_OK;
		}
#endif
		fd = PH7_StreamPosixFd(pDev);
		if( fd >= 0 ){
#ifndef __WINNT__
			int iFlags = fcntl(fd,F_GETFL,0);
			if( iFlags >= 0 ){
				if( bEnable ){
					iFlags &= ~O_NONBLOCK;
				}else{
					iFlags |= O_NONBLOCK;
				}
				if( fcntl(fd,F_SETFL,iFlags) == 0 ){
					pDev->bNonBlock = (sxu8)(bEnable ? 0 : 1);
				}
			}
#endif
		}
	}
	ph7_result_bool(pCtx,1);
	return PH7_OK;
}
/*
 * bool stream_set_timeout(resource $stream, int $seconds, int $microseconds = 0)
 *
 * php answers TRUE only for a stream whose transport HAS a timeout — a socket —
 * and FALSE for every file, pipe and memory buffer, because there is nothing
 * to wait on. Silently accepting it for a file would tell a caller its read is
 * bounded when it is not.
 */
PH7_PRIVATE int PH7_builtin_stream_set_timeout(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	io_private *pDev;
	ph7_socket *pSock;
	int rc;
	if( nArg < 2 ){
		ph7_result_bool(pCtx,0);
		return PH7_OK;
	}
	pDev = StreamSettingArg(pCtx,apArg[0],&rc);
	if( pDev == 0 ){
		return rc;
	}
	pSock = IoPrivateSocket(pDev);
	if( pSock == 0 ){
		ph7_result_bool(pCtx,0);
		return PH7_OK;
	}
#ifdef PH7_ENABLE_NET
	{
		ph7_int64 iSec = ph7_value_to_int64(apArg[1]);
		ph7_int64 iUsec = nArg > 2 ? ph7_value_to_int64(apArg[2]) : 0;
		if( iSec < 0 ){
			iSec = 0;
		}
		if( iUsec < 0 ){
			iUsec = 0;
		}
		PH7_NetSetRwTimeout(*pSock,iSec,iUsec);
		/* An expired read answers FALSE and says so through the metadata;
		 * without the armed flag it is indistinguishable from a non-blocking
		 * one, which answers "". */
		pDev->bHasTimeout = 1;
		pDev->bTimedOut = 0;
	}
#endif
	ph7_result_bool(pCtx,1);
	return PH7_OK;
}
/*
 * int stream_set_chunk_size(resource $stream, int $size)
 *
 * Answers the PREVIOUS size, which is what makes the setting restorable, and
 * refuses a non-positive one the way php does.
 */
PH7_PRIVATE int PH7_builtin_stream_set_chunk_size(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	io_private *pDev;
	ph7_int64 nSize;
	int rc;
	if( nArg < 2 ){
		ph7_result_int(pCtx,-1);
		return PH7_OK;
	}
	pDev = StreamSettingArg(pCtx,apArg[0],&rc);
	if( pDev == 0 ){
		return rc;
	}
	nSize = ph7_value_to_int64(apArg[1]);
	if( nSize < 1 ){
		return PH7_VmThrowException(pCtx,"ValueError",
			"stream_set_chunk_size(): Argument #2 ($size) must be greater than 0");
	}
	if( nSize > (ph7_int64)SXI32_HIGH ){
		/* php's own ceiling: the size is an int on its side, and storing a
		 * larger one made the NEXT call report a size no caller ever set. */
		return PH7_VmThrowException(pCtx,"ValueError",
			"stream_set_chunk_size(): Argument #2 ($size) is too large");
	}
	ph7_result_int64(pCtx,(ph7_int64)pDev->nChunk);
	pDev->nChunk = (sxu32)nSize;
	return PH7_OK;
}
/*
 * int stream_set_read_buffer(resource $stream, int $size)
 * int stream_set_write_buffer(resource $stream, int $size)  [set_file_buffer]
 *
 * php's stream layer has no stdio buffer left to hand these to: the read side
 * answers 0 (accepted) and the write side -1 (unsupported), for every stream
 * and every size. Both are still validated arguments, so a bad handle is the
 * same TypeError the rest of the family raises.
 */
PH7_PRIVATE int PH7_builtin_stream_set_read_buffer(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	int rc = PH7_OK;
	if( nArg < 2 ){
		ph7_result_int(pCtx,-1);
		return PH7_OK;
	}
	if( StreamSettingArg(pCtx,apArg[0],&rc) == 0 ){
		return rc;
	}
	ph7_result_int(pCtx,0);
	return PH7_OK;
}
PH7_PRIVATE int PH7_builtin_stream_set_write_buffer(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	int rc = PH7_OK;
	if( nArg < 2 ){
		ph7_result_int(pCtx,-1);
		return PH7_OK;
	}
	if( StreamSettingArg(pCtx,apArg[0],&rc) == 0 ){
		return rc;
	}
	ph7_result_int(pCtx,-1);
	return PH7_OK;
}
/*
 * int|false stream_copy_to_stream(resource $from, resource $to,
 *                                 ?int $length = null, int $offset = 0)
 *
 * The everyday way to move bytes between two open streams, and a loud
 * `Call to undefined function` here until now — so the workaround was
 * `fwrite($to, stream_get_contents($from))`, which reads the WHOLE source into
 * memory first. A NULL or negative $length is "the rest"; a POSITIVE $offset
 * seeks the source first and is php's only failure shape short of a broken
 * write.
 */
PH7_PRIVATE int PH7_builtin_stream_copy_to_stream(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	io_private *pFrom,*pTo;
	ph7_int64 nWant = -1,nOfft = 0,nTotal = 0;
	char zBuf[8192];
	int rc;
	if( nArg < 2 ){
		ph7_result_bool(pCtx,0);
		return PH7_OK;
	}
	pFrom = StreamSettingArgNamed(pCtx,apArg[0],1,"from",&rc);
	if( pFrom == 0 ){
		return rc;
	}
	pTo = StreamSettingArgNamed(pCtx,apArg[1],2,"to",&rc);
	if( pTo == 0 ){
		return rc;
	}
	if( pFrom->pStream == 0 || pFrom->pStream->xRead == 0
	 || pTo->pStream == 0 || pTo->pStream->xWrite == 0 ){
		ph7_result_bool(pCtx,0);
		return PH7_OK;
	}
	if( nArg > 2 && !ph7_value_is_null(apArg[2]) ){
		nWant = ph7_value_to_int64(apArg[2]);
	}
	if( nArg > 3 ){
		nOfft = ph7_value_to_int64(apArg[3]);
	}
	if( nOfft > 0 ){
		/* php seeks the SOURCE and gives up loudly when it cannot: a pipe has
		 * no position to move to, and silently copying from wherever it
		 * happens to be would answer for a different slice of the stream. */
		if( pFrom->pStream->xSeek == 0
		 || pFrom->pStream->xSeek(pFrom->pHandle,nOfft,0/*SEEK_SET*/) != PH7_OK ){
			if( pFrom->pStream->xSeek == 0 ){
				ph7_context_throw_error(pCtx,PH7_CTX_WARNING,
					"stream_copy_to_stream(): Stream does not support seeking");
			}
			ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,
				"stream_copy_to_stream(): Failed to seek to position %qd in the stream",nOfft);
			ph7_result_bool(pCtx,0);
			return PH7_OK;
		}
		ResetIOPrivate(pFrom);
	}
	if( nWant == 0 ){
		ph7_result_int(pCtx,0);
		return PH7_OK;
	}
#ifdef __WINNT__
	/* An unfiltered plain-file source goes through php's memory-mapped copy,
	 * whose Windows view at the end of the file is a failure: see
	 * PH7_WinFileMapsEmptyView(). */
	if( pFrom->pStream == &sWinFileStream && pFrom->pReadFilters == 0 && pFrom->pWriteFilters == 0
	 && PH7_WinFileMapsEmptyView(pFrom->pHandle,SyBlobLength(&pFrom->sBuffer) > pFrom->nOfft
			? (ph7_int64)(SyBlobLength(&pFrom->sBuffer) - pFrom->nOfft) : 0) ){
		ph7_result_bool(pCtx,0);
		return PH7_OK;
	}
#endif
	/* The destination may be sitting past its own line readers' read-ahead;
	 * the write has to land where the SCRIPT is, the rule fwrite() follows. */
	if( pTo->nOfft < SyBlobLength(&pTo->sBuffer) && pTo->pStream->xSeek ){
		pTo->pStream->xSeek(pTo->pHandle,
			-(ph7_int64)(SyBlobLength(&pTo->sBuffer) - pTo->nOfft),1/*SEEK_CUR*/);
		ResetIOPrivate(pTo);
	}
	for(;;){
		ph7_int64 nAsk = (ph7_int64)sizeof(zBuf);
		ph7_int64 nRead,nWr;
		if( nWant > 0 && nWant - nTotal < nAsk ){
			nAsk = nWant - nTotal;
		}
		if( nAsk < 1 ){
			break;
		}
		nRead = PH7_StreamRead(pFrom,zBuf,nAsk);
		if( nRead < 1 ){
			break;
		}
		nWr = PH7_StreamWrite(pTo,(const void *)zBuf,nRead);
		if( nWr < 0 ){
			break;
		}
		nTotal += nWr;
		if( nWr < nRead ){
			break;
		}
	}
	ph7_result_int64(pCtx,nTotal);
	return PH7_OK;
}
/*
 * int|false stream_select(?array &$read, ?array &$write, ?array &$except,
 *                         ?int $seconds, ?int $microseconds = null)
 *
 * The name that makes a program WAIT on several streams at once, and the reason
 * the settings family that shipped beside it had nothing to wait with: a
 * non-blocking read tells you a stream is not ready, and only this tells you
 * WHEN it becomes ready. It is what a proc_open() pipe pump, a socket server
 * loop and every event loop written in php is built on, and it was a loud
 * `Call to undefined function`.
 *
 * php's own shape, and the parts of it a re-derivation misses: the arrays are
 * REWRITTEN in place to hold only the ready entries, under their original keys;
 * a stream that cannot be represented as a descriptor is a warning naming its
 * TYPE, not a failure; nothing selectable at all is an Error rather than 0; and
 * a stream whose own read buffer still holds bytes is answered READY without
 * asking the OS at all — which is the difference between a loop that drains a
 * buffered handle and one that waits forever for data it has already read.
 */
#if !defined(__WINNT__) || defined(PH7_ENABLE_NET)
#define STREAM_SELECT_OK 1
#ifdef __UNIXES__
#include <sys/select.h>
#include <sys/time.h>
#endif
#endif
#define SEL_READ   0
#define SEL_WRITE  1
#define SEL_EXCEPT 2
/* What one walk over an argument is for. The order matters: php COUNTS the
 * already-buffered readable handles before it waits, and only rewrites the
 * arrays once it knows which answer it is giving. */
#define SELM_COLLECT  0 /* put every representable handle in its fd_set */
#define SELM_BUFFERED 1 /* keep the handles whose own buffer still holds bytes */
#define SELM_READY    2 /* keep the handles select() reported */
#define SELM_CLEAR    3 /* keep nothing: php empties the sets it is not answering */
typedef struct stream_select_ctx stream_select_ctx;
struct stream_select_ctx
{
	ph7_context *pCtx;
#ifdef STREAM_SELECT_OK
	fd_set aSet[3];    /* read / write / except, as select() takes them */
#endif
	int iMaxFd;
	int nSelectable;   /* entries that could be represented at all */
	int iWhich;        /* the set being walked (SEL_*) */
	int iMode;         /* SELM_*: what this walk is FOR */
	int nReady;
	int bBadEntry;     /* an entry that is not a stream at all */
	int bBadClosed;    /* ... and whether it was a CLOSED one (php words the two apart) */
	ph7_value *pOut;   /* the rebuilt array, while harvesting */
};
/*
 * What a select can WAIT on for this handle: the POSIX descriptor, or the
 * SOCKET, which is the only waitable thing a stream carries on Windows (the
 * file devices hold a HANDLE there, and select() cannot take one — a recorded
 * platform difference, §7.4). Answers -1 for a device with neither: a memory
 * buffer, a data:// payload, a userland wrapper.
 */
static ph7_int64 IoPrivateSelectHandle(io_private *pDev)
{
	int fd;
#ifdef PH7_ENABLE_NET
	ph7_socket *pSock = IoPrivateSocket(pDev);
	if( pSock ){
		return *pSock == PH7_NET_INVALID_SOCKET ? -1 : (ph7_int64)*pSock;
	}
#endif
	fd = PH7_StreamPosixFd(pDev);
	return fd < 0 ? -1 : (ph7_int64)fd;
}
/* Bytes this handle has already pulled off the device and not yet handed over. */
static sxu32 IoPrivateUnread(io_private *pDev)
{
	return StreamAheadBytes(pDev);
}
static void StreamSelectAdd(stream_select_ctx *pSel,ph7_int64 h)
{
#ifdef STREAM_SELECT_OK
#ifdef __WINNT__
	/* A Windows fd_set is an ARRAY of sockets, so what bounds it is how many
	 * are in it already rather than the value of this one. */
	if( pSel->aSet[pSel->iWhich].fd_count >= FD_SETSIZE ){
		return;
	}
	FD_SET((SOCKET)h,&pSel->aSet[pSel->iWhich]);
#else
	if( h < 0 || h >= (ph7_int64)FD_SETSIZE ){
		/* php ignores a descriptor an fd_set cannot hold (its own
		 * PHP_SAFE_FD_SET); writing past one corrupts the stack. */
		return;
	}
	FD_SET((int)h,&pSel->aSet[pSel->iWhich]);
#endif
	if( h > (ph7_int64)pSel->iMaxFd ){
		pSel->iMaxFd = (int)h;
	}
#else
	SXUNUSED(pSel);
	SXUNUSED(h);
#endif
}
static int StreamSelectIsSet(stream_select_ctx *pSel,ph7_int64 h)
{
#ifdef STREAM_SELECT_OK
#ifdef __WINNT__
	return FD_ISSET((SOCKET)h,&pSel->aSet[pSel->iWhich]) ? 1 : 0;
#else
	if( h < 0 || h >= (ph7_int64)FD_SETSIZE ){
		return 0;
	}
	return FD_ISSET((int)h,&pSel->aSet[pSel->iWhich]) ? 1 : 0;
#endif
#else
	SXUNUSED(pSel);
	SXUNUSED(h);
	return 0;
#endif
}
/*
 * One entry of one array: collected on the way in, harvested on the way out.
 * php never stops for an entry it cannot use — the diagnostics are remembered
 * and raised once the whole set is known, because whether the array held
 * ANYTHING selectable decides which of them php raises.
 */
static int StreamSelectWalk(ph7_value *pKey,ph7_value *pValue,void *pUserData)
{
	stream_select_ctx *pSel = (stream_select_ctx *)pUserData;
	io_private *pDev;
	ph7_int64 h;
	if( !ph7_value_is_resource(pValue) ){
		pSel->bBadEntry = 1;
		/* php words a value that is not a resource apart from a resource that is
		 * no longer open, and raises one per bad entry — so the LAST one seen is
		 * the message that reaches the caller. */
		pSel->bBadClosed = 0;
		return PH7_OK;
	}
	pDev = (io_private *)ph7_value_to_resource(pValue);
	if( IO_PRIVATE_INVALID(pDev) ){
		pSel->bBadEntry = pSel->bBadClosed = 1;
		return PH7_OK;
	}
	if( pSel->iMode == SELM_BUFFERED ){
		/* Deliberately BEFORE the descriptor lookup: php's shortcut lets a
		 * readable stream with no descriptor at all take part (a userland
		 * wrapper a line read has filled the buffer of), and answering 0 for one
		 * would sleep out the whole timeout over bytes the script already has. */
		if( IoPrivateUnread(pDev) > 0 ){
			if( pSel->pOut ){
				ph7_array_add_elem(pSel->pOut,pKey,pValue);
			}
			pSel->nReady++;
		}
		return PH7_OK;
	}
	h = IoPrivateSelectHandle(pDev);
	if( h < 0 ){
		if( pSel->iMode == SELM_COLLECT ){
			const char *zWrapper,*zLabel;
			IoPrivateStreamLabels(pDev,&zWrapper,&zLabel);
			ph7_context_throw_error_format(pSel->pCtx,PH7_CTX_WARNING,
				"Cannot represent a stream of type %s as a select()able descriptor",zLabel);
		}
		return PH7_OK;
	}
	if( pSel->iMode == SELM_COLLECT ){
		pSel->nSelectable++;
		StreamSelectAdd(pSel,h);
		return PH7_OK;
	}
	if( pSel->iMode == SELM_READY && StreamSelectIsSet(pSel,h) ){
		if( pSel->pOut ){
			ph7_array_add_elem(pSel->pOut,pKey,pValue);
		}
		pSel->nReady++;
	}
	return PH7_OK;
}
/* The wait itself, over the pair the caller's numbers were normalised into. */
static int StreamSelectWait(stream_select_ctx *pSel,ph7_int64 iSec,ph7_int64 iUsec,int bBlock,
	int *pErrno)
{
#ifdef STREAM_SELECT_OK
	struct timeval tv,*pTv = 0;
	int rc;
	if( !bBlock ){
		tv.tv_sec = (long)iSec;
		tv.tv_usec = (long)iUsec;
		pTv = &tv;
	}
	rc = select(pSel->iMaxFd + 1,&pSel->aSet[SEL_READ],&pSel->aSet[SEL_WRITE],
		&pSel->aSet[SEL_EXCEPT],pTv);
	if( rc < 0 && pErrno ){
#ifdef __WINNT__
		*pErrno = WSAGetLastError();
#else
		*pErrno = errno;
#endif
	}
	return rc;
#else
	/* No select() to call: a Windows build with no socket layer. */
	SXUNUSED(pSel);
	SXUNUSED(iSec);
	SXUNUSED(iUsec);
	SXUNUSED(bBlock);
	if( pErrno ){ *pErrno = 0; }
	return -1;
#endif
}
/* Walk one of the three arguments, if it IS one. */
static void StreamSelectEach(stream_select_ctx *pSel,ph7_value **apArg,int nArg,int iArg,int iWhich,
	int iMode)
{
	if( iArg >= nArg || apArg[iArg] == 0 || !ph7_value_is_array(apArg[iArg]) ){
		return;
	}
	pSel->iWhich = iWhich;
	pSel->iMode = iMode;
	ph7_array_walk(apArg[iArg],StreamSelectWalk,pSel);
}
/* Rebuild one argument from the entries that came back ready. php REPLACES the
 * array either way, so a set with nothing ready comes back empty. */
static int StreamSelectStore(stream_select_ctx *pSel,ph7_value **apArg,int nArg,int iArg,int iWhich,
	int iMode)
{
	if( iArg >= nArg || apArg[iArg] == 0 || !ph7_value_is_array(apArg[iArg]) ){
		return PH7_OK;
	}
	pSel->pOut = ph7_context_new_array(pSel->pCtx);
	if( pSel->pOut == 0 ){
		/* Leaving the caller's array alone would answer that every entry is
		 * ready, which is the one wrong answer this function must not give. */
		return PH7_ContextMemoryError(pSel->pCtx);
	}
	StreamSelectEach(pSel,apArg,nArg,iArg,iWhich,iMode);
	PH7_VmStoreArgByRef(pSel->pCtx->pVm,apArg[iArg],pSel->pOut);
	pSel->pOut = 0;
	return PH7_OK;
}
PH7_PRIVATE int PH7_builtin_stream_select(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	stream_select_ctx sSel;
	ph7_int64 iSec = 0,iUsec = 0;
	int bBlock = 1,iErrno = 0,rc,i;
	SyZero(&sSel,sizeof(sSel));
	sSel.pCtx = pCtx;
	sSel.iMaxFd = -1;
#ifdef STREAM_SELECT_OK
	for( i = 0 ; i < 3 ; i++ ){
		FD_ZERO(&sSel.aSet[i]);
	}
#endif
	StreamSelectEach(&sSel,apArg,nArg,0,SEL_READ,SELM_COLLECT);
	StreamSelectEach(&sSel,apArg,nArg,1,SEL_WRITE,SELM_COLLECT);
	StreamSelectEach(&sSel,apArg,nArg,2,SEL_EXCEPT,SELM_COLLECT);
	if( sSel.nSelectable < 1 ){
		/* php's own wording, and it carries no function name. It is the answer
		 * for three NULLs, for empty arrays, and for arrays holding nothing
		 * this engine can wait on — the caller asked to wait for nothing. */
		return PH7_VmThrowException(pCtx,"ValueError","No stream arrays were passed");
	}
	if( sSel.bBadEntry ){
		/* Raised only once the arrays are known to hold something to wait on —
		 * the empty-arrays Error wins over it — and BEFORE the timeout is
		 * looked at, which is the order php's own pending-exception check
		 * produces. */
		return PH7_VmThrowException(pCtx,"TypeError",
			"%s(): supplied %s is not a valid stream resource",
			ph7_function_name(pCtx),sSel.bBadClosed ? "resource" : "argument");
	}
	if( nArg > 3 && !ph7_value_is_null(apArg[3]) ){
		iSec = ph7_value_to_int64(apArg[3]);
		bBlock = 0;
		if( iSec < 0 ){
			return PH7_VmThrowException(pCtx,"ValueError",
				"%s(): Argument #4 ($seconds) must be greater than or equal to 0",
				ph7_function_name(pCtx));
		}
	}
	if( nArg > 4 && !ph7_value_is_null(apArg[4]) ){
		iUsec = ph7_value_to_int64(apArg[4]);
		if( bBlock ){
			/* php refuses the pair rather than guessing which one meant it: a
			 * NULL $seconds is "wait forever", and there is no such thing as
			 * waiting forever for five microseconds. */
			if( iUsec != 0 ){
				return PH7_VmThrowException(pCtx,"ValueError",
					"%s(): Argument #5 ($microseconds) must be null when argument #4 ($seconds) is null",
					ph7_function_name(pCtx));
			}
		}else if( iUsec < 0 ){
			return PH7_VmThrowException(pCtx,"ValueError",
				"%s(): Argument #5 ($microseconds) must be greater than or equal to 0",
				ph7_function_name(pCtx));
		}
	}
	if( iUsec > 999999 ){
		/* php carries the overflow into the seconds, because a tv_usec of a
		 * million or more is what Solaris and the BSDs refuse outright — so
		 * `stream_select($r, $w, $x, 0, 1500000)` waits a second and a half
		 * rather than failing. */
		iSec += iUsec / 1000000;
		iUsec %= 1000000;
	}
	/* php's own shortcut, and it comes BEFORE the wait: a handle whose buffer
	 * still holds bytes the script has not taken is ready NOW, whatever the OS
	 * would say about its descriptor — the device has nothing left to report.
	 * COUNTED first and stored second, because the count is what decides
	 * whether the arrays are rewritten from the buffers or from the wait. */
	StreamSelectEach(&sSel,apArg,nArg,0,SEL_READ,SELM_BUFFERED);
	if( sSel.nReady > 0 ){
		sSel.nReady = 0;
		if( StreamSelectStore(&sSel,apArg,nArg,0,SEL_READ,SELM_BUFFERED) != PH7_OK
		/* php answers only the readable ones then, and empties the other two. */
		 || StreamSelectStore(&sSel,apArg,nArg,1,SEL_WRITE,SELM_CLEAR) != PH7_OK
		 || StreamSelectStore(&sSel,apArg,nArg,2,SEL_EXCEPT,SELM_CLEAR) != PH7_OK ){
			return PH7_ContextMemoryError(pCtx);
		}
		ph7_result_int(pCtx,sSel.nReady);
		return PH7_OK;
	}
	rc = StreamSelectWait(&sSel,iSec,iUsec,bBlock,&iErrno);
	if( rc < 0 ){
#if defined(__WINNT__) && defined(PH7_ENABLE_NET)
		const char *zErr = PH7_NetStrError(iErrno);
#else
		const char *zErr = VfsStrerror(iErrno);
#endif
		ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,"Unable to select [%d]: %s (max_fd=%d)",
			iErrno,zErr,sSel.iMaxFd);
		ph7_result_bool(pCtx,0);
		return PH7_OK;
	}
	if( StreamSelectStore(&sSel,apArg,nArg,0,SEL_READ,SELM_READY) != PH7_OK
	 || StreamSelectStore(&sSel,apArg,nArg,1,SEL_WRITE,SELM_READY) != PH7_OK
	 || StreamSelectStore(&sSel,apArg,nArg,2,SEL_EXCEPT,SELM_READY) != PH7_OK ){
		return PH7_ContextMemoryError(pCtx);
	}
	/* The COUNT is select()'s own, not the entries kept: two array members can
	 * name one descriptor, and php answers what the OS said. */
	ph7_result_int(pCtx,rc);
	return PH7_OK;
}
/*
 * array stream_get_transports(void)
 *
 * The transports a stream_socket_client()/fsockopen() address may name. php's
 * own list is what its build registered, so this is what THIS engine can open:
 * the ssl/tls/udp/unix set is a recorded scope gap (§7.4), and answering for
 * transports that are not there would tell a script a connection will work
 * when it cannot.
 */
PH7_PRIVATE int PH7_builtin_stream_get_transports(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	ph7_value *pArr,*pV;
	SXUNUSED(nArg);
	SXUNUSED(apArg);
	pArr = ph7_context_new_array(pCtx);
	pV = ph7_context_new_scalar(pCtx);
	if( pArr == 0 || pV == 0 ){
		ph7_result_null(pCtx);
		return PH7_OK;
	}
#ifdef PH7_ENABLE_NET
	ph7_value_string(pV,"tcp",-1);
	ph7_array_add_elem(pArr,0,pV);
#endif
	ph7_result_value(pCtx,pArr);
	return PH7_OK;
}
/*
 * bool stream_supports_lock(resource $stream)
 *
 * The question flock() answers with a warning if you get it wrong: only a
 * device with a real lock operation can be locked, so a memory buffer and a
 * data:// payload are false.
 */
PH7_PRIVATE int PH7_builtin_stream_supports_lock(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	io_private *pDev;
	int rc;
	if( nArg < 1 ){
		ph7_result_bool(pCtx,0);
		return PH7_OK;
	}
	pDev = StreamSettingArg(pCtx,apArg[0],&rc);
	if( pDev == 0 ){
		return rc;
	}
	/* php locks at the DESCRIPTOR, so anything with one can be locked even
	 * when the device exposes no lock operation of its own (php://stdout, a
	 * pipe); a memory buffer and a data:// payload have neither and are the
	 * false answers. */
	ph7_result_bool(pCtx,pDev->bDir == 0
		&& ((pDev->pStream != 0 && pDev->pStream->xLock != 0)
		    || PH7_StreamPosixFd(pDev) >= 0));
	return PH7_OK;
}
/*
 * bool stream_is_local(resource|string $stream)
 *
 * php answers from the WRAPPER, not from the path: a stream opened by a URL
 * wrapper is not local, one opened by no wrapper at all (a pipe) is not local
 * either, and everything else — including php:// and a path naming a scheme
 * nobody registered — is.
 */
PH7_PRIVATE int PH7_builtin_stream_is_local(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	const ph7_io_stream *pStream;
	if( nArg < 1 ){
		ph7_result_bool(pCtx,0);
		return PH7_OK;
	}
	if( ph7_value_is_string(apArg[0]) ){
		static const char * const azUrlScheme[] = { "http://", "https://", "ftp://", "ftps://" };
		int nLen,i;
		const char *zPath = ph7_value_to_string(apArg[0],&nLen);
		if( nLen > (int)sizeof("file://")-1
		 && SyStrnicmp(zPath,"file://",sizeof("file://")-1) == 0
		 && zPath[sizeof("file://")-1] != '/'
		 && SyStrnicmp(zPath,"file://localhost/",sizeof("file://localhost/")-1) != 0 ){
			/* `file://host/path` names a REMOTE host, which php refuses rather
			 * than reading as a local path — so the answer is not local. */
			ph7_result_bool(pCtx,0);
			return PH7_OK;
		}
		for( i = 0 ; i < (int)(sizeof(azUrlScheme)/sizeof(azUrlScheme[0])) ; i++ ){
			int nScheme = (int)SyStrlen(azUrlScheme[i]);
			if( nLen >= nScheme && SyStrnicmp(zPath,azUrlScheme[i],(sxu32)nScheme) == 0 ){
				/* php registers these as URL wrappers whether or not this
				 * engine can OPEN them (http:// is a recorded gap, §7.4), and
				 * "is this path local?" has to answer for the scheme rather
				 * than for what happens to be implemented — the unsafe
				 * direction is answering TRUE about a remote URL. */
				ph7_result_bool(pCtx,0);
				return PH7_OK;
			}
		}
		pStream = PH7_VmGetStreamDevice(pCtx->pVm,&zPath,nLen);
		/* An unregistered scheme has no wrapper to ask, and php answers TRUE
		 * for it — the path is taken at face value. */
		ph7_result_bool(pCtx,pStream == 0 || !PH7_StreamIsUrlWrapper(pStream));
		return PH7_OK;
	}
	{
		int rc;
		io_private *pDev = StreamSettingArg(pCtx,apArg[0],&rc);
		const char *zWrapper,*zLabel;
		if( pDev == 0 ){
			return rc;
		}
		IoPrivateStreamLabels(pDev,&zWrapper,&zLabel);
		/* No wrapper (a popen() pipe, a socket) is php's other "not local". */
		ph7_result_bool(pCtx,zWrapper != 0 && !PH7_StreamIsUrlWrapper(pDev->pStream));
	}
	return PH7_OK;
}
/* PH7_ENABLE_NET */
PH7_PRIVATE int PH7_builtin_fopen(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	const ph7_io_stream *pStream;
	const char *zUri,*zMode;
	ph7_value *pResource;
	io_private *pDev;
	phl_stream_ctx *pCtxRes;
	int iLen,imLen,bThrew = 0;
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
	/* php's `?resource $context`: a resource that is not a stream-context is
	 * refused, and NULL means the DEFAULT context — never "no context at all".
	 * Resolved before the io_private chunk below, which a throw could not
	 * release. */
	pCtxRes = PH7_StreamCtxFromArg(pCtx,nArg,apArg,3,"$context",0,&bThrew);
	if( bThrew ){
		return PH7_OK;
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
	PH7_StreamCtxArm(pCtx->pVm,pCtxRes);
	/* Try to get a handle */
	pDev->pHandle = PH7_StreamOpenHandle(pCtx->pVm,pStream,zUri,iOpenFlags,
		nArg > 2 ? ph7_value_to_bool(apArg[2]) : FALSE,pResource,FALSE,0,ph7_function_name(pCtx));
	if( pDev->pHandle == 0 ){
		VfsThrowOpenWarning(pCtx,zUri);
		ph7_result_bool(pCtx,0);
		ReleaseIOPrivate(pCtx,pDev);
		return PH7_OK;
	}
	/* Remember what we were asked for: stream_get_meta_data() reports both.
	 * The URI is the ORIGINAL argument, not the scheme-stripped remainder
	 * PH7_VmGetStreamDevice() advanced zUri past. */
	{
		int nUri;
		const char *zOrig = ph7_value_to_string(apArg[0],&nUri);
		const char *zMeta = zMode;
		int nMeta = imLen;
		if( is_php_stream(pStream)
		 && PH7_PhpStreamKind(pDev->pHandle) == PH7_IO_STREAM_OUTPUT ){
			/* php://output has one mode whatever it was asked for. */
			zMeta = "wb";
			nMeta = 2;
		}else if( is_php_stream(pStream)
		 && PH7_PhpStreamKind(pDev->pHandle) == PH7_IO_STREAM_MEMORY ){
			/* php's memory streams do not keep the mode they were opened with:
			 * a buffer is readable and writable either way, so php reports the
			 * one it actually built. */
			int i,bWrite = 0,bAppend = imLen > 0 && (zMode[0] == 'a' || zMode[0] == 'A');
			for( i = 0 ; i < imLen ; i++ ){
				if( zMode[i] == 'w' || zMode[i] == 'W' || zMode[i] == 'a'
				 || zMode[i] == 'A' || zMode[i] == '+' ){
					bWrite = 1;
				}
			}
			zMeta = bWrite ? (bAppend ? "a+b" : "w+b") : "rb";
			nMeta = (int)SyStrlen(zMeta);
		}
		SetIOPrivateOpenedAs(pDev,zOrig,nUri,zMeta,nMeta);
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
		/* The WRITE chain gets its closing call while the device is still open:
		 * a filter that buffers has nowhere else to put its tail, and php's own
		 * close flushes before it closes. */
		PH7_StreamFilterReleaseChains(pDev);
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
	pHandle = PH7_StreamOpenHandle(pCtx->pVm,pStream,zFile,PH7_IO_OPEN_RDONLY,FALSE,0,FALSE,0,ph7_function_name(pCtx));
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
	pHandle = PH7_StreamOpenHandle(pCtx->pVm,pStream,zFile,PH7_IO_OPEN_RDONLY,FALSE,0,FALSE,0,ph7_function_name(pCtx));
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
	pHandle = PH7_StreamOpenHandle(pCtx->pVm,pStream,zFile,PH7_IO_OPEN_RDONLY,FALSE,0,FALSE,0,ph7_function_name(pCtx));
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
/* No streams means no stream contexts either, but PH7_VmReset still calls this. */
PH7_PRIVATE void PH7_StreamCtxVmReset(ph7_vm *pVm)
{
	SXUNUSED(pVm);
}
/* Same for the filter registry. */
PH7_PRIVATE void PH7_StreamFilterVmReset(ph7_vm *pVm)
{
	SXUNUSED(pVm);
}
#endif /* PH7_DISABLE_DISK_IO */
