# src/ph7/vfs_stream.c

<style>code, pre { background: none !important; white-space: pre !important; width: 100% !important; display: inline-block !important; } td { border: none !important; margin-top: 0 !important; margin-bottom: 0 !important; padding-top: 0 !important; padding-bottom: 0 !important; }</style>

Coverage: 3399/4128 lines (82.34%)

[Root index](../../index.md) | [Directory index](index.md)

|   Hits | Line | Source |
| -----: | ---: | :--- |
|      - |    1 | `/**` |
|      - |    2 | ` * SPDX-FileCopyrightText: 2011, 2012, 2013, 2014 Symisc Systems <licensing@symisc.net>` |
|      - |    3 | ` * SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>` |
|      - |    4 | ` * SPDX-License-Identifier: BSD-3-Clause` |
|      - |    5 | ` */` |
|      - |    6 | `#include "ph7int.h"` |
|      - |    7 | `#include <stdio.h>` |
|      - |    8 | `#include <errno.h>` |
|      - |    9 | `#include <string.h>` |
|      - |   10 |  |
|      - |   11 | `#ifdef __UNIXES__` |
|      - |   12 | `#include <unistd.h>` |
|      - |   13 | `#include <sys/wait.h>` |
|      - |   14 | `#include <fcntl.h>` |
|      - |   15 | `#include <signal.h>` |
|      - |   16 | `#endif` |
|      - |   17 | `#ifndef PH7_DISABLE_DISK_IO` |
|      - |   18 | `/*` |
|      - |   19 | ` * Section:` |
|      - |   20 | ` *    IO stream implementation.` |
|      - |   21 | ` * Status:` |
|      - |   22 | ` *    Stable.` |
|      - |   23 | ` */` |
|      - |   24 | `/* Forward declaration */` |
|      - |   25 | `static void ResetIOPrivate(io_private *pDev);` |
|      - |   26 | `/*` |
|      - |   27 | ` * How many bytes sit AHEAD of where the script is: the line readers' read-ahead` |
|      - |   28 | ` * plus whatever the read filter chain has already produced and nobody has taken` |
|      - |   29 | ` * yet. Both are past the position a script observes, so ftell(), a SEEK_CUR` |
|      - |   30 | `` * seek and stream_get_meta_data()'s `unread_bytes` all have to discount them.`` |
|      - |   31 | ` */` |
|      - |   32 | `static void ResetIOPrivate(io_private *pDev);` |
|      - |   33 | `static sxu32 StreamAheadBytes(io_private *pDev);` |
|      - |   34 | `/*` |
|      - |   35 | ` * A write lands where the SCRIPT is, not where the device is. Everything the` |
|      - |   36 | ` * readers pulled ahead — the line buffer and the filter chain's output alike —` |
|      - |   37 | ` * sits between the two, so it is stepped over and dropped before the write.` |
|      - |   38 | ` * php does the same by seeking to the logical position it tracks.` |
|      - |   39 | ` */` |
|    525 |   40 | `static void StreamSeekBackForWrite(io_private *pDev)` |
|      5 |   41 | `{` |
|    530 |   42 | `	sxu32 nAhead = StreamAheadBytes(pDev);` |
|    530 |   43 | `	if( nAhead > 0 && pDev->pStream && pDev->pStream->xSeek ){` |
|     16 |   44 | `		pDev->pStream->xSeek(pDev->pHandle,-(ph7_int64)nAhead,1/*SEEK_CUR*/);` |
|     16 |   45 | `		ResetIOPrivate(pDev);` |
|      7 |   46 | `	}` |
|    530 |   47 | `}` |
|    108 |   48 | `PH7_PRIVATE ph7_int64 PH7_StreamLogicalTell(io_private *pDev)` |
|      4 |   49 | `{` |
|      - |   50 | `	ph7_int64 iOfft;` |
|    112 |   51 | `	if( pDev == 0 ){` |
|    ! 0 |   52 | `		return -1;` |
|      - |   53 | `	}` |
|    112 |   54 | `	if( pDev->pReadFilters ){` |
|      - |   55 | `		/* A read filter breaks the tie between the device's offset and the` |
|      - |   56 | `		 * script's: four base64 characters come out of three bytes, so the two` |
|      - |   57 | `		 * numbers are not even the same magnitude. php counts what it` |
|      - |   58 | `		 * DELIVERED, and so does this — less whatever a line reader is still` |
|      - |   59 | `		 * holding on the script's behalf. */` |
|     27 |   60 | `		iOfft = pDev->iFiltPos;` |
|     27 |   61 | `		if( SyBlobLength(&pDev->sBuffer) > pDev->nOfft ){` |
|    ! 0 |   62 | `			iOfft -= (ph7_int64)(SyBlobLength(&pDev->sBuffer) - pDev->nOfft);` |
|    ! 0 |   63 | `		}` |
|     27 |   64 | `		return iOfft;` |
|      - |   65 | `	}` |
|     86 |   66 | `	if( pDev->pStream == 0 \|\| pDev->pStream->xTell == 0 ){` |
|    ! 0 |   67 | `		return -1;` |
|      - |   68 | `	}` |
|     86 |   69 | `	iOfft = pDev->pStream->xTell(pDev->pHandle);` |
|     86 |   70 | `	if( iOfft < 0 ){` |
|      5 |   71 | `		return iOfft;` |
|      - |   72 | `	}` |
|     82 |   73 | `	return iOfft - (ph7_int64)StreamAheadBytes(pDev);` |
|     58 |   74 | `}` |
|      - |   75 | `/*` |
|      - |   76 | ` * Seek the stream a php://filter proxy wraps. Same model as fseek() on a` |
|      - |   77 | ` * filtered handle: a relative move is resolved against the position the SCRIPT` |
|      - |   78 | ` * sees, because the device's own offset is not comparable to it.` |
|      - |   79 | ` */` |
|     38 |   80 | `PH7_PRIVATE int PH7_StreamSeekWrapped(io_private *pDev,ph7_int64 iOfft,int whence)` |
|      1 |   81 | `{` |
|      - |   82 | `	int rc;` |
|     39 |   83 | `	if( pDev == 0 \|\| pDev->pStream == 0 \|\| pDev->pStream->xSeek == 0 ){` |
|    ! 0 |   84 | `		return -1;` |
|      - |   85 | `	}` |
|     39 |   86 | `	if( pDev->pReadFilters && whence != 2 /* SEEK_END */ ){` |
|      9 |   87 | `		if( whence == 1 /* SEEK_CUR */ ){` |
|      3 |   88 | `			iOfft += PH7_StreamLogicalTell(pDev);` |
|      1 |   89 | `		}` |
|      9 |   90 | `		whence = 0; /* SEEK_SET */` |
|      4 |   91 | `	}` |
|     39 |   92 | `	rc = pDev->pStream->xSeek(pDev->pHandle,iOfft,whence);` |
|     39 |   93 | `	if( rc == PH7_OK ){` |
|     39 |   94 | `		SyBlobReset(&pDev->sBuffer);` |
|     39 |   95 | `		pDev->nOfft = 0;` |
|     39 |   96 | `		SyBlobReset(&pDev->sFilt);` |
|     39 |   97 | `		pDev->nFiltOfft = 0;` |
|     39 |   98 | `		pDev->bFiltDone = 0;` |
|     39 |   99 | `		pDev->bEof = 0;` |
|     39 |  100 | `		PH7_StreamFilterRewound(pDev);` |
|     39 |  101 | `		pDev->iFiltPos = whence == 0 ? iOfft` |
|     19 |  102 | `			: (pDev->pStream->xTell ? pDev->pStream->xTell(pDev->pHandle) : 0);` |
|     19 |  103 | `	}` |
|     39 |  104 | `	return rc;` |
|     20 |  105 | `}` |
|    184 |  106 | `PH7_PRIVATE io_private * PH7_StreamUnwrap(io_private *pDev)` |
|      1 |  107 | `{` |
|    185 |  108 | `	if( pDev && pDev->pStream && is_php_stream(pDev->pStream) ){` |
|     53 |  109 | `		io_private *pInner = PH7_PhpStreamInner(pDev->pHandle);` |
|     53 |  110 | `		if( pInner ){` |
|     15 |  111 | `			return pInner;` |
|      - |  112 | `		}` |
|     19 |  113 | `	}` |
|    171 |  114 | `	return pDev;` |
|     93 |  115 | `}` |
|    739 |  116 | `static sxu32 StreamAheadBytes(io_private *pDev)` |
|      5 |  117 | `{` |
|    744 |  118 | `	sxu32 n = 0;` |
|    744 |  119 | `	if( SyBlobLength(&pDev->sBuffer) > pDev->nOfft ){` |
|     48 |  120 | `		n += SyBlobLength(&pDev->sBuffer) - pDev->nOfft;` |
|     22 |  121 | `	}` |
|    744 |  122 | `	if( SyBlobLength(&pDev->sFilt) > pDev->nFiltOfft ){` |
|      5 |  123 | `		n += SyBlobLength(&pDev->sFilt) - pDev->nFiltOfft;` |
|      2 |  124 | `	}` |
|    744 |  125 | `	return n;` |
|      5 |  126 | `}` |
|      - |  127 | `#ifdef PH7_ENABLE_NET` |
|      - |  128 | `/* The socket handle, declared here because stream_get_meta_data()'s labels ask` |
|      - |  129 | ` * whether a socket has a transport under it. */` |
|      - |  130 | `typedef struct sock_private sock_private;` |
|      - |  131 | `struct sock_private` |
|      - |  132 | `{` |
|      - |  133 | `	ph7_vm *pVm;` |
|      - |  134 | `	ph7_socket sock;` |
|      - |  135 | `	int bEof;` |
|      - |  136 | `	int iLastErr; /* the OS code a failed send left, for php's own notice */` |
|      - |  137 | `	int bGeneric; /* a socketpair: no transport, and php labels it apart */` |
|      - |  138 | `};` |
|      - |  139 | `#endif` |
|      - |  140 | `/*` |
|      - |  141 | ` * Return the PHP resource-type name for a raw resource handle.` |
|      - |  142 | ` * Every IO handle this VFS hands out (fopen/tmpfile/popen/opendir and the` |
|      - |  143 | ` * STDIN/STDOUT/STDERR constants) is an io_private, which PHP reports as` |
|      - |  144 | ` * "stream"; anything else is "Unknown". The magic probe mirrors the` |
|      - |  145 | ` * IO_PRIVATE_INVALID check the rest of this file uses to validate handles.` |
|      - |  146 | ` */` |
|     74 |  147 | `PH7_PRIVATE const char * PH7_VfsResourceType(void *pResource)` |
|      2 |  148 | `{` |
|     76 |  149 | `	io_private *pDev = (io_private *)pResource;` |
|     76 |  150 | `	if( !IO_PRIVATE_INVALID(pDev) ){` |
|      - |  151 | `		/* php names a persistent stream apart, and that name is the only way a` |
|      - |  152 | `		 * script can see that its handle is one. */` |
|     40 |  153 | `		return pDev->bPersist ? "persistent stream" : "stream";` |
|      - |  154 | `	}` |
|     38 |  155 | `	if( pDev && pDev->iMagic == PROC_PRIVATE_MAGIC ){` |
|      - |  156 | `		/* proc_open()'s handle is not a stream and php does not call it one: it` |
|      - |  157 | `` 		 * answered "Unknown" here, so `get_resource_type($proc) === 'process'` `` |
|      - |  158 | `		 * — the documented way to tell a process handle from a pipe — was` |
|      - |  159 | `		 * false. Its header IS an io_private, magic field included, which is` |
|      - |  160 | `		 * what one probe can tell them apart by. */` |
|      4 |  161 | `		return "process";` |
|      - |  162 | `	}` |
|     34 |  163 | `	if( pDev && pDev->iMagic == STREAM_CTX_MAGIC ){` |
|      - |  164 | `		/* stream_context_create()'s handle, and the name php gives it. */` |
|     22 |  165 | `		return "stream-context";` |
|      - |  166 | `	}` |
|     13 |  167 | `	if( pDev && pDev->iMagic == STREAM_BUCKET_MAGIC ){` |
|      - |  168 | `		/* The handle a StreamBucket carries; php shows one there. */` |
|      5 |  169 | `		return "userfilter.bucket";` |
|      - |  170 | `	}` |
|      9 |  171 | `	if( pDev && pDev->iMagic == STREAM_BRIGADE_MAGIC ){` |
|      - |  172 | ``		/* The `$in` and `$out` a userland filter() is handed. */`` |
|      3 |  173 | `		return "userfilter.bucket brigade";` |
|      - |  174 | `	}` |
|      7 |  175 | `	if( pDev && pDev->iMagic == STREAM_FILTER_MAGIC ){` |
|      - |  176 | `		/* stream_filter_append()'s handle. Note the SPACE: php names the context` |
|      - |  177 | ``		 * `stream-context` and the filter `stream filter`. */`` |
|      5 |  178 | `		return "stream filter";` |
|      - |  179 | `	}` |
|      3 |  180 | `	return "Unknown";` |
|     39 |  181 | `}` |
|      - |  182 | `/*` |
|      - |  183 | ` * Return TRUE if the given resource handle is an io_private that has been closed` |
|      - |  184 | ` * (fclose/closedir/pclose) yet kept alive so shared copies still observe it. php` |
|      - |  185 | ` * reports such a value as gettype()=='resource (closed)' and is_resource()==false.` |
|      - |  186 | ` * The magic probe mirrors IO_PRIVATE_INVALID and is safe on any resource handle:` |
|      - |  187 | ` * every resource this engine hands out is a struct larger than an io_private, so` |
|      - |  188 | ` * the iMagic slot is always in bounds and never equals the closed magic by chance.` |
|      - |  189 | ` */` |
|    286 |  190 | `PH7_PRIVATE int PH7_VfsResourceIsClosed(void *pResource)` |
|      5 |  191 | `{` |
|    291 |  192 | `	io_private *pDev = (io_private *)pResource;` |
|    291 |  193 | `	return pDev != 0 && pDev->iMagic == IO_PRIVATE_CLOSED_MAGIC;` |
|      5 |  194 | `}` |
|      - |  195 | `/*` |
|      - |  196 | ` * bool ftruncate(resource $handle,int64 $size)` |
|      - |  197 | ` *  Truncates a file to a given length.` |
|      - |  198 | ` * Parameters` |
|      - |  199 | ` *  $handle` |
|      - |  200 | ` *   The file pointer.` |
|      - |  201 | ` *   Note:` |
|      - |  202 | ` *    The handle must be open for writing.` |
|      - |  203 | ` * $size` |
|      - |  204 | ` *   The size to truncate to.` |
|      - |  205 | ` * Return` |
|      - |  206 | ` *  TRUE on success or FALSE on failure.` |
|      - |  207 | ` */` |
|     10 |  208 | `PH7_PRIVATE int PH7_builtin_ftruncate(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 |  209 | `{` |
|      - |  210 | `	const ph7_io_stream *pStream;` |
|      - |  211 | `	io_private *pDev;` |
|      - |  212 | `	ph7_int64 nSize;` |
|      - |  213 | `	int rc;` |
|     11 |  214 | `	if( nArg < 2 \|\| !ph7_value_is_resource(apArg[0]) ){` |
|      - |  215 | `		/* Missing/Invalid arguments,return FALSE */` |
|    ! 0 |  216 | `		ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"Expecting an IO handle");` |
|    ! 0 |  217 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 |  218 | `		return PH7_OK;` |
|      - |  219 | `	}` |
|      - |  220 | `	/* Extract our private data */` |
|     11 |  221 | `	pDev = (io_private *)ph7_value_to_resource(apArg[0]);` |
|      - |  222 | `	/* Make sure we are dealing with a valid io_private instance */` |
|     11 |  223 | `	if( IO_PRIVATE_INVALID(pDev) ){` |
|      - |  224 | `		/*Expecting an IO handle */` |
|    ! 0 |  225 | `		ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"Expecting an IO handle");` |
|    ! 0 |  226 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 |  227 | `		return PH7_OK;` |
|      - |  228 | `	}` |
|     11 |  229 | `	nSize = ph7_value_to_int64(apArg[1]);` |
|     11 |  230 | `	if( nSize < 0 ){` |
|      - |  231 | `		/* php 8: catchable ValueError, raised BEFORE the unsupported-stream` |
|      - |  232 | `		 * check (php-src orders the size check first). PHL used to truncate. */` |
|      3 |  233 | `		return PH7_VmThrowException(pCtx,"ValueError",` |
|      - |  234 | `			"ftruncate(): Argument #2 ($size) must be greater than or equal to 0");` |
|      - |  235 | `	}` |
|      - |  236 | `	/* Point to the target IO stream device */` |
|      9 |  237 | `	pStream = pDev->pStream;` |
|      9 |  238 | `	if( pStream == 0  \|\| pStream->xTrunc == 0){` |
|    ! 0 |  239 | `		ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,` |
|      - |  240 | `			"IO routine(%s) not implemented in the underlying stream(%s) device,PH7 is returning FALSE",` |
|    ! 0 |  241 | `			ph7_function_name(pCtx),pStream ? pStream->zName : "null_stream"` |
|      - |  242 | `			);` |
|    ! 0 |  243 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 |  244 | `		return PH7_OK;` |
|      - |  245 | `	}` |
|      - |  246 | `	/* Perform the requested operation */` |
|      9 |  247 | `	rc = pStream->xTrunc(pDev->pHandle,nSize);` |
|      - |  248 | `	/* php does NOT touch the read buffer here: truncating is not a seek, the` |
|      - |  249 | `	 * position does not move, and what the readers already pulled ahead is` |
|      - |  250 | `	 * still what the next read answers. Dropping it made ftell() jump to the` |
|      - |  251 | `	 * device's own offset and the next read start there — past the new end` |
|      - |  252 | ``	 * (`""` where php answers the buffered line) or, after a truncation that`` |
|      - |  253 | `	 * GREW the file, over the NUL padding no php ever hands back. */` |
|      - |  254 | `	/* IO result */` |
|      9 |  255 | `	ph7_result_bool(pCtx,rc == PH7_OK);` |
|      9 |  256 | `	return PH7_OK;` |
|      6 |  257 | `}` |
|      - |  258 | `/*` |
|      - |  259 | ` * int fseek(resource $handle,int $offset[,int $whence = SEEK_SET ])` |
|      - |  260 | ` *  Seeks on a file pointer.` |
|      - |  261 | ` * Parameters` |
|      - |  262 | ` *  $handle` |
|      - |  263 | ` *   A file system pointer resource that is typically created using fopen().` |
|      - |  264 | ` * $offset` |
|      - |  265 | ` *   The offset.` |
|      - |  266 | ` *   To move to a position before the end-of-file, you need to pass a negative` |
|      - |  267 | ` *   value in offset and set whence to SEEK_END.` |
|      - |  268 | ` *   whence` |
|      - |  269 | ` *   whence values are:` |
|      - |  270 | ` *    SEEK_SET - Set position equal to offset bytes.` |
|      - |  271 | ` *    SEEK_CUR - Set position to current location plus offset.` |
|      - |  272 | ` *    SEEK_END - Set position to end-of-file plus offset.` |
|      - |  273 | ` * Return` |
|      - |  274 | ` *  0 on success,-1 on failure` |
|      - |  275 | ` */` |
|    120 |  276 | `PH7_PRIVATE int PH7_builtin_fseek(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      3 |  277 | `{` |
|      - |  278 | `	const ph7_io_stream *pStream;` |
|      - |  279 | `	io_private *pDev;` |
|      - |  280 | `	ph7_int64 iOfft;` |
|      - |  281 | `	int whence;` |
|      - |  282 | `	int rc;` |
|    123 |  283 | `	if( nArg < 2 \|\| !ph7_value_is_resource(apArg[0]) ){` |
|      - |  284 | `		/* Missing/Invalid arguments,return FALSE */` |
|    ! 0 |  285 | `		ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"Expecting an IO handle");` |
|    ! 0 |  286 | `		ph7_result_int(pCtx,-1);` |
|    ! 0 |  287 | `		return PH7_OK;` |
|      - |  288 | `	}` |
|      - |  289 | `	/* Extract our private data */` |
|    123 |  290 | `	pDev = (io_private *)ph7_value_to_resource(apArg[0]);` |
|      - |  291 | `	/* Make sure we are dealing with a valid io_private instance */` |
|    123 |  292 | `	if( IO_PRIVATE_INVALID(pDev) ){` |
|      - |  293 | `		/*Expecting an IO handle */` |
|    ! 0 |  294 | `		ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"Expecting an IO handle");` |
|    ! 0 |  295 | `		ph7_result_int(pCtx,-1);` |
|    ! 0 |  296 | `		return PH7_OK;` |
|      - |  297 | `	}` |
|      - |  298 | `	/* Point to the target IO stream device */` |
|    123 |  299 | `	pStream = pDev->pStream;` |
|    123 |  300 | `	if( pStream == 0  \|\| pStream->xSeek == 0){` |
|    ! 0 |  301 | `		ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,` |
|      - |  302 | `			"IO routine(%s) not implemented in the underlying stream(%s) device",` |
|    ! 0 |  303 | `			ph7_function_name(pCtx),pStream ? pStream->zName : "null_stream"` |
|      - |  304 | `			);` |
|    ! 0 |  305 | `		ph7_result_int(pCtx,-1);` |
|    ! 0 |  306 | `		return PH7_OK;` |
|      - |  307 | `	}` |
|      - |  308 | `	/* Extract the offset */` |
|    123 |  309 | `	iOfft = ph7_value_to_int64(apArg[1]);` |
|    123 |  310 | `	whence = 0;/* SEEK_SET */` |
|    123 |  311 | `	if( nArg > 2 ){` |
|      - |  312 | `		/* Read whatever was passed, coercing like php's ZPP: gating this on` |
|      - |  313 | `` 		 * ph7_value_is_int() left `fseek($f, 4, "99")` and `fseek($f, 4, 99.9)` `` |
|      - |  314 | `		 * falling back to whence 0, i.e. the very silent-SEEK_SET the check below` |
|      - |  315 | ``		 * exists to stop (and it disagreed with `99.0`, which is_int accepts). */`` |
|     48 |  316 | `		whence = (int)ph7_value_to_int64(apArg[2]);` |
|     23 |  317 | `	}` |
|    123 |  318 | `	if( whence != 0 /* SEEK_SET */ && whence != 1 /* SEEK_CUR */ && whence != 2 /* SEEK_END */ ){` |
|      - |  319 | `		/* php's php_stream_seek rejects an unknown whence and answers -1 WITHOUT` |
|      - |  320 | `		 * moving the cursor. PHL passed the raw value through to the driver, where` |
|      - |  321 | `		 * every non-CUR/END value fell into the SEEK_SET arm — so fseek($f, 0, 99)` |
|      - |  322 | `		 * reported success (0) AND silently seeked to the start of the stream, two` |
|      - |  323 | `		 * wrong answers from one unchecked argument. php raises no error here. */` |
|     13 |  324 | `		ph7_result_int(pCtx,-1);` |
|     13 |  325 | `		return PH7_OK;` |
|      - |  326 | `	}` |
|    111 |  327 | `	if( pDev->pReadFilters && whence != 2 /* SEEK_END */ ){` |
|      - |  328 | `		/* On a FILTERED stream the two positions are unrelated, so a relative` |
|      - |  329 | `		 * seek is resolved against the one the script sees and the device is` |
|      - |  330 | `		 * then placed at the result — php's own model, and the only one under` |
|      - |  331 | ``		 * which `fseek($f,0,SEEK_CUR)` is the no-op it looks like. */`` |
|      7 |  332 | `		if( whence == 1 /* SEEK_CUR */ ){` |
|      5 |  333 | `			iOfft += PH7_StreamLogicalTell(pDev);` |
|      2 |  334 | `		}` |
|      7 |  335 | `		rc = pStream->xSeek(pDev->pHandle,iOfft,0/*SEEK_SET*/);` |
|      7 |  336 | `		if( rc == PH7_OK ){` |
|      7 |  337 | `			ResetIOPrivate(pDev);` |
|      7 |  338 | `			pDev->iFiltPos = iOfft;` |
|      3 |  339 | `		}` |
|      7 |  340 | `		ph7_result_int(pCtx,rc == PH7_OK ? 0 : - 1);` |
|      7 |  341 | `		return PH7_OK;` |
|      - |  342 | `	}` |
|    105 |  343 | `	if( whence == 1 /* SEEK_CUR */ ){` |
|      - |  344 | `		/* The CURRENT position is the LOGICAL one: the device sits past the` |
|      - |  345 | `		 * read-ahead the line readers buffer, so seek relative to where the` |
|      - |  346 | `		 * SCRIPT is, not where the device is (StreamLogicalAdjust). Without` |
|      - |  347 | `		 * this, fseek($f,2,SEEK_CUR) after an fgets() that buffered ahead` |
|      - |  348 | `		 * skipped everything still sitting in the buffer. */` |
|     10 |  349 | `		iOfft -= (ph7_int64)StreamAheadBytes(pDev);` |
|      4 |  350 | `	}` |
|      - |  351 | `	/* Perform the requested operation */` |
|    105 |  352 | `	rc = pStream->xSeek(pDev->pHandle,iOfft,whence);` |
|    105 |  353 | `	if( rc == PH7_OK ){` |
|      - |  354 | `		/* Ignore buffered data */` |
|    101 |  355 | `		ResetIOPrivate(pDev);` |
|    101 |  356 | `		if( pDev->pReadFilters ){` |
|    ! 0 |  357 | `			pDev->iFiltPos = pStream->xTell ? pStream->xTell(pDev->pHandle) : 0;` |
|    ! 0 |  358 | `		}` |
|     49 |  359 | `	}` |
|      - |  360 | `	/* IO result */` |
|    105 |  361 | `	ph7_result_int(pCtx,rc == PH7_OK ? 0 : - 1);` |
|    105 |  362 | `	return PH7_OK;` |
|     63 |  363 | `}` |
|      - |  364 | `/*` |
|      - |  365 | ` * int64 ftell(resource $handle)` |
|      - |  366 | ` *  Returns the current position of the file read/write pointer.` |
|      - |  367 | ` * Parameters` |
|      - |  368 | ` *  $handle` |
|      - |  369 | ` *   The file pointer.` |
|      - |  370 | ` * Return` |
|      - |  371 | ` *  Returns the position of the file pointer referenced by handle` |
|      - |  372 | ` *  as an integer; i.e., its offset into the file stream.` |
|      - |  373 | ` *  FALSE is returned on failure.` |
|      - |  374 | ` */` |
|     88 |  375 | `PH7_PRIVATE int PH7_builtin_ftell(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      4 |  376 | `{` |
|      - |  377 | `	const ph7_io_stream *pStream;` |
|      - |  378 | `	io_private *pDev;` |
|      - |  379 | `	ph7_int64 iOfft;` |
|     92 |  380 | `	if( nArg < 1 \|\| !ph7_value_is_resource(apArg[0]) ){` |
|      - |  381 | `		/* Missing/Invalid arguments,return FALSE */` |
|    ! 0 |  382 | `		ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"Expecting an IO handle");` |
|    ! 0 |  383 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 |  384 | `		return PH7_OK;` |
|      - |  385 | `	}` |
|      - |  386 | `	/* Extract our private data */` |
|     92 |  387 | `	pDev = (io_private *)ph7_value_to_resource(apArg[0]);` |
|      - |  388 | `	/* Make sure we are dealing with a valid io_private instance */` |
|     92 |  389 | `	if( IO_PRIVATE_INVALID(pDev) ){` |
|      - |  390 | `		/*Expecting an IO handle */` |
|    ! 0 |  391 | `		ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"Expecting an IO handle");` |
|    ! 0 |  392 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 |  393 | `		return PH7_OK;` |
|      - |  394 | `	}` |
|      - |  395 | `	/* Point to the target IO stream device */` |
|     92 |  396 | `	pStream = pDev->pStream;` |
|     92 |  397 | `	if( pStream == 0  \|\| pStream->xTell == 0){` |
|    ! 0 |  398 | `		ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,` |
|      - |  399 | `			"IO routine(%s) not implemented in the underlying stream(%s) device,PH7 is returning FALSE",` |
|    ! 0 |  400 | `			ph7_function_name(pCtx),pStream ? pStream->zName : "null_stream"` |
|      - |  401 | `			);` |
|    ! 0 |  402 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 |  403 | `		return PH7_OK;` |
|      - |  404 | `	}` |
|      - |  405 | `	/* Perform the requested operation. The device sits past whatever the line` |
|      - |  406 | `	 * readers buffered ahead, so the SCRIPT's position is the device position` |
|      - |  407 | `	 * less the unconsumed remainder — ftell() after fgets("abcdefghij\nrest")` |
|      - |  408 | `	 * is php's 11, not the 15 the device already read. */` |
|     92 |  409 | `	iOfft = PH7_StreamLogicalTell(pDev);` |
|     92 |  410 | `	if( iOfft < 0 ){` |
|      - |  411 | `		/* The device does not know where it is -- php's answer for a stream` |
|      - |  412 | `		 * whose last seek FAILED (PDO's blob handle refuses one past its own` |
|      - |  413 | `		 * end and leaves the position unknown until a seek succeeds). */` |
|      5 |  414 | `		ph7_result_bool(pCtx,0);` |
|      5 |  415 | `		return PH7_OK;` |
|      - |  416 | `	}` |
|      - |  417 | `	/* IO result */` |
|     88 |  418 | `	ph7_result_int64(pCtx,iOfft);` |
|     88 |  419 | `	return PH7_OK;` |
|     48 |  420 | `}` |
|      - |  421 | `/*` |
|      - |  422 | ` * bool rewind(resource $handle)` |
|      - |  423 | ` *  Rewind the position of a file pointer.` |
|      - |  424 | ` * Parameters` |
|      - |  425 | ` *  $handle` |
|      - |  426 | ` *   The file pointer.` |
|      - |  427 | ` * Return` |
|      - |  428 | ` *  TRUE on success or FALSE on failure.` |
|      - |  429 | ` */` |
|    310 |  430 | `PH7_PRIVATE int PH7_builtin_rewind(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      4 |  431 | `{` |
|      - |  432 | `	const ph7_io_stream *pStream;` |
|      - |  433 | `	io_private *pDev;` |
|      - |  434 | `	int rc;` |
|    314 |  435 | `	if( nArg < 1 \|\| !ph7_value_is_resource(apArg[0]) ){` |
|      - |  436 | `		/* Missing/Invalid arguments,return FALSE */` |
|    ! 0 |  437 | `		ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"Expecting an IO handle");` |
|    ! 0 |  438 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 |  439 | `		return PH7_OK;` |
|      - |  440 | `	}` |
|      - |  441 | `	/* Extract our private data */` |
|    314 |  442 | `	pDev = (io_private *)ph7_value_to_resource(apArg[0]);` |
|      - |  443 | `	/* Make sure we are dealing with a valid io_private instance */` |
|    314 |  444 | `	if( IO_PRIVATE_INVALID(pDev) ){` |
|      - |  445 | `		/*Expecting an IO handle */` |
|    ! 0 |  446 | `		ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"Expecting an IO handle");` |
|    ! 0 |  447 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 |  448 | `		return PH7_OK;` |
|      - |  449 | `	}` |
|      - |  450 | `	/* Point to the target IO stream device */` |
|    314 |  451 | `	pStream = pDev->pStream;` |
|    314 |  452 | `	if( pStream == 0  \|\| pStream->xSeek == 0){` |
|    ! 0 |  453 | `		ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,` |
|      - |  454 | `			"IO routine(%s) not implemented in the underlying stream(%s) device,PH7 is returning FALSE",` |
|    ! 0 |  455 | `			ph7_function_name(pCtx),pStream ? pStream->zName : "null_stream"` |
|      - |  456 | `			);` |
|    ! 0 |  457 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 |  458 | `		return PH7_OK;` |
|      - |  459 | `	}` |
|      - |  460 | `	/* Perform the requested operation */` |
|    314 |  461 | `	rc = pStream->xSeek(pDev->pHandle,0,0/*SEEK_SET*/);` |
|    314 |  462 | `	if( rc == PH7_OK ){` |
|      - |  463 | `		/* Ignore buffered data */` |
|    314 |  464 | `		ResetIOPrivate(pDev);` |
|    155 |  465 | `	}` |
|      - |  466 | `	/* IO result */` |
|    314 |  467 | `	ph7_result_bool(pCtx,rc == PH7_OK);` |
|    314 |  468 | `	return PH7_OK;` |
|    159 |  469 | `}` |
|      - |  470 | `/*` |
|      - |  471 | ` * bool fflush(resource $handle)` |
|      - |  472 | ` *  Flushes the output to a file.` |
|      - |  473 | ` * Parameters` |
|      - |  474 | ` *  $handle` |
|      - |  475 | ` *   The file pointer.` |
|      - |  476 | ` * Return` |
|      - |  477 | ` *  TRUE on success or FALSE on failure.` |
|      - |  478 | ` */` |
|      6 |  479 | `PH7_PRIVATE int PH7_builtin_fflush(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 |  480 | `{` |
|      - |  481 | `	const ph7_io_stream *pStream;` |
|      - |  482 | `	io_private *pDev;` |
|      - |  483 | `	int rc;` |
|      7 |  484 | `	if( nArg < 1 \|\| !ph7_value_is_resource(apArg[0]) ){` |
|      - |  485 | `		/* Missing/Invalid arguments,return FALSE */` |
|    ! 0 |  486 | `		ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"Expecting an IO handle");` |
|    ! 0 |  487 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 |  488 | `		return PH7_OK;` |
|      - |  489 | `	}` |
|      - |  490 | `	/* Extract our private data */` |
|      7 |  491 | `	pDev = (io_private *)ph7_value_to_resource(apArg[0]);` |
|      - |  492 | `	/* Make sure we are dealing with a valid io_private instance */` |
|      7 |  493 | `	if( IO_PRIVATE_INVALID(pDev) ){` |
|      - |  494 | `		/*Expecting an IO handle */` |
|    ! 0 |  495 | `		ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"Expecting an IO handle");` |
|    ! 0 |  496 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 |  497 | `		return PH7_OK;` |
|      - |  498 | `	}` |
|      - |  499 | `	/* Point to the target IO stream device */` |
|      7 |  500 | `	pDev = PH7_StreamUnwrap(pDev);` |
|      7 |  501 | `	pStream = pDev->pStream;` |
|      7 |  502 | `	if( pStream == 0 \|\| pStream->xSync == 0){` |
|    ! 0 |  503 | `		ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,` |
|      - |  504 | `			"IO routine(%s) not implemented in the underlying stream(%s) device,PH7 is returning FALSE",` |
|    ! 0 |  505 | `			ph7_function_name(pCtx),pStream ? pStream->zName : "null_stream"` |
|      - |  506 | `			);` |
|    ! 0 |  507 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 |  508 | `		return PH7_OK;` |
|      - |  509 | `	}` |
|      - |  510 | `	/* Perform the requested operation */` |
|      7 |  511 | `	rc = pStream->xSync(pDev->pHandle);` |
|      - |  512 | `	/* IO result */` |
|      7 |  513 | `	ph7_result_bool(pCtx,rc == PH7_OK);` |
|      7 |  514 | `	return PH7_OK;` |
|      4 |  515 | `}` |
|      - |  516 | `/*` |
|      - |  517 | ` * php's end-of-file flag is set AFTER THE FACT: a stream is at EOF once one of` |
|      - |  518 | ` * its OWN reads has come back empty, and asking the question never reads. PHL` |
|      - |  519 | ` * used to probe the device instead — a read-ahead of up to 4 KB from inside` |
|      - |  520 | ` * feof() — which answered TRUE on a handle nothing had read yet (an empty file,` |
|      - |  521 | ` * a fresh php://memory), answered TRUE on a WRITE-only handle because the` |
|      - |  522 | `` * refused read looked like an end, and BLOCKED on `feof(STDIN)` with no input`` |
|      - |  523 | ` * waiting: a question about a stream is not a read of it. bEof is that flag,` |
|      - |  524 | ` * set wherever a read here comes back with nothing and cleared by every seek.` |
|      - |  525 | ` */` |
|      - |  526 | `static int IoPrivateUwrapEof(io_private *pDev,int *pAnswer);` |
|  25316 |  527 | `PH7_PRIVATE int PH7_StreamAtEof(io_private *pDev)` |
|      5 |  528 | `{` |
|      - |  529 | `	int bEof;` |
|  25321 |  530 | `	if( SyBlobLength(&pDev->sBuffer) > pDev->nOfft ){` |
|      - |  531 | `		/* Buffered bytes are not an end. */` |
|  16695 |  532 | `		return 0;` |
|      - |  533 | `	}` |
|   8631 |  534 | `	if( IoPrivateUwrapEof(pDev,&bEof) ){` |
|      - |  535 | `		/* A userland wrapper answers the question itself — php calls its` |
|      - |  536 | `		 * streamWrapper::stream_eof() rather than inferring anything. */` |
|      8 |  537 | `		return bEof;` |
|      - |  538 | `	}` |
|   8625 |  539 | `	return pDev->bEof != 0;` |
|  12663 |  540 | `}` |
|      - |  541 | `/*` |
|      - |  542 | ` * bool feof(resource $handle)` |
|      - |  543 | ` *  Tests for end-of-file on a file pointer.` |
|      - |  544 | ` * Parameters` |
|      - |  545 | ` *  $handle` |
|      - |  546 | ` *   The file pointer.` |
|      - |  547 | ` * Return` |
|      - |  548 | ` *  Returns TRUE if the file pointer is at EOF.FALSE otherwise` |
|      - |  549 | ` */` |
|  24932 |  550 | `PH7_PRIVATE int PH7_builtin_feof(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      5 |  551 | `{` |
|      - |  552 | `	const ph7_io_stream *pStream;` |
|      - |  553 | `	io_private *pDev;` |
|      - |  554 | `	int rc;` |
|  24937 |  555 | `	if( nArg < 1 \|\| !ph7_value_is_resource(apArg[0]) ){` |
|      - |  556 | `		/* Missing/Invalid arguments */` |
|    ! 0 |  557 | `		ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"Expecting an IO handle");` |
|    ! 0 |  558 | `		ph7_result_bool(pCtx,1);` |
|    ! 0 |  559 | `		return PH7_OK;` |
|      - |  560 | `	}` |
|      - |  561 | `	/* Extract our private data */` |
|  24937 |  562 | `	pDev = (io_private *)ph7_value_to_resource(apArg[0]);` |
|      - |  563 | `	/* Make sure we are dealing with a valid io_private instance */` |
|  24937 |  564 | `	if( IO_PRIVATE_INVALID(pDev) ){` |
|      - |  565 | `		/*Expecting an IO handle */` |
|    ! 0 |  566 | `		ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"Expecting an IO handle");` |
|    ! 0 |  567 | `		ph7_result_bool(pCtx,1);` |
|    ! 0 |  568 | `		return PH7_OK;` |
|      - |  569 | `	}` |
|      - |  570 | `	/* Point to the target IO stream device */` |
|  24937 |  571 | `	pStream = pDev->pStream;` |
|  24937 |  572 | `	if( pStream == 0 ){` |
|    ! 0 |  573 | `		ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,` |
|      - |  574 | `			"IO routine(%s) not implemented in the underlying stream(%s) device,PH7 is returning FALSE",` |
|    ! 0 |  575 | `			ph7_function_name(pCtx),pStream ? pStream->zName : "null_stream"` |
|      - |  576 | `			);` |
|    ! 0 |  577 | `		ph7_result_bool(pCtx,1);` |
|    ! 0 |  578 | `		return PH7_OK;` |
|      - |  579 | `	}` |
|  24937 |  580 | `	rc = PH7_StreamAtEof(pDev);` |
|      - |  581 | `	/* EOF or not */` |
|  24937 |  582 | `	ph7_result_bool(pCtx,rc != 0);` |
|  24937 |  583 | `	return PH7_OK;` |
|  12471 |  584 | `}` |
|      - |  585 | `/*` |
|      - |  586 | ` * Read n bytes from the underlying IO stream device.` |
|      - |  587 | ` * Return total numbers of bytes readen on success. A number < 1 on failure` |
|      - |  588 | ` * [i.e: IO error ] or EOF.` |
|      - |  589 | ` *` |
|      - |  590 | ` * This is the read every SCRIPT-level reader goes through, because it drains` |
|      - |  591 | ` * the line readers' read-ahead buffer first: a stream that fgets() has already` |
|      - |  592 | ` * pulled a block out of is positioned where the SCRIPT thinks it is, not where` |
|      - |  593 | ` * the device is. Anything reading from a caller's handle has to use this and` |
|      - |  594 | ` * not the device's own xRead.` |
|      - |  595 | ` */` |
|      - |  596 | `/*` |
|      - |  597 | ` * One read from the device, with the timeout bookkeeping php does for EVERY` |
|      - |  598 | `` * reader: `timed_out` describes the last read, so it is cleared on the way in`` |
|      - |  599 | ` * and set only by a wait that expired. Without the clear, one quiet period marks` |
|      - |  600 | ` * a handle timed out for the rest of its life — and now that every socket` |
|      - |  601 | ` * carries default_socket_timeout, that is every socket that ever waited. And` |
|      - |  602 | ` * without the set being here, only fread() would ever report one: fgets(),` |
|      - |  603 | ` * fgetc(), stream_get_line(), stream_get_contents() and fpassthru() all read` |
|      - |  604 | ` * through their own loops.` |
|      - |  605 | ` */` |
|  10675 |  606 | `static ph7_int64 IoPrivateRawRead(io_private *pDev,void *pBuf,ph7_int64 nLen)` |
|      5 |  607 | `{` |
|      - |  608 | `	ph7_int64 n;` |
|  10680 |  609 | `	pDev->bTimedOut = 0;` |
|  10680 |  610 | `	errno = 0;` |
|  10680 |  611 | `	n = pDev->pStream->xRead(pDev->pHandle,pBuf,nLen);` |
|  10675 |  612 | `	if( n < 0 && (errno == EAGAIN \|\| errno == EWOULDBLOCK)` |
|     28 |  613 | `	 && pDev->bHasTimeout && !pDev->bNonBlock ){` |
|      5 |  614 | `		pDev->bTimedOut = 1;` |
|      2 |  615 | `	}` |
|  10680 |  616 | `	return n;` |
|      5 |  617 | `}` |
|      - |  618 | `/*` |
|      - |  619 | ` * Serve a read from the FILTERED side of a handle. A filter changes the byte` |
|      - |  620 | ` * count — base64 makes four out of three, dechunk throws whole runs away — so` |
|      - |  621 | ` * what the chain produced cannot go straight into the caller's buffer: it waits` |
|      - |  622 | ` * in sFilt and is handed out from there.` |
|      - |  623 | ` *` |
|      - |  624 | ` * The fill loop runs until sFilt holds what was asked for or the device is` |
|      - |  625 | ` * spent, which is what keeps the caller's invariant intact: a SHORT answer here` |
|      - |  626 | ` * still means end of file, exactly as it does for an unfiltered read.` |
|      - |  627 | ` */` |
|    300 |  628 | `static ph7_int64 IoPrivateFilteredRead(io_private *pDev,void *pBuf,ph7_int64 nLen)` |
|      2 |  629 | `{` |
|    302 |  630 | `	phl_stream_filter *pChain = (phl_stream_filter *)pDev->pReadFilters;` |
|      - |  631 | `	sxu32 nAvail;` |
|      - |  632 | `	ph7_int64 n;` |
|      - |  633 |  |
|   2370 |  634 | `	while( pChain != 0 && !pDev->bFiltDone` |
|   2222 |  635 | `	    && (ph7_int64)(SyBlobLength(&pDev->sFilt) - pDev->nFiltOfft) < nLen ){` |
|      - |  636 | `		char zRaw[8192];` |
|   1368 |  637 | `		ph7_int64 nAsk = (ph7_int64)sizeof(zRaw);` |
|      - |  638 | `		ph7_int64 nRaw;` |
|      - |  639 | `		int iStatus;` |
|   1368 |  640 | `		if( pDev->nChunk > 0 && (ph7_int64)pDev->nChunk < nAsk ){` |
|   1099 |  641 | `			nAsk = (ph7_int64)pDev->nChunk;` |
|    549 |  642 | `		}` |
|   1368 |  643 | `		nRaw = IoPrivateRawRead(pDev,zRaw,nAsk);` |
|   1368 |  644 | `		if( nRaw < 0 ){` |
|    ! 0 |  645 | `			if( SyBlobLength(&pDev->sFilt) <= pDev->nFiltOfft ){` |
|      - |  646 | `				/* Nothing was ever produced: the IO error is the answer. */` |
|    ! 0 |  647 | `				return nRaw;` |
|      - |  648 | `			}` |
|    ! 0 |  649 | `			break;` |
|      - |  650 | `		}` |
|      - |  651 | `		{` |
|   1368 |  652 | `			int iF = nRaw > 0 ? PHL_PSFS_FLAG_NORMAL : PHL_PSFS_FLAG_FLUSH_CLOSE;` |
|      - |  653 | `			/* The device's end closes EVERY filter on the stream, not just the` |
|      - |  654 | `			 * head: each one's tail has to travel through the rest. */` |
|   1368 |  655 | `			iStatus = PH7_FilterChainProcess(pChain,zRaw,(sxu32)nRaw,iF,iF,&pDev->sFilt,0);` |
|      - |  656 | `		}` |
|   1368 |  657 | `		if( nRaw == 0 ){` |
|      - |  658 | `			/* The device is spent, and the call above was the chain's CLOSING` |
|      - |  659 | `			 * one: running it again would make a buffering filter emit its tail` |
|      - |  660 | `			 * twice, so the chain is finished for good. */` |
|    140 |  661 | `			pDev->bFiltDone = 1;` |
|    140 |  662 | `			break;` |
|      - |  663 | `		}` |
|   1230 |  664 | `		if( iStatus == PHL_PSFS_ERR_FATAL ){` |
|      - |  665 | `			/* A refusal ends the reading. php reports it to the reader as a` |
|      - |  666 | ``			 * FAILURE — `fread()` answers false, once — and only then as an end`` |
|      - |  667 | `			 * of file; what earlier calls already produced is still the` |
|      - |  668 | `			 * reader's, so the failure waits behind it. */` |
|      5 |  669 | `			pDev->bFiltDone = 1;` |
|      5 |  670 | `			pDev->bFiltErr = 1;` |
|      5 |  671 | `			break;` |
|      - |  672 | `		}` |
|      2 |  673 | `	}` |
|    302 |  674 | `	nAvail = SyBlobLength(&pDev->sFilt) - pDev->nFiltOfft;` |
|    302 |  675 | `	if( nAvail < 1 ){` |
|    140 |  676 | `		SyBlobReset(&pDev->sFilt);` |
|    140 |  677 | `		pDev->nFiltOfft = 0;` |
|    140 |  678 | `		if( pDev->bFiltErr ){` |
|      5 |  679 | `			pDev->bFiltErr = 0;   /* reported once; the read after it is an end */` |
|      5 |  680 | `			return -1;` |
|      - |  681 | `		}` |
|    136 |  682 | `		return pChain != 0 ? 0 : IoPrivateRawRead(pDev,pBuf,nLen);` |
|      - |  683 | `	}` |
|    164 |  684 | `	n = (ph7_int64)nAvail;` |
|    164 |  685 | `	if( n > nLen ){` |
|     25 |  686 | `		n = nLen;` |
|     12 |  687 | `	}` |
|    164 |  688 | `	SyMemcpy(SyBlobDataAt(&pDev->sFilt,pDev->nFiltOfft),pBuf,(sxu32)n);` |
|    164 |  689 | `	pDev->nFiltOfft += (sxu32)n;` |
|    164 |  690 | `	pDev->iFiltPos += n;` |
|    164 |  691 | `	if( pDev->nFiltOfft >= SyBlobLength(&pDev->sFilt) ){` |
|    140 |  692 | `		SyBlobReset(&pDev->sFilt);` |
|    140 |  693 | `		pDev->nFiltOfft = 0;` |
|     69 |  694 | `	}` |
|    164 |  695 | `	return n;` |
|    152 |  696 | `}` |
|   9609 |  697 | `static ph7_int64 IoPrivateDeviceRead(io_private *pDev,void *pBuf,ph7_int64 nLen)` |
|      5 |  698 | `{` |
|      - |  699 | `	ph7_int64 n;` |
|   9614 |  700 | `	if( pDev->pReadFilters != 0 \|\| SyBlobLength(&pDev->sFilt) > pDev->nFiltOfft ){` |
|      - |  701 | `		/* Bytes can still be waiting after the last read filter was REMOVED:` |
|      - |  702 | `		 * php flushes a filter on its way out and what it emitted belongs to` |
|      - |  703 | `		 * the reader that comes next. */` |
|    302 |  704 | `		return IoPrivateFilteredRead(pDev,pBuf,nLen);` |
|      - |  705 | `	}` |
|   9314 |  706 | `	errno = 0;` |
|   9314 |  707 | `	n = IoPrivateRawRead(pDev,pBuf,nLen);` |
|   9309 |  708 | `	if( n > 0 && is_php_stream(pDev->pStream)` |
|   1868 |  709 | `	 && PH7_PhpStreamTempDrained(pDev->pHandle) ){` |
|      - |  710 | `		/* php's php://temp raises its end flag as soon as a read has consumed` |
|      - |  711 | `		 * the buffer, one read before php://memory does. feof() still answers` |
|      - |  712 | `		 * false while the line readers hold bytes -- PH7_StreamAtEof() asks the` |
|      - |  713 | `		 * buffer first, which is php's own rule. */` |
|      5 |  714 | `		pDev->bEof = 1;` |
|      2 |  715 | `	}` |
|   9314 |  716 | `	if( n < 0 ){` |
|      - |  717 | `		/* LATCH the failure for the reader to report. php's notice comes from` |
|      - |  718 | `		 * the stream op, which knows the errno but not which builtin is asking;` |
|      - |  719 | `		 * here the builtin knows how to report and the device knows why, so the` |
|      - |  720 | `		 * two meet at the latch -- the same shape the socket write already uses.` |
|      - |  721 | `		 * Cleared by whoever reports it, so one failure is announced once. */` |
|     41 |  722 | `		pDev->iLastReadErr = errno ? errno : EIO;` |
|     19 |  723 | `	}` |
|   9314 |  724 | `	return n;` |
|   4808 |  725 | `}` |
|      - |  726 | `/*` |
|      - |  727 | `` * php's `fread(): Read of 8192 bytes failed with errno=9 Bad file descriptor`:`` |
|      - |  728 | ` * the NOTICE its plain-file read op raises when the device refuses -- a read` |
|      - |  729 | ` * from a handle opened write-only being the everyday case. The COUNT is not` |
|      - |  730 | ` * what the caller asked for: php fills its read buffer, so it reports the` |
|      - |  731 | ` * CHUNK size (8192 by default, whatever stream_set_chunk_size() left` |
|      - |  732 | ` * otherwise) at every reader. Silent for every other device, as php's is.` |
|      - |  733 | ` */` |
|   5500 |  734 | `PH7_PRIVATE void StreamReportReadFailure(ph7_context *pCtx,io_private *pDev)` |
|      5 |  735 | `{` |
|      - |  736 | `	int iErr;` |
|   5505 |  737 | `	if( pDev == 0 \|\| pDev->iLastReadErr == 0 ){` |
|   5473 |  738 | `		return;` |
|      - |  739 | `	}` |
|     35 |  740 | `	iErr = pDev->iLastReadErr;` |
|     35 |  741 | `	pDev->iLastReadErr = 0;` |
|     35 |  742 | `	if( pDev->pStream != pCtx->pVm->pDefStream ){` |
|      8 |  743 | `		return;` |
|      - |  744 | `	}` |
|     27 |  745 | `	ph7_context_throw_error_format(pCtx,PH7_CTX_NOTICE,` |
|      - |  746 | `		"Read of %u bytes failed with errno=%d %s",` |
|     26 |  747 | `		pDev->nChunk > 0 ? pDev->nChunk : 8192u,iErr,VfsStrerror(iErr));` |
|   2755 |  748 | `}` |
|    927 |  749 | `PH7_PRIVATE ph7_int64 PH7_StreamRead(io_private *pDev,void *pBuf,ph7_int64 nLen)` |
|      5 |  750 | `{` |
|    932 |  751 | `	const ph7_io_stream *pStream = pDev->pStream;` |
|    932 |  752 | `	char *zBuf = (char *)pBuf;` |
|      - |  753 | `	ph7_int64 n,nRead;` |
|    932 |  754 | `	n = SyBlobLength(&pDev->sBuffer) - pDev->nOfft;` |
|    932 |  755 | `	if( n > 0 ){` |
|     14 |  756 | `		if( n > nLen ){` |
|      6 |  757 | `			n = nLen;` |
|      2 |  758 | `		}` |
|      - |  759 | `		/* Copy the buffered data */` |
|     14 |  760 | `		SyMemcpy(SyBlobDataAt(&pDev->sBuffer,pDev->nOfft),pBuf,(sxu32)n);` |
|      - |  761 | `		/* Update the read offset */` |
|     14 |  762 | `		pDev->nOfft += (sxu32)n;` |
|     14 |  763 | `		if( pDev->nOfft >= SyBlobLength(&pDev->sBuffer) ){` |
|      - |  764 | `			/* Reset the working buffer so that we avoid excessive memory allocation */` |
|      9 |  765 | `			SyBlobReset(&pDev->sBuffer);` |
|      9 |  766 | `			pDev->nOfft = 0;` |
|      4 |  767 | `		}` |
|     14 |  768 | `		nLen -= n;` |
|     14 |  769 | `		if( nLen < 1 ){` |
|      - |  770 | `			/* All done */` |
|      6 |  771 | `			return n;` |
|      - |  772 | `		}` |
|      - |  773 | `		/* Advance the cursor */` |
|      9 |  774 | `		zBuf += n;` |
|      4 |  775 | `	}` |
|      - |  776 | `	/* Read without buffering */` |
|    928 |  777 | `	nRead = IoPrivateDeviceRead(pDev,zBuf,nLen);` |
|    923 |  778 | `	if( nRead == 0` |
|    743 |  779 | `	 \|\| (nRead > 0 && nRead < nLen && pStream->xSeek != 0 && pStream->xTell != 0` |
|    356 |  780 | `	     && pStream->xTell(pDev->pHandle) >= 0) ){` |
|      - |  781 | `		/* A read that came back with nothing IS php's end-of-file event, and` |
|      - |  782 | `		 * so is a SHORT one on a device that can say where it IS: php fills` |
|      - |  783 | ``		 * its buffer in a loop, so `fread($f, 100)` on a 12-byte file performs`` |
|      - |  784 | `		 * the second read that finds the end. The position query is what tells` |
|      - |  785 | `		 * a regular file from a FIFO — both arrive here through the same file` |
|      - |  786 | `		 * device, and a short read from a fifo, a pipe or a socket means only` |
|      - |  787 | `		 * that less had arrived, so latching there would end` |
|      - |  788 | ``		 * `while (!feof($p)) $s .= fread($p, 8192);` with data still coming. A`` |
|      - |  789 | `		 * NEGATIVE answer is an IO error and never latches. */` |
|    715 |  790 | `		pDev->bEof = 1;` |
|    354 |  791 | `	}` |
|    928 |  792 | `	if( nRead > 0 ){` |
|    520 |  793 | `		n += nRead;` |
|    669 |  794 | `	}else if( n < 1 ){` |
|      - |  795 | `		/* EOF or IO error */` |
|    407 |  796 | `		return nRead;` |
|      - |  797 | `	}` |
|    526 |  798 | `	return n;` |
|    467 |  799 | `}` |
|      - |  800 | `/*` |
|      - |  801 | ` * Every SCRIPT-level write goes through here, because a handle can carry a` |
|      - |  802 | ` * WRITE chain: php runs what the script wrote through the filters before the` |
|      - |  803 | ` * device sees any of it, and a filter changes the byte count — so what reaches` |
|      - |  804 | ` * the device is not what was handed in, while what fwrite() ANSWERS still is` |
|      - |  805 | ` * (php reports the bytes it CONSUMED, not the bytes it emitted).` |
|      - |  806 | ` */` |
|    655 |  807 | `PH7_PRIVATE ph7_int64 PH7_StreamWrite(io_private *pDev,const void *pData,ph7_int64 nLen)` |
|      5 |  808 | `{` |
|    660 |  809 | `	phl_stream_filter *pChain = (phl_stream_filter *)pDev->pWriteFilters;` |
|      - |  810 | `	SyBlob sOut;` |
|      - |  811 | `	ph7_int64 nWr;` |
|      - |  812 | `	int iStatus;` |
|    660 |  813 | `	if( pDev->pStream == 0 \|\| pDev->pStream->xWrite == 0 ){` |
|    ! 0 |  814 | `		return -1;` |
|      - |  815 | `	}` |
|    660 |  816 | `	if( pChain == 0 ){` |
|    630 |  817 | `		return pDev->pStream->xWrite(pDev->pHandle,pData,nLen);` |
|      - |  818 | `	}` |
|     32 |  819 | `	SyBlobInit(&sOut,pDev->sBuffer.pAllocator);` |
|     32 |  820 | `	iStatus = PH7_FilterChainProcess(pChain,pData,(sxu32)nLen,` |
|      - |  821 | `		PHL_PSFS_FLAG_NORMAL,PHL_PSFS_FLAG_NORMAL,&sOut,0);` |
|     32 |  822 | `	if( iStatus == PHL_PSFS_ERR_FATAL ){` |
|      5 |  823 | `		SyBlobRelease(&sOut);` |
|      5 |  824 | `		return -1;` |
|      - |  825 | `	}` |
|     28 |  826 | `	nWr = 0;` |
|     28 |  827 | `	if( SyBlobLength(&sOut) > 0 ){` |
|     41 |  828 | `		nWr = pDev->pStream->xWrite(pDev->pHandle,SyBlobData(&sOut),` |
|     26 |  829 | `			(ph7_int64)SyBlobLength(&sOut));` |
|     13 |  830 | `	}` |
|     28 |  831 | `	SyBlobRelease(&sOut);` |
|     28 |  832 | `	if( nWr < 0 ){` |
|    ! 0 |  833 | `		return -1;` |
|      - |  834 | `	}` |
|      - |  835 | `	/* A filter that held its input back (FEED_ME) still consumed it: php's` |
|      - |  836 | `	 * fwrite() answers the length it was given. */` |
|     28 |  837 | `	return nLen;` |
|    331 |  838 | `}` |
|      - |  839 | `/*` |
|      - |  840 | ` * Extract a single line from the buffered input.` |
|      - |  841 | ` */` |
|  19946 |  842 | `static sxi32 GetLine(io_private *pDev,ph7_int64 *pLen,const char **pzLine)` |
|      5 |  843 | `{` |
|      - |  844 | `	const char *zIn,*zEnd,*zPtr;` |
|  19951 |  845 | `	zIn = (const char *)SyBlobDataAt(&pDev->sBuffer,pDev->nOfft);` |
|  19951 |  846 | `	zEnd = &zIn[SyBlobLength(&pDev->sBuffer)-pDev->nOfft];` |
|  19951 |  847 | `	zPtr = zIn;` |
| 898110 |  848 | `	while( zIn < zEnd ){` |
| 897796 |  849 | `		if( zIn[0] == '\n' ){` |
|      - |  850 | `			/* Line found */` |
|  19637 |  851 | `			zIn++; /* Include the line ending as requested by the PHP specification */` |
|  19637 |  852 | `			*pLen = (ph7_int64)(zIn-zPtr);` |
|  19637 |  853 | `			*pzLine = zPtr;` |
|  19637 |  854 | `			return SXRET_OK;` |
|      - |  855 | `		}` |
| 878164 |  856 | `		zIn++;` |
|      5 |  857 | `	}` |
|      - |  858 | `	/* No line were found */` |
|    319 |  859 | `	return SXERR_NOTFOUND;` |
|   9978 |  860 | `}` |
|      - |  861 | `/*` |
|      - |  862 | ` * Read a single line from the underlying IO stream device.` |
|      - |  863 | ` */` |
|  25150 |  864 | `PH7_PRIVATE ph7_int64 StreamReadLine(io_private *pDev,const char **pzData,ph7_int64 nMaxLen)` |
|      5 |  865 | `{` |
|      - |  866 | `	char zBuf[8192];` |
|      - |  867 | `	ph7_int64 n;` |
|      - |  868 | `	sxi32 rc;` |
|  25155 |  869 | `	n = 0;` |
|  25155 |  870 | `	if( pDev->nOfft >= SyBlobLength(&pDev->sBuffer) ){` |
|      - |  871 | `		/* Reset the working buffer so that we avoid excessive memory allocation */` |
|   8373 |  872 | `		SyBlobReset(&pDev->sBuffer);` |
|   8373 |  873 | `		pDev->nOfft = 0;` |
|   4184 |  874 | `	}` |
|  25155 |  875 | `	if( SyBlobLength(&pDev->sBuffer) > pDev->nOfft ){` |
|      - |  876 | `		/* Check if there is a line */` |
|  16787 |  877 | `		rc = GetLine(pDev,&n,pzData);` |
|  16787 |  878 | `		if( rc == SXRET_OK ){` |
|      - |  879 | `			/* Got line. php caps it at nMaxLen bytes even when the newline` |
|      - |  880 | `			 * falls later; the remainder (incl. the newline) stays buffered. */` |
|  16713 |  881 | `			if( nMaxLen > 0 && n > nMaxLen ){` |
|    ! 0 |  882 | `				n = nMaxLen;` |
|    ! 0 |  883 | `			}` |
|  16713 |  884 | `			pDev->nOfft += (sxu32)n;` |
|  16713 |  885 | `			return n;` |
|      - |  886 | `		}` |
|     37 |  887 | `	}` |
|      - |  888 | `	/* Perform the read operation until a new line is extracted or length` |
|      - |  889 | `	 * limit is reached.` |
|      - |  890 | `	 */` |
|   4322 |  891 | `	for(;;){` |
|    101 |  892 | `		{` |
|      - |  893 | `			/* php fills its read buffer one CHUNK at a time, and` |
|      - |  894 | `			 * stream_set_chunk_size() is how a script asks for a smaller one. */` |
|   8649 |  895 | `			ph7_int64 nAsk = (ph7_int64)sizeof(zBuf);` |
|   8649 |  896 | `			if( pDev->nChunk > 0 && (ph7_int64)pDev->nChunk < nAsk ){` |
|     30 |  897 | `				nAsk = (ph7_int64)pDev->nChunk;` |
|     15 |  898 | `			}` |
|   8649 |  899 | `			if( nMaxLen > 0 && nMaxLen < nAsk ){` |
|     75 |  900 | `				nAsk = nMaxLen;` |
|     37 |  901 | `			}` |
|   8649 |  902 | `			n = IoPrivateDeviceRead(pDev,zBuf,nAsk);` |
|      - |  903 | `		}` |
|   8649 |  904 | `		if( n == 0 ){` |
|   5481 |  905 | `			pDev->bEof = 1;` |
|   2738 |  906 | `		}` |
|   8649 |  907 | `		if( n < 1 ){` |
|      - |  908 | `			/* EOF or IO error */` |
|   5485 |  909 | `			break;` |
|      - |  910 | `		}` |
|      - |  911 | `		/* Append the data just read */` |
|   3169 |  912 | `		SyBlobAppend(&pDev->sBuffer,zBuf,(sxu32)n);` |
|      - |  913 | `		/* Try to extract a line */` |
|   3169 |  914 | `		rc = GetLine(pDev,&n,pzData);` |
|   3169 |  915 | `		if( rc == SXRET_OK ){` |
|      - |  916 | `			/* Got one. Cap at nMaxLen (php's length limit); anything past the` |
|      - |  917 | `			 * cap, newline included, is left buffered for the next read. */` |
|   2929 |  918 | `			if( nMaxLen > 0 && n > nMaxLen ){` |
|      7 |  919 | `				n = nMaxLen;` |
|      3 |  920 | `			}` |
|   2929 |  921 | `			pDev->nOfft += (sxu32)n;` |
|   2929 |  922 | `			return n;` |
|      - |  923 | `		}` |
|    245 |  924 | `		if( nMaxLen > 0 && (SyBlobLength(&pDev->sBuffer) - pDev->nOfft >= nMaxLen) ){` |
|      - |  925 | `			/* Cap reached before a newline: return EXACTLY nMaxLen bytes (a prior` |
|      - |  926 | `			 * sub-cap leftover plus this read can hold more) and keep the` |
|      - |  927 | `			 * remainder buffered via nOfft for the next read, so we never hand` |
|      - |  928 | `			 * back more than nMaxLen. The top-of-function check reclaims the` |
|      - |  929 | `			 * buffer once it is fully consumed. */` |
|     39 |  930 | `			*pzData = (const char *)SyBlobDataAt(&pDev->sBuffer,pDev->nOfft);` |
|     39 |  931 | `			n = nMaxLen;` |
|     39 |  932 | `			pDev->nOfft += (sxu32)nMaxLen;` |
|     39 |  933 | `			return n;` |
|      - |  934 | `		}` |
|      5 |  935 | `	}` |
|   5485 |  936 | `	if( SyBlobLength(&pDev->sBuffer) > pDev->nOfft ){` |
|      - |  937 | `		/* Read limit reached,return the available data */` |
|    243 |  938 | `		*pzData = (const char *)SyBlobDataAt(&pDev->sBuffer,pDev->nOfft);` |
|    243 |  939 | `		n = SyBlobLength(&pDev->sBuffer) - pDev->nOfft;` |
|      - |  940 | `		/* Reset the working buffer */` |
|    243 |  941 | `		SyBlobReset(&pDev->sBuffer);` |
|    243 |  942 | `		pDev->nOfft = 0;` |
|    119 |  943 | `	}` |
|   5485 |  944 | `	return n;` |
|  12580 |  945 | `}` |
|      - |  946 | `/*` |
|      - |  947 | ` * Open an IO stream handle.` |
|      - |  948 | ` * Notes on stream:` |
|      - |  949 | ` * According to the PHP reference manual.` |
|      - |  950 | ` * In its simplest definition, a stream is a resource object which exhibits streamable behavior.` |
|      - |  951 | ` * That is, it can be read from or written to in a linear fashion, and may be able to fseek()` |
|      - |  952 | ` * to an arbitrary locations within the stream.` |
|      - |  953 | ` * A wrapper is additional code which tells the stream how to handle specific protocols/encodings.` |
|      - |  954 | ` * For example, the http wrapper knows how to translate a URL into an HTTP/1.0 request for a file` |
|      - |  955 | ` * on a remote server.` |
|      - |  956 | ` * A stream is referenced as: scheme://target` |
|      - |  957 | ` *   scheme(string) - The name of the wrapper to be used. Examples include: file, http...` |
|      - |  958 | ` *   If no wrapper is specified, the function default is used (typically file://).` |
|      - |  959 | ` *   target - Depends on the wrapper used. For filesystem related streams this is typically a path` |
|      - |  960 | ` *  and filename of the desired file. For network related streams this is typically a hostname, often` |
|      - |  961 | ` *  with a path appended.` |
|      - |  962 | ` *` |
|      - |  963 | ` * Note that PH7 IO streams looks like PHP streams but their implementation differ greately.` |
|      - |  964 | ` * Please refer to the official documentation for a full discussion.` |
|      - |  965 | ` * This function return a handle on success. Otherwise null.` |
|      - |  966 | ` */` |
|  39052 |  967 | `PH7_PRIVATE void * PH7_StreamOpenHandle(ph7_vm *pVm,const ph7_io_stream *pStream,const char *zFile,` |
|      - |  968 | `	int iFlags,int use_include,ph7_value *pResource,int bPushInclude,int *pNew,const char *zCaller)` |
|      5 |  969 | `{` |
|  39057 |  970 | `	void *pHandle = 0; /* cc warning */` |
|      - |  971 | `	SyString sFile;` |
|      - |  972 | `	ph7_value sDummy;` |
|      - |  973 | `	int rc;` |
|  39057 |  974 | `	if( pStream == 0 ){` |
|      - |  975 | `		/* No such stream device. The armed context describes THIS open and` |
|      - |  976 | `		 * nothing else, so it is dropped on every exit — a caller that armed one` |
|      - |  977 | `		 * and returned early must not leave it for the next open to pick up. */` |
|    ! 0 |  978 | `		pVm->pOpenCtx = 0;` |
|    ! 0 |  979 | `		if( pVm->nOpenDepth < 1 ){` |
|    ! 0 |  980 | `			pVm->zOpenErr = 0;` |
|    ! 0 |  981 | `		}` |
|    ! 0 |  982 | `		return 0;` |
|      - |  983 | `	}` |
|      - |  984 | `	/* Arm the reason THIS open would report. php's default for a wrapper that` |
|      - |  985 | `	 * logs nothing of its own is a flat "operation failed"; only the plain-file` |
|      - |  986 | `	 * wrapper reports an errno, which is why every other one used to print` |
|      - |  987 | ``	 * whatever errno was left over — `Success` for a failure, among others. An`` |
|      - |  988 | `	 * xOpen body may replace it through PH7_StreamSetOpenError(). */` |
|  39057 |  989 | `	if( pVm->nOpenDepth < 1 ){` |
|  39011 |  990 | `		pVm->zOpenErr = pStream == pVm->pDefStream ? 0 : "operation failed";` |
|  19503 |  991 | `	}` |
|  39057 |  992 | `	if( pStream->xOpen == 0 ){` |
|      - |  993 | `		/* A wrapper with a dir_opener and NOTHING else — glob:// is php's one,` |
|      - |  994 | `		 * and this is php's sentence for it. Reached before the call, because` |
|      - |  995 | `		 * the call would be through a null pointer. */` |
|      7 |  996 | `		pVm->pOpenCtx = 0;` |
|      7 |  997 | `		if( pVm->nOpenDepth < 1 ){` |
|      7 |  998 | `			pVm->zOpenErr = "wrapper does not support stream open";` |
|      3 |  999 | `		}` |
|      7 | 1000 | `		return 0;` |
|      - | 1001 | `	}` |
|      - | 1002 | `	/* A wrapper registered with STREAM_IS_URL speaks to the network, and php lets` |
|      - | 1003 | `	 * the configuration turn that off: allow_url_fopen for an ordinary open,` |
|      - | 1004 | `	 * allow_url_include for the one that EXECUTES what comes back — which is off` |
|      - | 1005 | `	 * by default, because including a remote file is the classic RFI. */` |
|  39051 | 1006 | `	if( PH7_StreamIsUrlWrapper(pStream) ){` |
|      - | 1007 | `		/* php tests BOTH, in this order: a URL wrapper is unusable at all without` |
|      - | 1008 | `		 * allow_url_fopen, and an INCLUDE needs allow_url_include on top of it. */` |
|     57 | 1009 | `		const char *zIni = 0;` |
|     57 | 1010 | `		if( !PH7_VmIniGetBool(pVm,"allow_url_fopen",1) ){` |
|      7 | 1011 | `			zIni = "allow_url_fopen";` |
|     54 | 1012 | `		}else if( bPushInclude && !PH7_VmIniGetBool(pVm,"allow_url_include",0) ){` |
|      5 | 1013 | `			zIni = "allow_url_include";` |
|      2 | 1014 | `		}` |
|     57 | 1015 | `		if( zIni ){` |
|      - | 1016 | `			SyString sCaller;` |
|      - | 1017 | `			char zMsg[160];` |
|     12 | 1018 | `			pVm->pOpenCtx = 0;` |
|     12 | 1019 | `			SyStringInitFromBuf(&sCaller,zCaller ? zCaller : "",zCaller ? SyStrlen(zCaller) : 0);` |
|     17 | 1020 | `			SyBufferFormat(zMsg,sizeof(zMsg),` |
|      - | 1021 | `				"%s:// wrapper is disabled in the server configuration by %s=0",` |
|     10 | 1022 | `				pStream->zName,zIni);` |
|     12 | 1023 | `			PH7_VmThrowError(pVm,zCaller ? &sCaller : 0,PH7_CTX_WARNING,zMsg);` |
|     12 | 1024 | `			return 0;` |
|      - | 1025 | `		}` |
|     22 | 1026 | `	}` |
|  39041 | 1027 | `	if( pResource == 0 ){` |
|      - | 1028 | `		/* VM-dependent devices (php://, data://, tcp://, userland wrappers)` |
|      - | 1029 | `		 * reach the VM only through pResource->pVm — their xOpen has no vm` |
|      - | 1030 | `		 * parameter. Callers like file_get_contents pass no resource, so hand` |
|      - | 1031 | `		 * every device a synthesized stack value carrying the VM; xOpen only` |
|      - | 1032 | `		 * reads it during the call, and file:// ignores it. */` |
|  38529 | 1033 | `		PH7_MemObjInit(pVm,&sDummy);` |
|  38529 | 1034 | `		pResource = &sDummy;` |
|  19262 | 1035 | `	}` |
|  39041 | 1036 | `	SyStringInitFromBuf(&sFile,zFile,SyStrlen(zFile));` |
|      - | 1037 | `	/* Everything from here to the matching decrement is INSIDE an open, so a` |
|      - | 1038 | `	 * wrapper that opens something of its own does not get to rename the` |
|      - | 1039 | `	 * failure its caller will report. */` |
|  39041 | 1040 | `	pVm->nOpenDepth++;` |
|  39041 | 1041 | `	if( use_include ){` |
|   5422 | 1042 | `		if(	/* include_path names DIRECTORIES, so it has nothing to say about a` |
|      - | 1043 | `` 			 * URL: walking it for a `php://filter/…` one built `<dir>/filter/…` `` |
|      - | 1044 | `			 * and reported the whole open as an IO error. The direct arm is the` |
|      - | 1045 | `			 * one that also marks the file as included, which is what` |
|      - | 1046 | `			 * include_once needs. */` |
|  10844 | 1047 | `			pStream != pVm->pDefStream \|\|` |
|  10832 | 1048 | `			sFile.zString[0] == '/' \|\|` |
|      - | 1049 | `#ifdef __WINNT__` |
|      - | 1050 | `			(sFile.nByte > 2 && sFile.zString[1] == ':' && (sFile.zString[2] == '\\' \|\| sFile.zString[2] == '/') ) \|\|` |
|      - | 1051 | `#endif` |
|  10721 | 1052 | `			(sFile.nByte > 1 && sFile.zString[0] == '.' && sFile.zString[1] == '/') \|\|` |
|  10714 | 1053 | `			(sFile.nByte > 2 && sFile.zString[0] == '.' && sFile.zString[1] == '.' && sFile.zString[2] == '/') ){` |
|      - | 1054 | `				/*  Open the file directly */` |
|    135 | 1055 | `				rc = pStream->xOpen(zFile,iFlags,pResource,&pHandle);` |
|    135 | 1056 | `				if( rc == PH7_OK && bPushInclude ){` |
|      - | 1057 | `					/* Mark as included */` |
|    133 | 1058 | `					PH7_VmPushFilePath(pVm,sFile.zString,sFile.nByte,FALSE,pNew);` |
|     64 | 1059 | `				}` |
|     70 | 1060 | `		}else{` |
|      - | 1061 | `			SyString *pPath;` |
|      - | 1062 | `			SyBlob sWorker;` |
|      - | 1063 | `#ifdef __WINNT__` |
|      - | 1064 | `			static const int c = '\\';` |
|      - | 1065 | `#else` |
|      - | 1066 | `			static const int c = '/';` |
|      - | 1067 | `#endif` |
|      - | 1068 | `			/* Init the path builder working buffer */` |
|  10719 | 1069 | `			SyBlobInit(&sWorker,&pVm->sAllocator);` |
|      - | 1070 | `			/* Build a path from the set of include path */` |
|  10719 | 1071 | `			SySetResetCursor(&pVm->aPaths);` |
|  10719 | 1072 | `			rc = SXERR_IO;` |
|  10743 | 1073 | `			while( SXRET_OK == SySetGetNextEntry(&pVm->aPaths,(void **)&pPath) ){` |
|      - | 1074 | `				/* Build full path */` |
|  10727 | 1075 | `				SyBlobFormat(&sWorker,"%z%c%z",pPath,c,&sFile);` |
|      - | 1076 | `				/* Append null terminator */` |
|  10727 | 1077 | `				if( SXRET_OK != SyBlobNullAppend(&sWorker) ){` |
|    ! 0 | 1078 | `					continue;` |
|      - | 1079 | `				}` |
|      - | 1080 | `				/* Try to open the file */` |
|  10727 | 1081 | `				rc = pStream->xOpen((const char *)SyBlobData(&sWorker),iFlags,pResource,&pHandle);` |
|  10727 | 1082 | `				if( rc == PH7_OK ){` |
|  10702 | 1083 | `					if( bPushInclude ){` |
|      - | 1084 | `						/* Mark as included */` |
|  10702 | 1085 | `						PH7_VmPushFilePath(pVm,(const char *)SyBlobData(&sWorker),SyBlobLength(&sWorker),FALSE,pNew);` |
|   5349 | 1086 | `					}` |
|  10702 | 1087 | `					break;` |
|      - | 1088 | `				}` |
|      - | 1089 | `				/* Reset the working buffer */` |
|     27 | 1090 | `				SyBlobReset(&sWorker);` |
|      - | 1091 | `				/* Check the next path */` |
|      3 | 1092 | `			}` |
|  10719 | 1093 | `			if( rc != PH7_OK ){` |
|      - | 1094 | `				/* php's LAST RESORT, and the one PHL never had: the directory of` |
|      - | 1095 | ``				 * the file that is EXECUTING. `include 'lib.php'` next to the`` |
|      - | 1096 | `				 * script has to work whatever directory the script was started` |
|      - | 1097 | `				 * from -- see PH7_VmExecutingDir(). Tried after the include_path` |
|      - | 1098 | `				 * entries, as php tries it. */` |
|      - | 1099 | `				SyString sDir;` |
|     19 | 1100 | `				if( PH7_VmExecutingDir(pVm,&sDir) ){` |
|     19 | 1101 | `					SyBlobReset(&sWorker);` |
|     19 | 1102 | `					SyBlobFormat(&sWorker,"%z%c%z",&sDir,c,&sFile);` |
|     19 | 1103 | `					if( SXRET_OK == SyBlobNullAppend(&sWorker) ){` |
|     19 | 1104 | `						rc = pStream->xOpen((const char *)SyBlobData(&sWorker),iFlags,pResource,&pHandle);` |
|     19 | 1105 | `						if( rc == PH7_OK && bPushInclude ){` |
|      4 | 1106 | `							PH7_VmPushFilePath(pVm,(const char *)SyBlobData(&sWorker),` |
|      2 | 1107 | `								SyBlobLength(&sWorker),FALSE,pNew);` |
|      1 | 1108 | `						}` |
|      8 | 1109 | `					}` |
|      8 | 1110 | `				}` |
|      8 | 1111 | `			}` |
|  10719 | 1112 | `			SyBlobRelease(&sWorker);` |
|      - | 1113 | `		}` |
|   5427 | 1114 | `	}else{` |
|      - | 1115 | `		/* Open the URI direcly */` |
|  28197 | 1116 | `		rc = pStream->xOpen(zFile,iFlags,pResource,&pHandle);` |
|      - | 1117 | `	}` |
|  39041 | 1118 | `	pVm->nOpenDepth--;` |
|      - | 1119 | `	/* The armed context describes exactly ONE open — every attempt of the` |
|      - | 1120 | `	 * include-path walk above included — so it is dropped here whether the open` |
|      - | 1121 | `	 * worked or not. A device that wanted it (a userland wrapper) read it while` |
|      - | 1122 | `	 * its xOpen was running. */` |
|  39041 | 1123 | `	pVm->pOpenCtx = 0;` |
|  39041 | 1124 | `	if( rc != PH7_OK ){` |
|      - | 1125 | `		/* IO error */` |
|    133 | 1126 | `		return 0;` |
|      - | 1127 | `	}` |
|      - | 1128 | `	/* Nothing failed, so nothing is owed a reason: a later warning must not` |
|      - | 1129 | `	 * find this one still armed. An INNER open succeeding says nothing about` |
|      - | 1130 | `	 * the outer one, which may still be on its way to failing. */` |
|  38913 | 1131 | `	if( pVm->nOpenDepth < 1 ){` |
|  38869 | 1132 | `		pVm->zOpenErr = 0;` |
|  19432 | 1133 | `	}` |
|      - | 1134 | `	/* Return the file handle */` |
|  38913 | 1135 | `	return pHandle;` |
|  19531 | 1136 | `}` |
|      - | 1137 | `/* See ph7int.h: the wrapper's own reason for the open in flight. */` |
|      6 | 1138 | `PH7_PRIVATE void PH7_StreamSetOpenError(ph7_vm *pVm,const char *zReason)` |
|      1 | 1139 | `{` |
|      - | 1140 | `	/* Only the OUTERMOST wrapper's own body may name the failure: an inner` |
|      - | 1141 | `	 * open's wrapper is describing something the caller never asked for. */` |
|      7 | 1142 | `	if( pVm->nOpenDepth == 1 ){` |
|      7 | 1143 | `		pVm->zOpenErr = zReason;` |
|      3 | 1144 | `	}` |
|      7 | 1145 | `}` |
|      - | 1146 | `/* See ph7int.h. */` |
|      4 | 1147 | `PH7_PRIVATE void PH7_StreamSetOpenErrorCall(ph7_vm *pVm,const char *zClass,const char *zMethod)` |
|      1 | 1148 | `{` |
|      5 | 1149 | `	if( pVm->nOpenDepth != 1 ){` |
|    ! 0 | 1150 | `		return;` |
|      - | 1151 | `	}` |
|      7 | 1152 | `	SyBufferFormat(pVm->zOpenErrBuf,sizeof(pVm->zOpenErrBuf),"\"%s::%s\" call failed",` |
|      2 | 1153 | `		zClass ? zClass : "",zMethod);` |
|      5 | 1154 | `	pVm->zOpenErr = pVm->zOpenErrBuf;` |
|      3 | 1155 | `}` |
|      - | 1156 | `/*` |
|      - | 1157 | ` * Read the whole contents of an open IO stream handle [i.e local file/URL..]` |
|      - | 1158 | ` * Store the read data in the given BLOB (last argument).` |
|      - | 1159 | ` * The read operation is stopped when he hit the EOF or an IO error occurs.` |
|      - | 1160 | ` */` |
|  10828 | 1161 | `PH7_PRIVATE sxi32 PH7_StreamReadWholeFile(void *pHandle,const ph7_io_stream *pStream,SyBlob *pOut)` |
|      5 | 1162 | `{` |
|      - | 1163 | `	ph7_int64 nRead;` |
|      - | 1164 | `	char zBuf[8192]; /* 8K */` |
|      - | 1165 | `	int rc;` |
|      - | 1166 | `	/* Perform the requested operation */` |
|  10845 | 1167 | `	for(;;){` |
|  21695 | 1168 | `		nRead = pStream->xRead(pHandle,zBuf,sizeof(zBuf));` |
|  21695 | 1169 | `		if( nRead < 1 ){` |
|      - | 1170 | `			/* EOF or IO error */` |
|  10833 | 1171 | `			break;` |
|      - | 1172 | `		}` |
|      - | 1173 | `		/* Append contents */` |
|  10867 | 1174 | `		rc = SyBlobAppend(pOut,zBuf,(sxu32)nRead);` |
|  10867 | 1175 | `		if( rc != SXRET_OK ){` |
|    ! 0 | 1176 | `			break;` |
|      - | 1177 | `		}` |
|      5 | 1178 | `	}` |
|  10833 | 1179 | `	return SyBlobLength(pOut) > 0 ? SXRET_OK : -1;` |
|      5 | 1180 | `}` |
|      - | 1181 | `/*` |
|      - | 1182 | ` * Close an open IO stream handle [i.e local file/URI..].` |
|      - | 1183 | ` */` |
|  39152 | 1184 | `PH7_PRIVATE void PH7_StreamCloseHandle(const ph7_io_stream *pStream,void *pHandle)` |
|      5 | 1185 | `{` |
|  39157 | 1186 | `	if( pStream->xClose ){` |
|  39157 | 1187 | `		pStream->xClose(pHandle);` |
|  19576 | 1188 | `	}` |
|  39157 | 1189 | `}` |
|      - | 1190 | `/*` |
|      - | 1191 | ` * string fgetc(resource $handle)` |
|      - | 1192 | ` *  Gets a character from the given file pointer.` |
|      - | 1193 | ` * Parameters` |
|      - | 1194 | ` *  $handle` |
|      - | 1195 | ` *   The file pointer.` |
|      - | 1196 | ` * Return` |
|      - | 1197 | ` *  Returns a string containing a single character read from the file` |
|      - | 1198 | ` *  pointed to by handle. Returns FALSE on EOF.` |
|      - | 1199 | ` * WARNING` |
|      - | 1200 | ` *  This operation is extremely slow.Avoid using it.` |
|      - | 1201 | ` */` |
|      6 | 1202 | `PH7_PRIVATE int PH7_builtin_fgetc(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 1203 | `{` |
|      - | 1204 | `	const ph7_io_stream *pStream;` |
|      - | 1205 | `	io_private *pDev;` |
|      - | 1206 | `	int c,n;` |
|      7 | 1207 | `	if( nArg < 1 \|\| !ph7_value_is_resource(apArg[0]) ){` |
|      - | 1208 | `		/* Missing/Invalid arguments,return FALSE */` |
|    ! 0 | 1209 | `		ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"Expecting an IO handle");` |
|    ! 0 | 1210 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 1211 | `		return PH7_OK;` |
|      - | 1212 | `	}` |
|      - | 1213 | `	/* Extract our private data */` |
|      7 | 1214 | `	pDev = (io_private *)ph7_value_to_resource(apArg[0]);` |
|      - | 1215 | `	/* Make sure we are dealing with a valid io_private instance */` |
|      7 | 1216 | `	if( IO_PRIVATE_INVALID(pDev) ){` |
|      - | 1217 | `		/*Expecting an IO handle */` |
|    ! 0 | 1218 | `		ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"Expecting an IO handle");` |
|    ! 0 | 1219 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 1220 | `		return PH7_OK;` |
|      - | 1221 | `	}` |
|      - | 1222 | `	/* Point to the target IO stream device */` |
|      7 | 1223 | `	pStream = pDev->pStream;` |
|      7 | 1224 | `	if( pStream == 0  ){` |
|    ! 0 | 1225 | `		ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,` |
|      - | 1226 | `			"IO routine(%s) not implemented in the underlying stream(%s) device,PH7 is returning FALSE",` |
|    ! 0 | 1227 | `			ph7_function_name(pCtx),pStream ? pStream->zName : "null_stream"` |
|      - | 1228 | `			);` |
|    ! 0 | 1229 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 1230 | `		return PH7_OK;` |
|      - | 1231 | `	}` |
|      - | 1232 | `	/* Perform the requested operation */` |
|      7 | 1233 | `	n = (int)PH7_StreamRead(pDev,(void *)&c,sizeof(char));` |
|      - | 1234 | `	/* IO result */` |
|      7 | 1235 | `	if( n < 1 ){` |
|      - | 1236 | `		/* EOF or error,return FALSE */` |
|      3 | 1237 | `		StreamReportReadFailure(pCtx,pDev);` |
|      3 | 1238 | `		ph7_result_bool(pCtx,0);` |
|      2 | 1239 | `	}else{` |
|      - | 1240 | `		/* Return the string holding the character */` |
|      5 | 1241 | `		ph7_result_string(pCtx,(const char *)&c,sizeof(char));` |
|      - | 1242 | `	}` |
|      7 | 1243 | `	return PH7_OK;` |
|      4 | 1244 | `}` |
|      - | 1245 | `/*` |
|      - | 1246 | ` * array\|int\|false\|null fscanf(resource $stream, string $format, mixed &...$vars)` |
|      - | 1247 | ` *  Parse the NEXT LINE of $stream according to $format.` |
|      - | 1248 | ` *` |
|      - | 1249 | ` *  php reads one whole line -- the newline included, which is what makes a` |
|      - | 1250 | `` *  trailing `%s` stop where it does -- and hands it to the same scanner`` |
|      - | 1251 | ` *  sscanf() runs, so every rule of that family (the two-pass format read, the` |
|      - | 1252 | ` *  -1 / NULL "nothing converted" answer) is this function's too. The one` |
|      - | 1253 | ` *  answer of its own is FALSE, and it means the STREAM was at its end: a line` |
|      - | 1254 | ` *  that scans to nothing is still NULL or -1, exactly as sscanf's would be.` |
|      - | 1255 | ` */` |
|     32 | 1256 | `PH7_PRIVATE int PH7_builtin_fscanf(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 1257 | `{` |
|      - | 1258 | `	const ph7_io_stream *pStream;` |
|      - | 1259 | `	const char *zLine,*zFmt;` |
|      - | 1260 | `	io_private *pDev;` |
|      - | 1261 | `	ph7_int64 n;` |
|     33 | 1262 | `	int nFmt = 0;` |
|     33 | 1263 | `	if( nArg < 2 \|\| !ph7_value_is_resource(apArg[0]) ){` |
|    ! 0 | 1264 | `		ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"Expecting an IO handle");` |
|    ! 0 | 1265 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 1266 | `		return PH7_OK;` |
|      - | 1267 | `	}` |
|     33 | 1268 | `	pDev = (io_private *)ph7_value_to_resource(apArg[0]);` |
|     33 | 1269 | `	if( IO_PRIVATE_INVALID(pDev) ){` |
|    ! 0 | 1270 | `		ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"Expecting an IO handle");` |
|    ! 0 | 1271 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 1272 | `		return PH7_OK;` |
|      - | 1273 | `	}` |
|     33 | 1274 | `	pStream = pDev->pStream;` |
|     33 | 1275 | `	if( pStream == 0 ){` |
|    ! 0 | 1276 | `		ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,` |
|      - | 1277 | `			"IO routine(%s) not implemented in the underlying stream(%s) device,PH7 is returning FALSE",` |
|    ! 0 | 1278 | `			ph7_function_name(pCtx),"null_stream"` |
|      - | 1279 | `			);` |
|    ! 0 | 1280 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 1281 | `		return PH7_OK;` |
|      - | 1282 | `	}` |
|     33 | 1283 | `	n = StreamReadLine(pDev,&zLine,-1);` |
|     33 | 1284 | `	if( n < 1 ){` |
|      - | 1285 | `		/* Nothing left in the stream at all. */` |
|      5 | 1286 | `		ph7_result_bool(pCtx,0);` |
|      5 | 1287 | `		return PH7_OK;` |
|      - | 1288 | `	}` |
|     29 | 1289 | `	zFmt = ph7_value_to_string(apArg[1],&nFmt);` |
|     29 | 1290 | `	return (int)PH7_ScanfRun(pCtx,zLine,(int)n,zFmt,nFmt,&apArg[2],nArg - 2);` |
|     17 | 1291 | `}` |
|      - | 1292 | `/*` |
|      - | 1293 | ` * string fgets(resource $handle[,int64 $length ])` |
|      - | 1294 | ` *  Gets line from file pointer.` |
|      - | 1295 | ` * Parameters` |
|      - | 1296 | ` *  $handle` |
|      - | 1297 | ` *   The file pointer.` |
|      - | 1298 | ` * $length` |
|      - | 1299 | ` *  Reading ends when length - 1 bytes have been read, on a newline` |
|      - | 1300 | ` *  (which is included in the return value), or on EOF (whichever comes first).` |
|      - | 1301 | ` *  If no length is specified, it will keep reading from the stream until it reaches` |
|      - | 1302 | ` *  the end of the line.` |
|      - | 1303 | ` * Return` |
|      - | 1304 | ` *  Returns a string of up to length - 1 bytes read from the file pointed to by handle.` |
|      - | 1305 | ` *  If there is no more data to read in the file pointer, then FALSE is returned.` |
|      - | 1306 | ` *  If an error occurs, FALSE is returned.` |
|      - | 1307 | ` */` |
|  24768 | 1308 | `PH7_PRIVATE int PH7_builtin_fgets(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      5 | 1309 | `{` |
|      - | 1310 | `	const ph7_io_stream *pStream;` |
|      - | 1311 | `	const char *zLine;` |
|      - | 1312 | `	io_private *pDev;` |
|      - | 1313 | `	ph7_int64 n,nLen;` |
|  24773 | 1314 | `	if( nArg < 1 \|\| !ph7_value_is_resource(apArg[0]) ){` |
|      - | 1315 | `		/* Missing/Invalid arguments,return FALSE */` |
|    ! 0 | 1316 | `		ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"Expecting an IO handle");` |
|    ! 0 | 1317 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 1318 | `		return PH7_OK;` |
|      - | 1319 | `	}` |
|      - | 1320 | `	/* Extract our private data */` |
|  24773 | 1321 | `	pDev = (io_private *)ph7_value_to_resource(apArg[0]);` |
|      - | 1322 | `	/* Make sure we are dealing with a valid io_private instance */` |
|  24773 | 1323 | `	if( IO_PRIVATE_INVALID(pDev) ){` |
|      - | 1324 | `		/*Expecting an IO handle */` |
|    ! 0 | 1325 | `		ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"Expecting an IO handle");` |
|    ! 0 | 1326 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 1327 | `		return PH7_OK;` |
|      - | 1328 | `	}` |
|      - | 1329 | `	/* Point to the target IO stream device */` |
|  24773 | 1330 | `	pStream = pDev->pStream;` |
|  24773 | 1331 | `	if( pStream == 0  ){` |
|    ! 0 | 1332 | `		ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,` |
|      - | 1333 | `			"IO routine(%s) not implemented in the underlying stream(%s) device,PH7 is returning FALSE",` |
|    ! 0 | 1334 | `			ph7_function_name(pCtx),pStream ? pStream->zName : "null_stream"` |
|      - | 1335 | `			);` |
|    ! 0 | 1336 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 1337 | `		return PH7_OK;` |
|      - | 1338 | `	}` |
|  24773 | 1339 | `	nLen = -1;` |
|  24773 | 1340 | `	if( nArg > 1 && !ph7_value_is_null(apArg[1]) ){` |
|      - | 1341 | `		/* Maximum data to read. PHP 8 raises a catchable ValueError for a` |
|      - | 1342 | `		 * non-positive length; a NULL (the ?int default) reads the whole line. */` |
|     61 | 1343 | `		nLen = ph7_value_to_int64(apArg[1]);` |
|     61 | 1344 | `		if( nLen < 1 ){` |
|      5 | 1345 | `			return PH7_VmThrowException(pCtx,"ValueError",` |
|      - | 1346 | `				"fgets(): Argument #2 ($length) must be greater than 0");` |
|      - | 1347 | `		}` |
|      - | 1348 | `		/* php reads at most length-1 bytes -- one byte is reserved for the` |
|      - | 1349 | `		 * string terminator -- so a length of 1 reads nothing and returns` |
|      - | 1350 | `		 * false at any position, exactly like EOF. */` |
|     57 | 1351 | `		nLen -= 1;` |
|     57 | 1352 | `		if( nLen == 0 ){` |
|      3 | 1353 | `			ph7_result_bool(pCtx,0);` |
|      3 | 1354 | `			return PH7_OK;` |
|      - | 1355 | `		}` |
|     27 | 1356 | `	}` |
|      - | 1357 | `	/* Perform the requested operation */` |
|  24767 | 1358 | `	n = StreamReadLine(pDev,&zLine,nLen);` |
|  24767 | 1359 | `	if( n < 1 ){` |
|      - | 1360 | `		/* EOF or IO error,return FALSE */` |
|   5175 | 1361 | `		StreamReportReadFailure(pCtx,pDev);` |
|   5175 | 1362 | `		ph7_result_bool(pCtx,0);` |
|   2590 | 1363 | `	}else{` |
|      - | 1364 | `		/* Return the freshly extracted line */` |
|  19597 | 1365 | `		ph7_result_string(pCtx,zLine,(int)n);` |
|      - | 1366 | `	}` |
|  24767 | 1367 | `	return PH7_OK;` |
|  12389 | 1368 | `}` |
|      - | 1369 | `/*` |
|      - | 1370 | ` * string\|false stream_get_line(resource $stream, int $length, string $ending = "")` |
|      - | 1371 | ` *  Read a line from a stream, up to $length bytes or the FIRST occurrence of` |
|      - | 1372 | ` *  $ending, whichever comes first. Unlike fgets(), the ending is CONSUMED but` |
|      - | 1373 | ` *  never returned, and it may be any string.` |
|      - | 1374 | ` *  php's window rule (php_stream_get_record), pinned by probe: the ending` |
|      - | 1375 | ` *  counts only when it fits ENTIRELY inside the first $length bytes —` |
|      - | 1376 | ` *  stream_get_line($h,4,"--") over "abc--def" answers "abc-", the raw window,` |
|      - | 1377 | ` *  because the ending straddles its edge — and a capped read consumes no` |
|      - | 1378 | ` *  ending that starts at the boundary. $length 0 means php's 8192 default; at` |
|      - | 1379 | ` *  EOF the remainder is returned as-is, and false only when nothing is left.` |
|      - | 1380 | ` */` |
|     62 | 1381 | `PH7_PRIVATE int PH7_builtin_stream_get_line(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      3 | 1382 | `{` |
|      - | 1383 | `	const ph7_io_stream *pStream;` |
|     65 | 1384 | `	const char *zEnding = "";` |
|      - | 1385 | `	io_private *pDev;` |
|      - | 1386 | `	ph7_int64 nMaxLen;` |
|     65 | 1387 | `	int nEndLen = 0;` |
|     65 | 1388 | `	sxu32 iScanFrom = 0;` |
|     65 | 1389 | `	int bEof = 0;` |
|     65 | 1390 | `	if( nArg < 2 ){` |
|      - | 1391 | `		/* The central arity screen reports this; keep a refusal for a direct call. */` |
|    ! 0 | 1392 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 1393 | `		return PH7_OK;` |
|      - | 1394 | `	}` |
|     65 | 1395 | `	if( !ph7_value_is_resource(apArg[0]) ){` |
|      4 | 1396 | `		return PH7_VmThrowException(pCtx,"TypeError",` |
|      - | 1397 | `			"stream_get_line(): Argument #1 ($stream) must be of type resource, %s given",` |
|      1 | 1398 | `			ph7_type_name(apArg[0]));` |
|      - | 1399 | `	}` |
|     63 | 1400 | `	pDev = (io_private *)ph7_value_to_resource(apArg[0]);` |
|     63 | 1401 | `	if( IO_PRIVATE_INVALID(pDev) ){` |
|      - | 1402 | `		/* A closed or foreign resource is php's own TypeError, not a warning. */` |
|      3 | 1403 | `		return PH7_VmThrowException(pCtx,"TypeError",` |
|      - | 1404 | `			"stream_get_line(): Argument #1 ($stream) must be an open stream resource");` |
|      - | 1405 | `	}` |
|     61 | 1406 | `	pStream = pDev->pStream;` |
|     61 | 1407 | `	if( pStream == 0 ){` |
|    ! 0 | 1408 | `		ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,` |
|      - | 1409 | `			"IO routine(%s) not implemented in the underlying stream(%s) device,PH7 is returning FALSE",` |
|    ! 0 | 1410 | `			ph7_function_name(pCtx),"null_stream"` |
|      - | 1411 | `			);` |
|    ! 0 | 1412 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 1413 | `		return PH7_OK;` |
|      - | 1414 | `	}` |
|     61 | 1415 | `	nMaxLen = ph7_value_to_int64(apArg[1]);` |
|     61 | 1416 | `	if( nMaxLen < 0 ){` |
|      3 | 1417 | `		return PH7_VmThrowException(pCtx,"ValueError",` |
|      - | 1418 | `			"stream_get_line(): Argument #2 ($length) must be greater than or equal to 0");` |
|      - | 1419 | `	}` |
|     59 | 1420 | `	if( nMaxLen == 0 ){` |
|      - | 1421 | `		/* php's documented default window */` |
|      3 | 1422 | `		nMaxLen = 8192;` |
|      1 | 1423 | `	}` |
|     59 | 1424 | `	if( nArg > 2 ){` |
|     55 | 1425 | `		zEnding = ph7_value_to_string(apArg[2],&nEndLen);` |
|     26 | 1426 | `	}` |
|     59 | 1427 | `	if( pDev->nOfft >= SyBlobLength(&pDev->sBuffer) ){` |
|      - | 1428 | `		/* Reset the working buffer so that we avoid excessive memory allocation */` |
|     31 | 1429 | `		SyBlobReset(&pDev->sBuffer);` |
|     31 | 1430 | `		pDev->nOfft = 0;` |
|     14 | 1431 | `	}` |
|      - | 1432 | `	/* Fill-and-scan: buffer chunks until the ending fits inside the window,` |
|      - | 1433 | `	 * the window itself fills, or the stream dries up. */` |
|     62 | 1434 | `	for(;;){` |
|    101 | 1435 | `		const char *zData = (const char *)SyBlobDataAt(&pDev->sBuffer,pDev->nOfft);` |
|    101 | 1436 | `		sxu32 nAvail = SyBlobLength(&pDev->sBuffer) - pDev->nOfft;` |
|    101 | 1437 | `		sxu32 nWindow = (nMaxLen < (ph7_int64)nAvail) ? (sxu32)nMaxLen : nAvail;` |
|      - | 1438 | `		ph7_int64 n;` |
|      - | 1439 | `		char zBuf[8192];` |
|    101 | 1440 | `		if( nEndLen > 0 && (sxu32)nEndLen <= nWindow ){` |
|      - | 1441 | `			/* The ending must END inside the window to count. Resume the scan` |
|      - | 1442 | `			 * where the previous fill left off — a candidate can straddle two` |
|      - | 1443 | `			 * fills, so back up by the ending's length less one. */` |
|      - | 1444 | `			sxu32 i;` |
|  40179 | 1445 | `			for( i = iScanFrom ; i + (sxu32)nEndLen <= nWindow ; i++ ){` |
|  40147 | 1446 | `				if( zData[i] == zEnding[0] && SyMemcmp(&zData[i],zEnding,(sxu32)nEndLen) == 0 ){` |
|     27 | 1447 | `					pDev->nOfft += i + (sxu32)nEndLen;` |
|     27 | 1448 | `					ph7_result_string(pCtx,zData,(int)i);` |
|     43 | 1449 | `					return PH7_OK;` |
|      - | 1450 | `				}` |
|  20063 | 1451 | `			}` |
|     34 | 1452 | `			iScanFrom = i;` |
|     16 | 1453 | `		}` |
|     77 | 1454 | `		if( (ph7_int64)nAvail >= nMaxLen ){` |
|      - | 1455 | `			/* Window full with no ending inside it: hand the window back raw,` |
|      - | 1456 | `			 * anything past it (an ending included) stays buffered. */` |
|     18 | 1457 | `			pDev->nOfft += (sxu32)nMaxLen;` |
|     18 | 1458 | `			ph7_result_string(pCtx,zData,(int)nMaxLen);` |
|     18 | 1459 | `			return PH7_OK;` |
|      - | 1460 | `		}` |
|     61 | 1461 | `		if( bEof ){` |
|      - | 1462 | `			/* EOF: the remainder as-is, false when nothing is left. */` |
|     18 | 1463 | `			if( nAvail > 0 ){` |
|     12 | 1464 | `				pDev->nOfft += nAvail;` |
|     12 | 1465 | `				ph7_result_string(pCtx,zData,(int)nAvail);` |
|      7 | 1466 | `			}else{` |
|      8 | 1467 | `				ph7_result_bool(pCtx,0);` |
|      - | 1468 | `			}` |
|     18 | 1469 | `			return PH7_OK;` |
|      - | 1470 | `		}` |
|     45 | 1471 | `		n = IoPrivateDeviceRead(pDev,zBuf,(ph7_int64)sizeof(zBuf));` |
|     45 | 1472 | `		if( n < 1 ){` |
|     18 | 1473 | `			bEof = 1;` |
|     18 | 1474 | `			if( n == 0 ){` |
|     18 | 1475 | `				pDev->bEof = 1;` |
|      8 | 1476 | `			}` |
|     18 | 1477 | `			continue;` |
|      - | 1478 | `		}` |
|     29 | 1479 | `		if( SXRET_OK != SyBlobAppend(&pDev->sBuffer,zBuf,(sxu32)n) ){` |
|    ! 0 | 1480 | `			return PH7_ContextMemoryError(pCtx);` |
|      - | 1481 | `		}` |
|      3 | 1482 | `	}` |
|     34 | 1483 | `}` |
|      - | 1484 | `/*` |
|      - | 1485 | ` * string fread(resource $handle,int64 $length)` |
|      - | 1486 | ` *  Binary-safe file read.` |
|      - | 1487 | ` * Parameters` |
|      - | 1488 | ` *  $handle` |
|      - | 1489 | ` *   The file pointer.` |
|      - | 1490 | ` * $length` |
|      - | 1491 | ` *  Up to length number of bytes read.` |
|      - | 1492 | ` * Return` |
|      - | 1493 | ` *  The data readen on success or FALSE on failure.` |
|      - | 1494 | ` */` |
|    242 | 1495 | `PH7_PRIVATE int PH7_builtin_fread(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      5 | 1496 | `{` |
|      - | 1497 | `	const ph7_io_stream *pStream;` |
|      - | 1498 | `	io_private *pDev;` |
|      - | 1499 | `	ph7_int64 nRead;` |
|      - | 1500 | `	void *pBuf;` |
|      - | 1501 | `	int nLen;` |
|    247 | 1502 | `	if( nArg < 1 \|\| !ph7_value_is_resource(apArg[0]) ){` |
|      - | 1503 | `		/* Missing/Invalid arguments,return FALSE */` |
|    ! 0 | 1504 | `		ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"Expecting an IO handle");` |
|    ! 0 | 1505 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 1506 | `		return PH7_OK;` |
|      - | 1507 | `	}` |
|      - | 1508 | `	/* Extract our private data */` |
|    247 | 1509 | `	pDev = (io_private *)ph7_value_to_resource(apArg[0]);` |
|      - | 1510 | `	/* Make sure we are dealing with a valid io_private instance */` |
|    247 | 1511 | `	if( IO_PRIVATE_INVALID(pDev) ){` |
|      - | 1512 | `		/*Expecting an IO handle */` |
|    ! 0 | 1513 | `		ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"Expecting an IO handle");` |
|    ! 0 | 1514 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 1515 | `		return PH7_OK;` |
|      - | 1516 | `	}` |
|      - | 1517 | `	/* Point to the target IO stream device */` |
|    247 | 1518 | `	pStream = pDev->pStream;` |
|    247 | 1519 | `	if( pStream == 0  ){` |
|    ! 0 | 1520 | `		ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,` |
|      - | 1521 | `			"IO routine(%s) not implemented in the underlying stream(%s) device,PH7 is returning FALSE",` |
|    ! 0 | 1522 | `			ph7_function_name(pCtx),pStream ? pStream->zName : "null_stream"` |
|      - | 1523 | `			);` |
|    ! 0 | 1524 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 1525 | `		return PH7_OK;` |
|      - | 1526 | `	}` |
|    247 | 1527 | `        nLen = 4096;` |
|    247 | 1528 | `	if( nArg > 1 ){` |
|      - | 1529 | `	  /* PHP 8 raises a catchable ValueError for a non-positive length; the` |
|      - | 1530 | `	   * $length parameter is non-nullable, so a NULL is rejected upstream by` |
|      - | 1531 | `	   * the central type screen (the recorded null-policy divergence). */` |
|    247 | 1532 | `	  ph7_int64 nWant = ph7_value_to_int64(apArg[1]);` |
|    247 | 1533 | `	  if( nWant < 1 ){` |
|      5 | 1534 | `		return PH7_VmThrowException(pCtx,"ValueError",` |
|      - | 1535 | `			"fread(): Argument #2 ($length) must be greater than 0");` |
|      - | 1536 | `	  }` |
|    243 | 1537 | `	  nLen = (int)nWant;` |
|    243 | 1538 | `	  if( nLen < 1 ){` |
|      - | 1539 | `		/* A > INT_MAX length overflowed the int cast; read a single chunk` |
|      - | 1540 | `		 * (StreamRead returns only what the stream holds) instead of` |
|      - | 1541 | `		 * over-allocating -- matching the pre-existing behavior for lengths` |
|      - | 1542 | `		 * that do not fit an int. */` |
|    ! 0 | 1543 | `		nLen = 4096;` |
|    ! 0 | 1544 | `	  }` |
|    119 | 1545 | `        }` |
|      - | 1546 | `	/* Allocate enough buffer */` |
|    243 | 1547 | `	pBuf = ph7_context_alloc_chunk(pCtx,(unsigned int)nLen,FALSE,FALSE);` |
|    243 | 1548 | `	if( pBuf == 0 ){` |
|    ! 0 | 1549 | `		ph7_context_throw_error(pCtx,PH7_CTX_ERR,"PH7 is running out of memory");` |
|    ! 0 | 1550 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 1551 | `		return PH7_OK;` |
|      - | 1552 | `	}` |
|      - | 1553 | `	/* Perform the requested operation */` |
|    243 | 1554 | `	errno = 0;` |
|    243 | 1555 | `	nRead = PH7_StreamRead(pDev,pBuf,(ph7_int64)nLen);` |
|    243 | 1556 | `	if( nRead < 0 && (errno == EAGAIN \|\| errno == EWOULDBLOCK) ){` |
|      - | 1557 | `		/* Nothing had ARRIVED yet, which is not a failure: php answers "" for a` |
|      - | 1558 | ``		 * read that could not proceed and reserves `false` for one that broke.`` |
|      - | 1559 | `		 * The question is answered by errno rather than by a per-handle flag —` |
|      - | 1560 | `		 * two handles can share one descriptor (every php://stdin is fd 0), so` |
|      - | 1561 | `		 * a flag on the handle that set the mode answers wrongly for its` |
|      - | 1562 | `		 * siblings, and a genuine EBADF on a non-blocking write-only handle` |
|      - | 1563 | `		 * would come back as "" rather than false. When a TIMEOUT is what` |
|      - | 1564 | ``		 * expired, php reports false and sets the metadata's `timed_out`. */`` |
|      7 | 1565 | `		if( pDev->bHasTimeout && !pDev->bNonBlock ){` |
|      - | 1566 | `			/* A handle in NON-BLOCKING mode is the other case: it answers "" for` |
|      - | 1567 | `			 * a read that found nothing whether or not a timeout is armed, and` |
|      - | 1568 | ``			 * every socket now carries `default_socket_timeout`. */`` |
|      3 | 1569 | `			pDev->bTimedOut = 1;` |
|      3 | 1570 | `			ph7_result_bool(pCtx,0);` |
|      2 | 1571 | `		}else{` |
|      5 | 1572 | `			ph7_result_string(pCtx,"",0);` |
|      1 | 1573 | `		}` |
|    240 | 1574 | `	}else if( nRead < 0 ){` |
|      - | 1575 | `		/* A real IO error, which is php's other false here. */` |
|     27 | 1576 | `		StreamReportReadFailure(pCtx,pDev);` |
|     27 | 1577 | `		ph7_result_bool(pCtx,0);` |
|     15 | 1578 | `	}else{` |
|      - | 1579 | `		/* Make a copy of the data just read. Zero bytes is EOF, not a failure:` |
|      - | 1580 | `		 * php answers "" for it (php_stream_read returns 0 and the empty string` |
|      - | 1581 | `		 * rides through), where PHL answered FALSE — so the ordinary` |
|      - | 1582 | ``		 * `while (!feof($f)) $buf .= fread($f, 8192);` loop ended on a value`` |
|      - | 1583 | ``		 * that means "the read failed" and a `=== false` guard fired at EOF. */`` |
|    213 | 1584 | `		ph7_result_string(pCtx,(const char *)pBuf,nRead > 0 ? (int)nRead : 0);` |
|      - | 1585 | `	}` |
|      - | 1586 | `	/* Release the buffer */` |
|    243 | 1587 | `	ph7_context_free_chunk(pCtx,pBuf);` |
|    243 | 1588 | `	return PH7_OK;` |
|    126 | 1589 | `}` |
|      - | 1590 | `/*` |
|      - | 1591 | ` * array fgetcsv(resource $handle [, int $length = 0` |
|      - | 1592 | ` *         [,string $delimiter = ','[,string $enclosure = '"'[,string $escape='\\']]]])` |
|      - | 1593 | ` * Gets line from file pointer and parse for CSV fields.` |
|      - | 1594 | ` * Parameters` |
|      - | 1595 | ` * $handle` |
|      - | 1596 | ` *   The file pointer.` |
|      - | 1597 | ` * $length` |
|      - | 1598 | ` *  Reading ends when length - 1 bytes have been read, on a newline` |
|      - | 1599 | ` *  (which is included in the return value), or on EOF (whichever comes first).` |
|      - | 1600 | ` *  If no length is specified, it will keep reading from the stream until it reaches` |
|      - | 1601 | ` *  the end of the line.` |
|      - | 1602 | ` * $delimiter` |
|      - | 1603 | ` *   Set the field delimiter (one character only).` |
|      - | 1604 | ` * $enclosure` |
|      - | 1605 | ` *   Set the field enclosure character (one character only).` |
|      - | 1606 | ` * $escape` |
|      - | 1607 | ` *   Set the escape character (one character only). Defaults as a backslash (\)` |
|      - | 1608 | ` * Return` |
|      - | 1609 | ` *  Returns a string of up to length - 1 bytes read from the file pointed to by handle.` |
|      - | 1610 | ` *  If there is no more data to read in the file pointer, then FALSE is returned.` |
|      - | 1611 | ` *  If an error occurs, FALSE is returned.` |
|      - | 1612 | ` */` |
|     68 | 1613 | `PH7_PRIVATE int PH7_builtin_fgetcsv(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 1614 | `{` |
|      - | 1615 | `	const ph7_io_stream *pStream;` |
|      - | 1616 | `	const char *zLine;` |
|      - | 1617 | `	io_private *pDev;` |
|      - | 1618 | `	ph7_int64 n,nLen;` |
|     69 | 1619 | `	int delim  = ',';   /* Delimiter */` |
|     69 | 1620 | `	int encl   = '"' ;  /* Enclosure */` |
|     69 | 1621 | `	int escape = '\\';  /* Escape character */` |
|     69 | 1622 | `	if( nArg < 1 \|\| !ph7_value_is_resource(apArg[0]) ){` |
|      - | 1623 | `		/* Missing/Invalid arguments,return FALSE */` |
|    ! 0 | 1624 | `		ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"Expecting an IO handle");` |
|    ! 0 | 1625 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 1626 | `		return PH7_OK;` |
|      - | 1627 | `	}` |
|      - | 1628 | `	/* Extract our private data */` |
|     69 | 1629 | `	pDev = (io_private *)ph7_value_to_resource(apArg[0]);` |
|      - | 1630 | `	/* Make sure we are dealing with a valid io_private instance */` |
|     69 | 1631 | `	if( IO_PRIVATE_INVALID(pDev) ){` |
|      - | 1632 | `		/*Expecting an IO handle */` |
|    ! 0 | 1633 | `		ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"Expecting an IO handle");` |
|    ! 0 | 1634 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 1635 | `		return PH7_OK;` |
|      - | 1636 | `	}` |
|      - | 1637 | `	/* Point to the target IO stream device */` |
|     69 | 1638 | `	pStream = pDev->pStream;` |
|     69 | 1639 | `	if( pStream == 0  ){` |
|    ! 0 | 1640 | `		ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,` |
|      - | 1641 | `			"IO routine(%s) not implemented in the underlying stream(%s) device,PH7 is returning FALSE",` |
|    ! 0 | 1642 | `			ph7_function_name(pCtx),pStream ? pStream->zName : "null_stream"` |
|      - | 1643 | `			);` |
|    ! 0 | 1644 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 1645 | `		return PH7_OK;` |
|      - | 1646 | `	}` |
|     69 | 1647 | `	if( nArg > 2 ){` |
|      - | 1648 | `		/* php validates $separator/$enclosure/$escape BEFORE $length (probed` |
|      - | 1649 | `		 * ordering) and even when the stream is already at EOF. */` |
|     67 | 1650 | `		sxi32 rc = PH7_CsvCharArg(pCtx,apArg[2],3,"separator",0,&delim);` |
|     67 | 1651 | `		if( rc != PH7_OK ){` |
|      7 | 1652 | `			return rc;` |
|      - | 1653 | `		}` |
|     61 | 1654 | `		if( nArg > 3 ){` |
|     61 | 1655 | `			rc = PH7_CsvCharArg(pCtx,apArg[3],4,"enclosure",0,&encl);` |
|     61 | 1656 | `			if( rc != PH7_OK ){` |
|      3 | 1657 | `				return rc;` |
|      - | 1658 | `			}` |
|     59 | 1659 | `			if( nArg > 4 ){` |
|     59 | 1660 | `				rc = PH7_CsvCharArg(pCtx,apArg[4],5,"escape",1,&escape);` |
|     59 | 1661 | `				if( rc != PH7_OK ){` |
|      3 | 1662 | `					return rc;` |
|      - | 1663 | `				}` |
|     28 | 1664 | `			}` |
|     28 | 1665 | `		}` |
|     28 | 1666 | `	}` |
|     59 | 1667 | `	nLen = -1;` |
|     59 | 1668 | `	if( nArg > 1 && !ph7_value_is_null(apArg[1]) ){` |
|      - | 1669 | `		/* Maximum data to read. PHP 8 raises a catchable ValueError when the` |
|      - | 1670 | `		 * length is negative or hits PHP_INT_MAX (the valid range is` |
|      - | 1671 | `		 * 0..PHP_INT_MAX-1, where 0/NULL both mean "no limit"). */` |
|     49 | 1672 | `		nLen = ph7_value_to_int64(apArg[1]);` |
|     49 | 1673 | `		if( nLen < 0 \|\| nLen >= SXI64_HIGH ){` |
|      3 | 1674 | `			return PH7_VmThrowException(pCtx,"ValueError",` |
|      - | 1675 | `				"fgetcsv(): Argument #2 ($length) must be between 0 and 9223372036854775806");` |
|      - | 1676 | `		}` |
|      - | 1677 | `		/* 0 means "no limit", which StreamReadLine already treats as unlimited. */` |
|     23 | 1678 | `	}` |
|      - | 1679 | `	/* Perform the requested operation */` |
|     57 | 1680 | `	n = StreamReadLine(pDev,&zLine,nLen);` |
|     57 | 1681 | `	if( n < 1 ){` |
|      - | 1682 | `		/* EOF or IO error,return FALSE */` |
|     13 | 1683 | `		StreamReportReadFailure(pCtx,pDev);` |
|     13 | 1684 | `		ph7_result_bool(pCtx,0);` |
|      7 | 1685 | `	}else{` |
|      - | 1686 | `		ph7_value *pArray;` |
|      - | 1687 | `		SyBlob sRec;` |
|      - | 1688 | `		PH7_CsvScan sScan;` |
|      - | 1689 | `		/* Create our array */` |
|     45 | 1690 | `		pArray = ph7_context_new_array(pCtx);` |
|     45 | 1691 | `		if( pArray == 0 ){` |
|    ! 0 | 1692 | `			ph7_context_throw_error(pCtx,PH7_CTX_ERR,"PH7 is running out of memory");` |
|    ! 0 | 1693 | `			ph7_result_null(pCtx);` |
|    ! 0 | 1694 | `			return PH7_OK;` |
|      - | 1695 | `		}` |
|      - | 1696 | `		/* A RECORD is not a line: an enclosure that is still open when the line` |
|      - | 1697 | `		 * ends means the value contains the newline and the record continues on` |
|      - | 1698 | `		 * the next one. Parsing a single line and stopping split such a value` |
|      - | 1699 | `		 * across two rows, with the halves quoted wrong. The whole record is` |
|      - | 1700 | `		 * gathered FIRST and parsed once -- the scan below carries its position` |
|      - | 1701 | `		 * across the appends, so a stray quote costs one pass over the file` |
|      - | 1702 | `		 * rather than one per line. */` |
|     45 | 1703 | `		SyBlobInit(&sRec,&pCtx->pVm->sAllocator);` |
|     45 | 1704 | `		SyBlobAppend(&sRec,(const void *)zLine,(sxu32)n);` |
|     45 | 1705 | `		PH7_CsvScanInit(&sScan);` |
|     55 | 1706 | `		while( PH7_CsvScanOpen(&sScan,(const char *)SyBlobData(&sRec),` |
|     27 | 1707 | `				SyBlobLength(&sRec),delim,encl,escape) ){` |
|     13 | 1708 | `			if( SyBlobLength(&sRec) >= (sxu32)SXI32_HIGH ){` |
|      - | 1709 | `				/* The parser measures in int; stop rather than wrap negative. */` |
|    ! 0 | 1710 | `				break;` |
|      - | 1711 | `			}` |
|      - | 1712 | `			/* Continuation reads are NOT capped by $length: php's limit applies` |
|      - | 1713 | `			 * to the first read of the record, and reusing it here ended the` |
|      - | 1714 | `			 * record on a chunk boundary in the middle of a quoted value. */` |
|     13 | 1715 | `			n = StreamReadLine(pDev,&zLine,0);` |
|     13 | 1716 | `			if( n < 1 ){` |
|      - | 1717 | `				/* EOF inside the enclosure: php answers what it has. */` |
|      3 | 1718 | `				break;` |
|      - | 1719 | `			}` |
|     11 | 1720 | `			SyBlobAppend(&sRec,(const void *)zLine,(sxu32)n);` |
|      1 | 1721 | `		}` |
|     67 | 1722 | `		PH7_ProcessCsv(pArray,(const char *)SyBlobData(&sRec),` |
|     44 | 1723 | `			(int)SyBlobLength(&sRec),delim,encl,escape,0);` |
|     45 | 1724 | `		SyBlobRelease(&sRec);` |
|      - | 1725 | `		/* Return the freshly created array  */` |
|     45 | 1726 | `		ph7_result_value(pCtx,pArray);` |
|      - | 1727 | `	}` |
|     57 | 1728 | `	return PH7_OK;` |
|     35 | 1729 | `}` |
|      - | 1730 | `/*` |
|      - | 1731 | ` * string fgetss(resource $handle [,int $length [,string $allowable_tags ]])` |
|      - | 1732 | ` *  Gets line from file pointer and strip HTML tags.` |
|      - | 1733 | ` * Parameters` |
|      - | 1734 | ` * $handle` |
|      - | 1735 | ` *   The file pointer.` |
|      - | 1736 | ` * $length` |
|      - | 1737 | ` *  Reading ends when length - 1 bytes have been read, on a newline` |
|      - | 1738 | ` *  (which is included in the return value), or on EOF (whichever comes first).` |
|      - | 1739 | ` *  If no length is specified, it will keep reading from the stream until it reaches` |
|      - | 1740 | ` *  the end of the line.` |
|      - | 1741 | ` * $allowable_tags` |
|      - | 1742 | ` *  You can use the optional second parameter to specify tags which should not be stripped.` |
|      - | 1743 | ` * Return` |
|      - | 1744 | ` *  Returns a string of up to length - 1 bytes read from the file pointed to by` |
|      - | 1745 | ` *  handle, with all HTML and PHP code stripped. If an error occurs, returns FALSE.` |
|      - | 1746 | ` */` |
|      2 | 1747 | `PH7_PRIVATE int PH7_builtin_fgetss(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 1748 | `{` |
|      - | 1749 | `	const ph7_io_stream *pStream;` |
|      - | 1750 | `	const char *zLine;` |
|      - | 1751 | `	io_private *pDev;` |
|      - | 1752 | `	ph7_int64 n,nLen;` |
|      3 | 1753 | `	if( nArg < 1 \|\| !ph7_value_is_resource(apArg[0]) ){` |
|      - | 1754 | `		/* Missing/Invalid arguments,return FALSE */` |
|    ! 0 | 1755 | `		ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"Expecting an IO handle");` |
|    ! 0 | 1756 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 1757 | `		return PH7_OK;` |
|      - | 1758 | `	}` |
|      - | 1759 | `	/* Extract our private data */` |
|      3 | 1760 | `	pDev = (io_private *)ph7_value_to_resource(apArg[0]);` |
|      - | 1761 | `	/* Make sure we are dealing with a valid io_private instance */` |
|      3 | 1762 | `	if( IO_PRIVATE_INVALID(pDev) ){` |
|      - | 1763 | `		/*Expecting an IO handle */` |
|    ! 0 | 1764 | `		ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"Expecting an IO handle");` |
|    ! 0 | 1765 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 1766 | `		return PH7_OK;` |
|      - | 1767 | `	}` |
|      - | 1768 | `	/* Point to the target IO stream device */` |
|      3 | 1769 | `	pStream = pDev->pStream;` |
|      3 | 1770 | `	if( pStream == 0  ){` |
|    ! 0 | 1771 | `		ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,` |
|      - | 1772 | `			"IO routine(%s) not implemented in the underlying stream(%s) device,PH7 is returning FALSE",` |
|    ! 0 | 1773 | `			ph7_function_name(pCtx),pStream ? pStream->zName : "null_stream"` |
|      - | 1774 | `			);` |
|    ! 0 | 1775 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 1776 | `		return PH7_OK;` |
|      - | 1777 | `	}` |
|      3 | 1778 | `	nLen = -1;` |
|      3 | 1779 | `	if( nArg > 1 ){` |
|      - | 1780 | `		/* Maximum data to read */` |
|    ! 0 | 1781 | `		nLen = ph7_value_to_int64(apArg[1]);` |
|    ! 0 | 1782 | `	}` |
|      - | 1783 | `	/* Perform the requested operation */` |
|      3 | 1784 | `	n = StreamReadLine(pDev,&zLine,nLen);` |
|      3 | 1785 | `	if( n < 1 ){` |
|      - | 1786 | `		/* EOF or IO error,return FALSE */` |
|    ! 0 | 1787 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 1788 | `	}else{` |
|      3 | 1789 | `		const char *zTaglist = 0;` |
|      3 | 1790 | `		int nTaglen = 0;` |
|      3 | 1791 | `		if( nArg > 2 && ph7_value_is_string(apArg[2]) ){` |
|      - | 1792 | `			/* Allowed tag */` |
|    ! 0 | 1793 | `			zTaglist = ph7_value_to_string(apArg[2],&nTaglen);` |
|    ! 0 | 1794 | `		}` |
|      - | 1795 | `		/* Process data just read */` |
|      3 | 1796 | `		PH7_StripTagsFromString(pCtx,zLine,(int)n,zTaglist,nTaglen,0);` |
|      - | 1797 | `	}` |
|      3 | 1798 | `	return PH7_OK;` |
|      2 | 1799 | `}` |
|      - | 1800 | `/*` |
|      - | 1801 | ` * string readdir(resource $dir_handle)` |
|      - | 1802 | ` *   Read entry from directory handle.` |
|      - | 1803 | ` * Parameter` |
|      - | 1804 | ` *  $dir_handle` |
|      - | 1805 | ` *   The directory handle resource previously opened with opendir().` |
|      - | 1806 | ` * Return` |
|      - | 1807 | ` *  Returns the filename on success or FALSE on failure.` |
|      - | 1808 | ` */` |
|  16400 | 1809 | `PH7_PRIVATE int PH7_builtin_readdir(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      5 | 1810 | `{` |
|      - | 1811 | `	const ph7_io_stream *pStream;` |
|      - | 1812 | `	io_private *pDev;` |
|      - | 1813 | `	int rc;` |
|  16405 | 1814 | `	if( nArg < 1 \|\| !ph7_value_is_resource(apArg[0]) ){` |
|      - | 1815 | `		/* Missing/Invalid arguments,return FALSE */` |
|    ! 0 | 1816 | `		ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"Expecting an IO handle");` |
|    ! 0 | 1817 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 1818 | `		return PH7_OK;` |
|      - | 1819 | `	}` |
|      - | 1820 | `	/* Extract our private data */` |
|  16405 | 1821 | `	pDev = (io_private *)ph7_value_to_resource(apArg[0]);` |
|      - | 1822 | `	/* Make sure we are dealing with a valid io_private instance */` |
|  16405 | 1823 | `	if( IO_PRIVATE_INVALID(pDev) ){` |
|      - | 1824 | `		/*Expecting an IO handle */` |
|    ! 0 | 1825 | `		ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"Expecting an IO handle");` |
|    ! 0 | 1826 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 1827 | `		return PH7_OK;` |
|      - | 1828 | `	}` |
|      - | 1829 | `	/* Point to the target IO stream device */` |
|  16405 | 1830 | `	pStream = pDev->pStream;` |
|  16405 | 1831 | `	if( pStream == 0  \|\| pStream->xReadDir == 0 ){` |
|    ! 0 | 1832 | `		ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,` |
|      - | 1833 | `			"IO routine(%s) not implemented in the underlying stream(%s) device,PH7 is returning FALSE",` |
|    ! 0 | 1834 | `			ph7_function_name(pCtx),pStream ? pStream->zName : "null_stream"` |
|      - | 1835 | `			);` |
|    ! 0 | 1836 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 1837 | `		return PH7_OK;` |
|      - | 1838 | `	}` |
|  16405 | 1839 | `	ph7_result_bool(pCtx,0);` |
|      - | 1840 | `	/* Perform the requested operation */` |
|  16405 | 1841 | `	rc = pStream->xReadDir(pDev->pHandle,pCtx);` |
|  16405 | 1842 | `	if( rc != PH7_OK ){` |
|      - | 1843 | `		/* Return FALSE */` |
|   1587 | 1844 | `		ph7_result_bool(pCtx,0);` |
|    791 | 1845 | `	}` |
|  16405 | 1846 | `	return PH7_OK;` |
|   8205 | 1847 | `}` |
|      - | 1848 | `/*` |
|      - | 1849 | ` * void rewinddir(resource $dir_handle)` |
|      - | 1850 | ` *   Rewind directory handle.` |
|      - | 1851 | ` * Parameter` |
|      - | 1852 | ` *  $dir_handle` |
|      - | 1853 | ` *   The directory handle resource previously opened with opendir().` |
|      - | 1854 | ` * Return` |
|      - | 1855 | ` *  FALSE on failure.` |
|      - | 1856 | ` */` |
|      6 | 1857 | `PH7_PRIVATE int PH7_builtin_rewinddir(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      2 | 1858 | `{` |
|      - | 1859 | `	const ph7_io_stream *pStream;` |
|      - | 1860 | `	io_private *pDev;` |
|      8 | 1861 | `	if( nArg < 1 \|\| !ph7_value_is_resource(apArg[0]) ){` |
|      - | 1862 | `		/* Missing/Invalid arguments,return FALSE */` |
|    ! 0 | 1863 | `		ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"Expecting an IO handle");` |
|    ! 0 | 1864 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 1865 | `		return PH7_OK;` |
|      - | 1866 | `	}` |
|      - | 1867 | `	/* Extract our private data */` |
|      8 | 1868 | `	pDev = (io_private *)ph7_value_to_resource(apArg[0]);` |
|      - | 1869 | `	/* Make sure we are dealing with a valid io_private instance */` |
|      8 | 1870 | `	if( IO_PRIVATE_INVALID(pDev) ){` |
|      - | 1871 | `		/*Expecting an IO handle */` |
|    ! 0 | 1872 | `		ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"Expecting an IO handle");` |
|    ! 0 | 1873 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 1874 | `		return PH7_OK;` |
|      - | 1875 | `	}` |
|      - | 1876 | `	/* Point to the target IO stream device */` |
|      8 | 1877 | `	pStream = pDev->pStream;` |
|      8 | 1878 | `	if( pStream == 0  \|\| pStream->xRewindDir == 0 ){` |
|    ! 0 | 1879 | `		ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,` |
|      - | 1880 | `			"IO routine(%s) not implemented in the underlying stream(%s) device,PH7 is returning FALSE",` |
|    ! 0 | 1881 | `			ph7_function_name(pCtx),pStream ? pStream->zName : "null_stream"` |
|      - | 1882 | `			);` |
|    ! 0 | 1883 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 1884 | `		return PH7_OK;` |
|      - | 1885 | `	}` |
|      - | 1886 | `	/* Perform the requested operation */` |
|      8 | 1887 | `	pStream->xRewindDir(pDev->pHandle);` |
|      8 | 1888 | `	return PH7_OK;` |
|      5 | 1889 | ` }` |
|      - | 1890 | `/* Forward declaration */` |
|      - | 1891 | `static void ReleaseIOPrivate(ph7_context *pCtx,io_private *pDev);` |
|      - | 1892 | `/*` |
|      - | 1893 | ` * void closedir(resource $dir_handle)` |
|      - | 1894 | ` *   Close directory handle.` |
|      - | 1895 | ` * Parameter` |
|      - | 1896 | ` *  $dir_handle` |
|      - | 1897 | ` *   The directory handle resource previously opened with opendir().` |
|      - | 1898 | ` * Return` |
|      - | 1899 | ` *  FALSE on failure.` |
|      - | 1900 | ` */` |
|   1590 | 1901 | `PH7_PRIVATE int PH7_builtin_closedir(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      5 | 1902 | `{` |
|      - | 1903 | `	const ph7_io_stream *pStream;` |
|      - | 1904 | `	io_private *pDev;` |
|   1595 | 1905 | `	if( nArg < 1 \|\| !ph7_value_is_resource(apArg[0]) ){` |
|      - | 1906 | `		/* Missing/Invalid arguments,return FALSE */` |
|    ! 0 | 1907 | `		ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"Expecting an IO handle");` |
|    ! 0 | 1908 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 1909 | `		return PH7_OK;` |
|      - | 1910 | `	}` |
|      - | 1911 | `	/* Extract our private data */` |
|   1595 | 1912 | `	pDev = (io_private *)ph7_value_to_resource(apArg[0]);` |
|      - | 1913 | `	/* Make sure we are dealing with a valid io_private instance */` |
|   1595 | 1914 | `	if( IO_PRIVATE_INVALID(pDev) ){` |
|      - | 1915 | `		/*Expecting an IO handle */` |
|    ! 0 | 1916 | `		ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"Expecting an IO handle");` |
|    ! 0 | 1917 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 1918 | `		return PH7_OK;` |
|      - | 1919 | `	}` |
|      - | 1920 | `	/* Point to the target IO stream device */` |
|   1595 | 1921 | `	pStream = pDev->pStream;` |
|   1595 | 1922 | `	if( pStream == 0  \|\| pStream->xCloseDir == 0 ){` |
|    ! 0 | 1923 | `		ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,` |
|      - | 1924 | `			"IO routine(%s) not implemented in the underlying stream(%s) device,PH7 is returning FALSE",` |
|    ! 0 | 1925 | `			ph7_function_name(pCtx),pStream ? pStream->zName : "null_stream"` |
|      - | 1926 | `			);` |
|    ! 0 | 1927 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 1928 | `		return PH7_OK;` |
|      - | 1929 | `	}` |
|      - | 1930 | `	/* Perform the requested operation */` |
|   1595 | 1931 | `	PH7_StreamFilterReleaseChains(pDev);` |
|   1595 | 1932 | `	pStream->xCloseDir(pDev->pHandle);` |
|      - | 1933 | `	/* Keep the handle alive but flag it closed (php: gettype()=='resource (closed)') */` |
|   1595 | 1934 | `	MarkIOPrivateClosed(pDev);` |
|   1595 | 1935 | `	return PH7_OK;` |
|    800 | 1936 | ` }` |
|      - | 1937 | `/*` |
|      - | 1938 | ` * resource opendir(string $path[,resource $context])` |
|      - | 1939 | ` *  Open directory handle.` |
|      - | 1940 | ` * Parameters` |
|      - | 1941 | ` * $path` |
|      - | 1942 | ` *   The directory path that is to be opened.` |
|      - | 1943 | ` * $context` |
|      - | 1944 | ` *   A context stream resource.` |
|      - | 1945 | ` * Return` |
|      - | 1946 | ` *  A directory handle resource on success,or FALSE on failure.` |
|      - | 1947 | ` */` |
|   1624 | 1948 | `PH7_PRIVATE int PH7_builtin_opendir(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      5 | 1949 | `{` |
|      - | 1950 | `	const ph7_io_stream *pStream;` |
|      - | 1951 | `	const char *zPath;` |
|      - | 1952 | `	io_private *pDev;` |
|   1629 | 1953 | `	int iLen,rc,bThrew = 0;` |
|   1629 | 1954 | `	if( nArg < 1 \|\| !ph7_value_is_string(apArg[0]) ){` |
|      - | 1955 | `		/* Missing/Invalid arguments,return FALSE */` |
|    ! 0 | 1956 | `		ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"Expecting a directory path");` |
|    ! 0 | 1957 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 1958 | `		return PH7_OK;` |
|      - | 1959 | `	}` |
|      - | 1960 | `	/* php refuses a resource that is not a stream-context. Nothing CONSUMES it` |
|      - | 1961 | `	 * here — dir_opendir() over a userland wrapper is not dispatched (§7.4` |
|      - | 1962 | `	 * slice-2 (e)) — but the refusal is the argument's contract. */` |
|   1629 | 1963 | `	PH7_StreamCtxFromArg(pCtx,nArg,apArg,1,"$context",0,&bThrew);` |
|   1629 | 1964 | `	if( bThrew ){` |
|      3 | 1965 | `		return PH7_OK;` |
|      - | 1966 | `	}` |
|      - | 1967 | `	/* Extract the target path */` |
|   1627 | 1968 | `	zPath  = ph7_value_to_string(apArg[0],&iLen);` |
|      - | 1969 | `	/* Try to extract a stream */` |
|   1627 | 1970 | `	pStream = PH7_VmGetStreamDevice(pCtx->pVm,&zPath,iLen);` |
|   1627 | 1971 | `	if( pStream == 0 ){` |
|    ! 0 | 1972 | `		VfsThrowNoDeviceWarning(pCtx,zPath,TRUE);` |
|    ! 0 | 1973 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 1974 | `		return PH7_OK;` |
|      - | 1975 | `	}` |
|   1627 | 1976 | `	if( pStream->xOpenDir == 0 ){` |
|    ! 0 | 1977 | `		ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,` |
|      - | 1978 | `			"IO routine(%s) not implemented in the underlying stream(%s) device",` |
|    ! 0 | 1979 | `			ph7_function_name(pCtx),pStream->zName` |
|      - | 1980 | `			);` |
|    ! 0 | 1981 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 1982 | `		return PH7_OK;` |
|      - | 1983 | `	}` |
|      - | 1984 | `	/* Allocate a new IO private instance */` |
|   1627 | 1985 | `	pDev = (io_private *)ph7_context_alloc_chunk(pCtx,sizeof(io_private),TRUE,FALSE);` |
|   1627 | 1986 | `	if( pDev == 0 ){` |
|    ! 0 | 1987 | `		ph7_context_throw_error(pCtx,PH7_CTX_ERR,"PH7 is running out of memory");` |
|    ! 0 | 1988 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 1989 | `		return PH7_OK;` |
|      - | 1990 | `	}` |
|      - | 1991 | `	/* Initialize the structure */` |
|   1627 | 1992 | `	InitIOPrivate(pCtx->pVm,pStream,pDev);` |
|      - | 1993 | `	/* Open the target directory. A device reaches the VM only through this` |
|      - | 1994 | `	 * argument -- its xOpenDir has no vm parameter -- and glob:// needs one to` |
|      - | 1995 | `	 * walk the filesystem and hold what it finds, so a caller that passed no` |
|      - | 1996 | `	 * context hands over a synthesized stack value carrying the VM, exactly as` |
|      - | 1997 | `	 * PH7_StreamOpenHandle() does for the byte-stream openers. */` |
|      - | 1998 | `	{` |
|      - | 1999 | `		ph7_value sDummy;` |
|   1627 | 2000 | `		ph7_value *pRes = nArg > 1 ? apArg[1] : 0;` |
|   1627 | 2001 | `		if( pRes == 0 ){` |
|    317 | 2002 | `			PH7_MemObjInit(pCtx->pVm,&sDummy);` |
|    317 | 2003 | `			pRes = &sDummy;` |
|    156 | 2004 | `		}` |
|   1627 | 2005 | `		rc = pStream->xOpenDir(zPath,pRes,&pDev->pHandle);` |
|   1627 | 2006 | `		if( pRes == &sDummy ){` |
|    317 | 2007 | `			PH7_MemObjRelease(&sDummy);` |
|    156 | 2008 | `		}` |
|      - | 2009 | `	}` |
|   1627 | 2010 | `	if( rc != PH7_OK ){` |
|      - | 2011 | ``		/* IO error: php WARNS here — `opendir(/nope): Failed to open directory: No`` |
|      - | 2012 | ``		 * such file or directory` — and PHL returned FALSE in silence. The message`` |
|      - | 2013 | `` 		 * names the ACTIVE function, which is how dir() gets php's `dir(...)` `` |
|      - | 2014 | `		 * wording out of the same call. */` |
|      - | 2015 | `#ifdef __WINNT__` |
|      5 | 2016 | `		if( pStream == &sWinFileStream ){` |
|      - | 2017 | `			/* php's plain-files opener on Windows warns with the system's own` |
|      - | 2018 | `			 * reason first, and only then fails the way every platform does. */` |
|      - | 2019 | `			char zSys[256];` |
|      5 | 2020 | `			int iSaved = errno;` |
|      5 | 2021 | `			unsigned long nCode = PH7_WinOpenDirReason(zSys,(int)sizeof(zSys));` |
|      5 | 2022 | `			if( nCode ){` |
|      5 | 2023 | `				PH7_VmThrowWarningFmt(pCtx->pVm,"%s(%s): %s (code: %lu)",` |
|      - | 2024 | `					ph7_function_name(pCtx),zPath,zSys,nCode);` |
|      - | 2025 | `			}` |
|      5 | 2026 | `			errno = iSaved;` |
|      - | 2027 | `		}` |
|      - | 2028 | `#endif` |
|     53 | 2029 | `		PH7_VmThrowWarningFmt(pCtx->pVm,"%s(%s): Failed to open directory: %s",` |
|     32 | 2030 | `			ph7_function_name(pCtx),zPath,VfsStrerror(errno));` |
|     37 | 2031 | `		ReleaseIOPrivate(pCtx,pDev);` |
|     37 | 2032 | `		ph7_result_bool(pCtx,0);` |
|     21 | 2033 | `	}else{` |
|      - | 2034 | `		/* php's directory handles carry a mode and NO uri, and name their own` |
|      - | 2035 | ``		 * ops `dir` rather than the byte-stream STDIO. */`` |
|   1595 | 2036 | `		SetIOPrivateOpenedAs(pDev,0,0,"r",1);` |
|   1595 | 2037 | `		pDev->bDir = 1;` |
|      - | 2038 | `		/* Return the handle as a resource */` |
|   1595 | 2039 | `		ph7_result_resource(pCtx,pDev);` |
|      - | 2040 | `	}` |
|   1627 | 2041 | `	return PH7_OK;` |
|    817 | 2042 | `}` |
|      - | 2043 | `/*` |
|      - | 2044 | ``  * `dir(string $directory, $context = null): Directory\|false` `` |
|      - | 2045 | ` *` |
|      - | 2046 | ` * php's own dir() opens the stream and fills the object itself, which is why its` |
|      - | 2047 | ` * class needs no constructor. The open goes through the engine's opendir builtin` |
|      - | 2048 | `` * with THIS context, so the failure warning names `dir(...)` exactly as php's`` |
|      - | 2049 | ` * does; a failed open is FALSE, where the chunk's version handed back a Directory` |
|      - | 2050 | `` * whose handle was `false`.`` |
|      - | 2051 | ` */` |
|      6 | 2052 | `PH7_PRIVATE int PH7_builtin_dir(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      2 | 2053 | `{` |
|      - | 2054 | `	ph7_class_instance *pObj;` |
|      - | 2055 | `	ph7_class *pClass;` |
|      - | 2056 | `	ph7_value *pRet;` |
|      - | 2057 | `	int rc;` |
|      8 | 2058 | `	rc = PH7_builtin_opendir(pCtx,nArg,apArg);` |
|      8 | 2059 | `	if( rc != PH7_OK ){` |
|    ! 0 | 2060 | `		return rc;` |
|      - | 2061 | `	}` |
|      8 | 2062 | `	pRet = pCtx->pRet;` |
|      8 | 2063 | `	if( pRet == 0 \|\| (pRet->iFlags & MEMOBJ_RES) == 0 ){` |
|      3 | 2064 | `		ph7_result_bool(pCtx,0);` |
|      3 | 2065 | `		return PH7_OK;` |
|      - | 2066 | `	}` |
|      6 | 2067 | `	pClass = PH7_VmExtractClass(pCtx->pVm,"Directory",sizeof("Directory")-1,FALSE,0);` |
|      6 | 2068 | `	pObj = pClass ? PH7_NewClassInstance(pCtx->pVm,pClass) : 0;` |
|      6 | 2069 | `	if( pObj == 0 ){` |
|    ! 0 | 2070 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 2071 | `		return PH7_OK;` |
|      - | 2072 | `	}` |
|      - | 2073 | `	/* php's order: the path first, then the handle (var_dump shows both). */` |
|      - | 2074 | `	{` |
|      6 | 2075 | `		int nPath = 0;` |
|      6 | 2076 | `		const char *zPath = nArg > 0 ? ph7_value_to_string(apArg[0],&nPath) : "";` |
|      6 | 2077 | `		PH7_NativeSetAttrStr(pCtx->pVm,pObj,"path",zPath,nPath);` |
|      - | 2078 | `	}` |
|      6 | 2079 | `	PH7_NativeSetProp(pCtx->pVm,pObj,"handle",sizeof("handle")-1,pRet);` |
|      6 | 2080 | `	PH7_NativeResultObject(pCtx,pObj);` |
|      6 | 2081 | `	return PH7_OK;` |
|      5 | 2082 | `}` |
|      - | 2083 | `/*` |
|      - | 2084 | ` * int readfile(string $filename[,bool $use_include_path = false [,resource $context ]])` |
|      - | 2085 | ` *  Reads a file and writes it to the output buffer.` |
|      - | 2086 | ` * Parameters` |
|      - | 2087 | ` *  $filename` |
|      - | 2088 | ` *   The filename being read.` |
|      - | 2089 | ` *  $use_include_path` |
|      - | 2090 | ` *   You can use the optional second parameter and set it to` |
|      - | 2091 | ` *   TRUE, if you want to search for the file in the include_path, too.` |
|      - | 2092 | ` *  $context` |
|      - | 2093 | ` *   A context stream resource.` |
|      - | 2094 | ` * Return` |
|      - | 2095 | ` *  The number of bytes read from the file on success or FALSE on failure.` |
|      - | 2096 | ` */` |
|      - | 2097 | `/*` |
|      - | 2098 | ` * php's IO failures are E_WARNINGs naming the function, the path and the system reason:` |
|      - | 2099 | ` *   file_get_contents(/nope): Failed to open stream: No such file or directory` |
|      - | 2100 | ` * PH7 raised an E_ERROR reading "func(): IO error while opening '/nope'" for every one of` |
|      - | 2101 | ` * them -- wrong severity, wrong text, and no reason. errno still holds the failing` |
|      - | 2102 | ` * syscall's code at this point (a successful call never clears it), which is where the` |
|      - | 2103 | ` * trailing reason comes from.` |
|      - | 2104 | ` */` |
|     12 | 2105 | `PH7_PRIVATE int PH7_builtin_readfile(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      4 | 2106 | `{` |
|     16 | 2107 | `	int use_include  = FALSE;` |
|      - | 2108 | `	const ph7_io_stream *pStream;` |
|      - | 2109 | `	ph7_int64 n,nRead;` |
|      - | 2110 | `	const char *zFile;` |
|      - | 2111 | `	char zBuf[8192];` |
|      - | 2112 | `	void *pHandle;` |
|      - | 2113 | `	phl_stream_ctx *pCtxRes;` |
|     16 | 2114 | `	int rc,nLen,bThrew = 0;` |
|     16 | 2115 | `	if( nArg < 1 \|\| !ph7_value_is_string(apArg[0]) ){` |
|      - | 2116 | `		/* Missing/Invalid arguments,return FALSE */` |
|    ! 0 | 2117 | `		ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"Expecting a file path");` |
|    ! 0 | 2118 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 2119 | `		return PH7_OK;` |
|      - | 2120 | `	}` |
|      - | 2121 | `	/* Extract the file path */` |
|     16 | 2122 | `	zFile = ph7_value_to_string(apArg[0],&nLen);` |
|      - | 2123 | `	/* Point to the target IO stream device */` |
|     16 | 2124 | `	pStream = PH7_VmGetStreamDevice(pCtx->pVm,&zFile,nLen);` |
|     16 | 2125 | `	if( pStream == 0 ){` |
|    ! 0 | 2126 | `		VfsThrowNoDeviceWarning(pCtx,zFile,FALSE);` |
|    ! 0 | 2127 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 2128 | `		return PH7_OK;` |
|      - | 2129 | `	}` |
|     16 | 2130 | `	if( nArg > 1 ){` |
|      6 | 2131 | `		use_include = ph7_value_to_bool(apArg[1]);` |
|      2 | 2132 | `	}` |
|      - | 2133 | ``	/* php's `?resource $context`: a resource that is not a stream-context is`` |
|      - | 2134 | `	 * refused, and NULL means the DEFAULT context — never "no context at all".` |
|      - | 2135 | `	 * The armed one describes exactly this open. */` |
|     16 | 2136 | `	pCtxRes = PH7_StreamCtxFromArg(pCtx,nArg,apArg,2,"$context",0,&bThrew);` |
|     16 | 2137 | `	if( bThrew ){` |
|      6 | 2138 | `		return PH7_OK;` |
|      - | 2139 | `	}` |
|     11 | 2140 | `	PH7_StreamCtxArm(pCtx->pVm,pCtxRes);` |
|      - | 2141 | `	/* Try to open the file in read-only mode */` |
|     15 | 2142 | `	pHandle = PH7_StreamOpenHandle(pCtx->pVm,pStream,zFile,PH7_IO_OPEN_RDONLY,` |
|      4 | 2143 | `		use_include,nArg > 2 ? apArg[2] : 0,FALSE,0,ph7_function_name(pCtx));` |
|     11 | 2144 | `	if( pHandle == 0 ){` |
|      9 | 2145 | `		VfsThrowOpenWarning(pCtx,zFile);` |
|      9 | 2146 | `		ph7_result_bool(pCtx,0);` |
|      9 | 2147 | `		return PH7_OK;` |
|      - | 2148 | `	}` |
|      - | 2149 | `	/* Perform the requested operation */` |
|      3 | 2150 | `	nRead = 0;` |
|      2 | 2151 | `	for(;;){` |
|      5 | 2152 | `		n = pStream->xRead(pHandle,zBuf,sizeof(zBuf));` |
|      5 | 2153 | `		if( n < 1 ){` |
|      - | 2154 | `			/* EOF or IO error,break immediately */` |
|      3 | 2155 | `			break;` |
|      - | 2156 | `		}` |
|      - | 2157 | `		/* Output data */` |
|      3 | 2158 | `		rc = ph7_context_output(pCtx,zBuf,(int)n);` |
|      3 | 2159 | `		if( rc == PH7_ABORT ){` |
|    ! 0 | 2160 | `			break;` |
|      - | 2161 | `		}` |
|      - | 2162 | `		/* Increment counter */` |
|      3 | 2163 | `		nRead += n;` |
|      1 | 2164 | `	}` |
|      - | 2165 | `	/* Close the stream */` |
|      3 | 2166 | `	PH7_StreamCloseHandle(pStream,pHandle);` |
|      - | 2167 | `	/* Total number of bytes readen */` |
|      3 | 2168 | `	ph7_result_int64(pCtx,nRead);` |
|      3 | 2169 | `	return PH7_OK;` |
|     10 | 2170 | `}` |
|      - | 2171 | `/*` |
|      - | 2172 | ` * string file_get_contents(string $filename[,bool $use_include_path = false` |
|      - | 2173 | ` *         [, resource $context [, int $offset = -1 [, int $maxlen ]]]])` |
|      - | 2174 | ` *  Reads entire file into a string.` |
|      - | 2175 | ` * Parameters` |
|      - | 2176 | ` *  $filename` |
|      - | 2177 | ` *   The filename being read.` |
|      - | 2178 | ` *  $use_include_path` |
|      - | 2179 | ` *   You can use the optional second parameter and set it to` |
|      - | 2180 | ` *   TRUE, if you want to search for the file in the include_path, too.` |
|      - | 2181 | ` *  $context` |
|      - | 2182 | ` *   A context stream resource.` |
|      - | 2183 | ` *  $offset` |
|      - | 2184 | ` *   The offset where the reading starts on the original stream.` |
|      - | 2185 | ` *  $maxlen` |
|      - | 2186 | ` *    Maximum length of data read. The default is to read until end of file` |
|      - | 2187 | ` *    is reached. Note that this parameter is applied to the stream processed by the filters.` |
|      - | 2188 | ` * Return` |
|      - | 2189 | ` *   The function returns the read data or FALSE on failure.` |
|      - | 2190 | ` */` |
|   9162 | 2191 | `PH7_PRIVATE int PH7_builtin_file_get_contents(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      5 | 2192 | `{` |
|      - | 2193 | `	const ph7_io_stream *pStream;` |
|      - | 2194 | `	ph7_int64 n,nRead,nMaxlen;` |
|   9167 | 2195 | `	int use_include  = FALSE;` |
|      - | 2196 | `	const char *zFile;` |
|      - | 2197 | `	char zBuf[8192];` |
|      - | 2198 | `	void *pHandle;` |
|      - | 2199 | `	phl_stream_ctx *pCtxRes;` |
|   9167 | 2200 | `	int nLen,bThrew = 0;` |
|      - | 2201 |  |
|   9167 | 2202 | `	if( nArg < 1 \|\| !ph7_value_is_string(apArg[0]) ){` |
|      - | 2203 | `		/* Missing/Invalid arguments,return FALSE */` |
|    ! 0 | 2204 | `		ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"Expecting a file path");` |
|    ! 0 | 2205 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 2206 | `		return PH7_OK;` |
|      - | 2207 | `	}` |
|      - | 2208 | `	/* Extract the file path */` |
|   9167 | 2209 | `	zFile = ph7_value_to_string(apArg[0],&nLen);` |
|      - | 2210 | `	/* PHP 8 validates $length (arg #5) up front, before the wrapper is resolved` |
|      - | 2211 | `	 * or the file opened, so a negative length raises its catchable ValueError` |
|      - | 2212 | `	 * even for a bad wrapper or a missing file; a NULL (the ?int default) reads` |
|      - | 2213 | `	 * the whole file. */` |
|   9167 | 2214 | `	if( nArg > 4 && !ph7_value_is_null(apArg[4]) ){` |
|     27 | 2215 | `		if( ph7_value_to_int64(apArg[4]) < 0 ){` |
|      5 | 2216 | `			return PH7_VmThrowException(pCtx,"ValueError",` |
|      - | 2217 | `				"file_get_contents(): Argument #5 ($length) must be greater than or equal to 0");` |
|      - | 2218 | `		}` |
|     11 | 2219 | `	}` |
|      - | 2220 | `	/* Point to the target IO stream device */` |
|   9163 | 2221 | `	pStream = PH7_VmGetStreamDevice(pCtx->pVm,&zFile,nLen);` |
|   9163 | 2222 | `	if( pStream == 0 ){` |
|     23 | 2223 | `		VfsThrowNoDeviceWarning(pCtx,zFile,FALSE);` |
|     23 | 2224 | `		ph7_result_bool(pCtx,0);` |
|     23 | 2225 | `		return PH7_OK;` |
|      - | 2226 | `	}` |
|   9141 | 2227 | `	nMaxlen = -1;` |
|   9141 | 2228 | `	if( nArg > 1 ){` |
|     37 | 2229 | `		use_include = ph7_value_to_bool(apArg[1]);` |
|     17 | 2230 | `	}` |
|      - | 2231 | ``	/* php's `?resource $context`: a resource that is not a stream-context is`` |
|      - | 2232 | `	 * refused, and NULL means the DEFAULT context — never "no context at all".` |
|      - | 2233 | `	 * The armed one describes exactly this open. */` |
|   9141 | 2234 | `	pCtxRes = PH7_StreamCtxFromArg(pCtx,nArg,apArg,2,"$context",0,&bThrew);` |
|   9141 | 2235 | `	if( bThrew ){` |
|      5 | 2236 | `		return PH7_OK;` |
|      - | 2237 | `	}` |
|   9137 | 2238 | `	PH7_StreamCtxArm(pCtx->pVm,pCtxRes);` |
|      - | 2239 | `	/* Try to open the file in read-only mode */` |
|   9137 | 2240 | `	pHandle = PH7_StreamOpenHandle(pCtx->pVm,pStream,zFile,PH7_IO_OPEN_RDONLY,use_include,nArg > 2 ? apArg[2] : 0,FALSE,0,ph7_function_name(pCtx));` |
|   9137 | 2241 | `	if( pHandle == 0 ){` |
|     37 | 2242 | `		VfsThrowOpenWarning(pCtx,zFile);` |
|     37 | 2243 | `		ph7_result_bool(pCtx,0);` |
|     37 | 2244 | `		return PH7_OK;` |
|      - | 2245 | `	}` |
|   9103 | 2246 | `	if( nArg > 3 ){` |
|      - | 2247 | `		/* Extract the offset */` |
|     25 | 2248 | `		n = ph7_value_to_int64(apArg[3]);` |
|     25 | 2249 | `		if( n > 0 ){` |
|      7 | 2250 | `			if( pStream->xSeek ){` |
|      - | 2251 | `				/* Seek to the desired offset */` |
|      7 | 2252 | `				pStream->xSeek(pHandle,n,0/*SEEK_SET*/);` |
|      3 | 2253 | `			}` |
|      3 | 2254 | `		}` |
|     25 | 2255 | `		if( nArg > 4 && !ph7_value_is_null(apArg[4]) ){` |
|      - | 2256 | `			/* Maximum data to read. An explicit 0 reads nothing (php returns` |
|      - | 2257 | `			 * ""); only an omitted or NULL length keeps the -1 "whole file"` |
|      - | 2258 | `			 * sentinel, so NULL must not collapse to 0 here. */` |
|     23 | 2259 | `			nMaxlen = ph7_value_to_int64(apArg[4]);` |
|     11 | 2260 | `		}` |
|     12 | 2261 | `	}` |
|      - | 2262 | `	/* Perform the requested operation. nMaxlen: -1 = whole file, 0 = read` |
|      - | 2263 | `	 * nothing, >0 = at most that many bytes. The nMaxlen==0 case falls straight` |
|      - | 2264 | `	 * through to the empty-string result below. */` |
|   9103 | 2265 | `	nRead = 0;` |
|  18253 | 2266 | `	while( nMaxlen != 0 ){` |
|      - | 2267 | `		/* Cap the chunk to the bytes STILL wanted (nMaxlen - nRead), not to the` |
|      - | 2268 | `		 * whole nMaxlen: with a limit above the buffer size the final chunk would` |
|      - | 2269 | `		 * otherwise overshoot and append past $length. */` |
|  18249 | 2270 | `		ph7_int64 nAsk = (ph7_int64)sizeof(zBuf);` |
|  18249 | 2271 | `		if( nMaxlen > 0 && (nMaxlen - nRead) < nAsk ){` |
|     17 | 2272 | `			nAsk = nMaxlen - nRead;` |
|      8 | 2273 | `		}` |
|  18249 | 2274 | `		n = pStream->xRead(pHandle,zBuf,nAsk);` |
|  18249 | 2275 | `		if( n < 1 ){` |
|      - | 2276 | `			/* EOF or IO error,break immediately */` |
|   9085 | 2277 | `			break;` |
|      - | 2278 | `		}` |
|      - | 2279 | `		/* Append data */` |
|   9169 | 2280 | `		ph7_result_string(pCtx,zBuf,(int)n);` |
|      - | 2281 | `		/* Increment read counter */` |
|   9169 | 2282 | `		nRead += n;` |
|   9169 | 2283 | `		if( nMaxlen > 0 && nRead >= nMaxlen ){` |
|      - | 2284 | `			/* Read limit reached */` |
|     15 | 2285 | `			break;` |
|      - | 2286 | `		}` |
|      5 | 2287 | `	}` |
|      - | 2288 | `	/* Close the stream */` |
|   9103 | 2289 | `	PH7_StreamCloseHandle(pStream,pHandle);` |
|      - | 2290 | `	/* A successfully opened but empty file yields "" in php (FALSE is only for an` |
|      - | 2291 | `	 * open failure, handled above); the read loop never set a string result, so` |
|      - | 2292 | `	 * force an empty string rather than leaving a null/FALSE result. */` |
|   9103 | 2293 | `	if( ph7_context_result_buf_length(pCtx) < 1 ){` |
|     99 | 2294 | `		ph7_result_string(pCtx,"",0);` |
|     47 | 2295 | `	}` |
|   9103 | 2296 | `	return PH7_OK;` |
|   4586 | 2297 | `}` |
|      - | 2298 | `/*` |
|      - | 2299 | ` * int file_put_contents(string $filename,mixed $data[,int $flags = 0[,resource $context]])` |
|      - | 2300 | ` *  Write a string to a file.` |
|      - | 2301 | ` * Parameters` |
|      - | 2302 | ` *  $filename` |
|      - | 2303 | ` *  Path to the file where to write the data.` |
|      - | 2304 | ` * $data` |
|      - | 2305 | ` *  The data to write(Must be a string).` |
|      - | 2306 | ` * $flags` |
|      - | 2307 | ` *  The value of flags can be any combination of the following` |
|      - | 2308 | ` * flags, joined with the binary OR (\|) operator.` |
|      - | 2309 | ` *   FILE_USE_INCLUDE_PATH 	Search for filename in the include directory. See include_path for more information.` |
|      - | 2310 | ` *   FILE_APPEND 	        If file filename already exists, append the data to the file instead of overwriting it.` |
|      - | 2311 | ` *   LOCK_EX 	            Acquire an exclusive lock on the file while proceeding to the writing.` |
|      - | 2312 | ` * context` |
|      - | 2313 | ` *  A context stream resource.` |
|      - | 2314 | ` * Return` |
|      - | 2315 | ` *  The function returns the number of bytes that were written to the file, or FALSE on failure.` |
|      - | 2316 | ` */` |
|      - | 2317 | `/*` |
|      - | 2318 | ` * Append a buffer to a file, creating it when absent, and raise php's open` |
|      - | 2319 | `` * warning (`f(/nope): Failed to open stream: …`) under the CALLING builtin's`` |
|      - | 2320 | ` * name when it cannot be opened. Returns PH7_OK or -1.` |
|      - | 2321 | ` *` |
|      - | 2322 | ` * This is error_log()'s message_type 3, factored here because that is where the` |
|      - | 2323 | ` * stream device, the open flags and the warning shape already live.` |
|      - | 2324 | ` */` |
|      6 | 2325 | `PH7_PRIVATE int PH7_VfsAppendFile(ph7_context *pCtx,const char *zFile,const void *pData,int nLen)` |
|      1 | 2326 | `{` |
|      - | 2327 | `	const ph7_io_stream *pStream;` |
|      - | 2328 | `	void *pHandle;` |
|      - | 2329 | `	int nPath;` |
|      7 | 2330 | `	if( zFile == 0 \|\| zFile[0] == 0 ){` |
|    ! 0 | 2331 | `		VfsThrowOpenWarning(pCtx,zFile);` |
|    ! 0 | 2332 | `		return -1;` |
|      - | 2333 | `	}` |
|      7 | 2334 | `	nPath = (int)SyStrlen(zFile);` |
|      7 | 2335 | `	pStream = PH7_VmGetStreamDevice(pCtx->pVm,&zFile,nPath);` |
|      7 | 2336 | `	if( pStream == 0 \|\| pStream->xWrite == 0 ){` |
|    ! 0 | 2337 | `		VfsThrowOpenWarning(pCtx,zFile);` |
|    ! 0 | 2338 | `		return -1;` |
|      - | 2339 | `	}` |
|     10 | 2340 | `	pHandle = PH7_StreamOpenHandle(pCtx->pVm,pStream,zFile,` |
|      3 | 2341 | `		PH7_IO_OPEN_CREATE\|PH7_IO_OPEN_RDWR\|PH7_IO_OPEN_APPEND,FALSE,0,FALSE,0,ph7_function_name(pCtx));` |
|      7 | 2342 | `	if( pHandle == 0 ){` |
|      3 | 2343 | `		VfsThrowOpenWarning(pCtx,zFile);` |
|      3 | 2344 | `		return -1;` |
|      - | 2345 | `	}` |
|      5 | 2346 | `	if( nLen > 0 && pStream->xWrite(pHandle,pData,nLen) < 0 ){` |
|    ! 0 | 2347 | `		PH7_StreamCloseHandle(pStream,pHandle);` |
|    ! 0 | 2348 | `		return -1;` |
|      - | 2349 | `	}` |
|      5 | 2350 | `	PH7_StreamCloseHandle(pStream,pHandle);` |
|      5 | 2351 | `	return PH7_OK;` |
|      4 | 2352 | `}` |
|  17304 | 2353 | `PH7_PRIVATE int PH7_builtin_file_put_contents(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      5 | 2354 | `{` |
|  17309 | 2355 | `	int use_include  = FALSE;` |
|      - | 2356 | `	const ph7_io_stream *pStream;` |
|      - | 2357 | `	const char *zFile;` |
|      - | 2358 | `	const char *zData;` |
|      - | 2359 | `	int iOpenFlags;` |
|      - | 2360 | `	void *pHandle;` |
|      - | 2361 | `	phl_stream_ctx *pCtxRes;` |
|      - | 2362 | `	int iFlags;` |
|  17309 | 2363 | `	int nLen,bThrew = 0;` |
|      - | 2364 |  |
|  17309 | 2365 | `	if( nArg < 2 \|\| !ph7_value_is_string(apArg[0]) ){` |
|      - | 2366 | `		/* Missing/Invalid arguments,return FALSE */` |
|    ! 0 | 2367 | `		ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"Expecting a file path");` |
|    ! 0 | 2368 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 2369 | `		return PH7_OK;` |
|      - | 2370 | `	}` |
|      - | 2371 | `	/* Extract the file path */` |
|  17309 | 2372 | `	zFile = ph7_value_to_string(apArg[0],&nLen);` |
|      - | 2373 | `	/* Point to the target IO stream device */` |
|  17309 | 2374 | `	pStream = PH7_VmGetStreamDevice(pCtx->pVm,&zFile,nLen);` |
|  17309 | 2375 | `	if( pStream == 0 ){` |
|    ! 0 | 2376 | `		VfsThrowNoDeviceWarning(pCtx,zFile,FALSE);` |
|    ! 0 | 2377 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 2378 | `		return PH7_OK;` |
|      - | 2379 | `	}` |
|      - | 2380 | `	/* Data to write */` |
|  17309 | 2381 | `	zData = ph7_value_to_string(apArg[1],&nLen);` |
|      - | 2382 | `	/* Try to open the file in read-write mode */` |
|  17309 | 2383 | `	iOpenFlags = PH7_IO_OPEN_CREATE\|PH7_IO_OPEN_RDWR\|PH7_IO_OPEN_TRUNC;` |
|      - | 2384 | `	/* Extract the flags */` |
|  17309 | 2385 | `	iFlags = 0;` |
|  17309 | 2386 | `	if( nArg > 2 ){` |
|      9 | 2387 | `		iFlags = ph7_value_to_int(apArg[2]);` |
|      9 | 2388 | `		if( iFlags & 0x01 /*FILE_USE_INCLUDE_PATH*/){` |
|    ! 0 | 2389 | `			use_include = TRUE;` |
|    ! 0 | 2390 | `		}` |
|      9 | 2391 | `		if( iFlags & 0x08 /* FILE_APPEND */){` |
|      - | 2392 | `			/* If the file already exists, append the data to the file` |
|      - | 2393 | `			 * instead of overwriting it.` |
|      - | 2394 | `			 */` |
|    ! 0 | 2395 | `			iOpenFlags &= ~PH7_IO_OPEN_TRUNC;` |
|      - | 2396 | `			/* Append mode */` |
|    ! 0 | 2397 | `			iOpenFlags \|= PH7_IO_OPEN_APPEND;` |
|    ! 0 | 2398 | `		}` |
|      3 | 2399 | `	}` |
|      - | 2400 | `	/* FILE_NO_DEFAULT_CONTEXT is the flag that means exactly "and do NOT fall` |
|      - | 2401 | `	 * back to the default context" — which is why it needed one to exist. */` |
|  25961 | 2402 | `	pCtxRes = PH7_StreamCtxFromArg(pCtx,nArg,apArg,3,"$context",` |
|  17304 | 2403 | `		(iFlags & 0x10) != 0,&bThrew);` |
|  17309 | 2404 | `	if( bThrew ){` |
|      6 | 2405 | `		return PH7_OK;` |
|      - | 2406 | `	}` |
|  17305 | 2407 | `	PH7_StreamCtxArm(pCtx->pVm,pCtxRes);` |
|  25955 | 2408 | `	pHandle = PH7_StreamOpenHandle(pCtx->pVm,pStream,zFile,iOpenFlags,use_include,` |
|   8650 | 2409 | `		nArg > 3 ? apArg[3] : 0,FALSE,FALSE,ph7_function_name(pCtx));` |
|  17305 | 2410 | `	if( pHandle == 0 ){` |
|      6 | 2411 | `		VfsThrowOpenWarning(pCtx,zFile);` |
|      6 | 2412 | `		ph7_result_bool(pCtx,0);` |
|      6 | 2413 | `		return PH7_OK;` |
|      - | 2414 | `	}` |
|  17301 | 2415 | `	if( nLen < 1 ){` |
|      - | 2416 | `		/* Empty data, file is created/truncated */` |
|    209 | 2417 | `		ph7_result_int64(pCtx,0);` |
|    209 | 2418 | `		PH7_StreamCloseHandle(pStream,pHandle);` |
|    209 | 2419 | `		return PH7_OK;` |
|      - | 2420 | `	}` |
|  17097 | 2421 | `	if( pStream->xWrite ){` |
|      - | 2422 | `		ph7_int64 n;` |
|  17097 | 2423 | `		if( (iFlags & 2/* LOCK_EX, php's value */) && pStream->xLock ){` |
|      - | 2424 | `			/* Try to acquire an exclusive lock */` |
|    ! 0 | 2425 | `			pStream->xLock(pHandle,1/* LOCK_EX */);` |
|    ! 0 | 2426 | `		}` |
|      - | 2427 | `		/* Perform the write operation */` |
|  17097 | 2428 | `		n = pStream->xWrite(pHandle,(const void *)zData,nLen);` |
|  17097 | 2429 | `		if( n < 0 ){` |
|      - | 2430 | `			/* IO error,return FALSE — with php's write-failure diagnostic. */` |
|      1 | 2431 | `			ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,` |
|      - | 2432 | `				"Write of %d bytes failed with errno=%d %s",` |
|    ! 0 | 2433 | `				(int)nLen,errno,VfsStrerror(errno));` |
|      1 | 2434 | `			ph7_result_bool(pCtx,0);` |
|      1 | 2435 | `		}else{` |
|      - | 2436 | `			/* Total number of bytes written */` |
|  17097 | 2437 | `			ph7_result_int64(pCtx,n);` |
|      - | 2438 | `		}` |
|   8551 | 2439 | `	}else{` |
|      - | 2440 | `		/* Read-only stream */` |
|    ! 0 | 2441 | `		ph7_context_throw_error_format(pCtx,PH7_CTX_ERR,` |
|      - | 2442 | `			"Read-only stream(%s): Cannot perform write operation",` |
|    ! 0 | 2443 | `			pStream ? pStream->zName : "null_stream"` |
|      - | 2444 | `			);` |
|    ! 0 | 2445 | `		ph7_result_bool(pCtx,0);` |
|      - | 2446 | `	}` |
|      - | 2447 | `	/* Close the handle */` |
|  17097 | 2448 | `	PH7_StreamCloseHandle(pStream,pHandle);` |
|  17097 | 2449 | `	return PH7_OK;` |
|   8657 | 2450 | `}` |
|      - | 2451 | `/*` |
|      - | 2452 | ` * array file(string $filename[,int $flags = 0[,resource $context]])` |
|      - | 2453 | ` *  Reads entire file into an array.` |
|      - | 2454 | ` * Parameters` |
|      - | 2455 | ` *  $filename` |
|      - | 2456 | ` *   The filename being read.` |
|      - | 2457 | ` *  $flags` |
|      - | 2458 | ` *   The optional parameter flags can be one, or more, of the following constants:` |
|      - | 2459 | ` *   FILE_USE_INCLUDE_PATH` |
|      - | 2460 | ` *       Search for the file in the include_path.` |
|      - | 2461 | ` *   FILE_IGNORE_NEW_LINES` |
|      - | 2462 | ` *       Do not add newline at the end of each array element` |
|      - | 2463 | ` *   FILE_SKIP_EMPTY_LINES` |
|      - | 2464 | ` *       Skip empty lines` |
|      - | 2465 | ` *  $context` |
|      - | 2466 | ` *   A context stream resource.` |
|      - | 2467 | ` * Return` |
|      - | 2468 | ` *   The function returns the read data or FALSE on failure.` |
|      - | 2469 | ` */` |
|     50 | 2470 | `PH7_PRIVATE int PH7_builtin_file(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      3 | 2471 | `{` |
|      - | 2472 | `	const char *zFile,*zPtr,*zEnd,*zBuf;` |
|      - | 2473 | `	ph7_value *pArray,*pLine;` |
|      - | 2474 | `	const ph7_io_stream *pStream;` |
|     53 | 2475 | `	int use_include = 0;` |
|      - | 2476 | `	io_private *pDev;` |
|      - | 2477 | `	phl_stream_ctx *pCtxRes;` |
|      - | 2478 | `	ph7_int64 n;` |
|      - | 2479 | `	int iFlags;` |
|     53 | 2480 | `	int nLen,bThrew = 0;` |
|      - | 2481 |  |
|     53 | 2482 | `	if( nArg < 1 \|\| !ph7_value_is_string(apArg[0]) ){` |
|      - | 2483 | `		/* Missing/Invalid arguments,return FALSE */` |
|    ! 0 | 2484 | `		ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"Expecting a file path");` |
|    ! 0 | 2485 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 2486 | `		return PH7_OK;` |
|      - | 2487 | `	}` |
|     53 | 2488 | `	iFlags = 0;` |
|     53 | 2489 | `	if( nArg > 1 ){` |
|      - | 2490 | `		/* php validates the mask FIRST — before the wrapper is resolved and before` |
|      - | 2491 | `		 * anything is allocated, so file("bogus://x", 8) is the ValueError and not a` |
|      - | 2492 | `		 * stream warning, and the throw cannot strand the io_private below (its chunk` |
|      - | 2493 | `		 * is not auto-released). file() accepts only USE_INCLUDE_PATH\|IGNORE_NEW_LINES\|` |
|      - | 2494 | `		 * SKIP_EMPTY_LINES\|NO_DEFAULT_CONTEXT (1\|2\|4\|16) — FILE_APPEND belongs to` |
|      - | 2495 | `		 * file_put_contents and is rejected here like any other stray bit. PHL masked` |
|      - | 2496 | `		 * the bits it knew and silently ignored the rest, so file($p, 8) and` |
|      - | 2497 | `		 * file($p, -1) read the file with a flag combination the caller never asked` |
|      - | 2498 | `		 * for. Read at 64-bit width so a high bit cannot be truncated into a valid` |
|      - | 2499 | `		 * mask. */` |
|     42 | 2500 | `		ph7_int64 nFlags = ph7_value_to_int64(apArg[1]);` |
|     42 | 2501 | `		if( nFlags & ~(ph7_int64)(0x01\|0x02\|0x04\|0x10) ){` |
|     13 | 2502 | `			return PH7_VmThrowException(pCtx,"ValueError",` |
|      - | 2503 | `				"file(): Argument #2 ($flags) must be a valid flag value");` |
|      - | 2504 | `		}` |
|     30 | 2505 | `		iFlags = (int)nFlags;` |
|     14 | 2506 | `	}` |
|      - | 2507 | `	/* Resolved here for the same reason the flag mask is: a refused $context` |
|      - | 2508 | `	 * must not strand the io_private chunk allocated below.` |
|      - | 2509 | `	 * FILE_NO_DEFAULT_CONTEXT is the flag that means exactly "and do NOT fall` |
|      - | 2510 | `	 * back to the default context" — which is why it needed one to exist. */` |
|     60 | 2511 | `	pCtxRes = PH7_StreamCtxFromArg(pCtx,nArg,apArg,2,"$context",` |
|     38 | 2512 | `		(iFlags & 0x10) != 0,&bThrew);` |
|     41 | 2513 | `	if( bThrew ){` |
|      3 | 2514 | `		return PH7_OK;` |
|      - | 2515 | `	}` |
|      - | 2516 | `	/* Extract the file path */` |
|     39 | 2517 | `	zFile = ph7_value_to_string(apArg[0],&nLen);` |
|      - | 2518 | `	/* Point to the target IO stream device */` |
|     39 | 2519 | `	pStream = PH7_VmGetStreamDevice(pCtx->pVm,&zFile,nLen);` |
|     39 | 2520 | `	if( pStream == 0 ){` |
|    ! 0 | 2521 | `		VfsThrowNoDeviceWarning(pCtx,zFile,FALSE);` |
|    ! 0 | 2522 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 2523 | `		return PH7_OK;` |
|      - | 2524 | `	}` |
|      - | 2525 | `	/* Allocate a new IO private instance */` |
|     39 | 2526 | `	pDev = (io_private *)ph7_context_alloc_chunk(pCtx,sizeof(io_private),TRUE,FALSE);` |
|     39 | 2527 | `	if( pDev == 0 ){` |
|    ! 0 | 2528 | `		ph7_context_throw_error(pCtx,PH7_CTX_ERR,"PH7 is running out of memory");` |
|    ! 0 | 2529 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 2530 | `		return PH7_OK;` |
|      - | 2531 | `	}` |
|      - | 2532 | `	/* Initialize the structure */` |
|     39 | 2533 | `	InitIOPrivate(pCtx->pVm,pStream,pDev);` |
|     39 | 2534 | `	if( iFlags & 0x01 /*FILE_USE_INCLUDE_PATH*/ ){` |
|      3 | 2535 | `		use_include = TRUE;` |
|      1 | 2536 | `	}` |
|      - | 2537 | `	/* Create the array and the working value */` |
|     39 | 2538 | `	pArray = ph7_context_new_array(pCtx);` |
|     39 | 2539 | `	pLine = ph7_context_new_scalar(pCtx);` |
|     39 | 2540 | `	if( pArray == 0 \|\| pLine == 0 ){` |
|    ! 0 | 2541 | `		ph7_context_throw_error(pCtx,PH7_CTX_ERR,"PH7 is running out of memory");` |
|    ! 0 | 2542 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 2543 | `		return PH7_OK;` |
|      - | 2544 | `	}` |
|     39 | 2545 | `	PH7_StreamCtxArm(pCtx->pVm,pCtxRes);` |
|      - | 2546 | `	/* Try to open the file in read-only mode */` |
|     39 | 2547 | `	pDev->pHandle = PH7_StreamOpenHandle(pCtx->pVm,pStream,zFile,PH7_IO_OPEN_RDONLY,use_include,nArg > 2 ? apArg[2] : 0,FALSE,0,ph7_function_name(pCtx));` |
|     39 | 2548 | `	if( pDev->pHandle == 0 ){` |
|      9 | 2549 | `		VfsThrowOpenWarning(pCtx,zFile);` |
|      9 | 2550 | `		ph7_result_bool(pCtx,0);` |
|      - | 2551 | `		/* Don't worry about freeing memory, everything will be released automatically` |
|      - | 2552 | `		 * as soon we return from this function.` |
|      - | 2553 | `		 */` |
|      9 | 2554 | `		return PH7_OK;` |
|      - | 2555 | `	}` |
|      - | 2556 | `	/* Perform the requested operation */` |
|     61 | 2557 | `	for(;;){` |
|      - | 2558 | `		/* Try to extract a line */` |
|    128 | 2559 | `		n = StreamReadLine(pDev,&zBuf,-1);` |
|    128 | 2560 | `		if( n < 1 ){` |
|      - | 2561 | `			/* EOF or IO error */` |
|     30 | 2562 | `			break;` |
|      - | 2563 | `		}` |
|      - | 2564 | `		/* Reset the cursor */` |
|     99 | 2565 | `		ph7_value_reset_string_cursor(pLine);` |
|      - | 2566 | `		/* Remove line ending if requested by the caller */` |
|     99 | 2567 | `		zPtr = zBuf;` |
|     99 | 2568 | `		zEnd = &zBuf[n];` |
|     99 | 2569 | `		if( iFlags & 0x02 /* FILE_IGNORE_NEW_LINES */ ){` |
|      - | 2570 | `			/* php strips ONE line ending: the LF, plus the CR that immediately` |
|      - | 2571 | `			 * precedes it. Not a run — "a\r\r\n" keeps its first CR and a` |
|      - | 2572 | `			 * CR-TERMINATED last line ("a\r", no LF) keeps it entirely. The` |
|      - | 2573 | `			 * platform-gated version this replaces left the CR on every CRLF line` |
|      - | 2574 | `			 * read on POSIX; a strip-all loop would instead eat data php keeps. */` |
|     55 | 2575 | `			if( zEnd > zPtr && zEnd[-1] == '\n' ){` |
|     43 | 2576 | `				n--;` |
|     43 | 2577 | `				zEnd--;` |
|     43 | 2578 | `				if( zEnd > zPtr && zEnd[-1] == '\r' ){` |
|     13 | 2579 | `					n--;` |
|     13 | 2580 | `					zEnd--;` |
|      6 | 2581 | `				}` |
|     21 | 2582 | `			}` |
|     27 | 2583 | `		}` |
|     99 | 2584 | `		if( iFlags & 0x04 /* FILE_SKIP_EMPTY_LINES */ ){` |
|      - | 2585 | `			/* php's "empty" is ZERO LENGTH after the optional newline strip — not` |
|      - | 2586 | `			 * "blank". PHL skipped any all-whitespace line, so a line of spaces was` |
|      - | 2587 | `			 * dropped where php keeps it, and without IGNORE_NEW_LINES a bare "\n"` |
|      - | 2588 | `			 * line (never zero-length, since the newline is still attached) was` |
|      - | 2589 | `			 * dropped too. Both are silent data loss from a read. */` |
|     31 | 2590 | `			if( zEnd <= zPtr ){` |
|      5 | 2591 | `				continue;` |
|      - | 2592 | `			}` |
|     13 | 2593 | `		}` |
|     95 | 2594 | `		ph7_value_string(pLine,zBuf,(int)(zEnd-zBuf));` |
|      - | 2595 | `		/* Insert line */` |
|     95 | 2596 | `		ph7_array_add_elem(pArray,0/* Automatic index assign*/,pLine);` |
|      1 | 2597 | `	}` |
|      - | 2598 | `	/* Close the stream */` |
|     30 | 2599 | `	PH7_StreamCloseHandle(pStream,pDev->pHandle);` |
|      - | 2600 | `	/* Release the io_private instance */` |
|     30 | 2601 | `	ReleaseIOPrivate(pCtx,pDev);` |
|      - | 2602 | `	/* Return the created array */` |
|     30 | 2603 | `	ph7_result_value(pCtx,pArray);` |
|     30 | 2604 | `	return PH7_OK;` |
|     28 | 2605 | `}` |
|      - | 2606 | `/*` |
|      - | 2607 | ` * bool copy(string $source,string $dest[,resource $context ] )` |
|      - | 2608 | ` *  Makes a copy of the file source to dest.` |
|      - | 2609 | ` * Parameters` |
|      - | 2610 | ` *  $source` |
|      - | 2611 | ` *   Path to the source file.` |
|      - | 2612 | ` *  $dest` |
|      - | 2613 | ` *   The destination path. If dest is a URL, the copy operation` |
|      - | 2614 | ` *   may fail if the wrapper does not support overwriting of existing files.` |
|      - | 2615 | ` *  $context` |
|      - | 2616 | ` *   A context stream resource.` |
|      - | 2617 | ` * Return` |
|      - | 2618 | ` *  TRUE on success or FALSE on failure.` |
|      - | 2619 | ` */` |
|      6 | 2620 | `PH7_PRIVATE int PH7_builtin_copy(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      2 | 2621 | `{` |
|      - | 2622 | `	const ph7_io_stream *pSin,*pSout;` |
|      - | 2623 | `	const char *zFile;` |
|      - | 2624 | `	char zBuf[8192];` |
|      - | 2625 | `	void *pIn,*pOut;` |
|      - | 2626 | `	phl_stream_ctx *pCtxRes;` |
|      - | 2627 | `	ph7_int64 n;` |
|      8 | 2628 | `	int nLen,bThrew = 0;` |
|      8 | 2629 | `	if( nArg < 2 \|\| !ph7_value_is_string(apArg[0]) \|\| !ph7_value_is_string(apArg[1])){` |
|      - | 2630 | `		/* Missing/Invalid arguments,return FALSE */` |
|    ! 0 | 2631 | `		ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"Expecting a source and a destination path");` |
|    ! 0 | 2632 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 2633 | `		return PH7_OK;` |
|      - | 2634 | `	}` |
|      - | 2635 | `	/* Extract the source name */` |
|      8 | 2636 | `	zFile = ph7_value_to_string(apArg[0],&nLen);` |
|      - | 2637 | `	/* Point to the target IO stream device */` |
|      8 | 2638 | `	pSin = PH7_VmGetStreamDevice(pCtx->pVm,&zFile,nLen);` |
|      8 | 2639 | `	if( pSin == 0 ){` |
|    ! 0 | 2640 | `		VfsThrowNoDeviceWarning(pCtx,zFile,FALSE);` |
|    ! 0 | 2641 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 2642 | `		return PH7_OK;` |
|      - | 2643 | `	}` |
|      - | 2644 | ``	/* php's `?resource $context`: a resource that is not a stream-context is`` |
|      - | 2645 | `	 * refused, and NULL means the DEFAULT context — never "no context at all".` |
|      - | 2646 | `	 * The armed one describes exactly this open. */` |
|      8 | 2647 | `	pCtxRes = PH7_StreamCtxFromArg(pCtx,nArg,apArg,2,"$context",0,&bThrew);` |
|      8 | 2648 | `	if( bThrew ){` |
|      3 | 2649 | `		return PH7_OK;` |
|      - | 2650 | `	}` |
|      6 | 2651 | `	PH7_StreamCtxArm(pCtx->pVm,pCtxRes);` |
|      - | 2652 | `	/* Try to open the source file in a read-only mode */` |
|      6 | 2653 | `	pIn = PH7_StreamOpenHandle(pCtx->pVm,pSin,zFile,PH7_IO_OPEN_RDONLY,FALSE,nArg > 2 ? apArg[2] : 0,FALSE,0,ph7_function_name(pCtx));` |
|      6 | 2654 | `	if( pIn == 0 ){` |
|      3 | 2655 | `		VfsThrowOpenWarning(pCtx,zFile);` |
|      3 | 2656 | `		ph7_result_bool(pCtx,0);` |
|      3 | 2657 | `		return PH7_OK;` |
|      - | 2658 | `	}` |
|      - | 2659 | `	/* Extract the destination name */` |
|      3 | 2660 | `	zFile = ph7_value_to_string(apArg[1],&nLen);` |
|      - | 2661 | `	/* Point to the target IO stream device */` |
|      3 | 2662 | `	pSout = PH7_VmGetStreamDevice(pCtx->pVm,&zFile,nLen);` |
|      3 | 2663 | `	if( pSout == 0 ){` |
|    ! 0 | 2664 | `		VfsThrowNoDeviceWarning(pCtx,zFile,FALSE);` |
|    ! 0 | 2665 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 2666 | `		PH7_StreamCloseHandle(pSin,pIn);` |
|    ! 0 | 2667 | `		return PH7_OK;` |
|      - | 2668 | `	}` |
|      3 | 2669 | `	if( pSout->xWrite == 0 ){` |
|    ! 0 | 2670 | `		ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,` |
|      - | 2671 | `			"IO routine(%s) not implemented in the underlying stream(%s) device,PH7 is returning FALSE",` |
|    ! 0 | 2672 | `			ph7_function_name(pCtx),pSin->zName` |
|      - | 2673 | `			);` |
|    ! 0 | 2674 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 2675 | `		PH7_StreamCloseHandle(pSin,pIn);` |
|    ! 0 | 2676 | `		return PH7_OK;` |
|      - | 2677 | `	}` |
|      - | 2678 | `	/* php hands the ONE context to both halves of the copy. */` |
|      3 | 2679 | `	PH7_StreamCtxArm(pCtx->pVm,pCtxRes);` |
|      - | 2680 | `	/* Try to open the destination file in a read-write mode */` |
|      4 | 2681 | `	pOut = PH7_StreamOpenHandle(pCtx->pVm,pSout,zFile,` |
|      1 | 2682 | `		PH7_IO_OPEN_CREATE\|PH7_IO_OPEN_TRUNC\|PH7_IO_OPEN_RDWR,FALSE,nArg > 2 ? apArg[2] : 0,FALSE,0,ph7_function_name(pCtx));` |
|      3 | 2683 | `	if( pOut == 0 ){` |
|    ! 0 | 2684 | `		VfsThrowOpenWarning(pCtx,zFile);` |
|    ! 0 | 2685 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 2686 | `		PH7_StreamCloseHandle(pSin,pIn);` |
|    ! 0 | 2687 | `		return PH7_OK;` |
|      - | 2688 | `	}` |
|      - | 2689 | `	/* Perform the requested operation */` |
|      2 | 2690 | `	for(;;){` |
|      - | 2691 | `		/* Read from source */` |
|      5 | 2692 | `		n = pSin->xRead(pIn,zBuf,sizeof(zBuf));` |
|      5 | 2693 | `		if( n < 1 ){` |
|      - | 2694 | `			/* EOF or IO error,break immediately */` |
|      3 | 2695 | `			break;` |
|      - | 2696 | `		}` |
|      - | 2697 | `		/* Write to dest */` |
|      3 | 2698 | `		n = pSout->xWrite(pOut,zBuf,n);` |
|      3 | 2699 | `		if( n < 1 ){` |
|      - | 2700 | `			/* IO error,break immediately */` |
|    ! 0 | 2701 | `			break;` |
|      - | 2702 | `		}` |
|      1 | 2703 | `	}` |
|      - | 2704 | `	/* Close the streams */` |
|      3 | 2705 | `	PH7_StreamCloseHandle(pSin,pIn);` |
|      3 | 2706 | `	PH7_StreamCloseHandle(pSout,pOut);` |
|      - | 2707 | `	/* Return TRUE */` |
|      3 | 2708 | `	ph7_result_bool(pCtx,1);` |
|      3 | 2709 | `	return PH7_OK;` |
|      5 | 2710 | `}` |
|      - | 2711 | `/*` |
|      - | 2712 | ` * array fstat(resource $handle)` |
|      - | 2713 | ` *  Gets information about a file using an open file pointer.` |
|      - | 2714 | ` * Parameters` |
|      - | 2715 | ` *  $handle` |
|      - | 2716 | ` *   The file pointer.` |
|      - | 2717 | ` * Return` |
|      - | 2718 | ` *  Returns an array with the statistics of the file or FALSE on failure.` |
|      - | 2719 | ` */` |
|     12 | 2720 | `PH7_PRIVATE int PH7_builtin_fstat(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 2721 | `{` |
|      - | 2722 | `	ph7_value *pArray,*pValue;` |
|      - | 2723 | `	const ph7_io_stream *pStream;` |
|      - | 2724 | `	io_private *pDev;` |
|     13 | 2725 | `	if( nArg < 1 \|\| !ph7_value_is_resource(apArg[0]) ){` |
|      - | 2726 | `		/* Missing/Invalid arguments,return FALSE */` |
|    ! 0 | 2727 | `		ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"Expecting an IO handle");` |
|    ! 0 | 2728 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 2729 | `		return PH7_OK;` |
|      - | 2730 | `	}` |
|      - | 2731 | `	/* Extract our private data */` |
|     13 | 2732 | `	pDev = (io_private *)ph7_value_to_resource(apArg[0]);` |
|      - | 2733 | `	/* Make sure we are dealing with a valid io_private instance */` |
|     13 | 2734 | `	if( IO_PRIVATE_INVALID(pDev) ){` |
|      - | 2735 | `		/* Expecting an IO handle */` |
|    ! 0 | 2736 | `		ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"Expecting an IO handle");` |
|    ! 0 | 2737 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 2738 | `		return PH7_OK;` |
|      - | 2739 | `	}` |
|      - | 2740 | `	/* A php://filter handle is the stream underneath it, and that is the one` |
|      - | 2741 | `	 * with a stat to answer. */` |
|     13 | 2742 | `	pDev = PH7_StreamUnwrap(pDev);` |
|      - | 2743 | `	/* Point to the target IO stream device */` |
|     13 | 2744 | `	pStream = pDev->pStream;` |
|     13 | 2745 | `	if( pStream == 0  \|\| pStream->xStat == 0){` |
|    ! 0 | 2746 | `		ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,` |
|      - | 2747 | `			"IO routine(%s) not implemented in the underlying stream(%s) device,PH7 is returning FALSE",` |
|    ! 0 | 2748 | `			ph7_function_name(pCtx),pStream ? pStream->zName : "null_stream"` |
|      - | 2749 | `			);` |
|    ! 0 | 2750 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 2751 | `		return PH7_OK;` |
|      - | 2752 | `	}` |
|      - | 2753 | `	/* Create the array and the working value */` |
|     13 | 2754 | `	pArray = ph7_context_new_array(pCtx);` |
|     13 | 2755 | `	pValue = ph7_context_new_scalar(pCtx);` |
|     13 | 2756 | `	if( pArray == 0 \|\| pValue == 0 ){` |
|    ! 0 | 2757 | `		ph7_context_throw_error(pCtx,PH7_CTX_ERR,"PH7 is running out of memory");` |
|    ! 0 | 2758 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 2759 | `		return PH7_OK;` |
|      - | 2760 | `	}` |
|      - | 2761 | `	/* Perform the requested operation */` |
|     13 | 2762 | `	pStream->xStat(pDev->pHandle,pArray,pValue);` |
|      - | 2763 | `	/* php answers the same thirteen fields twice -- numeric 0..12, then named` |
|      - | 2764 | `	 * (PH7_VfsStatDoubleUp); fstat() is stat()'s answer for an open handle and` |
|      - | 2765 | `	 * had the same missing half. */` |
|      - | 2766 | `	{` |
|     13 | 2767 | `		ph7_value *pFull = ph7_context_new_array(pCtx);` |
|     13 | 2768 | `		if( pFull && PH7_VfsStatDoubleUp(pArray,pFull) == PH7_OK ){` |
|      9 | 2769 | `			ph7_result_value(pCtx,pFull);` |
|      9 | 2770 | `			return PH7_OK;` |
|      - | 2771 | `		}` |
|      - | 2772 | `	}` |
|      - | 2773 | `	/* Return the freshly created array */` |
|      5 | 2774 | `	ph7_result_value(pCtx,pArray);` |
|      - | 2775 | `	/* Don't worry about freeing memory here,everything will be` |
|      - | 2776 | `	 * released automatically as soon we return from this function.` |
|      - | 2777 | `	 */` |
|      5 | 2778 | `	return PH7_OK;` |
|      7 | 2779 | `}` |
|      - | 2780 | `/*` |
|      - | 2781 | ` * php's socket ops report a failed send THEMSELVES, as an E_NOTICE naming the` |
|      - | 2782 | ` * count, the errno and its text, before the caller ever sees the false — so a` |
|      - | 2783 | ` * write to a peer that has gone is diagnosed rather than silent. Defined with` |
|      - | 2784 | ` * the socket device further down; the write paths that can reach a socket call` |
|      - | 2785 | ` * it where php's own do.` |
|      - | 2786 | ` */` |
|      - | 2787 | `static void SockReportWriteFailure(ph7_context *pCtx,io_private *pDev,int nLen);` |
|      - | 2788 | `/*` |
|      - | 2789 | ` * int fwrite(resource $handle,string $string[,int $length])` |
|      - | 2790 | ` *  Writes the contents of string to the file stream pointed to by handle.` |
|      - | 2791 | ` * Parameters` |
|      - | 2792 | ` *  $handle` |
|      - | 2793 | ` *   The file pointer.` |
|      - | 2794 | ` *  $string` |
|      - | 2795 | ` *   The string that is to be written.` |
|      - | 2796 | ` *  $length` |
|      - | 2797 | ` *   If the length argument is given, writing will stop after length bytes have been written` |
|      - | 2798 | ` *   or the end of string is reached, whichever comes first.` |
|      - | 2799 | ` * Return` |
|      - | 2800 | ` *  Returns the number of bytes written, or FALSE on error.` |
|      - | 2801 | ` */` |
|    419 | 2802 | `PH7_PRIVATE int PH7_builtin_fwrite(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      5 | 2803 | `{` |
|      - | 2804 | `	const ph7_io_stream *pStream;` |
|      - | 2805 | `	const char *zString;` |
|      - | 2806 | `	io_private *pDev;` |
|      - | 2807 | `	int nLen,n;` |
|    424 | 2808 | `	if( nArg < 2 \|\| !ph7_value_is_resource(apArg[0]) ){` |
|      - | 2809 | `		/* Missing/Invalid arguments,return FALSE */` |
|    ! 0 | 2810 | `		ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"Expecting an IO handle");` |
|    ! 0 | 2811 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 2812 | `		return PH7_OK;` |
|      - | 2813 | `	}` |
|      - | 2814 | `	/* Extract our private data */` |
|    424 | 2815 | `	pDev = (io_private *)ph7_value_to_resource(apArg[0]);` |
|      - | 2816 | `	/* Make sure we are dealing with a valid io_private instance */` |
|    424 | 2817 | `	if( IO_PRIVATE_INVALID(pDev) ){` |
|      - | 2818 | `		/* Expecting an IO handle */` |
|    ! 0 | 2819 | `		ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"Expecting an IO handle");` |
|    ! 0 | 2820 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 2821 | `		return PH7_OK;` |
|      - | 2822 | `	}` |
|      - | 2823 | `	/* Point to the target IO stream device */` |
|    424 | 2824 | `	pStream = pDev->pStream;` |
|    424 | 2825 | `	if( pStream == 0  \|\| pStream->xWrite == 0){` |
|    ! 0 | 2826 | `		ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,` |
|      - | 2827 | `			"IO routine(%s) not implemented in the underlying stream(%s) device,PH7 is returning FALSE",` |
|    ! 0 | 2828 | `			ph7_function_name(pCtx),pStream ? pStream->zName : "null_stream"` |
|      - | 2829 | `			);` |
|    ! 0 | 2830 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 2831 | `		return PH7_OK;` |
|      - | 2832 | `	}` |
|      - | 2833 | `	/* Extract the data to write */` |
|    424 | 2834 | `	zString = ph7_value_to_string(apArg[1],&nLen);` |
|    424 | 2835 | `	if( nArg > 2 && !ph7_value_is_null(apArg[2]) ){` |
|      - | 2836 | `		/* Maximum data length to write, read at 64-bit width so a large limit` |
|      - | 2837 | `		 * (PHP_INT_MAX as "no limit" is a common idiom) does not truncate to a` |
|      - | 2838 | `		 * negative int. php 8: NULL means "no limit" and a NEGATIVE $length` |
|      - | 2839 | `		 * writes NOTHING and returns 0 (probed; PHL used to ignore a negative` |
|      - | 2840 | `		 * and write the whole string). */` |
|     20 | 2841 | `		sxi64 nMax = ph7_value_to_int64(apArg[2]);` |
|     20 | 2842 | `		if( nMax < 0 ){` |
|      5 | 2843 | `			nLen = 0;` |
|     18 | 2844 | `		}else if( nMax < (sxi64)nLen ){` |
|     10 | 2845 | `			nLen = (int)nMax;` |
|      4 | 2846 | `		}` |
|      9 | 2847 | `	}` |
|    424 | 2848 | `	if( nLen < 1 ){` |
|      - | 2849 | `		/* Nothing to write */` |
|      9 | 2850 | `		ph7_result_int(pCtx,0);` |
|      9 | 2851 | `		return PH7_OK;` |
|      - | 2852 | `	}` |
|      - | 2853 | `	/* The device sits PAST what the readers pulled ahead: php writes at the` |
|      - | 2854 | `	 * LOGICAL position (fgets() then fwrite() overwrites what fgets left` |
|      - | 2855 | `	 * unread) — the ftell()/SEEK_CUR rule, applied to the write. */` |
|    416 | 2856 | `	StreamSeekBackForWrite(pDev);` |
|      - | 2857 | `	/* Perform the requested operation */` |
|    416 | 2858 | `	n = (int)PH7_StreamWrite(pDev,(const void *)zString,nLen);` |
|    416 | 2859 | `	if( n <  0 ){` |
|      - | 2860 | `		/* IO error,return FALSE */` |
|     32 | 2861 | `		SockReportWriteFailure(pCtx,pDev,nLen);` |
|     32 | 2862 | `		ph7_result_bool(pCtx,0);` |
|     17 | 2863 | `	}else{` |
|      - | 2864 | `		/* #Bytes written */` |
|    386 | 2865 | `		ph7_result_int(pCtx,n);` |
|      - | 2866 | `	}` |
|    416 | 2867 | `	return PH7_OK;` |
|    211 | 2868 | `}` |
|      - | 2869 | `/*` |
|      - | 2870 | ` * Write flock()'s optional by-reference &$would_block out-param. php writes it on` |
|      - | 2871 | ` * every call that does not throw — including the ones that answer FALSE — so a` |
|      - | 2872 | ` * script can tell contention (1) from a plain failure (0).` |
|      - | 2873 | ` */` |
|     38 | 2874 | `static void FlockStoreWouldBlock(ph7_context *pCtx,int nArg,ph7_value **apArg,int bWouldBlock)` |
|      1 | 2875 | `{` |
|      - | 2876 | `	ph7_value sVal;` |
|     39 | 2877 | `	if( nArg < 3 ){` |
|     25 | 2878 | `		return;` |
|      - | 2879 | `	}` |
|     15 | 2880 | `	PH7_MemObjInitFromInt(pCtx->pVm,&sVal,bWouldBlock ? 1 : 0);` |
|     15 | 2881 | `	PH7_VmStoreArgByRef(pCtx->pVm,apArg[2],&sVal);` |
|     15 | 2882 | `	PH7_MemObjRelease(&sVal);` |
|     20 | 2883 | `}` |
|      - | 2884 | `/*` |
|      - | 2885 | ` * bool flock(resource $handle,int $operation[,int &$would_block])` |
|      - | 2886 | ` *  Portable advisory file locking.` |
|      - | 2887 | ` * Parameters` |
|      - | 2888 | ` *  $handle` |
|      - | 2889 | ` *   The file pointer.` |
|      - | 2890 | ` *  $operation` |
|      - | 2891 | ` *   operation is one of the following:` |
|      - | 2892 | ` *      LOCK_SH to acquire a shared lock (reader).` |
|      - | 2893 | ` *      LOCK_EX to acquire an exclusive lock (writer).` |
|      - | 2894 | ` *      LOCK_UN to release a lock (shared or exclusive).` |
|      - | 2895 | ` *   optionally OR'd with LOCK_NB to fail immediately instead of waiting.` |
|      - | 2896 | ` *  &$would_block` |
|      - | 2897 | ` *   Set to 1 when a LOCK_NB request was refused because another holder has the` |
|      - | 2898 | ` *   file, 0 otherwise. php writes it on every call that does not throw.` |
|      - | 2899 | ` * Return` |
|      - | 2900 | ` *  Returns TRUE on success or FALSE on failure.` |
|      - | 2901 | ` */` |
|     46 | 2902 | `PH7_PRIVATE int PH7_builtin_flock(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 2903 | `{` |
|      - | 2904 | `	const ph7_io_stream *pStream;` |
|      - | 2905 | `	io_private *pDev;` |
|      - | 2906 | `	int nLock;` |
|      - | 2907 | `	int rc;` |
|     47 | 2908 | `	if( nArg < 2 \|\| !ph7_value_is_resource(apArg[0]) ){` |
|      - | 2909 | `		/* Missing/Invalid arguments,return FALSE */` |
|    ! 0 | 2910 | `		ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"Expecting an IO handle");` |
|    ! 0 | 2911 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 2912 | `		return PH7_OK;` |
|      - | 2913 | `	}` |
|      - | 2914 | `	/* Extract our private data */` |
|     47 | 2915 | `	pDev = (io_private *)ph7_value_to_resource(apArg[0]);` |
|      - | 2916 | `	/* Make sure we are dealing with a valid io_private instance */` |
|     47 | 2917 | `	if( IO_PRIVATE_INVALID(pDev) ){` |
|      - | 2918 | `		/*Expecting an IO handle */` |
|    ! 0 | 2919 | `		ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"Expecting an IO handle");` |
|    ! 0 | 2920 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 2921 | `		return PH7_OK;` |
|      - | 2922 | `	}` |
|      - | 2923 | `	/* Requested lock operation. php 8 validates it BEFORE the stream's lock` |
|      - | 2924 | `	 * support is considered: the low two bits select the action (its bison` |
|      - | 2925 | `	 * table is act = operation & 3), 0 is invalid, and every higher bit except` |
|      - | 2926 | `	 * LOCK_NB is ignored (flock($f,99) is LOCK_UN in php). */` |
|     47 | 2927 | `	nLock = ph7_value_to_int(apArg[1]);` |
|     47 | 2928 | `	if( (nLock & 3) == 0 ){` |
|      9 | 2929 | `		return PH7_VmThrowException(pCtx,"ValueError",` |
|      - | 2930 | `			"flock(): Argument #2 ($operation) must be one of LOCK_SH, LOCK_EX, or LOCK_UN");` |
|      - | 2931 | `	}` |
|     39 | 2932 | `	pDev = PH7_StreamUnwrap(pDev);` |
|      - | 2933 | `	/* Point to the target IO stream device */` |
|     39 | 2934 | `	pStream = pDev->pStream;` |
|     39 | 2935 | `	if( pStream == 0  \|\| pStream->xLock == 0){` |
|      - | 2936 | `		/* php returns FALSE silently when the stream does not support locking` |
|      - | 2937 | `		 * (php://memory & co) — no warning. It still writes $would_block. */` |
|      7 | 2938 | `		FlockStoreWouldBlock(pCtx,nArg,apArg,0);` |
|      7 | 2939 | `		ph7_result_bool(pCtx,0);` |
|      7 | 2940 | `		return PH7_OK;` |
|      - | 2941 | `	}` |
|      - | 2942 | `	/*` |
|      - | 2943 | `	 * Translate php's operation (LOCK_SH=1, LOCK_EX=2, LOCK_UN=3, optionally` |
|      - | 2944 | `	 * \|LOCK_NB=4) into the xLock() vtable value space documented in ph7.h.` |
|      - | 2945 | `	 */` |
|      - | 2946 | `	{` |
|     33 | 2947 | `		int iOp = nLock & 3;` |
|     33 | 2948 | `		int bNoBlock = (nLock & 4 /* LOCK_NB */) != 0;` |
|     33 | 2949 | `		if( iOp == 3 /* LOCK_UN */ ){` |
|     13 | 2950 | `			nLock = -1;` |
|     27 | 2951 | `		}else if( iOp == 2 /* LOCK_EX */ ){` |
|     11 | 2952 | `			nLock = bNoBlock ? PH7_IO_LOCK_EX_NB : PH7_IO_LOCK_EX;` |
|      6 | 2953 | `		}else{` |
|     11 | 2954 | `			nLock = bNoBlock ? PH7_IO_LOCK_SH_NB : PH7_IO_LOCK_SH;` |
|      - | 2955 | `		}` |
|      - | 2956 | `	}` |
|      - | 2957 | `	/* Lock operation */` |
|     33 | 2958 | `	rc = pStream->xLock(pDev->pHandle,nLock);` |
|      - | 2959 | `	/* A refused non-blocking request is php's $would_block: FALSE, and the` |
|      - | 2960 | `	 * out-param tells the script it was contention rather than an IO error. */` |
|     33 | 2961 | `	FlockStoreWouldBlock(pCtx,nArg,apArg,rc == SXERR_BUSY);` |
|      - | 2962 | `	/* IO result */` |
|     33 | 2963 | `	ph7_result_bool(pCtx,rc == PH7_OK);` |
|     33 | 2964 | `	return PH7_OK;` |
|     24 | 2965 | `}` |
|      - | 2966 | `/*` |
|      - | 2967 | ` * int fpassthru(resource $handle)` |
|      - | 2968 | ` *  Output all remaining data on a file pointer.` |
|      - | 2969 | ` * Parameters` |
|      - | 2970 | ` *  $handle` |
|      - | 2971 | ` *   The file pointer.` |
|      - | 2972 | ` * Return` |
|      - | 2973 | ` *  Total number of characters read from handle and passed through` |
|      - | 2974 | ` *  to the output on success or FALSE on failure.` |
|      - | 2975 | ` */` |
|      8 | 2976 | `PH7_PRIVATE int PH7_builtin_fpassthru(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 2977 | `{` |
|      - | 2978 | `	const ph7_io_stream *pStream;` |
|      - | 2979 | `	io_private *pDev;` |
|      - | 2980 | `	ph7_int64 n,nRead;` |
|      - | 2981 | `	char zBuf[8192];` |
|      - | 2982 | `	int rc;` |
|      9 | 2983 | `	if( nArg < 1 \|\| !ph7_value_is_resource(apArg[0]) ){` |
|      - | 2984 | `		/* Missing/Invalid arguments,return FALSE */` |
|    ! 0 | 2985 | `		ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"Expecting an IO handle");` |
|    ! 0 | 2986 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 2987 | `		return PH7_OK;` |
|      - | 2988 | `	}` |
|      - | 2989 | `	/* Extract our private data */` |
|      9 | 2990 | `	pDev = (io_private *)ph7_value_to_resource(apArg[0]);` |
|      - | 2991 | `	/* Make sure we are dealing with a valid io_private instance */` |
|      9 | 2992 | `	if( IO_PRIVATE_INVALID(pDev) ){` |
|      - | 2993 | `		/*Expecting an IO handle */` |
|    ! 0 | 2994 | `		ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"Expecting an IO handle");` |
|    ! 0 | 2995 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 2996 | `		return PH7_OK;` |
|      - | 2997 | `	}` |
|      - | 2998 | `	/* Point to the target IO stream device */` |
|      9 | 2999 | `	pStream = pDev->pStream;` |
|      9 | 3000 | `	if( pStream == 0  ){` |
|    ! 0 | 3001 | `		ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,` |
|      - | 3002 | `			"IO routine(%s) not implemented in the underlying stream(%s) device,PH7 is returning FALSE",` |
|    ! 0 | 3003 | `			ph7_function_name(pCtx),pStream ? pStream->zName : "null_stream"` |
|      - | 3004 | `			);` |
|    ! 0 | 3005 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 3006 | `		return PH7_OK;` |
|      - | 3007 | `	}` |
|      - | 3008 | `	/* Perform the requested operation */` |
|      9 | 3009 | `	nRead = 0;` |
|      8 | 3010 | `	for(;;){` |
|     17 | 3011 | `		n = PH7_StreamRead(pDev,zBuf,sizeof(zBuf));` |
|     17 | 3012 | `		if( n < 1 ){` |
|      - | 3013 | `			/* Error or EOF */` |
|      9 | 3014 | `			StreamReportReadFailure(pCtx,pDev);` |
|      9 | 3015 | `			if( n < 0 && nRead == 0 ){` |
|      - | 3016 | `				/* php answers the failing read's own -1 when NOTHING was passed` |
|      - | 3017 | `				 * through; a failure after some bytes reports those bytes. */` |
|      3 | 3018 | `				ph7_result_int64(pCtx,-1);` |
|      3 | 3019 | `				return PH7_OK;` |
|      - | 3020 | `			}` |
|      7 | 3021 | `			break;` |
|      - | 3022 | `		}` |
|      - | 3023 | `		/* Increment the read counter */` |
|      9 | 3024 | `		nRead += n;` |
|      - | 3025 | `		/* Output the bytes THIS read produced. Handing the running total to` |
|      - | 3026 | `		 * ph7_context_output() instead read past the end of zBuf from the second` |
|      - | 3027 | `		 * chunk on (an out-of-bounds read) and wrote the overrun to the output:` |
|      - | 3028 | `		 * a 12000-byte file passed through as 20189 bytes of file-plus-garbage. */` |
|      9 | 3029 | `		rc = ph7_context_output(pCtx,zBuf,(int)n);` |
|      9 | 3030 | `		if( rc == PH7_ABORT ){` |
|      - | 3031 | `			/* Consumer callback request an operation abort */` |
|    ! 0 | 3032 | `			break;` |
|      - | 3033 | `		}` |
|      1 | 3034 | `	}` |
|      - | 3035 | `	/* Total number of bytes readen */` |
|      7 | 3036 | `	ph7_result_int64(pCtx,nRead);` |
|      7 | 3037 | `	return PH7_OK;` |
|      5 | 3038 | `}` |
|      - | 3039 | `/* CSV writer private data */` |
|      - | 3040 | `struct csv_data` |
|      - | 3041 | `{` |
|      - | 3042 | `	int delimiter;     /* Delimiter. Default ',' */` |
|      - | 3043 | `	int enclosure;     /* Enclosure. Default '"' */` |
|      - | 3044 | `	int escape;        /* Escape, or PH7_CSV_NO_ESCAPE when "" disabled it */` |
|      - | 3045 | `	SyBlob *pLine;     /* The line being built */` |
|      - | 3046 | `	sxu32 nCount;      /* Fields still to write after this one */` |
|      - | 3047 | `};` |
|      - | 3048 | `/*` |
|      - | 3049 | ` * The following callback is used by fputcsv() to walk the $fields array and` |
|      - | 3050 | ` * append each entry to the line under construction. It is a port of php's own` |
|      - | 3051 | ` * php_fputcsv (ext/standard/file.c), and the parts a re-derivation gets wrong` |
|      - | 3052 | ` * are all here:` |
|      - | 3053 | ` *  - WHICH fields are enclosed. php quotes a field containing the delimiter,` |
|      - | 3054 | ` *    the enclosure, the escape (when one is enabled) or any of \n, \r, \t and` |
|      - | 3055 | ` *    SPACE. PH7 tested the first two only, so a field with an embedded newline` |
|      - | 3056 | ` *    was written raw and became two CSV ROWS on the way back in.` |
|      - | 3057 | ` *  - HOW an embedded enclosure is written: doubled, unless the escape character` |
|      - | 3058 | ` *    came immediately before it (then the pair is passed through as-is and the` |
|      - | 3059 | ` *    escape does NOT arm again for the byte after).` |
|      - | 3060 | ` *  - that an EMPTY field is still a field. PH7 returned early for a zero-length` |
|      - | 3061 | `` *    value and skipped its delimiter with it, so `['', 'a']` wrote "a" -- one`` |
|      - | 3062 | ` *    column where the caller wrote two, silently shifting every later column.` |
|      - | 3063 | ` * The delimiter goes BETWEEN fields, so it is written from the remaining count` |
|      - | 3064 | ` * rather than from a "not the first" flag: php appends it after every field but` |
|      - | 3065 | ` * the last, and an empty first field must still be followed by one.` |
|      - | 3066 | ` */` |
|    130 | 3067 | `static int csv_write_callback(ph7_value *pKey,ph7_value *pValue,void *pUserData)` |
|      1 | 3068 | `{` |
|    131 | 3069 | `	struct csv_data *pData = (struct csv_data *)pUserData;` |
|      - | 3070 | `	const char *zData;` |
|      - | 3071 | `	int nLen,i;` |
|    131 | 3072 | `	int bEnclose = 0;` |
|     65 | 3073 | `	SXUNUSED(pKey); /* cc warning */` |
|    131 | 3074 | `	zData = ph7_value_to_string(pValue,&nLen);` |
|    249 | 3075 | `	for( i = 0 ; i < nLen ; ++i ){` |
|    167 | 3076 | `		int c = (unsigned char)zData[i];` |
|    166 | 3077 | `		if( c == pData->delimiter \|\| c == pData->enclosure` |
|    147 | 3078 | `		 \|\| (pData->escape != PH7_CSV_NO_ESCAPE && c == pData->escape)` |
|    137 | 3079 | `		 \|\| c == '\n' \|\| c == '\r' \|\| c == '\t' \|\| c == ' ' ){` |
|     49 | 3080 | `			bEnclose = 1;` |
|     49 | 3081 | `			break;` |
|      - | 3082 | `		}` |
|     60 | 3083 | `	}` |
|    131 | 3084 | `	if( bEnclose ){` |
|     49 | 3085 | `		char cEnc = (char)pData->enclosure;` |
|     49 | 3086 | `		int bEscaped = 0;` |
|     49 | 3087 | `		SyBlobAppend(pData->pLine,(const void *)&cEnc,sizeof(char));` |
|    197 | 3088 | `		for( i = 0 ; i < nLen ; ++i ){` |
|    149 | 3089 | `			char c = zData[i];` |
|    149 | 3090 | `			if( pData->escape != PH7_CSV_NO_ESCAPE && (unsigned char)c == pData->escape ){` |
|      9 | 3091 | `				bEscaped = 1;` |
|    145 | 3092 | `			}else if( !bEscaped && (unsigned char)c == pData->enclosure ){` |
|     21 | 3093 | `				SyBlobAppend(pData->pLine,(const void *)&cEnc,sizeof(char));` |
|     11 | 3094 | `			}else{` |
|    121 | 3095 | `				bEscaped = 0;` |
|      - | 3096 | `			}` |
|    149 | 3097 | `			SyBlobAppend(pData->pLine,(const void *)&c,sizeof(char));` |
|     75 | 3098 | `		}` |
|     49 | 3099 | `		SyBlobAppend(pData->pLine,(const void *)&cEnc,sizeof(char));` |
|    107 | 3100 | `	}else if( nLen > 0 ){` |
|     63 | 3101 | `		SyBlobAppend(pData->pLine,(const void *)zData,(sxu32)nLen);` |
|     31 | 3102 | `	}` |
|    131 | 3103 | `	if( pData->nCount > 0 ){` |
|    131 | 3104 | `		pData->nCount--;` |
|     65 | 3105 | `	}` |
|    131 | 3106 | `	if( pData->nCount > 0 ){` |
|     49 | 3107 | `		char cDel = (char)pData->delimiter;` |
|     49 | 3108 | `		SyBlobAppend(pData->pLine,(const void *)&cDel,sizeof(char));` |
|     24 | 3109 | `	}` |
|    131 | 3110 | `	return PH7_OK;` |
|      1 | 3111 | `}` |
|      - | 3112 | `/*` |
|      - | 3113 | ` * int\|false fputcsv(resource $stream, array $fields, string $separator = ',',` |
|      - | 3114 | ` *                   string $enclosure = '"', string $escape = '\\',` |
|      - | 3115 | ` *                   string $eol = "\n")` |
|      - | 3116 | ` *  Format line as CSV and write to file pointer.` |
|      - | 3117 | ` * Parameters` |
|      - | 3118 | ` *  $stream` |
|      - | 3119 | ` *   Open file handle.` |
|      - | 3120 | ` *  $fields` |
|      - | 3121 | ` *   An array of values.` |
|      - | 3122 | ` *  $separator` |
|      - | 3123 | ` *   The optional separator parameter sets the field delimiter (one character only).` |
|      - | 3124 | ` *  $enclosure` |
|      - | 3125 | ` *   The optional enclosure parameter sets the field enclosure (one character only).` |
|      - | 3126 | ` *  $escape` |
|      - | 3127 | ` *   The escape character (one character), or "" to disable escaping entirely.` |
|      - | 3128 | ` *  $eol` |
|      - | 3129 | ` *   php 8.1's line ending. It is "\n" on EVERY platform -- php does not follow` |
|      - | 3130 | ` *   the host's convention here, and PHL used to write CRLF on Windows, so the` |
|      - | 3131 | ` *   same program produced a different FILE depending on where it ran.` |
|      - | 3132 | ` * Return` |
|      - | 3133 | ` *  The number of bytes written, or FALSE when the write fails. The count was` |
|      - | 3134 | ` *  missing entirely (the call answered NULL), so the documented` |
|      - | 3135 | `` *  `if (fputcsv(...) === false)` check never fired and a caller totalling the`` |
|      - | 3136 | ` *  bytes it wrote added nothing.` |
|      - | 3137 | ` */` |
|     96 | 3138 | `PH7_PRIVATE int PH7_builtin_fputcsv(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 3139 | `{` |
|      - | 3140 | `	const ph7_io_stream *pStream;` |
|      - | 3141 | `	struct csv_data sCsv;` |
|      - | 3142 | `	io_private *pDev;` |
|      - | 3143 | `	SyBlob sLine;` |
|     97 | 3144 | `	const char *zEol = "\n";` |
|     97 | 3145 | `	int nEol = 1;` |
|      - | 3146 | `	ph7_int64 nWr;` |
|     97 | 3147 | `	if( nArg < 2 \|\| !ph7_value_is_resource(apArg[0]) \|\| !ph7_value_is_array(apArg[1]) ){` |
|      - | 3148 | `		/* Missing/Invalid arguments,return FALSE */` |
|    ! 0 | 3149 | `		ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"Missing/Invalid arguments");` |
|    ! 0 | 3150 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 3151 | `		return PH7_OK;` |
|      - | 3152 | `	}` |
|      - | 3153 | `	/* Extract our private data */` |
|     97 | 3154 | `	pDev = (io_private *)ph7_value_to_resource(apArg[0]);` |
|      - | 3155 | `	/* Make sure we are dealing with a valid io_private instance */` |
|     97 | 3156 | `	if( IO_PRIVATE_INVALID(pDev) ){` |
|      - | 3157 | `		/*Expecting an IO handle */` |
|    ! 0 | 3158 | `		ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"Expecting an IO handle");` |
|    ! 0 | 3159 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 3160 | `		return PH7_OK;` |
|      - | 3161 | `	}` |
|      - | 3162 | `	/* Point to the target IO stream device */` |
|     97 | 3163 | `	pStream = pDev->pStream;` |
|     97 | 3164 | `	if( pStream == 0  \|\| pStream->xWrite == 0){` |
|    ! 0 | 3165 | `		ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,` |
|      - | 3166 | `			"IO routine(%s) not implemented in the underlying stream(%s) device,PH7 is returning FALSE",` |
|    ! 0 | 3167 | `			ph7_function_name(pCtx),pStream ? pStream->zName : "null_stream"` |
|      - | 3168 | `			);` |
|    ! 0 | 3169 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 3170 | `		return PH7_OK;` |
|      - | 3171 | `	}` |
|      - | 3172 | `	/* Set default csv separator */` |
|     97 | 3173 | `	sCsv.delimiter = ',';` |
|     97 | 3174 | `	sCsv.enclosure = '"';` |
|     97 | 3175 | `	sCsv.escape = '\\';` |
|     97 | 3176 | `	if( nArg > 2 ){` |
|     97 | 3177 | `		sxi32 rc = PH7_CsvCharArg(pCtx,apArg[2],3,"separator",0,&sCsv.delimiter);` |
|     97 | 3178 | `		if( rc != PH7_OK ){` |
|      5 | 3179 | `			return rc;` |
|      - | 3180 | `		}` |
|     93 | 3181 | `		if( nArg > 3 ){` |
|     93 | 3182 | `			rc = PH7_CsvCharArg(pCtx,apArg[3],4,"enclosure",0,&sCsv.enclosure);` |
|     93 | 3183 | `			if( rc != PH7_OK ){` |
|      5 | 3184 | `				return rc;` |
|      - | 3185 | `			}` |
|     89 | 3186 | `			if( nArg > 4 ){` |
|     89 | 3187 | `				rc = PH7_CsvCharArg(pCtx,apArg[4],5,"escape",1,&sCsv.escape);` |
|     89 | 3188 | `				if( rc != PH7_OK ){` |
|      5 | 3189 | `					return rc;` |
|      - | 3190 | `				}` |
|     85 | 3191 | `				if( nArg > 5 ){` |
|      - | 3192 | `					/* $eol takes ANY string, the empty one included -- it is not` |
|      - | 3193 | `					 * a single-character argument like the three above. */` |
|     75 | 3194 | `					zEol = ph7_value_to_string(apArg[5],&nEol);` |
|     37 | 3195 | `				}` |
|     42 | 3196 | `			}` |
|     42 | 3197 | `		}` |
|     42 | 3198 | `	}` |
|      - | 3199 | `	/* php builds the whole line first and writes it ONCE, which is what makes the` |
|      - | 3200 | `	 * byte count meaningful and keeps a partly-written row off the stream. */` |
|     85 | 3201 | `	SyBlobInit(&sLine,&pCtx->pVm->sAllocator);` |
|     85 | 3202 | `	sCsv.pLine = &sLine;` |
|     85 | 3203 | `	sCsv.nCount = (sxu32)ph7_array_count(apArg[1]);` |
|     85 | 3204 | `	ph7_array_walk(apArg[1],csv_write_callback,&sCsv);` |
|     85 | 3205 | `	if( nEol > 0 ){` |
|     83 | 3206 | `		SyBlobAppend(&sLine,(const void *)zEol,(sxu32)nEol);` |
|     41 | 3207 | `	}` |
|      - | 3208 | `	/* Write at the LOGICAL position, not the device one -- the same rule` |
|      - | 3209 | `	 * PH7_builtin_fwrite applies after a buffered read (fgets() then` |
|      - | 3210 | `	 * fputcsv() overwrites what fgets left unread). */` |
|     85 | 3211 | `	StreamSeekBackForWrite(pDev);` |
|    127 | 3212 | `	nWr = PH7_StreamWrite(pDev,(const void *)SyBlobData(&sLine),` |
|     84 | 3213 | `		(ph7_int64)SyBlobLength(&sLine));` |
|     85 | 3214 | `	if( nWr < 0 ){` |
|      3 | 3215 | `		SockReportWriteFailure(pCtx,pDev,(int)SyBlobLength(&sLine));` |
|      1 | 3216 | `	}` |
|     85 | 3217 | `	SyBlobRelease(&sLine);` |
|     85 | 3218 | `	if( nWr < 0 ){` |
|      3 | 3219 | `		ph7_result_bool(pCtx,0);` |
|      2 | 3220 | `	}else{` |
|     83 | 3221 | `		ph7_result_int64(pCtx,nWr);` |
|      - | 3222 | `	}` |
|     85 | 3223 | `	return PH7_OK;` |
|     49 | 3224 | `}` |
|      - | 3225 | `/*` |
|      - | 3226 | ` * fprintf,vfprintf private data.` |
|      - | 3227 | ` * An instance of the following structure is passed to the formatted` |
|      - | 3228 | ` * input consumer callback defined below.` |
|      - | 3229 | ` */` |
|      - | 3230 | `typedef struct fprintf_data fprintf_data;` |
|      - | 3231 | `struct fprintf_data` |
|      - | 3232 | `{` |
|      - | 3233 | `	io_private *pIO;        /* IO stream */` |
|      - | 3234 | `	ph7_int64 nCount;       /* Total bytes FORMATTED (php's answer, not the bytes` |
|      - | 3235 | `	                         * the device took: php builds the whole string, writes` |
|      - | 3236 | `	                         * it once and returns its length whatever the write` |
|      - | 3237 | `	                         * did) */` |
|      - | 3238 | `	int bIoErr;             /* the device refused, so stop feeding it -- but this` |
|      - | 3239 | `	                         * is an IO failure and not a mid-format THROW, and the` |
|      - | 3240 | `	                         * caller must not confuse the two */` |
|      - | 3241 | `};` |
|      - | 3242 | `/*` |
|      - | 3243 | ` * Callback [i.e: Formatted input consumer] for the fprintf function.` |
|      - | 3244 | ` */` |
|     40 | 3245 | `static int fprintfConsumer(ph7_context *pCtx,const char *zInput,int nLen,void *pUserData)` |
|      2 | 3246 | `{` |
|     42 | 3247 | `	fprintf_data *pFdata = (fprintf_data *)pUserData;` |
|      - | 3248 | `	ph7_int64 n;` |
|      - | 3249 | `	/* Write the formatted data */` |
|     42 | 3250 | `	n = PH7_StreamWrite(pFdata->pIO,(const void *)zInput,nLen);` |
|     42 | 3251 | `	pFdata->nCount += nLen;` |
|     42 | 3252 | `	if( n < 0 ){` |
|      3 | 3253 | `		SockReportWriteFailure(pCtx,pFdata->pIO,nLen);` |
|      - | 3254 | `		/* Nothing more can reach the device; stop, and let the caller answer.` |
|      - | 3255 | `		 * Propagating this as a THROW status aborted the whole script -- a` |
|      - | 3256 | `		 * failed fprintf() ended the program where php returns a number. */` |
|      3 | 3257 | `		pFdata->bIoErr = 1;` |
|      3 | 3258 | `		return SXERR_ABORT;` |
|      - | 3259 | `	}` |
|     40 | 3260 | `	return PH7_OK;` |
|     22 | 3261 | `}` |
|      - | 3262 | `/*` |
|      - | 3263 | ` * int fprintf(resource $handle,string $format[,mixed $args [, mixed $... ]])` |
|      - | 3264 | ` *  Write a formatted string to a stream.` |
|      - | 3265 | ` * Parameters` |
|      - | 3266 | ` *  $handle` |
|      - | 3267 | ` *   The file pointer.` |
|      - | 3268 | ` *  $format` |
|      - | 3269 | ` *   String format (see sprintf()).` |
|      - | 3270 | ` * Return` |
|      - | 3271 | ` *  The length of the written string.` |
|      - | 3272 | ` */` |
|     22 | 3273 | `PH7_PRIVATE int PH7_builtin_fprintf(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      2 | 3274 | `{` |
|      - | 3275 | `	fprintf_data sFdata;` |
|      - | 3276 | `	const char *zFormat;` |
|      - | 3277 | `	io_private *pDev;` |
|      - | 3278 | `	int nLen;` |
|     24 | 3279 | `	if( nArg < 2 ){` |
|    ! 0 | 3280 | `		ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"Invalid arguments");` |
|    ! 0 | 3281 | `		ph7_result_int(pCtx,0);` |
|    ! 0 | 3282 | `		return PH7_OK;` |
|      - | 3283 | `	}` |
|      - | 3284 | `	{` |
|      - | 3285 | `		/* php: a non-resource $stream is a TypeError (#1), not a warn-and-return-0. */` |
|     24 | 3286 | `		sxi32 rcs = PH7_CheckStreamArg(pCtx,apArg[0],1,"stream");` |
|     24 | 3287 | `		if( rcs != PH7_OK ){` |
|    ! 0 | 3288 | `			return rcs;` |
|      - | 3289 | `		}` |
|      - | 3290 | `	}` |
|      - | 3291 | `	/* Extract our private data */` |
|     24 | 3292 | `	pDev = (io_private *)ph7_value_to_resource(apArg[0]);` |
|      - | 3293 | `	/* Make sure we are dealing with a valid io_private instance */` |
|     24 | 3294 | `	if( IO_PRIVATE_INVALID(pDev) ){` |
|      - | 3295 | `		/*Expecting an IO handle */` |
|    ! 0 | 3296 | `		ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"Expecting an IO handle");` |
|    ! 0 | 3297 | `		ph7_result_int(pCtx,0);` |
|    ! 0 | 3298 | `		return PH7_OK;` |
|      - | 3299 | `	}` |
|      - | 3300 | `	/* Point to the target IO stream device */` |
|     24 | 3301 | `	if( pDev->pStream == 0  \|\| pDev->pStream->xWrite == 0 ){` |
|    ! 0 | 3302 | `		ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,` |
|      - | 3303 | `			"IO routine(%s) not implemented in the underlying stream(%s) device",` |
|    ! 0 | 3304 | `			ph7_function_name(pCtx),pDev->pStream ? pDev->pStream->zName : "null_stream"` |
|      - | 3305 | `			);` |
|    ! 0 | 3306 | `		ph7_result_int(pCtx,0);` |
|    ! 0 | 3307 | `		return PH7_OK;` |
|      - | 3308 | `	}` |
|      - | 3309 | `	/* PHP 8: a non-string-coercible $format (array/object/resource) is a TypeError (#2). */` |
|      - | 3310 | `	{` |
|     24 | 3311 | `		sxi32 rcf = PH7_FormatCheckFormatArg(pCtx,apArg[1],2);` |
|     24 | 3312 | `		if( rcf != PH7_OK ){` |
|    ! 0 | 3313 | `			return rcf;` |
|      - | 3314 | `		}` |
|      - | 3315 | `	}` |
|      - | 3316 | `	/* Extract the string format (scalars/null coerce). */` |
|     24 | 3317 | `	zFormat = ph7_value_to_string(apArg[1],&nLen);` |
|     24 | 3318 | `	if( nLen < 1 ){` |
|      - | 3319 | `		/* Empty string,return zero */` |
|    ! 0 | 3320 | `		ph7_result_int(pCtx,0);` |
|    ! 0 | 3321 | `		return PH7_OK;` |
|      - | 3322 | `	}` |
|      - | 3323 | `	{` |
|      - | 3324 | `		/* PHP 8: too few value arguments is a catchable ArgumentCountError before output,` |
|      - | 3325 | `		 * and php runs this check BEFORE validating the specifiers. fprintf's values start` |
|      - | 3326 | `		 * at apArg[2] (apArg[0]=stream, apArg[1]=format). */` |
|     24 | 3327 | `		sxi32 rcv = PH7_FormatCheckArgCount(pCtx,zFormat,nLen,nArg - 2,2,FALSE);` |
|     24 | 3328 | `		if( rcv != PH7_OK ){` |
|      3 | 3329 | `			return rcv;` |
|      - | 3330 | `		}` |
|      - | 3331 | `		/* PHP 8: an unknown or missing format specifier throws a catchable ValueError` |
|      - | 3332 | `		 * before any output; propagate the throw status verbatim. */` |
|     22 | 3333 | `		rcv = PH7_FormatValidate(pCtx,zFormat,nLen);` |
|     22 | 3334 | `		if( rcv != PH7_OK ){` |
|      5 | 3335 | `			return rcv;` |
|      - | 3336 | `		}` |
|      - | 3337 | `	}` |
|      - | 3338 | `	/* Prepare our private data */` |
|     18 | 3339 | `	sFdata.nCount = 0;` |
|     18 | 3340 | `	sFdata.pIO = pDev;` |
|     18 | 3341 | `	sFdata.bIoErr = 0;` |
|      - | 3342 | `	/* Format the string */` |
|      - | 3343 | `	{` |
|     18 | 3344 | `	sxi32 rcv = PH7_InputFormat(fprintfConsumer,pCtx,zFormat,nLen,nArg - 1,&apArg[1],(void *)&sFdata,FALSE);` |
|      - | 3345 | `	/* Return total number of bytes written */` |
|     18 | 3346 | `	ph7_result_int64(pCtx,sFdata.nCount);` |
|      - | 3347 | `	/* A %s argument that could not be coerced raised php's Error mid-format; the` |
|      - | 3348 | `	 * bytes still went to the stream, as php's do, so report the throw last. A` |
|      - | 3349 | `	 * refused DEVICE is not that: php answers the length either way. */` |
|     18 | 3350 | `	if( rcv != SXRET_OK && !sFdata.bIoErr ){` |
|      3 | 3351 | `		pCtx->nThrowRc = rcv;` |
|      3 | 3352 | `		return rcv;` |
|      - | 3353 | `	}` |
|      - | 3354 | `	}` |
|     15 | 3355 | `	return PH7_OK;` |
|     13 | 3356 | `}` |
|      - | 3357 | `/*` |
|      - | 3358 | ` * int vfprintf(resource $handle,string $format,array $args)` |
|      - | 3359 | ` *  Write a formatted string to a stream.` |
|      - | 3360 | ` * Parameters` |
|      - | 3361 | ` *  $handle` |
|      - | 3362 | ` *   The file pointer.` |
|      - | 3363 | ` *  $format` |
|      - | 3364 | ` *   String format (see sprintf()).` |
|      - | 3365 | ` * $args` |
|      - | 3366 | ` *   User arguments.` |
|      - | 3367 | ` * Return` |
|      - | 3368 | ` *  The length of the written string.` |
|      - | 3369 | ` */` |
|      8 | 3370 | `PH7_PRIVATE int PH7_builtin_vfprintf(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      2 | 3371 | `{` |
|      - | 3372 | `	fprintf_data sFdata;` |
|      - | 3373 | `	const char *zFormat;` |
|      - | 3374 | `	ph7_hashmap *pMap;` |
|      - | 3375 | `	io_private *pDev;` |
|      - | 3376 | `	SySet sArg;` |
|      - | 3377 | `	int n,nLen;` |
|     10 | 3378 | `	if( nArg < 3 ){` |
|    ! 0 | 3379 | `		ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"Invalid arguments");` |
|    ! 0 | 3380 | `		ph7_result_int(pCtx,0);` |
|    ! 0 | 3381 | `		return PH7_OK;` |
|      - | 3382 | `	}` |
|      - | 3383 | `	{` |
|      - | 3384 | `		/* php: a non-resource $stream is a TypeError (#1), not a warn-and-return-0. */` |
|     10 | 3385 | `		sxi32 rcs = PH7_CheckStreamArg(pCtx,apArg[0],1,"stream");` |
|     10 | 3386 | `		if( rcs != PH7_OK ){` |
|      3 | 3387 | `			return rcs;` |
|      - | 3388 | `		}` |
|      - | 3389 | `	}` |
|      - | 3390 | `	/* PHP 8 checks argument types left-to-right: $format (#2) then $values (#3). */` |
|      - | 3391 | `	{` |
|      8 | 3392 | `		sxi32 rcf = PH7_FormatCheckFormatArg(pCtx,apArg[1],2);` |
|      8 | 3393 | `		if( rcf != PH7_OK ){` |
|    ! 0 | 3394 | `			return rcf;` |
|      - | 3395 | `		}` |
|      - | 3396 | `	}` |
|      8 | 3397 | `	if( !ph7_value_is_array(apArg[2]) ){` |
|      - | 3398 | `		/* PHP 8: a non-array $values is a catchable TypeError. */` |
|      - | 3399 | `		char zBuf[64];` |
|    ! 0 | 3400 | `		return PH7_VmThrowException(pCtx,"TypeError",` |
|      - | 3401 | `			"vfprintf(): Argument #3 ($values) must be of type array, %s given",` |
|    ! 0 | 3402 | `			VmValueGivenName(apArg[2],zBuf,sizeof(zBuf)));` |
|      - | 3403 | `	}` |
|      - | 3404 | `	/* Extract our private data */` |
|      8 | 3405 | `	pDev = (io_private *)ph7_value_to_resource(apArg[0]);` |
|      - | 3406 | `	/* Make sure we are dealing with a valid io_private instance */` |
|      8 | 3407 | `	if( IO_PRIVATE_INVALID(pDev) ){` |
|      - | 3408 | `		/*Expecting an IO handle */` |
|    ! 0 | 3409 | `		ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"Expecting an IO handle");` |
|    ! 0 | 3410 | `		ph7_result_int(pCtx,0);` |
|    ! 0 | 3411 | `		return PH7_OK;` |
|      - | 3412 | `	}` |
|      - | 3413 | `	/* Point to the target IO stream device */` |
|      8 | 3414 | `	if( pDev->pStream == 0  \|\| pDev->pStream->xWrite == 0 ){` |
|    ! 0 | 3415 | `		ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,` |
|      - | 3416 | `			"IO routine(%s) not implemented in the underlying stream(%s) device",` |
|    ! 0 | 3417 | `			ph7_function_name(pCtx),pDev->pStream ? pDev->pStream->zName : "null_stream"` |
|      - | 3418 | `			);` |
|    ! 0 | 3419 | `		ph7_result_int(pCtx,0);` |
|    ! 0 | 3420 | `		return PH7_OK;` |
|      - | 3421 | `	}` |
|      - | 3422 | `	/* Extract the string format */` |
|      8 | 3423 | `	zFormat = ph7_value_to_string(apArg[1],&nLen);` |
|      8 | 3424 | `	if( nLen < 1 ){` |
|      - | 3425 | `		/* Empty string,return zero */` |
|    ! 0 | 3426 | `		ph7_result_int(pCtx,0);` |
|    ! 0 | 3427 | `		return PH7_OK;` |
|      - | 3428 | `	}` |
|      - | 3429 | `	/* Point to hashmap */` |
|      8 | 3430 | `	pMap = (ph7_hashmap *)apArg[2]->x.pOther;` |
|      - | 3431 | `	/* PHP 8: too few items in the $values array is a catchable ValueError before output.` |
|      - | 3432 | `	 * php runs this BEFORE validating the specifiers. */` |
|      - | 3433 | `	{` |
|      8 | 3434 | `		sxi32 rcc = PH7_FormatCheckArgCount(pCtx,zFormat,nLen,(int)pMap->nEntry,1,TRUE);` |
|      8 | 3435 | `		if( rcc != PH7_OK ){` |
|      3 | 3436 | `			return rcc;` |
|      - | 3437 | `		}` |
|      - | 3438 | `	}` |
|      - | 3439 | `	/* PHP 8: an unknown or missing format specifier throws a catchable ValueError before` |
|      - | 3440 | `	 * any output; propagate the throw status verbatim. */` |
|      - | 3441 | `	{` |
|      6 | 3442 | `		sxi32 rcv = PH7_FormatValidate(pCtx,zFormat,nLen);` |
|      6 | 3443 | `		if( rcv != PH7_OK ){` |
|    ! 0 | 3444 | `			return rcv;` |
|      - | 3445 | `		}` |
|      - | 3446 | `	}` |
|      - | 3447 | `	/* Extract arguments from the hashmap */` |
|      6 | 3448 | `	n = PH7_HashmapValuesToSet(pMap,&sArg);` |
|      - | 3449 | `	/* Prepare our private data */` |
|      6 | 3450 | `	sFdata.nCount = 0;` |
|      6 | 3451 | `	sFdata.pIO = pDev;` |
|      6 | 3452 | `	sFdata.bIoErr = 0;` |
|      - | 3453 | `	/* Format the string */` |
|      - | 3454 | `	{` |
|      6 | 3455 | `	sxi32 rcv = PH7_InputFormat(fprintfConsumer,pCtx,zFormat,nLen,n,(ph7_value **)SySetBasePtr(&sArg),(void *)&sFdata,TRUE);` |
|      - | 3456 | `	/* Return total number of bytes written*/` |
|      6 | 3457 | `	ph7_result_int64(pCtx,sFdata.nCount);` |
|      6 | 3458 | `	SySetRelease(&sArg);` |
|      - | 3459 | `	/* A %s argument that could not be coerced raised php's Error mid-format; the` |
|      - | 3460 | `	 * bytes still went to the stream, as php's do, so report the throw last. A` |
|      - | 3461 | `	 * refused DEVICE is not that: php answers the length either way. */` |
|      6 | 3462 | `	if( rcv != SXRET_OK && !sFdata.bIoErr ){` |
|      3 | 3463 | `		pCtx->nThrowRc = rcv;` |
|      3 | 3464 | `		return rcv;` |
|      - | 3465 | `	}` |
|      - | 3466 | `	}` |
|      3 | 3467 | `	return PH7_OK;` |
|      6 | 3468 | `}` |
|      - | 3469 | `/*` |
|      - | 3470 | ` * Convert open modes (string passed to the fopen() function) [i.e: 'r','w+','a',...] into PH7 flags.` |
|      - | 3471 | ` * According to the PHP reference manual:` |
|      - | 3472 | ` *  The mode parameter specifies the type of access you require to the stream. It may be any of the following` |
|      - | 3473 | ` *   'r' 	Open for reading only; place the file pointer at the beginning of the file.` |
|      - | 3474 | ` *   'r+' 	Open for reading and writing; place the file pointer at the beginning of the file.` |
|      - | 3475 | ` *   'w' 	Open for writing only; place the file pointer at the beginning of the file and truncate the file` |
|      - | 3476 | ` *          to zero length. If the file does not exist, attempt to create it.` |
|      - | 3477 | ` *   'w+' 	Open for reading and writing; place the file pointer at the beginning of the file and truncate` |
|      - | 3478 | ` *              the file to zero length. If the file does not exist, attempt to create it.` |
|      - | 3479 | ` *   'a' 	Open for writing only; place the file pointer at the end of the file. If the file does not` |
|      - | 3480 | ` *         exist, attempt to create it.` |
|      - | 3481 | ` *   'a+' 	Open for reading and writing; place the file pointer at the end of the file. If the file does` |
|      - | 3482 | ` *          not exist, attempt to create it.` |
|      - | 3483 | ` *   'x' 	Create and open for writing only; place the file pointer at the beginning of the file. If the file` |
|      - | 3484 | ` *         already exists,` |
|      - | 3485 | ` *         the fopen() call will fail by returning FALSE and generating an error of level E_WARNING. If the file` |
|      - | 3486 | ` *         does not exist attempt to create it. This is equivalent to specifying O_EXCL\|O_CREAT flags for` |
|      - | 3487 | ` *         the underlying open(2) system call.` |
|      - | 3488 | ` *   'x+' 	Create and open for reading and writing; otherwise it has the same behavior as 'x'.` |
|      - | 3489 | ` *   'c' 	Open the file for writing only. If the file does not exist, it is created. If it exists, it is neither truncated` |
|      - | 3490 | ` *          (as opposed to 'w'), nor the call to this function fails (as is the case with 'x'). The file pointer` |
|      - | 3491 | ` *          is positioned on the beginning of the file.` |
|      - | 3492 | ` *          This may be useful if it's desired to get an advisory lock (see flock()) before attempting to modify the file` |
|      - | 3493 | ` *          as using 'w' could truncate the file before the lock was obtained (if truncation is desired, ftruncate() can` |
|      - | 3494 | ` *          be used after the lock is requested).` |
|      - | 3495 | ` *   'c+' 	Open the file for reading and writing; otherwise it has the same behavior as 'c'.` |
|      - | 3496 | ` */` |
|      - | 3497 | `/*` |
|      - | 3498 | ` * php's php_stream_parse_fopen_modes, which is narrower than the list above` |
|      - | 3499 | `` * reads: only the FIRST character decides, it must be one of `r w a x c` in`` |
|      - | 3500 | `` * LOWER case, and everything after it is SCANNED -- `+` anywhere makes the`` |
|      - | 3501 | `` * open read-write, `b`/`t` anywhere pick the translation mode, and any other`` |
|      - | 3502 | ` * byte is ignored. Anything else is refused OUTRIGHT, which is what the -1` |
|      - | 3503 | ` * answer is for; the empty mode is one of them.` |
|      - | 3504 | ` *` |
|      - | 3505 | ` * The chunk read only the first TWO characters and had its own idea of both` |
|      - | 3506 | `` * halves, so six ordinary spellings opened the wrong way in silence: `rb+`,`` |
|      - | 3507 | `` * `ab+` and `cb+` -- the `+` is not in position two -- were opened read-only`` |
|      - | 3508 | ``  * or write-only, `rw` was READ-WRITE where php gives read-only, `wr` `` |
|      - | 3509 | ` * likewise, and an unknown or upper-case mode was accepted with a PH7-ism` |
|      - | 3510 | ` * notice and a read-only open where php refuses the call.` |
|      - | 3511 | ` */` |
|   1562 | 3512 | `static int StrModeToFlags(const char *zMode,int nLen,int *piFlags)` |
|      5 | 3513 | `{` |
|      - | 3514 | `	int iFlag,i;` |
|   1567 | 3515 | `	int bPlus = 0,bBin = 0,bText = 0;` |
|   1567 | 3516 | `	if( nLen < 1 ){` |
|      3 | 3517 | `		return -1;` |
|      - | 3518 | `	}` |
|   1565 | 3519 | `	switch( zMode[0] ){` |
|    320 | 3520 | `		case 'r':` |
|      - | 3521 | `			/* Read-only access */` |
|    645 | 3522 | `			iFlag = PH7_IO_OPEN_RDONLY;` |
|    645 | 3523 | `			break;` |
|    153 | 3524 | `		case 'w':` |
|      - | 3525 | `			/* Overwrite mode; create the file if it is not there */` |
|    309 | 3526 | `			iFlag = PH7_IO_OPEN_WRONLY\|PH7_IO_OPEN_TRUNC\|PH7_IO_OPEN_CREATE;` |
|    309 | 3527 | `			break;` |
|     10 | 3528 | `		case 'a':` |
|      - | 3529 | `			/* Append mode; create the file if it is not there */` |
|     21 | 3530 | `			iFlag = PH7_IO_OPEN_WRONLY\|PH7_IO_OPEN_APPEND\|PH7_IO_OPEN_CREATE;` |
|     21 | 3531 | `			break;` |
|    276 | 3532 | `		case 'x':` |
|      - | 3533 | `			/* Exclusive create: fails when the file already exists. EXCL is left` |
|      - | 3534 | `			 * to imply the creation on its own -- the device decoders test` |
|      - | 3535 | `			 * CREATE first, so setting both would drop the O_EXCL. */` |
|    557 | 3536 | `			iFlag = PH7_IO_OPEN_WRONLY\|PH7_IO_OPEN_EXCL;` |
|    557 | 3537 | `			break;` |
|      3 | 3538 | `		case 'c':` |
|      - | 3539 | `			/* Create if absent, and neither truncate nor fail if present */` |
|      7 | 3540 | `			iFlag = PH7_IO_OPEN_WRONLY\|PH7_IO_OPEN_CREATE;` |
|      7 | 3541 | `			break;` |
|     18 | 3542 | `		default:` |
|     37 | 3543 | `			return -1;` |
|      - | 3544 | `	}` |
|   1987 | 3545 | `	for( i = 1 ; i < nLen ; ++i ){` |
|    462 | 3546 | `		if( zMode[i] == '+' ){` |
|    378 | 3547 | `			bPlus = 1;` |
|    272 | 3548 | `		}else if( zMode[i] == 'b' ){` |
|     69 | 3549 | `			bBin = 1;` |
|     51 | 3550 | `		}else if( zMode[i] == 't' ){` |
|      7 | 3551 | `			bText = 1;` |
|      3 | 3552 | `		}` |
|    233 | 3553 | `	}` |
|   1529 | 3554 | `	if( bPlus ){` |
|    376 | 3555 | `		iFlag &= ~(PH7_IO_OPEN_RDONLY\|PH7_IO_OPEN_WRONLY);` |
|    376 | 3556 | `		iFlag \|= PH7_IO_OPEN_RDWR;` |
|    186 | 3557 | `	}` |
|      - | 3558 | ``	/* php's `b` wins over `t` when both are named, and binary is its default. */`` |
|   1529 | 3559 | `	if( bText && !bBin ){` |
|      3 | 3560 | `		iFlag \|= PH7_IO_OPEN_TEXT;` |
|      2 | 3561 | `	}else{` |
|   1527 | 3562 | `		iFlag \|= PH7_IO_OPEN_BINARY;` |
|      - | 3563 | `	}` |
|   1529 | 3564 | `	*piFlags = iFlag;` |
|   1529 | 3565 | `	return 0;` |
|    786 | 3566 | `}` |
|      - | 3567 | `/*` |
|      - | 3568 | ` * Initialize the IO private structure.` |
|      - | 3569 | ` */` |
|   9016 | 3570 | `PH7_PRIVATE void InitIOPrivate(ph7_vm *pVm,const ph7_io_stream *pStream,io_private *pOut)` |
|      5 | 3571 | `{` |
|   9021 | 3572 | `	pOut->pStream = pStream;` |
|   9021 | 3573 | `	SyBlobInit(&pOut->sBuffer,&pVm->sAllocator);` |
|   9021 | 3574 | `	pOut->nOfft = 0;` |
|   9021 | 3575 | `	SyBlobInit(&pOut->sUri,&pVm->sAllocator);` |
|   9021 | 3576 | `	pOut->zMode[0] = 0;` |
|   9021 | 3577 | `	pOut->bEof = 0;` |
|   9021 | 3578 | `	pOut->iLastReadErr = 0;` |
|   9021 | 3579 | `	pOut->bDir = 0;` |
|   9021 | 3580 | `	pOut->bPersist = 0;` |
|   9021 | 3581 | `	pOut->nChunk = 8192; /* php's own default, and what stream_set_chunk_size() reports first */` |
|   9021 | 3582 | `	pOut->bNonBlock = 0;` |
|   9021 | 3583 | `	pOut->bHasTimeout = 0;` |
|   9021 | 3584 | `	pOut->bTimedOut = 0;` |
|   9021 | 3585 | `	pOut->pReadFilters = 0;` |
|   9021 | 3586 | `	pOut->pWriteFilters = 0;` |
|   9021 | 3587 | `	pOut->bFiltDone = 0;` |
|   9021 | 3588 | `	pOut->bFiltErr = 0;` |
|   9021 | 3589 | `	pOut->iFiltPos = 0;` |
|   9021 | 3590 | `	pOut->pCtxRes = 0;` |
|   9021 | 3591 | `	SyBlobInit(&pOut->sFilt,&pVm->sAllocator);` |
|   9021 | 3592 | `	pOut->nFiltOfft = 0;` |
|      - | 3593 | `	/* Set the magic number */` |
|   9021 | 3594 | `	pOut->iMagic = IO_PRIVATE_MAGIC;` |
|   9021 | 3595 | `}` |
|      - | 3596 | `/*` |
|      - | 3597 | `` * Record what the opener was asked for, for stream_get_meta_data()'s `uri` and`` |
|      - | 3598 | `` * `mode` keys. php keeps the URI exactly as written (a relative path stays`` |
|      - | 3599 | ` * relative) and the mode in a 16-byte field; passing a NULL/empty zUri leaves` |
|      - | 3600 | ` * the key out, which is how a popen() pipe reports no wrapper and no uri.` |
|      - | 3601 | ` */` |
|   8904 | 3602 | `PH7_PRIVATE void SetIOPrivateOpenedAs(io_private *pDev,const char *zUri,int nUriLen,const char *zMode,int nModeLen)` |
|      5 | 3603 | `{` |
|   8909 | 3604 | `	if( pDev == 0 ){` |
|    ! 0 | 3605 | `		return;` |
|      - | 3606 | `	}` |
|   8909 | 3607 | `	SyBlobReset(&pDev->sUri);` |
|   8909 | 3608 | `	if( zUri && nUriLen > 0 ){` |
|   1699 | 3609 | `		SyBlobAppend(&pDev->sUri,zUri,(sxu32)nUriLen);` |
|    847 | 3610 | `	}` |
|   8909 | 3611 | `	if( zMode && nModeLen > 0 ){` |
|   8909 | 3612 | `		if( nModeLen > (int)sizeof(pDev->zMode) - 1 ){` |
|    ! 0 | 3613 | `			nModeLen = (int)sizeof(pDev->zMode) - 1;` |
|    ! 0 | 3614 | `		}` |
|   8909 | 3615 | `		SyMemcpy(zMode,pDev->zMode,(sxu32)nModeLen);` |
|   8909 | 3616 | `		pDev->zMode[nModeLen] = 0;` |
|   4457 | 3617 | `	}else{` |
|    ! 0 | 3618 | `		pDev->zMode[0] = 0;` |
|      - | 3619 | `	}` |
|   4457 | 3620 | `}` |
|      - | 3621 | `/*` |
|      - | 3622 | ` * Release the IO private structure.` |
|      - | 3623 | ` */` |
|    102 | 3624 | `static void ReleaseIOPrivate(ph7_context *pCtx,io_private *pDev)` |
|      5 | 3625 | `{` |
|    107 | 3626 | `	PH7_StreamFilterReleaseChains(pDev);` |
|    107 | 3627 | `	SyBlobRelease(&pDev->sBuffer);` |
|    107 | 3628 | `	SyBlobRelease(&pDev->sFilt);` |
|    107 | 3629 | `	SyBlobRelease(&pDev->sUri);` |
|    107 | 3630 | `	pDev->iMagic = 0x2126; /* Invalid magic number so we can detetct misuse */` |
|      - | 3631 | `	/* Release the whole structure */` |
|    107 | 3632 | `	ph7_context_free_chunk(pCtx,pDev);` |
|    107 | 3633 | `}` |
|      - | 3634 | `/*` |
|      - | 3635 | ` * Release a handle shell whose open FAILED: it never reached PHP, so nothing can` |
|      - | 3636 | ` * hold a copy and the chunk goes back. For a caller outside this unit (the` |
|      - | 3637 | ` * XMLWriter URI writer builds its own handle the way fopen does).` |
|      - | 3638 | ` */` |
|     16 | 3639 | `PH7_PRIVATE void PH7_StreamReleaseUnopened(ph7_context *pCtx,io_private *pDev)` |
|      1 | 3640 | `{` |
|     17 | 3641 | `	ReleaseIOPrivate(pCtx,pDev);` |
|     17 | 3642 | `}` |
|      - | 3643 | `/*` |
|      - | 3644 | ` * Mark a user-facing IO handle as closed while keeping the io_private alive.` |
|      - | 3645 | ` * The caller has already closed the underlying OS handle; we drop the work` |
|      - | 3646 | ` * buffer and stamp the closed magic so every ph7_value that still references` |
|      - | 3647 | ` * this handle sees a "resource (closed)" (php semantics). The struct is` |
|      - | 3648 | ` * reclaimed in bulk when the VM allocator is torn down. Freeing it here — as` |
|      - | 3649 | ` * fclose/closedir/pclose used to — would dangle the caller's copy (a latent,` |
|      - | 3650 | ` * pool-masked UAF) and keep reporting the handle open.` |
|      - | 3651 | ` */` |
|   8672 | 3652 | `PH7_PRIVATE void MarkIOPrivateClosed(io_private *pDev)` |
|      5 | 3653 | `{` |
|      - | 3654 | `	/* A filter outliving its handle would keep answering is_resource() and hold` |
|      - | 3655 | `	 * a pointer to a closed device; every close path releases the chains before` |
|      - | 3656 | `	 * the device goes, and this is the backstop for one that forgets. */` |
|   8677 | 3657 | `	PH7_StreamFilterReleaseChains(pDev);` |
|   8677 | 3658 | `	SyBlobRelease(&pDev->sBuffer);` |
|   8677 | 3659 | `	SyBlobRelease(&pDev->sFilt);` |
|   8677 | 3660 | `	SyBlobRelease(&pDev->sUri);` |
|   8677 | 3661 | `	pDev->pHandle = 0;` |
|   8677 | 3662 | `	pDev->iMagic = IO_PRIVATE_CLOSED_MAGIC;` |
|   8677 | 3663 | `}` |
|      - | 3664 | `/*` |
|      - | 3665 | ` * Reset the IO private structure.` |
|      - | 3666 | ` */` |
|    442 | 3667 | `static void ResetIOPrivate(io_private *pDev)` |
|      4 | 3668 | `{` |
|    446 | 3669 | `	SyBlobReset(&pDev->sBuffer);` |
|    446 | 3670 | `	pDev->nOfft = 0;` |
|      - | 3671 | `	/* A seek moves the DEVICE, so whatever the read chain had already produced` |
|      - | 3672 | `	 * from the old position is not what the new one answers. */` |
|    446 | 3673 | `	SyBlobReset(&pDev->sFilt);` |
|    446 | 3674 | `	pDev->nFiltOfft = 0;` |
|    446 | 3675 | `	pDev->bFiltDone = 0;` |
|    446 | 3676 | `	pDev->bFiltErr = 0;` |
|    446 | 3677 | `	PH7_StreamFilterRewound(pDev);` |
|      - | 3678 | `	/* Every caller of this has just MOVED the device (a seek, a rewind, a` |
|      - | 3679 | `	 * truncate), and php clears the end-of-file flag on exactly those. */` |
|    446 | 3680 | `	pDev->bEof = 0;` |
|    446 | 3681 | `}` |
|      - | 3682 | `/* Forward declaration */` |
|      - | 3683 |  |
|      - | 3684 | `/*` |
|      - | 3685 | ` * resource fopen(string $filename,string $mode [,bool $use_include_path = false[,resource $context ]])` |
|      - | 3686 | ` *  Open a file,a URL or any other IO stream.` |
|      - | 3687 | ` * Parameters` |
|      - | 3688 | ` *  $filename` |
|      - | 3689 | ` *   If filename is of the form "scheme://...", it is assumed to be a URL and PHP will search` |
|      - | 3690 | ` *   for a protocol handler (also known as a wrapper) for that scheme. If no scheme is given` |
|      - | 3691 | ` *   then a regular file is assumed.` |
|      - | 3692 | ` *  $mode` |
|      - | 3693 | ` *   The mode parameter specifies the type of access you require to the stream` |
|      - | 3694 | ` *   See the block comment associated with the StrModeToFlags() for the supported` |
|      - | 3695 | ` *   modes.` |
|      - | 3696 | ` *  $use_include_path` |
|      - | 3697 | ` *   You can use the optional second parameter and set it to` |
|      - | 3698 | ` *   TRUE, if you want to search for the file in the include_path, too.` |
|      - | 3699 | ` *  $context` |
|      - | 3700 | ` *   A context stream resource.` |
|      - | 3701 | ` * Return` |
|      - | 3702 | ` *  File handle on success or FALSE on failure.` |
|      - | 3703 | ` */` |
|      - | 3704 | `/*` |
|      - | 3705 | ` * string\|false stream_get_contents(resource $stream, int $maxLength = -1,` |
|      - | 3706 | ` *                                  int $offset = -1)` |
|      - | 3707 | ` *  Read the remaining contents of a stream into a string.` |
|      - | 3708 | ` */` |
|    266 | 3709 | `PH7_PRIVATE int PH7_builtin_stream_get_contents(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      2 | 3710 | `{` |
|      - | 3711 | `	const ph7_io_stream *pStream;` |
|      - | 3712 | `	io_private *pDev;` |
|    268 | 3713 | `	ph7_int64 nMax = -1;` |
|      - | 3714 | `	char zBuf[4096];` |
|      - | 3715 | `	ph7_int64 nRead;` |
|    268 | 3716 | `	if( nArg < 1 \|\| !ph7_value_is_resource(apArg[0]) ){` |
|    ! 0 | 3717 | `		ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"Expecting an IO handle");` |
|    ! 0 | 3718 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 3719 | `		return PH7_OK;` |
|      - | 3720 | `	}` |
|    268 | 3721 | `	pDev = (io_private *)ph7_value_to_resource(apArg[0]);` |
|    268 | 3722 | `	if( IO_PRIVATE_INVALID(pDev) ){` |
|    ! 0 | 3723 | `		ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"Expecting an IO handle");` |
|    ! 0 | 3724 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 3725 | `		return PH7_OK;` |
|      - | 3726 | `	}` |
|    268 | 3727 | `	pStream = pDev->pStream;` |
|    268 | 3728 | `	if( pStream == 0 \|\| pStream->xRead == 0 ){` |
|    ! 0 | 3729 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 3730 | `		return PH7_OK;` |
|      - | 3731 | `	}` |
|    268 | 3732 | `	if( nArg > 1 && !ph7_value_is_null(apArg[1]) ){` |
|      - | 3733 | `		/* PHP 8 raises a catchable ValueError below -1; -1 (or the NULL` |
|      - | 3734 | `		 * default) means "read until EOF". */` |
|     17 | 3735 | `		nMax = ph7_value_to_int64(apArg[1]);` |
|     17 | 3736 | `		if( nMax < -1 ){` |
|      3 | 3737 | `			return PH7_VmThrowException(pCtx,"ValueError",` |
|      - | 3738 | `				"stream_get_contents(): Argument #2 ($length) must be greater than or equal to -1");` |
|      - | 3739 | `		}` |
|      7 | 3740 | `	}` |
|    266 | 3741 | `	if( nArg > 2 ){` |
|      9 | 3742 | `		ph7_int64 iOfft = ph7_value_to_int64(apArg[2]);` |
|      9 | 3743 | `		if( iOfft >= 0 && pStream->xSeek ){` |
|      9 | 3744 | `			if( pStream->xSeek(pDev->pHandle,iOfft,0/*SEEK_SET*/) == PH7_OK ){` |
|      - | 3745 | `				/* A seek DISCARDS what was buffered ahead — the bytes belong to` |
|      - | 3746 | `				 * the position we just left. Without this the read below served` |
|      - | 3747 | `				 * the old position's leftovers and then continued from the new` |
|      - | 3748 | `				 * one. */` |
|      9 | 3749 | `				ResetIOPrivate(pDev);` |
|      4 | 3750 | `			}` |
|      4 | 3751 | `		}` |
|      4 | 3752 | `	}` |
|    266 | 3753 | `	ph7_result_string(pCtx,"",0); /* seed an empty string result */` |
|    513 | 3754 | `	while( nMax != 0 ){` |
|    507 | 3755 | `		ph7_int64 nAsk = (ph7_int64)sizeof(zBuf);` |
|    507 | 3756 | `		if( nMax > 0 && nMax < nAsk ){` |
|      5 | 3757 | `			nAsk = nMax;` |
|      2 | 3758 | `		}` |
|      - | 3759 | `		/* Through PH7_StreamRead, not the device: this is a SCRIPT-level read,` |
|      - | 3760 | `		 * and the line readers buffer AHEAD. Reading the device directly meant` |
|      - | 3761 | ``		 * `stream_get_contents()` after any fgets()/fgetc()/stream_get_line()`` |
|      - | 3762 | `		 * skipped everything still sitting in that buffer — on a file the` |
|      - | 3763 | `		 * line reader had already drained to its end, that is the WHOLE` |
|      - | 3764 | `		 * remainder, so the everyday "read the first line, then take the rest"` |
|      - | 3765 | `		 * idiom answered "" and the position it left behind was wrong too. */` |
|    507 | 3766 | `		nRead = PH7_StreamRead(pDev,zBuf,nAsk);` |
|    507 | 3767 | `		if( nRead < 1 ){` |
|    260 | 3768 | `			if( nRead == 0 ){` |
|    256 | 3769 | `				pDev->bEof = 1;` |
|    127 | 3770 | `			}` |
|    260 | 3771 | `			StreamReportReadFailure(pCtx,pDev);` |
|    260 | 3772 | `			break;` |
|      - | 3773 | `		}` |
|    249 | 3774 | `		ph7_result_string(pCtx,zBuf,(int)nRead); /* appends */` |
|    249 | 3775 | `		if( nMax > 0 ){` |
|      5 | 3776 | `			nMax -= nRead;` |
|      2 | 3777 | `		}` |
|      2 | 3778 | `	}` |
|    266 | 3779 | `	return PH7_OK;` |
|    135 | 3780 | `}` |
|      - | 3781 | `/*` |
|      - | 3782 | ` * array stream_get_wrappers(void) — names of the registered stream devices.` |
|      - | 3783 | ` */` |
|     20 | 3784 | `PH7_PRIVATE int PH7_builtin_stream_get_wrappers(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      3 | 3785 | `{` |
|      - | 3786 | `	ph7_value *pArr,*pV;` |
|      - | 3787 | `	ph7_io_stream **apDev;` |
|      - | 3788 | `	sxu32 n;` |
|     10 | 3789 | `	SXUNUSED(nArg);` |
|     10 | 3790 | `	SXUNUSED(apArg);` |
|     23 | 3791 | `	pArr = ph7_context_new_array(pCtx);` |
|     23 | 3792 | `	pV = ph7_context_new_scalar(pCtx);` |
|     23 | 3793 | `	if( pArr == 0 \|\| pV == 0 ){` |
|    ! 0 | 3794 | `		ph7_result_null(pCtx);` |
|    ! 0 | 3795 | `		return PH7_OK;` |
|      - | 3796 | `	}` |
|     23 | 3797 | `	apDev = (ph7_io_stream **)SySetBasePtr(&pCtx->pVm->aIOstream);` |
|    135 | 3798 | `	for( n = 0 ; n < SySetUsed(&pCtx->pVm->aIOstream) ; n++ ){` |
|      - | 3799 | `		/* A device a script has unregistered is GONE from php's list -- both a` |
|      - | 3800 | `		 * built-in it switched off and a userland wrapper it withdrew, which` |
|      - | 3801 | `		 * PHL used to keep naming here after neutering the slot behind it. */` |
|    115 | 3802 | `		if( PH7_VmStreamDeviceSuppressed(pCtx->pVm,apDev[n]) ){` |
|      9 | 3803 | `			continue;` |
|      - | 3804 | `		}` |
|    107 | 3805 | `		ph7_value_string(pV,apDev[n]->zName,-1);` |
|    107 | 3806 | `		ph7_array_add_elem(pArr,0,pV);` |
|    107 | 3807 | `		ph7_value_reset_string_cursor(pV);` |
|     55 | 3808 | `	}` |
|     23 | 3809 | `	ph7_result_value(pCtx,pArr);` |
|     23 | 3810 | `	return PH7_OK;` |
|     13 | 3811 | `}` |
|      - | 3812 | `/* The userland-wrapper pool is declared further down this file. */` |
|      - | 3813 | `static int IoPrivateIsUwrap(const ph7_io_stream *pStream);` |
|      - | 3814 | `static ph7_class_instance * IoPrivateUwrapObject(io_private *pDev);` |
|      - | 3815 | `/*` |
|      - | 3816 | ` * php names TWO things in a stream's metadata: the WRAPPER that opened it` |
|      - | 3817 | `` * (`wrapper_type`) and the ops that drive it (`stream_type`). They differ for`` |
|      - | 3818 | `` * nearly every device — an ordinary file is opened by `plainfile` and driven by`` |
|      - | 3819 | `` * `STDIO` — and PHL answered its own single device name for both, so neither`` |
|      - | 3820 | ` * key ever matched php. A stream php opens with NO wrapper (a popen()/proc_open()` |
|      - | 3821 | `` * pipe, a socket) reports no `wrapper_type` at all; *pzWrapper stays 0 for those.`` |
|      - | 3822 | ` */` |
|    100 | 3823 | `static void IoPrivateStreamLabels(io_private *pDev,const char **pzWrapper,const char **pzStream)` |
|      4 | 3824 | `{` |
|    104 | 3825 | `	const ph7_io_stream *pS = pDev->pStream;` |
|    104 | 3826 | `	*pzWrapper = 0;` |
|    104 | 3827 | `	*pzStream  = "STDIO";` |
|    104 | 3828 | `	if( pS == 0 ){` |
|    ! 0 | 3829 | `		return;` |
|      - | 3830 | `	}` |
|    104 | 3831 | `	if( pDev->bDir ){` |
|      3 | 3832 | `		*pzWrapper = "plainfile";` |
|      3 | 3833 | `		*pzStream  = "dir";` |
|      3 | 3834 | `		return;` |
|      - | 3835 | `	}` |
|    102 | 3836 | `	if( is_php_stream(pS) ){` |
|     31 | 3837 | `		*pzWrapper = "PHP";` |
|     31 | 3838 | `		if( PH7_PhpStreamKind(pDev->pHandle) == PH7_IO_STREAM_OUTPUT ){` |
|      - | 3839 | `			/* php://output is the VM's output consumer, not a descriptor. */` |
|      3 | 3840 | `			*pzStream = "Output";` |
|      3 | 3841 | `			return;` |
|      - | 3842 | `		}` |
|     29 | 3843 | `		if( PH7_PhpStreamKind(pDev->pHandle) == PH7_IO_STREAM_MEMORY ){` |
|      - | 3844 | `			/* php://memory and php://temp are ONE device here and two in php,` |
|      - | 3845 | `			 * which labels them apart; the URI is what separates them. */` |
|     21 | 3846 | `			const char *zUri = (const char *)SyBlobData(&pDev->sUri);` |
|     21 | 3847 | `			sxu32 nUri = SyBlobLength(&pDev->sUri);` |
|     30 | 3848 | `			*pzStream = ( nUri >= sizeof("php://temp")-1` |
|     18 | 3849 | `			           && SyStrnicmp(zUri,"php://temp",sizeof("php://temp")-1) == 0 )` |
|     18 | 3850 | `				? "TEMP" : "MEMORY";` |
|      9 | 3851 | `		}` |
|     29 | 3852 | `		return;` |
|      - | 3853 | `	}` |
|     73 | 3854 | `	if( is_data_stream(pS) ){` |
|     15 | 3855 | `		*pzWrapper = *pzStream = "RFC2397";` |
|     15 | 3856 | `		return;` |
|      - | 3857 | `	}` |
|     59 | 3858 | `	if( IoPrivateIsUwrap(pS) ){` |
|      7 | 3859 | `		*pzWrapper = *pzStream = "user-space";` |
|      7 | 3860 | `		return;` |
|      - | 3861 | `	}` |
|     53 | 3862 | `	if( pS->zName && SyStrncmp(pS->zName,"tcp",sizeof("tcp")) == 0 ){` |
|      - | 3863 | `		/* php names the socket ops and reports no wrapper for them — and names` |
|      - | 3864 | `		 * a socket with no transport under it (a pair) differently again. */` |
|      - | 3865 | `#ifdef PH7_ENABLE_NET` |
|     18 | 3866 | `		*pzStream = (pDev->pHandle && ((sock_private *)pDev->pHandle)->bGeneric)` |
|      - | 3867 | `			? "generic_socket" : "tcp_socket/ssl";` |
|      - | 3868 | `#else` |
|      - | 3869 | `		*pzStream = "tcp_socket/ssl";` |
|      - | 3870 | `#endif` |
|     18 | 3871 | `		return;` |
|      - | 3872 | `	}` |
|     36 | 3873 | `	if( SyBlobLength(&pDev->sUri) < 1 ){` |
|      - | 3874 | `		/* Opened from a DESCRIPTOR rather than through a wrapper — a popen()` |
|      - | 3875 | `		 * or proc_open() pipe end, or a device an extension built by hand like` |
|      - | 3876 | `		 * PDO's blob handle — which is precisely when php reports neither a` |
|      - | 3877 | ``		 * `wrapper_type` nor a `uri`. Such a device names its OWN ops: php's`` |
|      - | 3878 | ``		 * blob stream reports `PDOSQLite`, not the `STDIO` a descriptor gets. */`` |
|     10 | 3879 | `		if( pS->zName && pS->xOpen == 0 && pS->xOpenDir == 0 && pS->xSeek != 0 ){` |
|      - | 3880 | `			/* No opener, no descriptor under it, and it can SEEK: an extension` |
|      - | 3881 | `			 * built this handle itself (PDO's blob and LOB streams), and php` |
|      - | 3882 | `			 * names such a device's own ops. A pipe or a socket end has no seek` |
|      - | 3883 | `			 * and stays php's descriptor label. */` |
|      7 | 3884 | `			*pzStream = pS->zName;` |
|      3 | 3885 | `		}` |
|     10 | 3886 | `		return;` |
|      - | 3887 | `	}` |
|     27 | 3888 | `	*pzWrapper = "plainfile";` |
|     54 | 3889 | `}` |
|      - | 3890 | `/*` |
|      - | 3891 | ` * data:// carries its own metadata in php, and all of it comes back out of the` |
|      - | 3892 | ``  * URI the wrapper parsed: `data://<mediatype>[;name=value]*[;base64],<payload>` `` |
|      - | 3893 | ` * answers the media type, ONE KEY PER PARAMETER, and the base64 flag last. A` |
|      - | 3894 | `` * URI naming no media type has no `mediatype` key at all — php does not`` |
|      - | 3895 | ` * substitute the RFC's default — and a repeated parameter keeps its last value.` |
|      - | 3896 | ` */` |
|     12 | 3897 | `static void IoPrivateDataMeta(ph7_context *pCtx,io_private *pDev,ph7_value *pArr,ph7_value *pV)` |
|      1 | 3898 | `{` |
|     13 | 3899 | `	const char *zUri = (const char *)SyBlobData(&pDev->sUri);` |
|     13 | 3900 | `	sxu32 nUri = SyBlobLength(&pDev->sUri);` |
|     13 | 3901 | `	sxu32 nStart = 0,nComma,nSeg,i;` |
|     13 | 3902 | `	int bBase64 = 0,bFirst = 1;` |
|     13 | 3903 | `	if( nUri >= sizeof("data://")-1 && SyStrnicmp(zUri,"data://",sizeof("data://")-1) == 0 ){` |
|     13 | 3904 | `		nStart = sizeof("data://")-1;` |
|      6 | 3905 | `	}else if( nUri >= sizeof("data:")-1 && SyStrnicmp(zUri,"data:",sizeof("data:")-1) == 0 ){` |
|    ! 0 | 3906 | `		nStart = sizeof("data:")-1;` |
|    ! 0 | 3907 | `	}` |
|     13 | 3908 | `	nComma = nStart;` |
|    329 | 3909 | `	while( nComma < nUri && zUri[nComma] != ',' ){` |
|    317 | 3910 | `		nComma++;` |
|      1 | 3911 | `	}` |
|      - | 3912 | `	/* Walk the ';'-separated segments in front of the payload. */` |
|     23 | 3913 | `	for( nSeg = nStart ; nSeg <= nComma ; ){` |
|     23 | 3914 | `		sxu32 nEnd = nSeg;` |
|    329 | 3915 | `		while( nEnd < nComma && zUri[nEnd] != ';' ){` |
|    307 | 3916 | `			nEnd++;` |
|      1 | 3917 | `		}` |
|     23 | 3918 | `		if( bFirst ){` |
|     13 | 3919 | `			if( nEnd > nSeg ){` |
|     11 | 3920 | `				ph7_value_string(pV,&zUri[nSeg],(int)(nEnd - nSeg));` |
|     11 | 3921 | `				ph7_array_add_strkey_elem(pArr,"mediatype",pV);` |
|     11 | 3922 | `				ph7_value_reset_string_cursor(pV);` |
|      5 | 3923 | `			}` |
|     13 | 3924 | `			bFirst = 0;` |
|     17 | 3925 | `		}else if( nEnd - nSeg == sizeof("base64")-1` |
|      8 | 3926 | `		       && SyStrnicmp(&zUri[nSeg],"base64",sizeof("base64")-1) == 0 ){` |
|      5 | 3927 | `			bBase64 = 1;` |
|      3 | 3928 | `		}else{` |
|      - | 3929 | ``			/* `name=value`; php keys the array by the name, so a repeat wins. */`` |
|    167 | 3930 | `			for( i = nSeg ; i < nEnd && zUri[i] != '=' ; i++ ){}` |
|      7 | 3931 | `			if( i < nEnd && i > nSeg ){` |
|      - | 3932 | `				/* The name is keyed WHOLE — it has no length limit in the URI,` |
|      - | 3933 | `				 * and a clamped one files the value under a key no script can` |
|      - | 3934 | `				 * look up. */` |
|      7 | 3935 | `				ph7_value *pKey = ph7_context_new_scalar(pCtx);` |
|      7 | 3936 | `				if( pKey ){` |
|      7 | 3937 | `					ph7_value_string(pKey,&zUri[nSeg],(int)(i - nSeg));` |
|      7 | 3938 | `					ph7_value_string(pV,&zUri[i+1],(int)(nEnd - i - 1));` |
|      7 | 3939 | `					ph7_array_add_elem(pArr,pKey,pV);` |
|      7 | 3940 | `					ph7_value_reset_string_cursor(pV);` |
|      7 | 3941 | `					ph7_context_release_value(pCtx,pKey);` |
|      3 | 3942 | `				}` |
|      3 | 3943 | `			}` |
|      - | 3944 | `		}` |
|     23 | 3945 | `		if( nEnd >= nComma ){` |
|     13 | 3946 | `			break;` |
|      - | 3947 | `		}` |
|     11 | 3948 | `		nSeg = nEnd + 1;` |
|      1 | 3949 | `	}` |
|     13 | 3950 | `	ph7_value_bool(pV,bBase64);` |
|     13 | 3951 | `	ph7_array_add_strkey_elem(pArr,"base64",pV);` |
|     13 | 3952 | `}` |
|      - | 3953 | `/*` |
|      - | 3954 | ` * array stream_get_meta_data(resource $stream)` |
|      - | 3955 | ` *` |
|      - | 3956 | ` * php's own key set, in php's own order. What used to be here answered a` |
|      - | 3957 | `` * best-effort shape: `mode` and `uri` did not exist at all (so the documented`` |
|      - | 3958 | `` * way to ask a handle what FILE it is on was an `Undefined array key` and`` |
|      - | 3959 | `` * NULL), `eof` was hardcoded FALSE (a `while (!$m['eof'])` loop never ended),`` |
|      - | 3960 | `` * `unread_bytes` was hardcoded 0, and `wrapper_type`/`stream_type` were both`` |
|      - | 3961 | ` * PHL's internal device name rather than php's two different labels.` |
|      - | 3962 | ` */` |
|     88 | 3963 | `PH7_PRIVATE int PH7_builtin_stream_get_meta_data(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      4 | 3964 | `{` |
|      - | 3965 | `	const char *zWrapper,*zStream;` |
|      - | 3966 | `	io_private *pDev;` |
|      - | 3967 | `	ph7_value *pArr,*pV;` |
|      - | 3968 | `	sxu32 nUnread;` |
|     92 | 3969 | `	if( nArg < 1 \|\| !ph7_value_is_resource(apArg[0]) ){` |
|    ! 0 | 3970 | `		ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"Expecting an IO handle");` |
|    ! 0 | 3971 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 3972 | `		return PH7_OK;` |
|      - | 3973 | `	}` |
|     92 | 3974 | `	pDev = (io_private *)ph7_value_to_resource(apArg[0]);` |
|     92 | 3975 | `	if( IO_PRIVATE_INVALID(pDev) ){` |
|    ! 0 | 3976 | `		ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"Expecting an IO handle");` |
|    ! 0 | 3977 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 3978 | `		return PH7_OK;` |
|      - | 3979 | `	}` |
|     92 | 3980 | `	pArr = ph7_context_new_array(pCtx);` |
|     92 | 3981 | `	pV = ph7_context_new_scalar(pCtx);` |
|     92 | 3982 | `	if( pArr == 0 \|\| pV == 0 ){` |
|    ! 0 | 3983 | `		ph7_result_null(pCtx);` |
|    ! 0 | 3984 | `		return PH7_OK;` |
|      - | 3985 | `	}` |
|     92 | 3986 | `	IoPrivateStreamLabels(pDev,&zWrapper,&zStream);` |
|      - | 3987 | `	/* Sample this BEFORE the eof probe below: php answers eof from state it` |
|      - | 3988 | ``	 * already has and never reads ahead for it, so its `unread_bytes` counts`` |
|      - | 3989 | `	 * only what the SCRIPT's own reads left buffered. */` |
|     92 | 3990 | `	nUnread = StreamAheadBytes(pDev);` |
|     92 | 3991 | `	if( is_data_stream(pDev->pStream) ){` |
|      - | 3992 | `		/* A device that answers metadata of its OWN replaces php's three` |
|      - | 3993 | `		 * defaults rather than adding to them: data:// (and php://temp, which` |
|      - | 3994 | `		 * simply has none) report no timed_out/blocked/eof at all. */` |
|     13 | 3995 | `		IoPrivateDataMeta(pCtx,pDev,pArr,pV);` |
|     13 | 3996 | `		ph7_value_reset_string_cursor(pV);` |
|     83 | 3997 | `	}else if( is_php_stream(pDev->pStream)` |
|     49 | 3998 | `	       && PH7_PhpStreamKind(pDev->pHandle) == PH7_IO_STREAM_MEMORY` |
|     22 | 3999 | `	       && SyStrncmp(zStream,"TEMP",sizeof("TEMP")) == 0 ){` |
|      - | 4000 | `		/* php://temp: same rule, no keys of its own. */` |
|      2 | 4001 | `	}else{` |
|     78 | 4002 | `		ph7_value_bool(pV,pDev->bTimedOut != 0);` |
|     78 | 4003 | `		ph7_array_add_strkey_elem(pArr,"timed_out",pV);` |
|      - | 4004 | `		/* A stream php cannot put in non-blocking mode always reports blocked;` |
|      - | 4005 | `		 * bNonBlock is only ever set for one that CAN. */` |
|     78 | 4006 | `		ph7_value_bool(pV,pDev->bNonBlock == 0);` |
|     78 | 4007 | `		ph7_array_add_strkey_elem(pArr,"blocked",pV);` |
|      - | 4008 | `		/* The read-ahead this performs is feof()'s own, so a script that asks` |
|      - | 4009 | `		 * for the metadata and then reads sees every byte. */` |
|     78 | 4010 | `		ph7_value_bool(pV,PH7_StreamAtEof(pDev) != 0);` |
|     78 | 4011 | `		ph7_array_add_strkey_elem(pArr,"eof",pV);` |
|      - | 4012 | `	}` |
|      - | 4013 | `	{` |
|     92 | 4014 | `		ph7_class_instance *pObj = IoPrivateUwrapObject(pDev);` |
|     92 | 4015 | `		if( pObj ){` |
|      - | 4016 | `			/* php hands the wrapper INSTANCE back, which is the only way a` |
|      - | 4017 | `			 * script can reach the object serving an open userland stream. */` |
|      - | 4018 | `			ph7_value sObj;` |
|      5 | 4019 | `			PH7_MemObjInit(pCtx->pVm,&sObj);` |
|      5 | 4020 | `			sObj.x.pOther = pObj;` |
|      5 | 4021 | `			sObj.iFlags = MEMOBJ_OBJ;` |
|      5 | 4022 | `			ph7_array_add_strkey_elem(pArr,"wrapper_data",&sObj);` |
|      2 | 4023 | `		}` |
|      - | 4024 | `	}` |
|     92 | 4025 | `	if( zWrapper ){` |
|     67 | 4026 | `		ph7_value_string(pV,zWrapper,-1);` |
|     67 | 4027 | `		ph7_array_add_strkey_elem(pArr,"wrapper_type",pV);` |
|     67 | 4028 | `		ph7_value_reset_string_cursor(pV);` |
|     32 | 4029 | `	}` |
|     92 | 4030 | `	ph7_value_string(pV,zStream,-1);` |
|     92 | 4031 | `	ph7_array_add_strkey_elem(pArr,"stream_type",pV);` |
|     92 | 4032 | `	ph7_value_reset_string_cursor(pV);` |
|     92 | 4033 | `	ph7_value_string(pV,pDev->zMode,-1);` |
|     92 | 4034 | `	ph7_array_add_strkey_elem(pArr,"mode",pV);` |
|     92 | 4035 | `	ph7_value_reset_string_cursor(pV);` |
|      - | 4036 | `	/* Bytes already pulled off the device and not yet handed to the script —` |
|      - | 4037 | `	 * php's own writepos-minus-readpos, which was hardcoded 0. */` |
|     92 | 4038 | `	ph7_value_int64(pV,(ph7_int64)nUnread);` |
|     92 | 4039 | `	ph7_array_add_strkey_elem(pArr,"unread_bytes",pV);` |
|      - | 4040 | `	{` |
|      - | 4041 | `		/* php answers this from what the handle actually SITS ON, not from what` |
|      - | 4042 | `		 * the device could do: php://stdout is seekable into a file and not` |
|      - | 4043 | `		 * down a pipe, php://output never is, and a pipe is not. Ask the` |
|      - | 4044 | `		 * descriptor first and the device second; a USERLAND wrapper is php's` |
|      - | 4045 | `		 * one exception — its ops always carry a seek, so php always says yes. */` |
|     92 | 4046 | `		int bSeekable = pDev->pStream != 0 && pDev->pStream->xSeek != 0;` |
|     92 | 4047 | `		if( pDev->bDir ){` |
|      - | 4048 | `			/* php's directory ops carry a rewind, so a dir handle is seekable —` |
|      - | 4049 | `			 * and asking the FILE device where it is would hand lseek() the` |
|      - | 4050 | `			 * DIR* this handle stores where a file stores its descriptor. */` |
|      3 | 4051 | `			bSeekable = 1;` |
|     91 | 4052 | `		}else if( bSeekable && !IoPrivateIsUwrap(pDev->pStream) ){` |
|     67 | 4053 | `			int rcSeek = PH7_StreamHandleCanSeek(pDev);` |
|     67 | 4054 | `			if( rcSeek >= 0 ){` |
|     31 | 4055 | `				bSeekable = rcSeek;` |
|     51 | 4056 | `			}else if( pDev->pStream->xOpen != 0 && pDev->pStream->xTell != 0 ){` |
|      - | 4057 | `				/* Ask the HANDLE where it is, which is how a descriptor-backed` |
|      - | 4058 | `				 * device says it cannot seek. A device an extension built by` |
|      - | 4059 | `				 * hand (PDO's blob handle) has no opener and answers for itself` |
|      - | 4060 | `				 * -- its xSeek IS the answer, and a position it reports as` |
|      - | 4061 | `				 * unknown after a failed seek must not read as "not seekable". */` |
|     30 | 4062 | `				bSeekable = pDev->pStream->xTell(pDev->pHandle) >= 0;` |
|     14 | 4063 | `			}` |
|     32 | 4064 | `		}` |
|     92 | 4065 | `		ph7_value_bool(pV,bSeekable);` |
|      - | 4066 | `	}` |
|     92 | 4067 | `	ph7_array_add_strkey_elem(pArr,"seekable",pV);` |
|     92 | 4068 | `	if( SyBlobLength(&pDev->sUri) > 0 ){` |
|      - | 4069 | `		/* php keeps the path exactly as the opener received it — a relative` |
|      - | 4070 | `		 * one stays relative — and omits the key for a stream that has none. */` |
|     75 | 4071 | `		ph7_value_string(pV,(const char *)SyBlobData(&pDev->sUri),(int)SyBlobLength(&pDev->sUri));` |
|     75 | 4072 | `		ph7_array_add_strkey_elem(pArr,"uri",pV);` |
|     75 | 4073 | `		ph7_value_reset_string_cursor(pV);` |
|     36 | 4074 | `	}` |
|     92 | 4075 | `	ph7_result_value(pCtx,pArr);` |
|     92 | 4076 | `	return PH7_OK;` |
|     48 | 4077 | `}` |
|      - | 4078 | `/*` |
|      - | 4079 | ` * ---------------------------------------------------------------------------` |
|      - | 4080 | ` * Stream contexts (stream_context_create and the accessor family).` |
|      - | 4081 | ` *` |
|      - | 4082 | `` * php's context is a `stream-context` RESOURCE holding two things: a`` |
|      - | 4083 | `` * wrapper => option => value map, and the `notification` parameter. Both`` |
|      - | 4084 | ` * levels keep INSERTION order, which is the order stream_context_get_options()` |
|      - | 4085 | ` * answers in, so the store is a real nested array rather than a flat table.` |
|      - | 4086 | ` *` |
|      - | 4087 | ` * A PHL resource is a bare void*, so the struct opens with an io_private` |
|      - | 4088 | ` * header carrying its own magic (the shape proc_open()'s handle already uses)` |
|      - | 4089 | ` * and the VM owns every one it hands out.` |
|      - | 4090 | ` * ---------------------------------------------------------------------------` |
|      - | 4091 | ` */` |
|      - | 4092 | `/* Allocate one context, chained on the VM registry. */` |
|    312 | 4093 | `static phl_stream_ctx * StreamCtxNew(ph7_vm *pVm)` |
|      5 | 4094 | `{` |
|      - | 4095 | `	phl_stream_ctx *pRes;` |
|    317 | 4096 | `	if( pVm == 0 ){` |
|    ! 0 | 4097 | `		return 0;` |
|      - | 4098 | `	}` |
|    317 | 4099 | `	pRes = (phl_stream_ctx *)SyMemBackendAlloc(&pVm->sAllocator,sizeof(phl_stream_ctx));` |
|    317 | 4100 | `	if( pRes == 0 ){` |
|    ! 0 | 4101 | `		return 0;` |
|      - | 4102 | `	}` |
|    317 | 4103 | `	SyZero(pRes,sizeof(phl_stream_ctx));` |
|    317 | 4104 | `	pRes->base.iMagic = STREAM_CTX_MAGIC;` |
|    317 | 4105 | `	pRes->pVm = pVm;` |
|    317 | 4106 | `	pRes->pOptions = ph7_new_array(pVm);` |
|    317 | 4107 | `	if( pRes->pOptions == 0 ){` |
|    ! 0 | 4108 | `		SyMemBackendFree(&pVm->sAllocator,pRes);` |
|    ! 0 | 4109 | `		return 0;` |
|      - | 4110 | `	}` |
|    317 | 4111 | `	pRes->pNext = (phl_stream_ctx *)pVm->pStreamCtx;` |
|    317 | 4112 | `	pVm->pStreamCtx = (void *)pRes;` |
|    317 | 4113 | `	return pRes;` |
|    161 | 4114 | `}` |
|      - | 4115 | `/*` |
|      - | 4116 | ` * The context behind a ph7_value, or 0 when the value is not one. The magic` |
|      - | 4117 | ` * probe is the same in-bounds one every resource here answers to.` |
|      - | 4118 | ` */` |
|    136 | 4119 | `PH7_PRIVATE phl_stream_ctx * PH7_StreamCtxFromValue(ph7_value *pVal)` |
|      5 | 4120 | `{` |
|      - | 4121 | `	phl_stream_ctx *pRes;` |
|    141 | 4122 | `	if( pVal == 0 \|\| !ph7_value_is_resource(pVal) ){` |
|    ! 0 | 4123 | `		return 0;` |
|      - | 4124 | `	}` |
|    141 | 4125 | `	pRes = (phl_stream_ctx *)pVal->x.pOther;` |
|    141 | 4126 | `	if( pRes == 0 \|\| pRes->base.iMagic != STREAM_CTX_MAGIC ){` |
|     49 | 4127 | `		return 0;` |
|      - | 4128 | `	}` |
|     94 | 4129 | `	return pRes;` |
|     73 | 4130 | `}` |
|      - | 4131 | `/*` |
|      - | 4132 | ` * The per-VM DEFAULT context. php creates it on demand — the first` |
|      - | 4133 | ` * stream_context_get_default()/set_default() call — and every opener that was` |
|      - | 4134 | ` * handed no context of its own falls back to it.` |
|      - | 4135 | ` */` |
|  74748 | 4136 | `PH7_PRIVATE phl_stream_ctx * PH7_StreamCtxDefault(ph7_vm *pVm)` |
|      5 | 4137 | `{` |
|  74753 | 4138 | `	if( pVm == 0 ){` |
|    ! 0 | 4139 | `		return 0;` |
|      - | 4140 | `	}` |
|  74753 | 4141 | `	if( pVm->pDefaultCtx == 0 ){` |
|    257 | 4142 | `		pVm->pDefaultCtx = (void *)StreamCtxNew(pVm);` |
|    126 | 4143 | `	}` |
|  74753 | 4144 | `	return (phl_stream_ctx *)pVm->pDefaultCtx;` |
|  37379 | 4145 | `}` |
|      - | 4146 | `/*` |
|      - | 4147 | ` * Drop every context this VM created. Called from PH7_VmReset, so a reused VM` |
|      - | 4148 | ` * (the -S server's) does not carry one request's default context into the next.` |
|      - | 4149 | ` */` |
|     16 | 4150 | `PH7_PRIVATE void PH7_StreamCtxVmReset(ph7_vm *pVm)` |
|    ! 0 | 4151 | `{` |
|      - | 4152 | `	phl_stream_ctx *pRes;` |
|     16 | 4153 | `	if( pVm == 0 ){` |
|    ! 0 | 4154 | `		return;` |
|      - | 4155 | `	}` |
|     16 | 4156 | `	pRes = (phl_stream_ctx *)pVm->pStreamCtx;` |
|     24 | 4157 | `	while( pRes ){` |
|      8 | 4158 | `		phl_stream_ctx *pNext = pRes->pNext;` |
|      8 | 4159 | `		if( pRes->pOptions ){` |
|      8 | 4160 | `			ph7_release_value(pVm,pRes->pOptions);` |
|      4 | 4161 | `		}` |
|      8 | 4162 | `		if( pRes->pNotify ){` |
|    ! 0 | 4163 | `			ph7_release_value(pVm,pRes->pNotify);` |
|    ! 0 | 4164 | `		}` |
|      - | 4165 | `		/* Any ph7_value still naming this pointer must stop reporting a live` |
|      - | 4166 | `		 * context, so clear the magic before the memory goes back. */` |
|      8 | 4167 | `		pRes->base.iMagic = 0;` |
|      8 | 4168 | `		SyMemBackendFree(&pVm->sAllocator,pRes);` |
|      8 | 4169 | `		pRes = pNext;` |
|    ! 0 | 4170 | `	}` |
|     16 | 4171 | `	pVm->pStreamCtx = 0;` |
|     16 | 4172 | `	pVm->pDefaultCtx = 0;` |
|      - | 4173 | `	/* Whatever an interrupted open left armed named one of those. */` |
|     16 | 4174 | `	pVm->pOpenCtx = 0;` |
|      8 | 4175 | `}` |
|      - | 4176 | `/* The live element of pArray under pKey, or 0 when there is none. */` |
|    118 | 4177 | `static ph7_value * StreamCtxFetch(ph7_value *pArray,ph7_value *pKey)` |
|      4 | 4178 | `{` |
|      - | 4179 | `	ph7_hashmap_node *pNode;` |
|    122 | 4180 | `	if( pArray == 0 \|\| (pArray->iFlags & MEMOBJ_HASHMAP) == 0 ){` |
|    ! 0 | 4181 | `		return 0;` |
|      - | 4182 | `	}` |
|    122 | 4183 | `	if( PH7_HashmapLookup((ph7_hashmap *)pArray->x.pOther,pKey,&pNode) != SXRET_OK ){` |
|     56 | 4184 | `		return 0;` |
|      - | 4185 | `	}` |
|     70 | 4186 | `	return (ph7_value *)SySetAt(&pArray->pVm->aMemObj,pNode->nValIdx);` |
|     63 | 4187 | `}` |
|      - | 4188 | `/*` |
|      - | 4189 | ` * Store one option. The wrapper's sub-array is created on first use; an` |
|      - | 4190 | ` * existing one may be SHARED with the script array it was stored from, so it` |
|      - | 4191 | ` * is separated first — otherwise setting an option would write through into` |
|      - | 4192 | ` * the caller's own array.` |
|      - | 4193 | ` */` |
|     66 | 4194 | `static int StreamCtxSetOption(phl_stream_ctx *pRes,ph7_value *pWrapper,ph7_value *pName,ph7_value *pValue)` |
|      4 | 4195 | `{` |
|      - | 4196 | `	ph7_value sKey,sName,sVal;` |
|      - | 4197 | `	ph7_value *pSub;` |
|      - | 4198 | `	ph7_hashmap *pMap;` |
|     70 | 4199 | `	if( pRes == 0 \|\| pRes->pOptions == 0 \|\| pWrapper == 0 \|\| pName == 0 \|\| pValue == 0 ){` |
|    ! 0 | 4200 | `		return -1;` |
|      - | 4201 | `	}` |
|      - | 4202 | `	/* Every insertion below can reserve a memory object, which GROWS (and` |
|      - | 4203 | `	 * therefore moves) pVm->aMemObj — and all three arguments may point into` |
|      - | 4204 | `	 * it. Snapshot the structs first: a shallow copy is a safe insertion` |
|      - | 4205 | `	 * source, since the referent and the heap-resident blob survive the move. */` |
|     70 | 4206 | `	sKey = *pWrapper; pWrapper = &sKey;` |
|     70 | 4207 | `	sName = *pName;   pName = &sName;` |
|     70 | 4208 | `	sVal = *pValue;   pValue = &sVal;` |
|     70 | 4209 | `	pSub = StreamCtxFetch(pRes->pOptions,pWrapper);` |
|     70 | 4210 | `	if( pSub == 0 \|\| (pSub->iFlags & MEMOBJ_HASHMAP) == 0 ){` |
|     56 | 4211 | `		ph7_value *pFresh = ph7_new_array(pRes->pVm);` |
|     56 | 4212 | `		if( pFresh == 0 ){` |
|    ! 0 | 4213 | `			return -1;` |
|      - | 4214 | `		}` |
|     56 | 4215 | `		if( PH7_HashmapInsert((ph7_hashmap *)pRes->pOptions->x.pOther,pWrapper,pFresh) != SXRET_OK ){` |
|    ! 0 | 4216 | `			ph7_release_value(pRes->pVm,pFresh);` |
|    ! 0 | 4217 | `			return -1;` |
|      - | 4218 | `		}` |
|     56 | 4219 | `		ph7_release_value(pRes->pVm,pFresh);` |
|     56 | 4220 | `		pSub = StreamCtxFetch(pRes->pOptions,pWrapper);` |
|     56 | 4221 | `		if( pSub == 0 \|\| (pSub->iFlags & MEMOBJ_HASHMAP) == 0 ){` |
|    ! 0 | 4222 | `			return -1;` |
|      - | 4223 | `		}` |
|     26 | 4224 | `	}` |
|     70 | 4225 | `	pMap = PH7_HashmapCowSeparate(pRes->pVm,pSub);` |
|     70 | 4226 | `	if( pMap == 0 ){` |
|    ! 0 | 4227 | `		return -1;` |
|      - | 4228 | `	}` |
|     70 | 4229 | `	return PH7_HashmapInsert(pMap,pName,pValue) == SXRET_OK ? 0 : -1;` |
|     37 | 4230 | `}` |
|      - | 4231 | `/*` |
|      - | 4232 | ` * One wrapper option by name, or 0 when the context does not carry it. This is` |
|      - | 4233 | ` * the read side every consumer (the socket transports) asks through.` |
|      - | 4234 | ` */` |
|    264 | 4235 | `PH7_PRIVATE ph7_value * PH7_StreamCtxOption(phl_stream_ctx *pRes,const char *zWrapper,const char *zOption)` |
|      4 | 4236 | `{` |
|      - | 4237 | `	ph7_value *pSub;` |
|    268 | 4238 | `	if( pRes == 0 \|\| pRes->pOptions == 0 ){` |
|    ! 0 | 4239 | `		return 0;` |
|      - | 4240 | `	}` |
|    268 | 4241 | `	pSub = ph7_array_fetch(pRes->pOptions,zWrapper,-1);` |
|    268 | 4242 | `	if( pSub == 0 \|\| (pSub->iFlags & MEMOBJ_HASHMAP) == 0 ){` |
|    188 | 4243 | `		return 0;` |
|      - | 4244 | `	}` |
|     82 | 4245 | `	return ph7_array_fetch(pSub,zOption,-1);` |
|    136 | 4246 | `}` |
|      - | 4247 | `/*` |
|      - | 4248 | ` * php's parse_context_options: every entry must be wrappername => array, and a` |
|      - | 4249 | ` * non-array value — or an INTEGER key, which has no wrapper name at all — is` |
|      - | 4250 | ` * the ValueError below. An integer key one level DOWN has no option name, and` |
|      - | 4251 | ` * php drops that entry in silence rather than refusing the call.` |
|      - | 4252 | ` * Returns 0, or -1 once the exception has been raised.` |
|      - | 4253 | ` */` |
|     62 | 4254 | `static int StreamCtxParseOptions(ph7_context *pCtx,phl_stream_ctx *pRes,ph7_value *pOptions)` |
|      4 | 4255 | `{` |
|      - | 4256 | `	ph7_hashmap *pMap;` |
|      - | 4257 | `	ph7_hashmap_node *pEntry;` |
|     66 | 4258 | `	if( pOptions == 0 \|\| (pOptions->iFlags & MEMOBJ_HASHMAP) == 0 ){` |
|    ! 0 | 4259 | `		return 0;` |
|      - | 4260 | `	}` |
|     66 | 4261 | `	pMap = (ph7_hashmap *)pOptions->x.pOther;` |
|     66 | 4262 | `	pMap->pCur = pMap->pFirst;` |
|    118 | 4263 | `	while( (pEntry = PH7_HashmapGetNextEntry(pMap)) != 0 ){` |
|      - | 4264 | `		ph7_value sKey;` |
|      - | 4265 | `		ph7_value *pVal;` |
|      - | 4266 | `		int bBad;` |
|     60 | 4267 | `		PH7_MemObjInit(pRes->pVm,&sKey);` |
|     60 | 4268 | `		PH7_HashmapExtractNodeKey(pEntry,&sKey);` |
|     60 | 4269 | `		pVal = HashmapExtractNodeValue(pEntry);` |
|     86 | 4270 | `		bBad = ( (sKey.iFlags & MEMOBJ_STRING) == 0 \|\| pVal == 0` |
|     83 | 4271 | `		      \|\| (pVal->iFlags & MEMOBJ_HASHMAP) == 0 );` |
|     60 | 4272 | `		if( bBad ){` |
|      5 | 4273 | `			PH7_MemObjRelease(&sKey);` |
|      5 | 4274 | `			PH7_VmThrowException(pCtx,"ValueError",` |
|      - | 4275 | `				"Options should have the form [\"wrappername\"][\"optionname\"] = $value");` |
|      5 | 4276 | `			return -1;` |
|      - | 4277 | `		}` |
|      - | 4278 | `		{` |
|     56 | 4279 | `			ph7_hashmap *pSub = (ph7_hashmap *)pVal->x.pOther;` |
|      - | 4280 | `			ph7_hashmap_node *pOpt;` |
|     56 | 4281 | `			pSub->pCur = pSub->pFirst;` |
|    116 | 4282 | `			while( (pOpt = PH7_HashmapGetNextEntry(pSub)) != 0 ){` |
|      - | 4283 | `				ph7_value sName;` |
|      - | 4284 | `				ph7_value *pOptVal;` |
|     64 | 4285 | `				PH7_MemObjInit(pRes->pVm,&sName);` |
|     64 | 4286 | `				PH7_HashmapExtractNodeKey(pOpt,&sName);` |
|     64 | 4287 | `				pOptVal = HashmapExtractNodeValue(pOpt);` |
|     64 | 4288 | `				if( (sName.iFlags & MEMOBJ_STRING) && pOptVal ){` |
|     62 | 4289 | `					StreamCtxSetOption(pRes,&sKey,&sName,pOptVal);` |
|     29 | 4290 | `				}` |
|     64 | 4291 | `				PH7_MemObjRelease(&sName);` |
|      4 | 4292 | `			}` |
|      - | 4293 | `		}` |
|     56 | 4294 | `		PH7_MemObjRelease(&sKey);` |
|      4 | 4295 | `	}` |
|     62 | 4296 | `	return 0;` |
|     35 | 4297 | `}` |
|      - | 4298 | `/*` |
|      - | 4299 | `` * php's parse_context_params: only `notification` and `options` are read, and`` |
|      - | 4300 | ` * anything else in the array is ignored rather than refused. The notification` |
|      - | 4301 | ` * must be callable — php reports the same "must be an array with valid` |
|      - | 4302 | ` * callbacks as values" TypeError the callback taxonomy produces, naming` |
|      - | 4303 | ` * argument #1 whichever function was called.` |
|      - | 4304 | ` * Returns 0, or -1 once a diagnostic has been raised.` |
|      - | 4305 | ` */` |
|     10 | 4306 | `static int StreamCtxParseParams(ph7_context *pCtx,phl_stream_ctx *pRes,ph7_value *pParams,const char *zArgName)` |
|      1 | 4307 | `{` |
|      - | 4308 | `	ph7_value *pVal;` |
|     11 | 4309 | `	if( pParams == 0 \|\| (pParams->iFlags & MEMOBJ_HASHMAP) == 0 ){` |
|    ! 0 | 4310 | `		return 0;` |
|      - | 4311 | `	}` |
|     11 | 4312 | `	pVal = ph7_array_fetch(pParams,"notification",-1);` |
|     11 | 4313 | `	if( pVal ){` |
|      - | 4314 | `		char zBuf[128];` |
|      7 | 4315 | `		const char *zReason = PH7_VmCallableReason(pCtx->pVm,pVal,zBuf,(int)sizeof(zBuf));` |
|      7 | 4316 | `		if( zReason ){` |
|      5 | 4317 | `			return PH7_VmThrowException(pCtx,"TypeError",` |
|      - | 4318 | `				"%s(): Argument #1 (%s) must be an array with valid callbacks as values, %s",` |
|      3 | 4319 | `				ph7_function_name(pCtx),zArgName,zReason) == PH7_OK ? -1 : -1;` |
|      - | 4320 | `		}` |
|      5 | 4321 | `		if( pRes->pNotify == 0 ){` |
|      5 | 4322 | `			pRes->pNotify = ph7_new_scalar(pRes->pVm);` |
|      2 | 4323 | `		}` |
|      5 | 4324 | `		if( pRes->pNotify ){` |
|      5 | 4325 | `			PH7_MemObjStore(pVal,pRes->pNotify);` |
|      2 | 4326 | `		}` |
|      2 | 4327 | `	}` |
|      9 | 4328 | `	pVal = ph7_array_fetch(pParams,"options",-1);` |
|      9 | 4329 | `	if( pVal ){` |
|      5 | 4330 | `		if( (pVal->iFlags & MEMOBJ_HASHMAP) == 0 ){` |
|      - | 4331 | `			/* php's own wording for a params entry it cannot use. */` |
|    ! 0 | 4332 | `			return PH7_VmThrowException(pCtx,"TypeError",` |
|    ! 0 | 4333 | `				"Invalid stream/context parameter") == PH7_OK ? -1 : -1;` |
|      - | 4334 | `		}` |
|      5 | 4335 | `		if( StreamCtxParseOptions(pCtx,pRes,pVal) != 0 ){` |
|    ! 0 | 4336 | `			return -1;` |
|      - | 4337 | `		}` |
|      2 | 4338 | `	}` |
|      9 | 4339 | `	return 0;` |
|      6 | 4340 | `}` |
|      - | 4341 | `/*` |
|      - | 4342 | `` * Resolve the `$stream_or_context` first argument every accessor takes: a`` |
|      - | 4343 | ` * context resource answers itself, and a STREAM answers the context it` |
|      - | 4344 | ` * carries — created on demand for the setters, the way php's does, since a` |
|      - | 4345 | ` * stream opened without one still accepts stream_context_set_option().` |
|      - | 4346 | ` * Raises php's TypeError and returns 0 for anything else.` |
|      - | 4347 | ` */` |
|     70 | 4348 | `static phl_stream_ctx * StreamCtxArg(ph7_context *pCtx,ph7_value *pVal,int bCreate,` |
|      - | 4349 | `	const char *zArgName,int *pbThrew)` |
|      3 | 4350 | `{` |
|      - | 4351 | `	phl_stream_ctx *pRes;` |
|      - | 4352 | `	io_private *pDev;` |
|     73 | 4353 | `	*pbThrew = 1;` |
|     73 | 4354 | `	if( !ph7_value_is_resource(pVal) ){` |
|      4 | 4355 | `		PH7_VmThrowException(pCtx,"TypeError",` |
|      - | 4356 | `			"%s(): Argument #1 (%s) must be of type resource, %s given",` |
|      1 | 4357 | `			ph7_function_name(pCtx),zArgName,ph7_type_name(pVal));` |
|      3 | 4358 | `		return 0;` |
|      - | 4359 | `	}` |
|     71 | 4360 | `	pRes = PH7_StreamCtxFromValue(pVal);` |
|     71 | 4361 | `	if( pRes ){` |
|     57 | 4362 | `		*pbThrew = 0;` |
|     57 | 4363 | `		return pRes;` |
|      - | 4364 | `	}` |
|     16 | 4365 | `	pDev = (io_private *)ph7_value_to_resource(pVal);` |
|     16 | 4366 | `	if( IO_PRIVATE_INVALID(pDev) ){` |
|      - | 4367 | `		/* A closed handle, a process handle, anything that is neither: php` |
|      - | 4368 | `		 * refuses the call rather than answering an empty option set. */` |
|      4 | 4369 | `		PH7_VmThrowException(pCtx,"TypeError",` |
|      - | 4370 | `			"%s(): Argument #1 (%s) must be a valid stream/context",` |
|      1 | 4371 | `			ph7_function_name(pCtx),zArgName);` |
|      3 | 4372 | `		return 0;` |
|      - | 4373 | `	}` |
|     14 | 4374 | `	*pbThrew = 0;` |
|     14 | 4375 | `	if( pDev->pCtxRes == 0 && bCreate ){` |
|      3 | 4376 | `		pDev->pCtxRes = (void *)StreamCtxNew(pCtx->pVm);` |
|      1 | 4377 | `	}` |
|     14 | 4378 | `	return (phl_stream_ctx *)pDev->pCtxRes;` |
|     38 | 4379 | `}` |
|      - | 4380 | `/*` |
|      - | 4381 | `` * The `$context` argument sixteen rows of aBuiltinSig[] declare and no C body`` |
|      - | 4382 | `` * used to read. php's parameter is `?resource $context = null` and its rules`` |
|      - | 4383 | ` * are: a resource that is NOT a stream-context is refused outright, anything` |
|      - | 4384 | ` * else non-null is the ordinary type refusal, and NULL means the DEFAULT` |
|      - | 4385 | ` * context — which php creates on demand, so an opener never runs without one.` |
|      - | 4386 | ` *` |
|      - | 4387 | ` * bNoDefault is FILE_NO_DEFAULT_CONTEXT, the flag file()/file_get_contents()/` |
|      - | 4388 | ` * file_put_contents() carry to mean exactly "and do not fall back to it".` |
|      - | 4389 | ` * Returns 0 with *pbThrew set once a diagnostic has been raised.` |
|      - | 4390 | ` */` |
|  74678 | 4391 | `PH7_PRIVATE phl_stream_ctx * PH7_StreamCtxFromArg(ph7_context *pCtx,int nArg,ph7_value **apArg,` |
|      - | 4392 | `	int iArg,const char *zArgName,int bNoDefault,int *pbThrew)` |
|      5 | 4393 | `{` |
|      - | 4394 | `	phl_stream_ctx *pRes;` |
|  74683 | 4395 | `	*pbThrew = 0;` |
|  74683 | 4396 | `	if( iArg < nArg && apArg[iArg] && !ph7_value_is_null(apArg[iArg]) ){` |
|     82 | 4397 | `		if( !ph7_value_is_resource(apArg[iArg]) ){` |
|     11 | 4398 | `			*pbThrew = 1;` |
|     16 | 4399 | `			PH7_VmThrowException(pCtx,"TypeError",` |
|      - | 4400 | `				"%s(): Argument #%d (%s) must be of type resource or null, %s given",` |
|     10 | 4401 | `				ph7_function_name(pCtx),iArg + 1,zArgName,ph7_type_name(apArg[iArg]));` |
|     11 | 4402 | `			return 0;` |
|      - | 4403 | `		}` |
|     72 | 4404 | `		pRes = PH7_StreamCtxFromValue(apArg[iArg]);` |
|     72 | 4405 | `		if( pRes == 0 ){` |
|      - | 4406 | `			/* php names the RESOURCE it wanted rather than the argument here. */` |
|     34 | 4407 | `			*pbThrew = 1;` |
|     50 | 4408 | `			PH7_VmThrowException(pCtx,"TypeError",` |
|      - | 4409 | `				"%s(): supplied resource is not a valid Stream-Context resource",` |
|     16 | 4410 | `				ph7_function_name(pCtx));` |
|     34 | 4411 | `			return 0;` |
|      - | 4412 | `		}` |
|     39 | 4413 | `		return pRes;` |
|      - | 4414 | `	}` |
|  74605 | 4415 | `	return bNoDefault ? 0 : PH7_StreamCtxDefault(pCtx->pVm);` |
|  37344 | 4416 | `}` |
|      - | 4417 | `/*` |
|      - | 4418 | ` * Arm the context the NEXT open is to run under. PH7_StreamOpenHandle consumes` |
|      - | 4419 | ` * and clears it, so the slot describes exactly one open and a caller that never` |
|      - | 4420 | ` * set it finds nothing armed.` |
|      - | 4421 | ` */` |
|  28030 | 4422 | `PH7_PRIVATE void PH7_StreamCtxArm(ph7_vm *pVm,phl_stream_ctx *pRes)` |
|      5 | 4423 | `{` |
|  28035 | 4424 | `	if( pVm ){` |
|  28035 | 4425 | `		pVm->pOpenCtx = (void *)pRes;` |
|  14015 | 4426 | `	}` |
|  28035 | 4427 | `}` |
|      - | 4428 | `/*` |
|      - | 4429 | ` * resource stream_context_create(?array $options = null, ?array $params = null)` |
|      - | 4430 | ` */` |
|     58 | 4431 | `PH7_PRIVATE int PH7_builtin_stream_context_create(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      4 | 4432 | `{` |
|     62 | 4433 | `	phl_stream_ctx *pRes = StreamCtxNew(pCtx->pVm);` |
|     62 | 4434 | `	if( pRes == 0 ){` |
|    ! 0 | 4435 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 4436 | `		return PH7_OK;` |
|      - | 4437 | `	}` |
|     62 | 4438 | `	if( nArg > 0 && ph7_value_is_array(apArg[0]) ){` |
|     51 | 4439 | `		if( StreamCtxParseOptions(pCtx,pRes,apArg[0]) != 0 ){` |
|      5 | 4440 | `			return PH7_OK;` |
|      - | 4441 | `		}` |
|     22 | 4442 | `	}` |
|     58 | 4443 | `	if( nArg > 1 && ph7_value_is_array(apArg[1]) ){` |
|      - | 4444 | ``		/* php names argument #1 ($options) even for a bad `notification` that`` |
|      - | 4445 | `		 * arrived through $params — the error is raised against a hardcoded` |
|      - | 4446 | `		 * position, and a test that asserts the message would see it. */` |
|      9 | 4447 | `		if( StreamCtxParseParams(pCtx,pRes,apArg[1],"$options") != 0 ){` |
|      3 | 4448 | `			return PH7_OK;` |
|      - | 4449 | `		}` |
|      3 | 4450 | `	}` |
|     56 | 4451 | `	ph7_result_resource(pCtx,pRes);` |
|     56 | 4452 | `	return PH7_OK;` |
|     33 | 4453 | `}` |
|      - | 4454 | `/*` |
|      - | 4455 | ` * array stream_context_get_options(resource $stream_or_context)` |
|      - | 4456 | ` */` |
|     48 | 4457 | `PH7_PRIVATE int PH7_builtin_stream_context_get_options(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      3 | 4458 | `{` |
|      - | 4459 | `	phl_stream_ctx *pRes;` |
|      - | 4460 | `	int bThrew;` |
|     51 | 4461 | `	if( nArg < 1 ){` |
|    ! 0 | 4462 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 4463 | `		return PH7_OK;` |
|      - | 4464 | `	}` |
|      - | 4465 | `	/* A live stream that was never given a context answers the EMPTY option set` |
|      - | 4466 | `	 * rather than refusing the call, so nothing is created here. */` |
|     51 | 4467 | `	pRes = StreamCtxArg(pCtx,apArg[0],FALSE,"$stream_or_context",&bThrew);` |
|     51 | 4468 | `	if( bThrew ){` |
|      5 | 4469 | `		return PH7_OK;` |
|      - | 4470 | `	}` |
|     47 | 4471 | `	if( pRes == 0 ){` |
|      8 | 4472 | `		ph7_value *pArr = ph7_context_new_array(pCtx);` |
|      8 | 4473 | `		if( pArr == 0 ){` |
|    ! 0 | 4474 | `			ph7_result_bool(pCtx,0);` |
|    ! 0 | 4475 | `			return PH7_OK;` |
|      - | 4476 | `		}` |
|      8 | 4477 | `		ph7_result_value(pCtx,pArr);` |
|      8 | 4478 | `		return PH7_OK;` |
|      - | 4479 | `	}` |
|     41 | 4480 | `	ph7_result_value(pCtx,pRes->pOptions);` |
|     41 | 4481 | `	return PH7_OK;` |
|     27 | 4482 | `}` |
|      - | 4483 | `/*` |
|      - | 4484 | ` * bool stream_context_set_option(resource $context, string $wrapper, string $option_name, mixed $value)` |
|      - | 4485 | ` *` |
|      - | 4486 | ` * php also accepts the two-argument (context, options-array) spelling and` |
|      - | 4487 | ` * DEPRECATES it in 8.3 — §10 refuses what php deprecates, so an array in` |
|      - | 4488 | ` * argument #2 is the ordinary string TypeError here and the whole-array form` |
|      - | 4489 | ` * is spelled stream_context_set_options().` |
|      - | 4490 | ` */` |
|      8 | 4491 | `PH7_PRIVATE int PH7_builtin_stream_context_set_option(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      2 | 4492 | `{` |
|      - | 4493 | `	phl_stream_ctx *pRes;` |
|      - | 4494 | `	int bThrew;` |
|     10 | 4495 | `	if( nArg < 4 ){` |
|    ! 0 | 4496 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 4497 | `		return PH7_OK;` |
|      - | 4498 | `	}` |
|     10 | 4499 | `	pRes = StreamCtxArg(pCtx,apArg[0],TRUE,"$context",&bThrew);` |
|     10 | 4500 | `	if( pRes == 0 ){` |
|    ! 0 | 4501 | `		return PH7_OK;` |
|      - | 4502 | `	}` |
|     10 | 4503 | `	ph7_result_bool(pCtx,StreamCtxSetOption(pRes,apArg[1],apArg[2],apArg[3]) == 0);` |
|     10 | 4504 | `	return PH7_OK;` |
|      6 | 4505 | `}` |
|      - | 4506 | `/*` |
|      - | 4507 | ` * bool stream_context_set_options(resource $context, array $options)` |
|      - | 4508 | ` */` |
|      4 | 4509 | `PH7_PRIVATE int PH7_builtin_stream_context_set_options(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      2 | 4510 | `{` |
|      - | 4511 | `	phl_stream_ctx *pRes;` |
|      - | 4512 | `	int bThrew;` |
|      6 | 4513 | `	if( nArg < 2 ){` |
|    ! 0 | 4514 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 4515 | `		return PH7_OK;` |
|      - | 4516 | `	}` |
|      6 | 4517 | `	pRes = StreamCtxArg(pCtx,apArg[0],TRUE,"$context",&bThrew);` |
|      6 | 4518 | `	if( pRes == 0 ){` |
|    ! 0 | 4519 | `		return PH7_OK;` |
|      - | 4520 | `	}` |
|      6 | 4521 | `	if( StreamCtxParseOptions(pCtx,pRes,apArg[1]) != 0 ){` |
|    ! 0 | 4522 | `		return PH7_OK;` |
|      - | 4523 | `	}` |
|      6 | 4524 | `	ph7_result_bool(pCtx,1);` |
|      6 | 4525 | `	return PH7_OK;` |
|      4 | 4526 | `}` |
|      - | 4527 | `/*` |
|      - | 4528 | ` * array stream_context_get_params(resource $stream_or_context)` |
|      - | 4529 | `` *  php answers `notification` (only when one is set) and `options`, in that`` |
|      - | 4530 | ` *  order.` |
|      - | 4531 | ` */` |
|      8 | 4532 | `PH7_PRIVATE int PH7_builtin_stream_context_get_params(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 4533 | `{` |
|      - | 4534 | `	phl_stream_ctx *pRes;` |
|      - | 4535 | `	ph7_value *pArr;` |
|      - | 4536 | `	int bThrew;` |
|      9 | 4537 | `	if( nArg < 1 ){` |
|    ! 0 | 4538 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 4539 | `		return PH7_OK;` |
|      - | 4540 | `	}` |
|      9 | 4541 | `	pRes = StreamCtxArg(pCtx,apArg[0],TRUE,"$stream_or_context",&bThrew);` |
|      9 | 4542 | `	if( pRes == 0 ){` |
|    ! 0 | 4543 | `		return PH7_OK;` |
|      - | 4544 | `	}` |
|      9 | 4545 | `	pArr = ph7_context_new_array(pCtx);` |
|      9 | 4546 | `	if( pArr == 0 ){` |
|    ! 0 | 4547 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 4548 | `		return PH7_OK;` |
|      - | 4549 | `	}` |
|      9 | 4550 | `	if( pRes->pNotify ){` |
|      5 | 4551 | `		ph7_array_add_strkey_elem(pArr,"notification",pRes->pNotify);` |
|      2 | 4552 | `	}` |
|      9 | 4553 | `	ph7_array_add_strkey_elem(pArr,"options",pRes->pOptions);` |
|      9 | 4554 | `	ph7_result_value(pCtx,pArr);` |
|      9 | 4555 | `	return PH7_OK;` |
|      5 | 4556 | `}` |
|      - | 4557 | `/*` |
|      - | 4558 | ` * bool stream_context_set_params(resource $context, array $params)` |
|      - | 4559 | ` */` |
|      2 | 4560 | `PH7_PRIVATE int PH7_builtin_stream_context_set_params(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 4561 | `{` |
|      - | 4562 | `	phl_stream_ctx *pRes;` |
|      - | 4563 | `	int bThrew;` |
|      3 | 4564 | `	if( nArg < 2 ){` |
|    ! 0 | 4565 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 4566 | `		return PH7_OK;` |
|      - | 4567 | `	}` |
|      3 | 4568 | `	pRes = StreamCtxArg(pCtx,apArg[0],TRUE,"$context",&bThrew);` |
|      3 | 4569 | `	if( pRes == 0 ){` |
|    ! 0 | 4570 | `		return PH7_OK;` |
|      - | 4571 | `	}` |
|      3 | 4572 | `	if( StreamCtxParseParams(pCtx,pRes,apArg[1],"$context") != 0 ){` |
|    ! 0 | 4573 | `		return PH7_OK;` |
|      - | 4574 | `	}` |
|      3 | 4575 | `	ph7_result_bool(pCtx,1);` |
|      3 | 4576 | `	return PH7_OK;` |
|      2 | 4577 | `}` |
|      - | 4578 | `/*` |
|      - | 4579 | ` * resource stream_context_get_default(?array $options = null)` |
|      - | 4580 | ` * resource stream_context_set_default(array $options)` |
|      - | 4581 | ` *  Both answer the ONE default context and both MERGE their options into it —` |
|      - | 4582 | ` *  set_default is not a replacement, which is why a second call adds to what` |
|      - | 4583 | ` *  the first left.` |
|      - | 4584 | ` */` |
|     14 | 4585 | `static int StreamCtxDefaultCommon(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      2 | 4586 | `{` |
|     16 | 4587 | `	phl_stream_ctx *pRes = PH7_StreamCtxDefault(pCtx->pVm);` |
|     16 | 4588 | `	if( pRes == 0 ){` |
|    ! 0 | 4589 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 4590 | `		return PH7_OK;` |
|      - | 4591 | `	}` |
|     16 | 4592 | `	if( nArg > 0 && ph7_value_is_array(apArg[0]) ){` |
|      8 | 4593 | `		if( StreamCtxParseOptions(pCtx,pRes,apArg[0]) != 0 ){` |
|    ! 0 | 4594 | `			return PH7_OK;` |
|      - | 4595 | `		}` |
|      3 | 4596 | `	}` |
|     16 | 4597 | `	ph7_result_resource(pCtx,pRes);` |
|     16 | 4598 | `	return PH7_OK;` |
|      9 | 4599 | `}` |
|     10 | 4600 | `PH7_PRIVATE int PH7_builtin_stream_context_get_default(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 4601 | `{` |
|     11 | 4602 | `	return StreamCtxDefaultCommon(pCtx,nArg,apArg);` |
|      1 | 4603 | `}` |
|      4 | 4604 | `PH7_PRIVATE int PH7_builtin_stream_context_set_default(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      2 | 4605 | `{` |
|      6 | 4606 | `	return StreamCtxDefaultCommon(pCtx,nArg,apArg);` |
|      2 | 4607 | `}` |
|      - | 4608 | `/*` |
|      - | 4609 | ` * tcp:// socket stream (fsockopen / stream_socket_client). The handle is a` |
|      - | 4610 | ` * small struct carrying the OS socket plus an EOF latch, so feof() works.` |
|      - | 4611 | ` */` |
|      - | 4612 | `#ifdef PH7_ENABLE_NET` |
|     83 | 4613 | `static ph7_int64 SockStreamData_Read(void *pHandle,void *pBuffer,ph7_int64 nRead)` |
|      3 | 4614 | `{` |
|     86 | 4615 | `	sock_private *pSock = (sock_private *)pHandle;` |
|      - | 4616 | `	int n;` |
|     86 | 4617 | `	if( pSock == 0 \|\| pSock->sock == PH7_NET_INVALID_SOCKET ){` |
|      - | 4618 | `		/* A server asked for neither BIND nor LISTEN has no socket at all, and` |
|      - | 4619 | `		 * php answers false for a read on it — the shape an ERROR takes. */` |
|      6 | 4620 | `		return -1;` |
|      - | 4621 | `	}` |
|     82 | 4622 | `	if( pSock->bEof ){` |
|    ! 0 | 4623 | `		return 0;` |
|      - | 4624 | `	}` |
|     82 | 4625 | `	n = PH7_NetRecv(pSock->sock,pBuffer,(int)nRead,0);` |
|     82 | 4626 | `	if( n == 0 ){` |
|      - | 4627 | `		/* The peer closed: THIS is the end of the stream. */` |
|     27 | 4628 | `		pSock->bEof = 1;` |
|     27 | 4629 | `		return 0;` |
|      - | 4630 | `	}` |
|     56 | 4631 | `	if( n < 0 ){` |
|      - | 4632 | `		/* An error, and since stream_set_blocking()/stream_set_timeout() exist` |
|      - | 4633 | `		 * the ordinary one is EAGAIN — nothing had arrived YET. Latching EOF` |
|      - | 4634 | `		 * here (as this did for every n <= 0, safe only while every socket was` |
|      - | 4635 | `		 * blocking and untimed) made the first empty read close the connection` |
|      - | 4636 | `		 * for good and threw away everything the peer sent afterwards.` |
|      - | 4637 | `		 *` |
|      - | 4638 | `		 * The reader above tells "nothing yet" from "broken" by ERRNO, which a` |
|      - | 4639 | ``		 * Winsock call never touches: without this the `""` a non-blocking read`` |
|      - | 4640 | ``		 * answers and the `timed_out` an expired one reports were both lost on`` |
|      - | 4641 | `		 * Windows, and every such read came back as a plain failure. */` |
|      7 | 4642 | `		errno = PH7_NetWouldBlock() ? EAGAIN : (errno != 0 ? errno : EIO);` |
|      7 | 4643 | `		return -1;` |
|      - | 4644 | `	}` |
|     50 | 4645 | `	return (ph7_int64)n;` |
|     43 | 4646 | `}` |
|     73 | 4647 | `static ph7_int64 SockStreamData_Write(void *pHandle,const void *pBuf,ph7_int64 nWrite)` |
|      3 | 4648 | `{` |
|     76 | 4649 | `	sock_private *pSock = (sock_private *)pHandle;` |
|     76 | 4650 | `	const char *zBuf = (const char *)pBuf;` |
|     76 | 4651 | `	ph7_int64 nSent = 0;` |
|     76 | 4652 | `	if( pSock == 0 ){` |
|    ! 0 | 4653 | `		return -1;` |
|      - | 4654 | `	}` |
|     76 | 4655 | `	if( pSock->sock == PH7_NET_INVALID_SOCKET ){` |
|      - | 4656 | `		/* Nothing to send on, and php answers 0 rather than false for it. */` |
|      6 | 4657 | `		return 0;` |
|      - | 4658 | `	}` |
|      - | 4659 | `	/* php answers the number of bytes it MOVED. This used to hand back` |
|      - | 4660 | `	 * PH7_NetSendAll()'s STATUS — which is 0 on success — so every successful` |
|      - | 4661 | ``	 * `fwrite($sock,$s)` answered 0 bytes written: the `=== strlen($s)` check`` |
|      - | 4662 | `	 * failed on a write that worked, a partial-write retry loop never advanced,` |
|      - | 4663 | `	 * and stream_copy_to_stream() stopped after its first chunk. */` |
|    137 | 4664 | `	while( nSent < nWrite ){` |
|     74 | 4665 | `		int n = PH7_NetSend(pSock->sock,&zBuf[nSent],(int)(nWrite - nSent),0);` |
|     74 | 4666 | `		if( n > 0 ){` |
|     68 | 4667 | `			nSent += n;` |
|     68 | 4668 | `			continue;` |
|      - | 4669 | `		}` |
|      - | 4670 | `		/* Nothing more can go right now. On a non-blocking or timed-out handle` |
|      - | 4671 | `		 * that is php's 0 (or the partial count), and only a write that moved` |
|      - | 4672 | `		 * NO bytes at all for a real error is php's false — which is why the` |
|      - | 4673 | `		 * count is answered here rather than the status. */` |
|      7 | 4674 | `		if( PH7_NetWouldBlock() ){` |
|      5 | 4675 | `			return nSent;` |
|      - | 4676 | `		}` |
|      3 | 4677 | `		pSock->iLastErr = PH7_NetLastError();` |
|      3 | 4678 | `		return nSent > 0 ? nSent : -1;` |
|    ! 0 | 4679 | `	}` |
|     66 | 4680 | `	return nSent;` |
|     36 | 4681 | `}` |
|    114 | 4682 | `static void SockStreamData_Close(void *pHandle)` |
|      3 | 4683 | `{` |
|    117 | 4684 | `	sock_private *pSock = (sock_private *)pHandle;` |
|    117 | 4685 | `	if( pSock == 0 ){` |
|    ! 0 | 4686 | `		return;` |
|      - | 4687 | `	}` |
|    117 | 4688 | `	PH7_NetClose(pSock->sock);` |
|    117 | 4689 | `	SyMemBackendFree(&pSock->pVm->sAllocator,pSock);` |
|     60 | 4690 | `}` |
|      - | 4691 | `/* xOpen for "host:port" (the scheme is already stripped by the device lookup) */` |
|    ! 0 | 4692 | `static int SockStreamData_Open(const char *zName,int iMode,ph7_value *pResource,void ** ppHandle)` |
|    ! 0 | 4693 | `{` |
|      - | 4694 | `	sock_private *pSock;` |
|      - | 4695 | `	ph7_socket sock;` |
|      - | 4696 | `	char zHost[256];` |
|      - | 4697 | `	const char *zColon;` |
|    ! 0 | 4698 | `	int iPort = 0,iErrno = 0;` |
|    ! 0 | 4699 | `	const char *zErr = "";` |
|    ! 0 | 4700 | `	ph7_vm *pVm = pResource ? pResource->pVm : 0;` |
|    ! 0 | 4701 | `	SXUNUSED(iMode);` |
|    ! 0 | 4702 | `	if( pVm == 0 ){` |
|    ! 0 | 4703 | `		return -1;` |
|      - | 4704 | `	}` |
|    ! 0 | 4705 | `	zColon = SyStrlen(zName) ? &zName[SyStrlen(zName)-1] : zName;` |
|    ! 0 | 4706 | `	while( zColon > zName && zColon[0] != ':' ){` |
|    ! 0 | 4707 | `		zColon--;` |
|    ! 0 | 4708 | `	}` |
|    ! 0 | 4709 | `	if( zColon <= zName \|\| zColon[0] != ':' ){` |
|    ! 0 | 4710 | `		return -1;` |
|      - | 4711 | `	}` |
|      - | 4712 | `	{` |
|    ! 0 | 4713 | `		sxu32 n = (sxu32)(zColon - zName);` |
|    ! 0 | 4714 | `		if( n >= sizeof(zHost) ){` |
|    ! 0 | 4715 | `			n = sizeof(zHost) - 1;` |
|    ! 0 | 4716 | `		}` |
|    ! 0 | 4717 | `		SyMemcpy(zName,zHost,n);` |
|    ! 0 | 4718 | `		zHost[n] = 0;` |
|      - | 4719 | `	}` |
|      - | 4720 | `	{` |
|    ! 0 | 4721 | `		sxi32 iTmp = 0;` |
|    ! 0 | 4722 | `		SyStrToInt32(&zColon[1],(sxu32)SyStrlen(&zColon[1]),(void *)&iTmp,0);` |
|    ! 0 | 4723 | `		iPort = (int)iTmp;` |
|      - | 4724 | `	}` |
|    ! 0 | 4725 | `	sock = PH7_NetConnect(zHost,iPort,0,0,&iErrno,&zErr);` |
|    ! 0 | 4726 | `	if( sock == PH7_NET_INVALID_SOCKET ){` |
|    ! 0 | 4727 | `		return -1;` |
|      - | 4728 | `	}` |
|    ! 0 | 4729 | `	pSock = (sock_private *)SyMemBackendAlloc(&pVm->sAllocator,sizeof(sock_private));` |
|    ! 0 | 4730 | `	if( pSock == 0 ){` |
|    ! 0 | 4731 | `		PH7_NetClose(sock);` |
|    ! 0 | 4732 | `		return -1;` |
|      - | 4733 | `	}` |
|    ! 0 | 4734 | `	pSock->pVm = pVm;` |
|    ! 0 | 4735 | `	pSock->sock = sock;` |
|    ! 0 | 4736 | `	pSock->bEof = 0;` |
|    ! 0 | 4737 | `	pSock->iLastErr = 0;` |
|    ! 0 | 4738 | `	pSock->bGeneric = 0;` |
|    ! 0 | 4739 | `	*ppHandle = (void *)pSock;` |
|    ! 0 | 4740 | `	return PH7_OK;` |
|    ! 0 | 4741 | `}` |
|      - | 4742 | `/* php's own listen backlog for a stream server. */` |
|      - | 4743 | `#define SOCK_LISTEN_BACKLOG 128` |
|      - | 4744 | `/*` |
|      - | 4745 | `` * php's `fwrite(): Send of 4 bytes failed with errno=32 Broken pipe` — the`` |
|      - | 4746 | ` * NOTICE its socket ops raise for a send that failed, which is the only` |
|      - | 4747 | ` * diagnostic a write to a departed peer produces (the return value is the same` |
|      - | 4748 | ` * false a closed handle answers). The PLAIN-FILE device has the same notice` |
|      - | 4749 | `` * worded `Write of`, which is what a write to a handle opened read-only`` |
|      - | 4750 | ` * produces: php answers false AND says why, where this engine only answered` |
|      - | 4751 | ` * false. Silent for every other device — nothing else here has an OS error of` |
|      - | 4752 | ` * its own to report, and php's notice lives in those two stream ops alone.` |
|      - | 4753 | ` */` |
|     34 | 4754 | `static void SockReportWriteFailure(ph7_context *pCtx,io_private *pDev,int nLen)` |
|      2 | 4755 | `{` |
|     36 | 4756 | `	if( pDev == 0 ){` |
|    ! 0 | 4757 | `		return;` |
|      - | 4758 | `	}` |
|      - | 4759 | `#ifdef PH7_ENABLE_NET` |
|     36 | 4760 | `	if( pDev->pStream == &sTCP_Stream && pDev->pHandle ){` |
|      3 | 4761 | `		sock_private *pSock = (sock_private *)pDev->pHandle;` |
|      3 | 4762 | `		if( pSock->iLastErr != 0 ){` |
|      4 | 4763 | `			ph7_context_throw_error_format(pCtx,PH7_CTX_NOTICE,` |
|      - | 4764 | `				"Send of %d bytes failed with errno=%d %s",` |
|      1 | 4765 | `				nLen,pSock->iLastErr,PH7_NetStrError(pSock->iLastErr));` |
|      3 | 4766 | `			pSock->iLastErr = 0;` |
|      1 | 4767 | `		}` |
|      3 | 4768 | `		return;` |
|      - | 4769 | `	}` |
|      - | 4770 | `#endif` |
|     33 | 4771 | `	if( pDev->pStream == pCtx->pVm->pDefStream ){` |
|     45 | 4772 | `		ph7_context_throw_error_format(pCtx,PH7_CTX_NOTICE,` |
|     22 | 4773 | `			"Write of %d bytes failed with errno=%d %s",nLen,errno,VfsStrerror(errno));` |
|     11 | 4774 | `	}` |
|     19 | 4775 | `}` |
|      - | 4776 | `/* The settings family below owns both of these; the socket openers here are` |
|      - | 4777 | ` * declared ahead of it so one handle-wrapping routine can serve both halves. */` |
|      - | 4778 | `static io_private * StreamSettingArgNamed(ph7_context *pCtx,ph7_value *pArg,int iPos,` |
|      - | 4779 | `	const char *zName,int *pRc);` |
|      - | 4780 | `static ph7_socket * IoPrivateSocket(io_private *pDev);` |
|      - | 4781 | `/*` |
|      - | 4782 | ` * Wrap an open socket in the io_private every f* builtin drives, so a socket a` |
|      - | 4783 | ` * server accepted reads and writes exactly like one a client connected. A NULL` |
|      - | 4784 | ` * zUri is php's "opened by no name at all" — an accepted connection, which` |
|      - | 4785 | `` * reports no `uri` at all from stream_get_meta_data().`` |
|      - | 4786 | ` * Answers 0 (and closes the socket) when there is no memory for the handle.` |
|      - | 4787 | ` */` |
|    138 | 4788 | `static io_private * SockWrapSocket(ph7_context *pCtx,ph7_socket sock,const char *zUri,int nUri)` |
|      3 | 4789 | `{` |
|      - | 4790 | `	io_private *pDev;` |
|      - | 4791 | `	sock_private *pSock;` |
|    141 | 4792 | `	pDev = (io_private *)ph7_context_alloc_chunk(pCtx,sizeof(io_private),TRUE,FALSE);` |
|    141 | 4793 | `	pSock = (sock_private *)SyMemBackendAlloc(&pCtx->pVm->sAllocator,sizeof(sock_private));` |
|    141 | 4794 | `	if( pDev == 0 \|\| pSock == 0 ){` |
|    ! 0 | 4795 | `		if( pSock ){` |
|    ! 0 | 4796 | `			SyMemBackendFree(&pCtx->pVm->sAllocator,pSock);` |
|    ! 0 | 4797 | `		}` |
|    ! 0 | 4798 | `		if( pDev ){` |
|      - | 4799 | `			/* Allocated with AutoRelease = FALSE, so nothing else will reclaim` |
|      - | 4800 | `			 * this chunk — it is not an io_private yet and has no buffers. */` |
|    ! 0 | 4801 | `			ph7_context_free_chunk(pCtx,pDev);` |
|    ! 0 | 4802 | `		}` |
|    ! 0 | 4803 | `		PH7_NetClose(sock);` |
|    ! 0 | 4804 | `		return 0;` |
|      - | 4805 | `	}` |
|    141 | 4806 | `	pSock->pVm = pCtx->pVm;` |
|    141 | 4807 | `	pSock->sock = sock;` |
|    141 | 4808 | `	pSock->bEof = 0;` |
|    141 | 4809 | `	pSock->iLastErr = 0;` |
|    141 | 4810 | `	pSock->bGeneric = 0;` |
|    141 | 4811 | `	InitIOPrivate(pCtx->pVm,&sTCP_Stream,pDev);` |
|      - | 4812 | `	/* php's feof() answers TRUE for a stream whose socket was never created. */` |
|    141 | 4813 | `	pDev->bEof = (sxu8)(sock == PH7_NET_INVALID_SOCKET ? 1 : 0);` |
|    141 | 4814 | `	SetIOPrivateOpenedAs(pDev,zUri,nUri,"r+",2);` |
|    141 | 4815 | `	pDev->pHandle = (void *)pSock;` |
|    141 | 4816 | `	return pDev;` |
|     72 | 4817 | `}` |
|      - | 4818 | `/*` |
|      - | 4819 | ` * Undo a SockWrapSocket() whose partner could not be wrapped: the device's own` |
|      - | 4820 | ` * close hook frees the socket handle, and the io_private chunk goes with it.` |
|      - | 4821 | ` * Nothing has handed this out as a resource yet, so there is no ph7_value that` |
|      - | 4822 | ` * could observe it afterwards.` |
|      - | 4823 | ` */` |
|    ! 0 | 4824 | `static void SockCloseWrapped(ph7_context *pCtx,io_private *pDev)` |
|    ! 0 | 4825 | `{` |
|    ! 0 | 4826 | `	if( pDev == 0 ){` |
|    ! 0 | 4827 | `		return;` |
|      - | 4828 | `	}` |
|    ! 0 | 4829 | `	if( pDev->pStream && pDev->pStream->xClose && pDev->pHandle ){` |
|    ! 0 | 4830 | `		pDev->pStream->xClose(pDev->pHandle);` |
|    ! 0 | 4831 | `		pDev->pHandle = 0;` |
|    ! 0 | 4832 | `	}` |
|    ! 0 | 4833 | `	ReleaseIOPrivate(pCtx,pDev);` |
|    ! 0 | 4834 | `}` |
|      - | 4835 | `/* Forward: php's port rule, defined with the address parser further down. */` |
|      - | 4836 | `static int SockParsePort(const char *z,int n);` |
|      - | 4837 | `/*` |
|      - | 4838 | `` * php's `socket` context options, read into the shape net.c applies. Only the`` |
|      - | 4839 | `` * ones a tcp-only, IPv4-only transport can honour are read: `bindto`, which is`` |
|      - | 4840 | `` * the LOCAL address a client connects out from, `backlog`, `so_reuseport` and`` |
|      - | 4841 | ``  * `tcp_nodelay`. `so_broadcast` describes a datagram socket and `ipv6_v6only` `` |
|      - | 4842 | ` * an address family this build has not got, so they stay on the context` |
|      - | 4843 | ` * unapplied (§7.4 slice-2 (a)).` |
|      - | 4844 | ` *` |
|      - | 4845 | `` * `bindto` is "host:port", split at the FIRST colon with an atoi() port — the`` |
|      - | 4846 | ` * same address rule the server half already uses — and a spelling with no colon` |
|      - | 4847 | ` * at all is not an address, so php performs no bind and says nothing. A value` |
|      - | 4848 | ` * that is not a STRING is php's one hard failure here; everything else is a` |
|      - | 4849 | ` * warning and a connection made from wherever routing would have sent it.` |
|      - | 4850 | ` * Returns 0, or -1 with *pzErr set to php's refusal.` |
|      - | 4851 | ` */` |
|    182 | 4852 | `static int SockCtxOptions(phl_stream_ctx *pCtxRes,ph7_sockopts *pOut,char *zHostBuf,int nHostBuf,` |
|      - | 4853 | `	const char **pzErr)` |
|      4 | 4854 | `{` |
|      - | 4855 | `	ph7_value *pVal;` |
|    186 | 4856 | `	SyZero(pOut,sizeof(*pOut));` |
|    186 | 4857 | `	if( pCtxRes == 0 ){` |
|    117 | 4858 | `		return 0;` |
|      - | 4859 | `	}` |
|     70 | 4860 | `	pVal = PH7_StreamCtxOption(pCtxRes,"socket","backlog");` |
|     70 | 4861 | `	if( pVal ){` |
|      5 | 4862 | `		pOut->iBacklog = (int)ph7_value_to_int64(pVal);` |
|      2 | 4863 | `	}` |
|     70 | 4864 | `	pVal = PH7_StreamCtxOption(pCtxRes,"socket","so_reuseport");` |
|     70 | 4865 | `	pOut->bReusePort = pVal != 0 && ph7_value_to_bool(pVal);` |
|     70 | 4866 | `	pVal = PH7_StreamCtxOption(pCtxRes,"socket","tcp_nodelay");` |
|     70 | 4867 | `	pOut->bNoDelay = pVal != 0 && ph7_value_to_bool(pVal);` |
|     70 | 4868 | `	pVal = PH7_StreamCtxOption(pCtxRes,"socket","bindto");` |
|     70 | 4869 | `	if( pVal ){` |
|      - | 4870 | `		const char *zSpec;` |
|     15 | 4871 | `		int nSpec = 0,i,nHost = -1;` |
|     15 | 4872 | `		if( (pVal->iFlags & MEMOBJ_STRING) == 0 ){` |
|      3 | 4873 | `			*pzErr = "local_addr context option is not a string.";` |
|      3 | 4874 | `			return -1;` |
|      - | 4875 | `		}` |
|     13 | 4876 | `		zSpec = (const char *)SyBlobData(&pVal->sBlob);` |
|     13 | 4877 | `		nSpec = (int)SyBlobLength(&pVal->sBlob);` |
|    137 | 4878 | `		for( i = 0 ; i + 1 < nSpec ; i++ ){` |
|    135 | 4879 | `			if( zSpec[i] == ':' ){` |
|     11 | 4880 | `				nHost = i;` |
|     11 | 4881 | `				pOut->iBindPort = SockParsePort(&zSpec[i+1],nSpec - i - 1);` |
|     11 | 4882 | `				break;` |
|      - | 4883 | `			}` |
|     63 | 4884 | `		}` |
|     13 | 4885 | `		if( nHost >= 0 ){` |
|     11 | 4886 | `			if( nHost >= nHostBuf ){` |
|    ! 0 | 4887 | `				nHost = nHostBuf - 1;` |
|    ! 0 | 4888 | `			}` |
|     11 | 4889 | `			if( nHost > 0 ){` |
|     11 | 4890 | `				SyMemcpy(zSpec,zHostBuf,(sxu32)nHost);` |
|      5 | 4891 | `			}` |
|     11 | 4892 | `			zHostBuf[nHost] = 0;` |
|     11 | 4893 | `			pOut->zBindHost = zHostBuf;` |
|      5 | 4894 | `		}` |
|      6 | 4895 | `	}` |
|     68 | 4896 | `	return 0;` |
|     95 | 4897 | `}` |
|      - | 4898 | `/*` |
|      - | 4899 | ` * php's PERSISTENT sockets, which pfsockopen() and STREAM_CLIENT_PERSISTENT ask` |
|      - | 4900 | ` * for: a second open of the SAME address hands back the very same resource` |
|      - | 4901 | `` * rather than a second connection — `$a === $b` — and fclose() is what ends it,`` |
|      - | 4902 | ` * after which the next open dials again. The key is the address as the opener` |
|      - | 4903 | ` * spelled it, so "localhost:80" and "127.0.0.1:80" are two of them.` |
|      - | 4904 | ` */` |
|     26 | 4905 | `static void SockPersistKey(char *zBuf,int nBuf,int bClientForm,const char *zAddr,int nAddr)` |
|      1 | 4906 | `{` |
|      - | 4907 | `	/* php prefixes the key with the FUNCTION that asked, so a pfsockopen() and a` |
|      - | 4908 | `	 * persistent stream_socket_client() of one address are two connections. */` |
|     40 | 4909 | `	SyBufferFormat(zBuf,(sxu32)nBuf,"%s__%.*s",` |
|     13 | 4910 | `		bClientForm ? "stream_socket_client" : "pfsockopen",nAddr,zAddr);` |
|     27 | 4911 | `}` |
|     16 | 4912 | `static io_private * SockPersistFind(ph7_vm *pVm,const char *zKey)` |
|      1 | 4913 | `{` |
|     17 | 4914 | `	VmPersistSock *aSlot = (VmPersistSock *)SySetBasePtr(&pVm->aPersistSock);` |
|      - | 4915 | `	sxu32 i;` |
|     37 | 4916 | `	for( i = 0 ; i < SySetUsed(&pVm->aPersistSock) ; i++ ){` |
|     27 | 4917 | `		if( aSlot[i].zKey[0] && SyStrncmp(aSlot[i].zKey,zKey,(sxu32)SyStrlen(zKey) + 1) == 0 ){` |
|      9 | 4918 | `			if( !IO_PRIVATE_INVALID(aSlot[i].pDev) ){` |
|      7 | 4919 | `				return aSlot[i].pDev;` |
|      - | 4920 | `			}` |
|      - | 4921 | `			/* fclose()'d since: the slot is free for the next connection. */` |
|      3 | 4922 | `			aSlot[i].zKey[0] = 0;` |
|      3 | 4923 | `			aSlot[i].pDev = 0;` |
|      1 | 4924 | `		}` |
|     11 | 4925 | `	}` |
|     11 | 4926 | `	return 0;` |
|      9 | 4927 | `}` |
|     10 | 4928 | `static void SockPersistKeep(ph7_vm *pVm,const char *zKey,io_private *pDev)` |
|      1 | 4929 | `{` |
|     11 | 4930 | `	VmPersistSock *aSlot = (VmPersistSock *)SySetBasePtr(&pVm->aPersistSock);` |
|      - | 4931 | `	VmPersistSock sSlot;` |
|      - | 4932 | `	sxu32 i;` |
|     23 | 4933 | `	for( i = 0 ; i < SySetUsed(&pVm->aPersistSock) ; i++ ){` |
|     15 | 4934 | `		if( aSlot[i].zKey[0] == 0 \|\| IO_PRIVATE_INVALID(aSlot[i].pDev) ){` |
|      3 | 4935 | `			SyZero(&aSlot[i],sizeof(VmPersistSock));` |
|      3 | 4936 | `			Systrcpy(aSlot[i].zKey,(sxu32)sizeof(aSlot[i].zKey),zKey,0);` |
|      3 | 4937 | `			aSlot[i].pDev = pDev;` |
|      3 | 4938 | `			return;` |
|      - | 4939 | `		}` |
|      7 | 4940 | `	}` |
|      9 | 4941 | `	SyZero(&sSlot,sizeof(sSlot));` |
|      9 | 4942 | `	Systrcpy(sSlot.zKey,(sxu32)sizeof(sSlot.zKey),zKey,0);` |
|      9 | 4943 | `	sSlot.pDev = pDev;` |
|      9 | 4944 | `	SySetPut(&pVm->aPersistSock,(const void *)&sSlot);` |
|      6 | 4945 | `}` |
|      - | 4946 | `/*` |
|      - | 4947 | `` * php bounds every CONNECTED socket's reads by `default_socket_timeout` from the`` |
|      - | 4948 | ` * moment it is opened — a read from a peer that has gone quiet answers FALSE` |
|      - | 4949 | `` * after it, with `timed_out` set — where this engine armed nothing and waited`` |
|      - | 4950 | ` * forever. That is the difference between a program that reports a dead peer and` |
|      - | 4951 | ` * one that hangs.` |
|      - | 4952 | ` *` |
|      - | 4953 | ` * A LISTENING socket is deliberately left alone: php's accept timeout is its own` |
|      - | 4954 | ` * argument and its own select(), so arming the OS receive timeout here would` |
|      - | 4955 | `` * bound `stream_socket_accept($srv, -1)` — the wait a server asks to be`` |
|      - | 4956 | ` * unbounded — at sixty seconds.` |
|      - | 4957 | ` */` |
|    108 | 4958 | `static void SockArmDefaultTimeout(ph7_context *pCtx,io_private *pDev)` |
|      3 | 4959 | `{` |
|      - | 4960 | `	ph7_int64 iSec;` |
|    111 | 4961 | `	ph7_socket *pSock = pDev ? IoPrivateSocket(pDev) : 0;` |
|    111 | 4962 | `	if( pSock == 0 \|\| *pSock == PH7_NET_INVALID_SOCKET ){` |
|    ! 0 | 4963 | `		return;` |
|      - | 4964 | `	}` |
|    111 | 4965 | `	iSec = PH7_VmIniGetInt(pCtx->pVm,"default_socket_timeout",60);` |
|    111 | 4966 | `	if( iSec > 0 ){` |
|    111 | 4967 | `		PH7_NetSetRwTimeout(*pSock,iSec,0);` |
|    111 | 4968 | `		pDev->bHasTimeout = 1;` |
|     54 | 4969 | `	}` |
|     57 | 4970 | `}` |
|      - | 4971 | `/*` |
|      - | 4972 | ` * The out-params every address-taking opener carries, on the path that WORKED:` |
|      - | 4973 | ` * php writes 0 and "" into them rather than leaving whatever the caller's` |
|      - | 4974 | ` * variables already held.` |
|      - | 4975 | ` */` |
|    116 | 4976 | `static void SockAddressSuccess(ph7_context *pCtx,ph7_value **apArg,int nArg,int iArgErrno,int iArgErrstr)` |
|      3 | 4977 | `{` |
|    119 | 4978 | `	ph7_value *pTmp = ph7_context_new_scalar(pCtx);` |
|    119 | 4979 | `	if( pTmp == 0 ){` |
|    ! 0 | 4980 | `		return;` |
|      - | 4981 | `	}` |
|    119 | 4982 | `	if( iArgErrno >= 0 && nArg > iArgErrno ){` |
|     95 | 4983 | `		ph7_value_int(pTmp,0);` |
|     95 | 4984 | `		PH7_VmStoreArgByRef(pCtx->pVm,apArg[iArgErrno],pTmp);` |
|     46 | 4985 | `	}` |
|    119 | 4986 | `	if( iArgErrstr >= 0 && nArg > iArgErrstr ){` |
|     95 | 4987 | `		ph7_value_string(pTmp,"",0);` |
|     95 | 4988 | `		PH7_VmStoreArgByRef(pCtx->pVm,apArg[iArgErrstr],pTmp);` |
|     46 | 4989 | `	}` |
|     61 | 4990 | `}` |
|      - | 4991 | `/*` |
|      - | 4992 | ` * The failure shape the whole address-taking family shares: php words the` |
|      - | 4993 | ` * reason into BOTH the by-ref out-params and a warning naming the address as` |
|      - | 4994 | `` * the script wrote it. The `$errno` out-param stays 0 for everything the`` |
|      - | 4995 | ` * ADDRESS itself is refused for — php only ever reports an OS code for a` |
|      - | 4996 | ` * connect() that reached the network.` |
|      - | 4997 | ` */` |
|     88 | 4998 | `static void SockAddressFailure(ph7_context *pCtx,ph7_value **apArg,int nArg,int iArgErrno,int iArgErrstr,` |
|      - | 4999 | `	const char *zAddr,int nAddr,const char *zErr,int iErrno)` |
|      2 | 5000 | `{` |
|      - | 5001 | `	ph7_value *pTmp;` |
|     90 | 5002 | `	if( zErr == 0 ){` |
|    ! 0 | 5003 | `		zErr = "";` |
|    ! 0 | 5004 | `	}` |
|     90 | 5005 | `	pTmp = ph7_context_new_scalar(pCtx);` |
|     90 | 5006 | `	if( pTmp ){` |
|     90 | 5007 | `		if( iArgErrno >= 0 && nArg > iArgErrno ){` |
|     90 | 5008 | `			ph7_value_int(pTmp,iErrno);` |
|     90 | 5009 | `			PH7_VmStoreArgByRef(pCtx->pVm,apArg[iArgErrno],pTmp);` |
|     44 | 5010 | `		}` |
|     90 | 5011 | `		if( iArgErrstr >= 0 && nArg > iArgErrstr ){` |
|     90 | 5012 | `			ph7_value_string(pTmp,zErr,-1);` |
|     90 | 5013 | `			PH7_VmStoreArgByRef(pCtx->pVm,apArg[iArgErrstr],pTmp);` |
|     44 | 5014 | `		}` |
|     44 | 5015 | `	}` |
|      - | 5016 | `	/* NOTE: ph7_context_throw_error_format already prepends "fname(): " */` |
|    134 | 5017 | `	ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,"Unable to connect to %.*s (%s)",` |
|     44 | 5018 | `		nAddr,zAddr,zErr);` |
|     90 | 5019 | `}` |
|      - | 5020 | `/*` |
|      - | 5021 | ` * The one failure whose message names the HOST, and the one php reports TWICE:` |
|      - | 5022 | ` * its transport raises the text on its own before the opener that asked repeats` |
|      - | 5023 | ` * it inside "Unable to connect to". Composed here because net.c hands back a` |
|      - | 5024 | ` * static string and only the caller has the name to word in.` |
|      - | 5025 | ` */` |
|      6 | 5026 | `static const char * SockResolveFailure(ph7_context *pCtx,const char *zHost,char *zBuf,int nBuf)` |
|      1 | 5027 | `{` |
|     10 | 5028 | `	SyBufferFormat(zBuf,(sxu32)nBuf,` |
|      3 | 5029 | `		"php_network_getaddresses: getaddrinfo for %s failed: Name or service not known",zHost);` |
|      7 | 5030 | `	ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,"%s",zBuf);` |
|      7 | 5031 | `	return zBuf;` |
|      1 | 5032 | `}` |
|      - | 5033 | `PH7_PRIVATE const ph7_io_stream sTCP_Stream = {` |
|      - | 5034 | `	"tcp",` |
|      - | 5035 | `	PH7_IO_STREAM_VERSION,` |
|      - | 5036 | `	SockStreamData_Open, /* xOpen */` |
|      - | 5037 | `	0,   /* xOpenDir */` |
|      - | 5038 | `	SockStreamData_Close,/* xClose */` |
|      - | 5039 | `	0,  /* xCloseDir */` |
|      - | 5040 | `	SockStreamData_Read, /* xRead */` |
|      - | 5041 | `	0,  /* xReadDir */` |
|      - | 5042 | `	SockStreamData_Write,/* xWrite */` |
|      - | 5043 | `	0,  /* xSeek (sockets are not seekable) */` |
|      - | 5044 | `	0,  /* xLock */` |
|      - | 5045 | `	0,  /* xRewindDir */` |
|      - | 5046 | `	0,  /* xTell */` |
|      - | 5047 | `	0,  /* xTrunc */` |
|      - | 5048 | `	0,  /* xSync */` |
|      - | 5049 | `	0   /* xStat */` |
|      - | 5050 | `};` |
|      - | 5051 | `#endif /* PH7_ENABLE_NET */` |
|      - | 5052 | `/*` |
|      - | 5053 | ` * Userland stream wrappers (stream_wrapper_register). The engine's device` |
|      - | 5054 | ` * callbacks receive no device pointer, so each registered wrapper needs its` |
|      - | 5055 | ` * OWN xOpen thunk: PHL keeps a bounded pool of PHL_UWRAP_MAX slots, each with` |
|      - | 5056 | ` * a static thunk that knows its index (recorded limit; php has no cap).` |
|      - | 5057 | ` * The handle carries the userland object, and every stream op dispatches the` |
|      - | 5058 | ` * php streamWrapper protocol method on it.` |
|      - | 5059 | ` */` |
|      - | 5060 | `#define PHL_UWRAP_MAX 8` |
|      - | 5061 | `typedef struct uwrap_slot uwrap_slot;` |
|      - | 5062 | `struct uwrap_slot` |
|      - | 5063 | `{` |
|      - | 5064 | `	ph7_vm *pVm;              /* owning VM (0 = free slot) */` |
|      - | 5065 | `	char zScheme[32];         /* protocol name */` |
|      - | 5066 | `	char zClass[128];         /* userland wrapper class */` |
|      - | 5067 | `	int bIsUrl;               /* registered with STREAM_IS_URL: opening it is gated` |
|      - | 5068 | `	                           * by allow_url_fopen, INCLUDING it by` |
|      - | 5069 | `	                           * allow_url_include */` |
|      - | 5070 | `	ph7_io_stream sStream;    /* the device handed to the VM */` |
|      - | 5071 | `};` |
|      - | 5072 | `typedef struct uwrap_handle uwrap_handle;` |
|      - | 5073 | `struct uwrap_handle` |
|      - | 5074 | `{` |
|      - | 5075 | `	ph7_vm *pVm;` |
|      - | 5076 | `	ph7_class_instance *pObj; /* the wrapper instance (one per open stream) */` |
|      - | 5077 | `	int iSlot;` |
|      - | 5078 | `	int bEof;` |
|      - | 5079 | `};` |
|      - | 5080 | `static uwrap_slot g_aUwrap[PHL_UWRAP_MAX];` |
|      - | 5081 | `/*` |
|      - | 5082 | ` * Was this device registered with STREAM_IS_URL? Only a userland wrapper can` |
|      - | 5083 | ` * carry the flag, so the answer is a scan of the registration slots.` |
|      - | 5084 | ` */` |
|  39068 | 5085 | `PH7_PRIVATE int PH7_StreamIsUrlWrapper(const ph7_io_stream *pStream)` |
|      5 | 5086 | `{` |
|      - | 5087 | `	int i;` |
|      - | 5088 | `	/* php marks its own data:// wrapper a URL, and that is the one that matters` |
|      - | 5089 | ``	 * here: `include 'data://text/plain;base64,…'` executes bytes from the URI`` |
|      - | 5090 | `	 * itself, which is why php refuses it unless allow_url_include says` |
|      - | 5091 | `	 * otherwise. php:// is NOT a URL wrapper in php and stays open. */` |
|  39068 | 5092 | `	if( pStream && pStream->zName` |
|  39073 | 5093 | `	 && SyStrlen(pStream->zName) == 4 && SyStrnicmp(pStream->zName,"data",4) == 0 ){` |
|     51 | 5094 | `		return 1;` |
|      - | 5095 | `	}` |
| 350699 | 5096 | `	for( i = 0 ; i < PHL_UWRAP_MAX ; i++ ){` |
| 311741 | 5097 | `		if( g_aUwrap[i].pVm && &g_aUwrap[i].sStream == pStream ){` |
|     65 | 5098 | `			return g_aUwrap[i].bIsUrl;` |
|      - | 5099 | `		}` |
| 155842 | 5100 | `	}` |
|  38963 | 5101 | `	return 0;` |
|  19539 | 5102 | `}` |
|      - | 5103 | `/*` |
|      - | 5104 | ` * Is this device one of the userland wrapper slots? php labels every such` |
|      - | 5105 | `` * stream `user-space` rather than by its protocol.`` |
|      - | 5106 | ` */` |
|   8838 | 5107 | `static int IoPrivateIsUwrap(const ph7_io_stream *pStream)` |
|      5 | 5108 | `{` |
|      - | 5109 | `	int i;` |
|  79387 | 5110 | `	for( i = 0 ; i < PHL_UWRAP_MAX ; i++ ){` |
|  70569 | 5111 | `		if( g_aUwrap[i].pVm && &g_aUwrap[i].sStream == pStream ){` |
|     22 | 5112 | `			return 1;` |
|      - | 5113 | `		}` |
|  35277 | 5114 | `	}` |
|   8823 | 5115 | `	return 0;` |
|   4424 | 5116 | `}` |
|      - | 5117 | `/*` |
|      - | 5118 | ` * Is this device one of the registration slots at all? Unlike IoPrivateIsUwrap()` |
|      - | 5119 | ` * this does NOT ask whether the slot is still live -- restore() has to tell a` |
|      - | 5120 | ` * withdrawn userland wrapper from a built-in, and a withdrawn slot has already` |
|      - | 5121 | ` * had its pVm cleared.` |
|      - | 5122 | ` */` |
|     18 | 5123 | `static int UwrapIsSlotDevice(const ph7_io_stream *pStream)` |
|      1 | 5124 | `{` |
|      - | 5125 | `	int i;` |
|     99 | 5126 | `	for( i = 0 ; i < PHL_UWRAP_MAX ; i++ ){` |
|     89 | 5127 | `		if( &g_aUwrap[i].sStream == pStream ){` |
|      9 | 5128 | `			return 1;` |
|      - | 5129 | `		}` |
|     41 | 5130 | `	}` |
|     11 | 5131 | `	return 0;` |
|     10 | 5132 | `}` |
|      - | 5133 | `/* Forward: the protocol dispatcher is defined just below. */` |
|      - | 5134 | `static int UwrapCall(uwrap_handle *pH,const char *zMethod,int nArg,ph7_value **apArg,` |
|      - | 5135 | `	ph7_value *pResult);` |
|      - | 5136 | `/*` |
|      - | 5137 | ` * Ask a userland wrapper whether it is at end of file — php's own` |
|      - | 5138 | ` * streamWrapper::stream_eof(), which PHL used to leave undispatched, inferring` |
|      - | 5139 | ` * the answer from a zero-length read instead. Returns 0 when the handle is not` |
|      - | 5140 | ` * a userland stream (nothing written to *pAnswer).` |
|      - | 5141 | ` */` |
|   8626 | 5142 | `static int IoPrivateUwrapEof(io_private *pDev,int *pAnswer)` |
|      5 | 5143 | `{` |
|      - | 5144 | `	uwrap_handle *pH;` |
|      - | 5145 | `	ph7_value sRet;` |
|   8631 | 5146 | `	if( pDev == 0 \|\| pDev->pHandle == 0 \|\| !IoPrivateIsUwrap(pDev->pStream) ){` |
|   8625 | 5147 | `		return 0;` |
|      - | 5148 | `	}` |
|      8 | 5149 | `	pH = (uwrap_handle *)pDev->pHandle;` |
|      8 | 5150 | `	PH7_MemObjInit(pH->pVm,&sRet);` |
|      8 | 5151 | `	if( UwrapCall(pH,"stream_eof",0,0,&sRet) != 0 ){` |
|      - | 5152 | `		/* php's streamWrapper requires the method; a class without one keeps` |
|      - | 5153 | `		 * the read-derived answer rather than being called into. */` |
|    ! 0 | 5154 | `		PH7_MemObjRelease(&sRet);` |
|    ! 0 | 5155 | `		*pAnswer = pH->bEof;` |
|    ! 0 | 5156 | `		return 1;` |
|      - | 5157 | `	}` |
|      8 | 5158 | `	*pAnswer = ph7_value_to_bool(&sRet) ? 1 : 0;` |
|      8 | 5159 | `	PH7_MemObjRelease(&sRet);` |
|      8 | 5160 | `	return 1;` |
|   4318 | 5161 | `}` |
|      - | 5162 | `/*` |
|      - | 5163 | `` * The wrapper INSTANCE serving an open userland stream (php's `wrapper_data`),`` |
|      - | 5164 | ` * or 0 for any other device.` |
|      - | 5165 | ` */` |
|     88 | 5166 | `static ph7_class_instance * IoPrivateUwrapObject(io_private *pDev)` |
|      4 | 5167 | `{` |
|     92 | 5168 | `	if( pDev == 0 \|\| pDev->pHandle == 0 \|\| !IoPrivateIsUwrap(pDev->pStream) ){` |
|     88 | 5169 | `		return 0;` |
|      - | 5170 | `	}` |
|      5 | 5171 | `	return ((uwrap_handle *)pDev->pHandle)->pObj;` |
|     48 | 5172 | `}` |
|      - | 5173 | `/* Call $obj->$zMethod(...) and copy the result into pResult (may be 0) */` |
|    184 | 5174 | `static int UwrapCall(uwrap_handle *pH,const char *zMethod,int nArg,ph7_value **apArg,` |
|      - | 5175 | `	ph7_value *pResult)` |
|      3 | 5176 | `{` |
|      - | 5177 | `	ph7_class_method *pMeth;` |
|    187 | 5178 | `	if( pH == 0 \|\| pH->pObj == 0 ){` |
|    ! 0 | 5179 | `		return -1;` |
|      - | 5180 | `	}` |
|    187 | 5181 | `	pMeth = PH7_ClassExtractMethod(pH->pObj->pClass,zMethod,(sxu32)SyStrlen(zMethod));` |
|    187 | 5182 | `	if( pMeth == 0 ){` |
|     27 | 5183 | `		return -1;` |
|      - | 5184 | `	}` |
|    163 | 5185 | `	if( PH7_VmCallClassMethod(pH->pVm,pH->pObj,pMeth,pResult,nArg,apArg) != SXRET_OK ){` |
|    ! 0 | 5186 | `		return -1;` |
|      - | 5187 | `	}` |
|    163 | 5188 | `	return 0;` |
|     95 | 5189 | `}` |
|     58 | 5190 | `static ph7_int64 UwrapRead(void *pHandle,void *pBuffer,ph7_int64 nRead)` |
|      3 | 5191 | `{` |
|     61 | 5192 | `	uwrap_handle *pH = (uwrap_handle *)pHandle;` |
|      - | 5193 | `	ph7_value sArg,sRet;` |
|      - | 5194 | `	const char *zData;` |
|     61 | 5195 | `	int nData = 0;` |
|     61 | 5196 | `	ph7_int64 n = 0;` |
|     61 | 5197 | `	if( pH == 0 \|\| pH->bEof ){` |
|    ! 0 | 5198 | `		return 0;` |
|      - | 5199 | `	}` |
|     61 | 5200 | `	PH7_MemObjInit(pH->pVm,&sArg);` |
|     61 | 5201 | `	PH7_MemObjInit(pH->pVm,&sRet);` |
|     61 | 5202 | `	ph7_value_int64(&sArg,nRead);` |
|      - | 5203 | `	{` |
|      - | 5204 | `		ph7_value *apArg[1];` |
|     61 | 5205 | `		apArg[0] = &sArg;` |
|     61 | 5206 | `		if( UwrapCall(pH,"stream_read",1,apArg,&sRet) != 0 ){` |
|    ! 0 | 5207 | `			PH7_MemObjRelease(&sArg);` |
|    ! 0 | 5208 | `			PH7_MemObjRelease(&sRet);` |
|    ! 0 | 5209 | `			return -1;` |
|      - | 5210 | `		}` |
|      - | 5211 | `	}` |
|     61 | 5212 | `	zData = ph7_value_to_string(&sRet,&nData);` |
|     61 | 5213 | `	if( nData > 0 ){` |
|     31 | 5214 | `		if( (ph7_int64)nData > nRead ){` |
|    ! 0 | 5215 | `			nData = (int)nRead;` |
|    ! 0 | 5216 | `		}` |
|     31 | 5217 | `		SyMemcpy(zData,pBuffer,(sxu32)nData);` |
|     31 | 5218 | `		n = nData;` |
|     17 | 5219 | `	}else{` |
|     32 | 5220 | `		pH->bEof = 1;` |
|      - | 5221 | `	}` |
|     61 | 5222 | `	PH7_MemObjRelease(&sArg);` |
|     61 | 5223 | `	PH7_MemObjRelease(&sRet);` |
|     61 | 5224 | `	return n;` |
|     32 | 5225 | `}` |
|      4 | 5226 | `static ph7_int64 UwrapWrite(void *pHandle,const void *pBuf,ph7_int64 nWrite)` |
|      1 | 5227 | `{` |
|      5 | 5228 | `	uwrap_handle *pH = (uwrap_handle *)pHandle;` |
|      - | 5229 | `	ph7_value sArg,sRet;` |
|      - | 5230 | `	ph7_int64 n;` |
|      5 | 5231 | `	if( pH == 0 ){` |
|    ! 0 | 5232 | `		return -1;` |
|      - | 5233 | `	}` |
|      5 | 5234 | `	PH7_MemObjInit(pH->pVm,&sArg);` |
|      5 | 5235 | `	PH7_MemObjInit(pH->pVm,&sRet);` |
|      5 | 5236 | `	ph7_value_string(&sArg,(const char *)pBuf,(int)nWrite);` |
|      - | 5237 | `	{` |
|      - | 5238 | `		ph7_value *apArg[1];` |
|      5 | 5239 | `		apArg[0] = &sArg;` |
|      5 | 5240 | `		if( UwrapCall(pH,"stream_write",1,apArg,&sRet) != 0 ){` |
|    ! 0 | 5241 | `			PH7_MemObjRelease(&sArg);` |
|    ! 0 | 5242 | `			PH7_MemObjRelease(&sRet);` |
|    ! 0 | 5243 | `			return -1;` |
|      - | 5244 | `		}` |
|      - | 5245 | `	}` |
|      5 | 5246 | `	n = ph7_value_to_int64(&sRet);` |
|      5 | 5247 | `	PH7_MemObjRelease(&sArg);` |
|      5 | 5248 | `	PH7_MemObjRelease(&sRet);` |
|      5 | 5249 | `	return n;` |
|      3 | 5250 | `}` |
|      2 | 5251 | `static int UwrapSeek(void *pHandle,ph7_int64 iOfft,int whence)` |
|      1 | 5252 | `{` |
|      3 | 5253 | `	uwrap_handle *pH = (uwrap_handle *)pHandle;` |
|      - | 5254 | `	ph7_value sOfft,sWhence,sRet;` |
|      - | 5255 | `	ph7_value *apArg[2];` |
|      - | 5256 | `	int rc;` |
|      3 | 5257 | `	if( pH == 0 ){` |
|    ! 0 | 5258 | `		return -1;` |
|      - | 5259 | `	}` |
|      3 | 5260 | `	PH7_MemObjInit(pH->pVm,&sOfft);` |
|      3 | 5261 | `	PH7_MemObjInit(pH->pVm,&sWhence);` |
|      3 | 5262 | `	PH7_MemObjInit(pH->pVm,&sRet);` |
|      3 | 5263 | `	ph7_value_int64(&sOfft,iOfft);` |
|      3 | 5264 | `	ph7_value_int(&sWhence,whence);` |
|      3 | 5265 | `	apArg[0] = &sOfft;` |
|      3 | 5266 | `	apArg[1] = &sWhence;` |
|      3 | 5267 | `	rc = UwrapCall(pH,"stream_seek",2,apArg,&sRet);` |
|      3 | 5268 | `	if( rc == 0 ){` |
|      3 | 5269 | `		pH->bEof = 0;` |
|      3 | 5270 | `		rc = ph7_value_to_bool(&sRet) ? PH7_OK : -1;` |
|      1 | 5271 | `	}` |
|      3 | 5272 | `	PH7_MemObjRelease(&sOfft);` |
|      3 | 5273 | `	PH7_MemObjRelease(&sWhence);` |
|      3 | 5274 | `	PH7_MemObjRelease(&sRet);` |
|      3 | 5275 | `	return rc;` |
|      2 | 5276 | `}` |
|      6 | 5277 | `static ph7_int64 UwrapTell(void *pHandle)` |
|      1 | 5278 | `{` |
|      7 | 5279 | `	uwrap_handle *pH = (uwrap_handle *)pHandle;` |
|      - | 5280 | `	ph7_value sRet;` |
|      - | 5281 | `	ph7_int64 n;` |
|      7 | 5282 | `	if( pH == 0 ){` |
|    ! 0 | 5283 | `		return -1;` |
|      - | 5284 | `	}` |
|      7 | 5285 | `	PH7_MemObjInit(pH->pVm,&sRet);` |
|      7 | 5286 | `	if( UwrapCall(pH,"stream_tell",0,0,&sRet) != 0 ){` |
|      3 | 5287 | `		PH7_MemObjRelease(&sRet);` |
|      3 | 5288 | `		return -1;` |
|      - | 5289 | `	}` |
|      5 | 5290 | `	n = ph7_value_to_int64(&sRet);` |
|      5 | 5291 | `	PH7_MemObjRelease(&sRet);` |
|      5 | 5292 | `	return n;` |
|      4 | 5293 | `}` |
|     50 | 5294 | `static void UwrapClose(void *pHandle)` |
|      3 | 5295 | `{` |
|     53 | 5296 | `	uwrap_handle *pH = (uwrap_handle *)pHandle;` |
|     53 | 5297 | `	if( pH == 0 ){` |
|    ! 0 | 5298 | `		return;` |
|      - | 5299 | `	}` |
|     53 | 5300 | `	UwrapCall(pH,"stream_close",0,0,0);` |
|     53 | 5301 | `	if( pH->pObj ){` |
|     53 | 5302 | `		PH7_ClassInstanceUnref(pH->pObj);` |
|     25 | 5303 | `	}` |
|     53 | 5304 | `	SyMemBackendFree(&pH->pVm->sAllocator,pH);` |
|     28 | 5305 | `}` |
|      - | 5306 | `/* Shared open: instantiate the wrapper class and call stream_open() */` |
|     58 | 5307 | `static int UwrapOpenSlot(int iSlot,const char *zName,int iMode,ph7_value *pResource,void **ppHandle)` |
|      3 | 5308 | `{` |
|     61 | 5309 | `	uwrap_slot *pSlot = &g_aUwrap[iSlot];` |
|     61 | 5310 | `	ph7_vm *pVm = pResource ? pResource->pVm : 0;` |
|      - | 5311 | `	ph7_class *pClass;` |
|      - | 5312 | `	uwrap_handle *pH;` |
|      - | 5313 | `	ph7_value sPath,sMode,sOpts,sOpened,sRet;` |
|      - | 5314 | `	ph7_value *apArg[4];` |
|      - | 5315 | `	int rc;` |
|     61 | 5316 | `	if( pVm == 0 \|\| pSlot->pVm == 0 ){` |
|    ! 0 | 5317 | `		return -1;` |
|      - | 5318 | `	}` |
|     61 | 5319 | `	pClass = PH7_VmExtractClass(pVm,pSlot->zClass,(sxu32)SyStrlen(pSlot->zClass),TRUE,0);` |
|     61 | 5320 | `	if( pClass == 0 ){` |
|    ! 0 | 5321 | `		return -1;` |
|      - | 5322 | `	}` |
|     61 | 5323 | `	pH = (uwrap_handle *)SyMemBackendAlloc(&pVm->sAllocator,sizeof(uwrap_handle));` |
|     61 | 5324 | `	if( pH == 0 ){` |
|    ! 0 | 5325 | `		return -1;` |
|      - | 5326 | `	}` |
|     61 | 5327 | `	pH->pVm = pVm;` |
|     61 | 5328 | `	pH->iSlot = iSlot;` |
|     61 | 5329 | `	pH->bEof = 0;` |
|     61 | 5330 | `	pH->pObj = PH7_NewClassInstance(pVm,pClass);` |
|     61 | 5331 | `	if( pH->pObj == 0 ){` |
|    ! 0 | 5332 | `		SyMemBackendFree(&pVm->sAllocator,pH);` |
|    ! 0 | 5333 | `		return -1;` |
|      - | 5334 | `	}` |
|      - | 5335 | `	{` |
|      - | 5336 | `		/* php's streamWrapper::$context, set on the serving instance BEFORE` |
|      - | 5337 | `		 * stream_open() runs — which is the whole reason a userland wrapper can` |
|      - | 5338 | `		 * be configured per open. It is exactly what the OPENER resolved: the` |
|      - | 5339 | ``		 * default context substitutes for a NULL `$context` argument, so an`` |
|      - | 5340 | `		 * ordinary fopen() hands a resource over; but an opener with no such` |
|      - | 5341 | `		 * argument at all (md5_file(), include) and one that carried` |
|      - | 5342 | `		 * FILE_NO_DEFAULT_CONTEXT hand over php's NULL. Substituting the default` |
|      - | 5343 | `		 * here would make that flag mean nothing.` |
|      - | 5344 | `		 * The class need not declare the slot; php adds it either way. */` |
|     61 | 5345 | `		phl_stream_ctx *pOpenCtx = (phl_stream_ctx *)pVm->pOpenCtx;` |
|     61 | 5346 | `		ph7_value *pCtxSlot = PH7_NativeAttr(pH->pObj,"context");` |
|     61 | 5347 | `		if( pCtxSlot == 0 ){` |
|    ! 0 | 5348 | `			pCtxSlot = PH7_VmCreateDynamicAttr(pVm,pH->pObj,"context",sizeof("context")-1,0);` |
|    ! 0 | 5349 | `		}` |
|     61 | 5350 | `		if( pCtxSlot ){` |
|     61 | 5351 | `			if( pOpenCtx ){` |
|     49 | 5352 | `				ph7_value_resource(pCtxSlot,(void *)pOpenCtx);` |
|     26 | 5353 | `			}else{` |
|     14 | 5354 | `				ph7_value_null(pCtxSlot);` |
|      - | 5355 | `			}` |
|     29 | 5356 | `		}` |
|      - | 5357 | `	}` |
|      - | 5358 | `	/* php hands stream_open the FULL url, scheme included */` |
|     61 | 5359 | `	PH7_MemObjInit(pVm,&sPath);` |
|     61 | 5360 | `	PH7_MemObjInit(pVm,&sMode);` |
|     61 | 5361 | `	PH7_MemObjInit(pVm,&sOpts);` |
|     61 | 5362 | `	PH7_MemObjInit(pVm,&sRet);` |
|      - | 5363 | `	/* $opened_path is BY REFERENCE: the callee's binding needs a real memobj` |
|      - | 5364 | `	 * slot (a stack ph7_value has nIdx == SXU32_HIGH and the engine rejects` |
|      - | 5365 | `	 * it as "could not be passed by reference"). */` |
|      - | 5366 | `	{` |
|     61 | 5367 | `		ph7_value *pRefSlot = PH7_ReserveMemObj(pVm);` |
|     61 | 5368 | `		if( pRefSlot == 0 ){` |
|    ! 0 | 5369 | `			PH7_ClassInstanceUnref(pH->pObj);` |
|    ! 0 | 5370 | `			SyMemBackendFree(&pVm->sAllocator,pH);` |
|    ! 0 | 5371 | `			return -1;` |
|      - | 5372 | `		}` |
|     61 | 5373 | `		PH7_MemObjInit(pVm,&sOpened);` |
|     61 | 5374 | `		sOpened.nIdx = pRefSlot->nIdx;` |
|      - | 5375 | `	}` |
|     58 | 5376 | `	if( SyStrlen(pSlot->zScheme) == sizeof("file")-1` |
|     49 | 5377 | `	 && SyStrnicmp(pSlot->zScheme,"file",sizeof("file")-1) == 0 ){` |
|      - | 5378 | `		/* The one scheme php does NOT hand back whole. Its locate_url_wrapper` |
|      - | 5379 | `		 * strips "file://" for whoever owns the name, built-in or not, so a` |
|      - | 5380 | `		 * wrapper that replaced file:// sees the plain path -- the same bytes a` |
|      - | 5381 | `		 * bare path would have given it. */` |
|      3 | 5382 | `		ph7_value_string(&sPath,zName,-1);` |
|      2 | 5383 | `	}else{` |
|      - | 5384 | `		SyBlob sUrl;` |
|     59 | 5385 | `		SyBlobInit(&sUrl,&pVm->sAllocator);` |
|     59 | 5386 | `		SyBlobFormat(&sUrl,"%s://%s",pSlot->zScheme,zName);` |
|     59 | 5387 | `		ph7_value_string(&sPath,(const char *)SyBlobData(&sUrl),(int)SyBlobLength(&sUrl));` |
|     59 | 5388 | `		SyBlobRelease(&sUrl);` |
|      - | 5389 | `	}` |
|     89 | 5390 | `	ph7_value_string(&sMode,(iMode & PH7_IO_OPEN_WRONLY) ? "w"` |
|     56 | 5391 | `		: ((iMode & PH7_IO_OPEN_APPEND) ? "a" : "r"),-1);` |
|     61 | 5392 | `	ph7_value_int(&sOpts,0);` |
|     61 | 5393 | `	apArg[0] = &sPath;` |
|     61 | 5394 | `	apArg[1] = &sMode;` |
|     61 | 5395 | `	apArg[2] = &sOpts;` |
|     61 | 5396 | `	apArg[3] = &sOpened;` |
|     61 | 5397 | `	rc = UwrapCall(pH,"stream_open",4,apArg,&sRet);` |
|     61 | 5398 | `	if( rc == 0 && !ph7_value_to_bool(&sRet) ){` |
|      5 | 5399 | `		rc = -1;` |
|      2 | 5400 | `	}` |
|     61 | 5401 | `	PH7_MemObjRelease(&sPath);` |
|     61 | 5402 | `	PH7_MemObjRelease(&sMode);` |
|     61 | 5403 | `	PH7_MemObjRelease(&sOpts);` |
|     61 | 5404 | `	PH7_MemObjRelease(&sOpened);` |
|     61 | 5405 | `	PH7_MemObjRelease(&sRet);` |
|     61 | 5406 | `	if( rc != 0 ){` |
|      - | 5407 | `		/* php's own wording for a wrapper that declined: the call it made, not` |
|      - | 5408 | `		 * an errno the wrapper never set. */` |
|      5 | 5409 | `		PH7_StreamSetOpenErrorCall(pVm,pSlot->zClass,"stream_open");` |
|      5 | 5410 | `		PH7_ClassInstanceUnref(pH->pObj);` |
|      5 | 5411 | `		SyMemBackendFree(&pVm->sAllocator,pH);` |
|      5 | 5412 | `		return -1;` |
|      - | 5413 | `	}` |
|     57 | 5414 | `	*ppHandle = (void *)pH;` |
|     57 | 5415 | `	return PH7_OK;` |
|     32 | 5416 | `}` |
|      - | 5417 | `/* One xOpen thunk per slot (the device callbacks get no device pointer) */` |
|      - | 5418 | `#define PHL_UWRAP_THUNK(N) \` |
|      - | 5419 | `	static int UwrapOpen##N(const char *zName,int iMode,ph7_value *pResource,void **ppHandle) \` |
|      - | 5420 | `	{ return UwrapOpenSlot(N,zName,iMode,pResource,ppHandle); }` |
|     57 | 5421 | `PHL_UWRAP_THUNK(0)` |
|      5 | 5422 | `PHL_UWRAP_THUNK(1)` |
|    ! 0 | 5423 | `PHL_UWRAP_THUNK(2)` |
|    ! 0 | 5424 | `PHL_UWRAP_THUNK(3)` |
|    ! 0 | 5425 | `PHL_UWRAP_THUNK(4)` |
|    ! 0 | 5426 | `PHL_UWRAP_THUNK(5)` |
|    ! 0 | 5427 | `PHL_UWRAP_THUNK(6)` |
|    ! 0 | 5428 | `PHL_UWRAP_THUNK(7)` |
|      - | 5429 | `static int (* const g_aUwrapOpen[PHL_UWRAP_MAX])(const char *,int,ph7_value *,void **) = {` |
|      - | 5430 | `	UwrapOpen0,UwrapOpen1,UwrapOpen2,UwrapOpen3,` |
|      - | 5431 | `	UwrapOpen4,UwrapOpen5,UwrapOpen6,UwrapOpen7` |
|      - | 5432 | `};` |
|      - | 5433 | `/* Is this device already in the VM's list? (A slot survives its wrapper.) */` |
|     28 | 5434 | `static int UwrapDeviceInstalled(ph7_vm *pVm,const ph7_io_stream *pStream)` |
|      3 | 5435 | `{` |
|     31 | 5436 | `	ph7_io_stream **apDev = (ph7_io_stream **)SySetBasePtr(&pVm->aIOstream);` |
|      - | 5437 | `	sxu32 n;` |
|    175 | 5438 | `	for( n = 0 ; n < SySetUsed(&pVm->aIOstream) ; n++ ){` |
|    153 | 5439 | `		if( apDev[n] == pStream ){` |
|      7 | 5440 | `			return 1;` |
|      - | 5441 | `		}` |
|     75 | 5442 | `	}` |
|     25 | 5443 | `	return 0;` |
|     17 | 5444 | `}` |
|      - | 5445 | `/* Put a device back in service. */` |
|     28 | 5446 | `static void UwrapUnsuppressDevice(ph7_vm *pVm,const ph7_io_stream *pStream)` |
|      3 | 5447 | `{` |
|     31 | 5448 | `	const ph7_io_stream **apOff = (const ph7_io_stream **)SySetBasePtr(&pVm->aSuppressedIo);` |
|     31 | 5449 | `	sxu32 n,nKeep = 0;` |
|     41 | 5450 | `	for( n = 0 ; n < SySetUsed(&pVm->aSuppressedIo) ; n++ ){` |
|     11 | 5451 | `		if( apOff[n] == pStream ){` |
|      7 | 5452 | `			continue;` |
|      - | 5453 | `		}` |
|      5 | 5454 | `		apOff[nKeep++] = apOff[n];` |
|      3 | 5455 | `	}` |
|     31 | 5456 | `	SySetTruncate(&pVm->aSuppressedIo,nKeep);` |
|     31 | 5457 | `}` |
|      - | 5458 | `/*` |
|      - | 5459 | ` * bool stream_wrapper_register(string $protocol, string $class, int $flags = 0)` |
|      - | 5460 | ` * bool stream_wrapper_unregister(string $protocol)` |
|      - | 5461 | ` */` |
|     28 | 5462 | `PH7_PRIVATE int PH7_builtin_stream_wrapper_register(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      3 | 5463 | `{` |
|      - | 5464 | `	const char *zScheme,*zClass;` |
|     31 | 5465 | `	int nScheme,nClass,i,iFree = -1;` |
|     31 | 5466 | `	if( nArg < 2 ){` |
|    ! 0 | 5467 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 5468 | `		return PH7_OK;` |
|      - | 5469 | `	}` |
|     31 | 5470 | `	zScheme = ph7_value_to_string(apArg[0],&nScheme);` |
|     31 | 5471 | `	zClass  = ph7_value_to_string(apArg[1],&nClass);` |
|     28 | 5472 | `	if( nScheme < 1 \|\| nScheme >= (int)sizeof(g_aUwrap[0].zScheme)` |
|     31 | 5473 | `	 \|\| nClass < 1 \|\| nClass >= (int)sizeof(g_aUwrap[0].zClass) ){` |
|    ! 0 | 5474 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 5475 | `		return PH7_OK;` |
|      - | 5476 | `	}` |
|      - | 5477 | `	/* php: registering an already-taken protocol warns and returns false.` |
|      - | 5478 | `	 * (Scan the device list directly — PH7_VmGetStreamDevice falls back to` |
|      - | 5479 | `	 * the DEFAULT device for a scheme-less name, so it can't answer this.) */` |
|      - | 5480 | `	{` |
|     31 | 5481 | `		ph7_io_stream **apDev = (ph7_io_stream **)SySetBasePtr(&pCtx->pVm->aIOstream);` |
|      - | 5482 | `		sxu32 n;` |
|    181 | 5483 | `		for( n = 0 ; n < SySetUsed(&pCtx->pVm->aIOstream) ; n++ ){` |
|    153 | 5484 | `			if( PH7_VmStreamDeviceSuppressed(pCtx->pVm,apDev[n]) ){` |
|     11 | 5485 | `				continue; /* unregistered: the name is free again, which is the` |
|      - | 5486 | `				           * whole point of "replace file:// with my own" */` |
|      - | 5487 | `			}` |
|    140 | 5488 | `			if( (int)SyStrlen(apDev[n]->zName) == nScheme` |
|     86 | 5489 | `			 && SyStrnicmp(apDev[n]->zName,zScheme,(sxu32)nScheme) == 0 ){` |
|    ! 0 | 5490 | `				ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,` |
|    ! 0 | 5491 | `					"Protocol %.*s:// is already defined.",nScheme,zScheme);` |
|    ! 0 | 5492 | `				ph7_result_bool(pCtx,0);` |
|    ! 0 | 5493 | `				return PH7_OK;` |
|      - | 5494 | `			}` |
|     73 | 5495 | `		}` |
|      - | 5496 | `	}` |
|     35 | 5497 | `	for( i = 0 ; i < PHL_UWRAP_MAX ; i++ ){` |
|     35 | 5498 | `		if( g_aUwrap[i].pVm == 0 ){` |
|     31 | 5499 | `			iFree = i;` |
|     31 | 5500 | `			break;` |
|      - | 5501 | `		}` |
|      4 | 5502 | `	}` |
|     31 | 5503 | `	if( iFree < 0 ){` |
|    ! 0 | 5504 | `		ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,` |
|      - | 5505 | `			"Too many registered stream wrappers (PHL limit: %d)",PHL_UWRAP_MAX);` |
|    ! 0 | 5506 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 5507 | `		return PH7_OK;` |
|      - | 5508 | `	}` |
|      - | 5509 | `	{` |
|     31 | 5510 | `		uwrap_slot *pSlot = &g_aUwrap[iFree];` |
|     31 | 5511 | `		SyMemcpy(zScheme,pSlot->zScheme,(sxu32)nScheme);` |
|     31 | 5512 | `		pSlot->zScheme[nScheme] = 0;` |
|     31 | 5513 | `		SyMemcpy(zClass,pSlot->zClass,(sxu32)nClass);` |
|     31 | 5514 | `		pSlot->zClass[nClass] = 0;` |
|     31 | 5515 | `		pSlot->pVm = pCtx->pVm;` |
|      - | 5516 | `		/* $flags: php defines exactly one bit for it, STREAM_IS_URL, and it is the` |
|      - | 5517 | `		 * whole reason the argument exists — a wrapper that says it speaks to the` |
|      - | 5518 | `		 * NETWORK is the one allow_url_fopen and allow_url_include turn off. It was` |
|      - | 5519 | `		 * declared in the signature and read by nothing, so a wrapper registered as` |
|      - | 5520 | `		 * a URL was opened and INCLUDED like a local file whatever the` |
|      - | 5521 | `		 * configuration said. */` |
|     31 | 5522 | `		pSlot->bIsUrl = (nArg > 2 && (ph7_value_to_int64(apArg[2]) & PH7_STREAM_IS_URL) != 0);` |
|     31 | 5523 | `		SyZero(&pSlot->sStream,sizeof(ph7_io_stream));` |
|     31 | 5524 | `		pSlot->sStream.zName = pSlot->zScheme;` |
|     31 | 5525 | `		pSlot->sStream.iVersion = PH7_IO_STREAM_VERSION;` |
|     31 | 5526 | `		pSlot->sStream.xOpen = g_aUwrapOpen[iFree];` |
|     31 | 5527 | `		pSlot->sStream.xClose = UwrapClose;` |
|     31 | 5528 | `		pSlot->sStream.xRead = UwrapRead;` |
|     31 | 5529 | `		pSlot->sStream.xWrite = UwrapWrite;` |
|     31 | 5530 | `		pSlot->sStream.xSeek = UwrapSeek;` |
|     31 | 5531 | `		pSlot->sStream.xTell = UwrapTell;` |
|      - | 5532 | `		/* A slot is REUSED once its wrapper has been unregistered, and both the` |
|      - | 5533 | `		 * suppression set and the VM's device list still name it -- so lift the` |
|      - | 5534 | `		 * suppression and install the device only if it is not already there,` |
|      - | 5535 | `		 * or the freshly registered protocol would be born switched off (and` |
|      - | 5536 | `		 * listed twice). */` |
|     31 | 5537 | `		UwrapUnsuppressDevice(pCtx->pVm,&pSlot->sStream);` |
|     31 | 5538 | `		if( !UwrapDeviceInstalled(pCtx->pVm,&pSlot->sStream) ){` |
|     25 | 5539 | `			ph7_vm_config(pCtx->pVm,PH7_VM_CONFIG_IO_STREAM,&pSlot->sStream);` |
|     11 | 5540 | `		}` |
|      - | 5541 | `	}` |
|     31 | 5542 | `	ph7_result_bool(pCtx,1);` |
|     31 | 5543 | `	return PH7_OK;` |
|     17 | 5544 | `}` |
|      - | 5545 | `/*` |
|      - | 5546 | ` * Suppress a live device and, when it is a userland slot, retire the slot with` |
|      - | 5547 | ` * it. Answers 0 when nothing by that name was in service.` |
|      - | 5548 | ` *` |
|      - | 5549 | ` * The match is EXACT and case-SENSITIVE, which php's is too: opening a stream` |
|      - | 5550 | ` * folds the scheme ("FILE://x" reads a file), but unregister() and restore()` |
|      - | 5551 | ` * delete from the wrapper hash by the bytes the script wrote, so` |
|      - | 5552 | ` * stream_wrapper_unregister('FILE') fails where 'file' succeeds.` |
|      - | 5553 | ` */` |
|     16 | 5554 | `static int UwrapSuppressDevice(ph7_vm *pVm,const char *zScheme,int nScheme)` |
|      2 | 5555 | `{` |
|     18 | 5556 | `	ph7_io_stream **apDev = (ph7_io_stream **)SySetBasePtr(&pVm->aIOstream);` |
|     18 | 5557 | `	ph7_io_stream *pHit = 0;` |
|      - | 5558 | `	sxu32 n;` |
|      - | 5559 | `	int i;` |
|    112 | 5560 | `	for( n = 0 ; n < SySetUsed(&pVm->aIOstream) ; n++ ){` |
|     94 | 5561 | `		if( (int)SyStrlen(apDev[n]->zName) == nScheme` |
|     64 | 5562 | `		 && SyMemcmp(apDev[n]->zName,zScheme,(sxu32)nScheme) == 0` |
|     25 | 5563 | `		 && !PH7_VmStreamDeviceSuppressed(pVm,apDev[n]) ){` |
|     14 | 5564 | `			pHit = apDev[n]; /* the LIVE one is the last match */` |
|      6 | 5565 | `		}` |
|     49 | 5566 | `	}` |
|     18 | 5567 | `	if( pHit == 0 ){` |
|      5 | 5568 | `		return 0;` |
|      - | 5569 | `	}` |
|     14 | 5570 | `	if( SySetPut(&pVm->aSuppressedIo,(const void *)&pHit) != SXRET_OK ){` |
|    ! 0 | 5571 | `		return 0;` |
|      - | 5572 | `	}` |
|     46 | 5573 | `	for( i = 0 ; i < PHL_UWRAP_MAX ; i++ ){` |
|     42 | 5574 | `		if( g_aUwrap[i].pVm == pVm && &g_aUwrap[i].sStream == pHit ){` |
|     10 | 5575 | `			g_aUwrap[i].pVm = 0;` |
|     10 | 5576 | `			break;` |
|      - | 5577 | `		}` |
|     17 | 5578 | `	}` |
|     14 | 5579 | `	return 1;` |
|     10 | 5580 | `}` |
|      - | 5581 | `/*` |
|      - | 5582 | ` * bool stream_wrapper_unregister(string $protocol)` |
|      - | 5583 | ` *  Take a protocol out of service. It used to handle USERLAND slots only and` |
|      - | 5584 | ` *  answer FALSE for file/php/data/tcp, so the documented "replace file:// with` |
|      - | 5585 | ` *  my own wrapper" idiom failed loudly at the first step. A built-in is now` |
|      - | 5586 | ` *  suppressed per VM: PH7_VmGetStreamDevice() steps over it (including on the` |
|      - | 5587 | ` *  no-scheme default path, which is the same slot), stream_get_wrappers() stops` |
|      - | 5588 | ` *  naming it, and the name becomes free to register again.` |
|      - | 5589 | ` */` |
|     16 | 5590 | `PH7_PRIVATE int PH7_builtin_stream_wrapper_unregister(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      2 | 5591 | `{` |
|      - | 5592 | `	const char *zScheme;` |
|      - | 5593 | `	int nScheme;` |
|     18 | 5594 | `	if( nArg < 1 ){` |
|    ! 0 | 5595 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 5596 | `		return PH7_OK;` |
|      - | 5597 | `	}` |
|     18 | 5598 | `	zScheme = ph7_value_to_string(apArg[0],&nScheme);` |
|     18 | 5599 | `	if( nScheme > 0 && UwrapSuppressDevice(pCtx->pVm,zScheme,nScheme) ){` |
|     14 | 5600 | `		ph7_result_bool(pCtx,1);` |
|     14 | 5601 | `		return PH7_OK;` |
|      - | 5602 | `	}` |
|      7 | 5603 | `	ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,` |
|      2 | 5604 | `		"Unable to unregister protocol %.*s://",nScheme,zScheme);` |
|      5 | 5605 | `	ph7_result_bool(pCtx,0);` |
|      5 | 5606 | `	return PH7_OK;` |
|     10 | 5607 | `}` |
|      - | 5608 | `/*` |
|      - | 5609 | ` * bool stream_wrapper_restore(string $protocol)` |
|      - | 5610 | ` *  Put a BUILT-IN protocol back, whether it was unregistered or replaced. The` |
|      - | 5611 | ` *  other half of the override pair, and useless without it -- which is why the` |
|      - | 5612 | ` *  two ship together.` |
|      - | 5613 | ` *` |
|      - | 5614 | ` *  php's three answers: a protocol that was never built in is a warning and` |
|      - | 5615 | ` *  FALSE; one that is built in and was never touched is an E_NOTICE and TRUE` |
|      - | 5616 | ` *  (it is already what it should be); anything else is restored and TRUE.` |
|      - | 5617 | ` */` |
|     12 | 5618 | `PH7_PRIVATE int PH7_builtin_stream_wrapper_restore(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 5619 | `{` |
|     13 | 5620 | `	ph7_vm *pVm = pCtx->pVm;` |
|      - | 5621 | `	const ph7_io_stream **apOff;` |
|      - | 5622 | `	ph7_io_stream **apDev;` |
|      - | 5623 | `	const char *zScheme;` |
|     13 | 5624 | `	int nScheme,bBuiltin = 0,bChanged = 0,i;` |
|      - | 5625 | `	sxu32 n,nKeep;` |
|     13 | 5626 | `	if( nArg < 1 ){` |
|    ! 0 | 5627 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 5628 | `		return PH7_OK;` |
|      - | 5629 | `	}` |
|     13 | 5630 | `	zScheme = ph7_value_to_string(apArg[0],&nScheme);` |
|     13 | 5631 | `	apDev = (ph7_io_stream **)SySetBasePtr(&pVm->aIOstream);` |
|     85 | 5632 | `	for( n = 0 ; n < SySetUsed(&pVm->aIOstream) ; n++ ){` |
|     73 | 5633 | `		ph7_io_stream *pDev = apDev[n];` |
|     72 | 5634 | `		if( (int)SyStrlen(pDev->zName) != nScheme` |
|     54 | 5635 | `		 \|\| SyMemcmp(pDev->zName,zScheme,(sxu32)nScheme) != 0 ){` |
|     59 | 5636 | `			continue;` |
|      - | 5637 | `		}` |
|     15 | 5638 | `		if( UwrapIsSlotDevice(pDev) ){` |
|      - | 5639 | `			/* A userland wrapper standing in its place -- or one already` |
|      - | 5640 | `			 * withdrawn, which is still not a built-in. */` |
|      9 | 5641 | `			if( !PH7_VmStreamDeviceSuppressed(pVm,pDev) ){` |
|      5 | 5642 | `				bChanged = 1;` |
|      2 | 5643 | `			}` |
|      9 | 5644 | `			continue;` |
|      - | 5645 | `		}` |
|      7 | 5646 | `		bBuiltin = 1;` |
|      7 | 5647 | `		if( PH7_VmStreamDeviceSuppressed(pVm,pDev) ){` |
|      5 | 5648 | `			bChanged = 1;` |
|      2 | 5649 | `		}` |
|      4 | 5650 | `	}` |
|     13 | 5651 | `	if( !bBuiltin ){` |
|     10 | 5652 | `		ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,` |
|      3 | 5653 | `			"%.*s:// never existed, nothing to restore",nScheme,zScheme);` |
|      7 | 5654 | `		ph7_result_bool(pCtx,0);` |
|      7 | 5655 | `		return PH7_OK;` |
|      - | 5656 | `	}` |
|      7 | 5657 | `	if( !bChanged ){` |
|      - | 5658 | `		/* php answers TRUE here and says so at NOTICE level: the protocol is` |
|      - | 5659 | `		 * already the one it would restore. */` |
|      4 | 5660 | `		ph7_context_throw_error_format(pCtx,PH7_CTX_NOTICE,` |
|      1 | 5661 | `			"%.*s:// was never changed, nothing to restore",nScheme,zScheme);` |
|      3 | 5662 | `		ph7_result_bool(pCtx,1);` |
|      3 | 5663 | `		return PH7_OK;` |
|      - | 5664 | `	}` |
|      - | 5665 | `	/* Lift the suppression off the BUILT-IN first, by compacting the set... */` |
|      5 | 5666 | `	apOff = (const ph7_io_stream **)SySetBasePtr(&pVm->aSuppressedIo);` |
|      5 | 5667 | `	nKeep = 0;` |
|      9 | 5668 | `	for( n = 0 ; n < SySetUsed(&pVm->aSuppressedIo) ; n++ ){` |
|      5 | 5669 | `		const ph7_io_stream *pDev = apOff[n];` |
|      4 | 5670 | `		if( (int)SyStrlen(pDev->zName) == nScheme` |
|      4 | 5671 | `		 && SyMemcmp(pDev->zName,zScheme,(sxu32)nScheme) == 0` |
|      5 | 5672 | `		 && !UwrapIsSlotDevice(pDev) ){` |
|      5 | 5673 | `			continue; /* the built-in comes back */` |
|      - | 5674 | `		}` |
|    ! 0 | 5675 | `		apOff[nKeep++] = pDev;` |
|    ! 0 | 5676 | `	}` |
|      5 | 5677 | `	SySetTruncate(&pVm->aSuppressedIo,nKeep);` |
|      - | 5678 | `	/* ...then retire every userland wrapper standing in for the name. */` |
|     37 | 5679 | `	for( i = 0 ; i < PHL_UWRAP_MAX ; i++ ){` |
|     32 | 5680 | `		if( g_aUwrap[i].pVm == pVm` |
|     18 | 5681 | `		 && (int)SyStrlen(g_aUwrap[i].zScheme) == nScheme` |
|      5 | 5682 | `		 && SyMemcmp(g_aUwrap[i].zScheme,zScheme,(sxu32)nScheme) == 0 ){` |
|      5 | 5683 | `			const ph7_io_stream *pDead = &g_aUwrap[i].sStream;` |
|      5 | 5684 | `			g_aUwrap[i].pVm = 0;` |
|      5 | 5685 | `			if( !PH7_VmStreamDeviceSuppressed(pVm,pDead) ){` |
|      5 | 5686 | `				SySetPut(&pVm->aSuppressedIo,(const void *)&pDead);` |
|      2 | 5687 | `			}` |
|      2 | 5688 | `		}` |
|     17 | 5689 | `	}` |
|      5 | 5690 | `	ph7_result_bool(pCtx,1);` |
|      5 | 5691 | `	return PH7_OK;` |
|      7 | 5692 | `}` |
|      - | 5693 | `#ifdef PH7_ENABLE_NET` |
|      - | 5694 | `/*` |
|      - | 5695 | `` * php's socket address: `[transport://]host:port`. What a re-derivation gets`` |
|      - | 5696 | ` * wrong here is that BOTH halves have a diagnostic of their own, and neither is` |
|      - | 5697 | ` * the other: a transport this build does not carry is not a malformed address,` |
|      - | 5698 | ` * and an address with no port is not an unknown transport.` |
|      - | 5699 | ` */` |
|      - | 5700 | `#define SOCK_ADDR_OK        0` |
|      - | 5701 | `#define SOCK_ADDR_TRANSPORT 1 /* named a transport this build has not got */` |
|      - | 5702 | `#define SOCK_ADDR_PARSE     2 /* no port separator at all */` |
|      - | 5703 | `/*` |
|      - | 5704 | `` * php's port half is `atoi()` of whatever follows the FIRST colon, and the`` |
|      - | 5705 | ` * colon is looked for in every position but the LAST — which is the whole` |
|      - | 5706 | `` * difference between `127.0.0.1:` (php's "Failed to parse address") and`` |
|      - | 5707 | `` * `127.0.0.1:abc` (a port of 0, i.e. one the OS picks). A re-derivation that`` |
|      - | 5708 | ` * reads digits strictly refuses three addresses php accepts, and one that takes` |
|      - | 5709 | `` * the last colon reads `a:b:c` differently than php does.`` |
|      - | 5710 | ` */` |
|    208 | 5711 | `static int SockParsePort(const char *z,int n)` |
|      4 | 5712 | `{` |
|    212 | 5713 | `	int i = 0,iSign = 1,iVal = 0;` |
|    316 | 5714 | `	while( i < n && (z[i] == ' ' \|\| z[i] == '\t' \|\| z[i] == '\n' \|\| z[i] == '\r'` |
|    208 | 5715 | `	              \|\| z[i] == '\v' \|\| z[i] == '\f') ){` |
|    ! 0 | 5716 | `		i++;` |
|    ! 0 | 5717 | `	}` |
|    212 | 5718 | `	if( i < n && (z[i] == '+' \|\| z[i] == '-') ){` |
|    ! 0 | 5719 | `		iSign = z[i] == '-' ? -1 : 1;` |
|    ! 0 | 5720 | `		i++;` |
|    ! 0 | 5721 | `	}` |
|   1104 | 5722 | `	for( ; i < n && z[i] >= '0' && z[i] <= '9' ; i++ ){` |
|    895 | 5723 | `		if( iVal < 1000000000 ){` |
|    895 | 5724 | `			iVal = iVal * 10 + (z[i] - '0');` |
|    446 | 5725 | `		}` |
|    449 | 5726 | `	}` |
|    212 | 5727 | `	return iSign * iVal;` |
|      4 | 5728 | `}` |
|    204 | 5729 | `static int SockParseAddress(const char *zAddr,int nAddr,char *zHost,int nHostBuf,int *pPort,` |
|      - | 5730 | `	const char **pzTransport,int *pnTransport,const char **pzRest,int *pnRest)` |
|      4 | 5731 | `{` |
|    208 | 5732 | `	const char *zRest = zAddr;` |
|    208 | 5733 | `	int nRest = nAddr,i,nHost = -1;` |
|    208 | 5734 | `	*pPort = 0;` |
|    208 | 5735 | `	*pzTransport = "tcp";` |
|    208 | 5736 | `	*pnTransport = 3;` |
|   2016 | 5737 | `	for( i = 0 ; i + 2 < nAddr ; i++ ){` |
|   1890 | 5738 | `		if( zAddr[i] == ':' && zAddr[i+1] == '/' && zAddr[i+2] == '/' ){` |
|     82 | 5739 | `			*pzTransport = zAddr;` |
|     82 | 5740 | `			*pnTransport = i;` |
|     82 | 5741 | `			zRest = &zAddr[i+3];` |
|     82 | 5742 | `			nRest = nAddr - i - 3;` |
|     82 | 5743 | `			break;` |
|      - | 5744 | `		}` |
|    908 | 5745 | `	}` |
|    208 | 5746 | `	*pzRest = zRest;` |
|    208 | 5747 | `	*pnRest = nRest;` |
|    208 | 5748 | `	if( *pnTransport != 3 \|\| SyStrnicmp(*pzTransport,"tcp",3) != 0 ){` |
|      3 | 5749 | `		return SOCK_ADDR_TRANSPORT;` |
|      - | 5750 | `	}` |
|   1942 | 5751 | `	for( i = 0 ; i + 1 < nRest ; i++ ){` |
|   1936 | 5752 | `		if( zRest[i] == ':' ){` |
|    200 | 5753 | `			*pPort = SockParsePort(&zRest[i+1],nRest - i - 1);` |
|    200 | 5754 | `			nHost = i;` |
|    200 | 5755 | `			break;` |
|      - | 5756 | `		}` |
|    872 | 5757 | `	}` |
|    206 | 5758 | `	if( nHost < 0 ){` |
|      8 | 5759 | `		return SOCK_ADDR_PARSE;` |
|      - | 5760 | `	}` |
|    200 | 5761 | `	if( nHost >= nHostBuf ){` |
|    ! 0 | 5762 | `		nHost = nHostBuf - 1;` |
|    ! 0 | 5763 | `	}` |
|    200 | 5764 | `	if( nHost > 0 ){` |
|    196 | 5765 | `		SyMemcpy(zRest,zHost,(sxu32)nHost);` |
|     96 | 5766 | `	}` |
|    200 | 5767 | `	zHost[nHost] = 0;` |
|    200 | 5768 | `	return SOCK_ADDR_OK;` |
|    106 | 5769 | `}` |
|      - | 5770 | `/*` |
|      - | 5771 | ` * resource\|false fsockopen(string $hostname, int $port = -1, int &$error_code,` |
|      - | 5772 | ` *                          string &$error_message, ?float $timeout = null)` |
|      - | 5773 | ` * resource\|false stream_socket_client(string $address, int &$error_code,` |
|      - | 5774 | ` *                          string &$error_message, ?float $timeout = null, ...)` |
|      - | 5775 | ` * TCP only (the recorded scope: no ssl://, udp:// or unix:// yet).` |
|      - | 5776 | ` */` |
|    162 | 5777 | `PH7_PRIVATE int PH7_builtin_fsockopen(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      4 | 5778 | `{` |
|    166 | 5779 | `	const char *zFunc = ph7_function_name(pCtx);` |
|    166 | 5780 | `	int bClientForm = (zFunc[0] == 's'); /* stream_socket_client */` |
|    166 | 5781 | `	const char *zRaw,*zAddr,*zTransport,*zRest,*zErr = "";` |
|      - | 5782 | `	char zHost[256],zAddrBuf[352],zShowBuf[384],zMsg[512];` |
|      - | 5783 | `	const char *zShow;` |
|    166 | 5784 | `	int nRaw,nAddr,nShow,nTransport,nRest,iPortArg = -1,iPort = 0,iErrno = 0,iTimeoutMs = 0,rc;` |
|    166 | 5785 | `	int iFlags = PH7_STREAM_CLIENT_CONNECT,bPersist,bConnect;` |
|      - | 5786 | `	ph7_socket sock;` |
|      - | 5787 | `	io_private *pDev;` |
|    166 | 5788 | `	int iArgErrno = bClientForm ? 1 : 2;` |
|    166 | 5789 | `	int iArgErrstr = bClientForm ? 2 : 3;` |
|    166 | 5790 | `	int iArgTimeout = bClientForm ? 3 : 4;` |
|    166 | 5791 | `	phl_stream_ctx *pCtxRes = 0;` |
|      - | 5792 | `	ph7_sockopts sOpt;` |
|      - | 5793 | `	char zBindHost[256];` |
|    166 | 5794 | `	int bThrew = 0;` |
|    166 | 5795 | `	if( nArg < 1 ){` |
|    ! 0 | 5796 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 5797 | `		return PH7_OK;` |
|      - | 5798 | `	}` |
|    166 | 5799 | `	if( bClientForm ){` |
|      - | 5800 | ``		/* php's `?resource $context` — fsockopen()/pfsockopen() have no such`` |
|      - | 5801 | `		 * argument, so only the stream_socket_client() spelling takes one. */` |
|     46 | 5802 | `		pCtxRes = PH7_StreamCtxFromArg(pCtx,nArg,apArg,5,"$context",0,&bThrew);` |
|     46 | 5803 | `		if( bThrew ){` |
|    ! 0 | 5804 | `			return PH7_OK;` |
|      - | 5805 | `		}` |
|     21 | 5806 | `	}` |
|    166 | 5807 | `	zRaw = ph7_value_to_string(apArg[0],&nRaw);` |
|    166 | 5808 | `	if( !bClientForm && nArg > 1 && !ph7_value_is_null(apArg[1]) ){` |
|    121 | 5809 | `		iPortArg = ph7_value_to_int(apArg[1]);` |
|     60 | 5810 | `	}` |
|    166 | 5811 | `	if( bClientForm && nArg > 4 ){` |
|      - | 5812 | `		/* Declared in the signature and read by nothing until now, so the` |
|      - | 5813 | `		 * documented spellings did nothing and their constants were undefined` |
|      - | 5814 | `		 * fatals. */` |
|     26 | 5815 | `		iFlags = (int)ph7_value_to_int64(apArg[4]);` |
|     12 | 5816 | `	}` |
|      - | 5817 | `	/* pfsockopen() IS fsockopen() with this flag; php has no other difference` |
|      - | 5818 | `	 * between them. ASYNC_CONNECT is accepted and changes nothing here, because` |
|      - | 5819 | `	 * the connect() is blocking either way (§7.4 slice-2 (b)) — php reverts a` |
|      - | 5820 | `	 * socket it connected asynchronously to blocking mode too. */` |
|    187 | 5821 | `	bPersist = bClientForm ? (iFlags & PH7_STREAM_CLIENT_PERSISTENT) != 0` |
|    141 | 5822 | `		: (zFunc[0] == 'p');` |
|    166 | 5823 | `	bConnect = bClientForm ? (iFlags & PH7_STREAM_CLIENT_CONNECT) != 0 : 1;` |
|      - | 5824 | `	/* php builds ONE address out of fsockopen()'s two arguments — and only when` |
|      - | 5825 | ``	 * the port is a usable one, which is why `fsockopen($h)` reports the address`` |
|      - | 5826 | `	 * it could not parse rather than connecting to port 0. The address it SHOWS` |
|      - | 5827 | `	 * keeps the port either way. */` |
|    166 | 5828 | `	if( bClientForm \|\| iPortArg <= 0 ){` |
|     46 | 5829 | `		zAddr = zRaw;` |
|     46 | 5830 | `		nAddr = nRaw;` |
|     25 | 5831 | `	}else{` |
|    121 | 5832 | `		nAddr = (int)SyBufferFormat(zAddrBuf,sizeof(zAddrBuf),"%.*s:%d",nRaw,zRaw,iPortArg);` |
|    121 | 5833 | `		zAddr = zAddrBuf;` |
|      - | 5834 | `	}` |
|    166 | 5835 | `	if( bClientForm ){` |
|     46 | 5836 | `		zShow = zRaw;` |
|     46 | 5837 | `		nShow = nRaw;` |
|     25 | 5838 | `	}else{` |
|    121 | 5839 | `		nShow = (int)SyBufferFormat(zShowBuf,sizeof(zShowBuf),"%.*s:%d",nRaw,zRaw,iPortArg);` |
|    121 | 5840 | `		zShow = zShowBuf;` |
|      - | 5841 | `	}` |
|    166 | 5842 | `	rc = SockParseAddress(zAddr,nAddr,zHost,(int)sizeof(zHost),&iPort,&zTransport,&nTransport,` |
|      - | 5843 | `		&zRest,&nRest);` |
|    166 | 5844 | `	if( rc != SOCK_ADDR_OK ){` |
|    ! 0 | 5845 | `		if( rc == SOCK_ADDR_TRANSPORT ){` |
|      - | 5846 | `			/* php's own wording for a transport its build does not carry —` |
|      - | 5847 | `			 * which is what this engine's missing ones ARE (§7.4), and what a` |
|      - | 5848 | `			 * script reading $errstr is written against. This used to spell a` |
|      - | 5849 | `			 * message of PHL's own that no php ever answers. */` |
|    ! 0 | 5850 | `			SyBufferFormat(zMsg,sizeof(zMsg),` |
|      - | 5851 | `				"Unable to find the socket transport \"%.*s\" - did you forget to enable it when you configured PHP?",` |
|    ! 0 | 5852 | `				nTransport,zTransport);` |
|    ! 0 | 5853 | `		}else{` |
|    ! 0 | 5854 | `			SyBufferFormat(zMsg,sizeof(zMsg),"Failed to parse address \"%.*s\"",nRest,zRest);` |
|      - | 5855 | `		}` |
|    ! 0 | 5856 | `		SockAddressFailure(pCtx,apArg,nArg,iArgErrno,iArgErrstr,zShow,nShow,zMsg,0);` |
|    ! 0 | 5857 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 5858 | `		return PH7_OK;` |
|      - | 5859 | `	}` |
|    166 | 5860 | `	if( nArg > iArgTimeout && !ph7_value_is_null(apArg[iArgTimeout]) ){` |
|    154 | 5861 | `		double rTimeout = ph7_value_to_double(apArg[iArgTimeout]);` |
|    154 | 5862 | `		if( rTimeout > 0 ){` |
|    154 | 5863 | `			iTimeoutMs = (int)(rTimeout * 1000);` |
|     75 | 5864 | `		}` |
|     75 | 5865 | `	}` |
|    166 | 5866 | `	if( bPersist ){` |
|      - | 5867 | `		/* A live one for this address IS the answer: php hands the same resource` |
|      - | 5868 | `		 * back rather than opening a second connection to the same peer. */` |
|      - | 5869 | `		char zKey[320];` |
|      - | 5870 | `		io_private *pKept;` |
|     17 | 5871 | `		SockPersistKey(zKey,sizeof(zKey),bClientForm,zAddr,nAddr);` |
|     17 | 5872 | `		pKept = SockPersistFind(pCtx->pVm,zKey);` |
|     17 | 5873 | `		if( pKept ){` |
|      7 | 5874 | `			SockAddressSuccess(pCtx,apArg,nArg,iArgErrno,iArgErrstr);` |
|      7 | 5875 | `			ph7_result_resource(pCtx,pKept);` |
|      7 | 5876 | `			return PH7_OK;` |
|      - | 5877 | `		}` |
|      5 | 5878 | `	}` |
|    160 | 5879 | `	if( !bConnect ){` |
|      - | 5880 | `		/* php creates the socket while CONNECTING it, so a $flags without` |
|      - | 5881 | `		 * STREAM_CLIENT_CONNECT answers a stream with no socket behind it: no` |
|      - | 5882 | `		 * name at either end, reads false, writes 0, already at end of file. */` |
|      3 | 5883 | `		SockAddressSuccess(pCtx,apArg,nArg,iArgErrno,iArgErrstr);` |
|      3 | 5884 | `		pDev = SockWrapSocket(pCtx,PH7_NET_INVALID_SOCKET,zAddr,nAddr);` |
|      3 | 5885 | `		if( pDev == 0 ){` |
|    ! 0 | 5886 | `			ph7_result_bool(pCtx,0);` |
|    ! 0 | 5887 | `			return PH7_OK;` |
|      - | 5888 | `		}` |
|      3 | 5889 | `		pDev->pCtxRes = (void *)pCtxRes;` |
|      3 | 5890 | `		ph7_result_resource(pCtx,pDev);` |
|      3 | 5891 | `		return PH7_OK;` |
|      - | 5892 | `	}` |
|      - | 5893 | `	{` |
|      - | 5894 | ``		/* php reads the `socket` options at the moment it creates the socket:`` |
|      - | 5895 | `		 * so_reuseport and tcp_nodelay are a setsockopt on the fresh one, and` |
|      - | 5896 | `		 * bindto is the LOCAL address it takes before connecting. */` |
|    158 | 5897 | `		const char *zOptErr = 0;` |
|    158 | 5898 | `		if( SockCtxOptions(pCtxRes,&sOpt,zBindHost,(int)sizeof(zBindHost),&zOptErr) != 0 ){` |
|      - | 5899 | `			/* The one option failure php treats as a failed CONNECT rather than` |
|      - | 5900 | `			 * as a warning it can carry on past. */` |
|      3 | 5901 | `			SockAddressFailure(pCtx,apArg,nArg,iArgErrno,iArgErrstr,zShow,nShow,zOptErr,0);` |
|      3 | 5902 | `			ph7_result_bool(pCtx,0);` |
|      3 | 5903 | `			return PH7_OK;` |
|      - | 5904 | `		}` |
|      - | 5905 | `	}` |
|    156 | 5906 | `	sock = PH7_NetConnect(zHost,iPort,iTimeoutMs,&sOpt,&iErrno,&zErr);` |
|    156 | 5907 | `	if( sOpt.iBindErr ){` |
|      - | 5908 | `		/* php's own wording, and NEITHER shape stops the connection: the socket` |
|      - | 5909 | `		 * goes out from wherever the routing table would have sent it. It tells` |
|      - | 5910 | `		 * the two apart — a local address that is not a numeric literal at all` |
|      - | 5911 | `		 * names the host, one the OS refused to BIND names the address it tried` |
|      - | 5912 | `		 * and the reason. */` |
|      9 | 5913 | `		if( sOpt.iBindErr == PH7_SOCKOPT_BIND_RESOLVE ){` |
|      5 | 5914 | `			ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,"Invalid IP Address: %s",` |
|      4 | 5915 | `				sOpt.zBindHost ? sOpt.zBindHost : "");` |
|      3 | 5916 | `		}else{` |
|      - | 5917 | `			/* php RE-COMPOSES the address it tried from the parts it parsed, so` |
|      - | 5918 | ``			 * the quoted spelling is canonical: a `bindto` of "192.0.2.1:007"`` |
|      - | 5919 | `			 * is reported as '192.0.2.1:7'. */` |
|      5 | 5920 | `			ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,` |
|      - | 5921 | `				"Failed to bind to '%s:%d', system said: %s",` |
|      4 | 5922 | `				sOpt.zBindHost ? sOpt.zBindHost : "",sOpt.iBindPort,` |
|      2 | 5923 | `				PH7_NetStrError(sOpt.iBindErrno));` |
|      - | 5924 | `		}` |
|      4 | 5925 | `	}` |
|    156 | 5926 | `	if( sock == PH7_NET_INVALID_SOCKET ){` |
|     73 | 5927 | `		if( iErrno == PH7_NET_ERR_RESOLVE ){` |
|      3 | 5928 | `			zErr = SockResolveFailure(pCtx,zHost,zMsg,(int)sizeof(zMsg));` |
|      3 | 5929 | `			iErrno = 0;` |
|      1 | 5930 | `		}` |
|     73 | 5931 | `		SockAddressFailure(pCtx,apArg,nArg,iArgErrno,iArgErrstr,zShow,nShow,zErr,iErrno);` |
|     73 | 5932 | `		ph7_result_bool(pCtx,0);` |
|     73 | 5933 | `		return PH7_OK;` |
|      - | 5934 | `	}` |
|     83 | 5935 | `	SockAddressSuccess(pCtx,apArg,nArg,iArgErrno,iArgErrstr);` |
|      - | 5936 | `	/* Wrap the socket in an io_private so the whole f* family works on it. php` |
|      - | 5937 | ``	 * reports the ADDRESS it opened as the handle's `uri`, which is the same`` |
|      - | 5938 | `	 * one-address-out-of-two-arguments composition it connected through — so an` |
|      - | 5939 | `	 * argument naming only a host still records the port beside it. */` |
|     83 | 5940 | `	pDev = SockWrapSocket(pCtx,sock,zAddr,nAddr);` |
|     83 | 5941 | `	if( pDev == 0 ){` |
|    ! 0 | 5942 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 5943 | `		return PH7_OK;` |
|      - | 5944 | `	}` |
|      - | 5945 | `	/* php attaches the opener's context to a TRANSPORT stream and to nothing` |
|      - | 5946 | `	 * else — which is why stream_context_get_options() answers for a socket and` |
|      - | 5947 | `	 * answers the empty set for a file opened through the very same call. */` |
|     83 | 5948 | `	pDev->pCtxRes = (void *)pCtxRes;` |
|     83 | 5949 | `	SockArmDefaultTimeout(pCtx,pDev);` |
|     83 | 5950 | `	if( bPersist ){` |
|      - | 5951 | `		char zKey[320];` |
|     11 | 5952 | `		SockPersistKey(zKey,sizeof(zKey),bClientForm,zAddr,nAddr);` |
|     11 | 5953 | `		SockPersistKeep(pCtx->pVm,zKey,pDev);` |
|      - | 5954 | `		/* get_resource_type() names it apart, which is how a script can tell it` |
|      - | 5955 | `		 * asked for one at all. */` |
|     11 | 5956 | `		pDev->bPersist = 1;` |
|      5 | 5957 | `	}` |
|     83 | 5958 | `	ph7_result_resource(pCtx,pDev);` |
|     83 | 5959 | `	return PH7_OK;` |
|     85 | 5960 | `}` |
|      - | 5961 | `/*` |
|      - | 5962 | ` * resource\|false stream_socket_server(string $address, int &$error_code,` |
|      - | 5963 | ` *                    string &$error_message, int $flags = STREAM_SERVER_BIND\|STREAM_SERVER_LISTEN,` |
|      - | 5964 | ` *                    ?resource $context = null)` |
|      - | 5965 | ` *` |
|      - | 5966 | ` * The name a php program becomes a SERVER through, and a loud` |
|      - | 5967 | `` * `Call to undefined function` until now — so a script that listens on a port`` |
|      - | 5968 | ` * (a test double, a job runner, a line protocol) could not be spelled at all,` |
|      - | 5969 | ` * even though net.c had bind() and listen() all along.` |
|      - | 5970 | ` *` |
|      - | 5971 | ` * php's two flags are separate for a reason: BIND alone is what a datagram` |
|      - | 5972 | ` * socket wants (there is nothing to listen for), so LISTEN is what makes the` |
|      - | 5973 | ` * socket a stream server. Dropping LISTEN from a tcp:// address is therefore` |
|      - | 5974 | ` * a bound socket nothing can connect to, which is exactly what php answers.` |
|      - | 5975 | ` */` |
|     42 | 5976 | `PH7_PRIVATE int PH7_builtin_stream_socket_server(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      4 | 5977 | `{` |
|     46 | 5978 | `	const char *zAddr,*zTransport,*zRest,*zErr = "";` |
|      - | 5979 | `	char zHost[256];` |
|     46 | 5980 | `	int nAddr,nTransport,nRest,iPort = -1,iErrno = 0,iFlags,rc;` |
|      - | 5981 | `	ph7_socket sock;` |
|      - | 5982 | `	io_private *pDev;` |
|      - | 5983 | `	phl_stream_ctx *pCtxRes;` |
|      - | 5984 | `	ph7_sockopts sOpt;` |
|      - | 5985 | `	char zBindHost[256];` |
|     46 | 5986 | `	int bThrew = 0;` |
|     46 | 5987 | `	if( nArg < 1 ){` |
|    ! 0 | 5988 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 5989 | `		return PH7_OK;` |
|      - | 5990 | `	}` |
|     46 | 5991 | `	pCtxRes = PH7_StreamCtxFromArg(pCtx,nArg,apArg,4,"$context",0,&bThrew);` |
|     46 | 5992 | `	if( bThrew ){` |
|    ! 0 | 5993 | `		return PH7_OK;` |
|      - | 5994 | `	}` |
|      - | 5995 | ``	/* The signature row declares `string $address`, so whatever arrives has`` |
|      - | 5996 | `	 * already been screened; php's own ZPP then CASTS it, and refusing an int` |
|      - | 5997 | `` 	 * here would answer false in silence for `stream_socket_server(8080)` `` |
|      - | 5998 | `	 * where php reports the address it could not parse. */` |
|     46 | 5999 | `	zAddr = ph7_value_to_string(apArg[0],&nAddr);` |
|     30 | 6000 | `	iFlags = nArg > 3 ? (int)ph7_value_to_int64(apArg[3])` |
|     26 | 6001 | `		: (PH7_STREAM_SERVER_BIND\|PH7_STREAM_SERVER_LISTEN);` |
|     46 | 6002 | `	rc = SockParseAddress(zAddr,nAddr,zHost,(int)sizeof(zHost),&iPort,&zTransport,&nTransport,` |
|      - | 6003 | `		&zRest,&nRest);` |
|     46 | 6004 | `	if( rc != SOCK_ADDR_OK ){` |
|      - | 6005 | `		char zMsg[512];` |
|     10 | 6006 | `		if( rc == SOCK_ADDR_TRANSPORT ){` |
|      - | 6007 | `			/* php's own wording for a transport its build has not got, which is` |
|      - | 6008 | `			 * what udp://, unix:// and ssl:// are here (§7.4). */` |
|      4 | 6009 | `			SyBufferFormat(zMsg,sizeof(zMsg),` |
|      - | 6010 | `				"Unable to find the socket transport \"%.*s\" - did you forget to enable it when you configured PHP?",` |
|      1 | 6011 | `				nTransport,zTransport);` |
|      2 | 6012 | `		}else{` |
|      8 | 6013 | `			SyBufferFormat(zMsg,sizeof(zMsg),"Failed to parse address \"%.*s\"",nRest,zRest);` |
|      - | 6014 | `		}` |
|     10 | 6015 | `		SockAddressFailure(pCtx,apArg,nArg,1,2,zAddr,nAddr,zMsg,0);` |
|     10 | 6016 | `		ph7_result_bool(pCtx,0);` |
|     10 | 6017 | `		return PH7_OK;` |
|      - | 6018 | `	}` |
|     38 | 6019 | `	if( (iFlags & PH7_STREAM_SERVER_BIND) == 0 ){` |
|      - | 6020 | `		/* php creates the socket while BINDING it, so a $flags without` |
|      - | 6021 | `		 * STREAM_SERVER_BIND answers a socket stream with no socket behind it:` |
|      - | 6022 | `		 * it has no name, reads false, writes 0 and is already at end of file.` |
|      - | 6023 | ``		 * It does not even resolve the host — `stream_socket_server(':1', $e,`` |
|      - | 6024 | ``		 * $es, 0)` is a resource in php. */`` |
|      5 | 6025 | `		pDev = SockWrapSocket(pCtx,PH7_NET_INVALID_SOCKET,zAddr,nAddr);` |
|      5 | 6026 | `		if( pDev == 0 ){` |
|    ! 0 | 6027 | `			ph7_result_bool(pCtx,0);` |
|    ! 0 | 6028 | `			return PH7_OK;` |
|      - | 6029 | `		}` |
|      5 | 6030 | `		pDev->pCtxRes = (void *)pCtxRes;` |
|      5 | 6031 | `		SockAddressSuccess(pCtx,apArg,nArg,1,2);` |
|      5 | 6032 | `		ph7_result_resource(pCtx,pDev);` |
|      5 | 6033 | `		return PH7_OK;` |
|      - | 6034 | `	}` |
|     34 | 6035 | `	if( zHost[0] == 0 ){` |
|      - | 6036 | ``		/* An address with no host at all (`:8080`) is a name php asks the`` |
|      - | 6037 | `		 * resolver about and is refused for — NOT a wildcard bind. Answering` |
|      - | 6038 | `		 * 0.0.0.0 for it would put a listener on every interface of the` |
|      - | 6039 | `		 * machine, which is the unsafe direction. */` |
|      - | 6040 | `		char zMsg[512];` |
|      3 | 6041 | `		SockAddressFailure(pCtx,apArg,nArg,1,2,zAddr,nAddr,` |
|      1 | 6042 | `			SockResolveFailure(pCtx,zHost,zMsg,(int)sizeof(zMsg)),0);` |
|      2 | 6043 | `		ph7_result_bool(pCtx,0);` |
|      2 | 6044 | `		return PH7_OK;` |
|      - | 6045 | `	}` |
|      - | 6046 | `	{` |
|      - | 6047 | ``		/* The server half reads `backlog`, `so_reuseport` and `tcp_nodelay`;`` |
|      - | 6048 | ``		 * `bindto` is not one of its options, because the address argument IS`` |
|      - | 6049 | `		 * where a server binds (php ignores it here too). */` |
|     32 | 6050 | `		const char *zOptErr = 0;` |
|     32 | 6051 | `		if( SockCtxOptions(pCtxRes,&sOpt,zBindHost,(int)sizeof(zBindHost),&zOptErr) != 0 ){` |
|    ! 0 | 6052 | `			SockAddressFailure(pCtx,apArg,nArg,1,2,zAddr,nAddr,zOptErr,0);` |
|    ! 0 | 6053 | `			ph7_result_bool(pCtx,0);` |
|    ! 0 | 6054 | `			return PH7_OK;` |
|      - | 6055 | `		}` |
|     32 | 6056 | `		sOpt.zBindHost = 0;` |
|      - | 6057 | `	}` |
|     32 | 6058 | `	sock = PH7_NetBind(zHost,iPort,0,(iFlags & PH7_STREAM_SERVER_LISTEN) != 0,` |
|      - | 6059 | `		SOCK_LISTEN_BACKLOG,&sOpt,&iErrno,&zErr);` |
|     32 | 6060 | `	if( sock == PH7_NET_INVALID_SOCKET ){` |
|      - | 6061 | `		char zMsg[512];` |
|      5 | 6062 | `		if( iErrno == PH7_NET_ERR_RESOLVE ){` |
|      3 | 6063 | `			zErr = SockResolveFailure(pCtx,zHost,zMsg,(int)sizeof(zMsg));` |
|      1 | 6064 | `		}` |
|      - | 6065 | `		/* php reports no OS code for a refused ADDRESS — only a connect() that` |
|      - | 6066 | `		 * reached the network carries one — so this stays 0 for every arm. */` |
|      5 | 6067 | `		SockAddressFailure(pCtx,apArg,nArg,1,2,zAddr,nAddr,zErr,0);` |
|      5 | 6068 | `		ph7_result_bool(pCtx,0);` |
|      5 | 6069 | `		return PH7_OK;` |
|      - | 6070 | `	}` |
|     27 | 6071 | `	SockAddressSuccess(pCtx,apArg,nArg,1,2);` |
|     27 | 6072 | `	pDev = SockWrapSocket(pCtx,sock,zAddr,nAddr);` |
|     27 | 6073 | `	if( pDev == 0 ){` |
|    ! 0 | 6074 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 6075 | `		return PH7_OK;` |
|      - | 6076 | `	}` |
|     27 | 6077 | `	pDev->pCtxRes = (void *)pCtxRes;` |
|     27 | 6078 | `	ph7_result_resource(pCtx,pDev);` |
|     27 | 6079 | `	return PH7_OK;` |
|     25 | 6080 | `}` |
|      - | 6081 | `/*` |
|      - | 6082 | ` * resource\|false stream_socket_accept(resource $socket, ?float $timeout = null,` |
|      - | 6083 | ` *                                    string &$peer_name = null)` |
|      - | 6084 | ` *` |
|      - | 6085 | ` * The other half of a server, and the one with the timing in it. php waits at` |
|      - | 6086 | `` * most `default_socket_timeout` seconds by default — NOT forever — and reports`` |
|      - | 6087 | ` * an expired wait as a warning plus false, which is what lets a single-threaded` |
|      - | 6088 | ` * server do something else between connections. A negative timeout blocks.` |
|      - | 6089 | ` */` |
|     32 | 6090 | `PH7_PRIVATE int PH7_builtin_stream_socket_accept(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      4 | 6091 | `{` |
|      - | 6092 | `	io_private *pDev,*pOut;` |
|      - | 6093 | `	ph7_socket *pSock,sock;` |
|      - | 6094 | `	char zPeer[128];` |
|     36 | 6095 | `	int rc,bTimedOut = 0,iTimeoutMs;` |
|     36 | 6096 | `	if( nArg < 1 ){` |
|    ! 0 | 6097 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 6098 | `		return PH7_OK;` |
|      - | 6099 | `	}` |
|     36 | 6100 | `	pDev = StreamSettingArgNamed(pCtx,apArg[0],1,"socket",&rc);` |
|     36 | 6101 | `	if( pDev == 0 ){` |
|      3 | 6102 | `		return rc;` |
|      - | 6103 | `	}` |
|     34 | 6104 | `	pSock = IoPrivateSocket(pDev);` |
|     34 | 6105 | `	if( pSock == 0 ){` |
|      - | 6106 | `		/* Not a socket at all. php's own answer for it reads oddly and is what` |
|      - | 6107 | `		 * a script sees: the accept never reaches the network, so there is no` |
|      - | 6108 | `		 * OS error to report and php asks its error table for code 0. */` |
|      3 | 6109 | `		ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"Accept failed: Unknown error");` |
|      3 | 6110 | `		ph7_result_bool(pCtx,0);` |
|      3 | 6111 | `		return PH7_OK;` |
|      - | 6112 | `	}` |
|     31 | 6113 | `	if( nArg > 1 && !ph7_value_is_null(apArg[1]) ){` |
|     31 | 6114 | `		double rTimeout = ph7_value_to_double(apArg[1]);` |
|     31 | 6115 | `		iTimeoutMs = rTimeout < 0 ? -1 : (int)(rTimeout * 1000);` |
|     17 | 6116 | `	}else{` |
|    ! 0 | 6117 | `		iTimeoutMs = (int)(PH7_VmIniGetInt(pCtx->pVm,"default_socket_timeout",60) * 1000);` |
|    ! 0 | 6118 | `		if( iTimeoutMs < 0 ){` |
|    ! 0 | 6119 | `			iTimeoutMs = -1;` |
|    ! 0 | 6120 | `		}` |
|      - | 6121 | `	}` |
|     31 | 6122 | `	sock = PH7_NetAcceptTimed(*pSock,iTimeoutMs,&bTimedOut,zPeer,(int)sizeof(zPeer));` |
|     31 | 6123 | `	if( *pSock == PH7_NET_INVALID_SOCKET ){` |
|      - | 6124 | `		/* php waits and then reports the expiry; there is nothing to wait on. */` |
|      3 | 6125 | `		bTimedOut = 1;` |
|      1 | 6126 | `	}` |
|     31 | 6127 | `	if( sock == PH7_NET_INVALID_SOCKET ){` |
|      8 | 6128 | `		ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,"Accept failed: %s",` |
|      4 | 6129 | `			bTimedOut ? "Connection timed out" : PH7_NetStrError(PH7_NetLastError()));` |
|      6 | 6130 | `		ph7_result_bool(pCtx,0);` |
|      6 | 6131 | `		return PH7_OK;` |
|      - | 6132 | `	}` |
|     27 | 6133 | `	if( nArg > 2 ){` |
|      3 | 6134 | `		ph7_value *pTmp = ph7_context_new_scalar(pCtx);` |
|      3 | 6135 | `		if( pTmp ){` |
|      3 | 6136 | `			ph7_value_string(pTmp,zPeer,-1);` |
|      3 | 6137 | `			PH7_VmStoreArgByRef(pCtx->pVm,apArg[2],pTmp);` |
|      1 | 6138 | `		}` |
|      1 | 6139 | `	}` |
|      - | 6140 | ``	/* php reports no `uri` for an ACCEPTED connection: nothing opened it by`` |
|      - | 6141 | `	 * name, so stream_get_meta_data() has no address to answer with. */` |
|     27 | 6142 | `	pOut = SockWrapSocket(pCtx,sock,0,0);` |
|     27 | 6143 | `	if( pOut == 0 ){` |
|    ! 0 | 6144 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 6145 | `		return PH7_OK;` |
|      - | 6146 | `	}` |
|     27 | 6147 | `	SockArmDefaultTimeout(pCtx,pOut);` |
|     27 | 6148 | `	ph7_result_resource(pCtx,pOut);` |
|     27 | 6149 | `	return PH7_OK;` |
|     20 | 6150 | `}` |
|      - | 6151 | `/*` |
|      - | 6152 | `` * recvfrom()'s `&$address`: the sender for a datagram, empty for a connected`` |
|      - | 6153 | ` * stream that has none, and NULL for a read that did not happen — php writes it` |
|      - | 6154 | ` * on every call rather than leaving the caller's previous value in place.` |
|      - | 6155 | ` */` |
|     10 | 6156 | `static void SockStoreAddress(ph7_context *pCtx,ph7_value **apArg,int nArg,int iArg,const char *zAddr)` |
|      1 | 6157 | `{` |
|      - | 6158 | `	ph7_value *pTmp;` |
|     11 | 6159 | `	if( iArg >= nArg ){` |
|      9 | 6160 | `		return;` |
|      - | 6161 | `	}` |
|      3 | 6162 | `	pTmp = ph7_context_new_scalar(pCtx);` |
|      3 | 6163 | `	if( pTmp == 0 ){` |
|    ! 0 | 6164 | `		return;` |
|      - | 6165 | `	}` |
|      3 | 6166 | `	if( zAddr ){` |
|      3 | 6167 | `		ph7_value_string(pTmp,zAddr,-1);` |
|      2 | 6168 | `	}else{` |
|    ! 0 | 6169 | `		ph7_value_null(pTmp);` |
|      - | 6170 | `	}` |
|      3 | 6171 | `	PH7_VmStoreArgByRef(pCtx->pVm,apArg[iArg],pTmp);` |
|      6 | 6172 | `}` |
|      - | 6173 | `/*` |
|      - | 6174 | ` * bool stream_socket_shutdown(resource $stream, int $mode)` |
|      - | 6175 | ` *` |
|      - | 6176 | ` * The half-close: "I am done SENDING" without closing a handle the program` |
|      - | 6177 | ` * still wants to read from, which is how every request/response protocol tells` |
|      - | 6178 | ` * its peer the request is over. Nothing else can say it — fclose() takes the` |
|      - | 6179 | ` * read side with it.` |
|      - | 6180 | ` */` |
|      8 | 6181 | `PH7_PRIVATE int PH7_builtin_stream_socket_shutdown(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 6182 | `{` |
|      - | 6183 | `	io_private *pDev;` |
|      - | 6184 | `	ph7_socket *pSock;` |
|      - | 6185 | `	ph7_int64 iHow;` |
|      - | 6186 | `	int rc;` |
|      9 | 6187 | `	if( nArg < 2 ){` |
|    ! 0 | 6188 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 6189 | `		return PH7_OK;` |
|      - | 6190 | `	}` |
|      9 | 6191 | `	pDev = StreamSettingArgNamed(pCtx,apArg[0],1,"stream",&rc);` |
|      9 | 6192 | `	if( pDev == 0 ){` |
|    ! 0 | 6193 | `		return rc;` |
|      - | 6194 | `	}` |
|      9 | 6195 | `	iHow = ph7_value_to_int64(apArg[1]);` |
|      9 | 6196 | `	if( iHow != PH7_STREAM_SHUT_RD && iHow != PH7_STREAM_SHUT_WR && iHow != PH7_STREAM_SHUT_RDWR ){` |
|      - | 6197 | `		/* php names the three constants rather than the numbers behind them. */` |
|      4 | 6198 | `		return PH7_VmThrowException(pCtx,"ValueError",` |
|      - | 6199 | `			"%s(): Argument #2 ($mode) must be one of STREAM_SHUT_RD, STREAM_SHUT_WR, or STREAM_SHUT_RDWR",` |
|      1 | 6200 | `			ph7_function_name(pCtx));` |
|      - | 6201 | `	}` |
|      7 | 6202 | `	pSock = IoPrivateSocket(pDev);` |
|      7 | 6203 | `	if( pSock == 0 \|\| *pSock == PH7_NET_INVALID_SOCKET ){` |
|      - | 6204 | `		/* Not a socket: php answers false in silence, since there is no` |
|      - | 6205 | `		 * direction to shut down. */` |
|      3 | 6206 | `		ph7_result_bool(pCtx,0);` |
|      3 | 6207 | `		return PH7_OK;` |
|      - | 6208 | `	}` |
|      5 | 6209 | `	rc = PH7_NetShutdown(*pSock,(int)iHow) == PH7_OK;` |
|      5 | 6210 | `	if( rc && iHow != PH7_STREAM_SHUT_WR && PH7_NetAtEnd(*pSock) ){` |
|      - | 6211 | `		/* The read side is gone AND nothing is queued behind it, so this handle` |
|      - | 6212 | `		 * is at its end: php answers feof() for a socket by probing it, and a` |
|      - | 6213 | ``		 * `while (!feof($s))` drain loop after a half-close would otherwise spin`` |
|      - | 6214 | `		 * on a stream that can never answer again. Bytes that HAD arrived are` |
|      - | 6215 | `		 * still handed over — which is why the answer is probed rather than` |
|      - | 6216 | `		 * assumed, and why the device's own latch stays clear. */` |
|      3 | 6217 | `		pDev->bEof = 1;` |
|      1 | 6218 | `	}` |
|      5 | 6219 | `	ph7_result_bool(pCtx,rc);` |
|      5 | 6220 | `	return PH7_OK;` |
|      5 | 6221 | `}` |
|      - | 6222 | `/*` |
|      - | 6223 | ` * string\|false stream_socket_recvfrom(resource $socket, int $length, int $flags = 0,` |
|      - | 6224 | ` *                                    string &$address = null)` |
|      - | 6225 | ` * int\|false stream_socket_sendto(resource $socket, string $data, int $flags = 0,` |
|      - | 6226 | ` *                               string $address = "")` |
|      - | 6227 | ` *` |
|      - | 6228 | ` * The pair that reaches the socket UNDERNEATH the stream: php's own asks the` |
|      - | 6229 | `` * socket rather than the handle's read buffer, which is why `STREAM_PEEK` can`` |
|      - | 6230 | ` * look at bytes without consuming them (nothing else in the family can) and why` |
|      - | 6231 | ` * a recvfrom() on a handle a line read has already buffered WAITS for more.` |
|      - | 6232 | `` * The `$address` is what a datagram carries and a connected stream does not.`` |
|      - | 6233 | ` */` |
|     12 | 6234 | `PH7_PRIVATE int PH7_builtin_stream_socket_recvfrom(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 6235 | `{` |
|      - | 6236 | `	io_private *pDev;` |
|      - | 6237 | `	ph7_socket *pSock;` |
|      - | 6238 | `	ph7_int64 nLen;` |
|      - | 6239 | `	char zAddr[128],*zBuf;` |
|     13 | 6240 | `	int rc,iFlags = 0,n;` |
|     13 | 6241 | `	if( nArg < 2 ){` |
|    ! 0 | 6242 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 6243 | `		return PH7_OK;` |
|      - | 6244 | `	}` |
|     13 | 6245 | `	pDev = StreamSettingArgNamed(pCtx,apArg[0],1,"socket",&rc);` |
|     13 | 6246 | `	if( pDev == 0 ){` |
|    ! 0 | 6247 | `		return rc;` |
|      - | 6248 | `	}` |
|     13 | 6249 | `	nLen = ph7_value_to_int64(apArg[1]);` |
|     13 | 6250 | `	if( nLen < 1 ){` |
|      4 | 6251 | `		return PH7_VmThrowException(pCtx,"ValueError",` |
|      1 | 6252 | `			"%s(): Argument #2 ($length) must be greater than 0",ph7_function_name(pCtx));` |
|      - | 6253 | `	}` |
|     11 | 6254 | `	if( nArg > 2 ){` |
|      3 | 6255 | `		iFlags = (int)ph7_value_to_int64(apArg[2]);` |
|      1 | 6256 | `	}` |
|     11 | 6257 | `	pSock = IoPrivateSocket(pDev);` |
|     11 | 6258 | `	if( pSock == 0 \|\| *pSock == PH7_NET_INVALID_SOCKET ){` |
|      3 | 6259 | `		SockStoreAddress(pCtx,apArg,nArg,3,0);` |
|      3 | 6260 | `		ph7_result_bool(pCtx,0);` |
|      3 | 6261 | `		return PH7_OK;` |
|      - | 6262 | `	}` |
|      9 | 6263 | `	if( nLen > 0x7FFFFFF0 ){` |
|    ! 0 | 6264 | `		nLen = 0x7FFFFFF0;` |
|    ! 0 | 6265 | `	}` |
|      9 | 6266 | `	zBuf = (char *)ph7_context_alloc_chunk(pCtx,(unsigned int)nLen,FALSE,TRUE);` |
|      9 | 6267 | `	if( zBuf == 0 ){` |
|    ! 0 | 6268 | `		return PH7_ContextMemoryError(pCtx);` |
|      - | 6269 | `	}` |
|      9 | 6270 | `	zAddr[0] = 0;` |
|      9 | 6271 | `	n = PH7_NetRecvFrom(*pSock,zBuf,(int)nLen,iFlags,zAddr,(int)sizeof(zAddr));` |
|      - | 6272 | `	/* php writes the out-param on every call: the sender's address for a read` |
|      - | 6273 | `	 * that happened (empty for a connected stream, which has none to report) and` |
|      - | 6274 | `	 * NULL for one that did not — never the caller's previous value. */` |
|      9 | 6275 | `	SockStoreAddress(pCtx,apArg,nArg,3,n < 0 ? 0 : zAddr);` |
|      9 | 6276 | `	if( n < 0 ){` |
|    ! 0 | 6277 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 6278 | `	}else{` |
|      9 | 6279 | `		ph7_result_string(pCtx,zBuf,n);` |
|      - | 6280 | `	}` |
|      9 | 6281 | `	ph7_context_free_chunk(pCtx,zBuf);` |
|      9 | 6282 | `	return PH7_OK;` |
|      7 | 6283 | `}` |
|     12 | 6284 | `PH7_PRIVATE int PH7_builtin_stream_socket_sendto(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 6285 | `{` |
|      - | 6286 | `	io_private *pDev;` |
|      - | 6287 | `	ph7_socket *pSock;` |
|     13 | 6288 | `	const char *zData,*zSentTo = "";` |
|      - | 6289 | `	char zHost[256];` |
|     13 | 6290 | `	int rc,iFlags = 0,nData,n,iPort = 0,nSentTo = 0,iErr = 0;` |
|     13 | 6291 | `	if( nArg < 2 ){` |
|    ! 0 | 6292 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 6293 | `		return PH7_OK;` |
|      - | 6294 | `	}` |
|     13 | 6295 | `	pDev = StreamSettingArgNamed(pCtx,apArg[0],1,"socket",&rc);` |
|     13 | 6296 | `	if( pDev == 0 ){` |
|    ! 0 | 6297 | `		return rc;` |
|      - | 6298 | `	}` |
|     13 | 6299 | `	zData = ph7_value_to_string(apArg[1],&nData);` |
|     13 | 6300 | `	if( nArg > 2 ){` |
|      7 | 6301 | `		iFlags = (int)ph7_value_to_int64(apArg[2]);` |
|      3 | 6302 | `	}` |
|     13 | 6303 | `	zHost[0] = 0;` |
|     13 | 6304 | `	if( nArg > 3 && ph7_value_is_string(apArg[3]) ){` |
|      7 | 6305 | `		int nAddr,i,nHost = -1;` |
|      7 | 6306 | `		const char *zAddr = ph7_value_to_string(apArg[3],&nAddr);` |
|      7 | 6307 | `		if( nAddr > 0 ){` |
|      - | 6308 | `			/* php parses THIS address without looking for a transport at all —` |
|      - | 6309 | ``			 * the first colon is the separator, so `udp://1.2.3.4:53` names the`` |
|      - | 6310 | `			 * host "udp" — and an address it cannot turn into a sockaddr is a` |
|      - | 6311 | `			 * refusal rather than a send to the connected peer, which is where` |
|      - | 6312 | `			 * the bytes would otherwise silently go. */` |
|     49 | 6313 | `			for( i = 0 ; i + 1 < nAddr ; i++ ){` |
|     45 | 6314 | `				if( zAddr[i] == ':' ){` |
|      3 | 6315 | `					iPort = SockParsePort(&zAddr[i+1],nAddr - i - 1);` |
|      3 | 6316 | `					nHost = i;` |
|      3 | 6317 | `					break;` |
|      - | 6318 | `				}` |
|     22 | 6319 | `			}` |
|      7 | 6320 | `			if( nHost < 0 ){` |
|      7 | 6321 | `				ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,` |
|      2 | 6322 | ``					"Failed to parse `%.*s' into a valid network address",nAddr,zAddr);`` |
|      5 | 6323 | `				ph7_result_bool(pCtx,0);` |
|      5 | 6324 | `				return PH7_OK;` |
|      - | 6325 | `			}` |
|      3 | 6326 | `			if( nHost >= (int)sizeof(zHost) ){` |
|    ! 0 | 6327 | `				nHost = (int)sizeof(zHost) - 1;` |
|    ! 0 | 6328 | `			}` |
|      3 | 6329 | `			if( nHost > 0 ){` |
|      3 | 6330 | `				SyMemcpy(zAddr,zHost,(sxu32)nHost);` |
|      1 | 6331 | `			}` |
|      3 | 6332 | `			zHost[nHost] = 0;` |
|      3 | 6333 | `			zSentTo = zAddr;` |
|      3 | 6334 | `			nSentTo = nAddr;` |
|      1 | 6335 | `		}` |
|      1 | 6336 | `	}` |
|      9 | 6337 | `	pSock = IoPrivateSocket(pDev);` |
|      9 | 6338 | `	if( pSock == 0 \|\| *pSock == PH7_NET_INVALID_SOCKET ){` |
|      - | 6339 | `		/* php answers -1 here rather than false: this one reports the send()` |
|      - | 6340 | `		 * result, and it never made a call. */` |
|      3 | 6341 | `		ph7_result_int(pCtx,-1);` |
|      3 | 6342 | `		return PH7_OK;` |
|      - | 6343 | `	}` |
|      7 | 6344 | `	n = PH7_NetSendTo(*pSock,(const void *)zData,nData,iFlags,zHost,iPort,&iErr);` |
|      7 | 6345 | `	if( iErr == PH7_NET_ERR_RESOLVE ){` |
|      - | 6346 | `		/* php says it three times for one failure — the resolver's own text, the` |
|      - | 6347 | `		 * name it could not resolve, and the address it therefore could not` |
|      - | 6348 | `		 * parse — and answers FALSE rather than the -1 a failed send gives. */` |
|      - | 6349 | `		char zMsg[512];` |
|    ! 0 | 6350 | `		SockResolveFailure(pCtx,zHost,zMsg,(int)sizeof(zMsg));` |
|    ! 0 | 6351 | ``		ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,"Failed to resolve `%s': %s",zHost,zMsg);`` |
|    ! 0 | 6352 | `		ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,` |
|    ! 0 | 6353 | ``			"Failed to parse `%.*s' into a valid network address",nSentTo,zSentTo);`` |
|    ! 0 | 6354 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 6355 | `		return PH7_OK;` |
|      - | 6356 | `	}` |
|      7 | 6357 | `	if( n < 0 ){` |
|      - | 6358 | `		/* php reports the OS text and hands back the -1 send() answered — this` |
|      - | 6359 | `		 * one never answers false, which is why a caller compares it against 0` |
|      - | 6360 | `		 * rather than testing it for truth. The trailing newline is php's own. */` |
|      4 | 6361 | `		ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,"%s\n",` |
|      1 | 6362 | `			PH7_NetStrError(PH7_NetLastError()));` |
|      1 | 6363 | `	}` |
|      7 | 6364 | `	ph7_result_int(pCtx,n);` |
|      7 | 6365 | `	return PH7_OK;` |
|      7 | 6366 | `}` |
|      - | 6367 | `/*` |
|      - | 6368 | ` * array\|false stream_socket_pair(int $domain, int $type, int $protocol)` |
|      - | 6369 | ` *` |
|      - | 6370 | ` * Two connected sockets with no address between them — the two-way pipe a` |
|      - | 6371 | ` * program hands a child, or a test double hands the code under test. Which` |
|      - | 6372 | ` * $domain works is the OS's answer and not php's: POSIX has AF_UNIX and refuses` |
|      - | 6373 | ` * AF_INET, and Windows is the other way round (php emulates the pair over the` |
|      - | 6374 | ` * loopback there, and so does this).` |
|      - | 6375 | ` */` |
|      4 | 6376 | `PH7_PRIVATE int PH7_builtin_stream_socket_pair(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 6377 | `{` |
|      - | 6378 | `	ph7_socket aSock[2];` |
|      - | 6379 | `	io_private *apDev[2];` |
|      - | 6380 | `	ph7_value *pArr,*pVal;` |
|      5 | 6381 | `	int iErrno = 0,i;` |
|      5 | 6382 | `	if( nArg < 3 ){` |
|    ! 0 | 6383 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 6384 | `		return PH7_OK;` |
|      - | 6385 | `	}` |
|      6 | 6386 | `	if( PH7_NetSocketPair((int)ph7_value_to_int64(apArg[0]),(int)ph7_value_to_int64(apArg[1]),` |
|      7 | 6387 | `		(int)ph7_value_to_int64(apArg[2]),aSock,&iErrno) != PH7_OK ){` |
|      - | 6388 | `		/* php reports the OS code and its text, in that order and in brackets. */` |
|      4 | 6389 | `		ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,"Failed to create sockets: [%d]: %s",` |
|      1 | 6390 | `			iErrno,PH7_NetStrError(iErrno));` |
|      3 | 6391 | `		ph7_result_bool(pCtx,0);` |
|      3 | 6392 | `		return PH7_OK;` |
|      - | 6393 | `	}` |
|      3 | 6394 | `	pArr = ph7_context_new_array(pCtx);` |
|      3 | 6395 | `	pVal = ph7_context_new_scalar(pCtx);` |
|      3 | 6396 | `	apDev[0] = apDev[1] = 0;` |
|      3 | 6397 | `	if( pArr == 0 \|\| pVal == 0 ){` |
|    ! 0 | 6398 | `		PH7_NetClose(aSock[0]);` |
|    ! 0 | 6399 | `		PH7_NetClose(aSock[1]);` |
|    ! 0 | 6400 | `		return PH7_ContextMemoryError(pCtx);` |
|      - | 6401 | `	}` |
|      7 | 6402 | `	for( i = 0 ; i < 2 ; i++ ){` |
|      - | 6403 | `		/* No uri: nothing opened these by name, which is what php reports. */` |
|      5 | 6404 | `		apDev[i] = SockWrapSocket(pCtx,aSock[i],0,0);` |
|      5 | 6405 | `		if( apDev[i] == 0 ){` |
|      - | 6406 | `			/* SockWrapSocket closed the one it could not wrap; the OTHER end is` |
|      - | 6407 | `			 * still ours to close, wrapped or not. */` |
|    ! 0 | 6408 | `			if( i == 0 ){` |
|    ! 0 | 6409 | `				PH7_NetClose(aSock[1]);` |
|    ! 0 | 6410 | `			}else{` |
|    ! 0 | 6411 | `				SockCloseWrapped(pCtx,apDev[0]);` |
|      - | 6412 | `			}` |
|    ! 0 | 6413 | `			return PH7_ContextMemoryError(pCtx);` |
|      - | 6414 | `		}` |
|      - | 6415 | `		/* A pair has no transport of its own, and php labels it apart from a` |
|      - | 6416 | `		 * tcp:// stream for exactly that reason. */` |
|      5 | 6417 | `		((sock_private *)apDev[i]->pHandle)->bGeneric = 1;` |
|      5 | 6418 | `		SockArmDefaultTimeout(pCtx,apDev[i]);` |
|      3 | 6419 | `	}` |
|      7 | 6420 | `	for( i = 0 ; i < 2 ; i++ ){` |
|      5 | 6421 | `		ph7_value_resource(pVal,apDev[i]);` |
|      5 | 6422 | `		ph7_array_add_elem(pArr,0,pVal);` |
|      3 | 6423 | `	}` |
|      3 | 6424 | `	ph7_result_value(pCtx,pArr);` |
|      3 | 6425 | `	return PH7_OK;` |
|      3 | 6426 | `}` |
|      - | 6427 | `/*` |
|      - | 6428 | ` * string\|false stream_socket_get_name(resource $socket, bool $remote)` |
|      - | 6429 | ` *` |
|      - | 6430 | `` * Which address this socket sits on (`$remote` false) or is talking to (true).`` |
|      - | 6431 | `` * It is the only way to learn the port a server bound with `:0` actually got,`` |
|      - | 6432 | ` * so a test that needs a free port had to guess one without it.` |
|      - | 6433 | ` */` |
|     50 | 6434 | `PH7_PRIVATE int PH7_builtin_stream_socket_get_name(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      4 | 6435 | `{` |
|      - | 6436 | `	io_private *pDev;` |
|      - | 6437 | `	ph7_socket *pSock;` |
|      - | 6438 | `	char zName[128];` |
|      - | 6439 | `	int rc;` |
|     54 | 6440 | `	if( nArg < 2 ){` |
|    ! 0 | 6441 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 6442 | `		return PH7_OK;` |
|      - | 6443 | `	}` |
|     54 | 6444 | `	pDev = StreamSettingArgNamed(pCtx,apArg[0],1,"socket",&rc);` |
|     54 | 6445 | `	if( pDev == 0 ){` |
|      5 | 6446 | `		return rc;` |
|      - | 6447 | `	}` |
|     50 | 6448 | `	pSock = IoPrivateSocket(pDev);` |
|     50 | 6449 | `	if( pSock == 0 \|\| PH7_NetSockName(*pSock,ph7_value_to_bool(apArg[1]),zName,(int)sizeof(zName)) != PH7_OK ){` |
|      - | 6450 | `		/* php answers false for a stream that is not a socket, and for the peer` |
|      - | 6451 | `		 * of a socket that is not connected — an unaccepted server. */` |
|     15 | 6452 | `		ph7_result_bool(pCtx,0);` |
|     15 | 6453 | `		return PH7_OK;` |
|      - | 6454 | `	}` |
|     37 | 6455 | `	ph7_result_string(pCtx,zName,-1);` |
|     37 | 6456 | `	return PH7_OK;` |
|     29 | 6457 | `}` |
|      - | 6458 | `#endif /*` |
|      - | 6459 | ` * The stream SETTINGS family. Every one of these was a loud` |
|      - | 6460 | `` * `Call to undefined function` — so a program that puts a socket in`` |
|      - | 6461 | ` * non-blocking mode, bounds a read with a timeout, or asks whether a stream` |
|      - | 6462 | ` * can be locked before calling flock() did not run at all.` |
|      - | 6463 | ` *` |
|      - | 6464 | ` * The shared preamble: php refuses a non-resource with a TypeError naming the` |
|      - | 6465 | ` * parameter, and an already-closed handle the same way.` |
|      - | 6466 | ` */` |
|    498 | 6467 | `static io_private * StreamSettingArgNamed(ph7_context *pCtx,ph7_value *pArg,int iPos,` |
|      - | 6468 | `	const char *zName,int *pRc)` |
|      5 | 6469 | `{` |
|      - | 6470 | `	io_private *pDev;` |
|    503 | 6471 | `	*pRc = PH7_OK;` |
|    503 | 6472 | `	if( !ph7_value_is_resource(pArg) ){` |
|     20 | 6473 | `		*pRc = PH7_VmThrowException(pCtx,"TypeError",` |
|      - | 6474 | `			"%s(): Argument #%d ($%s) must be of type resource, %s given",` |
|      6 | 6475 | `			ph7_function_name(pCtx),iPos,zName,ph7_type_name(pArg));` |
|     14 | 6476 | `		return 0;` |
|      - | 6477 | `	}` |
|    491 | 6478 | `	pDev = (io_private *)ph7_value_to_resource(pArg);` |
|    491 | 6479 | `	if( IO_PRIVATE_INVALID(pDev) ){` |
|      4 | 6480 | `		*pRc = PH7_VmThrowException(pCtx,"TypeError",` |
|      - | 6481 | `			"%s(): Argument #%d ($%s) must be an open stream resource",` |
|      1 | 6482 | `			ph7_function_name(pCtx),iPos,zName);` |
|      3 | 6483 | `		return 0;` |
|      - | 6484 | `	}` |
|    489 | 6485 | `	return pDev;` |
|    254 | 6486 | `}` |
|      - | 6487 | ``/* The whole settings family names its one handle `$stream`; the copy names two. */`` |
|    140 | 6488 | `static io_private * StreamSettingArg(ph7_context *pCtx,ph7_value *pArg,int *pRc)` |
|      4 | 6489 | `{` |
|    144 | 6490 | `	return StreamSettingArgNamed(pCtx,pArg,1,"stream",pRc);` |
|      4 | 6491 | `}` |
|      - | 6492 | `/* The same screen, for the filter family in vfs_filter.c. */` |
|    170 | 6493 | `PH7_PRIVATE io_private * PH7_StreamHandleArg(ph7_context *pCtx,ph7_value *pArg,int iPos,` |
|      - | 6494 | `	const char *zName,int *pRc)` |
|      2 | 6495 | `{` |
|    172 | 6496 | `	return StreamSettingArgNamed(pCtx,pArg,iPos,zName,pRc);` |
|      2 | 6497 | `}` |
|      - | 6498 | `/* The tcp:// socket behind a handle, or 0 for any other device. */` |
|    346 | 6499 | `static ph7_socket * IoPrivateSocket(io_private *pDev)` |
|      5 | 6500 | `{` |
|      - | 6501 | `#ifdef PH7_ENABLE_NET` |
|    351 | 6502 | `	if( pDev->pStream == &sTCP_Stream && pDev->pHandle ){` |
|    283 | 6503 | `		return &((sock_private *)pDev->pHandle)->sock;` |
|      - | 6504 | `	}` |
|      - | 6505 | `#endif` |
|     33 | 6506 | `	SXUNUSED(pDev); /* cc warning when NET is off */` |
|     70 | 6507 | `	return 0;` |
|    178 | 6508 | `}` |
|      - | 6509 | `/*` |
|      - | 6510 | ` * bool stream_set_blocking(resource $stream, bool $enable)` |
|      - | 6511 | ` *` |
|      - | 6512 | ` * php sets the mode AT the descriptor and answers TRUE either way; a stream` |
|      - | 6513 | ` * with no descriptor — a memory buffer, a data:// payload — keeps reporting` |
|      - | 6514 | ` * itself blocked, which is why the flag is only recorded when it took. On` |
|      - | 6515 | ` * Windows php's plain-files device has no O_NONBLOCK to set, so every such` |
|      - | 6516 | ` * stream (a file, a pipe, php://stdin) answers FALSE there.` |
|      - | 6517 | ` */` |
|     30 | 6518 | `PH7_PRIVATE int PH7_builtin_stream_set_blocking(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      4 | 6519 | `{` |
|      - | 6520 | `	io_private *pDev;` |
|      - | 6521 | `	int rc,bEnable,fd;` |
|      - | 6522 | `	ph7_socket *pSock;` |
|     34 | 6523 | `	if( nArg < 2 ){` |
|    ! 0 | 6524 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 6525 | `		return PH7_OK;` |
|      - | 6526 | `	}` |
|     34 | 6527 | `	pDev = StreamSettingArg(pCtx,apArg[0],&rc);` |
|     34 | 6528 | `	if( pDev == 0 ){` |
|      3 | 6529 | `		return rc;` |
|      - | 6530 | `	}` |
|     32 | 6531 | `	bEnable = ph7_value_to_bool(apArg[1]);` |
|     32 | 6532 | `	pSock = IoPrivateSocket(pDev);` |
|     32 | 6533 | `	if( pSock ){` |
|      - | 6534 | `#ifdef PH7_ENABLE_NET` |
|      9 | 6535 | `		if( *pSock == PH7_NET_INVALID_SOCKET ){` |
|      - | 6536 | `			/* No socket to set the mode on: php's own answer is FALSE, which is` |
|      - | 6537 | `			 * the one place this family reports a setting that did not take. */` |
|      3 | 6538 | `			ph7_result_bool(pCtx,0);` |
|      3 | 6539 | `			return PH7_OK;` |
|      - | 6540 | `		}` |
|      6 | 6541 | `		PH7_NetSetBlocking(*pSock,bEnable);` |
|      - | 6542 | `#endif` |
|      6 | 6543 | `		pDev->bNonBlock = (sxu8)(bEnable ? 0 : 1);` |
|      4 | 6544 | `	}else{` |
|      - | 6545 | `#ifdef __WINNT__` |
|      1 | 6546 | `		if( PH7_StreamIsPlainDevice(pDev) ){` |
|      1 | 6547 | `			ph7_result_bool(pCtx,0);` |
|      1 | 6548 | `			return PH7_OK;` |
|      - | 6549 | `		}` |
|      - | 6550 | `#endif` |
|     23 | 6551 | `		fd = PH7_StreamPosixFd(pDev);` |
|     22 | 6552 | `		if( fd >= 0 ){` |
|      - | 6553 | `#ifndef __WINNT__` |
|     14 | 6554 | `			int iFlags = fcntl(fd,F_GETFL,0);` |
|     14 | 6555 | `			if( iFlags >= 0 ){` |
|     14 | 6556 | `				if( bEnable ){` |
|      4 | 6557 | `					iFlags &= ~O_NONBLOCK;` |
|      2 | 6558 | `				}else{` |
|     10 | 6559 | `					iFlags \|= O_NONBLOCK;` |
|      - | 6560 | `				}` |
|     14 | 6561 | `				if( fcntl(fd,F_SETFL,iFlags) == 0 ){` |
|     14 | 6562 | `					pDev->bNonBlock = (sxu8)(bEnable ? 0 : 1);` |
|      7 | 6563 | `				}` |
|      7 | 6564 | `			}` |
|      - | 6565 | `#endif` |
|      7 | 6566 | `		}` |
|      - | 6567 | `	}` |
|     29 | 6568 | `	ph7_result_bool(pCtx,1);` |
|     29 | 6569 | `	return PH7_OK;` |
|     19 | 6570 | `}` |
|      - | 6571 | `/*` |
|      - | 6572 | ` * bool stream_set_timeout(resource $stream, int $seconds, int $microseconds = 0)` |
|      - | 6573 | ` *` |
|      - | 6574 | ` * php answers TRUE only for a stream whose transport HAS a timeout — a socket —` |
|      - | 6575 | ` * and FALSE for every file, pipe and memory buffer, because there is nothing` |
|      - | 6576 | ` * to wait on. Silently accepting it for a file would tell a caller its read is` |
|      - | 6577 | ` * bounded when it is not.` |
|      - | 6578 | ` */` |
|     24 | 6579 | `PH7_PRIVATE int PH7_builtin_stream_set_timeout(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 6580 | `{` |
|      - | 6581 | `	io_private *pDev;` |
|      - | 6582 | `	ph7_socket *pSock;` |
|      - | 6583 | `	int rc;` |
|     25 | 6584 | `	if( nArg < 2 ){` |
|    ! 0 | 6585 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 6586 | `		return PH7_OK;` |
|      - | 6587 | `	}` |
|     25 | 6588 | `	pDev = StreamSettingArg(pCtx,apArg[0],&rc);` |
|     25 | 6589 | `	if( pDev == 0 ){` |
|    ! 0 | 6590 | `		return rc;` |
|      - | 6591 | `	}` |
|     25 | 6592 | `	pSock = IoPrivateSocket(pDev);` |
|     25 | 6593 | `	if( pSock == 0 ){` |
|      7 | 6594 | `		ph7_result_bool(pCtx,0);` |
|      7 | 6595 | `		return PH7_OK;` |
|      - | 6596 | `	}` |
|      - | 6597 | `#ifdef PH7_ENABLE_NET` |
|      - | 6598 | `	{` |
|     18 | 6599 | `		ph7_int64 iSec = ph7_value_to_int64(apArg[1]);` |
|     18 | 6600 | `		ph7_int64 iUsec = nArg > 2 ? ph7_value_to_int64(apArg[2]) : 0;` |
|     18 | 6601 | `		if( iSec < 0 ){` |
|    ! 0 | 6602 | `			iSec = 0;` |
|    ! 0 | 6603 | `		}` |
|     18 | 6604 | `		if( iUsec < 0 ){` |
|    ! 0 | 6605 | `			iUsec = 0;` |
|    ! 0 | 6606 | `		}` |
|     18 | 6607 | `		PH7_NetSetRwTimeout(*pSock,iSec,iUsec);` |
|      - | 6608 | `		/* An expired read answers FALSE and says so through the metadata;` |
|      - | 6609 | `		 * without the armed flag it is indistinguishable from a non-blocking` |
|      - | 6610 | `		 * one, which answers "". */` |
|     18 | 6611 | `		pDev->bHasTimeout = 1;` |
|     18 | 6612 | `		pDev->bTimedOut = 0;` |
|      - | 6613 | `	}` |
|      - | 6614 | `#endif` |
|     18 | 6615 | `	ph7_result_bool(pCtx,1);` |
|     18 | 6616 | `	return PH7_OK;` |
|     13 | 6617 | `}` |
|      - | 6618 | `/*` |
|      - | 6619 | ` * int stream_set_chunk_size(resource $stream, int $size)` |
|      - | 6620 | ` *` |
|      - | 6621 | ` * Answers the PREVIOUS size, which is what makes the setting restorable, and` |
|      - | 6622 | ` * refuses a non-positive one the way php does.` |
|      - | 6623 | ` */` |
|     46 | 6624 | `PH7_PRIVATE int PH7_builtin_stream_set_chunk_size(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 6625 | `{` |
|      - | 6626 | `	io_private *pDev;` |
|      - | 6627 | `	ph7_int64 nSize;` |
|      - | 6628 | `	int rc;` |
|     47 | 6629 | `	if( nArg < 2 ){` |
|    ! 0 | 6630 | `		ph7_result_int(pCtx,-1);` |
|    ! 0 | 6631 | `		return PH7_OK;` |
|      - | 6632 | `	}` |
|     47 | 6633 | `	pDev = StreamSettingArg(pCtx,apArg[0],&rc);` |
|     47 | 6634 | `	if( pDev == 0 ){` |
|    ! 0 | 6635 | `		return rc;` |
|      - | 6636 | `	}` |
|     47 | 6637 | `	nSize = ph7_value_to_int64(apArg[1]);` |
|     47 | 6638 | `	if( nSize < 1 ){` |
|      5 | 6639 | `		return PH7_VmThrowException(pCtx,"ValueError",` |
|      - | 6640 | `			"stream_set_chunk_size(): Argument #2 ($size) must be greater than 0");` |
|      - | 6641 | `	}` |
|     43 | 6642 | `	if( nSize > (ph7_int64)SXI32_HIGH ){` |
|      - | 6643 | `		/* php's own ceiling: the size is an int on its side, and storing a` |
|      - | 6644 | `		 * larger one made the NEXT call report a size no caller ever set. */` |
|      3 | 6645 | `		return PH7_VmThrowException(pCtx,"ValueError",` |
|      - | 6646 | `			"stream_set_chunk_size(): Argument #2 ($size) is too large");` |
|      - | 6647 | `	}` |
|     41 | 6648 | `	ph7_result_int64(pCtx,(ph7_int64)pDev->nChunk);` |
|     41 | 6649 | `	pDev->nChunk = (sxu32)nSize;` |
|     41 | 6650 | `	return PH7_OK;` |
|     24 | 6651 | `}` |
|      - | 6652 | `/*` |
|      - | 6653 | ` * int stream_set_read_buffer(resource $stream, int $size)` |
|      - | 6654 | ` * int stream_set_write_buffer(resource $stream, int $size)  [set_file_buffer]` |
|      - | 6655 | ` *` |
|      - | 6656 | ` * php's stream layer has no stdio buffer left to hand these to: the read side` |
|      - | 6657 | ` * answers 0 (accepted) and the write side -1 (unsupported), for every stream` |
|      - | 6658 | ` * and every size. Both are still validated arguments, so a bad handle is the` |
|      - | 6659 | ` * same TypeError the rest of the family raises.` |
|      - | 6660 | ` */` |
|      6 | 6661 | `PH7_PRIVATE int PH7_builtin_stream_set_read_buffer(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 6662 | `{` |
|      7 | 6663 | `	int rc = PH7_OK;` |
|      7 | 6664 | `	if( nArg < 2 ){` |
|    ! 0 | 6665 | `		ph7_result_int(pCtx,-1);` |
|    ! 0 | 6666 | `		return PH7_OK;` |
|      - | 6667 | `	}` |
|      7 | 6668 | `	if( StreamSettingArg(pCtx,apArg[0],&rc) == 0 ){` |
|    ! 0 | 6669 | `		return rc;` |
|      - | 6670 | `	}` |
|      7 | 6671 | `	ph7_result_int(pCtx,0);` |
|      7 | 6672 | `	return PH7_OK;` |
|      4 | 6673 | `}` |
|     12 | 6674 | `PH7_PRIVATE int PH7_builtin_stream_set_write_buffer(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 6675 | `{` |
|     13 | 6676 | `	int rc = PH7_OK;` |
|     13 | 6677 | `	if( nArg < 2 ){` |
|    ! 0 | 6678 | `		ph7_result_int(pCtx,-1);` |
|    ! 0 | 6679 | `		return PH7_OK;` |
|      - | 6680 | `	}` |
|     13 | 6681 | `	if( StreamSettingArg(pCtx,apArg[0],&rc) == 0 ){` |
|    ! 0 | 6682 | `		return rc;` |
|      - | 6683 | `	}` |
|     13 | 6684 | `	ph7_result_int(pCtx,-1);` |
|     13 | 6685 | `	return PH7_OK;` |
|      7 | 6686 | `}` |
|      - | 6687 | `/*` |
|      - | 6688 | ` * int\|false stream_copy_to_stream(resource $from, resource $to,` |
|      - | 6689 | ` *                                 ?int $length = null, int $offset = 0)` |
|      - | 6690 | ` *` |
|      - | 6691 | ` * The everyday way to move bytes between two open streams, and a loud` |
|      - | 6692 | `` * `Call to undefined function` here until now — so the workaround was`` |
|      - | 6693 | `` * `fwrite($to, stream_get_contents($from))`, which reads the WHOLE source into`` |
|      - | 6694 | ` * memory first. A NULL or negative $length is "the rest"; a POSITIVE $offset` |
|      - | 6695 | ` * seeks the source first and is php's only failure shape short of a broken` |
|      - | 6696 | ` * write.` |
|      - | 6697 | ` */` |
|     38 | 6698 | `PH7_PRIVATE int PH7_builtin_stream_copy_to_stream(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 6699 | `{` |
|      - | 6700 | `	io_private *pFrom,*pTo;` |
|     39 | 6701 | `	ph7_int64 nWant = -1,nOfft = 0,nTotal = 0;` |
|      - | 6702 | `	char zBuf[8192];` |
|      - | 6703 | `	int rc;` |
|     39 | 6704 | `	if( nArg < 2 ){` |
|    ! 0 | 6705 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 6706 | `		return PH7_OK;` |
|      - | 6707 | `	}` |
|     39 | 6708 | `	pFrom = StreamSettingArgNamed(pCtx,apArg[0],1,"from",&rc);` |
|     39 | 6709 | `	if( pFrom == 0 ){` |
|      3 | 6710 | `		return rc;` |
|      - | 6711 | `	}` |
|     37 | 6712 | `	pTo = StreamSettingArgNamed(pCtx,apArg[1],2,"to",&rc);` |
|     37 | 6713 | `	if( pTo == 0 ){` |
|      3 | 6714 | `		return rc;` |
|      - | 6715 | `	}` |
|     34 | 6716 | `	if( pFrom->pStream == 0 \|\| pFrom->pStream->xRead == 0` |
|     35 | 6717 | `	 \|\| pTo->pStream == 0 \|\| pTo->pStream->xWrite == 0 ){` |
|    ! 0 | 6718 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 6719 | `		return PH7_OK;` |
|      - | 6720 | `	}` |
|     35 | 6721 | `	if( nArg > 2 && !ph7_value_is_null(apArg[2]) ){` |
|     11 | 6722 | `		nWant = ph7_value_to_int64(apArg[2]);` |
|      5 | 6723 | `	}` |
|     35 | 6724 | `	if( nArg > 3 ){` |
|     19 | 6725 | `		nOfft = ph7_value_to_int64(apArg[3]);` |
|      9 | 6726 | `	}` |
|     35 | 6727 | `	if( nOfft > 0 ){` |
|      - | 6728 | `		/* php seeks the SOURCE and gives up loudly when it cannot: a pipe has` |
|      - | 6729 | `		 * no position to move to, and silently copying from wherever it` |
|      - | 6730 | `		 * happens to be would answer for a different slice of the stream. */` |
|      8 | 6731 | `		if( pFrom->pStream->xSeek == 0` |
|      8 | 6732 | `		 \|\| pFrom->pStream->xSeek(pFrom->pHandle,nOfft,0/*SEEK_SET*/) != PH7_OK ){` |
|      2 | 6733 | `			if( pFrom->pStream->xSeek == 0 ){` |
|      2 | 6734 | `				ph7_context_throw_error(pCtx,PH7_CTX_WARNING,` |
|      - | 6735 | `					"stream_copy_to_stream(): Stream does not support seeking");` |
|      1 | 6736 | `			}` |
|      3 | 6737 | `			ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,` |
|      1 | 6738 | `				"stream_copy_to_stream(): Failed to seek to position %qd in the stream",nOfft);` |
|      2 | 6739 | `			ph7_result_bool(pCtx,0);` |
|      2 | 6740 | `			return PH7_OK;` |
|      - | 6741 | `		}` |
|      7 | 6742 | `		ResetIOPrivate(pFrom);` |
|      3 | 6743 | `	}` |
|     33 | 6744 | `	if( nWant == 0 ){` |
|      3 | 6745 | `		ph7_result_int(pCtx,0);` |
|      3 | 6746 | `		return PH7_OK;` |
|      - | 6747 | `	}` |
|      - | 6748 | `#ifdef __WINNT__` |
|      - | 6749 | `	/* An unfiltered plain-file source goes through php's memory-mapped copy,` |
|      - | 6750 | `	 * whose Windows view at the end of the file is a failure: see` |
|      - | 6751 | `	 * PH7_WinFileMapsEmptyView(). */` |
|      - | 6752 | `	if( pFrom->pStream == &sWinFileStream && pFrom->pReadFilters == 0 && pFrom->pWriteFilters == 0` |
|      1 | 6753 | `	 && PH7_WinFileMapsEmptyView(pFrom->pHandle,SyBlobLength(&pFrom->sBuffer) > pFrom->nOfft` |
|      - | 6754 | `			? (ph7_int64)(SyBlobLength(&pFrom->sBuffer) - pFrom->nOfft) : 0) ){` |
|      1 | 6755 | `		ph7_result_bool(pCtx,0);` |
|      1 | 6756 | `		return PH7_OK;` |
|      - | 6757 | `	}` |
|      - | 6758 | `#endif` |
|      - | 6759 | `	/* The destination may be sitting past its own read-ahead; the write has to` |
|      - | 6760 | `	 * land where the SCRIPT is, the rule fwrite() follows. */` |
|     31 | 6761 | `	StreamSeekBackForWrite(pTo);` |
|     39 | 6762 | `	for(;;){` |
|     55 | 6763 | `		ph7_int64 nAsk = (ph7_int64)sizeof(zBuf);` |
|      - | 6764 | `		ph7_int64 nRead,nWr;` |
|     55 | 6765 | `		if( nWant > 0 && nWant - nTotal < nAsk ){` |
|     13 | 6766 | `			nAsk = nWant - nTotal;` |
|      6 | 6767 | `		}` |
|     55 | 6768 | `		if( nAsk < 1 ){` |
|      5 | 6769 | `			break;` |
|      - | 6770 | `		}` |
|     51 | 6771 | `		nRead = PH7_StreamRead(pFrom,zBuf,nAsk);` |
|     51 | 6772 | `		if( nRead < 1 ){` |
|     27 | 6773 | `			break;` |
|      - | 6774 | `		}` |
|     25 | 6775 | `		nWr = PH7_StreamWrite(pTo,(const void *)zBuf,nRead);` |
|     25 | 6776 | `		if( nWr < 0 ){` |
|    ! 0 | 6777 | `			SockReportWriteFailure(pCtx,pTo,(int)nRead);` |
|    ! 0 | 6778 | `			break;` |
|      - | 6779 | `		}` |
|     25 | 6780 | `		nTotal += nWr;` |
|     25 | 6781 | `		if( nWr < nRead ){` |
|    ! 0 | 6782 | `			break;` |
|      - | 6783 | `		}` |
|      1 | 6784 | `	}` |
|     31 | 6785 | `	ph7_result_int64(pCtx,nTotal);` |
|     31 | 6786 | `	return PH7_OK;` |
|     20 | 6787 | `}` |
|      - | 6788 | `/*` |
|      - | 6789 | ` * int\|false stream_select(?array &$read, ?array &$write, ?array &$except,` |
|      - | 6790 | ` *                         ?int $seconds, ?int $microseconds = null)` |
|      - | 6791 | ` *` |
|      - | 6792 | ` * The name that makes a program WAIT on several streams at once, and the reason` |
|      - | 6793 | ` * the settings family that shipped beside it had nothing to wait with: a` |
|      - | 6794 | ` * non-blocking read tells you a stream is not ready, and only this tells you` |
|      - | 6795 | ` * WHEN it becomes ready. It is what a proc_open() pipe pump, a socket server` |
|      - | 6796 | ` * loop and every event loop written in php is built on, and it was a loud` |
|      - | 6797 | `` * `Call to undefined function`.`` |
|      - | 6798 | ` *` |
|      - | 6799 | ` * php's own shape, and the parts of it a re-derivation misses: the arrays are` |
|      - | 6800 | ` * REWRITTEN in place to hold only the ready entries, under their original keys;` |
|      - | 6801 | ` * a stream that cannot be represented as a descriptor is a warning naming its` |
|      - | 6802 | ` * TYPE, not a failure; nothing selectable at all is an Error rather than 0; and` |
|      - | 6803 | ` * a stream whose own read buffer still holds bytes is answered READY without` |
|      - | 6804 | ` * asking the OS at all — which is the difference between a loop that drains a` |
|      - | 6805 | ` * buffered handle and one that waits forever for data it has already read.` |
|      - | 6806 | ` */` |
|      - | 6807 | `#if !defined(__WINNT__) \|\| defined(PH7_ENABLE_NET)` |
|      - | 6808 | `#define STREAM_SELECT_OK 1` |
|      - | 6809 | `#ifdef __UNIXES__` |
|      - | 6810 | `#include <sys/select.h>` |
|      - | 6811 | `#include <sys/time.h>` |
|      - | 6812 | `#endif` |
|      - | 6813 | `#endif` |
|      - | 6814 | `#define SEL_READ   0` |
|      - | 6815 | `#define SEL_WRITE  1` |
|      - | 6816 | `#define SEL_EXCEPT 2` |
|      - | 6817 | `/* What one walk over an argument is for. The order matters: php COUNTS the` |
|      - | 6818 | ` * already-buffered readable handles before it waits, and only rewrites the` |
|      - | 6819 | ` * arrays once it knows which answer it is giving. */` |
|      - | 6820 | `#define SELM_COLLECT  0 /* put every representable handle in its fd_set */` |
|      - | 6821 | `#define SELM_BUFFERED 1 /* keep the handles whose own buffer still holds bytes */` |
|      - | 6822 | `#define SELM_READY    2 /* keep the handles select() reported */` |
|      - | 6823 | `#define SELM_CLEAR    3 /* keep nothing: php empties the sets it is not answering */` |
|      - | 6824 | `typedef struct stream_select_ctx stream_select_ctx;` |
|      - | 6825 | `struct stream_select_ctx` |
|      - | 6826 | `{` |
|      - | 6827 | `	ph7_context *pCtx;` |
|      - | 6828 | `#ifdef STREAM_SELECT_OK` |
|      - | 6829 | `	fd_set aSet[3];    /* read / write / except, as select() takes them */` |
|      - | 6830 | `#endif` |
|      - | 6831 | `	int iMaxFd;` |
|      - | 6832 | `	int nSelectable;   /* entries that could be represented at all */` |
|      - | 6833 | `	int iWhich;        /* the set being walked (SEL_*) */` |
|      - | 6834 | `	int iMode;         /* SELM_*: what this walk is FOR */` |
|      - | 6835 | `	int nReady;` |
|      - | 6836 | `	int bBadEntry;     /* an entry that is not a stream at all */` |
|      - | 6837 | `	int bBadClosed;    /* ... and whether it was a CLOSED one (php words the two apart) */` |
|      - | 6838 | `	ph7_value *pOut;   /* the rebuilt array, while harvesting */` |
|      - | 6839 | `};` |
|      - | 6840 | `/*` |
|      - | 6841 | ` * What a select can WAIT on for this handle: the POSIX descriptor, or the` |
|      - | 6842 | ` * SOCKET, which is the only waitable thing a stream carries on Windows (the` |
|      - | 6843 | ` * file devices hold a HANDLE there, and select() cannot take one — a recorded` |
|      - | 6844 | ` * platform difference, §7.4). Answers -1 for a device with neither: a memory` |
|      - | 6845 | ` * buffer, a data:// payload, a userland wrapper.` |
|      - | 6846 | ` */` |
|     86 | 6847 | `static ph7_int64 IoPrivateSelectHandle(io_private *pDev)` |
|      1 | 6848 | `{` |
|      - | 6849 | `	int fd;` |
|      - | 6850 | `#ifdef PH7_ENABLE_NET` |
|     87 | 6851 | `	ph7_socket *pSock = IoPrivateSocket(pDev);` |
|     87 | 6852 | `	if( pSock ){` |
|     61 | 6853 | `		return *pSock == PH7_NET_INVALID_SOCKET ? -1 : (ph7_int64)*pSock;` |
|      - | 6854 | `	}` |
|      - | 6855 | `#endif` |
|     27 | 6856 | `	fd = PH7_StreamPosixFd(pDev);` |
|     27 | 6857 | `	return fd < 0 ? -1 : (ph7_int64)fd;` |
|     44 | 6858 | `}` |
|      - | 6859 | `/* Bytes this handle has already pulled off the device and not yet handed over. */` |
|     40 | 6860 | `static sxu32 IoPrivateUnread(io_private *pDev)` |
|      1 | 6861 | `{` |
|     41 | 6862 | `	return StreamAheadBytes(pDev);` |
|      1 | 6863 | `}` |
|     48 | 6864 | `static void StreamSelectAdd(stream_select_ctx *pSel,ph7_int64 h)` |
|      1 | 6865 | `{` |
|      - | 6866 | `#ifdef STREAM_SELECT_OK` |
|      - | 6867 | `#ifdef __WINNT__` |
|      - | 6868 | `	/* A Windows fd_set is an ARRAY of sockets, so what bounds it is how many` |
|      - | 6869 | `	 * are in it already rather than the value of this one. */` |
|      1 | 6870 | `	if( pSel->aSet[pSel->iWhich].fd_count >= FD_SETSIZE ){` |
|    ! 0 | 6871 | `		return;` |
|      - | 6872 | `	}` |
|      1 | 6873 | `	FD_SET((SOCKET)h,&pSel->aSet[pSel->iWhich]);` |
|      - | 6874 | `#else` |
|     48 | 6875 | `	if( h < 0 \|\| h >= (ph7_int64)FD_SETSIZE ){` |
|      - | 6876 | `		/* php ignores a descriptor an fd_set cannot hold (its own` |
|      - | 6877 | `		 * PHP_SAFE_FD_SET); writing past one corrupts the stack. */` |
|    ! 0 | 6878 | `		return;` |
|      - | 6879 | `	}` |
|     48 | 6880 | `	FD_SET((int)h,&pSel->aSet[pSel->iWhich]);` |
|      - | 6881 | `#endif` |
|     49 | 6882 | `	if( h > (ph7_int64)pSel->iMaxFd ){` |
|     39 | 6883 | `		pSel->iMaxFd = (int)h;` |
|     19 | 6884 | `	}` |
|      - | 6885 | `#else` |
|      - | 6886 | `	SXUNUSED(pSel);` |
|      - | 6887 | `	SXUNUSED(h);` |
|      - | 6888 | `#endif` |
|     25 | 6889 | `}` |
|     30 | 6890 | `static int StreamSelectIsSet(stream_select_ctx *pSel,ph7_int64 h)` |
|      1 | 6891 | `{` |
|      - | 6892 | `#ifdef STREAM_SELECT_OK` |
|      - | 6893 | `#ifdef __WINNT__` |
|      1 | 6894 | `	return FD_ISSET((SOCKET)h,&pSel->aSet[pSel->iWhich]) ? 1 : 0;` |
|      - | 6895 | `#else` |
|     30 | 6896 | `	if( h < 0 \|\| h >= (ph7_int64)FD_SETSIZE ){` |
|    ! 0 | 6897 | `		return 0;` |
|      - | 6898 | `	}` |
|     30 | 6899 | `	return FD_ISSET((int)h,&pSel->aSet[pSel->iWhich]) ? 1 : 0;` |
|      - | 6900 | `#endif` |
|      - | 6901 | `#else` |
|      - | 6902 | `	SXUNUSED(pSel);` |
|      - | 6903 | `	SXUNUSED(h);` |
|      - | 6904 | `	return 0;` |
|      - | 6905 | `#endif` |
|     16 | 6906 | `}` |
|      - | 6907 | `/*` |
|      - | 6908 | ` * One entry of one array: collected on the way in, harvested on the way out.` |
|      - | 6909 | ` * php never stops for an entry it cannot use — the diagnostics are remembered` |
|      - | 6910 | ` * and raised once the whole set is known, because whether the array held` |
|      - | 6911 | ` * ANYTHING selectable decides which of them php raises.` |
|      - | 6912 | ` */` |
|    134 | 6913 | `static int StreamSelectWalk(ph7_value *pKey,ph7_value *pValue,void *pUserData)` |
|      1 | 6914 | `{` |
|    135 | 6915 | `	stream_select_ctx *pSel = (stream_select_ctx *)pUserData;` |
|      - | 6916 | `	io_private *pDev;` |
|      - | 6917 | `	ph7_int64 h;` |
|    135 | 6918 | `	if( !ph7_value_is_resource(pValue) ){` |
|      7 | 6919 | `		pSel->bBadEntry = 1;` |
|      - | 6920 | `		/* php words a value that is not a resource apart from a resource that is` |
|      - | 6921 | `		 * no longer open, and raises one per bad entry — so the LAST one seen is` |
|      - | 6922 | `		 * the message that reaches the caller. */` |
|      7 | 6923 | `		pSel->bBadClosed = 0;` |
|      7 | 6924 | `		return PH7_OK;` |
|      - | 6925 | `	}` |
|    129 | 6926 | `	pDev = (io_private *)ph7_value_to_resource(pValue);` |
|    129 | 6927 | `	if( IO_PRIVATE_INVALID(pDev) ){` |
|      3 | 6928 | `		pSel->bBadEntry = pSel->bBadClosed = 1;` |
|      3 | 6929 | `		return PH7_OK;` |
|      - | 6930 | `	}` |
|    127 | 6931 | `	if( pSel->iMode == SELM_BUFFERED ){` |
|      - | 6932 | `		/* Deliberately BEFORE the descriptor lookup: php's shortcut lets a` |
|      - | 6933 | `		 * readable stream with no descriptor at all take part (a userland` |
|      - | 6934 | `		 * wrapper a line read has filled the buffer of), and answering 0 for one` |
|      - | 6935 | `		 * would sleep out the whole timeout over bytes the script already has. */` |
|     41 | 6936 | `		if( IoPrivateUnread(pDev) > 0 ){` |
|      9 | 6937 | `			if( pSel->pOut ){` |
|      5 | 6938 | `				ph7_array_add_elem(pSel->pOut,pKey,pValue);` |
|      2 | 6939 | `			}` |
|      9 | 6940 | `			pSel->nReady++;` |
|      4 | 6941 | `		}` |
|     41 | 6942 | `		return PH7_OK;` |
|      - | 6943 | `	}` |
|     87 | 6944 | `	h = IoPrivateSelectHandle(pDev);` |
|     87 | 6945 | `	if( h < 0 ){` |
|      7 | 6946 | `		if( pSel->iMode == SELM_COLLECT ){` |
|      - | 6947 | `			const char *zWrapper,*zLabel;` |
|      5 | 6948 | `			IoPrivateStreamLabels(pDev,&zWrapper,&zLabel);` |
|      7 | 6949 | `			ph7_context_throw_error_format(pSel->pCtx,PH7_CTX_WARNING,` |
|      2 | 6950 | `				"Cannot represent a stream of type %s as a select()able descriptor",zLabel);` |
|      2 | 6951 | `		}` |
|      7 | 6952 | `		return PH7_OK;` |
|      - | 6953 | `	}` |
|     81 | 6954 | `	if( pSel->iMode == SELM_COLLECT ){` |
|     49 | 6955 | `		pSel->nSelectable++;` |
|     49 | 6956 | `		StreamSelectAdd(pSel,h);` |
|     49 | 6957 | `		return PH7_OK;` |
|      - | 6958 | `	}` |
|     33 | 6959 | `	if( pSel->iMode == SELM_READY && StreamSelectIsSet(pSel,h) ){` |
|     21 | 6960 | `		if( pSel->pOut ){` |
|     21 | 6961 | `			ph7_array_add_elem(pSel->pOut,pKey,pValue);` |
|     10 | 6962 | `		}` |
|     21 | 6963 | `		pSel->nReady++;` |
|     10 | 6964 | `	}` |
|     33 | 6965 | `	return PH7_OK;` |
|     68 | 6966 | `}` |
|      - | 6967 | `/* The wait itself, over the pair the caller's numbers were normalised into. */` |
|     22 | 6968 | `static int StreamSelectWait(stream_select_ctx *pSel,ph7_int64 iSec,ph7_int64 iUsec,int bBlock,` |
|      - | 6969 | `	int *pErrno)` |
|      1 | 6970 | `{` |
|      - | 6971 | `#ifdef STREAM_SELECT_OK` |
|     23 | 6972 | `	struct timeval tv,*pTv = 0;` |
|      - | 6973 | `	int rc;` |
|     23 | 6974 | `	if( !bBlock ){` |
|     23 | 6975 | `		tv.tv_sec = (long)iSec;` |
|     23 | 6976 | `		tv.tv_usec = (long)iUsec;` |
|     23 | 6977 | `		pTv = &tv;` |
|     11 | 6978 | `	}` |
|     34 | 6979 | `	rc = select(pSel->iMaxFd + 1,&pSel->aSet[SEL_READ],&pSel->aSet[SEL_WRITE],` |
|     11 | 6980 | `		&pSel->aSet[SEL_EXCEPT],pTv);` |
|     23 | 6981 | `	if( rc < 0 && pErrno ){` |
|      - | 6982 | `#ifdef __WINNT__` |
|    ! 0 | 6983 | `		*pErrno = WSAGetLastError();` |
|      - | 6984 | `#else` |
|    ! 0 | 6985 | `		*pErrno = errno;` |
|      - | 6986 | `#endif` |
|    ! 0 | 6987 | `	}` |
|     23 | 6988 | `	return rc;` |
|      - | 6989 | `#else` |
|      - | 6990 | `	/* No select() to call: a Windows build with no socket layer. */` |
|      - | 6991 | `	SXUNUSED(pSel);` |
|      - | 6992 | `	SXUNUSED(iSec);` |
|      - | 6993 | `	SXUNUSED(iUsec);` |
|      - | 6994 | `	SXUNUSED(bBlock);` |
|      - | 6995 | `	if( pErrno ){ *pErrno = 0; }` |
|      - | 6996 | `	return -1;` |
|      - | 6997 | `#endif` |
|      1 | 6998 | `}` |
|      - | 6999 | `/* Walk one of the three arguments, if it IS one. */` |
|    190 | 7000 | `static void StreamSelectEach(stream_select_ctx *pSel,ph7_value **apArg,int nArg,int iArg,int iWhich,` |
|      - | 7001 | `	int iMode)` |
|      1 | 7002 | `{` |
|    191 | 7003 | `	if( iArg >= nArg \|\| apArg[iArg] == 0 \|\| !ph7_value_is_array(apArg[iArg]) ){` |
|     85 | 7004 | `		return;` |
|      - | 7005 | `	}` |
|    107 | 7006 | `	pSel->iWhich = iWhich;` |
|    107 | 7007 | `	pSel->iMode = iMode;` |
|    107 | 7008 | `	ph7_array_walk(apArg[iArg],StreamSelectWalk,pSel);` |
|     96 | 7009 | `}` |
|      - | 7010 | `/* Rebuild one argument from the entries that came back ready. php REPLACES the` |
|      - | 7011 | ` * array either way, so a set with nothing ready comes back empty. */` |
|     78 | 7012 | `static int StreamSelectStore(stream_select_ctx *pSel,ph7_value **apArg,int nArg,int iArg,int iWhich,` |
|      - | 7013 | `	int iMode)` |
|      1 | 7014 | `{` |
|     79 | 7015 | `	if( iArg >= nArg \|\| apArg[iArg] == 0 \|\| !ph7_value_is_array(apArg[iArg]) ){` |
|     47 | 7016 | `		return PH7_OK;` |
|      - | 7017 | `	}` |
|     33 | 7018 | `	pSel->pOut = ph7_context_new_array(pSel->pCtx);` |
|     33 | 7019 | `	if( pSel->pOut == 0 ){` |
|      - | 7020 | `		/* Leaving the caller's array alone would answer that every entry is` |
|      - | 7021 | `		 * ready, which is the one wrong answer this function must not give. */` |
|    ! 0 | 7022 | `		return PH7_ContextMemoryError(pSel->pCtx);` |
|      - | 7023 | `	}` |
|     33 | 7024 | `	StreamSelectEach(pSel,apArg,nArg,iArg,iWhich,iMode);` |
|     33 | 7025 | `	PH7_VmStoreArgByRef(pSel->pCtx->pVm,apArg[iArg],pSel->pOut);` |
|     33 | 7026 | `	pSel->pOut = 0;` |
|     33 | 7027 | `	return PH7_OK;` |
|     40 | 7028 | `}` |
|     44 | 7029 | `PH7_PRIVATE int PH7_builtin_stream_select(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 7030 | `{` |
|      - | 7031 | `	stream_select_ctx sSel;` |
|     45 | 7032 | `	ph7_int64 iSec = 0,iUsec = 0;` |
|     45 | 7033 | `	int bBlock = 1,iErrno = 0,rc,i;` |
|     45 | 7034 | `	SyZero(&sSel,sizeof(sSel));` |
|     45 | 7035 | `	sSel.pCtx = pCtx;` |
|     45 | 7036 | `	sSel.iMaxFd = -1;` |
|      - | 7037 | `#ifdef STREAM_SELECT_OK` |
|    177 | 7038 | `	for( i = 0 ; i < 3 ; i++ ){` |
|   1189 | 7039 | `		FD_ZERO(&sSel.aSet[i]);` |
|     67 | 7040 | `	}` |
|      - | 7041 | `#endif` |
|     45 | 7042 | `	StreamSelectEach(&sSel,apArg,nArg,0,SEL_READ,SELM_COLLECT);` |
|     45 | 7043 | `	StreamSelectEach(&sSel,apArg,nArg,1,SEL_WRITE,SELM_COLLECT);` |
|     45 | 7044 | `	StreamSelectEach(&sSel,apArg,nArg,2,SEL_EXCEPT,SELM_COLLECT);` |
|     45 | 7045 | `	if( sSel.nSelectable < 1 ){` |
|      - | 7046 | `		/* php's own wording, and it carries no function name. It is the answer` |
|      - | 7047 | `		 * for three NULLs, for empty arrays, and for arrays holding nothing` |
|      - | 7048 | `		 * this engine can wait on — the caller asked to wait for nothing. */` |
|      7 | 7049 | `		return PH7_VmThrowException(pCtx,"ValueError","No stream arrays were passed");` |
|      - | 7050 | `	}` |
|     39 | 7051 | `	if( sSel.bBadEntry ){` |
|      - | 7052 | `		/* Raised only once the arrays are known to hold something to wait on —` |
|      - | 7053 | `		 * the empty-arrays Error wins over it — and BEFORE the timeout is` |
|      - | 7054 | `		 * looked at, which is the order php's own pending-exception check` |
|      - | 7055 | `		 * produces. */` |
|     10 | 7056 | `		return PH7_VmThrowException(pCtx,"TypeError",` |
|      - | 7057 | `			"%s(): supplied %s is not a valid stream resource",` |
|      6 | 7058 | `			ph7_function_name(pCtx),sSel.bBadClosed ? "resource" : "argument");` |
|      - | 7059 | `	}` |
|     33 | 7060 | `	if( nArg > 3 && !ph7_value_is_null(apArg[3]) ){` |
|     31 | 7061 | `		iSec = ph7_value_to_int64(apArg[3]);` |
|     31 | 7062 | `		bBlock = 0;` |
|     31 | 7063 | `		if( iSec < 0 ){` |
|      4 | 7064 | `			return PH7_VmThrowException(pCtx,"ValueError",` |
|      - | 7065 | `				"%s(): Argument #4 ($seconds) must be greater than or equal to 0",` |
|      1 | 7066 | `				ph7_function_name(pCtx));` |
|      - | 7067 | `		}` |
|     14 | 7068 | `	}` |
|     31 | 7069 | `	if( nArg > 4 && !ph7_value_is_null(apArg[4]) ){` |
|     15 | 7070 | `		iUsec = ph7_value_to_int64(apArg[4]);` |
|     15 | 7071 | `		if( bBlock ){` |
|      - | 7072 | `			/* php refuses the pair rather than guessing which one meant it: a` |
|      - | 7073 | `			 * NULL $seconds is "wait forever", and there is no such thing as` |
|      - | 7074 | `			 * waiting forever for five microseconds. */` |
|      3 | 7075 | `			if( iUsec != 0 ){` |
|      4 | 7076 | `				return PH7_VmThrowException(pCtx,"ValueError",` |
|      - | 7077 | `					"%s(): Argument #5 ($microseconds) must be null when argument #4 ($seconds) is null",` |
|      1 | 7078 | `					ph7_function_name(pCtx));` |
|    ! 0 | 7079 | `			}` |
|     13 | 7080 | `		}else if( iUsec < 0 ){` |
|      4 | 7081 | `			return PH7_VmThrowException(pCtx,"ValueError",` |
|      - | 7082 | `				"%s(): Argument #5 ($microseconds) must be greater than or equal to 0",` |
|      1 | 7083 | `				ph7_function_name(pCtx));` |
|      - | 7084 | `		}` |
|      5 | 7085 | `	}` |
|     27 | 7086 | `	if( iUsec > 999999 ){` |
|      - | 7087 | `		/* php carries the overflow into the seconds, because a tv_usec of a` |
|      - | 7088 | `		 * million or more is what Solaris and the BSDs refuse outright — so` |
|      - | 7089 | ``		 * `stream_select($r, $w, $x, 0, 1500000)` waits a second and a half`` |
|      - | 7090 | `		 * rather than failing. */` |
|      3 | 7091 | `		iSec += iUsec / 1000000;` |
|      3 | 7092 | `		iUsec %= 1000000;` |
|      1 | 7093 | `	}` |
|      - | 7094 | `	/* php's own shortcut, and it comes BEFORE the wait: a handle whose buffer` |
|      - | 7095 | `	 * still holds bytes the script has not taken is ready NOW, whatever the OS` |
|      - | 7096 | `	 * would say about its descriptor — the device has nothing left to report.` |
|      - | 7097 | `	 * COUNTED first and stored second, because the count is what decides` |
|      - | 7098 | `	 * whether the arrays are rewritten from the buffers or from the wait. */` |
|     27 | 7099 | `	StreamSelectEach(&sSel,apArg,nArg,0,SEL_READ,SELM_BUFFERED);` |
|     27 | 7100 | `	if( sSel.nReady > 0 ){` |
|      5 | 7101 | `		sSel.nReady = 0;` |
|      4 | 7102 | `		if( StreamSelectStore(&sSel,apArg,nArg,0,SEL_READ,SELM_BUFFERED) != PH7_OK` |
|      - | 7103 | `		/* php answers only the readable ones then, and empties the other two. */` |
|      4 | 7104 | `		 \|\| StreamSelectStore(&sSel,apArg,nArg,1,SEL_WRITE,SELM_CLEAR) != PH7_OK` |
|      5 | 7105 | `		 \|\| StreamSelectStore(&sSel,apArg,nArg,2,SEL_EXCEPT,SELM_CLEAR) != PH7_OK ){` |
|    ! 0 | 7106 | `			return PH7_ContextMemoryError(pCtx);` |
|      - | 7107 | `		}` |
|      5 | 7108 | `		ph7_result_int(pCtx,sSel.nReady);` |
|      5 | 7109 | `		return PH7_OK;` |
|      - | 7110 | `	}` |
|     23 | 7111 | `	rc = StreamSelectWait(&sSel,iSec,iUsec,bBlock,&iErrno);` |
|     23 | 7112 | `	if( rc < 0 ){` |
|      - | 7113 | `#if defined(__WINNT__) && defined(PH7_ENABLE_NET)` |
|    ! 0 | 7114 | `		const char *zErr = PH7_NetStrError(iErrno);` |
|      - | 7115 | `#else` |
|    ! 0 | 7116 | `		const char *zErr = VfsStrerror(iErrno);` |
|      - | 7117 | `#endif` |
|    ! 0 | 7118 | `		ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,"Unable to select [%d]: %s (max_fd=%d)",` |
|    ! 0 | 7119 | `			iErrno,zErr,sSel.iMaxFd);` |
|    ! 0 | 7120 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 7121 | `		return PH7_OK;` |
|      - | 7122 | `	}` |
|     22 | 7123 | `	if( StreamSelectStore(&sSel,apArg,nArg,0,SEL_READ,SELM_READY) != PH7_OK` |
|     22 | 7124 | `	 \|\| StreamSelectStore(&sSel,apArg,nArg,1,SEL_WRITE,SELM_READY) != PH7_OK` |
|     23 | 7125 | `	 \|\| StreamSelectStore(&sSel,apArg,nArg,2,SEL_EXCEPT,SELM_READY) != PH7_OK ){` |
|    ! 0 | 7126 | `		return PH7_ContextMemoryError(pCtx);` |
|      - | 7127 | `	}` |
|      - | 7128 | `	/* The COUNT is select()'s own, not the entries kept: two array members can` |
|      - | 7129 | `	 * name one descriptor, and php answers what the OS said. */` |
|     23 | 7130 | `	ph7_result_int(pCtx,rc);` |
|     23 | 7131 | `	return PH7_OK;` |
|     23 | 7132 | `}` |
|      - | 7133 | `/*` |
|      - | 7134 | ` * array stream_get_transports(void)` |
|      - | 7135 | ` *` |
|      - | 7136 | ` * The transports a stream_socket_client()/fsockopen() address may name. php's` |
|      - | 7137 | ` * own list is what its build registered, so this is what THIS engine can open:` |
|      - | 7138 | ` * the ssl/tls/udp/unix set is a recorded scope gap (§7.4), and answering for` |
|      - | 7139 | ` * transports that are not there would tell a script a connection will work` |
|      - | 7140 | ` * when it cannot.` |
|      - | 7141 | ` */` |
|      8 | 7142 | `PH7_PRIVATE int PH7_builtin_stream_get_transports(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      2 | 7143 | `{` |
|      - | 7144 | `	ph7_value *pArr,*pV;` |
|      4 | 7145 | `	SXUNUSED(nArg);` |
|      4 | 7146 | `	SXUNUSED(apArg);` |
|     10 | 7147 | `	pArr = ph7_context_new_array(pCtx);` |
|     10 | 7148 | `	pV = ph7_context_new_scalar(pCtx);` |
|     10 | 7149 | `	if( pArr == 0 \|\| pV == 0 ){` |
|    ! 0 | 7150 | `		ph7_result_null(pCtx);` |
|    ! 0 | 7151 | `		return PH7_OK;` |
|      - | 7152 | `	}` |
|      - | 7153 | `#ifdef PH7_ENABLE_NET` |
|     10 | 7154 | `	ph7_value_string(pV,"tcp",-1);` |
|     10 | 7155 | `	ph7_array_add_elem(pArr,0,pV);` |
|      - | 7156 | `#endif` |
|     10 | 7157 | `	ph7_result_value(pCtx,pArr);` |
|     10 | 7158 | `	return PH7_OK;` |
|      6 | 7159 | `}` |
|      - | 7160 | `/*` |
|      - | 7161 | ` * bool stream_supports_lock(resource $stream)` |
|      - | 7162 | ` *` |
|      - | 7163 | ` * The question flock() answers with a warning if you get it wrong: only a` |
|      - | 7164 | ` * device with a real lock operation can be locked, so a memory buffer and a` |
|      - | 7165 | ` * data:// payload are false.` |
|      - | 7166 | ` */` |
|     14 | 7167 | `PH7_PRIVATE int PH7_builtin_stream_supports_lock(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 7168 | `{` |
|      - | 7169 | `	io_private *pDev;` |
|      - | 7170 | `	int rc;` |
|     15 | 7171 | `	if( nArg < 1 ){` |
|    ! 0 | 7172 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 7173 | `		return PH7_OK;` |
|      - | 7174 | `	}` |
|     15 | 7175 | `	pDev = StreamSettingArg(pCtx,apArg[0],&rc);` |
|     15 | 7176 | `	if( pDev == 0 ){` |
|      3 | 7177 | `		return rc;` |
|      - | 7178 | `	}` |
|      - | 7179 | `	/* php locks at the DESCRIPTOR, so anything with one can be locked even` |
|      - | 7180 | `	 * when the device exposes no lock operation of its own (php://stdout, a` |
|      - | 7181 | `	 * pipe); a memory buffer and a data:// payload have neither and are the` |
|      - | 7182 | `	 * false answers. */` |
|     13 | 7183 | `	pDev = PH7_StreamUnwrap(pDev);` |
|     23 | 7184 | `	ph7_result_bool(pCtx,pDev->bDir == 0` |
|     16 | 7185 | `		&& ((pDev->pStream != 0 && pDev->pStream->xLock != 0)` |
|      7 | 7186 | `		    \|\| PH7_StreamPosixFd(pDev) >= 0));` |
|     13 | 7187 | `	return PH7_OK;` |
|      8 | 7188 | `}` |
|      - | 7189 | `/*` |
|      - | 7190 | ` * bool stream_is_local(resource\|string $stream)` |
|      - | 7191 | ` *` |
|      - | 7192 | ` * php answers from the WRAPPER, not from the path: a stream opened by a URL` |
|      - | 7193 | ` * wrapper is not local, one opened by no wrapper at all (a pipe) is not local` |
|      - | 7194 | ` * either, and everything else — including php:// and a path naming a scheme` |
|      - | 7195 | ` * nobody registered — is.` |
|      - | 7196 | ` */` |
|     28 | 7197 | `PH7_PRIVATE int PH7_builtin_stream_is_local(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 7198 | `{` |
|      - | 7199 | `	const ph7_io_stream *pStream;` |
|     29 | 7200 | `	if( nArg < 1 ){` |
|    ! 0 | 7201 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 7202 | `		return PH7_OK;` |
|      - | 7203 | `	}` |
|     29 | 7204 | `	if( ph7_value_is_string(apArg[0]) ){` |
|      - | 7205 | `		static const char * const azUrlScheme[] = { "http://", "https://", "ftp://", "ftps://" };` |
|      - | 7206 | `		int nLen,i;` |
|     21 | 7207 | `		const char *zPath = ph7_value_to_string(apArg[0],&nLen);` |
|     20 | 7208 | `		if( nLen > (int)sizeof("file://")-1` |
|     19 | 7209 | `		 && SyStrnicmp(zPath,"file://",sizeof("file://")-1) == 0` |
|     11 | 7210 | `		 && zPath[sizeof("file://")-1] != '/'` |
|      4 | 7211 | `		 && SyStrnicmp(zPath,"file://localhost/",sizeof("file://localhost/")-1) != 0 ){` |
|      - | 7212 | ``			/* `file://host/path` names a REMOTE host, which php refuses rather`` |
|      - | 7213 | `			 * than reading as a local path — so the answer is not local. */` |
|      3 | 7214 | `			ph7_result_bool(pCtx,0);` |
|      3 | 7215 | `			return PH7_OK;` |
|      - | 7216 | `		}` |
|     83 | 7217 | `		for( i = 0 ; i < (int)(sizeof(azUrlScheme)/sizeof(azUrlScheme[0])) ; i++ ){` |
|     67 | 7218 | `			int nScheme = (int)SyStrlen(azUrlScheme[i]);` |
|     67 | 7219 | `			if( nLen >= nScheme && SyStrnicmp(zPath,azUrlScheme[i],(sxu32)nScheme) == 0 ){` |
|      - | 7220 | `				/* php registers these as URL wrappers whether or not this` |
|      - | 7221 | `				 * engine can OPEN them (http:// is a recorded gap, §7.4), and` |
|      - | 7222 | `				 * "is this path local?" has to answer for the scheme rather` |
|      - | 7223 | `				 * than for what happens to be implemented — the unsafe` |
|      - | 7224 | `				 * direction is answering TRUE about a remote URL. */` |
|      3 | 7225 | `				ph7_result_bool(pCtx,0);` |
|      3 | 7226 | `				return PH7_OK;` |
|      - | 7227 | `			}` |
|     33 | 7228 | `		}` |
|     17 | 7229 | `		pStream = PH7_VmGetStreamDevice(pCtx->pVm,&zPath,nLen);` |
|      - | 7230 | `		/* An unregistered scheme has no wrapper to ask, and php answers TRUE` |
|      - | 7231 | `		 * for it — the path is taken at face value. */` |
|     17 | 7232 | `		ph7_result_bool(pCtx,pStream == 0 \|\| !PH7_StreamIsUrlWrapper(pStream));` |
|     17 | 7233 | `		return PH7_OK;` |
|      - | 7234 | `	}` |
|      - | 7235 | `	{` |
|      - | 7236 | `		int rc;` |
|      9 | 7237 | `		io_private *pDev = StreamSettingArg(pCtx,apArg[0],&rc);` |
|      - | 7238 | `		const char *zWrapper,*zLabel;` |
|      9 | 7239 | `		if( pDev == 0 ){` |
|    ! 0 | 7240 | `			return rc;` |
|      - | 7241 | `		}` |
|      9 | 7242 | `		IoPrivateStreamLabels(pDev,&zWrapper,&zLabel);` |
|      - | 7243 | `		/* No wrapper (a popen() pipe, a socket) is php's other "not local". */` |
|      9 | 7244 | `		ph7_result_bool(pCtx,zWrapper != 0 && !PH7_StreamIsUrlWrapper(pDev->pStream));` |
|      - | 7245 | `	}` |
|      9 | 7246 | `	return PH7_OK;` |
|     15 | 7247 | `}` |
|      - | 7248 | `/* PH7_ENABLE_NET */` |
|      - | 7249 | `/*` |
|      - | 7250 | ` * The open that fopen() is: the device lookup, the io_private, the mode` |
|      - | 7251 | ``  * translation, the handle and the meta-data record `stream_get_meta_data()` `` |
|      - | 7252 | ` * reports back. php's fopen() and its SplFileObject constructor both call` |
|      - | 7253 | ` * php_stream_open_wrapper_ex(), so both doors here share this body rather than` |
|      - | 7254 | ` * spelling the sequence twice.` |
|      - | 7255 | ` *` |
|      - | 7256 | ` * NOTHING is reported from in here. The two failures are handed back through` |
|      - | 7257 | ` * *piErr, because the two callers word them differently: fopen() warns, while` |
|      - | 7258 | ` * SplFileObject's constructor promotes the same warning to a RuntimeException` |
|      - | 7259 | ` * (php's zend_replace_error_handling). *pzErrUri is the name to report -- the` |
|      - | 7260 | ` * scheme-stripped remainder, which is what the warning has always printed.` |
|      - | 7261 | ` */` |
|   1562 | 7262 | `PH7_PRIVATE io_private * PH7_StreamOpenPath(ph7_context *pCtx,ph7_value *pPath,` |
|      - | 7263 | `	const char *zMode,int nMode,int bUseInclude,phl_stream_ctx *pCtxRes,` |
|      - | 7264 | `	ph7_value *pCtxArg,int *piErr,const char **pzErrUri)` |
|      5 | 7265 | `{` |
|      - | 7266 | `	const ph7_io_stream *pStream;` |
|      - | 7267 | `	const char *zUri;` |
|      - | 7268 | `	ph7_value *pResource;` |
|      - | 7269 | `	io_private *pDev;` |
|      - | 7270 | `	int iLen,iOpenFlags;` |
|   1567 | 7271 | `	zUri = ph7_value_to_string(pPath,&iLen);` |
|   1567 | 7272 | `	*piErr = PH7_STREAM_OPEN_OK;` |
|   1567 | 7273 | `	*pzErrUri = zUri;` |
|      - | 7274 | `	/* Try to extract a stream */` |
|   1567 | 7275 | `	pStream = PH7_VmGetStreamDevice(pCtx->pVm,&zUri,iLen);` |
|   1567 | 7276 | `	*pzErrUri = zUri;` |
|   1567 | 7277 | `	if( pStream == 0 ){` |
|    ! 0 | 7278 | `		*piErr = PH7_STREAM_OPEN_NODEVICE;` |
|    ! 0 | 7279 | `		return 0;` |
|      - | 7280 | `	}` |
|      - | 7281 | `	/* php's mode grammar belongs to the PLAIN-FILE wrapper and to nothing else:` |
|      - | 7282 | `	 * php://, data:// and a userland wrapper are handed whatever the caller` |
|      - | 7283 | ``	 * wrote and decide for themselves (`fopen('php://memory','zz')` opens`` |
|      - | 7284 | `	 * read-only rather than failing), so only the default device refuses. */` |
|   1567 | 7285 | `	if( StrModeToFlags(zMode,nMode,&iOpenFlags) != 0 ){` |
|     39 | 7286 | `		if( pStream == pCtx->pVm->pDefStream ){` |
|     39 | 7287 | `			*piErr = PH7_STREAM_OPEN_BADMODE;` |
|     39 | 7288 | `			return 0;` |
|      - | 7289 | `		}` |
|    ! 0 | 7290 | `		iOpenFlags = PH7_IO_OPEN_RDONLY;` |
|    ! 0 | 7291 | `	}` |
|      - | 7292 | `	/* Allocate a new IO private instance */` |
|   1529 | 7293 | `	pDev = (io_private *)ph7_context_alloc_chunk(pCtx,sizeof(io_private),TRUE,FALSE);` |
|   1529 | 7294 | `	if( pDev == 0 ){` |
|    ! 0 | 7295 | `		*piErr = PH7_STREAM_OPEN_NOMEM;` |
|    ! 0 | 7296 | `		return 0;` |
|      - | 7297 | `	}` |
|   1529 | 7298 | `	pResource = 0;` |
|   1529 | 7299 | `	if( pCtxArg ){` |
|     10 | 7300 | `		pResource = pCtxArg;` |
|   1525 | 7301 | `	}else if( is_php_stream(pStream) \|\| is_data_stream(pStream) ){` |
|      - | 7302 | `		/* TICKET 1433-80: The php:// and data:// streams need a ph7_value to` |
|      - | 7303 | `		 * access the underlying virtual machine.` |
|      - | 7304 | `		 */` |
|    457 | 7305 | `		pResource = pPath;` |
|    226 | 7306 | `	}` |
|      - | 7307 | `	/* Initialize the structure */` |
|   1529 | 7308 | `	InitIOPrivate(pCtx->pVm,pStream,pDev);` |
|   1529 | 7309 | `	PH7_StreamCtxArm(pCtx->pVm,pCtxRes);` |
|      - | 7310 | `	/* Try to get a handle */` |
|   2291 | 7311 | `	pDev->pHandle = PH7_StreamOpenHandle(pCtx->pVm,pStream,zUri,iOpenFlags,` |
|    762 | 7312 | `		bUseInclude,pResource,FALSE,0,ph7_function_name(pCtx));` |
|   1529 | 7313 | `	if( pDev->pHandle == 0 ){` |
|     29 | 7314 | `		ReleaseIOPrivate(pCtx,pDev);` |
|     29 | 7315 | `		*piErr = PH7_STREAM_OPEN_FAILED;` |
|     29 | 7316 | `		return 0;` |
|      - | 7317 | `	}` |
|      - | 7318 | `	/* Remember what we were asked for: stream_get_meta_data() reports both.` |
|      - | 7319 | `	 * The URI is the ORIGINAL argument, not the scheme-stripped remainder` |
|      - | 7320 | `	 * PH7_VmGetStreamDevice() advanced zUri past. */` |
|      - | 7321 | `	{` |
|      - | 7322 | `		int nUri;` |
|   1503 | 7323 | `		const char *zOrig = ph7_value_to_string(pPath,&nUri);` |
|   1503 | 7324 | `		const char *zMeta = zMode;` |
|   1503 | 7325 | `		int nMeta = nMode;` |
|   1498 | 7326 | `		if( is_php_stream(pStream)` |
|    969 | 7327 | `		 && PH7_PhpStreamKind(pDev->pHandle) == PH7_IO_STREAM_OUTPUT ){` |
|      - | 7328 | `			/* php://output has one mode whatever it was asked for. */` |
|      5 | 7329 | `			zMeta = "wb";` |
|      5 | 7330 | `			nMeta = 2;` |
|   1497 | 7331 | `		}else if( is_php_stream(pStream)` |
|    965 | 7332 | `		 && PH7_PhpStreamKind(pDev->pHandle) == PH7_IO_STREAM_MEMORY ){` |
|      - | 7333 | `			/* php's memory streams do not keep the mode they were opened with:` |
|      - | 7334 | `			 * a buffer is readable and writable either way, so php reports the` |
|      - | 7335 | `			 * one it actually built. */` |
|    620 | 7336 | `			int i,bWrite = 0,bAppend = nMode > 0 && (zMode[0] == 'a' \|\| zMode[0] == 'A');` |
|   1175 | 7337 | `			for( i = 0 ; i < nMode ; i++ ){` |
|    760 | 7338 | `				if( zMode[i] == 'w' \|\| zMode[i] == 'W' \|\| zMode[i] == 'a'` |
|    562 | 7339 | `				 \|\| zMode[i] == 'A' \|\| zMode[i] == '+' ){` |
|    534 | 7340 | `					bWrite = 1;` |
|    265 | 7341 | `				}` |
|    385 | 7342 | `			}` |
|    415 | 7343 | `			zMeta = bWrite ? (bAppend ? "a+b" : "w+b") : "rb";` |
|    415 | 7344 | `			nMeta = (int)SyStrlen(zMeta);` |
|    205 | 7345 | `		}` |
|   1503 | 7346 | `		SetIOPrivateOpenedAs(pDev,zOrig,nUri,zMeta,nMeta);` |
|      - | 7347 | `	}` |
|   1503 | 7348 | `	return pDev;` |
|    786 | 7349 | `}` |
|   1436 | 7350 | `PH7_PRIVATE int PH7_builtin_fopen(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      5 | 7351 | `{` |
|      - | 7352 | `	const char *zMode,*zErrUri;` |
|      - | 7353 | `	io_private *pDev;` |
|      - | 7354 | `	phl_stream_ctx *pCtxRes;` |
|   1441 | 7355 | `	int imLen,bThrew = 0,iErr;` |
|   1441 | 7356 | `	if( nArg < 1 \|\| !ph7_value_is_string(apArg[0]) ){` |
|      - | 7357 | `		/* Missing/Invalid arguments,return FALSE */` |
|    ! 0 | 7358 | `		ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"Expecting a file path or URL");` |
|    ! 0 | 7359 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 7360 | `		return PH7_OK;` |
|      - | 7361 | `	}` |
|      - | 7362 | `	/* Extract the desired access mode */` |
|   1441 | 7363 | `	if( nArg > 1 ){` |
|   1441 | 7364 | `		zMode = ph7_value_to_string(apArg[1],&imLen);` |
|    723 | 7365 | `	}else{` |
|      - | 7366 | `		/* Set a default read-only mode */` |
|    ! 0 | 7367 | `		zMode = "r";` |
|    ! 0 | 7368 | `		imLen = (int)sizeof(char);` |
|      - | 7369 | `	}` |
|      - | 7370 | ``	/* php's `?resource $context`: a resource that is not a stream-context is`` |
|      - | 7371 | `	 * refused, and NULL means the DEFAULT context — never "no context at all".` |
|      - | 7372 | `	 * Resolved before the io_private chunk below, which a throw could not` |
|      - | 7373 | `	 * release. */` |
|   1441 | 7374 | `	pCtxRes = PH7_StreamCtxFromArg(pCtx,nArg,apArg,3,"$context",0,&bThrew);` |
|   1441 | 7375 | `	if( bThrew ){` |
|      5 | 7376 | `		return PH7_OK;` |
|      - | 7377 | `	}` |
|   2157 | 7378 | `	pDev = PH7_StreamOpenPath(pCtx,apArg[0],zMode,imLen,` |
|    720 | 7379 | `		nArg > 2 ? ph7_value_to_bool(apArg[2]) : FALSE,pCtxRes,` |
|    716 | 7380 | `		nArg > 3 ? apArg[3] : 0,&iErr,&zErrUri);` |
|   1437 | 7381 | `	if( pDev == 0 ){` |
|     61 | 7382 | `		if( iErr == PH7_STREAM_OPEN_NODEVICE ){` |
|    ! 0 | 7383 | `			VfsThrowNoDeviceWarning(pCtx,zErrUri,FALSE);` |
|     61 | 7384 | `		}else if( iErr == PH7_STREAM_OPEN_BADMODE ){` |
|     55 | 7385 | `			PH7_VmThrowWarningFmt(pCtx->pVm,` |
|      - | 7386 | ``				"%s(%s): Failed to open stream: `%.*s' is not a valid mode for fopen",`` |
|     18 | 7387 | `				ph7_function_name(pCtx),zErrUri,imLen,zMode);` |
|     43 | 7388 | `		}else if( iErr == PH7_STREAM_OPEN_FAILED ){` |
|     25 | 7389 | `			VfsThrowOpenWarning(pCtx,zErrUri);` |
|     14 | 7390 | `		}else{` |
|    ! 0 | 7391 | `			ph7_context_throw_error(pCtx,PH7_CTX_ERR,"PH7 is running out of memory");` |
|      - | 7392 | `		}` |
|     61 | 7393 | `		ph7_result_bool(pCtx,0);` |
|     61 | 7394 | `		return PH7_OK;` |
|      - | 7395 | `	}` |
|      - | 7396 | `	/* All done,return the io_private instance as a resource */` |
|   1379 | 7397 | `	ph7_result_resource(pCtx,pDev);` |
|   1379 | 7398 | `	return PH7_OK;` |
|    723 | 7399 | `}` |
|      - | 7400 | `/*` |
|      - | 7401 | ` * bool fclose(resource $handle)` |
|      - | 7402 | ` *  Closes an open file pointer` |
|      - | 7403 | ` * Parameters` |
|      - | 7404 | ` *  $handle` |
|      - | 7405 | ` *   The file pointer.` |
|      - | 7406 | ` * Return` |
|      - | 7407 | ` *  TRUE on success or FALSE on failure.` |
|      - | 7408 | ` */` |
|   1624 | 7409 | `PH7_PRIVATE int PH7_builtin_fclose(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      5 | 7410 | `{` |
|      - | 7411 | `	const ph7_io_stream *pStream;` |
|      - | 7412 | `	io_private *pDev;` |
|      - | 7413 | `	ph7_vm *pVm;` |
|   1629 | 7414 | `	if( nArg < 1 \|\| !ph7_value_is_resource(apArg[0]) ){` |
|      - | 7415 | `		/* Missing/Invalid arguments,return FALSE */` |
|    ! 0 | 7416 | `		ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"Expecting an IO handle");` |
|    ! 0 | 7417 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 7418 | `		return PH7_OK;` |
|      - | 7419 | `	}` |
|      - | 7420 | `	/* Extract our private data */` |
|   1629 | 7421 | `	pDev = (io_private *)ph7_value_to_resource(apArg[0]);` |
|      - | 7422 | `	/* php: fclose() on an already-closed stream raises a catchable TypeError */` |
|   1629 | 7423 | `	if( pDev != 0 && pDev->iMagic == IO_PRIVATE_CLOSED_MAGIC ){` |
|      3 | 7424 | `		return PH7_VmThrowException(pCtx,"TypeError",` |
|      - | 7425 | `			"fclose(): Argument #1 ($stream) must be an open stream resource");` |
|      - | 7426 | `	}` |
|      - | 7427 | `	/* Make sure we are dealing with a valid io_private instance */` |
|   1627 | 7428 | `	if( IO_PRIVATE_INVALID(pDev) ){` |
|      - | 7429 | `		/*Expecting an IO handle */` |
|    ! 0 | 7430 | `		ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"Expecting an IO handle");` |
|    ! 0 | 7431 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 7432 | `		return PH7_OK;` |
|      - | 7433 | `	}` |
|      - | 7434 | `	/* Point to the target IO stream device */` |
|   1627 | 7435 | `	pStream = pDev->pStream;` |
|   1627 | 7436 | `	if( pStream == 0 ){` |
|    ! 0 | 7437 | `		ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,` |
|      - | 7438 | `			"IO routine(%s) not implemented in the underlying stream(%s) device,PH7 is returning FALSE",` |
|    ! 0 | 7439 | `			ph7_function_name(pCtx),pStream ? pStream->zName : "null_stream"` |
|      - | 7440 | `			);` |
|    ! 0 | 7441 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 7442 | `		return PH7_OK;` |
|      - | 7443 | `	}` |
|      - | 7444 | `	/* Point to the VM that own this context */` |
|   1627 | 7445 | `	pVm = pCtx->pVm;` |
|      - | 7446 | `	/* TICKET 1433-62: Keep the STDIN/STDOUT/STDERR handles open */` |
|   1627 | 7447 | `	if( pDev != pVm->pStdin && pDev != pVm->pStdout && pDev != pVm->pStderr ){` |
|      - | 7448 | `		/* The WRITE chain gets its closing call while the device is still open:` |
|      - | 7449 | `		 * a filter that buffers has nowhere else to put its tail, and php's own` |
|      - | 7450 | `		 * close flushes before it closes. */` |
|   1627 | 7451 | `		PH7_StreamFilterReleaseChains(pDev);` |
|      - | 7452 | `		/* Perform the requested operation */` |
|   1627 | 7453 | `		PH7_StreamCloseHandle(pStream,pDev->pHandle);` |
|      - | 7454 | `		/* Keep the handle alive but flag it closed so shared copies see it */` |
|   1627 | 7455 | `		MarkIOPrivateClosed(pDev);` |
|    811 | 7456 | `	}` |
|      - | 7457 | `	/* Return TRUE */` |
|   1627 | 7458 | `	ph7_result_bool(pCtx,1);` |
|   1627 | 7459 | `	return PH7_OK;` |
|    817 | 7460 | `}` |
|      - | 7461 | `#if !defined(PH7_DISABLE_HASH_FUNC)` |
|      - | 7462 | `/*` |
|      - | 7463 | ` * MD5/SHA1 digest consumer.` |
|      - | 7464 | ` */` |
|    136 | 7465 | `static int vfsHashConsumer(const void *pData,unsigned int nLen,void *pUserData)` |
|      2 | 7466 | `{` |
|      - | 7467 | `	/* Append hex chunk verbatim */` |
|    138 | 7468 | `	ph7_result_string((ph7_context *)pUserData,(const char *)pData,(int)nLen);` |
|    138 | 7469 | `	return SXRET_OK;` |
|      2 | 7470 | `}` |
|      - | 7471 | `/*` |
|      - | 7472 | ` * string md5_file(string $uri[,bool $raw_output = false ])` |
|      - | 7473 | ` *  Calculates the md5 hash of a given file.` |
|      - | 7474 | ` * Parameters` |
|      - | 7475 | ` *  $uri` |
|      - | 7476 | ` *   Target URI (file(/path/to/something) or URL(http://www.symisc.net/))` |
|      - | 7477 | ` *  $raw_output` |
|      - | 7478 | ` *   When TRUE, returns the digest in raw binary format with a length of 16.` |
|      - | 7479 | ` * Return` |
|      - | 7480 | ` *  Return the MD5 digest on success or FALSE on failure.` |
|      - | 7481 | ` */` |
|      8 | 7482 | `PH7_PRIVATE int PH7_builtin_md5_file(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      3 | 7483 | `{` |
|      - | 7484 | `	const ph7_io_stream *pStream;` |
|      - | 7485 | `	unsigned char zDigest[16];` |
|     11 | 7486 | `	int raw_output  = FALSE;` |
|      - | 7487 | `	const char *zFile;` |
|      - | 7488 | `	MD5Context sCtx;` |
|      - | 7489 | `	char zBuf[8192];` |
|      - | 7490 | `	void *pHandle;` |
|      - | 7491 | `	ph7_int64 n;` |
|      - | 7492 | `	int nLen;` |
|     11 | 7493 | `	if( nArg < 1 \|\| !ph7_value_is_string(apArg[0]) ){` |
|      - | 7494 | `		/* Missing/Invalid arguments,return FALSE */` |
|    ! 0 | 7495 | `		ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"Expecting a file path");` |
|    ! 0 | 7496 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 7497 | `		return PH7_OK;` |
|      - | 7498 | `	}` |
|      - | 7499 | `	/* Extract the file path */` |
|     11 | 7500 | `	zFile = ph7_value_to_string(apArg[0],&nLen);` |
|      - | 7501 | `	/* Point to the target IO stream device */` |
|     11 | 7502 | `	pStream = PH7_VmGetStreamDevice(pCtx->pVm,&zFile,nLen);` |
|     11 | 7503 | `	if( pStream == 0 ){` |
|    ! 0 | 7504 | `		VfsThrowNoDeviceWarning(pCtx,zFile,FALSE);` |
|    ! 0 | 7505 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 7506 | `		return PH7_OK;` |
|      - | 7507 | `	}` |
|     11 | 7508 | `	if( nArg > 1 ){` |
|    ! 0 | 7509 | `		raw_output = ph7_value_to_bool(apArg[1]);` |
|    ! 0 | 7510 | `	}` |
|      - | 7511 | `	/* Try to open the file in read-only mode */` |
|     11 | 7512 | `	pHandle = PH7_StreamOpenHandle(pCtx->pVm,pStream,zFile,PH7_IO_OPEN_RDONLY,FALSE,0,FALSE,0,ph7_function_name(pCtx));` |
|     11 | 7513 | `	if( pHandle == 0 ){` |
|      3 | 7514 | `		VfsThrowOpenWarning(pCtx,zFile);` |
|      3 | 7515 | `		ph7_result_bool(pCtx,0);` |
|      3 | 7516 | `		return PH7_OK;` |
|      - | 7517 | `	}` |
|      - | 7518 | `	/* Init the MD5 context */` |
|      8 | 7519 | `	MD5Init(&sCtx);` |
|      - | 7520 | `	/* Perform the requested operation */` |
|      4 | 7521 | `	for(;;){` |
|     10 | 7522 | `		n = pStream->xRead(pHandle,zBuf,sizeof(zBuf));` |
|     10 | 7523 | `		if( n < 1 ){` |
|      - | 7524 | `			/* EOF or IO error,break immediately */` |
|      8 | 7525 | `			break;` |
|      - | 7526 | `		}` |
|      3 | 7527 | `		MD5Update(&sCtx,(const unsigned char *)zBuf,(unsigned int)n);` |
|      1 | 7528 | `	}` |
|      - | 7529 | `	/* Close the stream */` |
|      8 | 7530 | `	PH7_StreamCloseHandle(pStream,pHandle);` |
|      - | 7531 | `	/* Extract the digest */` |
|      8 | 7532 | `	MD5Final(zDigest,&sCtx);` |
|      8 | 7533 | `	if( raw_output ){` |
|      - | 7534 | `		/* Output raw digest */` |
|    ! 0 | 7535 | `		ph7_result_string(pCtx,(const char *)zDigest,sizeof(zDigest));` |
|    ! 0 | 7536 | `	}else{` |
|      - | 7537 | `		/* Perform a binary to hex conversion */` |
|      8 | 7538 | `		SyBinToHexConsumer((const void *)zDigest,sizeof(zDigest),vfsHashConsumer,pCtx);` |
|      - | 7539 | `	}` |
|      8 | 7540 | `	return PH7_OK;` |
|      7 | 7541 | `}` |
|      - | 7542 | `/*` |
|      - | 7543 | ` * string sha1_file(string $uri[,bool $raw_output = false ])` |
|      - | 7544 | ` *  Calculates the SHA1 hash of a given file.` |
|      - | 7545 | ` * Parameters` |
|      - | 7546 | ` *  $uri` |
|      - | 7547 | ` *   Target URI (file(/path/to/something) or URL(http://www.symisc.net/))` |
|      - | 7548 | ` *  $raw_output` |
|      - | 7549 | ` *   When TRUE, returns the digest in raw binary format with a length of 20.` |
|      - | 7550 | ` * Return` |
|      - | 7551 | ` *  Return the SHA1 digest on success or FALSE on failure.` |
|      - | 7552 | ` */` |
|      2 | 7553 | `PH7_PRIVATE int PH7_builtin_sha1_file(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 7554 | `{` |
|      - | 7555 | `	const ph7_io_stream *pStream;` |
|      - | 7556 | `	unsigned char zDigest[20];` |
|      3 | 7557 | `	int raw_output  = FALSE;` |
|      - | 7558 | `	const char *zFile;` |
|      - | 7559 | `	SHA1Context sCtx;` |
|      - | 7560 | `	char zBuf[8192];` |
|      - | 7561 | `	void *pHandle;` |
|      - | 7562 | `	ph7_int64 n;` |
|      - | 7563 | `	int nLen;` |
|      3 | 7564 | `	if( nArg < 1 \|\| !ph7_value_is_string(apArg[0]) ){` |
|      - | 7565 | `		/* Missing/Invalid arguments,return FALSE */` |
|    ! 0 | 7566 | `		ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"Expecting a file path");` |
|    ! 0 | 7567 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 7568 | `		return PH7_OK;` |
|      - | 7569 | `	}` |
|      - | 7570 | `	/* Extract the file path */` |
|      3 | 7571 | `	zFile = ph7_value_to_string(apArg[0],&nLen);` |
|      - | 7572 | `	/* Point to the target IO stream device */` |
|      3 | 7573 | `	pStream = PH7_VmGetStreamDevice(pCtx->pVm,&zFile,nLen);` |
|      3 | 7574 | `	if( pStream == 0 ){` |
|    ! 0 | 7575 | `		VfsThrowNoDeviceWarning(pCtx,zFile,FALSE);` |
|    ! 0 | 7576 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 7577 | `		return PH7_OK;` |
|      - | 7578 | `	}` |
|      3 | 7579 | `	if( nArg > 1 ){` |
|    ! 0 | 7580 | `		raw_output = ph7_value_to_bool(apArg[1]);` |
|    ! 0 | 7581 | `	}` |
|      - | 7582 | `	/* Try to open the file in read-only mode */` |
|      3 | 7583 | `	pHandle = PH7_StreamOpenHandle(pCtx->pVm,pStream,zFile,PH7_IO_OPEN_RDONLY,FALSE,0,FALSE,0,ph7_function_name(pCtx));` |
|      3 | 7584 | `	if( pHandle == 0 ){` |
|    ! 0 | 7585 | `		VfsThrowOpenWarning(pCtx,zFile);` |
|    ! 0 | 7586 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 7587 | `		return PH7_OK;` |
|      - | 7588 | `	}` |
|      - | 7589 | `	/* Init the SHA1 context */` |
|      3 | 7590 | `	SHA1Init(&sCtx);` |
|      - | 7591 | `	/* Perform the requested operation */` |
|      2 | 7592 | `	for(;;){` |
|      5 | 7593 | `		n = pStream->xRead(pHandle,zBuf,sizeof(zBuf));` |
|      5 | 7594 | `		if( n < 1 ){` |
|      - | 7595 | `			/* EOF or IO error,break immediately */` |
|      3 | 7596 | `			break;` |
|      - | 7597 | `		}` |
|      3 | 7598 | `		SHA1Update(&sCtx,(const unsigned char *)zBuf,(unsigned int)n);` |
|      1 | 7599 | `	}` |
|      - | 7600 | `	/* Close the stream */` |
|      3 | 7601 | `	PH7_StreamCloseHandle(pStream,pHandle);` |
|      - | 7602 | `	/* Extract the digest */` |
|      3 | 7603 | `	SHA1Final(&sCtx,zDigest);` |
|      3 | 7604 | `	if( raw_output ){` |
|      - | 7605 | `		/* Output raw digest */` |
|    ! 0 | 7606 | `		ph7_result_string(pCtx,(const char *)zDigest,sizeof(zDigest));` |
|    ! 0 | 7607 | `	}else{` |
|      - | 7608 | `		/* Perform a binary to hex conversion */` |
|      3 | 7609 | `		SyBinToHexConsumer((const void *)zDigest,sizeof(zDigest),vfsHashConsumer,pCtx);` |
|      - | 7610 | `	}` |
|      3 | 7611 | `	return PH7_OK;` |
|      2 | 7612 | `}` |
|      - | 7613 | `#endif /* PH7_DISABLE_HASH_FUNC */` |
|      - | 7614 | `/*` |
|      - | 7615 | ` * array parse_ini_file(string $filename[, bool $process_sections = false [, int $scanner_mode = INI_SCANNER_NORMAL ]] )` |
|      - | 7616 | ` *  Parse a configuration file.` |
|      - | 7617 | ` * Parameters` |
|      - | 7618 | ` * $filename` |
|      - | 7619 | ` *  The filename of the ini file being parsed.` |
|      - | 7620 | ` * $process_sections` |
|      - | 7621 | ` *  By setting the process_sections parameter to TRUE, you get a multidimensional array` |
|      - | 7622 | ` *  with the section names and settings included.` |
|      - | 7623 | ` *  The default for process_sections is FALSE.` |
|      - | 7624 | ` * $scanner_mode` |
|      - | 7625 | ` *  Can either be INI_SCANNER_NORMAL (default) or INI_SCANNER_RAW.` |
|      - | 7626 | ` *  If INI_SCANNER_RAW is supplied, then option values will not be parsed.` |
|      - | 7627 | ` * Return` |
|      - | 7628 | ` *  The settings are returned as an associative array on success.` |
|      - | 7629 | ` *  Otherwise is returned.` |
|      - | 7630 | ` */` |
|      8 | 7631 | `PH7_PRIVATE int PH7_builtin_parse_ini_file(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 7632 | `{` |
|      - | 7633 | `	const ph7_io_stream *pStream;` |
|      - | 7634 | `	const char *zFile;` |
|      - | 7635 | `	SyBlob sContents;` |
|      - | 7636 | `	void *pHandle;` |
|      - | 7637 | `	int nLen;` |
|      9 | 7638 | `	int iMode = PH7_INI_SCANNER_NORMAL;` |
|      9 | 7639 | `	sxi32 rc = PH7_OK;` |
|      9 | 7640 | `	if( nArg < 1 \|\| !ph7_value_is_string(apArg[0]) ){` |
|      - | 7641 | `		/* Missing/Invalid arguments,return FALSE */` |
|    ! 0 | 7642 | `		ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"Expecting a file path");` |
|    ! 0 | 7643 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 7644 | `		return PH7_OK;` |
|      - | 7645 | `	}` |
|      9 | 7646 | `	if( nArg > 2 && ph7_value_is_int(apArg[2]) ){` |
|      7 | 7647 | `		iMode = ph7_value_to_int(apArg[2]);` |
|      6 | 7648 | `		if( iMode != PH7_INI_SCANNER_NORMAL && iMode != PH7_INI_SCANNER_RAW` |
|      6 | 7649 | `		 && iMode != PH7_INI_SCANNER_TYPED ){` |
|      - | 7650 | `			/* php screens the mode BEFORE touching the file */` |
|      - | 7651 | ``			/* php's bare message: no `func(): ` qualifier on this one */`` |
|      3 | 7652 | `			PH7_VmThrowError(pCtx->pVm,0,PH7_CTX_WARNING,"Invalid scanner mode");` |
|      3 | 7653 | `			ph7_result_bool(pCtx,0);` |
|      3 | 7654 | `			return PH7_OK;` |
|      - | 7655 | `		}` |
|      2 | 7656 | `	}` |
|      - | 7657 | `	/* Extract the file path */` |
|      7 | 7658 | `	zFile = ph7_value_to_string(apArg[0],&nLen);` |
|      - | 7659 | `	/* Point to the target IO stream device */` |
|      7 | 7660 | `	pStream = PH7_VmGetStreamDevice(pCtx->pVm,&zFile,nLen);` |
|      7 | 7661 | `	if( pStream == 0 ){` |
|    ! 0 | 7662 | `		VfsThrowNoDeviceWarning(pCtx,zFile,FALSE);` |
|    ! 0 | 7663 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 7664 | `		return PH7_OK;` |
|      - | 7665 | `	}` |
|      - | 7666 | `	/* Try to open the file in read-only mode */` |
|      7 | 7667 | `	pHandle = PH7_StreamOpenHandle(pCtx->pVm,pStream,zFile,PH7_IO_OPEN_RDONLY,FALSE,0,FALSE,0,ph7_function_name(pCtx));` |
|      7 | 7668 | `	if( pHandle == 0 ){` |
|    ! 0 | 7669 | `		VfsThrowOpenWarning(pCtx,zFile);` |
|    ! 0 | 7670 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 7671 | `		return PH7_OK;` |
|      - | 7672 | `	}` |
|      7 | 7673 | `	SyBlobInit(&sContents,&pCtx->pVm->sAllocator);` |
|      - | 7674 | `	/* Read the whole file */` |
|      7 | 7675 | `	PH7_StreamReadWholeFile(pHandle,pStream,&sContents);` |
|      7 | 7676 | `	if( SyBlobLength(&sContents) < 1 ){` |
|      - | 7677 | `		/* Empty buffer,return FALSE */` |
|    ! 0 | 7678 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 7679 | `	}else{` |
|      - | 7680 | `		/* Process the raw INI buffer; capture an OOM abort to propagate below */` |
|     13 | 7681 | `		rc = PH7_ParseIniString(pCtx,(const char *)SyBlobData(&sContents),SyBlobLength(&sContents),` |
|      6 | 7682 | `			nArg > 1 ? ph7_value_to_bool(apArg[1]) : 0,iMode);` |
|      - | 7683 | `	}` |
|      - | 7684 | `	/* Close the stream */` |
|      7 | 7685 | `	PH7_StreamCloseHandle(pStream,pHandle);` |
|      - | 7686 | `	/* Release the working buffer */` |
|      7 | 7687 | `	SyBlobRelease(&sContents);` |
|      - | 7688 | `	/* Propagate an OOM abort so the fatal actually halts the VM */` |
|      7 | 7689 | `	return rc;` |
|      5 | 7690 | `}` |
|      - | 7691 | `/* ZIP archive processing moved to vfs_zip.c */` |
|      - | 7692 | `#else /* PH7_DISABLE_DISK_IO */` |
|      - | 7693 | `/*` |
|      - | 7694 | ` * Disk I/O is compiled out: this VFS hands out no resource handles, so` |
|      - | 7695 | ` * get_resource_type() has nothing that could be a "stream" and every` |
|      - | 7696 | ` * resource reports as "Unknown" (the same fallback the full build gives` |
|      - | 7697 | ` * to any non-VFS resource).` |
|      - | 7698 | ` */` |
|      - | 7699 | `PH7_PRIVATE const char * PH7_VfsResourceType(void *pResource)` |
|      - | 7700 | `{` |
|      - | 7701 | `	SXUNUSED(pResource);` |
|      - | 7702 | `	return "Unknown";` |
|      - | 7703 | `}` |
|      - | 7704 | `/* No disk I/O means no closeable handles: nothing is ever a closed resource. */` |
|      - | 7705 | `PH7_PRIVATE int PH7_VfsResourceIsClosed(void *pResource)` |
|      - | 7706 | `{` |
|      - | 7707 | `	SXUNUSED(pResource);` |
|      - | 7708 | `	return 0;` |
|      - | 7709 | `}` |
|      - | 7710 | `/* No streams means no stream contexts either, but PH7_VmReset still calls this. */` |
|      - | 7711 | `PH7_PRIVATE void PH7_StreamCtxVmReset(ph7_vm *pVm)` |
|      - | 7712 | `{` |
|      - | 7713 | `	SXUNUSED(pVm);` |
|      - | 7714 | `}` |
|      - | 7715 | `/* Same for the filter registry. */` |
|      - | 7716 | `PH7_PRIVATE void PH7_StreamFilterVmReset(ph7_vm *pVm)` |
|      - | 7717 | `{` |
|      - | 7718 | `	SXUNUSED(pVm);` |
|      - | 7719 | `}` |
|      - | 7720 | `#endif /* PH7_DISABLE_DISK_IO */` |
|      - | 7721 |  |
