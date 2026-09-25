# src/ph7/vfs_stream.c

<style>code, pre { background: none !important; white-space: pre !important; width: 100% !important; display: inline-block !important; } td { border: none !important; margin-top: 0 !important; margin-bottom: 0 !important; padding-top: 0 !important; padding-bottom: 0 !important; }</style>

Coverage: 3255/4015 lines (81.07%)

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
|    438 |   40 | `static void StreamSeekBackForWrite(io_private *pDev)` |
|      5 |   41 | `{` |
|    443 |   42 | `	sxu32 nAhead = StreamAheadBytes(pDev);` |
|    443 |   43 | `	if( nAhead > 0 && pDev->pStream && pDev->pStream->xSeek ){` |
|     16 |   44 | `		pDev->pStream->xSeek(pDev->pHandle,-(ph7_int64)nAhead,1/*SEEK_CUR*/);` |
|     16 |   45 | `		ResetIOPrivate(pDev);` |
|      7 |   46 | `	}` |
|    443 |   47 | `}` |
|     84 |   48 | `PH7_PRIVATE ph7_int64 PH7_StreamLogicalTell(io_private *pDev)` |
|      4 |   49 | `{` |
|      - |   50 | `	ph7_int64 iOfft;` |
|     88 |   51 | `	if( pDev == 0 ){` |
|    ! 0 |   52 | `		return -1;` |
|      - |   53 | `	}` |
|     88 |   54 | `	if( pDev->pReadFilters ){` |
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
|     62 |   66 | `	if( pDev->pStream == 0 \|\| pDev->pStream->xTell == 0 ){` |
|    ! 0 |   67 | `		return -1;` |
|      - |   68 | `	}` |
|     62 |   69 | `	iOfft = pDev->pStream->xTell(pDev->pHandle);` |
|     62 |   70 | `	if( iOfft < 0 ){` |
|    ! 0 |   71 | `		return iOfft;` |
|      - |   72 | `	}` |
|     62 |   73 | `	return iOfft - (ph7_int64)StreamAheadBytes(pDev);` |
|     46 |   74 | `}` |
|      - |   75 | `/*` |
|      - |   76 | ` * Seek the stream a php://filter proxy wraps. Same model as fseek() on a` |
|      - |   77 | ` * filtered handle: a relative move is resolved against the position the SCRIPT` |
|      - |   78 | ` * sees, because the device's own offset is not comparable to it.` |
|      - |   79 | ` */` |
|      8 |   80 | `PH7_PRIVATE int PH7_StreamSeekWrapped(io_private *pDev,ph7_int64 iOfft,int whence)` |
|      1 |   81 | `{` |
|      - |   82 | `	int rc;` |
|      9 |   83 | `	if( pDev == 0 \|\| pDev->pStream == 0 \|\| pDev->pStream->xSeek == 0 ){` |
|    ! 0 |   84 | `		return -1;` |
|      - |   85 | `	}` |
|      9 |   86 | `	if( pDev->pReadFilters && whence != 2 /* SEEK_END */ ){` |
|      9 |   87 | `		if( whence == 1 /* SEEK_CUR */ ){` |
|      3 |   88 | `			iOfft += PH7_StreamLogicalTell(pDev);` |
|      1 |   89 | `		}` |
|      9 |   90 | `		whence = 0; /* SEEK_SET */` |
|      4 |   91 | `	}` |
|      9 |   92 | `	rc = pDev->pStream->xSeek(pDev->pHandle,iOfft,whence);` |
|      9 |   93 | `	if( rc == PH7_OK ){` |
|      9 |   94 | `		SyBlobReset(&pDev->sBuffer);` |
|      9 |   95 | `		pDev->nOfft = 0;` |
|      9 |   96 | `		SyBlobReset(&pDev->sFilt);` |
|      9 |   97 | `		pDev->nFiltOfft = 0;` |
|      9 |   98 | `		pDev->bFiltDone = 0;` |
|      9 |   99 | `		pDev->bEof = 0;` |
|      9 |  100 | `		PH7_StreamFilterRewound(pDev);` |
|      9 |  101 | `		pDev->iFiltPos = whence == 0 ? iOfft` |
|      4 |  102 | `			: (pDev->pStream->xTell ? pDev->pStream->xTell(pDev->pHandle) : 0);` |
|      4 |  103 | `	}` |
|      9 |  104 | `	return rc;` |
|      5 |  105 | `}` |
|    166 |  106 | `PH7_PRIVATE io_private * PH7_StreamUnwrap(io_private *pDev)` |
|      1 |  107 | `{` |
|    167 |  108 | `	if( pDev && pDev->pStream && is_php_stream(pDev->pStream) ){` |
|     53 |  109 | `		io_private *pInner = PH7_PhpStreamInner(pDev->pHandle);` |
|     53 |  110 | `		if( pInner ){` |
|     15 |  111 | `			return pInner;` |
|      - |  112 | `		}` |
|     19 |  113 | `	}` |
|    153 |  114 | `	return pDev;` |
|     84 |  115 | `}` |
|    626 |  116 | `static sxu32 StreamAheadBytes(io_private *pDev)` |
|      5 |  117 | `{` |
|    631 |  118 | `	sxu32 n = 0;` |
|    631 |  119 | `	if( SyBlobLength(&pDev->sBuffer) > pDev->nOfft ){` |
|     49 |  120 | `		n += SyBlobLength(&pDev->sBuffer) - pDev->nOfft;` |
|     22 |  121 | `	}` |
|    631 |  122 | `	if( SyBlobLength(&pDev->sFilt) > pDev->nFiltOfft ){` |
|      5 |  123 | `		n += SyBlobLength(&pDev->sFilt) - pDev->nFiltOfft;` |
|      2 |  124 | `	}` |
|    631 |  125 | `	return n;` |
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
|     70 |  147 | `PH7_PRIVATE const char * PH7_VfsResourceType(void *pResource)` |
|      2 |  148 | `{` |
|     72 |  149 | `	io_private *pDev = (io_private *)pResource;` |
|     72 |  150 | `	if( !IO_PRIVATE_INVALID(pDev) ){` |
|      - |  151 | `		/* php names a persistent stream apart, and that name is the only way a` |
|      - |  152 | `		 * script can see that its handle is one. */` |
|     36 |  153 | `		return pDev->bPersist ? "persistent stream" : "stream";` |
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
|     37 |  181 | `}` |
|      - |  182 | `/*` |
|      - |  183 | ` * Return TRUE if the given resource handle is an io_private that has been closed` |
|      - |  184 | ` * (fclose/closedir/pclose) yet kept alive so shared copies still observe it. php` |
|      - |  185 | ` * reports such a value as gettype()=='resource (closed)' and is_resource()==false.` |
|      - |  186 | ` * The magic probe mirrors IO_PRIVATE_INVALID and is safe on any resource handle:` |
|      - |  187 | ` * every resource this engine hands out is a struct larger than an io_private, so` |
|      - |  188 | ` * the iMagic slot is always in bounds and never equals the closed magic by chance.` |
|      - |  189 | ` */` |
|    190 |  190 | `PH7_PRIVATE int PH7_VfsResourceIsClosed(void *pResource)` |
|      5 |  191 | `{` |
|    195 |  192 | `	io_private *pDev = (io_private *)pResource;` |
|    195 |  193 | `	return pDev != 0 && pDev->iMagic == IO_PRIVATE_CLOSED_MAGIC;` |
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
|     58 |  276 | `PH7_PRIVATE int PH7_builtin_fseek(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      3 |  277 | `{` |
|      - |  278 | `	const ph7_io_stream *pStream;` |
|      - |  279 | `	io_private *pDev;` |
|      - |  280 | `	ph7_int64 iOfft;` |
|      - |  281 | `	int whence;` |
|      - |  282 | `	int rc;` |
|     61 |  283 | `	if( nArg < 2 \|\| !ph7_value_is_resource(apArg[0]) ){` |
|      - |  284 | `		/* Missing/Invalid arguments,return FALSE */` |
|    ! 0 |  285 | `		ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"Expecting an IO handle");` |
|    ! 0 |  286 | `		ph7_result_int(pCtx,-1);` |
|    ! 0 |  287 | `		return PH7_OK;` |
|      - |  288 | `	}` |
|      - |  289 | `	/* Extract our private data */` |
|     61 |  290 | `	pDev = (io_private *)ph7_value_to_resource(apArg[0]);` |
|      - |  291 | `	/* Make sure we are dealing with a valid io_private instance */` |
|     61 |  292 | `	if( IO_PRIVATE_INVALID(pDev) ){` |
|      - |  293 | `		/*Expecting an IO handle */` |
|    ! 0 |  294 | `		ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"Expecting an IO handle");` |
|    ! 0 |  295 | `		ph7_result_int(pCtx,-1);` |
|    ! 0 |  296 | `		return PH7_OK;` |
|      - |  297 | `	}` |
|      - |  298 | `	/* Point to the target IO stream device */` |
|     61 |  299 | `	pStream = pDev->pStream;` |
|     61 |  300 | `	if( pStream == 0  \|\| pStream->xSeek == 0){` |
|    ! 0 |  301 | `		ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,` |
|      - |  302 | `			"IO routine(%s) not implemented in the underlying stream(%s) device",` |
|    ! 0 |  303 | `			ph7_function_name(pCtx),pStream ? pStream->zName : "null_stream"` |
|      - |  304 | `			);` |
|    ! 0 |  305 | `		ph7_result_int(pCtx,-1);` |
|    ! 0 |  306 | `		return PH7_OK;` |
|      - |  307 | `	}` |
|      - |  308 | `	/* Extract the offset */` |
|     61 |  309 | `	iOfft = ph7_value_to_int64(apArg[1]);` |
|     61 |  310 | `	whence = 0;/* SEEK_SET */` |
|     61 |  311 | `	if( nArg > 2 ){` |
|      - |  312 | `		/* Read whatever was passed, coercing like php's ZPP: gating this on` |
|      - |  313 | `` 		 * ph7_value_is_int() left `fseek($f, 4, "99")` and `fseek($f, 4, 99.9)` `` |
|      - |  314 | `		 * falling back to whence 0, i.e. the very silent-SEEK_SET the check below` |
|      - |  315 | ``		 * exists to stop (and it disagreed with `99.0`, which is_int accepts). */`` |
|     46 |  316 | `		whence = (int)ph7_value_to_int64(apArg[2]);` |
|     22 |  317 | `	}` |
|     61 |  318 | `	if( whence != 0 /* SEEK_SET */ && whence != 1 /* SEEK_CUR */ && whence != 2 /* SEEK_END */ ){` |
|      - |  319 | `		/* php's php_stream_seek rejects an unknown whence and answers -1 WITHOUT` |
|      - |  320 | `		 * moving the cursor. PHL passed the raw value through to the driver, where` |
|      - |  321 | `		 * every non-CUR/END value fell into the SEEK_SET arm — so fseek($f, 0, 99)` |
|      - |  322 | `		 * reported success (0) AND silently seeked to the start of the stream, two` |
|      - |  323 | `		 * wrong answers from one unchecked argument. php raises no error here. */` |
|     13 |  324 | `		ph7_result_int(pCtx,-1);` |
|     13 |  325 | `		return PH7_OK;` |
|      - |  326 | `	}` |
|     49 |  327 | `	if( pDev->pReadFilters && whence != 2 /* SEEK_END */ ){` |
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
|     43 |  343 | `	if( whence == 1 /* SEEK_CUR */ ){` |
|      - |  344 | `		/* The CURRENT position is the LOGICAL one: the device sits past the` |
|      - |  345 | `		 * read-ahead the line readers buffer, so seek relative to where the` |
|      - |  346 | `		 * SCRIPT is, not where the device is (StreamLogicalAdjust). Without` |
|      - |  347 | `		 * this, fseek($f,2,SEEK_CUR) after an fgets() that buffered ahead` |
|      - |  348 | `		 * skipped everything still sitting in the buffer. */` |
|     10 |  349 | `		iOfft -= (ph7_int64)StreamAheadBytes(pDev);` |
|      4 |  350 | `	}` |
|      - |  351 | `	/* Perform the requested operation */` |
|     43 |  352 | `	rc = pStream->xSeek(pDev->pHandle,iOfft,whence);` |
|     43 |  353 | `	if( rc == PH7_OK ){` |
|      - |  354 | `		/* Ignore buffered data */` |
|     43 |  355 | `		ResetIOPrivate(pDev);` |
|     43 |  356 | `		if( pDev->pReadFilters ){` |
|    ! 0 |  357 | `			pDev->iFiltPos = pStream->xTell ? pStream->xTell(pDev->pHandle) : 0;` |
|    ! 0 |  358 | `		}` |
|     20 |  359 | `	}` |
|      - |  360 | `	/* IO result */` |
|     43 |  361 | `	ph7_result_int(pCtx,rc == PH7_OK ? 0 : - 1);` |
|     43 |  362 | `	return PH7_OK;` |
|     32 |  363 | `}` |
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
|     64 |  375 | `PH7_PRIVATE int PH7_builtin_ftell(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      4 |  376 | `{` |
|      - |  377 | `	const ph7_io_stream *pStream;` |
|      - |  378 | `	io_private *pDev;` |
|      - |  379 | `	ph7_int64 iOfft;` |
|     68 |  380 | `	if( nArg < 1 \|\| !ph7_value_is_resource(apArg[0]) ){` |
|      - |  381 | `		/* Missing/Invalid arguments,return FALSE */` |
|    ! 0 |  382 | `		ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"Expecting an IO handle");` |
|    ! 0 |  383 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 |  384 | `		return PH7_OK;` |
|      - |  385 | `	}` |
|      - |  386 | `	/* Extract our private data */` |
|     68 |  387 | `	pDev = (io_private *)ph7_value_to_resource(apArg[0]);` |
|      - |  388 | `	/* Make sure we are dealing with a valid io_private instance */` |
|     68 |  389 | `	if( IO_PRIVATE_INVALID(pDev) ){` |
|      - |  390 | `		/*Expecting an IO handle */` |
|    ! 0 |  391 | `		ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"Expecting an IO handle");` |
|    ! 0 |  392 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 |  393 | `		return PH7_OK;` |
|      - |  394 | `	}` |
|      - |  395 | `	/* Point to the target IO stream device */` |
|     68 |  396 | `	pStream = pDev->pStream;` |
|     68 |  397 | `	if( pStream == 0  \|\| pStream->xTell == 0){` |
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
|     68 |  409 | `	iOfft = PH7_StreamLogicalTell(pDev);` |
|      - |  410 | `	/* IO result */` |
|     68 |  411 | `	ph7_result_int64(pCtx,iOfft);` |
|     68 |  412 | `	return PH7_OK;` |
|     36 |  413 | `}` |
|      - |  414 | `/*` |
|      - |  415 | ` * bool rewind(resource $handle)` |
|      - |  416 | ` *  Rewind the position of a file pointer.` |
|      - |  417 | ` * Parameters` |
|      - |  418 | ` *  $handle` |
|      - |  419 | ` *   The file pointer.` |
|      - |  420 | ` * Return` |
|      - |  421 | ` *  TRUE on success or FALSE on failure.` |
|      - |  422 | ` */` |
|    306 |  423 | `PH7_PRIVATE int PH7_builtin_rewind(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      4 |  424 | `{` |
|      - |  425 | `	const ph7_io_stream *pStream;` |
|      - |  426 | `	io_private *pDev;` |
|      - |  427 | `	int rc;` |
|    310 |  428 | `	if( nArg < 1 \|\| !ph7_value_is_resource(apArg[0]) ){` |
|      - |  429 | `		/* Missing/Invalid arguments,return FALSE */` |
|    ! 0 |  430 | `		ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"Expecting an IO handle");` |
|    ! 0 |  431 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 |  432 | `		return PH7_OK;` |
|      - |  433 | `	}` |
|      - |  434 | `	/* Extract our private data */` |
|    310 |  435 | `	pDev = (io_private *)ph7_value_to_resource(apArg[0]);` |
|      - |  436 | `	/* Make sure we are dealing with a valid io_private instance */` |
|    310 |  437 | `	if( IO_PRIVATE_INVALID(pDev) ){` |
|      - |  438 | `		/*Expecting an IO handle */` |
|    ! 0 |  439 | `		ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"Expecting an IO handle");` |
|    ! 0 |  440 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 |  441 | `		return PH7_OK;` |
|      - |  442 | `	}` |
|      - |  443 | `	/* Point to the target IO stream device */` |
|    310 |  444 | `	pStream = pDev->pStream;` |
|    310 |  445 | `	if( pStream == 0  \|\| pStream->xSeek == 0){` |
|    ! 0 |  446 | `		ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,` |
|      - |  447 | `			"IO routine(%s) not implemented in the underlying stream(%s) device,PH7 is returning FALSE",` |
|    ! 0 |  448 | `			ph7_function_name(pCtx),pStream ? pStream->zName : "null_stream"` |
|      - |  449 | `			);` |
|    ! 0 |  450 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 |  451 | `		return PH7_OK;` |
|      - |  452 | `	}` |
|      - |  453 | `	/* Perform the requested operation */` |
|    310 |  454 | `	rc = pStream->xSeek(pDev->pHandle,0,0/*SEEK_SET*/);` |
|    310 |  455 | `	if( rc == PH7_OK ){` |
|      - |  456 | `		/* Ignore buffered data */` |
|    310 |  457 | `		ResetIOPrivate(pDev);` |
|    153 |  458 | `	}` |
|      - |  459 | `	/* IO result */` |
|    310 |  460 | `	ph7_result_bool(pCtx,rc == PH7_OK);` |
|    310 |  461 | `	return PH7_OK;` |
|    157 |  462 | `}` |
|      - |  463 | `/*` |
|      - |  464 | ` * bool fflush(resource $handle)` |
|      - |  465 | ` *  Flushes the output to a file.` |
|      - |  466 | ` * Parameters` |
|      - |  467 | ` *  $handle` |
|      - |  468 | ` *   The file pointer.` |
|      - |  469 | ` * Return` |
|      - |  470 | ` *  TRUE on success or FALSE on failure.` |
|      - |  471 | ` */` |
|      4 |  472 | `PH7_PRIVATE int PH7_builtin_fflush(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 |  473 | `{` |
|      - |  474 | `	const ph7_io_stream *pStream;` |
|      - |  475 | `	io_private *pDev;` |
|      - |  476 | `	int rc;` |
|      5 |  477 | `	if( nArg < 1 \|\| !ph7_value_is_resource(apArg[0]) ){` |
|      - |  478 | `		/* Missing/Invalid arguments,return FALSE */` |
|    ! 0 |  479 | `		ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"Expecting an IO handle");` |
|    ! 0 |  480 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 |  481 | `		return PH7_OK;` |
|      - |  482 | `	}` |
|      - |  483 | `	/* Extract our private data */` |
|      5 |  484 | `	pDev = (io_private *)ph7_value_to_resource(apArg[0]);` |
|      - |  485 | `	/* Make sure we are dealing with a valid io_private instance */` |
|      5 |  486 | `	if( IO_PRIVATE_INVALID(pDev) ){` |
|      - |  487 | `		/*Expecting an IO handle */` |
|    ! 0 |  488 | `		ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"Expecting an IO handle");` |
|    ! 0 |  489 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 |  490 | `		return PH7_OK;` |
|      - |  491 | `	}` |
|      - |  492 | `	/* Point to the target IO stream device */` |
|      5 |  493 | `	pDev = PH7_StreamUnwrap(pDev);` |
|      5 |  494 | `	pStream = pDev->pStream;` |
|      5 |  495 | `	if( pStream == 0 \|\| pStream->xSync == 0){` |
|    ! 0 |  496 | `		ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,` |
|      - |  497 | `			"IO routine(%s) not implemented in the underlying stream(%s) device,PH7 is returning FALSE",` |
|    ! 0 |  498 | `			ph7_function_name(pCtx),pStream ? pStream->zName : "null_stream"` |
|      - |  499 | `			);` |
|    ! 0 |  500 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 |  501 | `		return PH7_OK;` |
|      - |  502 | `	}` |
|      - |  503 | `	/* Perform the requested operation */` |
|      5 |  504 | `	rc = pStream->xSync(pDev->pHandle);` |
|      - |  505 | `	/* IO result */` |
|      5 |  506 | `	ph7_result_bool(pCtx,rc == PH7_OK);` |
|      5 |  507 | `	return PH7_OK;` |
|      3 |  508 | `}` |
|      - |  509 | `/*` |
|      - |  510 | ` * php's end-of-file flag is set AFTER THE FACT: a stream is at EOF once one of` |
|      - |  511 | ` * its OWN reads has come back empty, and asking the question never reads. PHL` |
|      - |  512 | ` * used to probe the device instead — a read-ahead of up to 4 KB from inside` |
|      - |  513 | ` * feof() — which answered TRUE on a handle nothing had read yet (an empty file,` |
|      - |  514 | ` * a fresh php://memory), answered TRUE on a WRITE-only handle because the` |
|      - |  515 | `` * refused read looked like an end, and BLOCKED on `feof(STDIN)` with no input`` |
|      - |  516 | ` * waiting: a question about a stream is not a read of it. bEof is that flag,` |
|      - |  517 | ` * set wherever a read here comes back with nothing and cleared by every seek.` |
|      - |  518 | ` */` |
|      - |  519 | `static int IoPrivateUwrapEof(io_private *pDev,int *pAnswer);` |
|  22387 |  520 | `static int IoPrivateAtEof(io_private *pDev)` |
|      5 |  521 | `{` |
|      - |  522 | `	int bEof;` |
|  22392 |  523 | `	if( SyBlobLength(&pDev->sBuffer) > pDev->nOfft ){` |
|      - |  524 | `		/* Buffered bytes are not an end. */` |
|  14443 |  525 | `		return 0;` |
|      - |  526 | `	}` |
|   7954 |  527 | `	if( IoPrivateUwrapEof(pDev,&bEof) ){` |
|      - |  528 | `		/* A userland wrapper answers the question itself — php calls its` |
|      - |  529 | `		 * streamWrapper::stream_eof() rather than inferring anything. */` |
|      8 |  530 | `		return bEof;` |
|      - |  531 | `	}` |
|   7948 |  532 | `	return pDev->bEof != 0;` |
|  11198 |  533 | `}` |
|      - |  534 | `/*` |
|      - |  535 | ` * bool feof(resource $handle)` |
|      - |  536 | ` *  Tests for end-of-file on a file pointer.` |
|      - |  537 | ` * Parameters` |
|      - |  538 | ` *  $handle` |
|      - |  539 | ` *   The file pointer.` |
|      - |  540 | ` * Return` |
|      - |  541 | ` *  Returns TRUE if the file pointer is at EOF.FALSE otherwise` |
|      - |  542 | ` */` |
|  22319 |  543 | `PH7_PRIVATE int PH7_builtin_feof(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      5 |  544 | `{` |
|      - |  545 | `	const ph7_io_stream *pStream;` |
|      - |  546 | `	io_private *pDev;` |
|      - |  547 | `	int rc;` |
|  22324 |  548 | `	if( nArg < 1 \|\| !ph7_value_is_resource(apArg[0]) ){` |
|      - |  549 | `		/* Missing/Invalid arguments */` |
|    ! 0 |  550 | `		ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"Expecting an IO handle");` |
|    ! 0 |  551 | `		ph7_result_bool(pCtx,1);` |
|    ! 0 |  552 | `		return PH7_OK;` |
|      - |  553 | `	}` |
|      - |  554 | `	/* Extract our private data */` |
|  22324 |  555 | `	pDev = (io_private *)ph7_value_to_resource(apArg[0]);` |
|      - |  556 | `	/* Make sure we are dealing with a valid io_private instance */` |
|  22324 |  557 | `	if( IO_PRIVATE_INVALID(pDev) ){` |
|      - |  558 | `		/*Expecting an IO handle */` |
|    ! 0 |  559 | `		ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"Expecting an IO handle");` |
|    ! 0 |  560 | `		ph7_result_bool(pCtx,1);` |
|    ! 0 |  561 | `		return PH7_OK;` |
|      - |  562 | `	}` |
|      - |  563 | `	/* Point to the target IO stream device */` |
|  22324 |  564 | `	pStream = pDev->pStream;` |
|  22324 |  565 | `	if( pStream == 0 ){` |
|    ! 0 |  566 | `		ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,` |
|      - |  567 | `			"IO routine(%s) not implemented in the underlying stream(%s) device,PH7 is returning FALSE",` |
|    ! 0 |  568 | `			ph7_function_name(pCtx),pStream ? pStream->zName : "null_stream"` |
|      - |  569 | `			);` |
|    ! 0 |  570 | `		ph7_result_bool(pCtx,1);` |
|    ! 0 |  571 | `		return PH7_OK;` |
|      - |  572 | `	}` |
|  22324 |  573 | `	rc = IoPrivateAtEof(pDev);` |
|      - |  574 | `	/* EOF or not */` |
|  22324 |  575 | `	ph7_result_bool(pCtx,rc != 0);` |
|  22324 |  576 | `	return PH7_OK;` |
|  11164 |  577 | `}` |
|      - |  578 | `/*` |
|      - |  579 | ` * Read n bytes from the underlying IO stream device.` |
|      - |  580 | ` * Return total numbers of bytes readen on success. A number < 1 on failure` |
|      - |  581 | ` * [i.e: IO error ] or EOF.` |
|      - |  582 | ` *` |
|      - |  583 | ` * This is the read every SCRIPT-level reader goes through, because it drains` |
|      - |  584 | ` * the line readers' read-ahead buffer first: a stream that fgets() has already` |
|      - |  585 | ` * pulled a block out of is positioned where the SCRIPT thinks it is, not where` |
|      - |  586 | ` * the device is. Anything reading from a caller's handle has to use this and` |
|      - |  587 | ` * not the device's own xRead.` |
|      - |  588 | ` */` |
|      - |  589 | `/*` |
|      - |  590 | ` * One read from the device, with the timeout bookkeeping php does for EVERY` |
|      - |  591 | `` * reader: `timed_out` describes the last read, so it is cleared on the way in`` |
|      - |  592 | ` * and set only by a wait that expired. Without the clear, one quiet period marks` |
|      - |  593 | ` * a handle timed out for the rest of its life — and now that every socket` |
|      - |  594 | ` * carries default_socket_timeout, that is every socket that ever waited. And` |
|      - |  595 | ` * without the set being here, only fread() would ever report one: fgets(),` |
|      - |  596 | ` * fgetc(), stream_get_line(), stream_get_contents() and fpassthru() all read` |
|      - |  597 | ` * through their own loops.` |
|      - |  598 | ` */` |
|   9986 |  599 | `static ph7_int64 IoPrivateRawRead(io_private *pDev,void *pBuf,ph7_int64 nLen)` |
|      5 |  600 | `{` |
|      - |  601 | `	ph7_int64 n;` |
|   9991 |  602 | `	pDev->bTimedOut = 0;` |
|   9991 |  603 | `	errno = 0;` |
|   9991 |  604 | `	n = pDev->pStream->xRead(pDev->pHandle,pBuf,nLen);` |
|   9986 |  605 | `	if( n < 0 && (errno == EAGAIN \|\| errno == EWOULDBLOCK)` |
|     16 |  606 | `	 && pDev->bHasTimeout && !pDev->bNonBlock ){` |
|      5 |  607 | `		pDev->bTimedOut = 1;` |
|      2 |  608 | `	}` |
|   9991 |  609 | `	return n;` |
|      5 |  610 | `}` |
|      - |  611 | `/*` |
|      - |  612 | ` * Serve a read from the FILTERED side of a handle. A filter changes the byte` |
|      - |  613 | ` * count — base64 makes four out of three, dechunk throws whole runs away — so` |
|      - |  614 | ` * what the chain produced cannot go straight into the caller's buffer: it waits` |
|      - |  615 | ` * in sFilt and is handed out from there.` |
|      - |  616 | ` *` |
|      - |  617 | ` * The fill loop runs until sFilt holds what was asked for or the device is` |
|      - |  618 | ` * spent, which is what keeps the caller's invariant intact: a SHORT answer here` |
|      - |  619 | ` * still means end of file, exactly as it does for an unfiltered read.` |
|      - |  620 | ` */` |
|    300 |  621 | `static ph7_int64 IoPrivateFilteredRead(io_private *pDev,void *pBuf,ph7_int64 nLen)` |
|      2 |  622 | `{` |
|    302 |  623 | `	phl_stream_filter *pChain = (phl_stream_filter *)pDev->pReadFilters;` |
|      - |  624 | `	sxu32 nAvail;` |
|      - |  625 | `	ph7_int64 n;` |
|      - |  626 |  |
|   2370 |  627 | `	while( pChain != 0 && !pDev->bFiltDone` |
|   2222 |  628 | `	    && (ph7_int64)(SyBlobLength(&pDev->sFilt) - pDev->nFiltOfft) < nLen ){` |
|      - |  629 | `		char zRaw[8192];` |
|   1368 |  630 | `		ph7_int64 nAsk = (ph7_int64)sizeof(zRaw);` |
|      - |  631 | `		ph7_int64 nRaw;` |
|      - |  632 | `		int iStatus;` |
|   1368 |  633 | `		if( pDev->nChunk > 0 && (ph7_int64)pDev->nChunk < nAsk ){` |
|   1099 |  634 | `			nAsk = (ph7_int64)pDev->nChunk;` |
|    549 |  635 | `		}` |
|   1368 |  636 | `		nRaw = IoPrivateRawRead(pDev,zRaw,nAsk);` |
|   1368 |  637 | `		if( nRaw < 0 ){` |
|    ! 0 |  638 | `			if( SyBlobLength(&pDev->sFilt) <= pDev->nFiltOfft ){` |
|      - |  639 | `				/* Nothing was ever produced: the IO error is the answer. */` |
|    ! 0 |  640 | `				return nRaw;` |
|      - |  641 | `			}` |
|    ! 0 |  642 | `			break;` |
|      - |  643 | `		}` |
|      - |  644 | `		{` |
|   1368 |  645 | `			int iF = nRaw > 0 ? PHL_PSFS_FLAG_NORMAL : PHL_PSFS_FLAG_FLUSH_CLOSE;` |
|      - |  646 | `			/* The device's end closes EVERY filter on the stream, not just the` |
|      - |  647 | `			 * head: each one's tail has to travel through the rest. */` |
|   1368 |  648 | `			iStatus = PH7_FilterChainProcess(pChain,zRaw,(sxu32)nRaw,iF,iF,&pDev->sFilt,0);` |
|      - |  649 | `		}` |
|   1368 |  650 | `		if( nRaw == 0 ){` |
|      - |  651 | `			/* The device is spent, and the call above was the chain's CLOSING` |
|      - |  652 | `			 * one: running it again would make a buffering filter emit its tail` |
|      - |  653 | `			 * twice, so the chain is finished for good. */` |
|    140 |  654 | `			pDev->bFiltDone = 1;` |
|    140 |  655 | `			break;` |
|      - |  656 | `		}` |
|   1230 |  657 | `		if( iStatus == PHL_PSFS_ERR_FATAL ){` |
|      - |  658 | `			/* A refusal ends the reading. php reports it to the reader as a` |
|      - |  659 | ``			 * FAILURE — `fread()` answers false, once — and only then as an end`` |
|      - |  660 | `			 * of file; what earlier calls already produced is still the` |
|      - |  661 | `			 * reader's, so the failure waits behind it. */` |
|      5 |  662 | `			pDev->bFiltDone = 1;` |
|      5 |  663 | `			pDev->bFiltErr = 1;` |
|      5 |  664 | `			break;` |
|      - |  665 | `		}` |
|      2 |  666 | `	}` |
|    302 |  667 | `	nAvail = SyBlobLength(&pDev->sFilt) - pDev->nFiltOfft;` |
|    302 |  668 | `	if( nAvail < 1 ){` |
|    140 |  669 | `		SyBlobReset(&pDev->sFilt);` |
|    140 |  670 | `		pDev->nFiltOfft = 0;` |
|    140 |  671 | `		if( pDev->bFiltErr ){` |
|      5 |  672 | `			pDev->bFiltErr = 0;   /* reported once; the read after it is an end */` |
|      5 |  673 | `			return -1;` |
|      - |  674 | `		}` |
|    136 |  675 | `		return pChain != 0 ? 0 : IoPrivateRawRead(pDev,pBuf,nLen);` |
|      - |  676 | `	}` |
|    164 |  677 | `	n = (ph7_int64)nAvail;` |
|    164 |  678 | `	if( n > nLen ){` |
|     25 |  679 | `		n = nLen;` |
|     12 |  680 | `	}` |
|    164 |  681 | `	SyMemcpy(SyBlobDataAt(&pDev->sFilt,pDev->nFiltOfft),pBuf,(sxu32)n);` |
|    164 |  682 | `	pDev->nFiltOfft += (sxu32)n;` |
|    164 |  683 | `	pDev->iFiltPos += n;` |
|    164 |  684 | `	if( pDev->nFiltOfft >= SyBlobLength(&pDev->sFilt) ){` |
|    140 |  685 | `		SyBlobReset(&pDev->sFilt);` |
|    140 |  686 | `		pDev->nFiltOfft = 0;` |
|     69 |  687 | `	}` |
|    164 |  688 | `	return n;` |
|    152 |  689 | `}` |
|   8920 |  690 | `static ph7_int64 IoPrivateDeviceRead(io_private *pDev,void *pBuf,ph7_int64 nLen)` |
|      5 |  691 | `{` |
|   8925 |  692 | `	if( pDev->pReadFilters != 0 \|\| SyBlobLength(&pDev->sFilt) > pDev->nFiltOfft ){` |
|      - |  693 | `		/* Bytes can still be waiting after the last read filter was REMOVED:` |
|      - |  694 | `		 * php flushes a filter on its way out and what it emitted belongs to` |
|      - |  695 | `		 * the reader that comes next. */` |
|    302 |  696 | `		return IoPrivateFilteredRead(pDev,pBuf,nLen);` |
|      - |  697 | `	}` |
|   8625 |  698 | `	return IoPrivateRawRead(pDev,pBuf,nLen);` |
|   4464 |  699 | `}` |
|    788 |  700 | `PH7_PRIVATE ph7_int64 PH7_StreamRead(io_private *pDev,void *pBuf,ph7_int64 nLen)` |
|      5 |  701 | `{` |
|    793 |  702 | `	const ph7_io_stream *pStream = pDev->pStream;` |
|    793 |  703 | `	char *zBuf = (char *)pBuf;` |
|      - |  704 | `	ph7_int64 n,nRead;` |
|    793 |  705 | `	n = SyBlobLength(&pDev->sBuffer) - pDev->nOfft;` |
|    793 |  706 | `	if( n > 0 ){` |
|     12 |  707 | `		if( n > nLen ){` |
|      6 |  708 | `			n = nLen;` |
|      2 |  709 | `		}` |
|      - |  710 | `		/* Copy the buffered data */` |
|     12 |  711 | `		SyMemcpy(SyBlobDataAt(&pDev->sBuffer,pDev->nOfft),pBuf,(sxu32)n);` |
|      - |  712 | `		/* Update the read offset */` |
|     12 |  713 | `		pDev->nOfft += (sxu32)n;` |
|     12 |  714 | `		if( pDev->nOfft >= SyBlobLength(&pDev->sBuffer) ){` |
|      - |  715 | `			/* Reset the working buffer so that we avoid excessive memory allocation */` |
|      7 |  716 | `			SyBlobReset(&pDev->sBuffer);` |
|      7 |  717 | `			pDev->nOfft = 0;` |
|      3 |  718 | `		}` |
|     12 |  719 | `		nLen -= n;` |
|     12 |  720 | `		if( nLen < 1 ){` |
|      - |  721 | `			/* All done */` |
|      6 |  722 | `			return n;` |
|      - |  723 | `		}` |
|      - |  724 | `		/* Advance the cursor */` |
|      7 |  725 | `		zBuf += n;` |
|      3 |  726 | `	}` |
|      - |  727 | `	/* Read without buffering */` |
|    789 |  728 | `	nRead = IoPrivateDeviceRead(pDev,zBuf,nLen);` |
|    784 |  729 | `	if( nRead == 0` |
|    620 |  730 | `	 \|\| (nRead > 0 && nRead < nLen && pStream->xSeek != 0 && pStream->xTell != 0` |
|    310 |  731 | `	     && pStream->xTell(pDev->pHandle) >= 0) ){` |
|      - |  732 | `		/* A read that came back with nothing IS php's end-of-file event, and` |
|      - |  733 | `		 * so is a SHORT one on a device that can say where it IS: php fills` |
|      - |  734 | ``		 * its buffer in a loop, so `fread($f, 100)` on a 12-byte file performs`` |
|      - |  735 | `		 * the second read that finds the end. The position query is what tells` |
|      - |  736 | `		 * a regular file from a FIFO — both arrive here through the same file` |
|      - |  737 | `		 * device, and a short read from a fifo, a pipe or a socket means only` |
|      - |  738 | `		 * that less had arrived, so latching there would end` |
|      - |  739 | ``		 * `while (!feof($p)) $s .= fread($p, 8192);` with data still coming. A`` |
|      - |  740 | `		 * NEGATIVE answer is an IO error and never latches. */` |
|    637 |  741 | `		pDev->bEof = 1;` |
|    316 |  742 | `	}` |
|    789 |  743 | `	if( nRead > 0 ){` |
|    435 |  744 | `		n += nRead;` |
|    573 |  745 | `	}else if( n < 1 ){` |
|      - |  746 | `		/* EOF or IO error */` |
|    355 |  747 | `		return nRead;` |
|      - |  748 | `	}` |
|    439 |  749 | `	return n;` |
|    398 |  750 | `}` |
|      - |  751 | `/*` |
|      - |  752 | ` * Every SCRIPT-level write goes through here, because a handle can carry a` |
|      - |  753 | ` * WRITE chain: php runs what the script wrote through the filters before the` |
|      - |  754 | ` * device sees any of it, and a filter changes the byte count — so what reaches` |
|      - |  755 | ` * the device is not what was handed in, while what fwrite() ANSWERS still is` |
|      - |  756 | ` * (php reports the bytes it CONSUMED, not the bytes it emitted).` |
|      - |  757 | ` */` |
|    476 |  758 | `PH7_PRIVATE ph7_int64 PH7_StreamWrite(io_private *pDev,const void *pData,ph7_int64 nLen)` |
|      5 |  759 | `{` |
|    481 |  760 | `	phl_stream_filter *pChain = (phl_stream_filter *)pDev->pWriteFilters;` |
|      - |  761 | `	SyBlob sOut;` |
|      - |  762 | `	ph7_int64 nWr;` |
|      - |  763 | `	int iStatus;` |
|    481 |  764 | `	if( pDev->pStream == 0 \|\| pDev->pStream->xWrite == 0 ){` |
|    ! 0 |  765 | `		return -1;` |
|      - |  766 | `	}` |
|    481 |  767 | `	if( pChain == 0 ){` |
|    451 |  768 | `		return pDev->pStream->xWrite(pDev->pHandle,pData,nLen);` |
|      - |  769 | `	}` |
|     32 |  770 | `	SyBlobInit(&sOut,pDev->sBuffer.pAllocator);` |
|     32 |  771 | `	iStatus = PH7_FilterChainProcess(pChain,pData,(sxu32)nLen,` |
|      - |  772 | `		PHL_PSFS_FLAG_NORMAL,PHL_PSFS_FLAG_NORMAL,&sOut,0);` |
|     32 |  773 | `	if( iStatus == PHL_PSFS_ERR_FATAL ){` |
|      5 |  774 | `		SyBlobRelease(&sOut);` |
|      5 |  775 | `		return -1;` |
|      - |  776 | `	}` |
|     28 |  777 | `	nWr = 0;` |
|     28 |  778 | `	if( SyBlobLength(&sOut) > 0 ){` |
|     41 |  779 | `		nWr = pDev->pStream->xWrite(pDev->pHandle,SyBlobData(&sOut),` |
|     26 |  780 | `			(ph7_int64)SyBlobLength(&sOut));` |
|     13 |  781 | `	}` |
|     28 |  782 | `	SyBlobRelease(&sOut);` |
|     28 |  783 | `	if( nWr < 0 ){` |
|    ! 0 |  784 | `		return -1;` |
|      - |  785 | `	}` |
|      - |  786 | `	/* A filter that held its input back (FEED_ME) still consumed it: php's` |
|      - |  787 | `	 * fwrite() answers the length it was given. */` |
|     28 |  788 | `	return nLen;` |
|    241 |  789 | `}` |
|      - |  790 | `/*` |
|      - |  791 | ` * Extract a single line from the buffered input.` |
|      - |  792 | ` */` |
|  17494 |  793 | `static sxi32 GetLine(io_private *pDev,ph7_int64 *pLen,const char **pzLine)` |
|      5 |  794 | `{` |
|      - |  795 | `	const char *zIn,*zEnd,*zPtr;` |
|  17499 |  796 | `	zIn = (const char *)SyBlobDataAt(&pDev->sBuffer,pDev->nOfft);` |
|  17499 |  797 | `	zEnd = &zIn[SyBlobLength(&pDev->sBuffer)-pDev->nOfft];` |
|  17499 |  798 | `	zPtr = zIn;` |
| 805071 |  799 | `	while( zIn < zEnd ){` |
| 804803 |  800 | `		if( zIn[0] == '\n' ){` |
|      - |  801 | `			/* Line found */` |
|  17231 |  802 | `			zIn++; /* Include the line ending as requested by the PHP specification */` |
|  17231 |  803 | `			*pLen = (ph7_int64)(zIn-zPtr);` |
|  17231 |  804 | `			*pzLine = zPtr;` |
|  17231 |  805 | `			return SXRET_OK;` |
|      - |  806 | `		}` |
| 787577 |  807 | `		zIn++;` |
|      5 |  808 | `	}` |
|      - |  809 | `	/* No line were found */` |
|    273 |  810 | `	return SXERR_NOTFOUND;` |
|   8752 |  811 | `}` |
|      - |  812 | `/*` |
|      - |  813 | ` * Read a single line from the underlying IO stream device.` |
|      - |  814 | ` */` |
|  22426 |  815 | `static ph7_int64 StreamReadLine(io_private *pDev,const char **pzData,ph7_int64 nMaxLen)` |
|      5 |  816 | `{` |
|      - |  817 | `	char zBuf[8192];` |
|      - |  818 | `	ph7_int64 n;` |
|      - |  819 | `	sxi32 rc;` |
|  22431 |  820 | `	n = 0;` |
|  22431 |  821 | `	if( pDev->nOfft >= SyBlobLength(&pDev->sBuffer) ){` |
|      - |  822 | `		/* Reset the working buffer so that we avoid excessive memory allocation */` |
|   7867 |  823 | `		SyBlobReset(&pDev->sBuffer);` |
|   7867 |  824 | `		pDev->nOfft = 0;` |
|   3931 |  825 | `	}` |
|  22431 |  826 | `	if( SyBlobLength(&pDev->sBuffer) > pDev->nOfft ){` |
|      - |  827 | `		/* Check if there is a line */` |
|  14569 |  828 | `		rc = GetLine(pDev,&n,pzData);` |
|  14569 |  829 | `		if( rc == SXRET_OK ){` |
|      - |  830 | `			/* Got line. php caps it at nMaxLen bytes even when the newline` |
|      - |  831 | `			 * falls later; the remainder (incl. the newline) stays buffered. */` |
|  14495 |  832 | `			if( nMaxLen > 0 && n > nMaxLen ){` |
|    ! 0 |  833 | `				n = nMaxLen;` |
|    ! 0 |  834 | `			}` |
|  14495 |  835 | `			pDev->nOfft += (sxu32)n;` |
|  14495 |  836 | `			return n;` |
|      - |  837 | `		}` |
|     37 |  838 | `	}` |
|      - |  839 | `	/* Perform the read operation until a new line is extracted or length` |
|      - |  840 | `	 * limit is reached.` |
|      - |  841 | `	 */` |
|   4047 |  842 | `	for(;;){` |
|     79 |  843 | `		{` |
|      - |  844 | `			/* php fills its read buffer one CHUNK at a time, and` |
|      - |  845 | `			 * stream_set_chunk_size() is how a script asks for a smaller one. */` |
|   8099 |  846 | `			ph7_int64 nAsk = (ph7_int64)sizeof(zBuf);` |
|   8099 |  847 | `			if( pDev->nChunk > 0 && (ph7_int64)pDev->nChunk < nAsk ){` |
|     30 |  848 | `				nAsk = (ph7_int64)pDev->nChunk;` |
|     15 |  849 | `			}` |
|   8099 |  850 | `			if( nMaxLen > 0 && nMaxLen < nAsk ){` |
|     69 |  851 | `				nAsk = nMaxLen;` |
|     34 |  852 | `			}` |
|   8099 |  853 | `			n = IoPrivateDeviceRead(pDev,zBuf,nAsk);` |
|      - |  854 | `		}` |
|   8099 |  855 | `		if( n == 0 ){` |
|   5167 |  856 | `			pDev->bEof = 1;` |
|   2581 |  857 | `		}` |
|   8099 |  858 | `		if( n < 1 ){` |
|      - |  859 | `			/* EOF or IO error */` |
|   5169 |  860 | `			break;` |
|      - |  861 | `		}` |
|      - |  862 | `		/* Append the data just read */` |
|   2935 |  863 | `		SyBlobAppend(&pDev->sBuffer,zBuf,(sxu32)n);` |
|      - |  864 | `		/* Try to extract a line */` |
|   2935 |  865 | `		rc = GetLine(pDev,&n,pzData);` |
|   2935 |  866 | `		if( rc == SXRET_OK ){` |
|      - |  867 | `			/* Got one. Cap at nMaxLen (php's length limit); anything past the` |
|      - |  868 | `			 * cap, newline included, is left buffered for the next read. */` |
|   2741 |  869 | `			if( nMaxLen > 0 && n > nMaxLen ){` |
|      7 |  870 | `				n = nMaxLen;` |
|      3 |  871 | `			}` |
|   2741 |  872 | `			pDev->nOfft += (sxu32)n;` |
|   2741 |  873 | `			return n;` |
|      - |  874 | `		}` |
|    199 |  875 | `		if( nMaxLen > 0 && (SyBlobLength(&pDev->sBuffer) - pDev->nOfft >= nMaxLen) ){` |
|      - |  876 | `			/* Cap reached before a newline: return EXACTLY nMaxLen bytes (a prior` |
|      - |  877 | `			 * sub-cap leftover plus this read can hold more) and keep the` |
|      - |  878 | `			 * remainder buffered via nOfft for the next read, so we never hand` |
|      - |  879 | `			 * back more than nMaxLen. The top-of-function check reclaims the` |
|      - |  880 | `			 * buffer once it is fully consumed. */` |
|     37 |  881 | `			*pzData = (const char *)SyBlobDataAt(&pDev->sBuffer,pDev->nOfft);` |
|     37 |  882 | `			n = nMaxLen;` |
|     37 |  883 | `			pDev->nOfft += (sxu32)nMaxLen;` |
|     37 |  884 | `			return n;` |
|      - |  885 | `		}` |
|      5 |  886 | `	}` |
|   5169 |  887 | `	if( SyBlobLength(&pDev->sBuffer) > pDev->nOfft ){` |
|      - |  888 | `		/* Read limit reached,return the available data */` |
|    199 |  889 | `		*pzData = (const char *)SyBlobDataAt(&pDev->sBuffer,pDev->nOfft);` |
|    199 |  890 | `		n = SyBlobLength(&pDev->sBuffer) - pDev->nOfft;` |
|      - |  891 | `		/* Reset the working buffer */` |
|    199 |  892 | `		SyBlobReset(&pDev->sBuffer);` |
|    199 |  893 | `		pDev->nOfft = 0;` |
|     97 |  894 | `	}` |
|   5169 |  895 | `	return n;` |
|  11218 |  896 | `}` |
|      - |  897 | `/*` |
|      - |  898 | ` * Open an IO stream handle.` |
|      - |  899 | ` * Notes on stream:` |
|      - |  900 | ` * According to the PHP reference manual.` |
|      - |  901 | ` * In its simplest definition, a stream is a resource object which exhibits streamable behavior.` |
|      - |  902 | ` * That is, it can be read from or written to in a linear fashion, and may be able to fseek()` |
|      - |  903 | ` * to an arbitrary locations within the stream.` |
|      - |  904 | ` * A wrapper is additional code which tells the stream how to handle specific protocols/encodings.` |
|      - |  905 | ` * For example, the http wrapper knows how to translate a URL into an HTTP/1.0 request for a file` |
|      - |  906 | ` * on a remote server.` |
|      - |  907 | ` * A stream is referenced as: scheme://target` |
|      - |  908 | ` *   scheme(string) - The name of the wrapper to be used. Examples include: file, http...` |
|      - |  909 | ` *   If no wrapper is specified, the function default is used (typically file://).` |
|      - |  910 | ` *   target - Depends on the wrapper used. For filesystem related streams this is typically a path` |
|      - |  911 | ` *  and filename of the desired file. For network related streams this is typically a hostname, often` |
|      - |  912 | ` *  with a path appended.` |
|      - |  913 | ` *` |
|      - |  914 | ` * Note that PH7 IO streams looks like PHP streams but their implementation differ greately.` |
|      - |  915 | ` * Please refer to the official documentation for a full discussion.` |
|      - |  916 | ` * This function return a handle on success. Otherwise null.` |
|      - |  917 | ` */` |
|  35324 |  918 | `PH7_PRIVATE void * PH7_StreamOpenHandle(ph7_vm *pVm,const ph7_io_stream *pStream,const char *zFile,` |
|      - |  919 | `	int iFlags,int use_include,ph7_value *pResource,int bPushInclude,int *pNew,const char *zCaller)` |
|      5 |  920 | `{` |
|  35329 |  921 | `	void *pHandle = 0; /* cc warning */` |
|      - |  922 | `	SyString sFile;` |
|      - |  923 | `	ph7_value sDummy;` |
|      - |  924 | `	int rc;` |
|  35329 |  925 | `	if( pStream == 0 ){` |
|      - |  926 | `		/* No such stream device. The armed context describes THIS open and` |
|      - |  927 | `		 * nothing else, so it is dropped on every exit — a caller that armed one` |
|      - |  928 | `		 * and returned early must not leave it for the next open to pick up. */` |
|    ! 0 |  929 | `		pVm->pOpenCtx = 0;` |
|    ! 0 |  930 | `		return 0;` |
|      - |  931 | `	}` |
|      - |  932 | `	/* A wrapper registered with STREAM_IS_URL speaks to the network, and php lets` |
|      - |  933 | `	 * the configuration turn that off: allow_url_fopen for an ordinary open,` |
|      - |  934 | `	 * allow_url_include for the one that EXECUTES what comes back — which is off` |
|      - |  935 | `	 * by default, because including a remote file is the classic RFI. */` |
|  35329 |  936 | `	if( PH7_StreamIsUrlWrapper(pStream) ){` |
|      - |  937 | `		/* php tests BOTH, in this order: a URL wrapper is unusable at all without` |
|      - |  938 | `		 * allow_url_fopen, and an INCLUDE needs allow_url_include on top of it. */` |
|     47 |  939 | `		const char *zIni = 0;` |
|     47 |  940 | `		if( !PH7_VmIniGetBool(pVm,"allow_url_fopen",1) ){` |
|      7 |  941 | `			zIni = "allow_url_fopen";` |
|     43 |  942 | `		}else if( bPushInclude && !PH7_VmIniGetBool(pVm,"allow_url_include",0) ){` |
|      5 |  943 | `			zIni = "allow_url_include";` |
|      2 |  944 | `		}` |
|     47 |  945 | `		if( zIni ){` |
|      - |  946 | `			SyString sCaller;` |
|      - |  947 | `			char zMsg[160];` |
|     12 |  948 | `			pVm->pOpenCtx = 0;` |
|     12 |  949 | `			SyStringInitFromBuf(&sCaller,zCaller ? zCaller : "",zCaller ? SyStrlen(zCaller) : 0);` |
|     17 |  950 | `			SyBufferFormat(zMsg,sizeof(zMsg),` |
|      - |  951 | `				"%s:// wrapper is disabled in the server configuration by %s=0",` |
|     10 |  952 | `				pStream->zName,zIni);` |
|     12 |  953 | `			PH7_VmThrowError(pVm,zCaller ? &sCaller : 0,PH7_CTX_WARNING,zMsg);` |
|     12 |  954 | `			return 0;` |
|      - |  955 | `		}` |
|     17 |  956 | `	}` |
|  35319 |  957 | `	if( pResource == 0 ){` |
|      - |  958 | `		/* VM-dependent devices (php://, data://, tcp://, userland wrappers)` |
|      - |  959 | `		 * reach the VM only through pResource->pVm — their xOpen has no vm` |
|      - |  960 | `		 * parameter. Callers like file_get_contents pass no resource, so hand` |
|      - |  961 | `		 * every device a synthesized stack value carrying the VM; xOpen only` |
|      - |  962 | `		 * reads it during the call, and file:// ignores it. */` |
|  34871 |  963 | `		PH7_MemObjInit(pVm,&sDummy);` |
|  34871 |  964 | `		pResource = &sDummy;` |
|  17433 |  965 | `	}` |
|  35319 |  966 | `	SyStringInitFromBuf(&sFile,zFile,SyStrlen(zFile));` |
|  35319 |  967 | `	if( use_include ){` |
|   5095 |  968 | `		if(	/* include_path names DIRECTORIES, so it has nothing to say about a` |
|      - |  969 | `` 			 * URL: walking it for a `php://filter/…` one built `<dir>/filter/…` `` |
|      - |  970 | `			 * and reported the whole open as an IO error. The direct arm is the` |
|      - |  971 | `			 * one that also marks the file as included, which is what` |
|      - |  972 | `			 * include_once needs. */` |
|  10190 |  973 | `			pStream != pVm->pDefStream \|\|` |
|  10178 |  974 | `			sFile.zString[0] == '/' \|\|` |
|      - |  975 | `#ifdef __WINNT__` |
|      - |  976 | `			(sFile.nByte > 2 && sFile.zString[1] == ':' && (sFile.zString[2] == '\\' \|\| sFile.zString[2] == '/') ) \|\|` |
|      - |  977 | `#endif` |
|  10085 |  978 | `			(sFile.nByte > 1 && sFile.zString[0] == '.' && sFile.zString[1] == '/') \|\|` |
|  10078 |  979 | `			(sFile.nByte > 2 && sFile.zString[0] == '.' && sFile.zString[1] == '.' && sFile.zString[2] == '/') ){` |
|      - |  980 | `				/*  Open the file directly */` |
|    117 |  981 | `				rc = pStream->xOpen(zFile,iFlags,pResource,&pHandle);` |
|    117 |  982 | `				if( rc == PH7_OK && bPushInclude ){` |
|      - |  983 | `					/* Mark as included */` |
|    115 |  984 | `					PH7_VmPushFilePath(pVm,sFile.zString,sFile.nByte,FALSE,pNew);` |
|     55 |  985 | `				}` |
|     61 |  986 | `		}else{` |
|      - |  987 | `			SyString *pPath;` |
|      - |  988 | `			SyBlob sWorker;` |
|      - |  989 | `#ifdef __WINNT__` |
|      - |  990 | `			static const int c = '\\';` |
|      - |  991 | `#else` |
|      - |  992 | `			static const int c = '/';` |
|      - |  993 | `#endif` |
|      - |  994 | `			/* Init the path builder working buffer */` |
|  10083 |  995 | `			SyBlobInit(&sWorker,&pVm->sAllocator);` |
|      - |  996 | `			/* Build a path from the set of include path */` |
|  10083 |  997 | `			SySetResetCursor(&pVm->aPaths);` |
|  10083 |  998 | `			rc = SXERR_IO;` |
|  10107 |  999 | `			while( SXRET_OK == SySetGetNextEntry(&pVm->aPaths,(void **)&pPath) ){` |
|      - | 1000 | `				/* Build full path */` |
|  10091 | 1001 | `				SyBlobFormat(&sWorker,"%z%c%z",pPath,c,&sFile);` |
|      - | 1002 | `				/* Append null terminator */` |
|  10091 | 1003 | `				if( SXRET_OK != SyBlobNullAppend(&sWorker) ){` |
|    ! 0 | 1004 | `					continue;` |
|      - | 1005 | `				}` |
|      - | 1006 | `				/* Try to open the file */` |
|  10091 | 1007 | `				rc = pStream->xOpen((const char *)SyBlobData(&sWorker),iFlags,pResource,&pHandle);` |
|  10091 | 1008 | `				if( rc == PH7_OK ){` |
|  10066 | 1009 | `					if( bPushInclude ){` |
|      - | 1010 | `						/* Mark as included */` |
|  10066 | 1011 | `						PH7_VmPushFilePath(pVm,(const char *)SyBlobData(&sWorker),SyBlobLength(&sWorker),FALSE,pNew);` |
|   5031 | 1012 | `					}` |
|  10066 | 1013 | `					break;` |
|      - | 1014 | `				}` |
|      - | 1015 | `				/* Reset the working buffer */` |
|     27 | 1016 | `				SyBlobReset(&sWorker);` |
|      - | 1017 | `				/* Check the next path */` |
|      3 | 1018 | `			}` |
|  10083 | 1019 | `			if( rc != PH7_OK ){` |
|      - | 1020 | `				/* php's LAST RESORT, and the one PHL never had: the directory of` |
|      - | 1021 | ``				 * the file that is EXECUTING. `include 'lib.php'` next to the`` |
|      - | 1022 | `				 * script has to work whatever directory the script was started` |
|      - | 1023 | `				 * from -- see PH7_VmExecutingDir(). Tried after the include_path` |
|      - | 1024 | `				 * entries, as php tries it. */` |
|      - | 1025 | `				SyString sDir;` |
|     19 | 1026 | `				if( PH7_VmExecutingDir(pVm,&sDir) ){` |
|     19 | 1027 | `					SyBlobReset(&sWorker);` |
|     19 | 1028 | `					SyBlobFormat(&sWorker,"%z%c%z",&sDir,c,&sFile);` |
|     19 | 1029 | `					if( SXRET_OK == SyBlobNullAppend(&sWorker) ){` |
|     19 | 1030 | `						rc = pStream->xOpen((const char *)SyBlobData(&sWorker),iFlags,pResource,&pHandle);` |
|     19 | 1031 | `						if( rc == PH7_OK && bPushInclude ){` |
|      4 | 1032 | `							PH7_VmPushFilePath(pVm,(const char *)SyBlobData(&sWorker),` |
|      2 | 1033 | `								SyBlobLength(&sWorker),FALSE,pNew);` |
|      1 | 1034 | `						}` |
|      8 | 1035 | `					}` |
|      8 | 1036 | `				}` |
|      8 | 1037 | `			}` |
|  10083 | 1038 | `			SyBlobRelease(&sWorker);` |
|      - | 1039 | `		}` |
|   5100 | 1040 | `	}else{` |
|      - | 1041 | `		/* Open the URI direcly */` |
|  25129 | 1042 | `		rc = pStream->xOpen(zFile,iFlags,pResource,&pHandle);` |
|      - | 1043 | `	}` |
|      - | 1044 | `	/* The armed context describes exactly ONE open — every attempt of the` |
|      - | 1045 | `	 * include-path walk above included — so it is dropped here whether the open` |
|      - | 1046 | `	 * worked or not. A device that wanted it (a userland wrapper) read it while` |
|      - | 1047 | `	 * its xOpen was running. */` |
|  35319 | 1048 | `	pVm->pOpenCtx = 0;` |
|  35319 | 1049 | `	if( rc != PH7_OK ){` |
|      - | 1050 | `		/* IO error */` |
|     63 | 1051 | `		return 0;` |
|      - | 1052 | `	}` |
|      - | 1053 | `	/* Return the file handle */` |
|  35261 | 1054 | `	return pHandle;` |
|  17667 | 1055 | `}` |
|      - | 1056 | `/*` |
|      - | 1057 | ` * Read the whole contents of an open IO stream handle [i.e local file/URL..]` |
|      - | 1058 | ` * Store the read data in the given BLOB (last argument).` |
|      - | 1059 | ` * The read operation is stopped when he hit the EOF or an IO error occurs.` |
|      - | 1060 | ` */` |
|  10156 | 1061 | `PH7_PRIVATE sxi32 PH7_StreamReadWholeFile(void *pHandle,const ph7_io_stream *pStream,SyBlob *pOut)` |
|      5 | 1062 | `{` |
|      - | 1063 | `	ph7_int64 nRead;` |
|      - | 1064 | `	char zBuf[8192]; /* 8K */` |
|      - | 1065 | `	int rc;` |
|      - | 1066 | `	/* Perform the requested operation */` |
|  10157 | 1067 | `	for(;;){` |
|  20319 | 1068 | `		nRead = pStream->xRead(pHandle,zBuf,sizeof(zBuf));` |
|  20319 | 1069 | `		if( nRead < 1 ){` |
|      - | 1070 | `			/* EOF or IO error */` |
|  10161 | 1071 | `			break;` |
|      - | 1072 | `		}` |
|      - | 1073 | `		/* Append contents */` |
|  10163 | 1074 | `		rc = SyBlobAppend(pOut,zBuf,(sxu32)nRead);` |
|  10163 | 1075 | `		if( rc != SXRET_OK ){` |
|    ! 0 | 1076 | `			break;` |
|      - | 1077 | `		}` |
|      5 | 1078 | `	}` |
|  10161 | 1079 | `	return SyBlobLength(pOut) > 0 ? SXRET_OK : -1;` |
|      5 | 1080 | `}` |
|      - | 1081 | `/*` |
|      - | 1082 | ` * Close an open IO stream handle [i.e local file/URI..].` |
|      - | 1083 | ` */` |
|  35410 | 1084 | `PH7_PRIVATE void PH7_StreamCloseHandle(const ph7_io_stream *pStream,void *pHandle)` |
|      5 | 1085 | `{` |
|  35415 | 1086 | `	if( pStream->xClose ){` |
|  35415 | 1087 | `		pStream->xClose(pHandle);` |
|  17705 | 1088 | `	}` |
|  35415 | 1089 | `}` |
|      - | 1090 | `/*` |
|      - | 1091 | ` * string fgetc(resource $handle)` |
|      - | 1092 | ` *  Gets a character from the given file pointer.` |
|      - | 1093 | ` * Parameters` |
|      - | 1094 | ` *  $handle` |
|      - | 1095 | ` *   The file pointer.` |
|      - | 1096 | ` * Return` |
|      - | 1097 | ` *  Returns a string containing a single character read from the file` |
|      - | 1098 | ` *  pointed to by handle. Returns FALSE on EOF.` |
|      - | 1099 | ` * WARNING` |
|      - | 1100 | ` *  This operation is extremely slow.Avoid using it.` |
|      - | 1101 | ` */` |
|      4 | 1102 | `PH7_PRIVATE int PH7_builtin_fgetc(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 1103 | `{` |
|      - | 1104 | `	const ph7_io_stream *pStream;` |
|      - | 1105 | `	io_private *pDev;` |
|      - | 1106 | `	int c,n;` |
|      5 | 1107 | `	if( nArg < 1 \|\| !ph7_value_is_resource(apArg[0]) ){` |
|      - | 1108 | `		/* Missing/Invalid arguments,return FALSE */` |
|    ! 0 | 1109 | `		ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"Expecting an IO handle");` |
|    ! 0 | 1110 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 1111 | `		return PH7_OK;` |
|      - | 1112 | `	}` |
|      - | 1113 | `	/* Extract our private data */` |
|      5 | 1114 | `	pDev = (io_private *)ph7_value_to_resource(apArg[0]);` |
|      - | 1115 | `	/* Make sure we are dealing with a valid io_private instance */` |
|      5 | 1116 | `	if( IO_PRIVATE_INVALID(pDev) ){` |
|      - | 1117 | `		/*Expecting an IO handle */` |
|    ! 0 | 1118 | `		ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"Expecting an IO handle");` |
|    ! 0 | 1119 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 1120 | `		return PH7_OK;` |
|      - | 1121 | `	}` |
|      - | 1122 | `	/* Point to the target IO stream device */` |
|      5 | 1123 | `	pStream = pDev->pStream;` |
|      5 | 1124 | `	if( pStream == 0  ){` |
|    ! 0 | 1125 | `		ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,` |
|      - | 1126 | `			"IO routine(%s) not implemented in the underlying stream(%s) device,PH7 is returning FALSE",` |
|    ! 0 | 1127 | `			ph7_function_name(pCtx),pStream ? pStream->zName : "null_stream"` |
|      - | 1128 | `			);` |
|    ! 0 | 1129 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 1130 | `		return PH7_OK;` |
|      - | 1131 | `	}` |
|      - | 1132 | `	/* Perform the requested operation */` |
|      5 | 1133 | `	n = (int)PH7_StreamRead(pDev,(void *)&c,sizeof(char));` |
|      - | 1134 | `	/* IO result */` |
|      5 | 1135 | `	if( n < 1 ){` |
|      - | 1136 | `		/* EOF or error,return FALSE */` |
|    ! 0 | 1137 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 1138 | `	}else{` |
|      - | 1139 | `		/* Return the string holding the character */` |
|      5 | 1140 | `		ph7_result_string(pCtx,(const char *)&c,sizeof(char));` |
|      - | 1141 | `	}` |
|      5 | 1142 | `	return PH7_OK;` |
|      3 | 1143 | `}` |
|      - | 1144 | `/*` |
|      - | 1145 | ` * string fgets(resource $handle[,int64 $length ])` |
|      - | 1146 | ` *  Gets line from file pointer.` |
|      - | 1147 | ` * Parameters` |
|      - | 1148 | ` *  $handle` |
|      - | 1149 | ` *   The file pointer.` |
|      - | 1150 | ` * $length` |
|      - | 1151 | ` *  Reading ends when length - 1 bytes have been read, on a newline` |
|      - | 1152 | ` *  (which is included in the return value), or on EOF (whichever comes first).` |
|      - | 1153 | ` *  If no length is specified, it will keep reading from the stream until it reaches` |
|      - | 1154 | ` *  the end of the line.` |
|      - | 1155 | ` * Return` |
|      - | 1156 | ` *  Returns a string of up to length - 1 bytes read from the file pointed to by handle.` |
|      - | 1157 | ` *  If there is no more data to read in the file pointer, then FALSE is returned.` |
|      - | 1158 | ` *  If an error occurs, FALSE is returned.` |
|      - | 1159 | ` */` |
|  22236 | 1160 | `PH7_PRIVATE int PH7_builtin_fgets(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      5 | 1161 | `{` |
|      - | 1162 | `	const ph7_io_stream *pStream;` |
|      - | 1163 | `	const char *zLine;` |
|      - | 1164 | `	io_private *pDev;` |
|      - | 1165 | `	ph7_int64 n,nLen;` |
|  22241 | 1166 | `	if( nArg < 1 \|\| !ph7_value_is_resource(apArg[0]) ){` |
|      - | 1167 | `		/* Missing/Invalid arguments,return FALSE */` |
|    ! 0 | 1168 | `		ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"Expecting an IO handle");` |
|    ! 0 | 1169 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 1170 | `		return PH7_OK;` |
|      - | 1171 | `	}` |
|      - | 1172 | `	/* Extract our private data */` |
|  22241 | 1173 | `	pDev = (io_private *)ph7_value_to_resource(apArg[0]);` |
|      - | 1174 | `	/* Make sure we are dealing with a valid io_private instance */` |
|  22241 | 1175 | `	if( IO_PRIVATE_INVALID(pDev) ){` |
|      - | 1176 | `		/*Expecting an IO handle */` |
|    ! 0 | 1177 | `		ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"Expecting an IO handle");` |
|    ! 0 | 1178 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 1179 | `		return PH7_OK;` |
|      - | 1180 | `	}` |
|      - | 1181 | `	/* Point to the target IO stream device */` |
|  22241 | 1182 | `	pStream = pDev->pStream;` |
|  22241 | 1183 | `	if( pStream == 0  ){` |
|    ! 0 | 1184 | `		ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,` |
|      - | 1185 | `			"IO routine(%s) not implemented in the underlying stream(%s) device,PH7 is returning FALSE",` |
|    ! 0 | 1186 | `			ph7_function_name(pCtx),pStream ? pStream->zName : "null_stream"` |
|      - | 1187 | `			);` |
|    ! 0 | 1188 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 1189 | `		return PH7_OK;` |
|      - | 1190 | `	}` |
|  22241 | 1191 | `	nLen = -1;` |
|  22241 | 1192 | `	if( nArg > 1 && !ph7_value_is_null(apArg[1]) ){` |
|      - | 1193 | `		/* Maximum data to read. PHP 8 raises a catchable ValueError for a` |
|      - | 1194 | `		 * non-positive length; a NULL (the ?int default) reads the whole line. */` |
|     61 | 1195 | `		nLen = ph7_value_to_int64(apArg[1]);` |
|     61 | 1196 | `		if( nLen < 1 ){` |
|      5 | 1197 | `			return PH7_VmThrowException(pCtx,"ValueError",` |
|      - | 1198 | `				"fgets(): Argument #2 ($length) must be greater than 0");` |
|      - | 1199 | `		}` |
|      - | 1200 | `		/* php reads at most length-1 bytes -- one byte is reserved for the` |
|      - | 1201 | `		 * string terminator -- so a length of 1 reads nothing and returns` |
|      - | 1202 | `		 * false at any position, exactly like EOF. */` |
|     57 | 1203 | `		nLen -= 1;` |
|     57 | 1204 | `		if( nLen == 0 ){` |
|      3 | 1205 | `			ph7_result_bool(pCtx,0);` |
|      3 | 1206 | `			return PH7_OK;` |
|      - | 1207 | `		}` |
|     27 | 1208 | `	}` |
|      - | 1209 | `	/* Perform the requested operation */` |
|  22235 | 1210 | `	n = StreamReadLine(pDev,&zLine,nLen);` |
|  22235 | 1211 | `	if( n < 1 ){` |
|      - | 1212 | `		/* EOF or IO error,return FALSE */` |
|   4933 | 1213 | `		ph7_result_bool(pCtx,0);` |
|   2469 | 1214 | `	}else{` |
|      - | 1215 | `		/* Return the freshly extracted line */` |
|  17307 | 1216 | `		ph7_result_string(pCtx,zLine,(int)n);` |
|      - | 1217 | `	}` |
|  22235 | 1218 | `	return PH7_OK;` |
|  11123 | 1219 | `}` |
|      - | 1220 | `/*` |
|      - | 1221 | ` * string\|false stream_get_line(resource $stream, int $length, string $ending = "")` |
|      - | 1222 | ` *  Read a line from a stream, up to $length bytes or the FIRST occurrence of` |
|      - | 1223 | ` *  $ending, whichever comes first. Unlike fgets(), the ending is CONSUMED but` |
|      - | 1224 | ` *  never returned, and it may be any string.` |
|      - | 1225 | ` *  php's window rule (php_stream_get_record), pinned by probe: the ending` |
|      - | 1226 | ` *  counts only when it fits ENTIRELY inside the first $length bytes —` |
|      - | 1227 | ` *  stream_get_line($h,4,"--") over "abc--def" answers "abc-", the raw window,` |
|      - | 1228 | ` *  because the ending straddles its edge — and a capped read consumes no` |
|      - | 1229 | ` *  ending that starts at the boundary. $length 0 means php's 8192 default; at` |
|      - | 1230 | ` *  EOF the remainder is returned as-is, and false only when nothing is left.` |
|      - | 1231 | ` */` |
|     62 | 1232 | `PH7_PRIVATE int PH7_builtin_stream_get_line(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      3 | 1233 | `{` |
|      - | 1234 | `	const ph7_io_stream *pStream;` |
|     65 | 1235 | `	const char *zEnding = "";` |
|      - | 1236 | `	io_private *pDev;` |
|      - | 1237 | `	ph7_int64 nMaxLen;` |
|     65 | 1238 | `	int nEndLen = 0;` |
|     65 | 1239 | `	sxu32 iScanFrom = 0;` |
|     65 | 1240 | `	int bEof = 0;` |
|     65 | 1241 | `	if( nArg < 2 ){` |
|      - | 1242 | `		/* The central arity screen reports this; keep a refusal for a direct call. */` |
|    ! 0 | 1243 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 1244 | `		return PH7_OK;` |
|      - | 1245 | `	}` |
|     65 | 1246 | `	if( !ph7_value_is_resource(apArg[0]) ){` |
|      4 | 1247 | `		return PH7_VmThrowException(pCtx,"TypeError",` |
|      - | 1248 | `			"stream_get_line(): Argument #1 ($stream) must be of type resource, %s given",` |
|      1 | 1249 | `			ph7_type_name(apArg[0]));` |
|      - | 1250 | `	}` |
|     63 | 1251 | `	pDev = (io_private *)ph7_value_to_resource(apArg[0]);` |
|     63 | 1252 | `	if( IO_PRIVATE_INVALID(pDev) ){` |
|      - | 1253 | `		/* A closed or foreign resource is php's own TypeError, not a warning. */` |
|      3 | 1254 | `		return PH7_VmThrowException(pCtx,"TypeError",` |
|      - | 1255 | `			"stream_get_line(): Argument #1 ($stream) must be an open stream resource");` |
|      - | 1256 | `	}` |
|     61 | 1257 | `	pStream = pDev->pStream;` |
|     61 | 1258 | `	if( pStream == 0 ){` |
|    ! 0 | 1259 | `		ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,` |
|      - | 1260 | `			"IO routine(%s) not implemented in the underlying stream(%s) device,PH7 is returning FALSE",` |
|    ! 0 | 1261 | `			ph7_function_name(pCtx),"null_stream"` |
|      - | 1262 | `			);` |
|    ! 0 | 1263 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 1264 | `		return PH7_OK;` |
|      - | 1265 | `	}` |
|     61 | 1266 | `	nMaxLen = ph7_value_to_int64(apArg[1]);` |
|     61 | 1267 | `	if( nMaxLen < 0 ){` |
|      3 | 1268 | `		return PH7_VmThrowException(pCtx,"ValueError",` |
|      - | 1269 | `			"stream_get_line(): Argument #2 ($length) must be greater than or equal to 0");` |
|      - | 1270 | `	}` |
|     59 | 1271 | `	if( nMaxLen == 0 ){` |
|      - | 1272 | `		/* php's documented default window */` |
|      3 | 1273 | `		nMaxLen = 8192;` |
|      1 | 1274 | `	}` |
|     59 | 1275 | `	if( nArg > 2 ){` |
|     55 | 1276 | `		zEnding = ph7_value_to_string(apArg[2],&nEndLen);` |
|     26 | 1277 | `	}` |
|     59 | 1278 | `	if( pDev->nOfft >= SyBlobLength(&pDev->sBuffer) ){` |
|      - | 1279 | `		/* Reset the working buffer so that we avoid excessive memory allocation */` |
|     31 | 1280 | `		SyBlobReset(&pDev->sBuffer);` |
|     31 | 1281 | `		pDev->nOfft = 0;` |
|     14 | 1282 | `	}` |
|      - | 1283 | `	/* Fill-and-scan: buffer chunks until the ending fits inside the window,` |
|      - | 1284 | `	 * the window itself fills, or the stream dries up. */` |
|     62 | 1285 | `	for(;;){` |
|    101 | 1286 | `		const char *zData = (const char *)SyBlobDataAt(&pDev->sBuffer,pDev->nOfft);` |
|    101 | 1287 | `		sxu32 nAvail = SyBlobLength(&pDev->sBuffer) - pDev->nOfft;` |
|    101 | 1288 | `		sxu32 nWindow = (nMaxLen < (ph7_int64)nAvail) ? (sxu32)nMaxLen : nAvail;` |
|      - | 1289 | `		ph7_int64 n;` |
|      - | 1290 | `		char zBuf[8192];` |
|    101 | 1291 | `		if( nEndLen > 0 && (sxu32)nEndLen <= nWindow ){` |
|      - | 1292 | `			/* The ending must END inside the window to count. Resume the scan` |
|      - | 1293 | `			 * where the previous fill left off — a candidate can straddle two` |
|      - | 1294 | `			 * fills, so back up by the ending's length less one. */` |
|      - | 1295 | `			sxu32 i;` |
|  40179 | 1296 | `			for( i = iScanFrom ; i + (sxu32)nEndLen <= nWindow ; i++ ){` |
|  40147 | 1297 | `				if( zData[i] == zEnding[0] && SyMemcmp(&zData[i],zEnding,(sxu32)nEndLen) == 0 ){` |
|     27 | 1298 | `					pDev->nOfft += i + (sxu32)nEndLen;` |
|     27 | 1299 | `					ph7_result_string(pCtx,zData,(int)i);` |
|     43 | 1300 | `					return PH7_OK;` |
|      - | 1301 | `				}` |
|  20063 | 1302 | `			}` |
|     34 | 1303 | `			iScanFrom = i;` |
|     16 | 1304 | `		}` |
|     77 | 1305 | `		if( (ph7_int64)nAvail >= nMaxLen ){` |
|      - | 1306 | `			/* Window full with no ending inside it: hand the window back raw,` |
|      - | 1307 | `			 * anything past it (an ending included) stays buffered. */` |
|     18 | 1308 | `			pDev->nOfft += (sxu32)nMaxLen;` |
|     18 | 1309 | `			ph7_result_string(pCtx,zData,(int)nMaxLen);` |
|     18 | 1310 | `			return PH7_OK;` |
|      - | 1311 | `		}` |
|     61 | 1312 | `		if( bEof ){` |
|      - | 1313 | `			/* EOF: the remainder as-is, false when nothing is left. */` |
|     18 | 1314 | `			if( nAvail > 0 ){` |
|     12 | 1315 | `				pDev->nOfft += nAvail;` |
|     12 | 1316 | `				ph7_result_string(pCtx,zData,(int)nAvail);` |
|      7 | 1317 | `			}else{` |
|      8 | 1318 | `				ph7_result_bool(pCtx,0);` |
|      - | 1319 | `			}` |
|     18 | 1320 | `			return PH7_OK;` |
|      - | 1321 | `		}` |
|     45 | 1322 | `		n = IoPrivateDeviceRead(pDev,zBuf,(ph7_int64)sizeof(zBuf));` |
|     45 | 1323 | `		if( n < 1 ){` |
|     18 | 1324 | `			bEof = 1;` |
|     18 | 1325 | `			if( n == 0 ){` |
|     18 | 1326 | `				pDev->bEof = 1;` |
|      8 | 1327 | `			}` |
|     18 | 1328 | `			continue;` |
|      - | 1329 | `		}` |
|     29 | 1330 | `		if( SXRET_OK != SyBlobAppend(&pDev->sBuffer,zBuf,(sxu32)n) ){` |
|    ! 0 | 1331 | `			return PH7_ContextMemoryError(pCtx);` |
|      - | 1332 | `		}` |
|      3 | 1333 | `	}` |
|     34 | 1334 | `}` |
|      - | 1335 | `/*` |
|      - | 1336 | ` * string fread(resource $handle,int64 $length)` |
|      - | 1337 | ` *  Binary-safe file read.` |
|      - | 1338 | ` * Parameters` |
|      - | 1339 | ` *  $handle` |
|      - | 1340 | ` *   The file pointer.` |
|      - | 1341 | ` * $length` |
|      - | 1342 | ` *  Up to length number of bytes read.` |
|      - | 1343 | ` * Return` |
|      - | 1344 | ` *  The data readen on success or FALSE on failure.` |
|      - | 1345 | ` */` |
|    141 | 1346 | `PH7_PRIVATE int PH7_builtin_fread(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      5 | 1347 | `{` |
|      - | 1348 | `	const ph7_io_stream *pStream;` |
|      - | 1349 | `	io_private *pDev;` |
|      - | 1350 | `	ph7_int64 nRead;` |
|      - | 1351 | `	void *pBuf;` |
|      - | 1352 | `	int nLen;` |
|    146 | 1353 | `	if( nArg < 1 \|\| !ph7_value_is_resource(apArg[0]) ){` |
|      - | 1354 | `		/* Missing/Invalid arguments,return FALSE */` |
|    ! 0 | 1355 | `		ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"Expecting an IO handle");` |
|    ! 0 | 1356 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 1357 | `		return PH7_OK;` |
|      - | 1358 | `	}` |
|      - | 1359 | `	/* Extract our private data */` |
|    146 | 1360 | `	pDev = (io_private *)ph7_value_to_resource(apArg[0]);` |
|      - | 1361 | `	/* Make sure we are dealing with a valid io_private instance */` |
|    146 | 1362 | `	if( IO_PRIVATE_INVALID(pDev) ){` |
|      - | 1363 | `		/*Expecting an IO handle */` |
|    ! 0 | 1364 | `		ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"Expecting an IO handle");` |
|    ! 0 | 1365 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 1366 | `		return PH7_OK;` |
|      - | 1367 | `	}` |
|      - | 1368 | `	/* Point to the target IO stream device */` |
|    146 | 1369 | `	pStream = pDev->pStream;` |
|    146 | 1370 | `	if( pStream == 0  ){` |
|    ! 0 | 1371 | `		ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,` |
|      - | 1372 | `			"IO routine(%s) not implemented in the underlying stream(%s) device,PH7 is returning FALSE",` |
|    ! 0 | 1373 | `			ph7_function_name(pCtx),pStream ? pStream->zName : "null_stream"` |
|      - | 1374 | `			);` |
|    ! 0 | 1375 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 1376 | `		return PH7_OK;` |
|      - | 1377 | `	}` |
|    146 | 1378 | `        nLen = 4096;` |
|    146 | 1379 | `	if( nArg > 1 ){` |
|      - | 1380 | `	  /* PHP 8 raises a catchable ValueError for a non-positive length; the` |
|      - | 1381 | `	   * $length parameter is non-nullable, so a NULL is rejected upstream by` |
|      - | 1382 | `	   * the central type screen (the recorded null-policy divergence). */` |
|    146 | 1383 | `	  ph7_int64 nWant = ph7_value_to_int64(apArg[1]);` |
|    146 | 1384 | `	  if( nWant < 1 ){` |
|      5 | 1385 | `		return PH7_VmThrowException(pCtx,"ValueError",` |
|      - | 1386 | `			"fread(): Argument #2 ($length) must be greater than 0");` |
|      - | 1387 | `	  }` |
|    142 | 1388 | `	  nLen = (int)nWant;` |
|    142 | 1389 | `	  if( nLen < 1 ){` |
|      - | 1390 | `		/* A > INT_MAX length overflowed the int cast; read a single chunk` |
|      - | 1391 | `		 * (StreamRead returns only what the stream holds) instead of` |
|      - | 1392 | `		 * over-allocating -- matching the pre-existing behavior for lengths` |
|      - | 1393 | `		 * that do not fit an int. */` |
|    ! 0 | 1394 | `		nLen = 4096;` |
|    ! 0 | 1395 | `	  }` |
|     68 | 1396 | `        }` |
|      - | 1397 | `	/* Allocate enough buffer */` |
|    142 | 1398 | `	pBuf = ph7_context_alloc_chunk(pCtx,(unsigned int)nLen,FALSE,FALSE);` |
|    142 | 1399 | `	if( pBuf == 0 ){` |
|    ! 0 | 1400 | `		ph7_context_throw_error(pCtx,PH7_CTX_ERR,"PH7 is running out of memory");` |
|    ! 0 | 1401 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 1402 | `		return PH7_OK;` |
|      - | 1403 | `	}` |
|      - | 1404 | `	/* Perform the requested operation */` |
|    142 | 1405 | `	errno = 0;` |
|    142 | 1406 | `	nRead = PH7_StreamRead(pDev,pBuf,(ph7_int64)nLen);` |
|    142 | 1407 | `	if( nRead < 0 && (errno == EAGAIN \|\| errno == EWOULDBLOCK) ){` |
|      - | 1408 | `		/* Nothing had ARRIVED yet, which is not a failure: php answers "" for a` |
|      - | 1409 | ``		 * read that could not proceed and reserves `false` for one that broke.`` |
|      - | 1410 | `		 * The question is answered by errno rather than by a per-handle flag —` |
|      - | 1411 | `		 * two handles can share one descriptor (every php://stdin is fd 0), so` |
|      - | 1412 | `		 * a flag on the handle that set the mode answers wrongly for its` |
|      - | 1413 | `		 * siblings, and a genuine EBADF on a non-blocking write-only handle` |
|      - | 1414 | `		 * would come back as "" rather than false. When a TIMEOUT is what` |
|      - | 1415 | ``		 * expired, php reports false and sets the metadata's `timed_out`. */`` |
|      7 | 1416 | `		if( pDev->bHasTimeout && !pDev->bNonBlock ){` |
|      - | 1417 | `			/* A handle in NON-BLOCKING mode is the other case: it answers "" for` |
|      - | 1418 | `			 * a read that found nothing whether or not a timeout is armed, and` |
|      - | 1419 | ``			 * every socket now carries `default_socket_timeout`. */`` |
|      3 | 1420 | `			pDev->bTimedOut = 1;` |
|      3 | 1421 | `			ph7_result_bool(pCtx,0);` |
|      2 | 1422 | `		}else{` |
|      5 | 1423 | `			ph7_result_string(pCtx,"",0);` |
|      1 | 1424 | `		}` |
|    139 | 1425 | `	}else if( nRead < 0 ){` |
|      - | 1426 | `		/* A real IO error, which is php's other false here. */` |
|     11 | 1427 | `		ph7_result_bool(pCtx,0);` |
|      7 | 1428 | `	}else{` |
|      - | 1429 | `		/* Make a copy of the data just read. Zero bytes is EOF, not a failure:` |
|      - | 1430 | `		 * php answers "" for it (php_stream_read returns 0 and the empty string` |
|      - | 1431 | `		 * rides through), where PHL answered FALSE — so the ordinary` |
|      - | 1432 | ``		 * `while (!feof($f)) $buf .= fread($f, 8192);` loop ended on a value`` |
|      - | 1433 | ``		 * that means "the read failed" and a `=== false` guard fired at EOF. */`` |
|    128 | 1434 | `		ph7_result_string(pCtx,(const char *)pBuf,nRead > 0 ? (int)nRead : 0);` |
|      - | 1435 | `	}` |
|      - | 1436 | `	/* Release the buffer */` |
|    142 | 1437 | `	ph7_context_free_chunk(pCtx,pBuf);` |
|    142 | 1438 | `	return PH7_OK;` |
|     75 | 1439 | `}` |
|      - | 1440 | `/*` |
|      - | 1441 | ` * array fgetcsv(resource $handle [, int $length = 0` |
|      - | 1442 | ` *         [,string $delimiter = ','[,string $enclosure = '"'[,string $escape='\\']]]])` |
|      - | 1443 | ` * Gets line from file pointer and parse for CSV fields.` |
|      - | 1444 | ` * Parameters` |
|      - | 1445 | ` * $handle` |
|      - | 1446 | ` *   The file pointer.` |
|      - | 1447 | ` * $length` |
|      - | 1448 | ` *  Reading ends when length - 1 bytes have been read, on a newline` |
|      - | 1449 | ` *  (which is included in the return value), or on EOF (whichever comes first).` |
|      - | 1450 | ` *  If no length is specified, it will keep reading from the stream until it reaches` |
|      - | 1451 | ` *  the end of the line.` |
|      - | 1452 | ` * $delimiter` |
|      - | 1453 | ` *   Set the field delimiter (one character only).` |
|      - | 1454 | ` * $enclosure` |
|      - | 1455 | ` *   Set the field enclosure character (one character only).` |
|      - | 1456 | ` * $escape` |
|      - | 1457 | ` *   Set the escape character (one character only). Defaults as a backslash (\)` |
|      - | 1458 | ` * Return` |
|      - | 1459 | ` *  Returns a string of up to length - 1 bytes read from the file pointed to by handle.` |
|      - | 1460 | ` *  If there is no more data to read in the file pointer, then FALSE is returned.` |
|      - | 1461 | ` *  If an error occurs, FALSE is returned.` |
|      - | 1462 | ` */` |
|     68 | 1463 | `PH7_PRIVATE int PH7_builtin_fgetcsv(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 1464 | `{` |
|      - | 1465 | `	const ph7_io_stream *pStream;` |
|      - | 1466 | `	const char *zLine;` |
|      - | 1467 | `	io_private *pDev;` |
|      - | 1468 | `	ph7_int64 n,nLen;` |
|     69 | 1469 | `	int delim  = ',';   /* Delimiter */` |
|     69 | 1470 | `	int encl   = '"' ;  /* Enclosure */` |
|     69 | 1471 | `	int escape = '\\';  /* Escape character */` |
|     69 | 1472 | `	if( nArg < 1 \|\| !ph7_value_is_resource(apArg[0]) ){` |
|      - | 1473 | `		/* Missing/Invalid arguments,return FALSE */` |
|    ! 0 | 1474 | `		ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"Expecting an IO handle");` |
|    ! 0 | 1475 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 1476 | `		return PH7_OK;` |
|      - | 1477 | `	}` |
|      - | 1478 | `	/* Extract our private data */` |
|     69 | 1479 | `	pDev = (io_private *)ph7_value_to_resource(apArg[0]);` |
|      - | 1480 | `	/* Make sure we are dealing with a valid io_private instance */` |
|     69 | 1481 | `	if( IO_PRIVATE_INVALID(pDev) ){` |
|      - | 1482 | `		/*Expecting an IO handle */` |
|    ! 0 | 1483 | `		ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"Expecting an IO handle");` |
|    ! 0 | 1484 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 1485 | `		return PH7_OK;` |
|      - | 1486 | `	}` |
|      - | 1487 | `	/* Point to the target IO stream device */` |
|     69 | 1488 | `	pStream = pDev->pStream;` |
|     69 | 1489 | `	if( pStream == 0  ){` |
|    ! 0 | 1490 | `		ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,` |
|      - | 1491 | `			"IO routine(%s) not implemented in the underlying stream(%s) device,PH7 is returning FALSE",` |
|    ! 0 | 1492 | `			ph7_function_name(pCtx),pStream ? pStream->zName : "null_stream"` |
|      - | 1493 | `			);` |
|    ! 0 | 1494 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 1495 | `		return PH7_OK;` |
|      - | 1496 | `	}` |
|     69 | 1497 | `	if( nArg > 2 ){` |
|      - | 1498 | `		/* php validates $separator/$enclosure/$escape BEFORE $length (probed` |
|      - | 1499 | `		 * ordering) and even when the stream is already at EOF. */` |
|     67 | 1500 | `		sxi32 rc = PH7_CsvCharArg(pCtx,apArg[2],3,"separator",0,&delim);` |
|     67 | 1501 | `		if( rc != PH7_OK ){` |
|      7 | 1502 | `			return rc;` |
|      - | 1503 | `		}` |
|     61 | 1504 | `		if( nArg > 3 ){` |
|     61 | 1505 | `			rc = PH7_CsvCharArg(pCtx,apArg[3],4,"enclosure",0,&encl);` |
|     61 | 1506 | `			if( rc != PH7_OK ){` |
|      3 | 1507 | `				return rc;` |
|      - | 1508 | `			}` |
|     59 | 1509 | `			if( nArg > 4 ){` |
|     59 | 1510 | `				rc = PH7_CsvCharArg(pCtx,apArg[4],5,"escape",1,&escape);` |
|     59 | 1511 | `				if( rc != PH7_OK ){` |
|      3 | 1512 | `					return rc;` |
|      - | 1513 | `				}` |
|     28 | 1514 | `			}` |
|     28 | 1515 | `		}` |
|     28 | 1516 | `	}` |
|     59 | 1517 | `	nLen = -1;` |
|     59 | 1518 | `	if( nArg > 1 && !ph7_value_is_null(apArg[1]) ){` |
|      - | 1519 | `		/* Maximum data to read. PHP 8 raises a catchable ValueError when the` |
|      - | 1520 | `		 * length is negative or hits PHP_INT_MAX (the valid range is` |
|      - | 1521 | `		 * 0..PHP_INT_MAX-1, where 0/NULL both mean "no limit"). */` |
|     49 | 1522 | `		nLen = ph7_value_to_int64(apArg[1]);` |
|     49 | 1523 | `		if( nLen < 0 \|\| nLen >= SXI64_HIGH ){` |
|      3 | 1524 | `			return PH7_VmThrowException(pCtx,"ValueError",` |
|      - | 1525 | `				"fgetcsv(): Argument #2 ($length) must be between 0 and 9223372036854775806");` |
|      - | 1526 | `		}` |
|      - | 1527 | `		/* 0 means "no limit", which StreamReadLine already treats as unlimited. */` |
|     23 | 1528 | `	}` |
|      - | 1529 | `	/* Perform the requested operation */` |
|     57 | 1530 | `	n = StreamReadLine(pDev,&zLine,nLen);` |
|     57 | 1531 | `	if( n < 1 ){` |
|      - | 1532 | `		/* EOF or IO error,return FALSE */` |
|     13 | 1533 | `		ph7_result_bool(pCtx,0);` |
|      7 | 1534 | `	}else{` |
|      - | 1535 | `		ph7_value *pArray;` |
|      - | 1536 | `		SyBlob sRec;` |
|      - | 1537 | `		PH7_CsvScan sScan;` |
|      - | 1538 | `		/* Create our array */` |
|     45 | 1539 | `		pArray = ph7_context_new_array(pCtx);` |
|     45 | 1540 | `		if( pArray == 0 ){` |
|    ! 0 | 1541 | `			ph7_context_throw_error(pCtx,PH7_CTX_ERR,"PH7 is running out of memory");` |
|    ! 0 | 1542 | `			ph7_result_null(pCtx);` |
|    ! 0 | 1543 | `			return PH7_OK;` |
|      - | 1544 | `		}` |
|      - | 1545 | `		/* A RECORD is not a line: an enclosure that is still open when the line` |
|      - | 1546 | `		 * ends means the value contains the newline and the record continues on` |
|      - | 1547 | `		 * the next one. Parsing a single line and stopping split such a value` |
|      - | 1548 | `		 * across two rows, with the halves quoted wrong. The whole record is` |
|      - | 1549 | `		 * gathered FIRST and parsed once -- the scan below carries its position` |
|      - | 1550 | `		 * across the appends, so a stray quote costs one pass over the file` |
|      - | 1551 | `		 * rather than one per line. */` |
|     45 | 1552 | `		SyBlobInit(&sRec,&pCtx->pVm->sAllocator);` |
|     45 | 1553 | `		SyBlobAppend(&sRec,(const void *)zLine,(sxu32)n);` |
|     45 | 1554 | `		PH7_CsvScanInit(&sScan);` |
|     55 | 1555 | `		while( PH7_CsvScanOpen(&sScan,(const char *)SyBlobData(&sRec),` |
|     27 | 1556 | `				SyBlobLength(&sRec),delim,encl,escape) ){` |
|     13 | 1557 | `			if( SyBlobLength(&sRec) >= (sxu32)SXI32_HIGH ){` |
|      - | 1558 | `				/* The parser measures in int; stop rather than wrap negative. */` |
|    ! 0 | 1559 | `				break;` |
|      - | 1560 | `			}` |
|      - | 1561 | `			/* Continuation reads are NOT capped by $length: php's limit applies` |
|      - | 1562 | `			 * to the first read of the record, and reusing it here ended the` |
|      - | 1563 | `			 * record on a chunk boundary in the middle of a quoted value. */` |
|     13 | 1564 | `			n = StreamReadLine(pDev,&zLine,0);` |
|     13 | 1565 | `			if( n < 1 ){` |
|      - | 1566 | `				/* EOF inside the enclosure: php answers what it has. */` |
|      3 | 1567 | `				break;` |
|      - | 1568 | `			}` |
|     11 | 1569 | `			SyBlobAppend(&sRec,(const void *)zLine,(sxu32)n);` |
|      1 | 1570 | `		}` |
|     67 | 1571 | `		PH7_ProcessCsv(pArray,(const char *)SyBlobData(&sRec),` |
|     44 | 1572 | `			(int)SyBlobLength(&sRec),delim,encl,escape,0);` |
|     45 | 1573 | `		SyBlobRelease(&sRec);` |
|      - | 1574 | `		/* Return the freshly created array  */` |
|     45 | 1575 | `		ph7_result_value(pCtx,pArray);` |
|      - | 1576 | `	}` |
|     57 | 1577 | `	return PH7_OK;` |
|     35 | 1578 | `}` |
|      - | 1579 | `/*` |
|      - | 1580 | ` * string fgetss(resource $handle [,int $length [,string $allowable_tags ]])` |
|      - | 1581 | ` *  Gets line from file pointer and strip HTML tags.` |
|      - | 1582 | ` * Parameters` |
|      - | 1583 | ` * $handle` |
|      - | 1584 | ` *   The file pointer.` |
|      - | 1585 | ` * $length` |
|      - | 1586 | ` *  Reading ends when length - 1 bytes have been read, on a newline` |
|      - | 1587 | ` *  (which is included in the return value), or on EOF (whichever comes first).` |
|      - | 1588 | ` *  If no length is specified, it will keep reading from the stream until it reaches` |
|      - | 1589 | ` *  the end of the line.` |
|      - | 1590 | ` * $allowable_tags` |
|      - | 1591 | ` *  You can use the optional second parameter to specify tags which should not be stripped.` |
|      - | 1592 | ` * Return` |
|      - | 1593 | ` *  Returns a string of up to length - 1 bytes read from the file pointed to by` |
|      - | 1594 | ` *  handle, with all HTML and PHP code stripped. If an error occurs, returns FALSE.` |
|      - | 1595 | ` */` |
|      2 | 1596 | `PH7_PRIVATE int PH7_builtin_fgetss(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 1597 | `{` |
|      - | 1598 | `	const ph7_io_stream *pStream;` |
|      - | 1599 | `	const char *zLine;` |
|      - | 1600 | `	io_private *pDev;` |
|      - | 1601 | `	ph7_int64 n,nLen;` |
|      3 | 1602 | `	if( nArg < 1 \|\| !ph7_value_is_resource(apArg[0]) ){` |
|      - | 1603 | `		/* Missing/Invalid arguments,return FALSE */` |
|    ! 0 | 1604 | `		ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"Expecting an IO handle");` |
|    ! 0 | 1605 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 1606 | `		return PH7_OK;` |
|      - | 1607 | `	}` |
|      - | 1608 | `	/* Extract our private data */` |
|      3 | 1609 | `	pDev = (io_private *)ph7_value_to_resource(apArg[0]);` |
|      - | 1610 | `	/* Make sure we are dealing with a valid io_private instance */` |
|      3 | 1611 | `	if( IO_PRIVATE_INVALID(pDev) ){` |
|      - | 1612 | `		/*Expecting an IO handle */` |
|    ! 0 | 1613 | `		ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"Expecting an IO handle");` |
|    ! 0 | 1614 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 1615 | `		return PH7_OK;` |
|      - | 1616 | `	}` |
|      - | 1617 | `	/* Point to the target IO stream device */` |
|      3 | 1618 | `	pStream = pDev->pStream;` |
|      3 | 1619 | `	if( pStream == 0  ){` |
|    ! 0 | 1620 | `		ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,` |
|      - | 1621 | `			"IO routine(%s) not implemented in the underlying stream(%s) device,PH7 is returning FALSE",` |
|    ! 0 | 1622 | `			ph7_function_name(pCtx),pStream ? pStream->zName : "null_stream"` |
|      - | 1623 | `			);` |
|    ! 0 | 1624 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 1625 | `		return PH7_OK;` |
|      - | 1626 | `	}` |
|      3 | 1627 | `	nLen = -1;` |
|      3 | 1628 | `	if( nArg > 1 ){` |
|      - | 1629 | `		/* Maximum data to read */` |
|    ! 0 | 1630 | `		nLen = ph7_value_to_int64(apArg[1]);` |
|    ! 0 | 1631 | `	}` |
|      - | 1632 | `	/* Perform the requested operation */` |
|      3 | 1633 | `	n = StreamReadLine(pDev,&zLine,nLen);` |
|      3 | 1634 | `	if( n < 1 ){` |
|      - | 1635 | `		/* EOF or IO error,return FALSE */` |
|    ! 0 | 1636 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 1637 | `	}else{` |
|      3 | 1638 | `		const char *zTaglist = 0;` |
|      3 | 1639 | `		int nTaglen = 0;` |
|      3 | 1640 | `		if( nArg > 2 && ph7_value_is_string(apArg[2]) ){` |
|      - | 1641 | `			/* Allowed tag */` |
|    ! 0 | 1642 | `			zTaglist = ph7_value_to_string(apArg[2],&nTaglen);` |
|    ! 0 | 1643 | `		}` |
|      - | 1644 | `		/* Process data just read */` |
|      3 | 1645 | `		PH7_StripTagsFromString(pCtx,zLine,(int)n,zTaglist,nTaglen,0);` |
|      - | 1646 | `	}` |
|      3 | 1647 | `	return PH7_OK;` |
|      2 | 1648 | `}` |
|      - | 1649 | `/*` |
|      - | 1650 | ` * string readdir(resource $dir_handle)` |
|      - | 1651 | ` *   Read entry from directory handle.` |
|      - | 1652 | ` * Parameter` |
|      - | 1653 | ` *  $dir_handle` |
|      - | 1654 | ` *   The directory handle resource previously opened with opendir().` |
|      - | 1655 | ` * Return` |
|      - | 1656 | ` *  Returns the filename on success or FALSE on failure.` |
|      - | 1657 | ` */` |
|  13327 | 1658 | `PH7_PRIVATE int PH7_builtin_readdir(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      5 | 1659 | `{` |
|      - | 1660 | `	const ph7_io_stream *pStream;` |
|      - | 1661 | `	io_private *pDev;` |
|      - | 1662 | `	int rc;` |
|  13332 | 1663 | `	if( nArg < 1 \|\| !ph7_value_is_resource(apArg[0]) ){` |
|      - | 1664 | `		/* Missing/Invalid arguments,return FALSE */` |
|    ! 0 | 1665 | `		ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"Expecting an IO handle");` |
|    ! 0 | 1666 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 1667 | `		return PH7_OK;` |
|      - | 1668 | `	}` |
|      - | 1669 | `	/* Extract our private data */` |
|  13332 | 1670 | `	pDev = (io_private *)ph7_value_to_resource(apArg[0]);` |
|      - | 1671 | `	/* Make sure we are dealing with a valid io_private instance */` |
|  13332 | 1672 | `	if( IO_PRIVATE_INVALID(pDev) ){` |
|      - | 1673 | `		/*Expecting an IO handle */` |
|    ! 0 | 1674 | `		ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"Expecting an IO handle");` |
|    ! 0 | 1675 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 1676 | `		return PH7_OK;` |
|      - | 1677 | `	}` |
|      - | 1678 | `	/* Point to the target IO stream device */` |
|  13332 | 1679 | `	pStream = pDev->pStream;` |
|  13332 | 1680 | `	if( pStream == 0  \|\| pStream->xReadDir == 0 ){` |
|    ! 0 | 1681 | `		ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,` |
|      - | 1682 | `			"IO routine(%s) not implemented in the underlying stream(%s) device,PH7 is returning FALSE",` |
|    ! 0 | 1683 | `			ph7_function_name(pCtx),pStream ? pStream->zName : "null_stream"` |
|      - | 1684 | `			);` |
|    ! 0 | 1685 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 1686 | `		return PH7_OK;` |
|      - | 1687 | `	}` |
|  13332 | 1688 | `	ph7_result_bool(pCtx,0);` |
|      - | 1689 | `	/* Perform the requested operation */` |
|  13332 | 1690 | `	rc = pStream->xReadDir(pDev->pHandle,pCtx);` |
|  13332 | 1691 | `	if( rc != PH7_OK ){` |
|      - | 1692 | `		/* Return FALSE */` |
|   1274 | 1693 | `		ph7_result_bool(pCtx,0);` |
|    634 | 1694 | `	}` |
|  13332 | 1695 | `	return PH7_OK;` |
|   6666 | 1696 | `}` |
|      - | 1697 | `/*` |
|      - | 1698 | ` * void rewinddir(resource $dir_handle)` |
|      - | 1699 | ` *   Rewind directory handle.` |
|      - | 1700 | ` * Parameter` |
|      - | 1701 | ` *  $dir_handle` |
|      - | 1702 | ` *   The directory handle resource previously opened with opendir().` |
|      - | 1703 | ` * Return` |
|      - | 1704 | ` *  FALSE on failure.` |
|      - | 1705 | ` */` |
|      4 | 1706 | `PH7_PRIVATE int PH7_builtin_rewinddir(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      2 | 1707 | `{` |
|      - | 1708 | `	const ph7_io_stream *pStream;` |
|      - | 1709 | `	io_private *pDev;` |
|      6 | 1710 | `	if( nArg < 1 \|\| !ph7_value_is_resource(apArg[0]) ){` |
|      - | 1711 | `		/* Missing/Invalid arguments,return FALSE */` |
|    ! 0 | 1712 | `		ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"Expecting an IO handle");` |
|    ! 0 | 1713 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 1714 | `		return PH7_OK;` |
|      - | 1715 | `	}` |
|      - | 1716 | `	/* Extract our private data */` |
|      6 | 1717 | `	pDev = (io_private *)ph7_value_to_resource(apArg[0]);` |
|      - | 1718 | `	/* Make sure we are dealing with a valid io_private instance */` |
|      6 | 1719 | `	if( IO_PRIVATE_INVALID(pDev) ){` |
|      - | 1720 | `		/*Expecting an IO handle */` |
|    ! 0 | 1721 | `		ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"Expecting an IO handle");` |
|    ! 0 | 1722 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 1723 | `		return PH7_OK;` |
|      - | 1724 | `	}` |
|      - | 1725 | `	/* Point to the target IO stream device */` |
|      6 | 1726 | `	pStream = pDev->pStream;` |
|      6 | 1727 | `	if( pStream == 0  \|\| pStream->xRewindDir == 0 ){` |
|    ! 0 | 1728 | `		ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,` |
|      - | 1729 | `			"IO routine(%s) not implemented in the underlying stream(%s) device,PH7 is returning FALSE",` |
|    ! 0 | 1730 | `			ph7_function_name(pCtx),pStream ? pStream->zName : "null_stream"` |
|      - | 1731 | `			);` |
|    ! 0 | 1732 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 1733 | `		return PH7_OK;` |
|      - | 1734 | `	}` |
|      - | 1735 | `	/* Perform the requested operation */` |
|      6 | 1736 | `	pStream->xRewindDir(pDev->pHandle);` |
|      6 | 1737 | `	return PH7_OK;` |
|      4 | 1738 | ` }` |
|      - | 1739 | `/* Forward declaration */` |
|      - | 1740 | `static void ReleaseIOPrivate(ph7_context *pCtx,io_private *pDev);` |
|      - | 1741 | `/*` |
|      - | 1742 | ` * void closedir(resource $dir_handle)` |
|      - | 1743 | ` *   Close directory handle.` |
|      - | 1744 | ` * Parameter` |
|      - | 1745 | ` *  $dir_handle` |
|      - | 1746 | ` *   The directory handle resource previously opened with opendir().` |
|      - | 1747 | ` * Return` |
|      - | 1748 | ` *  FALSE on failure.` |
|      - | 1749 | ` */` |
|   1279 | 1750 | `PH7_PRIVATE int PH7_builtin_closedir(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      5 | 1751 | `{` |
|      - | 1752 | `	const ph7_io_stream *pStream;` |
|      - | 1753 | `	io_private *pDev;` |
|   1284 | 1754 | `	if( nArg < 1 \|\| !ph7_value_is_resource(apArg[0]) ){` |
|      - | 1755 | `		/* Missing/Invalid arguments,return FALSE */` |
|    ! 0 | 1756 | `		ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"Expecting an IO handle");` |
|    ! 0 | 1757 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 1758 | `		return PH7_OK;` |
|      - | 1759 | `	}` |
|      - | 1760 | `	/* Extract our private data */` |
|   1284 | 1761 | `	pDev = (io_private *)ph7_value_to_resource(apArg[0]);` |
|      - | 1762 | `	/* Make sure we are dealing with a valid io_private instance */` |
|   1284 | 1763 | `	if( IO_PRIVATE_INVALID(pDev) ){` |
|      - | 1764 | `		/*Expecting an IO handle */` |
|    ! 0 | 1765 | `		ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"Expecting an IO handle");` |
|    ! 0 | 1766 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 1767 | `		return PH7_OK;` |
|      - | 1768 | `	}` |
|      - | 1769 | `	/* Point to the target IO stream device */` |
|   1284 | 1770 | `	pStream = pDev->pStream;` |
|   1284 | 1771 | `	if( pStream == 0  \|\| pStream->xCloseDir == 0 ){` |
|    ! 0 | 1772 | `		ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,` |
|      - | 1773 | `			"IO routine(%s) not implemented in the underlying stream(%s) device,PH7 is returning FALSE",` |
|    ! 0 | 1774 | `			ph7_function_name(pCtx),pStream ? pStream->zName : "null_stream"` |
|      - | 1775 | `			);` |
|    ! 0 | 1776 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 1777 | `		return PH7_OK;` |
|      - | 1778 | `	}` |
|      - | 1779 | `	/* Perform the requested operation */` |
|   1284 | 1780 | `	PH7_StreamFilterReleaseChains(pDev);` |
|   1284 | 1781 | `	pStream->xCloseDir(pDev->pHandle);` |
|      - | 1782 | `	/* Keep the handle alive but flag it closed (php: gettype()=='resource (closed)') */` |
|   1284 | 1783 | `	MarkIOPrivateClosed(pDev);` |
|   1284 | 1784 | `	return PH7_OK;` |
|    644 | 1785 | ` }` |
|      - | 1786 | `/*` |
|      - | 1787 | ` * resource opendir(string $path[,resource $context])` |
|      - | 1788 | ` *  Open directory handle.` |
|      - | 1789 | ` * Parameters` |
|      - | 1790 | ` * $path` |
|      - | 1791 | ` *   The directory path that is to be opened.` |
|      - | 1792 | ` * $context` |
|      - | 1793 | ` *   A context stream resource.` |
|      - | 1794 | ` * Return` |
|      - | 1795 | ` *  A directory handle resource on success,or FALSE on failure.` |
|      - | 1796 | ` */` |
|   1309 | 1797 | `PH7_PRIVATE int PH7_builtin_opendir(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      5 | 1798 | `{` |
|      - | 1799 | `	const ph7_io_stream *pStream;` |
|      - | 1800 | `	const char *zPath;` |
|      - | 1801 | `	io_private *pDev;` |
|   1314 | 1802 | `	int iLen,rc,bThrew = 0;` |
|   1314 | 1803 | `	if( nArg < 1 \|\| !ph7_value_is_string(apArg[0]) ){` |
|      - | 1804 | `		/* Missing/Invalid arguments,return FALSE */` |
|    ! 0 | 1805 | `		ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"Expecting a directory path");` |
|    ! 0 | 1806 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 1807 | `		return PH7_OK;` |
|      - | 1808 | `	}` |
|      - | 1809 | `	/* php refuses a resource that is not a stream-context. Nothing CONSUMES it` |
|      - | 1810 | `	 * here — dir_opendir() over a userland wrapper is not dispatched (§7.4` |
|      - | 1811 | `	 * slice-2 (e)) — but the refusal is the argument's contract. */` |
|   1314 | 1812 | `	PH7_StreamCtxFromArg(pCtx,nArg,apArg,1,"$context",0,&bThrew);` |
|   1314 | 1813 | `	if( bThrew ){` |
|      3 | 1814 | `		return PH7_OK;` |
|      - | 1815 | `	}` |
|      - | 1816 | `	/* Extract the target path */` |
|   1312 | 1817 | `	zPath  = ph7_value_to_string(apArg[0],&iLen);` |
|      - | 1818 | `	/* Try to extract a stream */` |
|   1312 | 1819 | `	pStream = PH7_VmGetStreamDevice(pCtx->pVm,&zPath,iLen);` |
|   1312 | 1820 | `	if( pStream == 0 ){` |
|    ! 0 | 1821 | `		VfsThrowNoDeviceWarning(pCtx,zPath,TRUE);` |
|    ! 0 | 1822 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 1823 | `		return PH7_OK;` |
|      - | 1824 | `	}` |
|   1312 | 1825 | `	if( pStream->xOpenDir == 0 ){` |
|    ! 0 | 1826 | `		ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,` |
|      - | 1827 | `			"IO routine(%s) not implemented in the underlying stream(%s) device",` |
|    ! 0 | 1828 | `			ph7_function_name(pCtx),pStream->zName` |
|      - | 1829 | `			);` |
|    ! 0 | 1830 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 1831 | `		return PH7_OK;` |
|      - | 1832 | `	}` |
|      - | 1833 | `	/* Allocate a new IO private instance */` |
|   1312 | 1834 | `	pDev = (io_private *)ph7_context_alloc_chunk(pCtx,sizeof(io_private),TRUE,FALSE);` |
|   1312 | 1835 | `	if( pDev == 0 ){` |
|    ! 0 | 1836 | `		ph7_context_throw_error(pCtx,PH7_CTX_ERR,"PH7 is running out of memory");` |
|    ! 0 | 1837 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 1838 | `		return PH7_OK;` |
|      - | 1839 | `	}` |
|      - | 1840 | `	/* Initialize the structure */` |
|   1312 | 1841 | `	InitIOPrivate(pCtx->pVm,pStream,pDev);` |
|      - | 1842 | `	/* Open the target directory */` |
|   1312 | 1843 | `	rc = pStream->xOpenDir(zPath,nArg > 1 ? apArg[1] : 0,&pDev->pHandle);` |
|   1312 | 1844 | `	if( rc != PH7_OK ){` |
|      - | 1845 | ``		/* IO error: php WARNS here — `opendir(/nope): Failed to open directory: No`` |
|      - | 1846 | ``		 * such file or directory` — and PHL returned FALSE in silence. The message`` |
|      - | 1847 | `` 		 * names the ACTIVE function, which is how dir() gets php's `dir(...)` `` |
|      - | 1848 | `		 * wording out of the same call. */` |
|      - | 1849 | `#ifdef __WINNT__` |
|      5 | 1850 | `		if( pStream == &sWinFileStream ){` |
|      - | 1851 | `			/* php's plain-files opener on Windows warns with the system's own` |
|      - | 1852 | `			 * reason first, and only then fails the way every platform does. */` |
|      - | 1853 | `			char zSys[256];` |
|      5 | 1854 | `			int iSaved = errno;` |
|      5 | 1855 | `			unsigned long nCode = PH7_WinOpenDirReason(zSys,(int)sizeof(zSys));` |
|      5 | 1856 | `			if( nCode ){` |
|      5 | 1857 | `				PH7_VmThrowWarningFmt(pCtx->pVm,"%s(%s): %s (code: %lu)",` |
|      - | 1858 | `					ph7_function_name(pCtx),zPath,zSys,nCode);` |
|      - | 1859 | `			}` |
|      5 | 1860 | `			errno = iSaved;` |
|      - | 1861 | `		}` |
|      - | 1862 | `#endif` |
|     47 | 1863 | `		PH7_VmThrowWarningFmt(pCtx->pVm,"%s(%s): Failed to open directory: %s",` |
|     28 | 1864 | `			ph7_function_name(pCtx),zPath,VfsStrerror(errno));` |
|     33 | 1865 | `		ReleaseIOPrivate(pCtx,pDev);` |
|     33 | 1866 | `		ph7_result_bool(pCtx,0);` |
|     19 | 1867 | `	}else{` |
|      - | 1868 | `		/* php's directory handles carry a mode and NO uri, and name their own` |
|      - | 1869 | ``		 * ops `dir` rather than the byte-stream STDIO. */`` |
|   1284 | 1870 | `		SetIOPrivateOpenedAs(pDev,0,0,"r",1);` |
|   1284 | 1871 | `		pDev->bDir = 1;` |
|      - | 1872 | `		/* Return the handle as a resource */` |
|   1284 | 1873 | `		ph7_result_resource(pCtx,pDev);` |
|      - | 1874 | `	}` |
|   1312 | 1875 | `	return PH7_OK;` |
|    659 | 1876 | `}` |
|      - | 1877 | `/*` |
|      - | 1878 | ``  * `dir(string $directory, $context = null): Directory\|false` `` |
|      - | 1879 | ` *` |
|      - | 1880 | ` * php's own dir() opens the stream and fills the object itself, which is why its` |
|      - | 1881 | ` * class needs no constructor. The open goes through the engine's opendir builtin` |
|      - | 1882 | `` * with THIS context, so the failure warning names `dir(...)` exactly as php's`` |
|      - | 1883 | ` * does; a failed open is FALSE, where the chunk's version handed back a Directory` |
|      - | 1884 | `` * whose handle was `false`.`` |
|      - | 1885 | ` */` |
|      4 | 1886 | `PH7_PRIVATE int PH7_builtin_dir(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 1887 | `{` |
|      - | 1888 | `	ph7_class_instance *pObj;` |
|      - | 1889 | `	ph7_class *pClass;` |
|      - | 1890 | `	ph7_value *pRet;` |
|      - | 1891 | `	int rc;` |
|      5 | 1892 | `	rc = PH7_builtin_opendir(pCtx,nArg,apArg);` |
|      5 | 1893 | `	if( rc != PH7_OK ){` |
|    ! 0 | 1894 | `		return rc;` |
|      - | 1895 | `	}` |
|      5 | 1896 | `	pRet = pCtx->pRet;` |
|      5 | 1897 | `	if( pRet == 0 \|\| (pRet->iFlags & MEMOBJ_RES) == 0 ){` |
|      3 | 1898 | `		ph7_result_bool(pCtx,0);` |
|      3 | 1899 | `		return PH7_OK;` |
|      - | 1900 | `	}` |
|      3 | 1901 | `	pClass = PH7_VmExtractClass(pCtx->pVm,"Directory",sizeof("Directory")-1,FALSE,0);` |
|      3 | 1902 | `	pObj = pClass ? PH7_NewClassInstance(pCtx->pVm,pClass) : 0;` |
|      3 | 1903 | `	if( pObj == 0 ){` |
|    ! 0 | 1904 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 1905 | `		return PH7_OK;` |
|      - | 1906 | `	}` |
|      - | 1907 | `	/* php's order: the path first, then the handle (var_dump shows both). */` |
|      - | 1908 | `	{` |
|      3 | 1909 | `		int nPath = 0;` |
|      3 | 1910 | `		const char *zPath = nArg > 0 ? ph7_value_to_string(apArg[0],&nPath) : "";` |
|      3 | 1911 | `		PH7_NativeSetAttrStr(pCtx->pVm,pObj,"path",zPath,nPath);` |
|      - | 1912 | `	}` |
|      3 | 1913 | `	PH7_NativeSetProp(pCtx->pVm,pObj,"handle",sizeof("handle")-1,pRet);` |
|      3 | 1914 | `	PH7_NativeResultObject(pCtx,pObj);` |
|      3 | 1915 | `	return PH7_OK;` |
|      3 | 1916 | `}` |
|      - | 1917 | `/*` |
|      - | 1918 | ` * int readfile(string $filename[,bool $use_include_path = false [,resource $context ]])` |
|      - | 1919 | ` *  Reads a file and writes it to the output buffer.` |
|      - | 1920 | ` * Parameters` |
|      - | 1921 | ` *  $filename` |
|      - | 1922 | ` *   The filename being read.` |
|      - | 1923 | ` *  $use_include_path` |
|      - | 1924 | ` *   You can use the optional second parameter and set it to` |
|      - | 1925 | ` *   TRUE, if you want to search for the file in the include_path, too.` |
|      - | 1926 | ` *  $context` |
|      - | 1927 | ` *   A context stream resource.` |
|      - | 1928 | ` * Return` |
|      - | 1929 | ` *  The number of bytes read from the file on success or FALSE on failure.` |
|      - | 1930 | ` */` |
|      - | 1931 | `/*` |
|      - | 1932 | ` * php's IO failures are E_WARNINGs naming the function, the path and the system reason:` |
|      - | 1933 | ` *   file_get_contents(/nope): Failed to open stream: No such file or directory` |
|      - | 1934 | ` * PH7 raised an E_ERROR reading "func(): IO error while opening '/nope'" for every one of` |
|      - | 1935 | ` * them -- wrong severity, wrong text, and no reason. errno still holds the failing` |
|      - | 1936 | ` * syscall's code at this point (a successful call never clears it), which is where the` |
|      - | 1937 | ` * trailing reason comes from.` |
|      - | 1938 | ` */` |
|      8 | 1939 | `PH7_PRIVATE int PH7_builtin_readfile(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      3 | 1940 | `{` |
|     11 | 1941 | `	int use_include  = FALSE;` |
|      - | 1942 | `	const ph7_io_stream *pStream;` |
|      - | 1943 | `	ph7_int64 n,nRead;` |
|      - | 1944 | `	const char *zFile;` |
|      - | 1945 | `	char zBuf[8192];` |
|      - | 1946 | `	void *pHandle;` |
|      - | 1947 | `	phl_stream_ctx *pCtxRes;` |
|     11 | 1948 | `	int rc,nLen,bThrew = 0;` |
|     11 | 1949 | `	if( nArg < 1 \|\| !ph7_value_is_string(apArg[0]) ){` |
|      - | 1950 | `		/* Missing/Invalid arguments,return FALSE */` |
|    ! 0 | 1951 | `		ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"Expecting a file path");` |
|    ! 0 | 1952 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 1953 | `		return PH7_OK;` |
|      - | 1954 | `	}` |
|      - | 1955 | `	/* Extract the file path */` |
|     11 | 1956 | `	zFile = ph7_value_to_string(apArg[0],&nLen);` |
|      - | 1957 | `	/* Point to the target IO stream device */` |
|     11 | 1958 | `	pStream = PH7_VmGetStreamDevice(pCtx->pVm,&zFile,nLen);` |
|     11 | 1959 | `	if( pStream == 0 ){` |
|    ! 0 | 1960 | `		VfsThrowNoDeviceWarning(pCtx,zFile,FALSE);` |
|    ! 0 | 1961 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 1962 | `		return PH7_OK;` |
|      - | 1963 | `	}` |
|     11 | 1964 | `	if( nArg > 1 ){` |
|      6 | 1965 | `		use_include = ph7_value_to_bool(apArg[1]);` |
|      2 | 1966 | `	}` |
|      - | 1967 | ``	/* php's `?resource $context`: a resource that is not a stream-context is`` |
|      - | 1968 | `	 * refused, and NULL means the DEFAULT context — never "no context at all".` |
|      - | 1969 | `	 * The armed one describes exactly this open. */` |
|     11 | 1970 | `	pCtxRes = PH7_StreamCtxFromArg(pCtx,nArg,apArg,2,"$context",0,&bThrew);` |
|     11 | 1971 | `	if( bThrew ){` |
|      6 | 1972 | `		return PH7_OK;` |
|      - | 1973 | `	}` |
|      6 | 1974 | `	PH7_StreamCtxArm(pCtx->pVm,pCtxRes);` |
|      - | 1975 | `	/* Try to open the file in read-only mode */` |
|      8 | 1976 | `	pHandle = PH7_StreamOpenHandle(pCtx->pVm,pStream,zFile,PH7_IO_OPEN_RDONLY,` |
|      2 | 1977 | `		use_include,nArg > 2 ? apArg[2] : 0,FALSE,0,ph7_function_name(pCtx));` |
|      6 | 1978 | `	if( pHandle == 0 ){` |
|      3 | 1979 | `		VfsThrowOpenWarning(pCtx,zFile);` |
|      3 | 1980 | `		ph7_result_bool(pCtx,0);` |
|      3 | 1981 | `		return PH7_OK;` |
|      - | 1982 | `	}` |
|      - | 1983 | `	/* Perform the requested operation */` |
|      3 | 1984 | `	nRead = 0;` |
|      2 | 1985 | `	for(;;){` |
|      5 | 1986 | `		n = pStream->xRead(pHandle,zBuf,sizeof(zBuf));` |
|      5 | 1987 | `		if( n < 1 ){` |
|      - | 1988 | `			/* EOF or IO error,break immediately */` |
|      3 | 1989 | `			break;` |
|      - | 1990 | `		}` |
|      - | 1991 | `		/* Output data */` |
|      3 | 1992 | `		rc = ph7_context_output(pCtx,zBuf,(int)n);` |
|      3 | 1993 | `		if( rc == PH7_ABORT ){` |
|    ! 0 | 1994 | `			break;` |
|      - | 1995 | `		}` |
|      - | 1996 | `		/* Increment counter */` |
|      3 | 1997 | `		nRead += n;` |
|      1 | 1998 | `	}` |
|      - | 1999 | `	/* Close the stream */` |
|      3 | 2000 | `	PH7_StreamCloseHandle(pStream,pHandle);` |
|      - | 2001 | `	/* Total number of bytes readen */` |
|      3 | 2002 | `	ph7_result_int64(pCtx,nRead);` |
|      3 | 2003 | `	return PH7_OK;` |
|      7 | 2004 | `}` |
|      - | 2005 | `/*` |
|      - | 2006 | ` * string file_get_contents(string $filename[,bool $use_include_path = false` |
|      - | 2007 | ` *         [, resource $context [, int $offset = -1 [, int $maxlen ]]]])` |
|      - | 2008 | ` *  Reads entire file into a string.` |
|      - | 2009 | ` * Parameters` |
|      - | 2010 | ` *  $filename` |
|      - | 2011 | ` *   The filename being read.` |
|      - | 2012 | ` *  $use_include_path` |
|      - | 2013 | ` *   You can use the optional second parameter and set it to` |
|      - | 2014 | ` *   TRUE, if you want to search for the file in the include_path, too.` |
|      - | 2015 | ` *  $context` |
|      - | 2016 | ` *   A context stream resource.` |
|      - | 2017 | ` *  $offset` |
|      - | 2018 | ` *   The offset where the reading starts on the original stream.` |
|      - | 2019 | ` *  $maxlen` |
|      - | 2020 | ` *    Maximum length of data read. The default is to read until end of file` |
|      - | 2021 | ` *    is reached. Note that this parameter is applied to the stream processed by the filters.` |
|      - | 2022 | ` * Return` |
|      - | 2023 | ` *   The function returns the read data or FALSE on failure.` |
|      - | 2024 | ` */` |
|   8340 | 2025 | `PH7_PRIVATE int PH7_builtin_file_get_contents(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      5 | 2026 | `{` |
|      - | 2027 | `	const ph7_io_stream *pStream;` |
|      - | 2028 | `	ph7_int64 n,nRead,nMaxlen;` |
|   8345 | 2029 | `	int use_include  = FALSE;` |
|      - | 2030 | `	const char *zFile;` |
|      - | 2031 | `	char zBuf[8192];` |
|      - | 2032 | `	void *pHandle;` |
|      - | 2033 | `	phl_stream_ctx *pCtxRes;` |
|   8345 | 2034 | `	int nLen,bThrew = 0;` |
|      - | 2035 |  |
|   8345 | 2036 | `	if( nArg < 1 \|\| !ph7_value_is_string(apArg[0]) ){` |
|      - | 2037 | `		/* Missing/Invalid arguments,return FALSE */` |
|    ! 0 | 2038 | `		ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"Expecting a file path");` |
|    ! 0 | 2039 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 2040 | `		return PH7_OK;` |
|      - | 2041 | `	}` |
|      - | 2042 | `	/* Extract the file path */` |
|   8345 | 2043 | `	zFile = ph7_value_to_string(apArg[0],&nLen);` |
|      - | 2044 | `	/* PHP 8 validates $length (arg #5) up front, before the wrapper is resolved` |
|      - | 2045 | `	 * or the file opened, so a negative length raises its catchable ValueError` |
|      - | 2046 | `	 * even for a bad wrapper or a missing file; a NULL (the ?int default) reads` |
|      - | 2047 | `	 * the whole file. */` |
|   8345 | 2048 | `	if( nArg > 4 && !ph7_value_is_null(apArg[4]) ){` |
|     27 | 2049 | `		if( ph7_value_to_int64(apArg[4]) < 0 ){` |
|      5 | 2050 | `			return PH7_VmThrowException(pCtx,"ValueError",` |
|      - | 2051 | `				"file_get_contents(): Argument #5 ($length) must be greater than or equal to 0");` |
|      - | 2052 | `		}` |
|     11 | 2053 | `	}` |
|      - | 2054 | `	/* Point to the target IO stream device */` |
|   8341 | 2055 | `	pStream = PH7_VmGetStreamDevice(pCtx->pVm,&zFile,nLen);` |
|   8341 | 2056 | `	if( pStream == 0 ){` |
|     23 | 2057 | `		VfsThrowNoDeviceWarning(pCtx,zFile,FALSE);` |
|     23 | 2058 | `		ph7_result_bool(pCtx,0);` |
|     23 | 2059 | `		return PH7_OK;` |
|      - | 2060 | `	}` |
|   8319 | 2061 | `	nMaxlen = -1;` |
|   8319 | 2062 | `	if( nArg > 1 ){` |
|     37 | 2063 | `		use_include = ph7_value_to_bool(apArg[1]);` |
|     17 | 2064 | `	}` |
|      - | 2065 | ``	/* php's `?resource $context`: a resource that is not a stream-context is`` |
|      - | 2066 | `	 * refused, and NULL means the DEFAULT context — never "no context at all".` |
|      - | 2067 | `	 * The armed one describes exactly this open. */` |
|   8319 | 2068 | `	pCtxRes = PH7_StreamCtxFromArg(pCtx,nArg,apArg,2,"$context",0,&bThrew);` |
|   8319 | 2069 | `	if( bThrew ){` |
|      5 | 2070 | `		return PH7_OK;` |
|      - | 2071 | `	}` |
|   8315 | 2072 | `	PH7_StreamCtxArm(pCtx->pVm,pCtxRes);` |
|      - | 2073 | `	/* Try to open the file in read-only mode */` |
|   8315 | 2074 | `	pHandle = PH7_StreamOpenHandle(pCtx->pVm,pStream,zFile,PH7_IO_OPEN_RDONLY,use_include,nArg > 2 ? apArg[2] : 0,FALSE,0,ph7_function_name(pCtx));` |
|   8315 | 2075 | `	if( pHandle == 0 ){` |
|     25 | 2076 | `		VfsThrowOpenWarning(pCtx,zFile);` |
|     25 | 2077 | `		ph7_result_bool(pCtx,0);` |
|     25 | 2078 | `		return PH7_OK;` |
|      - | 2079 | `	}` |
|   8293 | 2080 | `	if( nArg > 3 ){` |
|      - | 2081 | `		/* Extract the offset */` |
|     25 | 2082 | `		n = ph7_value_to_int64(apArg[3]);` |
|     25 | 2083 | `		if( n > 0 ){` |
|      7 | 2084 | `			if( pStream->xSeek ){` |
|      - | 2085 | `				/* Seek to the desired offset */` |
|      7 | 2086 | `				pStream->xSeek(pHandle,n,0/*SEEK_SET*/);` |
|      3 | 2087 | `			}` |
|      3 | 2088 | `		}` |
|     25 | 2089 | `		if( nArg > 4 && !ph7_value_is_null(apArg[4]) ){` |
|      - | 2090 | `			/* Maximum data to read. An explicit 0 reads nothing (php returns` |
|      - | 2091 | `			 * ""); only an omitted or NULL length keeps the -1 "whole file"` |
|      - | 2092 | `			 * sentinel, so NULL must not collapse to 0 here. */` |
|     23 | 2093 | `			nMaxlen = ph7_value_to_int64(apArg[4]);` |
|     11 | 2094 | `		}` |
|     12 | 2095 | `	}` |
|      - | 2096 | `	/* Perform the requested operation. nMaxlen: -1 = whole file, 0 = read` |
|      - | 2097 | `	 * nothing, >0 = at most that many bytes. The nMaxlen==0 case falls straight` |
|      - | 2098 | `	 * through to the empty-string result below. */` |
|   8293 | 2099 | `	nRead = 0;` |
|  16529 | 2100 | `	while( nMaxlen != 0 ){` |
|      - | 2101 | `		/* Cap the chunk to the bytes STILL wanted (nMaxlen - nRead), not to the` |
|      - | 2102 | `		 * whole nMaxlen: with a limit above the buffer size the final chunk would` |
|      - | 2103 | `		 * otherwise overshoot and append past $length. */` |
|  16525 | 2104 | `		ph7_int64 nAsk = (ph7_int64)sizeof(zBuf);` |
|  16525 | 2105 | `		if( nMaxlen > 0 && (nMaxlen - nRead) < nAsk ){` |
|     17 | 2106 | `			nAsk = nMaxlen - nRead;` |
|      8 | 2107 | `		}` |
|  16525 | 2108 | `		n = pStream->xRead(pHandle,zBuf,nAsk);` |
|  16525 | 2109 | `		if( n < 1 ){` |
|      - | 2110 | `			/* EOF or IO error,break immediately */` |
|   8275 | 2111 | `			break;` |
|      - | 2112 | `		}` |
|      - | 2113 | `		/* Append data */` |
|   8255 | 2114 | `		ph7_result_string(pCtx,zBuf,(int)n);` |
|      - | 2115 | `		/* Increment read counter */` |
|   8255 | 2116 | `		nRead += n;` |
|   8255 | 2117 | `		if( nMaxlen > 0 && nRead >= nMaxlen ){` |
|      - | 2118 | `			/* Read limit reached */` |
|     15 | 2119 | `			break;` |
|      - | 2120 | `		}` |
|      5 | 2121 | `	}` |
|      - | 2122 | `	/* Close the stream */` |
|   8293 | 2123 | `	PH7_StreamCloseHandle(pStream,pHandle);` |
|      - | 2124 | `	/* A successfully opened but empty file yields "" in php (FALSE is only for an` |
|      - | 2125 | `	 * open failure, handled above); the read loop never set a string result, so` |
|      - | 2126 | `	 * force an empty string rather than leaving a null/FALSE result. */` |
|   8293 | 2127 | `	if( ph7_context_result_buf_length(pCtx) < 1 ){` |
|     89 | 2128 | `		ph7_result_string(pCtx,"",0);` |
|     42 | 2129 | `	}` |
|   8293 | 2130 | `	return PH7_OK;` |
|   4175 | 2131 | `}` |
|      - | 2132 | `/*` |
|      - | 2133 | ` * int file_put_contents(string $filename,mixed $data[,int $flags = 0[,resource $context]])` |
|      - | 2134 | ` *  Write a string to a file.` |
|      - | 2135 | ` * Parameters` |
|      - | 2136 | ` *  $filename` |
|      - | 2137 | ` *  Path to the file where to write the data.` |
|      - | 2138 | ` * $data` |
|      - | 2139 | ` *  The data to write(Must be a string).` |
|      - | 2140 | ` * $flags` |
|      - | 2141 | ` *  The value of flags can be any combination of the following` |
|      - | 2142 | ` * flags, joined with the binary OR (\|) operator.` |
|      - | 2143 | ` *   FILE_USE_INCLUDE_PATH 	Search for filename in the include directory. See include_path for more information.` |
|      - | 2144 | ` *   FILE_APPEND 	        If file filename already exists, append the data to the file instead of overwriting it.` |
|      - | 2145 | ` *   LOCK_EX 	            Acquire an exclusive lock on the file while proceeding to the writing.` |
|      - | 2146 | ` * context` |
|      - | 2147 | ` *  A context stream resource.` |
|      - | 2148 | ` * Return` |
|      - | 2149 | ` *  The function returns the number of bytes that were written to the file, or FALSE on failure.` |
|      - | 2150 | ` */` |
|      - | 2151 | `/*` |
|      - | 2152 | ` * Append a buffer to a file, creating it when absent, and raise php's open` |
|      - | 2153 | `` * warning (`f(/nope): Failed to open stream: …`) under the CALLING builtin's`` |
|      - | 2154 | ` * name when it cannot be opened. Returns PH7_OK or -1.` |
|      - | 2155 | ` *` |
|      - | 2156 | ` * This is error_log()'s message_type 3, factored here because that is where the` |
|      - | 2157 | ` * stream device, the open flags and the warning shape already live.` |
|      - | 2158 | ` */` |
|      6 | 2159 | `PH7_PRIVATE int PH7_VfsAppendFile(ph7_context *pCtx,const char *zFile,const void *pData,int nLen)` |
|      1 | 2160 | `{` |
|      - | 2161 | `	const ph7_io_stream *pStream;` |
|      - | 2162 | `	void *pHandle;` |
|      - | 2163 | `	int nPath;` |
|      7 | 2164 | `	if( zFile == 0 \|\| zFile[0] == 0 ){` |
|    ! 0 | 2165 | `		VfsThrowOpenWarning(pCtx,zFile);` |
|    ! 0 | 2166 | `		return -1;` |
|      - | 2167 | `	}` |
|      7 | 2168 | `	nPath = (int)SyStrlen(zFile);` |
|      7 | 2169 | `	pStream = PH7_VmGetStreamDevice(pCtx->pVm,&zFile,nPath);` |
|      7 | 2170 | `	if( pStream == 0 \|\| pStream->xWrite == 0 ){` |
|    ! 0 | 2171 | `		VfsThrowOpenWarning(pCtx,zFile);` |
|    ! 0 | 2172 | `		return -1;` |
|      - | 2173 | `	}` |
|     10 | 2174 | `	pHandle = PH7_StreamOpenHandle(pCtx->pVm,pStream,zFile,` |
|      3 | 2175 | `		PH7_IO_OPEN_CREATE\|PH7_IO_OPEN_RDWR\|PH7_IO_OPEN_APPEND,FALSE,0,FALSE,0,ph7_function_name(pCtx));` |
|      7 | 2176 | `	if( pHandle == 0 ){` |
|      3 | 2177 | `		VfsThrowOpenWarning(pCtx,zFile);` |
|      3 | 2178 | `		return -1;` |
|      - | 2179 | `	}` |
|      5 | 2180 | `	if( nLen > 0 && pStream->xWrite(pHandle,pData,nLen) < 0 ){` |
|    ! 0 | 2181 | `		PH7_StreamCloseHandle(pStream,pHandle);` |
|    ! 0 | 2182 | `		return -1;` |
|      - | 2183 | `	}` |
|      5 | 2184 | `	PH7_StreamCloseHandle(pStream,pHandle);` |
|      5 | 2185 | `	return PH7_OK;` |
|      4 | 2186 | `}` |
|  15778 | 2187 | `PH7_PRIVATE int PH7_builtin_file_put_contents(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      5 | 2188 | `{` |
|  15783 | 2189 | `	int use_include  = FALSE;` |
|      - | 2190 | `	const ph7_io_stream *pStream;` |
|      - | 2191 | `	const char *zFile;` |
|      - | 2192 | `	const char *zData;` |
|      - | 2193 | `	int iOpenFlags;` |
|      - | 2194 | `	void *pHandle;` |
|      - | 2195 | `	phl_stream_ctx *pCtxRes;` |
|      - | 2196 | `	int iFlags;` |
|  15783 | 2197 | `	int nLen,bThrew = 0;` |
|      - | 2198 |  |
|  15783 | 2199 | `	if( nArg < 2 \|\| !ph7_value_is_string(apArg[0]) ){` |
|      - | 2200 | `		/* Missing/Invalid arguments,return FALSE */` |
|    ! 0 | 2201 | `		ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"Expecting a file path");` |
|    ! 0 | 2202 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 2203 | `		return PH7_OK;` |
|      - | 2204 | `	}` |
|      - | 2205 | `	/* Extract the file path */` |
|  15783 | 2206 | `	zFile = ph7_value_to_string(apArg[0],&nLen);` |
|      - | 2207 | `	/* Point to the target IO stream device */` |
|  15783 | 2208 | `	pStream = PH7_VmGetStreamDevice(pCtx->pVm,&zFile,nLen);` |
|  15783 | 2209 | `	if( pStream == 0 ){` |
|    ! 0 | 2210 | `		VfsThrowNoDeviceWarning(pCtx,zFile,FALSE);` |
|    ! 0 | 2211 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 2212 | `		return PH7_OK;` |
|      - | 2213 | `	}` |
|      - | 2214 | `	/* Data to write */` |
|  15783 | 2215 | `	zData = ph7_value_to_string(apArg[1],&nLen);` |
|      - | 2216 | `	/* Try to open the file in read-write mode */` |
|  15783 | 2217 | `	iOpenFlags = PH7_IO_OPEN_CREATE\|PH7_IO_OPEN_RDWR\|PH7_IO_OPEN_TRUNC;` |
|      - | 2218 | `	/* Extract the flags */` |
|  15783 | 2219 | `	iFlags = 0;` |
|  15783 | 2220 | `	if( nArg > 2 ){` |
|      9 | 2221 | `		iFlags = ph7_value_to_int(apArg[2]);` |
|      9 | 2222 | `		if( iFlags & 0x01 /*FILE_USE_INCLUDE_PATH*/){` |
|    ! 0 | 2223 | `			use_include = TRUE;` |
|    ! 0 | 2224 | `		}` |
|      9 | 2225 | `		if( iFlags & 0x08 /* FILE_APPEND */){` |
|      - | 2226 | `			/* If the file already exists, append the data to the file` |
|      - | 2227 | `			 * instead of overwriting it.` |
|      - | 2228 | `			 */` |
|    ! 0 | 2229 | `			iOpenFlags &= ~PH7_IO_OPEN_TRUNC;` |
|      - | 2230 | `			/* Append mode */` |
|    ! 0 | 2231 | `			iOpenFlags \|= PH7_IO_OPEN_APPEND;` |
|    ! 0 | 2232 | `		}` |
|      3 | 2233 | `	}` |
|      - | 2234 | `	/* FILE_NO_DEFAULT_CONTEXT is the flag that means exactly "and do NOT fall` |
|      - | 2235 | `	 * back to the default context" — which is why it needed one to exist. */` |
|  23672 | 2236 | `	pCtxRes = PH7_StreamCtxFromArg(pCtx,nArg,apArg,3,"$context",` |
|  15778 | 2237 | `		(iFlags & 0x10) != 0,&bThrew);` |
|  15783 | 2238 | `	if( bThrew ){` |
|      6 | 2239 | `		return PH7_OK;` |
|      - | 2240 | `	}` |
|  15779 | 2241 | `	PH7_StreamCtxArm(pCtx->pVm,pCtxRes);` |
|  23666 | 2242 | `	pHandle = PH7_StreamOpenHandle(pCtx->pVm,pStream,zFile,iOpenFlags,use_include,` |
|   7887 | 2243 | `		nArg > 3 ? apArg[3] : 0,FALSE,FALSE,ph7_function_name(pCtx));` |
|  15779 | 2244 | `	if( pHandle == 0 ){` |
|      6 | 2245 | `		VfsThrowOpenWarning(pCtx,zFile);` |
|      6 | 2246 | `		ph7_result_bool(pCtx,0);` |
|      6 | 2247 | `		return PH7_OK;` |
|      - | 2248 | `	}` |
|  15775 | 2249 | `	if( nLen < 1 ){` |
|      - | 2250 | `		/* Empty data, file is created/truncated */` |
|    109 | 2251 | `		ph7_result_int64(pCtx,0);` |
|    109 | 2252 | `		PH7_StreamCloseHandle(pStream,pHandle);` |
|    109 | 2253 | `		return PH7_OK;` |
|      - | 2254 | `	}` |
|  15671 | 2255 | `	if( pStream->xWrite ){` |
|      - | 2256 | `		ph7_int64 n;` |
|  15671 | 2257 | `		if( (iFlags & 2/* LOCK_EX, php's value */) && pStream->xLock ){` |
|      - | 2258 | `			/* Try to acquire an exclusive lock */` |
|    ! 0 | 2259 | `			pStream->xLock(pHandle,1/* LOCK_EX */);` |
|    ! 0 | 2260 | `		}` |
|      - | 2261 | `		/* Perform the write operation */` |
|  15671 | 2262 | `		n = pStream->xWrite(pHandle,(const void *)zData,nLen);` |
|  15671 | 2263 | `		if( n < 0 ){` |
|      - | 2264 | `			/* IO error,return FALSE — with php's write-failure diagnostic. */` |
|      1 | 2265 | `			ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,` |
|      - | 2266 | `				"Write of %d bytes failed with errno=%d %s",` |
|    ! 0 | 2267 | `				(int)nLen,errno,VfsStrerror(errno));` |
|      1 | 2268 | `			ph7_result_bool(pCtx,0);` |
|      1 | 2269 | `		}else{` |
|      - | 2270 | `			/* Total number of bytes written */` |
|  15671 | 2271 | `			ph7_result_int64(pCtx,n);` |
|      - | 2272 | `		}` |
|   7838 | 2273 | `	}else{` |
|      - | 2274 | `		/* Read-only stream */` |
|    ! 0 | 2275 | `		ph7_context_throw_error_format(pCtx,PH7_CTX_ERR,` |
|      - | 2276 | `			"Read-only stream(%s): Cannot perform write operation",` |
|    ! 0 | 2277 | `			pStream ? pStream->zName : "null_stream"` |
|      - | 2278 | `			);` |
|    ! 0 | 2279 | `		ph7_result_bool(pCtx,0);` |
|      - | 2280 | `	}` |
|      - | 2281 | `	/* Close the handle */` |
|  15671 | 2282 | `	PH7_StreamCloseHandle(pStream,pHandle);` |
|  15671 | 2283 | `	return PH7_OK;` |
|   7894 | 2284 | `}` |
|      - | 2285 | `/*` |
|      - | 2286 | ` * array file(string $filename[,int $flags = 0[,resource $context]])` |
|      - | 2287 | ` *  Reads entire file into an array.` |
|      - | 2288 | ` * Parameters` |
|      - | 2289 | ` *  $filename` |
|      - | 2290 | ` *   The filename being read.` |
|      - | 2291 | ` *  $flags` |
|      - | 2292 | ` *   The optional parameter flags can be one, or more, of the following constants:` |
|      - | 2293 | ` *   FILE_USE_INCLUDE_PATH` |
|      - | 2294 | ` *       Search for the file in the include_path.` |
|      - | 2295 | ` *   FILE_IGNORE_NEW_LINES` |
|      - | 2296 | ` *       Do not add newline at the end of each array element` |
|      - | 2297 | ` *   FILE_SKIP_EMPTY_LINES` |
|      - | 2298 | ` *       Skip empty lines` |
|      - | 2299 | ` *  $context` |
|      - | 2300 | ` *   A context stream resource.` |
|      - | 2301 | ` * Return` |
|      - | 2302 | ` *   The function returns the read data or FALSE on failure.` |
|      - | 2303 | ` */` |
|     50 | 2304 | `PH7_PRIVATE int PH7_builtin_file(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      4 | 2305 | `{` |
|      - | 2306 | `	const char *zFile,*zPtr,*zEnd,*zBuf;` |
|      - | 2307 | `	ph7_value *pArray,*pLine;` |
|      - | 2308 | `	const ph7_io_stream *pStream;` |
|     54 | 2309 | `	int use_include = 0;` |
|      - | 2310 | `	io_private *pDev;` |
|      - | 2311 | `	phl_stream_ctx *pCtxRes;` |
|      - | 2312 | `	ph7_int64 n;` |
|      - | 2313 | `	int iFlags;` |
|     54 | 2314 | `	int nLen,bThrew = 0;` |
|      - | 2315 |  |
|     54 | 2316 | `	if( nArg < 1 \|\| !ph7_value_is_string(apArg[0]) ){` |
|      - | 2317 | `		/* Missing/Invalid arguments,return FALSE */` |
|    ! 0 | 2318 | `		ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"Expecting a file path");` |
|    ! 0 | 2319 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 2320 | `		return PH7_OK;` |
|      - | 2321 | `	}` |
|     54 | 2322 | `	iFlags = 0;` |
|     54 | 2323 | `	if( nArg > 1 ){` |
|      - | 2324 | `		/* php validates the mask FIRST — before the wrapper is resolved and before` |
|      - | 2325 | `		 * anything is allocated, so file("bogus://x", 8) is the ValueError and not a` |
|      - | 2326 | `		 * stream warning, and the throw cannot strand the io_private below (its chunk` |
|      - | 2327 | `		 * is not auto-released). file() accepts only USE_INCLUDE_PATH\|IGNORE_NEW_LINES\|` |
|      - | 2328 | `		 * SKIP_EMPTY_LINES\|NO_DEFAULT_CONTEXT (1\|2\|4\|16) — FILE_APPEND belongs to` |
|      - | 2329 | `		 * file_put_contents and is rejected here like any other stray bit. PHL masked` |
|      - | 2330 | `		 * the bits it knew and silently ignored the rest, so file($p, 8) and` |
|      - | 2331 | `		 * file($p, -1) read the file with a flag combination the caller never asked` |
|      - | 2332 | `		 * for. Read at 64-bit width so a high bit cannot be truncated into a valid` |
|      - | 2333 | `		 * mask. */` |
|     42 | 2334 | `		ph7_int64 nFlags = ph7_value_to_int64(apArg[1]);` |
|     42 | 2335 | `		if( nFlags & ~(ph7_int64)(0x01\|0x02\|0x04\|0x10) ){` |
|     13 | 2336 | `			return PH7_VmThrowException(pCtx,"ValueError",` |
|      - | 2337 | `				"file(): Argument #2 ($flags) must be a valid flag value");` |
|      - | 2338 | `		}` |
|     30 | 2339 | `		iFlags = (int)nFlags;` |
|     14 | 2340 | `	}` |
|      - | 2341 | `	/* Resolved here for the same reason the flag mask is: a refused $context` |
|      - | 2342 | `	 * must not strand the io_private chunk allocated below.` |
|      - | 2343 | `	 * FILE_NO_DEFAULT_CONTEXT is the flag that means exactly "and do NOT fall` |
|      - | 2344 | `	 * back to the default context" — which is why it needed one to exist. */` |
|     61 | 2345 | `	pCtxRes = PH7_StreamCtxFromArg(pCtx,nArg,apArg,2,"$context",` |
|     38 | 2346 | `		(iFlags & 0x10) != 0,&bThrew);` |
|     42 | 2347 | `	if( bThrew ){` |
|      3 | 2348 | `		return PH7_OK;` |
|      - | 2349 | `	}` |
|      - | 2350 | `	/* Extract the file path */` |
|     40 | 2351 | `	zFile = ph7_value_to_string(apArg[0],&nLen);` |
|      - | 2352 | `	/* Point to the target IO stream device */` |
|     40 | 2353 | `	pStream = PH7_VmGetStreamDevice(pCtx->pVm,&zFile,nLen);` |
|     40 | 2354 | `	if( pStream == 0 ){` |
|    ! 0 | 2355 | `		VfsThrowNoDeviceWarning(pCtx,zFile,FALSE);` |
|    ! 0 | 2356 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 2357 | `		return PH7_OK;` |
|      - | 2358 | `	}` |
|      - | 2359 | `	/* Allocate a new IO private instance */` |
|     40 | 2360 | `	pDev = (io_private *)ph7_context_alloc_chunk(pCtx,sizeof(io_private),TRUE,FALSE);` |
|     40 | 2361 | `	if( pDev == 0 ){` |
|    ! 0 | 2362 | `		ph7_context_throw_error(pCtx,PH7_CTX_ERR,"PH7 is running out of memory");` |
|    ! 0 | 2363 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 2364 | `		return PH7_OK;` |
|      - | 2365 | `	}` |
|      - | 2366 | `	/* Initialize the structure */` |
|     40 | 2367 | `	InitIOPrivate(pCtx->pVm,pStream,pDev);` |
|     40 | 2368 | `	if( iFlags & 0x01 /*FILE_USE_INCLUDE_PATH*/ ){` |
|      3 | 2369 | `		use_include = TRUE;` |
|      1 | 2370 | `	}` |
|      - | 2371 | `	/* Create the array and the working value */` |
|     40 | 2372 | `	pArray = ph7_context_new_array(pCtx);` |
|     40 | 2373 | `	pLine = ph7_context_new_scalar(pCtx);` |
|     40 | 2374 | `	if( pArray == 0 \|\| pLine == 0 ){` |
|    ! 0 | 2375 | `		ph7_context_throw_error(pCtx,PH7_CTX_ERR,"PH7 is running out of memory");` |
|    ! 0 | 2376 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 2377 | `		return PH7_OK;` |
|      - | 2378 | `	}` |
|     40 | 2379 | `	PH7_StreamCtxArm(pCtx->pVm,pCtxRes);` |
|      - | 2380 | `	/* Try to open the file in read-only mode */` |
|     40 | 2381 | `	pDev->pHandle = PH7_StreamOpenHandle(pCtx->pVm,pStream,zFile,PH7_IO_OPEN_RDONLY,use_include,nArg > 2 ? apArg[2] : 0,FALSE,0,ph7_function_name(pCtx));` |
|     40 | 2382 | `	if( pDev->pHandle == 0 ){` |
|     10 | 2383 | `		VfsThrowOpenWarning(pCtx,zFile);` |
|     10 | 2384 | `		ph7_result_bool(pCtx,0);` |
|      - | 2385 | `		/* Don't worry about freeing memory, everything will be released automatically` |
|      - | 2386 | `		 * as soon we return from this function.` |
|      - | 2387 | `		 */` |
|     10 | 2388 | `		return PH7_OK;` |
|      - | 2389 | `	}` |
|      - | 2390 | `	/* Perform the requested operation */` |
|     61 | 2391 | `	for(;;){` |
|      - | 2392 | `		/* Try to extract a line */` |
|    128 | 2393 | `		n = StreamReadLine(pDev,&zBuf,-1);` |
|    128 | 2394 | `		if( n < 1 ){` |
|      - | 2395 | `			/* EOF or IO error */` |
|     30 | 2396 | `			break;` |
|      - | 2397 | `		}` |
|      - | 2398 | `		/* Reset the cursor */` |
|     99 | 2399 | `		ph7_value_reset_string_cursor(pLine);` |
|      - | 2400 | `		/* Remove line ending if requested by the caller */` |
|     99 | 2401 | `		zPtr = zBuf;` |
|     99 | 2402 | `		zEnd = &zBuf[n];` |
|     99 | 2403 | `		if( iFlags & 0x02 /* FILE_IGNORE_NEW_LINES */ ){` |
|      - | 2404 | `			/* php strips ONE line ending: the LF, plus the CR that immediately` |
|      - | 2405 | `			 * precedes it. Not a run — "a\r\r\n" keeps its first CR and a` |
|      - | 2406 | `			 * CR-TERMINATED last line ("a\r", no LF) keeps it entirely. The` |
|      - | 2407 | `			 * platform-gated version this replaces left the CR on every CRLF line` |
|      - | 2408 | `			 * read on POSIX; a strip-all loop would instead eat data php keeps. */` |
|     55 | 2409 | `			if( zEnd > zPtr && zEnd[-1] == '\n' ){` |
|     43 | 2410 | `				n--;` |
|     43 | 2411 | `				zEnd--;` |
|     43 | 2412 | `				if( zEnd > zPtr && zEnd[-1] == '\r' ){` |
|     13 | 2413 | `					n--;` |
|     13 | 2414 | `					zEnd--;` |
|      6 | 2415 | `				}` |
|     21 | 2416 | `			}` |
|     27 | 2417 | `		}` |
|     99 | 2418 | `		if( iFlags & 0x04 /* FILE_SKIP_EMPTY_LINES */ ){` |
|      - | 2419 | `			/* php's "empty" is ZERO LENGTH after the optional newline strip — not` |
|      - | 2420 | `			 * "blank". PHL skipped any all-whitespace line, so a line of spaces was` |
|      - | 2421 | `			 * dropped where php keeps it, and without IGNORE_NEW_LINES a bare "\n"` |
|      - | 2422 | `			 * line (never zero-length, since the newline is still attached) was` |
|      - | 2423 | `			 * dropped too. Both are silent data loss from a read. */` |
|     31 | 2424 | `			if( zEnd <= zPtr ){` |
|      5 | 2425 | `				continue;` |
|      - | 2426 | `			}` |
|     13 | 2427 | `		}` |
|     95 | 2428 | `		ph7_value_string(pLine,zBuf,(int)(zEnd-zBuf));` |
|      - | 2429 | `		/* Insert line */` |
|     95 | 2430 | `		ph7_array_add_elem(pArray,0/* Automatic index assign*/,pLine);` |
|      1 | 2431 | `	}` |
|      - | 2432 | `	/* Close the stream */` |
|     30 | 2433 | `	PH7_StreamCloseHandle(pStream,pDev->pHandle);` |
|      - | 2434 | `	/* Release the io_private instance */` |
|     30 | 2435 | `	ReleaseIOPrivate(pCtx,pDev);` |
|      - | 2436 | `	/* Return the created array */` |
|     30 | 2437 | `	ph7_result_value(pCtx,pArray);` |
|     30 | 2438 | `	return PH7_OK;` |
|     29 | 2439 | `}` |
|      - | 2440 | `/*` |
|      - | 2441 | ` * bool copy(string $source,string $dest[,resource $context ] )` |
|      - | 2442 | ` *  Makes a copy of the file source to dest.` |
|      - | 2443 | ` * Parameters` |
|      - | 2444 | ` *  $source` |
|      - | 2445 | ` *   Path to the source file.` |
|      - | 2446 | ` *  $dest` |
|      - | 2447 | ` *   The destination path. If dest is a URL, the copy operation` |
|      - | 2448 | ` *   may fail if the wrapper does not support overwriting of existing files.` |
|      - | 2449 | ` *  $context` |
|      - | 2450 | ` *   A context stream resource.` |
|      - | 2451 | ` * Return` |
|      - | 2452 | ` *  TRUE on success or FALSE on failure.` |
|      - | 2453 | ` */` |
|      6 | 2454 | `PH7_PRIVATE int PH7_builtin_copy(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      2 | 2455 | `{` |
|      - | 2456 | `	const ph7_io_stream *pSin,*pSout;` |
|      - | 2457 | `	const char *zFile;` |
|      - | 2458 | `	char zBuf[8192];` |
|      - | 2459 | `	void *pIn,*pOut;` |
|      - | 2460 | `	phl_stream_ctx *pCtxRes;` |
|      - | 2461 | `	ph7_int64 n;` |
|      8 | 2462 | `	int nLen,bThrew = 0;` |
|      8 | 2463 | `	if( nArg < 2 \|\| !ph7_value_is_string(apArg[0]) \|\| !ph7_value_is_string(apArg[1])){` |
|      - | 2464 | `		/* Missing/Invalid arguments,return FALSE */` |
|    ! 0 | 2465 | `		ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"Expecting a source and a destination path");` |
|    ! 0 | 2466 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 2467 | `		return PH7_OK;` |
|      - | 2468 | `	}` |
|      - | 2469 | `	/* Extract the source name */` |
|      8 | 2470 | `	zFile = ph7_value_to_string(apArg[0],&nLen);` |
|      - | 2471 | `	/* Point to the target IO stream device */` |
|      8 | 2472 | `	pSin = PH7_VmGetStreamDevice(pCtx->pVm,&zFile,nLen);` |
|      8 | 2473 | `	if( pSin == 0 ){` |
|    ! 0 | 2474 | `		VfsThrowNoDeviceWarning(pCtx,zFile,FALSE);` |
|    ! 0 | 2475 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 2476 | `		return PH7_OK;` |
|      - | 2477 | `	}` |
|      - | 2478 | ``	/* php's `?resource $context`: a resource that is not a stream-context is`` |
|      - | 2479 | `	 * refused, and NULL means the DEFAULT context — never "no context at all".` |
|      - | 2480 | `	 * The armed one describes exactly this open. */` |
|      8 | 2481 | `	pCtxRes = PH7_StreamCtxFromArg(pCtx,nArg,apArg,2,"$context",0,&bThrew);` |
|      8 | 2482 | `	if( bThrew ){` |
|      3 | 2483 | `		return PH7_OK;` |
|      - | 2484 | `	}` |
|      6 | 2485 | `	PH7_StreamCtxArm(pCtx->pVm,pCtxRes);` |
|      - | 2486 | `	/* Try to open the source file in a read-only mode */` |
|      6 | 2487 | `	pIn = PH7_StreamOpenHandle(pCtx->pVm,pSin,zFile,PH7_IO_OPEN_RDONLY,FALSE,nArg > 2 ? apArg[2] : 0,FALSE,0,ph7_function_name(pCtx));` |
|      6 | 2488 | `	if( pIn == 0 ){` |
|      3 | 2489 | `		VfsThrowOpenWarning(pCtx,zFile);` |
|      3 | 2490 | `		ph7_result_bool(pCtx,0);` |
|      3 | 2491 | `		return PH7_OK;` |
|      - | 2492 | `	}` |
|      - | 2493 | `	/* Extract the destination name */` |
|      3 | 2494 | `	zFile = ph7_value_to_string(apArg[1],&nLen);` |
|      - | 2495 | `	/* Point to the target IO stream device */` |
|      3 | 2496 | `	pSout = PH7_VmGetStreamDevice(pCtx->pVm,&zFile,nLen);` |
|      3 | 2497 | `	if( pSout == 0 ){` |
|    ! 0 | 2498 | `		VfsThrowNoDeviceWarning(pCtx,zFile,FALSE);` |
|    ! 0 | 2499 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 2500 | `		PH7_StreamCloseHandle(pSin,pIn);` |
|    ! 0 | 2501 | `		return PH7_OK;` |
|      - | 2502 | `	}` |
|      3 | 2503 | `	if( pSout->xWrite == 0 ){` |
|    ! 0 | 2504 | `		ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,` |
|      - | 2505 | `			"IO routine(%s) not implemented in the underlying stream(%s) device,PH7 is returning FALSE",` |
|    ! 0 | 2506 | `			ph7_function_name(pCtx),pSin->zName` |
|      - | 2507 | `			);` |
|    ! 0 | 2508 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 2509 | `		PH7_StreamCloseHandle(pSin,pIn);` |
|    ! 0 | 2510 | `		return PH7_OK;` |
|      - | 2511 | `	}` |
|      - | 2512 | `	/* php hands the ONE context to both halves of the copy. */` |
|      3 | 2513 | `	PH7_StreamCtxArm(pCtx->pVm,pCtxRes);` |
|      - | 2514 | `	/* Try to open the destination file in a read-write mode */` |
|      4 | 2515 | `	pOut = PH7_StreamOpenHandle(pCtx->pVm,pSout,zFile,` |
|      1 | 2516 | `		PH7_IO_OPEN_CREATE\|PH7_IO_OPEN_TRUNC\|PH7_IO_OPEN_RDWR,FALSE,nArg > 2 ? apArg[2] : 0,FALSE,0,ph7_function_name(pCtx));` |
|      3 | 2517 | `	if( pOut == 0 ){` |
|    ! 0 | 2518 | `		VfsThrowOpenWarning(pCtx,zFile);` |
|    ! 0 | 2519 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 2520 | `		PH7_StreamCloseHandle(pSin,pIn);` |
|    ! 0 | 2521 | `		return PH7_OK;` |
|      - | 2522 | `	}` |
|      - | 2523 | `	/* Perform the requested operation */` |
|      2 | 2524 | `	for(;;){` |
|      - | 2525 | `		/* Read from source */` |
|      5 | 2526 | `		n = pSin->xRead(pIn,zBuf,sizeof(zBuf));` |
|      5 | 2527 | `		if( n < 1 ){` |
|      - | 2528 | `			/* EOF or IO error,break immediately */` |
|      3 | 2529 | `			break;` |
|      - | 2530 | `		}` |
|      - | 2531 | `		/* Write to dest */` |
|      3 | 2532 | `		n = pSout->xWrite(pOut,zBuf,n);` |
|      3 | 2533 | `		if( n < 1 ){` |
|      - | 2534 | `			/* IO error,break immediately */` |
|    ! 0 | 2535 | `			break;` |
|      - | 2536 | `		}` |
|      1 | 2537 | `	}` |
|      - | 2538 | `	/* Close the streams */` |
|      3 | 2539 | `	PH7_StreamCloseHandle(pSin,pIn);` |
|      3 | 2540 | `	PH7_StreamCloseHandle(pSout,pOut);` |
|      - | 2541 | `	/* Return TRUE */` |
|      3 | 2542 | `	ph7_result_bool(pCtx,1);` |
|      3 | 2543 | `	return PH7_OK;` |
|      5 | 2544 | `}` |
|      - | 2545 | `/*` |
|      - | 2546 | ` * array fstat(resource $handle)` |
|      - | 2547 | ` *  Gets information about a file using an open file pointer.` |
|      - | 2548 | ` * Parameters` |
|      - | 2549 | ` *  $handle` |
|      - | 2550 | ` *   The file pointer.` |
|      - | 2551 | ` * Return` |
|      - | 2552 | ` *  Returns an array with the statistics of the file or FALSE on failure.` |
|      - | 2553 | ` */` |
|      6 | 2554 | `PH7_PRIVATE int PH7_builtin_fstat(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 2555 | `{` |
|      - | 2556 | `	ph7_value *pArray,*pValue;` |
|      - | 2557 | `	const ph7_io_stream *pStream;` |
|      - | 2558 | `	io_private *pDev;` |
|      7 | 2559 | `	if( nArg < 1 \|\| !ph7_value_is_resource(apArg[0]) ){` |
|      - | 2560 | `		/* Missing/Invalid arguments,return FALSE */` |
|    ! 0 | 2561 | `		ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"Expecting an IO handle");` |
|    ! 0 | 2562 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 2563 | `		return PH7_OK;` |
|      - | 2564 | `	}` |
|      - | 2565 | `	/* Extract our private data */` |
|      7 | 2566 | `	pDev = (io_private *)ph7_value_to_resource(apArg[0]);` |
|      - | 2567 | `	/* Make sure we are dealing with a valid io_private instance */` |
|      7 | 2568 | `	if( IO_PRIVATE_INVALID(pDev) ){` |
|      - | 2569 | `		/* Expecting an IO handle */` |
|    ! 0 | 2570 | `		ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"Expecting an IO handle");` |
|    ! 0 | 2571 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 2572 | `		return PH7_OK;` |
|      - | 2573 | `	}` |
|      - | 2574 | `	/* A php://filter handle is the stream underneath it, and that is the one` |
|      - | 2575 | `	 * with a stat to answer. */` |
|      7 | 2576 | `	pDev = PH7_StreamUnwrap(pDev);` |
|      - | 2577 | `	/* Point to the target IO stream device */` |
|      7 | 2578 | `	pStream = pDev->pStream;` |
|      7 | 2579 | `	if( pStream == 0  \|\| pStream->xStat == 0){` |
|    ! 0 | 2580 | `		ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,` |
|      - | 2581 | `			"IO routine(%s) not implemented in the underlying stream(%s) device,PH7 is returning FALSE",` |
|    ! 0 | 2582 | `			ph7_function_name(pCtx),pStream ? pStream->zName : "null_stream"` |
|      - | 2583 | `			);` |
|    ! 0 | 2584 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 2585 | `		return PH7_OK;` |
|      - | 2586 | `	}` |
|      - | 2587 | `	/* Create the array and the working value */` |
|      7 | 2588 | `	pArray = ph7_context_new_array(pCtx);` |
|      7 | 2589 | `	pValue = ph7_context_new_scalar(pCtx);` |
|      7 | 2590 | `	if( pArray == 0 \|\| pValue == 0 ){` |
|    ! 0 | 2591 | `		ph7_context_throw_error(pCtx,PH7_CTX_ERR,"PH7 is running out of memory");` |
|    ! 0 | 2592 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 2593 | `		return PH7_OK;` |
|      - | 2594 | `	}` |
|      - | 2595 | `	/* Perform the requested operation */` |
|      7 | 2596 | `	pStream->xStat(pDev->pHandle,pArray,pValue);` |
|      - | 2597 | `	/* php answers the same thirteen fields twice -- numeric 0..12, then named` |
|      - | 2598 | `	 * (PH7_VfsStatDoubleUp); fstat() is stat()'s answer for an open handle and` |
|      - | 2599 | `	 * had the same missing half. */` |
|      - | 2600 | `	{` |
|      7 | 2601 | `		ph7_value *pFull = ph7_context_new_array(pCtx);` |
|      7 | 2602 | `		if( pFull && PH7_VfsStatDoubleUp(pArray,pFull) == PH7_OK ){` |
|      7 | 2603 | `			ph7_result_value(pCtx,pFull);` |
|      7 | 2604 | `			return PH7_OK;` |
|      - | 2605 | `		}` |
|      - | 2606 | `	}` |
|      - | 2607 | `	/* Return the freshly created array */` |
|    ! 0 | 2608 | `	ph7_result_value(pCtx,pArray);` |
|      - | 2609 | `	/* Don't worry about freeing memory here,everything will be` |
|      - | 2610 | `	 * released automatically as soon we return from this function.` |
|      - | 2611 | `	 */` |
|    ! 0 | 2612 | `	return PH7_OK;` |
|      4 | 2613 | `}` |
|      - | 2614 | `/*` |
|      - | 2615 | ` * php's socket ops report a failed send THEMSELVES, as an E_NOTICE naming the` |
|      - | 2616 | ` * count, the errno and its text, before the caller ever sees the false — so a` |
|      - | 2617 | ` * write to a peer that has gone is diagnosed rather than silent. Defined with` |
|      - | 2618 | ` * the socket device further down; the write paths that can reach a socket call` |
|      - | 2619 | ` * it where php's own do.` |
|      - | 2620 | ` */` |
|      - | 2621 | `static void SockReportWriteFailure(ph7_context *pCtx,io_private *pDev,int nLen);` |
|      - | 2622 | `/*` |
|      - | 2623 | ` * int fwrite(resource $handle,string $string[,int $length])` |
|      - | 2624 | ` *  Writes the contents of string to the file stream pointed to by handle.` |
|      - | 2625 | ` * Parameters` |
|      - | 2626 | ` *  $handle` |
|      - | 2627 | ` *   The file pointer.` |
|      - | 2628 | ` *  $string` |
|      - | 2629 | ` *   The string that is to be written.` |
|      - | 2630 | ` *  $length` |
|      - | 2631 | ` *   If the length argument is given, writing will stop after length bytes have been written` |
|      - | 2632 | ` *   or the end of string is reached, whichever comes first.` |
|      - | 2633 | ` * Return` |
|      - | 2634 | ` *  Returns the number of bytes written, or FALSE on error.` |
|      - | 2635 | ` */` |
|    332 | 2636 | `PH7_PRIVATE int PH7_builtin_fwrite(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      5 | 2637 | `{` |
|      - | 2638 | `	const ph7_io_stream *pStream;` |
|      - | 2639 | `	const char *zString;` |
|      - | 2640 | `	io_private *pDev;` |
|      - | 2641 | `	int nLen,n;` |
|    337 | 2642 | `	if( nArg < 2 \|\| !ph7_value_is_resource(apArg[0]) ){` |
|      - | 2643 | `		/* Missing/Invalid arguments,return FALSE */` |
|    ! 0 | 2644 | `		ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"Expecting an IO handle");` |
|    ! 0 | 2645 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 2646 | `		return PH7_OK;` |
|      - | 2647 | `	}` |
|      - | 2648 | `	/* Extract our private data */` |
|    337 | 2649 | `	pDev = (io_private *)ph7_value_to_resource(apArg[0]);` |
|      - | 2650 | `	/* Make sure we are dealing with a valid io_private instance */` |
|    337 | 2651 | `	if( IO_PRIVATE_INVALID(pDev) ){` |
|      - | 2652 | `		/* Expecting an IO handle */` |
|    ! 0 | 2653 | `		ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"Expecting an IO handle");` |
|    ! 0 | 2654 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 2655 | `		return PH7_OK;` |
|      - | 2656 | `	}` |
|      - | 2657 | `	/* Point to the target IO stream device */` |
|    337 | 2658 | `	pStream = pDev->pStream;` |
|    337 | 2659 | `	if( pStream == 0  \|\| pStream->xWrite == 0){` |
|    ! 0 | 2660 | `		ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,` |
|      - | 2661 | `			"IO routine(%s) not implemented in the underlying stream(%s) device,PH7 is returning FALSE",` |
|    ! 0 | 2662 | `			ph7_function_name(pCtx),pStream ? pStream->zName : "null_stream"` |
|      - | 2663 | `			);` |
|    ! 0 | 2664 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 2665 | `		return PH7_OK;` |
|      - | 2666 | `	}` |
|      - | 2667 | `	/* Extract the data to write */` |
|    337 | 2668 | `	zString = ph7_value_to_string(apArg[1],&nLen);` |
|    337 | 2669 | `	if( nArg > 2 && !ph7_value_is_null(apArg[2]) ){` |
|      - | 2670 | `		/* Maximum data length to write, read at 64-bit width so a large limit` |
|      - | 2671 | `		 * (PHP_INT_MAX as "no limit" is a common idiom) does not truncate to a` |
|      - | 2672 | `		 * negative int. php 8: NULL means "no limit" and a NEGATIVE $length` |
|      - | 2673 | `		 * writes NOTHING and returns 0 (probed; PHL used to ignore a negative` |
|      - | 2674 | `		 * and write the whole string). */` |
|     16 | 2675 | `		sxi64 nMax = ph7_value_to_int64(apArg[2]);` |
|     16 | 2676 | `		if( nMax < 0 ){` |
|      3 | 2677 | `			nLen = 0;` |
|     15 | 2678 | `		}else if( nMax < (sxi64)nLen ){` |
|      8 | 2679 | `			nLen = (int)nMax;` |
|      3 | 2680 | `		}` |
|      7 | 2681 | `	}` |
|    337 | 2682 | `	if( nLen < 1 ){` |
|      - | 2683 | `		/* Nothing to write */` |
|      5 | 2684 | `		ph7_result_int(pCtx,0);` |
|      5 | 2685 | `		return PH7_OK;` |
|      - | 2686 | `	}` |
|      - | 2687 | `	/* The device sits PAST what the readers pulled ahead: php writes at the` |
|      - | 2688 | `	 * LOGICAL position (fgets() then fwrite() overwrites what fgets left` |
|      - | 2689 | `	 * unread) — the ftell()/SEEK_CUR rule, applied to the write. */` |
|    333 | 2690 | `	StreamSeekBackForWrite(pDev);` |
|      - | 2691 | `	/* Perform the requested operation */` |
|    333 | 2692 | `	n = (int)PH7_StreamWrite(pDev,(const void *)zString,nLen);` |
|    333 | 2693 | `	if( n <  0 ){` |
|      - | 2694 | `		/* IO error,return FALSE */` |
|      8 | 2695 | `		SockReportWriteFailure(pCtx,pDev,nLen);` |
|      8 | 2696 | `		ph7_result_bool(pCtx,0);` |
|      5 | 2697 | `	}else{` |
|      - | 2698 | `		/* #Bytes written */` |
|    327 | 2699 | `		ph7_result_int(pCtx,n);` |
|      - | 2700 | `	}` |
|    333 | 2701 | `	return PH7_OK;` |
|    169 | 2702 | `}` |
|      - | 2703 | `/*` |
|      - | 2704 | ` * Write flock()'s optional by-reference &$would_block out-param. php writes it on` |
|      - | 2705 | ` * every call that does not throw — including the ones that answer FALSE — so a` |
|      - | 2706 | ` * script can tell contention (1) from a plain failure (0).` |
|      - | 2707 | ` */` |
|     34 | 2708 | `static void FlockStoreWouldBlock(ph7_context *pCtx,int nArg,ph7_value **apArg,int bWouldBlock)` |
|      1 | 2709 | `{` |
|      - | 2710 | `	ph7_value sVal;` |
|     35 | 2711 | `	if( nArg < 3 ){` |
|     23 | 2712 | `		return;` |
|      - | 2713 | `	}` |
|     13 | 2714 | `	PH7_MemObjInitFromInt(pCtx->pVm,&sVal,bWouldBlock ? 1 : 0);` |
|     13 | 2715 | `	PH7_VmStoreArgByRef(pCtx->pVm,apArg[2],&sVal);` |
|     13 | 2716 | `	PH7_MemObjRelease(&sVal);` |
|     18 | 2717 | `}` |
|      - | 2718 | `/*` |
|      - | 2719 | ` * bool flock(resource $handle,int $operation[,int &$would_block])` |
|      - | 2720 | ` *  Portable advisory file locking.` |
|      - | 2721 | ` * Parameters` |
|      - | 2722 | ` *  $handle` |
|      - | 2723 | ` *   The file pointer.` |
|      - | 2724 | ` *  $operation` |
|      - | 2725 | ` *   operation is one of the following:` |
|      - | 2726 | ` *      LOCK_SH to acquire a shared lock (reader).` |
|      - | 2727 | ` *      LOCK_EX to acquire an exclusive lock (writer).` |
|      - | 2728 | ` *      LOCK_UN to release a lock (shared or exclusive).` |
|      - | 2729 | ` *   optionally OR'd with LOCK_NB to fail immediately instead of waiting.` |
|      - | 2730 | ` *  &$would_block` |
|      - | 2731 | ` *   Set to 1 when a LOCK_NB request was refused because another holder has the` |
|      - | 2732 | ` *   file, 0 otherwise. php writes it on every call that does not throw.` |
|      - | 2733 | ` * Return` |
|      - | 2734 | ` *  Returns TRUE on success or FALSE on failure.` |
|      - | 2735 | ` */` |
|     42 | 2736 | `PH7_PRIVATE int PH7_builtin_flock(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 2737 | `{` |
|      - | 2738 | `	const ph7_io_stream *pStream;` |
|      - | 2739 | `	io_private *pDev;` |
|      - | 2740 | `	int nLock;` |
|      - | 2741 | `	int rc;` |
|     43 | 2742 | `	if( nArg < 2 \|\| !ph7_value_is_resource(apArg[0]) ){` |
|      - | 2743 | `		/* Missing/Invalid arguments,return FALSE */` |
|    ! 0 | 2744 | `		ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"Expecting an IO handle");` |
|    ! 0 | 2745 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 2746 | `		return PH7_OK;` |
|      - | 2747 | `	}` |
|      - | 2748 | `	/* Extract our private data */` |
|     43 | 2749 | `	pDev = (io_private *)ph7_value_to_resource(apArg[0]);` |
|      - | 2750 | `	/* Make sure we are dealing with a valid io_private instance */` |
|     43 | 2751 | `	if( IO_PRIVATE_INVALID(pDev) ){` |
|      - | 2752 | `		/*Expecting an IO handle */` |
|    ! 0 | 2753 | `		ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"Expecting an IO handle");` |
|    ! 0 | 2754 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 2755 | `		return PH7_OK;` |
|      - | 2756 | `	}` |
|      - | 2757 | `	/* Requested lock operation. php 8 validates it BEFORE the stream's lock` |
|      - | 2758 | `	 * support is considered: the low two bits select the action (its bison` |
|      - | 2759 | `	 * table is act = operation & 3), 0 is invalid, and every higher bit except` |
|      - | 2760 | `	 * LOCK_NB is ignored (flock($f,99) is LOCK_UN in php). */` |
|     43 | 2761 | `	nLock = ph7_value_to_int(apArg[1]);` |
|     43 | 2762 | `	if( (nLock & 3) == 0 ){` |
|      9 | 2763 | `		return PH7_VmThrowException(pCtx,"ValueError",` |
|      - | 2764 | `			"flock(): Argument #2 ($operation) must be one of LOCK_SH, LOCK_EX, or LOCK_UN");` |
|      - | 2765 | `	}` |
|     35 | 2766 | `	pDev = PH7_StreamUnwrap(pDev);` |
|      - | 2767 | `	/* Point to the target IO stream device */` |
|     35 | 2768 | `	pStream = pDev->pStream;` |
|     35 | 2769 | `	if( pStream == 0  \|\| pStream->xLock == 0){` |
|      - | 2770 | `		/* php returns FALSE silently when the stream does not support locking` |
|      - | 2771 | `		 * (php://memory & co) — no warning. It still writes $would_block. */` |
|      7 | 2772 | `		FlockStoreWouldBlock(pCtx,nArg,apArg,0);` |
|      7 | 2773 | `		ph7_result_bool(pCtx,0);` |
|      7 | 2774 | `		return PH7_OK;` |
|      - | 2775 | `	}` |
|      - | 2776 | `	/*` |
|      - | 2777 | `	 * Translate php's operation (LOCK_SH=1, LOCK_EX=2, LOCK_UN=3, optionally` |
|      - | 2778 | `	 * \|LOCK_NB=4) into the xLock() vtable value space documented in ph7.h.` |
|      - | 2779 | `	 */` |
|      - | 2780 | `	{` |
|     29 | 2781 | `		int iOp = nLock & 3;` |
|     29 | 2782 | `		int bNoBlock = (nLock & 4 /* LOCK_NB */) != 0;` |
|     29 | 2783 | `		if( iOp == 3 /* LOCK_UN */ ){` |
|     11 | 2784 | `			nLock = -1;` |
|     24 | 2785 | `		}else if( iOp == 2 /* LOCK_EX */ ){` |
|     11 | 2786 | `			nLock = bNoBlock ? PH7_IO_LOCK_EX_NB : PH7_IO_LOCK_EX;` |
|      6 | 2787 | `		}else{` |
|      9 | 2788 | `			nLock = bNoBlock ? PH7_IO_LOCK_SH_NB : PH7_IO_LOCK_SH;` |
|      - | 2789 | `		}` |
|      - | 2790 | `	}` |
|      - | 2791 | `	/* Lock operation */` |
|     29 | 2792 | `	rc = pStream->xLock(pDev->pHandle,nLock);` |
|      - | 2793 | `	/* A refused non-blocking request is php's $would_block: FALSE, and the` |
|      - | 2794 | `	 * out-param tells the script it was contention rather than an IO error. */` |
|     29 | 2795 | `	FlockStoreWouldBlock(pCtx,nArg,apArg,rc == SXERR_BUSY);` |
|      - | 2796 | `	/* IO result */` |
|     29 | 2797 | `	ph7_result_bool(pCtx,rc == PH7_OK);` |
|     29 | 2798 | `	return PH7_OK;` |
|     22 | 2799 | `}` |
|      - | 2800 | `/*` |
|      - | 2801 | ` * int fpassthru(resource $handle)` |
|      - | 2802 | ` *  Output all remaining data on a file pointer.` |
|      - | 2803 | ` * Parameters` |
|      - | 2804 | ` *  $handle` |
|      - | 2805 | ` *   The file pointer.` |
|      - | 2806 | ` * Return` |
|      - | 2807 | ` *  Total number of characters read from handle and passed through` |
|      - | 2808 | ` *  to the output on success or FALSE on failure.` |
|      - | 2809 | ` */` |
|      4 | 2810 | `PH7_PRIVATE int PH7_builtin_fpassthru(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 2811 | `{` |
|      - | 2812 | `	const ph7_io_stream *pStream;` |
|      - | 2813 | `	io_private *pDev;` |
|      - | 2814 | `	ph7_int64 n,nRead;` |
|      - | 2815 | `	char zBuf[8192];` |
|      - | 2816 | `	int rc;` |
|      5 | 2817 | `	if( nArg < 1 \|\| !ph7_value_is_resource(apArg[0]) ){` |
|      - | 2818 | `		/* Missing/Invalid arguments,return FALSE */` |
|    ! 0 | 2819 | `		ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"Expecting an IO handle");` |
|    ! 0 | 2820 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 2821 | `		return PH7_OK;` |
|      - | 2822 | `	}` |
|      - | 2823 | `	/* Extract our private data */` |
|      5 | 2824 | `	pDev = (io_private *)ph7_value_to_resource(apArg[0]);` |
|      - | 2825 | `	/* Make sure we are dealing with a valid io_private instance */` |
|      5 | 2826 | `	if( IO_PRIVATE_INVALID(pDev) ){` |
|      - | 2827 | `		/*Expecting an IO handle */` |
|    ! 0 | 2828 | `		ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"Expecting an IO handle");` |
|    ! 0 | 2829 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 2830 | `		return PH7_OK;` |
|      - | 2831 | `	}` |
|      - | 2832 | `	/* Point to the target IO stream device */` |
|      5 | 2833 | `	pStream = pDev->pStream;` |
|      5 | 2834 | `	if( pStream == 0  ){` |
|    ! 0 | 2835 | `		ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,` |
|      - | 2836 | `			"IO routine(%s) not implemented in the underlying stream(%s) device,PH7 is returning FALSE",` |
|    ! 0 | 2837 | `			ph7_function_name(pCtx),pStream ? pStream->zName : "null_stream"` |
|      - | 2838 | `			);` |
|    ! 0 | 2839 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 2840 | `		return PH7_OK;` |
|      - | 2841 | `	}` |
|      - | 2842 | `	/* Perform the requested operation */` |
|      5 | 2843 | `	nRead = 0;` |
|      5 | 2844 | `	for(;;){` |
|     11 | 2845 | `		n = PH7_StreamRead(pDev,zBuf,sizeof(zBuf));` |
|     11 | 2846 | `		if( n < 1 ){` |
|      - | 2847 | `			/* Error or EOF */` |
|      5 | 2848 | `			break;` |
|      - | 2849 | `		}` |
|      - | 2850 | `		/* Increment the read counter */` |
|      7 | 2851 | `		nRead += n;` |
|      - | 2852 | `		/* Output the bytes THIS read produced. Handing the running total to` |
|      - | 2853 | `		 * ph7_context_output() instead read past the end of zBuf from the second` |
|      - | 2854 | `		 * chunk on (an out-of-bounds read) and wrote the overrun to the output:` |
|      - | 2855 | `		 * a 12000-byte file passed through as 20189 bytes of file-plus-garbage. */` |
|      7 | 2856 | `		rc = ph7_context_output(pCtx,zBuf,(int)n);` |
|      7 | 2857 | `		if( rc == PH7_ABORT ){` |
|      - | 2858 | `			/* Consumer callback request an operation abort */` |
|    ! 0 | 2859 | `			break;` |
|      - | 2860 | `		}` |
|      1 | 2861 | `	}` |
|      - | 2862 | `	/* Total number of bytes readen */` |
|      5 | 2863 | `	ph7_result_int64(pCtx,nRead);` |
|      5 | 2864 | `	return PH7_OK;` |
|      3 | 2865 | `}` |
|      - | 2866 | `/* CSV writer private data */` |
|      - | 2867 | `struct csv_data` |
|      - | 2868 | `{` |
|      - | 2869 | `	int delimiter;     /* Delimiter. Default ',' */` |
|      - | 2870 | `	int enclosure;     /* Enclosure. Default '"' */` |
|      - | 2871 | `	int escape;        /* Escape, or PH7_CSV_NO_ESCAPE when "" disabled it */` |
|      - | 2872 | `	SyBlob *pLine;     /* The line being built */` |
|      - | 2873 | `	sxu32 nCount;      /* Fields still to write after this one */` |
|      - | 2874 | `};` |
|      - | 2875 | `/*` |
|      - | 2876 | ` * The following callback is used by fputcsv() to walk the $fields array and` |
|      - | 2877 | ` * append each entry to the line under construction. It is a port of php's own` |
|      - | 2878 | ` * php_fputcsv (ext/standard/file.c), and the parts a re-derivation gets wrong` |
|      - | 2879 | ` * are all here:` |
|      - | 2880 | ` *  - WHICH fields are enclosed. php quotes a field containing the delimiter,` |
|      - | 2881 | ` *    the enclosure, the escape (when one is enabled) or any of \n, \r, \t and` |
|      - | 2882 | ` *    SPACE. PH7 tested the first two only, so a field with an embedded newline` |
|      - | 2883 | ` *    was written raw and became two CSV ROWS on the way back in.` |
|      - | 2884 | ` *  - HOW an embedded enclosure is written: doubled, unless the escape character` |
|      - | 2885 | ` *    came immediately before it (then the pair is passed through as-is and the` |
|      - | 2886 | ` *    escape does NOT arm again for the byte after).` |
|      - | 2887 | ` *  - that an EMPTY field is still a field. PH7 returned early for a zero-length` |
|      - | 2888 | `` *    value and skipped its delimiter with it, so `['', 'a']` wrote "a" -- one`` |
|      - | 2889 | ` *    column where the caller wrote two, silently shifting every later column.` |
|      - | 2890 | ` * The delimiter goes BETWEEN fields, so it is written from the remaining count` |
|      - | 2891 | ` * rather than from a "not the first" flag: php appends it after every field but` |
|      - | 2892 | ` * the last, and an empty first field must still be followed by one.` |
|      - | 2893 | ` */` |
|    124 | 2894 | `static int csv_write_callback(ph7_value *pKey,ph7_value *pValue,void *pUserData)` |
|      1 | 2895 | `{` |
|    125 | 2896 | `	struct csv_data *pData = (struct csv_data *)pUserData;` |
|      - | 2897 | `	const char *zData;` |
|      - | 2898 | `	int nLen,i;` |
|    125 | 2899 | `	int bEnclose = 0;` |
|     62 | 2900 | `	SXUNUSED(pKey); /* cc warning */` |
|    125 | 2901 | `	zData = ph7_value_to_string(pValue,&nLen);` |
|    237 | 2902 | `	for( i = 0 ; i < nLen ; ++i ){` |
|    159 | 2903 | `		int c = (unsigned char)zData[i];` |
|    158 | 2904 | `		if( c == pData->delimiter \|\| c == pData->enclosure` |
|    141 | 2905 | `		 \|\| (pData->escape != PH7_CSV_NO_ESCAPE && c == pData->escape)` |
|    131 | 2906 | `		 \|\| c == '\n' \|\| c == '\r' \|\| c == '\t' \|\| c == ' ' ){` |
|     47 | 2907 | `			bEnclose = 1;` |
|     47 | 2908 | `			break;` |
|      - | 2909 | `		}` |
|     57 | 2910 | `	}` |
|    125 | 2911 | `	if( bEnclose ){` |
|     47 | 2912 | `		char cEnc = (char)pData->enclosure;` |
|     47 | 2913 | `		int bEscaped = 0;` |
|     47 | 2914 | `		SyBlobAppend(pData->pLine,(const void *)&cEnc,sizeof(char));` |
|    189 | 2915 | `		for( i = 0 ; i < nLen ; ++i ){` |
|    143 | 2916 | `			char c = zData[i];` |
|    143 | 2917 | `			if( pData->escape != PH7_CSV_NO_ESCAPE && (unsigned char)c == pData->escape ){` |
|      9 | 2918 | `				bEscaped = 1;` |
|    139 | 2919 | `			}else if( !bEscaped && (unsigned char)c == pData->enclosure ){` |
|     21 | 2920 | `				SyBlobAppend(pData->pLine,(const void *)&cEnc,sizeof(char));` |
|     11 | 2921 | `			}else{` |
|    115 | 2922 | `				bEscaped = 0;` |
|      - | 2923 | `			}` |
|    143 | 2924 | `			SyBlobAppend(pData->pLine,(const void *)&c,sizeof(char));` |
|     72 | 2925 | `		}` |
|     47 | 2926 | `		SyBlobAppend(pData->pLine,(const void *)&cEnc,sizeof(char));` |
|    102 | 2927 | `	}else if( nLen > 0 ){` |
|     59 | 2928 | `		SyBlobAppend(pData->pLine,(const void *)zData,(sxu32)nLen);` |
|     29 | 2929 | `	}` |
|    125 | 2930 | `	if( pData->nCount > 0 ){` |
|    125 | 2931 | `		pData->nCount--;` |
|     62 | 2932 | `	}` |
|    125 | 2933 | `	if( pData->nCount > 0 ){` |
|     47 | 2934 | `		char cDel = (char)pData->delimiter;` |
|     47 | 2935 | `		SyBlobAppend(pData->pLine,(const void *)&cDel,sizeof(char));` |
|     23 | 2936 | `	}` |
|    125 | 2937 | `	return PH7_OK;` |
|      1 | 2938 | `}` |
|      - | 2939 | `/*` |
|      - | 2940 | ` * int\|false fputcsv(resource $stream, array $fields, string $separator = ',',` |
|      - | 2941 | ` *                   string $enclosure = '"', string $escape = '\\',` |
|      - | 2942 | ` *                   string $eol = "\n")` |
|      - | 2943 | ` *  Format line as CSV and write to file pointer.` |
|      - | 2944 | ` * Parameters` |
|      - | 2945 | ` *  $stream` |
|      - | 2946 | ` *   Open file handle.` |
|      - | 2947 | ` *  $fields` |
|      - | 2948 | ` *   An array of values.` |
|      - | 2949 | ` *  $separator` |
|      - | 2950 | ` *   The optional separator parameter sets the field delimiter (one character only).` |
|      - | 2951 | ` *  $enclosure` |
|      - | 2952 | ` *   The optional enclosure parameter sets the field enclosure (one character only).` |
|      - | 2953 | ` *  $escape` |
|      - | 2954 | ` *   The escape character (one character), or "" to disable escaping entirely.` |
|      - | 2955 | ` *  $eol` |
|      - | 2956 | ` *   php 8.1's line ending. It is "\n" on EVERY platform -- php does not follow` |
|      - | 2957 | ` *   the host's convention here, and PHL used to write CRLF on Windows, so the` |
|      - | 2958 | ` *   same program produced a different FILE depending on where it ran.` |
|      - | 2959 | ` * Return` |
|      - | 2960 | ` *  The number of bytes written, or FALSE when the write fails. The count was` |
|      - | 2961 | ` *  missing entirely (the call answered NULL), so the documented` |
|      - | 2962 | `` *  `if (fputcsv(...) === false)` check never fired and a caller totalling the`` |
|      - | 2963 | ` *  bytes it wrote added nothing.` |
|      - | 2964 | ` */` |
|     92 | 2965 | `PH7_PRIVATE int PH7_builtin_fputcsv(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 2966 | `{` |
|      - | 2967 | `	const ph7_io_stream *pStream;` |
|      - | 2968 | `	struct csv_data sCsv;` |
|      - | 2969 | `	io_private *pDev;` |
|      - | 2970 | `	SyBlob sLine;` |
|     93 | 2971 | `	const char *zEol = "\n";` |
|     93 | 2972 | `	int nEol = 1;` |
|      - | 2973 | `	ph7_int64 nWr;` |
|     93 | 2974 | `	if( nArg < 2 \|\| !ph7_value_is_resource(apArg[0]) \|\| !ph7_value_is_array(apArg[1]) ){` |
|      - | 2975 | `		/* Missing/Invalid arguments,return FALSE */` |
|    ! 0 | 2976 | `		ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"Missing/Invalid arguments");` |
|    ! 0 | 2977 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 2978 | `		return PH7_OK;` |
|      - | 2979 | `	}` |
|      - | 2980 | `	/* Extract our private data */` |
|     93 | 2981 | `	pDev = (io_private *)ph7_value_to_resource(apArg[0]);` |
|      - | 2982 | `	/* Make sure we are dealing with a valid io_private instance */` |
|     93 | 2983 | `	if( IO_PRIVATE_INVALID(pDev) ){` |
|      - | 2984 | `		/*Expecting an IO handle */` |
|    ! 0 | 2985 | `		ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"Expecting an IO handle");` |
|    ! 0 | 2986 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 2987 | `		return PH7_OK;` |
|      - | 2988 | `	}` |
|      - | 2989 | `	/* Point to the target IO stream device */` |
|     93 | 2990 | `	pStream = pDev->pStream;` |
|     93 | 2991 | `	if( pStream == 0  \|\| pStream->xWrite == 0){` |
|    ! 0 | 2992 | `		ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,` |
|      - | 2993 | `			"IO routine(%s) not implemented in the underlying stream(%s) device,PH7 is returning FALSE",` |
|    ! 0 | 2994 | `			ph7_function_name(pCtx),pStream ? pStream->zName : "null_stream"` |
|      - | 2995 | `			);` |
|    ! 0 | 2996 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 2997 | `		return PH7_OK;` |
|      - | 2998 | `	}` |
|      - | 2999 | `	/* Set default csv separator */` |
|     93 | 3000 | `	sCsv.delimiter = ',';` |
|     93 | 3001 | `	sCsv.enclosure = '"';` |
|     93 | 3002 | `	sCsv.escape = '\\';` |
|     93 | 3003 | `	if( nArg > 2 ){` |
|     93 | 3004 | `		sxi32 rc = PH7_CsvCharArg(pCtx,apArg[2],3,"separator",0,&sCsv.delimiter);` |
|     93 | 3005 | `		if( rc != PH7_OK ){` |
|      5 | 3006 | `			return rc;` |
|      - | 3007 | `		}` |
|     89 | 3008 | `		if( nArg > 3 ){` |
|     89 | 3009 | `			rc = PH7_CsvCharArg(pCtx,apArg[3],4,"enclosure",0,&sCsv.enclosure);` |
|     89 | 3010 | `			if( rc != PH7_OK ){` |
|      5 | 3011 | `				return rc;` |
|      - | 3012 | `			}` |
|     85 | 3013 | `			if( nArg > 4 ){` |
|     85 | 3014 | `				rc = PH7_CsvCharArg(pCtx,apArg[4],5,"escape",1,&sCsv.escape);` |
|     85 | 3015 | `				if( rc != PH7_OK ){` |
|      5 | 3016 | `					return rc;` |
|      - | 3017 | `				}` |
|     81 | 3018 | `				if( nArg > 5 ){` |
|      - | 3019 | `					/* $eol takes ANY string, the empty one included -- it is not` |
|      - | 3020 | `					 * a single-character argument like the three above. */` |
|     73 | 3021 | `					zEol = ph7_value_to_string(apArg[5],&nEol);` |
|     36 | 3022 | `				}` |
|     40 | 3023 | `			}` |
|     40 | 3024 | `		}` |
|     40 | 3025 | `	}` |
|      - | 3026 | `	/* php builds the whole line first and writes it ONCE, which is what makes the` |
|      - | 3027 | `	 * byte count meaningful and keeps a partly-written row off the stream. */` |
|     81 | 3028 | `	SyBlobInit(&sLine,&pCtx->pVm->sAllocator);` |
|     81 | 3029 | `	sCsv.pLine = &sLine;` |
|     81 | 3030 | `	sCsv.nCount = (sxu32)ph7_array_count(apArg[1]);` |
|     81 | 3031 | `	ph7_array_walk(apArg[1],csv_write_callback,&sCsv);` |
|     81 | 3032 | `	if( nEol > 0 ){` |
|     79 | 3033 | `		SyBlobAppend(&sLine,(const void *)zEol,(sxu32)nEol);` |
|     39 | 3034 | `	}` |
|      - | 3035 | `	/* Write at the LOGICAL position, not the device one -- the same rule` |
|      - | 3036 | `	 * PH7_builtin_fwrite applies after a buffered read (fgets() then` |
|      - | 3037 | `	 * fputcsv() overwrites what fgets left unread). */` |
|     81 | 3038 | `	StreamSeekBackForWrite(pDev);` |
|    121 | 3039 | `	nWr = PH7_StreamWrite(pDev,(const void *)SyBlobData(&sLine),` |
|     80 | 3040 | `		(ph7_int64)SyBlobLength(&sLine));` |
|     81 | 3041 | `	SyBlobRelease(&sLine);` |
|     81 | 3042 | `	if( nWr < 0 ){` |
|    ! 0 | 3043 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 3044 | `	}else{` |
|     81 | 3045 | `		ph7_result_int64(pCtx,nWr);` |
|      - | 3046 | `	}` |
|     81 | 3047 | `	return PH7_OK;` |
|     47 | 3048 | `}` |
|      - | 3049 | `/*` |
|      - | 3050 | ` * fprintf,vfprintf private data.` |
|      - | 3051 | ` * An instance of the following structure is passed to the formatted` |
|      - | 3052 | ` * input consumer callback defined below.` |
|      - | 3053 | ` */` |
|      - | 3054 | `typedef struct fprintf_data fprintf_data;` |
|      - | 3055 | `struct fprintf_data` |
|      - | 3056 | `{` |
|      - | 3057 | `	io_private *pIO;        /* IO stream */` |
|      - | 3058 | `	ph7_int64 nCount;       /* Total number of bytes written */` |
|      - | 3059 | `};` |
|      - | 3060 | `/*` |
|      - | 3061 | ` * Callback [i.e: Formatted input consumer] for the fprintf function.` |
|      - | 3062 | ` */` |
|     38 | 3063 | `static int fprintfConsumer(ph7_context *pCtx,const char *zInput,int nLen,void *pUserData)` |
|      2 | 3064 | `{` |
|     40 | 3065 | `	fprintf_data *pFdata = (fprintf_data *)pUserData;` |
|      - | 3066 | `	ph7_int64 n;` |
|      - | 3067 | `	/* Write the formatted data */` |
|     40 | 3068 | `	n = PH7_StreamWrite(pFdata->pIO,(const void *)zInput,nLen);` |
|     40 | 3069 | `	if( n < 1 ){` |
|    ! 0 | 3070 | `		SXUNUSED(pCtx); /* cc warning */` |
|      - | 3071 | `		/* IO error,abort immediately */` |
|    ! 0 | 3072 | `		return SXERR_ABORT;` |
|      - | 3073 | `	}` |
|      - | 3074 | `	/* Increment counter */` |
|     40 | 3075 | `	pFdata->nCount += n;` |
|     40 | 3076 | `	return PH7_OK;` |
|     21 | 3077 | `}` |
|      - | 3078 | `/*` |
|      - | 3079 | ` * int fprintf(resource $handle,string $format[,mixed $args [, mixed $... ]])` |
|      - | 3080 | ` *  Write a formatted string to a stream.` |
|      - | 3081 | ` * Parameters` |
|      - | 3082 | ` *  $handle` |
|      - | 3083 | ` *   The file pointer.` |
|      - | 3084 | ` *  $format` |
|      - | 3085 | ` *   String format (see sprintf()).` |
|      - | 3086 | ` * Return` |
|      - | 3087 | ` *  The length of the written string.` |
|      - | 3088 | ` */` |
|     20 | 3089 | `PH7_PRIVATE int PH7_builtin_fprintf(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      2 | 3090 | `{` |
|      - | 3091 | `	fprintf_data sFdata;` |
|      - | 3092 | `	const char *zFormat;` |
|      - | 3093 | `	io_private *pDev;` |
|      - | 3094 | `	int nLen;` |
|     22 | 3095 | `	if( nArg < 2 ){` |
|    ! 0 | 3096 | `		ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"Invalid arguments");` |
|    ! 0 | 3097 | `		ph7_result_int(pCtx,0);` |
|    ! 0 | 3098 | `		return PH7_OK;` |
|      - | 3099 | `	}` |
|      - | 3100 | `	{` |
|      - | 3101 | `		/* php: a non-resource $stream is a TypeError (#1), not a warn-and-return-0. */` |
|     22 | 3102 | `		sxi32 rcs = PH7_CheckStreamArg(pCtx,apArg[0],1,"stream");` |
|     22 | 3103 | `		if( rcs != PH7_OK ){` |
|    ! 0 | 3104 | `			return rcs;` |
|      - | 3105 | `		}` |
|      - | 3106 | `	}` |
|      - | 3107 | `	/* Extract our private data */` |
|     22 | 3108 | `	pDev = (io_private *)ph7_value_to_resource(apArg[0]);` |
|      - | 3109 | `	/* Make sure we are dealing with a valid io_private instance */` |
|     22 | 3110 | `	if( IO_PRIVATE_INVALID(pDev) ){` |
|      - | 3111 | `		/*Expecting an IO handle */` |
|    ! 0 | 3112 | `		ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"Expecting an IO handle");` |
|    ! 0 | 3113 | `		ph7_result_int(pCtx,0);` |
|    ! 0 | 3114 | `		return PH7_OK;` |
|      - | 3115 | `	}` |
|      - | 3116 | `	/* Point to the target IO stream device */` |
|     22 | 3117 | `	if( pDev->pStream == 0  \|\| pDev->pStream->xWrite == 0 ){` |
|    ! 0 | 3118 | `		ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,` |
|      - | 3119 | `			"IO routine(%s) not implemented in the underlying stream(%s) device",` |
|    ! 0 | 3120 | `			ph7_function_name(pCtx),pDev->pStream ? pDev->pStream->zName : "null_stream"` |
|      - | 3121 | `			);` |
|    ! 0 | 3122 | `		ph7_result_int(pCtx,0);` |
|    ! 0 | 3123 | `		return PH7_OK;` |
|      - | 3124 | `	}` |
|      - | 3125 | `	/* PHP 8: a non-string-coercible $format (array/object/resource) is a TypeError (#2). */` |
|      - | 3126 | `	{` |
|     22 | 3127 | `		sxi32 rcf = PH7_FormatCheckFormatArg(pCtx,apArg[1],2);` |
|     22 | 3128 | `		if( rcf != PH7_OK ){` |
|    ! 0 | 3129 | `			return rcf;` |
|      - | 3130 | `		}` |
|      - | 3131 | `	}` |
|      - | 3132 | `	/* Extract the string format (scalars/null coerce). */` |
|     22 | 3133 | `	zFormat = ph7_value_to_string(apArg[1],&nLen);` |
|     22 | 3134 | `	if( nLen < 1 ){` |
|      - | 3135 | `		/* Empty string,return zero */` |
|    ! 0 | 3136 | `		ph7_result_int(pCtx,0);` |
|    ! 0 | 3137 | `		return PH7_OK;` |
|      - | 3138 | `	}` |
|      - | 3139 | `	{` |
|      - | 3140 | `		/* PHP 8: too few value arguments is a catchable ArgumentCountError before output,` |
|      - | 3141 | `		 * and php runs this check BEFORE validating the specifiers. fprintf's values start` |
|      - | 3142 | `		 * at apArg[2] (apArg[0]=stream, apArg[1]=format). */` |
|     22 | 3143 | `		sxi32 rcv = PH7_FormatCheckArgCount(pCtx,zFormat,nLen,nArg - 2,2,FALSE);` |
|     22 | 3144 | `		if( rcv != PH7_OK ){` |
|      3 | 3145 | `			return rcv;` |
|      - | 3146 | `		}` |
|      - | 3147 | `		/* PHP 8: an unknown or missing format specifier throws a catchable ValueError` |
|      - | 3148 | `		 * before any output; propagate the throw status verbatim. */` |
|     20 | 3149 | `		rcv = PH7_FormatValidate(pCtx,zFormat,nLen);` |
|     20 | 3150 | `		if( rcv != PH7_OK ){` |
|      5 | 3151 | `			return rcv;` |
|      - | 3152 | `		}` |
|      - | 3153 | `	}` |
|      - | 3154 | `	/* Prepare our private data */` |
|     16 | 3155 | `	sFdata.nCount = 0;` |
|     16 | 3156 | `	sFdata.pIO = pDev;` |
|      - | 3157 | `	/* Format the string */` |
|      - | 3158 | `	{` |
|     16 | 3159 | `	sxi32 rcv = PH7_InputFormat(fprintfConsumer,pCtx,zFormat,nLen,nArg - 1,&apArg[1],(void *)&sFdata,FALSE);` |
|      - | 3160 | `	/* Return total number of bytes written */` |
|     16 | 3161 | `	ph7_result_int64(pCtx,sFdata.nCount);` |
|      - | 3162 | `	/* A %s argument that could not be coerced raised php's Error mid-format; the` |
|      - | 3163 | `	 * bytes still went to the stream, as php's do, so report the throw last. */` |
|     16 | 3164 | `	if( rcv != SXRET_OK ){` |
|      3 | 3165 | `		pCtx->nThrowRc = rcv;` |
|      3 | 3166 | `		return rcv;` |
|      - | 3167 | `	}` |
|      - | 3168 | `	}` |
|     13 | 3169 | `	return PH7_OK;` |
|     12 | 3170 | `}` |
|      - | 3171 | `/*` |
|      - | 3172 | ` * int vfprintf(resource $handle,string $format,array $args)` |
|      - | 3173 | ` *  Write a formatted string to a stream.` |
|      - | 3174 | ` * Parameters` |
|      - | 3175 | ` *  $handle` |
|      - | 3176 | ` *   The file pointer.` |
|      - | 3177 | ` *  $format` |
|      - | 3178 | ` *   String format (see sprintf()).` |
|      - | 3179 | ` * $args` |
|      - | 3180 | ` *   User arguments.` |
|      - | 3181 | ` * Return` |
|      - | 3182 | ` *  The length of the written string.` |
|      - | 3183 | ` */` |
|      8 | 3184 | `PH7_PRIVATE int PH7_builtin_vfprintf(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      2 | 3185 | `{` |
|      - | 3186 | `	fprintf_data sFdata;` |
|      - | 3187 | `	const char *zFormat;` |
|      - | 3188 | `	ph7_hashmap *pMap;` |
|      - | 3189 | `	io_private *pDev;` |
|      - | 3190 | `	SySet sArg;` |
|      - | 3191 | `	int n,nLen;` |
|     10 | 3192 | `	if( nArg < 3 ){` |
|    ! 0 | 3193 | `		ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"Invalid arguments");` |
|    ! 0 | 3194 | `		ph7_result_int(pCtx,0);` |
|    ! 0 | 3195 | `		return PH7_OK;` |
|      - | 3196 | `	}` |
|      - | 3197 | `	{` |
|      - | 3198 | `		/* php: a non-resource $stream is a TypeError (#1), not a warn-and-return-0. */` |
|     10 | 3199 | `		sxi32 rcs = PH7_CheckStreamArg(pCtx,apArg[0],1,"stream");` |
|     10 | 3200 | `		if( rcs != PH7_OK ){` |
|      3 | 3201 | `			return rcs;` |
|      - | 3202 | `		}` |
|      - | 3203 | `	}` |
|      - | 3204 | `	/* PHP 8 checks argument types left-to-right: $format (#2) then $values (#3). */` |
|      - | 3205 | `	{` |
|      8 | 3206 | `		sxi32 rcf = PH7_FormatCheckFormatArg(pCtx,apArg[1],2);` |
|      8 | 3207 | `		if( rcf != PH7_OK ){` |
|    ! 0 | 3208 | `			return rcf;` |
|      - | 3209 | `		}` |
|      - | 3210 | `	}` |
|      8 | 3211 | `	if( !ph7_value_is_array(apArg[2]) ){` |
|      - | 3212 | `		/* PHP 8: a non-array $values is a catchable TypeError. */` |
|      - | 3213 | `		char zBuf[64];` |
|    ! 0 | 3214 | `		return PH7_VmThrowException(pCtx,"TypeError",` |
|      - | 3215 | `			"vfprintf(): Argument #3 ($values) must be of type array, %s given",` |
|    ! 0 | 3216 | `			VmValueGivenName(apArg[2],zBuf,sizeof(zBuf)));` |
|      - | 3217 | `	}` |
|      - | 3218 | `	/* Extract our private data */` |
|      8 | 3219 | `	pDev = (io_private *)ph7_value_to_resource(apArg[0]);` |
|      - | 3220 | `	/* Make sure we are dealing with a valid io_private instance */` |
|      8 | 3221 | `	if( IO_PRIVATE_INVALID(pDev) ){` |
|      - | 3222 | `		/*Expecting an IO handle */` |
|    ! 0 | 3223 | `		ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"Expecting an IO handle");` |
|    ! 0 | 3224 | `		ph7_result_int(pCtx,0);` |
|    ! 0 | 3225 | `		return PH7_OK;` |
|      - | 3226 | `	}` |
|      - | 3227 | `	/* Point to the target IO stream device */` |
|      8 | 3228 | `	if( pDev->pStream == 0  \|\| pDev->pStream->xWrite == 0 ){` |
|    ! 0 | 3229 | `		ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,` |
|      - | 3230 | `			"IO routine(%s) not implemented in the underlying stream(%s) device",` |
|    ! 0 | 3231 | `			ph7_function_name(pCtx),pDev->pStream ? pDev->pStream->zName : "null_stream"` |
|      - | 3232 | `			);` |
|    ! 0 | 3233 | `		ph7_result_int(pCtx,0);` |
|    ! 0 | 3234 | `		return PH7_OK;` |
|      - | 3235 | `	}` |
|      - | 3236 | `	/* Extract the string format */` |
|      8 | 3237 | `	zFormat = ph7_value_to_string(apArg[1],&nLen);` |
|      8 | 3238 | `	if( nLen < 1 ){` |
|      - | 3239 | `		/* Empty string,return zero */` |
|    ! 0 | 3240 | `		ph7_result_int(pCtx,0);` |
|    ! 0 | 3241 | `		return PH7_OK;` |
|      - | 3242 | `	}` |
|      - | 3243 | `	/* Point to hashmap */` |
|      8 | 3244 | `	pMap = (ph7_hashmap *)apArg[2]->x.pOther;` |
|      - | 3245 | `	/* PHP 8: too few items in the $values array is a catchable ValueError before output.` |
|      - | 3246 | `	 * php runs this BEFORE validating the specifiers. */` |
|      - | 3247 | `	{` |
|      8 | 3248 | `		sxi32 rcc = PH7_FormatCheckArgCount(pCtx,zFormat,nLen,(int)pMap->nEntry,1,TRUE);` |
|      8 | 3249 | `		if( rcc != PH7_OK ){` |
|      3 | 3250 | `			return rcc;` |
|      - | 3251 | `		}` |
|      - | 3252 | `	}` |
|      - | 3253 | `	/* PHP 8: an unknown or missing format specifier throws a catchable ValueError before` |
|      - | 3254 | `	 * any output; propagate the throw status verbatim. */` |
|      - | 3255 | `	{` |
|      6 | 3256 | `		sxi32 rcv = PH7_FormatValidate(pCtx,zFormat,nLen);` |
|      6 | 3257 | `		if( rcv != PH7_OK ){` |
|    ! 0 | 3258 | `			return rcv;` |
|      - | 3259 | `		}` |
|      - | 3260 | `	}` |
|      - | 3261 | `	/* Extract arguments from the hashmap */` |
|      6 | 3262 | `	n = PH7_HashmapValuesToSet(pMap,&sArg);` |
|      - | 3263 | `	/* Prepare our private data */` |
|      6 | 3264 | `	sFdata.nCount = 0;` |
|      6 | 3265 | `	sFdata.pIO = pDev;` |
|      - | 3266 | `	/* Format the string */` |
|      - | 3267 | `	{` |
|      6 | 3268 | `	sxi32 rcv = PH7_InputFormat(fprintfConsumer,pCtx,zFormat,nLen,n,(ph7_value **)SySetBasePtr(&sArg),(void *)&sFdata,TRUE);` |
|      - | 3269 | `	/* Return total number of bytes written*/` |
|      6 | 3270 | `	ph7_result_int64(pCtx,sFdata.nCount);` |
|      6 | 3271 | `	SySetRelease(&sArg);` |
|      - | 3272 | `	/* A %s argument that could not be coerced raised php's Error mid-format; the` |
|      - | 3273 | `	 * bytes still went to the stream, as php's do, so report the throw last. */` |
|      6 | 3274 | `	if( rcv != SXRET_OK ){` |
|      3 | 3275 | `		pCtx->nThrowRc = rcv;` |
|      3 | 3276 | `		return rcv;` |
|      - | 3277 | `	}` |
|      - | 3278 | `	}` |
|      3 | 3279 | `	return PH7_OK;` |
|      6 | 3280 | `}` |
|      - | 3281 | `/*` |
|      - | 3282 | ` * Convert open modes (string passed to the fopen() function) [i.e: 'r','w+','a',...] into PH7 flags.` |
|      - | 3283 | ` * According to the PHP reference manual:` |
|      - | 3284 | ` *  The mode parameter specifies the type of access you require to the stream. It may be any of the following` |
|      - | 3285 | ` *   'r' 	Open for reading only; place the file pointer at the beginning of the file.` |
|      - | 3286 | ` *   'r+' 	Open for reading and writing; place the file pointer at the beginning of the file.` |
|      - | 3287 | ` *   'w' 	Open for writing only; place the file pointer at the beginning of the file and truncate the file` |
|      - | 3288 | ` *          to zero length. If the file does not exist, attempt to create it.` |
|      - | 3289 | ` *   'w+' 	Open for reading and writing; place the file pointer at the beginning of the file and truncate` |
|      - | 3290 | ` *              the file to zero length. If the file does not exist, attempt to create it.` |
|      - | 3291 | ` *   'a' 	Open for writing only; place the file pointer at the end of the file. If the file does not` |
|      - | 3292 | ` *         exist, attempt to create it.` |
|      - | 3293 | ` *   'a+' 	Open for reading and writing; place the file pointer at the end of the file. If the file does` |
|      - | 3294 | ` *          not exist, attempt to create it.` |
|      - | 3295 | ` *   'x' 	Create and open for writing only; place the file pointer at the beginning of the file. If the file` |
|      - | 3296 | ` *         already exists,` |
|      - | 3297 | ` *         the fopen() call will fail by returning FALSE and generating an error of level E_WARNING. If the file` |
|      - | 3298 | ` *         does not exist attempt to create it. This is equivalent to specifying O_EXCL\|O_CREAT flags for` |
|      - | 3299 | ` *         the underlying open(2) system call.` |
|      - | 3300 | ` *   'x+' 	Create and open for reading and writing; otherwise it has the same behavior as 'x'.` |
|      - | 3301 | ` *   'c' 	Open the file for writing only. If the file does not exist, it is created. If it exists, it is neither truncated` |
|      - | 3302 | ` *          (as opposed to 'w'), nor the call to this function fails (as is the case with 'x'). The file pointer` |
|      - | 3303 | ` *          is positioned on the beginning of the file.` |
|      - | 3304 | ` *          This may be useful if it's desired to get an advisory lock (see flock()) before attempting to modify the file` |
|      - | 3305 | ` *          as using 'w' could truncate the file before the lock was obtained (if truncation is desired, ftruncate() can` |
|      - | 3306 | ` *          be used after the lock is requested).` |
|      - | 3307 | ` *   'c+' 	Open the file for reading and writing; otherwise it has the same behavior as 'c'.` |
|      - | 3308 | ` */` |
|    916 | 3309 | `static int StrModeToFlags(ph7_context *pCtx,const char *zMode,int nLen)` |
|      5 | 3310 | `{` |
|    921 | 3311 | `	const char *zEnd = &zMode[nLen];` |
|    921 | 3312 | `	int iFlag = 0;` |
|      - | 3313 | `	int c;` |
|    921 | 3314 | `	if( nLen < 1 ){` |
|      - | 3315 | `		/* Open in a read-only mode */` |
|    ! 0 | 3316 | `		return PH7_IO_OPEN_RDONLY;` |
|      - | 3317 | `	}` |
|    921 | 3318 | `	c = zMode[0];` |
|    921 | 3319 | `	if( c == 'r' \|\| c == 'R' ){` |
|      - | 3320 | `		/* Read-only access */` |
|    469 | 3321 | `		iFlag = PH7_IO_OPEN_RDONLY;` |
|    469 | 3322 | `		zMode++; /* Advance */` |
|    469 | 3323 | `		if( zMode < zEnd ){` |
|    197 | 3324 | `			c = zMode[0];` |
|    197 | 3325 | `			if( c == '+' \|\| c == 'w' \|\| c == 'W' ){` |
|      - | 3326 | `				/* Read+Write access */` |
|    173 | 3327 | `				iFlag = PH7_IO_OPEN_RDWR;` |
|     84 | 3328 | `			}` |
|    101 | 3329 | `		}` |
|    689 | 3330 | `	}else if( c == 'w' \|\| c == 'W' ){` |
|      - | 3331 | `		/* Overwrite mode.` |
|      - | 3332 | `		 * If the file does not exists,try to create it` |
|      - | 3333 | `		 */` |
|    236 | 3334 | `		iFlag = PH7_IO_OPEN_WRONLY\|PH7_IO_OPEN_TRUNC\|PH7_IO_OPEN_CREATE;` |
|    236 | 3335 | `		zMode++; /* Advance */` |
|    236 | 3336 | `		if( zMode < zEnd ){` |
|    178 | 3337 | `			c = zMode[0];` |
|    178 | 3338 | `			if( c == '+' \|\| c == 'r' \|\| c == 'R' ){` |
|      - | 3339 | `				/* Read+Write access */` |
|    178 | 3340 | `				iFlag &= ~PH7_IO_OPEN_WRONLY;` |
|    178 | 3341 | `				iFlag \|= PH7_IO_OPEN_RDWR;` |
|     88 | 3342 | `			}` |
|     92 | 3343 | `		}` |
|    341 | 3344 | `	}else if( c == 'a' \|\| c == 'A' ){` |
|      - | 3345 | `		/* Append mode (place the file pointer at the end of the file).` |
|      - | 3346 | `		 * Create the file if it does not exists.` |
|      - | 3347 | `		 */` |
|      3 | 3348 | `		iFlag = PH7_IO_OPEN_WRONLY\|PH7_IO_OPEN_APPEND\|PH7_IO_OPEN_CREATE;` |
|      3 | 3349 | `		zMode++; /* Advance */` |
|      3 | 3350 | `		if( zMode < zEnd ){` |
|    ! 0 | 3351 | `			c = zMode[0];` |
|    ! 0 | 3352 | `			if( c == '+' ){` |
|      - | 3353 | `				/* Read-Write access */` |
|    ! 0 | 3354 | `				iFlag &= ~PH7_IO_OPEN_WRONLY;` |
|    ! 0 | 3355 | `				iFlag \|= PH7_IO_OPEN_RDWR;` |
|    ! 0 | 3356 | `			}` |
|      1 | 3357 | `		}` |
|    224 | 3358 | `	}else if( c == 'x' \|\| c == 'X' ){` |
|      - | 3359 | `		/* Exclusive access.` |
|      - | 3360 | `		 * If the file already exists,return immediately with a failure code.` |
|      - | 3361 | `		 * Otherwise create a new file.` |
|      - | 3362 | `		 */` |
|    223 | 3363 | `		iFlag = PH7_IO_OPEN_WRONLY\|PH7_IO_OPEN_EXCL;` |
|    223 | 3364 | `		zMode++; /* Advance */` |
|    223 | 3365 | `		if( zMode < zEnd ){` |
|    ! 0 | 3366 | `			c = zMode[0];` |
|    ! 0 | 3367 | `			if( c == '+' \|\| c == 'r' \|\| c == 'R' ){` |
|      - | 3368 | `				/* Read-Write access */` |
|    ! 0 | 3369 | `				iFlag &= ~PH7_IO_OPEN_WRONLY;` |
|    ! 0 | 3370 | `				iFlag \|= PH7_IO_OPEN_RDWR;` |
|    ! 0 | 3371 | `			}` |
|      5 | 3372 | `		}` |
|    109 | 3373 | `	}else if( c == 'c' \|\| c == 'C' ){` |
|      - | 3374 | `		/* Overwrite mode.Create the file if it does not exists.*/` |
|    ! 0 | 3375 | `		iFlag = PH7_IO_OPEN_WRONLY\|PH7_IO_OPEN_CREATE;` |
|    ! 0 | 3376 | `		zMode++; /* Advance */` |
|    ! 0 | 3377 | `		if( zMode < zEnd ){` |
|    ! 0 | 3378 | `			c = zMode[0];` |
|    ! 0 | 3379 | `			if( c == '+' ){` |
|      - | 3380 | `				/* Read-Write access */` |
|    ! 0 | 3381 | `				iFlag &= ~PH7_IO_OPEN_WRONLY;` |
|    ! 0 | 3382 | `				iFlag \|= PH7_IO_OPEN_RDWR;` |
|    ! 0 | 3383 | `			}` |
|    ! 0 | 3384 | `		}` |
|    ! 0 | 3385 | `	}else{` |
|      - | 3386 | `		/* Invalid mode. Assume a read only open */` |
|    ! 0 | 3387 | `		ph7_context_throw_error(pCtx,PH7_CTX_NOTICE,"Invalid open mode,PH7 is assuming a Read-Only open");` |
|    ! 0 | 3388 | `		iFlag = PH7_IO_OPEN_RDONLY;` |
|      - | 3389 | `	}` |
|   1291 | 3390 | `	while( zMode < zEnd ){` |
|    375 | 3391 | `		c = zMode[0];` |
|    375 | 3392 | `		if( c == 'b' \|\| c == 'B' ){` |
|     27 | 3393 | `			iFlag &= ~PH7_IO_OPEN_TEXT;` |
|     27 | 3394 | `			iFlag \|= PH7_IO_OPEN_BINARY;` |
|    362 | 3395 | `		}else if( c == 't' \|\| c == 'T' ){` |
|    ! 0 | 3396 | `			iFlag &= ~PH7_IO_OPEN_BINARY;` |
|    ! 0 | 3397 | `			iFlag \|= PH7_IO_OPEN_TEXT;` |
|    ! 0 | 3398 | `		}` |
|    375 | 3399 | `		zMode++;` |
|      5 | 3400 | `	}` |
|    921 | 3401 | `	return iFlag;` |
|    463 | 3402 | `}` |
|      - | 3403 | `/*` |
|      - | 3404 | ` * Initialize the IO private structure.` |
|      - | 3405 | ` */` |
|   7689 | 3406 | `PH7_PRIVATE void InitIOPrivate(ph7_vm *pVm,const ph7_io_stream *pStream,io_private *pOut)` |
|      5 | 3407 | `{` |
|   7694 | 3408 | `	pOut->pStream = pStream;` |
|   7694 | 3409 | `	SyBlobInit(&pOut->sBuffer,&pVm->sAllocator);` |
|   7694 | 3410 | `	pOut->nOfft = 0;` |
|   7694 | 3411 | `	SyBlobInit(&pOut->sUri,&pVm->sAllocator);` |
|   7694 | 3412 | `	pOut->zMode[0] = 0;` |
|   7694 | 3413 | `	pOut->bEof = 0;` |
|   7694 | 3414 | `	pOut->bDir = 0;` |
|   7694 | 3415 | `	pOut->bPersist = 0;` |
|   7694 | 3416 | `	pOut->nChunk = 8192; /* php's own default, and what stream_set_chunk_size() reports first */` |
|   7694 | 3417 | `	pOut->bNonBlock = 0;` |
|   7694 | 3418 | `	pOut->bHasTimeout = 0;` |
|   7694 | 3419 | `	pOut->bTimedOut = 0;` |
|   7694 | 3420 | `	pOut->pReadFilters = 0;` |
|   7694 | 3421 | `	pOut->pWriteFilters = 0;` |
|   7694 | 3422 | `	pOut->bFiltDone = 0;` |
|   7694 | 3423 | `	pOut->bFiltErr = 0;` |
|   7694 | 3424 | `	pOut->iFiltPos = 0;` |
|   7694 | 3425 | `	pOut->pCtxRes = 0;` |
|   7694 | 3426 | `	SyBlobInit(&pOut->sFilt,&pVm->sAllocator);` |
|   7694 | 3427 | `	pOut->nFiltOfft = 0;` |
|      - | 3428 | `	/* Set the magic number */` |
|   7694 | 3429 | `	pOut->iMagic = IO_PRIVATE_MAGIC;` |
|   7694 | 3430 | `}` |
|      - | 3431 | `/*` |
|      - | 3432 | `` * Record what the opener was asked for, for stream_get_meta_data()'s `uri` and`` |
|      - | 3433 | `` * `mode` keys. php keeps the URI exactly as written (a relative path stays`` |
|      - | 3434 | ` * relative) and the mode in a 16-byte field; passing a NULL/empty zUri leaves` |
|      - | 3435 | ` * the key out, which is how a popen() pipe reports no wrapper and no uri.` |
|      - | 3436 | ` */` |
|   7619 | 3437 | `PH7_PRIVATE void SetIOPrivateOpenedAs(io_private *pDev,const char *zUri,int nUriLen,const char *zMode,int nModeLen)` |
|      5 | 3438 | `{` |
|   7624 | 3439 | `	if( pDev == 0 ){` |
|    ! 0 | 3440 | `		return;` |
|      - | 3441 | `	}` |
|   7624 | 3442 | `	SyBlobReset(&pDev->sUri);` |
|   7624 | 3443 | `	if( zUri && nUriLen > 0 ){` |
|   1067 | 3444 | `		SyBlobAppend(&pDev->sUri,zUri,(sxu32)nUriLen);` |
|    531 | 3445 | `	}` |
|   7624 | 3446 | `	if( zMode && nModeLen > 0 ){` |
|   7624 | 3447 | `		if( nModeLen > (int)sizeof(pDev->zMode) - 1 ){` |
|    ! 0 | 3448 | `			nModeLen = (int)sizeof(pDev->zMode) - 1;` |
|    ! 0 | 3449 | `		}` |
|   7624 | 3450 | `		SyMemcpy(zMode,pDev->zMode,(sxu32)nModeLen);` |
|   7624 | 3451 | `		pDev->zMode[nModeLen] = 0;` |
|   3814 | 3452 | `	}else{` |
|    ! 0 | 3453 | `		pDev->zMode[0] = 0;` |
|      - | 3454 | `	}` |
|   3814 | 3455 | `}` |
|      - | 3456 | `/*` |
|      - | 3457 | ` * Release the IO private structure.` |
|      - | 3458 | ` */` |
|     62 | 3459 | `static void ReleaseIOPrivate(ph7_context *pCtx,io_private *pDev)` |
|      5 | 3460 | `{` |
|     67 | 3461 | `	PH7_StreamFilterReleaseChains(pDev);` |
|     67 | 3462 | `	SyBlobRelease(&pDev->sBuffer);` |
|     67 | 3463 | `	SyBlobRelease(&pDev->sFilt);` |
|     67 | 3464 | `	SyBlobRelease(&pDev->sUri);` |
|     67 | 3465 | `	pDev->iMagic = 0x2126; /* Invalid magic number so we can detetct misuse */` |
|      - | 3466 | `	/* Release the whole structure */` |
|     67 | 3467 | `	ph7_context_free_chunk(pCtx,pDev);` |
|     67 | 3468 | `}` |
|      - | 3469 | `/*` |
|      - | 3470 | ` * Mark a user-facing IO handle as closed while keeping the io_private alive.` |
|      - | 3471 | ` * The caller has already closed the underlying OS handle; we drop the work` |
|      - | 3472 | ` * buffer and stamp the closed magic so every ph7_value that still references` |
|      - | 3473 | ` * this handle sees a "resource (closed)" (php semantics). The struct is` |
|      - | 3474 | ` * reclaimed in bulk when the VM allocator is torn down. Freeing it here — as` |
|      - | 3475 | ` * fclose/closedir/pclose used to — would dangle the caller's copy (a latent,` |
|      - | 3476 | ` * pool-masked UAF) and keep reporting the handle open.` |
|      - | 3477 | ` */` |
|   7395 | 3478 | `PH7_PRIVATE void MarkIOPrivateClosed(io_private *pDev)` |
|      5 | 3479 | `{` |
|      - | 3480 | `	/* A filter outliving its handle would keep answering is_resource() and hold` |
|      - | 3481 | `	 * a pointer to a closed device; every close path releases the chains before` |
|      - | 3482 | `	 * the device goes, and this is the backstop for one that forgets. */` |
|   7400 | 3483 | `	PH7_StreamFilterReleaseChains(pDev);` |
|   7400 | 3484 | `	SyBlobRelease(&pDev->sBuffer);` |
|   7400 | 3485 | `	SyBlobRelease(&pDev->sFilt);` |
|   7400 | 3486 | `	SyBlobRelease(&pDev->sUri);` |
|   7400 | 3487 | `	pDev->pHandle = 0;` |
|   7400 | 3488 | `	pDev->iMagic = IO_PRIVATE_CLOSED_MAGIC;` |
|   7400 | 3489 | `}` |
|      - | 3490 | `/*` |
|      - | 3491 | ` * Reset the IO private structure.` |
|      - | 3492 | ` */` |
|    380 | 3493 | `static void ResetIOPrivate(io_private *pDev)` |
|      5 | 3494 | `{` |
|    385 | 3495 | `	SyBlobReset(&pDev->sBuffer);` |
|    385 | 3496 | `	pDev->nOfft = 0;` |
|      - | 3497 | `	/* A seek moves the DEVICE, so whatever the read chain had already produced` |
|      - | 3498 | `	 * from the old position is not what the new one answers. */` |
|    385 | 3499 | `	SyBlobReset(&pDev->sFilt);` |
|    385 | 3500 | `	pDev->nFiltOfft = 0;` |
|    385 | 3501 | `	pDev->bFiltDone = 0;` |
|    385 | 3502 | `	pDev->bFiltErr = 0;` |
|    385 | 3503 | `	PH7_StreamFilterRewound(pDev);` |
|      - | 3504 | `	/* Every caller of this has just MOVED the device (a seek, a rewind, a` |
|      - | 3505 | `	 * truncate), and php clears the end-of-file flag on exactly those. */` |
|    385 | 3506 | `	pDev->bEof = 0;` |
|    385 | 3507 | `}` |
|      - | 3508 | `/* Forward declaration */` |
|      - | 3509 |  |
|      - | 3510 | `/*` |
|      - | 3511 | ` * resource fopen(string $filename,string $mode [,bool $use_include_path = false[,resource $context ]])` |
|      - | 3512 | ` *  Open a file,a URL or any other IO stream.` |
|      - | 3513 | ` * Parameters` |
|      - | 3514 | ` *  $filename` |
|      - | 3515 | ` *   If filename is of the form "scheme://...", it is assumed to be a URL and PHP will search` |
|      - | 3516 | ` *   for a protocol handler (also known as a wrapper) for that scheme. If no scheme is given` |
|      - | 3517 | ` *   then a regular file is assumed.` |
|      - | 3518 | ` *  $mode` |
|      - | 3519 | ` *   The mode parameter specifies the type of access you require to the stream` |
|      - | 3520 | ` *   See the block comment associated with the StrModeToFlags() for the supported` |
|      - | 3521 | ` *   modes.` |
|      - | 3522 | ` *  $use_include_path` |
|      - | 3523 | ` *   You can use the optional second parameter and set it to` |
|      - | 3524 | ` *   TRUE, if you want to search for the file in the include_path, too.` |
|      - | 3525 | ` *  $context` |
|      - | 3526 | ` *   A context stream resource.` |
|      - | 3527 | ` * Return` |
|      - | 3528 | ` *  File handle on success or FALSE on failure.` |
|      - | 3529 | ` */` |
|      - | 3530 | `/*` |
|      - | 3531 | ` * string\|false stream_get_contents(resource $stream, int $maxLength = -1,` |
|      - | 3532 | ` *                                  int $offset = -1)` |
|      - | 3533 | ` *  Read the remaining contents of a stream into a string.` |
|      - | 3534 | ` */` |
|    256 | 3535 | `PH7_PRIVATE int PH7_builtin_stream_get_contents(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      2 | 3536 | `{` |
|      - | 3537 | `	const ph7_io_stream *pStream;` |
|      - | 3538 | `	io_private *pDev;` |
|    258 | 3539 | `	ph7_int64 nMax = -1;` |
|      - | 3540 | `	char zBuf[4096];` |
|      - | 3541 | `	ph7_int64 nRead;` |
|    258 | 3542 | `	if( nArg < 1 \|\| !ph7_value_is_resource(apArg[0]) ){` |
|    ! 0 | 3543 | `		ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"Expecting an IO handle");` |
|    ! 0 | 3544 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 3545 | `		return PH7_OK;` |
|      - | 3546 | `	}` |
|    258 | 3547 | `	pDev = (io_private *)ph7_value_to_resource(apArg[0]);` |
|    258 | 3548 | `	if( IO_PRIVATE_INVALID(pDev) ){` |
|    ! 0 | 3549 | `		ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"Expecting an IO handle");` |
|    ! 0 | 3550 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 3551 | `		return PH7_OK;` |
|      - | 3552 | `	}` |
|    258 | 3553 | `	pStream = pDev->pStream;` |
|    258 | 3554 | `	if( pStream == 0 \|\| pStream->xRead == 0 ){` |
|    ! 0 | 3555 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 3556 | `		return PH7_OK;` |
|      - | 3557 | `	}` |
|    258 | 3558 | `	if( nArg > 1 && !ph7_value_is_null(apArg[1]) ){` |
|      - | 3559 | `		/* PHP 8 raises a catchable ValueError below -1; -1 (or the NULL` |
|      - | 3560 | `		 * default) means "read until EOF". */` |
|     17 | 3561 | `		nMax = ph7_value_to_int64(apArg[1]);` |
|     17 | 3562 | `		if( nMax < -1 ){` |
|      3 | 3563 | `			return PH7_VmThrowException(pCtx,"ValueError",` |
|      - | 3564 | `				"stream_get_contents(): Argument #2 ($length) must be greater than or equal to -1");` |
|      - | 3565 | `		}` |
|      7 | 3566 | `	}` |
|    256 | 3567 | `	if( nArg > 2 ){` |
|      9 | 3568 | `		ph7_int64 iOfft = ph7_value_to_int64(apArg[2]);` |
|      9 | 3569 | `		if( iOfft >= 0 && pStream->xSeek ){` |
|      9 | 3570 | `			if( pStream->xSeek(pDev->pHandle,iOfft,0/*SEEK_SET*/) == PH7_OK ){` |
|      - | 3571 | `				/* A seek DISCARDS what was buffered ahead — the bytes belong to` |
|      - | 3572 | `				 * the position we just left. Without this the read below served` |
|      - | 3573 | `				 * the old position's leftovers and then continued from the new` |
|      - | 3574 | `				 * one. */` |
|      9 | 3575 | `				ResetIOPrivate(pDev);` |
|      4 | 3576 | `			}` |
|      4 | 3577 | `		}` |
|      4 | 3578 | `	}` |
|    256 | 3579 | `	ph7_result_string(pCtx,"",0); /* seed an empty string result */` |
|    493 | 3580 | `	while( nMax != 0 ){` |
|    487 | 3581 | `		ph7_int64 nAsk = (ph7_int64)sizeof(zBuf);` |
|    487 | 3582 | `		if( nMax > 0 && nMax < nAsk ){` |
|      5 | 3583 | `			nAsk = nMax;` |
|      2 | 3584 | `		}` |
|      - | 3585 | `		/* Through PH7_StreamRead, not the device: this is a SCRIPT-level read,` |
|      - | 3586 | `		 * and the line readers buffer AHEAD. Reading the device directly meant` |
|      - | 3587 | ``		 * `stream_get_contents()` after any fgets()/fgetc()/stream_get_line()`` |
|      - | 3588 | `		 * skipped everything still sitting in that buffer — on a file the` |
|      - | 3589 | `		 * line reader had already drained to its end, that is the WHOLE` |
|      - | 3590 | `		 * remainder, so the everyday "read the first line, then take the rest"` |
|      - | 3591 | `		 * idiom answered "" and the position it left behind was wrong too. */` |
|    487 | 3592 | `		nRead = PH7_StreamRead(pDev,zBuf,nAsk);` |
|    487 | 3593 | `		if( nRead < 1 ){` |
|    250 | 3594 | `			if( nRead == 0 ){` |
|    248 | 3595 | `				pDev->bEof = 1;` |
|    123 | 3596 | `			}` |
|    250 | 3597 | `			break;` |
|      - | 3598 | `		}` |
|    239 | 3599 | `		ph7_result_string(pCtx,zBuf,(int)nRead); /* appends */` |
|    239 | 3600 | `		if( nMax > 0 ){` |
|      5 | 3601 | `			nMax -= nRead;` |
|      2 | 3602 | `		}` |
|      2 | 3603 | `	}` |
|    256 | 3604 | `	return PH7_OK;` |
|    130 | 3605 | `}` |
|      - | 3606 | `/*` |
|      - | 3607 | ` * array stream_get_wrappers(void) — names of the registered stream devices.` |
|      - | 3608 | ` */` |
|     18 | 3609 | `PH7_PRIVATE int PH7_builtin_stream_get_wrappers(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      3 | 3610 | `{` |
|      - | 3611 | `	ph7_value *pArr,*pV;` |
|      - | 3612 | `	ph7_io_stream **apDev;` |
|      - | 3613 | `	sxu32 n;` |
|      9 | 3614 | `	SXUNUSED(nArg);` |
|      9 | 3615 | `	SXUNUSED(apArg);` |
|     21 | 3616 | `	pArr = ph7_context_new_array(pCtx);` |
|     21 | 3617 | `	pV = ph7_context_new_scalar(pCtx);` |
|     21 | 3618 | `	if( pArr == 0 \|\| pV == 0 ){` |
|    ! 0 | 3619 | `		ph7_result_null(pCtx);` |
|    ! 0 | 3620 | `		return PH7_OK;` |
|      - | 3621 | `	}` |
|     21 | 3622 | `	apDev = (ph7_io_stream **)SySetBasePtr(&pCtx->pVm->aIOstream);` |
|    105 | 3623 | `	for( n = 0 ; n < SySetUsed(&pCtx->pVm->aIOstream) ; n++ ){` |
|      - | 3624 | `		/* A device a script has unregistered is GONE from php's list -- both a` |
|      - | 3625 | `		 * built-in it switched off and a userland wrapper it withdrew, which` |
|      - | 3626 | `		 * PHL used to keep naming here after neutering the slot behind it. */` |
|     87 | 3627 | `		if( PH7_VmStreamDeviceSuppressed(pCtx->pVm,apDev[n]) ){` |
|      9 | 3628 | `			continue;` |
|      - | 3629 | `		}` |
|     79 | 3630 | `		ph7_value_string(pV,apDev[n]->zName,-1);` |
|     79 | 3631 | `		ph7_array_add_elem(pArr,0,pV);` |
|     79 | 3632 | `		ph7_value_reset_string_cursor(pV);` |
|     41 | 3633 | `	}` |
|     21 | 3634 | `	ph7_result_value(pCtx,pArr);` |
|     21 | 3635 | `	return PH7_OK;` |
|     12 | 3636 | `}` |
|      - | 3637 | `/* The userland-wrapper pool is declared further down this file. */` |
|      - | 3638 | `static int IoPrivateIsUwrap(const ph7_io_stream *pStream);` |
|      - | 3639 | `static ph7_class_instance * IoPrivateUwrapObject(io_private *pDev);` |
|      - | 3640 | `/*` |
|      - | 3641 | ` * php names TWO things in a stream's metadata: the WRAPPER that opened it` |
|      - | 3642 | `` * (`wrapper_type`) and the ops that drive it (`stream_type`). They differ for`` |
|      - | 3643 | `` * nearly every device — an ordinary file is opened by `plainfile` and driven by`` |
|      - | 3644 | `` * `STDIO` — and PHL answered its own single device name for both, so neither`` |
|      - | 3645 | ` * key ever matched php. A stream php opens with NO wrapper (a popen()/proc_open()` |
|      - | 3646 | `` * pipe, a socket) reports no `wrapper_type` at all; *pzWrapper stays 0 for those.`` |
|      - | 3647 | ` */` |
|     94 | 3648 | `static void IoPrivateStreamLabels(io_private *pDev,const char **pzWrapper,const char **pzStream)` |
|      4 | 3649 | `{` |
|     98 | 3650 | `	const ph7_io_stream *pS = pDev->pStream;` |
|     98 | 3651 | `	*pzWrapper = 0;` |
|     98 | 3652 | `	*pzStream  = "STDIO";` |
|     98 | 3653 | `	if( pS == 0 ){` |
|    ! 0 | 3654 | `		return;` |
|      - | 3655 | `	}` |
|     98 | 3656 | `	if( pDev->bDir ){` |
|      3 | 3657 | `		*pzWrapper = "plainfile";` |
|      3 | 3658 | `		*pzStream  = "dir";` |
|      3 | 3659 | `		return;` |
|      - | 3660 | `	}` |
|     96 | 3661 | `	if( is_php_stream(pS) ){` |
|     31 | 3662 | `		*pzWrapper = "PHP";` |
|     31 | 3663 | `		if( PH7_PhpStreamKind(pDev->pHandle) == PH7_IO_STREAM_OUTPUT ){` |
|      - | 3664 | `			/* php://output is the VM's output consumer, not a descriptor. */` |
|      3 | 3665 | `			*pzStream = "Output";` |
|      3 | 3666 | `			return;` |
|      - | 3667 | `		}` |
|     29 | 3668 | `		if( PH7_PhpStreamKind(pDev->pHandle) == PH7_IO_STREAM_MEMORY ){` |
|      - | 3669 | `			/* php://memory and php://temp are ONE device here and two in php,` |
|      - | 3670 | `			 * which labels them apart; the URI is what separates them. */` |
|     21 | 3671 | `			const char *zUri = (const char *)SyBlobData(&pDev->sUri);` |
|     21 | 3672 | `			sxu32 nUri = SyBlobLength(&pDev->sUri);` |
|     30 | 3673 | `			*pzStream = ( nUri >= sizeof("php://temp")-1` |
|     18 | 3674 | `			           && SyStrnicmp(zUri,"php://temp",sizeof("php://temp")-1) == 0 )` |
|     18 | 3675 | `				? "TEMP" : "MEMORY";` |
|      9 | 3676 | `		}` |
|     29 | 3677 | `		return;` |
|      - | 3678 | `	}` |
|     67 | 3679 | `	if( is_data_stream(pS) ){` |
|     15 | 3680 | `		*pzWrapper = *pzStream = "RFC2397";` |
|     15 | 3681 | `		return;` |
|      - | 3682 | `	}` |
|     53 | 3683 | `	if( IoPrivateIsUwrap(pS) ){` |
|      7 | 3684 | `		*pzWrapper = *pzStream = "user-space";` |
|      7 | 3685 | `		return;` |
|      - | 3686 | `	}` |
|     47 | 3687 | `	if( pS->zName && SyStrncmp(pS->zName,"tcp",sizeof("tcp")) == 0 ){` |
|      - | 3688 | `		/* php names the socket ops and reports no wrapper for them — and names` |
|      - | 3689 | `		 * a socket with no transport under it (a pair) differently again. */` |
|      - | 3690 | `#ifdef PH7_ENABLE_NET` |
|     18 | 3691 | `		*pzStream = (pDev->pHandle && ((sock_private *)pDev->pHandle)->bGeneric)` |
|      - | 3692 | `			? "generic_socket" : "tcp_socket/ssl";` |
|      - | 3693 | `#else` |
|      - | 3694 | `		*pzStream = "tcp_socket/ssl";` |
|      - | 3695 | `#endif` |
|     18 | 3696 | `		return;` |
|      - | 3697 | `	}` |
|     30 | 3698 | `	if( SyBlobLength(&pDev->sUri) < 1 ){` |
|      - | 3699 | `		/* Opened from a DESCRIPTOR rather than through a wrapper — a popen()` |
|      - | 3700 | `		 * or proc_open() pipe end — which is precisely when php reports` |
|      - | 3701 | ``		 * neither a `wrapper_type` nor a `uri`. */`` |
|      3 | 3702 | `		return;` |
|      - | 3703 | `	}` |
|     27 | 3704 | `	*pzWrapper = "plainfile";` |
|     51 | 3705 | `}` |
|      - | 3706 | `/*` |
|      - | 3707 | ` * data:// carries its own metadata in php, and all of it comes back out of the` |
|      - | 3708 | ``  * URI the wrapper parsed: `data://<mediatype>[;name=value]*[;base64],<payload>` `` |
|      - | 3709 | ` * answers the media type, ONE KEY PER PARAMETER, and the base64 flag last. A` |
|      - | 3710 | `` * URI naming no media type has no `mediatype` key at all — php does not`` |
|      - | 3711 | ` * substitute the RFC's default — and a repeated parameter keeps its last value.` |
|      - | 3712 | ` */` |
|     12 | 3713 | `static void IoPrivateDataMeta(ph7_context *pCtx,io_private *pDev,ph7_value *pArr,ph7_value *pV)` |
|      1 | 3714 | `{` |
|     13 | 3715 | `	const char *zUri = (const char *)SyBlobData(&pDev->sUri);` |
|     13 | 3716 | `	sxu32 nUri = SyBlobLength(&pDev->sUri);` |
|     13 | 3717 | `	sxu32 nStart = 0,nComma,nSeg,i;` |
|     13 | 3718 | `	int bBase64 = 0,bFirst = 1;` |
|     13 | 3719 | `	if( nUri >= sizeof("data://")-1 && SyStrnicmp(zUri,"data://",sizeof("data://")-1) == 0 ){` |
|     13 | 3720 | `		nStart = sizeof("data://")-1;` |
|      6 | 3721 | `	}else if( nUri >= sizeof("data:")-1 && SyStrnicmp(zUri,"data:",sizeof("data:")-1) == 0 ){` |
|    ! 0 | 3722 | `		nStart = sizeof("data:")-1;` |
|    ! 0 | 3723 | `	}` |
|     13 | 3724 | `	nComma = nStart;` |
|    329 | 3725 | `	while( nComma < nUri && zUri[nComma] != ',' ){` |
|    317 | 3726 | `		nComma++;` |
|      1 | 3727 | `	}` |
|      - | 3728 | `	/* Walk the ';'-separated segments in front of the payload. */` |
|     23 | 3729 | `	for( nSeg = nStart ; nSeg <= nComma ; ){` |
|     23 | 3730 | `		sxu32 nEnd = nSeg;` |
|    329 | 3731 | `		while( nEnd < nComma && zUri[nEnd] != ';' ){` |
|    307 | 3732 | `			nEnd++;` |
|      1 | 3733 | `		}` |
|     23 | 3734 | `		if( bFirst ){` |
|     13 | 3735 | `			if( nEnd > nSeg ){` |
|     11 | 3736 | `				ph7_value_string(pV,&zUri[nSeg],(int)(nEnd - nSeg));` |
|     11 | 3737 | `				ph7_array_add_strkey_elem(pArr,"mediatype",pV);` |
|     11 | 3738 | `				ph7_value_reset_string_cursor(pV);` |
|      5 | 3739 | `			}` |
|     13 | 3740 | `			bFirst = 0;` |
|     17 | 3741 | `		}else if( nEnd - nSeg == sizeof("base64")-1` |
|      8 | 3742 | `		       && SyStrnicmp(&zUri[nSeg],"base64",sizeof("base64")-1) == 0 ){` |
|      5 | 3743 | `			bBase64 = 1;` |
|      3 | 3744 | `		}else{` |
|      - | 3745 | ``			/* `name=value`; php keys the array by the name, so a repeat wins. */`` |
|    167 | 3746 | `			for( i = nSeg ; i < nEnd && zUri[i] != '=' ; i++ ){}` |
|      7 | 3747 | `			if( i < nEnd && i > nSeg ){` |
|      - | 3748 | `				/* The name is keyed WHOLE — it has no length limit in the URI,` |
|      - | 3749 | `				 * and a clamped one files the value under a key no script can` |
|      - | 3750 | `				 * look up. */` |
|      7 | 3751 | `				ph7_value *pKey = ph7_context_new_scalar(pCtx);` |
|      7 | 3752 | `				if( pKey ){` |
|      7 | 3753 | `					ph7_value_string(pKey,&zUri[nSeg],(int)(i - nSeg));` |
|      7 | 3754 | `					ph7_value_string(pV,&zUri[i+1],(int)(nEnd - i - 1));` |
|      7 | 3755 | `					ph7_array_add_elem(pArr,pKey,pV);` |
|      7 | 3756 | `					ph7_value_reset_string_cursor(pV);` |
|      7 | 3757 | `					ph7_context_release_value(pCtx,pKey);` |
|      3 | 3758 | `				}` |
|      3 | 3759 | `			}` |
|      - | 3760 | `		}` |
|     23 | 3761 | `		if( nEnd >= nComma ){` |
|     13 | 3762 | `			break;` |
|      - | 3763 | `		}` |
|     11 | 3764 | `		nSeg = nEnd + 1;` |
|      1 | 3765 | `	}` |
|     13 | 3766 | `	ph7_value_bool(pV,bBase64);` |
|     13 | 3767 | `	ph7_array_add_strkey_elem(pArr,"base64",pV);` |
|     13 | 3768 | `}` |
|      - | 3769 | `/*` |
|      - | 3770 | ` * array stream_get_meta_data(resource $stream)` |
|      - | 3771 | ` *` |
|      - | 3772 | ` * php's own key set, in php's own order. What used to be here answered a` |
|      - | 3773 | `` * best-effort shape: `mode` and `uri` did not exist at all (so the documented`` |
|      - | 3774 | `` * way to ask a handle what FILE it is on was an `Undefined array key` and`` |
|      - | 3775 | `` * NULL), `eof` was hardcoded FALSE (a `while (!$m['eof'])` loop never ended),`` |
|      - | 3776 | `` * `unread_bytes` was hardcoded 0, and `wrapper_type`/`stream_type` were both`` |
|      - | 3777 | ` * PHL's internal device name rather than php's two different labels.` |
|      - | 3778 | ` */` |
|     82 | 3779 | `PH7_PRIVATE int PH7_builtin_stream_get_meta_data(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      4 | 3780 | `{` |
|      - | 3781 | `	const char *zWrapper,*zStream;` |
|      - | 3782 | `	io_private *pDev;` |
|      - | 3783 | `	ph7_value *pArr,*pV;` |
|      - | 3784 | `	sxu32 nUnread;` |
|     86 | 3785 | `	if( nArg < 1 \|\| !ph7_value_is_resource(apArg[0]) ){` |
|    ! 0 | 3786 | `		ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"Expecting an IO handle");` |
|    ! 0 | 3787 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 3788 | `		return PH7_OK;` |
|      - | 3789 | `	}` |
|     86 | 3790 | `	pDev = (io_private *)ph7_value_to_resource(apArg[0]);` |
|     86 | 3791 | `	if( IO_PRIVATE_INVALID(pDev) ){` |
|    ! 0 | 3792 | `		ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"Expecting an IO handle");` |
|    ! 0 | 3793 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 3794 | `		return PH7_OK;` |
|      - | 3795 | `	}` |
|     86 | 3796 | `	pArr = ph7_context_new_array(pCtx);` |
|     86 | 3797 | `	pV = ph7_context_new_scalar(pCtx);` |
|     86 | 3798 | `	if( pArr == 0 \|\| pV == 0 ){` |
|    ! 0 | 3799 | `		ph7_result_null(pCtx);` |
|    ! 0 | 3800 | `		return PH7_OK;` |
|      - | 3801 | `	}` |
|     86 | 3802 | `	IoPrivateStreamLabels(pDev,&zWrapper,&zStream);` |
|      - | 3803 | `	/* Sample this BEFORE the eof probe below: php answers eof from state it` |
|      - | 3804 | ``	 * already has and never reads ahead for it, so its `unread_bytes` counts`` |
|      - | 3805 | `	 * only what the SCRIPT's own reads left buffered. */` |
|     86 | 3806 | `	nUnread = StreamAheadBytes(pDev);` |
|     86 | 3807 | `	if( is_data_stream(pDev->pStream) ){` |
|      - | 3808 | `		/* A device that answers metadata of its OWN replaces php's three` |
|      - | 3809 | `		 * defaults rather than adding to them: data:// (and php://temp, which` |
|      - | 3810 | `		 * simply has none) report no timed_out/blocked/eof at all. */` |
|     13 | 3811 | `		IoPrivateDataMeta(pCtx,pDev,pArr,pV);` |
|     13 | 3812 | `		ph7_value_reset_string_cursor(pV);` |
|     77 | 3813 | `	}else if( is_php_stream(pDev->pStream)` |
|     46 | 3814 | `	       && PH7_PhpStreamKind(pDev->pHandle) == PH7_IO_STREAM_MEMORY` |
|     22 | 3815 | `	       && SyStrncmp(zStream,"TEMP",sizeof("TEMP")) == 0 ){` |
|      - | 3816 | `		/* php://temp: same rule, no keys of its own. */` |
|      2 | 3817 | `	}else{` |
|     72 | 3818 | `		ph7_value_bool(pV,pDev->bTimedOut != 0);` |
|     72 | 3819 | `		ph7_array_add_strkey_elem(pArr,"timed_out",pV);` |
|      - | 3820 | `		/* A stream php cannot put in non-blocking mode always reports blocked;` |
|      - | 3821 | `		 * bNonBlock is only ever set for one that CAN. */` |
|     72 | 3822 | `		ph7_value_bool(pV,pDev->bNonBlock == 0);` |
|     72 | 3823 | `		ph7_array_add_strkey_elem(pArr,"blocked",pV);` |
|      - | 3824 | `		/* The read-ahead this performs is feof()'s own, so a script that asks` |
|      - | 3825 | `		 * for the metadata and then reads sees every byte. */` |
|     72 | 3826 | `		ph7_value_bool(pV,IoPrivateAtEof(pDev) != 0);` |
|     72 | 3827 | `		ph7_array_add_strkey_elem(pArr,"eof",pV);` |
|      - | 3828 | `	}` |
|      - | 3829 | `	{` |
|     86 | 3830 | `		ph7_class_instance *pObj = IoPrivateUwrapObject(pDev);` |
|     86 | 3831 | `		if( pObj ){` |
|      - | 3832 | `			/* php hands the wrapper INSTANCE back, which is the only way a` |
|      - | 3833 | `			 * script can reach the object serving an open userland stream. */` |
|      - | 3834 | `			ph7_value sObj;` |
|      5 | 3835 | `			PH7_MemObjInit(pCtx->pVm,&sObj);` |
|      5 | 3836 | `			sObj.x.pOther = pObj;` |
|      5 | 3837 | `			sObj.iFlags = MEMOBJ_OBJ;` |
|      5 | 3838 | `			ph7_array_add_strkey_elem(pArr,"wrapper_data",&sObj);` |
|      2 | 3839 | `		}` |
|      - | 3840 | `	}` |
|     86 | 3841 | `	if( zWrapper ){` |
|     67 | 3842 | `		ph7_value_string(pV,zWrapper,-1);` |
|     67 | 3843 | `		ph7_array_add_strkey_elem(pArr,"wrapper_type",pV);` |
|     67 | 3844 | `		ph7_value_reset_string_cursor(pV);` |
|     32 | 3845 | `	}` |
|     86 | 3846 | `	ph7_value_string(pV,zStream,-1);` |
|     86 | 3847 | `	ph7_array_add_strkey_elem(pArr,"stream_type",pV);` |
|     86 | 3848 | `	ph7_value_reset_string_cursor(pV);` |
|     86 | 3849 | `	ph7_value_string(pV,pDev->zMode,-1);` |
|     86 | 3850 | `	ph7_array_add_strkey_elem(pArr,"mode",pV);` |
|     86 | 3851 | `	ph7_value_reset_string_cursor(pV);` |
|      - | 3852 | `	/* Bytes already pulled off the device and not yet handed to the script —` |
|      - | 3853 | `	 * php's own writepos-minus-readpos, which was hardcoded 0. */` |
|     86 | 3854 | `	ph7_value_int64(pV,(ph7_int64)nUnread);` |
|     86 | 3855 | `	ph7_array_add_strkey_elem(pArr,"unread_bytes",pV);` |
|      - | 3856 | `	{` |
|      - | 3857 | `		/* php answers this from what the handle actually SITS ON, not from what` |
|      - | 3858 | `		 * the device could do: php://stdout is seekable into a file and not` |
|      - | 3859 | `		 * down a pipe, php://output never is, and a pipe is not. Ask the` |
|      - | 3860 | `		 * descriptor first and the device second; a USERLAND wrapper is php's` |
|      - | 3861 | `		 * one exception — its ops always carry a seek, so php always says yes. */` |
|     86 | 3862 | `		int bSeekable = pDev->pStream != 0 && pDev->pStream->xSeek != 0;` |
|     86 | 3863 | `		if( pDev->bDir ){` |
|      - | 3864 | `			/* php's directory ops carry a rewind, so a dir handle is seekable —` |
|      - | 3865 | `			 * and asking the FILE device where it is would hand lseek() the` |
|      - | 3866 | `			 * DIR* this handle stores where a file stores its descriptor. */` |
|      3 | 3867 | `			bSeekable = 1;` |
|     85 | 3868 | `		}else if( bSeekable && !IoPrivateIsUwrap(pDev->pStream) ){` |
|     61 | 3869 | `			int rcSeek = PH7_StreamHandleCanSeek(pDev);` |
|     61 | 3870 | `			if( rcSeek >= 0 ){` |
|     31 | 3871 | `				bSeekable = rcSeek;` |
|     45 | 3872 | `			}else if( pDev->pStream->xTell != 0 ){` |
|     30 | 3873 | `				bSeekable = pDev->pStream->xTell(pDev->pHandle) >= 0;` |
|     14 | 3874 | `			}` |
|     29 | 3875 | `		}` |
|     86 | 3876 | `		ph7_value_bool(pV,bSeekable);` |
|      - | 3877 | `	}` |
|     86 | 3878 | `	ph7_array_add_strkey_elem(pArr,"seekable",pV);` |
|     86 | 3879 | `	if( SyBlobLength(&pDev->sUri) > 0 ){` |
|      - | 3880 | `		/* php keeps the path exactly as the opener received it — a relative` |
|      - | 3881 | `		 * one stays relative — and omits the key for a stream that has none. */` |
|     75 | 3882 | `		ph7_value_string(pV,(const char *)SyBlobData(&pDev->sUri),(int)SyBlobLength(&pDev->sUri));` |
|     75 | 3883 | `		ph7_array_add_strkey_elem(pArr,"uri",pV);` |
|     75 | 3884 | `		ph7_value_reset_string_cursor(pV);` |
|     36 | 3885 | `	}` |
|     86 | 3886 | `	ph7_result_value(pCtx,pArr);` |
|     86 | 3887 | `	return PH7_OK;` |
|     45 | 3888 | `}` |
|      - | 3889 | `/*` |
|      - | 3890 | ` * ---------------------------------------------------------------------------` |
|      - | 3891 | ` * Stream contexts (stream_context_create and the accessor family).` |
|      - | 3892 | ` *` |
|      - | 3893 | `` * php's context is a `stream-context` RESOURCE holding two things: a`` |
|      - | 3894 | `` * wrapper => option => value map, and the `notification` parameter. Both`` |
|      - | 3895 | ` * levels keep INSERTION order, which is the order stream_context_get_options()` |
|      - | 3896 | ` * answers in, so the store is a real nested array rather than a flat table.` |
|      - | 3897 | ` *` |
|      - | 3898 | ` * A PHL resource is a bare void*, so the struct opens with an io_private` |
|      - | 3899 | ` * header carrying its own magic (the shape proc_open()'s handle already uses)` |
|      - | 3900 | ` * and the VM owns every one it hands out.` |
|      - | 3901 | ` * ---------------------------------------------------------------------------` |
|      - | 3902 | ` */` |
|      - | 3903 | `/* Allocate one context, chained on the VM registry. */` |
|    284 | 3904 | `static phl_stream_ctx * StreamCtxNew(ph7_vm *pVm)` |
|      5 | 3905 | `{` |
|      - | 3906 | `	phl_stream_ctx *pRes;` |
|    289 | 3907 | `	if( pVm == 0 ){` |
|    ! 0 | 3908 | `		return 0;` |
|      - | 3909 | `	}` |
|    289 | 3910 | `	pRes = (phl_stream_ctx *)SyMemBackendAlloc(&pVm->sAllocator,sizeof(phl_stream_ctx));` |
|    289 | 3911 | `	if( pRes == 0 ){` |
|    ! 0 | 3912 | `		return 0;` |
|      - | 3913 | `	}` |
|    289 | 3914 | `	SyZero(pRes,sizeof(phl_stream_ctx));` |
|    289 | 3915 | `	pRes->base.iMagic = STREAM_CTX_MAGIC;` |
|    289 | 3916 | `	pRes->pVm = pVm;` |
|    289 | 3917 | `	pRes->pOptions = ph7_new_array(pVm);` |
|    289 | 3918 | `	if( pRes->pOptions == 0 ){` |
|    ! 0 | 3919 | `		SyMemBackendFree(&pVm->sAllocator,pRes);` |
|    ! 0 | 3920 | `		return 0;` |
|      - | 3921 | `	}` |
|    289 | 3922 | `	pRes->pNext = (phl_stream_ctx *)pVm->pStreamCtx;` |
|    289 | 3923 | `	pVm->pStreamCtx = (void *)pRes;` |
|    289 | 3924 | `	return pRes;` |
|    147 | 3925 | `}` |
|      - | 3926 | `/*` |
|      - | 3927 | ` * The context behind a ph7_value, or 0 when the value is not one. The magic` |
|      - | 3928 | ` * probe is the same in-bounds one every resource here answers to.` |
|      - | 3929 | ` */` |
|    134 | 3930 | `PH7_PRIVATE phl_stream_ctx * PH7_StreamCtxFromValue(ph7_value *pVal)` |
|      5 | 3931 | `{` |
|      - | 3932 | `	phl_stream_ctx *pRes;` |
|    139 | 3933 | `	if( pVal == 0 \|\| !ph7_value_is_resource(pVal) ){` |
|    ! 0 | 3934 | `		return 0;` |
|      - | 3935 | `	}` |
|    139 | 3936 | `	pRes = (phl_stream_ctx *)pVal->x.pOther;` |
|    139 | 3937 | `	if( pRes == 0 \|\| pRes->base.iMagic != STREAM_CTX_MAGIC ){` |
|     49 | 3938 | `		return 0;` |
|      - | 3939 | `	}` |
|     92 | 3940 | `	return pRes;` |
|     72 | 3941 | `}` |
|      - | 3942 | `/*` |
|      - | 3943 | ` * The per-VM DEFAULT context. php creates it on demand — the first` |
|      - | 3944 | ` * stream_context_get_default()/set_default() call — and every opener that was` |
|      - | 3945 | ` * handed no context of its own falls back to it.` |
|      - | 3946 | ` */` |
|  67265 | 3947 | `PH7_PRIVATE phl_stream_ctx * PH7_StreamCtxDefault(ph7_vm *pVm)` |
|      5 | 3948 | `{` |
|  67270 | 3949 | `	if( pVm == 0 ){` |
|    ! 0 | 3950 | `		return 0;` |
|      - | 3951 | `	}` |
|  67270 | 3952 | `	if( pVm->pDefaultCtx == 0 ){` |
|    233 | 3953 | `		pVm->pDefaultCtx = (void *)StreamCtxNew(pVm);` |
|    114 | 3954 | `	}` |
|  67270 | 3955 | `	return (phl_stream_ctx *)pVm->pDefaultCtx;` |
|  33637 | 3956 | `}` |
|      - | 3957 | `/*` |
|      - | 3958 | ` * Drop every context this VM created. Called from PH7_VmReset, so a reused VM` |
|      - | 3959 | ` * (the -S server's) does not carry one request's default context into the next.` |
|      - | 3960 | ` */` |
|     16 | 3961 | `PH7_PRIVATE void PH7_StreamCtxVmReset(ph7_vm *pVm)` |
|    ! 0 | 3962 | `{` |
|      - | 3963 | `	phl_stream_ctx *pRes;` |
|     16 | 3964 | `	if( pVm == 0 ){` |
|    ! 0 | 3965 | `		return;` |
|      - | 3966 | `	}` |
|     16 | 3967 | `	pRes = (phl_stream_ctx *)pVm->pStreamCtx;` |
|     24 | 3968 | `	while( pRes ){` |
|      8 | 3969 | `		phl_stream_ctx *pNext = pRes->pNext;` |
|      8 | 3970 | `		if( pRes->pOptions ){` |
|      8 | 3971 | `			ph7_release_value(pVm,pRes->pOptions);` |
|      4 | 3972 | `		}` |
|      8 | 3973 | `		if( pRes->pNotify ){` |
|    ! 0 | 3974 | `			ph7_release_value(pVm,pRes->pNotify);` |
|    ! 0 | 3975 | `		}` |
|      - | 3976 | `		/* Any ph7_value still naming this pointer must stop reporting a live` |
|      - | 3977 | `		 * context, so clear the magic before the memory goes back. */` |
|      8 | 3978 | `		pRes->base.iMagic = 0;` |
|      8 | 3979 | `		SyMemBackendFree(&pVm->sAllocator,pRes);` |
|      8 | 3980 | `		pRes = pNext;` |
|    ! 0 | 3981 | `	}` |
|     16 | 3982 | `	pVm->pStreamCtx = 0;` |
|     16 | 3983 | `	pVm->pDefaultCtx = 0;` |
|      - | 3984 | `	/* Whatever an interrupted open left armed named one of those. */` |
|     16 | 3985 | `	pVm->pOpenCtx = 0;` |
|      8 | 3986 | `}` |
|      - | 3987 | `/* The live element of pArray under pKey, or 0 when there is none. */` |
|    114 | 3988 | `static ph7_value * StreamCtxFetch(ph7_value *pArray,ph7_value *pKey)` |
|      4 | 3989 | `{` |
|      - | 3990 | `	ph7_hashmap_node *pNode;` |
|    118 | 3991 | `	if( pArray == 0 \|\| (pArray->iFlags & MEMOBJ_HASHMAP) == 0 ){` |
|    ! 0 | 3992 | `		return 0;` |
|      - | 3993 | `	}` |
|    118 | 3994 | `	if( PH7_HashmapLookup((ph7_hashmap *)pArray->x.pOther,pKey,&pNode) != SXRET_OK ){` |
|     54 | 3995 | `		return 0;` |
|      - | 3996 | `	}` |
|     68 | 3997 | `	return (ph7_value *)SySetAt(&pArray->pVm->aMemObj,pNode->nValIdx);` |
|     61 | 3998 | `}` |
|      - | 3999 | `/*` |
|      - | 4000 | ` * Store one option. The wrapper's sub-array is created on first use; an` |
|      - | 4001 | ` * existing one may be SHARED with the script array it was stored from, so it` |
|      - | 4002 | ` * is separated first — otherwise setting an option would write through into` |
|      - | 4003 | ` * the caller's own array.` |
|      - | 4004 | ` */` |
|     64 | 4005 | `static int StreamCtxSetOption(phl_stream_ctx *pRes,ph7_value *pWrapper,ph7_value *pName,ph7_value *pValue)` |
|      4 | 4006 | `{` |
|      - | 4007 | `	ph7_value sKey,sName,sVal;` |
|      - | 4008 | `	ph7_value *pSub;` |
|      - | 4009 | `	ph7_hashmap *pMap;` |
|     68 | 4010 | `	if( pRes == 0 \|\| pRes->pOptions == 0 \|\| pWrapper == 0 \|\| pName == 0 \|\| pValue == 0 ){` |
|    ! 0 | 4011 | `		return -1;` |
|      - | 4012 | `	}` |
|      - | 4013 | `	/* Every insertion below can reserve a memory object, which GROWS (and` |
|      - | 4014 | `	 * therefore moves) pVm->aMemObj — and all three arguments may point into` |
|      - | 4015 | `	 * it. Snapshot the structs first: a shallow copy is a safe insertion` |
|      - | 4016 | `	 * source, since the referent and the heap-resident blob survive the move. */` |
|     68 | 4017 | `	sKey = *pWrapper; pWrapper = &sKey;` |
|     68 | 4018 | `	sName = *pName;   pName = &sName;` |
|     68 | 4019 | `	sVal = *pValue;   pValue = &sVal;` |
|     68 | 4020 | `	pSub = StreamCtxFetch(pRes->pOptions,pWrapper);` |
|     68 | 4021 | `	if( pSub == 0 \|\| (pSub->iFlags & MEMOBJ_HASHMAP) == 0 ){` |
|     54 | 4022 | `		ph7_value *pFresh = ph7_new_array(pRes->pVm);` |
|     54 | 4023 | `		if( pFresh == 0 ){` |
|    ! 0 | 4024 | `			return -1;` |
|      - | 4025 | `		}` |
|     54 | 4026 | `		if( PH7_HashmapInsert((ph7_hashmap *)pRes->pOptions->x.pOther,pWrapper,pFresh) != SXRET_OK ){` |
|    ! 0 | 4027 | `			ph7_release_value(pRes->pVm,pFresh);` |
|    ! 0 | 4028 | `			return -1;` |
|      - | 4029 | `		}` |
|     54 | 4030 | `		ph7_release_value(pRes->pVm,pFresh);` |
|     54 | 4031 | `		pSub = StreamCtxFetch(pRes->pOptions,pWrapper);` |
|     54 | 4032 | `		if( pSub == 0 \|\| (pSub->iFlags & MEMOBJ_HASHMAP) == 0 ){` |
|    ! 0 | 4033 | `			return -1;` |
|      - | 4034 | `		}` |
|     25 | 4035 | `	}` |
|     68 | 4036 | `	pMap = PH7_HashmapCowSeparate(pRes->pVm,pSub);` |
|     68 | 4037 | `	if( pMap == 0 ){` |
|    ! 0 | 4038 | `		return -1;` |
|      - | 4039 | `	}` |
|     68 | 4040 | `	return PH7_HashmapInsert(pMap,pName,pValue) == SXRET_OK ? 0 : -1;` |
|     36 | 4041 | `}` |
|      - | 4042 | `/*` |
|      - | 4043 | ` * One wrapper option by name, or 0 when the context does not carry it. This is` |
|      - | 4044 | ` * the read side every consumer (the socket transports) asks through.` |
|      - | 4045 | ` */` |
|    264 | 4046 | `PH7_PRIVATE ph7_value * PH7_StreamCtxOption(phl_stream_ctx *pRes,const char *zWrapper,const char *zOption)` |
|      4 | 4047 | `{` |
|      - | 4048 | `	ph7_value *pSub;` |
|    268 | 4049 | `	if( pRes == 0 \|\| pRes->pOptions == 0 ){` |
|    ! 0 | 4050 | `		return 0;` |
|      - | 4051 | `	}` |
|    268 | 4052 | `	pSub = ph7_array_fetch(pRes->pOptions,zWrapper,-1);` |
|    268 | 4053 | `	if( pSub == 0 \|\| (pSub->iFlags & MEMOBJ_HASHMAP) == 0 ){` |
|    188 | 4054 | `		return 0;` |
|      - | 4055 | `	}` |
|     82 | 4056 | `	return ph7_array_fetch(pSub,zOption,-1);` |
|    136 | 4057 | `}` |
|      - | 4058 | `/*` |
|      - | 4059 | ` * php's parse_context_options: every entry must be wrappername => array, and a` |
|      - | 4060 | ` * non-array value — or an INTEGER key, which has no wrapper name at all — is` |
|      - | 4061 | ` * the ValueError below. An integer key one level DOWN has no option name, and` |
|      - | 4062 | ` * php drops that entry in silence rather than refusing the call.` |
|      - | 4063 | ` * Returns 0, or -1 once the exception has been raised.` |
|      - | 4064 | ` */` |
|     58 | 4065 | `static int StreamCtxParseOptions(ph7_context *pCtx,phl_stream_ctx *pRes,ph7_value *pOptions)` |
|      4 | 4066 | `{` |
|      - | 4067 | `	ph7_hashmap *pMap;` |
|      - | 4068 | `	ph7_hashmap_node *pEntry;` |
|     62 | 4069 | `	if( pOptions == 0 \|\| (pOptions->iFlags & MEMOBJ_HASHMAP) == 0 ){` |
|    ! 0 | 4070 | `		return 0;` |
|      - | 4071 | `	}` |
|     62 | 4072 | `	pMap = (ph7_hashmap *)pOptions->x.pOther;` |
|     62 | 4073 | `	pMap->pCur = pMap->pFirst;` |
|    112 | 4074 | `	while( (pEntry = PH7_HashmapGetNextEntry(pMap)) != 0 ){` |
|      - | 4075 | `		ph7_value sKey;` |
|      - | 4076 | `		ph7_value *pVal;` |
|      - | 4077 | `		int bBad;` |
|     58 | 4078 | `		PH7_MemObjInit(pRes->pVm,&sKey);` |
|     58 | 4079 | `		PH7_HashmapExtractNodeKey(pEntry,&sKey);` |
|     58 | 4080 | `		pVal = HashmapExtractNodeValue(pEntry);` |
|     83 | 4081 | `		bBad = ( (sKey.iFlags & MEMOBJ_STRING) == 0 \|\| pVal == 0` |
|     80 | 4082 | `		      \|\| (pVal->iFlags & MEMOBJ_HASHMAP) == 0 );` |
|     58 | 4083 | `		if( bBad ){` |
|      5 | 4084 | `			PH7_MemObjRelease(&sKey);` |
|      5 | 4085 | `			PH7_VmThrowException(pCtx,"ValueError",` |
|      - | 4086 | `				"Options should have the form [\"wrappername\"][\"optionname\"] = $value");` |
|      5 | 4087 | `			return -1;` |
|      - | 4088 | `		}` |
|      - | 4089 | `		{` |
|     54 | 4090 | `			ph7_hashmap *pSub = (ph7_hashmap *)pVal->x.pOther;` |
|      - | 4091 | `			ph7_hashmap_node *pOpt;` |
|     54 | 4092 | `			pSub->pCur = pSub->pFirst;` |
|    112 | 4093 | `			while( (pOpt = PH7_HashmapGetNextEntry(pSub)) != 0 ){` |
|      - | 4094 | `				ph7_value sName;` |
|      - | 4095 | `				ph7_value *pOptVal;` |
|     62 | 4096 | `				PH7_MemObjInit(pRes->pVm,&sName);` |
|     62 | 4097 | `				PH7_HashmapExtractNodeKey(pOpt,&sName);` |
|     62 | 4098 | `				pOptVal = HashmapExtractNodeValue(pOpt);` |
|     62 | 4099 | `				if( (sName.iFlags & MEMOBJ_STRING) && pOptVal ){` |
|     60 | 4100 | `					StreamCtxSetOption(pRes,&sKey,&sName,pOptVal);` |
|     28 | 4101 | `				}` |
|     62 | 4102 | `				PH7_MemObjRelease(&sName);` |
|      4 | 4103 | `			}` |
|      - | 4104 | `		}` |
|     54 | 4105 | `		PH7_MemObjRelease(&sKey);` |
|      4 | 4106 | `	}` |
|     58 | 4107 | `	return 0;` |
|     33 | 4108 | `}` |
|      - | 4109 | `/*` |
|      - | 4110 | `` * php's parse_context_params: only `notification` and `options` are read, and`` |
|      - | 4111 | ` * anything else in the array is ignored rather than refused. The notification` |
|      - | 4112 | ` * must be callable — php reports the same "must be an array with valid` |
|      - | 4113 | ` * callbacks as values" TypeError the callback taxonomy produces, naming` |
|      - | 4114 | ` * argument #1 whichever function was called.` |
|      - | 4115 | ` * Returns 0, or -1 once a diagnostic has been raised.` |
|      - | 4116 | ` */` |
|     10 | 4117 | `static int StreamCtxParseParams(ph7_context *pCtx,phl_stream_ctx *pRes,ph7_value *pParams,const char *zArgName)` |
|      1 | 4118 | `{` |
|      - | 4119 | `	ph7_value *pVal;` |
|     11 | 4120 | `	if( pParams == 0 \|\| (pParams->iFlags & MEMOBJ_HASHMAP) == 0 ){` |
|    ! 0 | 4121 | `		return 0;` |
|      - | 4122 | `	}` |
|     11 | 4123 | `	pVal = ph7_array_fetch(pParams,"notification",-1);` |
|     11 | 4124 | `	if( pVal ){` |
|      - | 4125 | `		char zBuf[128];` |
|      7 | 4126 | `		const char *zReason = PH7_VmCallableReason(pCtx->pVm,pVal,zBuf,(int)sizeof(zBuf));` |
|      7 | 4127 | `		if( zReason ){` |
|      5 | 4128 | `			return PH7_VmThrowException(pCtx,"TypeError",` |
|      - | 4129 | `				"%s(): Argument #1 (%s) must be an array with valid callbacks as values, %s",` |
|      3 | 4130 | `				ph7_function_name(pCtx),zArgName,zReason) == PH7_OK ? -1 : -1;` |
|      - | 4131 | `		}` |
|      5 | 4132 | `		if( pRes->pNotify == 0 ){` |
|      5 | 4133 | `			pRes->pNotify = ph7_new_scalar(pRes->pVm);` |
|      2 | 4134 | `		}` |
|      5 | 4135 | `		if( pRes->pNotify ){` |
|      5 | 4136 | `			PH7_MemObjStore(pVal,pRes->pNotify);` |
|      2 | 4137 | `		}` |
|      2 | 4138 | `	}` |
|      9 | 4139 | `	pVal = ph7_array_fetch(pParams,"options",-1);` |
|      9 | 4140 | `	if( pVal ){` |
|      5 | 4141 | `		if( (pVal->iFlags & MEMOBJ_HASHMAP) == 0 ){` |
|      - | 4142 | `			/* php's own wording for a params entry it cannot use. */` |
|    ! 0 | 4143 | `			return PH7_VmThrowException(pCtx,"TypeError",` |
|    ! 0 | 4144 | `				"Invalid stream/context parameter") == PH7_OK ? -1 : -1;` |
|      - | 4145 | `		}` |
|      5 | 4146 | `		if( StreamCtxParseOptions(pCtx,pRes,pVal) != 0 ){` |
|    ! 0 | 4147 | `			return -1;` |
|      - | 4148 | `		}` |
|      2 | 4149 | `	}` |
|      9 | 4150 | `	return 0;` |
|      6 | 4151 | `}` |
|      - | 4152 | `/*` |
|      - | 4153 | `` * Resolve the `$stream_or_context` first argument every accessor takes: a`` |
|      - | 4154 | ` * context resource answers itself, and a STREAM answers the context it` |
|      - | 4155 | ` * carries — created on demand for the setters, the way php's does, since a` |
|      - | 4156 | ` * stream opened without one still accepts stream_context_set_option().` |
|      - | 4157 | ` * Raises php's TypeError and returns 0 for anything else.` |
|      - | 4158 | ` */` |
|     70 | 4159 | `static phl_stream_ctx * StreamCtxArg(ph7_context *pCtx,ph7_value *pVal,int bCreate,` |
|      - | 4160 | `	const char *zArgName,int *pbThrew)` |
|      3 | 4161 | `{` |
|      - | 4162 | `	phl_stream_ctx *pRes;` |
|      - | 4163 | `	io_private *pDev;` |
|     73 | 4164 | `	*pbThrew = 1;` |
|     73 | 4165 | `	if( !ph7_value_is_resource(pVal) ){` |
|      4 | 4166 | `		PH7_VmThrowException(pCtx,"TypeError",` |
|      - | 4167 | `			"%s(): Argument #1 (%s) must be of type resource, %s given",` |
|      1 | 4168 | `			ph7_function_name(pCtx),zArgName,ph7_type_name(pVal));` |
|      3 | 4169 | `		return 0;` |
|      - | 4170 | `	}` |
|     71 | 4171 | `	pRes = PH7_StreamCtxFromValue(pVal);` |
|     71 | 4172 | `	if( pRes ){` |
|     57 | 4173 | `		*pbThrew = 0;` |
|     57 | 4174 | `		return pRes;` |
|      - | 4175 | `	}` |
|     16 | 4176 | `	pDev = (io_private *)ph7_value_to_resource(pVal);` |
|     16 | 4177 | `	if( IO_PRIVATE_INVALID(pDev) ){` |
|      - | 4178 | `		/* A closed handle, a process handle, anything that is neither: php` |
|      - | 4179 | `		 * refuses the call rather than answering an empty option set. */` |
|      4 | 4180 | `		PH7_VmThrowException(pCtx,"TypeError",` |
|      - | 4181 | `			"%s(): Argument #1 (%s) must be a valid stream/context",` |
|      1 | 4182 | `			ph7_function_name(pCtx),zArgName);` |
|      3 | 4183 | `		return 0;` |
|      - | 4184 | `	}` |
|     14 | 4185 | `	*pbThrew = 0;` |
|     14 | 4186 | `	if( pDev->pCtxRes == 0 && bCreate ){` |
|      3 | 4187 | `		pDev->pCtxRes = (void *)StreamCtxNew(pCtx->pVm);` |
|      1 | 4188 | `	}` |
|     14 | 4189 | `	return (phl_stream_ctx *)pDev->pCtxRes;` |
|     38 | 4190 | `}` |
|      - | 4191 | `/*` |
|      - | 4192 | `` * The `$context` argument sixteen rows of aBuiltinSig[] declare and no C body`` |
|      - | 4193 | `` * used to read. php's parameter is `?resource $context = null` and its rules`` |
|      - | 4194 | ` * are: a resource that is NOT a stream-context is refused outright, anything` |
|      - | 4195 | ` * else non-null is the ordinary type refusal, and NULL means the DEFAULT` |
|      - | 4196 | ` * context — which php creates on demand, so an opener never runs without one.` |
|      - | 4197 | ` *` |
|      - | 4198 | ` * bNoDefault is FILE_NO_DEFAULT_CONTEXT, the flag file()/file_get_contents()/` |
|      - | 4199 | ` * file_put_contents() carry to mean exactly "and do not fall back to it".` |
|      - | 4200 | ` * Returns 0 with *pbThrew set once a diagnostic has been raised.` |
|      - | 4201 | ` */` |
|  67317 | 4202 | `PH7_PRIVATE phl_stream_ctx * PH7_StreamCtxFromArg(ph7_context *pCtx,int nArg,ph7_value **apArg,` |
|      - | 4203 | `	int iArg,const char *zArgName,int bNoDefault,int *pbThrew)` |
|      5 | 4204 | `{` |
|      - | 4205 | `	phl_stream_ctx *pRes;` |
|  67322 | 4206 | `	*pbThrew = 0;` |
|  67322 | 4207 | `	if( iArg < nArg && apArg[iArg] && !ph7_value_is_null(apArg[iArg]) ){` |
|     76 | 4208 | `		if( !ph7_value_is_resource(apArg[iArg]) ){` |
|      7 | 4209 | `			*pbThrew = 1;` |
|     10 | 4210 | `			PH7_VmThrowException(pCtx,"TypeError",` |
|      - | 4211 | `				"%s(): Argument #%d (%s) must be of type resource or null, %s given",` |
|      6 | 4212 | `				ph7_function_name(pCtx),iArg + 1,zArgName,ph7_type_name(apArg[iArg]));` |
|      7 | 4213 | `			return 0;` |
|      - | 4214 | `		}` |
|     70 | 4215 | `		pRes = PH7_StreamCtxFromValue(apArg[iArg]);` |
|     70 | 4216 | `		if( pRes == 0 ){` |
|      - | 4217 | `			/* php names the RESOURCE it wanted rather than the argument here. */` |
|     34 | 4218 | `			*pbThrew = 1;` |
|     50 | 4219 | `			PH7_VmThrowException(pCtx,"TypeError",` |
|      - | 4220 | `				"%s(): supplied resource is not a valid Stream-Context resource",` |
|     16 | 4221 | `				ph7_function_name(pCtx));` |
|     34 | 4222 | `			return 0;` |
|      - | 4223 | `		}` |
|     37 | 4224 | `		return pRes;` |
|      - | 4225 | `	}` |
|  67250 | 4226 | `	return bNoDefault ? 0 : PH7_StreamCtxDefault(pCtx->pVm);` |
|  33663 | 4227 | `}` |
|      - | 4228 | `/*` |
|      - | 4229 | ` * Arm the context the NEXT open is to run under. PH7_StreamOpenHandle consumes` |
|      - | 4230 | ` * and clears it, so the slot describes exactly one open and a caller that never` |
|      - | 4231 | ` * set it finds nothing armed.` |
|      - | 4232 | ` */` |
|  25070 | 4233 | `PH7_PRIVATE void PH7_StreamCtxArm(ph7_vm *pVm,phl_stream_ctx *pRes)` |
|      5 | 4234 | `{` |
|  25075 | 4235 | `	if( pVm ){` |
|  25075 | 4236 | `		pVm->pOpenCtx = (void *)pRes;` |
|  12535 | 4237 | `	}` |
|  25075 | 4238 | `}` |
|      - | 4239 | `/*` |
|      - | 4240 | ` * resource stream_context_create(?array $options = null, ?array $params = null)` |
|      - | 4241 | ` */` |
|     54 | 4242 | `PH7_PRIVATE int PH7_builtin_stream_context_create(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      4 | 4243 | `{` |
|     58 | 4244 | `	phl_stream_ctx *pRes = StreamCtxNew(pCtx->pVm);` |
|     58 | 4245 | `	if( pRes == 0 ){` |
|    ! 0 | 4246 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 4247 | `		return PH7_OK;` |
|      - | 4248 | `	}` |
|     58 | 4249 | `	if( nArg > 0 && ph7_value_is_array(apArg[0]) ){` |
|     47 | 4250 | `		if( StreamCtxParseOptions(pCtx,pRes,apArg[0]) != 0 ){` |
|      5 | 4251 | `			return PH7_OK;` |
|      - | 4252 | `		}` |
|     20 | 4253 | `	}` |
|     54 | 4254 | `	if( nArg > 1 && ph7_value_is_array(apArg[1]) ){` |
|      - | 4255 | ``		/* php names argument #1 ($options) even for a bad `notification` that`` |
|      - | 4256 | `		 * arrived through $params — the error is raised against a hardcoded` |
|      - | 4257 | `		 * position, and a test that asserts the message would see it. */` |
|      9 | 4258 | `		if( StreamCtxParseParams(pCtx,pRes,apArg[1],"$options") != 0 ){` |
|      3 | 4259 | `			return PH7_OK;` |
|      - | 4260 | `		}` |
|      3 | 4261 | `	}` |
|     52 | 4262 | `	ph7_result_resource(pCtx,pRes);` |
|     52 | 4263 | `	return PH7_OK;` |
|     31 | 4264 | `}` |
|      - | 4265 | `/*` |
|      - | 4266 | ` * array stream_context_get_options(resource $stream_or_context)` |
|      - | 4267 | ` */` |
|     48 | 4268 | `PH7_PRIVATE int PH7_builtin_stream_context_get_options(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      3 | 4269 | `{` |
|      - | 4270 | `	phl_stream_ctx *pRes;` |
|      - | 4271 | `	int bThrew;` |
|     51 | 4272 | `	if( nArg < 1 ){` |
|    ! 0 | 4273 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 4274 | `		return PH7_OK;` |
|      - | 4275 | `	}` |
|      - | 4276 | `	/* A live stream that was never given a context answers the EMPTY option set` |
|      - | 4277 | `	 * rather than refusing the call, so nothing is created here. */` |
|     51 | 4278 | `	pRes = StreamCtxArg(pCtx,apArg[0],FALSE,"$stream_or_context",&bThrew);` |
|     51 | 4279 | `	if( bThrew ){` |
|      5 | 4280 | `		return PH7_OK;` |
|      - | 4281 | `	}` |
|     47 | 4282 | `	if( pRes == 0 ){` |
|      8 | 4283 | `		ph7_value *pArr = ph7_context_new_array(pCtx);` |
|      8 | 4284 | `		if( pArr == 0 ){` |
|    ! 0 | 4285 | `			ph7_result_bool(pCtx,0);` |
|    ! 0 | 4286 | `			return PH7_OK;` |
|      - | 4287 | `		}` |
|      8 | 4288 | `		ph7_result_value(pCtx,pArr);` |
|      8 | 4289 | `		return PH7_OK;` |
|      - | 4290 | `	}` |
|     41 | 4291 | `	ph7_result_value(pCtx,pRes->pOptions);` |
|     41 | 4292 | `	return PH7_OK;` |
|     27 | 4293 | `}` |
|      - | 4294 | `/*` |
|      - | 4295 | ` * bool stream_context_set_option(resource $context, string $wrapper, string $option_name, mixed $value)` |
|      - | 4296 | ` *` |
|      - | 4297 | ` * php also accepts the two-argument (context, options-array) spelling and` |
|      - | 4298 | ` * DEPRECATES it in 8.3 — §10 refuses what php deprecates, so an array in` |
|      - | 4299 | ` * argument #2 is the ordinary string TypeError here and the whole-array form` |
|      - | 4300 | ` * is spelled stream_context_set_options().` |
|      - | 4301 | ` */` |
|      8 | 4302 | `PH7_PRIVATE int PH7_builtin_stream_context_set_option(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      2 | 4303 | `{` |
|      - | 4304 | `	phl_stream_ctx *pRes;` |
|      - | 4305 | `	int bThrew;` |
|     10 | 4306 | `	if( nArg < 4 ){` |
|    ! 0 | 4307 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 4308 | `		return PH7_OK;` |
|      - | 4309 | `	}` |
|     10 | 4310 | `	pRes = StreamCtxArg(pCtx,apArg[0],TRUE,"$context",&bThrew);` |
|     10 | 4311 | `	if( pRes == 0 ){` |
|    ! 0 | 4312 | `		return PH7_OK;` |
|      - | 4313 | `	}` |
|     10 | 4314 | `	ph7_result_bool(pCtx,StreamCtxSetOption(pRes,apArg[1],apArg[2],apArg[3]) == 0);` |
|     10 | 4315 | `	return PH7_OK;` |
|      6 | 4316 | `}` |
|      - | 4317 | `/*` |
|      - | 4318 | ` * bool stream_context_set_options(resource $context, array $options)` |
|      - | 4319 | ` */` |
|      4 | 4320 | `PH7_PRIVATE int PH7_builtin_stream_context_set_options(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      2 | 4321 | `{` |
|      - | 4322 | `	phl_stream_ctx *pRes;` |
|      - | 4323 | `	int bThrew;` |
|      6 | 4324 | `	if( nArg < 2 ){` |
|    ! 0 | 4325 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 4326 | `		return PH7_OK;` |
|      - | 4327 | `	}` |
|      6 | 4328 | `	pRes = StreamCtxArg(pCtx,apArg[0],TRUE,"$context",&bThrew);` |
|      6 | 4329 | `	if( pRes == 0 ){` |
|    ! 0 | 4330 | `		return PH7_OK;` |
|      - | 4331 | `	}` |
|      6 | 4332 | `	if( StreamCtxParseOptions(pCtx,pRes,apArg[1]) != 0 ){` |
|    ! 0 | 4333 | `		return PH7_OK;` |
|      - | 4334 | `	}` |
|      6 | 4335 | `	ph7_result_bool(pCtx,1);` |
|      6 | 4336 | `	return PH7_OK;` |
|      4 | 4337 | `}` |
|      - | 4338 | `/*` |
|      - | 4339 | ` * array stream_context_get_params(resource $stream_or_context)` |
|      - | 4340 | `` *  php answers `notification` (only when one is set) and `options`, in that`` |
|      - | 4341 | ` *  order.` |
|      - | 4342 | ` */` |
|      8 | 4343 | `PH7_PRIVATE int PH7_builtin_stream_context_get_params(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 4344 | `{` |
|      - | 4345 | `	phl_stream_ctx *pRes;` |
|      - | 4346 | `	ph7_value *pArr;` |
|      - | 4347 | `	int bThrew;` |
|      9 | 4348 | `	if( nArg < 1 ){` |
|    ! 0 | 4349 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 4350 | `		return PH7_OK;` |
|      - | 4351 | `	}` |
|      9 | 4352 | `	pRes = StreamCtxArg(pCtx,apArg[0],TRUE,"$stream_or_context",&bThrew);` |
|      9 | 4353 | `	if( pRes == 0 ){` |
|    ! 0 | 4354 | `		return PH7_OK;` |
|      - | 4355 | `	}` |
|      9 | 4356 | `	pArr = ph7_context_new_array(pCtx);` |
|      9 | 4357 | `	if( pArr == 0 ){` |
|    ! 0 | 4358 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 4359 | `		return PH7_OK;` |
|      - | 4360 | `	}` |
|      9 | 4361 | `	if( pRes->pNotify ){` |
|      5 | 4362 | `		ph7_array_add_strkey_elem(pArr,"notification",pRes->pNotify);` |
|      2 | 4363 | `	}` |
|      9 | 4364 | `	ph7_array_add_strkey_elem(pArr,"options",pRes->pOptions);` |
|      9 | 4365 | `	ph7_result_value(pCtx,pArr);` |
|      9 | 4366 | `	return PH7_OK;` |
|      5 | 4367 | `}` |
|      - | 4368 | `/*` |
|      - | 4369 | ` * bool stream_context_set_params(resource $context, array $params)` |
|      - | 4370 | ` */` |
|      2 | 4371 | `PH7_PRIVATE int PH7_builtin_stream_context_set_params(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 4372 | `{` |
|      - | 4373 | `	phl_stream_ctx *pRes;` |
|      - | 4374 | `	int bThrew;` |
|      3 | 4375 | `	if( nArg < 2 ){` |
|    ! 0 | 4376 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 4377 | `		return PH7_OK;` |
|      - | 4378 | `	}` |
|      3 | 4379 | `	pRes = StreamCtxArg(pCtx,apArg[0],TRUE,"$context",&bThrew);` |
|      3 | 4380 | `	if( pRes == 0 ){` |
|    ! 0 | 4381 | `		return PH7_OK;` |
|      - | 4382 | `	}` |
|      3 | 4383 | `	if( StreamCtxParseParams(pCtx,pRes,apArg[1],"$context") != 0 ){` |
|    ! 0 | 4384 | `		return PH7_OK;` |
|      - | 4385 | `	}` |
|      3 | 4386 | `	ph7_result_bool(pCtx,1);` |
|      3 | 4387 | `	return PH7_OK;` |
|      2 | 4388 | `}` |
|      - | 4389 | `/*` |
|      - | 4390 | ` * resource stream_context_get_default(?array $options = null)` |
|      - | 4391 | ` * resource stream_context_set_default(array $options)` |
|      - | 4392 | ` *  Both answer the ONE default context and both MERGE their options into it —` |
|      - | 4393 | ` *  set_default is not a replacement, which is why a second call adds to what` |
|      - | 4394 | ` *  the first left.` |
|      - | 4395 | ` */` |
|     14 | 4396 | `static int StreamCtxDefaultCommon(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      2 | 4397 | `{` |
|     16 | 4398 | `	phl_stream_ctx *pRes = PH7_StreamCtxDefault(pCtx->pVm);` |
|     16 | 4399 | `	if( pRes == 0 ){` |
|    ! 0 | 4400 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 4401 | `		return PH7_OK;` |
|      - | 4402 | `	}` |
|     16 | 4403 | `	if( nArg > 0 && ph7_value_is_array(apArg[0]) ){` |
|      8 | 4404 | `		if( StreamCtxParseOptions(pCtx,pRes,apArg[0]) != 0 ){` |
|    ! 0 | 4405 | `			return PH7_OK;` |
|      - | 4406 | `		}` |
|      3 | 4407 | `	}` |
|     16 | 4408 | `	ph7_result_resource(pCtx,pRes);` |
|     16 | 4409 | `	return PH7_OK;` |
|      9 | 4410 | `}` |
|     10 | 4411 | `PH7_PRIVATE int PH7_builtin_stream_context_get_default(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 4412 | `{` |
|     11 | 4413 | `	return StreamCtxDefaultCommon(pCtx,nArg,apArg);` |
|      1 | 4414 | `}` |
|      4 | 4415 | `PH7_PRIVATE int PH7_builtin_stream_context_set_default(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      2 | 4416 | `{` |
|      6 | 4417 | `	return StreamCtxDefaultCommon(pCtx,nArg,apArg);` |
|      2 | 4418 | `}` |
|      - | 4419 | `/*` |
|      - | 4420 | ` * tcp:// socket stream (fsockopen / stream_socket_client). The handle is a` |
|      - | 4421 | ` * small struct carrying the OS socket plus an EOF latch, so feof() works.` |
|      - | 4422 | ` */` |
|      - | 4423 | `#ifdef PH7_ENABLE_NET` |
|     46 | 4424 | `static ph7_int64 SockStreamData_Read(void *pHandle,void *pBuffer,ph7_int64 nRead)` |
|      3 | 4425 | `{` |
|     49 | 4426 | `	sock_private *pSock = (sock_private *)pHandle;` |
|      - | 4427 | `	int n;` |
|     49 | 4428 | `	if( pSock == 0 \|\| pSock->sock == PH7_NET_INVALID_SOCKET ){` |
|      - | 4429 | `		/* A server asked for neither BIND nor LISTEN has no socket at all, and` |
|      - | 4430 | `		 * php answers false for a read on it — the shape an ERROR takes. */` |
|      6 | 4431 | `		return -1;` |
|      - | 4432 | `	}` |
|     45 | 4433 | `	if( pSock->bEof ){` |
|    ! 0 | 4434 | `		return 0;` |
|      - | 4435 | `	}` |
|     45 | 4436 | `	n = PH7_NetRecv(pSock->sock,pBuffer,(int)nRead,0);` |
|     45 | 4437 | `	if( n == 0 ){` |
|      - | 4438 | `		/* The peer closed: THIS is the end of the stream. */` |
|      9 | 4439 | `		pSock->bEof = 1;` |
|      9 | 4440 | `		return 0;` |
|      - | 4441 | `	}` |
|     37 | 4442 | `	if( n < 0 ){` |
|      - | 4443 | `		/* An error, and since stream_set_blocking()/stream_set_timeout() exist` |
|      - | 4444 | `		 * the ordinary one is EAGAIN — nothing had arrived YET. Latching EOF` |
|      - | 4445 | `		 * here (as this did for every n <= 0, safe only while every socket was` |
|      - | 4446 | `		 * blocking and untimed) made the first empty read close the connection` |
|      - | 4447 | `		 * for good and threw away everything the peer sent afterwards.` |
|      - | 4448 | `		 *` |
|      - | 4449 | `		 * The reader above tells "nothing yet" from "broken" by ERRNO, which a` |
|      - | 4450 | ``		 * Winsock call never touches: without this the `""` a non-blocking read`` |
|      - | 4451 | ``		 * answers and the `timed_out` an expired one reports were both lost on`` |
|      - | 4452 | `		 * Windows, and every such read came back as a plain failure. */` |
|      7 | 4453 | `		errno = PH7_NetWouldBlock() ? EAGAIN : (errno != 0 ? errno : EIO);` |
|      7 | 4454 | `		return -1;` |
|      - | 4455 | `	}` |
|     31 | 4456 | `	return (ph7_int64)n;` |
|     25 | 4457 | `}` |
|     58 | 4458 | `static ph7_int64 SockStreamData_Write(void *pHandle,const void *pBuf,ph7_int64 nWrite)` |
|      3 | 4459 | `{` |
|     61 | 4460 | `	sock_private *pSock = (sock_private *)pHandle;` |
|     61 | 4461 | `	const char *zBuf = (const char *)pBuf;` |
|     61 | 4462 | `	ph7_int64 nSent = 0;` |
|     61 | 4463 | `	if( pSock == 0 ){` |
|    ! 0 | 4464 | `		return -1;` |
|      - | 4465 | `	}` |
|     61 | 4466 | `	if( pSock->sock == PH7_NET_INVALID_SOCKET ){` |
|      - | 4467 | `		/* Nothing to send on, and php answers 0 rather than false for it. */` |
|      6 | 4468 | `		return 0;` |
|      - | 4469 | `	}` |
|      - | 4470 | `	/* php answers the number of bytes it MOVED. This used to hand back` |
|      - | 4471 | `	 * PH7_NetSendAll()'s STATUS — which is 0 on success — so every successful` |
|      - | 4472 | ``	 * `fwrite($sock,$s)` answered 0 bytes written: the `=== strlen($s)` check`` |
|      - | 4473 | `	 * failed on a write that worked, a partial-write retry loop never advanced,` |
|      - | 4474 | `	 * and stream_copy_to_stream() stopped after its first chunk. */` |
|    107 | 4475 | `	while( nSent < nWrite ){` |
|     58 | 4476 | `		int n = PH7_NetSend(pSock->sock,&zBuf[nSent],(int)(nWrite - nSent),0);` |
|     58 | 4477 | `		if( n > 0 ){` |
|     53 | 4478 | `			nSent += n;` |
|     53 | 4479 | `			continue;` |
|      - | 4480 | `		}` |
|      - | 4481 | `		/* Nothing more can go right now. On a non-blocking or timed-out handle` |
|      - | 4482 | `		 * that is php's 0 (or the partial count), and only a write that moved` |
|      - | 4483 | `		 * NO bytes at all for a real error is php's false — which is why the` |
|      - | 4484 | `		 * count is answered here rather than the status. */` |
|      6 | 4485 | `		if( PH7_NetWouldBlock() ){` |
|      4 | 4486 | `			return nSent;` |
|      - | 4487 | `		}` |
|      3 | 4488 | `		pSock->iLastErr = PH7_NetLastError();` |
|      3 | 4489 | `		return nSent > 0 ? nSent : -1;` |
|    ! 0 | 4490 | `	}` |
|     52 | 4491 | `	return nSent;` |
|     30 | 4492 | `}` |
|     78 | 4493 | `static void SockStreamData_Close(void *pHandle)` |
|      3 | 4494 | `{` |
|     81 | 4495 | `	sock_private *pSock = (sock_private *)pHandle;` |
|     81 | 4496 | `	if( pSock == 0 ){` |
|    ! 0 | 4497 | `		return;` |
|      - | 4498 | `	}` |
|     81 | 4499 | `	PH7_NetClose(pSock->sock);` |
|     81 | 4500 | `	SyMemBackendFree(&pSock->pVm->sAllocator,pSock);` |
|     42 | 4501 | `}` |
|      - | 4502 | `/* xOpen for "host:port" (the scheme is already stripped by the device lookup) */` |
|    ! 0 | 4503 | `static int SockStreamData_Open(const char *zName,int iMode,ph7_value *pResource,void ** ppHandle)` |
|    ! 0 | 4504 | `{` |
|      - | 4505 | `	sock_private *pSock;` |
|      - | 4506 | `	ph7_socket sock;` |
|      - | 4507 | `	char zHost[256];` |
|      - | 4508 | `	const char *zColon;` |
|    ! 0 | 4509 | `	int iPort = 0,iErrno = 0;` |
|    ! 0 | 4510 | `	const char *zErr = "";` |
|    ! 0 | 4511 | `	ph7_vm *pVm = pResource ? pResource->pVm : 0;` |
|    ! 0 | 4512 | `	SXUNUSED(iMode);` |
|    ! 0 | 4513 | `	if( pVm == 0 ){` |
|    ! 0 | 4514 | `		return -1;` |
|      - | 4515 | `	}` |
|    ! 0 | 4516 | `	zColon = SyStrlen(zName) ? &zName[SyStrlen(zName)-1] : zName;` |
|    ! 0 | 4517 | `	while( zColon > zName && zColon[0] != ':' ){` |
|    ! 0 | 4518 | `		zColon--;` |
|    ! 0 | 4519 | `	}` |
|    ! 0 | 4520 | `	if( zColon <= zName \|\| zColon[0] != ':' ){` |
|    ! 0 | 4521 | `		return -1;` |
|      - | 4522 | `	}` |
|      - | 4523 | `	{` |
|    ! 0 | 4524 | `		sxu32 n = (sxu32)(zColon - zName);` |
|    ! 0 | 4525 | `		if( n >= sizeof(zHost) ){` |
|    ! 0 | 4526 | `			n = sizeof(zHost) - 1;` |
|    ! 0 | 4527 | `		}` |
|    ! 0 | 4528 | `		SyMemcpy(zName,zHost,n);` |
|    ! 0 | 4529 | `		zHost[n] = 0;` |
|      - | 4530 | `	}` |
|      - | 4531 | `	{` |
|    ! 0 | 4532 | `		sxi32 iTmp = 0;` |
|    ! 0 | 4533 | `		SyStrToInt32(&zColon[1],(sxu32)SyStrlen(&zColon[1]),(void *)&iTmp,0);` |
|    ! 0 | 4534 | `		iPort = (int)iTmp;` |
|      - | 4535 | `	}` |
|    ! 0 | 4536 | `	sock = PH7_NetConnect(zHost,iPort,0,0,&iErrno,&zErr);` |
|    ! 0 | 4537 | `	if( sock == PH7_NET_INVALID_SOCKET ){` |
|    ! 0 | 4538 | `		return -1;` |
|      - | 4539 | `	}` |
|    ! 0 | 4540 | `	pSock = (sock_private *)SyMemBackendAlloc(&pVm->sAllocator,sizeof(sock_private));` |
|    ! 0 | 4541 | `	if( pSock == 0 ){` |
|    ! 0 | 4542 | `		PH7_NetClose(sock);` |
|    ! 0 | 4543 | `		return -1;` |
|      - | 4544 | `	}` |
|    ! 0 | 4545 | `	pSock->pVm = pVm;` |
|    ! 0 | 4546 | `	pSock->sock = sock;` |
|    ! 0 | 4547 | `	pSock->bEof = 0;` |
|    ! 0 | 4548 | `	pSock->iLastErr = 0;` |
|    ! 0 | 4549 | `	pSock->bGeneric = 0;` |
|    ! 0 | 4550 | `	*ppHandle = (void *)pSock;` |
|    ! 0 | 4551 | `	return PH7_OK;` |
|    ! 0 | 4552 | `}` |
|      - | 4553 | `/* php's own listen backlog for a stream server. */` |
|      - | 4554 | `#define SOCK_LISTEN_BACKLOG 128` |
|      - | 4555 | `/*` |
|      - | 4556 | `` * php's `fwrite(): Send of 4 bytes failed with errno=32 Broken pipe` — the`` |
|      - | 4557 | ` * NOTICE its socket ops raise for a send that failed, which is the only` |
|      - | 4558 | ` * diagnostic a write to a departed peer produces (the return value is the same` |
|      - | 4559 | ` * false a closed handle answers). Silent for every other device: nothing else` |
|      - | 4560 | ` * here has an OS error of its own to report.` |
|      - | 4561 | ` */` |
|      6 | 4562 | `static void SockReportWriteFailure(ph7_context *pCtx,io_private *pDev,int nLen)` |
|      2 | 4563 | `{` |
|      - | 4564 | `#ifdef PH7_ENABLE_NET` |
|      8 | 4565 | `	if( pDev && pDev->pStream == &sTCP_Stream && pDev->pHandle ){` |
|      3 | 4566 | `		sock_private *pSock = (sock_private *)pDev->pHandle;` |
|      3 | 4567 | `		if( pSock->iLastErr != 0 ){` |
|      4 | 4568 | `			ph7_context_throw_error_format(pCtx,PH7_CTX_NOTICE,` |
|      - | 4569 | `				"Send of %d bytes failed with errno=%d %s",` |
|      1 | 4570 | `				nLen,pSock->iLastErr,PH7_NetStrError(pSock->iLastErr));` |
|      3 | 4571 | `			pSock->iLastErr = 0;` |
|      1 | 4572 | `		}` |
|      1 | 4573 | `	}` |
|      - | 4574 | `#else` |
|      - | 4575 | `	SXUNUSED(pCtx);` |
|      - | 4576 | `	SXUNUSED(pDev);` |
|      - | 4577 | `	SXUNUSED(nLen);` |
|      - | 4578 | `#endif` |
|      8 | 4579 | `}` |
|      - | 4580 | `/* The settings family below owns both of these; the socket openers here are` |
|      - | 4581 | ` * declared ahead of it so one handle-wrapping routine can serve both halves. */` |
|      - | 4582 | `static io_private * StreamSettingArgNamed(ph7_context *pCtx,ph7_value *pArg,int iPos,` |
|      - | 4583 | `	const char *zName,int *pRc);` |
|      - | 4584 | `static ph7_socket * IoPrivateSocket(io_private *pDev);` |
|      - | 4585 | `/*` |
|      - | 4586 | ` * Wrap an open socket in the io_private every f* builtin drives, so a socket a` |
|      - | 4587 | ` * server accepted reads and writes exactly like one a client connected. A NULL` |
|      - | 4588 | ` * zUri is php's "opened by no name at all" — an accepted connection, which` |
|      - | 4589 | `` * reports no `uri` at all from stream_get_meta_data().`` |
|      - | 4590 | ` * Answers 0 (and closes the socket) when there is no memory for the handle.` |
|      - | 4591 | ` */` |
|    102 | 4592 | `static io_private * SockWrapSocket(ph7_context *pCtx,ph7_socket sock,const char *zUri,int nUri)` |
|      3 | 4593 | `{` |
|      - | 4594 | `	io_private *pDev;` |
|      - | 4595 | `	sock_private *pSock;` |
|    105 | 4596 | `	pDev = (io_private *)ph7_context_alloc_chunk(pCtx,sizeof(io_private),TRUE,FALSE);` |
|    105 | 4597 | `	pSock = (sock_private *)SyMemBackendAlloc(&pCtx->pVm->sAllocator,sizeof(sock_private));` |
|    105 | 4598 | `	if( pDev == 0 \|\| pSock == 0 ){` |
|    ! 0 | 4599 | `		if( pSock ){` |
|    ! 0 | 4600 | `			SyMemBackendFree(&pCtx->pVm->sAllocator,pSock);` |
|    ! 0 | 4601 | `		}` |
|    ! 0 | 4602 | `		if( pDev ){` |
|      - | 4603 | `			/* Allocated with AutoRelease = FALSE, so nothing else will reclaim` |
|      - | 4604 | `			 * this chunk — it is not an io_private yet and has no buffers. */` |
|    ! 0 | 4605 | `			ph7_context_free_chunk(pCtx,pDev);` |
|    ! 0 | 4606 | `		}` |
|    ! 0 | 4607 | `		PH7_NetClose(sock);` |
|    ! 0 | 4608 | `		return 0;` |
|      - | 4609 | `	}` |
|    105 | 4610 | `	pSock->pVm = pCtx->pVm;` |
|    105 | 4611 | `	pSock->sock = sock;` |
|    105 | 4612 | `	pSock->bEof = 0;` |
|    105 | 4613 | `	pSock->iLastErr = 0;` |
|    105 | 4614 | `	pSock->bGeneric = 0;` |
|    105 | 4615 | `	InitIOPrivate(pCtx->pVm,&sTCP_Stream,pDev);` |
|      - | 4616 | `	/* php's feof() answers TRUE for a stream whose socket was never created. */` |
|    105 | 4617 | `	pDev->bEof = (sxu8)(sock == PH7_NET_INVALID_SOCKET ? 1 : 0);` |
|    105 | 4618 | `	SetIOPrivateOpenedAs(pDev,zUri,nUri,"r+",2);` |
|    105 | 4619 | `	pDev->pHandle = (void *)pSock;` |
|    105 | 4620 | `	return pDev;` |
|     54 | 4621 | `}` |
|      - | 4622 | `/*` |
|      - | 4623 | ` * Undo a SockWrapSocket() whose partner could not be wrapped: the device's own` |
|      - | 4624 | ` * close hook frees the socket handle, and the io_private chunk goes with it.` |
|      - | 4625 | ` * Nothing has handed this out as a resource yet, so there is no ph7_value that` |
|      - | 4626 | ` * could observe it afterwards.` |
|      - | 4627 | ` */` |
|    ! 0 | 4628 | `static void SockCloseWrapped(ph7_context *pCtx,io_private *pDev)` |
|    ! 0 | 4629 | `{` |
|    ! 0 | 4630 | `	if( pDev == 0 ){` |
|    ! 0 | 4631 | `		return;` |
|      - | 4632 | `	}` |
|    ! 0 | 4633 | `	if( pDev->pStream && pDev->pStream->xClose && pDev->pHandle ){` |
|    ! 0 | 4634 | `		pDev->pStream->xClose(pDev->pHandle);` |
|    ! 0 | 4635 | `		pDev->pHandle = 0;` |
|    ! 0 | 4636 | `	}` |
|    ! 0 | 4637 | `	ReleaseIOPrivate(pCtx,pDev);` |
|    ! 0 | 4638 | `}` |
|      - | 4639 | `/* Forward: php's port rule, defined with the address parser further down. */` |
|      - | 4640 | `static int SockParsePort(const char *z,int n);` |
|      - | 4641 | `/*` |
|      - | 4642 | `` * php's `socket` context options, read into the shape net.c applies. Only the`` |
|      - | 4643 | `` * ones a tcp-only, IPv4-only transport can honour are read: `bindto`, which is`` |
|      - | 4644 | `` * the LOCAL address a client connects out from, `backlog`, `so_reuseport` and`` |
|      - | 4645 | ``  * `tcp_nodelay`. `so_broadcast` describes a datagram socket and `ipv6_v6only` `` |
|      - | 4646 | ` * an address family this build has not got, so they stay on the context` |
|      - | 4647 | ` * unapplied (§7.4 slice-2 (a)).` |
|      - | 4648 | ` *` |
|      - | 4649 | `` * `bindto` is "host:port", split at the FIRST colon with an atoi() port — the`` |
|      - | 4650 | ` * same address rule the server half already uses — and a spelling with no colon` |
|      - | 4651 | ` * at all is not an address, so php performs no bind and says nothing. A value` |
|      - | 4652 | ` * that is not a STRING is php's one hard failure here; everything else is a` |
|      - | 4653 | ` * warning and a connection made from wherever routing would have sent it.` |
|      - | 4654 | ` * Returns 0, or -1 with *pzErr set to php's refusal.` |
|      - | 4655 | ` */` |
|    110 | 4656 | `static int SockCtxOptions(phl_stream_ctx *pCtxRes,ph7_sockopts *pOut,char *zHostBuf,int nHostBuf,` |
|      - | 4657 | `	const char **pzErr)` |
|      4 | 4658 | `{` |
|      - | 4659 | `	ph7_value *pVal;` |
|    114 | 4660 | `	SyZero(pOut,sizeof(*pOut));` |
|    114 | 4661 | `	if( pCtxRes == 0 ){` |
|     45 | 4662 | `		return 0;` |
|      - | 4663 | `	}` |
|     70 | 4664 | `	pVal = PH7_StreamCtxOption(pCtxRes,"socket","backlog");` |
|     70 | 4665 | `	if( pVal ){` |
|      5 | 4666 | `		pOut->iBacklog = (int)ph7_value_to_int64(pVal);` |
|      2 | 4667 | `	}` |
|     70 | 4668 | `	pVal = PH7_StreamCtxOption(pCtxRes,"socket","so_reuseport");` |
|     70 | 4669 | `	pOut->bReusePort = pVal != 0 && ph7_value_to_bool(pVal);` |
|     70 | 4670 | `	pVal = PH7_StreamCtxOption(pCtxRes,"socket","tcp_nodelay");` |
|     70 | 4671 | `	pOut->bNoDelay = pVal != 0 && ph7_value_to_bool(pVal);` |
|     70 | 4672 | `	pVal = PH7_StreamCtxOption(pCtxRes,"socket","bindto");` |
|     70 | 4673 | `	if( pVal ){` |
|      - | 4674 | `		const char *zSpec;` |
|     15 | 4675 | `		int nSpec = 0,i,nHost = -1;` |
|     15 | 4676 | `		if( (pVal->iFlags & MEMOBJ_STRING) == 0 ){` |
|      3 | 4677 | `			*pzErr = "local_addr context option is not a string.";` |
|      3 | 4678 | `			return -1;` |
|      - | 4679 | `		}` |
|     13 | 4680 | `		zSpec = (const char *)SyBlobData(&pVal->sBlob);` |
|     13 | 4681 | `		nSpec = (int)SyBlobLength(&pVal->sBlob);` |
|    137 | 4682 | `		for( i = 0 ; i + 1 < nSpec ; i++ ){` |
|    135 | 4683 | `			if( zSpec[i] == ':' ){` |
|     11 | 4684 | `				nHost = i;` |
|     11 | 4685 | `				pOut->iBindPort = SockParsePort(&zSpec[i+1],nSpec - i - 1);` |
|     11 | 4686 | `				break;` |
|      - | 4687 | `			}` |
|     63 | 4688 | `		}` |
|     13 | 4689 | `		if( nHost >= 0 ){` |
|     11 | 4690 | `			if( nHost >= nHostBuf ){` |
|    ! 0 | 4691 | `				nHost = nHostBuf - 1;` |
|    ! 0 | 4692 | `			}` |
|     11 | 4693 | `			if( nHost > 0 ){` |
|     11 | 4694 | `				SyMemcpy(zSpec,zHostBuf,(sxu32)nHost);` |
|      5 | 4695 | `			}` |
|     11 | 4696 | `			zHostBuf[nHost] = 0;` |
|     11 | 4697 | `			pOut->zBindHost = zHostBuf;` |
|      5 | 4698 | `		}` |
|      6 | 4699 | `	}` |
|     68 | 4700 | `	return 0;` |
|     59 | 4701 | `}` |
|      - | 4702 | `/*` |
|      - | 4703 | ` * php's PERSISTENT sockets, which pfsockopen() and STREAM_CLIENT_PERSISTENT ask` |
|      - | 4704 | ` * for: a second open of the SAME address hands back the very same resource` |
|      - | 4705 | `` * rather than a second connection — `$a === $b` — and fclose() is what ends it,`` |
|      - | 4706 | ` * after which the next open dials again. The key is the address as the opener` |
|      - | 4707 | ` * spelled it, so "localhost:80" and "127.0.0.1:80" are two of them.` |
|      - | 4708 | ` */` |
|     26 | 4709 | `static void SockPersistKey(char *zBuf,int nBuf,int bClientForm,const char *zAddr,int nAddr)` |
|      1 | 4710 | `{` |
|      - | 4711 | `	/* php prefixes the key with the FUNCTION that asked, so a pfsockopen() and a` |
|      - | 4712 | `	 * persistent stream_socket_client() of one address are two connections. */` |
|     40 | 4713 | `	SyBufferFormat(zBuf,(sxu32)nBuf,"%s__%.*s",` |
|     13 | 4714 | `		bClientForm ? "stream_socket_client" : "pfsockopen",nAddr,zAddr);` |
|     27 | 4715 | `}` |
|     16 | 4716 | `static io_private * SockPersistFind(ph7_vm *pVm,const char *zKey)` |
|      1 | 4717 | `{` |
|     17 | 4718 | `	VmPersistSock *aSlot = (VmPersistSock *)SySetBasePtr(&pVm->aPersistSock);` |
|      - | 4719 | `	sxu32 i;` |
|     37 | 4720 | `	for( i = 0 ; i < SySetUsed(&pVm->aPersistSock) ; i++ ){` |
|     27 | 4721 | `		if( aSlot[i].zKey[0] && SyStrncmp(aSlot[i].zKey,zKey,(sxu32)SyStrlen(zKey) + 1) == 0 ){` |
|      9 | 4722 | `			if( !IO_PRIVATE_INVALID(aSlot[i].pDev) ){` |
|      7 | 4723 | `				return aSlot[i].pDev;` |
|      - | 4724 | `			}` |
|      - | 4725 | `			/* fclose()'d since: the slot is free for the next connection. */` |
|      3 | 4726 | `			aSlot[i].zKey[0] = 0;` |
|      3 | 4727 | `			aSlot[i].pDev = 0;` |
|      1 | 4728 | `		}` |
|     11 | 4729 | `	}` |
|     11 | 4730 | `	return 0;` |
|      9 | 4731 | `}` |
|     10 | 4732 | `static void SockPersistKeep(ph7_vm *pVm,const char *zKey,io_private *pDev)` |
|      1 | 4733 | `{` |
|     11 | 4734 | `	VmPersistSock *aSlot = (VmPersistSock *)SySetBasePtr(&pVm->aPersistSock);` |
|      - | 4735 | `	VmPersistSock sSlot;` |
|      - | 4736 | `	sxu32 i;` |
|     23 | 4737 | `	for( i = 0 ; i < SySetUsed(&pVm->aPersistSock) ; i++ ){` |
|     15 | 4738 | `		if( aSlot[i].zKey[0] == 0 \|\| IO_PRIVATE_INVALID(aSlot[i].pDev) ){` |
|      3 | 4739 | `			SyZero(&aSlot[i],sizeof(VmPersistSock));` |
|      3 | 4740 | `			Systrcpy(aSlot[i].zKey,(sxu32)sizeof(aSlot[i].zKey),zKey,0);` |
|      3 | 4741 | `			aSlot[i].pDev = pDev;` |
|      3 | 4742 | `			return;` |
|      - | 4743 | `		}` |
|      7 | 4744 | `	}` |
|      9 | 4745 | `	SyZero(&sSlot,sizeof(sSlot));` |
|      9 | 4746 | `	Systrcpy(sSlot.zKey,(sxu32)sizeof(sSlot.zKey),zKey,0);` |
|      9 | 4747 | `	sSlot.pDev = pDev;` |
|      9 | 4748 | `	SySetPut(&pVm->aPersistSock,(const void *)&sSlot);` |
|      6 | 4749 | `}` |
|      - | 4750 | `/*` |
|      - | 4751 | `` * php bounds every CONNECTED socket's reads by `default_socket_timeout` from the`` |
|      - | 4752 | ` * moment it is opened — a read from a peer that has gone quiet answers FALSE` |
|      - | 4753 | `` * after it, with `timed_out` set — where this engine armed nothing and waited`` |
|      - | 4754 | ` * forever. That is the difference between a program that reports a dead peer and` |
|      - | 4755 | ` * one that hangs.` |
|      - | 4756 | ` *` |
|      - | 4757 | ` * A LISTENING socket is deliberately left alone: php's accept timeout is its own` |
|      - | 4758 | ` * argument and its own select(), so arming the OS receive timeout here would` |
|      - | 4759 | `` * bound `stream_socket_accept($srv, -1)` — the wait a server asks to be`` |
|      - | 4760 | ` * unbounded — at sixty seconds.` |
|      - | 4761 | ` */` |
|     72 | 4762 | `static void SockArmDefaultTimeout(ph7_context *pCtx,io_private *pDev)` |
|      3 | 4763 | `{` |
|      - | 4764 | `	ph7_int64 iSec;` |
|     75 | 4765 | `	ph7_socket *pSock = pDev ? IoPrivateSocket(pDev) : 0;` |
|     75 | 4766 | `	if( pSock == 0 \|\| *pSock == PH7_NET_INVALID_SOCKET ){` |
|    ! 0 | 4767 | `		return;` |
|      - | 4768 | `	}` |
|     75 | 4769 | `	iSec = PH7_VmIniGetInt(pCtx->pVm,"default_socket_timeout",60);` |
|     75 | 4770 | `	if( iSec > 0 ){` |
|     75 | 4771 | `		PH7_NetSetRwTimeout(*pSock,iSec,0);` |
|     75 | 4772 | `		pDev->bHasTimeout = 1;` |
|     36 | 4773 | `	}` |
|     39 | 4774 | `}` |
|      - | 4775 | `/*` |
|      - | 4776 | ` * The out-params every address-taking opener carries, on the path that WORKED:` |
|      - | 4777 | ` * php writes 0 and "" into them rather than leaving whatever the caller's` |
|      - | 4778 | ` * variables already held.` |
|      - | 4779 | ` */` |
|     80 | 4780 | `static void SockAddressSuccess(ph7_context *pCtx,ph7_value **apArg,int nArg,int iArgErrno,int iArgErrstr)` |
|      3 | 4781 | `{` |
|     83 | 4782 | `	ph7_value *pTmp = ph7_context_new_scalar(pCtx);` |
|     83 | 4783 | `	if( pTmp == 0 ){` |
|    ! 0 | 4784 | `		return;` |
|      - | 4785 | `	}` |
|     83 | 4786 | `	if( iArgErrno >= 0 && nArg > iArgErrno ){` |
|     59 | 4787 | `		ph7_value_int(pTmp,0);` |
|     59 | 4788 | `		PH7_VmStoreArgByRef(pCtx->pVm,apArg[iArgErrno],pTmp);` |
|     28 | 4789 | `	}` |
|     83 | 4790 | `	if( iArgErrstr >= 0 && nArg > iArgErrstr ){` |
|     59 | 4791 | `		ph7_value_string(pTmp,"",0);` |
|     59 | 4792 | `		PH7_VmStoreArgByRef(pCtx->pVm,apArg[iArgErrstr],pTmp);` |
|     28 | 4793 | `	}` |
|     43 | 4794 | `}` |
|      - | 4795 | `/*` |
|      - | 4796 | ` * The failure shape the whole address-taking family shares: php words the` |
|      - | 4797 | ` * reason into BOTH the by-ref out-params and a warning naming the address as` |
|      - | 4798 | `` * the script wrote it. The `$errno` out-param stays 0 for everything the`` |
|      - | 4799 | ` * ADDRESS itself is refused for — php only ever reports an OS code for a` |
|      - | 4800 | ` * connect() that reached the network.` |
|      - | 4801 | ` */` |
|     52 | 4802 | `static void SockAddressFailure(ph7_context *pCtx,ph7_value **apArg,int nArg,int iArgErrno,int iArgErrstr,` |
|      - | 4803 | `	const char *zAddr,int nAddr,const char *zErr,int iErrno)` |
|      2 | 4804 | `{` |
|      - | 4805 | `	ph7_value *pTmp;` |
|     54 | 4806 | `	if( zErr == 0 ){` |
|    ! 0 | 4807 | `		zErr = "";` |
|    ! 0 | 4808 | `	}` |
|     54 | 4809 | `	pTmp = ph7_context_new_scalar(pCtx);` |
|     54 | 4810 | `	if( pTmp ){` |
|     54 | 4811 | `		if( iArgErrno >= 0 && nArg > iArgErrno ){` |
|     54 | 4812 | `			ph7_value_int(pTmp,iErrno);` |
|     54 | 4813 | `			PH7_VmStoreArgByRef(pCtx->pVm,apArg[iArgErrno],pTmp);` |
|     26 | 4814 | `		}` |
|     54 | 4815 | `		if( iArgErrstr >= 0 && nArg > iArgErrstr ){` |
|     54 | 4816 | `			ph7_value_string(pTmp,zErr,-1);` |
|     54 | 4817 | `			PH7_VmStoreArgByRef(pCtx->pVm,apArg[iArgErrstr],pTmp);` |
|     26 | 4818 | `		}` |
|     26 | 4819 | `	}` |
|      - | 4820 | `	/* NOTE: ph7_context_throw_error_format already prepends "fname(): " */` |
|     80 | 4821 | `	ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,"Unable to connect to %.*s (%s)",` |
|     26 | 4822 | `		nAddr,zAddr,zErr);` |
|     54 | 4823 | `}` |
|      - | 4824 | `/*` |
|      - | 4825 | ` * The one failure whose message names the HOST, and the one php reports TWICE:` |
|      - | 4826 | ` * its transport raises the text on its own before the opener that asked repeats` |
|      - | 4827 | ` * it inside "Unable to connect to". Composed here because net.c hands back a` |
|      - | 4828 | ` * static string and only the caller has the name to word in.` |
|      - | 4829 | ` */` |
|      6 | 4830 | `static const char * SockResolveFailure(ph7_context *pCtx,const char *zHost,char *zBuf,int nBuf)` |
|      1 | 4831 | `{` |
|     10 | 4832 | `	SyBufferFormat(zBuf,(sxu32)nBuf,` |
|      3 | 4833 | `		"php_network_getaddresses: getaddrinfo for %s failed: Name or service not known",zHost);` |
|      7 | 4834 | `	ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,"%s",zBuf);` |
|      7 | 4835 | `	return zBuf;` |
|      1 | 4836 | `}` |
|      - | 4837 | `PH7_PRIVATE const ph7_io_stream sTCP_Stream = {` |
|      - | 4838 | `	"tcp",` |
|      - | 4839 | `	PH7_IO_STREAM_VERSION,` |
|      - | 4840 | `	SockStreamData_Open, /* xOpen */` |
|      - | 4841 | `	0,   /* xOpenDir */` |
|      - | 4842 | `	SockStreamData_Close,/* xClose */` |
|      - | 4843 | `	0,  /* xCloseDir */` |
|      - | 4844 | `	SockStreamData_Read, /* xRead */` |
|      - | 4845 | `	0,  /* xReadDir */` |
|      - | 4846 | `	SockStreamData_Write,/* xWrite */` |
|      - | 4847 | `	0,  /* xSeek (sockets are not seekable) */` |
|      - | 4848 | `	0,  /* xLock */` |
|      - | 4849 | `	0,  /* xRewindDir */` |
|      - | 4850 | `	0,  /* xTell */` |
|      - | 4851 | `	0,  /* xTrunc */` |
|      - | 4852 | `	0,  /* xSync */` |
|      - | 4853 | `	0   /* xStat */` |
|      - | 4854 | `};` |
|      - | 4855 | `#endif /* PH7_ENABLE_NET */` |
|      - | 4856 | `/*` |
|      - | 4857 | ` * Userland stream wrappers (stream_wrapper_register). The engine's device` |
|      - | 4858 | ` * callbacks receive no device pointer, so each registered wrapper needs its` |
|      - | 4859 | ` * OWN xOpen thunk: PHL keeps a bounded pool of PHL_UWRAP_MAX slots, each with` |
|      - | 4860 | ` * a static thunk that knows its index (recorded limit; php has no cap).` |
|      - | 4861 | ` * The handle carries the userland object, and every stream op dispatches the` |
|      - | 4862 | ` * php streamWrapper protocol method on it.` |
|      - | 4863 | ` */` |
|      - | 4864 | `#define PHL_UWRAP_MAX 8` |
|      - | 4865 | `typedef struct uwrap_slot uwrap_slot;` |
|      - | 4866 | `struct uwrap_slot` |
|      - | 4867 | `{` |
|      - | 4868 | `	ph7_vm *pVm;              /* owning VM (0 = free slot) */` |
|      - | 4869 | `	char zScheme[32];         /* protocol name */` |
|      - | 4870 | `	char zClass[128];         /* userland wrapper class */` |
|      - | 4871 | `	int bIsUrl;               /* registered with STREAM_IS_URL: opening it is gated` |
|      - | 4872 | `	                           * by allow_url_fopen, INCLUDING it by` |
|      - | 4873 | `	                           * allow_url_include */` |
|      - | 4874 | `	ph7_io_stream sStream;    /* the device handed to the VM */` |
|      - | 4875 | `};` |
|      - | 4876 | `typedef struct uwrap_handle uwrap_handle;` |
|      - | 4877 | `struct uwrap_handle` |
|      - | 4878 | `{` |
|      - | 4879 | `	ph7_vm *pVm;` |
|      - | 4880 | `	ph7_class_instance *pObj; /* the wrapper instance (one per open stream) */` |
|      - | 4881 | `	int iSlot;` |
|      - | 4882 | `	int bEof;` |
|      - | 4883 | `};` |
|      - | 4884 | `static uwrap_slot g_aUwrap[PHL_UWRAP_MAX];` |
|      - | 4885 | `/*` |
|      - | 4886 | ` * Was this device registered with STREAM_IS_URL? Only a userland wrapper can` |
|      - | 4887 | ` * carry the flag, so the answer is a scan of the registration slots.` |
|      - | 4888 | ` */` |
|  35346 | 4889 | `PH7_PRIVATE int PH7_StreamIsUrlWrapper(const ph7_io_stream *pStream)` |
|      5 | 4890 | `{` |
|      - | 4891 | `	int i;` |
|      - | 4892 | `	/* php marks its own data:// wrapper a URL, and that is the one that matters` |
|      - | 4893 | ``	 * here: `include 'data://text/plain;base64,…'` executes bytes from the URI`` |
|      - | 4894 | `	 * itself, which is why php refuses it unless allow_url_include says` |
|      - | 4895 | `	 * otherwise. php:// is NOT a URL wrapper in php and stays open. */` |
|  35346 | 4896 | `	if( pStream && pStream->zName` |
|  35351 | 4897 | `	 && SyStrlen(pStream->zName) == 4 && SyStrnicmp(pStream->zName,"data",4) == 0 ){` |
|     41 | 4898 | `		return 1;` |
|      - | 4899 | `	}` |
| 317323 | 4900 | `	for( i = 0 ; i < PHL_UWRAP_MAX ; i++ ){` |
| 282073 | 4901 | `		if( g_aUwrap[i].pVm && &g_aUwrap[i].sStream == pStream ){` |
|     61 | 4902 | `			return g_aUwrap[i].bIsUrl;` |
|      - | 4903 | `		}` |
| 141010 | 4904 | `	}` |
|  35255 | 4905 | `	return 0;` |
|  17678 | 4906 | `}` |
|      - | 4907 | `/*` |
|      - | 4908 | ` * Is this device one of the userland wrapper slots? php labels every such` |
|      - | 4909 | `` * stream `user-space` rather than by its protocol.`` |
|      - | 4910 | ` */` |
|   8143 | 4911 | `static int IoPrivateIsUwrap(const ph7_io_stream *pStream)` |
|      5 | 4912 | `{` |
|      - | 4913 | `	int i;` |
|  73132 | 4914 | `	for( i = 0 ; i < PHL_UWRAP_MAX ; i++ ){` |
|  65009 | 4915 | `		if( g_aUwrap[i].pVm && &g_aUwrap[i].sStream == pStream ){` |
|     22 | 4916 | `			return 1;` |
|      - | 4917 | `		}` |
|  32493 | 4918 | `	}` |
|   8128 | 4919 | `	return 0;` |
|   4076 | 4920 | `}` |
|      - | 4921 | `/*` |
|      - | 4922 | ` * Is this device one of the registration slots at all? Unlike IoPrivateIsUwrap()` |
|      - | 4923 | ` * this does NOT ask whether the slot is still live -- restore() has to tell a` |
|      - | 4924 | ` * withdrawn userland wrapper from a built-in, and a withdrawn slot has already` |
|      - | 4925 | ` * had its pVm cleared.` |
|      - | 4926 | ` */` |
|     18 | 4927 | `static int UwrapIsSlotDevice(const ph7_io_stream *pStream)` |
|      1 | 4928 | `{` |
|      - | 4929 | `	int i;` |
|     99 | 4930 | `	for( i = 0 ; i < PHL_UWRAP_MAX ; i++ ){` |
|     89 | 4931 | `		if( &g_aUwrap[i].sStream == pStream ){` |
|      9 | 4932 | `			return 1;` |
|      - | 4933 | `		}` |
|     41 | 4934 | `	}` |
|     11 | 4935 | `	return 0;` |
|     10 | 4936 | `}` |
|      - | 4937 | `/* Forward: the protocol dispatcher is defined just below. */` |
|      - | 4938 | `static int UwrapCall(uwrap_handle *pH,const char *zMethod,int nArg,ph7_value **apArg,` |
|      - | 4939 | `	ph7_value *pResult);` |
|      - | 4940 | `/*` |
|      - | 4941 | ` * Ask a userland wrapper whether it is at end of file — php's own` |
|      - | 4942 | ` * streamWrapper::stream_eof(), which PHL used to leave undispatched, inferring` |
|      - | 4943 | ` * the answer from a zero-length read instead. Returns 0 when the handle is not` |
|      - | 4944 | ` * a userland stream (nothing written to *pAnswer).` |
|      - | 4945 | ` */` |
|   7949 | 4946 | `static int IoPrivateUwrapEof(io_private *pDev,int *pAnswer)` |
|      5 | 4947 | `{` |
|      - | 4948 | `	uwrap_handle *pH;` |
|      - | 4949 | `	ph7_value sRet;` |
|   7954 | 4950 | `	if( pDev == 0 \|\| pDev->pHandle == 0 \|\| !IoPrivateIsUwrap(pDev->pStream) ){` |
|   7948 | 4951 | `		return 0;` |
|      - | 4952 | `	}` |
|      8 | 4953 | `	pH = (uwrap_handle *)pDev->pHandle;` |
|      8 | 4954 | `	PH7_MemObjInit(pH->pVm,&sRet);` |
|      8 | 4955 | `	if( UwrapCall(pH,"stream_eof",0,0,&sRet) != 0 ){` |
|      - | 4956 | `		/* php's streamWrapper requires the method; a class without one keeps` |
|      - | 4957 | `		 * the read-derived answer rather than being called into. */` |
|    ! 0 | 4958 | `		PH7_MemObjRelease(&sRet);` |
|    ! 0 | 4959 | `		*pAnswer = pH->bEof;` |
|    ! 0 | 4960 | `		return 1;` |
|      - | 4961 | `	}` |
|      8 | 4962 | `	*pAnswer = ph7_value_to_bool(&sRet) ? 1 : 0;` |
|      8 | 4963 | `	PH7_MemObjRelease(&sRet);` |
|      8 | 4964 | `	return 1;` |
|   3979 | 4965 | `}` |
|      - | 4966 | `/*` |
|      - | 4967 | `` * The wrapper INSTANCE serving an open userland stream (php's `wrapper_data`),`` |
|      - | 4968 | ` * or 0 for any other device.` |
|      - | 4969 | ` */` |
|     82 | 4970 | `static ph7_class_instance * IoPrivateUwrapObject(io_private *pDev)` |
|      4 | 4971 | `{` |
|     86 | 4972 | `	if( pDev == 0 \|\| pDev->pHandle == 0 \|\| !IoPrivateIsUwrap(pDev->pStream) ){` |
|     82 | 4973 | `		return 0;` |
|      - | 4974 | `	}` |
|      5 | 4975 | `	return ((uwrap_handle *)pDev->pHandle)->pObj;` |
|     45 | 4976 | `}` |
|      - | 4977 | `/* Call $obj->$zMethod(...) and copy the result into pResult (may be 0) */` |
|    180 | 4978 | `static int UwrapCall(uwrap_handle *pH,const char *zMethod,int nArg,ph7_value **apArg,` |
|      - | 4979 | `	ph7_value *pResult)` |
|      3 | 4980 | `{` |
|      - | 4981 | `	ph7_class_method *pMeth;` |
|    183 | 4982 | `	if( pH == 0 \|\| pH->pObj == 0 ){` |
|    ! 0 | 4983 | `		return -1;` |
|      - | 4984 | `	}` |
|    183 | 4985 | `	pMeth = PH7_ClassExtractMethod(pH->pObj->pClass,zMethod,(sxu32)SyStrlen(zMethod));` |
|    183 | 4986 | `	if( pMeth == 0 ){` |
|     27 | 4987 | `		return -1;` |
|      - | 4988 | `	}` |
|    159 | 4989 | `	if( PH7_VmCallClassMethod(pH->pVm,pH->pObj,pMeth,pResult,nArg,apArg) != SXRET_OK ){` |
|    ! 0 | 4990 | `		return -1;` |
|      - | 4991 | `	}` |
|    159 | 4992 | `	return 0;` |
|     93 | 4993 | `}` |
|     58 | 4994 | `static ph7_int64 UwrapRead(void *pHandle,void *pBuffer,ph7_int64 nRead)` |
|      3 | 4995 | `{` |
|     61 | 4996 | `	uwrap_handle *pH = (uwrap_handle *)pHandle;` |
|      - | 4997 | `	ph7_value sArg,sRet;` |
|      - | 4998 | `	const char *zData;` |
|     61 | 4999 | `	int nData = 0;` |
|     61 | 5000 | `	ph7_int64 n = 0;` |
|     61 | 5001 | `	if( pH == 0 \|\| pH->bEof ){` |
|    ! 0 | 5002 | `		return 0;` |
|      - | 5003 | `	}` |
|     61 | 5004 | `	PH7_MemObjInit(pH->pVm,&sArg);` |
|     61 | 5005 | `	PH7_MemObjInit(pH->pVm,&sRet);` |
|     61 | 5006 | `	ph7_value_int64(&sArg,nRead);` |
|      - | 5007 | `	{` |
|      - | 5008 | `		ph7_value *apArg[1];` |
|     61 | 5009 | `		apArg[0] = &sArg;` |
|     61 | 5010 | `		if( UwrapCall(pH,"stream_read",1,apArg,&sRet) != 0 ){` |
|    ! 0 | 5011 | `			PH7_MemObjRelease(&sArg);` |
|    ! 0 | 5012 | `			PH7_MemObjRelease(&sRet);` |
|    ! 0 | 5013 | `			return -1;` |
|      - | 5014 | `		}` |
|      - | 5015 | `	}` |
|     61 | 5016 | `	zData = ph7_value_to_string(&sRet,&nData);` |
|     61 | 5017 | `	if( nData > 0 ){` |
|     31 | 5018 | `		if( (ph7_int64)nData > nRead ){` |
|    ! 0 | 5019 | `			nData = (int)nRead;` |
|    ! 0 | 5020 | `		}` |
|     31 | 5021 | `		SyMemcpy(zData,pBuffer,(sxu32)nData);` |
|     31 | 5022 | `		n = nData;` |
|     17 | 5023 | `	}else{` |
|     32 | 5024 | `		pH->bEof = 1;` |
|      - | 5025 | `	}` |
|     61 | 5026 | `	PH7_MemObjRelease(&sArg);` |
|     61 | 5027 | `	PH7_MemObjRelease(&sRet);` |
|     61 | 5028 | `	return n;` |
|     32 | 5029 | `}` |
|      4 | 5030 | `static ph7_int64 UwrapWrite(void *pHandle,const void *pBuf,ph7_int64 nWrite)` |
|      1 | 5031 | `{` |
|      5 | 5032 | `	uwrap_handle *pH = (uwrap_handle *)pHandle;` |
|      - | 5033 | `	ph7_value sArg,sRet;` |
|      - | 5034 | `	ph7_int64 n;` |
|      5 | 5035 | `	if( pH == 0 ){` |
|    ! 0 | 5036 | `		return -1;` |
|      - | 5037 | `	}` |
|      5 | 5038 | `	PH7_MemObjInit(pH->pVm,&sArg);` |
|      5 | 5039 | `	PH7_MemObjInit(pH->pVm,&sRet);` |
|      5 | 5040 | `	ph7_value_string(&sArg,(const char *)pBuf,(int)nWrite);` |
|      - | 5041 | `	{` |
|      - | 5042 | `		ph7_value *apArg[1];` |
|      5 | 5043 | `		apArg[0] = &sArg;` |
|      5 | 5044 | `		if( UwrapCall(pH,"stream_write",1,apArg,&sRet) != 0 ){` |
|    ! 0 | 5045 | `			PH7_MemObjRelease(&sArg);` |
|    ! 0 | 5046 | `			PH7_MemObjRelease(&sRet);` |
|    ! 0 | 5047 | `			return -1;` |
|      - | 5048 | `		}` |
|      - | 5049 | `	}` |
|      5 | 5050 | `	n = ph7_value_to_int64(&sRet);` |
|      5 | 5051 | `	PH7_MemObjRelease(&sArg);` |
|      5 | 5052 | `	PH7_MemObjRelease(&sRet);` |
|      5 | 5053 | `	return n;` |
|      3 | 5054 | `}` |
|      2 | 5055 | `static int UwrapSeek(void *pHandle,ph7_int64 iOfft,int whence)` |
|      1 | 5056 | `{` |
|      3 | 5057 | `	uwrap_handle *pH = (uwrap_handle *)pHandle;` |
|      - | 5058 | `	ph7_value sOfft,sWhence,sRet;` |
|      - | 5059 | `	ph7_value *apArg[2];` |
|      - | 5060 | `	int rc;` |
|      3 | 5061 | `	if( pH == 0 ){` |
|    ! 0 | 5062 | `		return -1;` |
|      - | 5063 | `	}` |
|      3 | 5064 | `	PH7_MemObjInit(pH->pVm,&sOfft);` |
|      3 | 5065 | `	PH7_MemObjInit(pH->pVm,&sWhence);` |
|      3 | 5066 | `	PH7_MemObjInit(pH->pVm,&sRet);` |
|      3 | 5067 | `	ph7_value_int64(&sOfft,iOfft);` |
|      3 | 5068 | `	ph7_value_int(&sWhence,whence);` |
|      3 | 5069 | `	apArg[0] = &sOfft;` |
|      3 | 5070 | `	apArg[1] = &sWhence;` |
|      3 | 5071 | `	rc = UwrapCall(pH,"stream_seek",2,apArg,&sRet);` |
|      3 | 5072 | `	if( rc == 0 ){` |
|      3 | 5073 | `		pH->bEof = 0;` |
|      3 | 5074 | `		rc = ph7_value_to_bool(&sRet) ? PH7_OK : -1;` |
|      1 | 5075 | `	}` |
|      3 | 5076 | `	PH7_MemObjRelease(&sOfft);` |
|      3 | 5077 | `	PH7_MemObjRelease(&sWhence);` |
|      3 | 5078 | `	PH7_MemObjRelease(&sRet);` |
|      3 | 5079 | `	return rc;` |
|      2 | 5080 | `}` |
|      6 | 5081 | `static ph7_int64 UwrapTell(void *pHandle)` |
|      1 | 5082 | `{` |
|      7 | 5083 | `	uwrap_handle *pH = (uwrap_handle *)pHandle;` |
|      - | 5084 | `	ph7_value sRet;` |
|      - | 5085 | `	ph7_int64 n;` |
|      7 | 5086 | `	if( pH == 0 ){` |
|    ! 0 | 5087 | `		return -1;` |
|      - | 5088 | `	}` |
|      7 | 5089 | `	PH7_MemObjInit(pH->pVm,&sRet);` |
|      7 | 5090 | `	if( UwrapCall(pH,"stream_tell",0,0,&sRet) != 0 ){` |
|      3 | 5091 | `		PH7_MemObjRelease(&sRet);` |
|      3 | 5092 | `		return -1;` |
|      - | 5093 | `	}` |
|      5 | 5094 | `	n = ph7_value_to_int64(&sRet);` |
|      5 | 5095 | `	PH7_MemObjRelease(&sRet);` |
|      5 | 5096 | `	return n;` |
|      4 | 5097 | `}` |
|     50 | 5098 | `static void UwrapClose(void *pHandle)` |
|      3 | 5099 | `{` |
|     53 | 5100 | `	uwrap_handle *pH = (uwrap_handle *)pHandle;` |
|     53 | 5101 | `	if( pH == 0 ){` |
|    ! 0 | 5102 | `		return;` |
|      - | 5103 | `	}` |
|     53 | 5104 | `	UwrapCall(pH,"stream_close",0,0,0);` |
|     53 | 5105 | `	if( pH->pObj ){` |
|     53 | 5106 | `		PH7_ClassInstanceUnref(pH->pObj);` |
|     25 | 5107 | `	}` |
|     53 | 5108 | `	SyMemBackendFree(&pH->pVm->sAllocator,pH);` |
|     28 | 5109 | `}` |
|      - | 5110 | `/* Shared open: instantiate the wrapper class and call stream_open() */` |
|     54 | 5111 | `static int UwrapOpenSlot(int iSlot,const char *zName,int iMode,ph7_value *pResource,void **ppHandle)` |
|      3 | 5112 | `{` |
|     57 | 5113 | `	uwrap_slot *pSlot = &g_aUwrap[iSlot];` |
|     57 | 5114 | `	ph7_vm *pVm = pResource ? pResource->pVm : 0;` |
|      - | 5115 | `	ph7_class *pClass;` |
|      - | 5116 | `	uwrap_handle *pH;` |
|      - | 5117 | `	ph7_value sPath,sMode,sOpts,sOpened,sRet;` |
|      - | 5118 | `	ph7_value *apArg[4];` |
|      - | 5119 | `	int rc;` |
|     57 | 5120 | `	if( pVm == 0 \|\| pSlot->pVm == 0 ){` |
|    ! 0 | 5121 | `		return -1;` |
|      - | 5122 | `	}` |
|     57 | 5123 | `	pClass = PH7_VmExtractClass(pVm,pSlot->zClass,(sxu32)SyStrlen(pSlot->zClass),TRUE,0);` |
|     57 | 5124 | `	if( pClass == 0 ){` |
|    ! 0 | 5125 | `		return -1;` |
|      - | 5126 | `	}` |
|     57 | 5127 | `	pH = (uwrap_handle *)SyMemBackendAlloc(&pVm->sAllocator,sizeof(uwrap_handle));` |
|     57 | 5128 | `	if( pH == 0 ){` |
|    ! 0 | 5129 | `		return -1;` |
|      - | 5130 | `	}` |
|     57 | 5131 | `	pH->pVm = pVm;` |
|     57 | 5132 | `	pH->iSlot = iSlot;` |
|     57 | 5133 | `	pH->bEof = 0;` |
|     57 | 5134 | `	pH->pObj = PH7_NewClassInstance(pVm,pClass);` |
|     57 | 5135 | `	if( pH->pObj == 0 ){` |
|    ! 0 | 5136 | `		SyMemBackendFree(&pVm->sAllocator,pH);` |
|    ! 0 | 5137 | `		return -1;` |
|      - | 5138 | `	}` |
|      - | 5139 | `	{` |
|      - | 5140 | `		/* php's streamWrapper::$context, set on the serving instance BEFORE` |
|      - | 5141 | `		 * stream_open() runs — which is the whole reason a userland wrapper can` |
|      - | 5142 | `		 * be configured per open. It is exactly what the OPENER resolved: the` |
|      - | 5143 | ``		 * default context substitutes for a NULL `$context` argument, so an`` |
|      - | 5144 | `		 * ordinary fopen() hands a resource over; but an opener with no such` |
|      - | 5145 | `		 * argument at all (md5_file(), include) and one that carried` |
|      - | 5146 | `		 * FILE_NO_DEFAULT_CONTEXT hand over php's NULL. Substituting the default` |
|      - | 5147 | `		 * here would make that flag mean nothing.` |
|      - | 5148 | `		 * The class need not declare the slot; php adds it either way. */` |
|     57 | 5149 | `		phl_stream_ctx *pOpenCtx = (phl_stream_ctx *)pVm->pOpenCtx;` |
|     57 | 5150 | `		ph7_value *pCtxSlot = PH7_NativeAttr(pH->pObj,"context");` |
|     57 | 5151 | `		if( pCtxSlot == 0 ){` |
|    ! 0 | 5152 | `			pCtxSlot = PH7_VmCreateDynamicAttr(pVm,pH->pObj,"context",sizeof("context")-1,0);` |
|    ! 0 | 5153 | `		}` |
|     57 | 5154 | `		if( pCtxSlot ){` |
|     57 | 5155 | `			if( pOpenCtx ){` |
|     45 | 5156 | `				ph7_value_resource(pCtxSlot,(void *)pOpenCtx);` |
|     24 | 5157 | `			}else{` |
|     14 | 5158 | `				ph7_value_null(pCtxSlot);` |
|      - | 5159 | `			}` |
|     27 | 5160 | `		}` |
|      - | 5161 | `	}` |
|      - | 5162 | `	/* php hands stream_open the FULL url, scheme included */` |
|     57 | 5163 | `	PH7_MemObjInit(pVm,&sPath);` |
|     57 | 5164 | `	PH7_MemObjInit(pVm,&sMode);` |
|     57 | 5165 | `	PH7_MemObjInit(pVm,&sOpts);` |
|     57 | 5166 | `	PH7_MemObjInit(pVm,&sRet);` |
|      - | 5167 | `	/* $opened_path is BY REFERENCE: the callee's binding needs a real memobj` |
|      - | 5168 | `	 * slot (a stack ph7_value has nIdx == SXU32_HIGH and the engine rejects` |
|      - | 5169 | `	 * it as "could not be passed by reference"). */` |
|      - | 5170 | `	{` |
|     57 | 5171 | `		ph7_value *pRefSlot = PH7_ReserveMemObj(pVm);` |
|     57 | 5172 | `		if( pRefSlot == 0 ){` |
|    ! 0 | 5173 | `			PH7_ClassInstanceUnref(pH->pObj);` |
|    ! 0 | 5174 | `			SyMemBackendFree(&pVm->sAllocator,pH);` |
|    ! 0 | 5175 | `			return -1;` |
|      - | 5176 | `		}` |
|     57 | 5177 | `		PH7_MemObjInit(pVm,&sOpened);` |
|     57 | 5178 | `		sOpened.nIdx = pRefSlot->nIdx;` |
|      - | 5179 | `	}` |
|     54 | 5180 | `	if( SyStrlen(pSlot->zScheme) == sizeof("file")-1` |
|     47 | 5181 | `	 && SyStrnicmp(pSlot->zScheme,"file",sizeof("file")-1) == 0 ){` |
|      - | 5182 | `		/* The one scheme php does NOT hand back whole. Its locate_url_wrapper` |
|      - | 5183 | `		 * strips "file://" for whoever owns the name, built-in or not, so a` |
|      - | 5184 | `		 * wrapper that replaced file:// sees the plain path -- the same bytes a` |
|      - | 5185 | `		 * bare path would have given it. */` |
|      3 | 5186 | `		ph7_value_string(&sPath,zName,-1);` |
|      2 | 5187 | `	}else{` |
|      - | 5188 | `		SyBlob sUrl;` |
|     55 | 5189 | `		SyBlobInit(&sUrl,&pVm->sAllocator);` |
|     55 | 5190 | `		SyBlobFormat(&sUrl,"%s://%s",pSlot->zScheme,zName);` |
|     55 | 5191 | `		ph7_value_string(&sPath,(const char *)SyBlobData(&sUrl),(int)SyBlobLength(&sUrl));` |
|     55 | 5192 | `		SyBlobRelease(&sUrl);` |
|      - | 5193 | `	}` |
|     83 | 5194 | `	ph7_value_string(&sMode,(iMode & PH7_IO_OPEN_WRONLY) ? "w"` |
|     52 | 5195 | `		: ((iMode & PH7_IO_OPEN_APPEND) ? "a" : "r"),-1);` |
|     57 | 5196 | `	ph7_value_int(&sOpts,0);` |
|     57 | 5197 | `	apArg[0] = &sPath;` |
|     57 | 5198 | `	apArg[1] = &sMode;` |
|     57 | 5199 | `	apArg[2] = &sOpts;` |
|     57 | 5200 | `	apArg[3] = &sOpened;` |
|     57 | 5201 | `	rc = UwrapCall(pH,"stream_open",4,apArg,&sRet);` |
|     57 | 5202 | `	if( rc == 0 && !ph7_value_to_bool(&sRet) ){` |
|    ! 0 | 5203 | `		rc = -1;` |
|    ! 0 | 5204 | `	}` |
|     57 | 5205 | `	PH7_MemObjRelease(&sPath);` |
|     57 | 5206 | `	PH7_MemObjRelease(&sMode);` |
|     57 | 5207 | `	PH7_MemObjRelease(&sOpts);` |
|     57 | 5208 | `	PH7_MemObjRelease(&sOpened);` |
|     57 | 5209 | `	PH7_MemObjRelease(&sRet);` |
|     57 | 5210 | `	if( rc != 0 ){` |
|    ! 0 | 5211 | `		PH7_ClassInstanceUnref(pH->pObj);` |
|    ! 0 | 5212 | `		SyMemBackendFree(&pVm->sAllocator,pH);` |
|    ! 0 | 5213 | `		return -1;` |
|      - | 5214 | `	}` |
|     57 | 5215 | `	*ppHandle = (void *)pH;` |
|     57 | 5216 | `	return PH7_OK;` |
|     30 | 5217 | `}` |
|      - | 5218 | `/* One xOpen thunk per slot (the device callbacks get no device pointer) */` |
|      - | 5219 | `#define PHL_UWRAP_THUNK(N) \` |
|      - | 5220 | `	static int UwrapOpen##N(const char *zName,int iMode,ph7_value *pResource,void **ppHandle) \` |
|      - | 5221 | `	{ return UwrapOpenSlot(N,zName,iMode,pResource,ppHandle); }` |
|     53 | 5222 | `PHL_UWRAP_THUNK(0)` |
|      5 | 5223 | `PHL_UWRAP_THUNK(1)` |
|    ! 0 | 5224 | `PHL_UWRAP_THUNK(2)` |
|    ! 0 | 5225 | `PHL_UWRAP_THUNK(3)` |
|    ! 0 | 5226 | `PHL_UWRAP_THUNK(4)` |
|    ! 0 | 5227 | `PHL_UWRAP_THUNK(5)` |
|    ! 0 | 5228 | `PHL_UWRAP_THUNK(6)` |
|    ! 0 | 5229 | `PHL_UWRAP_THUNK(7)` |
|      - | 5230 | `static int (* const g_aUwrapOpen[PHL_UWRAP_MAX])(const char *,int,ph7_value *,void **) = {` |
|      - | 5231 | `	UwrapOpen0,UwrapOpen1,UwrapOpen2,UwrapOpen3,` |
|      - | 5232 | `	UwrapOpen4,UwrapOpen5,UwrapOpen6,UwrapOpen7` |
|      - | 5233 | `};` |
|      - | 5234 | `/* Is this device already in the VM's list? (A slot survives its wrapper.) */` |
|     26 | 5235 | `static int UwrapDeviceInstalled(ph7_vm *pVm,const ph7_io_stream *pStream)` |
|      3 | 5236 | `{` |
|     29 | 5237 | `	ph7_io_stream **apDev = (ph7_io_stream **)SySetBasePtr(&pVm->aIOstream);` |
|      - | 5238 | `	sxu32 n;` |
|    137 | 5239 | `	for( n = 0 ; n < SySetUsed(&pVm->aIOstream) ; n++ ){` |
|    117 | 5240 | `		if( apDev[n] == pStream ){` |
|      7 | 5241 | `			return 1;` |
|      - | 5242 | `		}` |
|     57 | 5243 | `	}` |
|     23 | 5244 | `	return 0;` |
|     16 | 5245 | `}` |
|      - | 5246 | `/* Put a device back in service. */` |
|     26 | 5247 | `static void UwrapUnsuppressDevice(ph7_vm *pVm,const ph7_io_stream *pStream)` |
|      3 | 5248 | `{` |
|     29 | 5249 | `	const ph7_io_stream **apOff = (const ph7_io_stream **)SySetBasePtr(&pVm->aSuppressedIo);` |
|     29 | 5250 | `	sxu32 n,nKeep = 0;` |
|     39 | 5251 | `	for( n = 0 ; n < SySetUsed(&pVm->aSuppressedIo) ; n++ ){` |
|     11 | 5252 | `		if( apOff[n] == pStream ){` |
|      7 | 5253 | `			continue;` |
|      - | 5254 | `		}` |
|      5 | 5255 | `		apOff[nKeep++] = apOff[n];` |
|      3 | 5256 | `	}` |
|     29 | 5257 | `	SySetTruncate(&pVm->aSuppressedIo,nKeep);` |
|     29 | 5258 | `}` |
|      - | 5259 | `/*` |
|      - | 5260 | ` * bool stream_wrapper_register(string $protocol, string $class, int $flags = 0)` |
|      - | 5261 | ` * bool stream_wrapper_unregister(string $protocol)` |
|      - | 5262 | ` */` |
|     26 | 5263 | `PH7_PRIVATE int PH7_builtin_stream_wrapper_register(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      3 | 5264 | `{` |
|      - | 5265 | `	const char *zScheme,*zClass;` |
|     29 | 5266 | `	int nScheme,nClass,i,iFree = -1;` |
|     29 | 5267 | `	if( nArg < 2 ){` |
|    ! 0 | 5268 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 5269 | `		return PH7_OK;` |
|      - | 5270 | `	}` |
|     29 | 5271 | `	zScheme = ph7_value_to_string(apArg[0],&nScheme);` |
|     29 | 5272 | `	zClass  = ph7_value_to_string(apArg[1],&nClass);` |
|     26 | 5273 | `	if( nScheme < 1 \|\| nScheme >= (int)sizeof(g_aUwrap[0].zScheme)` |
|     29 | 5274 | `	 \|\| nClass < 1 \|\| nClass >= (int)sizeof(g_aUwrap[0].zClass) ){` |
|    ! 0 | 5275 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 5276 | `		return PH7_OK;` |
|      - | 5277 | `	}` |
|      - | 5278 | `	/* php: registering an already-taken protocol warns and returns false.` |
|      - | 5279 | `	 * (Scan the device list directly — PH7_VmGetStreamDevice falls back to` |
|      - | 5280 | `	 * the DEFAULT device for a scheme-less name, so it can't answer this.) */` |
|      - | 5281 | `	{` |
|     29 | 5282 | `		ph7_io_stream **apDev = (ph7_io_stream **)SySetBasePtr(&pCtx->pVm->aIOstream);` |
|      - | 5283 | `		sxu32 n;` |
|    143 | 5284 | `		for( n = 0 ; n < SySetUsed(&pCtx->pVm->aIOstream) ; n++ ){` |
|    117 | 5285 | `			if( PH7_VmStreamDeviceSuppressed(pCtx->pVm,apDev[n]) ){` |
|     11 | 5286 | `				continue; /* unregistered: the name is free again, which is the` |
|      - | 5287 | `				           * whole point of "replace file:// with my own" */` |
|      - | 5288 | `			}` |
|    104 | 5289 | `			if( (int)SyStrlen(apDev[n]->zName) == nScheme` |
|     63 | 5290 | `			 && SyStrnicmp(apDev[n]->zName,zScheme,(sxu32)nScheme) == 0 ){` |
|    ! 0 | 5291 | `				ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,` |
|    ! 0 | 5292 | `					"Protocol %.*s:// is already defined.",nScheme,zScheme);` |
|    ! 0 | 5293 | `				ph7_result_bool(pCtx,0);` |
|    ! 0 | 5294 | `				return PH7_OK;` |
|      - | 5295 | `			}` |
|     55 | 5296 | `		}` |
|      - | 5297 | `	}` |
|     33 | 5298 | `	for( i = 0 ; i < PHL_UWRAP_MAX ; i++ ){` |
|     33 | 5299 | `		if( g_aUwrap[i].pVm == 0 ){` |
|     29 | 5300 | `			iFree = i;` |
|     29 | 5301 | `			break;` |
|      - | 5302 | `		}` |
|      4 | 5303 | `	}` |
|     29 | 5304 | `	if( iFree < 0 ){` |
|    ! 0 | 5305 | `		ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,` |
|      - | 5306 | `			"Too many registered stream wrappers (PHL limit: %d)",PHL_UWRAP_MAX);` |
|    ! 0 | 5307 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 5308 | `		return PH7_OK;` |
|      - | 5309 | `	}` |
|      - | 5310 | `	{` |
|     29 | 5311 | `		uwrap_slot *pSlot = &g_aUwrap[iFree];` |
|     29 | 5312 | `		SyMemcpy(zScheme,pSlot->zScheme,(sxu32)nScheme);` |
|     29 | 5313 | `		pSlot->zScheme[nScheme] = 0;` |
|     29 | 5314 | `		SyMemcpy(zClass,pSlot->zClass,(sxu32)nClass);` |
|     29 | 5315 | `		pSlot->zClass[nClass] = 0;` |
|     29 | 5316 | `		pSlot->pVm = pCtx->pVm;` |
|      - | 5317 | `		/* $flags: php defines exactly one bit for it, STREAM_IS_URL, and it is the` |
|      - | 5318 | `		 * whole reason the argument exists — a wrapper that says it speaks to the` |
|      - | 5319 | `		 * NETWORK is the one allow_url_fopen and allow_url_include turn off. It was` |
|      - | 5320 | `		 * declared in the signature and read by nothing, so a wrapper registered as` |
|      - | 5321 | `		 * a URL was opened and INCLUDED like a local file whatever the` |
|      - | 5322 | `		 * configuration said. */` |
|     29 | 5323 | `		pSlot->bIsUrl = (nArg > 2 && (ph7_value_to_int64(apArg[2]) & PH7_STREAM_IS_URL) != 0);` |
|     29 | 5324 | `		SyZero(&pSlot->sStream,sizeof(ph7_io_stream));` |
|     29 | 5325 | `		pSlot->sStream.zName = pSlot->zScheme;` |
|     29 | 5326 | `		pSlot->sStream.iVersion = PH7_IO_STREAM_VERSION;` |
|     29 | 5327 | `		pSlot->sStream.xOpen = g_aUwrapOpen[iFree];` |
|     29 | 5328 | `		pSlot->sStream.xClose = UwrapClose;` |
|     29 | 5329 | `		pSlot->sStream.xRead = UwrapRead;` |
|     29 | 5330 | `		pSlot->sStream.xWrite = UwrapWrite;` |
|     29 | 5331 | `		pSlot->sStream.xSeek = UwrapSeek;` |
|     29 | 5332 | `		pSlot->sStream.xTell = UwrapTell;` |
|      - | 5333 | `		/* A slot is REUSED once its wrapper has been unregistered, and both the` |
|      - | 5334 | `		 * suppression set and the VM's device list still name it -- so lift the` |
|      - | 5335 | `		 * suppression and install the device only if it is not already there,` |
|      - | 5336 | `		 * or the freshly registered protocol would be born switched off (and` |
|      - | 5337 | `		 * listed twice). */` |
|     29 | 5338 | `		UwrapUnsuppressDevice(pCtx->pVm,&pSlot->sStream);` |
|     29 | 5339 | `		if( !UwrapDeviceInstalled(pCtx->pVm,&pSlot->sStream) ){` |
|     23 | 5340 | `			ph7_vm_config(pCtx->pVm,PH7_VM_CONFIG_IO_STREAM,&pSlot->sStream);` |
|     10 | 5341 | `		}` |
|      - | 5342 | `	}` |
|     29 | 5343 | `	ph7_result_bool(pCtx,1);` |
|     29 | 5344 | `	return PH7_OK;` |
|     16 | 5345 | `}` |
|      - | 5346 | `/*` |
|      - | 5347 | ` * Suppress a live device and, when it is a userland slot, retire the slot with` |
|      - | 5348 | ` * it. Answers 0 when nothing by that name was in service.` |
|      - | 5349 | ` *` |
|      - | 5350 | ` * The match is EXACT and case-SENSITIVE, which php's is too: opening a stream` |
|      - | 5351 | ` * folds the scheme ("FILE://x" reads a file), but unregister() and restore()` |
|      - | 5352 | ` * delete from the wrapper hash by the bytes the script wrote, so` |
|      - | 5353 | ` * stream_wrapper_unregister('FILE') fails where 'file' succeeds.` |
|      - | 5354 | ` */` |
|     14 | 5355 | `static int UwrapSuppressDevice(ph7_vm *pVm,const char *zScheme,int nScheme)` |
|      2 | 5356 | `{` |
|     16 | 5357 | `	ph7_io_stream **apDev = (ph7_io_stream **)SySetBasePtr(&pVm->aIOstream);` |
|     16 | 5358 | `	ph7_io_stream *pHit = 0;` |
|      - | 5359 | `	sxu32 n;` |
|      - | 5360 | `	int i;` |
|     84 | 5361 | `	for( n = 0 ; n < SySetUsed(&pVm->aIOstream) ; n++ ){` |
|     68 | 5362 | `		if( (int)SyStrlen(apDev[n]->zName) == nScheme` |
|     46 | 5363 | `		 && SyMemcmp(apDev[n]->zName,zScheme,(sxu32)nScheme) == 0` |
|     19 | 5364 | `		 && !PH7_VmStreamDeviceSuppressed(pVm,apDev[n]) ){` |
|     12 | 5365 | `			pHit = apDev[n]; /* the LIVE one is the last match */` |
|      5 | 5366 | `		}` |
|     36 | 5367 | `	}` |
|     16 | 5368 | `	if( pHit == 0 ){` |
|      5 | 5369 | `		return 0;` |
|      - | 5370 | `	}` |
|     12 | 5371 | `	if( SySetPut(&pVm->aSuppressedIo,(const void *)&pHit) != SXRET_OK ){` |
|    ! 0 | 5372 | `		return 0;` |
|      - | 5373 | `	}` |
|     44 | 5374 | `	for( i = 0 ; i < PHL_UWRAP_MAX ; i++ ){` |
|     40 | 5375 | `		if( g_aUwrap[i].pVm == pVm && &g_aUwrap[i].sStream == pHit ){` |
|      8 | 5376 | `			g_aUwrap[i].pVm = 0;` |
|      8 | 5377 | `			break;` |
|      - | 5378 | `		}` |
|     17 | 5379 | `	}` |
|     12 | 5380 | `	return 1;` |
|      9 | 5381 | `}` |
|      - | 5382 | `/*` |
|      - | 5383 | ` * bool stream_wrapper_unregister(string $protocol)` |
|      - | 5384 | ` *  Take a protocol out of service. It used to handle USERLAND slots only and` |
|      - | 5385 | ` *  answer FALSE for file/php/data/tcp, so the documented "replace file:// with` |
|      - | 5386 | ` *  my own wrapper" idiom failed loudly at the first step. A built-in is now` |
|      - | 5387 | ` *  suppressed per VM: PH7_VmGetStreamDevice() steps over it (including on the` |
|      - | 5388 | ` *  no-scheme default path, which is the same slot), stream_get_wrappers() stops` |
|      - | 5389 | ` *  naming it, and the name becomes free to register again.` |
|      - | 5390 | ` */` |
|     14 | 5391 | `PH7_PRIVATE int PH7_builtin_stream_wrapper_unregister(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      2 | 5392 | `{` |
|      - | 5393 | `	const char *zScheme;` |
|      - | 5394 | `	int nScheme;` |
|     16 | 5395 | `	if( nArg < 1 ){` |
|    ! 0 | 5396 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 5397 | `		return PH7_OK;` |
|      - | 5398 | `	}` |
|     16 | 5399 | `	zScheme = ph7_value_to_string(apArg[0],&nScheme);` |
|     16 | 5400 | `	if( nScheme > 0 && UwrapSuppressDevice(pCtx->pVm,zScheme,nScheme) ){` |
|     12 | 5401 | `		ph7_result_bool(pCtx,1);` |
|     12 | 5402 | `		return PH7_OK;` |
|      - | 5403 | `	}` |
|      7 | 5404 | `	ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,` |
|      2 | 5405 | `		"Unable to unregister protocol %.*s://",nScheme,zScheme);` |
|      5 | 5406 | `	ph7_result_bool(pCtx,0);` |
|      5 | 5407 | `	return PH7_OK;` |
|      9 | 5408 | `}` |
|      - | 5409 | `/*` |
|      - | 5410 | ` * bool stream_wrapper_restore(string $protocol)` |
|      - | 5411 | ` *  Put a BUILT-IN protocol back, whether it was unregistered or replaced. The` |
|      - | 5412 | ` *  other half of the override pair, and useless without it -- which is why the` |
|      - | 5413 | ` *  two ship together.` |
|      - | 5414 | ` *` |
|      - | 5415 | ` *  php's three answers: a protocol that was never built in is a warning and` |
|      - | 5416 | ` *  FALSE; one that is built in and was never touched is an E_NOTICE and TRUE` |
|      - | 5417 | ` *  (it is already what it should be); anything else is restored and TRUE.` |
|      - | 5418 | ` */` |
|     12 | 5419 | `PH7_PRIVATE int PH7_builtin_stream_wrapper_restore(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 5420 | `{` |
|     13 | 5421 | `	ph7_vm *pVm = pCtx->pVm;` |
|      - | 5422 | `	const ph7_io_stream **apOff;` |
|      - | 5423 | `	ph7_io_stream **apDev;` |
|      - | 5424 | `	const char *zScheme;` |
|     13 | 5425 | `	int nScheme,bBuiltin = 0,bChanged = 0,i;` |
|      - | 5426 | `	sxu32 n,nKeep;` |
|     13 | 5427 | `	if( nArg < 1 ){` |
|    ! 0 | 5428 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 5429 | `		return PH7_OK;` |
|      - | 5430 | `	}` |
|     13 | 5431 | `	zScheme = ph7_value_to_string(apArg[0],&nScheme);` |
|     13 | 5432 | `	apDev = (ph7_io_stream **)SySetBasePtr(&pVm->aIOstream);` |
|     73 | 5433 | `	for( n = 0 ; n < SySetUsed(&pVm->aIOstream) ; n++ ){` |
|     61 | 5434 | `		ph7_io_stream *pDev = apDev[n];` |
|     60 | 5435 | `		if( (int)SyStrlen(pDev->zName) != nScheme` |
|     44 | 5436 | `		 \|\| SyMemcmp(pDev->zName,zScheme,(sxu32)nScheme) != 0 ){` |
|     47 | 5437 | `			continue;` |
|      - | 5438 | `		}` |
|     15 | 5439 | `		if( UwrapIsSlotDevice(pDev) ){` |
|      - | 5440 | `			/* A userland wrapper standing in its place -- or one already` |
|      - | 5441 | `			 * withdrawn, which is still not a built-in. */` |
|      9 | 5442 | `			if( !PH7_VmStreamDeviceSuppressed(pVm,pDev) ){` |
|      5 | 5443 | `				bChanged = 1;` |
|      2 | 5444 | `			}` |
|      9 | 5445 | `			continue;` |
|      - | 5446 | `		}` |
|      7 | 5447 | `		bBuiltin = 1;` |
|      7 | 5448 | `		if( PH7_VmStreamDeviceSuppressed(pVm,pDev) ){` |
|      5 | 5449 | `			bChanged = 1;` |
|      2 | 5450 | `		}` |
|      4 | 5451 | `	}` |
|     13 | 5452 | `	if( !bBuiltin ){` |
|     10 | 5453 | `		ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,` |
|      3 | 5454 | `			"%.*s:// never existed, nothing to restore",nScheme,zScheme);` |
|      7 | 5455 | `		ph7_result_bool(pCtx,0);` |
|      7 | 5456 | `		return PH7_OK;` |
|      - | 5457 | `	}` |
|      7 | 5458 | `	if( !bChanged ){` |
|      - | 5459 | `		/* php answers TRUE here and says so at NOTICE level: the protocol is` |
|      - | 5460 | `		 * already the one it would restore. */` |
|      4 | 5461 | `		ph7_context_throw_error_format(pCtx,PH7_CTX_NOTICE,` |
|      1 | 5462 | `			"%.*s:// was never changed, nothing to restore",nScheme,zScheme);` |
|      3 | 5463 | `		ph7_result_bool(pCtx,1);` |
|      3 | 5464 | `		return PH7_OK;` |
|      - | 5465 | `	}` |
|      - | 5466 | `	/* Lift the suppression off the BUILT-IN first, by compacting the set... */` |
|      5 | 5467 | `	apOff = (const ph7_io_stream **)SySetBasePtr(&pVm->aSuppressedIo);` |
|      5 | 5468 | `	nKeep = 0;` |
|      9 | 5469 | `	for( n = 0 ; n < SySetUsed(&pVm->aSuppressedIo) ; n++ ){` |
|      5 | 5470 | `		const ph7_io_stream *pDev = apOff[n];` |
|      4 | 5471 | `		if( (int)SyStrlen(pDev->zName) == nScheme` |
|      4 | 5472 | `		 && SyMemcmp(pDev->zName,zScheme,(sxu32)nScheme) == 0` |
|      5 | 5473 | `		 && !UwrapIsSlotDevice(pDev) ){` |
|      5 | 5474 | `			continue; /* the built-in comes back */` |
|      - | 5475 | `		}` |
|    ! 0 | 5476 | `		apOff[nKeep++] = pDev;` |
|    ! 0 | 5477 | `	}` |
|      5 | 5478 | `	SySetTruncate(&pVm->aSuppressedIo,nKeep);` |
|      - | 5479 | `	/* ...then retire every userland wrapper standing in for the name. */` |
|     37 | 5480 | `	for( i = 0 ; i < PHL_UWRAP_MAX ; i++ ){` |
|     32 | 5481 | `		if( g_aUwrap[i].pVm == pVm` |
|     18 | 5482 | `		 && (int)SyStrlen(g_aUwrap[i].zScheme) == nScheme` |
|      5 | 5483 | `		 && SyMemcmp(g_aUwrap[i].zScheme,zScheme,(sxu32)nScheme) == 0 ){` |
|      5 | 5484 | `			const ph7_io_stream *pDead = &g_aUwrap[i].sStream;` |
|      5 | 5485 | `			g_aUwrap[i].pVm = 0;` |
|      5 | 5486 | `			if( !PH7_VmStreamDeviceSuppressed(pVm,pDead) ){` |
|      5 | 5487 | `				SySetPut(&pVm->aSuppressedIo,(const void *)&pDead);` |
|      2 | 5488 | `			}` |
|      2 | 5489 | `		}` |
|     17 | 5490 | `	}` |
|      5 | 5491 | `	ph7_result_bool(pCtx,1);` |
|      5 | 5492 | `	return PH7_OK;` |
|      7 | 5493 | `}` |
|      - | 5494 | `#ifdef PH7_ENABLE_NET` |
|      - | 5495 | `/*` |
|      - | 5496 | `` * php's socket address: `[transport://]host:port`. What a re-derivation gets`` |
|      - | 5497 | ` * wrong here is that BOTH halves have a diagnostic of their own, and neither is` |
|      - | 5498 | ` * the other: a transport this build does not carry is not a malformed address,` |
|      - | 5499 | ` * and an address with no port is not an unknown transport.` |
|      - | 5500 | ` */` |
|      - | 5501 | `#define SOCK_ADDR_OK        0` |
|      - | 5502 | `#define SOCK_ADDR_TRANSPORT 1 /* named a transport this build has not got */` |
|      - | 5503 | `#define SOCK_ADDR_PARSE     2 /* no port separator at all */` |
|      - | 5504 | `/*` |
|      - | 5505 | `` * php's port half is `atoi()` of whatever follows the FIRST colon, and the`` |
|      - | 5506 | ` * colon is looked for in every position but the LAST — which is the whole` |
|      - | 5507 | `` * difference between `127.0.0.1:` (php's "Failed to parse address") and`` |
|      - | 5508 | `` * `127.0.0.1:abc` (a port of 0, i.e. one the OS picks). A re-derivation that`` |
|      - | 5509 | ` * reads digits strictly refuses three addresses php accepts, and one that takes` |
|      - | 5510 | `` * the last colon reads `a:b:c` differently than php does.`` |
|      - | 5511 | ` */` |
|    136 | 5512 | `static int SockParsePort(const char *z,int n)` |
|      4 | 5513 | `{` |
|    140 | 5514 | `	int i = 0,iSign = 1,iVal = 0;` |
|    208 | 5515 | `	while( i < n && (z[i] == ' ' \|\| z[i] == '\t' \|\| z[i] == '\n' \|\| z[i] == '\r'` |
|    136 | 5516 | `	              \|\| z[i] == '\v' \|\| z[i] == '\f') ){` |
|    ! 0 | 5517 | `		i++;` |
|    ! 0 | 5518 | `	}` |
|    140 | 5519 | `	if( i < n && (z[i] == '+' \|\| z[i] == '-') ){` |
|    ! 0 | 5520 | `		iSign = z[i] == '-' ? -1 : 1;` |
|    ! 0 | 5521 | `		i++;` |
|    ! 0 | 5522 | `	}` |
|    672 | 5523 | `	for( ; i < n && z[i] >= '0' && z[i] <= '9' ; i++ ){` |
|    535 | 5524 | `		if( iVal < 1000000000 ){` |
|    535 | 5525 | `			iVal = iVal * 10 + (z[i] - '0');` |
|    266 | 5526 | `		}` |
|    269 | 5527 | `	}` |
|    140 | 5528 | `	return iSign * iVal;` |
|      4 | 5529 | `}` |
|    132 | 5530 | `static int SockParseAddress(const char *zAddr,int nAddr,char *zHost,int nHostBuf,int *pPort,` |
|      - | 5531 | `	const char **pzTransport,int *pnTransport,const char **pzRest,int *pnRest)` |
|      4 | 5532 | `{` |
|    136 | 5533 | `	const char *zRest = zAddr;` |
|    136 | 5534 | `	int nRest = nAddr,i,nHost = -1;` |
|    136 | 5535 | `	*pPort = 0;` |
|    136 | 5536 | `	*pzTransport = "tcp";` |
|    136 | 5537 | `	*pnTransport = 3;` |
|   1008 | 5538 | `	for( i = 0 ; i + 2 < nAddr ; i++ ){` |
|    954 | 5539 | `		if( zAddr[i] == ':' && zAddr[i+1] == '/' && zAddr[i+2] == '/' ){` |
|     82 | 5540 | `			*pzTransport = zAddr;` |
|     82 | 5541 | `			*pnTransport = i;` |
|     82 | 5542 | `			zRest = &zAddr[i+3];` |
|     82 | 5543 | `			nRest = nAddr - i - 3;` |
|     82 | 5544 | `			break;` |
|      - | 5545 | `		}` |
|    440 | 5546 | `	}` |
|    136 | 5547 | `	*pzRest = zRest;` |
|    136 | 5548 | `	*pnRest = nRest;` |
|    136 | 5549 | `	if( *pnTransport != 3 \|\| SyStrnicmp(*pzTransport,"tcp",3) != 0 ){` |
|      3 | 5550 | `		return SOCK_ADDR_TRANSPORT;` |
|      - | 5551 | `	}` |
|   1222 | 5552 | `	for( i = 0 ; i + 1 < nRest ; i++ ){` |
|   1216 | 5553 | `		if( zRest[i] == ':' ){` |
|    128 | 5554 | `			*pPort = SockParsePort(&zRest[i+1],nRest - i - 1);` |
|    128 | 5555 | `			nHost = i;` |
|    128 | 5556 | `			break;` |
|      - | 5557 | `		}` |
|    548 | 5558 | `	}` |
|    134 | 5559 | `	if( nHost < 0 ){` |
|      8 | 5560 | `		return SOCK_ADDR_PARSE;` |
|      - | 5561 | `	}` |
|    128 | 5562 | `	if( nHost >= nHostBuf ){` |
|    ! 0 | 5563 | `		nHost = nHostBuf - 1;` |
|    ! 0 | 5564 | `	}` |
|    128 | 5565 | `	if( nHost > 0 ){` |
|    124 | 5566 | `		SyMemcpy(zRest,zHost,(sxu32)nHost);` |
|     60 | 5567 | `	}` |
|    128 | 5568 | `	zHost[nHost] = 0;` |
|    128 | 5569 | `	return SOCK_ADDR_OK;` |
|     70 | 5570 | `}` |
|      - | 5571 | `/*` |
|      - | 5572 | ` * resource\|false fsockopen(string $hostname, int $port = -1, int &$error_code,` |
|      - | 5573 | ` *                          string &$error_message, ?float $timeout = null)` |
|      - | 5574 | ` * resource\|false stream_socket_client(string $address, int &$error_code,` |
|      - | 5575 | ` *                          string &$error_message, ?float $timeout = null, ...)` |
|      - | 5576 | ` * TCP only (the recorded scope: no ssl://, udp:// or unix:// yet).` |
|      - | 5577 | ` */` |
|     90 | 5578 | `PH7_PRIVATE int PH7_builtin_fsockopen(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      4 | 5579 | `{` |
|     94 | 5580 | `	const char *zFunc = ph7_function_name(pCtx);` |
|     94 | 5581 | `	int bClientForm = (zFunc[0] == 's'); /* stream_socket_client */` |
|     94 | 5582 | `	const char *zRaw,*zAddr,*zTransport,*zRest,*zErr = "";` |
|      - | 5583 | `	char zHost[256],zAddrBuf[352],zShowBuf[384],zMsg[512];` |
|      - | 5584 | `	const char *zShow;` |
|     94 | 5585 | `	int nRaw,nAddr,nShow,nTransport,nRest,iPortArg = -1,iPort = 0,iErrno = 0,iTimeoutMs = 0,rc;` |
|     94 | 5586 | `	int iFlags = PH7_STREAM_CLIENT_CONNECT,bPersist,bConnect;` |
|      - | 5587 | `	ph7_socket sock;` |
|      - | 5588 | `	io_private *pDev;` |
|     94 | 5589 | `	int iArgErrno = bClientForm ? 1 : 2;` |
|     94 | 5590 | `	int iArgErrstr = bClientForm ? 2 : 3;` |
|     94 | 5591 | `	int iArgTimeout = bClientForm ? 3 : 4;` |
|     94 | 5592 | `	phl_stream_ctx *pCtxRes = 0;` |
|      - | 5593 | `	ph7_sockopts sOpt;` |
|      - | 5594 | `	char zBindHost[256];` |
|     94 | 5595 | `	int bThrew = 0;` |
|     94 | 5596 | `	if( nArg < 1 ){` |
|    ! 0 | 5597 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 5598 | `		return PH7_OK;` |
|      - | 5599 | `	}` |
|     94 | 5600 | `	if( bClientForm ){` |
|      - | 5601 | ``		/* php's `?resource $context` — fsockopen()/pfsockopen() have no such`` |
|      - | 5602 | `		 * argument, so only the stream_socket_client() spelling takes one. */` |
|     46 | 5603 | `		pCtxRes = PH7_StreamCtxFromArg(pCtx,nArg,apArg,5,"$context",0,&bThrew);` |
|     46 | 5604 | `		if( bThrew ){` |
|    ! 0 | 5605 | `			return PH7_OK;` |
|      - | 5606 | `		}` |
|     21 | 5607 | `	}` |
|     94 | 5608 | `	zRaw = ph7_value_to_string(apArg[0],&nRaw);` |
|     94 | 5609 | `	if( !bClientForm && nArg > 1 && !ph7_value_is_null(apArg[1]) ){` |
|     49 | 5610 | `		iPortArg = ph7_value_to_int(apArg[1]);` |
|     24 | 5611 | `	}` |
|     94 | 5612 | `	if( bClientForm && nArg > 4 ){` |
|      - | 5613 | `		/* Declared in the signature and read by nothing until now, so the` |
|      - | 5614 | `		 * documented spellings did nothing and their constants were undefined` |
|      - | 5615 | `		 * fatals. */` |
|     26 | 5616 | `		iFlags = (int)ph7_value_to_int64(apArg[4]);` |
|     12 | 5617 | `	}` |
|      - | 5618 | `	/* pfsockopen() IS fsockopen() with this flag; php has no other difference` |
|      - | 5619 | `	 * between them. ASYNC_CONNECT is accepted and changes nothing here, because` |
|      - | 5620 | `	 * the connect() is blocking either way (§7.4 slice-2 (b)) — php reverts a` |
|      - | 5621 | `	 * socket it connected asynchronously to blocking mode too. */` |
|    115 | 5622 | `	bPersist = bClientForm ? (iFlags & PH7_STREAM_CLIENT_PERSISTENT) != 0` |
|     69 | 5623 | `		: (zFunc[0] == 'p');` |
|     94 | 5624 | `	bConnect = bClientForm ? (iFlags & PH7_STREAM_CLIENT_CONNECT) != 0 : 1;` |
|      - | 5625 | `	/* php builds ONE address out of fsockopen()'s two arguments — and only when` |
|      - | 5626 | ``	 * the port is a usable one, which is why `fsockopen($h)` reports the address`` |
|      - | 5627 | `	 * it could not parse rather than connecting to port 0. The address it SHOWS` |
|      - | 5628 | `	 * keeps the port either way. */` |
|     94 | 5629 | `	if( bClientForm \|\| iPortArg <= 0 ){` |
|     46 | 5630 | `		zAddr = zRaw;` |
|     46 | 5631 | `		nAddr = nRaw;` |
|     25 | 5632 | `	}else{` |
|     49 | 5633 | `		nAddr = (int)SyBufferFormat(zAddrBuf,sizeof(zAddrBuf),"%.*s:%d",nRaw,zRaw,iPortArg);` |
|     49 | 5634 | `		zAddr = zAddrBuf;` |
|      - | 5635 | `	}` |
|     94 | 5636 | `	if( bClientForm ){` |
|     46 | 5637 | `		zShow = zRaw;` |
|     46 | 5638 | `		nShow = nRaw;` |
|     25 | 5639 | `	}else{` |
|     49 | 5640 | `		nShow = (int)SyBufferFormat(zShowBuf,sizeof(zShowBuf),"%.*s:%d",nRaw,zRaw,iPortArg);` |
|     49 | 5641 | `		zShow = zShowBuf;` |
|      - | 5642 | `	}` |
|     94 | 5643 | `	rc = SockParseAddress(zAddr,nAddr,zHost,(int)sizeof(zHost),&iPort,&zTransport,&nTransport,` |
|      - | 5644 | `		&zRest,&nRest);` |
|     94 | 5645 | `	if( rc != SOCK_ADDR_OK ){` |
|    ! 0 | 5646 | `		if( rc == SOCK_ADDR_TRANSPORT ){` |
|      - | 5647 | `			/* php's own wording for a transport its build does not carry —` |
|      - | 5648 | `			 * which is what this engine's missing ones ARE (§7.4), and what a` |
|      - | 5649 | `			 * script reading $errstr is written against. This used to spell a` |
|      - | 5650 | `			 * message of PHL's own that no php ever answers. */` |
|    ! 0 | 5651 | `			SyBufferFormat(zMsg,sizeof(zMsg),` |
|      - | 5652 | `				"Unable to find the socket transport \"%.*s\" - did you forget to enable it when you configured PHP?",` |
|    ! 0 | 5653 | `				nTransport,zTransport);` |
|    ! 0 | 5654 | `		}else{` |
|    ! 0 | 5655 | `			SyBufferFormat(zMsg,sizeof(zMsg),"Failed to parse address \"%.*s\"",nRest,zRest);` |
|      - | 5656 | `		}` |
|    ! 0 | 5657 | `		SockAddressFailure(pCtx,apArg,nArg,iArgErrno,iArgErrstr,zShow,nShow,zMsg,0);` |
|    ! 0 | 5658 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 5659 | `		return PH7_OK;` |
|      - | 5660 | `	}` |
|     94 | 5661 | `	if( nArg > iArgTimeout && !ph7_value_is_null(apArg[iArgTimeout]) ){` |
|     82 | 5662 | `		double rTimeout = ph7_value_to_double(apArg[iArgTimeout]);` |
|     82 | 5663 | `		if( rTimeout > 0 ){` |
|     82 | 5664 | `			iTimeoutMs = (int)(rTimeout * 1000);` |
|     39 | 5665 | `		}` |
|     39 | 5666 | `	}` |
|     94 | 5667 | `	if( bPersist ){` |
|      - | 5668 | `		/* A live one for this address IS the answer: php hands the same resource` |
|      - | 5669 | `		 * back rather than opening a second connection to the same peer. */` |
|      - | 5670 | `		char zKey[320];` |
|      - | 5671 | `		io_private *pKept;` |
|     17 | 5672 | `		SockPersistKey(zKey,sizeof(zKey),bClientForm,zAddr,nAddr);` |
|     17 | 5673 | `		pKept = SockPersistFind(pCtx->pVm,zKey);` |
|     17 | 5674 | `		if( pKept ){` |
|      7 | 5675 | `			SockAddressSuccess(pCtx,apArg,nArg,iArgErrno,iArgErrstr);` |
|      7 | 5676 | `			ph7_result_resource(pCtx,pKept);` |
|      7 | 5677 | `			return PH7_OK;` |
|      - | 5678 | `		}` |
|      5 | 5679 | `	}` |
|     88 | 5680 | `	if( !bConnect ){` |
|      - | 5681 | `		/* php creates the socket while CONNECTING it, so a $flags without` |
|      - | 5682 | `		 * STREAM_CLIENT_CONNECT answers a stream with no socket behind it: no` |
|      - | 5683 | `		 * name at either end, reads false, writes 0, already at end of file. */` |
|      3 | 5684 | `		SockAddressSuccess(pCtx,apArg,nArg,iArgErrno,iArgErrstr);` |
|      3 | 5685 | `		pDev = SockWrapSocket(pCtx,PH7_NET_INVALID_SOCKET,zAddr,nAddr);` |
|      3 | 5686 | `		if( pDev == 0 ){` |
|    ! 0 | 5687 | `			ph7_result_bool(pCtx,0);` |
|    ! 0 | 5688 | `			return PH7_OK;` |
|      - | 5689 | `		}` |
|      3 | 5690 | `		pDev->pCtxRes = (void *)pCtxRes;` |
|      3 | 5691 | `		ph7_result_resource(pCtx,pDev);` |
|      3 | 5692 | `		return PH7_OK;` |
|      - | 5693 | `	}` |
|      - | 5694 | `	{` |
|      - | 5695 | ``		/* php reads the `socket` options at the moment it creates the socket:`` |
|      - | 5696 | `		 * so_reuseport and tcp_nodelay are a setsockopt on the fresh one, and` |
|      - | 5697 | `		 * bindto is the LOCAL address it takes before connecting. */` |
|     86 | 5698 | `		const char *zOptErr = 0;` |
|     86 | 5699 | `		if( SockCtxOptions(pCtxRes,&sOpt,zBindHost,(int)sizeof(zBindHost),&zOptErr) != 0 ){` |
|      - | 5700 | `			/* The one option failure php treats as a failed CONNECT rather than` |
|      - | 5701 | `			 * as a warning it can carry on past. */` |
|      3 | 5702 | `			SockAddressFailure(pCtx,apArg,nArg,iArgErrno,iArgErrstr,zShow,nShow,zOptErr,0);` |
|      3 | 5703 | `			ph7_result_bool(pCtx,0);` |
|      3 | 5704 | `			return PH7_OK;` |
|      - | 5705 | `		}` |
|      - | 5706 | `	}` |
|     84 | 5707 | `	sock = PH7_NetConnect(zHost,iPort,iTimeoutMs,&sOpt,&iErrno,&zErr);` |
|     84 | 5708 | `	if( sOpt.iBindErr ){` |
|      - | 5709 | `		/* php's own wording, and NEITHER shape stops the connection: the socket` |
|      - | 5710 | `		 * goes out from wherever the routing table would have sent it. It tells` |
|      - | 5711 | `		 * the two apart — a local address that is not a numeric literal at all` |
|      - | 5712 | `		 * names the host, one the OS refused to BIND names the address it tried` |
|      - | 5713 | `		 * and the reason. */` |
|      9 | 5714 | `		if( sOpt.iBindErr == PH7_SOCKOPT_BIND_RESOLVE ){` |
|      5 | 5715 | `			ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,"Invalid IP Address: %s",` |
|      4 | 5716 | `				sOpt.zBindHost ? sOpt.zBindHost : "");` |
|      3 | 5717 | `		}else{` |
|      - | 5718 | `			/* php RE-COMPOSES the address it tried from the parts it parsed, so` |
|      - | 5719 | ``			 * the quoted spelling is canonical: a `bindto` of "192.0.2.1:007"`` |
|      - | 5720 | `			 * is reported as '192.0.2.1:7'. */` |
|      5 | 5721 | `			ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,` |
|      - | 5722 | `				"Failed to bind to '%s:%d', system said: %s",` |
|      4 | 5723 | `				sOpt.zBindHost ? sOpt.zBindHost : "",sOpt.iBindPort,` |
|      2 | 5724 | `				PH7_NetStrError(sOpt.iBindErrno));` |
|      - | 5725 | `		}` |
|      4 | 5726 | `	}` |
|     84 | 5727 | `	if( sock == PH7_NET_INVALID_SOCKET ){` |
|     37 | 5728 | `		if( iErrno == PH7_NET_ERR_RESOLVE ){` |
|      3 | 5729 | `			zErr = SockResolveFailure(pCtx,zHost,zMsg,(int)sizeof(zMsg));` |
|      3 | 5730 | `			iErrno = 0;` |
|      1 | 5731 | `		}` |
|     37 | 5732 | `		SockAddressFailure(pCtx,apArg,nArg,iArgErrno,iArgErrstr,zShow,nShow,zErr,iErrno);` |
|     37 | 5733 | `		ph7_result_bool(pCtx,0);` |
|     37 | 5734 | `		return PH7_OK;` |
|      - | 5735 | `	}` |
|     47 | 5736 | `	SockAddressSuccess(pCtx,apArg,nArg,iArgErrno,iArgErrstr);` |
|      - | 5737 | `	/* Wrap the socket in an io_private so the whole f* family works on it. php` |
|      - | 5738 | ``	 * reports the ADDRESS it opened as the handle's `uri`, which is the same`` |
|      - | 5739 | `	 * one-address-out-of-two-arguments composition it connected through — so an` |
|      - | 5740 | `	 * argument naming only a host still records the port beside it. */` |
|     47 | 5741 | `	pDev = SockWrapSocket(pCtx,sock,zAddr,nAddr);` |
|     47 | 5742 | `	if( pDev == 0 ){` |
|    ! 0 | 5743 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 5744 | `		return PH7_OK;` |
|      - | 5745 | `	}` |
|      - | 5746 | `	/* php attaches the opener's context to a TRANSPORT stream and to nothing` |
|      - | 5747 | `	 * else — which is why stream_context_get_options() answers for a socket and` |
|      - | 5748 | `	 * answers the empty set for a file opened through the very same call. */` |
|     47 | 5749 | `	pDev->pCtxRes = (void *)pCtxRes;` |
|     47 | 5750 | `	SockArmDefaultTimeout(pCtx,pDev);` |
|     47 | 5751 | `	if( bPersist ){` |
|      - | 5752 | `		char zKey[320];` |
|     11 | 5753 | `		SockPersistKey(zKey,sizeof(zKey),bClientForm,zAddr,nAddr);` |
|     11 | 5754 | `		SockPersistKeep(pCtx->pVm,zKey,pDev);` |
|      - | 5755 | `		/* get_resource_type() names it apart, which is how a script can tell it` |
|      - | 5756 | `		 * asked for one at all. */` |
|     11 | 5757 | `		pDev->bPersist = 1;` |
|      5 | 5758 | `	}` |
|     47 | 5759 | `	ph7_result_resource(pCtx,pDev);` |
|     47 | 5760 | `	return PH7_OK;` |
|     49 | 5761 | `}` |
|      - | 5762 | `/*` |
|      - | 5763 | ` * resource\|false stream_socket_server(string $address, int &$error_code,` |
|      - | 5764 | ` *                    string &$error_message, int $flags = STREAM_SERVER_BIND\|STREAM_SERVER_LISTEN,` |
|      - | 5765 | ` *                    ?resource $context = null)` |
|      - | 5766 | ` *` |
|      - | 5767 | ` * The name a php program becomes a SERVER through, and a loud` |
|      - | 5768 | `` * `Call to undefined function` until now — so a script that listens on a port`` |
|      - | 5769 | ` * (a test double, a job runner, a line protocol) could not be spelled at all,` |
|      - | 5770 | ` * even though net.c had bind() and listen() all along.` |
|      - | 5771 | ` *` |
|      - | 5772 | ` * php's two flags are separate for a reason: BIND alone is what a datagram` |
|      - | 5773 | ` * socket wants (there is nothing to listen for), so LISTEN is what makes the` |
|      - | 5774 | ` * socket a stream server. Dropping LISTEN from a tcp:// address is therefore` |
|      - | 5775 | ` * a bound socket nothing can connect to, which is exactly what php answers.` |
|      - | 5776 | ` */` |
|     42 | 5777 | `PH7_PRIVATE int PH7_builtin_stream_socket_server(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      4 | 5778 | `{` |
|     46 | 5779 | `	const char *zAddr,*zTransport,*zRest,*zErr = "";` |
|      - | 5780 | `	char zHost[256];` |
|     46 | 5781 | `	int nAddr,nTransport,nRest,iPort = -1,iErrno = 0,iFlags,rc;` |
|      - | 5782 | `	ph7_socket sock;` |
|      - | 5783 | `	io_private *pDev;` |
|      - | 5784 | `	phl_stream_ctx *pCtxRes;` |
|      - | 5785 | `	ph7_sockopts sOpt;` |
|      - | 5786 | `	char zBindHost[256];` |
|     46 | 5787 | `	int bThrew = 0;` |
|     46 | 5788 | `	if( nArg < 1 ){` |
|    ! 0 | 5789 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 5790 | `		return PH7_OK;` |
|      - | 5791 | `	}` |
|     46 | 5792 | `	pCtxRes = PH7_StreamCtxFromArg(pCtx,nArg,apArg,4,"$context",0,&bThrew);` |
|     46 | 5793 | `	if( bThrew ){` |
|    ! 0 | 5794 | `		return PH7_OK;` |
|      - | 5795 | `	}` |
|      - | 5796 | ``	/* The signature row declares `string $address`, so whatever arrives has`` |
|      - | 5797 | `	 * already been screened; php's own ZPP then CASTS it, and refusing an int` |
|      - | 5798 | `` 	 * here would answer false in silence for `stream_socket_server(8080)` `` |
|      - | 5799 | `	 * where php reports the address it could not parse. */` |
|     46 | 5800 | `	zAddr = ph7_value_to_string(apArg[0],&nAddr);` |
|     30 | 5801 | `	iFlags = nArg > 3 ? (int)ph7_value_to_int64(apArg[3])` |
|     26 | 5802 | `		: (PH7_STREAM_SERVER_BIND\|PH7_STREAM_SERVER_LISTEN);` |
|     46 | 5803 | `	rc = SockParseAddress(zAddr,nAddr,zHost,(int)sizeof(zHost),&iPort,&zTransport,&nTransport,` |
|      - | 5804 | `		&zRest,&nRest);` |
|     46 | 5805 | `	if( rc != SOCK_ADDR_OK ){` |
|      - | 5806 | `		char zMsg[512];` |
|     10 | 5807 | `		if( rc == SOCK_ADDR_TRANSPORT ){` |
|      - | 5808 | `			/* php's own wording for a transport its build has not got, which is` |
|      - | 5809 | `			 * what udp://, unix:// and ssl:// are here (§7.4). */` |
|      4 | 5810 | `			SyBufferFormat(zMsg,sizeof(zMsg),` |
|      - | 5811 | `				"Unable to find the socket transport \"%.*s\" - did you forget to enable it when you configured PHP?",` |
|      1 | 5812 | `				nTransport,zTransport);` |
|      2 | 5813 | `		}else{` |
|      8 | 5814 | `			SyBufferFormat(zMsg,sizeof(zMsg),"Failed to parse address \"%.*s\"",nRest,zRest);` |
|      - | 5815 | `		}` |
|     10 | 5816 | `		SockAddressFailure(pCtx,apArg,nArg,1,2,zAddr,nAddr,zMsg,0);` |
|     10 | 5817 | `		ph7_result_bool(pCtx,0);` |
|     10 | 5818 | `		return PH7_OK;` |
|      - | 5819 | `	}` |
|     38 | 5820 | `	if( (iFlags & PH7_STREAM_SERVER_BIND) == 0 ){` |
|      - | 5821 | `		/* php creates the socket while BINDING it, so a $flags without` |
|      - | 5822 | `		 * STREAM_SERVER_BIND answers a socket stream with no socket behind it:` |
|      - | 5823 | `		 * it has no name, reads false, writes 0 and is already at end of file.` |
|      - | 5824 | ``		 * It does not even resolve the host — `stream_socket_server(':1', $e,`` |
|      - | 5825 | ``		 * $es, 0)` is a resource in php. */`` |
|      5 | 5826 | `		pDev = SockWrapSocket(pCtx,PH7_NET_INVALID_SOCKET,zAddr,nAddr);` |
|      5 | 5827 | `		if( pDev == 0 ){` |
|    ! 0 | 5828 | `			ph7_result_bool(pCtx,0);` |
|    ! 0 | 5829 | `			return PH7_OK;` |
|      - | 5830 | `		}` |
|      5 | 5831 | `		pDev->pCtxRes = (void *)pCtxRes;` |
|      5 | 5832 | `		SockAddressSuccess(pCtx,apArg,nArg,1,2);` |
|      5 | 5833 | `		ph7_result_resource(pCtx,pDev);` |
|      5 | 5834 | `		return PH7_OK;` |
|      - | 5835 | `	}` |
|     34 | 5836 | `	if( zHost[0] == 0 ){` |
|      - | 5837 | ``		/* An address with no host at all (`:8080`) is a name php asks the`` |
|      - | 5838 | `		 * resolver about and is refused for — NOT a wildcard bind. Answering` |
|      - | 5839 | `		 * 0.0.0.0 for it would put a listener on every interface of the` |
|      - | 5840 | `		 * machine, which is the unsafe direction. */` |
|      - | 5841 | `		char zMsg[512];` |
|      3 | 5842 | `		SockAddressFailure(pCtx,apArg,nArg,1,2,zAddr,nAddr,` |
|      1 | 5843 | `			SockResolveFailure(pCtx,zHost,zMsg,(int)sizeof(zMsg)),0);` |
|      2 | 5844 | `		ph7_result_bool(pCtx,0);` |
|      2 | 5845 | `		return PH7_OK;` |
|      - | 5846 | `	}` |
|      - | 5847 | `	{` |
|      - | 5848 | ``		/* The server half reads `backlog`, `so_reuseport` and `tcp_nodelay`;`` |
|      - | 5849 | ``		 * `bindto` is not one of its options, because the address argument IS`` |
|      - | 5850 | `		 * where a server binds (php ignores it here too). */` |
|     32 | 5851 | `		const char *zOptErr = 0;` |
|     32 | 5852 | `		if( SockCtxOptions(pCtxRes,&sOpt,zBindHost,(int)sizeof(zBindHost),&zOptErr) != 0 ){` |
|    ! 0 | 5853 | `			SockAddressFailure(pCtx,apArg,nArg,1,2,zAddr,nAddr,zOptErr,0);` |
|    ! 0 | 5854 | `			ph7_result_bool(pCtx,0);` |
|    ! 0 | 5855 | `			return PH7_OK;` |
|      - | 5856 | `		}` |
|     32 | 5857 | `		sOpt.zBindHost = 0;` |
|      - | 5858 | `	}` |
|     32 | 5859 | `	sock = PH7_NetBind(zHost,iPort,0,(iFlags & PH7_STREAM_SERVER_LISTEN) != 0,` |
|      - | 5860 | `		SOCK_LISTEN_BACKLOG,&sOpt,&iErrno,&zErr);` |
|     32 | 5861 | `	if( sock == PH7_NET_INVALID_SOCKET ){` |
|      - | 5862 | `		char zMsg[512];` |
|      5 | 5863 | `		if( iErrno == PH7_NET_ERR_RESOLVE ){` |
|      3 | 5864 | `			zErr = SockResolveFailure(pCtx,zHost,zMsg,(int)sizeof(zMsg));` |
|      1 | 5865 | `		}` |
|      - | 5866 | `		/* php reports no OS code for a refused ADDRESS — only a connect() that` |
|      - | 5867 | `		 * reached the network carries one — so this stays 0 for every arm. */` |
|      5 | 5868 | `		SockAddressFailure(pCtx,apArg,nArg,1,2,zAddr,nAddr,zErr,0);` |
|      5 | 5869 | `		ph7_result_bool(pCtx,0);` |
|      5 | 5870 | `		return PH7_OK;` |
|      - | 5871 | `	}` |
|     27 | 5872 | `	SockAddressSuccess(pCtx,apArg,nArg,1,2);` |
|     27 | 5873 | `	pDev = SockWrapSocket(pCtx,sock,zAddr,nAddr);` |
|     27 | 5874 | `	if( pDev == 0 ){` |
|    ! 0 | 5875 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 5876 | `		return PH7_OK;` |
|      - | 5877 | `	}` |
|     27 | 5878 | `	pDev->pCtxRes = (void *)pCtxRes;` |
|     27 | 5879 | `	ph7_result_resource(pCtx,pDev);` |
|     27 | 5880 | `	return PH7_OK;` |
|     25 | 5881 | `}` |
|      - | 5882 | `/*` |
|      - | 5883 | ` * resource\|false stream_socket_accept(resource $socket, ?float $timeout = null,` |
|      - | 5884 | ` *                                    string &$peer_name = null)` |
|      - | 5885 | ` *` |
|      - | 5886 | ` * The other half of a server, and the one with the timing in it. php waits at` |
|      - | 5887 | `` * most `default_socket_timeout` seconds by default — NOT forever — and reports`` |
|      - | 5888 | ` * an expired wait as a warning plus false, which is what lets a single-threaded` |
|      - | 5889 | ` * server do something else between connections. A negative timeout blocks.` |
|      - | 5890 | ` */` |
|     32 | 5891 | `PH7_PRIVATE int PH7_builtin_stream_socket_accept(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      4 | 5892 | `{` |
|      - | 5893 | `	io_private *pDev,*pOut;` |
|      - | 5894 | `	ph7_socket *pSock,sock;` |
|      - | 5895 | `	char zPeer[128];` |
|     36 | 5896 | `	int rc,bTimedOut = 0,iTimeoutMs;` |
|     36 | 5897 | `	if( nArg < 1 ){` |
|    ! 0 | 5898 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 5899 | `		return PH7_OK;` |
|      - | 5900 | `	}` |
|     36 | 5901 | `	pDev = StreamSettingArgNamed(pCtx,apArg[0],1,"socket",&rc);` |
|     36 | 5902 | `	if( pDev == 0 ){` |
|      3 | 5903 | `		return rc;` |
|      - | 5904 | `	}` |
|     34 | 5905 | `	pSock = IoPrivateSocket(pDev);` |
|     34 | 5906 | `	if( pSock == 0 ){` |
|      - | 5907 | `		/* Not a socket at all. php's own answer for it reads oddly and is what` |
|      - | 5908 | `		 * a script sees: the accept never reaches the network, so there is no` |
|      - | 5909 | `		 * OS error to report and php asks its error table for code 0. */` |
|      3 | 5910 | `		ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"Accept failed: Unknown error");` |
|      3 | 5911 | `		ph7_result_bool(pCtx,0);` |
|      3 | 5912 | `		return PH7_OK;` |
|      - | 5913 | `	}` |
|     31 | 5914 | `	if( nArg > 1 && !ph7_value_is_null(apArg[1]) ){` |
|     31 | 5915 | `		double rTimeout = ph7_value_to_double(apArg[1]);` |
|     31 | 5916 | `		iTimeoutMs = rTimeout < 0 ? -1 : (int)(rTimeout * 1000);` |
|     17 | 5917 | `	}else{` |
|    ! 0 | 5918 | `		iTimeoutMs = (int)(PH7_VmIniGetInt(pCtx->pVm,"default_socket_timeout",60) * 1000);` |
|    ! 0 | 5919 | `		if( iTimeoutMs < 0 ){` |
|    ! 0 | 5920 | `			iTimeoutMs = -1;` |
|    ! 0 | 5921 | `		}` |
|      - | 5922 | `	}` |
|     31 | 5923 | `	sock = PH7_NetAcceptTimed(*pSock,iTimeoutMs,&bTimedOut,zPeer,(int)sizeof(zPeer));` |
|     31 | 5924 | `	if( *pSock == PH7_NET_INVALID_SOCKET ){` |
|      - | 5925 | `		/* php waits and then reports the expiry; there is nothing to wait on. */` |
|      3 | 5926 | `		bTimedOut = 1;` |
|      1 | 5927 | `	}` |
|     31 | 5928 | `	if( sock == PH7_NET_INVALID_SOCKET ){` |
|      8 | 5929 | `		ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,"Accept failed: %s",` |
|      4 | 5930 | `			bTimedOut ? "Connection timed out" : PH7_NetStrError(PH7_NetLastError()));` |
|      6 | 5931 | `		ph7_result_bool(pCtx,0);` |
|      6 | 5932 | `		return PH7_OK;` |
|      - | 5933 | `	}` |
|     27 | 5934 | `	if( nArg > 2 ){` |
|      3 | 5935 | `		ph7_value *pTmp = ph7_context_new_scalar(pCtx);` |
|      3 | 5936 | `		if( pTmp ){` |
|      3 | 5937 | `			ph7_value_string(pTmp,zPeer,-1);` |
|      3 | 5938 | `			PH7_VmStoreArgByRef(pCtx->pVm,apArg[2],pTmp);` |
|      1 | 5939 | `		}` |
|      1 | 5940 | `	}` |
|      - | 5941 | ``	/* php reports no `uri` for an ACCEPTED connection: nothing opened it by`` |
|      - | 5942 | `	 * name, so stream_get_meta_data() has no address to answer with. */` |
|     27 | 5943 | `	pOut = SockWrapSocket(pCtx,sock,0,0);` |
|     27 | 5944 | `	if( pOut == 0 ){` |
|    ! 0 | 5945 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 5946 | `		return PH7_OK;` |
|      - | 5947 | `	}` |
|     27 | 5948 | `	SockArmDefaultTimeout(pCtx,pOut);` |
|     27 | 5949 | `	ph7_result_resource(pCtx,pOut);` |
|     27 | 5950 | `	return PH7_OK;` |
|     20 | 5951 | `}` |
|      - | 5952 | `/*` |
|      - | 5953 | `` * recvfrom()'s `&$address`: the sender for a datagram, empty for a connected`` |
|      - | 5954 | ` * stream that has none, and NULL for a read that did not happen — php writes it` |
|      - | 5955 | ` * on every call rather than leaving the caller's previous value in place.` |
|      - | 5956 | ` */` |
|     10 | 5957 | `static void SockStoreAddress(ph7_context *pCtx,ph7_value **apArg,int nArg,int iArg,const char *zAddr)` |
|      1 | 5958 | `{` |
|      - | 5959 | `	ph7_value *pTmp;` |
|     11 | 5960 | `	if( iArg >= nArg ){` |
|      9 | 5961 | `		return;` |
|      - | 5962 | `	}` |
|      3 | 5963 | `	pTmp = ph7_context_new_scalar(pCtx);` |
|      3 | 5964 | `	if( pTmp == 0 ){` |
|    ! 0 | 5965 | `		return;` |
|      - | 5966 | `	}` |
|      3 | 5967 | `	if( zAddr ){` |
|      3 | 5968 | `		ph7_value_string(pTmp,zAddr,-1);` |
|      2 | 5969 | `	}else{` |
|    ! 0 | 5970 | `		ph7_value_null(pTmp);` |
|      - | 5971 | `	}` |
|      3 | 5972 | `	PH7_VmStoreArgByRef(pCtx->pVm,apArg[iArg],pTmp);` |
|      6 | 5973 | `}` |
|      - | 5974 | `/*` |
|      - | 5975 | ` * bool stream_socket_shutdown(resource $stream, int $mode)` |
|      - | 5976 | ` *` |
|      - | 5977 | ` * The half-close: "I am done SENDING" without closing a handle the program` |
|      - | 5978 | ` * still wants to read from, which is how every request/response protocol tells` |
|      - | 5979 | ` * its peer the request is over. Nothing else can say it — fclose() takes the` |
|      - | 5980 | ` * read side with it.` |
|      - | 5981 | ` */` |
|      8 | 5982 | `PH7_PRIVATE int PH7_builtin_stream_socket_shutdown(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 5983 | `{` |
|      - | 5984 | `	io_private *pDev;` |
|      - | 5985 | `	ph7_socket *pSock;` |
|      - | 5986 | `	ph7_int64 iHow;` |
|      - | 5987 | `	int rc;` |
|      9 | 5988 | `	if( nArg < 2 ){` |
|    ! 0 | 5989 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 5990 | `		return PH7_OK;` |
|      - | 5991 | `	}` |
|      9 | 5992 | `	pDev = StreamSettingArgNamed(pCtx,apArg[0],1,"stream",&rc);` |
|      9 | 5993 | `	if( pDev == 0 ){` |
|    ! 0 | 5994 | `		return rc;` |
|      - | 5995 | `	}` |
|      9 | 5996 | `	iHow = ph7_value_to_int64(apArg[1]);` |
|      9 | 5997 | `	if( iHow != PH7_STREAM_SHUT_RD && iHow != PH7_STREAM_SHUT_WR && iHow != PH7_STREAM_SHUT_RDWR ){` |
|      - | 5998 | `		/* php names the three constants rather than the numbers behind them. */` |
|      4 | 5999 | `		return PH7_VmThrowException(pCtx,"ValueError",` |
|      - | 6000 | `			"%s(): Argument #2 ($mode) must be one of STREAM_SHUT_RD, STREAM_SHUT_WR, or STREAM_SHUT_RDWR",` |
|      1 | 6001 | `			ph7_function_name(pCtx));` |
|      - | 6002 | `	}` |
|      7 | 6003 | `	pSock = IoPrivateSocket(pDev);` |
|      7 | 6004 | `	if( pSock == 0 \|\| *pSock == PH7_NET_INVALID_SOCKET ){` |
|      - | 6005 | `		/* Not a socket: php answers false in silence, since there is no` |
|      - | 6006 | `		 * direction to shut down. */` |
|      3 | 6007 | `		ph7_result_bool(pCtx,0);` |
|      3 | 6008 | `		return PH7_OK;` |
|      - | 6009 | `	}` |
|      5 | 6010 | `	rc = PH7_NetShutdown(*pSock,(int)iHow) == PH7_OK;` |
|      5 | 6011 | `	if( rc && iHow != PH7_STREAM_SHUT_WR && PH7_NetAtEnd(*pSock) ){` |
|      - | 6012 | `		/* The read side is gone AND nothing is queued behind it, so this handle` |
|      - | 6013 | `		 * is at its end: php answers feof() for a socket by probing it, and a` |
|      - | 6014 | ``		 * `while (!feof($s))` drain loop after a half-close would otherwise spin`` |
|      - | 6015 | `		 * on a stream that can never answer again. Bytes that HAD arrived are` |
|      - | 6016 | `		 * still handed over — which is why the answer is probed rather than` |
|      - | 6017 | `		 * assumed, and why the device's own latch stays clear. */` |
|      3 | 6018 | `		pDev->bEof = 1;` |
|      1 | 6019 | `	}` |
|      5 | 6020 | `	ph7_result_bool(pCtx,rc);` |
|      5 | 6021 | `	return PH7_OK;` |
|      5 | 6022 | `}` |
|      - | 6023 | `/*` |
|      - | 6024 | ` * string\|false stream_socket_recvfrom(resource $socket, int $length, int $flags = 0,` |
|      - | 6025 | ` *                                    string &$address = null)` |
|      - | 6026 | ` * int\|false stream_socket_sendto(resource $socket, string $data, int $flags = 0,` |
|      - | 6027 | ` *                               string $address = "")` |
|      - | 6028 | ` *` |
|      - | 6029 | ` * The pair that reaches the socket UNDERNEATH the stream: php's own asks the` |
|      - | 6030 | `` * socket rather than the handle's read buffer, which is why `STREAM_PEEK` can`` |
|      - | 6031 | ` * look at bytes without consuming them (nothing else in the family can) and why` |
|      - | 6032 | ` * a recvfrom() on a handle a line read has already buffered WAITS for more.` |
|      - | 6033 | `` * The `$address` is what a datagram carries and a connected stream does not.`` |
|      - | 6034 | ` */` |
|     12 | 6035 | `PH7_PRIVATE int PH7_builtin_stream_socket_recvfrom(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 6036 | `{` |
|      - | 6037 | `	io_private *pDev;` |
|      - | 6038 | `	ph7_socket *pSock;` |
|      - | 6039 | `	ph7_int64 nLen;` |
|      - | 6040 | `	char zAddr[128],*zBuf;` |
|     13 | 6041 | `	int rc,iFlags = 0,n;` |
|     13 | 6042 | `	if( nArg < 2 ){` |
|    ! 0 | 6043 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 6044 | `		return PH7_OK;` |
|      - | 6045 | `	}` |
|     13 | 6046 | `	pDev = StreamSettingArgNamed(pCtx,apArg[0],1,"socket",&rc);` |
|     13 | 6047 | `	if( pDev == 0 ){` |
|    ! 0 | 6048 | `		return rc;` |
|      - | 6049 | `	}` |
|     13 | 6050 | `	nLen = ph7_value_to_int64(apArg[1]);` |
|     13 | 6051 | `	if( nLen < 1 ){` |
|      4 | 6052 | `		return PH7_VmThrowException(pCtx,"ValueError",` |
|      1 | 6053 | `			"%s(): Argument #2 ($length) must be greater than 0",ph7_function_name(pCtx));` |
|      - | 6054 | `	}` |
|     11 | 6055 | `	if( nArg > 2 ){` |
|      3 | 6056 | `		iFlags = (int)ph7_value_to_int64(apArg[2]);` |
|      1 | 6057 | `	}` |
|     11 | 6058 | `	pSock = IoPrivateSocket(pDev);` |
|     11 | 6059 | `	if( pSock == 0 \|\| *pSock == PH7_NET_INVALID_SOCKET ){` |
|      3 | 6060 | `		SockStoreAddress(pCtx,apArg,nArg,3,0);` |
|      3 | 6061 | `		ph7_result_bool(pCtx,0);` |
|      3 | 6062 | `		return PH7_OK;` |
|      - | 6063 | `	}` |
|      9 | 6064 | `	if( nLen > 0x7FFFFFF0 ){` |
|    ! 0 | 6065 | `		nLen = 0x7FFFFFF0;` |
|    ! 0 | 6066 | `	}` |
|      9 | 6067 | `	zBuf = (char *)ph7_context_alloc_chunk(pCtx,(unsigned int)nLen,FALSE,TRUE);` |
|      9 | 6068 | `	if( zBuf == 0 ){` |
|    ! 0 | 6069 | `		return PH7_ContextMemoryError(pCtx);` |
|      - | 6070 | `	}` |
|      9 | 6071 | `	zAddr[0] = 0;` |
|      9 | 6072 | `	n = PH7_NetRecvFrom(*pSock,zBuf,(int)nLen,iFlags,zAddr,(int)sizeof(zAddr));` |
|      - | 6073 | `	/* php writes the out-param on every call: the sender's address for a read` |
|      - | 6074 | `	 * that happened (empty for a connected stream, which has none to report) and` |
|      - | 6075 | `	 * NULL for one that did not — never the caller's previous value. */` |
|      9 | 6076 | `	SockStoreAddress(pCtx,apArg,nArg,3,n < 0 ? 0 : zAddr);` |
|      9 | 6077 | `	if( n < 0 ){` |
|    ! 0 | 6078 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 6079 | `	}else{` |
|      9 | 6080 | `		ph7_result_string(pCtx,zBuf,n);` |
|      - | 6081 | `	}` |
|      9 | 6082 | `	ph7_context_free_chunk(pCtx,zBuf);` |
|      9 | 6083 | `	return PH7_OK;` |
|      7 | 6084 | `}` |
|     12 | 6085 | `PH7_PRIVATE int PH7_builtin_stream_socket_sendto(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 6086 | `{` |
|      - | 6087 | `	io_private *pDev;` |
|      - | 6088 | `	ph7_socket *pSock;` |
|     13 | 6089 | `	const char *zData,*zSentTo = "";` |
|      - | 6090 | `	char zHost[256];` |
|     13 | 6091 | `	int rc,iFlags = 0,nData,n,iPort = 0,nSentTo = 0,iErr = 0;` |
|     13 | 6092 | `	if( nArg < 2 ){` |
|    ! 0 | 6093 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 6094 | `		return PH7_OK;` |
|      - | 6095 | `	}` |
|     13 | 6096 | `	pDev = StreamSettingArgNamed(pCtx,apArg[0],1,"socket",&rc);` |
|     13 | 6097 | `	if( pDev == 0 ){` |
|    ! 0 | 6098 | `		return rc;` |
|      - | 6099 | `	}` |
|     13 | 6100 | `	zData = ph7_value_to_string(apArg[1],&nData);` |
|     13 | 6101 | `	if( nArg > 2 ){` |
|      7 | 6102 | `		iFlags = (int)ph7_value_to_int64(apArg[2]);` |
|      3 | 6103 | `	}` |
|     13 | 6104 | `	zHost[0] = 0;` |
|     13 | 6105 | `	if( nArg > 3 && ph7_value_is_string(apArg[3]) ){` |
|      7 | 6106 | `		int nAddr,i,nHost = -1;` |
|      7 | 6107 | `		const char *zAddr = ph7_value_to_string(apArg[3],&nAddr);` |
|      7 | 6108 | `		if( nAddr > 0 ){` |
|      - | 6109 | `			/* php parses THIS address without looking for a transport at all —` |
|      - | 6110 | ``			 * the first colon is the separator, so `udp://1.2.3.4:53` names the`` |
|      - | 6111 | `			 * host "udp" — and an address it cannot turn into a sockaddr is a` |
|      - | 6112 | `			 * refusal rather than a send to the connected peer, which is where` |
|      - | 6113 | `			 * the bytes would otherwise silently go. */` |
|     49 | 6114 | `			for( i = 0 ; i + 1 < nAddr ; i++ ){` |
|     45 | 6115 | `				if( zAddr[i] == ':' ){` |
|      3 | 6116 | `					iPort = SockParsePort(&zAddr[i+1],nAddr - i - 1);` |
|      3 | 6117 | `					nHost = i;` |
|      3 | 6118 | `					break;` |
|      - | 6119 | `				}` |
|     22 | 6120 | `			}` |
|      7 | 6121 | `			if( nHost < 0 ){` |
|      7 | 6122 | `				ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,` |
|      2 | 6123 | ``					"Failed to parse `%.*s' into a valid network address",nAddr,zAddr);`` |
|      5 | 6124 | `				ph7_result_bool(pCtx,0);` |
|      5 | 6125 | `				return PH7_OK;` |
|      - | 6126 | `			}` |
|      3 | 6127 | `			if( nHost >= (int)sizeof(zHost) ){` |
|    ! 0 | 6128 | `				nHost = (int)sizeof(zHost) - 1;` |
|    ! 0 | 6129 | `			}` |
|      3 | 6130 | `			if( nHost > 0 ){` |
|      3 | 6131 | `				SyMemcpy(zAddr,zHost,(sxu32)nHost);` |
|      1 | 6132 | `			}` |
|      3 | 6133 | `			zHost[nHost] = 0;` |
|      3 | 6134 | `			zSentTo = zAddr;` |
|      3 | 6135 | `			nSentTo = nAddr;` |
|      1 | 6136 | `		}` |
|      1 | 6137 | `	}` |
|      9 | 6138 | `	pSock = IoPrivateSocket(pDev);` |
|      9 | 6139 | `	if( pSock == 0 \|\| *pSock == PH7_NET_INVALID_SOCKET ){` |
|      - | 6140 | `		/* php answers -1 here rather than false: this one reports the send()` |
|      - | 6141 | `		 * result, and it never made a call. */` |
|      3 | 6142 | `		ph7_result_int(pCtx,-1);` |
|      3 | 6143 | `		return PH7_OK;` |
|      - | 6144 | `	}` |
|      7 | 6145 | `	n = PH7_NetSendTo(*pSock,(const void *)zData,nData,iFlags,zHost,iPort,&iErr);` |
|      7 | 6146 | `	if( iErr == PH7_NET_ERR_RESOLVE ){` |
|      - | 6147 | `		/* php says it three times for one failure — the resolver's own text, the` |
|      - | 6148 | `		 * name it could not resolve, and the address it therefore could not` |
|      - | 6149 | `		 * parse — and answers FALSE rather than the -1 a failed send gives. */` |
|      - | 6150 | `		char zMsg[512];` |
|    ! 0 | 6151 | `		SockResolveFailure(pCtx,zHost,zMsg,(int)sizeof(zMsg));` |
|    ! 0 | 6152 | ``		ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,"Failed to resolve `%s': %s",zHost,zMsg);`` |
|    ! 0 | 6153 | `		ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,` |
|    ! 0 | 6154 | ``			"Failed to parse `%.*s' into a valid network address",nSentTo,zSentTo);`` |
|    ! 0 | 6155 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 6156 | `		return PH7_OK;` |
|      - | 6157 | `	}` |
|      7 | 6158 | `	if( n < 0 ){` |
|      - | 6159 | `		/* php reports the OS text and hands back the -1 send() answered — this` |
|      - | 6160 | `		 * one never answers false, which is why a caller compares it against 0` |
|      - | 6161 | `		 * rather than testing it for truth. The trailing newline is php's own. */` |
|      4 | 6162 | `		ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,"%s\n",` |
|      1 | 6163 | `			PH7_NetStrError(PH7_NetLastError()));` |
|      1 | 6164 | `	}` |
|      7 | 6165 | `	ph7_result_int(pCtx,n);` |
|      7 | 6166 | `	return PH7_OK;` |
|      7 | 6167 | `}` |
|      - | 6168 | `/*` |
|      - | 6169 | ` * array\|false stream_socket_pair(int $domain, int $type, int $protocol)` |
|      - | 6170 | ` *` |
|      - | 6171 | ` * Two connected sockets with no address between them — the two-way pipe a` |
|      - | 6172 | ` * program hands a child, or a test double hands the code under test. Which` |
|      - | 6173 | ` * $domain works is the OS's answer and not php's: POSIX has AF_UNIX and refuses` |
|      - | 6174 | ` * AF_INET, and Windows is the other way round (php emulates the pair over the` |
|      - | 6175 | ` * loopback there, and so does this).` |
|      - | 6176 | ` */` |
|      4 | 6177 | `PH7_PRIVATE int PH7_builtin_stream_socket_pair(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 6178 | `{` |
|      - | 6179 | `	ph7_socket aSock[2];` |
|      - | 6180 | `	io_private *apDev[2];` |
|      - | 6181 | `	ph7_value *pArr,*pVal;` |
|      5 | 6182 | `	int iErrno = 0,i;` |
|      5 | 6183 | `	if( nArg < 3 ){` |
|    ! 0 | 6184 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 6185 | `		return PH7_OK;` |
|      - | 6186 | `	}` |
|      6 | 6187 | `	if( PH7_NetSocketPair((int)ph7_value_to_int64(apArg[0]),(int)ph7_value_to_int64(apArg[1]),` |
|      7 | 6188 | `		(int)ph7_value_to_int64(apArg[2]),aSock,&iErrno) != PH7_OK ){` |
|      - | 6189 | `		/* php reports the OS code and its text, in that order and in brackets. */` |
|      4 | 6190 | `		ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,"Failed to create sockets: [%d]: %s",` |
|      1 | 6191 | `			iErrno,PH7_NetStrError(iErrno));` |
|      3 | 6192 | `		ph7_result_bool(pCtx,0);` |
|      3 | 6193 | `		return PH7_OK;` |
|      - | 6194 | `	}` |
|      3 | 6195 | `	pArr = ph7_context_new_array(pCtx);` |
|      3 | 6196 | `	pVal = ph7_context_new_scalar(pCtx);` |
|      3 | 6197 | `	apDev[0] = apDev[1] = 0;` |
|      3 | 6198 | `	if( pArr == 0 \|\| pVal == 0 ){` |
|    ! 0 | 6199 | `		PH7_NetClose(aSock[0]);` |
|    ! 0 | 6200 | `		PH7_NetClose(aSock[1]);` |
|    ! 0 | 6201 | `		return PH7_ContextMemoryError(pCtx);` |
|      - | 6202 | `	}` |
|      7 | 6203 | `	for( i = 0 ; i < 2 ; i++ ){` |
|      - | 6204 | `		/* No uri: nothing opened these by name, which is what php reports. */` |
|      5 | 6205 | `		apDev[i] = SockWrapSocket(pCtx,aSock[i],0,0);` |
|      5 | 6206 | `		if( apDev[i] == 0 ){` |
|      - | 6207 | `			/* SockWrapSocket closed the one it could not wrap; the OTHER end is` |
|      - | 6208 | `			 * still ours to close, wrapped or not. */` |
|    ! 0 | 6209 | `			if( i == 0 ){` |
|    ! 0 | 6210 | `				PH7_NetClose(aSock[1]);` |
|    ! 0 | 6211 | `			}else{` |
|    ! 0 | 6212 | `				SockCloseWrapped(pCtx,apDev[0]);` |
|      - | 6213 | `			}` |
|    ! 0 | 6214 | `			return PH7_ContextMemoryError(pCtx);` |
|      - | 6215 | `		}` |
|      - | 6216 | `		/* A pair has no transport of its own, and php labels it apart from a` |
|      - | 6217 | `		 * tcp:// stream for exactly that reason. */` |
|      5 | 6218 | `		((sock_private *)apDev[i]->pHandle)->bGeneric = 1;` |
|      5 | 6219 | `		SockArmDefaultTimeout(pCtx,apDev[i]);` |
|      3 | 6220 | `	}` |
|      7 | 6221 | `	for( i = 0 ; i < 2 ; i++ ){` |
|      5 | 6222 | `		ph7_value_resource(pVal,apDev[i]);` |
|      5 | 6223 | `		ph7_array_add_elem(pArr,0,pVal);` |
|      3 | 6224 | `	}` |
|      3 | 6225 | `	ph7_result_value(pCtx,pArr);` |
|      3 | 6226 | `	return PH7_OK;` |
|      3 | 6227 | `}` |
|      - | 6228 | `/*` |
|      - | 6229 | ` * string\|false stream_socket_get_name(resource $socket, bool $remote)` |
|      - | 6230 | ` *` |
|      - | 6231 | `` * Which address this socket sits on (`$remote` false) or is talking to (true).`` |
|      - | 6232 | `` * It is the only way to learn the port a server bound with `:0` actually got,`` |
|      - | 6233 | ` * so a test that needs a free port had to guess one without it.` |
|      - | 6234 | ` */` |
|     50 | 6235 | `PH7_PRIVATE int PH7_builtin_stream_socket_get_name(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      4 | 6236 | `{` |
|      - | 6237 | `	io_private *pDev;` |
|      - | 6238 | `	ph7_socket *pSock;` |
|      - | 6239 | `	char zName[128];` |
|      - | 6240 | `	int rc;` |
|     54 | 6241 | `	if( nArg < 2 ){` |
|    ! 0 | 6242 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 6243 | `		return PH7_OK;` |
|      - | 6244 | `	}` |
|     54 | 6245 | `	pDev = StreamSettingArgNamed(pCtx,apArg[0],1,"socket",&rc);` |
|     54 | 6246 | `	if( pDev == 0 ){` |
|      5 | 6247 | `		return rc;` |
|      - | 6248 | `	}` |
|     50 | 6249 | `	pSock = IoPrivateSocket(pDev);` |
|     50 | 6250 | `	if( pSock == 0 \|\| PH7_NetSockName(*pSock,ph7_value_to_bool(apArg[1]),zName,(int)sizeof(zName)) != PH7_OK ){` |
|      - | 6251 | `		/* php answers false for a stream that is not a socket, and for the peer` |
|      - | 6252 | `		 * of a socket that is not connected — an unaccepted server. */` |
|     15 | 6253 | `		ph7_result_bool(pCtx,0);` |
|     15 | 6254 | `		return PH7_OK;` |
|      - | 6255 | `	}` |
|     37 | 6256 | `	ph7_result_string(pCtx,zName,-1);` |
|     37 | 6257 | `	return PH7_OK;` |
|     29 | 6258 | `}` |
|      - | 6259 | `#endif /*` |
|      - | 6260 | ` * The stream SETTINGS family. Every one of these was a loud` |
|      - | 6261 | `` * `Call to undefined function` — so a program that puts a socket in`` |
|      - | 6262 | ` * non-blocking mode, bounds a read with a timeout, or asks whether a stream` |
|      - | 6263 | ` * can be locked before calling flock() did not run at all.` |
|      - | 6264 | ` *` |
|      - | 6265 | ` * The shared preamble: php refuses a non-resource with a TypeError naming the` |
|      - | 6266 | ` * parameter, and an already-closed handle the same way.` |
|      - | 6267 | ` */` |
|    478 | 6268 | `static io_private * StreamSettingArgNamed(ph7_context *pCtx,ph7_value *pArg,int iPos,` |
|      - | 6269 | `	const char *zName,int *pRc)` |
|      5 | 6270 | `{` |
|      - | 6271 | `	io_private *pDev;` |
|    483 | 6272 | `	*pRc = PH7_OK;` |
|    483 | 6273 | `	if( !ph7_value_is_resource(pArg) ){` |
|     20 | 6274 | `		*pRc = PH7_VmThrowException(pCtx,"TypeError",` |
|      - | 6275 | `			"%s(): Argument #%d ($%s) must be of type resource, %s given",` |
|      6 | 6276 | `			ph7_function_name(pCtx),iPos,zName,ph7_type_name(pArg));` |
|     14 | 6277 | `		return 0;` |
|      - | 6278 | `	}` |
|    471 | 6279 | `	pDev = (io_private *)ph7_value_to_resource(pArg);` |
|    471 | 6280 | `	if( IO_PRIVATE_INVALID(pDev) ){` |
|      4 | 6281 | `		*pRc = PH7_VmThrowException(pCtx,"TypeError",` |
|      - | 6282 | `			"%s(): Argument #%d ($%s) must be an open stream resource",` |
|      1 | 6283 | `			ph7_function_name(pCtx),iPos,zName);` |
|      3 | 6284 | `		return 0;` |
|      - | 6285 | `	}` |
|    469 | 6286 | `	return pDev;` |
|    244 | 6287 | `}` |
|      - | 6288 | ``/* The whole settings family names its one handle `$stream`; the copy names two. */`` |
|    120 | 6289 | `static io_private * StreamSettingArg(ph7_context *pCtx,ph7_value *pArg,int *pRc)` |
|      4 | 6290 | `{` |
|    124 | 6291 | `	return StreamSettingArgNamed(pCtx,pArg,1,"stream",pRc);` |
|      4 | 6292 | `}` |
|      - | 6293 | `/* The same screen, for the filter family in vfs_filter.c. */` |
|    170 | 6294 | `PH7_PRIVATE io_private * PH7_StreamHandleArg(ph7_context *pCtx,ph7_value *pArg,int iPos,` |
|      - | 6295 | `	const char *zName,int *pRc)` |
|      2 | 6296 | `{` |
|    172 | 6297 | `	return StreamSettingArgNamed(pCtx,pArg,iPos,zName,pRc);` |
|      2 | 6298 | `}` |
|      - | 6299 | `/* The tcp:// socket behind a handle, or 0 for any other device. */` |
|    292 | 6300 | `static ph7_socket * IoPrivateSocket(io_private *pDev)` |
|      5 | 6301 | `{` |
|      - | 6302 | `#ifdef PH7_ENABLE_NET` |
|    297 | 6303 | `	if( pDev->pStream == &sTCP_Stream && pDev->pHandle ){` |
|    229 | 6304 | `		return &((sock_private *)pDev->pHandle)->sock;` |
|      - | 6305 | `	}` |
|      - | 6306 | `#endif` |
|     33 | 6307 | `	SXUNUSED(pDev); /* cc warning when NET is off */` |
|     70 | 6308 | `	return 0;` |
|    151 | 6309 | `}` |
|      - | 6310 | `/*` |
|      - | 6311 | ` * bool stream_set_blocking(resource $stream, bool $enable)` |
|      - | 6312 | ` *` |
|      - | 6313 | ` * php sets the mode AT the descriptor and answers TRUE either way; a stream` |
|      - | 6314 | ` * with no descriptor — a memory buffer, a data:// payload — keeps reporting` |
|      - | 6315 | ` * itself blocked, which is why the flag is only recorded when it took. On` |
|      - | 6316 | ` * Windows php's plain-files device has no O_NONBLOCK to set, so every such` |
|      - | 6317 | ` * stream (a file, a pipe, php://stdin) answers FALSE there.` |
|      - | 6318 | ` */` |
|     30 | 6319 | `PH7_PRIVATE int PH7_builtin_stream_set_blocking(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      4 | 6320 | `{` |
|      - | 6321 | `	io_private *pDev;` |
|      - | 6322 | `	int rc,bEnable,fd;` |
|      - | 6323 | `	ph7_socket *pSock;` |
|     34 | 6324 | `	if( nArg < 2 ){` |
|    ! 0 | 6325 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 6326 | `		return PH7_OK;` |
|      - | 6327 | `	}` |
|     34 | 6328 | `	pDev = StreamSettingArg(pCtx,apArg[0],&rc);` |
|     34 | 6329 | `	if( pDev == 0 ){` |
|      3 | 6330 | `		return rc;` |
|      - | 6331 | `	}` |
|     32 | 6332 | `	bEnable = ph7_value_to_bool(apArg[1]);` |
|     32 | 6333 | `	pSock = IoPrivateSocket(pDev);` |
|     32 | 6334 | `	if( pSock ){` |
|      - | 6335 | `#ifdef PH7_ENABLE_NET` |
|      9 | 6336 | `		if( *pSock == PH7_NET_INVALID_SOCKET ){` |
|      - | 6337 | `			/* No socket to set the mode on: php's own answer is FALSE, which is` |
|      - | 6338 | `			 * the one place this family reports a setting that did not take. */` |
|      3 | 6339 | `			ph7_result_bool(pCtx,0);` |
|      3 | 6340 | `			return PH7_OK;` |
|      - | 6341 | `		}` |
|      6 | 6342 | `		PH7_NetSetBlocking(*pSock,bEnable);` |
|      - | 6343 | `#endif` |
|      6 | 6344 | `		pDev->bNonBlock = (sxu8)(bEnable ? 0 : 1);` |
|      4 | 6345 | `	}else{` |
|      - | 6346 | `#ifdef __WINNT__` |
|      1 | 6347 | `		if( PH7_StreamIsPlainDevice(pDev) ){` |
|      1 | 6348 | `			ph7_result_bool(pCtx,0);` |
|      1 | 6349 | `			return PH7_OK;` |
|      - | 6350 | `		}` |
|      - | 6351 | `#endif` |
|     23 | 6352 | `		fd = PH7_StreamPosixFd(pDev);` |
|     22 | 6353 | `		if( fd >= 0 ){` |
|      - | 6354 | `#ifndef __WINNT__` |
|     14 | 6355 | `			int iFlags = fcntl(fd,F_GETFL,0);` |
|     14 | 6356 | `			if( iFlags >= 0 ){` |
|     14 | 6357 | `				if( bEnable ){` |
|      4 | 6358 | `					iFlags &= ~O_NONBLOCK;` |
|      2 | 6359 | `				}else{` |
|     10 | 6360 | `					iFlags \|= O_NONBLOCK;` |
|      - | 6361 | `				}` |
|     14 | 6362 | `				if( fcntl(fd,F_SETFL,iFlags) == 0 ){` |
|     14 | 6363 | `					pDev->bNonBlock = (sxu8)(bEnable ? 0 : 1);` |
|      7 | 6364 | `				}` |
|      7 | 6365 | `			}` |
|      - | 6366 | `#endif` |
|      7 | 6367 | `		}` |
|      - | 6368 | `	}` |
|     29 | 6369 | `	ph7_result_bool(pCtx,1);` |
|     29 | 6370 | `	return PH7_OK;` |
|     19 | 6371 | `}` |
|      - | 6372 | `/*` |
|      - | 6373 | ` * bool stream_set_timeout(resource $stream, int $seconds, int $microseconds = 0)` |
|      - | 6374 | ` *` |
|      - | 6375 | ` * php answers TRUE only for a stream whose transport HAS a timeout — a socket —` |
|      - | 6376 | ` * and FALSE for every file, pipe and memory buffer, because there is nothing` |
|      - | 6377 | ` * to wait on. Silently accepting it for a file would tell a caller its read is` |
|      - | 6378 | ` * bounded when it is not.` |
|      - | 6379 | ` */` |
|      6 | 6380 | `PH7_PRIVATE int PH7_builtin_stream_set_timeout(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 6381 | `{` |
|      - | 6382 | `	io_private *pDev;` |
|      - | 6383 | `	ph7_socket *pSock;` |
|      - | 6384 | `	int rc;` |
|      7 | 6385 | `	if( nArg < 2 ){` |
|    ! 0 | 6386 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 6387 | `		return PH7_OK;` |
|      - | 6388 | `	}` |
|      7 | 6389 | `	pDev = StreamSettingArg(pCtx,apArg[0],&rc);` |
|      7 | 6390 | `	if( pDev == 0 ){` |
|    ! 0 | 6391 | `		return rc;` |
|      - | 6392 | `	}` |
|      7 | 6393 | `	pSock = IoPrivateSocket(pDev);` |
|      7 | 6394 | `	if( pSock == 0 ){` |
|      7 | 6395 | `		ph7_result_bool(pCtx,0);` |
|      7 | 6396 | `		return PH7_OK;` |
|      - | 6397 | `	}` |
|      - | 6398 | `#ifdef PH7_ENABLE_NET` |
|      - | 6399 | `	{` |
|    ! 0 | 6400 | `		ph7_int64 iSec = ph7_value_to_int64(apArg[1]);` |
|    ! 0 | 6401 | `		ph7_int64 iUsec = nArg > 2 ? ph7_value_to_int64(apArg[2]) : 0;` |
|    ! 0 | 6402 | `		if( iSec < 0 ){` |
|    ! 0 | 6403 | `			iSec = 0;` |
|    ! 0 | 6404 | `		}` |
|    ! 0 | 6405 | `		if( iUsec < 0 ){` |
|    ! 0 | 6406 | `			iUsec = 0;` |
|    ! 0 | 6407 | `		}` |
|    ! 0 | 6408 | `		PH7_NetSetRwTimeout(*pSock,iSec,iUsec);` |
|      - | 6409 | `		/* An expired read answers FALSE and says so through the metadata;` |
|      - | 6410 | `		 * without the armed flag it is indistinguishable from a non-blocking` |
|      - | 6411 | `		 * one, which answers "". */` |
|    ! 0 | 6412 | `		pDev->bHasTimeout = 1;` |
|    ! 0 | 6413 | `		pDev->bTimedOut = 0;` |
|      - | 6414 | `	}` |
|      - | 6415 | `#endif` |
|    ! 0 | 6416 | `	ph7_result_bool(pCtx,1);` |
|    ! 0 | 6417 | `	return PH7_OK;` |
|      4 | 6418 | `}` |
|      - | 6419 | `/*` |
|      - | 6420 | ` * int stream_set_chunk_size(resource $stream, int $size)` |
|      - | 6421 | ` *` |
|      - | 6422 | ` * Answers the PREVIOUS size, which is what makes the setting restorable, and` |
|      - | 6423 | ` * refuses a non-positive one the way php does.` |
|      - | 6424 | ` */` |
|     44 | 6425 | `PH7_PRIVATE int PH7_builtin_stream_set_chunk_size(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 6426 | `{` |
|      - | 6427 | `	io_private *pDev;` |
|      - | 6428 | `	ph7_int64 nSize;` |
|      - | 6429 | `	int rc;` |
|     45 | 6430 | `	if( nArg < 2 ){` |
|    ! 0 | 6431 | `		ph7_result_int(pCtx,-1);` |
|    ! 0 | 6432 | `		return PH7_OK;` |
|      - | 6433 | `	}` |
|     45 | 6434 | `	pDev = StreamSettingArg(pCtx,apArg[0],&rc);` |
|     45 | 6435 | `	if( pDev == 0 ){` |
|    ! 0 | 6436 | `		return rc;` |
|      - | 6437 | `	}` |
|     45 | 6438 | `	nSize = ph7_value_to_int64(apArg[1]);` |
|     45 | 6439 | `	if( nSize < 1 ){` |
|      5 | 6440 | `		return PH7_VmThrowException(pCtx,"ValueError",` |
|      - | 6441 | `			"stream_set_chunk_size(): Argument #2 ($size) must be greater than 0");` |
|      - | 6442 | `	}` |
|     41 | 6443 | `	if( nSize > (ph7_int64)SXI32_HIGH ){` |
|      - | 6444 | `		/* php's own ceiling: the size is an int on its side, and storing a` |
|      - | 6445 | `		 * larger one made the NEXT call report a size no caller ever set. */` |
|      3 | 6446 | `		return PH7_VmThrowException(pCtx,"ValueError",` |
|      - | 6447 | `			"stream_set_chunk_size(): Argument #2 ($size) is too large");` |
|      - | 6448 | `	}` |
|     39 | 6449 | `	ph7_result_int64(pCtx,(ph7_int64)pDev->nChunk);` |
|     39 | 6450 | `	pDev->nChunk = (sxu32)nSize;` |
|     39 | 6451 | `	return PH7_OK;` |
|     23 | 6452 | `}` |
|      - | 6453 | `/*` |
|      - | 6454 | ` * int stream_set_read_buffer(resource $stream, int $size)` |
|      - | 6455 | ` * int stream_set_write_buffer(resource $stream, int $size)  [set_file_buffer]` |
|      - | 6456 | ` *` |
|      - | 6457 | ` * php's stream layer has no stdio buffer left to hand these to: the read side` |
|      - | 6458 | ` * answers 0 (accepted) and the write side -1 (unsupported), for every stream` |
|      - | 6459 | ` * and every size. Both are still validated arguments, so a bad handle is the` |
|      - | 6460 | ` * same TypeError the rest of the family raises.` |
|      - | 6461 | ` */` |
|      6 | 6462 | `PH7_PRIVATE int PH7_builtin_stream_set_read_buffer(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 6463 | `{` |
|      7 | 6464 | `	int rc = PH7_OK;` |
|      7 | 6465 | `	if( nArg < 2 ){` |
|    ! 0 | 6466 | `		ph7_result_int(pCtx,-1);` |
|    ! 0 | 6467 | `		return PH7_OK;` |
|      - | 6468 | `	}` |
|      7 | 6469 | `	if( StreamSettingArg(pCtx,apArg[0],&rc) == 0 ){` |
|    ! 0 | 6470 | `		return rc;` |
|      - | 6471 | `	}` |
|      7 | 6472 | `	ph7_result_int(pCtx,0);` |
|      7 | 6473 | `	return PH7_OK;` |
|      4 | 6474 | `}` |
|     12 | 6475 | `PH7_PRIVATE int PH7_builtin_stream_set_write_buffer(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 6476 | `{` |
|     13 | 6477 | `	int rc = PH7_OK;` |
|     13 | 6478 | `	if( nArg < 2 ){` |
|    ! 0 | 6479 | `		ph7_result_int(pCtx,-1);` |
|    ! 0 | 6480 | `		return PH7_OK;` |
|      - | 6481 | `	}` |
|     13 | 6482 | `	if( StreamSettingArg(pCtx,apArg[0],&rc) == 0 ){` |
|    ! 0 | 6483 | `		return rc;` |
|      - | 6484 | `	}` |
|     13 | 6485 | `	ph7_result_int(pCtx,-1);` |
|     13 | 6486 | `	return PH7_OK;` |
|      7 | 6487 | `}` |
|      - | 6488 | `/*` |
|      - | 6489 | ` * int\|false stream_copy_to_stream(resource $from, resource $to,` |
|      - | 6490 | ` *                                 ?int $length = null, int $offset = 0)` |
|      - | 6491 | ` *` |
|      - | 6492 | ` * The everyday way to move bytes between two open streams, and a loud` |
|      - | 6493 | `` * `Call to undefined function` here until now — so the workaround was`` |
|      - | 6494 | `` * `fwrite($to, stream_get_contents($from))`, which reads the WHOLE source into`` |
|      - | 6495 | ` * memory first. A NULL or negative $length is "the rest"; a POSITIVE $offset` |
|      - | 6496 | ` * seeks the source first and is php's only failure shape short of a broken` |
|      - | 6497 | ` * write.` |
|      - | 6498 | ` */` |
|     38 | 6499 | `PH7_PRIVATE int PH7_builtin_stream_copy_to_stream(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 6500 | `{` |
|      - | 6501 | `	io_private *pFrom,*pTo;` |
|     39 | 6502 | `	ph7_int64 nWant = -1,nOfft = 0,nTotal = 0;` |
|      - | 6503 | `	char zBuf[8192];` |
|      - | 6504 | `	int rc;` |
|     39 | 6505 | `	if( nArg < 2 ){` |
|    ! 0 | 6506 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 6507 | `		return PH7_OK;` |
|      - | 6508 | `	}` |
|     39 | 6509 | `	pFrom = StreamSettingArgNamed(pCtx,apArg[0],1,"from",&rc);` |
|     39 | 6510 | `	if( pFrom == 0 ){` |
|      3 | 6511 | `		return rc;` |
|      - | 6512 | `	}` |
|     37 | 6513 | `	pTo = StreamSettingArgNamed(pCtx,apArg[1],2,"to",&rc);` |
|     37 | 6514 | `	if( pTo == 0 ){` |
|      3 | 6515 | `		return rc;` |
|      - | 6516 | `	}` |
|     34 | 6517 | `	if( pFrom->pStream == 0 \|\| pFrom->pStream->xRead == 0` |
|     35 | 6518 | `	 \|\| pTo->pStream == 0 \|\| pTo->pStream->xWrite == 0 ){` |
|    ! 0 | 6519 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 6520 | `		return PH7_OK;` |
|      - | 6521 | `	}` |
|     35 | 6522 | `	if( nArg > 2 && !ph7_value_is_null(apArg[2]) ){` |
|     11 | 6523 | `		nWant = ph7_value_to_int64(apArg[2]);` |
|      5 | 6524 | `	}` |
|     35 | 6525 | `	if( nArg > 3 ){` |
|     19 | 6526 | `		nOfft = ph7_value_to_int64(apArg[3]);` |
|      9 | 6527 | `	}` |
|     35 | 6528 | `	if( nOfft > 0 ){` |
|      - | 6529 | `		/* php seeks the SOURCE and gives up loudly when it cannot: a pipe has` |
|      - | 6530 | `		 * no position to move to, and silently copying from wherever it` |
|      - | 6531 | `		 * happens to be would answer for a different slice of the stream. */` |
|      8 | 6532 | `		if( pFrom->pStream->xSeek == 0` |
|      8 | 6533 | `		 \|\| pFrom->pStream->xSeek(pFrom->pHandle,nOfft,0/*SEEK_SET*/) != PH7_OK ){` |
|      2 | 6534 | `			if( pFrom->pStream->xSeek == 0 ){` |
|      2 | 6535 | `				ph7_context_throw_error(pCtx,PH7_CTX_WARNING,` |
|      - | 6536 | `					"stream_copy_to_stream(): Stream does not support seeking");` |
|      1 | 6537 | `			}` |
|      3 | 6538 | `			ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,` |
|      1 | 6539 | `				"stream_copy_to_stream(): Failed to seek to position %qd in the stream",nOfft);` |
|      2 | 6540 | `			ph7_result_bool(pCtx,0);` |
|      2 | 6541 | `			return PH7_OK;` |
|      - | 6542 | `		}` |
|      7 | 6543 | `		ResetIOPrivate(pFrom);` |
|      3 | 6544 | `	}` |
|     33 | 6545 | `	if( nWant == 0 ){` |
|      3 | 6546 | `		ph7_result_int(pCtx,0);` |
|      3 | 6547 | `		return PH7_OK;` |
|      - | 6548 | `	}` |
|      - | 6549 | `#ifdef __WINNT__` |
|      - | 6550 | `	/* An unfiltered plain-file source goes through php's memory-mapped copy,` |
|      - | 6551 | `	 * whose Windows view at the end of the file is a failure: see` |
|      - | 6552 | `	 * PH7_WinFileMapsEmptyView(). */` |
|      - | 6553 | `	if( pFrom->pStream == &sWinFileStream && pFrom->pReadFilters == 0 && pFrom->pWriteFilters == 0` |
|      1 | 6554 | `	 && PH7_WinFileMapsEmptyView(pFrom->pHandle,SyBlobLength(&pFrom->sBuffer) > pFrom->nOfft` |
|      - | 6555 | `			? (ph7_int64)(SyBlobLength(&pFrom->sBuffer) - pFrom->nOfft) : 0) ){` |
|      1 | 6556 | `		ph7_result_bool(pCtx,0);` |
|      1 | 6557 | `		return PH7_OK;` |
|      - | 6558 | `	}` |
|      - | 6559 | `#endif` |
|      - | 6560 | `	/* The destination may be sitting past its own read-ahead; the write has to` |
|      - | 6561 | `	 * land where the SCRIPT is, the rule fwrite() follows. */` |
|     31 | 6562 | `	StreamSeekBackForWrite(pTo);` |
|     39 | 6563 | `	for(;;){` |
|     55 | 6564 | `		ph7_int64 nAsk = (ph7_int64)sizeof(zBuf);` |
|      - | 6565 | `		ph7_int64 nRead,nWr;` |
|     55 | 6566 | `		if( nWant > 0 && nWant - nTotal < nAsk ){` |
|     13 | 6567 | `			nAsk = nWant - nTotal;` |
|      6 | 6568 | `		}` |
|     55 | 6569 | `		if( nAsk < 1 ){` |
|      5 | 6570 | `			break;` |
|      - | 6571 | `		}` |
|     51 | 6572 | `		nRead = PH7_StreamRead(pFrom,zBuf,nAsk);` |
|     51 | 6573 | `		if( nRead < 1 ){` |
|     27 | 6574 | `			break;` |
|      - | 6575 | `		}` |
|     25 | 6576 | `		nWr = PH7_StreamWrite(pTo,(const void *)zBuf,nRead);` |
|     25 | 6577 | `		if( nWr < 0 ){` |
|    ! 0 | 6578 | `			break;` |
|      - | 6579 | `		}` |
|     25 | 6580 | `		nTotal += nWr;` |
|     25 | 6581 | `		if( nWr < nRead ){` |
|    ! 0 | 6582 | `			break;` |
|      - | 6583 | `		}` |
|      1 | 6584 | `	}` |
|     31 | 6585 | `	ph7_result_int64(pCtx,nTotal);` |
|     31 | 6586 | `	return PH7_OK;` |
|     20 | 6587 | `}` |
|      - | 6588 | `/*` |
|      - | 6589 | ` * int\|false stream_select(?array &$read, ?array &$write, ?array &$except,` |
|      - | 6590 | ` *                         ?int $seconds, ?int $microseconds = null)` |
|      - | 6591 | ` *` |
|      - | 6592 | ` * The name that makes a program WAIT on several streams at once, and the reason` |
|      - | 6593 | ` * the settings family that shipped beside it had nothing to wait with: a` |
|      - | 6594 | ` * non-blocking read tells you a stream is not ready, and only this tells you` |
|      - | 6595 | ` * WHEN it becomes ready. It is what a proc_open() pipe pump, a socket server` |
|      - | 6596 | ` * loop and every event loop written in php is built on, and it was a loud` |
|      - | 6597 | `` * `Call to undefined function`.`` |
|      - | 6598 | ` *` |
|      - | 6599 | ` * php's own shape, and the parts of it a re-derivation misses: the arrays are` |
|      - | 6600 | ` * REWRITTEN in place to hold only the ready entries, under their original keys;` |
|      - | 6601 | ` * a stream that cannot be represented as a descriptor is a warning naming its` |
|      - | 6602 | ` * TYPE, not a failure; nothing selectable at all is an Error rather than 0; and` |
|      - | 6603 | ` * a stream whose own read buffer still holds bytes is answered READY without` |
|      - | 6604 | ` * asking the OS at all — which is the difference between a loop that drains a` |
|      - | 6605 | ` * buffered handle and one that waits forever for data it has already read.` |
|      - | 6606 | ` */` |
|      - | 6607 | `#if !defined(__WINNT__) \|\| defined(PH7_ENABLE_NET)` |
|      - | 6608 | `#define STREAM_SELECT_OK 1` |
|      - | 6609 | `#ifdef __UNIXES__` |
|      - | 6610 | `#include <sys/select.h>` |
|      - | 6611 | `#include <sys/time.h>` |
|      - | 6612 | `#endif` |
|      - | 6613 | `#endif` |
|      - | 6614 | `#define SEL_READ   0` |
|      - | 6615 | `#define SEL_WRITE  1` |
|      - | 6616 | `#define SEL_EXCEPT 2` |
|      - | 6617 | `/* What one walk over an argument is for. The order matters: php COUNTS the` |
|      - | 6618 | ` * already-buffered readable handles before it waits, and only rewrites the` |
|      - | 6619 | ` * arrays once it knows which answer it is giving. */` |
|      - | 6620 | `#define SELM_COLLECT  0 /* put every representable handle in its fd_set */` |
|      - | 6621 | `#define SELM_BUFFERED 1 /* keep the handles whose own buffer still holds bytes */` |
|      - | 6622 | `#define SELM_READY    2 /* keep the handles select() reported */` |
|      - | 6623 | `#define SELM_CLEAR    3 /* keep nothing: php empties the sets it is not answering */` |
|      - | 6624 | `typedef struct stream_select_ctx stream_select_ctx;` |
|      - | 6625 | `struct stream_select_ctx` |
|      - | 6626 | `{` |
|      - | 6627 | `	ph7_context *pCtx;` |
|      - | 6628 | `#ifdef STREAM_SELECT_OK` |
|      - | 6629 | `	fd_set aSet[3];    /* read / write / except, as select() takes them */` |
|      - | 6630 | `#endif` |
|      - | 6631 | `	int iMaxFd;` |
|      - | 6632 | `	int nSelectable;   /* entries that could be represented at all */` |
|      - | 6633 | `	int iWhich;        /* the set being walked (SEL_*) */` |
|      - | 6634 | `	int iMode;         /* SELM_*: what this walk is FOR */` |
|      - | 6635 | `	int nReady;` |
|      - | 6636 | `	int bBadEntry;     /* an entry that is not a stream at all */` |
|      - | 6637 | `	int bBadClosed;    /* ... and whether it was a CLOSED one (php words the two apart) */` |
|      - | 6638 | `	ph7_value *pOut;   /* the rebuilt array, while harvesting */` |
|      - | 6639 | `};` |
|      - | 6640 | `/*` |
|      - | 6641 | ` * What a select can WAIT on for this handle: the POSIX descriptor, or the` |
|      - | 6642 | ` * SOCKET, which is the only waitable thing a stream carries on Windows (the` |
|      - | 6643 | ` * file devices hold a HANDLE there, and select() cannot take one — a recorded` |
|      - | 6644 | ` * platform difference, §7.4). Answers -1 for a device with neither: a memory` |
|      - | 6645 | ` * buffer, a data:// payload, a userland wrapper.` |
|      - | 6646 | ` */` |
|     86 | 6647 | `static ph7_int64 IoPrivateSelectHandle(io_private *pDev)` |
|      1 | 6648 | `{` |
|      - | 6649 | `	int fd;` |
|      - | 6650 | `#ifdef PH7_ENABLE_NET` |
|     87 | 6651 | `	ph7_socket *pSock = IoPrivateSocket(pDev);` |
|     87 | 6652 | `	if( pSock ){` |
|     61 | 6653 | `		return *pSock == PH7_NET_INVALID_SOCKET ? -1 : (ph7_int64)*pSock;` |
|      - | 6654 | `	}` |
|      - | 6655 | `#endif` |
|     27 | 6656 | `	fd = PH7_StreamPosixFd(pDev);` |
|     27 | 6657 | `	return fd < 0 ? -1 : (ph7_int64)fd;` |
|     44 | 6658 | `}` |
|      - | 6659 | `/* Bytes this handle has already pulled off the device and not yet handed over. */` |
|     40 | 6660 | `static sxu32 IoPrivateUnread(io_private *pDev)` |
|      1 | 6661 | `{` |
|     41 | 6662 | `	return StreamAheadBytes(pDev);` |
|      1 | 6663 | `}` |
|     48 | 6664 | `static void StreamSelectAdd(stream_select_ctx *pSel,ph7_int64 h)` |
|      1 | 6665 | `{` |
|      - | 6666 | `#ifdef STREAM_SELECT_OK` |
|      - | 6667 | `#ifdef __WINNT__` |
|      - | 6668 | `	/* A Windows fd_set is an ARRAY of sockets, so what bounds it is how many` |
|      - | 6669 | `	 * are in it already rather than the value of this one. */` |
|      1 | 6670 | `	if( pSel->aSet[pSel->iWhich].fd_count >= FD_SETSIZE ){` |
|    ! 0 | 6671 | `		return;` |
|      - | 6672 | `	}` |
|      1 | 6673 | `	FD_SET((SOCKET)h,&pSel->aSet[pSel->iWhich]);` |
|      - | 6674 | `#else` |
|     48 | 6675 | `	if( h < 0 \|\| h >= (ph7_int64)FD_SETSIZE ){` |
|      - | 6676 | `		/* php ignores a descriptor an fd_set cannot hold (its own` |
|      - | 6677 | `		 * PHP_SAFE_FD_SET); writing past one corrupts the stack. */` |
|    ! 0 | 6678 | `		return;` |
|      - | 6679 | `	}` |
|     48 | 6680 | `	FD_SET((int)h,&pSel->aSet[pSel->iWhich]);` |
|      - | 6681 | `#endif` |
|     49 | 6682 | `	if( h > (ph7_int64)pSel->iMaxFd ){` |
|     39 | 6683 | `		pSel->iMaxFd = (int)h;` |
|     19 | 6684 | `	}` |
|      - | 6685 | `#else` |
|      - | 6686 | `	SXUNUSED(pSel);` |
|      - | 6687 | `	SXUNUSED(h);` |
|      - | 6688 | `#endif` |
|     25 | 6689 | `}` |
|     30 | 6690 | `static int StreamSelectIsSet(stream_select_ctx *pSel,ph7_int64 h)` |
|      1 | 6691 | `{` |
|      - | 6692 | `#ifdef STREAM_SELECT_OK` |
|      - | 6693 | `#ifdef __WINNT__` |
|      1 | 6694 | `	return FD_ISSET((SOCKET)h,&pSel->aSet[pSel->iWhich]) ? 1 : 0;` |
|      - | 6695 | `#else` |
|     30 | 6696 | `	if( h < 0 \|\| h >= (ph7_int64)FD_SETSIZE ){` |
|    ! 0 | 6697 | `		return 0;` |
|      - | 6698 | `	}` |
|     30 | 6699 | `	return FD_ISSET((int)h,&pSel->aSet[pSel->iWhich]) ? 1 : 0;` |
|      - | 6700 | `#endif` |
|      - | 6701 | `#else` |
|      - | 6702 | `	SXUNUSED(pSel);` |
|      - | 6703 | `	SXUNUSED(h);` |
|      - | 6704 | `	return 0;` |
|      - | 6705 | `#endif` |
|     16 | 6706 | `}` |
|      - | 6707 | `/*` |
|      - | 6708 | ` * One entry of one array: collected on the way in, harvested on the way out.` |
|      - | 6709 | ` * php never stops for an entry it cannot use — the diagnostics are remembered` |
|      - | 6710 | ` * and raised once the whole set is known, because whether the array held` |
|      - | 6711 | ` * ANYTHING selectable decides which of them php raises.` |
|      - | 6712 | ` */` |
|    134 | 6713 | `static int StreamSelectWalk(ph7_value *pKey,ph7_value *pValue,void *pUserData)` |
|      1 | 6714 | `{` |
|    135 | 6715 | `	stream_select_ctx *pSel = (stream_select_ctx *)pUserData;` |
|      - | 6716 | `	io_private *pDev;` |
|      - | 6717 | `	ph7_int64 h;` |
|    135 | 6718 | `	if( !ph7_value_is_resource(pValue) ){` |
|      7 | 6719 | `		pSel->bBadEntry = 1;` |
|      - | 6720 | `		/* php words a value that is not a resource apart from a resource that is` |
|      - | 6721 | `		 * no longer open, and raises one per bad entry — so the LAST one seen is` |
|      - | 6722 | `		 * the message that reaches the caller. */` |
|      7 | 6723 | `		pSel->bBadClosed = 0;` |
|      7 | 6724 | `		return PH7_OK;` |
|      - | 6725 | `	}` |
|    129 | 6726 | `	pDev = (io_private *)ph7_value_to_resource(pValue);` |
|    129 | 6727 | `	if( IO_PRIVATE_INVALID(pDev) ){` |
|      3 | 6728 | `		pSel->bBadEntry = pSel->bBadClosed = 1;` |
|      3 | 6729 | `		return PH7_OK;` |
|      - | 6730 | `	}` |
|    127 | 6731 | `	if( pSel->iMode == SELM_BUFFERED ){` |
|      - | 6732 | `		/* Deliberately BEFORE the descriptor lookup: php's shortcut lets a` |
|      - | 6733 | `		 * readable stream with no descriptor at all take part (a userland` |
|      - | 6734 | `		 * wrapper a line read has filled the buffer of), and answering 0 for one` |
|      - | 6735 | `		 * would sleep out the whole timeout over bytes the script already has. */` |
|     41 | 6736 | `		if( IoPrivateUnread(pDev) > 0 ){` |
|      9 | 6737 | `			if( pSel->pOut ){` |
|      5 | 6738 | `				ph7_array_add_elem(pSel->pOut,pKey,pValue);` |
|      2 | 6739 | `			}` |
|      9 | 6740 | `			pSel->nReady++;` |
|      4 | 6741 | `		}` |
|     41 | 6742 | `		return PH7_OK;` |
|      - | 6743 | `	}` |
|     87 | 6744 | `	h = IoPrivateSelectHandle(pDev);` |
|     87 | 6745 | `	if( h < 0 ){` |
|      7 | 6746 | `		if( pSel->iMode == SELM_COLLECT ){` |
|      - | 6747 | `			const char *zWrapper,*zLabel;` |
|      5 | 6748 | `			IoPrivateStreamLabels(pDev,&zWrapper,&zLabel);` |
|      7 | 6749 | `			ph7_context_throw_error_format(pSel->pCtx,PH7_CTX_WARNING,` |
|      2 | 6750 | `				"Cannot represent a stream of type %s as a select()able descriptor",zLabel);` |
|      2 | 6751 | `		}` |
|      7 | 6752 | `		return PH7_OK;` |
|      - | 6753 | `	}` |
|     81 | 6754 | `	if( pSel->iMode == SELM_COLLECT ){` |
|     49 | 6755 | `		pSel->nSelectable++;` |
|     49 | 6756 | `		StreamSelectAdd(pSel,h);` |
|     49 | 6757 | `		return PH7_OK;` |
|      - | 6758 | `	}` |
|     33 | 6759 | `	if( pSel->iMode == SELM_READY && StreamSelectIsSet(pSel,h) ){` |
|     21 | 6760 | `		if( pSel->pOut ){` |
|     21 | 6761 | `			ph7_array_add_elem(pSel->pOut,pKey,pValue);` |
|     10 | 6762 | `		}` |
|     21 | 6763 | `		pSel->nReady++;` |
|     10 | 6764 | `	}` |
|     33 | 6765 | `	return PH7_OK;` |
|     68 | 6766 | `}` |
|      - | 6767 | `/* The wait itself, over the pair the caller's numbers were normalised into. */` |
|     22 | 6768 | `static int StreamSelectWait(stream_select_ctx *pSel,ph7_int64 iSec,ph7_int64 iUsec,int bBlock,` |
|      - | 6769 | `	int *pErrno)` |
|      1 | 6770 | `{` |
|      - | 6771 | `#ifdef STREAM_SELECT_OK` |
|     23 | 6772 | `	struct timeval tv,*pTv = 0;` |
|      - | 6773 | `	int rc;` |
|     23 | 6774 | `	if( !bBlock ){` |
|     23 | 6775 | `		tv.tv_sec = (long)iSec;` |
|     23 | 6776 | `		tv.tv_usec = (long)iUsec;` |
|     23 | 6777 | `		pTv = &tv;` |
|     11 | 6778 | `	}` |
|     34 | 6779 | `	rc = select(pSel->iMaxFd + 1,&pSel->aSet[SEL_READ],&pSel->aSet[SEL_WRITE],` |
|     11 | 6780 | `		&pSel->aSet[SEL_EXCEPT],pTv);` |
|     23 | 6781 | `	if( rc < 0 && pErrno ){` |
|      - | 6782 | `#ifdef __WINNT__` |
|    ! 0 | 6783 | `		*pErrno = WSAGetLastError();` |
|      - | 6784 | `#else` |
|    ! 0 | 6785 | `		*pErrno = errno;` |
|      - | 6786 | `#endif` |
|    ! 0 | 6787 | `	}` |
|     23 | 6788 | `	return rc;` |
|      - | 6789 | `#else` |
|      - | 6790 | `	/* No select() to call: a Windows build with no socket layer. */` |
|      - | 6791 | `	SXUNUSED(pSel);` |
|      - | 6792 | `	SXUNUSED(iSec);` |
|      - | 6793 | `	SXUNUSED(iUsec);` |
|      - | 6794 | `	SXUNUSED(bBlock);` |
|      - | 6795 | `	if( pErrno ){ *pErrno = 0; }` |
|      - | 6796 | `	return -1;` |
|      - | 6797 | `#endif` |
|      1 | 6798 | `}` |
|      - | 6799 | `/* Walk one of the three arguments, if it IS one. */` |
|    190 | 6800 | `static void StreamSelectEach(stream_select_ctx *pSel,ph7_value **apArg,int nArg,int iArg,int iWhich,` |
|      - | 6801 | `	int iMode)` |
|      1 | 6802 | `{` |
|    191 | 6803 | `	if( iArg >= nArg \|\| apArg[iArg] == 0 \|\| !ph7_value_is_array(apArg[iArg]) ){` |
|     85 | 6804 | `		return;` |
|      - | 6805 | `	}` |
|    107 | 6806 | `	pSel->iWhich = iWhich;` |
|    107 | 6807 | `	pSel->iMode = iMode;` |
|    107 | 6808 | `	ph7_array_walk(apArg[iArg],StreamSelectWalk,pSel);` |
|     96 | 6809 | `}` |
|      - | 6810 | `/* Rebuild one argument from the entries that came back ready. php REPLACES the` |
|      - | 6811 | ` * array either way, so a set with nothing ready comes back empty. */` |
|     78 | 6812 | `static int StreamSelectStore(stream_select_ctx *pSel,ph7_value **apArg,int nArg,int iArg,int iWhich,` |
|      - | 6813 | `	int iMode)` |
|      1 | 6814 | `{` |
|     79 | 6815 | `	if( iArg >= nArg \|\| apArg[iArg] == 0 \|\| !ph7_value_is_array(apArg[iArg]) ){` |
|     47 | 6816 | `		return PH7_OK;` |
|      - | 6817 | `	}` |
|     33 | 6818 | `	pSel->pOut = ph7_context_new_array(pSel->pCtx);` |
|     33 | 6819 | `	if( pSel->pOut == 0 ){` |
|      - | 6820 | `		/* Leaving the caller's array alone would answer that every entry is` |
|      - | 6821 | `		 * ready, which is the one wrong answer this function must not give. */` |
|    ! 0 | 6822 | `		return PH7_ContextMemoryError(pSel->pCtx);` |
|      - | 6823 | `	}` |
|     33 | 6824 | `	StreamSelectEach(pSel,apArg,nArg,iArg,iWhich,iMode);` |
|     33 | 6825 | `	PH7_VmStoreArgByRef(pSel->pCtx->pVm,apArg[iArg],pSel->pOut);` |
|     33 | 6826 | `	pSel->pOut = 0;` |
|     33 | 6827 | `	return PH7_OK;` |
|     40 | 6828 | `}` |
|     44 | 6829 | `PH7_PRIVATE int PH7_builtin_stream_select(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 6830 | `{` |
|      - | 6831 | `	stream_select_ctx sSel;` |
|     45 | 6832 | `	ph7_int64 iSec = 0,iUsec = 0;` |
|     45 | 6833 | `	int bBlock = 1,iErrno = 0,rc,i;` |
|     45 | 6834 | `	SyZero(&sSel,sizeof(sSel));` |
|     45 | 6835 | `	sSel.pCtx = pCtx;` |
|     45 | 6836 | `	sSel.iMaxFd = -1;` |
|      - | 6837 | `#ifdef STREAM_SELECT_OK` |
|    177 | 6838 | `	for( i = 0 ; i < 3 ; i++ ){` |
|   1189 | 6839 | `		FD_ZERO(&sSel.aSet[i]);` |
|     67 | 6840 | `	}` |
|      - | 6841 | `#endif` |
|     45 | 6842 | `	StreamSelectEach(&sSel,apArg,nArg,0,SEL_READ,SELM_COLLECT);` |
|     45 | 6843 | `	StreamSelectEach(&sSel,apArg,nArg,1,SEL_WRITE,SELM_COLLECT);` |
|     45 | 6844 | `	StreamSelectEach(&sSel,apArg,nArg,2,SEL_EXCEPT,SELM_COLLECT);` |
|     45 | 6845 | `	if( sSel.nSelectable < 1 ){` |
|      - | 6846 | `		/* php's own wording, and it carries no function name. It is the answer` |
|      - | 6847 | `		 * for three NULLs, for empty arrays, and for arrays holding nothing` |
|      - | 6848 | `		 * this engine can wait on — the caller asked to wait for nothing. */` |
|      7 | 6849 | `		return PH7_VmThrowException(pCtx,"ValueError","No stream arrays were passed");` |
|      - | 6850 | `	}` |
|     39 | 6851 | `	if( sSel.bBadEntry ){` |
|      - | 6852 | `		/* Raised only once the arrays are known to hold something to wait on —` |
|      - | 6853 | `		 * the empty-arrays Error wins over it — and BEFORE the timeout is` |
|      - | 6854 | `		 * looked at, which is the order php's own pending-exception check` |
|      - | 6855 | `		 * produces. */` |
|     10 | 6856 | `		return PH7_VmThrowException(pCtx,"TypeError",` |
|      - | 6857 | `			"%s(): supplied %s is not a valid stream resource",` |
|      6 | 6858 | `			ph7_function_name(pCtx),sSel.bBadClosed ? "resource" : "argument");` |
|      - | 6859 | `	}` |
|     33 | 6860 | `	if( nArg > 3 && !ph7_value_is_null(apArg[3]) ){` |
|     31 | 6861 | `		iSec = ph7_value_to_int64(apArg[3]);` |
|     31 | 6862 | `		bBlock = 0;` |
|     31 | 6863 | `		if( iSec < 0 ){` |
|      4 | 6864 | `			return PH7_VmThrowException(pCtx,"ValueError",` |
|      - | 6865 | `				"%s(): Argument #4 ($seconds) must be greater than or equal to 0",` |
|      1 | 6866 | `				ph7_function_name(pCtx));` |
|      - | 6867 | `		}` |
|     14 | 6868 | `	}` |
|     31 | 6869 | `	if( nArg > 4 && !ph7_value_is_null(apArg[4]) ){` |
|     15 | 6870 | `		iUsec = ph7_value_to_int64(apArg[4]);` |
|     15 | 6871 | `		if( bBlock ){` |
|      - | 6872 | `			/* php refuses the pair rather than guessing which one meant it: a` |
|      - | 6873 | `			 * NULL $seconds is "wait forever", and there is no such thing as` |
|      - | 6874 | `			 * waiting forever for five microseconds. */` |
|      3 | 6875 | `			if( iUsec != 0 ){` |
|      4 | 6876 | `				return PH7_VmThrowException(pCtx,"ValueError",` |
|      - | 6877 | `					"%s(): Argument #5 ($microseconds) must be null when argument #4 ($seconds) is null",` |
|      1 | 6878 | `					ph7_function_name(pCtx));` |
|    ! 0 | 6879 | `			}` |
|     13 | 6880 | `		}else if( iUsec < 0 ){` |
|      4 | 6881 | `			return PH7_VmThrowException(pCtx,"ValueError",` |
|      - | 6882 | `				"%s(): Argument #5 ($microseconds) must be greater than or equal to 0",` |
|      1 | 6883 | `				ph7_function_name(pCtx));` |
|      - | 6884 | `		}` |
|      5 | 6885 | `	}` |
|     27 | 6886 | `	if( iUsec > 999999 ){` |
|      - | 6887 | `		/* php carries the overflow into the seconds, because a tv_usec of a` |
|      - | 6888 | `		 * million or more is what Solaris and the BSDs refuse outright — so` |
|      - | 6889 | ``		 * `stream_select($r, $w, $x, 0, 1500000)` waits a second and a half`` |
|      - | 6890 | `		 * rather than failing. */` |
|      3 | 6891 | `		iSec += iUsec / 1000000;` |
|      3 | 6892 | `		iUsec %= 1000000;` |
|      1 | 6893 | `	}` |
|      - | 6894 | `	/* php's own shortcut, and it comes BEFORE the wait: a handle whose buffer` |
|      - | 6895 | `	 * still holds bytes the script has not taken is ready NOW, whatever the OS` |
|      - | 6896 | `	 * would say about its descriptor — the device has nothing left to report.` |
|      - | 6897 | `	 * COUNTED first and stored second, because the count is what decides` |
|      - | 6898 | `	 * whether the arrays are rewritten from the buffers or from the wait. */` |
|     27 | 6899 | `	StreamSelectEach(&sSel,apArg,nArg,0,SEL_READ,SELM_BUFFERED);` |
|     27 | 6900 | `	if( sSel.nReady > 0 ){` |
|      5 | 6901 | `		sSel.nReady = 0;` |
|      4 | 6902 | `		if( StreamSelectStore(&sSel,apArg,nArg,0,SEL_READ,SELM_BUFFERED) != PH7_OK` |
|      - | 6903 | `		/* php answers only the readable ones then, and empties the other two. */` |
|      4 | 6904 | `		 \|\| StreamSelectStore(&sSel,apArg,nArg,1,SEL_WRITE,SELM_CLEAR) != PH7_OK` |
|      5 | 6905 | `		 \|\| StreamSelectStore(&sSel,apArg,nArg,2,SEL_EXCEPT,SELM_CLEAR) != PH7_OK ){` |
|    ! 0 | 6906 | `			return PH7_ContextMemoryError(pCtx);` |
|      - | 6907 | `		}` |
|      5 | 6908 | `		ph7_result_int(pCtx,sSel.nReady);` |
|      5 | 6909 | `		return PH7_OK;` |
|      - | 6910 | `	}` |
|     23 | 6911 | `	rc = StreamSelectWait(&sSel,iSec,iUsec,bBlock,&iErrno);` |
|     23 | 6912 | `	if( rc < 0 ){` |
|      - | 6913 | `#if defined(__WINNT__) && defined(PH7_ENABLE_NET)` |
|    ! 0 | 6914 | `		const char *zErr = PH7_NetStrError(iErrno);` |
|      - | 6915 | `#else` |
|    ! 0 | 6916 | `		const char *zErr = VfsStrerror(iErrno);` |
|      - | 6917 | `#endif` |
|    ! 0 | 6918 | `		ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,"Unable to select [%d]: %s (max_fd=%d)",` |
|    ! 0 | 6919 | `			iErrno,zErr,sSel.iMaxFd);` |
|    ! 0 | 6920 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 6921 | `		return PH7_OK;` |
|      - | 6922 | `	}` |
|     22 | 6923 | `	if( StreamSelectStore(&sSel,apArg,nArg,0,SEL_READ,SELM_READY) != PH7_OK` |
|     22 | 6924 | `	 \|\| StreamSelectStore(&sSel,apArg,nArg,1,SEL_WRITE,SELM_READY) != PH7_OK` |
|     23 | 6925 | `	 \|\| StreamSelectStore(&sSel,apArg,nArg,2,SEL_EXCEPT,SELM_READY) != PH7_OK ){` |
|    ! 0 | 6926 | `		return PH7_ContextMemoryError(pCtx);` |
|      - | 6927 | `	}` |
|      - | 6928 | `	/* The COUNT is select()'s own, not the entries kept: two array members can` |
|      - | 6929 | `	 * name one descriptor, and php answers what the OS said. */` |
|     23 | 6930 | `	ph7_result_int(pCtx,rc);` |
|     23 | 6931 | `	return PH7_OK;` |
|     23 | 6932 | `}` |
|      - | 6933 | `/*` |
|      - | 6934 | ` * array stream_get_transports(void)` |
|      - | 6935 | ` *` |
|      - | 6936 | ` * The transports a stream_socket_client()/fsockopen() address may name. php's` |
|      - | 6937 | ` * own list is what its build registered, so this is what THIS engine can open:` |
|      - | 6938 | ` * the ssl/tls/udp/unix set is a recorded scope gap (§7.4), and answering for` |
|      - | 6939 | ` * transports that are not there would tell a script a connection will work` |
|      - | 6940 | ` * when it cannot.` |
|      - | 6941 | ` */` |
|      8 | 6942 | `PH7_PRIVATE int PH7_builtin_stream_get_transports(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      2 | 6943 | `{` |
|      - | 6944 | `	ph7_value *pArr,*pV;` |
|      4 | 6945 | `	SXUNUSED(nArg);` |
|      4 | 6946 | `	SXUNUSED(apArg);` |
|     10 | 6947 | `	pArr = ph7_context_new_array(pCtx);` |
|     10 | 6948 | `	pV = ph7_context_new_scalar(pCtx);` |
|     10 | 6949 | `	if( pArr == 0 \|\| pV == 0 ){` |
|    ! 0 | 6950 | `		ph7_result_null(pCtx);` |
|    ! 0 | 6951 | `		return PH7_OK;` |
|      - | 6952 | `	}` |
|      - | 6953 | `#ifdef PH7_ENABLE_NET` |
|     10 | 6954 | `	ph7_value_string(pV,"tcp",-1);` |
|     10 | 6955 | `	ph7_array_add_elem(pArr,0,pV);` |
|      - | 6956 | `#endif` |
|     10 | 6957 | `	ph7_result_value(pCtx,pArr);` |
|     10 | 6958 | `	return PH7_OK;` |
|      6 | 6959 | `}` |
|      - | 6960 | `/*` |
|      - | 6961 | ` * bool stream_supports_lock(resource $stream)` |
|      - | 6962 | ` *` |
|      - | 6963 | ` * The question flock() answers with a warning if you get it wrong: only a` |
|      - | 6964 | ` * device with a real lock operation can be locked, so a memory buffer and a` |
|      - | 6965 | ` * data:// payload are false.` |
|      - | 6966 | ` */` |
|     14 | 6967 | `PH7_PRIVATE int PH7_builtin_stream_supports_lock(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 6968 | `{` |
|      - | 6969 | `	io_private *pDev;` |
|      - | 6970 | `	int rc;` |
|     15 | 6971 | `	if( nArg < 1 ){` |
|    ! 0 | 6972 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 6973 | `		return PH7_OK;` |
|      - | 6974 | `	}` |
|     15 | 6975 | `	pDev = StreamSettingArg(pCtx,apArg[0],&rc);` |
|     15 | 6976 | `	if( pDev == 0 ){` |
|      3 | 6977 | `		return rc;` |
|      - | 6978 | `	}` |
|      - | 6979 | `	/* php locks at the DESCRIPTOR, so anything with one can be locked even` |
|      - | 6980 | `	 * when the device exposes no lock operation of its own (php://stdout, a` |
|      - | 6981 | `	 * pipe); a memory buffer and a data:// payload have neither and are the` |
|      - | 6982 | `	 * false answers. */` |
|     13 | 6983 | `	pDev = PH7_StreamUnwrap(pDev);` |
|     23 | 6984 | `	ph7_result_bool(pCtx,pDev->bDir == 0` |
|     16 | 6985 | `		&& ((pDev->pStream != 0 && pDev->pStream->xLock != 0)` |
|      7 | 6986 | `		    \|\| PH7_StreamPosixFd(pDev) >= 0));` |
|     13 | 6987 | `	return PH7_OK;` |
|      8 | 6988 | `}` |
|      - | 6989 | `/*` |
|      - | 6990 | ` * bool stream_is_local(resource\|string $stream)` |
|      - | 6991 | ` *` |
|      - | 6992 | ` * php answers from the WRAPPER, not from the path: a stream opened by a URL` |
|      - | 6993 | ` * wrapper is not local, one opened by no wrapper at all (a pipe) is not local` |
|      - | 6994 | ` * either, and everything else — including php:// and a path naming a scheme` |
|      - | 6995 | ` * nobody registered — is.` |
|      - | 6996 | ` */` |
|     28 | 6997 | `PH7_PRIVATE int PH7_builtin_stream_is_local(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 6998 | `{` |
|      - | 6999 | `	const ph7_io_stream *pStream;` |
|     29 | 7000 | `	if( nArg < 1 ){` |
|    ! 0 | 7001 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 7002 | `		return PH7_OK;` |
|      - | 7003 | `	}` |
|     29 | 7004 | `	if( ph7_value_is_string(apArg[0]) ){` |
|      - | 7005 | `		static const char * const azUrlScheme[] = { "http://", "https://", "ftp://", "ftps://" };` |
|      - | 7006 | `		int nLen,i;` |
|     21 | 7007 | `		const char *zPath = ph7_value_to_string(apArg[0],&nLen);` |
|     20 | 7008 | `		if( nLen > (int)sizeof("file://")-1` |
|     19 | 7009 | `		 && SyStrnicmp(zPath,"file://",sizeof("file://")-1) == 0` |
|     11 | 7010 | `		 && zPath[sizeof("file://")-1] != '/'` |
|      4 | 7011 | `		 && SyStrnicmp(zPath,"file://localhost/",sizeof("file://localhost/")-1) != 0 ){` |
|      - | 7012 | ``			/* `file://host/path` names a REMOTE host, which php refuses rather`` |
|      - | 7013 | `			 * than reading as a local path — so the answer is not local. */` |
|      3 | 7014 | `			ph7_result_bool(pCtx,0);` |
|      3 | 7015 | `			return PH7_OK;` |
|      - | 7016 | `		}` |
|     83 | 7017 | `		for( i = 0 ; i < (int)(sizeof(azUrlScheme)/sizeof(azUrlScheme[0])) ; i++ ){` |
|     67 | 7018 | `			int nScheme = (int)SyStrlen(azUrlScheme[i]);` |
|     67 | 7019 | `			if( nLen >= nScheme && SyStrnicmp(zPath,azUrlScheme[i],(sxu32)nScheme) == 0 ){` |
|      - | 7020 | `				/* php registers these as URL wrappers whether or not this` |
|      - | 7021 | `				 * engine can OPEN them (http:// is a recorded gap, §7.4), and` |
|      - | 7022 | `				 * "is this path local?" has to answer for the scheme rather` |
|      - | 7023 | `				 * than for what happens to be implemented — the unsafe` |
|      - | 7024 | `				 * direction is answering TRUE about a remote URL. */` |
|      3 | 7025 | `				ph7_result_bool(pCtx,0);` |
|      3 | 7026 | `				return PH7_OK;` |
|      - | 7027 | `			}` |
|     33 | 7028 | `		}` |
|     17 | 7029 | `		pStream = PH7_VmGetStreamDevice(pCtx->pVm,&zPath,nLen);` |
|      - | 7030 | `		/* An unregistered scheme has no wrapper to ask, and php answers TRUE` |
|      - | 7031 | `		 * for it — the path is taken at face value. */` |
|     17 | 7032 | `		ph7_result_bool(pCtx,pStream == 0 \|\| !PH7_StreamIsUrlWrapper(pStream));` |
|     17 | 7033 | `		return PH7_OK;` |
|      - | 7034 | `	}` |
|      - | 7035 | `	{` |
|      - | 7036 | `		int rc;` |
|      9 | 7037 | `		io_private *pDev = StreamSettingArg(pCtx,apArg[0],&rc);` |
|      - | 7038 | `		const char *zWrapper,*zLabel;` |
|      9 | 7039 | `		if( pDev == 0 ){` |
|    ! 0 | 7040 | `			return rc;` |
|      - | 7041 | `		}` |
|      9 | 7042 | `		IoPrivateStreamLabels(pDev,&zWrapper,&zLabel);` |
|      - | 7043 | `		/* No wrapper (a popen() pipe, a socket) is php's other "not local". */` |
|      9 | 7044 | `		ph7_result_bool(pCtx,zWrapper != 0 && !PH7_StreamIsUrlWrapper(pDev->pStream));` |
|      - | 7045 | `	}` |
|      9 | 7046 | `	return PH7_OK;` |
|     15 | 7047 | `}` |
|      - | 7048 | `/* PH7_ENABLE_NET */` |
|    920 | 7049 | `PH7_PRIVATE int PH7_builtin_fopen(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      5 | 7050 | `{` |
|      - | 7051 | `	const ph7_io_stream *pStream;` |
|      - | 7052 | `	const char *zUri,*zMode;` |
|      - | 7053 | `	ph7_value *pResource;` |
|      - | 7054 | `	io_private *pDev;` |
|      - | 7055 | `	phl_stream_ctx *pCtxRes;` |
|    925 | 7056 | `	int iLen,imLen,bThrew = 0;` |
|      - | 7057 | `	int iOpenFlags;` |
|    925 | 7058 | `	if( nArg < 1 \|\| !ph7_value_is_string(apArg[0]) ){` |
|      - | 7059 | `		/* Missing/Invalid arguments,return FALSE */` |
|    ! 0 | 7060 | `		ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"Expecting a file path or URL");` |
|    ! 0 | 7061 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 7062 | `		return PH7_OK;` |
|      - | 7063 | `	}` |
|      - | 7064 | `	/* Extract the URI and the desired access mode */` |
|    925 | 7065 | `	zUri  = ph7_value_to_string(apArg[0],&iLen);` |
|    925 | 7066 | `	if( nArg > 1 ){` |
|    925 | 7067 | `		zMode = ph7_value_to_string(apArg[1],&imLen);` |
|    465 | 7068 | `	}else{` |
|      - | 7069 | `		/* Set a default read-only mode */` |
|    ! 0 | 7070 | `		zMode = "r";` |
|    ! 0 | 7071 | `		imLen = (int)sizeof(char);` |
|      - | 7072 | `	}` |
|      - | 7073 | ``	/* php's `?resource $context`: a resource that is not a stream-context is`` |
|      - | 7074 | `	 * refused, and NULL means the DEFAULT context — never "no context at all".` |
|      - | 7075 | `	 * Resolved before the io_private chunk below, which a throw could not` |
|      - | 7076 | `	 * release. */` |
|    925 | 7077 | `	pCtxRes = PH7_StreamCtxFromArg(pCtx,nArg,apArg,3,"$context",0,&bThrew);` |
|    925 | 7078 | `	if( bThrew ){` |
|      5 | 7079 | `		return PH7_OK;` |
|      - | 7080 | `	}` |
|      - | 7081 | `	/* Try to extract a stream */` |
|    921 | 7082 | `	pStream = PH7_VmGetStreamDevice(pCtx->pVm,&zUri,iLen);` |
|    921 | 7083 | `	if( pStream == 0 ){` |
|    ! 0 | 7084 | `		VfsThrowNoDeviceWarning(pCtx,zUri,FALSE);` |
|    ! 0 | 7085 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 7086 | `		return PH7_OK;` |
|      - | 7087 | `	}` |
|      - | 7088 | `	/* Allocate a new IO private instance */` |
|    921 | 7089 | `	pDev = (io_private *)ph7_context_alloc_chunk(pCtx,sizeof(io_private),TRUE,FALSE);` |
|    921 | 7090 | `	if( pDev == 0 ){` |
|    ! 0 | 7091 | `		ph7_context_throw_error(pCtx,PH7_CTX_ERR,"PH7 is running out of memory");` |
|    ! 0 | 7092 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 7093 | `		return PH7_OK;` |
|      - | 7094 | `	}` |
|    921 | 7095 | `	pResource = 0;` |
|    921 | 7096 | `	if( nArg > 3 ){` |
|      8 | 7097 | `		pResource = apArg[3];` |
|    918 | 7098 | `	}else if( is_php_stream(pStream) \|\| is_data_stream(pStream) ){` |
|      - | 7099 | `		/* TICKET 1433-80: The php:// and data:// streams need a ph7_value to` |
|      - | 7100 | `		 * access the underlying virtual machine.` |
|      - | 7101 | `		 */` |
|    419 | 7102 | `		pResource = apArg[0];` |
|    207 | 7103 | `	}` |
|      - | 7104 | `	/* Initialize the structure */` |
|    921 | 7105 | `	InitIOPrivate(pCtx->pVm,pStream,pDev);` |
|      - | 7106 | `	/* Convert open mode to PH7 flags */` |
|    921 | 7107 | `	iOpenFlags = StrModeToFlags(pCtx,zMode,imLen);` |
|    921 | 7108 | `	PH7_StreamCtxArm(pCtx->pVm,pCtxRes);` |
|      - | 7109 | `	/* Try to get a handle */` |
|   1383 | 7110 | `	pDev->pHandle = PH7_StreamOpenHandle(pCtx->pVm,pStream,zUri,iOpenFlags,` |
|    462 | 7111 | `		nArg > 2 ? ph7_value_to_bool(apArg[2]) : FALSE,pResource,FALSE,0,ph7_function_name(pCtx));` |
|    921 | 7112 | `	if( pDev->pHandle == 0 ){` |
|      8 | 7113 | `		VfsThrowOpenWarning(pCtx,zUri);` |
|      8 | 7114 | `		ph7_result_bool(pCtx,0);` |
|      8 | 7115 | `		ReleaseIOPrivate(pCtx,pDev);` |
|      8 | 7116 | `		return PH7_OK;` |
|      - | 7117 | `	}` |
|      - | 7118 | `	/* Remember what we were asked for: stream_get_meta_data() reports both.` |
|      - | 7119 | `	 * The URI is the ORIGINAL argument, not the scheme-stripped remainder` |
|      - | 7120 | `	 * PH7_VmGetStreamDevice() advanced zUri past. */` |
|      - | 7121 | `	{` |
|      - | 7122 | `		int nUri;` |
|    915 | 7123 | `		const char *zOrig = ph7_value_to_string(apArg[0],&nUri);` |
|    915 | 7124 | `		const char *zMeta = zMode;` |
|    915 | 7125 | `		int nMeta = imLen;` |
|    910 | 7126 | `		if( is_php_stream(pStream)` |
|    658 | 7127 | `		 && PH7_PhpStreamKind(pDev->pHandle) == PH7_IO_STREAM_OUTPUT ){` |
|      - | 7128 | `			/* php://output has one mode whatever it was asked for. */` |
|      5 | 7129 | `			zMeta = "wb";` |
|      5 | 7130 | `			nMeta = 2;` |
|    909 | 7131 | `		}else if( is_php_stream(pStream)` |
|    654 | 7132 | `		 && PH7_PhpStreamKind(pDev->pHandle) == PH7_IO_STREAM_MEMORY ){` |
|      - | 7133 | `			/* php's memory streams do not keep the mode they were opened with:` |
|      - | 7134 | `			 * a buffer is readable and writable either way, so php reports the` |
|      - | 7135 | `			 * one it actually built. */` |
|    569 | 7136 | `			int i,bWrite = 0,bAppend = imLen > 0 && (zMode[0] == 'a' \|\| zMode[0] == 'A');` |
|   1081 | 7137 | `			for( i = 0 ; i < imLen ; i++ ){` |
|    700 | 7138 | `				if( zMode[i] == 'w' \|\| zMode[i] == 'W' \|\| zMode[i] == 'a'` |
|    530 | 7139 | `				 \|\| zMode[i] == 'A' \|\| zMode[i] == '+' ){` |
|    503 | 7140 | `					bWrite = 1;` |
|    249 | 7141 | `				}` |
|    355 | 7142 | `			}` |
|    381 | 7143 | `			zMeta = bWrite ? (bAppend ? "a+b" : "w+b") : "rb";` |
|    381 | 7144 | `			nMeta = (int)SyStrlen(zMeta);` |
|    188 | 7145 | `		}` |
|    915 | 7146 | `		SetIOPrivateOpenedAs(pDev,zOrig,nUri,zMeta,nMeta);` |
|      - | 7147 | `	}` |
|      - | 7148 | `	/* All done,return the io_private instance as a resource */` |
|    915 | 7149 | `	ph7_result_resource(pCtx,pDev);` |
|    915 | 7150 | `	return PH7_OK;` |
|    465 | 7151 | `}` |
|      - | 7152 | `/*` |
|      - | 7153 | ` * bool fclose(resource $handle)` |
|      - | 7154 | ` *  Closes an open file pointer` |
|      - | 7155 | ` * Parameters` |
|      - | 7156 | ` *  $handle` |
|      - | 7157 | ` *   The file pointer.` |
|      - | 7158 | ` * Return` |
|      - | 7159 | ` *  TRUE on success or FALSE on failure.` |
|      - | 7160 | ` */` |
|   1066 | 7161 | `PH7_PRIVATE int PH7_builtin_fclose(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      5 | 7162 | `{` |
|      - | 7163 | `	const ph7_io_stream *pStream;` |
|      - | 7164 | `	io_private *pDev;` |
|      - | 7165 | `	ph7_vm *pVm;` |
|   1071 | 7166 | `	if( nArg < 1 \|\| !ph7_value_is_resource(apArg[0]) ){` |
|      - | 7167 | `		/* Missing/Invalid arguments,return FALSE */` |
|    ! 0 | 7168 | `		ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"Expecting an IO handle");` |
|    ! 0 | 7169 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 7170 | `		return PH7_OK;` |
|      - | 7171 | `	}` |
|      - | 7172 | `	/* Extract our private data */` |
|   1071 | 7173 | `	pDev = (io_private *)ph7_value_to_resource(apArg[0]);` |
|      - | 7174 | `	/* php: fclose() on an already-closed stream raises a catchable TypeError */` |
|   1071 | 7175 | `	if( pDev != 0 && pDev->iMagic == IO_PRIVATE_CLOSED_MAGIC ){` |
|      3 | 7176 | `		return PH7_VmThrowException(pCtx,"TypeError",` |
|      - | 7177 | `			"fclose(): Argument #1 ($stream) must be an open stream resource");` |
|      - | 7178 | `	}` |
|      - | 7179 | `	/* Make sure we are dealing with a valid io_private instance */` |
|   1069 | 7180 | `	if( IO_PRIVATE_INVALID(pDev) ){` |
|      - | 7181 | `		/*Expecting an IO handle */` |
|    ! 0 | 7182 | `		ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"Expecting an IO handle");` |
|    ! 0 | 7183 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 7184 | `		return PH7_OK;` |
|      - | 7185 | `	}` |
|      - | 7186 | `	/* Point to the target IO stream device */` |
|   1069 | 7187 | `	pStream = pDev->pStream;` |
|   1069 | 7188 | `	if( pStream == 0 ){` |
|    ! 0 | 7189 | `		ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,` |
|      - | 7190 | `			"IO routine(%s) not implemented in the underlying stream(%s) device,PH7 is returning FALSE",` |
|    ! 0 | 7191 | `			ph7_function_name(pCtx),pStream ? pStream->zName : "null_stream"` |
|      - | 7192 | `			);` |
|    ! 0 | 7193 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 7194 | `		return PH7_OK;` |
|      - | 7195 | `	}` |
|      - | 7196 | `	/* Point to the VM that own this context */` |
|   1069 | 7197 | `	pVm = pCtx->pVm;` |
|      - | 7198 | `	/* TICKET 1433-62: Keep the STDIN/STDOUT/STDERR handles open */` |
|   1069 | 7199 | `	if( pDev != pVm->pStdin && pDev != pVm->pStdout && pDev != pVm->pStderr ){` |
|      - | 7200 | `		/* The WRITE chain gets its closing call while the device is still open:` |
|      - | 7201 | `		 * a filter that buffers has nowhere else to put its tail, and php's own` |
|      - | 7202 | `		 * close flushes before it closes. */` |
|   1069 | 7203 | `		PH7_StreamFilterReleaseChains(pDev);` |
|      - | 7204 | `		/* Perform the requested operation */` |
|   1069 | 7205 | `		PH7_StreamCloseHandle(pStream,pDev->pHandle);` |
|      - | 7206 | `		/* Keep the handle alive but flag it closed so shared copies see it */` |
|   1069 | 7207 | `		MarkIOPrivateClosed(pDev);` |
|    532 | 7208 | `	}` |
|      - | 7209 | `	/* Return TRUE */` |
|   1069 | 7210 | `	ph7_result_bool(pCtx,1);` |
|   1069 | 7211 | `	return PH7_OK;` |
|    538 | 7212 | `}` |
|      - | 7213 | `#if !defined(PH7_DISABLE_HASH_FUNC)` |
|      - | 7214 | `/*` |
|      - | 7215 | ` * MD5/SHA1 digest consumer.` |
|      - | 7216 | ` */` |
|    136 | 7217 | `static int vfsHashConsumer(const void *pData,unsigned int nLen,void *pUserData)` |
|      2 | 7218 | `{` |
|      - | 7219 | `	/* Append hex chunk verbatim */` |
|    138 | 7220 | `	ph7_result_string((ph7_context *)pUserData,(const char *)pData,(int)nLen);` |
|    138 | 7221 | `	return SXRET_OK;` |
|      2 | 7222 | `}` |
|      - | 7223 | `/*` |
|      - | 7224 | ` * string md5_file(string $uri[,bool $raw_output = false ])` |
|      - | 7225 | ` *  Calculates the md5 hash of a given file.` |
|      - | 7226 | ` * Parameters` |
|      - | 7227 | ` *  $uri` |
|      - | 7228 | ` *   Target URI (file(/path/to/something) or URL(http://www.symisc.net/))` |
|      - | 7229 | ` *  $raw_output` |
|      - | 7230 | ` *   When TRUE, returns the digest in raw binary format with a length of 16.` |
|      - | 7231 | ` * Return` |
|      - | 7232 | ` *  Return the MD5 digest on success or FALSE on failure.` |
|      - | 7233 | ` */` |
|      8 | 7234 | `PH7_PRIVATE int PH7_builtin_md5_file(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      2 | 7235 | `{` |
|      - | 7236 | `	const ph7_io_stream *pStream;` |
|      - | 7237 | `	unsigned char zDigest[16];` |
|     10 | 7238 | `	int raw_output  = FALSE;` |
|      - | 7239 | `	const char *zFile;` |
|      - | 7240 | `	MD5Context sCtx;` |
|      - | 7241 | `	char zBuf[8192];` |
|      - | 7242 | `	void *pHandle;` |
|      - | 7243 | `	ph7_int64 n;` |
|      - | 7244 | `	int nLen;` |
|     10 | 7245 | `	if( nArg < 1 \|\| !ph7_value_is_string(apArg[0]) ){` |
|      - | 7246 | `		/* Missing/Invalid arguments,return FALSE */` |
|    ! 0 | 7247 | `		ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"Expecting a file path");` |
|    ! 0 | 7248 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 7249 | `		return PH7_OK;` |
|      - | 7250 | `	}` |
|      - | 7251 | `	/* Extract the file path */` |
|     10 | 7252 | `	zFile = ph7_value_to_string(apArg[0],&nLen);` |
|      - | 7253 | `	/* Point to the target IO stream device */` |
|     10 | 7254 | `	pStream = PH7_VmGetStreamDevice(pCtx->pVm,&zFile,nLen);` |
|     10 | 7255 | `	if( pStream == 0 ){` |
|    ! 0 | 7256 | `		VfsThrowNoDeviceWarning(pCtx,zFile,FALSE);` |
|    ! 0 | 7257 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 7258 | `		return PH7_OK;` |
|      - | 7259 | `	}` |
|     10 | 7260 | `	if( nArg > 1 ){` |
|    ! 0 | 7261 | `		raw_output = ph7_value_to_bool(apArg[1]);` |
|    ! 0 | 7262 | `	}` |
|      - | 7263 | `	/* Try to open the file in read-only mode */` |
|     10 | 7264 | `	pHandle = PH7_StreamOpenHandle(pCtx->pVm,pStream,zFile,PH7_IO_OPEN_RDONLY,FALSE,0,FALSE,0,ph7_function_name(pCtx));` |
|     10 | 7265 | `	if( pHandle == 0 ){` |
|      3 | 7266 | `		VfsThrowOpenWarning(pCtx,zFile);` |
|      3 | 7267 | `		ph7_result_bool(pCtx,0);` |
|      3 | 7268 | `		return PH7_OK;` |
|      - | 7269 | `	}` |
|      - | 7270 | `	/* Init the MD5 context */` |
|      8 | 7271 | `	MD5Init(&sCtx);` |
|      - | 7272 | `	/* Perform the requested operation */` |
|      4 | 7273 | `	for(;;){` |
|     10 | 7274 | `		n = pStream->xRead(pHandle,zBuf,sizeof(zBuf));` |
|     10 | 7275 | `		if( n < 1 ){` |
|      - | 7276 | `			/* EOF or IO error,break immediately */` |
|      8 | 7277 | `			break;` |
|      - | 7278 | `		}` |
|      3 | 7279 | `		MD5Update(&sCtx,(const unsigned char *)zBuf,(unsigned int)n);` |
|      1 | 7280 | `	}` |
|      - | 7281 | `	/* Close the stream */` |
|      8 | 7282 | `	PH7_StreamCloseHandle(pStream,pHandle);` |
|      - | 7283 | `	/* Extract the digest */` |
|      8 | 7284 | `	MD5Final(zDigest,&sCtx);` |
|      8 | 7285 | `	if( raw_output ){` |
|      - | 7286 | `		/* Output raw digest */` |
|    ! 0 | 7287 | `		ph7_result_string(pCtx,(const char *)zDigest,sizeof(zDigest));` |
|    ! 0 | 7288 | `	}else{` |
|      - | 7289 | `		/* Perform a binary to hex conversion */` |
|      8 | 7290 | `		SyBinToHexConsumer((const void *)zDigest,sizeof(zDigest),vfsHashConsumer,pCtx);` |
|      - | 7291 | `	}` |
|      8 | 7292 | `	return PH7_OK;` |
|      6 | 7293 | `}` |
|      - | 7294 | `/*` |
|      - | 7295 | ` * string sha1_file(string $uri[,bool $raw_output = false ])` |
|      - | 7296 | ` *  Calculates the SHA1 hash of a given file.` |
|      - | 7297 | ` * Parameters` |
|      - | 7298 | ` *  $uri` |
|      - | 7299 | ` *   Target URI (file(/path/to/something) or URL(http://www.symisc.net/))` |
|      - | 7300 | ` *  $raw_output` |
|      - | 7301 | ` *   When TRUE, returns the digest in raw binary format with a length of 20.` |
|      - | 7302 | ` * Return` |
|      - | 7303 | ` *  Return the SHA1 digest on success or FALSE on failure.` |
|      - | 7304 | ` */` |
|      2 | 7305 | `PH7_PRIVATE int PH7_builtin_sha1_file(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 7306 | `{` |
|      - | 7307 | `	const ph7_io_stream *pStream;` |
|      - | 7308 | `	unsigned char zDigest[20];` |
|      3 | 7309 | `	int raw_output  = FALSE;` |
|      - | 7310 | `	const char *zFile;` |
|      - | 7311 | `	SHA1Context sCtx;` |
|      - | 7312 | `	char zBuf[8192];` |
|      - | 7313 | `	void *pHandle;` |
|      - | 7314 | `	ph7_int64 n;` |
|      - | 7315 | `	int nLen;` |
|      3 | 7316 | `	if( nArg < 1 \|\| !ph7_value_is_string(apArg[0]) ){` |
|      - | 7317 | `		/* Missing/Invalid arguments,return FALSE */` |
|    ! 0 | 7318 | `		ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"Expecting a file path");` |
|    ! 0 | 7319 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 7320 | `		return PH7_OK;` |
|      - | 7321 | `	}` |
|      - | 7322 | `	/* Extract the file path */` |
|      3 | 7323 | `	zFile = ph7_value_to_string(apArg[0],&nLen);` |
|      - | 7324 | `	/* Point to the target IO stream device */` |
|      3 | 7325 | `	pStream = PH7_VmGetStreamDevice(pCtx->pVm,&zFile,nLen);` |
|      3 | 7326 | `	if( pStream == 0 ){` |
|    ! 0 | 7327 | `		VfsThrowNoDeviceWarning(pCtx,zFile,FALSE);` |
|    ! 0 | 7328 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 7329 | `		return PH7_OK;` |
|      - | 7330 | `	}` |
|      3 | 7331 | `	if( nArg > 1 ){` |
|    ! 0 | 7332 | `		raw_output = ph7_value_to_bool(apArg[1]);` |
|    ! 0 | 7333 | `	}` |
|      - | 7334 | `	/* Try to open the file in read-only mode */` |
|      3 | 7335 | `	pHandle = PH7_StreamOpenHandle(pCtx->pVm,pStream,zFile,PH7_IO_OPEN_RDONLY,FALSE,0,FALSE,0,ph7_function_name(pCtx));` |
|      3 | 7336 | `	if( pHandle == 0 ){` |
|    ! 0 | 7337 | `		VfsThrowOpenWarning(pCtx,zFile);` |
|    ! 0 | 7338 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 7339 | `		return PH7_OK;` |
|      - | 7340 | `	}` |
|      - | 7341 | `	/* Init the SHA1 context */` |
|      3 | 7342 | `	SHA1Init(&sCtx);` |
|      - | 7343 | `	/* Perform the requested operation */` |
|      2 | 7344 | `	for(;;){` |
|      5 | 7345 | `		n = pStream->xRead(pHandle,zBuf,sizeof(zBuf));` |
|      5 | 7346 | `		if( n < 1 ){` |
|      - | 7347 | `			/* EOF or IO error,break immediately */` |
|      3 | 7348 | `			break;` |
|      - | 7349 | `		}` |
|      3 | 7350 | `		SHA1Update(&sCtx,(const unsigned char *)zBuf,(unsigned int)n);` |
|      1 | 7351 | `	}` |
|      - | 7352 | `	/* Close the stream */` |
|      3 | 7353 | `	PH7_StreamCloseHandle(pStream,pHandle);` |
|      - | 7354 | `	/* Extract the digest */` |
|      3 | 7355 | `	SHA1Final(&sCtx,zDigest);` |
|      3 | 7356 | `	if( raw_output ){` |
|      - | 7357 | `		/* Output raw digest */` |
|    ! 0 | 7358 | `		ph7_result_string(pCtx,(const char *)zDigest,sizeof(zDigest));` |
|    ! 0 | 7359 | `	}else{` |
|      - | 7360 | `		/* Perform a binary to hex conversion */` |
|      3 | 7361 | `		SyBinToHexConsumer((const void *)zDigest,sizeof(zDigest),vfsHashConsumer,pCtx);` |
|      - | 7362 | `	}` |
|      3 | 7363 | `	return PH7_OK;` |
|      2 | 7364 | `}` |
|      - | 7365 | `#endif /* PH7_DISABLE_HASH_FUNC */` |
|      - | 7366 | `/*` |
|      - | 7367 | ` * array parse_ini_file(string $filename[, bool $process_sections = false [, int $scanner_mode = INI_SCANNER_NORMAL ]] )` |
|      - | 7368 | ` *  Parse a configuration file.` |
|      - | 7369 | ` * Parameters` |
|      - | 7370 | ` * $filename` |
|      - | 7371 | ` *  The filename of the ini file being parsed.` |
|      - | 7372 | ` * $process_sections` |
|      - | 7373 | ` *  By setting the process_sections parameter to TRUE, you get a multidimensional array` |
|      - | 7374 | ` *  with the section names and settings included.` |
|      - | 7375 | ` *  The default for process_sections is FALSE.` |
|      - | 7376 | ` * $scanner_mode` |
|      - | 7377 | ` *  Can either be INI_SCANNER_NORMAL (default) or INI_SCANNER_RAW.` |
|      - | 7378 | ` *  If INI_SCANNER_RAW is supplied, then option values will not be parsed.` |
|      - | 7379 | ` * Return` |
|      - | 7380 | ` *  The settings are returned as an associative array on success.` |
|      - | 7381 | ` *  Otherwise is returned.` |
|      - | 7382 | ` */` |
|      8 | 7383 | `PH7_PRIVATE int PH7_builtin_parse_ini_file(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 7384 | `{` |
|      - | 7385 | `	const ph7_io_stream *pStream;` |
|      - | 7386 | `	const char *zFile;` |
|      - | 7387 | `	SyBlob sContents;` |
|      - | 7388 | `	void *pHandle;` |
|      - | 7389 | `	int nLen;` |
|      9 | 7390 | `	int iMode = PH7_INI_SCANNER_NORMAL;` |
|      9 | 7391 | `	sxi32 rc = PH7_OK;` |
|      9 | 7392 | `	if( nArg < 1 \|\| !ph7_value_is_string(apArg[0]) ){` |
|      - | 7393 | `		/* Missing/Invalid arguments,return FALSE */` |
|    ! 0 | 7394 | `		ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"Expecting a file path");` |
|    ! 0 | 7395 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 7396 | `		return PH7_OK;` |
|      - | 7397 | `	}` |
|      9 | 7398 | `	if( nArg > 2 && ph7_value_is_int(apArg[2]) ){` |
|      7 | 7399 | `		iMode = ph7_value_to_int(apArg[2]);` |
|      6 | 7400 | `		if( iMode != PH7_INI_SCANNER_NORMAL && iMode != PH7_INI_SCANNER_RAW` |
|      6 | 7401 | `		 && iMode != PH7_INI_SCANNER_TYPED ){` |
|      - | 7402 | `			/* php screens the mode BEFORE touching the file */` |
|      - | 7403 | ``			/* php's bare message: no `func(): ` qualifier on this one */`` |
|      3 | 7404 | `			PH7_VmThrowError(pCtx->pVm,0,PH7_CTX_WARNING,"Invalid scanner mode");` |
|      3 | 7405 | `			ph7_result_bool(pCtx,0);` |
|      3 | 7406 | `			return PH7_OK;` |
|      - | 7407 | `		}` |
|      2 | 7408 | `	}` |
|      - | 7409 | `	/* Extract the file path */` |
|      7 | 7410 | `	zFile = ph7_value_to_string(apArg[0],&nLen);` |
|      - | 7411 | `	/* Point to the target IO stream device */` |
|      7 | 7412 | `	pStream = PH7_VmGetStreamDevice(pCtx->pVm,&zFile,nLen);` |
|      7 | 7413 | `	if( pStream == 0 ){` |
|    ! 0 | 7414 | `		VfsThrowNoDeviceWarning(pCtx,zFile,FALSE);` |
|    ! 0 | 7415 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 7416 | `		return PH7_OK;` |
|      - | 7417 | `	}` |
|      - | 7418 | `	/* Try to open the file in read-only mode */` |
|      7 | 7419 | `	pHandle = PH7_StreamOpenHandle(pCtx->pVm,pStream,zFile,PH7_IO_OPEN_RDONLY,FALSE,0,FALSE,0,ph7_function_name(pCtx));` |
|      7 | 7420 | `	if( pHandle == 0 ){` |
|    ! 0 | 7421 | `		VfsThrowOpenWarning(pCtx,zFile);` |
|    ! 0 | 7422 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 7423 | `		return PH7_OK;` |
|      - | 7424 | `	}` |
|      7 | 7425 | `	SyBlobInit(&sContents,&pCtx->pVm->sAllocator);` |
|      - | 7426 | `	/* Read the whole file */` |
|      7 | 7427 | `	PH7_StreamReadWholeFile(pHandle,pStream,&sContents);` |
|      7 | 7428 | `	if( SyBlobLength(&sContents) < 1 ){` |
|      - | 7429 | `		/* Empty buffer,return FALSE */` |
|    ! 0 | 7430 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 7431 | `	}else{` |
|      - | 7432 | `		/* Process the raw INI buffer; capture an OOM abort to propagate below */` |
|     13 | 7433 | `		rc = PH7_ParseIniString(pCtx,(const char *)SyBlobData(&sContents),SyBlobLength(&sContents),` |
|      6 | 7434 | `			nArg > 1 ? ph7_value_to_bool(apArg[1]) : 0,iMode);` |
|      - | 7435 | `	}` |
|      - | 7436 | `	/* Close the stream */` |
|      7 | 7437 | `	PH7_StreamCloseHandle(pStream,pHandle);` |
|      - | 7438 | `	/* Release the working buffer */` |
|      7 | 7439 | `	SyBlobRelease(&sContents);` |
|      - | 7440 | `	/* Propagate an OOM abort so the fatal actually halts the VM */` |
|      7 | 7441 | `	return rc;` |
|      5 | 7442 | `}` |
|      - | 7443 | `/* ZIP archive processing moved to vfs_zip.c */` |
|      - | 7444 | `#else /* PH7_DISABLE_DISK_IO */` |
|      - | 7445 | `/*` |
|      - | 7446 | ` * Disk I/O is compiled out: this VFS hands out no resource handles, so` |
|      - | 7447 | ` * get_resource_type() has nothing that could be a "stream" and every` |
|      - | 7448 | ` * resource reports as "Unknown" (the same fallback the full build gives` |
|      - | 7449 | ` * to any non-VFS resource).` |
|      - | 7450 | ` */` |
|      - | 7451 | `PH7_PRIVATE const char * PH7_VfsResourceType(void *pResource)` |
|      - | 7452 | `{` |
|      - | 7453 | `	SXUNUSED(pResource);` |
|      - | 7454 | `	return "Unknown";` |
|      - | 7455 | `}` |
|      - | 7456 | `/* No disk I/O means no closeable handles: nothing is ever a closed resource. */` |
|      - | 7457 | `PH7_PRIVATE int PH7_VfsResourceIsClosed(void *pResource)` |
|      - | 7458 | `{` |
|      - | 7459 | `	SXUNUSED(pResource);` |
|      - | 7460 | `	return 0;` |
|      - | 7461 | `}` |
|      - | 7462 | `/* No streams means no stream contexts either, but PH7_VmReset still calls this. */` |
|      - | 7463 | `PH7_PRIVATE void PH7_StreamCtxVmReset(ph7_vm *pVm)` |
|      - | 7464 | `{` |
|      - | 7465 | `	SXUNUSED(pVm);` |
|      - | 7466 | `}` |
|      - | 7467 | `/* Same for the filter registry. */` |
|      - | 7468 | `PH7_PRIVATE void PH7_StreamFilterVmReset(ph7_vm *pVm)` |
|      - | 7469 | `{` |
|      - | 7470 | `	SXUNUSED(pVm);` |
|      - | 7471 | `}` |
|      - | 7472 | `#endif /* PH7_DISABLE_DISK_IO */` |
|      - | 7473 |  |
