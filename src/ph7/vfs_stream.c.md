# src/ph7/vfs_stream.c

<style>code, pre { background: none !important; white-space: pre !important; width: 100% !important; display: inline-block !important; } td { border: none !important; margin-top: 0 !important; margin-bottom: 0 !important; padding-top: 0 !important; padding-bottom: 0 !important; }</style>

Coverage: 3259/4019 lines (81.09%)

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
|    435 |   40 | `static void StreamSeekBackForWrite(io_private *pDev)` |
|      5 |   41 | `{` |
|    440 |   42 | `	sxu32 nAhead = StreamAheadBytes(pDev);` |
|    440 |   43 | `	if( nAhead > 0 && pDev->pStream && pDev->pStream->xSeek ){` |
|     16 |   44 | `		pDev->pStream->xSeek(pDev->pHandle,-(ph7_int64)nAhead,1/*SEEK_CUR*/);` |
|     16 |   45 | `		ResetIOPrivate(pDev);` |
|      7 |   46 | `	}` |
|    440 |   47 | `}` |
|     84 |   48 | `PH7_PRIVATE ph7_int64 PH7_StreamLogicalTell(io_private *pDev)` |
|      3 |   49 | `{` |
|      - |   50 | `	ph7_int64 iOfft;` |
|     87 |   51 | `	if( pDev == 0 ){` |
|    ! 0 |   52 | `		return -1;` |
|      - |   53 | `	}` |
|     87 |   54 | `	if( pDev->pReadFilters ){` |
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
|     61 |   66 | `	if( pDev->pStream == 0 \|\| pDev->pStream->xTell == 0 ){` |
|    ! 0 |   67 | `		return -1;` |
|      - |   68 | `	}` |
|     61 |   69 | `	iOfft = pDev->pStream->xTell(pDev->pHandle);` |
|     61 |   70 | `	if( iOfft < 0 ){` |
|    ! 0 |   71 | `		return iOfft;` |
|      - |   72 | `	}` |
|     61 |   73 | `	return iOfft - (ph7_int64)StreamAheadBytes(pDev);` |
|     45 |   74 | `}` |
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
|    623 |  116 | `static sxu32 StreamAheadBytes(io_private *pDev)` |
|      5 |  117 | `{` |
|    628 |  118 | `	sxu32 n = 0;` |
|    628 |  119 | `	if( SyBlobLength(&pDev->sBuffer) > pDev->nOfft ){` |
|     47 |  120 | `		n += SyBlobLength(&pDev->sBuffer) - pDev->nOfft;` |
|     22 |  121 | `	}` |
|    628 |  122 | `	if( SyBlobLength(&pDev->sFilt) > pDev->nFiltOfft ){` |
|      5 |  123 | `		n += SyBlobLength(&pDev->sFilt) - pDev->nFiltOfft;` |
|      2 |  124 | `	}` |
|    628 |  125 | `	return n;` |
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
|    192 |  190 | `PH7_PRIVATE int PH7_VfsResourceIsClosed(void *pResource)` |
|      5 |  191 | `{` |
|    197 |  192 | `	io_private *pDev = (io_private *)pResource;` |
|    197 |  193 | `	return pDev != 0 && pDev->iMagic == IO_PRIVATE_CLOSED_MAGIC;` |
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
|      2 |  277 | `{` |
|      - |  278 | `	const ph7_io_stream *pStream;` |
|      - |  279 | `	io_private *pDev;` |
|      - |  280 | `	ph7_int64 iOfft;` |
|      - |  281 | `	int whence;` |
|      - |  282 | `	int rc;` |
|     60 |  283 | `	if( nArg < 2 \|\| !ph7_value_is_resource(apArg[0]) ){` |
|      - |  284 | `		/* Missing/Invalid arguments,return FALSE */` |
|    ! 0 |  285 | `		ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"Expecting an IO handle");` |
|    ! 0 |  286 | `		ph7_result_int(pCtx,-1);` |
|    ! 0 |  287 | `		return PH7_OK;` |
|      - |  288 | `	}` |
|      - |  289 | `	/* Extract our private data */` |
|     60 |  290 | `	pDev = (io_private *)ph7_value_to_resource(apArg[0]);` |
|      - |  291 | `	/* Make sure we are dealing with a valid io_private instance */` |
|     60 |  292 | `	if( IO_PRIVATE_INVALID(pDev) ){` |
|      - |  293 | `		/*Expecting an IO handle */` |
|    ! 0 |  294 | `		ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"Expecting an IO handle");` |
|    ! 0 |  295 | `		ph7_result_int(pCtx,-1);` |
|    ! 0 |  296 | `		return PH7_OK;` |
|      - |  297 | `	}` |
|      - |  298 | `	/* Point to the target IO stream device */` |
|     60 |  299 | `	pStream = pDev->pStream;` |
|     60 |  300 | `	if( pStream == 0  \|\| pStream->xSeek == 0){` |
|    ! 0 |  301 | `		ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,` |
|      - |  302 | `			"IO routine(%s) not implemented in the underlying stream(%s) device",` |
|    ! 0 |  303 | `			ph7_function_name(pCtx),pStream ? pStream->zName : "null_stream"` |
|      - |  304 | `			);` |
|    ! 0 |  305 | `		ph7_result_int(pCtx,-1);` |
|    ! 0 |  306 | `		return PH7_OK;` |
|      - |  307 | `	}` |
|      - |  308 | `	/* Extract the offset */` |
|     60 |  309 | `	iOfft = ph7_value_to_int64(apArg[1]);` |
|     60 |  310 | `	whence = 0;/* SEEK_SET */` |
|     60 |  311 | `	if( nArg > 2 ){` |
|      - |  312 | `		/* Read whatever was passed, coercing like php's ZPP: gating this on` |
|      - |  313 | `` 		 * ph7_value_is_int() left `fseek($f, 4, "99")` and `fseek($f, 4, 99.9)` `` |
|      - |  314 | `		 * falling back to whence 0, i.e. the very silent-SEEK_SET the check below` |
|      - |  315 | ``		 * exists to stop (and it disagreed with `99.0`, which is_int accepts). */`` |
|     46 |  316 | `		whence = (int)ph7_value_to_int64(apArg[2]);` |
|     22 |  317 | `	}` |
|     60 |  318 | `	if( whence != 0 /* SEEK_SET */ && whence != 1 /* SEEK_CUR */ && whence != 2 /* SEEK_END */ ){` |
|      - |  319 | `		/* php's php_stream_seek rejects an unknown whence and answers -1 WITHOUT` |
|      - |  320 | `		 * moving the cursor. PHL passed the raw value through to the driver, where` |
|      - |  321 | `		 * every non-CUR/END value fell into the SEEK_SET arm — so fseek($f, 0, 99)` |
|      - |  322 | `		 * reported success (0) AND silently seeked to the start of the stream, two` |
|      - |  323 | `		 * wrong answers from one unchecked argument. php raises no error here. */` |
|     13 |  324 | `		ph7_result_int(pCtx,-1);` |
|     13 |  325 | `		return PH7_OK;` |
|      - |  326 | `	}` |
|     48 |  327 | `	if( pDev->pReadFilters && whence != 2 /* SEEK_END */ ){` |
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
|     42 |  343 | `	if( whence == 1 /* SEEK_CUR */ ){` |
|      - |  344 | `		/* The CURRENT position is the LOGICAL one: the device sits past the` |
|      - |  345 | `		 * read-ahead the line readers buffer, so seek relative to where the` |
|      - |  346 | `		 * SCRIPT is, not where the device is (StreamLogicalAdjust). Without` |
|      - |  347 | `		 * this, fseek($f,2,SEEK_CUR) after an fgets() that buffered ahead` |
|      - |  348 | `		 * skipped everything still sitting in the buffer. */` |
|     10 |  349 | `		iOfft -= (ph7_int64)StreamAheadBytes(pDev);` |
|      4 |  350 | `	}` |
|      - |  351 | `	/* Perform the requested operation */` |
|     42 |  352 | `	rc = pStream->xSeek(pDev->pHandle,iOfft,whence);` |
|     42 |  353 | `	if( rc == PH7_OK ){` |
|      - |  354 | `		/* Ignore buffered data */` |
|     42 |  355 | `		ResetIOPrivate(pDev);` |
|     42 |  356 | `		if( pDev->pReadFilters ){` |
|    ! 0 |  357 | `			pDev->iFiltPos = pStream->xTell ? pStream->xTell(pDev->pHandle) : 0;` |
|    ! 0 |  358 | `		}` |
|     20 |  359 | `	}` |
|      - |  360 | `	/* IO result */` |
|     42 |  361 | `	ph7_result_int(pCtx,rc == PH7_OK ? 0 : - 1);` |
|     42 |  362 | `	return PH7_OK;` |
|     31 |  363 | `}` |
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
|      3 |  376 | `{` |
|      - |  377 | `	const ph7_io_stream *pStream;` |
|      - |  378 | `	io_private *pDev;` |
|      - |  379 | `	ph7_int64 iOfft;` |
|     67 |  380 | `	if( nArg < 1 \|\| !ph7_value_is_resource(apArg[0]) ){` |
|      - |  381 | `		/* Missing/Invalid arguments,return FALSE */` |
|    ! 0 |  382 | `		ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"Expecting an IO handle");` |
|    ! 0 |  383 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 |  384 | `		return PH7_OK;` |
|      - |  385 | `	}` |
|      - |  386 | `	/* Extract our private data */` |
|     67 |  387 | `	pDev = (io_private *)ph7_value_to_resource(apArg[0]);` |
|      - |  388 | `	/* Make sure we are dealing with a valid io_private instance */` |
|     67 |  389 | `	if( IO_PRIVATE_INVALID(pDev) ){` |
|      - |  390 | `		/*Expecting an IO handle */` |
|    ! 0 |  391 | `		ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"Expecting an IO handle");` |
|    ! 0 |  392 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 |  393 | `		return PH7_OK;` |
|      - |  394 | `	}` |
|      - |  395 | `	/* Point to the target IO stream device */` |
|     67 |  396 | `	pStream = pDev->pStream;` |
|     67 |  397 | `	if( pStream == 0  \|\| pStream->xTell == 0){` |
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
|     67 |  409 | `	iOfft = PH7_StreamLogicalTell(pDev);` |
|      - |  410 | `	/* IO result */` |
|     67 |  411 | `	ph7_result_int64(pCtx,iOfft);` |
|     67 |  412 | `	return PH7_OK;` |
|     35 |  413 | `}` |
|      - |  414 | `/*` |
|      - |  415 | ` * bool rewind(resource $handle)` |
|      - |  416 | ` *  Rewind the position of a file pointer.` |
|      - |  417 | ` * Parameters` |
|      - |  418 | ` *  $handle` |
|      - |  419 | ` *   The file pointer.` |
|      - |  420 | ` * Return` |
|      - |  421 | ` *  TRUE on success or FALSE on failure.` |
|      - |  422 | ` */` |
|    308 |  423 | `PH7_PRIVATE int PH7_builtin_rewind(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      3 |  424 | `{` |
|      - |  425 | `	const ph7_io_stream *pStream;` |
|      - |  426 | `	io_private *pDev;` |
|      - |  427 | `	int rc;` |
|    311 |  428 | `	if( nArg < 1 \|\| !ph7_value_is_resource(apArg[0]) ){` |
|      - |  429 | `		/* Missing/Invalid arguments,return FALSE */` |
|    ! 0 |  430 | `		ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"Expecting an IO handle");` |
|    ! 0 |  431 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 |  432 | `		return PH7_OK;` |
|      - |  433 | `	}` |
|      - |  434 | `	/* Extract our private data */` |
|    311 |  435 | `	pDev = (io_private *)ph7_value_to_resource(apArg[0]);` |
|      - |  436 | `	/* Make sure we are dealing with a valid io_private instance */` |
|    311 |  437 | `	if( IO_PRIVATE_INVALID(pDev) ){` |
|      - |  438 | `		/*Expecting an IO handle */` |
|    ! 0 |  439 | `		ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"Expecting an IO handle");` |
|    ! 0 |  440 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 |  441 | `		return PH7_OK;` |
|      - |  442 | `	}` |
|      - |  443 | `	/* Point to the target IO stream device */` |
|    311 |  444 | `	pStream = pDev->pStream;` |
|    311 |  445 | `	if( pStream == 0  \|\| pStream->xSeek == 0){` |
|    ! 0 |  446 | `		ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,` |
|      - |  447 | `			"IO routine(%s) not implemented in the underlying stream(%s) device,PH7 is returning FALSE",` |
|    ! 0 |  448 | `			ph7_function_name(pCtx),pStream ? pStream->zName : "null_stream"` |
|      - |  449 | `			);` |
|    ! 0 |  450 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 |  451 | `		return PH7_OK;` |
|      - |  452 | `	}` |
|      - |  453 | `	/* Perform the requested operation */` |
|    311 |  454 | `	rc = pStream->xSeek(pDev->pHandle,0,0/*SEEK_SET*/);` |
|    311 |  455 | `	if( rc == PH7_OK ){` |
|      - |  456 | `		/* Ignore buffered data */` |
|    311 |  457 | `		ResetIOPrivate(pDev);` |
|    154 |  458 | `	}` |
|      - |  459 | `	/* IO result */` |
|    311 |  460 | `	ph7_result_bool(pCtx,rc == PH7_OK);` |
|    311 |  461 | `	return PH7_OK;` |
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
|  23092 |  520 | `static int IoPrivateAtEof(io_private *pDev)` |
|      5 |  521 | `{` |
|      - |  522 | `	int bEof;` |
|  23097 |  523 | `	if( SyBlobLength(&pDev->sBuffer) > pDev->nOfft ){` |
|      - |  524 | `		/* Buffered bytes are not an end. */` |
|  14981 |  525 | `		return 0;` |
|      - |  526 | `	}` |
|   8121 |  527 | `	if( IoPrivateUwrapEof(pDev,&bEof) ){` |
|      - |  528 | `		/* A userland wrapper answers the question itself — php calls its` |
|      - |  529 | `		 * streamWrapper::stream_eof() rather than inferring anything. */` |
|      8 |  530 | `		return bEof;` |
|      - |  531 | `	}` |
|   8115 |  532 | `	return pDev->bEof != 0;` |
|  11551 |  533 | `}` |
|      - |  534 | `/*` |
|      - |  535 | ` * bool feof(resource $handle)` |
|      - |  536 | ` *  Tests for end-of-file on a file pointer.` |
|      - |  537 | ` * Parameters` |
|      - |  538 | ` *  $handle` |
|      - |  539 | ` *   The file pointer.` |
|      - |  540 | ` * Return` |
|      - |  541 | ` *  Returns TRUE if the file pointer is at EOF.FALSE otherwise` |
|      - |  542 | ` */` |
|  23024 |  543 | `PH7_PRIVATE int PH7_builtin_feof(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      5 |  544 | `{` |
|      - |  545 | `	const ph7_io_stream *pStream;` |
|      - |  546 | `	io_private *pDev;` |
|      - |  547 | `	int rc;` |
|  23029 |  548 | `	if( nArg < 1 \|\| !ph7_value_is_resource(apArg[0]) ){` |
|      - |  549 | `		/* Missing/Invalid arguments */` |
|    ! 0 |  550 | `		ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"Expecting an IO handle");` |
|    ! 0 |  551 | `		ph7_result_bool(pCtx,1);` |
|    ! 0 |  552 | `		return PH7_OK;` |
|      - |  553 | `	}` |
|      - |  554 | `	/* Extract our private data */` |
|  23029 |  555 | `	pDev = (io_private *)ph7_value_to_resource(apArg[0]);` |
|      - |  556 | `	/* Make sure we are dealing with a valid io_private instance */` |
|  23029 |  557 | `	if( IO_PRIVATE_INVALID(pDev) ){` |
|      - |  558 | `		/*Expecting an IO handle */` |
|    ! 0 |  559 | `		ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"Expecting an IO handle");` |
|    ! 0 |  560 | `		ph7_result_bool(pCtx,1);` |
|    ! 0 |  561 | `		return PH7_OK;` |
|      - |  562 | `	}` |
|      - |  563 | `	/* Point to the target IO stream device */` |
|  23029 |  564 | `	pStream = pDev->pStream;` |
|  23029 |  565 | `	if( pStream == 0 ){` |
|    ! 0 |  566 | `		ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,` |
|      - |  567 | `			"IO routine(%s) not implemented in the underlying stream(%s) device,PH7 is returning FALSE",` |
|    ! 0 |  568 | `			ph7_function_name(pCtx),pStream ? pStream->zName : "null_stream"` |
|      - |  569 | `			);` |
|    ! 0 |  570 | `		ph7_result_bool(pCtx,1);` |
|    ! 0 |  571 | `		return PH7_OK;` |
|      - |  572 | `	}` |
|  23029 |  573 | `	rc = IoPrivateAtEof(pDev);` |
|      - |  574 | `	/* EOF or not */` |
|  23029 |  575 | `	ph7_result_bool(pCtx,rc != 0);` |
|  23029 |  576 | `	return PH7_OK;` |
|  11517 |  577 | `}` |
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
|  10156 |  599 | `static ph7_int64 IoPrivateRawRead(io_private *pDev,void *pBuf,ph7_int64 nLen)` |
|      5 |  600 | `{` |
|      - |  601 | `	ph7_int64 n;` |
|  10161 |  602 | `	pDev->bTimedOut = 0;` |
|  10161 |  603 | `	errno = 0;` |
|  10161 |  604 | `	n = pDev->pStream->xRead(pDev->pHandle,pBuf,nLen);` |
|  10156 |  605 | `	if( n < 0 && (errno == EAGAIN \|\| errno == EWOULDBLOCK)` |
|     16 |  606 | `	 && pDev->bHasTimeout && !pDev->bNonBlock ){` |
|      5 |  607 | `		pDev->bTimedOut = 1;` |
|      2 |  608 | `	}` |
|  10161 |  609 | `	return n;` |
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
|   9090 |  690 | `static ph7_int64 IoPrivateDeviceRead(io_private *pDev,void *pBuf,ph7_int64 nLen)` |
|      5 |  691 | `{` |
|   9095 |  692 | `	if( pDev->pReadFilters != 0 \|\| SyBlobLength(&pDev->sFilt) > pDev->nFiltOfft ){` |
|      - |  693 | `		/* Bytes can still be waiting after the last read filter was REMOVED:` |
|      - |  694 | `		 * php flushes a filter on its way out and what it emitted belongs to` |
|      - |  695 | `		 * the reader that comes next. */` |
|    302 |  696 | `		return IoPrivateFilteredRead(pDev,pBuf,nLen);` |
|      - |  697 | `	}` |
|   8795 |  698 | `	return IoPrivateRawRead(pDev,pBuf,nLen);` |
|   4550 |  699 | `}` |
|    790 |  700 | `PH7_PRIVATE ph7_int64 PH7_StreamRead(io_private *pDev,void *pBuf,ph7_int64 nLen)` |
|      5 |  701 | `{` |
|    795 |  702 | `	const ph7_io_stream *pStream = pDev->pStream;` |
|    795 |  703 | `	char *zBuf = (char *)pBuf;` |
|      - |  704 | `	ph7_int64 n,nRead;` |
|    795 |  705 | `	n = SyBlobLength(&pDev->sBuffer) - pDev->nOfft;` |
|    795 |  706 | `	if( n > 0 ){` |
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
|    791 |  728 | `	nRead = IoPrivateDeviceRead(pDev,zBuf,nLen);` |
|    786 |  729 | `	if( nRead == 0` |
|    621 |  730 | `	 \|\| (nRead > 0 && nRead < nLen && pStream->xSeek != 0 && pStream->xTell != 0` |
|    312 |  731 | `	     && pStream->xTell(pDev->pHandle) >= 0) ){` |
|      - |  732 | `		/* A read that came back with nothing IS php's end-of-file event, and` |
|      - |  733 | `		 * so is a SHORT one on a device that can say where it IS: php fills` |
|      - |  734 | ``		 * its buffer in a loop, so `fread($f, 100)` on a 12-byte file performs`` |
|      - |  735 | `		 * the second read that finds the end. The position query is what tells` |
|      - |  736 | `		 * a regular file from a FIFO — both arrive here through the same file` |
|      - |  737 | `		 * device, and a short read from a fifo, a pipe or a socket means only` |
|      - |  738 | `		 * that less had arrived, so latching there would end` |
|      - |  739 | ``		 * `while (!feof($p)) $s .= fread($p, 8192);` with data still coming. A`` |
|      - |  740 | `		 * NEGATIVE answer is an IO error and never latches. */` |
|    641 |  741 | `		pDev->bEof = 1;` |
|    318 |  742 | `	}` |
|    791 |  743 | `	if( nRead > 0 ){` |
|    435 |  744 | `		n += nRead;` |
|    576 |  745 | `	}else if( n < 1 ){` |
|      - |  746 | `		/* EOF or IO error */` |
|    357 |  747 | `		return nRead;` |
|      - |  748 | `	}` |
|    439 |  749 | `	return n;` |
|    400 |  750 | `}` |
|      - |  751 | `/*` |
|      - |  752 | ` * Every SCRIPT-level write goes through here, because a handle can carry a` |
|      - |  753 | ` * WRITE chain: php runs what the script wrote through the filters before the` |
|      - |  754 | ` * device sees any of it, and a filter changes the byte count — so what reaches` |
|      - |  755 | ` * the device is not what was handed in, while what fwrite() ANSWERS still is` |
|      - |  756 | ` * (php reports the bytes it CONSUMED, not the bytes it emitted).` |
|      - |  757 | ` */` |
|    495 |  758 | `PH7_PRIVATE ph7_int64 PH7_StreamWrite(io_private *pDev,const void *pData,ph7_int64 nLen)` |
|      5 |  759 | `{` |
|    500 |  760 | `	phl_stream_filter *pChain = (phl_stream_filter *)pDev->pWriteFilters;` |
|      - |  761 | `	SyBlob sOut;` |
|      - |  762 | `	ph7_int64 nWr;` |
|      - |  763 | `	int iStatus;` |
|    500 |  764 | `	if( pDev->pStream == 0 \|\| pDev->pStream->xWrite == 0 ){` |
|    ! 0 |  765 | `		return -1;` |
|      - |  766 | `	}` |
|    500 |  767 | `	if( pChain == 0 ){` |
|    470 |  768 | `		return pDev->pStream->xWrite(pDev->pHandle,pData,nLen);` |
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
|    249 |  789 | `}` |
|      - |  790 | `/*` |
|      - |  791 | ` * Extract a single line from the buffered input.` |
|      - |  792 | ` */` |
|  18092 |  793 | `static sxi32 GetLine(io_private *pDev,ph7_int64 *pLen,const char **pzLine)` |
|      5 |  794 | `{` |
|      - |  795 | `	const char *zIn,*zEnd,*zPtr;` |
|  18097 |  796 | `	zIn = (const char *)SyBlobDataAt(&pDev->sBuffer,pDev->nOfft);` |
|  18097 |  797 | `	zEnd = &zIn[SyBlobLength(&pDev->sBuffer)-pDev->nOfft];` |
|  18097 |  798 | `	zPtr = zIn;` |
| 824076 |  799 | `	while( zIn < zEnd ){` |
| 823786 |  800 | `		if( zIn[0] == '\n' ){` |
|      - |  801 | `			/* Line found */` |
|  17807 |  802 | `			zIn++; /* Include the line ending as requested by the PHP specification */` |
|  17807 |  803 | `			*pLen = (ph7_int64)(zIn-zPtr);` |
|  17807 |  804 | `			*pzLine = zPtr;` |
|  17807 |  805 | `			return SXRET_OK;` |
|      - |  806 | `		}` |
| 805984 |  807 | `		zIn++;` |
|      5 |  808 | `	}` |
|      - |  809 | `	/* No line were found */` |
|    295 |  810 | `	return SXERR_NOTFOUND;` |
|   9051 |  811 | `}` |
|      - |  812 | `/*` |
|      - |  813 | ` * Read a single line from the underlying IO stream device.` |
|      - |  814 | ` */` |
|  23110 |  815 | `static ph7_int64 StreamReadLine(io_private *pDev,const char **pzData,ph7_int64 nMaxLen)` |
|      5 |  816 | `{` |
|      - |  817 | `	char zBuf[8192];` |
|      - |  818 | `	ph7_int64 n;` |
|      - |  819 | `	sxi32 rc;` |
|  23115 |  820 | `	n = 0;` |
|  23115 |  821 | `	if( pDev->nOfft >= SyBlobLength(&pDev->sBuffer) ){` |
|      - |  822 | `		/* Reset the working buffer so that we avoid excessive memory allocation */` |
|   8013 |  823 | `		SyBlobReset(&pDev->sBuffer);` |
|   8013 |  824 | `		pDev->nOfft = 0;` |
|   4004 |  825 | `	}` |
|  23115 |  826 | `	if( SyBlobLength(&pDev->sBuffer) > pDev->nOfft ){` |
|      - |  827 | `		/* Check if there is a line */` |
|  15107 |  828 | `		rc = GetLine(pDev,&n,pzData);` |
|  15107 |  829 | `		if( rc == SXRET_OK ){` |
|      - |  830 | `			/* Got line. php caps it at nMaxLen bytes even when the newline` |
|      - |  831 | `			 * falls later; the remainder (incl. the newline) stays buffered. */` |
|  15033 |  832 | `			if( nMaxLen > 0 && n > nMaxLen ){` |
|    ! 0 |  833 | `				n = nMaxLen;` |
|    ! 0 |  834 | `			}` |
|  15033 |  835 | `			pDev->nOfft += (sxu32)n;` |
|  15033 |  836 | `			return n;` |
|      - |  837 | `		}` |
|     37 |  838 | `	}` |
|      - |  839 | `	/* Perform the read operation until a new line is extracted or length` |
|      - |  840 | `	 * limit is reached.` |
|      - |  841 | `	 */` |
|   4131 |  842 | `	for(;;){` |
|     90 |  843 | `		{` |
|      - |  844 | `			/* php fills its read buffer one CHUNK at a time, and` |
|      - |  845 | `			 * stream_set_chunk_size() is how a script asks for a smaller one. */` |
|   8267 |  846 | `			ph7_int64 nAsk = (ph7_int64)sizeof(zBuf);` |
|   8267 |  847 | `			if( pDev->nChunk > 0 && (ph7_int64)pDev->nChunk < nAsk ){` |
|     30 |  848 | `				nAsk = (ph7_int64)pDev->nChunk;` |
|     15 |  849 | `			}` |
|   8267 |  850 | `			if( nMaxLen > 0 && nMaxLen < nAsk ){` |
|     69 |  851 | `				nAsk = nMaxLen;` |
|     34 |  852 | `			}` |
|   8267 |  853 | `			n = IoPrivateDeviceRead(pDev,zBuf,nAsk);` |
|      - |  854 | `		}` |
|   8267 |  855 | `		if( n == 0 ){` |
|   5275 |  856 | `			pDev->bEof = 1;` |
|   2635 |  857 | `		}` |
|   8267 |  858 | `		if( n < 1 ){` |
|      - |  859 | `			/* EOF or IO error */` |
|   5277 |  860 | `			break;` |
|      - |  861 | `		}` |
|      - |  862 | `		/* Append the data just read */` |
|   2995 |  863 | `		SyBlobAppend(&pDev->sBuffer,zBuf,(sxu32)n);` |
|      - |  864 | `		/* Try to extract a line */` |
|   2995 |  865 | `		rc = GetLine(pDev,&n,pzData);` |
|   2995 |  866 | `		if( rc == SXRET_OK ){` |
|      - |  867 | `			/* Got one. Cap at nMaxLen (php's length limit); anything past the` |
|      - |  868 | `			 * cap, newline included, is left buffered for the next read. */` |
|   2779 |  869 | `			if( nMaxLen > 0 && n > nMaxLen ){` |
|      7 |  870 | `				n = nMaxLen;` |
|      3 |  871 | `			}` |
|   2779 |  872 | `			pDev->nOfft += (sxu32)n;` |
|   2779 |  873 | `			return n;` |
|      - |  874 | `		}` |
|    221 |  875 | `		if( nMaxLen > 0 && (SyBlobLength(&pDev->sBuffer) - pDev->nOfft >= nMaxLen) ){` |
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
|   5277 |  887 | `	if( SyBlobLength(&pDev->sBuffer) > pDev->nOfft ){` |
|      - |  888 | `		/* Read limit reached,return the available data */` |
|    221 |  889 | `		*pzData = (const char *)SyBlobDataAt(&pDev->sBuffer,pDev->nOfft);` |
|    221 |  890 | `		n = SyBlobLength(&pDev->sBuffer) - pDev->nOfft;` |
|      - |  891 | `		/* Reset the working buffer */` |
|    221 |  892 | `		SyBlobReset(&pDev->sBuffer);` |
|    221 |  893 | `		pDev->nOfft = 0;` |
|    108 |  894 | `	}` |
|   5277 |  895 | `	return n;` |
|  11560 |  896 | `}` |
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
|  36538 |  918 | `PH7_PRIVATE void * PH7_StreamOpenHandle(ph7_vm *pVm,const ph7_io_stream *pStream,const char *zFile,` |
|      - |  919 | `	int iFlags,int use_include,ph7_value *pResource,int bPushInclude,int *pNew,const char *zCaller)` |
|      5 |  920 | `{` |
|  36543 |  921 | `	void *pHandle = 0; /* cc warning */` |
|      - |  922 | `	SyString sFile;` |
|      - |  923 | `	ph7_value sDummy;` |
|      - |  924 | `	int rc;` |
|  36543 |  925 | `	if( pStream == 0 ){` |
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
|  36543 |  936 | `	if( PH7_StreamIsUrlWrapper(pStream) ){` |
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
|  36533 |  957 | `	if( pResource == 0 ){` |
|      - |  958 | `		/* VM-dependent devices (php://, data://, tcp://, userland wrappers)` |
|      - |  959 | `		 * reach the VM only through pResource->pVm — their xOpen has no vm` |
|      - |  960 | `		 * parameter. Callers like file_get_contents pass no resource, so hand` |
|      - |  961 | `		 * every device a synthesized stack value carrying the VM; xOpen only` |
|      - |  962 | `		 * reads it during the call, and file:// ignores it. */` |
|  36051 |  963 | `		PH7_MemObjInit(pVm,&sDummy);` |
|  36051 |  964 | `		pResource = &sDummy;` |
|  18023 |  965 | `	}` |
|  36533 |  966 | `	SyStringInitFromBuf(&sFile,zFile,SyStrlen(zFile));` |
|  36533 |  967 | `	if( use_include ){` |
|   5245 |  968 | `		if(	/* include_path names DIRECTORIES, so it has nothing to say about a` |
|      - |  969 | `` 			 * URL: walking it for a `php://filter/…` one built `<dir>/filter/…` `` |
|      - |  970 | `			 * and reported the whole open as an IO error. The direct arm is the` |
|      - |  971 | `			 * one that also marks the file as included, which is what` |
|      - |  972 | `			 * include_once needs. */` |
|  10490 |  973 | `			pStream != pVm->pDefStream \|\|` |
|  10478 |  974 | `			sFile.zString[0] == '/' \|\|` |
|      - |  975 | `#ifdef __WINNT__` |
|      - |  976 | `			(sFile.nByte > 2 && sFile.zString[1] == ':' && (sFile.zString[2] == '\\' \|\| sFile.zString[2] == '/') ) \|\|` |
|      - |  977 | `#endif` |
|  10385 |  978 | `			(sFile.nByte > 1 && sFile.zString[0] == '.' && sFile.zString[1] == '/') \|\|` |
|  10378 |  979 | `			(sFile.nByte > 2 && sFile.zString[0] == '.' && sFile.zString[1] == '.' && sFile.zString[2] == '/') ){` |
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
|  10382 |  995 | `			SyBlobInit(&sWorker,&pVm->sAllocator);` |
|      - |  996 | `			/* Build a path from the set of include path */` |
|  10382 |  997 | `			SySetResetCursor(&pVm->aPaths);` |
|  10382 |  998 | `			rc = SXERR_IO;` |
|  10406 |  999 | `			while( SXRET_OK == SySetGetNextEntry(&pVm->aPaths,(void **)&pPath) ){` |
|      - | 1000 | `				/* Build full path */` |
|  10390 | 1001 | `				SyBlobFormat(&sWorker,"%z%c%z",pPath,c,&sFile);` |
|      - | 1002 | `				/* Append null terminator */` |
|  10390 | 1003 | `				if( SXRET_OK != SyBlobNullAppend(&sWorker) ){` |
|    ! 0 | 1004 | `					continue;` |
|      - | 1005 | `				}` |
|      - | 1006 | `				/* Try to open the file */` |
|  10390 | 1007 | `				rc = pStream->xOpen((const char *)SyBlobData(&sWorker),iFlags,pResource,&pHandle);` |
|  10390 | 1008 | `				if( rc == PH7_OK ){` |
|  10366 | 1009 | `					if( bPushInclude ){` |
|      - | 1010 | `						/* Mark as included */` |
|  10366 | 1011 | `						PH7_VmPushFilePath(pVm,(const char *)SyBlobData(&sWorker),SyBlobLength(&sWorker),FALSE,pNew);` |
|   5181 | 1012 | `					}` |
|  10366 | 1013 | `					break;` |
|      - | 1014 | `				}` |
|      - | 1015 | `				/* Reset the working buffer */` |
|     28 | 1016 | `				SyBlobReset(&sWorker);` |
|      - | 1017 | `				/* Check the next path */` |
|      4 | 1018 | `			}` |
|  10382 | 1019 | `			if( rc != PH7_OK ){` |
|      - | 1020 | `				/* php's LAST RESORT, and the one PHL never had: the directory of` |
|      - | 1021 | ``				 * the file that is EXECUTING. `include 'lib.php'` next to the`` |
|      - | 1022 | `				 * script has to work whatever directory the script was started` |
|      - | 1023 | `				 * from -- see PH7_VmExecutingDir(). Tried after the include_path` |
|      - | 1024 | `				 * entries, as php tries it. */` |
|      - | 1025 | `				SyString sDir;` |
|     20 | 1026 | `				if( PH7_VmExecutingDir(pVm,&sDir) ){` |
|     20 | 1027 | `					SyBlobReset(&sWorker);` |
|     20 | 1028 | `					SyBlobFormat(&sWorker,"%z%c%z",&sDir,c,&sFile);` |
|     20 | 1029 | `					if( SXRET_OK == SyBlobNullAppend(&sWorker) ){` |
|     20 | 1030 | `						rc = pStream->xOpen((const char *)SyBlobData(&sWorker),iFlags,pResource,&pHandle);` |
|     20 | 1031 | `						if( rc == PH7_OK && bPushInclude ){` |
|      4 | 1032 | `							PH7_VmPushFilePath(pVm,(const char *)SyBlobData(&sWorker),` |
|      2 | 1033 | `								SyBlobLength(&sWorker),FALSE,pNew);` |
|      1 | 1034 | `						}` |
|      8 | 1035 | `					}` |
|      8 | 1036 | `				}` |
|      8 | 1037 | `			}` |
|  10382 | 1038 | `			SyBlobRelease(&sWorker);` |
|      - | 1039 | `		}` |
|   5250 | 1040 | `	}else{` |
|      - | 1041 | `		/* Open the URI direcly */` |
|  26043 | 1042 | `		rc = pStream->xOpen(zFile,iFlags,pResource,&pHandle);` |
|      - | 1043 | `	}` |
|      - | 1044 | `	/* The armed context describes exactly ONE open — every attempt of the` |
|      - | 1045 | `	 * include-path walk above included — so it is dropped here whether the open` |
|      - | 1046 | `	 * worked or not. A device that wanted it (a userland wrapper) read it while` |
|      - | 1047 | `	 * its xOpen was running. */` |
|  36533 | 1048 | `	pVm->pOpenCtx = 0;` |
|  36533 | 1049 | `	if( rc != PH7_OK ){` |
|      - | 1050 | `		/* IO error */` |
|     95 | 1051 | `		return 0;` |
|      - | 1052 | `	}` |
|      - | 1053 | `	/* Return the file handle */` |
|  36443 | 1054 | `	return pHandle;` |
|  18274 | 1055 | `}` |
|      - | 1056 | `/*` |
|      - | 1057 | ` * Read the whole contents of an open IO stream handle [i.e local file/URL..]` |
|      - | 1058 | ` * Store the read data in the given BLOB (last argument).` |
|      - | 1059 | ` * The read operation is stopped when he hit the EOF or an IO error occurs.` |
|      - | 1060 | ` */` |
|  10474 | 1061 | `PH7_PRIVATE sxi32 PH7_StreamReadWholeFile(void *pHandle,const ph7_io_stream *pStream,SyBlob *pOut)` |
|      5 | 1062 | `{` |
|      - | 1063 | `	ph7_int64 nRead;` |
|      - | 1064 | `	char zBuf[8192]; /* 8K */` |
|      - | 1065 | `	int rc;` |
|      - | 1066 | `	/* Perform the requested operation */` |
|  10478 | 1067 | `	for(;;){` |
|  20961 | 1068 | `		nRead = pStream->xRead(pHandle,zBuf,sizeof(zBuf));` |
|  20961 | 1069 | `		if( nRead < 1 ){` |
|      - | 1070 | `			/* EOF or IO error */` |
|  10479 | 1071 | `			break;` |
|      - | 1072 | `		}` |
|      - | 1073 | `		/* Append contents */` |
|  10487 | 1074 | `		rc = SyBlobAppend(pOut,zBuf,(sxu32)nRead);` |
|  10487 | 1075 | `		if( rc != SXRET_OK ){` |
|    ! 0 | 1076 | `			break;` |
|      - | 1077 | `		}` |
|      5 | 1078 | `	}` |
|  10479 | 1079 | `	return SyBlobLength(pOut) > 0 ? SXRET_OK : -1;` |
|      5 | 1080 | `}` |
|      - | 1081 | `/*` |
|      - | 1082 | ` * Close an open IO stream handle [i.e local file/URI..].` |
|      - | 1083 | ` */` |
|  36590 | 1084 | `PH7_PRIVATE void PH7_StreamCloseHandle(const ph7_io_stream *pStream,void *pHandle)` |
|      5 | 1085 | `{` |
|  36595 | 1086 | `	if( pStream->xClose ){` |
|  36595 | 1087 | `		pStream->xClose(pHandle);` |
|  18295 | 1088 | `	}` |
|  36595 | 1089 | `}` |
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
|  22920 | 1160 | `PH7_PRIVATE int PH7_builtin_fgets(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      5 | 1161 | `{` |
|      - | 1162 | `	const ph7_io_stream *pStream;` |
|      - | 1163 | `	const char *zLine;` |
|      - | 1164 | `	io_private *pDev;` |
|      - | 1165 | `	ph7_int64 n,nLen;` |
|  22925 | 1166 | `	if( nArg < 1 \|\| !ph7_value_is_resource(apArg[0]) ){` |
|      - | 1167 | `		/* Missing/Invalid arguments,return FALSE */` |
|    ! 0 | 1168 | `		ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"Expecting an IO handle");` |
|    ! 0 | 1169 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 1170 | `		return PH7_OK;` |
|      - | 1171 | `	}` |
|      - | 1172 | `	/* Extract our private data */` |
|  22925 | 1173 | `	pDev = (io_private *)ph7_value_to_resource(apArg[0]);` |
|      - | 1174 | `	/* Make sure we are dealing with a valid io_private instance */` |
|  22925 | 1175 | `	if( IO_PRIVATE_INVALID(pDev) ){` |
|      - | 1176 | `		/*Expecting an IO handle */` |
|    ! 0 | 1177 | `		ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"Expecting an IO handle");` |
|    ! 0 | 1178 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 1179 | `		return PH7_OK;` |
|      - | 1180 | `	}` |
|      - | 1181 | `	/* Point to the target IO stream device */` |
|  22925 | 1182 | `	pStream = pDev->pStream;` |
|  22925 | 1183 | `	if( pStream == 0  ){` |
|    ! 0 | 1184 | `		ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,` |
|      - | 1185 | `			"IO routine(%s) not implemented in the underlying stream(%s) device,PH7 is returning FALSE",` |
|    ! 0 | 1186 | `			ph7_function_name(pCtx),pStream ? pStream->zName : "null_stream"` |
|      - | 1187 | `			);` |
|    ! 0 | 1188 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 1189 | `		return PH7_OK;` |
|      - | 1190 | `	}` |
|  22925 | 1191 | `	nLen = -1;` |
|  22925 | 1192 | `	if( nArg > 1 && !ph7_value_is_null(apArg[1]) ){` |
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
|  22919 | 1210 | `	n = StreamReadLine(pDev,&zLine,nLen);` |
|  22919 | 1211 | `	if( n < 1 ){` |
|      - | 1212 | `		/* EOF or IO error,return FALSE */` |
|   5019 | 1213 | `		ph7_result_bool(pCtx,0);` |
|   2512 | 1214 | `	}else{` |
|      - | 1215 | `		/* Return the freshly extracted line */` |
|  17905 | 1216 | `		ph7_result_string(pCtx,zLine,(int)n);` |
|      - | 1217 | `	}` |
|  22919 | 1218 | `	return PH7_OK;` |
|  11465 | 1219 | `}` |
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
|    140 | 1346 | `PH7_PRIVATE int PH7_builtin_fread(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      5 | 1347 | `{` |
|      - | 1348 | `	const ph7_io_stream *pStream;` |
|      - | 1349 | `	io_private *pDev;` |
|      - | 1350 | `	ph7_int64 nRead;` |
|      - | 1351 | `	void *pBuf;` |
|      - | 1352 | `	int nLen;` |
|    145 | 1353 | `	if( nArg < 1 \|\| !ph7_value_is_resource(apArg[0]) ){` |
|      - | 1354 | `		/* Missing/Invalid arguments,return FALSE */` |
|    ! 0 | 1355 | `		ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"Expecting an IO handle");` |
|    ! 0 | 1356 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 1357 | `		return PH7_OK;` |
|      - | 1358 | `	}` |
|      - | 1359 | `	/* Extract our private data */` |
|    145 | 1360 | `	pDev = (io_private *)ph7_value_to_resource(apArg[0]);` |
|      - | 1361 | `	/* Make sure we are dealing with a valid io_private instance */` |
|    145 | 1362 | `	if( IO_PRIVATE_INVALID(pDev) ){` |
|      - | 1363 | `		/*Expecting an IO handle */` |
|    ! 0 | 1364 | `		ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"Expecting an IO handle");` |
|    ! 0 | 1365 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 1366 | `		return PH7_OK;` |
|      - | 1367 | `	}` |
|      - | 1368 | `	/* Point to the target IO stream device */` |
|    145 | 1369 | `	pStream = pDev->pStream;` |
|    145 | 1370 | `	if( pStream == 0  ){` |
|    ! 0 | 1371 | `		ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,` |
|      - | 1372 | `			"IO routine(%s) not implemented in the underlying stream(%s) device,PH7 is returning FALSE",` |
|    ! 0 | 1373 | `			ph7_function_name(pCtx),pStream ? pStream->zName : "null_stream"` |
|      - | 1374 | `			);` |
|    ! 0 | 1375 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 1376 | `		return PH7_OK;` |
|      - | 1377 | `	}` |
|    145 | 1378 | `        nLen = 4096;` |
|    145 | 1379 | `	if( nArg > 1 ){` |
|      - | 1380 | `	  /* PHP 8 raises a catchable ValueError for a non-positive length; the` |
|      - | 1381 | `	   * $length parameter is non-nullable, so a NULL is rejected upstream by` |
|      - | 1382 | `	   * the central type screen (the recorded null-policy divergence). */` |
|    145 | 1383 | `	  ph7_int64 nWant = ph7_value_to_int64(apArg[1]);` |
|    145 | 1384 | `	  if( nWant < 1 ){` |
|      5 | 1385 | `		return PH7_VmThrowException(pCtx,"ValueError",` |
|      - | 1386 | `			"fread(): Argument #2 ($length) must be greater than 0");` |
|      - | 1387 | `	  }` |
|    141 | 1388 | `	  nLen = (int)nWant;` |
|    141 | 1389 | `	  if( nLen < 1 ){` |
|      - | 1390 | `		/* A > INT_MAX length overflowed the int cast; read a single chunk` |
|      - | 1391 | `		 * (StreamRead returns only what the stream holds) instead of` |
|      - | 1392 | `		 * over-allocating -- matching the pre-existing behavior for lengths` |
|      - | 1393 | `		 * that do not fit an int. */` |
|    ! 0 | 1394 | `		nLen = 4096;` |
|    ! 0 | 1395 | `	  }` |
|     68 | 1396 | `        }` |
|      - | 1397 | `	/* Allocate enough buffer */` |
|    141 | 1398 | `	pBuf = ph7_context_alloc_chunk(pCtx,(unsigned int)nLen,FALSE,FALSE);` |
|    141 | 1399 | `	if( pBuf == 0 ){` |
|    ! 0 | 1400 | `		ph7_context_throw_error(pCtx,PH7_CTX_ERR,"PH7 is running out of memory");` |
|    ! 0 | 1401 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 1402 | `		return PH7_OK;` |
|      - | 1403 | `	}` |
|      - | 1404 | `	/* Perform the requested operation */` |
|    141 | 1405 | `	errno = 0;` |
|    141 | 1406 | `	nRead = PH7_StreamRead(pDev,pBuf,(ph7_int64)nLen);` |
|    141 | 1407 | `	if( nRead < 0 && (errno == EAGAIN \|\| errno == EWOULDBLOCK) ){` |
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
|    138 | 1425 | `	}else if( nRead < 0 ){` |
|      - | 1426 | `		/* A real IO error, which is php's other false here. */` |
|     11 | 1427 | `		ph7_result_bool(pCtx,0);` |
|      7 | 1428 | `	}else{` |
|      - | 1429 | `		/* Make a copy of the data just read. Zero bytes is EOF, not a failure:` |
|      - | 1430 | `		 * php answers "" for it (php_stream_read returns 0 and the empty string` |
|      - | 1431 | `		 * rides through), where PHL answered FALSE — so the ordinary` |
|      - | 1432 | ``		 * `while (!feof($f)) $buf .= fread($f, 8192);` loop ended on a value`` |
|      - | 1433 | ``		 * that means "the read failed" and a `=== false` guard fired at EOF. */`` |
|    127 | 1434 | `		ph7_result_string(pCtx,(const char *)pBuf,nRead > 0 ? (int)nRead : 0);` |
|      - | 1435 | `	}` |
|      - | 1436 | `	/* Release the buffer */` |
|    141 | 1437 | `	ph7_context_free_chunk(pCtx,pBuf);` |
|    141 | 1438 | `	return PH7_OK;` |
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
|  13745 | 1658 | `PH7_PRIVATE int PH7_builtin_readdir(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      5 | 1659 | `{` |
|      - | 1660 | `	const ph7_io_stream *pStream;` |
|      - | 1661 | `	io_private *pDev;` |
|      - | 1662 | `	int rc;` |
|  13750 | 1663 | `	if( nArg < 1 \|\| !ph7_value_is_resource(apArg[0]) ){` |
|      - | 1664 | `		/* Missing/Invalid arguments,return FALSE */` |
|    ! 0 | 1665 | `		ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"Expecting an IO handle");` |
|    ! 0 | 1666 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 1667 | `		return PH7_OK;` |
|      - | 1668 | `	}` |
|      - | 1669 | `	/* Extract our private data */` |
|  13750 | 1670 | `	pDev = (io_private *)ph7_value_to_resource(apArg[0]);` |
|      - | 1671 | `	/* Make sure we are dealing with a valid io_private instance */` |
|  13750 | 1672 | `	if( IO_PRIVATE_INVALID(pDev) ){` |
|      - | 1673 | `		/*Expecting an IO handle */` |
|    ! 0 | 1674 | `		ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"Expecting an IO handle");` |
|    ! 0 | 1675 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 1676 | `		return PH7_OK;` |
|      - | 1677 | `	}` |
|      - | 1678 | `	/* Point to the target IO stream device */` |
|  13750 | 1679 | `	pStream = pDev->pStream;` |
|  13750 | 1680 | `	if( pStream == 0  \|\| pStream->xReadDir == 0 ){` |
|    ! 0 | 1681 | `		ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,` |
|      - | 1682 | `			"IO routine(%s) not implemented in the underlying stream(%s) device,PH7 is returning FALSE",` |
|    ! 0 | 1683 | `			ph7_function_name(pCtx),pStream ? pStream->zName : "null_stream"` |
|      - | 1684 | `			);` |
|    ! 0 | 1685 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 1686 | `		return PH7_OK;` |
|      - | 1687 | `	}` |
|  13750 | 1688 | `	ph7_result_bool(pCtx,0);` |
|      - | 1689 | `	/* Perform the requested operation */` |
|  13750 | 1690 | `	rc = pStream->xReadDir(pDev->pHandle,pCtx);` |
|  13750 | 1691 | `	if( rc != PH7_OK ){` |
|      - | 1692 | `		/* Return FALSE */` |
|   1301 | 1693 | `		ph7_result_bool(pCtx,0);` |
|    647 | 1694 | `	}` |
|  13750 | 1695 | `	return PH7_OK;` |
|   6873 | 1696 | `}` |
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
|   1306 | 1750 | `PH7_PRIVATE int PH7_builtin_closedir(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      5 | 1751 | `{` |
|      - | 1752 | `	const ph7_io_stream *pStream;` |
|      - | 1753 | `	io_private *pDev;` |
|   1311 | 1754 | `	if( nArg < 1 \|\| !ph7_value_is_resource(apArg[0]) ){` |
|      - | 1755 | `		/* Missing/Invalid arguments,return FALSE */` |
|    ! 0 | 1756 | `		ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"Expecting an IO handle");` |
|    ! 0 | 1757 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 1758 | `		return PH7_OK;` |
|      - | 1759 | `	}` |
|      - | 1760 | `	/* Extract our private data */` |
|   1311 | 1761 | `	pDev = (io_private *)ph7_value_to_resource(apArg[0]);` |
|      - | 1762 | `	/* Make sure we are dealing with a valid io_private instance */` |
|   1311 | 1763 | `	if( IO_PRIVATE_INVALID(pDev) ){` |
|      - | 1764 | `		/*Expecting an IO handle */` |
|    ! 0 | 1765 | `		ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"Expecting an IO handle");` |
|    ! 0 | 1766 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 1767 | `		return PH7_OK;` |
|      - | 1768 | `	}` |
|      - | 1769 | `	/* Point to the target IO stream device */` |
|   1311 | 1770 | `	pStream = pDev->pStream;` |
|   1311 | 1771 | `	if( pStream == 0  \|\| pStream->xCloseDir == 0 ){` |
|    ! 0 | 1772 | `		ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,` |
|      - | 1773 | `			"IO routine(%s) not implemented in the underlying stream(%s) device,PH7 is returning FALSE",` |
|    ! 0 | 1774 | `			ph7_function_name(pCtx),pStream ? pStream->zName : "null_stream"` |
|      - | 1775 | `			);` |
|    ! 0 | 1776 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 1777 | `		return PH7_OK;` |
|      - | 1778 | `	}` |
|      - | 1779 | `	/* Perform the requested operation */` |
|   1311 | 1780 | `	PH7_StreamFilterReleaseChains(pDev);` |
|   1311 | 1781 | `	pStream->xCloseDir(pDev->pHandle);` |
|      - | 1782 | `	/* Keep the handle alive but flag it closed (php: gettype()=='resource (closed)') */` |
|   1311 | 1783 | `	MarkIOPrivateClosed(pDev);` |
|   1311 | 1784 | `	return PH7_OK;` |
|    657 | 1785 | ` }` |
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
|   1336 | 1797 | `PH7_PRIVATE int PH7_builtin_opendir(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      5 | 1798 | `{` |
|      - | 1799 | `	const ph7_io_stream *pStream;` |
|      - | 1800 | `	const char *zPath;` |
|      - | 1801 | `	io_private *pDev;` |
|   1341 | 1802 | `	int iLen,rc,bThrew = 0;` |
|   1341 | 1803 | `	if( nArg < 1 \|\| !ph7_value_is_string(apArg[0]) ){` |
|      - | 1804 | `		/* Missing/Invalid arguments,return FALSE */` |
|    ! 0 | 1805 | `		ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"Expecting a directory path");` |
|    ! 0 | 1806 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 1807 | `		return PH7_OK;` |
|      - | 1808 | `	}` |
|      - | 1809 | `	/* php refuses a resource that is not a stream-context. Nothing CONSUMES it` |
|      - | 1810 | `	 * here — dir_opendir() over a userland wrapper is not dispatched (§7.4` |
|      - | 1811 | `	 * slice-2 (e)) — but the refusal is the argument's contract. */` |
|   1341 | 1812 | `	PH7_StreamCtxFromArg(pCtx,nArg,apArg,1,"$context",0,&bThrew);` |
|   1341 | 1813 | `	if( bThrew ){` |
|      3 | 1814 | `		return PH7_OK;` |
|      - | 1815 | `	}` |
|      - | 1816 | `	/* Extract the target path */` |
|   1339 | 1817 | `	zPath  = ph7_value_to_string(apArg[0],&iLen);` |
|      - | 1818 | `	/* Try to extract a stream */` |
|   1339 | 1819 | `	pStream = PH7_VmGetStreamDevice(pCtx->pVm,&zPath,iLen);` |
|   1339 | 1820 | `	if( pStream == 0 ){` |
|    ! 0 | 1821 | `		VfsThrowNoDeviceWarning(pCtx,zPath,TRUE);` |
|    ! 0 | 1822 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 1823 | `		return PH7_OK;` |
|      - | 1824 | `	}` |
|   1339 | 1825 | `	if( pStream->xOpenDir == 0 ){` |
|    ! 0 | 1826 | `		ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,` |
|      - | 1827 | `			"IO routine(%s) not implemented in the underlying stream(%s) device",` |
|    ! 0 | 1828 | `			ph7_function_name(pCtx),pStream->zName` |
|      - | 1829 | `			);` |
|    ! 0 | 1830 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 1831 | `		return PH7_OK;` |
|      - | 1832 | `	}` |
|      - | 1833 | `	/* Allocate a new IO private instance */` |
|   1339 | 1834 | `	pDev = (io_private *)ph7_context_alloc_chunk(pCtx,sizeof(io_private),TRUE,FALSE);` |
|   1339 | 1835 | `	if( pDev == 0 ){` |
|    ! 0 | 1836 | `		ph7_context_throw_error(pCtx,PH7_CTX_ERR,"PH7 is running out of memory");` |
|    ! 0 | 1837 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 1838 | `		return PH7_OK;` |
|      - | 1839 | `	}` |
|      - | 1840 | `	/* Initialize the structure */` |
|   1339 | 1841 | `	InitIOPrivate(pCtx->pVm,pStream,pDev);` |
|      - | 1842 | `	/* Open the target directory */` |
|   1339 | 1843 | `	rc = pStream->xOpenDir(zPath,nArg > 1 ? apArg[1] : 0,&pDev->pHandle);` |
|   1339 | 1844 | `	if( rc != PH7_OK ){` |
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
|   1311 | 1870 | `		SetIOPrivateOpenedAs(pDev,0,0,"r",1);` |
|   1311 | 1871 | `		pDev->bDir = 1;` |
|      - | 1872 | `		/* Return the handle as a resource */` |
|   1311 | 1873 | `		ph7_result_resource(pCtx,pDev);` |
|      - | 1874 | `	}` |
|   1339 | 1875 | `	return PH7_OK;` |
|    672 | 1876 | `}` |
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
|   8678 | 2025 | `PH7_PRIVATE int PH7_builtin_file_get_contents(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      5 | 2026 | `{` |
|      - | 2027 | `	const ph7_io_stream *pStream;` |
|      - | 2028 | `	ph7_int64 n,nRead,nMaxlen;` |
|   8683 | 2029 | `	int use_include  = FALSE;` |
|      - | 2030 | `	const char *zFile;` |
|      - | 2031 | `	char zBuf[8192];` |
|      - | 2032 | `	void *pHandle;` |
|      - | 2033 | `	phl_stream_ctx *pCtxRes;` |
|   8683 | 2034 | `	int nLen,bThrew = 0;` |
|      - | 2035 |  |
|   8683 | 2036 | `	if( nArg < 1 \|\| !ph7_value_is_string(apArg[0]) ){` |
|      - | 2037 | `		/* Missing/Invalid arguments,return FALSE */` |
|    ! 0 | 2038 | `		ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"Expecting a file path");` |
|    ! 0 | 2039 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 2040 | `		return PH7_OK;` |
|      - | 2041 | `	}` |
|      - | 2042 | `	/* Extract the file path */` |
|   8683 | 2043 | `	zFile = ph7_value_to_string(apArg[0],&nLen);` |
|      - | 2044 | `	/* PHP 8 validates $length (arg #5) up front, before the wrapper is resolved` |
|      - | 2045 | `	 * or the file opened, so a negative length raises its catchable ValueError` |
|      - | 2046 | `	 * even for a bad wrapper or a missing file; a NULL (the ?int default) reads` |
|      - | 2047 | `	 * the whole file. */` |
|   8683 | 2048 | `	if( nArg > 4 && !ph7_value_is_null(apArg[4]) ){` |
|     27 | 2049 | `		if( ph7_value_to_int64(apArg[4]) < 0 ){` |
|      5 | 2050 | `			return PH7_VmThrowException(pCtx,"ValueError",` |
|      - | 2051 | `				"file_get_contents(): Argument #5 ($length) must be greater than or equal to 0");` |
|      - | 2052 | `		}` |
|     11 | 2053 | `	}` |
|      - | 2054 | `	/* Point to the target IO stream device */` |
|   8679 | 2055 | `	pStream = PH7_VmGetStreamDevice(pCtx->pVm,&zFile,nLen);` |
|   8679 | 2056 | `	if( pStream == 0 ){` |
|     23 | 2057 | `		VfsThrowNoDeviceWarning(pCtx,zFile,FALSE);` |
|     23 | 2058 | `		ph7_result_bool(pCtx,0);` |
|     23 | 2059 | `		return PH7_OK;` |
|      - | 2060 | `	}` |
|   8657 | 2061 | `	nMaxlen = -1;` |
|   8657 | 2062 | `	if( nArg > 1 ){` |
|     37 | 2063 | `		use_include = ph7_value_to_bool(apArg[1]);` |
|     17 | 2064 | `	}` |
|      - | 2065 | ``	/* php's `?resource $context`: a resource that is not a stream-context is`` |
|      - | 2066 | `	 * refused, and NULL means the DEFAULT context — never "no context at all".` |
|      - | 2067 | `	 * The armed one describes exactly this open. */` |
|   8657 | 2068 | `	pCtxRes = PH7_StreamCtxFromArg(pCtx,nArg,apArg,2,"$context",0,&bThrew);` |
|   8657 | 2069 | `	if( bThrew ){` |
|      5 | 2070 | `		return PH7_OK;` |
|      - | 2071 | `	}` |
|   8653 | 2072 | `	PH7_StreamCtxArm(pCtx->pVm,pCtxRes);` |
|      - | 2073 | `	/* Try to open the file in read-only mode */` |
|   8653 | 2074 | `	pHandle = PH7_StreamOpenHandle(pCtx->pVm,pStream,zFile,PH7_IO_OPEN_RDONLY,use_include,nArg > 2 ? apArg[2] : 0,FALSE,0,ph7_function_name(pCtx));` |
|   8653 | 2075 | `	if( pHandle == 0 ){` |
|     26 | 2076 | `		VfsThrowOpenWarning(pCtx,zFile);` |
|     26 | 2077 | `		ph7_result_bool(pCtx,0);` |
|     26 | 2078 | `		return PH7_OK;` |
|      - | 2079 | `	}` |
|   8631 | 2080 | `	if( nArg > 3 ){` |
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
|   8631 | 2099 | `	nRead = 0;` |
|  17235 | 2100 | `	while( nMaxlen != 0 ){` |
|      - | 2101 | `		/* Cap the chunk to the bytes STILL wanted (nMaxlen - nRead), not to the` |
|      - | 2102 | `		 * whole nMaxlen: with a limit above the buffer size the final chunk would` |
|      - | 2103 | `		 * otherwise overshoot and append past $length. */` |
|  17231 | 2104 | `		ph7_int64 nAsk = (ph7_int64)sizeof(zBuf);` |
|  17231 | 2105 | `		if( nMaxlen > 0 && (nMaxlen - nRead) < nAsk ){` |
|     17 | 2106 | `			nAsk = nMaxlen - nRead;` |
|      8 | 2107 | `		}` |
|  17231 | 2108 | `		n = pStream->xRead(pHandle,zBuf,nAsk);` |
|  17231 | 2109 | `		if( n < 1 ){` |
|      - | 2110 | `			/* EOF or IO error,break immediately */` |
|   8613 | 2111 | `			break;` |
|      - | 2112 | `		}` |
|      - | 2113 | `		/* Append data */` |
|   8623 | 2114 | `		ph7_result_string(pCtx,zBuf,(int)n);` |
|      - | 2115 | `		/* Increment read counter */` |
|   8623 | 2116 | `		nRead += n;` |
|   8623 | 2117 | `		if( nMaxlen > 0 && nRead >= nMaxlen ){` |
|      - | 2118 | `			/* Read limit reached */` |
|     15 | 2119 | `			break;` |
|      - | 2120 | `		}` |
|      5 | 2121 | `	}` |
|      - | 2122 | `	/* Close the stream */` |
|   8631 | 2123 | `	PH7_StreamCloseHandle(pStream,pHandle);` |
|      - | 2124 | `	/* A successfully opened but empty file yields "" in php (FALSE is only for an` |
|      - | 2125 | `	 * open failure, handled above); the read loop never set a string result, so` |
|      - | 2126 | `	 * force an empty string rather than leaving a null/FALSE result. */` |
|   8631 | 2127 | `	if( ph7_context_result_buf_length(pCtx) < 1 ){` |
|     95 | 2128 | `		ph7_result_string(pCtx,"",0);` |
|     45 | 2129 | `	}` |
|   8631 | 2130 | `	return PH7_OK;` |
|   4344 | 2131 | `}` |
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
|  16252 | 2187 | `PH7_PRIVATE int PH7_builtin_file_put_contents(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      5 | 2188 | `{` |
|  16257 | 2189 | `	int use_include  = FALSE;` |
|      - | 2190 | `	const ph7_io_stream *pStream;` |
|      - | 2191 | `	const char *zFile;` |
|      - | 2192 | `	const char *zData;` |
|      - | 2193 | `	int iOpenFlags;` |
|      - | 2194 | `	void *pHandle;` |
|      - | 2195 | `	phl_stream_ctx *pCtxRes;` |
|      - | 2196 | `	int iFlags;` |
|  16257 | 2197 | `	int nLen,bThrew = 0;` |
|      - | 2198 |  |
|  16257 | 2199 | `	if( nArg < 2 \|\| !ph7_value_is_string(apArg[0]) ){` |
|      - | 2200 | `		/* Missing/Invalid arguments,return FALSE */` |
|    ! 0 | 2201 | `		ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"Expecting a file path");` |
|    ! 0 | 2202 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 2203 | `		return PH7_OK;` |
|      - | 2204 | `	}` |
|      - | 2205 | `	/* Extract the file path */` |
|  16257 | 2206 | `	zFile = ph7_value_to_string(apArg[0],&nLen);` |
|      - | 2207 | `	/* Point to the target IO stream device */` |
|  16257 | 2208 | `	pStream = PH7_VmGetStreamDevice(pCtx->pVm,&zFile,nLen);` |
|  16257 | 2209 | `	if( pStream == 0 ){` |
|    ! 0 | 2210 | `		VfsThrowNoDeviceWarning(pCtx,zFile,FALSE);` |
|    ! 0 | 2211 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 2212 | `		return PH7_OK;` |
|      - | 2213 | `	}` |
|      - | 2214 | `	/* Data to write */` |
|  16257 | 2215 | `	zData = ph7_value_to_string(apArg[1],&nLen);` |
|      - | 2216 | `	/* Try to open the file in read-write mode */` |
|  16257 | 2217 | `	iOpenFlags = PH7_IO_OPEN_CREATE\|PH7_IO_OPEN_RDWR\|PH7_IO_OPEN_TRUNC;` |
|      - | 2218 | `	/* Extract the flags */` |
|  16257 | 2219 | `	iFlags = 0;` |
|  16257 | 2220 | `	if( nArg > 2 ){` |
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
|  24383 | 2236 | `	pCtxRes = PH7_StreamCtxFromArg(pCtx,nArg,apArg,3,"$context",` |
|  16252 | 2237 | `		(iFlags & 0x10) != 0,&bThrew);` |
|  16257 | 2238 | `	if( bThrew ){` |
|      6 | 2239 | `		return PH7_OK;` |
|      - | 2240 | `	}` |
|  16253 | 2241 | `	PH7_StreamCtxArm(pCtx->pVm,pCtxRes);` |
|  24377 | 2242 | `	pHandle = PH7_StreamOpenHandle(pCtx->pVm,pStream,zFile,iOpenFlags,use_include,` |
|   8124 | 2243 | `		nArg > 3 ? apArg[3] : 0,FALSE,FALSE,ph7_function_name(pCtx));` |
|  16253 | 2244 | `	if( pHandle == 0 ){` |
|      6 | 2245 | `		VfsThrowOpenWarning(pCtx,zFile);` |
|      6 | 2246 | `		ph7_result_bool(pCtx,0);` |
|      6 | 2247 | `		return PH7_OK;` |
|      - | 2248 | `	}` |
|  16249 | 2249 | `	if( nLen < 1 ){` |
|      - | 2250 | `		/* Empty data, file is created/truncated */` |
|    113 | 2251 | `		ph7_result_int64(pCtx,0);` |
|    113 | 2252 | `		PH7_StreamCloseHandle(pStream,pHandle);` |
|    113 | 2253 | `		return PH7_OK;` |
|      - | 2254 | `	}` |
|  16141 | 2255 | `	if( pStream->xWrite ){` |
|      - | 2256 | `		ph7_int64 n;` |
|  16141 | 2257 | `		if( (iFlags & 2/* LOCK_EX, php's value */) && pStream->xLock ){` |
|      - | 2258 | `			/* Try to acquire an exclusive lock */` |
|    ! 0 | 2259 | `			pStream->xLock(pHandle,1/* LOCK_EX */);` |
|    ! 0 | 2260 | `		}` |
|      - | 2261 | `		/* Perform the write operation */` |
|  16141 | 2262 | `		n = pStream->xWrite(pHandle,(const void *)zData,nLen);` |
|  16141 | 2263 | `		if( n < 0 ){` |
|      - | 2264 | `			/* IO error,return FALSE — with php's write-failure diagnostic. */` |
|      1 | 2265 | `			ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,` |
|      - | 2266 | `				"Write of %d bytes failed with errno=%d %s",` |
|    ! 0 | 2267 | `				(int)nLen,errno,VfsStrerror(errno));` |
|      1 | 2268 | `			ph7_result_bool(pCtx,0);` |
|      1 | 2269 | `		}else{` |
|      - | 2270 | `			/* Total number of bytes written */` |
|  16141 | 2271 | `			ph7_result_int64(pCtx,n);` |
|      - | 2272 | `		}` |
|   8073 | 2273 | `	}else{` |
|      - | 2274 | `		/* Read-only stream */` |
|    ! 0 | 2275 | `		ph7_context_throw_error_format(pCtx,PH7_CTX_ERR,` |
|      - | 2276 | `			"Read-only stream(%s): Cannot perform write operation",` |
|    ! 0 | 2277 | `			pStream ? pStream->zName : "null_stream"` |
|      - | 2278 | `			);` |
|    ! 0 | 2279 | `		ph7_result_bool(pCtx,0);` |
|      - | 2280 | `	}` |
|      - | 2281 | `	/* Close the handle */` |
|  16141 | 2282 | `	PH7_StreamCloseHandle(pStream,pHandle);` |
|  16141 | 2283 | `	return PH7_OK;` |
|   8131 | 2284 | `}` |
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
|      3 | 2305 | `{` |
|      - | 2306 | `	const char *zFile,*zPtr,*zEnd,*zBuf;` |
|      - | 2307 | `	ph7_value *pArray,*pLine;` |
|      - | 2308 | `	const ph7_io_stream *pStream;` |
|     53 | 2309 | `	int use_include = 0;` |
|      - | 2310 | `	io_private *pDev;` |
|      - | 2311 | `	phl_stream_ctx *pCtxRes;` |
|      - | 2312 | `	ph7_int64 n;` |
|      - | 2313 | `	int iFlags;` |
|     53 | 2314 | `	int nLen,bThrew = 0;` |
|      - | 2315 |  |
|     53 | 2316 | `	if( nArg < 1 \|\| !ph7_value_is_string(apArg[0]) ){` |
|      - | 2317 | `		/* Missing/Invalid arguments,return FALSE */` |
|    ! 0 | 2318 | `		ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"Expecting a file path");` |
|    ! 0 | 2319 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 2320 | `		return PH7_OK;` |
|      - | 2321 | `	}` |
|     53 | 2322 | `	iFlags = 0;` |
|     53 | 2323 | `	if( nArg > 1 ){` |
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
|     60 | 2345 | `	pCtxRes = PH7_StreamCtxFromArg(pCtx,nArg,apArg,2,"$context",` |
|     38 | 2346 | `		(iFlags & 0x10) != 0,&bThrew);` |
|     41 | 2347 | `	if( bThrew ){` |
|      3 | 2348 | `		return PH7_OK;` |
|      - | 2349 | `	}` |
|      - | 2350 | `	/* Extract the file path */` |
|     39 | 2351 | `	zFile = ph7_value_to_string(apArg[0],&nLen);` |
|      - | 2352 | `	/* Point to the target IO stream device */` |
|     39 | 2353 | `	pStream = PH7_VmGetStreamDevice(pCtx->pVm,&zFile,nLen);` |
|     39 | 2354 | `	if( pStream == 0 ){` |
|    ! 0 | 2355 | `		VfsThrowNoDeviceWarning(pCtx,zFile,FALSE);` |
|    ! 0 | 2356 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 2357 | `		return PH7_OK;` |
|      - | 2358 | `	}` |
|      - | 2359 | `	/* Allocate a new IO private instance */` |
|     39 | 2360 | `	pDev = (io_private *)ph7_context_alloc_chunk(pCtx,sizeof(io_private),TRUE,FALSE);` |
|     39 | 2361 | `	if( pDev == 0 ){` |
|    ! 0 | 2362 | `		ph7_context_throw_error(pCtx,PH7_CTX_ERR,"PH7 is running out of memory");` |
|    ! 0 | 2363 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 2364 | `		return PH7_OK;` |
|      - | 2365 | `	}` |
|      - | 2366 | `	/* Initialize the structure */` |
|     39 | 2367 | `	InitIOPrivate(pCtx->pVm,pStream,pDev);` |
|     39 | 2368 | `	if( iFlags & 0x01 /*FILE_USE_INCLUDE_PATH*/ ){` |
|      3 | 2369 | `		use_include = TRUE;` |
|      1 | 2370 | `	}` |
|      - | 2371 | `	/* Create the array and the working value */` |
|     39 | 2372 | `	pArray = ph7_context_new_array(pCtx);` |
|     39 | 2373 | `	pLine = ph7_context_new_scalar(pCtx);` |
|     39 | 2374 | `	if( pArray == 0 \|\| pLine == 0 ){` |
|    ! 0 | 2375 | `		ph7_context_throw_error(pCtx,PH7_CTX_ERR,"PH7 is running out of memory");` |
|    ! 0 | 2376 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 2377 | `		return PH7_OK;` |
|      - | 2378 | `	}` |
|     39 | 2379 | `	PH7_StreamCtxArm(pCtx->pVm,pCtxRes);` |
|      - | 2380 | `	/* Try to open the file in read-only mode */` |
|     39 | 2381 | `	pDev->pHandle = PH7_StreamOpenHandle(pCtx->pVm,pStream,zFile,PH7_IO_OPEN_RDONLY,use_include,nArg > 2 ? apArg[2] : 0,FALSE,0,ph7_function_name(pCtx));` |
|     39 | 2382 | `	if( pDev->pHandle == 0 ){` |
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
|     28 | 2439 | `}` |
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
|    329 | 2636 | `PH7_PRIVATE int PH7_builtin_fwrite(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      5 | 2637 | `{` |
|      - | 2638 | `	const ph7_io_stream *pStream;` |
|      - | 2639 | `	const char *zString;` |
|      - | 2640 | `	io_private *pDev;` |
|      - | 2641 | `	int nLen,n;` |
|    334 | 2642 | `	if( nArg < 2 \|\| !ph7_value_is_resource(apArg[0]) ){` |
|      - | 2643 | `		/* Missing/Invalid arguments,return FALSE */` |
|    ! 0 | 2644 | `		ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"Expecting an IO handle");` |
|    ! 0 | 2645 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 2646 | `		return PH7_OK;` |
|      - | 2647 | `	}` |
|      - | 2648 | `	/* Extract our private data */` |
|    334 | 2649 | `	pDev = (io_private *)ph7_value_to_resource(apArg[0]);` |
|      - | 2650 | `	/* Make sure we are dealing with a valid io_private instance */` |
|    334 | 2651 | `	if( IO_PRIVATE_INVALID(pDev) ){` |
|      - | 2652 | `		/* Expecting an IO handle */` |
|    ! 0 | 2653 | `		ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"Expecting an IO handle");` |
|    ! 0 | 2654 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 2655 | `		return PH7_OK;` |
|      - | 2656 | `	}` |
|      - | 2657 | `	/* Point to the target IO stream device */` |
|    334 | 2658 | `	pStream = pDev->pStream;` |
|    334 | 2659 | `	if( pStream == 0  \|\| pStream->xWrite == 0){` |
|    ! 0 | 2660 | `		ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,` |
|      - | 2661 | `			"IO routine(%s) not implemented in the underlying stream(%s) device,PH7 is returning FALSE",` |
|    ! 0 | 2662 | `			ph7_function_name(pCtx),pStream ? pStream->zName : "null_stream"` |
|      - | 2663 | `			);` |
|    ! 0 | 2664 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 2665 | `		return PH7_OK;` |
|      - | 2666 | `	}` |
|      - | 2667 | `	/* Extract the data to write */` |
|    334 | 2668 | `	zString = ph7_value_to_string(apArg[1],&nLen);` |
|    334 | 2669 | `	if( nArg > 2 && !ph7_value_is_null(apArg[2]) ){` |
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
|    334 | 2682 | `	if( nLen < 1 ){` |
|      - | 2683 | `		/* Nothing to write */` |
|      5 | 2684 | `		ph7_result_int(pCtx,0);` |
|      5 | 2685 | `		return PH7_OK;` |
|      - | 2686 | `	}` |
|      - | 2687 | `	/* The device sits PAST what the readers pulled ahead: php writes at the` |
|      - | 2688 | `	 * LOGICAL position (fgets() then fwrite() overwrites what fgets left` |
|      - | 2689 | `	 * unread) — the ftell()/SEEK_CUR rule, applied to the write. */` |
|    330 | 2690 | `	StreamSeekBackForWrite(pDev);` |
|      - | 2691 | `	/* Perform the requested operation */` |
|    330 | 2692 | `	n = (int)PH7_StreamWrite(pDev,(const void *)zString,nLen);` |
|    330 | 2693 | `	if( n <  0 ){` |
|      - | 2694 | `		/* IO error,return FALSE */` |
|      8 | 2695 | `		SockReportWriteFailure(pCtx,pDev,nLen);` |
|      8 | 2696 | `		ph7_result_bool(pCtx,0);` |
|      5 | 2697 | `	}else{` |
|      - | 2698 | `		/* #Bytes written */` |
|    324 | 2699 | `		ph7_result_int(pCtx,n);` |
|      - | 2700 | `	}` |
|    330 | 2701 | `	return PH7_OK;` |
|    166 | 2702 | `}` |
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
|    936 | 3309 | `static int StrModeToFlags(ph7_context *pCtx,const char *zMode,int nLen)` |
|      5 | 3310 | `{` |
|    941 | 3311 | `	const char *zEnd = &zMode[nLen];` |
|    941 | 3312 | `	int iFlag = 0;` |
|      - | 3313 | `	int c;` |
|    941 | 3314 | `	if( nLen < 1 ){` |
|      - | 3315 | `		/* Open in a read-only mode */` |
|    ! 0 | 3316 | `		return PH7_IO_OPEN_RDONLY;` |
|      - | 3317 | `	}` |
|    941 | 3318 | `	c = zMode[0];` |
|    941 | 3319 | `	if( c == 'r' \|\| c == 'R' ){` |
|      - | 3320 | `		/* Read-only access */` |
|    475 | 3321 | `		iFlag = PH7_IO_OPEN_RDONLY;` |
|    475 | 3322 | `		zMode++; /* Advance */` |
|    475 | 3323 | `		if( zMode < zEnd ){` |
|    195 | 3324 | `			c = zMode[0];` |
|    195 | 3325 | `			if( c == '+' \|\| c == 'w' \|\| c == 'W' ){` |
|      - | 3326 | `				/* Read+Write access */` |
|    171 | 3327 | `				iFlag = PH7_IO_OPEN_RDWR;` |
|     84 | 3328 | `			}` |
|    101 | 3329 | `		}` |
|    706 | 3330 | `	}else if( c == 'w' \|\| c == 'W' ){` |
|      - | 3331 | `		/* Overwrite mode.` |
|      - | 3332 | `		 * If the file does not exists,try to create it` |
|      - | 3333 | `		 */` |
|    241 | 3334 | `		iFlag = PH7_IO_OPEN_WRONLY\|PH7_IO_OPEN_TRUNC\|PH7_IO_OPEN_CREATE;` |
|    241 | 3335 | `		zMode++; /* Advance */` |
|    241 | 3336 | `		if( zMode < zEnd ){` |
|    180 | 3337 | `			c = zMode[0];` |
|    180 | 3338 | `			if( c == '+' \|\| c == 'r' \|\| c == 'R' ){` |
|      - | 3339 | `				/* Read+Write access */` |
|    180 | 3340 | `				iFlag &= ~PH7_IO_OPEN_WRONLY;` |
|    180 | 3341 | `				iFlag \|= PH7_IO_OPEN_RDWR;` |
|     89 | 3342 | `			}` |
|     92 | 3343 | `		}` |
|    352 | 3344 | `	}else if( c == 'a' \|\| c == 'A' ){` |
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
|    232 | 3358 | `	}else if( c == 'x' \|\| c == 'X' ){` |
|      - | 3359 | `		/* Exclusive access.` |
|      - | 3360 | `		 * If the file already exists,return immediately with a failure code.` |
|      - | 3361 | `		 * Otherwise create a new file.` |
|      - | 3362 | `		 */` |
|    231 | 3363 | `		iFlag = PH7_IO_OPEN_WRONLY\|PH7_IO_OPEN_EXCL;` |
|    231 | 3364 | `		zMode++; /* Advance */` |
|    231 | 3365 | `		if( zMode < zEnd ){` |
|    ! 0 | 3366 | `			c = zMode[0];` |
|    ! 0 | 3367 | `			if( c == '+' \|\| c == 'r' \|\| c == 'R' ){` |
|      - | 3368 | `				/* Read-Write access */` |
|    ! 0 | 3369 | `				iFlag &= ~PH7_IO_OPEN_WRONLY;` |
|    ! 0 | 3370 | `				iFlag \|= PH7_IO_OPEN_RDWR;` |
|    ! 0 | 3371 | `			}` |
|      5 | 3372 | `		}` |
|    113 | 3373 | `	}else if( c == 'c' \|\| c == 'C' ){` |
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
|   1313 | 3390 | `	while( zMode < zEnd ){` |
|    375 | 3391 | `		c = zMode[0];` |
|    375 | 3392 | `		if( c == 'b' \|\| c == 'B' ){` |
|     27 | 3393 | `			iFlag &= ~PH7_IO_OPEN_TEXT;` |
|     27 | 3394 | `			iFlag \|= PH7_IO_OPEN_BINARY;` |
|    362 | 3395 | `		}else if( c == 't' \|\| c == 'T' ){` |
|    ! 0 | 3396 | `			iFlag &= ~PH7_IO_OPEN_BINARY;` |
|    ! 0 | 3397 | `			iFlag \|= PH7_IO_OPEN_TEXT;` |
|    ! 0 | 3398 | `		}` |
|    375 | 3399 | `		zMode++;` |
|      3 | 3400 | `	}` |
|    941 | 3401 | `	return iFlag;` |
|    473 | 3402 | `}` |
|      - | 3403 | `/*` |
|      - | 3404 | ` * Initialize the IO private structure.` |
|      - | 3405 | ` */` |
|   7868 | 3406 | `PH7_PRIVATE void InitIOPrivate(ph7_vm *pVm,const ph7_io_stream *pStream,io_private *pOut)` |
|      5 | 3407 | `{` |
|   7873 | 3408 | `	pOut->pStream = pStream;` |
|   7873 | 3409 | `	SyBlobInit(&pOut->sBuffer,&pVm->sAllocator);` |
|   7873 | 3410 | `	pOut->nOfft = 0;` |
|   7873 | 3411 | `	SyBlobInit(&pOut->sUri,&pVm->sAllocator);` |
|   7873 | 3412 | `	pOut->zMode[0] = 0;` |
|   7873 | 3413 | `	pOut->bEof = 0;` |
|   7873 | 3414 | `	pOut->bDir = 0;` |
|   7873 | 3415 | `	pOut->bPersist = 0;` |
|   7873 | 3416 | `	pOut->nChunk = 8192; /* php's own default, and what stream_set_chunk_size() reports first */` |
|   7873 | 3417 | `	pOut->bNonBlock = 0;` |
|   7873 | 3418 | `	pOut->bHasTimeout = 0;` |
|   7873 | 3419 | `	pOut->bTimedOut = 0;` |
|   7873 | 3420 | `	pOut->pReadFilters = 0;` |
|   7873 | 3421 | `	pOut->pWriteFilters = 0;` |
|   7873 | 3422 | `	pOut->bFiltDone = 0;` |
|   7873 | 3423 | `	pOut->bFiltErr = 0;` |
|   7873 | 3424 | `	pOut->iFiltPos = 0;` |
|   7873 | 3425 | `	pOut->pCtxRes = 0;` |
|   7873 | 3426 | `	SyBlobInit(&pOut->sFilt,&pVm->sAllocator);` |
|   7873 | 3427 | `	pOut->nFiltOfft = 0;` |
|      - | 3428 | `	/* Set the magic number */` |
|   7873 | 3429 | `	pOut->iMagic = IO_PRIVATE_MAGIC;` |
|   7873 | 3430 | `}` |
|      - | 3431 | `/*` |
|      - | 3432 | `` * Record what the opener was asked for, for stream_get_meta_data()'s `uri` and`` |
|      - | 3433 | `` * `mode` keys. php keeps the URI exactly as written (a relative path stays`` |
|      - | 3434 | ` * relative) and the mode in a 16-byte field; passing a NULL/empty zUri leaves` |
|      - | 3435 | ` * the key out, which is how a popen() pipe reports no wrapper and no uri.` |
|      - | 3436 | ` */` |
|   7782 | 3437 | `PH7_PRIVATE void SetIOPrivateOpenedAs(io_private *pDev,const char *zUri,int nUriLen,const char *zMode,int nModeLen)` |
|      5 | 3438 | `{` |
|   7787 | 3439 | `	if( pDev == 0 ){` |
|    ! 0 | 3440 | `		return;` |
|      - | 3441 | `	}` |
|   7787 | 3442 | `	SyBlobReset(&pDev->sUri);` |
|   7787 | 3443 | `	if( zUri && nUriLen > 0 ){` |
|   1095 | 3444 | `		SyBlobAppend(&pDev->sUri,zUri,(sxu32)nUriLen);` |
|    545 | 3445 | `	}` |
|   7787 | 3446 | `	if( zMode && nModeLen > 0 ){` |
|   7787 | 3447 | `		if( nModeLen > (int)sizeof(pDev->zMode) - 1 ){` |
|    ! 0 | 3448 | `			nModeLen = (int)sizeof(pDev->zMode) - 1;` |
|    ! 0 | 3449 | `		}` |
|   7787 | 3450 | `		SyMemcpy(zMode,pDev->zMode,(sxu32)nModeLen);` |
|   7787 | 3451 | `		pDev->zMode[nModeLen] = 0;` |
|   3895 | 3452 | `	}else{` |
|    ! 0 | 3453 | `		pDev->zMode[0] = 0;` |
|      - | 3454 | `	}` |
|   3895 | 3455 | `}` |
|      - | 3456 | `/*` |
|      - | 3457 | ` * Release the IO private structure.` |
|      - | 3458 | ` */` |
|     78 | 3459 | `static void ReleaseIOPrivate(ph7_context *pCtx,io_private *pDev)` |
|      5 | 3460 | `{` |
|     83 | 3461 | `	PH7_StreamFilterReleaseChains(pDev);` |
|     83 | 3462 | `	SyBlobRelease(&pDev->sBuffer);` |
|     83 | 3463 | `	SyBlobRelease(&pDev->sFilt);` |
|     83 | 3464 | `	SyBlobRelease(&pDev->sUri);` |
|     83 | 3465 | `	pDev->iMagic = 0x2126; /* Invalid magic number so we can detetct misuse */` |
|      - | 3466 | `	/* Release the whole structure */` |
|     83 | 3467 | `	ph7_context_free_chunk(pCtx,pDev);` |
|     83 | 3468 | `}` |
|      - | 3469 | `/*` |
|      - | 3470 | ` * Release a handle shell whose open FAILED: it never reached PHP, so nothing can` |
|      - | 3471 | ` * hold a copy and the chunk goes back. For a caller outside this unit (the` |
|      - | 3472 | ` * XMLWriter URI writer builds its own handle the way fopen does).` |
|      - | 3473 | ` */` |
|     16 | 3474 | `PH7_PRIVATE void PH7_StreamReleaseUnopened(ph7_context *pCtx,io_private *pDev)` |
|      1 | 3475 | `{` |
|     17 | 3476 | `	ReleaseIOPrivate(pCtx,pDev);` |
|     17 | 3477 | `}` |
|      - | 3478 | `/*` |
|      - | 3479 | ` * Mark a user-facing IO handle as closed while keeping the io_private alive.` |
|      - | 3480 | ` * The caller has already closed the underlying OS handle; we drop the work` |
|      - | 3481 | ` * buffer and stamp the closed magic so every ph7_value that still references` |
|      - | 3482 | ` * this handle sees a "resource (closed)" (php semantics). The struct is` |
|      - | 3483 | ` * reclaimed in bulk when the VM allocator is torn down. Freeing it here — as` |
|      - | 3484 | ` * fclose/closedir/pclose used to — would dangle the caller's copy (a latent,` |
|      - | 3485 | ` * pool-masked UAF) and keep reporting the handle open.` |
|      - | 3486 | ` */` |
|   7556 | 3487 | `PH7_PRIVATE void MarkIOPrivateClosed(io_private *pDev)` |
|      5 | 3488 | `{` |
|      - | 3489 | `	/* A filter outliving its handle would keep answering is_resource() and hold` |
|      - | 3490 | `	 * a pointer to a closed device; every close path releases the chains before` |
|      - | 3491 | `	 * the device goes, and this is the backstop for one that forgets. */` |
|   7561 | 3492 | `	PH7_StreamFilterReleaseChains(pDev);` |
|   7561 | 3493 | `	SyBlobRelease(&pDev->sBuffer);` |
|   7561 | 3494 | `	SyBlobRelease(&pDev->sFilt);` |
|   7561 | 3495 | `	SyBlobRelease(&pDev->sUri);` |
|   7561 | 3496 | `	pDev->pHandle = 0;` |
|   7561 | 3497 | `	pDev->iMagic = IO_PRIVATE_CLOSED_MAGIC;` |
|   7561 | 3498 | `}` |
|      - | 3499 | `/*` |
|      - | 3500 | ` * Reset the IO private structure.` |
|      - | 3501 | ` */` |
|    382 | 3502 | `static void ResetIOPrivate(io_private *pDev)` |
|      3 | 3503 | `{` |
|    385 | 3504 | `	SyBlobReset(&pDev->sBuffer);` |
|    385 | 3505 | `	pDev->nOfft = 0;` |
|      - | 3506 | `	/* A seek moves the DEVICE, so whatever the read chain had already produced` |
|      - | 3507 | `	 * from the old position is not what the new one answers. */` |
|    385 | 3508 | `	SyBlobReset(&pDev->sFilt);` |
|    385 | 3509 | `	pDev->nFiltOfft = 0;` |
|    385 | 3510 | `	pDev->bFiltDone = 0;` |
|    385 | 3511 | `	pDev->bFiltErr = 0;` |
|    385 | 3512 | `	PH7_StreamFilterRewound(pDev);` |
|      - | 3513 | `	/* Every caller of this has just MOVED the device (a seek, a rewind, a` |
|      - | 3514 | `	 * truncate), and php clears the end-of-file flag on exactly those. */` |
|    385 | 3515 | `	pDev->bEof = 0;` |
|    385 | 3516 | `}` |
|      - | 3517 | `/* Forward declaration */` |
|      - | 3518 |  |
|      - | 3519 | `/*` |
|      - | 3520 | ` * resource fopen(string $filename,string $mode [,bool $use_include_path = false[,resource $context ]])` |
|      - | 3521 | ` *  Open a file,a URL or any other IO stream.` |
|      - | 3522 | ` * Parameters` |
|      - | 3523 | ` *  $filename` |
|      - | 3524 | ` *   If filename is of the form "scheme://...", it is assumed to be a URL and PHP will search` |
|      - | 3525 | ` *   for a protocol handler (also known as a wrapper) for that scheme. If no scheme is given` |
|      - | 3526 | ` *   then a regular file is assumed.` |
|      - | 3527 | ` *  $mode` |
|      - | 3528 | ` *   The mode parameter specifies the type of access you require to the stream` |
|      - | 3529 | ` *   See the block comment associated with the StrModeToFlags() for the supported` |
|      - | 3530 | ` *   modes.` |
|      - | 3531 | ` *  $use_include_path` |
|      - | 3532 | ` *   You can use the optional second parameter and set it to` |
|      - | 3533 | ` *   TRUE, if you want to search for the file in the include_path, too.` |
|      - | 3534 | ` *  $context` |
|      - | 3535 | ` *   A context stream resource.` |
|      - | 3536 | ` * Return` |
|      - | 3537 | ` *  File handle on success or FALSE on failure.` |
|      - | 3538 | ` */` |
|      - | 3539 | `/*` |
|      - | 3540 | ` * string\|false stream_get_contents(resource $stream, int $maxLength = -1,` |
|      - | 3541 | ` *                                  int $offset = -1)` |
|      - | 3542 | ` *  Read the remaining contents of a stream into a string.` |
|      - | 3543 | ` */` |
|    258 | 3544 | `PH7_PRIVATE int PH7_builtin_stream_get_contents(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      2 | 3545 | `{` |
|      - | 3546 | `	const ph7_io_stream *pStream;` |
|      - | 3547 | `	io_private *pDev;` |
|    260 | 3548 | `	ph7_int64 nMax = -1;` |
|      - | 3549 | `	char zBuf[4096];` |
|      - | 3550 | `	ph7_int64 nRead;` |
|    260 | 3551 | `	if( nArg < 1 \|\| !ph7_value_is_resource(apArg[0]) ){` |
|    ! 0 | 3552 | `		ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"Expecting an IO handle");` |
|    ! 0 | 3553 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 3554 | `		return PH7_OK;` |
|      - | 3555 | `	}` |
|    260 | 3556 | `	pDev = (io_private *)ph7_value_to_resource(apArg[0]);` |
|    260 | 3557 | `	if( IO_PRIVATE_INVALID(pDev) ){` |
|    ! 0 | 3558 | `		ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"Expecting an IO handle");` |
|    ! 0 | 3559 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 3560 | `		return PH7_OK;` |
|      - | 3561 | `	}` |
|    260 | 3562 | `	pStream = pDev->pStream;` |
|    260 | 3563 | `	if( pStream == 0 \|\| pStream->xRead == 0 ){` |
|    ! 0 | 3564 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 3565 | `		return PH7_OK;` |
|      - | 3566 | `	}` |
|    260 | 3567 | `	if( nArg > 1 && !ph7_value_is_null(apArg[1]) ){` |
|      - | 3568 | `		/* PHP 8 raises a catchable ValueError below -1; -1 (or the NULL` |
|      - | 3569 | `		 * default) means "read until EOF". */` |
|     17 | 3570 | `		nMax = ph7_value_to_int64(apArg[1]);` |
|     17 | 3571 | `		if( nMax < -1 ){` |
|      3 | 3572 | `			return PH7_VmThrowException(pCtx,"ValueError",` |
|      - | 3573 | `				"stream_get_contents(): Argument #2 ($length) must be greater than or equal to -1");` |
|      - | 3574 | `		}` |
|      7 | 3575 | `	}` |
|    258 | 3576 | `	if( nArg > 2 ){` |
|      9 | 3577 | `		ph7_int64 iOfft = ph7_value_to_int64(apArg[2]);` |
|      9 | 3578 | `		if( iOfft >= 0 && pStream->xSeek ){` |
|      9 | 3579 | `			if( pStream->xSeek(pDev->pHandle,iOfft,0/*SEEK_SET*/) == PH7_OK ){` |
|      - | 3580 | `				/* A seek DISCARDS what was buffered ahead — the bytes belong to` |
|      - | 3581 | `				 * the position we just left. Without this the read below served` |
|      - | 3582 | `				 * the old position's leftovers and then continued from the new` |
|      - | 3583 | `				 * one. */` |
|      9 | 3584 | `				ResetIOPrivate(pDev);` |
|      4 | 3585 | `			}` |
|      4 | 3586 | `		}` |
|      4 | 3587 | `	}` |
|    258 | 3588 | `	ph7_result_string(pCtx,"",0); /* seed an empty string result */` |
|    496 | 3589 | `	while( nMax != 0 ){` |
|    490 | 3590 | `		ph7_int64 nAsk = (ph7_int64)sizeof(zBuf);` |
|    490 | 3591 | `		if( nMax > 0 && nMax < nAsk ){` |
|      5 | 3592 | `			nAsk = nMax;` |
|      2 | 3593 | `		}` |
|      - | 3594 | `		/* Through PH7_StreamRead, not the device: this is a SCRIPT-level read,` |
|      - | 3595 | `		 * and the line readers buffer AHEAD. Reading the device directly meant` |
|      - | 3596 | ``		 * `stream_get_contents()` after any fgets()/fgetc()/stream_get_line()`` |
|      - | 3597 | `		 * skipped everything still sitting in that buffer — on a file the` |
|      - | 3598 | `		 * line reader had already drained to its end, that is the WHOLE` |
|      - | 3599 | `		 * remainder, so the everyday "read the first line, then take the rest"` |
|      - | 3600 | `		 * idiom answered "" and the position it left behind was wrong too. */` |
|    490 | 3601 | `		nRead = PH7_StreamRead(pDev,zBuf,nAsk);` |
|    490 | 3602 | `		if( nRead < 1 ){` |
|    252 | 3603 | `			if( nRead == 0 ){` |
|    250 | 3604 | `				pDev->bEof = 1;` |
|    124 | 3605 | `			}` |
|    252 | 3606 | `			break;` |
|      - | 3607 | `		}` |
|    240 | 3608 | `		ph7_result_string(pCtx,zBuf,(int)nRead); /* appends */` |
|    240 | 3609 | `		if( nMax > 0 ){` |
|      5 | 3610 | `			nMax -= nRead;` |
|      2 | 3611 | `		}` |
|      2 | 3612 | `	}` |
|    258 | 3613 | `	return PH7_OK;` |
|    131 | 3614 | `}` |
|      - | 3615 | `/*` |
|      - | 3616 | ` * array stream_get_wrappers(void) — names of the registered stream devices.` |
|      - | 3617 | ` */` |
|     18 | 3618 | `PH7_PRIVATE int PH7_builtin_stream_get_wrappers(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      3 | 3619 | `{` |
|      - | 3620 | `	ph7_value *pArr,*pV;` |
|      - | 3621 | `	ph7_io_stream **apDev;` |
|      - | 3622 | `	sxu32 n;` |
|      9 | 3623 | `	SXUNUSED(nArg);` |
|      9 | 3624 | `	SXUNUSED(apArg);` |
|     21 | 3625 | `	pArr = ph7_context_new_array(pCtx);` |
|     21 | 3626 | `	pV = ph7_context_new_scalar(pCtx);` |
|     21 | 3627 | `	if( pArr == 0 \|\| pV == 0 ){` |
|    ! 0 | 3628 | `		ph7_result_null(pCtx);` |
|    ! 0 | 3629 | `		return PH7_OK;` |
|      - | 3630 | `	}` |
|     21 | 3631 | `	apDev = (ph7_io_stream **)SySetBasePtr(&pCtx->pVm->aIOstream);` |
|    105 | 3632 | `	for( n = 0 ; n < SySetUsed(&pCtx->pVm->aIOstream) ; n++ ){` |
|      - | 3633 | `		/* A device a script has unregistered is GONE from php's list -- both a` |
|      - | 3634 | `		 * built-in it switched off and a userland wrapper it withdrew, which` |
|      - | 3635 | `		 * PHL used to keep naming here after neutering the slot behind it. */` |
|     87 | 3636 | `		if( PH7_VmStreamDeviceSuppressed(pCtx->pVm,apDev[n]) ){` |
|      9 | 3637 | `			continue;` |
|      - | 3638 | `		}` |
|     79 | 3639 | `		ph7_value_string(pV,apDev[n]->zName,-1);` |
|     79 | 3640 | `		ph7_array_add_elem(pArr,0,pV);` |
|     79 | 3641 | `		ph7_value_reset_string_cursor(pV);` |
|     41 | 3642 | `	}` |
|     21 | 3643 | `	ph7_result_value(pCtx,pArr);` |
|     21 | 3644 | `	return PH7_OK;` |
|     12 | 3645 | `}` |
|      - | 3646 | `/* The userland-wrapper pool is declared further down this file. */` |
|      - | 3647 | `static int IoPrivateIsUwrap(const ph7_io_stream *pStream);` |
|      - | 3648 | `static ph7_class_instance * IoPrivateUwrapObject(io_private *pDev);` |
|      - | 3649 | `/*` |
|      - | 3650 | ` * php names TWO things in a stream's metadata: the WRAPPER that opened it` |
|      - | 3651 | `` * (`wrapper_type`) and the ops that drive it (`stream_type`). They differ for`` |
|      - | 3652 | `` * nearly every device — an ordinary file is opened by `plainfile` and driven by`` |
|      - | 3653 | `` * `STDIO` — and PHL answered its own single device name for both, so neither`` |
|      - | 3654 | ` * key ever matched php. A stream php opens with NO wrapper (a popen()/proc_open()` |
|      - | 3655 | `` * pipe, a socket) reports no `wrapper_type` at all; *pzWrapper stays 0 for those.`` |
|      - | 3656 | ` */` |
|     94 | 3657 | `static void IoPrivateStreamLabels(io_private *pDev,const char **pzWrapper,const char **pzStream)` |
|      4 | 3658 | `{` |
|     98 | 3659 | `	const ph7_io_stream *pS = pDev->pStream;` |
|     98 | 3660 | `	*pzWrapper = 0;` |
|     98 | 3661 | `	*pzStream  = "STDIO";` |
|     98 | 3662 | `	if( pS == 0 ){` |
|    ! 0 | 3663 | `		return;` |
|      - | 3664 | `	}` |
|     98 | 3665 | `	if( pDev->bDir ){` |
|      3 | 3666 | `		*pzWrapper = "plainfile";` |
|      3 | 3667 | `		*pzStream  = "dir";` |
|      3 | 3668 | `		return;` |
|      - | 3669 | `	}` |
|     96 | 3670 | `	if( is_php_stream(pS) ){` |
|     31 | 3671 | `		*pzWrapper = "PHP";` |
|     31 | 3672 | `		if( PH7_PhpStreamKind(pDev->pHandle) == PH7_IO_STREAM_OUTPUT ){` |
|      - | 3673 | `			/* php://output is the VM's output consumer, not a descriptor. */` |
|      3 | 3674 | `			*pzStream = "Output";` |
|      3 | 3675 | `			return;` |
|      - | 3676 | `		}` |
|     29 | 3677 | `		if( PH7_PhpStreamKind(pDev->pHandle) == PH7_IO_STREAM_MEMORY ){` |
|      - | 3678 | `			/* php://memory and php://temp are ONE device here and two in php,` |
|      - | 3679 | `			 * which labels them apart; the URI is what separates them. */` |
|     21 | 3680 | `			const char *zUri = (const char *)SyBlobData(&pDev->sUri);` |
|     21 | 3681 | `			sxu32 nUri = SyBlobLength(&pDev->sUri);` |
|     30 | 3682 | `			*pzStream = ( nUri >= sizeof("php://temp")-1` |
|     18 | 3683 | `			           && SyStrnicmp(zUri,"php://temp",sizeof("php://temp")-1) == 0 )` |
|     18 | 3684 | `				? "TEMP" : "MEMORY";` |
|      9 | 3685 | `		}` |
|     29 | 3686 | `		return;` |
|      - | 3687 | `	}` |
|     67 | 3688 | `	if( is_data_stream(pS) ){` |
|     15 | 3689 | `		*pzWrapper = *pzStream = "RFC2397";` |
|     15 | 3690 | `		return;` |
|      - | 3691 | `	}` |
|     53 | 3692 | `	if( IoPrivateIsUwrap(pS) ){` |
|      7 | 3693 | `		*pzWrapper = *pzStream = "user-space";` |
|      7 | 3694 | `		return;` |
|      - | 3695 | `	}` |
|     47 | 3696 | `	if( pS->zName && SyStrncmp(pS->zName,"tcp",sizeof("tcp")) == 0 ){` |
|      - | 3697 | `		/* php names the socket ops and reports no wrapper for them — and names` |
|      - | 3698 | `		 * a socket with no transport under it (a pair) differently again. */` |
|      - | 3699 | `#ifdef PH7_ENABLE_NET` |
|     18 | 3700 | `		*pzStream = (pDev->pHandle && ((sock_private *)pDev->pHandle)->bGeneric)` |
|      - | 3701 | `			? "generic_socket" : "tcp_socket/ssl";` |
|      - | 3702 | `#else` |
|      - | 3703 | `		*pzStream = "tcp_socket/ssl";` |
|      - | 3704 | `#endif` |
|     18 | 3705 | `		return;` |
|      - | 3706 | `	}` |
|     30 | 3707 | `	if( SyBlobLength(&pDev->sUri) < 1 ){` |
|      - | 3708 | `		/* Opened from a DESCRIPTOR rather than through a wrapper — a popen()` |
|      - | 3709 | `		 * or proc_open() pipe end — which is precisely when php reports` |
|      - | 3710 | ``		 * neither a `wrapper_type` nor a `uri`. */`` |
|      3 | 3711 | `		return;` |
|      - | 3712 | `	}` |
|     27 | 3713 | `	*pzWrapper = "plainfile";` |
|     51 | 3714 | `}` |
|      - | 3715 | `/*` |
|      - | 3716 | ` * data:// carries its own metadata in php, and all of it comes back out of the` |
|      - | 3717 | ``  * URI the wrapper parsed: `data://<mediatype>[;name=value]*[;base64],<payload>` `` |
|      - | 3718 | ` * answers the media type, ONE KEY PER PARAMETER, and the base64 flag last. A` |
|      - | 3719 | `` * URI naming no media type has no `mediatype` key at all — php does not`` |
|      - | 3720 | ` * substitute the RFC's default — and a repeated parameter keeps its last value.` |
|      - | 3721 | ` */` |
|     12 | 3722 | `static void IoPrivateDataMeta(ph7_context *pCtx,io_private *pDev,ph7_value *pArr,ph7_value *pV)` |
|      1 | 3723 | `{` |
|     13 | 3724 | `	const char *zUri = (const char *)SyBlobData(&pDev->sUri);` |
|     13 | 3725 | `	sxu32 nUri = SyBlobLength(&pDev->sUri);` |
|     13 | 3726 | `	sxu32 nStart = 0,nComma,nSeg,i;` |
|     13 | 3727 | `	int bBase64 = 0,bFirst = 1;` |
|     13 | 3728 | `	if( nUri >= sizeof("data://")-1 && SyStrnicmp(zUri,"data://",sizeof("data://")-1) == 0 ){` |
|     13 | 3729 | `		nStart = sizeof("data://")-1;` |
|      6 | 3730 | `	}else if( nUri >= sizeof("data:")-1 && SyStrnicmp(zUri,"data:",sizeof("data:")-1) == 0 ){` |
|    ! 0 | 3731 | `		nStart = sizeof("data:")-1;` |
|    ! 0 | 3732 | `	}` |
|     13 | 3733 | `	nComma = nStart;` |
|    329 | 3734 | `	while( nComma < nUri && zUri[nComma] != ',' ){` |
|    317 | 3735 | `		nComma++;` |
|      1 | 3736 | `	}` |
|      - | 3737 | `	/* Walk the ';'-separated segments in front of the payload. */` |
|     23 | 3738 | `	for( nSeg = nStart ; nSeg <= nComma ; ){` |
|     23 | 3739 | `		sxu32 nEnd = nSeg;` |
|    329 | 3740 | `		while( nEnd < nComma && zUri[nEnd] != ';' ){` |
|    307 | 3741 | `			nEnd++;` |
|      1 | 3742 | `		}` |
|     23 | 3743 | `		if( bFirst ){` |
|     13 | 3744 | `			if( nEnd > nSeg ){` |
|     11 | 3745 | `				ph7_value_string(pV,&zUri[nSeg],(int)(nEnd - nSeg));` |
|     11 | 3746 | `				ph7_array_add_strkey_elem(pArr,"mediatype",pV);` |
|     11 | 3747 | `				ph7_value_reset_string_cursor(pV);` |
|      5 | 3748 | `			}` |
|     13 | 3749 | `			bFirst = 0;` |
|     17 | 3750 | `		}else if( nEnd - nSeg == sizeof("base64")-1` |
|      8 | 3751 | `		       && SyStrnicmp(&zUri[nSeg],"base64",sizeof("base64")-1) == 0 ){` |
|      5 | 3752 | `			bBase64 = 1;` |
|      3 | 3753 | `		}else{` |
|      - | 3754 | ``			/* `name=value`; php keys the array by the name, so a repeat wins. */`` |
|    167 | 3755 | `			for( i = nSeg ; i < nEnd && zUri[i] != '=' ; i++ ){}` |
|      7 | 3756 | `			if( i < nEnd && i > nSeg ){` |
|      - | 3757 | `				/* The name is keyed WHOLE — it has no length limit in the URI,` |
|      - | 3758 | `				 * and a clamped one files the value under a key no script can` |
|      - | 3759 | `				 * look up. */` |
|      7 | 3760 | `				ph7_value *pKey = ph7_context_new_scalar(pCtx);` |
|      7 | 3761 | `				if( pKey ){` |
|      7 | 3762 | `					ph7_value_string(pKey,&zUri[nSeg],(int)(i - nSeg));` |
|      7 | 3763 | `					ph7_value_string(pV,&zUri[i+1],(int)(nEnd - i - 1));` |
|      7 | 3764 | `					ph7_array_add_elem(pArr,pKey,pV);` |
|      7 | 3765 | `					ph7_value_reset_string_cursor(pV);` |
|      7 | 3766 | `					ph7_context_release_value(pCtx,pKey);` |
|      3 | 3767 | `				}` |
|      3 | 3768 | `			}` |
|      - | 3769 | `		}` |
|     23 | 3770 | `		if( nEnd >= nComma ){` |
|     13 | 3771 | `			break;` |
|      - | 3772 | `		}` |
|     11 | 3773 | `		nSeg = nEnd + 1;` |
|      1 | 3774 | `	}` |
|     13 | 3775 | `	ph7_value_bool(pV,bBase64);` |
|     13 | 3776 | `	ph7_array_add_strkey_elem(pArr,"base64",pV);` |
|     13 | 3777 | `}` |
|      - | 3778 | `/*` |
|      - | 3779 | ` * array stream_get_meta_data(resource $stream)` |
|      - | 3780 | ` *` |
|      - | 3781 | ` * php's own key set, in php's own order. What used to be here answered a` |
|      - | 3782 | `` * best-effort shape: `mode` and `uri` did not exist at all (so the documented`` |
|      - | 3783 | `` * way to ask a handle what FILE it is on was an `Undefined array key` and`` |
|      - | 3784 | `` * NULL), `eof` was hardcoded FALSE (a `while (!$m['eof'])` loop never ended),`` |
|      - | 3785 | `` * `unread_bytes` was hardcoded 0, and `wrapper_type`/`stream_type` were both`` |
|      - | 3786 | ` * PHL's internal device name rather than php's two different labels.` |
|      - | 3787 | ` */` |
|     82 | 3788 | `PH7_PRIVATE int PH7_builtin_stream_get_meta_data(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      4 | 3789 | `{` |
|      - | 3790 | `	const char *zWrapper,*zStream;` |
|      - | 3791 | `	io_private *pDev;` |
|      - | 3792 | `	ph7_value *pArr,*pV;` |
|      - | 3793 | `	sxu32 nUnread;` |
|     86 | 3794 | `	if( nArg < 1 \|\| !ph7_value_is_resource(apArg[0]) ){` |
|    ! 0 | 3795 | `		ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"Expecting an IO handle");` |
|    ! 0 | 3796 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 3797 | `		return PH7_OK;` |
|      - | 3798 | `	}` |
|     86 | 3799 | `	pDev = (io_private *)ph7_value_to_resource(apArg[0]);` |
|     86 | 3800 | `	if( IO_PRIVATE_INVALID(pDev) ){` |
|    ! 0 | 3801 | `		ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"Expecting an IO handle");` |
|    ! 0 | 3802 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 3803 | `		return PH7_OK;` |
|      - | 3804 | `	}` |
|     86 | 3805 | `	pArr = ph7_context_new_array(pCtx);` |
|     86 | 3806 | `	pV = ph7_context_new_scalar(pCtx);` |
|     86 | 3807 | `	if( pArr == 0 \|\| pV == 0 ){` |
|    ! 0 | 3808 | `		ph7_result_null(pCtx);` |
|    ! 0 | 3809 | `		return PH7_OK;` |
|      - | 3810 | `	}` |
|     86 | 3811 | `	IoPrivateStreamLabels(pDev,&zWrapper,&zStream);` |
|      - | 3812 | `	/* Sample this BEFORE the eof probe below: php answers eof from state it` |
|      - | 3813 | ``	 * already has and never reads ahead for it, so its `unread_bytes` counts`` |
|      - | 3814 | `	 * only what the SCRIPT's own reads left buffered. */` |
|     86 | 3815 | `	nUnread = StreamAheadBytes(pDev);` |
|     86 | 3816 | `	if( is_data_stream(pDev->pStream) ){` |
|      - | 3817 | `		/* A device that answers metadata of its OWN replaces php's three` |
|      - | 3818 | `		 * defaults rather than adding to them: data:// (and php://temp, which` |
|      - | 3819 | `		 * simply has none) report no timed_out/blocked/eof at all. */` |
|     13 | 3820 | `		IoPrivateDataMeta(pCtx,pDev,pArr,pV);` |
|     13 | 3821 | `		ph7_value_reset_string_cursor(pV);` |
|     77 | 3822 | `	}else if( is_php_stream(pDev->pStream)` |
|     46 | 3823 | `	       && PH7_PhpStreamKind(pDev->pHandle) == PH7_IO_STREAM_MEMORY` |
|     22 | 3824 | `	       && SyStrncmp(zStream,"TEMP",sizeof("TEMP")) == 0 ){` |
|      - | 3825 | `		/* php://temp: same rule, no keys of its own. */` |
|      2 | 3826 | `	}else{` |
|     72 | 3827 | `		ph7_value_bool(pV,pDev->bTimedOut != 0);` |
|     72 | 3828 | `		ph7_array_add_strkey_elem(pArr,"timed_out",pV);` |
|      - | 3829 | `		/* A stream php cannot put in non-blocking mode always reports blocked;` |
|      - | 3830 | `		 * bNonBlock is only ever set for one that CAN. */` |
|     72 | 3831 | `		ph7_value_bool(pV,pDev->bNonBlock == 0);` |
|     72 | 3832 | `		ph7_array_add_strkey_elem(pArr,"blocked",pV);` |
|      - | 3833 | `		/* The read-ahead this performs is feof()'s own, so a script that asks` |
|      - | 3834 | `		 * for the metadata and then reads sees every byte. */` |
|     72 | 3835 | `		ph7_value_bool(pV,IoPrivateAtEof(pDev) != 0);` |
|     72 | 3836 | `		ph7_array_add_strkey_elem(pArr,"eof",pV);` |
|      - | 3837 | `	}` |
|      - | 3838 | `	{` |
|     86 | 3839 | `		ph7_class_instance *pObj = IoPrivateUwrapObject(pDev);` |
|     86 | 3840 | `		if( pObj ){` |
|      - | 3841 | `			/* php hands the wrapper INSTANCE back, which is the only way a` |
|      - | 3842 | `			 * script can reach the object serving an open userland stream. */` |
|      - | 3843 | `			ph7_value sObj;` |
|      5 | 3844 | `			PH7_MemObjInit(pCtx->pVm,&sObj);` |
|      5 | 3845 | `			sObj.x.pOther = pObj;` |
|      5 | 3846 | `			sObj.iFlags = MEMOBJ_OBJ;` |
|      5 | 3847 | `			ph7_array_add_strkey_elem(pArr,"wrapper_data",&sObj);` |
|      2 | 3848 | `		}` |
|      - | 3849 | `	}` |
|     86 | 3850 | `	if( zWrapper ){` |
|     67 | 3851 | `		ph7_value_string(pV,zWrapper,-1);` |
|     67 | 3852 | `		ph7_array_add_strkey_elem(pArr,"wrapper_type",pV);` |
|     67 | 3853 | `		ph7_value_reset_string_cursor(pV);` |
|     32 | 3854 | `	}` |
|     86 | 3855 | `	ph7_value_string(pV,zStream,-1);` |
|     86 | 3856 | `	ph7_array_add_strkey_elem(pArr,"stream_type",pV);` |
|     86 | 3857 | `	ph7_value_reset_string_cursor(pV);` |
|     86 | 3858 | `	ph7_value_string(pV,pDev->zMode,-1);` |
|     86 | 3859 | `	ph7_array_add_strkey_elem(pArr,"mode",pV);` |
|     86 | 3860 | `	ph7_value_reset_string_cursor(pV);` |
|      - | 3861 | `	/* Bytes already pulled off the device and not yet handed to the script —` |
|      - | 3862 | `	 * php's own writepos-minus-readpos, which was hardcoded 0. */` |
|     86 | 3863 | `	ph7_value_int64(pV,(ph7_int64)nUnread);` |
|     86 | 3864 | `	ph7_array_add_strkey_elem(pArr,"unread_bytes",pV);` |
|      - | 3865 | `	{` |
|      - | 3866 | `		/* php answers this from what the handle actually SITS ON, not from what` |
|      - | 3867 | `		 * the device could do: php://stdout is seekable into a file and not` |
|      - | 3868 | `		 * down a pipe, php://output never is, and a pipe is not. Ask the` |
|      - | 3869 | `		 * descriptor first and the device second; a USERLAND wrapper is php's` |
|      - | 3870 | `		 * one exception — its ops always carry a seek, so php always says yes. */` |
|     86 | 3871 | `		int bSeekable = pDev->pStream != 0 && pDev->pStream->xSeek != 0;` |
|     86 | 3872 | `		if( pDev->bDir ){` |
|      - | 3873 | `			/* php's directory ops carry a rewind, so a dir handle is seekable —` |
|      - | 3874 | `			 * and asking the FILE device where it is would hand lseek() the` |
|      - | 3875 | `			 * DIR* this handle stores where a file stores its descriptor. */` |
|      3 | 3876 | `			bSeekable = 1;` |
|     85 | 3877 | `		}else if( bSeekable && !IoPrivateIsUwrap(pDev->pStream) ){` |
|     61 | 3878 | `			int rcSeek = PH7_StreamHandleCanSeek(pDev);` |
|     61 | 3879 | `			if( rcSeek >= 0 ){` |
|     31 | 3880 | `				bSeekable = rcSeek;` |
|     45 | 3881 | `			}else if( pDev->pStream->xTell != 0 ){` |
|     30 | 3882 | `				bSeekable = pDev->pStream->xTell(pDev->pHandle) >= 0;` |
|     14 | 3883 | `			}` |
|     29 | 3884 | `		}` |
|     86 | 3885 | `		ph7_value_bool(pV,bSeekable);` |
|      - | 3886 | `	}` |
|     86 | 3887 | `	ph7_array_add_strkey_elem(pArr,"seekable",pV);` |
|     86 | 3888 | `	if( SyBlobLength(&pDev->sUri) > 0 ){` |
|      - | 3889 | `		/* php keeps the path exactly as the opener received it — a relative` |
|      - | 3890 | `		 * one stays relative — and omits the key for a stream that has none. */` |
|     75 | 3891 | `		ph7_value_string(pV,(const char *)SyBlobData(&pDev->sUri),(int)SyBlobLength(&pDev->sUri));` |
|     75 | 3892 | `		ph7_array_add_strkey_elem(pArr,"uri",pV);` |
|     75 | 3893 | `		ph7_value_reset_string_cursor(pV);` |
|     36 | 3894 | `	}` |
|     86 | 3895 | `	ph7_result_value(pCtx,pArr);` |
|     86 | 3896 | `	return PH7_OK;` |
|     45 | 3897 | `}` |
|      - | 3898 | `/*` |
|      - | 3899 | ` * ---------------------------------------------------------------------------` |
|      - | 3900 | ` * Stream contexts (stream_context_create and the accessor family).` |
|      - | 3901 | ` *` |
|      - | 3902 | `` * php's context is a `stream-context` RESOURCE holding two things: a`` |
|      - | 3903 | `` * wrapper => option => value map, and the `notification` parameter. Both`` |
|      - | 3904 | ` * levels keep INSERTION order, which is the order stream_context_get_options()` |
|      - | 3905 | ` * answers in, so the store is a real nested array rather than a flat table.` |
|      - | 3906 | ` *` |
|      - | 3907 | ` * A PHL resource is a bare void*, so the struct opens with an io_private` |
|      - | 3908 | ` * header carrying its own magic (the shape proc_open()'s handle already uses)` |
|      - | 3909 | ` * and the VM owns every one it hands out.` |
|      - | 3910 | ` * ---------------------------------------------------------------------------` |
|      - | 3911 | ` */` |
|      - | 3912 | `/* Allocate one context, chained on the VM registry. */` |
|    294 | 3913 | `static phl_stream_ctx * StreamCtxNew(ph7_vm *pVm)` |
|      5 | 3914 | `{` |
|      - | 3915 | `	phl_stream_ctx *pRes;` |
|    299 | 3916 | `	if( pVm == 0 ){` |
|    ! 0 | 3917 | `		return 0;` |
|      - | 3918 | `	}` |
|    299 | 3919 | `	pRes = (phl_stream_ctx *)SyMemBackendAlloc(&pVm->sAllocator,sizeof(phl_stream_ctx));` |
|    299 | 3920 | `	if( pRes == 0 ){` |
|    ! 0 | 3921 | `		return 0;` |
|      - | 3922 | `	}` |
|    299 | 3923 | `	SyZero(pRes,sizeof(phl_stream_ctx));` |
|    299 | 3924 | `	pRes->base.iMagic = STREAM_CTX_MAGIC;` |
|    299 | 3925 | `	pRes->pVm = pVm;` |
|    299 | 3926 | `	pRes->pOptions = ph7_new_array(pVm);` |
|    299 | 3927 | `	if( pRes->pOptions == 0 ){` |
|    ! 0 | 3928 | `		SyMemBackendFree(&pVm->sAllocator,pRes);` |
|    ! 0 | 3929 | `		return 0;` |
|      - | 3930 | `	}` |
|    299 | 3931 | `	pRes->pNext = (phl_stream_ctx *)pVm->pStreamCtx;` |
|    299 | 3932 | `	pVm->pStreamCtx = (void *)pRes;` |
|    299 | 3933 | `	return pRes;` |
|    152 | 3934 | `}` |
|      - | 3935 | `/*` |
|      - | 3936 | ` * The context behind a ph7_value, or 0 when the value is not one. The magic` |
|      - | 3937 | ` * probe is the same in-bounds one every resource here answers to.` |
|      - | 3938 | ` */` |
|    134 | 3939 | `PH7_PRIVATE phl_stream_ctx * PH7_StreamCtxFromValue(ph7_value *pVal)` |
|      5 | 3940 | `{` |
|      - | 3941 | `	phl_stream_ctx *pRes;` |
|    139 | 3942 | `	if( pVal == 0 \|\| !ph7_value_is_resource(pVal) ){` |
|    ! 0 | 3943 | `		return 0;` |
|      - | 3944 | `	}` |
|    139 | 3945 | `	pRes = (phl_stream_ctx *)pVal->x.pOther;` |
|    139 | 3946 | `	if( pRes == 0 \|\| pRes->base.iMagic != STREAM_CTX_MAGIC ){` |
|     49 | 3947 | `		return 0;` |
|      - | 3948 | `	}` |
|     92 | 3949 | `	return pRes;` |
|     72 | 3950 | `}` |
|      - | 3951 | `/*` |
|      - | 3952 | ` * The per-VM DEFAULT context. php creates it on demand — the first` |
|      - | 3953 | ` * stream_context_get_default()/set_default() call — and every opener that was` |
|      - | 3954 | ` * handed no context of its own falls back to it.` |
|      - | 3955 | ` */` |
|  69726 | 3956 | `PH7_PRIVATE phl_stream_ctx * PH7_StreamCtxDefault(ph7_vm *pVm)` |
|      5 | 3957 | `{` |
|  69731 | 3958 | `	if( pVm == 0 ){` |
|    ! 0 | 3959 | `		return 0;` |
|      - | 3960 | `	}` |
|  69731 | 3961 | `	if( pVm->pDefaultCtx == 0 ){` |
|    241 | 3962 | `		pVm->pDefaultCtx = (void *)StreamCtxNew(pVm);` |
|    118 | 3963 | `	}` |
|  69731 | 3964 | `	return (phl_stream_ctx *)pVm->pDefaultCtx;` |
|  34867 | 3965 | `}` |
|      - | 3966 | `/*` |
|      - | 3967 | ` * Drop every context this VM created. Called from PH7_VmReset, so a reused VM` |
|      - | 3968 | ` * (the -S server's) does not carry one request's default context into the next.` |
|      - | 3969 | ` */` |
|     16 | 3970 | `PH7_PRIVATE void PH7_StreamCtxVmReset(ph7_vm *pVm)` |
|    ! 0 | 3971 | `{` |
|      - | 3972 | `	phl_stream_ctx *pRes;` |
|     16 | 3973 | `	if( pVm == 0 ){` |
|    ! 0 | 3974 | `		return;` |
|      - | 3975 | `	}` |
|     16 | 3976 | `	pRes = (phl_stream_ctx *)pVm->pStreamCtx;` |
|     24 | 3977 | `	while( pRes ){` |
|      8 | 3978 | `		phl_stream_ctx *pNext = pRes->pNext;` |
|      8 | 3979 | `		if( pRes->pOptions ){` |
|      8 | 3980 | `			ph7_release_value(pVm,pRes->pOptions);` |
|      4 | 3981 | `		}` |
|      8 | 3982 | `		if( pRes->pNotify ){` |
|    ! 0 | 3983 | `			ph7_release_value(pVm,pRes->pNotify);` |
|    ! 0 | 3984 | `		}` |
|      - | 3985 | `		/* Any ph7_value still naming this pointer must stop reporting a live` |
|      - | 3986 | `		 * context, so clear the magic before the memory goes back. */` |
|      8 | 3987 | `		pRes->base.iMagic = 0;` |
|      8 | 3988 | `		SyMemBackendFree(&pVm->sAllocator,pRes);` |
|      8 | 3989 | `		pRes = pNext;` |
|    ! 0 | 3990 | `	}` |
|     16 | 3991 | `	pVm->pStreamCtx = 0;` |
|     16 | 3992 | `	pVm->pDefaultCtx = 0;` |
|      - | 3993 | `	/* Whatever an interrupted open left armed named one of those. */` |
|     16 | 3994 | `	pVm->pOpenCtx = 0;` |
|      8 | 3995 | `}` |
|      - | 3996 | `/* The live element of pArray under pKey, or 0 when there is none. */` |
|    118 | 3997 | `static ph7_value * StreamCtxFetch(ph7_value *pArray,ph7_value *pKey)` |
|      4 | 3998 | `{` |
|      - | 3999 | `	ph7_hashmap_node *pNode;` |
|    122 | 4000 | `	if( pArray == 0 \|\| (pArray->iFlags & MEMOBJ_HASHMAP) == 0 ){` |
|    ! 0 | 4001 | `		return 0;` |
|      - | 4002 | `	}` |
|    122 | 4003 | `	if( PH7_HashmapLookup((ph7_hashmap *)pArray->x.pOther,pKey,&pNode) != SXRET_OK ){` |
|     56 | 4004 | `		return 0;` |
|      - | 4005 | `	}` |
|     70 | 4006 | `	return (ph7_value *)SySetAt(&pArray->pVm->aMemObj,pNode->nValIdx);` |
|     63 | 4007 | `}` |
|      - | 4008 | `/*` |
|      - | 4009 | ` * Store one option. The wrapper's sub-array is created on first use; an` |
|      - | 4010 | ` * existing one may be SHARED with the script array it was stored from, so it` |
|      - | 4011 | ` * is separated first — otherwise setting an option would write through into` |
|      - | 4012 | ` * the caller's own array.` |
|      - | 4013 | ` */` |
|     66 | 4014 | `static int StreamCtxSetOption(phl_stream_ctx *pRes,ph7_value *pWrapper,ph7_value *pName,ph7_value *pValue)` |
|      4 | 4015 | `{` |
|      - | 4016 | `	ph7_value sKey,sName,sVal;` |
|      - | 4017 | `	ph7_value *pSub;` |
|      - | 4018 | `	ph7_hashmap *pMap;` |
|     70 | 4019 | `	if( pRes == 0 \|\| pRes->pOptions == 0 \|\| pWrapper == 0 \|\| pName == 0 \|\| pValue == 0 ){` |
|    ! 0 | 4020 | `		return -1;` |
|      - | 4021 | `	}` |
|      - | 4022 | `	/* Every insertion below can reserve a memory object, which GROWS (and` |
|      - | 4023 | `	 * therefore moves) pVm->aMemObj — and all three arguments may point into` |
|      - | 4024 | `	 * it. Snapshot the structs first: a shallow copy is a safe insertion` |
|      - | 4025 | `	 * source, since the referent and the heap-resident blob survive the move. */` |
|     70 | 4026 | `	sKey = *pWrapper; pWrapper = &sKey;` |
|     70 | 4027 | `	sName = *pName;   pName = &sName;` |
|     70 | 4028 | `	sVal = *pValue;   pValue = &sVal;` |
|     70 | 4029 | `	pSub = StreamCtxFetch(pRes->pOptions,pWrapper);` |
|     70 | 4030 | `	if( pSub == 0 \|\| (pSub->iFlags & MEMOBJ_HASHMAP) == 0 ){` |
|     56 | 4031 | `		ph7_value *pFresh = ph7_new_array(pRes->pVm);` |
|     56 | 4032 | `		if( pFresh == 0 ){` |
|    ! 0 | 4033 | `			return -1;` |
|      - | 4034 | `		}` |
|     56 | 4035 | `		if( PH7_HashmapInsert((ph7_hashmap *)pRes->pOptions->x.pOther,pWrapper,pFresh) != SXRET_OK ){` |
|    ! 0 | 4036 | `			ph7_release_value(pRes->pVm,pFresh);` |
|    ! 0 | 4037 | `			return -1;` |
|      - | 4038 | `		}` |
|     56 | 4039 | `		ph7_release_value(pRes->pVm,pFresh);` |
|     56 | 4040 | `		pSub = StreamCtxFetch(pRes->pOptions,pWrapper);` |
|     56 | 4041 | `		if( pSub == 0 \|\| (pSub->iFlags & MEMOBJ_HASHMAP) == 0 ){` |
|    ! 0 | 4042 | `			return -1;` |
|      - | 4043 | `		}` |
|     26 | 4044 | `	}` |
|     70 | 4045 | `	pMap = PH7_HashmapCowSeparate(pRes->pVm,pSub);` |
|     70 | 4046 | `	if( pMap == 0 ){` |
|    ! 0 | 4047 | `		return -1;` |
|      - | 4048 | `	}` |
|     70 | 4049 | `	return PH7_HashmapInsert(pMap,pName,pValue) == SXRET_OK ? 0 : -1;` |
|     37 | 4050 | `}` |
|      - | 4051 | `/*` |
|      - | 4052 | ` * One wrapper option by name, or 0 when the context does not carry it. This is` |
|      - | 4053 | ` * the read side every consumer (the socket transports) asks through.` |
|      - | 4054 | ` */` |
|    264 | 4055 | `PH7_PRIVATE ph7_value * PH7_StreamCtxOption(phl_stream_ctx *pRes,const char *zWrapper,const char *zOption)` |
|      4 | 4056 | `{` |
|      - | 4057 | `	ph7_value *pSub;` |
|    268 | 4058 | `	if( pRes == 0 \|\| pRes->pOptions == 0 ){` |
|    ! 0 | 4059 | `		return 0;` |
|      - | 4060 | `	}` |
|    268 | 4061 | `	pSub = ph7_array_fetch(pRes->pOptions,zWrapper,-1);` |
|    268 | 4062 | `	if( pSub == 0 \|\| (pSub->iFlags & MEMOBJ_HASHMAP) == 0 ){` |
|    188 | 4063 | `		return 0;` |
|      - | 4064 | `	}` |
|     82 | 4065 | `	return ph7_array_fetch(pSub,zOption,-1);` |
|    136 | 4066 | `}` |
|      - | 4067 | `/*` |
|      - | 4068 | ` * php's parse_context_options: every entry must be wrappername => array, and a` |
|      - | 4069 | ` * non-array value — or an INTEGER key, which has no wrapper name at all — is` |
|      - | 4070 | ` * the ValueError below. An integer key one level DOWN has no option name, and` |
|      - | 4071 | ` * php drops that entry in silence rather than refusing the call.` |
|      - | 4072 | ` * Returns 0, or -1 once the exception has been raised.` |
|      - | 4073 | ` */` |
|     60 | 4074 | `static int StreamCtxParseOptions(ph7_context *pCtx,phl_stream_ctx *pRes,ph7_value *pOptions)` |
|      4 | 4075 | `{` |
|      - | 4076 | `	ph7_hashmap *pMap;` |
|      - | 4077 | `	ph7_hashmap_node *pEntry;` |
|     64 | 4078 | `	if( pOptions == 0 \|\| (pOptions->iFlags & MEMOBJ_HASHMAP) == 0 ){` |
|    ! 0 | 4079 | `		return 0;` |
|      - | 4080 | `	}` |
|     64 | 4081 | `	pMap = (ph7_hashmap *)pOptions->x.pOther;` |
|     64 | 4082 | `	pMap->pCur = pMap->pFirst;` |
|    116 | 4083 | `	while( (pEntry = PH7_HashmapGetNextEntry(pMap)) != 0 ){` |
|      - | 4084 | `		ph7_value sKey;` |
|      - | 4085 | `		ph7_value *pVal;` |
|      - | 4086 | `		int bBad;` |
|     60 | 4087 | `		PH7_MemObjInit(pRes->pVm,&sKey);` |
|     60 | 4088 | `		PH7_HashmapExtractNodeKey(pEntry,&sKey);` |
|     60 | 4089 | `		pVal = HashmapExtractNodeValue(pEntry);` |
|     86 | 4090 | `		bBad = ( (sKey.iFlags & MEMOBJ_STRING) == 0 \|\| pVal == 0` |
|     83 | 4091 | `		      \|\| (pVal->iFlags & MEMOBJ_HASHMAP) == 0 );` |
|     60 | 4092 | `		if( bBad ){` |
|      5 | 4093 | `			PH7_MemObjRelease(&sKey);` |
|      5 | 4094 | `			PH7_VmThrowException(pCtx,"ValueError",` |
|      - | 4095 | `				"Options should have the form [\"wrappername\"][\"optionname\"] = $value");` |
|      5 | 4096 | `			return -1;` |
|      - | 4097 | `		}` |
|      - | 4098 | `		{` |
|     56 | 4099 | `			ph7_hashmap *pSub = (ph7_hashmap *)pVal->x.pOther;` |
|      - | 4100 | `			ph7_hashmap_node *pOpt;` |
|     56 | 4101 | `			pSub->pCur = pSub->pFirst;` |
|    116 | 4102 | `			while( (pOpt = PH7_HashmapGetNextEntry(pSub)) != 0 ){` |
|      - | 4103 | `				ph7_value sName;` |
|      - | 4104 | `				ph7_value *pOptVal;` |
|     64 | 4105 | `				PH7_MemObjInit(pRes->pVm,&sName);` |
|     64 | 4106 | `				PH7_HashmapExtractNodeKey(pOpt,&sName);` |
|     64 | 4107 | `				pOptVal = HashmapExtractNodeValue(pOpt);` |
|     64 | 4108 | `				if( (sName.iFlags & MEMOBJ_STRING) && pOptVal ){` |
|     62 | 4109 | `					StreamCtxSetOption(pRes,&sKey,&sName,pOptVal);` |
|     29 | 4110 | `				}` |
|     64 | 4111 | `				PH7_MemObjRelease(&sName);` |
|      4 | 4112 | `			}` |
|      - | 4113 | `		}` |
|     56 | 4114 | `		PH7_MemObjRelease(&sKey);` |
|      4 | 4115 | `	}` |
|     60 | 4116 | `	return 0;` |
|     34 | 4117 | `}` |
|      - | 4118 | `/*` |
|      - | 4119 | `` * php's parse_context_params: only `notification` and `options` are read, and`` |
|      - | 4120 | ` * anything else in the array is ignored rather than refused. The notification` |
|      - | 4121 | ` * must be callable — php reports the same "must be an array with valid` |
|      - | 4122 | ` * callbacks as values" TypeError the callback taxonomy produces, naming` |
|      - | 4123 | ` * argument #1 whichever function was called.` |
|      - | 4124 | ` * Returns 0, or -1 once a diagnostic has been raised.` |
|      - | 4125 | ` */` |
|     10 | 4126 | `static int StreamCtxParseParams(ph7_context *pCtx,phl_stream_ctx *pRes,ph7_value *pParams,const char *zArgName)` |
|      1 | 4127 | `{` |
|      - | 4128 | `	ph7_value *pVal;` |
|     11 | 4129 | `	if( pParams == 0 \|\| (pParams->iFlags & MEMOBJ_HASHMAP) == 0 ){` |
|    ! 0 | 4130 | `		return 0;` |
|      - | 4131 | `	}` |
|     11 | 4132 | `	pVal = ph7_array_fetch(pParams,"notification",-1);` |
|     11 | 4133 | `	if( pVal ){` |
|      - | 4134 | `		char zBuf[128];` |
|      7 | 4135 | `		const char *zReason = PH7_VmCallableReason(pCtx->pVm,pVal,zBuf,(int)sizeof(zBuf));` |
|      7 | 4136 | `		if( zReason ){` |
|      5 | 4137 | `			return PH7_VmThrowException(pCtx,"TypeError",` |
|      - | 4138 | `				"%s(): Argument #1 (%s) must be an array with valid callbacks as values, %s",` |
|      3 | 4139 | `				ph7_function_name(pCtx),zArgName,zReason) == PH7_OK ? -1 : -1;` |
|      - | 4140 | `		}` |
|      5 | 4141 | `		if( pRes->pNotify == 0 ){` |
|      5 | 4142 | `			pRes->pNotify = ph7_new_scalar(pRes->pVm);` |
|      2 | 4143 | `		}` |
|      5 | 4144 | `		if( pRes->pNotify ){` |
|      5 | 4145 | `			PH7_MemObjStore(pVal,pRes->pNotify);` |
|      2 | 4146 | `		}` |
|      2 | 4147 | `	}` |
|      9 | 4148 | `	pVal = ph7_array_fetch(pParams,"options",-1);` |
|      9 | 4149 | `	if( pVal ){` |
|      5 | 4150 | `		if( (pVal->iFlags & MEMOBJ_HASHMAP) == 0 ){` |
|      - | 4151 | `			/* php's own wording for a params entry it cannot use. */` |
|    ! 0 | 4152 | `			return PH7_VmThrowException(pCtx,"TypeError",` |
|    ! 0 | 4153 | `				"Invalid stream/context parameter") == PH7_OK ? -1 : -1;` |
|      - | 4154 | `		}` |
|      5 | 4155 | `		if( StreamCtxParseOptions(pCtx,pRes,pVal) != 0 ){` |
|    ! 0 | 4156 | `			return -1;` |
|      - | 4157 | `		}` |
|      2 | 4158 | `	}` |
|      9 | 4159 | `	return 0;` |
|      6 | 4160 | `}` |
|      - | 4161 | `/*` |
|      - | 4162 | `` * Resolve the `$stream_or_context` first argument every accessor takes: a`` |
|      - | 4163 | ` * context resource answers itself, and a STREAM answers the context it` |
|      - | 4164 | ` * carries — created on demand for the setters, the way php's does, since a` |
|      - | 4165 | ` * stream opened without one still accepts stream_context_set_option().` |
|      - | 4166 | ` * Raises php's TypeError and returns 0 for anything else.` |
|      - | 4167 | ` */` |
|     70 | 4168 | `static phl_stream_ctx * StreamCtxArg(ph7_context *pCtx,ph7_value *pVal,int bCreate,` |
|      - | 4169 | `	const char *zArgName,int *pbThrew)` |
|      3 | 4170 | `{` |
|      - | 4171 | `	phl_stream_ctx *pRes;` |
|      - | 4172 | `	io_private *pDev;` |
|     73 | 4173 | `	*pbThrew = 1;` |
|     73 | 4174 | `	if( !ph7_value_is_resource(pVal) ){` |
|      4 | 4175 | `		PH7_VmThrowException(pCtx,"TypeError",` |
|      - | 4176 | `			"%s(): Argument #1 (%s) must be of type resource, %s given",` |
|      1 | 4177 | `			ph7_function_name(pCtx),zArgName,ph7_type_name(pVal));` |
|      3 | 4178 | `		return 0;` |
|      - | 4179 | `	}` |
|     71 | 4180 | `	pRes = PH7_StreamCtxFromValue(pVal);` |
|     71 | 4181 | `	if( pRes ){` |
|     57 | 4182 | `		*pbThrew = 0;` |
|     57 | 4183 | `		return pRes;` |
|      - | 4184 | `	}` |
|     16 | 4185 | `	pDev = (io_private *)ph7_value_to_resource(pVal);` |
|     16 | 4186 | `	if( IO_PRIVATE_INVALID(pDev) ){` |
|      - | 4187 | `		/* A closed handle, a process handle, anything that is neither: php` |
|      - | 4188 | `		 * refuses the call rather than answering an empty option set. */` |
|      4 | 4189 | `		PH7_VmThrowException(pCtx,"TypeError",` |
|      - | 4190 | `			"%s(): Argument #1 (%s) must be a valid stream/context",` |
|      1 | 4191 | `			ph7_function_name(pCtx),zArgName);` |
|      3 | 4192 | `		return 0;` |
|      - | 4193 | `	}` |
|     14 | 4194 | `	*pbThrew = 0;` |
|     14 | 4195 | `	if( pDev->pCtxRes == 0 && bCreate ){` |
|      3 | 4196 | `		pDev->pCtxRes = (void *)StreamCtxNew(pCtx->pVm);` |
|      1 | 4197 | `	}` |
|     14 | 4198 | `	return (phl_stream_ctx *)pDev->pCtxRes;` |
|     38 | 4199 | `}` |
|      - | 4200 | `/*` |
|      - | 4201 | `` * The `$context` argument sixteen rows of aBuiltinSig[] declare and no C body`` |
|      - | 4202 | `` * used to read. php's parameter is `?resource $context = null` and its rules`` |
|      - | 4203 | ` * are: a resource that is NOT a stream-context is refused outright, anything` |
|      - | 4204 | ` * else non-null is the ordinary type refusal, and NULL means the DEFAULT` |
|      - | 4205 | ` * context — which php creates on demand, so an opener never runs without one.` |
|      - | 4206 | ` *` |
|      - | 4207 | ` * bNoDefault is FILE_NO_DEFAULT_CONTEXT, the flag file()/file_get_contents()/` |
|      - | 4208 | ` * file_put_contents() carry to mean exactly "and do not fall back to it".` |
|      - | 4209 | ` * Returns 0 with *pbThrew set once a diagnostic has been raised.` |
|      - | 4210 | ` */` |
|  69778 | 4211 | `PH7_PRIVATE phl_stream_ctx * PH7_StreamCtxFromArg(ph7_context *pCtx,int nArg,ph7_value **apArg,` |
|      - | 4212 | `	int iArg,const char *zArgName,int bNoDefault,int *pbThrew)` |
|      5 | 4213 | `{` |
|      - | 4214 | `	phl_stream_ctx *pRes;` |
|  69783 | 4215 | `	*pbThrew = 0;` |
|  69783 | 4216 | `	if( iArg < nArg && apArg[iArg] && !ph7_value_is_null(apArg[iArg]) ){` |
|     76 | 4217 | `		if( !ph7_value_is_resource(apArg[iArg]) ){` |
|      7 | 4218 | `			*pbThrew = 1;` |
|     10 | 4219 | `			PH7_VmThrowException(pCtx,"TypeError",` |
|      - | 4220 | `				"%s(): Argument #%d (%s) must be of type resource or null, %s given",` |
|      6 | 4221 | `				ph7_function_name(pCtx),iArg + 1,zArgName,ph7_type_name(apArg[iArg]));` |
|      7 | 4222 | `			return 0;` |
|      - | 4223 | `		}` |
|     70 | 4224 | `		pRes = PH7_StreamCtxFromValue(apArg[iArg]);` |
|     70 | 4225 | `		if( pRes == 0 ){` |
|      - | 4226 | `			/* php names the RESOURCE it wanted rather than the argument here. */` |
|     34 | 4227 | `			*pbThrew = 1;` |
|     50 | 4228 | `			PH7_VmThrowException(pCtx,"TypeError",` |
|      - | 4229 | `				"%s(): supplied resource is not a valid Stream-Context resource",` |
|     16 | 4230 | `				ph7_function_name(pCtx));` |
|     34 | 4231 | `			return 0;` |
|      - | 4232 | `		}` |
|     37 | 4233 | `		return pRes;` |
|      - | 4234 | `	}` |
|  69711 | 4235 | `	return bNoDefault ? 0 : PH7_StreamCtxDefault(pCtx->pVm);` |
|  34893 | 4236 | `}` |
|      - | 4237 | `/*` |
|      - | 4238 | ` * Arm the context the NEXT open is to run under. PH7_StreamOpenHandle consumes` |
|      - | 4239 | ` * and clears it, so the slot describes exactly one open and a caller that never` |
|      - | 4240 | ` * set it finds nothing armed.` |
|      - | 4241 | ` */` |
|  25902 | 4242 | `PH7_PRIVATE void PH7_StreamCtxArm(ph7_vm *pVm,phl_stream_ctx *pRes)` |
|      5 | 4243 | `{` |
|  25907 | 4244 | `	if( pVm ){` |
|  25907 | 4245 | `		pVm->pOpenCtx = (void *)pRes;` |
|  12951 | 4246 | `	}` |
|  25907 | 4247 | `}` |
|      - | 4248 | `/*` |
|      - | 4249 | ` * resource stream_context_create(?array $options = null, ?array $params = null)` |
|      - | 4250 | ` */` |
|     56 | 4251 | `PH7_PRIVATE int PH7_builtin_stream_context_create(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      4 | 4252 | `{` |
|     60 | 4253 | `	phl_stream_ctx *pRes = StreamCtxNew(pCtx->pVm);` |
|     60 | 4254 | `	if( pRes == 0 ){` |
|    ! 0 | 4255 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 4256 | `		return PH7_OK;` |
|      - | 4257 | `	}` |
|     60 | 4258 | `	if( nArg > 0 && ph7_value_is_array(apArg[0]) ){` |
|     49 | 4259 | `		if( StreamCtxParseOptions(pCtx,pRes,apArg[0]) != 0 ){` |
|      5 | 4260 | `			return PH7_OK;` |
|      - | 4261 | `		}` |
|     21 | 4262 | `	}` |
|     56 | 4263 | `	if( nArg > 1 && ph7_value_is_array(apArg[1]) ){` |
|      - | 4264 | ``		/* php names argument #1 ($options) even for a bad `notification` that`` |
|      - | 4265 | `		 * arrived through $params — the error is raised against a hardcoded` |
|      - | 4266 | `		 * position, and a test that asserts the message would see it. */` |
|      9 | 4267 | `		if( StreamCtxParseParams(pCtx,pRes,apArg[1],"$options") != 0 ){` |
|      3 | 4268 | `			return PH7_OK;` |
|      - | 4269 | `		}` |
|      3 | 4270 | `	}` |
|     54 | 4271 | `	ph7_result_resource(pCtx,pRes);` |
|     54 | 4272 | `	return PH7_OK;` |
|     32 | 4273 | `}` |
|      - | 4274 | `/*` |
|      - | 4275 | ` * array stream_context_get_options(resource $stream_or_context)` |
|      - | 4276 | ` */` |
|     48 | 4277 | `PH7_PRIVATE int PH7_builtin_stream_context_get_options(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      3 | 4278 | `{` |
|      - | 4279 | `	phl_stream_ctx *pRes;` |
|      - | 4280 | `	int bThrew;` |
|     51 | 4281 | `	if( nArg < 1 ){` |
|    ! 0 | 4282 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 4283 | `		return PH7_OK;` |
|      - | 4284 | `	}` |
|      - | 4285 | `	/* A live stream that was never given a context answers the EMPTY option set` |
|      - | 4286 | `	 * rather than refusing the call, so nothing is created here. */` |
|     51 | 4287 | `	pRes = StreamCtxArg(pCtx,apArg[0],FALSE,"$stream_or_context",&bThrew);` |
|     51 | 4288 | `	if( bThrew ){` |
|      5 | 4289 | `		return PH7_OK;` |
|      - | 4290 | `	}` |
|     47 | 4291 | `	if( pRes == 0 ){` |
|      8 | 4292 | `		ph7_value *pArr = ph7_context_new_array(pCtx);` |
|      8 | 4293 | `		if( pArr == 0 ){` |
|    ! 0 | 4294 | `			ph7_result_bool(pCtx,0);` |
|    ! 0 | 4295 | `			return PH7_OK;` |
|      - | 4296 | `		}` |
|      8 | 4297 | `		ph7_result_value(pCtx,pArr);` |
|      8 | 4298 | `		return PH7_OK;` |
|      - | 4299 | `	}` |
|     41 | 4300 | `	ph7_result_value(pCtx,pRes->pOptions);` |
|     41 | 4301 | `	return PH7_OK;` |
|     27 | 4302 | `}` |
|      - | 4303 | `/*` |
|      - | 4304 | ` * bool stream_context_set_option(resource $context, string $wrapper, string $option_name, mixed $value)` |
|      - | 4305 | ` *` |
|      - | 4306 | ` * php also accepts the two-argument (context, options-array) spelling and` |
|      - | 4307 | ` * DEPRECATES it in 8.3 — §10 refuses what php deprecates, so an array in` |
|      - | 4308 | ` * argument #2 is the ordinary string TypeError here and the whole-array form` |
|      - | 4309 | ` * is spelled stream_context_set_options().` |
|      - | 4310 | ` */` |
|      8 | 4311 | `PH7_PRIVATE int PH7_builtin_stream_context_set_option(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      2 | 4312 | `{` |
|      - | 4313 | `	phl_stream_ctx *pRes;` |
|      - | 4314 | `	int bThrew;` |
|     10 | 4315 | `	if( nArg < 4 ){` |
|    ! 0 | 4316 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 4317 | `		return PH7_OK;` |
|      - | 4318 | `	}` |
|     10 | 4319 | `	pRes = StreamCtxArg(pCtx,apArg[0],TRUE,"$context",&bThrew);` |
|     10 | 4320 | `	if( pRes == 0 ){` |
|    ! 0 | 4321 | `		return PH7_OK;` |
|      - | 4322 | `	}` |
|     10 | 4323 | `	ph7_result_bool(pCtx,StreamCtxSetOption(pRes,apArg[1],apArg[2],apArg[3]) == 0);` |
|     10 | 4324 | `	return PH7_OK;` |
|      6 | 4325 | `}` |
|      - | 4326 | `/*` |
|      - | 4327 | ` * bool stream_context_set_options(resource $context, array $options)` |
|      - | 4328 | ` */` |
|      4 | 4329 | `PH7_PRIVATE int PH7_builtin_stream_context_set_options(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      2 | 4330 | `{` |
|      - | 4331 | `	phl_stream_ctx *pRes;` |
|      - | 4332 | `	int bThrew;` |
|      6 | 4333 | `	if( nArg < 2 ){` |
|    ! 0 | 4334 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 4335 | `		return PH7_OK;` |
|      - | 4336 | `	}` |
|      6 | 4337 | `	pRes = StreamCtxArg(pCtx,apArg[0],TRUE,"$context",&bThrew);` |
|      6 | 4338 | `	if( pRes == 0 ){` |
|    ! 0 | 4339 | `		return PH7_OK;` |
|      - | 4340 | `	}` |
|      6 | 4341 | `	if( StreamCtxParseOptions(pCtx,pRes,apArg[1]) != 0 ){` |
|    ! 0 | 4342 | `		return PH7_OK;` |
|      - | 4343 | `	}` |
|      6 | 4344 | `	ph7_result_bool(pCtx,1);` |
|      6 | 4345 | `	return PH7_OK;` |
|      4 | 4346 | `}` |
|      - | 4347 | `/*` |
|      - | 4348 | ` * array stream_context_get_params(resource $stream_or_context)` |
|      - | 4349 | `` *  php answers `notification` (only when one is set) and `options`, in that`` |
|      - | 4350 | ` *  order.` |
|      - | 4351 | ` */` |
|      8 | 4352 | `PH7_PRIVATE int PH7_builtin_stream_context_get_params(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 4353 | `{` |
|      - | 4354 | `	phl_stream_ctx *pRes;` |
|      - | 4355 | `	ph7_value *pArr;` |
|      - | 4356 | `	int bThrew;` |
|      9 | 4357 | `	if( nArg < 1 ){` |
|    ! 0 | 4358 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 4359 | `		return PH7_OK;` |
|      - | 4360 | `	}` |
|      9 | 4361 | `	pRes = StreamCtxArg(pCtx,apArg[0],TRUE,"$stream_or_context",&bThrew);` |
|      9 | 4362 | `	if( pRes == 0 ){` |
|    ! 0 | 4363 | `		return PH7_OK;` |
|      - | 4364 | `	}` |
|      9 | 4365 | `	pArr = ph7_context_new_array(pCtx);` |
|      9 | 4366 | `	if( pArr == 0 ){` |
|    ! 0 | 4367 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 4368 | `		return PH7_OK;` |
|      - | 4369 | `	}` |
|      9 | 4370 | `	if( pRes->pNotify ){` |
|      5 | 4371 | `		ph7_array_add_strkey_elem(pArr,"notification",pRes->pNotify);` |
|      2 | 4372 | `	}` |
|      9 | 4373 | `	ph7_array_add_strkey_elem(pArr,"options",pRes->pOptions);` |
|      9 | 4374 | `	ph7_result_value(pCtx,pArr);` |
|      9 | 4375 | `	return PH7_OK;` |
|      5 | 4376 | `}` |
|      - | 4377 | `/*` |
|      - | 4378 | ` * bool stream_context_set_params(resource $context, array $params)` |
|      - | 4379 | ` */` |
|      2 | 4380 | `PH7_PRIVATE int PH7_builtin_stream_context_set_params(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 4381 | `{` |
|      - | 4382 | `	phl_stream_ctx *pRes;` |
|      - | 4383 | `	int bThrew;` |
|      3 | 4384 | `	if( nArg < 2 ){` |
|    ! 0 | 4385 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 4386 | `		return PH7_OK;` |
|      - | 4387 | `	}` |
|      3 | 4388 | `	pRes = StreamCtxArg(pCtx,apArg[0],TRUE,"$context",&bThrew);` |
|      3 | 4389 | `	if( pRes == 0 ){` |
|    ! 0 | 4390 | `		return PH7_OK;` |
|      - | 4391 | `	}` |
|      3 | 4392 | `	if( StreamCtxParseParams(pCtx,pRes,apArg[1],"$context") != 0 ){` |
|    ! 0 | 4393 | `		return PH7_OK;` |
|      - | 4394 | `	}` |
|      3 | 4395 | `	ph7_result_bool(pCtx,1);` |
|      3 | 4396 | `	return PH7_OK;` |
|      2 | 4397 | `}` |
|      - | 4398 | `/*` |
|      - | 4399 | ` * resource stream_context_get_default(?array $options = null)` |
|      - | 4400 | ` * resource stream_context_set_default(array $options)` |
|      - | 4401 | ` *  Both answer the ONE default context and both MERGE their options into it —` |
|      - | 4402 | ` *  set_default is not a replacement, which is why a second call adds to what` |
|      - | 4403 | ` *  the first left.` |
|      - | 4404 | ` */` |
|     14 | 4405 | `static int StreamCtxDefaultCommon(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      2 | 4406 | `{` |
|     16 | 4407 | `	phl_stream_ctx *pRes = PH7_StreamCtxDefault(pCtx->pVm);` |
|     16 | 4408 | `	if( pRes == 0 ){` |
|    ! 0 | 4409 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 4410 | `		return PH7_OK;` |
|      - | 4411 | `	}` |
|     16 | 4412 | `	if( nArg > 0 && ph7_value_is_array(apArg[0]) ){` |
|      8 | 4413 | `		if( StreamCtxParseOptions(pCtx,pRes,apArg[0]) != 0 ){` |
|    ! 0 | 4414 | `			return PH7_OK;` |
|      - | 4415 | `		}` |
|      3 | 4416 | `	}` |
|     16 | 4417 | `	ph7_result_resource(pCtx,pRes);` |
|     16 | 4418 | `	return PH7_OK;` |
|      9 | 4419 | `}` |
|     10 | 4420 | `PH7_PRIVATE int PH7_builtin_stream_context_get_default(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 4421 | `{` |
|     11 | 4422 | `	return StreamCtxDefaultCommon(pCtx,nArg,apArg);` |
|      1 | 4423 | `}` |
|      4 | 4424 | `PH7_PRIVATE int PH7_builtin_stream_context_set_default(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      2 | 4425 | `{` |
|      6 | 4426 | `	return StreamCtxDefaultCommon(pCtx,nArg,apArg);` |
|      2 | 4427 | `}` |
|      - | 4428 | `/*` |
|      - | 4429 | ` * tcp:// socket stream (fsockopen / stream_socket_client). The handle is a` |
|      - | 4430 | ` * small struct carrying the OS socket plus an EOF latch, so feof() works.` |
|      - | 4431 | ` */` |
|      - | 4432 | `#ifdef PH7_ENABLE_NET` |
|     44 | 4433 | `static ph7_int64 SockStreamData_Read(void *pHandle,void *pBuffer,ph7_int64 nRead)` |
|      3 | 4434 | `{` |
|     47 | 4435 | `	sock_private *pSock = (sock_private *)pHandle;` |
|      - | 4436 | `	int n;` |
|     47 | 4437 | `	if( pSock == 0 \|\| pSock->sock == PH7_NET_INVALID_SOCKET ){` |
|      - | 4438 | `		/* A server asked for neither BIND nor LISTEN has no socket at all, and` |
|      - | 4439 | `		 * php answers false for a read on it — the shape an ERROR takes. */` |
|      6 | 4440 | `		return -1;` |
|      - | 4441 | `	}` |
|     43 | 4442 | `	if( pSock->bEof ){` |
|    ! 0 | 4443 | `		return 0;` |
|      - | 4444 | `	}` |
|     43 | 4445 | `	n = PH7_NetRecv(pSock->sock,pBuffer,(int)nRead,0);` |
|     43 | 4446 | `	if( n == 0 ){` |
|      - | 4447 | `		/* The peer closed: THIS is the end of the stream. */` |
|      9 | 4448 | `		pSock->bEof = 1;` |
|      9 | 4449 | `		return 0;` |
|      - | 4450 | `	}` |
|     35 | 4451 | `	if( n < 0 ){` |
|      - | 4452 | `		/* An error, and since stream_set_blocking()/stream_set_timeout() exist` |
|      - | 4453 | `		 * the ordinary one is EAGAIN — nothing had arrived YET. Latching EOF` |
|      - | 4454 | `		 * here (as this did for every n <= 0, safe only while every socket was` |
|      - | 4455 | `		 * blocking and untimed) made the first empty read close the connection` |
|      - | 4456 | `		 * for good and threw away everything the peer sent afterwards.` |
|      - | 4457 | `		 *` |
|      - | 4458 | `		 * The reader above tells "nothing yet" from "broken" by ERRNO, which a` |
|      - | 4459 | ``		 * Winsock call never touches: without this the `""` a non-blocking read`` |
|      - | 4460 | ``		 * answers and the `timed_out` an expired one reports were both lost on`` |
|      - | 4461 | `		 * Windows, and every such read came back as a plain failure. */` |
|      7 | 4462 | `		errno = PH7_NetWouldBlock() ? EAGAIN : (errno != 0 ? errno : EIO);` |
|      7 | 4463 | `		return -1;` |
|      - | 4464 | `	}` |
|     29 | 4465 | `	return (ph7_int64)n;` |
|     25 | 4466 | `}` |
|     55 | 4467 | `static ph7_int64 SockStreamData_Write(void *pHandle,const void *pBuf,ph7_int64 nWrite)` |
|      3 | 4468 | `{` |
|     58 | 4469 | `	sock_private *pSock = (sock_private *)pHandle;` |
|     58 | 4470 | `	const char *zBuf = (const char *)pBuf;` |
|     58 | 4471 | `	ph7_int64 nSent = 0;` |
|     58 | 4472 | `	if( pSock == 0 ){` |
|    ! 0 | 4473 | `		return -1;` |
|      - | 4474 | `	}` |
|     58 | 4475 | `	if( pSock->sock == PH7_NET_INVALID_SOCKET ){` |
|      - | 4476 | `		/* Nothing to send on, and php answers 0 rather than false for it. */` |
|      6 | 4477 | `		return 0;` |
|      - | 4478 | `	}` |
|      - | 4479 | `	/* php answers the number of bytes it MOVED. This used to hand back` |
|      - | 4480 | `	 * PH7_NetSendAll()'s STATUS — which is 0 on success — so every successful` |
|      - | 4481 | ``	 * `fwrite($sock,$s)` answered 0 bytes written: the `=== strlen($s)` check`` |
|      - | 4482 | `	 * failed on a write that worked, a partial-write retry loop never advanced,` |
|      - | 4483 | `	 * and stream_copy_to_stream() stopped after its first chunk. */` |
|    101 | 4484 | `	while( nSent < nWrite ){` |
|     56 | 4485 | `		int n = PH7_NetSend(pSock->sock,&zBuf[nSent],(int)(nWrite - nSent),0);` |
|     56 | 4486 | `		if( n > 0 ){` |
|     50 | 4487 | `			nSent += n;` |
|     50 | 4488 | `			continue;` |
|      - | 4489 | `		}` |
|      - | 4490 | `		/* Nothing more can go right now. On a non-blocking or timed-out handle` |
|      - | 4491 | `		 * that is php's 0 (or the partial count), and only a write that moved` |
|      - | 4492 | `		 * NO bytes at all for a real error is php's false — which is why the` |
|      - | 4493 | `		 * count is answered here rather than the status. */` |
|      7 | 4494 | `		if( PH7_NetWouldBlock() ){` |
|      5 | 4495 | `			return nSent;` |
|      - | 4496 | `		}` |
|      3 | 4497 | `		pSock->iLastErr = PH7_NetLastError();` |
|      3 | 4498 | `		return nSent > 0 ? nSent : -1;` |
|    ! 0 | 4499 | `	}` |
|     48 | 4500 | `	return nSent;` |
|     27 | 4501 | `}` |
|     78 | 4502 | `static void SockStreamData_Close(void *pHandle)` |
|      3 | 4503 | `{` |
|     81 | 4504 | `	sock_private *pSock = (sock_private *)pHandle;` |
|     81 | 4505 | `	if( pSock == 0 ){` |
|    ! 0 | 4506 | `		return;` |
|      - | 4507 | `	}` |
|     81 | 4508 | `	PH7_NetClose(pSock->sock);` |
|     81 | 4509 | `	SyMemBackendFree(&pSock->pVm->sAllocator,pSock);` |
|     42 | 4510 | `}` |
|      - | 4511 | `/* xOpen for "host:port" (the scheme is already stripped by the device lookup) */` |
|    ! 0 | 4512 | `static int SockStreamData_Open(const char *zName,int iMode,ph7_value *pResource,void ** ppHandle)` |
|    ! 0 | 4513 | `{` |
|      - | 4514 | `	sock_private *pSock;` |
|      - | 4515 | `	ph7_socket sock;` |
|      - | 4516 | `	char zHost[256];` |
|      - | 4517 | `	const char *zColon;` |
|    ! 0 | 4518 | `	int iPort = 0,iErrno = 0;` |
|    ! 0 | 4519 | `	const char *zErr = "";` |
|    ! 0 | 4520 | `	ph7_vm *pVm = pResource ? pResource->pVm : 0;` |
|    ! 0 | 4521 | `	SXUNUSED(iMode);` |
|    ! 0 | 4522 | `	if( pVm == 0 ){` |
|    ! 0 | 4523 | `		return -1;` |
|      - | 4524 | `	}` |
|    ! 0 | 4525 | `	zColon = SyStrlen(zName) ? &zName[SyStrlen(zName)-1] : zName;` |
|    ! 0 | 4526 | `	while( zColon > zName && zColon[0] != ':' ){` |
|    ! 0 | 4527 | `		zColon--;` |
|    ! 0 | 4528 | `	}` |
|    ! 0 | 4529 | `	if( zColon <= zName \|\| zColon[0] != ':' ){` |
|    ! 0 | 4530 | `		return -1;` |
|      - | 4531 | `	}` |
|      - | 4532 | `	{` |
|    ! 0 | 4533 | `		sxu32 n = (sxu32)(zColon - zName);` |
|    ! 0 | 4534 | `		if( n >= sizeof(zHost) ){` |
|    ! 0 | 4535 | `			n = sizeof(zHost) - 1;` |
|    ! 0 | 4536 | `		}` |
|    ! 0 | 4537 | `		SyMemcpy(zName,zHost,n);` |
|    ! 0 | 4538 | `		zHost[n] = 0;` |
|      - | 4539 | `	}` |
|      - | 4540 | `	{` |
|    ! 0 | 4541 | `		sxi32 iTmp = 0;` |
|    ! 0 | 4542 | `		SyStrToInt32(&zColon[1],(sxu32)SyStrlen(&zColon[1]),(void *)&iTmp,0);` |
|    ! 0 | 4543 | `		iPort = (int)iTmp;` |
|      - | 4544 | `	}` |
|    ! 0 | 4545 | `	sock = PH7_NetConnect(zHost,iPort,0,0,&iErrno,&zErr);` |
|    ! 0 | 4546 | `	if( sock == PH7_NET_INVALID_SOCKET ){` |
|    ! 0 | 4547 | `		return -1;` |
|      - | 4548 | `	}` |
|    ! 0 | 4549 | `	pSock = (sock_private *)SyMemBackendAlloc(&pVm->sAllocator,sizeof(sock_private));` |
|    ! 0 | 4550 | `	if( pSock == 0 ){` |
|    ! 0 | 4551 | `		PH7_NetClose(sock);` |
|    ! 0 | 4552 | `		return -1;` |
|      - | 4553 | `	}` |
|    ! 0 | 4554 | `	pSock->pVm = pVm;` |
|    ! 0 | 4555 | `	pSock->sock = sock;` |
|    ! 0 | 4556 | `	pSock->bEof = 0;` |
|    ! 0 | 4557 | `	pSock->iLastErr = 0;` |
|    ! 0 | 4558 | `	pSock->bGeneric = 0;` |
|    ! 0 | 4559 | `	*ppHandle = (void *)pSock;` |
|    ! 0 | 4560 | `	return PH7_OK;` |
|    ! 0 | 4561 | `}` |
|      - | 4562 | `/* php's own listen backlog for a stream server. */` |
|      - | 4563 | `#define SOCK_LISTEN_BACKLOG 128` |
|      - | 4564 | `/*` |
|      - | 4565 | `` * php's `fwrite(): Send of 4 bytes failed with errno=32 Broken pipe` — the`` |
|      - | 4566 | ` * NOTICE its socket ops raise for a send that failed, which is the only` |
|      - | 4567 | ` * diagnostic a write to a departed peer produces (the return value is the same` |
|      - | 4568 | ` * false a closed handle answers). Silent for every other device: nothing else` |
|      - | 4569 | ` * here has an OS error of its own to report.` |
|      - | 4570 | ` */` |
|      6 | 4571 | `static void SockReportWriteFailure(ph7_context *pCtx,io_private *pDev,int nLen)` |
|      2 | 4572 | `{` |
|      - | 4573 | `#ifdef PH7_ENABLE_NET` |
|      8 | 4574 | `	if( pDev && pDev->pStream == &sTCP_Stream && pDev->pHandle ){` |
|      3 | 4575 | `		sock_private *pSock = (sock_private *)pDev->pHandle;` |
|      3 | 4576 | `		if( pSock->iLastErr != 0 ){` |
|      4 | 4577 | `			ph7_context_throw_error_format(pCtx,PH7_CTX_NOTICE,` |
|      - | 4578 | `				"Send of %d bytes failed with errno=%d %s",` |
|      1 | 4579 | `				nLen,pSock->iLastErr,PH7_NetStrError(pSock->iLastErr));` |
|      3 | 4580 | `			pSock->iLastErr = 0;` |
|      1 | 4581 | `		}` |
|      1 | 4582 | `	}` |
|      - | 4583 | `#else` |
|      - | 4584 | `	SXUNUSED(pCtx);` |
|      - | 4585 | `	SXUNUSED(pDev);` |
|      - | 4586 | `	SXUNUSED(nLen);` |
|      - | 4587 | `#endif` |
|      8 | 4588 | `}` |
|      - | 4589 | `/* The settings family below owns both of these; the socket openers here are` |
|      - | 4590 | ` * declared ahead of it so one handle-wrapping routine can serve both halves. */` |
|      - | 4591 | `static io_private * StreamSettingArgNamed(ph7_context *pCtx,ph7_value *pArg,int iPos,` |
|      - | 4592 | `	const char *zName,int *pRc);` |
|      - | 4593 | `static ph7_socket * IoPrivateSocket(io_private *pDev);` |
|      - | 4594 | `/*` |
|      - | 4595 | ` * Wrap an open socket in the io_private every f* builtin drives, so a socket a` |
|      - | 4596 | ` * server accepted reads and writes exactly like one a client connected. A NULL` |
|      - | 4597 | ` * zUri is php's "opened by no name at all" — an accepted connection, which` |
|      - | 4598 | `` * reports no `uri` at all from stream_get_meta_data().`` |
|      - | 4599 | ` * Answers 0 (and closes the socket) when there is no memory for the handle.` |
|      - | 4600 | ` */` |
|    102 | 4601 | `static io_private * SockWrapSocket(ph7_context *pCtx,ph7_socket sock,const char *zUri,int nUri)` |
|      3 | 4602 | `{` |
|      - | 4603 | `	io_private *pDev;` |
|      - | 4604 | `	sock_private *pSock;` |
|    105 | 4605 | `	pDev = (io_private *)ph7_context_alloc_chunk(pCtx,sizeof(io_private),TRUE,FALSE);` |
|    105 | 4606 | `	pSock = (sock_private *)SyMemBackendAlloc(&pCtx->pVm->sAllocator,sizeof(sock_private));` |
|    105 | 4607 | `	if( pDev == 0 \|\| pSock == 0 ){` |
|    ! 0 | 4608 | `		if( pSock ){` |
|    ! 0 | 4609 | `			SyMemBackendFree(&pCtx->pVm->sAllocator,pSock);` |
|    ! 0 | 4610 | `		}` |
|    ! 0 | 4611 | `		if( pDev ){` |
|      - | 4612 | `			/* Allocated with AutoRelease = FALSE, so nothing else will reclaim` |
|      - | 4613 | `			 * this chunk — it is not an io_private yet and has no buffers. */` |
|    ! 0 | 4614 | `			ph7_context_free_chunk(pCtx,pDev);` |
|    ! 0 | 4615 | `		}` |
|    ! 0 | 4616 | `		PH7_NetClose(sock);` |
|    ! 0 | 4617 | `		return 0;` |
|      - | 4618 | `	}` |
|    105 | 4619 | `	pSock->pVm = pCtx->pVm;` |
|    105 | 4620 | `	pSock->sock = sock;` |
|    105 | 4621 | `	pSock->bEof = 0;` |
|    105 | 4622 | `	pSock->iLastErr = 0;` |
|    105 | 4623 | `	pSock->bGeneric = 0;` |
|    105 | 4624 | `	InitIOPrivate(pCtx->pVm,&sTCP_Stream,pDev);` |
|      - | 4625 | `	/* php's feof() answers TRUE for a stream whose socket was never created. */` |
|    105 | 4626 | `	pDev->bEof = (sxu8)(sock == PH7_NET_INVALID_SOCKET ? 1 : 0);` |
|    105 | 4627 | `	SetIOPrivateOpenedAs(pDev,zUri,nUri,"r+",2);` |
|    105 | 4628 | `	pDev->pHandle = (void *)pSock;` |
|    105 | 4629 | `	return pDev;` |
|     54 | 4630 | `}` |
|      - | 4631 | `/*` |
|      - | 4632 | ` * Undo a SockWrapSocket() whose partner could not be wrapped: the device's own` |
|      - | 4633 | ` * close hook frees the socket handle, and the io_private chunk goes with it.` |
|      - | 4634 | ` * Nothing has handed this out as a resource yet, so there is no ph7_value that` |
|      - | 4635 | ` * could observe it afterwards.` |
|      - | 4636 | ` */` |
|    ! 0 | 4637 | `static void SockCloseWrapped(ph7_context *pCtx,io_private *pDev)` |
|    ! 0 | 4638 | `{` |
|    ! 0 | 4639 | `	if( pDev == 0 ){` |
|    ! 0 | 4640 | `		return;` |
|      - | 4641 | `	}` |
|    ! 0 | 4642 | `	if( pDev->pStream && pDev->pStream->xClose && pDev->pHandle ){` |
|    ! 0 | 4643 | `		pDev->pStream->xClose(pDev->pHandle);` |
|    ! 0 | 4644 | `		pDev->pHandle = 0;` |
|    ! 0 | 4645 | `	}` |
|    ! 0 | 4646 | `	ReleaseIOPrivate(pCtx,pDev);` |
|    ! 0 | 4647 | `}` |
|      - | 4648 | `/* Forward: php's port rule, defined with the address parser further down. */` |
|      - | 4649 | `static int SockParsePort(const char *z,int n);` |
|      - | 4650 | `/*` |
|      - | 4651 | `` * php's `socket` context options, read into the shape net.c applies. Only the`` |
|      - | 4652 | `` * ones a tcp-only, IPv4-only transport can honour are read: `bindto`, which is`` |
|      - | 4653 | `` * the LOCAL address a client connects out from, `backlog`, `so_reuseport` and`` |
|      - | 4654 | ``  * `tcp_nodelay`. `so_broadcast` describes a datagram socket and `ipv6_v6only` `` |
|      - | 4655 | ` * an address family this build has not got, so they stay on the context` |
|      - | 4656 | ` * unapplied (§7.4 slice-2 (a)).` |
|      - | 4657 | ` *` |
|      - | 4658 | `` * `bindto` is "host:port", split at the FIRST colon with an atoi() port — the`` |
|      - | 4659 | ` * same address rule the server half already uses — and a spelling with no colon` |
|      - | 4660 | ` * at all is not an address, so php performs no bind and says nothing. A value` |
|      - | 4661 | ` * that is not a STRING is php's one hard failure here; everything else is a` |
|      - | 4662 | ` * warning and a connection made from wherever routing would have sent it.` |
|      - | 4663 | ` * Returns 0, or -1 with *pzErr set to php's refusal.` |
|      - | 4664 | ` */` |
|    110 | 4665 | `static int SockCtxOptions(phl_stream_ctx *pCtxRes,ph7_sockopts *pOut,char *zHostBuf,int nHostBuf,` |
|      - | 4666 | `	const char **pzErr)` |
|      4 | 4667 | `{` |
|      - | 4668 | `	ph7_value *pVal;` |
|    114 | 4669 | `	SyZero(pOut,sizeof(*pOut));` |
|    114 | 4670 | `	if( pCtxRes == 0 ){` |
|     45 | 4671 | `		return 0;` |
|      - | 4672 | `	}` |
|     70 | 4673 | `	pVal = PH7_StreamCtxOption(pCtxRes,"socket","backlog");` |
|     70 | 4674 | `	if( pVal ){` |
|      5 | 4675 | `		pOut->iBacklog = (int)ph7_value_to_int64(pVal);` |
|      2 | 4676 | `	}` |
|     70 | 4677 | `	pVal = PH7_StreamCtxOption(pCtxRes,"socket","so_reuseport");` |
|     70 | 4678 | `	pOut->bReusePort = pVal != 0 && ph7_value_to_bool(pVal);` |
|     70 | 4679 | `	pVal = PH7_StreamCtxOption(pCtxRes,"socket","tcp_nodelay");` |
|     70 | 4680 | `	pOut->bNoDelay = pVal != 0 && ph7_value_to_bool(pVal);` |
|     70 | 4681 | `	pVal = PH7_StreamCtxOption(pCtxRes,"socket","bindto");` |
|     70 | 4682 | `	if( pVal ){` |
|      - | 4683 | `		const char *zSpec;` |
|     15 | 4684 | `		int nSpec = 0,i,nHost = -1;` |
|     15 | 4685 | `		if( (pVal->iFlags & MEMOBJ_STRING) == 0 ){` |
|      3 | 4686 | `			*pzErr = "local_addr context option is not a string.";` |
|      3 | 4687 | `			return -1;` |
|      - | 4688 | `		}` |
|     13 | 4689 | `		zSpec = (const char *)SyBlobData(&pVal->sBlob);` |
|     13 | 4690 | `		nSpec = (int)SyBlobLength(&pVal->sBlob);` |
|    137 | 4691 | `		for( i = 0 ; i + 1 < nSpec ; i++ ){` |
|    135 | 4692 | `			if( zSpec[i] == ':' ){` |
|     11 | 4693 | `				nHost = i;` |
|     11 | 4694 | `				pOut->iBindPort = SockParsePort(&zSpec[i+1],nSpec - i - 1);` |
|     11 | 4695 | `				break;` |
|      - | 4696 | `			}` |
|     63 | 4697 | `		}` |
|     13 | 4698 | `		if( nHost >= 0 ){` |
|     11 | 4699 | `			if( nHost >= nHostBuf ){` |
|    ! 0 | 4700 | `				nHost = nHostBuf - 1;` |
|    ! 0 | 4701 | `			}` |
|     11 | 4702 | `			if( nHost > 0 ){` |
|     11 | 4703 | `				SyMemcpy(zSpec,zHostBuf,(sxu32)nHost);` |
|      5 | 4704 | `			}` |
|     11 | 4705 | `			zHostBuf[nHost] = 0;` |
|     11 | 4706 | `			pOut->zBindHost = zHostBuf;` |
|      5 | 4707 | `		}` |
|      6 | 4708 | `	}` |
|     68 | 4709 | `	return 0;` |
|     59 | 4710 | `}` |
|      - | 4711 | `/*` |
|      - | 4712 | ` * php's PERSISTENT sockets, which pfsockopen() and STREAM_CLIENT_PERSISTENT ask` |
|      - | 4713 | ` * for: a second open of the SAME address hands back the very same resource` |
|      - | 4714 | `` * rather than a second connection — `$a === $b` — and fclose() is what ends it,`` |
|      - | 4715 | ` * after which the next open dials again. The key is the address as the opener` |
|      - | 4716 | ` * spelled it, so "localhost:80" and "127.0.0.1:80" are two of them.` |
|      - | 4717 | ` */` |
|     26 | 4718 | `static void SockPersistKey(char *zBuf,int nBuf,int bClientForm,const char *zAddr,int nAddr)` |
|      1 | 4719 | `{` |
|      - | 4720 | `	/* php prefixes the key with the FUNCTION that asked, so a pfsockopen() and a` |
|      - | 4721 | `	 * persistent stream_socket_client() of one address are two connections. */` |
|     40 | 4722 | `	SyBufferFormat(zBuf,(sxu32)nBuf,"%s__%.*s",` |
|     13 | 4723 | `		bClientForm ? "stream_socket_client" : "pfsockopen",nAddr,zAddr);` |
|     27 | 4724 | `}` |
|     16 | 4725 | `static io_private * SockPersistFind(ph7_vm *pVm,const char *zKey)` |
|      1 | 4726 | `{` |
|     17 | 4727 | `	VmPersistSock *aSlot = (VmPersistSock *)SySetBasePtr(&pVm->aPersistSock);` |
|      - | 4728 | `	sxu32 i;` |
|     37 | 4729 | `	for( i = 0 ; i < SySetUsed(&pVm->aPersistSock) ; i++ ){` |
|     27 | 4730 | `		if( aSlot[i].zKey[0] && SyStrncmp(aSlot[i].zKey,zKey,(sxu32)SyStrlen(zKey) + 1) == 0 ){` |
|      9 | 4731 | `			if( !IO_PRIVATE_INVALID(aSlot[i].pDev) ){` |
|      7 | 4732 | `				return aSlot[i].pDev;` |
|      - | 4733 | `			}` |
|      - | 4734 | `			/* fclose()'d since: the slot is free for the next connection. */` |
|      3 | 4735 | `			aSlot[i].zKey[0] = 0;` |
|      3 | 4736 | `			aSlot[i].pDev = 0;` |
|      1 | 4737 | `		}` |
|     11 | 4738 | `	}` |
|     11 | 4739 | `	return 0;` |
|      9 | 4740 | `}` |
|     10 | 4741 | `static void SockPersistKeep(ph7_vm *pVm,const char *zKey,io_private *pDev)` |
|      1 | 4742 | `{` |
|     11 | 4743 | `	VmPersistSock *aSlot = (VmPersistSock *)SySetBasePtr(&pVm->aPersistSock);` |
|      - | 4744 | `	VmPersistSock sSlot;` |
|      - | 4745 | `	sxu32 i;` |
|     23 | 4746 | `	for( i = 0 ; i < SySetUsed(&pVm->aPersistSock) ; i++ ){` |
|     15 | 4747 | `		if( aSlot[i].zKey[0] == 0 \|\| IO_PRIVATE_INVALID(aSlot[i].pDev) ){` |
|      3 | 4748 | `			SyZero(&aSlot[i],sizeof(VmPersistSock));` |
|      3 | 4749 | `			Systrcpy(aSlot[i].zKey,(sxu32)sizeof(aSlot[i].zKey),zKey,0);` |
|      3 | 4750 | `			aSlot[i].pDev = pDev;` |
|      3 | 4751 | `			return;` |
|      - | 4752 | `		}` |
|      7 | 4753 | `	}` |
|      9 | 4754 | `	SyZero(&sSlot,sizeof(sSlot));` |
|      9 | 4755 | `	Systrcpy(sSlot.zKey,(sxu32)sizeof(sSlot.zKey),zKey,0);` |
|      9 | 4756 | `	sSlot.pDev = pDev;` |
|      9 | 4757 | `	SySetPut(&pVm->aPersistSock,(const void *)&sSlot);` |
|      6 | 4758 | `}` |
|      - | 4759 | `/*` |
|      - | 4760 | `` * php bounds every CONNECTED socket's reads by `default_socket_timeout` from the`` |
|      - | 4761 | ` * moment it is opened — a read from a peer that has gone quiet answers FALSE` |
|      - | 4762 | `` * after it, with `timed_out` set — where this engine armed nothing and waited`` |
|      - | 4763 | ` * forever. That is the difference between a program that reports a dead peer and` |
|      - | 4764 | ` * one that hangs.` |
|      - | 4765 | ` *` |
|      - | 4766 | ` * A LISTENING socket is deliberately left alone: php's accept timeout is its own` |
|      - | 4767 | ` * argument and its own select(), so arming the OS receive timeout here would` |
|      - | 4768 | `` * bound `stream_socket_accept($srv, -1)` — the wait a server asks to be`` |
|      - | 4769 | ` * unbounded — at sixty seconds.` |
|      - | 4770 | ` */` |
|     72 | 4771 | `static void SockArmDefaultTimeout(ph7_context *pCtx,io_private *pDev)` |
|      3 | 4772 | `{` |
|      - | 4773 | `	ph7_int64 iSec;` |
|     75 | 4774 | `	ph7_socket *pSock = pDev ? IoPrivateSocket(pDev) : 0;` |
|     75 | 4775 | `	if( pSock == 0 \|\| *pSock == PH7_NET_INVALID_SOCKET ){` |
|    ! 0 | 4776 | `		return;` |
|      - | 4777 | `	}` |
|     75 | 4778 | `	iSec = PH7_VmIniGetInt(pCtx->pVm,"default_socket_timeout",60);` |
|     75 | 4779 | `	if( iSec > 0 ){` |
|     75 | 4780 | `		PH7_NetSetRwTimeout(*pSock,iSec,0);` |
|     75 | 4781 | `		pDev->bHasTimeout = 1;` |
|     36 | 4782 | `	}` |
|     39 | 4783 | `}` |
|      - | 4784 | `/*` |
|      - | 4785 | ` * The out-params every address-taking opener carries, on the path that WORKED:` |
|      - | 4786 | ` * php writes 0 and "" into them rather than leaving whatever the caller's` |
|      - | 4787 | ` * variables already held.` |
|      - | 4788 | ` */` |
|     80 | 4789 | `static void SockAddressSuccess(ph7_context *pCtx,ph7_value **apArg,int nArg,int iArgErrno,int iArgErrstr)` |
|      3 | 4790 | `{` |
|     83 | 4791 | `	ph7_value *pTmp = ph7_context_new_scalar(pCtx);` |
|     83 | 4792 | `	if( pTmp == 0 ){` |
|    ! 0 | 4793 | `		return;` |
|      - | 4794 | `	}` |
|     83 | 4795 | `	if( iArgErrno >= 0 && nArg > iArgErrno ){` |
|     59 | 4796 | `		ph7_value_int(pTmp,0);` |
|     59 | 4797 | `		PH7_VmStoreArgByRef(pCtx->pVm,apArg[iArgErrno],pTmp);` |
|     28 | 4798 | `	}` |
|     83 | 4799 | `	if( iArgErrstr >= 0 && nArg > iArgErrstr ){` |
|     59 | 4800 | `		ph7_value_string(pTmp,"",0);` |
|     59 | 4801 | `		PH7_VmStoreArgByRef(pCtx->pVm,apArg[iArgErrstr],pTmp);` |
|     28 | 4802 | `	}` |
|     43 | 4803 | `}` |
|      - | 4804 | `/*` |
|      - | 4805 | ` * The failure shape the whole address-taking family shares: php words the` |
|      - | 4806 | ` * reason into BOTH the by-ref out-params and a warning naming the address as` |
|      - | 4807 | `` * the script wrote it. The `$errno` out-param stays 0 for everything the`` |
|      - | 4808 | ` * ADDRESS itself is refused for — php only ever reports an OS code for a` |
|      - | 4809 | ` * connect() that reached the network.` |
|      - | 4810 | ` */` |
|     52 | 4811 | `static void SockAddressFailure(ph7_context *pCtx,ph7_value **apArg,int nArg,int iArgErrno,int iArgErrstr,` |
|      - | 4812 | `	const char *zAddr,int nAddr,const char *zErr,int iErrno)` |
|      2 | 4813 | `{` |
|      - | 4814 | `	ph7_value *pTmp;` |
|     54 | 4815 | `	if( zErr == 0 ){` |
|    ! 0 | 4816 | `		zErr = "";` |
|    ! 0 | 4817 | `	}` |
|     54 | 4818 | `	pTmp = ph7_context_new_scalar(pCtx);` |
|     54 | 4819 | `	if( pTmp ){` |
|     54 | 4820 | `		if( iArgErrno >= 0 && nArg > iArgErrno ){` |
|     54 | 4821 | `			ph7_value_int(pTmp,iErrno);` |
|     54 | 4822 | `			PH7_VmStoreArgByRef(pCtx->pVm,apArg[iArgErrno],pTmp);` |
|     26 | 4823 | `		}` |
|     54 | 4824 | `		if( iArgErrstr >= 0 && nArg > iArgErrstr ){` |
|     54 | 4825 | `			ph7_value_string(pTmp,zErr,-1);` |
|     54 | 4826 | `			PH7_VmStoreArgByRef(pCtx->pVm,apArg[iArgErrstr],pTmp);` |
|     26 | 4827 | `		}` |
|     26 | 4828 | `	}` |
|      - | 4829 | `	/* NOTE: ph7_context_throw_error_format already prepends "fname(): " */` |
|     80 | 4830 | `	ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,"Unable to connect to %.*s (%s)",` |
|     26 | 4831 | `		nAddr,zAddr,zErr);` |
|     54 | 4832 | `}` |
|      - | 4833 | `/*` |
|      - | 4834 | ` * The one failure whose message names the HOST, and the one php reports TWICE:` |
|      - | 4835 | ` * its transport raises the text on its own before the opener that asked repeats` |
|      - | 4836 | ` * it inside "Unable to connect to". Composed here because net.c hands back a` |
|      - | 4837 | ` * static string and only the caller has the name to word in.` |
|      - | 4838 | ` */` |
|      6 | 4839 | `static const char * SockResolveFailure(ph7_context *pCtx,const char *zHost,char *zBuf,int nBuf)` |
|      1 | 4840 | `{` |
|     10 | 4841 | `	SyBufferFormat(zBuf,(sxu32)nBuf,` |
|      3 | 4842 | `		"php_network_getaddresses: getaddrinfo for %s failed: Name or service not known",zHost);` |
|      7 | 4843 | `	ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,"%s",zBuf);` |
|      7 | 4844 | `	return zBuf;` |
|      1 | 4845 | `}` |
|      - | 4846 | `PH7_PRIVATE const ph7_io_stream sTCP_Stream = {` |
|      - | 4847 | `	"tcp",` |
|      - | 4848 | `	PH7_IO_STREAM_VERSION,` |
|      - | 4849 | `	SockStreamData_Open, /* xOpen */` |
|      - | 4850 | `	0,   /* xOpenDir */` |
|      - | 4851 | `	SockStreamData_Close,/* xClose */` |
|      - | 4852 | `	0,  /* xCloseDir */` |
|      - | 4853 | `	SockStreamData_Read, /* xRead */` |
|      - | 4854 | `	0,  /* xReadDir */` |
|      - | 4855 | `	SockStreamData_Write,/* xWrite */` |
|      - | 4856 | `	0,  /* xSeek (sockets are not seekable) */` |
|      - | 4857 | `	0,  /* xLock */` |
|      - | 4858 | `	0,  /* xRewindDir */` |
|      - | 4859 | `	0,  /* xTell */` |
|      - | 4860 | `	0,  /* xTrunc */` |
|      - | 4861 | `	0,  /* xSync */` |
|      - | 4862 | `	0   /* xStat */` |
|      - | 4863 | `};` |
|      - | 4864 | `#endif /* PH7_ENABLE_NET */` |
|      - | 4865 | `/*` |
|      - | 4866 | ` * Userland stream wrappers (stream_wrapper_register). The engine's device` |
|      - | 4867 | ` * callbacks receive no device pointer, so each registered wrapper needs its` |
|      - | 4868 | ` * OWN xOpen thunk: PHL keeps a bounded pool of PHL_UWRAP_MAX slots, each with` |
|      - | 4869 | ` * a static thunk that knows its index (recorded limit; php has no cap).` |
|      - | 4870 | ` * The handle carries the userland object, and every stream op dispatches the` |
|      - | 4871 | ` * php streamWrapper protocol method on it.` |
|      - | 4872 | ` */` |
|      - | 4873 | `#define PHL_UWRAP_MAX 8` |
|      - | 4874 | `typedef struct uwrap_slot uwrap_slot;` |
|      - | 4875 | `struct uwrap_slot` |
|      - | 4876 | `{` |
|      - | 4877 | `	ph7_vm *pVm;              /* owning VM (0 = free slot) */` |
|      - | 4878 | `	char zScheme[32];         /* protocol name */` |
|      - | 4879 | `	char zClass[128];         /* userland wrapper class */` |
|      - | 4880 | `	int bIsUrl;               /* registered with STREAM_IS_URL: opening it is gated` |
|      - | 4881 | `	                           * by allow_url_fopen, INCLUDING it by` |
|      - | 4882 | `	                           * allow_url_include */` |
|      - | 4883 | `	ph7_io_stream sStream;    /* the device handed to the VM */` |
|      - | 4884 | `};` |
|      - | 4885 | `typedef struct uwrap_handle uwrap_handle;` |
|      - | 4886 | `struct uwrap_handle` |
|      - | 4887 | `{` |
|      - | 4888 | `	ph7_vm *pVm;` |
|      - | 4889 | `	ph7_class_instance *pObj; /* the wrapper instance (one per open stream) */` |
|      - | 4890 | `	int iSlot;` |
|      - | 4891 | `	int bEof;` |
|      - | 4892 | `};` |
|      - | 4893 | `static uwrap_slot g_aUwrap[PHL_UWRAP_MAX];` |
|      - | 4894 | `/*` |
|      - | 4895 | ` * Was this device registered with STREAM_IS_URL? Only a userland wrapper can` |
|      - | 4896 | ` * carry the flag, so the answer is a scan of the registration slots.` |
|      - | 4897 | ` */` |
|  36560 | 4898 | `PH7_PRIVATE int PH7_StreamIsUrlWrapper(const ph7_io_stream *pStream)` |
|      5 | 4899 | `{` |
|      - | 4900 | `	int i;` |
|      - | 4901 | `	/* php marks its own data:// wrapper a URL, and that is the one that matters` |
|      - | 4902 | ``	 * here: `include 'data://text/plain;base64,…'` executes bytes from the URI`` |
|      - | 4903 | `	 * itself, which is why php refuses it unless allow_url_include says` |
|      - | 4904 | `	 * otherwise. php:// is NOT a URL wrapper in php and stays open. */` |
|  36560 | 4905 | `	if( pStream && pStream->zName` |
|  36565 | 4906 | `	 && SyStrlen(pStream->zName) == 4 && SyStrnicmp(pStream->zName,"data",4) == 0 ){` |
|     41 | 4907 | `		return 1;` |
|      - | 4908 | `	}` |
| 328249 | 4909 | `	for( i = 0 ; i < PHL_UWRAP_MAX ; i++ ){` |
| 291785 | 4910 | `		if( g_aUwrap[i].pVm && &g_aUwrap[i].sStream == pStream ){` |
|     61 | 4911 | `			return g_aUwrap[i].bIsUrl;` |
|      - | 4912 | `		}` |
| 145866 | 4913 | `	}` |
|  36469 | 4914 | `	return 0;` |
|  18285 | 4915 | `}` |
|      - | 4916 | `/*` |
|      - | 4917 | ` * Is this device one of the userland wrapper slots? php labels every such` |
|      - | 4918 | `` * stream `user-space` rather than by its protocol.`` |
|      - | 4919 | ` */` |
|   8310 | 4920 | `static int IoPrivateIsUwrap(const ph7_io_stream *pStream)` |
|      5 | 4921 | `{` |
|      - | 4922 | `	int i;` |
|  74635 | 4923 | `	for( i = 0 ; i < PHL_UWRAP_MAX ; i++ ){` |
|  66345 | 4924 | `		if( g_aUwrap[i].pVm && &g_aUwrap[i].sStream == pStream ){` |
|     22 | 4925 | `			return 1;` |
|      - | 4926 | `		}` |
|  33165 | 4927 | `	}` |
|   8295 | 4928 | `	return 0;` |
|   4160 | 4929 | `}` |
|      - | 4930 | `/*` |
|      - | 4931 | ` * Is this device one of the registration slots at all? Unlike IoPrivateIsUwrap()` |
|      - | 4932 | ` * this does NOT ask whether the slot is still live -- restore() has to tell a` |
|      - | 4933 | ` * withdrawn userland wrapper from a built-in, and a withdrawn slot has already` |
|      - | 4934 | ` * had its pVm cleared.` |
|      - | 4935 | ` */` |
|     18 | 4936 | `static int UwrapIsSlotDevice(const ph7_io_stream *pStream)` |
|      1 | 4937 | `{` |
|      - | 4938 | `	int i;` |
|     99 | 4939 | `	for( i = 0 ; i < PHL_UWRAP_MAX ; i++ ){` |
|     89 | 4940 | `		if( &g_aUwrap[i].sStream == pStream ){` |
|      9 | 4941 | `			return 1;` |
|      - | 4942 | `		}` |
|     41 | 4943 | `	}` |
|     11 | 4944 | `	return 0;` |
|     10 | 4945 | `}` |
|      - | 4946 | `/* Forward: the protocol dispatcher is defined just below. */` |
|      - | 4947 | `static int UwrapCall(uwrap_handle *pH,const char *zMethod,int nArg,ph7_value **apArg,` |
|      - | 4948 | `	ph7_value *pResult);` |
|      - | 4949 | `/*` |
|      - | 4950 | ` * Ask a userland wrapper whether it is at end of file — php's own` |
|      - | 4951 | ` * streamWrapper::stream_eof(), which PHL used to leave undispatched, inferring` |
|      - | 4952 | ` * the answer from a zero-length read instead. Returns 0 when the handle is not` |
|      - | 4953 | ` * a userland stream (nothing written to *pAnswer).` |
|      - | 4954 | ` */` |
|   8116 | 4955 | `static int IoPrivateUwrapEof(io_private *pDev,int *pAnswer)` |
|      5 | 4956 | `{` |
|      - | 4957 | `	uwrap_handle *pH;` |
|      - | 4958 | `	ph7_value sRet;` |
|   8121 | 4959 | `	if( pDev == 0 \|\| pDev->pHandle == 0 \|\| !IoPrivateIsUwrap(pDev->pStream) ){` |
|   8115 | 4960 | `		return 0;` |
|      - | 4961 | `	}` |
|      8 | 4962 | `	pH = (uwrap_handle *)pDev->pHandle;` |
|      8 | 4963 | `	PH7_MemObjInit(pH->pVm,&sRet);` |
|      8 | 4964 | `	if( UwrapCall(pH,"stream_eof",0,0,&sRet) != 0 ){` |
|      - | 4965 | `		/* php's streamWrapper requires the method; a class without one keeps` |
|      - | 4966 | `		 * the read-derived answer rather than being called into. */` |
|    ! 0 | 4967 | `		PH7_MemObjRelease(&sRet);` |
|    ! 0 | 4968 | `		*pAnswer = pH->bEof;` |
|    ! 0 | 4969 | `		return 1;` |
|      - | 4970 | `	}` |
|      8 | 4971 | `	*pAnswer = ph7_value_to_bool(&sRet) ? 1 : 0;` |
|      8 | 4972 | `	PH7_MemObjRelease(&sRet);` |
|      8 | 4973 | `	return 1;` |
|   4063 | 4974 | `}` |
|      - | 4975 | `/*` |
|      - | 4976 | `` * The wrapper INSTANCE serving an open userland stream (php's `wrapper_data`),`` |
|      - | 4977 | ` * or 0 for any other device.` |
|      - | 4978 | ` */` |
|     82 | 4979 | `static ph7_class_instance * IoPrivateUwrapObject(io_private *pDev)` |
|      4 | 4980 | `{` |
|     86 | 4981 | `	if( pDev == 0 \|\| pDev->pHandle == 0 \|\| !IoPrivateIsUwrap(pDev->pStream) ){` |
|     82 | 4982 | `		return 0;` |
|      - | 4983 | `	}` |
|      5 | 4984 | `	return ((uwrap_handle *)pDev->pHandle)->pObj;` |
|     45 | 4985 | `}` |
|      - | 4986 | `/* Call $obj->$zMethod(...) and copy the result into pResult (may be 0) */` |
|    180 | 4987 | `static int UwrapCall(uwrap_handle *pH,const char *zMethod,int nArg,ph7_value **apArg,` |
|      - | 4988 | `	ph7_value *pResult)` |
|      3 | 4989 | `{` |
|      - | 4990 | `	ph7_class_method *pMeth;` |
|    183 | 4991 | `	if( pH == 0 \|\| pH->pObj == 0 ){` |
|    ! 0 | 4992 | `		return -1;` |
|      - | 4993 | `	}` |
|    183 | 4994 | `	pMeth = PH7_ClassExtractMethod(pH->pObj->pClass,zMethod,(sxu32)SyStrlen(zMethod));` |
|    183 | 4995 | `	if( pMeth == 0 ){` |
|     27 | 4996 | `		return -1;` |
|      - | 4997 | `	}` |
|    159 | 4998 | `	if( PH7_VmCallClassMethod(pH->pVm,pH->pObj,pMeth,pResult,nArg,apArg) != SXRET_OK ){` |
|    ! 0 | 4999 | `		return -1;` |
|      - | 5000 | `	}` |
|    159 | 5001 | `	return 0;` |
|     93 | 5002 | `}` |
|     58 | 5003 | `static ph7_int64 UwrapRead(void *pHandle,void *pBuffer,ph7_int64 nRead)` |
|      3 | 5004 | `{` |
|     61 | 5005 | `	uwrap_handle *pH = (uwrap_handle *)pHandle;` |
|      - | 5006 | `	ph7_value sArg,sRet;` |
|      - | 5007 | `	const char *zData;` |
|     61 | 5008 | `	int nData = 0;` |
|     61 | 5009 | `	ph7_int64 n = 0;` |
|     61 | 5010 | `	if( pH == 0 \|\| pH->bEof ){` |
|    ! 0 | 5011 | `		return 0;` |
|      - | 5012 | `	}` |
|     61 | 5013 | `	PH7_MemObjInit(pH->pVm,&sArg);` |
|     61 | 5014 | `	PH7_MemObjInit(pH->pVm,&sRet);` |
|     61 | 5015 | `	ph7_value_int64(&sArg,nRead);` |
|      - | 5016 | `	{` |
|      - | 5017 | `		ph7_value *apArg[1];` |
|     61 | 5018 | `		apArg[0] = &sArg;` |
|     61 | 5019 | `		if( UwrapCall(pH,"stream_read",1,apArg,&sRet) != 0 ){` |
|    ! 0 | 5020 | `			PH7_MemObjRelease(&sArg);` |
|    ! 0 | 5021 | `			PH7_MemObjRelease(&sRet);` |
|    ! 0 | 5022 | `			return -1;` |
|      - | 5023 | `		}` |
|      - | 5024 | `	}` |
|     61 | 5025 | `	zData = ph7_value_to_string(&sRet,&nData);` |
|     61 | 5026 | `	if( nData > 0 ){` |
|     31 | 5027 | `		if( (ph7_int64)nData > nRead ){` |
|    ! 0 | 5028 | `			nData = (int)nRead;` |
|    ! 0 | 5029 | `		}` |
|     31 | 5030 | `		SyMemcpy(zData,pBuffer,(sxu32)nData);` |
|     31 | 5031 | `		n = nData;` |
|     17 | 5032 | `	}else{` |
|     32 | 5033 | `		pH->bEof = 1;` |
|      - | 5034 | `	}` |
|     61 | 5035 | `	PH7_MemObjRelease(&sArg);` |
|     61 | 5036 | `	PH7_MemObjRelease(&sRet);` |
|     61 | 5037 | `	return n;` |
|     32 | 5038 | `}` |
|      4 | 5039 | `static ph7_int64 UwrapWrite(void *pHandle,const void *pBuf,ph7_int64 nWrite)` |
|      1 | 5040 | `{` |
|      5 | 5041 | `	uwrap_handle *pH = (uwrap_handle *)pHandle;` |
|      - | 5042 | `	ph7_value sArg,sRet;` |
|      - | 5043 | `	ph7_int64 n;` |
|      5 | 5044 | `	if( pH == 0 ){` |
|    ! 0 | 5045 | `		return -1;` |
|      - | 5046 | `	}` |
|      5 | 5047 | `	PH7_MemObjInit(pH->pVm,&sArg);` |
|      5 | 5048 | `	PH7_MemObjInit(pH->pVm,&sRet);` |
|      5 | 5049 | `	ph7_value_string(&sArg,(const char *)pBuf,(int)nWrite);` |
|      - | 5050 | `	{` |
|      - | 5051 | `		ph7_value *apArg[1];` |
|      5 | 5052 | `		apArg[0] = &sArg;` |
|      5 | 5053 | `		if( UwrapCall(pH,"stream_write",1,apArg,&sRet) != 0 ){` |
|    ! 0 | 5054 | `			PH7_MemObjRelease(&sArg);` |
|    ! 0 | 5055 | `			PH7_MemObjRelease(&sRet);` |
|    ! 0 | 5056 | `			return -1;` |
|      - | 5057 | `		}` |
|      - | 5058 | `	}` |
|      5 | 5059 | `	n = ph7_value_to_int64(&sRet);` |
|      5 | 5060 | `	PH7_MemObjRelease(&sArg);` |
|      5 | 5061 | `	PH7_MemObjRelease(&sRet);` |
|      5 | 5062 | `	return n;` |
|      3 | 5063 | `}` |
|      2 | 5064 | `static int UwrapSeek(void *pHandle,ph7_int64 iOfft,int whence)` |
|      1 | 5065 | `{` |
|      3 | 5066 | `	uwrap_handle *pH = (uwrap_handle *)pHandle;` |
|      - | 5067 | `	ph7_value sOfft,sWhence,sRet;` |
|      - | 5068 | `	ph7_value *apArg[2];` |
|      - | 5069 | `	int rc;` |
|      3 | 5070 | `	if( pH == 0 ){` |
|    ! 0 | 5071 | `		return -1;` |
|      - | 5072 | `	}` |
|      3 | 5073 | `	PH7_MemObjInit(pH->pVm,&sOfft);` |
|      3 | 5074 | `	PH7_MemObjInit(pH->pVm,&sWhence);` |
|      3 | 5075 | `	PH7_MemObjInit(pH->pVm,&sRet);` |
|      3 | 5076 | `	ph7_value_int64(&sOfft,iOfft);` |
|      3 | 5077 | `	ph7_value_int(&sWhence,whence);` |
|      3 | 5078 | `	apArg[0] = &sOfft;` |
|      3 | 5079 | `	apArg[1] = &sWhence;` |
|      3 | 5080 | `	rc = UwrapCall(pH,"stream_seek",2,apArg,&sRet);` |
|      3 | 5081 | `	if( rc == 0 ){` |
|      3 | 5082 | `		pH->bEof = 0;` |
|      3 | 5083 | `		rc = ph7_value_to_bool(&sRet) ? PH7_OK : -1;` |
|      1 | 5084 | `	}` |
|      3 | 5085 | `	PH7_MemObjRelease(&sOfft);` |
|      3 | 5086 | `	PH7_MemObjRelease(&sWhence);` |
|      3 | 5087 | `	PH7_MemObjRelease(&sRet);` |
|      3 | 5088 | `	return rc;` |
|      2 | 5089 | `}` |
|      6 | 5090 | `static ph7_int64 UwrapTell(void *pHandle)` |
|      1 | 5091 | `{` |
|      7 | 5092 | `	uwrap_handle *pH = (uwrap_handle *)pHandle;` |
|      - | 5093 | `	ph7_value sRet;` |
|      - | 5094 | `	ph7_int64 n;` |
|      7 | 5095 | `	if( pH == 0 ){` |
|    ! 0 | 5096 | `		return -1;` |
|      - | 5097 | `	}` |
|      7 | 5098 | `	PH7_MemObjInit(pH->pVm,&sRet);` |
|      7 | 5099 | `	if( UwrapCall(pH,"stream_tell",0,0,&sRet) != 0 ){` |
|      3 | 5100 | `		PH7_MemObjRelease(&sRet);` |
|      3 | 5101 | `		return -1;` |
|      - | 5102 | `	}` |
|      5 | 5103 | `	n = ph7_value_to_int64(&sRet);` |
|      5 | 5104 | `	PH7_MemObjRelease(&sRet);` |
|      5 | 5105 | `	return n;` |
|      4 | 5106 | `}` |
|     50 | 5107 | `static void UwrapClose(void *pHandle)` |
|      3 | 5108 | `{` |
|     53 | 5109 | `	uwrap_handle *pH = (uwrap_handle *)pHandle;` |
|     53 | 5110 | `	if( pH == 0 ){` |
|    ! 0 | 5111 | `		return;` |
|      - | 5112 | `	}` |
|     53 | 5113 | `	UwrapCall(pH,"stream_close",0,0,0);` |
|     53 | 5114 | `	if( pH->pObj ){` |
|     53 | 5115 | `		PH7_ClassInstanceUnref(pH->pObj);` |
|     25 | 5116 | `	}` |
|     53 | 5117 | `	SyMemBackendFree(&pH->pVm->sAllocator,pH);` |
|     28 | 5118 | `}` |
|      - | 5119 | `/* Shared open: instantiate the wrapper class and call stream_open() */` |
|     54 | 5120 | `static int UwrapOpenSlot(int iSlot,const char *zName,int iMode,ph7_value *pResource,void **ppHandle)` |
|      3 | 5121 | `{` |
|     57 | 5122 | `	uwrap_slot *pSlot = &g_aUwrap[iSlot];` |
|     57 | 5123 | `	ph7_vm *pVm = pResource ? pResource->pVm : 0;` |
|      - | 5124 | `	ph7_class *pClass;` |
|      - | 5125 | `	uwrap_handle *pH;` |
|      - | 5126 | `	ph7_value sPath,sMode,sOpts,sOpened,sRet;` |
|      - | 5127 | `	ph7_value *apArg[4];` |
|      - | 5128 | `	int rc;` |
|     57 | 5129 | `	if( pVm == 0 \|\| pSlot->pVm == 0 ){` |
|    ! 0 | 5130 | `		return -1;` |
|      - | 5131 | `	}` |
|     57 | 5132 | `	pClass = PH7_VmExtractClass(pVm,pSlot->zClass,(sxu32)SyStrlen(pSlot->zClass),TRUE,0);` |
|     57 | 5133 | `	if( pClass == 0 ){` |
|    ! 0 | 5134 | `		return -1;` |
|      - | 5135 | `	}` |
|     57 | 5136 | `	pH = (uwrap_handle *)SyMemBackendAlloc(&pVm->sAllocator,sizeof(uwrap_handle));` |
|     57 | 5137 | `	if( pH == 0 ){` |
|    ! 0 | 5138 | `		return -1;` |
|      - | 5139 | `	}` |
|     57 | 5140 | `	pH->pVm = pVm;` |
|     57 | 5141 | `	pH->iSlot = iSlot;` |
|     57 | 5142 | `	pH->bEof = 0;` |
|     57 | 5143 | `	pH->pObj = PH7_NewClassInstance(pVm,pClass);` |
|     57 | 5144 | `	if( pH->pObj == 0 ){` |
|    ! 0 | 5145 | `		SyMemBackendFree(&pVm->sAllocator,pH);` |
|    ! 0 | 5146 | `		return -1;` |
|      - | 5147 | `	}` |
|      - | 5148 | `	{` |
|      - | 5149 | `		/* php's streamWrapper::$context, set on the serving instance BEFORE` |
|      - | 5150 | `		 * stream_open() runs — which is the whole reason a userland wrapper can` |
|      - | 5151 | `		 * be configured per open. It is exactly what the OPENER resolved: the` |
|      - | 5152 | ``		 * default context substitutes for a NULL `$context` argument, so an`` |
|      - | 5153 | `		 * ordinary fopen() hands a resource over; but an opener with no such` |
|      - | 5154 | `		 * argument at all (md5_file(), include) and one that carried` |
|      - | 5155 | `		 * FILE_NO_DEFAULT_CONTEXT hand over php's NULL. Substituting the default` |
|      - | 5156 | `		 * here would make that flag mean nothing.` |
|      - | 5157 | `		 * The class need not declare the slot; php adds it either way. */` |
|     57 | 5158 | `		phl_stream_ctx *pOpenCtx = (phl_stream_ctx *)pVm->pOpenCtx;` |
|     57 | 5159 | `		ph7_value *pCtxSlot = PH7_NativeAttr(pH->pObj,"context");` |
|     57 | 5160 | `		if( pCtxSlot == 0 ){` |
|    ! 0 | 5161 | `			pCtxSlot = PH7_VmCreateDynamicAttr(pVm,pH->pObj,"context",sizeof("context")-1,0);` |
|    ! 0 | 5162 | `		}` |
|     57 | 5163 | `		if( pCtxSlot ){` |
|     57 | 5164 | `			if( pOpenCtx ){` |
|     45 | 5165 | `				ph7_value_resource(pCtxSlot,(void *)pOpenCtx);` |
|     24 | 5166 | `			}else{` |
|     14 | 5167 | `				ph7_value_null(pCtxSlot);` |
|      - | 5168 | `			}` |
|     27 | 5169 | `		}` |
|      - | 5170 | `	}` |
|      - | 5171 | `	/* php hands stream_open the FULL url, scheme included */` |
|     57 | 5172 | `	PH7_MemObjInit(pVm,&sPath);` |
|     57 | 5173 | `	PH7_MemObjInit(pVm,&sMode);` |
|     57 | 5174 | `	PH7_MemObjInit(pVm,&sOpts);` |
|     57 | 5175 | `	PH7_MemObjInit(pVm,&sRet);` |
|      - | 5176 | `	/* $opened_path is BY REFERENCE: the callee's binding needs a real memobj` |
|      - | 5177 | `	 * slot (a stack ph7_value has nIdx == SXU32_HIGH and the engine rejects` |
|      - | 5178 | `	 * it as "could not be passed by reference"). */` |
|      - | 5179 | `	{` |
|     57 | 5180 | `		ph7_value *pRefSlot = PH7_ReserveMemObj(pVm);` |
|     57 | 5181 | `		if( pRefSlot == 0 ){` |
|    ! 0 | 5182 | `			PH7_ClassInstanceUnref(pH->pObj);` |
|    ! 0 | 5183 | `			SyMemBackendFree(&pVm->sAllocator,pH);` |
|    ! 0 | 5184 | `			return -1;` |
|      - | 5185 | `		}` |
|     57 | 5186 | `		PH7_MemObjInit(pVm,&sOpened);` |
|     57 | 5187 | `		sOpened.nIdx = pRefSlot->nIdx;` |
|      - | 5188 | `	}` |
|     54 | 5189 | `	if( SyStrlen(pSlot->zScheme) == sizeof("file")-1` |
|     47 | 5190 | `	 && SyStrnicmp(pSlot->zScheme,"file",sizeof("file")-1) == 0 ){` |
|      - | 5191 | `		/* The one scheme php does NOT hand back whole. Its locate_url_wrapper` |
|      - | 5192 | `		 * strips "file://" for whoever owns the name, built-in or not, so a` |
|      - | 5193 | `		 * wrapper that replaced file:// sees the plain path -- the same bytes a` |
|      - | 5194 | `		 * bare path would have given it. */` |
|      3 | 5195 | `		ph7_value_string(&sPath,zName,-1);` |
|      2 | 5196 | `	}else{` |
|      - | 5197 | `		SyBlob sUrl;` |
|     55 | 5198 | `		SyBlobInit(&sUrl,&pVm->sAllocator);` |
|     55 | 5199 | `		SyBlobFormat(&sUrl,"%s://%s",pSlot->zScheme,zName);` |
|     55 | 5200 | `		ph7_value_string(&sPath,(const char *)SyBlobData(&sUrl),(int)SyBlobLength(&sUrl));` |
|     55 | 5201 | `		SyBlobRelease(&sUrl);` |
|      - | 5202 | `	}` |
|     83 | 5203 | `	ph7_value_string(&sMode,(iMode & PH7_IO_OPEN_WRONLY) ? "w"` |
|     52 | 5204 | `		: ((iMode & PH7_IO_OPEN_APPEND) ? "a" : "r"),-1);` |
|     57 | 5205 | `	ph7_value_int(&sOpts,0);` |
|     57 | 5206 | `	apArg[0] = &sPath;` |
|     57 | 5207 | `	apArg[1] = &sMode;` |
|     57 | 5208 | `	apArg[2] = &sOpts;` |
|     57 | 5209 | `	apArg[3] = &sOpened;` |
|     57 | 5210 | `	rc = UwrapCall(pH,"stream_open",4,apArg,&sRet);` |
|     57 | 5211 | `	if( rc == 0 && !ph7_value_to_bool(&sRet) ){` |
|    ! 0 | 5212 | `		rc = -1;` |
|    ! 0 | 5213 | `	}` |
|     57 | 5214 | `	PH7_MemObjRelease(&sPath);` |
|     57 | 5215 | `	PH7_MemObjRelease(&sMode);` |
|     57 | 5216 | `	PH7_MemObjRelease(&sOpts);` |
|     57 | 5217 | `	PH7_MemObjRelease(&sOpened);` |
|     57 | 5218 | `	PH7_MemObjRelease(&sRet);` |
|     57 | 5219 | `	if( rc != 0 ){` |
|    ! 0 | 5220 | `		PH7_ClassInstanceUnref(pH->pObj);` |
|    ! 0 | 5221 | `		SyMemBackendFree(&pVm->sAllocator,pH);` |
|    ! 0 | 5222 | `		return -1;` |
|      - | 5223 | `	}` |
|     57 | 5224 | `	*ppHandle = (void *)pH;` |
|     57 | 5225 | `	return PH7_OK;` |
|     30 | 5226 | `}` |
|      - | 5227 | `/* One xOpen thunk per slot (the device callbacks get no device pointer) */` |
|      - | 5228 | `#define PHL_UWRAP_THUNK(N) \` |
|      - | 5229 | `	static int UwrapOpen##N(const char *zName,int iMode,ph7_value *pResource,void **ppHandle) \` |
|      - | 5230 | `	{ return UwrapOpenSlot(N,zName,iMode,pResource,ppHandle); }` |
|     53 | 5231 | `PHL_UWRAP_THUNK(0)` |
|      5 | 5232 | `PHL_UWRAP_THUNK(1)` |
|    ! 0 | 5233 | `PHL_UWRAP_THUNK(2)` |
|    ! 0 | 5234 | `PHL_UWRAP_THUNK(3)` |
|    ! 0 | 5235 | `PHL_UWRAP_THUNK(4)` |
|    ! 0 | 5236 | `PHL_UWRAP_THUNK(5)` |
|    ! 0 | 5237 | `PHL_UWRAP_THUNK(6)` |
|    ! 0 | 5238 | `PHL_UWRAP_THUNK(7)` |
|      - | 5239 | `static int (* const g_aUwrapOpen[PHL_UWRAP_MAX])(const char *,int,ph7_value *,void **) = {` |
|      - | 5240 | `	UwrapOpen0,UwrapOpen1,UwrapOpen2,UwrapOpen3,` |
|      - | 5241 | `	UwrapOpen4,UwrapOpen5,UwrapOpen6,UwrapOpen7` |
|      - | 5242 | `};` |
|      - | 5243 | `/* Is this device already in the VM's list? (A slot survives its wrapper.) */` |
|     26 | 5244 | `static int UwrapDeviceInstalled(ph7_vm *pVm,const ph7_io_stream *pStream)` |
|      3 | 5245 | `{` |
|     29 | 5246 | `	ph7_io_stream **apDev = (ph7_io_stream **)SySetBasePtr(&pVm->aIOstream);` |
|      - | 5247 | `	sxu32 n;` |
|    137 | 5248 | `	for( n = 0 ; n < SySetUsed(&pVm->aIOstream) ; n++ ){` |
|    117 | 5249 | `		if( apDev[n] == pStream ){` |
|      7 | 5250 | `			return 1;` |
|      - | 5251 | `		}` |
|     57 | 5252 | `	}` |
|     23 | 5253 | `	return 0;` |
|     16 | 5254 | `}` |
|      - | 5255 | `/* Put a device back in service. */` |
|     26 | 5256 | `static void UwrapUnsuppressDevice(ph7_vm *pVm,const ph7_io_stream *pStream)` |
|      3 | 5257 | `{` |
|     29 | 5258 | `	const ph7_io_stream **apOff = (const ph7_io_stream **)SySetBasePtr(&pVm->aSuppressedIo);` |
|     29 | 5259 | `	sxu32 n,nKeep = 0;` |
|     39 | 5260 | `	for( n = 0 ; n < SySetUsed(&pVm->aSuppressedIo) ; n++ ){` |
|     11 | 5261 | `		if( apOff[n] == pStream ){` |
|      7 | 5262 | `			continue;` |
|      - | 5263 | `		}` |
|      5 | 5264 | `		apOff[nKeep++] = apOff[n];` |
|      3 | 5265 | `	}` |
|     29 | 5266 | `	SySetTruncate(&pVm->aSuppressedIo,nKeep);` |
|     29 | 5267 | `}` |
|      - | 5268 | `/*` |
|      - | 5269 | ` * bool stream_wrapper_register(string $protocol, string $class, int $flags = 0)` |
|      - | 5270 | ` * bool stream_wrapper_unregister(string $protocol)` |
|      - | 5271 | ` */` |
|     26 | 5272 | `PH7_PRIVATE int PH7_builtin_stream_wrapper_register(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      3 | 5273 | `{` |
|      - | 5274 | `	const char *zScheme,*zClass;` |
|     29 | 5275 | `	int nScheme,nClass,i,iFree = -1;` |
|     29 | 5276 | `	if( nArg < 2 ){` |
|    ! 0 | 5277 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 5278 | `		return PH7_OK;` |
|      - | 5279 | `	}` |
|     29 | 5280 | `	zScheme = ph7_value_to_string(apArg[0],&nScheme);` |
|     29 | 5281 | `	zClass  = ph7_value_to_string(apArg[1],&nClass);` |
|     26 | 5282 | `	if( nScheme < 1 \|\| nScheme >= (int)sizeof(g_aUwrap[0].zScheme)` |
|     29 | 5283 | `	 \|\| nClass < 1 \|\| nClass >= (int)sizeof(g_aUwrap[0].zClass) ){` |
|    ! 0 | 5284 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 5285 | `		return PH7_OK;` |
|      - | 5286 | `	}` |
|      - | 5287 | `	/* php: registering an already-taken protocol warns and returns false.` |
|      - | 5288 | `	 * (Scan the device list directly — PH7_VmGetStreamDevice falls back to` |
|      - | 5289 | `	 * the DEFAULT device for a scheme-less name, so it can't answer this.) */` |
|      - | 5290 | `	{` |
|     29 | 5291 | `		ph7_io_stream **apDev = (ph7_io_stream **)SySetBasePtr(&pCtx->pVm->aIOstream);` |
|      - | 5292 | `		sxu32 n;` |
|    143 | 5293 | `		for( n = 0 ; n < SySetUsed(&pCtx->pVm->aIOstream) ; n++ ){` |
|    117 | 5294 | `			if( PH7_VmStreamDeviceSuppressed(pCtx->pVm,apDev[n]) ){` |
|     11 | 5295 | `				continue; /* unregistered: the name is free again, which is the` |
|      - | 5296 | `				           * whole point of "replace file:// with my own" */` |
|      - | 5297 | `			}` |
|    104 | 5298 | `			if( (int)SyStrlen(apDev[n]->zName) == nScheme` |
|     63 | 5299 | `			 && SyStrnicmp(apDev[n]->zName,zScheme,(sxu32)nScheme) == 0 ){` |
|    ! 0 | 5300 | `				ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,` |
|    ! 0 | 5301 | `					"Protocol %.*s:// is already defined.",nScheme,zScheme);` |
|    ! 0 | 5302 | `				ph7_result_bool(pCtx,0);` |
|    ! 0 | 5303 | `				return PH7_OK;` |
|      - | 5304 | `			}` |
|     55 | 5305 | `		}` |
|      - | 5306 | `	}` |
|     33 | 5307 | `	for( i = 0 ; i < PHL_UWRAP_MAX ; i++ ){` |
|     33 | 5308 | `		if( g_aUwrap[i].pVm == 0 ){` |
|     29 | 5309 | `			iFree = i;` |
|     29 | 5310 | `			break;` |
|      - | 5311 | `		}` |
|      4 | 5312 | `	}` |
|     29 | 5313 | `	if( iFree < 0 ){` |
|    ! 0 | 5314 | `		ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,` |
|      - | 5315 | `			"Too many registered stream wrappers (PHL limit: %d)",PHL_UWRAP_MAX);` |
|    ! 0 | 5316 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 5317 | `		return PH7_OK;` |
|      - | 5318 | `	}` |
|      - | 5319 | `	{` |
|     29 | 5320 | `		uwrap_slot *pSlot = &g_aUwrap[iFree];` |
|     29 | 5321 | `		SyMemcpy(zScheme,pSlot->zScheme,(sxu32)nScheme);` |
|     29 | 5322 | `		pSlot->zScheme[nScheme] = 0;` |
|     29 | 5323 | `		SyMemcpy(zClass,pSlot->zClass,(sxu32)nClass);` |
|     29 | 5324 | `		pSlot->zClass[nClass] = 0;` |
|     29 | 5325 | `		pSlot->pVm = pCtx->pVm;` |
|      - | 5326 | `		/* $flags: php defines exactly one bit for it, STREAM_IS_URL, and it is the` |
|      - | 5327 | `		 * whole reason the argument exists — a wrapper that says it speaks to the` |
|      - | 5328 | `		 * NETWORK is the one allow_url_fopen and allow_url_include turn off. It was` |
|      - | 5329 | `		 * declared in the signature and read by nothing, so a wrapper registered as` |
|      - | 5330 | `		 * a URL was opened and INCLUDED like a local file whatever the` |
|      - | 5331 | `		 * configuration said. */` |
|     29 | 5332 | `		pSlot->bIsUrl = (nArg > 2 && (ph7_value_to_int64(apArg[2]) & PH7_STREAM_IS_URL) != 0);` |
|     29 | 5333 | `		SyZero(&pSlot->sStream,sizeof(ph7_io_stream));` |
|     29 | 5334 | `		pSlot->sStream.zName = pSlot->zScheme;` |
|     29 | 5335 | `		pSlot->sStream.iVersion = PH7_IO_STREAM_VERSION;` |
|     29 | 5336 | `		pSlot->sStream.xOpen = g_aUwrapOpen[iFree];` |
|     29 | 5337 | `		pSlot->sStream.xClose = UwrapClose;` |
|     29 | 5338 | `		pSlot->sStream.xRead = UwrapRead;` |
|     29 | 5339 | `		pSlot->sStream.xWrite = UwrapWrite;` |
|     29 | 5340 | `		pSlot->sStream.xSeek = UwrapSeek;` |
|     29 | 5341 | `		pSlot->sStream.xTell = UwrapTell;` |
|      - | 5342 | `		/* A slot is REUSED once its wrapper has been unregistered, and both the` |
|      - | 5343 | `		 * suppression set and the VM's device list still name it -- so lift the` |
|      - | 5344 | `		 * suppression and install the device only if it is not already there,` |
|      - | 5345 | `		 * or the freshly registered protocol would be born switched off (and` |
|      - | 5346 | `		 * listed twice). */` |
|     29 | 5347 | `		UwrapUnsuppressDevice(pCtx->pVm,&pSlot->sStream);` |
|     29 | 5348 | `		if( !UwrapDeviceInstalled(pCtx->pVm,&pSlot->sStream) ){` |
|     23 | 5349 | `			ph7_vm_config(pCtx->pVm,PH7_VM_CONFIG_IO_STREAM,&pSlot->sStream);` |
|     10 | 5350 | `		}` |
|      - | 5351 | `	}` |
|     29 | 5352 | `	ph7_result_bool(pCtx,1);` |
|     29 | 5353 | `	return PH7_OK;` |
|     16 | 5354 | `}` |
|      - | 5355 | `/*` |
|      - | 5356 | ` * Suppress a live device and, when it is a userland slot, retire the slot with` |
|      - | 5357 | ` * it. Answers 0 when nothing by that name was in service.` |
|      - | 5358 | ` *` |
|      - | 5359 | ` * The match is EXACT and case-SENSITIVE, which php's is too: opening a stream` |
|      - | 5360 | ` * folds the scheme ("FILE://x" reads a file), but unregister() and restore()` |
|      - | 5361 | ` * delete from the wrapper hash by the bytes the script wrote, so` |
|      - | 5362 | ` * stream_wrapper_unregister('FILE') fails where 'file' succeeds.` |
|      - | 5363 | ` */` |
|     14 | 5364 | `static int UwrapSuppressDevice(ph7_vm *pVm,const char *zScheme,int nScheme)` |
|      2 | 5365 | `{` |
|     16 | 5366 | `	ph7_io_stream **apDev = (ph7_io_stream **)SySetBasePtr(&pVm->aIOstream);` |
|     16 | 5367 | `	ph7_io_stream *pHit = 0;` |
|      - | 5368 | `	sxu32 n;` |
|      - | 5369 | `	int i;` |
|     84 | 5370 | `	for( n = 0 ; n < SySetUsed(&pVm->aIOstream) ; n++ ){` |
|     68 | 5371 | `		if( (int)SyStrlen(apDev[n]->zName) == nScheme` |
|     46 | 5372 | `		 && SyMemcmp(apDev[n]->zName,zScheme,(sxu32)nScheme) == 0` |
|     19 | 5373 | `		 && !PH7_VmStreamDeviceSuppressed(pVm,apDev[n]) ){` |
|     12 | 5374 | `			pHit = apDev[n]; /* the LIVE one is the last match */` |
|      5 | 5375 | `		}` |
|     36 | 5376 | `	}` |
|     16 | 5377 | `	if( pHit == 0 ){` |
|      5 | 5378 | `		return 0;` |
|      - | 5379 | `	}` |
|     12 | 5380 | `	if( SySetPut(&pVm->aSuppressedIo,(const void *)&pHit) != SXRET_OK ){` |
|    ! 0 | 5381 | `		return 0;` |
|      - | 5382 | `	}` |
|     44 | 5383 | `	for( i = 0 ; i < PHL_UWRAP_MAX ; i++ ){` |
|     40 | 5384 | `		if( g_aUwrap[i].pVm == pVm && &g_aUwrap[i].sStream == pHit ){` |
|      8 | 5385 | `			g_aUwrap[i].pVm = 0;` |
|      8 | 5386 | `			break;` |
|      - | 5387 | `		}` |
|     17 | 5388 | `	}` |
|     12 | 5389 | `	return 1;` |
|      9 | 5390 | `}` |
|      - | 5391 | `/*` |
|      - | 5392 | ` * bool stream_wrapper_unregister(string $protocol)` |
|      - | 5393 | ` *  Take a protocol out of service. It used to handle USERLAND slots only and` |
|      - | 5394 | ` *  answer FALSE for file/php/data/tcp, so the documented "replace file:// with` |
|      - | 5395 | ` *  my own wrapper" idiom failed loudly at the first step. A built-in is now` |
|      - | 5396 | ` *  suppressed per VM: PH7_VmGetStreamDevice() steps over it (including on the` |
|      - | 5397 | ` *  no-scheme default path, which is the same slot), stream_get_wrappers() stops` |
|      - | 5398 | ` *  naming it, and the name becomes free to register again.` |
|      - | 5399 | ` */` |
|     14 | 5400 | `PH7_PRIVATE int PH7_builtin_stream_wrapper_unregister(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      2 | 5401 | `{` |
|      - | 5402 | `	const char *zScheme;` |
|      - | 5403 | `	int nScheme;` |
|     16 | 5404 | `	if( nArg < 1 ){` |
|    ! 0 | 5405 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 5406 | `		return PH7_OK;` |
|      - | 5407 | `	}` |
|     16 | 5408 | `	zScheme = ph7_value_to_string(apArg[0],&nScheme);` |
|     16 | 5409 | `	if( nScheme > 0 && UwrapSuppressDevice(pCtx->pVm,zScheme,nScheme) ){` |
|     12 | 5410 | `		ph7_result_bool(pCtx,1);` |
|     12 | 5411 | `		return PH7_OK;` |
|      - | 5412 | `	}` |
|      7 | 5413 | `	ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,` |
|      2 | 5414 | `		"Unable to unregister protocol %.*s://",nScheme,zScheme);` |
|      5 | 5415 | `	ph7_result_bool(pCtx,0);` |
|      5 | 5416 | `	return PH7_OK;` |
|      9 | 5417 | `}` |
|      - | 5418 | `/*` |
|      - | 5419 | ` * bool stream_wrapper_restore(string $protocol)` |
|      - | 5420 | ` *  Put a BUILT-IN protocol back, whether it was unregistered or replaced. The` |
|      - | 5421 | ` *  other half of the override pair, and useless without it -- which is why the` |
|      - | 5422 | ` *  two ship together.` |
|      - | 5423 | ` *` |
|      - | 5424 | ` *  php's three answers: a protocol that was never built in is a warning and` |
|      - | 5425 | ` *  FALSE; one that is built in and was never touched is an E_NOTICE and TRUE` |
|      - | 5426 | ` *  (it is already what it should be); anything else is restored and TRUE.` |
|      - | 5427 | ` */` |
|     12 | 5428 | `PH7_PRIVATE int PH7_builtin_stream_wrapper_restore(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 5429 | `{` |
|     13 | 5430 | `	ph7_vm *pVm = pCtx->pVm;` |
|      - | 5431 | `	const ph7_io_stream **apOff;` |
|      - | 5432 | `	ph7_io_stream **apDev;` |
|      - | 5433 | `	const char *zScheme;` |
|     13 | 5434 | `	int nScheme,bBuiltin = 0,bChanged = 0,i;` |
|      - | 5435 | `	sxu32 n,nKeep;` |
|     13 | 5436 | `	if( nArg < 1 ){` |
|    ! 0 | 5437 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 5438 | `		return PH7_OK;` |
|      - | 5439 | `	}` |
|     13 | 5440 | `	zScheme = ph7_value_to_string(apArg[0],&nScheme);` |
|     13 | 5441 | `	apDev = (ph7_io_stream **)SySetBasePtr(&pVm->aIOstream);` |
|     73 | 5442 | `	for( n = 0 ; n < SySetUsed(&pVm->aIOstream) ; n++ ){` |
|     61 | 5443 | `		ph7_io_stream *pDev = apDev[n];` |
|     60 | 5444 | `		if( (int)SyStrlen(pDev->zName) != nScheme` |
|     44 | 5445 | `		 \|\| SyMemcmp(pDev->zName,zScheme,(sxu32)nScheme) != 0 ){` |
|     47 | 5446 | `			continue;` |
|      - | 5447 | `		}` |
|     15 | 5448 | `		if( UwrapIsSlotDevice(pDev) ){` |
|      - | 5449 | `			/* A userland wrapper standing in its place -- or one already` |
|      - | 5450 | `			 * withdrawn, which is still not a built-in. */` |
|      9 | 5451 | `			if( !PH7_VmStreamDeviceSuppressed(pVm,pDev) ){` |
|      5 | 5452 | `				bChanged = 1;` |
|      2 | 5453 | `			}` |
|      9 | 5454 | `			continue;` |
|      - | 5455 | `		}` |
|      7 | 5456 | `		bBuiltin = 1;` |
|      7 | 5457 | `		if( PH7_VmStreamDeviceSuppressed(pVm,pDev) ){` |
|      5 | 5458 | `			bChanged = 1;` |
|      2 | 5459 | `		}` |
|      4 | 5460 | `	}` |
|     13 | 5461 | `	if( !bBuiltin ){` |
|     10 | 5462 | `		ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,` |
|      3 | 5463 | `			"%.*s:// never existed, nothing to restore",nScheme,zScheme);` |
|      7 | 5464 | `		ph7_result_bool(pCtx,0);` |
|      7 | 5465 | `		return PH7_OK;` |
|      - | 5466 | `	}` |
|      7 | 5467 | `	if( !bChanged ){` |
|      - | 5468 | `		/* php answers TRUE here and says so at NOTICE level: the protocol is` |
|      - | 5469 | `		 * already the one it would restore. */` |
|      4 | 5470 | `		ph7_context_throw_error_format(pCtx,PH7_CTX_NOTICE,` |
|      1 | 5471 | `			"%.*s:// was never changed, nothing to restore",nScheme,zScheme);` |
|      3 | 5472 | `		ph7_result_bool(pCtx,1);` |
|      3 | 5473 | `		return PH7_OK;` |
|      - | 5474 | `	}` |
|      - | 5475 | `	/* Lift the suppression off the BUILT-IN first, by compacting the set... */` |
|      5 | 5476 | `	apOff = (const ph7_io_stream **)SySetBasePtr(&pVm->aSuppressedIo);` |
|      5 | 5477 | `	nKeep = 0;` |
|      9 | 5478 | `	for( n = 0 ; n < SySetUsed(&pVm->aSuppressedIo) ; n++ ){` |
|      5 | 5479 | `		const ph7_io_stream *pDev = apOff[n];` |
|      4 | 5480 | `		if( (int)SyStrlen(pDev->zName) == nScheme` |
|      4 | 5481 | `		 && SyMemcmp(pDev->zName,zScheme,(sxu32)nScheme) == 0` |
|      5 | 5482 | `		 && !UwrapIsSlotDevice(pDev) ){` |
|      5 | 5483 | `			continue; /* the built-in comes back */` |
|      - | 5484 | `		}` |
|    ! 0 | 5485 | `		apOff[nKeep++] = pDev;` |
|    ! 0 | 5486 | `	}` |
|      5 | 5487 | `	SySetTruncate(&pVm->aSuppressedIo,nKeep);` |
|      - | 5488 | `	/* ...then retire every userland wrapper standing in for the name. */` |
|     37 | 5489 | `	for( i = 0 ; i < PHL_UWRAP_MAX ; i++ ){` |
|     32 | 5490 | `		if( g_aUwrap[i].pVm == pVm` |
|     18 | 5491 | `		 && (int)SyStrlen(g_aUwrap[i].zScheme) == nScheme` |
|      5 | 5492 | `		 && SyMemcmp(g_aUwrap[i].zScheme,zScheme,(sxu32)nScheme) == 0 ){` |
|      5 | 5493 | `			const ph7_io_stream *pDead = &g_aUwrap[i].sStream;` |
|      5 | 5494 | `			g_aUwrap[i].pVm = 0;` |
|      5 | 5495 | `			if( !PH7_VmStreamDeviceSuppressed(pVm,pDead) ){` |
|      5 | 5496 | `				SySetPut(&pVm->aSuppressedIo,(const void *)&pDead);` |
|      2 | 5497 | `			}` |
|      2 | 5498 | `		}` |
|     17 | 5499 | `	}` |
|      5 | 5500 | `	ph7_result_bool(pCtx,1);` |
|      5 | 5501 | `	return PH7_OK;` |
|      7 | 5502 | `}` |
|      - | 5503 | `#ifdef PH7_ENABLE_NET` |
|      - | 5504 | `/*` |
|      - | 5505 | `` * php's socket address: `[transport://]host:port`. What a re-derivation gets`` |
|      - | 5506 | ` * wrong here is that BOTH halves have a diagnostic of their own, and neither is` |
|      - | 5507 | ` * the other: a transport this build does not carry is not a malformed address,` |
|      - | 5508 | ` * and an address with no port is not an unknown transport.` |
|      - | 5509 | ` */` |
|      - | 5510 | `#define SOCK_ADDR_OK        0` |
|      - | 5511 | `#define SOCK_ADDR_TRANSPORT 1 /* named a transport this build has not got */` |
|      - | 5512 | `#define SOCK_ADDR_PARSE     2 /* no port separator at all */` |
|      - | 5513 | `/*` |
|      - | 5514 | `` * php's port half is `atoi()` of whatever follows the FIRST colon, and the`` |
|      - | 5515 | ` * colon is looked for in every position but the LAST — which is the whole` |
|      - | 5516 | `` * difference between `127.0.0.1:` (php's "Failed to parse address") and`` |
|      - | 5517 | `` * `127.0.0.1:abc` (a port of 0, i.e. one the OS picks). A re-derivation that`` |
|      - | 5518 | ` * reads digits strictly refuses three addresses php accepts, and one that takes` |
|      - | 5519 | `` * the last colon reads `a:b:c` differently than php does.`` |
|      - | 5520 | ` */` |
|    136 | 5521 | `static int SockParsePort(const char *z,int n)` |
|      4 | 5522 | `{` |
|    140 | 5523 | `	int i = 0,iSign = 1,iVal = 0;` |
|    208 | 5524 | `	while( i < n && (z[i] == ' ' \|\| z[i] == '\t' \|\| z[i] == '\n' \|\| z[i] == '\r'` |
|    136 | 5525 | `	              \|\| z[i] == '\v' \|\| z[i] == '\f') ){` |
|    ! 0 | 5526 | `		i++;` |
|    ! 0 | 5527 | `	}` |
|    140 | 5528 | `	if( i < n && (z[i] == '+' \|\| z[i] == '-') ){` |
|    ! 0 | 5529 | `		iSign = z[i] == '-' ? -1 : 1;` |
|    ! 0 | 5530 | `		i++;` |
|    ! 0 | 5531 | `	}` |
|    672 | 5532 | `	for( ; i < n && z[i] >= '0' && z[i] <= '9' ; i++ ){` |
|    535 | 5533 | `		if( iVal < 1000000000 ){` |
|    535 | 5534 | `			iVal = iVal * 10 + (z[i] - '0');` |
|    266 | 5535 | `		}` |
|    269 | 5536 | `	}` |
|    140 | 5537 | `	return iSign * iVal;` |
|      4 | 5538 | `}` |
|    132 | 5539 | `static int SockParseAddress(const char *zAddr,int nAddr,char *zHost,int nHostBuf,int *pPort,` |
|      - | 5540 | `	const char **pzTransport,int *pnTransport,const char **pzRest,int *pnRest)` |
|      4 | 5541 | `{` |
|    136 | 5542 | `	const char *zRest = zAddr;` |
|    136 | 5543 | `	int nRest = nAddr,i,nHost = -1;` |
|    136 | 5544 | `	*pPort = 0;` |
|    136 | 5545 | `	*pzTransport = "tcp";` |
|    136 | 5546 | `	*pnTransport = 3;` |
|   1008 | 5547 | `	for( i = 0 ; i + 2 < nAddr ; i++ ){` |
|    954 | 5548 | `		if( zAddr[i] == ':' && zAddr[i+1] == '/' && zAddr[i+2] == '/' ){` |
|     82 | 5549 | `			*pzTransport = zAddr;` |
|     82 | 5550 | `			*pnTransport = i;` |
|     82 | 5551 | `			zRest = &zAddr[i+3];` |
|     82 | 5552 | `			nRest = nAddr - i - 3;` |
|     82 | 5553 | `			break;` |
|      - | 5554 | `		}` |
|    440 | 5555 | `	}` |
|    136 | 5556 | `	*pzRest = zRest;` |
|    136 | 5557 | `	*pnRest = nRest;` |
|    136 | 5558 | `	if( *pnTransport != 3 \|\| SyStrnicmp(*pzTransport,"tcp",3) != 0 ){` |
|      3 | 5559 | `		return SOCK_ADDR_TRANSPORT;` |
|      - | 5560 | `	}` |
|   1222 | 5561 | `	for( i = 0 ; i + 1 < nRest ; i++ ){` |
|   1216 | 5562 | `		if( zRest[i] == ':' ){` |
|    128 | 5563 | `			*pPort = SockParsePort(&zRest[i+1],nRest - i - 1);` |
|    128 | 5564 | `			nHost = i;` |
|    128 | 5565 | `			break;` |
|      - | 5566 | `		}` |
|    548 | 5567 | `	}` |
|    134 | 5568 | `	if( nHost < 0 ){` |
|      8 | 5569 | `		return SOCK_ADDR_PARSE;` |
|      - | 5570 | `	}` |
|    128 | 5571 | `	if( nHost >= nHostBuf ){` |
|    ! 0 | 5572 | `		nHost = nHostBuf - 1;` |
|    ! 0 | 5573 | `	}` |
|    128 | 5574 | `	if( nHost > 0 ){` |
|    124 | 5575 | `		SyMemcpy(zRest,zHost,(sxu32)nHost);` |
|     60 | 5576 | `	}` |
|    128 | 5577 | `	zHost[nHost] = 0;` |
|    128 | 5578 | `	return SOCK_ADDR_OK;` |
|     70 | 5579 | `}` |
|      - | 5580 | `/*` |
|      - | 5581 | ` * resource\|false fsockopen(string $hostname, int $port = -1, int &$error_code,` |
|      - | 5582 | ` *                          string &$error_message, ?float $timeout = null)` |
|      - | 5583 | ` * resource\|false stream_socket_client(string $address, int &$error_code,` |
|      - | 5584 | ` *                          string &$error_message, ?float $timeout = null, ...)` |
|      - | 5585 | ` * TCP only (the recorded scope: no ssl://, udp:// or unix:// yet).` |
|      - | 5586 | ` */` |
|     90 | 5587 | `PH7_PRIVATE int PH7_builtin_fsockopen(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      4 | 5588 | `{` |
|     94 | 5589 | `	const char *zFunc = ph7_function_name(pCtx);` |
|     94 | 5590 | `	int bClientForm = (zFunc[0] == 's'); /* stream_socket_client */` |
|     94 | 5591 | `	const char *zRaw,*zAddr,*zTransport,*zRest,*zErr = "";` |
|      - | 5592 | `	char zHost[256],zAddrBuf[352],zShowBuf[384],zMsg[512];` |
|      - | 5593 | `	const char *zShow;` |
|     94 | 5594 | `	int nRaw,nAddr,nShow,nTransport,nRest,iPortArg = -1,iPort = 0,iErrno = 0,iTimeoutMs = 0,rc;` |
|     94 | 5595 | `	int iFlags = PH7_STREAM_CLIENT_CONNECT,bPersist,bConnect;` |
|      - | 5596 | `	ph7_socket sock;` |
|      - | 5597 | `	io_private *pDev;` |
|     94 | 5598 | `	int iArgErrno = bClientForm ? 1 : 2;` |
|     94 | 5599 | `	int iArgErrstr = bClientForm ? 2 : 3;` |
|     94 | 5600 | `	int iArgTimeout = bClientForm ? 3 : 4;` |
|     94 | 5601 | `	phl_stream_ctx *pCtxRes = 0;` |
|      - | 5602 | `	ph7_sockopts sOpt;` |
|      - | 5603 | `	char zBindHost[256];` |
|     94 | 5604 | `	int bThrew = 0;` |
|     94 | 5605 | `	if( nArg < 1 ){` |
|    ! 0 | 5606 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 5607 | `		return PH7_OK;` |
|      - | 5608 | `	}` |
|     94 | 5609 | `	if( bClientForm ){` |
|      - | 5610 | ``		/* php's `?resource $context` — fsockopen()/pfsockopen() have no such`` |
|      - | 5611 | `		 * argument, so only the stream_socket_client() spelling takes one. */` |
|     46 | 5612 | `		pCtxRes = PH7_StreamCtxFromArg(pCtx,nArg,apArg,5,"$context",0,&bThrew);` |
|     46 | 5613 | `		if( bThrew ){` |
|    ! 0 | 5614 | `			return PH7_OK;` |
|      - | 5615 | `		}` |
|     21 | 5616 | `	}` |
|     94 | 5617 | `	zRaw = ph7_value_to_string(apArg[0],&nRaw);` |
|     94 | 5618 | `	if( !bClientForm && nArg > 1 && !ph7_value_is_null(apArg[1]) ){` |
|     49 | 5619 | `		iPortArg = ph7_value_to_int(apArg[1]);` |
|     24 | 5620 | `	}` |
|     94 | 5621 | `	if( bClientForm && nArg > 4 ){` |
|      - | 5622 | `		/* Declared in the signature and read by nothing until now, so the` |
|      - | 5623 | `		 * documented spellings did nothing and their constants were undefined` |
|      - | 5624 | `		 * fatals. */` |
|     26 | 5625 | `		iFlags = (int)ph7_value_to_int64(apArg[4]);` |
|     12 | 5626 | `	}` |
|      - | 5627 | `	/* pfsockopen() IS fsockopen() with this flag; php has no other difference` |
|      - | 5628 | `	 * between them. ASYNC_CONNECT is accepted and changes nothing here, because` |
|      - | 5629 | `	 * the connect() is blocking either way (§7.4 slice-2 (b)) — php reverts a` |
|      - | 5630 | `	 * socket it connected asynchronously to blocking mode too. */` |
|    115 | 5631 | `	bPersist = bClientForm ? (iFlags & PH7_STREAM_CLIENT_PERSISTENT) != 0` |
|     69 | 5632 | `		: (zFunc[0] == 'p');` |
|     94 | 5633 | `	bConnect = bClientForm ? (iFlags & PH7_STREAM_CLIENT_CONNECT) != 0 : 1;` |
|      - | 5634 | `	/* php builds ONE address out of fsockopen()'s two arguments — and only when` |
|      - | 5635 | ``	 * the port is a usable one, which is why `fsockopen($h)` reports the address`` |
|      - | 5636 | `	 * it could not parse rather than connecting to port 0. The address it SHOWS` |
|      - | 5637 | `	 * keeps the port either way. */` |
|     94 | 5638 | `	if( bClientForm \|\| iPortArg <= 0 ){` |
|     46 | 5639 | `		zAddr = zRaw;` |
|     46 | 5640 | `		nAddr = nRaw;` |
|     25 | 5641 | `	}else{` |
|     49 | 5642 | `		nAddr = (int)SyBufferFormat(zAddrBuf,sizeof(zAddrBuf),"%.*s:%d",nRaw,zRaw,iPortArg);` |
|     49 | 5643 | `		zAddr = zAddrBuf;` |
|      - | 5644 | `	}` |
|     94 | 5645 | `	if( bClientForm ){` |
|     46 | 5646 | `		zShow = zRaw;` |
|     46 | 5647 | `		nShow = nRaw;` |
|     25 | 5648 | `	}else{` |
|     49 | 5649 | `		nShow = (int)SyBufferFormat(zShowBuf,sizeof(zShowBuf),"%.*s:%d",nRaw,zRaw,iPortArg);` |
|     49 | 5650 | `		zShow = zShowBuf;` |
|      - | 5651 | `	}` |
|     94 | 5652 | `	rc = SockParseAddress(zAddr,nAddr,zHost,(int)sizeof(zHost),&iPort,&zTransport,&nTransport,` |
|      - | 5653 | `		&zRest,&nRest);` |
|     94 | 5654 | `	if( rc != SOCK_ADDR_OK ){` |
|    ! 0 | 5655 | `		if( rc == SOCK_ADDR_TRANSPORT ){` |
|      - | 5656 | `			/* php's own wording for a transport its build does not carry —` |
|      - | 5657 | `			 * which is what this engine's missing ones ARE (§7.4), and what a` |
|      - | 5658 | `			 * script reading $errstr is written against. This used to spell a` |
|      - | 5659 | `			 * message of PHL's own that no php ever answers. */` |
|    ! 0 | 5660 | `			SyBufferFormat(zMsg,sizeof(zMsg),` |
|      - | 5661 | `				"Unable to find the socket transport \"%.*s\" - did you forget to enable it when you configured PHP?",` |
|    ! 0 | 5662 | `				nTransport,zTransport);` |
|    ! 0 | 5663 | `		}else{` |
|    ! 0 | 5664 | `			SyBufferFormat(zMsg,sizeof(zMsg),"Failed to parse address \"%.*s\"",nRest,zRest);` |
|      - | 5665 | `		}` |
|    ! 0 | 5666 | `		SockAddressFailure(pCtx,apArg,nArg,iArgErrno,iArgErrstr,zShow,nShow,zMsg,0);` |
|    ! 0 | 5667 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 5668 | `		return PH7_OK;` |
|      - | 5669 | `	}` |
|     94 | 5670 | `	if( nArg > iArgTimeout && !ph7_value_is_null(apArg[iArgTimeout]) ){` |
|     82 | 5671 | `		double rTimeout = ph7_value_to_double(apArg[iArgTimeout]);` |
|     82 | 5672 | `		if( rTimeout > 0 ){` |
|     82 | 5673 | `			iTimeoutMs = (int)(rTimeout * 1000);` |
|     39 | 5674 | `		}` |
|     39 | 5675 | `	}` |
|     94 | 5676 | `	if( bPersist ){` |
|      - | 5677 | `		/* A live one for this address IS the answer: php hands the same resource` |
|      - | 5678 | `		 * back rather than opening a second connection to the same peer. */` |
|      - | 5679 | `		char zKey[320];` |
|      - | 5680 | `		io_private *pKept;` |
|     17 | 5681 | `		SockPersistKey(zKey,sizeof(zKey),bClientForm,zAddr,nAddr);` |
|     17 | 5682 | `		pKept = SockPersistFind(pCtx->pVm,zKey);` |
|     17 | 5683 | `		if( pKept ){` |
|      7 | 5684 | `			SockAddressSuccess(pCtx,apArg,nArg,iArgErrno,iArgErrstr);` |
|      7 | 5685 | `			ph7_result_resource(pCtx,pKept);` |
|      7 | 5686 | `			return PH7_OK;` |
|      - | 5687 | `		}` |
|      5 | 5688 | `	}` |
|     88 | 5689 | `	if( !bConnect ){` |
|      - | 5690 | `		/* php creates the socket while CONNECTING it, so a $flags without` |
|      - | 5691 | `		 * STREAM_CLIENT_CONNECT answers a stream with no socket behind it: no` |
|      - | 5692 | `		 * name at either end, reads false, writes 0, already at end of file. */` |
|      3 | 5693 | `		SockAddressSuccess(pCtx,apArg,nArg,iArgErrno,iArgErrstr);` |
|      3 | 5694 | `		pDev = SockWrapSocket(pCtx,PH7_NET_INVALID_SOCKET,zAddr,nAddr);` |
|      3 | 5695 | `		if( pDev == 0 ){` |
|    ! 0 | 5696 | `			ph7_result_bool(pCtx,0);` |
|    ! 0 | 5697 | `			return PH7_OK;` |
|      - | 5698 | `		}` |
|      3 | 5699 | `		pDev->pCtxRes = (void *)pCtxRes;` |
|      3 | 5700 | `		ph7_result_resource(pCtx,pDev);` |
|      3 | 5701 | `		return PH7_OK;` |
|      - | 5702 | `	}` |
|      - | 5703 | `	{` |
|      - | 5704 | ``		/* php reads the `socket` options at the moment it creates the socket:`` |
|      - | 5705 | `		 * so_reuseport and tcp_nodelay are a setsockopt on the fresh one, and` |
|      - | 5706 | `		 * bindto is the LOCAL address it takes before connecting. */` |
|     86 | 5707 | `		const char *zOptErr = 0;` |
|     86 | 5708 | `		if( SockCtxOptions(pCtxRes,&sOpt,zBindHost,(int)sizeof(zBindHost),&zOptErr) != 0 ){` |
|      - | 5709 | `			/* The one option failure php treats as a failed CONNECT rather than` |
|      - | 5710 | `			 * as a warning it can carry on past. */` |
|      3 | 5711 | `			SockAddressFailure(pCtx,apArg,nArg,iArgErrno,iArgErrstr,zShow,nShow,zOptErr,0);` |
|      3 | 5712 | `			ph7_result_bool(pCtx,0);` |
|      3 | 5713 | `			return PH7_OK;` |
|      - | 5714 | `		}` |
|      - | 5715 | `	}` |
|     84 | 5716 | `	sock = PH7_NetConnect(zHost,iPort,iTimeoutMs,&sOpt,&iErrno,&zErr);` |
|     84 | 5717 | `	if( sOpt.iBindErr ){` |
|      - | 5718 | `		/* php's own wording, and NEITHER shape stops the connection: the socket` |
|      - | 5719 | `		 * goes out from wherever the routing table would have sent it. It tells` |
|      - | 5720 | `		 * the two apart — a local address that is not a numeric literal at all` |
|      - | 5721 | `		 * names the host, one the OS refused to BIND names the address it tried` |
|      - | 5722 | `		 * and the reason. */` |
|      9 | 5723 | `		if( sOpt.iBindErr == PH7_SOCKOPT_BIND_RESOLVE ){` |
|      5 | 5724 | `			ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,"Invalid IP Address: %s",` |
|      4 | 5725 | `				sOpt.zBindHost ? sOpt.zBindHost : "");` |
|      3 | 5726 | `		}else{` |
|      - | 5727 | `			/* php RE-COMPOSES the address it tried from the parts it parsed, so` |
|      - | 5728 | ``			 * the quoted spelling is canonical: a `bindto` of "192.0.2.1:007"`` |
|      - | 5729 | `			 * is reported as '192.0.2.1:7'. */` |
|      5 | 5730 | `			ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,` |
|      - | 5731 | `				"Failed to bind to '%s:%d', system said: %s",` |
|      4 | 5732 | `				sOpt.zBindHost ? sOpt.zBindHost : "",sOpt.iBindPort,` |
|      2 | 5733 | `				PH7_NetStrError(sOpt.iBindErrno));` |
|      - | 5734 | `		}` |
|      4 | 5735 | `	}` |
|     84 | 5736 | `	if( sock == PH7_NET_INVALID_SOCKET ){` |
|     37 | 5737 | `		if( iErrno == PH7_NET_ERR_RESOLVE ){` |
|      3 | 5738 | `			zErr = SockResolveFailure(pCtx,zHost,zMsg,(int)sizeof(zMsg));` |
|      3 | 5739 | `			iErrno = 0;` |
|      1 | 5740 | `		}` |
|     37 | 5741 | `		SockAddressFailure(pCtx,apArg,nArg,iArgErrno,iArgErrstr,zShow,nShow,zErr,iErrno);` |
|     37 | 5742 | `		ph7_result_bool(pCtx,0);` |
|     37 | 5743 | `		return PH7_OK;` |
|      - | 5744 | `	}` |
|     47 | 5745 | `	SockAddressSuccess(pCtx,apArg,nArg,iArgErrno,iArgErrstr);` |
|      - | 5746 | `	/* Wrap the socket in an io_private so the whole f* family works on it. php` |
|      - | 5747 | ``	 * reports the ADDRESS it opened as the handle's `uri`, which is the same`` |
|      - | 5748 | `	 * one-address-out-of-two-arguments composition it connected through — so an` |
|      - | 5749 | `	 * argument naming only a host still records the port beside it. */` |
|     47 | 5750 | `	pDev = SockWrapSocket(pCtx,sock,zAddr,nAddr);` |
|     47 | 5751 | `	if( pDev == 0 ){` |
|    ! 0 | 5752 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 5753 | `		return PH7_OK;` |
|      - | 5754 | `	}` |
|      - | 5755 | `	/* php attaches the opener's context to a TRANSPORT stream and to nothing` |
|      - | 5756 | `	 * else — which is why stream_context_get_options() answers for a socket and` |
|      - | 5757 | `	 * answers the empty set for a file opened through the very same call. */` |
|     47 | 5758 | `	pDev->pCtxRes = (void *)pCtxRes;` |
|     47 | 5759 | `	SockArmDefaultTimeout(pCtx,pDev);` |
|     47 | 5760 | `	if( bPersist ){` |
|      - | 5761 | `		char zKey[320];` |
|     11 | 5762 | `		SockPersistKey(zKey,sizeof(zKey),bClientForm,zAddr,nAddr);` |
|     11 | 5763 | `		SockPersistKeep(pCtx->pVm,zKey,pDev);` |
|      - | 5764 | `		/* get_resource_type() names it apart, which is how a script can tell it` |
|      - | 5765 | `		 * asked for one at all. */` |
|     11 | 5766 | `		pDev->bPersist = 1;` |
|      5 | 5767 | `	}` |
|     47 | 5768 | `	ph7_result_resource(pCtx,pDev);` |
|     47 | 5769 | `	return PH7_OK;` |
|     49 | 5770 | `}` |
|      - | 5771 | `/*` |
|      - | 5772 | ` * resource\|false stream_socket_server(string $address, int &$error_code,` |
|      - | 5773 | ` *                    string &$error_message, int $flags = STREAM_SERVER_BIND\|STREAM_SERVER_LISTEN,` |
|      - | 5774 | ` *                    ?resource $context = null)` |
|      - | 5775 | ` *` |
|      - | 5776 | ` * The name a php program becomes a SERVER through, and a loud` |
|      - | 5777 | `` * `Call to undefined function` until now — so a script that listens on a port`` |
|      - | 5778 | ` * (a test double, a job runner, a line protocol) could not be spelled at all,` |
|      - | 5779 | ` * even though net.c had bind() and listen() all along.` |
|      - | 5780 | ` *` |
|      - | 5781 | ` * php's two flags are separate for a reason: BIND alone is what a datagram` |
|      - | 5782 | ` * socket wants (there is nothing to listen for), so LISTEN is what makes the` |
|      - | 5783 | ` * socket a stream server. Dropping LISTEN from a tcp:// address is therefore` |
|      - | 5784 | ` * a bound socket nothing can connect to, which is exactly what php answers.` |
|      - | 5785 | ` */` |
|     42 | 5786 | `PH7_PRIVATE int PH7_builtin_stream_socket_server(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      4 | 5787 | `{` |
|     46 | 5788 | `	const char *zAddr,*zTransport,*zRest,*zErr = "";` |
|      - | 5789 | `	char zHost[256];` |
|     46 | 5790 | `	int nAddr,nTransport,nRest,iPort = -1,iErrno = 0,iFlags,rc;` |
|      - | 5791 | `	ph7_socket sock;` |
|      - | 5792 | `	io_private *pDev;` |
|      - | 5793 | `	phl_stream_ctx *pCtxRes;` |
|      - | 5794 | `	ph7_sockopts sOpt;` |
|      - | 5795 | `	char zBindHost[256];` |
|     46 | 5796 | `	int bThrew = 0;` |
|     46 | 5797 | `	if( nArg < 1 ){` |
|    ! 0 | 5798 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 5799 | `		return PH7_OK;` |
|      - | 5800 | `	}` |
|     46 | 5801 | `	pCtxRes = PH7_StreamCtxFromArg(pCtx,nArg,apArg,4,"$context",0,&bThrew);` |
|     46 | 5802 | `	if( bThrew ){` |
|    ! 0 | 5803 | `		return PH7_OK;` |
|      - | 5804 | `	}` |
|      - | 5805 | ``	/* The signature row declares `string $address`, so whatever arrives has`` |
|      - | 5806 | `	 * already been screened; php's own ZPP then CASTS it, and refusing an int` |
|      - | 5807 | `` 	 * here would answer false in silence for `stream_socket_server(8080)` `` |
|      - | 5808 | `	 * where php reports the address it could not parse. */` |
|     46 | 5809 | `	zAddr = ph7_value_to_string(apArg[0],&nAddr);` |
|     30 | 5810 | `	iFlags = nArg > 3 ? (int)ph7_value_to_int64(apArg[3])` |
|     26 | 5811 | `		: (PH7_STREAM_SERVER_BIND\|PH7_STREAM_SERVER_LISTEN);` |
|     46 | 5812 | `	rc = SockParseAddress(zAddr,nAddr,zHost,(int)sizeof(zHost),&iPort,&zTransport,&nTransport,` |
|      - | 5813 | `		&zRest,&nRest);` |
|     46 | 5814 | `	if( rc != SOCK_ADDR_OK ){` |
|      - | 5815 | `		char zMsg[512];` |
|     10 | 5816 | `		if( rc == SOCK_ADDR_TRANSPORT ){` |
|      - | 5817 | `			/* php's own wording for a transport its build has not got, which is` |
|      - | 5818 | `			 * what udp://, unix:// and ssl:// are here (§7.4). */` |
|      4 | 5819 | `			SyBufferFormat(zMsg,sizeof(zMsg),` |
|      - | 5820 | `				"Unable to find the socket transport \"%.*s\" - did you forget to enable it when you configured PHP?",` |
|      1 | 5821 | `				nTransport,zTransport);` |
|      2 | 5822 | `		}else{` |
|      8 | 5823 | `			SyBufferFormat(zMsg,sizeof(zMsg),"Failed to parse address \"%.*s\"",nRest,zRest);` |
|      - | 5824 | `		}` |
|     10 | 5825 | `		SockAddressFailure(pCtx,apArg,nArg,1,2,zAddr,nAddr,zMsg,0);` |
|     10 | 5826 | `		ph7_result_bool(pCtx,0);` |
|     10 | 5827 | `		return PH7_OK;` |
|      - | 5828 | `	}` |
|     38 | 5829 | `	if( (iFlags & PH7_STREAM_SERVER_BIND) == 0 ){` |
|      - | 5830 | `		/* php creates the socket while BINDING it, so a $flags without` |
|      - | 5831 | `		 * STREAM_SERVER_BIND answers a socket stream with no socket behind it:` |
|      - | 5832 | `		 * it has no name, reads false, writes 0 and is already at end of file.` |
|      - | 5833 | ``		 * It does not even resolve the host — `stream_socket_server(':1', $e,`` |
|      - | 5834 | ``		 * $es, 0)` is a resource in php. */`` |
|      5 | 5835 | `		pDev = SockWrapSocket(pCtx,PH7_NET_INVALID_SOCKET,zAddr,nAddr);` |
|      5 | 5836 | `		if( pDev == 0 ){` |
|    ! 0 | 5837 | `			ph7_result_bool(pCtx,0);` |
|    ! 0 | 5838 | `			return PH7_OK;` |
|      - | 5839 | `		}` |
|      5 | 5840 | `		pDev->pCtxRes = (void *)pCtxRes;` |
|      5 | 5841 | `		SockAddressSuccess(pCtx,apArg,nArg,1,2);` |
|      5 | 5842 | `		ph7_result_resource(pCtx,pDev);` |
|      5 | 5843 | `		return PH7_OK;` |
|      - | 5844 | `	}` |
|     34 | 5845 | `	if( zHost[0] == 0 ){` |
|      - | 5846 | ``		/* An address with no host at all (`:8080`) is a name php asks the`` |
|      - | 5847 | `		 * resolver about and is refused for — NOT a wildcard bind. Answering` |
|      - | 5848 | `		 * 0.0.0.0 for it would put a listener on every interface of the` |
|      - | 5849 | `		 * machine, which is the unsafe direction. */` |
|      - | 5850 | `		char zMsg[512];` |
|      3 | 5851 | `		SockAddressFailure(pCtx,apArg,nArg,1,2,zAddr,nAddr,` |
|      1 | 5852 | `			SockResolveFailure(pCtx,zHost,zMsg,(int)sizeof(zMsg)),0);` |
|      2 | 5853 | `		ph7_result_bool(pCtx,0);` |
|      2 | 5854 | `		return PH7_OK;` |
|      - | 5855 | `	}` |
|      - | 5856 | `	{` |
|      - | 5857 | ``		/* The server half reads `backlog`, `so_reuseport` and `tcp_nodelay`;`` |
|      - | 5858 | ``		 * `bindto` is not one of its options, because the address argument IS`` |
|      - | 5859 | `		 * where a server binds (php ignores it here too). */` |
|     32 | 5860 | `		const char *zOptErr = 0;` |
|     32 | 5861 | `		if( SockCtxOptions(pCtxRes,&sOpt,zBindHost,(int)sizeof(zBindHost),&zOptErr) != 0 ){` |
|    ! 0 | 5862 | `			SockAddressFailure(pCtx,apArg,nArg,1,2,zAddr,nAddr,zOptErr,0);` |
|    ! 0 | 5863 | `			ph7_result_bool(pCtx,0);` |
|    ! 0 | 5864 | `			return PH7_OK;` |
|      - | 5865 | `		}` |
|     32 | 5866 | `		sOpt.zBindHost = 0;` |
|      - | 5867 | `	}` |
|     32 | 5868 | `	sock = PH7_NetBind(zHost,iPort,0,(iFlags & PH7_STREAM_SERVER_LISTEN) != 0,` |
|      - | 5869 | `		SOCK_LISTEN_BACKLOG,&sOpt,&iErrno,&zErr);` |
|     32 | 5870 | `	if( sock == PH7_NET_INVALID_SOCKET ){` |
|      - | 5871 | `		char zMsg[512];` |
|      5 | 5872 | `		if( iErrno == PH7_NET_ERR_RESOLVE ){` |
|      3 | 5873 | `			zErr = SockResolveFailure(pCtx,zHost,zMsg,(int)sizeof(zMsg));` |
|      1 | 5874 | `		}` |
|      - | 5875 | `		/* php reports no OS code for a refused ADDRESS — only a connect() that` |
|      - | 5876 | `		 * reached the network carries one — so this stays 0 for every arm. */` |
|      5 | 5877 | `		SockAddressFailure(pCtx,apArg,nArg,1,2,zAddr,nAddr,zErr,0);` |
|      5 | 5878 | `		ph7_result_bool(pCtx,0);` |
|      5 | 5879 | `		return PH7_OK;` |
|      - | 5880 | `	}` |
|     27 | 5881 | `	SockAddressSuccess(pCtx,apArg,nArg,1,2);` |
|     27 | 5882 | `	pDev = SockWrapSocket(pCtx,sock,zAddr,nAddr);` |
|     27 | 5883 | `	if( pDev == 0 ){` |
|    ! 0 | 5884 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 5885 | `		return PH7_OK;` |
|      - | 5886 | `	}` |
|     27 | 5887 | `	pDev->pCtxRes = (void *)pCtxRes;` |
|     27 | 5888 | `	ph7_result_resource(pCtx,pDev);` |
|     27 | 5889 | `	return PH7_OK;` |
|     25 | 5890 | `}` |
|      - | 5891 | `/*` |
|      - | 5892 | ` * resource\|false stream_socket_accept(resource $socket, ?float $timeout = null,` |
|      - | 5893 | ` *                                    string &$peer_name = null)` |
|      - | 5894 | ` *` |
|      - | 5895 | ` * The other half of a server, and the one with the timing in it. php waits at` |
|      - | 5896 | `` * most `default_socket_timeout` seconds by default — NOT forever — and reports`` |
|      - | 5897 | ` * an expired wait as a warning plus false, which is what lets a single-threaded` |
|      - | 5898 | ` * server do something else between connections. A negative timeout blocks.` |
|      - | 5899 | ` */` |
|     32 | 5900 | `PH7_PRIVATE int PH7_builtin_stream_socket_accept(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      4 | 5901 | `{` |
|      - | 5902 | `	io_private *pDev,*pOut;` |
|      - | 5903 | `	ph7_socket *pSock,sock;` |
|      - | 5904 | `	char zPeer[128];` |
|     36 | 5905 | `	int rc,bTimedOut = 0,iTimeoutMs;` |
|     36 | 5906 | `	if( nArg < 1 ){` |
|    ! 0 | 5907 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 5908 | `		return PH7_OK;` |
|      - | 5909 | `	}` |
|     36 | 5910 | `	pDev = StreamSettingArgNamed(pCtx,apArg[0],1,"socket",&rc);` |
|     36 | 5911 | `	if( pDev == 0 ){` |
|      3 | 5912 | `		return rc;` |
|      - | 5913 | `	}` |
|     34 | 5914 | `	pSock = IoPrivateSocket(pDev);` |
|     34 | 5915 | `	if( pSock == 0 ){` |
|      - | 5916 | `		/* Not a socket at all. php's own answer for it reads oddly and is what` |
|      - | 5917 | `		 * a script sees: the accept never reaches the network, so there is no` |
|      - | 5918 | `		 * OS error to report and php asks its error table for code 0. */` |
|      3 | 5919 | `		ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"Accept failed: Unknown error");` |
|      3 | 5920 | `		ph7_result_bool(pCtx,0);` |
|      3 | 5921 | `		return PH7_OK;` |
|      - | 5922 | `	}` |
|     31 | 5923 | `	if( nArg > 1 && !ph7_value_is_null(apArg[1]) ){` |
|     31 | 5924 | `		double rTimeout = ph7_value_to_double(apArg[1]);` |
|     31 | 5925 | `		iTimeoutMs = rTimeout < 0 ? -1 : (int)(rTimeout * 1000);` |
|     17 | 5926 | `	}else{` |
|    ! 0 | 5927 | `		iTimeoutMs = (int)(PH7_VmIniGetInt(pCtx->pVm,"default_socket_timeout",60) * 1000);` |
|    ! 0 | 5928 | `		if( iTimeoutMs < 0 ){` |
|    ! 0 | 5929 | `			iTimeoutMs = -1;` |
|    ! 0 | 5930 | `		}` |
|      - | 5931 | `	}` |
|     31 | 5932 | `	sock = PH7_NetAcceptTimed(*pSock,iTimeoutMs,&bTimedOut,zPeer,(int)sizeof(zPeer));` |
|     31 | 5933 | `	if( *pSock == PH7_NET_INVALID_SOCKET ){` |
|      - | 5934 | `		/* php waits and then reports the expiry; there is nothing to wait on. */` |
|      3 | 5935 | `		bTimedOut = 1;` |
|      1 | 5936 | `	}` |
|     31 | 5937 | `	if( sock == PH7_NET_INVALID_SOCKET ){` |
|      8 | 5938 | `		ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,"Accept failed: %s",` |
|      4 | 5939 | `			bTimedOut ? "Connection timed out" : PH7_NetStrError(PH7_NetLastError()));` |
|      6 | 5940 | `		ph7_result_bool(pCtx,0);` |
|      6 | 5941 | `		return PH7_OK;` |
|      - | 5942 | `	}` |
|     27 | 5943 | `	if( nArg > 2 ){` |
|      3 | 5944 | `		ph7_value *pTmp = ph7_context_new_scalar(pCtx);` |
|      3 | 5945 | `		if( pTmp ){` |
|      3 | 5946 | `			ph7_value_string(pTmp,zPeer,-1);` |
|      3 | 5947 | `			PH7_VmStoreArgByRef(pCtx->pVm,apArg[2],pTmp);` |
|      1 | 5948 | `		}` |
|      1 | 5949 | `	}` |
|      - | 5950 | ``	/* php reports no `uri` for an ACCEPTED connection: nothing opened it by`` |
|      - | 5951 | `	 * name, so stream_get_meta_data() has no address to answer with. */` |
|     27 | 5952 | `	pOut = SockWrapSocket(pCtx,sock,0,0);` |
|     27 | 5953 | `	if( pOut == 0 ){` |
|    ! 0 | 5954 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 5955 | `		return PH7_OK;` |
|      - | 5956 | `	}` |
|     27 | 5957 | `	SockArmDefaultTimeout(pCtx,pOut);` |
|     27 | 5958 | `	ph7_result_resource(pCtx,pOut);` |
|     27 | 5959 | `	return PH7_OK;` |
|     20 | 5960 | `}` |
|      - | 5961 | `/*` |
|      - | 5962 | `` * recvfrom()'s `&$address`: the sender for a datagram, empty for a connected`` |
|      - | 5963 | ` * stream that has none, and NULL for a read that did not happen — php writes it` |
|      - | 5964 | ` * on every call rather than leaving the caller's previous value in place.` |
|      - | 5965 | ` */` |
|     10 | 5966 | `static void SockStoreAddress(ph7_context *pCtx,ph7_value **apArg,int nArg,int iArg,const char *zAddr)` |
|      1 | 5967 | `{` |
|      - | 5968 | `	ph7_value *pTmp;` |
|     11 | 5969 | `	if( iArg >= nArg ){` |
|      9 | 5970 | `		return;` |
|      - | 5971 | `	}` |
|      3 | 5972 | `	pTmp = ph7_context_new_scalar(pCtx);` |
|      3 | 5973 | `	if( pTmp == 0 ){` |
|    ! 0 | 5974 | `		return;` |
|      - | 5975 | `	}` |
|      3 | 5976 | `	if( zAddr ){` |
|      3 | 5977 | `		ph7_value_string(pTmp,zAddr,-1);` |
|      2 | 5978 | `	}else{` |
|    ! 0 | 5979 | `		ph7_value_null(pTmp);` |
|      - | 5980 | `	}` |
|      3 | 5981 | `	PH7_VmStoreArgByRef(pCtx->pVm,apArg[iArg],pTmp);` |
|      6 | 5982 | `}` |
|      - | 5983 | `/*` |
|      - | 5984 | ` * bool stream_socket_shutdown(resource $stream, int $mode)` |
|      - | 5985 | ` *` |
|      - | 5986 | ` * The half-close: "I am done SENDING" without closing a handle the program` |
|      - | 5987 | ` * still wants to read from, which is how every request/response protocol tells` |
|      - | 5988 | ` * its peer the request is over. Nothing else can say it — fclose() takes the` |
|      - | 5989 | ` * read side with it.` |
|      - | 5990 | ` */` |
|      8 | 5991 | `PH7_PRIVATE int PH7_builtin_stream_socket_shutdown(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 5992 | `{` |
|      - | 5993 | `	io_private *pDev;` |
|      - | 5994 | `	ph7_socket *pSock;` |
|      - | 5995 | `	ph7_int64 iHow;` |
|      - | 5996 | `	int rc;` |
|      9 | 5997 | `	if( nArg < 2 ){` |
|    ! 0 | 5998 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 5999 | `		return PH7_OK;` |
|      - | 6000 | `	}` |
|      9 | 6001 | `	pDev = StreamSettingArgNamed(pCtx,apArg[0],1,"stream",&rc);` |
|      9 | 6002 | `	if( pDev == 0 ){` |
|    ! 0 | 6003 | `		return rc;` |
|      - | 6004 | `	}` |
|      9 | 6005 | `	iHow = ph7_value_to_int64(apArg[1]);` |
|      9 | 6006 | `	if( iHow != PH7_STREAM_SHUT_RD && iHow != PH7_STREAM_SHUT_WR && iHow != PH7_STREAM_SHUT_RDWR ){` |
|      - | 6007 | `		/* php names the three constants rather than the numbers behind them. */` |
|      4 | 6008 | `		return PH7_VmThrowException(pCtx,"ValueError",` |
|      - | 6009 | `			"%s(): Argument #2 ($mode) must be one of STREAM_SHUT_RD, STREAM_SHUT_WR, or STREAM_SHUT_RDWR",` |
|      1 | 6010 | `			ph7_function_name(pCtx));` |
|      - | 6011 | `	}` |
|      7 | 6012 | `	pSock = IoPrivateSocket(pDev);` |
|      7 | 6013 | `	if( pSock == 0 \|\| *pSock == PH7_NET_INVALID_SOCKET ){` |
|      - | 6014 | `		/* Not a socket: php answers false in silence, since there is no` |
|      - | 6015 | `		 * direction to shut down. */` |
|      3 | 6016 | `		ph7_result_bool(pCtx,0);` |
|      3 | 6017 | `		return PH7_OK;` |
|      - | 6018 | `	}` |
|      5 | 6019 | `	rc = PH7_NetShutdown(*pSock,(int)iHow) == PH7_OK;` |
|      5 | 6020 | `	if( rc && iHow != PH7_STREAM_SHUT_WR && PH7_NetAtEnd(*pSock) ){` |
|      - | 6021 | `		/* The read side is gone AND nothing is queued behind it, so this handle` |
|      - | 6022 | `		 * is at its end: php answers feof() for a socket by probing it, and a` |
|      - | 6023 | ``		 * `while (!feof($s))` drain loop after a half-close would otherwise spin`` |
|      - | 6024 | `		 * on a stream that can never answer again. Bytes that HAD arrived are` |
|      - | 6025 | `		 * still handed over — which is why the answer is probed rather than` |
|      - | 6026 | `		 * assumed, and why the device's own latch stays clear. */` |
|      3 | 6027 | `		pDev->bEof = 1;` |
|      1 | 6028 | `	}` |
|      5 | 6029 | `	ph7_result_bool(pCtx,rc);` |
|      5 | 6030 | `	return PH7_OK;` |
|      5 | 6031 | `}` |
|      - | 6032 | `/*` |
|      - | 6033 | ` * string\|false stream_socket_recvfrom(resource $socket, int $length, int $flags = 0,` |
|      - | 6034 | ` *                                    string &$address = null)` |
|      - | 6035 | ` * int\|false stream_socket_sendto(resource $socket, string $data, int $flags = 0,` |
|      - | 6036 | ` *                               string $address = "")` |
|      - | 6037 | ` *` |
|      - | 6038 | ` * The pair that reaches the socket UNDERNEATH the stream: php's own asks the` |
|      - | 6039 | `` * socket rather than the handle's read buffer, which is why `STREAM_PEEK` can`` |
|      - | 6040 | ` * look at bytes without consuming them (nothing else in the family can) and why` |
|      - | 6041 | ` * a recvfrom() on a handle a line read has already buffered WAITS for more.` |
|      - | 6042 | `` * The `$address` is what a datagram carries and a connected stream does not.`` |
|      - | 6043 | ` */` |
|     12 | 6044 | `PH7_PRIVATE int PH7_builtin_stream_socket_recvfrom(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 6045 | `{` |
|      - | 6046 | `	io_private *pDev;` |
|      - | 6047 | `	ph7_socket *pSock;` |
|      - | 6048 | `	ph7_int64 nLen;` |
|      - | 6049 | `	char zAddr[128],*zBuf;` |
|     13 | 6050 | `	int rc,iFlags = 0,n;` |
|     13 | 6051 | `	if( nArg < 2 ){` |
|    ! 0 | 6052 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 6053 | `		return PH7_OK;` |
|      - | 6054 | `	}` |
|     13 | 6055 | `	pDev = StreamSettingArgNamed(pCtx,apArg[0],1,"socket",&rc);` |
|     13 | 6056 | `	if( pDev == 0 ){` |
|    ! 0 | 6057 | `		return rc;` |
|      - | 6058 | `	}` |
|     13 | 6059 | `	nLen = ph7_value_to_int64(apArg[1]);` |
|     13 | 6060 | `	if( nLen < 1 ){` |
|      4 | 6061 | `		return PH7_VmThrowException(pCtx,"ValueError",` |
|      1 | 6062 | `			"%s(): Argument #2 ($length) must be greater than 0",ph7_function_name(pCtx));` |
|      - | 6063 | `	}` |
|     11 | 6064 | `	if( nArg > 2 ){` |
|      3 | 6065 | `		iFlags = (int)ph7_value_to_int64(apArg[2]);` |
|      1 | 6066 | `	}` |
|     11 | 6067 | `	pSock = IoPrivateSocket(pDev);` |
|     11 | 6068 | `	if( pSock == 0 \|\| *pSock == PH7_NET_INVALID_SOCKET ){` |
|      3 | 6069 | `		SockStoreAddress(pCtx,apArg,nArg,3,0);` |
|      3 | 6070 | `		ph7_result_bool(pCtx,0);` |
|      3 | 6071 | `		return PH7_OK;` |
|      - | 6072 | `	}` |
|      9 | 6073 | `	if( nLen > 0x7FFFFFF0 ){` |
|    ! 0 | 6074 | `		nLen = 0x7FFFFFF0;` |
|    ! 0 | 6075 | `	}` |
|      9 | 6076 | `	zBuf = (char *)ph7_context_alloc_chunk(pCtx,(unsigned int)nLen,FALSE,TRUE);` |
|      9 | 6077 | `	if( zBuf == 0 ){` |
|    ! 0 | 6078 | `		return PH7_ContextMemoryError(pCtx);` |
|      - | 6079 | `	}` |
|      9 | 6080 | `	zAddr[0] = 0;` |
|      9 | 6081 | `	n = PH7_NetRecvFrom(*pSock,zBuf,(int)nLen,iFlags,zAddr,(int)sizeof(zAddr));` |
|      - | 6082 | `	/* php writes the out-param on every call: the sender's address for a read` |
|      - | 6083 | `	 * that happened (empty for a connected stream, which has none to report) and` |
|      - | 6084 | `	 * NULL for one that did not — never the caller's previous value. */` |
|      9 | 6085 | `	SockStoreAddress(pCtx,apArg,nArg,3,n < 0 ? 0 : zAddr);` |
|      9 | 6086 | `	if( n < 0 ){` |
|    ! 0 | 6087 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 6088 | `	}else{` |
|      9 | 6089 | `		ph7_result_string(pCtx,zBuf,n);` |
|      - | 6090 | `	}` |
|      9 | 6091 | `	ph7_context_free_chunk(pCtx,zBuf);` |
|      9 | 6092 | `	return PH7_OK;` |
|      7 | 6093 | `}` |
|     12 | 6094 | `PH7_PRIVATE int PH7_builtin_stream_socket_sendto(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 6095 | `{` |
|      - | 6096 | `	io_private *pDev;` |
|      - | 6097 | `	ph7_socket *pSock;` |
|     13 | 6098 | `	const char *zData,*zSentTo = "";` |
|      - | 6099 | `	char zHost[256];` |
|     13 | 6100 | `	int rc,iFlags = 0,nData,n,iPort = 0,nSentTo = 0,iErr = 0;` |
|     13 | 6101 | `	if( nArg < 2 ){` |
|    ! 0 | 6102 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 6103 | `		return PH7_OK;` |
|      - | 6104 | `	}` |
|     13 | 6105 | `	pDev = StreamSettingArgNamed(pCtx,apArg[0],1,"socket",&rc);` |
|     13 | 6106 | `	if( pDev == 0 ){` |
|    ! 0 | 6107 | `		return rc;` |
|      - | 6108 | `	}` |
|     13 | 6109 | `	zData = ph7_value_to_string(apArg[1],&nData);` |
|     13 | 6110 | `	if( nArg > 2 ){` |
|      7 | 6111 | `		iFlags = (int)ph7_value_to_int64(apArg[2]);` |
|      3 | 6112 | `	}` |
|     13 | 6113 | `	zHost[0] = 0;` |
|     13 | 6114 | `	if( nArg > 3 && ph7_value_is_string(apArg[3]) ){` |
|      7 | 6115 | `		int nAddr,i,nHost = -1;` |
|      7 | 6116 | `		const char *zAddr = ph7_value_to_string(apArg[3],&nAddr);` |
|      7 | 6117 | `		if( nAddr > 0 ){` |
|      - | 6118 | `			/* php parses THIS address without looking for a transport at all —` |
|      - | 6119 | ``			 * the first colon is the separator, so `udp://1.2.3.4:53` names the`` |
|      - | 6120 | `			 * host "udp" — and an address it cannot turn into a sockaddr is a` |
|      - | 6121 | `			 * refusal rather than a send to the connected peer, which is where` |
|      - | 6122 | `			 * the bytes would otherwise silently go. */` |
|     49 | 6123 | `			for( i = 0 ; i + 1 < nAddr ; i++ ){` |
|     45 | 6124 | `				if( zAddr[i] == ':' ){` |
|      3 | 6125 | `					iPort = SockParsePort(&zAddr[i+1],nAddr - i - 1);` |
|      3 | 6126 | `					nHost = i;` |
|      3 | 6127 | `					break;` |
|      - | 6128 | `				}` |
|     22 | 6129 | `			}` |
|      7 | 6130 | `			if( nHost < 0 ){` |
|      7 | 6131 | `				ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,` |
|      2 | 6132 | ``					"Failed to parse `%.*s' into a valid network address",nAddr,zAddr);`` |
|      5 | 6133 | `				ph7_result_bool(pCtx,0);` |
|      5 | 6134 | `				return PH7_OK;` |
|      - | 6135 | `			}` |
|      3 | 6136 | `			if( nHost >= (int)sizeof(zHost) ){` |
|    ! 0 | 6137 | `				nHost = (int)sizeof(zHost) - 1;` |
|    ! 0 | 6138 | `			}` |
|      3 | 6139 | `			if( nHost > 0 ){` |
|      3 | 6140 | `				SyMemcpy(zAddr,zHost,(sxu32)nHost);` |
|      1 | 6141 | `			}` |
|      3 | 6142 | `			zHost[nHost] = 0;` |
|      3 | 6143 | `			zSentTo = zAddr;` |
|      3 | 6144 | `			nSentTo = nAddr;` |
|      1 | 6145 | `		}` |
|      1 | 6146 | `	}` |
|      9 | 6147 | `	pSock = IoPrivateSocket(pDev);` |
|      9 | 6148 | `	if( pSock == 0 \|\| *pSock == PH7_NET_INVALID_SOCKET ){` |
|      - | 6149 | `		/* php answers -1 here rather than false: this one reports the send()` |
|      - | 6150 | `		 * result, and it never made a call. */` |
|      3 | 6151 | `		ph7_result_int(pCtx,-1);` |
|      3 | 6152 | `		return PH7_OK;` |
|      - | 6153 | `	}` |
|      7 | 6154 | `	n = PH7_NetSendTo(*pSock,(const void *)zData,nData,iFlags,zHost,iPort,&iErr);` |
|      7 | 6155 | `	if( iErr == PH7_NET_ERR_RESOLVE ){` |
|      - | 6156 | `		/* php says it three times for one failure — the resolver's own text, the` |
|      - | 6157 | `		 * name it could not resolve, and the address it therefore could not` |
|      - | 6158 | `		 * parse — and answers FALSE rather than the -1 a failed send gives. */` |
|      - | 6159 | `		char zMsg[512];` |
|    ! 0 | 6160 | `		SockResolveFailure(pCtx,zHost,zMsg,(int)sizeof(zMsg));` |
|    ! 0 | 6161 | ``		ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,"Failed to resolve `%s': %s",zHost,zMsg);`` |
|    ! 0 | 6162 | `		ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,` |
|    ! 0 | 6163 | ``			"Failed to parse `%.*s' into a valid network address",nSentTo,zSentTo);`` |
|    ! 0 | 6164 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 6165 | `		return PH7_OK;` |
|      - | 6166 | `	}` |
|      7 | 6167 | `	if( n < 0 ){` |
|      - | 6168 | `		/* php reports the OS text and hands back the -1 send() answered — this` |
|      - | 6169 | `		 * one never answers false, which is why a caller compares it against 0` |
|      - | 6170 | `		 * rather than testing it for truth. The trailing newline is php's own. */` |
|      4 | 6171 | `		ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,"%s\n",` |
|      1 | 6172 | `			PH7_NetStrError(PH7_NetLastError()));` |
|      1 | 6173 | `	}` |
|      7 | 6174 | `	ph7_result_int(pCtx,n);` |
|      7 | 6175 | `	return PH7_OK;` |
|      7 | 6176 | `}` |
|      - | 6177 | `/*` |
|      - | 6178 | ` * array\|false stream_socket_pair(int $domain, int $type, int $protocol)` |
|      - | 6179 | ` *` |
|      - | 6180 | ` * Two connected sockets with no address between them — the two-way pipe a` |
|      - | 6181 | ` * program hands a child, or a test double hands the code under test. Which` |
|      - | 6182 | ` * $domain works is the OS's answer and not php's: POSIX has AF_UNIX and refuses` |
|      - | 6183 | ` * AF_INET, and Windows is the other way round (php emulates the pair over the` |
|      - | 6184 | ` * loopback there, and so does this).` |
|      - | 6185 | ` */` |
|      4 | 6186 | `PH7_PRIVATE int PH7_builtin_stream_socket_pair(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 6187 | `{` |
|      - | 6188 | `	ph7_socket aSock[2];` |
|      - | 6189 | `	io_private *apDev[2];` |
|      - | 6190 | `	ph7_value *pArr,*pVal;` |
|      5 | 6191 | `	int iErrno = 0,i;` |
|      5 | 6192 | `	if( nArg < 3 ){` |
|    ! 0 | 6193 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 6194 | `		return PH7_OK;` |
|      - | 6195 | `	}` |
|      6 | 6196 | `	if( PH7_NetSocketPair((int)ph7_value_to_int64(apArg[0]),(int)ph7_value_to_int64(apArg[1]),` |
|      7 | 6197 | `		(int)ph7_value_to_int64(apArg[2]),aSock,&iErrno) != PH7_OK ){` |
|      - | 6198 | `		/* php reports the OS code and its text, in that order and in brackets. */` |
|      4 | 6199 | `		ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,"Failed to create sockets: [%d]: %s",` |
|      1 | 6200 | `			iErrno,PH7_NetStrError(iErrno));` |
|      3 | 6201 | `		ph7_result_bool(pCtx,0);` |
|      3 | 6202 | `		return PH7_OK;` |
|      - | 6203 | `	}` |
|      3 | 6204 | `	pArr = ph7_context_new_array(pCtx);` |
|      3 | 6205 | `	pVal = ph7_context_new_scalar(pCtx);` |
|      3 | 6206 | `	apDev[0] = apDev[1] = 0;` |
|      3 | 6207 | `	if( pArr == 0 \|\| pVal == 0 ){` |
|    ! 0 | 6208 | `		PH7_NetClose(aSock[0]);` |
|    ! 0 | 6209 | `		PH7_NetClose(aSock[1]);` |
|    ! 0 | 6210 | `		return PH7_ContextMemoryError(pCtx);` |
|      - | 6211 | `	}` |
|      7 | 6212 | `	for( i = 0 ; i < 2 ; i++ ){` |
|      - | 6213 | `		/* No uri: nothing opened these by name, which is what php reports. */` |
|      5 | 6214 | `		apDev[i] = SockWrapSocket(pCtx,aSock[i],0,0);` |
|      5 | 6215 | `		if( apDev[i] == 0 ){` |
|      - | 6216 | `			/* SockWrapSocket closed the one it could not wrap; the OTHER end is` |
|      - | 6217 | `			 * still ours to close, wrapped or not. */` |
|    ! 0 | 6218 | `			if( i == 0 ){` |
|    ! 0 | 6219 | `				PH7_NetClose(aSock[1]);` |
|    ! 0 | 6220 | `			}else{` |
|    ! 0 | 6221 | `				SockCloseWrapped(pCtx,apDev[0]);` |
|      - | 6222 | `			}` |
|    ! 0 | 6223 | `			return PH7_ContextMemoryError(pCtx);` |
|      - | 6224 | `		}` |
|      - | 6225 | `		/* A pair has no transport of its own, and php labels it apart from a` |
|      - | 6226 | `		 * tcp:// stream for exactly that reason. */` |
|      5 | 6227 | `		((sock_private *)apDev[i]->pHandle)->bGeneric = 1;` |
|      5 | 6228 | `		SockArmDefaultTimeout(pCtx,apDev[i]);` |
|      3 | 6229 | `	}` |
|      7 | 6230 | `	for( i = 0 ; i < 2 ; i++ ){` |
|      5 | 6231 | `		ph7_value_resource(pVal,apDev[i]);` |
|      5 | 6232 | `		ph7_array_add_elem(pArr,0,pVal);` |
|      3 | 6233 | `	}` |
|      3 | 6234 | `	ph7_result_value(pCtx,pArr);` |
|      3 | 6235 | `	return PH7_OK;` |
|      3 | 6236 | `}` |
|      - | 6237 | `/*` |
|      - | 6238 | ` * string\|false stream_socket_get_name(resource $socket, bool $remote)` |
|      - | 6239 | ` *` |
|      - | 6240 | `` * Which address this socket sits on (`$remote` false) or is talking to (true).`` |
|      - | 6241 | `` * It is the only way to learn the port a server bound with `:0` actually got,`` |
|      - | 6242 | ` * so a test that needs a free port had to guess one without it.` |
|      - | 6243 | ` */` |
|     50 | 6244 | `PH7_PRIVATE int PH7_builtin_stream_socket_get_name(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      4 | 6245 | `{` |
|      - | 6246 | `	io_private *pDev;` |
|      - | 6247 | `	ph7_socket *pSock;` |
|      - | 6248 | `	char zName[128];` |
|      - | 6249 | `	int rc;` |
|     54 | 6250 | `	if( nArg < 2 ){` |
|    ! 0 | 6251 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 6252 | `		return PH7_OK;` |
|      - | 6253 | `	}` |
|     54 | 6254 | `	pDev = StreamSettingArgNamed(pCtx,apArg[0],1,"socket",&rc);` |
|     54 | 6255 | `	if( pDev == 0 ){` |
|      5 | 6256 | `		return rc;` |
|      - | 6257 | `	}` |
|     50 | 6258 | `	pSock = IoPrivateSocket(pDev);` |
|     50 | 6259 | `	if( pSock == 0 \|\| PH7_NetSockName(*pSock,ph7_value_to_bool(apArg[1]),zName,(int)sizeof(zName)) != PH7_OK ){` |
|      - | 6260 | `		/* php answers false for a stream that is not a socket, and for the peer` |
|      - | 6261 | `		 * of a socket that is not connected — an unaccepted server. */` |
|     15 | 6262 | `		ph7_result_bool(pCtx,0);` |
|     15 | 6263 | `		return PH7_OK;` |
|      - | 6264 | `	}` |
|     37 | 6265 | `	ph7_result_string(pCtx,zName,-1);` |
|     37 | 6266 | `	return PH7_OK;` |
|     29 | 6267 | `}` |
|      - | 6268 | `#endif /*` |
|      - | 6269 | ` * The stream SETTINGS family. Every one of these was a loud` |
|      - | 6270 | `` * `Call to undefined function` — so a program that puts a socket in`` |
|      - | 6271 | ` * non-blocking mode, bounds a read with a timeout, or asks whether a stream` |
|      - | 6272 | ` * can be locked before calling flock() did not run at all.` |
|      - | 6273 | ` *` |
|      - | 6274 | ` * The shared preamble: php refuses a non-resource with a TypeError naming the` |
|      - | 6275 | ` * parameter, and an already-closed handle the same way.` |
|      - | 6276 | ` */` |
|    478 | 6277 | `static io_private * StreamSettingArgNamed(ph7_context *pCtx,ph7_value *pArg,int iPos,` |
|      - | 6278 | `	const char *zName,int *pRc)` |
|      5 | 6279 | `{` |
|      - | 6280 | `	io_private *pDev;` |
|    483 | 6281 | `	*pRc = PH7_OK;` |
|    483 | 6282 | `	if( !ph7_value_is_resource(pArg) ){` |
|     20 | 6283 | `		*pRc = PH7_VmThrowException(pCtx,"TypeError",` |
|      - | 6284 | `			"%s(): Argument #%d ($%s) must be of type resource, %s given",` |
|      6 | 6285 | `			ph7_function_name(pCtx),iPos,zName,ph7_type_name(pArg));` |
|     14 | 6286 | `		return 0;` |
|      - | 6287 | `	}` |
|    471 | 6288 | `	pDev = (io_private *)ph7_value_to_resource(pArg);` |
|    471 | 6289 | `	if( IO_PRIVATE_INVALID(pDev) ){` |
|      4 | 6290 | `		*pRc = PH7_VmThrowException(pCtx,"TypeError",` |
|      - | 6291 | `			"%s(): Argument #%d ($%s) must be an open stream resource",` |
|      1 | 6292 | `			ph7_function_name(pCtx),iPos,zName);` |
|      3 | 6293 | `		return 0;` |
|      - | 6294 | `	}` |
|    469 | 6295 | `	return pDev;` |
|    244 | 6296 | `}` |
|      - | 6297 | ``/* The whole settings family names its one handle `$stream`; the copy names two. */`` |
|    120 | 6298 | `static io_private * StreamSettingArg(ph7_context *pCtx,ph7_value *pArg,int *pRc)` |
|      4 | 6299 | `{` |
|    124 | 6300 | `	return StreamSettingArgNamed(pCtx,pArg,1,"stream",pRc);` |
|      4 | 6301 | `}` |
|      - | 6302 | `/* The same screen, for the filter family in vfs_filter.c. */` |
|    170 | 6303 | `PH7_PRIVATE io_private * PH7_StreamHandleArg(ph7_context *pCtx,ph7_value *pArg,int iPos,` |
|      - | 6304 | `	const char *zName,int *pRc)` |
|      2 | 6305 | `{` |
|    172 | 6306 | `	return StreamSettingArgNamed(pCtx,pArg,iPos,zName,pRc);` |
|      2 | 6307 | `}` |
|      - | 6308 | `/* The tcp:// socket behind a handle, or 0 for any other device. */` |
|    292 | 6309 | `static ph7_socket * IoPrivateSocket(io_private *pDev)` |
|      5 | 6310 | `{` |
|      - | 6311 | `#ifdef PH7_ENABLE_NET` |
|    297 | 6312 | `	if( pDev->pStream == &sTCP_Stream && pDev->pHandle ){` |
|    229 | 6313 | `		return &((sock_private *)pDev->pHandle)->sock;` |
|      - | 6314 | `	}` |
|      - | 6315 | `#endif` |
|     33 | 6316 | `	SXUNUSED(pDev); /* cc warning when NET is off */` |
|     70 | 6317 | `	return 0;` |
|    151 | 6318 | `}` |
|      - | 6319 | `/*` |
|      - | 6320 | ` * bool stream_set_blocking(resource $stream, bool $enable)` |
|      - | 6321 | ` *` |
|      - | 6322 | ` * php sets the mode AT the descriptor and answers TRUE either way; a stream` |
|      - | 6323 | ` * with no descriptor — a memory buffer, a data:// payload — keeps reporting` |
|      - | 6324 | ` * itself blocked, which is why the flag is only recorded when it took. On` |
|      - | 6325 | ` * Windows php's plain-files device has no O_NONBLOCK to set, so every such` |
|      - | 6326 | ` * stream (a file, a pipe, php://stdin) answers FALSE there.` |
|      - | 6327 | ` */` |
|     30 | 6328 | `PH7_PRIVATE int PH7_builtin_stream_set_blocking(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      4 | 6329 | `{` |
|      - | 6330 | `	io_private *pDev;` |
|      - | 6331 | `	int rc,bEnable,fd;` |
|      - | 6332 | `	ph7_socket *pSock;` |
|     34 | 6333 | `	if( nArg < 2 ){` |
|    ! 0 | 6334 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 6335 | `		return PH7_OK;` |
|      - | 6336 | `	}` |
|     34 | 6337 | `	pDev = StreamSettingArg(pCtx,apArg[0],&rc);` |
|     34 | 6338 | `	if( pDev == 0 ){` |
|      3 | 6339 | `		return rc;` |
|      - | 6340 | `	}` |
|     32 | 6341 | `	bEnable = ph7_value_to_bool(apArg[1]);` |
|     32 | 6342 | `	pSock = IoPrivateSocket(pDev);` |
|     32 | 6343 | `	if( pSock ){` |
|      - | 6344 | `#ifdef PH7_ENABLE_NET` |
|      9 | 6345 | `		if( *pSock == PH7_NET_INVALID_SOCKET ){` |
|      - | 6346 | `			/* No socket to set the mode on: php's own answer is FALSE, which is` |
|      - | 6347 | `			 * the one place this family reports a setting that did not take. */` |
|      3 | 6348 | `			ph7_result_bool(pCtx,0);` |
|      3 | 6349 | `			return PH7_OK;` |
|      - | 6350 | `		}` |
|      6 | 6351 | `		PH7_NetSetBlocking(*pSock,bEnable);` |
|      - | 6352 | `#endif` |
|      6 | 6353 | `		pDev->bNonBlock = (sxu8)(bEnable ? 0 : 1);` |
|      4 | 6354 | `	}else{` |
|      - | 6355 | `#ifdef __WINNT__` |
|      1 | 6356 | `		if( PH7_StreamIsPlainDevice(pDev) ){` |
|      1 | 6357 | `			ph7_result_bool(pCtx,0);` |
|      1 | 6358 | `			return PH7_OK;` |
|      - | 6359 | `		}` |
|      - | 6360 | `#endif` |
|     23 | 6361 | `		fd = PH7_StreamPosixFd(pDev);` |
|     22 | 6362 | `		if( fd >= 0 ){` |
|      - | 6363 | `#ifndef __WINNT__` |
|     14 | 6364 | `			int iFlags = fcntl(fd,F_GETFL,0);` |
|     14 | 6365 | `			if( iFlags >= 0 ){` |
|     14 | 6366 | `				if( bEnable ){` |
|      4 | 6367 | `					iFlags &= ~O_NONBLOCK;` |
|      2 | 6368 | `				}else{` |
|     10 | 6369 | `					iFlags \|= O_NONBLOCK;` |
|      - | 6370 | `				}` |
|     14 | 6371 | `				if( fcntl(fd,F_SETFL,iFlags) == 0 ){` |
|     14 | 6372 | `					pDev->bNonBlock = (sxu8)(bEnable ? 0 : 1);` |
|      7 | 6373 | `				}` |
|      7 | 6374 | `			}` |
|      - | 6375 | `#endif` |
|      7 | 6376 | `		}` |
|      - | 6377 | `	}` |
|     29 | 6378 | `	ph7_result_bool(pCtx,1);` |
|     29 | 6379 | `	return PH7_OK;` |
|     19 | 6380 | `}` |
|      - | 6381 | `/*` |
|      - | 6382 | ` * bool stream_set_timeout(resource $stream, int $seconds, int $microseconds = 0)` |
|      - | 6383 | ` *` |
|      - | 6384 | ` * php answers TRUE only for a stream whose transport HAS a timeout — a socket —` |
|      - | 6385 | ` * and FALSE for every file, pipe and memory buffer, because there is nothing` |
|      - | 6386 | ` * to wait on. Silently accepting it for a file would tell a caller its read is` |
|      - | 6387 | ` * bounded when it is not.` |
|      - | 6388 | ` */` |
|      6 | 6389 | `PH7_PRIVATE int PH7_builtin_stream_set_timeout(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 6390 | `{` |
|      - | 6391 | `	io_private *pDev;` |
|      - | 6392 | `	ph7_socket *pSock;` |
|      - | 6393 | `	int rc;` |
|      7 | 6394 | `	if( nArg < 2 ){` |
|    ! 0 | 6395 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 6396 | `		return PH7_OK;` |
|      - | 6397 | `	}` |
|      7 | 6398 | `	pDev = StreamSettingArg(pCtx,apArg[0],&rc);` |
|      7 | 6399 | `	if( pDev == 0 ){` |
|    ! 0 | 6400 | `		return rc;` |
|      - | 6401 | `	}` |
|      7 | 6402 | `	pSock = IoPrivateSocket(pDev);` |
|      7 | 6403 | `	if( pSock == 0 ){` |
|      7 | 6404 | `		ph7_result_bool(pCtx,0);` |
|      7 | 6405 | `		return PH7_OK;` |
|      - | 6406 | `	}` |
|      - | 6407 | `#ifdef PH7_ENABLE_NET` |
|      - | 6408 | `	{` |
|    ! 0 | 6409 | `		ph7_int64 iSec = ph7_value_to_int64(apArg[1]);` |
|    ! 0 | 6410 | `		ph7_int64 iUsec = nArg > 2 ? ph7_value_to_int64(apArg[2]) : 0;` |
|    ! 0 | 6411 | `		if( iSec < 0 ){` |
|    ! 0 | 6412 | `			iSec = 0;` |
|    ! 0 | 6413 | `		}` |
|    ! 0 | 6414 | `		if( iUsec < 0 ){` |
|    ! 0 | 6415 | `			iUsec = 0;` |
|    ! 0 | 6416 | `		}` |
|    ! 0 | 6417 | `		PH7_NetSetRwTimeout(*pSock,iSec,iUsec);` |
|      - | 6418 | `		/* An expired read answers FALSE and says so through the metadata;` |
|      - | 6419 | `		 * without the armed flag it is indistinguishable from a non-blocking` |
|      - | 6420 | `		 * one, which answers "". */` |
|    ! 0 | 6421 | `		pDev->bHasTimeout = 1;` |
|    ! 0 | 6422 | `		pDev->bTimedOut = 0;` |
|      - | 6423 | `	}` |
|      - | 6424 | `#endif` |
|    ! 0 | 6425 | `	ph7_result_bool(pCtx,1);` |
|    ! 0 | 6426 | `	return PH7_OK;` |
|      4 | 6427 | `}` |
|      - | 6428 | `/*` |
|      - | 6429 | ` * int stream_set_chunk_size(resource $stream, int $size)` |
|      - | 6430 | ` *` |
|      - | 6431 | ` * Answers the PREVIOUS size, which is what makes the setting restorable, and` |
|      - | 6432 | ` * refuses a non-positive one the way php does.` |
|      - | 6433 | ` */` |
|     44 | 6434 | `PH7_PRIVATE int PH7_builtin_stream_set_chunk_size(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 6435 | `{` |
|      - | 6436 | `	io_private *pDev;` |
|      - | 6437 | `	ph7_int64 nSize;` |
|      - | 6438 | `	int rc;` |
|     45 | 6439 | `	if( nArg < 2 ){` |
|    ! 0 | 6440 | `		ph7_result_int(pCtx,-1);` |
|    ! 0 | 6441 | `		return PH7_OK;` |
|      - | 6442 | `	}` |
|     45 | 6443 | `	pDev = StreamSettingArg(pCtx,apArg[0],&rc);` |
|     45 | 6444 | `	if( pDev == 0 ){` |
|    ! 0 | 6445 | `		return rc;` |
|      - | 6446 | `	}` |
|     45 | 6447 | `	nSize = ph7_value_to_int64(apArg[1]);` |
|     45 | 6448 | `	if( nSize < 1 ){` |
|      5 | 6449 | `		return PH7_VmThrowException(pCtx,"ValueError",` |
|      - | 6450 | `			"stream_set_chunk_size(): Argument #2 ($size) must be greater than 0");` |
|      - | 6451 | `	}` |
|     41 | 6452 | `	if( nSize > (ph7_int64)SXI32_HIGH ){` |
|      - | 6453 | `		/* php's own ceiling: the size is an int on its side, and storing a` |
|      - | 6454 | `		 * larger one made the NEXT call report a size no caller ever set. */` |
|      3 | 6455 | `		return PH7_VmThrowException(pCtx,"ValueError",` |
|      - | 6456 | `			"stream_set_chunk_size(): Argument #2 ($size) is too large");` |
|      - | 6457 | `	}` |
|     39 | 6458 | `	ph7_result_int64(pCtx,(ph7_int64)pDev->nChunk);` |
|     39 | 6459 | `	pDev->nChunk = (sxu32)nSize;` |
|     39 | 6460 | `	return PH7_OK;` |
|     23 | 6461 | `}` |
|      - | 6462 | `/*` |
|      - | 6463 | ` * int stream_set_read_buffer(resource $stream, int $size)` |
|      - | 6464 | ` * int stream_set_write_buffer(resource $stream, int $size)  [set_file_buffer]` |
|      - | 6465 | ` *` |
|      - | 6466 | ` * php's stream layer has no stdio buffer left to hand these to: the read side` |
|      - | 6467 | ` * answers 0 (accepted) and the write side -1 (unsupported), for every stream` |
|      - | 6468 | ` * and every size. Both are still validated arguments, so a bad handle is the` |
|      - | 6469 | ` * same TypeError the rest of the family raises.` |
|      - | 6470 | ` */` |
|      6 | 6471 | `PH7_PRIVATE int PH7_builtin_stream_set_read_buffer(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 6472 | `{` |
|      7 | 6473 | `	int rc = PH7_OK;` |
|      7 | 6474 | `	if( nArg < 2 ){` |
|    ! 0 | 6475 | `		ph7_result_int(pCtx,-1);` |
|    ! 0 | 6476 | `		return PH7_OK;` |
|      - | 6477 | `	}` |
|      7 | 6478 | `	if( StreamSettingArg(pCtx,apArg[0],&rc) == 0 ){` |
|    ! 0 | 6479 | `		return rc;` |
|      - | 6480 | `	}` |
|      7 | 6481 | `	ph7_result_int(pCtx,0);` |
|      7 | 6482 | `	return PH7_OK;` |
|      4 | 6483 | `}` |
|     12 | 6484 | `PH7_PRIVATE int PH7_builtin_stream_set_write_buffer(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 6485 | `{` |
|     13 | 6486 | `	int rc = PH7_OK;` |
|     13 | 6487 | `	if( nArg < 2 ){` |
|    ! 0 | 6488 | `		ph7_result_int(pCtx,-1);` |
|    ! 0 | 6489 | `		return PH7_OK;` |
|      - | 6490 | `	}` |
|     13 | 6491 | `	if( StreamSettingArg(pCtx,apArg[0],&rc) == 0 ){` |
|    ! 0 | 6492 | `		return rc;` |
|      - | 6493 | `	}` |
|     13 | 6494 | `	ph7_result_int(pCtx,-1);` |
|     13 | 6495 | `	return PH7_OK;` |
|      7 | 6496 | `}` |
|      - | 6497 | `/*` |
|      - | 6498 | ` * int\|false stream_copy_to_stream(resource $from, resource $to,` |
|      - | 6499 | ` *                                 ?int $length = null, int $offset = 0)` |
|      - | 6500 | ` *` |
|      - | 6501 | ` * The everyday way to move bytes between two open streams, and a loud` |
|      - | 6502 | `` * `Call to undefined function` here until now — so the workaround was`` |
|      - | 6503 | `` * `fwrite($to, stream_get_contents($from))`, which reads the WHOLE source into`` |
|      - | 6504 | ` * memory first. A NULL or negative $length is "the rest"; a POSITIVE $offset` |
|      - | 6505 | ` * seeks the source first and is php's only failure shape short of a broken` |
|      - | 6506 | ` * write.` |
|      - | 6507 | ` */` |
|     38 | 6508 | `PH7_PRIVATE int PH7_builtin_stream_copy_to_stream(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 6509 | `{` |
|      - | 6510 | `	io_private *pFrom,*pTo;` |
|     39 | 6511 | `	ph7_int64 nWant = -1,nOfft = 0,nTotal = 0;` |
|      - | 6512 | `	char zBuf[8192];` |
|      - | 6513 | `	int rc;` |
|     39 | 6514 | `	if( nArg < 2 ){` |
|    ! 0 | 6515 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 6516 | `		return PH7_OK;` |
|      - | 6517 | `	}` |
|     39 | 6518 | `	pFrom = StreamSettingArgNamed(pCtx,apArg[0],1,"from",&rc);` |
|     39 | 6519 | `	if( pFrom == 0 ){` |
|      3 | 6520 | `		return rc;` |
|      - | 6521 | `	}` |
|     37 | 6522 | `	pTo = StreamSettingArgNamed(pCtx,apArg[1],2,"to",&rc);` |
|     37 | 6523 | `	if( pTo == 0 ){` |
|      3 | 6524 | `		return rc;` |
|      - | 6525 | `	}` |
|     34 | 6526 | `	if( pFrom->pStream == 0 \|\| pFrom->pStream->xRead == 0` |
|     35 | 6527 | `	 \|\| pTo->pStream == 0 \|\| pTo->pStream->xWrite == 0 ){` |
|    ! 0 | 6528 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 6529 | `		return PH7_OK;` |
|      - | 6530 | `	}` |
|     35 | 6531 | `	if( nArg > 2 && !ph7_value_is_null(apArg[2]) ){` |
|     11 | 6532 | `		nWant = ph7_value_to_int64(apArg[2]);` |
|      5 | 6533 | `	}` |
|     35 | 6534 | `	if( nArg > 3 ){` |
|     19 | 6535 | `		nOfft = ph7_value_to_int64(apArg[3]);` |
|      9 | 6536 | `	}` |
|     35 | 6537 | `	if( nOfft > 0 ){` |
|      - | 6538 | `		/* php seeks the SOURCE and gives up loudly when it cannot: a pipe has` |
|      - | 6539 | `		 * no position to move to, and silently copying from wherever it` |
|      - | 6540 | `		 * happens to be would answer for a different slice of the stream. */` |
|      8 | 6541 | `		if( pFrom->pStream->xSeek == 0` |
|      8 | 6542 | `		 \|\| pFrom->pStream->xSeek(pFrom->pHandle,nOfft,0/*SEEK_SET*/) != PH7_OK ){` |
|      2 | 6543 | `			if( pFrom->pStream->xSeek == 0 ){` |
|      2 | 6544 | `				ph7_context_throw_error(pCtx,PH7_CTX_WARNING,` |
|      - | 6545 | `					"stream_copy_to_stream(): Stream does not support seeking");` |
|      1 | 6546 | `			}` |
|      3 | 6547 | `			ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,` |
|      1 | 6548 | `				"stream_copy_to_stream(): Failed to seek to position %qd in the stream",nOfft);` |
|      2 | 6549 | `			ph7_result_bool(pCtx,0);` |
|      2 | 6550 | `			return PH7_OK;` |
|      - | 6551 | `		}` |
|      7 | 6552 | `		ResetIOPrivate(pFrom);` |
|      3 | 6553 | `	}` |
|     33 | 6554 | `	if( nWant == 0 ){` |
|      3 | 6555 | `		ph7_result_int(pCtx,0);` |
|      3 | 6556 | `		return PH7_OK;` |
|      - | 6557 | `	}` |
|      - | 6558 | `#ifdef __WINNT__` |
|      - | 6559 | `	/* An unfiltered plain-file source goes through php's memory-mapped copy,` |
|      - | 6560 | `	 * whose Windows view at the end of the file is a failure: see` |
|      - | 6561 | `	 * PH7_WinFileMapsEmptyView(). */` |
|      - | 6562 | `	if( pFrom->pStream == &sWinFileStream && pFrom->pReadFilters == 0 && pFrom->pWriteFilters == 0` |
|      1 | 6563 | `	 && PH7_WinFileMapsEmptyView(pFrom->pHandle,SyBlobLength(&pFrom->sBuffer) > pFrom->nOfft` |
|      - | 6564 | `			? (ph7_int64)(SyBlobLength(&pFrom->sBuffer) - pFrom->nOfft) : 0) ){` |
|      1 | 6565 | `		ph7_result_bool(pCtx,0);` |
|      1 | 6566 | `		return PH7_OK;` |
|      - | 6567 | `	}` |
|      - | 6568 | `#endif` |
|      - | 6569 | `	/* The destination may be sitting past its own read-ahead; the write has to` |
|      - | 6570 | `	 * land where the SCRIPT is, the rule fwrite() follows. */` |
|     31 | 6571 | `	StreamSeekBackForWrite(pTo);` |
|     39 | 6572 | `	for(;;){` |
|     55 | 6573 | `		ph7_int64 nAsk = (ph7_int64)sizeof(zBuf);` |
|      - | 6574 | `		ph7_int64 nRead,nWr;` |
|     55 | 6575 | `		if( nWant > 0 && nWant - nTotal < nAsk ){` |
|     13 | 6576 | `			nAsk = nWant - nTotal;` |
|      6 | 6577 | `		}` |
|     55 | 6578 | `		if( nAsk < 1 ){` |
|      5 | 6579 | `			break;` |
|      - | 6580 | `		}` |
|     51 | 6581 | `		nRead = PH7_StreamRead(pFrom,zBuf,nAsk);` |
|     51 | 6582 | `		if( nRead < 1 ){` |
|     27 | 6583 | `			break;` |
|      - | 6584 | `		}` |
|     25 | 6585 | `		nWr = PH7_StreamWrite(pTo,(const void *)zBuf,nRead);` |
|     25 | 6586 | `		if( nWr < 0 ){` |
|    ! 0 | 6587 | `			break;` |
|      - | 6588 | `		}` |
|     25 | 6589 | `		nTotal += nWr;` |
|     25 | 6590 | `		if( nWr < nRead ){` |
|    ! 0 | 6591 | `			break;` |
|      - | 6592 | `		}` |
|      1 | 6593 | `	}` |
|     31 | 6594 | `	ph7_result_int64(pCtx,nTotal);` |
|     31 | 6595 | `	return PH7_OK;` |
|     20 | 6596 | `}` |
|      - | 6597 | `/*` |
|      - | 6598 | ` * int\|false stream_select(?array &$read, ?array &$write, ?array &$except,` |
|      - | 6599 | ` *                         ?int $seconds, ?int $microseconds = null)` |
|      - | 6600 | ` *` |
|      - | 6601 | ` * The name that makes a program WAIT on several streams at once, and the reason` |
|      - | 6602 | ` * the settings family that shipped beside it had nothing to wait with: a` |
|      - | 6603 | ` * non-blocking read tells you a stream is not ready, and only this tells you` |
|      - | 6604 | ` * WHEN it becomes ready. It is what a proc_open() pipe pump, a socket server` |
|      - | 6605 | ` * loop and every event loop written in php is built on, and it was a loud` |
|      - | 6606 | `` * `Call to undefined function`.`` |
|      - | 6607 | ` *` |
|      - | 6608 | ` * php's own shape, and the parts of it a re-derivation misses: the arrays are` |
|      - | 6609 | ` * REWRITTEN in place to hold only the ready entries, under their original keys;` |
|      - | 6610 | ` * a stream that cannot be represented as a descriptor is a warning naming its` |
|      - | 6611 | ` * TYPE, not a failure; nothing selectable at all is an Error rather than 0; and` |
|      - | 6612 | ` * a stream whose own read buffer still holds bytes is answered READY without` |
|      - | 6613 | ` * asking the OS at all — which is the difference between a loop that drains a` |
|      - | 6614 | ` * buffered handle and one that waits forever for data it has already read.` |
|      - | 6615 | ` */` |
|      - | 6616 | `#if !defined(__WINNT__) \|\| defined(PH7_ENABLE_NET)` |
|      - | 6617 | `#define STREAM_SELECT_OK 1` |
|      - | 6618 | `#ifdef __UNIXES__` |
|      - | 6619 | `#include <sys/select.h>` |
|      - | 6620 | `#include <sys/time.h>` |
|      - | 6621 | `#endif` |
|      - | 6622 | `#endif` |
|      - | 6623 | `#define SEL_READ   0` |
|      - | 6624 | `#define SEL_WRITE  1` |
|      - | 6625 | `#define SEL_EXCEPT 2` |
|      - | 6626 | `/* What one walk over an argument is for. The order matters: php COUNTS the` |
|      - | 6627 | ` * already-buffered readable handles before it waits, and only rewrites the` |
|      - | 6628 | ` * arrays once it knows which answer it is giving. */` |
|      - | 6629 | `#define SELM_COLLECT  0 /* put every representable handle in its fd_set */` |
|      - | 6630 | `#define SELM_BUFFERED 1 /* keep the handles whose own buffer still holds bytes */` |
|      - | 6631 | `#define SELM_READY    2 /* keep the handles select() reported */` |
|      - | 6632 | `#define SELM_CLEAR    3 /* keep nothing: php empties the sets it is not answering */` |
|      - | 6633 | `typedef struct stream_select_ctx stream_select_ctx;` |
|      - | 6634 | `struct stream_select_ctx` |
|      - | 6635 | `{` |
|      - | 6636 | `	ph7_context *pCtx;` |
|      - | 6637 | `#ifdef STREAM_SELECT_OK` |
|      - | 6638 | `	fd_set aSet[3];    /* read / write / except, as select() takes them */` |
|      - | 6639 | `#endif` |
|      - | 6640 | `	int iMaxFd;` |
|      - | 6641 | `	int nSelectable;   /* entries that could be represented at all */` |
|      - | 6642 | `	int iWhich;        /* the set being walked (SEL_*) */` |
|      - | 6643 | `	int iMode;         /* SELM_*: what this walk is FOR */` |
|      - | 6644 | `	int nReady;` |
|      - | 6645 | `	int bBadEntry;     /* an entry that is not a stream at all */` |
|      - | 6646 | `	int bBadClosed;    /* ... and whether it was a CLOSED one (php words the two apart) */` |
|      - | 6647 | `	ph7_value *pOut;   /* the rebuilt array, while harvesting */` |
|      - | 6648 | `};` |
|      - | 6649 | `/*` |
|      - | 6650 | ` * What a select can WAIT on for this handle: the POSIX descriptor, or the` |
|      - | 6651 | ` * SOCKET, which is the only waitable thing a stream carries on Windows (the` |
|      - | 6652 | ` * file devices hold a HANDLE there, and select() cannot take one — a recorded` |
|      - | 6653 | ` * platform difference, §7.4). Answers -1 for a device with neither: a memory` |
|      - | 6654 | ` * buffer, a data:// payload, a userland wrapper.` |
|      - | 6655 | ` */` |
|     86 | 6656 | `static ph7_int64 IoPrivateSelectHandle(io_private *pDev)` |
|      1 | 6657 | `{` |
|      - | 6658 | `	int fd;` |
|      - | 6659 | `#ifdef PH7_ENABLE_NET` |
|     87 | 6660 | `	ph7_socket *pSock = IoPrivateSocket(pDev);` |
|     87 | 6661 | `	if( pSock ){` |
|     61 | 6662 | `		return *pSock == PH7_NET_INVALID_SOCKET ? -1 : (ph7_int64)*pSock;` |
|      - | 6663 | `	}` |
|      - | 6664 | `#endif` |
|     27 | 6665 | `	fd = PH7_StreamPosixFd(pDev);` |
|     27 | 6666 | `	return fd < 0 ? -1 : (ph7_int64)fd;` |
|     44 | 6667 | `}` |
|      - | 6668 | `/* Bytes this handle has already pulled off the device and not yet handed over. */` |
|     40 | 6669 | `static sxu32 IoPrivateUnread(io_private *pDev)` |
|      1 | 6670 | `{` |
|     41 | 6671 | `	return StreamAheadBytes(pDev);` |
|      1 | 6672 | `}` |
|     48 | 6673 | `static void StreamSelectAdd(stream_select_ctx *pSel,ph7_int64 h)` |
|      1 | 6674 | `{` |
|      - | 6675 | `#ifdef STREAM_SELECT_OK` |
|      - | 6676 | `#ifdef __WINNT__` |
|      - | 6677 | `	/* A Windows fd_set is an ARRAY of sockets, so what bounds it is how many` |
|      - | 6678 | `	 * are in it already rather than the value of this one. */` |
|      1 | 6679 | `	if( pSel->aSet[pSel->iWhich].fd_count >= FD_SETSIZE ){` |
|    ! 0 | 6680 | `		return;` |
|      - | 6681 | `	}` |
|      1 | 6682 | `	FD_SET((SOCKET)h,&pSel->aSet[pSel->iWhich]);` |
|      - | 6683 | `#else` |
|     48 | 6684 | `	if( h < 0 \|\| h >= (ph7_int64)FD_SETSIZE ){` |
|      - | 6685 | `		/* php ignores a descriptor an fd_set cannot hold (its own` |
|      - | 6686 | `		 * PHP_SAFE_FD_SET); writing past one corrupts the stack. */` |
|    ! 0 | 6687 | `		return;` |
|      - | 6688 | `	}` |
|     48 | 6689 | `	FD_SET((int)h,&pSel->aSet[pSel->iWhich]);` |
|      - | 6690 | `#endif` |
|     49 | 6691 | `	if( h > (ph7_int64)pSel->iMaxFd ){` |
|     39 | 6692 | `		pSel->iMaxFd = (int)h;` |
|     19 | 6693 | `	}` |
|      - | 6694 | `#else` |
|      - | 6695 | `	SXUNUSED(pSel);` |
|      - | 6696 | `	SXUNUSED(h);` |
|      - | 6697 | `#endif` |
|     25 | 6698 | `}` |
|     30 | 6699 | `static int StreamSelectIsSet(stream_select_ctx *pSel,ph7_int64 h)` |
|      1 | 6700 | `{` |
|      - | 6701 | `#ifdef STREAM_SELECT_OK` |
|      - | 6702 | `#ifdef __WINNT__` |
|      1 | 6703 | `	return FD_ISSET((SOCKET)h,&pSel->aSet[pSel->iWhich]) ? 1 : 0;` |
|      - | 6704 | `#else` |
|     30 | 6705 | `	if( h < 0 \|\| h >= (ph7_int64)FD_SETSIZE ){` |
|    ! 0 | 6706 | `		return 0;` |
|      - | 6707 | `	}` |
|     30 | 6708 | `	return FD_ISSET((int)h,&pSel->aSet[pSel->iWhich]) ? 1 : 0;` |
|      - | 6709 | `#endif` |
|      - | 6710 | `#else` |
|      - | 6711 | `	SXUNUSED(pSel);` |
|      - | 6712 | `	SXUNUSED(h);` |
|      - | 6713 | `	return 0;` |
|      - | 6714 | `#endif` |
|     16 | 6715 | `}` |
|      - | 6716 | `/*` |
|      - | 6717 | ` * One entry of one array: collected on the way in, harvested on the way out.` |
|      - | 6718 | ` * php never stops for an entry it cannot use — the diagnostics are remembered` |
|      - | 6719 | ` * and raised once the whole set is known, because whether the array held` |
|      - | 6720 | ` * ANYTHING selectable decides which of them php raises.` |
|      - | 6721 | ` */` |
|    134 | 6722 | `static int StreamSelectWalk(ph7_value *pKey,ph7_value *pValue,void *pUserData)` |
|      1 | 6723 | `{` |
|    135 | 6724 | `	stream_select_ctx *pSel = (stream_select_ctx *)pUserData;` |
|      - | 6725 | `	io_private *pDev;` |
|      - | 6726 | `	ph7_int64 h;` |
|    135 | 6727 | `	if( !ph7_value_is_resource(pValue) ){` |
|      7 | 6728 | `		pSel->bBadEntry = 1;` |
|      - | 6729 | `		/* php words a value that is not a resource apart from a resource that is` |
|      - | 6730 | `		 * no longer open, and raises one per bad entry — so the LAST one seen is` |
|      - | 6731 | `		 * the message that reaches the caller. */` |
|      7 | 6732 | `		pSel->bBadClosed = 0;` |
|      7 | 6733 | `		return PH7_OK;` |
|      - | 6734 | `	}` |
|    129 | 6735 | `	pDev = (io_private *)ph7_value_to_resource(pValue);` |
|    129 | 6736 | `	if( IO_PRIVATE_INVALID(pDev) ){` |
|      3 | 6737 | `		pSel->bBadEntry = pSel->bBadClosed = 1;` |
|      3 | 6738 | `		return PH7_OK;` |
|      - | 6739 | `	}` |
|    127 | 6740 | `	if( pSel->iMode == SELM_BUFFERED ){` |
|      - | 6741 | `		/* Deliberately BEFORE the descriptor lookup: php's shortcut lets a` |
|      - | 6742 | `		 * readable stream with no descriptor at all take part (a userland` |
|      - | 6743 | `		 * wrapper a line read has filled the buffer of), and answering 0 for one` |
|      - | 6744 | `		 * would sleep out the whole timeout over bytes the script already has. */` |
|     41 | 6745 | `		if( IoPrivateUnread(pDev) > 0 ){` |
|      9 | 6746 | `			if( pSel->pOut ){` |
|      5 | 6747 | `				ph7_array_add_elem(pSel->pOut,pKey,pValue);` |
|      2 | 6748 | `			}` |
|      9 | 6749 | `			pSel->nReady++;` |
|      4 | 6750 | `		}` |
|     41 | 6751 | `		return PH7_OK;` |
|      - | 6752 | `	}` |
|     87 | 6753 | `	h = IoPrivateSelectHandle(pDev);` |
|     87 | 6754 | `	if( h < 0 ){` |
|      7 | 6755 | `		if( pSel->iMode == SELM_COLLECT ){` |
|      - | 6756 | `			const char *zWrapper,*zLabel;` |
|      5 | 6757 | `			IoPrivateStreamLabels(pDev,&zWrapper,&zLabel);` |
|      7 | 6758 | `			ph7_context_throw_error_format(pSel->pCtx,PH7_CTX_WARNING,` |
|      2 | 6759 | `				"Cannot represent a stream of type %s as a select()able descriptor",zLabel);` |
|      2 | 6760 | `		}` |
|      7 | 6761 | `		return PH7_OK;` |
|      - | 6762 | `	}` |
|     81 | 6763 | `	if( pSel->iMode == SELM_COLLECT ){` |
|     49 | 6764 | `		pSel->nSelectable++;` |
|     49 | 6765 | `		StreamSelectAdd(pSel,h);` |
|     49 | 6766 | `		return PH7_OK;` |
|      - | 6767 | `	}` |
|     33 | 6768 | `	if( pSel->iMode == SELM_READY && StreamSelectIsSet(pSel,h) ){` |
|     21 | 6769 | `		if( pSel->pOut ){` |
|     21 | 6770 | `			ph7_array_add_elem(pSel->pOut,pKey,pValue);` |
|     10 | 6771 | `		}` |
|     21 | 6772 | `		pSel->nReady++;` |
|     10 | 6773 | `	}` |
|     33 | 6774 | `	return PH7_OK;` |
|     68 | 6775 | `}` |
|      - | 6776 | `/* The wait itself, over the pair the caller's numbers were normalised into. */` |
|     22 | 6777 | `static int StreamSelectWait(stream_select_ctx *pSel,ph7_int64 iSec,ph7_int64 iUsec,int bBlock,` |
|      - | 6778 | `	int *pErrno)` |
|      1 | 6779 | `{` |
|      - | 6780 | `#ifdef STREAM_SELECT_OK` |
|     23 | 6781 | `	struct timeval tv,*pTv = 0;` |
|      - | 6782 | `	int rc;` |
|     23 | 6783 | `	if( !bBlock ){` |
|     23 | 6784 | `		tv.tv_sec = (long)iSec;` |
|     23 | 6785 | `		tv.tv_usec = (long)iUsec;` |
|     23 | 6786 | `		pTv = &tv;` |
|     11 | 6787 | `	}` |
|     34 | 6788 | `	rc = select(pSel->iMaxFd + 1,&pSel->aSet[SEL_READ],&pSel->aSet[SEL_WRITE],` |
|     11 | 6789 | `		&pSel->aSet[SEL_EXCEPT],pTv);` |
|     23 | 6790 | `	if( rc < 0 && pErrno ){` |
|      - | 6791 | `#ifdef __WINNT__` |
|    ! 0 | 6792 | `		*pErrno = WSAGetLastError();` |
|      - | 6793 | `#else` |
|    ! 0 | 6794 | `		*pErrno = errno;` |
|      - | 6795 | `#endif` |
|    ! 0 | 6796 | `	}` |
|     23 | 6797 | `	return rc;` |
|      - | 6798 | `#else` |
|      - | 6799 | `	/* No select() to call: a Windows build with no socket layer. */` |
|      - | 6800 | `	SXUNUSED(pSel);` |
|      - | 6801 | `	SXUNUSED(iSec);` |
|      - | 6802 | `	SXUNUSED(iUsec);` |
|      - | 6803 | `	SXUNUSED(bBlock);` |
|      - | 6804 | `	if( pErrno ){ *pErrno = 0; }` |
|      - | 6805 | `	return -1;` |
|      - | 6806 | `#endif` |
|      1 | 6807 | `}` |
|      - | 6808 | `/* Walk one of the three arguments, if it IS one. */` |
|    190 | 6809 | `static void StreamSelectEach(stream_select_ctx *pSel,ph7_value **apArg,int nArg,int iArg,int iWhich,` |
|      - | 6810 | `	int iMode)` |
|      1 | 6811 | `{` |
|    191 | 6812 | `	if( iArg >= nArg \|\| apArg[iArg] == 0 \|\| !ph7_value_is_array(apArg[iArg]) ){` |
|     85 | 6813 | `		return;` |
|      - | 6814 | `	}` |
|    107 | 6815 | `	pSel->iWhich = iWhich;` |
|    107 | 6816 | `	pSel->iMode = iMode;` |
|    107 | 6817 | `	ph7_array_walk(apArg[iArg],StreamSelectWalk,pSel);` |
|     96 | 6818 | `}` |
|      - | 6819 | `/* Rebuild one argument from the entries that came back ready. php REPLACES the` |
|      - | 6820 | ` * array either way, so a set with nothing ready comes back empty. */` |
|     78 | 6821 | `static int StreamSelectStore(stream_select_ctx *pSel,ph7_value **apArg,int nArg,int iArg,int iWhich,` |
|      - | 6822 | `	int iMode)` |
|      1 | 6823 | `{` |
|     79 | 6824 | `	if( iArg >= nArg \|\| apArg[iArg] == 0 \|\| !ph7_value_is_array(apArg[iArg]) ){` |
|     47 | 6825 | `		return PH7_OK;` |
|      - | 6826 | `	}` |
|     33 | 6827 | `	pSel->pOut = ph7_context_new_array(pSel->pCtx);` |
|     33 | 6828 | `	if( pSel->pOut == 0 ){` |
|      - | 6829 | `		/* Leaving the caller's array alone would answer that every entry is` |
|      - | 6830 | `		 * ready, which is the one wrong answer this function must not give. */` |
|    ! 0 | 6831 | `		return PH7_ContextMemoryError(pSel->pCtx);` |
|      - | 6832 | `	}` |
|     33 | 6833 | `	StreamSelectEach(pSel,apArg,nArg,iArg,iWhich,iMode);` |
|     33 | 6834 | `	PH7_VmStoreArgByRef(pSel->pCtx->pVm,apArg[iArg],pSel->pOut);` |
|     33 | 6835 | `	pSel->pOut = 0;` |
|     33 | 6836 | `	return PH7_OK;` |
|     40 | 6837 | `}` |
|     44 | 6838 | `PH7_PRIVATE int PH7_builtin_stream_select(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 6839 | `{` |
|      - | 6840 | `	stream_select_ctx sSel;` |
|     45 | 6841 | `	ph7_int64 iSec = 0,iUsec = 0;` |
|     45 | 6842 | `	int bBlock = 1,iErrno = 0,rc,i;` |
|     45 | 6843 | `	SyZero(&sSel,sizeof(sSel));` |
|     45 | 6844 | `	sSel.pCtx = pCtx;` |
|     45 | 6845 | `	sSel.iMaxFd = -1;` |
|      - | 6846 | `#ifdef STREAM_SELECT_OK` |
|    177 | 6847 | `	for( i = 0 ; i < 3 ; i++ ){` |
|   1189 | 6848 | `		FD_ZERO(&sSel.aSet[i]);` |
|     67 | 6849 | `	}` |
|      - | 6850 | `#endif` |
|     45 | 6851 | `	StreamSelectEach(&sSel,apArg,nArg,0,SEL_READ,SELM_COLLECT);` |
|     45 | 6852 | `	StreamSelectEach(&sSel,apArg,nArg,1,SEL_WRITE,SELM_COLLECT);` |
|     45 | 6853 | `	StreamSelectEach(&sSel,apArg,nArg,2,SEL_EXCEPT,SELM_COLLECT);` |
|     45 | 6854 | `	if( sSel.nSelectable < 1 ){` |
|      - | 6855 | `		/* php's own wording, and it carries no function name. It is the answer` |
|      - | 6856 | `		 * for three NULLs, for empty arrays, and for arrays holding nothing` |
|      - | 6857 | `		 * this engine can wait on — the caller asked to wait for nothing. */` |
|      7 | 6858 | `		return PH7_VmThrowException(pCtx,"ValueError","No stream arrays were passed");` |
|      - | 6859 | `	}` |
|     39 | 6860 | `	if( sSel.bBadEntry ){` |
|      - | 6861 | `		/* Raised only once the arrays are known to hold something to wait on —` |
|      - | 6862 | `		 * the empty-arrays Error wins over it — and BEFORE the timeout is` |
|      - | 6863 | `		 * looked at, which is the order php's own pending-exception check` |
|      - | 6864 | `		 * produces. */` |
|     10 | 6865 | `		return PH7_VmThrowException(pCtx,"TypeError",` |
|      - | 6866 | `			"%s(): supplied %s is not a valid stream resource",` |
|      6 | 6867 | `			ph7_function_name(pCtx),sSel.bBadClosed ? "resource" : "argument");` |
|      - | 6868 | `	}` |
|     33 | 6869 | `	if( nArg > 3 && !ph7_value_is_null(apArg[3]) ){` |
|     31 | 6870 | `		iSec = ph7_value_to_int64(apArg[3]);` |
|     31 | 6871 | `		bBlock = 0;` |
|     31 | 6872 | `		if( iSec < 0 ){` |
|      4 | 6873 | `			return PH7_VmThrowException(pCtx,"ValueError",` |
|      - | 6874 | `				"%s(): Argument #4 ($seconds) must be greater than or equal to 0",` |
|      1 | 6875 | `				ph7_function_name(pCtx));` |
|      - | 6876 | `		}` |
|     14 | 6877 | `	}` |
|     31 | 6878 | `	if( nArg > 4 && !ph7_value_is_null(apArg[4]) ){` |
|     15 | 6879 | `		iUsec = ph7_value_to_int64(apArg[4]);` |
|     15 | 6880 | `		if( bBlock ){` |
|      - | 6881 | `			/* php refuses the pair rather than guessing which one meant it: a` |
|      - | 6882 | `			 * NULL $seconds is "wait forever", and there is no such thing as` |
|      - | 6883 | `			 * waiting forever for five microseconds. */` |
|      3 | 6884 | `			if( iUsec != 0 ){` |
|      4 | 6885 | `				return PH7_VmThrowException(pCtx,"ValueError",` |
|      - | 6886 | `					"%s(): Argument #5 ($microseconds) must be null when argument #4 ($seconds) is null",` |
|      1 | 6887 | `					ph7_function_name(pCtx));` |
|    ! 0 | 6888 | `			}` |
|     13 | 6889 | `		}else if( iUsec < 0 ){` |
|      4 | 6890 | `			return PH7_VmThrowException(pCtx,"ValueError",` |
|      - | 6891 | `				"%s(): Argument #5 ($microseconds) must be greater than or equal to 0",` |
|      1 | 6892 | `				ph7_function_name(pCtx));` |
|      - | 6893 | `		}` |
|      5 | 6894 | `	}` |
|     27 | 6895 | `	if( iUsec > 999999 ){` |
|      - | 6896 | `		/* php carries the overflow into the seconds, because a tv_usec of a` |
|      - | 6897 | `		 * million or more is what Solaris and the BSDs refuse outright — so` |
|      - | 6898 | ``		 * `stream_select($r, $w, $x, 0, 1500000)` waits a second and a half`` |
|      - | 6899 | `		 * rather than failing. */` |
|      3 | 6900 | `		iSec += iUsec / 1000000;` |
|      3 | 6901 | `		iUsec %= 1000000;` |
|      1 | 6902 | `	}` |
|      - | 6903 | `	/* php's own shortcut, and it comes BEFORE the wait: a handle whose buffer` |
|      - | 6904 | `	 * still holds bytes the script has not taken is ready NOW, whatever the OS` |
|      - | 6905 | `	 * would say about its descriptor — the device has nothing left to report.` |
|      - | 6906 | `	 * COUNTED first and stored second, because the count is what decides` |
|      - | 6907 | `	 * whether the arrays are rewritten from the buffers or from the wait. */` |
|     27 | 6908 | `	StreamSelectEach(&sSel,apArg,nArg,0,SEL_READ,SELM_BUFFERED);` |
|     27 | 6909 | `	if( sSel.nReady > 0 ){` |
|      5 | 6910 | `		sSel.nReady = 0;` |
|      4 | 6911 | `		if( StreamSelectStore(&sSel,apArg,nArg,0,SEL_READ,SELM_BUFFERED) != PH7_OK` |
|      - | 6912 | `		/* php answers only the readable ones then, and empties the other two. */` |
|      4 | 6913 | `		 \|\| StreamSelectStore(&sSel,apArg,nArg,1,SEL_WRITE,SELM_CLEAR) != PH7_OK` |
|      5 | 6914 | `		 \|\| StreamSelectStore(&sSel,apArg,nArg,2,SEL_EXCEPT,SELM_CLEAR) != PH7_OK ){` |
|    ! 0 | 6915 | `			return PH7_ContextMemoryError(pCtx);` |
|      - | 6916 | `		}` |
|      5 | 6917 | `		ph7_result_int(pCtx,sSel.nReady);` |
|      5 | 6918 | `		return PH7_OK;` |
|      - | 6919 | `	}` |
|     23 | 6920 | `	rc = StreamSelectWait(&sSel,iSec,iUsec,bBlock,&iErrno);` |
|     23 | 6921 | `	if( rc < 0 ){` |
|      - | 6922 | `#if defined(__WINNT__) && defined(PH7_ENABLE_NET)` |
|    ! 0 | 6923 | `		const char *zErr = PH7_NetStrError(iErrno);` |
|      - | 6924 | `#else` |
|    ! 0 | 6925 | `		const char *zErr = VfsStrerror(iErrno);` |
|      - | 6926 | `#endif` |
|    ! 0 | 6927 | `		ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,"Unable to select [%d]: %s (max_fd=%d)",` |
|    ! 0 | 6928 | `			iErrno,zErr,sSel.iMaxFd);` |
|    ! 0 | 6929 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 6930 | `		return PH7_OK;` |
|      - | 6931 | `	}` |
|     22 | 6932 | `	if( StreamSelectStore(&sSel,apArg,nArg,0,SEL_READ,SELM_READY) != PH7_OK` |
|     22 | 6933 | `	 \|\| StreamSelectStore(&sSel,apArg,nArg,1,SEL_WRITE,SELM_READY) != PH7_OK` |
|     23 | 6934 | `	 \|\| StreamSelectStore(&sSel,apArg,nArg,2,SEL_EXCEPT,SELM_READY) != PH7_OK ){` |
|    ! 0 | 6935 | `		return PH7_ContextMemoryError(pCtx);` |
|      - | 6936 | `	}` |
|      - | 6937 | `	/* The COUNT is select()'s own, not the entries kept: two array members can` |
|      - | 6938 | `	 * name one descriptor, and php answers what the OS said. */` |
|     23 | 6939 | `	ph7_result_int(pCtx,rc);` |
|     23 | 6940 | `	return PH7_OK;` |
|     23 | 6941 | `}` |
|      - | 6942 | `/*` |
|      - | 6943 | ` * array stream_get_transports(void)` |
|      - | 6944 | ` *` |
|      - | 6945 | ` * The transports a stream_socket_client()/fsockopen() address may name. php's` |
|      - | 6946 | ` * own list is what its build registered, so this is what THIS engine can open:` |
|      - | 6947 | ` * the ssl/tls/udp/unix set is a recorded scope gap (§7.4), and answering for` |
|      - | 6948 | ` * transports that are not there would tell a script a connection will work` |
|      - | 6949 | ` * when it cannot.` |
|      - | 6950 | ` */` |
|      8 | 6951 | `PH7_PRIVATE int PH7_builtin_stream_get_transports(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      2 | 6952 | `{` |
|      - | 6953 | `	ph7_value *pArr,*pV;` |
|      4 | 6954 | `	SXUNUSED(nArg);` |
|      4 | 6955 | `	SXUNUSED(apArg);` |
|     10 | 6956 | `	pArr = ph7_context_new_array(pCtx);` |
|     10 | 6957 | `	pV = ph7_context_new_scalar(pCtx);` |
|     10 | 6958 | `	if( pArr == 0 \|\| pV == 0 ){` |
|    ! 0 | 6959 | `		ph7_result_null(pCtx);` |
|    ! 0 | 6960 | `		return PH7_OK;` |
|      - | 6961 | `	}` |
|      - | 6962 | `#ifdef PH7_ENABLE_NET` |
|     10 | 6963 | `	ph7_value_string(pV,"tcp",-1);` |
|     10 | 6964 | `	ph7_array_add_elem(pArr,0,pV);` |
|      - | 6965 | `#endif` |
|     10 | 6966 | `	ph7_result_value(pCtx,pArr);` |
|     10 | 6967 | `	return PH7_OK;` |
|      6 | 6968 | `}` |
|      - | 6969 | `/*` |
|      - | 6970 | ` * bool stream_supports_lock(resource $stream)` |
|      - | 6971 | ` *` |
|      - | 6972 | ` * The question flock() answers with a warning if you get it wrong: only a` |
|      - | 6973 | ` * device with a real lock operation can be locked, so a memory buffer and a` |
|      - | 6974 | ` * data:// payload are false.` |
|      - | 6975 | ` */` |
|     14 | 6976 | `PH7_PRIVATE int PH7_builtin_stream_supports_lock(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 6977 | `{` |
|      - | 6978 | `	io_private *pDev;` |
|      - | 6979 | `	int rc;` |
|     15 | 6980 | `	if( nArg < 1 ){` |
|    ! 0 | 6981 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 6982 | `		return PH7_OK;` |
|      - | 6983 | `	}` |
|     15 | 6984 | `	pDev = StreamSettingArg(pCtx,apArg[0],&rc);` |
|     15 | 6985 | `	if( pDev == 0 ){` |
|      3 | 6986 | `		return rc;` |
|      - | 6987 | `	}` |
|      - | 6988 | `	/* php locks at the DESCRIPTOR, so anything with one can be locked even` |
|      - | 6989 | `	 * when the device exposes no lock operation of its own (php://stdout, a` |
|      - | 6990 | `	 * pipe); a memory buffer and a data:// payload have neither and are the` |
|      - | 6991 | `	 * false answers. */` |
|     13 | 6992 | `	pDev = PH7_StreamUnwrap(pDev);` |
|     23 | 6993 | `	ph7_result_bool(pCtx,pDev->bDir == 0` |
|     16 | 6994 | `		&& ((pDev->pStream != 0 && pDev->pStream->xLock != 0)` |
|      7 | 6995 | `		    \|\| PH7_StreamPosixFd(pDev) >= 0));` |
|     13 | 6996 | `	return PH7_OK;` |
|      8 | 6997 | `}` |
|      - | 6998 | `/*` |
|      - | 6999 | ` * bool stream_is_local(resource\|string $stream)` |
|      - | 7000 | ` *` |
|      - | 7001 | ` * php answers from the WRAPPER, not from the path: a stream opened by a URL` |
|      - | 7002 | ` * wrapper is not local, one opened by no wrapper at all (a pipe) is not local` |
|      - | 7003 | ` * either, and everything else — including php:// and a path naming a scheme` |
|      - | 7004 | ` * nobody registered — is.` |
|      - | 7005 | ` */` |
|     28 | 7006 | `PH7_PRIVATE int PH7_builtin_stream_is_local(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 7007 | `{` |
|      - | 7008 | `	const ph7_io_stream *pStream;` |
|     29 | 7009 | `	if( nArg < 1 ){` |
|    ! 0 | 7010 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 7011 | `		return PH7_OK;` |
|      - | 7012 | `	}` |
|     29 | 7013 | `	if( ph7_value_is_string(apArg[0]) ){` |
|      - | 7014 | `		static const char * const azUrlScheme[] = { "http://", "https://", "ftp://", "ftps://" };` |
|      - | 7015 | `		int nLen,i;` |
|     21 | 7016 | `		const char *zPath = ph7_value_to_string(apArg[0],&nLen);` |
|     20 | 7017 | `		if( nLen > (int)sizeof("file://")-1` |
|     19 | 7018 | `		 && SyStrnicmp(zPath,"file://",sizeof("file://")-1) == 0` |
|     11 | 7019 | `		 && zPath[sizeof("file://")-1] != '/'` |
|      4 | 7020 | `		 && SyStrnicmp(zPath,"file://localhost/",sizeof("file://localhost/")-1) != 0 ){` |
|      - | 7021 | ``			/* `file://host/path` names a REMOTE host, which php refuses rather`` |
|      - | 7022 | `			 * than reading as a local path — so the answer is not local. */` |
|      3 | 7023 | `			ph7_result_bool(pCtx,0);` |
|      3 | 7024 | `			return PH7_OK;` |
|      - | 7025 | `		}` |
|     83 | 7026 | `		for( i = 0 ; i < (int)(sizeof(azUrlScheme)/sizeof(azUrlScheme[0])) ; i++ ){` |
|     67 | 7027 | `			int nScheme = (int)SyStrlen(azUrlScheme[i]);` |
|     67 | 7028 | `			if( nLen >= nScheme && SyStrnicmp(zPath,azUrlScheme[i],(sxu32)nScheme) == 0 ){` |
|      - | 7029 | `				/* php registers these as URL wrappers whether or not this` |
|      - | 7030 | `				 * engine can OPEN them (http:// is a recorded gap, §7.4), and` |
|      - | 7031 | `				 * "is this path local?" has to answer for the scheme rather` |
|      - | 7032 | `				 * than for what happens to be implemented — the unsafe` |
|      - | 7033 | `				 * direction is answering TRUE about a remote URL. */` |
|      3 | 7034 | `				ph7_result_bool(pCtx,0);` |
|      3 | 7035 | `				return PH7_OK;` |
|      - | 7036 | `			}` |
|     33 | 7037 | `		}` |
|     17 | 7038 | `		pStream = PH7_VmGetStreamDevice(pCtx->pVm,&zPath,nLen);` |
|      - | 7039 | `		/* An unregistered scheme has no wrapper to ask, and php answers TRUE` |
|      - | 7040 | `		 * for it — the path is taken at face value. */` |
|     17 | 7041 | `		ph7_result_bool(pCtx,pStream == 0 \|\| !PH7_StreamIsUrlWrapper(pStream));` |
|     17 | 7042 | `		return PH7_OK;` |
|      - | 7043 | `	}` |
|      - | 7044 | `	{` |
|      - | 7045 | `		int rc;` |
|      9 | 7046 | `		io_private *pDev = StreamSettingArg(pCtx,apArg[0],&rc);` |
|      - | 7047 | `		const char *zWrapper,*zLabel;` |
|      9 | 7048 | `		if( pDev == 0 ){` |
|    ! 0 | 7049 | `			return rc;` |
|      - | 7050 | `		}` |
|      9 | 7051 | `		IoPrivateStreamLabels(pDev,&zWrapper,&zLabel);` |
|      - | 7052 | `		/* No wrapper (a popen() pipe, a socket) is php's other "not local". */` |
|      9 | 7053 | `		ph7_result_bool(pCtx,zWrapper != 0 && !PH7_StreamIsUrlWrapper(pDev->pStream));` |
|      - | 7054 | `	}` |
|      9 | 7055 | `	return PH7_OK;` |
|     15 | 7056 | `}` |
|      - | 7057 | `/* PH7_ENABLE_NET */` |
|    940 | 7058 | `PH7_PRIVATE int PH7_builtin_fopen(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      5 | 7059 | `{` |
|      - | 7060 | `	const ph7_io_stream *pStream;` |
|      - | 7061 | `	const char *zUri,*zMode;` |
|      - | 7062 | `	ph7_value *pResource;` |
|      - | 7063 | `	io_private *pDev;` |
|      - | 7064 | `	phl_stream_ctx *pCtxRes;` |
|    945 | 7065 | `	int iLen,imLen,bThrew = 0;` |
|      - | 7066 | `	int iOpenFlags;` |
|    945 | 7067 | `	if( nArg < 1 \|\| !ph7_value_is_string(apArg[0]) ){` |
|      - | 7068 | `		/* Missing/Invalid arguments,return FALSE */` |
|    ! 0 | 7069 | `		ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"Expecting a file path or URL");` |
|    ! 0 | 7070 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 7071 | `		return PH7_OK;` |
|      - | 7072 | `	}` |
|      - | 7073 | `	/* Extract the URI and the desired access mode */` |
|    945 | 7074 | `	zUri  = ph7_value_to_string(apArg[0],&iLen);` |
|    945 | 7075 | `	if( nArg > 1 ){` |
|    945 | 7076 | `		zMode = ph7_value_to_string(apArg[1],&imLen);` |
|    475 | 7077 | `	}else{` |
|      - | 7078 | `		/* Set a default read-only mode */` |
|    ! 0 | 7079 | `		zMode = "r";` |
|    ! 0 | 7080 | `		imLen = (int)sizeof(char);` |
|      - | 7081 | `	}` |
|      - | 7082 | ``	/* php's `?resource $context`: a resource that is not a stream-context is`` |
|      - | 7083 | `	 * refused, and NULL means the DEFAULT context — never "no context at all".` |
|      - | 7084 | `	 * Resolved before the io_private chunk below, which a throw could not` |
|      - | 7085 | `	 * release. */` |
|    945 | 7086 | `	pCtxRes = PH7_StreamCtxFromArg(pCtx,nArg,apArg,3,"$context",0,&bThrew);` |
|    945 | 7087 | `	if( bThrew ){` |
|      5 | 7088 | `		return PH7_OK;` |
|      - | 7089 | `	}` |
|      - | 7090 | `	/* Try to extract a stream */` |
|    941 | 7091 | `	pStream = PH7_VmGetStreamDevice(pCtx->pVm,&zUri,iLen);` |
|    941 | 7092 | `	if( pStream == 0 ){` |
|    ! 0 | 7093 | `		VfsThrowNoDeviceWarning(pCtx,zUri,FALSE);` |
|    ! 0 | 7094 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 7095 | `		return PH7_OK;` |
|      - | 7096 | `	}` |
|      - | 7097 | `	/* Allocate a new IO private instance */` |
|    941 | 7098 | `	pDev = (io_private *)ph7_context_alloc_chunk(pCtx,sizeof(io_private),TRUE,FALSE);` |
|    941 | 7099 | `	if( pDev == 0 ){` |
|    ! 0 | 7100 | `		ph7_context_throw_error(pCtx,PH7_CTX_ERR,"PH7 is running out of memory");` |
|    ! 0 | 7101 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 7102 | `		return PH7_OK;` |
|      - | 7103 | `	}` |
|    941 | 7104 | `	pResource = 0;` |
|    941 | 7105 | `	if( nArg > 3 ){` |
|      8 | 7106 | `		pResource = apArg[3];` |
|    938 | 7107 | `	}else if( is_php_stream(pStream) \|\| is_data_stream(pStream) ){` |
|      - | 7108 | `		/* TICKET 1433-80: The php:// and data:// streams need a ph7_value to` |
|      - | 7109 | `		 * access the underlying virtual machine.` |
|      - | 7110 | `		 */` |
|    429 | 7111 | `		pResource = apArg[0];` |
|    212 | 7112 | `	}` |
|      - | 7113 | `	/* Initialize the structure */` |
|    941 | 7114 | `	InitIOPrivate(pCtx->pVm,pStream,pDev);` |
|      - | 7115 | `	/* Convert open mode to PH7 flags */` |
|    941 | 7116 | `	iOpenFlags = StrModeToFlags(pCtx,zMode,imLen);` |
|    941 | 7117 | `	PH7_StreamCtxArm(pCtx->pVm,pCtxRes);` |
|      - | 7118 | `	/* Try to get a handle */` |
|   1413 | 7119 | `	pDev->pHandle = PH7_StreamOpenHandle(pCtx->pVm,pStream,zUri,iOpenFlags,` |
|    472 | 7120 | `		nArg > 2 ? ph7_value_to_bool(apArg[2]) : FALSE,pResource,FALSE,0,ph7_function_name(pCtx));` |
|    941 | 7121 | `	if( pDev->pHandle == 0 ){` |
|      8 | 7122 | `		VfsThrowOpenWarning(pCtx,zUri);` |
|      8 | 7123 | `		ph7_result_bool(pCtx,0);` |
|      8 | 7124 | `		ReleaseIOPrivate(pCtx,pDev);` |
|      8 | 7125 | `		return PH7_OK;` |
|      - | 7126 | `	}` |
|      - | 7127 | `	/* Remember what we were asked for: stream_get_meta_data() reports both.` |
|      - | 7128 | `	 * The URI is the ORIGINAL argument, not the scheme-stripped remainder` |
|      - | 7129 | `	 * PH7_VmGetStreamDevice() advanced zUri past. */` |
|      - | 7130 | `	{` |
|      - | 7131 | `		int nUri;` |
|    935 | 7132 | `		const char *zOrig = ph7_value_to_string(apArg[0],&nUri);` |
|    935 | 7133 | `		const char *zMeta = zMode;` |
|    935 | 7134 | `		int nMeta = imLen;` |
|    930 | 7135 | `		if( is_php_stream(pStream)` |
|    673 | 7136 | `		 && PH7_PhpStreamKind(pDev->pHandle) == PH7_IO_STREAM_OUTPUT ){` |
|      - | 7137 | `			/* php://output has one mode whatever it was asked for. */` |
|      5 | 7138 | `			zMeta = "wb";` |
|      5 | 7139 | `			nMeta = 2;` |
|    929 | 7140 | `		}else if( is_php_stream(pStream)` |
|    669 | 7141 | `		 && PH7_PhpStreamKind(pDev->pHandle) == PH7_IO_STREAM_MEMORY ){` |
|      - | 7142 | `			/* php's memory streams do not keep the mode they were opened with:` |
|      - | 7143 | `			 * a buffer is readable and writable either way, so php reports the` |
|      - | 7144 | `			 * one it actually built. */` |
|    584 | 7145 | `			int i,bWrite = 0,bAppend = imLen > 0 && (zMode[0] == 'a' \|\| zMode[0] == 'A');` |
|   1103 | 7146 | `			for( i = 0 ; i < imLen ; i++ ){` |
|    712 | 7147 | `				if( zMode[i] == 'w' \|\| zMode[i] == 'W' \|\| zMode[i] == 'a'` |
|    538 | 7148 | `				 \|\| zMode[i] == 'A' \|\| zMode[i] == '+' ){` |
|    507 | 7149 | `					bWrite = 1;` |
|    252 | 7150 | `				}` |
|    361 | 7151 | `			}` |
|    391 | 7152 | `			zMeta = bWrite ? (bAppend ? "a+b" : "w+b") : "rb";` |
|    391 | 7153 | `			nMeta = (int)SyStrlen(zMeta);` |
|    193 | 7154 | `		}` |
|    935 | 7155 | `		SetIOPrivateOpenedAs(pDev,zOrig,nUri,zMeta,nMeta);` |
|      - | 7156 | `	}` |
|      - | 7157 | `	/* All done,return the io_private instance as a resource */` |
|    935 | 7158 | `	ph7_result_resource(pCtx,pDev);` |
|    935 | 7159 | `	return PH7_OK;` |
|    475 | 7160 | `}` |
|      - | 7161 | `/*` |
|      - | 7162 | ` * bool fclose(resource $handle)` |
|      - | 7163 | ` *  Closes an open file pointer` |
|      - | 7164 | ` * Parameters` |
|      - | 7165 | ` *  $handle` |
|      - | 7166 | ` *   The file pointer.` |
|      - | 7167 | ` * Return` |
|      - | 7168 | ` *  TRUE on success or FALSE on failure.` |
|      - | 7169 | ` */` |
|   1084 | 7170 | `PH7_PRIVATE int PH7_builtin_fclose(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      5 | 7171 | `{` |
|      - | 7172 | `	const ph7_io_stream *pStream;` |
|      - | 7173 | `	io_private *pDev;` |
|      - | 7174 | `	ph7_vm *pVm;` |
|   1089 | 7175 | `	if( nArg < 1 \|\| !ph7_value_is_resource(apArg[0]) ){` |
|      - | 7176 | `		/* Missing/Invalid arguments,return FALSE */` |
|    ! 0 | 7177 | `		ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"Expecting an IO handle");` |
|    ! 0 | 7178 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 7179 | `		return PH7_OK;` |
|      - | 7180 | `	}` |
|      - | 7181 | `	/* Extract our private data */` |
|   1089 | 7182 | `	pDev = (io_private *)ph7_value_to_resource(apArg[0]);` |
|      - | 7183 | `	/* php: fclose() on an already-closed stream raises a catchable TypeError */` |
|   1089 | 7184 | `	if( pDev != 0 && pDev->iMagic == IO_PRIVATE_CLOSED_MAGIC ){` |
|      3 | 7185 | `		return PH7_VmThrowException(pCtx,"TypeError",` |
|      - | 7186 | `			"fclose(): Argument #1 ($stream) must be an open stream resource");` |
|      - | 7187 | `	}` |
|      - | 7188 | `	/* Make sure we are dealing with a valid io_private instance */` |
|   1087 | 7189 | `	if( IO_PRIVATE_INVALID(pDev) ){` |
|      - | 7190 | `		/*Expecting an IO handle */` |
|    ! 0 | 7191 | `		ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"Expecting an IO handle");` |
|    ! 0 | 7192 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 7193 | `		return PH7_OK;` |
|      - | 7194 | `	}` |
|      - | 7195 | `	/* Point to the target IO stream device */` |
|   1087 | 7196 | `	pStream = pDev->pStream;` |
|   1087 | 7197 | `	if( pStream == 0 ){` |
|    ! 0 | 7198 | `		ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,` |
|      - | 7199 | `			"IO routine(%s) not implemented in the underlying stream(%s) device,PH7 is returning FALSE",` |
|    ! 0 | 7200 | `			ph7_function_name(pCtx),pStream ? pStream->zName : "null_stream"` |
|      - | 7201 | `			);` |
|    ! 0 | 7202 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 7203 | `		return PH7_OK;` |
|      - | 7204 | `	}` |
|      - | 7205 | `	/* Point to the VM that own this context */` |
|   1087 | 7206 | `	pVm = pCtx->pVm;` |
|      - | 7207 | `	/* TICKET 1433-62: Keep the STDIN/STDOUT/STDERR handles open */` |
|   1087 | 7208 | `	if( pDev != pVm->pStdin && pDev != pVm->pStdout && pDev != pVm->pStderr ){` |
|      - | 7209 | `		/* The WRITE chain gets its closing call while the device is still open:` |
|      - | 7210 | `		 * a filter that buffers has nowhere else to put its tail, and php's own` |
|      - | 7211 | `		 * close flushes before it closes. */` |
|   1087 | 7212 | `		PH7_StreamFilterReleaseChains(pDev);` |
|      - | 7213 | `		/* Perform the requested operation */` |
|   1087 | 7214 | `		PH7_StreamCloseHandle(pStream,pDev->pHandle);` |
|      - | 7215 | `		/* Keep the handle alive but flag it closed so shared copies see it */` |
|   1087 | 7216 | `		MarkIOPrivateClosed(pDev);` |
|    541 | 7217 | `	}` |
|      - | 7218 | `	/* Return TRUE */` |
|   1087 | 7219 | `	ph7_result_bool(pCtx,1);` |
|   1087 | 7220 | `	return PH7_OK;` |
|    547 | 7221 | `}` |
|      - | 7222 | `#if !defined(PH7_DISABLE_HASH_FUNC)` |
|      - | 7223 | `/*` |
|      - | 7224 | ` * MD5/SHA1 digest consumer.` |
|      - | 7225 | ` */` |
|    136 | 7226 | `static int vfsHashConsumer(const void *pData,unsigned int nLen,void *pUserData)` |
|      2 | 7227 | `{` |
|      - | 7228 | `	/* Append hex chunk verbatim */` |
|    138 | 7229 | `	ph7_result_string((ph7_context *)pUserData,(const char *)pData,(int)nLen);` |
|    138 | 7230 | `	return SXRET_OK;` |
|      2 | 7231 | `}` |
|      - | 7232 | `/*` |
|      - | 7233 | ` * string md5_file(string $uri[,bool $raw_output = false ])` |
|      - | 7234 | ` *  Calculates the md5 hash of a given file.` |
|      - | 7235 | ` * Parameters` |
|      - | 7236 | ` *  $uri` |
|      - | 7237 | ` *   Target URI (file(/path/to/something) or URL(http://www.symisc.net/))` |
|      - | 7238 | ` *  $raw_output` |
|      - | 7239 | ` *   When TRUE, returns the digest in raw binary format with a length of 16.` |
|      - | 7240 | ` * Return` |
|      - | 7241 | ` *  Return the MD5 digest on success or FALSE on failure.` |
|      - | 7242 | ` */` |
|      8 | 7243 | `PH7_PRIVATE int PH7_builtin_md5_file(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      2 | 7244 | `{` |
|      - | 7245 | `	const ph7_io_stream *pStream;` |
|      - | 7246 | `	unsigned char zDigest[16];` |
|     10 | 7247 | `	int raw_output  = FALSE;` |
|      - | 7248 | `	const char *zFile;` |
|      - | 7249 | `	MD5Context sCtx;` |
|      - | 7250 | `	char zBuf[8192];` |
|      - | 7251 | `	void *pHandle;` |
|      - | 7252 | `	ph7_int64 n;` |
|      - | 7253 | `	int nLen;` |
|     10 | 7254 | `	if( nArg < 1 \|\| !ph7_value_is_string(apArg[0]) ){` |
|      - | 7255 | `		/* Missing/Invalid arguments,return FALSE */` |
|    ! 0 | 7256 | `		ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"Expecting a file path");` |
|    ! 0 | 7257 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 7258 | `		return PH7_OK;` |
|      - | 7259 | `	}` |
|      - | 7260 | `	/* Extract the file path */` |
|     10 | 7261 | `	zFile = ph7_value_to_string(apArg[0],&nLen);` |
|      - | 7262 | `	/* Point to the target IO stream device */` |
|     10 | 7263 | `	pStream = PH7_VmGetStreamDevice(pCtx->pVm,&zFile,nLen);` |
|     10 | 7264 | `	if( pStream == 0 ){` |
|    ! 0 | 7265 | `		VfsThrowNoDeviceWarning(pCtx,zFile,FALSE);` |
|    ! 0 | 7266 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 7267 | `		return PH7_OK;` |
|      - | 7268 | `	}` |
|     10 | 7269 | `	if( nArg > 1 ){` |
|    ! 0 | 7270 | `		raw_output = ph7_value_to_bool(apArg[1]);` |
|    ! 0 | 7271 | `	}` |
|      - | 7272 | `	/* Try to open the file in read-only mode */` |
|     10 | 7273 | `	pHandle = PH7_StreamOpenHandle(pCtx->pVm,pStream,zFile,PH7_IO_OPEN_RDONLY,FALSE,0,FALSE,0,ph7_function_name(pCtx));` |
|     10 | 7274 | `	if( pHandle == 0 ){` |
|      3 | 7275 | `		VfsThrowOpenWarning(pCtx,zFile);` |
|      3 | 7276 | `		ph7_result_bool(pCtx,0);` |
|      3 | 7277 | `		return PH7_OK;` |
|      - | 7278 | `	}` |
|      - | 7279 | `	/* Init the MD5 context */` |
|      8 | 7280 | `	MD5Init(&sCtx);` |
|      - | 7281 | `	/* Perform the requested operation */` |
|      4 | 7282 | `	for(;;){` |
|     10 | 7283 | `		n = pStream->xRead(pHandle,zBuf,sizeof(zBuf));` |
|     10 | 7284 | `		if( n < 1 ){` |
|      - | 7285 | `			/* EOF or IO error,break immediately */` |
|      8 | 7286 | `			break;` |
|      - | 7287 | `		}` |
|      3 | 7288 | `		MD5Update(&sCtx,(const unsigned char *)zBuf,(unsigned int)n);` |
|      1 | 7289 | `	}` |
|      - | 7290 | `	/* Close the stream */` |
|      8 | 7291 | `	PH7_StreamCloseHandle(pStream,pHandle);` |
|      - | 7292 | `	/* Extract the digest */` |
|      8 | 7293 | `	MD5Final(zDigest,&sCtx);` |
|      8 | 7294 | `	if( raw_output ){` |
|      - | 7295 | `		/* Output raw digest */` |
|    ! 0 | 7296 | `		ph7_result_string(pCtx,(const char *)zDigest,sizeof(zDigest));` |
|    ! 0 | 7297 | `	}else{` |
|      - | 7298 | `		/* Perform a binary to hex conversion */` |
|      8 | 7299 | `		SyBinToHexConsumer((const void *)zDigest,sizeof(zDigest),vfsHashConsumer,pCtx);` |
|      - | 7300 | `	}` |
|      8 | 7301 | `	return PH7_OK;` |
|      6 | 7302 | `}` |
|      - | 7303 | `/*` |
|      - | 7304 | ` * string sha1_file(string $uri[,bool $raw_output = false ])` |
|      - | 7305 | ` *  Calculates the SHA1 hash of a given file.` |
|      - | 7306 | ` * Parameters` |
|      - | 7307 | ` *  $uri` |
|      - | 7308 | ` *   Target URI (file(/path/to/something) or URL(http://www.symisc.net/))` |
|      - | 7309 | ` *  $raw_output` |
|      - | 7310 | ` *   When TRUE, returns the digest in raw binary format with a length of 20.` |
|      - | 7311 | ` * Return` |
|      - | 7312 | ` *  Return the SHA1 digest on success or FALSE on failure.` |
|      - | 7313 | ` */` |
|      2 | 7314 | `PH7_PRIVATE int PH7_builtin_sha1_file(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 7315 | `{` |
|      - | 7316 | `	const ph7_io_stream *pStream;` |
|      - | 7317 | `	unsigned char zDigest[20];` |
|      3 | 7318 | `	int raw_output  = FALSE;` |
|      - | 7319 | `	const char *zFile;` |
|      - | 7320 | `	SHA1Context sCtx;` |
|      - | 7321 | `	char zBuf[8192];` |
|      - | 7322 | `	void *pHandle;` |
|      - | 7323 | `	ph7_int64 n;` |
|      - | 7324 | `	int nLen;` |
|      3 | 7325 | `	if( nArg < 1 \|\| !ph7_value_is_string(apArg[0]) ){` |
|      - | 7326 | `		/* Missing/Invalid arguments,return FALSE */` |
|    ! 0 | 7327 | `		ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"Expecting a file path");` |
|    ! 0 | 7328 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 7329 | `		return PH7_OK;` |
|      - | 7330 | `	}` |
|      - | 7331 | `	/* Extract the file path */` |
|      3 | 7332 | `	zFile = ph7_value_to_string(apArg[0],&nLen);` |
|      - | 7333 | `	/* Point to the target IO stream device */` |
|      3 | 7334 | `	pStream = PH7_VmGetStreamDevice(pCtx->pVm,&zFile,nLen);` |
|      3 | 7335 | `	if( pStream == 0 ){` |
|    ! 0 | 7336 | `		VfsThrowNoDeviceWarning(pCtx,zFile,FALSE);` |
|    ! 0 | 7337 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 7338 | `		return PH7_OK;` |
|      - | 7339 | `	}` |
|      3 | 7340 | `	if( nArg > 1 ){` |
|    ! 0 | 7341 | `		raw_output = ph7_value_to_bool(apArg[1]);` |
|    ! 0 | 7342 | `	}` |
|      - | 7343 | `	/* Try to open the file in read-only mode */` |
|      3 | 7344 | `	pHandle = PH7_StreamOpenHandle(pCtx->pVm,pStream,zFile,PH7_IO_OPEN_RDONLY,FALSE,0,FALSE,0,ph7_function_name(pCtx));` |
|      3 | 7345 | `	if( pHandle == 0 ){` |
|    ! 0 | 7346 | `		VfsThrowOpenWarning(pCtx,zFile);` |
|    ! 0 | 7347 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 7348 | `		return PH7_OK;` |
|      - | 7349 | `	}` |
|      - | 7350 | `	/* Init the SHA1 context */` |
|      3 | 7351 | `	SHA1Init(&sCtx);` |
|      - | 7352 | `	/* Perform the requested operation */` |
|      2 | 7353 | `	for(;;){` |
|      5 | 7354 | `		n = pStream->xRead(pHandle,zBuf,sizeof(zBuf));` |
|      5 | 7355 | `		if( n < 1 ){` |
|      - | 7356 | `			/* EOF or IO error,break immediately */` |
|      3 | 7357 | `			break;` |
|      - | 7358 | `		}` |
|      3 | 7359 | `		SHA1Update(&sCtx,(const unsigned char *)zBuf,(unsigned int)n);` |
|      1 | 7360 | `	}` |
|      - | 7361 | `	/* Close the stream */` |
|      3 | 7362 | `	PH7_StreamCloseHandle(pStream,pHandle);` |
|      - | 7363 | `	/* Extract the digest */` |
|      3 | 7364 | `	SHA1Final(&sCtx,zDigest);` |
|      3 | 7365 | `	if( raw_output ){` |
|      - | 7366 | `		/* Output raw digest */` |
|    ! 0 | 7367 | `		ph7_result_string(pCtx,(const char *)zDigest,sizeof(zDigest));` |
|    ! 0 | 7368 | `	}else{` |
|      - | 7369 | `		/* Perform a binary to hex conversion */` |
|      3 | 7370 | `		SyBinToHexConsumer((const void *)zDigest,sizeof(zDigest),vfsHashConsumer,pCtx);` |
|      - | 7371 | `	}` |
|      3 | 7372 | `	return PH7_OK;` |
|      2 | 7373 | `}` |
|      - | 7374 | `#endif /* PH7_DISABLE_HASH_FUNC */` |
|      - | 7375 | `/*` |
|      - | 7376 | ` * array parse_ini_file(string $filename[, bool $process_sections = false [, int $scanner_mode = INI_SCANNER_NORMAL ]] )` |
|      - | 7377 | ` *  Parse a configuration file.` |
|      - | 7378 | ` * Parameters` |
|      - | 7379 | ` * $filename` |
|      - | 7380 | ` *  The filename of the ini file being parsed.` |
|      - | 7381 | ` * $process_sections` |
|      - | 7382 | ` *  By setting the process_sections parameter to TRUE, you get a multidimensional array` |
|      - | 7383 | ` *  with the section names and settings included.` |
|      - | 7384 | ` *  The default for process_sections is FALSE.` |
|      - | 7385 | ` * $scanner_mode` |
|      - | 7386 | ` *  Can either be INI_SCANNER_NORMAL (default) or INI_SCANNER_RAW.` |
|      - | 7387 | ` *  If INI_SCANNER_RAW is supplied, then option values will not be parsed.` |
|      - | 7388 | ` * Return` |
|      - | 7389 | ` *  The settings are returned as an associative array on success.` |
|      - | 7390 | ` *  Otherwise is returned.` |
|      - | 7391 | ` */` |
|      8 | 7392 | `PH7_PRIVATE int PH7_builtin_parse_ini_file(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 7393 | `{` |
|      - | 7394 | `	const ph7_io_stream *pStream;` |
|      - | 7395 | `	const char *zFile;` |
|      - | 7396 | `	SyBlob sContents;` |
|      - | 7397 | `	void *pHandle;` |
|      - | 7398 | `	int nLen;` |
|      9 | 7399 | `	int iMode = PH7_INI_SCANNER_NORMAL;` |
|      9 | 7400 | `	sxi32 rc = PH7_OK;` |
|      9 | 7401 | `	if( nArg < 1 \|\| !ph7_value_is_string(apArg[0]) ){` |
|      - | 7402 | `		/* Missing/Invalid arguments,return FALSE */` |
|    ! 0 | 7403 | `		ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"Expecting a file path");` |
|    ! 0 | 7404 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 7405 | `		return PH7_OK;` |
|      - | 7406 | `	}` |
|      9 | 7407 | `	if( nArg > 2 && ph7_value_is_int(apArg[2]) ){` |
|      7 | 7408 | `		iMode = ph7_value_to_int(apArg[2]);` |
|      6 | 7409 | `		if( iMode != PH7_INI_SCANNER_NORMAL && iMode != PH7_INI_SCANNER_RAW` |
|      6 | 7410 | `		 && iMode != PH7_INI_SCANNER_TYPED ){` |
|      - | 7411 | `			/* php screens the mode BEFORE touching the file */` |
|      - | 7412 | ``			/* php's bare message: no `func(): ` qualifier on this one */`` |
|      3 | 7413 | `			PH7_VmThrowError(pCtx->pVm,0,PH7_CTX_WARNING,"Invalid scanner mode");` |
|      3 | 7414 | `			ph7_result_bool(pCtx,0);` |
|      3 | 7415 | `			return PH7_OK;` |
|      - | 7416 | `		}` |
|      2 | 7417 | `	}` |
|      - | 7418 | `	/* Extract the file path */` |
|      7 | 7419 | `	zFile = ph7_value_to_string(apArg[0],&nLen);` |
|      - | 7420 | `	/* Point to the target IO stream device */` |
|      7 | 7421 | `	pStream = PH7_VmGetStreamDevice(pCtx->pVm,&zFile,nLen);` |
|      7 | 7422 | `	if( pStream == 0 ){` |
|    ! 0 | 7423 | `		VfsThrowNoDeviceWarning(pCtx,zFile,FALSE);` |
|    ! 0 | 7424 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 7425 | `		return PH7_OK;` |
|      - | 7426 | `	}` |
|      - | 7427 | `	/* Try to open the file in read-only mode */` |
|      7 | 7428 | `	pHandle = PH7_StreamOpenHandle(pCtx->pVm,pStream,zFile,PH7_IO_OPEN_RDONLY,FALSE,0,FALSE,0,ph7_function_name(pCtx));` |
|      7 | 7429 | `	if( pHandle == 0 ){` |
|    ! 0 | 7430 | `		VfsThrowOpenWarning(pCtx,zFile);` |
|    ! 0 | 7431 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 7432 | `		return PH7_OK;` |
|      - | 7433 | `	}` |
|      7 | 7434 | `	SyBlobInit(&sContents,&pCtx->pVm->sAllocator);` |
|      - | 7435 | `	/* Read the whole file */` |
|      7 | 7436 | `	PH7_StreamReadWholeFile(pHandle,pStream,&sContents);` |
|      7 | 7437 | `	if( SyBlobLength(&sContents) < 1 ){` |
|      - | 7438 | `		/* Empty buffer,return FALSE */` |
|    ! 0 | 7439 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 7440 | `	}else{` |
|      - | 7441 | `		/* Process the raw INI buffer; capture an OOM abort to propagate below */` |
|     13 | 7442 | `		rc = PH7_ParseIniString(pCtx,(const char *)SyBlobData(&sContents),SyBlobLength(&sContents),` |
|      6 | 7443 | `			nArg > 1 ? ph7_value_to_bool(apArg[1]) : 0,iMode);` |
|      - | 7444 | `	}` |
|      - | 7445 | `	/* Close the stream */` |
|      7 | 7446 | `	PH7_StreamCloseHandle(pStream,pHandle);` |
|      - | 7447 | `	/* Release the working buffer */` |
|      7 | 7448 | `	SyBlobRelease(&sContents);` |
|      - | 7449 | `	/* Propagate an OOM abort so the fatal actually halts the VM */` |
|      7 | 7450 | `	return rc;` |
|      5 | 7451 | `}` |
|      - | 7452 | `/* ZIP archive processing moved to vfs_zip.c */` |
|      - | 7453 | `#else /* PH7_DISABLE_DISK_IO */` |
|      - | 7454 | `/*` |
|      - | 7455 | ` * Disk I/O is compiled out: this VFS hands out no resource handles, so` |
|      - | 7456 | ` * get_resource_type() has nothing that could be a "stream" and every` |
|      - | 7457 | ` * resource reports as "Unknown" (the same fallback the full build gives` |
|      - | 7458 | ` * to any non-VFS resource).` |
|      - | 7459 | ` */` |
|      - | 7460 | `PH7_PRIVATE const char * PH7_VfsResourceType(void *pResource)` |
|      - | 7461 | `{` |
|      - | 7462 | `	SXUNUSED(pResource);` |
|      - | 7463 | `	return "Unknown";` |
|      - | 7464 | `}` |
|      - | 7465 | `/* No disk I/O means no closeable handles: nothing is ever a closed resource. */` |
|      - | 7466 | `PH7_PRIVATE int PH7_VfsResourceIsClosed(void *pResource)` |
|      - | 7467 | `{` |
|      - | 7468 | `	SXUNUSED(pResource);` |
|      - | 7469 | `	return 0;` |
|      - | 7470 | `}` |
|      - | 7471 | `/* No streams means no stream contexts either, but PH7_VmReset still calls this. */` |
|      - | 7472 | `PH7_PRIVATE void PH7_StreamCtxVmReset(ph7_vm *pVm)` |
|      - | 7473 | `{` |
|      - | 7474 | `	SXUNUSED(pVm);` |
|      - | 7475 | `}` |
|      - | 7476 | `/* Same for the filter registry. */` |
|      - | 7477 | `PH7_PRIVATE void PH7_StreamFilterVmReset(ph7_vm *pVm)` |
|      - | 7478 | `{` |
|      - | 7479 | `	SXUNUSED(pVm);` |
|      - | 7480 | `}` |
|      - | 7481 | `#endif /* PH7_DISABLE_DISK_IO */` |
|      - | 7482 |  |
